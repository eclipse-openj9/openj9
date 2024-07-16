/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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

#include <algorithm>
#include <stdlib.h>
#include <string.h>
#if defined(LINUX)
#include <sys/mman.h> // for madvise
#endif // LINUX
#include "OMR/Bytes.hpp"
#include "j9.h"
#include "j9cp.h"
#include "j9protos.h"
#include "j9thread.h"
#include "jitprotos.h"
#define J9_EXTERNAL_TO_VM
#include "vmaccess.h"
#include "control/CompilationRuntime.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/FrontEnd.hpp"
#include "env/IO.hpp"
#include "env/VerboseLog.hpp"
#include "env/VMJ9.h"
#include "env/ut_j9jit.h"
#include "infra/CriticalSection.hpp"
#include "infra/Monitor.hpp"
#include "runtime/ArtifactManager.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/CodeCacheMemorySegment.hpp"
#include "runtime/J9VMAccess.hpp"

#if defined(LINUX)
#if !defined(MADV_HUGEPAGE)
#define MADV_HUGEPAGE 14
#endif /* MADV_HUGEPAGE */
#endif /* LINUX */

TR::CodeCacheManager *J9::CodeCacheManager::_codeCacheManager = NULL;
J9JavaVM *J9::CodeCacheManager::_javaVM = NULL;
J9JITConfig *J9::CodeCacheManager::_jitConfig = NULL;

TR::CodeCacheManager *
J9::CodeCacheManager::self()
   {
   return static_cast<TR::CodeCacheManager *>(this);
   }

TR_FrontEnd *
J9::CodeCacheManager::fe()
   {
   return _fe;
   }

TR_J9VMBase *
J9::CodeCacheManager::fej9()
   {
   return (TR_J9VMBase *)(self()->fe());
   }

TR::CodeCache *
J9::CodeCacheManager::initialize(bool useConsolidatedCache, uint32_t numberOfCodeCachesToCreateAtStartup)
   {
   _jitConfig = self()->fej9()->getJ9JITConfig();
   _javaVM = _jitConfig->javaVM;

   return self()->OMR::CodeCacheManager::initialize(useConsolidatedCache, numberOfCodeCachesToCreateAtStartup);
   }

bool
J9::CodeCacheManager::isSufficientPhysicalMemoryAvailableForAllocation(size_t requestedCodeCacheSize)
   {
   TR::CodeCacheConfig &config = self()->codeCacheConfig();
   TR::CompilationInfo * compInfo = getCompilationInfo(_jitConfig);
   bool incomplete;
   // If swap memory is not available then no need to do further check
   if (!compInfo->isSwapMemoryDisabled())
      {
      return true;
      }

   // Read the amount of free physical memory currently, bypassing the caching mechanism
   uint64_t freePhysicalMemoryB = compInfo->computeAndCacheFreePhysicalMemory(incomplete, 0);
   // Avoid consuming the last drop of physical memory for JIT; leave some for emergency situations
   uint64_t safeMemReserve = (uint64_t)TR::Options::getSafeReservePhysicalMemoryValue();
   uint64_t desiredMemory = requestedCodeCacheSize + safeMemReserve;

   if (!incomplete && freePhysicalMemoryB < desiredMemory)
      {
      if (config.verboseCodeCache() || config.verbosePerformance())
         TR_VerboseLog::writeLineLocked(TR_Vlog_CODECACHE, "Warning: low physical memory detected during code cache allocation, requestedCodeCacheSize=%zu, freePhysicalMemory=%zu, safeMemReserve=%zu",
                                       requestedCodeCacheSize, (size_t)freePhysicalMemoryB, (size_t)safeMemReserve);
      return false;
      }
   else
      {
      return true;
      }
   }

