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

#include "env/J9SharedCache.hpp"

#include <algorithm>
#include "j9cfg.h"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/jittypes.h"
#include "env/VMAccessCriticalSection.hpp"
#include "runtime/CodeRuntime.hpp"
#include "control/CompilationRuntime.hpp"
#include "env/VMJ9.h"
#include "env/j9method.h"
#include "runtime/IProfiler.hpp"
#include "env/ClassLoaderTable.hpp"

#define   LOG(n,c) \
   if (_logLevel >= (3*n)) \
      {\
      c;\
      }

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


TR_J9SharedCache::TR_J9SharedCache(TR_J9VMBase *fe)
   {
   _fe = fe;
   _jitConfig = fe->getJ9JITConfig();
   _javaVM = _jitConfig->javaVM;
   _compInfo = TR::CompilationInfo::get(_jitConfig);
   _aotStats = fe->getPrivateConfig()->aotStats;
   _sharedCacheConfig = _javaVM->sharedClassConfig;
   _cacheStartAddress = (UDATA) _sharedCacheConfig->cacheDescriptorList->cacheStartAddress;
   _cacheSizeInBytes = _sharedCacheConfig->cacheDescriptorList->cacheSizeBytes;
   _numDigitsForCacheOffsets=8;
   if (_cacheSizeInBytes > (UDATA)0xFFFFFFFF)
      _numDigitsForCacheOffsets=16;

   _hintsEnabledMask = 0;
   if (!TR::Options::getAOTCmdLineOptions()->getOption(TR_DisableSharedCacheHints))
      _hintsEnabledMask = TR::Options::getAOTCmdLineOptions()->getEnableSCHintFlags();

   _initialHintSCount = std::min(TR::Options::getCmdLineOptions()->getInitialSCount(), TR::Options::getAOTCmdLineOptions()->getInitialSCount());
   if (_initialHintSCount == 0)
      _initialHintSCount = 1;


   _logLevel = std::max(TR::Options::getAOTCmdLineOptions()->getAotrtDebugLevel(), TR::Options::getCmdLineOptions()->getAotrtDebugLevel());

   _verboseHints = TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseSCHints);

   LOG(5, { log("\t_sharedCacheConfig %p\n", _sharedCacheConfig); });
   LOG(5, { log("\t_cacheStartAddress %p\n", _cacheStartAddress); });
   LOG(5, { log("\t_cacheSizeInBytes %p\n", _cacheSizeInBytes); });
   LOG(5, { log("\tlast cache address %p\n", _cacheStartAddress + _cacheSizeInBytes); });
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


uint32_t
TR_J9SharedCache::getHint(J9VMThread * vmThread, J9Method *method)
   {
   uint32_t result = 0;

#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
   J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);

   unsigned char storeBuffer[4];
   uint32_t bufferLength = 4;
   J9SharedDataDescriptor descriptor;
   descriptor.address = storeBuffer;
   descriptor.length = bufferLength;
   descriptor.type = J9SHR_ATTACHED_DATA_TYPE_JITHINT;
   descriptor.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;
   IDATA dataIsCorrupt;
   uint32_t *find = (uint32_t *)sharedCacheConfig()->findAttachedData(vmThread, romMethod, &descriptor, &dataIsCorrupt);

   if ((find == (uint32_t *)descriptor.address) && (dataIsCorrupt == -1))
      result = *find;
#endif // defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))

   return result;
   }

uint16_t
TR_J9SharedCache::getAllEnabledHints(J9Method *method)
   {
   uint16_t hintFlags = 0;
#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
   if (_hintsEnabledMask)
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      J9VMThread * vmThread = fej9->getCurrentVMThread();
      uint32_t scHints = getHint(vmThread, method);
      hintFlags = *((uint16_t *)&scHints) & _hintsEnabledMask;
      }
#endif // defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
   return hintFlags;
   }


