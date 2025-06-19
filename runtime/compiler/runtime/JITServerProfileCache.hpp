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
#ifndef JITSERVER_PROFILE_CACHE_H
#define JITSERVER_PROFILE_CACHE_H
#include <stdint.h>
#include <stddef.h>
#include "env/PersistentCollections.hpp"
#include "env/TRMemory.hpp"

class TR_IPBytecodeHashTableEntry;
class TR_IPBCDataCallGraph;
class AOTCacheMethodRecord;
class TR_ContiguousIPMethodHashTableEntry;
class TR_FaninSummaryInfo;
namespace TR { class Monitor; }


struct BytecodeProfileSummary
   {
   BytecodeProfileSummary(uint64_t samples, size_t numBC, bool stable) :
      _numSamples(samples), _numProfiledBytecodes(numBC), _stable(stable) {}
   BytecodeProfileSummary() : _numSamples(0), _numProfiledBytecodes(0), _stable(false) {}
   uint64_t _numSamples;
   size_t _numProfiledBytecodes;
   bool _stable;
   };

struct FaninProfileSummary
   {
   FaninProfileSummary(uint64_t samples) : _numSamples(samples) {}
   uint64_t _numSamples;
   // Other fields could be added here if they are useful to comparing the quality of two sources
   };

// Profiling information at the server is kept on a per-method basis.
// Each ProfiledMethodEntry will keep profiling information related to
// (1) various method bytecodes and related to (2) method fan-in.
// The data should be easily serializable so that it can be saved to persistent storage.
class ProfiledMethodEntry
   {
   public:
   TR_PERSISTENT_ALLOC(TR_Memory::JITServerProfileCache)

   class BytecodeProfile
      {
      public:
      // Class that provides a summary of the profiling data
      BytecodeProfile();
      ~BytecodeProfile();
      uint64_t getNumSamples() const { return _numSamples; }
      void setNumSamples(uint64_t num) { _numSamples = num; }
      size_t numProfiledBytecodes() const { return _data.size(); }
      BytecodeProfileSummary getSummary() const;
      const PersistentVector<TR_IPBytecodeHashTableEntry *> &getData() const { return _data; }
      void addBytecodeData(const Vector<TR_IPBytecodeHashTableEntry *> &entries, uint64_t numSamples, bool isStable);
      void clear();

      private:
      uint64_t _numSamples; // Total profiling samples across all bytecodes
      // The actual data is a vector of profiling entries, one per bytecode of interest.
      // The vector could be totally empty, if the method has no bytecodes that can be profiled.
      // Storing empty profiles could be useful to avoid quering the client.
      // TODO: consider allocating the entries inline, as a stream of data. This means
      // that we will have a vector of bytes because the entries have different lengths.
      PersistentVector<TR_IPBytecodeHashTableEntry *> _data; // Allocated with persistentMemory using the global allocator
      bool _stable; // 'true' if method was compiled (or was under compilation) at the time when the profiling
                    // info was stored. Thus, we don't expect the quality of this data to increase in the buture.
      }; // BytecodeProfile
   class FaninProfile
      {
      public:
      FaninProfile() : _numSamples(0), _numSamplesOtherBucket(0), _numCallers(0) {}
      uint64_t getNumSamples() const { return _numSamples; }
      void setNumSamples(uint64_t num) { _numSamples = num; }
      uint64_t getNumSamplesOtherBucket() const { return _numSamplesOtherBucket; }
      void setNumSamplesOtherBucket(uint64_t num) { _numSamplesOtherBucket = num; }
      uint32_t getNumCallers() const { return _numCallers; }
      void setNumCallers(uint32_t n) { _numCallers = n; }
      FaninProfileSummary getSummary() const { return FaninProfileSummary(getNumSamples()); }
      // The fanin information kept by the client has 20 method buckets to track 20 different callers.
      // It also includes an 'otherBucket' to catch samples that fall on callers not captured by the 20
      // method buckets mentioned above.
      // To simplify implementation, the shared fanin profile kept by the server will not keep the
      // identity of the 20 tracked callers. Instead, the server will only remember the total number
      // of callers (capped at 20), the total number of samples for this callee and the number
      // of samples that fall in the 'otherBucket'
      uint64_t _numSamples; // Total number of fanin samples for this callee. TODO: do we need 64 bits for this?
      uint64_t _numSamplesOtherBucket; // Number of samples falling in the 'otherBucket'
      uint32_t _numCallers; // Number of different callers (max is MAX_IPMETHOD_CALLERS+1)
      }; // FaninProfile

   ProfiledMethodEntry(const AOTCacheMethodRecord *methodRecord, const J9ROMMethod *romMethod, J9ROMClass *romClass);
   ~ProfiledMethodEntry();
   uint64_t getNumSamples() const { return _bytecodeProfile ? _bytecodeProfile->getNumSamples() : 0; }
   BytecodeProfile *getBytecodeProfile() const { return _bytecodeProfile; }
   BytecodeProfileSummary getBytecodeProfileSummary() const;
   bool addBytecodeData(const Vector<TR_IPBytecodeHashTableEntry *> &entries, uint64_t numSamples, bool isStable);
   void cloneBytecodeData(TR_Memory &trMemory, bool stable,
                          Vector<TR_IPBytecodeHashTableEntry *> &newEntries,
                          Vector<TR_IPBCDataCallGraph *> &cgEntries) const;

   FaninProfile &getFaninProfileRef() { return _faninProfile; }
   FaninProfileSummary getFaninProfileSummary() const { return _faninProfile.getSummary(); };
   void addFanInData(uint64_t numSamples, uint64_t numSamplesOtherBucket, uint32_t numCallers)
      {
      _faninProfile.setNumSamples(numSamples);
      _faninProfile.setNumSamplesOtherBucket(numSamplesOtherBucket);
      _faninProfile.setNumCallers(numCallers);
      }

   private:
   void deleteBytecodeData();

   const AOTCacheMethodRecord *const _methodRecord; // Link back to the AOTCacheMethodRecord associated with this entry
   const J9ROMMethod *const _romMethod;
   J9ROMClass *const _romClass; // Counting references to shared ROMClass cache entry prevents deletion of profiling entries

   // Created on demand; will stay NULL if we never attempted to store corresponding data for this method
   BytecodeProfile *_bytecodeProfile;
   // FaninProfile is allocated inline since it has a small size. 0 samples is like the fanin profile has never been cached
   FaninProfile _faninProfile;
   }; // class ProfiledMethodEntry

