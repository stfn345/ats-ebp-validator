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


#ifndef __H_ATSTESTAPP_767JKS
#define __H_ATSTESTAPP_767JKS

#include "EBPStreamBuffer.h"

//#define PREREAD_EBP_SEARCH_TIME_MSECS   10000
//#define PREREAD_EBP_SEARCH_TIME_MSECS   10000000


ebp_boundary_info_t *setupDefaultBoundaryInfoArray();
void printBoundaryInfoArray(ebp_boundary_info_t *boundaryInfoArray);
int modBoundaryInfoArray (ebp_descriptor_t *ebpDescriptor, ebp_t *ebp, ebp_boundary_info_t *boundaryInfoArray,
   int currentFileIndex, int currentStreamIndex, program_stream_info_t *programStreamInfo, int numFiles,
   ebp_stream_info_t *videoStreamInfo);
void populateProgramStreamInfo(program_stream_info_t *programStreamInfo, mpeg2ts_program_t *m2p);

void printStreamInfo(int numIngests, int numStreams, ebp_stream_info_t **streamInfoArray, char **ingestNames,
                     program_stream_info_t *programStreamInfo);
void freeProgramStreamInfo (program_stream_info_t *programStreamInfo);

int prereadFiles(int numFiles, char **fileNames, program_stream_info_t *programStreamInfo);
int prereadIngestStreams(int numIngestStreams, circular_buffer_t **ingestBuffers, program_stream_info_t *programStreamInfo);

int teardownQueues(int numFiles, int numStreams, ebp_stream_info_t **streamInfoArray);
int getVideoPID(program_stream_info_t *programStreamInfo, uint32_t *PIDOut, uint32_t *streamType);
int getAudioPID(program_stream_info_t *programStreamInfo, char *languageIn, uint32_t PIDIn, uint32_t *PIDOut, uint32_t *streamType);
int getStreamIndex(program_stream_info_t *programStreamInfo, uint32_t PID, int *streamIndexOut);
int getFileWithPID(program_stream_info_t *programStreamInfo, int numFiles, uint32_t PID, int *fileIndexWithPID);
varray_t *getUniqueAudioIDArray(int numFiles, program_stream_info_t *programStreamInfo, int *useLanguageAsID);

int setupQueues(int numIngests, program_stream_info_t *programStreamInfo,
                ebp_stream_info_t ***streamInfoArray, int *numStreamsPerIngest);

void runFileIngestMode(int numFiles, char **filePaths, int peekFlag);
void runStreamIngestMode(int numngestStreams, char **ingestAddrs, int peekFlag, int enableStreamDump);

int parseMulticastAddrArg (char *inputArg, unsigned long *pIP, unsigned long *psrcIP, unsigned short *pPort);

int startThreads_FileIngest(int numFiles, int totalNumStreams, ebp_stream_info_t **streamInfoArray, char **fileNames,
   int *filePassFails, pthread_t ***fileIngestThreads, pthread_t ***analysisThreads, pthread_attr_t *threadAttr);
int startThreads_StreamIngest(int numIngestStreams, int totalNumStreams, ebp_stream_info_t **streamInfoArray, circular_buffer_t **ingestBuffers,
   int *filePassFails, pthread_t ***streamIngestThreads, pthread_t ***analysisThreads, pthread_attr_t *threadAttr,
   ebp_stream_ingest_thread_params_t ***ebpStreamIngestThreadParamsOut);

int startSocketReceiveThreads (int numIngestStreams, char **mcastAddrs, circular_buffer_t **ingestBuffers,
   pthread_t ***socketReceiveThreads, pthread_attr_t *threadAttr, ebp_socket_receive_thread_params_t ***ebpSocketReceiveThreadParams,
   int enableStreamDump);
int stopSocketReceiveThreads (int numIngestStreams, pthread_t **socketReceiveThreads, 
   pthread_attr_t *threadAttr, ebp_socket_receive_thread_params_t **ebpSocketReceiveThreadParams);
int waitForSocketReceiveThreadsToExit(int numIngestStreams,
   pthread_t **socketReceiveThreads, pthread_attr_t *threadAttr);

int waitForThreadsToExit(int numIngests, int totalNumStreams,
   pthread_t **ingestThreads, pthread_t **analysisThreads, pthread_attr_t *threadAttr);
void analyzeResults(int numIngests, int numStreams, ebp_stream_info_t **streamInfoArray, char **ingestNames,
   int *filePassFails);

void aphabetizeLanguageDescriptorLanguages (language_descriptor_t* languageDescriptor);
void aphabetizeStringArray(char **stringArray, int stringArraySz);

int getFileWithVideoPID(program_stream_info_t *programStreamInfo, int numFiles, uint32_t PID, 
                        int *fileIndexWithPID);

void printIngestStatus (ebp_socket_receive_thread_params_t **ebpSocketReceiveThreadParams, int numIngestStreams, int numStreams,
                        ebp_stream_info_t **streamInfoArray);




#endif // __H_ATSTESTAPP_767JKS
