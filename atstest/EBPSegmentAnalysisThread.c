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
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include <mpeg2ts_demux.h>
#include <libts_common.h>
#include <tpes.h>
#include <ebp.h>


#include "EBPSegmentAnalysisThread.h"
#include "EBPThreadLogging.h"

void *EBPSegmentAnalysisThreadProc(void *threadParams)
{
   int returnCode = 0;
   int queueSize = 0;
   int queueExit = 0;

   ebp_segment_analysis_thread_params_t *ebpSegmentAnalysisThreadParams = (ebp_segment_analysis_thread_params_t *)threadParams;
   printf("EBPSegmentAnalysisThread (%d) starting...\n", ebpSegmentAnalysisThreadParams->threadID);

   int *fifoNotActive = (int *) calloc (ebpSegmentAnalysisThreadParams->numStreamInfos, sizeof (int));

   returnCode = syncIncomingStreams (ebpSegmentAnalysisThreadParams->threadID, ebpSegmentAnalysisThreadParams->numStreamInfos, 
      ebpSegmentAnalysisThreadParams->streamInfos, fifoNotActive);
   if (returnCode != 0)
   {
      // GORP: handle return code
   }

   int exitThread = 0;
   while (!exitThread)
   {
//      printf ("EBPSegmentAnalysisThread (%d): in while: numFifos = %d\n", ebpSegmentAnalysisThreadParams->threadID,
//         ebpSegmentAnalysisThreadParams->numFifos);
      exitThread = 1;

      int64_t nextPTS = 0;

      for (int i=0; i<ebpSegmentAnalysisThreadParams->numStreamInfos; i++)
      {
         thread_safe_fifo_t *fifo = ebpSegmentAnalysisThreadParams->streamInfos[i]->fifo;
         ebp_stream_info_t *streamInfo = ebpSegmentAnalysisThreadParams->streamInfos[i];
         if (fifoNotActive[i] || fifo == NULL)
         {
            printf ("EBPSegmentAnalysisThread (%d): fifo %d not active --- skipping\n", ebpSegmentAnalysisThreadParams->threadID, i);
            continue;
         }

         void *element;
         printf ("EBPSegmentAnalysisThread (%d) calling fifo_pop for fifo %d\n", ebpSegmentAnalysisThreadParams->threadID, i);
         returnCode = fifo_pop (fifo, &element);
         if (returnCode != 0)
         {
            printf ("EBPSegmentAnalysisThread (%d) error %d calling fifo_pop for fifo %d\n", ebpSegmentAnalysisThreadParams->threadID,
               returnCode, i);
            // GORP: do something here
         }
         else
         {
//            printf ("EBPSegmentAnalysisThread (%d) pop complete: element = %x\n", ebpSegmentAnalysisThreadParams->threadID,
//               (unsigned int)element);

            if (element == NULL)
            {
               printf ("EBPSegmentAnalysisThread (%d) pop complete: element = NULL: marking fifo %d as inactive\n", 
                  ebpSegmentAnalysisThreadParams->threadID, i);
               // worker thread is done -- keep track of these
               fifoNotActive[i] = 1;
            }
            else
            {
               ebp_segment_info_t *ebpSegmentInfo = (ebp_segment_info_t *)element;
               printf ("EBPSegmentAnalysisThread (%d): POPPED PTS = %"PRId64" from fifo %d (PID %d), descriptor = %x\n", 
                  ebpSegmentAnalysisThreadParams->threadID, ebpSegmentInfo->PTS, i, streamInfo->PID, (unsigned int)(ebpSegmentInfo->latestEBPDescriptor));

               if (nextPTS == 0)
               {
                  nextPTS = ebpSegmentInfo->PTS;
               }
               else
               {
                  if (ebpSegmentInfo->PTS != nextPTS)
                  {
                     printf ("EBPSegmentAnalysisThread (%d): FAIL: PTS MISMATCH for fifo %d (PID %d). Expected %"PRId64", Actual %"PRId64"\n",
                        ebpSegmentAnalysisThreadParams->threadID, i, streamInfo->PID, nextPTS, ebpSegmentInfo->PTS);
                  }
               }
               
               cleanupEBPSegmentInfo (ebpSegmentInfo);
               exitThread = 0;
            }
         }
      }
   }

   printf("EBPSegmentAnalysisThread (%d) exiting...\n", ebpSegmentAnalysisThreadParams->threadID);
   free (ebpSegmentAnalysisThreadParams->streamInfos);
   free (ebpSegmentAnalysisThreadParams);
   pthread_exit(NULL);
}

