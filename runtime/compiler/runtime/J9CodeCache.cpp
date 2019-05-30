/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifdef J9ZTPF
#define __TPF_DO_NOT_MAP_ATOE_REMOVE
#endif

#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include "j9.h"
#include "j9protos.h"
#include "j9thread.h"
#include "jitprotos.h"
#include "j9cp.h"
#define J9_EXTERNAL_TO_VM
#include "runtime/J9VMAccess.hpp"
#include "vmaccess.h"
#include "infra/Monitor.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "codegen/FrontEnd.hpp"
#ifdef CODECACHE_STATS
#include "infra/Statistics.hpp"
#endif
#include "env/CompilerEnv.hpp"
#include "env/ut_j9jit.h"
#include "infra/CriticalSection.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/CodeCacheMemorySegment.hpp"
#include "env/VMJ9.h"
#include "runtime/ArtifactManager.hpp"
#include "env/IO.hpp"
#include "runtime/HookHelpers.hpp"

OMR::CodeCacheMethodHeader *getCodeCacheMethodHeader(char *p, int searchLimit, J9JITExceptionTable * metaData);

#define addFreeBlock2(start, end) addFreeBlock2WithCallSite((start), (end), __FILE__, __LINE__)


TR::CodeCache *
J9::CodeCache::self()
   {
   return static_cast<TR::CodeCache *>(this);
   }

TR::CodeCacheMemorySegment*
J9::CodeCache::trj9segment()
   {
   return self()->segment();
   }

J9MemorySegment *
J9::CodeCache::j9segment()
   {
   return self()->trj9segment()->j9segment();
   }


TR::CodeCache *
J9::CodeCache::allocate(TR::CodeCacheManager *cacheManager, size_t segmentSize, int32_t reservingCompThreadID)
   {
   TR::CodeCache *newCodeCache = OMR::CodeCache::allocate(cacheManager, segmentSize, reservingCompThreadID);
   if (newCodeCache != NULL)
      {
      // Generate a trace point into the Snap file
      Trc_JIT_CodeCacheAllocated(newCodeCache, newCodeCache->getCodeBase(), newCodeCache->getCodeTop());
      }

   return newCodeCache;
   }


bool
J9::CodeCache::initialize(TR::CodeCacheManager *manager,
                          TR::CodeCacheMemorySegment *codeCacheSegment,
                          size_t allocatedCodeCacheSizeInBytes)
   {
   // make J9 memory segment look all used up
   //J9MemorySegment *j9segment = _segment->segment();
   //j9segment->heapAlloc = j9segment->heapTop;

   TR::CodeCacheConfig & config = manager->codeCacheConfig();
   if (config.needsMethodTrampolines())
      {
      int32_t percentageToUse;
      if (!(TR::Options::getCmdLineOptions()->getTrampolineSpacePercentage() > 0))
         {
#if defined(TR_HOST_X86) && defined(TR_HOST_64BIT)
         percentageToUse = 7;
#else
         percentageToUse = 4;
#endif
         // The number of helpers and the trampoline size are both factors here
         size_t trampolineSpaceSize = config.trampolineCodeSize() * config.numRuntimeHelpers();
         if (trampolineSpaceSize >= 3400)
            {
            // This will be PPC64, AMD64 and 390
            if (config.codeCacheKB() < 512 && config.codeCacheKB() > 256)
               percentageToUse = 5;
            else if (config.codeCacheKB() <= 256)
               percentageToUse = 6;
            }
         }
      else
         {
         percentageToUse = TR::Options::getCmdLineOptions()->getTrampolineSpacePercentage();
         }

      config._trampolineSpacePercentage = percentageToUse;
      }

   if (!self()->OMR::CodeCache::initialize(manager, codeCacheSegment, allocatedCodeCacheSizeInBytes))
      return false;
   self()->setInitialAllocationPointers();

   _manager->reportCodeLoadEvents();

   return true;
   }


