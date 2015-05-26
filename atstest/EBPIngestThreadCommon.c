/*
Copyright (c) 2015, Cable Television Laboratories, Inc.(“CableLabs”)
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of CableLabs nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL CABLELABS BE LIABLE FOR ANY
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
#include <psi.h>
#include <ebp.h>
#include <scte35.h>

#include "h264_stream.h"

#include "EBPSegmentAnalysisThread.h"
#include "EBPFileIngestThread.h"
#include "EBPThreadLogging.h"

#include "ATSTestDefines.h"
#include "ATSTestReport.h"
#include "ATSTestAppConfig.h"



static char *STREAM_TYPE_UNKNOWN = "UNKNOWN";
static char *STREAM_TYPE_VIDEO = "VIDEO";
static char *STREAM_TYPE_AUDIO = "AUDIO";

static char* getStreamTypeDesc (elementary_stream_info_t *esi);


static int validate_ts_packet(ts_packet_t *ts, elementary_stream_info_t *es_info, void *arg)
{

   if (IS_SCTE35_STREAM(es_info->stream_type))
   {
      // GORP: handle tables split across mult packets

      ebp_ingest_thread_params_t *ebpIngestThreadParams = (ebp_ingest_thread_params_t *)arg;
      
      // need last PTS here to evaluate if this SCTE has come in too close to PTS
      uint64_t currentPTS = ebpIngestThreadParams->currentVideoPTS;

      pruneOldSCTE35Map(ebpIngestThreadParams->mapOldSCTE35SpliceInserts, currentPTS);

      LOG_INFO_ARGS ("SCTE35 table detected for thread %d", ebpIngestThreadParams->threadNum);

      scte35_splice_info_section* splice_info = scte35_splice_info_section_new(); 
      
      int returnCode = scte35_splice_info_section_read(splice_info, ts->payload.bytes, 
         ts->payload.len, ts->header.payload_unit_start_indicator,
         &(ebpIngestThreadParams->scte35TableBuffer));
      if (returnCode < 0)
      {
         LOG_ERROR_ARGS("IngestThread %d: Error parsing SCTE35 table", ebpIngestThreadParams->threadNum);
         reportAddErrorLogArgs("IngestThread %d: Error parsing SCTE35 table", ebpIngestThreadParams->threadNum);
         return 0;
      }
      else if (returnCode == 0)
      {
         LOG_INFO_ARGS("IngestThread %d: SCTE35 table mult TS packet table...", ebpIngestThreadParams->threadNum);
         return 0;
      }

      scte35_splice_info_section_print_stdout(splice_info); 

      if (is_splice_insert (splice_info))
      {
         scte35_splice_insert* spliceInsert = get_splice_insert (splice_info);

         // check for entry in map of old SCTE35 to be sure eventId is not re-used prematurely
         LOG_INFO_ARGS("IngestThread %d: Searching for reuse of eventID %d", ebpIngestThreadParams->threadNum,
            spliceInsert->splice_event_id);
         void * value = hashtable_search(ebpIngestThreadParams->mapOldSCTE35SpliceInserts, &(spliceInsert->splice_event_id));
         if (value != NULL)
         {
            LOG_WARN_ARGS("IngestThread %d: FAIL: SCTE35 event ID %d re-used prematurely", 
               ebpIngestThreadParams->threadNum, spliceInsert->splice_event_id);
            reportAddErrorLogArgs("IngestThread %d: FAIL: SCTE35 event ID %d re-used prematurely", 
               ebpIngestThreadParams->threadNum, spliceInsert->splice_event_id);

            ebpIngestThreadParams->ingestPassFail = 0;
         }
         else
         {
            LOG_INFO_ARGS("IngestThread %d: eventID %d NOT re-used", ebpIngestThreadParams->threadNum,
               spliceInsert->splice_event_id);
         }
               
         // if splice insert message, add to SCTE35 PTS lists for all partitions
         // if program_splice == 1, then add to all PIDS, else add to specific PIDS

         if (spliceInsert -> program_splice_flag)
         {
            uint64_t PTS = get_splice_insert_PTS (splice_info);
            if (PTS != 0)
            {
               // GORP: if splice immediate flag, how to handle?

               LOG_INFO_ARGS("IngestThread %d: scte35MinimumPrerollSeconds = %f, PTS = %"PRId64", currentPTS = %"PRId64"", 
                  ebpIngestThreadParams->threadNum, g_ATSTestAppConfig.scte35MinimumPrerollSeconds, PTS, currentPTS);
               LOG_INFO_ARGS("IngestThread %d: scte35MinimumPrerollSeconds = %f", ebpIngestThreadParams->threadNum,
                  g_ATSTestAppConfig.scte35MinimumPrerollSeconds);
               if (PTS < currentPTS + g_ATSTestAppConfig.scte35MinimumPrerollSeconds * 90000)
               {
                  LOG_WARN_ARGS("IngestThread %d: FAIL: current PTS %"PRId64" is too close to SCTE35 PTS %"PRId64" ", 
                     ebpIngestThreadParams->threadNum, currentPTS, PTS);
                  reportAddErrorLogArgs("IngestThread %d: FAIL: current PTS %"PRId64" is too close to SCTE35 PTS %"PRId64" ", 
                     ebpIngestThreadParams->threadNum, currentPTS, PTS);

                  ebpIngestThreadParams->ingestPassFail = 0;
               }

               int arrayIndex = get2DArrayIndex (ebpIngestThreadParams->threadNum, 0, ebpIngestThreadParams->numStreams);    
               ebp_stream_info_t **streamInfos = &((ebpIngestThreadParams->allStreamInfos)[arrayIndex]);

               for (int i=0; i<ebpIngestThreadParams->numStreams; i++)
               {
                  addSCTE35Point_AllBoundaries (ebpIngestThreadParams->threadNum, streamInfos[i], splice_info);
               }
            }
         }
         else
         {
            if (spliceInsert->components != NULL)
            {
               // GORP: if splice immediate flag, how to handle?

               for (int i=0; i<vqarray_length(spliceInsert->components); i++)
               {
                  scte35_splice_insert_component *component = (scte35_splice_insert_component *) vqarray_get (spliceInsert->components, i);
                  if (component->splice_time != NULL && component->splice_time->pts_time != 0)
                  {
                     uint64_t scte35PTS = component->splice_time->pts_time + splice_info->pts_adjustment;
                     if (scte35PTS > (currentPTS + (uint64_t)(g_ATSTestAppConfig.scte35MinimumPrerollSeconds * 90000)))
                     {
                        LOG_WARN_ARGS("IngestThread %d: FAIL: current PTS %"PRId64" is too close to SCTE35 PTS %"PRId64" ", 
                           ebpIngestThreadParams->threadNum, currentPTS, scte35PTS);
                        reportAddErrorLogArgs("IngestThread %d: FAIL: current PTS %"PRId64" is too close to SCTE35 PTS %"PRId64" ", 
                           ebpIngestThreadParams->threadNum, currentPTS, scte35PTS);

                        ebpIngestThreadParams->ingestPassFail = 0;
                     }

                     int arrayIndex = get2DArrayIndex (ebpIngestThreadParams->threadNum, 0, ebpIngestThreadParams->numStreams);    
                     ebp_stream_info_t **streamInfos = &((ebpIngestThreadParams->allStreamInfos)[arrayIndex]);

                     for (int i=0; i<ebpIngestThreadParams->numStreams; i++)
                     {
                        if (streamInfos[i]->PID == component->component_tag)
                        {
                           addSCTE35Point_AllBoundaries (ebpIngestThreadParams->threadNum, streamInfos[i], 
                              splice_info);
                        }
                     }
                  }
               }
            }
         }
      }
      if (is_time_signal (splice_info))
      {
         scte35_time_signal* spliceTimeSignal = get_time_signal (splice_info);
         if (spliceTimeSignal->splice_time->time_specified_flag)
         {
            uint64_t PTS = spliceTimeSignal->splice_time->pts_time + splice_info->pts_adjustment;
            if (PTS != 0)
            {
               int arrayIndex = get2DArrayIndex (ebpIngestThreadParams->threadNum, 0, ebpIngestThreadParams->numStreams);    
               ebp_stream_info_t **streamInfos = &((ebpIngestThreadParams->allStreamInfos)[arrayIndex]);

               for (int i=0; i<ebpIngestThreadParams->numStreams; i++)
               {
                  addSCTE35Point_AllBoundaries (ebpIngestThreadParams->threadNum, streamInfos[i], splice_info);
               }
            }
         }
      }


      scte35_splice_info_section_free (splice_info);
      return 0;
   }

   return 1;
}

static char* getStreamTypeDesc (elementary_stream_info_t *esi)
{
   if (IS_VIDEO_STREAM(esi->stream_type)) return STREAM_TYPE_VIDEO;
   if (IS_AUDIO_STREAM(esi->stream_type)) return STREAM_TYPE_AUDIO;

   return STREAM_TYPE_UNKNOWN;
}

/*
static void printBoundaryInfoArray(ebp_boundary_info_t *boundaryInfoArray)
{
   LOG_INFO ("      EBP Boundary Info:");

   for (int i=0; i<EBP_NUM_PARTITIONS; i++)
   {
      if (boundaryInfoArray[i].isBoundary)
      {
         if (boundaryInfoArray[i].isImplicit)
         {
            LOG_INFO_ARGS ("         PARTITION %d: IMPLICIT, PID = %d, FileIndex = %d", i, boundaryInfoArray[i].implicitPID, 
               boundaryInfoArray[i].implicitFileIndex);
         }
         else
         {
            LOG_INFO_ARGS ("         PARTITION %d: EXPLICIT", i);
         }
      }
   }
}
*/