void
J9::CodeCacheManager::addCodeCache(TR::CodeCache *codeCache)
   {
   self()->OMR::CodeCacheManager::addCodeCache(codeCache);

   // Do not execute the rest of the method which has to do with segments
   //if (_codeCacheRepositorySegment)
   //   return;

   // Inform the VM about the addition of the new code-cache.
   // Note that add_code_cache below will acquire exclusive vm
   // access to prevent any stack walker, etc from traversing
   // the AVL Trees while we do a modification upon them.
   // Also note that at this stage we should not really hold
   // any monitors since thats likely to deadlock.
   //
   TR::CodeCacheMemorySegment *segment = codeCache->segment();
   J9MemorySegment *j9segment = segment->j9segment();
   if (j9segment)
      {
      /* Make sure that we have VM access at this point */

      /* NOTE: the way that the VM deals with VM access will likely change in the
           future at which time this code will need to be removed and updated. */

      J9VMThread *vmThread = TR::CodeCacheManager::javaVM()->internalVMFunctions->currentVMThread(TR::CodeCacheManager::javaVM());
      bool threadHadNoVMAccess = false;
      if (vmThread)
         {
         threadHadNoVMAccess = (!(vmThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS));

         /* Acquire VM access only if we don't have it */
         if(threadHadNoVMAccess)
            acquireVMAccessNoSuspend(vmThread);
         }

      /* Add this code caches memory segment to the translation artifacts */
      jit_artifact_protected_add_code_cache(TR::CodeCacheManager::javaVM(),
                                            TR::CodeCacheManager::jitConfig()->translationArtifacts,
                                            j9segment, NULL);

      /* Release VM access only if we didn't have it before the call */
      if(threadHadNoVMAccess)
         releaseVMAccess(vmThread);
      }
   }


void
J9::CodeCacheManager::addFaintCacheBlock(OMR::MethodExceptionData  *metaData, uint8_t bytesToSaveAtStart)
   {
   PORT_ACCESS_FROM_JITCONFIG(_jitConfig);

   OMR::FaintCacheBlock *block = (OMR::FaintCacheBlock *)j9mem_allocate_memory(sizeof(OMR::FaintCacheBlock), J9MEM_CATEGORY_JIT);
   if (!block)
      return;
   block->_metaData = metaData;

   OMR::FaintCacheBlock * & faintBlockList = reinterpret_cast<OMR::FaintCacheBlock * &>(_jitConfig->methodsToDelete);
   block->_next = faintBlockList;
   block->_bytesToSaveAtStart = bytesToSaveAtStart;
   block->_isStillLive = false;
   faintBlockList = block;
   }


// May add the faint cache block to freeBlockList.
// Caller should expect that block may sometimes not be added.
void
J9::CodeCacheManager::freeFaintCacheBlock(OMR::FaintCacheBlock *block, uint8_t *startPC)
   {
   TR::CodeCache *owningCodeCache = self()->findCodeCacheFromPC(startPC);
   owningCodeCache->addFreeBlock(block);
   }


void
J9::CodeCacheManager::setCodeCacheFull()
   {
   self()->OMR::CodeCacheManager::setCodeCacheFull();
   _jitConfig->runtimeFlags |= J9JIT_CODE_CACHE_FULL;
   }


void
J9::CodeCacheManager::purgeClassLoaderFromFaintBlocks(J9ClassLoader *classLoader)
   {
   OMR::FaintCacheBlock *currentFaintBlock = static_cast<OMR::FaintCacheBlock *>(_jitConfig->methodsToDelete);
   OMR::FaintCacheBlock *previousFaintBlock = 0;
   while (currentFaintBlock)
      {
      if (classLoader == J9_CLASS_FROM_METHOD(currentFaintBlock->_metaData->ramMethod)->classLoader)
         {
         PORT_ACCESS_FROM_JITCONFIG(_jitConfig);
         if (previousFaintBlock)
            {
            previousFaintBlock->_next = currentFaintBlock->_next;
            j9mem_free_memory(currentFaintBlock);
            currentFaintBlock = previousFaintBlock->_next;
            continue;
            }
         else
            {
            _jitConfig->methodsToDelete = currentFaintBlock->_next;
            j9mem_free_memory(currentFaintBlock);
            currentFaintBlock = static_cast<OMR::FaintCacheBlock *>(_jitConfig->methodsToDelete);
            continue;
            }
         }
      previousFaintBlock = currentFaintBlock;
      currentFaintBlock = currentFaintBlock->_next;
      }
   }

