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



static int validate_ts_packet(ts_packet_t *ts, elementary_stream_info_t *es_info, void *arg)
{
   return 1;
}

static int validate_pes_packet(pes_packet_t *pes, elementary_stream_info_t *esi, vqarray_t *ts_queue, void *arg)
{
   // Get the first TS packet and check it for EBP
   ts_packet_t *ts = (ts_packet_t*)vqarray_get(ts_queue,0);
   if (ts == NULL)
   {
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
      // GORP: this should never happen
      pes_free(pes);
      return 1;
   }

   ebp_t* ebp = getEBP(ts);
   if (ebp != NULL)
   {
         char *streamTypeDesc = "UNKNOWN";
         if (IS_VIDEO_STREAM(esi->stream_type)) streamTypeDesc = "VIDEO";
         if (IS_AUDIO_STREAM(esi->stream_type)) streamTypeDesc = "AUDIO";

         LOG_DEBUG_ARGS("Found EBP data in transport packet: PID %d (%s)", esi->elementary_PID, streamTypeDesc);
   }

   if (streamInfo->ebpImplicit && streamInfo->lastVideoChunkPTSValid)
   {
      if (ebp != NULL)
      {
         // GORP: FAIL HERE: implicit EBP, but found explicit EBP
         pes_free(pes);
         return 1;
      }

      if (streamInfo->lastVideoChunkPTSValid && pes->header.PTS > streamInfo->lastVideoChunkPTS)
      {
         // GORP: handle implicit case

         streamInfo->lastVideoChunkPTSValid = 0;
      }
   }
   else if (!streamInfo->ebpImplicit)
   {
      if (ebp != NULL)
      {
         uint32_t sapType = 0;  // GORP: fill in
         ebp_descriptor_t* ebpDescriptor = getEBPDescriptor (esi);  // could be null

         postToFIFO (pes->header.PTS, sapType, ebp, ebpDescriptor, esi->elementary_PID, ebpFileIngestThreadParams);

         if (IS_VIDEO_STREAM(esi->stream_type))
         {
            // GORP: if this is video, set lastVideoPTS in implicit fifos
         }
      }
   }

   pes_free(pes);
   return 1;
}

ebp_t* getEBP(ts_packet_t *ts)
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
            LOG_ERROR("FAIL: Multiple EBP structures detected with a single PES packet!  Not allowed!");
            return NULL;
         }

         found_ebp = 1;

         if (!ts->header.payload_unit_start_indicator) 
         {
            LOG_ERROR("FAIL: EBP present on a TS packet that does not have PUSI bit set!");
            return NULL;
         }

         // Parse the EBP
         ebp = ebp_new();
         if (!ebp_read(ebp, scte128))
         {
            LOG_ERROR("FAIL: Error parsing EBP!");
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
   printf ("pmt_processor: arg = %x\n", (unsigned int) arg);
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
   printf("EBPFileIngestThread #%d starting...ebpFileIngestThreadParams = %x\n", 
      ebpFileIngestThreadParams->threadNum, (unsigned int)ebpFileIngestThreadParams);

   // do file reading here
   FILE *infile = NULL;
   if ((infile = fopen(ebpFileIngestThreadParams->filePath, "rb")) == NULL)
   {
      LOG_ERROR_ARGS("FAIL: Cannot open file %s - %s", ebpFileIngestThreadParams->filePath, strerror(errno));
      cleanupAndExit(ebpFileIngestThreadParams);
   }

   mpeg2ts_stream_t *m2s = NULL;

   if (NULL == (m2s = mpeg2ts_stream_new()))
   {
      LOG_ERROR("Error creating MPEG-2 STREAM object");
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
      LOG_ERROR("Could not register EBP descriptor parser!");
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
      printf ("total_packets = %d, num_packets = %d\n", total_packets, num_packets);
      for (int i = 0; i < num_packets; i++)
      {
         ts_packet_t *ts = ts_new();
         ts_read(ts, ts_buf + i * TS_SIZE, TS_SIZE);
         mpeg2ts_stream_read_ts_packet(m2s, ts);
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
      if (PID == streamInfos[i]->PID)
      {
         *fifoOut = streamInfos[i]->fifo;
         *fifoIndex = i;
         return;
      }
   }
}

void postToFIFO (uint64_t PTS, uint32_t sapType, ebp_t *ebp, ebp_descriptor_t *ebpDescriptor,
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
      // GORP: this should never happen
      return;
   }

   printf ("EBPFileIngestThread (%d): POSTING PTS %"PRId64" to FIFO (PID %d)\n", ebpFileIngestThreadParams->threadNum,
      PTS, PID);
   int returnCode = fifo_push (fifo, ebpSegmentInfo);
   if (returnCode != 0)
   {
      printf ("EBPFileIngestThread (%d) error %d calling fifo_push on fifo %d (PID %d)\n", ebpFileIngestThreadParams->threadNum, 
         returnCode, (ebpFileIngestThreadParams->streamInfos[fifoIndex])->fifo->id, PID);
      // GORP: do something here??
   }
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
         printThreadDebugMessage ("EBPFileIngestThread #%d error %d calling fifo_push\n", ebpFileIngestThreadParams->threadNum, returnCode);
         // GORP: do something here??
      }
   }

   printThreadDebugMessage("EBPFileIngestThread #%d exiting...\n", ebpFileIngestThreadParams->threadNum);
   free (ebpFileIngestThreadParams);
   pthread_exit(NULL);
}



