/*******************************************************************************
 * Copyright (c) 2021, 2022 IBM Corp. and others
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

#include "control/CompilationRuntime.hpp"
#include "env/StackMemoryRegion.hpp"
#include "infra/CriticalSection.hpp"
#include "runtime/JITServerAOTCache.hpp"
#include "runtime/JITServerSharedROMClassCache.hpp"

size_t JITServerAOTCacheMap::_cacheMaxBytes = 300 * 1024 * 1024;
bool JITServerAOTCacheMap::_cacheIsFull = false;


void *
AOTCacheRecord::allocate(size_t size)
   {
   void *ptr = TR::Compiler->persistentGlobalMemory()->allocatePersistentMemory(size, TR_Memory::JITServerAOTCache);
   if (!ptr)
      throw std::bad_alloc();
   return ptr;
   }

void
AOTCacheRecord::free(void *ptr)
   {
   TR::Compiler->persistentGlobalMemory()->freePersistentMemory(ptr);
   }


ClassLoaderSerializationRecord::ClassLoaderSerializationRecord(uintptr_t id, const uint8_t *name, size_t nameLength) :
   AOTSerializationRecord(size(nameLength), id, AOTSerializationRecordType::ClassLoader),
   _nameLength(nameLength)
   {
   memcpy(_name, name, nameLength);
   }

AOTCacheClassLoaderRecord::AOTCacheClassLoaderRecord(uintptr_t id, const uint8_t *name, size_t nameLength) :
   _data(id, name, nameLength)
   {
   }

AOTCacheClassLoaderRecord *
AOTCacheClassLoaderRecord::create(uintptr_t id, const uint8_t *name, size_t nameLength)
   {
   void *ptr = AOTCacheRecord::allocate(size(nameLength));
   return new (ptr) AOTCacheClassLoaderRecord(id, name, nameLength);
   }


ClassSerializationRecord::ClassSerializationRecord(uintptr_t id, uintptr_t classLoaderId,
                                                   const JITServerROMClassHash &hash, const J9ROMClass *romClass) :
   AOTSerializationRecord(size(J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass))), id, AOTSerializationRecordType::Class),
   _classLoaderId(classLoaderId), _hash(hash), _romClassSize(romClass->romSize),
   _nameLength(J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass)))
   {
   memcpy(_name, J9UTF8_DATA(J9ROMCLASS_CLASSNAME(romClass)), _nameLength);
   }

AOTCacheClassRecord::AOTCacheClassRecord(uintptr_t id, const AOTCacheClassLoaderRecord *classLoaderRecord,
                                         const JITServerROMClassHash &hash, const J9ROMClass *romClass) :
   _classLoaderRecord(classLoaderRecord),
   _data(id, classLoaderRecord->data().id(), hash, romClass)
   {
   }

AOTCacheClassRecord *
AOTCacheClassRecord::create(uintptr_t id, const AOTCacheClassLoaderRecord *classLoaderRecord,
                            const JITServerROMClassHash &hash, const J9ROMClass *romClass)
   {
   void *ptr = AOTCacheRecord::allocate(size(J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass))));
   return new (ptr) AOTCacheClassRecord(id, classLoaderRecord, hash, romClass);
   }

void
AOTCacheClassRecord::subRecordsDo(const std::function<void(const AOTCacheRecord *)> &f) const
   {
   f(_classLoaderRecord);
   }


MethodSerializationRecord::MethodSerializationRecord(uintptr_t id, uintptr_t definingClassId, uint32_t index) :
   AOTSerializationRecord(sizeof(*this), id, AOTSerializationRecordType::Method),
   _definingClassId(definingClassId), _index(index)
   {
   }

AOTCacheMethodRecord::AOTCacheMethodRecord(uintptr_t id, const AOTCacheClassRecord *definingClassRecord,
                                           uint32_t index) :
   _definingClassRecord(definingClassRecord),
   _data(id, definingClassRecord->data().id(), index)
   {
   }

AOTCacheMethodRecord *
AOTCacheMethodRecord::create(uintptr_t id, const AOTCacheClassRecord *definingClassRecord, uint32_t index)
   {
   void *ptr = AOTCacheRecord::allocate(sizeof(AOTCacheMethodRecord));
   return new (ptr) AOTCacheMethodRecord(id, definingClassRecord, index);
   }

void
AOTCacheMethodRecord::subRecordsDo(const std::function<void(const AOTCacheRecord *)> &f) const
   {
   f(_definingClassRecord);
   }


template<class D, class R, typename... Args>
AOTCacheListRecord<D, R, Args...>::AOTCacheListRecord(uintptr_t id, const R *const *records,
                                                      size_t length, Args... args) :
   _data(id, length, args...)
   {
   for (size_t i = 0; i < length; ++i)
      _data.list().ids()[i] = records[i]->data().id();
   memcpy((void *)this->records(), records, length * sizeof(R *));
   }

template<class D, class R, typename... Args> void
AOTCacheListRecord<D, R, Args...>::subRecordsDo(const std::function<void(const AOTCacheRecord *)> &f) const
   {
   for (size_t i = 0; i < _data.list().length(); ++i)
      f(records()[i]);
   }


ClassChainSerializationRecord::ClassChainSerializationRecord(uintptr_t id, size_t length) :
   AOTSerializationRecord(size(length), id, AOTSerializationRecordType::ClassChain),
   _list(length)
   {
   }

AOTCacheClassChainRecord *
AOTCacheClassChainRecord::create(uintptr_t id, const AOTCacheClassRecord *const *records, size_t length)
   {
   void *ptr = AOTCacheRecord::allocate(size(length));
   return new (ptr) AOTCacheClassChainRecord(id, records, length);
   }


WellKnownClassesSerializationRecord::WellKnownClassesSerializationRecord(uintptr_t id, size_t length,
                                                                         uintptr_t includedClasses) :
   AOTSerializationRecord(size(length), id, AOTSerializationRecordType::WellKnownClasses),
   _includedClasses(includedClasses), _list(length)
   {
   }

AOTCacheWellKnownClassesRecord *
AOTCacheWellKnownClassesRecord::create(uintptr_t id, const AOTCacheClassChainRecord *const *records,
                                       size_t length, uintptr_t includedClasses)
   {
   void *ptr = AOTCacheRecord::allocate(size(length));
   return new (ptr) AOTCacheWellKnownClassesRecord(id, records, length, includedClasses);
   }


AOTHeaderSerializationRecord::AOTHeaderSerializationRecord(uintptr_t id, const TR_AOTHeader *header) :
   AOTSerializationRecord(sizeof(*this), id, AOTSerializationRecordType::AOTHeader),
   _header(*header)
   {
   }

AOTCacheAOTHeaderRecord::AOTCacheAOTHeaderRecord(uintptr_t id, const TR_AOTHeader *header) :
   _data(id, header)
   {
   }

AOTCacheAOTHeaderRecord *
AOTCacheAOTHeaderRecord::create(uintptr_t id, const TR_AOTHeader *header)
   {
   void *ptr = AOTCacheRecord::allocate(sizeof(AOTCacheAOTHeaderRecord));
   return new (ptr) AOTCacheAOTHeaderRecord(id, header);
   }


SerializedAOTMethod::SerializedAOTMethod(uintptr_t definingClassChainId, uint32_t index,
                                         TR_Hotness optLevel, uintptr_t aotHeaderId, size_t numRecords,
                                         const void *code, size_t codeSize, const void *data, size_t dataSize) :
   _size(size(numRecords, codeSize, dataSize)),
   _definingClassChainId(definingClassChainId), _index(index),
   _optLevel(optLevel), _aotHeaderId(aotHeaderId),
   _numRecords(numRecords), _codeSize(codeSize), _dataSize(dataSize)
   {
   memcpy((void *)this->code(), code, codeSize);
   memcpy((void *)this->data(), data, dataSize);
   }

CachedAOTMethod::CachedAOTMethod(const AOTCacheClassChainRecord *definingClassChainRecord, uint32_t index,
                                 TR_Hotness optLevel, const AOTCacheAOTHeaderRecord *aotHeaderRecord,
                                 const Vector<std::pair<const AOTCacheRecord *, uintptr_t>> &records,
                                 const void *code, size_t codeSize, const void *data, size_t dataSize) :
   _nextRecord(NULL),
   _data(definingClassChainRecord->data().id(), index, optLevel,
         aotHeaderRecord->data().id(), records.size(), code, codeSize, data, dataSize),
   _definingClassChainRecord(definingClassChainRecord)
   {
   for (size_t i = 0; i < records.size(); ++i)
      {
      const AOTSerializationRecord *record = records[i].first->dataAddr();
      new (&_data.offsets()[i]) SerializedSCCOffset(record->id(), record->type(), records[i].second);
      ((const AOTCacheRecord **)this->records())[i] = records[i].first;
      }
   }

CachedAOTMethod *
CachedAOTMethod::create(const AOTCacheClassChainRecord *definingClassChainRecord, uint32_t index,
                        TR_Hotness optLevel, const AOTCacheAOTHeaderRecord *aotHeaderRecord,
                        const Vector<std::pair<const AOTCacheRecord *, uintptr_t>> &records,
                        const void *code, size_t codeSize, const void *data, size_t dataSize)
   {
   void *ptr = AOTCacheRecord::allocate(size(records.size(), codeSize, dataSize));
   return new (ptr) CachedAOTMethod(definingClassChainRecord, index, optLevel, aotHeaderRecord,
                                    records, code, codeSize, data, dataSize);
   }


bool
JITServerAOTCache::ClassLoaderKey::operator==(const ClassLoaderKey &k) const
   {
   return J9UTF8_DATA_EQUALS(_name, _nameLength, k._name, k._nameLength);
   }

size_t
JITServerAOTCache::ClassLoaderKey::Hash::operator()(const ClassLoaderKey &k) const noexcept
   {
   size_t h = 0;
   for (size_t i = 0; i < k._nameLength; ++i)
      h = (h << 5) - h + k._name[i];
   return h;
   }


bool
JITServerAOTCache::ClassKey::operator==(const ClassKey &k) const
   {
   return (_classLoaderRecord == k._classLoaderRecord) && (*_hash == *k._hash);
   }

size_t
JITServerAOTCache::ClassKey::Hash::operator()(const ClassKey &k) const noexcept
   {
   // Remove trailing zero bits in aligned pointer for better hash distribution
   return ((uintptr_t)k._classLoaderRecord >> 3) ^ std::hash<JITServerROMClassHash>()(*k._hash);
   }


static size_t recordListHash(const AOTCacheRecord *const *records, size_t length)
{
   size_t h = length;
   for (size_t i = 0; i < length; ++i)
      h ^= (uintptr_t)records[i] >> 3;// Remove trailing zero bits in aligned pointer for better hash distribution
   return h;
}


bool
JITServerAOTCache::ClassChainKey::operator==(const ClassChainKey &k) const
   {
   return (_length == k._length) && (memcmp(_records, k._records, _length * sizeof(_records[0])) == 0);
   }

size_t
JITServerAOTCache::ClassChainKey::Hash::operator()(const ClassChainKey &k) const noexcept
   {
   return recordListHash((const AOTCacheRecord *const *)k._records, k._length);
   }


bool
JITServerAOTCache::WellKnownClassesKey::operator==(const WellKnownClassesKey &k) const
   {
   return (_length == k._length) && (_includedClasses == k._includedClasses) &&
          (memcmp(_records, k._records, _length * sizeof(_records[0])) == 0);
   }

size_t
JITServerAOTCache::WellKnownClassesKey::Hash::operator()(const WellKnownClassesKey &k) const noexcept
   {
   return recordListHash((const AOTCacheRecord *const *)k._records, k._length) ^ k._includedClasses;
   }


bool
JITServerAOTCache::AOTHeaderKey::operator==(const AOTHeaderKey &k) const
   {
   return memcmp(_header, k._header, sizeof(*_header)) == 0;
   }

size_t
JITServerAOTCache::AOTHeaderKey::Hash::operator()(const AOTHeaderKey &k) const noexcept
   {
   // Treat TR_AOTHeader as an array of size_t words (most of its fields are word-sized)
   size_t h = 0;
   for (size_t i = 0; i < sizeof(*k._header) / sizeof(size_t); ++i)
      h ^= ((const size_t *)k._header)[i];
   return h;
   }


// Insert the value (which must be allocated with AOTCacheRecord::allocate())
// with the key into the map, avoiding memory leaks in case of exceptions.
template<typename K, typename V, typename H> static void
addToMap(PersistentUnorderedMap<K, V *, H> &map,
         const typename PersistentUnorderedMap<K, V *, H>::const_iterator &it,
         const K &key, V *value)
   {
   try
      {
      map.insert(it, { key, value });
      }
   catch (...)
      {
      AOTCacheRecord::free(value);
      throw;
      }
   }

// Free all the values (which must be allocated with AOTCacheRecord::allocate()) in the map.
// NOTE: This function can only be used in the destructor of the object containing the map.
// The now invalid pointers stay in the map, so it must be destroyed after this call.
template<typename K, typename V, typename H> static void
freeMapValues(const PersistentUnorderedMap<K, V *, H> &map)
   {
   for (auto &kv : map)
      AOTCacheRecord::free(kv.second);
   }


JITServerAOTCache::JITServerAOTCache(const std::string &name) :
   _name(name),
   _classLoaderMap(decltype(_classLoaderMap)::allocator_type(TR::Compiler->persistentGlobalAllocator())),
   _classLoaderHead(NULL),
   _classLoaderTail(NULL),
   _nextClassLoaderId(1),// ID 0 is invalid
   _classLoaderMonitor(TR::Monitor::create("JIT-JITServerAOTCacheClassLoaderMonitor")),
   _classMap(decltype(_classMap)::allocator_type(TR::Compiler->persistentGlobalAllocator())),
   _classHead(NULL),
   _classTail(NULL),
   _nextClassId(1),// ID 0 is invalid
   _classMonitor(TR::Monitor::create("JIT-JITServerAOTCacheClassMonitor")),
   _methodMap(decltype(_methodMap)::allocator_type(TR::Compiler->persistentGlobalAllocator())),
   _methodHead(NULL),
   _methodTail(NULL),
   _nextMethodId(1),// ID 0 is invalid
   _methodMonitor(TR::Monitor::create("JIT-JITServerAOTCacheMethodMonitor")),
   _classChainMap(decltype(_classChainMap)::allocator_type(TR::Compiler->persistentGlobalAllocator())),
   _classChainHead(NULL),
   _classChainTail(NULL),
   _nextClassChainId(1),// ID 0 is invalid
   _classChainMonitor(TR::Monitor::create("JIT-JITServerAOTCacheClassChainMonitor")),
   _wellKnownClassesMap(decltype(_wellKnownClassesMap)::allocator_type(TR::Compiler->persistentGlobalAllocator())),
   _wellKnownClassesHead(NULL),
   _wellKnownClassesTail(NULL),
   _nextWellKnownClassesId(1),// ID 0 is invalid
   _wellKnownClassesMonitor(TR::Monitor::create("JIT-JITServerAOTCacheWellKnownClassesMonitor")),
   _aotHeaderMap(decltype(_aotHeaderMap)::allocator_type(TR::Compiler->persistentGlobalAllocator())),
   _aotHeaderHead(NULL),
   _aotHeaderTail(NULL),
   _nextAOTHeaderId(1),// ID 0 is invalid
   _aotHeaderMonitor(TR::Monitor::create("JIT-JITServerAOTCacheAOTHeaderMonitor")),
   _cachedMethodMap(decltype(_cachedMethodMap)::allocator_type(TR::Compiler->persistentGlobalAllocator())),
   _cachedMethodHead(NULL),
   _cachedMethodTail(NULL),
   _cachedMethodMonitor(TR::Monitor::create("JIT-JITServerAOTCacheCachedMethodMonitor")),
   _numCacheBypasses(0), _numCacheHits(0), _numCacheMisses(0),
   _numDeserializedMethods(0), _numDeserializationFailures(0)
   {
   bool allMonitors = _classLoaderMonitor && _classMonitor && _methodMonitor &&
                      _classChainMonitor && _wellKnownClassesMonitor &&
                      _aotHeaderMonitor && _cachedMethodMonitor;
   if (!allMonitors)
      throw std::bad_alloc();
   }

JITServerAOTCache::~JITServerAOTCache()
   {
   freeMapValues(_classLoaderMap);
   freeMapValues(_classMap);
   freeMapValues(_methodMap);
   freeMapValues(_classChainMap);
   freeMapValues(_wellKnownClassesMap);
   freeMapValues(_aotHeaderMap);
   freeMapValues(_cachedMethodMap);

   TR::Monitor::destroy(_classMonitor);
   TR::Monitor::destroy(_classLoaderMonitor);
   TR::Monitor::destroy(_methodMonitor);
   TR::Monitor::destroy(_classChainMonitor);
   TR::Monitor::destroy(_wellKnownClassesMonitor);
   TR::Monitor::destroy(_aotHeaderMonitor);
   TR::Monitor::destroy(_cachedMethodMonitor);
   }


// Helper macros to make the code for printing class and method names to vlog more concise
#define RECORD_NAME(record) (int)(record).nameLength(), (const char *)(record).name()
#define LENGTH_AND_DATA(str) J9UTF8_LENGTH(str), (const char *)J9UTF8_DATA(str)
#define ROMMETHOD_NAS(romMethod) \
   LENGTH_AND_DATA(J9ROMMETHOD_NAME(romMethod)), LENGTH_AND_DATA(J9ROMMETHOD_SIGNATURE(romMethod))


const AOTCacheClassLoaderRecord *
JITServerAOTCache::getClassLoaderRecord(const uint8_t *name, size_t nameLength)
   {
   TR_ASSERT(nameLength, "Empty class loader identifying name");
   OMR::CriticalSection cs(_classLoaderMonitor);

   auto it = _classLoaderMap.find({ name, nameLength });
   if (it != _classLoaderMap.end())
      return it->second;

   if (!JITServerAOTCacheMap::cacheHasSpace())
      {
      return NULL;
      }

   auto record = AOTCacheClassLoaderRecord::create(_nextClassLoaderId, name, nameLength);
   addToMap(_classLoaderMap, it, { record->data().name(), record->data().nameLength() }, record);
   ++_nextClassLoaderId;

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
         "AOT cache %s: created class loader ID %zu -> %.*s",
         _name.c_str(), record->data().id(), (int)nameLength, (const char *)name
      );

   return record;
   }

const AOTCacheClassRecord *
JITServerAOTCache::getClassRecord(const AOTCacheClassLoaderRecord *classLoaderRecord, const J9ROMClass *romClass)
   {
   JITServerROMClassHash hash;
   if (auto cache = TR::CompilationInfo::get()->getJITServerSharedROMClassCache())
      hash = cache->getHash(romClass);
   else
      hash = JITServerROMClassHash(romClass);

   OMR::CriticalSection cs(_classMonitor);

   auto it = _classMap.find({ classLoaderRecord, &hash });
   if (it != _classMap.end())
      return it->second;

   if (!JITServerAOTCacheMap::cacheHasSpace())
      {
      return NULL;
      }

   auto record = AOTCacheClassRecord::create(_nextClassId, classLoaderRecord, hash, romClass);
   addToMap(_classMap, it, { classLoaderRecord, &record->data().hash() }, record);
   ++_nextClassId;

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      {
      const ClassSerializationRecord &c = record->data();
      char buffer[ROMCLASS_HASH_BYTES * 2 + 1];
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
         "AOT cache %s: created class ID %zu -> %.*s size %u hash %s",
         _name.c_str(), c.id(), RECORD_NAME(c), romClass->romSize, hash.toString(buffer, sizeof(buffer))
      );
      }

   return record;
   }

const AOTCacheMethodRecord *
JITServerAOTCache::getMethodRecord(const AOTCacheClassRecord *definingClassRecord,
                                   uint32_t index, const J9ROMMethod *romMethod)
   {
   MethodKey key(definingClassRecord, index);
   OMR::CriticalSection cs(_methodMonitor);

   auto it = _methodMap.find(key);
   if (it != _methodMap.end())
      return it->second;

   if (!JITServerAOTCacheMap::cacheHasSpace())
      {
      return NULL;
      }

   auto record = AOTCacheMethodRecord::create(_nextMethodId, definingClassRecord, index);
   addToMap(_methodMap, it, key, record);
   ++_nextMethodId;

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      {
      const ClassSerializationRecord &c = definingClassRecord->data();
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
         "AOT cache %s: created method ID %zu -> %.*s.%.*s%.*s index %u class ID %zu",
         _name.c_str(), record->data().id(), RECORD_NAME(c), ROMMETHOD_NAS(romMethod), index, c.id()
      );
      }

   return record;
   }

const AOTCacheClassChainRecord *
JITServerAOTCache::getClassChainRecord(const AOTCacheClassRecord *const *classRecords, size_t length)
   {
   OMR::CriticalSection cs(_classChainMonitor);

   auto it = _classChainMap.find({ classRecords, length });
   if (it != _classChainMap.end())
      return it->second;

   if (!JITServerAOTCacheMap::cacheHasSpace())
      {
      return NULL;
      }

   auto record = AOTCacheClassChainRecord::create(_nextClassChainId, classRecords, length);
   addToMap(_classChainMap, it, { record->records(), length }, record);
   ++_nextClassChainId;

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      {
      const ClassSerializationRecord &c = classRecords[0]->data();
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
         "AOT cache %s: created class chain ID %zu -> %.*s ID %zu length %zu",
         _name.c_str(), record->data().id(), RECORD_NAME(c), c.id(), length
      );
      }

   return record;
   }

const AOTCacheWellKnownClassesRecord *
JITServerAOTCache::getWellKnownClassesRecord(const AOTCacheClassChainRecord *const *chainRecords,
                                             size_t length, uintptr_t includedClasses)
{
   OMR::CriticalSection cs(_wellKnownClassesMonitor);

   auto it = _wellKnownClassesMap.find({ chainRecords, length, includedClasses });
   if (it != _wellKnownClassesMap.end())
      return it->second;

   if (!JITServerAOTCacheMap::cacheHasSpace())
      {
      return NULL;
      }

   auto record = AOTCacheWellKnownClassesRecord::create(_nextWellKnownClassesId, chainRecords, length, includedClasses);
   addToMap(_wellKnownClassesMap, it, { record->records(), length, includedClasses }, record);
   ++_nextWellKnownClassesId;

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
         "AOT cache %s: created well-known classes ID %zu -> length %zu includedClasses %zx",
         _name.c_str(), record->data().id(), includedClasses, length
      );

   return record;
}

const AOTCacheAOTHeaderRecord *
JITServerAOTCache::getAOTHeaderRecord(const TR_AOTHeader *header, uint64_t clientUID)
   {
   OMR::CriticalSection cs(_aotHeaderMonitor);

   auto it = _aotHeaderMap.find({ header });
   if (it != _aotHeaderMap.end())
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "AOT cache %s: using existing AOT header ID %zu for clientUID %llu",
            _name.c_str(), it->second->data().id(), (unsigned long long)clientUID
         );
      return it->second;
      }

   if (!JITServerAOTCacheMap::cacheHasSpace())
      {
      return NULL;
      }

   auto record = AOTCacheAOTHeaderRecord::create(_nextAOTHeaderId, header);
   addToMap(_aotHeaderMap, it, { record->data().header() }, record);
   ++_nextAOTHeaderId;

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
         "AOT cache %s: created AOT header ID %zu for clientUID %llu",
         _name.c_str(), record->data().id(), (unsigned long long)clientUID
      );

   return record;
   }


bool
JITServerAOTCache::storeMethod(const AOTCacheClassChainRecord *definingClassChainRecord, uint32_t index,
                               TR_Hotness optLevel, const AOTCacheAOTHeaderRecord *aotHeaderRecord,
                               const Vector<std::pair<const AOTCacheRecord *, uintptr_t/*reloDataOffset*/>> &records,
                               const void *code, size_t codeSize, const void *data, size_t dataSize,
                               const char *signature, uint64_t clientUID)
   {
   uintptr_t definingClassId = definingClassChainRecord->records()[0]->data().id();
   const char *levelName = TR::Compilation::getHotnessName(optLevel);

   CachedMethodKey key(definingClassChainRecord, index, optLevel, aotHeaderRecord);
   OMR::CriticalSection cs(_cachedMethodMonitor);

   if (!JITServerAOTCacheMap::cacheHasSpace())
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "AOT cache %s: method %s @ %s index %u class ID %zu AOT header ID %zu compiled fully but failed to store due to AOT cache size limit",
            _name.c_str(), signature, levelName, index, definingClassId, aotHeaderRecord->data().id()
         );
      return false;
      }

   auto it = _cachedMethodMap.find(key);
   if (it != _cachedMethodMap.end())
      {
      //NOTE: Current implementation keeps the first version of the method for this key in the cache.
      //      If we want to keep the most recent version instead, we will need to synchronize deleting
      //      the old version with any concurrent threads that could be sending it to other clients.
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "AOT cache %s: method %s @ %s index %u class ID %zu AOT header ID %zu already exists",
            _name.c_str(), signature, levelName, index, definingClassId, aotHeaderRecord->data().id()
         );
      return false;
      }

   auto method = CachedAOTMethod::create(definingClassChainRecord, index, optLevel, aotHeaderRecord,
                                         records, code, codeSize, data, dataSize);
   addToMap(_cachedMethodMap, it, key, method);

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
         "AOT cache %s: stored method %s @ %s index %u class ID %zu "
         "AOT header ID %zu with %zu serialization records for clientUID %llu",
         _name.c_str(), signature, levelName, index, definingClassId,
         aotHeaderRecord->data().id(), records.size(), (unsigned long long)clientUID
      );

   return true;
   }

