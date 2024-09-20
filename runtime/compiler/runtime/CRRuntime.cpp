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

#include "control/CompilationController.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/CompilationStrategy.hpp"
#include "control/CompilationThread.hpp"
#include "control/CompileBeforeCheckpoint.hpp"
#include "control/Options.hpp"
#include "control/OptionsPostRestore.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/J9SegmentAllocator.hpp"
#include "env/RawAllocator.hpp"
#include "env/SystemSegmentProvider.hpp"
#include "env/VerboseLog.hpp"
#include "infra/Assert.hpp"
#include "infra/CriticalSection.hpp"
#include "infra/Monitor.hpp"
#include "runtime/CRRuntime.hpp"
#include "runtime/IProfiler.hpp"
#include "runtime/J9VMAccess.hpp"

#if defined(J9VM_OPT_JITSERVER)
#include "net/ClientStream.hpp"
#endif

template void TR::CRRuntime::removeMethodsFromMemoizedCompilations<J9Class>(J9Class *entryToRemove);
template void TR::CRRuntime::removeMethodsFromMemoizedCompilations<J9Method>(J9Method *entryToRemove);

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
   _checkpointStatus(TR_CheckpointStatus::NO_CHECKPOINT_IN_PROGRESS),
   _failedComps(),
   _forcedRecomps(),
   _impMethodForCR(),
   _jniMethodAddr(),
   _proactiveCompEnv(),
   _vmMethodTraceEnabled(false),
   _vmExceptionEventsHooked(false),
   _fsdEnabled(false),
   _restoreTime(0)
   {
#if defined(J9VM_OPT_JITSERVER)
   _canPerformRemoteCompilationInCRIUMode = false;
   _remoteCompilationRequestedAtBootstrap = false;
   _remoteCompilationExplicitlyDisabledAtBootstrap = false;
#endif
   }

void
TR::CRRuntime::cacheEventsStatus()
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

   _fsdEnabled = J9::Options::_fsdInitStatus == J9::Options::FSDInit_Initialized;
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

template<typename T>
void
TR::CRRuntime::pushMemoizedCompilation(TR_MemoizedCompilations& list, J9Method *method, void *data)
   {
   auto newEntry = new (_compInfo->persistentMemory()) T(method, data);
   if (newEntry)
      list.add(newEntry);
   }

J9Method *
TR::CRRuntime::popMemoizedCompilation(TR_MemoizedCompilations& list, void **data)
   {
   J9Method *method = NULL;
   auto memComp = list.pop();
   if (memComp)
      {
      method = memComp->getMethod();
      if (data)
         *data = memComp->getData();
      jitPersistentFree(memComp);
      }
   return method;
   }

static bool shouldRemoveMemoizedCompilation(J9Method *j9methodInList, J9Class *j9class)
   {
   return J9_CLASS_FROM_METHOD(j9methodInList) == j9class;
   }

static bool shouldRemoveMemoizedCompilation(J9Method *j9methodInList, J9Method *j9method)
   {
   return j9methodInList == j9method;
   }

template<typename T>
void
TR::CRRuntime::removeMemoizedCompilation(TR_MemoizedCompilations& list, T *entryToRemove)
   {
   if (!list.isEmpty())
      {
      // Remove from front of list
      auto curr = list.getFirst();
      while (curr)
         {
         J9Method *j9methodInList = curr->getMethod();

         if (shouldRemoveMemoizedCompilation(j9methodInList, entryToRemove))
            jitPersistentFree(list.pop());
         else
            break;

         curr = list.getFirst();
         }

      // Remove from middle of list
      auto prev = curr;
      curr = prev ? prev->getNext() : NULL;
      while (curr)
         {
         J9Method *j9methodInList = curr->getMethod();

         if (shouldRemoveMemoizedCompilation(j9methodInList, entryToRemove))
            {
            list.removeAfter(prev, curr);
            jitPersistentFree(curr);
            }
         else
            {
            prev = curr;
            }

         curr = prev->getNext();
         }
      }
   }

