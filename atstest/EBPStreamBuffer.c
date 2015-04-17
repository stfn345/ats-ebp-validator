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

#include <stdlib.h>
#include <inttypes.h>
#include <memory.h>

#include "log.h"
#include "EBPStreamBuffer.h"
#include "ATSTestReport.h"


static int cb_read_size_internal (circular_buffer_t *cb);
static int cb_peek_size_internal (circular_buffer_t *cb);


int cb_init (circular_buffer_t *cb, int bufferSz)
{
   if (cb == NULL)
   {
      LOG_ERROR ("EBPSreamBuffer: cb_init: cb == NULL");
      reportAddErrorLog ("EBPSreamBuffer: cb_init: cb == NULL");
      return -1;
   }

   int returnCode = pthread_mutex_init(&(cb->mutex), NULL);
   if (returnCode != 0)
   {
      LOG_ERROR_ARGS ("EBPSreamBuffer: cb_init: pthread_mutex_init failed: %d", returnCode);
      reportAddErrorLogArgs ("EBPSreamBuffer: cb_init: pthread_mutex_init failed: %d", returnCode);
      return -1;
   }

   returnCode = pthread_cond_init(&(cb->cb_nonempty_cond), NULL);
   if (returnCode != 0)
   {
      LOG_ERROR_ARGS ("EBPSreamBuffer: cb_init: pthread_cond_init failed: %d", returnCode);
      reportAddErrorLogArgs ("EBPSreamBuffer: cb_init: pthread_cond_init failed: %d", returnCode);
      return -1;
   }


   LOG_INFO_ARGS ("EBPSreamBuffer: cb_init: initializing buffer with size %d", bufferSz);
   cb->buf = (uint8_t*) malloc (bufferSz);
   cb->bufSz = bufferSz;
   cb->writePtr = cb->buf;
   cb->readPtr = cb->buf;
   cb->peekPtr = cb->buf;

   cb->writeBufferTraversals = 0;
   cb->readBufferTraversals = 0;
   cb->peekBufferTraversals = 0;

   return 0;
}

void cb_empty (circular_buffer_t *cb)
{
   cb->writePtr = cb->buf;
   cb->readPtr = cb->buf;
   cb->peekPtr = cb->buf;

   cb->writeBufferTraversals = 0;
   cb->readBufferTraversals = 0;
   cb->peekBufferTraversals = 0;
}

int cb_is_disabled (circular_buffer_t *cb) 
{
   return cb->disabled;
}

int cb_disable (circular_buffer_t *cb) 
{
   int returnCode = pthread_mutex_lock (&(cb->mutex));
   if (returnCode != 0)
   {
      LOG_ERROR_ARGS ("EBPSreamBuffer: cb_disable: pthread_mutex_lock failed: %d", returnCode);
      reportAddErrorLogArgs ("EBPSreamBuffer: cb_disable: pthread_mutex_lock failed: %d", returnCode);
      return -2;
   }

   cb->disabled = 1;

   returnCode = pthread_cond_signal(&(cb->cb_nonempty_cond));
   if (returnCode != 0)
   {
      // unlock mutex before returning
      LOG_ERROR_ARGS ("EBPSreamBuffer: cb_disable: pthread_cond_signal failed: %d", returnCode);
      reportAddErrorLogArgs ("EBPSreamBuffer: cb_disable: pthread_cond_signal failed: %d", returnCode);
      pthread_mutex_unlock (&(cb->mutex));
      return -1;  
   }

   returnCode = pthread_mutex_unlock (&(cb->mutex));
   if (returnCode != 0)
   {
      LOG_ERROR_ARGS ("EBPSreamBuffer: cb_disable: pthread_mutex_unlock failed: %d", returnCode);
      reportAddErrorLogArgs ("EBPSreamBuffer: cb_disable: pthread_mutex_unlock failed: %d", returnCode);
      return -2;
   }

   return 0;
}

