/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#ifndef COMPILATION_RUNTIME_HPP
#define COMPILATION_RUNTIME_HPP

#pragma once

#include "AtomicSupport.hpp"
#include "compile/CompilationTypes.hpp"
#include "control/CompilationPriority.hpp"
#include "control/CompilationOperations.hpp"
#include "control/CompilationTracingFacility.hpp"
#include "control/ClassHolder.hpp"
#include "env/CpuUtilization.hpp"
#include "env/Processors.hpp"
#include "env/ProcessorInfo.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "infra/Flags.hpp"
#include "infra/Statistics.hpp"
#include "control/rossa.h"
#include "runtime/RelocationRuntime.hpp"
#if defined(J9VM_OPT_JITSERVER)
#include "env/PersistentCollections.hpp"
#include "net/ServerStream.hpp"
#endif /* defined(J9VM_OPT_JITSERVER) */

extern "C" {
struct J9Method;
struct J9JITConfig;
struct J9VMThread;
struct J9ROMMethod;
struct J9ClassLoader;
struct J9SharedClassJavacoreDataDescriptor;
}

class CpuUtilization;
namespace TR { class CompilationInfoPerThread; }
namespace TR { class CompilationInfoPerThreadBase; }
class TR_FilterBST;
class TR_FrontEnd;
class TR_HWProfiler;
class TR_J9VMBase;
class TR_LowPriorityCompQueue;
class TR_OptimizationPlan;
class TR_PersistentMethodInfo;
class TR_RelocationRuntime;
class TR_ResolvedMethod;
namespace TR { class MonitorTable; }
namespace TR { class IlGeneratorMethodDetails; }
namespace TR { class Options; }
namespace J9 { class RWMonitor; }
struct TR_JitPrivateConfig;
struct TR_MethodToBeCompiled;
template <typename T> class TR_PersistentArray;
typedef J9JITExceptionTable TR_MethodMetaData;
#if defined(J9VM_OPT_JITSERVER)
class ClientSessionHT;
#endif /* defined(J9VM_OPT_JITSERVER) */

struct TR_SignatureCountPair
{
   TR_PERSISTENT_ALLOC(TR_Memory::OptimizationPlan)
   TR_SignatureCountPair(char *string, int32_t c)
      {
      count = c;

      for(int32_t i=0 ; i <  4000 ; i++)
         {
         signature[i] = string[i];
         if(string[i] == '\0')
            break;
         }

      };
   char signature[4000];
   int32_t count;
};

#ifndef J9_INVOCATION_COUNT_MASK
#define J9_INVOCATION_COUNT_MASK                   0x00000000FFFFFFFF
#endif

class TR_LowPriorityCompQueue
   {
   public:
      friend class TR_DebugExt;
      TR_PERSISTENT_ALLOC(TR_Memory::PersistentInfo); // TODO: define its own category
      static const uint32_t HT_SIZE = (1 << 13); // power of two for cheap modulo
      TR_LowPriorityCompQueue();
      ~TR_LowPriorityCompQueue();

      void setCompInfo(TR::CompilationInfo *compInfo) { _compInfo = compInfo; }
      bool hasLowPriorityRequest() const { return (_firstLPQentry != NULL); }
      TR_MethodToBeCompiled *getFirstLPQRequest() const { return _firstLPQentry; }
      TR_MethodToBeCompiled *extractFirstLPQRequest();
      TR_MethodToBeCompiled *findAndDequeueFromLPQ(TR::IlGeneratorMethodDetails &details,
         uint8_t reason, TR_J9VMBase *fe, bool & dequeued);
      void enqueueCompReqToLPQ(TR_MethodToBeCompiled *compReq);
      bool createLowPriorityCompReqAndQueueIt(TR::IlGeneratorMethodDetails &details, void *startPC, uint8_t reason);
      bool addFirstTimeCompReqToLPQ(J9Method *j9method, uint8_t reason);
      bool addUpgradeReqToLPQ(TR_MethodToBeCompiled*);
      int32_t getLowPriorityQueueSize() const { return _sizeLPQ; }
      int32_t getLPQWeight() const { return _LPQWeight; }
      void increaseLPQWeightBy(uint8_t weight) { _LPQWeight += (int32_t)weight; }
      void decreaseLPQWeightBy(uint8_t weight) { _LPQWeight -= (int32_t)weight; }
      void invalidateRequestsForUnloadedMethods(J9Class * unloadedClass);
      void purgeLPQ();

      // The following refer to the mechanism that schedules low priority
      // compilation requests based on the Iprofiler
      static uint32_t hash(J9Method *j9method) { return ((uintptr_t)j9method >> 3) & (HT_SIZE - 1); }
      bool isTrackingEnabled() const { return _trackingEnabled; }
      void startTrackingIProfiledCalls(int32_t threshold);
      bool isTrackableMethod(J9Method *j9method) const;
      void stopTrackingMethod(J9Method *j9method); // Executed by the compilation thread when a low pri req is processed
      void tryToScheduleCompilation(J9VMThread *vmThread, J9Method *j9method); // Executed by the IProfiler thread
      void purgeEntriesOnClassLoaderUnloading(J9ClassLoader *j9classLoader);
      void purgeEntriesOnClassRedefinition(J9Class *j9class);
      void printStats() const;
      void incStatsBypass() { _STAT_bypass++; } // Done by comp threads
      void incStatsCompFromLPQ(uint8_t reason); // Done by JIT
      void incStatsReqQueuedToLPQ(uint8_t reason);
      void incNumFailuresToEnqueue() { _STAT_numFailedToEnqueueInLPQ++; }

      struct Entry
         {
         uintptr_t _j9method; // this is the key; Initialized by IProfiler thread; reset by compilation thread
         uint32_t   _count; // updated by IProfiler thread
         bool       _queuedForCompilation; // used to avoid duplicates in the queue; set by IProfiler thread
         bool isInvalid() const { return !_j9method; }
         void setInvalid() { _j9method = 0; }
         void initialize(J9Method *m, uint32_t c) { _j9method = (uintptr_t)m; _count = c; _queuedForCompilation = false; }
         };
   private:
      TR::CompilationInfo*    _compInfo;
      TR_MethodToBeCompiled* _firstLPQentry; // first entry of low priority queue
      TR_MethodToBeCompiled* _lastLPQentry; // last entry of low priority queue
      int32_t                _sizeLPQ; // size of low priority queue
      int32_t                _LPQWeight; // weight of low priority queue
      uint32_t               _threshold;
      bool                   _trackingEnabled;
      // Direct mapped hashtable that records j9methods and "counts"
      Entry *_spine;         // my hashtable
      // stats written by IProfiler thread
      uint32_t _STAT_compReqQueuedByIProfiler;
      uint32_t _STAT_conflict;
      uint32_t _STAT_staleScrubbed;
      // stats written by comp thread
      uint32_t _STAT_bypass;   // an LPQ compilation was overtaken by a normal compilation
      uint32_t _STAT_compReqQueuedByJIT;
      uint32_t _STAT_LPQcompFromIprofiler; // first time compilations coming from LPQ
      uint32_t _STAT_LPQcompFromInterpreter;
      uint32_t _STAT_LPQcompUpgrade;
      // stats written by application threads
      uint32_t _STAT_compReqQueuedByInterpreter;
      uint32_t _STAT_numFailedToEnqueueInLPQ;
   }; // TR_LowPriorityCompQueue


// Definition of compilation queue to hold JProfiling candidates
class TR_JProfilingQueue
   {
   public:
      friend class TR_DebugExt;
      TR_PERSISTENT_ALLOC(TR_Memory::PersistentInfo); // TODO: define its own category
      TR_JProfilingQueue() : _firstQentry(NULL), _lastQentry(NULL), _size(0), _weight(0), _allowProcessing(false) {};
      void setCompInfo(TR::CompilationInfo *compInfo) { _compInfo = compInfo; }
      bool isEmpty() const { return (_firstQentry == NULL); }
      TR_MethodToBeCompiled *getFirstCompRequest() const { return _firstQentry; }
      TR_MethodToBeCompiled *extractFirstCompRequest();
      bool createCompReqAndQueueIt(TR::IlGeneratorMethodDetails &details, void *startPC);
      int32_t getQSize() const { return _size; }
      int32_t getQWeight() const { return _weight; }
      bool getAllowProcessing() const { return _allowProcessing; }
      void setAllowProcessing() { _allowProcessing = true; }
      static bool isJProfilingCandidate(TR_MethodToBeCompiled *entry, TR::Options *options, TR_J9VMBase* fej9);
      void invalidateRequestsForUnloadedMethods(J9Class * unloadedClass);
      void purge();
   private:
      void increaseQWeightBy(uint8_t weight) { _weight += (int32_t)weight; }
      void decreaseQWeightBy(uint8_t weight) { _weight -= (int32_t)weight; }
      void enqueueCompReq(TR_MethodToBeCompiled *compReq);

      TR::CompilationInfo*   _compInfo;
      TR_MethodToBeCompiled* _firstQentry; // first entry of JProfiling queue
      TR_MethodToBeCompiled* _lastQentry; // last entry of JProfiling queue
      int32_t                _size; // size of JProfiling queue
      int32_t                _weight; // weight of JProfiling queue
      bool                   _allowProcessing;
   };


