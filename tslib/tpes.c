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

#include <assert.h>

#include "tpes.h"
#include "libts_common.h"
#include "log.h"
#include "vqarray.h"

pes_demux_t* pes_demux_new(pes_processor_t pes_processor) 
{ 
   pes_demux_t *pdm = malloc(sizeof(pes_demux_t)); 
   if (pdm != NULL) 
   {
      pdm->ts_queue = vqarray_new(); 
      pdm->process_pes_packet = pes_processor; 
   }
   return pdm;
}

void pes_demux_free(pes_demux_t *pdm) 
{ 
   if (pdm == NULL) return; 
   if (pdm->ts_queue != NULL) 
   {
      vqarray_foreach(pdm->ts_queue, (vqarray_functor_t)ts_free); 
      vqarray_free(pdm->ts_queue);
   }
   
   if (pdm->pes_arg != NULL && pdm->pes_arg_destructor != NULL) 
   {
      pdm->pes_arg_destructor(pdm->pes_arg);
   }
   
   free(pdm);
}


int pes_demux_process_ts_packet(ts_packet_t *ts, elementary_stream_info_t *es_info, void *arg) 
{ 
   if ( es_info == NULL || arg == NULL)
       return 0; 

//   printf ("pes_demux_process_ts_packet\n");


   int end_of_pes = !!( ts == NULL );
   if ( !end_of_pes ) end_of_pes = ts->header.payload_unit_start_indicator;
   
   pes_demux_t *pdm = (pes_demux_t *)arg; 
   
   if ( end_of_pes ) 
   {      
      int packets_in_queue = vqarray_length(pdm->ts_queue); 
      if (packets_in_queue > 0) 
      {
         // we have something in the queue
         // chances are this is a PES packet
         int i;
         ts_packet_t *tsp = vqarray_get(pdm->ts_queue, 0);
         if (tsp->header.payload_unit_start_indicator == 0)
         {
            // the queue doesn't start with a complete TS packet
            LOG_ERROR("PES queue does not start from PUSI=1");
            // we'll do nothing and just clear the queue
            if (pdm->process_pes_packet != NULL) 
            {
               // at this point we don't own the PES packet memory
               pdm->process_pes_packet( NULL, es_info, pdm->ts_queue, pdm->pes_arg);  
            }
         }
         else
         {
         
         buf_t *vec = malloc(packets_in_queue * sizeof(buf_t)); // can be optimized...
         
         for (int i = 0; i < packets_in_queue; i++) 
         {
            tsp = vqarray_get(pdm->ts_queue, i); 
            if ((tsp != NULL) && (tsp->header.adaptation_field_control & TS_PAYLOAD)) 
            {
               vec[i].len = tsp->payload.len; 
               vec[i].bytes = tsp->payload.bytes;
            }
            else 
            {
               vec[i].len = 0; 
               vec[i].bytes = NULL;
            }            
         }
         pes_packet_t *pes = pes_new(); 
         pes_read_vec(pes, vec, packets_in_queue);
            
            if (pdm->process_pes_packet != NULL) 
            {
               // at this point we don't own the PES packet memory
               pdm->process_pes_packet(pes, es_info, pdm->ts_queue, pdm->pes_arg);  
            }
            else 
         {
            pes_free(pes); 
         }
         
            free(vec);        
         }
         
         
         // clean up 
         ts_packet_t *tmp = NULL; 
         while ((tmp = vqarray_shift(pdm->ts_queue)) != NULL) 
         {
            ts_free(tmp);
         }
                 
      }
   }
   if ( ts != NULL ) 
   {
       vqarray_add(pdm->ts_queue, ts);
   }
   return 1;   
}