static int validate_pes_packet(pes_packet_t *pes, elementary_stream_info_t *esi, vqarray_t *ts_queue, void *arg)
{
   LOG_DEBUG("validate_pes_packet");
   // Get the first TS packet and check it for EBP


   ts_packet_t *first_ts = (ts_packet_t*)vqarray_get(ts_queue,0);
   if (first_ts == NULL)
   {
      // GORP: should this be an error?
      pes_free(pes);
      return 1; // Don't care about this packet
   }

   ebp_ingest_thread_params_t *ebpIngestThreadParams = (ebp_ingest_thread_params_t *)arg;

   int arrayIndex = get2DArrayIndex (ebpIngestThreadParams->threadNum, 0, 
      ebpIngestThreadParams->numStreams);    
   ebp_stream_info_t **streamInfos = &((ebpIngestThreadParams->allStreamInfos)[arrayIndex]);

   thread_safe_fifo_t *fifo = NULL;
   int fifoIndex = -1;
   findFIFO (esi->elementary_PID, streamInfos, ebpIngestThreadParams->numStreams,
      &fifo, &fifoIndex);
   ebp_stream_info_t * streamInfo = streamInfos[fifoIndex];

   if (streamInfo->isVideo)
   {
      ebpIngestThreadParams->currentVideoPTS = pes->header.PTS;
   }
   
   if (fifo == NULL)
   {
      // this can happen if the PES packet is SCTE35
      LOG_WARN_ARGS("IngestThread %d: FAIL: Cannot find fifo for PID %d (%s)", 
         ebpIngestThreadParams->threadNum, esi->elementary_PID, getStreamTypeDesc (esi));
      reportAddErrorLogArgs("IngestThread %d: FAIL: Cannot find fifo for PID %d (%s)", 
         ebpIngestThreadParams->threadNum, esi->elementary_PID, getStreamTypeDesc (esi));

      ebpIngestThreadParams->ingestPassFail = 0;

      pes_free(pes);
      return 1;
   }

   ebp_t* ebp = getEBP(first_ts, streamInfo, ebpIngestThreadParams->threadNum);
   if (ATS_TEST_CASE_AUDIO_IMPLICIT_TRIGGER || ATS_TEST_CASE_AUDIO_XFILE_IMPLICIT_TRIGGER)
   {
      // testing -- removes ebp from audio to test implicit triggering
      if (esi->elementary_PID == 482)
      {
         ebp = NULL;
      }
   }

   if (ebp != NULL)
   {
      LOG_DEBUG_ARGS("IngestThread %d: Found EBP data in transport packet: PID %d (%s)", 
         ebpIngestThreadParams->threadNum, esi->elementary_PID, getStreamTypeDesc (esi));
//      ebp_print_stdout(ebp);
         
      if (ebp_validate_groups(ebp) != 0)
      {
         // if there is a ebp in the stream, then we need an ebp_descriptor to interpret it
         LOG_ERROR_ARGS("IngestThread %d: FAIL: EBP group validation failed: PID %d (%s)", 
            ebpIngestThreadParams->threadNum, esi->elementary_PID, getStreamTypeDesc (esi));
         reportAddErrorLogArgs("IngestThread %d: FAIL: EBP group validation failed: PID %d (%s)", 
            ebpIngestThreadParams->threadNum, esi->elementary_PID, getStreamTypeDesc (esi));

         streamInfo->streamPassFail = 0;
      }
   }
   else
   {
      LOG_DEBUG_ARGS("IngestThread %d: No EBP data in transport packet: PID %d (%s)", 
         ebpIngestThreadParams->threadNum, esi->elementary_PID, getStreamTypeDesc (esi));
   }

      
   // isBoundary is an array indexed by PartitionId -- we will analyze the segment once
   // for each partition that triggered a boundary
   int *isBoundary = (int *)calloc (EBP_NUM_PARTITIONS, sizeof(int));
//   LOG_INFO_ARGS("EBPIngestThread %d: Calling detectBoundary. (PID %d)", 
//            ebpIngestThreadParams->threadNum, esi->elementary_PID);
   int boundaryDetected = detectBoundary(ebpIngestThreadParams->threadNum, ebp, 
      streamInfo, pes->header.PTS, isBoundary, ebpIngestThreadParams->mapOldSCTE35SpliceInserts);
//   LOG_INFO_ARGS("EBPIngestThread %d: DONE calling detectBoundary. (PID %d)", 
//            ebpIngestThreadParams->threadNum, esi->elementary_PID);

   int lastVideoPTSSet = 0;


   if (boundaryDetected)
   {
      LOG_INFO_ARGS("EBPIngestThread %d: boundaryDetected. (PID %d)", 
            ebpIngestThreadParams->threadNum, esi->elementary_PID);

      streamInfo->ebpChunkCntr++;
      pes->header.PTS = adjustPTSForTests (pes->header.PTS, ebpIngestThreadParams->threadNum, 
         streamInfo);
   }

   for (uint8_t i=0; i<EBP_NUM_PARTITIONS; i++)
   {
      if (isBoundary[i])
      {
         uint32_t sapType = getSAPType(pes, first_ts, esi->stream_type);

         ebp_descriptor_t* ebpDescriptor = getEBPDescriptor (esi);  // could be null

         ebp_t* ebp_copy = getEBP(first_ts, streamInfo, ebpIngestThreadParams->threadNum);

         if (ATS_TEST_CASE_ACQUISITION_TIME_NOT_PRESENT != 0 && ebp_copy != NULL && 
            ebpIngestThreadParams->threadNum == 0)
         {
            LOG_INFO_ARGS("IngestThread %d: ATS_TEST_CASE_ACQUISITION_TIME_NOT_PRESENT: ebp_copy->ebp_time_flag before: %d", 
               ebpIngestThreadParams->threadNum, ebp_copy->ebp_time_flag);
            ebp_copy->ebp_time_flag = 0;
         }
         if (ATS_TEST_CASE_ACQUISITION_TIME_MISMATCH != 0 && ebp_copy != NULL && esi->elementary_PID == 482)
         {
            if (ebpIngestThreadParams->threadNum == 1)
            {
               ebp_copy->ebp_acquisition_time = 190000;
            }
            else
            {
               ebp_copy->ebp_acquisition_time = 180000;
            }
            LOG_INFO_ARGS("IngestThread %d: ATS_TEST_CASE_ACQUISITION_TIME_MISMATCH: ebp_copy->ebp_acquisition_time: %"PRId64"", 
               ebpIngestThreadParams->threadNum, ebp_copy->ebp_acquisition_time);
         }

         int returnCode = postToFIFO (pes->header.PTS, sapType, ebp_copy, ebpDescriptor, 
            esi->elementary_PID, i, ebpIngestThreadParams->threadNum, 
            ebpIngestThreadParams->numStreams, 
            ebpIngestThreadParams->allStreamInfos);
         if (returnCode != 0)
         {
            LOG_ERROR_ARGS("IngestThread %d: FAIL: Error posting to FIFO for partition %d: PID %d (%s)", 
               ebpIngestThreadParams->threadNum, i, esi->elementary_PID, getStreamTypeDesc (esi));
            reportAddErrorLogArgs("IngestThread %d: FAIL: Error posting to FIFO for partition %d: PID %d (%s)", 
               ebpIngestThreadParams->threadNum, i, esi->elementary_PID, getStreamTypeDesc (esi));

            streamInfo->streamPassFail = 0;
         }

         // do the following once only -- use flag lastVideoPTSSet to tell if already done
         if ((lastVideoPTSSet == 0) && IS_VIDEO_STREAM(esi->stream_type))
         {
            // if this is video, set lastVideoPTS in audio fifos
            for (int ii=0; ii<ebpIngestThreadParams->numStreams; ii++)
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
            if (streamInfo->lastVideoChunkPTS != 0 && pes->header.PTS > (streamInfo->lastVideoChunkPTS + 3 * 90000))  // PTS is 90kHz ticks
            {
               LOG_ERROR_ARGS("IngestThread %d: FAIL: Audio PTS (%"PRId64") lags video PTS (%"PRId64") by more than 3 seconds: PID %d (%s)", 
                  ebpIngestThreadParams->threadNum, pes->header.PTS, streamInfo->lastVideoChunkPTS, 
                  esi->elementary_PID, getStreamTypeDesc (esi));
               reportAddErrorLogArgs("IngestThread %d: FAIL: Audio PTS (%"PRId64") lags video PTS (%"PRId64") by more than 3 seconds: PID %d (%s)", 
                  ebpIngestThreadParams->threadNum, pes->header.PTS, streamInfo->lastVideoChunkPTS, 
                  esi->elementary_PID, getStreamTypeDesc (esi));

               streamInfo->streamPassFail = 0;
            }
            streamInfo->lastVideoChunkPTSValid = 0;
         }

         triggerImplicitBoundaries (ebpIngestThreadParams->threadNum, 
            ebpIngestThreadParams->allStreamInfos, 
            ebpIngestThreadParams->numStreams, 
            ebpIngestThreadParams->numIngests, 
            fifoIndex, pes->header.PTS, (uint8_t) i /* partition ID */,
            ebpIngestThreadParams->threadNum);

      }
   }
   
   if (ebp != NULL)
   {
      ebp_free(ebp);
   }

   free (isBoundary);

   // check if this PTS exceeds any SCTE35 PTSs
   checkPTSAgainstSCTE35Points_AllBoundaries (ebpIngestThreadParams->threadNum, streamInfo, pes->header.PTS,
      ebpIngestThreadParams->mapOldSCTE35SpliceInserts);

   pes_free(pes);
   return 1;
}


