/*******************************************************************************
 * Copyright IBM Corp. and others 2018
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

#include <string.h>

#include "control/JITServerCompilationThread.hpp"

#include "codegen/CodeGenerator.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/MethodToBeCompiled.hpp"
#include "control/JITServerHelpers.hpp"
#include "env/ClassTableCriticalSection.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "env/JITServerAllocationRegion.hpp"
#include "env/JITServerPersistentCHTable.hpp"
#include "env/SystemSegmentProvider.hpp"
#include "env/VerboseLog.hpp"
#include "env/ut_j9jit.h"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheExceptions.hpp"
#include "runtime/J9VMAccess.hpp"
#include "runtime/RelocationTarget.hpp"
#include "net/ClientStream.hpp"
#include "net/ServerStream.hpp"
#include "jitprotos.h"
#include "vmaccess.h"

/**
 * @brief Helper method executed at the end of a compilation
 * to determine whether the server is running out of memory.
 */
JITServer::ServerMemoryState
computeServerMemoryState(TR::CompilationInfo *compInfo)
   {
   // Compute LOW memory threshold relative to the number of clients but cap it at 16
   // to avoid wasting memory in cases when clients disconnect without notifying the server
   size_t numClients = compInfo->getClientSessionHT()->size();
   numClients = numClients > 16 ? 16 : numClients;
   uint64_t lowMemoryThreshold = TR::Options::getSafeReservePhysicalMemoryValue() + (numClients + 4) * TR::Options::getScratchSpaceLowerBound();
   uint64_t veryLowMemoryThreshold = TR::Options::getSafeReservePhysicalMemoryValue() + 4 * TR::Options::getScratchSpaceLowerBound();

   uint64_t freePhysicalMemorySizeB = compInfo->getCachedFreePhysicalMemoryB();

   // If the last measurement was LOW or VERY_LOW, sample memory at a higher rate to
   // get more up-to-date information
   JITServer::ServerMemoryState memoryState = JITServer::ServerMemoryState::NORMAL;
   int64_t updatePeriodMs = -1;
   if (freePhysicalMemorySizeB != OMRPORT_MEMINFO_NOT_AVAILABLE)
      {
      if (freePhysicalMemorySizeB <= veryLowMemoryThreshold)
         updatePeriodMs = 50;
      else if (freePhysicalMemorySizeB <= lowMemoryThreshold)
         updatePeriodMs = 250;
      }
   bool incompleteInfo;
   freePhysicalMemorySizeB = compInfo->computeAndCacheFreePhysicalMemory(incompleteInfo, updatePeriodMs);

   if (freePhysicalMemorySizeB != OMRPORT_MEMINFO_NOT_AVAILABLE)
      {
      memoryState = freePhysicalMemorySizeB <= veryLowMemoryThreshold ?
         JITServer::ServerMemoryState::VERY_LOW :
         (freePhysicalMemorySizeB <= lowMemoryThreshold ?
            JITServer::ServerMemoryState::LOW :
            JITServer::ServerMemoryState::NORMAL);
      return memoryState;
      }
   // memory info not available, return the default state
   return JITServer::ServerMemoryState::NORMAL;
   }

/**
 * @brief Helper method executed at the end of a compilation
 * to determine whether the server is approaching the maximum number of active threads.
 */
JITServer::ServerActiveThreadsState
computeServerActiveThreadsState(TR::CompilationInfo *compInfo)
   {
   int32_t highActiveThreadThreshold = TR::Options::getHighActiveThreadThreshold();
   int32_t veryHighActiveThreadThreshold = TR::Options::getVeryHighActiveThreadThreshold();

   // Typically getNumCompThreadsActive() needs to be protected by the compilationQueueMonitor,
   // except here we allow some small imprecision because we implement a heuristic.
   int32_t numOfActiveThreads = compInfo->getNumCompThreadsActive();

   JITServer::ServerActiveThreadsState activeThreadState = numOfActiveThreads > veryHighActiveThreadThreshold ?
      JITServer::ServerActiveThreadsState::VERY_HIGH_THREAD :
      (numOfActiveThreads > highActiveThreadThreshold ?
         JITServer::ServerActiveThreadsState::HIGH_THREAD :
         JITServer::ServerActiveThreadsState::NORMAL_THREAD);
   return activeThreadState;
   }

/**
 * @brief Method executed by JITServer to process the end of a compilation.
 */
void
outOfProcessCompilationEnd(TR_MethodToBeCompiled *entry, TR::Compilation *comp)
   {
   entry->_tryCompilingAgain = false; // TODO: Need to handle recompilations gracefully when relocation fails
   auto compInfoPT = (TR::CompilationInfoPerThreadRemote *)entry->_compInfoPT;

   TR::CodeCache *codeCache = comp->cg()->getCodeCache();

   TR_ASSERT(comp->getAotMethodDataStart(), "The header must have been set");
   TR_AOTMethodHeader *aotMethodHeaderEntry = comp->getAotMethodHeaderEntry();

   uint8_t *codeStart = (uint8_t *)aotMethodHeaderEntry->compileMethodCodeStartPC;
   OMR::CodeCacheMethodHeader *codeCacheHeader = (OMR::CodeCacheMethodHeader *)codeStart;

   TR_DataCache *dataCache = (TR_DataCache *)comp->getReservedDataCache();
   TR_ASSERT(dataCache, "A dataCache must be reserved for JITServer compilations");
   J9JITDataCacheHeader *dataCacheHeader = (J9JITDataCacheHeader *)comp->getAotMethodDataStart();

   size_t codeSize = codeCache->getWarmCodeAlloc() - (uint8_t *)codeCacheHeader;
   size_t dataSize = dataCache->getSegment()->heapAlloc - (uint8_t *)dataCacheHeader;

   std::string codeCacheStr((const char *)codeCacheHeader, codeSize);
   std::string dataCacheStr((const char *)dataCacheHeader, dataSize);

   CHTableCommitData chTableData;
   if (!comp->getOption(TR_DisableCHOpts) && !entry->_useAotCompilation)
      {
      TR_CHTable *chTable = comp->getCHTable();
      chTableData = chTable->computeDataForCHTableCommit(comp);
      }

   auto classesThatShouldNotBeNewlyExtended = compInfoPT->getClassesThatShouldNotBeNewlyExtended();

   // Pack log file to send to client
   std::string logFileStr = TR::Options::packLogFile(comp->getOutFile());

   // Send runtime assumptions created during compilation to the client
   std::vector<SerializedRuntimeAssumption> serializedRuntimeAssumptions;
   if (comp->getSerializedRuntimeAssumptions().size() > 0)
      {
      serializedRuntimeAssumptions.reserve(comp->getSerializedRuntimeAssumptions().size());
      for (auto it : comp->getSerializedRuntimeAssumptions())
         {
         serializedRuntimeAssumptions.push_back(*it);
         }
      }

   auto resolvedMirrorMethodsPersistIPInfo = compInfoPT->getCachedResolvedMirrorMethodsPersistIPInfo();

   JITServer::ServerMemoryState memoryState = computeServerMemoryState(compInfoPT->getCompilationInfo());
   JITServer::ServerActiveThreadsState activeThreadState = computeServerActiveThreadsState(compInfoPT->getCompilationInfo());

   // Send methods requring resolved trampolines in this compilation to the client
   std::vector<TR_OpaqueMethodBlock *> methodsRequiringTrampolines;
   if (comp->getMethodsRequiringTrampolines().size() > 0)
      {
      methodsRequiringTrampolines.reserve(comp->getMethodsRequiringTrampolines().size());
      for (auto it : comp->getMethodsRequiringTrampolines())
         {
         methodsRequiringTrampolines.push_back(it);
         }
      }

   auto clientData = comp->getClientData();
   bool aotCacheStore = comp->isAOTCacheStore();
   bool useServerOffsets = aotCacheStore && clientData->useServerOffsets(entry->_stream);
   const CachedAOTMethod *methodRecord = NULL;

   if (compInfoPT->isAOTCacheStore())
      {
      if (comp->isAOTCacheStore())
         {
         auto clientData = comp->getClientData();
         auto cache = clientData->getAOTCache();
         cache->storeMethod(compInfoPT->getDefiningClassChainRecord(), compInfoPT->getMethodIndex(),
                            entry->_optimizationPlan->getOptLevel(), clientData->getAOTHeaderRecord(),
                            comp->getSerializationRecords(), codeCacheHeader, codeSize,
                            dataCacheHeader, dataSize, comp->signature(), clientData->getClientUID(), methodRecord);
         }
      else if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Failed to serialize AOT method %s", comp->signature());
         }
      }

   if (useServerOffsets)
      {
      auto aotCache = clientData->getAOTCache();

      CachedAOTMethod *freshMethodRecord = NULL;
      if (!methodRecord)
         {
         freshMethodRecord = CachedAOTMethod::create(compInfoPT->getDefiningClassChainRecord(),
                                                     compInfoPT->getMethodIndex(),
                                                     entry->_optimizationPlan->getOptLevel(),
                                                     clientData->getAOTHeaderRecord(),
                                                     comp->getSerializationRecords(),
                                                     codeCacheHeader, codeSize,
                                                     dataCacheHeader, dataSize,
                                                     comp->signature());
         methodRecord = freshMethodRecord;
         }

      VectorAllocator<const AOTSerializationRecord *> recordsAllocator(comp->trMemory()->heapMemoryRegion());
      Vector<const AOTSerializationRecord *> records(recordsAllocator);
         {
         OMR::CriticalSection cs(clientData->getAOTCacheKnownIdsMonitor());
         records = aotCache->getSerializationRecords(methodRecord, clientData->getAOTCacheKnownIds(), *comp->trMemory());
         }

      std::vector<std::string> serializedRecords;
      serializedRecords.reserve(records.size());
      for (auto r : records)
         serializedRecords.push_back(std::string((const char *)r, r->size()));

      entry->_stream->finishAotStoreCompilation(
         std::string((const char *)&methodRecord->data(), methodRecord->data().size()),
         serializedRecords,
         chTableData,
         std::vector<TR_OpaqueClassBlock*>(classesThatShouldNotBeNewlyExtended->begin(), classesThatShouldNotBeNewlyExtended->end()),
         logFileStr,
         resolvedMirrorMethodsPersistIPInfo
            ? std::vector<TR_ResolvedJ9Method*>(resolvedMirrorMethodsPersistIPInfo->begin(), resolvedMirrorMethodsPersistIPInfo->end())
            : std::vector<TR_ResolvedJ9Method*>(),
         *entry->_optimizationPlan, serializedRuntimeAssumptions, memoryState, activeThreadState, methodsRequiringTrampolines
      );
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d has successfully compiled AOT cache store %s memoryState=%d",
            compInfoPT->getCompThreadId(), compInfoPT->getCompilation()->signature(), memoryState);
         }
      if (freshMethodRecord)
         {
         AOTCacheRecord::free(freshMethodRecord);
         methodRecord = NULL;
         }
      }
   else
      {
      entry->_stream->finishCompilation(
         codeCacheStr, dataCacheStr, chTableData,
         std::vector<TR_OpaqueClassBlock*>(classesThatShouldNotBeNewlyExtended->begin(), classesThatShouldNotBeNewlyExtended->end()),
         logFileStr,
         resolvedMirrorMethodsPersistIPInfo
            ? std::vector<TR_ResolvedJ9Method*>(resolvedMirrorMethodsPersistIPInfo->begin(), resolvedMirrorMethodsPersistIPInfo->end())
            : std::vector<TR_ResolvedJ9Method*>(),
         *entry->_optimizationPlan, serializedRuntimeAssumptions, memoryState, activeThreadState, methodsRequiringTrampolines
      );
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d has successfully compiled %s memoryState=%d",
            compInfoPT->getCompThreadId(), compInfoPT->getCompilation()->signature(), memoryState);
         }
      }
   compInfoPT->clearPerCompilationCaches();

   Trc_JITServerCompileEnd(compInfoPT->getCompilationThread(), compInfoPT->getCompThreadId(),
         compInfoPT->getCompilation()->signature(), compInfoPT->getCompilation()->getHotnessName());

   // Check whether we need to save a copy of the AOTcache in a file
   if (compInfoPT->getCompilationInfo()->getPersistentInfo()->getJITServerUseAOTCachePersistence())
      {
      if (clientData->usesAOTCache())
         clientData->getAOTCache()->triggerAOTCacheStoreToFileIfNeeded();
      }
   }

