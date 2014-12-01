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
#include "EBPFileIngestThread.h"
#include "EBPThreadLogging.h"


static char *STREAM_TYPE_UNKNOWN = "UNKNOWN";
static char *STREAM_TYPE_VIDEO = "VIDEO";
static char *STREAM_TYPE_AUDIO = "AUDIO";

static char* getStreamTypeDesc (elementary_stream_info_t *esi);


static int validate_ts_packet(ts_packet_t *ts, elementary_stream_info_t *es_info, void *arg)
{
   return 1;
}

static char* getStreamTypeDesc (elementary_stream_info_t *esi)
{
   if (IS_VIDEO_STREAM(esi->stream_type)) return STREAM_TYPE_VIDEO;
   if (IS_AUDIO_STREAM(esi->stream_type)) return STREAM_TYPE_AUDIO;

   return STREAM_TYPE_UNKNOWN;
}

static int validate_pes_packet(pes_packet_t *pes, elementary_stream_info_t *esi, vqarray_t *ts_queue, void *arg)
{
   // Get the first TS packet and check it for EBP
   ts_packet_t *ts = (ts_packet_t*)vqarray_get(ts_queue,0);
   if (ts == NULL)
   {
      // GORP: should this be an error?
      pes_free(pes);
      return 1; // Don't care about this packet
   }

   ebp_file_ingest_thread_params_t *ebpFileIngestThreadParams = (ebp_file_ingest_thread_params_t *)arg;

   int arrayIndex = get2DArrayIndex (ebpFileIngestThreadParams->threadNum, 0, ebpFileIngestThreadParams->numStreams);    
   ebp_stream_info_t **streamInfos = &((ebpFileIngestThreadParams->allStreamInfos)[arrayIndex]);

   thread_safe_fifo_t *fifo = NULL;
   int fifoIndex = -1;
   findFIFO (esi->elementary_PID, streamInfos, ebpFileIngestThreadParams->numStreams,
      &fifo, &fifoIndex);
   ebp_stream_info_t * streamInfo = streamInfos[fifoIndex];
   
   if (fifo == NULL)
   {
      // this should never happen -- maybe this should be fatal
      LOG_ERROR_ARGS("FileIngestThread %d: FAIL: Cannot find fifo for PID %d (%s)", 
         ebpFileIngestThreadParams->threadNum, esi->elementary_PID, getStreamTypeDesc (esi));

      ebpFileIngestThreadParams->filePassFail = 0;

      pes_free(pes);
      return 1;
   }

   ebp_t* ebp = getEBP(ts, streamInfo, ebpFileIngestThreadParams->threadNum);
   // GORP: testing -- removes ebp from audio to test implicit triggering
/*   if (esi->elementary_PID != 481)
   {
      ebp = NULL;
   }
*/
   if (ebp != NULL)
   {
      LOG_INFO_ARGS("FileIngestThread %d: Found EBP data in transport packet: PID %d (%s)", 
         ebpFileIngestThreadParams->threadNum, esi->elementary_PID, getStreamTypeDesc (esi));
//      ebp_print_stdout(ebp);
         
      if (ebp_validate_groups(ebp) != 0)
      {
         // if there is a ebp in the stream, then we need an ebp_descriptor to interpret it
         LOG_ERROR_ARGS("FileIngestThread %d: FAIL: EBP group validation failed: PID %d (%s)", 
            ebpFileIngestThreadParams->threadNum, esi->elementary_PID, getStreamTypeDesc (esi));

         streamInfo->streamPassFail = 0;
      }
   }

      
   // isBoundary is an array indexed by PartitionId -- we will analyze the segment once
   // for each partition that triggered a boundary
   int *isBoundary = (int *)calloc (EBP_NUM_PARTITIONS, sizeof(int));
 //  LOG_INFO_ARGS("EBPFileIngestThread %d: Calling detectBoundary. (PID %d)", 
 //           ebpFileIngestThreadParams->threadNum, esi->elementary_PID);
   detectBoundary(ebpFileIngestThreadParams->threadNum, ebp, streamInfo, pes->header.PTS, isBoundary);

   int lastVideoPTSSet = 0;

   for (uint8_t i=0; i<EBP_NUM_PARTITIONS; i++)
   {
      if (isBoundary[i])
      {
         uint32_t sapType = 0;  // GORP: fill in
         ebp_descriptor_t* ebpDescriptor = getEBPDescriptor (esi);  // could be null

         ebp_t* ebp_copy = getEBP(ts, streamInfo, ebpFileIngestThreadParams->threadNum);
         int returnCode = postToFIFO (pes->header.PTS, sapType, ebp_copy, ebpDescriptor, 
            esi->elementary_PID, ebpFileIngestThreadParams, i);
         if (returnCode != 0)
         {
            LOG_ERROR_ARGS("FileIngestThread %d: FAIL: Error posting to FIFO for partition %d: PID %d (%s)", 
               ebpFileIngestThreadParams->threadNum, i, esi->elementary_PID, getStreamTypeDesc (esi));

            streamInfo->streamPassFail = 0;
         }


         // do the following once only -- use flag lastVideoPTSSet to tell if already done
         if ((lastVideoPTSSet == 0) && IS_VIDEO_STREAM(esi->stream_type))
         {
            // if this is video, set lastVideoPTS in audio fifos
            for (int ii=0; ii<ebpFileIngestThreadParams->numStreams; ii++)
            {
               if (ii == fifoIndex)
               {
                  continue;
               }

               ebp_stream_info_t * streamInfoTemp = streamInfos[ii];
                     
               streamInfoTemp->lastVideoChunkPTS = pes->header.PTS;
               streamInfoTemp->lastVideoChunkPTSValid = 1;
            }
            lastVideoPTSSet = 1;
         }
         else if (IS_AUDIO_STREAM(esi->stream_type))
         {
           // this is an audio stream, so check that audio does not lag video by more than 3 seconds
            if (pes->header.PTS > (streamInfo->lastVideoChunkPTS + 3 * 90000))  // PTS is 90kHz ticks
            {
               LOG_ERROR_ARGS("FileIngestThread %d: FAIL: Audio PTS (%"PRId64") lags video PTS (%"PRId64") by more than 3 seconds: PID %d (%s)", 
                  ebpFileIngestThreadParams->threadNum, pes->header.PTS, streamInfo->lastVideoChunkPTS, 
                  esi->elementary_PID, getStreamTypeDesc (esi));

               streamInfo->streamPassFail = 0;
            }
            streamInfo->lastVideoChunkPTSValid = 0;
         }

         triggerImplicitBoundaries (ebpFileIngestThreadParams->threadNum, 
            ebpFileIngestThreadParams->allStreamInfos, 
            ebpFileIngestThreadParams->numStreams, ebpFileIngestThreadParams->numFiles, 
            fifoIndex, pes->header.PTS, (uint8_t) i /* partition ID */,
            ebpFileIngestThreadParams->threadNum);

      }
   }
   
   if (ebp != NULL)
   {
      ebp_free(ebp);
   }

   free (isBoundary);

   pes_free(pes);
   return 1;
}

