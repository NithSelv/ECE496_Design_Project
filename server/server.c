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
#include <zlib.h>

void setup_server_struct(struct sockaddr_in* addr, int port) {
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = htonl(INADDR_ANY);
    memset(addr->sin_zero, 0, 8); 
    return;
}

int main(int argc, char* argv[]){
    
    int port, num_connections, sockfd, newfd, bound, connecting, num_bytes, total_bytes;
    char receive_buffer[4096];
    char header[4096] = "HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nContent-Type: text/plain; version=0.0.4; charset=utf-8\r\n\r\n";
    char body[4096] = "# HELP random_metric1 Totally random value p1.\n# TYPE random_metric1 counter\nrandom_metric1 100.32\n";
    char send_buffer[4096];
    char send_buffer2[4096];
    struct sockaddr_in server_addr;

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

    while(1) {

	struct sockaddr_storage client_addr;
    	socklen_t client_addr_size;

    	connecting = listen(sockfd, num_connections);

    	if (connecting < 0) {
		printf("FAILED: Listen failed!");
		close(sockfd);
		return -1;
    	}

    	newfd = accept(sockfd, (struct sockaddr *restrict)&client_addr, (socklen_t *restrict)&client_addr_size);

    	if (newfd < 0) {
		printf("FAILED: Accept failed!");
		close(newfd);
		close(sockfd);
		return -1;
    	}

	memset((void*)receive_buffer, 0, sizeof(receive_buffer));
	num_bytes = recv(newfd, receive_buffer, sizeof(receive_buffer), 0);

	if (num_bytes < 0) {
		printf("FAILED: Receive failed!");
		perror("THis is the error: ");
		close(newfd);
		close(sockfd);
		return -1;
	}

	memset((void*)send_buffer, 0, sizeof(send_buffer));
	memset((void*)send_buffer2, 0, sizeof(send_buffer2));

	strcpy(send_buffer, body);
	strcpy(send_buffer2, header);

	z_stream zs;
	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	zs.opaque = Z_NULL;
	zs.avail_in = (uInt)sizeof(send_buffer);
	zs.next_in = (Bytef *)send_buffer;
	zs.avail_out = (uInt)(sizeof(send_buffer2) - strlen(send_buffer2));
	zs.next_out = (Bytef *)(send_buffer2+strlen(send_buffer2));
	deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY);
	deflate(&zs, Z_FINISH);
	deflateEnd(&zs); 
	

	total_bytes = 0;
	while (total_bytes != sizeof(send_buffer2)) {
		num_bytes = send(newfd, send_buffer2+total_bytes, sizeof(send_buffer2) - total_bytes, 0);

		if (num_bytes < 0) {
			printf("FAILED: Send failed!");
			close(newfd);
			close(sockfd);
			return -1;
		}
		total_bytes += num_bytes;
	}
	printf("One connection handled!\n");
	
    }

    close(newfd);
    close(sockfd);		
    return 0;
}




    
    
