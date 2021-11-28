#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/resource.h>
#include <sstream>
#include <iostream>
#include <time.h>

using namespace std;

void setup_server_struct(struct sockaddr_in* addr, int port) {
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = htonl(INADDR_ANY);
    memset(addr->sin_zero, 0, 8); 
    return;
}

struct Node {
    char * field;
    char * value;
    Node* next;
};

class Metrics_Database {
    private:
	Node metrics;
	Node* last;
	int size;
    public:
	Metrics_Database() {
	    this->metrics.field = NULL;
	    this->metrics.value = NULL;
	    this->metrics.next = NULL;
	    this->last = NULL;
	    this->size = 0;
	}

	void Add_Metric(char * name, char* value) {
	    if (this->last == NULL) {
		this->last = &(this->metrics);
	    } else {
		this->last->next = new Node();
		this->last = this->last->next;
	    }
	    this->last->field = (char *)malloc(sizeof(char) * (strlen(name) + 1));
	    this->last->value = (char *)malloc(sizeof(char) * (strlen(value) + 1));
	    memset((void*)this->last->field, 0, sizeof(this->last->field));
	    memset((void*)this->last->value, 0, sizeof(this->last->value));
	    strcpy(this->last->field, name);
	    strcpy(this->last->value, value);
	    this->size += strlen(name) + strlen(value) + 2;
	}

	char* Find_Metric(char* name) {
	    char* temp = NULL;
	    Node* current = &(this->metrics);
	    while (current != NULL) {
		if (strcmp(current->field, name) == 0) {
		    temp = (char *)malloc(sizeof(current->value));
		    strcpy(temp, current->value);
		    return (temp);
		}
		current = current->next;
	    }
	    return NULL;
	}

	char** Find_Metrics(char* names[], int size) {
	    int i = 0;
	    char** metrics_list = new char*[size];
	    for (i = 0; i < size; i++) {
		metrics_list[i] = this->Find_Metric(names[i]);
	    }
	    return metrics_list;
	}

	int Set_Metric(char* name, char* value) {
	    Node* current = &(this->metrics);
	    while (current != NULL) {
		if (strcmp(current->field, name) == 0) {
		    this->size -= sizeof(current->value);
		    delete[] current->value;
		    current->value = (char *)malloc(sizeof(char) * (strlen(value) + 1));
		    memset((void*)current->value, 0, sizeof(current->value));
		    strcpy(current->value, value);
		    this->size += sizeof(current->value);
		    return 0;
		}
		current = current->next;
	    }
	    return -1;
	}

	int* Set_Metrics(char* names[], char* values[], int size) {
	    int i = 0;
	    int* metrics_list = new int[size];
	    for (i = 0; i < size; i++) {
		metrics_list[i] = this->Set_Metric(names[i], values[i]);
	    }
	    return metrics_list;
	}

	char* Prepare_Metric_Body(char* name) {
	    char * value = Find_Metric(name);
	    if (value == NULL) {
		return NULL;
	    } else {
		char* msg = (char*)malloc(sizeof(char) * (strlen(name) + strlen(value) + 2));
		memset((void*)msg, 0, sizeof(msg));
		strcpy(msg, name);
		strcat(msg, " ");
		strcat(msg, value);
		return msg;
	    }
	}

	char* Prepare_Metrics_Body(char* names[], int size) {
	    int i = 0;
	    char* msg = (char*)malloc(sizeof(char) * this->size);
	    memset((void*)msg, 0, sizeof(msg));
	    for (i = 0; i < size; i++) {
		char* temp = this->Find_Metric(names[i]);
		if (temp == NULL) {
		    delete[] msg;
		    msg = NULL;
		    return NULL;
		} else {
		    if (i != 0) {
			strcat(msg, "\n");
		    }
		    if (i == 0) {
			strcpy(msg, names[i]);
		    } else {
		        strcat(msg, names[i]);
		    }
		    strcat(msg, " ");
		    strcat(msg, temp);
		}
	    }
	    return msg;
	}

