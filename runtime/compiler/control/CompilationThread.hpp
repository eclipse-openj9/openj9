/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef COMPILATIONTHREAD_INCL
#define COMPILATIONTHREAD_INCL

#include <ctime>
#include "control/CompilationPriority.hpp"
#include "env/RawAllocator.hpp"
#include "j9.h"
#include "j9thread.h"
#include "j9cfg.h"
#include "env/VMJ9.h"
#include "control/rossa.h"
#include "env/CpuUtilization.hpp"
#include "infra/Statistics.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "runtime/DataCache.hpp"
#include "infra/Monitor.hpp"
#include "ilgen/IlGenRequest.hpp"
#include "infra/MonitorTable.hpp"
#include "infra/RWMonitor.hpp"
#include "infra/Link.hpp"
#include "env/IO.hpp"
#include "runtime/RelocationRuntime.hpp"
#include "env/J9SegmentCache.hpp"

#define METHOD_POOL_SIZE_THRESHOLD 64
#define MAX_SAMPLING_FREQUENCY     0x7FFFFFFF

// Reasons for interrupting a compilation
const uint8_t GC_COMP_INTERRUPT       = 1;
const uint8_t SHUTDOWN_COMP_INTERRUPT = 2;

const  int32_t IDLE_THRESHOLD       = 50;  // % CPU

namespace TR { class Monitor; }
namespace TR { class PersistentInfo; }
namespace TR { class SegmentAllocator; }
namespace TR { class CompilationInfoPerThreadBase; } // forward declaration
namespace TR { class CompilationInfoPerThread; }     // forward declaration
namespace TR { class CompilationInfo; }              // forward declaration
struct TR_MethodToBeCompiled;
class TR_ResolvedMethod;
class TR_RelocationRuntime;

enum CompilationThreadState
   {
   COMPTHREAD_UNINITIALIZED,     // comp thread has not yet initialized
   COMPTHREAD_ACTIVE,            // comp thread is actively processing queue entries
   COMPTHREAD_SIGNAL_WAIT,       // comp transitioning to waiting on the queue
   COMPTHREAD_WAITING,           // comp thread is waiting on the queue
   COMPTHREAD_SIGNAL_SUSPEND,    // comp thread received suspension request
   COMPTHREAD_SUSPENDED,         // comp thread is waiting on the compThreadMonitor
   COMPTHREAD_SIGNAL_TERMINATE,  // compthread will terminate
   COMPTHREAD_STOPPING,          // compthread is terminating
   COMPTHREAD_STOPPED,           // compthread has terminated
   COMPTHREAD_ABORT              // compthread failed to initialize
   };

struct CompileParameters
   {
   CompileParameters(
         TR::CompilationInfoPerThreadBase *compilationInfo,
         TR_J9VMBase* vm,
         J9VMThread * vmThread,
         TR_RelocationRuntime *reloRuntime,
         TR_OptimizationPlan * optimizationPlan,
         TR::SegmentAllocator &scratchSegmentProvider,
         TR::Region &dispatchRegion,
         TR_Memory &trMemory,
         const TR::CompileIlGenRequest &ilGenRequest
      ) :
      _compilationInfo(compilationInfo),
      _vm(vm),
      _vmThread(vmThread),
      _reloRuntime(reloRuntime),
      _optimizationPlan(optimizationPlan),
      _scratchSegmentProvider(scratchSegmentProvider),
      _dispatchRegion(dispatchRegion),
      _trMemory(trMemory),
      _ilGenRequest(ilGenRequest)
      {}

   TR_Memory *trMemory() { return &_trMemory; }

   TR::CompilationInfoPerThreadBase *_compilationInfo;
   TR_J9VMBase        *_vm;
   J9VMThread         *_vmThread;
   TR_RelocationRuntime *_reloRuntime;
   TR_OptimizationPlan*_optimizationPlan;
   TR::SegmentAllocator &_scratchSegmentProvider;
   TR::Region           &_dispatchRegion;
   TR_Memory            &_trMemory;
   TR::CompileIlGenRequest  _ilGenRequest;
   };

