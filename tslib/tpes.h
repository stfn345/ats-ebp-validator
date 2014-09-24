/*

 Copyright (c) 2012-, ISO/IEC JTC1/SC29/WG11
 Written by Alex Giladi <alex.giladi@gmail.com>
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

#ifndef _TSLIB_TPES_H_
#define _TSLIB_TPES_H_

#include <stdint.h>

#include "bs.h"
#include "common.h"
#include "ts.h"
#include "pes.h"
#include "psi.h"
#include "vqarray.h"

#ifdef __cplusplus
extern "C" 
{
#endif

#include <stdint.h>

#include "bs.h"
#include "common.h"
#include "ts.h"
#include "vqarray.h"


typedef int (*pes_processor_t)(pes_packet_t *, elementary_stream_info_t *, vqarray_t*, void *); 
typedef int (*pes_arg_destructor_t)(void *); 

typedef struct 
{
   vqarray_t *ts_queue; 
   pes_processor_t process_pes_packet; 
   void *pes_arg; 
   pes_arg_destructor_t pes_arg_destructor;
} pes_demux_t; 


pes_demux_t* pes_demux_new(pes_processor_t pes_processor); 
void pes_demux_free(pes_demux_t *pdm); 
int pes_demux_process_ts_packet(ts_packet_t *ts, elementary_stream_info_t *es_info, void *arg); 


#ifdef __cplusplus
}
#endif

#endif
