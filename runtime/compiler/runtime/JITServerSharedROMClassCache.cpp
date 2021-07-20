/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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
#include "AtomicSupport.hpp"
#include "control/CompilationRuntime.hpp"
#include "env/CompilerEnv.hpp"
#include "infra/CriticalSection.hpp"
#include "runtime/JITServerSharedROMClassCache.hpp"


#define JITSERVER_SHARED_ROMCLASS_EYECATCHER 0xC1A55E7E

struct JITServerSharedROMClassCache::Entry
   {
   Entry(const J9ROMClass *romClass) :
      _refCount(1), _hash(NULL), _eyeCatcher(JITSERVER_SHARED_ROMCLASS_EYECATCHER)
      {
      memcpy(_data, romClass, romClass->romSize);
      }

   // The ROMClass is embedded in this structure in order to avoid exposing it in the API
   static Entry *get(const J9ROMClass *romClass)
      {
      auto entry = (Entry *)((uint8_t *)romClass - offsetof(Entry, _data));
      TR_ASSERT_FATAL(entry->_eyeCatcher == JITSERVER_SHARED_ROMCLASS_EYECATCHER,
                      "ROMClass not embedded in cache entry");
      return entry;
      }

   J9ROMClass *acquire()
      {
      VM_AtomicSupport::add(&_refCount, 1);
      return (J9ROMClass *)_data;
      }

   // Returns new reference count
   size_t release() { return VM_AtomicSupport::subtract(&_refCount, 1); }

   volatile size_t _refCount;
   // Store the pointer to this entry's key so that we don't have to
   // recompute it when deleting the entry from the map.
   //NOTE: The entity pointed to by _hash is not owned by this Entry,
   //      and must not be deleted when the Entry is destroyed.
   const JITServerROMClassHash *_hash;
   const size_t _eyeCatcher;
   uint8_t _data[];// embedded J9ROMClass
   };


struct JITServerSharedROMClassCache::Partition
   {
   Partition(TR_PersistentMemory *persistentMemory, TR::Monitor *monitor) :
      _persistentMemory(persistentMemory), _monitor(monitor),
      _map(decltype(_map)::allocator_type(persistentMemory->_persistentAllocator.get())),
      _maxSize(0) { }

   ~Partition()
      {
      for (const auto &kv : _map)
         _persistentMemory->freePersistentMemory(kv.second);
      }

   J9ROMClass *getOrCreate(const J9ROMClass *packedROMClass, const JITServerROMClassHash &hash);
   void release(Entry *entry);

   TR_PersistentMemory *const _persistentMemory;
   TR::Monitor *const _monitor;
   // To avoid comparing the ROMClass contents inside a critical section when
   // inserting a new entry (which would increase lock contention), we instead
   // use a hash of the contents as the key. The hash is computed outside of
   // the critical section, and key hashing and comparison are very quick.
   PersistentUnorderedMap<JITServerROMClassHash, Entry *> _map;
   size_t _maxSize;
   };


JITServerSharedROMClassCache::JITServerSharedROMClassCache(size_t numPartitions) :
   _numPartitions(numPartitions), _persistentMemory(NULL),
   _partitions((Partition *)TR::Compiler->persistentGlobalMemory()->allocatePersistentMemory(
               numPartitions * sizeof(Partition), TR_Memory::ROMClass)),
   _monitors((TR::Monitor **) TR::Compiler->persistentGlobalMemory()->allocatePersistentMemory(
               numPartitions * sizeof(TR::Monitor *), TR_Memory::ROMClass))
   {
   if (!_partitions || !_monitors)
      throw std::bad_alloc();

   for (size_t i = 0; i < numPartitions; ++i)
      {
      _monitors[i] = TR::Monitor::create("JIT-JITServerSharedROMClassCachePartitionMonitor");
      if (!_monitors[i])
         throw std::bad_alloc();
      }
   }

JITServerSharedROMClassCache::~JITServerSharedROMClassCache()
   {
   if (isInitialized())
      shutdown(false);

   for (size_t i = 0; i < _numPartitions; ++i)
      TR::Monitor::destroy(_monitors[i]);

   TR::Compiler->persistentGlobalAllocator().deallocate(_partitions);
   TR::Compiler->persistentGlobalAllocator().deallocate(_monitors);
   }


void
JITServerSharedROMClassCache::initialize(J9JITConfig *jitConfig)
   {
   TR_ASSERT(!isInitialized(), "Already initialized");

   // Must only be called when the first client session is created
   auto compInfo = TR::CompilationInfo::get();
   TR_ASSERT(compInfo->getCompilationMonitor()->owned_by_self(), "Must hold compilationMonitor");
   TR_ASSERT(compInfo->getClientSessionHT()->size() == 0, "Must have no clients");

   TR::PersistentAllocatorKit kit(1 << 20/*1 MB*/, *TR::Compiler->javaVM);
   auto allocator = new (TR::Compiler->rawAllocator) TR::PersistentAllocator(kit);
   try
      {
      _persistentMemory = new (TR::Compiler->rawAllocator) TR_PersistentMemory(jitConfig, *allocator);
      for (size_t i = 0; i < _numPartitions; ++i)
         new (&_partitions[i]) Partition(_persistentMemory, _monitors[i]);
      }
   catch (...)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "ERROR: Failed to initialize shared ROMClass cache");
      // This automatically releases all resources (persistent allocations) in Partition objects
      allocator->~PersistentAllocator();
      TR::Compiler->rawAllocator.deallocate(allocator);
      if (_persistentMemory)
         TR::Compiler->rawAllocator.deallocate(_persistentMemory);
      _persistentMemory = NULL;
      throw;
      }
   }