template<typename T>
void
TR::CRRuntime::removeMethodsFromMemoizedCompilations(T *entryToRemove)
   {
   OMR::CriticalSection removeMemoizedCompilations(getCRRuntimeMonitor());
   removeMemoizedCompilation<T>(_failedComps, entryToRemove);
   removeMemoizedCompilation<T>(_forcedRecomps, entryToRemove);
   removeMemoizedCompilation<T>(_impMethodForCR, entryToRemove);
   }

void
TR::CRRuntime::purgeMemoizedCompilation(TR_MemoizedCompilations& list)
   {
   while (!list.isEmpty())
      {
      jitPersistentFree(list.pop());
      }
   }

void
TR::CRRuntime::purgeMemoizedCompilations()
   {
   OMR::CriticalSection removeMemoizedCompilations(getCRRuntimeMonitor());
   purgeMemoizedCompilation(_failedComps);
   purgeMemoizedCompilation(_forcedRecomps);
   purgeMemoizedCompilation(_impMethodForCR);
   }

void
TR::CRRuntime::pushFailedCompilation(J9Method *method)
   {
   pushMemoizedCompilation<TR_MemoizedComp>(_failedComps, method);
   }

void
TR::CRRuntime::pushForcedRecompilation(J9Method *method)
   {
   pushMemoizedCompilation<TR_MemoizedComp>(_forcedRecomps, method);
   }

void
TR::CRRuntime::pushImportantMethodForCR(J9Method *method)
   {
   pushMemoizedCompilation<TR_MemoizedComp>(_impMethodForCR, method);
   }

void
TR::CRRuntime::pushJNIAddr(J9Method *method, void *addr)
   {
   pushMemoizedCompilation<TR_JNIMethodAddr>(_jniMethodAddr, method, addr);
   }

void
TR::CRRuntime::resetJNIAddr()
   {
   OMR::CriticalSection resetJNI(getCRRuntimeMonitor());
   while (!_jniMethodAddr.isEmpty())
      {
      J9Method *method;
      void *addr;
      while ((method = popJNIAddr(&addr)))
         {
         TR_ASSERT_FATAL(addr, "JNI Address to be reset cannot be NULL!");
         _compInfo->setJ9MethodExtra(method, reinterpret_cast<intptr_t>(addr));
         }
      }
   }

void
TR::CRRuntime::triggerRecompilationForPreCheckpointGeneratedFSDBodies(J9VMThread *vmThread)
   {
   TR_J9VMBase *fe = TR_J9VMBase::get(getJITConfig(), vmThread);

   J9Method *method;
   while ((method = popForcedRecompilation()))
      {
      if (_compInfo->isCompiled(method))
         {
         if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCheckpointRestoreDetails))
            TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "%p Attempting to force %p for recompilation", vmThread, method);

         TR_MethodEvent event;
         event._eventType = TR_MethodEvent::ForcedRecompilationPostRestore;
         event._j9method = method;
         event._oldStartPC = method->extra;
         event._vmThread = vmThread;
         event._classNeedingThunk = 0;
         bool newPlanCreated = false;
         TR_OptimizationPlan *plan = TR::CompilationController::getCompilationStrategy()->processEvent(&event, &newPlanCreated);
         // the plan needs to be created only when async compilation is possible
         // Otherwise the compilation will be triggered on next invocation
         if (plan)
            {
            TR_PersistentJittedBodyInfo *bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(method->extra);
            TR_PersistentMethodInfo *methodInfo = bodyInfo->getMethodInfo();
            methodInfo->setReasonForRecompilation(TR_PersistentMethodInfo::RecompDueToCRIU);

            bool queued = false;

            // Release the CR Runtime Monitor here as induceRecompilation will
            // eventually acquire the Comp Monitor.
            releaseCRRuntimeMonitor();

            // Induce the recompilation
            TR::Recompilation::induceRecompilation(fe, method->extra, &queued, plan);

            // Re-acquire the CR Runtime Monitor now that this thread does not
            // have the Comp Monitor in hand.
            acquireCRRuntimeMonitor();

            if (!queued && newPlanCreated)
               TR_OptimizationPlan::freeOptimizationPlan(plan);
            }
         }
      }
   }

