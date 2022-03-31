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

#include "MetricDatabase.hpp"
std::string MetricDatabase::findMetric(const char* name) // changed return type from char* (unused so far)
   {
   std::map<std::string, std::string>::iterator it = _mdb.find(name);
   if (it != _mdb.end())
      {
      return it->second; // found
      }
   else
      {
      perror("Metric Server: Metric to be read not found!");
      std::string s;
      s.clear();
      return s; // not found
      }
   }

void MetricDatabase::setMetric(const char* name, char* value)
   {
   std::map<std::string, std::string>::iterator it = this->_mdb.find(name);
   if (it != this->_mdb.end()) // found
      {
      it->second = value;
      }
   else
      {
      this->_mdb.insert(std::make_pair(name, value));
      }
   }

std::string MetricDatabase::prepareAllMetricsBody() // changed return type from char* (used in server.cpp)
   {
   std::string msg;
   msg.clear();
   for (std::map<std::string, std::string>::iterator it=this->_mdb.begin(); it!=this->_mdb.end(); ++it)
      {
      msg.append(it->first);
      msg.append(" ");
      msg.append(it->second);
      msg.append("\n");
      }
   return msg;
   }

void MetricDatabase::print()
   {
   for (std::map<std::string, std::string>::iterator it=this->_mdb.begin(); it!=this->_mdb.end(); ++it)
      {
      std::cout << it->first << " => " << it->second << std::endl;
      }
   }

void MetricDatabase::update(J9JITConfig* jitConfig)
   {
   //These metrics are from JITServer
   TR::CompilationInfo* cInfo = TR::CompilationInfo::get(jitConfig);
   CpuUtilization* cUtil = cInfo->getCpuUtil();

   char data[4096];
   memset((void*)data, 0, sizeof(data));
   sprintf(data, "%d", cInfo->getMethodQueueSize());
   this->setMetric("compilationQueueSize", data);
   memset((void*)data, 0, sizeof(data));
   sprintf(data, "%u", cInfo->getClientSessionHT()->size());
   this->setMetric("numberOfClients", data);
   memset((void*)data, 0, sizeof(data));
   sprintf(data, "%d", cInfo->getNumUsableCompilationThreads());
   this->setMetric("totalNumberOfCompilationThreads", data);
   memset((void*)data, 0, sizeof(data));
   sprintf(data, "%d", cInfo->getNumCompThreadsActive());
   this->setMetric("totalNumberOfActiveThreads", data);
   memset((void*)data, 0, sizeof(data));
   bool incompInfo;
   sprintf(data, "%lu", (cInfo->computeAndCacheFreePhysicalMemory(incompInfo) >> 20));
   this->setMetric("physicalMemoryFreeInMB", data); 

   if (TR::CompilationInfoPerThreadRemote::getNumClearedCaches() > 0)
      {
      memset((void*)data, 0, sizeof(data));
      sprintf(data, "%d", TR::CompilationInfoPerThreadRemote::getNumClearedCaches());
      this->setMetric("numberOfClearedCaches", data);
      }

   if (cUtil->isFunctional())
      {
      memset((void*)data, 0, sizeof(data));
      sprintf(data, "%d", cUtil->getCpuUsage());
      this->setMetric("percentCpuLoad", data);
      memset((void*)data, 0, sizeof(data));
      sprintf(data, "%d", cUtil->getAvgCpuUsage());
      this->setMetric("percentAverageCpuLoad", data);
      memset((void*)data, 0, sizeof(data));
      sprintf(data, "%d", cUtil->getVmCpuUsage());
      this->setMetric("percentJVMCpuLoad", data);
      }

   //These metrics are from the metric server thread
   struct rusage r;
   if (getrusage(RUSAGE_THREAD, (struct rusage*)&r) == 0)
      {
      memset((void*)data, 0, sizeof(data));
      sprintf(data, "%li", r.ru_maxrss);
      this->setMetric("metricServerMaxPeakMemInKB", data);
      memset((void*)data, 0, sizeof(data));
      sprintf(data, "%li", r.ru_minflt);
      this->setMetric("metricServerNumPageFaultNotIO", data);
      memset((void*)data, 0, sizeof(data));
      sprintf(data, "%li", r.ru_majflt);
      this->setMetric("metricServerNumPageFaultByIO", data);
      memset((void*)data, 0, sizeof(data));
      sprintf(data, "%li", r.ru_inblock);
      this->setMetric("metricServerNumInput", data);
      memset((void*)data, 0, sizeof(data));
      sprintf(data, "%li", r.ru_oublock);
      this->setMetric("metricServerNumOutput", data);
      memset((void*)data, 0, sizeof(data));
      sprintf(data, "%li", r.ru_nvcsw);
      this->setMetric("metricServerNumVolCS", data);
      memset((void*)data, 0, sizeof(data));
      sprintf(data, "%li", r.ru_nivcsw);
      this->setMetric("metricServerNumInVolCS", data);
      }
   memset((void*)data, 0, sizeof(data));
   struct sysinfo s;
   if (sysinfo((struct sysinfo*)&s) == 0) 
      {
      sprintf(data, "%lu", s.totalram);
      this->setMetric("machineTotalUsageMemInB", data);
      memset((void*)data, 0, sizeof(data));
      sprintf(data, "%lu", s.freeram);
      this->setMetric("machineTotalFreeMemInB", data);
      memset((void*)data, 0, sizeof(data));
      }
   }
