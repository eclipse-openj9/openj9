/*******************************************************************************
 * Copyright IBM Corp. and others 2025
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
#include "runtime/JITServerAOTCache.hpp"
#include "runtime/JITServerProfileCache.hpp"
#include "runtime/JITServerSharedROMClassCache.hpp"

ProfiledMethodEntry::BytecodeProfile::BytecodeProfile() :
   _numSamples(0),
   _data(decltype(_data)::allocator_type(TR::Compiler->persistentGlobalAllocator())),
   _stable(false)
   {
   }

ProfiledMethodEntry::BytecodeProfile::~BytecodeProfile()
   {
   clear();
   }

/**
 * @brief Return a summary of the bytecode profiling data for this method.
 *        The summary includes the total number of profiling samples,
 *        the number of bytecodes that have profiling information and
 *        whether the profiling information is 'stable'.
 *        This information will be used to estimate the quality of the profiling data.
 * @return BytecodeProfileSummary
 */
BytecodeProfileSummary
ProfiledMethodEntry::BytecodeProfile::getSummary() const
   {
   return BytecodeProfileSummary(getNumSamples(), numProfiledBytecodes(), _stable);
   }


/**
 * @brief Discard the profiling data for this method
 */
void
ProfiledMethodEntry::BytecodeProfile::clear()
   {
   for (auto entry : _data)
      {
      // delete entry
      entry->~TR_IPBytecodeHashTableEntry();
      TR::Compiler->persistentGlobalMemory()->freePersistentMemory(entry);
      }
   _data.clear();
   _numSamples = 0;
   _stable = false;
   }

/**
 * @brief Add bytecode profiling data to the shared profile repository for a method
 *
 * @param entries Vector of profiling entries to be added to this method's bytecode profile
 * @param numSamples Total number of profiling samples for this method
 * @param isStable 'true' if the profiling data quality is not expected to grow in the future
 */
void
ProfiledMethodEntry::BytecodeProfile::addBytecodeData(const Vector<TR_IPBytecodeHashTableEntry *> &entries, uint64_t numSamples, bool isStable)
   {
   _data.reserve(entries.size());
   // entries passed in use stack memory; we need to allocate clones with global persistent memory
   for (const auto &entry : entries)
      {
      TR_IPBytecodeHashTableEntry *newEntry = entry->clone(TR::Compiler->persistentGlobalMemory(), TR_Memory::JITServerProfileCache);
      _data.push_back(newEntry);
      }
   _numSamples = numSamples;
   _stable = isStable;
   }

ProfiledMethodEntry::ProfiledMethodEntry(const AOTCacheMethodRecord *methodRecord, const J9ROMMethod *romMethod, J9ROMClass *romClass) :
   _methodRecord(methodRecord), _romMethod(romMethod), _romClass(romClass),
   _bytecodeProfile(NULL), _faninProfile(NULL)
   {
   // Increase the reference count to prevent deletion of this romClass from the shared repository
   TR::CompilationInfo::get()->getJITServerSharedROMClassCache()->acquire(_romClass);
   }

ProfiledMethodEntry::~ProfiledMethodEntry()
   {
   // Decrement the reference count to allow deletion of this romClass from the shared repository
   TR::CompilationInfo::get()->getJITServerSharedROMClassCache()->release(_romClass);

   deleteBytecodeData();

   if (_faninProfile)
      {
      // TODO: free more stuff here once we add it
      _faninProfile->~FanInProfile();
      TR::Compiler->persistentGlobalMemory()->freePersistentMemory(_faninProfile);
      _faninProfile = NULL;
      }
   }

BytecodeProfileSummary
ProfiledMethodEntry::getBytecodeProfileSummary() const
   {
   return _bytecodeProfile ? _bytecodeProfile->getSummary() : BytecodeProfileSummary();
   }

/**
 * @brief Delete all bytecode profiling data for this method
 * @note JITServerSharedProfileCache monitor must be held.
 */
void
ProfiledMethodEntry::deleteBytecodeData()
   {
   if (_bytecodeProfile)
      {
      _bytecodeProfile->~BytecodeProfile();
      TR::Compiler->persistentGlobalMemory()->freePersistentMemory(_bytecodeProfile);
      _bytecodeProfile = NULL;
      }
   }

/**
 * @brief Add bytecode profiling data to the shared profile repository for a method.
 *        If bytecode profiling data already exists, it will be deleted first.
 * @param entries Vector of profiling entries to be added to this method's profile
 * @param numSamples Total number of profiling samples for this method
 * @param isStable 'true' if the profiling data quality is not expected to grow in the future
 * @return 'true' if profiling added was added successfully
 * @note JITServerSharedProfileCache monitor must be held held.
 */
