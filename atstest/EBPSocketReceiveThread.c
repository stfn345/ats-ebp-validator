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
#include <sys/types.h>
#include <arpa/inet.h>

#include "log.h"
#include "EBPSocketReceiveThread.h"
#include "EBPThreadLogging.h"
#include "ATSTestReport.h"

#include "ts.h"

static char *g_streamDumpBaseName = "EBPStreamDump";


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
      reportAddErrorLogArgs("EBPSocketReceiveThread %d: Buffer not integral number of TS packets", 
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
      reportAddErrorLogArgs("EBPSocketReceiveThread %d: Error creating socket: %s", 
         ebpSocketReceiveThreadParams->threadNum, strerror(errno));
      return NULL;
   }

   int isMulticast = ((ebpSocketReceiveThreadParams->ipAddr >> 28) == 14);

   LOG_INFO_ARGS("EBPSocketReceiveThread %d: Binding socket...ebpSocketReceiveThreadParams->ipAddr = %x, isMulticast = %d", 
      ebpSocketReceiveThreadParams->threadNum, (unsigned int)ebpSocketReceiveThreadParams->ipAddr, isMulticast);
   // bind to loacal addr
	struct sockaddr_in myAddr;
   memset((void *)&myAddr, 0, sizeof(myAddr));
	myAddr.sin_family = AF_INET;
	myAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myAddr.sin_port = htons(ebpSocketReceiveThreadParams->port);

	returnCode = bind(mySocket, (struct sockaddr *)&myAddr, sizeof(myAddr));
   if (returnCode < 0)
   {
      LOG_ERROR_ARGS("EBPSocketReceiveThread %d: Error binding socket: %s", 
         ebpSocketReceiveThreadParams->threadNum, strerror(errno));
      reportAddErrorLogArgs("EBPSocketReceiveThread %d: Error binding socket: %s", 
         ebpSocketReceiveThreadParams->threadNum, strerror(errno));
      return NULL;
	}


   int rcvBufSz = 2000000;
   if (setsockopt(mySocket, SOL_SOCKET, SO_RCVBUF, &rcvBufSz, sizeof(int)) < 0) 
   {
      LOG_ERROR_ARGS("EBPSocketReceiveThread %d: Error from setsockopt: %s", 
         ebpSocketReceiveThreadParams->threadNum, strerror(errno));
      reportAddErrorLogArgs("EBPSocketReceiveThread %d: Error from setsockopt: %s", 
         ebpSocketReceiveThreadParams->threadNum, strerror(errno));
      return NULL;
   }

   if (isMulticast)
   {
      if (ebpSocketReceiveThreadParams->srcipAddr != 0)
      {
         struct ip_mreq_source mreqsrc;

         /* use setsockopt() to request that the kernel join a multicast group */
         mreqsrc.imr_multiaddr.s_addr=htonl(ebpSocketReceiveThreadParams->ipAddr);
         mreqsrc.imr_interface.s_addr=htonl(INADDR_ANY);
         mreqsrc.imr_sourceaddr.s_addr=htonl(ebpSocketReceiveThreadParams->srcipAddr);
         if (setsockopt(mySocket, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP, &mreqsrc, sizeof(mreqsrc)) < 0)
         {
            LOG_ERROR_ARGS("EBPSocketReceiveThread %d: Error from setsockopt: %s",
               ebpSocketReceiveThreadParams->threadNum, strerror(errno));
            reportAddErrorLogArgs("EBPSocketReceiveThread %d: Error from setsockopt: %s",
               ebpSocketReceiveThreadParams->threadNum, strerror(errno));
            return NULL;
         }
      }
      else
      {
         struct ip_mreq mreq;
         /* use setsockopt() to request that the kernel join a multicast group */
         mreq.imr_multiaddr.s_addr=htonl(ebpSocketReceiveThreadParams->ipAddr);
         mreq.imr_interface.s_addr=htonl(INADDR_ANY);
         if (setsockopt(mySocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
         {
            LOG_ERROR_ARGS("EBPSocketReceiveThread %d: Error from setsockopt: %s",
               ebpSocketReceiveThreadParams->threadNum, strerror(errno));
            reportAddErrorLogArgs("EBPSocketReceiveThread %d: Error from setsockopt: %s",
               ebpSocketReceiveThreadParams->threadNum, strerror(errno));
            return NULL;
         }
      }
   }


   fd_set rfds;
   struct timeval tv;
   int retval;

   tv.tv_sec = 1;
   tv.tv_usec = 0;

   int totalTSPacketsReceived = 0;

   // open log file
   FILE *streamLogFileHandle = NULL;
   LOG_INFO_ARGS ("In EBPSocketReceiveThreadProc: ebpSocketReceiveThreadParams->enableStreamDump = %d", ebpSocketReceiveThreadParams->enableStreamDump);
   if (ebpSocketReceiveThreadParams->enableStreamDump)
   {
      printf ("Opening stream log file\n");
      // build stream dump file path here
      char streamLogFile[2048];
      if (getcwd(streamLogFile, 1024) == NULL)
      {
         // log error, skip stream dump

         LOG_ERROR_ARGS("EBPSocketReceiveThread %d: Error getting working dir: stream dump disabled", 
            ebpSocketReceiveThreadParams->threadNum);
         reportAddErrorLogArgs("EBPSocketReceiveThread %d: Error getting working dir: stream dump disabled", 
            ebpSocketReceiveThreadParams->threadNum);

         ebpSocketReceiveThreadParams->enableStreamDump = 0;
      }
      else
      {
         char temp[100];
         sprintf (temp, "%s_%u.%u.%u.%u_%d.ts", 
            g_streamDumpBaseName, 
            (unsigned int) (ebpSocketReceiveThreadParams->ipAddr >> 24),
            (unsigned int) ((ebpSocketReceiveThreadParams->ipAddr >> 16) & 0x0FF), 
            (unsigned int) ((ebpSocketReceiveThreadParams->ipAddr >> 8) & 0x0FF), 
            (unsigned int) ((ebpSocketReceiveThreadParams->ipAddr) & 0x0FF), 
            ebpSocketReceiveThreadParams->port);
         strcat (streamLogFile, "/");
         strcat (streamLogFile, temp);

         LOG_INFO_ARGS("EBPSocketReceiveThread %d: Opening streamLogFile %s", ebpSocketReceiveThreadParams->threadNum,
               streamLogFile);
         if ((streamLogFileHandle = fopen(streamLogFile, "wb")) == NULL)
         {
            LOG_ERROR_ARGS("EBPSocketReceiveThread %d: Error opening streamLogFile %s: %s", ebpSocketReceiveThreadParams->threadNum,
               streamLogFile, strerror(errno));
            reportAddErrorLogArgs("EBPSocketReceiveThread %d: Error opening streamLogFile %s", ebpSocketReceiveThreadParams->threadNum,
               streamLogFile, strerror(errno));
         }
      }
   }


   while (!ebpSocketReceiveThreadParams->stopFlag)
   {
      // only ask for the number of bytes for which there is space
      int availableSpace = cb_available_write_size (ebpSocketReceiveThreadParams->cb);

//      LOG_INFO_ARGS("EBPSocketReceiveThread %d: availableSpace = %d", ebpSocketReceiveThreadParams->threadNum,
//         availableSpace);


      FD_ZERO(&rfds);
      FD_SET(mySocket, &rfds);

      returnCode = select(mySocket + 1, &rfds, NULL, NULL, &tv);

      if (returnCode == -1)
      {
         LOG_ERROR_ARGS("EBPSocketReceiveThread %d: Error receiving from socket: %s", 
            ebpSocketReceiveThreadParams->threadNum, strerror(errno));
         reportAddErrorLogArgs("EBPSocketReceiveThread %d: Error receiving from socket: %s", 
            ebpSocketReceiveThreadParams->threadNum, strerror(errno));
         break;
      }
      else if (returnCode == 0)
      {
         if (cb_is_disabled (ebpSocketReceiveThreadParams->cb))
         {
            break;
         }

         continue;
      }

      LOG_INFO_ARGS("EBPSocketReceiveThread %d: Receiving...", ebpSocketReceiveThreadParams->threadNum);
      returnCode = recv(mySocket, ts_buf, availableSpace, 0 /* flags */);
      if (returnCode < 0)
      {
         LOG_ERROR_ARGS("EBPSocketReceiveThread %d: Error receiving from socket: %s", 
            ebpSocketReceiveThreadParams->threadNum, strerror(errno));
         reportAddErrorLogArgs("EBPSocketReceiveThread %d: Error receiving from socket: %s", 
            ebpSocketReceiveThreadParams->threadNum, strerror(errno));
         break;
      }

      LOG_INFO_ARGS ("EBPSocketReceiveThread %d: Received %d bytes", ebpSocketReceiveThreadParams->threadNum, returnCode);
      if (returnCode%TS_SIZE != 0)
      {
         LOG_ERROR_ARGS("EBPSocketReceiveThread %d: Received bytes not integral number of TS packets", 
            ebpSocketReceiveThreadParams->threadNum);
         reportAddErrorLogArgs("EBPSocketReceiveThread %d: Received bytes not integral number of TS packets", 
            ebpSocketReceiveThreadParams->threadNum);
         break;
      }

      ebpSocketReceiveThreadParams->receivedBytes += returnCode;

//      LOG_INFO_ARGS ("EBPSocketReceiveThread %d: Writing %d bytes to circular buffer", ebpSocketReceiveThreadParams->threadNum, returnCode);
      int returnCodeTemp = cb_write (ebpSocketReceiveThreadParams->cb, ts_buf, returnCode);
      if (returnCodeTemp == -99)
      {
         LOG_INFO_ARGS("EBPSocketReceiveThread %d: circular buffer disabled: exiting", 
            ebpSocketReceiveThreadParams->threadNum);
         break;
      }
      else if (returnCodeTemp < 0)
      {
         LOG_ERROR_ARGS("EBPSocketReceiveThread %d: Error writing to circular buffer", 
            ebpSocketReceiveThreadParams->threadNum);
         reportAddErrorLogArgs("EBPSocketReceiveThread %d: Error writing to circular buffer", 
            ebpSocketReceiveThreadParams->threadNum);
         break;
      }

      totalTSPacketsReceived += (returnCode / TS_SIZE);
//      LOG_INFO_ARGS("EBPSocketReceiveThread %d: Total TS packets: %d", 
//            ebpSocketReceiveThreadParams->threadNum, totalTSPacketsReceived);

      if (ebpSocketReceiveThreadParams->enableStreamDump)
      {
         // log to file
         size_t numBytesWritten = fwrite (ts_buf, 1, returnCode, streamLogFileHandle);
         if (numBytesWritten != returnCode)
         {
            LOG_ERROR_ARGS("EBPSocketReceiveThread %d: Error writing to log file", 
               ebpSocketReceiveThreadParams->threadNum);
            reportAddErrorLogArgs("EBPSocketReceiveThread %d: Error writing to log file", 
               ebpSocketReceiveThreadParams->threadNum);
         }
      }
   }

   close (mySocket);
   if (streamLogFileHandle != NULL)
   {
      fclose (streamLogFileHandle);
   }

   pthread_exit(NULL);

   // NOTE: calling code frees ebpSocketReceiveThreadParams

   return NULL;
}