// Deal with class unloading
//
void
J9::CodeCacheManager::onClassUnloading(J9ClassLoader *loaderPtr)
   {
   TR::CodeCacheConfig &config = self()->codeCacheConfig();
   if (!config.needsMethodTrampolines())
      return;

   // Don't allow hashEntry to linger somewhere else
   self()->synchronizeTrampolines();

      {
      CacheListCriticalSection scanCacheList(self());
      for (TR::CodeCache *codeCache = self()->getFirstCodeCache(); codeCache; codeCache = codeCache->next())
         {
         codeCache->onClassUnloading(loaderPtr);
         }
      }
   }

TR::CodeCache*
J9::CodeCacheManager::reserveCodeCache(bool compilationCodeAllocationsMustBeContiguous,
                                      size_t sizeEstimate,
                                      int32_t compThreadID,
                                      int32_t *numReserved)
   {
   TR::CodeCache *codeCache = self()->OMR::CodeCacheManager::reserveCodeCache(compilationCodeAllocationsMustBeContiguous,
                                                                            sizeEstimate,
                                                                            compThreadID,
                                                                            numReserved);
   if (codeCache == NULL)
      {
      J9JITConfig *jitConfig = self()->fej9()->getJ9JITConfig();
      jitConfig->runtimeFlags |= J9JIT_CODE_CACHE_FULL;
      }
   return codeCache;
   }

void
J9::CodeCacheManager::reportCodeLoadEvents()
   {
   OMR::CodeCacheManager::CacheListCriticalSection reportingCodeLoadEvents(self());
   for (TR::CodeCache *codeCache = self()->getFirstCodeCache(); codeCache; codeCache = codeCache->next())
      {
      codeCache->reportCodeLoadEvents();
      }
   }


TR::CodeCacheMemorySegment *
J9::CodeCacheManager::allocateCodeCacheSegment(size_t segmentSize,
                                              size_t &codeCacheSizeToAllocate,
                                              void *preferredStartAddress)
   {
   J9JavaVM *javaVM = this->javaVM();
   J9JITConfig *jitConfig = this->jitConfig();
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);

   J9PortVmemParams vmemParams;
   j9vmem_vmem_params_init(&vmemParams);
   UDATA *pageSizes = j9vmem_supported_page_sizes();

   TR::CodeCacheConfig &config = self()->codeCacheConfig();

   UDATA ccTotalSizeB = config.codeCacheTotalKB() << 10;

   // Determine whether we are actually using large pages.
   // Note that config.largeCodePageSize() could be set at the default page size (given by pageSize[0])
   size_t largeCodePageSize = 0;
   if (config.largeCodePageSize() > pageSizes[0])
      largeCodePageSize = config.largeCodePageSize();

#if defined(TR_TARGET_POWER) && defined(TR_HOST_POWER)
   /* Use largeCodePageSize on PPC only if its 16M.
    If we pass in any pagesize other than the default page size, the port library picks the shared memory api to allocate which wastes memory */
   size_t sixteenMegs = 16 * 1024 * 1024;
   if (largeCodePageSize == sixteenMegs)
#else
   if (largeCodePageSize > 0)
#endif
      {
      vmemParams.pageSize = largeCodePageSize;
      vmemParams.pageFlags = config.largeCodePageFlags();

      // Recalculate the page size if using large pages
      if(vmemParams.pageSize > ccTotalSizeB)
         {
         for (UDATA pageIndex = 0; 0 != pageSizes[pageIndex]; ++pageIndex)
            {
            if(pageSizes[pageIndex] <= ccTotalSizeB)
               vmemParams.pageSize = pageSizes[pageIndex];
            }
         if (config.verboseCodeCache() || config.verbosePerformance())
            TR_VerboseLog::writeLineLocked(TR_Vlog_CODECACHE,"Warning: Using page size %zu instead of large page size %zu", (size_t)vmemParams.pageSize, largeCodePageSize);
         }
      }

   UDATA mode = J9PORT_VMEM_MEMORY_MODE_READ |
                J9PORT_VMEM_MEMORY_MODE_WRITE |
                J9PORT_VMEM_MEMORY_MODE_EXECUTE;

   UDATA segmentType = MEMORY_TYPE_CODE | MEMORY_TYPE_RAM;

   /* On AIX memory cannot be reserved, and it has to be commited upfront */
   /* See bugzilla 109956 */
