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

#include "j9cfg.h"

#include "env/TRMemory.hpp"

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

   private:

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

   J9JITConfig *_jitConfig;
   TR::CompilationInfo *_compInfo;

   TR::Monitor *_compMonitor;
   TR::Monitor *_crMonitor;

   TR_CheckpointStatus _checkpointStatus;

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
