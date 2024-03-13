/*******************************************************************************
 * Copyright IBM Corp. and others 2024
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "j9.h"
#include "j9nonbuilder.h"
#include "j9thread.h"

#include "control/CompilationRuntime.hpp"
#include "control/CompileBeforeCheckpoint.hpp"
#include "control/Options.hpp"
#include "control/OptionsPostRestore.hpp"
#include "env/RawAllocator.hpp"
#include "env/J9SegmentAllocator.hpp"
#include "env/SystemSegmentProvider.hpp"
#include "env/VerboseLog.hpp"
#include "infra/CriticalSection.hpp"
#include "infra/Monitor.hpp"
#include "runtime/CRRuntime.hpp"
#include "runtime/J9VMAccess.hpp"
#if defined(J9VM_OPT_JITSERVER)
#include "net/ClientStream.hpp"
#endif

class ReleaseVMAccessAndAcquireMonitor
   {
   public:
   ReleaseVMAccessAndAcquireMonitor(J9VMThread *vmThread, TR::Monitor *monitor)
      : _hadVMAccess(vmThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS),
        _vmThread(vmThread),
        _monitor(monitor)
      {
      if (_hadVMAccess)
         releaseVMAccessNoSuspend(_vmThread);
      _monitor->enter();
      }

   ~ReleaseVMAccessAndAcquireMonitor()
      {
      _monitor->exit();
      if (_hadVMAccess)
         acquireVMAccessNoSuspend(_vmThread);
      }

   private:
   bool         _hadVMAccess;
   J9VMThread  *_vmThread;
   TR::Monitor *_monitor;
   };

TR::CRRuntime::CRRuntime(J9JITConfig *jitConfig, TR::CompilationInfo *compInfo) :
   _jitConfig(jitConfig),
   _compInfo(compInfo),
   _compMonitor(compInfo->getCompilationMonitor()),
   _crMonitor(TR::Monitor::create("JIT-CheckpointRestoreMonitor")),
   _crRuntimeMonitor(TR::Monitor::create("JIT-CRRuntimeMonitor")),
   _crRuntimeThread(NULL),
   _crRuntimeOSThread(NULL),
   _crRuntimeThreadLifetimeState(TR_CRRuntimeThreadLifetimeStates::CR_THR_NOT_CREATED),
   _checkpointStatus(TR_CheckpointStatus::NO_CHECKPOINT_IN_PROGRESS)
   {
   // TR::CompilationInfo is initialized in the JIT_INITIALIZED bootstrap
   // stage, whereas J9_EXTENDED_RUNTIME_METHOD_TRACE_ENABLED is set in the
   // TRACE_ENGINE_INITIALIZED stage, which happens first.
   _vmMethodTraceEnabled = jitConfig->javaVM->extendedRuntimeFlags & J9_EXTENDED_RUNTIME_METHOD_TRACE_ENABLED;

   // TR::CompilationInfo is initialized in the JIT_INITIALIZED bootstrap
   // stage, whereas dump agents are initialized in the
   // PORT_LIBRARY_GUARANTEED stage, which happens first.
   bool exceptionCatchEventHooked
      = J9_EVENT_IS_HOOKED(jitConfig->javaVM->hookInterface, J9HOOK_VM_EXCEPTION_CATCH)
        || J9_EVENT_IS_RESERVED(jitConfig->javaVM->hookInterface, J9HOOK_VM_EXCEPTION_CATCH);
   bool exceptionThrowEventHooked
      = J9_EVENT_IS_HOOKED(jitConfig->javaVM->hookInterface, J9HOOK_VM_EXCEPTION_THROW)
        || J9_EVENT_IS_RESERVED(jitConfig->javaVM->hookInterface, J9HOOK_VM_EXCEPTION_THROW);
   _vmExceptionEventsHooked = exceptionCatchEventHooked || exceptionThrowEventHooked;

#if defined(J9VM_OPT_JITSERVER)
   _canPerformRemoteCompilationInCRIUMode = false;
   _remoteCompilationRequestedAtBootstrap = false;
   _remoteCompilationExplicitlyDisabledAtBootstrap = false;
#endif
   }

void
TR::CRRuntime::acquireCRMonitor()
   {
   _crMonitor->enter();
   }

void
TR::CRRuntime::releaseCRMonitor()
   {
   _crMonitor->exit();
   }

void
TR::CRRuntime::waitOnCRMonitor()
   {
   _crMonitor->wait();
   }

void
TR::CRRuntime::acquireCompMonitor()
   {
   _compMonitor->enter();
   }

void
TR::CRRuntime::releaseCompMonitor()
   {
   _compMonitor->exit();
   }

void
TR::CRRuntime::acquireCRRuntimeMonitor()
   {
   _crRuntimeMonitor->enter();
   }

void
TR::CRRuntime::releaseCRRuntimeMonitor()
   {
   _crRuntimeMonitor->exit();
   }

void
TR::CRRuntime::waitOnCRRuntimeMonitor()
   {
   _crRuntimeMonitor->wait();
   }

/* IMPORTANT: There should be no return, or C++ exception thrown, after
 *            releasing the Comp Monitor below until it is re-acquired.
 *            The Comp Monitor may be acquired in an OMR::CriticalSection
 *            object; a return, or thrown exception, will destruct this
 *            object as part leaving the scope, or stack unwinding, which
 *            will attempt to release the monitor in its destructor.
 */
