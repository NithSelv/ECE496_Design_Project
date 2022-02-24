/*******************************************************************************
 * Copyright (c) 2000, 2022 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

// This class is used to handle the client communications
class Client
   {
// These are private variables and functions used to manipulate send and recv buffers
private:
   int _sockfd;
   struct sockaddr_storage _clientAddr;
   struct timeval _recvTimer;
   struct timeval _sendTimer;
   socklen_t _clientAddrSize;
   char _recvBuffer[4096];

   // This private function allows us to set the timeout for receiving messages.
   int clientSetRecvTimeout()
      {
      if (setsockopt(this->_sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&(this->_recvTimer), sizeof(this->_recvTimer)) < 0)
         {
         perror("Metric Server: Failed to set timeout!");
         close(this->_sockfd);
         return Client::timeoutFailed;
         }
      return Client::success;
      }

   // This private function allows us to set the timeout for sending messages.
   int clientSetSendTimeout()
      {
      if (setsockopt(this->_sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&(this->_sendTimer), sizeof(this->_sendTimer)) < 0)
         {
         perror("Metric Server: Failed to set timeout!");
         close(this->_sockfd);
         return Client::timeoutFailed;
         }
      return Client::success;
      }

public:
   // Some enums for error codes
   enum ReturnCodes {success = 0, acceptFailed = -1, timeoutFailed = -2, receiveFailed = -3, sendFailed = -4};
   // Set the initial sockfd to some invalid value
   Client(int timeout_sec, int timeout_usec)
      {
      this->_sockfd = -1;
      this->_recvBuffer[0] = '\0';
      this->_sendTimer.tv_sec = timeout_sec;
      this->_sendTimer.tv_usec = timeout_usec;
      this->_recvTimer.tv_sec = timeout_sec;
      this->_recvTimer.tv_usec = timeout_usec;
      }
   // Accept the connection and populate the client structures
   int clientAccept(int serverSock)
      {
      this->_sockfd = accept(serverSock, (struct sockaddr *)&(this->_clientAddr), (socklen_t *)&(this->_clientAddrSize));
      if (this->_sockfd < 0)
         {
         perror("Metric Server: Failed to connect to client!");
         return Client::acceptFailed;
         }
      return Client::success;
      }
   // Add a timeout for receving messages and store the received message in the buffer
   int clientReceive()
      {
      int totalBytes = 0;
      this->clientSetRecvTimeout();
      memset(this->_recvBuffer, 0, sizeof(this->_recvBuffer));
      int numBytes = recv(this->_sockfd, this->_recvBuffer, sizeof(this->_recvBuffer)-1, 0);
      while (numBytes > 0)
         {
         totalBytes += numBytes;
         if (totalBytes >= 4095)
            {
            numBytes = -1;
            continue;
            }
         numBytes = recv(this->_sockfd, &(this->_recvBuffer[totalBytes]), sizeof(this->_recvBuffer)-totalBytes-1, 0);
         }
      if (!((numBytes == -1) && ((errno == EAGAIN)||(errno == EWOULDBLOCK))))
         {
         perror("Metric Server: Failed to receive msg!");
         close(this->_sockfd);
         return Client::receiveFailed;
         }
      if (totalBytes == 0) 
         {
         close(this->_sockfd);
         return Client::receiveFailed;
         }
      this->_recvBuffer[totalBytes] = '\0';
      return Client::success;
      }
   // Add a timeout for sending messages and send it
   int clientSend(std::vector<char> sendBuffer)
      {
      int totalBytes = 0;
      this->clientSetSendTimeout();
      int numBytes = send(this->_sockfd, &(sendBuffer[0]), sendBuffer.size(), 0);
      while (numBytes > 0)
         {
         totalBytes += numBytes;
         numBytes = send(this->_sockfd, &(sendBuffer[totalBytes]), sendBuffer.size()-totalBytes, 0);
         }
      if ((numBytes < 0) && !((errno == EAGAIN)||(errno == EWOULDBLOCK)))
         {
         perror("Metric Server: Failed to send msg!");
         close(this->_sockfd);
         return Client::sendFailed;
         }
      
      return Client::success;
      }
   // Return the received message 
   char* clientGetRecvMsg() {
   return this->_recvBuffer;
   }
   // Return client sockfd
   int clientGetSockfd() {
   return this->_sockfd;
   }
   // Remember to close the connection once finished
   // Structures go out of scope so no need for memory cleanup
   void clientClose()
      {
      close(this->_sockfd);
      }
   };