// Supporting class for getting information on density of samples
class TR_JitSampleInfo
   {
   public:
      void init()
         {
         _maxSamplesPerSecond = 0;
         _samplesPerSecondDuringLastInterval = 0;
         _sizeOfLastInterval = 1; // avoid check for divide by 0
         _globalSampleCounterInLastInterval = 0;
         _timestampOfLastInterval = 0;
         _increaseFactor = 1;
         }
      TR_JitSampleInfo() { init(); }
      uint32_t getMaxSamplesPerSecond() const { return _maxSamplesPerSecond; }
      uint32_t getSamplesPerSecondDuringLastInterval() const { return _samplesPerSecondDuringLastInterval; }
      uint32_t getCurrentSamplesPerSecond(uint64_t crtTime, uint32_t crtGlobalSampleCounter) const
         {
         uint64_t diffTime = crtTime - _timestampOfLastInterval;
         return diffTime > 0 ? (crtGlobalSampleCounter - _globalSampleCounterInLastInterval) * 1000 / diffTime : 0;
         }
      void update(uint64_t crtTime, uint32_t crtGlobalSampleCounter);
      uint32_t getIncreaseFactor() const { return _increaseFactor; }
   private:
      uint32_t _maxSamplesPerSecond;
      uint32_t _samplesPerSecondDuringLastInterval;
      uint32_t _sizeOfLastInterval; // ms
      uint32_t _globalSampleCounterInLastInterval;
      uint64_t _timestampOfLastInterval;
      uint32_t _increaseFactor;
   }; // class TR_JitSampleInfo

// The following class is used for tracking methods that have their invocation
// count decremented due to a sample in interpreted code. When that happens
// we add the method to a list. When a method needs to be compiled we check
// the list and if the method is in the list we delete it and schedule a
// SamplingJprofiling compilation for the count that has been skipped
class TR_InterpreterSamplingTracking
   {
   public:
      TR_PERSISTENT_ALLOC(TR_MemoryBase::CompilationInfo);
   class TR_MethodCnt
      {
      friend class TR_InterpreterSamplingTracking;
      TR_MethodCnt(J9Method *m, int32_t c) : _next(NULL), _method(m), _skippedCount(c) {}

      TR_MethodCnt *_next;
      J9Method * _method;
      int32_t _skippedCount;
      };

      TR_InterpreterSamplingTracking(TR::CompilationInfo *compInfo) :
         _container(NULL), _compInfo(compInfo), _maxElements(0), _size(0) {}
      // both these methods need the compilation monitor in hand
      void addOrUpdate(J9Method *method, int32_t cnt);
      int32_t findAndDelete(J9Method *method);
      void onClassUnloading() {/* TODO: prune list when classes change or get unloaded */}
      uint32_t getMaxElements() const { return _maxElements; }
   private:
      TR_MethodCnt *_container;
      TR::CompilationInfo *_compInfo; // cache value of compInfo
      uint32_t _maxElements;
      uint32_t _size;
   };


class J9Method_HT
   {
   public:
      TR_PERSISTENT_ALLOC(TR_MemoryBase::CompilationInfo);
      static const size_t LOG_HT_SIZE = 6;
      static const size_t HT_SIZE = 1 << LOG_HT_SIZE;
      static const size_t MASK = HT_SIZE - 1;
      struct HT_Entry
         {
         HT_Entry *_next; // for linking entries together
         J9Method *_j9method;
         volatile int32_t  _count; // invocation count when last visited
         uint32_t _seqID;
         uint64_t _timestamp;
         HT_Entry(J9Method *j9method, uint64_t timestamp);
         uint64_t getTimestamp() const { return _timestamp; }
         };
      J9Method_HT(TR::PersistentInfo *persistentInfo);
      size_t getNumEntries() const { return _numEntries; }
      // method used by various hooks that perform class unloading
      void onClassUnloading();

   protected:
      TR::PersistentInfo *getPersistentInfo() const { return _persistentInfo; }
      size_t hash(J9Method *j9method) const
         { return (size_t)(((uintptr_t)j9method >> 3) ^ ((uintptr_t)j9method >> (3 + LOG_HT_SIZE))); }
      HT_Entry *find(J9Method *j9method) const;
      bool addNewEntry(J9Method *j9method, uint64_t timestamp);
   private:
      // Backbone of the hashtable
      struct HT_Entry *_spine[HT_SIZE];
      TR::PersistentInfo *_persistentInfo;
      int32_t _numEntries; // for stats; may be imprecise due to multithreading issues
   };

class DLTTracking : public J9Method_HT
   {
   public:
   DLTTracking(TR::PersistentInfo *persistentInfo) : J9Method_HT(persistentInfo) {}
   // Method used by application threads when a DLT decision needs to be taken
   // Need to have VMaccess in hand
   bool shouldIssueDLTCompilation(J9Method *j9method, int32_t numHitsInDLTBuffer);

   // Method used by application threads whenever invocation count
   // for a method is changed abruptly (e.g. interpreter sampling)
   // Need to have VMaccess in hand
   void adjustStoredCounterForMethod(J9Method *j9method, int32_t countDiff);
   };



//--------------------------------- TR::CompilationInfo -----------------------
namespace TR
{

class CompilationInfo
   {
public:
   friend class ::TR_DebugExt;
   friend class TR::CompilationInfoPerThreadBase;
   friend class TR::CompilationInfoPerThread;
   friend class ::TR_LowPriorityCompQueue;
   friend class ::TR_JProfilingQueue;

   TR_PERSISTENT_ALLOC(TR_MemoryBase::CompilationInfo);

   enum hotnessWeights
      {
      AOT_LOAD_WEIGHT       = 1, // actually AOT loads have their own parameter
      THUNKS_WEIGHT         = 1,
      NOOPT_WEIGHT          = 1,
      COLD_WEIGHT           = 2,
      WARM_LOOPLESS_WEIGHT  = 6,
      WARM_LOOPY_WEIGHT     = 12,
      JSR292_WEIGHT         = 20,
      HOT_WEIGHT            = 30,
      VERY_HOT_WEIGHT       = 100,
      MAX_WEIGHT            = 255
      };

   enum TR_SamplerStates
      {
      SAMPLER_NOT_INITIALIZED = 0,
      SAMPLER_DEFAULT,
      SAMPLER_IDLE,
      SAMPLER_DEEPIDLE,
      SAMPLER_SUSPENDED,
      SAMPLER_STOPPED,
      SAMPLER_LAST_STATE, // must be the last one
      // If adding new state, one must add a new name as well in samplerThreadStateNames array
      // and a frequency in samplerThreadStateFrequencies array
     };

   enum TR_SamplingThreadLifetimeStates
      {
      SAMPLE_THR_NOT_CREATED = 0,
      SAMPLE_THR_FAILED_TO_ATTACH,
      SAMPLE_THR_ATTACHED,
      SAMPLE_THR_INITIALIZED,
      SAMPLE_THR_STOPPING,
      SAMPLE_THR_DESTROYED,
      SAMPLE_THR_LAST_STATE // must be the last one
      };

   enum TR_CompThreadActions
      {
      PROCESS_ENTRY,
      GO_TO_SLEEP_EMPTY_QUEUE,
      GO_TO_SLEEP_CONCURRENT_EXPENSIVE_REQUESTS,
      SUSPEND_COMP_THREAD_EXCEED_CPU_ENTITLEMENT,
      THROTTLE_COMP_THREAD_EXCEED_CPU_ENTITLEMENT,
      SUSPEND_COMP_THREAD_EMPTY_QUEUE,
      UNDEFINED_ACTION
      };

   struct DLT_record
      {
      DLT_record         *_next;
      J9Method           *_method;
      void               *_dltEntry;
      int32_t             _bcIndex;
      };

