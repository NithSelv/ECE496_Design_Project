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

// This class is used to handle the server communications
class Server
   {
private:
   struct sockaddr_in server_addr;
   int _port;
   int _numConnections;
   int _sockfd;

public:
   // These are the enums for all the return codes that our server can return
   enum Return_Codes {Success = 0, InvalidPort = -1, InvalidNumConnections = -2, SocketCreationFailed = -3, BindingFailed = -4, ListeningFailed = -5};
   // Initialize the server values except for the sockfd
   // This works for IPV4, may need to change it to adjust for IPV6
   Server(int port, int numConnections)
      {
      this->_port = port;
      this->_numConnections = numConnections;
      this->_sockfd = -1;
      this->server_addr.sin_port = htons(port);
      this->server_addr.sin_family = AF_INET;
      this->server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
      memset(server_addr.sin_zero, 0, 8);
      }
   // Check to make sure the input is valid and then begin to create the socket, bind it, and set it to listen
   int Start()
      {
      // Make sure we are listening on a valid port number
      if ((this->_port < 1024) || (this->_port > 65535))
         {
         std::cout << "Invalid port number!" << std::endl;
         return Server::InvalidPort;
         }
      if ((this->_numConnections < 1) || (this->_numConnections > 5))
         {
         std::cout << "Invalid number of connections!" << std::endl;
         return Server::InvalidNumConnections;
         }
      // Create the socket
      this->_sockfd = socket(AF_INET, SOCK_STREAM, 0);
      if (this->_sockfd < 0)
         {
         std::cout << "Socket Creation Failed!" << std::endl;
         return Server::SocketCreationFailed;
         }
      // Bind it to the server
      if (bind(this->_sockfd, (struct sockaddr *)&(this->server_addr), (socklen_t)sizeof(this->server_addr)) != 0)
         {
         std::cout << "Socket Binding Failed!" << std::endl;
         close(this->_sockfd);
         return Server::BindingFailed;
         }
      // Start listening
      if (listen(this->_sockfd, this->_numConnections) < 0)
         {
         std::cout << "Socket Failed to Listen!" << std::endl;
         close(this->_sockfd);
         return Server::ListeningFailed;
         }
      return Server::Success;
      }
   // Get the sockfd
   int getSockfd()
      {
      return this->_sockfd;
      }
   };
