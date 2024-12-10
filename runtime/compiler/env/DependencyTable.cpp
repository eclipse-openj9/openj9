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

#include "control/CompilationRuntime.hpp"
#include "control/CompilationThread.hpp"
#include "env/ClassLoaderTable.hpp"
#include "env/DependencyTable.hpp"
#include "env/J9SharedCache.hpp"
#include "env/PersistentCHTable.hpp"

#if !defined(PERSISTENT_COLLECTIONS_UNSUPPORTED)

TR_AOTDependencyTable::TR_AOTDependencyTable(TR_J9SharedCache *sharedCache) :
   _isActive(true),
   _sharedCache(sharedCache),
   _tableMonitor(TR::Monitor::create("JIT-AOTDependencyTableMonitor")),
   _offsetMap(decltype(_offsetMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _methodMap(decltype(_methodMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _pendingLoads(decltype(_pendingLoads)::allocator_type(TR::Compiler->persistentAllocator()))
   { }

bool
TR_AOTDependencyTable::trackMethod(J9VMThread *vmThread, J9Method *method, J9ROMMethod *romMethod, bool &dependenciesSatisfied)
   {
   const uintptr_t *methodDependencies = NULL;
   if (!_sharedCache->methodHasAOTBodyWithDependencies(vmThread, romMethod, methodDependencies))
      return false;

   if (!methodDependencies)
      {
      dependenciesSatisfied = true;
      if (TR::Options::getVerboseOption(TR_VerboseDependencyTracking))
         TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "Dependency table: method %p with 0 dependencies will start at count 0", method);

      return true;
      }

   OMR::CriticalSection cs(_tableMonitor);
   if (!isActive())
      return false;

   try
      {
      uintptr_t totalDependencies = *methodDependencies;
      uintptr_t numberRemainingDependencies = totalDependencies;

      auto m_it = _methodMap.insert({method, {0, methodDependencies}});
      auto methodEntry = &(*m_it.first);

      for (size_t i = 1; i <= totalDependencies; ++i)
         {
         bool needsInitialization = false;
         uintptr_t chainOffset = decodeDependencyOffset(methodDependencies[i], needsInitialization);
         uintptr_t offset = _sharedCache->startingROMClassOffsetOfClassChain(_sharedCache->pointerFromOffsetInSharedCache(chainOffset));
         auto entry = getOffsetEntry(offset, true);
         if (needsInitialization)
            entry->_waitingInitMethods.insert(methodEntry);
         else
            entry->_waitingLoadMethods.insert(methodEntry);

         if (findCandidateForDependency(entry->_loadedClasses, needsInitialization))
            numberRemainingDependencies -= 1;
         }

      if (numberRemainingDependencies == 0)
         {
         stopTracking(methodEntry, false);
         dependenciesSatisfied = true;

         if (TR::Options::getVerboseOption(TR_VerboseDependencyTracking))
            TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "Dependency table: method %p with %lu dependencies will start at count 0", method, totalDependencies);
         }
      else
         {
         methodEntry->second._remainingDependencies = numberRemainingDependencies;
         if (TR::Options::getVerboseOption(TR_VerboseDependencyTracking))
            TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "Dependency table: method %p with %lu dependencies will be tracked with %lu remaining",
                                           method,
                                           totalDependencies,
                                           numberRemainingDependencies);
         }
      }
   catch (std::exception&)
      {
      deactivateTable();
      return false;
      }

   return true;
   }

void
TR_AOTDependencyTable::methodWillBeCompiled(J9Method *method)
   {
   OMR::CriticalSection cs(_tableMonitor);
   if (!isActive())
      return;

   // TODO: For now we simply stop tracking method if for some reason a
   // compilation is triggered for it, but if the compilation in question is an
   // AOT load we might consider preventing the load from taking place (by
   // increasing the counts and continuing to track the method, or marking the
   // method as ineligible for loads and giving up on tracking it).
   stopTracking(method, true);
   }

void
TR_AOTDependencyTable::stopTracking(MethodEntryRef entry, bool isEarlyStop)
   {
   auto method = entry->first;
   auto methodEntry = entry->second;
   auto dependencyChain = methodEntry._dependencyChain;
   auto dependencyChainLength = *dependencyChain;

   bool verboseUnsatisfied = isEarlyStop && TR::Options::getVerboseOption(TR_VerboseDependencyTracking);
   bool verboseUnsatisfiedDetails = isEarlyStop && TR::Options::getVerboseOption(TR_VerboseDependencyTrackingDetails);

   if (verboseUnsatisfied)
      TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "Dependency table: early tracking stop for method %p with %lu remaining dependencies", method, methodEntry._remainingDependencies);

   for (size_t i = 1; i <= dependencyChainLength; ++i)
      {
      bool needsInitialization = false;
      uintptr_t chainOffset = decodeDependencyOffset(dependencyChain[i], needsInitialization);
      uintptr_t offset = _sharedCache->startingROMClassOffsetOfClassChain(_sharedCache->pointerFromOffsetInSharedCache(chainOffset));

      auto o_it = _offsetMap.find(offset);

      if (verboseUnsatisfiedDetails && !findCandidateForDependency(o_it->second._loadedClasses, needsInitialization))
         TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "Dependency table: stopped tracking method %p with unsatisfied dependency chainOffset=%lu romClassOffset=%lu needsInit=%d",
                                        method, chainOffset, offset, needsInitialization);

      if (needsInitialization)
         o_it->second._waitingInitMethods.erase(entry);
      else
         o_it->second._waitingLoadMethods.erase(entry);

      eraseOffsetEntryIfEmpty(o_it->second, offset);
      }

   _methodMap.erase(entry->first);
   }

