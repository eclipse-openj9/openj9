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

#include "runtime/JITServerStatisticsThread.hpp"
#include "runtime/JITClientSession.hpp" // for purgeOldDataIfNeeded()
#include "env/VMJ9.h" // for TR_JitPrivateConfig
#include "control/CompilationRuntime.hpp" // for CompilatonInfo

JITServerStatisticsThread::JITServerStatisticsThread()
   : _statisticsThread(NULL), _statisticsThreadMonitor(NULL), _statisticsOSThread(NULL),
   _statisticsThreadAttachAttempted(false), _statisticsThreadExitFlag(false), _statisticsFrequency(TR::Options::getStatisticsFrequency())
   {
   }

JITServerStatisticsThread * JITServerStatisticsThread::allocate()
   {
   JITServerStatisticsThread * statsThreadObj = new (PERSISTENT_NEW) JITServerStatisticsThread();
   return statsThreadObj;
   }

// Routine executed by the statistics thread
static int32_t J9THREAD_PROC statisticsThreadProc(void * entryarg)
   {
   J9JITConfig * jitConfig = (J9JITConfig *) entryarg;
   J9JavaVM * vm = jitConfig->javaVM;

   JITServerStatisticsThread *statsThreadObj = ((TR_JitPrivateConfig*)(jitConfig->privateConfig))->statisticsThreadObject;
   UDATA samplingPeriod    = std::max(static_cast<UDATA>(TR::Options::_minSamplingPeriod), jitConfig->samplingFrequency);

   J9VMThread *statThread = NULL;
   // Attach this thread to the VM
   int rc = vm->internalVMFunctions->internalAttachCurrentThread(vm, &statThread, NULL,
                                  J9_PRIVATE_FLAGS_DAEMON_THREAD | J9_PRIVATE_FLAGS_NO_OBJECT |
                                  J9_PRIVATE_FLAGS_SYSTEM_THREAD | J9_PRIVATE_FLAGS_ATTACHED_THREAD,
                                  statsThreadObj->getStatisticsOSThread());

   // Inform main thread that attach operation finished (either successfully or not)
   statsThreadObj->getStatisticsThreadMonitor()->enter();
   statsThreadObj->setAttachAttempted(true);
   if (rc == JNI_OK)
      statsThreadObj->setStatisticsThread(statThread);
   statsThreadObj->getStatisticsThreadMonitor()->notifyAll();
   statsThreadObj->getStatisticsThreadMonitor()->exit();

   if (rc != JNI_OK)
      {
      return JNI_ERR; // attaching the JITServer Statistics thread failed
      }

   j9thread_set_name(j9thread_self(), "JITServer Statistics Thread");

   TR::CompilationInfo *compInfo = TR::CompilationInfo::get(jitConfig);
   TR::PersistentInfo *persistentInfo = compInfo->getPersistentInfo();
   PORT_ACCESS_FROM_JAVAVM(vm);
   uint64_t crtTime = j9time_current_time_millis();
   uint64_t lastStatsTime = crtTime;
   uint64_t lastPurgeTime = crtTime;

   persistentInfo->setStartTime(crtTime);
   persistentInfo->setElapsedTime(0);
   while(!statsThreadObj->getStatisticsThreadExitFlag())
      {
      while(!statsThreadObj->getStatisticsThreadExitFlag() && j9thread_sleep_interruptable((IDATA) samplingPeriod, 0) == 0)
         {
         // Read current time but prevent situations where clock goes backwards
         // Maybe we should use a monotonic clock
         uint64_t t = j9time_current_time_millis();
         if (t > crtTime)
            crtTime = t;
         persistentInfo->setElapsedTime(crtTime - persistentInfo->getStartTime());

         // Every 10000 ms look for stale sessions from clients that were inactive
         // for a long time and purge them
         if (crtTime - lastPurgeTime >= 10000)
            {
            lastPurgeTime = crtTime;
            OMR::CriticalSection compilationMonitorLock(compInfo->getCompilationMonitor());
            compInfo->getClientSessionHT()->purgeOldDataIfNeeded();
            }

         // Print operational statistics to vlog if enabled
         if ((statsThreadObj->getStatisticsFrequency() != 0) && ((crtTime - lastStatsTime) > statsThreadObj->getStatisticsFrequency()))
            {
            int32_t cpuUsage = 0, avgCpuUsage = 0, vmCpuUsage = 0;
            CpuUtilization *cpuUtil = compInfo->getCpuUtil();
            if (cpuUtil->isFunctional())
               {
               cpuUtil->updateCpuUtil(jitConfig);
               cpuUsage = cpuUtil->getCpuUsage();
               avgCpuUsage = cpuUtil->getAvgCpuUsage();
               vmCpuUsage = cpuUtil->getVmCpuUsage();
               }
            TR_VerboseLog::vlogAcquire();
            TR_VerboseLog::writeLine(TR_Vlog_JITServer, "Number of clients : %u", compInfo->getClientSessionHT()->size());
            TR_VerboseLog::writeLine(TR_Vlog_JITServer, "Total compilation threads : %d", compInfo->getNumUsableCompilationThreads());
            TR_VerboseLog::writeLine(TR_Vlog_JITServer, "Active compilation threads : %d",compInfo->getNumCompThreadsActive());
            if (cpuUtil->isFunctional())
               {
               TR_VerboseLog::writeLine(TR_Vlog_JITServer, "CpuLoad %d%% (AvgUsage %d%%) JvmCpu %d%%", cpuUsage, avgCpuUsage, vmCpuUsage);
               }
            TR_VerboseLog::vlogRelease();
            lastStatsTime = crtTime;
            }
         }
      // This thread has been interrupted or StatisticsThreadExitFlag flag was set
      }

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Detaching JITServer statistics thread");

   // Will reach here only if the _statisticsThreadExitFlag is set from the stopStatisticsThread().
   vm->internalVMFunctions->DetachCurrentThread((JavaVM *) vm);
   statsThreadObj->getStatisticsThreadMonitor()->enter();
   statsThreadObj->setStatisticsThread(NULL);
   statsThreadObj->getStatisticsThreadMonitor()->notifyAll();
   j9thread_exit((J9ThreadMonitor*)statsThreadObj->getStatisticsThreadMonitor()->getVMMonitor());

   return 0;
   }

