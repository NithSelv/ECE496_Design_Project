#include <cstdio>
#include <cstdlib>
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

void setup_server_struct(struct sockaddr_in* addr, int port) {
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = htonl(INADDR_ANY);
    memset(addr->sin_zero, 0, 8); 
    return;
}

char* http_error_check(Http_Request* req, Http_Response* rep, Metrics_Database* db, char* port_num) {
    //char* client = (*req).Find("User-Agent");
    //if ((client == NULL) || (strncmp(client, "Prometheus", 10) != 0)) {
        //printf("Unauthorized Client!\n");
	//(*rep).Add_Field("Header", "HTTP/1.1 401 Unauthorized");	
	//return (*rep).Prepare_Http_Response();
    //} 

    char* type = (*req).Find("Type");
    char* metric = (*req).Find("Metric");
    char* ip = (*req).Find("Host");

    if (ip == NULL) {
	printf("Invalid Request!\n");
	(*rep).Add_Field("Header", "HTTP/1.1 400 Bad Request");
	return (*rep).Prepare_Http_Response();	
    }

    char* port_s = strtok(ip, ":");
    char* host = port_s;
    port_s = strtok(NULL, ":");

    if ((type == NULL) || (metric == NULL) || (host == NULL) || (port_s == NULL)) {
	printf("Invalid Request!\n");		
	(*rep).Add_Field("Header", "HTTP/1.1 400 Bad Request");
	return (*rep).Prepare_Http_Response();
    } 

    if (strncmp(type, "GET", 3) != 0){
	printf("Invalid Request!\n");		
	(*rep).Add_Field("Header", "HTTP/1.1 400 Bad Request");
	return (*rep).Prepare_Http_Response();
    }

    if (strncmp(metric, "/metrics", 8) != 0) {
	printf("Not Found!\n");		
	(*rep).Add_Field("Header", "HTTP/1.1 404 Not Found");
	return (*rep).Prepare_Http_Response();
    }

    if ((strncmp(host, "localhost", 9) != 0) && (strncmp(port_s, port_num, strlen(port_num)) != 0)) {
	printf("Wrong server or port!\n");		
	(*rep).Add_Field("Header", "HTTP/1.1 303 See Other");
	return (*rep).Prepare_Http_Response();
    }

    printf("Valid Request Received!\n");
    (*rep).Add_Field("Header", "HTTP/1.1 200 OK");
    (*rep).Add_Field("Content-Type", "text/plain; version=0.0.4; charset=utf-8");
    char * body = db->Prepare_All_Metrics_Body();
    (*rep).Add_Field("Body", body);
    delete body;
    body = NULL;
    return (*rep).Prepare_Http_Response();
}

int main(int argc, char* argv[]){
    
    int port, num_connections, sockfd, newfd, bound, connecting, timeout, db_check;
    char port_num[100] = "";
    double start_time = 0;
    char receive_buffer[4096];
    char send_buffer[4096];
    struct sockaddr_in server_addr;
    struct timeval start;
    gettimeofday(&start, NULL);
    start_time = start.tv_sec + start.tv_usec * 0.000001;
    Metrics_Database db;

    db_check = db.Initialize(start_time);
    if (db_check != 0) {
	printf("FAILED: Unable to initialize database!\n");
        return -1;
    }

    if (argc != 4) {
        printf("FAILED: Not enough arguments\n");
        return -1;
    } else {
	strcpy(port_num, argv[1]);
        port = atoi(argv[1]);
	num_connections = atoi(argv[2]);
	timeout = atoi(argv[3]);
    }

    setup_server_struct(&server_addr, port);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        printf("FAILED: Socket Creation Failed!\n");
        close(sockfd);
        return -1;
    }

    bound = bind(sockfd, (struct sockaddr *)&server_addr, (socklen_t)sizeof(server_addr));   

    if (bound != 0) {
        printf("FAILED: Binding Failed!\n");
        close(sockfd);
        return -1;
    }

    connecting = listen(sockfd, num_connections);

    if (connecting < 0) {
	printf("FAILED: Listen failed!");
	close(sockfd);
	return -1;
    }


    while(1) {

	struct sockaddr_storage client_addr;
    	socklen_t client_addr_size;

    	newfd = accept(sockfd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_size);

    	if (newfd < 0) {
		printf("FAILED: Accept failed!");
		close(newfd);
		close(sockfd);
		return -1;
    	}

	struct timeval timer;
	timer.tv_sec = timeout;
	int time_set = setsockopt(newfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timer, sizeof(timer));

        if (time_set < 0) {
        	printf("FAILED: Could not set timeout!\n");
		close(newfd);
		close(sockfd);
		return -1;
	}

	int num_bytes = 0;
    	memset((void*)receive_buffer, 0, sizeof(receive_buffer));
    	num_bytes = recv(newfd, receive_buffer, sizeof(receive_buffer), 0);
    	if ((num_bytes == -1) && ((errno == EAGAIN)||(errno == EWOULDBLOCK))) {
	    close(newfd);
	    continue;
    	} else if (num_bytes < 0) {
	    printf("FAILED: Receive failed!");
	    close(newfd);
	    close(sockfd);
	    return -1;
        }

	Http_Request req;
	Http_Response rep;

	req.Parse(receive_buffer);

	char * http_rep = http_error_check(&req, &rep, &db, port_num);

	req.Print();
	printf("\n\n");
	rep.Print();

	int total_bytes = 0;
    	num_bytes = 0;
    	memset((void*)send_buffer, 0, sizeof(send_buffer));
    	strcpy(send_buffer, http_rep);

    	while (total_bytes != (int)strlen(send_buffer)) {
            num_bytes = send(newfd, send_buffer+total_bytes, strlen(send_buffer) - total_bytes, 0);
	    if (num_bytes < 0) {
		printf("FAILED: Send failed!");
		close(newfd);
		close(sockfd);
		return -1;
	    }
	    total_bytes += num_bytes;
        }

	db_check = db.Update(start_time);
	if (db_check < 0) {
	    printf("FAILED: database did not update!");
	    close(newfd);
	    close(sockfd);
	    return -1;
	}
	printf("One connection handled!\n");

	close(newfd);
	
    }
    close(sockfd);		
    return 0;
}