ebp_t* getEBP(ts_packet_t *ts, ebp_stream_info_t * streamInfo, int threadNum)
{
   ebp_t* ebp = NULL;

   vqarray_t *scte128_data;
   if (!TS_HAS_ADAPTATION_FIELD(*ts) ||
         (scte128_data = ts->adaptation_field.scte128_private_data) == NULL ||
         vqarray_length(scte128_data) == 0)
   {
      return NULL;
   }

   int found_ebp = 0;
   for (vqarray_iterator_t *it = vqarray_iterator_new(scte128_data); vqarray_iterator_has_next(it);)
   {
      ts_scte128_private_data_t *scte128 = (ts_scte128_private_data_t*)vqarray_iterator_next(it);

      // Validate that we have a tag of 0xDF and a format id of 'EBP0' (0x45425030)
      if (scte128 != NULL && scte128->tag == 0xDF && scte128->format_identifier == 0x45425030)
      {
         if (found_ebp)
         {
            LOG_ERROR_ARGS("FileIngestThread %d: FAIL: Multiple EBP structures detected with a single PES packet: PID %d", 
               threadNum, streamInfo->PID);

            streamInfo->streamPassFail = 0;
               
            return NULL;
         }

         found_ebp = 1;

         if (!ts->header.payload_unit_start_indicator) 
         {
            LOG_ERROR_ARGS("FileIngestThread %d: FAIL: EBP present on a TS packet that does not have PUSI bit set: PID %d", 
               threadNum, streamInfo->PID);

            streamInfo->streamPassFail = 0;

            return NULL;
         }

         // Parse the EBP
         ebp = ebp_new();
         if (!ebp_read(ebp, scte128))
         {
            LOG_ERROR_ARGS("FileIngestThread %d: FAIL: Error parsing EBP: PID %d", 
               threadNum, streamInfo->PID);

            streamInfo->streamPassFail = 0;

            return NULL;
         }
      }
   }

   return ebp;
}