void
TR_AOTDependencyTable::stopTracking(J9Method *method, bool isEarlyStop)
   {
   auto entry = _methodMap.find(method);
   if (entry != _methodMap.end())
      stopTracking(&*entry, isEarlyStop);
   }

void
TR_AOTDependencyTable::eraseOffsetEntryIfEmpty(const OffsetEntry &entry, uintptr_t offset)
   {
   if (entry._loadedClasses.empty() && entry._waitingInitMethods.empty() && entry._waitingLoadMethods.empty())
      _offsetMap.erase(offset);
   }

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
      return;
      }

   resolvePendingLoads();
   }

void
TR_AOTDependencyTable::registerSatisfaction(PersistentUnorderedSet<MethodEntryRef> waitingMethods)
   {
   for (auto &entry: waitingMethods)
      {
      if (entry->second._remainingDependencies == 1)
         _pendingLoads.insert(entry);
      else
         --entry->second._remainingDependencies;
      }
   }

void
TR_AOTDependencyTable::classLoadEventAtOffset(J9Class *ramClass, uintptr_t offset, bool isClassLoad, bool isClassInitialization)
   {
   auto offsetEntry = getOffsetEntry(offset, isClassLoad);

   // We only need to check for chain validity on load, because for
   // initialization (that isn't a simultaneous load) we can simply check to see
   // if ramClass is already tracked and abort the update if not.
   if (!offsetEntry)
      return;
   if (!isClassLoad && (offsetEntry->_loadedClasses.find(ramClass) == offsetEntry->_loadedClasses.end()))
      return;

   if (TR::Options::getVerboseOption(TR_VerboseDependencyTracking))
      {
      auto name = J9ROMCLASS_CLASSNAME(ramClass->romClass);
      TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "Dependency table: class load event %.*s ramClass=%p romClassOffset=%lu isLoad=%d isInit=%d",
                                     J9UTF8_LENGTH(name), J9UTF8_DATA(name), ramClass, offset, isClassLoad, isClassInitialization);
      }

   // Check for dependency satisfaction if this is the first class initialized
   // for this offset.
   if (isClassInitialization)
      {
      // TODO: need to confirm that ramClass->initializeStatus will never itself
      // be J9ClassInitSucceeded, in which case the loop below can be replaced
      // with !findCandidateForDependency()
      bool existingInit = false;
      for (const auto& entry: offsetEntry->_loadedClasses)
         {
         if ((J9ClassInitSucceeded == entry->initializeStatus) && (entry != ramClass))
            {
            existingInit = true;
            break;
            }
         }
      if (!existingInit)
         registerSatisfaction(offsetEntry->_waitingInitMethods);
      }

   // Track the class, and also check for dependency satisfaction if this is the
   // first class loaded for this offset
   if (isClassLoad)
      {
      if (!findCandidateForDependency(offsetEntry->_loadedClasses, false))
         registerSatisfaction(offsetEntry->_waitingLoadMethods);
      offsetEntry->_loadedClasses.insert(ramClass);
      }
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
      PersistentUnorderedSet<MethodEntryRef> waitingLoadMethods(PersistentUnorderedSet<MethodEntryRef>::allocator_type(TR::Compiler->persistentAllocator()));
      PersistentUnorderedSet<MethodEntryRef> waitingInitMethods(PersistentUnorderedSet<MethodEntryRef>::allocator_type(TR::Compiler->persistentAllocator()));
      return &(*_offsetMap.insert(it, {offset, {loadedClasses, waitingLoadMethods, waitingInitMethods}})).second;
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

   auto ramClass = (J9Class *)clazz;
   if (invalidateClassAtOffset(ramClass, classOffset))
      invalidateMethodsOfClass(ramClass);
   }

void
TR_AOTDependencyTable::registerDissatisfaction(PersistentUnorderedSet<MethodEntryRef> waitingMethods)
   {
   for (auto& entry: waitingMethods)
      {
      ++entry->second._remainingDependencies;
      _pendingLoads.erase(entry);
      }
   }

bool
TR_AOTDependencyTable::invalidateClassAtOffset(J9Class *ramClass, uintptr_t romClassOffset)
   {
   auto entry = getOffsetEntry(romClassOffset, false);
   if (entry)
      {
      if (TR::Options::getVerboseOption(TR_VerboseDependencyTracking))
         TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "Dependency table: invalidating class %p romClassOffset=%lu", ramClass, romClassOffset);

      entry->_loadedClasses.erase(ramClass);

      // Update the waiting methods if the removal of ramClass caused a
      // dependency to become unsatisfied
      if (entry->_loadedClasses.empty())
         {
         registerDissatisfaction(entry->_waitingLoadMethods);
         registerDissatisfaction(entry->_waitingInitMethods);
         eraseOffsetEntryIfEmpty(*entry, romClassOffset);
         }
      else if (!findCandidateForDependency(entry->_loadedClasses, true))
         {
         registerDissatisfaction(entry->_waitingInitMethods);
         }

      return true;
      }

   return false;
   }