#if defined(TR_HOST_S390)
struct timeval;
#endif

class TR_DataCache;

namespace TR {

class CompilationInfoPerThreadBase
   {
   TR_PERSISTENT_ALLOC(TR_Memory::CompilationInfoPerThreadBase);
   friend class TR::CompilationInfo;
   friend class ::TR_DebugExt;
   public:
   CompilationInfoPerThreadBase(TR::CompilationInfo &compInfo, J9JITConfig *jitConfig, int32_t id, bool onSeparateThread);

   TR::CompilationInfo    *getCompilationInfo() { return &_compInfo; }
   J9JITConfig           *getJitConfig() { return _jitConfig; }
   TR_MethodToBeCompiled *getMethodBeingCompiled() { return _methodBeingCompiled; }
   void                   setMethodBeingCompiled(TR_MethodToBeCompiled *m) { _methodBeingCompiled = m; }

   CompilationThreadState getCompilationThreadState() {return _compilationThreadState;}
   virtual void           setCompilationThreadState(CompilationThreadState v);
   bool                   compilationThreadIsActive();
   TR::Compilation        *getCompilation() { return _compiler; }
   void                   setCompilation(TR::Compilation *compiler);
   void                   zeroCompilation();
   void                   printCompilationThreadTime();
   TR_MethodMetaData     *getMetadata() {return _metadata;}
   void                   setMetadata(TR_MethodMetaData *m) {_metadata = m;}
   void *compile(J9VMThread *context, TR_MethodToBeCompiled *entry, J9::J9SegmentProvider &scratchSegmentProvider);
   TR_MethodMetaData *compile(J9VMThread *context, TR::Compilation *,
                 TR_ResolvedMethod *compilee, TR_J9VMBase &, TR_OptimizationPlan*, TR::SegmentAllocator const &scratchSegmentProvider);

   void preCompilationTasks(J9VMThread * vmThread,
                            J9JavaVM *javaVM,
                            TR_MethodToBeCompiled *entry,
                            J9Method *method,
                            const void **aotCachedMethod,
                            TR_Memory &trMemory,
                            bool &canDoRelocatableCompile,
                            bool &eligibleForRelocatableCompile,
                            TR_RelocationRuntime *reloRuntime);

   void *postCompilationTasks(J9VMThread * vmThread,
                              TR_MethodToBeCompiled *entry,
                              J9Method *method,
                              const void *aotCachedMethod,
                              TR_MethodMetaData *metaData,
                              bool canDoRelocatableCompile,
                              bool eligibleForRelocatableCompile,
                              TR_RelocationRuntime *reloRuntime);

#if defined(J9VM_OPT_SHARED_CLASSES) && defined(J9VM_INTERP_AOT_RUNTIME_SUPPORT)
   TR_MethodMetaData *installAotCachedMethod(
                                J9VMThread *vmThread,
                                const void *aotCachedMethod,
                                J9Method *method,
                                TR_FrontEnd *fe,
                                TR::Options *options,
                                TR_ResolvedMethod *compilee,
                                TR_MethodToBeCompiled *entry,
                                TR::Compilation *compiler
                                );
#endif

   /*
    * LdTM: This should, pedantically speaking, be an 'extern "C"' friend function rather than a static member function (with C++ linkage).
    * However a brief search reveals that such an approach approach is fraught with its own set of compiler-bug-related risk.
    */
   static TR_MethodMetaData *wrappedCompile(J9PortLibrary *portLib, void * opaqueParameters);

   bool methodCanBeCompiled(TR_Memory *trMemory, TR_FrontEnd *fe, TR_ResolvedMethod *compilee, TR_FilterBST *&filter);
   int32_t                getCompThreadId() const { return _compThreadId; }

