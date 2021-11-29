#include <cstdio>
#include <cstring>
#include "node.h"

//This is a class that is used to prepare the HTTP Response by adding http headers and the body and then setting up the correct format
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
		this->last->next = NULL;
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

