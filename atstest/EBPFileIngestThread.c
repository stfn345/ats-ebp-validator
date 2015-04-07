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

#include <mpeg2ts_demux.h>
#include <libts_common.h>
#include <tpes.h>
#include <ebp.h>

#include "h264_stream.h"

#include "EBPSegmentAnalysisThread.h"
#include "EBPIngestThreadCommon.h"
#include "EBPFileIngestThread.h"
#include "EBPThreadLogging.h"
#include "ATSTestDefines.h"
#include "ATSTestReport.h"


void *EBPFileIngestThreadProc(void *threadParams)
{
   int returnCode = 0;

   ebp_file_ingest_thread_params_t * ebpFileIngestThreadParams = (ebp_file_ingest_thread_params_t *)threadParams;
   LOG_INFO_ARGS("EBPFileIngestThread %d starting...ebpFileIngestThreadParams = %p", 
      ebpFileIngestThreadParams->ebpIngestThreadParams->threadNum, ebpFileIngestThreadParams);

   // do file reading here
   FILE *infile = NULL;
   if ((infile = fopen(ebpFileIngestThreadParams->filePath, "rb")) == NULL)
   {
      LOG_ERROR_ARGS("EBPFileIngestThread %d: FAIL: Cannot open file %s - %s", 
         ebpFileIngestThreadParams->ebpIngestThreadParams->threadNum, ebpFileIngestThreadParams->filePath, strerror(errno));
      reportAddErrorLogArgs("EBPFileIngestThread %d: FAIL: Cannot open file %s - %s", 
         ebpFileIngestThreadParams->ebpIngestThreadParams->threadNum, ebpFileIngestThreadParams->filePath, strerror(errno));

      ebpFileIngestThreadParams->ebpIngestThreadParams->ingestPassFail = 0;
      cleanupAndExit(ebpFileIngestThreadParams);
   }

   mpeg2ts_stream_t *m2s = NULL;

   if (NULL == (m2s = mpeg2ts_stream_new()))
   {
      LOG_ERROR_ARGS("EBPFileIngestThread %d: FAIL: Error creating MPEG-2 STREAM object",
         ebpFileIngestThreadParams->ebpIngestThreadParams->threadNum);
      reportAddErrorLogArgs("EBPFileIngestThread %d: FAIL: Error creating MPEG-2 STREAM object",
         ebpFileIngestThreadParams->ebpIngestThreadParams->threadNum);
      ebpFileIngestThreadParams->ebpIngestThreadParams->ingestPassFail = 0;
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
         ebpFileIngestThreadParams->ebpIngestThreadParams->threadNum);
      reportAddErrorLogArgs("EBPFileIngestThread %d: FAIL: Could not register EBP descriptor parser",
         ebpFileIngestThreadParams->ebpIngestThreadParams->threadNum);
      ebpFileIngestThreadParams->ebpIngestThreadParams->ingestPassFail = 0;

      cleanupAndExit(ebpFileIngestThreadParams);
   }

   m2s->pat_processor = (pat_processor_t)ingest_pat_processor;
   m2s->arg = ebpFileIngestThreadParams->ebpIngestThreadParams;
   m2s->arg_destructor = NULL;

   int num_packets = 4096;
   uint8_t *ts_buf = malloc(TS_SIZE * 4096);

   int total_packets = 0;

   while ((num_packets = fread(ts_buf, TS_SIZE, 4096, infile)) > 0)
   {
      total_packets += num_packets;
      LOG_INFO_ARGS ("total_packets = %d, num_packets = %d", total_packets, num_packets);
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

void cleanupAndExit(ebp_file_ingest_thread_params_t *ebpFileIngestThreadParams)
{
   int arrayIndex = get2DArrayIndex (ebpFileIngestThreadParams->ebpIngestThreadParams->threadNum, 0, 
      ebpFileIngestThreadParams->ebpIngestThreadParams->numStreams);
   ebp_stream_info_t **streamInfos = &(ebpFileIngestThreadParams->ebpIngestThreadParams->allStreamInfos[arrayIndex]);

   int returnCode = 0;
   void *element = NULL;
   for (int i=0; i<ebpFileIngestThreadParams->ebpIngestThreadParams->numStreams; i++)
   {
      returnCode = fifo_push (streamInfos[i]->fifo, element);
      if (returnCode != 0)
      {
         LOG_ERROR_ARGS ("EBPFileIngestThread %d: FATAL error %d calling fifo_push on fifo %d (PID %d) during cleanup", 
            ebpFileIngestThreadParams->ebpIngestThreadParams->threadNum, returnCode, 
            streamInfos[i]->fifo->id, streamInfos[i]->PID);
         reportAddErrorLogArgs ("EBPFileIngestThread %d: FATAL error %d calling fifo_push on fifo %d (PID %d) during cleanup", 
            ebpFileIngestThreadParams->ebpIngestThreadParams->threadNum, returnCode, 
            streamInfos[i]->fifo->id, streamInfos[i]->PID);

         // fatal error: exit
         exit(-1);
      }
   }

   LOG_INFO_ARGS ("EBPFileIngestThread %d exiting...", ebpFileIngestThreadParams->ebpIngestThreadParams->threadNum);
   free (ebpFileIngestThreadParams->ebpIngestThreadParams);
   free (ebpFileIngestThreadParams);
   pthread_exit(NULL);
}


