/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
#if defined(J9VM_OPT_JITSERVER)
#include "env/VMJ9Server.hpp"
#include "env/PersistentCollections.hpp"
#endif /* defined(J9VM_OPT_JITSERVER) */

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
#if defined(J9VM_OPT_JITSERVER)
class ClientSessionData;
namespace JITServer
   {
   class ClientStream;
   class ServerStream;
   }
#endif /* defined(J9VM_OPT_JITSERVER) */

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
   TR_MethodMetaData *performAOTLoad(J9VMThread *context, TR::Compilation *, TR_ResolvedMethod *compilee, TR_J9VMBase *vm, J9Method *method);

   void preCompilationTasks(J9VMThread * vmThread,
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
   const void* findAotBodyInSCC(J9VMThread *vmThread, const J9ROMMethod *romMethod);

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
    * However a brief search reveals that such an approach is fraught with its own set of compiler-bug-related risk.
    */
   static TR_MethodMetaData *wrappedCompile(J9PortLibrary *portLib, void * opaqueParameters);

   bool methodCanBeCompiled(TR_Memory *trMemory, TR_FrontEnd *fe, TR_ResolvedMethod *compilee, TR_FilterBST *&filter);
   int32_t                getCompThreadId() const { return _compThreadId; }

   /**
    * \brief
    *
    * An RAII class used to scope an interruptible operation on a compilation thread.
    *
    * \details
    *
    * Interruptible (\see InterruptibleOperation) and Uninterruptible (\see UninterruptibleOperation) operations related
    * classes expressing whether the current compilation thread can terminate the compilation at the next yield point.
    * The two classes can be intertwined and the logic of what happens when scopes mix is as follows:
    *
    * - Nesting an InterruptibleOperation under an UninterruptibleOperation acts as a NOP
    * - Nesting an UninterruptibleOperation under an InterruptibleOperation results in an UninterruptibleOperation that
    *   once gone out of scope results in an InterruptibleOperation
    *
    * Put simply, an UninterruptibleOperation takes higher precedence over an InterruptibleOperation.
    */
   class InterruptibleOperation
      {
      friend class TR::CompilationInfoPerThreadBase;

   public:
      InterruptibleOperation(TR::CompilationInfoPerThreadBase &compThread) :
         _compThread(compThread)
         {
         if (_compThread._uninterruptableOperationDepth == 0)
            {
            _compThread._compilationCanBeInterrupted = true;
            }
         }

      ~InterruptibleOperation()
         {
         if (_compThread._uninterruptableOperationDepth == 0)
            {
            _compThread._compilationCanBeInterrupted = false;
            }
         }

   private:
      TR::CompilationInfoPerThreadBase & _compThread;
      };

   /**
    * \brief
    *
    * An RAII class used to scope an uninterruptible operation on a compilation thread.
    *
    * \details
    *
    * Interruptible (\see InterruptibleOperation) and Uninterruptible (\see UninterruptibleOperation) operations related
    * classes expressing whether the current compilation thread can terminate the compilation at the next yield point.
    * The two classes can be intertwined and the logic of what happens when scopes mix is as follows:
    *
    * - Nesting an InterruptibleOperation under an UninterruptibleOperation acts as a NOP
    * - Nesting an UninterruptibleOperation under an InterruptibleOperation results in an UninterruptibleOperation that
    *   once gone out of scope results in an InterruptibleOperation
    *
    * Put simply, an UninterruptibleOperation takes higher precedence over an InterruptibleOperation.
    */
   class UninterruptibleOperation
      {
      friend class TR::CompilationInfoPerThreadBase;

   public:
      UninterruptibleOperation(TR::CompilationInfoPerThreadBase &compThread) :
         _compThread(compThread), _originalValue(_compThread._compilationCanBeInterrupted)
         {
         _compThread._compilationCanBeInterrupted = false;
         _compThread._uninterruptableOperationDepth++;
         }

      ~UninterruptibleOperation()
         {
         _compThread._compilationCanBeInterrupted = _originalValue;
         _compThread._uninterruptableOperationDepth--;
         }

   private:
      TR::CompilationInfoPerThreadBase & _compThread;

      /// Represents the original value of whether the compilation can be interrupted before executing the
      /// uninterruptable operation
      bool _originalValue;
      };

   uint8_t compilationShouldBeInterrupted() const
      {
      return _compilationCanBeInterrupted ? _compilationShouldBeInterrupted : 0;
      }

   void                   setCompilationShouldBeInterrupted(uint8_t reason) { _compilationShouldBeInterrupted = reason; }
   TR_DataCache*          reservedDataCache() { return _reservedDataCache; }
   void                   setReservedDataCache(TR_DataCache *dataCache) { _reservedDataCache = dataCache; }
   int32_t                getNumJITCompilations() const { return _numJITCompilations; }
   void                   incNumJITCompilations() { _numJITCompilations++; }
   int32_t                getQszWhenCompStarted() const { return _qszWhenCompStarted; }
   void                   generatePerfToolEntry(); // for Linux only
   uintptr_t              getTimeWhenCompStarted() const { return _timeWhenCompStarted; }
   void                   setTimeWhenCompStarted(UDATA t) { _timeWhenCompStarted = t; }

   TR_RelocationRuntime  *reloRuntime();
   static TR::FILE *getPerfFile() { return _perfFile; } // used on Linux for perl tool support
   static void setPerfFile(TR::FILE *f) { _perfFile = f; }

#if defined(J9VM_OPT_JITSERVER)
   void                     setClientData(ClientSessionData *data) { _cachedClientDataPtr = data; }
   ClientSessionData       *getClientData() const { return _cachedClientDataPtr; }

   void                     setClientStream(JITServer::ClientStream *stream) { _clientStream = stream; }
   JITServer::ClientStream *getClientStream() const { return _clientStream; }

   /**
      @brief After this method runs, all subsequent persistent allocations
      on the current thread for a given client will use a per-client allocator,
      instead of the global one.
    */
   void                     enterPerClientAllocationRegion();

   /**
      @brief Ends per-client allocation on this thread. After this method returns,
      all allocations will use the global persistent allocator.
   */
   void                     exitPerClientAllocationRegion();
   TR_PersistentMemory *getPerClientPersistentMemory() { return _perClientPersistentMemory; }

   /**
      @brief Heuristic that returns true if compiling a method of given size
             and at given optimization level less is likely to consume little
             memory relative to what the JVM has at its disposal
    */
   bool isMemoryCheapCompilation(uint32_t bcsz, TR_Hotness optLevel);
   /**
      @brief Heuristic that returns true if compiling a method of given size
             and at given optimization level less is likely to consume little
             CPU relative to what the JVM has at its disposal
    */
   bool isCPUCheapCompilation(uint32_t bcsz, TR_Hotness optLevel);
   /**
      @brief Returns true if we truly cannot perform a remote compilation
             maybe because the server is not compatible, is not available, etc.

             Note, that if this query returns false, it is not guaranteed that we
             can do a remote compilation, because the server could have died since
             we last checked.
    */
   bool cannotPerformRemoteComp(
#if defined(J9VM_OPT_CRIU_SUPPORT)
      J9VMThread *vmThread
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
   );
   /**
      @brief Returns true if heuristics determine that we have the resources to perform
             this compilation locally, rather than offloading it to the remote server.
    */
   bool preferLocalComp(const TR_MethodToBeCompiled *entry);


   bool compilationCanBeInterrupted() const { return _compilationCanBeInterrupted; }

   void downgradeLocalCompilationIfLowPhysicalMemory(TR_MethodToBeCompiled *entry);

#endif /* defined(J9VM_OPT_JITSERVER) */

   protected:

   TR::CompilationInfo &        _compInfo;
   J9JITConfig * const          _jitConfig;
   TR_SharedCacheRelocationRuntime _sharedCacheReloRuntime;
#if defined(J9VM_OPT_JITSERVER)
   TR_JITServerRelocationRuntime _remoteCompileReloRuntime;
#endif /* defined(J9VM_OPT_JITSERVER) */
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

   /// Determines whether this compilation thread can be interrupted the compile at the next yield point. A different
   /// thread may still request that the compilation _should_ be interrupted, however we may not be in a state at
   /// which we _can_ interrupt.
   bool _compilationCanBeInterrupted;

   /// Counts the number of nested \see UninterruptibleOperation in the stack. An \see InterruptibleOperation can only
   /// be issued if the depth of uninterruptable operations is 0.
   int32_t _uninterruptableOperationDepth;

   bool                         _addToJProfilingQueue;

   volatile CompilationThreadState _compilationThreadState;
   volatile CompilationThreadState _previousCompilationThreadState;
   volatile uint8_t             _compilationShouldBeInterrupted;

   static TR::FILE *_perfFile; // used on Linux for perl tool support

#if defined(J9VM_OPT_JITSERVER)
   ClientSessionData * _cachedClientDataPtr;
   JITServer::ClientStream * _clientStream;
   TR_PersistentMemory * _perClientPersistentMemory;
#endif /* defined(J9VM_OPT_JITSERVER) */

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
      const char *exceptionName,
      TR_MethodToBeCompiled *entry);

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
   TR::Monitor           *getCompThreadMonitor() { return _compThreadMonitor; }
   void                   run();
   void                   processEntries();
   virtual void           processEntry(TR_MethodToBeCompiled &entry, J9::J9SegmentProvider &scratchSegmentProvider);
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
   TR::FILE              *getRTLogFile() { return _rtLogFile; }
   void                   closeRTLogFile();
   void                   openRTLogFile();
   virtual void           freeAllResources();

