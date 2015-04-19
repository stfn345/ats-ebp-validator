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

#ifndef __H_EBP_COMMON_AGS6Q76
#define __H_EBP_COMMON_AGS6Q76

#include "ThreadSafeFIFO.h"
#include "log.h"
#include "varray.h"
#include "ebp.h"


#define EBP_NUM_PARTITIONS 10  // 0 - 9
//#define EBP_ALLOWED_PTS_JITTER_SECS  2  // if the expected period is given in the EBP descriptor, then this value
                                        // gives the required accuracy in conforming to that period
//#define SCTE35_EBP_PTS_JITTER_SECS  2  // the allowed timing jitter between SCTE35 points and their accompanying EBPs
#define SAP_STREAM_TYPE_NOT_SUPPORTED  99
#define SAP_STREAM_TYPE_ERROR  100

typedef struct
{
   int isBoundary;
   int isImplicit;

   int implicitFileIndex;  // implicit boundaries can trigger off of other files
   uint32_t implicitPID; // only applicable if implicit -- default is video PID

   varray_t* queueLastImplicitPTS;

   varray_t* listSCTE35;

   uint64_t lastPTS;

} ebp_boundary_info_t;

typedef struct
{
   uint32_t PID;  // PID within program for this fifo
   int isVideo; // =1 if this fifo carries video info, =0 if audio

   uint64_t lastVideoChunkPTS;
   int lastVideoChunkPTSValid;

   ebp_boundary_info_t* ebpBoundaryInfo;  // contain info on which partitions are boundaries

   int streamPassFail;  // 1 == pass, 0 == fail

   thread_safe_fifo_t *fifo;

   int ebpChunkCntr;

} ebp_stream_info_t;

typedef struct
{
   int numStreams;
   uint32_t *stream_types;
   uint32_t *PIDs;
   ebp_descriptor_t **ebpDescriptors;
   ebp_t **ebps;
   varray_t **ebpLists;

   // concatenation of language, component name, and AC3 language
   char **language;

} program_stream_info_t;



int get2DArrayIndex (int fileIndex, int streamIndex, int numStreams);


#endif  // __H_EBP_COMMON_AGS6Q76