// Deal with class unloading
void
J9::CodeCache::onClassUnloading(J9ClassLoader *loaderPtr)
   {
   OMR::CodeCacheHashEntry *entry, *prev, *next;
   int32_t idx;

   for (idx=0; idx<_resolvedMethodHT->_size; idx++)
      {
      prev = NULL;
      entry = _resolvedMethodHT->_buckets[idx];
      while (entry != NULL)
         {
         next = entry->_next;
         if (J9_CLASS_FROM_METHOD((J9Method *)(entry->_info._resolved._method))->classLoader == loaderPtr)
            {
            if (prev == NULL)
               _resolvedMethodHT->_buckets[idx] = next;
            else
               prev->_next = next;
            self()->freeHashEntry(entry);
            }
         else
            {
            prev = entry;
            }
         entry = next;
         }
      }

   for (idx=0; idx<_unresolvedMethodHT->_size; idx++)
      {
      prev = NULL;
      entry = _unresolvedMethodHT->_buckets[idx];
      while (entry != NULL)
         {
         next = entry->_next;
         if (J9_CLASS_FROM_CP((J9ConstantPool *)(entry->_info._unresolved._constPool))->classLoader == loaderPtr)
            {
            if (prev == NULL)
               _unresolvedMethodHT->_buckets[idx] = next;
            else
               prev->_next = next;
            self()->freeHashEntry(entry);
            }
         else
            {
            prev = entry;
            }
         entry = next;
         }
      }
   }


void
J9::CodeCache::onClassRedefinition(TR_OpaqueMethodBlock *oldMethod, TR_OpaqueMethodBlock *newMethod)
   {
   OMR::CodeCacheHashEntry *entry = _resolvedMethodHT->findResolvedMethod(oldMethod);
   if (!entry)
      return;
   _resolvedMethodHT->remove(entry);
   entry->_key = _resolvedMethodHT->hashResolvedMethod(newMethod);
   entry->_info._resolved._method = newMethod;
   entry->_info._resolved._currentStartPC = NULL;
   _resolvedMethodHT->add(entry);

   // scope for artifact manager critical section
      {
      TR_TranslationArtifactManager::CriticalSection artifactManager;
      // Test for anonymous class
      J9Class *j9clazz = J9_CLASS_FROM_METHOD((J9Method *)newMethod);
      if (_manager->fej9()->isAnonymousClass((TR_OpaqueClassBlock*)j9clazz))
         j9clazz->classFlags |= J9ClassContainsMethodsPresentInMCCHash;
      else
         j9clazz->classLoader->flags |= J9CLASSLOADER_CONTAINS_METHODS_PRESENT_IN_MCC_HASH;
      }
   }


OMR::CodeCacheHashEntry *
J9::CodeCache::findUnresolvedMethod(void *constPool, int32_t constPoolIndex)
   {
   return _unresolvedMethodHT->findUnresolvedMethod(constPool, constPoolIndex);
   }


bool
J9::CodeCache::addUnresolvedMethod(void *constPool, int32_t constPoolIndex)
   {
   OMR::CodeCacheHashEntry *entry = self()->allocateHashEntry();

   if (!entry)
      return false;

   entry->_key = _unresolvedMethodHT->hashUnresolvedMethod(constPool, constPoolIndex);
   entry->_info._unresolved._constPool = constPool;
   entry->_info._unresolved._constPoolIndex = constPoolIndex;
   _unresolvedMethodHT->add(entry);

   // scope for artifact manager critical section
      {
      TR_TranslationArtifactManager::CriticalSection artifactMgr;
      // Test for anonymous class
      J9Class *j9clazz = J9_CLASS_FROM_CP(constPool);
      if (_manager->fej9()->isAnonymousClass((TR_OpaqueClassBlock*)j9clazz))
         j9clazz->classFlags |= J9ClassContainsMethodsPresentInMCCHash;
      else
         j9clazz->classLoader->flags |= J9CLASSLOADER_CONTAINS_METHODS_PRESENT_IN_MCC_HASH;
      }

   return true;
   }


