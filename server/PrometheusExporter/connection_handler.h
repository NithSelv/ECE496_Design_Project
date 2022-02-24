#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <sys/poll.h>
#include "client.h"

class Connection_Handler {
	private:
		std::vector<pollfd> fds;
		std::vector<Client> clients;
		int num_fds;
		int nfds;
		int listener;
		int timeout;
		
	public:
		Connection_Handler(int num_fds, int sockfd, int timeout) {
			this->num_fds = num_fds;
			this->nfds = 1;
			this->listener = sockfd;
			this->timeout = timeout;
			struct pollfd p;
			p.fd = sockfd;
			p.events = POLLIN;
			p.revents = 0;
			this->fds.push_back(p);
		}
		
		void Handle_Connections() {
			int success = poll(&(this->fds[0]), this->nfds, -1);
			//Poll failed
			if (success < 0) {
				std::cout << "Error: Poll failed!" << std::endl;
				return;
			}
			int size = this->nfds;
			for (int i = 0; i < size; i++) {
				//No event
				if (this->fds[i].revents <= 0) {
					continue;
				}
				//We can read from the sockets
				if ((this->fds[i].revents & POLLIN) == POLLIN) {
					//Incoming connection
					if (this->fds[i].fd == this->listener) {
						//Accept connection if there is enough space
						if (this->fds.size() < this->num_fds) {
							Client client;
							client.clientAccept(this->sockfd);
							this->clients.push_back(client);
							//Add to fds if there is enough space
							struct pollfd p;
							p.fd = client.getSockfd();
							p.events = POLLIN;
							p.revents = 0;
							this->fds.push_back(p);
						} else {
							std::cout << "Not enough space to handle connection: Connection dropped!" << std::endl;
						}
					//Either new data or a request to close connection	
					} else {
						int check = this->clients[i-1].clientReceive(this->timeout);
						// Receive failed, so this connection has been killed. Mark it for removal.
						if (check < 0) {
							this->fds[i].fd = -1;
						// Receive passed, handle the http request and kill the connection if keep alive is not specified
						} else {
							receivers.push_back(this->clients[i-1]);
						}

					}
				}
			}
		}
};
