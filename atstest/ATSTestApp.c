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
#include <errno.h>
#include <mpeg2ts_demux.h>
#include <libts_common.h>
#include <tpes.h>
#include <ebp.h>
#include <pthread.h>
#include <getopt.h>

#include "EBPCommon.h"
#include "EBPFileIngestThread.h"
#include "EBPSegmentAnalysisThread.h"

#include "ATSTestApp.h"


static int g_bPATFound = 0;
static int g_bPMTFound = 0;
static int g_numVideoChunksFound = 0;

static int pmt_processor(mpeg2ts_program_t *m2p, void *arg);
static int pat_processor(mpeg2ts_stream_t *m2s, void *arg);

static struct option long_options[] = { 
    { "peek",	   no_argument,        NULL, 'p' }, 
    { "help",       no_argument,        NULL, 'h' }, 
}; 

static char options[] = 
"\t-p, --peek\n" 
"\t-h, --help\n"; 

static void usage() 
{ 
    fprintf(stderr, "\nATSTestApp\n"); 
    fprintf(stderr, "\nUsage: \nnATSTestApp [options] <input file 1> <input file 2> ... <input file N>\n\nOptions:\n%s\n", options);
}

void printBoundaryInfoArray(ebp_boundary_info_t *boundaryInfoArray)
{
   LOG_INFO ("      EBP Boundary Info:");

   for (int i=0; i<EBP_NUM_PARTITIONS; i++)
   {
      if (boundaryInfoArray[i].isBoundary)
      {
         if (boundaryInfoArray[i].isImplicit)
         {
            LOG_INFO_ARGS ("         PARTITION %d: IMPLICIT, PID = %d", i, boundaryInfoArray[i].implicitPID);
         }
         else
         {
            LOG_INFO_ARGS ("         PARTITION %d: EXPLICIT", i);
         }
      }
   }
}

int modBoundaryInfoArray (ebp_descriptor_t * ebpDescriptor, ebp_boundary_info_t *boundaryInfoArray)
{
   for (int i=0; i<ebpDescriptor->num_partitions; i++)
   {
      ebp_partition_data_t *partition = (ebp_partition_data_t *)vqarray_get(ebpDescriptor->partition_data, i);
      if (partition->partition_id > 9)
      {
         LOG_ERROR("Main:modBoundaryInfoArray: FAIL: PartitionID > 9 detected %s");
         return -1;
      }

      boundaryInfoArray[partition->partition_id].isBoundary = partition->boundary_flag;
      boundaryInfoArray[partition->partition_id].isImplicit = !(partition->ebp_data_explicit_flag);
      boundaryInfoArray[partition->partition_id].implicitPID = partition->ebp_pid;
   }

   return 0;
}

static int pmt_processor(mpeg2ts_program_t *m2p, void *arg)
{
   LOG_DEBUG ("pmt_processor");
   g_bPMTFound = 1;

   if (m2p == NULL || m2p->pmt == NULL) // if we don't have any PSI, there's nothing we can do
      return 0;

   return 1;
}

static int pat_processor(mpeg2ts_stream_t *m2s, void *arg)
{
   LOG_DEBUG ("pat_processor");
   g_bPATFound = 1;

   for (int i = 0; i < vqarray_length(m2s->programs); i++)
   {
      mpeg2ts_program_t *m2p = vqarray_get(m2s->programs, i);

      if (m2p == NULL) continue;
      m2p->pmt_processor =  (pmt_processor_t)pmt_processor;
      m2p->arg = arg;
   }
   return 1;
}

