/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#include "env/J9SharedCache.hpp"

#include <algorithm>
#include "j9cfg.h"
#include "control/CompilationRuntime.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/ClassLoaderTable.hpp"
#include "env/ClassTableCriticalSection.hpp"
#include "env/jittypes.h"
#include "env/j9method.h"
#include "env/PersistentCHTable.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "env/VMJ9.h"
#include "exceptions/PersistenceFailure.hpp"
#include "infra/CriticalSection.hpp"
#include "runtime/CodeRuntime.hpp"
#include "runtime/IProfiler.hpp"
#include "runtime/RuntimeAssumptions.hpp"
#if defined(J9VM_OPT_JITSERVER)
#include "control/CompilationThread.hpp" // for TR::compInfoPT
#include "control/JITServerHelpers.hpp"
#include "runtime/JITClientSession.hpp"
#endif

#define LOG(logLevel, format, ...)               \
   if (_logLevel >= logLevel)                    \
      {                                          \
      log("" format "", ##__VA_ARGS__);          \
      }

// From CompositeCache.cpp
#define UPDATEPTR(ca) (((uint8_t *)(ca)) + (ca)->updateSRP)
#define SEGUPDATEPTR(ca) (((uint8_t *)(ca)) + (ca)->segmentSRP)

TR_J9SharedCache::TR_J9SharedCacheDisabledReason TR_J9SharedCache::_sharedCacheState = TR_J9SharedCache::UNINITIALIZED;
TR_YesNoMaybe TR_J9SharedCache::_sharedCacheDisabledBecauseFull = TR_maybe;
UDATA TR_J9SharedCache::_storeSharedDataFailedLength = 0;

TR_YesNoMaybe TR_J9SharedCache::isSharedCacheDisabledBecauseFull(TR::CompilationInfo *compInfo)
   {
   if (_sharedCacheDisabledBecauseFull == TR_maybe)
      {
      if (_sharedCacheState == SHARED_CACHE_FULL)
         {
         _sharedCacheDisabledBecauseFull = TR_yes;
         }
#if defined(J9VM_OPT_SHARED_CLASSES) && defined(J9VM_INTERP_AOT_RUNTIME_SUPPORT)
      // Need to check if the AOT Header / Class Chain Data failed to store because
      // the SCC is full or for some other reason.
      else if (_sharedCacheState == AOT_HEADER_STORE_FAILED ||
               _sharedCacheState == SHARED_CACHE_CLASS_CHAIN_STORE_FAILED)
         {
         J9JavaVM * javaVM = compInfo->getJITConfig()->javaVM;
         if (javaVM->sharedClassConfig && javaVM->sharedClassConfig->getJavacoreData)
            {
            J9SharedClassJavacoreDataDescriptor javacoreData;
            memset(&javacoreData, 0, sizeof(J9SharedClassJavacoreDataDescriptor));
            javaVM->sharedClassConfig->getJavacoreData(javaVM, &javacoreData);

            if (javacoreData.freeBytes <= _storeSharedDataFailedLength)
               _sharedCacheDisabledBecauseFull = TR_yes;
            else
               _sharedCacheDisabledBecauseFull = TR_no;

            if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
               TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "Free Bytes in SCC = %u B", javacoreData.freeBytes);
            }
         else
            {
            _sharedCacheDisabledBecauseFull = TR_no;
            }
         }
#endif
      else
         {
         _sharedCacheDisabledBecauseFull = TR_no;
         }
      }

   return _sharedCacheDisabledBecauseFull;
   }

const CCVResult
TR_J9SharedCache::getCachedCCVResult(TR_OpaqueClassBlock *clazz)
   {
   if (TR::Options::getCmdLineOptions()->allowRecompilation()
       && !TR::Options::getCmdLineOptions()->getOption(TR_DisableCHOpts))
      {
      TR::ClassTableCriticalSection cacheResult(_fe);
      TR_PersistentCHTable *table = _compInfo->getPersistentInfo()->getPersistentCHTable();
      TR_PersistentClassInfo *classInfo = table->findClassInfo(clazz);
      return classInfo->getCCVResult();
      }
   return CCVResult::notYetValidated;
   }

bool
TR_J9SharedCache::cacheCCVResult(TR_OpaqueClassBlock *clazz, CCVResult result)
   {
   if (TR::Options::getCmdLineOptions()->allowRecompilation()
       && !TR::Options::getCmdLineOptions()->getOption(TR_DisableCHOpts))
      {
      TR::ClassTableCriticalSection cacheResult(_fe);
      TR_PersistentCHTable *table = _compInfo->getPersistentInfo()->getPersistentCHTable();
      TR_PersistentClassInfo *classInfo = table->findClassInfo(clazz);
      classInfo->setCCVResult(result);
      return true;
      }
   return false;
   }

TR_J9SharedCache::TR_J9SharedCache(TR_J9VMBase *fe)
   {
#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   _fe = fe;
   _jitConfig = fe->getJ9JITConfig();
   _javaVM = _jitConfig->javaVM;
   _compInfo = TR::CompilationInfo::get(_jitConfig);
   _aotStats = fe->getPrivateConfig()->aotStats;
   _sharedCacheConfig = _javaVM->sharedClassConfig;
   _numDigitsForCacheOffsets = 8;

#if defined(J9VM_OPT_JITSERVER)
   TR_ASSERT_FATAL(_sharedCacheConfig || _compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER, "Must have _sharedCacheConfig");
#else
   TR_ASSERT_FATAL(_sharedCacheConfig, "Must have _sharedCacheConfig");
#endif

#if defined(J9VM_OPT_JITSERVER)
   if (_sharedCacheConfig)
#endif
      {
      UDATA totalCacheSize = 0;
      J9SharedClassCacheDescriptor *curCache = _sharedCacheConfig->cacheDescriptorList;
      do
         {
         totalCacheSize += curCache->cacheSizeBytes;
         curCache = curCache->next;
         }
      while (curCache != _sharedCacheConfig->cacheDescriptorList);

      if (totalCacheSize > UINT_MAX)
         _numDigitsForCacheOffsets = 16;

      _hintsEnabledMask = 0;
      if (!TR::Options::getAOTCmdLineOptions()->getOption(TR_DisableSharedCacheHints))
         _hintsEnabledMask = TR::Options::getAOTCmdLineOptions()->getEnableSCHintFlags();

      _initialHintSCount = std::min(TR::Options::getCmdLineOptions()->getInitialSCount(), TR::Options::getAOTCmdLineOptions()->getInitialSCount());
      if (_initialHintSCount == 0)
         _initialHintSCount = 1;

      _logLevel = std::max(TR::Options::getAOTCmdLineOptions()->getAotrtDebugLevel(), TR::Options::getCmdLineOptions()->getAotrtDebugLevel());

      _verboseHints = TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseSCHints);

      LOG(1, "\t_sharedCacheConfig %p\n", _sharedCacheConfig);
      LOG(1, "\ttotalCacheSize %p\n", totalCacheSize);
      }
#endif
   }

J9SharedClassCacheDescriptor *
TR_J9SharedCache::getCacheDescriptorList()
   {
#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   if (_sharedCacheConfig)
      return _sharedCacheConfig->cacheDescriptorList;
#endif
   return NULL;
   }

