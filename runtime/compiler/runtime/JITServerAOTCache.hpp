/*******************************************************************************
 * Copyright (c) 2021, 2023 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef JITSERVER_AOTCACHE_H
#define JITSERVER_AOTCACHE_H

#include <functional>

#include "env/TRMemory.hpp"
#include "env/PersistentCollections.hpp"
#include "runtime/JITServerAOTSerializationRecords.hpp"

static const uint32_t JITSERVER_AOTCACHE_VERSION = 1;
static const char JITSERVER_AOTCACHE_EYECATCHER[] = "AOTCACHE";
// the eye-catcher is not null-terminated in the snapshot files
static const size_t JITSERVER_AOTCACHE_EYECATCHER_LENGTH = sizeof(JITSERVER_AOTCACHE_EYECATCHER) - 1;

namespace TR { class Monitor; }
namespace J9 { class J9SegmentProvider;}

// Information relevant to the compatibility of a cache snapshot with the server.
struct JITServerAOTCacheVersion
   {
   char _eyeCatcher[JITSERVER_AOTCACHE_EYECATCHER_LENGTH];
   uint32_t _snapshotVersion;
   uint32_t _padding; // explicit padding, currently unused
   uint64_t _jitserverVersion;
   };

// The header information for an AOT cache snapshot.
struct JITServerAOTCacheHeader
   {
   JITServerAOTCacheVersion _version;
   uint64_t _serverUID;
   size_t _numClassLoaderRecords;
   size_t _numClassRecords;
   size_t _numMethodRecords;
   size_t _numClassChainRecords;
   size_t _numWellKnownClassesRecords;
   size_t _numAOTHeaderRecords;
   size_t _numThunkRecords;
   size_t _numCachedAOTMethods;
   size_t _nextClassLoaderId;
   size_t _nextClassId;
   size_t _nextMethodId;
   size_t _nextClassChainId;
   size_t _nextWellKnownClassesId;
   size_t _nextAOTHeaderId;
   size_t _nextThunkId;
   };

struct AOTCacheClassLoaderRecord;
struct AOTCacheClassRecord;
struct AOTCacheMethodRecord;
struct AOTCacheClassChainRecord;
struct AOTCacheWellKnownClassesRecord;
struct AOTCacheAOTHeaderRecord;

#define LOAD_AOTCACHE_REQUEST (JITServer::ServerStream *)0x1
#define SAVE_AOTCACHE_REQUEST (JITServer::ServerStream *)0x3 // pointers cannot have the last bit set

// Base class for serialization record "wrappers" stored at the server.
//
// When a cached serialized method is sent to a client, the server needs
// to gather all the serialization records that the method refers to.
// Serialization records refer to their sub-records (e.g. class chain
// records to class records) by IDs. Mapping IDs to records would require
// a data structure (e.g. vector or unordered map) synchronized with a lock.
// To avoid locking while gathering serialization records for a method, we
// store direct pointers to sub-records in the record wrappers at the server.
//
// This class is also used to allow virtual methods which are prohibited
// in AOTSerializationRecord structs since they are sent over the network.
//
// Each AOTCacheRecord is a single contiguous object (variable-sized for most
// record types) in order to simplify memory management and exception handling.
// The subclasses store their underlying serialization record data inline
// (as well as variable-length arrays of subrecord pointers in some cases).
//
// Each AOTCacheRecord also stores a _nextRecord pointer that points to the next record in
// a traversal of all of the records of a particular subclass (used for cache persistence).
class AOTCacheRecord
   {
public:
   // Disable copying since instances can be variable-sized
   AOTCacheRecord(const AOTCacheRecord &) = delete;
   void operator=(const AOTCacheRecord &) = delete;

   // Returns the address of the underlying serialization record
   virtual const AOTSerializationRecord *dataAddr() const = 0;
   // Calls f(r) for each sub-record r
   virtual void subRecordsDo(const std::function<void(const AOTCacheRecord *)> &f) const { }

   static void *allocate(size_t size);
   static void free(void *ptr);

   template<class R> static R *readRecord(FILE *f, const JITServerAOTCacheReadContext &context);

   AOTCacheRecord *getNextRecord() const { return _nextRecord; }
   void setNextRecord(AOTCacheRecord *record) { _nextRecord = record; }

protected:
   AOTCacheRecord() : _nextRecord(NULL) {}

private:
   // Set the subrecord pointers in the variable-length portion of a record if necessary.
   // The subrecord pointers in the statically-sized portion of a record are set in the record constructors.
   virtual bool setSubrecordPointers(const JITServerAOTCacheReadContext &context) { return true; }

   AOTCacheRecord *_nextRecord;
   };


class AOTCacheClassLoaderRecord final : public AOTCacheRecord
   {
public:
   const ClassLoaderSerializationRecord &data() const { return _data; }
   const AOTSerializationRecord *dataAddr() const override { return &_data; }

   static const char *getRecordName() { return "class loader"; }
   static AOTCacheClassLoaderRecord *create(uintptr_t id, const uint8_t *name, size_t nameLength);

private:
   using SerializationRecord = ClassLoaderSerializationRecord;

   friend AOTCacheClassLoaderRecord *AOTCacheRecord::readRecord<>(FILE *f, const JITServerAOTCacheReadContext &context);

   AOTCacheClassLoaderRecord(uintptr_t id, const uint8_t *name, size_t nameLength);
   AOTCacheClassLoaderRecord(const JITServerAOTCacheReadContext &context, const ClassLoaderSerializationRecord &header) {}

   static size_t size(size_t nameLength)
      {
      return offsetof(AOTCacheClassLoaderRecord, _data) + ClassLoaderSerializationRecord::size(nameLength);
      }

   static size_t size(const ClassLoaderSerializationRecord &header) { return size(header.nameLength()); }

   const ClassLoaderSerializationRecord _data;
   };


class AOTCacheClassRecord final : public AOTCacheRecord
   {
public:
   const AOTCacheClassLoaderRecord *classLoaderRecord() const { return _classLoaderRecord; }
   const ClassSerializationRecord &data() const { return _data; }
   const AOTSerializationRecord *dataAddr() const override { return &_data; }

   static const char *getRecordName() { return "class"; }
   static AOTCacheClassRecord *create(uintptr_t id, const AOTCacheClassLoaderRecord *classLoaderRecord,
                                      const JITServerROMClassHash &hash, const J9ROMClass *romClass);
   void subRecordsDo(const std::function<void(const AOTCacheRecord *)> &f) const override;

private:
   using SerializationRecord = ClassSerializationRecord;

   friend AOTCacheClassRecord *AOTCacheRecord::readRecord<>(FILE *f, const JITServerAOTCacheReadContext &context);

   AOTCacheClassRecord(uintptr_t id, const AOTCacheClassLoaderRecord *classLoaderRecord,
                       const JITServerROMClassHash &hash, const J9ROMClass *romClass);
   AOTCacheClassRecord(const JITServerAOTCacheReadContext &context, const ClassSerializationRecord &header);

   static size_t size(size_t nameLength)
      {
      return offsetof(AOTCacheClassRecord, _data) + ClassSerializationRecord::size(nameLength);
      }

   static size_t size(const ClassSerializationRecord &header) { return size(header.nameLength()); }

   const AOTCacheClassLoaderRecord *const _classLoaderRecord;
   const ClassSerializationRecord _data;
   };


class AOTCacheMethodRecord final : public AOTCacheRecord
   {
public:
   const AOTCacheClassRecord *definingClassRecord() const { return _definingClassRecord; }
   const MethodSerializationRecord &data() const { return _data; }
   const AOTSerializationRecord *dataAddr() const override { return &_data; }

   static const char *getRecordName() { return "method"; }
   static AOTCacheMethodRecord *create(uintptr_t id, const AOTCacheClassRecord *definingClassRecord, uint32_t index);
   void subRecordsDo(const std::function<void(const AOTCacheRecord *)> &f) const override;

private:
   using SerializationRecord = MethodSerializationRecord;

   friend AOTCacheMethodRecord *AOTCacheRecord::readRecord<>(FILE *f, const JITServerAOTCacheReadContext &context);

   AOTCacheMethodRecord(uintptr_t id, const AOTCacheClassRecord *definingClassRecord, uint32_t index);
   AOTCacheMethodRecord(const JITServerAOTCacheReadContext &context, const MethodSerializationRecord &header);

   static size_t size(const MethodSerializationRecord &header) { return sizeof(AOTCacheMethodRecord); }

   const AOTCacheClassRecord *const _definingClassRecord;
   const MethodSerializationRecord _data;
   };


// Helper template class to avoid duplicating code for class chain records and well-known classes records
// D is one of: ClassChainSerializationRecord, WellKnownClassesSerializationRecord
// R is one of: AOTCacheClassRecord, AOTCacheClassChainRecord
template<class D, class R, typename... Args>
class AOTCacheListRecord : public AOTCacheRecord
   {
public:
   const D &data() const { return _data; }
   const AOTSerializationRecord *dataAddr() const override { return &_data; }
   // Array of record pointers is stored inline after the array of IDs that is stored inline after struct D header
   const R *const *records() const { return (const R *const *)_data.end(); }
   R **records() { return (R **)_data.end(); }

   void subRecordsDo(const std::function<void(const AOTCacheRecord *)> &f) const override;

protected:
   AOTCacheListRecord(uintptr_t id, const R *const *records, size_t length, Args... args);
   AOTCacheListRecord(const JITServerAOTCacheReadContext &context, const D &header) {}

   static size_t size(size_t length)
      {
      return offsetof(AOTCacheListRecord, _data) + D::size(length) + length * sizeof(R *);
      }

   static size_t size(const D &header) { return size(header.list().length()); }

   bool setSubrecordPointers(const Vector<R *> &cacheRecords, const char *recordName, const char *subrecordName);

   // Layout: struct D header, uintptr_t ids[length], const R *records[length]
   D _data;
   };


class AOTCacheClassChainRecord final : public AOTCacheListRecord<ClassChainSerializationRecord, AOTCacheClassRecord>
   {
public:
   static const char *getRecordName() { return "class chain"; }
   static AOTCacheClassChainRecord *create(uintptr_t id, const AOTCacheClassRecord *const *records, size_t length);

   const AOTCacheClassRecord *rootClassRecord() const { return records()[0]; }
   const AOTCacheClassLoaderRecord *rootClassLoaderRecord() const { return rootClassRecord()->classLoaderRecord(); }

private:
   using SerializationRecord = ClassChainSerializationRecord;

   friend AOTCacheClassChainRecord *AOTCacheRecord::readRecord<>(FILE *f, const JITServerAOTCacheReadContext &context);

   bool setSubrecordPointers(const JITServerAOTCacheReadContext &context) override;

   using AOTCacheListRecord::AOTCacheListRecord;
   };


class AOTCacheWellKnownClassesRecord final :
   public AOTCacheListRecord<WellKnownClassesSerializationRecord, AOTCacheClassChainRecord, uintptr_t>
   {
public:
   static const char *getRecordName() { return "well-known classes"; }
   static AOTCacheWellKnownClassesRecord *create(uintptr_t id, const AOTCacheClassChainRecord *const *records,
                                                 size_t length, uintptr_t includedClasses);

private:
   using SerializationRecord = WellKnownClassesSerializationRecord;

   friend AOTCacheWellKnownClassesRecord *AOTCacheRecord::readRecord<>(FILE *f, const JITServerAOTCacheReadContext &context);

   bool setSubrecordPointers(const JITServerAOTCacheReadContext &context) override;

   using AOTCacheListRecord::AOTCacheListRecord;
   };


class AOTCacheAOTHeaderRecord final : public AOTCacheRecord
   {
public:
   const AOTHeaderSerializationRecord &data() const { return _data; }
   const AOTSerializationRecord *dataAddr() const override { return &_data; }

   static const char *getRecordName() { return "AOT header"; }
   static AOTCacheAOTHeaderRecord *create(uintptr_t id, const TR_AOTHeader *header);

private:
   using SerializationRecord = AOTHeaderSerializationRecord;

   friend AOTCacheAOTHeaderRecord *AOTCacheRecord::readRecord<>(FILE *f, const JITServerAOTCacheReadContext &context);

   AOTCacheAOTHeaderRecord(uintptr_t id, const TR_AOTHeader *header);
   AOTCacheAOTHeaderRecord(const JITServerAOTCacheReadContext &context, const AOTHeaderSerializationRecord &header) {}

   static size_t size(const AOTHeaderSerializationRecord &header) { return sizeof(AOTHeaderSerializationRecord); }

   const AOTHeaderSerializationRecord _data;
   };


class AOTCacheThunkRecord final : public AOTCacheRecord
{
public:
   const ThunkSerializationRecord &data() const { return _data; }
   const AOTSerializationRecord *dataAddr() const override { return &_data; }

   static const char *getRecordName() { return "thunk"; }
   static AOTCacheThunkRecord *create(uintptr_t id, const uint8_t *signature, uint32_t signatureSize, const uint8_t *thunkStart, uint32_t thunkSize);

private:
   using SerializationRecord = ThunkSerializationRecord;

   friend AOTCacheThunkRecord *AOTCacheRecord::readRecord<>(FILE *f, const JITServerAOTCacheReadContext &context);

   AOTCacheThunkRecord(uintptr_t id, const uint8_t *signature, uint32_t signatureSize, const uint8_t *thunkStart, uint32_t thunkSize);
   AOTCacheThunkRecord(const JITServerAOTCacheReadContext &context, const ThunkSerializationRecord &header) {}

   static size_t size(uint32_t signatureSize, uint32_t thunkSize)
      {
      return offsetof(AOTCacheThunkRecord, _data) + ThunkSerializationRecord::size(signatureSize, thunkSize);
      }

   static size_t size(const ThunkSerializationRecord &header) { return size(header.signatureSize(), header.thunkSize()); }

   const ThunkSerializationRecord _data;
};


// Wrapper class for serialized AOT methods stored in the cache at the server.
// Serves the same purpose as AOTCacheRecord (serialization record wrappers).
class CachedAOTMethod
   {
public:
   // Disable copying since instances are variable-sized
   CachedAOTMethod(const CachedAOTMethod &) = delete;
   void operator=(const CachedAOTMethod &) = delete;

   const AOTCacheClassChainRecord *definingClassChainRecord() const { return _definingClassChainRecord; }
   const AOTCacheClassRecord *definingClassRecord() const { return _definingClassChainRecord->records()[0]; }
   const SerializedAOTMethod &data() const { return _data; }
   SerializedAOTMethod &data() { return _data; }
   const AOTCacheRecord *const *records() const { return (const AOTCacheRecord *const *)_data.end(); }
   AOTCacheRecord **records() { return (AOTCacheRecord **)_data.end(); }

   static const char *getRecordName() { return "cached AOT method"; }
   static CachedAOTMethod *create(const AOTCacheClassChainRecord *definingClassChainRecord, uint32_t index,
                                  TR_Hotness optLevel, const AOTCacheAOTHeaderRecord *aotHeaderRecord,
                                  const Vector<std::pair<const AOTCacheRecord *, uintptr_t>> &records,
                                  const void *code, size_t codeSize, const void *data, size_t dataSize);

   CachedAOTMethod *getNextRecord() const { return _nextRecord; }
   void setNextRecord(CachedAOTMethod *record) { _nextRecord = record; }

private:
   using SerializationRecord = SerializedAOTMethod;

   friend CachedAOTMethod *AOTCacheRecord::readRecord<>(FILE *f, const JITServerAOTCacheReadContext &context);

   CachedAOTMethod(const AOTCacheClassChainRecord *definingClassChainRecord, uint32_t index,
                   TR_Hotness optLevel, const AOTCacheAOTHeaderRecord *aotHeaderRecord,
                   const Vector<std::pair<const AOTCacheRecord *, uintptr_t>> &records,
                   const void *code, size_t codeSize, const void *data, size_t dataSize);
   CachedAOTMethod(const JITServerAOTCacheReadContext &context, const SerializedAOTMethod &header);

   SerializedAOTMethod *dataAddr() { return &_data; }

   static size_t size(size_t numRecords, size_t codeSize, size_t dataSize)
      {
      return offsetof(CachedAOTMethod, _data) + SerializedAOTMethod::size(numRecords, codeSize, dataSize) +
             numRecords * sizeof(AOTCacheRecord *);
      }

   static size_t size(const SerializedAOTMethod &header) { return size(header.numRecords(), header.codeSize(), header.dataSize()); }

   bool setSubrecordPointers(const JITServerAOTCacheReadContext &context);

   CachedAOTMethod *_nextRecord;
   const AOTCacheClassChainRecord *const _definingClassChainRecord;
   SerializedAOTMethod _data;
   // Array of record pointers is stored inline after serialized AOT method data
   };


// This class implements the storage of serialized AOT methods and their
// serialization records at the JITServer. It is only used on the server side.
// Each AOT cache instance is identified by a unique name and stores its own
// independent set of serialized AOT methods and their serialization records.
class JITServerAOTCache
   {
public:
   TR_PERSISTENT_ALLOC(TR_Memory::JITServerAOTCache)

   JITServerAOTCache(const std::string &name);
   ~JITServerAOTCache();

   const std::string &name() const { return _name; }

   // Each get{Type}Record() method except getThunkRecord returns the record for given parameters (which fully identify
   // the unique record), by either looking up the existing record or creating a new one if there is sufficient
   // space. The getThunkRecord method instead has an accompanying createAndStoreThunk method that will create and store
   // a new thunk record if there is sufficient space.
   const AOTCacheClassLoaderRecord *getClassLoaderRecord(const uint8_t *name, size_t nameLength);
   const AOTCacheClassRecord *getClassRecord(const AOTCacheClassLoaderRecord *loaderRecord, const J9ROMClass *romClass);
   const AOTCacheMethodRecord *getMethodRecord(const AOTCacheClassRecord *definingClassRecord,
                                               uint32_t index, const J9ROMMethod *romMethod);
   const AOTCacheClassChainRecord *getClassChainRecord(const AOTCacheClassRecord *const *classRecords, size_t length);
   const AOTCacheWellKnownClassesRecord *getWellKnownClassesRecord(const AOTCacheClassChainRecord *const *chainRecords,
                                                                   size_t length, uintptr_t includedClasses);
   const AOTCacheThunkRecord *getThunkRecord(const uint8_t *signature, uint32_t signatureSize);
   const AOTCacheThunkRecord *createAndStoreThunk(const uint8_t *signature, uint32_t signatureSize, const uint8_t *thunkCode, uint32_t thunkCodeSize);
   const AOTCacheAOTHeaderRecord *getAOTHeaderRecord(const TR_AOTHeader *header, uint64_t clientUID);

   // Add a serialized AOT method to the cache. The key identifying the method is a combination of:
   // - class chain record for its defining class;
   // - index in the array of methods in the defining class;
   // - AOT header record for the TR_AOTHeader of the client JVM this method was compiled for;
   // - optimization level.
   // Each item in the `records` vector corresponds to an SCC offset stored in the AOT method's relocation data.
   // Note that the SCC offsets corresponding to AOTCacheThunkRecord records will not be used, as thunks are defined
   // globally in each client and are addressed by signature.
   // Returns true if the method was successfully added, false otherwise (if a method already exists for this key).
   bool storeMethod(const AOTCacheClassChainRecord *definingClassChainRecord, uint32_t index,
                    TR_Hotness optLevel, const AOTCacheAOTHeaderRecord *aotHeaderRecord,
                    const Vector<std::pair<const AOTCacheRecord *, uintptr_t/*reloDataOffset*/>> &records,
                    const void *code, size_t codeSize, const void *data, size_t dataSize,
                    const char *signature, uint64_t clientUID);

   // Lookup a serialized method for the given key (see comment for storeMethod() above)
   // in the cache. Returns NULL if no such method exists in the cache.
   const CachedAOTMethod *findMethod(const AOTCacheClassChainRecord *definingClassChainRecord, uint32_t index,
                                     TR_Hotness optLevel, const AOTCacheAOTHeaderRecord *aotHeaderRecord);

   using KnownIdSet = PersistentUnorderedSet<uintptr_t/*recordIdAndType*/>;

   // Get serialization records the method refers to, excluding the ones already
   // present in the knownIds set (i.e. already deseralized and cached at the client).
   // The result is sorted in "dependency order": for each record in the resulting list,
   // all the records that it depends on are stored in the list at lower indices.
   Vector<const AOTSerializationRecord *>
   getSerializationRecords(const CachedAOTMethod *method, const KnownIdSet &knownIds, TR_Memory &trMemory) const;

   void incNumCacheBypasses() { ++_numCacheBypasses; }
   void incNumCacheMisses() { ++_numCacheMisses; }
   size_t getNumDeserializedMethods() const { return _numDeserializedMethods; }
   void incNumDeserializedMethods() { ++_numDeserializedMethods; }
   void incNumDeserializationFailures() { ++_numDeserializationFailures; }

   void printStats(FILE *f) const;

   size_t writeCache(FILE *f) const;
   static JITServerAOTCache *readCache(FILE *f, const std::string &name, TR_Memory &trMemory);
   size_t getNumCachedMethods() const;
   void setMinNumAOTMethodsToSave(size_t num) { _minNumAOTMethodsToSave = num; }

  /**
   * @brief  If conditions are right, launch another compilation thread to store this AOT cache to a file.
   *
   * This is done if both conditions below are met:
   * (1) "enough" methods were added to the in-memory AOT cache since the last snapshot
   * and
   * (2) "enough" time has passed since the last snapshot
   *
   * Until the save operation is complete, the _saveOperationInProgress flag is set
   * to 'true' to prevent other threads launching other save operations
   * Internally, this method aquires temporarily the _cachedMethodMonitor,
   * the monitor protecting the aotCacheMap and the compilation monitor.
   *
   * @return true if the save operation was launched, false otherwise
   */
   bool triggerAOTCacheStoreToFileIfNeeded();
   void finalizeSaveOperation(bool success, size_t numMethodsSavedToFile);
   void excludeCacheFromSavingToFile() { _excludedFromSavingToFile = true; }

   /**
      @brief Determine if current in-memory AOT cache is "better" than the one on file.

      An in-memory AOT cache is considered "better" than the one on file if it has `numExtraMethods` more AOT methods.
      Artificially, it is also considered better if one of the following conditions is true:
      (1) cache on file is incompatible with the in-memory cache
      (2) file containing the cache cannot be opened or its content cannot be read

      @param cacheFileName The name of the file containing the AOT snapshot to be compared (second term of the comparison)
      @param numExtraMethods How many more methods need to be in the in-memory cache compared to the AOT snapshot in order to be considered "better"
      @return true if the in-memory cache is better than the one on file, false otherwise
   */
   bool isAOTCacheBetterThanSnapshot(const std::string &cacheFileName, size_t numExtraMethods);

