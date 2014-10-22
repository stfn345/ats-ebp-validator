
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
   printf ("pmt_processor\n");
   g_bPMTFound = 1;

   if (m2p == NULL || m2p->pmt == NULL) // if we don't have any PSI, there's nothing we can do
      return 0;

   return 1;
}

static int pat_processor(mpeg2ts_stream_t *m2s, void *arg)
{
   printf ("pat_processor\n");
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
   printf ("prereadFiles: entering\n");

   for (int i=0; i<numFiles; i++)
   {
      printf ("FilePath %d = %s\n", i, fileNames[i]); 

      // reset PAT/PMT read flags
      g_bPATFound = 0;
      g_bPMTFound = 0;

      FILE *infile = NULL;
      if ((infile = fopen(fileNames[i], "rb")) == NULL)
      {
         LOG_ERROR_ARGS("Cannot open file %s - %s", fileNames[i], strerror(errno));
         return 1;
      }

      mpeg2ts_stream_t *m2s = NULL;

      if (NULL == (m2s = mpeg2ts_stream_new()))
      {
         LOG_ERROR("Error creating MPEG-2 STREAM object");
         return 1;
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
               printf ("PAT/PMT found -- breaking\n");
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
            printf ("stream %d: stream_type = %u, elementary_PID = %u, ES_info_length = %u\n",
               j, pi->es_info->stream_type, pi->es_info->elementary_PID, pi->es_info->ES_info_length);

            programStreamInfo[i].stream_types[j] = pi->es_info->stream_type;
            programStreamInfo[i].PIDs[j] = pi->es_info->elementary_PID;
         }
      }

      mpeg2ts_stream_free(m2s);

      fclose(infile);
   }

   printf ("prereadFiles: exiting\n");
   return 0;
}

int get2DArrayIndex (int fileIndex, int streamIndex, int numStreams)
{
   return fileIndex * numStreams + streamIndex;
}

int teardownQueues(int numFiles, int numStreams, ebp_stream_info_t **streamInfoArray)
{
   printf ("teardownQueues: entering\n");
   int returnCode = 0;

   for (int fileIndex=0; fileIndex < numFiles; fileIndex++)
   {
      for (int streamIndex=0; streamIndex < numStreams; streamIndex++)
      {
         int arrayIndex = get2DArrayIndex (fileIndex, streamIndex, numStreams);
         ebp_stream_info_t *streamInfo = streamInfoArray[arrayIndex];

         printf ("teardownQueues: fifo (file_index = %d, streamIndex = %d, PID = %d) push_counter = %d, pop_counter = %d\n", 
            fileIndex, streamIndex, streamInfo->PID, streamInfo->fifo->push_counter, streamInfo->fifo->pop_counter);
         printf ("teardownQueues: arrayIndex = %d: calling fifo_destroy: fifo = %x\n", arrayIndex,
            (unsigned int)streamInfo->fifo);
         returnCode = fifo_destroy (streamInfo->fifo);
         if (returnCode != 0)
         {
            printf ("teardownQueues: error destroying queue\n");
         }

         free (streamInfo->fifo);
         free (streamInfo);
      }
   }

   free (streamInfoArray);

   printf ("teardownQueues: exiting\n");
   return 0;
}

int setupQueues(int numFiles, char **fileNames, program_stream_info_t *programStreamInfo,
                ebp_stream_info_t ***streamInfoArray, int *numStreamsPerFile)
{
   printf ("setupQueues: entering\n");

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

         printf ("setupQueues: arrayIndex = %d: calling fifo_create: fifo = %x\n", arrayIndex, (unsigned int)streamInfo->fifo);
         returnCode = fifo_create (streamInfo->fifo, fileIndex);
         if (returnCode != 0)
         {
            printf ("setupQueues: error creating queue\n");
            return -1;
         }

         streamInfo->PID = ((programStreamInfo[fileIndex]).PIDs)[streamIndex];
         uint32_t streamType = ((programStreamInfo[fileIndex]).stream_types)[streamIndex];
         streamInfo->isVideo = IS_VIDEO_STREAM(streamType);
         streamInfo->ebpImplicit = 0;  // explicit
         streamInfo->ebpImplicitPID = 0;
         streamInfo->lastVideoChunkPTS = 0;
         streamInfo->lastVideoChunkPTSValid = 0;
         streamInfo->testPassFail = 1;
      }
   }

   printf ("setupQueues: exiting\n");
   return 0;
}

int startThreads(int numFiles, int totalNumStreams, ebp_stream_info_t **streamInfoArray, char **fileNames,
   pthread_t ***fileIngestThreads, pthread_t ***analysisThreads, pthread_attr_t *threadAttr)
{
   printf ("startThreads: entering\n");

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
      printf("In main: creating analyzer thread\n");
      returnCode = pthread_create(analyzerThread, threadAttr, EBPSegmentAnalysisThreadProc, (void *)ebpSegmentAnalysisThreadParams);
      if (returnCode)
      {
          printf("ERROR %d creating analyzer thread\n", returnCode);
          exit(-1);
      }
   }


   // start the file ingest threads
   *fileIngestThreads = (pthread_t **) calloc (numFiles, sizeof(pthread_t*));
   for (int threadIndex = 0; threadIndex < numFiles; threadIndex++)
   {
      int arrayIndex = get2DArrayIndex (threadIndex, 0, totalNumStreams);
      printf ("arrayindex = %d\n", arrayIndex);
      ebp_stream_info_t **streamInfos = &(streamInfoArray[arrayIndex]);
       
      printf ("streamInfos = %x\n", (unsigned int)streamInfos);
      printf ("streamInfos[0]->PID = %d\n", streamInfos[0]->PID);
      
      ebp_file_ingest_thread_params_t *ebpFileIngestThreadParams = (ebp_file_ingest_thread_params_t *)malloc (sizeof(ebp_file_ingest_thread_params_t));
      ebpFileIngestThreadParams->threadNum = threadIndex;
      ebpFileIngestThreadParams->numStreamInfos = totalNumStreams;
      ebpFileIngestThreadParams->streamInfos = streamInfos;
      ebpFileIngestThreadParams->filePath = fileNames[threadIndex];

      (*fileIngestThreads)[threadIndex] = (pthread_t *)malloc (sizeof (pthread_t));
      pthread_t *fileIngestThread = (*fileIngestThreads)[threadIndex];
      printf("In main: creating fileIngest thread\n");
      returnCode = pthread_create(fileIngestThread, threadAttr, EBPFileIngestThreadProc, (void *)ebpFileIngestThreadParams);
      if (returnCode)
      {
          printf("ERROR %d creating fileIngest thread\n", returnCode);
          exit(-1);
      }
  }

   printf ("startThreads: exiting\n");
   return 0;
}
  
