#include <cstdio>
#include <cstring>
#include <sys/resource.h>
#include <time.h>
#include "node.h"

//This class is used to add/update/store metrics for use by the server
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

	void Add_Metric(const char* name, char* value) {
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

	int Set_Metric(const char* name, char* value) {
	    Node* current = &(this->metrics);
	    while (current != NULL) {
		if (strcmp(current->field, name) == 0) {
		    this->size -= sizeof(current->value);
		    delete current->value;
		    current->value = NULL;
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

	int Initialize(double start_time) {
    	    struct rusage stats;
    	    struct timeval now;
    	    char data[4096];
    	    memset((void*)data, 0, sizeof(data));
    	    gettimeofday(&now, NULL);
    	    int ret = getrusage(RUSAGE_SELF, &stats);
    	    if (ret == 0) {
		double cpu_time = stats.ru_utime.tv_sec + 0.000001 * stats.ru_utime.tv_usec;
		sprintf(data, "%f", cpu_time);
    		this->Add_Metric("cpu_time_sec", data);
		memset((void*)data, 0, sizeof(data));
		double calendar_time = now.tv_sec + 0.000001 * now.tv_usec - start_time;
		sprintf(data, "%f", calendar_time);
		this->Add_Metric("calendar_time", data);
    	    }
    	    return ret;
	}

	int Update(double start_time) {
    	    struct rusage stats;
    	    struct timeval now;
    	    char data[4096];
    	    memset((void*)data, 0, sizeof(data));
    	    gettimeofday(&now, NULL);
    	    int ret = getrusage(RUSAGE_SELF, &stats);
    	    if (ret == 0) {
	    	double cpu_time = stats.ru_utime.tv_sec + 0.000001 * stats.ru_utime.tv_usec;
		sprintf(data, "%f", cpu_time);
    		this->Set_Metric("cpu_time_sec", data);
		memset((void*)data, 0, sizeof(data));
		double calendar_time = now.tv_sec + 0.000001 * now.tv_usec - start_time;
		sprintf(data, "%f", calendar_time);
		this->Set_Metric("calendar_time", data);
     	    }
    	    return ret;
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

