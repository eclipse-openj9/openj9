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

#ifndef J9_CODECACHEMANAGER_INCL
#define J9_CODECACHEMANAGER_INCL

#ifndef J9_CODECACHEMANAGER_COMPOSED
#define J9_CODECACHEMANAGER_COMPOSED
namespace J9 { class CodeCacheManager; }
namespace J9 { typedef CodeCacheManager CodeCacheManagerConnector; }
#endif


#include "env/jittypes.h"
//#include "runtime/CodeCacheMemorySegment.hpp"
//#include "runtime/CodeCache.hpp"
#include "runtime/OMRCodeCacheManager.hpp"
#include "env/IO.hpp"
#include "env/VMJ9.h"

struct J9JITConfig;
struct J9JavaVM;
struct J9ClassLoader;

namespace TR { class CodeCacheMemorySegment; }
namespace TR { class CodeCache; }

namespace J9 {

class OMR_EXTENSIBLE CodeCacheManager : public OMR::CodeCacheManagerConnector
   {
   TR::CodeCacheManager *self();

public:
   CodeCacheManager(TR_FrontEnd *fe, TR::RawAllocator rawAllocator) :
      OMR::CodeCacheManagerConnector(rawAllocator),
      _fe(fe)
      {
      _codeCacheManager = reinterpret_cast<TR::CodeCacheManager *>(this);
      }

   void *operator new(size_t s, TR::CodeCacheManager *m) { return m; }

   static TR::CodeCacheManager *instance() { return _codeCacheManager; }
   static J9JITConfig *jitConfig()         { return _jitConfig; }
   static J9JavaVM *javaVM()               { return _javaVM; }

   TR_FrontEnd *fe();
   TR_J9VMBase *fej9();

   TR::CodeCache *initialize(bool useConsolidatedCache, uint32_t numberOfCodeCachesToCreateAtStartup);

   void addCodeCache(TR::CodeCache *codeCache);

   TR::CodeCacheMemorySegment *allocateCodeCacheSegment(size_t segmentSize,
                                                        size_t &codeCacheSizeToAllocate,
                                                        void *preferredStartAddress);
   void *chooseCacheStartAddress(size_t repositorySize);

   void addFaintCacheBlock(OMR::MethodExceptionData *metaData, uint8_t bytesToSaveAtStart);
   void freeFaintCacheBlock(OMR::FaintCacheBlock *block, uint8_t *startPC);

   // reclaim the whole method body (which is guaranteed to be fully dead already - hence
   // no need for leaving stumps at the head or the tail
   void purgeClassLoaderFromFaintBlocks(J9ClassLoader *classLoader);
   void onClassUnloading(J9ClassLoader *loaderPtr);

   TR::CodeCache * reserveCodeCache(bool compilationCodeAllocationsMustBeContiguous,
                                    size_t sizeEstimate,
                                    int32_t compThreadID,
                                    int32_t *numReserved);

   TR::CodeCacheMemorySegment *setupMemorySegmentFromRepository(uint8_t *start,
                                                                uint8_t *end,
                                                                size_t & codeCacheSizeToAllocate);
   void freeMemorySegment(TR::CodeCacheMemorySegment *segment);

   void reportCodeLoadEvents();

   static const uint32_t SAFE_DISTANCE_REPOSITORY_JITLIBRARY = 64 * 1024 * 1024;  // 64MB to account for some safe JIT library size
   static const uintptr_t MAX_DISTANCE_NEAR_JITLIBRARY_TO_AVOID_TRAMPOLINE = 0x80000000 - 64 * 1024 * 1024; // 2GB - 64MB

   void setCodeCacheFull();

   void onFSDDecompile();
   void onClassRedefinition(TR_OpaqueMethodBlock *oldMethod, TR_OpaqueMethodBlock *newMethod);

   uintptr_t getSomeJitLibraryAddress();
   bool isInRange(uintptr_t address1, uintptr_t address2, uintptr_t range);

   /**
    * @brief Reserve a trampoline for an interface PIC slot.
    *
    * @param[in] callSite : address in the code cache of the call instruction
    * @param[in] method : J9Method for the interface
    */
   void reservationInterfaceCache(void *callSite, TR_OpaqueMethodBlock *method);

   /**
    * @brief Re-do the helper trampoline initialization for existing codeCaches
    */
   void lateInitialization();

   /**
    * @brief Add block defined by metaData to the freeBlockList.  The caller
    *        should expect that the block may not always be added.
    */
   void addFreeBlock(void *metaData, uint8_t *startPC);

   /**
    * @brief Answers whether the total code cache capacity is nearly used up.
    *        This code is executed by compilation thread and by application
    *        threads when trying to induce a profiling compilation. Acquires
    *        codeCacheList.mutex.
    *
    * @return true if capacity nearly reached; false otherwise.
    */
   bool almostOutOfCodeCache();

   /**
    * @brief Print some code cache usage statistics
    */
   void printMccStats();

   /**
    * @brief Print space remaining in each code cache
    */
   void printRemainingSpaceInCodeCaches();

   /**
    * @brief Print occupancy stats for each code cache
    */
   void printOccupancyStats();

private :
   TR_FrontEnd *_fe;
   static TR::CodeCacheManager *_codeCacheManager;
   static J9JITConfig *_jitConfig;
   static J9JavaVM *_javaVM;
   };

} // namespace J9

#endif // J9_CODECACHEMANAGER_INCL