bool
TR_J9SharedCache::isHint(J9Method *method, TR_SharedCacheHint theHint, uint16_t *dataField)
   {
   bool isHint = false;

#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
   uint16_t hint = ((uint16_t)theHint) & _hintsEnabledMask;
   if (hint != 0)
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
      J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
      J9VMThread * vmThread = fej9->getCurrentVMThread();
      uint32_t scHints = getHint(vmThread, method);
      uint16_t hintFlags = *((uint16_t *)&scHints);

      if (dataField)
         *dataField = *(((uint16_t *)&scHints) + 1);

      if (_verboseHints)
         {
         char methodSignature[500];
         int32_t maxSignatureLength = 500;
         fej9->printTruncatedSignature(methodSignature, maxSignatureLength, (TR_OpaqueMethodBlock *) method);
         TR_VerboseLog::writeLineLocked(TR_Vlog_SCHINTS,"is hint %x(%x) %s", hintFlags, hint, methodSignature);
         }
      isHint = (hintFlags & hint) != 0;
      }
#endif // defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390))
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
#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
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

      uint32_t scHintData = getHint(vmThread, method);
      uint16_t *hintFlags = (uint16_t *)&scHintData;
      uint16_t *hintCount = ((uint16_t *)&scHintData) + 1; // Flags and new count field needs to be in contiguous location to be stored into sharecache

      bool hintDidNotExist = ((newHint & *hintFlags) == 0);
      const uint32_t scHintDataLength = 4;

      if (hintDidNotExist)
         {
         uint32_t bytesToPersist = 0;

         if (!SCfull)
            {
            if (isFailedValidationHint)
               *hintCount = 10 * _initialHintSCount;

            J9SharedDataDescriptor descriptor;
            descriptor.address = (U_8 *)(uintptrj_t)scHintData;
            descriptor.length = scHintDataLength; // Size includes the 2nd data field, currently only used for TR_HintFailedValidation
            descriptor.type = J9SHR_ATTACHED_DATA_TYPE_JITHINT;
            descriptor.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;
            UDATA store = sharedCacheConfig()->storeAttachedData(vmThread, romMethod, &descriptor, 0);
            TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig());
            if (store == 0)
               {
               if (_verboseHints)
                  TR_VerboseLog::writeLineLocked(TR_Vlog_SCHINTS,"hint added 0x%x, key = %s, scount: %d", *hintFlags, methodSignature, *hintCount);
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
      else
         {
         bool updateHint = true;
         if (isFailedValidationHint)
            {
            uint16_t oldCount = *hintCount;
            uint16_t newCount = std::min(oldCount * 10, TR_DEFAULT_INITIAL_COUNT);
            if (newCount == oldCount)
               {
               updateHint = false;
               if (_verboseHints)
                  {
                  TR_VerboseLog::writeLineLocked(TR_Vlog_SCHINTS,"hint reached max count of %d", oldCount);
                  }
               }
            }

         if (updateHint)
            {
            J9SharedDataDescriptor descriptor;
            descriptor.address = (U_8 *)(uintptrj_t)scHintData;
            descriptor.length = scHintDataLength; // Size includes the 2nd data field, currently only used for TR_HintFailedValidation
            descriptor.type = J9SHR_ATTACHED_DATA_TYPE_JITHINT;
            descriptor.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;
            UDATA update = sharedCacheConfig()->updateAttachedData(vmThread, romMethod, 0, &descriptor);

            if (_verboseHints)
               {
               if (update == 0)
                  {
                  TR_VerboseLog::writeLineLocked(TR_Vlog_SCHINTS,"hint updated 0x%x, key = %s, scount: %d", *hintFlags, methodSignature, *hintCount);
                  }
               else
                  {
                  TR_VerboseLog::writeLineLocked(TR_Vlog_SCHINTS,"hint error: could not be updated into SC\n");
                  }
               }
            }
         }
      }

#endif // defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390))
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

J9Class *
TR_J9SharedCache::matchRAMclassFromROMclass(J9ROMClass * clazz, TR::Compilation * comp)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   J9VMThread *vmThread = fej9->getCurrentVMThread();
   J9UTF8 *className = J9ROMCLASS_CLASSNAME(clazz);
   J9Class *ramClass = NULL;

      {
      TR::VMAccessCriticalSection matchRAMclassFromROMclass(fej9);
      ramClass = jitGetClassInClassloaderFromUTF8(vmThread,
                                                  ((TR_ResolvedJ9Method *)comp->getCurrentMethod())->getClassLoader(),
                                                  (char *) J9UTF8_DATA(className),
                                                  J9UTF8_LENGTH(className));
      if (!ramClass)
         {
         ramClass = jitGetClassInClassloaderFromUTF8(vmThread, (J9ClassLoader *) javaVM()->systemClassLoader,
            (char *) J9UTF8_DATA(className), J9UTF8_LENGTH(className));
         }
      }

   return ramClass;
   }

