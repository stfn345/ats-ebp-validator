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
#include <time.h>
#include <string.h>
#include <stdio.h>

#include <inttypes.h>
#include <tpes.h>


#include "ATSTestReport.h"
#include "varray.h"

static char *g_reportBaseName = "EBPToolReport_";

static varray_t* g_listBPInfos;
static varray_t* g_listErrorMsgs;
static varray_t* g_listInfoMsgs;

int reportGet2DArrayIndex (int fileIndex, int streamIndex, int numStreams)
{
   return fileIndex * numStreams + streamIndex;
}


void reportInit()
{
   g_listBPInfos = varray_new();
   g_listErrorMsgs = varray_new();
   g_listInfoMsgs = varray_new();
}

void reportClearData(int numIngests, int numStreams, ebp_stream_info_t **streamInfoArray, int *filePassFails)
{
   // walk list of PTS and free each, then empty list
   for (int i=0; i<varray_length(g_listBPInfos); i++)
   {
      bp_info_t *tmp = (bp_info_t *) varray_get(g_listBPInfos, i);
      free (tmp);
   }
   varray_clear(g_listBPInfos);

   for (int i=0; i<varray_length(g_listInfoMsgs); i++)
   // walk list of error msgs and free each then empty list
   {
      char *tmp = (char*) varray_get(g_listInfoMsgs, i);
      free (tmp);
   }
   varray_clear(g_listInfoMsgs);

   for (int i=0; i<varray_length(g_listErrorMsgs); i++)
   // walk list of error msgs and free each then empty list
   {
      char *tmp = (char*) varray_get(g_listErrorMsgs, i);
      free (tmp);
   }
   varray_clear(g_listErrorMsgs);

   // clear pass/fail flags
   for (int i=0; i<numIngests; i++)
   {
      filePassFails[i] = 1;
      for (int j=0; j<numStreams; j++)
      {
         int arrayIndex = reportGet2DArrayIndex (i, j, numStreams);
         ebp_stream_info_t *streamInfo = streamInfoArray[arrayIndex];

         if (streamInfo == NULL)
         {
            // if stream is absent from this file
            continue;
         }

         streamInfo->streamPassFail = 1;
      }
   }
}

void reportCleanup()
{
   reportClearData(0, 0, NULL, NULL);
   varray_free(g_listBPInfos);
   varray_free(g_listErrorMsgs);
   varray_free(g_listInfoMsgs);
}

void reportAddPTS (int64_t PTS, uint8_t partitionId, uint8_t ingestId, uint8_t streamId, uint32_t PID)
{
   bp_info_t * bpInfo = (bp_info_t *)malloc (sizeof(bp_info_t));
   bpInfo->PTS = PTS;
   bpInfo->partitionId = partitionId;
   bpInfo->ingestId = ingestId;
   bpInfo->streamId = streamId;
   bpInfo->PID = PID;

   varray_add(g_listBPInfos, bpInfo);
}

void reportAddInfoLog (char *infoMsg)
{
   char *temp = (char *) malloc (strlen(infoMsg) + 1);
   strcpy(temp, infoMsg);
   varray_add(g_listInfoMsgs, temp);
}

void reportAddErrorLog (char *errorMsg)
{
   char *temp = (char *) malloc (strlen(errorMsg) + 1);
   strcpy(temp, errorMsg);
   varray_add(g_listErrorMsgs, temp);
}