const CachedAOTMethod *
JITServerAOTCache::findMethod(const AOTCacheClassChainRecord *definingClassChainRecord, uint32_t index,
                              TR_Hotness optLevel, const AOTCacheAOTHeaderRecord *aotHeaderRecord)
   {
   CachedMethodKey key(definingClassChainRecord, index, optLevel, aotHeaderRecord);
   OMR::CriticalSection cs(_cachedMethodMonitor);

   auto it = _cachedMethodMap.find(key);
   if (it == _cachedMethodMap.end())
      {
      ++_numCacheMisses;
      return NULL;
      }

   ++_numCacheHits;
   return it->second;
   }


Vector<const AOTSerializationRecord *>
JITServerAOTCache::getSerializationRecords(const CachedAOTMethod *method, const KnownIdSet &knownIds,
                                           TR_Memory &trMemory) const
   {
   VectorAllocator<const AOTSerializationRecord *> resultAllocator(trMemory.heapMemoryRegion());
   Vector<const AOTSerializationRecord *> result(resultAllocator);

   TR::StackMemoryRegion stackMemoryRegion(trMemory);
   UnorderedSetAllocator<const AOTCacheRecord *> newRecordsAllocator(trMemory.currentStackRegion());
   // Keep track of visited records to avoid duplicates
   UnorderedSet<const AOTCacheRecord *> newRecords(newRecordsAllocator);

   addRecord(method->definingClassChainRecord(), result, newRecords, knownIds);
   //NOTE: AOT header record doesn't need to be sent to the client.
   //      If the cached method was found for this client's compilation
   //      request, its AOT header is already guaranteed to be compatible.
   for (size_t i = 0; i < method->data().numRecords(); ++i)
      addRecord(method->records()[i], result, newRecords, knownIds);

   return result;
   }