uint32_t getSAPType(pes_packet_t *pes, ts_packet_t *first_ts,  uint32_t streamType)
{
   switch (streamType) 
   {
      case STREAM_TYPE_AVC:
         return getSAPType_AVC(pes, first_ts);
      case STREAM_TYPE_MPEG2_AAC:
         return getSAPType_MPEG2_AAC(pes, first_ts);
      case STREAM_TYPE_MPEG4_AAC:
         return getSAPType_MPEG4_AAC(pes, first_ts);
      case STREAM_TYPE_AC3_AUDIO:
         return getSAPType_AC3(pes, first_ts);
      case STREAM_TYPE_MPEG2_VIDEO:
         return getSAPType_MPEG2_VIDEO(pes, first_ts);

      // video
      case STREAM_TYPE_HEVC:
      case STREAM_TYPE_MPEG1_VIDEO:
      case STREAM_TYPE_MPEG4_VIDEO:
      case STREAM_TYPE_SVC:
      case STREAM_TYPE_MVC:
      case STREAM_TYPE_S3D_SC_MPEG2:
      case STREAM_TYPE_S3D_SC_AVC:

         // audio
      case STREAM_TYPE_MPEG1_AUDIO:
      case STREAM_TYPE_MPEG2_AUDIO:
      case STREAM_TYPE_MPEG4_AAC_RAW:

      default:
         return SAP_STREAM_TYPE_NOT_SUPPORTED;
   }
}

uint32_t getSAPType_MPEG2_AAC(pes_packet_t *pes, ts_packet_t *first_ts)
{
   // look for sync bits 0xFFF at start of PES to confirm that the PES starts an audio frame

   uint8_t* buf = pes->payload;
   int len = pes->payload_len;

   uint16_t syncWordExpected = 0xFFF;
   uint16_t syncWordActual = ((0xF0 & buf[1]) << 4) | buf[0];  // GORP: byte order here?

   if (syncWordExpected == syncWordActual)
   {
      return 1;  // GORP: is this right??
   }
   else
   {
      LOG_ERROR_ARGS("getSAPType_MPEG2_AAC: SAP_STREAM_TYPE_ERROR: Expected sync word = %x. Actual sync word = %x",
         syncWordExpected, syncWordActual);
      reportAddErrorLogArgs("getSAPType_MPEG2_AAC: SAP_STREAM_TYPE_ERROR: Expected sync word = %x. Actual sync word = %x",
         syncWordExpected, syncWordActual);
      return SAP_STREAM_TYPE_ERROR;
   }
}

