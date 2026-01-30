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

#include "bcnames.h"
#include "control/CompilationController.hpp"
#include "control/CompilationRuntime.hpp" // for CompilationInfo
#include "control/JITServerCompilationThread.hpp"
#include "control/JITServerHelpers.hpp"
#include "control/MethodToBeCompiled.hpp" // for TR_MethodToBeCompiled
#include "env/JITServerPersistentCHTable.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/ut_j9jit.h"
#include "env/VerboseLog.hpp"
#include "net/ServerStream.hpp" // for JITServer::ServerStream
#include "runtime/IProfiler.hpp"
#include "runtime/JITClientSession.hpp"
#include "runtime/JITServerProfileCache.hpp"
#include "runtime/JITServerSharedROMClassCache.hpp"
#include "runtime/RuntimeAssumptions.hpp" // for TR_AddressSet

TR_OpaqueClassBlock * const ClientSessionData::mustClearCachesFlag = reinterpret_cast<TR_OpaqueClassBlock *>(~0);

ClientSessionData::ClientSessionData(uint64_t clientUID, uint32_t seqNo, TR_PersistentMemory *persistentMemory, bool usesPerClientMemory) :
   _clientUID(clientUID), _maxReceivedSeqNo(seqNo), _lastProcessedCriticalSeqNo(seqNo),
   _persistentMemory(persistentMemory),
   _usesPerClientMemory(usesPerClientMemory),
   _OOSequenceEntryList(NULL), _chTable(NULL),
   _romClassMap(decltype(_romClassMap)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _J9MethodMap(decltype(_J9MethodMap)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _classBySignatureMap(decltype(_classBySignatureMap)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _methodByNameMap(decltype(_methodByNameMap)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _DLTedMethodSet(decltype(_DLTedMethodSet)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _classChainDataMap(decltype(_classChainDataMap)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _constantPoolToClassMap(decltype(_constantPoolToClassMap)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _unloadedClassAddresses(NULL),
   _requestUnloadedClasses(true),
   _staticFinalDataMap(decltype(_staticFinalDataMap)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _rtResolve(false),
   _registeredJ2IThunksMap(decltype(_registeredJ2IThunksMap)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _registeredInvokeExactJ2IThunksSet(decltype(_registeredInvokeExactJ2IThunksSet)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _permanentLoaders(decltype(_permanentLoaders)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _wellKnownClasses(),
   _isInStartupPhase(false),
   _aotCacheName(), _aotCache(NULL), _aotHeaderRecord(NULL),
   _aotCacheKnownIds(decltype(_aotCacheKnownIds)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _sharedProfileCache(NULL),
   _classRecordMap(decltype(_classRecordMap)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _numSharedProfileCacheMethodLoads(0),
   _numSharedProfileCacheMethodLoadsFailed(0),
   _numSharedProfileCacheMethodStores(0),
   _numSharedProfileCacheMethodStoresFailed(0),
   _numSharedFaninCacheMethodLoads(0),
   _numSharedFaninCacheMethodLoadsFailed(0),
   _numSharedFaninCacheMethodStores(0),
   _numSharedFaninCacheMethodStoresFailed(0)
   {
   updateTimeOfLastAccess();
   _javaLangClassPtr = NULL;
   _inUse = 1;
   _numActiveThreads = 0;
   _romMapMonitor = TR::Monitor::create("JIT-JITServerROMMapMonitor");
   _classMapMonitor = TR::Monitor::create("JIT-JITServerClassMapMonitor");
   _methodMapMonitor = TR::Monitor::create("JIT-JITServerMethodMapMonitor");
   _DLTSetMonitor = TR::Monitor::create("JIT-JITServerDLTSetMonitor");
   _classChainDataMapMonitor = TR::Monitor::create("JIT-JITServerClassChainDataMapMonitor");
   _sequencingMonitor = TR::Monitor::create("JIT-JITServerSequencingMonitor");
   _cacheInitMonitor = TR::Monitor::create("JIT-JITServerCacheInitMonitor");
   _constantPoolMapMonitor = TR::Monitor::create("JIT-JITServerConstantPoolMonitor");
   _vmInfo = NULL;
   _staticMapMonitor = TR::Monitor::create("JIT-JITServerStaticMapMonitor");
   _markedForDeletion = false;
   _thunkSetMonitor = TR::Monitor::create("JIT-JITServerThunkSetMonitor");
   _permanentLoadersMonitor = TR::Monitor::create("JIT-JITServerPermanentLoadersMonitor");

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
   TR::Monitor::destroy(_methodMapMonitor);
   TR::Monitor::destroy(_DLTSetMonitor);
   TR::Monitor::destroy(_classChainDataMapMonitor);
   TR::Monitor::destroy(_sequencingMonitor);
   TR::Monitor::destroy(_cacheInitMonitor);
   TR::Monitor::destroy(_constantPoolMapMonitor);
   TR::Monitor::destroy(_staticMapMonitor);
   TR::Monitor::destroy(_thunkSetMonitor);
   TR::Monitor::destroy(_permanentLoadersMonitor);
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

   std::vector<J9Class*> unloadedClassesSystemClassLoader;
   ClientSessionData::VMInfo *vminfo = compInfoPT->getClientData()->getOrCacheVMInfo(compInfoPT->getMethodBeingCompiled()->_stream);
   J9ClassLoader *systemClassLoader = (J9ClassLoader*)(vminfo->_systemClassLoader);
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
            //                         _class, _pair,                   _cp,_record,_cached
            unloadedClasses.push_back({ clazz, ClassLoaderStringPair(), NULL, NULL, false });
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

         J9ClassLoader *cl = (J9ClassLoader *)(it->second._classLoader);
         ClassLoaderStringPair key(cl, sigStr);
         const AOTCacheClassRecord *record = it->second._aotCacheClassRecord;
         //Class is cached, so retain the data to be used for purging the caches.
         unloadedClasses.push_back({ clazz, key, cp, record, true });
         if (cl == systemClassLoader)
            unloadedClassesSystemClassLoader.push_back((J9Class*)clazz);

         // For _classBySignatureMap entries that were cached by referencing class loader
         // we need to delete them using the correct class loader
         auto &classLoadersMap = it->second._referencingClassLoaders;
         for (auto it = classLoadersMap.begin(); it != classLoadersMap.end(); ++it)
            {
            ClassLoaderStringPair key = { *it, sigStr };
            unloadedClasses.push_back({ clazz, key, cp, record, true });
            }

         J9Method *methods = it->second._methodsOfClass;
            {
            // Delete unloaded j9methods from the set of DLTed methods.
            OMR::CriticalSection cs(getDLTSetMonitor());
            for (size_t i = 0; i < romClass->romMethodCount; i++)
               {
               J9Method *j9method = methods + i;
               _DLTedMethodSet.erase(j9method);
               }
            } // end critical section getDLTSetMonitor()

         // Delete all the cached J9Methods belonging to this unloaded class.
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
               _J9MethodMap.erase(iter);
               }
            }
         it->second.freeClassInfo(_persistentMemory);
         _romClassMap.erase(it);
         }
      if (useSharedProfileCache())
         {
         purgeCache(unloadedClasses, _classRecordMap, &ClassUnloadedData::_record);
         }
      } // end critical section getROMMapMonitor()

   for (J9Class* unloadedClazz: unloadedClassesSystemClassLoader)
      {
      OMR::CriticalSection methodByNameMapPurge(compInfoPT->getClientData()->getMethodMapMonitor());
      for (auto it = _methodByNameMap.begin(); it != _methodByNameMap.end();)
         {
         if (it->first.first == unloadedClazz)
            {
            it = _methodByNameMap.erase(it);
            }
         else
            {
            ++it;
            }
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
   purgeCache(unloadedClasses, getClassBySignatureMap(), &ClassUnloadedData::_pair);
   }

   // purge Constant pool to class cache
   {
   OMR::CriticalSection constantPoolToClassMap(getConstantPoolMonitor());
   purgeCache(unloadedClasses, getConstantPoolToClassMap(), &ClassUnloadedData::_cp);
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
                  "compThreadID=%d found clazz %p in the cache and updated bit J9ClassHasIllegalFinalFieldModifications to 1", compThreadID, clazz);
            }
         }
      }
   }
void
ClientSessionData::dumpAllBytecodeProfilingData()
   {
   // Open the file where the IP data will be dumped to.
   // The file will be suffixed with the client ID for which the data is dumped.
   FILE *logFile = NULL;
   char filename[128];
   int retVal = snprintf(filename, sizeof(filename), "ipdata.server.%llu", (unsigned long long)getClientUID());
   if (retVal > 0 && retVal < sizeof(filename))
      logFile = fopen(filename, "w+");

   if (logFile)
      {
      OMR::CriticalSection getRemoteROMClass(getROMMapMonitor());
      for (const auto &entry : getJ9MethodMap())
         {
         const struct J9MethodInfo &methodInfo = entry.second;
         const auto iProfilerMap = methodInfo._IPData;
         if (iProfilerMap)
            {
            J9ROMClass *romClass = methodInfo.definingROMClass();
            J9ROMMethod *romMethod = methodInfo._romMethod;
            J9UTF8 *className = J9ROMCLASS_CLASSNAME(romClass);
            J9UTF8 *name = J9ROMMETHOD_NAME(romMethod);
            J9UTF8 *signature = J9ROMMETHOD_SIGNATURE(romMethod);
            fprintf(logFile, "Method: %.*s.%.*s%.*s\n", J9UTF8_LENGTH(className), (char *)J9UTF8_DATA(className),
                                                        J9UTF8_LENGTH(name), (char *)J9UTF8_DATA(name),
                                                        J9UTF8_LENGTH(signature), (char *)J9UTF8_DATA(signature));
            // Each element in this map represents a bytecode that is profiled
            for (const auto &bcEntry : *iProfilerMap)
               {
               uint32_t bcOffset = bcEntry.first;
               TR_IPBytecodeHashTableEntry *ipEntry = bcEntry.second;
               U_8 *pc = (U_8*)ipEntry->getPC();
               TR_IPBCDataFourBytes *branchData = ipEntry->asIPBCDataFourBytes();
               if (branchData)
                  {
                  fprintf(logFile, "\tOffset %" OMR_PRIu32 "\tbranch samples=%" OMR_PRIu32 "\t taken=%" OMR_PRIu32 "\n",
                     bcOffset, branchData->getNumSamples(), (uint32_t)(branchData->getData() & 0xffff));
                  continue;
                  }
               TR_IPBCDataEightWords *switchData = ipEntry->asIPBCDataEightWords();
               if (switchData)
                  {
                  fprintf(logFile, "\tOffset %" OMR_PRIu32 "\tswitch samples=%" OMR_PRIu32 "\n",
                     bcOffset, switchData->getNumSamples());
                  continue;
                  }
               TR_IPBCDataDirectCall *ipbcDirectCallData = ipEntry->asIPBCDataDirectCall();
               if (ipbcDirectCallData)
                  {
                  fprintf(logFile, "\tOffset %" OMR_PRIu32 "\tDirectCall samples=%" OMR_PRIu32 " TooBig=%d\n",
                     bcOffset, ipbcDirectCallData->getNumSamples(), ipbcDirectCallData->isWarmCallGraphTooBig());
                  continue;
                  }
               TR_IPBCDataCallGraph *ipbcCGData = ipEntry->asIPBCDataCallGraph();
               if (!ipbcCGData)
                  continue;
               fprintf(logFile, "\tOffset %" OMR_PRIu32 "\t", bcOffset);
               switch (*pc)
                  {
                  case JBinvokeinterface:  fprintf(logFile, "JBinvokeinterface"); break;
                  case JBinvokeinterface2: fprintf(logFile, "JBinvokeinterface2"); break;
                  case JBinvokevirtual:    fprintf(logFile, "JBinvokevirtual"); break;
                  case JBinstanceof:       fprintf(logFile, "JBinstanceof"); break;
                  case JBcheckcast:        fprintf(logFile, "JBcheckcast"); break;
                  default:                 fprintf(logFile, "JBunknown");
                  }
               fprintf(logFile, " samples=%" OMR_PRIu32 " TooBig=%d\n", ipbcCGData->getNumSamples(), ipbcCGData->isWarmCallGraphTooBig());
               CallSiteProfileInfo *cgData = ipbcCGData->getCGData();
               for (int j = 0; j < NUM_CS_SLOTS; j++)
                  {
                  if (cgData->getClazz(j))
                     {
                     J9ROMClass *romClass = JITServerHelpers::getRemoteROMClassIfCached(this, (J9Class*)cgData->getClazz(j));
                     int32_t len = 0;
                     const char *s = NULL;
                     if (romClass)
                        {
                        s = utf8Data(J9ROMCLASS_CLASSNAME(romClass), len);
                        }
                     else
                        {
                        s = "UNKNOWN";
                        len = 7;
                        }
                     fprintf(logFile, "\t\tW:%4u\tM:%#" OMR_PRIxPTR "\t%.*s\n", cgData->_weight[j], cgData->getClazz(j), len, s);
                     }
                  }
               if (cgData->_residueWeight)
                  fprintf(logFile, "\t\tW:%4u\tM:0xffffffff\tOTHERBUCKET\n", cgData->_residueWeight);
               }
            }
         }
      } // end critical section
   if (logFile)
      fclose(logFile);
   }

/**
 * @brief Print to vlog stats about the per-client profile cache.
 *        Printing will happen when client disconnects and the client session is destroyed.
 *        The information will include the number methods profiled, the number of bytecode entries
 *        and the number of samples in the cache.
 */
void
ClientSessionData::printIProfilerCacheStats()
   {
   if (!TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfile))
      return;
   size_t numMethodsProfiled = 0;
   size_t numBytecodeEntries = 0;
   size_t numSamples = 0;
      {
      OMR::CriticalSection getRemoteROMClass(getROMMapMonitor());
      for (const auto &entry : getJ9MethodMap())
         {
         numMethodsProfiled++;
         const auto iProfilerMap = entry.second._IPData;
         if (iProfilerMap)
            {
            // The number of elements in this map represents the number of different bytecodes that are profiled
            numBytecodeEntries += iProfilerMap->size();
            for (const auto &bcEntry : *iProfilerMap)
               numSamples += bcEntry.second->getNumSamples();
            }
         }
      } // end critical section
   TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Client %" OMR_PRIu64 " ProfileCache stats: numMethodsCached=%zu, bytecodesProfiled=%zu samples=%zu",
                                  getClientUID(), numMethodsProfiled, numBytecodeEntries, numSamples);
   }

/**
 * @brief Get the profiling info for the specified j9method and bytecodeIndex
 *
 * @param method The j9method whose profiling info we want
 * @param byteCodeIndex The bytecodeIndex whose profiling info we want
 * @param methodInfoPresent On return, set to 'true' if method has any profiling info whatsoever
 * @return A pointer to the TR_IPBytecodeHashTableEntry containing profiling info.
 *         NULL if specified bytecodeIndex does not have profiling info.
 */
TR_IPBytecodeHashTableEntry*
ClientSessionData::getCachedIProfilerInfo(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, bool *methodInfoPresent)
   {
   *methodInfoPresent = false;
   TR_IPBytecodeHashTableEntry *ipEntry = NULL;
   OMR::CriticalSection getRemoteROMClass(getROMMapMonitor());
   // check whether info about j9method is cached
   auto &j9methodMap = getJ9MethodMap();
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


/**
 * @brief Store IProfiler info for a single bytecode in the per-client persistent cache
 *
 * @param method The j9method for which the IProfiler info is to be stored
 * @param byteCodeIndex The bytecode index for which the IProfiler info is to be stored
 * @param entry The IProfiler info to be stored
 * @param isCompiled Indicates whether the method is compiled at the client.
 *                   This is used to set the _isCompiledWhenProfiling flag.
 * @return 'true' if profiling info has been store; 'false' otherwise
 * @note Acquires/releases ROMMapMonitor
 */
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

   /**
 * @brief Given a vector of bytecode profiling entries, store those entries into the per-client profile cache.
 *        Note that the entries are already allocated with the 'right' allocator.
 *        They only need to be 'attached' to the iProfilerMap of the method.
 * @param method: The j9method for which IProfiler entries are stored
 * @param entries: The vector of IProfiler entries to be stored
 * @param isCompiled: 'true' if 'method' is compiled at client (thus IProfiler info is stable)
 * @return 'true' if the caching attempt is successful
 * @note Acquires/releases the ROMMapMonitor.
 */
bool
ClientSessionData::cacheIProfilerInfo(TR_OpaqueMethodBlock *method, const Vector<TR_IPBytecodeHashTableEntry *> &entries, bool isCompiled)
   {
   OMR::CriticalSection getRemoteROMClass(getROMMapMonitor());
   // check whether info about j9method exists
   auto &j9methodMap = getJ9MethodMap();
   auto it = j9methodMap.find((J9Method*)method);
   if (it != j9methodMap.end())
      {
      IPTable_t *iProfilerMap = it->second._IPData;
      // This function is called from loadBytecodeDataFromSharedProfileCache()
      // The iProfiler map should not exist because we would have used the data from it
      // and not tried to load profile data from the shared repository.
      // However, another thread could have beat us to it.
      if (!iProfilerMap)
         {
         // Allocate a new iProfiler map
         iProfilerMap = new (_persistentMemory->_persistentAllocator.get())
            IPTable_t(IPTable_t::allocator_type(_persistentMemory->_persistentAllocator.get()));

         it->second._IPData = iProfilerMap; // possibly NULL if allocation fails.
         it->second._isCompiledWhenProfiling = isCompiled;
         if (iProfilerMap)
            {
            uintptr_t methodStart = (uintptr_t)TR::Compiler->mtd.bytecodeStart(method);
            for (auto &entry : entries)
               {
               TR_ASSERT_FATAL(entry->getPC() >= methodStart,
                              "PC cannot be smaller than methodStart. PC=%" OMR_PRIuPTR " methodStart=" OMR_PRIuPTR "\n",
                              entry->getPC(), methodStart);
               iProfilerMap->insert({ entry->getPC() - methodStart, entry });
               // Note: It's possible that we run out of persistent memory and cache only part of the entries for this method.
               // This is unlikely and only cause a small performance regression if anything (this compilation will be aborted).
               }
            return true;
            }
         }
      }
   return false;
   }

void
ClientSessionData::checkProfileDataMatching(J9Method *method, const std::string &ipdata)
   {
   // Walk the data sent by the client and deserialize each entry
   const char *bufferPtr = &ipdata[0];
   uint32_t methodSize = 0; // Will be computed later
   IPTable_t *iProfilerMap = NULL;

   OMR::CriticalSection cs(getROMMapMonitor());
   auto it = getJ9MethodMap().find(method);
   TR_ASSERT_FATAL(it != getJ9MethodMap().end(), "Method %p must be cached", method);
   J9MethodInfo &methodInfo = it->second;
   iProfilerMap = methodInfo._IPData;

   TR_ASSERT_FATAL(iProfilerMap, "There must be some IP data cached because we just loaded from shared profile repository");
   TR_IPBCDataStorageHeader *header = NULL;
   static size_t numReceivedEntries = 0;
   static size_t numBadEntries = 0;
   do {
      header = (TR_IPBCDataStorageHeader *)bufferPtr;
      uint32_t entryType = header->ID;
      uint32_t bci = header->pc;
      numReceivedEntries++;

      switch (entryType)
         {
         case TR_IPBCD_FOUR_BYTES:
            {
            // Search my hashtable
            auto it = iProfilerMap->find(bci);
            if (it == iProfilerMap->end())
               {
               numBadEntries++;
               fprintf(stderr, "WARNING: Shared profile has no info for bci=%u method %p %zu/%zu bad entries\n", (unsigned)bci, method, numBadEntries, numReceivedEntries);
               // Print all entries
               fprintf(stderr, "The following bci are cached: ");
               for (auto &cachedEntry : *iProfilerMap)
                  fprintf(stderr, "%u ", cachedEntry.first);
               fprintf(stderr, "\n");
               }
            else
               {
               TR_IPBCDataFourBytesStorage *serializedEntry = (TR_IPBCDataFourBytesStorage*)bufferPtr;
               uint32_t receivedData = serializedEntry->data;
               uint32_t fallThroughCount0 = receivedData & 0x0000FFFF;
               uint32_t branchToCount0 = (receivedData & 0xFFFF0000) >> 16;
               bool fallThroughDirection0 = fallThroughCount0 > branchToCount0;

               TR_IPBCDataFourBytes *cachedEntry = (TR_IPBCDataFourBytes*)it->second;
               uint32_t cachedData = (uint32_t)cachedEntry->getData();
               uint32_t fallThroughCount1 = cachedData & 0x0000FFFF;
               uint32_t branchToCount1 = (cachedData & 0xFFFF0000) >> 16;
               bool fallThroughDirection1 = fallThroughCount1 > branchToCount1;
               if (fallThroughDirection0 != fallThroughDirection1)
                  {
                  numBadEntries++;
                  fprintf(stderr, "WARNING: Branch direction mismatch for bci=%u method %p %zu/%zu bad entries\n", (unsigned)bci, method, numBadEntries, numReceivedEntries);
                  }
               }
            }
            break;
         case TR_IPBCD_EIGHT_WORDS:
            break;
         case TR_IPBCD_CALL_GRAPH:
            // TODO: also take care of interface2 complexity
            break;
         default:
            TR_ASSERT_FATAL(false, "Unknown profile entry type %u", (unsigned)entryType);
         }
      // Advance to the next entry
      bufferPtr += header->left;
      } while (header->left != 0);
   }


/**
 * @brief Obtain a pointer to the method profiling info in the shared profile cache
 *
 * @param method The j9method of interest
 * @return A pointer to the shared profile entry for the requested j9method
 * @note Acquires/releases ROMMapMonitor and sharedProfileCache monitor
 */
ProfiledMethodEntry *
ClientSessionData::getSharedProfileCacheForMethod(J9Method* method)
   {
   // Convert from J9method to AOTCacheMethodRecord.
   const AOTCacheMethodRecord *methodRecord = NULL;
      {
      OMR::CriticalSection cs(getROMMapMonitor());
      auto it = getJ9MethodMap().find(method);
      TR_ASSERT_FATAL(it != getJ9MethodMap().end(), "Method %p must be already cached", method);
      methodRecord = getMethodRecord(it->second, method);
      }
   ProfiledMethodEntry *methodProfile = NULL; // Populate later with info from shared profile cache (if any)
   if (methodRecord)
      {
      OMR::CriticalSection cs(_sharedProfileCache->monitor());
      methodProfile = _sharedProfileCache->getProfileForMethod(methodRecord);
      }
   return methodProfile;
   }

   /**
 * @brief Retrieve a "summary" of the profile info stored in the shared repository for the given method
 *
 * @param method j9method of interest
 * @return BytecodeProfileSummary
 * @note Acquires/releases ROMMapMonitor and shareProfileCache monitor
 */
BytecodeProfileSummary
ClientSessionData::getSharedBytecodeProfileSummary(J9Method* method)
   {
   // Convert from J9method to AOTCacheMethodRecord.
   const AOTCacheMethodRecord *methodRecord = NULL;
      {
      OMR::CriticalSection cs(getROMMapMonitor());
      auto it = getJ9MethodMap().find(method);
      TR_ASSERT_FATAL(it != getJ9MethodMap().end(), "Method %p must be already cached", method);
      methodRecord = getMethodRecord(it->second, method);
      }
   if (methodRecord)
      {
      OMR::CriticalSection cs(_sharedProfileCache->monitor());
      ProfiledMethodEntry *methodProfile = _sharedProfileCache->getProfileForMethod(methodRecord);
      if (methodProfile)
         return methodProfile->getBytecodeProfileSummary();
      }
   return BytecodeProfileSummary(); // empty struct
   }

   /**
 * @brief Copy out all the bytecode profiling data from the sharedProfileCache into a map attached to clientSession.
 *        If the client data is stable, use persistent memory, otherwise use heap memory.
 *        The persistent memory and heap memory need to be specific to a client.
 *
 * @param method The j9method whose profiling data is copied out
 * @param stable True if the profiling info cannot grow at the client (method has been compiled already)
 * @param comp Compilation object
 * @param ipdata Profiling data sent by client in serialized form (std::string). Used for validation.
 * @return Returns true if the operation succeeded, false otherwise
 * @note This function may send messages to the client to get missing class information for the call graph entries
 * @note Acquires/releases the following locks: ROMMapMonitor, sharedProfileCache monitor, _aotCacheKnownIdsMonitor
 */
bool
ClientSessionData::loadBytecodeDataFromSharedProfileCache(J9Method *method, bool stable, TR::Compilation *comp, const std::string &ipdata)
   {
   //TODO: add trace points
   if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfileDetails))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "comp %p Loading profile for j9method %p", comp, method);
   bool success = true; // optimistic
   TR_Memory &trMemory = *comp->trMemory();
   TR::StackMemoryRegion region(trMemory);
   // Convert from J9Method to AOTCacheMethodRecord.
   const AOTCacheMethodRecord *methodRecord = NULL;
   J9MethodInfo *methodInfo = NULL;
      {
      OMR::CriticalSection cs(getROMMapMonitor());
      auto it = getJ9MethodMap().find(method); // This is the method I am interested in
      if (it != getJ9MethodMap().end())
         {
         methodInfo = &it->second;
         methodRecord = getMethodRecord(it->second, method);
         if (!methodRecord) // Maybe we cannot create it because we reached the AOT cache memory limit
            return false;
         }
      else
         {
         TR_ASSERT_FATAL(false, "Method %p must be already cached", method);
         }
      } // end critical section


   if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfileDetails))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "\tWill clone entries. methodRecord=%p", methodRecord);

   // Get the bytecode profiling info for this j9method.
   Vector<TR_IPBytecodeHashTableEntry *> newEntries(region); // This vector includes cgEntries
   Vector<TR_IPBCDataCallGraph *> cgEntries(region);

      {
      OMR::CriticalSection cs(_sharedProfileCache->monitor());
      ProfiledMethodEntry *methodEntry = _sharedProfileCache->getProfileForMethod(methodRecord);
      if (methodEntry)
         {
         // Get profiling entries from shared profile repo and populate 'newEntries' and 'cgEntries'
         methodEntry->cloneBytecodeData(trMemory, stable, newEntries, cgEntries);
         }
      else
         {
         return false;
         }
      } // end critical section

   // Note: 'newEntries' was populated with pointers to newly allocated entries (using persistent memory or heap memory).
   // Thus, if something prevents us from storing this data in the per-client profile cache, we must free it.
   try
      {

      if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfileDetails))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "\tHave cloned entries. Fix ramClasses for numCgEntries=%zu", cgEntries.size());

      // Go over the cgEntries and for each valid class slot
      // convert class records to RAMClass pointers valid at the client.
      // NOTE: PC is still valid since it points into shared ROMMethod
      // I need to hold ROMMapMonitor because I am accessing some maps protected by it
      // For an entry in uniqueUncachedRecords we have a corresponding entry in uniqueUncachedIndexes.
      // That entry is a vector of tuples representing the cgEntries where patching needs to happen.
      Vector<const AOTCacheClassRecord *> uniqueUncachedClassRecords(region);
      Vector<Vector<std::pair<uint32_t, uint32_t>>> uncachedIndexes(region);

         {
         OMR::CriticalSection cs(getROMMapMonitor());
         for (uint32_t i = 0; i < cgEntries.size(); ++i)
            {
            auto csInfo = cgEntries[i]->getCGData();
            for (uint32_t j = 0; j < NUM_CS_SLOTS; ++j)
               {
               if (auto record = (const AOTCacheClassRecord *)csInfo->getClazz(j))
                  {
                  if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfileDetails))
                     TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "\tLooking at entry %p entryIndex %" OMR_PRIu32  " slot %" OMR_PRIu32 " classRecord %p classLoaderRecord %p",
                                                   cgEntries[i], i, j, record, record->classLoaderRecord());
                  auto c_it = _classRecordMap.find(record); // This _classRecordMap is stored for each client
                  if (c_it != _classRecordMap.end())
                     {
                     csInfo->setClazz(j, (uintptr_t)c_it->second); // c_it->second is a j9class
                     if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfileDetails))
                        TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "\t\tFound classRecord in _classRecordMap. j9class=%p", c_it->second);
                     continue;
                     }

                  // We could not find the <AOTCacheClassRecord, j9class> association
                  // Memorize this fact to ask the client later.
                  // TODO: maybe we should limit ourselves to just the dominant entry, or maybe the client should only send us the dominant entry (based on some option)
                  if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfileDetails))
                     TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "\tCould not find the j9clazz so we have to ask the client later");

                  // Update the vector of unique uncached class records.
                  // Do a linear search since the size of the vector is typically 1-4 entries (most common case is 1).
                  bool found = false;
                  for (size_t index = 0; index < uniqueUncachedClassRecords.size(); ++index)
                     {
                     if (uniqueUncachedClassRecords[index] == record)
                        {
                        uncachedIndexes[index].push_back({i, j}); // cgEntry i, slot j
                        found = true;
                        break;
                        }
                     }
                  if (!found) // Unseen record so far
                     {
                     uniqueUncachedClassRecords.push_back(record);

                     Vector<std::pair<uint32_t, uint32_t>> locations(region);
                     locations.push_back({i, j});
                     uncachedIndexes.emplace_back(locations);
                     // TODO: make this assert non-fatal after testing
                     TR_ASSERT_FATAL(uniqueUncachedClassRecords.size() == uncachedIndexes.size(), "The size of the uniqueUncachedRecords and uncachedIndexes vectors must be equal");
                     }
                  }
               } // end for (uint32_t j = 0; j < NUM_CS_SLOTS; ++j)
            } // end for (uint32_t i = 0; i < cgEntries.size(); ++i)
         } // end critical section
      if (uniqueUncachedClassRecords.size())
         {
         // At this point we may have a vector of class records that could not be converted to RAMClasses.
         // Request missing RAMClasses from the client.
         // The client can use the deserialization mechanism to transform class IDs into RAMClasses.
         std::vector<uintptr_t> classIds; // This is what we are going to send to the client
         classIds.reserve(uniqueUncachedClassRecords.size());

         // Build the vector of serialization records that need to be sent to the client for proper deserialization.
         // Use _aotCacheKnownIds to filter out the IDs that the client already knows about.
         Vector<const AOTSerializationRecord *> recordsToSend(region);
         recordsToSend.reserve(uniqueUncachedClassRecords.size() * 2); // *2 because we may send both the class loader records and the class records
         size_t recordsSize = 0;
            {
            OMR::CriticalSection cs(_aotCacheKnownIdsMonitor);  // Because we access _aotCacheKnownIds map

            for (size_t i = 0; i < uniqueUncachedClassRecords.size(); ++i)
               {
               auto classRecord = uniqueUncachedClassRecords[i];
               auto loaderRecord = classRecord->classLoaderRecord();
               TR_ASSERT_FATAL(loaderRecord, "classRecord=%p has NULL class loader record", classRecord);
               classIds.push_back(classRecord->data().id()); // Will send the class IDs to the client

               // Serialize class loader record before class record (dependency order)
               if (_aotCacheKnownIds.find(loaderRecord->data().idAndType()) == _aotCacheKnownIds.end())
                  {
                  // The client does not know yet about this record. Send the corresponding serialization record.
                  recordsToSend.push_back(&loaderRecord->data());
                  recordsSize += loaderRecord->data().AOTSerializationRecord::size();
                  }
               if (_aotCacheKnownIds.find(classRecord->data().idAndType()) == _aotCacheKnownIds.end())
                  {
                  // The client does not know yet about this record. Send the corresponding serialization record.
                  recordsToSend.push_back(&classRecord->data());
                  recordsSize += classRecord->data().AOTSerializationRecord::size();
                  }
               } // end for
            } // end critical section

         // Pack the serialization records that we gathered in 'recordsToSend' into a format that can be sent over the network.
         std::string recordsStr(recordsSize, 0);
         JITServerAOTCache::packSerializationRecords(recordsToSend, (uint8_t *)recordsStr.data(), recordsStr.size());

         if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfileDetails))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "\tcomp=%p Will send message to client with %zu serialized records asking for %zu classes",
                                          comp, recordsToSend.size(), classIds.size());

         // Now, send a message to the client with the classIDs and serialization records and read the reply.
         auto stream = comp->getStream();
         stream->write(JITServer::MessageType::AOTCache_getRAMClassFromClassRecordBatch, classIds, recordsStr);
         auto recv = stream->read<std::vector<J9Class *>>();
         // Note that we don't get full information about classes at this point,
         // only j9class pointers valid at the client because we don't need more than that.
         auto &ramClasses = std::get<0>(recv);

         if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfileDetails))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "\tcomp=%p Have received response from client for %zu ramClasses", comp, ramClasses.size());

         if (ramClasses.empty()) // Possible if the deserializer was reset
            {
            if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfile))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                  "ERROR: Failed to get uncached classes for profiled method %p ID %zu "
                  "while compiling %s for clientUID %llu", method, methodRecord->data().id(),
                  comp->signature(), (unsigned long long)_clientUID);
            _numSharedProfileCacheMethodLoadsFailed++;
            if (stable)
               {
               for (auto& profilingEntry : newEntries)
                  {
                  trMemory.freeMemory(profilingEntry, persistentAlloc);
                  }
               newEntries.clear();
               }
            return false;
            }

         TR_ASSERT_FATAL(ramClasses.size() == uniqueUncachedClassRecords.size(), "Invalid vector length %zu != %zu", ramClasses.size(), uniqueUncachedClassRecords.size());
         // Update the set of IDs that the client knows about, but consider only the classes that were successfully identified.
            {
            OMR::CriticalSection cs(_aotCacheKnownIdsMonitor);

            for (size_t i = 0; i < uniqueUncachedClassRecords.size(); ++i)
               {
               if (ramClasses[i])
                  {
                  _aotCacheKnownIds.insert(uniqueUncachedClassRecords[i]->data().idAndType());
                  _aotCacheKnownIds.insert(uniqueUncachedClassRecords[i]->classLoaderRecord()->data().idAndType());
                  }
               }
            } // end critical section

         // Update the _classRecordMap with classRecord-->j9class mappings,
         // and replace the classRecords with j9classes in the cgEntries.
         // Note that if the client couldn't find the right j9class once, it's likely that it will not
         // be able to find the right class in the future too, so we cache classRecord-->NULL mappings in this case.
         OMR::CriticalSection cs(getROMMapMonitor()); // protect concurrent access to _classRecordMap

         if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfileDetails))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "\tWill start patching");

         for (size_t i = 0; i < uniqueUncachedClassRecords.size(); ++i)
            {
            auto classRecord = uniqueUncachedClassRecords[i];
            //if (ramClasses[i]) // TODO: debatable. To be tuned.
            _classRecordMap.insert({ classRecord, (TR_OpaqueClassBlock *)(ramClasses[i]) });

            // Patch up the cg entries with j9classes that we got from the client
            const auto &placesToPatch = uncachedIndexes[i];
            for (const auto& placeToPatch : placesToPatch)
               {
               uint32_t cgEntryIndex = placeToPatch.first;
               uint32_t slotIndex    = placeToPatch.second;
               uintptr_t ramClassToPatch = (uintptr_t)ramClasses[i];

               if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfileDetails))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "\t\tWill patch entry %p index %" OMR_PRIu32 " slot %" OMR_PRIu32 " with %p",
                                                                     cgEntries[cgEntryIndex], cgEntryIndex, slotIndex, (J9Class*)ramClassToPatch);

               CallSiteProfileInfo *csInfo = cgEntries[cgEntryIndex]->getCGData();
               csInfo->setClazz(slotIndex, ramClassToPatch);

               if (!ramClasses[i])
                  {
                  bool isDominantSlot = csInfo->getDominantSlot() == slotIndex;
                  if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfile))
                     {
                     TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                        "Shared Profile ERROR: Failed to get RAMClass for class %.*s ID %zu for profiled method %p ID %" OMR_PRIuPTR
                        " CG slot index %" OMR_PRIu32 " weight %u relWeight %.0f%% dominant=%d residue=%u while compiling %s",
                        RECORD_NAME(&classRecord->data()), classRecord->data().id(), method, methodRecord->data().id(),
                        slotIndex, (unsigned)csInfo->_weight[slotIndex], csInfo->_weight[slotIndex]*100.0/cgEntries[cgEntryIndex]->getSumCount(),
                        isDominantSlot, (unsigned)csInfo->_residueWeight, comp->signature());
                     }
                  if (isDominantSlot)
                     {
                     // TODO: We have a slot with RAMClass==0 and this is the dominant class. We may
                     // need to do some changes: move its samples to the other category, though I don't know if we support
                     // entries with just one class and the rest being into the other category. There might be some
                     // implicit assumptions that the 3 slots must be filled before having samples in the "other" category.
                     // Must check all the IProfiler code.
                     // If this dominant slot uses 100% of the samples, then we should use profiling info from the client.
                     // Note: Most of the methods that fall into this category have "lambda" or "proxy" in their name.
                     }
                  }
               }
            }
         }
      // Validate cgEntries against the profiling entries sent by the client.
      // We want to make sure that the j9class pointers that we materialized are valid.
      if (!ipdata.empty() && !cgEntries.empty() && TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfileDetails))
         {
         Vector<TR_IPBytecodeHashTableEntry *> clientCgEntries(region);
         JITServerIProfiler::deserializeIProfilerData(method, ipdata, clientCgEntries, comp->trMemory(), /*cgEntriesOnly=*/true);

         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "\tWill validate %zu cgEntries aganist %zu client entries", cgEntries.size(), clientCgEntries.size());

         for (const auto &cgEntry : cgEntries)
            {
            // Search for a matching PC entry in the client's profile.
            for (const auto &clientCgEntry : clientCgEntries)
               {
               if  (cgEntry->getPC() == clientCgEntry->getPC())
                  {
                  // Compare the slots holding j9methods.
                  auto csInfoServer = cgEntry->getCGData();
                  auto csInfoClient = ((TR_IPBCDataCallGraph*)clientCgEntry)->getCGData();
                  for (int slot = 0; slot < NUM_CS_SLOTS; ++slot)
                     {
                     auto serverClazz = csInfoServer->getClazz(slot);
                     auto clientClazz = csInfoClient->getClazz(slot);
                     if (serverClazz && clientClazz && serverClazz != clientClazz)
                        {
                        // It may be ok to have discrepancies for highly polymorphic call sites.
                        // Let's test that by looking at the "other" category as well.
                        bool isDominantSlot = csInfoClient->getDominantSlot() == slot;
                        TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Discrepancy for jmethod %p slot %d serverClazz=%p clientClazz=%p samples=%u residue=%u",
                           method, slot, serverClazz, clientClazz, (unsigned)csInfoClient->_weight[slot], (unsigned)csInfoClient->_residueWeight);
                        }
                     }
                  break;
                  }
               }
            }
         } // end validation
      }
   catch (...)
      {
      // Free the entries allocated with persistent memory and rethrow
      if (stable)
         {
         for (auto& profilingEntry : newEntries)
            {
            trMemory.freeMemory(profilingEntry, persistentAlloc);
            }
         newEntries.clear();
         }
      throw;
      }
   // Put the assembled profile entries into the private profile repository (per-client or per compilation)
   // depending on how "stable" the information at the client is. Note that the shared profile info could
   // have a different "stable" value, so the same client may load the same shared profile several times.
   if (stable)
      {
      success = cacheIProfilerInfo((TR_OpaqueMethodBlock*)method, newEntries, stable);
      if (!success)
         {
         // Delete 'newEntries' because another thread beat us to caching profiling info per client session
         if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfileDetails))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "\tWill delete the newEntries allocated with persistent memory because another thread was faster than us");
         for (auto& profilingEntry : newEntries)
            {
            trMemory.freeMemory(profilingEntry, persistentAlloc);
            }
         newEntries.clear();
         }
      }
   else
      {
      auto compInfoPT = (TR::CompilationInfoPerThreadRemote *)(comp->fej9()->_compInfoPT);
      success = compInfoPT->cacheIProfilerInfo((TR_OpaqueMethodBlock*)method, newEntries);
      // TODO: transform this into a non-fatal assert after testing
      TR_ASSERT_FATAL(success, "How we can fail this storage of profiling info?");
      }
   if (success)
      {
      _numSharedProfileCacheMethodLoads++;
      if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfile))
         {
         TR_VerboseLog::CriticalSection vlogLock;
         TR_VerboseLog::write(TR_Vlog_JITServer, "Loaded %zu bytecode entries from shared profile repository for method %p ",
                                                newEntries.size(), method);
         TR::CompilationInfo::printMethodNameToVlog(methodInfo->definingROMClass(), methodInfo->_romMethod);
         TR_VerboseLog::writeLine(" ROMMethod %p ROMClass=%p", methodInfo->_romMethod, methodInfo->definingROMClass());
         }
      }
   else
      {
      _numSharedProfileCacheMethodLoadsFailed++;
      }
   return success;
   }

