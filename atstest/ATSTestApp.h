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
#define PREREAD_EBP_SEARCH_TIME_MSECS   10000

typedef struct
{
   int numStreams;
   uint32_t *stream_types;
   uint32_t *PIDs;
   ebp_descriptor_t **ebpDescriptors;
   ebp_t **ebps;

   // concatenation of language, component name, and AC3 language
   char **language;

} program_stream_info_t;


ebp_boundary_info_t *setupDefaultBoundaryInfoArray();
void printBoundaryInfoArray(ebp_boundary_info_t *boundaryInfoArray);
int modBoundaryInfoArray (ebp_descriptor_t *ebpDescriptor, ebp_t *ebp, ebp_boundary_info_t *boundaryInfoArray,
   int currentFileIndex, int currentStreamIndex, program_stream_info_t *programStreamInfo, int numFiles,
   ebp_stream_info_t *videoStreamInfo);
void populateProgramStreamInfo(program_stream_info_t *programStreamInfo, mpeg2ts_program_t *m2p);

void printStreamInfo(int numFiles, int numStreams, ebp_stream_info_t **streamInfoArray, char **fileNames);
void freeProgramStreamInfo (program_stream_info_t *programStreamInfo);

int prereadFiles(int numFiles, char **fileNames, program_stream_info_t *programStreamInfo);
int teardownQueues(int numFiles, int numStreams, ebp_stream_info_t **streamInfoArray);
int getVideoPID(program_stream_info_t *programStreamInfo, uint32_t *PIDOut, uint32_t *streamType);
int getAudioPID(program_stream_info_t *programStreamInfo, char *languageIn, uint32_t PIDIn, uint32_t *PIDOut, uint32_t *streamType);
int getStreamIndex(program_stream_info_t *programStreamInfo, uint32_t PID, int *streamIndexOut);
int getFileWithPID(program_stream_info_t *programStreamInfo, int numFiles, uint32_t PID, int *fileIndexWithPID);
varray_t *getUniqueAudioIDArray(int numFiles, program_stream_info_t *programStreamInfo, int *useLanguageAsID);
int setupQueues(int numFiles, char **fileNames, program_stream_info_t *programStreamInfo,
                ebp_stream_info_t ***streamInfoArray, int *numStreamsPerFile);
int startThreads(int numFiles, int totalNumStreams, ebp_stream_info_t **streamInfoArray, char **fileNames,
   int *filePassFails, pthread_t ***fileIngestThreads, pthread_t ***analysisThreads, pthread_attr_t *threadAttr);
int waitForThreadsToExit(int numFiles, int totalNumStreams,
   pthread_t **fileIngestThreads, pthread_t **analysisThreads, pthread_attr_t *threadAttr);
void analyzeResults(int numFiles, int numStreams, ebp_stream_info_t **streamInfoArray, char **fileNames,
   int *filePassFails);

void aphabetizeLanguageDescriptorLanguages (language_descriptor_t* languageDescriptor);
void aphabetizeStringArray(char **stringArray, int stringArraySz);

int getFileWithVideoPID(program_stream_info_t *programStreamInfo, int numFiles, uint32_t PID, 
                        int *fileIndexWithPID);


#endif // __H_ATSTESTAPP_767JKS