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
namespace JITaaS { class J9ServerStream; }

class TR_J9SharedCache : public TR_SharedCache
   {
public:
   TR_ALLOC(TR_Memory::SharedCache)

   TR_J9SharedCache(TR_J9VMBase *fe);

   virtual bool isHint(TR_ResolvedMethod *, TR_SharedCacheHint, uint16_t *dataField = NULL);
   virtual bool isHint(J9Method *, TR_SharedCacheHint, uint16_t *dataField = NULL);
   // virtual bool isHint(J9ROMMethod *, TR_SharedCacheHint, uint16_t *dataField = NULL);
   virtual uint16_t getAllEnabledHints(J9Method *method);
   // virtual uint16_t getAllEnabledHints(J9ROMMethod *method);
   virtual void addHint(J9Method *, TR_SharedCacheHint);
   // virtual void addHint(J9ROMMethod *, TR_SharedCacheHint);
   virtual void addHint(TR_ResolvedMethod *, TR_SharedCacheHint);
   virtual bool isMostlyFull();

   virtual void *pointerFromOffsetInSharedCache(void *offset);
   virtual void *offsetInSharedCacheFromPointer(void *ptr);

   virtual void persistIprofileInfo(TR::ResolvedMethodSymbol *, TR::Compilation *comp);
   virtual void persistIprofileInfo(TR::ResolvedMethodSymbol *, TR_ResolvedMethod*, TR::Compilation *comp);

   virtual bool canRememberClass(TR_OpaqueClassBlock *classPtr)
      {
      return (rememberClass((J9Class *) classPtr, false) != NULL);
      }

   virtual uintptrj_t *rememberClass(TR_OpaqueClassBlock *classPtr)
      {
      return (uintptrj_t *) rememberClass((J9Class *) classPtr, true);
      }

   virtual UDATA *rememberClass(J9Class *clazz, bool create=true);

   virtual UDATA rememberDebugCounterName(const char *name);
   virtual const char *getDebugCounterName(UDATA offset);

   virtual bool classMatchesCachedVersion(J9Class *clazz, UDATA *chainData=NULL);
   virtual bool classMatchesCachedVersion(TR_OpaqueClassBlock *classPtr, UDATA *chainData=NULL)
      {
      return classMatchesCachedVersion((J9Class *) classPtr, chainData);
      }

   virtual TR_OpaqueClassBlock *lookupClassFromChainAndLoader(uintptrj_t *chainData, void *classLoader);

   virtual bool isPointerInSharedCache(void *ptr, void * & cacheOffset);

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
   
   virtual UDATA getCacheStartAddress() { return _cacheStartAddress; }

private:
   J9JITConfig *jitConfig() { return _jitConfig; }
   J9JavaVM *javaVM() { return _javaVM; }
   TR_J9VMBase *fe() { return _fe; }
   J9SharedClassConfig *sharedCacheConfig() { return _sharedCacheConfig; }

   TR_AOTStats *aotStats() { return _aotStats; }

   void log(char *format, ...);

   uint32_t getHint(J9VMThread * vmThread, J9Method *method);
   uint32_t getHint(J9VMThread * vmThread, J9ROMMethod *method);

   void convertUnsignedOffsetToASCII(UDATA offset, char *myBuffer);
   void createClassKey(UDATA classOffsetInCache, char *key, uint32_t & keyLength);

   uint32_t numInterfacesImplemented(J9Class *clazz);

   bool writeClassToChain(J9ROMClass *romClass, UDATA * & chainPtr);
   bool writeClassesToChain(J9Class *clazz, int32_t numSuperclasses, UDATA * & chainPtr);
   bool writeInterfacesToChain(J9Class *clazz, UDATA * & chainPtr);
   bool fillInClassChain(J9Class *clazz, UDATA *chainData, uint32_t chainLength,
                         uint32_t numSuperclasses, uint32_t numInterfaces);

   bool romclassMatchesCachedVersion(J9ROMClass *romClass, UDATA * & chainPtr, UDATA *chainEnd);
   UDATA *findChainForClass(J9Class *clazz, const char *key, uint32_t keyLength);

   uint16_t _initialHintSCount;
   uint16_t _hintsEnabledMask;

   J9JavaVM *_javaVM;
   J9JITConfig *_jitConfig;
   TR_J9VMBase *_fe;
   TR::CompilationInfo *_compInfo;

   TR_AOTStats *_aotStats;
   J9SharedClassConfig *_sharedCacheConfig;
   UDATA _cacheStartAddress;
   UDATA _cacheSizeInBytes;
   UDATA _numDigitsForCacheOffsets;

   uint32_t _logLevel;
   bool _verboseHints;
   
   static TR_J9SharedCacheDisabledReason _sharedCacheState;
   static TR_YesNoMaybe                  _sharedCacheDisabledBecauseFull;
   static UDATA                          _storeSharedDataFailedLength;
   };

