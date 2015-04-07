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

#ifndef __H_EBPSEGMENTANALYSISTHREAD_67511FLKKJF
#define __H_EBPSEGMENTANALYSISTHREAD_67511FLKKJF

#include "EBPCommon.h"

typedef struct 
{  
   int threadID;
   int numFiles;
   ebp_stream_info_t **streamInfos;

} ebp_segment_analysis_thread_params_t;

typedef struct
{
   int64_t PTS;
   uint32_t SAPType;
   uint8_t partitionId;

   ebp_t *EBP;
   ebp_descriptor_t *latestEBPDescriptor;

} ebp_segment_info_t;


void cleanupEBPSegmentInfo (ebp_segment_info_t *ebpSegmentInfo);
void *EBPSegmentAnalysisThreadProc(void *threadParams);
int syncIncomingStreams (int threadID, int numFiles, ebp_stream_info_t **streamInfos, int *fifoNotActive);
void checkDistanceFromLastPTS(int threadID, ebp_stream_info_t *streamInfo, ebp_segment_info_t *ebpSegmentInfo,
                              int fifoId);


#endif  // __H_EBPSEGMENTANALYSISTHREAD_67511FLKKJF
