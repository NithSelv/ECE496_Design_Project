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

// This class is used to handle the client communications
class Client
   {
// These are private variables and functions used to manipulate send and recv buffers
private:
   int _sockfd;
   struct sockaddr_storage _clientAddr;
   struct timeval _recvTimer;
   struct timeval _sendTimer;
   socklen_t _clientAddrSize;
   char _recvBuffer[4096];

   // This private function allows us to set the timeout for receiving messages.
   int Set_Recv_Timeout(int timeout)
      {
      this->_recvTimer.tv_sec = timeout;
      if (setsockopt(this->_sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&(this->_recvTimer), sizeof(this->_recvTimer)) < 0)
         {
         std::cout << "Failed to set timeout!" << std::endl;
         close(this->_sockfd);
         return Client::TimeoutFailed;
         }
      return Client::Success;
      }

   // This private function allows us to set the timeout for sending messages.
   int Set_Send_Timeout(int timeout)
      {
      this->_sendTimer.tv_sec = timeout;
      if (setsockopt(this->_sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&(this->_sendTimer), sizeof(this->_sendTimer)) < 0)
         {
         std::cout << "Failed to set timeout!" << std::endl;
         close(this->_sockfd);
         return Client::TimeoutFailed;
         }
      return Client::Success;
      }

public:
   // Some enums for error codes
   enum Return_Codes {Success = 0, AcceptFailed = -1, TimeoutFailed = -2, ReceiveFailed = -3, SendFailed = -4};
   // Set the initial sockfd to some invalid value
   Client()
      {
      this->_sockfd = -1;
      this->_recvBuffer[0] = '\0';
      }
   // Accept the connection and populate the client structures
   int Accept(int server_sock)
      {
      this->_sockfd = accept(server_sock, (struct sockaddr *)&(this->_clientAddr), (socklen_t *)&(this->_clientAddrSize));
      if (this->_sockfd < 0)
         {
         std::cout << "Failed to connect to client!" << std::endl;
         return Client::AcceptFailed;
         }
      return Client::Success;
      }
   // Add a timeout for receving messages and store the received message in the buffer
   int Receive(int timeout)
      {
      int total_bytes = 0;
      this->Set_Recv_Timeout(timeout);
      int num_bytes = recv(this->_sockfd, this->_recvBuffer, sizeof(this->_recvBuffer)-1, 0);
      while (num_bytes > 0)
         {
         total_bytes += num_bytes;
         if (total_bytes >= 4095)
            {
            num_bytes = -1;
            continue;
            }
         num_bytes = recv(this->_sockfd, &(this->_recvBuffer[total_bytes]), sizeof(this->_recvBuffer)-total_bytes-1, 0);
         }
      if (!((num_bytes == -1) && ((errno == EAGAIN)||(errno == EWOULDBLOCK))))
         {
         std::cout << "Failed to receive msg!" << std::endl;
         close(this->_sockfd);
         return Client::ReceiveFailed;
         }
      this->_recvBuffer[total_bytes] = '\0';
      return Client::Success;
      }
   // Add a timeout for sending messages and send it
   int Send(std::vector<char> send_buffer, int timeout)
      {
      int total_bytes = 0;
      this->Set_Send_Timeout(timeout);

      int num_bytes = send(this->_sockfd, &(send_buffer[0]), send_buffer.size(), 0);
      while (num_bytes > 0)
         {
         total_bytes += num_bytes;
         num_bytes = send(this->_sockfd, &(send_buffer[total_bytes]), send_buffer.size()-total_bytes, 0);
         }
      if ((num_bytes < 0) && !((errno == EAGAIN)||(errno == EWOULDBLOCK)))
         {
         std::cout << "Failed to send msg!" << std::endl;
         close(this->_sockfd);
         return Client::SendFailed;
         }
      return Client::Success;
      }
   // Return the received message 
   char* getRecvMsg() {
   return this->_recvBuffer;
   }
   // Remember to close the connection once finished
   // Structures go out of scope so no need for memory cleanup
   void Close()
      {
      close(this->_sockfd);
      }
   };