void
JITServerSharedROMClassCache::shutdown(bool lastClient)
   {
   TR_ASSERT(isInitialized(), "Must be initialized");

   if (lastClient)
      {
      // Must only be called when the last client session is destroyed
      auto compInfo = TR::CompilationInfo::get();
      TR_ASSERT(compInfo->getCompilationMonitor()->owned_by_self(), "Must hold compilationMonitor");
      TR_ASSERT(compInfo->getClientSessionHT()->size() == 0, "Must have no clients");

      // There should be no ROMClasses left in the cache if there are no clients using them
      size_t numClasses = 0, maxClasses = 0;
      for (size_t i = 0; i < _numPartitions; ++i)
         {
         numClasses += _partitions[i]._map.size();
         maxClasses += _partitions[i]._maxSize;
         }
      if (numClasses)
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(
               TR_Vlog_JITServer, "ERROR: %zu / %zu classes left in shared ROMClass cache at shutdown",
               numClasses, maxClasses
            );
         //NOTE: This assertion is not fatal since there are known cases when a cached
         //      ROMClass is abandoned without decrementing its reference count,
         //      e.g. due to mishandled exceptions or races between multiple threads
         TR_ASSERT(false, "%zu / %zu classes left in shared ROMClass cache at shutdown",
                   numClasses, maxClasses);
         }
      }

#if defined(DEBUG)
   // Calling destructors is not necessary - memory will be freed automatically with the persistent allocator
   for (size_t i = 0; i < _numPartitions; ++i)
      _partitions[i].~Partition();
#endif /* defined(DEBUG) */

   auto allocator = &_persistentMemory->_persistentAllocator.get();
   // This automatically releases all resources (persistent allocations) in Partition objects
   allocator->~PersistentAllocator();
   TR::Compiler->rawAllocator.deallocate(allocator);
   TR::Compiler->rawAllocator.deallocate(_persistentMemory);
   _persistentMemory = NULL;
   }


J9ROMClass *
JITServerSharedROMClassCache::getOrCreate(const J9ROMClass *packedROMClass)
   {
   JITServerROMClassHash hash(packedROMClass);
   return getPartition(hash).getOrCreate(packedROMClass, hash);
   }

void
JITServerSharedROMClassCache::release(J9ROMClass *romClass)
   {
   auto entry = Entry::get(romClass);
   // To reduce lock contention, we synchronize access to the reference count
   // using atomic operations. Releasing a ROMClass doesn't require the monitor
   // unless it's the last reference. This should help in the scenario when a
   // client session is destroyed and all its cached ROMClasses are released.
   if (entry->release() == 0)
      getPartition(*entry->_hash).release(entry);
   }

const JITServerROMClassHash &
JITServerSharedROMClassCache::getHash(const J9ROMClass *romClass)
   {
   return *Entry::get(romClass)->_hash;
   }

JITServerSharedROMClassCache::Partition &
JITServerSharedROMClassCache::getPartition(const JITServerROMClassHash &hash)
   {
   // Partition index for a given ROMClass is determined by word 1 of its hash
   // (word 0 is used by the hash function in the partition's unordered map)
   static_assert(ROMCLASS_HASH_WORDS >= 2, "ROMClass hash must have at least 2 words");
   return _partitions[hash.getWord(1) % _numPartitions];
   }


J9ROMClass *
JITServerSharedROMClassCache::Partition::getOrCreate(const J9ROMClass *packedROMClass,
                                                     const JITServerROMClassHash &hash)
   {
      {
      OMR::CriticalSection sharedROMClassCache(_monitor);
      auto it = _map.find(hash);
      if (it != _map.end())
         return it->second->acquire();// Reuse existing entry, incrementing its reference count
      }

   // Create new entry outside of the critical section to reduce lock contention
   size_t size = sizeof(Entry) + packedROMClass->romSize;
   void *ptr = _persistentMemory->allocatePersistentMemory(size, TR_Memory::ROMClass);
   if (!ptr)
      throw std::bad_alloc();
   auto entry = new (ptr) Entry(packedROMClass);
   auto romClass = (J9ROMClass *)entry->_data;

   try
      {
      OMR::CriticalSection sharedROMClassCache(_monitor);
      auto it = _map.insert({ hash, entry });
      if (it.second)
         {
         entry->_hash = &it.first->first;
         _maxSize = std::max(_maxSize, _map.size());
         }
      else
         {
         // Another thread already created this entry; reuse it
         romClass = it.first->second->acquire();
         }
      }
   catch (...)
      {
      // Prevent memory leak if map insertion failed
      _persistentMemory->freePersistentMemory(entry);
      throw;
      }

   // Free the newly allocated entry if it won't be used
   if (romClass != (J9ROMClass *)entry->_data)
      _persistentMemory->freePersistentMemory(entry);
   return romClass;
   }

void
JITServerSharedROMClassCache::Partition::release(JITServerSharedROMClassCache::Entry *entry)
   {
      {
      OMR::CriticalSection sharedROMClassCache(_monitor);
      // Another thread could have looked up and acquired this entry while its reference
      // count was 0, need to check again if it's still 0. The value is guaranteed to be
      // fresh since the field is declared volatile (so that the compiler will generate
      // a memory read), and the monitor acquisition above implies a memory barrier.
      if (entry->_refCount != 0)
         return;

      auto it = _map.find(*entry->_hash);
      TR_ASSERT(it != _map.end(), "Entry to be removed not found");
      TR_ASSERT(it->second == entry, "Duplicate entry");
      _map.erase(it);
      }

   _persistentMemory->freePersistentMemory(entry);
   }
