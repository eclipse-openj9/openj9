/*******************************************************************************
 * Copyright IBM Corp. and others 2019
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

#include "runtime/JITClientSession.hpp"

#include "control/CompilationRuntime.hpp" // for CompilationInfo
#include "control/MethodToBeCompiled.hpp" // for TR_MethodToBeCompiled
#include "control/JITServerHelpers.hpp"
#include "control/JITServerCompilationThread.hpp"
#include "env/JITServerPersistentCHTable.hpp"
#include "env/ut_j9jit.h"
#include "env/VerboseLog.hpp"
#include "net/ServerStream.hpp" // for JITServer::ServerStream
#include "runtime/JITServerSharedROMClassCache.hpp"
#include "runtime/RuntimeAssumptions.hpp" // for TR_AddressSet
#include "control/CompilationController.hpp"

TR_OpaqueClassBlock * const ClientSessionData::mustClearCachesFlag = reinterpret_cast<TR_OpaqueClassBlock *>(~0);

ClientSessionData::ClientSessionData(uint64_t clientUID, uint32_t seqNo, TR_PersistentMemory *persistentMemory, bool usesPerClientMemory) :
   _clientUID(clientUID), _maxReceivedSeqNo(seqNo), _lastProcessedCriticalSeqNo(seqNo),
   _persistentMemory(persistentMemory),
   _usesPerClientMemory(usesPerClientMemory),
   _OOSequenceEntryList(NULL), _chTable(NULL),
   _romClassMap(decltype(_romClassMap)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _J9MethodMap(decltype(_J9MethodMap)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _classBySignatureMap(decltype(_classBySignatureMap)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _classChainDataMap(decltype(_classChainDataMap)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _constantPoolToClassMap(decltype(_constantPoolToClassMap)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _unloadedClassAddresses(NULL),
   _requestUnloadedClasses(true),
   _staticFinalDataMap(decltype(_staticFinalDataMap)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _rtResolve(false),
   _registeredJ2IThunksMap(decltype(_registeredJ2IThunksMap)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _registeredInvokeExactJ2IThunksSet(decltype(_registeredInvokeExactJ2IThunksSet)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _wellKnownClasses(),
   _isInStartupPhase(false),
   _aotCacheName(), _aotCache(NULL), _aotHeaderRecord(NULL),
   _aotCacheKnownIds(decltype(_aotCacheKnownIds)::allocator_type(persistentMemory->_persistentAllocator.get()))
   {
   updateTimeOfLastAccess();
   _javaLangClassPtr = NULL;
   _inUse = 1;
   _numActiveThreads = 0;
   _romMapMonitor = TR::Monitor::create("JIT-JITServerROMMapMonitor");
   _classMapMonitor = TR::Monitor::create("JIT-JITServerClassMapMonitor");
   _classChainDataMapMonitor = TR::Monitor::create("JIT-JITServerClassChainDataMapMonitor");
   _sequencingMonitor = TR::Monitor::create("JIT-JITServerSequencingMonitor");
   _cacheInitMonitor = TR::Monitor::create("JIT-JITServerCacheInitMonitor");
   _constantPoolMapMonitor = TR::Monitor::create("JIT-JITServerConstantPoolMonitor");
   _vmInfo = NULL;
   _staticMapMonitor = TR::Monitor::create("JIT-JITServerStaticMapMonitor");
   _markedForDeletion = false;
   _thunkSetMonitor = TR::Monitor::create("JIT-JITServerThunkSetMonitor");

   _bClassUnloadingAttempt = false;
   _classUnloadRWMutex = NULL;
   if (omrthread_rwmutex_init(&_classUnloadRWMutex, 0, "JITServer class unload RWMutex"))
      {
      TR_ASSERT_FATAL(false, "Failed to initialize JITServer class unload RWMutex");
      }

   TR::SymbolValidationManager::populateSystemClassesNotWorthRemembering(this);

   _wellKnownClassesMonitor = TR::Monitor::create("JIT-JITServerWellKnownClassesMonitor");
   _aotCacheKnownIdsMonitor = TR::Monitor::create("JIT-JITServerAOTCacheKnownIdsMonitor");
   }

ClientSessionData::~ClientSessionData()
   {
   //NOTE:
   //
   // Persistent members of the client session need to be deallocated explicitly
   // with the per-client allocator. This is because in some places from where the
   // session is destroyed, the per-client allocation region cannot be entered.
   //
   // Any objects whose lifetime is bound to the client session that are NOT allocated with
   // the per-client persistent allocator need to be explicitly deallocated or released in
   // ClientSessionData::destroy() in the 'if (usesPerClientMemory && useAOTCache)' branch.
   // Examples of such objects are monitors (including ones inside client session members)
   // and objects allocated with the global persistent allocator (e.g. shared ROMClasses).

   clearCaches();

   if (_vmInfo)
      {
      destroyJ9SharedClassCacheDescriptorList();
      _persistentMemory->freePersistentMemory(_vmInfo);
      }

   destroyMonitors();
   }

void
ClientSessionData::destroyMonitors()
   {
   TR::Monitor::destroy(_romMapMonitor);
   TR::Monitor::destroy(_classMapMonitor);
   TR::Monitor::destroy(_classChainDataMapMonitor);
   TR::Monitor::destroy(_sequencingMonitor);
   TR::Monitor::destroy(_cacheInitMonitor);
   TR::Monitor::destroy(_constantPoolMapMonitor);
   TR::Monitor::destroy(_staticMapMonitor);
   TR::Monitor::destroy(_thunkSetMonitor);
   omrthread_rwmutex_destroy(_classUnloadRWMutex);
   _classUnloadRWMutex = NULL;
   TR::Monitor::destroy(_wellKnownClassesMonitor);
   TR::Monitor::destroy(_aotCacheKnownIdsMonitor);
   }

void
ClientSessionData::updateTimeOfLastAccess()
   {
   PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);
   _timeOfLastAccess = j9time_current_time_millis();
   }

// This method is called within a critical section such that no two threads can enter it concurrently
void
ClientSessionData::initializeUnloadedClassAddrRanges(const std::vector<TR_AddressRange> &unloadedClassRanges, int32_t maxRanges)
   {
   OMR::CriticalSection getUnloadedClasses(getROMMapMonitor());

   if (!_unloadedClassAddresses)
      _unloadedClassAddresses = new (_persistentMemory) TR_AddressSet(_persistentMemory, maxRanges);
   _unloadedClassAddresses->setRanges(unloadedClassRanges);
   }

void
ClientSessionData::processUnloadedClasses(const std::vector<TR_OpaqueClassBlock*> &classes, bool updateUnloadedClasses)
   {
   const size_t numOfUnloadedClasses = classes.size();
   auto compInfoPT = TR::compInfoPT;
   int32_t compThreadID = compInfoPT->getCompThreadId();

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d will process a list of %zu unloaded classes for clientUID %llu",
         compThreadID, numOfUnloadedClasses, (unsigned long long)_clientUID);

   Trc_JITServerUnloadClasses(TR::compInfoPT->getCompilationThread(),
         compThreadID, this, (unsigned long long)_clientUID, (unsigned long long)numOfUnloadedClasses);

   if (numOfUnloadedClasses > 0)
      writeAcquireClassUnloadRWMutex(); //TODO: use RAII style to avoid problems with exceptions

   //Vector to hold the list of the unloaded classes and the corresponding data needed for purging the Caches
   std::vector<ClassUnloadedData> unloadedClasses;
   unloadedClasses.reserve(numOfUnloadedClasses);
      {
      OMR::CriticalSection processUnloadedClasses(getROMMapMonitor());

      for (auto clazz : classes)
         {
         if (updateUnloadedClasses)
            _unloadedClassAddresses->add((uintptr_t)clazz);

         auto it = _romClassMap.find((J9Class*)clazz);
         if (it == _romClassMap.end())
            {
            //Class is not cached so this entry will be used to delete the entry from caches by value.
            ClassLoaderStringPair key;
            unloadedClasses.push_back({ clazz, key, NULL, false });
            continue; // unloaded class was never cached
            }

         J9ConstantPool *cp = it->second._constantPool;

         J9ROMClass *romClass = it->second._romClass;

         J9UTF8 *clazzName = NNSRP_GET(romClass->className, J9UTF8 *);
         char *className = (char *) clazzName->data;
         int32_t sigLen = (int32_t) clazzName->length;
         sigLen = (className[0] == '[' ? sigLen : sigLen + 2);

         // copy of classNameToSignature method which can't be used
         // here because compilation object hasn't been initialized yet

         std::string sigStr(sigLen, 0);
         if (className[0] == '[')
            {
            memcpy(&sigStr[0], className, sigLen);
            }
         else
            {
            if (TR::Compiler->om.areFlattenableValueTypesEnabled() &&
                TR::Compiler->om.isQDescriptorForValueTypesSupported() &&
                TR::Compiler->cls.isPrimitiveValueTypeClass(clazz))
               sigStr[0] = 'Q';
            else
               sigStr[0] = 'L';
            memcpy(&sigStr[1], className, sigLen - 2);
            sigStr[sigLen-1]=';';
            }

         J9ClassLoader * cl = (J9ClassLoader *)(it->second._classLoader);
         ClassLoaderStringPair key = { cl, sigStr };
         //Class is cached, so retain the data to be used for purging the caches.
         unloadedClasses.push_back({ clazz, key, cp, true });

         // For _classBySignatureMap entries that were cached by referencing class loader
         // we need to delete them using the correct class loader
         auto &classLoadersMap = it->second._referencingClassLoaders;
         for (auto it = classLoadersMap.begin(); it != classLoadersMap.end(); ++it)
            {
            ClassLoaderStringPair key = { *it, sigStr };
            unloadedClasses.push_back({ clazz, key, cp, true });
            }

         J9Method *methods = it->second._methodsOfClass;
         // delete all the cached J9Methods belonging to this unloaded class
         for (size_t i = 0; i < romClass->romMethodCount; i++)
            {
            J9Method *j9method = methods + i;
            auto iter = _J9MethodMap.find(j9method);
            if (iter != _J9MethodMap.end())
               {
               IPTable_t *ipDataHT = iter->second._IPData;
               if (ipDataHT)
                  {
                  for (auto& entryIt : *ipDataHT)
                     {
                     auto entryPtr = entryIt.second;
                     if (entryPtr)
                        jitPersistentFree(entryPtr);
                     }
                  ipDataHT->~IPTable_t();
                  _persistentMemory->freePersistentMemory(ipDataHT);
                  iter->second._IPData = NULL;
                  }
               _J9MethodMap.erase(j9method);
               }
            }
         it->second.freeClassInfo(_persistentMemory);
         _romClassMap.erase(it);
         }
      }

   // remove the class chain data from the cache for the unloaded class.
   {
   OMR::CriticalSection processUnloadedClasses(getClassChainDataMapMonitor());

   for (auto clazz : classes)
      _classChainDataMap.erase((J9Class*)clazz);
   }

   // purge Class by name cache
   {
   OMR::CriticalSection classMapCS(getClassMapMonitor());
   purgeCache(&unloadedClasses, getClassBySignatureMap(), &ClassUnloadedData::_pair);
   }

   // purge Constant pool to class cache
   {
   OMR::CriticalSection constantPoolToClassMap(getConstantPoolMonitor());
   purgeCache(&unloadedClasses, getConstantPoolToClassMap(), &ClassUnloadedData::_cp);
   }

   if (numOfUnloadedClasses > 0)
      writeReleaseClassUnloadRWMutex();
   }

void
ClientSessionData::processIllegalFinalFieldModificationList(const std::vector<TR_OpaqueClassBlock*> &classes)
   {
   const size_t numOfClasses = classes.size();
   int32_t compThreadID = TR::compInfoPT->getCompThreadId();

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
         "compThreadID=%d will process a list of %zu classes with illegal final field modification for clientUID %llu",
            compThreadID, numOfClasses, (unsigned long long)_clientUID);
      {
      OMR::CriticalSection processClassesWithIllegalModification(getROMMapMonitor());
      for (auto clazz : classes)
         {
         auto it = _romClassMap.find((J9Class*)clazz);
         if (it != _romClassMap.end())
            {
            it->second._classFlags |= J9ClassHasIllegalFinalFieldModifications;
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                  "compThreadID=%d found clazz %p in the cache and updated bit J9ClassHasIllegalFinalFieldModifications to 1\n", compThreadID, clazz);
            }
         }
      }
   }

TR_IPBytecodeHashTableEntry*
ClientSessionData::getCachedIProfilerInfo(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, bool *methodInfoPresent)
   {
   *methodInfoPresent = false;
   TR_IPBytecodeHashTableEntry *ipEntry = NULL;
   OMR::CriticalSection getRemoteROMClass(getROMMapMonitor());
   // check whether info about j9method is cached
   auto & j9methodMap = getJ9MethodMap();
   auto it = j9methodMap.find((J9Method*)method);
   if (it != j9methodMap.end())
      {
      // check whether j9method data has any IP information
      auto iProfilerMap = it->second._IPData;
      if (iProfilerMap)
         {
         *methodInfoPresent = true;
         // check whether desired bcindex is cached
         auto ipData = iProfilerMap->find(byteCodeIndex);
         if (ipData != iProfilerMap->end())
            {
            ipEntry = ipData->second;
            }
         }
      }
   else
      {
      // Very unlikely scenario because the optimizer will have created  ResolvedJ9Method
      // whose constructor would have fetched and cached the j9method info
      TR_ASSERT(false, "profilingSample: asking about j9method=%p but this is not present in the J9MethodMap", method);
      }
   return ipEntry;
   }

bool
ClientSessionData::cacheIProfilerInfo(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR_IPBytecodeHashTableEntry *entry, bool isCompiled)
   {
   OMR::CriticalSection getRemoteROMClass(getROMMapMonitor());
   // check whether info about j9method exists
   auto & j9methodMap = getJ9MethodMap();
   auto it = j9methodMap.find((J9Method*)method);
   if (it != j9methodMap.end())
      {
      IPTable_t *iProfilerMap = it->second._IPData;
      if (!iProfilerMap)
         {
         // Check and update if method is compiled when collecting profiling data
         if (isCompiled)
            it->second._isCompiledWhenProfiling = true;

         // allocate a new iProfiler map
         iProfilerMap = new (_persistentMemory->_persistentAllocator.get()) IPTable_t(
            IPTable_t::allocator_type(_persistentMemory->_persistentAllocator.get())
         );
         if (iProfilerMap)
            {
            it->second._IPData = iProfilerMap;
            // entry could be null; this means that the method has no IProfiler info
            if (entry)
               iProfilerMap->insert({ byteCodeIndex, entry });
            return true;
            }
         }
      else
         {
         if (entry)
            iProfilerMap->insert({ byteCodeIndex, entry });
         return true;
         }
      }
   else
      {
      // JITServer TODO: count how many times we cannot cache. There should be very few instances if at all.
      }
   return false; // false means that caching attempt failed
   }

void
ClientSessionData::printStats()
   {
   PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);
   j9tty_printf(PORTLIB, "\tNum cached ROM classes: %d\n", _romClassMap.size());
   j9tty_printf(PORTLIB, "\tNum cached ROM methods: %d\n", _J9MethodMap.size());
   size_t total = 0;
   for (auto& it : _romClassMap)
      total += it.second._romClass->romSize;

   j9tty_printf(PORTLIB, "\tTotal size of cached ROM classes + methods: %d bytes\n", total);
   }

ClientSessionData::ClassInfo::ClassInfo(TR_PersistentMemory *persistentMemory) :
   _romClass(NULL),
   _remoteRomClass(NULL),
   _methodsOfClass(NULL),
   _baseComponentClass(NULL),
   _numDimensions(0),
   _parentClass(NULL),
   _interfaces(NULL),
   _byteOffsetToLockword(0),
   _classHasFinalFields(false),
   _classInitialized(false),
   _classDepthAndFlags(0),
   _leafComponentClass(NULL),
   _classLoader(NULL),
   _hostClass(NULL),
   _componentClass(NULL),
   _arrayClass(NULL),
   _totalInstanceSize(0),
   _constantPool(NULL),
   _classFlags(0),
   _classChainOffsetIdentifyingLoader(0),
   _classNameIdentifyingLoader(),
   _aotCacheClassRecord(NULL),
   _arrayElementSize(0),
   _defaultValueSlotAddress(NULL),
   _classOfStaticCache(decltype(_classOfStaticCache)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _constantClassPoolCache(decltype(_constantClassPoolCache)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _fieldAttributesCache(decltype(_fieldAttributesCache)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _staticAttributesCache(decltype(_staticAttributesCache)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _fieldAttributesCacheAOT(decltype(_fieldAttributesCacheAOT)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _staticAttributesCacheAOT(decltype(_fieldAttributesCacheAOT)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _jitFieldsCache(decltype(_jitFieldsCache)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _fieldOrStaticDeclaringClassCache(decltype(_fieldOrStaticDeclaringClassCache)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _fieldOrStaticDefiningClassCache(decltype(_fieldOrStaticDefiningClassCache)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _J9MethodNameCache(decltype(_J9MethodNameCache)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _referencingClassLoaders(decltype(_referencingClassLoaders)::allocator_type(persistentMemory->_persistentAllocator.get()))
   {
   }

void
ClientSessionData::ClassInfo::freeClassInfo(TR_PersistentMemory *persistentMemory)
   {
   JITServerHelpers::freeRemoteROMClass(_romClass, persistentMemory);

   // free cached _interfaces
   _interfaces->~PersistentVector<TR_OpaqueClassBlock *>();
   persistentMemory->freePersistentMemory(_interfaces);
   }

ClientSessionData::VMInfo *
ClientSessionData::getOrCacheVMInfo(JITServer::ServerStream *stream)
   {
   if (!_vmInfo)
      {
      stream->write(JITServer::MessageType::VM_getVMInfo, JITServer::Void());
      auto recv = stream->read<VMInfo, std::vector<CacheDescriptor>, std::string>();
      _vmInfo = new (_persistentMemory->_persistentAllocator.get()) VMInfo(std::get<0>(recv));
      _vmInfo->_j9SharedClassCacheDescriptorList = reconstructJ9SharedClassCacheDescriptorList(std::get<1>(recv));
      _aotCacheName = std::get<2>(recv);
      }
   return _vmInfo;
   }

J9SharedClassCacheDescriptor *
ClientSessionData::reconstructJ9SharedClassCacheDescriptorList(const std::vector<ClientSessionData::CacheDescriptor> &listOfCacheDescriptors)
   {
   J9SharedClassCacheDescriptor * cur = NULL;
   J9SharedClassCacheDescriptor * prev = NULL;
   J9SharedClassCacheDescriptor * head = NULL;
   for (size_t i = 0; i < listOfCacheDescriptors.size(); i++)
      {
      auto cacheDesc = listOfCacheDescriptors[i];
      cur = new (_persistentMemory->_persistentAllocator.get()) J9SharedClassCacheDescriptor();
      cur->cacheStartAddress = (J9SharedCacheHeader *)cacheDesc.cacheStartAddress;
      cur->cacheSizeBytes = cacheDesc.cacheSizeBytes;
      cur->romclassStartAddress = (void *)cacheDesc.romClassStartAddress;
      cur->metadataStartAddress = (void *)cacheDesc.metadataStartAddress;
      if (prev)
         {
         prev->next = cur;
         cur->previous = prev;
         }
      else
         {
         head = cur;
         }
      prev = cur;
      }
   if (!head)
      return NULL;
   head->previous = prev; // assign head's previous to tail
   prev->next = head; // assign tail's previous to head
   return head;
   }

void
ClientSessionData::destroyJ9SharedClassCacheDescriptorList()
   {
   if (!_vmInfo->_j9SharedClassCacheDescriptorList)
      return;
   J9SharedClassCacheDescriptor * cur = _vmInfo->_j9SharedClassCacheDescriptorList;
   J9SharedClassCacheDescriptor * next = NULL;
   _vmInfo->_j9SharedClassCacheDescriptorList->previous->next = NULL; // break the circular links by setting tail's next pointer to be NULL
   while (cur)
      {
      next = cur->next;
      _persistentMemory->freePersistentMemory(cur);
      cur = next;
      }
   _vmInfo->_j9SharedClassCacheDescriptorList = NULL;
   }


void
ClientSessionData::clearCaches(bool locked)
   {
   TR_ASSERT(!_inUse || _sequencingMonitor->owned_by_self(), "Must have sequencing monitor");
   TR_ASSERT(_numActiveThreads == 0 || locked, "Must have no active threads when accessing without locks");

   _classBySignatureMap.clear();

   if (_unloadedClassAddresses)
      {
      _unloadedClassAddresses->destroy(_persistentMemory);
      _persistentMemory->freePersistentMemory(_unloadedClassAddresses);
      _unloadedClassAddresses = NULL;
      }

   // Free memory for all hashtables with IProfiler info
   for (auto &it : _J9MethodMap)
      {
      IPTable_t *ipDataHT = it.second._IPData;
      // It it exists, walk the collection of <pc, TR_IPBytecodeHashTableEntry*> mappings
      if (ipDataHT)
         {
         for (auto &entryIt : *ipDataHT)
            {
            auto entryPtr = entryIt.second;
            if (entryPtr)
               _persistentMemory->freePersistentMemory(entryPtr);
            }
         ipDataHT->~IPTable_t();
         _persistentMemory->freePersistentMemory(ipDataHT);
         it.second._IPData = NULL;
         }
      }

   _J9MethodMap.clear();
   // Free memory for j9class info
   for (auto &it : _romClassMap)
      it.second.freeClassInfo(_persistentMemory);

   _romClassMap.clear();

   _classChainDataMap.clear();
   _constantPoolToClassMap.clear();

   _registeredJ2IThunksMap.clear();
   _registeredInvokeExactJ2IThunksSet.clear();
   _bClassUnloadingAttempt = false;

   if (_chTable)
      {
      // Free CH table
      _chTable->~JITServerPersistentCHTable();
      _persistentMemory->freePersistentMemory(_chTable);
      _chTable = NULL;
      }

   _wellKnownClasses.clear();
   _aotCacheKnownIds.clear();

   setCachesAreCleared(true);
   }

void
ClientSessionData::clearCachesLocked(TR_J9VMBase *fe)
   {
   // Acquire all required locks to clear client session caches
   // This should be used when caches need to be cleared
   // while the client is still sending requests
   writeAcquireClassUnloadRWMutex();
      {
      OMR::CriticalSection processUnloadedClasses(getROMMapMonitor());
      clearCaches(true);
      }
   writeReleaseClassUnloadRWMutex();
   }

void
ClientSessionData::destroy(ClientSessionData *clientSession)
   {
   TR_PersistentMemory *persistentMemory = clientSession->persistentMemory();
   bool usesPerClientMemory = clientSession->usesPerClientMemory();

   // Since the client session and all its persistent objects are allocated with its own persistent
   // allocator, we can avoid calling the destructor. All per-client persistent allocations are
   // automatically freed when the allocator instance is destroyed. The only objects that need to
   // be explicitly destroyed are the ones allocated globally, e.g. monitors and shared ROMClasses.
   //
   // This optimization is mostly useful with AOT cache (otherwise its performance
   // impact is negligible) and can be error-prone (i.e. resulting in memory leaks
   // for globally allocated objects). We only enable it when using AOT cache.
   auto compInfo = TR::CompilationInfo::get();
   bool useAOTCache = compInfo->getPersistentInfo()->getJITServerUseAOTCache();
   if (usesPerClientMemory && useAOTCache)
      {
      // Destroy objects that are allocated globally:
      // shared ROMClasses (if enabled), monitors, std::strings
      auto sharedROMClassCache = compInfo->getJITServerSharedROMClassCache();
      for (auto &it : clientSession->_romClassMap)
         {
         if (sharedROMClassCache)
            sharedROMClassCache->release(it.second._romClass);
         it.second._classNameIdentifyingLoader.~basic_string();
         for (auto &kv : it.second._J9MethodNameCache)
            kv.second.~J9MethodNameAndSignature();
         }

      for (auto &it : clientSession->_classBySignatureMap)
         it.first.~ClassLoaderStringPair();
      for (auto &it : clientSession->_registeredJ2IThunksMap)
         it.first.first.~basic_string();
      for (auto &it : clientSession->_registeredInvokeExactJ2IThunksSet)
         it.first.~basic_string();

      clientSession->destroyMonitors();
      if (clientSession->_chTable)
         TR::Monitor::destroy(clientSession->_chTable->getCHTableMonitor());
      clientSession->_aotCacheName.~basic_string();
      }
   else
      {
      clientSession->~ClientSessionData();
      persistentMemory->freePersistentMemory(clientSession);
      }

   if (usesPerClientMemory)
      {
      persistentMemory->_persistentAllocator.get().~PersistentAllocator();
      TR::Compiler->rawAllocator.deallocate(&persistentMemory->_persistentAllocator.get());
      TR::Compiler->rawAllocator.deallocate(persistentMemory);
      }
   }

// notifyAndDetachFirstWaitingThread needs to be executed with sequencingMonitor in hand
TR_MethodToBeCompiled *
ClientSessionData::notifyAndDetachFirstWaitingThread()
   {
   TR_MethodToBeCompiled *entry = _OOSequenceEntryList;
   if (entry)
      {
      entry->getMonitor()->enter();
      entry->getMonitor()->notifyAll();
      entry->getMonitor()->exit();
      _OOSequenceEntryList = entry->_next;
      }
   return entry;
   }

TR_PersistentCHTable *
ClientSessionData::getCHTable()
   {
   if (!_chTable)
      {
      _chTable = new (_persistentMemory) JITServerPersistentCHTable(_persistentMemory);
      }
   return _chTable;
   }

template <typename map, typename key>
void ClientSessionData::purgeCache(std::vector<ClassUnloadedData> *unloadedClasses, map& m, key ClassUnloadedData::*k)
   {
   ClassUnloadedData *data = unloadedClasses->data();
   std::vector<ClassUnloadedData>::iterator it = unloadedClasses->begin();
   while (it != unloadedClasses->end())
      {
      if (it->_cached)
         {
         m.erase((data->*k));
         }
      else
         {
         //If the class is not cached this is the place to iterate the cache(Map) for deleting the entry by value rather then key.
         auto itClass = m.begin();
         while (itClass != m.end())
            {
            if (itClass->second == data->_class)
               {
               m.erase(itClass);
               break;
               }
            ++itClass;
            }
         }
      //DO NOT remove the entry from the unloadedClasses as it will be needed to purge other caches.
      ++it;
      ++data;
      }
   }

void
ClientSessionData::readAcquireClassUnloadRWMutex(TR::CompilationInfoPerThreadBase *compInfoPT)
   {
   // compInfoPT must be associated with the compilation thread that calls this
   omrthread_rwmutex_enter_read(_classUnloadRWMutex);
   static_cast<TR::CompilationInfoPerThreadRemote *>(compInfoPT)->incrementClassUnloadReadMutexDepth();
   }

void
ClientSessionData::readReleaseClassUnloadRWMutex(TR::CompilationInfoPerThreadBase *compInfoPT)
   {
   // compInfoPT must be associated with the compilation thread that calls this
   static_cast<TR::CompilationInfoPerThreadRemote *>(compInfoPT)->decrementClassUnloadReadMutexDepth();
   omrthread_rwmutex_exit_read(_classUnloadRWMutex);
   }

void
ClientSessionData::writeAcquireClassUnloadRWMutex()
   {
    _bClassUnloadingAttempt = true;
   omrthread_rwmutex_enter_write(_classUnloadRWMutex);
   }

void
ClientSessionData::writeReleaseClassUnloadRWMutex()
   {
   _bClassUnloadingAttempt = false;
   omrthread_rwmutex_exit_write(_classUnloadRWMutex);
   }

const void *
ClientSessionData::getCachedWellKnownClassChainOffsets(unsigned int includedClasses, size_t numClasses,
                                                       const uintptr_t *classChainOffsets,
                                                       const AOTCacheWellKnownClassesRecord *&wellKnownClassesRecord)
   {
   TR_ASSERT(numClasses <= WELL_KNOWN_CLASS_COUNT, "Too many well-known classes");
   OMR::CriticalSection wellKnownClasses(_wellKnownClassesMonitor);

   if (_wellKnownClasses._includedClasses == includedClasses &&
       memcmp(_wellKnownClasses._classChainOffsets, classChainOffsets,
              numClasses * sizeof(classChainOffsets[0])) == 0)
      {
      TR_ASSERT(_wellKnownClasses._wellKnownClassChainOffsets, "Cached well-known class chain offsets pointer is NULL");
      wellKnownClassesRecord = _wellKnownClasses._aotCacheWellKnownClassesRecord;
      return _wellKnownClasses._wellKnownClassChainOffsets;
      }

   wellKnownClassesRecord = NULL;
   return NULL;
   }

void
ClientSessionData::cacheWellKnownClassChainOffsets(unsigned int includedClasses, size_t numClasses,
                                                   const uintptr_t *classChainOffsets, const void *wellKnownClassChainOffsets,
                                                   const AOTCacheClassChainRecord *const *classChainRecords,
                                                   const AOTCacheWellKnownClassesRecord *&wellKnownClassesRecord)
   {
   TR_ASSERT(wellKnownClassChainOffsets, "Well-known class chain offsets pointer is NULL");
   TR_ASSERT(numClasses <= WELL_KNOWN_CLASS_COUNT, "Too many well-known classes");
   OMR::CriticalSection wellKnownClasses(_wellKnownClassesMonitor);

   _wellKnownClasses._includedClasses = includedClasses;
   memcpy(_wellKnownClasses._classChainOffsets, classChainOffsets,
          numClasses * sizeof(classChainOffsets[0]));
   // Zero out the tail of the array
   memset(_wellKnownClasses._classChainOffsets + numClasses, 0,
          (WELL_KNOWN_CLASS_COUNT - numClasses) * sizeof(classChainOffsets[0]));
   _wellKnownClasses._wellKnownClassChainOffsets = wellKnownClassChainOffsets;

   // Create and save AOT cache well-known classes record if requested
   wellKnownClassesRecord = classChainRecords ?
      _aotCache->getWellKnownClassesRecord(classChainRecords, numClasses, includedClasses) : NULL;
   _wellKnownClasses._aotCacheWellKnownClassesRecord = wellKnownClassesRecord;
   }

JITServerAOTCache *
ClientSessionData::getOrCreateAOTCache(JITServer::ServerStream *stream)
   {
   if (!_vmInfo)
      getOrCacheVMInfo(stream);

   if (!_aotCache && _vmInfo->_useAOTCache)
      {
      if (auto aotCacheMap = TR::CompilationInfo::get()->getJITServerAOTCacheMap())
         {
         bool cacheIsBeingLoadedFromDisk = false;
         auto aotCache = aotCacheMap->get(_aotCacheName, _clientUID, cacheIsBeingLoadedFromDisk);
         if (!aotCache)
            {
            if (cacheIsBeingLoadedFromDisk)
               {
               if (TR::Options::getVerboseOption(TR_VerboseJITServer))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                     "clientUID=%llu requested AOT cache but currently that cache is being loaded from disk", (unsigned long long)_clientUID);
               }
            else // We cannot create the cache because memory limit is reached
               {
               _vmInfo->_useAOTCache = false;
               if (TR::Options::getVerboseOption(TR_VerboseJITServer))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                     "clientUID=%llu requested AOT cache but the AOT cache size limit has been reached, disabling AOT cache",
                     (unsigned long long)_clientUID
                  );
               }
               return NULL;
            }

         auto record = aotCache->getAOTHeaderRecord(&_vmInfo->_aotHeader, _clientUID);
         if (record)
            {
            _aotHeaderRecord = record;
            // We use a barrier here to ensure that _aotHeaderRecord is set before _aotCache, as other code
            // assumes that _aotHeaderRecord is non-null whenever _aotCache is non-null.
            VM_AtomicSupport::writeBarrier();
            _aotCache = aotCache;
            }
         else
            {
            _vmInfo->_useAOTCache = false;
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                  "clientUID=%llu failed to create AOT header record due to AOT cache size limit, disabling AOT cache",
                  (unsigned long long)_clientUID
               );
            }
         }
      else
         {
         _vmInfo->_useAOTCache = false;
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
               "clientUID=%llu requested AOT cache while it is disabled at the server", (unsigned long long)_clientUID
            );
         }
      }

   return _aotCache;
   }

const AOTCacheClassRecord *
ClientSessionData::getClassRecord(ClientSessionData::ClassInfo &classInfo, bool &missingLoaderInfo)
{
   if (!classInfo._aotCacheClassRecord)
      {
      auto &name = classInfo._classNameIdentifyingLoader;
      if (name.empty())
         {
         TR_ASSERT(!classInfo._classChainOffsetIdentifyingLoader,
                   "Valid class chain offset but missing class name identifying loader");
         missingLoaderInfo = true;
         return NULL;
         }

      auto classLoaderRecord = _aotCache->getClassLoaderRecord((const uint8_t *)name.data(), name.size());
      if (!classLoaderRecord)
         {
         return NULL;
         }
      classInfo._aotCacheClassRecord = _aotCache->getClassRecord(classLoaderRecord, classInfo._romClass);
      if (classInfo._aotCacheClassRecord)
         {
         // The name string is no longer needed; free the memory used by it by setting it to an empty string
         std::string().swap(classInfo._classNameIdentifyingLoader);
         }
      }

   return classInfo._aotCacheClassRecord;
}

const AOTCacheClassRecord *
ClientSessionData::getClassRecord(J9Class *clazz, bool &missingLoaderInfo, bool &uncachedClass)
   {
   TR_ASSERT(getROMMapMonitor()->owned_by_self(), "Must hold ROMMapMonitor");

   auto it = getROMClassMap().find(clazz);
   if (it == getROMClassMap().end())
      {
      uncachedClass = true;
      return NULL;
      }

   auto record = getClassRecord(it->second, missingLoaderInfo);
   return record;
   }

const AOTCacheClassRecord *
ClientSessionData::getClassRecord(J9Class *clazz, JITServer::ServerStream *stream, bool &missingLoaderInfo)
   {
   const AOTCacheClassRecord *record = NULL;
   bool uncachedClass = false;
      {
      OMR::CriticalSection cs(getROMMapMonitor());
      record = getClassRecord(clazz, missingLoaderInfo, uncachedClass);
      }

   if (missingLoaderInfo)
      {
      // Request and cache missing class loader info from the client
      stream->write(JITServer::MessageType::SharedCache_getClassChainOffsetIdentifyingLoader, clazz, true);
      auto recv = stream->read<uintptr_t, std::string>();
      uintptr_t offset = std::get<0>(recv);
      auto &name = std::get<1>(recv);

      if (offset)
         {
         OMR::CriticalSection cs(getROMMapMonitor());
         auto it = getROMClassMap().find((J9Class *)clazz);
         TR_ASSERT(it != getROMClassMap().end(), "Class %p must be already cached", clazz);
         it->second._classChainOffsetIdentifyingLoader = offset;
         it->second._classNameIdentifyingLoader = name;
         record = getClassRecord(it->second, missingLoaderInfo);
         TR_ASSERT(!missingLoaderInfo, "Class %p cannot have missing loader info as we've just received it from the client", clazz);
         }
      else if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "ERROR: clientUID %llu failed to get class name identifying loader for class %p",
            (unsigned long long)_clientUID, clazz
         );
         }
      }
   else if (uncachedClass)
      {
      // Request and cache class info from the client
      JITServerHelpers::ClassInfoTuple classInfoTuple;
      auto romClass = JITServerHelpers::getRemoteROMClass(clazz, stream, _persistentMemory, classInfoTuple);
      JITServerHelpers::cacheRemoteROMClassOrFreeIt(this, clazz, romClass, classInfoTuple);

      OMR::CriticalSection cs(getROMMapMonitor());
      record = getClassRecord(clazz, missingLoaderInfo, uncachedClass);
      TR_ASSERT(!uncachedClass, "Class %p cannot be uncached", clazz);
      }

   return record;
   }

const AOTCacheMethodRecord *
ClientSessionData::getMethodRecord(J9Method *method, J9Class *definingClass, JITServer::ServerStream *stream)
   {
      {
      OMR::CriticalSection cs(getROMMapMonitor());
      auto it = getJ9MethodMap().find(method);
      if ((it != getJ9MethodMap().end()) && it->second._aotCacheMethodRecord)
         return it->second._aotCacheMethodRecord;
      }

   bool missingLoaderInfo = false;
   auto classRecord = getClassRecord(definingClass, stream, missingLoaderInfo);
   if (!classRecord)
      return NULL;

   OMR::CriticalSection cs(getROMMapMonitor());
   auto it = getJ9MethodMap().find(method);
   TR_ASSERT(it != getJ9MethodMap().end(), "Method %p must be already cached", method);
   it->second._aotCacheMethodRecord = _aotCache->getMethodRecord(classRecord, it->second._index, it->second._romMethod);
   return it->second._aotCacheMethodRecord;
   }

const AOTCacheClassChainRecord *
ClientSessionData::getClassChainRecord(J9Class *clazz, uintptr_t *classChain,
                                       const std::vector<J9Class *> &ramClassChain, JITServer::ServerStream *stream,
                                       bool &missingLoaderInfo)
   {
   TR_ASSERT(!ramClassChain.empty() && (ramClassChain.size() <= TR_J9SharedCache::maxClassChainLength),
             "Invalid class chain length: %zu", ramClassChain.size());

   // Check if this class chain record is already cached
      {
      OMR::CriticalSection cs(getClassChainDataMapMonitor());
      auto it = getClassChainDataMap().find(clazz);
      if ((it != getClassChainDataMap().end()) && it->second._aotCacheClassChainRecord)
         return it->second._aotCacheClassChainRecord;
      }

   const AOTCacheClassRecord *classRecords[TR_J9SharedCache::maxClassChainLength] = {0};
   size_t uncachedIndexes[TR_J9SharedCache::maxClassChainLength] = {0};
   std::vector<J9Class *> uncachedRAMClasses;
   uncachedRAMClasses.reserve(ramClassChain.size());
   size_t missingLoaderRecordIndexes[TR_J9SharedCache::maxClassChainLength] = {0};
   size_t numMissingLoaderRecords = 0;

   // Get class records for which all info is already available, remembering classes that we need to request info for
      {
      OMR::CriticalSection cs(getROMMapMonitor());

      for (size_t i = 0; i < ramClassChain.size(); ++i)
         {
         bool missingLoaderRecord = false;
         bool uncachedClass = false;
         if (!(classRecords[i] = getClassRecord(ramClassChain[i], missingLoaderRecord, uncachedClass)))
            {
            if (missingLoaderRecord)
               {
               missingLoaderRecordIndexes[numMissingLoaderRecords++] = i;
               }
            else if (uncachedClass)
               {
               uncachedIndexes[uncachedRAMClasses.size()] = i;
               uncachedRAMClasses.push_back(ramClassChain[i]);
               }
            else
               {
               // There must have been an allocation failure.
               return NULL;
               }
            }
         }
      }

   if (!uncachedRAMClasses.empty())
      {
      // Request uncached classes from the client and cache them
      stream->write(JITServer::MessageType::AOTCache_getROMClassBatch, uncachedRAMClasses);
      auto recv = stream->read<std::vector<JITServerHelpers::ClassInfoTuple>>();
      auto classInfoTuples = std::get<0>(recv);
      JITServerHelpers::cacheRemoteROMClassBatch(this, uncachedRAMClasses, classInfoTuples);

      // Get class records for newly cached classes, remembering classes with missing class loader info
      OMR::CriticalSection cs(getROMMapMonitor());
      for (size_t i = 0; i < uncachedRAMClasses.size(); ++i)
         {
         bool missingLoaderRecord = false;
         bool uncachedClass = false;
         if (!(classRecords[uncachedIndexes[i]] = getClassRecord(uncachedRAMClasses[i], missingLoaderRecord, uncachedClass)))
            {
            TR_ASSERT(!uncachedClass, "Class %p must be already cached", uncachedRAMClasses[i]);
            if (missingLoaderRecord)
               {
               missingLoaderRecordIndexes[numMissingLoaderRecords++] = uncachedIndexes[i];
               }
            else
               {
               // There must have been an allocation failure.
               return NULL;
               }
            }
         }
      }

   // Get remaining class records, requesting their missing class loader info from the client
   for (size_t i = 0; i < numMissingLoaderRecords; ++i)
      {
      size_t idx = missingLoaderRecordIndexes[i];
      if (!(classRecords[idx] = getClassRecord(ramClassChain[idx], stream, missingLoaderInfo)))
         return NULL;
      }

   // Cache the new class chain record
   auto record = _aotCache->getClassChainRecord(classRecords, ramClassChain.size());
   OMR::CriticalSection cs(getClassChainDataMapMonitor());
   auto result = getClassChainDataMap().insert({ clazz, { classChain, record } });
   if (!result.second)
      result.first->second._aotCacheClassChainRecord = record;
   return record;
   }


ClientSessionHT*
ClientSessionHT::allocate()
   {
   return new (TR::Compiler->persistentGlobalAllocator()) ClientSessionHT();
   }

ClientSessionHT::ClientSessionHT() :
   _clientSessionMap(decltype(_clientSessionMap)::allocator_type(TR::Compiler->persistentGlobalAllocator())),
   TIME_BETWEEN_PURGES(TR::Options::_timeBetweenPurges),
   OLD_AGE(TR::Options::_oldAge), // 90 minutes
   OLD_AGE_UNDER_LOW_MEMORY(TR::Options::_oldAgeUnderLowMemory), // 5 minutes
   _compInfo(TR::CompilationController::getCompilationInfo())
   {
   PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);
   _timeOfLastPurge = j9time_current_time_millis();
   _clientSessionMap.reserve(250); // allow room for at least 250 clients
   }

// The destructor is currently never called because the server does not exit cleanly
ClientSessionHT::~ClientSessionHT()
   {
   for (auto iter = _clientSessionMap.begin(); iter != _clientSessionMap.end(); ++iter)
      {
      ClientSessionData::destroy(iter->second); // delete the client data
      _clientSessionMap.erase(iter); // delete the mapping from the hashtable
      }
   }

// Search the clientSessionHashtable for the given clientUID and return the
// data corresponding to the client.
// If the clientUID does not already exist in the HT, insert a new blank entry.
// Must have compilation monitor in hand when calling this function.
// Side effects: _inUse is incremented on the ClientSessionData
//               _lastProcessedCriticalSeqNo is populated if a new ClientSessionData is created
//                timeOflastAccess is updated with current time.
ClientSessionData *
ClientSessionHT::findOrCreateClientSession(uint64_t clientUID, uint32_t seqNo, bool *newSessionWasCreated, J9JITConfig *jitConfig)
   {
   *newSessionWasCreated = false;
   ClientSessionData *clientData = findClientSession(clientUID);
   if (!clientData)
      {
      TR_PersistentMemory *sessionMemory = NULL;
      bool usesPerClientMemory = true;
      static const char* disablePerClientPersistentAllocation = feGetEnv("TR_DisablePerClientPersistentAllocation");
      if (!disablePerClientPersistentAllocation)
         {
         // allocate new persistent allocator and memory and store them inside the client session
         TR::PersistentAllocatorKit kit(1 << 20/*1 MB*/, *TR::Compiler->javaVM);
         auto allocator = new (TR::Compiler->rawAllocator) TR::PersistentAllocator(kit);
         try
            {
            sessionMemory = new (TR::Compiler->rawAllocator) TR_PersistentMemory(jitConfig, *allocator);
            }
         catch (...)
            {
            allocator->~PersistentAllocator();
            TR::Compiler->rawAllocator.deallocate(allocator);
            throw;
            }
         }
      else
         {
         // passed env variable to disable per-client allocation, always use the global allocator
         usesPerClientMemory = false;
         sessionMemory = TR::Compiler->persistentGlobalMemory();
         }

      // If this is the first client, initialize the shared ROMClass cache
      if (_clientSessionMap.empty())
         {
         if (auto cache = TR::CompilationInfo::get()->getJITServerSharedROMClassCache())
            cache->initialize(jitConfig);
         }

      // allocate a new ClientSessionData object and create a clientUID mapping
      clientData = new (sessionMemory) ClientSessionData(clientUID, seqNo, sessionMemory, usesPerClientMemory);
      if (clientData)
         {
         _clientSessionMap[clientUID] = clientData;
         *newSessionWasCreated = true;
         if (TR::Options::getVerboseOption(TR_VerboseJITServer) ||
             TR::Options::getVerboseOption(TR_VerboseJITServerConns))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                                           "t=%6u A new client (clientUID=%llu) connected. Server allocated a new client session.",
                                           (uint32_t) _compInfo->getPersistentInfo()->getElapsedTime(),
                                           (unsigned long long) clientUID);
            }
         }
      else
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "ERROR: Server could not allocate client session data");
         }
      }
   return clientData;
   }