int prereadFiles(int numFiles, char **fileNames, program_stream_info_t *programStreamInfo)
{
   LOG_INFO ("prereadFiles: entering");

   for (int i=0; i<numFiles; i++)
   {
      LOG_INFO_ARGS ("Main:prereadFiles: FilePath %d = %s", i, fileNames[i]); 

      // reset PAT/PMT read flags
      g_bPATFound = 0;
      g_bPMTFound = 0;

      FILE *infile = NULL;
      if ((infile = fopen(fileNames[i], "rb")) == NULL)
      {
         LOG_ERROR_ARGS("Main:prereadFiles: Cannot open file %s - %s", fileNames[i], strerror(errno));
         return -1;
      }

      mpeg2ts_stream_t *m2s = NULL;

      if (NULL == (m2s = mpeg2ts_stream_new()))
      {
         LOG_ERROR_ARGS("Main:prereadFiles: Error creating MPEG-2 STREAM object for file %s", fileNames[i]);
         return -1;
      }

       // Register EBP descriptor parser
      descriptor_table_entry_t *desc = calloc(1, sizeof(descriptor_table_entry_t));
      desc->tag = EBP_DESCRIPTOR;
      desc->free_descriptor = ebp_descriptor_free;
      desc->print_descriptor = ebp_descriptor_print;
      desc->read_descriptor = ebp_descriptor_read;
      if (!register_descriptor(desc))
      {
         LOG_ERROR_ARGS("Main:prereadFiles: FAIL: Could not register EBP descriptor parser for file %s", fileNames[i]);
         return -1;
      }

      m2s->pat_processor = (pat_processor_t)pat_processor;
      m2s->arg = NULL;
      m2s->arg_destructor = NULL;

      int num_packets = 4096;
      uint8_t *ts_buf = malloc(TS_SIZE * 4096);

      while (!(g_bPATFound && g_bPMTFound) && 
         (num_packets = fread(ts_buf, TS_SIZE, 4096, infile)) > 0)
      {
         for (int i = 0; i < num_packets; i++)
         {
            ts_packet_t *ts = ts_new();
            ts_read(ts, ts_buf + i * TS_SIZE, TS_SIZE);
            mpeg2ts_stream_read_ts_packet(m2s, ts);

            // check if PAT?PMT read -- if so, break out
            if (g_bPATFound && g_bPMTFound)
            {
               LOG_DEBUG ("Main:prereadFiles: PAT/PMT found");
               break;
            }
         }
      }

      
      mpeg2ts_program_t *m2p = ( mpeg2ts_program_t *) vqarray_get(m2s->programs, 0);
      pid_info_t *pi = NULL;

      programStreamInfo[i].numStreams = vqarray_length(m2p->pids);
      programStreamInfo[i].stream_types = (uint32_t*) calloc (programStreamInfo[i].numStreams, sizeof (uint32_t));
      programStreamInfo[i].PIDs = (uint32_t*) calloc (programStreamInfo[i].numStreams, sizeof (uint32_t));
      programStreamInfo[i].ebpDescriptors = (ebp_descriptor_t**) calloc (programStreamInfo[i].numStreams, sizeof (ebp_descriptor_t *));

      for (int j = 0; j < vqarray_length(m2p->pids); j++)
      {
         if ((pi = vqarray_get(m2p->pids, j)) != NULL)
         {
            LOG_INFO_ARGS ("Main:prereadFiles: stream %d: stream_type = %u, elementary_PID = %u, ES_info_length = %u",
               j, pi->es_info->stream_type, pi->es_info->elementary_PID, pi->es_info->ES_info_length);

            programStreamInfo[i].stream_types[j] = pi->es_info->stream_type;
            programStreamInfo[i].PIDs[j] = pi->es_info->elementary_PID;

            ebp_descriptor_t* ebpDescriptor = getEBPDescriptor (pi->es_info);
            if (ebpDescriptor != NULL)
            {
               ebp_descriptor_print_stdout (ebpDescriptor);
               programStreamInfo[i].ebpDescriptors[j] = ebp_descriptor_copy(ebpDescriptor);
            }
            else
            {
               LOG_INFO ("NULL ebp_descriptor");
            }
         }
      }

      mpeg2ts_stream_free(m2s);

      fclose(infile);
   }

   LOG_INFO ("Main:prereadFiles: exiting");
   return 0;
}

int get2DArrayIndex (int fileIndex, int streamIndex, int numStreams)
{
   return fileIndex * numStreams + streamIndex;
}

