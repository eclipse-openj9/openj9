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

#ifndef SHARED_CACHE_HPP
#define SHARED_CACHE_HPP

//#define HINTS_IN_SHAREDCACHE_OBJECT

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
   virtual uintptrj_t *rememberClass(TR_OpaqueClassBlock *clazz) { return 0; }
   virtual bool classMatchesCachedVersion(TR_OpaqueClassBlock *clazz, uintptrj_t *chainData=NULL) { return false; }

   virtual void *pointerFromOffsetInSharedCache(void *offset) { return (void *) NULL; }
   virtual void *offsetInSharedCacheFromPointer(void *ptr) { return (void *) NULL; }

   virtual bool isPointerInSharedCache(void *ptr, void * & cacheOffset) { return false; }

   virtual TR_OpaqueClassBlock *lookupClassFromChainAndLoader(uintptrj_t *cinaData, void *loader) { return NULL; }

   virtual uintptrj_t getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCache(TR_OpaqueClassBlock *clazz) { return 0; }

   void setPersistentClassLoaderTable(TR_PersistentClassLoaderTable *table) { _persistentClassLoaderTable = table; }

   TR_PersistentClassLoaderTable *persistentClassLoaderTable() { return _persistentClassLoaderTable; }

private:

   TR_PersistentClassLoaderTable *_persistentClassLoaderTable;

   };

#endif
