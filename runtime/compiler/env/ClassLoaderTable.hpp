/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef CLASSLOADERTABLE_INCL
#define CLASSLOADERTABLE_INCL

#include "env/TRMemory.hpp"


#define CLASSLOADERTABLE_SIZE 2053

class TR_J9SharedCache;
struct TR_ClassLoaderInfo;

class TR_PersistentClassLoaderTable
   {
public:

   TR_PERSISTENT_ALLOC(TR_Memory::PersistentCHTable)
   TR_PersistentClassLoaderTable(TR_PersistentMemory *persistentMemory);

   void associateClassLoaderWithClass(J9VMThread *vmThread, void *loader, TR_OpaqueClassBlock *clazz);
   void *lookupClassChainAssociatedWithClassLoader(void *loader) const;
   void *lookupClassLoaderAssociatedWithClassChain(void *chain) const;
#if defined(J9VM_OPT_JITSERVER)
   // JIT client needs to associate each class loader with the name of its first loaded class
   // in order to support AOT method serialization (and caching at JITServer) and deserialization.
   std::pair<void *, void *>// loader, chain
   lookupClassLoaderAndChainAssociatedWithClassName(const uint8_t *data, size_t length) const;
   void *lookupClassLoaderAssociatedWithClassName(const uint8_t *data, size_t length) const;
   const J9UTF8 *lookupClassNameAssociatedWithClassLoader(void *loader) const;
#endif /* defined(J9VM_OPT_JITSERVER) */
   void removeClassLoader(J9VMThread *vmThread, void *loader);

   TR_J9SharedCache *getSharedCache() const { return _sharedCache; }
   void setSharedCache(TR_J9SharedCache *sharedCache) { _sharedCache = sharedCache; }

private:

   friend class TR_Debug;

   TR_PersistentMemory *const _persistentMemory;
   TR_J9SharedCache *_sharedCache;

   TR_ClassLoaderInfo *_loaderTable[CLASSLOADERTABLE_SIZE];
   TR_ClassLoaderInfo *_chainTable[CLASSLOADERTABLE_SIZE];
#if defined(J9VM_OPT_JITSERVER)
   TR_ClassLoaderInfo *_nameTable[CLASSLOADERTABLE_SIZE];
#endif /* defined(J9VM_OPT_JITSERVER) */
   };

#endif