#ifndef AIXPPC
   if (config.codeCachePadKB())
      {
      segmentType |= MEMORY_TYPE_UNCOMMITTED;
      }
   else
#endif
      {
      mode |= J9PORT_VMEM_MEMORY_MODE_COMMIT;
      }

   vmemParams.mode = mode;
   vmemParams.category = J9MEM_CATEGORY_JIT_CODE_CACHE;

    /* If codeCachePadKB is defined, get the maximum between the requested size and codeCachePadKB.
      If not defined, codeCachePadKB is 0, so getting the maximum will give segmentSize. */
   codeCacheSizeToAllocate = std::max(segmentSize, (config.codeCachePadKB() << 10));
   // For virtual allocations the size must always be a multiple of the page size
   codeCacheSizeToAllocate = (codeCacheSizeToAllocate + (vmemParams.pageSize-1)) & (~(vmemParams.pageSize-1));
   vmemParams.byteAmount = codeCacheSizeToAllocate;

   // Fail code cache allocation, if the available physical memory is low
   if (!self()->isSufficientPhysicalMemoryAvailableForAllocation(codeCacheSizeToAllocate))
      return 0;

   void *defaultEndAddress = vmemParams.endAddress;

   uintptr_t someJitLibraryAddress = self()->getSomeJitLibraryAddress();

#if defined(TR_TARGET_POWER)
   size_t alignment = 64 * 1024;
#elif (defined(LINUX) && defined(TR_TARGET_X86))
   size_t alignment = 2 * 1024 * 1024;
#elif (defined(TR_TARGET_S390))
   size_t alignment = 1024 * 1024;
#else
   size_t alignment = pageSizes[0];
#endif
   if (largeCodePageSize > 0)
      alignment = largeCodePageSize;

   if (preferredStartAddress)
      {
      vmemParams.options |= J9PORT_VMEM_STRICT_ADDRESS; // if we cannot allocate a block in the desired range, return NULL
      vmemParams.alignmentInBytes = alignment;
#if defined(LINUX)
      vmemParams.options |= J9PORT_VMEM_ALLOC_QUICK; // Use smaps for guidance
      vmemParams.startAddress = preferredStartAddress;
      if ((uintptr_t)preferredStartAddress < someJitLibraryAddress) // Allocate before JIT dll
         {
         vmemParams.endAddress = (void *)(someJitLibraryAddress - SAFE_DISTANCE_REPOSITORY_JITLIBRARY - segmentSize);
         }
      else // Allocate after JIT dll
         {
         vmemParams.endAddress = (void *)(someJitLibraryAddress + MAX_DISTANCE_NEAR_JITLIBRARY_TO_AVOID_TRAMPOLINE - segmentSize);
         }
      // If the code cache repository size is very large, it may not be possible to
      // allocate it in the vicinity of the JIT dll. If that's the case, let the OS
      // pick any address it wants.
      if (vmemParams.startAddress >= vmemParams.endAddress)
         {
         preferredStartAddress = NULL;
         vmemParams.startAddress = NULL;
         vmemParams.endAddress = defaultEndAddress;
         vmemParams.options &= ~(J9PORT_VMEM_STRICT_ADDRESS);
         vmemParams.options &= ~(J9PORT_VMEM_ALLOC_QUICK);
         }
#else
     vmemParams.startAddress = preferredStartAddress;
     vmemParams.endAddress = (void *)(((UDATA)preferredStartAddress) + SAFE_DISTANCE_REPOSITORY_JITLIBRARY);
#endif
      }
#if defined(J9ZOS390)
   else
      {
      vmemParams.options |= J9PORT_VMEM_STRICT_ADDRESS; // if we cannot allocate a block whose start is between preferredStartAddress and endAddress, return NULL
      if (!TR::Options::getCmdLineOptions()->getOption(TR_EnableRMODE64))
         {
         vmemParams.startAddress = (void *)0x0;
         vmemParams.endAddress =   (void *)0x7FFFFFFF;
         }
      else
         {
         vmemParams.startAddress = (void *)0x80000000;
         vmemParams.endAddress =   (void *)0x7FFFFFFFFFFFFFFF;
         }
      }
