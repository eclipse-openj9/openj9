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
#ifndef CR_RUNTIME_HPP
#define CR_RUNTIME_HPP

#include "j9.h"
#include "env/TRMemory.hpp"
#include "infra/Link.hpp"

extern "C" {
struct J9JITConfig;
struct J9VMThread;
struct J9Method;
}

namespace TR { class Monitor; }
namespace TR { class CompilationInfo; }

namespace TR
{
class CRRuntime
   {
   public:

   TR_PERSISTENT_ALLOC(TR_MemoryBase::CompilationInfo);

   enum TR_CheckpointStatus
      {
      NO_CHECKPOINT_IN_PROGRESS,
      COMPILE_METHODS_FOR_CHECKPOINT,
      SUSPEND_THREADS_FOR_CHECKPOINT,
      INTERRUPT_CHECKPOINT,
      READY_FOR_CHECKPOINT_RESTORE
      };

   enum TR_CRRuntimeThreadLifetimeStates
      {
      CR_THR_NOT_CREATED = 0,
      CR_THR_FAILED_TO_ATTACH,
      CR_THR_INITIALIZED,
      CR_THR_TRIGGER_RECOMP,
      CR_THR_STOPPING,
      CR_THR_DESTROYED,
      CR_THR_LAST_STATE // must be the last one
      };

   CRRuntime(J9JITConfig *jitConfig, TR::CompilationInfo *compInfo);

   /* The CR Monitor (Checkpoint/Restore Monitor) must always be acquired with
    * Comp Monitor in hand. If waiting on the CR Monitor, the Comp Monitor
    * should be released. After being notified, the CR Monitor should be
    * released before re-acquiring the Comp Monitor.
    */
   TR::Monitor *getCRMonitor() { return _crMonitor; }
   void acquireCRMonitor();
   void releaseCRMonitor();
   void waitOnCRMonitor();

   /* The CR Runtime Monitor can be acquired with the Comp Monitor in hand.
    * However, if a does not already have the Comp Monitor, it should NOT
    * do so it once it acquires the CR Runtime Monitor. If the Comp Monitor is
    * desired, the CR Runtime Monitor should be released and re-acquired AFTER
    * acquring the Comp Monitor.
    */
   TR::Monitor* getCRRuntimeMonitor() { return _crRuntimeMonitor;  }
   void acquireCRRuntimeMonitor();
   void releaseCRRuntimeMonitor();
   void waitOnCRRuntimeMonitor();

   /* The following APIs should only be invoked with the Comp Monitor in hand. */
   bool isCheckpointInProgress()            { return _checkpointStatus != TR_CheckpointStatus::NO_CHECKPOINT_IN_PROGRESS;      }
   void resetCheckpointInProgress()         {        _checkpointStatus  = TR_CheckpointStatus::NO_CHECKPOINT_IN_PROGRESS;      }
   bool shouldCompileMethodsForCheckpoint() { return _checkpointStatus == TR_CheckpointStatus::COMPILE_METHODS_FOR_CHECKPOINT; }
   void setCompileMethodsForCheckpoint()    {        _checkpointStatus  = TR_CheckpointStatus::COMPILE_METHODS_FOR_CHECKPOINT; }
   bool shouldSuspendThreadsForCheckpoint() { return _checkpointStatus == TR_CheckpointStatus::SUSPEND_THREADS_FOR_CHECKPOINT; }
   void setSuspendThreadsForCheckpoint()    {        _checkpointStatus  = TR_CheckpointStatus::SUSPEND_THREADS_FOR_CHECKPOINT; }
   bool readyForCheckpointRestore()         { return _checkpointStatus == TR_CheckpointStatus::READY_FOR_CHECKPOINT_RESTORE;   }
   void setReadyForCheckpointRestore()      {        _checkpointStatus  = TR_CheckpointStatus::READY_FOR_CHECKPOINT_RESTORE;   }
   bool shouldCheckpointBeInterrupted()     { return _checkpointStatus == TR_CheckpointStatus::INTERRUPT_CHECKPOINT;           }
   void interruptCheckpoint()               {        _checkpointStatus  = TR_CheckpointStatus::INTERRUPT_CHECKPOINT;           }

   void setVMMethodTraceEnabled(bool trace) { _vmMethodTraceEnabled = trace; }
   bool isVMMethodTraceEnabled()            { return _vmMethodTraceEnabled;  }

   void setVMExceptionEventsHooked(bool trace) { _vmExceptionEventsHooked = trace; }
   bool isVMExceptionEventsHooked()            { return _vmExceptionEventsHooked;  }

#if defined(J9VM_OPT_JITSERVER)
   bool canPerformRemoteCompilationInCRIUMode()                   { return _canPerformRemoteCompilationInCRIUMode;       }
   void setCanPerformRemoteCompilationInCRIUMode(bool remoteComp) { _canPerformRemoteCompilationInCRIUMode = remoteComp; }