void *
TR_J9SharedCache::pointerFromOffsetInSharedCache(void *offset)
   {
   return (void *) ((UDATA)offset + _cacheStartAddress);
   }

void *
TR_J9SharedCache::offsetInSharedCacheFromPointer(void *ptr)
   {
   return (void *) ((UDATA)ptr - _cacheStartAddress);
   }

bool
TR_J9SharedCache::isPointerInSharedCache(void *ptr, void * & cacheOffset)
   {
   UDATA offset = (UDATA) offsetInSharedCacheFromPointer(ptr);
   if (offset < _cacheSizeInBytes)
      {
      cacheOffset = (void*)offset;
      return true;
      }
   return false;
   }

J9ROMClass *
TR_J9SharedCache::startingROMClassOfClassChain(UDATA *classChain)
   {
   UDATA lengthInBytes = classChain[0];
   TR_ASSERT_FATAL(lengthInBytes >= 2 * sizeof (UDATA), "class chain is too short!");

   void *romClassOffset = reinterpret_cast<void*>(classChain[1]);
   void *romClass = pointerFromOffsetInSharedCache(romClassOffset);
   return static_cast<J9ROMClass*>(romClass);
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
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   J9UTF8 * className = J9ROMCLASS_CLASSNAME(clazz->romClass);
   LOG(5,{ log("rememberClass class %p %.*s\n", clazz, J9UTF8_LENGTH(className), J9UTF8_DATA(className)); });

   void * classOffsetInCache;
   if (! isPointerInSharedCache(clazz->romClass, classOffsetInCache))
      {
      LOG(5,{ log("\trom class not in shared cache, returning\n"); });
      return NULL;
      }

   char key[17]; // longest possible key length is way less than 16 digits
   uint32_t keyLength;
   createClassKey((UDATA)classOffsetInCache, key, keyLength);

   LOG(9, { log("\tkey created: %.*s\n", keyLength, key); });

   UDATA *chainData = findChainForClass(clazz, key, keyLength);
   if (chainData != NULL)
      {
      LOG(5, { log("\tchain exists (%p) so nothing to store\n", chainData); });
      return chainData;
      }

   int32_t numSuperclasses =  J9CLASS_DEPTH(clazz);
   int32_t numInterfaces = numInterfacesImplemented(clazz);

   LOG(9, { log("\tcreating chain now: 1 + 1 + %d superclasses + %d interfaces\n", numSuperclasses, numInterfaces); });
   UDATA chainLength = (2 + numSuperclasses + numInterfaces) * sizeof(UDATA);
   const uint32_t maxChainLength = 32;
   UDATA typicalChainData[maxChainLength];
   chainData = typicalChainData;
   if (chainLength > maxChainLength*sizeof(UDATA))
      {
      LOG(5, { log("\t\t > %d so bailing\n", maxChainLength); });
      return NULL;
      }

   if (! fillInClassChain(clazz, chainData, chainLength, numSuperclasses, numInterfaces))
      {
      LOG(5, { log("\tfillInClassChain failed, bailing\n"); });
      return NULL;
      }

   if (!create)
      {
      LOG(5, { log("\tnot asked to create but could create, returning non-null\n"); });
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
      LOG(5, { log("\tstored data, chain at %p\n", chainData); });
      }
   else
      {
      LOG(5, { log("\tunable to store chain\n"); });
      TR::Options::getAOTCmdLineOptions()->setOption(TR_NoStoreAOT);

      setSharedCacheDisabledReason(SHARED_CACHE_CLASS_CHAIN_STORE_FAILED);
      setStoreSharedDataFailedLength(chainDataLength);
      }

   return chainData;
   }