#if defined(J9VM_OPT_JITSERVER)
   TR_J9ServerVM            *getServerVM() const { return _serverVM; }
   void                      setServerVM(TR_J9ServerVM *vm) { _serverVM = vm; }
   TR_J9SharedCacheServerVM *getSharedCacheServerVM() const { return _sharedCacheServerVM; }
   void                      setSharedCacheServerVM(TR_J9SharedCacheServerVM *vm) { _sharedCacheServerVM = vm; }
   JITServer::ServerStream  *getStream();
   J9ROMClass               *getAndCacheRemoteROMClass(J9Class *clazz);
   J9ROMClass               *getRemoteROMClassIfCached(J9Class *clazz);
   PersistentUnorderedSet<TR_OpaqueClassBlock*> *getClassesThatShouldNotBeNewlyExtended() const { return _classesThatShouldNotBeNewlyExtended; }
#endif /* defined(J9VM_OPT_JITSERVER) */

   protected:
   J9::J9SegmentCache initializeSegmentCache(J9::J9SegmentProvider &segmentProvider);

   j9thread_t             _osThread;
   J9VMThread            *_compilationThread;
   int32_t                _compThreadPriority; // to reduce number of checks
   TR::Monitor           *_compThreadMonitor;
   char                  *_activeThreadName; // name of thread when active
   char                  *_suspendedThreadName; // name of thread when suspended
   uint64_t               _lastTimeThreadWasSuspended; // RAS; only accessed by the thread itself
   uint64_t               _lastTimeThreadWentToSleep; // RAS; only accessed by the thread itself
   int32_t                _lastCompilationDuration; // wall clock, ms
   bool                   _initializationSucceeded;
   bool                   _isDiagnosticThread;
   CpuSelfThreadUtilization _compThreadCPU;
   TR::FILE              *_rtLogFile;
#if defined(J9VM_OPT_JITSERVER)
   TR_J9ServerVM         *_serverVM;
   TR_J9SharedCacheServerVM *_sharedCacheServerVM;
   // The following hastable caches <classLoader,classname> --> <J9Class> mappings
   // The cache only lives during a compilation due to class unloading concerns
   PersistentUnorderedSet<TR_OpaqueClassBlock*> *_classesThatShouldNotBeNewlyExtended;
#endif /* defined(J9VM_OPT_JITSERVER) */

   }; // CompilationInfoPerThread

#if defined(J9VM_OPT_JITSERVER)
extern thread_local TR::CompilationInfoPerThread * compInfoPT;
#endif /* defined(J9VM_OPT_JITSERVER) */

} // namespace TR

#endif // COMPILATIONTHREAD_INCL
