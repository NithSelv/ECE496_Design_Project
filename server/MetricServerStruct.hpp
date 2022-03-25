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
   int _sockfd;

public:
   // These are the enums for all the return codes that our server can return
   enum ReturnCodes {success = 0, invalidPort = -1, socketCreationFailed = -2, socketOptionFailed = -3, bindingFailed = -4, listeningFailed = -5};
   // Initialize the server values except for the sockfd
   // This works for IPV4, may need to change it to adjust for IPV6
   Server(int port);
   int serverStart();
   // Get the sockfd
   int serverGetSockfd();
   // Clear all the data structures and close the connection if not already closed
   void serverClear();
   void serverClose();
   };
