/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#include "runtime/JITaaSIProfiler.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/JITaaSCompilationThread.hpp"
#include "infra/CriticalSection.hpp" // for OMR::CriticalSection
#include "ilgen/J9ByteCode.hpp"
#include "ilgen/J9ByteCodeIterator.hpp"
#include "env/StackMemoryRegion.hpp"
#include "bcnames.h"

TR_JITaaSIProfiler *
TR_JITaaSIProfiler::allocate(J9JITConfig *jitConfig)
   {
   TR_JITaaSIProfiler * profiler = new (PERSISTENT_NEW) TR_JITaaSIProfiler(jitConfig);
   return profiler;
   }

TR_JITaaSIProfiler::TR_JITaaSIProfiler(J9JITConfig *jitConfig)
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
      if (callerIdx >= TR_IPMethodHashTableEntry::MAX_IPMETHOD_CALLERS)
         break;
      auto &serialCaller = serialEntry._callers[callerIdx];
      serialCaller._method = caller->getMethod();
      serialCaller._pcIndex = caller->getPCIndex();
      serialCaller._weight = caller->getWeight();
      callerIdx++;
      }
   return serialEntry;
   }

TR_IPBytecodeHashTableEntry*
TR_JITaaSIProfiler::ipBytecodeHashTableEntryFactory(TR_IPBCDataStorageHeader *storage, uintptrj_t pc, TR_Memory* mem, TR_AllocationKind allocKind)
   {
   TR_IPBytecodeHashTableEntry *entry =  nullptr;
   uint32_t entryType = storage->ID;
   if (entryType == TR_IPBCD_FOUR_BYTES)
      {
      TR_ASSERT(storage->left == 0 || storage->left == sizeof(TR_IPBCDataFourBytesStorage), "Wrong size for serialized IP entry %u != %u", storage->left, sizeof(TR_IPBCDataFourBytesStorage));
      entry = (TR_IPBytecodeHashTableEntry*)mem->allocateMemory(sizeof(TR_IPBCDataFourBytes), allocKind, TR_Memory::IPBCDataFourBytes);
      entry = new (entry) TR_IPBCDataFourBytes(pc);
      }
   else if (entryType == TR_IPBCD_CALL_GRAPH)
      {
      TR_ASSERT(storage->left == 0 || storage->left == sizeof(TR_IPBCDataCallGraphStorage), "Wrong size for serialized IP entry %u != %u", storage->left, sizeof(TR_IPBCDataCallGraphStorage));
      entry = (TR_IPBytecodeHashTableEntry*)mem->allocateMemory(sizeof(TR_IPBCDataCallGraph), allocKind, TR_Memory::IPBCDataCallGraph);
      entry = new (entry) TR_IPBCDataCallGraph(pc);
      }
   else if (entryType == TR_IPBCD_EIGHT_WORDS)
      {
      TR_ASSERT(storage->left == 0 || storage->left == sizeof(TR_IPBCDataEightWordsStorage), "Wrong size for serialized IP entry %u != %u", storage->left, sizeof(TR_IPBCDataEightWordsStorage));
      entry = (TR_IPBytecodeHashTableEntry*)mem->allocateMemory(sizeof(TR_IPBCDataEightWords), allocKind, TR_Memory::IPBCDataEightWords);
      entry = new (entry) TR_IPBCDataEightWords(pc);
      }
   else
      {
      TR_ASSERT(false, "Unknown entry type %u", entryType);
      }
   return entry;
   }

TR_IPMethodHashTableEntry *
TR_JITaaSIProfiler::searchForMethodSample(TR_OpaqueMethodBlock *omb, int32_t bucket)
   {
   auto stream = TR::CompilationInfo::getStream();
   if (!stream)
      {
      return nullptr;
      }
   stream->write(JITaaS::J9ServerMessageType::IProfiler_searchForMethodSample, omb, bucket);
   const std::string entryStr = std::get<0>(stream->read<std::string>());
   if (entryStr.empty())
      {
      return nullptr;
      }
   const auto serialEntry = (TR_ContiguousIPMethodHashTableEntry*) &entryStr[0];
   return deserializeMethodEntry(serialEntry);
   }

