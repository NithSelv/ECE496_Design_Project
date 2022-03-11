/*******************************************************************************
 * Copyright (c) 2018, 2020 IBM Corp. and others
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

#ifndef METRICSERVER_HPP
#define METRICSERVER_HPP

#include "j9.h"
#include "infra/Monitor.hpp"
#include "net/ServerStream.hpp"

//Implementation of a metric endpoint that runs on a separate thread 
//1. Create an instance of the TR_MetricServer object with allocate()
//2. Run the metric server thread with start(javaVM);
//3. Stop it with the stop() function

//NOTE: The code here is only meant to be used to run the server thread and kill it if needed, the actual implementation of the server is in the MetricServerHandler.hpp

class TR_MetricServer {

   private:
      J9VMThread *_metricServer;
      TR::Monitor *_metricServerMonitor;
      j9thread_t _metricServerOSThread;
      volatile bool _metricServerAttachAttempted;
      volatile bool _metricServerExitFlag;

   public:
      TR_MetricServer();
      static TR_MetricServer* allocate();
      void start(J9JITConfig *jitConfig);
      void stop();

      void handleMetricRequests(J9JITConfig *jitConfig);
      int32_t waitForMetricServerExit(J9JavaVM *javaVM);

      void setAttachAttempted(bool b) { _metricServerAttachAttempted = b; }
      bool getAttachAttempted() const { return _metricServerAttachAttempted; }


      J9VMThread* getMetricServer() const { return _metricServer; }
      void setMetricServer(J9VMThread* thread) { _metricServer = thread; }

      j9thread_t getMetricServerOSThread() const { return _metricServerOSThread; }
      TR::Monitor* getMetricServerMonitor() const { return _metricServerMonitor; }

      bool getMetricServerExitFlag() const { return _metricServerExitFlag; }
      void setMetricServerExitFlag() { _metricServerExitFlag = true; }
};

#endif
