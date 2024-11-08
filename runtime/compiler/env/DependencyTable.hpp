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

#ifndef DEPENDENCYTABLE_INCL
#define DEPENDENCYTABLE_INCL

#include "env/J9PersistentInfo.hpp"
#include "env/PersistentCollections.hpp"
#include "env/VMJ9.h"

class TR_J9SharedCache;

#if defined(PERSISTENT_COLLECTIONS_UNSUPPORTED)

class TR_AOTDependencyTable
   {
public:
   void classLoadEvent(TR_OpaqueClassBlock *ramClass, bool isClassLoad, bool isClassInitialization) {}
   void invalidateUnloadedClass(TR_OpaqueClassBlock *ramClass) {}
   void invalidateRedefinedClass(TR_PersistentCHTable *table, TR_J9VMBase *fej9, TR_OpaqueClassBlock *oldClass, TR_OpaqueClassBlock *freshClass) {}
   TR_OpaqueClassBlock *findClassCandidate(uintptr_t offset) { return NULL; }
   };

#else

struct OffsetEntry
   {
   PersistentUnorderedSet<J9Class *> _loadedClasses;
   };

/**
 * \brief Tracking AOT load dependencies
 *
 * The dependency table maintains a map from ROM class offsets to loaded
 * J9Classes with that offset and a valid stored class chain. (There is at most
 * one chain with a particular first ROM class offset, so any two such classes
 * must have identical chains). The class chains of these classes must have been
 * shared when they were loaded in order to be tracked.
 *
 * The dependency table starts active, if the option is enabled. The public
 * methods of this class responsible for updating the table will catch
 * exceptions thrown internally and deactivate the table in response.
 * (Only failures to allocate are possible).
 *
 * In future it will also track pending AOT loads so the counts of their
 * associated RAM methods can be reduced when all of their AOT load dependencies
 * have been satisfied.
 */
class TR_AOTDependencyTable
   {
public:
   TR_PERSISTENT_ALLOC(TR_Memory::PersistentCHTable)
   TR_AOTDependencyTable(TR_J9SharedCache *sharedCache);
   // Update the table in response to a class load or initialization event. The
   // isClassInitialization parameter is currently unused.
   void classLoadEvent(TR_OpaqueClassBlock *ramClass, bool isClassLoad, bool isClassInitialization);

   // Invalidate an unloaded class
   void invalidateUnloadedClass(TR_OpaqueClassBlock *ramClass);

   // Invalidate a redefined class
   void invalidateRedefinedClass(TR_PersistentCHTable *table, TR_J9VMBase *fej9, TR_OpaqueClassBlock *oldClass, TR_OpaqueClassBlock *freshClass);

   // Given a class chain offset in the local SCC, return an initialized class
   // with a valid class chain starting with that offset.
   TR_OpaqueClassBlock *findCandidateFromChainOffset(TR::Compilation *comp, uintptr_t chainOffset);

private:
   bool isActive() const { return _isActive; }
   void setInactive() { _isActive = false; }
   // Deallocate the internal structures of the table and mark the table as
   // inactive. Must be called with the _tableMonitor in hand.
   void deactivateTable();

   void classLoadEventAtOffset(J9Class *ramClass, uintptr_t offset, bool isClassLoad, bool isClassInitialization);
   // Invalidate a class with a particular ROM class offset. Returns false if
   // the class wasn't tracked.
   bool invalidateClassAtOffset(J9Class *ramClass, uintptr_t romClassOffset);
   void recheckSubclass(J9Class *ramClass, uintptr_t offset, bool shouldRevalidate);
   OffsetEntry *getOffsetEntry(uintptr_t offset, bool create);

   // Initially true, and set to false if there is a failure to allocate.
   bool _isActive;

   TR_J9SharedCache *_sharedCache;
   TR::Monitor *const _tableMonitor;

   // A map from ROM class offsets to offset entries. The _loadedClasses of
   // those entries will contain classes with a valid class chain in the SCC
   // corresponding to that offset. Having the key be the ROM class offset
   // avoids the need to recreate the class chain to consult this map; this
   // works because there can be at most one class chain in the SCC whose entry
   // is a particular ROM class offset.
   PersistentUnorderedMap<uintptr_t, OffsetEntry> _offsetMap;
   };

#endif /* defined(PERSISTENT_COLLECTIONS_UNSUPPORTED) */
#endif