void
TR_J9SharedCache::log(char *format, ...)
   {
   PORT_ACCESS_FROM_PORT(_javaVM->portLibrary);
   char outputBuffer[512] = "TR_J9SC:";
   const uint32_t startOffset = 8;   // strlen("TR_J9SC:")

   va_list args;
   va_start(args, format);
   j9str_vprintf(outputBuffer+startOffset, 512, format, args);
   va_end(args);

   JITRT_LOCK_LOG(jitConfig());
   JITRT_PRINTF(jitConfig())(jitConfig(), "%s", outputBuffer);
   JITRT_UNLOCK_LOG(jitConfig());
   }

TR_J9SharedCache::SCCHint
TR_J9SharedCache::getHint(J9VMThread * vmThread, J9Method *method)
   {
   SCCHint result;

#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);

   J9SharedDataDescriptor descriptor;
   descriptor.address = (U_8 *)&result;
   descriptor.length = sizeof(result);
   descriptor.type = J9SHR_ATTACHED_DATA_TYPE_JITHINT;
   descriptor.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;
   IDATA dataIsCorrupt;
   SCCHint *find = (SCCHint *)sharedCacheConfig()->findAttachedData(vmThread, romMethod, &descriptor, &dataIsCorrupt);

   if ((find != (SCCHint *)descriptor.address) || (dataIsCorrupt != -1))
      {
      result.clear();
      }
#endif // defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   return result;
   }

uint16_t
TR_J9SharedCache::getAllEnabledHints(J9Method *method)
   {
   uint16_t hintFlags = 0;
#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   if (_hintsEnabledMask)
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      J9VMThread * vmThread = fej9->getCurrentVMThread();
      SCCHint scHints = getHint(vmThread, method);
      hintFlags = scHints.flags & _hintsEnabledMask;
      }
#endif // defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   return hintFlags;
   }


bool
TR_J9SharedCache::isHint(J9Method *method, TR_SharedCacheHint theHint, uint16_t *dataField)
   {
   bool isHint = false;

#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   uint16_t hint = ((uint16_t)theHint) & _hintsEnabledMask;
   if (hint != 0)
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
      J9VMThread * vmThread = fej9->getCurrentVMThread();
      SCCHint scHints = getHint(vmThread, method);

      if (dataField)
         *dataField = scHints.data;

      if (_verboseHints)
         {
         char methodSignature[500];
         int32_t maxSignatureLength = 500;
         fej9->printTruncatedSignature(methodSignature, maxSignatureLength, (TR_OpaqueMethodBlock *) method);
         TR_VerboseLog::writeLineLocked(TR_Vlog_SCHINTS,"is hint %x(%x) %s", scHints.flags, hint, methodSignature);
         }
      isHint = (scHints.flags & hint) != 0;
      }
#endif // defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   return isHint;
}

bool
TR_J9SharedCache::isHint(TR_ResolvedMethod *method, TR_SharedCacheHint hint, uint16_t *dataField)
   {
   return isHint(((TR_ResolvedJ9Method *) method)->ramMethod(), hint, dataField);
   }

void
TR_J9SharedCache::addHint(J9Method * method, TR_SharedCacheHint theHint)
   {
#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   static bool SCfull = false;
   uint16_t newHint = ((uint16_t)theHint) & _hintsEnabledMask;
   if (newHint)
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
      J9VMThread * vmThread = fej9->getCurrentVMThread();

      char methodSignature[500];
      uint32_t maxSignatureLength = 500;

      bool isFailedValidationHint = (newHint == TR_HintFailedValidation);

      if (_verboseHints)
         {
         fej9->printTruncatedSignature(methodSignature, maxSignatureLength, (TR_OpaqueMethodBlock *) method);
         TR_VerboseLog::writeLineLocked(TR_Vlog_SCHINTS,"adding hint 0x%x %s", newHint, methodSignature);
         }

      // There is only one scenario where concurrency *may* matter, so we don't get a lock
      // The scenario is where a compilation thread wants to register a hint about the method it's compiling at
      // the same time that another thread is inlining it *for the first time*. In that case, one of the hints
      // won't be registered. The affect, however is minimal, and likely to correct itself in the current run
      // (if the inlining hint is missed) or a subsequent run (if the other hint is missed).

      SCCHint scHints = getHint(vmThread, method);
      const uint32_t scHintDataLength = sizeof(scHints);

      if (scHints.flags == 0) // If no prior hints exist, we can perform a "storeAttachedData" operation
         {
         uint32_t bytesToPersist = 0;

         if (!SCfull)
            {
            scHints.flags |= newHint;
            if (isFailedValidationHint)
               scHints.data = 10 * _initialHintSCount;

            J9SharedDataDescriptor descriptor;
            descriptor.address = (U_8 *)&scHints;
            descriptor.length = scHintDataLength; // Size includes the 2nd data field, currently only used for TR_HintFailedValidation
            descriptor.type = J9SHR_ATTACHED_DATA_TYPE_JITHINT;
            descriptor.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;
            UDATA store = sharedCacheConfig()->storeAttachedData(vmThread, romMethod, &descriptor, 0);
            TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig());
            if (store == 0)
               {
               if (_verboseHints)
                  TR_VerboseLog::writeLineLocked(TR_Vlog_SCHINTS,"hint added 0x%x, key = %s, scount: %d", scHints.flags, methodSignature, scHints.data);
               }
            else if (store != J9SHR_RESOURCE_STORE_FULL)
               {
               if (_verboseHints)
                  TR_VerboseLog::writeLineLocked(TR_Vlog_SCHINTS,"hint error: could not be added into SC\n");
               }
            else
               {
               SCfull = true;
               bytesToPersist = scHintDataLength;
               if (_verboseHints)
                  TR_VerboseLog::writeLineLocked(TR_Vlog_SCHINTS,"hint error: SCC full\n");
               }
            }
         else // SCC Full
            {
            bytesToPersist = scHintDataLength;
            }

         if (SCfull &&
             bytesToPersist &&
             !TR::Options::getCmdLineOptions()->getOption(TR_DisableUpdateJITBytesSize))
            {
            _compInfo->increaseUnstoredBytes(0, bytesToPersist);
            }
         }
      else // Some hints already exist for this method. We must perform an "updateAttachedData"
         {
         bool updateHint = false;
         bool hintDidNotExist = ((newHint & scHints.flags) == 0);
         if (hintDidNotExist)
            {
            updateHint = true;
            scHints.flags |= newHint;
            if (isFailedValidationHint)
               scHints.data = 10 * _initialHintSCount;
            }
         else
            {
            // hint already exists, but maybe we need to update the count
            if (isFailedValidationHint)
               {
               uint16_t oldCount = scHints.data;
               uint16_t newCount = std::min(oldCount * 10, TR_DEFAULT_INITIAL_COUNT);
               if (newCount != oldCount)
                  {
                  updateHint = true;
                  scHints.data = newCount;
                  }
               else
                  {
                  if (_verboseHints)
                     {
                     TR_VerboseLog::writeLineLocked(TR_Vlog_SCHINTS, "hint reached max count of %d", oldCount);
                     }
                  }
               }
            }
         if (updateHint)
            {
            J9SharedDataDescriptor descriptor;
            descriptor.address = (U_8 *)&scHints;
            descriptor.length = scHintDataLength; // Size includes the 2nd data field, currently only used for TR_HintFailedValidation
            descriptor.type = J9SHR_ATTACHED_DATA_TYPE_JITHINT;
            descriptor.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;
            UDATA update = sharedCacheConfig()->updateAttachedData(vmThread, romMethod, 0, &descriptor);

            if (_verboseHints)
               {
               if (update == 0)
                  {
                  TR_VerboseLog::writeLineLocked(TR_Vlog_SCHINTS,"hint updated 0x%x, key = %s, scount: %d", scHints.flags, methodSignature, scHints.data);
                  }
               else
                  {
                  TR_VerboseLog::writeLineLocked(TR_Vlog_SCHINTS,"hint error: could not be updated into SC\n");
                  }
               }
            }
         }
      }