private:
   struct StringKey
      {
      bool operator==(const StringKey &k) const;
      struct Hash { size_t operator()(const StringKey &k) const noexcept; };

      const uint8_t *const _string;
      const size_t _stringLength;
      };

   using ClassLoaderKey = StringKey;

   static ClassLoaderKey getRecordKey(const AOTCacheClassLoaderRecord *record)
      { return { record->data().name(), record->data().nameLength() }; }

   struct ClassKey
      {
      bool operator==(const ClassKey &k) const;
      struct Hash { size_t operator()(const ClassKey &k) const noexcept; };

      const AOTCacheClassLoaderRecord *const _classLoaderRecord;
      const JITServerROMClassHash *const _hash;
      };

   static ClassKey getRecordKey(const AOTCacheClassRecord *record)
      { return { record->classLoaderRecord(), &record->data().hash() }; }

   using MethodKey = std::pair<const AOTCacheClassRecord *, uint32_t/*index*/>;

   static MethodKey getRecordKey(const AOTCacheMethodRecord *record)
      { return { record->definingClassRecord(), record->data().index() }; }

   struct ClassChainKey
      {
      bool operator==(const ClassChainKey &k) const;
      struct Hash { size_t operator()(const ClassChainKey &k) const noexcept; };

      const AOTCacheClassRecord *const *const _records;
      const size_t _length;
      };

   static ClassChainKey getRecordKey(const AOTCacheClassChainRecord *record)
      { return { record->records(), record->data().list().length() }; }

   struct WellKnownClassesKey
      {
      bool operator==(const WellKnownClassesKey &k) const;
      struct Hash { size_t operator()(const WellKnownClassesKey &k) const noexcept; };

      const AOTCacheClassChainRecord *const *const _records;
      const size_t _length;
      const uintptr_t _includedClasses;
      };

   static WellKnownClassesKey getRecordKey(const AOTCacheWellKnownClassesRecord *record)
      { return { record->records(), record->data().list().length(), record->data().includedClasses() }; }

   struct AOTHeaderKey
      {
      bool operator==(const AOTHeaderKey &k) const;
      struct Hash { size_t operator()(const AOTHeaderKey &k) const noexcept; };

      const TR_AOTHeader *const _header;
      };

   static AOTHeaderKey getRecordKey(const AOTCacheAOTHeaderRecord *record)
      { return { record->data().header() }; }

   using ThunkKey = StringKey;

   static ThunkKey getRecordKey(const AOTCacheThunkRecord *record)
      { return { record->data().signature(), record->data().signatureSize() }; }

   //NOTE: Current implementation doesn't support compatible differences in AOT headers.
   //      A cached method can only be sent to a client with the exact same AOT header.
   using CachedMethodKey = std::tuple<const AOTCacheClassChainRecord *, uint32_t/*index*/,
                                      TR_Hotness, const AOTCacheAOTHeaderRecord *>;

   // Helper method used in getSerializationRecords()
   void addRecord(const AOTCacheRecord *record, Vector<const AOTSerializationRecord *> &result,
                  UnorderedSet<const AOTCacheRecord *> &newRecords, const KnownIdSet &knownIds) const;
   // Read a cache snapshot into an empty cache
   bool readCache(FILE *f, const JITServerAOTCacheHeader &header, TR_Memory &trMemory);

   template<typename K, typename V, typename H>
   static bool readRecords(FILE *f, JITServerAOTCacheReadContext &context, size_t numRecordsToRead,
                           PersistentUnorderedMap<K, V *, H> &map, V *&traversalHead, V *&traversalTail, Vector<V *> &records);

   const std::string _name;

   // Along with each map we also store pointers to the start and end points of a traversal of all the records.
   // The _nextRecord in each record points to the next record in this traversal.
   PersistentUnorderedMap<ClassLoaderKey, AOTCacheClassLoaderRecord *, ClassLoaderKey::Hash> _classLoaderMap;
   uintptr_t _nextClassLoaderId;
   AOTCacheClassLoaderRecord *_classLoaderHead;
   AOTCacheClassLoaderRecord *_classLoaderTail;
   TR::Monitor *const _classLoaderMonitor;

   PersistentUnorderedMap<ClassKey, AOTCacheClassRecord *, ClassKey::Hash> _classMap;
   uintptr_t _nextClassId;
   AOTCacheClassRecord *_classHead;
   AOTCacheClassRecord *_classTail;
   TR::Monitor *const _classMonitor;

   PersistentUnorderedMap<MethodKey, AOTCacheMethodRecord *> _methodMap;
   uintptr_t _nextMethodId;
   AOTCacheMethodRecord *_methodHead;
   AOTCacheMethodRecord *_methodTail;
   TR::Monitor *const _methodMonitor;

   PersistentUnorderedMap<ClassChainKey, AOTCacheClassChainRecord *, ClassChainKey::Hash> _classChainMap;
   AOTCacheClassChainRecord *_classChainHead;
   AOTCacheClassChainRecord *_classChainTail;
   uintptr_t _nextClassChainId;
   TR::Monitor *const _classChainMonitor;

   PersistentUnorderedMap<WellKnownClassesKey, AOTCacheWellKnownClassesRecord *,
                          WellKnownClassesKey::Hash> _wellKnownClassesMap;
   AOTCacheWellKnownClassesRecord *_wellKnownClassesHead;
   AOTCacheWellKnownClassesRecord *_wellKnownClassesTail;
   uintptr_t _nextWellKnownClassesId;
   TR::Monitor *const _wellKnownClassesMonitor;

   PersistentUnorderedMap<AOTHeaderKey, AOTCacheAOTHeaderRecord *, AOTHeaderKey::Hash> _aotHeaderMap;
   AOTCacheAOTHeaderRecord *_aotHeaderHead;
   AOTCacheAOTHeaderRecord *_aotHeaderTail;
   uintptr_t _nextAOTHeaderId;
   TR::Monitor *const _aotHeaderMonitor;

   PersistentUnorderedMap<ThunkKey, AOTCacheThunkRecord *, ThunkKey::Hash> _thunkMap;
   uintptr_t _nextThunkId;
   AOTCacheThunkRecord *_thunkHead;
   AOTCacheThunkRecord *_thunkTail;
   TR::Monitor *const _thunkMonitor;

   PersistentUnorderedMap<CachedMethodKey, CachedAOTMethod *> _cachedMethodMap;
   CachedAOTMethod *_cachedMethodHead;
   CachedAOTMethod *_cachedMethodTail;
   TR::Monitor *const _cachedMethodMonitor;

   uint64_t _timePrevSaveOperation;   // Millis when this cache was last saved to file
   size_t _minNumAOTMethodsToSave;    // Minimum number of AOT methods present in the cache before considering a save operation
   bool _saveOperationInProgress;     // True if an AOTCache save operation is in progress
   bool _excludedFromSavingToFile;    // True if this cache is excluded from saving to file

   // Statistics
   size_t _numCacheBypasses;
   size_t _numCacheHits;
   size_t _numCacheMisses;
   size_t _numDeserializedMethods;
   size_t _numDeserializationFailures;
   };