TR::CompilationInfoPerThreadRemote::CompilationInfoPerThreadRemote(TR::CompilationInfo &compInfo, J9JITConfig *jitConfig, int32_t id, bool isDiagnosticThread)
   : CompilationInfoPerThread(compInfo, jitConfig, id, isDiagnosticThread),
   _recompilationMethodInfo(NULL),
   _seqNo(0),
   _waitToBeNotified(false),
   _clientOptions(NULL),
   _clientOptionsSize(0),
   _methodIPDataPerComp(NULL),
   _resolvedMethodInfoMap(NULL),
   _resolvedMirrorMethodsPersistIPInfo(NULL),
   _classOfStaticMap(NULL),
   _fieldAttributesCache(NULL),
   _staticAttributesCache(NULL),
   _isUnresolvedStrCache(NULL),
   _classUnloadReadMutexDepth(0),
   _aotCacheStore(false),
   _methodIndex((uint32_t)-1),
   _definingClassChainRecord(NULL)
   {}

/**
 * @brief Method executed by JITServer to dequeue and notify all waiting threads
 *        that the condition they were waiting for has been fulfilled.
 *        Needs to be executed with clientSession->getSequencingMonitor() in hand.
 */
void
TR::CompilationInfoPerThreadRemote::notifyAndDetachWaitingRequests(ClientSessionData *clientSession)
   {
   TR_MethodToBeCompiled *nextEntry = clientSession->getOOSequenceEntryList();
   // I need to keep notifying until the first request that is blocked
   // because the request it depends on hasn't been processed yet.
   while (nextEntry)
      {
      uint32_t nextWaitingSeqNo = ((CompilationInfoPerThreadRemote*)(nextEntry->_compInfoPT))->getSeqNo();
      uint32_t expectedSeqNo = ((CompilationInfoPerThreadRemote*)(nextEntry->_compInfoPT))->getExpectedSeqNo();
      if (expectedSeqNo <= clientSession->getLastProcessedCriticalSeqNo())
         {
         clientSession->notifyAndDetachFirstWaitingThread();
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d notifying out-of-sequence thread %d for clientUID=%llu seqNo=%u (entry=%p)",
               getCompThreadId(), nextEntry->_compInfoPT->getCompThreadId(), (unsigned long long)clientSession->getClientUID(), nextWaitingSeqNo, nextEntry);
         nextEntry = clientSession->getOOSequenceEntryList();
         }
      else // The request I depend on hasn't been processed yet, so stop here
         {
         break;
         }
      }
   }

int32_t TR::CompilationInfoPerThreadRemote::_numClearedCaches = 0;

/**
 * @brief Method executed by a compilation thread at JITServer to wait for all
 *        previous compilation requests it depends on to be processed.
 *        Needs to be executed with sequencingMonitor in hand.
 */