class TR_J9JITaaSServerSharedCache : public TR_J9SharedCache
   {
public:
   TR_ALLOC(TR_Memory::SharedCache)

   TR_J9JITaaSServerSharedCache(TR_J9VMBase *fe);

   virtual bool isHint(TR_ResolvedMethod *, TR_SharedCacheHint, uint16_t *dataField = NULL) override { TR_ASSERT(false, "called"); return false;}
   virtual bool isHint(J9Method *, TR_SharedCacheHint, uint16_t *dataField = NULL) override { TR_ASSERT(false, "called"); return false;}
   // virtual bool isHint(J9ROMMethod *, TR_SharedCacheHint, uint16_t *dataField = NULL) override { TR_ASSERT(false, "called"); return false;}
   virtual uint16_t getAllEnabledHints(J9Method *method) override { TR_ASSERT(false, "called"); return 0;}
   // virtual uint16_t getAllEnabledHints(J9ROMMethod *method) override { TR_ASSERT(false, "called"); return 0;}
   virtual void addHint(J9Method *, TR_SharedCacheHint) override;
   // virtual void addHint(J9ROMMethod *, TR_SharedCacheHint) override { TR_ASSERT(false, "called"); }
   virtual bool isMostlyFull() override { TR_ASSERT(false, "called"); return false;}

   virtual void *pointerFromOffsetInSharedCache(void *offset) override;
   virtual void *offsetInSharedCacheFromPointer(void *ptr) override;

   virtual UDATA *rememberClass(J9Class *clazz, bool create=true) override;

   virtual UDATA rememberDebugCounterName(const char *name) override { TR_ASSERT(false, "called"); return NULL;}
   virtual const char *getDebugCounterName(UDATA offset) override { TR_ASSERT(false, "called"); return NULL;}

   virtual bool classMatchesCachedVersion(J9Class *clazz, UDATA *chainData=NULL) override { TR_ASSERT(false, "called"); return false;}

   virtual TR_OpaqueClassBlock *lookupClassFromChainAndLoader(uintptrj_t *chainData, void *classLoader) override { TR_ASSERT(false, "called"); return NULL;}

   virtual bool isPointerInSharedCache(void *ptr, void * & cacheOffset) override { TR_ASSERT(false, "called"); return false;}

   
   static void setSharedCacheDisabledReason(TR_J9SharedCacheDisabledReason state) { TR_ASSERT(false, "called"); }
   static TR_J9SharedCacheDisabledReason getSharedCacheDisabledReason() { TR_ASSERT(false, "called"); return TR_J9SharedCache::TR_J9SharedCacheDisabledReason::UNINITIALIZED;}
   static TR_YesNoMaybe isSharedCacheDisabledBecauseFull(TR::CompilationInfo *compInfo) { TR_ASSERT(false, "called"); return TR_no;}
   static void setStoreSharedDataFailedLength(UDATA length) { TR_ASSERT(false, "called"); }
   
   virtual UDATA getCacheStartAddress() override;

   virtual uintptrj_t getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCache(TR_OpaqueClassBlock *clazz) override;

   void setStream(JITaaS::J9ServerStream *stream) { _stream = stream; }

private:
   JITaaS::J9ServerStream *_stream;
   };

#endif