#endif

#ifdef LINUX
      if (_disclaimEnabled)
         {
         // If swap is enabled, we can allocate memory with mmap(MAP_ANOYNMOUS|MAP_PRIVATE) and disclaim to swap
         // If swap is not enabled we can disclaim to a backing file
         TR::CompilationInfo * compInfo = TR::CompilationInfo::get(_jitConfig);
         if (!TR::Options::getCmdLineOptions()->getOption(TR_DisclaimMemoryOnSwap) || compInfo->isSwapMemoryDisabled())
            {
            segmentType |= MEMORY_TYPE_DISCLAIMABLE_TO_FILE;
            }
         }
#endif

   mcc_printf("TR::CodeCache::allocate : requesting %d bytes\n", codeCacheSizeToAllocate);
   mcc_printf("TR::CodeCache::allocate : javaVM = %p\n", javaVM);
   mcc_printf("TR::CodeCache::allocate : codeCacheList = %p\n", jitConfig->codeCacheList);
   mcc_printf("TR::CodeCache::allocate : size = %p\n", codeCacheSizeToAllocate);
   mcc_printf("TR::CodeCache::allocate : largeCodePageSize = %d\n", largeCodePageSize);
   mcc_printf("TR::CodeCache::allocate : segmentType = %x\n", segmentType);
   mcc_printf("TR::CodeCache::allocate : mode = %x\n", mode);
   mcc_printf("TR::CodeCache::allocate : preferredStartAddress = %p\n", preferredStartAddress);

   J9MemorySegment *codeCacheSegment =
      javaVM->internalVMFunctions->allocateVirtualMemorySegmentInList(javaVM,
                                                                      jitConfig->codeCacheList,
                                                                      codeCacheSizeToAllocate,
                                                                      segmentType,
                                                                      &vmemParams);

   if (!codeCacheSegment && preferredStartAddress)
      {
#if !defined(J9ZOS390)
      // We could have failed because we wanted a start address.
      // Let's try without it.
      vmemParams.startAddress = NULL;
      vmemParams.options &= ~(J9PORT_VMEM_STRICT_ADDRESS);
      vmemParams.options &= ~(J9PORT_VMEM_ADDRESS_HINT);
      vmemParams.options &= ~(J9PORT_VMEM_ALLOC_QUICK);
      vmemParams.endAddress = defaultEndAddress;
#else
      vmemParams.options |= J9PORT_VMEM_STRICT_ADDRESS; // if we cannot allocate a block whose start is between preferredStartAddress and endAddress, return NULL
      if (!TR::Options::getCmdLineOptions()->getOption(TR_EnableRMODE64))
         {
         vmemParams.startAddress = (void *)0x0;
         vmemParams.endAddress =   (void *)0x7FFFFFFF;
         }
      else
         {
         vmemParams.startAddress = (void *)0x80000000;
         vmemParams.endAddress =   (void *)0x7FFFFFFFFFFFFFFF;
         }
#endif

      codeCacheSegment =
          javaVM->internalVMFunctions->allocateVirtualMemorySegmentInList(javaVM,
                                                                          jitConfig->codeCacheList,
                                                                          codeCacheSizeToAllocate,
                                                                          segmentType,
                                                                          &vmemParams);
      }

   if (codeCacheSegment)
      {
      mcc_printf("TR::CodeCache::allocated : codeCacheSegment is %p\n",codeCacheSegment);
      if (config.verboseCodeCache())
         {
         const char *verboseLogString = "The code cache repository was allocated between addresses %p and %p alignment=%zu largeCodePageSize=%zu";
         if (preferredStartAddress && self()->isInRange((uintptr_t)(codeCacheSegment->baseAddress), someJitLibraryAddress, MAX_DISTANCE_NEAR_JITLIBRARY_TO_AVOID_TRAMPOLINE))
            {
            verboseLogString = "The code cache repository was allocated between addresses %p and %p to avoid helper trampolines. alignment=%zu largeCodePageSize=%zu";
            }
         TR_VerboseLog::writeLineLocked(TR_Vlog_CODECACHE, verboseLogString, codeCacheSegment->baseAddress, codeCacheSegment->heapTop, alignment, largeCodePageSize);
         }
#ifdef LINUX
      if (0 != madvise((void *)codeCacheSegment->baseAddress, (codeCacheSegment->heapTop - codeCacheSegment->baseAddress), MADV_HUGEPAGE))
         {
         if (config.verboseCodeCache() || TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_PERF,"Warning: madvise failed while providing hint to use large pages for code cache");
            }
         }