void
TR::CompilationInfoPerThreadRemote::waitForMyTurn(ClientSessionData *clientSession, TR_MethodToBeCompiled &entry)
   {
   TR_ASSERT(getMethodBeingCompiled() == &entry, "Must have stored the entry to be compiled into compInfoPT");
   TR_ASSERT(entry._compInfoPT == this, "Must have stored compInfoPT into the entry to be compiled");
   uint32_t seqNo = getSeqNo();
   uint32_t criticalSeqNo = getExpectedSeqNo(); // This is the seqNo that I must wait for

   // Insert this thread into the list of out of sequence entries
   JITServerHelpers::insertIntoOOSequenceEntryList(clientSession, &entry);

   do // Do a timed wait until the missing seqNo arrives
      {
      // Always reset _waitToBeNotified before waiting on the monitor
      // If a notification does not arrive, this request will timeout and possibly clear the caches
      setWaitToBeNotified(false);

      entry.getMonitor()->enter();
      clientSession->getSequencingMonitor()->exit(); // release monitor before waiting
      const int64_t waitTimeMillis = 1000; // TODO: create an option for this
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d (entry=%p) doing a timed wait for %d ms (waiting for seqNo=%u)",
         getCompThreadId(), &entry, (int32_t)waitTimeMillis, criticalSeqNo);

      Trc_JITServerTimedWait(getCompilationThread(), getCompThreadId(), clientSession,
            (unsigned long long)clientSession->getClientUID(), &entry,
            seqNo, criticalSeqNo, clientSession->getNumActiveThreads(), (int32_t)waitTimeMillis);

      intptr_t monitorStatus = entry.getMonitor()->wait_timed(waitTimeMillis, 0); // 0 or J9THREAD_TIMED_OUT
      if (monitorStatus == 0) // Thread was notified
         {
         entry.getMonitor()->exit();
         clientSession->getSequencingMonitor()->enter();

         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d (entry=%p) is parked. seqNo=%u was notified",
               getCompThreadId(), &entry, seqNo);
         // Will verify condition again to see if expectedSeqNo has advanced enough

         Trc_JITServerParkThread(getCompilationThread(), getCompThreadId(), clientSession,
               (unsigned long long)clientSession->getClientUID(), &entry,
               seqNo, criticalSeqNo, clientSession->getNumActiveThreads(), seqNo);
         }
      else // Timeout
         {
         entry.getMonitor()->exit();
         TR_ASSERT(monitorStatus == J9THREAD_TIMED_OUT, "Unexpected monitor state");
         if (TR::Options::isAnyVerboseOptionSet(TR_VerboseCompFailure, TR_VerboseJITServer, TR_VerbosePerformance))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d (entry=%p) timed-out while waiting for seqNo=%u ",
               getCompThreadId(), &entry, criticalSeqNo);

         Trc_JITServerTimedOut(getCompilationThread(), getCompThreadId(), clientSession,
               (unsigned long long)clientSession->getClientUID(), &entry,
               seqNo, criticalSeqNo, clientSession->getNumActiveThreads(), criticalSeqNo);

         // The simplest thing is to delete the cached data for the session and start fresh
         // However, we must wait for any active threads to drain out
         clientSession->getSequencingMonitor()->enter();

         // This thread could have been waiting, then timed-out and then waited
         // to acquire the sequencing monitor. Check whether it can proceed.
         if (criticalSeqNo <= clientSession->getLastProcessedCriticalSeqNo())
            {
            // The entry cannot be in the list
            TR_MethodToBeCompiled *headEntry = clientSession->getOOSequenceEntryList();
            if (headEntry)
               {
               uint32_t headSeqNo = ((TR::CompilationInfoPerThreadRemote*)(headEntry->_compInfoPT))->getSeqNo();
               TR_ASSERT_FATAL(seqNo < headSeqNo, "Next in line method cannot be in the waiting list: seqNo=%u >= headSeqNo=%u entry=%p headEntry=%p",
                  seqNo, headSeqNo, &entry, headEntry);
               }
            break; // It's my turn, so proceed
            }

         if (clientSession->getNumActiveThreads() <= 0 && // Wait for active threads to quiesce
            &entry == clientSession->getOOSequenceEntryList() && // Allow only the smallest seqNo which is the head
            !getWaitToBeNotified()) // Avoid a cohort of threads clearing the caches
            {
            clientSession->clearCaches();
            incNumClearedCaches();

            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                  "compThreadID=%d has cleared the session caches for clientUID=%llu criticalSeqNo=%u seqNo=%u firstEntry=%p",
                  getCompThreadId(), clientSession->getClientUID(), criticalSeqNo, seqNo, &entry);

            Trc_JITServerClearedSessionCaches(getCompilationThread(), getCompThreadId(), clientSession,
                  (unsigned long long)clientSession->getClientUID(),
                  seqNo, criticalSeqNo, clientSession->getNumActiveThreads(), &entry,
                  clientSession->getLastProcessedCriticalSeqNo(), seqNo);

            clientSession->setLastProcessedCriticalSeqNo(criticalSeqNo);// Allow myself to go through
            notifyAndDetachWaitingRequests(clientSession);
            // Mark the next request that it should not try to clear the caches,
            // but rather to sleep again waiting for my notification.
            // We only need to do this for the head entry in the waiting list
            // due to the check `&entry == clientSession->getOOSequenceEntryList()` above
            TR_MethodToBeCompiled *nextWaitingEntry = clientSession->getOOSequenceEntryList();
            if (nextWaitingEntry)
               {
               ((TR::CompilationInfoPerThreadRemote*)(nextWaitingEntry->_compInfoPT))->setWaitToBeNotified(true);
               }
            }
         else
            {
            // Wait until all active threads have been drained
            // and the head of the list has cleared the caches
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                  "compThreadID=%d which previously timed-out will go to sleep again. Possible reasons numActiveThreads=%d waitToBeNotified=%d",
                  getCompThreadId(), clientSession->getNumActiveThreads(), getWaitToBeNotified());

            Trc_JITServerThreadGoSleep(getCompilationThread(), getCompThreadId(), clientSession,
                  (unsigned long long)clientSession->getClientUID(),
                  seqNo, criticalSeqNo, clientSession->getNumActiveThreads(), getWaitToBeNotified());
            }
         }
      } while (criticalSeqNo > clientSession->getLastProcessedCriticalSeqNo());
   }

bool
TR::CompilationInfoPerThreadRemote::serveCachedAOTMethod(TR_MethodToBeCompiled &entry, J9Method *method, J9Class *definingClass,
                                                         TR_OptimizationPlan *optPlan, ClientSessionData *clientData,
                                                         J9::J9SegmentProvider &scratchSegmentProvider)
   {
   PORT_ACCESS_FROM_JITCONFIG(_jitConfig);
   uintptr_t startTime = TR::Options::getVerboseOption(TR_VerbosePerformance) ? j9time_usec_clock() : 0;

   auto aotCache = clientData->getAOTCache();
   auto serializedMethod = aotCache->findMethod(_definingClassChainRecord, _methodIndex,
                                                optPlan->getOptLevel(), clientData->getAOTHeaderRecord());
   if (!serializedMethod)
      return false;

   size_t segmentSize = scratchSegmentProvider.getPreferredSegmentSize();
   if (!segmentSize)
      segmentSize = 1 << 24/*16 MB*/;
   TR::RawAllocator rawAllocator(getCompilationThread()->javaVM);
   J9::SystemSegmentProvider segmentProvider(1 << 16/*64 KB*/, segmentSize, TR::Options::getScratchSpaceLimit(),
                                             scratchSegmentProvider, rawAllocator);
   TR::Region region(segmentProvider, rawAllocator);
   TR_Memory trMemory(*clientData->persistentMemory(), region);

   VectorAllocator<const AOTSerializationRecord *> recordsAllocator(trMemory.heapMemoryRegion());
   Vector<const AOTSerializationRecord *> records(recordsAllocator);
      {
      OMR::CriticalSection cs(clientData->getAOTCacheKnownIdsMonitor());
      records = aotCache->getSerializationRecords(serializedMethod, clientData->getAOTCacheKnownIds(), trMemory);
      }

   std::vector<std::string> serializedRecords;
   serializedRecords.reserve(records.size());
   for (auto r : records)
      serializedRecords.push_back(std::string((const char *)r, r->size()));

   // Record the AOT cache hit as a "compilation end" in vlog.
   // The code is mostly copied from CompilationInfoPerThreadBase::logCompilationSuccess().
   if (TR::Options::isAnyVerboseOptionSet(TR_VerbosePerformance, TR_VerboseCompileEnd))
      {
      uintptr_t currentTime = TR::Options::getVerboseOption(TR_VerbosePerformance) ? j9time_usec_clock() : 0;

      auto &classRecord = serializedMethod->definingClassRecord()->data();
      const J9ROMMethod *romMethod;
         {
         OMR::CriticalSection cs(clientData->getROMMapMonitor());
         auto it = clientData->getJ9MethodMap().find(method);
         TR_ASSERT(it != clientData->getJ9MethodMap().end(), "Method %p must be cached", method);
         romMethod = it->second._romMethod;
         }

      TR_VerboseLog::CriticalSection vlogLock;
      TR_VerboseLog::write(TR_Vlog_COMP,
         "(AOT cache %s) %.*s.%.*s%.*s %s Q_SZ=%d Q_SZI=%d QW=%d j9m=%p bcsz=%u",
         TR::Compilation::getHotnessName(optPlan->getOptLevel()), (int)classRecord.nameLength(), classRecord.name(),
         J9UTF8_LENGTH(J9ROMMETHOD_NAME(romMethod)), (const char *)J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)),
         J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(romMethod)), (const char *)J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(romMethod)),
         entry.getMethodDetails().name(), _compInfo.getMethodQueueSize(), _compInfo.getNumQueuedFirstTimeCompilations(),
         _compInfo.getQueueWeight(), method, _compInfo.getMethodBytecodeSize(romMethod)
      );

      if (TR::Options::getVerboseOption(TR_VerbosePerformance))
         TR_VerboseLog::write(" time=%zuus mem=[region=%llu system=%llu]KB", currentTime - startTime,
                              (unsigned long long)segmentProvider.regionBytesAllocated() / 1024,
                              (unsigned long long)segmentProvider.systemBytesAllocated() / 1024);

      TR_VerboseLog::write(" compThreadID=%d", getCompThreadId());

      CpuUtilization *cpuUtil = _compInfo.getCpuUtil();
      if (cpuUtil->isFunctional())
         TR_VerboseLog::write(" CpuLoad=%d%%(%d%%avg) JvmCpu=%d%%", cpuUtil->getCpuUsage(),
                              cpuUtil->getAvgCpuUsage(), cpuUtil->getVmCpuUsage());

      if (TR::Options::getVerboseOption(TR_VerboseCompilationThreads))
         {
         int32_t cpuUtil = getCompThreadCPU().getThreadLastCpuUtil();
         if (cpuUtil >= 0)
            TR_VerboseLog::write(" compCPU=%d%%", cpuUtil);
         }

      if (TR::Options::getVerboseOption(TR_VerbosePerformance))
         TR_VerboseLog::write(" queueTime=%zuus", currentTime - entry._entryTime);

      TR_VerboseLog::writeLine("");
      }

   //NOTE: Leaving optimization plan unchanged. This can be changed in the future.
   entry._stream->write(JITServer::MessageType::AOTCache_serializedAOTMethod,
                        std::string((const char *)&serializedMethod->data(), serializedMethod->data().size()),
                        serializedRecords, *optPlan, computeServerMemoryState(getCompilationInfo()),
                        computeServerActiveThreadsState(getCompilationInfo()));
   return true;
   }

/**
 * @brief Method executed by JITServer to process the compilation request.
 */
