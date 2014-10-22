
#include <stdlib.h>
#include <errno.h>
#include <mpeg2ts_demux.h>
#include <libts_common.h>
#include <tpes.h>
#include <ebp.h>
#include <pthread.h>

#include "EBPCommon.h"
#include "EBPFileIngestThread.h"
#include "EBPSegmentAnalysisThread.h"

#define TS_SIZE 188

static int g_bPATFound = 0;
static int g_bPMTFound = 0;
static int g_numVideoChunksFound = 0;

typedef struct
{
   int numStreams;
   uint32_t *stream_types;
   uint32_t *PIDs;
} program_stream_info_t;

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

      for (int j = 0; j < vqarray_length(m2p->pids); j++)
      {
         if ((pi = vqarray_get(m2p->pids, j)) != NULL)
         {
            LOG_DEBUG_ARGS ("Main:prereadFiles: stream %d: stream_type = %u, elementary_PID = %u, ES_info_length = %u",
               j, pi->es_info->stream_type, pi->es_info->elementary_PID, pi->es_info->ES_info_length);

            programStreamInfo[i].stream_types[j] = pi->es_info->stream_type;
            programStreamInfo[i].PIDs[j] = pi->es_info->elementary_PID;
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
   // GORP: we need to count number of distinct audio streams in set of files, but for now, 
   // just assume that all files have the same audio streams
   
   int numStreams = programStreamInfo[0].numStreams;
   *numStreamsPerFile = numStreams;

   // *streamInfoArray is an array of stream_info pointers, not stream_infos.  this way, if a particular stream_info is not
   // needed for a file, that stream_info pointer can be left null
   *streamInfoArray = (ebp_stream_info_t **) calloc (numFiles * numStreams, sizeof (ebp_stream_info_t*));

   for (int fileIndex=0; fileIndex < numFiles; fileIndex++)
   {
      for (int streamIndex=0; streamIndex < numStreams; streamIndex++)
      {
         int arrayIndex = get2DArrayIndex (fileIndex, streamIndex, numStreams);
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

         streamInfo->PID = ((programStreamInfo[fileIndex]).PIDs)[streamIndex];
         uint32_t streamType = ((programStreamInfo[fileIndex]).stream_types)[streamIndex];
         streamInfo->isVideo = IS_VIDEO_STREAM(streamType);
         streamInfo->ebpImplicit = 0;  // explicit
         streamInfo->ebpImplicitPID = 0;
         streamInfo->lastVideoChunkPTS = 0;
         streamInfo->lastVideoChunkPTSValid = 0;
         streamInfo->streamPassFail = 1;
      }
   }

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

         // GORP: include video/audio info
         // GORP: include expicit/implicit info
         LOG_INFO_ARGS ("      PID %d: %s", streamInfo->PID,
            (streamInfo->streamPassFail?"PASS":"FAIL"));
      }
      LOG_INFO ("");
   }

   LOG_INFO ("TEST RESULTS END");
   LOG_INFO ("");
   LOG_INFO ("");
}

int main(int argc, char** argv) 
{
   LOG_INFO ("Main: entering");

   if (argc < 2)
   {
      return 1;
   }

   int numFiles = argc-1;
   for (int i=0; i<numFiles; i++)
   {
      LOG_INFO_ARGS ("Main: FilePath %d = %s", i, argv[i+1]); 
   }

   program_stream_info_t *programStreamInfo = (program_stream_info_t *)calloc (numFiles, 
      sizeof (program_stream_info_t));
   int *filePassFails = calloc(numFiles, sizeof(int));
   for (int i=0; i<numFiles; i++)
   {
      filePassFails[i] = 1;
   }

   int returnCode = prereadFiles(numFiles, &argv[1], programStreamInfo);
   if (returnCode != 0)
   {
      LOG_ERROR ("Main: FATAL ERROR during prereadFiles: exiting"); 
      exit (-1);
   }


   // array of fifo pointers
   ebp_stream_info_t **streamInfoArray = NULL;
   int numStreamsPerFile;
   returnCode = setupQueues(numFiles, &argv[1], programStreamInfo, &streamInfoArray, &numStreamsPerFile);
   if (returnCode != 0)
   {
      LOG_ERROR ("Main: FATAL ERROR during setupQueues: exiting"); 
      exit (-1);
   }

   pthread_t **fileIngestThreads;
   pthread_t **analysisThreads;
   pthread_attr_t threadAttr;

   returnCode = startThreads(numFiles, numStreamsPerFile, streamInfoArray, &argv[1], filePassFails,
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
   analyzeResults(numFiles, numStreamsPerFile, streamInfoArray, &argv[1], filePassFails);


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
