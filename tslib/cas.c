/*
 Copyright (c) 2014-, ISO/IEC JTC1/SC29/WG11
 
 Written by Alex Giladi <alex.giladi@gmail.com>
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 * Neither the name of ISO/IEC nor the
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

#include "cas.h"
#include "libts_common.h"
#include "log.h"
#include "vqarray.h"


ca_system_t* ca_system_new(int CA_system_id) 
{ 
   ca_system_t *cas = calloc(1, sizeof(ca_system_t)); 
   cas->id = CA_system_id; 
   cas->ecm_pids = vqarray_new(); 
   return cas;
}

void ca_system_free(ca_system_t *cas) 
{ 
   ecm_pid_t *ep = NULL; 
   
   while ((ep = vqarray_shift(cas->ecm_pids)) != NULL) 
   {
      if (ep->ecm != NULL) ts_free(ep->ecm); 
      free(ep);
   }
   vqarray_free(cas->ecm_pids); 
   free(cas);
}


// FIXME: for now, we ignore EMMs

int ca_system_process_ts_packet(ts_packet_t *ts, elementary_stream_info_t *es_info, void *arg) 
{ 
   if (ts == NULL || arg == NULL) return 0; 
   vqarray_t *cas_list = (vqarray_t *)arg; 
   ecm_pid_t *ep = NULL; 
   
   for (int i = 0; i < vqarray_length(cas_list); i++) 
   {
      ca_system_t *cas = vqarray_get(cas->ecm_pids, i); 
      if (cas == NULL) continue; 
      
      for (int j  = 0; j < vqarray_length(cas->ecm_pids); j++) 
      {
         ep = vqarray_get(cas->ecm_pids, j); 
         if (ep != NULL) 
         {
            if (ep->PID == ts->header.PID) ts_free(ep->ecm); 
            ep->ecm = ts; 
            return 1;
         }            
      }
   }
   
   ts_free(ts); 
   LOG_WARN_ARGS("PID 0x%04X does not appear to carry CAS-related information", ts->header.PID); 
   
   return 0;
}

int ca_system_process_ca_descriptor(vqarray_t *cas_list, elementary_stream_info_t *esi, ca_descriptor_t *cad) 
{ 
   int i = 0; 
   ca_system_t *cas = NULL; 
   
   if (cas_list == NULL) return 0; 
   
   for (int i = 0; i < vqarray_length(cas_list); i++) 
   {
      cas = vqarray_get(cas->ecm_pids, i); 
      if (cas == NULL) continue; 
      if (cas->id == cad->CA_system_ID) break; 
   }
   
   if (cas == NULL) 
   {
      // new CAS
      cas = ca_system_new(cad->CA_system_ID) ;
   }
   
   if (esi == NULL) 
   {
      // ca_descriptor is from CAT
      cas->EMM_PID = cad->CA_PID; 
      return 1;     
   }
   
   // from here onwards this descriptor is per single ES

   ecm_pid_t *ep = NULL; 
   
   for (i = 0; i < vqarray_length(cas->ecm_pids); i++) 
   {
      ecm_pid_t *ep = vqarray_get(cas->ecm_pids, i); 
      if ((ep != NULL) &&  (ep->PID == cad->CA_PID)) 
      {
         for (int j = 0; j < vqarray_length(ep->elementary_pids); j++) 
         {
            elementary_stream_info_t *tmp = vqarray_get(ep->elementary_pids, i); 
            if (tmp->elementary_PID == esi->elementary_PID) return 1;            
         }
      }
   }
   
   // we have a new ECM PID
   if (ep == NULL) 
   {
      ep = malloc(sizeof(ecm_pid_t)); 
      ep->PID = cad->CA_PID;
      ep->elementary_pids = vqarray_new(); 
      vqarray_push(ep->elementary_pids, esi); 
      ep->ecm = NULL; 
      vqarray_push(cas->ecm_pids, ep);
   }

   return 1;
}

ts_packet_t* ca_system_get_ecm(ca_system_t *cas, uint32_t ECM_PID) 
{ 
   if (cas == NULL) return NULL; 
   for (int i  = 0; i < vqarray_length(cas->ecm_pids); i++) 
   {
      ecm_pid_t *ep = vqarray_get(cas->ecm_pids, i); 
      if ((ep != NULL) &&  (ep->PID == ECM_PID)) return ep->ecm;
   }
   return NULL;
}