int teardownQueues(int numFiles, int numStreams, ebp_stream_info_t **streamInfoArray)
{
   LOG_INFO ("Main:teardownQueues: entering");
   int returnCode = 0;

   for (int fileIndex=0; fileIndex < numFiles; fileIndex++)
   {
      for (int streamIndex=0; streamIndex < numStreams; streamIndex++)
      {
         int arrayIndex = get2DArrayIndex (fileIndex, streamIndex, numStreams);
         ebp_stream_info_t *streamInfo = streamInfoArray[arrayIndex];

         LOG_INFO_ARGS ("Main:teardownQueues: fifo (file_index = %d, streamIndex = %d, PID = %d) push_counter = %d, pop_counter = %d", 
            fileIndex, streamIndex, streamInfo->PID, streamInfo->fifo->push_counter, streamInfo->fifo->pop_counter);
         LOG_DEBUG_ARGS ("Main:teardownQueues: calling fifo_destroy for file %d stream %d", 
               fileIndex, streamIndex);
         returnCode = fifo_destroy (streamInfo->fifo);
         if (returnCode != 0)
         {
            LOG_ERROR_ARGS ("Main:teardownQueues: FAIL: error destroying queue for file %d stream %d",
               fileIndex, streamIndex);
            returnCode = -1;
         }

         free (streamInfo->fifo);
         free (streamInfo);
      }
   }

   free (streamInfoArray);

   LOG_INFO ("Main:teardownQueues: exiting");
   return returnCode;
}

int getVideoPID(program_stream_info_t *programStreamInfo, uint32_t *PIDOut, uint32_t *streamType)
{
   int videoFound = 0;
   for (int streamIndex = 0; streamIndex < programStreamInfo->numStreams; streamIndex++)
   {
      if (IS_VIDEO_STREAM((programStreamInfo->stream_types)[streamIndex]))
      {
         if (videoFound)
         {
            LOG_ERROR ("Main:getVideoPID: FAIL: more than one video stream found");
            return -1;
         }

         *PIDOut = (programStreamInfo->PIDs)[streamIndex];
         *streamType = (programStreamInfo->stream_types)[streamIndex];
         videoFound = 1;
      }
   }

   if (videoFound)
   {
      return 0;
   }
   else
   {
      LOG_ERROR ("Main:getVideoPID: FAIL: no video stream found");
      return -1;
   }
}

int getAudioPID(program_stream_info_t *programStreamInfo, uint32_t PIDIn, uint32_t *PIDOut, uint32_t *streamType)
{
   for (int streamIndex = 0; streamIndex < programStreamInfo->numStreams; streamIndex++)
   {
      if (IS_AUDIO_STREAM((programStreamInfo->stream_types)[streamIndex]))
      {
         if (PIDIn == (programStreamInfo->PIDs)[streamIndex])
         {
            *PIDOut = (programStreamInfo->PIDs)[streamIndex];
            *streamType = (programStreamInfo->stream_types)[streamIndex];
            return 0;
         }
      }
   }

   return -1;
}

varray_t *getUniqueAudioPIDArray(int numFiles, program_stream_info_t *programStreamInfo)
{
   LOG_INFO ("Main:getNumUniqueAudioStreams: entering");

  // count how many unique audio streams there are
   varray_t *audioStreamPIDArray = varray_new();
   for (int fileIndex=0; fileIndex < numFiles; fileIndex++)
   {
      for (int streamIndex = 0; streamIndex < programStreamInfo[fileIndex].numStreams; streamIndex++)
      {
         if (IS_AUDIO_STREAM((programStreamInfo[fileIndex].stream_types)[streamIndex]))
         {
            int foundPID = 0;
            for (int i=0; i<varray_length(audioStreamPIDArray); i++)
            {
               varray_elem_t* element = varray_get(audioStreamPIDArray, i);
               uint32_t PID = *((uint32_t *)element);
               if (PID == (programStreamInfo[fileIndex].PIDs)[streamIndex])
               {
                  foundPID = 1;
                  break;
               }
            }

            if (!foundPID)
            {
               LOG_INFO_ARGS ("Main:getNumUniqueAudioStreams: (%d, %d), adding PID %d", 
                  fileIndex, streamIndex, (programStreamInfo[fileIndex].PIDs)[streamIndex]);

               // add PID here
               varray_add (audioStreamPIDArray, &((programStreamInfo[fileIndex].PIDs)[streamIndex]));
            }
            else
            {
                LOG_INFO_ARGS ("Main:getNumUniqueAudioStreams: (%d, %d), NOT adding PID %d", 
                  fileIndex, streamIndex, (programStreamInfo[fileIndex].PIDs)[streamIndex]);

           }
         }
      }
   }
                
   LOG_INFO ("Main:getNumUniqueAudioStreams: exting");
   return audioStreamPIDArray;
}