#endif // LINUX

      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
         TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "Allocated new code cache segment %p starting at address %p",
                                        codeCacheSegment,
                                        codeCacheSegment->heapBase);
      }
   else
      {
      // TODO: we should generate a trace point
      mcc_printf("TR::CodeCache::allocate : codeCacheSegment is NULL, %p\n",codeCacheSegment);

      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
         TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "Failed to allocate new code cache segment of %d Kb", _jitConfig->codeCacheKB);

      return 0;
      }

#ifndef AIXPPC
   /* Commit segmentSize only. Accessing beyond that point may result in a segmentation fault */
   if (config.codeCachePadKB())
      {
      void* rc = j9vmem_commit_memory(codeCacheSegment->vmemIdentifier.address,
                                      segmentSize,
                                      &codeCacheSegment->vmemIdentifier);

      if (NULL == rc)
         {
         // TODO: we should generate a trace point
         // TODO: is this the right way of deallocating virtual memory?
         javaVM->internalVMFunctions->freeMemorySegment(javaVM, codeCacheSegment, 1);
         // or
         // javaVM->internalVMFunctions->freeMemorySegmentListEntry(jitConfig->codeCacheList, codeCacheSegment);
         return 0;
         }
      }
#endif

   mcc_printf("TR::CodeCache::allocate : codeCacheSegment in %p\n",codeCacheSegment);
   mcc_printf("TR::CodeCache::allocate : requested segment size = %d\n", codeCacheSizeToAllocate);
   mcc_printf("TR::CodeCache::allocate : real heap base = %p\n", codeCacheSegment->heapBase);
   mcc_printf("TR::CodeCache::allocate : real heap top = %p\n", codeCacheSegment->heapTop);
   mcc_printf("TR::CodeCache::allocate : alloc of codeCacheSegment = %p\n",codeCacheSegment->baseAddress);
   mcc_printf("TR::CodeCache::allocate : size of codeCacheSegment = %d\n",codeCacheSegment->size);

   if (config.verboseCodeCache())
      TR_VerboseLog::writeLineLocked(TR_Vlog_CODECACHE, "allocated code cache segment of size %u", codeCacheSizeToAllocate);

   TR::CodeCacheMemorySegment *memSegment = (TR::CodeCacheMemorySegment *) self()->getMemory(sizeof(TR::CodeCacheMemorySegment));
   new (memSegment) TR::CodeCacheMemorySegment(codeCacheSegment);

   return memSegment;
   }

TR::CodeCacheMemorySegment *
J9::CodeCacheManager::setupMemorySegmentFromRepository(uint8_t *start,
                                                      uint8_t *end,
                                                      size_t & codeCacheSizeToAllocate)
   {
   // create and set the j9segment object
   J9MemorySegment *j9segment = (J9MemorySegment *) self()->getMemory(sizeof(J9MemorySegment));
   j9segment->baseAddress = start;
   j9segment->heapBase = start;
   j9segment->heapAlloc = start;
   j9segment->heapTop = end;

   // create and set the TR_CodeCacheMemorySegment object
   TR::CodeCacheMemorySegment *memorySegment = (TR::CodeCacheMemorySegment *) self()->getMemory(sizeof(TR::CodeCacheMemorySegment));
   new (memorySegment) TR::CodeCacheMemorySegment(j9segment);

   j9segment->heapAlloc = end; // make it look full?

   return memorySegment;
   }