uint32_t getSAPType_MPEG4_AAC(pes_packet_t *pes, ts_packet_t *first_ts)
{
   // same as MPEG 2
   return getSAPType_MPEG4_AAC(pes, first_ts);
}

uint32_t getSAPType_AC3(pes_packet_t *pes, ts_packet_t *first_ts)
{
// From the AC3 spec, Sec 7.1.1:
// "AC-3 bit streams contain coded exponents for all independent channels, all coupled channels,
// and for the coupling and low frequency effects channels (when they are enabled). Since audio
// information is not shared across frames, block 0 of every frame will include new exponents for
// every channel. Exponent information may be shared across blocks within a frame, so blocks 1
// through 5 may reuse exponents from previous blocks."
// 
// So, all AC3 frames are independent, and its sufficient to check that the PES packet 
// contents start with a new AC3 frame.

   uint8_t* buf = pes->payload;
   int len = pes->payload_len;

   uint16_t syncWordExpected = 0x0B77;
   uint16_t syncWordActual = (buf[1] << 8) | buf[0];  // GORP: byte order here?

   if (syncWordExpected == syncWordActual)
   {
      return 1;  // GORP: is this right??
   }
   else
   {
      return SAP_STREAM_TYPE_ERROR;
   }
}

uint32_t getSAPType_MPEG2_VIDEO(pes_packet_t *pes, ts_packet_t *first_ts)
{
   // GORP: fill in
   return SAP_STREAM_TYPE_NOT_SUPPORTED;
}

