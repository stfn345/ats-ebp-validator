/*

 Copyright (c) 2012-, ISO/IEC JTC1/SC29/WG11 
 Written by Alex Giladi <alex.giladi@gmail.com> and Vlad Zbarsky <zbarsky@cornell.edu>
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 * Neither the name of the ISO/IEC nor the
 names of its contributors may be used to endorse or promote products
 derived from this software without specific prior written permission.

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

#ifndef _STREAMKIT_LOG_H_
#define _STREAMKIT_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

// we need those for inttypes.h in C++
#if defined __cplusplus && !defined __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS 1
#endif
#include <inttypes.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

int set_log_file(char * logFilePath);
void cleanup_log_file();

extern int tslib_loglevel;
extern FILE* tslib_logfile;



#define SKIT_LOG_TYPE_UINT			0x01
#define SKIT_LOG_TYPE_UINT_HEX		0x02
#define SKIT_LOG_TYPE_STR			0x03

#define SKIT_LOG_TYPE_UINT_DBG		0x04
#define SKIT_LOG_TYPE_UINT_HEX_DBG	0x05
#define SKIT_LOG_TYPE_STR_DBG		0x06

#define SKIT_LOG_UINT32_DBG(str, prefix, arg, n)  fprintf((tslib_logfile == NULL)?stdout:tslib_logfile, "DEBUG: %s%s=%"PRIu32"\n", prefix, #arg, (arg));
#define SKIT_LOG_UINT32_HEX_DBG(str, prefix, arg, n)  fprintf((tslib_logfile == NULL)?stdout:tslib_logfile, "DEBUG: %s%s=%"PRIX32"\n", prefix, #arg, (arg));
#define SKIT_LOG_UINT64_DBG(str, prefix, arg, n)  fprintf((tslib_logfile == NULL)?stdout:tslib_logfile, "DEBUG: %s%s=%"PRIu64"\n", prefix, #arg, (arg));

#define SKIT_LOG_UINT(str, level, arg, n) skit_log_struct((level), #arg,  (arg), SKIT_LOG_TYPE_UINT, NULL);
#define SKIT_LOG_UINT_DBG(str, level, arg, n) skit_log_struct((level), #arg,  (arg), SKIT_LOG_TYPE_UINT_DBG, NULL);
#define SKIT_LOG_UINT_HEX(str, level, arg, n) skit_log_struct((level), #arg,  (arg), SKIT_LOG_TYPE_UINT_HEX, NULL);
#define SKIT_LOG_UINT_HEX_DBG(str, level, arg, n) skit_log_struct((level), #arg,  (arg), SKIT_LOG_TYPE_UINT_HEX_DBG, NULL);
#define SKIT_LOG_UINT_VERBOSE(str, level, arg, explain, n) skit_log_struct((level), #arg,  (arg), SKIT_LOG_TYPE_UINT, explain);
#define SKIT_LOG_STR(str, level, arg, n) skit_log_struct((level), #arg, arg, SKIT_LOG_TYPE_STR, NULL);
#define SKIT_LOG_STR_DBG(str, level, arg, n) skit_log_struct((level), #arg, arg, SKIT_LOG_TYPE_STR_DBG, NULL);

int skit_log_struct(int level, char *name, uint64_t value, int type, char *str);

// More traditional debug logging
// tslib-global loglevel: error > warn (default) > info > debug
#define TSLIB_LOG_LEVEL_ERROR		1
#define TSLIB_LOG_LEVEL_WARN		((TSLIB_LOG_LEVEL_ERROR) + 1)
#define TSLIB_LOG_LEVEL_INFO		((TSLIB_LOG_LEVEL_ERROR) + 2)
#define TSLIB_LOG_LEVEL_DEBUG		((TSLIB_LOG_LEVEL_ERROR) + 3)

#define TSLIB_LOG_LEVEL_DEFAULT		TSLIB_LOG_LEVEL_WARN


#define LOG_ERROR(msg)				{ if (tslib_loglevel >= TSLIB_LOG_LEVEL_ERROR) { \
            fprintf((tslib_logfile == NULL)?stdout:tslib_logfile, "ERROR: %s\t\t[%s() @ %s:%d]\n", msg, __FUNCTION__, __FILE__, __LINE__);  \
            fflush ((tslib_logfile == NULL)?stdout:tslib_logfile); } \
         }
#define LOG_ERROR_ARGS(format, ...)	{ if (tslib_loglevel >= TSLIB_LOG_LEVEL_ERROR) { \
										fprintf((tslib_logfile == NULL)?stdout:tslib_logfile, "ERROR: "); \
										fprintf((tslib_logfile == NULL)?stdout:tslib_logfile, format, __VA_ARGS__); \
										fprintf((tslib_logfile == NULL)?stdout:tslib_logfile, "\t\t[%s() @ %s:%d]\n", __FUNCTION__, __FILE__, __LINE__); \
                              fflush ((tslib_logfile == NULL)?stdout:tslib_logfile); } \
									}

#define LOG_WARN(msg)	{	if (tslib_loglevel >= TSLIB_LOG_LEVEL_DEBUG) { \
											fprintf((tslib_logfile == NULL)?stdout:tslib_logfile, "WARNING: %s\t\t[%s() @ %s:%d]\n", msg, __FUNCTION__, __FILE__, __LINE__); \
                                 fflush ((tslib_logfile == NULL)?stdout:tslib_logfile); } \
                              else if (tslib_loglevel >= TSLIB_LOG_LEVEL_WARN) { \
											fprintf((tslib_logfile == NULL)?stdout:tslib_logfile, "WARNING: %s\n", msg); \
                                 fflush ((tslib_logfile == NULL)?stdout:tslib_logfile); } \
									}
#define LOG_WARN_ARGS(format, ...)	{ if (tslib_loglevel >= TSLIB_LOG_LEVEL_WARN) { \
										fprintf((tslib_logfile == NULL)?stdout:tslib_logfile, "WARNING: "); \
										fprintf((tslib_logfile == NULL)?stdout:tslib_logfile, format, __VA_ARGS__); \
										if (tslib_loglevel >= TSLIB_LOG_LEVEL_DEBUG) \
											fprintf((tslib_logfile == NULL)?stdout:tslib_logfile, "\t\t[%s() @ %s:%d]\n", __FUNCTION__, __FILE__, __LINE__); \
										else \
											fprintf((tslib_logfile == NULL)?stdout:tslib_logfile, "\n"); \
                              fflush ((tslib_logfile == NULL)?stdout:tslib_logfile); } \
									}

#define LOG_INFO(msg)				{ if (tslib_loglevel >= TSLIB_LOG_LEVEL_INFO) { \
										fprintf((tslib_logfile == NULL)?stdout:tslib_logfile, "INFO: %s\n", msg); \
                              fflush ((tslib_logfile == NULL)?stdout:tslib_logfile); } \
                                 }
#define LOG_INFO_ARGS(format, ...)	{ if (tslib_loglevel >= TSLIB_LOG_LEVEL_INFO) { \
										fprintf((tslib_logfile == NULL)?stdout:tslib_logfile, "INFO: "); \
										fprintf((tslib_logfile == NULL)?stdout:tslib_logfile, format, __VA_ARGS__); \
										fprintf((tslib_logfile == NULL)?stdout:tslib_logfile, "\n"); \
                              fflush ((tslib_logfile == NULL)?stdout:tslib_logfile); } \
									}

#define LOG_DEBUG(msg)		{ if (tslib_loglevel >= TSLIB_LOG_LEVEL_DEBUG) { \
										fprintf((tslib_logfile == NULL)?stdout:tslib_logfile, "DEBUG: %s\t\t[%s() @ %s:%d]\n", msg, __FUNCTION__, __FILE__, __LINE__); \
                              fflush ((tslib_logfile == NULL)?stdout:tslib_logfile); } \
                           }
#define LOG_DEBUG_ARGS(format, ...)	{ if (tslib_loglevel >= TSLIB_LOG_LEVEL_DEBUG) { \
										fprintf((tslib_logfile == NULL)?stdout:tslib_logfile, "DEBUG: "); \
										fprintf((tslib_logfile == NULL)?stdout:tslib_logfile, format, __VA_ARGS__); \
										fprintf((tslib_logfile == NULL)?stdout:tslib_logfile, "\t\t[%s() @ %s:%d]\n", __FUNCTION__, __FILE__, __LINE__);  \
                              fflush ((tslib_logfile == NULL)?stdout:tslib_logfile); } \
									}


#ifdef __cplusplus
}
#endif

#endif // _STREAMKIT_LOG_H_