ebp_descriptor_t* getEBPDescriptor (elementary_stream_info_t *esi)
{
   vqarray_t *descriptors = esi->descriptors;

   for (int i=0; i<vqarray_length(descriptors); i++)
   {
      descriptor_t* descriptor = vqarray_get(descriptors, i);
/*      if (descriptor == NULL)
      {
         printf ("descriptor %d NULL\n", i);
      }
      else
      {
         printf ("descriptor %d: tag = %d\n", i, descriptor->tag);
      }
      */

      if (descriptor != NULL && descriptor->tag == EBP_DESCRIPTOR)
      {
         return (ebp_descriptor_t*)descriptor;
      }
   }

   return NULL;
}

component_name_descriptor_t* getComponentNameDescriptor (elementary_stream_info_t *esi)
{
   vqarray_t *descriptors = esi->descriptors;

   for (int i=0; i<vqarray_length(descriptors); i++)
   {
      descriptor_t* descriptor = vqarray_get(descriptors, i);
/*      if (descriptor == NULL)
      {
         printf ("descriptor %d NULL\n", i);
      }
      else
      {
         printf ("descriptor %d: tag = %d\n", i, descriptor->tag);
      }
      */

      if (descriptor != NULL && descriptor->tag == COMPONENT_NAME_DESCRIPTOR)
      {
         return (component_name_descriptor_t*)descriptor;
      }
   }

   return NULL;
}

ac3_descriptor_t* getAC3Descriptor (elementary_stream_info_t *esi)
{
   vqarray_t *descriptors = esi->descriptors;

   for (int i=0; i<vqarray_length(descriptors); i++)
   {
      descriptor_t* descriptor = vqarray_get(descriptors, i);
/*      if (descriptor == NULL)
      {
         printf ("descriptor %d NULL\n", i);
      }
      else
      {
         printf ("descriptor %d: tag = %d\n", i, descriptor->tag);
      }
      */

      if (descriptor != NULL && descriptor->tag == AC3_DESCRIPTOR)
      {
         return (ac3_descriptor_t*)descriptor;
      }
   }

   return NULL;
}

language_descriptor_t* getLanguageDescriptor (elementary_stream_info_t *esi)
{
   vqarray_t *descriptors = esi->descriptors;

   for (int i=0; i<vqarray_length(descriptors); i++)
   {
      descriptor_t* descriptor = vqarray_get(descriptors, i);
/*      if (descriptor == NULL)
      {
         printf ("descriptor %d NULL\n", i);
      }
      else
      {
         printf ("descriptor %d: tag = %d\n", i, descriptor->tag);
      }
      */

      if (descriptor != NULL && descriptor->tag == ISO_639_LANGUAGE_DESCRIPTOR)
      {
         return (language_descriptor_t*)descriptor;
      }
   }

   return NULL;
}