void TR::CRRuntime::releaseCompMonitorUntilNotifiedOnCRMonitor() throw()
   {
   TR_ASSERT_FATAL(getCompilationMonitor()->owned_by_self(),
                   "releaseCompMonitorUntilNotifiedOnCRMonitor should not be called without the Comp Monitor!\n");

   /* Acquire the CR monitor */
   acquireCRMonitor();

   /* Release the Comp Monitor before waiting */
   releaseCompMonitor();

   /* Wait until notified */
   waitOnCRMonitor();

   /* Release CR Monitor and re-acquire the Comp Monitor */
   releaseCRMonitor();
   acquireCompMonitor();
   }

bool TR::CRRuntime::compileMethodsForCheckpoint(J9VMThread *vmThread)
   {
   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCheckpointRestore))
      TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "Preparing to compile methods for checkpoint");

   /* Indicate to compilation threads that they should compile methods for checkpoint/restore */
   setCompileMethodsForCheckpoint();

   /* Queue compilation of interpreted methods */
   try
      {
      TR_J9VMBase *fej9 = TR_J9VMBase::get(getJITConfig(), vmThread);

      TR::RawAllocator rawAllocator(getJITConfig()->javaVM);
      J9::SegmentAllocator segmentAllocator(MEMORY_TYPE_JIT_SCRATCH_SPACE | MEMORY_TYPE_VIRTUAL, *getJITConfig()->javaVM);
      J9::SystemSegmentProvider regionSegmentProvider(
         1 << 20,
         1 << 20,
         TR::Options::getScratchSpaceLimit(),
         segmentAllocator,
         rawAllocator
         );
      TR::Region compForCheckpointRegion(regionSegmentProvider, rawAllocator);

      TR::CompileBeforeCheckpoint compileBeforeCheckpoint(compForCheckpointRegion, vmThread, fej9, getCompInfo());
      compileBeforeCheckpoint.collectAndCompileMethodsBeforeCheckpoint();
      }
   catch (const std::exception &e)
      {
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCheckpointRestore))
         TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "Failed to collect methods for compilation before checkpoint");
      }

   /* Determine whether to wait on the CR Monitor. */
   while (getCompInfo()->getMethodQueueSize() && !shouldCheckpointBeInterrupted())
      {
      releaseCompMonitorUntilNotifiedOnCRMonitor();
      }

   /* Abort if checkpoint should be interrupted. */
   if (shouldCheckpointBeInterrupted())
      {
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCheckpointRestore))
         TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "Aborting; checkpoint is interrupted");
      return false;
      }

   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCheckpointRestore))
      TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "Done compiling methods for checkpoint");

   return true;
   }