   struct CompilationStatistics
      {
      // perhaps not the greatest way to do this, but TR_Stats classes weren't an exact match
      uint32_t _heartbeatWindowCount;
      uint32_t _windowStartTick;
      uint32_t _sampleMessagesSent;
      uint32_t _sampleMessagesReceived;
      uint32_t _interpretedMethodSamples;
      uint32_t _compiledMethodSamples;
      uint32_t _compiledMethodSamplesIgnored;
      uint32_t _ticksInIdleMode;
      uint32_t _methodsCompiledOnCount;
      uint32_t _methodsReachingSampleInterval;
      uint32_t _methodsSelectedToRecompile;
      uint32_t _methodsSampleWindowReset;
      }; // CompilationStatistics

   struct CompilationStatsPerInterval
      {
      uint32_t _interpretedMethodSamples;
      uint32_t _compiledMethodSamples;
      uint32_t _numFirstTimeCompilationsInInterval;
      uint32_t _numRecompilationsInInterval;
      uint32_t _numRecompPrevInterval;
      uint32_t _samplesSentInInterval;
      void reset()
         {
         _interpretedMethodSamples = _compiledMethodSamples = 0;
         _numFirstTimeCompilationsInInterval = _numRecompilationsInInterval = 0;
         _numRecompPrevInterval = 0; _samplesSentInInterval = 0;
         }
      void decay()
         {
         _interpretedMethodSamples >>= 1;
         _compiledMethodSamples >>= 1;
         _numFirstTimeCompilationsInInterval >>= 1;
         _numRecompPrevInterval = _numRecompilationsInInterval; // remember the previous value
         _numRecompilationsInInterval = (_numRecompilationsInInterval*2)/3;
         _samplesSentInInterval = 0;
         }
      }; // CompilationStatsPerInterval

   static bool createCompilationInfo(J9JITConfig * jitConfig);
   static void freeCompilationInfo(J9JITConfig *jitConfig);
   static TR::CompilationInfo *get(J9JITConfig * = 0) { return _compilationRuntime; }
   static bool shouldRetryCompilation(TR_MethodToBeCompiled *entry, TR::Compilation *comp);
   static bool shouldAbortCompilation(TR_MethodToBeCompiled *entry, TR::PersistentInfo *persistentInfo);
   static bool canRelocateMethod(TR::Compilation * comp);
   static bool useSeparateCompilationThread();
   static int computeCompilationThreadPriority(J9JavaVM *vm);
   static void *compilationEnd(J9VMThread *context, TR::IlGeneratorMethodDetails & details, J9JITConfig *jitConfig, void * startPC,
                               void *oldStartPC, TR_FrontEnd *vm=0, TR_MethodToBeCompiled *entry=NULL, TR::Compilation *comp=NULL);
#if defined(J9VM_OPT_JITSERVER)
   static JITServer::ServerStream *getStream();
#endif /* defined(J9VM_OPT_JITSERVER) */
   static bool isInterpreted(J9Method *method) { return !isCompiled(method); }
   static bool isCompiled(J9Method *method)
      {
#if defined(J9VM_OPT_JITSERVER)
      if (auto stream = getStream())
         {
         stream->write(JITServer::MessageType::CompInfo_isCompiled, method);
         return std::get<0>(stream->read<bool>());
         }
#endif /* defined(J9VM_OPT_JITSERVER) */
      return (((uintptr_t)method->extra) & J9_STARTPC_NOT_TRANSLATED) == 0;
      }
   static bool isJNINative(J9Method *method)
      {
#if defined(J9VM_OPT_JITSERVER)
      if (auto stream = getStream())
         {
         stream->write(JITServer::MessageType::CompInfo_isJNINative, method);
         return std::get<0>(stream->read<bool>());
         }
#endif /* defined(J9VM_OPT_JITSERVER) */
      // Note: This query is only concerned with the method to be compiled
      // and so we don't have to care if the VM has a FastJNI version
      return (((uintptr_t)method->constantPool) & J9_STARTPC_JNI_NATIVE) != 0;
      }

   static int32_t getInvocationCount(J9Method *method)
      {
#if defined(J9VM_OPT_JITSERVER)
      if (auto stream = getStream())
         {
         stream->write(JITServer::MessageType::CompInfo_getInvocationCount, method);
         return std::get<0>(stream->read<int32_t>());
         }
#endif /* defined(J9VM_OPT_JITSERVER) */
      if (((intptr_t)method->extra & J9_STARTPC_NOT_TRANSLATED) == 0)
         return -1;
      int32_t count = getJ9MethodVMExtra(method);
      if (count < 0)
         return count;
      return count >> 1;
      }
   static intptr_t getJ9MethodExtra(J9Method *method)
      {
#if defined(J9VM_OPT_JITSERVER)
      if (auto stream = getStream())
         {
         stream->write(JITServer::MessageType::CompInfo_getJ9MethodExtra, method);
         return (intptr_t) std::get<0>(stream->read<uint64_t>());
         }
#endif /* defined(J9VM_OPT_JITSERVER) */
      return (intptr_t)method->extra;
      }
   static int32_t getJ9MethodVMExtra(J9Method *method)
      {
#if defined(J9VM_OPT_JITSERVER)
      TR_ASSERT_FATAL(!TR::CompilationInfo::getStream(), "not yet implemented for JITServer");
#endif /* defined(J9VM_OPT_JITSERVER) */
      return (int32_t)((intptr_t)method->extra);
      }
   static uint32_t getJ9MethodJITExtra(J9Method *method)
      {
#if defined(J9VM_OPT_JITSERVER)
      TR_ASSERT_FATAL(!TR::CompilationInfo::getStream(), "not yet implemented for JITServer");
#endif /* defined(J9VM_OPT_JITSERVER) */
      TR_ASSERT((intptr_t)method->extra & J9_STARTPC_NOT_TRANSLATED, "MethodExtra Already Jitted!");
      return (uint32_t)((uintptr_t)method->extra >> 32);
      }
   static void * getJ9MethodStartPC(J9Method *method)
      {
#if defined(J9VM_OPT_JITSERVER)
      if (auto stream = getStream())
         {
         stream->write(JITServer::MessageType::CompInfo_getJ9MethodStartPC, method);
         return std::get<0>(stream->read<void*>());
         }
      else
#endif /* defined(J9VM_OPT_JITSERVER) */
         {
         TR_ASSERT(!((intptr_t)method->extra & J9_STARTPC_NOT_TRANSLATED), "Method NOT Jitted!");
         return (void *)method->extra;
         }
      }
   static void setJ9MethodExtra(J9Method *method, intptr_t newValue)
      {
#if defined(J9VM_OPT_JITSERVER)
      if (auto stream = getStream())
         {
         stream->write(JITServer::MessageType::CompInfo_setJ9MethodExtra, method, (uint64_t) newValue);
         std::get<0>(stream->read<JITServer::Void>());
         }
      else
#endif /* defined(J9VM_OPT_JITSERVER) */
         {
         method->extra = (void *)newValue;
         }
      }
   static bool setJ9MethodExtraAtomic(J9Method *method, intptr_t oldValue, intptr_t newValue)
      {
#if defined(J9VM_OPT_JITSERVER)
      TR_ASSERT_FATAL(!TR::CompilationInfo::getStream(), "not yet implemented for JITServer");
#endif /* defined(J9VM_OPT_JITSERVER) */
      return oldValue == VM_AtomicSupport::lockCompareExchange((UDATA*)&method->extra, oldValue, newValue);
      }
   static bool setJ9MethodExtraAtomic(J9Method *method, intptr_t newValue)
      {
#if defined(J9VM_OPT_JITSERVER)
      TR_ASSERT_FATAL(!TR::CompilationInfo::getStream(), "not yet implemented for JITServer");
#endif /* defined(J9VM_OPT_JITSERVER) */
      intptr_t oldValue = (intptr_t)method->extra;
      return setJ9MethodExtraAtomic(method, oldValue, newValue);
      }
   static bool setJ9MethodVMExtra(J9Method *method, int32_t value)
      {
#if defined(J9VM_OPT_JITSERVER)
      TR_ASSERT_FATAL(!TR::CompilationInfo::getStream(), "not yet implemented for JITServer");
#endif /* defined(J9VM_OPT_JITSERVER) */
      intptr_t oldValue = (intptr_t)method->extra;
      //intptr_t newValue = oldValue & (intptr_t)~J9_INVOCATION_COUNT_MASK;
      //newValue |= (intptr_t)value;
      intptr_t newValue = (intptr_t)value;
      return setJ9MethodExtraAtomic(method, oldValue, newValue);
      }
   static bool setInvocationCount(J9Method *method, int32_t newCount)
      {
#if defined(J9VM_OPT_JITSERVER)
      if (auto stream = getStream())
         {
         stream->write(JITServer::MessageType::CompInfo_setInvocationCount, method, newCount);
         return std::get<0>(stream->read<bool>());
         }
#endif /* defined(J9VM_OPT_JITSERVER) */
      newCount = (newCount << 1) | 1;
      if (newCount < 1)
         return false;
      return setJ9MethodVMExtra(method, newCount);
      }
   static bool setInvocationCount(J9Method *method, int32_t oldCount, int32_t newCount)
      {
#if defined(J9VM_OPT_JITSERVER)
      if (auto stream = getStream())
         {
         stream->write(JITServer::MessageType::CompInfo_setInvocationCountAtomic, method, oldCount, newCount);
         return std::get<0>(stream->read<bool>());
         }
#endif /* defined(J9VM_OPT_JITSERVER) */
      newCount = (newCount << 1) | 1;
      oldCount = (oldCount << 1) | 1;
      if (newCount < 0)
         return false;
      intptr_t oldMethodExtra = (intptr_t) method->extra & (intptr_t)(~J9_INVOCATION_COUNT_MASK);
      intptr_t newMethodExtra = oldMethodExtra | newCount;
      oldMethodExtra = oldMethodExtra | oldCount;
      bool success = setJ9MethodExtraAtomic(method, oldMethodExtra, newMethodExtra);
      if (success)
         {
         DLTTracking *dltHT = _compilationRuntime->getDLT_HT();
         if (dltHT)
            dltHT->adjustStoredCounterForMethod(method, oldCount - newCount);
         }
      return success;
      }
   static void setInitialInvocationCountUnsynchronized(J9Method *method, int32_t value)
      {
#if defined(J9VM_OPT_JITSERVER)
      TR_ASSERT_FATAL(!TR::CompilationInfo::getStream(), "not yet implemented for JITServer");
#endif /* defined(J9VM_OPT_JITSERVER) */
      value = (value << 1) | 1;
      if (value < 0)
          value = INT_MAX;
      method->extra = reinterpret_cast<void *>(value);
      }