static int pmt_processor(mpeg2ts_program_t *m2p, void *arg)
{
   LOG_INFO_ARGS ("pmt_processor: arg = %x", (unsigned int) arg);
   if (m2p == NULL || m2p->pmt == NULL) // if we don't have any PSI, there's nothing we can do
      return 0;

   pid_info_t *pi = NULL;
   for (int i = 0; i < vqarray_length(m2p->pids); i++) // TODO replace linear search w/ hashtable lookup in the future
   {
      if ((pi = vqarray_get(m2p->pids, i)) != NULL)
      {
         int handle_pid = 0;

         if (IS_VIDEO_STREAM(pi->es_info->stream_type))
         {
            // Look for the SCTE128 descriptor in the ES loop
            descriptor_t *desc = NULL;
            for (int d = 0; d < vqarray_length(pi->es_info->descriptors); d++)
            {
               if ((desc = vqarray_get(pi->es_info->descriptors, d)) != NULL && desc->tag == 0x97)
               {
                  mpeg2ts_program_enable_scte128(m2p);
               }
            }
            handle_pid = 1;
         }
         else if (IS_AUDIO_STREAM(pi->es_info->stream_type))
         {
            handle_pid = 1;
         }

         if (handle_pid)
         {
            pes_demux_t *pd = pes_demux_new(validate_pes_packet);
            pd->pes_arg = arg;
            pd->pes_arg_destructor = NULL;

            // hook PES demuxer to the PID processor
            demux_pid_handler_t *demux_handler = calloc(1, sizeof(demux_pid_handler_t));
            demux_handler->process_ts_packet = pes_demux_process_ts_packet;
            demux_handler->arg = pd;
            demux_handler->arg_destructor = (arg_destructor_t)pes_demux_free;

            // hook PES demuxer to the PID processor
            demux_pid_handler_t *demux_validator = calloc(1, sizeof(demux_pid_handler_t));
            demux_validator->process_ts_packet = validate_ts_packet;
            demux_validator->arg = arg;
            demux_validator->arg_destructor = NULL;

            // hook PID processor to PID
            mpeg2ts_program_register_pid_processor(m2p, pi->es_info->elementary_PID, demux_handler, demux_validator);
         }
      }
   }

   return 1;
}

static int pat_processor(mpeg2ts_stream_t *m2s, void *arg)
{

   for (int i = 0; i < vqarray_length(m2s->programs); i++)
   {
      mpeg2ts_program_t *m2p = vqarray_get(m2s->programs, i);

      if (m2p == NULL) continue;
      m2p->pmt_processor =  (pmt_processor_t)pmt_processor;
      m2p->arg = arg;
   }
   return 1;
}