bool TR::CRRuntime::suspendCompThreadsForCheckpoint(J9VMThread *vmThread)
   {
   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCheckpointRestore))
      TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "Preparing to suspend threads for checkpoint");

   /* Indicate to compilation threads that they should suspend for checkpoint/restore */
   setSuspendThreadsForCheckpoint();

   /* Inform compilation threads to suspend themselves. The compilation
    * threads will complete any in-progress compilations first.
    */
   bool purgeMethodQueue = false;
   getCompInfo()->suspendCompilationThread(purgeMethodQueue);

   /* With the thread state now updated, notify any active comp threads waiting for work */
   getCompilationMonitor()->notifyAll();

   /* Wait until all compilation threads are suspended. */
   for (int32_t i = getCompInfo()->getFirstCompThreadID(); i <= getCompInfo()->getLastCompThreadID(); i++)
      {
      TR::CompilationInfoPerThread *curCompThreadInfoPT = getCompInfo()->getArrayOfCompilationInfoPerThread()[i];
      /* Determine whether to wait on the CR Monitor. */
      while (!shouldCheckpointBeInterrupted()
             && curCompThreadInfoPT->getCompilationThreadState() != COMPTHREAD_SUSPENDED)
         {
         releaseCompMonitorUntilNotifiedOnCRMonitor();
         }

      /* Stop cycling through the threads if checkpointing should be interrupted. */
      if (shouldCheckpointBeInterrupted())
         {
         if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCheckpointRestore))
            TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "Aborting; checkpoint is interrupted");
         return false;
         }
      }

   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCheckpointRestore))
      TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "Finished suspending threads for checkpoint");

   return true;
   }

bool
TR::CRRuntime::suspendJITThreadsForCheckpoint(J9VMThread *vmThread)
   {
   // Suspend compilation threads for checkpoint
   if (!suspendCompThreadsForCheckpoint(vmThread))
      return false;

   // Suspend Sampler Thread
   if (getJITConfig()->samplerMonitor)
      {
      j9thread_monitor_enter(getJITConfig()->samplerMonitor);
      j9thread_interrupt(getJITConfig()->samplerThread);

      // Determine whether to wait on the CR Monitor.
      //
      // Note, this thread releases the sampler monitor and then
      // acquires the CR monitor inside releaseCompMonitorUntilNotifiedOnCRMonitor.
      while (!shouldCheckpointBeInterrupted()
             && getCompInfo()->getSamplingThreadLifetimeState() != TR::CompilationInfo::SAMPLE_THR_SUSPENDED)
         {
         j9thread_monitor_exit(getJITConfig()->samplerMonitor);
         releaseCompMonitorUntilNotifiedOnCRMonitor();
         j9thread_monitor_enter(getJITConfig()->samplerMonitor);
         }

      j9thread_monitor_exit(getJITConfig()->samplerMonitor);
      }

   // Suspend IProfiler Thread
   TR_IProfiler *iProfiler = TR_J9VMBase::get(getJITConfig(), NULL)->getIProfiler();
   if (iProfiler && iProfiler->getIProfilerMonitor())
      {
      iProfiler->getIProfilerMonitor()->enter();

      TR_ASSERT_FATAL(iProfiler->getIProfilerThreadLifetimeState() != TR_IProfiler::IPROF_THR_SUSPENDED,
                      "IProfiler Thread should not already be in state IPROF_THR_SUSPENDED.\n");

      // Don't change the state if the JVM is currently shutting down.
      if (iProfiler->getIProfilerThreadLifetimeState() != TR_IProfiler::IPROF_THR_STOPPING)
         iProfiler->setIProfilerThreadLifetimeState(TR_IProfiler::IPROF_THR_SUSPENDING);

      // During shutdown, both the IProfiler Thread and the Shutdown Thread could be
      // waiting on the IProfiler Monitor, so notifyAll so that the IProfiler Thread
      // wakes up.
      iProfiler->getIProfilerMonitor()->notifyAll();

      // Determine whether to wait on the CR Monitor.
      //
      // Note, this thread releases the iprofiler monitor and then
      // acquires the CR monitor inside releaseCompMonitorUntilNotifiedOnCRMonitor.
      while (!shouldCheckpointBeInterrupted()
             && iProfiler->getIProfilerThreadLifetimeState() != TR_IProfiler::IPROF_THR_SUSPENDED)
         {
         iProfiler->getIProfilerMonitor()->exit();
         releaseCompMonitorUntilNotifiedOnCRMonitor();
         iProfiler->getIProfilerMonitor()->enter();
         }

      iProfiler->getIProfilerMonitor()->exit();
      }

   return !shouldCheckpointBeInterrupted();
   }