#endif // defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   }

void
TR_J9SharedCache::addHint(TR_ResolvedMethod * method, TR_SharedCacheHint hint)
   {
   addHint(((TR_ResolvedJ9Method *) method)->ramMethod(), hint);
   }


void
TR_J9SharedCache::persistIprofileInfo(TR::ResolvedMethodSymbol *methodSymbol, TR::Compilation *comp)
   {
   persistIprofileInfo(methodSymbol, methodSymbol->getResolvedMethod(), comp);
   }

void
TR_J9SharedCache::persistIprofileInfo(TR::ResolvedMethodSymbol *methodSymbol, TR_ResolvedMethod *method, TR::Compilation *comp)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   TR_IProfiler *profiler = fej9->getIProfiler();
   if (profiler)
      profiler->persistIprofileInfo(methodSymbol, method, comp);
   }

bool
TR_J9SharedCache::isMostlyFull()
   {
   return (double) sharedCacheConfig()->getFreeSpaceBytes(_javaVM) / sharedCacheConfig()->getCacheSizeBytes(_javaVM) < 0.8;
   }

bool
TR_J9SharedCache::isPointerInCache(const J9SharedClassCacheDescriptor *cacheDesc, void *ptr)
   {
   bool isPointerInCache = false;
#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   uintptr_t ptrValue = reinterpret_cast<uintptr_t>(ptr);
   uintptr_t cacheStart = reinterpret_cast<uintptr_t>(cacheDesc->cacheStartAddress); // Inclusive
   uintptr_t cacheEnd = cacheStart + cacheDesc->cacheSizeBytes;    // Exclusive

   isPointerInCache = (ptrValue >= cacheStart) && (ptrValue < cacheEnd);
#endif
   return isPointerInCache;
   }

bool
TR_J9SharedCache::isOffsetInCache(const J9SharedClassCacheDescriptor *cacheDesc, uintptr_t offset)
   {
   bool isOffsetInCache = false;
#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   uintptr_t decodedOffset = isOffsetFromStart(offset) ? decodeOffsetFromStart(offset) : decodeOffsetFromEnd(offset);
   isOffsetInCache = (decodedOffset < cacheDesc->cacheSizeBytes);
#endif
   return isOffsetInCache;
   }

bool
TR_J9SharedCache::isPointerInMetadataSectionInCache(const J9SharedClassCacheDescriptor *cacheDesc, void *ptr)
   {
   bool isPointerInMetadataSection = false;
#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   if (isPointerInCache(cacheDesc, ptr))
      {
      uintptr_t ptrValue = reinterpret_cast<uintptr_t>(ptr);
      uintptr_t metadataEndAddress   = reinterpret_cast<uintptr_t>(UPDATEPTR(cacheDesc->cacheStartAddress)); // Inclusive
      uintptr_t metadataStartAddress = reinterpret_cast<uintptr_t>(cacheDesc->metadataStartAddress);         // Exclusive

      isPointerInMetadataSection = (ptrValue >= metadataEndAddress) && (ptrValue < metadataStartAddress);
      }
#endif
   return isPointerInMetadataSection;
   }

bool
TR_J9SharedCache::isOffsetInMetadataSectionInCache(const J9SharedClassCacheDescriptor *cacheDesc, uintptr_t offset)
   {
   bool isOffsetInMetadataSection = false;
#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   if (isOffsetFromEnd(offset) && isOffsetInCache(cacheDesc, offset))
      {
      uintptr_t metadataEndAddress   = reinterpret_cast<uintptr_t>(UPDATEPTR(cacheDesc->cacheStartAddress));
      uintptr_t metadataStartAddress = reinterpret_cast<uintptr_t>(cacheDesc->metadataStartAddress);

      uintptr_t metadataEndOffset = metadataStartAddress - metadataEndAddress; // Inclusive

      isOffsetInMetadataSection = ((decodeOffsetFromEnd(offset) > 0 ) && (decodeOffsetFromEnd(offset) <= metadataEndOffset));
      }
#endif
   return isOffsetInMetadataSection;
   }

bool
TR_J9SharedCache::isPointerInROMClassesSectionInCache(const J9SharedClassCacheDescriptor *cacheDesc, void *ptr)
   {
   bool isPointerInRomClassesSection = false;
#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   if (isPointerInCache(cacheDesc, ptr))
      {
      uintptr_t ptrValue = reinterpret_cast<uintptr_t>(ptr);
      uintptr_t romclassStartAddress = reinterpret_cast<uintptr_t>(cacheDesc->romclassStartAddress);            // Inclusive
      uintptr_t romclassEndAddress   = reinterpret_cast<uintptr_t>(SEGUPDATEPTR(cacheDesc->cacheStartAddress)); // Exclusive

      isPointerInRomClassesSection = (ptrValue >= romclassStartAddress) && (ptrValue < romclassEndAddress);
      }
#endif
   return isPointerInRomClassesSection;
   }

bool
TR_J9SharedCache::isOffsetinROMClassesSectionInCache(const J9SharedClassCacheDescriptor *cacheDesc, uintptr_t offset)
   {
   bool isOffsetInRomClassesSection = false;
#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   if (isOffsetFromStart(offset) && isOffsetInCache(cacheDesc, offset))
      {
      uintptr_t romclassStartAddress = reinterpret_cast<uintptr_t>(cacheDesc->romclassStartAddress);
      uintptr_t romclassEndAddress   = reinterpret_cast<uintptr_t>(SEGUPDATEPTR(cacheDesc->cacheStartAddress));

      uintptr_t romclassEndOffset = romclassEndAddress - romclassStartAddress; // Exclusive

      isOffsetInRomClassesSection = (decodeOffsetFromStart(offset) < romclassEndOffset);
      }
#endif
   return isOffsetInRomClassesSection;
   }

void *
TR_J9SharedCache::pointerFromOffsetInSharedCache(uintptr_t offset)
   {
   uintptr_t ptr = 0;
   if (isOffsetInSharedCache(offset, &ptr))
      {
      return (void *)ptr;
      }
   TR_ASSERT_FATAL(false, "Shared cache offset %d out of bounds", offset);
   return (void *)ptr;
   }