// This method is used to search only the hash table
TR_IPBytecodeHashTableEntry*
TR_JITaaSIProfiler::profilingSample(uintptrj_t pc, uintptrj_t data, bool addIt, bool isRIData, uint32_t freq)
   {
   if (addIt)
      return nullptr; // Server should not create any samples

   TR_ASSERT(false, "not implemented for JITaaS");
   return nullptr;
   }



// This method is used to search the hash table first, then the shared cache
TR_IPBytecodeHashTableEntry*
TR_JITaaSIProfiler::profilingSample(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex,
                                  TR::Compilation *comp, uintptrj_t data, bool addIt)
   {
   if (addIt)
      return nullptr; // Server should not create any samples

   ClientSessionData *clientSessionData = comp->fej9()->_compInfoPT->getClientData();
   TR_IPBytecodeHashTableEntry *entry = nullptr;

   // Check the cache first, if allowed
   //
   if (_useCaching)
      {
      bool methodInfoPresent = false;
      entry = clientSessionData->getCachedIProfilerInfo(method, byteCodeIndex, &methodInfoPresent);
      if (methodInfoPresent)
         {
         _statsIProfilerInfoFromCache++;
#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
         // sanity check
         // Ask the client again and see if the two sources of information match
         auto stream = TR::CompilationInfo::getStream();
         stream->write(JITaaS::J9ServerMessageType::IProfiler_profilingSample, method, byteCodeIndex, (uintptrj_t)1);
         auto recv = stream->read<std::string, bool>();
         const std::string ipdata = std::get<0>(recv);
         bool wholeMethod = std::get<1>(recv); // indicates whether the client has sent info for entire method
         TR_ASSERT(!wholeMethod, "Client should not have sent whole method info");
         uintptrj_t methodStart = TR::Compiler->mtd.bytecodeStart(method);
         TR_IPBCDataStorageHeader *clientData = ipdata.empty() ? nullptr : (TR_IPBCDataStorageHeader *) &ipdata[0];
         bool isMethodBeingCompiled = (method == comp->getMethodBeingCompiled()->getPersistentIdentifier());
         validateCachedIPEntry(entry, clientData, methodStart, isMethodBeingCompiled, method);
#endif
         return entry; // could be NULL
         }
      }
   // Now ask the client
   //
   auto stream = TR::CompilationInfo::getStream();
   stream->write(JITaaS::J9ServerMessageType::IProfiler_profilingSample, method, byteCodeIndex, (uintptrj_t)0);
   auto recv = stream->read<std::string, bool>();
   const std::string ipdata = std::get<0>(recv);
   bool wholeMethod = std::get<1>(recv); // indicates whether the client sent info for entire method
   _statsIProfilerInfoMsgToClient++;

   bool doCache = _useCaching && wholeMethod;
   if (!doCache)
      _statsIProfilerInfoReqNotCacheable++;
  
   if (ipdata.empty()) // client didn't send us anything
      {
      _statsIProfilerInfoIsEmpty++;
      // FIXME: distinguish between no info and invalidated entry
      if (doCache)
         {
         // cache some empty data so that we don't ask again for this method
         if (!clientSessionData->cacheIProfilerInfo(method, byteCodeIndex, nullptr))
            _statsIProfilerInfoCachingFailures++;
         }
      return nullptr;
      }
   
   uintptrj_t methodStart = TR::Compiler->mtd.bytecodeStart(method);

   if (doCache)
      {
      // Walk the data sent by the client and add new entries to our internal hashtable
      const char *bufferPtr = &ipdata[0];
      TR_IPBCDataStorageHeader *storage = nullptr;
      do {
         storage = (TR_IPBCDataStorageHeader *)bufferPtr;
         // allocate a new entry of the specified type
         entry = ipBytecodeHashTableEntryFactory(storage, methodStart+storage->pc, comp->trMemory(), persistentAlloc);
         if (entry)
            {
            // fill the new entry with data sent by client
            entry->deserialize(storage);
            // Add the entry to the cache if allowed
            // Note that it's possible that the method got unloaded since we last talked
            // to the client and the unload event was communicated through another compilation 
            // request which is going to be handled by another thread
            //
            // Interfaces are weird; we may have to add 2 to the bci that is going to be the key
            //
            uint32_t bci = storage->pc;
            if (storage->ID == TR_IPBCD_CALL_GRAPH)
               {
               U_8* pc = (U_8*)entry->getPC();
               if ((*pc == JBinvokeinterface2) && (*(pc + 2) == JBinvokeinterface)) // FIXME: must test the length of the method
                  bci += 2;
               }
            if (!clientSessionData->cacheIProfilerInfo(method, bci, entry))
               {
               // If caching failed we must delete the entry allocated with persistent memory
               _statsIProfilerInfoCachingFailures++;
               jitPersistentFree(entry);
               // should we break here or keep going?
               }
            }
         else
            {
            // What should we do if we cannot allocate from persistent memory?
            break; // no point in trying again
            }
         // Advance to the next entry
         bufferPtr += storage->left;
         } while (storage->left != 0);
      // Now that all the entries are added to the cache, search the cache
      bool methodInfoPresent = false;
      entry = clientSessionData->getCachedIProfilerInfo(method, byteCodeIndex, &methodInfoPresent);
      } 
   else // No caching
      {
      // Did the client sent an entire method? Such a waste
      // This could happen if, through options, we disable the caching at the server
      if (wholeMethod)
         {
         // Find my desired pc/bci and return a heap allocated entry for it
         const char *bufferPtr = &ipdata[0];
         TR_IPBCDataStorageHeader *storage = nullptr;
         do {
            storage = (TR_IPBCDataStorageHeader *)bufferPtr;
            uint32_t bci = storage->pc;
            if (storage->ID == TR_IPBCD_CALL_GRAPH)
               {
               U_8* pc = (U_8*)entry->getPC();
               if ((*pc == JBinvokeinterface2) && (*(pc + 2) == JBinvokeinterface)) // FIXME: must test the length of the method
                  bci += 2;
               }
            if (bci == byteCodeIndex)
               {
               // Found my desired entry
               entry = ipBytecodeHashTableEntryFactory(storage, bci, comp->trMemory(), heapAlloc);
               if (entry)
                  entry->deserialize(storage);
               break;
               }
            // Move to the next entry
            bufferPtr += storage->left;
            } while (storage->left != 0);
         }
      else // single IP entry
         {
         TR_IPBCDataStorageHeader *storage = (TR_IPBCDataStorageHeader*)&ipdata[0];
         entry = ipBytecodeHashTableEntryFactory(storage, methodStart + storage->pc, comp->trMemory(), heapAlloc);
         if (entry)
            entry->deserialize(storage);
         }
      }
  
   return entry;
   }

