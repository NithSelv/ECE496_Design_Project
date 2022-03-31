/*******************************************************************************
 * Copyright (c) 2000, 2022 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "MetricHttpRequest.hpp"

HttpRequest::HttpRequest()
   {
   this->_type = HttpRequest::httpNa;
   this->_metric[0] = '\0';
   this->_connection[0] = '\0';
   }
// Take a string literal as the http request and get the desired fields
int HttpRequest::parse(char* req)
   {
   long unsigned int numChars = strcspn(req, "\r");
   char* starter = req;
   char* lastLine = strstr(req, "\r\n\r\n");
   if (lastLine == NULL)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Metric Server: Received Malformed Http Request!\n");
      perror("Metric Server: Received a malformed request!");
      return HttpRequest::invalidRequest;
      }
   char firstHeader[64];
   char* saveptr;
   char type[16];
   memset((void*)firstHeader, 0, sizeof(firstHeader));
   memset((void*)type, 0, sizeof(type));
   strncpy(firstHeader, req, std::min(numChars, sizeof(firstHeader)-1));
   char* arg = strtok_r((char *)firstHeader, " ", (char **)&saveptr);
   int i = 0;
   while ((arg != NULL) && (i < 3))
      {
      if (i == 0)
         {
         if ((strlen(arg) == strlen("GET")) && (strcmp(arg, "GET") == 0))
            this->_type = HttpRequest::httpGet;
         else if ((strlen(arg) == strlen("HEAD")) && (strcmp(arg, "HEAD") == 0))
            this->_type = HttpRequest::httpHead;
         else if ((strlen(arg) == strlen("POST")) && (strcmp(arg, "POST") == 0))
            this->_type = HttpRequest::httpPost;
         else if ((strlen(arg) == strlen("PUT")) && (strcmp(arg, "PUT") == 0))
            this->_type = HttpRequest::httpPut;
         else if ((strlen(arg) == strlen("DELETE")) && (strcmp(arg, "DELETE") == 0))
            this->_type = HttpRequest::httpDelete;
         else if ((strlen(arg) == strlen("PATCH")) && (strcmp(arg, "PATCH") == 0))
            this->_type = HttpRequest::httpPatch;
         else
            this->_type = HttpRequest::httpNa;
         }
      else if (i == 1)
         {
         strncpy(this->_metric, arg, std::min(strlen(arg)+1, sizeof(this->_metric)-1));
         }
      arg = strtok_r(NULL, " ", (char **)&saveptr);
      i++;
      }

   starter += (numChars + 2);

   while (starter < (lastLine+2))
      {
      numChars = strcspn(starter, "\r");
      long unsigned int numFieldChars = strcspn(starter, ":");
      char field[16];
      memset((void*)field, 0, sizeof(field));
      strncpy(field, starter, std::min(numFieldChars, sizeof(field)-1));
      field[std::min(numFieldChars, sizeof(field)-1)] = '\0';
      if ((strlen(field) == strlen("Connection")) && (strcmp(field, "Connection") == 0))
         {
         strncpy(this->_connection, starter+numFieldChars+2, std::min(numChars-numFieldChars-2, sizeof(this->_connection)-1));
         this->_connection[std::min(strlen(field), sizeof(this->_connection)-1)] = '\0';
         break;
         }
      starter += (numChars + 2);
      }
   return HttpRequest::success;
   }
