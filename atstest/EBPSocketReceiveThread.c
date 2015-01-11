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
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include "log.h"
#include "EBPSocketReceiveThread.h"
#include "EBPThreadLogging.h"
#include "ts.h"

void *EBPSocketReceiveThreadProc(void *threadParams)
{
   int returnCode = 0;

   ebp_socket_receive_thread_params_t * ebpSocketReceiveThreadParams = (ebp_socket_receive_thread_params_t *)threadParams;
   LOG_INFO_ARGS("EBPSocketReceiveThread %d starting...ebpSocketReceiveThreadParams->port = %d", 
      ebpSocketReceiveThreadParams->threadNum, ebpSocketReceiveThreadParams->port);

   int num_bytes = 0;
   int ts_buf_sz = ebpSocketReceiveThreadParams->cb->bufSz;
   int num_packets = ts_buf_sz / TS_SIZE;
   if (ts_buf_sz % TS_SIZE != 0)
   {
      LOG_ERROR_ARGS("EBPSocketReceiveThread %d: Buffer not integral number of TS packets", 
         ebpSocketReceiveThreadParams->threadNum);
      return NULL;
   }

   uint8_t *ts_buf = malloc(ts_buf_sz);

   int total_packets = 0;


   // create socket
   LOG_INFO_ARGS("EBPSocketReceiveThread %d: Creating socket...", ebpSocketReceiveThreadParams->threadNum);
   int mySocket = socket(AF_INET, SOCK_DGRAM, 0);
   if (mySocket < 0)
   {
      LOG_ERROR_ARGS("EBPSocketReceiveThread %d: Error creating socket: %s", 
         ebpSocketReceiveThreadParams->threadNum, strerror(errno));
      return NULL;
   }

   LOG_INFO_ARGS("EBPSocketReceiveThread %d: Binding socket...", ebpSocketReceiveThreadParams->threadNum);
   // bind to loacal addr
	struct sockaddr_in myAddr;
   memset((void *)&myAddr, 0, sizeof(myAddr));
	myAddr.sin_family = AF_INET;
	myAddr.sin_addr.s_addr = htonl(ebpSocketReceiveThreadParams->ipAddr);
	myAddr.sin_port = htons(ebpSocketReceiveThreadParams->port);

	returnCode = bind(mySocket, (struct sockaddr *)&myAddr, sizeof(myAddr));
   if (returnCode < 0)
   {
      LOG_ERROR_ARGS("EBPSocketReceiveThread %d: Error binding socket: %s", 
         ebpSocketReceiveThreadParams->threadNum, strerror(errno));
      return NULL;
	}

   fd_set rfds;
   struct timeval tv;
   int retval;

   tv.tv_sec = 1;
   tv.tv_usec = 0;

   while (!ebpSocketReceiveThreadParams->stopFlag)
   {
      // only ask for the number of bytes for which there is space
      int availableSpace = cb_available (ebpSocketReceiveThreadParams->cb);

      LOG_INFO_ARGS("EBPSocketReceiveThread %d: availableSpace = %d", ebpSocketReceiveThreadParams->threadNum,
         availableSpace);


      FD_ZERO(&rfds);
      FD_SET(mySocket, &rfds);

      returnCode = select(mySocket + 1, &rfds, NULL, NULL, &tv);

      if (returnCode == -1)
      {
         LOG_ERROR_ARGS("EBPSocketReceiveThread %d: Error receiving from socket: %s", 
            ebpSocketReceiveThreadParams->threadNum, strerror(errno));
         return NULL;
      }
      else if (returnCode == 0)
      {
         continue;
      }

      LOG_INFO_ARGS("EBPSocketReceiveThread %d: Receiving...", ebpSocketReceiveThreadParams->threadNum);
      returnCode = recv(mySocket, ts_buf, availableSpace, 0 /* flags */);
      if (returnCode < 0)
      {
         LOG_ERROR_ARGS("EBPSocketReceiveThread %d: Error receiving from socket: %s", 
            ebpSocketReceiveThreadParams->threadNum, strerror(errno));
         break;
      }

      LOG_INFO_ARGS ("EBPSocketReceiveThread %d: Received %d bytes", ebpSocketReceiveThreadParams->threadNum, returnCode);
      //      if (returnCode%TS_SIZE != 0)
      if (returnCode%17 != 0)  // GORP
      {
         LOG_ERROR_ARGS("EBPSocketReceiveThread %d: Received bytes not integral number of TS packets", 
            ebpSocketReceiveThreadParams->threadNum);
         return NULL;
      }

      LOG_INFO_ARGS ("EBPSocketReceiveThread %d: Writing %d bytes to circular buffer", ebpSocketReceiveThreadParams->threadNum, returnCode);
      int returnCodeTemp = cb_write (ebpSocketReceiveThreadParams->cb, ts_buf, returnCode);
      if (returnCodeTemp < 0)
      {
         LOG_ERROR_ARGS("EBPSocketReceiveThread %d: Error writing to crcular buffer", 
            ebpSocketReceiveThreadParams->threadNum);
         return NULL;
      }

      if (ebpSocketReceiveThreadParams->streamLogFile != NULL)
      {
         // GORP: log to file
      }
   }


   close (mySocket);

   pthread_exit(NULL);

   // NOTE: calling code frees ebpSocketReceiveThreadParams

   return NULL;
}