bool
J9::CodeCache::addResolvedMethod(TR_OpaqueMethodBlock *method)
   {
   if (!self()->OMR::CodeCache::addResolvedMethod(method))
      return false;

   // scope for artifact manager critical section
      {
      TR_TranslationArtifactManager::CriticalSection artifactMgr;
      // Test for anonymous class
      J9Class *j9clazz = J9_CLASS_FROM_METHOD(reinterpret_cast<J9Method *>(method));
      if (_manager->fej9()->isAnonymousClass((TR_OpaqueClassBlock*)j9clazz))
         j9clazz->classFlags |= J9ClassContainsMethodsPresentInMCCHash;
      else
         j9clazz->classLoader->flags |= J9CLASSLOADER_CONTAINS_METHODS_PRESENT_IN_MCC_HASH;
      }

   return true;
   }


// Change an unresolved method in the hash table to a resolved method.
//
void
J9::CodeCache::resolveHashEntry(OMR::CodeCacheHashEntry *entry, TR_OpaqueMethodBlock *method)
   {
   // extract the entry from the unresolved hash table
   if (!_unresolvedMethodHT->remove(entry))
      {
      //suspicious: should any asserts actually happen? why is this commented out?
      /////TR_ASSERT(0);     // internal inconsistency, should never happen
      }

   entry->_key = _resolvedMethodHT->hashResolvedMethod(method);
   entry->_info._resolved._method = method;
   entry->_info._resolved._currentStartPC = NULL;
   entry->_info._resolved._currentTrampoline = NULL;

   // insert the entry into the resolved table without any internal allocations
   _resolvedMethodHT->add(entry);

   // scope for artifact manager critical section
      {
      TR_TranslationArtifactManager::CriticalSection artifactMgr;
      // Test for anonymous class
      J9Class *j9clazz = J9_CLASS_FROM_METHOD(reinterpret_cast<J9Method *>(method));
     if (_manager->fej9()->isAnonymousClass((TR_OpaqueClassBlock*)j9clazz))
         j9clazz->classFlags |= J9ClassContainsMethodsPresentInMCCHash;
      else
         j9clazz->classLoader->flags |= J9CLASSLOADER_CONTAINS_METHODS_PRESENT_IN_MCC_HASH;
      }
   }


// May add the faint cache block to _freeBlockList.
// Caller should expect that block may sometimes not be added.
void
J9::CodeCache::addFreeBlock(OMR::FaintCacheBlock *block)
   {
   J9JITExceptionTable  *metaData = block->_metaData;
   OMR::CodeCacheMethodHeader *warmBlock = getCodeCacheMethodHeader((char *)metaData->startPC, 32, metaData);

   UDATA realStartPC = metaData->startPC + block->_bytesToSaveAtStart;

   // Update the metaData end field accordingly
   metaData->endPC = (UDATA) realStartPC;

   TR::CodeCacheConfig & config = _manager->codeCacheConfig();
   size_t round = (size_t) config.codeCacheAlignment() - 1;
   realStartPC = (realStartPC + round) & ~round;
   size_t endPtr = (size_t) warmBlock+warmBlock->_size;
   if (endPtr > realStartPC+sizeof(OMR::CodeCacheFreeCacheBlock))
       warmBlock->_size = realStartPC - (UDATA)warmBlock;

   if (self()->addFreeBlock2((uint8_t *) realStartPC, (uint8_t *)endPtr))
      {
      // Update the block size to reflect the remaining stub
      warmBlock->_size = realStartPC - (UDATA)warmBlock;
      }
   else
      {
      // See CMVC 145246: Difficult to see how this situation arises, but has been confirmed in tests.
      // Possible race condition, where the warmBlock sans stub has already been freed?
      //
      //     warmBlock->size
      //            |
      //     |<----------->|
      //     +-------------+                +-----------
      //     |             |\\\\\\\\\\\\\\\\.
      //     +-------------+                +-----------
      //     |             |                |
      // warmBlock     warmBlock +     realStartPC
      //             warmBlock->size
      }

   // Tiered Code Cache
   if (metaData->startColdPC)
      {
      // startPC of the cold code is preceded by OMR::CodeCacheMethodHeader
      OMR::CodeCacheMethodHeader *coldBlock = (OMR::CodeCacheMethodHeader*)(metaData->startColdPC-sizeof(OMR::CodeCacheMethodHeader));
      if (self()->addFreeBlock2((uint8_t *)coldBlock, (uint8_t *)coldBlock+coldBlock->_size))
         {}
      }
      // Update the metaData fields accordingly
      metaData->endWarmPC = metaData->endPC;
      metaData->startColdPC = 0;
   }