void *EBPFileIngestThreadProc(void *threadParams)
{
   int returnCode = 0;

   ebp_file_ingest_thread_params_t * ebpFileIngestThreadParams = (ebp_file_ingest_thread_params_t *)threadParams;
   LOG_INFO_ARGS("EBPFileIngestThread %d starting...ebpFileIngestThreadParams = %x", 
      ebpFileIngestThreadParams->threadNum, (unsigned int)ebpFileIngestThreadParams);

   // do file reading here
   FILE *infile = NULL;
   if ((infile = fopen(ebpFileIngestThreadParams->filePath, "rb")) == NULL)
   {
      LOG_ERROR_ARGS("EBPFileIngestThread %d: FAIL: Cannot open file %s - %s", 
         ebpFileIngestThreadParams->threadNum, ebpFileIngestThreadParams->filePath, strerror(errno));

      ebpFileIngestThreadParams->filePassFail = 0;
      cleanupAndExit(ebpFileIngestThreadParams);
   }

   mpeg2ts_stream_t *m2s = NULL;

   if (NULL == (m2s = mpeg2ts_stream_new()))
   {
      LOG_ERROR_ARGS("EBPFileIngestThread %d: FAIL: Error creating MPEG-2 STREAM object",
         ebpFileIngestThreadParams->threadNum);
      ebpFileIngestThreadParams->filePassFail = 0;
      cleanupAndExit(ebpFileIngestThreadParams);
   }

   // Register EBP descriptor parser
   descriptor_table_entry_t *desc = calloc(1, sizeof(descriptor_table_entry_t));
   desc->tag = EBP_DESCRIPTOR;
   desc->free_descriptor = ebp_descriptor_free;
   desc->print_descriptor = ebp_descriptor_print;
   desc->read_descriptor = ebp_descriptor_read;
   if (!register_descriptor(desc))
   {
      LOG_ERROR_ARGS("EBPFileIngestThread %d: FAIL: Could not register EBP descriptor parser",
         ebpFileIngestThreadParams->threadNum);
      ebpFileIngestThreadParams->filePassFail = 0;

      cleanupAndExit(ebpFileIngestThreadParams);
   }

   m2s->pat_processor = (pat_processor_t)pat_processor;
   m2s->arg = ebpFileIngestThreadParams;
   m2s->arg_destructor = NULL;

   int num_packets = 4096;
   uint8_t *ts_buf = malloc(TS_SIZE * 4096);

   int total_packets = 0;

   while ((num_packets = fread(ts_buf, TS_SIZE, 4096, infile)) > 0)
   {
      total_packets += num_packets;
      LOG_DEBUG_ARGS ("total_packets = %d, num_packets = %d", total_packets, num_packets);
      for (int i = 0; i < num_packets; i++)
      {
         ts_packet_t *ts = ts_new();
         ts_read(ts, ts_buf + i * TS_SIZE, TS_SIZE);
         int returnCode = mpeg2ts_stream_read_ts_packet(m2s, ts);
         // GORP: error checking here: need to augment mpeg2ts_stream_read_ts_packet's error checking
      }
   }

   mpeg2ts_stream_free(m2s);

   fclose(infile);

   cleanupAndExit(ebpFileIngestThreadParams);

   return NULL;
}

void findFIFO (uint32_t PID, ebp_stream_info_t **streamInfos, int numStreams,
   thread_safe_fifo_t**fifoOut, int *fifoIndex)
{
   *fifoOut = NULL;
   *fifoIndex = -1;

   for (int i=0; i<numStreams; i++)
   {
      if (streamInfos[i] != NULL)
      {
         if (PID == streamInfos[i]->PID)
         {
            *fifoOut = streamInfos[i]->fifo;
            *fifoIndex = i;
            return;
         }
      }
   }
}

int postToFIFO  (uint64_t PTS, uint32_t sapType, ebp_t *ebp, ebp_descriptor_t *ebpDescriptor,
                 uint32_t PID, ebp_file_ingest_thread_params_t *ebpFileIngestThreadParams, uint8_t partitionId)
{
   ebp_segment_info_t *ebpSegmentInfo = 
      (ebp_segment_info_t *)malloc (sizeof (ebp_segment_info_t));

   ebpSegmentInfo->PTS = PTS;
   ebpSegmentInfo->SAPType = sapType;
   ebpSegmentInfo->EBP = ebp;
   ebpSegmentInfo->partitionId = partitionId;
   // make a copy of ebp_descriptor since this mem could be freed before being prcessed by analysis thread
   ebpSegmentInfo->latestEBPDescriptor = ebp_descriptor_copy(ebpDescriptor);  
   
   int arrayIndex = get2DArrayIndex (ebpFileIngestThreadParams->threadNum, 0, ebpFileIngestThreadParams->numStreams);
   ebp_stream_info_t **streamInfos = &((ebpFileIngestThreadParams->allStreamInfos)[arrayIndex]);

   thread_safe_fifo_t* fifo = NULL;
   int fifoIndex = -1;
   findFIFO (PID, streamInfos, ebpFileIngestThreadParams->numStreams, &fifo, &fifoIndex);
   if (fifo == NULL)
   {
      // this should never happen
      LOG_ERROR_ARGS ("EBPFileIngestThread %d: FAIL: FIFO not found for PID %d", 
         ebpFileIngestThreadParams->threadNum, PID);
      return -1;
   }

   LOG_INFO_ARGS ("EBPFileIngestThread %d: POSTING PTS %"PRId64" to for partition %d FIFO (PID %d)", 
      ebpFileIngestThreadParams->threadNum, PTS, partitionId, PID);
   int returnCode = fifo_push (fifo, ebpSegmentInfo);
   if (returnCode != 0)
   {
      LOG_ERROR_ARGS ("EBPFileIngestThread %d: FATAL error %d calling fifo_push on fifo %d (PID %d)", 
         ebpFileIngestThreadParams->threadNum, 
         returnCode, (streamInfos[fifoIndex])->fifo->id, PID);
      exit (-1);
   }

   return 0;
}