   static uint32_t getMethodBytecodeSize(const J9ROMMethod * romMethod);
   static uint32_t getMethodBytecodeSize(J9Method* method);

   // Check to see if the J9AccMethodHasMethodHandleInvokes flag is set
   static bool isJSR292(const J9ROMMethod *romMethod);
   static bool isJSR292(J9Method *j9method);

#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   static void disableAOTCompilations();
#endif

   void * operator new(size_t s, void * p) throw() { return p; }
   CompilationInfo (J9JITConfig *jitConfig);
   TR::Monitor *getCompilationMonitor() {return _compilationMonitor;}
   void acquireCompMonitor(J9VMThread *vmThread); // used when we know we have a compilation monitor
   void releaseCompMonitor(J9VMThread *vmThread); // used when we know we have a compilation monitor
   void waitOnCompMonitor(J9VMThread *vmThread);
   intptr_t waitOnCompMonitorTimed(J9VMThread *vmThread, int64_t millis, int32_t nanos);

   TR_PersistentMemory *     persistentMemory() { return _persistentMemory; }

   TR::PersistentInfo    * getPersistentInfo() { return persistentMemory()->getPersistentInfo(); }
   TR_MethodToBeCompiled *requestExistsInCompilationQueue(TR::IlGeneratorMethodDetails & details, TR_FrontEnd *fe);

   TR_MethodToBeCompiled *addMethodToBeCompiled(TR::IlGeneratorMethodDetails &details, void *pc, CompilationPriority priority,
      bool async, TR_OptimizationPlan *optPlan, bool *queued, TR_YesNoMaybe methodIsInSharedCache);
#if defined(J9VM_OPT_JITSERVER)
   TR_MethodToBeCompiled *addOutOfProcessMethodToBeCompiled(JITServer::ServerStream *stream);
#endif /* defined(J9VM_OPT_JITSERVER) */
   void                   queueEntry(TR_MethodToBeCompiled *entry);
   void                   recycleCompilationEntry(TR_MethodToBeCompiled *cur);
#if defined(J9VM_OPT_JITSERVER)
   void                   requeueOutOfProcessEntry(TR_MethodToBeCompiled *entry);
#endif /* defined(J9VM_OPT_JITSERVER) */
   TR_MethodToBeCompiled *adjustCompilationEntryAndRequeue(TR::IlGeneratorMethodDetails &details,
                                                           TR_PersistentMethodInfo *methodInfo,
                                                           TR_Hotness newOptLevel, bool useProfiling,
                                                           CompilationPriority priority, TR_J9VMBase *fe);
   void changeCompReqFromAsyncToSync(J9Method * method);
   int32_t                promoteMethodInAsyncQueue(J9Method * method, void *pc);
   TR_MethodToBeCompiled *getNextMethodToBeCompiled(TR::CompilationInfoPerThread *compInfoPT, bool compThreadCameOutOfSleep, TR_CompThreadActions*);
   TR_MethodToBeCompiled *peekNextMethodToBeCompiled();
   TR_MethodToBeCompiled *getMethodQueue() { return _methodQueue; }
   int32_t getOverallCompCpuUtilization() const { return _overallCompCpuUtilization; } // -1 in case of error. 0 if feature is not enabled
   void setOverallCompCpuUtilization(int32_t c) { _overallCompCpuUtilization = c; }
   TR_YesNoMaybe exceedsCompCpuEntitlement() const { return _exceedsCompCpuEntitlement; }
   void setExceedsCompCpuEntitlement(TR_YesNoMaybe value) { _exceedsCompCpuEntitlement = value; }
   int32_t computeCompThreadSleepTime(int32_t compilationTimeMs);
   bool                   isQueuedForCompilation(J9Method *, void *oldStartPC);
   void *                 startPCIfAlreadyCompiled(J9VMThread *, TR::IlGeneratorMethodDetails & details, void *oldStartPC);

   static int32_t getCompThreadSuspensionThreshold(int32_t threadID) { return _compThreadSuspensionThresholds[threadID]; }

   // updateNumUsableCompThreads() is called before startCompilationThread() to update TR::Options::_numUsableCompilationThreads.
   // It makes sure the number of usable compilation threads is within allowed bounds.
   // If not, set it to the upper bound based on the mode: JITClient/non-JITServer or JITServer.
   void updateNumUsableCompThreads(int32_t &numUsableCompThreads);
   bool allocateCompilationThreads(int32_t numUsableCompThreads);
   void freeAllCompilationThreads();
   void freeAllResources();

   uintptr_t startCompilationThread(int32_t priority, int32_t id, bool isDiagnosticThread);
   bool initializeCompilationOnApplicationThread();
   bool  asynchronousCompilation();
   void stopCompilationThreads();
   void suspendCompilationThread();
   void resumeCompilationThread();
   void purgeMethodQueue(TR_CompilationErrorCode errorCode);
   void *compileMethod(J9VMThread * context, TR::IlGeneratorMethodDetails &details, void *oldStartPC,
      TR_YesNoMaybe async, TR_CompilationErrorCode *, bool *queued, TR_OptimizationPlan *optPlan);

   void *compileOnApplicationThread(J9VMThread * context, TR::IlGeneratorMethodDetails &details, void *oldStartPC,
                                    TR_CompilationErrorCode *,
                                    TR_OptimizationPlan *optPlan);
   void *compileOnSeparateThread(J9VMThread * context, TR::IlGeneratorMethodDetails &details, void *oldStartPC,
                                 TR_YesNoMaybe async, TR_CompilationErrorCode*,
                                 bool *queued, TR_OptimizationPlan *optPlan);
   void invalidateRequestsForUnloadedMethods(TR_OpaqueClassBlock *unloadedClass, J9VMThread * vmThread, bool hotCodeReplacement);
   void invalidateRequestsForNativeMethods(J9Class * clazz, J9VMThread * vmThread);
#if defined(J9VM_JIT_DYNAMIC_LOOP_TRANSFER)
   void *searchForDLTRecord(J9Method *method, int32_t bcIndex);
   void  insertDLTRecord(J9Method *method, int32_t bcIndex, void *dltEntry);
   void  cleanDLTRecordOnUnload();
   DLTTracking *getDLT_HT() const { return _dltHT; }
   void setDLT_HT(DLTTracking *dltHT) { _dltHT = dltHT; }
#else
   DLTTracking *getDLT_HT() const { return NULL; }
#endif // J9VM_JIT_DYNAMIC_LOOP_TRANSFER

