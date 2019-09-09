/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

#include "control/JITServerCompilationThread.hpp"

#include "codegen/CodeGenerator.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/MethodToBeCompiled.hpp"
#include "env/ClassTableCriticalSection.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheExceptions.hpp"
#include "runtime/J9VMAccess.hpp"
#include "runtime/RelocationTarget.hpp"
#include "jitprotos.h"
#include "vmaccess.h"

TR::CompilationInfoPerThreadRemote::CompilationInfoPerThreadRemote(TR::CompilationInfo &compInfo, J9JITConfig *jitConfig, int32_t id, bool isDiagnosticThread)
   : CompilationInfoPerThread(compInfo, jitConfig, id, isDiagnosticThread),
   _recompilationMethodInfo(NULL),
   _seqNo(0),
   _waitToBeNotified(false),
   _methodIPDataPerComp(NULL),
   _resolvedMethodInfoMap(NULL),
   _resolvedMirrorMethodsPersistIPInfo(NULL),
   _classOfStaticMap(NULL),
   _fieldAttributesCache(NULL),
   _staticAttributesCache(NULL),
   _isUnresolvedStrCache(NULL)
   {}

/**
 * Code to be executed on the JITServer to store bytecode iprofiler info to heap memory (instead of to persistent memory)
 *
 * @param method J9Method in question
 * @param byteCodeIndex bytecode in question
 * @param entry iprofile data to be stored
 * @return always return true at the moment
 */
bool
TR::CompilationInfoPerThreadRemote::cacheIProfilerInfo(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR_IPBytecodeHashTableEntry *entry)
   {
   IPTableHeapEntry *entryMap = NULL;
   if(!getCachedValueFromPerCompilationMap(_methodIPDataPerComp, (J9Method *) method, entryMap))
      {
      // Either first time cacheIProfilerInfo called during current compilation,
      // so _methodIPDataPerComp is NULL, or caching a new method, entryMap not found
      if (entry)
         {
         cacheToPerCompilationMap(entryMap, byteCodeIndex, entry);
         }
      else
         {
         // If entry is NULL, create entryMap, but do not cache anything
         initializePerCompilationCache(entryMap);
         }
      cacheToPerCompilationMap(_methodIPDataPerComp, (J9Method *) method, entryMap);
      }
   else if (entry)
      {
      // Adding an entry for already seen method (i.e. entryMap exists)
      cacheToPerCompilationMap(entryMap, byteCodeIndex, entry);
      }
   return true;
   }

/**
 * Code to be executed on the JITServer to retrieve bytecode iprofiler info from the heap memory
 *
 * @param method J9Method in question
 * @param byteCodeIndex bytecode in question
 * @param methodInfoPresent indicates to the caller whether the data is present
 * @return IPTableHeapEntry bytecode iprofile data
 */
TR_IPBytecodeHashTableEntry*
TR::CompilationInfoPerThreadRemote::getCachedIProfilerInfo(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, bool *methodInfoPresent)
   {
   *methodInfoPresent = false;

   IPTableHeapEntry *entryMap = NULL;
   TR_IPBytecodeHashTableEntry *ipEntry = NULL;

   getCachedValueFromPerCompilationMap(_methodIPDataPerComp, (J9Method *) method, entryMap);
   // methodInfoPresent=true means that we cached info for this method (i.e. entryMap is not NULL),
   // but entry for this bytecode might not exist, which means it's NULL
   if (entryMap)
      {
      *methodInfoPresent = true;
      getCachedValueFromPerCompilationMap(entryMap, byteCodeIndex, ipEntry);
      }
   return ipEntry;
   }

void
TR::CompilationInfoPerThreadRemote::cacheResolvedMirrorMethodsPersistIPInfo(TR_ResolvedJ9Method *resolvedMethod)
   {
   if (!_resolvedMirrorMethodsPersistIPInfo)
      {
      initializePerCompilationCache(_resolvedMirrorMethodsPersistIPInfo);
      if (!_resolvedMirrorMethodsPersistIPInfo)
         return;
      }

   _resolvedMirrorMethodsPersistIPInfo->push_back(resolvedMethod);
   }
