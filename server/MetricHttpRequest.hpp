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

#include <cstdio>
#include <cstring>
#include <algorithm>
#include "env/VMJ9.h"
#include "env/TRMemory.hpp"
#include "env/VerboseLog.hpp"
#include "control/CompilationRuntime.hpp"

// This class is used to parse the HTTP Request and query for results
class HttpRequest
   {
// These are the fields that we are interested in
// We do not store the values for all fields
private:
   int _type;
   char _metric[16];
   char _connection[16];

public:
   // Enums for the possible types of Http Requests
   enum HttpTypes {httpGet = 0, httpHead = 1, httpPost = 2, httpPut = 3, httpDelete = 4, httpPatch = 5, httpNa = -1};
   enum ParseError {invalidRequest = -1, success = 0};
   // Initialize all the fields to NULL
   HttpRequest();
   // Take a string literal as the http request and get the desired fields
   int parse(char* req);
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