void
TR::CRRuntime::setupEnvForProactiveCompilation(J9JavaVM *javaVM, J9VMThread *vmThread, TR_J9VMBase *fej9)
   {
   /* Proactive compilation should not be FSD compiles */
   if (javaVM->internalVMFunctions->isDebugOnRestoreEnabled(javaVM))
      {
      TR::Options::getCmdLineOptions()->setFSDOptionsForAll(false);
      TR::Options::getAOTCmdLineOptions()->setFSDOptionsForAll(false);
#if defined(J9VM_OPT_JITSERVER)
      _proactiveCompEnv._canDoRemoteComp = _compInfo->getCRRuntime()->canPerformRemoteCompilationInCRIUMode();
      _proactiveCompEnv._clientUID = _compInfo->getPersistentInfo()->getClientUID();
      _proactiveCompEnv._serverUID = _compInfo->getPersistentInfo()->getServerUID();
      _proactiveCompEnv._remoteCompMode = J9::PersistentInfo::_remoteCompilationMode;

      setCanPerformRemoteCompilationInCRIUMode(false);
      getCompInfo()->getPersistentInfo()->setClientUID(0);
      getCompInfo()->getPersistentInfo()->setServerUID(0);
      fej9->getJ9JITConfig()->clientUID = 0;
      fej9->getJ9JITConfig()->serverUID = 0;
      J9::PersistentInfo::_remoteCompilationMode = JITServer::NONE;
#endif // if defined(J9VM_OPT_JITSERVER)
      }
   }

