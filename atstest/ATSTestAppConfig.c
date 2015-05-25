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
#include <ctype.h>

#include "log.h"
#include "ATSTestAppConfig.h"
#include "ATSTestReport.h"

ats_test_app_config_t g_ATSTestAppConfig;


void printTestConfig()
{
   LOG_INFO ("ATS Test Config:");
   LOG_INFO_ARGS ("     logFilePath = %s", g_ATSTestAppConfig.logFilePath);
   LOG_INFO_ARGS ("     ebpPrereadSearchTimeMsecs = %d", g_ATSTestAppConfig.ebpPrereadSearchTimeMsecs);
   LOG_INFO_ARGS ("     ebpAllowedPTSJitterSecs = %f", g_ATSTestAppConfig.ebpAllowedPTSJitterSecs);
   LOG_INFO_ARGS ("     ebpSCTE35PTSJitterSecs = %f", g_ATSTestAppConfig.ebpSCTE35PTSJitterSecs);
   LOG_INFO_ARGS ("     scte35MinimumPrerollSeconds = %f", g_ATSTestAppConfig.scte35MinimumPrerollSeconds);
   LOG_INFO_ARGS ("     scte35SpliceEventTimeToLiveSecs = %f", g_ATSTestAppConfig.scte35SpliceEventTimeToLiveSecs);
   LOG_INFO_ARGS ("     socketRcvBufferSz = %d", g_ATSTestAppConfig.socketRcvBufferSz);
   LOG_INFO_ARGS ("     ingestCircularBufferSz = %d", g_ATSTestAppConfig.ingestCircularBufferSz);
   LOG_INFO_ARGS ("     logLevel = %d", g_ATSTestAppConfig.logLevel);
}

void setTestConfigDefaults()
{
   g_ATSTestAppConfig.logFilePath = (char *) calloc (1, 2048);
   if (getcwd(g_ATSTestAppConfig.logFilePath, 1024) == NULL)
   {
      LOG_ERROR("ATSTestAppConfig:setTestConfigDefaults: Error getting cwd");
      reportAddErrorLog("ATSTestAppConfig:setTestConfigDefaults: Error getting cwd");
      
      strcpy (g_ATSTestAppConfig.logFilePath, "EBPTestLog.txt");
   }
   else
   {
      strcat (g_ATSTestAppConfig.logFilePath, "/EBPTestLog.txt");
   }


   g_ATSTestAppConfig.ebpPrereadSearchTimeMsecs = 10000000;
   g_ATSTestAppConfig.ebpAllowedPTSJitterSecs = 2.0;
   g_ATSTestAppConfig.ebpSCTE35PTSJitterSecs = 2.0;

   g_ATSTestAppConfig.scte35MinimumPrerollSeconds = 10.0;
   g_ATSTestAppConfig.scte35SpliceEventTimeToLiveSecs = 10.0;

   g_ATSTestAppConfig.socketRcvBufferSz = 2000000;
   g_ATSTestAppConfig.ingestCircularBufferSz = 1880000;
   g_ATSTestAppConfig.logLevel = 3;

}


int initTestConfig()
{
   setTestConfigDefaults();
   int returnCode = readTestConfigFile();
   if (returnCode == 0)
   {
      printTestConfig();
   }

   return returnCode;
}

int readTestConfigFile()
{
   char configFilePath[2048];
   if (getcwd(configFilePath, 1024) == NULL)
   {
      LOG_ERROR("ATSTestAppConfig: Error getting cwd");
      reportAddErrorLog("ATSTestAppConfig: Error getting cwd");
      return -1;
   }

   strcat (configFilePath, "/ATSTestApp.props");

   FILE *configFile = fopen (configFilePath, "rt");
   if (configFile == NULL)
   {
      //LOG_ERROR_ARGS("ATSTestAppConfig: Error opening config file %s", configFilePath);
      //reportAddErrorLogArgs("ATSTestAppConfig: Error opening config file %s", configFilePath);
      return -1;
   }

   char line[256];
   char name[256];
   char value[256];
   char *nameTrimmed = NULL;
   char *valueTrimmed = NULL;

   while(fgets(line, 256, configFile) != NULL)
   {         
 //     printf ("line = %s\n", line);
      char *temp = strchr(line, '='); // Find first occurrence of character c in string. 
      if (temp != NULL)
      {
         *temp = 0;  // split the string into two
         strcpy (name, line);
         strcpy (value, temp + 1);

         nameTrimmed = trimWhitespace(name);
         valueTrimmed = trimWhitespace(value);
 //        printf ("nameTrimmed = %s, valueTrimmed = %s\n", nameTrimmed, valueTrimmed);

         if (strcmp("logFilePath", nameTrimmed) == 0)
         {
            // logFilePath mem is already allocated
            strcpy (g_ATSTestAppConfig.logFilePath, valueTrimmed);
         }
         else if (strcmp("logLevel", nameTrimmed) == 0)
         {
            g_ATSTestAppConfig.logLevel = atoi (valueTrimmed);
         }
         else if (strcmp("ebpPrereadSearchTimeMsecs", nameTrimmed) == 0)
         {
            g_ATSTestAppConfig.ebpPrereadSearchTimeMsecs = strtoul (valueTrimmed, NULL, 10);
         }
         else if (strcmp("ebpAllowedPTSJitterSecs", nameTrimmed) == 0)
         {
            g_ATSTestAppConfig.ebpAllowedPTSJitterSecs = atof (valueTrimmed);
         }
         else if (strcmp("ebpSCTE35PTSJitterSecs", nameTrimmed) == 0)
         {
            g_ATSTestAppConfig.ebpSCTE35PTSJitterSecs = atof (valueTrimmed);
         }
         else if (strcmp("scte35MinimumPrerollSeconds", nameTrimmed) == 0)
         {
            g_ATSTestAppConfig.scte35MinimumPrerollSeconds = atof (valueTrimmed);
         }
         else if (strcmp("scte35SpliceEventTimeToLiveSecs", nameTrimmed) == 0)
         {
            g_ATSTestAppConfig.scte35SpliceEventTimeToLiveSecs = atof (valueTrimmed);
         }
         else if (strcmp("socketRcvBufferSz", nameTrimmed) == 0)
         {
            g_ATSTestAppConfig.socketRcvBufferSz = atoi (valueTrimmed);
         }
         else if (strcmp("ingestCircularBufferSz", nameTrimmed) == 0)
         {
            g_ATSTestAppConfig.ingestCircularBufferSz = atoi (valueTrimmed);
         }
         else
         {
            LOG_INFO_ARGS ("Unknown configuration property %s ignored", nameTrimmed);
         }
      }
   }

   fclose(configFile);

   return 0;
}

char *trimWhitespace(char *string)
{
   int numLeadingWhitespace = 0;
   int numTrailingWhitespace = 0;

   int i=0;
   while (1)
   {
      if (string[i] == 0 || isspace((int)(string[i])) == 0)
      {
         break;
      }
        
      i++;
   }
   numLeadingWhitespace = i;

   i=strlen(string) - 1;
   while (1)
   {
      if (i < 0 || isspace((int)(string[i])) == 0)
      {
         break;
      }
        
      string[i] = 0;
      numTrailingWhitespace++;
      i--;
   }

   return (string + numLeadingWhitespace);
}