void cb_free (circular_buffer_t *cb)
{
   pthread_mutex_destroy(&(cb->mutex));

   int returnCode = pthread_cond_destroy(&(cb->cb_nonempty_cond));
   if (returnCode != 0)
   {
      LOG_ERROR_ARGS ("EBPSreamBuffer: cb_free: Error %d calling pthread_cond_destroy", returnCode);
      reportAddErrorLogArgs ("EBPSreamBuffer: cb_free: Error %d calling pthread_cond_destroy", returnCode);
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
      reportAddErrorLogArgs ("EBPSreamBuffer: cb_read_or_peek: pthread_mutex_lock failed: %d", returnCode);
      return -2;
   }

   int usedSz = 0;
   if (isPeek)
   {
      usedSz = cb_peek_size_internal (cb);
   }
   else
   {
      usedSz = cb_read_size_internal (cb);
   }

   if (usedSz == 0)
   {
      if (cb->disabled)
      {
         // unlock mutex before returning
         pthread_mutex_unlock (&(cb->mutex));
         return -99;
      }

      returnCode = pthread_cond_wait(&(cb->cb_nonempty_cond), &(cb->mutex));
      if (returnCode != 0)
      {
         // unlock mutex before returning
         LOG_ERROR_ARGS ("EBPSreamBuffer: cb_read_or_peek: pthread_cond_wait failed: %d", returnCode);
         reportAddErrorLogArgs ("EBPSreamBuffer: cb_read_or_peek: pthread_cond_wait failed: %d", returnCode);
         pthread_mutex_unlock (&(cb->mutex));
         return -1;
      }

      if (isPeek)
      {
         usedSz = cb_peek_size_internal (cb);
      }
      else
      {
         usedSz = cb_read_size_internal (cb);
      }
   }

   int sizeToCopy = usedSz;
   if (usedSz > bytesSz)
   {
      sizeToCopy = bytesSz;
   }

   uint8_t *startPtr = 0;
   if (isPeek)
   {
      startPtr = cb->peekPtr;
   }
   else
   {
      startPtr = cb->readPtr;
   }

  // uint8_t *readPtr_Original = cb->readPtr;
 //  LOG_INFO_ARGS ("cb_read_or_peek: isPeek = %d, startPtr = 0x%x, sizeToCopy = %d", isPeek, (unsigned int)startPtr, sizeToCopy);

   int sz1 = cb->buf + cb->bufSz - startPtr;
   if (sz1 > sizeToCopy)
   {
      sz1 = sizeToCopy;
   }
   int sz2 = sizeToCopy - sz1;
//   LOG_INFO_ARGS ("cb_read_or_peek: sz1 = %d, sz2= %d", sz1, sz2);

   memcpy (bytes, startPtr, sz1);
   startPtr += sz1;
   if (startPtr >= cb->buf + cb->bufSz)
   {
      startPtr = cb->buf;
      if (isPeek)
      {
         cb->peekBufferTraversals++;
 //        printf ("peekBufferTraversals incrementing\n");
      }
      else
      {
         cb->readBufferTraversals++;
 //        printf ("readBufferTraversals incrementing\n");
      }
   }

   memcpy (bytes + sz1, startPtr, sz2);
   startPtr += sz2;

   if (isPeek)
   {
      cb->peekPtr = startPtr;
   }
   else
   {
      cb->readPtr = startPtr;

      // read automatically resets peeks
      cb->peekPtr = startPtr;
      cb->peekBufferTraversals = cb->readBufferTraversals;
   }
//   LOG_INFO_ARGS ("cb_read_or_peek: ending cb->readPtr = 0x%x", (unsigned int)cb->readPtr);

   returnCode = pthread_mutex_unlock (&(cb->mutex));
   if (returnCode != 0)
   {
      LOG_ERROR_ARGS ("EBPSreamBuffer: cb_read_or_peek: pthread_mutex_unlock failed: %d", returnCode);
      reportAddErrorLogArgs ("EBPSreamBuffer: cb_read_or_peek: pthread_mutex_unlock failed: %d", returnCode);
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
      reportAddErrorLogArgs ("EBPSreamBuffer: cb_write: pthread_mutex_lock failed: %d", returnCode);
      return -2;
   }
      
   if (cb->disabled)
   {
      // unlock mutex before returning
      pthread_mutex_unlock (&(cb->mutex));
      return -99;
   }

   int availableSz = cb->bufSz - cb_read_size_internal (cb);
//   LOG_INFO_ARGS ("EBPSreamBuffer: cb_write: cb->bufSz = %d, cb_read_size_internal (cb) = %d, availableSz = %d", 
//      cb->bufSz, cb_read_size_internal (cb), availableSz);

   if (bytesSz > availableSz)
   {
      LOG_ERROR_ARGS ("EBPSreamBuffer: cb_write: requested sz %d greater than available sz %d", bytesSz, availableSz);
      reportAddErrorLogArgs ("EBPSreamBuffer: cb_write: requested sz %d greater than available sz %d", bytesSz, availableSz);

      returnCode = pthread_mutex_unlock (&(cb->mutex));
      if (returnCode != 0)
      {
         LOG_ERROR_ARGS ("EBPSreamBuffer: cb_write: pthread_mutex_unlock failed: %d", returnCode);
         reportAddErrorLogArgs ("EBPSreamBuffer: cb_write: pthread_mutex_unlock failed: %d", returnCode);
         return -2;
      }

      return -1;
   }

 //  LOG_INFO_ARGS ("cb_write: cb->writePtr = 0x%x, bytesSz = %d", (unsigned int)cb->writePtr, bytesSz);

   int sz1 = cb->buf + cb->bufSz - cb->writePtr;
   if (sz1 > bytesSz)
   {
      sz1 = bytesSz;
   }
   int sz2 = bytesSz - sz1;
//   LOG_INFO_ARGS ("cb_write: sz1 = %d, sz2 = %d", sz1, sz2);

   memcpy (cb->writePtr, bytes, sz1);
   cb->writePtr += sz1;
   if (cb->writePtr >= cb->buf + cb->bufSz)
   {
      cb->writePtr = cb->buf;
      cb->writeBufferTraversals++;
 //     printf ("writeBufferTraversals incrementing\n");

   }

   memcpy (cb->writePtr, bytes + sz1, sz2);
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
      reportAddErrorLogArgs ("EBPSreamBuffer: cb_write: pthread_mutex_unlock failed: %d", returnCode);
      return -2;
   }

   return 0;
}

