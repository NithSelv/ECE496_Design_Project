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
	    msg = NULL;
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
    	db->Add_Metric("CPU_Time_Sec", data);
	memset((void*)data, 0, sizeof(data));
	double calendar_time = now.tv_sec + 0.000001 * now.tv_usec - start_time;
	sprintf(data, "%f", calendar_time);
	db->Add_Metric("Calendar_Time", data);
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
    	db->Set_Metric("cpu_time_sec", data);
	memset((void*)data, 0, sizeof(data));
	double calendar_time = now.tv_sec + 0.000001 * now.tv_usec - start_time;
	sprintf(data, "%f", calendar_time);
	db->Set_Metric("calendar_time", data);
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
	    int num_chars = strcspn(req, "\r");
	    char* starter = req;
	    char* last_line = strstr(req, "\r\n\r\n");

	    this->data.field = (char *)malloc(sizeof(char) * (strlen("Request") + 1));
	    this->data.value = (char *)malloc(sizeof(char) * (num_chars + 1));


	    memset((void*)this->data.field, 0, sizeof(this->data.field));
	    memset((void*)this->data.value, 0, sizeof(this->data.value));

	    strncpy(this->data.field, "Request", strlen("Request"));
	    strncpy(this->data.value, req, num_chars);

	    Node* node = &(this->data);

	    char* arg = strtok(this->data.value, " ");
	    int i = 0;
	    while (arg != NULL) {
		if (i == 3) {
		    break;
		}
		node->next = new Node();
		node = node->next;
		node->field = NULL;
	    	node->value = NULL;
		node->next = NULL;
		if (i == 0) {
		    
		    node->field = (char *)malloc(sizeof(char) * (strlen("Type") + 1));
		    strcpy(node->field, "Type");
		} else if (i == 1) {
	 	    node->field = (char *)malloc(sizeof(char) * (strlen("Metric") + 1));
		    strcpy(node->field, "Metric");
		} else if (i == 2) {
		    node->field = (char *)malloc(sizeof(char) * (strlen("Version") + 1));
		    strcpy(node->field, "Version");
		}
	    	node->value = (char *)malloc(sizeof(char) * (strlen(arg) + 1));
		strcpy(node->value, arg);
		arg = strtok(NULL, " ");
		i = i + 1;
	    }

	    starter += (num_chars + 2);

	    while (starter != (last_line+2)) {
		num_chars = strcspn(starter, "\r");
		node->next = new Node();
		node = node->next;
		node->next = NULL;
		node->field = NULL;
		node->value = NULL;
		int num_field_chars = strcspn(starter, ":");
		node->field = (char *)malloc(sizeof(char) * (num_field_chars + 1));
		node->value = (char *)malloc(sizeof(char) * (num_chars - num_field_chars - 1));
		
		memset((void*)node->field, 0, sizeof(node->field));
		memset((void*)node->value, 0, sizeof(node->value));

		strncpy(node->field, starter, num_field_chars);
		strncpy(node->value, starter+num_field_chars+2, num_chars - num_field_chars - 2);
		node->field[num_field_chars] = '\0';
		node->value[num_chars - num_field_chars - 2] = '\0';
		
		starter += (num_chars + 2);
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

	char * Find(char* field) {
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
	    delete this->data.field;
	    this->data.field = NULL;
	    delete this->data.value;
	    this->data.value = NULL;
	    Node* node = this->data.next;
	    while (node != NULL) {
		delete node->field;
		node->field = NULL;
		delete node->value;
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
	    int ending = 0;
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
		if (((current->next != NULL) && (strcmp(current->next->field, "Body") == 0)) || ((current->next == NULL) && (ending == 0))) {
		    strcat(this->send_buffer, "\r\n");
		    ending = 1;
		}
		current = current->next;
	    }
	    return (this->send_buffer);
	}

	

	~Http_Response() {
	    this->last = NULL;
	    delete[] this->send_buffer;
	    delete this->data.field;
	    this->data.field = NULL;
	    delete this->data.value;
	    this->data.value = NULL;
	    this->send_buffer = NULL;
	    this->buffer_size = 0;
	    Node* node = this->data.next;
	    while (node != NULL) {
		delete node->field;
		node->field = NULL;
		delete node->value;
		node->value = NULL;
		Node* temp = node;
		node = node->next;
		delete temp;
	    }
	    this->buffer_size = 0;
	}
};