void *
TR_J9SharedCache::romStructureFromOffsetInSharedCache(uintptr_t offset)
   {
   void *romStructure = NULL;
   if (isROMStructureOffsetInSharedCache(offset, &romStructure))
      {
      return romStructure;
      }
   TR_ASSERT_FATAL(false, "Shared cache ROM Structure offset %d out of bounds", offset);
   return romStructure;
   }

J9ROMClass *
TR_J9SharedCache::romClassFromOffsetInSharedCache(uintptr_t offset)
   {
   return reinterpret_cast<J9ROMClass *>(romStructureFromOffsetInSharedCache(offset));
   }

J9ROMMethod *
TR_J9SharedCache::romMethodFromOffsetInSharedCache(uintptr_t offset)
   {
   return reinterpret_cast<J9ROMMethod *>(romStructureFromOffsetInSharedCache(offset));
   }

void *
TR_J9SharedCache::ptrToROMClassesSectionFromOffsetInSharedCache(uintptr_t offset)
   {
   return romStructureFromOffsetInSharedCache(offset);
   }

bool
TR_J9SharedCache::isOffsetInSharedCache(uintptr_t encoded_offset, void *ptr)
   {
#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
#if defined(J9VM_OPT_MULTI_LAYER_SHARED_CLASS_CACHE)
   // The cache descriptor list is linked last to first and is circular, so last->previous == first.
   J9SharedClassCacheDescriptor *firstCache = getCacheDescriptorList()->previous;
   J9SharedClassCacheDescriptor *curCache = firstCache;
   do
      {
      TR_ASSERT_FATAL(isOffsetFromEnd(encoded_offset), "Shared cache (encoded) offset %lld not from end\n", encoded_offset);
      if (isOffsetInMetadataSectionInCache(curCache, encoded_offset))
         {
         if (ptr)
            {
            uintptr_t metadataStartAddress = reinterpret_cast<uintptr_t>(curCache->metadataStartAddress);
            *reinterpret_cast<uintptr_t *>(ptr) = metadataStartAddress - decodeOffsetFromEnd(encoded_offset);
            }
         return true;
         }
      encoded_offset = encodeOffsetFromEnd(decodeOffsetFromEnd(encoded_offset) - curCache->cacheSizeBytes);
      curCache = curCache->previous;
      }
   while (curCache != firstCache);
#else // !J9VM_OPT_MULTI_LAYER_SHARED_CLASS_CACHE
   TR_ASSERT_FATAL(isOffsetFromEnd(encoded_offset), "Shared cache (encoded) offset %lld not from end\n", encoded_offset);
   J9SharedClassCacheDescriptor *curCache = getCacheDescriptorList();
   if (isOffsetInMetadataSectionInCache(curCache, encoded_offset))
      {
      if (ptr)
         {
         uintptr_t metadataStartAddress = reinterpret_cast<uintptr_t>(curCache->metadataStartAddress);
         *reinterpret_cast<uintptr_t *>(ptr) = metadataStartAddress - decodeOffsetFromEnd(encoded_offset);
         }
      return true;
      }
#endif // J9VM_OPT_MULTI_LAYER_SHARED_CLASS_CACHE
#endif
   return false;
   }

bool
TR_J9SharedCache::isROMStructureOffsetInSharedCache(uintptr_t encoded_offset, void **romStructurePtr)
   {
#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
#if defined(J9VM_OPT_MULTI_LAYER_SHARED_CLASS_CACHE)
   // The cache descriptor list is linked last to first and is circular, so last->previous == first.
   J9SharedClassCacheDescriptor *firstCache = getCacheDescriptorList()->previous;
   J9SharedClassCacheDescriptor *curCache = firstCache;
   do
      {
      TR_ASSERT_FATAL(isOffsetFromStart(encoded_offset), "Shared cache (encoded) offset %lld not from start\n", encoded_offset);
      if (isOffsetinROMClassesSectionInCache(curCache, encoded_offset))
         {
         if (romStructurePtr)
            {
            uintptr_t romclassStartAddress = reinterpret_cast<uintptr_t>(curCache->romclassStartAddress);
            *romStructurePtr = reinterpret_cast<void *>(romclassStartAddress + decodeOffsetFromStart(encoded_offset));
            }
         return true;
         }
      encoded_offset = encodeOffsetFromStart(decodeOffsetFromStart(encoded_offset) - curCache->cacheSizeBytes);
      curCache = curCache->previous;
      }
   while (curCache != firstCache);
#else // !J9VM_OPT_MULTI_LAYER_SHARED_CLASS_CACHE
   TR_ASSERT_FATAL(isOffsetFromStart(encoded_offset), "Shared cache (encoded) offset %lld not from start\n", encoded_offset);
   J9SharedClassCacheDescriptor *curCache = getCacheDescriptorList();
   if (isOffsetinROMClassesSectionInCache(curCache, encoded_offset))
      {
      if (romStructurePtr)
         {
         uintptr_t romclassStartAddress = reinterpret_cast<uintptr_t>(curCache->romclassStartAddress);
         *romStructurePtr = reinterpret_cast<void *>(romclassStartAddress + decodeOffsetFromStart(encoded_offset));
         }
      return true;
      }
#endif // J9VM_OPT_MULTI_LAYER_SHARED_CLASS_CACHE
#endif
   return false;
   }

bool
TR_J9SharedCache::isROMClassOffsetInSharedCache(uintptr_t offset, J9ROMClass **romClassPtr)
   {
   return isROMStructureOffsetInSharedCache(offset, reinterpret_cast<void **>(romClassPtr));
   }

bool
TR_J9SharedCache::isROMMethodOffsetInSharedCache(uintptr_t offset, J9ROMMethod **romMethodPtr)
   {
   return isROMStructureOffsetInSharedCache(offset, reinterpret_cast<void **>(romMethodPtr));
   }

bool
TR_J9SharedCache::isOffsetOfPtrToROMClassesSectionInSharedCache(uintptr_t offset, void **ptr)
   {
   return isROMStructureOffsetInSharedCache(offset, ptr);
   }

uintptr_t
TR_J9SharedCache::offsetInSharedCacheFromPointer(void *ptr)
   {
   uintptr_t offset = 0;
   if (isPointerInSharedCache(ptr, &offset))
      {
      return offset;
      }
   TR_ASSERT_FATAL(false, "Shared cache pointer %p out of bounds", ptr);
   return offset;
   }

uintptr_t
TR_J9SharedCache::offsetInSharedcacheFromROMStructure(void *romStructure)
   {
   uintptr_t offset = 0;
   if (isROMStructureInSharedCache(romStructure, &offset))
      {
      return offset;
      }
   TR_ASSERT_FATAL(false, "Shared cache ROM Structure pointer %p out of bounds", romStructure);
   return offset;
   }

uintptr_t
TR_J9SharedCache::offsetInSharedCacheFromROMClass(J9ROMClass *romClass)
   {
   return offsetInSharedcacheFromROMStructure(romClass);
   }

uintptr_t
TR_J9SharedCache::offsetInSharedCacheFromROMMethod(J9ROMMethod *romMethod)
   {
   return offsetInSharedcacheFromROMStructure(romMethod);
   }

uintptr_t
TR_J9SharedCache::offsetInSharedCacheFromPtrToROMClassesSection(void *ptr)
   {
   return offsetInSharedcacheFromROMStructure(ptr);
   }