uint32_t getSAPType_AVC(pes_packet_t *pes, ts_packet_t *first_ts)
{
   uint32_t SAPType = SAP_STREAM_TYPE_ERROR;
   if (first_ts->adaptation_field.random_access_indicator) 
   {
      int nal_start, nal_end;
      int returnCode;
      uint8_t* buf = pes->payload;
      int len = pes->payload_len;

      // walk the nal units in the PES payload and check to see if they are type 1 or type 5 -- these determine SAP type
      int index = 0;
      while ((len > index) && ((returnCode = find_nal_unit(buf + index, len - index, &nal_start, &nal_end)) !=  0))
      {
//         printf("nal_start = %d, nal_end = %d \n", nal_start, nal_end);
         h264_stream_t* h = h264_new();
         read_nal_unit(h, &buf[nal_start + index], nal_end - nal_start);
//         printf("h->nal->nal_unit_type: %d \n", h->nal->nal_unit_type);
         if (h->nal->nal_unit_type == 5)
         {
            SAPType = 1;
            h264_free(h);
            break;
         }
         else if (h->nal->nal_unit_type == 1)
         {
            SAPType = 2;
            h264_free(h);
            break;
         }

         h264_free(h);
         index += nal_end;
      }
   }

   return SAPType;
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

   vqarray_iterator_t *it = vqarray_iterator_new(scte128_data);
   for (; vqarray_iterator_has_next(it);)
   {
      ts_scte128_private_data_t *scte128 = (ts_scte128_private_data_t*)vqarray_iterator_next(it);

      // Validate that we have a tag of 0xDF and a format id of 'EBP0' (0x45425030)
      if (scte128 != NULL && scte128->tag == 0xDF && scte128->format_identifier == 0x45425030)
      {
         if (found_ebp)
         {
            LOG_ERROR_ARGS("IngestThread %d: FAIL: Multiple EBP structures detected with a single PES packet: PID %d", 
               threadNum, streamInfo->PID);
            reportAddErrorLogArgs("IngestThread %d: FAIL: Multiple EBP structures detected with a single PES packet: PID %d", 
               threadNum, streamInfo->PID);

            streamInfo->streamPassFail = 0;
               
            vqarray_iterator_free (it);
            return NULL;
         }

         found_ebp = 1;

         if (!ts->header.payload_unit_start_indicator) 
         {
            LOG_ERROR_ARGS("IngestThread %d: FAIL: EBP present on a TS packet that does not have PUSI bit set: PID %d", 
               threadNum, streamInfo->PID);
            reportAddErrorLogArgs("IngestThread %d: FAIL: EBP present on a TS packet that does not have PUSI bit set: PID %d", 
               threadNum, streamInfo->PID);

            streamInfo->streamPassFail = 0;

            vqarray_iterator_free (it);
            return NULL;
         }

         // Parse the EBP
         ebp = ebp_new();
         if (!ebp_read(ebp, scte128))
         {
            LOG_ERROR_ARGS("IngestThread %d: FAIL: Error parsing EBP: PID %d", 
               threadNum, streamInfo->PID);
            reportAddErrorLogArgs("IngestThread %d: FAIL: Error parsing EBP: PID %d", 
               threadNum, streamInfo->PID);

            streamInfo->streamPassFail = 0;

            vqarray_iterator_free (it);
            return NULL;
         }
      }
   }
   vqarray_iterator_free (it);

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

int ingest_pmt_processor(mpeg2ts_program_t *m2p, void *arg)
{
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
         else if (IS_SCTE35_STREAM(pi->es_info->stream_type))
         {
            handle_pid = 1;
         }

         if (handle_pid)
         {
            LOG_INFO ("pmt_processor -- allocating....");
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

int ingest_pat_processor(mpeg2ts_stream_t *m2s, void *arg)
{
   for (int i = 0; i < vqarray_length(m2s->programs); i++)
   {
      mpeg2ts_program_t *m2p = vqarray_get(m2s->programs, i);

      if (m2p == NULL) continue;
      m2p->pmt_processor =  (pmt_processor_t)ingest_pmt_processor;
      m2p->arg = arg;
   }
   return 1;
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
   uint32_t PID, uint8_t partitionId, int threadNum, int numStreams, ebp_stream_info_t **allStreamInfos)
{
   ebp_segment_info_t *ebpSegmentInfo = 
      (ebp_segment_info_t *)calloc (1, sizeof (ebp_segment_info_t));

   ebpSegmentInfo->PTS = PTS;
   ebpSegmentInfo->SAPType = sapType;
   if (ATS_TEST_CASE_SAP_TYPE_MISMATCH_AND_TOO_LARGE != 0)
   {
      ebp->ebp_sap_flag = 1;
      ebp->ebp_sap_type = 5;
//      ebpSegmentInfo->SAPType = 3;
   }
   else if (ATS_TEST_CASE_SAP_TYPE_NOT_1_OR_2 != 0)
   {
      ebpSegmentInfo->SAPType = 3;
   }

   ebpSegmentInfo->EBP = ebp;
   ebpSegmentInfo->partitionId = partitionId;
   // make a copy of ebp_descriptor since this mem could be freed before being prcessed by analysis thread
   ebpSegmentInfo->latestEBPDescriptor = ebp_descriptor_copy(ebpDescriptor);  
   
   int arrayIndex = get2DArrayIndex (threadNum, 0, numStreams);
   ebp_stream_info_t **streamInfos = &(allStreamInfos[arrayIndex]);

   thread_safe_fifo_t* fifo = NULL;
   int fifoIndex = -1;
   findFIFO (PID, streamInfos, numStreams, &fifo, &fifoIndex);
   if (fifo == NULL)
   {
      // this should never happen
      LOG_ERROR_ARGS ("EBPIngestThread %d: FAIL: FIFO not found for PID %d", 
         threadNum, PID);
      reportAddErrorLogArgs ("EBPIngestThread %d: FAIL: FIFO not found for PID %d", 
         threadNum, PID);
      return -1;
   }

   LOG_INFO_ARGS ("EBPIngestThread %d: POSTING PTS %"PRId64" for partition %d to FIFO %d (PID %d)", 
      threadNum, PTS, partitionId, (streamInfos[fifoIndex])->fifo->id, PID);
 //  reportAddPTS (PTS, partitionId, threadNum, (streamInfos[fifoIndex])->fifo->id, PID);
   
   char ptsString[13];
   reportAddInfoLogArgs ("EBPIngestThread %d: POSTING PTS %"PRId64" (%s) for partition %d to FIFO %d (PID %d)", 
      threadNum, PTS, pts_dts_to_string(PTS, ptsString), 
      partitionId, (streamInfos[fifoIndex])->fifo->id, PID);

   reportAddPTS (PTS, partitionId, threadNum, fifoIndex, PID);

   int returnCode = fifo_push (fifo, ebpSegmentInfo);
   if (returnCode != 0)
   {
      LOG_ERROR_ARGS ("EBPIngestThread %d: FATAL error %d calling fifo_push on fifo %d (PID %d)", 
         threadNum, returnCode, (streamInfos[fifoIndex])->fifo->id, PID);
      reportAddErrorLogArgs ("EBPIngestThread %d: FATAL error %d calling fifo_push on fifo %d (PID %d)", 
         threadNum, returnCode, (streamInfos[fifoIndex])->fifo->id, PID);
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
            LOG_INFO_ARGS("EBPIngestThread %d: Triggering implicit boundary", 
                     currentFileIndex);
               
            uint64_t *PTSTemp = (uint64_t *)malloc(sizeof(uint64_t));
            *PTSTemp = PTS;
            varray_insert(ebpBoundaryInfo[partitionId].queueLastImplicitPTS, 0, PTSTemp);
         }
      }
   }
}

int detectBoundary(int threadNum, ebp_t* ebp, ebp_stream_info_t *streamInfo, uint64_t PTS, int *isBoundary,
                   hashtable_t *mapOldSCTE35SpliceInserts)
{
   // isBoundary is an array indexed by PartitionId;

   int nReturnCode = 0;

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
            LOG_INFO_ARGS("EBPIngestThread %d: detectBoundary partition %d: lastImplicitPTS = %"PRId64", PTS = %"PRId64" (PID %d)", 
                  threadNum, i, *PTSTemp, PTS, streamInfo->PID);
         }
         */

         if (PTS > *PTSTemp)
         {
            varray_pop(ebpBoundaryInfo[i].queueLastImplicitPTS);
            free (PTSTemp);
            isBoundary[i] = 1;
            nReturnCode = 1;

            if (ebpBoundaryInfo[i].listSCTE35 != NULL)
            {
               // check against SCTE35
               
               if (checkEBPAgainstSCTE35Points (ebpBoundaryInfo[i].listSCTE35, PTS, g_ATSTestAppConfig.ebpSCTE35PTSJitterSecs * 90000, threadNum,
                  i /* partitionID */, streamInfo->PID, mapOldSCTE35SpliceInserts))
               {
                  char ptsString[13];
                  reportAddInfoLogArgs("IngestThread %d: PTS %"PRId64" (%s) matches SCTE35 PTS for partition %d: PID %d: distance from last PTS = %"PRId64"", 
                     threadNum, PTS, pts_dts_to_string(PTS, ptsString), i /* partitionID */, streamInfo->PID, PTS - ebpBoundaryInfo[i].lastPTS);

                  // reset the lastPTS, because the SCTE35-inspired EBP will violate the prescribed EBP periodicity
                  ebpBoundaryInfo[i].lastPTS = 0;
               }
            }
         }
      }
   }

   if (ebp == NULL)
   {
      return nReturnCode;
   }

   // next check for explicit boundaries
   if (ebp->ebp_segment_flag)
   {
      if (ebpBoundaryInfo[EBP_PARTITION_SEGMENT].isBoundary)
      {
         if (ebpBoundaryInfo[EBP_PARTITION_SEGMENT].isImplicit)
         {
            LOG_ERROR_ARGS("EBPIngestThread %d: FAIL: Unexpected EBP struct (1) detected for partition %d: PID %d", 
               threadNum, EBP_PARTITION_SEGMENT, streamInfo->PID);
            reportAddErrorLogArgs("EBPIngestThread %d: FAIL: Unexpected EBP struct (1) detected for partition %d: PID %d", 
               threadNum, EBP_PARTITION_SEGMENT, streamInfo->PID);
         }
         else
         {
            isBoundary[EBP_PARTITION_SEGMENT] = 1;
            nReturnCode = 1;
         }
      }
      else
      {
         LOG_ERROR_ARGS("IngestThread %d: FAIL: Unexpected EBP struct detected (2) for partition %d: PID %d", 
            threadNum, EBP_PARTITION_SEGMENT, streamInfo->PID);
         reportAddErrorLogArgs("IngestThread %d: FAIL: Unexpected EBP struct detected (2) for partition %d: PID %d", 
            threadNum, EBP_PARTITION_SEGMENT, streamInfo->PID);
      }
   }

   if (ebp->ebp_fragment_flag)
   {
      if (ebpBoundaryInfo[EBP_PARTITION_FRAGMENT].isBoundary)
      {
         if (ebpBoundaryInfo[EBP_PARTITION_FRAGMENT].isImplicit)
         {
            LOG_ERROR_ARGS("IngestThread %d: FAIL: Unexpected EBP struct detected (1) for partition %d: PID %d", 
               threadNum, EBP_PARTITION_FRAGMENT, streamInfo->PID);
            reportAddErrorLogArgs("IngestThread %d: FAIL: Unexpected EBP struct detected (1) for partition %d: PID %d", 
               threadNum, EBP_PARTITION_FRAGMENT, streamInfo->PID);
         }
         else
         {
            isBoundary[EBP_PARTITION_FRAGMENT] = 1;
            nReturnCode = 1;
         }
      }
      else
      {
         LOG_ERROR_ARGS("IngestThread %d: FAIL: Unexpected EBP struct detected (2) for partition %d: PID %d", 
            threadNum, EBP_PARTITION_FRAGMENT, streamInfo->PID);
         reportAddErrorLogArgs("IngestThread %d: FAIL: Unexpected EBP struct detected (2) for partition %d: PID %d", 
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
                  LOG_ERROR_ARGS("IngestThread %d: FAIL: Unexpected EBP struct detected (1) for partition %d: PID %d", 
                     threadNum, i, streamInfo->PID);
                  reportAddErrorLogArgs("IngestThread %d: FAIL: Unexpected EBP struct detected (1) for partition %d: PID %d", 
                     threadNum, i, streamInfo->PID);
               }
               else
               {
                  isBoundary[i] = 1;
                  nReturnCode = 1;
               }
            }
            else
            {
               LOG_ERROR_ARGS("IngestThread %d: FAIL: Unexpected EBP struct detected (2) for partition %d: PID %d", 
                  threadNum, i, streamInfo->PID);
               reportAddErrorLogArgs("IngestThread %d: FAIL: Unexpected EBP struct detected (2) for partition %d: PID %d", 
                  threadNum, i, streamInfo->PID);
            }
         }

         ebp_ext_partitions_temp = ebp_ext_partitions_temp >> 1;
      }
   }

   for (int i=0; i<EBP_NUM_PARTITIONS; i++)
   {
      if (isBoundary[i] && ebpBoundaryInfo[i].listSCTE35 != NULL)
      {
         // check against SCTE35  
         if (checkEBPAgainstSCTE35Points (ebpBoundaryInfo[i].listSCTE35, PTS, g_ATSTestAppConfig.ebpSCTE35PTSJitterSecs * 90000, threadNum,
            i /* partitionID */, streamInfo->PID, mapOldSCTE35SpliceInserts))
         {
            char ptsString[13];
            reportAddInfoLogArgs("IngestThread %d: PTS %"PRId64" (%s) matches SCTE35 PTS for partition %d: PID %d: distance from last PTS = %"PRId64"", 
               threadNum, PTS, pts_dts_to_string(PTS, ptsString), i /* partitionID */, streamInfo->PID, PTS - ebpBoundaryInfo[i].lastPTS);

            // reset the lastPTS, because the SCTE35-inspired EBP will violate the prescribed EBP periodicity
            ebpBoundaryInfo[i].lastPTS = 0;
         }
      }
   }

   return nReturnCode;
}