void
TR::CRRuntime::teardownEnvForProactiveCompilation(J9JavaVM *javaVM, J9VMThread *vmThread, TR_J9VMBase *fej9)
   {
   /* Proactive compilation should not be FSD compiles */
   if (javaVM->internalVMFunctions->isDebugOnRestoreEnabled(javaVM))
      {
      TR::Options::getCmdLineOptions()->setFSDOptionsForAll(true);
      TR::Options::getAOTCmdLineOptions()->setFSDOptionsForAll(true);
#if defined(J9VM_OPT_JITSERVER)
      setCanPerformRemoteCompilationInCRIUMode(_proactiveCompEnv._canDoRemoteComp);
      getCompInfo()->getPersistentInfo()->setClientUID(_proactiveCompEnv._clientUID);
      getCompInfo()->getPersistentInfo()->setServerUID(_proactiveCompEnv._serverUID);
      fej9->getJ9JITConfig()->clientUID = _proactiveCompEnv._clientUID;
      fej9->getJ9JITConfig()->serverUID = _proactiveCompEnv._serverUID;
      J9::PersistentInfo::_remoteCompilationMode = _proactiveCompEnv._remoteCompMode;
#endif // if defined(J9VM_OPT_JITSERVER)
      }
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

   /* Wait until existing compilations are done before modifying the env */
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

      {
      J9JavaVM *javaVM = getJITConfig()->javaVM;
      TR_J9VMBase *fej9 = TR_J9VMBase::get(getJITConfig(), vmThread);

      SetupEnvForProactiveComp setupProactiveCompEnv(this, javaVM, vmThread, fej9);

      /* Queue compilation of interpreted methods */
      try
         {
         TR::RawAllocator rawAllocator(javaVM);
         J9::SegmentAllocator segmentAllocator(MEMORY_TYPE_JIT_SCRATCH_SPACE | MEMORY_TYPE_VIRTUAL, *javaVM);
         J9::SystemSegmentProvider regionSegmentProvider(
            1 << 20,
            1 << 20,
            TR::Options::getScratchSpaceLimit(),
            segmentAllocator,
            rawAllocator
            );
         TR::Region compForCheckpointRegion(regionSegmentProvider, rawAllocator);

         TR::CompileBeforeCheckpoint compileBeforeCheckpoint(compForCheckpointRegion, vmThread, fej9, getCompInfo());
         compileBeforeCheckpoint.compileMethodsBeforeCheckpoint();
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
   // Allow heuristics to turn on the IProfiler
   if (_jitConfig->javaVM->internalVMFunctions->isDebugOnRestoreEnabled(_jitConfig->javaVM)
       && !_jitConfig->javaVM->internalVMFunctions->isCheckpointAllowed(_jitConfig->javaVM))
      {
      turnOffInterpreterProfiling(_jitConfig);
      TR::Options::getCmdLineOptions()->setOption(TR_NoIProfilerDuringStartupPhase);
      }

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
   PORT_ACCESS_FROM_JAVAVM(getJITConfig()->javaVM);
   TR::PersistentInfo *persistentInfo = getCompInfo()->getPersistentInfo();

   if (TR::Options::isAnyVerboseOptionSet())
      TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "Start and elapsed time: startTime=%6u, elapsedTime=%6u",
                                       (uint32_t)persistentInfo->getStartTime(), (uint32_t)persistentInfo->getElapsedTime());

   uint64_t crtTime = j9time_current_time_millis() - persistentInfo->getElapsedTime();
   persistentInfo->setStartTime(crtTime);

   if (TR::Options::isAnyVerboseOptionSet())
      TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "Reset start and elapsed time: startTime=%6u, elapsedTime=%6u",
                                       (uint32_t)persistentInfo->getStartTime(), (uint32_t)persistentInfo->getElapsedTime());

   _restoreTime = persistentInfo->getElapsedTime();
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

   // Make sure the limit for the ghost files is at least as big as the data cache size
   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableDataCacheDisclaiming) ||
       TR::Options::getCmdLineOptions()->getOption(TR_EnableCodeCacheDisclaiming))
      {
      U_32 ghostFileLimit = std::max(vm->jitConfig->dataCacheKB, vm->jitConfig->codeCacheTotalKB) * 1024; // convert to bytes
      vm->internalVMFunctions->setRequiredGhostFileLimit(vmThread, ghostFileLimit);
      }

   setReadyForCheckpointRestore();

   char * printPersistentMem = feGetEnv("TR_PrintPersistentMem");
   if (printPersistentMem)
      {
      if (trPersistentMemory)
         trPersistentMemory->printMemStats();
      }

   printIprofilerStats(TR::Options::getCmdLineOptions(),
                       _jitConfig,
                       TR_J9VMBase::get(_jitConfig, NULL)->getIProfiler(),
                       "Checkpoint");
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
TR::CRRuntime::recompileMethodsCompiledPreCheckpoint()
   {
   if (!getCRRuntimeThread())
      return;

   OMR::CriticalSection recompFSDBodies(getCRRuntimeMonitor());
   if (getCRRuntimeThreadLifetimeState() == TR_CRRuntimeThreadLifetimeStates::CR_THR_INITIALIZED)
      {
      setCRRuntimeThreadLifetimeState(TR_CRRuntimeThreadLifetimeStates::CR_THR_TRIGGER_RECOMP);
      getCRRuntimeMonitor()->notifyAll();
      }
   }

bool
TR::CRRuntime::allowStateChange()
   {
   TR::PersistentInfo *persistentInfo = getCompInfo()->getPersistentInfo();
   return (_restoreTime != 0) && ((persistentInfo->getElapsedTime() - _restoreTime) > TR::Options::_delayBeforeStateChange);
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

      auto state = getCRRuntimeThreadLifetimeState();
      if (state == TR_CRRuntimeThreadLifetimeStates::CR_THR_STOPPING)
         {
         releaseCRRuntimeMonitor();
         break;
         }
      else if (state == TR_CRRuntimeThreadLifetimeStates::CR_THR_TRIGGER_RECOMP)
         {
         triggerRecompilationForPreCheckpointGeneratedFSDBodies(getCRRuntimeThread());

         // Because the CR Runtime Monitor may have been released, only reset
         // the state if the current state is still CR_THR_TRIGGER_RECOMP
         if (getCRRuntimeThreadLifetimeState() == TR_CRRuntimeThreadLifetimeStates::CR_THR_TRIGGER_RECOMP)
            setCRRuntimeThreadLifetimeState(TR_CRRuntimeThreadLifetimeStates::CR_THR_INITIALIZED);
         }
      else
         {
         TR_ASSERT_FATAL(false, "Invalid state %d\n", state);
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