class JITServerSharedProfileCache
   {
   public:
   // NOTE: AOT cache memory limit applies to profile cache as well
   TR_PERSISTENT_ALLOC(TR_Memory::JITServerProfileCache)

   JITServerSharedProfileCache(JITServerAOTCache *aotCache, J9JavaVM *javaVM);
   ~JITServerSharedProfileCache();

   TR::Monitor *monitor() const { return _monitor; }
   ProfiledMethodEntry *getProfileForMethod(const AOTCacheMethodRecord *);
   bool addBytecodeData(const Vector<TR_IPBytecodeHashTableEntry *> &entries, const AOTCacheMethodRecord *methodRecord,
                        const J9ROMMethod *romMethod, J9ROMClass *romClass, uint64_t numSamples, bool isStable);
   // Returns NULL with 'present' set to true if data for method is cached but empty
   TR_IPBytecodeHashTableEntry *getBytecodeData(const AOTCacheMethodRecord *methodRecord, uint32_t bci,
                                                bool &present, TR_Memory &trMemory, TR::Region &cgEntryRegion);
   bool addFaninData(const TR_ContiguousIPMethodHashTableEntry *serialEntry, const AOTCacheMethodRecord *methodRecord,
                     const J9ROMMethod *romMethod, J9ROMClass *romClass);
   TR_FaninSummaryInfo *getFaninData(const AOTCacheMethodRecord *methodRecord, TR_Memory *trMemory);
   size_t getNumStores() const { return _numStores; }
   size_t getNumOverwrites() const { return _numOverwrites; }
   void printStats(FILE *f) const;
   static int compareBytecodeProfiles(const BytecodeProfileSummary &profile1, const BytecodeProfileSummary &profile2);
   static int compareFaninProfiles(const FaninProfileSummary &profile1, const FaninProfileSummary &profile2);
   private:
   PersistentUnorderedMap<const AOTCacheMethodRecord *, ProfiledMethodEntry> _methodProfileMap;
   TR::Monitor *const _monitor;
   // The aotCache that is associated with this sharedProfileCache
   JITServerAOTCache *const _aotCache;
   // Statistics
   size_t _numStores;
   size_t _numOverwrites; // part of the store operations

   //TR::RawAllocator _rawAllocator;
   //J9::SegmentAllocator _segmentAllocator;
   //J9::SystemSegmentProvider _segmentProvider;
   //TR::Region _region;
   //TR_Memory _trMemory;
   }; // SharedProfileCache


#endif // JITSERVER_PROFILE_CACHE_H