int waitForThreadsToExit(int numFiles, int totalNumStreams,
   pthread_t **fileIngestThreads, pthread_t **analysisThreads, pthread_attr_t *threadAttr)
{
   printf ("waitForThreadsToExit: entering\n");
   void *status;
   int returnCode;

   for (int threadIndex = 0; threadIndex < totalNumStreams; threadIndex++)
   {
      returnCode = pthread_join(*(analysisThreads[threadIndex]), &status);
      if (returnCode) 
      {
          printf("ERROR; pthread_join() returned: %d\n", returnCode);
 //         exit(-1);
      }

      printf("Completed join with analyzer thread %d: status = %ld\n", threadIndex, (long)status);
   }

   for (int threadIndex = 0; threadIndex < numFiles; threadIndex++)
   {
      returnCode = pthread_join(*(fileIngestThreads[threadIndex]), &status);
      if (returnCode) 
      {
          printf("ERROR; pthread_join() returned: %d\n", returnCode);
 //         exit(-1);
      }

      printf("Completed join with fileIngest thread %d: status = %ld\n", threadIndex, (long)status);
   }

   pthread_attr_destroy(threadAttr);
        
   printf ("waitForThreadsToExit: exiting\n");
   return 0;
}

int main(int argc, char** argv) 
{
   printf ("In Main\n");

   if (argc < 2)
   {
      return 1;
   }

   int numFiles = argc-1;
   for (int i=0; i<numFiles; i++)
   {
      printf ("FilePath %d = %s\n", i, argv[i+1]); 
   }

   program_stream_info_t *programStreamInfo = (program_stream_info_t *)calloc (numFiles, 
      sizeof (program_stream_info_t));

   prereadFiles(numFiles, &argv[1], programStreamInfo);


   // array of fifo pointers
   ebp_stream_info_t **streamInfoArray = NULL;
   int numStreamsPerFile;
   setupQueues(numFiles, &argv[1], programStreamInfo, &streamInfoArray, &numStreamsPerFile);

   pthread_t **fileIngestThreads;
   pthread_t **analysisThreads;
   pthread_attr_t threadAttr;

   startThreads(numFiles, numStreamsPerFile, streamInfoArray, &argv[1],
      &fileIngestThreads, &analysisThreads, &threadAttr);


   waitForThreadsToExit(numFiles, numStreamsPerFile, fileIngestThreads, analysisThreads, &threadAttr);

   printf ("Calling teardownQueues\n");
   teardownQueues(numFiles, numStreamsPerFile, streamInfoArray);

   pthread_exit(NULL);

   return 0;
}


int was_main2(int argc, char** argv) 
{
    printf ("In Main\n");
    printf ("argc = %d\n", argc);

   if (argc < 2)
   {
      return 1;
   }

   char *fname = argv[1];
   if (fname == NULL || fname[0] == 0)
   {
      LOG_ERROR("No input file provided");
      return 1;
   }

   FILE *infile = NULL;
   if ((infile = fopen(fname, "rb")) == NULL)
   {
      LOG_ERROR_ARGS("Cannot open file %s - %s", fname, strerror(errno));
      return 1;
   }

   mpeg2ts_stream_t *m2s = NULL;

   if (NULL == (m2s = mpeg2ts_stream_new()))
   {
      LOG_ERROR("Error creating MPEG-2 STREAM object");
      return 1;
   }

   // Register EBP descriptor parser
   descriptor_table_entry_t *desc = calloc(1, sizeof(descriptor_table_entry_t));
   desc->tag = EBP_DESCRIPTOR;
   desc->free_descriptor = ebp_descriptor_free;
   desc->print_descriptor = ebp_descriptor_print;
   desc->read_descriptor = ebp_descriptor_read;
   if (!register_descriptor(desc))
   {
      LOG_ERROR("Could not register EBP descriptor parser!");
      return 1;
   }

   m2s->pat_processor = (pat_processor_t)pat_processor;
   m2s->arg = NULL;
   m2s->arg_destructor = NULL;

   int num_packets = 4096;
   uint8_t *ts_buf = malloc(TS_SIZE * 4096);

   while ((num_packets = fread(ts_buf, TS_SIZE, 4096, infile)) > 0)
   {
      for (int i = 0; i < num_packets; i++)
      {
         ts_packet_t *ts = ts_new();
         ts_read(ts, ts_buf + i * TS_SIZE, TS_SIZE);
         mpeg2ts_stream_read_ts_packet(m2s, ts);
      }
   }

   mpeg2ts_stream_free(m2s);

   fclose(infile);

   return tslib_errno;
}
