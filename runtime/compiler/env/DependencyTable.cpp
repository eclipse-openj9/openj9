/*******************************************************************************
 * Copyright IBM Corp. and others 2024
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


#include "control/CompilationThread.hpp"
#include "env/DependencyTable.hpp"
#include "env/J9SharedCache.hpp"
#include "env/PersistentCHTable.hpp"

#if !defined(PERSISTENT_COLLECTIONS_UNSUPPORTED)

TR_AOTDependencyTable::TR_AOTDependencyTable(TR_J9SharedCache *sharedCache) :
   _isActive(true),
   _sharedCache(sharedCache),
   _tableMonitor(TR::Monitor::create("JIT-AOTDependencyTableMonitor")),
   _offsetMap(decltype(_offsetMap)::allocator_type(TR::Compiler->persistentAllocator()))
   { }

void
TR_AOTDependencyTable::classLoadEvent(TR_OpaqueClassBlock *clazz, bool isClassLoad, bool isClassInitialization)
   {
   auto ramClass = (J9Class *)clazz;

   uintptr_t classOffset = TR_SharedCache::INVALID_ROM_CLASS_OFFSET;
   if (!_sharedCache->isClassInSharedCache(clazz, &classOffset))
      return;

   // We only need to check if clazz matches its cached version on load; on
   // initialization, it will be in the _offsetMap if it did match.
   if (isClassLoad && !_sharedCache->classMatchesCachedVersion(ramClass, NULL))
      return;

   OMR::CriticalSection cs(_tableMonitor);
   if (!isActive())
      return;

   try
      {
      classLoadEventAtOffset(ramClass, classOffset, isClassLoad, isClassInitialization);
      }
   catch (std::exception&)
      {
      deactivateTable();
      }
   }

void
TR_AOTDependencyTable::classLoadEventAtOffset(J9Class *ramClass, uintptr_t offset, bool isClassLoad, bool isClassInitialization)
   {
   auto entry = getOffsetEntry(offset, isClassLoad);
   TR_ASSERT(entry || !isClassLoad, "Class %p offset %lu initialized without loading");

   if (isClassLoad)
      entry->_loadedClasses.insert(ramClass);
   }

OffsetEntry *
TR_AOTDependencyTable::getOffsetEntry(uintptr_t offset, bool create)
   {
   auto it = _offsetMap.find(offset);
   if (it != _offsetMap.end())
      return &it->second;

   if (create)
      {
      PersistentUnorderedSet<J9Class *> loadedClasses(PersistentUnorderedSet<J9Class *>::allocator_type(TR::Compiler->persistentAllocator()));
      return &(*_offsetMap.insert(it, {offset, {loadedClasses}})).second;
      }

   return NULL;
   }

void
TR_AOTDependencyTable::invalidateUnloadedClass(TR_OpaqueClassBlock *clazz)
   {
   uintptr_t classOffset = TR_SharedCache::INVALID_ROM_CLASS_OFFSET;
   if (!_sharedCache->isClassInSharedCache(clazz, &classOffset))
      return;


   OMR::CriticalSection cs(_tableMonitor);
   if (!isActive())
      return;

   invalidateClassAtOffset((J9Class *)clazz, classOffset);
   }

bool
TR_AOTDependencyTable::invalidateClassAtOffset(J9Class *ramClass, uintptr_t romClassOffset)
   {
   auto entry = getOffsetEntry(romClassOffset, false);
   if (entry)
      {
      entry->_loadedClasses.erase(ramClass);
      if (entry->_loadedClasses.empty())
         _offsetMap.erase(romClassOffset);
      return true;
      }
   return false;
   }

// If an entry exists for a class, remove it. Otherwise, if we should
// revalidate, add an entry if the class has a valid chain.
void
TR_AOTDependencyTable::recheckSubclass(J9Class *ramClass, uintptr_t offset, bool shouldRevalidate)
   {
   if (invalidateClassAtOffset(ramClass, offset))
      return;

   if (shouldRevalidate && _sharedCache->classMatchesCachedVersion(ramClass, NULL))
      {
      bool initialized = J9ClassInitSucceeded == ramClass->initializeStatus;
      classLoadEventAtOffset(ramClass, offset, true, initialized);
      }
   }

// In a class redefinition event, an old class is replaced by a fresh class. If
// the ROM class offset changed as a result, it and all its subclasses that
// formerly had valid chains will now be guaranteed not to match, so the entries
// for these must be removed. If the new offset is valid, any class that didn't
// have an entry should be rechecked.
void
TR_AOTDependencyTable::invalidateRedefinedClass(TR_PersistentCHTable *table, TR_J9VMBase *fej9, TR_OpaqueClassBlock *oldClass, TR_OpaqueClassBlock *freshClass)
   {
   uintptr_t freshClassOffset = TR_SharedCache::INVALID_ROM_CLASS_OFFSET;
   uintptr_t oldClassOffset = TR_SharedCache::INVALID_ROM_CLASS_OFFSET;
   if (!_sharedCache->isClassInSharedCache(freshClass, &freshClassOffset) && !_sharedCache->isClassInSharedCache(oldClass, &oldClassOffset))
      return;

   if (oldClassOffset == freshClassOffset)
      {
      OMR::CriticalSection cs(_tableMonitor);
      if (!isActive())
         return;

      try
         {
         // If the offset is unchanged and the old class was tracked, the new
         // class will have a valid chain as well, so we only need to swap the
         // old and fresh class pointers.
         if (invalidateClassAtOffset((J9Class *)oldClass, oldClassOffset))
            {
            auto freshRamClass = (J9Class *)freshClass;
            bool initialized = J9ClassInitSucceeded == freshRamClass->initializeStatus;
            classLoadEventAtOffset(freshRamClass, freshClassOffset, true, initialized);
            }
         }
      catch (std::exception&)
         {
         deactivateTable();
         }

      return;
      }

   bool revalidateUntrackedClasses = freshClassOffset != TR_SharedCache::INVALID_ROM_CLASS_OFFSET;

   TR_PersistentClassInfo *classInfo = table->findClassInfo(oldClass);
   TR_PersistentCHTable::ClassList classList(TR::Compiler->persistentAllocator());

   table->collectAllSubClasses(classInfo, classList, fej9);
   classList.push_front(classInfo);

   OMR::CriticalSection cs(_tableMonitor);
   if (!isActive())
      return;

   try
      {
      for (auto iter = classList.begin(); iter != classList.end(); iter++)
         {
         auto clazz = (J9Class *)(*iter)->getClassId();
         uintptr_t offset = TR_SharedCache::INVALID_ROM_CLASS_OFFSET;
         if (!_sharedCache->isClassInSharedCache(clazz, &offset))
            continue;
         recheckSubclass(clazz, offset, revalidateUntrackedClasses);
         }
      }
   catch (std::exception&)
      {
      deactivateTable();
      }
   }

TR_OpaqueClassBlock *
TR_AOTDependencyTable::findClassCandidate(uintptr_t romClassOffset)
   {
   OMR::CriticalSection cs(_tableMonitor);

   if (!isActive())
      return NULL;

   auto it = _offsetMap.find(romClassOffset);
   if (it == _offsetMap.end())
      return NULL;

   for (const auto& clazz: it->second._loadedClasses)
      {
      if (J9ClassInitSucceeded == clazz->initializeStatus)
         return (TR_OpaqueClassBlock *)clazz;
      }

   return NULL;
   }

void
TR_AOTDependencyTable::deactivateTable()
   {
   _offsetMap.clear();
   setInactive();
   }

#endif /* !defined(PERSISTENT_COLLECTIONS_UNSUPPORTED) */