void
TR::CRRuntime::resumeJITThreadsForRestore(J9VMThread *vmThread)
   {
   // Resume suspended IProfiler Thread
   TR_IProfiler *iProfiler = TR_J9VMBase::get(getJITConfig(), NULL)->getIProfiler();
   if (iProfiler && iProfiler->getIProfilerMonitor())
      {
      iProfiler->getIProfilerMonitor()->enter();
      iProfiler->setIProfilerThreadLifetimeState(TR_IProfiler::IPROF_THR_RESUMING);
      iProfiler->getIProfilerMonitor()->notifyAll();
      iProfiler->getIProfilerMonitor()->exit();
      }

   // Resume suspended Sampler Thread
   if (getJITConfig()->samplerMonitor)
      {
      j9thread_monitor_enter(getJITConfig()->samplerMonitor);
      getCompInfo()->setSamplingThreadLifetimeState(TR::CompilationInfo::SAMPLE_THR_RESUMING);
      j9thread_monitor_notify_all(getJITConfig()->samplerMonitor);
      j9thread_monitor_exit(getJITConfig()->samplerMonitor);
      }

   // Resume suspended compilation threads.
   getCompInfo()->resumeCompilationThread();
   }

/* Post-restore, reset the start time. While the Checkpoint phase is
 * conceptually part of building the application, in order to ensure
 * consistency with parts of the compiler that memoize elapsd time,
 * the start time is reset to pretend like the JVM started
 * persistentInfo->getElapsedTime() milliseconds ago. This will impact
 * options such as -XsamplingExpirationTime. However, such an option
 * may not make sense in the context of checkpoint/restore.
 */
void
TR::CRRuntime::resetStartTime()
   {
   PORT_ACCESS_FROM_JAVAVM(jitConfig->javaVM);
   TR::PersistentInfo *persistentInfo = getCompInfo()->getPersistentInfo();

   if (TR::Options::isAnyVerboseOptionSet())
      TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "Start and elapsed time: startTime=%6u, elapsedTime=%6u",
                                       (uint32_t)persistentInfo->getStartTime(), (uint32_t)persistentInfo->getElapsedTime());

   uint64_t crtTime = j9time_current_time_millis() - persistentInfo->getElapsedTime();
   persistentInfo->setStartTime(crtTime);

   if (TR::Options::isAnyVerboseOptionSet())
      TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "Reset start and elapsed time: startTime=%6u, elapsedTime=%6u",
                                       (uint32_t)persistentInfo->getStartTime(), (uint32_t)persistentInfo->getElapsedTime());
   }

void
TR::CRRuntime::prepareForCheckpoint()
   {
   J9JavaVM   *vm       = getJITConfig()->javaVM;
   J9VMThread *vmThread = vm->internalVMFunctions->currentVMThread(vm);

   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCheckpointRestore))
      TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "Preparing for checkpoint");

   {
   ReleaseVMAccessAndAcquireMonitor suspendCompThreadsForCheckpointCriticalSection(vmThread, getCompilationMonitor());

   if (TR::Options::_sleepMsBeforeCheckpoint)
      {
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCheckpointRestore))
         TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "Sleeping for %d ms", TR::Options::_sleepMsBeforeCheckpoint);

      releaseCompMonitor();
      j9thread_sleep(static_cast<int64_t>(TR::Options::_sleepMsBeforeCheckpoint));
      acquireCompMonitor();
      }

   // Check if the checkpoint is interrupted
   if (shouldCheckpointBeInterrupted())
      return;

   TR_ASSERT_FATAL(!isCheckpointInProgress(), "Checkpoint already in progress!\n");

   // Compile methods for checkpoint
   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableCompilationBeforeCheckpoint))
      if (!compileMethodsForCheckpoint(vmThread))
         return;

   // Suspend JIT threads for checkpoint
   if (!suspendJITThreadsForCheckpoint(vmThread))
      return;

