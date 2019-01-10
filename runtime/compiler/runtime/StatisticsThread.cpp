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

#include "runtime/StatisticsThread.hpp"
#include "env/VMJ9.h"
#include "runtime/CompileService.hpp"
#include "control/JITaaSCompilationThread.hpp"

TR_StatisticsThread::TR_StatisticsThread()
   : _statisticsThread(NULL), _statisticsThreadMonitor(NULL), _statisticsOSThread(NULL),
   _statisticsThreadAttachAttempted(false), _statisticsThreadExitFlag(false), _statisticsFrequency(TR::Options::getStatisticsFrequency())
   {
   }

TR_StatisticsThread * TR_StatisticsThread::allocate()
   {
   TR_StatisticsThread * statisticsThread = new (PERSISTENT_NEW) TR_StatisticsThread();
   return statisticsThread;
   }

static int32_t J9THREAD_PROC statisticsThreadProc(void * entryarg)
   {
   J9JITConfig * jitConfig = (J9JITConfig *) entryarg;
   J9JavaVM * vm = jitConfig->javaVM;

   TR_StatisticsThread *statisticsThread = ((TR_JitPrivateConfig*)(jitConfig->privateConfig))->statisticsThread;
   UDATA samplingPeriod    = std::max(static_cast<UDATA>(TR::Options::_minSamplingPeriod), jitConfig->samplingFrequency);

   J9VMThread *statThread = NULL;
   PORT_ACCESS_FROM_JAVAVM(vm);

   int rc = vm->internalVMFunctions->internalAttachCurrentThread(vm, &statThread, NULL,
                                  J9_PRIVATE_FLAGS_DAEMON_THREAD | J9_PRIVATE_FLAGS_NO_OBJECT |
                                  J9_PRIVATE_FLAGS_SYSTEM_THREAD | J9_PRIVATE_FLAGS_ATTACHED_THREAD,
                                  statisticsThread->getStatisticsOSThread());
   statisticsThread->getStatisticsThreadMonitor()->enter();
   statisticsThread->setAttachAttempted(true);
   if (rc == JNI_OK)
      statisticsThread->setStatisticsThread(statThread);

   statisticsThread->getStatisticsThreadMonitor()->notifyAll();
   statisticsThread->getStatisticsThreadMonitor()->exit();

   if (rc != JNI_OK)
      {
      return JNI_ERR; // attaching the JITaaS Statistics thread failed
      }

   j9thread_set_name(j9thread_self(), "JITaaS Server Statistics Thread");

   TR::CompilationInfo *compInfo = TR::CompilationInfo::get(jitConfig);
   TR::PersistentInfo *persistentInfo = compInfo->getPersistentInfo();
   uint64_t crtTime = j9time_current_time_millis();
   uint64_t lastStatistics = crtTime;

   persistentInfo->setStartTime(crtTime);
   persistentInfo->setElapsedTime(0);
   while(!statisticsThread->getStatisticsThreadExitFlag())
      {
      while(!statisticsThread->getStatisticsThreadExitFlag() && j9thread_sleep_interruptable((IDATA) samplingPeriod, 0) == 0)
         {
         crtTime = j9time_current_time_millis();
         persistentInfo->setElapsedTime(crtTime - persistentInfo->getStartTime());

         if ((statisticsThread->getStatisticsFrequency() != 0) && ((crtTime - lastStatistics) > statisticsThread->getStatisticsFrequency()))
            {
            int32_t cpuUsage = 0, avgCpuUsage = 0, vmCpuUsage = 0;
            bool isFunctional = false;
            CpuUtilization *cpuUtil = compInfo->getCpuUtil();
            if (cpuUtil->isFunctional())
               {
               cpuUtil->updateCpuUtil(jitConfig);
               cpuUsage = cpuUtil->getCpuUsage();
               avgCpuUsage = cpuUtil->getAvgCpuUsage();
               vmCpuUsage = cpuUtil->getVmCpuUsage();
               isFunctional = true;
               }
            TR_VerboseLog::vlogAcquire();
            TR_VerboseLog::writeLine(TR_Vlog_JITaaS, "Number of clients : %u", compInfo->getClientSessionHT()->size());
            TR_VerboseLog::writeLine(TR_Vlog_JITaaS, "Total compilation threads : %d", compInfo->getNumUsableCompilationThreads());
            TR_VerboseLog::writeLine(TR_Vlog_JITaaS, "Active compilation threads : %d",compInfo->getNumCompThreadsActive());
            if (isFunctional)
               {
               TR_VerboseLog::writeLine(TR_Vlog_JITaaS, "CpuLoad %d%% (AvgUsage %d%%) JvmCpu %d%%", cpuUsage, avgCpuUsage, vmCpuUsage);
               }
            TR_VerboseLog::vlogRelease();
            lastStatistics = crtTime;
            }
         }
      // This thread has been interrupted or StatisticsThreadExitFlag flag was set
      }

   if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Detaching JITaaSServer statistics thread");

   // Will reach here only if the StatisticsThreadExitFlag is set from the StopStatisticsThread.
   vm->internalVMFunctions->DetachCurrentThread((JavaVM *) vm);
   statisticsThread->getStatisticsThreadMonitor()->enter();
   statisticsThread->setStatisticsThread(NULL);
   statisticsThread->getStatisticsThreadMonitor()->notifyAll();
   j9thread_exit((J9ThreadMonitor*)statisticsThread->getStatisticsThreadMonitor()->getVMMonitor());

   return 0;
   }