/**
  * @brief Take the serialized IP data sent by the client for the indicated method and store it in the shared profile cache.
  *
  * @param method j9method for which profiling information is stored
  * @param ipdata The IProfiler data in serialized format received from the client
  * @param numSamples The total number of samples for the profile tom be stored
  * @param isStable 'true' if the profiling data quality is not expected to grow in the future
  * @param comp The compilation object
  * @return true if the bytecode profile was successfully stored in the shared profile cache, false otherwise
  * @note Acquires/releases ROMMapMonitor and shared profile repository monitor
  * @note This function may send messages to the client
  */
bool
ClientSessionData::storeBytecodeProfileInSharedRepository(TR_OpaqueMethodBlock *method,
                                                          const std::string &ipData,
                                                          uint64_t numSamples,
                                                          bool isStable,
                                                          TR::Compilation *comp)
   {
   // Get the J9MethodInfo where the private IProfiler info resides
   // TODO: can we obtain the methodInfo from the caller?

   // TODO: We can use   methodRecord = getMethodRecord(method, &methodInfo);
   J9MethodInfo *methodInfo = NULL;
   const AOTCacheMethodRecord *methodRecord = NULL;
      {
      OMR::CriticalSection cs(getROMMapMonitor());
      auto it = getJ9MethodMap().find((J9Method *)method);
      TR_ASSERT_FATAL(it != getJ9MethodMap().end(), "Method %p must be already cached", method);
      methodInfo = &it->second;

      // Obtain or create the methodRecord for this j9method. This assumes we have the ROMMapMonitor
      methodRecord = getMethodRecord(*methodInfo, (J9Method *)method);
      if (!methodRecord)
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfile))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
               "ERROR: Failed to get method record for profiled method %p while compiling %s for clientUID %llu",
               method, comp->signature(), (unsigned long long)_clientUID);
         _numSharedProfileCacheMethodStoresFailed++;
         return false; // serious error
         }
      }
   TR::StackMemoryRegion region(*comp->trMemory());
   Vector<TR_IPBytecodeHashTableEntry *> entries(region); // These do not include cgEntries
   Vector<TR_IPBCDataCallGraph *>        cgEntries(region);

   // Go through the entire ipData that we received from the client in serialized format,
   // deserialize the data and create entries with stack memory.
   uintptr_t methodStart = TR::Compiler->mtd.bytecodeStart(method);
   auto bufferPtr = (uint8_t *)ipData.data();
   while (bufferPtr < (uint8_t *)ipData.data() + ipData.size())
      {
      auto storage = (TR_IPBCDataStorageHeader *)bufferPtr;
      auto entry = JITServerIProfiler::ipBytecodeHashTableEntryFactory(storage, methodStart+storage->pc, comp->trMemory(),
                                                                       stackAlloc);
      entry->deserialize(storage);
      if (storage->ID == TR_IPBCD_CALL_GRAPH)
         cgEntries.push_back((TR_IPBCDataCallGraph *)entry);
      else
         entries.push_back(entry);

      if (!storage->left)
         break;
      bufferPtr += storage->left;
      }

   if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfileDetails))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Storing profile to shared repository for method %p. %zu non-cgEntries and %zu cgEntries",
         method, entries.size(), cgEntries.size());

   // If there are CallGraph entries, we must replace the J9class pointers in these entries with AOT cache class records.
   if (!cgEntries.empty())
      {
      struct CGEntryMetadata
         {
         J9Class * _ramClass;
         Vector<std::pair<uint32_t, uint32_t>> _entriesToPatch; // Fields: (1) index of the entry in cgEntries, (2) index of the slot in the CGData structure
         const AOTCacheClassRecord * _classRecord; // Class record corresponding to _ramClass
         };

      // Collect all distinct RAM classes that need to be converted into class records.
      Vector<CGEntryMetadata> uniqueRAMClasses(region);
      for (uint32_t i = 0; i < cgEntries.size(); ++i)
         {
         auto csInfo = cgEntries[i]->getCGData();
         for (uint32_t j = 0; j < NUM_CS_SLOTS; ++j)
            {
            auto ramClass = (J9Class *)csInfo->getClazz((int)j);
            if (!ramClass)
               continue; // jump over empty slots
            // Linear search for duplicates
            bool found  = false;
            for (uint32_t k = 0; k < uniqueRAMClasses.size(); ++k)
               {
               if (ramClass == uniqueRAMClasses[k]._ramClass) // Already present
                  {
                  uniqueRAMClasses[k]._entriesToPatch.push_back({i, j}); // cgEntry 'i', slot 'j'
                  found = true;
                  break;
                  }
               }
            if (!found) // New unique ramClass
               {
               Vector<std::pair<uint32_t, uint32_t>> patchPoint(region);
               patchPoint.push_back({i, j}); // cgEntry 'i', slot 'j'
               uniqueRAMClasses.emplace_back(CGEntryMetadata{ramClass, patchPoint, NULL});
               }
            } // end for (uint32_t j = 0; j < NUM_CS_SLOTS; ++j)
         } // end for (uint32_t i = 0; i < cgEntries.size(); ++i)

      if (!uniqueRAMClasses.empty() && TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfileDetails))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "\tStoring profile: detected %zu unique classes that needs to be converted", uniqueRAMClasses.size());

      // Convert RAM classes from uniqueRAMClasses into class records and put those class records in 'classRecords' vector
      std::vector<J9Class *> uncachedRAMClasses; // These will be sent to the client
      Vector<uint32_t> uncachedClassIndexes(region); // Remembers which classes in uniqueRAMClasses need attention
      {
      OMR::CriticalSection cs(getROMMapMonitor());
      for (uint32_t k = 0; k < uniqueRAMClasses.size(); ++k)
         {
         J9Class *ramClass = uniqueRAMClasses[k]._ramClass;
         bool missingLoaderInfo = false;
         bool uncachedClass = false;
         J9Class *uncachedBaseComponent = NULL;

         auto classRecord = getClassRecord(ramClass, missingLoaderInfo, uncachedClass, uncachedBaseComponent);

         if (classRecord)
            {
            uniqueRAMClasses[k]._classRecord = classRecord;
            }
         else if (uncachedClass)
            {
            uncachedClassIndexes.push_back(k);
            uncachedRAMClasses.push_back(ramClass);
            }
         else if (uncachedBaseComponent)
            {
            uncachedClassIndexes.push_back(k);
            uncachedRAMClasses.push_back(uncachedBaseComponent);
            }
         else
            {
            // We could have reached the memory limit for AOT cache
            if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfile))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                  "ERROR: Failed to get class record for class %p in profiling data for j9method %p "
                  "ID %zu while compiling %s for clientUID %llu", ramClass, method,
                  methodRecord->data().id(), comp->signature(), (unsigned long long)_clientUID
               );
            _numSharedProfileCacheMethodStoresFailed++;
            return false; //debatable if we should continue with this method
            }
         } // end for (uint32_t k = 0; k < uniqueRAMClasses.size(); ++k)
      } // end critical section


      // Request uncached classes from the client as a batch and cache them.
      // Note: this situation happens very rarely.
      if (!uncachedRAMClasses.empty())
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfileDetails))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "\tSending AOTCache_getROMClassBatch message for %zu classes", uncachedRAMClasses.size());

         JITServer::ServerStream *stream = comp->getStream();
         stream->write(JITServer::MessageType::AOTCache_getROMClassBatch, uncachedRAMClasses);
         auto recv = stream->read<std::vector<JITServerHelpers::ClassInfoTuple>, std::vector<J9Class *>>();
         auto &classInfoTuples = std::get<0>(recv);
         auto &uncachedBases = std::get<1>(recv);
         // The classInfoTuples sent by the client also contains the base components that the client
         // discovered (added at the end), but the 'uncached' vector only contains what the server requested.
         // We need to add the supplemental base components that the client discovered.
         uncachedRAMClasses.insert(uncachedRAMClasses.end(), uncachedBases.begin(), uncachedBases.end());

         JITServerHelpers::cacheRemoteROMClassBatch(this, uncachedRAMClasses, classInfoTuples);

         // Get class records for newly cached classes
            {
            OMR::CriticalSection cs(getROMMapMonitor());
            for (auto &idx : uncachedClassIndexes)
               {
               J9Class *ramClass = uniqueRAMClasses[idx]._ramClass;
               bool missingLoaderInfo = false;
               bool uncachedClass = false;
               J9Class *uncachedBaseComponent = NULL;
               auto classRecord = getClassRecord(ramClass, missingLoaderInfo, uncachedClass, uncachedBaseComponent);
               if (classRecord)
                  {
                  uniqueRAMClasses[idx]._classRecord = classRecord;
                  }
               else
                  {
                  TR_ASSERT_FATAL(!uncachedClass && !uncachedBaseComponent, "Class %p must be already cached", ramClass);
                  // We could have reached the memory limit for the AOT cache
                  if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfile))
                     TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                        "ERROR: Failed to get class record for class %p in profiling data for j9method %p "
                        "ID %zu while compiling %s for clientUID %llu", ramClass, method,
                        methodRecord->data().id(), comp->signature(), (unsigned long long)_clientUID);
                  // TODO: make this a recoverable error if the dominant target is still intact
                  // Move the samples from this entry to the "other" category.
                  // Maybe create a fake UNKNOWN class record.
                  _numSharedProfileCacheMethodStoresFailed++;
                  return false; //debatable if we should continue with this method
                  }
               } // end for loop
            } // end critical section
         } // if (!uncachedRAMClasses.empty())

      // Patching cgEntries
      for (uint32_t k = 0; k < uniqueRAMClasses.size(); ++k)
         {
         // Same ramClass can be present in several entries/slots
         for (const auto &entryToPatch : uniqueRAMClasses[k]._entriesToPatch)
            {
            auto idx  = entryToPatch.first;
            auto slot = entryToPatch.second;
            auto csInfo = cgEntries[idx]->getCGData();
            csInfo->setClazz(slot, (uintptr_t)uniqueRAMClasses[k]._classRecord);
            }
         }

      // Add the transformed callGraph entries to the branch/switch entries set.
      entries.insert(entries.end(), cgEntries.begin(), cgEntries.end());
      }

   bool success = true; // optimistic
   // Write the profile data to the shared repository.
   // Note: methodInfo should still be valid because classUnloding cannot happen without us being informed.
   if (_sharedProfileCache->addBytecodeData(entries, methodRecord, methodInfo->_romMethod, methodInfo->definingROMClass(), numSamples, isStable))
      {
      _numSharedProfileCacheMethodStores++;
      if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfile))
         {
         TR_VerboseLog::CriticalSection vlogLock;
         TR_VerboseLog::write(TR_Vlog_JITServer, "Stored %zu bytecode entries (%" OMR_PRIu64 " samples) into shared profiling repo for method %p ",
                                                  entries.size(), numSamples, method);
         TR::CompilationInfo::printMethodNameToVlog(methodInfo->definingROMClass(), methodInfo->_romMethod);
         TR_VerboseLog::writeLine(" ROMMethod %p ROMClass=%p", methodInfo->_romMethod, methodInfo->definingROMClass());
         }
      }
   else
      {
      _numSharedProfileCacheMethodStoresFailed++;
      success = false;
      if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfile))
         {
         TR_VerboseLog::CriticalSection vlogLock;
         TR_VerboseLog::write(TR_Vlog_JITServer, "WARNING: Failed to store profiling info into the shared repo for method %p ",
                                                  method);
         TR::CompilationInfo::printMethodNameToVlog(methodInfo->definingROMClass(), methodInfo->_romMethod);
         TR_VerboseLog::writeLine(" ROMMethod %p ROMClass=%p", methodInfo->_romMethod, methodInfo->definingROMClass());
         }
      }
   return success;
   }

