#include "runtime/JaasIProfiler.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/JaasCompilationThread.hpp"
#include "infra/CriticalSection.hpp" // for OMR::CriticalSection

TR_JaasIProfiler *
TR_JaasIProfiler::allocate(J9JITConfig *jitConfig)
   {
   TR_JaasIProfiler * profiler = new (PERSISTENT_NEW) TR_JaasIProfiler(jitConfig);
   return profiler;
   }

TR_JaasIProfiler::TR_JaasIProfiler(J9JITConfig *jitConfig)
   : TR_IProfiler(jitConfig), _statsIProfilerInfoFromCache(0), _statsIProfilerInfoMsgToClient(0),
   _statsIProfilerInfoReqNotCacheable(0), _statsIProfilerInfoIsEmpty(0), _statsIProfilerInfoCachingFailures(0)
   {
   _useCaching = feGetEnv("TR_DisableIPCaching") ? false: true;
   }

TR_IPMethodHashTableEntry *
deserializeMethodEntry(TR_ContiguousIPMethodHashTableEntry *serialEntry)
   {
   // TODO when caching don't use heap mem
   TR_IPMethodHashTableEntry *entry = (TR_IPMethodHashTableEntry *)TR::comp()->trMemory()->allocateHeapMemory(sizeof(TR_IPMethodHashTableEntry));
   if (entry)
      {
      memset(entry, 0, sizeof(TR_IPMethodHashTableEntry));
      entry->_next = nullptr; // TODO: set this if caching locally
      entry->_method = serialEntry->_method;
      entry->_otherBucket = serialEntry->_otherBucket;

      size_t callerCount = 0;
      for (; callerCount < TR_IPMethodHashTableEntry::MAX_IPMETHOD_CALLERS; callerCount++)
         if (serialEntry->_callers[callerCount]._method == nullptr)
            break;

      // TODO when caching don't use heap mem
      TR_IPMethodData *callerStore = (TR_IPMethodData*)TR::comp()->trMemory()->allocateHeapMemory(callerCount * sizeof(TR_IPMethodData));
      if (callerStore)
         {
         TR_IPMethodData *caller = &entry->_caller;
         for (size_t i = 0; i < callerCount; i++)
            {
            auto &serialCaller = serialEntry->_callers[i];
            TR_ASSERT(serialCaller._method, "callerCount was computed incorrectly");

            if (i != 0)
               {
               TR_IPMethodData* newCaller = &callerStore[i];
               caller->next = newCaller;
               caller = newCaller;
               }

            caller->setMethod(serialCaller._method);
            caller->setPCIndex(serialCaller._pcIndex);
            caller->setWeight(serialCaller._weight);
            caller->next = nullptr;
            }
         }
      }
   return entry;
   }

TR_ContiguousIPMethodHashTableEntry
TR_ContiguousIPMethodHashTableEntry::serialize(TR_IPMethodHashTableEntry *entry)
   {
   TR_ContiguousIPMethodHashTableEntry serialEntry;
   memset(&serialEntry, 0, sizeof(TR_ContiguousIPMethodHashTableEntry));
   serialEntry._method = entry->_method;
   serialEntry._otherBucket = entry->_otherBucket;

   size_t callerIdx = 0;
   for (TR_IPMethodData *caller = &entry->_caller; caller; caller = caller->next)
      {
      auto &serialCaller = serialEntry._callers[callerIdx];
      serialCaller._method = caller->getMethod();
      serialCaller._pcIndex = caller->getPCIndex();
      serialCaller._weight = caller->getWeight();
      callerIdx++;
      }
   return serialEntry;
   }

TR_IPBytecodeHashTableEntry*
TR_JaasIProfiler::ipBytecodeHashTableEntryFactory(uintptrj_t pc, TR_Memory* mem, TR_AllocationKind allocKind)
   {
   TR_IPBytecodeHashTableEntry *entry =  nullptr;
   U_8 byteCode = *(U_8 *)pc;

   if (isCompact(byteCode))
      {
      entry = (TR_IPBytecodeHashTableEntry*)mem->allocateMemory(sizeof(TR_IPBCDataFourBytes), allocKind, TR_Memory::IPBCDataFourBytes);
      entry = new (entry) TR_IPBCDataFourBytes(pc);
      }
   else
      {
      if (isSwitch(byteCode))
         {
         entry = (TR_IPBytecodeHashTableEntry*)mem->allocateMemory(sizeof(TR_IPBCDataEightWords), allocKind, TR_Memory::IPBCDataEightWords);
         entry = new (entry) TR_IPBCDataEightWords(pc);
         }
      else
         {
         entry = (TR_IPBytecodeHashTableEntry*)mem->allocateMemory(sizeof(TR_IPBCDataCallGraph), allocKind, TR_Memory::IPBCDataCallGraph);
         entry = new (entry) TR_IPBCDataCallGraph(pc);
         }
      }
   return entry;
   }