   J9::RWMonitor *getClassUnloadMonitor() { return _classUnloadMonitor; }

   // Synchronize output to the vlog file (or stdout if there is no file)
   //
   void vlogAcquire();
   void vlogRelease();

   // Synchronize output to the rtlog file (or stdout if there is no file)
   //
   void rtlogAcquire();
   void rtlogRelease();

   // Synchronize updates to J9Method objects
   //
   void acquireCompilationLock();
   void releaseCompilationLock();

   TR::Monitor *createLogMonitor();
   void acquireLogMonitor();
   void releaseLogMonitor();

   // Handling of ordered compiles
   //
   void triggerOrderedCompiles(TR_FrontEnd *vm, intptr_t tickCount);

   int32_t getQueueWeight() const { return _queueWeight; }
   void increaseQueueWeightBy(uint8_t w) { _queueWeight += (int32_t)w; }
   int32_t decreaseQueueWeightBy(uint8_t w) { TR_ASSERT((int32_t)w <= _queueWeight, "assertion failure"); return _queueWeight -= (int32_t)w; }
   int32_t getOverallQueueWeight() const { return _queueWeight; /*+ (_LPQWeight >> 1);*/ } // make secondary queue count only half as much
   int32_t getMethodQueueSize() const { return _numQueuedMethods; }
   void incrementMethodQueueSize();
   int32_t getPeakMethodQueueSize() const { return _maxQueueSize; }
   int32_t getNumQueuedFirstTimeCompilations() const { return _numQueuedFirstTimeCompilations; }
   void decNumGCRReqestsQueued(TR_MethodToBeCompiled *entry);
   void incNumGCRRequestsQueued(TR_MethodToBeCompiled *entry);
   int32_t getNumGCRRequestsQueued() const { return _numGCRQueued; }
   void decNumInvReqestsQueued(TR_MethodToBeCompiled *entry);
   void incNumInvRequestsQueued(TR_MethodToBeCompiled *entry);
   void updateCompQueueAccountingOnDequeue(TR_MethodToBeCompiled *entry);
   int32_t getNumCompThreadsActive() const { return _numCompThreadsActive; }
   void    incNumCompThreadsActive() { _numCompThreadsActive++; }
   void    decNumCompThreadsActive() { _numCompThreadsActive--; }
   void    setNumCompThreadsActive(int32_t n) { _numCompThreadsActive = n; }
   int32_t getNumCompThreadsJobless() const { return _numCompThreadsJobless; }
   void    incNumCompThreadsJobless() { _numCompThreadsJobless++; }
   void    decNumCompThreadsJobless() { _numCompThreadsJobless--; }
   void    setNumCompThreadsJobless(int32_t n) { _numCompThreadsJobless = n; }
   int32_t getNumCompThreadsCompilingHotterMethods() const { return _numCompThreadsCompilingHotterMethods; }
   void incNumCompThreadsCompilingHotterMethods() { _numCompThreadsCompilingHotterMethods++; }
   void decNumCompThreadsCompilingHotterMethods() { _numCompThreadsCompilingHotterMethods--; TR_ASSERT(_numCompThreadsCompilingHotterMethods>=0, "assertion failure");}
   void setNumCompThreadsCompilingHotterMethods(int32_t n) { _numCompThreadsCompilingHotterMethods = n; }
   int32_t getNumAppThreadsActive() const { return _numAppThreadsActive; }
   uint64_t getElapsedTimeNumAppThreadsActiveWasSet() const { return _elapsedTimeNumAppThreadsActiveWasSet; }
   void setNumAppThreadsActive(int32_t n, uint64_t t) { _numAppThreadsActive = n; _elapsedTimeNumAppThreadsActiveWasSet = t; }
   bool    useOptLevelAdjustment();


   bool              getHasResumableTrapHandler() {return _flags.testAny(HasResumableTrapHandler);}
   void              setHasResumableTrapHandler(bool t=true)
      {
      if (t)
         _flags.set(HasResumableTrapHandler);
      else
         _flags.reset(HasResumableTrapHandler);
      }

   bool              getHasFixedFrameC_CallingConvention() {return _flags.testAny(HasFixedFrameC_CallingConvention);}
   void              setHasFixedFrameC_CallingConvention() { _flags.set(HasFixedFrameC_CallingConvention);}

   uint32_t           getNumberBytesReadInaccessible() {return _numberBytesReadInaccessible;}
   uint32_t           setNumberBytesReadInaccessible(int32_t r) { return (_numberBytesReadInaccessible = r); }

   uint32_t           getNumberBytesWriteInaccessible() {return _numberBytesWriteInaccessible;}
   uint32_t           setNumberBytesWriteInaccessible(int32_t w) { return (_numberBytesWriteInaccessible = w); }

   bool              getSupportsScaledIndexAddressing() {return _flags.testAny(SupportsScaledIndexAddressing);}
   void              setSupportsScaledIndexAddressing() { _flags.set(SupportsScaledIndexAddressing);}

   bool              isInZOSSupervisorState() {return _flags.testAny(IsInZOSSupervisorState);}
   void              setIsInZOSSupervisorState() { _flags.set(IsInZOSSupervisorState);}

   TR_LinkHead0<TR_ClassHolder> *getListOfClassesToCompile() { return &_classesToCompileList; }
   int32_t getCompilationLag();
   int32_t getCompilationLagUnlocked() { return getCompilationLag(); } // will go away
   bool    dynamicThreadPriority();

   void    updateCompilationErrorStats(TR_CompilationErrorCode errorCode) { statCompErrors.update(errorCode); }
   bool    SmoothCompilation(TR_MethodToBeCompiled *entry, int32_t *optLevelAdjustment);
   bool    compBudgetSupport() const {return _compBudgetSupport;}
   void    setCompBudgetSupport(bool val) {_compBudgetSupport = val;}
   int32_t getCompBudget() const;
   void    setCompBudget(int32_t budget) {_compilationBudget = budget;}
   int32_t idleThreshold() const {return _idleThreshold;}
   void    setIdleThreshold(int32_t threshold) {_idleThreshold = threshold;}
   int64_t getCpuTimeSpentInCompilation();
   int32_t changeCompThreadPriority(int32_t priority, int32_t locationCode);
   TR_MethodToBeCompiled *getFirstDowngradedMethod();
   uint64_t getLastReqStartTime() const {return _lastReqStartTime;}
   void     setLastReqStartTime(uint64_t t) { _lastReqStartTime = t; }
   CpuUtilization * getCpuUtil() const { return _cpuUtil;}
   void setCpuUtil(CpuUtilization *cpuUtil){ _cpuUtil = cpuUtil; }
   UDATA getVMStateOfCrashedThread() { return _vmStateOfCrashedThread; }
   void setVMStateOfCrashedThread(UDATA vmState) { _vmStateOfCrashedThread = vmState; }
   void printCompQueue();
   TR::CompilationInfoPerThread *getCompilationInfoForDumpThread() const { return _compInfoForDiagnosticCompilationThread; }
   TR::CompilationInfoPerThread * const *getArrayOfCompilationInfoPerThread() const { return _arrayOfCompilationInfoPerThread; }
   uint32_t getAotQueryTime() { return _statTotalAotQueryTime; }
   void     setAotQueryTime(uint32_t queryTime) { _statTotalAotQueryTime = queryTime; }
   uint32_t getAotRelocationTime() { return _statTotalAotRelocationTime; }
   void     setAotRelocationTime(uint32_t reloTime) { _statTotalAotRelocationTime = reloTime; }
   void    incrementNumMethodsFoundInSharedCache() { _numMethodsFoundInSharedCache++; }
   int32_t numMethodsFoundInSharedCache() { return _numMethodsFoundInSharedCache; }
   int32_t getNumInvRequestsInCompQueue() const { return _numInvRequestsInCompQueue; }
   TR::CompilationInfoPerThreadBase *getCompInfoForCompOnAppThread() const { return _compInfoForCompOnAppThread; }
   J9JITConfig *getJITConfig() { return _jitConfig; }
   TR::CompilationInfoPerThread *getCompInfoForThread(J9VMThread *vmThread);
   int32_t getNumUsableCompilationThreads() const { return _numCompThreads; }
   int32_t getNumTotalCompilationThreads() const { return _numCompThreads + _numDiagnosticThreads; }
   TR::CompilationInfoPerThreadBase *getCompInfoWithID(int32_t ID);
   TR::Compilation *getCompilationWithID(int32_t ID);
   TR::CompilationInfoPerThread *getFirstSuspendedCompilationThread();
   bool useMultipleCompilationThreads() { return (getNumUsableCompilationThreads() + _numDiagnosticThreads) > 1; }
   bool getRampDownMCT() const { return _rampDownMCT; }
   void setRampDownMCT() { _rampDownMCT = true; } // cannot be reset
   static void printMethodNameToVlog(J9Method *method);
   static void printMethodNameToVlog(const J9ROMClass* romClass, const J9ROMMethod* romMethod);