   bool remoteCompilationRequestedAtBootstrap()                   { return _remoteCompilationRequestedAtBootstrap;       }
   void setRemoteCompilationRequestedAtBootstrap(bool remoteComp) { _remoteCompilationRequestedAtBootstrap = remoteComp; }

   bool remoteCompilationExplicitlyDisabledAtBootstrap()                   { return _remoteCompilationExplicitlyDisabledAtBootstrap;       }
   void setRemoteCompilationExplicitlyDisabledAtBootstrap(bool remoteComp) { _remoteCompilationExplicitlyDisabledAtBootstrap = remoteComp; }
#endif

   TR_CRRuntimeThreadLifetimeStates getCRRuntimeThreadLifetimeState()           { return _crRuntimeThreadLifetimeState;  }
   void setCRRuntimeThreadLifetimeState(TR_CRRuntimeThreadLifetimeStates state) { _crRuntimeThreadLifetimeState = state; }

   j9thread_t getCRRuntimeOSThread()             { return _crRuntimeOSThread;   }
   J9VMThread *getCRRuntimeThread()              { return _crRuntimeThread;     }
   void setCRRuntimeThread(J9VMThread *vmThread) { _crRuntimeThread = vmThread; }
   void startCRRuntimeThread(J9JavaVM *javaVM);
   void stopCRRuntimeThread();

   /* The following methods should be only be invoked with the CR Runtime
    * Monitor in hand.
    */
   void pushFailedCompilation(J9Method *method)    { pushMemoizedCompilation(_failedComps, method);   }
   J9Method * popFailedCompilation()               { return popMemoizedCompilation(_failedComps);     }
   void pushForcedRecompilation(J9Method *method)  { pushMemoizedCompilation(_forcedRecomps, method); }
   J9Method * popForcedRecompilation()             { return popMemoizedCompilation(_forcedRecomps);   }

   /**
    * @brief Remove appropriate methods from all of the memoized lists. This
    *        method acquires the CR Runtime Monitor.
    *
    * @param entryToRemove The entry that determines what should be removed from
    *                      the various memoized lists.
    */
   template <typename T>
   void removeMethodsFromMemoizedCompilations(T *entryToRemove);

   /**
    * @brief Empty the various memoized lists. This method acquires the CR
    *        Runtime Monitor.
    */
   void purgeMemoizedCompilations();

   /**
    * @brief Processing method that the CR Runtime Thread executes.
    */
   void process();

   /**
   * @brief Work that is necessary prior to taking a snapshot. This includes:
   *        - Setting the _checkpointStatus state.
   *        - Suspending all compiler threads.
   *        - Waiting until all compiler threads are suspended.
   */
   void prepareForCheckpoint();

   /**
   * @brief Work that is necessary after the JVM has been restored. This
   *        includes:
   *        - Resetting the _checkpointStatus state.
   *        - Processing post-restore options.
   *        - Resuming all suspended compiler threads.
   */
   void prepareForRestore();

   /**
    * @brief Notify the CR Runtime Thread to recompile methods that were
    *        memoized pre-checkpoint.
    */
   void recompileMethodsCompiledPreCheckpoint();

   private:

   class TR_MemoizedComp : public TR_Link0<TR_MemoizedComp>
      {
      public:
      TR_PERSISTENT_ALLOC(TR_MemoryBase::CompilationInfo);

      TR_MemoizedComp(J9Method *method) { _method = method; }
      J9Method *getMethod() { return _method; }

      private:
      J9Method *_method;
      };
   typedef TR_LinkHead0<TR_MemoizedComp> TR_MemoizedCompilations;

   /* These are private since this monitor should only be externally accessed
    * via the TR::CompilationInfo object.
    */
   TR::Monitor *getCompilationMonitor() { return _compMonitor; }
   void acquireCompMonitor();
   void releaseCompMonitor();

   TR::CompilationInfo *getCompInfo() { return _compInfo;  }
   J9JITConfig *getJITConfig()        { return _jitConfig; }

   /**
    * @brief Release the comp monitor in order to wait on the CR Monitor;
    *        release the CR Monitor and reacquire the comp monitor once
    *        notified.
    *
    * IMPORTANT: There should be no return, or C++ exception thrown, after
    *            releasing the Comp Monitor until it is re-acquired.
    *            The Comp Monitor may be acquired in an OMR::CriticalSection
    *            object; a return, or thrown exception, will destruct this
    *            object as part leaving the scope, or stack unwinding, which
    *            will attempt to release the monitor in its destructor.
    */
   void releaseCompMonitorUntilNotifiedOnCRMonitor() throw();

