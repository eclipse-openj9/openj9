/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

#ifndef JITSERVER_STATISTICSTHREAD_HPP
#define JITSERVER_STATISTICSTHREAD_HPP

#include "j9.h" // for J9JavaVM
#include "infra/Monitor.hpp"  // for TR::Monitor

/**
   @class JITServerStatisticsThread
   @brief Implementation of a heartbeat mechanism that periodically prints operational statistics to vlog

   The JITServer statistics thread plays the role of the samplingThread in a normal JVM
   It has 3 main duties:
   1) Keeps track of time so that other parties can have a cheap way of accessing elapsed time
   2) Purges the stale client sessions periodically (every 10 seconds)
   3) Prints to vlog operational statistics like: number of clients that are connected, 
      number of active compilations threads, CPU utilization of the JITServer, etc.
   The period of the statistics printout is given by _statisticsFrequency. If this value is 0, 
   no statistics are printed. This value can be changed with -Xjit:statisticsFrequency=<period-in-ms>
   To disable the JITServerStatisticsThread functionality completely use -Xjit:samplingFrequency=0
*/
class JITServerStatisticsThread
   {
   public:
   JITServerStatisticsThread();
   /**
      @brief Allocate a JITServerStatisticsThread object which can be used to start/stop a statistics thread
   */
   static JITServerStatisticsThread* allocate();

   /**
      @brief Creates and starts a statistics thread for JITServer
      If the thread cannot be created/started, getStatisticsThread() will return 0
   */
   void startStatisticsThread(J9JavaVM *javaVM);

   /**
      @brief Stops the statistics thread and destroys the statisticsThreadMonitor
      This routine is typically executed at shutdown. 
   */
   void stopStatisticsThread(J9JITConfig * jitConfig);

   void setAttachAttempted(bool b) { _statisticsThreadAttachAttempted = b; }
   bool getAttachAttempted() const { return _statisticsThreadAttachAttempted; }
   J9VMThread* getStatisticsThread() const { return _statisticsThread; }
   void setStatisticsThread(J9VMThread* thread) { _statisticsThread = thread; }
   j9thread_t getStatisticsOSThread() const { return _statisticsOSThread; }
   TR::Monitor* getStatisticsThreadMonitor() const { return _statisticsThreadMonitor; }
   bool getStatisticsThreadExitFlag() const { return _statisticsThreadExitFlag; }
   void setStatisticsThreadExitFlag() { _statisticsThreadExitFlag = true; }
   int32_t getStatisticsFrequency() const { return _statisticsFrequency; };

   private:
   J9VMThread *_statisticsThread;
   TR::Monitor *_statisticsThreadMonitor;
   j9thread_t _statisticsOSThread;
   volatile bool _statisticsThreadAttachAttempted;
   volatile bool _statisticsThreadExitFlag;
   int32_t _statisticsFrequency; // 0 means that we don't want any statistics printed
   };

#endif // JITSERVER_STATISTICSTHREAD_HPP