int cb_read_size (circular_buffer_t *cb)
{
   int returnCode = pthread_mutex_lock (&(cb->mutex));
   if (returnCode != 0)
   {
      LOG_ERROR_ARGS ("EBPSreamBuffer: cb_read_size: pthread_mutex_lock failed: %d", returnCode);
      reportAddErrorLogArgs ("EBPSreamBuffer: cb_read_size: pthread_mutex_lock failed: %d", returnCode);
      return -2;
   }

   int size = cb_read_size_internal (cb);

//      printf ("cb->readPtr = %x, cb->writePtr = %x size = %d\n", (unsigned int)cb->readPtr, (unsigned int)cb->writePtr, size);

   returnCode = pthread_mutex_unlock (&(cb->mutex));
   if (returnCode != 0)
   {
      LOG_ERROR_ARGS ("EBPSreamBuffer: cb_read_size: pthread_mutex_unlock failed: %d", returnCode);
      reportAddErrorLogArgs ("EBPSreamBuffer: cb_read_size: pthread_mutex_unlock failed: %d", returnCode);
      return -2;
   }

   return size;
}

int cb_available_write_size (circular_buffer_t *cb)
{
   return cb->bufSz - cb_read_size (cb);
}

int cb_get_total_size (circular_buffer_t *cb)
{
   return cb->bufSz;
}

int cb_read_size_internal (circular_buffer_t *cb)
{
   // WARNING: THIS FUNCTION DOES NOT LOCK THE MUTEX AND SHOULD ONLY
   // BE CALLED BY A FUNCTION THAT HAS ALREADY LOCKED IT

   return cb->bufSz * (cb->writeBufferTraversals - cb->readBufferTraversals) + cb->writePtr - cb->readPtr;
}

int cb_peek_size_internal (circular_buffer_t *cb)
{
   // WARNING: THIS FUNCTION DOES NOT LOCK THE MUTEX AND SHOULD ONLY
   // BE CALLED BY A FUNCTION THAT HAS ALREADY LOCKED IT

   return cb->bufSz * (cb->writeBufferTraversals - cb->peekBufferTraversals) + cb->writePtr - cb->peekPtr;
}