int32_t
TR_JITaaSIProfiler::getMaxCallCount()
   {
   auto stream = TR::CompilationInfo::getStream();
   stream->write(JITaaS::J9ServerMessageType::IProfiler_getMaxCallCount, JITaaS::Void());
   return std::get<0>(stream->read<int32_t>());
   }

void
TR_JITaaSIProfiler::printStats()
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

TR_JITaaSClientIProfiler *
TR_JITaaSClientIProfiler::allocate(J9JITConfig *jitConfig)
   {
   TR_JITaaSClientIProfiler * profiler = new (PERSISTENT_NEW) TR_JITaaSClientIProfiler(jitConfig);
   return profiler;
   }

TR_JITaaSClientIProfiler::TR_JITaaSClientIProfiler(J9JITConfig *jitConfig)
   : TR_IProfiler(jitConfig)
   {
   }

/**
 *  walkILTreeForIProfilingEntries
 *
 * Given a method, use the bytecodeIterator to walk over its bytecodes and determine
 * the bytecodePCs that have IProfiler information. Create a sorted array with such
 * bytecodePCs
 * 
 * @param pcEntries An array that needs to be populated (in sorted fashion)
 * @param numEntries (output) Returns the number of entries in the array
 * @param bcIterator The bytecodeIterator (must be allocated by the caller)
 * @param method j9method to be scanned
 * @param BCvisit Bit vector used to avoid scanning the same bytecode twice (why?)
 * @param abort (output) set to true if something goes wrong
 *
 * @return Number of bytes needed to store all IProfiler entries of this method
 */
