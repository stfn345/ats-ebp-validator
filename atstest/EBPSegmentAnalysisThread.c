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
   LOG_INFO_ARGS("EBPSegmentAnalysisThread %d: starting...", ebpSegmentAnalysisThreadParams->threadID);

   int *fifoNotActive = (int *) calloc (ebpSegmentAnalysisThreadParams->numFiles, sizeof (int));

   returnCode = syncIncomingStreams (ebpSegmentAnalysisThreadParams->threadID, ebpSegmentAnalysisThreadParams->numFiles, 
      ebpSegmentAnalysisThreadParams->streamInfos, fifoNotActive);
   if (returnCode != 0)
   {
      LOG_ERROR_ARGS("EBPSegmentAnalysisThread %d: Fatal error syncing streams: exiting", 
         ebpSegmentAnalysisThreadParams->threadID);
      exit (-1);
   }

   int exitThread = 0;
   while (!exitThread)
   {
      exitThread = 1;

      int nextPTSSet = 0;
      int64_t nextPTS = 0;

      uint32_t acquisitionTimeSecs = 0;
      float acquisitionTimeFracSec = 0.0;
      int acquisitionTimePresent = 0;
      int acquisitionTimeSet = 0;

      for (int i=0; i<ebpSegmentAnalysisThreadParams->numFiles; i++)
      {
         ebp_stream_info_t *streamInfo = ebpSegmentAnalysisThreadParams->streamInfos[i];
         if (fifoNotActive[i] || streamInfo == NULL)
         {
            LOG_DEBUG_ARGS ("EBPSegmentAnalysisThread %d: fifo %d not active --- skipping", ebpSegmentAnalysisThreadParams->threadID, i);
            continue;
         }
         thread_safe_fifo_t *fifo = streamInfo->fifo;

         void *element;
         LOG_DEBUG_ARGS ("EBPSegmentAnalysisThread %d: calling fifo_pop for fifo %d", ebpSegmentAnalysisThreadParams->threadID, i);
         returnCode = fifo_pop (fifo, &element);
         if (returnCode != 0)
         {
            LOG_ERROR_ARGS ("EBPSegmentAnalysisThread %d: FATAL error %d calling fifo_pop for fifo %d", ebpSegmentAnalysisThreadParams->threadID,
               returnCode, i);

            // fatal error here
            streamInfo->streamPassFail = 0;
            exit (-1);
         }
         else
         {
//            LOG_DEBUG_ARGS ("EBPSegmentAnalysisThread (%d) pop complete: element = %x", ebpSegmentAnalysisThreadParams->threadID,
//               (unsigned int)element);

            if (element == NULL)
            {
               LOG_DEBUG_ARGS ("EBPSegmentAnalysisThread %d: pop complete: element = NULL: marking fifo %d as inactive", 
                  ebpSegmentAnalysisThreadParams->threadID, i);
               // worker thread is done -- keep track of these
               fifoNotActive[i] = 1;
            }
            else
            {
               ebp_segment_info_t *ebpSegmentInfo = (ebp_segment_info_t *)element;
               LOG_INFO_ARGS ("EBPSegmentAnalysisThread %d: POPPED PTS = %"PRId64" from fifo %d (PID %d), descriptor = %x", 
                  ebpSegmentAnalysisThreadParams->threadID, ebpSegmentInfo->PTS, i, streamInfo->PID, 
                  (unsigned int)(ebpSegmentInfo->latestEBPDescriptor));
                  
               if (!nextPTSSet)
               {
                  nextPTS = ebpSegmentInfo->PTS;
                  nextPTSSet = 1;
               }
               else
               {
                  if (ebpSegmentInfo->PTS != nextPTS)
                  {
                     LOG_ERROR_ARGS ("EBPSegmentAnalysisThread %d: FAIL: PTS MISMATCH for fifo %d (PID %d). Expected %"PRId64", Actual %"PRId64"",
                        ebpSegmentAnalysisThreadParams->threadID, i, streamInfo->PID, nextPTS, ebpSegmentInfo->PTS);
                     streamInfo->streamPassFail = 0;
                  }
               }

               // next check that acquisition time matches
               if (!acquisitionTimeSet)
               {
                  if (ebpSegmentInfo->EBP == NULL || ebpSegmentInfo->EBP->ebp_time_flag == 0)
                  {
                     acquisitionTimePresent = 0;
                  }
                  else
                  {
                     acquisitionTimePresent = 1;
                     parseNTPTimestamp(ebpSegmentInfo->EBP->ebp_acquisition_time, &acquisitionTimeSecs, &acquisitionTimeFracSec);
                  }

                  acquisitionTimeSet = 1;
               }
               else
               {
                  int acquisitionTimePresentTemp = (ebpSegmentInfo->EBP != NULL && ebpSegmentInfo->EBP->ebp_time_flag != 0);
                  if (acquisitionTimePresentTemp != acquisitionTimePresent)
                  {
                     LOG_ERROR_ARGS ("EBPSegmentAnalysisThread %d: FAIL: presence of acquisition time mismatch for fifo %d (PID %d).",
                        ebpSegmentAnalysisThreadParams->threadID, i, streamInfo->PID);
                     streamInfo->streamPassFail = 0;
                  }
                  else if (acquisitionTimePresentTemp)
                  {
                     uint32_t acquisitionTimeSecsTemp = 0;
                     float acquisitionTimeFracSecTemp = 0.0;
                     parseNTPTimestamp(ebpSegmentInfo->EBP->ebp_acquisition_time, 
                        &acquisitionTimeSecsTemp, &acquisitionTimeFracSecTemp);
                     if (acquisitionTimeSecsTemp != acquisitionTimeSecs ||
                        acquisitionTimeFracSecTemp != acquisitionTimeFracSec)
                     {
                        LOG_ERROR_ARGS ("EBPSegmentAnalysisThread %d: FAIL: Acquisition time MISMATCH for fifo %d (PID %d). Expected %d,%f, Actual %d,%f", 
                                        ebpSegmentAnalysisThreadParams->threadID, i, streamInfo->PID, 
                                        acquisitionTimeSecs, acquisitionTimeFracSec,
                                        acquisitionTimeSecsTemp, acquisitionTimeFracSecTemp);
                        streamInfo->streamPassFail = 0;
                     }
                     else
                     {
                        LOG_INFO_ARGS ("EBPSegmentAnalysisThread %d: Acquisition time for fifo %d (PID %d): %d,%f", 
                                        ebpSegmentAnalysisThreadParams->threadID, i, streamInfo->PID, 
                                        acquisitionTimeSecs, acquisitionTimeFracSec);
                     }
                  }
               }

               // Next check that the SAPType is valid
               if (ebpSegmentInfo->SAPType == SAP_STREAM_TYPE_NOT_SUPPORTED)
               {
                  LOG_INFO_ARGS ("EBPSegmentAnalysisThread %d: SAP_STREAM_TYPE_NOT_SUPPORTED for fifo %d (PID %d).", 
                     ebpSegmentAnalysisThreadParams->threadID, i, streamInfo->PID);
               }
               if (ebpSegmentInfo->SAPType == SAP_STREAM_TYPE_ERROR)
               {
                  LOG_ERROR_ARGS ("EBPSegmentAnalysisThread %d: SAP_STREAM_TYPE_ERROR for fifo %d (PID %d).", 
                     ebpSegmentAnalysisThreadParams->threadID, i, streamInfo->PID);
                  streamInfo->streamPassFail = 0;
               }

               if (ebpSegmentInfo->EBP == NULL)
               {
                  LOG_INFO_ARGS ("EBPSegmentAnalysisThread %d: ebpSegmentInfo->EBP == NULL for fifo %d (PID %d).", 
                     ebpSegmentAnalysisThreadParams->threadID, i, streamInfo->PID);
               }
               

               if (ebpSegmentInfo->SAPType != SAP_STREAM_TYPE_NOT_SUPPORTED && 
                   ebpSegmentInfo->SAPType != SAP_STREAM_TYPE_ERROR && 
                   ebpSegmentInfo->EBP != NULL)
               {
                  if (ebpSegmentInfo->EBP->ebp_sap_flag)
                  {
                     if (ebpSegmentInfo->EBP->ebp_sap_type != ebpSegmentInfo->SAPType)
                     {
                        LOG_ERROR_ARGS ("EBPSegmentAnalysisThread %d: FAIL: SAP Type MISMATCH for fifo %d (PID %d). Expected %d, Actual %d",
                                        ebpSegmentAnalysisThreadParams->threadID, i, streamInfo->PID, 
                                        ebpSegmentInfo->EBP->ebp_sap_type, ebpSegmentInfo->SAPType);
                        streamInfo->streamPassFail = 0;
                     }
                     else
                     {
                        LOG_INFO_ARGS ("EBPSegmentAnalysisThread %d: SAP Type MATCH for fifo %d (PID %d). Expected %d, Actual %d", 
                                        ebpSegmentAnalysisThreadParams->threadID, i, streamInfo->PID, 
                                        ebpSegmentInfo->EBP->ebp_sap_type, ebpSegmentInfo->SAPType);
                     }
   
                     // check descriptor SAP_max
                     if (ebpSegmentInfo->latestEBPDescriptor != NULL)
                     {
                        ebp_partition_data_t* partition = 
                           get_partition (ebpSegmentInfo->latestEBPDescriptor, ebpSegmentInfo->partitionId);
                        if (partition != NULL)
                        {
                           if (ebpSegmentInfo->EBP->ebp_sap_type > partition->sap_type_max)
                           {
                              LOG_ERROR_ARGS ("EBPSegmentAnalysisThread %d: FAIL: SAP Type too large for partition %d in fifo %d (PID %d). EBP Descriptor SAP Max %d, Actual %d", 
                                              ebpSegmentAnalysisThreadParams->threadID, ebpSegmentInfo->partitionId, i, streamInfo->PID, 
                                              partition->sap_type_max, ebpSegmentInfo->EBP->ebp_sap_type);
                              streamInfo->streamPassFail = 0;
                           }
                           else
                           {
                              LOG_INFO_ARGS ("EBPSegmentAnalysisThread %d: SAP Type OK for partition %d in fifo %d (PID %d). EBP Descriptor SAP Max %d, Actual %d", 
                                              ebpSegmentAnalysisThreadParams->threadID, 
                                              ebpSegmentInfo->partitionId, i, streamInfo->PID, 
                                              partition->sap_type_max, ebpSegmentInfo->SAPType);
                           }
                        }
                     }
                  }
                  else
                  {
                     // GORP: it seems like this should apply even when EBP struct not present
                     if (ebpSegmentInfo->SAPType != 1 && ebpSegmentInfo->SAPType !=2)
                     {
                        LOG_ERROR_ARGS ("EBPSegmentAnalysisThread %d: FAIL: Expected SAP Type 1 or 2 for fifo %d (PID %d). Actual SAP Type: %d", 
                           ebpSegmentAnalysisThreadParams->threadID, i, streamInfo->PID, ebpSegmentInfo->SAPType);
                        streamInfo->streamPassFail = 0;
                     }
                     else
                     {
                        LOG_INFO_ARGS ("EBPSegmentAnalysisThread %d: No ebp_sap_flag set: Expected SAP Type 1 or 2 for fifo %d (PID %d). Actual SAP Type: %d", 
                                        ebpSegmentAnalysisThreadParams->threadID, i, streamInfo->PID, 
                                        ebpSegmentInfo->SAPType);
                     }
                  }
               }
               
               cleanupEBPSegmentInfo (ebpSegmentInfo);
               exitThread = 0;
            }
         }
      }
   }

   LOG_INFO_ARGS ("EBPSegmentAnalysisThread %d: exiting...", ebpSegmentAnalysisThreadParams->threadID);
   free (ebpSegmentAnalysisThreadParams->streamInfos);
   free (ebpSegmentAnalysisThreadParams);
   pthread_exit(NULL);
}