char* http_error_check(Http_Request* req, Http_Response* rep, Metrics_Database* db, char* port_num, char* body) {
    char* client = (*req).Find("User-Agent");
    if ((client == NULL) || (strncmp(client, "Prometheus", 10) != 0)) {
        printf("Unauthorized Client!\n");
	(*rep).Add_Field("Header", "HTTP/1.1 401 Unauthorized");	
	return (*rep).Prepare_Http_Response();
    } 

    char* type = (*req).Find("Type");
    char* metric = (*req).Find("Metric");
    char* ip = (*req).Find("Host");

    if (ip == NULL) {
	printf("Invalid Request!\n");
	(*rep).Add_Field("Header", "HTTP/1.1 400 Bad Request");
	return (*rep).Prepare_Http_Response();	
    }

    char* port_s = strtok(ip, ":");
    char* host = port_s;
    port_s = strtok(NULL, ":");

    if ((type == NULL) || (metric == NULL) || (host == NULL) || (port_s == NULL)) {
	printf("Invalid Request!\n");		
	(*rep).Add_Field("Header", "HTTP/1.1 400 Bad Request");
	return (*rep).Prepare_Http_Response();
    } 

    if (strncmp(type, "GET", 3) != 0){
	printf("Invalid Request!\n");		
	(*rep).Add_Field("Header", "HTTP/1.1 400 Bad Request");
	return (*rep).Prepare_Http_Response();
    }

    if (strncmp(metric, "/metrics", 8) != 0) {
	printf("Not Found!\n");		
	(*rep).Add_Field("Header", "HTTP/1.1 404 Not Found");
	return (*rep).Prepare_Http_Response();
    }

    if ((strncmp(host, "localhost", 9) != 0) && (strncmp(port_s, port_num, strlen(port_num)) != 0)) {
	printf("Wrong server or port!\n");		
	(*rep).Add_Field("Header", "HTTP/1.1 303 See Other");
	return (*rep).Prepare_Http_Response();
    }

    printf("Valid Request Received!\n");
    (*rep).Add_Field("Header", "HTTP/1.1 200 OK");
    (*rep).Add_Field("Content-Type", "text/plain; version=0.0.4; charset=utf-8");
    body = db->Prepare_All_Metrics_Body();
    (*rep).Add_Field("Body", body);
    return (*rep).Prepare_Http_Response();
}

int main(int argc, char* argv[]){
    
    int port, num_connections, sockfd, newfd, bound, connecting, timeout, db_check, sock_check;
    char port_num[100] = "";
    double start_time = 0;
    char receive_buffer[4096];
    char send_buffer[4096];
    struct sockaddr_in server_addr;
    struct timeval start;
    gettimeofday(&start, NULL);
    start_time = start.tv_sec + start.tv_usec * 0.000001;
    Metrics_Database db;

    db_check = initialize_database(&db, start_time);
    if (db_check != 0) {
	printf("FAILED: Unable to initialize database!\n");
        return -1;
    }

    if (argc != 4) {
        printf("FAILED: Not enough arguments\n");
        return -1;
    } else {
	strcpy(port_num, argv[1]);
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
	timer.tv_sec = timeout;
	int time_set = setsockopt(newfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timer, sizeof(timer));

        if (time_set < 0) {
        	printf("FAILED: Could not set timeout!\n");
		close(newfd);
		close(sockfd);
		return -1;
	}

	int num_bytes = 0;
    	memset((void*)receive_buffer, 0, sizeof(receive_buffer));
    	num_bytes = recv(newfd, receive_buffer, sizeof(receive_buffer), 0);
    	if ((num_bytes == -1) && ((errno == EAGAIN)||(errno == EWOULDBLOCK))) {
	    close(newfd);
	    continue;
    	} else if (num_bytes < 0) {
	    printf("FAILED: Receive failed!");
	    close(newfd);
	    close(sockfd);
	    return -1;
        }

	Http_Request req;
	Http_Response rep;

	req.Parse(receive_buffer);

	char* body;

	char * http_rep = http_error_check(&req, &rep, &db, port_num, body);

	req.Print();
	printf("\n");
	rep.Print();

	int total_bytes = 0;
    	num_bytes = 0;
    	memset((void*)send_buffer, 0, sizeof(send_buffer));
    	strcpy(send_buffer, http_rep);

	printf("%s\n", send_buffer);

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

	delete body;
	body = NULL;
	http_rep = NULL;

	db_check = populate_database(&db, start_time);
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