uint32_t
TR_JITaaSClientIProfiler::walkILTreeForIProfilingEntries(uintptrj_t *pcEntries, uint32_t &numEntries, TR_J9ByteCodeIterator *bcIterator,
                                                       TR_OpaqueMethodBlock *method, TR_BitVector *BCvisit, bool &abort)
   {
   abort = false; // optimistic
   uint32_t bytesFootprint = 0;
   uint32_t methodSize = TR::Compiler->mtd.bytecodeSize(method);
   for (TR_J9ByteCode bc = bcIterator->first(); bc != J9BCunknown; bc = bcIterator->next())
      {
      uint32_t bci = bcIterator->bcIndex();
      if (bci < methodSize && !BCvisit->isSet(bci))
         {
         uintptrj_t thisPC = getSearchPCFromMethodAndBCIndex(method, bci);

         TR_IPBytecodeHashTableEntry *entry = profilingSample(thisPC, 0, false);
         BCvisit->set(bci);
         if (entry && !invalidateEntryIfInconsistent(entry))
            {
            // now check if it can be persisted, lock it, and add it to my list.
            uint32_t canPersist = entry->canBeSerialized(getCompInfo()->getPersistentInfo());
            if (canPersist == IPBC_ENTRY_CAN_PERSIST)
               {
               bytesFootprint += entry->getBytesFootprint();
               // doing insertion sort as we go.
               int32_t i;
               for (i = numEntries; i > 0 && pcEntries[i - 1] > thisPC; i--)
                  {
                  pcEntries[i] = pcEntries[i - 1];
                  }
               pcEntries[i] = thisPC;
               numEntries++;
               }
            else // cannot persist
               {
               switch (canPersist) {
                  case IPBC_ENTRY_PERSIST_LOCK:
                     // that means the entry is locked by another thread. going to abort the
                     // storage of iprofiler information for this method
                  {
                  // In some corner cases of invoke interface, we may come across the same entry
                  // twice under 2 different bytecodes. In that case, the other entry has been
                  // locked by this thread and is in the list of entries, so don't abort.
                  int32_t i;
                  bool found = false;
                  int32_t a1 = 0, a2 = numEntries - 1;
                  while (a2 >= a1 && !found)
                     {
                     i = (a1 + a2) / 2;
                     if (pcEntries[i] == thisPC)
                        found = true;
                     else if (pcEntries[i] < thisPC)
                        a1 = i + 1;
                     else
                        a2 = i - 1;
                     }
                  if (!found)
                     {
                     abort = true;
                     return 0;
                     }
                  }
                  break;
                  case IPBC_ENTRY_PERSIST_UNLOADED:
                     _STATS_entriesNotPersisted_Unloaded++;
                     break;
                  default:
                     _STATS_entriesNotPersisted_Other++;
                  }
               }
            }
         }
      else
         {
         TR_ASSERT(bci < methodSize, "bytecode can't be greater then method size");
         }
      }
   return bytesFootprint;
   }