   /**
    * @brief Compile methods for checkpoint.
    *
    * IMPORTANT: Must be called with the comp monitor in hand.
    *
    * @param vmThread The J9VMThread
    *
    * @return false if the checkpoint is interrupted, true otherwise.
    *
    */
   bool compileMethodsForCheckpoint(J9VMThread *vmThread);

   /**
    * @brief Suspend compilation threads for checkpoint.
    *
    * IMPORTANT: Must be called with the comp monitor in hand.
    *
    * @param vmThread The J9VMThread
    *
    * @return false false if the checkpoint is interrupted, true otherwise.
    */
   bool suspendCompThreadsForCheckpoint(J9VMThread *vmThread);

   /**
    * @brief Suspend all JIT threads such as
    *        * Compilation Threads
    *        * Sampler Thread
    *
    * @param vmThread The J9VMThread
    *
    * @return false if the checkpoint is interrupted, true otherwise.
    */
   bool suspendJITThreadsForCheckpoint(J9VMThread *vmThread);

   /**
    * @brief Resume all JIT threads suspended by
    *        suspendCompilerThreadsForCheckpoint
    *
    * @param vmThread The J9VMThread
    */
   void resumeJITThreadsForRestore(J9VMThread *vmThread);

   /**
    * @brief Reset Start Time post retore
    */
   void resetStartTime();

   /**
    * @brief Helper method to push a J9Method onto the front of list that is
    *        used to memoize a future compilation of said J9Method. This method
    *        should only be called with the Comp Monitor in hand.
    *
    * @param list A reference to the TR_MemoizedCompilations list
    * @param method The method whose compilation should be memoized
    */
   void pushMemoizedCompilation(TR_MemoizedCompilations& list, J9Method *method);

   /**
    * @brief Helper method to pop a J9Method from a list that is used to memoize
    *        a future compilation of said J9Method; this action will remove the
    *        J9Method from the list. This method should only be called with the
    *        Comp Monitor in hand.
    *
    * @param list A reference to the TR_MemoizedCompilations list
    *
    * @return the J9Method at the front of the list; NULL if the list is empty.
    */
   J9Method * popMemoizedCompilation(TR_MemoizedCompilations& list);

   /**
    * @brief Helper method to remove a J9Method from a list that is used to
    *        memoize a future compilation of said J9Method. This method should
    *        only be called with the Comp Monitor in hand.
    *
    * @param list A reference to the TR_MemoizedCompilations list
    * @param clazz The class of the methods to be removed from the list
    */
   template<typename T>
   void removeMemoizedCompilation(TR_MemoizedCompilations& list, T *entryToRemove);

   /**
    * @brief Empty the entries in the list that is passed to this method.
    *
    * @param list A reference to the TR_MemoizedCompilations list
    */
   void purgeMemoizedCompilation(TR_MemoizedCompilations& list);

   /**
    * @brief Trigger compilation of any compilations that failed pre-checkpoint;
    *        specifically, any compilations added to the _failedComps list.
    *
    *        This method is called with the CR Runtime Monitor in hand.
    *
    * @param vmThread the J9VMThread
    */
   void triggerCompilationOfFailedCompilationsPreCheckpoint(J9VMThread *vmThread);

   /**
    * @brief Trigger recompilation of the compilations that were generated with
    *        FSD enabled pre-checkpoint; specifically, any compilations added to
    *        the _forcedRecomps list.
    *
    *        This method is called with the CR Runtime Monitor in hand.
    *
    * @param vmThread the J9VMThread
    */
   void triggerRecompilationForPreCheckpointGeneratedFSDBodies(J9VMThread *vmThread);

   J9JITConfig *_jitConfig;
   TR::CompilationInfo *_compInfo;

   TR::Monitor *_compMonitor;
   TR::Monitor *_crMonitor;
   TR::Monitor *_crRuntimeMonitor;

   J9VMThread *_crRuntimeThread;
   j9thread_t _crRuntimeOSThread;

   TR_CRRuntimeThreadLifetimeStates _crRuntimeThreadLifetimeState;
   TR_CheckpointStatus _checkpointStatus;

   TR_MemoizedCompilations _failedComps;
   TR_MemoizedCompilations _forcedRecomps;

   bool _vmMethodTraceEnabled;
   bool _vmExceptionEventsHooked;

#if defined(J9VM_OPT_JITSERVER)
   bool _canPerformRemoteCompilationInCRIUMode;
   bool _remoteCompilationRequestedAtBootstrap;
   bool _remoteCompilationExplicitlyDisabledAtBootstrap;
#endif
   };

} // namespace TR

#endif // CR_RUNTIME_HPP