uint64_t adjustPTSForTests (uint64_t PTSIn, int fileIndex, ebp_stream_info_t * streamInfo)
{
   uint64_t PTSOut = PTSIn;

   // change PTS here for tests
   if (ATS_TEST_CASE_AUDIO_PTS_GAP)
   {
      // in a particular file, for a particular triggered segment, subtract 1 second from audio PTS
      if (fileIndex == 1 && streamInfo->PID == 482 && streamInfo->ebpChunkCntr == 3)
      {
         LOG_INFO ("ATS_TEST_CASE_AUDIO_PTS_GAP: subtracting 1 second from PTS for file 1 / PID 482");
         PTSOut -= 90000;
      }
   }
   else if (ATS_TEST_CASE_AUDIO_PTS_OVERLAP)
   {
      // in a particular file, for a particular triggered segment, add 1 second from audio PTS
      if (fileIndex == 1 && streamInfo->PID == 482 && streamInfo->ebpChunkCntr == 3)
      {
         LOG_INFO ("ATS_TEST_CASE_AUDIO_PTS_GAP: adding 1 second from PTS for file 1 / PID 482");
         PTSOut += 90000;
      }
   }
   else if (ATS_TEST_CASE_VIDEO_PTS_GAP)
   {
      // in a particular file, for a particular triggered segment, subtract 1 second from video PTS
      if (fileIndex == 1 && streamInfo->PID == 481 && streamInfo->ebpChunkCntr == 3)
      {
         LOG_INFO ("ATS_TEST_CASE_AUDIO_PTS_GAP: subtracting 1 second from PTS for file 1 / PID 481");
         PTSOut -= 90000;
      }
   }
   else if (ATS_TEST_CASE_VIDEO_PTS_OVERLAP)
   {
      // in a particular file, for a particular triggered segment, add 1 second to video PTS
      if (fileIndex == 1 && streamInfo->PID == 481 && streamInfo->ebpChunkCntr == 3)
      {
         LOG_INFO ("ATS_TEST_CASE_AUDIO_PTS_GAP: adding 1 second from PTS for file 1 / PID 481");
         PTSOut += 90000;
      }
   }
   else if (ATS_TEST_CASE_AUDIO_PTS_OFFSET)
   {
      // in a particular file, subtract 1 second from audio PTS -- this offsets whole stream
      if (fileIndex == 1 && streamInfo->PID == 482)
      {
         LOG_INFO ("ATS_TEST_CASE_AUDIO_PTS_GAP: subtracting 1 second from PTS for file 1 / PID 482");
         PTSOut += 90000;
      }
   }
   else if (ATS_TEST_CASE_VIDEO_PTS_OFFSET)
   {
      // in a particular file, subtract 1 second from video PTS -- this offsets whole stream
      if (fileIndex == 1 && streamInfo->PID == 481)
      {
         LOG_INFO ("ATS_TEST_CASE_AUDIO_PTS_GAP: subtracting 1 second from PTS for file 1 / PID 481");
         PTSOut += 90000;
      }
   }
   else if (ATS_TEST_CASE_AUDIO_PTS_BIG_LAG)
   {
      // in all files, add 4 seconds to audio PTS
      if (streamInfo->PID == 483)
      {
         PTSOut += 90000 * 4;
      }
   }

   return PTSOut;
}

uint64_t getSCTE35PTS (scte35_splice_info_section *scte35InfoSection, uint32_t PID)
{
   LOG_INFO ("getSCTE35PTS: entering")
   uint64_t PTS = 0;
   if (scte35InfoSection->splice_command_type == SCTE35_SPLICE_INSERT_CMD)
   {
      scte35_splice_insert* spliceInsert = get_splice_insert (scte35InfoSection);
               
      LOG_INFO ("getSCTE35PTS: inside SCTE35_SPLICE_INSERT_CMD")
      if (spliceInsert -> program_splice_flag)
      {
         LOG_INFO ("getSCTE35PTS: program_splice_flag")
         PTS = get_splice_insert_PTS (scte35InfoSection);
         LOG_INFO_ARGS ("getSCTE35PTS: program_splice_flag: PTS = %"PRId64"", PTS)
      }
      else
      {
         LOG_INFO ("getSCTE35PTS: NO program_splice_flag")
         if (spliceInsert->components != NULL)
         {
            // GORP: if splice immediate flag, how to handle?

            for (int i=0; i<vqarray_length(spliceInsert->components); i++)
            {
               scte35_splice_insert_component *component = (scte35_splice_insert_component *) vqarray_get (spliceInsert->components, i);
               if (component->splice_time != NULL && 
                   component->splice_time->time_specified_flag && 
                   component->splice_time->pts_time != 0 &&
                   PID == component->component_tag)
               {
                  PTS = component->splice_time->pts_time;
                  LOG_INFO_ARGS ("getSCTE35PTS: component: PTS = %"PRId64"", PTS)
                  break;
               }
            }
         }     
      }
   }
   else if (scte35InfoSection->splice_command_type == SCTE35_TIME_SIGNAL_CMD)
   {
      scte35_time_signal *timeSignalCmd = (scte35_time_signal *)(scte35InfoSection->splice_command);
      if (timeSignalCmd->splice_time->time_specified_flag)
      {
         PTS = timeSignalCmd->splice_time->pts_time;
      }
   }

   return PTS;
}

