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

//This function is used to initialize the sockaddr_in struct for the server
void setup_server_struct(struct sockaddr_in* addr, int listen_port);

int mkdir(const char* pathname, mode_t mode);

int main(int argc, char* argv[]){
    //From the command line arguments get the port we will be listening on
    printf("Parsing command line arguments...\n");
    srand(time(0));
    int listen_port = 0;
    clock_t start_clock;
    clock_t end_clock;
    int frag_no = 0;
    //size_t pkt_frag_no = 0;
    int size = 0;
    //size_t pkt_size = 0;
    char filedata[1000];
    //char pkt_filedata[1000];
    //char* pkt_file_name = NULL;
    int total_frag = 0;
    //size_t pkt_total_frag = 0;
    if (argc != 2) {
        printf("FAILED: Invalid Arguments! -> Expected Number Of Arguments: 1\n");
        return -1;
    } else {
        listen_port = atoi(argv[1]);
        printf("PASSED: Valid Arguments!\n");
    }
    

    //Create the UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    //Create a sockaddr struct and initialize it with the host ip address and given port
    //Also, the padding should be null-terminated since we're dealing with strings
    struct sockaddr_in addr;
    setup_server_struct(&addr, listen_port);

    //Bind the socket to the given ip and port
    //Also, check to make sure bind worked
    printf("Binding server...\n");
    int bound = bind(sock, (struct sockaddr *)&addr, (socklen_t)sizeof(addr));
    if (bound != 0) {
        printf("FAILED: Binding Failed!\n");
        close(sock);
        return -2;
    }
    printf("PASSED: Binding Succeeded!\n");

    //The server doesn't know the source address of the machine that will try to send a message to it
    //so we need to have these empty structs which will be used to obtain the address from the packet
    struct sockaddr_storage addr2;
    socklen_t addr2_len;

    //These are buffers used to hold messages for sending and receiving
    char send_buffer[4096] = "";
    char receive_buffer[4096] = "";

    //These are some strings to hold some keywords
    char command[4] = "ftp";
    char pos_reply[4] = "yes";
    char neg_reply[3] = "no";
        
    //Receive the first message and store it in the receive_buffer, this should be "ftp"
    //Also, check if recvfrom failed by making sure that it returns a nonnegative value
    printf("Waiting for request from client...\n");
    int num_bytes = recvfrom(sock, receive_buffer, sizeof(receive_buffer), 0, (struct sockaddr*)&addr2, &addr2_len);   
    if (num_bytes < 0) {
        printf("FAILED: Receive Failed!\n");
        close(sock);
        return -3;
    }
    printf("PASSED: Received Message! -> %s\n", receive_buffer);
        
    //Check to make sure that the message is actually "ftp" and send "yes" if it is and "no" otherwise
    //Check to make sure sendto returns a nonnegative integer
    printf("Determining response...\n");
    if (strncmp(receive_buffer, command, strlen(command)) == 0) {
        printf("Sending Response: yes...\n");
        strncpy(send_buffer,pos_reply, strlen(pos_reply));
        num_bytes = sendto(sock, send_buffer, sizeof(send_buffer), 0, (struct sockaddr*)&addr2, addr2_len);
        start_clock = clock();
        //sleep(2);
    } else {
        printf("Sending Response: no...\n");
        strncpy(send_buffer,neg_reply, strlen(neg_reply));
        num_bytes = sendto(sock, send_buffer, sizeof(send_buffer), 0, (struct sockaddr*)&addr2, addr2_len);
    }

    if (num_bytes < 0) {
        printf("FAILED: Send Failed!\n");
        close(sock);
        return -4;
    }
    printf("PASSED: Sent Response! -> %s\n", send_buffer);

    //New part for lab2
    printf("Commencing File Transfer...\n");
    memset((void *)filedata, 0, sizeof(filedata));
    memset((void *)receive_buffer, 0, sizeof(receive_buffer));
    num_bytes = recvfrom(sock, receive_buffer, sizeof(receive_buffer), 0, (struct sockaddr*)&addr2, &addr2_len);  
    end_clock = clock();
    printf("RTT from server to client is: %f seconds\n", (double)(end_clock - start_clock) / CLOCKS_PER_SEC);
    //sleep(2); 
    if (num_bytes < 0) {
        printf("FAILED: Receive Failed!\n");
        close(sock);
        return -3;
    }
    printf("PASSED: Received Packet %s!\n", receive_buffer);
    char temp[100];
    memset((void *)temp, 0, sizeof(temp));
    int start_index = 0;
    int end_index = 0;
    while (receive_buffer[end_index] != ':') {
        end_index++;
    }
    memcpy((void *)temp, (void *)&(receive_buffer[start_index]), end_index - start_index);
    total_frag = atoi(temp);
    printf("Here is total_frag: %d\n", total_frag);
    start_index = end_index+1;
    end_index++;
    while (receive_buffer[end_index] != ':') {
        end_index++;
    }
    memset((void *)temp, 0, sizeof(temp));
    memcpy((void *)temp, (void *)&(receive_buffer[start_index]), end_index - start_index);
    start_index = end_index+1;
    end_index++;
    frag_no = atoi(temp);
    printf("Here is frag_no: %d\n", frag_no);
    while (receive_buffer[end_index] != ':') {
        end_index++;
    }
    memset((void *)temp, 0, sizeof(temp));
    memcpy((void *)temp, (void *)&(receive_buffer[start_index]), end_index - start_index);
    start_index = end_index+1;
    end_index++;
    size = atoi(temp);
    printf("Here is size: %d\n", size);
    while (receive_buffer[end_index] != ':') {
        end_index++;
    }
    memset((void *)temp, 0, sizeof(temp));
    memcpy((void *)temp, (void *)&(receive_buffer[start_index]), end_index - start_index);
    printf("Here is file_name: %s\n", temp);
    int file_name_size = end_index - start_index;
    start_index = end_index+1;
    end_index++;
    memcpy((void *)filedata, (void *)&(receive_buffer[start_index]), size);
    printf("Here is file_data: %s\n", filedata);
    printf("Writing Packet\n");
    mkdir("server_output", 0777);
    char path[100] = "./server_output/";
    FILE* fd = fopen(strncat(path, temp, file_name_size), "wb");
    num_bytes = fwrite((const void *)filedata, size, 1, fd);
    if (num_bytes < 0) {
        printf("FAILED: Write Failed!\n");
        fclose(fd);
        close(sock);
        return -3;
    }
    printf("PASSED: Write Passed!\n");
    printf("Sending ACK to Client...!\n");
    memset((void *)send_buffer, 0, sizeof(send_buffer));
    strncpy(send_buffer,pos_reply, strlen(pos_reply));
    num_bytes = sendto(sock, send_buffer, sizeof(send_buffer), 0, (struct sockaddr*)&addr2, addr2_len);
    if (num_bytes < 0) {
        printf("FAILED: Send Failed!\n");
        fclose(fd);
        close(sock);
        return -4;
    }
    printf("PASSED: Sent ACK! -> %s\n", send_buffer);

    while (frag_no != total_frag) {
        memset((void *)filedata, 0, sizeof(filedata));
        memset((void *)receive_buffer, 0, sizeof(receive_buffer));
        num_bytes = recvfrom(sock, receive_buffer, sizeof(receive_buffer), 0, (struct sockaddr*)&addr2, &addr2_len);  
        //sleep(2); 
        if (num_bytes < 0) {
            printf("FAILED: Receive Failed!\n");
            fclose(fd);
            close(sock);
            return -3;
        }

        int rand_num = rand() % 100;
        if (rand_num < 10) {
            printf("Packet has been dropped! Packet needs to be retransmitted!\n");
            continue;
        }

        printf("PASSED: Received Packet %s!\n", receive_buffer);
        char temp[100];
        memset((void *)temp, 0, sizeof(temp));
        int start_index = 0;
        int end_index = 0;
        while (receive_buffer[end_index] != ':') {
            end_index++;
        }
        memcpy((void *)temp, (void *)&(receive_buffer[start_index]), end_index - start_index);
        total_frag = atoi(temp);
        printf("Here is total_frag: %d\n", total_frag);
        start_index = end_index+1;
        end_index++;
        while (receive_buffer[end_index] != ':') {
            end_index++;
        }
        memset((void *)temp, 0, sizeof(temp));
        memcpy((void *)temp, (void *)&(receive_buffer[start_index]), end_index - start_index);
        start_index = end_index+1;
        end_index++;
        frag_no = atoi(temp);
        printf("Here is frag_no: %d\n", frag_no);
        while (receive_buffer[end_index] != ':') {
            end_index++;
        }
        memset((void *)temp, 0, sizeof(temp));
        memcpy((void *)temp, (void *)&(receive_buffer[start_index]), end_index - start_index);
        start_index = end_index+1;
        end_index++;
        size = atoi(temp);
        printf("Here is size: %d\n", size);
        while (receive_buffer[end_index] != ':') {
            end_index++;
        }
        memset((void *)temp, 0, sizeof(temp));
        memcpy((void *)temp, (void *)&(receive_buffer[start_index]), end_index - start_index);
        printf("Here is file_name: %s\n", temp);
        if (strncmp(&(path[16]), temp, file_name_size) != 0) {
            printf("FAILED: Wrong Filename!\n");
            fclose(fd);
            close(sock);
            return -3;
        }
        start_index = end_index+1;
        end_index++;
        memcpy((void *)filedata, (void *)&(receive_buffer[start_index]), size);
        printf("Here is file_data: %s\n", filedata);
        printf("Writing Packet\n");
        num_bytes = fwrite((const void *)filedata, size, 1, fd);
        if (num_bytes < 0) {
            printf("FAILED: Write Failed!\n");
            fclose(fd);
            close(sock);
            return -3;
        }
        printf("PASSED: Write Passed!\n");
        printf("Sending ACK to Client...!\n");
        memset((void *)send_buffer, 0, sizeof(send_buffer));
        strncpy(send_buffer,pos_reply, strlen(pos_reply));
        num_bytes = sendto(sock, send_buffer, sizeof(send_buffer), 0, (struct sockaddr*)&addr2, addr2_len);
        if (num_bytes < 0) {
            printf("FAILED: Send Failed!\n");
            fclose(fd);
            close(sock);
            return -4;
        }
        printf("PASSED: Sent ACK! -> %s\n", send_buffer);
    }
    printf("File successfully transferred!\n");
    
    //The server is done responding to the client, there is no need to keep the socket open
    //Also, we could let the server keep polling, but that is not needed for this lab
    fclose(fd);
    close(sock);
    return 0;
}

//Initialize addr with the ip type, host ip address and given port
//Also, the padding should be null-terminated since we're dealing with strings
void setup_server_struct(struct sockaddr_in* addr, int listen_port) {
    addr->sin_family = AF_INET;
    addr->sin_port = htons(listen_port);
    addr->sin_addr.s_addr = htonl(INADDR_ANY);
    memset(addr->sin_zero, 0, 8); 
    return;
}
    
    