int setupQueues(int numFiles, char **fileNames, program_stream_info_t *programStreamInfo,
                ebp_stream_info_t ***streamInfoArray, int *numStreamsPerFile)
{
   LOG_INFO ("Main:setupQueues: entering");

   int returnCode;

   // we need to set up a 2D array of queueus.  The array size is numFiles x numStreams
   // where numStreams is the total of all possible streams (video + all possible audio).
   // so if one file contains an audio stream and another file contains two other DIFFERENT
   // audio files, then the total number of streams is 4 (video + 3 audio).

   // GORP: we need to distinguish audio streams, but for now, distinguish by PID
   

   // count how many unique audio streams there are
   varray_t *audioStreamPIDArray = getUniqueAudioPIDArray(numFiles, programStreamInfo);
   int numStreams = varray_length(audioStreamPIDArray) + 1 /*video stream */;
   LOG_INFO_ARGS ("Main:setupQueues: numStreams = %d", numStreams);

   *numStreamsPerFile = numStreams;


   // *streamInfoArray is an array of stream_info pointers, not stream_infos.  if a particular stream_info is not
   // applicable for a file, that stream_info pointer can be left null.  In what follows we allocate stream info
   // objets for each strem within a file.  The first stream info is always the video, and then next ones are audio -- we
   // use the list of unique audio streams in the fileset obtained above to order the audio stream info list.
   // if a file does not contain an audio stream, the stream info object for that stream is left NULL -- this
   // is how the analysis threads know which stream info objects apply to a file.
   *streamInfoArray = (ebp_stream_info_t **) calloc (numFiles * numStreams, sizeof (ebp_stream_info_t*));

   for (int fileIndex=0; fileIndex < numFiles; fileIndex++)
   {
      for (int streamIndex=0; streamIndex < numStreams; streamIndex++)
      {
         int arrayIndex = get2DArrayIndex (fileIndex, streamIndex, numStreams);

         uint32_t PID;
         uint32_t streamType;
         if (streamIndex == 0)
         {
            returnCode = getVideoPID(&(programStreamInfo[fileIndex]), &PID, &streamType);
            if (returnCode != 0)
            {
               // no video stream -- this is legal
               LOG_INFO_ARGS ("Main:setupQueues: no video stream for file %d", fileIndex);
               continue;
            }
         }
         else 
         {
            PID = *((uint32_t *)varray_get (audioStreamPIDArray, streamIndex-1));
            uint32_t PIDTemp;
            returnCode = getAudioPID(&(programStreamInfo[fileIndex]), PID, &PIDTemp, &streamType);
            if (returnCode != 0)
            {
               // this audio stream doesn't exist, so leave stream info NULL
               LOG_INFO_ARGS ("Main:setupQueues: (%d, %d) skipping fifo creation for PID %d", 
                  fileIndex, streamIndex, PID);
               continue;
            }         
         }

         LOG_INFO_ARGS ("Main:setupQueues: (%d, %d) creating fifo for PID %d", 
                  fileIndex, streamIndex, PID);

        (*streamInfoArray)[arrayIndex] = (ebp_stream_info_t *)calloc (1, sizeof (ebp_stream_info_t));
         ebp_stream_info_t *streamInfo = (*streamInfoArray)[arrayIndex];

         streamInfo->fifo = (thread_safe_fifo_t *)calloc (1, sizeof (thread_safe_fifo_t));

         LOG_DEBUG_ARGS ("Main:setupQueues: calling fifo_create for file %d, stream %d", 
               fileIndex, streamIndex);
         returnCode = fifo_create (streamInfo->fifo, fileIndex);
         if (returnCode != 0)
         {
            LOG_ERROR_ARGS ("Main:setupQueues: error creating fifo for file %d, stream %d", 
               fileIndex, streamIndex);
            return -1;
         }

         streamInfo->PID = PID;
         streamInfo->isVideo = IS_VIDEO_STREAM(streamType);

         streamInfo->lastVideoChunkPTS = 0;
         streamInfo->lastVideoChunkPTSValid = 0;
         streamInfo->streamPassFail = 1;

         streamInfo->ebpBoundaryInfo = setupDefaultBoundaryInfoArray();
         returnCode = modBoundaryInfoArray (programStreamInfo[fileIndex].ebpDescriptors[streamIndex], 
            streamInfo->ebpBoundaryInfo);
         if (returnCode != 0)
         {
            LOG_ERROR_ARGS ("Main:setupQueues: error modifying boundary info for file %d, stream %d", 
               fileIndex, streamIndex);
            return -1;
         }
      }
   }

   varray_free(audioStreamPIDArray);
   LOG_INFO ("Main:setupQueues: exiting");
   return 0;
}

