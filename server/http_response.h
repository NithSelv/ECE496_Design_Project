#include <cstdio>
#include <cstring>
#include <iostream>

//This is a class that is used to prepare the HTTP Response by adding http headers and the body and then setting up the correct format
class Http_Response {
    private:
	char data[4096];
	int bytes_written;
    public:
	//These are the enums for all the error codes that this class' methods can return
	enum Return_Codes {Success = 0, HeaderInvalid = -1, BodyInvalid = -2};
	Http_Response() {
	    memset(this->data, 0, sizeof(this->data));
	    this->bytes_written = 0;
	}

	int Add_Header_Field(const char* field, const char* value) {
	    
	    if ((sizeof(this->data)-bytes_written) < strlen(field)+strlen(value)+6) {
		std::cout << "Failed: There is not enough buffer space to process this response!" << std::endl;
		return Http_Response::HeaderInvalid;
	    }
	    strcat(this->data, field);
	    strcat(this->data, " : ");
	    strcat(this->data, value);
	    strcat(this->data, "\r\n");
	    this->bytes_written += strlen(field)+strlen(value)+5;
	    return Http_Response::Success;
	}

	int Add_Body(char* body) {
	    if ((sizeof(this->data)-bytes_written) < strlen(body)+3) {
		std::cout << "Failed: There is not enough buffer space to process this response!" << std::endl;
		return Http_Response::BodyInvalid;
	    }
	    strcat(this->data, "\r\n");
	    strcat(this->data, body);
	    this->bytes_written += strlen(body)+2;
	    return Http_Response::Success;
	}

	std::string Prepare_Http_Response() {
	    std::string str(this->data);
	    return str;
	}
};

