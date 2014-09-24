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

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

#include "log.h"

int tslib_loglevel = TSLIB_LOG_LEVEL_DEBUG; 

#define INDENT_LEVEL	4
#define PREFIX_BUF_LEN	0x80

int skit_log_struct(int num_indents, char *name, uint64_t value, int type, char *str) 
{ 
   if (name == NULL) return 0; 
   
   // get rid of prefixes
   char *last_dot = strrchr(name, '.'); 
   char *last_arrow = strrchr(name, '>'); 
   char *real_name = NULL; 
   
   if (last_dot == NULL) 
   {
      real_name = (last_arrow != NULL) ? last_arrow + 1 : name;
   } 
   else  
   {
      if (last_arrow != NULL) real_name = (last_dot > last_arrow) ? last_dot : last_arrow; 
      else real_name = last_dot; 
      real_name++;
   }
   
   int nbytes = 0, indent_len = INDENT_LEVEL * num_indents; 
   if (indent_len >= PREFIX_BUF_LEN) 
   {
      LOG_ERROR_ARGS("Too many indents - %d", num_indents); 
      return 0;
   }
   char prefix[PREFIX_BUF_LEN]; 
   memset(prefix, ' ', indent_len); 
   prefix[indent_len] = 0; 
   
// rewrite this shit in a sane way!!!
   
   switch (type) 
   {
   case SKIT_LOG_TYPE_UINT:
      nbytes += fprintf(stdout, "INFO: %s%s=%"PRId64"", prefix, real_name, (uint64_t)value); 
      break; 
   case SKIT_LOG_TYPE_UINT_DBG:
      nbytes += fprintf(stdout, "DEBUG: %s%s=%"PRId64"", prefix, real_name, (uint64_t)value); 
      break; 
   case SKIT_LOG_TYPE_UINT_HEX:
      nbytes += fprintf(stdout, "INFO: %s%s=0x%"PRIX64"", prefix, real_name, (uint64_t)value); 
      break; 
   case SKIT_LOG_TYPE_UINT_HEX_DBG:
      nbytes += fprintf(stdout, "DEBUG: %s%s=0x%"PRIX64"", prefix, real_name, (uint64_t)value); 
      break; 
   case SKIT_LOG_TYPE_STR:
      nbytes += fprintf(stdout, "INFO: %s%s=%s", prefix, real_name, (char *)value); 
      break; 
   case SKIT_LOG_TYPE_STR_DBG:
      nbytes += fprintf(stdout, "INFO: %s%s=%s", prefix, real_name, (char *)value); 
      break; 
   default:
      break;
   }

   if (str)
   nbytes += fprintf(stdout, " (%s)\n", str);
   else
   nbytes += fprintf(stdout, "\n");

   // TODO logging tasks:
   //  - additional targets (file, string)
   //  - additional log formats -- e.g. XML or/and JSON ???
   //  - separate handling strings....
   return nbytes;
}
