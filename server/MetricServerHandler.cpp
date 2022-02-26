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
#include "PrometheusExporter/metrics_database.h"
#include "PrometheusExporter/http_request.h"
#include "PrometheusExporter/http_response.h"
#include "PrometheusExporter/client.h"
#include "PrometheusExporter/server.h"
#include "MetricServerHandler.h"
#include "control/CompilationRuntime.hpp"
#include "env/TRMemory.hpp"
#include "env/VMJ9.h"
#include "env/VerboseLog.hpp"
#include "net/CommunicationStream.hpp"
#include "net/LoadSSLLibs.hpp"
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

static void TR_MetricServerHandler::Start(J9JITConfig* jitConfig, MetricServer* m) {
   // Make sure that this server is SSL encrypted
   TR::PersistentInfo *info = getCompilationInfo(jitConfig)->getPersistentInfo();
   SSL_CTX *sslCtx = NULL;
   if (JITServer::CommunicationStream::useSSL())
      {
      JITServer::CommunicationStream::initSSL();
      sslCtx = createSSLContext(info);
      }

   // We will use this object to store our metrics for reading and writing
   MetricsDatabase db;
   // Initialize the database with the metrics extracted from jitserver
   db.update(jitConfig);

   // This server object will handle the listening of connections
   Server server = Server(JITSERVER_METRIC_SERVER_PORT);

   // Start the server
   if (server.serverStart() < 0)
      {
      exit(1);
      }
   
   // Create the array of pollfds and client structures
   struct pollfd fds[JITSERVER_METRIC_SERVER_POLLFDS+1];
   Client clients[JITSERVER_METRIC_SERVER_POLLFDS];
   fds[0].fd = server.getSockfd();
   fds[0].events = POLLIN;
   fds[0].revents = 0;
   // Keep a queue for free spots on the pollfd array
   std::vector<int> newfds;
   // Initialize the other fds so that they don't trigger anything
   for (int i = 1; i < (JITSERVER_METRIC_SERVER_POLLFDS+1); i++) 
   {
      fds[i].fd = -1;
      fds[i].events = POLLIN;
      fds[i].revents = 0;
      newfds.push_back(i);
   }
   while (!m->getMetricServerExitFlag())
      {
      int check = 0;
      // Poll every few ms
      check = poll(&fds, JITSERVER_METRIC_SERVER_POLLFDS+1, JITSERVER_METRIC_SERVER_TIMEOUT_USEC / 1000);
      if (m->getMetricServerExitFlag()) // if we are exiting, no need to check poll() status
         {
         break;
         }
      else if (check == 0) // poll() timed out and no fd is ready
         {
         continue;
         }
      else if (check < 0)
         {
         if (errno == EINTR)
            {
            continue;
            }
         else
            {
            perror("Metric Server: Error in polling listening socket!");
            exit(1);
            }
         }
      else if ((fds[0].revents != POLLIN) && (fds[0].revents != 0))
         {
         fprintf(stderr, "Metric Server: Unexpected event occurred during poll for new metric server connection: revents=%d\n", fds[0].revents);
         exit(1);
         }
      // Prepare to accept new connections
      if (fds[0].revents == POLLIN) 
         {
            // We need to wait and handle a few connections before taking anymore connections
            if (newfds.size() == 0) 
               {
                  break;
               }
            else
               {
                  // There's some available spots, take one and use it to handle an incoming request.
                  int new_index = newfds.back();
                  // If accept fails, then reset fds and reset the clients[new_index-1]
                  if (clients[new_index-1].clientAccept(server.getSockfd(), sslCtx) < 0)
                     {
                     fds[0].revents = 0;
                     clients[new_index-1].clientClear();
                     break;
                     }
                  fds[new_index].fd = clients[new_index-1].getSockfd();
                  fds[new_index].events = POLLIN;
                  fds[new_index].revents = 0;
                  newfds.pop_back();
                  fds[0].revents = 0;
               }
         }
      // Update the database
      db.update(jitConfig);
      // Prepare to handle requests from accepted connections
      for (int i = 1; i < (JITSERVER_METRIC_SERVER_POLLFDS+1); i++) 
      {
         // Received incoming data
         if ((fds[i].fd > 0) && (fds[i].revents == POLLIN))
         {
            // This object is used for handling the parsing of the HTTP Request
            HttpRequest req;

            // Receive the msg from the client/Prometheus
            if (clients[i-1].clientReceive(JITSERVER_METRIC_SERVER_TIMEOUT_USEC) < 0)
            {
               // Clears the structure for reuse and closes the socket
               clients[i-1].clientClear();
               // Reset this pollfd
               fds[i].fd = -1;
               fds[i].events = POLLIN;
               fds[i].revents = 0;
               // Add it back into the queue
               newfds.push_back(i);
               continue;
            }

            // Parse the std::string into the http request object
            char* data = clients[i-1].clientGetRecvMsg();
            if (req.parse(data) < 0)
               {
               // Clears the structure for reuse and closes the socket
               clients[i-1].clientClear();
               // Reset this pollfd
               fds[i].fd = -1;
               fds[i].events = POLLIN;
               fds[i].revents = 0;
               // Add it back into the queue
               newfds.push_back(i);
               continue;
               }

            std::vector<char> send_buffer = httpErrorCheck(req, db);

            // Verify that the http request is valid and then
            // send back the response
            if (clients[i-1].clientSend(send_buffer, JITSERVER_METRIC_SERVER_TIMEOUT_USEC) < 0)
               {
               // Clears the structure for reuse and closes the socket
               clients[i-1].clientClear();
               // Reset this pollfd
               fds[i].fd = -1;
               fds[i].events = POLLIN;
               fds[i].revents = 0;
               // Add it back into the queue
               newfds.push_back(i);
               continue;
               }
    
            const char* connection = req.getConnection();
            // Check if we need to keep this connection alive for another request
            if (strcmp(connection, "keep-alive") == 0)
               fds[i].revents = 0;
               continue;

            // Clears the structure for reuse and closes the socket
            clients[i-1].clientClear();
            // Reset this pollfd
            fds[i].fd = -1;
            fds[i].events = POLLIN;
            fds[i].revents = 0;
            // Add it back into the queue
            newfds.push_back(i);
         }
         // They closed the connection on us, so kill the connection
         else if ((fds[i].fd > 0) && (fds[i].revents == POLLHUP))
         {
            // Clears the structure for reuse and closes the socket
            clients[i-1].clientClear();
            // Reset this pollfd
            fds[i].fd = -1;
            fds[i].events = POLLIN;
            fds[i].revents = 0;
            // Add it back into the queue
            newfds.push_back(i);
         }
      }
      if ((fds[0].revents == POLLIN) && (newfds.size() != 0))
         {
         // There's some available spots, take one and use it to handle an incoming request.
         int new_index = newfds.back();
         // If accept fails, then reset fds and reset the clients[new_index-1]
         if (clients[new_index-1].clientAccept(server.getSockfd(), sslCtx) < 0)
            {
               fds[0].revents = 0;
               clients[new_index-1].clientClear();
               break;
            }
            fds[new_index].fd = clients[new_index-1].getSockfd();
            fds[new_index].events = POLLIN;
            fds[new_index].revents = 0;
            newfds.pop_back();
            fds[0].revents = 0;
         }
   }
   // Close down any open clients and clear any info
   fds[0].fd = -1;
   fds[0].events = POLLIN;
   fds[0].revents = 0;

   for (int i = 1; i < (JITSERVER_METRIC_SERVER_POLLFDS+1); i++) 
   {
      fds[i].fd = -1;
      fds[i].events = POLLIN;
      fds[i].revents = 0;
      clients[i-1].clientClear();
   }

   // Close down the server and clear any info
   server.serverClear();
   if (sslCtx)
      {
      (*OSSL_CTX_free)(sslCtx);
      (*OEVP_cleanup)();
      }
}

std::vector<char> httpErrorCheck(HttpRequest req, MetricsDatabase db)
   {
   HttpResponse rep;
   int type = req.getType();
   std::string metric(req.getMetric());

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
   rep.addBody(db.prepareAllMetricsBody());
   return rep.prepareHttpResponse();
   }
