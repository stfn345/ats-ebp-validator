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
#include <string.h>
#include "log.h"


int ATS_TEST_CASE_AUDIO_PTS_GAP                    = 0;
int ATS_TEST_CASE_AUDIO_PTS_OVERLAP                = 0;
int ATS_TEST_CASE_VIDEO_PTS_GAP                    = 0;
int ATS_TEST_CASE_VIDEO_PTS_OVERLAP                = 0;
int ATS_TEST_CASE_AUDIO_PTS_OFFSET                 = 0;
int ATS_TEST_CASE_VIDEO_PTS_OFFSET                 = 0;
int ATS_TEST_CASE_AUDIO_PTS_BIG_LAG                = 0;

int ATS_TEST_CASE_AUDIO_IMPLICIT_TRIGGER           = 0;
int ATS_TEST_CASE_AUDIO_XFILE_IMPLICIT_TRIGGER     = 0;
int ATS_TEST_CASE_NO_EBP_DESCRIPTOR                = 0;

int ATS_TEST_CASE_AUDIO_UNIQUE_LANG                = 0;

int ATS_TEST_CASE_SAP_TYPE_MISMATCH_AND_TOO_LARGE  = 0;
int ATS_TEST_CASE_SAP_TYPE_NOT_1_OR_2              = 0;

int ATS_TEST_CASE_ACQUISITION_TIME_NOT_PRESENT     = 0;
int ATS_TEST_CASE_ACQUISITION_TIME_MISMATCH        = 0;

int setTestCase (char * testCaseName)
{
   LOG_INFO_ARGS ("Setting test case: %s", testCaseName);

   if (strcmp(testCaseName, "ATS_TEST_CASE_AUDIO_PTS_GAP") == 0)
   {
      ATS_TEST_CASE_AUDIO_PTS_GAP = 1;
      return 0;
   }
   else if (strcmp(testCaseName, "ATS_TEST_CASE_AUDIO_PTS_OVERLAP") == 0)
   {
      ATS_TEST_CASE_AUDIO_PTS_OVERLAP = 1;
      return 0;
   }
   else if (strcmp(testCaseName, "ATS_TEST_CASE_VIDEO_PTS_GAP") == 0)
   {
      ATS_TEST_CASE_VIDEO_PTS_GAP = 1;
      return 0;
   }
   else if (strcmp(testCaseName, "ATS_TEST_CASE_VIDEO_PTS_OVERLAP") == 0)
   {
      ATS_TEST_CASE_VIDEO_PTS_OVERLAP = 1;
      return 0;
   }
   else if (strcmp(testCaseName, "ATS_TEST_CASE_AUDIO_PTS_OFFSET") == 0)
   {
      ATS_TEST_CASE_AUDIO_PTS_OFFSET = 1;
      return 0;
   }
   else if (strcmp(testCaseName, "ATS_TEST_CASE_VIDEO_PTS_OFFSET") == 0)
   {
      ATS_TEST_CASE_VIDEO_PTS_OFFSET = 1;
      return 0;
   }
   else if (strcmp(testCaseName, "ATS_TEST_CASE_AUDIO_PTS_BIG_LAG") == 0)
   {
      ATS_TEST_CASE_AUDIO_PTS_BIG_LAG = 1;
      return 0;
   }
   else if (strcmp(testCaseName, "ATS_TEST_CASE_AUDIO_IMPLICIT_TRIGGER") == 0)
   {
      ATS_TEST_CASE_AUDIO_IMPLICIT_TRIGGER = 1;
      return 0;
   }
   else if (strcmp(testCaseName, "ATS_TEST_CASE_AUDIO_XFILE_IMPLICIT_TRIGGER") == 0)
   {
      ATS_TEST_CASE_AUDIO_XFILE_IMPLICIT_TRIGGER = 1;
      return 0;
   }
   else if (strcmp(testCaseName, "ATS_TEST_CASE_NO_EBP_DESCRIPTOR") == 0)
   {
      ATS_TEST_CASE_NO_EBP_DESCRIPTOR = 1;
      return 0;
   }
   else if (strcmp(testCaseName, "ATS_TEST_CASE_AUDIO_UNIQUE_LANG") == 0)
   {
      ATS_TEST_CASE_AUDIO_UNIQUE_LANG = 1;
      return 0;
   }
   else if (strcmp(testCaseName, "ATS_TEST_CASE_SAP_TYPE_MISMATCH_AND_TOO_LARGE") == 0)
   {
      ATS_TEST_CASE_SAP_TYPE_MISMATCH_AND_TOO_LARGE = 1;
      return 0;
   }
   else if (strcmp(testCaseName, "ATS_TEST_CASE_SAP_TYPE_NOT_1_OR_2") == 0)
   {
      ATS_TEST_CASE_SAP_TYPE_NOT_1_OR_2 = 1;
      return 0;
   }
   else if (strcmp(testCaseName, "ATS_TEST_CASE_ACQUISITION_TIME_NOT_PRESENT") == 0)
   {
      ATS_TEST_CASE_ACQUISITION_TIME_NOT_PRESENT = 1;
      return 0;
   }
   else if (strcmp(testCaseName, "ATS_TEST_CASE_ACQUISITION_TIME_MISMATCH") == 0)
   {
      ATS_TEST_CASE_ACQUISITION_TIME_MISMATCH = 1;
      return 0;
   }

   return -1;
}