int syncIncomingStreams (int threadID, int numStreamInfos, ebp_stream_info_t **streamInfos, int *fifoNotActive)
{
   int64_t startPTS = 0;
   int returnCode = 0;
   void *element;
         
   printf ("EBPSegmentAnalysisThread:syncIncomingStreams (%d): entering\n", threadID);

   // cycle through all fifos and peek at starting PTS -- take the latest of these as the starting 
   // PTS for the stream analysis
   for (int i=0; i<numStreamInfos; i++)
   {
      thread_safe_fifo_t *fifo =streamInfos[i]->fifo;
      if (fifoNotActive[i] || fifo == NULL)
      {
         printf ("EBPSegmentAnalysisThread:syncIncomingStreams (%d): fifo %d not active --- skipping\n", threadID, i);
         continue;
      }

      printf ("EBPSegmentAnalysisThread:syncIncomingStreams (%d) calling fifo_peek for fifo %d\n", threadID, i);
      returnCode = fifo_peek (fifo, &element);
      if (returnCode != 0)
      {
         printf ("EBPSegmentAnalysisThread:syncIncomingStreams (%d) error %d calling fifo_peek for fifo %d\n", threadID,
            returnCode, i);
         // fatal error here -- exit thread?
         continue;
      }

      if (element == NULL)
      {
         printf ("EBPSegmentAnalysisThread:syncIncomingStreams (%d) peek complete: element = NULL: marking fifo %d inactive\n", 
            threadID, i);
         // worker thread is done -- keep track of these
         fifoNotActive[i] = 1;
      }
      else
      {            
         ebp_segment_info_t *ebpSegmentInfo = (ebp_segment_info_t *)element;
         printf ("EBPSegmentAnalysisThread:syncIncomingStreams (%d): PEEKED PTS = %"PRId64" from fifo %d (PID %d), descriptor = %x\n", 
            threadID, ebpSegmentInfo->PTS, i, streamInfos[i]->PID, (unsigned int)(ebpSegmentInfo->latestEBPDescriptor));               
         if (ebpSegmentInfo->PTS > startPTS)
         {
            startPTS = ebpSegmentInfo->PTS;
         }
      }
   }
         
   printf ("EBPSegmentAnalysisThread:syncIncomingStreams (%d): starting PTS = %"PRId64"\n", threadID, startPTS);               

   // next cycle through all queues and peek at PTS, popping off ones that are prior to the analysis start PTS
   for (int i=0; i<numStreamInfos; i++)
   {
      thread_safe_fifo_t *fifo = streamInfos[i]->fifo;
      if (fifoNotActive[i] || fifo == NULL)
      {
         printf ("EBPSegmentAnalysisThread:syncIncomingStreams (%d) pruning: fifo %d not active --- skipping\n", threadID, i);
         continue;
      }

      while (1)
      {
         printf ("EBPSegmentAnalysisThread:syncIncomingStreams (%d) pruning: calling fifo_peek for fifo %d\n", threadID, i);
         returnCode = fifo_peek (fifo, &element);
         if (returnCode != 0)
         {
            printf ("EBPSegmentAnalysisThread:syncIncomingStreams (%d) pruning: error %d calling fifo_peek for fifo %d\n", threadID,
               returnCode, i);
            // fatal error here -- exit thread?
            break;
         }

         if (element == NULL)
         {
            printf ("EBPSegmentAnalysisThread:syncIncomingStreams (%d) pruning: pop complete: element = NULL -- marking fifo %d inactive\n", threadID, i);
            // worker thread is done -- keep track of these
            fifoNotActive[i] = 1;
            break;
         }
         else
         {            
            ebp_segment_info_t *ebpSegmentInfo = (ebp_segment_info_t *)element;
            printf ("EBPSegmentAnalysisThread:syncIncomingStreams (%d) pruning: PEEKED PTS = %"PRId64" from fifo %d (PID %d)\n", 
               threadID, ebpSegmentInfo->PTS, i, streamInfos[i]->PID);               
            if (ebpSegmentInfo->PTS < startPTS)
            {
               printf ("EBPSegmentAnalysisThread:syncIncomingStreams (%d) pruning: calling fifo_pop for fifo %d\n", threadID, i);
               returnCode = fifo_pop (fifo, &element);
               if (returnCode != 0)
               {
                  printf ("EBPSegmentAnalysisThread:syncIncomingStreams (%d) pruning: error %d calling fifo_pop for fifo %d\n", threadID,
                     returnCode, i);
                  // fatal error here -- exit thread?
                  break;
               }
            }
            else
            {
               // done
               break;
            }
         }
      }
   }
   
   printf ("EBPSegmentAnalysisThread:syncIncomingStreams (%d): complete\n", threadID);

   return 0;
}


void cleanupEBPSegmentInfo (ebp_segment_info_t *ebpSegmentInfo)
{
   if (ebpSegmentInfo->EBP != NULL)
   {
      ebp_free(ebpSegmentInfo->EBP);
   }

   if (ebpSegmentInfo->latestEBPDescriptor != NULL)
   {
      ebp_descriptor_free((descriptor_t *)(ebpSegmentInfo->latestEBPDescriptor));
   }
}