// Search the clientSessionHashtable for the given clientUID and delete  the
// data corresponding to the client. Return true if client data found and deleted.
// Must have compilation monitor in hand when calling this function.
bool
ClientSessionHT::deleteClientSession(uint64_t clientUID, bool forDeletion)
   {
   ClientSessionData *clientData = NULL;
   auto clientDataIt = _clientSessionMap.find(clientUID);
   if (clientDataIt != _clientSessionMap.end())
      {
      clientData = clientDataIt->second;
      if (forDeletion)
         clientData->markForDeletion();

      if ((clientData->getInUse() == 0) && clientData->isMarkedForDeletion())
         {
         ClientSessionData::destroy(clientData); // delete the client data
         _clientSessionMap.erase(clientDataIt); // delete the mapping from the hashtable

         // If this was the last client, shutdown the shared ROMClass cache
         if (_clientSessionMap.empty())
            {
            if (auto cache = TR::CompilationInfo::get()->getJITServerSharedROMClassCache())
               cache->shutdown();
            }

         return true;
         }
      }
   return false;
   }

// Search the clientSessionHashtable for the given clientUID and return the
// data corresponding to the client.
// Must have compilation monitor in hand when calling this function.
// Side effects: _inUse is incremented on the ClientSessionData and the
// timeOflastAccess is updated with curent time.
ClientSessionData *
ClientSessionHT::findClientSession(uint64_t clientUID)
   {
   ClientSessionData *clientData = NULL;
   auto clientDataIt = _clientSessionMap.find(clientUID);
   if (clientDataIt != _clientSessionMap.end())
      {
      // if clientData found in hashtable, update the access time before returning it
      clientData = clientDataIt->second;
      clientData->incInUse();
      clientData->updateTimeOfLastAccess();
      }
   return clientData;
   }