/**
 * Code to be executed on the JITaaS client to serialize IP data of a method
 *
 * @param pcEntries Sorted array with PCs that have IProfiler info
 * @param numEntries Number of entries in the above array; guaranteed > 0
 * @param memChunk Storage area where we serialize entries
 * @param methodStartAddress Start address of the bytecodes for the method
 * @return Total memory space used for serialization
 */
uintptr_t
TR_JITaaSClientIProfiler::serializeIProfilerMethodEntries(uintptrj_t *pcEntries, uint32_t numEntries,
                                                        uintptr_t memChunk, uintptrj_t methodStartAddress)
   {
   uintptr_t crtAddr = memChunk;
   TR_IPBCDataStorageHeader * storage = nullptr;
   for (uint32_t i = 0; i < numEntries; ++i)
      {
      storage = (TR_IPBCDataStorageHeader *)crtAddr;
      TR_IPBytecodeHashTableEntry *entry = profilingSample(pcEntries[i], 0, false);
      entry->serialize(methodStartAddress, storage, getCompInfo()->getPersistentInfo());

      // optimistically set link to next entry
      uint32_t bytes = entry->getBytesFootprint();
      TR_ASSERT(bytes < 1 << 8, "Error storing iprofile information: left child too far away"); // current size of left child
      storage->left = bytes;

      crtAddr += bytes; // advance to the next position
      }
   // Unlink the last entry
   storage->left = 0;

   return crtAddr - memChunk;
   }

// Code executed at the client to serialize IProfiler info for an entire method
void
TR_JITaaSClientIProfiler::serializeAndSendIProfileInfoForMethod(TR_OpaqueMethodBlock*method, TR::Compilation *comp, JITaaS::J9ClientStream *client)
   {
   TR::StackMemoryRegion stackMemoryRegion(*comp->trMemory());
   uint32_t numEntries = 0;
   uint32_t bytesFootprint = 0;
   uintptrj_t methodSize  = (uintptrj_t)TR::Compiler->mtd.bytecodeSize(method);
   uintptrj_t methodStart = (uintptrj_t)TR::Compiler->mtd.bytecodeStart(method);

   uintptrj_t * pcEntries = nullptr;
   try {
      TR_ResolvedJ9Method resolvedj9method = TR_ResolvedJ9Method(method, comp->fej9(), comp->trMemory());
      TR_J9ByteCodeIterator bci(nullptr, &resolvedj9method, static_cast<TR_J9VMBase *> (comp->fej9()), comp);
      // Allocate memory for every possible node in this method
      TR_BitVector *BCvisit = new (comp->trStackMemory()) TR_BitVector(methodSize, comp->trMemory(), stackAlloc);
      pcEntries = (uintptrj_t *)comp->trMemory()->allocateMemory(sizeof(uintptrj_t) * methodSize, stackAlloc);

      // Walk all bytecodes and populate the sorted array of interesting PCs (pcEntries)
      // numEntries will indicate how many entries have been populated
      // These profiling entries have been 'locked' so we must remember to unlock them
      bool abort = false;
      bytesFootprint = walkILTreeForIProfilingEntries(pcEntries, numEntries, &bci, method, BCvisit, abort);
      if (numEntries && !abort)
         {
         // Serialize the entries
         std::string buffer(bytesFootprint, '\0');
         intptrj_t writtenBytes = serializeIProfilerMethodEntries(pcEntries, numEntries, (uintptr_t)&buffer[0], methodStart);
         TR_ASSERT(writtenBytes == bytesFootprint, "BST doesn't match expected footprint");
         // send the information to the server
         client->write(buffer, true);
         }
      else
         {
         client->write(std::string(), true);
         }
      // release any entry that has been locked by us
      for (uint32_t i = 0; i < numEntries; i++)
         {
         TR_IPBCDataCallGraph *cgEntry = profilingSample(pcEntries[i], 0, false)->asIPBCDataCallGraph();
         if (cgEntry)
            cgEntry->releaseEntry();
         }
      }
   catch (const std::exception &e)
      {
      // release any entry that has been locked by us
      for (uint32_t i = 0; i < numEntries; i++)
         {
         TR_IPBCDataCallGraph *cgEntry = profilingSample(pcEntries[i], 0, false)->asIPBCDataCallGraph();
         if (cgEntry)
            cgEntry->releaseEntry();
         }
      throw;
      }
   }

