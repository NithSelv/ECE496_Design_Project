//We would like to give acknowledgement to Brian "Beej Jorgensen" Hall and his "Beej's Guide
//To Network Programming" pdf which helped with setting up the sockets
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <time.h> 
#include "message.h"
#include <pthread.h>

enum PACKET_TYPE{LOGIN,LO_ACK,LO_NAK,EXIT,JOIN,JN_ACK,JN_NAK,LEAVE_SESS,NEW_SESS,NS_ACK,MESSAGE,QUERY,QU_ACK};

pthread_mutex_t locks[6];

static const char* name_list[5] = {"John", "Marco", "Frank", "Bob", "Jonny"}; 
static const char* passwd_list[5] = {"gogo3", "mmhm2", "ltm3s", "funnn3", "gem764"}; 

struct client_args {
	int sock_fd;
	struct sockaddr_storage addr;
	socklen_t addr_size;
	char receive_buffer[4096];
	char send_buffer[4096];
};

char session_list[5][100] = {"", "", "", "", ""};
struct client_args* client_list[5] = {NULL, NULL, NULL, NULL, NULL};

int send_msg(struct client_args* c, int type, char* source, char*data, int i) {
	int total_bytes = 0;
	int num_bytes = 0;
	struct Message msg;
	msg.type = type;
	strcpy(msg.source, source);
	msg.size = sizeof(data);
	strcpy(msg.data, data);
	char* temp = messageToBuffer(&msg);
	pthread_mutex_lock(&locks[i]);
	strcpy(c->send_buffer, temp);
	while (total_bytes < 4096) {
	    num_bytes = send(c->sock_fd, (c->send_buffer+total_bytes), sizeof(c->send_buffer)-total_bytes, 0);
	    if (num_bytes < 0) {
	        printf("FAILED: Failed to send message\n");
		    pthread_mutex_unlock(&locks[i]);
		    close(c->sock_fd);
		    return -1;
	    } 
	    total_bytes += num_bytes;
	}
	memset((void *)c->send_buffer, 0, sizeof(c->send_buffer));
	pthread_mutex_unlock(&locks[i]);
	return 0;
}

int recv_msg(struct client_args* c) {
	int total_bytes = 0;
	int num_bytes = 0;
	memset((void *)c->receive_buffer, 0, sizeof(c->receive_buffer));
	while (total_bytes < 4096) {
	    num_bytes = recv(c->sock_fd, (c->receive_buffer+total_bytes), sizeof(c->receive_buffer)-total_bytes, 0);
	    if (num_bytes < 0) {
            printf("FAILED: Receive Failed");
		    close(c->sock_fd);
		    return -1;
	    }
	    total_bytes += num_bytes;
	}
	return 0;
}