void addSCTE35Point (varray_t* scte35List, scte35_splice_info_section *scte35InfoSection, int threadNum, int partitionID, uint32_t PID)
{
   // walk list and compare scte35_splice_info_section
   // if scte35_splice_info_section is a splice_insert, check cancel flag
   // also compare eventID, and replace if eventID is a dup
   // if time_signal, compare PTS
   // if PTS equal to list member, no action necessary
   // else insert PTS in ordered list

   int insertComplete = 0;

   LOG_INFO_ARGS("IngestThread %d: Adding SCTE35 for partition %d: PID %d", 
      threadNum, partitionID, PID);
   reportAddInfoLogArgs("IngestThread %d: Adding SCTE35 for partition %d: PID %d", 
      threadNum, partitionID, PID);

   int isSpliceInsertCmd = 0;
   int isTimeSignalCmd = 0;
   scte35_splice_insert *spliceInsertCmd = NULL;
   uint64_t PTS = 0;
   if (scte35InfoSection->splice_command_type == SCTE35_SPLICE_INSERT_CMD)
   {
      isSpliceInsertCmd = 1;
      spliceInsertCmd = (scte35_splice_insert *)(scte35InfoSection->splice_command);
   }
   else if (scte35InfoSection->splice_command_type == SCTE35_TIME_SIGNAL_CMD)
   {
      isTimeSignalCmd = 1;
      scte35_time_signal *timeSignalCmd = (scte35_time_signal *)(scte35InfoSection->splice_command);
      if (timeSignalCmd->splice_time->time_specified_flag)
      {
         PTS = timeSignalCmd->splice_time->pts_time;
      }
      else
      {
         return;
      }
   }

   for (int i=0; i<varray_length(scte35List); i++)
   {
      scte35_splice_info_section *scte35InfoSectionTemp = (scte35_splice_info_section *) varray_get (scte35List, i);
      if (isSpliceInsertCmd && scte35InfoSectionTemp->splice_command_type == SCTE35_SPLICE_INSERT_CMD)
      {
         scte35_splice_insert *spliceInsertCmdTemp = (scte35_splice_insert *)(scte35InfoSectionTemp->splice_command);
         if (spliceInsertCmd->splice_event_id == spliceInsertCmdTemp->splice_event_id)
         {
            if (spliceInsertCmd->splice_event_cancel_indicator)
            {
               // delete event
               varray_remove (scte35List, i);
               LOG_INFO_ARGS("IngestThread %d: SCTE35 EventID %d for partition %d: PID %d cancelled -- removing", 
                  threadNum, spliceInsertCmd->splice_event_id, partitionID, PID);
            }
            else
            {
               // replace element
               scte35_splice_info_section* scte35InfoSectionTemp2 = scte35_splice_info_section_copy(scte35InfoSection);
               varray_set (scte35List, i, scte35InfoSectionTemp2);
               LOG_INFO_ARGS("IngestThread %d: SCTE35 EventID %d for partition %d: PID %d already present -- replacing", 
                  threadNum, spliceInsertCmd->splice_event_id, partitionID, PID);
            }

            scte35_splice_info_section_free(scte35InfoSectionTemp); 
            insertComplete = 1;
            break;
         }
      }
      else if (isTimeSignalCmd && scte35InfoSectionTemp->splice_command_type == SCTE35_TIME_SIGNAL_CMD)
      {
         scte35_time_signal *timeSignalCmdTemp = (scte35_time_signal *)(scte35InfoSectionTemp->splice_command);
         // NOTE: time_specified_flag should already have been checked when this event was added to the list
         uint64_t PTSTemp = timeSignalCmdTemp->splice_time->pts_time + scte35InfoSection->pts_adjustment;

         if (PTS == PTSTemp)
         {
            // do nothing
            insertComplete = 1;
            break;
         }
         if (PTS < PTSTemp)
         {
            // insert into SCTEList
            scte35_splice_info_section* scte35InfoSectionTemp2 = scte35_splice_info_section_copy(scte35InfoSection);
            varray_insert(scte35List, i, scte35InfoSectionTemp2);

            insertComplete = 1;
            break;
         }
      }      
   }

   if (!insertComplete)
   {
      // add to end of SCTEList
      scte35_splice_info_section* scte35InfoSectionTemp2 = scte35_splice_info_section_copy(scte35InfoSection);
      varray_add(scte35List, scte35InfoSectionTemp2);
   }
}

int checkPTSAgainstSCTE35Points (varray_t* scte35List, uint64_t PTS, uint64_t deltaSCTE35PTS, int threadNum,
   int partitionID, uint32_t PID, hashtable_t *mapOldSCTE35SpliceInserts)
{
   // check if PTS has passed entries in SCTE35 list
   LOG_INFO_ARGS("IngestThread %d: checkPTSAgainstSCTE35Points: PTS = %"PRId64" for partition %d: PID %d, deltaSCTE35PTS = %"PRId64"", 
      threadNum, PTS, partitionID, PID, deltaSCTE35PTS);

   int nReturnCode = 0;

   LOG_INFO_ARGS ("IngestThread %d: checkPTSAgainstSCTE35Points: varray_length(scte35List) = %d", 
      threadNum, varray_length(scte35List));
   for (int i=0; i<varray_length(scte35List); )
   {
      scte35_splice_info_section *scte35InfoSection = (scte35_splice_info_section *) varray_get (scte35List, i);

      scte35_splice_info_section_print_stdout(scte35InfoSection);
      uint64_t SCTE35PTS = getSCTE35PTS (scte35InfoSection, PID);
      LOG_INFO_ARGS("IngestThread %d: checkPTSAgainstSCTE35Points: SCTE35PTS = %"PRId64"", 
         threadNum, SCTE35PTS);

      if (PTS > SCTE35PTS + deltaSCTE35PTS)
      {
         // SCTE35 point has been passed without an EBP being present, so log error and discard SCTE35 PTS

         LOG_ERROR_ARGS("IngestThread %d: FAIL: Out of date SCTE35 PTS %"PRId64" detected for partition %d: PID %d", 
            threadNum, SCTE35PTS, partitionID, PID);
         reportAddErrorLogArgs("IngestThread %d: FAIL: Out of date SCTE35 PTS %"PRId64" detected for partition %d: PID %d", 
            threadNum, SCTE35PTS, partitionID, PID);

         // add to map of oldSCTE35
         if (is_splice_insert(scte35InfoSection))
         {
            uint32_t tmpEventId = get_splice_insert_eventID (scte35InfoSection);
            // check if this eventID has already been added
            LOG_INFO_ARGS("IngestThread %d: Checking to see if event ID %d is already in hashtable", 
               threadNum, tmpEventId);
            LOG_INFO_ARGS("IngestThread %d: hashtable count = %d", 
               threadNum, hashtable_count(mapOldSCTE35SpliceInserts));
            
            if (hashtable_search(mapOldSCTE35SpliceInserts, &tmpEventId) == NULL)
            {
               LOG_INFO_ARGS("IngestThread %d: Event ID %d NOT already in hashtable", 
                  threadNum, tmpEventId);
               uint64_t* myPTS = (uint64_t*) calloc (1, sizeof (uint64_t));
               *myPTS = scte35_get_latest_PTS (scte35InfoSection);

               uint32_t* myEventId = (uint32_t*) calloc (1, sizeof (uint32_t));
               *myEventId = get_splice_insert_eventID (scte35InfoSection);

               LOG_INFO_ARGS("IngestThread %d: Adding event ID %d to hashtable (PTS = %"PRId64")", 
                  threadNum, *myEventId, *myPTS);
               hashtable_insert (mapOldSCTE35SpliceInserts, myEventId, myPTS);
               LOG_INFO_ARGS("IngestThread %d: Done adding event ID %d to hashtable (PTS = %"PRId64")", 
                  threadNum, *myEventId, *myPTS);
            }
            else
            {
               LOG_INFO_ARGS("IngestThread %d: Event ID %d IS already in hashtable", 
                  threadNum, tmpEventId);
            }
         }
         else
         {
            LOG_INFO_ARGS("IngestThread %d: SCTE35 msg is not a splice insert", 
                  threadNum);
         }

         varray_remove (scte35List, i);
         scte35_splice_info_section_free (scte35InfoSection);

         // careful here -- if we have just removed the current element, dont increment i

         // this signals test failure to calling code
         nReturnCode = -1;
      }
      else
      {
         i++;
      }
   }

   LOG_INFO_ARGS("IngestThread %d: checkPTSAgainstSCTE35Points returning", 
     threadNum);
   return nReturnCode;
}