   void queueForcedAOTUpgrade(TR_MethodToBeCompiled *originalEntry);

   void queueForcedAOTUpgrade(TR_MethodToBeCompiled *originalEntry, uint16_t hints, TR_FrontEnd *fe);

   uint32_t getNumTargetCPUs() const { return _cpuEntitlement.getNumTargetCPUs(); }

   bool isHypervisorPresent() { return _cpuEntitlement.isHypervisorPresent(); }
   double getGuestCpuEntitlement() const { return _cpuEntitlement.getGuestCpuEntitlement(); }
   void computeAndCacheCpuEntitlement() { _cpuEntitlement.computeAndCacheCpuEntitlement(); }
   double getJvmCpuEntitlement() const { return _cpuEntitlement.getJvmCpuEntitlement(); }

   void setProcessorByDebugOption();

   bool importantMethodForStartup(J9Method *method);
   bool shouldDowngradeCompReq(TR_MethodToBeCompiled *entry);


   int32_t computeDynamicDumbInlinerBytecodeSizeCutoff(TR::Options *options);
   TR_YesNoMaybe shouldActivateNewCompThread();
#if DEBUG
   void debugPrint(char *debugString);
   void debugPrint(J9VMThread *, char *);
   void debugPrint(char *, intptr_t);
   void debugPrint(J9VMThread *, char *, IDATA);
   void debugPrint(J9Method*);
   void debugPrint(char *, J9Method *);
   void debugPrint(J9VMThread *, char *, TR_MethodToBeCompiled *);
   void debugPrint(char * debugString, TR::IlGeneratorMethodDetails & details, J9VMThread * vmThread);
   void printQueue();
#else // !DEBUG
   void debugPrint(char *debugString) {}
   void debugPrint(J9VMThread *, char *) {}
   void debugPrint(char *, intptr_t) {}
   void debugPrint(J9VMThread *, char *, IDATA) {}
   void debugPrint(J9Method*) {}
   void debugPrint(char *, J9Method *) {}
   void debugPrint(J9VMThread *, char *, TR_MethodToBeCompiled *) {}
   void debugPrint(char * debugString, TR::IlGeneratorMethodDetails & details, J9VMThread * vmThread) { }
   void printQueue() {}
#endif // DEBUG
   void debugPrint(TR_OpaqueMethodBlock *omb){ debugPrint((J9Method*)omb); }

   enum {SMALL_LAG=1, MEDIUM_LAG, LARGE_LAG};
   bool methodCanBeCompiled(TR_FrontEnd *, TR_ResolvedMethod *compilee, TR_FilterBST *&filter);

   void emitJvmpiExtendedDataBuffer(TR::Compilation *&compiler, J9VMThread *vmThread, J9Method *&_method, TR_MethodMetaData *metaData);
   void emitJvmpiCallSites(TR::Compilation *&compiler, J9VMThread *vmThread, J9Method *&_method);

   void setAllCompilationsShouldBeInterrupted();

   int32_t getIprofilerMaxCount() const { return _iprofilerMaxCount; }
   void setIprofilerMaxCount(int32_t n) { _iprofilerMaxCount = n; }

   bool compTracingEnabled() const { return _compilationTracingFacility.isInitialized(); }
   void addCompilationTraceEntry(J9VMThread * vmThread, TR_CompilationOperations op, uint32_t otherData=0);

   TR_SharedCacheRelocationRuntime  *reloRuntime() { return &_sharedCacheReloRuntime; }

   int32_t incNumSeriousFailures() { return ++_numSeriousFailures; } // no atomicity guarantees for the increment

   TR_SamplerStates getSamplerState() const { return _samplerState; }
   void setSamplerState(TR_SamplerStates s) { _prevSamplerState = _samplerState;  _samplerState = s; }
   TR_SamplerStates getPrevSamplerState() const { return _prevSamplerState; }


   J9VMThread *getSamplerThread() const { return _samplerThread; }
   void setSamplerThread(J9VMThread *thr) { _samplerThread = thr; }
   // use jitConfig->samplerMonitor for the following two routines
   TR_SamplingThreadLifetimeStates getSamplingThreadLifetimeState() const { return _samplingThreadLifetimeState; }
   void setSamplingThreadLifetimeState(TR_SamplingThreadLifetimeStates s) { _samplingThreadLifetimeState = s; }
   int32_t getSamplingThreadWaitTimeInDeepIdleToNotifyVM() const { return _samplingThreadWaitTimeInDeepIdleToNotifyVM; }
   void setSamplingThreadWaitTimeInDeepIdleToNotifyVM()
      {
      int32_t minIdleWaitTimeToNotifyVM = _jitConfig->javaVM->internalVMFunctions->getVMMinIdleWaitTime(_jitConfig->javaVM);

      if (minIdleWaitTimeToNotifyVM == 0)
         _samplingThreadWaitTimeInDeepIdleToNotifyVM = -1;
      else
         {
         if (minIdleWaitTimeToNotifyVM <= TR::Options::_waitTimeToEnterDeepIdleMode)
            _samplingThreadWaitTimeInDeepIdleToNotifyVM = 0;
         else
            _samplingThreadWaitTimeInDeepIdleToNotifyVM = minIdleWaitTimeToNotifyVM - TR::Options::_waitTimeToEnterDeepIdleMode;
         }
      }

   TR::CompilationInfoPerThread * findFirstLowPriorityCompilationInProgress(CompilationPriority priority); // needs compilationQueueMonitor in hand

   TR_YesNoMaybe isWarmSCC() const { return _warmSCC; }
   void setIsWarmSCC(TR_YesNoMaybe param) { _warmSCC = param; }
#if defined(J9VM_OPT_SHARED_CLASSES)
   J9SharedClassJavacoreDataDescriptor* getAddrOfJavacoreData() { return &_javacoreData; }
#endif
   TR::Monitor *getIProfilerBufferArrivalMonitor() const { return _iprofilerBufferArrivalMonitor; }

   TR_HWProfiler *getHWProfiler() const;

   TR_JProfilerThread *getJProfilerThread() const;

   int32_t calculateCodeSize(TR_MethodMetaData *metaData);
   void increaseUnstoredBytes(U_32 aotBytes, U_32 jitBytes);

   /**
   * @brief Compute free physical memory taking into account container limits
   *
   * @param incompleteInfo   [OUTPUT] Boolean indicating that cached/buffered memory couldn't be read
   * @return                 A value representing the free physicalMemory
                             or OMRPORT_MEMINFO_NOT_AVAILABLE in case of error
   */
   uint64_t computeFreePhysicalMemory(bool &incompleteInfo);

   /**
   * @brief Compute free physical memory taking into account container limits and caches it for later use
   *
   * To limit overhead, this function caches the computed value and
   * only refreshes it if more than "updatePeriodMs" have passed since the last update
   * If updatePeriodMs==-1, then updatePeriodMs uses a default value of 500 ms
   *
   * @param incompleteInfo   [OUTPUT] Boolean indicating that cached/buffered memory couldn't be read
   * @param updatePeriodMs   Indicates how often the cached values are refreshed
   * @return                 A value representing the free physicalMemory
                             or OMRPORT_MEMINFO_NOT_AVAILABLE in case of error
   */
   uint64_t computeAndCacheFreePhysicalMemory(bool &incompleteInfo, int64_t updatePeriodMs=-1);
   uint64_t computeFreePhysicalLimitAndAbortCompilationIfLow(TR::Compilation *comp, bool &incompleteInfo, size_t sizeToAllocate);

   TR_LowPriorityCompQueue &getLowPriorityCompQueue() { return _lowPriorityCompilationScheduler; }
   bool canProcessLowPriorityRequest();
   TR_CompilationErrorCode scheduleLPQAndBumpCount(TR::IlGeneratorMethodDetails &details, TR_J9VMBase *fe);

