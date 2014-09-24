
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


#include "std.h"


#include <math.h>

stc_t* stc_new() 
{ 
   stc_t *stc = malloc(sizeof(stc_t)); 
   stc->prev_pcr = UINT64_MAX; 
   stc->ts_in = vqarray_new(); 
   stc->ts_out = vqarray_new(); 
   stc->pcr_rate = -1.0; 
   return stc;
}
void stc_free(stc_t *stc) 
{ 
   if (stc == NULL) return; 
   
   if (stc->ts_in != NULL) 
   {
      vqarray_foreach(stc->ts_in, (vqarray_functor_t)ts_free); 
      vqarray_free(stc->ts_in);
   }
   
   if (stc->ts_out != NULL) 
   {
      vqarray_foreach(stc->ts_out, (vqarray_functor_t)ts_free); 
      vqarray_free(stc->ts_out);
   }
   
   free(stc);
}

int stc_put_ts_packet(stc_t *stc, ts_packet_t *ts) 
{ 
   if (stc == NULL || ts == NULL) return 0; 
   
   uint64_t real_pcr = ts_read_pcr(ts); 
   
   if (real_pcr < PCR_MAX) 
   {
      // we have a vald PCR
      
      SKIT_PRINT_UINT64(real_pcr/300);
      SKIT_PRINT_DOUBLE(stc->pcr_rate);

      if (stc->prev_pcr < PCR_MAX) 
      {
         // this is the first PCR we're seeing in this stream
         stc->prev_pcr = real_pcr; 
         stc->num_bytes = TS_SIZE;
         
      } 
      else 
      {
         // adjust for mod42 operations
         uint64_t adj_pcr = (real_pcr > stc->prev_pcr) ? real_pcr : real_pcr + PCR_MAX; 
         
         stc->pcr_rate = ((double)stc->num_bytes) / (adj_pcr - stc->prev_pcr); 
         
         ts_packet_t *qtsp = NULL; 
         
         for (int i = 1; (qtsp = vqarray_shift(stc->ts_in)) != NULL; i++) 
         {
            qtsp->pcr_int = stc->prev_pcr + (uint64_t)llrint(i * TS_SIZE * stc->pcr_rate); 
            if (qtsp->pcr_int >= PCR_MAX) 
            {
               qtsp->pcr_int -= PCR_MAX;
            }
            qtsp->pcr_int = real_pcr; // debug
            vqarray_add(stc->ts_out, qtsp);
         }
         
         
         stc->num_bytes = TS_SIZE; 
         stc->prev_pcr = real_pcr;
      }
      vqarray_add(stc->ts_in, ts);
   } 
   else 
   {
      stc->num_bytes += TS_SIZE; 
      vqarray_add(stc->ts_in, ts);
   }
   
   return 1;
}

ts_packet_t* stc_get_ts_packet(stc_t *stc) 
{ 
   if (stc == NULL) return NULL; 
   if (stc->ts_out == NULL) return NULL;   
   return (vqarray_shift(stc->ts_out));
}

void stc_flush(stc_t *stc) 
{ 
   
   ts_packet_t *qtsp = NULL; 
   
   for (int i = 1; (qtsp = vqarray_shift(stc->ts_in)) != NULL; i++) 
   {
      if (isnormal(qtsp->pcr_int)) 
      {
         qtsp->pcr_int = stc->prev_pcr + (uint64_t)llrint(i * TS_SIZE * stc->pcr_rate); 
         if (qtsp->pcr_int >= PCR_MAX) 
         {
            qtsp->pcr_int -= PCR_MAX;
         }
      }
      
      vqarray_add(stc->ts_out, qtsp);
   }
}