void
JITServerAOTCache::addRecord(const AOTCacheRecord *record, Vector<const AOTSerializationRecord *> &result,
                             UnorderedSet<const AOTCacheRecord *> &newRecords, const KnownIdSet &knownIds) const
   {
   // Check if the record is already known and deserialized at the client
   const AOTSerializationRecord *data = record->dataAddr();
   uintptr_t idAndType = AOTSerializationRecord::idAndType(data->id(), data->type());
   if (knownIds.find(idAndType) != knownIds.end())
      return;

   // Check if the record was already visited
   auto it = newRecords.find(record);
   if (it != newRecords.end())
      return;

   //NOTE: Using recursion here is reasonable since its depth is limited to a maximum of 3 nested calls:
   //      wkc record -> class chain record -> class record -> class loader record
   record->subRecordsDo([&](const AOTCacheRecord *r)
      {
      addRecord(r, result, newRecords, knownIds);
      });

   newRecords.insert(record);
   result.push_back(data);
   }


void
JITServerAOTCache::printStats(FILE *f) const
   {
   fprintf(f,
      "JITServer AOT cache %s statistics:\n"
      "\tstored methods: %zu\n"
      "\tclass loader records: %zu\n"
      "\tclass records: %zu\n"
      "\tmethod records: %zu\n"
      "\tclass chain records: %zu\n"
      "\twell-known classes records: %zu\n"
      "\tAOT header records: %zu\n"
      "\tcache bypasses: %zu\n"
      "\tcache hits: %zu\n"
      "\tcache misses: %zu\n"
      "\tdeserialized methods: %zu\n"
      "\tdeserialization failures: %zu\n",
      _name.c_str(),
      _cachedMethodMap.size(),
      _classLoaderMap.size(),
      _classMap.size(),
      _methodMap.size(),
      _classChainMap.size(),
      _wellKnownClassesMap.size(),
      _aotHeaderMap.size(),
      _numCacheBypasses,
      _numCacheHits,
      _numCacheMisses,
      _numDeserializedMethods,
      _numDeserializationFailures
   );
   }