int syncIncomingStreams (int threadID, int numFiles, ebp_stream_info_t **streamInfos, int *fifoNotActive)
{
   int64_t startPTS = 0;
   int returnCode = 0;
   void *element;
         
   LOG_INFO_ARGS ("EBPSegmentAnalysisThread:syncIncomingStreams %d: entering ", threadID);

   // cycle through all fifos and peek at starting PTS -- take the latest of these as the starting 
   // PTS for the stream analysis
   for (int i=0; i<numFiles; i++)
   {
      ebp_stream_info_t *streamInfo = streamInfos[i];
      if (fifoNotActive[i] || streamInfo == NULL)
      {
         LOG_INFO_ARGS ("EBPSegmentAnalysisThread:syncIncomingStreams %d: fifo %d not active --- skipping", threadID, i);
         continue;
      }
      thread_safe_fifo_t *fifo = streamInfos[i]->fifo;

      LOG_DEBUG_ARGS ("EBPSegmentAnalysisThread:syncIncomingStreams %d: calling fifo_peek for fifo %d", threadID, i);
      returnCode = fifo_peek (fifo, &element);
      if (returnCode != 0)
      {
         LOG_ERROR_ARGS ("EBPSegmentAnalysisThread:syncIncomingStreams %d: FATAL error %d calling fifo_peek for fifo %d", threadID,
            returnCode, i);
         streamInfo->streamPassFail = 0;
         // fatal error here -- exit
         exit (-1);
      }

      if (element == NULL)
      {
         LOG_INFO_ARGS ("EBPSegmentAnalysisThread:syncIncomingStreams %d: peek complete: element = NULL: marking fifo %d inactive", 
            threadID, i);
         // worker thread is done -- keep track of these
         fifoNotActive[i] = 1;
      }
      else
      {            
         ebp_segment_info_t *ebpSegmentInfo = (ebp_segment_info_t *)element;
         LOG_DEBUG_ARGS ("EBPSegmentAnalysisThread:syncIncomingStreams %d: PEEKED PTS = %"PRId64" from fifo %d (PID %d), descriptor = %x", 
            threadID, ebpSegmentInfo->PTS, i, streamInfos[i]->PID, (unsigned int)(ebpSegmentInfo->latestEBPDescriptor));               
         if (ebpSegmentInfo->PTS > startPTS)
         {
            startPTS = ebpSegmentInfo->PTS;
         }
      }
   }
         
   LOG_INFO_ARGS ("EBPSegmentAnalysisThread:syncIncomingStreams %d: starting PTS = %"PRId64"", threadID, startPTS);               

   // next cycle through all queues and peek at PTS, popping off ones that are prior to the analysis start PTS
   for (int i=0; i<numFiles; i++)
   {
      ebp_stream_info_t *streamInfo = streamInfos[i];
      if (fifoNotActive[i] || streamInfo == NULL)
      {
         LOG_INFO_ARGS ("EBPSegmentAnalysisThread:syncIncomingStreams %d: pruning: fifo %d not active --- skipping", threadID, i);
         continue;
      }
      thread_safe_fifo_t *fifo = streamInfos[i]->fifo;

      while (1)
      {
         LOG_DEBUG_ARGS ("EBPSegmentAnalysisThread:syncIncomingStreams %d: pruning: calling fifo_peek for fifo %d", threadID, i);
         returnCode = fifo_peek (fifo, &element);
         if (returnCode != 0)
         {
            LOG_ERROR_ARGS ("EBPSegmentAnalysisThread:syncIncomingStreams %d: pruning: FATAL error %d calling fifo_peek for fifo %d", threadID,
               returnCode, i);

            streamInfo->streamPassFail = 0;
            // fatal error here -- exit
            exit (-1);
         }

         if (element == NULL)
         {
            LOG_INFO_ARGS ("EBPSegmentAnalysisThread:syncIncomingStreams %d: pruning: peek complete: element = NULL -- marking fifo %d inactive", threadID, i);
            // worker thread is done -- keep track of these
            fifoNotActive[i] = 1;
            break;
         }
         else
         {            
            ebp_segment_info_t *ebpSegmentInfo = (ebp_segment_info_t *)element;
            LOG_INFO_ARGS ("EBPSegmentAnalysisThread:syncIncomingStreams %d: pruning: PEEKED PTS = %"PRId64" from fifo %d (PID %d)", 
               threadID, ebpSegmentInfo->PTS, i, streamInfos[i]->PID);               
            if (ebpSegmentInfo->PTS < startPTS)
            {
               LOG_INFO_ARGS ("EBPSegmentAnalysisThread:syncIncomingStreams %d: pruning: calling fifo_pop for fifo %d", threadID, i);
               returnCode = fifo_pop (fifo, &element);
               if (returnCode != 0)
               {
                  LOG_ERROR_ARGS ("EBPSegmentAnalysisThread:syncIncomingStreams %d: pruning: FATAL error %d calling fifo_pop for fifo %d", threadID,
                     returnCode, i);
                  streamInfo->streamPassFail = 0;
                  // fatal error here -- exit
                  exit (-1);
               }

               cleanupEBPSegmentInfo (ebpSegmentInfo);
            }
            else
            {
               // done
               break;
            }
         }
      }
   }
   
   LOG_INFO_ARGS ("EBPSegmentAnalysisThread:syncIncomingStreams (%d): complete", threadID);

   return 0;
}


void cleanupEBPSegmentInfo (ebp_segment_info_t *ebpSegmentInfo)
{
   if (ebpSegmentInfo == NULL)
   {
      return;
   }

   if (ebpSegmentInfo->EBP != NULL)
   {
      ebp_free(ebpSegmentInfo->EBP);
   }

   if (ebpSegmentInfo->latestEBPDescriptor != NULL)
   {
      ebp_descriptor_free((descriptor_t *)(ebpSegmentInfo->latestEBPDescriptor));
   }
}