TR_IPMethodHashTableEntry *
TR_JaasIProfiler::searchForMethodSample(TR_OpaqueMethodBlock *omb, int32_t bucket)
   {
   auto stream = TR::CompilationInfo::getStream();
   if (!stream)
      {
      return nullptr;
      }
   stream->write(JAAS::J9ServerMessageType::IProfiler_searchForMethodSample, omb, bucket);
   const std::string &entryStr = std::get<0>(stream->read<std::string>());
   if (entryStr.empty())
      {
      return nullptr;
      }
   const auto serialEntry = (TR_ContiguousIPMethodHashTableEntry*) &entryStr[0];
   return deserializeMethodEntry(serialEntry);
   }

// This method is used to search only the hash table
TR_IPBytecodeHashTableEntry*
TR_JaasIProfiler::profilingSample(uintptrj_t pc, uintptrj_t data, bool addIt, bool isRIData, uint32_t freq)
   {
   if (addIt)
      return nullptr; // Server should not create any samples

   TR_ASSERT(false, "not implemented for JaaS");
   return nullptr;
   }

// This method is used to search the hash table first, then the shared cache
TR_IPBytecodeHashTableEntry*
TR_JaasIProfiler::profilingSample(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex,
                                  TR::Compilation *comp, uintptrj_t data, bool addIt)
   {
   if (addIt)
      return nullptr; // Server should not create any samples

   ClientSessionData *clientSessionData = comp->fej9()->_compInfoPT->getClientData();
   TR_IPBytecodeHashTableEntry *entry = nullptr;
      
   if (_useCaching)
      {
      // Check the cache first
      bool found = false;
      entry = clientSessionData->getCachedIProfilerInfo(method, byteCodeIndex, &found);
      if (found)
         {
         _statsIProfilerInfoFromCache++;
         return entry;
         }
      }

   // Now ask the client
   auto stream = TR::CompilationInfo::getStream();
   stream->write(JAAS::J9ServerMessageType::IProfiler_profilingSample, method, byteCodeIndex, data);
   auto recv = stream->read<std::string, bool>();
   const std::string ipdata = std::get<0>(recv);
   bool isCompiled = std::get<1>(recv);
   _statsIProfilerInfoMsgToClient++;

   bool doCache = false;
   if (_useCaching)
      {
      // Only compiled methods and methods in process of compilation are cacheable
      if (isCompiled || method == comp->methodToBeCompiled()->getPersistentIdentifier())
         doCache = true;
      else
         _statsIProfilerInfoReqNotCacheable++;
      }

   if (ipdata.empty())
      {
      _statsIProfilerInfoIsEmpty++;
      // FIXME: distinguish between no info and invalidated entry
      if (doCache)
         {
         if (!clientSessionData->cacheIProfilerInfo(method, byteCodeIndex, nullptr))
            _statsIProfilerInfoCachingFailures++;
         }
      return nullptr;
      }

   TR_IPBCDataStorageHeader *storage = (TR_IPBCDataStorageHeader*) &ipdata[0];
   uintptrj_t pc = getSearchPC(method, byteCodeIndex, comp);
   U_8 byteCode =  *(U_8 *)pc;
   // If we want to cache the result we use persistent memory. Otherwise use heapmemory
   entry = ipBytecodeHashTableEntryFactory(pc, comp->trMemory(), (doCache ? persistentAlloc : heapAlloc));
   if (entry)
      {
      entry->deserialize(storage);
      // Add the entry to the cache if allowed
      // Note that it's possible that the method got unloaded since we last talked
      // to the client and the unload event was communicated through another compilation 
      // request which is going to be handled by another thread
      if (doCache)
         {
         bool cached = clientSessionData->cacheIProfilerInfo(method, byteCodeIndex, entry);
         // If caching failed we must delete the entry allocated with persistent memory
         // and allocate a new one using heap memory
         if (!cached)
            {
            _statsIProfilerInfoCachingFailures++;
            jitPersistentFree(entry);
            entry = ipBytecodeHashTableEntryFactory(pc, comp->trMemory(), heapAlloc);
            if (entry)
               entry->deserialize(storage);
            }
         }
      }
  
   return entry;
   }

int32_t
TR_JaasIProfiler::getMaxCallCount()
   {
   auto stream = TR::CompilationInfo::getStream();
   stream->write(JAAS::J9ServerMessageType::IProfiler_getMaxCallCount, JAAS::Void());
   return std::get<0>(stream->read<int32_t>());
   }

void
TR_JaasIProfiler::printStats()
   {
   PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);
   j9tty_printf(PORTLIB, "IProfilerInfoMsgToClient: %6u  IProfilerInfoMsgReplyIsEmpty: %6u\n", _statsIProfilerInfoMsgToClient, _statsIProfilerInfoIsEmpty);
   if (_useCaching)
      {
      j9tty_printf(PORTLIB, "IProfilerInfoNotCacheable:   %6u\n", _statsIProfilerInfoReqNotCacheable);
      j9tty_printf(PORTLIB, "IProfilerInfoCachingFailure: %6u\n", _statsIProfilerInfoCachingFailures);
      j9tty_printf(PORTLIB, "IProfilerInfoFromCache:   %6u\n", _statsIProfilerInfoFromCache);
      }
   }