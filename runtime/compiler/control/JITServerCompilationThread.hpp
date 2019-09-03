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

#ifndef JITaaS_COMPILATION_THREAD_H
#define JITaaS_COMPILATION_THREAD_H

#include <unordered_map>
#include "control/CompilationThread.hpp"
#include "net/ClientStream.hpp"
#include "env/PersistentCollections.hpp"
#include "env/j9methodServer.hpp"
#include "env/J9CPU.hpp"
#include "runtime/JITClientSession.hpp"

class TR_PersistentClassInfo;
class TR_IPBytecodeHashTableEntry;
struct TR_RemoteROMStringKey;


using IPTableHeapEntry = UnorderedMap<uint32_t, TR_IPBytecodeHashTableEntry*>;
using IPTableHeap_t = UnorderedMap<J9Method *, IPTableHeapEntry *>;
using ResolvedMirrorMethodsPersistIP_t = Vector<TR_ResolvedJ9Method *>;
using ClassOfStatic_t = UnorderedMap<std::pair<TR_OpaqueClassBlock *, int32_t>, TR_OpaqueClassBlock *>;
using FieldOrStaticAttrTable_t = UnorderedMap<std::pair<TR_OpaqueClassBlock *, int32_t>, TR_J9MethodFieldAttributes>;


size_t methodStringLength(J9ROMMethod *);
std::string packROMClass(J9ROMClass *, TR_Memory *);
TR_MethodMetaData *remoteCompile(J9VMThread *, TR::Compilation *, TR_ResolvedMethod *,
   J9Method *, TR::IlGeneratorMethodDetails &, TR::CompilationInfoPerThreadBase *);
TR_MethodMetaData *remoteCompilationEnd(J9VMThread * vmThread, TR::Compilation *comp, TR_ResolvedMethod * compilee, J9Method * method,
   TR::CompilationInfoPerThreadBase *compInfoPT, const std::string& codeCacheStr, const std::string& dataCacheStr);
void outOfProcessCompilationEnd(TR_MethodToBeCompiled *entry, TR::Compilation *comp);
void printJITaaSMsgStats(J9JITConfig *);
void printJITServerCHTableStats(J9JITConfig *, TR::CompilationInfo *);
void printJITaaSCacheStats(J9JITConfig *, TR::CompilationInfo *);

namespace TR
{
// Objects of this type are instantiated at JITaaS server
class CompilationInfoPerThreadRemote : public TR::CompilationInfoPerThread
   {
   public:
   friend class TR::CompilationInfo;
   CompilationInfoPerThreadRemote(TR::CompilationInfo &compInfo, J9JITConfig *jitConfig, int32_t id, bool isDiagnosticThread);
   virtual void processEntry(TR_MethodToBeCompiled &entry, J9::J9SegmentProvider &scratchSegmentProvider) override;
   TR_PersistentMethodInfo *getRecompilationMethodInfo() { return _recompilationMethodInfo; }
   uint32_t getSeqNo() const { return _seqNo; }; // for ordering requests at the server
   void setSeqNo(uint32_t seqNo) { _seqNo = seqNo; }
   void waitForMyTurn(ClientSessionData *clientSession, TR_MethodToBeCompiled &entry); // return false if timeout
   void updateSeqNo(ClientSessionData *clientSession);
   bool getWaitToBeNotified() const { return _waitToBeNotified; }
   void setWaitToBeNotified(bool b) { _waitToBeNotified = b; }

   bool cacheIProfilerInfo(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR_IPBytecodeHashTableEntry *entry);
   TR_IPBytecodeHashTableEntry *getCachedIProfilerInfo(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, bool *methodInfoPresent);
   void cacheResolvedMethod(TR_ResolvedMethodKey key, TR_OpaqueMethodBlock *method, uint32_t vTableSlot, TR_ResolvedJ9JITaaSServerMethodInfo &methodInfo);
   bool getCachedResolvedMethod(TR_ResolvedMethodKey key, TR_ResolvedJ9JITaaSServerMethod *owningMethod, TR_ResolvedMethod **resolvedMethod, bool *unresolvedInCP = NULL);
   TR_ResolvedMethodKey getResolvedMethodKey(TR_ResolvedMethodType type, TR_OpaqueClassBlock *ramClass, int32_t cpIndex, TR_OpaqueClassBlock *classObject = NULL);

   void cacheResolvedMirrorMethodsPersistIPInfo(TR_ResolvedJ9Method *resolvedMethod);
   ResolvedMirrorMethodsPersistIP_t *getCachedResolvedMirrorMethodsPersistIPInfo() { return _resolvedMirrorMethodsPersistIPInfo; }

   void cacheNullClassOfStatic(TR_OpaqueClassBlock *ramClass, int32_t cpIndex);
   bool getCachedNullClassOfStatic(TR_OpaqueClassBlock *ramClass, int32_t cpIndex);

   void cacheFieldOrStaticAttributes(TR_OpaqueClassBlock *ramClass, int32_t cpIndex, const TR_J9MethodFieldAttributes &attrs, bool isStatic);
   bool getCachedFieldOrStaticAttributes(TR_OpaqueClassBlock *ramClass, int32_t cpIndex, TR_J9MethodFieldAttributes &attrs, bool isStatic);

   void cacheIsUnresolvedStr(TR_OpaqueClassBlock *ramClass, int32_t cpIndex, const TR_IsUnresolvedString &stringAttrs);
   bool getCachedIsUnresolvedStr(TR_OpaqueClassBlock *ramClass, int32_t cpIndex, TR_IsUnresolvedString &stringAttrs);