int startThreads(int numFiles, int totalNumStreams, ebp_stream_info_t **streamInfoArray, char **fileNames,
   int *filePassFails, pthread_t ***fileIngestThreads, pthread_t ***analysisThreads, pthread_attr_t *threadAttr)
{
   LOG_INFO ("Main:startThreads: entering");

   int returnCode = 0;

   // one worker thread per file, one analysis thread per streamtype
   // num worker thread = numFiles
   // num analysis threads = totalNumStreams

   pthread_attr_init(threadAttr);
   pthread_attr_setdetachstate(threadAttr, PTHREAD_CREATE_JOINABLE);


   // start analyzer threads
   *analysisThreads = (pthread_t **) calloc (totalNumStreams, sizeof(pthread_t*));
   for (int threadIndex = 0; threadIndex < totalNumStreams; threadIndex++)
   {
      // this is freed by the analysis thread when it exits
      ebp_stream_info_t **streamInfos = (ebp_stream_info_t **)calloc(numFiles, sizeof (ebp_stream_info_t*));
      for (int i=0; i<numFiles; i++)
      {
         int arrayIndex = get2DArrayIndex (i, threadIndex, totalNumStreams);

         streamInfos[i] = streamInfoArray[arrayIndex];
      }
      
      ebp_segment_analysis_thread_params_t *ebpSegmentAnalysisThreadParams = (ebp_segment_analysis_thread_params_t *)malloc (sizeof(ebp_segment_analysis_thread_params_t));
      ebpSegmentAnalysisThreadParams->threadID = 100 + threadIndex;
      ebpSegmentAnalysisThreadParams->numStreamInfos = numFiles;
      ebpSegmentAnalysisThreadParams->streamInfos = streamInfos;

      (*analysisThreads)[threadIndex] = (pthread_t *)malloc (sizeof (pthread_t));
      pthread_t *analyzerThread = (*analysisThreads)[threadIndex];
      LOG_INFO_ARGS("Main:startThreads: creating analyzer thread %d", threadIndex);
      returnCode = pthread_create(analyzerThread, threadAttr, EBPSegmentAnalysisThreadProc, (void *)ebpSegmentAnalysisThreadParams);
      if (returnCode)
      {
         LOG_ERROR_ARGS("Main:startThreads: FAIL: error %d creating analyzer thread %d", 
            returnCode, threadIndex);
          return -1;
      }
   }


   // start the file ingest threads
   *fileIngestThreads = (pthread_t **) calloc (numFiles, sizeof(pthread_t*));
   for (int threadIndex = 0; threadIndex < numFiles; threadIndex++)
   {
      int arrayIndex = get2DArrayIndex (threadIndex, 0, totalNumStreams);
      ebp_stream_info_t **streamInfos = &(streamInfoArray[arrayIndex]);
             
      ebp_file_ingest_thread_params_t *ebpFileIngestThreadParams = (ebp_file_ingest_thread_params_t *)malloc (sizeof(ebp_file_ingest_thread_params_t));
      ebpFileIngestThreadParams->threadNum = threadIndex;
      ebpFileIngestThreadParams->numStreamInfos = totalNumStreams;
      ebpFileIngestThreadParams->streamInfos = streamInfos;
      ebpFileIngestThreadParams->filePath = fileNames[threadIndex];
      ebpFileIngestThreadParams->filePassFail = &(filePassFails[threadIndex]);

      (*fileIngestThreads)[threadIndex] = (pthread_t *)malloc (sizeof (pthread_t));
      pthread_t *fileIngestThread = (*fileIngestThreads)[threadIndex];
      LOG_INFO_ARGS("Main:startThreads: creating fileIngest thread %d", threadIndex);
      returnCode = pthread_create(fileIngestThread, threadAttr, EBPFileIngestThreadProc, (void *)ebpFileIngestThreadParams);
      if (returnCode)
      {
         LOG_ERROR_ARGS("Main:startThreads: FAIL: error %d creating fileIngest thread %d", 
            returnCode, threadIndex);
          return -1;
      }
  }

   LOG_INFO("Main:startThreads: exiting");
   return 0;
}
  