   bool                   compilationCanBeInterrupted() const { return _compilationCanBeInterrupted; }
   void                   enterInterruptibleOperation()
      {
      TR_ASSERT(!_compilationCanBeInterrupted, "This guard is not re-entrant.");
      _compilationCanBeInterrupted = true;
      }
   void                   exitInterruptibleOperation()
      {
      TR_ASSERT(_compilationCanBeInterrupted, "Exiting interruptible operation without having entered.");
      _compilationCanBeInterrupted = false;
      }

   class InterruptibleOperation
      {
   public:
      InterruptibleOperation(TR::CompilationInfoPerThreadBase &compThread) :
         _compThread(compThread)
         {
         _compThread.enterInterruptibleOperation();
         }

      ~InterruptibleOperation()
         {
         _compThread.exitInterruptibleOperation();
         }

   private:
      TR::CompilationInfoPerThreadBase & _compThread;

      };

   uint8_t                compilationShouldBeInterrupted() const { return _compilationShouldBeInterrupted; }
   void                   setCompilationShouldBeInterrupted(uint8_t reason) { _compilationShouldBeInterrupted = reason; }
   TR_DataCache*          reservedDataCache() { return _reservedDataCache; }
   void                   setReservedDataCache(TR_DataCache *dataCache) { _reservedDataCache = dataCache; }
   int32_t                getNumJITCompilations() const { return _numJITCompilations; }
   void                   incNumJITCompilations() { _numJITCompilations++; }
   int32_t                getQszWhenCompStarted() const { return _qszWhenCompStarted; }
   void                   generatePerfToolEntry(); // for Linux only
   uintptr_t              getTimeWhenCompStarted() const { return _timeWhenCompStarted; }
   void                   setTimeWhenCompStarted(UDATA t) { _timeWhenCompStarted = t; }

   TR_RelocationRuntime  *reloRuntime() { return &_reloRuntime; }
   static TR::FILE *getPerfFile() { return _perfFile; } // used on Linux for perl tool support
   static void setPerfFile(TR::FILE *f) { _perfFile = f; }

   protected:

   TR::CompilationInfo &         _compInfo;
   J9JITConfig * const          _jitConfig;
   TR_SharedCacheRelocationRuntime _reloRuntime;
   int32_t const                _compThreadId; // unique number starting from 0; Only used for compilation on separate thread
   bool const                   _onSeparateThread;

   TR_J9VMBase *                _vm;
   TR_MethodToBeCompiled *      _methodBeingCompiled;
   TR::Compilation *            _compiler;
   TR_MethodMetaData *          _metadata;
   TR_DataCache *               _reservedDataCache;
   uintptr_t                    _timeWhenCompStarted;
   int32_t                      _numJITCompilations; // num JIT compilations this thread has performed; AOT loads not counted
   int32_t                      _qszWhenCompStarted; // size of compilation queue and compilation starts
   bool                         _compilationCanBeInterrupted;
   bool                         _addToJProfilingQueue;

   volatile CompilationThreadState _compilationThreadState;
   volatile CompilationThreadState _previousCompilationThreadState;
   volatile uint8_t             _compilationShouldBeInterrupted;

   static TR::FILE *_perfFile; // used on Linux for perl tool support


private:
   void logCompilationSuccess(
      J9VMThread *vmThread,
      TR_J9VMBase & vm,
      J9Method * method,
      const TR::SegmentAllocator &scratchSegmentProvider,
      TR_ResolvedMethod * compilee,
      TR::Compilation *compiler,
      TR_MethodMetaData *metadata,
      TR_OptimizationPlan * optimizationPlan);

   void processException(
      J9VMThread *vmThread,
      const TR::SegmentAllocator &scratchSegmentProvider,
      TR::Compilation * compiler,
      volatile bool & haveLockedClassUnloadMonitor,
      const char *exceptionName
      ) throw();

   void processExceptionCommonTasks(
      J9VMThread *vmThread,
      TR::SegmentAllocator const &scratchSegmentProvider,
      TR::Compilation * compiler,
      const char *exceptionName);

#if defined(TR_HOST_S390)
   void outputVerboseMMapEntry(
      TR_ResolvedMethod *compilee,
      const struct ::tm &date,
      const struct timeval &time,
      void *startPC,
      void *endPC,
      const char *fmt,
      const char *profiledString,
      const char *compileString
      );
   void outputVerboseMMapEntries(
      TR_ResolvedMethod *compilee,
      TR_MethodMetaData *metaData,
      const char *profiledString,
      const char *compileString
      );
#endif

   }; // CompilationInfoPerThreadBase
}

