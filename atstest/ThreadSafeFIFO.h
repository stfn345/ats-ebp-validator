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

#ifndef __H_THREAD_SAFE_FIFO_LLIH876JHG221
#define __H_THREAD_SAFE_FIFO_LLIH876JHG221

#include <pthread.h>
#include "varray.h"

typedef struct
{
   int id;  // ID to uniquely tag this fifo

   varray_t* queue;

   pthread_mutex_t fifo_mutex;
   pthread_cond_t fifo_nonempty_cond;

   int push_counter;
   int pop_counter;

} thread_safe_fifo_t;


int fifo_create (thread_safe_fifo_t *fifo, int id);
int fifo_destroy (thread_safe_fifo_t *fifo);
int fifo_push (thread_safe_fifo_t *fifo, void *element);
int fifo_pop (thread_safe_fifo_t *fifo, void **element);
int fifo_peek (thread_safe_fifo_t *fifo, void **element);
int fifo_get_state (thread_safe_fifo_t *fifo, int *size);

// intternal methods
int fifo_pop_peek (thread_safe_fifo_t *fifo, void **element, int isPop);



#endif // __H_THREAD_SAFE_FIFO_LLIH876JHG221
