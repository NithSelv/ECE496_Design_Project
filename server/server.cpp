#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

void setup_server_struct(struct sockaddr_in* addr, int port) {
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = htonl(INADDR_ANY);
    memset(addr->sin_zero, 0, 8); 
    return;
}

struct Http_Node {
    char * field;
    char * value;
    Http_Node* next;
};

class Http_Request {
    private:
	Http_Node data;
    public:
	Http_Request() {
	    this->data.field = NULL;
	    this->data.value = NULL;
	    this->data.next = NULL;
	}

	void Parse(char* req) {
	    char* line = strstr(req, "\r\n");
	    int num_chars = (line - req);
	    int increment = 0;

	    this->data.field = (char *)malloc(sizeof(char) * (strlen("Request") + 1));
	    this->data.value = (char *)malloc(sizeof(char) * (num_chars + 1));


	    memset((void*)this->data.field, 0, sizeof(this->data.field));
	    memset((void*)this->data.value, 0, sizeof(this->data.value));

	    strncpy(this->data.field, "Request", strlen("Request"));
	    strncpy(this->data.value, req, num_chars);

	    increment += num_chars + 2;
	    line = strstr(req+increment, "\r\n");
	    num_chars = line - (req+increment);

	    Http_Node* node = &(this->data);
	    node->next = NULL;

	    while (num_chars > 0) {
		node->next = new Http_Node();
		node = node->next;
		node->next = NULL;
		char* field = strstr(req+increment, ": ");
		int num_field_chars = field - (req+increment);
		node->field = (char *)malloc(sizeof(char) * (num_field_chars + 1));
		node->value = (char *)malloc(sizeof(char) * (num_chars - num_field_chars));
		
		memset((void*)node->field, 0, sizeof(node->field));
		memset((void*)node->value, 0, sizeof(node->value));

		strncpy(node->field, req+increment, num_field_chars);
		strncpy(node->value, field+2, num_chars - num_field_chars - 1);

		increment += num_chars + 2;
		line = strstr(req+increment, "\r\n");
		num_chars = line - (req+increment);
	    }
	}

	void Print() {
	    Http_Node* current = &(this->data);
	    printf("Here is the received HTTP Request: \n");
	    while (current != NULL) {
		printf("%s: %s\n", current->field, current->value);
		current = current->next;
	    }
	}

	char * Find(const char* field) {
	    Http_Node* current = &(this->data);
	    while (current != NULL) {
		if (strcmp(current->field, field) == 0) {
		    return (current->value);
		}
		current = current->next;
	    }
	    return NULL;
	}

	~Http_Request() {
	    Http_Node* node = &(this->data);
	    while (node != NULL) {
		delete[] node->field;
		node->field = NULL;
		delete[] node->value;
		node->value = NULL;
		Http_Node* temp = node;
		node = node->next;
		temp->next = NULL;
	    }
	}
};

class Http_Response {
    private:
	Http_Node data;
	Http_Node* last;
	char * send_buffer;
	int buffer_size;
    public:
	Http_Response() {
	    this->data.field = NULL;
	    this->data.value = NULL;
	    this->data.next = NULL;
	    this->last = NULL;
	    this->send_buffer = NULL;
	    this->buffer_size = 0;
	}

	void Add_Field(const char* field, const char* value) {
	    if (this->last == NULL) {
		this->last = &(this->data);
	    } else {
		this->last->next = new Http_Node();
		this->last = this->last->next;
	    }
	    this->last->field = (char *)malloc(sizeof(char) * (strlen(field) + 1));
	    this->last->value = (char *)malloc(sizeof(char) * (strlen(value) + 1));
	    memset((void*)this->last->field, 0, sizeof(this->last->field));
	    memset((void*)this->last->value, 0, sizeof(this->last->value));
	    strcpy(this->last->field, field);
	    strcpy(this->last->value, value);
	    if (strcmp(this->last->field, "Header") == 0) {
		this->buffer_size += strlen(this->last->value) + 2;
	    } else if  (strcmp(this->last->field, "Body") == 0) {
		this->buffer_size += strlen(this->last->value);
	    } else {
		this->buffer_size += strlen(this->last->field) + strlen(this->last->value) + 4;
	    }
	    this->buffer_size += 2;
	}