void *handle_client_request(void* client_args) {
	struct client_args* c = (struct client_args*)client_args;
	//Now we should wait for a login attempt from the client
	printf("Waiting for login msg...");
	int success = recv_msg(c);
	if (success == -1) {
        free(c);
	    return NULL;
	}
	struct Message* msg = bufferToMessage(c->receive_buffer);
	int i = 0;
	if (msg->type == LOGIN) {
	    printf("PASSED: Received login msg!\n");
	    int client_found = 0;
	    printf("Checking userid and passwd...\n");
	    for (i = 0; i < 5; i++) {
            if ((strcmp(msg->source, name_list[i]) == 0) && (strcmp(msg->data, passwd_list[i]) == 0)) {
		        pthread_mutex_lock(&locks[i]);
		        if (client_list[i] != NULL) {
	 	            printf("FAILED: Client %d, already logged in!\n", i);
			        printf("Sending LO_NACK...\n");
			        pthread_mutex_unlock(&locks[i]);
			        success = send_msg(c, LO_NAK, "server", "Client already logged in!", i);
			        if (success == -1) {
                        free(c);
			            return NULL;
			        }
			        close(c->sock_fd);
                    free(c);
			        return NULL;
		        } else {
		            printf("PASSED: Client %d, logged in!\n", i);
			        client_list[i] = c;
			        pthread_mutex_unlock(&locks[i]);
			        printf("Sending LO_ACK...\n");
			        success = send_msg(c, LO_ACK, "server", "", i);
			        if (success == -1) {
			            printf("FAILED: Send failed!!\n");
                        pthread_mutex_lock(&locks[i]);
                        free(client_list[i]);
                        client_list[i] = NULL;
                        memset((void *)session_list[i], 0, sizeof(session_list[i]));
                        pthread_mutex_unlock(&locks[i]);
			            return NULL;
			        }
			        client_found = 1;
			        break;
		        }
		    }
	    }
	    if (client_found == 0) {
	        printf("FAILED: Either userid or passwd is incorrect!\n");
		    success = send_msg(c, LO_NAK, "server", "Client not registered!", i);
		    if (success == -1) {
		        printf("FAILED: Send failed!!\n");
                pthread_mutex_lock(&locks[i]);
                free(client_list[i]);
                client_list[i] = NULL;
                memset((void *)session_list[i], 0, sizeof(session_list[i]));
                pthread_mutex_unlock(&locks[i]);
		        return NULL;
		    }
		    close(c->sock_fd);
            pthread_mutex_lock(&locks[i]);
            free(client_list[i]);
            client_list[i] = NULL;
            memset((void *)session_list[i], 0, sizeof(session_list[i]));
            pthread_mutex_unlock(&locks[i]);
		    return NULL;
	    }

	} else {
	    printf("FAILED: Did not receive login message!\n");
	    close(c->sock_fd);
        pthread_mutex_lock(&locks[i]);
        free(client_list[i]);
        client_list[i] = NULL;
        memset((void *)session_list[i], 0, sizeof(session_list[i]));
        pthread_mutex_unlock(&locks[i]);
	    return NULL;
    }
	
	while(1) {
	    printf("Waiting for user input...");
        printf("Here's the first socky: %d\n", c->sock_fd);
	    success = recv_msg(c);
	    if (success == -1) {
            pthread_mutex_lock(&locks[i]);
            free(client_list[i]);
            client_list[i] = NULL;
            memset((void *)session_list[i], 0, sizeof(session_list[i]));
            pthread_mutex_unlock(&locks[i]);
	        return NULL;
	    }
	    msg = bufferToMessage(c->receive_buffer);

	    if (msg->type == EXIT) {
		    pthread_mutex_lock(&locks[i]);
            free(client_list[i]);
		    client_list[i] = NULL;
		    memset((void *)session_list[i], 0, sizeof(session_list[i]));
		    pthread_mutex_unlock(&locks[i]);
		    return NULL;
	    } else if (msg->type == LEAVE_SESS) {
		    pthread_mutex_lock(&locks[i]);
		    memset((void *)session_list[i], 0, sizeof(session_list[i]));
		    pthread_mutex_unlock(&locks[i]);
	    } else if (msg->type == QUERY) {
		    int j = 0;
		    char data[1000] = "users: ";
		    char data2[1000] = " sessions: ";
		    for (j = 0; j < 5; j++) {
                if (client_list[j] != NULL) {
		            strcat(data, name_list[j]);
                    strcat(data, ", ");
		            strcat(data2, session_list[j]);
                    strcat(data2, ", ");
                }
		    }
		    strcat(data, data2);
		    success = send_msg(client_list[i], QU_ACK, "server", data, i);
		    if (success == -1) {
		        printf("FAILED: Send failed!\n");
                pthread_mutex_lock(&locks[i]);
                free(client_list[i]);
                client_list[i] = NULL;
                memset((void *)session_list[i], 0, sizeof(session_list[i]));
                pthread_mutex_unlock(&locks[i]);
		        return NULL;
		    }
	    } else if (msg->type == MESSAGE) {
		    if (strcmp(session_list[i], "") != 0) {
		        int j = 0;
		        for (j = 0; j < 5; j++) {
			        if (strcmp(session_list[j], session_list[i]) == 0) {
			            success = send_msg(client_list[j], MESSAGE, msg->source, msg->data, j);
			            if (success == -1) {
		    	            printf("FAILED: Send failed!\n");
                            pthread_mutex_lock(&locks[i]);
                            free(client_list[i]);
                            client_list[i] = NULL;
                            memset((void *)session_list[i], 0, sizeof(session_list[i]));
                            pthread_mutex_unlock(&locks[i]);
				            return NULL;
		                }
		            }
		        }
		    } else {
		        printf(msg->data);
		    }
	    } else if (msg->type == NEW_SESS) {
		    pthread_mutex_lock(&locks[i]);
            memset((void *)session_list[i], 0, sizeof(session_list[i]));
		    strcpy(session_list[i], msg->data);
		    pthread_mutex_unlock(&locks[i]);
		    success = send_msg(c, NS_ACK, "server", "", i);
		    if (success == -1) {
		        printf("FAILED: Send failed!\n");
                pthread_mutex_lock(&locks[i]);
                free(client_list[i]);
                client_list[i] = NULL;
                memset((void *)session_list[i], 0, sizeof(session_list[i]));
                pthread_mutex_unlock(&locks[i]);
		        return NULL;
		    }
	    } else if (msg->type == JOIN) {
		    char data[1000] = "";
		    if (strcmp(session_list[i], msg->data) == 0) {
		        strcat(data, msg->data);
		        strcat(data, ",");
		        strcat(data, " Client already in session!");
		        success = send_msg(c, JN_NAK, "server", data, i);
		        if (success == -1) {
		            printf("FAILED: Send failed!\n");
                    pthread_mutex_lock(&locks[i]);
                    free(client_list[i]);
                    client_list[i] = NULL;
                    memset((void *)session_list[i], 0, sizeof(session_list[i]));
                    pthread_mutex_unlock(&locks[i]);
		            return NULL;
		        }
		    } else {
		        int j = 0;
		        for (j = 0; j < 5; j++) {
		            if (strcmp(session_list[j], msg->data) == 0) {
                        pthread_mutex_lock(&locks[i]);
                        memset((void *)session_list[i], 0, sizeof(session_list[i]));
			            strcpy(session_list[i], msg->data);
                        pthread_mutex_unlock(&locks[i]);
			            success = send_msg(client_list[i], JN_ACK, "server", msg->data, i);
		    	        if (success == -1) {
		                    printf("FAILED: Send failed!\n");
                            pthread_mutex_lock(&locks[i]);
                            free(client_list[i]);
                            client_list[i] = NULL;
                            memset((void *)session_list[i], 0, sizeof(session_list[i]));
                            pthread_mutex_unlock(&locks[i]);
		                    return NULL;
		                }
			            break;
		            }
		        }
		        if (j == 5) {
			        strcat(data, msg->data);
		            strcat(data, ",");
		            strcat(data, " Could not find session!");
			        success = send_msg(client_list[i], JN_NAK, "server", data, i);
		    	    if (success == -1) {
		                printf("FAILED: Send failed!\n");
                        pthread_mutex_lock(&locks[i]);
                        free(client_list[i]);
                        client_list[i] = NULL;
                        memset((void *)session_list[i], 0, sizeof(session_list[i]));
                        pthread_mutex_unlock(&locks[i]);
		                return NULL;
		            }
		        }
            }
	    } else {
	        printf("Invalid command!\n");
	    }
	}
	return NULL;	  
}

