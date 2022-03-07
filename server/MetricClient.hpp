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
#include "j9.h"
#include "control/CompilationRuntime.hpp"
#include "env/TRMemory.hpp"
#include "env/VMJ9.h"
#include "env/VerboseLog.hpp"
#include <openssl/bio.h>
#include <openssl/ssl.h>

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
   BIO* _bio;
   SSL_CTX* _sslCtx;
   int _useSSL;

   // This private function allows us to set the timeout for receiving messages.
   int clientSetRecvTimeout(int timeout)
      {
      this->_recvTimer.tv_sec = 0;
      this->_recvTimer.tv_usec = timeout;
      if (setsockopt(this->_sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&(this->_recvTimer), sizeof(this->_recvTimer)) < 0)
         {
         perror("Metric Server: Failed to set timeout!");
         close(this->_sockfd);
         return Client::timeoutFailed;
         }
      return Client::success;
      }

   // This private function allows us to set the timeout for sending messages.
   int clientSetSendTimeout(int timeout)
      {
      this->_sendTimer.tv_sec = 0;
      this->_sendTimer.tv_usec = timeout;
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
   Client()
      {
      this->_sockfd = -1;
      this->_recvBuffer[0] = '\0';
      this->_bio = NULL;
      this->_useSSL = 0;
      this->_sslCtx = NULL;
      }

   // Use the functions below for ssl

   static bool handleOpenSSLConnectionError(int connfd, SSL *&ssl, BIO *&bio, const char *errMsg) 
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
          TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "%s: errno=%d", errMsg, errno);
      (*OERR_print_errors_fp)(stderr);

      close(connfd);
      if (bio)
         {
         (*OBIO_free_all)(bio);
         bio = NULL;
         }
      return false;
      }

   static bool acceptOpenSSLConnection(SSL_CTX *sslCtx, int connfd, BIO *&bio) 
      {
      SSL *ssl = NULL;
      bio = (*OBIO_new_ssl)(sslCtx, false);
      if (!bio)
         return handleOpenSSLConnectionError(connfd, ssl, bio, "Error creating new BIO");

      if ((*OBIO_ctrl)(bio, BIO_C_GET_SSL, false, (char *) &ssl) != 1) // BIO_get_ssl(bio, &ssl)
         return handleOpenSSLConnectionError(connfd, ssl, bio, "Failed to get BIO SSL");

      if ((*OSSL_set_fd)(ssl, connfd) != 1)
         return handleOpenSSLConnectionError(connfd, ssl, bio, "Error setting SSL file descriptor");

      if ((*OSSL_accept)(ssl) <= 0)
         return handleOpenSSLConnectionError(connfd, ssl, bio, "Error accepting SSL connection");

      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "SSL connection on socket 0x%x, Version: %s, Cipher: %s\n",
                                                     connfd, (*OSSL_get_version)(ssl), (*OSSL_get_cipher)(ssl));
      return true;
      }

   // Accept the connection and populate the client structures
   int clientAccept(int serverSock, SSL_CTX* sslCtx)
      {
      this->_sslCtx = sslCtx;
      this->_sockfd = accept(serverSock, (struct sockaddr *)&(this->_clientAddr), (socklen_t *)&(this->_clientAddrSize));
      if (this->_sockfd < 0)
         {
            if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
               {
               perror("Metric Server: Failed to connect to client!");
               }
            else
               {
               perror("Metric Server: Connection timed out!");
               }
            return Client::acceptFailed;
         }
      if (sslCtx != NULL) 
         {
            if (!acceptOpenSSLConnection(sslCtx, this->_sockfd, this->_bio))
            {
               perror("Metric Server: Failed to connect to client using SSL!");
               return Client::acceptFailed;
            }
            else 
            {
               this->_useSSL = 1;
            }
         }
      return Client::success;
      }
   // Add a timeout for receving messages and store the received message in the buffer
   int clientReceive(int timeout)
      {
      int totalBytes = 0;
      int numBytes = 0;
      this->clientSetRecvTimeout(timeout);
      memset(this->_recvBuffer, 0, sizeof(this->_recvBuffer));
      if (this->_useSSL == 1)
         SSL_CTX_set_mode(this->_sslCtx, SSL_MODE_AUTO_RETRY); 
      // We aren't using SSL
      if (this->_useSSL == 0)
         {
         numBytes = recv(this->_sockfd, this->_recvBuffer, sizeof(this->_recvBuffer)-1, 0);
         }
      // We are using SSL
      else
         {
         SSL* ssl = NULL;
         BIO_get_ssl(this->_bio, ssl);
         numBytes = SSL_read(ssl, this->_recvBuffer, sizeof(this->_recvBuffer)-1);
         }
         
      while (numBytes > 0)
         {
         totalBytes += numBytes;
         if (totalBytes >= 4095)
            {
            numBytes = -1;
            continue;
            }
         if (this->_useSSL == 0)
            {
            numBytes = recv(this->_sockfd, &(this->_recvBuffer[totalBytes]), sizeof(this->_recvBuffer)-totalBytes-1, 0);
            }
         else 
            {
            SSL* ssl = NULL;
            BIO_get_ssl(this->_bio, ssl);
            numBytes = SSL_read(ssl, &(this->_recvBuffer[totalBytes]), sizeof(this->_recvBuffer)-totalBytes-1);
            }
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
   int clientSend(std::vector<char> sendBuffer, int timeout)
      {
      int totalBytes = 0;
      int numBytes = 0;
      this->clientSetSendTimeout(timeout);
      if (this->_useSSL == 1)
         SSL_CTX_set_mode(this->_sslCtx, SSL_MODE_AUTO_RETRY);
      // Don't use SSL
      if (this->_useSSL == 0)
         {
         numBytes = send(this->_sockfd, &(sendBuffer[0]), sendBuffer.size(), 0);
         }
      // Use SSL
      else
         {
         SSL* ssl = NULL;
         BIO_get_ssl(this->_bio, ssl);
         numBytes = SSL_write(ssl, &(sendBuffer[0]), sendBuffer.size());
         }

      while (numBytes > 0)
         {
         totalBytes += numBytes;
         if (this->_useSSL == 0)
            {
            numBytes = send(this->_sockfd, &(sendBuffer[totalBytes]), sendBuffer.size()-totalBytes, 0);
            }
         else 
            {
            SSL* ssl = NULL;
            BIO_get_ssl(this->_bio, ssl);
            numBytes = SSL_write(ssl, &(sendBuffer[totalBytes]), sendBuffer.size()-totalBytes);
            }
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
   // Clear the client structure for reuse
   void clientClear() {
   if (this->_sockfd > 0)
      this->clientClose();
   this->_bio = NULL;
   this->_sslCtx = NULL;
   this->_useSSL = 0;
   memset(this->_recvBuffer, 0, sizeof(this->_recvBuffer));
   memset((void*)this->_clientAddr, 0, sizeof(this->_clientAddr));
   this->_clientAddrSize = 0;
   }
   // Remember to close the connection once finished
   // Structures go out of scope so no need for memory cleanup
   void clientClose()
      {
      close(this->_sockfd);
      this->_sockfd = -1;
      }
   };

