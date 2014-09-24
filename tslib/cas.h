/*

 Copyright (c) 2014-, ISO/IEC JTC1/SC29/WG11
 
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

#ifndef _TSLIB_CAS_H_
#define _TSLIB_CAS_H_

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
#include "descriptors.h"

typedef struct 
{
   int PID;    
   ts_packet_t* ecm; 
   vqarray_t* elementary_pids;
} ecm_pid_t; 

typedef struct
{
   int id; 
   int EMM_PID; 
   vqarray_t *ecm_pids; 
   buf_t emm;
} ca_system_t;



ca_system_t* ca_system_new(int CA_system_id);
void ca_system_free(ca_system_t *cas);



/**
 * Process a TS packet from an ECM PID 
 * 
 * @author agiladi (4/24/2014)
 * 
 * @param ts ECM packet
 * @param es_info es_info for the ECM PID
 * @param arg vqarray of ca_system_t objects 
 * 
 * @return 1 if processed. 
 */
int ca_system_process_ts_packet(ts_packet_t *ts, elementary_stream_info_t *es_info, void *arg);

int ca_system_process_ca_descriptor(vqarray_t *cas_list, elementary_stream_info_t *esi, ca_descriptor_t *cad);

#ifdef __cplusplus
}
#endif

#endif
