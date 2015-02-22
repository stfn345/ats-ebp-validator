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


#include "ATSTestReport.h"
#include "varray.h"

static char *g_reportBaseName = "EBPToolReport_";

static varray_t* g_listBPInfos;
static varray_t* g_listErrorMsgs;
static varray_t* g_listInfoMsgs;


void reportInit()
{
   g_listBPInfos = varray_new();
   g_listErrorMsgs = varray_new();
   g_listInfoMsgs = varray_new();
}

void reportClearData()
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
}

void reportCleanup()
{
   reportClearData();
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

char *reportPrint(int numIngests, int numStreams, ebp_stream_info_t **streamInfoArray, char **ingestNames)
{
   // GORP: add overall pass/fail

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

   reportPrintStreamInfo(myFile, numIngests, numStreams, streamInfoArray, ingestNames);

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

   fprintf (myFile, "\nBoundary Points:\n");
   for (int i=0; i<varray_length(g_listBPInfos); i++)
   {
      bp_info_t *tmp = (bp_info_t *) varray_get(g_listBPInfos, i);
      fprintf (myFile, "ingest #%d, stream #%d (PID %d), partition #%d, PTS = %"PRId64"\n", 
         tmp->ingestId, tmp->streamId, tmp->PID, tmp->partitionId, tmp->PTS);
   }


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

void reportPrintStreamInfo(FILE *reportFile, int numIngests, int numStreams, ebp_stream_info_t **streamInfoArray, char **ingestNames)
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
      }
      fprintf (reportFile, "\n");

   }

   fprintf (reportFile, "\n");
}

