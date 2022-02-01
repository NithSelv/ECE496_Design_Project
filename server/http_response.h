#include <cstdio>
#include <cstring>
#include <vector>
#include <iostream>

//This is a class that is used to prepare the HTTP Response by adding http headers and the body and then setting up the correct format
class Http_Response {
    private:
	std::vector<char> msg;
	int bytes_written;
    public:
	Http_Response() {
	    this->msg.clear(); 
	}

	void Add_Header_Field(const char* field, const char* value) {
	    if (!((strlen(field) == strlen("")) && (strcmp(field, "") == 0))) {
		this->msg.insert(this->msg.end(), field, field+strlen(field));
		this->msg.push_back(' ');
		this->msg.push_back(':');
		this->msg.push_back(' ');	    	
	    }
	    this->msg.insert(this->msg.end(), value, value+strlen(value));
	    this->msg.push_back('\r');
	    this->msg.push_back('\n');
	}

	void Add_Body(std::string body) {
	    this->msg.push_back('\r');
	    this->msg.push_back('\n');
	    this->msg.insert(this->msg.end(), body.begin(), body.end());
	}

	std::vector<char> Prepare_Http_Response() {
	    return this->msg;
	}
};

