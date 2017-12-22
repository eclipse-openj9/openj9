/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef CLASSLOADERTABLE_INCL
#define CLASSLOADERTABLE_INCL

#include "compile/Compilation.hpp"
#include "env/SharedCache.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/Array.hpp"
#include "runtime/RuntimeAssumptions.hpp"

class TR_Debug;
class TR_DebugExt;

#define   CLASSLOADERTABLE_SIZE   2053

struct TR_ClassLoaderInfo;

class TR_PersistentClassLoaderTable
   {
   public:

   TR_ALLOC(TR_Memory::PersistentCHTable)
   TR_PersistentClassLoaderTable(TR_PersistentMemory *);

   void associateClassLoaderWithClass(void *classLoaderPointer, TR_OpaqueClassBlock *clazz);
   void *lookupClassLoaderAssociatedWithClassChain(void *classChainPointer);
   void *lookupClassChainAssociatedWithClassLoader(void *classLoaderPointer);
   void removeClassLoader(void *classLoaderPointer);

   void setSharedCache(TR_SharedCache *sharedCache) { _sharedCache = sharedCache; }

   private:

   friend class TR_Debug;
   friend class TR_DebugExt;

   TR_PersistentMemory *trPersistentMemory() { return _trPersistentMemory; }


   int32_t hashLoader(void *classLoaderPointer);
   int32_t hashClassChain(void *classChain);

   TR_ClassLoaderInfo * _loaderTable[CLASSLOADERTABLE_SIZE];
   TR_ClassLoaderInfo * _classChainTable[CLASSLOADERTABLE_SIZE];
   TR_PersistentMemory *_trPersistentMemory;

   TR_SharedCache *sharedCache() { return _sharedCache; }
   TR_SharedCache *_sharedCache;

   static int32_t _numClassLoaders;
   };

#endif