void triggerImplicitBoundaries (int threadNum, ebp_stream_info_t **streamInfoArray, int numStreams, int numFiles,
   int currentStreamIndex, uint64_t PTS, uint8_t partitionId, int currentFileIndex)
{
   uint32_t currentPID = streamInfoArray[get2DArrayIndex (currentFileIndex, currentStreamIndex, numStreams)]->PID;

   for (int fileIndex=0; fileIndex<numFiles; fileIndex++)
   {
      for (int streamIndex=0; streamIndex<numStreams; streamIndex++)
      {
         int arrayIndex = get2DArrayIndex (fileIndex, streamIndex, numStreams);

         if (streamIndex == currentStreamIndex && fileIndex == currentFileIndex) continue;
      
         ebp_boundary_info_t *ebpBoundaryInfo = streamInfoArray[arrayIndex]->ebpBoundaryInfo;

         if (ebpBoundaryInfo[partitionId].isBoundary && 
            ebpBoundaryInfo[partitionId].isImplicit &&
            (ebpBoundaryInfo[partitionId].implicitFileIndex == currentFileIndex) &&
            (ebpBoundaryInfo[partitionId].implicitPID == currentPID))
         {
            LOG_INFO_ARGS("EBPFileIngestThread %d: Triggering implicit boundary", 
                     currentFileIndex);
               
            uint64_t *PTSTemp = (uint64_t *)malloc(sizeof(uint64_t));
            *PTSTemp = PTS;
            varray_insert(ebpBoundaryInfo[partitionId].queueLastImplicitPTS, 0, PTSTemp);
         }
      }
   }
}


