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

void setup_server_struct(struct sockaddr_in* addr, int port) {
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = htonl(INADDR_ANY);
    memset(addr->sin_zero, 0, 8); 
    return;
}

int main(int argc, char* argv[]){
    
    int port, num_connections, sockfd, newfd, bound, connecting, num_bytes_rec, num_bytes_send;
    char receive_buffer[4096];
    char send_buffer[4096];
    struct sockaddr_in server_addr;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_size;
    FILE* fp = NULL;

    if (argc != 3) {
        printf("FAILED: Not enough arguments\n");
        return -1;
    } else {
        port = atoi(argv[1]);
	num_connections = atoi(argv[2]);
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

    fp = fopen("./temp.txt", "wb");

    while(1) {

    	connecting = listen(sockfd, num_connections);

    	if (connecting < 0) {
		printf("FAILED: Listen failed!");
		close(sockfd);
		fclose(fp);
		return -1;
    	}

    	newfd = accept(sockfd, (struct sockaddr *restrict)&client_addr, (socklen_t *restrict)&client_addr_size);

	if (newfd < 0) {
		printf("FAILED: Accept failed!");
		close(newfd);
		close(sockfd);
		fclose(fp);
		return -1;
	}

	memset((void*)receive_buffer, 0, sizeof(receive_buffer));
	num_bytes_rec = recv(newfd, receive_buffer, sizeof(receive_buffer), 0);

	if (num_bytes_rec < 0) {
		printf("FAILED: Receive failed!");
		perror("This is the error: ");
		close(newfd);
		close(sockfd);
		fclose(fp);
		return -1;
	}

	while (num_bytes_rec > 0) {
		fwrite((const void*)receive_buffer, num_bytes_rec, 1, fp);
		memset((void*)receive_buffer, 0, sizeof(receive_buffer));
		num_bytes_rec = recv(newfd, receive_buffer, sizeof(receive_buffer), 0);		
	}
	
	//send
	sprintf(send_buffer, "%s\n", "test");
	num_bytes_send = send(newfd, send_buffer, sizeof(send_buffer), 0);
	
	if (num_bytes_send < 0) {
		printf("FAILED: Send failed!");
		perror("This is the error: ");
		close(newfd);
		close(sockfd);
		fclose(fp);
		return -1;
	}

	close(newfd);
	close(sockfd);	
	fclose(fp);	
	return 0;
    }
}




    
    
