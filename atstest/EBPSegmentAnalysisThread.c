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

   EBPSegmentAnalysisThreadParams *ebpSegmentAnalysisThreadParams = (EBPSegmentAnalysisThreadParams *)threadParams;
   printf("EBPSegmentAnalysisThread (%d) starting...\n", ebpSegmentAnalysisThreadParams->threadID);

   int *fifoNotActive = (int *) calloc (ebpSegmentAnalysisThreadParams->numFifos, sizeof (int));

   int exitThread = 0;
   while (!exitThread)
   {
//      printf ("EBPSegmentAnalysisThread (%d): in while: numFifos = %d\n", ebpSegmentAnalysisThreadParams->threadID,
//         ebpSegmentAnalysisThreadParams->numFifos);
      exitThread = 1;
      for (int i=0; i<ebpSegmentAnalysisThreadParams->numFifos; i++)
      {
         if (fifoNotActive[i])
         {
            printf ("EBPSegmentAnalysisThread (%d): fifo %d not active --- skipping\n", ebpSegmentAnalysisThreadParams->threadID, i);
            continue;
         }

         thread_safe_fifo_t *fifo = ebpSegmentAnalysisThreadParams->fifos[i];

         void *element;
         printf ("EBPSegmentAnalysisThread (%d) calling fifo_pop for thread %d\n", ebpSegmentAnalysisThreadParams->threadID, i);
         returnCode = fifo_pop (fifo, &element);
         if (returnCode != 0)
         {
            printf ("EBPSegmentAnalysisThread (%d) error %d calling fifo_pop for thread %d\n", ebpSegmentAnalysisThreadParams->threadID,
               returnCode, i);
            // GORP: do something here
         }
         else
         {
//            printf ("EBPSegmentAnalysisThread (%d) pop complete: element = %x\n", ebpSegmentAnalysisThreadParams->threadID,
//               (unsigned int)element);

            if (element == NULL)
            {
               printf ("EBPSegmentAnalysisThread (%d) pop complete: element = NULL\n", ebpSegmentAnalysisThreadParams->threadID);
               // worker thread is done -- keep track of these
               fifoNotActive[i] = 1;
            }
            else
            {
 //              printf ("EBPSegmentAnalysisThread (%d) popped element %d from fifo %d\n", ebpSegmentAnalysisThreadParams->threadID,
 //                 *((int *)element), i);
               
               // GORP: process element -- fill this in

               EBPSegmentInfo *ebpSegmentInfo = (EBPSegmentInfo *)element;
               printf ("EBPSegmentAnalysisThread (%d): POPPED PTS = %"PRId64" from fifo %d (PID %d), descriptor = %x\n", 
                  ebpSegmentAnalysisThreadParams->threadID, ebpSegmentInfo->PTS, i, fifo->PID, (unsigned int)(ebpSegmentInfo->latestEBPDescriptor));
               
               cleanupEBPSegmentInfo (ebpSegmentInfo);
               exitThread = 0;
            }
         }
      }
   }

   printf("EBPSegmentAnalysisThread (%d) exiting...\n", ebpSegmentAnalysisThreadParams->threadID);
   free (ebpSegmentAnalysisThreadParams);
   pthread_exit(NULL);
}

void cleanupEBPSegmentInfo (EBPSegmentInfo *ebpSegmentInfo)
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