/**
 * @brief Load the fanin data from the shared profile cache, and return it as a
 *        TR_FaninSummaryInfo struct allocated with "heap" memory.
 *
 * @param method RAMMethod for which we want to obtain the fanin info
 * @param trMemory TR_Memory object for alocating "heap" memory
 * @return Pointer to the TR_FaninSummaryInfo struct allocated with heap memory
 * @note Acquires/releases ROMMapMonitor and shared profile repository monitor
 */
TR_FaninSummaryInfo *
ClientSessionData::loadFaninDataFromSharedProfileCache(TR_OpaqueMethodBlock *method, TR_Memory *trMemory)
   {
   const J9MethodInfo *methodInfo = NULL;
   const AOTCacheMethodRecord *methodRecord = getMethodRecord((J9Method *)method, &methodInfo);
   if (!methodRecord)
      {
      _numSharedFaninCacheMethodLoadsFailed++;
      if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfile))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "ERROR: Failed to get method record for profiled method %p while compiling %s for clientUID %llu",
                                                            method, TR::comp()->signature(), (unsigned long long)_clientUID);
      return NULL;
      }
   TR_ASSERT_FATAL(methodInfo, "We cannot obtain a methodRecord without a methodInfo"); // TODO change this to not fatal
   TR_FaninSummaryInfo *faninMethodEntry = _sharedProfileCache->getFaninData(methodRecord, trMemory);
   if (!faninMethodEntry)
      {
      _numSharedFaninCacheMethodLoadsFailed++;
      if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfile))
         {
         TR_VerboseLog::CriticalSection vlogLock;
         TR_VerboseLog::write(TR_Vlog_JITServer, "WARNING: Failed to load fanin info from shared profile repository for method %p ", method);
         TR::CompilationInfo::printMethodNameToVlog(methodInfo->definingROMClass(), methodInfo->_romMethod);
         TR_VerboseLog::writeLine(" ROMMethod %p ROMClass=%p", methodInfo->_romMethod, methodInfo->definingROMClass());
         }
      }
   else
      {
      _numSharedFaninCacheMethodLoads++;
      if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfile))
         {
         TR_VerboseLog::CriticalSection vlogLock;
         TR_VerboseLog::write(TR_Vlog_JITServer, "Loaded fanin info (%" OMR_PRIu64 " samples, %" OMR_PRIu32 "  callers) from shared profile repository for method %p ",
                                                  faninMethodEntry->_totalSamples, faninMethodEntry->_numCallers, method);
         TR::CompilationInfo::printMethodNameToVlog(methodInfo->definingROMClass(), methodInfo->_romMethod);
         TR_VerboseLog::writeLine(" ROMMethod %p ROMClass=%p", methodInfo->_romMethod, methodInfo->definingROMClass());
         }
      }
   return faninMethodEntry;
   }