// May add the block defined in metaData to freeBlockList.
// Caller should expect that block may sometimes not be added.
OMR::CodeCacheMethodHeader *
J9::CodeCache::addFreeBlock(void  *voidMetaData)
   {
   J9JITExceptionTable *metaData = static_cast<J9JITExceptionTable *>(voidMetaData);
   // The entire method body including the method header will be removed
   OMR::CodeCacheMethodHeader *warmBlock = getCodeCacheMethodHeader((char *)metaData->startPC, 32, metaData);
   TR::CodeCacheConfig & config = _manager->codeCacheConfig();
   if (warmBlock)
      {
      if (config.verboseReclamation())
         {
         TR_J9VMBase *fe = _manager->fej9();
         TR_VerboseLog::writeLineLocked(TR_Vlog_CODECACHE,"CC=%p unloading j9method=%p metaData=%p warmBlock=%p size=%d: %.*s.%.*s%.*s", this, metaData->ramMethod,metaData, warmBlock, (int)warmBlock->_size, J9UTF8_LENGTH(metaData->className), J9UTF8_DATA(metaData->className), J9UTF8_LENGTH(metaData->methodName), J9UTF8_DATA(metaData->methodName), J9UTF8_LENGTH(metaData->methodSignature), J9UTF8_DATA(metaData->methodSignature));
         }

      // When entire method is removed we can remove the jittedBodyInfo as well
      if (metaData->bodyInfo &&
          !TR::Options::getCmdLineOptions()->getOption(TR_EnableHCR) &&
          !TR::Options::getCmdLineOptions()->getOption(TR_FullSpeedDebug))
         {
         TR_PersistentJittedBodyInfo *bi = (TR_PersistentJittedBodyInfo*)metaData->bodyInfo;
         if (!bi->getIsAotedBody()) // AOTed methods allocate the bodyInfo in the dataCache with allocateException
            {
            // This code is used only for class unloading (for recompilation we call
            // addFreeBlock with  FaintCacheBlock); thus we can reclaim the persistentMethodInfo as well
            //
            // Memorize persistentMethodInfo before freeing jittedBodyInfo
            TR_PersistentMethodInfo * pmi = bi->getMethodInfo();

            // Now we can free the bodyInfo
            // Don't free bodyInfo if it is likely to be in the DataCache
            // (IsAotedBody==false when addFreeBlock is called during compilation)
            if (!pmi || !pmi->isInDataCache())
               {
               TR_Memory::jitPersistentFree(bi);
               // If we free bodyInfo, we need to also free metaData->bodyInfo->mapTable by calling freeFastWalkCache()
               J9VMThread *currentVMThread = _manager->javaVM()->internalVMFunctions->currentVMThread(_manager->javaVM());
               freeFastWalkCache(currentVMThread, metaData);
               metaData->bodyInfo = NULL;
               }

            // Attempt to free the persistentMethodInfo
            if (pmi && !pmi->isInDataCache())
               {
               // There could be several bodyInfo pointing to the same methodInfo
               // Prevent deallocating twice by freeing only for the last body
               if (TR::Compiler->mtd.startPC((TR_OpaqueMethodBlock*)metaData->ramMethod) == (uintptr_t)metaData->startPC)
                  {
                  // Clear profile info
                  pmi->setBestProfileInfo(NULL);
                  pmi->setRecentProfileInfo(NULL);

                  if (TR::Options::getVerboseOption(TR_VerboseReclamation))
                     {
                        TR_VerboseLog::writeLineLocked(TR_Vlog_RECLAMATION, "Reclaiming PersistentMethodInfo 0x%p.", pmi);
                     }
                  TR_Memory::jitPersistentFree(pmi);
                  }
               }
            }
         }
      }

   if (self()->addFreeBlock2((uint8_t *)warmBlock, (uint8_t *)((UDATA)warmBlock+warmBlock->_size)))
      {}

   if (metaData->startColdPC)
      {
      // startPC of the cold code is preceded by OMR::CodeCacheMethodHeader
      OMR::CodeCacheMethodHeader *coldBlock = (OMR::CodeCacheMethodHeader*)((UDATA)metaData->startColdPC-sizeof(OMR::CodeCacheMethodHeader));
      if (self()->addFreeBlock2((uint8_t *)coldBlock, (uint8_t *)coldBlock+coldBlock->_size))
         {}
      }

   return warmBlock;
   }


