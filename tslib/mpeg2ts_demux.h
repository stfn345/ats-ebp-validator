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

#ifndef _MPEG2TS_DEMUX_H_
#define _MPEG2TS_DEMUX_H_

#include <stdint.h>

#include "bs.h"
#include "common.h"
#include "ts.h"
#include "pes.h"
#include "psi.h"
#include "cas.h"
#include "descriptors.h"
#include "vqarray.h"

#ifdef __cplusplus
extern "C" 
{
#endif

struct _mpeg2ts_stream_; 
struct _mpeg2ts_program_; 

typedef int (*ts_pid_processor_t)(ts_packet_t *, elementary_stream_info_t *, void *); 
typedef int (*pat_processor_t)(struct _mpeg2ts_stream_ *, void *); 
typedef int (*cat_processor_t)(struct _mpeg2ts_stream_ *, void *); 
typedef int (*pmt_processor_t)(struct _mpeg2ts_program_ *, void *); 

typedef int (*arg_destructor_t)(void *); 

struct _mpeg2ts_program_
{
   uint32_t PID;            /// PMT PID
   uint32_t program_number; 
   
   vqarray_t *pids; /// list of PIDs belonging to this program 
                    /// each element is of type pid_info_t
   
   struct 
   {
      int64_t first_pcr; 
      int32_t num_rollovers; 
      int64_t pcr[2]; 
      int32_t packets_from_last_pcr;
      double pcr_rate;

   } pcr_info; /// information on STC clock state
   
   program_map_section_t *pmt;      /// parsed PMT
   pmt_processor_t pmt_processor;   /// callback called after PMT was processed
   void *arg;                       /// argument for PMT callback
   arg_destructor_t arg_destructor; /// destructor for the callback argument
}; 


struct _mpeg2ts_stream_ 
{
   program_association_section_t *pat; /// PAT
   conditional_access_section_t *cat;  /// CAT
   pat_processor_t pat_processor;      /// callback called after PAT was processed
   cat_processor_t cat_processor;      /// callback called after CAT was processed
   vqarray_t *programs;                /// list of programs in this multiplex
   vqarray_t *ca_systems;              /// list of conditional access systems in this multiplex
   void *arg;                          /// argument for PAT/CAT callbacks
   arg_destructor_t arg_destructor;    /// destructor for the callback argument
}; 

typedef struct _mpeg2ts_stream_  mpeg2ts_stream_t; 
typedef struct _mpeg2ts_program_ mpeg2ts_program_t; 

typedef struct  
{
   void *arg;                            /// argument for ts packet processor
   arg_destructor_t arg_destructor;      /// destructor for arg
   ts_pid_processor_t process_ts_packet; /// ts packet processor, needs to be registered with mpeg2ts_program
} demux_pid_handler_t; 

typedef struct 
{
   void *arg; 
   // TODO impl
} mux_pid_handler_t; 

typedef struct 
{
   demux_pid_handler_t *demux_handler;   /// demux handler
   demux_pid_handler_t *demux_validator; /// demux validator
   // TODO: mux_pid_handler_t*
   elementary_stream_info_t *es_info;  /// ES-level information (type, descriptors)
   int continuity_counter;             /// running continuity counter
   uint64_t num_packets;
} pid_info_t; 

/**
 * Initialize mpeg2ts_stream_t object
 * 
 * @author agiladi (3/21/2014) 
 * @return initialized but empty object 
 */
mpeg2ts_stream_t* mpeg2ts_stream_new(); 

/**
 * Free mpeg2ts_stream_t object
 * 
 * @author agiladi (3/21/2014)

 */
void mpeg2ts_stream_free(mpeg2ts_stream_t *m2s); 

/**
 * Read a parsed transport stream packet
 * 
 * @author agiladi (3/21/2014)
 * 
 * @param m2s MPEG-2 TS multiplex
 * @param ts  parsed TS packet
 * @note by calling this function the caller relinquishes the 
 *       ownership of the TS packet, and the latter may or may
 *       not be freed by mpeg2ts_stream
 * @see ts_read 
 * 
 * @return int 
 */
int mpeg2ts_stream_read_ts_packet(mpeg2ts_stream_t *m2s, ts_packet_t *ts); 

/**
 * Initialize new program object
 * 
 * @author agiladi (3/21/2014)
 * 
 * @return mpeg2ts_program_t* 
 */
mpeg2ts_program_t* mpeg2ts_program_new(); 

/**
 * Free a program object
 * 
 * @author agiladi (3/21/2014)
 * 
 * @param m2p 
 */
void mpeg2ts_program_free(mpeg2ts_program_t *m2p); 

/**
 * Register a PID processor callback for a given PID
 * 
 * @author agiladi (3/20/2014)
 * 
 * @param m2p program to which the PID belongs
 * @param PID PID for which the callback is registered
 * @param handler callback which will be called per each TS 
 *                packet from this PID (note: this is one of the
 *                most frequently called functions!!!)
 * @param validator optional callback for each TS packet from 
 *        this PID. The validator callback may not alter the
 *        state of the packet or own the memory.
 * 
 * @return zero if registration succeeded. 
 */
int mpeg2ts_program_register_pid_processor(mpeg2ts_program_t *m2p, uint32_t PID, demux_pid_handler_t *handler,demux_pid_handler_t *validator); 

/**
 * Unregister a PID processor callback for a given PID
 * 
 * @author agiladi (3/20/2014)
 * 
 * @param m2p program to which the PID belongs
 * @param PID PID for which the callback is registered

 * 
 * @return zero if unregistration succeeded. 
 */
int mpeg2ts_program_unregister_pid_processor(mpeg2ts_program_t *m2p, uint32_t PID); 

//int mpeg2ts_program_read_ts_packet(mpeg2ts_program_t *m2p, ts_packet_t *ts);
int mpeg2ts_stream_reset(mpeg2ts_stream_t *m2s);

#ifdef __cplusplus
}
#endif

#endif // _MPEG2TS_DEMUX_H_
