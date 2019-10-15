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

#ifndef J9SHARED_CACHE_HPP
#define J9SHARED_CACHE_HPP

#include "compiler/env/SharedCache.hpp"

#include <stdint.h>
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "runtime/J9Runtime.hpp"

class TR_J9VMBase;
class TR_ResolvedMethod;
namespace TR { class CompilationInfo; }

/**
 * \brief An interface to the VM's shared class cache.
 *
 * This class provides an interface to the VM's shared class cache as represented by the the descriptors in J9SharedClassConfig::cacheDescriptorList.
 * The cache descriptor list is a circular linked list. It is doubly linked when J9VM_OPT_MULTI_LAYER_SHARED_CLASS_CACHE is defined
 * or singly linked otherwise.
 *
 * If J9VM_OPT_MULTI_LAYER_SHARED_CLASS_CACHE is not defined, the list consists of a single element who's next pointer refers back to itself.
 * Offsets into the shared cache represent the distance from the start of the cache. Converting between pointers and offsets can be done with
 * simple pointer arithmetic.
 *
 * If J9VM_OPT_MULTI_LAYER_SHARED_CLASS_CACHE is defined, the head of the list represents the top-most (and therefore writable)
 * layer of the shared class cache, the next element represents the (N-1)th layer, and so on. The element previous to the head represents the base layer.
 * The list of cache layers can be thought of as a single logical cache starting at layer 0 and ending at layer N. Offsets into the shared cache
 * represent the distance from the start of the cache. Converting between pointers and offsets requires traversing the list starting at the base layer.
 */