// Purge the old client session data from the hashtable and
// update the timeOfLastPurge.
// Entries with _inUse > 0 must be left alone, though having
// very old entries in use it's a sign of a programming error.
// This routine must be executed with compilation monitor in hand.
void
ClientSessionHT::purgeOldDataIfNeeded()
   {
   PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);
   int64_t crtTime = j9time_current_time_millis();
   bool incomplete;
   int64_t oldAge = OLD_AGE;

   if (crtTime - _timeOfLastPurge > TIME_BETWEEN_PURGES)
      {
      uint64_t freePhysicalMemory = _compInfo->computeAndCacheFreePhysicalMemory(incomplete); //check if memory is free
      if (freePhysicalMemory != OMRPORT_MEMINFO_NOT_AVAILABLE && !incomplete)
         {
         if (freePhysicalMemory < TR::Options::getSafeReservePhysicalMemoryValue() + 4 * TR::Options::getScratchSpaceLowerBound())
            {
            oldAge = OLD_AGE_UNDER_LOW_MEMORY; //memory is low
            }
         }
      // Time for a purge operation.
      // Scan the entire table and delete old elements that are not in use
      for (auto iter = _clientSessionMap.begin(); iter != _clientSessionMap.end(); ++iter)
         {
         TR_ASSERT(iter->second->getInUse() >= 0, "_inUse=%d must be positive\n", iter->second->getInUse());
         if (iter->second->getInUse() == 0 &&
            crtTime - iter->second->getTimeOflastAccess() > oldAge)
            {
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "t=%u Server will purge session data for clientUID %llu of age %lld. Number of clients before purge: %u",
                  (uint32_t)_compInfo->getPersistentInfo()->getElapsedTime(), (unsigned long long)iter->first, (long long)oldAge, size());
            ClientSessionData::destroy(iter->second); // delete the client data
            _clientSessionMap.erase(iter); // delete the mapping from the hashtable
            }
         }
      _timeOfLastPurge = crtTime;

      // JITServer TODO: keep stats on how many elements were purged
      }
   }

// to print these stats,
// set the env var `TR_PrintJITServerCacheStats=1`
// run the server with `-Xdump:jit:events=user`
// then `kill -3` it when you want to print them
void
ClientSessionHT::printStats()
   {
   PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);
   j9tty_printf(PORTLIB, "Client sessions:\n");
   for (auto session : _clientSessionMap)
      {
      j9tty_printf(PORTLIB, "Session for id %d:\n", session.first);
      session.second->printStats();
      }
   }
