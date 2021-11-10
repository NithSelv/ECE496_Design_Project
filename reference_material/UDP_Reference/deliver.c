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
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>

//This function is used to set up addr so that messages can be sent to the server
void setup_server_address(int* sock, struct sockaddr_in* addr, char* server_address, int server_port); 

int main(int argc, char* argv[]) {

    //Check and parse the command line arguments to obtain the server address and port
    printf("Checking arguments...\n");
    char* server_address;
    int server_port;
    struct timeval start_clock;
    struct timeval end_clock;
    void * tzp = NULL;
    struct timeval timeout;
    if (argc != 3) {
        printf("FAILED: Arguments Invalid! -> Number Of Arguments Expected: 2!\n");
        return -1;
    } else {
        printf("PASSED: Arguments Valid!\n");
        server_address = argv[1];
        server_port = atoi(argv[2]);
    }

    //Create the UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    //Create the sockaddr_in struct and initialize it appropiately
    struct sockaddr_in address;

    //These buffers will hold client requests
    char command[4096] = "";
    char file_name[4096] = "";
    char line[4096] = "";

    //These buffers will hold messages for sending and receiving
    char send_buffer[4096] = "";
    char receive_buffer[4096] = ""; 

    //These are strings that hold important keywords
    char ftp[4] = "ftp";
    char pos_reply[4] = "yes";
    char neg_reply[3] = "no";

    setup_server_address(&sock, &address, server_address, server_port); 

    //These structs are used to contain the address of the server that we received a message from
    socklen_t addr2_len;
    struct sockaddr_storage addr2; 
    
    //Ask the user to input a command, expecting ftp <valid_file_name>
    //Check if an invalid command was received and if the file is not accessible
    printf("Please enter which file you would like to transfer as: ftp <file_name>...\n");
    fgets(line, sizeof(line), stdin);
    int num_args = sscanf(line,"%s %s", command, file_name);
    if (num_args !=2) {
        printf("FAILED: Incorrect Number Of Arguments -> Expected Number Of Arguments: 2!\n");
        close(sock);
        return -2;
    } else if (strncmp(command, ftp, strlen(ftp)) != 0) {
        printf("FAILED: Invalid Command -> %s %s!\n", command, file_name);
        close(sock);
        return -3;
    }
    if (access(file_name, F_OK) != 0) {
        printf("FAILED: File Is Not Accessible -> %s!\n", file_name);
        close(sock);
        return -4;
    } 
    printf("PASSED: Command Is Valid And File Is Accessible!\n");

    //Create and send the first message "ftp" using send_buffer and sendto
    //Check to make sure sendto returns a nonnegative integer
    printf("Sending request to server...\n");
    strncpy(send_buffer,command,strlen(command));
    gettimeofday(&start_clock, tzp);
    tzp = NULL;
    int num_bytes = sendto(sock, send_buffer, sizeof(send_buffer), 0, (struct sockaddr*)&address, (socklen_t)sizeof(address));
    if (num_bytes < 0) {
        printf("FAILED: Send Failed!\n");
        close(sock);
        return -5;
    }
    printf("PASSED: Sent Message -> %s!\n", send_buffer);

    //Wait for the server to send back the response, expect either a "yes" or a "no"
    //Check if recvfrom returns a nonnegative integer
    printf("Waiting for server response...\n");        
    num_bytes = recvfrom(sock, receive_buffer, sizeof(receive_buffer), 0, (struct sockaddr*)&addr2, &addr2_len);
    gettimeofday(&end_clock, tzp);
    printf("RTT from client to server is %f microseconds\n", (double)(end_clock.tv_usec - start_clock.tv_usec));
    timeout.tv_usec = end_clock.tv_usec - start_clock.tv_usec;
    timeout.tv_sec = end_clock.tv_sec - start_clock.tv_sec;
    if (num_bytes < 0) {
        printf("FAILED: Receive Failed!\n");
        close(sock);
        return -6;
    }
    printf("PASSED: Received Response From Server! -> %s\n", receive_buffer);

    //Check if the return message was a "yes" or unexpected
    printf("Determining if file transfer may start...\n");
    if (strncmp(receive_buffer, pos_reply, strlen(pos_reply)) == 0) {
        printf("A file transfer can start.\n");
    } else if (strncmp(receive_buffer, neg_reply, strlen(neg_reply)) == 0) {
        printf("A file transfer can not start.\n");
    } else {
        printf("FAILED: Unknown Message Received From Server!\n");
        close(sock);
        return -7;
    }

    //New code for lab2
    struct stat s;
    int total_frag = 0;
    char filedata[1000];
    int frag_no = 0;
    memset((void *)filedata, 0, sizeof(filedata));
    stat(file_name, &s);
    if ((s.st_size % 1000) > 0) {
        total_frag++;
    }
    total_frag += s.st_size / 1000;
    printf("Commencing File Transfer...!\n");
    int fd = open(file_name, O_RDONLY);
    for (frag_no = 1; frag_no < (total_frag+1); frag_no++) {
        printf("Reading Fragment Number: %d out of %d fragments\n", frag_no, total_frag);
        memset((void *)send_buffer, 0, sizeof(send_buffer));
        num_bytes = read(fd, (void *)filedata, sizeof(filedata));
        if (num_bytes <= 0) {
            printf("FAILED: Read Failed!\n");
            close(fd);
            close(sock);
            return -8;
        }
        printf("PASSED: Read Successful!\n");
        char temp[100];
        memset((void *)temp, 0, sizeof(temp));
        sprintf(temp, "%d", total_frag);
        strcpy(send_buffer, temp);
        strcat(send_buffer, ":");
        memset((void *)temp, 0, sizeof(temp));
        sprintf(temp, "%d", frag_no);
        strcat(send_buffer, temp);
        strcat(send_buffer, ":");
        memset((void *)temp, 0, sizeof(temp));
        sprintf(temp, "%d", num_bytes);
        strcat(send_buffer, temp);
        strcat(send_buffer, ":");
        strcat(send_buffer, file_name);
        strcat(send_buffer, ":");
        char * t = (char *)strrchr(send_buffer, ':');
        t++;
        memcpy((void *)(t), (void *)(filedata), num_bytes);
        printf("Sending Packet Number: %d!\n", frag_no);
        num_bytes = sendto(sock, send_buffer, sizeof(send_buffer), 0, (struct sockaddr*)&address, (socklen_t)sizeof(address));
        if (num_bytes < 0) {
            printf("FAILED: Send Failed!\n");
            close(fd);
            close(sock);
            return -5;
        }
        printf("PASSED: Sent Message!\n");
        printf("Hey this is packet size %ld\n", sizeof(send_buffer));
        memset((void *)receive_buffer, 0, sizeof(receive_buffer));
        printf("Receiving ACK...\n");
        int receive_ACK = 0;
        while (receive_ACK == 0) {
            int time_set = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
            if (time_set < 0) {
                printf("Failed: Was not able to set the time interval for recvfrom!\n");
                close(fd);
                close(sock);
                return -7;
            }

            num_bytes = recvfrom(sock, receive_buffer, sizeof(receive_buffer), 0, (struct sockaddr*)&addr2, &addr2_len);
            if ((num_bytes == -1) && ((errno == EAGAIN)||(errno == EWOULDBLOCK))) {
                printf("Did not receive ACK! Packet was dropped!\n");
                printf("Retransmitting Packet Number: %d!\n", frag_no);
                num_bytes = sendto(sock, send_buffer, sizeof(send_buffer), 0, (struct sockaddr*)&address, (socklen_t)sizeof(address));
                if (num_bytes < 0) {
                    printf("FAILED: Retransmit Failed!\n");
                    close(fd);
                    close(sock);
                    return -5;
                }
                printf("PASSED: Retransmitted Message!\n");
                printf("Hey this is packet size %ld\n", sizeof(send_buffer));
                memset((void *)receive_buffer, 0, sizeof(receive_buffer));
                printf("Receiving ACK...\n");
                continue;
            } else if (num_bytes < 0) {
                printf("FAILED: Receive Failed!\n");
                close(fd);
                close(sock);
                return -6;
            } else {
                receive_ACK = 1;
            }
        }
        printf("PASSED: Received ACK From Server! -> %s\n", receive_buffer);
        printf("Preparing next packet...\n");
    }
    printf("File successfully transferred!\n");
    close(fd);  
    close(sock);
    return 0;
}

//As mentioned above, this function exists to setup addr to use for sending messages to the server    
void setup_server_address(int* sock, struct sockaddr_in* addr, char* server_address, int server_port) {
    addr->sin_family = AF_INET;
    addr->sin_port = htons(server_port);
    inet_pton(AF_INET, server_address, (void *)&(addr->sin_addr.s_addr));
    memset(addr->sin_zero, 0, 8); 
    return;
}
    
