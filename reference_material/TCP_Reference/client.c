//We would like to give acknowledgement to Brian "Beej Jorgensen" Hall and his "Beej's Guide
//To Network Programming" pdf which helped with setting up the sockets
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include "message.h"
#include <pthread.h>

//LOGIN 0
//LO_ACK 1
//LO_NAK 2
//EXIT 3
//JOIN 4
//JN_ACK 5
//JN_NAK 6
//LEAVE_SESS 7
//NEW_SESS 8
//NS_ACK 9
//MESSAGE 10
//QUERY 11
//QU_ACK 12

static char USER_ID[32]  = "";  

enum PACKET_TYPE{LOGIN,LO_ACK,LO_NAK,EXIT,JOIN,JN_ACK,JN_NAK,LEAVE_SESS,NEW_SESS,NS_ACK,MESSAGE,QUERY,QU_ACK};

//available input command
static const char LOGINCMD[] = "/login";
static const char LOGOUT[] = "/logout";
static const char JOINSESS[] = "/joinsession";
static const char LEAVESESS[] = "/leavesession";
static const char CREATESESS[] = "/createsession";
static const char GETLIST[] = "/list";
static const char QUIT[] = "/quit";

void logout(int sockfd){
    struct Message command_message;
    command_message.type = EXIT; 
    strcpy(command_message.source, USER_ID);
    command_message.size = 0;
    strcpy(command_message.data, "");	
    printf("Sending logout request to server...\n");
    char send_buffer[4096] = "";
    char* temp_char = messageToBuffer(&command_message);
    strcpy(send_buffer,temp_char);
    printf("Sending Message packet:%s\n",send_buffer);
    int total_bytes = 0;
    int num_bytes = 0;
    while (total_bytes < 4096) {
        num_bytes = send(sockfd, send_buffer+total_bytes, sizeof(send_buffer)-total_bytes, 0);	
        if(num_bytes<0) {
    	    printf("Message sent error!\n");
            close(sockfd);
            return;
        }	
        total_bytes += num_bytes;
    }
	
}	
void getlist(int sockfd){
    struct Message command_message;
    command_message.type = QUERY; 
    strcpy(command_message.source, USER_ID);
    command_message.size = 0;
    strcpy(command_message.data, "");	
    printf("Sending GET_LIST request to server...\n");
    char send_buffer[4096] = "";
    char* temp_char = messageToBuffer(&command_message);
    strcpy(send_buffer,temp_char);
    printf("Sending Message packet:%s\n",send_buffer);
    int total_bytes = 0;
    int num_bytes = 0;
    while (total_bytes < 4096) {
        num_bytes = send(sockfd, send_buffer+total_bytes, sizeof(send_buffer)-total_bytes, 0);	
        if(num_bytes<0) {
    	    printf("Message sent error!\n");
            close(sockfd);
            return;
        }	
        total_bytes += num_bytes;
    }    

}
void joinsess(int sockfd, char *sessID){
    struct Message command_message;
    command_message.type = JOIN; 
    strcpy(command_message.source, USER_ID);
    command_message.size = strlen(sessID);
    strcpy(command_message.data, sessID);	
    printf("Sending JOIN_SESS request to server...\n");
    char send_buffer[4096] = "";
    char* temp_char = messageToBuffer(&command_message);
    strcpy(send_buffer,temp_char);
    printf("Sending Message packet:%s\n",send_buffer);
    int total_bytes = 0;
    int num_bytes = 0;
    while (total_bytes < 4096) {
        num_bytes = send(sockfd, send_buffer+total_bytes, sizeof(send_buffer)-total_bytes, 0);	
        if(num_bytes<0) {
    	    printf("Message sent error!\n");
            close(sockfd);
            return;
        }	
        total_bytes += num_bytes;
    }   
}	

