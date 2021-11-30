#include <cstdio>
#include <cstring>
#include "node.h"

//This class is used to parse the HTTP Request and query for results
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

