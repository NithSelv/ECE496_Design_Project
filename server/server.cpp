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
#include "node.h"
#include "metrics_database.h"
#include "http_request.h"
#include "http_response.h"
#include "client.h"
#include "server.h"
#include "globals.h"

std::string http_error_check(Http_Request &req, Metrics_Database* db) {
    Http_Response rep;

    int type = req.Find_Type();
    char* metric = req.Find("Metric");

    if ((type == -1) || (metric == NULL)) {		
	rep.Add_Header_Field("", "HTTP/1.1 400 Bad Request");
	return rep.Prepare_Http_Response();
    } 

    if (type != Http_Request::Http_Types::GET){		
	rep.Add_Header_Field("", "HTTP/1.1 400 Bad Request");
	return rep.Prepare_Http_Response();
    }

    if ((strlen(metric) != strlen("/metrics")) || (strcmp(metric, "/metrics") != 0)) {		
	rep.Add_Header_Field("", "HTTP/1.1 404 Not Found");
	return rep.Prepare_Http_Response();
    }

    rep.Add_Header_Field("", "HTTP/1.1 200 OK");
    rep.Add_Header_Field("Content-Type", "text/plain; version=0.0.4; charset=utf-8");
    char * body = db->Prepare_All_Metrics_Body();
    rep.Add_Body(body);
    delete body;
    body = NULL;
    return rep.Prepare_Http_Response();
}

/*This is the main function that runs the server code, it takes in 3 arguments
1: port number, 2: number of connections that can be handled, 3: how long the server should wait for a connection's request before timing out (sec)*/
int main(int argc, char* argv[]){
    //First check to make sure that we have all 3 required arguments
    if (argc < 4) {
        std::cout << "FAILED: Not enough arguments" << std::endl;
        return Codes::TooFewArgs;
    } else if (argc > 4) {
	std::cout << "FAILED: Too many arguments" << std::endl;
        return Codes::TooManyArgs;
    }

    /*TO BE REMOVED: This is just an object used to compute the calendar time metric
    Once we can retrieve metrics from JITServer, we will remove this*/
    struct timeval start;
    gettimeofday(&start, NULL);

    //Create the database dynamically, we will be storing/adding/reading/deleting metrics from this object.
    Metrics_Database* db = new Metrics_Database();

    //Initialize the database with the initial metrics (supposed to take in no arguments but will be fixed once integrated with JITServer)
    if (db->Initialize(start.tv_sec + start.tv_usec * 0.000001) != 0) {
	std::cout << "FAILED: Unable to initialize database!" << std::endl;
	delete db;
        return Codes::DatabaseFailed;
    }

    //Assign the arguments to the appropiate variables, we will use them later
    int port = atoi(argv[1]);
    int num_connections = atoi(argv[2]);
    int timeout = atoi(argv[3]);

    //Start the server 
    Server server = Server(port, num_connections);
    if (server.Start() < 0) {
	std::cout << "Server failed to start!" << std::endl;
	delete db;
	return Codes::ServerFailed;
    }

    //Handle each connection by the following
    while(1) {
	//This object is used for server-client communications
	Client client; 
	//This object is used for handling the parsing of the HTTP Request
	Http_Request req;

	//Accept a new connection
    	if (client.Accept(&server) < 0) {
	    continue;
    	}
	//Receive the msg from the client/Prometheus
	if (client.Receive(timeout) < 0) {
	    continue;
	}
	//Parse the std::string into the http request object
	req.Parse(client.Get_Recv_Msg().c_str());

	//Verify that the http request is valid and then generate a std::string for the response
	std::string http_rep = http_error_check(&req, db);

	//Send back the response
	if (client.Send(&http_rep, timeout) < 0) {
	    continue;
	}

	//Update the database 
	if (db->Update(start.tv_sec + start.tv_usec * 0.000001) < 0) {
	    std::cout << "FAILED: database did not update!" << std::endl;
	}

	//Close the client connection 
	client.Close();	
    }		
}

