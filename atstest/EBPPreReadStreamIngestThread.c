/*
Copyright (c) 2015, Cable Television Laboratories, Inc.(“CableLabs”)
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of CableLabs nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL CABLELABS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "ATSTestReport.h"

#include <mpeg2ts_demux.h>
#include <libts_common.h>
#include <tpes.h>
#include <ebp.h>
#include <varray.h>

#include "h264_stream.h"

#include "ATSTestAppConfig.h"

#include "EBPPreReadStreamIngestThread.h"
#include "EBPThreadLogging.h"
#include "ATSTestDefines.h"

static int preread_pmt_processor(mpeg2ts_program_t *m2p, void *arg);
static int preread_pat_processor(mpeg2ts_stream_t *m2s, void *arg);
static int preread_handle_ts_packet(ts_packet_t *ts, elementary_stream_info_t *es_info, void *arg);
static int preread_handle_pes_packet(pes_packet_t *pes, elementary_stream_info_t *esi, vqarray_t *ts_queue, void *arg);


void *EBPPreReadStreamIngestThreadProc(void *threadParams)
{
   int returnCode = 0;

   ebp_preread_stream_ingest_thread_params_t * ebpPreReadStreamIngestThreadParams = (ebp_preread_stream_ingest_thread_params_t *)threadParams;
   LOG_INFO_ARGS("EBPPreReadStreamIngestThread %d starting...ebpPreReadStreamIngestThreadParams = %p", 
      ebpPreReadStreamIngestThreadParams->threadNum, ebpPreReadStreamIngestThreadParams);

   int num_packets = 4096;
   int ts_buf_sz = TS_SIZE * num_packets;
   uint8_t *ts_buf = malloc(ts_buf_sz);

   LOG_INFO ("\n");
   LOG_INFO_ARGS ("Main:prereadIngestStreams: IngestStream %d", ebpPreReadStreamIngestThreadParams->threadNum); 

   // reset PAT/PMT read flags
   ebpPreReadStreamIngestThreadParams->bPATFound = 0;
   ebpPreReadStreamIngestThreadParams->bPMTFound = 0;
   ebpPreReadStreamIngestThreadParams->bEBPSearchEnded = 0;
   ebpPreReadStreamIngestThreadParams->streamStartTimeMsecs = -1;

   mpeg2ts_stream_t *m2s = NULL;

   if (NULL == (m2s = mpeg2ts_stream_new()))
   {
      LOG_ERROR_ARGS("Main:prereadIngestStreams: Error creating MPEG-2 STREAM object for ingestStream %d", 
         ebpPreReadStreamIngestThreadParams->threadNum);
      reportAddErrorLogArgs("Main:prereadIngestStreams: Error creating MPEG-2 STREAM object for ingestStream %d", 
         ebpPreReadStreamIngestThreadParams->threadNum);
      // GORP: fatal error here
      return NULL;
   }

   // Register EBP descriptor parser
   descriptor_table_entry_t *desc = calloc(1, sizeof(descriptor_table_entry_t));
   desc->tag = EBP_DESCRIPTOR;
   desc->free_descriptor = ebp_descriptor_free;
   desc->print_descriptor = ebp_descriptor_print;
   desc->read_descriptor = ebp_descriptor_read;
   if (!register_descriptor(desc))
   {
      LOG_ERROR_ARGS("Main:prereadIngestStreams: FAIL: Could not register EBP descriptor parser for ingest stream %d", 
         ebpPreReadStreamIngestThreadParams->threadNum);
      reportAddErrorLogArgs("Main:prereadIngestStreams: FAIL: Could not register EBP descriptor parser for ingest stream %d", 
         ebpPreReadStreamIngestThreadParams->threadNum);
      // GORP: fatal error here
      return NULL;
   }
      
   m2s->pat_processor = (pat_processor_t)preread_pat_processor;
   m2s->arg = ebpPreReadStreamIngestThreadParams;
   m2s->arg_destructor = NULL;

   int num_bytes = 0;
   while (!(ebpPreReadStreamIngestThreadParams->bPATFound && 
            ebpPreReadStreamIngestThreadParams->bPMTFound && 
            ebpPreReadStreamIngestThreadParams->bEBPSearchEnded) 
      && (num_bytes = cb_peek (ebpPreReadStreamIngestThreadParams->cb, ts_buf, ts_buf_sz)) > 0)
   {
      if (num_bytes % TS_SIZE)
      {
         LOG_ERROR_ARGS("Main:prereadIngestStreams: FAIL: Incomplete transport packet received for ingest stream %d", 
            ebpPreReadStreamIngestThreadParams->threadNum);
         reportAddErrorLogArgs("Main:prereadIngestStreams: FAIL: Incomplete transport packet received for ingest stream %d", 
            ebpPreReadStreamIngestThreadParams->threadNum);
         // GORP: fatal error here
         return NULL;
      }
      num_packets = num_bytes / TS_SIZE;

      for (int i = 0; i < num_packets; i++)
      {
         ts_packet_t *ts = ts_new();
         ts_read(ts, ts_buf + i * TS_SIZE, TS_SIZE);
         mpeg2ts_stream_read_ts_packet(m2s, ts);

         // check if PAT/PMT read -- if so, break out
         if (ebpPreReadStreamIngestThreadParams->bPATFound && 
             ebpPreReadStreamIngestThreadParams->bPMTFound && 
             ebpPreReadStreamIngestThreadParams->bEBPSearchEnded)
         {
            LOG_DEBUG ("Main:prereadIngestStreams: EBP search ended");
            break;
         }
      }
   }

   mpeg2ts_stream_free(m2s);  

   free (ts_buf);
   LOG_INFO_ARGS ("Main:prereadIngestStreams: exiting for ingest stream %d", ebpPreReadStreamIngestThreadParams->threadNum);

   return NULL;
}

static int preread_handle_ts_packet(ts_packet_t *ts, elementary_stream_info_t *es_info, void *arg)
{
   return 1;
}

static int preread_handle_pes_packet(pes_packet_t *pes, elementary_stream_info_t *esi, vqarray_t *ts_queue, void *arg)
{
   // Get the first TS packet and check it for EBP
   ts_packet_t *ts = (ts_packet_t*)vqarray_get(ts_queue,0);
   if (ts == NULL)
   {
      // GORP: should this be an error?
      pes_free(pes);
      return 1; // Don't care about this packet
   }

   ebp_preread_stream_ingest_thread_params_t * ebpPreReadStreamIngestThreadParams = (ebp_preread_stream_ingest_thread_params_t *)arg;

//   printf ("handle_pes_packet, PID = 0x%x, PTS = %"PRId64"\n", esi->elementary_PID, pes->header.PTS);
         
   // Read N seconds of data 
   // if we get an ebp struct in each stream before that, then exit.

   // get time and exit if greater that limit
   int64_t currentTimeMsecs = (pes->header.PTS * 1000) / 90000;
   if (ebpPreReadStreamIngestThreadParams->streamStartTimeMsecs == -1)
   {
      ebpPreReadStreamIngestThreadParams->streamStartTimeMsecs = currentTimeMsecs;
   }
   else
   {
      
      if ((currentTimeMsecs - ebpPreReadStreamIngestThreadParams->streamStartTimeMsecs) > g_ATSTestAppConfig.ebpPrereadSearchTimeMsecs)
      {
         LOG_INFO ("EBP search complete\n");
         ebpPreReadStreamIngestThreadParams->bEBPSearchEnded = 1;
      }
   }


   ebp_stream_info_t streamInfo;  // this isnt really used for anything -- just a placeholder in the following
   streamInfo.PID = esi->elementary_PID;
   ebp_t* ebp = getEBP(ts, &streamInfo, -1 /* threadNum */);
         
   if (ATS_TEST_CASE_AUDIO_IMPLICIT_TRIGGER || ATS_TEST_CASE_AUDIO_XFILE_IMPLICIT_TRIGGER)
   {
      // testing -- removes ebp from audio to test implicit triggering
      if (esi->elementary_PID == 482)
      {
         ebp = NULL;
      }
   }

   if (ebp != NULL)
   {   
      // store EBP struct in programStreamInfo (by PID) -- this is used for later EBP boundary analysis
      program_stream_info_t *programStreamInfo = ebpPreReadStreamIngestThreadParams->programStreamInfo;
      for (int i=0; i<programStreamInfo->numStreams; i++)
      {
         if ((programStreamInfo->PIDs)[i] == esi->elementary_PID)
         {
            (programStreamInfo->ebps)[i] = ebp_copy(ebp);

            if ((programStreamInfo->ebpLists)[i] == NULL)
            {
               (programStreamInfo->ebpLists)[i] = varray_new();
            }

            varray_add ((programStreamInfo->ebpLists)[i], ebp_copy(ebp));
         }
      }
     
      ebp_free(ebp);
   }
   
   if (vqarray_length(ebpPreReadStreamIngestThreadParams->unfinishedPIDs) == 0)
   {
      LOG_INFO ("EBP detection complete -- all PIDS have EBP descriptors");
      ebpPreReadStreamIngestThreadParams->bEBPSearchEnded = 1;
   }

   pes_free(pes);
   return 1;
}


