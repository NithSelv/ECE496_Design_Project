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
#include <time.h>
#include <vector>
#include "metrics_database.h"
#include "http_request.h"
#include "http_response.h"
#include "client.h"
#include "server.h"

// These constants represent the various return codes that our main function can return.
enum Codes {Success = 0, TooFewArgs = -1, TooManyArgs = -2, DatabaseFailed = -3, ServerFailed = -4};

std::vector<char> http_error_check(Http_Request* req, Metrics_Database* db)
   {
   Http_Response rep;
   int type = req->getType();
   std::string metric(req->getMetric());

   if (type != Http_Request::GET)
      {
      rep.Add_Header_Field("", "HTTP/1.1 400 Bad Request");
      rep.Add_Body("");
      return rep.Prepare_Http_Response();
      }

   if (metric.find("/metrics\0") == std::string::npos)
      {
      rep.Add_Header_Field("", "HTTP/1.1 404 Not Found");
      rep.Add_Body("");
      return rep.Prepare_Http_Response();
      }

   rep.Add_Header_Field("", "HTTP/1.1 200 OK");
   rep.Add_Header_Field("Content-Type", "text/plain; version=0.0.4; charset=utf-8");
   rep.Add_Body(db->Prepare_All_Metrics_Body());
   return rep.Prepare_Http_Response();
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
      return TooFewArgs;
      }
   else if (argc > 4)
      {
      std::cout << "FAILED: Too many arguments" << std::endl;
      return TooManyArgs;
      }

   // TO BE REMOVED: This is just an object used to compute the calendar time metric
   // Once we can retrieve metrics from JITServer, we will remove this
   struct timeval start;
   gettimeofday(&start, NULL);
   // Create the database dynamically, we will be storing/adding/reading/deleting metrics from this object.
   Metrics_Database* db = new Metrics_Database();

   // Initialize the database with the initial metrics
   // (supposed to take in no arguments but will be fixed once integrated with JITServer)
   if (db->Initialize(start.tv_sec + start.tv_usec * 0.000001) != 0)
      {
      std::cout << "FAILED: Unable to initialize database!" << std::endl;
      delete db;
      return DatabaseFailed;
      }

   // Assign the arguments to the appropiate variables, we will use them later
   int port = atoi(argv[1]);
   int num_connections = atoi(argv[2]);
   int timeout = atoi(argv[3]);

   // Start the server
   Server server = Server(port, num_connections);
   if (server.Start() < 0)
      {
      std::cout << "Server failed to start!" << std::endl;
      delete db;
      return ServerFailed;
      }

   // Handle each connection by the following
   while (1)
      {
      // Update the database
      if (db->Update(start.tv_sec + start.tv_usec * 0.000001) < 0)
         std::cout << "FAILED: database did not update!" << std::endl;

      // This object is used for server-client communications
      Client client;
      // This object is used for handling the parsing of the HTTP Request
      Http_Request req;

      // Accept a new connection
      if (client.Accept(server.getSockfd()) < 0)
         continue;

      int keep_alive = 1;
      while (keep_alive > 0)
         {
         // Receive the msg from the client/Prometheus
         if (client.Receive(timeout) < 0)
            {
            keep_alive = -1;
            break;
            }
         // Parse the std::string into the http request object
         if (req.Parse(client.getRecvMsg()) < 0)
            {
            keep_alive = 0;
            break;
            }
         keep_alive = 0;
         // Check if we need to keep this connection alive for another request
         if ((strlen(req.getConnection()) == strlen("keep-alive")) && (strcmp(req.getConnection(), "keep-alive") == 0))
            keep_alive = 1;

         // Verify that the http request is valid and then
         // send back the response
         if (client.Send(http_error_check(&req, db), timeout) < 0)
            {
            keep_alive = -1;
            break;
            }
         }
      // Close the client connection
      if (keep_alive == 0)
         client.Close();
      }
}
