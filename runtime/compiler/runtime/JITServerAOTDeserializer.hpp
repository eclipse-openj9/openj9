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

#ifndef JITSERVER_AOT_DESERIALIZER_H
#define JITSERVER_AOT_DESERIALIZER_H

#include "env/TRMemory.hpp"
#include "env/PersistentCollections.hpp"
#include "runtime/JITServerAOTSerializationRecords.hpp"

class TR_PersistentClassLoaderTable;
namespace TR { class Compilation; }
namespace TR { class Monitor; }

// This class defines the base interface for the deserialization of cached AOT methods received from a JITServer,
// and initializes certain elements common to the implementations. Its derived classes contain the actual
// deserialization implementation, and at most one of those implementations should be initialized at any one time.
//
// The following is a description of what the implementations all do:
//
// Deserialization involves looking up classes, methods etc. by name, and computing hashes of packed ROMClasses
// (which can be a relatively heavy operation). To improve performance, the deserializer caches results of
// lookups and ROMClass hash validations (including mismatches) for each AOT serialization record.
//
// The IDs of newly cached records are communicated back to the server (with subsequent compilation requests)
// so that it doesn't keep sending records used by multiple methods (e.g. class and class chain records for
// all well-known classes are referred to by any AOT method compiled with SVM).
//
// A JIT client can reconnect to a different JITServer instance after the previous instance fails or shuts down.
// Since AOT cache record IDs are specific to a JITServer instance, the deserializer cache must be purged
// upon connecting to a new instance. This is done in the reset() function, which will be called by some compilation thread
// in handleServerMessage(). To ensure that compilations on other threads are notified of a reset, the reset() function sets
// a flag indicating a reset occurred in every compilation thread. Every lookup or store operation involving the deserializer's
// caches will first acquire the appropriate monitor and check whether or not the current thread was notified of a reset.
// If it was, the operations will set a bool &wasReset flag to notify the caller of that fact, so it can abort whatever it was doing.
// The only exceptions are the invalidate* methods, which are called only when the current thread has exclusive VM access,
// and so no compilation thread could possibly be attempting to reset the deserializer at the same time they are called.
//
// Certain functions in this class and in its subclasses take a bool &wasReset parameter. This is set to true when a concurrent reset
// was detected before accessing the deserializer's cached data.
class JITServerAOTDeserializer
   {
public:
   TR_PERSISTENT_ALLOC(TR_Memory::JITServerAOTCache)

   JITServerAOTDeserializer(TR_PersistentClassLoaderTable *loaderTable);
   ~JITServerAOTDeserializer();

   // Deserializes in place a serialized AOT method received from JITServer. Returns true on success.
   // Caches new serialization records and adds their IDs to the set of new known IDs.
   bool deserialize(SerializedAOTMethod *method, const std::vector<std::string> &records,
                    TR::Compilation *comp, bool &usesSVM);

   // Invalidation functions called from class and class loader unload JIT hooks to invalidate RAMClass
   // and class loader pointers cached by the deserializer.
   virtual void invalidateClassLoader(J9VMThread *vmThread, J9ClassLoader *loader) = 0;
   virtual void invalidateClass(J9VMThread *vmThread, J9Class *ramClass) = 0;

   // Invalidates all cached serialization records. Must be called when the client
   // connects to a new server instance (e.g. upon receving a VM_getVMInfo message),
   // before attempting to deserialize any method received from the new server instance
   // or otherwise use the cached data in the deserializer
   void reset(TR::CompilationInfoPerThread *compInfoPT);

   // IDs of records newly cached during deserialization of an AOT method are sent to the JITServer with
   // the next compilation request, so that the server can update its set of known IDs for this client.
   // This function returns the list of IDs cached since the last call, and clears the set of new known IDs.
   std::vector<uintptr_t/*idAndType*/> getNewKnownIds(TR::Compilation *comp);

   void incNumCacheBypasses() { ++_numCacheBypasses; }
   void incNumCacheMisses() { ++_numCacheMisses; }
   size_t getNumDeserializedMethods() const { return _numDeserializedMethods; }

   void printStats(FILE *f) const;

protected:
   bool deserializerWasReset(TR::Compilation *comp, bool &wasReset);
   bool deserializationFailure(const SerializedAOTMethod *method, TR::Compilation *comp, bool wasReset);

   // Returns true if ROMClass hash matches the one in the serialization record
   bool isClassMatching(const ClassSerializationRecord *record, J9Class *ramClass, TR::Compilation *comp);

   template<typename V> V
   findInMap(const PersistentUnorderedMap<uintptr_t, V> &map, uintptr_t id, TR::Monitor *monitor, TR::Compilation *comp, bool &wasReset);

   TR_PersistentClassLoaderTable *const getClassLoaderTable() const { return _loaderTable; }
   TR::Monitor *const getClassLoaderMonitor() const { return _classLoaderMonitor; }
   TR::Monitor *const getClassMonitor() const { return _classMonitor; }
   TR::Monitor *const getMethodMonitor() const { return _methodMonitor; }
   TR::Monitor *const getClassChainMonitor() const { return _classChainMonitor; }
   TR::Monitor *const getWellKnownClassesMonitor() const { return _wellKnownClassesMonitor; }
   TR::Monitor *const getNewKnownIdsMonitor() const { return _newKnownIdsMonitor; }
   TR::Monitor *const getResetMonitor() const { return _resetMonitor; }

   PersistentUnorderedSet<uintptr_t/*idAndType*/> &getNewKnownIds() { return _newKnownIds; }
   TR_PersistentClassLoaderTable *getLoaderTable() const { return _loaderTable; }

private:
   // Clear the internal caches of the deserializer. Must be called with every monitor in hand
   virtual void clearCachedData() = 0;

   // Deserializes/validates and caches an AOT serialization record.
   // Returns true if the record is valid (e.g. class was found and its hash matches),
   // and sets isNew to true if the record was newly cached (not already known).
   // Returns false if the record is invalid (e.g. ROMClass hash doesn't match)
   // or not yet valid (e.g. class has not been loaded yet).
   bool cacheRecord(const AOTSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset);

   // Cache a ClassLoaderSerializationRecord by looking up a class loader in the current JVM with a first-loaded class name
   // that matches what was recorded during compilation. This will be used to find candidate J9Classes when deserializing other
   // records, and isn't guranteed to have any particular relationship with the actual compile-time class loader.
   virtual bool cacheRecord(const ClassLoaderSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset) = 0;
   // Cache a ClassSerializationRecord by looking up a J9Class in the current JVM, using its associated ClassLoaderRecord, that
   // has a name and ROM class hash that matches what was recorded at compile time. These are used to construct RAM class chains
   // for class chain serialization records. The J9Classes found may not have any particular relationship with the ones recorded
   // at compile time.
   virtual bool cacheRecord(const ClassSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset) = 0;
   // Cache a MethodSerializationRecord by looking up its defining J9Class using its (already-cached) defining
   // ClassSerializationRecord. No extra guarantees are provided beyond what the associated ClassSerializationRecord provides.
   virtual bool cacheRecord(const MethodSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset) = 0;
   // Cache a ClassChainSerializationRecord by constructing a RAM class chain using its stored ClassSerializationRecord IDs.
   // We then construct the actual RAM class chain of the first class in the ClassChainSerializationRecord, and make sure that
   // the constructed and actual RAM class chains match. This ensures that the first class in the chain matches what was recorded
   // at compile time, giving the same guarantees as J9SharedCache::classMatchesCachedVersion().
   virtual bool cacheRecord(const ClassChainSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset) = 0;
   // Cache a WellKnownClassesSerializationRecord. No extra guarantees are provided beyond what the associated
   // ClassChainSerializationRecords of the individual well-known classes chains provide.
   virtual bool cacheRecord(const WellKnownClassesSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset) = 0;
   // Deserialize a ThunkSerializationRecord by installing it in the JVM if one cannot be found through the compilation frontend.
   // No special validation or caching needs to be performed.
   virtual bool cacheRecord(const ThunkSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset) = 0;

   // Returns false on failure
   virtual bool updateSCCOffsets(SerializedAOTMethod *method, TR::Compilation *comp, bool &wasReset, bool &usesSVM) = 0;

   TR_PersistentClassLoaderTable *const _loaderTable;

   // NOTE: Locking hierarchy used in this class and its derivatives follows cycle-free dependency order
   // between serialization record types and guarantees that there are no deadlocks:
   // - _resetMonitor < _wellKnownClassesMonitor < _classChainMonitor < _classMonitor;
   // - _methodMonitor < _classMonitor;
   // - remaining monitors are "leafs".

   TR::Monitor *const _classLoaderMonitor;
   TR::Monitor *const _classMonitor;
   TR::Monitor *const _methodMonitor;
   TR::Monitor *const _classChainMonitor;
   TR::Monitor *const _wellKnownClassesMonitor;
   TR::Monitor *const _newKnownIdsMonitor;
   TR::Monitor *const _resetMonitor;

   PersistentUnorderedSet<uintptr_t/*idAndType*/> _newKnownIds;

   // Statistics
   size_t _numCacheBypasses;
   size_t _numCacheHits;
   size_t _numCacheMisses;
   size_t _numDeserializedMethods;
   size_t _numDeserializationFailures;
   size_t _numClassSizeMismatches;
   size_t _numClassHashMismatches;
   };

