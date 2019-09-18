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
 * @brief Method executed by JITServer to store bytecode iprofiler info to heap memory (instead of to persistent memory)
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
 * @brief Method executed by JITServer to retrieve bytecode iprofiler info from the heap memory
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

/**
 * @brief Method executed by JITServer to cache a resolved method to the resolved method cache
 *
 * @param key Identifier used to identify a resolved method in resolved methods cache
 * @param method The resolved method of interest
 * @param vTableSlot The vTableSlot for the resolved method of interest
 * @param methodInfo Additional method info about the resolved method of interest
 * @return returns void
 */
void
TR::CompilationInfoPerThreadRemote::cacheResolvedMethod(TR_ResolvedMethodKey key, TR_OpaqueMethodBlock *method, uint32_t vTableSlot, TR_ResolvedJ9JITServerMethodInfo &methodInfo)
   {
   static bool useCaching = !feGetEnv("TR_DisableResolvedMethodsCaching");
   if (!useCaching)
      return;

   cacheToPerCompilationMap(_resolvedMethodInfoMap, key, {method, vTableSlot, methodInfo});
   }

/**
 * @brief Method executed by JITServer to retrieve a resolved method from the resolved method cache
 *
 * @param key Identifier used to identify a resolved method in resolved methods cache
 * @param owningMethod Owning method of the resolved method of interest
 * @param resolvedMethod The resolved method of interest, set by this API
 * @param unresolvedInCP The unresolvedInCP boolean value of interest, set by this API
 * @return returns true if method is cached, sets resolvedMethod and unresolvedInCP to cached values, false otherwise.
 */
bool
TR::CompilationInfoPerThreadRemote::getCachedResolvedMethod(TR_ResolvedMethodKey key, TR_ResolvedJ9JITServerMethod *owningMethod, TR_ResolvedMethod **resolvedMethod, bool *unresolvedInCP)
   {
   TR_ResolvedMethodCacheEntry methodCacheEntry;
   if (getCachedValueFromPerCompilationMap(_resolvedMethodInfoMap, key, methodCacheEntry))
      {
      auto comp = getCompilation();
      TR_OpaqueMethodBlock *method = methodCacheEntry.method;
      if (!method)
         {
         *resolvedMethod = NULL;
         return true;
         }
      auto methodInfo = methodCacheEntry.methodInfo;
      uint32_t vTableSlot = methodCacheEntry.vTableSlot;


      // Re-add validation record
      if (comp->compileRelocatableCode() && comp->getOption(TR_UseSymbolValidationManager) && !comp->getSymbolValidationManager()->inHeuristicRegion())
         {
         if(!owningMethod->addValidationRecordForCachedResolvedMethod(key, method))
            {
            // Could not add a validation record
            *resolvedMethod = NULL;
            if (unresolvedInCP)
               *unresolvedInCP = true;
            return true;
            }
         }
      // Create resolved method from cached method info
      if (key.type != TR_ResolvedMethodType::VirtualFromOffset)
         {
         *resolvedMethod = owningMethod->createResolvedMethodFromJ9Method(
                              comp,
                              key.cpIndex,
                              vTableSlot,
                              (J9Method *) method,
                              unresolvedInCP,
                              NULL,
                              methodInfo);
         }
      else
         {
         if (_vm->isAOT_DEPRECATED_DO_NOT_USE())
            *resolvedMethod = method ? new (comp->trHeapMemory()) TR_ResolvedRelocatableJ9JITServerMethod(method, _vm, comp->trMemory(), methodInfo, owningMethod) : 0;
         else
            *resolvedMethod = method ? new (comp->trHeapMemory()) TR_ResolvedJ9JITServerMethod(method, _vm, comp->trMemory(), methodInfo, owningMethod) : 0;
         }

      if (*resolvedMethod)
         {
         if (unresolvedInCP)
            *unresolvedInCP = false;
         return true;
         }
      else
         {
         TR_ASSERT(false, "Should not have cached unresolved method globally");
         }
      }
   return false;
   }

/**
 * @brief Method executed by JITServer to compose a TR_ResolvedMethodKey used for the resolved method cache
 *
 * @param type Resolved method type: VirtualFromCP, VirtualFromOffset, Interface, Static, Special, ImproperInterface, NoType
 * @param ramClass ramClass of resolved method of interest 
 * @param cpIndex constant pool index 
 * @param classObject default to NULL, only set for resolved interface method 
 * @return key used to identify a resolved method in the resolved method cache
 */
