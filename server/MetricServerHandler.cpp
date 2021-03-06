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
#include <arpa/inet.h>
#include <chrono>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>	/* for TCP_NODELAY option */
#include <time.h>
#include <vector>
#include <openssl/err.h>
#include <sys/un.h>
//#include "control/Options.hpp"
#include "env/ClassLoaderTable.hpp"
#include "env/annotations/AnnotationBase.hpp"
#include "env/ut_j9jit.h"
#include "jvminit.h"
#include "net/LoadSSLLibs.hpp"
#include "MetricDatabase.hpp"
#include "MetricHttpRequest.hpp"
#include "MetricHttpResponse.hpp"
#include "MetricClient.hpp"
#include "MetricServerStruct.hpp"
#include "MetricServerHandler.hpp"
#include "control/CompilationRuntime.hpp"
#include "env/TRMemory.hpp"
#include "env/VMJ9.h"
#include "env/VerboseLog.hpp"
#include "net/CommunicationStream.hpp"
#include "net/ServerStream.hpp"
#include "runtime/CompileService.hpp"

static SSL_CTX * createSSLContext(TR::PersistentInfo *info) {
   SSL_CTX *ctx = (*OSSL_CTX_new)((*OSSLv23_server_method)());

   if (!ctx)
      {
      perror("can't create SSL context");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }

   const char *sessionIDContext = "JITServer";
   (*OSSL_CTX_set_session_id_context)(ctx, (const unsigned char*)sessionIDContext, strlen(sessionIDContext));

   if ((*OSSL_CTX_set_ecdh_auto)(ctx, 1) != 1)
      {
      perror("failed to configure SSL ecdh");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }

   TR::CompilationInfo *compInfo = TR::CompilationInfo::get();
   auto &sslKeys = compInfo->getJITServerSslKeys();
   auto &sslCerts = compInfo->getJITServerSslCerts();
   auto &sslRootCerts = compInfo->getJITServerSslRootCerts();

   TR_ASSERT_FATAL(sslKeys.size() == 1 && sslCerts.size() == 1, "only one key and cert is supported for now");
   TR_ASSERT_FATAL(sslRootCerts.size() == 0, "server does not understand root certs yet");

   // Parse and set private key
   BIO *keyMem = (*OBIO_new_mem_buf)(&sslKeys[0][0], sslKeys[0].size());
   if (!keyMem)
      {
      perror("cannot create memory buffer for private key (OOM?)");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }
   EVP_PKEY *privKey = (*OPEM_read_bio_PrivateKey)(keyMem, NULL, NULL, NULL);
   if (!privKey)
      {
      perror("cannot parse private key");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }
   if ((*OSSL_CTX_use_PrivateKey)(ctx, privKey) != 1)
      {
      perror("cannot use private key");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }

   // Parse and set certificate
   BIO *certMem = (*OBIO_new_mem_buf)(&sslCerts[0][0], sslCerts[0].size());
   if (!certMem)
      {
      perror("cannot create memory buffer for cert (OOM?)");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }
   X509 *certificate = (*OPEM_read_bio_X509)(certMem, NULL, NULL, NULL);
   if (!certificate)
      {
      perror("cannot parse cert");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }
   if ((*OSSL_CTX_use_certificate)(ctx, certificate) != 1)
      {
      perror("cannot use cert");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }

   // Verify key and cert are valid
   if ((*OSSL_CTX_check_private_key)(ctx) != 1)
      {
      perror("private key check failed");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }

   // verify server identity using standard method
   (*OSSL_CTX_set_verify)(ctx, SSL_VERIFY_PEER, NULL);

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Successfully initialized SSL context (%s)\n", (*OOpenSSL_version)(0));

   return ctx;
}

