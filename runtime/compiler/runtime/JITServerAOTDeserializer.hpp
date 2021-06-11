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

#ifndef JITSERVER_AOT_DESERIALIZER_H
#define JITSERVER_AOT_DESERIALIZER_H

#include "env/TRMemory.hpp"
#include "env/PersistentCollections.hpp"
#include "runtime/JITServerAOTSerializationRecords.hpp"

class TR_PersistentClassLoaderTable;
namespace TR { class Compilation; }
namespace TR { class Monitor; }


// This class implements deserialization of cached AOT methods received from JITServer.
//
// Deserialization involves looking up classes, methods etc. by name, and computing hashes of packed ROMClasses
// (which can be a relatively heavy operation). To improve performance, the deserializer caches results of
// lookups and ROMClass hash validations (including mismatches) for each AOT serialization record.
//
// The IDs of newly cached records are communicated back to the server (with subsequent compilation requests)
// so that it doesn't keep sending records used by multiple methods (e.g. class and class chain records for
// all well-known classes are referred to by any AOT method compiled with SVM).
//
// The deserializer cache stores pointers to "RAM" entities, which have to be invalidated when classes and
// class loaders are unloaded, and SCC offsets to "ROM" entities, which always remain valid once cached.
//
// JIT client can reconnect do a different JITServer instance after the previous instance fails or shuts down.
// Since AOT cache record IDs are specific to a JITServer instance, the deserializer cache must be purged
// upon connecting to a new instance. This is done in the reset() function. Any concurrent deserialization
// (of a method received just before the server failure) detects that a reset is in progress and fails.
class JITServerAOTDeserializer
   {
public:
   TR_PERSISTENT_ALLOC(TR_Memory::JITServerAOTCache)

   JITServerAOTDeserializer(TR_PersistentClassLoaderTable *loaderTable);
   ~JITServerAOTDeserializer();

   void setSharedCache(TR_J9SharedCache *sharedCache) { _sharedCache = sharedCache; }

   // Deserializes in place a serialized AOT method received from JITServer. Returns true on success.
   // Caches new serialization records and adds their IDs to the set of new known IDs.
   bool deserialize(SerializedAOTMethod *method, const std::vector<std::string> &records,
                    TR::Compilation *comp);

   // Invalidation functions called from class and class loader unload JIT hooks to invalidate RAMClass
   // and class loader pointers cached by the deserializer. Note that cached SCC offsets stay valid.
   void invalidateClassLoader(J9VMThread *vmThread, J9ClassLoader *loader);
   void invalidateClass(J9VMThread *vmThread, J9Class *ramClass);

   // Invalidates all cached serialization records. Must be called when the client
   // connects to a new server instance (e.g. upon receving a VM_getVMInfo message),
   // before attempting to deserialize any method received from the new server instance.
   void reset();

   // IDs of records newly cached during deserialization of an AOT method are sent to the JITServer with
   // the next compilation request, so that the server can update its set of known IDs for this client.
   // This function returns the list of IDs cached since the last call, and clears the set of new known IDs.
   std::vector<uintptr_t/*idAndType*/> getNewKnownIds();

private:
   struct ClassLoaderEntry
      {
      J9ClassLoader *_loader;// NULL if class loader was unloaded
      uintptr_t _loaderChainSCCOffset;
      };

   struct ClassEntry
      {
      J9Class *_ramClass;// NULL if class ID is invalid (was not found or its hash didn't match), or class was unloaded
      uintptr_t _romClassSCCOffset;// -1 if class ID is invalid
      uintptr_t _loaderChainSCCOffset;
      };

   bool isResetInProgress(bool &wasReset) { return _resetInProgress ? (wasReset = true) : false; }

   //NOTE: All the functions below that take a 'bool &wasReset' argument set it to true
   //      if the operation failed due to a concurrent reset of the deserializer.

   // Deserializes/validates and caches an AOT serialization record.
   // Returns true if the record is valid (e.g. class was found and its hash matches),
   // and sets isNew to true if the record was newly cached (not already known).
   // Returns false if the record is invalid (e.g. ROMClass hash doesn't match)
   // or not yet valid (e.g. class has not been loaded yet).
   bool cacheRecord(const AOTSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset);

   // Returns true if ROMClass hash matches the one in the serialization record
   bool isClassMatching(const ClassSerializationRecord *record, J9Class *ramClass, TR::Compilation *comp);

   bool cacheRecord(const ClassLoaderSerializationRecord *record, bool &isNew, bool &wasReset);
   bool cacheRecord(const ClassSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset);
   bool cacheRecord(const MethodSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset);
   bool cacheRecord(const ClassChainSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset);
   bool cacheRecord(const WellKnownClassesSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset);

   // Returns the class loader for given class loader ID, either cached or
   // looked up using the cached SCC offset if the class loader was unloaded.
   // The SCC offset of the identifying class chain is returned in loaderSCCOffset.
   J9ClassLoader *getClassLoader(uintptr_t id, uintptr_t &loaderSCCOffset, bool &wasReset);
   // Returns the RAMClass for given class ID, either cached or
   // looked up using the cached SCC offsets if the class was unloaded.
   J9Class *getRAMClass(uintptr_t id, TR::Compilation *comp, bool &wasReset);

   // Returns -1 on failure
   uintptr_t getSCCOffset(AOTSerializationRecordType type, uintptr_t id, bool &wasReset);

   bool deserializationFailure(const SerializedAOTMethod *method, TR::Compilation *comp, bool wasReset);
   // Returns false on failure
   bool updateSCCOffsets(SerializedAOTMethod *method, TR::Compilation *comp, bool &wasReset);

   TR_PersistentClassLoaderTable *const _loaderTable;
   TR_J9SharedCache *_sharedCache;

   //NOTE: Locking hierarchy used in this class follows cycle-free dependency order
   // between serialization record types and guarantees that there are no deadlocks:
   // - _resetMonitor < _wellKnownClassesMonitor < _classChainMonitor < _classMonitor;
   // - _methodMonitor < _classMonitor;
   // - remaining monitors are "leafs".

   PersistentUnorderedMap<uintptr_t/*ID*/, ClassLoaderEntry> _classLoaderIdMap;
   // This map is needed for invalidating unloaded class loaders
   PersistentUnorderedMap<J9ClassLoader *, uintptr_t/*ID*/> _classLoaderPtrMap;
   TR::Monitor *const _classLoaderMonitor;

   PersistentUnorderedMap<uintptr_t/*ID*/, ClassEntry> _classIdMap;
   // This map is needed for invalidating unloaded classes
   PersistentUnorderedMap<J9Class *, uintptr_t/*ID*/> _classPtrMap;
   TR::Monitor *const _classMonitor;

   PersistentUnorderedMap<uintptr_t/*ID*/, uintptr_t/*SCC offset*/> _methodMap;
   TR::Monitor *const _methodMonitor;

   PersistentUnorderedMap<uintptr_t/*ID*/, uintptr_t/*SCC offset*/> _classChainMap;
   TR::Monitor *const _classChainMonitor;

   PersistentUnorderedMap<uintptr_t/*ID*/, uintptr_t/*SCC offset*/> _wellKnownClassesMap;
   TR::Monitor *const _wellKnownClassesMonitor;

   PersistentUnorderedSet<uintptr_t/*idAndType*/> _newKnownIds;
   TR::Monitor *const _newKnownIdsMonitor;

   volatile bool _resetInProgress;
   TR::Monitor *const _resetMonitor;
   };


#endif /* JITSERVER_AOT_DESERIALIZER_H */
