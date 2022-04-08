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
#include "net/ServerStream.hpp"
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
   int clientSetRecvTimeout(int timeout);

   // This private function allows us to set the timeout for sending messages.
   int clientSetSendTimeout(int timeout);

public:
   // Some enums for error codes
   enum ReturnCodes {success = 0, acceptFailed = -1, timeoutFailed = -2, receiveFailed = -3, sendFailed = -4};
   // Set the initial sockfd to some invalid value
   Client();

   // Use the functions below for ssl

   bool handleOpenSSLConnectionError(int connfd, SSL *&ssl, BIO *&bio, const char *errMsg);

   bool acceptOpenSSLConnection(SSL_CTX *sslCtx, int connfd, BIO *&bio);

   // Accept the connection and populate the client structures
   int clientAccept(int serverSock, SSL_CTX* sslCtx);
   // Add a timeout for receving messages and store the received message in the buffer
   int clientReceive(int timeout);
   // Add a timeout for sending messages and send it
   int clientSend(std::vector<char> sendBuffer, int timeout);
   // Return the received message 
   char* clientGetRecvMsg();
   // Return client sockfd
   int clientGetSockfd();
   // Clear the client structure for reuse
   void clientClear();
   // Remember to close the connection once finished
   // Structures go out of scope so no need for memory cleanup
   void clientClose();
   };