#if defined(J9VM_OPT_JITSERVER)
   // If this is a JITServer client that has an SSL context, free that context now
   if (getCompInfo()->getPersistentInfo()->getRemoteCompilationMode() == JITServer::CLIENT)
      {
      if (JITServer::CommunicationStream::useSSL())
         {
         getCompInfo()->freeClientSslCertificates();
         JITServer::ClientStream::freeSSLContext();
         }
      }
#endif

   setReadyForCheckpointRestore();
   }

   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCheckpointRestore))
      TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "Ready for checkpoint");
   }

void
TR::CRRuntime::prepareForRestore()
   {
   J9JavaVM   *vm       = getJITConfig()->javaVM;
   J9VMThread *vmThread = vm->internalVMFunctions->currentVMThread(vm);

   PORT_ACCESS_FROM_JAVAVM(vm);
   OMRPORT_ACCESS_FROM_J9PORT(PORTLIB);

   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCheckpointRestore))
      TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "Preparing for restore");

   // Process the post-restore options
   J9::OptionsPostRestore::processOptionsPostRestore(vmThread, getJITConfig(), getCompInfo());

   {
   OMR::CriticalSection resumeCompThreadsCriticalSection(getCompilationMonitor());

   TR_ASSERT_FATAL(readyForCheckpointRestore(), "Not ready for Checkpoint Restore\n");

   // Reset the checkpoint in progress flag.
   resetCheckpointInProgress();

   // Reset the start time.
   resetStartTime();

   // Resume JIT threads.
   resumeJITThreadsForRestore(vmThread);
   }

   // Check if there is no swap memory post restore
   J9MemoryInfo memInfo;
   if ((omrsysinfo_get_memory_info(&memInfo) == 0) && (0 == memInfo.totalSwap))
      {
      getCompInfo()->setIsSwapMemoryDisabled(true);
      }
   else
      {
      getCompInfo()->setIsSwapMemoryDisabled(false);
      }
   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCodeCache))
      {
      TR_VerboseLog::writeLineLocked(TR_Vlog_CODECACHE,
                                     "At Checkpoint Restore:: Swap Memory is %s",
                                     getCompInfo()->isSwapMemoryDisabled()? "disabled":"enabled");
      }

   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCheckpointRestore))
      TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "Ready for restore");
   }

void
TR::CRRuntime::process()
   {
   acquireCRRuntimeMonitor();
   do
      {
      while (getCRRuntimeThreadLifetimeState() == TR_CRRuntimeThreadLifetimeStates::CR_THR_INITIALIZED)
         {
         waitOnCRRuntimeMonitor();
         }

      if (getCRRuntimeThreadLifetimeState() == TR_CRRuntimeThreadLifetimeStates::CR_THR_STOPPING)
         {
         releaseCRRuntimeMonitor();
         break;
         }
      }
   while (true);
   }