// Maps AOT cache names to JITServerAOTCache instances
class JITServerAOTCacheMap
   {
public:
   TR_PERSISTENT_ALLOC(TR_Memory::JITServerAOTCache)

   JITServerAOTCacheMap();
   ~JITServerAOTCacheMap();
   /**
      @brief Enqueue the given AOT cache name to be saved saving to file, later-on.
   */
   void queueAOTCacheForSavingToFile(const std::string &cacheName);

   /**
      @brief Attempt to save to file the next AOT cache that is queued for saving.
   */
   bool saveNextQueuedAOTCacheToFile();

   /**
      @brief Load from file the next AOT cache that is queued for loading.

      If the load operation succeeds, a new named in-memory cache is created.
      If the load operation fails, this cache name is excluded from future load operations.
      Any exceptions thrown by this method are caught and logged.
      This method acquires the AOTCacheMap monitor.
   */
   void loadNextQueuedAOTCacheFromFile(J9::J9SegmentProvider &scratchSegmentProvider);

   /**
      @brief Obtain a pointer to a named AOT cache. If it doesn't exist, attempt to create one.

      If the cache exists, return a pointer to it.
      If the cache doesn't exist and the persistence feature is disabled, create an empty cache;
      if the persistence feature is enabled and this cache name is not on the exclusion list,
      enqueue a special compilation request that will load the cache from file and set "pending" to true

      @param name  Name of the cache to get or create
      @param clientUID UID of the client that will be using this cache
      @param pending Output flag indicating that a cache load operation from file is in progress, but not read yet
      @return Pointer to the named AOT cache, or NULL if the cache could not be created right away (or at all)
   */
   JITServerAOTCache *get(const std::string &name, uint64_t clientUID, bool &pending);
   size_t getNumDeserializedMethods() const;

   static void setCacheMaxBytes(size_t bytes) { _cacheMaxBytes = bytes; }
   static bool cacheHasSpace();

   void printStats(FILE *f) const;

private:
   static std::string buildCacheFileName(const std::string &cacheDir, const std::string &cacheName);

   PersistentUnorderedMap<std::string, JITServerAOTCache *> _map;

   // The following set is used to keep track of the caches that are in process of being loaded from file
   // Once a cache is loaded, it is removed from the set and added to the _map
   PersistentUnorderedSet<std::string> _cachesBeingLoaded;

   // _cachesToLoadQueue is used to order the caches that need to be loaded from file.
   // A compilation thread dequeues the first name and starts to load the cache from file.
   // When it finishes (either successfully or not), it deletes the name from _cachesBeingLoaded.
   PersistentList<std::string> _cachesToLoadQueue;

   // _cachesExcludedFromLoading is used to keep track of the caches that we don't want to load
   // from file, maybe because the load operation already was attempted and failed
   // We could also populate this set from command line options
   PersistentUnorderedSet<std::string> _cachesExcludedFromLoading;

   // _cachesToSaveQueue is used to remember the caches that need to be saved to file
   // A compilation thread dequeues the first name and tries to save the named cache to a file,
   // possibly overwriting an existent file
   PersistentList<std::string> _cachesToSaveQueue;

   TR::Monitor *const _monitor;

   static size_t _cacheMaxBytes;
   static bool _cacheIsFull;
   };

#endif /* defined(JITSERVER_AOTCACHE_H) */