TR_ResolvedMethodKey
TR::CompilationInfoPerThreadRemote::getResolvedMethodKey(TR_ResolvedMethodType type, TR_OpaqueClassBlock *ramClass, int32_t cpIndex, TR_OpaqueClassBlock *classObject)
   {
   TR_ResolvedMethodKey key = {type, ramClass, cpIndex, classObject};
   return key;
   }

/**
 * @brief Method executed by JITServer to save the mirrors of resolved method of interest to a list
 *
 * @param resolvedMethod The mirror of the resolved method of interest (existing at JITClient)
 * @return void
 */
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

/**
 * @brief Method executed by JITServer to remember NULL answers for classOfStatic() queries
 *
 * @param ramClass The static class of interest as part of the key
 * @param cpIndex The constant pool index of interest as part of the key
 * @return void
 */
void
TR::CompilationInfoPerThreadRemote::cacheNullClassOfStatic(TR_OpaqueClassBlock *ramClass, int32_t cpIndex)
   {
   TR_OpaqueClassBlock *nullClazz = NULL;
   cacheToPerCompilationMap(_classOfStaticMap, std::make_pair(ramClass, cpIndex), nullClazz);
   }

/**
 * @brief Method executed by JITServer to determine if a previous classOfStatic() query returned NULL
 *
 * @param ramClass The static class of interest 
 * @param cpIndex The constant pool index of interest
 * @return returns true if the previous classOfStatic() query returned NULL and false otherwise
 */
bool
TR::CompilationInfoPerThreadRemote::getCachedNullClassOfStatic(TR_OpaqueClassBlock *ramClass, int32_t cpIndex)
   {
   TR_OpaqueClassBlock *nullClazz;
   return getCachedValueFromPerCompilationMap(_classOfStaticMap, std::make_pair(ramClass, cpIndex), nullClazz);
   }

/**
 * @brief Method executed by JITServer to cache field or static attributes
 *
 * @param ramClass The ramClass of interest as part of the key
 * @param cpIndex The cpIndex of interest as part of the key
 * @param attrs The value we are going to cache
 * @param isStatic Whether the field is static
 * @return void
 */
void
TR::CompilationInfoPerThreadRemote::cacheFieldOrStaticAttributes(TR_OpaqueClassBlock *ramClass, int32_t cpIndex, const TR_J9MethodFieldAttributes &attrs, bool isStatic)
   {
   if (isStatic)
      cacheToPerCompilationMap(_staticAttributesCache, std::make_pair(ramClass, cpIndex), attrs);
   else
      cacheToPerCompilationMap(_fieldAttributesCache, std::make_pair(ramClass, cpIndex), attrs);
   }

/**
 * @brief Method executed by JITServer to retrieve field or static attributes from the cache
 *
 * @param ramClass The ramClass of interest as part of the key  
 * @param cpIndex The cpIndex of interest as part of the value
 * @param attrs The value to be set by the API 
 * @param isStatic Whether the field is static 
 * @return returns true if found in cache else false
 */
bool
TR::CompilationInfoPerThreadRemote::getCachedFieldOrStaticAttributes(TR_OpaqueClassBlock *ramClass, int32_t cpIndex, TR_J9MethodFieldAttributes &attrs, bool isStatic)
   {
   if (isStatic)
      return getCachedValueFromPerCompilationMap(_staticAttributesCache, std::make_pair(ramClass, cpIndex), attrs);
   else
      return getCachedValueFromPerCompilationMap(_fieldAttributesCache, std::make_pair(ramClass, cpIndex), attrs);
   }

/**
 * @brief Method executed by JITServer to cache unresolved string
 *
 * @param ramClass The ramClass of interest as part of the key
 * @param cpIndex The cpIndex of interest as part of the key
 * @param stringAttrs The value we are going to cache
 * @return void
 */
void
TR::CompilationInfoPerThreadRemote::cacheIsUnresolvedStr(TR_OpaqueClassBlock *ramClass, int32_t cpIndex, const TR_IsUnresolvedString &stringAttrs)
   {
   cacheToPerCompilationMap(_isUnresolvedStrCache, std::make_pair(ramClass, cpIndex), stringAttrs);
   }

/**
 * @brief Method executed by JITServer to retrieve unresolved string
 *
 * @param ramClass The ramClass of interest as part of the key
 * @param cpIndex The cpIndex of interest as part of the key
 * @param attrs The value to be set by the API
 * @return returns true if found in cache else false
 */
bool
TR::CompilationInfoPerThreadRemote::getCachedIsUnresolvedStr(TR_OpaqueClassBlock *ramClass, int32_t cpIndex, TR_IsUnresolvedString &stringAttrs)
   {
   return getCachedValueFromPerCompilationMap(_isUnresolvedStrCache, std::make_pair(ramClass, cpIndex), stringAttrs);
   }