UDATA
TR_J9SharedCache::rememberDebugCounterName(const char *name)
   {
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

   UDATA offset = data ? (UDATA)offsetInSharedCacheFromPointer((void *)data) : (UDATA)-1;

   //printf("\nrememberDebugCounterName: Tried to store %s (%p), data=%p, offset=%p\n", name, name, data, offset);

   return offset;
   }

const char *
TR_J9SharedCache::getDebugCounterName(UDATA offset)
   {
   const char *name = (offset != (UDATA)-1) ? (const char *)pointerFromOffsetInSharedCache((void *)offset) : NULL;

   //printf("\ngetDebugCounterName: Tried to find %p, name=%s (%p)\n", offset, (name ? name : ""), name);

   return name;
   }

uint32_t
TR_J9SharedCache::numInterfacesImplemented(J9Class *clazz)
   {
   uint32_t count=0;
   J9ITable *element = (J9ITable *)clazz->iTable;
   while (element != NULL)
      {
      count++;
      element = element->next;
      }
   return count;
   }

bool
TR_J9SharedCache::writeClassToChain(J9ROMClass *romClass, UDATA * & chainPtr)
   {
   void *classOffsetInCache;
   if (!isPointerInSharedCache(romClass, classOffsetInCache))
      {
      LOG(9, { log("\t\tromclass %p not in shared cache, writeClassToChain returning false\n", romClass); });
      return false;
      }

   J9UTF8 * className = J9ROMCLASS_CLASSNAME(romClass);
   LOG(9, {log("\t\tChain %p storing romclass %p (%.*s) offset %d\n", chainPtr, romClass, J9UTF8_LENGTH(className), J9UTF8_DATA(className), classOffsetInCache); });
   *chainPtr++ = (UDATA) classOffsetInCache;
   return true;
   }

bool
TR_J9SharedCache::writeClassesToChain(J9Class **superclasses, int32_t numSuperclasses, UDATA * & chainPtr)
   {
   LOG(9, { log("\t\twriteClassesToChain:\n"); });

   for (int32_t index=0; index < numSuperclasses;index++)
      {
      J9ROMClass *romClass = superclasses[index]->romClass;
      if (! writeClassToChain(romClass, chainPtr))
         return false;
      }
   return true;
   }

bool
TR_J9SharedCache::writeInterfacesToChain(J9Class *clazz, UDATA * & chainPtr)
   {
   LOG(9, { log("\t\twriteInterfacesToChain:\n"); });

   J9ITable *element = (J9ITable *)clazz->iTable;
   while (element != NULL)
      {
      J9ROMClass *romClass = element->interfaceClass->romClass;
      if (!writeClassToChain(romClass, chainPtr))
         return false;

      element = element->next;
      }

   return true;
   }

bool
TR_J9SharedCache::fillInClassChain(J9Class *clazz, UDATA *chainData, uint32_t chainLength,
                                   uint32_t numSuperclasses, uint32_t numInterfaces)
   {
   LOG(9, { log("\t\tChain %p store chainLength %d\n", chainData, chainLength); });

   UDATA *chainPtr = chainData;
   *chainPtr++ = chainLength;
   writeClassToChain(clazz->romClass, chainPtr);
   if (! writeClassesToChain(clazz->superclasses, numSuperclasses, chainPtr))
      {
      return false;
      }

   if (! writeInterfacesToChain(clazz, chainPtr))
      {
      return false;
      }

   LOG(9, {log("\t\tfillInClassChain returning true\n"); });
   return chainData;
   }


bool
TR_J9SharedCache::romclassMatchesCachedVersion(J9ROMClass *romClass, UDATA * & chainPtr, UDATA *chainEnd)
   {
   J9UTF8 * className = J9ROMCLASS_CLASSNAME(romClass);
   LOG(9, { log("\t\tExamining romclass %p (%.*s) offset %d, comparing to %d\n", romClass, J9UTF8_LENGTH(className), J9UTF8_DATA(className), (UDATA)romClass - _cacheStartAddress, *chainPtr); });
   if (chainPtr > chainEnd || ((UDATA)romClass - _cacheStartAddress) != *chainPtr++)
      return false;
   return true;
   }

