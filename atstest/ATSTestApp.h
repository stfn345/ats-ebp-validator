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


#ifndef __H_ATSTESTAPP_767JKS
#define __H_ATSTESTAPP_767JKS

#define TS_SIZE 188

typedef struct
{
   int numStreams;
   uint32_t *stream_types;
   uint32_t *PIDs;
   ebp_descriptor_t **ebpDescriptors;

} program_stream_info_t;

ebp_boundary_info_t *setupDefaultBoundaryInfoArray()
{
   ebp_boundary_info_t * boundaryInfoArray = (ebp_boundary_info_t *) calloc (EBP_NUM_PARTITIONS, sizeof(ebp_boundary_info_t));

   // by default, segments and fragments are assumed to be boundaries
   boundaryInfoArray[1].isBoundary = 1;
   boundaryInfoArray[2].isBoundary = 1;

   return boundaryInfoArray;
}

void printBoundaryInfoArray(ebp_boundary_info_t *boundaryInfoArray);
int modBoundaryInfoArray (ebp_descriptor_t * ebpDescriptor, ebp_boundary_info_t *boundaryInfoArray);
int prereadFiles(int numFiles, char **fileNames, program_stream_info_t *programStreamInfo);
int get2DArrayIndex (int fileIndex, int streamIndex, int numStreams);
int teardownQueues(int numFiles, int numStreams, ebp_stream_info_t **streamInfoArray);
int getVideoPID(program_stream_info_t *programStreamInfo, uint32_t *PIDOut, uint32_t *streamType);
int getAudioPID(program_stream_info_t *programStreamInfo, uint32_t PIDIn, uint32_t *PIDOut, uint32_t *streamType);
varray_t *getUniqueAudioPIDArray(int numFiles, program_stream_info_t *programStreamInfo);
int setupQueues(int numFiles, char **fileNames, program_stream_info_t *programStreamInfo,
                ebp_stream_info_t ***streamInfoArray, int *numStreamsPerFile);
int startThreads(int numFiles, int totalNumStreams, ebp_stream_info_t **streamInfoArray, char **fileNames,
   int *filePassFails, pthread_t ***fileIngestThreads, pthread_t ***analysisThreads, pthread_attr_t *threadAttr);
int waitForThreadsToExit(int numFiles, int totalNumStreams,
   pthread_t **fileIngestThreads, pthread_t **analysisThreads, pthread_attr_t *threadAttr);
void analyzeResults(int numFiles, int numStreams, ebp_stream_info_t **streamInfoArray, char **fileNames,
   int *filePassFails);




#endif // __H_ATSTESTAPP_767JKS