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

#include "MetricClient.hpp"

int SSL_write(SSL *ssl, void* buf, int num);
int SSL_read(SSL *ssl, void*buf, int num);

// This private function allows us to set the timeout for receiving messages.
int Client::clientSetRecvTimeout(int timeout)
   {
   this->_recvTimer.tv_sec = 0;
   this->_recvTimer.tv_usec = timeout;
   if (setsockopt(this->_sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&(this->_recvTimer), sizeof(this->_recvTimer)) < 0)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Failed to set receive timeout!\n");
      perror("Metric Server: Failed to set timeout!");
      close(this->_sockfd);
      return Client::timeoutFailed;
      }
   return Client::success;
   }

// This private function allows us to set the timeout for sending messages.
int Client::clientSetSendTimeout(int timeout)
   {
   this->_sendTimer.tv_sec = 0;
   this->_sendTimer.tv_usec = timeout;
   if (setsockopt(this->_sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&(this->_sendTimer), sizeof(this->_sendTimer)) < 0)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Failed to set send timeout!\n");
      perror("Metric Server: Failed to set send timeout!");
      close(this->_sockfd);
      return Client::timeoutFailed;
      }
   return Client::success;
   }
// Set the initial sockfd to some invalid value
Client::Client()
   {
   this->_sockfd = -1;
   this->_recvBuffer[0] = '\0';
   this->_bio = NULL;
   this->_useSSL = 0;
   this->_sslCtx = NULL;
   }

// Use the functions below for ssl

bool Client::handleOpenSSLConnectionError(int connfd, SSL *&ssl, BIO *&bio, const char *errMsg) 
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

bool Client::acceptOpenSSLConnection(SSL_CTX *sslCtx, int connfd, BIO *&bio) 
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
int Client::clientAccept(int serverSock, SSL_CTX* sslCtx)
   {
   this->_sslCtx = sslCtx;
   this->_sockfd = accept(serverSock, (struct sockaddr *)&(this->_clientAddr), (socklen_t *)&(this->_clientAddrSize));
   if (this->_sockfd < 0)
      {
         if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
            {
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Failed to connect to client!\n");
            perror("Metric Server: Failed to connect to client!");
            }
         else
            {
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Connection request timed out!\n");
            perror("Metric Server: Connection timed out!");
            }
         return Client::acceptFailed;
      }
   if (sslCtx != NULL) 
      {
         if (!acceptOpenSSLConnection(sslCtx, this->_sockfd, this->_bio))
         {
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Failed to connect with SSL!\n");
            perror("Metric Server: Failed to connect to client using SSL!");
            return Client::acceptFailed;
         }
         else 
         {
            this->_useSSL = 1;
         }
      }
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Client connection was accepted!\n");
   return Client::success;
   }
// Add a timeout for receving messages and store the received message in the buffer
int Client::clientReceive(int timeout)
   {
   int totalBytes = 0;
   int numBytes = 0;
   this->clientSetRecvTimeout(timeout);
   memset(this->_recvBuffer, 0, sizeof(this->_recvBuffer));
   // We aren't using SSL
   if (this->_useSSL == 0)
      {
      numBytes = recv(this->_sockfd, this->_recvBuffer, sizeof(this->_recvBuffer)-1, 0);
      }
   // We are using SSL
   else
      {
      
      numBytes = (*OBIO_read)(this->_bio, this->_recvBuffer, sizeof(this->_recvBuffer)-1);
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
         numBytes = (*OBIO_read)(this->_bio, &(this->_recvBuffer[totalBytes]), sizeof(this->_recvBuffer)-totalBytes-1);
         }
      }
   if (!((numBytes == -1) && ((errno == EAGAIN)||(errno == EWOULDBLOCK))))
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Receive failed!\n");
      perror("Metric Server: Failed to receive msg!");
      close(this->_sockfd);
      return Client::receiveFailed;
      }
   if (totalBytes == 0) 
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Client killed connection or connection timed out!\n");
      close(this->_sockfd);
      return Client::receiveFailed;
      }
   this->_recvBuffer[totalBytes] = '\0';
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Client msg received!\n");
   return Client::success;
   }
// Add a timeout for sending messages and send it
int Client::clientSend(std::vector<char> sendBuffer, int timeout)
   {
   int totalBytes = 0;
   int numBytes = 0;
   this->clientSetSendTimeout(timeout);
   // Don't use SSL
   if (this->_useSSL == 0)
      {
      numBytes = send(this->_sockfd, &(sendBuffer[0]), sendBuffer.size(), 0);
      }
   // Use SSL
   else
      {
      numBytes = (*OBIO_write)(this->_bio, &(sendBuffer[0]), sendBuffer.size());
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
         numBytes = (*OBIO_write)(this->_bio, &(sendBuffer[totalBytes]), sendBuffer.size()-totalBytes);
         }
      }
   if ((numBytes < 0) && !((errno == EAGAIN)||(errno == EWOULDBLOCK)))
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Send failed!\n");
      perror("Metric Server: Failed to send msg!");
      close(this->_sockfd);
      return Client::sendFailed;
      }
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Client msg sent!\n");
   return Client::success;
   }
// Return the received message 
char* Client::clientGetRecvMsg() 
   {
   return this->_recvBuffer;
   }
// Return client sockfd
int Client::clientGetSockfd() 
   {
   return this->_sockfd;
   }
// Clear the client structure for reuse
void Client::clientClear() 
   {
   if (this->_sockfd > 0)
      this->clientClose();
   this->_bio = NULL;
   this->_sslCtx = NULL;
   this->_useSSL = 0;
   memset(this->_recvBuffer, 0, sizeof(this->_recvBuffer));
   memset((void*)&(this->_clientAddr), 0, sizeof(this->_clientAddr));
   this->_clientAddrSize = 0;
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Clearing client info!\n");
   }
// Remember to close the connection once finished
// Structures go out of scope so no need for memory cleanup
void Client::clientClose()
   {
   close(this->_sockfd);
   this->_sockfd = -1;
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Closed client fd!\n");
   }