	void Print() {
	    Http_Node* current = &(this->data);
	    printf("Here is the send HTTP Response: \n");
	    while (current != NULL) {
		printf("%s: %s\n", current->field, current->value);
		current = current->next;
	    }
	}

	char* Prepare_Http_Response() {
	    Http_Node* current = &(this->data);
	    this->send_buffer = (char *)malloc(sizeof(char) * (this->buffer_size + 1));
	    memset((void*)this->send_buffer, 0, sizeof(this->send_buffer));
	    while (current != NULL) {
		if (strcmp(current->field, "Header") == 0) {
		    strcpy(this->send_buffer, current->value);
		    strcat(this->send_buffer, "\r\n");
		} else if (strcmp(current->field, "Body") == 0) {
		    strcat(this->send_buffer, current->value);
		} else {
		    strcat(this->send_buffer, current->field);
		    strcat(this->send_buffer, ": ");
		    strcat(this->send_buffer, current->value);
		    strcat(this->send_buffer, "\r\n");
		}
		if ((current->next != NULL) && (strcmp(current->next->field, "Body") == 0)) {
		    strcat(this->send_buffer, "\r\n");
		}
		current = current->next;
	    }
	    return (this->send_buffer);
	}

	

	~Http_Response() {
	    this->last = NULL;
	    delete[] this->send_buffer;
	    this->send_buffer = NULL;
	    this->buffer_size = 0;
	    Http_Node* node = &(this->data);
	    while (node != NULL) {
		delete[] node->field;
		node->field = NULL;
		delete[] node->value;
		node->value = NULL;
		Http_Node* temp = node;
		node = node->next;
		temp->next = NULL;
	    }
	    this->buffer_size = 0;
	}
};

int main(int argc, char* argv[]){
    
    int port, num_connections, sockfd, newfd, bound, connecting, num_bytes, total_bytes;
    //char header[4096] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain; version=0.0.4; charset=utf-8\r\n\r\n";
    //char body[4096] = "# HELP random_metric1 Totally random value p1.\n# TYPE random_metric1 counter\nrandom_metric1 100.32\n";
    char receive_buffer[4096];
    char send_buffer[4096];
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

    connecting = listen(sockfd, num_connections);

    if (connecting < 0) {
	printf("FAILED: Listen failed!");
	close(sockfd);
	return -1;
    }


    while(1) {
	struct sockaddr_storage client_addr;
    	socklen_t client_addr_size;

    	newfd = accept(sockfd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_size);

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

	Http_Request req;
	req.Parse(receive_buffer);
	req.Print();

	//memset((void*)send_buffer, 0, sizeof(send_buffer));
	
	//strcpy(send_buffer, header);
	//strcat(send_buffer, body);
	Http_Response rep;
	rep.Add_Field("Header", "HTTP/1.1 200 OK");
	rep.Add_Field("Content-Type", "text/plain; version=0.0.4; charset=utf-8");
	rep.Add_Field("Body", "# HELP random_metric1 Totally random value p1.\n# TYPE random_metric1 counter\nrandom_metric1 100.32\n");
	char* temp = rep.Prepare_Http_Response();
	rep.Print();

	memset((void*)send_buffer, 0, sizeof(send_buffer));
	strcpy(send_buffer, temp);

	total_bytes = 0;
	while (total_bytes != (int)strlen(send_buffer)) {
		num_bytes = send(newfd, send_buffer+total_bytes, strlen(send_buffer) - total_bytes, 0);

		if (num_bytes < 0) {
			printf("FAILED: Send failed!");
			close(newfd);
			close(sockfd);
			return -1;
		}
		total_bytes += num_bytes;
	}
	printf("One connection handled!\n");
	close(newfd);
	
    }

    close(sockfd);		
    return 0;
}

