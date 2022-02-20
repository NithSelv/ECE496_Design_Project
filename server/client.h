#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

//This class is used to handle the client communications
class Client {
    //These are private variables and functions used to manipulate send and recv buffers
    private:
	int sockfd;
	struct sockaddr_storage client_addr;
	struct timeval recv_timer;
	struct timeval send_timer;
    	socklen_t client_addr_size;
	char recv_buffer[4096];

	//This private function allows us to set the timeout for receiving messages.
	int Set_Recv_Timeout(int timeout) {
	    this->recv_timer.tv_sec = timeout;
	    if (setsockopt(this->sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&(this->recv_timer), sizeof(this->recv_timer)) < 0) {
		std::cout << "Failed to set timeout!" << std::endl;
		close(this->sockfd);
		return Client::TimeoutFailed;
	    }
	    return Client::Success;
	}

	//This private function allows us to set the timeout for sending messages.
	int Set_Send_Timeout(int timeout) {
	    this->send_timer.tv_sec = timeout;
	    if (setsockopt(this->sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&(this->send_timer), sizeof(this->send_timer)) < 0) {
		std::cout << "Failed to set timeout!" << std::endl;
		close(this->sockfd);
		return Client::TimeoutFailed;
	    }
	    return Client::Success;
	}
    public:
	//Some enums for error codes
	enum Return_Codes {Success = 0, AcceptFailed = -1, TimeoutFailed = -2, ReceiveFailed = -3, SendFailed = -4};
	//Set the initial sockfd to some invalid value
	Client() {
	    this->sockfd = -1;
	    this->recv_buffer[0] = '\0';
	}
	//Accept the connection and populate the client structures
	int Accept(int server_sock) {
	    this->sockfd = accept(server_sock, (struct sockaddr *)&(this->client_addr), (socklen_t *)&(this->client_addr_size));
	    if (this->sockfd < 0) {
		std::cout << "Failed to connect to client!" << std::endl;
		return Client::AcceptFailed;
	    }
	    return Client::Success;
	}
	//Add a timeout for receving messages and store the received message in the buffer
	int Receive(int timeout) {
	    int total_bytes = 0;
	    this->Set_Recv_Timeout(timeout);
	    memset(this->recv_buffer, 0, sizeof(this->recv_buffer)); 
	    int num_bytes = recv(this->sockfd, this->recv_buffer, sizeof(this->recv_buffer)-1, 0);
	    while (num_bytes > 0) {
		total_bytes += num_bytes;
		if (total_bytes >= 4095) {
		    num_bytes = -1;
		    continue;
		}
		num_bytes = recv(this->sockfd, &(this->recv_buffer[total_bytes]), sizeof(this->recv_buffer)-total_bytes-1, 0);
	    }
	    if (!((num_bytes == -1) && ((errno == EAGAIN)||(errno == EWOULDBLOCK)))) {
	    	std::cout << "Failed to receive msg!" << std::endl;
	    	close(this->sockfd);
	    	return Client::ReceiveFailed;
            }
	    this->recv_buffer[total_bytes] = '\0';
	    printf("%02x%02x", this->recv_buffer[total_bytes-2], this->recv_buffer[total_bytes-1]);
	    return Client::Success;
	}
	//Add a timeout for sending messages and send it
	int Send(std::vector<char> send_buffer, int timeout) {
	    int total_bytes = 0;
	    this->Set_Send_Timeout(timeout);
	    
	    int num_bytes = send(this->sockfd, &(send_buffer[0]), send_buffer.size(), 0);
	    while (num_bytes > 0) {
		total_bytes += num_bytes;
		num_bytes = send(this->sockfd, &(send_buffer[total_bytes]), send_buffer.size()-total_bytes, 0);
	    }
	    if ((num_bytes < 0) && !((errno == EAGAIN)||(errno == EWOULDBLOCK))) {
	    	std::cout << "Failed to send msg!" << std::endl;
	    	close(this->sockfd);
	    	return Client::SendFailed;
            }
	    return Client::Success;
	}
	//Return the received message 
        char* getRecvMsg() {
	    return this->recv_buffer;
	}
	//Remember to close the connection once finished
	//Structures go out of scope so no need for memory cleanup
	void Close() {
	    close(this->sockfd);
	}
};