static int preread_pmt_processor(mpeg2ts_program_t *m2p, void *arg)
{
   LOG_INFO ("preread_pmt_processor");
   ebp_preread_stream_ingest_thread_params_t * ebpPreReadStreamIngestThreadParams = (ebp_preread_stream_ingest_thread_params_t *)arg;
   ebpPreReadStreamIngestThreadParams->bPMTFound = 1;

   if (m2p == NULL || m2p->pmt == NULL) // if we don't have any PSI, there's nothing we can do
      return 0;

   pid_info_t *pi = NULL;
   ebpPreReadStreamIngestThreadParams->unfinishedPIDs = vqarray_new();
   ebpPreReadStreamIngestThreadParams->ebpStructs = vqarray_new();

   program_stream_info_t *programStreamInfo = ebpPreReadStreamIngestThreadParams->programStreamInfo;
   populateProgramStreamInfo(programStreamInfo, m2p);
            
   for (int i = 0; i < vqarray_length(m2p->pids); i++) // TODO replace linear search w/ hashtable lookup in the future
   {
      if ((pi = vqarray_get(m2p->pids, i)) != NULL)
      {
         int handle_pid = 0;

//         printf ("stream type = %d\n", pi->es_info->stream_type);
//         printf ("elementary PID = %d\n", pi->es_info->elementary_PID);
         descriptor_t *desc = NULL;
         if (IS_SCTE35_STREAM(pi->es_info->stream_type))
         {
            LOG_INFO_ARGS ("SCTE35: stream type = %d", pi->es_info->stream_type);
            LOG_INFO_ARGS ("SCTE35: elementary PID = %d", pi->es_info->elementary_PID);

            pes_demux_t *pd = pes_demux_new(NULL);
            pd->pes_arg = arg;
            pd->pes_arg_destructor = NULL;

            // hook PES demuxer to the PID processor
            demux_pid_handler_t *demux_handler = calloc(1, sizeof(demux_pid_handler_t));
            demux_handler->process_ts_packet = pes_demux_process_ts_packet;
            demux_handler->arg = pd;
            demux_handler->arg_destructor = (arg_destructor_t)pes_demux_free;

            // hook PES demuxer to the PID processor
            demux_pid_handler_t *demux_validator = calloc(1, sizeof(demux_pid_handler_t));
            demux_validator->process_ts_packet = preread_handle_ts_packet;
            demux_validator->arg = arg;
            demux_validator->arg_destructor = NULL;

            // hook PID processor to PID
            mpeg2ts_program_register_pid_processor(m2p, pi->es_info->elementary_PID, demux_handler, demux_validator);
         }
         else if (IS_VIDEO_STREAM(pi->es_info->stream_type))
         {
            // Look for the SCTE128 descriptor in the ES loop
            for (int d = 0; d < vqarray_length(pi->es_info->descriptors); d++)
            {
               if ((desc = vqarray_get(pi->es_info->descriptors, d)) != NULL)
               {
                  if (desc->tag == 0x97)
                  {
                     mpeg2ts_program_enable_scte128(m2p);
                  }
               }
            }
            handle_pid = 1;
         }
         else if (IS_AUDIO_STREAM(pi->es_info->stream_type))
         {
            handle_pid = 1;
         }
            
         if (handle_pid)
         {
            ebp_descriptor_t* ebpDescriptor = getEBPDescriptor (pi->es_info);
            if (ATS_TEST_CASE_NO_EBP_DESCRIPTOR)
            {
               // testing -- removes ebpDescriptor to test triggering without descriptor present
               ebpDescriptor = NULL;
            }
            else if (IS_AUDIO_STREAM(pi->es_info->stream_type) && 
               (ATS_TEST_CASE_AUDIO_IMPLICIT_TRIGGER || ATS_TEST_CASE_AUDIO_XFILE_IMPLICIT_TRIGGER))
            {
               // testing -- removes ebpDescriptor to test implicit triggering
               ebpDescriptor = NULL;
            }

            if (ebpDescriptor == NULL)
            {
               LOG_INFO_ARGS ("Starting EBP detection for PID: %d", pi->es_info->elementary_PID);
               vqarray_add(ebpPreReadStreamIngestThreadParams->unfinishedPIDs, (vqarray_elem_t *)pi->es_info->elementary_PID);
            }
            else
            {
               LOG_INFO_ARGS ("EBP Descriptor present -- no EBP detection necessary for PID: %d", pi->es_info->elementary_PID);
            }

            pes_demux_t *pd = pes_demux_new(preread_handle_pes_packet);
            pd->pes_arg = arg;
            pd->pes_arg_destructor = NULL;

            // hook PES demuxer to the PID processor
            demux_pid_handler_t *demux_handler = calloc(1, sizeof(demux_pid_handler_t));
            demux_handler->process_ts_packet = pes_demux_process_ts_packet;
            demux_handler->arg = pd;
            demux_handler->arg_destructor = (arg_destructor_t)pes_demux_free;

            // hook PES demuxer to the PID processor
            demux_pid_handler_t *demux_validator = calloc(1, sizeof(demux_pid_handler_t));
            demux_validator->process_ts_packet = preread_handle_ts_packet;
            demux_validator->arg = arg;
            demux_validator->arg_destructor = NULL;

            // hook PID processor to PID
            mpeg2ts_program_register_pid_processor(m2p, pi->es_info->elementary_PID, demux_handler, demux_validator);
         }
      }
   }

   return 1;
}