void
JITServerStatisticsThread::startStatisticsThread(J9JavaVM *javaVM)
   {
   PORT_ACCESS_FROM_JAVAVM(javaVM);

   _statisticsThreadMonitor = TR::Monitor::create("JITServer-StatisticsThreadMonitor");
   if (_statisticsThreadMonitor)
      {
      const UDATA defaultOSStackSize = javaVM->defaultOSStackSize; //256KB stack size
      if (javaVM->internalVMFunctions->createThreadWithCategory(&_statisticsOSThread,
                                                               defaultOSStackSize,
                                                               J9THREAD_PRIORITY_NORMAL,
                                                               0,
                                                               &statisticsThreadProc,
                                                               javaVM->jitConfig,
                                                               J9THREAD_CATEGORY_SYSTEM_JIT_THREAD))
         { 
         // cannot create the statistics thread
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
            TR::Monitor::destroy(_statisticsThreadMonitor);
            _statisticsThreadMonitor = NULL;
            }
         }
      }
   }

void
JITServerStatisticsThread::stopStatisticsThread(J9JITConfig * jitConfig)
   {
   if (_statisticsThread) // Thread should be attached by now
      {
      getStatisticsThreadMonitor()->enter();
      setStatisticsThreadExitFlag();
      j9thread_interrupt(_statisticsOSThread);

      // wait till the thread exits
      while(_statisticsThread)
         getStatisticsThreadMonitor()->wait();

      getStatisticsThreadMonitor()->exit();

      //Monitor is no longer needed
      getStatisticsThreadMonitor()->destroy();
      _statisticsThreadMonitor = NULL;
      }
   }