void
TR::CompilationInfoPerThreadRemote::processEntry(TR_MethodToBeCompiled &entry, J9::J9SegmentProvider &scratchSegmentProvider)
   {
   static bool enableJITServerPerCompConn = feGetEnv("TR_EnableJITServerPerCompConn") ? true : false;

   bool abortCompilation = false;
   bool deleteStream = false;
   uint64_t clientId = 0;
   TR::CompilationInfo *compInfo = getCompilationInfo();
   J9VMThread *compThread = getCompilationThread();
   JITServer::ServerStream *stream = entry._stream;
   setMethodBeingCompiled(&entry); // Must have compilation monitor
   entry._compInfoPT = this; // Create the reverse link
   // Update the last time the compilation thread had to do something.
   compInfo->setLastReqStartTime(compInfo->getPersistentInfo()->getElapsedTime());


   if (stream == LOAD_AOTCACHE_REQUEST || stream == SAVE_AOTCACHE_REQUEST)
      {
      // This is not a true compilation request, but rather a request to save/load an AOTCache to/from file
      compInfo->releaseCompMonitor(compThread);
      auto aotCacheMap = compInfo->getJITServerAOTCacheMap();
      TR_ASSERT(aotCacheMap, "aotCacheMap must exist if such a special request was issued");
      if (stream == LOAD_AOTCACHE_REQUEST)
         aotCacheMap->loadNextQueuedAOTCacheFromFile(scratchSegmentProvider);
      else
         aotCacheMap->saveNextQueuedAOTCacheToFile();

      // We had the compilation monitor in hand when entering processEntry() and we must leave with it in hand
      compInfo->acquireCompMonitor(compThread);

      // Put the request back into the pool
      setMethodBeingCompiled(NULL); // Must have the compQmonitor
      compInfo->recycleCompilationEntry(&entry);
      // Will not attempt to suspend this compilation thread.
      // If the queue is empty, it will go to sleep waiting for work
      return;
      }

   clearPerCompilationCaches();

   _recompilationMethodInfo = NULL;

   // Release compMonitor before doing the blocking read
   compInfo->releaseCompMonitor(compThread);

   char *clientOptions = NULL;
   TR_OptimizationPlan *optPlan = NULL;
   _vm = NULL;
   bool useAotCompilation = false;
   uint32_t seqNo = 0;
   ClientSessionData *clientSession = NULL;
   bool isCriticalRequest = false;
   // numActiveThreads is incremented after it waits for its turn to execute
   // and before the thread processes unloaded classes and CHTable init and update.
   // A stream exception could be thrown at any time such as reading the compilation
   // request (numActiveThreads hasn't been incremented), or an exeption could be
   // thrown when a message such as MessageType::getUnloadedClassRangesAndCHTable
   // is sent to the client (numActiveThreads has been incremented).
   // hasIncNumActiveThreads is used to determine if decNumActiveThreads() should be
   // called when an exception is thrown.
   bool hasIncNumActiveThreads = false;
   // Keep track of whether the lastProcessedCriticalSeqNo in the client session was updated
   bool hasUpdatedSeqNo = false;
   bool aotCacheHit = false;

   _aotCacheStore = false;
   _methodIndex = (uint32_t)-1;
   _definingClassChainRecord = NULL;

   try
      {
      auto req = stream->readCompileRequest<
         uint64_t, uint32_t, uint32_t, J9Method *, J9Class *, TR_OptimizationPlan, std::string,
         J9::IlGeneratorMethodDetailsType, std::vector<TR_OpaqueClassBlock *>, std::vector<TR_OpaqueClassBlock *>,
         JITServerHelpers::ClassInfoTuple, std::string, std::string, std::string, std::string,
         bool, bool, bool, bool, uint32_t, uintptr_t, std::vector<J9Class *>, std::vector<J9Class *>,
         std::vector<JITServerHelpers::ClassInfoTuple>, std::vector<uintptr_t>
      >();

      clientId                      = std::get<0>(req);
      seqNo                         = std::get<1>(req); // Sequence number at the client
      uint32_t criticalSeqNo        = std::get<2>(req); // Sequence number of the request this request depends upon
      J9Method *ramMethod           = std::get<3>(req);
      J9Class *clazz                = std::get<4>(req);
      auto &clientOptPlan           = std::get<5>(req);
      auto &detailsStr              = std::get<6>(req);
      auto detailsType              = std::get<7>(req);
      auto &unloadedClasses         = std::get<8>(req);
      auto &illegalModificationList = std::get<9>(req);
      auto &classInfoTuple          = std::get<10>(req);
      auto &clientOptStr            = std::get<11>(req);
      auto &recompInfoStr           = std::get<12>(req);
      auto &chtableUnloads          = std::get<13>(req);
      auto &chtableMods             = std::get<14>(req);
      useAotCompilation             = std::get<15>(req);
      bool isInStartupPhase         = std::get<16>(req);
      bool requestedAOTCacheStore   = std::get<17>(req);
      bool requestedAOTCacheLoad    = std::get<18>(req);
      _methodIndex                  = std::get<19>(req);
      uintptr_t classChainOffset    = std::get<20>(req);
      auto &ramClassChain           = std::get<21>(req);
      auto &uncachedRAMClasses      = std::get<22>(req);
      auto &uncachedClassInfos      = std::get<23>(req);
      auto &newKnownIds             = std::get<24>(req);

      TR_ASSERT_FATAL(TR::Compiler->persistentMemory() == compInfo->persistentMemory(),
                      "per-client persistent memory must not be set at this point");

      TR::IlGeneratorMethodDetails *clientDetails = (TR::IlGeneratorMethodDetails *)detailsStr.data();
      *(uintptr_t *)clientDetails = 0; // smash remote vtable pointer to catch bugs early
      TR::IlGeneratorMethodDetails serverDetailsStorage;
      TR::IlGeneratorMethodDetails *serverDetails = TR::IlGeneratorMethodDetails::clone(serverDetailsStorage, *clientDetails, detailsType);

      isCriticalRequest =
         (!chtableMods.empty()
         || !chtableUnloads.empty()
         || !illegalModificationList.empty()
         || !unloadedClasses.empty())
         && !serverDetails->isJitDumpMethod(); // if this is a JitDump recompilation, ignore any critical updates

      if (useAotCompilation)
         {
         _vm = TR_J9VMBase::get(_jitConfig, compThread, TR_J9VMBase::J9_SHARED_CACHE_SERVER_VM);
         }
      else
         {
         _vm = TR_J9VMBase::get(_jitConfig, compThread, TR_J9VMBase::J9_SERVER_VM);
         }

      if (_vm->sharedCache())
         {
         // Set/update stream pointer in shared cache.
         // Note that if remote-AOT is enabled, even regular J9_SERVER_VM will have a shared cache
         // This behaviour is consistent with non-JITServer
         ((TR_J9JITServerSharedCache *) _vm->sharedCache())->setStream(stream);
         // Also cache this compilation thread
         ((TR_J9JITServerSharedCache *) _vm->sharedCache())->setCompInfoPT(this);
         }

      //if (seqNo == 501)
      //   throw JITServer::StreamFailure(); // stress testing

      stream->setClientId(clientId);
      setSeqNo(seqNo); // Memorize the sequence number of this request
      setExpectedSeqNo(criticalSeqNo); // Memorize the message I have to wait for

      bool sessionDataWasEmpty = false;
         {
         // Get a pointer to this client's session data
         // Obtain monitor RAII style because creating a new hastable entry may throw bad_alloc
         OMR::CriticalSection compilationMonitorLock(compInfo->getCompilationMonitor());
         compInfo->getClientSessionHT()->purgeOldDataIfNeeded(); // Try to purge old data
         if (!(clientSession = compInfo->getClientSessionHT()->findOrCreateClientSession(clientId, criticalSeqNo, &sessionDataWasEmpty, _jitConfig)))
            throw std::bad_alloc();

         setClientData(clientSession); // Cache the session data into CompilationInfoPerThreadRemote object

         // After this line, all persistent allocations are made per-client,
         // until exitPerClientAllocationRegion is called
         enterPerClientAllocationRegion();
         TR_ASSERT(!clientSession->usesPerClientMemory() || (TR::Compiler->persistentMemory() != compInfo->persistentMemory()),
                   "per-client persistent memory must be set at this point");

         clientSession->setIsInStartupPhase(isInStartupPhase);
         } // End critical section

     if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "compThreadID=%d %s clientSessionData=%p for clientUID=%llu seqNo=%u (isCritical=%d) (criticalSeqNo=%u lastProcessedCriticalReq=%u)",
            getCompThreadId(), sessionDataWasEmpty ? "created" : "found", clientSession, (unsigned long long)clientId, seqNo,
            isCriticalRequest, criticalSeqNo, clientSession->getLastProcessedCriticalSeqNo());

      Trc_JITServerClientSessionData1(compThread, getCompThreadId(), sessionDataWasEmpty ? "created" : "found",
         clientSession, (unsigned long long)clientId, seqNo, isCriticalRequest, criticalSeqNo, clientSession->getLastProcessedCriticalSeqNo());

      if (!newKnownIds.empty())
         {
         OMR::CriticalSection cs(clientSession->getAOTCacheKnownIdsMonitor());
         clientSession->getAOTCacheKnownIds().insert(newKnownIds.begin(), newKnownIds.end());
         }

      // We must process unloaded classes lists in the same order they were generated at the client
      // Use a sequencing scheme to re-order compilation requests
      //
      clientSession->getSequencingMonitor()->enter();
      clientSession->updateMaxReceivedSeqNo(seqNo); // TODO: why do I need this?

      // This request can go through as long as criticalSeqNo has been processed
      if (criticalSeqNo > clientSession->getLastProcessedCriticalSeqNo())
         {
         // Park this request until `criticalSeqNo` arrives and is processed
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d out-of-sequence msg=%u detected for clientUID=%llu criticalSeqNo=%u > lastCriticalSeqNo=%u. Parking this thread (entry=%p)",
            getCompThreadId(), seqNo, (unsigned long long)clientId, criticalSeqNo, clientSession->getLastProcessedCriticalSeqNo(), &entry);

         Trc_JITServerOutOfSequenceMsg1(compThread, getCompThreadId(), clientSession, (unsigned long long)clientId, &entry, seqNo,
            clientSession->getLastProcessedCriticalSeqNo(), clientSession->getNumActiveThreads(), clientSession->getLastProcessedCriticalSeqNo());

         waitForMyTurn(clientSession, entry);
         }

      TR_ASSERT_FATAL(criticalSeqNo <= clientSession->getLastProcessedCriticalSeqNo(),
         "Critical requests must be processed in order: compThreadID=%d seqNo=%u criticalSeqNo=%u > lastProcessedCriticalSeqNo=%u clientUID=%llu",
         getCompThreadId(), seqNo, criticalSeqNo, clientSession->getLastProcessedCriticalSeqNo(), (unsigned long long)clientId);

      if (criticalSeqNo < clientSession->getLastProcessedCriticalSeqNo())
         {
         // It's possible that a critical request is delayed so much that we clear the caches, start over,
         // and then the missing request arrives. At this point we should ignore it, so we throw StreamOOO.
         // Another possibility is when request 1 arrives before request 0 and creates the session, setting
         // lastProcessedCriticalSeqNo to 1. As request 0 is "critical" by definition, we throw StreamOOO.
         // Request 0 will be aborted (retried at te client) while request 1 will ask about entire CHTable
         // because CHTable is empty at the server
         if (isCriticalRequest)
            {
            if (TR::Options::isAnyVerboseOptionSet(TR_VerboseCompFailure, TR_VerboseJITServer, TR_VerbosePerformance))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                  "compThreadID=%d discarding older msg for clientUID=%llu seqNo=%u criticalSeqNo=%u < lastProcessedCriticalSeqNo=%u",
                  getCompThreadId(), (unsigned long long)clientId, seqNo, criticalSeqNo, clientSession->getLastProcessedCriticalSeqNo());

            Trc_JITServerDiscardMessage(compThread, getCompThreadId(), clientSession,
               (unsigned long long)clientId, seqNo, criticalSeqNo, clientSession->getNumActiveThreads());

            clientSession->getSequencingMonitor()->exit();
            throw JITServer::StreamOOO();
            }
         }

      // Increment the number of active threads before issuing the read for ramClass
      clientSession->incNumActiveThreads();
      hasIncNumActiveThreads = true;

      // If class redefinition using HCR extensions occurred, must clear all the caches
      bool mustClearCaches = std::find(unloadedClasses.begin(), unloadedClasses.end(), ClientSessionData::mustClearCachesFlag) != unloadedClasses.end();
      if (mustClearCaches)
         {
         if (TR::Options::isAnyVerboseOptionSet(TR_VerboseCompFailure, TR_VerboseJITServer, TR_VerbosePerformance))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
               "compThreadID=%d clearing all caches for clientUID=%llu due to class redefinition using HCR extensions",
               getCompThreadId(), (unsigned long long) clientId);
         Trc_JITServerClearCaches(compThread, getCompThreadId(), clientSession, (unsigned long long) clientId);

         clientSession->clearCachesLocked(_vm);
         }

      // We can release the sequencing monitor now because no critical request with a
      // larger sequence number can pass until I increment lastProcessedCriticalSeqNo
      clientSession->getSequencingMonitor()->exit();

      // At this point I know that all preceeding requests have been processed
      // and only one thread with critical information can ever be present in this section
      if (!clientSession->cachesAreCleared())
         {
         // Free data for all classes that were unloaded for this sequence number
         // Redefined classes are marked as unloaded, since they need to be cleared
         // from the ROM class cache.
         if (!unloadedClasses.empty())
            {
            clientSession->processUnloadedClasses(unloadedClasses, true); // this locks getROMMapMonitor()
            }

         if (!illegalModificationList.empty())
            {
            clientSession->processIllegalFinalFieldModificationList(illegalModificationList); // this locks getROMMapMonitor()
            }

         // Process the CHTable updates in order
         // Note that applying the updates will acquire the CHTable monitor and VMAccess
         if ((!chtableUnloads.empty() || !chtableMods.empty())
             && !serverDetails->isJitDumpMethod())
            {
            auto chTable = (JITServerPersistentCHTable*)clientSession->getCHTable(); // Will create CHTable if it doesn't exist
            TR_ASSERT_FATAL(chTable->isInitialized(), "CHTable must have been initialized for clientUID=%llu", (unsigned long long)clientId);
            chTable->doUpdate(_vm, chtableUnloads, chtableMods);
            }
         }
      else // Internal caches are empty
         {
         OMR::CriticalSection cs(clientSession->getCacheInitMonitor());
         if (clientSession->cachesAreCleared())
            {
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d will ask for address ranges of unloaded classes and CHTable for clientUID %llu",
                  getCompThreadId(), (unsigned long long)clientId);

            stream->write(JITServer::MessageType::getUnloadedClassRangesAndCHTable, compInfo->getPersistentInfo()->getServerUID());
            auto response = stream->read<std::vector<TR_AddressRange>, int32_t, std::string>();
            // TODO: we could send JVM info that is global and does not change together with CHTable
            auto &unloadedClassRanges = std::get<0>(response);
            auto maxRanges = std::get<1>(response);
            std::string &serializedCHTable = std::get<2>(response);

            clientSession->initializeUnloadedClassAddrRanges(unloadedClassRanges, maxRanges);
            if (!unloadedClasses.empty())
               {
               // This function updates multiple caches based on the newly unloaded classes list.
               // Pass `false` here to indicate that we want the unloaded class ranges table cache excluded,
               // since we just retrieved the entire table and it should therefore already be up to date.
               clientSession->processUnloadedClasses(unloadedClasses, false);
               }
            auto chTable = static_cast<JITServerPersistentCHTable *>(clientSession->getCHTable());
            // Need CHTable mutex
            TR_ASSERT_FATAL(!chTable->isInitialized(), "CHTable must be empty for clientUID=%llu", (unsigned long long)clientId);
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d will initialize CHTable for clientUID %llu size=%zu",
                  getCompThreadId(), (unsigned long long)clientId, serializedCHTable.size());
            chTable->initializeCHTable(_vm, serializedCHTable);
            clientSession->setCachesAreCleared(false);
            }
         }

      // Critical requests must update lastProcessedCriticalSeqNo and notify any waiting threads.
      // Dependent threads will pass through once we've released the sequencing monitor.
      if (isCriticalRequest)
         {
         clientSession->getSequencingMonitor()->enter();
         TR_ASSERT_FATAL(criticalSeqNo <= clientSession->getLastProcessedCriticalSeqNo(),
            "compThreadID=%d clientSessionData=%p clientUID=%llu seqNo=%u, criticalSeqNo=%u != lastProcessedCriticalSeqNo=%u, numActiveThreads=%d",
            getCompThreadId(), clientSession, (unsigned long long)clientId, seqNo, criticalSeqNo,
            clientSession->getLastProcessedCriticalSeqNo(), clientSession->getNumActiveThreads());

         clientSession->setLastProcessedCriticalSeqNo(seqNo);
         hasUpdatedSeqNo = true;
         Trc_JITServerUpdateSeqNo(getCompilationThread(), getCompThreadId(), clientSession,
                                 (unsigned long long)clientSession->getClientUID(),
                                 getSeqNo(), criticalSeqNo, clientSession->getNumActiveThreads(),
                                 clientSession->getLastProcessedCriticalSeqNo(), seqNo);

         // Notify waiting threads. I need to keep notifying until the first request that
         // is blocked because the request it depends on hasn't been processed yet.
         notifyAndDetachWaitingRequests(clientSession);

         // Finally, release the sequencing monitor so that other threads
         // can process their lists of unloaded classes
         clientSession->getSequencingMonitor()->exit();
         }

      // Copy the option string
      copyClientOptions(clientOptStr, clientSession->persistentMemory());

      // _recompilationMethodInfo will be passed to J9::Recompilation::_methodInfo,
      // which will get freed in populateBodyInfo() when creating the
      // method meta data during the compilation. Since _recompilationMethodInfo is already
      // freed in populateBodyInfo(), no need to free it at the end of the compilation in this method.
      if (recompInfoStr.size() > 0)
         {
         _recompilationMethodInfo = new (clientSession->persistentMemory()) TR_PersistentMethodInfo();
         memcpy(_recompilationMethodInfo, recompInfoStr.data(), sizeof(TR_PersistentMethodInfo));
         J9::Recompilation::resetPersistentProfileInfo(_recompilationMethodInfo);
         }
      // Get the ROMClass for the method to be compiled if it is already cached
      // Or read it from the compilation request and cache it otherwise
      J9ROMClass *romClass = NULL;
      if (!(romClass = JITServerHelpers::getRemoteROMClassIfCached(clientSession, clazz)))
         {
         // Class for current request is not yet cached.
         // If the client sent us the desired information in the compilation request, use that.
         if(!(std::get<0>(classInfoTuple).empty()))
            {
            romClass = JITServerHelpers::romClassFromString(std::get<0>(classInfoTuple), std::get<25>(classInfoTuple), clientSession->persistentMemory());
            }
         else
            {
            // The client did not embed info about desired class in the compilation request.
            // This could happen if the client determined that it sent required information in
            // a previous request, info which the server is expected to cache. However, this
            // could be a new JITServer instance.
            // Send a message to the client to retrieve desired info.
            romClass = JITServerHelpers::getRemoteROMClass(clazz, stream, clientSession->persistentMemory(), classInfoTuple);
            }
         romClass = JITServerHelpers::cacheRemoteROMClassOrFreeIt(clientSession, clazz, romClass, classInfoTuple);
         TR_ASSERT_FATAL(romClass, "ROM class of J9Class=%p must be cached at this point", clazz);
         }

      // Optimization plan needs to use the global allocator,
      // because there is a global pool of plans
         {
         JITServer::GlobalAllocationRegion globalRegion(this);
         // Build my entry
         if (!(optPlan = TR_OptimizationPlan::alloc(clientOptPlan.getOptLevel())))
            throw std::bad_alloc();
         optPlan->clone(&clientOptPlan);
         if (optPlan->isLogCompilation())
            optPlan->setLogCompilation(clientOptPlan.getLogCompilation());
         }

      // Remember the timestamp of when the request was queued before re-initializing the entry
      uintptr_t entryTime = entry._entryTime;

      // All entries have the same priority for now. In the future we may want to give higher priority to sync requests
      // Also, oldStartPC is always NULL for JITServer
      entry._freeTag = ENTRY_IN_POOL_FREE; // Pretend we just got it from the pool because we need to initialize it again
      entry.initialize(*serverDetails, NULL, CP_SYNC_NORMAL, optPlan);

      entry._entryTime = entryTime; // Use the more accurate original entry timestamp
      entry._compInfoPT = this; // Need to know which comp thread is handling this request
      entry._methodIsInSharedCache = false; // No SCC for now in JITServer
      entry._useAotCompilation = useAotCompilation;
      entry._async = true; // All of requests at the server are async
      // Weight is irrelevant for JITServer.
      // If we want something then we need to increaseQueueWeightBy(weight) while holding compilation monitor
      entry._weight = 0;
      entry._jitStateWhenQueued = compInfo->getPersistentInfo()->getJitState();
      entry._stream = stream; // Add the stream to the entry

      auto aotCache = clientSession->getOrCreateAOTCache(stream);
      _aotCacheStore = requestedAOTCacheStore && aotCache && JITServerAOTCacheMap::cacheHasSpace();
      bool aotCacheLoad = requestedAOTCacheLoad && aotCache;
      if (aotCache && !aotCacheLoad)
         aotCache->incNumCacheBypasses();

      if (_aotCacheStore || aotCacheLoad)
         {
         // Get defining class chain record to use as a part of the key to lookup or store the method in AOT cache
         JITServerHelpers::cacheRemoteROMClassBatch(clientSession, uncachedRAMClasses, uncachedClassInfos);
         bool missingLoaderInfo = false;
         _definingClassChainRecord = clientSession->getClassChainRecord(clazz, classChainOffset, ramClassChain, stream,
                                                                        missingLoaderInfo, &scratchSegmentProvider);
         if (!_definingClassChainRecord)
            {
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                  "clientUID %llu failed to get defining class chain record for %p due to %s; "
                  "method %p won't be loaded from or stored in AOT cache", (unsigned long long)clientId, clazz,
                  missingLoaderInfo ? "missing class loader info" : "the AOT cache size limit", ramMethod
               );

            if (aotCacheLoad)
               aotCache->incNumCacheMisses();
            _aotCacheStore = false;
            aotCacheLoad = false;
            }
         }

      if (aotCacheLoad)
         aotCacheHit = serveCachedAOTMethod(entry, ramMethod, clazz, &clientOptPlan, clientSession, scratchSegmentProvider);

      // If the client requests that the server use AOT cache offsets during AOT cache compilations, then
      // the client will be ignoring its local SCC (if it even exists) for the duration of the compilation.
      // This means that if the client requests an AOT cache store in that scenario and the server
      // cannot fulfill that request, the server must abort the compilation.
      // If the client isn't requesting server offsets, then the compilation can still continue, albeit as
      // a regular AOT compilation.
      if (clientSession->useServerOffsets(stream) &&
          requestedAOTCacheStore &&
          !aotCacheHit &&
          !_aotCacheStore)
         {
         abortCompilation = true;
         bool aotCacheUnavailable = !aotCache;
         bool aotCacheStoreUnavailable = aotCacheUnavailable || !JITServerAOTCacheMap::cacheHasSpace();
         stream->write(JITServer::MessageType::AOTCache_failure, aotCacheUnavailable, aotCacheStoreUnavailable);
         }
      }
   catch (const JITServer::StreamFailure &e)
      {
      // This could happen because the client disconnected on purpose, in which case there is no harm done,
      // or if there was a network problem. In the latter case, if the request was critical, the server
      // will observe a missing seqNo and after a timeout, it will clear the caches and start over
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d stream failed while reading the compilation request: %s",
            getCompThreadId(), e.what());

      Trc_JITServerStreamFailure(compThread, getCompThreadId(), __FUNCTION__, "", "", e.what());

      abortCompilation = true;
      deleteStream = true;
      }
   catch (const JITServer::StreamVersionIncompatible &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer) || TR::Options::getVerboseOption(TR_VerboseJITServerConns))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d stream version incompatible: %s", getCompThreadId(), e.what());

      Trc_JITServerStreamVersionIncompatible(compThread,  getCompThreadId(), __FUNCTION__, "", "", e.what());

      stream->writeError(compilationStreamVersionIncompatible);
      abortCompilation = true;
      deleteStream = true;
      }
   catch (const JITServer::StreamMessageTypeMismatch &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d stream message type mismatch: %s", getCompThreadId(), e.what());

      Trc_JITServerStreamMessageTypeMismatch(compThread, getCompThreadId(), __FUNCTION__, "", "", e.what());

      stream->writeError(compilationStreamMessageTypeMismatch);
      abortCompilation = true;
      deleteStream = true;
      }
   catch (const JITServer::StreamConnectionTerminate &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d stream connection terminated by JITClient on stream %p while reading the compilation request: %s",
         getCompThreadId(), stream, e.what());

      Trc_JITServerStreamConnectionTerminate(compThread, getCompThreadId(), __FUNCTION__, stream, e.what());

      abortCompilation = true;
      deleteStream = true;
      }
   catch (const JITServer::StreamClientSessionTerminate &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d stream client session terminated by JITClient: %s", getCompThreadId(), e.what());

      Trc_JITServerStreamClientSessionTerminate(compThread, getCompThreadId(), __FUNCTION__, e.what());

      abortCompilation = true;
      deleteStream = true;

      deleteClientSessionData(e.getClientId(), compInfo, compThread);
      }
   catch (const JITServer::StreamInterrupted &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d stream interrupted by JITClient while reading the compilation request: %s",
            getCompThreadId(), e.what());

      Trc_JITServerStreamInterrupted(compThread, getCompThreadId(), __FUNCTION__, "", "", e.what());

      abortCompilation = true;
      // If the client aborted this compilation, it could have happened while asking for entire
      // CHTable and unloaded class address ranges and at that point the seqNo was not updated.
      // In such case, we must update seqNo now to allow for blocking threads to pass through.
      // Since the caches are already cleared there is no harm in discarding this message.
      if (isCriticalRequest && !hasUpdatedSeqNo)
         {
         clientSession->getSequencingMonitor()->enter();
         if (seqNo > clientSession->getLastProcessedCriticalSeqNo())
            {
            Trc_JITServerUpdateSeqNo(getCompilationThread(), getCompThreadId(), clientSession,
               (unsigned long long)clientSession->getClientUID(),
               seqNo, getExpectedSeqNo(), clientSession->getNumActiveThreads(),
               clientSession->getLastProcessedCriticalSeqNo(), seqNo);

            clientSession->setLastProcessedCriticalSeqNo(seqNo);
            notifyAndDetachWaitingRequests(clientSession);
            }
         clientSession->getSequencingMonitor()->exit();
         }
      }
   catch (const JITServer::StreamOOO &e)
      {
      // Error message was printed when the exception was thrown
      stream->writeError(compilationStreamLostMessage); // the client should recognize this code and retry
      abortCompilation = true;
      }
   catch (const std::bad_alloc &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Server out of memory in processEntry: %s", e.what());
      stream->writeError(compilationLowPhysicalMemory, (uint64_t) computeServerMemoryState(getCompilationInfo()));
      abortCompilation = true;
      }
   catch (const JITServer::StreamAotCacheMapRequest &e)
      {
      const std::string& aotCacheName = e.getCacheName();

      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "compThreadID=%d handling request for AOT cache %s method list",
            getCompThreadId(), aotCacheName.c_str());
         }

      auto aotCacheMap = compInfo->getJITServerAOTCacheMap();

      // If the cache exists but is not loaded, get() will set
      // pending to True and return NULL for aotCache;
      // we treat this NULL as a failure like any other NULL
      bool pending = false;
      auto aotCache = aotCacheMap->get(aotCacheName, 0, pending);

      std::vector<std::string> methodSignaturesV;
      if (aotCache)
         {
         auto cachedMethodMonitor = aotCache->getCachedMethodMonitor();
         try
            {
               {
               OMR::CriticalSection cs(cachedMethodMonitor);

               auto cachedAOTMethod = aotCache->getCachedMethodHead();
               methodSignaturesV.reserve(aotCache->getCachedMethodMap().size());

               for (;cachedAOTMethod != NULL;
                  cachedAOTMethod = cachedAOTMethod->getNextRecord())
                  {
                  const SerializedAOTMethod &serializedAOTMethod = cachedAOTMethod->data();
                  methodSignaturesV.push_back(std::string(serializedAOTMethod.signature()));
                  }
               }
            }
         catch (const std::bad_alloc &e)
            {
            if (TR::Options::isAnyVerboseOptionSet(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE,
                  "std::bad_alloc: %s",
                  e.what());
            }

         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                                          "Sending the list of AOT methods size %d",
                                          methodSignaturesV.size());
            }
         }
      else // Failed getting aotCache, treat pending as a failure
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Failed getting aotCache");
            }
         }
      stream->write(JITServer::MessageType::AOTCacheMap_reply, methodSignaturesV);

      abortCompilation = true;
      deleteStream = true;
      }

   // Acquire VM access
   //
   acquireVMAccessNoSuspend(compThread);

   if (abortCompilation)
      {
      if (clientOptions)
         {
         deleteClientOptions(getClientData()->persistentMemory());
         }

      if (_recompilationMethodInfo)
         {
         getClientData()->persistentMemory()->freePersistentMemory(_recompilationMethodInfo);
         _recompilationMethodInfo = NULL;
         }

      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         {
         if (getClientData())
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d did an early abort for clientUID=%llu seqNo=%u",
               getCompThreadId(), getClientData()->getClientUID(), getSeqNo());
            Trc_JITServerEarlyAbortClientData(compThread, getCompThreadId(), (unsigned long long)getClientData()->getClientUID(), getSeqNo());
            }
         else
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d did an early abort",
               getCompThreadId());
            Trc_JITServerEarlyAbort(compThread, getCompThreadId());
            }
         }

      compInfo->acquireCompMonitor(compThread);
      releaseVMAccess(compThread);
      compInfo->decreaseQueueWeightBy(entry._weight);

      // Put the request back into the pool
      setMethodBeingCompiled(NULL); // Must have the compQmonitor

      exitPerClientAllocationRegion();

      if (optPlan)
         {
         TR_OptimizationPlan::freeOptimizationPlan(optPlan);
         }

      if (!compInfo->getPersistentInfo()->getDisableFurtherCompilation()
          && !deleteStream
          && !enableJITServerPerCompConn)
         {
         compInfo->requeueOutOfProcessEntry(&entry);
         }
      else
         {
         // Delete server stream if per compilation connections are enabled
         // or if the server disabled compilations due to a crash
         stream->~ServerStream();
         TR::Compiler->persistentGlobalAllocator().deallocate(stream);
         entry._stream = NULL;
         compInfo->recycleCompilationEntry(&entry);
         }

      // Reset the pointer to the cached client session data
      if (getClientData())
         {
         if (hasIncNumActiveThreads)
            {
            getClientData()->getSequencingMonitor()->enter();
            getClientData()->decNumActiveThreads();
            getClientData()->getSequencingMonitor()->exit();
            }

         getClientData()->decInUse();  // We have the compilation monitor so it's safe to access the inUse counter
         if (getClientData()->getInUse() == 0)
            {
            bool result = compInfo->getClientSessionHT()->deleteClientSession(clientId, false);
            if (result)
               {
               if (TR::Options::getVerboseOption(TR_VerboseJITServer))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,"client (%llu) deleted", (unsigned long long)clientId);
               }
            }
         setClientData(NULL);
         }
      return;
      }

   // Do the hack for newInstance thunks
   // Also make the method appear as interpreted, otherwise we might want to access recompilation info
   // JITServer TODO: is this ever executed for JITServer?
   //if (entry.getMethodDetails().isNewInstanceThunk())
   //   {
   //   J9::NewInstanceThunkDetails &newInstanceDetails = static_cast<J9::NewInstanceThunkDetails &>(entry.getMethodDetails());
   //   J9Class  *classForNewInstance = newInstanceDetails.classNeedingThunk();
   //   TR::CompilationInfo::setJ9MethodExtra(ramMethod, (uintptr_t)classForNewInstance | J9_STARTPC_NOT_TRANSLATED);
   //   }

