/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#ifndef STATISTICSTHREAD_HPP
#define STATISTICSTHREAD_HPP

#include "j9.h"
#include "infra/Monitor.hpp"  // TR::Monitor

class TR_StatisticsThread
   {
public:
   TR_StatisticsThread();
   static TR_StatisticsThread* allocate();
   void startStatisticsThread(J9JavaVM *javaVM);
   void setAttachAttempted(bool b) { _statisticsThreadAttachAttempted = b; }
   bool getAttachAttempted() { return _statisticsThreadAttachAttempted; }

   J9VMThread* getStatisticsThread() { return _statisticsThread; }
   void setStatisticsThread(J9VMThread* thread) { _statisticsThread = thread; }
   j9thread_t getStatisticsOSThread() { return _statisticsOSThread; }
   TR::Monitor* getStatisticsThreadMonitor() { return _statisticsThreadMonitor; }

   uint32_t getStatisticsThreadExitFlag() { return _statisticsThreadExitFlag; }
   void setStatisticsThreadExitFlag() { _statisticsThreadExitFlag = 1; }
   void stopStatisticsThread(J9JITConfig * jitConfig);
   int32_t getStatisticsFrequency() { return _statisticsFrequency; };
private:
   J9VMThread *_statisticsThread;
   TR::Monitor *_statisticsThreadMonitor;
   j9thread_t _statisticsOSThread;
   volatile bool _statisticsThreadAttachAttempted;
   volatile uint32_t _statisticsThreadExitFlag;
   int32_t _statisticsFrequency;
   };

#endif