	char* Prepare_All_Metrics_Body() {
	    Node* current = &(this->metrics);
	    char* msg = (char*)malloc(sizeof(char) * this->size);
	    memset((void*)msg, 0, sizeof(msg));
	    while (current != NULL) {
		if (current != (&this->metrics)) {
		    strcat(msg, "\n");
		}
		if (current == (&this->metrics)) {
		    strcpy(msg, current->field);
		} else {
		    strcat(msg, current->field);
		}
		strcat(msg, " ");
		strcat(msg, current->value);
		current = current->next;
	    }
	    return msg;
	}

	void Print() {
	    char* msg = this->Prepare_All_Metrics_Body();
	    printf("Metrics Database: \n");
	    printf("%s\n", msg);
	    delete[] msg;
	}

	
	~Metrics_Database() {
	    this->last = NULL;
	    Node* node = &(this->metrics);
	    while (node != NULL) {
		delete[] node->field;
		node->field = NULL;
		delete[] node->value;
		node->value = NULL;
		Node* temp = node;
		node = node->next;
		temp->next = NULL;
	    }
	}

};

int initialize_database(Metrics_Database* db, double start_time) {
    struct rusage stats;
    struct timeval now;
    char* data = new char[4096];
    memset((void*)data, 0, sizeof(data));
    gettimeofday(&now, NULL);
    int ret = getrusage(RUSAGE_SELF, &stats);
    if (ret == 0) {
	double cpu_time = stats.ru_utime.tv_sec + 0.000001 * stats.ru_utime.tv_usec;
	sprintf(data, "%f", cpu_time);
    	db->Add_Metric("User CPU Time (Seconds)", data);
	memset((void*)data, 0, sizeof(data));
	double calendar_time = now.tv_sec + 0.000001 * now.tv_usec - start_time;
	sprintf(data, "%f", calendar_time);
	db->Add_Metric("Calendar Time (Seconds)", data);
    }
    delete[] data;
    return ret;
}

int populate_database(Metrics_Database* db, double start_time) {
    struct rusage stats;
    struct timeval now;
    char* data = new char[4096];
    memset((void*)data, 0, sizeof(data));
    gettimeofday(&now, NULL);
    int ret = getrusage(RUSAGE_SELF, &stats);
    if (ret == 0) {
	double cpu_time = stats.ru_utime.tv_sec + 0.000001 * stats.ru_utime.tv_usec;
	sprintf(data, "%f", cpu_time);
    	db->Set_Metric("User CPU Time (Seconds)", data);
	memset((void*)data, 0, sizeof(data));
	double calendar_time = now.tv_sec + 0.000001 * now.tv_usec - start_time;
	sprintf(data, "%f", calendar_time);
	db->Set_Metric("Calendar Time (Seconds)", data);
    }
    delete[] data;
    return ret;
}

class Http_Request {
    private:
	Node data;
    public:
	Http_Request() {
	    this->data.field = NULL;
	    this->data.value = NULL;
	    this->data.next = NULL;
	}

	void Parse(char* req) {
	    char* line = strstr(req, "\r\n");
	    char* last_line = strstr(req, "\r\n\r\n");
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

	    Node* node = &(this->data);
	    node->next = NULL;

	    while (line != last_line) {
		num_chars = line - (req+increment);
		node->next = new Node();
		node = node->next;
		node->next = NULL;
		node->field = NULL;
		node->value = NULL;
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
	    }
	}

	void Print() {
	    Node* current = &(this->data);
	    printf("Here is the received HTTP Request: \n");
	    while (current != NULL) {
		printf("%s: %s\n", current->field, current->value);
		current = current->next;
	    }
	}

	char * Find(const char* field) {
	    Node* current = &(this->data);
	    while (current != NULL) {
		if (strcmp(current->field, field) == 0) {
		    return (current->value);
		}
		current = current->next;
	    }
	    return NULL;
	}

	~Http_Request() {
	    delete[] this->data.field;
	    this->data.field = NULL;
	    delete[] this->data.value;
	    this->data.value = NULL;
	    Node* node = this->data.next;
	    while (node != NULL) {
		delete[] node->field;
		node->field = NULL;
		delete[] node->value;
		node->value = NULL;
		Node* temp = node;
		node = node->next;
		delete temp;
	    }
	}
};

