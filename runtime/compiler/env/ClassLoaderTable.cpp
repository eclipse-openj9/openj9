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

#include "env/ClassLoaderTable.hpp"


enum TableKind { Loader, Chain };

// To make the two-way map between class loaders and class chains more efficient, this struct
// is linked into two linked lists - one for each hash table in TR_PersistentClassLoaderTable.
struct TR_ClassLoaderInfo
   {
   TR_PERSISTENT_ALLOC(TR_Memory::PersistentCHTable)

   TR_ClassLoaderInfo(void *loader, void *chain) :
      _loader(loader), _loaderTableNext(NULL),
      _chain(chain), _chainTableNext(NULL) { }

   template<TableKind T> TR_ClassLoaderInfo *&next();
   template<TableKind T> bool equals(const void *key) const;

   void *const _loader;
   TR_ClassLoaderInfo *_loaderTableNext;
   void *const _chain;
   TR_ClassLoaderInfo *_chainTableNext;
   };


template<TableKind T> static size_t hash(const void *key);

template<TableKind T> static TR_ClassLoaderInfo *
lookup(TR_ClassLoaderInfo *const *table, size_t &index, TR_ClassLoaderInfo *&prev, const void *key)
   {
   index = hash<T>(key) % CLASSLOADERTABLE_SIZE;
   TR_ClassLoaderInfo *info = table[index];
   prev = NULL;
   while (info && !info->equals<T>(key))
      {
      prev = info;
      info = info->next<T>();
      }
   return info;
   }

template<TableKind T> static void
insert(TR_ClassLoaderInfo *info, TR_ClassLoaderInfo **table, size_t index)
   {
   info->next<T>() = table[index];
   table[index] = info;
   }

template<TableKind T> static void
remove(TR_ClassLoaderInfo *info, TR_ClassLoaderInfo *prev, TR_ClassLoaderInfo **table, size_t index)
   {
   if (prev)
      prev->next<T>() = info->next<T>();
   else
      table[index] = info->next<T>();
   }


template<> TR_ClassLoaderInfo *&TR_ClassLoaderInfo::next<Loader>() { return _loaderTableNext; }
template<> bool TR_ClassLoaderInfo::equals<Loader>(const void *loader) const { return loader == _loader; }
// Remove trailing zero bits in aligned pointer for better hash distribution
template<> size_t hash<Loader>(const void *loader) { return (uintptr_t)loader >> 3; }

template<> TR_ClassLoaderInfo *&TR_ClassLoaderInfo::next<Chain>() { return _chainTableNext; }
template<> bool TR_ClassLoaderInfo::equals<Chain>(const void *chain) const { return chain == _chain; }
// Remove trailing zero bits in aligned pointer for better hash distribution
template<> size_t hash<Chain>(const void *chain) { return (uintptr_t)chain >> 3; }


TR_PersistentClassLoaderTable::TR_PersistentClassLoaderTable(TR_PersistentMemory *persistentMemory) :
   _persistentMemory(persistentMemory), _sharedCache(NULL)
   {
   memset(_loaderTable, 0, sizeof(_loaderTable));
   memset(_chainTable, 0, sizeof(_chainTable));
   }


void
TR_PersistentClassLoaderTable::associateClassLoaderWithClass(void *loader, TR_OpaqueClassBlock *clazz)
   {
   if (!_sharedCache)
      return;

   // Lookup by class loader and check if it already has an associated class
   size_t index;
   TR_ClassLoaderInfo *prev;
   TR_ClassLoaderInfo *info = lookup<Loader>(_loaderTable, index, prev, loader);
   if (info)
      return;

   void *chain = _sharedCache->rememberClass(clazz);
   if (!chain)
      return;
   TR_ASSERT(_sharedCache->isPointerInSharedCache(chain), "Class chain must be in SCC");

   info = new (_persistentMemory) TR_ClassLoaderInfo(loader, chain);
   if (!info)
      {
      // This is a bad situation because we can't associate the right class with this class loader.
      // Probably not critical if multiple class loaders aren't routinely loading the exact same class.
      //TODO: Prevent this class loader from associating with a different class
      return;
      }
   insert<Loader>(info, _loaderTable, index);

   // Lookup by class chain and check if was already associated with another class loader
   TR_ClassLoaderInfo *otherInfo = lookup<Chain>(_chainTable, index, prev, chain);
   if (otherInfo)
      {
      // There is more than one class loader with identical first loaded class.
      // Current heuristic doesn't work in this scenario, but it doesn't break
      // correctness, and in the worst case can only result in failed AOT loads.
      // We have added this loader to _classLoaderTable, which has a nice side
      // benefit that we won't keep trying to add it, so leave it there.
      return;
      }
   insert<Chain>(info, _chainTable, index);
   }


void *
TR_PersistentClassLoaderTable::lookupClassChainAssociatedWithClassLoader(void *loader) const
   {
   if (!_sharedCache)
      return NULL;

   size_t index;
   TR_ClassLoaderInfo *prev;
   TR_ClassLoaderInfo *info = lookup<Loader>(_loaderTable, index, prev, loader);
   return info ? info->_chain : NULL;
   }

void *
TR_PersistentClassLoaderTable::lookupClassLoaderAssociatedWithClassChain(void *chain) const
   {
   if (!_sharedCache)
      return NULL;

   size_t index;
   TR_ClassLoaderInfo *prev;
   TR_ClassLoaderInfo *info = lookup<Chain>(_chainTable, index, prev, chain);
   return info ? info->_loader : NULL;
   }


void
TR_PersistentClassLoaderTable::removeClassLoader(void *loader)
   {
   if (!_sharedCache)
      return;

   // Remove from the table indexed by class loader
   size_t index;
   TR_ClassLoaderInfo *prev;
   TR_ClassLoaderInfo *info = lookup<Loader>(_loaderTable, index, prev, loader);
   if (!info)
      return;
   remove<Loader>(info, prev, _loaderTable, index);

   // Remove from the table indexed by class chain
   TR_ClassLoaderInfo *otherInfo = lookup<Chain>(_chainTable, index, prev, info->_chain);
   if (otherInfo == info)// Otherwise the entry belongs to another class loader
      remove<Chain>(info, prev, _chainTable, index);

   _persistentMemory->freePersistentMemory(info);
   }
