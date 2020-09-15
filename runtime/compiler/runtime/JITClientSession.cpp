/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

#include "runtime/JITClientSession.hpp"

#include "control/CompilationRuntime.hpp" // for CompilationInfo
#include "control/MethodToBeCompiled.hpp" // for TR_MethodToBeCompiled
#include "control/JITServerHelpers.hpp"
#include "env/ut_j9jit.h"
#include "net/ServerStream.hpp" // for JITServer::ServerStream
#include "runtime/RuntimeAssumptions.hpp" // for TR_AddressSet
#include "env/JITServerPersistentCHTable.hpp"


ClientSessionData::ClientSessionData(uint64_t clientUID, uint32_t seqNo) : 
   _clientUID(clientUID), _expectedSeqNo(seqNo), _maxReceivedSeqNo(seqNo), _OOSequenceEntryList(NULL),
   _chTable(NULL),
   _romClassMap(decltype(_romClassMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _J9MethodMap(decltype(_J9MethodMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classBySignatureMap(decltype(_classBySignatureMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classChainDataMap(decltype(_classChainDataMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _constantPoolToClassMap(decltype(_constantPoolToClassMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _unloadedClassAddresses(NULL),
   _requestUnloadedClasses(true),
   _staticFinalDataMap(decltype(_staticFinalDataMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _rtResolve(false),
   _registeredJ2IThunksMap(decltype(_registeredJ2IThunksMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _registeredInvokeExactJ2IThunksSet(decltype(_registeredInvokeExactJ2IThunksSet)::allocator_type(TR::Compiler->persistentAllocator()))
   {
   updateTimeOfLastAccess();
   _javaLangClassPtr = NULL;
   _inUse = 1;
   _numActiveThreads = 0;
   _romMapMonitor = TR::Monitor::create("JIT-JITServerROMMapMonitor");
   _classMapMonitor = TR::Monitor::create("JIT-JITServerClassMapMonitor");
   _classChainDataMapMonitor = TR::Monitor::create("JIT-JITServerClassChainDataMapMonitor");
   _sequencingMonitor = TR::Monitor::create("JIT-JITServerSequencingMonitor");
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
   }

ClientSessionData::~ClientSessionData()
   {
   clearCaches();
   _romMapMonitor->destroy();
   _classMapMonitor->destroy();
   _classChainDataMapMonitor->destroy();
   _sequencingMonitor->destroy();
   _constantPoolMapMonitor->destroy();
   _staticMapMonitor->destroy();
   if (_vmInfo)
      {
      destroyJ9SharedClassCacheDescriptorList();
      jitPersistentFree(_vmInfo);
      }
   _thunkSetMonitor->destroy();

   omrthread_rwmutex_destroy(_classUnloadRWMutex);
   _classUnloadRWMutex = NULL;
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
      _unloadedClassAddresses = new (PERSISTENT_NEW) TR_AddressSet(trPersistentMemory, maxRanges);
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
                  jitPersistentFree(ipDataHT);
                  iter->second._IPData = NULL;
                  }
               _J9MethodMap.erase(j9method);
               }
            }
         it->second.freeClassInfo();
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
         iProfilerMap = new (PERSISTENT_NEW) IPTable_t(IPTable_t::allocator_type(TR::Compiler->persistentAllocator()));
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

ClientSessionData::ClassInfo::ClassInfo() :
   _romClass(NULL),
   _remoteRomClass(NULL),
   _methodsOfClass(NULL),
   _baseComponentClass(NULL),
   _numDimensions(0),
   _parentClass(NULL),
   _interfaces(NULL),
   _classHasFinalFields(false),
   _classDepthAndFlags(0),
   _classInitialized(false),
   _byteOffsetToLockword(0),
   _leafComponentClass(NULL),
   _classLoader(NULL),
   _hostClass(NULL),
   _componentClass(NULL),
   _arrayClass(NULL),
   _totalInstanceSize(0),
   _constantPool(NULL),
   _classFlags(0),
   _classChainOffsetOfIdentifyingLoaderForClazz(0),
   _remoteROMStringsCache(decltype(_remoteROMStringsCache)::allocator_type(TR::Compiler->persistentAllocator())),
   _fieldOrStaticNameCache(decltype(_fieldOrStaticNameCache)::allocator_type(TR::Compiler->persistentAllocator())),
   _classOfStaticCache(decltype(_classOfStaticCache)::allocator_type(TR::Compiler->persistentAllocator())),
   _constantClassPoolCache(decltype(_constantClassPoolCache)::allocator_type(TR::Compiler->persistentAllocator())),
   _fieldAttributesCache(decltype(_fieldAttributesCache)::allocator_type(TR::Compiler->persistentAllocator())),
   _staticAttributesCache(decltype(_staticAttributesCache)::allocator_type(TR::Compiler->persistentAllocator())),
   _fieldAttributesCacheAOT(decltype(_fieldAttributesCacheAOT)::allocator_type(TR::Compiler->persistentAllocator())),
   _staticAttributesCacheAOT(decltype(_fieldAttributesCacheAOT)::allocator_type(TR::Compiler->persistentAllocator())),
   _jitFieldsCache(decltype(_jitFieldsCache)::allocator_type(TR::Compiler->persistentAllocator())),
   _fieldOrStaticDeclaringClassCache(decltype(_fieldOrStaticDeclaringClassCache)::allocator_type(TR::Compiler->persistentAllocator())),
   _fieldOrStaticDefiningClassCache(decltype(_fieldOrStaticDefiningClassCache)::allocator_type(TR::Compiler->persistentAllocator())),	
   _J9MethodNameCache(decltype(_J9MethodNameCache)::allocator_type(TR::Compiler->persistentAllocator())),
   _referencingClassLoaders(decltype(_referencingClassLoaders)::allocator_type(TR::Compiler->persistentAllocator()))
   {
   }

void
ClientSessionData::ClassInfo::freeClassInfo()
   {
   TR_Memory::jitPersistentFree(_romClass);

   // free cached _interfaces
   _interfaces->~PersistentVector<TR_OpaqueClassBlock *>();
   jitPersistentFree(_interfaces);
   }

ClientSessionData::VMInfo *
ClientSessionData::getOrCacheVMInfo(JITServer::ServerStream *stream)
   {
   if (!_vmInfo)
      {
      stream->write(JITServer::MessageType::VM_getVMInfo, JITServer::Void());
      auto recv = stream->read<VMInfo, std::vector<CacheDescriptor> >();
      _vmInfo = new (PERSISTENT_NEW) VMInfo(std::get<0>(recv));
      _vmInfo->_j9SharedClassCacheDescriptorList = reconstructJ9SharedClassCacheDescriptorList(std::get<1>(recv));
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
      cur = new (PERSISTENT_NEW) J9SharedClassCacheDescriptor();
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
      jitPersistentFree(cur);
      cur = next;
      }
   _vmInfo->_j9SharedClassCacheDescriptorList = NULL;
   }


void
ClientSessionData::clearCaches()
   {
      {
      OMR::CriticalSection clearCache(getClassMapMonitor());
      _classBySignatureMap.clear();
      }
   OMR::CriticalSection getRemoteROMClass(getROMMapMonitor());

   if (_unloadedClassAddresses)
      {
      _unloadedClassAddresses->destroy();
      jitPersistentFree(_unloadedClassAddresses);
      _unloadedClassAddresses = NULL;
      }
  _requestUnloadedClasses = true;

   // Free memory for all hashtables with IProfiler info
   for (auto& it : _J9MethodMap)
      {
      IPTable_t *ipDataHT = it.second._IPData;
      // It it exists, walk the collection of <pc, TR_IPBytecodeHashTableEntry*> mappings
      if (ipDataHT)
         {
         for (auto& entryIt : *ipDataHT)
            {
            auto entryPtr = entryIt.second;
            if (entryPtr)
               jitPersistentFree(entryPtr);
            }
         ipDataHT->~IPTable_t();
         jitPersistentFree(ipDataHT);
         it.second._IPData = NULL;
         }
      }

   _J9MethodMap.clear();
   // Free memory for j9class info
   for (auto& it : _romClassMap)
      it.second.freeClassInfo();

   _romClassMap.clear();

   _classChainDataMap.clear();

   _registeredJ2IThunksMap.clear();
   _registeredInvokeExactJ2IThunksSet.clear();
   _bClassUnloadingAttempt = false;

   if (_chTable)
      {
      // Free CH table
      _chTable->~JITServerPersistentCHTable();
      jitPersistentFree(_chTable);
      _chTable = NULL;
      }
   }

void
ClientSessionData::destroy(ClientSessionData *clientSession)
   {
   clientSession->~ClientSessionData();
   TR_PersistentMemory::jitPersistentFree(clientSession);
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
      _chTable = new (PERSISTENT_NEW) JITServerPersistentCHTable(trPersistentMemory);
      }
   return _chTable;
   }

char *
ClientSessionData::ClassInfo::getRemoteROMString(int32_t &len, void *basePtr, std::initializer_list<size_t> offsets)
   {
   auto offsetFirst = *offsets.begin();
   auto offsetSecond = (offsets.size() == 2) ? *(offsets.begin() + 1) : 0;

   TR_ASSERT(offsetFirst < (1 << 16) && offsetSecond < (1 << 16), "Offsets are larger than 16 bits");
   TR_ASSERT(offsets.size() <= 2, "Number of offsets is greater than 2"); 

   // create a key for hashing into a table of strings
   TR_RemoteROMStringKey key;
   uint32_t offsetKey = (offsetFirst << 16) + offsetSecond;
   key._basePtr = basePtr;
   key._offsets = offsetKey;
   
   std::string *cachedStr = NULL;
   bool isCached = false;
   auto gotStr = _remoteROMStringsCache.find(key);
   if (gotStr != _remoteROMStringsCache.end())
      {
      cachedStr = &(gotStr->second);
      isCached = true;
      }

   // only make a query if a string hasn't been cached
   if (!isCached)
      {
      size_t offsetFromROMClass = (uint8_t*) basePtr - (uint8_t*) _romClass;
      std::string offsetsStr((char*) offsets.begin(), offsets.size() * sizeof(size_t));
      
      JITServer::ServerStream *stream = TR::CompilationInfo::getStream();      
      stream->write(JITServer::MessageType::ClassInfo_getRemoteROMString, _remoteRomClass, offsetFromROMClass, offsetsStr);
      cachedStr = &(_remoteROMStringsCache.insert({key, std::get<0>(stream->read<std::string>())}).first->second);
      }

   len = cachedStr->length();
   return &(cachedStr->at(0));
   }

// Takes a pointer to some data which is placed statically relative to the rom class,
// as well as a list of offsets to J9SRP fields. The first offset is applied before the first
// SRP is followed.
//
// If at any point while following the chain of SRP pointers we land outside the ROM class,
// then we fall back to getRemoteROMString which follows the same process on the client.
//
// This is a workaround because some data referenced in the ROM constant pool is located outside of
// it, but we cannot easily determine if the reference is outside or not (or even if it is a reference!)
// because the data is untyped.
char *
ClientSessionData::ClassInfo::getROMString(int32_t &len, void *basePtr, std::initializer_list<size_t> offsets)
   {
   uint8_t *ptr = (uint8_t*) basePtr;
   for (size_t offset : offsets)
      {
      ptr += offset;
      if (!JITServerHelpers::isAddressInROMClass(ptr, _romClass))
         return getRemoteROMString(len, basePtr, offsets);
      ptr = ptr + *(J9SRP*)ptr;
      }
   if (!JITServerHelpers::isAddressInROMClass(ptr, _romClass))
      return getRemoteROMString(len, basePtr, offsets);
   char *data = utf8Data((J9UTF8*) ptr, len);
   return data;
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
ClientSessionData::readAcquireClassUnloadRWMutex()
   {
   omrthread_rwmutex_enter_read(_classUnloadRWMutex);
   }

void
ClientSessionData::readReleaseClassUnloadRWMutex()
   {
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

ClientSessionHT*
ClientSessionHT::allocate()
   {
   return new (PERSISTENT_NEW) ClientSessionHT();
   }

ClientSessionHT::ClientSessionHT() : _clientSessionMap(decltype(_clientSessionMap)::allocator_type(TR::Compiler->persistentAllocator())),
                                     TIME_BETWEEN_PURGES(1000*60*30), // JITServer TODO: this must come from options
                                     OLD_AGE(1000*60*1000) // 1000 minutes
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
//               _expectedSeqNo is populated if a new ClientSessionData is created
//                timeOflastAccess is updated with curent time.
ClientSessionData *
ClientSessionHT::findOrCreateClientSession(uint64_t clientUID, uint32_t seqNo, bool *newSessionWasCreated)
   {
   *newSessionWasCreated = false;
   ClientSessionData *clientData = findClientSession(clientUID);
   if (!clientData)
      {
      // alocate a new ClientSessionData object and create a clientUID mapping
      clientData = new (PERSISTENT_NEW) ClientSessionData(clientUID, seqNo);
      if (clientData)
         {
         _clientSessionMap[clientUID] = clientData;
         *newSessionWasCreated = true;
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Server allocated data for a new clientUID %llu", (unsigned long long)clientUID);
         }
      else
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "ERROR: Server could not allocate client session data");
         // should we throw bad_alloc here?
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
   if (crtTime - _timeOfLastPurge > TIME_BETWEEN_PURGES)
      {
      // Time for a purge operation.
      // Scan the entire table and delete old elements that are not in use
      for (auto iter = _clientSessionMap.begin(); iter != _clientSessionMap.end(); ++iter)
         {
         TR_ASSERT(iter->second->getInUse() >= 0, "_inUse=%d must be positive\n", iter->second->getInUse());
         if (iter->second->getInUse() == 0 &&
             crtTime - iter->second->getTimeOflastAccess() > OLD_AGE)
            {
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Server will purge session data for clientUID %llu", (unsigned long long)iter->first);
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
