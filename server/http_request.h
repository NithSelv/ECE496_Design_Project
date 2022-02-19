#include <cstdio>
#include <cstring>
#include <algorithm>

// This class is used to parse the HTTP Request and query for results
class Http_Request
   {
// These are the fields that we are interested in
// We do not store the values for all fields
private:
   int _type;
   char _metric[16];
   char _connection[16];

public:
   // Enums for the possible types of Http Requests
   enum Http_Types {GET = 0, HEAD = 1, POST = 2, PUT = 3, DELETE = 4, PATCH = 5, NA = -1};
   enum Parse_Error {Invalid_Request = -1, Success = 0};
   // Initialize all the fields to NULL
   Http_Request()
      {
      this->_type = Http_Request::NA;
      this->_metric[0] = '\0';
      this->_connection[0] = '\0';
      }
   // Take a string literal as the http request and get the desired fields
   int Parse(char* req)
      {
      long unsigned int num_chars = strcspn(req, "\r");
      char* starter = req;
      char* last_line = strstr(req, "\r\n\r\n");
      if (last_line == NULL)
         {
         std::cout << "Error: Received a malformed request!" << std::endl;
         return Http_Request::Invalid_Request;
         }
      char first_header[64];
      char* saveptr;
      char type[16];
      memset((void*)first_header, 0, sizeof(first_header));
      memset((void*)type, 0, sizeof(type));
      strncpy(first_header, req, std::min(num_chars, sizeof(first_header)-1));
      char* arg = strtok_r((char *)first_header, " ", (char **)&saveptr);
      int i = 0;
      while ((arg != NULL) && (i < 3))
         {
         if (i == 0)
            {
            if ((strlen(arg) == strlen("GET")) && (strcmp(arg, "GET") == 0))
               this->_type = Http_Request::GET;
            else if ((strlen(arg) == strlen("HEAD")) && (strcmp(arg, "HEAD") == 0))
               this->_type = Http_Request::HEAD;
            else if ((strlen(arg) == strlen("POST")) && (strcmp(arg, "POST") == 0))
               this->_type = Http_Request::POST;
            else if ((strlen(arg) == strlen("PUT")) && (strcmp(arg, "PUT") == 0))
               this->_type = Http_Request::PUT;
            else if ((strlen(arg) == strlen("DELETE")) && (strcmp(arg, "DELETE") == 0))
               this->_type = Http_Request::DELETE;
            else if ((strlen(arg) == strlen("PATCH")) && (strcmp(arg, "PATCH") == 0))
               this->_type = Http_Request::PATCH;
            else
               this->_type = Http_Request::NA;
            }
         else if (i == 1)
            {
            strncpy(this->_metric, arg, std::min(strlen(arg)+1, sizeof(this->_metric)-1));
            }
         arg = strtok_r(NULL, " ", (char **)&saveptr);
         i++;
         }

      starter += (num_chars + 2);

      while (starter < (last_line+2))
         {
         num_chars = strcspn(starter, "\r");
         long unsigned int num_field_chars = strcspn(starter, ":");
         char field[16];
         memset((void*)field, 0, sizeof(field));
         strncpy(field, starter, std::min(num_field_chars, sizeof(field)-1));
         field[std::min(num_field_chars, sizeof(field)-1)] = '\0';
         if ((strlen(field) == strlen("Connection")) && (strcmp(field, "Connection") == 0))
            {
            strncpy(this->_connection, starter+num_field_chars+2, std::min(num_chars-num_field_chars-2, sizeof(this->_connection)-1));
            this->_connection[std::min(strlen(field), sizeof(this->_connection)-1)] = '\0';
            break;
            }
         starter += (num_chars + 2);
         }
      return Http_Request::Success;
      }
   // These functions are used to access the stored fields
   int getType() const
      {
      return this->_type;
      }
   const char * getMetric() const
      {
      return this->_metric;
      }
   const char * getConnection() const
      {
      return this->_connection;
      }
   };