bool
ProfiledMethodEntry::addBytecodeData(const Vector<TR_IPBytecodeHashTableEntry *> &entries, uint64_t numSamples, bool isStable)
   {
   // Existing data will be deleted. However, there is a possibility of an allocation failure for
   // the new data and therefore only partial new data will be stored (or none at all). To cope with
   // this situation we allocate new data in a shadow vector and only if everything went all we
   // replace the existing data with the shadow vector.
   bool success = false;
   try
      {
      // Create new profile in a temporary
      BytecodeProfile *newBytecodeProfile = new (TR::Compiler->persistentGlobalAllocator()) BytecodeProfile();
      newBytecodeProfile->addBytecodeData(entries, numSamples, isStable);
      // Delete old profile
      deleteBytecodeData();
      // Attach the new profile
      _bytecodeProfile = newBytecodeProfile;
      success = true;
      }
   catch (const std::bad_alloc &allocationFailure)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfile))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "WARNING: Allocation failure while adding profile data to shared repository: %s", allocationFailure.what());
      }
   return success;
   }

/**
 * @brief Copy out the shared profiling data for this method into a vector of pointers
 *        to newly allocated 'TR_IPBytecodeHashTableEntry' entries. The allocations will
 *        use persistentMemory if the 'stable' parameter is 'true' or heapMemory specific
 *        to the compilation in progress, if 'stable' is false.
 * @param trMemory Reference to TR_Memory for current compilation
 * @param stable Boolean indicating whether the profling info cannot grow in the future.
 * @param newEntries OUT Vector of pointers to TR_IPBytecodeHashTableEntry entries. The
 *                   entries are allocated either with "heap" memory or with persistent memory.
 * @param cgEntries OUT Vector of pointers to TR_IPBCDataCallGraph entries that need to be adjusted.
 *                  Note that these entries are also present in `newEntries`
 * @note JITServerSharedProfileCache monitor must be held.
 */
void
ProfiledMethodEntry::cloneBytecodeData(TR_Memory &trMemory, bool stable,
                                       Vector<TR_IPBytecodeHashTableEntry *> &newEntries,
                                       Vector<TR_IPBCDataCallGraph *> &cgEntries) const
   {
   TR_ASSERT_FATAL(_bytecodeProfile, "We must have _bytecodeProfile if the caller decided to take profiled information from the shared repository");

   size_t numProfileEntries = _bytecodeProfile->getData().size();

   newEntries.reserve(numProfileEntries);
   cgEntries.reserve(numProfileEntries);

   for (const auto& entry : _bytecodeProfile->getData())
      {
      // Clone the entries using the appropriate memory type
      TR_IPBytecodeHashTableEntry *newEntry = NULL;
      if (stable)
         {
         // Use persistent memory specific to this client
         newEntry = entry->clone(trMemory.trPersistentMemory());
         }
      else // Non-stable data; use heap memory associated with this compilation
         {
         newEntry = entry->clone(trMemory.heapMemoryRegion());
         }
      newEntries.push_back(newEntry);
      // Remember the callGraph entries so that we can adjust the slots
      TR_IPBCDataCallGraph *cgEntry = entry->asIPBCDataCallGraph();
      if (cgEntry)
         cgEntries.push_back(cgEntry);
      } // end for
   }

/**
 * @brief Constructor for JITServerSharedProfileCache
 * @todo Consider using a dedicated persistent allocator rather than the global one
 */
JITServerSharedProfileCache::JITServerSharedProfileCache(JITServerAOTCache *aotCache, J9JavaVM *javaVM) :
   _aotCache(aotCache),
   _methodProfileMap(decltype(_methodProfileMap)::allocator_type(TR::Compiler->persistentGlobalAllocator())),
   _monitor(TR::Monitor::create("JIT-SharedProfileCacheMonitor")),
   _numStores(0), _numOverwrites(0)
   //,_rawAllocator(javaVM), _segmentAllocator(MEMORY_TYPE_JIT_SCRATCH_SPACE | MEMORY_TYPE_VIRTUAL, *javaVM),
   //TODO: configurable scratch memory limit
   //_segmentProvider(64 * 1024, 16 * 1024 * 1024, 16 * 1024 * 1024, _segmentAllocator, _rawAllocator),
   //_region(_segmentProvider, _rawAllocator), _trMemory(*TR::Compiler->persistentGlobalMemory(), _region)
   {
   if (!_monitor)
      throw std::bad_alloc();
   }

JITServerSharedProfileCache::~JITServerSharedProfileCache()
   {
   TR::Monitor::destroy(_monitor);
   }

/**
 * @brief Find the profiling info specific to the method of interest and return a pointer to it
 * @note Must hold the JITServerSharedProfileCache monitor
 * @param methodRecord: the method for which we are looking for profiling data
 * @return Pointer to the shared profiling data for the method of interest
 */
ProfiledMethodEntry *
JITServerSharedProfileCache::getProfileForMethod(const AOTCacheMethodRecord *methodRecord)
   {
   TR_ASSERT(_monitor->owned_by_self(), "Must hold monitor");

   auto it = _methodProfileMap.find(methodRecord);
   if (it != _methodProfileMap.end())
     return &it->second;

   return NULL;
   }