char *reportPrint(int numIngests, int numStreams, ebp_stream_info_t **streamInfoArray, char **ingestNames, int *filePassFails,
                  program_stream_info_t *programStreamInfo)
{
   char reportPath[2048];
   if (getcwd(reportPath, 1024) == NULL)
   {
      return NULL;
   }

   time_t now;
   struct tm * nowStruct;

   time (&now);
   nowStruct = localtime (&now);
   char timestamp[20];
   sprintf (timestamp, "%02d%02d%02d_%02d%02d%02d", nowStruct->tm_mon + 1, nowStruct->tm_mday, nowStruct->tm_year,
      nowStruct->tm_hour, nowStruct->tm_min, nowStruct->tm_sec);

   strcat (reportPath, "/");
   strcat (reportPath, g_reportBaseName);
   strcat (reportPath, timestamp);
   strcat (reportPath, ".txt");

   FILE *myFile = fopen (reportPath, "w");
   if (myFile == NULL)
   {
      return NULL;
   }
   fprintf (myFile, "EBP Conformance Test Report\n");

   reportPrintStreamInfo(myFile, numIngests, numStreams, streamInfoArray, ingestNames, programStreamInfo);

   fprintf (myFile, "\nERROR Msgs:\n");
   for (int i=0; i<varray_length(g_listErrorMsgs); i++)
   {
      char *tmp = (char*) varray_get(g_listErrorMsgs, i);
      fprintf (myFile, "%s\n", tmp);
   }

   fprintf (myFile, "\nINFO Msgs:\n");
   for (int i=0; i<varray_length(g_listInfoMsgs); i++)
   {
      char *tmp = (char*) varray_get(g_listInfoMsgs, i);
      fprintf (myFile, "%s\n", tmp);
   }


   // char* pts_dts_to_string(uint64_t pts_dts, char inout[13]);

   fprintf (myFile, "\nBoundary Points:\n");
   char ptsString[13];
   for (int i=0; i<varray_length(g_listBPInfos); i++)
   {
      bp_info_t *tmp = (bp_info_t *) varray_get(g_listBPInfos, i);
      fprintf (myFile, "ingest #%d, stream #%d (PID %d), partition #%d, PTS = %"PRId64" (%s) \n", 
         tmp->ingestId, tmp->streamId, tmp->PID, tmp->partitionId, tmp->PTS, 
         pts_dts_to_string(tmp->PTS, ptsString));
   }


   fprintf (myFile, "\n");
   fprintf (myFile, "\n");
   fprintf (myFile, "TEST RESULTS\n");
   fprintf (myFile, "\n");

   for (int i=0; i<numIngests; i++)
   {
      fprintf (myFile, "Input %s\n", ingestNames[i]);
      int overallPassFail = filePassFails[i];
      for (int j=0; j<numStreams; j++)
      {
         int arrayIndex = reportGet2DArrayIndex (i, j, numStreams);
         ebp_stream_info_t *streamInfo = streamInfoArray[arrayIndex];
         if (streamInfo == NULL)
         {
            // if stream is absent from this file
            continue;
         }

         overallPassFail &= streamInfo->streamPassFail;
      }

      fprintf (myFile, "   Overall PassFail Result: %s\n", (overallPassFail?"PASS":"FAIL"));
      fprintf (myFile, "   Stream PassFail Results:\n");
      for (int j=0; j<numStreams; j++)
      {
         int arrayIndex = reportGet2DArrayIndex (i, j, numStreams);
         ebp_stream_info_t *streamInfo = streamInfoArray[arrayIndex];

         if (streamInfo == NULL)
         {
            // if stream is absent from this file
            continue;
         }

         fprintf (myFile, "      PID %d (%s): %s\n", streamInfo->PID, (streamInfo->isVideo?"VIDEO":"AUDIO"),
            (streamInfo->streamPassFail?"PASS":"FAIL"));

         reportPrintBoundaryInfoArray(myFile, streamInfo->ebpBoundaryInfo);
      }
      fprintf (myFile, "\n");
   }

   fprintf (myFile, "TEST RESULTS END\n");

   fprintf (myFile, "\n");
   fclose (myFile);


   char * reportPathTmp = (char *) malloc (strlen(reportPath) + 1);
   strcpy (reportPathTmp, reportPath);
   return reportPathTmp;
}

void reportAddInfoLogArgs (const char *fmt, ...)
{
   va_list args;

   char *temp = (char *) malloc (1024);

   va_start (args, fmt);
   vsprintf (temp, fmt, args);
   va_end (args);

   varray_add(g_listInfoMsgs, temp);
}