static int preread_pat_processor(mpeg2ts_stream_t *m2s, void *arg)
{
   LOG_INFO_ARGS ("preread_pat_processor: %d", vqarray_length(m2s->programs));
   ebp_preread_stream_ingest_thread_params_t * ebpPreReadStreamIngestThreadParams = (ebp_preread_stream_ingest_thread_params_t *)arg;
   ebpPreReadStreamIngestThreadParams->bPATFound = 1;

   for (int i = 0; i < vqarray_length(m2s->programs); i++)
   {
      mpeg2ts_program_t *m2p = vqarray_get(m2s->programs, i);

      if (m2p == NULL) continue;
      m2p->pmt_processor =  (pmt_processor_t)preread_pmt_processor;
      m2p->arg = arg;
   }
   LOG_INFO ("preread_pat_processor: DONE");
   return 1;
}

void populateProgramStreamInfo(program_stream_info_t *programStreamInfo, mpeg2ts_program_t *m2p)
{
   pid_info_t *pi;
//   programStreamInfo->numStreams = vqarray_length(m2p->pids);
   programStreamInfo->numStreams = 0;
   for (int j = 0; j < vqarray_length(m2p->pids); j++)
   {
      if ((pi = vqarray_get(m2p->pids, j)) != NULL)
      {
         if (IS_AUDIO_STREAM(pi->es_info->stream_type) || IS_VIDEO_STREAM(pi->es_info->stream_type))
         {
            programStreamInfo->numStreams++;
         }
      }
   }


   programStreamInfo->stream_types = (uint32_t*) calloc (programStreamInfo->numStreams, sizeof (uint32_t));
   programStreamInfo->PIDs = (uint32_t*) calloc (programStreamInfo->numStreams, sizeof (uint32_t));
   programStreamInfo->ebpDescriptors = (ebp_descriptor_t**) calloc (programStreamInfo->numStreams, sizeof (ebp_descriptor_t *));
   programStreamInfo->ebps = (ebp_t**) calloc (programStreamInfo->numStreams, sizeof (ebp_t *));
   programStreamInfo->ebpLists = (varray_t**) calloc (programStreamInfo->numStreams, sizeof (varray_t *));
   programStreamInfo->language = (char**) calloc (programStreamInfo->numStreams, sizeof (char *));

   int progStreamIndex = 0;

   for (int j = 0; j < vqarray_length(m2p->pids); j++)
   {
      if ((pi = vqarray_get(m2p->pids, j)) != NULL)
      {
         if (!IS_AUDIO_STREAM(pi->es_info->stream_type) && !IS_VIDEO_STREAM(pi->es_info->stream_type))
         {
            continue;
         }

         LOG_INFO_ARGS ("Main:prereadFiles: stream %d: stream_type = %u, elementary_PID = %u, ES_info_length = %u",
            progStreamIndex, pi->es_info->stream_type, pi->es_info->elementary_PID, pi->es_info->ES_info_length);

         programStreamInfo->stream_types[progStreamIndex] = pi->es_info->stream_type;
         programStreamInfo->PIDs[progStreamIndex] = pi->es_info->elementary_PID;

         ebp_descriptor_t* ebpDescriptor = getEBPDescriptor (pi->es_info);
         if (ATS_TEST_CASE_AUDIO_IMPLICIT_TRIGGER &&
            pi->es_info->elementary_PID == 482)
         {
            ebpDescriptor = NULL;
         }
         else if (ATS_TEST_CASE_AUDIO_XFILE_IMPLICIT_TRIGGER != 0 && pi->es_info->elementary_PID == 482)
         {
            ebp_partition_data_t* partition = get_partition (ebpDescriptor, EBP_PARTITION_FRAGMENT);
            partition->ebp_data_explicit_flag = 0;
            partition->ebp_pid = 481;
            partition = get_partition (ebpDescriptor, EBP_PARTITION_SEGMENT);
            partition->ebp_data_explicit_flag = 0;
            partition->ebp_pid = 481;
         }

         if (ebpDescriptor != NULL)
         {
            ebp_descriptor_print_stdout (ebpDescriptor);
            programStreamInfo->ebpDescriptors[progStreamIndex] = ebp_descriptor_copy(ebpDescriptor);
         }
         else
         {
            LOG_INFO ("NULL ebp_descriptor");
         }

         language_descriptor_t* languageDescriptor = getLanguageDescriptor (pi->es_info);
         component_name_descriptor_t* componentNameDescriptor = getComponentNameDescriptor (pi->es_info);
         ac3_descriptor_t* ac3Descriptor = getAC3Descriptor (pi->es_info);
         int languageStringSz = 3; // commas + NULL terminator

         if (languageDescriptor != NULL)
         {
            for (int ii=0; ii<languageDescriptor->num_languages; ii++)
            {
               languageStringSz += 4; // 3 chars plus a : to separate languages
            }
         }
         if (componentNameDescriptor != NULL && componentNameDescriptor->num_names)
         {
//            void sortStringArray(componentNameDescriptor->names, componentNameDescriptor->numNames);
            for (int ii=0; ii<componentNameDescriptor->num_names; ii++)
            {
               languageStringSz += strlen(componentNameDescriptor->names[ii]) + 1;  // strlen plus a : to separate names
            }
         }
         if (ac3Descriptor != NULL)
         {
            languageStringSz += 3;
         }
            
         programStreamInfo->language[progStreamIndex] = (char *)calloc (languageStringSz, 1);

         if (languageDescriptor != NULL)
         {
            // alphabetize multiple language strings in case they are in a different
            // order in different streams
            alphabetizeLanguageDescriptorLanguages (languageDescriptor);
            for (int ii=0; ii<languageDescriptor->num_languages; ii++)
            {
               strcat (programStreamInfo->language[progStreamIndex], languageDescriptor->languages[ii].ISO_639_language_code);
               strcat (programStreamInfo->language[progStreamIndex], ":");
            }

            LOG_INFO_ARGS ("Num Languages = %d, Language = %s", languageDescriptor->num_languages, programStreamInfo->language[progStreamIndex]);
         }
         strcat (programStreamInfo->language[progStreamIndex], ",");

         if (componentNameDescriptor != NULL && componentNameDescriptor->num_names)
         {
            alphabetizeStringArray(componentNameDescriptor->names, componentNameDescriptor->num_names);
            for (int ii=0; ii<componentNameDescriptor->num_names; ii++)
            {
               strcat (programStreamInfo->language[progStreamIndex], componentNameDescriptor->names[ii]);               
               strcat (programStreamInfo->language[progStreamIndex], ":");
            }
         }
         else
         {
            if (ATS_TEST_CASE_AUDIO_UNIQUE_LANG)
            {
               // testing: tests discrimination by language by making a unique language per stream
               char temp[10];
               sprintf (temp, "%d", pi->es_info->elementary_PID);
               strcat (programStreamInfo->language[progStreamIndex], temp);  
            }
         }
         
         strcat (programStreamInfo->language[progStreamIndex], ",");

         if (ac3Descriptor != NULL)
         {
            strcat (programStreamInfo->language[progStreamIndex], ac3Descriptor->language);               
         }
               
         LOG_INFO_ARGS ("Language = %s", programStreamInfo->language[progStreamIndex]);
      
         progStreamIndex++;
      }
   }
}


void alphabetizeStringArray(char **stringArray, int stringArraySz)
{
   // bubble sort
   if (stringArraySz <= 1)
   {
      return;
   }

   int done = 0;
   while (!done)
   {
      done = 1;
      for (int i=1; i<stringArraySz; i++)
      {
         if (strcmp(stringArray[i-1], stringArray[i]) > 0)
         {
            char *temp = stringArray[i-1];
            stringArray[i-1] = stringArray[i];
            stringArray[i] = temp;

            done = 0;
         }
      }
   }
}

void alphabetizeLanguageDescriptorLanguages (language_descriptor_t* languageDescriptor)
{
   // bubble sort
   if (languageDescriptor->num_languages <= 1)
   {
      return;
   }

   int done = 0;
   while (!done)
   {
      done = 1;
      for (int i=1; i<languageDescriptor->num_languages; i++)
      {
         if (strcmp(languageDescriptor->languages[i-1].ISO_639_language_code, 
            languageDescriptor->languages[i].ISO_639_language_code) > 0)
         {
            iso639_lang_t temp = languageDescriptor->languages[i-1];
            languageDescriptor->languages[i-1] = languageDescriptor->languages[i];
            languageDescriptor->languages[i] = temp;

            done = 0;
         }
      }
   }
}