void
TR_AOTDependencyTable::invalidateMethodsOfClass(J9Class *ramClass)
   {
   for (uint32_t i = 0; i < ramClass->romClass->romMethodCount; i++)
      stopTracking(&ramClass->ramMethods[i], true);
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
            invalidateMethodsOfClass((J9Class *)oldClass);
            auto freshRamClass = (J9Class *)freshClass;
            bool initialized = J9ClassInitSucceeded == freshRamClass->initializeStatus;
            classLoadEventAtOffset(freshRamClass, freshClassOffset, true, initialized);
            }
         }
      catch (std::exception&)
         {
         deactivateTable();
         return;
         }

      resolvePendingLoads();
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
      // Invalidate the methods of oldClass first, so _pendingLoads doesn't have
      // to be cleared of invalidated method entries
      invalidateMethodsOfClass((J9Class *)oldClass);
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

   resolvePendingLoads();
   }

void
TR_AOTDependencyTable::resolvePendingLoads()
   {
   for (auto& entry: _pendingLoads)
      {
      auto method = entry->first;
      auto initCount = TR::CompilationInfo::getInvocationCount(method);
      auto count = initCount;
      while (count > 0)
         {
         if (TR::CompilationInfo::setInvocationCount(method, count, 0))
            count = 0;
         else
            count = TR::CompilationInfo::getInvocationCount(method);
         }

      if (TR::Options::isAnyVerboseOptionSet(TR_VerboseDependencyTracking, TR_VerboseCounts))
         TR_VerboseLog::writeLineLocked(TR_Vlog_INFO,
                                        "Dependency table: pending load %s - method %p count %lu --> %lu",
                                        (count == 0) ? "success" : "failure",
                                        method,
                                        initCount,
                                        count);

      stopTracking(entry, false);
      }
   _pendingLoads.clear();
   }

J9Class *
TR_AOTDependencyTable::findCandidateWithChainAndLoader(TR::Compilation *comp, uintptr_t classChainOffset, void *classLoaderChain)
   {
   if (comp->isDeserializedAOTMethod() || comp->ignoringLocalSCC())
      return NULL;

   auto classChain = (uintptr_t *)_sharedCache->pointerFromOffsetInSharedCache(classChainOffset);
   return findChainLoaderCandidate(comp, classChain, classLoaderChain);
   }

J9Class *
TR_AOTDependencyTable::findCandidateWithChainAndLoader(TR::Compilation *comp, uintptr_t *classChain, void *classLoaderChain)
   {
   if (comp->isDeserializedAOTMethod() || comp->ignoringLocalSCC())
      return NULL;

   return findChainLoaderCandidate(comp, classChain, classLoaderChain);
   }

J9Class *
TR_AOTDependencyTable::findChainLoaderCandidate(TR::Compilation *comp, uintptr_t *classChain, void *classLoaderChain)
   {
   TR_ASSERT_FATAL(classLoaderChain, "Must be given a loader chain");

   uintptr_t romClassOffset = _sharedCache->startingROMClassOffsetOfClassChain(classChain);

   OMR::CriticalSection cs(_tableMonitor);

   if (!isActive())
      return NULL;

   auto it = _offsetMap.find(romClassOffset);
   if (it == _offsetMap.end())
      return NULL;

   for (const auto& clazz: it->second._loadedClasses)
      {
      // Init condition taken from jitGetClassInClassloaderFromUTF8()
      if ((J9ClassInitFailed != clazz->initializeStatus) &&
          (_sharedCache->persistentClassLoaderTable()->lookupClassChainAssociatedWithClassLoader(clazz->classLoader) == classLoaderChain))
         return clazz;
      }

   return NULL;
   }

void
TR_AOTDependencyTable::printStats()
   {
   TR_VerboseLog::CriticalSection vlogLock;

   TR_VerboseLog::writeLine(TR_Vlog_INFO, "Dependency table: %lu methods remain tracked", _methodMap.size());

   for (auto methodEntry : _methodMap)
      stopTracking(&methodEntry, true);
   }

J9Class *
TR_AOTDependencyTable::findCandidateForDependency(const PersistentUnorderedSet<J9Class *> &loadedClasses, bool needsInitialization)
   {
   for (const auto& clazz: loadedClasses)
      {
      if (!needsInitialization || (J9ClassInitSucceeded == clazz->initializeStatus))
         return clazz;
      }

   return NULL;
   }

void
TR_AOTDependencyTable::deactivateTable()
   {
   _offsetMap.clear();
   _methodMap.clear();
   _pendingLoads.clear();
   setInactive();
   }

#endif /* !defined(PERSISTENT_COLLECTIONS_UNSUPPORTED) */