std::vector<char> httpErrorCheck(HttpRequest req, MetricDatabase db)
   {
   HttpResponse rep;
   int type = req.getType();
   std::string metric(req.getMetric());

   if (type != HttpRequest::httpGet)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: HTTP 400 Bad Request: Did not receive HTTP GET Request!\n");
      rep.addHeaderField("", "HTTP/1.1 400 Bad Request");
      rep.addBody("");
      return rep.prepareHttpResponse();
      }

   if (((metric.find("/metrics\0") == std::string::npos)&&(metric.find("%2Fmetrics\0") == std::string::npos)) && ((metric.find("/liveness\0") == std::string::npos)&&(metric.find("%2Fliveness\0") == std::string::npos)))
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: HTTP 404 Not Found: Did Not Receive Liveness probe or Metric Request!\n");
      rep.addHeaderField("", "HTTP/1.1 404 Not Found");
      rep.addBody("");
      return rep.prepareHttpResponse();
      }


   rep.addHeaderField("", "HTTP/1.1 200 OK");
   rep.addHeaderField("Content-Type", "text/plain; version=0.0.4; charset=utf-8");
   if ((metric.find("/metrics\0") != std::string::npos)||(metric.find("%2Fmetrics\0") != std::string::npos))
      {
      rep.addBody(db.prepareAllMetricsBody());
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Prepared Metrics HTTP Response!\n");
      }
   else 
      {
      std::string s("");
      rep.addBody(s);
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Prepared Liveness Probe HTTP Response!\n");
      }
   return rep.prepareHttpResponse();
   }