// This deserializer implements the following scheme:
//
// 1. AOT cache serialization records are resolved into their corresponding "RAM" entities.
// 2. The persistent representation of these RAM entities is found in the local SCC
// 3. The RAM entities and their local SCC offsets are cached, to avoid deserializing the
//    same record multiple times and to allow for the re-caching of classes and class
//    loaders if they were invalidated and subsequently reloaded.
// 4. The offsets in the cached AOT method are updated with the local SCC offsets.
//
// Methods deserialized with this deserializer must be relocated with a TR_J9SharedCache
// in the frontend (i.e., with the shared cache not overridden in the frontend).
class JITServerLocalSCCAOTDeserializer : public JITServerAOTDeserializer
   {
public:
   TR_PERSISTENT_ALLOC(TR_Memory::JITServerAOTCache)

   JITServerLocalSCCAOTDeserializer(TR_PersistentClassLoaderTable *loaderTable);

   virtual void invalidateClassLoader(J9VMThread *vmThread, J9ClassLoader *loader) override;
   virtual void invalidateClass(J9VMThread *vmThread, J9Class *ramClass) override;

private:
   virtual void clearCachedData() override;

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

   virtual bool cacheRecord(const ClassLoaderSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset) override;
   virtual bool cacheRecord(const ClassSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset) override;
   virtual bool cacheRecord(const MethodSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset) override;
   virtual bool cacheRecord(const ClassChainSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset) override;
   virtual bool cacheRecord(const WellKnownClassesSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset) override;
   virtual bool cacheRecord(const ThunkSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset) override;

   // Returns the class loader for given class loader ID, either cached or
   // looked up using the cached SCC offset if the class loader was unloaded.
   // The SCC offset of the identifying class chain is returned in loaderSCCOffset.
   J9ClassLoader *getClassLoader(uintptr_t id, uintptr_t &loaderSCCOffset, TR::Compilation *comp, bool &wasReset);
   // Returns the RAMClass for given class ID, either cached or
   // looked up using the cached SCC offsets if the class was unloaded.
   J9Class *getRAMClass(uintptr_t id, TR::Compilation *comp, bool &wasReset);

   virtual bool updateSCCOffsets(SerializedAOTMethod *method, TR::Compilation *comp, bool &wasReset, bool &usesSVM) override;

   // Returns -1 on failure
   uintptr_t getSCCOffset(AOTSerializationRecordType type, uintptr_t id, TR::Compilation *comp, bool &wasReset);

   TR_J9SharedCache *const _sharedCache;

   PersistentUnorderedMap<uintptr_t/*ID*/, ClassLoaderEntry> _classLoaderIdMap;
   // This map is needed for invalidating unloaded class loaders
   PersistentUnorderedMap<J9ClassLoader *, uintptr_t/*ID*/> _classLoaderPtrMap;

   PersistentUnorderedMap<uintptr_t/*ID*/, ClassEntry> _classIdMap;
   // This map is needed for invalidating unloaded classes
   PersistentUnorderedMap<J9Class *, uintptr_t/*ID*/> _classPtrMap;

   PersistentUnorderedMap<uintptr_t/*ID*/, uintptr_t/*SCC offset*/> _methodMap;

   PersistentUnorderedMap<uintptr_t/*ID*/, uintptr_t/*SCC offset*/> _classChainMap;

   PersistentUnorderedMap<uintptr_t/*ID*/, uintptr_t/*SCC offset*/> _wellKnownClassesMap;
   };