   TR_JProfilingQueue &getJProfilingCompQueue() { return _JProfilingQueue; }

   TR_JitSampleInfo &getJitSampleInfoRef() { return _jitSampleInfo; }
   TR_InterpreterSamplingTracking *getInterpSamplTrackingInfo() const { return _interpSamplTrackingInfo; }

   int32_t getAppSleepNano() const { return _appSleepNano; }
   void setAppSleepNano(int32_t t) { _appSleepNano = t; }
   int32_t computeAppSleepNano() const;
   TR_YesNoMaybe detectCompThreadStarvation();
   bool getStarvationDetected() const { return _starvationDetected; }
   void setStarvationDetected(bool b) { _starvationDetected = b; }
   int32_t getTotalCompThreadCpuUtilWhenStarvationComputed() const { return _totalCompThreadCpuUtilWhenStarvationComputed; }
   int32_t getNumActiveCompThreadsWhenStarvationComputed() const { return _numActiveCompThreadsWhenStarvationComputed; }
   bool canProcessJProfilingRequest();
   bool getSuspendThreadDueToLowPhysicalMemory() const { return _suspendThreadDueToLowPhysicalMemory; }
   void setSuspendThreadDueToLowPhysicalMemory(bool b) { _suspendThreadDueToLowPhysicalMemory = b; }

#if defined(J9VM_OPT_JITSERVER)
   ClientSessionHT *getClientSessionHT() const { return _clientSessionHT; }
   void setClientSessionHT(ClientSessionHT *ht) { _clientSessionHT = ht; }

   PersistentVector<TR_OpaqueClassBlock*> *getUnloadedClassesTempList() const { return _unloadedClassesTempList; }
   void setUnloadedClassesTempList(PersistentVector<TR_OpaqueClassBlock*> *it) { _unloadedClassesTempList = it; }
   PersistentVector<TR_OpaqueClassBlock*> *getIllegalFinalFieldModificationList() const { return _illegalFinalFieldModificationList; }
   void setIllegalFinalFieldModificationList(PersistentVector<TR_OpaqueClassBlock*> *it) { _illegalFinalFieldModificationList = it; }
   PersistentUnorderedMap<TR_OpaqueClassBlock*, uint8_t> *getNewlyExtendedClasses() const { return _newlyExtendedClasses; }
   void classGotNewlyExtended(TR_OpaqueClassBlock* clazz)
      {
      auto &newlyExtendedClasses = *getNewlyExtendedClasses();
      uint8_t inProgress = getCHTableUpdateDone();
      if (inProgress)
         newlyExtendedClasses[clazz] |= inProgress;
      }
   void setNewlyExtendedClasses(PersistentUnorderedMap<TR_OpaqueClassBlock*, uint8_t> *it) { _newlyExtendedClasses = it; }

   TR::Monitor *getSequencingMonitor() const { return _sequencingMonitor; }
   uint32_t getCompReqSeqNo() const { return _compReqSeqNo; }
   uint32_t incCompReqSeqNo() { return ++_compReqSeqNo; }
   void markCHTableUpdateDone(uint8_t threadId) { _chTableUpdateFlags |= (1 << threadId); }
   void resetCHTableUpdateDone(uint8_t threadId) { _chTableUpdateFlags &= ~(1 << threadId); }
   uint8_t getCHTableUpdateDone() const { return _chTableUpdateFlags; }

   const PersistentVector<std::string> &getJITServerSslKeys() const { return _sslKeys; }
   void  addJITServerSslKey(const std::string &key) { _sslKeys.push_back(key); }
   const PersistentVector<std::string> &getJITServerSslCerts() const { return _sslCerts; }
   void  addJITServerSslCert(const std::string &cert) { _sslCerts.push_back(cert); }
   const std::string &getJITServerSslRootCerts() const { return _sslRootCerts; }
   void  setJITServerSslRootCerts(const std::string &cert) { _sslRootCerts = cert; }
#endif /* defined(J9VM_OPT_JITSERVER) */

   static void replenishInvocationCount(J9Method* method, TR::Compilation* comp);

   static void addJ9HookVMDynamicCodeLoadForAOT(J9VMThread * vmThread, J9Method * method, J9JITConfig *jitConfig, TR_MethodMetaData* relocatedMetaData);

#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   static void storeAOTInSharedCache(J9VMThread *vmThread, J9ROMMethod *romMethod, const U_8 *dataStart, UDATA dataSize, const U_8 *codeStart, UDATA codeSize, TR::Compilation *comp, J9JITConfig *jitConfig, TR_MethodToBeCompiled *entry);
#endif

   static int32_t         VERY_SMALL_QUEUE;
   static int32_t         SMALL_QUEUE;
   static int32_t         MEDIUM_LARGE_QUEUE;
   static int32_t         LARGE_QUEUE;
   static int32_t         VERY_LARGE_QUEUE;

   static const char     *OperationNames[];

   struct CompilationStatistics _stats;
   struct CompilationStatsPerInterval _intervalStats;
   TR_PersistentArray<TR_SignatureCountPair *> *_persistedMethods;

   // Must be less than 8 at the JITClient or non-JITServer mode.
   // Because in some parts of the code (CHTable) we keep flags on a byte variable.
   static const uint32_t MAX_CLIENT_USABLE_COMP_THREADS = 7;  // For JITClient and non-JITServer mode
   static const uint32_t MAX_SERVER_USABLE_COMP_THREADS = 63; // JITServer
   static const uint32_t MAX_DIAGNOSTIC_COMP_THREADS = 1;

private:

   enum
      {
      JITA2N_EXTENDED_DATA_EYE_CATCHER      = 0xCCCCCCCC,
      JITA2N_EXTENDED_DATA_LINENUM          = 0xBEEFCAFE,   // Picked early and used... so we'll leave it
      JITA2N_EXTENDED_DATA_CALLSITES        = 0xCAFE0002,
      JITA2N_EXTENDED_DATA_INLINETABLE      = 0xCAFE0003,
      JITA2N_EXTENDED_DATA_COMPILATION_DESC = 0xCAFE0004
      };

   /**
    * @brief getCompilationQueueEntry gets a TR_MethodToBeCompiled for use in queuing compilations
    * @return Returns an unused queue entry if available, or NULL otherwise.
    */
   TR_MethodToBeCompiled * getCompilationQueueEntry();

   int bufferSizeCompilationAttributes();
   uint8_t * bufferPopulateCompilationAttributes(U_8 *buffer, TR::Compilation *&compiler, TR_MethodMetaData *metaData);
   int bufferSizeInlinedCallSites(TR::Compilation *&compiler, TR_MethodMetaData *metaData);
   uint8_t * bufferPopulateInlinedCallSites(uint8_t * buffer, TR::Compilation *&compiler, TR_MethodMetaData *metaData);
   int bufferSizeLineNumberTable(TR::Compilation *&compiler, TR_MethodMetaData *metaData, J9Method *&_method);
   uint8_t * bufferPopulateLineNumberTable(uint8_t *buffer, TR::Compilation *&compiler, TR_MethodMetaData *metaData, J9Method *&_method);

   J9Method *getRamMethod(TR_FrontEnd *vm, char *className, char *methodName, char *signature);
   //char *buildMethodString(TR_ResolvedMethod *method);

   static const size_t DLT_HASHSIZE = 123;

   static TR::CompilationInfo * _compilationRuntime;

   static int32_t *_compThreadActivationThresholds;
   static int32_t *_compThreadSuspensionThresholds;
   static int32_t *_compThreadActivationThresholdsonStarvation;

   TR::CompilationInfoPerThread **_arrayOfCompilationInfoPerThread; // First NULL entry means end of the array
   TR::CompilationInfoPerThread *_compInfoForDiagnosticCompilationThread; // compinfo for dump compilation thread
   TR::CompilationInfoPerThreadBase *_compInfoForCompOnAppThread; // This is NULL for separate compilation thread
   TR_MethodToBeCompiled *_methodQueue;
   TR_MethodToBeCompiled *_methodPool;
   int32_t                _methodPoolSize; // shouldn't this and _methodPool be static?

