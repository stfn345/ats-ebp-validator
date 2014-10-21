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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "ThreadSafeFIFO.h"
#include "EBPThreadLogging.h"


int fifo_create (thread_safe_fifo_t *fifo, int id)
{
   fifo->id = id;

   int returnCode = pthread_mutex_init(&(fifo->fifo_mutex), NULL);
   if (returnCode != 0)
   {
      printThreadDebugMessage ("fifo_create (%d): Error %d calling pthread_mutex_init\n", fifo->id, returnCode);
      return -1;
   }

   returnCode = pthread_cond_init(&(fifo->fifo_nonempty_cond), NULL);
   if (returnCode != 0)
   {
      printThreadDebugMessage ("fifo_create (%d): Error %d calling pthread_cond_init\n", fifo->id, returnCode);
      return -1;
   }

   fifo->queue = varray_new();

   return 0;
}

int fifo_destroy (thread_safe_fifo_t *fifo)
{
   int returnCode = pthread_mutex_destroy(&(fifo->fifo_mutex));
   if (returnCode != 0)
   {
      printThreadDebugMessage ("fifo_destroy (%d): Error %d calling pthread_mutex_destroy\n", fifo->id, returnCode);
   }

   returnCode = pthread_cond_destroy(&(fifo->fifo_nonempty_cond));
   if (returnCode != 0)
   {
      printThreadDebugMessage ("fifo_destroy (%d): Error %d calling pthread_cond_destroy\n", fifo->id, returnCode);
   }

   varray_free(fifo->queue);

   return 0;
}

int fifo_push (thread_safe_fifo_t *fifo, void *element)
{
   int returnCode = 0;
   printThreadDebugMessage ("fifo_push (%d): entering...\n", fifo->id);

   printThreadDebugMessage ("fifo_push (%d): calling pthread_mutex_lock\n", fifo->id);
   returnCode = pthread_mutex_lock (&(fifo->fifo_mutex));
   if (returnCode != 0)
   {
      printThreadDebugMessage ("fifo_push (%d): error %d calling pthread_mutex_lock\n", fifo->id, returnCode);
      return -1;
   }


   printThreadDebugMessage ("fifo_push (%d): doing push\n", fifo->id);

   // do push onto queue here
   varray_insert(fifo->queue, 0, element);

   fifo->push_counter++;
   
   printThreadDebugMessage ("fifo_push (%d): doing push: fifo->value = %x\n", fifo->id, (unsigned int)element);


   printThreadDebugMessage ("fifo_push (%d): calling pthread_cond_signal\n", fifo->id);
   returnCode = pthread_cond_signal(&(fifo->fifo_nonempty_cond));
   if (returnCode != 0)
   {
      printThreadDebugMessage ("fifo_push (%d): error %d calling pthread_cond_signal\n", fifo->id, returnCode);
      // unlock mutex before returning
      pthread_mutex_unlock (&(fifo->fifo_mutex));
      return -1;  
   }

   printThreadDebugMessage ("fifo_push (%d): calling pthread_mutex_unlock\n", fifo->id);
   returnCode = pthread_mutex_unlock (&(fifo->fifo_mutex));
   if (returnCode != 0)
   {
      printThreadDebugMessage ("fifo_push (%d): error %d calling pthread_mutex_unlock\n", fifo->id, returnCode);
      return -1;
   }

   printThreadDebugMessage ("fifo_push (%d): exiting\n", fifo->id);
   return 0;
}

int fifo_pop (thread_safe_fifo_t *fifo, void **element)
{
   int returnCode = 0;
   printThreadDebugMessage ("fifo_pop (%d): entering...\n", fifo->id);

   returnCode = pthread_mutex_lock (&(fifo->fifo_mutex));
   if (returnCode != 0)
   {
      printThreadDebugMessage ("fifo_pop (%d): error %d calling pthread_mutex_lock\n", fifo->id, returnCode);
      return -1;
   }

   printThreadDebugMessage ("fifo_pop (%d): doing pop\n", fifo->id);

   // check queue not empty here
   if (varray_length(fifo->queue) != 0)
   {
      printThreadDebugMessage ("fifo_pop (%d): queue not empty -- fifo_pop setting element\n", fifo->id);
   }
   else
   {
      printThreadDebugMessage ("fifo_pop (%d): queue empty -- pop entering wait\n", fifo->id);
      returnCode = pthread_cond_wait(&(fifo->fifo_nonempty_cond), &(fifo->fifo_mutex));
      if (returnCode != 0)
      {
         printThreadDebugMessage ("fifo_pop (%d): error %d calling pthread_cond_signal\n", fifo->id, returnCode);
         // unlock mutex before returning
         pthread_mutex_unlock (&(fifo->fifo_mutex));
         return -1;
      }

      printThreadDebugMessage ("fifo_pop (%d): setting element -- 2\n", fifo->id);
   }

   *element = varray_pop(fifo->queue);
   fifo->pop_counter++;
   printThreadDebugMessage ("fifo_pop (%d): setting element: *element = %x\n", fifo->id, (unsigned int)(*element));

   printThreadDebugMessage ("fifo_pop (%d): calling pthread_mutex_unlock\n", fifo->id);
   returnCode = pthread_mutex_unlock (&(fifo->fifo_mutex));
   if (returnCode != 0)
   {
      printThreadDebugMessage ("fifo_pop (%d): Error %d calling pthread_mutex_unlock\n", fifo->id, returnCode);
      return -1;
   }

   printThreadDebugMessage ("fifo_pop (%d): exiting\n", fifo->id);
   return 0;
}

int fifo_get_state (thread_safe_fifo_t *fifo, int *size)
{
   int returnCode = 0;
   printThreadDebugMessage ("fifo_get_state (%d): entering\n", fifo->id);

   returnCode = pthread_mutex_lock (&(fifo->fifo_mutex));
   if (returnCode != 0)
   {
      printThreadDebugMessage ("fifo_get_state (%d): Error %d calling pthread_mutex_lock\n", fifo->id, returnCode);
      return -1;
   }

   *size = varray_length(fifo->queue);

   returnCode = pthread_mutex_unlock (&(fifo->fifo_mutex));
   if (returnCode != 0)
   {
      printThreadDebugMessage ("fifo_get_state (%d): Error %d calling pthread_mutex_unlock\n", fifo->id, returnCode);
      return -1;
   }

   return 0;
}