void detectBoundary(int threadNum, ebp_t* ebp, ebp_stream_info_t *streamInfo, uint64_t PTS, int *isBoundary)
{
   // isBoundary is an array indexed by PartitionId;

   ebp_boundary_info_t *ebpBoundaryInfo = streamInfo->ebpBoundaryInfo;

   // first check for implicit boundaries
   for (int i=1; i<EBP_NUM_PARTITIONS; i++)  // skip partition 0
   {

      if (ebpBoundaryInfo[i].isBoundary && 
         ebpBoundaryInfo[i].isImplicit && 
         varray_length(ebpBoundaryInfo[i].queueLastImplicitPTS) != 0)
      {
         uint64_t *PTSTemp = (uint64_t *) varray_peek(ebpBoundaryInfo[i].queueLastImplicitPTS);
/*         if (i == 1 || i == 2)
         {
            LOG_INFO_ARGS("EBPFileIngestThread %d: detectBoundary partition %d: lastImplicitPTS = %"PRId64", PTS = %"PRId64" (PID %d)", 
                  threadNum, i, *PTSTemp, PTS, streamInfo->PID);
         }
         */

         if (PTS > *PTSTemp)
         {
            varray_pop(ebpBoundaryInfo[i].queueLastImplicitPTS);
            isBoundary[i] = 1;
         }
      }
   }

   if (ebp == NULL)
   {
      return;
   }

   // next check for explicit boundaries
   if (ebp->ebp_segment_flag)
   {
      if (ebpBoundaryInfo[EBP_PARTITION_SEGMENT].isBoundary)
      {
         if (ebpBoundaryInfo[EBP_PARTITION_SEGMENT].isImplicit)
         {
            LOG_ERROR_ARGS("FileIngestThread %d: FAIL: Unexpected EBP struct detected for partition %d: PID %d", 
               threadNum, EBP_PARTITION_SEGMENT, streamInfo->PID);
         }
         else
         {
            isBoundary[EBP_PARTITION_SEGMENT] = 1;
         }
      }
      else
      {
         LOG_ERROR_ARGS("FileIngestThread %d: FAIL: Unexpected EBP struct detected for partition %d: PID %d", 
            threadNum, EBP_PARTITION_SEGMENT, streamInfo->PID);
      }
   }

   if (ebp->ebp_fragment_flag)
   {
      if (ebpBoundaryInfo[EBP_PARTITION_FRAGMENT].isBoundary)
      {
         if (ebpBoundaryInfo[EBP_PARTITION_FRAGMENT].isImplicit)
         {
            LOG_ERROR_ARGS("FileIngestThread %d: FAIL: Unexpected EBP struct detected for partition %d: PID %d", 
               threadNum, EBP_PARTITION_FRAGMENT, streamInfo->PID);
         }
         else
         {
            isBoundary[EBP_PARTITION_FRAGMENT] = 1;
         }
      }
      else
      {
         LOG_ERROR_ARGS("FileIngestThread %d: FAIL: Unexpected EBP struct detected for partition %d: PID %d", 
            threadNum, EBP_PARTITION_FRAGMENT, streamInfo->PID);
      }
   }

   if (ebp->ebp_ext_partition_flag)
   {
      uint8_t ebp_ext_partitions_temp = ebp->ebp_ext_partitions;
      ebp_ext_partitions_temp = ebp_ext_partitions_temp >> 1; // skip partition1d 0

      // partiton 1 and 2 are not included in the extended partition mask, so skip them
      for (int i=3; i<EBP_NUM_PARTITIONS; i++)
      {
         if (ebp_ext_partitions_temp & 0x1)
         {
            if ((ebpBoundaryInfo[i].isBoundary))
            {
               if (ebpBoundaryInfo[i].isImplicit)
               {
                  LOG_ERROR_ARGS("FileIngestThread %d: FAIL: Unexpected EBP struct detected for partition %d: PID %d", 
                     threadNum, i, streamInfo->PID);
               }
               else
               {
                  isBoundary[i] = 1;
               }
            }
            else
            {
               LOG_ERROR_ARGS("FileIngestThread %d: FAIL: Unexpected EBP struct detected for partition %d: PID %d", 
                  threadNum, i, streamInfo->PID);
            }
         }

         ebp_ext_partitions_temp = ebp_ext_partitions_temp >> 1;
      }
   }

}

void cleanupAndExit(ebp_file_ingest_thread_params_t *ebpFileIngestThreadParams)
{
   int arrayIndex = get2DArrayIndex (ebpFileIngestThreadParams->threadNum, 0, ebpFileIngestThreadParams->numStreams);
   ebp_stream_info_t **streamInfos = &(ebpFileIngestThreadParams->allStreamInfos[arrayIndex]);

   int returnCode = 0;
   void *element = NULL;
   for (int i=0; i<ebpFileIngestThreadParams->numStreams; i++)
   {
      returnCode = fifo_push (streamInfos[i]->fifo, element);
      if (returnCode != 0)
      {
         LOG_ERROR_ARGS ("EBPFileIngestThread %d: FATAL error %d calling fifo_push on fifo %d (PID %d) during cleanup", 
            ebpFileIngestThreadParams->threadNum, returnCode, 
            streamInfos[i]->fifo->id, streamInfos[i]->PID);

         // fatal error: exit
         exit(-1);
      }
   }

   LOG_INFO_ARGS ("EBPFileIngestThread %d exiting...", ebpFileIngestThreadParams->threadNum);
   free (ebpFileIngestThreadParams);
   pthread_exit(NULL);
}



