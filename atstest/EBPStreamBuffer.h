/*
Copyright (c) 2015, Cable Television Laboratories, Inc.(“CableLabs”)
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of CableLabs nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL CABLELABS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __H_EBP_STREAM_BUFFER_
#define __H_EBP_STREAM_BUFFER_

#include <pthread.h>


typedef struct 
{
   uint8_t *buf;
   int bufSz;
   uint8_t* writePtr;
   uint8_t* readPtr;
   uint8_t* peekPtr;

   unsigned int writeBufferTraversals;
   unsigned int readBufferTraversals;
   unsigned int peekBufferTraversals;

   pthread_mutex_t mutex;
   pthread_cond_t cb_nonempty_cond;

   int disabled;

} circular_buffer_t;

int cb_init (circular_buffer_t *cb, int bufferSz);
void cb_free (circular_buffer_t *cb);
void cb_empty (circular_buffer_t *cb);
int cb_disable (circular_buffer_t *cb);
int cb_is_disabled (circular_buffer_t *cb);
int cb_get_total_size (circular_buffer_t *cb);

int cb_peek (circular_buffer_t *cb, uint8_t* bytes, int bytesSz);
int cb_read (circular_buffer_t *cb, uint8_t* bytes, int bytesSz);
int cb_read_or_peek (circular_buffer_t *cb, uint8_t* bytes, int bytesSz, int isPeek);
int cb_write (circular_buffer_t *cb, uint8_t* bytes, int bytesSz);

int cb_read_size (circular_buffer_t *cb);
int cb_available_write_size (circular_buffer_t *cb);


#endif  // __H_EBP_STREAM_BUFFER_