void
J9::CodeCache::dumpCodeCache()
   {
   self()->OMR::CodeCache::dumpCodeCache();
   printf("  |-- segment                = 0x%p\n", _segment );
   printf("  |-- segment->heapBase      = 0x%08x\n", _segment->segmentBase() );
   printf("  |-- segment->heapTop       = 0x%08x\n", _segment->segmentTop() );
   }

#define HELPER_TRAMPOLINE_AREA_NAME "JIT helper trampoline area"
#define METHOD_TRAMPOLINE_AREA_NAME "JIT method trampoline area"
#define PRELOADED_CODE_AREA_NAME    "JIT code cache pre loaded code area"

void
J9::CodeCache::reportCodeLoadEvents()
   {
   J9JavaVM *javaVM = _manager->javaVM();

   if (!J9_EVENT_IS_HOOKED(javaVM->hookInterface, J9HOOK_VM_DYNAMIC_CODE_LOAD))
      return;

   J9VMThread *currentThread = javaVM->internalVMFunctions->currentVMThread(javaVM);

   _flags |= OMR::CODECACHE_TRAMP_REPORTED;
   _flags |= OMR::CODECACHE_CCPRELOADED_REPORTED;

   UDATA helperTrampolinesSize = (UDATA) _helperTop - (UDATA) _helperBase;
   if (helperTrampolinesSize > 0)
      ALWAYS_TRIGGER_J9HOOK_VM_DYNAMIC_CODE_LOAD(javaVM->hookInterface,
                                                 currentThread,
                                                 NULL,
                                                 (void *) _helperBase,
                                                 helperTrampolinesSize,
                                                 HELPER_TRAMPOLINE_AREA_NAME,
                                                 NULL);

   UDATA methodTrampolinesSize = (UDATA) _helperBase - (UDATA) _trampolineBase;
   if (methodTrampolinesSize > 0)
      ALWAYS_TRIGGER_J9HOOK_VM_DYNAMIC_CODE_LOAD(javaVM->hookInterface,
                                                 currentThread,
                                                 NULL,
                                                 (void *) _trampolineBase,
                                                 methodTrampolinesSize,
                                                 METHOD_TRAMPOLINE_AREA_NAME,
                                                 NULL);

   UDATA preLoadedCodeSize = (UDATA) _trampolineBase - (UDATA) _CCPreLoadedCodeBase;
   if (preLoadedCodeSize > 0)
      ALWAYS_TRIGGER_J9HOOK_VM_DYNAMIC_CODE_LOAD(javaVM->hookInterface,
                                                 currentThread,
                                                 NULL,
                                                 (void *) _CCPreLoadedCodeBase,
                                                 preLoadedCodeSize,
                                                 PRELOADED_CODE_AREA_NAME,
                                                 NULL);
   }

void
J9::CodeCache::generatePerfToolEntries(TR::FILE *file)
   {
#ifdef LINUX
   if (!file)
      return;

   UDATA helperTrampolinesSize = (UDATA) _helperTop - (UDATA) _helperBase;
   if (helperTrampolinesSize > 0)
         j9jit_fprintf(file, "%p %lX %s\n", _helperBase, helperTrampolinesSize,
                       HELPER_TRAMPOLINE_AREA_NAME);

   UDATA methodTrampolinesSize = (UDATA) _helperBase - (UDATA) _trampolineBase;
   if (methodTrampolinesSize > 0)
         j9jit_fprintf(file, "%p %lX %s\n", _trampolineBase, methodTrampolinesSize,
                       METHOD_TRAMPOLINE_AREA_NAME);

   UDATA preLoadedCodeSize = (UDATA) _trampolineBase - (UDATA) _CCPreLoadedCodeBase;
   if (preLoadedCodeSize > 0)
         j9jit_fprintf(file, "%p %lX %s\n", _CCPreLoadedCodeBase, preLoadedCodeSize,
                       PRELOADED_CODE_AREA_NAME);
#endif
   }