//--------------------------------------------------------------------
// The following class will be use by the separate compilation threads
// TR::CompilationInfoPerThreadBase will be used by compilation on application thread

namespace TR
{

class CompilationInfoPerThread : public TR::CompilationInfoPerThreadBase
   {
   friend class TR::CompilationInfo;
   friend class ::TR_DebugExt;
   public:
   CompilationInfoPerThread(TR::CompilationInfo &compInfo, J9JITConfig *jitConfig, int32_t id, bool isDiagnosticThread);
   bool                   initializationSucceeded() { return _initializationSucceeded; }
   j9thread_t             getOsThread() { return _osThread; }
   void                   setOsThread(j9thread_t t) { _osThread = t; }
   J9VMThread            *getCompilationThread() {return _compilationThread;}
   void                   setCompilationThread(J9VMThread *t) { _compilationThread = t; }
   int32_t                getCompThreadPriority() const { return _compThreadPriority; }
   void                   setCompThreadPriority(int32_t priority) { _compThreadPriority = priority; }
   int32_t                changeCompThreadPriority(int32_t priority, int32_t locationCode);
   TR::Monitor *getCompThreadMonitor() { return _compThreadMonitor; }
   void                   run();
   void                   processEntries();
   void                   processEntry(TR_MethodToBeCompiled &entry, J9::J9SegmentProvider &scratchSegmentProvider);
   bool                   shouldPerformCompilation(TR_MethodToBeCompiled &entry);
   void                   waitForWork();
   void                   doSuspend();
   void                   suspendCompilationThread();
   void                   resumeCompilationThread();
   void                   requeue(); // requeue only works for compilation on separate thread
   virtual void           setCompilationThreadState(CompilationThreadState v);
   void                   waitForGCCycleMonitor(bool threadHasVMAccess);
   char                  *getActiveThreadName() const { return _activeThreadName; }
   char                  *getSuspendedThreadName() const { return _suspendedThreadName; }
   int64_t                getLastTimeThreadWasSuspended() const { return _lastTimeThreadWasSuspended; }
   void                   setLastTimeThreadWasSuspended(int64_t t) { _lastTimeThreadWasSuspended = t; }
   int64_t                getLastTimeThreadWentToSleep() const { return _lastTimeThreadWentToSleep; }
   void                   setLastTimeThreadWentToSleep(int64_t t) { _lastTimeThreadWentToSleep = t; }
   int32_t                getLastCompilationDuration() const { return _lastCompilationDuration; }
   void                   setLastCompilationDuration(int32_t t) { _lastCompilationDuration = t; }
   bool                   isDiagnosticThread() const { return _isDiagnosticThread; }
   CpuSelfThreadUtilization& getCompThreadCPU() { return _compThreadCPU; }

   private:
   J9::J9SegmentCache initializeSegmentCache(J9::J9SegmentProvider &segmentProvider);

   j9thread_t             _osThread;
   J9VMThread            *_compilationThread;
   int32_t                _compThreadPriority; // to reduce number of checks
   TR::Monitor *_compThreadMonitor;
   char                  *_activeThreadName; // name of thread when active
   char                  *_suspendedThreadName; // name of thread when suspended
   uint64_t               _lastTimeThreadWasSuspended; // RAS; only accessed by the thread itself
   uint64_t               _lastTimeThreadWentToSleep; // RAS; only accessed by the thread itself
   int32_t                _lastCompilationDuration; // wall clock, ms
   bool                   _initializationSucceeded;
   bool                   _isDiagnosticThread;
   CpuSelfThreadUtilization _compThreadCPU;
   }; // CompilationInfoPerThread

} // namespace TR

#endif // COMPILATIONTHREAD_INCL