/**
 * @brief Take the serialized fanin data sent by the client for the indicated method and store it in the shared profile cache.
 *
 * @param method j9method for which profiling information is stored
 * @param serialEntry The fanin data in serialized format received from the client
 * @return true if the data was added to shared profile repository
 * @note Acquires/releases ROMMapMonitor and shared profile repository monitor
 */
bool
ClientSessionData::storeFaninDataInSharedProfileCache(TR_OpaqueMethodBlock *method, const TR_ContiguousIPMethodHashTableEntry *serialEntry)
   {
   const J9MethodInfo *methodInfo = NULL;
   const AOTCacheMethodRecord *methodRecord = getMethodRecord((J9Method *)method, &methodInfo);
   if (!methodRecord)
      {
      _numSharedFaninCacheMethodStoresFailed++;
      if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfile))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "ERROR: Failed to get method record for profiled method %p while compiling %s for clientUID %llu",
                                                            method, TR::comp()->signature(), (unsigned long long)_clientUID);
      return false;
      }
   TR_ASSERT_FATAL(methodInfo, "We cannot obtain a methodRecord without a methodInfo"); // TODO change this to not fatal
   // Note: methodInfo should still be valid because classUnloding cannot happen without us being informed.
   bool success = _sharedProfileCache->addFaninData(serialEntry, methodRecord, methodInfo->_romMethod, methodInfo->definingROMClass());
   if (success)
      {
      _numSharedFaninCacheMethodStores++;
      if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfile))
         {
         TR_VerboseLog::CriticalSection vlogLock;
         TR_VerboseLog::write(TR_Vlog_JITServer, "Stored fanin info (%" OMR_PRIu64 " samples, %" OMR_PRIu32 " callers) to shared profile repository for method %p ",
                                                 serialEntry->_totalSamples, serialEntry->_callerCount, method);
         TR::CompilationInfo::printMethodNameToVlog(methodInfo->definingROMClass(), methodInfo->_romMethod);
         TR_VerboseLog::writeLine(" ROMMethod %p ROMClass=%p", methodInfo->_romMethod, methodInfo->definingROMClass());
         }
      }
   else
      {
      _numSharedFaninCacheMethodStoresFailed++;
      if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfile))
         {
         TR_VerboseLog::CriticalSection vlogLock;
         TR_VerboseLog::write(TR_Vlog_JITServer, "WARNING: Failed to store fanin info to shared profile repository for method %p ", method);
         TR::CompilationInfo::printMethodNameToVlog(methodInfo->definingROMClass(), methodInfo->_romMethod);
         TR_VerboseLog::writeLine(" ROMMethod %p ROMClass=%p", methodInfo->_romMethod, methodInfo->definingROMClass());
         }
      }
   return success;
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
   _classChainOffsetIdentifyingLoader(TR_SharedCache::INVALID_CLASS_CHAIN_OFFSET),
   _classNameIdentifyingLoader(),
   _aotCacheClassRecord(NULL),
   _arrayElementSize(0),
   _nullRestrictedArrayClass(NULL),
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
   _isStableCache(decltype(_isStableCache)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _referencingClassLoaders(decltype(_referencingClassLoaders)::allocator_type(persistentMemory->_persistentAllocator.get())),
   _referenceSlotsInClass(decltype(_referenceSlotsInClass)::allocator_type(persistentMemory->_persistentAllocator.get()))
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

   _DLTedMethodSet.clear();

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
   _classRecordMap.clear();

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

/**
 * @brief For each unloaded class in 'unloadedClasses', purge from cache 'm' all elements matching key 'k'
 *        from the unloadedClasses entry.
 *        If the class to be unloaded is "cached", we search the cache 'm' for the key 'k'.
 *        If not cached, we delete from cache 'm' all elements that a "value" equal to 'k'. This is
 *        used for some reverse maps of the form {someInfo --> j9class}
 *
 * @param unloadedClasses List of unloaded classes (and associated info) to process
 * @param m The cache that needs to be cleaned from unloaded classes
 * @param k Matching criteria for erasing from cache 'm'
 */
template <typename map, typename key>
void ClientSessionData::purgeCache(const std::vector<ClassUnloadedData> &unloadedClasses, map& m, const key ClassUnloadedData::*k)
   {
   for (auto &data : unloadedClasses)
      {
      if (data._cached)
         {
         m.erase((data.*k));
         }
      else
         {
         //If the class is not cached, this is the place to iterate the cache(Map) for deleting the entry by value rather than key.
         for (auto it = m.begin(); it != m.end(); ++it)
            {
            if (it->second == data._class)
               {
               m.erase(it);
               break;
               }
            }
         }
      //DO NOT remove the entry from the unloadedClasses as it will be needed to purge other caches.
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

void
ClientSessionData::getPermanentLoaders(
   TR::vector<J9ClassLoader*, TR::Region&> &dest,
   size_t n,
   JITServer::ServerStream *stream)
   {
   dest.clear();
   dest.reserve(n);

   size_t serverCount = SIZE_MAX;

   // critical section scope
      {
      OMR::CriticalSection lock(_permanentLoadersMonitor);

      // Check if we already have enough permanent loaders from the client for
      // this compilation. We almost always should.
      serverCount = _permanentLoaders.size();
      if (serverCount >= n)
         {
         auto it = _permanentLoaders.begin();
         dest.insert(dest.end(), it, it + n);
         return;
         }
      }

   // Request the missing loaders from the client.
   stream->write(
      JITServer::MessageType::ClientSessionData_getPermanentLoaders,
      serverCount);

   auto recv = stream->read<std::vector<J9ClassLoader*>>();
   auto &newPermanentLoaders = std::get<0>(recv);

   OMR::CriticalSection lock(_permanentLoadersMonitor);

   // Don't just append to _permanentLoaders. Another thread may have requested
   // some of the same loaders.
   for (size_t i = 0; i < newPermanentLoaders.size(); i++)
      {
      if (serverCount + i < _permanentLoaders.size())
         {
         TR_ASSERT_FATAL(
            newPermanentLoaders[i] == _permanentLoaders[serverCount + i],
            "client permanent loader mismatch at index %zu: %p vs. %p",
            serverCount + i,
            newPermanentLoaders[i],
            _permanentLoaders[serverCount + i]);
         }
      else
         {
         TR_ASSERT_FATAL(
            serverCount + i == _permanentLoaders.size(),
            "skipped past the end of _permanentLoaders");

         _permanentLoaders.push_back(newPermanentLoaders[i]);
         }
      }

   TR_ASSERT_FATAL(
      _permanentLoaders.size() >= n,
      "too few permanent loaders: have %zu but need %zu",
      _permanentLoaders.size(),
      n);

   auto it = _permanentLoaders.begin();
   dest.insert(dest.end(), it, it + n);
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
         // The following may create the AOTCache (if it doesn't exist already) and the sharedProfileCache
         auto aotCache = aotCacheMap->get(_aotCacheName, _clientUID, cacheIsBeingLoadedFromDisk);
         if (!aotCache)
            {
            if (cacheIsBeingLoadedFromDisk)
               {
               // In the background, JITServer loads the AOTCache from persistent storage.
               // We may initialize ClientSessionData._useAOTCache the next time we execute this function.
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
            // If this client wants to use the sharedProfilerCache, save a shortcut inside the clientSession.
            if (_vmInfo->_useSharedProfileCache)
               _sharedProfileCache = aotCache->sharedProfileCache();
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
ClientSessionData::getClassRecord(ClassInfo &classInfo, bool &missingLoaderInfo, J9Class *&uncachedBaseComponent,
                                  J9::J9SegmentProvider *scratchSegmentProvider)
{
   TR_ASSERT(getROMMapMonitor()->owned_by_self(), "Must hold ROMMapMonitor");

   if (classInfo._aotCacheClassRecord)
      return classInfo._aotCacheClassRecord;

   const J9ROMClass *baseComponent = NULL;
   if (classInfo._numDimensions)
      {
      auto it = getROMClassMap().find((J9Class *)classInfo._baseComponentClass);
      if (it == getROMClassMap().end())
         {
         uncachedBaseComponent = (J9Class *)classInfo._baseComponentClass;
         return NULL;
         }
      baseComponent = it->second._romClass;
      }

   auto &name = classInfo._classNameIdentifyingLoader;
   if (name.empty())
      {
      TR_ASSERT(TR_SharedCache::INVALID_CLASS_CHAIN_OFFSET != classInfo._classChainOffsetIdentifyingLoader,
                "Valid class chain offset but missing class name identifying loader");
      missingLoaderInfo = true;
      return NULL;
      }

   auto classLoaderRecord = _aotCache->getClassLoaderRecord((const uint8_t *)name.data(), name.size());
   if (!classLoaderRecord)
      return NULL; // Reached AOT cache size limit

   classInfo._aotCacheClassRecord = _aotCache->getClassRecord(classLoaderRecord, classInfo._romClass, baseComponent,
                                                              classInfo._numDimensions, scratchSegmentProvider);
   if (!classInfo._aotCacheClassRecord)
      return NULL; // Reached AOT cache size limit

   // The name string is no longer needed; free the memory used by it by setting it to an empty string
   std::string().swap(classInfo._classNameIdentifyingLoader);

   if (useSharedProfileCache())
      _classRecordMap.insert({ classInfo._aotCacheClassRecord, (TR_OpaqueClassBlock *)classInfo._ramClass });

   return classInfo._aotCacheClassRecord;
}

const AOTCacheClassRecord *
ClientSessionData::getClassRecord(J9Class *clazz, bool &missingLoaderInfo, bool &uncachedClass,
                                  J9Class *&uncachedBaseComponent, J9::J9SegmentProvider *scratchSegmentProvider)
   {
   TR_ASSERT(getROMMapMonitor()->owned_by_self(), "Must hold ROMMapMonitor");

   auto it = getROMClassMap().find(clazz);
   if (it != getROMClassMap().end())
      return getClassRecord(it->second, missingLoaderInfo, uncachedBaseComponent, scratchSegmentProvider);

   uncachedClass = true;
   return NULL;
   }

const AOTCacheClassRecord *
ClientSessionData::getClassRecord(J9Class *clazz, JITServer::ServerStream *stream,
                                  bool &missingLoaderInfo, J9::J9SegmentProvider *scratchSegmentProvider)
   {
   const AOTCacheClassRecord *record = NULL;
   bool uncachedClass = false;
   J9Class *uncachedBaseComponent = NULL;
      {
      OMR::CriticalSection cs(getROMMapMonitor());
      record = getClassRecord(clazz, missingLoaderInfo, uncachedClass, uncachedBaseComponent, scratchSegmentProvider);
      }
   if (record)
      return record;

   if (uncachedClass)
      {
      // Request and cache class info from the client
      JITServerHelpers::ClassInfoTuple classInfoTuple;
      auto romClass = JITServerHelpers::getRemoteROMClass(clazz, stream, _persistentMemory, classInfoTuple);
      JITServerHelpers::cacheRemoteROMClassOrFreeIt(this, clazz, romClass, classInfoTuple);

      OMR::CriticalSection cs(getROMMapMonitor());
      record = getClassRecord(clazz, missingLoaderInfo, uncachedClass, uncachedBaseComponent, scratchSegmentProvider);
      TR_ASSERT(!uncachedClass, "Class %p must be already cached", clazz);
      }

   if (uncachedBaseComponent)
      {
      // Request and cache base component class info from the client
      JITServerHelpers::ClassInfoTuple classInfoTuple;
      auto romClass = JITServerHelpers::getRemoteROMClass(uncachedBaseComponent, stream, _persistentMemory, classInfoTuple);
      JITServerHelpers::cacheRemoteROMClassOrFreeIt(this, uncachedBaseComponent, romClass, classInfoTuple);

      OMR::CriticalSection cs(getROMMapMonitor());
      record = getClassRecord(clazz, missingLoaderInfo, uncachedClass, uncachedBaseComponent, scratchSegmentProvider);
      TR_ASSERT(!uncachedClass && !uncachedBaseComponent, "Class %p and base component must be already cached", clazz);
      }

   if (missingLoaderInfo)
      {
      // Request and cache missing class loader info from the client
      stream->write(JITServer::MessageType::SharedCache_getClassChainOffsetIdentifyingLoader, clazz, true);
      auto recv = stream->read<uintptr_t, std::string>();
      uintptr_t offset = std::get<0>(recv);
      auto &name = std::get<1>(recv);
      if (!name.empty())
         {
         OMR::CriticalSection cs(getROMMapMonitor());
         auto it = getROMClassMap().find((J9Class *)clazz);
         TR_ASSERT(it != getROMClassMap().end(), "Class %p must be already cached", clazz);
         it->second._classChainOffsetIdentifyingLoader = offset;
         it->second._classNameIdentifyingLoader = name;
         record = getClassRecord(it->second, missingLoaderInfo, uncachedBaseComponent, scratchSegmentProvider);
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

   return record;
   }

// This version of getMethodRecord() assumes we already know the J9MethodInfo
// for the j9method of interest and that the ROMMapMonitor is already acquired.
// No messages will be sent because, having the J9MethodInfo means the server
// already cached the information about j9method and its class.
// The AOTcache must be enabled.
const AOTCacheMethodRecord *
ClientSessionData::getMethodRecord(J9MethodInfo &methodInfo, J9Method *ramMethod)
   {
   // Typically, we already have an _aotCacheMethodRecord inside the methodInfo
   TR_ASSERT(getROMMapMonitor()->owned_by_self(), "Must hold ROMMapMonitor");

   // The methodInfo which is stored for any J9Method cached by the server
   // may already contain a pointer to a corresponding aotCacheMethodRecord
   if (!methodInfo._aotCacheMethodRecord)
      {
      // Since we don't have the _aotCacheMethodRecord, we must compute it
      bool missingLoaderInfo = false;
      J9Class *uncachedBaseComponent = NULL;
      // First get or create a classRecord based on _definingClassInfo
      auto classRecord = getClassRecord(methodInfo._definingClassInfo, missingLoaderInfo, uncachedBaseComponent);
      TR_ASSERT_FATAL(!uncachedBaseComponent, "Method %p defined by array class %p", ramMethod, methodInfo.definingClass());
      if (!classRecord)
         return NULL;
      // Then create a method record and cache it in J9MethodInfo
      methodInfo._aotCacheMethodRecord = _aotCache->getMethodRecord(classRecord, methodInfo._index, methodInfo._romMethod);
      }

   return methodInfo._aotCacheMethodRecord;
   }

const AOTCacheMethodRecord *
ClientSessionData::getMethodRecord(J9Method *ramMethod, const J9MethodInfo **methodInfo)
   {
   OMR::CriticalSection cs(getROMMapMonitor());

   auto it = getJ9MethodMap().find(ramMethod);
   TR_ASSERT(it != getJ9MethodMap().end(), "Method %p must be already cached", ramMethod);
   if (methodInfo)
      *methodInfo = &it->second;

   return getMethodRecord(it->second, ramMethod);
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
ClientSessionData::getClassChainRecord(J9Class *clazz, uintptr_t classChainOffset,
                                       const std::vector<J9Class *> &ramClassChain,
                                       JITServer::ServerStream *stream, bool &missingLoaderInfo,
                                       J9::J9SegmentProvider *scratchSegmentProvider)
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
   uncachedRAMClasses.reserve(ramClassChain.size() + 1); // Extra slot for possibly uncached base component class
   J9Class *baseComponent = NULL;
   size_t missingLoaderRecordIndexes[TR_J9SharedCache::maxClassChainLength] = {0};
   size_t numMissingLoaderRecords = 0;

   // Get class records for which all info is already available, remembering classes that we need to request info for
      {
      OMR::CriticalSection cs(getROMMapMonitor());

      for (size_t i = 0; i < ramClassChain.size(); ++i)
         {
         bool missingLoaderRecord = false;
         bool uncachedClass = false;
         J9Class *uncachedBaseComponent = NULL;
         classRecords[i] = getClassRecord(ramClassChain[i], missingLoaderRecord, uncachedClass,
                                          uncachedBaseComponent, scratchSegmentProvider);

         if (!classRecords[i])
            {
            if (uncachedClass)
               {
               uncachedIndexes[uncachedRAMClasses.size()] = i;
               uncachedRAMClasses.push_back(ramClassChain[i]);
               }
            else if (uncachedBaseComponent)
               {
               TR_ASSERT_FATAL(!baseComponent && (i == 0), "Only root class can be an array");
               baseComponent = uncachedBaseComponent;
               // The corner case of missing both the base component class and the class loader info is still handled
               // correctly by requesting the class loader info in the getClassRecord(ramClassChain[0], ...) call below.
               }
            else if (missingLoaderRecord)
               {
               missingLoaderRecordIndexes[numMissingLoaderRecords++] = i;
               }
            else
               {
               return NULL; // Reached AOT cache size limit
               }
            }
         }
      }

   size_t numUncachedClasses = uncachedRAMClasses.size(); // This remembers the size without the base component
   if (baseComponent)
      uncachedRAMClasses.push_back(baseComponent); // The base component is last in the vector

   if (!uncachedRAMClasses.empty())
      {
      // Request uncached classes from the client and cache them
      stream->write(JITServer::MessageType::AOTCache_getROMClassBatch, uncachedRAMClasses);
      auto recv = stream->read<std::vector<JITServerHelpers::ClassInfoTuple>, std::vector<J9Class *>>();
      auto &classInfoTuples = std::get<0>(recv);
      auto &uncachedBases = std::get<1>(recv);
      TR_ASSERT_FATAL(uncachedBases.empty(), "Client should not have sent any bases because the server asked for them explicitely");
      // The client may send us some extra classInfos. Add their corresponding j9classes at the end.
      uncachedRAMClasses.insert(uncachedRAMClasses.end(), uncachedBases.begin(), uncachedBases.end());

      JITServerHelpers::cacheRemoteROMClassBatch(this, uncachedRAMClasses, classInfoTuples);

      // Get root class record if its base component class was previously uncached
      if (baseComponent)
         {
         classRecords[0] = getClassRecord(ramClassChain[0], stream, missingLoaderInfo, scratchSegmentProvider);
         if (!classRecords[0])
            return NULL; // Missing class loader info or reached AOT cache size limit
         baseComponent = NULL;
         }

      // Get class records for newly cached classes, remembering classes with missing class loader info
      OMR::CriticalSection cs(getROMMapMonitor());
      for (size_t i = 0; i < numUncachedClasses; ++i) // This possibly skips the last entry which could be the base component class which was processed separately
         {
         size_t idx = uncachedIndexes[i];
         bool missingLoaderRecord = false;
         bool uncachedClass = false;
         J9Class *uncachedBaseComponent = NULL;
         classRecords[idx] = getClassRecord(uncachedRAMClasses[i], missingLoaderRecord, uncachedClass,
                                            uncachedBaseComponent, scratchSegmentProvider);

         if (!classRecords[idx])
            {
            TR_ASSERT(!uncachedClass, "Class %p must be already cached", uncachedRAMClasses[i]);
            if (uncachedBaseComponent)
               {
               TR_ASSERT_FATAL(!baseComponent && (idx == 0), "Only root class can be an array");
               baseComponent = uncachedBaseComponent; // Can this ever happen?
               }
            else if (missingLoaderRecord)
               {
               missingLoaderRecordIndexes[numMissingLoaderRecords++] = idx;
               }
            else
               {
               return NULL; // Reached AOT cache size limit
               }
            }
         }
      }

   // Get remaining class records, requesting their missing class loader info from the client
   for (size_t i = 0; i < numMissingLoaderRecords; ++i)
      {
      size_t idx = missingLoaderRecordIndexes[i];
      classRecords[idx] = getClassRecord(ramClassChain[idx], stream, missingLoaderInfo, scratchSegmentProvider);
      if (!classRecords[idx])
         return NULL; // Missing class loader info or reached AOT cache size limit
      }

   // Get root class record if its base component class is uncached
   if (baseComponent)
      {
      classRecords[0] = getClassRecord(ramClassChain[0], stream, missingLoaderInfo, scratchSegmentProvider);
      if (!classRecords[0])
         return NULL; // Missing class loader info or reached AOT cache size limit
      }

   // Cache the new class chain record
   auto record = _aotCache->getClassChainRecord(classRecords, ramClassChain.size());
   OMR::CriticalSection cs(getClassChainDataMapMonitor());
   auto result = getClassChainDataMap().insert({ clazz, { classChainOffset, record } });
   if (!result.second)
      result.first->second._aotCacheClassChainRecord = record;
   return record;
   }

const AOTCacheWellKnownClassesRecord *
ClientSessionData::getWellKnownClassesRecord(const AOTCacheClassChainRecord *const *chainRecords,
                                             size_t length, uintptr_t includedClasses)
   {
   return _aotCache->getWellKnownClassesRecord(chainRecords, length, includedClasses);
   }

bool
ClientSessionData::useServerOffsets(JITServer::ServerStream *stream)
   {
   auto *vmInfo = getOrCacheVMInfo(stream);
   return vmInfo->_useServerOffsets;
   }

/**
 * @brief Print to vlog shared profile cache stats from client point of view
 * @note -Xjit:verbose={JITServerSharedProfile} needs to specified
 */
void
ClientSessionData::printSharedProfileCacheStats() const
   {
   if (useSharedProfileCache() && TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfile))
      {
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Client=%" OMR_PRIu64
         " numSharedProfileCacheMethodLoads=%" OMR_PRIu32
         " numSharedProfileCacheMethodLoadsFailed=%" OMR_PRIu32
         " numSharedProfileCacheMethodStores=%" OMR_PRIu32
         " numSharedProfileCacheMethodStoresFailed=%" OMR_PRIu32 "",
         getClientUID(),
         _numSharedProfileCacheMethodLoads,
         _numSharedProfileCacheMethodLoadsFailed,
         _numSharedProfileCacheMethodStores,
         _numSharedProfileCacheMethodStoresFailed);
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Client=%" OMR_PRIu64
         " numSharedFaninCacheMethodLoads=%" OMR_PRIu32
         " numSharedFaninCacheMethodLoadsFailed=%" OMR_PRIu32
         " numSharedFaninCacheMethodStores=%" OMR_PRIu32
         " numSharedFaninCacheMethodStoresFailed=%" OMR_PRIu32 "",
         getClientUID(),
         _numSharedFaninCacheMethodLoads,
         _numSharedFaninCacheMethodLoadsFailed,
         _numSharedFaninCacheMethodStores,
         _numSharedFaninCacheMethodStoresFailed);
      }
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
            {
            // When shared profile cache is used, the SharedROMClassCache may remain active
            // even after the last client disconnects. Thus, we need a test to avoid re-initialing it.
            if (!cache->isInitialized())
               cache->initialize(jitConfig);
            }
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
         if (feGetEnv("TR_DumpIProfilerData"))
            clientData->dumpAllBytecodeProfilingData();
         clientData->printIProfilerCacheStats();
         clientData->printSharedProfileCacheStats();
         ClientSessionData::destroy(clientData); // delete the client data
         _clientSessionMap.erase(clientDataIt); // delete the mapping from the hashtable

         // If this was the last client, shutdown the shared ROMClass cache.
         // Note that this may not happen if the shared profile cache needs some ROMMethods.
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

      bool hadExistingClients = !_clientSessionMap.empty();
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
      // If the purge operation was responsible for deleting the last client sessions, shut down the shared ROM class cache as well
      if (hadExistingClients && _clientSessionMap.empty())
         {
         if (auto cache = TR::CompilationInfo::get()->getJITServerSharedROMClassCache())
            cache->shutdown();
         }

      _timeOfLastPurge = crtTime;

      // JITServer TODO: keep stats on how many elements were purged
      }
   }

// To print these stats,
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
