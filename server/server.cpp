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
#include <sys/poll.h>
#include <netinet/in.h>
#include <time.h>
#include <vector>
#include "metrics_database.h"
#include "http_request.h"
#include "http_response.h"
#include "client.h"
#include "server.h"

// These constants represent the various return codes that our main function can return.
enum Codes {success = 0, tooFewArgs = -1, tooManyArgs = -2, databaseFailed = -3, serverFailed = -4};

std::vector<char> httpErrorCheck(HttpRequest* req, MetricsDatabase* db)
   {
   HttpResponse rep;
   int type = req->getType();
   std::string metric(req->getMetric());

   if (type != HttpRequest::httpGet)
      {
      rep.addHeaderField("", "HTTP/1.1 400 Bad Request");
      rep.addBody("");
      return rep.prepareHttpResponse();
      }

   if (metric.find("/metrics\0") == std::string::npos)
      {
      rep.addHeaderField("", "HTTP/1.1 404 Not Found");
      rep.addBody("");
      return rep.prepareHttpResponse();
      }

   rep.addHeaderField("", "HTTP/1.1 200 OK");
   rep.addHeaderField("Content-Type", "text/plain; version=0.0.4; charset=utf-8");
   rep.addBody(db->prepareAllMetricsBody());
   return rep.prepareHttpResponse();
   }

// This is the main function that runs the server code, it takes in 3 arguments
// 1: port number
// 2: number of connections that can be handled
// 3: how long the server should wait for a connection's request before timing out (sec)
int main(int argc, char* argv[])
   {
   // First check to make sure that we have all 3 required arguments
   if (argc < 4)
      {
      std::cout << "FAILED: Not enough arguments" << std::endl;
      return tooFewArgs;
      }
   else if (argc > 4)
      {
      std::cout << "FAILED: Too many arguments" << std::endl;
      return tooManyArgs;
      }

   // TO BE REMOVED: This is just an object used to compute the calendar time metric
   // Once we can retrieve metrics from JITServer, we will remove this
   struct timeval start;
   gettimeofday(&start, NULL);
   // Create the database dynamically, we will be storing/adding/reading/deleting metrics from this object.
   MetricsDatabase* db = new MetricsDatabase();

   // Initialize the database with the initial metrics
   // (supposed to take in no arguments but will be fixed once integrated with JITServer)
   if (db->initialize(start.tv_sec + start.tv_usec * 0.000001) != 0)
      {
      std::cout << "FAILED: Unable to initialize database!" << std::endl;
      delete db;
      return databaseFailed;
      }

   // Assign the arguments to the appropiate variables, we will use them later
   int port = atoi(argv[1]);
   int numConnections = atoi(argv[2]);
   int timeout = atoi(argv[3]);

   // Start the server
   Server server = Server(port, numConnections);
   if (server.serverStart() < 0)
      {
      std::cout << "Server failed to start!" << std::endl;
      delete db;
      return serverFailed;
      }

   // Handle each connection by the following
   while (1)
      {

      // This object is used for server-client communications
      Client client;

      // Accept a new connection
      if (client.clientAccept(server.serverGetSockfd()) < 0)
         continue;

      int keepAlive = 1;
      while (keepAlive > 0)
         {
         // This object is used for handling the parsing of the HTTP Request
         HttpRequest req;

         // Update the database
         if (db->update(start.tv_sec + start.tv_usec * 0.000001) < 0)
            std::cout << "FAILED: database did not update!" << std::endl;

         // Receive the msg from the client/Prometheus
         int check = client.clientReceive(timeout);
         if (check < 0)
            {
            keepAlive = -1;
            break;
            }
         // Parse the std::string into the http request object
         char* data = client.clientGetRecvMsg();
         if (req.parse(data) < 0)
            {
            keepAlive = 0;
            break;
            }
         keepAlive = 0;
         const char* connection = req.getConnection();
         // Check if we need to keep this connection alive for another request
         if (strcmp(connection, "keep-alive") == 0)
            keepAlive = 1;

         std::vector<char> send_buffer = httpErrorCheck(&req, db);

         // Verify that the http request is valid and then
         // send back the response
         if (client.clientSend(send_buffer, timeout) < 0)
            {
            keepAlive = -1;
            break;
            }
         }
      // Close the client connection
      if (keepAlive == 0)
         client.clientClose();
      }
}