int waitForThreadsToExit(int numFiles, int totalNumStreams,
   pthread_t **fileIngestThreads, pthread_t **analysisThreads, pthread_attr_t *threadAttr)
{
   LOG_INFO("Main:waitForThreadsToExit: entering");
   void *status;
   int returnCode = 0;

   for (int threadIndex = 0; threadIndex < totalNumStreams; threadIndex++)
   {
      returnCode = pthread_join(*(analysisThreads[threadIndex]), &status);
      if (returnCode) 
      {
         LOG_ERROR_ARGS ("Main:waitForThreadsToExit: error %d from pthread_join() for analysis thread %d",
            returnCode, threadIndex);
         returnCode = -1;
      }

      LOG_INFO_ARGS("Main:waitForThreadsToExit: completed join with analysis thread %d: status = %ld", 
         threadIndex, (long)status);
   }

   for (int threadIndex = 0; threadIndex < numFiles; threadIndex++)
   {
      returnCode = pthread_join(*(fileIngestThreads[threadIndex]), &status);
      if (returnCode) 
      {
         LOG_ERROR_ARGS ("Main:waitForThreadsToExit: error %d from pthread_join() for fileIngest thread %d",
            returnCode, threadIndex);
         returnCode = -1;
      }

      LOG_INFO_ARGS("Main:waitForThreadsToExit: completed join with fileIngest thread %d: status = %ld", 
         threadIndex, (long)status);
   }

   pthread_attr_destroy(threadAttr);
        
   LOG_INFO ("Main:waitForThreadsToExit: exiting");
   return returnCode;
}

void analyzeResults(int numFiles, int numStreams, ebp_stream_info_t **streamInfoArray, char **fileNames,
   int *filePassFails)
{
   LOG_INFO ("");
   LOG_INFO ("");
   LOG_INFO ("TEST RESULTS");
   LOG_INFO ("");

   for (int i=0; i<numFiles; i++)
   {
      LOG_INFO_ARGS ("File %s", fileNames[i]);
      LOG_INFO_ARGS ("   File PassFail Result: %s", (filePassFails[i]?"PASS":"FAIL"));
      LOG_INFO ("   Stream PassFail Results:");
      for (int j=0; j<numStreams; j++)
      {
         int arrayIndex = get2DArrayIndex (i, j, numStreams);
         ebp_stream_info_t *streamInfo = streamInfoArray[arrayIndex];

         if (streamInfo == NULL)
         {
            // if stream is absent from this file
            continue;
         }

         LOG_INFO_ARGS ("      PID %d (%s): %s", streamInfo->PID, (streamInfo->isVideo?"VIDEO":"AUDIO"),
            (streamInfo->streamPassFail?"PASS":"FAIL"));

         printBoundaryInfoArray(streamInfo->ebpBoundaryInfo);
      }
      LOG_INFO ("");

   }

   LOG_INFO ("TEST RESULTS END");
   LOG_INFO ("");
   LOG_INFO ("");
}

