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

#ifndef SHARED_CACHE_HPP
#define SHARED_CACHE_HPP

#include "j9.h"
#include "env/jittypes.h"

class TR_PersistentClassLoaderTable;
class TR_ResolvedMethod;
namespace TR { class ResolvedMethodSymbol; }

class TR_SharedCache
   {
public:
   virtual void persistIprofileInfo(TR::ResolvedMethodSymbol *, TR::Compilation *) { return; }
   virtual void persistIprofileInfo(TR::ResolvedMethodSymbol *, TR_ResolvedMethod*, TR::Compilation *) { return; }

   virtual bool isMostlyFull() { return false; }

   virtual bool canRememberClass(TR_OpaqueClassBlock *clazz) { return 0; }
   virtual uintptr_t *rememberClass(TR_OpaqueClassBlock *clazz) { return 0; }
   virtual bool classMatchesCachedVersion(TR_OpaqueClassBlock *clazz, uintptr_t *chainData=NULL) { return false; }

   virtual void *pointerFromOffsetInSharedCache(uintptr_t offset) { return NULL; }
   virtual uintptr_t offsetInSharedCacheFromPointer(void *ptr) { return 0; }
   virtual bool isPointerInSharedCache(void *ptr, uintptr_t *cacheOffset = NULL) { return false; }
   virtual bool isOffsetInSharedCache(uintptr_t offset, void *ptr = NULL) { return false; }

   virtual J9ROMClass *romClassFromOffsetInSharedCache(uintptr_t offset) { return NULL; }
   virtual uintptr_t offsetInSharedCacheFromROMClass(J9ROMClass *romClass) { return 0; }
   virtual bool isROMClassInSharedCache(J9ROMClass *romClass, uintptr_t *cacheOffset = NULL) { return false; }
   virtual bool isROMClassOffsetInSharedCache(uintptr_t offset, J9ROMClass **romClassPtr = NULL) { return false; }

   virtual J9ROMMethod *romMethodFromOffsetInSharedCache(uintptr_t offset) { return NULL; }
   virtual uintptr_t offsetInSharedCacheFromROMMethod(J9ROMMethod *romMethod) { return 0; }
   virtual bool isROMMethodInSharedCache(J9ROMMethod *romMethod, uintptr_t *cacheOffset = NULL) { return false; }
   virtual bool isROMMethodOffsetInSharedCache(uintptr_t offset, J9ROMMethod **romMethodPtr = NULL) { return false; }

   virtual void *ptrToROMClassesSectionFromOffsetInSharedCache(uintptr_t offset) { return NULL; }
   virtual uintptr_t offsetInSharedCacheFromPtrToROMClassesSection(void *ptr) { return 0; }
   virtual bool isPtrToROMClassesSectionInSharedCache(void *ptr, uintptr_t *cacheOffset = NULL) { return false; }
   virtual bool isOffsetOfPtrToROMClassesSectionInSharedCache(uintptr_t offset, void **ptr = NULL) { return false; }

   virtual TR_OpaqueClassBlock *lookupClassFromChainAndLoader(uintptr_t *cinaData, void *loader) { return NULL; }

   virtual uintptr_t getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCache(TR_OpaqueClassBlock *clazz) { return 0; }

   void setPersistentClassLoaderTable(TR_PersistentClassLoaderTable *table) { _persistentClassLoaderTable = table; }

   TR_PersistentClassLoaderTable *persistentClassLoaderTable() { return _persistentClassLoaderTable; }

private:

   TR_PersistentClassLoaderTable *_persistentClassLoaderTable;

   };

#endif
