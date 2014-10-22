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

#ifndef __H_EBPFILEINGESTTHREAD_67511FLKKJF
#define __H_EBPFILEINGESTTHREAD_67511FLKKJF

#include "ThreadSafeFIFO.h"

typedef struct 
{
    int threadNum;
    char *filePath;
    int numStreamInfos;
    ebp_stream_info_t **streamInfos;

} ebp_file_ingest_thread_params_t;

void cleanupAndExit(ebp_file_ingest_thread_params_t *ebpFileIngestThreadParams);
void postToFIFO (uint64_t PTS, uint32_t sapType, ebp_t *ebp, ebp_descriptor_t *ebpDescriptor,
                 uint32_t PID, ebp_file_ingest_thread_params_t *ebpFileIngestThreadParams);
ebp_descriptor_t* getEBPDescriptor (elementary_stream_info_t *esi);
void findFIFO (uint32_t PID, ebp_stream_info_t **streamInfos, int numStreamInfos,
   thread_safe_fifo_t**fifoOut, int *fifoIndex);
ebp_t* getEBP(ts_packet_t *ts);

void *EBPFileIngestThreadProc(void *threadParams);

#endif  // __H_EBPFILEINGESTTHREAD_67511FLKKJF
