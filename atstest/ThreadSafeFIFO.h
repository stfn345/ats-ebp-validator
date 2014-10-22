/*
** Copyright (C) 2014  Cable Television Laboratories, Inc.
** Contact: http://www.cablelabs.com/
 

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
