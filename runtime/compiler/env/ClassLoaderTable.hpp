/*******************************************************************************
 * Copyright (c) 2000, 2021 IBM Corp. and others
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

#include "env/SharedCache.hpp"
#include "env/TRMemory.hpp"


#define CLASSLOADERTABLE_SIZE 2053

struct TR_ClassLoaderInfo;

class TR_PersistentClassLoaderTable
   {
public:

   TR_PERSISTENT_ALLOC(TR_Memory::PersistentCHTable)
   TR_PersistentClassLoaderTable(TR_PersistentMemory *persistentMemory);

   void associateClassLoaderWithClass(void *loader, TR_OpaqueClassBlock *clazz);
   void *lookupClassChainAssociatedWithClassLoader(void *loader) const;
   void *lookupClassLoaderAssociatedWithClassChain(void *chain) const;
   void removeClassLoader(void *loader);

   void setSharedCache(TR_SharedCache *sharedCache) { _sharedCache = sharedCache; }

private:

   friend class TR_Debug;
   friend class TR_DebugExt;

   TR_PersistentMemory *const _persistentMemory;
   TR_SharedCache *_sharedCache;

   TR_ClassLoaderInfo *_loaderTable[CLASSLOADERTABLE_SIZE];
   TR_ClassLoaderInfo *_chainTable[CLASSLOADERTABLE_SIZE];
   };

#endif