UDATA *
TR_J9SharedCache::findChainForClass(J9Class *clazz, const char *key, uint32_t keyLength)
   {
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
   return (UDATA *) dataDescriptor.address;
   }

bool
TR_J9SharedCache::classMatchesCachedVersion(J9Class *clazz, UDATA *chainData)
   {
   J9UTF8 * className = J9ROMCLASS_CLASSNAME(clazz->romClass);
   LOG(5, { log("classMatchesCachedVersion class %p %.*s\n", clazz, J9UTF8_LENGTH(className), J9UTF8_DATA(className)); });

   void *classOffsetInCache;
   if (! isPointerInSharedCache(clazz->romClass, classOffsetInCache))
      {
      LOG(5, { log("\tclass not in shared cache, returning false\n"); });
      return false;
      }

   if (chainData == NULL)
      {
      char key[17]; // longest possible key length is way less than 16 digits
      uint32_t keyLength;
      createClassKey((UDATA)classOffsetInCache, key, keyLength);
      LOG(9, { log("\tno chain specific, so looking up for key %.*s\n", keyLength, key); });
      chainData = findChainForClass(clazz, key, keyLength);
      if (chainData == NULL)
         {
         LOG(5, { log("\tno stored chain, returning false\n"); });
         return false;
         }
      }

   UDATA *chainPtr = chainData;
   UDATA chainLength = *chainPtr++;
   UDATA *chainEnd = (UDATA *) (((U_8*)chainData) + chainLength);
   LOG(9, { log("\tfound chain: %p with length %d\n", chainData, chainLength); });

   if (! romclassMatchesCachedVersion(clazz->romClass, chainPtr, chainEnd))
      {
         LOG(5, { log("\tClass did not match, returning false\n"); });
         return false;
      }

   int32_t numSuperclasses = J9CLASS_DEPTH(clazz);
   J9Class **superclasses = clazz->superclasses;
   for (int32_t index=0; index < numSuperclasses;index++)
      {
      if (! romclassMatchesCachedVersion(superclasses[index]->romClass, chainPtr, chainEnd))
         {
         LOG(5, { log("\tClass in hierarchy did not match, returning false\n"); });
         return false;
         }
      }

   J9ITable *interfaceElement = (J9ITable *) clazz->iTable;
   while (interfaceElement)
      {
      if (! romclassMatchesCachedVersion(interfaceElement->interfaceClass->romClass, chainPtr, chainEnd))
         {
         LOG(5, { log("\tInterface class did not match, returning false\n"); });
         return false;
         }
      interfaceElement = interfaceElement->next;
      }

   if (chainPtr != chainEnd)
      {
      LOG(5, { log("\tfinished classes and interfaces, but not at chain end, returning false\n"); });
      return false;
      }

   LOG(5, { log("\tMatch!  return true\n"); });
   return true;
   }

TR_OpaqueClassBlock *
TR_J9SharedCache::lookupClassFromChainAndLoader(uintptrj_t *chainData, void *classLoader)
   {
   UDATA *ptrToRomClassOffset = chainData+1;
   J9ROMClass *romClass = (J9ROMClass *)(_cacheStartAddress + *ptrToRomClassOffset);
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

uintptrj_t
TR_J9SharedCache::getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCache(TR_OpaqueClassBlock *clazz)
   {
   void *loaderForClazz = _fe->getClassLoader(clazz);
   void *classChainIdentifyingLoaderForClazz = persistentClassLoaderTable()->lookupClassChainAssociatedWithClassLoader(loaderForClazz);
   uintptrj_t classChainOffsetInSharedCache = (uintptrj_t) offsetInSharedCacheFromPointer(classChainIdentifyingLoaderForClazz);
   return classChainOffsetInSharedCache;
   }
