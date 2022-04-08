/*******************************************************************************
 * Copyright (c) 2018, 2020 IBM Corp. and others
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
#include "MetricServerStruct.hpp"

// Initialize the server values except for the sockfd
// This works for IPV4, may need to change it to adjust for IPV6
Server::Server(int port)
   {
   this->_port = port;
   this->_sockfd = -1;
   this->_serverAddr.sin_port = htons(port);
   this->_serverAddr.sin_family = AF_INET;
   this->_serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
   memset(_serverAddr.sin_zero, 0, 8);
   }
// Check to make sure the input is valid and then begin to create the socket, bind it, and set it to listen
int Server::serverStart()
   {
   // Make sure we are listening on a valid port number
   if ((this->_port < 1024) || (this->_port > 65535))
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Invalid port number specified!\n");
      perror("Metric Server: Invalid port number!");
      return Server::invalidPort;
      }
   // Create the socket
   this->_sockfd = socket(AF_INET,  SOCK_STREAM | SOCK_NONBLOCK, 0);
   if (this->_sockfd < 0)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Failed to create server fd!\n");
      perror("Metric Server: Socket Creation Failed!");
      return Server::socketCreationFailed;
      }
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Server fd is open!\n");
   // Set some socket options for reuse and keep alive packets
   int flag = true;
   if (setsockopt(this->_sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&flag, sizeof(flag)) < 0)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Failed to set socket option: SO_REUSEADDR!\n");
      perror("Metric Server: Can't set SO_REUSEADDR!");
      close(this->_sockfd);
      this->_sockfd = -1;
      return Server::socketOptionFailed;
      }

   if (setsockopt(this->_sockfd, SOL_SOCKET, SO_KEEPALIVE, (void *)&flag, sizeof(flag)) < 0)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Failed to set socket option: SO_KEEPALIVE!\n");
      perror("Metric Server: Can't set SO_KEEPALIVE!");
      close(this->_sockfd);
      this->_sockfd = -1;
      return Server::socketOptionFailed;
      }

   // Bind it to the server
   if (bind(this->_sockfd, (struct sockaddr *)&(this->_serverAddr), (socklen_t)sizeof(this->_serverAddr)) != 0)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Failed to bind server fd!\n");
      perror("Metric Server: Socket Binding Failed!");
      close(this->_sockfd);
      this->_sockfd = -1;
      return Server::bindingFailed;
      }
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Server is bound!\n");
   // Start listening
   if (listen(this->_sockfd, SOMAXCONN) < 0)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Server failed to listen for incoming connections!\n");
      perror("Metric Server: Socket Failed to Listen!");
      close(this->_sockfd);
      this->_sockfd = -1;
      return Server::listeningFailed;
      }
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Server is bound and listening!\n");
   return Server::success;
   }
// Get the sockfd
int Server::serverGetSockfd()
   {
   return this->_sockfd;
   }
// Clear all the data structures and close the connection if not already closed
void Server::serverClear()
   {
   if (this->_sockfd > 0)
      this->serverClose();
   this->_port = -1;
   memset((void*)&(this->_serverAddr), 0, sizeof(this->_serverAddr));
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Cleared server info!\n");
   }
   
void Server::serverClose() 
   {
   if (this->_sockfd > 0)
      close(this->_sockfd);
   this->_sockfd = -1;
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Closing server fd!\n");
   }