   void clearPerCompilationCaches();
   void deleteClientSessionData(uint64_t clientId, TR::CompilationInfo* compInfo, J9VMThread* compThread);

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

class JITServerHelpers
   {
   public:
   enum ClassInfoDataType
      {
      CLASSINFO_ROMCLASS_MODIFIERS,
      CLASSINFO_ROMCLASS_EXTRAMODIFIERS,
      CLASSINFO_BASE_COMPONENT_CLASS,
      CLASSINFO_NUMBER_DIMENSIONS,
      CLASSINFO_PARENT_CLASS,
      CLASSINFO_INTERFACE_CLASS,
      CLASSINFO_CLASS_HAS_FINAL_FIELDS,
      CLASSINFO_CLASS_DEPTH_AND_FLAGS,
      CLASSINFO_CLASS_INITIALIZED,
      CLASSINFO_BYTE_OFFSET_TO_LOCKWORD,
      CLASSINFO_LEAF_COMPONENT_CLASS,
      CLASSINFO_CLASS_LOADER,
      CLASSINFO_HOST_CLASS,
      CLASSINFO_COMPONENT_CLASS,
      CLASSINFO_ARRAY_CLASS,
      CLASSINFO_TOTAL_INSTANCE_SIZE,
      CLASSINFO_CLASS_OF_STATIC_CACHE,
      CLASSINFO_REMOTE_ROM_CLASS,
      CLASSINFO_CLASS_FLAGS,
      CLASSINFO_METHODS_OF_CLASS,
      };
   // NOTE: when adding new elements to this tuple, add them to the end,
   // to not mess with the established order.
   using ClassInfoTuple = std::tuple
      <
      std::string, J9Method *,                                       // 0: string                   1: _methodsOfClass
      TR_OpaqueClassBlock *, int32_t,                                // 2: _baseComponentClass      3: _numDimensions
      TR_OpaqueClassBlock *, std::vector<TR_OpaqueClassBlock *>,     // 4: _parentClass             5: _tmpInterfaces
      std::vector<uint8_t>, bool,                                    // 6: _methodTracingInfo       7: _classHasFinalFields
      uintptrj_t, bool,                                              // 8: _classDepthAndFlags      9: _classInitialized
      uint32_t, TR_OpaqueClassBlock *,                               // 10: _byteOffsetToLockword   11: _leafComponentClass
      void *, TR_OpaqueClassBlock *,                                 // 12: _classLoader            13: _hostClass
      TR_OpaqueClassBlock *, TR_OpaqueClassBlock *,                  // 14: _componentClass         15: _arrayClass
      uintptrj_t, J9ROMClass *,                                      // 16: _totalInstanceSize      17: _remoteRomClass
      uintptrj_t, uintptrj_t                                         // 18: _constantPool           19: _classFlags
      >;
   static ClassInfoTuple packRemoteROMClassInfo(J9Class *clazz, J9VMThread *vmThread, TR_Memory *trMemory);
   static void cacheRemoteROMClass(ClientSessionData *clientSessionData, J9Class *clazz, J9ROMClass *romClass, ClassInfoTuple *classInfoTuple);
   static void cacheRemoteROMClass(ClientSessionData *clientSessionData, J9Class *clazz, J9ROMClass *romClass, ClassInfoTuple *classInfoTuple, ClientSessionData::ClassInfo &classInfo);
   static J9ROMClass *getRemoteROMClassIfCached(ClientSessionData *clientSessionData, J9Class *clazz);
   static J9ROMClass *getRemoteROMClass(J9Class *, JITServer::ServerStream *stream, TR_Memory *trMemory, ClassInfoTuple *classInfoTuple);
   static J9ROMClass *romClassFromString(const std::string &romClassStr, TR_PersistentMemory *trMemory);
   static bool getAndCacheRAMClassInfo(J9Class *clazz, ClientSessionData *clientSessionData, JITServer::ServerStream *stream, ClassInfoDataType dataType, void *data);
   static bool getAndCacheRAMClassInfo(J9Class *clazz, ClientSessionData *clientSessionData, JITServer::ServerStream *stream, ClassInfoDataType dataType1, void *data1, ClassInfoDataType dataType2, void *data2);
   static void getROMClassData(const ClientSessionData::ClassInfo &classInfo, ClassInfoDataType dataType, void *data);
   static J9ROMMethod *romMethodOfRamMethod(J9Method* method);

   // functions used for allowing the client to compile locally when server is unavailable
   // should be only used on the client side
   static void postStreamFailure(OMRPortLibrary *portLibrary);
   static bool shouldRetryConnection(OMRPortLibrary *portLibrary);
   static void postStreamConnectionSuccess();
   static bool isServerAvailable() { return _serverAvailable; }
   static TR::Monitor * getClientStreamMonitor()
      {
      if (!_clientStreamMonitor)
         _clientStreamMonitor = TR::Monitor::create("clientStreamMonitor");
      return _clientStreamMonitor;
      }
   static void insertIntoOOSequenceEntryList(ClientSessionData *clientData, TR_MethodToBeCompiled *entry);

   private:
   static uint64_t _waitTime;
   static uint64_t _nextConnectionRetryTime;
   static bool _serverAvailable;
   static TR::Monitor * _clientStreamMonitor;
   }; // class JITServerHelpers

#endif // defined(JITaaS_COMPILATION_THREAD_H)
