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

#include <stdlib.h>
#include <inttypes.h>
#include <memory.h>

#include "log.h"
#include "EBPStreamBuffer.h"

static int cb_size_internal (circular_buffer_t *cb);


int cb_init (circular_buffer_t *cb, int bufferSz)
{
   if (cb == NULL)
   {
      LOG_ERROR ("EBPSreamBuffer: cb_init: cb == NULL");
      return -1;
   }

   int returnCode = pthread_mutex_init(&(cb->mutex), NULL);
   if (returnCode != 0)
   {
      LOG_ERROR_ARGS ("EBPSreamBuffer: cb_init: pthread_mutex_init failed: %d", returnCode);
      return -1;
   }

   returnCode = pthread_cond_init(&(cb->cb_nonempty_cond), NULL);
   if (returnCode != 0)
   {
      LOG_ERROR_ARGS ("EBPSreamBuffer: cb_init: pthread_cond_init failed: %d", returnCode);
      return -1;
   }


   cb->buf = (uint8_t*) malloc (bufferSz);
   cb->bufSz = bufferSz;
   cb->writePtr = cb->buf;
   cb->readPtr = cb->buf;
   cb->noWrites = 1;

   return 0;
}

void cb_free (circular_buffer_t *cb)
{
   pthread_mutex_destroy(&(cb->mutex));

   int returnCode = pthread_cond_destroy(&(cb->cb_nonempty_cond));
   if (returnCode != 0)
   {
      LOG_ERROR_ARGS ("EBPSreamBuffer: cb_free: Error %d calling pthread_cond_destroy", returnCode);
   }

   free (cb->buf);
   free (cb);
}

int cb_peek (circular_buffer_t *cb, uint8_t* bytes, int bytesSz)
{
   return cb_read_or_peek (cb, bytes, bytesSz, 1);
}

int cb_read (circular_buffer_t *cb, uint8_t* bytes, int bytesSz)
{
   return cb_read_or_peek (cb, bytes, bytesSz, 0);
}

int cb_read_or_peek (circular_buffer_t *cb, uint8_t* bytes, int bytesSz, int isPeek)
{
   int returnCode = pthread_mutex_lock (&(cb->mutex));
   if (returnCode != 0)
   {
      LOG_ERROR_ARGS ("EBPSreamBuffer: cb_read_or_peek: pthread_mutex_lock failed: %d", returnCode);
      return -2;
   }

   int usedSz = cb_size_internal (cb);

   // GORP: check this
   if (usedSz == 0)
   {
      returnCode = pthread_cond_wait(&(cb->cb_nonempty_cond), &(cb->mutex));
      if (returnCode != 0)
      {
         // unlock mutex before returning
         LOG_ERROR_ARGS ("EBPSreamBuffer: cb_read_or_peek: pthread_cond_wait failed: %d", returnCode);
         pthread_mutex_unlock (&(cb->mutex));
         return -1;
      }

      usedSz = cb_size_internal (cb);
   }

   int sizeToCopy = usedSz;
   if (usedSz > bytesSz)
   {
      sizeToCopy = bytesSz;
   }

   uint8_t *readPtr_Original = cb->readPtr;

   int sz1 = cb->buf + cb->bufSz - cb->readPtr;
   if (sz1 > sizeToCopy)
   {
      sz1 = sizeToCopy;
   }
   int sz2 = sizeToCopy - sz1;

   memcpy (cb->readPtr, bytes, sz1);
   cb->readPtr += sz1;
   if (cb->readPtr >= cb->buf + cb->bufSz)
   {
      cb->readPtr = cb->buf;
   }

   memcpy (cb->readPtr, bytes + sz1, sz2);
   cb->readPtr += sz2;

   if (isPeek)
   {
      cb->readPtr = readPtr_Original;
   }

   returnCode = pthread_mutex_unlock (&(cb->mutex));
   if (returnCode != 0)
   {
      LOG_ERROR_ARGS ("EBPSreamBuffer: cb_read_or_peek: pthread_mutex_unlock failed: %d", returnCode);
      return -2;
   }

   return sizeToCopy;
}

int cb_write (circular_buffer_t *cb, uint8_t* bytes, int bytesSz)
{
   int returnCode = pthread_mutex_lock (&(cb->mutex));
   if (returnCode != 0)
   {
      LOG_ERROR_ARGS ("EBPSreamBuffer: cb_write: pthread_mutex_lock failed: %d", returnCode);
      return -2;
   }

   int availableSz = cb->bufSz - cb_size_internal (cb);

   if (bytesSz > availableSz)
   {
      LOG_ERROR_ARGS ("EBPSreamBuffer: cb_write: requested sz %d greater than available sz %d", bytesSz, availableSz);

      returnCode = pthread_mutex_unlock (&(cb->mutex));
      if (returnCode != 0)
      {
         LOG_ERROR_ARGS ("EBPSreamBuffer: cb_write: pthread_mutex_unlock failed: %d", returnCode);
         return -2;
      }

      return -1;
   }

   int sz1 = cb->buf + cb->bufSz - cb->writePtr;
   if (sz1 > bytesSz)
   {
      sz1 = bytesSz;
   }
   int sz2 = bytesSz - sz1;

   memcpy (bytes, cb->writePtr, sz1);
   cb->writePtr += sz1;
   if (cb->writePtr >= cb->buf + cb->bufSz)
   {
      cb->writePtr = cb->buf;
   }

   memcpy (bytes + sz1, cb->writePtr, sz2);
   cb->writePtr += sz2;

   returnCode = pthread_cond_signal(&(cb->cb_nonempty_cond));
   if (returnCode != 0)
   {
      // unlock mutex before returning
      pthread_mutex_unlock (&(cb->mutex));
      return -1;  
   }

   returnCode = pthread_mutex_unlock (&(cb->mutex));
   if (returnCode != 0)
   {
      LOG_ERROR_ARGS ("EBPSreamBuffer: cb_write: pthread_mutex_unlock failed: %d", returnCode);
      return -2;
   }

   return 0;
}

int cb_size (circular_buffer_t *cb)
{
   int returnCode = pthread_mutex_lock (&(cb->mutex));
   if (returnCode != 0)
   {
      LOG_ERROR_ARGS ("EBPSreamBuffer: cb_size: pthread_mutex_lock failed: %d", returnCode);
      return -2;
   }

   int size = cb_size_internal (cb);

//      printf ("cb->readPtr = %x, cb->writePtr = %x size = %d\n", (unsigned int)cb->readPtr, (unsigned int)cb->writePtr, size);

   returnCode = pthread_mutex_unlock (&(cb->mutex));
   if (returnCode != 0)
   {
      LOG_ERROR_ARGS ("EBPSreamBuffer: cb_size: pthread_mutex_unlock failed: %d", returnCode);
      return -2;
   }

   return size;
}

int cb_available (circular_buffer_t *cb)
{
   printf ("cb->bufSz = %d\n", cb->bufSz);
   return cb->bufSz - cb_size (cb);
}

int cb_size_internal (circular_buffer_t *cb)
{
   // WARNING: THIS FUNCTION DOES NOT LOCK THE MUTEX AND SHOULD ONLY
   // BE CALLED BY A FUNCTION THAT HAS ALREADY LOCKED IT

   int size = 0;
   if (cb->noWrites || cb->writePtr > cb->readPtr)
   {
      size = cb->writePtr - cb->readPtr;
   }
   else
   {
      size = cb->bufSz - (cb->readPtr - cb->writePtr);
   }

   return size;
}

