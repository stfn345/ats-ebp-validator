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

#include "h264_stream.h"

#include "EBPSegmentAnalysisThread.h"
#include "EBPIngestThreadCommon.h"
#include "EBPStreamIngestThread.h"
#include "EBPThreadLogging.h"
#include "ATSTestDefines.h"

void *EBPStreamIngestThreadProc(void *threadParams)
{
   int returnCode = 0;

   ebp_stream_ingest_thread_params_t * ebpStreamIngestThreadParams = (ebp_stream_ingest_thread_params_t *)threadParams;
   LOG_INFO_ARGS("EBPStreamIngestThread %d starting...ebpStreamIngestThreadParams = %p", 
      ebpStreamIngestThreadParams->ebpIngestThreadParams->threadNum, ebpStreamIngestThreadParams);

   mpeg2ts_stream_t *m2s = NULL;

   if (NULL == (m2s = mpeg2ts_stream_new()))
   {
      LOG_ERROR_ARGS("EBPStreamIngestThread %d: FAIL: Error creating MPEG-2 STREAM object",
         ebpStreamIngestThreadParams->ebpIngestThreadParams->threadNum);
      reportAddErrorLogArgs("EBPStreamIngestThread %d: FAIL: Error creating MPEG-2 STREAM object",
         ebpStreamIngestThreadParams->ebpIngestThreadParams->threadNum);
      ebpStreamIngestThreadParams->ebpIngestThreadParams->ingestPassFail = 0;
      streamIngestCleanup(ebpStreamIngestThreadParams);
   }

   // Register EBP descriptor parser
   descriptor_table_entry_t *desc = calloc(1, sizeof(descriptor_table_entry_t));
   desc->tag = EBP_DESCRIPTOR;
   desc->free_descriptor = ebp_descriptor_free;
   desc->print_descriptor = ebp_descriptor_print;
   desc->read_descriptor = ebp_descriptor_read;
   if (!register_descriptor(desc))
   {
      LOG_ERROR_ARGS("EBPStreamIngestThread %d: FAIL: Could not register EBP descriptor parser",
         ebpStreamIngestThreadParams->ebpIngestThreadParams->threadNum);
      reportAddErrorLogArgs("EBPStreamIngestThread %d: FAIL: Could not register EBP descriptor parser",
         ebpStreamIngestThreadParams->ebpIngestThreadParams->threadNum);
      ebpStreamIngestThreadParams->ebpIngestThreadParams->ingestPassFail = 0;

      streamIngestCleanup(ebpStreamIngestThreadParams);
   }

   m2s->pat_processor = (pat_processor_t)ingest_pat_processor;
   m2s->arg = ebpStreamIngestThreadParams->ebpIngestThreadParams;
   m2s->arg_destructor = NULL;

   int num_packets = 4096;
   int num_bytes = 0;
   int ts_buf_sz = num_packets * TS_SIZE;
   uint8_t *ts_buf = malloc(num_packets * TS_SIZE);

   int total_packets = 0;

   while (((num_bytes = cb_read (ebpStreamIngestThreadParams->cb, ts_buf, ts_buf_sz)) > 0))
   {
      if (num_bytes % TS_SIZE)
      {
         LOG_ERROR_ARGS ("EBPStreamIngestThread %d: FAIL: Bytes read not a multiple of TS packets: %d", 
            ebpStreamIngestThreadParams->ebpIngestThreadParams->threadNum, num_bytes);
         reportAddErrorLogArgs ("EBPStreamIngestThread %d: FAIL: Bytes read not a multiple of TS packets: %d", 
            ebpStreamIngestThreadParams->ebpIngestThreadParams->threadNum, num_bytes);
      }
      num_packets = num_bytes / TS_SIZE;

 //     LOG_INFO_ARGS ("buf: 0x%x, 0x%x, 0x%x, 0x%x", ts_buf[0], ts_buf[1], ts_buf[2], ts_buf[3]);
      for (int i = 0; i < num_packets; i++)
      {
         total_packets++;
//         LOG_INFO_ARGS ("packet #%d, total_packets = %d", i, total_packets);
         ts_packet_t *ts = ts_new();
         ts_read(ts, ts_buf + i * TS_SIZE, TS_SIZE);
         int returnCode = mpeg2ts_stream_read_ts_packet(m2s, ts);
         // GORP: error checking here: need to augment mpeg2ts_stream_read_ts_packet's error checking
      }

//      LOG_INFO_ARGS ("total_packets = %d", total_packets);
   }

   mpeg2ts_stream_free(m2s);

   streamIngestCleanup(ebpStreamIngestThreadParams);

   return NULL;
}

void streamIngestCleanup(ebp_stream_ingest_thread_params_t *ebpStreamIngestThreadParams)
{
   int arrayIndex = get2DArrayIndex (ebpStreamIngestThreadParams->ebpIngestThreadParams->threadNum, 0, 
      ebpStreamIngestThreadParams->ebpIngestThreadParams->numStreams);
   ebp_stream_info_t **streamInfos = &(ebpStreamIngestThreadParams->ebpIngestThreadParams->allStreamInfos[arrayIndex]);

   int returnCode = 0;
   void *element = NULL;
   for (int i=0; i<ebpStreamIngestThreadParams->ebpIngestThreadParams->numStreams; i++)
   {
      returnCode = fifo_push (streamInfos[i]->fifo, element);
      if (returnCode != 0)
      {
         LOG_ERROR_ARGS ("EBPStreamIngestThread %d: FATAL error %d calling fifo_push on fifo %d (PID %d) during cleanup", 
            ebpStreamIngestThreadParams->ebpIngestThreadParams->threadNum, returnCode, 
            streamInfos[i]->fifo->id, streamInfos[i]->PID);
         reportAddErrorLogArgs ("EBPStreamIngestThread %d: FATAL error %d calling fifo_push on fifo %d (PID %d) during cleanup", 
            ebpStreamIngestThreadParams->ebpIngestThreadParams->threadNum, returnCode, 
            streamInfos[i]->fifo->id, streamInfos[i]->PID);

         // fatal error: exit
         exit(-1);
      }
   }

   LOG_INFO_ARGS ("EBPStreamIngestThread %d exiting...", ebpStreamIngestThreadParams->ebpIngestThreadParams->threadNum);
   printf ("EBPStreamIngestThread %d exiting...\n", ebpStreamIngestThreadParams->ebpIngestThreadParams->threadNum);
   free (ebpStreamIngestThreadParams->ebpIngestThreadParams);
   free (ebpStreamIngestThreadParams);
   //pthread_exit(NULL);
}


