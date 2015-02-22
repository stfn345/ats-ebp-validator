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

#ifndef __H_ATS_TEST_REPORT
#define __H_ATS_TEST_REPORT

#include <unistd.h>
#include <stdarg.h>

#include "../atstest/EBPCommon.h"

typedef struct
{
   int64_t PTS;
   uint8_t partitionId;
   uint8_t ingestId;
   uint8_t streamId;
   uint32_t PID;

} bp_info_t;


void reportAddPTS (int64_t PTS, uint8_t partitionId, uint8_t ingestId, uint8_t streamId, uint32_t PID);

void reportAddErrorLog (char *errorMsg);
void reportAddErrorLogArgs (const char *fmt, ...);
void reportAddInfoLog (char *infoMsg);
void reportAddInfoLogArgs (const char *fmt, ...);

void reportClearData();
void reportInit();
void reportCleanup();
char *reportPrint(int numIngests, int numStreams, ebp_stream_info_t **streamInfoArray, char **ingestNames);

void reportPrintBoundaryInfoArray(FILE *reportFile, ebp_boundary_info_t *boundaryInfoArray);
void reportPrintStreamInfo(FILE *reportFile, int numIngests, int numStreams, ebp_stream_info_t **streamInfoArray, char **ingestNames);






#endif  // __H_ATS_TEST_REPORT


