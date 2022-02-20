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
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

// This class is used to handle the server communications
class Server
   {
private:
   struct sockaddr_in _serverAddr;
   int _port;
   int _numConnections;
   int _sockfd;

public:
   // These are the enums for all the return codes that our server can return
   enum ReturnCodes {success = 0, invalidPort = -1, invalidNumConnections = -2, socketCreationFailed = -3, bindingFailed = -4, listeningFailed = -5};
   // Initialize the server values except for the sockfd
   // This works for IPV4, may need to change it to adjust for IPV6
   Server(int port, int numConnections)
      {
      this->_port = port;
      this->_numConnections = numConnections;
      this->_sockfd = -1;
      this->_serverAddr.sin_port = htons(port);
      this->_serverAddr.sin_family = AF_INET;
      this->_serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
      memset(_serverAddr.sin_zero, 0, 8);
      }
   // Check to make sure the input is valid and then begin to create the socket, bind it, and set it to listen
   int serverStart()
      {
      // Make sure we are listening on a valid port number
      if ((this->_port < 1024) || (this->_port > 65535))
         {
         std::cout << "Invalid port number!" << std::endl;
         return Server::invalidPort;
         }
      if ((this->_numConnections < 1) || (this->_numConnections > 5))
         {
         std::cout << "Invalid number of connections!" << std::endl;
         return Server::invalidNumConnections;
         }
      // Create the socket
      this->_sockfd = socket(AF_INET, SOCK_STREAM, 0);
      if (this->_sockfd < 0)
         {
         std::cout << "Socket Creation Failed!" << std::endl;
         return Server::socketCreationFailed;
         }
      // Bind it to the server
      if (bind(this->_sockfd, (struct sockaddr *)&(this->_serverAddr), (socklen_t)sizeof(this->_serverAddr)) != 0)
         {
         std::cout << "Socket Binding Failed!" << std::endl;
         close(this->_sockfd);
         return Server::bindingFailed;
         }
      // Start listening
      if (listen(this->_sockfd, this->_numConnections) < 0)
         {
         std::cout << "Socket Failed to Listen!" << std::endl;
         close(this->_sockfd);
         return Server::listeningFailed;
         }
      return Server::success;
      }
   // Get the sockfd
   int serverGetSockfd()
      {
      return this->_sockfd;
      }
   };