void leavesess(int sockfd){
    struct Message command_message;
    command_message.type = LEAVE_SESS; 
    strcpy(command_message.source, USER_ID);
    command_message.size = 0;
    strcpy(command_message.data, "");		
    printf("Sending LEAVE_SESS request to server...\n");
    char send_buffer[4096] = "";
    char* temp_char = messageToBuffer(&command_message);
    strcpy(send_buffer,temp_char);
    printf("Sending Message packet:%s\n",send_buffer);
    int total_bytes = 0;
    int num_bytes = 0;
    while (total_bytes < 4096) {
        num_bytes = send(sockfd, send_buffer+total_bytes, sizeof(send_buffer)-total_bytes, 0);	
        if(num_bytes<0) {
    	    printf("Message sent error!\n");
            close(sockfd);
            return;
        }	
        total_bytes += num_bytes;
    }	    
}	
void createsess(int sockfd, char *sessID){
    struct Message command_message;
    command_message.type = NEW_SESS; 
    strcpy(command_message.source, USER_ID);
    command_message.size = strlen(sessID);
    strcpy(command_message.data, sessID);	
    printf("Sending CREATE_SESS request to server...\n");
    char send_buffer[4096] = "";
    char* temp_char = messageToBuffer(&command_message);
    strcpy(send_buffer,temp_char);
    printf("Sending Message packet:%s\n",send_buffer);
    int total_bytes = 0;
    int num_bytes = 0;
    while (total_bytes < 4096) {
        num_bytes = send(sockfd, send_buffer+total_bytes, sizeof(send_buffer)-total_bytes, 0);	
        if(num_bytes<0) {
    	    printf("Message sent error!\n");
            close(sockfd);
            return;
        }	
        total_bytes += num_bytes;
    }	
}
void sendmess(int sockfd, char *message) {
    struct Message command_message;
    command_message.type = MESSAGE; 
    strcpy(command_message.source, USER_ID);
    command_message.size = strlen(message);
    strcpy(command_message.data, message);	
    printf("Sending MESSAGE to SESSION...\n");
    char send_buffer[4096] = "";
    char* temp_char = messageToBuffer(&command_message);
    strcpy(send_buffer,temp_char);
    printf("Sending Message packet:%s\n",send_buffer);
    int total_bytes = 0;
    int num_bytes = 0;
    while (total_bytes < 4096) {
        num_bytes = send(sockfd, send_buffer+total_bytes, sizeof(send_buffer)-total_bytes, 0);	
        if(num_bytes<0) {
    	    printf("Message sent error!\n");
            close(sockfd);
            return;
        }	
        total_bytes += num_bytes;
    }
} 

void *receive(void *sockfd) {
    int* received_sock = (int *)sockfd;
    int total_bytes = 0;
    int num_bytes = 0;
    while(1){
	    char receive_buffer[4096] = "";
        total_bytes = 0;
        while (total_bytes < 4096) {
	        num_bytes = recv(*received_sock, receive_buffer+total_bytes, sizeof(receive_buffer)-total_bytes, 0);
            if (num_bytes < 0) {
                printf("FAILED: Send Failed!\n");
                close(*received_sock);
                return NULL;
            }
            total_bytes += num_bytes;
        }
            
        printf("Received Response From Server! -> %s\n", receive_buffer);
        struct Message *received = bufferToMessage(receive_buffer);
	    if(num_bytes < 0) {
		    printf("Receive error. Receive again\n");
	    } else if (received->type == JN_ACK) {
	        printf("Join session successfully to %s\n",received->data);
	    } else if (received->type == JN_NAK) {
	        printf("Join session Failed %s\n",received->data);
	    } else if (received->type == NS_ACK) {
	        printf("Create session successfully.\n");
	    } else if (received->type == MESSAGE) {
	        printf("receive message from %s: %s\n",received->source, received->data);	
	    } else if (received->type == QU_ACK) {
	        printf("List of User:%s\n",received->data);
	    }
    }	    
    return NULL;
}

//As mentioned above, this function exists to setup addr to use for sending messages to the server    
void setup_server_address(int* sock, struct sockaddr_in* addr, char* server_address, int server_port) {
    addr->sin_family = AF_INET;
    addr->sin_port = htons(server_port);
    inet_pton(AF_INET, server_address, (void *)&(addr->sin_addr.s_addr));
    memset(addr->sin_zero, 0, 8); 
    return;
}