bool
JITServerAOTCacheMap::cacheHasSpace()
   {
   if (_cacheIsFull)
      {
      return false;
      }


   // The AOT cache allocations are used as a stand-in for the total memory used by all AOT caches.
   // This underestimates the true value, but should be correlated with it.
   size_t aotTotalRecordAllocations = TR::Compiler->persistentGlobalMemory()->_totalPersistentAllocations[TR_Memory::JITServerAOTCache];
   if (aotTotalRecordAllocations >= _cacheMaxBytes)
      {
      _cacheIsFull = true;
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                                        "AOT cache allocations exceeded maximum of %zu bytes, disabling future allocations",
                                        _cacheMaxBytes);
         }
      return false;
      }
   else
      {
      return true;
      }
   }

JITServerAOTCacheMap::JITServerAOTCacheMap() :
   _map(decltype(_map)::allocator_type(TR::Compiler->persistentGlobalAllocator())),
   _monitor(TR::Monitor::create("JIT-JITServerAOTCacheMapMonitor"))
   {
   if (!_monitor)
      throw std::bad_alloc();
   }

JITServerAOTCacheMap::~JITServerAOTCacheMap()
   {
   for (auto &kv : _map)
      {
      kv.second->~JITServerAOTCache();
      TR::Compiler->persistentGlobalMemory()->freePersistentMemory(kv.second);
      }
   TR::Monitor::destroy(_monitor);
   }