// GORP: add peek option

int main(int argc, char** argv) 
{
   if (argc < 2)
   {
      usage();
      return 1;
   }
    
   int c;
   int long_options_index; 

   int peekFlag = 0;

   while ((c = getopt_long(argc, argv, "ph", long_options, &long_options_index)) != -1) 
   {
       switch (c) 
       {
         case 'p':
            peekFlag = 1; 
            break;         
         case 'h':
         default:
            usage(); 
            return 1;
       }
   }


   LOG_INFO_ARGS ("Main: entering: optind = %d", optind);
   int numFiles = argc-optind;
   for (int i=optind; i<argc; i++)
   {
      LOG_INFO_ARGS ("Main: FilePath %d = %s", i, argv[i]); 
   }

   program_stream_info_t *programStreamInfo = (program_stream_info_t *)calloc (numFiles, 
      sizeof (program_stream_info_t));
   int *filePassFails = calloc(numFiles, sizeof(int));
   for (int i=0; i<numFiles; i++)
   {
      filePassFails[i] = 1;
   }

   int returnCode = prereadFiles(numFiles, &argv[optind], programStreamInfo);
   if (returnCode != 0)
   {
      LOG_ERROR ("Main: FATAL ERROR during prereadFiles: exiting"); 
      exit (-1);
   }

   if (peekFlag)
   {
      return 0;
   }

   // array of fifo pointers
   ebp_stream_info_t **streamInfoArray = NULL;
   int numStreamsPerFile;
   returnCode = setupQueues(numFiles, &argv[optind], programStreamInfo, &streamInfoArray, &numStreamsPerFile);
   if (returnCode != 0)
   {
      LOG_ERROR ("Main: FATAL ERROR during setupQueues: exiting"); 
      exit (-1);
   }

   pthread_t **fileIngestThreads;
   pthread_t **analysisThreads;
   pthread_attr_t threadAttr;

   returnCode = startThreads(numFiles, numStreamsPerFile, streamInfoArray, &argv[optind], filePassFails,
      &fileIngestThreads, &analysisThreads, &threadAttr);
   if (returnCode != 0)
   {
      LOG_ERROR ("Main: FATAL ERROR during startThreads: exiting"); 
      exit (-1);
   }


   returnCode = waitForThreadsToExit(numFiles, numStreamsPerFile, fileIngestThreads, analysisThreads, &threadAttr);
   if (returnCode != 0)
   {
      LOG_ERROR ("Main: FATAL ERROR during waitForThreadsToExit: exiting"); 
      exit (-1);
   }

   // analyze the pass fail results
   analyzeResults(numFiles, numStreamsPerFile, streamInfoArray, &argv[optind], filePassFails);


   returnCode = teardownQueues(numFiles, numStreamsPerFile, streamInfoArray);
   if (returnCode != 0)
   {
      LOG_ERROR ("Main: FATAL ERROR during teardownQueues: exiting"); 
      exit (-1);
   }

   free (programStreamInfo);
   free (filePassFails);

   pthread_exit(NULL);

   LOG_INFO ("Main: exiting");
   return 0;
}


void testsParseNTPTimestamp()
{
   uint32_t numSeconds;
   float fractionalSecond;

   uint64_t ntpTime = 0x0000000180000000;

   parseNTPTimestamp(ntpTime, &numSeconds, &fractionalSecond);

   printf ("numSeconds = %u, fractionalSecond = %f\n", numSeconds, fractionalSecond);
}

