/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#ifndef METHODTOBECOMPILED_HPP
#define METHODTOBECOMPILED_HPP

#pragma once

#include "control/CompilationPriority.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"

#define MAX_COMPILE_ATTEMPTS       3

#define ENTRY_INITIALIZED      0x1
#define ENTRY_QUEUED           0x2
#define ENTRY_IN_POOL_NOT_FREE 0x4 // comp thread finished and put the entry back in the pool
#define ENTRY_IN_POOL_FREE     0x8
#define ENTRY_DEALLOCATED      0x10

namespace TR { class CompilationInfoPerThreadBase; }
class TR_OptimizationPlan;
namespace TR { class Monitor; }
struct J9JITConfig;
struct J9VMThread;

struct TR_MethodToBeCompiled
   {
   enum LPQ_REASON { REASON_NONE = 0, REASON_IPROFILER_CALLS, REASON_LOW_COUNT_EXPIRED, REASON_UPGRADE };
   static int16_t _globalIndex;
   static TR_MethodToBeCompiled *allocate(J9JITConfig *jitConfig);
   void shutdown();

   void initialize(TR::IlGeneratorMethodDetails & details, void *oldStartPC, CompilationPriority p, TR_OptimizationPlan *optimizationPlan);

   TR::Monitor *getMonitor() { return _monitor; }
   TR::IlGeneratorMethodDetails & getMethodDetails(){ return *_methodDetails; }
   bool isDLTCompile()   { return getMethodDetails().isMethodInProgress(); }
   bool isCompiled();
   bool isJNINative();
   void acquireSlotMonitor(J9VMThread *vmThread);
   void releaseSlotMonitor(J9VMThread *vmThread);
   void setAotCodeToBeRelocated(const void *m);


   TR_MethodToBeCompiled *_next;
   TR::IlGeneratorMethodDetails _methodDetailsStorage;
   TR::IlGeneratorMethodDetails *_methodDetails;
   void                  *_oldStartPC;
   void                  *_newStartPC;
   TR::Monitor *_monitor;
   char                   _monitorName[30]; // to be able to deallocate the string
   TR_OptimizationPlan   *_optimizationPlan;
   uint64_t              _entryTime; // time it was added to the queue (ms)
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
   bool                   _useAotCompilation;// used for AOT shared cache
   bool                   _doNotUseAotCodeFromSharedCache;
   bool                   _tryCompilingAgain;

   bool                   _async;           // flag for async compilation; used to print in vlog
   bool                   _changedFromAsyncToSync; // to prevent DLT compiling again and again we flag
                                                   // the normal requests for which a DLT request also
                                                   // exists, so that any further normal compilation
                                                   // requests for the same method will be performed
                                                   // synchronously
   bool                   _entryShouldBeDeallocated;  // when set, the thread requesting the compilation
                                                      // should deallocate the entry
                                                      // It is set only when stopping compilation thread
   bool                  _entryIsCountedAsInvRequest; // when adding a request to the queue, if this is
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
   int16_t                _index;
   uint8_t                _freeTag; // temporary to catch a nasty bug
   uint8_t                _weight; // Up to 256 levels of weight
   bool                   _hasIncrementedNumCompThreadsCompilingHotterMethods;
   uint8_t                _jitStateWhenQueued;
   }; // TR_MethodToBeCompiled


#endif // METHODTOBECOMPILED_HPP

