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
#include <string>
#include <map>
#include <iostream>
#include <sys/resource.h>
#include <time.h>

// This class is used to add/update/store metrics for use by the server
class MetricsDatabase
   {
private:
   std::map<std::string, std::string> _mdb;
   
public:
   void addMetric(const char* name, char* value)
      {
      this->_mdb.insert(std::make_pair(name, value));
      }

   std::string findMetric(const char* name) // changed return type from char* (unused so far)
      {
      std::map<std::string, std::string>::iterator it = _mdb.find(name);
      if (it != _mdb.end())
         {
         return it->second; // found
         }
      else
         {
         std::string s;
         s.clear();
         return s; // not found
         }
      }

   int setMetric(const char* name, char* value)
      {
      std::map<std::string, std::string>::iterator it = this->_mdb.find(name);
      if (it != this->_mdb.end()) // found
         {
         it->second = value;
         return 0;
         }
      else
         {
         return -1;
         }
      }

   std::string prepareAllMetricsBody() // changed return type from char* (used in server.cpp)
      {
      std::string msg;
      for (auto it=this->_mdb.begin(); it!=this->_mdb.end(); ++it)
         {
         msg.append(it->first);
         msg.append(" ");
         msg.append(it->second);
         msg.append("\n");
         }
      return msg;
      }

   void print()
      {
      for (auto it=this->_mdb.begin(); it!=this->_mdb.end(); ++it)
         {
         std::cout << it->first << " => " << it->second << std::endl;
         }
      }

   int initialize(double startTime)
      {
      struct rusage stats;
      struct timeval now;
      char data[4096];
      memset((void*)data, 0, sizeof(data));
      gettimeofday(&now, NULL);
      int ret = getrusage(RUSAGE_SELF, &stats);
      if (ret == 0)
         {
         double cpuTime = stats.ru_utime.tv_sec + 0.000001 * stats.ru_utime.tv_usec;
         sprintf(data, "%f", cpuTime);
         this->addMetric("cpuTime", data);
         memset((void*)data, 0, sizeof(data));
         double calendarTime = now.tv_sec + 0.000001 * now.tv_usec - startTime;
         sprintf(data, "%f", calendarTime);
         this->addMetric("calendarTime", data);
         }
      return ret;
      }

   int update(double startTime)
      {
      struct rusage stats;
      struct timeval now;
      char data[4096];
      memset((void*)data, 0, sizeof(data));
      gettimeofday(&now, NULL);
      int ret = getrusage(RUSAGE_SELF, &stats);
      if (ret == 0)
         {
         double cpuTime = stats.ru_utime.tv_sec + 0.000001 * stats.ru_utime.tv_usec;
         sprintf(data, "%f", cpuTime);
         this->setMetric("cpuTime", data);
         memset((void*)data, 0, sizeof(data));
         double calendarTime = now.tv_sec + 0.000001 * now.tv_usec - startTime;
         sprintf(data, "%f", calendarTime);
         this->setMetric("calendarTime", data);
         }
      return ret;
      }
   };