void
TR_JITaaSIProfiler::validateCachedIPEntry(TR_IPBytecodeHashTableEntry *entry, TR_IPBCDataStorageHeader *clientData, uintptrj_t methodStart, bool isMethodBeingCompiled, TR_OpaqueMethodBlock *method)
   {
   if (!clientData)
      {
      if (entry)
         {
         fprintf(stderr, "Error for cached IP data: ipdata is empty but we have a cached entry=%p\n", entry);
         }
      }
   else // client sent us some data
      {
      if (!entry)
         {
         static int cnt = 0;
         fprintf(stderr, "Error for cached IP data: server sent us something but we have no cached entry. isMethodBeingCompiled=%d cnt=%d\n", isMethodBeingCompiled, ++cnt);
         fprintf(stderr, "\tMethod=%p methodStart=%p bci=%u ID=%u\n", method, (void*)methodStart, clientData->pc, clientData->ID);
         // This situation can happen for methods that we are just compiling which
         // can accumulate more samples while they get compiled
         }
      else // we have data from 2 sources
         {
         // Do the bytecodes match?
         uintptrj_t clientPC = clientData->pc + methodStart;
         TR_ASSERT(clientPC == entry->getPC(), "Missmatch for bci: clientPC: (%u + %p)=%p   cachedPC: %p\n", clientData->pc, (void*)methodStart, (void*)clientPC, entry->getPC());
         // Do the type of entries match?
         switch (clientData->ID)
            {
            case TR_IPBCD_FOUR_BYTES:
               {
               TR_IPBCDataFourBytes *concreteEntry = entry->asIPBCDataFourBytes();
               TR_ASSERT(concreteEntry, "Cached IP entry is not of type TR_IPBCDataFourBytes");
               uint32_t sentData = ((TR_IPBCDataFourBytesStorage *)clientData)->data; 
               uint32_t foundData = (uint32_t)concreteEntry->getData();
               if (sentData != foundData)
                  {
                  fprintf(stderr, "Missmatch for branchInfo sentData=%x, foundData=%x\n", sentData, foundData);
                  // If the branch bias is totally different, flag that as a serious error
                  }
               }
               break;
            case TR_IPBCD_EIGHT_WORDS:
               {
               TR_IPBCDataEightWords *concreteEntry = entry->asIPBCDataEightWords();
               TR_ASSERT(concreteEntry, "Cached IP entry is not of type TR_IPBCDataEightWords");
               }
               break;
            case TR_IPBCD_CALL_GRAPH:
               {
               TR_IPBCDataCallGraph *concreteEntry = entry->asIPBCDataCallGraph();
               TR_ASSERT(concreteEntry, "Cached IP entry is not of type TR_IPBCDataCallGraph");
               // Check that the dominant target is the same in both cases
               CallSiteProfileInfo *csInfoServer = concreteEntry->getCGData();
               CallSiteProfileInfo *csInfoClient = &(((TR_IPBCDataCallGraphStorage *)clientData)->_csInfo);
              
               int32_t sumW;
               int32_t maxW;
               uintptrj_t domClazzClient = csInfoClient->getDominantClass(sumW, maxW);
               uintptrj_t domClazzServer = csInfoServer->getDominantClass(sumW, maxW);
               TR_ASSERT(domClazzClient == domClazzServer, "Missmatch dominant class client=%p server=%p", (void*)domClazzClient, (void*)domClazzServer);
               }
               break;
            default:
               TR_ASSERT(false, "Unknown type of IP info %u", clientData->ID);
            }
         }
      }
   }
