/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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
#ifndef JITSERVER_ROMCLASS_CACHE_H
#define JITSERVER_ROMCLASS_CACHE_H

#include "env/TRMemory.hpp"
#include "env/PersistentCollections.hpp"
#include "infra/Monitor.hpp"
#include "runtime/JITServerROMClassHash.hpp"


// Stores a single copy of each distinct ROMClass that is shared by multiple
// client sessions in order to reduce JITServer memory usage.
class JITServerSharedROMClassCache
   {
public:
   TR_PERSISTENT_ALLOC(TR_Memory::ROMClass)

   //NOTE: The cache is not usable until initialize() is called
   JITServerSharedROMClassCache(size_t numPartitions);
   ~JITServerSharedROMClassCache();

   // Initializes the cache. Must be called when the first client session is created.
   void initialize(J9JITConfig *jitConfig);
   // Releases memory used by the cache. Must be called when the last client session is destroyed.
   void shutdown(bool lastClient = true);

   // Get an existing cache entry for packedROMClass or create one. The packedROMClassHash may be NULL; if not,
   // it will be the cached deterministic hash of packedROMClass received from the client.
   J9ROMClass *getOrCreate(const J9ROMClass *packedROMClass, const JITServerROMClassHash *packedROMClassHash);
   void release(J9ROMClass *romClass);

   // Get precomputed hash of a shared ROMClass
   static const JITServerROMClassHash &getHash(const J9ROMClass *romClass);

private:
   struct Entry;
   struct Partition;

   // To reduce lock contention, the cache is divided into a number of
   // partitions, each synchronized with a separate monitor
   Partition &getPartition(const JITServerROMClassHash &hash);

   void createPartitions();
   void destroyPartitions();

   bool isInitialized() const { return _persistentMemory != NULL; }

   const size_t _numPartitions;
   TR_PersistentMemory *_persistentMemory;
   Partition *const _partitions;
   TR::Monitor **const _monitors;
};


#endif /* JITSERVER_ROMCLASS_CACHE_H */
