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
   static Entry *get(J9ROMClass *romClass)
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


JITServerSharedROMClassCache::JITServerSharedROMClassCache() :
   _persistentMemory(TR::Compiler->persistentGlobalMemory()),
   _map(decltype(_map)::allocator_type(TR::Compiler->persistentGlobalAllocator())),
   _monitor(TR::Monitor::create("JIT-JITServerSharedROMClassCacheMonitor"))
   {
   if (!_monitor)
      throw std::bad_alloc();
   }

JITServerSharedROMClassCache::~JITServerSharedROMClassCache()
   {
   for (const auto &kv : _map)
      _persistentMemory->freePersistentMemory(kv.second);
   _monitor->destroy();
   }


J9ROMClass *
JITServerSharedROMClassCache::getOrCreate(const J9ROMClass *packedROMClass)
   {
   JITServerROMClassHash hash(packedROMClass);

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
         entry->_hash = &it.first->first;
      else
         romClass = it.first->second->acquire();// Another thread already created this entry; reuse it
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
JITServerSharedROMClassCache::release(J9ROMClass *romClass)
   {
   auto entry = Entry::get(romClass);
   // To reduce lock contention, we synchronize access to the reference count
   // using atomic operations. Releasing a ROMClass doesn't require the monitor
   // unless it's the last reference. This should help in the scenario when a
   // client session is destroyed and all its cached ROMClasses are released.
   if (entry->release() != 0)
      return;

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
