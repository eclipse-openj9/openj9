/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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
#include "runtime/RuntimeAssumptions.hpp" // for TR_AddressSet
#include "net/ServerStream.hpp" // for JITServer::ServerStream
#include "control/MethodToBeCompiled.hpp" // for TR_MethodToBeCompiled
#include "control/CompilationRuntime.hpp" // for CompilationInfo


ClientSessionData::ClientSessionData(uint64_t clientUID, uint32_t seqNo) : 
   _clientUID(clientUID), _expectedSeqNo(seqNo), _maxReceivedSeqNo(seqNo), _OOSequenceEntryList(NULL),
   _chTableClassMap(decltype(_chTableClassMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _romClassMap(decltype(_romClassMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _J9MethodMap(decltype(_J9MethodMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classByNameMap(decltype(_classByNameMap)::allocator_type(TR::Compiler->persistentAllocator())),
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
   }

ClientSessionData::~ClientSessionData()
   {
   clearCaches();
   _romMapMonitor->destroy();
   _classMapMonitor->destroy();
   _classChainDataMapMonitor->destroy();
   _sequencingMonitor->destroy();
   _constantPoolMapMonitor->destroy();
   if (_unloadedClassAddresses)
      {
      _unloadedClassAddresses->destroy();
      jitPersistentFree(_unloadedClassAddresses);
      }
   _staticMapMonitor->destroy();
   if (_vmInfo)
      jitPersistentFree(_vmInfo);
   _thunkSetMonitor->destroy();
   }

void
ClientSessionData::updateTimeOfLastAccess()
   {
   PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);
   _timeOfLastAccess = j9time_current_time_millis();
   }

void
ClientSessionData::processUnloadedClasses(JITServer::ServerStream *stream, const std::vector<TR_OpaqueClassBlock*> &classes)
   {
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Server will process a list of %u unloaded classes for clientUID %llu", (unsigned)classes.size(), (unsigned long long)_clientUID);
   //Vector to hold the list of the unloaded classes and the corresponding data needed for purging the Caches
   std::vector<ClassUnloadedData> unloadedClasses;
   unloadedClasses.reserve(classes.size());
   bool updateUnloadedClasses = true;
   if (_requestUnloadedClasses)
      {
      stream->write(JITServer::MessageType::getUnloadedClassRanges, JITServer::Void());

      auto response = stream->read<std::vector<TR_AddressRange>, int32_t>();
      auto unloadedClassRanges = std::get<0>(response);
      auto maxRanges = std::get<1>(response);

         {
         OMR::CriticalSection getUnloadedClasses(getROMMapMonitor());

         if (!_unloadedClassAddresses)
            _unloadedClassAddresses = new (PERSISTENT_NEW) TR_AddressSet(trPersistentMemory, maxRanges);
         _unloadedClassAddresses->setRanges(unloadedClassRanges);
         _requestUnloadedClasses = false;
         }

      updateUnloadedClasses = false;
      }
      {
      OMR::CriticalSection processUnloadedClasses(getROMMapMonitor());

      for (auto clazz : classes)
         {
         if (updateUnloadedClasses)
            _unloadedClassAddresses->add((uintptrj_t)clazz);

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
         std::string className((const char *)clazzName->data, (int32_t)clazzName->length);
         J9ClassLoader * cl = (J9ClassLoader *)(it->second._classLoader);
         ClassLoaderStringPair key = { cl, className };
         //Class is cached, so retain the data to be used for purging the caches.
         unloadedClasses.push_back({ clazz, key, cp, true });

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
                  for (auto entryIt : *ipDataHT)
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
   purgeCache(&unloadedClasses, getClassByNameMap(), &ClassUnloadedData::_pair);
   }

   // purge Constant pool to class cache
   {
   OMR::CriticalSection constantPoolToClassMap(getConstantPoolMonitor());
   purgeCache(&unloadedClasses, getConstantPoolToClassMap(), &ClassUnloadedData::_cp);
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
ClientSessionData::cacheIProfilerInfo(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR_IPBytecodeHashTableEntry *entry)
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
         bool isCompiled = TR::CompilationInfo::isCompiled((J9Method*)method);
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
   for (auto it : _romClassMap)
      total += it.second._romClass->romSize;

   j9tty_printf(PORTLIB, "\tTotal size of cached ROM classes + methods: %d bytes\n", total);
   }

ClientSessionData::ClassInfo::ClassInfo() :
   _romClass(NULL),
   _remoteRomClass(NULL),
   _methodsOfClass(NULL),
   _baseComponentClass(NULL),
   _numDimensions(0),
   _remoteROMStringsCache(decltype(_remoteROMStringsCache)::allocator_type(TR::Compiler->persistentAllocator())),
   _fieldOrStaticNameCache(decltype(_fieldOrStaticNameCache)::allocator_type(TR::Compiler->persistentAllocator())),
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
   _classOfStaticCache(decltype(_classOfStaticCache)::allocator_type(TR::Compiler->persistentAllocator())),
   _constantClassPoolCache(decltype(_constantClassPoolCache)::allocator_type(TR::Compiler->persistentAllocator())),
   _fieldAttributesCache(decltype(_fieldAttributesCache)::allocator_type(TR::Compiler->persistentAllocator())),
   _staticAttributesCache(decltype(_staticAttributesCache)::allocator_type(TR::Compiler->persistentAllocator())),
   _fieldAttributesCacheAOT(decltype(_fieldAttributesCacheAOT)::allocator_type(TR::Compiler->persistentAllocator())),
   _staticAttributesCacheAOT(decltype(_fieldAttributesCacheAOT)::allocator_type(TR::Compiler->persistentAllocator())),
   _constantPool(NULL),
   _jitFieldsCache(decltype(_jitFieldsCache)::allocator_type(TR::Compiler->persistentAllocator())),
   _classFlags(0),
   _fieldOrStaticDeclaringClassCache(decltype(_fieldOrStaticDeclaringClassCache)::allocator_type(TR::Compiler->persistentAllocator())),
   _J9MethodNameCache(decltype(_J9MethodNameCache)::allocator_type(TR::Compiler->persistentAllocator()))
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
      _vmInfo = new (PERSISTENT_NEW) VMInfo(std::get<0>(stream->read<VMInfo>()));
      }
   return _vmInfo;
   }


void
ClientSessionData::clearCaches()
   {
      {
      OMR::CriticalSection clearCache(getClassMapMonitor());
      _classByNameMap.clear();
      }
   OMR::CriticalSection getRemoteROMClass(getROMMapMonitor());
   // Free memory for all hashtables with IProfiler info
   for (auto it : _J9MethodMap)
      {
      IPTable_t *ipDataHT = it.second._IPData;
      // It it exists, walk the collection of <pc, TR_IPBytecodeHashTableEntry*> mappings
      if (ipDataHT)
         {
         for (auto entryIt : *ipDataHT)
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
   for (auto it : _romClassMap)
      it.second.freeClassInfo();

   _romClassMap.clear();

   _classChainDataMap.clear();

   // Free CHTable 
   for (auto it : _chTableClassMap)
      {
      TR_PersistentClassInfo *classInfo = it.second;
      classInfo->removeSubClasses();
      jitPersistentFree(classInfo);
      }
   _chTableClassMap.clear();
   _requestUnloadedClasses = true;
   _registeredJ2IThunksMap.clear();
   _registeredInvokeExactJ2IThunksSet.clear();
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


template <typename map, typename key>
void ClientSessionData::purgeCache(std::vector<ClassUnloadedData> *unloadedClasses, map m, key ClassUnloadedData::*k)
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
