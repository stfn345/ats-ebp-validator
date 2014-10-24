/*
** Copyright (C) 2014  Cable Television Laboratories, Inc.
** Contact: http://www.cablelabs.com/
 

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
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

#include <mpeg2ts_demux.h>
#include <libts_common.h>
#include <tpes.h>
#include <ebp.h>


#include "EBPSegmentAnalysisThread.h"
#include "EBPFileIngestThread.h"
#include "EBPThreadLogging.h"


static char *STREAM_TYPE_UNKNOWN = "UNKNOWN";
static char *STREAM_TYPE_VIDEO = "VIDEO";
static char *STREAM_TYPE_AUDIO = "AUDIO";

static int validate_ts_packet(ts_packet_t *ts, elementary_stream_info_t *es_info, void *arg)
{
   return 1;
}

static char* getStreamTypeDesc (elementary_stream_info_t *esi)
{
   if (IS_VIDEO_STREAM(esi->stream_type)) return STREAM_TYPE_VIDEO;
   if (IS_AUDIO_STREAM(esi->stream_type)) return STREAM_TYPE_AUDIO;

   return STREAM_TYPE_UNKNOWN;
}

static int validate_pes_packet(pes_packet_t *pes, elementary_stream_info_t *esi, vqarray_t *ts_queue, void *arg)
{
   // Get the first TS packet and check it for EBP
   ts_packet_t *ts = (ts_packet_t*)vqarray_get(ts_queue,0);
   if (ts == NULL)
   {
      // GORP: should this be an error?
      pes_free(pes);
      return 1; // Don't care about this packet
   }
         
   ebp_file_ingest_thread_params_t *ebpFileIngestThreadParams = (ebp_file_ingest_thread_params_t *)arg;

   thread_safe_fifo_t *fifo = NULL;
   int fifoIndex = -1;
   findFIFO (esi->elementary_PID, ebpFileIngestThreadParams->streamInfos, ebpFileIngestThreadParams->numStreamInfos,
      &fifo, &fifoIndex);
   ebp_stream_info_t * streamInfo = ebpFileIngestThreadParams->streamInfos[fifoIndex];
   
   if (fifo == NULL)
   {
      // this should never happen -- maybe this should be fatal
      LOG_ERROR_ARGS("FileIngestThread %d: FAIL: Cannot find fifo for PID %d (%s)", 
         ebpFileIngestThreadParams->threadNum, esi->elementary_PID, getStreamTypeDesc (esi));

      ebpFileIngestThreadParams->filePassFail = 0;

      pes_free(pes);
      return 1;
   }

   ebp_t* ebp = getEBP(ts, streamInfo, ebpFileIngestThreadParams->threadNum);
   if (ebp != NULL)
   {
      LOG_DEBUG_ARGS("FileIngestThread %d: Found EBP data in transport packet: PID %d (%s)", 
         ebpFileIngestThreadParams->threadNum, esi->elementary_PID, getStreamTypeDesc (esi));
      ebp_print_stdout(ebp);
   }

   if (streamInfo->ebpImplicit)
   {
      if (ebp != NULL)
      {
         LOG_ERROR_ARGS("FileIngestThread %d: FAIL: Explicit EBP found in implicit-EBP stream: PID %d (%s)", 
          ebpFileIngestThreadParams->threadNum, esi->elementary_PID, getStreamTypeDesc (esi));

         streamInfo->streamPassFail = 0;
      }

      if (streamInfo->lastVideoChunkPTSValid && pes->header.PTS > streamInfo->lastVideoChunkPTS)
      {
         uint32_t sapType = 0;  // GORP: fill in
         ebp_descriptor_t* ebpDescriptor = getEBPDescriptor (esi);  // could be null

         int returnCode = postToFIFO (pes->header.PTS, sapType, ebp, ebpDescriptor, esi->elementary_PID, ebpFileIngestThreadParams);
         if (returnCode != 0)
         {
            LOG_ERROR_ARGS("FileIngestThread %d: FAIL: Error posting to FIFO: PID %d (%s)", 
               ebpFileIngestThreadParams->threadNum, esi->elementary_PID, getStreamTypeDesc (esi));

            streamInfo->streamPassFail = 0;
         }

         streamInfo->lastVideoChunkPTSValid = 0;
      }
   }
   else if (!streamInfo->ebpImplicit)
   {
      if (ebp != NULL)
      {
         ebp_descriptor_t* ebpDescriptor = getEBPDescriptor (esi);
         if (ebpDescriptor == NULL)
         {
            // if there is a ebp in the stream, then we need an ebp_descriptor to interpret it
            LOG_ERROR_ARGS("FileIngestThread %d: FAIL: EBP struct present but EBP Descriptor missing: PID %d (%s)", 
               ebpFileIngestThreadParams->threadNum, esi->elementary_PID, getStreamTypeDesc (esi));

            streamInfo->streamPassFail = 0;
         }

         if (ebp_validate_groups(ebp) != 0)
         {
            // if there is a ebp in the stream, then we need an ebp_descriptor to interpret it
            LOG_ERROR_ARGS("FileIngestThread %d: FAIL: EBP group validation failed: PID %d (%s)", 
               ebpFileIngestThreadParams->threadNum, esi->elementary_PID, getStreamTypeDesc (esi));

            streamInfo->streamPassFail = 0;
         }

         // check if this is a boundary
         if (ebpDescriptor != NULL &&
             ((ebp->ebp_fragment_flag && does_fragment_mark_boundary (ebpDescriptor)) ||
             (ebp->ebp_segment_flag && does_segment_mark_boundary (ebpDescriptor))))
         {
            // yes, its a boundary

            uint32_t sapType = 0;  // GORP: fill in
            int returnCode = postToFIFO (pes->header.PTS, sapType, ebp, ebpDescriptor, esi->elementary_PID, ebpFileIngestThreadParams);
            if (returnCode != 0)
            {
               LOG_ERROR_ARGS("FileIngestThread %d: FAIL: Error posting to FIFO: PID %d (%s)", 
                  ebpFileIngestThreadParams->threadNum, esi->elementary_PID, getStreamTypeDesc (esi));

               streamInfo->streamPassFail = 0;
            }

            if (IS_VIDEO_STREAM(esi->stream_type))
            {
               // if this is video, set lastVideoPTS in audio fifos
               for (int i=0; i<ebpFileIngestThreadParams->numStreamInfos; i++)
               {
                  if (i == fifoIndex)
                  {
                     continue;
                  }

                  ebp_stream_info_t * streamInfoTemp = ebpFileIngestThreadParams->streamInfos[i];
                     
                  streamInfoTemp->lastVideoChunkPTS = pes->header.PTS;
                  streamInfoTemp->lastVideoChunkPTSValid = 1;
               }
            }
            else
            {
               // this is an audio stream, so check that audio does not lag video by more than 3 seconds
               if (pes->header.PTS > (streamInfo->lastVideoChunkPTS + 3 * 90000))  // PTS is 90kHz ticks
               {
                  LOG_ERROR_ARGS("FileIngestThread %d: FAIL: Audio PTS (%"PRId64") lags video PTS (%"PRId64") by more than 3 seconds: PID %d (%s)", 
                     ebpFileIngestThreadParams->threadNum, pes->header.PTS, streamInfo->lastVideoChunkPTS, 
                     esi->elementary_PID, getStreamTypeDesc (esi));

                  streamInfo->streamPassFail = 0;
               }
            }
         }
      }
   }

   pes_free(pes);
   return 1;
}

ebp_t* getEBP(ts_packet_t *ts, ebp_stream_info_t * streamInfo, int threadNum)
{
   ebp_t* ebp = NULL;

   vqarray_t *scte128_data;
   if (!TS_HAS_ADAPTATION_FIELD(*ts) ||
         (scte128_data = ts->adaptation_field.scte128_private_data) == NULL ||
         vqarray_length(scte128_data) == 0)
   {
      return NULL;
   }

   int found_ebp = 0;
   for (vqarray_iterator_t *it = vqarray_iterator_new(scte128_data); vqarray_iterator_has_next(it);)
   {
      ts_scte128_private_data_t *scte128 = (ts_scte128_private_data_t*)vqarray_iterator_next(it);

      // Validate that we have a tag of 0xDF and a format id of 'EBP0' (0x45425030)
      if (scte128 != NULL && scte128->tag == 0xDF && scte128->format_identifier == 0x45425030)
      {
         if (found_ebp)
         {
            LOG_ERROR_ARGS("FileIngestThread %d: FAIL: Multiple EBP structures detected with a single PES packet: PID %d", 
               threadNum, streamInfo->PID);

            streamInfo->streamPassFail = 0;
               
            return NULL;
         }

         found_ebp = 1;

         if (!ts->header.payload_unit_start_indicator) 
         {
            LOG_ERROR_ARGS("FileIngestThread %d: FAIL: EBP present on a TS packet that does not have PUSI bit set: PID %d", 
               threadNum, streamInfo->PID);

            streamInfo->streamPassFail = 0;

            return NULL;
         }

         // Parse the EBP
         ebp = ebp_new();
         if (!ebp_read(ebp, scte128))
         {
            LOG_ERROR_ARGS("FileIngestThread %d: FAIL: Error parsing EBP: PID %d", 
               threadNum, streamInfo->PID);

            streamInfo->streamPassFail = 0;

            return NULL;
         }
      }
   }

   return ebp;
}


ebp_descriptor_t* getEBPDescriptor (elementary_stream_info_t *esi)
{
   vqarray_t *descriptors = esi->descriptors;

   for (int i=0; i<vqarray_length(descriptors); i++)
   {
      descriptor_t* descriptor = vqarray_get(descriptors, i);
      if (descriptor != NULL && descriptor->tag == EBP_DESCRIPTOR)
      {
         return (ebp_descriptor_t*)descriptor;
      }
   }

   return NULL;
}

static int pmt_processor(mpeg2ts_program_t *m2p, void *arg)
{
   LOG_INFO_ARGS ("pmt_processor: arg = %x", (unsigned int) arg);
   if (m2p == NULL || m2p->pmt == NULL) // if we don't have any PSI, there's nothing we can do
      return 0;

   pid_info_t *pi = NULL;
   for (int i = 0; i < vqarray_length(m2p->pids); i++) // TODO replace linear search w/ hashtable lookup in the future
   {
      if ((pi = vqarray_get(m2p->pids, i)) != NULL)
      {
         int handle_pid = 0;

         if (IS_VIDEO_STREAM(pi->es_info->stream_type))
         {
            // Look for the SCTE128 descriptor in the ES loop
            descriptor_t *desc = NULL;
            for (int d = 0; d < vqarray_length(pi->es_info->descriptors); d++)
            {
               if ((desc = vqarray_get(pi->es_info->descriptors, d)) != NULL && desc->tag == 0x97)
               {
                  mpeg2ts_program_enable_scte128(m2p);
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
            pes_demux_t *pd = pes_demux_new(validate_pes_packet);
            pd->pes_arg = arg;
            pd->pes_arg_destructor = NULL;

            // hook PES demuxer to the PID processor
            demux_pid_handler_t *demux_handler = calloc(1, sizeof(demux_pid_handler_t));
            demux_handler->process_ts_packet = pes_demux_process_ts_packet;
            demux_handler->arg = pd;
            demux_handler->arg_destructor = (arg_destructor_t)pes_demux_free;

            // hook PES demuxer to the PID processor
            demux_pid_handler_t *demux_validator = calloc(1, sizeof(demux_pid_handler_t));
            demux_validator->process_ts_packet = validate_ts_packet;
            demux_validator->arg = arg;
            demux_validator->arg_destructor = NULL;

            // hook PID processor to PID
            mpeg2ts_program_register_pid_processor(m2p, pi->es_info->elementary_PID, demux_handler, demux_validator);
         }
      }
   }

   return 1;
}

static int pat_processor(mpeg2ts_stream_t *m2s, void *arg)
{

   for (int i = 0; i < vqarray_length(m2s->programs); i++)
   {
      mpeg2ts_program_t *m2p = vqarray_get(m2s->programs, i);

      if (m2p == NULL) continue;
      m2p->pmt_processor =  (pmt_processor_t)pmt_processor;
      m2p->arg = arg;
   }
   return 1;
}

void *EBPFileIngestThreadProc(void *threadParams)
{
   int returnCode = 0;

   ebp_file_ingest_thread_params_t * ebpFileIngestThreadParams = (ebp_file_ingest_thread_params_t *)threadParams;
   LOG_INFO_ARGS("EBPFileIngestThread %d starting...ebpFileIngestThreadParams = %x", 
      ebpFileIngestThreadParams->threadNum, (unsigned int)ebpFileIngestThreadParams);

   // do file reading here
   FILE *infile = NULL;
   if ((infile = fopen(ebpFileIngestThreadParams->filePath, "rb")) == NULL)
   {
      LOG_ERROR_ARGS("EBPFileIngestThread %d: FAIL: Cannot open file %s - %s", 
         ebpFileIngestThreadParams->threadNum, ebpFileIngestThreadParams->filePath, strerror(errno));

      ebpFileIngestThreadParams->filePassFail = 0;
      cleanupAndExit(ebpFileIngestThreadParams);
   }

   mpeg2ts_stream_t *m2s = NULL;

   if (NULL == (m2s = mpeg2ts_stream_new()))
   {
      LOG_ERROR_ARGS("EBPFileIngestThread %d: FAIL: Error creating MPEG-2 STREAM object",
         ebpFileIngestThreadParams->threadNum);
      ebpFileIngestThreadParams->filePassFail = 0;
      cleanupAndExit(ebpFileIngestThreadParams);
   }

   // Register EBP descriptor parser
   descriptor_table_entry_t *desc = calloc(1, sizeof(descriptor_table_entry_t));
   desc->tag = EBP_DESCRIPTOR;
   desc->free_descriptor = ebp_descriptor_free;
   desc->print_descriptor = ebp_descriptor_print;
   desc->read_descriptor = ebp_descriptor_read;
   if (!register_descriptor(desc))
   {
      LOG_ERROR_ARGS("EBPFileIngestThread %d: FAIL: Could not register EBP descriptor parser",
         ebpFileIngestThreadParams->threadNum);
      ebpFileIngestThreadParams->filePassFail = 0;

      cleanupAndExit(ebpFileIngestThreadParams);
   }

   m2s->pat_processor = (pat_processor_t)pat_processor;
   m2s->arg = ebpFileIngestThreadParams;
   m2s->arg_destructor = NULL;

   int num_packets = 4096;
   uint8_t *ts_buf = malloc(TS_SIZE * 4096);

   int total_packets = 0;

   while ((num_packets = fread(ts_buf, TS_SIZE, 4096, infile)) > 0)
   {
      total_packets += num_packets;
      LOG_DEBUG_ARGS ("total_packets = %d, num_packets = %d", total_packets, num_packets);
      for (int i = 0; i < num_packets; i++)
      {
         ts_packet_t *ts = ts_new();
         ts_read(ts, ts_buf + i * TS_SIZE, TS_SIZE);
         int returnCode = mpeg2ts_stream_read_ts_packet(m2s, ts);
         // GORP: error checking here: need to augment mpeg2ts_stream_read_ts_packet's error checking
      }
   }

   mpeg2ts_stream_free(m2s);

   fclose(infile);

   cleanupAndExit(ebpFileIngestThreadParams);

   return NULL;
}

void findFIFO (uint32_t PID, ebp_stream_info_t **streamInfos, int numStreamInfos,
   thread_safe_fifo_t**fifoOut, int *fifoIndex)
{
   *fifoOut = NULL;
   *fifoIndex = -1;

   for (int i=0; i<numStreamInfos; i++)
   {
      if (streamInfos[i] != NULL)
      {
         if (PID == streamInfos[i]->PID)
         {
            *fifoOut = streamInfos[i]->fifo;
            *fifoIndex = i;
            return;
         }
      }
   }
}

int postToFIFO  (uint64_t PTS, uint32_t sapType, ebp_t *ebp, ebp_descriptor_t *ebpDescriptor,
                 uint32_t PID, ebp_file_ingest_thread_params_t *ebpFileIngestThreadParams)
{
   ebp_segment_info_t *ebpSegmentInfo = 
      (ebp_segment_info_t *)malloc (sizeof (ebp_segment_info_t));

   ebpSegmentInfo->PTS = PTS;
   ebpSegmentInfo->SAPType = sapType;
   ebpSegmentInfo->EBP = ebp;
   // make a copy of ebp_descriptor since this mem could be freed before being prcessed by analysis thread
   ebpSegmentInfo->latestEBPDescriptor = ebp_descriptor_copy(ebpDescriptor);  
   
   thread_safe_fifo_t* fifo = NULL;
   int fifoIndex = -1;
   findFIFO (PID, ebpFileIngestThreadParams->streamInfos, ebpFileIngestThreadParams->numStreamInfos, &fifo, &fifoIndex);
   if (fifo == NULL)
   {
      // this should never happen
      LOG_ERROR_ARGS ("EBPFileIngestThread %d: FAIL: FIFO not found for PID %d", 
         ebpFileIngestThreadParams->threadNum, PID);
      return -1;
   }

   LOG_INFO_ARGS ("EBPFileIngestThread %d: POSTING PTS %"PRId64" to FIFO (PID %d)", 
      ebpFileIngestThreadParams->threadNum, PTS, PID);
   int returnCode = fifo_push (fifo, ebpSegmentInfo);
   if (returnCode != 0)
   {
      LOG_ERROR_ARGS ("EBPFileIngestThread %d: FATAL error %d calling fifo_push on fifo %d (PID %d)", 
         ebpFileIngestThreadParams->threadNum, 
         returnCode, (ebpFileIngestThreadParams->streamInfos[fifoIndex])->fifo->id, PID);
      exit (-1);
   }

   return 0;
}

void cleanupAndExit(ebp_file_ingest_thread_params_t *ebpFileIngestThreadParams)
{
   int returnCode = 0;
   void *element = NULL;
   for (int i=0; i<ebpFileIngestThreadParams->numStreamInfos; i++)
   {
      returnCode = fifo_push (ebpFileIngestThreadParams->streamInfos[i]->fifo, element);
      if (returnCode != 0)
      {
         LOG_ERROR_ARGS ("EBPFileIngestThread %d: FATAL error %d calling fifo_push on fifo %d (PID %d) during cleanup", 
            ebpFileIngestThreadParams->threadNum, returnCode, 
            (ebpFileIngestThreadParams->streamInfos[i])->fifo->id, (ebpFileIngestThreadParams->streamInfos[i])->PID);

         // fatal error: exit
         exit(-1);
      }
   }

   LOG_INFO_ARGS ("EBPFileIngestThread %d exiting...", ebpFileIngestThreadParams->threadNum);
   free (ebpFileIngestThreadParams);
   pthread_exit(NULL);
}



