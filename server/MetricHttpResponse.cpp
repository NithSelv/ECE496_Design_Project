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


#include "MetricHttpResponse.hpp"

HttpResponse::HttpResponse()
   {
   this->_msg.clear();
   }

void HttpResponse::addHeaderField(const char* field, const char* value)
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

void HttpResponse::addBody(std::string body)
   {
   this->_msg.push_back('\r');
   this->_msg.push_back('\n');
   this->_msg.insert(this->_msg.end(), body.begin(), body.end());
   }

std::vector<char> HttpResponse::prepareHttpResponse()
   {
   return this->_msg;
   }