void reportAddErrorLogArgs (const char *fmt, ...)
{
   va_list args;

   char *temp = (char *) malloc (1024);

   va_start (args, fmt);
   vsprintf (temp, fmt, args);
   va_end (args);

   varray_add(g_listErrorMsgs, temp);
}

void reportPrintBoundaryInfoArray(FILE *reportFile, ebp_boundary_info_t *boundaryInfoArray)
{
   fprintf (reportFile, "      EBP Boundary Info:\n");

   for (int i=0; i<EBP_NUM_PARTITIONS; i++)
   {
      if (boundaryInfoArray[i].isBoundary)
      {
         if (boundaryInfoArray[i].isImplicit)
         {
            fprintf (reportFile, "         PARTITION %d: IMPLICIT, PID = %d, FileIndex = %d\n", i, boundaryInfoArray[i].implicitPID, 
               boundaryInfoArray[i].implicitFileIndex);
         }
         else
         {
            fprintf (reportFile, "         PARTITION %d: EXPLICIT\n", i);
         }
      }
   }
}

void reportPrintStreamInfo(FILE *reportFile, int numIngests, int numStreams, ebp_stream_info_t **streamInfoArray, char **ingestNames,
                           program_stream_info_t *programStreamInfo)
{
   fprintf (reportFile, "\n");
   fprintf (reportFile, "\n");

   for (int i=0; i<numIngests; i++)
   {
      fprintf (reportFile, "Input %s\n", ingestNames[i]);
      fprintf (reportFile, "   Stream:\n");
      for (int j=0; j<numStreams; j++)
      {
         int arrayIndex = i * numStreams + j;
         ebp_stream_info_t *streamInfo = streamInfoArray[arrayIndex];

         if (streamInfo == NULL)
         {
            // if stream is absent from this file
            continue;
         }

         fprintf (reportFile, "      PID %d (%s)\n", streamInfo->PID, (streamInfo->isVideo?"VIDEO":"AUDIO"));

         reportPrintBoundaryInfoArray(reportFile, streamInfo->ebpBoundaryInfo);
         
         ebp_descriptor_t* ebpDescriptor = getEBPDescriptorFromProgramStreamStruct (&(programStreamInfo[i]), streamInfo->PID);
         if (ebpDescriptor != NULL)
         {
            reportPrintEBPDescriptor(reportFile, ebpDescriptor);
         }
         else
         {
            fprintf (reportFile, "\n         EBP Descriptor not Found\n");
         }

         ebp_t* ebp = getEBPFromProgramStreamStruct (&(programStreamInfo[i]), streamInfo->PID);
         if (ebp != NULL)
         {
            reportPrintEBPStruct(reportFile, ebp);
         }
         else
         {
            if (ebpDescriptor != NULL)
            {
               fprintf (reportFile, "         EBP Struct search not performed\n");
            }
            else
            {
               fprintf (reportFile, "         EBP Struct not found\n");
            }
         }

         fprintf (reportFile, "\n");


      }
      fprintf (reportFile, "\n");
   }
}