class Http_Response {
    private:
	Node data;
	Node* last;
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
		this->last->next = new Node();
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
	    Node* current = &(this->data);
	    printf("Here is the sent HTTP Response: \n");
	    while (current != NULL) {
		printf("%s: %s\n", current->field, current->value);
		current = current->next;
	    }
	}

	char* Prepare_Http_Response() {
	    Node* current = &(this->data);
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
	    delete[] this->data.field;
	    this->data.field = NULL;
	    delete[] this->data.value;
	    this->data.value = NULL;
	    this->send_buffer = NULL;
	    this->buffer_size = 0;
	    Node* node = this->data.next;
	    while (node != NULL) {
		delete[] node->field;
		node->field = NULL;
		delete[] node->value;
		node->value = NULL;
		Node* temp = node;
		node = node->next;
		delete temp;
	    }
	    this->buffer_size = 0;
	}
};

int main(int argc, char* argv[]){
    
    int port, num_connections, sockfd, newfd, bound, connecting, num_bytes, total_bytes, timeout, db_check;
    double start_time = 0;
    char receive_buffer[4096];
    char send_buffer[4096];
    struct sockaddr_in server_addr;
    struct timeval start;
    gettimeofday(&start, NULL);
    start_time = start.tv_sec + start.tv_usec * 0.000001;
    Metrics_Database* db = new Metrics_Database();

    db_check = initialize_database(db, start_time);
    if (db_check != 0) {
	printf("FAILED: Unable to initialize database!\n");
        return -1;
    }

    if (argc != 4) {
        printf("FAILED: Not enough arguments\n");
        return -1;
    } else {
        port = atoi(argv[1]);
	num_connections = atoi(argv[2]);
	timeout = atoi(argv[3]);
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

	struct timeval timer;
	timer.tv_sec = 0;
	timer.tv_usec = timeout;
	int time_set = setsockopt(newfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timer, sizeof(timer));

        if (time_set < 0) {
        	printf("FAILED: Could not set timeout!\n");
		close(newfd);
		close(sockfd);
		return -1;
	}

	memset((void*)receive_buffer, 0, sizeof(receive_buffer));
	num_bytes = recv(newfd, receive_buffer, sizeof(receive_buffer), 0);

	if ((num_bytes == -1) && ((errno == EAGAIN)||(errno == EWOULDBLOCK))) {
		printf("TIMEDOUT: Server timeout waiting for packet. Connection closed.\n");
		close(newfd);
		continue;
	} else if (num_bytes < 0) {
		printf("FAILED: Receive failed!");
		close(newfd);
		close(sockfd);
		return -1;
	}

	Http_Response rep;
	rep.Add_Field("Header", "HTTP/1.1 200 OK");
	rep.Add_Field("Content-Type", "text/plain; version=0.0.4; charset=utf-8");
	//rep.Add_Field("Body", "# HELP random_metric1 Totally random value p1.\n# TYPE random_metric1 counter\nrandom_metric1 100.32\n");
	rep.Add_Field("Body", db->Prepare_All_Metrics_Body());
	char* temp = rep.Prepare_Http_Response();
	rep.Print();

	Http_Request req;
	req.Parse(receive_buffer);
	//char* client = req.Find("User-Agent");
	//if ((client == NULL) || (strncmp(client, "Prometheus", 10) != 0)) {
		//printf("Unknown client attempted to connect!\n");
		//req.Print();	
		//close(newfd);
		//continue;
	//}

	char* request = req.Find("Request");

	if (request == NULL) {
		printf("Invalid Request!\n");		
		close(newfd);
		continue;
	}
	req.Print();

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

	db_check = populate_database(db, start_time);
	if (db_check < 0) {
	    printf("FAILED: database did not update!");
	    close(newfd);
	    close(sockfd);
	    return -1;
	}
	printf("One connection handled!\n");
	close(newfd);
	
    }

    close(sockfd);		
    return 0;
}

