/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

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
#include "env/ut_j9jit.h"
#include "infra/CriticalSection.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheMemorySegment.hpp"
#include "env/VMJ9.h"
#include "runtime/ArtifactManager.hpp"
#include "env/IO.hpp"

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
   _jitConfig->lastCodeAllocSize = 0;
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

   TR::CodeCacheConfig &config = self()->codeCacheConfig();

   size_t largeCodePageSize = config.largeCodePageSize();
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

   void *defaultEndAddress = vmemParams.endAddress;

   if (preferredStartAddress)
      {
      vmemParams.options |= J9PORT_VMEM_STRICT_ADDRESS; // if we cannot allocate a block whose start is between preferredStartAddress and endAddress, return NULL
      vmemParams.startAddress = preferredStartAddress;
      vmemParams.endAddress = (void *)(((UDATA)preferredStartAddress) + SAFE_DISTANCE_REPOSITORY_JITLIBRARY);
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
      // we could have failed because we wanted a start address
      // let's try without it
      vmemParams.startAddress = NULL;
      vmemParams.options &= ~(J9PORT_VMEM_STRICT_ADDRESS);
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
   else if (preferredStartAddress)
      {
      if (config.verboseCodeCache())
         TR_VerboseLog::writeLineLocked(TR_Vlog_CODECACHE,
            "The code cache repository was allocated between addresses %p and %p to be near the VM/JIT modules to avoid trampolines.",
            vmemParams.startAddress,
            vmemParams.endAddress);
      }

   if (!codeCacheSegment)
      {
      // TODO: we should generate a trace point
      mcc_printf("TR::CodeCache::allocate : codeCacheSegment is NULL, %p\n",codeCacheSegment);
      return 0;
      }
   else
      {
      mcc_printf("TR::CodeCache::allocated : codeCacheSegment is %p\n",codeCacheSegment);
#if defined(J9ZOS390)
      if (!preferredStartAddress && config.verboseCodeCache())
         TR_VerboseLog::writeLineLocked(TR_Vlog_CODECACHE,
            "The code cache repository was allocated between addresses %p and %p",
            vmemParams.startAddress,
            vmemParams.endAddress);
#endif
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
      size_t alignment = 2 * 1024 * 1024; // 2MB alignment
      TR::CodeCacheConfig & config = self()->codeCacheConfig();
      const size_t largeCodePageSize = config.largeCodePageSize();

      // at least on Windows we need 2MB minimum alignment for 64bit
      if (largeCodePageSize > alignment)
         alignment = largeCodePageSize;

      size_t safeDistance = repositorySize + TR::CodeCacheManager::SAFE_DISTANCE_REPOSITORY_JITLIBRARY;
      void (TR::CodeCacheManager::*pFunc)(TR::CodeCache *codeCache) = &TR::CodeCacheManager::addCodeCache;
      size_t someFunctionPointer = (size_t) ((void *&) pFunc);

      mcc_printf("addCodeCache() function address is %p\n", someFunctionPointer);

      // if we are using 1GB pages or something even larger
      if (largeCodePageSize >= 0x40000000)
         {
         if (someFunctionPointer > largeCodePageSize * 2)
            {
            // round down to the nearest GB first
            startAddress = (void *)(((size_t)someFunctionPointer) & ~(largeCodePageSize-1));
            // then move down 1GB to allocate the code cache
            startAddress = (void *)(((uint8_t *)startAddress) - largeCodePageSize);
            }
         }
      else
         {
         if (someFunctionPointer > safeDistance+alignment)
            {
            // otherwise move back some space larger than the VM DLL footprint
            startAddress = (void *)(((uint8_t *)someFunctionPointer) - safeDistance);
            // align so that port library returns exactly what we wanted
            startAddress = (void *) align((uint8_t *)startAddress, alignment - 1);
            }
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