#ifdef STATS
   statQueueSize.update(compInfo->getMethodQueueSize());
#endif
   void *startPC = NULL;
   // The following block of code will return with compilation monitor in hand
   if (aotCacheHit)
      {
      compInfo->acquireCompMonitor(compThread);
      stream->setClientData(NULL);
      }
   else
      {
      stream->setClientData(clientSession);
      getClientData()->readAcquireClassUnloadRWMutex(this);

      startPC = compile(compThread, &entry, scratchSegmentProvider);

      getClientData()->readReleaseClassUnloadRWMutex(this);
      stream->setClientData(NULL);
      }

   deleteClientOptions(getClientData()->persistentMemory());

   // Notify any threads waiting on this entry's monitor
   // The only thread that should be holding a monitor should be the JitDump thread
   entry.getMonitor()->notifyAll();
   // Release the queue slot monitor because we don't need it anymore
   // This will allow us to acquire the sequencing monitor later
   entry.releaseSlotMonitor(compThread);

   entry._newStartPC = startPC;
   // Update statistics regarding the compilation status (including compilationOK)
   compInfo->updateCompilationErrorStats((TR_CompilationErrorCode)entry._compErrCode);

   // Save the pointer to the plan before recycling the entry
   // Decrease the queue weight
   compInfo->decreaseQueueWeightBy(entry._weight);

   exitPerClientAllocationRegion();

   TR_OptimizationPlan::freeOptimizationPlan(optPlan); // we no longer need the optimization plan

   // Put the request back into the pool
   setMethodBeingCompiled(NULL);

   if (!compInfo->getPersistentInfo()->getDisableFurtherCompilation()
       && !enableJITServerPerCompConn
       && entry._compErrCode != compilationStreamFailure)
      {
      compInfo->requeueOutOfProcessEntry(&entry);
      }
   else
      {
      TR_ASSERT(entry._stream, "stream should still exist after compilation even if it encounters a streamFailure.");
      // Delete server stream if per compilation connections are enabled,
      // if the server disabled compilations due to a crash,
      // or if the stream failed
      stream->~ServerStream();
      TR::Compiler->persistentGlobalAllocator().deallocate(stream);
      entry._stream = NULL;
      compInfo->recycleCompilationEntry(&entry);
      }

   compInfo->printQueue();

   // Decrement number of active threads before _inUse, but we
   // need to acquire the sequencing monitor when accessing numActiveThreads
   getClientData()->getSequencingMonitor()->enter();
   getClientData()->decNumActiveThreads();
   getClientData()->getSequencingMonitor()->exit();
   getClientData()->decInUse();  // We have the compMonitor so it's safe to access the inUse counter
   if (getClientData()->getInUse() == 0)
      {
      bool deleted = compInfo->getClientSessionHT()->deleteClientSession(clientId, false);
      if (deleted && TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,"client (%llu) deleted", (unsigned long long)clientId);
      }

   setClientData(NULL); // Reset the pointer to the cached client session data

   // At this point we should always have VMAccess
   // We should always have the compilation monitor
   // we should never have classUnloadMonitor

   // Release VM access
   //
   compInfo->debugPrint(compThread, "\tcompilation thread releasing VM access\n");
   releaseVMAccess(compThread);
   compInfo->debugPrint(compThread, "-VMacc\n");

   // We can suspend this thread if too many are active
   if (
      !isDiagnosticThread() // Must not be reserved for log
      && compInfo->getNumCompThreadsActive() > 1 // we should have at least one active besides this one
      && compilationThreadIsActive() // We haven't already been signaled to suspend or terminate
      && (compInfo->getRampDownMCT() || compInfo->getSuspendThreadDueToLowPhysicalMemory())
      )
      {
      // Suspend this thread
      setCompilationThreadState(COMPTHREAD_SIGNAL_SUSPEND);
      compInfo->decNumCompThreadsActive();
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreads))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "t=%6u Suspend compThread %d Qweight=%d active=%d %s %s",
            (uint32_t)compInfo->getPersistentInfo()->getElapsedTime(),
            getCompThreadId(),
            compInfo->getQueueWeight(),
            compInfo->getNumCompThreadsActive(),
            compInfo->getRampDownMCT() ? "RampDownMCT" : "",
            compInfo->getSuspendThreadDueToLowPhysicalMemory() ? "LowPhysicalMem" : "");
         }
      // If the other remaining active thread(s) are sleeping (maybe because
      // we wanted to avoid two concurrent hot requests) we need to wake them
      // now as a preventive measure. Worst case scenario they will go back to sleep
      if (compInfo->getNumCompThreadsJobless() > 0)
         {
         compInfo->getCompilationMonitor()->notifyAll();
         if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreads))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "t=%6u compThread %d notifying other sleeping comp threads. Jobless=%d",
               (uint32_t)compInfo->getPersistentInfo()->getElapsedTime(),
               getCompThreadId(),
               compInfo->getNumCompThreadsJobless());
            }
         }
      }
   else // We will not suspend this thread
      {
      // If the low memory flag was set but there was no additional comp thread
      // to suspend, we must clear the flag now
      if (compInfo->getSuspendThreadDueToLowPhysicalMemory() &&
         compInfo->getNumCompThreadsActive() < 2)
         compInfo->setSuspendThreadDueToLowPhysicalMemory(false);
      }
   }

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
 * @param isUnresolvedInCP The unresolvedInCP boolean value of interest
 * @param methodInfo Additional method info about the resolved method of interest
 * @return returns void
 */