void setup_server_struct(struct sockaddr_in* addr, int listen_port) {
    addr->sin_family = AF_INET;
    addr->sin_port = htons(listen_port);
    addr->sin_addr.s_addr = htonl(INADDR_ANY);
    memset(addr->sin_zero, 0, 8); 
    return;
}

int main(int argc, char* argv[]){
    printf("Generating mutexes...\n");
    FILE* fp;
    int i = 0;
    for (i = 0; i < 6; i++) {
        int locked = pthread_mutex_init(&locks[i], NULL);
	    if (locked != 0) {
	        printf("FAILED: Unable to initialize locks!\n");
	        return -1;
	    }
    }
    printf("PASSED: Mutexes initialized!\n");

    //From the command line arguments get the port we will be listening on
    printf("Parsing command line arguments...\n");
    srand(time(0));
    int listen_port = 0;
    int open_connection = -1;
    int bound = -1;
    if (argc != 2) {
        printf("FAILED: Invalid Arguments! -> Expected Number Of Arguments: 1\n");
        return -1;
    } else {
        char netbuffer[4096];
        int num_ports = 0;
        char cmd[100] = "netstat -na | grep \":";
        strcat(cmd, argv[1]);
        strcat(cmd, "\"");
        fp = popen(cmd, "r");
        num_ports = fread((void *)netbuffer, 1, 4096, fp);
        if (num_ports > 0) {
            printf("FAILED: Port already in use!\n");
            pclose(fp);
            return -1;
        }
        pclose(fp);
        listen_port = atoi(argv[1]);
        printf("PASSED: Valid Arguments!\n");
    }
    
    int sockfd, new_fd;

   // !! don't forget your error checking for these calls !!

   // first, load up address structs with getaddrinfo():

    struct sockaddr_in addr;
    setup_server_struct(&addr, listen_port);

    // make a socket, bind it, and listen on it:

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bound = bind(sockfd, (struct sockaddr *)&addr, (socklen_t)sizeof(addr));   

    if (bound != 0) {
        printf("FAILED: Binding Failed!\n");
        close(sockfd);
        return -2;
    }

    printf("PASSED: Binding Succeeded!\n");

    while(1) {
	    struct sockaddr_storage their_addr;
        socklen_t addr_size;
	    struct client_args* c = (struct client_args*)malloc(sizeof(struct client_args));
	  
	    addr_size = sizeof(their_addr);
        printf("Listening for a connection...\n");
        open_connection = listen(sockfd, 30);
        if (open_connection < 0) {
	        printf("FAILED: Listen failed!");
	        close(sockfd);
	        return -1;
	    }
    	// now accept an incoming connection:
	    // blocks until a request is received
        printf("Waiting for a connection...\n");
    	new_fd = accept(sockfd, (struct sockaddr *restrict)&their_addr, (socklen_t *restrict)&addr_size);
	    if (new_fd == -1) {
	        printf("FAILED: Accept failed!");
	        close(sockfd);
	        return -1;
	    } else {
	        printf("PASSED: Received a valid connection!");
	        c->addr = their_addr;
	        c->addr_size = addr_size;
	        c->sock_fd = new_fd;
	        memset((void *)c->send_buffer, 0, sizeof(c->send_buffer));
	        memset((void *)c->receive_buffer, 0, sizeof(c->receive_buffer));
	        pthread_t client_thread;
	        printf("Creating thread to handle request...");
	        int thread_create = pthread_create(&client_thread, NULL, handle_client_request, c);
	        if (thread_create == 0) {
                printf("Thread created successfully!");
	        } else {
                printf("FAILED: Thread creation unsuccessful!");
		        close(sockfd);
		        return -1;
	        }
	    }
    }
    close(sockfd);
    return 0;
}




    
    