int main(void) {

    //Check and parse the command line arguments to obtain the server address and port
    printf("Ready to login: /login + account + password + host + port:\n");

    while (1) {

        //if inputs exit then logout + exit the loop
	    //else wait for login     
        char server_address[32] = "";
        char server_port[32] = "";
        char command[4096] = "";
        char line[4096] = "";
        char receive_buffer[4096] = ""; 
	    char otherdata[4096] = "";
	    char username[32] = "";
	    char password[32] = "";
        int sockfd;
        
	    while(1){
	        fgets(line, sizeof(line), stdin);
            int num_args = sscanf(line,"%s %s %s %s %s", command, username,password,server_address,server_port);
	        printf("%s %s %s %s %s\n", command, username,password,server_address,server_port);
	    
	        if (strcmp(command,QUIT) == 0) {
	  	        printf("No Logout required. Quit successfully.\n");
		        return 0;
	        }
	        if (strcmp(command,LOGINCMD) == 0 && num_args == 5) {
		        printf("logging...\n");
		        break;
	        }
	        printf("Not enough argument, please enter again:\n");
	    }
  
        // make a socket:
  
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in address;
        printf("This is the server_address: %s, and the server port: %d\n", server_address, atoi(server_port));
        setup_server_address(&sockfd, &address, server_address, atoi(server_port)); 
  
        // connect!

        int connect_byte = connect(sockfd, (const struct sockaddr*)&address, (socklen_t)sizeof(address));	
        if (connect_byte < 0) {
            perror("Error: ");
            //printf("FAILED: connect error!\n");
            close(sockfd);
            return -3;
        }		

        printf("TCP Connection is successful, waiting for loggin.\n");	

        struct Message command_message;
        command_message.type = LOGIN; 
        strcpy(command_message.source, username);
        command_message.size = sizeof(password);
        strcpy(command_message.data, password);
        
    

        printf("Sending request to server...\n");

        char send_buffer[4096] = "";
        char* temp_char = messageToBuffer(&command_message);
        strcpy(send_buffer,temp_char);

        int num_bytes = 0;
        int total_bytes = 0;

        while (total_bytes < 4096) {
            num_bytes = send(sockfd, send_buffer+total_bytes, sizeof(send_buffer)-total_bytes, 0);
            if (num_bytes < 0) {
                printf("FAILED: Send Failed!\n");
                close(sockfd);
                return -5;
            }
            total_bytes += num_bytes;
        }      
        printf("PASSED: Sent Message -> %s!\n", send_buffer);

        printf("Waiting for server ACK Login...\n");  
        total_bytes = 0;
        while (total_bytes < 4096) {
            num_bytes = recv(sockfd, receive_buffer+total_bytes, sizeof(receive_buffer)-total_bytes, 0);
            if (num_bytes < 0) {
                printf("FAILED: Receive Failed!\n");
                close(sockfd);
                return -6;
            }
            total_bytes += num_bytes;
        }
        printf("Received Response From Server! -> %s\n", receive_buffer);

        struct Message *login_ack = bufferToMessage(receive_buffer);
	    bool LOGIN_BOOL = 0;
	    pthread_t receiving_thread; 
	    if (login_ack->type == LO_ACK) {
	        printf("Login successfully!\n");
    	    strcpy(USER_ID,username);		    
	        LOGIN_BOOL = 1;
	        int thread_create = pthread_create(&receiving_thread, NULL, receive, &sockfd);
	        while(thread_create != 0) {
	    	    printf("create thread failed do it again\n");
	            thread_create = pthread_create(&receiving_thread, NULL, receive, &sockfd);
	        }		    
	    } else {
	        printf("Login ack failed: %s\n", login_ack->data);		
	    }
	    //Start receive pthread
	    //
	
	    while(LOGIN_BOOL) {
	        strcpy(otherdata, "");
	        printf("Please Enter Command:\n");
	        fgets(line, sizeof(line), stdin);
            sscanf(line,"%s %s", command, otherdata);
	        if (strcmp(command,LOGINCMD) == 0 ) {
		        printf("The account is logged in already:%s",username);
	        } else if (strcmp(command,LOGOUT) == 0 ) {
	    	    logout(sockfd);
	    	    break;
	        } else if (strcmp(command,QUIT) == 0 ) {
		        logout(sockfd);
		        pthread_cancel(receiving_thread);
		        printf("QUIT...Have a good day!!!\n");
		        close(sockfd);
		        return 0;
	        } else if (strcmp(command,GETLIST) == 0 ) {
		        getlist(sockfd);	
	        } else if (strcmp(command,JOINSESS) == 0 ) {
		        joinsess(sockfd,otherdata);
	        } else if (strcmp(command,LEAVESESS) == 0 ) {
	    	    leavesess(sockfd);
	        } else if (strcmp(command,CREATESESS) == 0) {
		        createsess(sockfd,otherdata);
	        } else {
	    	    sendmess(sockfd,line);
	        }

	    }
	    //end with receive pthread
	    pthread_cancel(receiving_thread);

	    if(LOGIN_BOOL == 1) {
            printf("logged out!\n");
	    }    
        close(sockfd);
	    printf("Closed sockfd. Please Login again:\n");
    }	
    return 0;
}

