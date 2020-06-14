/*******************************************************************************
 * Copyright (c) 2018, 2020 IBM Corp. and others
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

#ifndef JITSERVER_COMPILATION_THREAD_H
#define JITSERVER_COMPILATION_THREAD_H

#include "control/CompilationThread.hpp"
#include "env/j9methodServer.hpp"
#include "runtime/JITClientSession.hpp"

class TR_IPBytecodeHashTableEntry;

using IPTableHeapEntry = UnorderedMap<uint32_t, TR_IPBytecodeHashTableEntry*>;
using IPTableHeap_t = UnorderedMap<J9Method *, IPTableHeapEntry *>;
using ResolvedMirrorMethodsPersistIP_t = Vector<TR_ResolvedJ9Method *>;
using ClassOfStatic_t = UnorderedMap<std::pair<TR_OpaqueClassBlock *, int32_t>, TR_OpaqueClassBlock *>;
using FieldOrStaticAttrTable_t = UnorderedMap<std::pair<TR_OpaqueClassBlock *, int32_t>, TR_J9MethodFieldAttributes>;

void outOfProcessCompilationEnd(TR_MethodToBeCompiled *entry, TR::Compilation *comp);

namespace TR
{
// Objects of this type are instantiated at JITServer
class CompilationInfoPerThreadRemote : public TR::CompilationInfoPerThread
   {
   public:
   friend class TR::CompilationInfo;
   CompilationInfoPerThreadRemote(TR::CompilationInfo &compInfo, J9JITConfig *jitConfig, int32_t id, bool isDiagnosticThread);

   virtual void processEntry(TR_MethodToBeCompiled &entry, J9::J9SegmentProvider &scratchSegmentProvider) override;
   TR_PersistentMethodInfo *getRecompilationMethodInfo() const { return _recompilationMethodInfo; }

   uint32_t getSeqNo() const { return _seqNo; }; // For ordering requests at the server
   void setSeqNo(uint32_t seqNo) { _seqNo = seqNo; }
   void updateSeqNo(ClientSessionData *clientSession);

   void waitForMyTurn(ClientSessionData *clientSession, TR_MethodToBeCompiled &entry); // Return false if timeout
   bool getWaitToBeNotified() const { return _waitToBeNotified; }
   void setWaitToBeNotified(bool b) { _waitToBeNotified = b; }

   bool cacheIProfilerInfo(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR_IPBytecodeHashTableEntry *entry);
   TR_IPBytecodeHashTableEntry *getCachedIProfilerInfo(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, bool *methodInfoPresent);

   void cacheResolvedMethod(TR_ResolvedMethodKey key, TR_OpaqueMethodBlock *method, uint32_t vTableSlot, const TR_ResolvedJ9JITServerMethodInfo &methodInfo, int32_t ttlForUnresolved = 2);
   bool getCachedResolvedMethod(TR_ResolvedMethodKey key, TR_ResolvedJ9JITServerMethod *owningMethod, TR_ResolvedMethod **resolvedMethod, bool *unresolvedInCP = NULL);
   TR_ResolvedMethodKey getResolvedMethodKey(TR_ResolvedMethodType type, TR_OpaqueClassBlock *ramClass, int32_t cpIndex, TR_OpaqueClassBlock *classObject = NULL);

   void cacheResolvedMirrorMethodsPersistIPInfo(TR_ResolvedJ9Method *resolvedMethod);
   ResolvedMirrorMethodsPersistIP_t *getCachedResolvedMirrorMethodsPersistIPInfo() const { return _resolvedMirrorMethodsPersistIPInfo; }

   void cacheNullClassOfStatic(TR_OpaqueClassBlock *ramClass, int32_t cpIndex);
   bool getCachedNullClassOfStatic(TR_OpaqueClassBlock *ramClass, int32_t cpIndex);

   void cacheFieldOrStaticAttributes(TR_OpaqueClassBlock *ramClass, int32_t cpIndex, const TR_J9MethodFieldAttributes &attrs, bool isStatic);
   bool getCachedFieldOrStaticAttributes(TR_OpaqueClassBlock *ramClass, int32_t cpIndex, TR_J9MethodFieldAttributes &attrs, bool isStatic);

   void cacheIsUnresolvedStr(TR_OpaqueClassBlock *ramClass, int32_t cpIndex, const TR_IsUnresolvedString &stringAttrs);
   bool getCachedIsUnresolvedStr(TR_OpaqueClassBlock *ramClass, int32_t cpIndex, TR_IsUnresolvedString &stringAttrs);

   void clearPerCompilationCaches();
   void deleteClientSessionData(uint64_t clientId, TR::CompilationInfo* compInfo, J9VMThread* compThread);
   virtual void freeAllResources() override;

   private:
   /* Template method for allocating a cache of type T on the heap.
    * Cache pointer must be NULL.
    */
   template <typename T>
   bool initializePerCompilationCache(T* &cache)
      {
      // Initialize map
      TR_ASSERT(!cache, "Cache already initialized");
      TR_Memory *trMemory = getCompilation()->trMemory();
      cache = new (trMemory->trHeapMemory()) T(typename T::allocator_type(trMemory->heapMemoryRegion()));
      return cache != NULL;
      }

   /* Template method for storing key-value pairs (of types K and V respectively)
    * to a heap-allocated unordered map.
    * If a map is NULL, will allocate it.
    */
   template <typename K, typename V>
   void cacheToPerCompilationMap(UnorderedMap<K, V>* &map, const K &key, const V &value)
      {
      if (!map)
         initializePerCompilationCache(map);
      map->insert({ key, value });
      }

   /* Template method for retrieving values from heap-allocated unordered map.
    * If the map is NULL or value is not found, returns false.
    * Otherwise, sets value to retrieved value and returns true
    */
   template <typename K, typename V>
   bool getCachedValueFromPerCompilationMap(UnorderedMap<K, V>* &map, const K &key, V &value)
      {
      if (!map)
         return false;
      auto it = map->find(key);
      if (it != map->end())
         {
         // Found entry at a given key
         value = it->second;
         return true;
         }
      return false;
      }

   /* Template method for clearing a heap-allocated cache.
    * Simply sets pointer to cache to NULL.
    */
   template <typename T>
   void clearPerCompilationCache(T* &cache)
      {
      // Since cache was heap-allocated,
      // memory will be released automatically at the end of the compilation
      cache = NULL;
      }

   TR_PersistentMethodInfo *_recompilationMethodInfo;
   uint32_t _seqNo;
   bool _waitToBeNotified; // accessed with clientSession->_sequencingMonitor in hand
   IPTableHeap_t *_methodIPDataPerComp;
   TR_ResolvedMethodInfoCache *_resolvedMethodInfoMap;
   ResolvedMirrorMethodsPersistIP_t *_resolvedMirrorMethodsPersistIPInfo; //list of mirrors of resolved methods for persisting IProfiler info
   ClassOfStatic_t *_classOfStaticMap;
   FieldOrStaticAttrTable_t *_fieldAttributesCache;
   FieldOrStaticAttrTable_t *_staticAttributesCache;
   UnorderedMap<std::pair<TR_OpaqueClassBlock *, int32_t>, TR_IsUnresolvedString> *_isUnresolvedStrCache;
   }; // class CompilationInfoPerThreadRemote
} // namespace TR

#endif // defined(JITSERVER_COMPILATION_THREAD_H)