// Remove over-booked trampoline reservations
//
void
J9::CodeCache::adjustTrampolineReservation(TR_OpaqueMethodBlock *method,
                                           void *cp,
                                           int32_t cpIndex)
   {
   OMR::CodeCacheHashEntry *unresolvedEntry;
   OMR::CodeCacheHashEntry *resolvedEntry;

   TR::CodeCacheConfig &config = _manager->codeCacheConfig();
   if (!config.needsMethodTrampolines())
      return;

   // scope to update reservation
      {
      CacheCriticalSection updateReservation(self());

      unresolvedEntry = _unresolvedMethodHT->findUnresolvedMethod(cp, cpIndex);
      resolvedEntry   = _resolvedMethodHT->findResolvedMethod(method);

      //seems suspicious to have this assertion disabled....why is it ok to fall off without a reservation?
      //TR_ASSERT(unresolvedEntry || resolvedEntry);
      if (unresolvedEntry && resolvedEntry)
         {
         // remove 1 trampoline reservation
         self()->unreserveSpaceForTrampoline();

         // remove the entry from the unresolved hash table and release it
         if (_unresolvedMethodHT->remove(unresolvedEntry))
            self()->freeHashEntry(unresolvedEntry);
         }
      else if (unresolvedEntry && !resolvedEntry)
         {
         // Move the unresolved entry to the resolved table.
         self()->resolveHashEntry(unresolvedEntry, method);
         }
      }
   }


void
J9::CodeCache::onFSDDecompile()
   {
   self()->resetTrampolines();
   }

void
J9::CodeCache::resetTrampolines()
   {
   TR_ASSERT(_manager->codeCacheConfig().needsMethodTrampolines(), "Attempting to purge trampolines when they do not exist");

   OMR::CodeCacheHashEntry *entry, *next;
   int idx;

   // In theory, we can recycle everything in codeCaches at this point. For minimum changes, we will only
   // purge trampoline hashTable(s) and temporary trampoline syncBlocks.
   // What can be recycled further: codeCache itself, trampoline space
   for (idx = 0; idx < _resolvedMethodHT->_size; idx++)
      {
      entry = _resolvedMethodHT->_buckets[idx];
      _resolvedMethodHT->_buckets[idx] = NULL;
      while (entry)
         {
         next = entry->_next;
         self()->freeHashEntry(entry);
         entry = next;
         }
      }

   for (idx = 0; idx < _unresolvedMethodHT->_size; idx++)
      {
      entry = _unresolvedMethodHT->_buckets[idx];
      _unresolvedMethodHT->_buckets[idx] = NULL;
      while (entry)
         {
         next = entry->_next;
         self()->freeHashEntry(entry);
         entry = next;
         }
      }

   //reset the trampoline marks back to their starting positions
   _trampolineAllocationMark = _trampolineBase;
   _trampolineReservationMark = _trampolineBase;

   OMR::CodeCacheTempTrampolineSyncBlock *syncBlock;
   if (!_tempTrampolinesMax)
      return;

   _flags &= ~OMR::CODECACHE_FULL_SYNC_REQUIRED;
   for (syncBlock = self()->trampolineSyncList(); syncBlock; syncBlock = syncBlock->_next)
      syncBlock->_entryCount = 0;

   _tempTrampolineNext = _tempTrampolineBase;
   }



