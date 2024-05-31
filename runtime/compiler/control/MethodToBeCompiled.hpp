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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef METHODTOBECOMPILED_HPP
#define METHODTOBECOMPILED_HPP

#pragma once

#include "j9.h"
#if defined(J9VM_OPT_JITSERVER)
#include "compile/CompilationTypes.hpp"
#endif /* defined(J9VM_OPT_JITSERVER) */
#include "control/CompilationPriority.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"


#define MAX_COMPILE_ATTEMPTS 3

#define ENTRY_INITIALIZED      0x1
#define ENTRY_QUEUED           0x2
#define ENTRY_IN_POOL_NOT_FREE 0x4 // comp thread finished and put the entry back in the pool
#define ENTRY_IN_POOL_FREE     0x8
#define ENTRY_DEALLOCATED      0x10

namespace TR { class CompilationInfoPerThreadBase; }
class TR_OptimizationPlan;
#if defined(J9VM_OPT_JITSERVER)
namespace JITServer { class ServerStream; }
#endif /* defined(J9VM_OPT_JITSERVER) */
namespace TR { class Monitor; }
struct J9JITConfig;
struct J9VMThread;

struct TR_MethodToBeCompiled
   {
   enum LPQ_REASON
      {
      REASON_NONE = 0,
      REASON_IPROFILER_CALLS,
      REASON_LOW_COUNT_EXPIRED,
      REASON_UPGRADE,
#if defined(J9VM_OPT_JITSERVER)
      REASON_SERVER_UNAVAILABLE
#endif
      };

   static int16_t _globalIndex;
   static TR_MethodToBeCompiled *allocate(J9JITConfig *jitConfig);
   void shutdown();

   void initialize(TR::IlGeneratorMethodDetails &details, void *oldStartPC,
                   CompilationPriority priority, TR_OptimizationPlan *optimizationPlan);

   TR::Monitor *getMonitor() { return _monitor; }
   TR::IlGeneratorMethodDetails &getMethodDetails() const { return *_methodDetails; }
   bool isDLTCompile() const { return getMethodDetails().isMethodInProgress(); }
   bool isCompiled() const;
   bool isJNINative() const;
   void acquireSlotMonitor(J9VMThread *vmThread);
   void releaseSlotMonitor(J9VMThread *vmThread);
   void setAotCodeToBeRelocated(const void *m);
   bool isAotLoad() const { return _doAotLoad; }
#if defined(J9VM_OPT_JITSERVER)
   bool isRemoteCompReq() const { return _remoteCompReq; } // at the client
   void setRemoteCompReq() { _remoteCompReq = true; }
   void unsetRemoteCompReq() { _remoteCompReq = false; }
   bool isOutOfProcessCompReq() const { return _stream != NULL; } // at the server
   uint64_t getClientUID() const;
   bool hasChangedToLocalSyncComp() const { return _origOptLevel != unknownHotness; }
   void setShouldUpgradeOutOfProcessCompilation() { _shouldUpgradeOutOfProcessCompilation = true; }
   bool shouldUpgradeOutOfProcessCompilation() const { return _shouldUpgradeOutOfProcessCompilation; }
#else /* defined(J9VM_OPT_JITSERVER) */
   bool isRemoteCompReq() const { return false; } // at the client
   bool isOutOfProcessCompReq() const { return false; } // at the server
#endif /* defined(J9VM_OPT_JITSERVER) */

   TR_MethodToBeCompiled *_next;
   TR::IlGeneratorMethodDetails _methodDetailsStorage;
   TR::IlGeneratorMethodDetails *_methodDetails;
   void                  *_oldStartPC;
   void                  *_newStartPC;
   TR::Monitor           *_monitor;
   char                   _monitorName[28]; // to be able to deallocate the string
                                            // "JIT-QueueSlotMonitor-N" for 16-bit index N fits into 28 characters
   TR_OptimizationPlan   *_optimizationPlan;
   // Timestamp of when the request was added to the queue (microseconds). Only set when the TR_VerbosePerformance
   // option (i.e. -Xjit:verbose=compilePerformance) is enabled. Note that this timestamp is not reset when the
   // request is re-queued after a failed compilation. Once the compilation is finally successful, the timestamp
   // is used to compute the total compilation request latency (including queuing time and failed attempts).
   uintptr_t              _entryTime;
   TR::CompilationInfoPerThreadBase *_compInfoPT; // pointer to the thread that is handling this request
   const void *           _aotCodeToBeRelocated;

   uint16_t /*CompilationPriority*/_priority;
   int16_t                _numThreadsWaiting;
   int8_t                 _compilationAttemptsLeft;
   int8_t                 _compErrCode;
   int8_t                 _methodIsInSharedCache;/*TR_YesNoMaybe*/
   uint8_t                _reqFromSecondaryQueue; // This is actually of LPQ_REASON type

   // list of flags
   bool                   _reqFromJProfilingQueue;
   bool                   _unloadedMethod; // flag set by the GC thread during unloading
                                           // need to have vmaccess for accessing this flag
   bool                   _doAotLoad;// used for AOT shared cache
   bool                   _useAotCompilation;// used for AOT shared cache
   bool                   _doNotAOTCompile; // Set when we want to forbid all future compilation attempts
                                            // from being AOT compilations (loads or stores).
   bool                   _tryCompilingAgain;

   bool                   _async;           // flag for async compilation; used to print in vlog
   bool                   _changedFromAsyncToSync; // to prevent DLT compiling again and again we flag
                                                   // the normal requests for which a DLT request also
                                                   // exists, so that any further normal compilation
                                                   // requests for the same method will be performed
                                                   // synchronously
   bool                   _entryShouldBeDeallocated; // when set, the thread requesting the compilation
                                                     // should deallocate the entry
                                                     // It is set only when stopping compilation thread
   bool                   _entryIsCountedAsInvRequest; // when adding a request to the queue, if this is
                                                       // an invalidation request, flag the entry and
                                                       // increment a counter. Note that this flag is solely
                                                       // used for proper counting. An entry could have been
                                                       // queued as a normal request and later on be
                                                       // transformed into a INV request. The flag is only set
                                                       // if the request started as an INV request
   bool                   _GCRrequest; // Needed to be able to decrement the number of GCR requests in the queue
                                       // The flag in methodInfo is not enough because it may indicate true when
                                       // the entry is queued, but change afterwards if method receives samples
                                       // to be upgraded to hot or scorching
   bool                   _hasIncrementedNumCompThreadsCompilingHotterMethods;

   int16_t                _index;
   uint8_t                _freeTag; // temporary to catch a nasty bug
   uint8_t                _weight; // Up to 256 levels of weight
   uint8_t                _jitStateWhenQueued;

   bool                   _checkpointInProgress;

#if defined(J9VM_OPT_JITSERVER)
   // Comp request should be sent remotely to JITServer
   bool _remoteCompReq;
   // Flag used to determine whether a cold local compilation should be upgraded by LPQ
   bool _shouldUpgradeOutOfProcessCompilation;
   // Set at the client after a failed AOT deserialization or load.
   // If set, the client will not request an AOT cache store or load on the next compilation attempt.
   // In particular, _useAOTCacheCompilation will be false if this is true.
   bool _doNotLoadFromJITServerAOTCache;
   // Set at the client in preCompilationTasks; will cause the client to request that the
   // method be stored in the JITServer's AOT cache. Only used when getJITServerAOTCacheIgnoreLocalSCC()
   // is true.
   bool _useAOTCacheCompilation;
   // Cache original optLevel when transforming a remote sync compilation to a local cheap one
   TR_Hotness _origOptLevel;
   // A non-NULL field denotes an out-of-process compilation request
   JITServer::ServerStream *_stream;
#endif /* defined(J9VM_OPT_JITSERVER) */
   }; // TR_MethodToBeCompiled

#endif // METHODTOBECOMPILED_HPP
