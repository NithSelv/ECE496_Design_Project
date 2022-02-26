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

static bool handleOpenSSLConnectionError(int connfd, SSL *&ssl, BIO *&bio, const char *errMsg) {
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

static bool acceptOpenSSLConnection(SSL_CTX *sslCtx, int connfd, BIO *&bio) {
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
                  clients[new_index-1].clientAccept(server.getSockfd(), sslCtx);
                  fds[new_index].fd = clients[new_index-1].getSockfd();
                  fds[new_index].events = POLLIN;
                  fds[new_index].revents = 0;
                  newfds.pop_back();
               }
         }

      // Prepare to handle requests from accepted connections
      int success = 0;
      for (int i = 1; i < (JITSERVER_METRIC_SERVER_POLLFDS+1); i++) 
      {
         if (fds[i].fd > 0) 
         {
            // Receive the msg from the client/Prometheus
            success = client.clientReceive(timeout);
            //Check if HTTP Message is valid
            //Send appropiate response
            //If Keep-Alive, then don't remove socket else remove it and add it back to the queue
         }
      }

   }

   // Close down the server and clear any info
   server.serverClear();
   if (sslCtx)
      {
      (*OSSL_CTX_free)(sslCtx);
      (*OEVP_cleanup)();
      }
}

std::vector<char> httpErrorCheck(HttpRequest req, MetricsDatabase* db)
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
   rep.addBody(db->prepareAllMetricsBody());
   return rep.prepareHttpResponse();
   }

// This is the main function that runs the server code, it takes in 3 arguments
// 1: port number
// 2: number of connections that can be handled
// 3: how long the server should wait for a connection's request before timing out (sec)
int main(int argc, char* argv[])
   {

   // Handle each connection by the following
   while (1)
      {

      // This object is used for server-client communications
      Client client;

      // Accept a new connection
      int sockfd = server.serverGetSockfd();
      int accept = client.clientAccept(sockfd);
      if (accept < 0)
         continue;

      int keepAlive = 1;
      while (keepAlive > 0)
         {
         // This object is used for handling the parsing of the HTTP Request
         HttpRequest req;

         // Update the database
         int check = db->update(start.tv_sec + start.tv_usec * 0.000001);
         if (check < 0)
            std::cout << "FAILED: database did not update!" << std::endl;

         // Receive the msg from the client/Prometheus
         check = client.clientReceive(timeout);
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

         std::vector<char> send_buffer = httpErrorCheck(req, db);

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