void
J9::CodeCacheManager::freeMemorySegment(TR::CodeCacheMemorySegment *segment)
   {
   segment->free(self());
   }

void *
J9::CodeCacheManager::chooseCacheStartAddress(size_t repositorySize)
   {
   void *startAddress = NULL;
#if defined(TR_HOST_64BIT) && defined(TR_TARGET_X86)
   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableSmartPlacementOfCodeCaches))
      {
      size_t alignment = 2 * 1024 * 1024; // 2MB alignment is preferred on x86-64
      TR::CodeCacheConfig & config = self()->codeCacheConfig();
      const size_t largeCodePageSize = config.largeCodePageSize();

      // at least on Windows we need 2MB minimum alignment for 64bit
      if (largeCodePageSize > alignment)
         alignment = largeCodePageSize;

      uintptr_t someFunctionPointer = self()->getSomeJitLibraryAddress();

      mcc_printf("addCodeCache() function address is %p\n", someFunctionPointer);

      // Depending on where the JIT dll is placed in memory, we may want
      // to allocate the code cache before or after the JIT dll
      if (someFunctionPointer > MAX_DISTANCE_NEAR_JITLIBRARY_TO_AVOID_TRAMPOLINE)
         {
         // Enough space is available before the JIT dll
         // For Linux we can afford to search a larger space (~2 GB) because we use smaps to accelerate the search operation.
         // Also, if we use very large pages (1 GB), then we can afford to search a larger space because there won't be
         // too many attempts to find the right spot.
         if (TR::Compiler->target.isLinux() || alignment > 2 * 1024 * 1024)
            {
            startAddress = (void *)OMR::align((size_t)(someFunctionPointer - MAX_DISTANCE_NEAR_JITLIBRARY_TO_AVOID_TRAMPOLINE), alignment);
            }
         else
            {
            size_t safeDistance = repositorySize + SAFE_DISTANCE_REPOSITORY_JITLIBRARY;
            startAddress = (void *)OMR::align((size_t)(someFunctionPointer - safeDistance), alignment);
            }
         }
      else // Choose a starting address after the JIT dll
         {
         startAddress = (void *)OMR::align((size_t)(someFunctionPointer + SAFE_DISTANCE_REPOSITORY_JITLIBRARY), alignment);
         }
      }
#endif
   return startAddress;
   }


// Deal with FSD decompilation trigger
//
void
J9::CodeCacheManager::onFSDDecompile()
   {
   TR::CodeCacheConfig &config = self()->codeCacheConfig();
   if (!config.needsMethodTrampolines())
      return;

      {
      CacheListCriticalSection scanCacheList(self());
      for (TR::CodeCache *codeCache = self()->getFirstCodeCache(); codeCache; codeCache = codeCache->next())
         {
         codeCache->onFSDDecompile();
         }
      }
   }


// Deal with class redefinition
//
void
J9::CodeCacheManager::onClassRedefinition(TR_OpaqueMethodBlock *oldMethod,
                                          TR_OpaqueMethodBlock *newMethod)
   {
   TR::CodeCacheConfig &config = self()->codeCacheConfig();
   if (!config.needsMethodTrampolines())
      return;

   // Don't allow hashEntry to linger somewhere else
   self()->synchronizeTrampolines();

      {
      CacheListCriticalSection scanCacheList(self());
      for (TR::CodeCache *codeCache = self()->getFirstCodeCache(); codeCache; codeCache = codeCache->next())
         {
         codeCache->onClassRedefinition(oldMethod, newMethod);
         }
      }
   }

// Get an address within the jit library
//
uintptr_t
J9::CodeCacheManager::getSomeJitLibraryAddress()
   {
   void (TR::CodeCacheManager::*pFunc)(TR::CodeCache *codeCache) = &TR::CodeCacheManager::addCodeCache;
   return (uintptr_t) ((void *&) pFunc);
   }

// Determine whether address1 and address2 are in range
//
bool
J9::CodeCacheManager::isInRange(uintptr_t address1, uintptr_t address2, uintptr_t range)
   {
   if (address1 > address2)
      return (address1 - address2) <= range;
   else
      return (address2 - address1) <= range;
   }