   J9JITConfig           *_jitConfig;
   TR_PersistentMemory   *_persistentMemory;  // memorize the persistentMemory
   TR::Monitor *_compilationMonitor;
   J9::RWMonitor *_classUnloadMonitor;
   TR::Monitor *_logMonitor; // only used if multiple compilation threads
   TR::Monitor *_schedulingMonitor;
#if defined(J9VM_JIT_DYNAMIC_LOOP_TRANSFER)
   TR::Monitor *_dltMonitor;
   struct DLT_record     *_freeDLTRecord;
   struct DLT_record     *_dltHash[DLT_HASHSIZE];
#endif
   DLTTracking           *_dltHT;

   TR::Monitor *_vlogMonitor;
   TR::Monitor *_rtlogMonitor;
   TR::Monitor *_iprofilerBufferArrivalMonitor;
   TR::Monitor *_applicationThreadMonitor;
   TR::MonitorTable *_j9MonitorTable; // used only for RAS (debuggerExtensions); no accessor; use TR_J9MonitorTable::get() everywhere else
   TR_LinkHead0<TR_ClassHolder> _classesToCompileList; // used by compileClasses; adjusted by unload hooks
   intptr_t               _numSyncCompilations;
   intptr_t               _numAsyncCompilations;
   int32_t                _numCompThreadsActive;
   int32_t                _numCompThreadsJobless; // threads are not suspended, but have no work to do
   int32_t                _numCompThreadsCompilingHotterMethods; // allow only one at a time; use compQmonitor to change
   int32_t                _numAppThreadsActive; // updated by sampling thread once in a while
   uint64_t               _elapsedTimeNumAppThreadsActiveWasSet; // ms; maintained by sampling thread
   int32_t                _numQueuedMethods;
   int32_t                _maxQueueSize;
   int32_t                _numQueuedFirstTimeCompilations; // these have oldStartPC==0
   int32_t                _queueWeight; // approximation on overhead to process the entire queue
   CpuUtilization*        _cpuUtil; // object to compute cpu utilization
   int32_t                _overallCompCpuUtilization; // In percentage points. Valid only if TR::Options::_compThreadCPUEntitlement has a positive value
   int32_t                _idleThreshold; // % of entire machine CPU
   int32_t                _compilationBudget;
   TR_YesNoMaybe          _warmSCC; // shared class cache has methods, so this could be second run
   bool                   _compBudgetSupport;
   bool                   _rampDownMCT; // flag that from now on we should not activate more than one compilation thread
                                        // Once set, the flag is never reset
   TR_YesNoMaybe          _exceedsCompCpuEntitlement;
   J9VMThread            *_samplerThread; // The Os thread for this VM attached thread is stored at jitConfig->samplerThread
   TR_SamplerStates       _samplerState; // access is guarded by J9JavaVM->vmThreadListMutex
   TR_SamplerStates       _prevSamplerState; // previous state of the sampler thread
   volatile TR_SamplingThreadLifetimeStates _samplingThreadLifetimeState; // access guarded by jitConfig->samplerMonitor (most of the time)
   int32_t                _samplingThreadWaitTimeInDeepIdleToNotifyVM;
   int32_t                _numMethodsFoundInSharedCache;
   int32_t                _numInvRequestsInCompQueue; // number of invalidation requests present in the compilation queue
   uint64_t               _lastReqStartTime; // time (ms) when processing the last request started
   uint64_t               _lastCompilationsShouldBeInterruptedTime; // RAS
// statistics
   int32_t                _statsOptLevels[numHotnessLevels]; // will be zeroed with memset
   uint32_t               _statNumAotedMethods;
   uint32_t               _statNumMethodsFromSharedCache; // methods whose body was taken from shared cache
   uint32_t               _statNumAotedMethodsRecompiled;
   uint32_t               _statNumForcedAotUpgrades;
   uint32_t               _statNumJNIMethodsCompiled;
   TR_StatsEvents<compilationMaxError> statCompErrors;
   uint32_t               _statNumPriorityChanges; // statistics
   uint32_t               _statNumYields;
   uint32_t               _statNumUpgradeInterpretedMethod;
   uint32_t               _statNumDowngradeInterpretedMethod;
   uint32_t               _statNumUpgradeJittedMethod;
   uint32_t               _statNumQueuePromotions;
   uint32_t               _statNumGCRInducedCompilations;
   uint32_t               _statNumSamplingJProfilingBodies;
   uint32_t               _statNumJProfilingBodies;
   uint32_t               _statNumRecompilationForBodiesWithJProfiling;
   uint32_t               _statNumMethodsFromJProfilingQueue;
   uint32_t               _statTotalAotQueryTime;
   uint32_t               _statTotalAotRelocationTime;

   uint32_t               _numberBytesReadInaccessible;
   uint32_t               _numberBytesWriteInaccessible;
   flags32_t              _flags;
   int32_t                _numSeriousFailures; // count failures where method needs to continue interpreted

   TR::Monitor           *_gpuInitMonitor;

   enum // flags
      {
      //                           = 0x00000001,     // AVAILABLE FOR USE !!
      HasResumableTrapHandler          = 0x00000002,
      HasFixedFrameC_CallingConvention = 0x00000004,
      SupportsScaledIndexAddressing    = 0x00000080,
      S390SupportsDFP                  = 0x00000100,
      S390SupportsFPE                  = 0x00000200,
      IsInZOSSupervisorState           = 0x00008000,
      S390SupportsTM                   = 0x00010000,
      S390SupportsRI                   = 0x00020000,
      S390SupportsVectorFacility       = 0x00040000,
      DummyLastFlag
      };

#ifdef DEBUG
   bool                   _traceCompiling;
#endif
   int32_t                _numCompThreads; // Number of usable compilation threads that does not include the diagnostic thread
   int32_t                _numDiagnosticThreads;
   int32_t                _iprofilerMaxCount;
   int32_t                _numGCRQueued; // how many GCR requests are in the queue
                                         // We should disable GCR counting if too many
                                         // GCR requests because GCR counting has a large
                                         // negative effect on performance
   //----------------
   int32_t                _appSleepNano; // make app threads sleep when sampling

   bool                   _starvationDetected;
   int32_t                _totalCompThreadCpuUtilWhenStarvationComputed;   // for RAS purposes
   int32_t                _numActiveCompThreadsWhenStarvationComputed; // for RAS purposes
   //--------------
   TR_LowPriorityCompQueue _lowPriorityCompilationScheduler;
   TR_JProfilingQueue      _JProfilingQueue;

   TR::CompilationTracingFacility _compilationTracingFacility; // Must be initialized before using
   TR_CpuEntitlement _cpuEntitlement;
   TR_JitSampleInfo  _jitSampleInfo;
   TR_SharedCacheRelocationRuntime _sharedCacheReloRuntime;
   uintptr_t _vmStateOfCrashedThread; // Set by Jit Dump; used by diagnostic thread
#if defined(J9VM_OPT_SHARED_CLASSES)
   J9SharedClassJavacoreDataDescriptor _javacoreData;
#endif
   uint64_t _cachedFreePhysicalMemoryB;
   bool _cachedIncompleteFreePhysicalMemory;
   bool _cgroupMemorySubsystemEnabled; // true when running in container and the memory subsystem is enabled
   // The following flag is set when the JIT is not allowed to allocate
   // a scratch segment due to low physical memory.
   // It is reset when a compilation thread is suspended, thus possibly
   // freeing scratch segments it holds to
   bool _suspendThreadDueToLowPhysicalMemory;
   TR_InterpreterSamplingTracking *_interpSamplTrackingInfo;

#if defined(J9VM_OPT_JITSERVER)
   ClientSessionHT               *_clientSessionHT; // JITServer hashtable that holds session information about JITClients
   PersistentVector<TR_OpaqueClassBlock*> *_unloadedClassesTempList; // JITServer list of classes unloaded
   PersistentVector<TR_OpaqueClassBlock*> *_illegalFinalFieldModificationList; // JITServer list of classes that have J9ClassHasIllegalFinalFieldModifications is set
   TR::Monitor                   *_sequencingMonitor; // Used for ordering outgoing messages at the client
   uint32_t                      _compReqSeqNo; // seqNo for outgoing messages at the client
   PersistentUnorderedMap<TR_OpaqueClassBlock*, uint8_t> *_newlyExtendedClasses; // JITServer table of newly extended classes
   uint8_t                       _chTableUpdateFlags;
   uint32_t                      _localGCCounter; // Number of local gc cycles done
   std::string                   _sslRootCerts;
   PersistentVector<std::string> _sslKeys;
   PersistentVector<std::string> _sslCerts;
#endif /* defined(J9VM_OPT_JITSERVER) */
   }; // CompilationInfo
}


#endif // COMPILATIONRUNTIME_HPP

