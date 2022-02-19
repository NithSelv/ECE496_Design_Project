#include <cstdio>
#include <cstring>
#include <vector>
#include <iostream>

//This is a class that is used to prepare the HTTP Response by adding http headers and the body and then setting up the correct format
class Http_Response
   {
private:
   std::vector<char> _msg;
   //int bytes_written; // unused
   
public:
   Http_Response()
      {
      this->_msg.clear();
      }

   void Add_Header_Field(const char* field, const char* value)
      {
      if (!((strlen(field) == strlen("")) && (strcmp(field, "") == 0)))
         {
         this->_msg.insert(this->_msg.end(), field, field+strlen(field));
         this->_msg.push_back(' ');
         this->_msg.push_back(':');
         this->_msg.push_back(' ');
         }
      this->_msg.insert(this->_msg.end(), value, value+strlen(value));
      this->_msg.push_back('\r');
      this->_msg.push_back('\n');
      }

   void Add_Body(std::string body)
      {
      this->_msg.push_back('\r');
      this->_msg.push_back('\n');
      this->_msg.insert(this->_msg.end(), body.begin(), body.end());
      }

   std::vector<char> Prepare_Http_Response()
      {
      return this->_msg;
      }
   };