class TR_J9SharedCache : public TR_SharedCache
   {
public:
   TR_ALLOC(TR_Memory::SharedCache)

   TR_J9SharedCache(TR_J9VMBase *fe);

   bool isHint(TR_ResolvedMethod *, TR_SharedCacheHint, uint16_t *dataField = NULL);
   bool isHint(J9Method *, TR_SharedCacheHint, uint16_t *dataField = NULL);
   uint16_t getAllEnabledHints(J9Method *method);
   void addHint(J9Method *, TR_SharedCacheHint);
   void addHint(TR_ResolvedMethod *, TR_SharedCacheHint);
   bool isMostlyFull();

   /**
    * \brief Converts a shared cache offset into a pointer.
    *
    * \param[in] offset The offset to convert.
    * \return A pointer. Raises a fatal assertion before returning NULL if the offset is invalid.
    */
   virtual void *pointerFromOffsetInSharedCache(uintptr_t offset);

   /**
    * \brief Converts a pointer into the shared cache into an offset.
    *
    * \param[in] ptr The pointer to convert.
    * \return An offset. Raises a fatal assertion before returning 0 if the pointer is invalid.
    */
   virtual uintptr_t offsetInSharedCacheFromPointer(void *ptr);

   void persistIprofileInfo(TR::ResolvedMethodSymbol *, TR::Compilation *comp);
   void persistIprofileInfo(TR::ResolvedMethodSymbol *, TR_ResolvedMethod*, TR::Compilation *comp);

   bool canRememberClass(TR_OpaqueClassBlock *classPtr)
      {
      return (rememberClass((J9Class *) classPtr, false) != NULL);
      }

   uintptrj_t *rememberClass(TR_OpaqueClassBlock *classPtr)
      {
      return (uintptrj_t *) rememberClass((J9Class *) classPtr, true);
      }

   UDATA *rememberClass(J9Class *clazz, bool create=true);

   UDATA rememberDebugCounterName(const char *name);
   const char *getDebugCounterName(UDATA offset);

   bool classMatchesCachedVersion(J9Class *clazz, UDATA *chainData=NULL);
   bool classMatchesCachedVersion(TR_OpaqueClassBlock *classPtr, UDATA *chainData=NULL)
      {
      return classMatchesCachedVersion((J9Class *) classPtr, chainData);
      }

   TR_OpaqueClassBlock *lookupClassFromChainAndLoader(uintptrj_t *chainData, void *classLoader);

   /**
    * \brief Checks whether the specified pointer points into the shared cache.
    *
    * \param[in] ptr The pointer to check.
    * \param[out] cacheOffset If ptr points into the shared cache and this parameter is not NULL the result of converting ptr into an offset will be returned here. If ptr does not point into the shared cache this parameter is ignored.
    * \return True if the pointer points into the shared cache, false otherwise.
    */
   bool isPointerInSharedCache(void *ptr, uintptrj_t *cacheOffset = NULL);

   J9ROMClass *startingROMClassOfClassChain(UDATA *classChain);

   virtual uintptrj_t getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCache(TR_OpaqueClassBlock *clazz);

   enum TR_J9SharedCacheDisabledReason
      {
      UNINITIALIZED,
      NOT_DISABLED,
      AOT_DISABLED,
      AOT_HEADER_INVALID,
      AOT_HEADER_FAILED_TO_ALLOCATE,
      J9_SHARED_CACHE_FAILED_TO_ALLOCATE,
      SHARED_CACHE_STORE_ERROR,
      SHARED_CACHE_FULL,
      // The following are probably equivalent to SHARED_CACHE_FULL -
      // they could have failed because of no space but no error code is returned.
      SHARED_CACHE_CLASS_CHAIN_STORE_FAILED,
      AOT_HEADER_STORE_FAILED
      };

   static void setSharedCacheDisabledReason(TR_J9SharedCacheDisabledReason state) { _sharedCacheState = state; }
   static TR_J9SharedCacheDisabledReason getSharedCacheDisabledReason() { return _sharedCacheState; }
   static TR_YesNoMaybe isSharedCacheDisabledBecauseFull(TR::CompilationInfo *compInfo);
   static void setStoreSharedDataFailedLength(UDATA length) {_storeSharedDataFailedLength = length; }

private:
   J9JITConfig *jitConfig() { return _jitConfig; }
   J9JavaVM *javaVM() { return _javaVM; }
   TR_J9VMBase *fe() { return _fe; }
   J9SharedClassConfig *sharedCacheConfig() { return _sharedCacheConfig; }

   TR_AOTStats *aotStats() { return _aotStats; }

   void log(char *format, ...);

   uint32_t getHint(J9VMThread * vmThread, J9Method *method);

   void convertUnsignedOffsetToASCII(UDATA offset, char *myBuffer);
   void createClassKey(UDATA classOffsetInCache, char *key, uint32_t & keyLength);

   uint32_t numInterfacesImplemented(J9Class *clazz);

   bool writeClassToChain(J9ROMClass *romClass, UDATA * & chainPtr);
   bool writeClassesToChain(J9Class **superclasses, int32_t numSuperclasses, UDATA * & chainPtr);
   bool writeInterfacesToChain(J9Class *clazz, UDATA * & chainPtr);
   bool fillInClassChain(J9Class *clazz, UDATA *chainData, uint32_t chainLength,
                         uint32_t numSuperclasses, uint32_t numInterfaces);

   bool romclassMatchesCachedVersion(J9ROMClass *romClass, UDATA * & chainPtr, UDATA *chainEnd);
   UDATA *findChainForClass(J9Class *clazz, const char *key, uint32_t keyLength);

   static bool isPointerInCache(const J9SharedClassCacheDescriptor *cacheDesc, void *ptr);

   uint16_t _initialHintSCount;
   uint16_t _hintsEnabledMask;

   J9JavaVM *_javaVM;
   J9JITConfig *_jitConfig;
   TR_J9VMBase *_fe;
   TR::CompilationInfo *_compInfo;

   TR_AOTStats *_aotStats;
   J9SharedClassConfig *_sharedCacheConfig;
   UDATA _numDigitsForCacheOffsets;

   uint32_t _logLevel;
   bool _verboseHints;

   static TR_J9SharedCacheDisabledReason _sharedCacheState;
   static TR_YesNoMaybe                  _sharedCacheDisabledBecauseFull;
   static UDATA                          _storeSharedDataFailedLength;
   };

#endif