bool
TR_J9SharedCache::isPointerInSharedCache(void *ptr, uintptr_t *cacheOffset)
   {
#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
#if defined(J9VM_OPT_MULTI_LAYER_SHARED_CLASS_CACHE)
   uintptr_t offset = 0;
   // The cache descriptor list is linked last to first and is circular, so last->previous == first.
   J9SharedClassCacheDescriptor *firstCache = getCacheDescriptorList()->previous;
   J9SharedClassCacheDescriptor *curCache = firstCache;
   do
      {
      if (isPointerInMetadataSectionInCache(curCache, ptr))
         {
         if (cacheOffset)
            {
            uintptr_t metadataStartAddress = reinterpret_cast<uintptr_t>(curCache->metadataStartAddress);
            *cacheOffset = encodeOffsetFromEnd(metadataStartAddress - reinterpret_cast<uintptr_t>(ptr) + offset);
            }
         return true;
         }
      offset += curCache->cacheSizeBytes;
      curCache = curCache->previous;
      }
   while (curCache != firstCache);
#else // !J9VM_OPT_MULTI_LAYER_SHARED_CLASS_CACHE
   J9SharedClassCacheDescriptor *curCache = getCacheDescriptorList();
   if (isPointerInMetadataSectionInCache(curCache, ptr))
      {
      if (cacheOffset)
         {
         uintptr_t metadataStartAddress = reinterpret_cast<uintptr_t>(curCache->metadataStartAddress);
         *cacheOffset = encodeOffsetFromEnd(metadataStartAddress - reinterpret_cast<uintptr_t>(ptr));
         }
      return true;
      }
#endif // J9VM_OPT_MULTI_LAYER_SHARED_CLASS_CACHE
#endif
   return false;
   }

bool
TR_J9SharedCache::isROMStructureInSharedCache(void *romStructure, uintptr_t *cacheOffset)
   {
#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
#if defined(J9VM_OPT_MULTI_LAYER_SHARED_CLASS_CACHE)
   uintptr_t offset = 0;
   // The cache descriptor list is linked last to first and is circular, so last->previous == first.
   J9SharedClassCacheDescriptor *firstCache = getCacheDescriptorList()->previous;
   J9SharedClassCacheDescriptor *curCache = firstCache;
   do
      {
      if (isPointerInROMClassesSectionInCache(curCache, romStructure))
         {
         if (cacheOffset)
            {
            uintptr_t romclassStartAddress = reinterpret_cast<uintptr_t>(curCache->romclassStartAddress);
            *cacheOffset = encodeOffsetFromStart(reinterpret_cast<uintptr_t>(romStructure) - romclassStartAddress + offset);
            }
         return true;
         }
      offset += curCache->cacheSizeBytes;
      curCache = curCache->previous;
      }
   while (curCache != firstCache);
#else // !J9VM_OPT_MULTI_LAYER_SHARED_CLASS_CACHE
   J9SharedClassCacheDescriptor *curCache = getCacheDescriptorList();
   if (isPointerInROMClassesSectionInCache(curCache, romStructure))
      {
      if (cacheOffset)
         {
         uintptr_t romclassStartAddress = reinterpret_cast<uintptr_t>(curCache->romclassStartAddress);
         *cacheOffset = encodeOffsetFromStart(reinterpret_cast<uintptr_t>(romStructure) - romclassStartAddress);
         }
      return true;
      }
#endif // J9VM_OPT_MULTI_LAYER_SHARED_CLASS_CACHE
#endif
   return false;
   }

bool
TR_J9SharedCache::isROMClassInSharedCache(J9ROMClass *romClass, uintptr_t *cacheOffset)
   {
   return isROMStructureInSharedCache(romClass, cacheOffset);
   }

bool
TR_J9SharedCache::isROMMethodInSharedCache(J9ROMMethod *romMethod, uintptr_t *cacheOffset)
   {
   return isROMStructureInSharedCache(romMethod, cacheOffset);
   }

bool
TR_J9SharedCache::isPtrToROMClassesSectionInSharedCache(void *ptr, uintptr_t *cacheOffset)
   {
   return isROMStructureInSharedCache(ptr, cacheOffset);
   }

J9ROMClass *
TR_J9SharedCache::startingROMClassOfClassChain(UDATA *classChain)
   {
   UDATA lengthInBytes = classChain[0];
   TR_ASSERT_FATAL(lengthInBytes >= 2 * sizeof (UDATA), "class chain is too short!");

   UDATA romClassOffset = classChain[1];
   return romClassFromOffsetInSharedCache(romClassOffset);
   }

// convert an offset into a string of 8 characters
void
TR_J9SharedCache::convertUnsignedOffsetToASCII(UDATA offset, char *buffer)
   {
   for (int i = _numDigitsForCacheOffsets; i >= 0; i--, offset >>= 4)
      {
      uint8_t lowNibble = offset & 0xf;
      buffer[i] = (lowNibble > 9 ?  lowNibble - 10 + 'a' :  lowNibble + '0');
      }
   buffer[_numDigitsForCacheOffsets] = 0;
   TR_ASSERT(offset == 0, "Unsigned offset unexpectedly not fully converted to ASCII");
   }

void
TR_J9SharedCache::createClassKey(UDATA classOffsetInCache, char *key, uint32_t & keyLength)
   {
   keyLength = _numDigitsForCacheOffsets;
   convertUnsignedOffsetToASCII(classOffsetInCache, key);
   }

