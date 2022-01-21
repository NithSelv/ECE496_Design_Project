#include <cstdio>
#include <cstring>
#include <algorithm>

//This class is used to parse the HTTP Request and query for results
class Http_Request {
    //These are the fields that we are interested in
    //We do not store the values for all fields
    private:
	int Type;
	char* Metric;
	char* Connection;
    public:
	//Enums for the possible types of Http Requests
	enum Http_Types {GET = 0, HEAD = 1, POST = 2, PUT = 3, DELETE = 4, PATCH = 5, NA = -1};
	//Initialize all the fields to NULL
	Http_Request() {
	    this->Type = Http_Request::Http_Types::NA;
	}
	//Take a string literal as the http request and get the desired fields
	void Parse(char* req) { //removed const
	    int num_chars = strcspn(req, "\r");
	    char* starter = req;
	    char* last_line = strstr(req, "\r\n\r\n");
	    char first_header[64];
	    char type[16];
	    memset((void*)first_header, 0, sizeof(first_header));
	    memset((void*)type, 0, sizeof(type));
	    strncpy(first_header, req, std::min(num_chars, (int)(sizeof(first_header)-1)));
	    char* arg = strtok(first_header, " ");
	    int i = 0;
	    while ((arg != NULL) && (i < 3)) {
		if (i == 0) { //cant use switch statement with char
			if (arg == "GET")
			    this->Type = Http_Request::Http_Types::GET;
			else if (arg == "HEAD")
			    this->Type = Http_Request::Http_Types::HEAD;
		    	else if (arg == "POST")
			    this->Type = Http_Request::Http_Types::POST;
			else if (arg == "PUT")
			    this->Type = Http_Request::Http_Types::PUT;
			else if (arg == "DELETE")
			    this->Type = Http_Request::Http_Types::DELETE;
			else if (arg == "PATCH")
			    this->Type = Http_Request::Http_Types::PATCH;
			else
			    this->Type = Http_Request::Http_Types::NA;
	    	}
	    	else if (i == 1) {
	 	    strncpy(this->Metric, arg, std::min(strlen(arg)+1, sizeof(this->Metric)-1));
		}
		arg = strtok(NULL, " ");
		i++;
	    	}

	    starter += (num_chars + 2);

	    while (starter < (last_line+2)) {
		num_chars = strcspn(starter, "\r");
		int num_field_chars = strcspn(starter, ":");
		char field[16];
		memset((void*)field, 0, sizeof(field));
		strncpy(field, starter, std::min(num_field_chars, (int)sizeof(field)-1));
		field[std::min(num_field_chars, (int)(sizeof(field)-1))] = '\0';
		if ((strlen(field) == strlen("Connection")) && (strcmp(field, "Connection") == 0)) {
		    strncpy(this->Connection, starter+num_field_chars+2, std::min(num_chars-num_field_chars-2, (int)sizeof(this->Connection)-1));
		    this->Connection[std::min(strlen(field), sizeof(this->Connection)-1)] = '\0';
		    break;
		}
		starter += (num_chars + 2);
	    }
	}
	
	
	//These functions are used to access the stored fields
	int Find_Type() {
	    return this->Type;
	}
	char * Find_Metric() {
	    return this->Metric;
	}
	char * Find_Connection() {
	    return this->Connection;
	}
};