// This deserializer implements the following scheme:
//
// 1. AOT cache serialization records are resolved into their corresponding "RAM" entities.
// 2. The RAM entities are cached, to avoid deserializing the same record multiple times
// 3. The offsets in the cached AOT method are updated with the idAndType of the corresponding
//    serialization record, to act as "AOT cache offsets" in lieu of local SCC offsets.
class JITServerNoSCCAOTDeserializer : public JITServerAOTDeserializer
   {
public:
   TR_PERSISTENT_ALLOC(TR_Memory::JITServerAOTCache)

   JITServerNoSCCAOTDeserializer(TR_PersistentClassLoaderTable *loaderTable);

   virtual void invalidateClassLoader(J9VMThread *vmThread, J9ClassLoader *loader) override;
   virtual void invalidateClass(J9VMThread *vmThread, J9Class *ramClass) override;
   void invalidateMethod(J9Method *method);

   static uintptr_t offsetId(uintptr_t offset)
      { return AOTSerializationRecord::getId(offset); }
   static AOTSerializationRecordType offsetType(uintptr_t offset)
      { return AOTSerializationRecord::getType(offset); }

private:
   struct ClassEntry
      {
      // NULL if class ID is invalid (was not found or its hash didn't match), or class was unloaded
      J9Class *_ramClass;
      uintptr_t _classLoaderId;
      };

   virtual void clearCachedData() override;

   virtual bool cacheRecord(const ClassLoaderSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset) override;
   virtual bool cacheRecord(const ClassSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset) override;
   virtual bool cacheRecord(const MethodSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset) override;
   virtual bool cacheRecord(const ClassChainSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset) override;
   virtual bool cacheRecord(const WellKnownClassesSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset) override;
   virtual bool cacheRecord(const ThunkSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset) override;

   virtual bool updateSCCOffsets(SerializedAOTMethod *method, TR::Compilation *comp, bool &wasReset, bool &usesSVM) override;

   bool revalidateClassChain(uintptr_t *classChain, TR::Compilation *comp, bool &wasReset);
   void getRAMClassChain(TR::Compilation *comp, J9Class *clazz, J9Class **chainBuffer, size_t &chainLength);

   bool revalidateWellKnownClasses(uintptr_t *wellKnownClassesChain, TR::Compilation *comp, bool &wasReset);

   static uintptr_t encodeOffset(const AOTSerializationRecord *record)
      { return AOTSerializationRecord::idAndType(record->id(), record->type()); }

   static uintptr_t encodeOffset(const SerializedSCCOffset &serializedOffset)
      { return AOTSerializationRecord::idAndType(serializedOffset.recordId(), serializedOffset.recordType()); }

   static uintptr_t encodeClassOffset(uintptr_t id)
      { return AOTSerializationRecord::idAndType(id, AOTSerializationRecordType::Class); }

   static uintptr_t encodeClassChainOffset(uintptr_t id)
      { return AOTSerializationRecord::idAndType(id, AOTSerializationRecordType::ClassChain); }

   PersistentUnorderedMap<uintptr_t/*ID*/, J9ClassLoader *> _classLoaderIdMap;
   PersistentUnorderedMap<J9ClassLoader *, uintptr_t/*ID*/> _classLoaderPtrMap;

   PersistentUnorderedMap<uintptr_t/*ID*/, ClassEntry> _classIdMap;
   PersistentUnorderedMap<J9Class *, uintptr_t/*ID*/> _classPtrMap;

   PersistentUnorderedMap<uintptr_t/*ID*/, J9Method *> _methodIdMap;
   PersistentUnorderedMap<J9Method *, uintptr_t> _methodPtrMap;

   PersistentUnorderedMap<uintptr_t/*ID*/, uintptr_t * /*deserializer chain*/> _classChainMap;

   PersistentUnorderedMap<uintptr_t/*ID*/, uintptr_t * /*deserializer chain offsets*/> _wellKnownClassesMap;
   };

#endif /* JITSERVER_AOT_DESERIALIZER_H */