UDATA *
TR_J9SharedCache::rememberClass(J9Class *clazz, bool create)
   {
   UDATA *chainData = NULL;
#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   J9ROMClass *romClass = TR::Compiler->cls.romClassOf(fej9->convertClassPtrToClassOffset(clazz));

   J9UTF8 * className = J9ROMCLASS_CLASSNAME(romClass);
   LOG(1, "rememberClass class %p romClass %p %.*s\n", clazz, romClass, J9UTF8_LENGTH(className), J9UTF8_DATA(className));

   uintptr_t classOffsetInCache;
   if (!isROMClassInSharedCache(romClass, &classOffsetInCache))
      {
      LOG(1,"\trom class not in shared cache, returning\n");
      return NULL;
      }

   char key[17]; // longest possible key length is way less than 16 digits
   uint32_t keyLength;
   createClassKey(classOffsetInCache, key, keyLength);

   LOG(3, "\tkey created: %.*s\n", keyLength, key);

   chainData = findChainForClass(clazz, key, keyLength);
   if (chainData != NULL)
      {
      LOG(1, "\tchain exists (%p) so nothing to store\n", chainData);
      return chainData;
      }

   int32_t numSuperclasses = TR::Compiler->cls.classDepthOf(fe()->convertClassPtrToClassOffset(clazz));
   int32_t numInterfaces = numInterfacesImplemented(clazz);

   LOG(3, "\tcreating chain now: 1 + 1 + %d superclasses + %d interfaces\n", numSuperclasses, numInterfaces);
   UDATA chainLength = (2 + numSuperclasses + numInterfaces) * sizeof(UDATA);
   const uint32_t maxChainLength = 32;
   UDATA typicalChainData[maxChainLength];
   chainData = typicalChainData;
   if (chainLength > maxChainLength*sizeof(UDATA))
      {
      LOG(1, "\t\t > %d so bailing\n", maxChainLength);
      return NULL;
      }

   if (!fillInClassChain(clazz, chainData, chainLength, numSuperclasses, numInterfaces))
      {
      LOG(1, "\tfillInClassChain failed, bailing\n");
      return NULL;
      }

   if (!create)
      {
      LOG(1, "\tnot asked to create but could create, returning non-null\n");
      return (UDATA *) 0x1;
      }

   UDATA chainDataLength = chainData[0];

   J9SharedDataDescriptor dataDescriptor;
   dataDescriptor.address = (U_8*)chainData;
   dataDescriptor.length  = chainDataLength;
   dataDescriptor.type    = J9SHR_DATA_TYPE_AOTCLASSCHAIN;
   dataDescriptor.flags   = J9SHRDATA_SINGLE_STORE_FOR_KEY_TYPE;

   if (aotStats())
      aotStats()->numNewCHEntriesInSharedClass++;

   J9VMThread *vmThread = fej9->getCurrentVMThread();
   chainData = (UDATA *) sharedCacheConfig()->storeSharedData(vmThread,
                                                             (const char*)key,
                                                             keyLength,
                                                             &dataDescriptor);
   if (chainData)
      {
      LOG(1, "\tstored data, chain at %p\n", chainData);
      }
   else
      {
      LOG(1, "\tunable to store chain\n");
      TR::Options::getAOTCmdLineOptions()->setOption(TR_NoStoreAOT);

      setSharedCacheDisabledReason(SHARED_CACHE_CLASS_CHAIN_STORE_FAILED);
      setStoreSharedDataFailedLength(chainDataLength);
      }
#endif
   return chainData;
   }

UDATA
TR_J9SharedCache::rememberDebugCounterName(const char *name)
   {
   UDATA offset = 0;
#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   J9VMThread *vmThread = fej9->getCurrentVMThread();

   J9SharedDataDescriptor dataDescriptor;
   dataDescriptor.address = (U_8*)name;
   dataDescriptor.length  = (strlen(name) + 1); // +1 for the \0 terminator
   dataDescriptor.type    = J9SHR_DATA_TYPE_JITHINT;
   dataDescriptor.flags   = J9SHRDATA_NOT_INDEXED;

   const U_8 *data = sharedCacheConfig()->storeSharedData(vmThread,
                                        (const char*)NULL,
                                        0,
                                        &dataDescriptor);

   offset = data ? offsetInSharedCacheFromPointer((void *)data) : (UDATA)-1;

   //printf("\nrememberDebugCounterName: Tried to store %s (%p), data=%p, offset=%p\n", name, name, data, offset);
#endif
   return offset;
   }

const char *
TR_J9SharedCache::getDebugCounterName(UDATA offset)
   {
   const char *name = (offset != (UDATA)-1) ? (const char *)pointerFromOffsetInSharedCache(offset) : NULL;

   //printf("\ngetDebugCounterName: Tried to find %p, name=%s (%p)\n", offset, (name ? name : ""), name);

   return name;
   }

uint32_t
TR_J9SharedCache::numInterfacesImplemented(J9Class *clazz)
   {
   uint32_t count=0;
   J9ITable *element = TR::Compiler->cls.iTableOf(fe()->convertClassPtrToClassOffset(clazz));
   while (element != NULL)
      {
      count++;
      element = TR::Compiler->cls.iTableNext(element);
      }
   return count;
   }

bool
TR_J9SharedCache::writeClassToChain(J9ROMClass *romClass, UDATA * & chainPtr)
   {
   uintptr_t classOffsetInCache;
   if (!isROMClassInSharedCache(romClass, &classOffsetInCache))
      {
      LOG(3, "\t\tromclass %p not in shared cache, writeClassToChain returning false\n", romClass);
      return false;
      }

   J9UTF8 * className = J9ROMCLASS_CLASSNAME(romClass);
   LOG(3, "\t\tChain %p storing romclass %p (%.*s) offset %d\n", chainPtr, romClass, J9UTF8_LENGTH(className), J9UTF8_DATA(className), classOffsetInCache);
   *chainPtr++ = classOffsetInCache;
   return true;
   }

bool
TR_J9SharedCache::writeClassesToChain(J9Class *clazz, int32_t numSuperclasses, UDATA * & chainPtr)
   {
   LOG(3, "\t\twriteClassesToChain:\n");

   for (int32_t index=0; index < numSuperclasses;index++)
      {
      J9ROMClass *romClass = TR::Compiler->cls.romClassOfSuperClass(fe()->convertClassPtrToClassOffset(clazz), index);
      if (!writeClassToChain(romClass, chainPtr))
         return false;
      }
   return true;
   }

bool
TR_J9SharedCache::writeInterfacesToChain(J9Class *clazz, UDATA * & chainPtr)
   {
   LOG(3, "\t\twriteInterfacesToChain:\n");

   J9ITable *element = TR::Compiler->cls.iTableOf(fe()->convertClassPtrToClassOffset(clazz));
   while (element != NULL)
      {
      J9ROMClass *romClass = TR::Compiler->cls.iTableRomClass(element);
      if (!writeClassToChain(romClass, chainPtr))
         return false;

      element = TR::Compiler->cls.iTableNext(element);
      }

   return true;
   }

bool
TR_J9SharedCache::fillInClassChain(J9Class *clazz, UDATA *chainData, uint32_t chainLength,
                                   uint32_t numSuperclasses, uint32_t numInterfaces)
   {
   LOG(3, "\t\tChain %p store chainLength %d\n", chainData, chainLength);

   UDATA *chainPtr = chainData;
   *chainPtr++ = chainLength;
   J9ROMClass* romClass = TR::Compiler->cls.romClassOf(fe()->convertClassPtrToClassOffset(clazz));
   writeClassToChain(romClass, chainPtr);
   if (!writeClassesToChain(clazz, numSuperclasses, chainPtr))
      {
      return false;
      }

   if (!writeInterfacesToChain(clazz, chainPtr))
      {
      return false;
      }

   LOG(3, "\t\tfillInClassChain returning true\n");
   return chainData;
   }


bool
TR_J9SharedCache::romclassMatchesCachedVersion(J9ROMClass *romClass, UDATA * & chainPtr, UDATA *chainEnd)
   {
   J9UTF8 * className = J9ROMCLASS_CLASSNAME(romClass);
   UDATA romClassOffset;
   if (!isROMClassInSharedCache(romClass, &romClassOffset))
      return false;
   LOG(3, "\t\tExamining romclass %p (%.*s) offset %d, comparing to %d\n", romClass, J9UTF8_LENGTH(className), J9UTF8_DATA(className), romClassOffset, *chainPtr);
   if ((chainPtr > chainEnd) || (romClassOffset != *chainPtr++))
      return false;
   return true;
   }