void TR_MetricServerHandlerStart(J9JITConfig* jitConfig, TR_MetricServer const* m) {
   J9JavaVM* vm = jitConfig->javaVM;
   // First make sure to get the options specified by the user
   int metricPort=7390;
   int metricTimeout=100;
   int metricMaxConnections=10;
   int metricHttpsPort=7391;

   const char* arg1 = "-XX:MetricServerPort=";
   const char* arg2 = "-XX:MetricServerTimeoutMs=";
   const char* arg3 = "-XX:MetricServerMaxConnections=";
   const char* arg4 = "-XX:MetricHttpsPort=";

   int metricPortIdx = FIND_ARG_IN_VMARGS(STARTSWITH_MATCH, arg1, 0);
   int metricTimeoutIdx = FIND_ARG_IN_VMARGS(STARTSWITH_MATCH, arg2, 0);
   int metricMaxConnectionsIdx = FIND_ARG_IN_VMARGS(STARTSWITH_MATCH, arg3, 0);
   int metricHttpsPortIdx = FIND_ARG_IN_VMARGS(STARTSWITH_MATCH, arg4, 0);

   if (metricPortIdx >= 0) 
      {
      GET_INTEGER_VALUE(metricPortIdx, arg1, metricPort);
      if (metricPort <= 0)
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Invalid Argument: Port!\n");
         perror("Error: Invalid Number for Port!");
         exit(1);
         }
      }
   if (metricTimeoutIdx >= 0)
      {
      GET_INTEGER_VALUE(metricTimeoutIdx, arg2, metricTimeout);
      if (metricTimeout <= 0)
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Invalid Argument: Timeout!\n");
         perror("Error: Invalid Number for Timeout!");
         exit(1);
         }
      }
   if (metricMaxConnectionsIdx >= 0)
      {
      GET_INTEGER_VALUE(metricMaxConnectionsIdx, arg3, metricMaxConnections);
      if (metricMaxConnections <= 0)
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Invalid Argument: Max Number of Connections!\n");
         perror("Error: Invalid Number for Max Connections!");
         exit(1);
         }
      }
   if (metricHttpsPortIdx >= 0) 
      {
      GET_INTEGER_VALUE(metricHttpsPortIdx, arg4, metricHttpsPort);
      if (metricHttpsPort <= 0)
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Invalid Argument: Https Port!\n");
         perror("Error: Invalid Number for Https Port!");
         exit(1);
         }
      }

   // Make sure that this server is SSL encrypted
   TR::PersistentInfo *info = getCompilationInfo(jitConfig)->getPersistentInfo();
   SSL_CTX *sslCtx = NULL;
   if (JITServer::CommunicationStream::useSSL())
      {
      JITServer::CommunicationStream::initSSL();
      sslCtx = createSSLContext(info);
      }

   if (sslCtx == NULL)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: SSL is not enabled only HTTP is supported!\n");
      }

   // We will use this object to store our metrics for reading and writing
   MetricDatabase db;
   // Initialize the database with the metrics extracted from jitserver
   db.update(jitConfig);
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Metrics initialized!\n");

   // This server object will handle the listening of Http connections
   Server server = Server(metricPort);

   // This server object will handle the listening of Https connections
   Server serverHttps = Server(metricHttpsPort);

   // Start the server
   if (server.serverStart() < 0)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Failed to start!\n");
      server.serverClear();
      if (sslCtx)
         {
         serverHttps.serverClear();
         (*OSSL_CTX_free)(sslCtx);
         (*OEVP_cleanup)();
         }
      exit(1);
      }
   
   if ((sslCtx) && (serverHttps.serverStart() < 0))
      {
      server.serverClear();
      serverHttps.serverClear();
      (*OSSL_CTX_free)(sslCtx);
      (*OEVP_cleanup)();
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Https Metric Server: Failed to start!\n");
      exit(1);
      }


   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Started Metric Server!\n");
   
   // Create the vector of pollfds and client structures
   std::vector<pollfd> fds;
   struct pollfd init_fd;
   init_fd.fd = server.serverGetSockfd();
   init_fd.events = POLLIN;
   init_fd.revents = 0;
   fds.push_back(init_fd);
   int listeners = 1;
   if (sslCtx)
      {
      struct pollfd init_fd_https;
      init_fd_https.fd = serverHttps.serverGetSockfd();
      init_fd_https.events = POLLIN;
      init_fd_https.revents = 0;
      fds.push_back(init_fd_https);
      listeners = 2;
      }

   std::vector<Client> clients;

   while (!m->getMetricServerExitFlag())
      {
      int check = 0;
      // Poll every few ms
      check = poll(&fds[0], fds.size(), metricTimeout);
      if (m->getMetricServerExitFlag()) // if we are exiting, no need to check poll() status
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Shutting down Metric Server!\n");
         break;
         }
      else if (check == 0) // poll() timed out and no fd is ready
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: No fd is ready in this poll iteration!\n");
         continue;
         }
      else if (check < 0)
         {
         if (errno == EINTR)
            {
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Poll was interrupted!\n");
            continue;
            }
         else
            {
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Poll failed! Killing Metric Server!\n");
            perror("Metric Server: Error in polling listening socket!");
            // Clear everything before exiting!
            server.serverClear();
            for (int i = 0; i < clients.size(); i++)
               {
               clients[i].clientClear();
               }
            fds.erase(fds.begin(), fds.end());
            clients.erase(clients.begin(), clients.end());
            if (sslCtx)
               {
               serverHttps.serverClear();
               (*OSSL_CTX_free)(sslCtx);
               (*OEVP_cleanup)();
               }
            exit(1);
            }
         }
      else if (((fds[0].revents != POLLIN) && (fds[0].revents != 0))||((sslCtx) && (fds[1].revents != POLLIN) && (fds[1].revents != 0)))
         {
         fprintf(stderr, "Metric Server: Unexpected event occurred during poll for new metric server connection: revents=%d\n", fds[0].revents);
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Unexpected poll event at listening socket!\n");
         // Clear everything before exiting!
         server.serverClear();
         for (int i = 0; i < clients.size(); i++)
            {
            clients[i].clientClear();
            }
         fds.erase(fds.begin(), fds.end());
         clients.erase(clients.begin(), clients.end());
         if (sslCtx)
            {
            serverHttps.serverClear();
            (*OSSL_CTX_free)(sslCtx);
            (*OEVP_cleanup)();
            }
         exit(1);
         }
      int newfd = 0;
      int newfd_https = 0;
      // Prepare to accept new http connections
      if (fds[0].revents == POLLIN) 
         {
            
            // If accept fails, then reset fds and reset the clients[new_index-1]
            Client client;
            pollfd pfd;
            if (client.clientAccept(server.serverGetSockfd(), NULL, 1) < 0)
               {
               if (TR::Options::getVerboseOption(TR_VerboseJITServer))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Failed to accept http connection!\n");
               fds[0].revents = 0;
               client.clientClear();
               }
            else if (clients.size() > metricMaxConnections)
               {
               break;
               }
            else 
               {
               if (TR::Options::getVerboseOption(TR_VerboseJITServer))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Accepting new http connection!\n");
               pfd.fd = client.clientGetSockfd();
               newfd = pfd.fd;
               pfd.events = POLLIN;
               pfd.revents = 0;
               fds.push_back(pfd);
               clients.push_back(client);
               fds[0].revents = 0;
               }
         }
      // Prepare to accept new https connections
      if ((sslCtx) && (fds[1].revents == POLLIN)) 
         {
            
            // If accept fails, then reset fds and reset the clients[new_index-1]
            Client client;
            pollfd pfd;
            if (client.clientAccept(serverHttps.serverGetSockfd(), sslCtx, 1) < 0)
               {
               if (TR::Options::getVerboseOption(TR_VerboseJITServer))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Failed to accept https connection!\n");
               fds[0].revents = 0;
               client.clientClear();
               }
            else if (clients.size() > metricMaxConnections)
               {
               break;
               }
            else 
               {
               if (TR::Options::getVerboseOption(TR_VerboseJITServer))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Accepting new https connection!\n");
               pfd.fd = client.clientGetSockfd();
               newfd_https = pfd.fd;
               pfd.events = POLLIN;
               pfd.revents = 0;
               fds.push_back(pfd);
               clients.push_back(client);
               fds[1].revents = 0;
               }
         }
      // Update the database
      db.update(jitConfig);
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Updated metrics!\n");
      
      // Prepare to handle requests from accepted connections
      for (int i = listeners; i < fds.size(); i++) 
      {
         // Received incoming data
         if ((fds[i].fd > 0) && (fds[i].revents == POLLIN))
         {
            // This object is used for handling the parsing of the HTTP Request
            HttpRequest req;

            // Receive the msg from the client/Prometheus
            if (clients[i-listeners].clientReceive(1) < 0)
            {
               if (TR::Options::getVerboseOption(TR_VerboseJITServer))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Failed to receive msg!\n");
               // Clears the structure for reuse and closes the socket
               clients[i-listeners].clientClear();
               // Reset this pollfd
               fds[i].fd = -1;
               fds[i].events = 0;
               fds[i].revents = 0;
               continue;
            }
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Received msg!\n");
            // Parse the std::string into the http request object
            char* data = clients[i-listeners].clientGetRecvMsg();
            if (req.parse(data) < 0)
               {
               if (TR::Options::getVerboseOption(TR_VerboseJITServer))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Invalid Request received!\n");
               // Clears the structure for reuse and closes the socket
               clients[i-listeners].clientClear();
               // Reset this pollfd
               fds[i].fd = -1;
               fds[i].events = POLLIN;
               fds[i].revents = 0;
               continue;
               }

            std::vector<char> send_buffer;
            send_buffer = httpErrorCheck(req, db);

            // Verify that the http request is valid and then
            // send back the response
            if (clients[i-listeners].clientSend(send_buffer, 1) < 0)
               {
               if (TR::Options::getVerboseOption(TR_VerboseJITServer))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Failed to send msg!\n");
               // Clears the structure for reuse and closes the socket
               clients[i-listeners].clientClear();
               // Reset this pollfd
               fds[i].fd = -1;
               fds[i].events = POLLIN;
               fds[i].revents = 0;
               continue;
               }

            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Sent msg!\n");
    
            const char* connection = req.getConnection();
            // Check if we need to keep this connection alive for another request
            if (strcmp(connection, "keep-alive") == 0)
               {
               if (TR::Options::getVerboseOption(TR_VerboseJITServer))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Maintaining client connection!\n");
               fds[i].revents = 0;
               continue;
               }
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Closing client connection!\n");
            // Clears the structure for reuse and closes the socket
            clients[i-listeners].clientClear();
            // Reset this pollfd
            fds[i].fd = -1;
            fds[i].events = POLLIN;
            fds[i].revents = 0;
         }
         // They closed the connection on us, so kill the connection
         else if ((fds[i].fd > 0) && ((fds[i].revents == POLLHUP)||((fds[i].fd != newfd)&&(fds[i].revents == 0)&&(clients[i-listeners].clientGetUseSSL() <= 0))||((sslCtx)&&(fds[i].fd != newfd_https)&&(fds[i].revents == 0)&&(clients[i-listeners].clientGetUseSSL() > 0))))
         {
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Closing socket for killed connection!\n");
            // Clears the structure for reuse and closes the socket
            clients[i-listeners].clientClear();
            // Reset this pollfd
            fds[i].fd = -1;
            fds[i].events = POLLIN;
            fds[i].revents = 0;
         }
      }
      //Remove unnecessary fds
      int idx = 0;
      while (idx <= (fds.size()-1)) 
         {
         if (fds[idx].fd < 0)
            {
            fds.erase(fds.begin()+idx);
            clients.erase(clients.begin()+idx-listeners);
            }
         else
            {
            idx++;
            }
         }
   }
   // Close down any open clients and clear any info
   fds.erase(fds.begin());
   if (sslCtx)
      fds.erase(fds.begin());

   for (int i = (fds.size()-1); i >= 0; i--) 
   {
      fds.erase(fds.begin()+i);
      clients[i].clientClear();
      clients.erase(clients.begin()+i);
   }

   // Close down the server and clear any info
   server.serverClear();
   if (sslCtx)
      serverHttps.serverClear();

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Closed all open sockets!\n");

   if (sslCtx)
      {
      (*OSSL_CTX_free)(sslCtx);
      (*OEVP_cleanup)();
      }
}