void
TR::CompilationInfoPerThreadRemote::cacheResolvedMethod(TR_ResolvedMethodKey key,
                                                        TR_OpaqueMethodBlock *method,
                                                        uint32_t vTableSlot,
                                                        const TR_ResolvedJ9JITServerMethodInfo
                                                            &methodInfo,
                                                        bool isUnresolvedInCP,
                                                        int32_t ttlForUnresolved)
   {
   static bool useCaching = !feGetEnv("TR_DisableResolvedMethodsCaching");
   if (!useCaching)
      return;
   // Create a new TR_ResolvedJ9JITServerMethodInfo using scratch memory

   TR_ASSERT_FATAL(getCompilation(), "Must be in compilation when calling cacheResolvedMethod\n");
   TR_Memory *trMemory = getCompilation()->trMemory();

   TR_PersistentJittedBodyInfo *bodyInfo = NULL;
   if (!std::get<1>(methodInfo).empty())
      {
      bodyInfo = (TR_PersistentJittedBodyInfo*) trMemory->allocateHeapMemory(sizeof(TR_PersistentJittedBodyInfo), TR_MemoryBase::Recompilation);
      memcpy(bodyInfo, std::get<1>(methodInfo).data(), sizeof(TR_PersistentJittedBodyInfo));
      }
   TR_PersistentMethodInfo *pMethodInfo =  NULL;
   if (!std::get<2>(methodInfo).empty())
      {
      pMethodInfo = (TR_PersistentMethodInfo*) trMemory->allocateHeapMemory(sizeof(TR_PersistentMethodInfo), TR_MemoryBase::Recompilation);
      memcpy(pMethodInfo, std::get<2>(methodInfo).data(), sizeof(TR_PersistentMethodInfo));
      }
   TR_ContiguousIPMethodHashTableEntry *entry = NULL;
   if (!std::get<3>(methodInfo).empty())
      {
      entry = (TR_ContiguousIPMethodHashTableEntry*) trMemory->allocateHeapMemory(sizeof(TR_ContiguousIPMethodHashTableEntry), TR_MemoryBase::Recompilation);
      memcpy(entry, std::get<3>(methodInfo).data(), sizeof(TR_ContiguousIPMethodHashTableEntry));
      }

   TR_ResolvedMethodCacheEntry cacheEntry;
   cacheEntry.method = method;
   cacheEntry.vTableSlot = vTableSlot;
   cacheEntry.methodInfoStruct = std::get<0>(methodInfo);
   cacheEntry.persistentBodyInfo = bodyInfo;
   cacheEntry.persistentMethodInfo = pMethodInfo;
   cacheEntry.IPMethodInfo = entry;
   cacheEntry.isUnresolvedInCP = isUnresolvedInCP;

   // time-to-live for cached unresolved methods.
   // Irrelevant for resolved methods.
   cacheEntry.ttlForUnresolved = ttlForUnresolved;

   cacheToPerCompilationMap(_resolvedMethodInfoMap, key, cacheEntry);
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
TR::CompilationInfoPerThreadRemote::getCachedResolvedMethod(TR_ResolvedMethodKey key, TR_ResolvedJ9JITServerMethod *owningMethod,
                                                            TR_ResolvedMethod **resolvedMethod, bool *unresolvedInCP)
   {
   TR_ResolvedMethodCacheEntry methodCacheEntry = {0};

   *resolvedMethod = NULL;

   if (unresolvedInCP)
      *unresolvedInCP = true;

   if (getCachedValueFromPerCompilationMap(_resolvedMethodInfoMap, key, methodCacheEntry))
      {
      auto comp = getCompilation();
      TR_OpaqueMethodBlock *method = methodCacheEntry.method;

      // if remoteMirror == NULL means we have cached an unresolved method;
      // purge the cached entry if time-to-live has expired.
      if (!methodCacheEntry.methodInfoStruct.remoteMirror)
         {
         // decrement time-to-live of this unresolved entry
         methodCacheEntry.ttlForUnresolved--;

         if (methodCacheEntry.ttlForUnresolved <= 0)
            _resolvedMethodInfoMap->erase(key);
         return true;
         }

      uint32_t vTableSlot = methodCacheEntry.vTableSlot;
      auto methodInfoStruct = methodCacheEntry.methodInfoStruct;
      TR_ResolvedJ9JITServerMethodInfo methodInfo = make_tuple(methodInfoStruct,
         methodCacheEntry.persistentBodyInfo ? std::string((const char*)methodCacheEntry.persistentBodyInfo, sizeof(TR_PersistentJittedBodyInfo)) : std::string(),
         methodCacheEntry.persistentMethodInfo ? std::string((const char*)methodCacheEntry.persistentMethodInfo, sizeof(TR_PersistentMethodInfo)) : std::string(),
         methodCacheEntry.IPMethodInfo ? std::string((const char*)methodCacheEntry.IPMethodInfo, sizeof(TR_ContiguousIPMethodHashTableEntry)) : std::string());
      // Re-add validation record
      if (comp->compileRelocatableCode() && comp->getOption(TR_UseSymbolValidationManager) && !comp->getSymbolValidationManager()->inHeuristicRegion())
         {
         if(!owningMethod->addValidationRecordForCachedResolvedMethod(key, method))
            return true; // Could not add a validation record
         }

      // Create resolved method from cached method info
      if (key.type != TR_ResolvedMethodType::VirtualFromOffset)
         {
         *resolvedMethod = owningMethod->createResolvedMethodFromJ9Method(
                              comp,
                              key.cpIndex,
                              vTableSlot,
                              (J9Method *) method,
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
            *unresolvedInCP = methodCacheEntry.isUnresolvedInCP;
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

/**
 * @brief Method executed by JITServer to clear cache for per compilation
 */
void
TR::CompilationInfoPerThreadRemote::clearPerCompilationCaches()
   {
   clearPerCompilationCache(_methodIPDataPerComp);
   clearPerCompilationCache(_resolvedMethodInfoMap);
   clearPerCompilationCache(_resolvedMirrorMethodsPersistIPInfo);
   clearPerCompilationCache(_classOfStaticMap);
   clearPerCompilationCache(_fieldAttributesCache);
   clearPerCompilationCache(_staticAttributesCache);
   clearPerCompilationCache(_isUnresolvedStrCache);
   }

/**
 * @brief Method executed by JITServer to delete client session data when client stream is terminated
 */
void
TR::CompilationInfoPerThreadRemote::deleteClientSessionData(uint64_t clientId, TR::CompilationInfo* compInfo, J9VMThread* compThread)
   {
   // Need to acquire compilation monitor for both deleting the client data and the setting the thread state to COMPTHREAD_SIGNAL_SUSPEND.
   compInfo->acquireCompMonitor(compThread);
   bool result = compInfo->getClientSessionHT()->deleteClientSession(clientId, true);
   if (TR::Options::getVerboseOption(TR_VerboseJITServer) ||
       TR::Options::getVerboseOption(TR_VerboseJITServerConns))
      {
      uint32_t timestamp = (uint32_t) compInfo->getPersistentInfo()->getElapsedTime();
      if (!result)
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,"t=%6u Client (clientUID=%llu) disconnected. Client session not deleted", timestamp, (unsigned long long)clientId);
         }
      else
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,"t=%6u Client (clientUID=%llu) disconnected. Client session deleted", timestamp, (unsigned long long)clientId);
         }
      }

   compInfo->releaseCompMonitor(compThread);
   }

void
TR::CompilationInfoPerThreadRemote::freeAllResources()
   {
   if (_recompilationMethodInfo)
      {
      TR_Memory::jitPersistentFree(_recompilationMethodInfo);
      _recompilationMethodInfo = NULL;
      }

   TR::CompilationInfoPerThread::freeAllResources();
   }