UDATA *
TR_J9SharedCache::findChainForClass(J9Class *clazz, const char *key, uint32_t keyLength)
   {
   UDATA * chainForClass = NULL;
#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   J9SharedDataDescriptor dataDescriptor;
   dataDescriptor.address = NULL;
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());

   J9VMThread *vmThread = fej9->getCurrentVMThread();
   sharedCacheConfig()->findSharedData(vmThread,
                                       key,
                                       keyLength,
                                       J9SHR_DATA_TYPE_AOTCLASSCHAIN,
                                       FALSE,
                                       &dataDescriptor,
                                       NULL);

   //fprintf(stderr,"findChainForClass: key %.*s chain %p\n", keyLength, key, dataDescriptor.address);
   chainForClass = (UDATA *) dataDescriptor.address;
#endif
   return chainForClass;
   }

bool
TR_J9SharedCache::validateSuperClassesInClassChain(TR_OpaqueClassBlock *clazz, UDATA * & chainPtr, UDATA *chainEnd)
   {
   int32_t numSuperclasses = TR::Compiler->cls.classDepthOf(clazz);
   for (int32_t index=0; index < numSuperclasses; index++)
      {
      J9ROMClass *romClass = TR::Compiler->cls.romClassOfSuperClass(clazz, index);
      if (!romclassMatchesCachedVersion(romClass, chainPtr, chainEnd))
         {
         LOG(1, "\tClass in hierarchy did not match, returning false\n");
         return false;
         }
      }
   return true;
   }

bool
TR_J9SharedCache::validateInterfacesInClassChain(TR_OpaqueClassBlock *clazz, UDATA * & chainPtr, UDATA *chainEnd)
   {
   J9ITable *interfaceElement = TR::Compiler->cls.iTableOf(clazz);
   while (interfaceElement)
      {
      J9ROMClass * romClass = TR::Compiler->cls.iTableRomClass(interfaceElement);
      if (!romclassMatchesCachedVersion(romClass, chainPtr, chainEnd))
         {
         LOG(1, "\tInterface class did not match, returning false\n");
         return false;
         }
      interfaceElement = TR::Compiler->cls.iTableNext(interfaceElement);
      }
   return true;
   }

bool
TR_J9SharedCache::validateClassChain(J9ROMClass *romClass, TR_OpaqueClassBlock *clazz, UDATA * & chainPtr, UDATA *chainEnd)
   {
   bool validationSucceeded = false;

   if (!romclassMatchesCachedVersion(romClass, chainPtr, chainEnd))
      {
      LOG(1, "\tClass did not match, returning false\n");
      }
   else if (!validateSuperClassesInClassChain(clazz, chainPtr, chainEnd))
      {
      LOG(1, "\tClass in hierarchy did not match, returning false\n");
      }
   else if (!validateInterfacesInClassChain(clazz, chainPtr, chainEnd))
      {
      LOG(1, "\tInterface class did not match, returning false\n");
      }
   else if (chainPtr != chainEnd)
      {
      LOG(1, "\tfinished classes and interfaces, but not at chain end, returning false\n");
      }
   else
      {
      validationSucceeded = true;
      }

   return validationSucceeded;
   }

bool
TR_J9SharedCache::classMatchesCachedVersion(J9Class *clazz, UDATA *chainData)
   {
   J9ROMClass *romClass = TR::Compiler->cls.romClassOf(fe()->convertClassPtrToClassOffset(clazz));
   J9UTF8 * className = J9ROMCLASS_CLASSNAME(romClass);
   LOG(1, "classMatchesCachedVersion class %p %.*s\n", clazz, J9UTF8_LENGTH(className), J9UTF8_DATA(className));

   uintptr_t classOffsetInCache;

   /* If the pointer isn't the SCC, then return false immmediately
    * as the map holds offsets into the SCC of romclasses
    */
   if (!isROMClassInSharedCache(romClass, &classOffsetInCache))
      {
      LOG(1, "\tclass not in shared cache, returning false\n");
      return false;
      }

   /* Check if the validation of the class chain was previously
    * performed; if so, return the result of that validation
    */
   if (TR::Options::getAOTCmdLineOptions()->getOption(TR_EnableClassChainValidationCaching))
      {
      auto result = getCachedCCVResult(reinterpret_cast<TR_OpaqueClassBlock *>(clazz));
      if (result == CCVResult::success)
         {
         LOG(1, "\tcached result: validation succeeded\n");
         return true;
         }
      else if (result == CCVResult::failure)
         {
         LOG(1, "\tcached result: validation failed\n");
         return false;
         }
      else
         {
         TR_ASSERT_FATAL(result == CCVResult::notYetValidated, "Unknown result cached %d\n", result);
         }
      }

   /* If the chainData passed in is NULL, try to find it in the SCC
    * using the romclass
    */
   if (chainData == NULL)
      {
      char key[17]; // longest possible key length is way less than 16 digits
      uint32_t keyLength;
      createClassKey(classOffsetInCache, key, keyLength);
      LOG(3, "\tno chain specific, so looking up for key %.*s\n", keyLength, key);
      chainData = findChainForClass(clazz, key, keyLength);
      }

   /* If the chainData is still NULL, cache the result as failure
    * and return false
    */
   if (chainData == NULL)
      {
      LOG(1, "\tno stored chain, returning false\n");
      if (TR::Options::getAOTCmdLineOptions()->getOption(TR_EnableClassChainValidationCaching))
         cacheCCVResult(reinterpret_cast<TR_OpaqueClassBlock *>(clazz), CCVResult::failure);

      return false;
      }

   UDATA *chainPtr = chainData;
   UDATA chainLength = *chainPtr++;
   UDATA *chainEnd = (UDATA *) (((U_8*)chainData) + chainLength);
   LOG(3, "\tfound chain: %p with length %d\n", chainData, chainLength);

   /* Perform class chain validation */
   bool success = validateClassChain(romClass, fe()->convertClassPtrToClassOffset(clazz), chainPtr, chainEnd);

   /* Cache the result of the validation */
   if (TR::Options::getAOTCmdLineOptions()->getOption(TR_EnableClassChainValidationCaching))
      {
      auto result = success ? CCVResult::success : CCVResult::failure;
      cacheCCVResult(reinterpret_cast<TR_OpaqueClassBlock *>(clazz), result);
      }

   if (success)
      LOG(1, "\tMatch!  return true\n");

   return success;
   }

TR_OpaqueClassBlock *
TR_J9SharedCache::lookupClassFromChainAndLoader(uintptr_t *chainData, void *classLoader)
   {
   UDATA *ptrToRomClassOffset = chainData+1;
   J9ROMClass *romClass = romClassFromOffsetInSharedCache(*ptrToRomClassOffset);
   J9UTF8 *className = J9ROMCLASS_CLASSNAME(romClass);
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   J9VMThread *vmThread = fej9->getCurrentVMThread();
   J9Class *clazz = jitGetClassInClassloaderFromUTF8(vmThread, (J9ClassLoader *) classLoader,
                                                     (char *) J9UTF8_DATA(className),
                                                     J9UTF8_LENGTH(className));

   if (clazz != NULL && classMatchesCachedVersion(clazz, chainData))
      return (TR_OpaqueClassBlock *) clazz;

   return NULL;
   }