/**
 * @brief Adds bytecode profiling data to the shared profile repository
 * @note Existing profiling data for the method in question (if available) will be overwritten
 *       but we may refuse to add the new data if better data already exists
 * @note Acquires the shared profile repository monitor.
 * @param entries Vector of pointers to TR_IPBytecodeHashTableEntry entities to be added (stack allocated)
 * @param methodRecord The method record for which we add the profiling data
 * @param romMethod ROMMethod corresponding to the method for which we add the data
 * @param romClass ROMClass of the romMethod for which we add the data
 * @param numSamples Total number of profiling samples for the specified method
 * @param isStable 'true' if the profiling data quality is not expected to grow in the future
 * @return 'true' if the profiling data was successfully added to the shared repository, 'false' otherwise
 */
bool
JITServerSharedProfileCache::addBytecodeData(const Vector<TR_IPBytecodeHashTableEntry *> &entries,
                                             const AOTCacheMethodRecord *methodRecord,
                                             const J9ROMMethod *romMethod,
                                             J9ROMClass *romClass,
                                             uint64_t numSamples,
                                             bool isStable)
   {
   TR_ASSERT_FATAL(methodRecord, "methodRecord must exist");
   OMR::CriticalSection cs(monitor());
   auto it = _methodProfileMap.find(methodRecord);
   if (it == _methodProfileMap.end())
      {
      // At the moment there is no data for the method in question. Create an empty map.
      it = _methodProfileMap.emplace_hint(it, std::piecewise_construct, std::forward_as_tuple(methodRecord),
                                              std::forward_as_tuple(methodRecord, romMethod, romClass));
      }
   else // Some data already exists and could be overwritten
      {
      ProfiledMethodEntry::BytecodeProfile *storedProfile = it->second.getBytecodeProfile();
      // Two threads may want to store the same data one after another (I have seen this happening).
      // Prevent unnecesary work if the quality of the new data is the same or lower.
      if (storedProfile)
         {
         BytecodeProfileSummary clientProfileSummary(numSamples, entries.size(), isStable);
         BytecodeProfileSummary sharedProfileSummary = storedProfile->getSummary();
         int sharedProfileQuality = compareBytecodeProfiles(sharedProfileSummary, clientProfileSummary);
         if (sharedProfileQuality >= 0) // EXisting data is at least as good as the new data
            {
            if (TR::Options::getVerboseOption(TR_VerboseJITServerSharedProfile))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                  "WARNING: Avoiding overwriting existing shared bytecode profile data."
                  "New profiled bytecodes=%zu old profile bytecodes=%zu; newSamples=%" OMR_PRIu64 " oldSamples=%" OMR_PRIu64,
                  entries.size(), storedProfile->numProfiledBytecodes(), numSamples, storedProfile->getNumSamples());
            return false;
            }
         else // Existing data will be overwitten
            {
            _numOverwrites++;
            }
         }
      }
   _numStores++; // failed or not
   ProfiledMethodEntry &profiledMethodEntry = it->second;
   return profiledMethodEntry.addBytecodeData(entries, numSamples, isStable);
   // TODO: We may also fail due to reaching limit for AOT cache space
   }

void
JITServerSharedProfileCache::printStats(FILE *f) const
   {
   fprintf(f, "Stats about JITServer shared profile cache:\n");
   fprintf(f, "\tNum store operations: %zu\n", _numStores);
   fprintf(f, "\tNum overwrite operations: %zu\n", _numOverwrites);
   }

/**
 * @brief Compare two bytecode profiles based on heuristics.
 * @return If the first profile has better "quality" than the second profile, return 1.
 *         If the second profile has better "quality" than the first profile, return -1.
 *         If the "quality" of the two sources is comparable, return 0.
 */
int
JITServerSharedProfileCache::compareBytecodeProfiles(const BytecodeProfileSummary &profile1, const BytecodeProfileSummary &profile2)
   {
   if (profile1._numSamples > profile2._numSamples + 10 && profile1._numSamples * 15 > profile2._numSamples * 16) // 6.7% more samples
      {
      if (profile1._numProfiledBytecodes >= profile2._numProfiledBytecodes)
         {
         // More samples and more or the same number of profiled bytecode
         return 1;
         }
      else
         {
         // More samples, but less profiled bytecodes. Profile1 can be considered better
         // only if it has substantially more samples.
         if (profile1._numSamples * 3 > profile2._numSamples * 4) // 33% more samples
            return 1;
         else
            return 0; // 6.7% - 33% more samples, but lower number of profiled bytecodes
         }
      }
   else if (profile2._numSamples > profile1._numSamples + 10 && profile2._numSamples * 15 > profile1._numSamples * 16) // 6.7% more samples
      {
      if (profile2._numProfiledBytecodes >= profile1._numProfiledBytecodes)
         {
         // More samples and more or the same number of profiled bytecode
         return -1;
         }
      else
         {
         // More samples, but less profiled bytecodes. Profile2 can be considered better
         // only if it has substantially more samples.
         if (profile2._numSamples * 3 > profile1._numSamples * 4) // 33% more samples
            return -1;
         else
            return 0; // 6.7% - 33% more samples, but lower number of profiled bytecodes
         }
      }
   else // About the same number of samples. Let's look at bytecodes profiled
      {
      if (profile1._numProfiledBytecodes == profile2._numProfiledBytecodes)
         return 0;
      else if (profile1._numProfiledBytecodes > profile2._numProfiledBytecodes)
         return 1;
      else
         return -1;
      }
   }
