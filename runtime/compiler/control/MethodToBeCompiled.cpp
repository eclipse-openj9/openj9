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

#include "control/MethodToBeCompiled.hpp"
#include "infra/MonitorTable.hpp"
#include "control/OptimizationPlan.hpp"
#include "control/CompilationRuntime.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "j9.h"
#if defined(J9VM_OPT_JITSERVER)
#include "control/CompilationThread.hpp"
#include "net/ServerStream.hpp"
#endif /* defined(J9VM_OPT_JITSERVER) */

int16_t TR_MethodToBeCompiled::_globalIndex = 0;

TR_MethodToBeCompiled *TR_MethodToBeCompiled::allocate(J9JITConfig *jitConfig)
   {
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);
   TR_MethodToBeCompiled *entry = (TR_MethodToBeCompiled*)
      j9mem_allocate_memory(sizeof(TR_MethodToBeCompiled), J9MEM_CATEGORY_JIT);
   if (entry == NULL)  // Memory Allocation Failure.
      return NULL;

   entry->_index = _globalIndex++;
   sprintf(entry->_monitorName, "JIT-QueueSlotMonitor-%d", (int32_t)entry->_index);
   entry->_monitor = TR::Monitor::create(entry->_monitorName);
   if (!entry->_monitor)
      {
      j9mem_free_memory(entry);
      return NULL;
      }
   return entry;
   }

void TR_MethodToBeCompiled::initialize(TR::IlGeneratorMethodDetails & details, void *oldStartPC, CompilationPriority p, TR_OptimizationPlan *optimizationPlan)
   {
   _methodDetails = TR::IlGeneratorMethodDetails::clone(_methodDetailsStorage, details);
   _optimizationPlan = optimizationPlan;
   _next = NULL;
   _oldStartPC = oldStartPC;
   _newStartPC = NULL;
   _priority = p;
   _numThreadsWaiting = 0;
   _compErrCode = compilationOK;
   _compilationAttemptsLeft = (TR::Options::canJITCompile()) ? MAX_COMPILE_ATTEMPTS : 1;
   _unloadedMethod = false;
   _doAotLoad = false;
   _useAotCompilation = false;
   _doNotUseAotCodeFromSharedCache = false;
   _tryCompilingAgain = false;
   _compInfoPT = NULL;
   _aotCodeToBeRelocated = NULL;
   if (_optimizationPlan)
      _optimizationPlan->setIsAotLoad(false);
   _async = false;
   _reqFromSecondaryQueue = TR_MethodToBeCompiled::REASON_NONE;
   _reqFromJProfilingQueue = false;
   _changedFromAsyncToSync = false;
   _entryShouldBeDeallocated = false;
   _hasIncrementedNumCompThreadsCompilingHotterMethods = false;
   _weight = 0;
   _jitStateWhenQueued = UNDEFINED_STATE;
   _entryIsCountedAsInvRequest = false;
   _GCRrequest = false;

   _methodIsInSharedCache = TR_maybe;
#if defined(J9VM_OPT_JITSERVER)
   _remoteCompReq = false;
   _stream = NULL;
   _clientOptions = NULL;
   _clientOptionsSize = 0;
   _origOptLevel = unknownHotness;
#endif /* defined(J9VM_OPT_JITSERVER) */

   TR_ASSERT_FATAL(_freeTag & ENTRY_IN_POOL_FREE, "initializing an entry which is not free");

   _freeTag = ENTRY_INITIALIZED;
   }

void
TR_MethodToBeCompiled::shutdown()
   {
#if defined(J9VM_OPT_JITSERVER)
   freeJITServerAllocations();
#endif /* defined(J9VM_OPT_JITSERVER) */
   TR::MonitorTable *table = TR::MonitorTable::get();
   if (!table) return;
   table->removeAndDestroy(_monitor);
   _monitor = NULL;
   _freeTag |= ENTRY_DEALLOCATED;
   }

void TR_MethodToBeCompiled::acquireSlotMonitor(J9VMThread *vmThread)
   {
   getMonitor()->enter();
   // Must have the compilationMonitor in hand to be able to call this method
   //addCompilationTraceEntry(vmThread, OP_HasAcquiredCompilationMonitor);
   //fprintf(stderr, "Thread %p has acquired slot monitor\n", vmThread);
   }

void TR_MethodToBeCompiled::releaseSlotMonitor(J9VMThread *vmThread)
   {
   // Must have the compilationMonitor in hand to be able to call this method
    //addCompilationTraceEntry(vmThread, OP_HasAcquiredCompilationMonitor);
    //fprintf(stderr, "Thread %p will release slot monitor\n", vmThread);
   getMonitor()->exit();
   }

bool
TR_MethodToBeCompiled::isCompiled() const
   {
   return TR::CompilationInfo::isCompiled(getMethodDetails().getMethod());
   }

bool
TR_MethodToBeCompiled::isJNINative() const
   {
   return TR::CompilationInfo::isJNINative(getMethodDetails().getMethod());
   }

void
TR_MethodToBeCompiled::setAotCodeToBeRelocated(const void *m)
   {
   _aotCodeToBeRelocated = m;
   _optimizationPlan->setIsAotLoad(m!=0);
   }

#if defined(J9VM_OPT_JITSERVER)
uint64_t 
TR_MethodToBeCompiled::getClientUID() const
   {
   return _stream->getClientId();
   }

void
TR_MethodToBeCompiled::freeJITServerAllocations()
   {
   if (_clientOptions)
      {
      _compInfoPT->getCompilationInfo()->persistentMemory()->freePersistentMemory((void *)_clientOptions);
      _clientOptions = NULL;
      }
   }
#endif /* defined(J9VM_OPT_JITSERVER) */