uintptr_t
TR_J9SharedCache::getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCache(TR_OpaqueClassBlock *clazz)
   {
   void *loaderForClazz = _fe->getClassLoader(clazz);
   void *classChainIdentifyingLoaderForClazz = persistentClassLoaderTable()->lookupClassChainAssociatedWithClassLoader(loaderForClazz);

   uintptr_t classChainOffsetInSharedCache;
   TR::Compilation *comp = TR::comp();
   if (comp)
      {
      /*
       * TR_J9SharedCache::offsetInSharedCacheFrom* asserts if the pointer
       * passed in does not exist in the SCC. Under HCR, when an agent redefines
       * a class, it causes the J9Class pointer to stay the same, but the
       * J9ROMClass pointer changes. This means that if the compiler has a
       * reference to a J9Class who J9ROMClass was in the SCC at one point in the
       * compilation, it may no longer be so at another point in the compilation.
       *
       * This means that the compilation is no longer valid and should be aborted.
       * Even if there isn't an abort during the compilation, at the end of the
       * compilation, the compiler will fail the compile if such a redefinition
       * occurred.
       *
       * Calling TR_J9SharedCache::offsetInSharedCacheFromPointer after such a
       * redefinition could result in an assert. Therefore, this method exists as
       * a wrapper around TR_J9SharedCache::isPointerInSharedCache which doesn't
       * assert and conveniently, updates the location referred to by the cacheOffset
       * pointer passed in as a parameter.
       *
       * If the ptr isn't in the the SCC, then the current method will abort the
       * compilation. If the ptr is in the SCC, then the cacheOffset will be updated.
       */
      if (!isPointerInSharedCache(classChainIdentifyingLoaderForClazz, &classChainOffsetInSharedCache))
         comp->failCompilation<J9::ClassChainPersistenceFailure>("Failed to find pointer %p in SCC", classChainIdentifyingLoaderForClazz);
      }
   else
      {
      /*
       * If we're not in a compilation, then perhaps it's better to call this API
       * which will assert if anything's amiss
       */
      classChainOffsetInSharedCache = offsetInSharedCacheFromPointer(classChainIdentifyingLoaderForClazz);
      }

   return classChainOffsetInSharedCache;
   }

#if defined(J9VM_OPT_JITSERVER)
uintptr_t
TR_J9SharedCache::getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCacheNoFail(TR_OpaqueClassBlock *clazz)
   {
   TR_ASSERT_FATAL(TR::comp() && !TR::comp()->isOutOfProcessCompilation(), "getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCacheNoFail should be called only the JVM client");
   void *loaderForClazz = _fe->getClassLoader(clazz);
   void *classChainIdentifyingLoaderForClazz = persistentClassLoaderTable()->lookupClassChainAssociatedWithClassLoader(loaderForClazz);
   uintptr_t classChainOffsetInSharedCache;
   if (!isPointerInSharedCache(classChainIdentifyingLoaderForClazz, &classChainOffsetInSharedCache))
      return 0;

   return classChainOffsetInSharedCache;
   }
#endif // defined(J9VM_OPT_JITSERVER)

const void *
TR_J9SharedCache::storeSharedData(J9VMThread *vmThread, char *key, J9SharedDataDescriptor *descriptor)
   {
#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
   return _sharedCacheConfig->storeSharedData(
         vmThread,
         key,
         strlen(key),
         descriptor);
#else
   return NULL;
#endif
   }

#if defined(J9VM_OPT_JITSERVER)
TR_J9JITServerSharedCache::TR_J9JITServerSharedCache(TR_J9VMBase *fe)
   : TR_J9SharedCache(fe)
   {
   _stream = NULL;
   }

UDATA *
TR_J9JITServerSharedCache::rememberClass(J9Class *clazz, bool create)
   {
   TR_ASSERT(_stream, "stream must be initialized by now");
   auto clientData = TR::compInfoPT->getClientData();
   PersistentUnorderedMap<J9Class *, UDATA *> & cache = clientData->getClassChainDataCache();
      {
      OMR::CriticalSection classChainDataMapMonitor(clientData->getClassChainDataMapMonitor());
      auto it = cache.find(clazz);
      if (it != cache.end())
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Chain exists (%p) so nothing to store \n", it->second);
         return it->second;
         }
      }
   _stream->write(JITServer::MessageType::SharedCache_rememberClass, clazz, create);
   UDATA * chainData = std::get<0>(_stream->read<UDATA *>());
   if (chainData)
      {
      if (!create)
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "not asked to create but could create, returning non-null \n");
         }
      else
         {
         OMR::CriticalSection classChainDataMapMonitor(clientData->getClassChainDataMapMonitor());
         cache.insert(std::make_pair(clazz, chainData));
         }
      }
   return chainData;
   }

J9SharedClassCacheDescriptor *
TR_J9JITServerSharedCache::getCacheDescriptorList()
   {
   auto *vmInfo = TR::compInfoPT->getClientData()->getOrCacheVMInfo(_stream);
   return vmInfo->_j9SharedClassCacheDescriptorList;
   }

uintptr_t
TR_J9JITServerSharedCache::getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCache(TR_OpaqueClassBlock *clazz)
   {
   uintptr_t  classChainOffset = 0;
   TR_ASSERT(_stream, "stream must be initialized by now");
   ClientSessionData *clientData = TR::compInfoPT->getClientData();
   JITServerHelpers::getAndCacheRAMClassInfo((J9Class *)clazz, clientData, _stream,
                                             JITServerHelpers::CLASSINFO_CLASS_CHAIN_OFFSET, (void *)&classChainOffset);
   // Test if cached value of `classChainOffset` is initialized. Ask the client if it's not.
   // This situation is possible if we cache ClassInfo during a non-AOT compilation.
   if (classChainOffset == 0)
      {
      _stream->write(JITServer::MessageType::SharedCache_getClassChainOffsetInSharedCache, clazz);
      classChainOffset = std::get<0>(_stream->read<uintptr_t>());

      // If we got a valid value back, cache that
      if (classChainOffset)
         {
         OMR::CriticalSection getRemoteROMClass(clientData->getROMMapMonitor());
         auto it = clientData->getROMClassMap().find((J9Class*) clazz);
         if (it != clientData->getROMClassMap().end())
            {
            it->second._classChainOffsetOfIdentifyingLoaderForClazz = classChainOffset;
            }
         }
      }
   return classChainOffset;
   }

void
TR_J9JITServerSharedCache::addHint(J9Method * method, TR_SharedCacheHint theHint)
   {
   TR_ASSERT(_stream, "stream must be initialized by now");
   auto *vmInfo = TR::compInfoPT->getClientData()->getOrCacheVMInfo(_stream);
   if (vmInfo->_hasSharedClassCache)
      {
      _stream->write(JITServer::MessageType::SharedCache_addHint, method, theHint);
      _stream->read<JITServer::Void>();
      }
   }

const void *
TR_J9JITServerSharedCache::storeSharedData(J9VMThread *vmThread, char *key, J9SharedDataDescriptor *descriptor)
   {
   TR_ASSERT(_stream, "stream must be initialized by now");
   std::string dataStr((char *) descriptor->address, descriptor->length);

   _stream->write(JITServer::MessageType::SharedCache_storeSharedData, std::string(key, strlen(key)), *descriptor, dataStr);
   return std::get<0>(_stream->read<const void *>());
   }

#endif