JITServerAOTCache *
JITServerAOTCacheMap::get(const std::string &name, uint64_t clientUID)
   {
   OMR::CriticalSection cs(_monitor);

   auto it = _map.find(name);
   if (it != _map.end())
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Using existing AOT cache %s for clientUID %llu",
                                        name.c_str(), (unsigned long long)clientUID);
      return it->second;
      }

   if (!JITServerAOTCacheMap::cacheHasSpace())
      {
      return NULL;
      }

   auto cache = new (TR::Compiler->persistentGlobalMemory()) JITServerAOTCache(name);
   if (!cache)
      throw std::bad_alloc();

   try
      {
      _map.insert(it, { name, cache });
      }
   catch (...)
      {
      cache->~JITServerAOTCache();
      TR::Compiler->persistentGlobalMemory()->freePersistentMemory(cache);
      throw;
      }

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Created AOT cache %s for clientUID %llu",
                                     name.c_str(), (unsigned long long)clientUID);
   return cache;
   }


size_t
JITServerAOTCacheMap::getNumDeserializedMethods() const
   {
   size_t result = 0;
   OMR::CriticalSection cs(_monitor);
   for (auto &it : _map)
      result += it.second->getNumDeserializedMethods();
   return result;
   }

void
JITServerAOTCacheMap::printStats(FILE *f) const
   {
   OMR::CriticalSection cs(_monitor);
   for (auto &it : _map)
      it.second->printStats(f);
   }
