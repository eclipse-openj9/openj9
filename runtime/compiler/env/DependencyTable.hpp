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
   bool trackMethod(J9VMThread *vmThread, J9Method *method, J9ROMMethod *romMethod, bool &dependenciesSatisfied) { return false; }
   void methodIsBeingCompiled(J9Method *method) {}
   void classLoadEvent(TR_OpaqueClassBlock *ramClass, bool isClassLoad, bool isClassInitialization) {}
   void invalidateUnloadedClass(TR_OpaqueClassBlock *ramClass) {}
   void invalidateRedefinedClass(TR_PersistentCHTable *table, TR_J9VMBase *fej9, TR_OpaqueClassBlock *oldClass, TR_OpaqueClassBlock *freshClass) {}
   J9Class *findCandidateWithChainAndLoader(TR::Compilation *comp, uintptr_t classChainOffset, void *classLoaderChain) { return NULL; }
   J9Class *findCandidateWithChainAndLoader(TR::Compilation *comp, uintptr_t *classChain, void *classLoaderChain) { return NULL; }
   void methodWillBeCompiled(J9Method *method) {}
   void printStats() {}
   };

#else

struct MethodEntry
   {
   // The number of dependencies left to be satisfied
   uintptr_t _remainingDependencies;
   // The dependency chain stored in the local SCC (used to stop tracking a
   // method)
   const uintptr_t *_dependencyChain;
   };

typedef std::pair<J9Method *const, MethodEntry>* MethodEntryRef;

struct OffsetEntry
   {
   // Classes that have been loaded and have a particular valid class chain
   // offset
   PersistentUnorderedSet<J9Class *> _loadedClasses;
   // Methods waiting for a class with this offset to be loaded
   PersistentUnorderedSet<MethodEntryRef> _waitingLoadMethods;
   // Methods waiting for a class with this offset to be initialized
   PersistentUnorderedSet<MethodEntryRef> _waitingInitMethods;
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

   // If the given method has an AOT body with stored dependencies in the local
   // SCC, trackMethod() will determine how many are currently satisfied. If all
   // are, dependenciesSatisfied will be set to true. Otherwise, the method and
   // its dependencies will be tracked until all the dependencies are satisfied,
   // at which point the method's count will be reduced to zero.
   //
   // Returns true if the method had stored dependencies and either they were
   // all satisfied immediately, or the method was successfully tracked.
   bool trackMethod(J9VMThread *vmThread, J9Method *method, J9ROMMethod *romMethod, bool &dependenciesSatisfied);

   // Inform the dependency table that a method is being compiled, so it can
   // stop tracking the method. Will invalidate the MethodEntryRef pointer for
   // method, if it was being tracked.
   void methodWillBeCompiled(J9Method *method);

   // Update the table in response to a class load or initialization event.
   void classLoadEvent(TR_OpaqueClassBlock *ramClass, bool isClassLoad, bool isClassInitialization);

   // Invalidate an unloaded class. Will invalidate the MethodEntryRef for every
   // RAM method of ramClass.
   void invalidateUnloadedClass(TR_OpaqueClassBlock *ramClass);

   // Invalidate a redefined class. Will invalidate the MethodEntryRef for every
   // RAM method of ramClass.
   void invalidateRedefinedClass(TR_PersistentCHTable *table, TR_J9VMBase *fej9, TR_OpaqueClassBlock *oldClass, TR_OpaqueClassBlock *freshClass);

   // Given a class chain and class loader chain, return an initialized class
   // with a valid class chain starting with that offset and with a class loader
   // with that loader chain
   J9Class *findCandidateWithChainAndLoader(TR::Compilation *comp, uintptr_t classChainOffset, void *classLoaderChain);
   J9Class *findCandidateWithChainAndLoader(TR::Compilation *comp, uintptr_t *classChain, void *classLoaderChain);

   static uintptr_t decodeDependencyOffset(uintptr_t offset)
      {
      return offset | 1;
      }
   static uintptr_t decodeDependencyOffset(uintptr_t offset, bool &needsInitialization)
      {
      needsInitialization = (offset & 1) == 1;
      return decodeDependencyOffset(offset);
      }
   static uintptr_t encodeDependencyOffset(uintptr_t offset, bool needsInitialization)
      {
      return needsInitialization ? offset : (offset & ~1);
      }

   void printStats();
private:
   bool isActive() const { return _isActive; }
   void setInactive() { _isActive = false; }

   // Deallocate the internal structures of the table and mark the table as
   // inactive. Must be called with the _tableMonitor in hand.
   void deactivateTable();

   // Register a class load event for ramClass at offset. If any methods had
   // their dependencies satisfied by this event, they will be added to
   // methodsToQueue.
   void classLoadEventAtOffset(J9Class *ramClass, uintptr_t offset, bool isClassLoad, bool isClassInitialization);

   // Invalidate a class with a particular ROM class offset. Returns false if
   // the class wasn't tracked. If pendingMethodQueue is not NULL, we will also
   // remove all methods whose dependencies became unsatisfied from
   // pendingMethodQueue.
   bool invalidateClassAtOffset(J9Class *ramClass, uintptr_t romClassOffset);

   // Invalidate any tracked methods of ramClass. This will invalidate the
   // MethodEntryRef of those methods. When registering a class redefinition
   // event, this is best called before any revalidation occurs, so that stale
   // method entry pointers are not collected in _pendingLoads.
   void invalidateMethodsOfClass(J9Class *ramClass);

   void recheckSubclass(J9Class *ramClass, uintptr_t offset, bool shouldRevalidate);

   // Get the OffsetEntry corresponding to offset. If create is true this will
   // never return a NULL entry (but it may throw).
   OffsetEntry *getOffsetEntry(uintptr_t offset, bool create);

   J9Class *findCandidateForDependency(const PersistentUnorderedSet<J9Class *> &loadedClasses, bool needsInitialization);
   J9Class *findChainLoaderCandidate(TR::Compilation *comp, uintptr_t *classChain, void *classLoaderChain);

   // Stop tracking the given method. This will invalidate the MethodEntryRef
   // for the method.
   void stopTracking(MethodEntryRef entry, bool isEarlyStop);
   void stopTracking(J9Method *method, bool isEarlyStop);

   // Queue and clear the _pendingLoads, and remove those methods from tracking.
   // Must be called at the end of any dependency table operation that could
   // have led to a pending load being registered.
   void resolvePendingLoads();

   // Erase the given entry at offset if the entry is empty. This will
   // invalidate the pointer to OffsetEntry.
   void eraseOffsetEntryIfEmpty(const OffsetEntry &entry, uintptr_t offset);

   void registerSatisfaction(PersistentUnorderedSet<MethodEntryRef> waitingMethods);
   void registerDissatisfaction(PersistentUnorderedSet<MethodEntryRef> waitingMethods);

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

   // A map from methods to their tracking information in the dependency table
   PersistentUnorderedMap<J9Method *, MethodEntry> _methodMap;

   // Any pending method loads that were triggered by the current dependency
   // table transaction
   PersistentUnorderedSet<MethodEntryRef> _pendingLoads;
   };

#endif /* defined(PERSISTENT_COLLECTIONS_UNSUPPORTED) */
#endif