//------------------------------ reserveUnresolvedTrampoline ----------------
// Find or create a reservation for an unresolved method trampoline.
// Method must be called with VM access in hand to prevent unloading
// Returns 0 on success or a negative error code on failure
//---------------------------------------------------------------------------
int32_t
J9::CodeCache::reserveUnresolvedTrampoline(void *cp, int32_t cpIndex)
   {
   int32_t retValue = OMR::CodeCacheErrorCode::ERRORCODE_SUCCESS; // assume success

   // If the platform does not need trampolines, return success
   TR::CodeCacheConfig &config = _manager->codeCacheConfig();
   if (!config.needsMethodTrampolines())
      return OMR::CodeCacheErrorCode::ERRORCODE_SUCCESS;

   // scope for cache critical section
      {
      CacheCriticalSection reserveTrampoline(self());

      // check if we already have a reservation for this name/classLoader key
      OMR::CodeCacheHashEntry *entry = _unresolvedMethodHT->findUnresolvedMethod(cp, cpIndex);
      if (!entry)
         {
         // There isn't a reservation for this particular name/classLoader, make one
         //
         retValue = self()->reserveSpaceForTrampoline_bridge();
         if (retValue == OMR::CodeCacheErrorCode::ERRORCODE_SUCCESS)
            {
            if (!self()->addUnresolvedMethod(cp, cpIndex))
               retValue = OMR::CodeCacheErrorCode::ERRORCODE_FATALERROR; // couldn't allocate memory from VM
            }
         }
      }

   return retValue;
   }



void
J9::CodeCache::setInitialAllocationPointers()
   {
   _warmCodeAllocBase = self()->getWarmCodeAlloc();
   _coldCodeAllocBase = self()->getColdCodeAlloc();
   }

void
J9::CodeCache::resetAllocationPointers()
   {
   // Compute how much memory we give back to update the free space in the repository
   size_t warmSize = self()->getWarmCodeAlloc() - _warmCodeAllocBase;
   size_t coldSize = _coldCodeAllocBase - self()->getColdCodeAlloc();
   size_t freedSpace = warmSize + coldSize;
   _manager->increaseFreeSpaceInCodeCacheRepository(freedSpace);
   self()->setWarmCodeAlloc(_warmCodeAllocBase);
   self()->setColdCodeAlloc(_coldCodeAllocBase);
   }

void
J9::CodeCache::resetCodeCache()
   {
   self()->resetAllocationPointers();
   if (_manager->codeCacheConfig().needsMethodTrampolines())
      self()->resetTrampolines();
   }


extern "C"
   {

   // **************************************************************************
   // Unwrapper functions to be called from jitCallCFunction

   void mcc_callPointPatching_unwrapper(void **argsPtr, void **resPtr)
      {
      TR::CodeCache *codeCache = TR::CodeCacheManager::instance()->findCodeCacheFromPC(argsPtr[1]);
      if (codeCache)
         codeCache->patchCallPoint(reinterpret_cast<TR_OpaqueMethodBlock *>(argsPtr[0]), argsPtr[1], argsPtr[2], argsPtr[3]);
      }

   void mcc_reservationAdjustment_unwrapper(void **argsPtr, void **resPtr)
      {
      TR::CodeCache *codeCache = TR::CodeCacheManager::instance()->findCodeCacheFromPC(argsPtr[0]);
      if (codeCache)
         codeCache->adjustTrampolineReservation(reinterpret_cast<TR_OpaqueMethodBlock *>(argsPtr[1]), argsPtr[2], static_cast<int32_t>((UDATA)argsPtr[3]));
      }

   void mcc_lookupHelperTrampoline_unwrapper(void **argsPtr, void **resPtr)
      {
      intptrj_t trampoline = TR::CodeCacheManager::instance()->findHelperTrampoline(static_cast<int32_t>((UDATA)argsPtr[1]), argsPtr[0]);
      *resPtr = reinterpret_cast<void *>(trampoline);
      }

   void mcc_reservationInterfaceCache_unwrapper(void **args, void**res)
      {
      TR::CodeCacheManager::instance()->reservationInterfaceCache(args[0], reinterpret_cast<TR_OpaqueMethodBlock *>(args[1]));
      }

   OMR::CodeCacheTrampolineCode *
   mcc_replaceTrampoline(TR_OpaqueMethodBlock *method,
                         void                 *callSite,
                         void                 *oldTrampoline,
                         void                 *oldTargetPC,
                         void                 *newTargetPC,
                         bool                 needSync)
      {
      return TR::CodeCacheManager::instance()->replaceTrampoline(method, callSite, oldTrampoline, oldTargetPC, newTargetPC, needSync);
      }

   }