static int32_t J9THREAD_PROC crRuntimeThreadProc(void * entryarg)
   {
   J9JITConfig *jitConfig = (J9JITConfig *) entryarg;
   J9JavaVM *vm = jitConfig->javaVM;
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get(jitConfig);
   TR::CRRuntime *crRuntime = compInfo->getCRRuntime();
   J9VMThread *crRuntimeThread = NULL;

   PORT_ACCESS_FROM_JITCONFIG(jitConfig);

   int rc = vm->internalVMFunctions->internalAttachCurrentThread(vm, &crRuntimeThread, NULL,
                                  J9_PRIVATE_FLAGS_DAEMON_THREAD | J9_PRIVATE_FLAGS_NO_OBJECT |
                                  J9_PRIVATE_FLAGS_SYSTEM_THREAD | J9_PRIVATE_FLAGS_ATTACHED_THREAD,
                                  crRuntime->getCRRuntimeOSThread());

   crRuntime->getCRRuntimeMonitor()->enter();
   if (rc == JNI_OK)
      {
      crRuntime->setCRRuntimeThread(crRuntimeThread);
      j9thread_set_name(j9thread_self(), "CR Runtime");
      crRuntime->setCRRuntimeThreadLifetimeState(TR::CRRuntime::CR_THR_INITIALIZED);
      }
   else
      {
      crRuntime->setCRRuntimeThreadLifetimeState(TR::CRRuntime::CR_THR_FAILED_TO_ATTACH);
      }
   crRuntime->getCRRuntimeMonitor()->notifyAll();
   crRuntime->getCRRuntimeMonitor()->exit();

   // attaching the CR Runtime Thread failed
   if (rc != JNI_OK)
      return JNI_ERR;

   crRuntime->process();

   vm->internalVMFunctions->DetachCurrentThread((JavaVM *) vm);
   crRuntime->setCRRuntimeThread(NULL);
   crRuntime->getCRRuntimeMonitor()->enter();
   crRuntime->setCRRuntimeThreadLifetimeState(TR::CRRuntime::CR_THR_DESTROYED);
   crRuntime->getCRRuntimeMonitor()->notifyAll();
   j9thread_exit((J9ThreadMonitor*)crRuntime->getCRRuntimeMonitor()->getVMMonitor());

   return 0;
   }

void
TR::CRRuntime::startCRRuntimeThread(J9JavaVM *javaVM)
   {
   PORT_ACCESS_FROM_JAVAVM(javaVM);

   //256KB stack size
   const UDATA defaultOSStackSize = javaVM->defaultOSStackSize;
   UDATA priority = J9THREAD_PRIORITY_NORMAL;

   // create the CR Runtime Thread
   if (javaVM->internalVMFunctions->createThreadWithCategory(&_crRuntimeOSThread,
                                                            defaultOSStackSize,
                                                            priority,
                                                            0,
                                                            &crRuntimeThreadProc,
                                                            javaVM->jitConfig,
                                                            J9THREAD_CATEGORY_SYSTEM_JIT_THREAD))
      {
      j9tty_printf(PORTLIB, "Error: Unable to create CR Runtime Thread\n");
      }
   else
      {
      // Must wait here until the thread gets created; otherwise an early
      // shutdown does not know whether or not to destroy the thread
      getCRRuntimeMonitor()->enter();
      while (getCRRuntimeThreadLifetimeState() == TR_CRRuntimeThreadLifetimeStates::CR_THR_NOT_CREATED)
         getCRRuntimeMonitor()->wait();
      getCRRuntimeMonitor()->exit();

      // At this point the CR Runtime thread should have attached successfully
      // and changed the state to CR_THR_INITIALIZED, or failed to attach and
      // changed the state to CR_THR_FAILED_TO_ATTACH
      if (getCRRuntimeThreadLifetimeState() == TR_CRRuntimeThreadLifetimeStates::CR_THR_FAILED_TO_ATTACH)
         {
         _crRuntimeThread = NULL;
         j9tty_printf(PORTLIB, "Error: Unable to attach CR Runtime Thread\n");
         }
      }
   }

void
TR::CRRuntime::stopCRRuntimeThread()
   {
   PORT_ACCESS_FROM_JITCONFIG(_jitConfig);

   getCRRuntimeMonitor()->enter();
   if (!getCRRuntimeThread()) // We could not create the CR Runtime Thread
      {
      getCRRuntimeMonitor()->exit();
      return;
      }

   setCRRuntimeThreadLifetimeState(TR_CRRuntimeThreadLifetimeStates::CR_THR_STOPPING);
   while (getCRRuntimeThreadLifetimeState() == TR_CRRuntimeThreadLifetimeStates::CR_THR_STOPPING)
      {
      getCRRuntimeMonitor()->notifyAll();
      getCRRuntimeMonitor()->wait();
      }

   getCRRuntimeMonitor()->exit();
   }