void reportPrintEBPDescriptor(FILE *reportFile, const ebp_descriptor_t *ebp_desc)
{
   fprintf (reportFile, "\n         EBP Descriptor:\n");
   fprintf (reportFile, "            tag = %d, length = %d, timescale_flag = %d, ticks_per_second = %d, ebp_distance_width_minus_1 = %d\n", 
      ebp_desc->descriptor.tag, ebp_desc->descriptor.length, 
      ebp_desc->timescale_flag, ebp_desc->ticks_per_second, ebp_desc->ebp_distance_width_minus_1);

   int num_partitions = 0;
   if (ebp_desc->partition_data != NULL)
   {
      num_partitions = vqarray_length(ebp_desc->partition_data);
      fprintf (reportFile, "            num_partitions = %d\n", num_partitions);

      for (int i=0; i<num_partitions; i++)
      {
         ebp_partition_data_t* partition = (ebp_partition_data_t*)vqarray_get(ebp_desc->partition_data, i);

         fprintf (reportFile, "            partition[%d]:\n", i);
         fprintf (reportFile, "               ebp_data_explicit_flag = %d, representation_id_flag = %d, partition_id = %d, ebp_pid = %d\n", 
            partition->ebp_data_explicit_flag, partition->representation_id_flag, partition->partition_id,
            partition->ebp_pid);
         fprintf (reportFile, "               boundary_flag = %d, ebp_distance = %d, sap_type_max = %d, acquisition_time_flag = %d, representation_id = %"PRId64"\n", 
            partition->boundary_flag, partition->ebp_distance, partition->sap_type_max, 
            partition->acquisition_time_flag, partition->representation_id);
      }
   }
   else
   {
      fprintf (reportFile, "            num_partitions = %d:\n", num_partitions);
   }
}

void reportPrintEBPStruct(FILE *reportFile, const ebp_t *ebp)
{
    fprintf (reportFile, "         EBP struct:\n");
    fprintf (reportFile, "            ebp_fragment_flag = %d\n", ebp->ebp_fragment_flag);
    fprintf (reportFile, "            ebp_segment_flag = %d\n", ebp->ebp_segment_flag);
    fprintf (reportFile, "            ebp_sap_flag = %d\n", ebp->ebp_sap_flag);
    fprintf (reportFile, "            ebp_grouping_flag = %d\n", ebp->ebp_grouping_flag);
    fprintf (reportFile, "            ebp_time_flag = %d\n", ebp->ebp_time_flag);
    fprintf (reportFile, "            ebp_concealment_flag = %d\n", ebp->ebp_concealment_flag);
    fprintf (reportFile, "            ebp_extension_flag = %d\n", ebp->ebp_extension_flag);
    fprintf (reportFile, "            ebp_ext_partition_flag = %d\n", ebp->ebp_ext_partition_flag);
    fprintf (reportFile, "            ebp_sap_type = %d\n", ebp->ebp_sap_type);
    
    int num_ebp_grouping_ids = 0;
    if ((uint32_t)ebp->ebp_grouping_ids != 0)
    {
        fprintf (reportFile, "            ebp_grouping_ids = %x\n", (uint32_t)ebp->ebp_grouping_ids);
        num_ebp_grouping_ids = vqarray_length(ebp->ebp_grouping_ids);
        fprintf (reportFile, "            num_ebp_grouping_ids = %d\n", num_ebp_grouping_ids);
        for (int i=0; i<num_ebp_grouping_ids; i++)
        {
            fprintf (reportFile, "                ebp_grouping_ids[%d] = %d\n", i, *((uint32_t*)vqarray_get(ebp->ebp_grouping_ids, i)));
        }
    }
    else
    {
        fprintf (reportFile, "            num_ebp_grouping_ids = %d\n", num_ebp_grouping_ids);
    }

    fprintf (reportFile, "            ebp_acquisition_time = %"PRId64"\n", ebp->ebp_acquisition_time);
    fprintf (reportFile, "            ebp_ext_partitions = %d\n", ebp->ebp_ext_partitions);
}

ebp_t* getEBPFromProgramStreamStruct (program_stream_info_t *programStreamInfo, uint32_t PID)
{
   for (int i=0; i<programStreamInfo->numStreams; i++)
   {
      if ((programStreamInfo->PIDs)[i] == PID)
      {
         return (programStreamInfo->ebps)[i];
      }
   }

   return NULL;
}

ebp_descriptor_t* getEBPDescriptorFromProgramStreamStruct (program_stream_info_t *programStreamInfo, uint32_t PID)
{
   for (int i=0; i<programStreamInfo->numStreams; i++)
   {
      if ((programStreamInfo->PIDs)[i] == PID)
      {
         return (programStreamInfo->ebpDescriptors)[i];
      }
   }

   return NULL;
}