int checkEBPAgainstSCTE35Points (varray_t* scte35List, uint64_t PTS, uint64_t deltaSCTE35PTS, int threadNum,
   int partitionID, uint32_t PID, hashtable_t *mapOldSCTE35SpliceInserts)
{
   LOG_INFO ("checkEBPAgainstSCTE35Points -- entering");
   int returnCode = 0;
   // check if PTS is sufficiently near the SCTE35 points

//   LOG_INFO_ARGS("IngestThread %d: checkEBPAgainstSCTE35Points: PTS = %"PRId64" for partition %d: PID %d, deltaSCTE35PTS = %"PRId64"", 
//      threadNum, PTS, partitionID, PID, deltaSCTE35PTS);

   for (int i=0; i<varray_length(scte35List); )
   {
      scte35_splice_info_section *scte35InfoSection = (scte35_splice_info_section *) varray_get (scte35List, i);
//      LOG_INFO_ARGS("IngestThread %d: checkEBPAgainstSCTE35Points: *SCTE35PTS = %"PRId64"", 
//         threadNum, *SCTE35PTS);

      uint64_t SCTE35PTS = getSCTE35PTS (scte35InfoSection, PID);

      if (PTS >= SCTE35PTS - deltaSCTE35PTS && PTS <= SCTE35PTS + deltaSCTE35PTS)
      {
         // match -- remove entry from SCTE35 list

         LOG_INFO_ARGS("IngestThread %d: PTS %"PRId64" matches SCTE35 PTS %"PRId64" for partition %d: PID %d", 
            threadNum, PTS, SCTE35PTS, partitionID, PID);
         reportAddInfoLogArgs("IngestThread %d: PTS %"PRId64" matches SCTE35 PTS %"PRId64" for partition %d: PID %d", 
            threadNum, PTS, SCTE35PTS, partitionID, PID);

         // add to map of oldSCTE35
         if (is_splice_insert(scte35InfoSection))
         {
            LOG_INFO_ARGS("IngestThread %d: checking mapOldSCTE35SpliceInserts...", 
               threadNum);
            uint32_t tmpEventId = get_splice_insert_eventID (scte35InfoSection);
            if (hashtable_search(mapOldSCTE35SpliceInserts, &tmpEventId) == NULL)
            {
               uint64_t* myPTS = (uint64_t*) calloc (1, sizeof (uint64_t));
               *myPTS = scte35_get_latest_PTS (scte35InfoSection);

               uint32_t* myEventId = (uint32_t*) calloc (1, sizeof (uint32_t));
               *myEventId = get_splice_insert_eventID (scte35InfoSection);

               LOG_INFO_ARGS("IngestThread %d: adding SCTE35 to mapOldSCTE35SpliceInserts, eventID = %d", 
                  threadNum, *myEventId);
               hashtable_insert (mapOldSCTE35SpliceInserts, myEventId, myPTS);
            }
         }

         varray_remove (scte35List, i);
         scte35_splice_info_section_free (scte35InfoSection);

         // careful here -- if we have just removed the current element, dont increment i

         returnCode = 1;
         break;  // only allow one matched SCTE35 at a time
      }
      else
      {
         i++;
      }
   }

   LOG_INFO ("checkEBPAgainstSCTE35Points -- exiting");
   return returnCode;
}

void addSCTE35Point_AllBoundaries (int threadNum, ebp_stream_info_t *streamInfo, scte35_splice_info_section *scte35InfoSection)
{
   LOG_INFO ("addSCTE35Point_AllBoundaries -- entering");
   ebp_boundary_info_t *ebpBoundaryInfo = streamInfo->ebpBoundaryInfo;

   if (streamInfo->fifo == NULL)
   {
      // this elementary stream is not handled for this file/ingest
      return;
   }

   for (int i=0; i<EBP_NUM_PARTITIONS; i++)
   {
      if (!ebpBoundaryInfo[i].isBoundary)
      {
         continue;
      }

      if (ebpBoundaryInfo[i].listSCTE35 == NULL)
      {
         ebpBoundaryInfo[i].listSCTE35 = varray_new();
      }
      addSCTE35Point (ebpBoundaryInfo[i].listSCTE35, scte35InfoSection, threadNum, i /* partitionID */, streamInfo->PID);
   }
   LOG_INFO ("addSCTE35Point_AllBoundaries -- exiting");
}

void checkPTSAgainstSCTE35Points_AllBoundaries (int threadNum, ebp_stream_info_t *streamInfo, uint64_t PTS,
                                                hashtable_t *mapOldSCTE35SpliceInserts)
{
   LOG_INFO ("checkPTSAgainstSCTE35Points_AllBoundaries -- entering");
   ebp_boundary_info_t *ebpBoundaryInfo = streamInfo->ebpBoundaryInfo;

   for (int i=0; i<EBP_NUM_PARTITIONS; i++)
   {
      if (ebpBoundaryInfo[i].listSCTE35 == NULL)
      {
         continue;
      }  
      
      if (checkPTSAgainstSCTE35Points (ebpBoundaryInfo[i].listSCTE35, PTS, g_ATSTestAppConfig.ebpSCTE35PTSJitterSecs * 90000, 
         threadNum, i /* partitionID */, streamInfo->PID, mapOldSCTE35SpliceInserts) != 0)
      {
         streamInfo->streamPassFail = 0;
      }
   }
   LOG_INFO ("checkPTSAgainstSCTE35Points_AllBoundaries -- exiting");
}


void pruneOldSCTE35Map(hashtable_t *mapOldSCTE35SpliceInserts, uint64_t currentPTS)
{
   void** keyArray;
   int keyArraySz = 0;
   printf ("pruneOldSCTE35Map -- entering (printf)");
   LOG_INFO ("pruneOldSCTE35Map -- entering");
   hashtable_get_key_array(mapOldSCTE35SpliceInserts, &keyArray, &keyArraySz);

   for (int i=0; i<keyArraySz; i++)
   {
      void * value = hashtable_search(mapOldSCTE35SpliceInserts, keyArray[i]);
      uint64_t *scte35PTS = (uint64_t *) value;
      LOG_INFO_ARGS ("pruneOldSCTE35Map (%d) -- key = %d PTS = %"PRId64"", i, *((uint32_t*)keyArray[i]), *scte35PTS);

      if (currentPTS > *scte35PTS + g_ATSTestAppConfig.scte35SpliceEventTimeToLiveSecs * 90000)
      {
         LOG_INFO_ARGS ("pruneOldSCTE35Map (%d) -- removing key %d", i, *((uint32_t*)keyArray[i]));
         hashtable_remove(mapOldSCTE35SpliceInserts, keyArray[i]);
      }
   }

   LOG_INFO ("pruneOldSCTE35Map -- freeing key Array");
   free (keyArray);
   LOG_INFO ("pruneOldSCTE35Map -- exiting");
}

