/*******************************************************************************
 * Copyright (c) 2018, 2021 IBM Corp. and others
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

#include <arpa/inet.h>
#include <chrono>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>	/* for TCP_NODELAY option */
#include <openssl/err.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> /// gethostname, read, write
#include "control/CompilationRuntime.hpp"
#include "env/TRMemory.hpp"
#include "env/VMJ9.h"
#include "env/VerboseLog.hpp"
#include "net/CommunicationStream.hpp"
//#include "net/LoadSSLLibs.hpp"
#include "net/ServerStream.hpp"
#include "runtime/CompileService.hpp"
#include "runtime/MetricServer.hpp"
#include "runtime/MetricServerHandler.hpp"

TR_MetricServer::TR_MetricServer()
   : _metricServer(NULL), _metricServerMonitor(NULL), _metricServerOSThread(NULL), _metricServerAttachAttempted(false), _metricServerExitFlag(false) {
}

// This function is where we need to run the server
void TR_MetricServer::handleMetricRequests(J9JavaVm* javaVm) {
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Running JITServer Metrics Server");
   TR_MetricServerHandler::Start(javaVm);
}

TR_MetricsServer * TR_MetricServer::allocate() {
   TR_MetricServer * listener = new (PERSISTENT_NEW) TR_MetricServer();
   return listener;
}

static int32_t J9THREAD_PROC metricServerProc(void * entryarg)
   {
   J9JITConfig * jitConfig = (J9JITConfig *) entryarg;
   J9JavaVM * vm = jitConfig->javaVM;
   TR_MetricServer *listener = ((TR_JitPrivateConfig*)(jitConfig->privateConfig))->listener;
   J9VMThread *listenerThread = NULL;
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);

   int rc = vm->internalVMFunctions->internalAttachCurrentThread(vm, &listenerThread, NULL,
                                  J9_PRIVATE_FLAGS_DAEMON_THREAD | J9_PRIVATE_FLAGS_NO_OBJECT |
                                  J9_PRIVATE_FLAGS_SYSTEM_THREAD | J9_PRIVATE_FLAGS_ATTACHED_THREAD,
                                  listener->getMetricServerOSThread());

   listener->getMetricServerMonitor()->enter();
   listener->setAttachAttempted(true);
   if (rc == JNI_OK)
      listener->setMetricServer(listenerThread);
   listener->getMetricServerMonitor()->notifyAll();
   listener->getMetricServerMonitor()->exit();
   if (rc != JNI_OK)
      return JNI_ERR; // attaching the JITServer Metric Server failed

   j9thread_set_name(j9thread_self(), "JITServer Metric Server");

   listener->handleMetricRequests(vm);

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Detaching JITServer Metrics Server");

   vm->internalVMFunctions->DetachCurrentThread((JavaVM *) vm);
   listener->getMetricsServerMonitor()->enter();
   listener->setMetricsServer(NULL);
   listener->getMetricsServerMonitor()->notifyAll();
   j9thread_exit((J9ThreadMonitor*)listener->getMetricsServerMonitor()->getVMMonitor());

   return 0;
}

void TR_MetricServer::start(J9JavaVM *javaVM) {
   PORT_ACCESS_FROM_JAVAVM(javaVM);

   UDATA priority;
   priority = J9THREAD_PRIORITY_NORMAL;

   _metricServerMonitor = TR::Monitor::create("JITServer-MetricServerMonitor");
   if (_metricServerMonitor)
      {
      // create the thread for listening to a metric request
      const UDATA defaultOSStackSize = javaVM->defaultOSStackSize; //256KB stack size

      if (J9THREAD_SUCCESS != javaVM->internalVMFunctions->createJoinableThreadWithCategory(&_metricServerOSThread,
                                                               defaultOSStackSize,
                                                               priority,
                                                               0,
                                                               &metricServerProc,
                                                               javaVM->jitConfig,
                                                               J9THREAD_CATEGORY_SYSTEM_JIT_THREAD))
         { // cannot create the metric server thread
         j9tty_printf(PORTLIB, "Error: Unable to create JITServer Metric Server.\n");
         TR::Monitor::destroy(_metricServerMonitor);
         _metricServerMonitor = NULL;
         }
      else // must wait here until the thread gets created; otherwise an early shutdown
         { // does not know whether or not to destroy the thread
         _metricServerMonitor->enter();
         while (!getAttachAttempted())
            _metricServerMonitor->wait();
         _metricServerMonitor->exit();
         if (!getMetricServer())
            {
            j9tty_printf(PORTLIB, "Error: JITServer Metric Server attach failed.\n");
            }
         }
      }
   else
      {
      j9tty_printf(PORTLIB, "Error: Unable to create JITServer Metric Server Monitor\n");
      }
   }

int32_t TR_MetricServer::waitForMetricServerExit(J9JavaVM *javaVM) {
   if (NULL != _metricServerOSThread)
      return omrthread_join(_metricServerOSThread);
   else
      return 0;
}

void TR_MetricServer::stop() {
   if (getMetricServer())
      {
      _metricServerMonitor->enter();
      setMetricServerExitFlag();
      _metricServerMonitor->wait();
      _metricServerMonitor->exit();
      TR::Monitor::destroy(_metricServerMonitor);
      _metricServerMonitor = NULL;
      }
}