void
J9::CodeCacheManager::reservationInterfaceCache(void *callSite, TR_OpaqueMethodBlock *method)
   {
   TR::CodeCacheConfig &config = self()->codeCacheConfig();
   if (!config.needsMethodTrampolines())
      return;

   TR::CodeCache *codeCache = self()->findCodeCacheFromPC(callSite);
   if (!codeCache)
      return;

   codeCache->findOrAddResolvedMethod(method);
   }


void
J9::CodeCacheManager::lateInitialization()
   {
   TR::CodeCacheConfig &config = self()->codeCacheConfig();
   if (!config.trampolineCodeSize())
      return;

   for (TR::CodeCache * codeCache = self()->getFirstCodeCache(); codeCache; codeCache = codeCache->next())
      {
      config.mccCallbacks().createHelperTrampolines((uint8_t *)codeCache->getHelperBase(), config.numRuntimeHelpers());
      }
   }


void
J9::CodeCacheManager::addFreeBlock(void *metaData, uint8_t *startPC)
   {
   TR::CodeCache *owningCodeCache = self()->findCodeCacheFromPC(startPC);
   owningCodeCache->addFreeBlock(metaData);
   }


bool
J9::CodeCacheManager::almostOutOfCodeCache()
   {
   if (self()->lowCodeCacheSpaceThresholdReached())
      return true;

   TR::CodeCacheConfig &config = self()->codeCacheConfig();

   // If we can allocate another code cache we are fine
   // Put common case first
   if (self()->canAddNewCodeCache())
      return false;
   else
      {
      // Check the space in the most current code cache
      bool foundSpace = false;

         {
         CacheListCriticalSection scanCacheList(self());
         for (TR::CodeCache *codeCache = self()->getFirstCodeCache(); codeCache; codeCache = codeCache->next())
            {
            if (codeCache->getFreeContiguousSpace() >= config.lowCodeCacheThreshold())
               {
               foundSpace = true;
               break;
               }
            }
         }

      if (!foundSpace)
         {
         _lowCodeCacheSpaceThresholdReached = true;   // Flag can be checked under debugger
         if (config.verbosePerformance())
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_CODECACHE,"Reached code cache space threshold. Disabling JIT profiling.");
            }

         return true;
         }
      }

   return false;
   }


void
J9::CodeCacheManager::printMccStats()
   {
   self()->printRemainingSpaceInCodeCaches();
   self()->printOccupancyStats();
   }


void
J9::CodeCacheManager::printRemainingSpaceInCodeCaches()
   {
   CacheListCriticalSection scanCacheList(self());
   for (TR::CodeCache *codeCache = self()->getFirstCodeCache(); codeCache; codeCache = codeCache->next())
      {
      fprintf(stderr, "cache %p has %" OMR_PRIdSIZE " bytes empty\n", codeCache, codeCache->getFreeContiguousSpace());
      if (codeCache->isReserved())
         fprintf(stderr, "Above cache is reserved by compThread %d\n", codeCache->getReservingCompThreadID());
      }
   }


void
J9::CodeCacheManager::printOccupancyStats()
   {
   CacheListCriticalSection scanCacheList(self());
   for (TR::CodeCache *codeCache = self()->getFirstCodeCache(); codeCache; codeCache = codeCache->next())
      {
      codeCache->printOccupancyStats();
      }
   }


int32_t
J9::CodeCacheManager::disclaimAllCodeCaches()
   {
   if (!_disclaimEnabled)
      return 0;

   int32_t numDisclaimed = 0;

#ifdef LINUX
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get(_jitConfig);
   bool canDisclaimOnSwap = TR::Options::getCmdLineOptions()->getOption(TR_DisclaimMemoryOnSwap) && !compInfo->isSwapMemoryDisabled();

   CacheListCriticalSection scanCacheList(self());
   for (TR::CodeCache *codeCache = self()->getFirstCodeCache(); codeCache; codeCache = codeCache->next())
      {
      numDisclaimed += codeCache->disclaim(self(), canDisclaimOnSwap);
      }
#endif // LINUX

   return numDisclaimed;
   }