void
TR_StatisticsThread::startStatisticsThread(J9JavaVM *javaVM)
   {
   PORT_ACCESS_FROM_JAVAVM(javaVM);
   UDATA priority;
   priority = J9THREAD_PRIORITY_NORMAL;

   _statisticsThreadMonitor = TR::Monitor::create("JITaaS-StatisticsThreadMonitor");
   if (_statisticsThreadMonitor)
      {
      const UDATA defaultOSStackSize = javaVM->defaultOSStackSize; //256KB stack size
      if (javaVM->internalVMFunctions->createThreadWithCategory(&_statisticsOSThread,
                                                               defaultOSStackSize,
                                                               priority,
                                                               0,
                                                               &statisticsThreadProc,
                                                               javaVM->jitConfig,
                                                               J9THREAD_CATEGORY_SYSTEM_JIT_THREAD))
         { // cannot create the statistics thread
         j9tty_printf(PORTLIB, "Error: Unable to create JITaaS Statistics Thread.\n");
         TR::Monitor::destroy(_statisticsThreadMonitor);
         _statisticsThreadMonitor = NULL;
         }
      else // must wait here until the thread gets created; otherwise an early shutdown
         { // does not know whether or not to destroy the thread
         _statisticsThreadMonitor->enter();
         while (!getAttachAttempted())
            _statisticsThreadMonitor->wait();
         _statisticsThreadMonitor->exit();
         if (!getStatisticsThread())
            {
            j9tty_printf(PORTLIB, "Error: JITaaS Statistics Thread attach failed.\n");
            }
         }
      }
   else
      {
      j9tty_printf(PORTLIB, "Error: Unable to create JITaaS Statistics Monitor\n");
      }
   }

void
TR_StatisticsThread::stopStatisticsThread(J9JITConfig * jitConfig)
   {
   if (_statisticsThread) // Thread should be attached by now
      {
      getStatisticsThreadMonitor()->enter();
      setStatisticsThreadExitFlag();
      j9thread_interrupt(_statisticsOSThread);

      // wait till the thread exit
      while(_statisticsThread)
         getStatisticsThreadMonitor()->wait();

      getStatisticsThreadMonitor()->exit();

      //Monitor no more needed
      getStatisticsThreadMonitor()->destroy();
      _statisticsThreadMonitor = NULL;
      }
   }
