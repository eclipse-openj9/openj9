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

#include <cstdio> // for rename()
#include "control/CompilationRuntime.hpp"
#include "env/J9SegmentProvider.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/SystemSegmentProvider.hpp"
#include "infra/CriticalSection.hpp"
#include "runtime/JITServerAOTCache.hpp"
#include "runtime/JITServerSharedROMClassCache.hpp"
#include "net/CommunicationStream.hpp"

struct JITServerAOTCacheReadContext
   {
   JITServerAOTCacheReadContext(const JITServerAOTCacheHeader &header, TR::StackMemoryRegion &stackMemoryRegion);

   Vector<AOTCacheClassLoaderRecord *> _classLoaderRecords;
   Vector<AOTCacheClassRecord *> _classRecords;
   Vector<AOTCacheMethodRecord *> _methodRecords;
   Vector<AOTCacheClassChainRecord *> _classChainRecords;
   Vector<AOTCacheWellKnownClassesRecord *> _wellKnownClassesRecords;
   Vector<AOTCacheAOTHeaderRecord *> _aotHeaderRecords;
   Vector<AOTCacheThunkRecord *> _thunkRecords;
   };

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

// Read a single AOT cache record R from a cache file
template<class R> R *
AOTCacheRecord::readRecord(FILE *f, const JITServerAOTCacheReadContext &context)
   {
   typename R::SerializationRecord header;
   if (1 != fread(&header, sizeof(header), 1, f))
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: Could not read %s record header", R::getRecordName());
      return NULL;
      }

   if (!header.isValidHeader(context))
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: Header for %s record is invalid", R::getRecordName());
      return NULL;
      }

   R *record = new (AOTCacheRecord::allocate(R::size(header))) R(context, header);
   memcpy((void *)record->dataAddr(), &header, sizeof(header));

   size_t variableDataBytes = record->dataAddr()->size() - sizeof(header);
   if (0 != variableDataBytes)
      {
      if (1 != fread((uint8_t *)record->dataAddr() + sizeof(header), variableDataBytes, 1, f))
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: Unable to read variable part of %s record", R::getRecordName());
         AOTCacheRecord::free(record);
         return NULL;
         }
      }

   if (!record->setSubrecordPointers(context))
      {
      AOTCacheRecord::free(record);
      return NULL;
      }

   return record;
   }

bool
AOTSerializationRecord::isValidHeader(AOTSerializationRecordType type) const
   {
   return (type == this->type()) &&
          (0 != this->id());
   }

ClassLoaderSerializationRecord::ClassLoaderSerializationRecord() :
   AOTSerializationRecord(0, 0, AOTSerializationRecordType::ClassLoader),
   _nameLength(0)
   {
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

ClassSerializationRecord::ClassSerializationRecord() :
   AOTSerializationRecord(0, 0, AOTSerializationRecordType::Class),
   _classLoaderId(0), _hash(), _romClassSize(0), _nameLength(0)
   {
   }

bool
ClassSerializationRecord::isValidHeader(const JITServerAOTCacheReadContext &context) const
   {
   return AOTSerializationRecord::isValidHeader(AOTSerializationRecordType::Class) &&
          (classLoaderId() < context._classLoaderRecords.size()) &&
          context._classLoaderRecords[classLoaderId()];
   }

AOTCacheClassRecord::AOTCacheClassRecord(uintptr_t id, const AOTCacheClassLoaderRecord *classLoaderRecord,
                                         const JITServerROMClassHash &hash, const J9ROMClass *romClass) :
   _classLoaderRecord(classLoaderRecord),
   _data(id, classLoaderRecord->data().id(), hash, romClass)
   {
   }

AOTCacheClassRecord::AOTCacheClassRecord(const JITServerAOTCacheReadContext &context, const ClassSerializationRecord &header) :
   _classLoaderRecord(context._classLoaderRecords[header.classLoaderId()])
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

MethodSerializationRecord::MethodSerializationRecord() :
   AOTSerializationRecord(0, 0, AOTSerializationRecordType::Method),
   _definingClassId(0), _index(0)
   {
   }

bool
MethodSerializationRecord::isValidHeader(const JITServerAOTCacheReadContext &context) const
      {
      return AOTSerializationRecord::isValidHeader(AOTSerializationRecordType::Method) &&
             (definingClassId() < context._classRecords.size()) &&
             context._classRecords[definingClassId()];
      }

AOTCacheMethodRecord::AOTCacheMethodRecord(uintptr_t id, const AOTCacheClassRecord *definingClassRecord,
                                           uint32_t index) :
   _definingClassRecord(definingClassRecord),
   _data(id, definingClassRecord->data().id(), index)
   {
   }

AOTCacheMethodRecord::AOTCacheMethodRecord(const JITServerAOTCacheReadContext &context, const MethodSerializationRecord &header) :
   _definingClassRecord(context._classRecords[header.definingClassId()])
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

template<class D, class R, typename... Args> bool
AOTCacheListRecord<D, R, Args...>::setSubrecordPointers(const Vector<R *> &cacheRecords, const char *recordName, const char *subrecordName)
   {
   for (size_t i = 0; i < data().list().length(); ++i)
      {
      uintptr_t id = data().list().ids()[i];
      if ((id >= cacheRecords.size()) || !cacheRecords[id])
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: Invalid %s subrecord: type %s, ID %zu", recordName, subrecordName, id);
         return false;
         }
      records()[i] = cacheRecords[id];
      }
   return true;
   }

ClassChainSerializationRecord::ClassChainSerializationRecord(uintptr_t id, size_t length) :
   AOTSerializationRecord(size(length), id, AOTSerializationRecordType::ClassChain),
   _list(length)
   {
   }

ClassChainSerializationRecord::ClassChainSerializationRecord() :
   AOTSerializationRecord(0, 0, AOTSerializationRecordType::ClassChain),
   _list(0)
   {
   }

AOTCacheClassChainRecord *
AOTCacheClassChainRecord::create(uintptr_t id, const AOTCacheClassRecord *const *records, size_t length)
   {
   void *ptr = AOTCacheRecord::allocate(size(length));
   return new (ptr) AOTCacheClassChainRecord(id, records, length);
   }

bool
AOTCacheClassChainRecord::setSubrecordPointers(const JITServerAOTCacheReadContext &context)
   {
   return AOTCacheListRecord::setSubrecordPointers(context._classRecords, "class chain", "class");
   }

WellKnownClassesSerializationRecord::WellKnownClassesSerializationRecord(uintptr_t id, size_t length,
                                                                         uintptr_t includedClasses) :
   AOTSerializationRecord(size(length), id, AOTSerializationRecordType::WellKnownClasses),
   _includedClasses(includedClasses), _list(length)
   {
   }

WellKnownClassesSerializationRecord::WellKnownClassesSerializationRecord() :
   AOTSerializationRecord(0, 0, AOTSerializationRecordType::WellKnownClasses),
   _includedClasses(0), _list(0)
   {
   }

AOTCacheWellKnownClassesRecord *
AOTCacheWellKnownClassesRecord::create(uintptr_t id, const AOTCacheClassChainRecord *const *records,
                                       size_t length, uintptr_t includedClasses)
   {
   void *ptr = AOTCacheRecord::allocate(size(length));
   return new (ptr) AOTCacheWellKnownClassesRecord(id, records, length, includedClasses);
   }

bool
AOTCacheWellKnownClassesRecord::setSubrecordPointers(const JITServerAOTCacheReadContext &context)
   {
   return AOTCacheListRecord::setSubrecordPointers(context._classChainRecords, "well-known classes", "class chain");
   }

AOTHeaderSerializationRecord::AOTHeaderSerializationRecord(uintptr_t id, const TR_AOTHeader *header) :
   AOTSerializationRecord(sizeof(*this), id, AOTSerializationRecordType::AOTHeader),
   _header(*header)
   {
   }

AOTHeaderSerializationRecord::AOTHeaderSerializationRecord() :
   AOTSerializationRecord(0, 0, AOTSerializationRecordType::AOTHeader),
   _header({0})
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

ThunkSerializationRecord::ThunkSerializationRecord(uintptr_t id, const uint8_t *signature, uint32_t signatureSize,
                                                   const uint8_t *thunkStart,  uint32_t thunkSize) :
   AOTSerializationRecord(size(signatureSize, thunkSize), id, AOTSerializationRecordType::Thunk),
   _signatureSize(signatureSize),
   _thunkSize(thunkSize)
   {
   memcpy((void *)this->signature(), signature, signatureSize);
   memcpy((void *)this->thunkStart(), thunkStart, thunkSize);
   }

ThunkSerializationRecord::ThunkSerializationRecord() :
   AOTSerializationRecord(0, 0, AOTSerializationRecordType::Thunk),
   _signatureSize(0),
   _thunkSize(0)
   {
   }

AOTCacheThunkRecord::AOTCacheThunkRecord(uintptr_t id, const uint8_t *signature, uint32_t signatureSize,
                                         const uint8_t *thunkStart, uint32_t thunkSize) :
   _data(id, signature, signatureSize, thunkStart, thunkSize)
   {
   }

AOTCacheThunkRecord *
AOTCacheThunkRecord::create(uintptr_t id, const uint8_t *signature, uint32_t signatureSize, const uint8_t *thunkStart, uint32_t thunkSize)
   {
   void *ptr = AOTCacheRecord::allocate(size(signatureSize, thunkSize));
   return new (ptr) AOTCacheThunkRecord(id, signature, signatureSize, thunkStart, thunkSize);
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

SerializedAOTMethod::SerializedAOTMethod() :
   _size(0),
   _definingClassChainId(0), _index(0),
   _optLevel(TR_Hotness::numHotnessLevels), _aotHeaderId(0),
   _numRecords(0), _codeSize(0), _dataSize(0)
   {
   }

bool
SerializedAOTMethod::isValidHeader(const JITServerAOTCacheReadContext &context) const
   {
   return _optLevel < TR_Hotness::numHotnessLevels &&
          (definingClassChainId() < context._classChainRecords.size()) &&
          (aotHeaderId() < context._aotHeaderRecords.size()) &&
          context._classChainRecords[definingClassChainId()] &&
          context._aotHeaderRecords[aotHeaderId()];
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

CachedAOTMethod::CachedAOTMethod(const JITServerAOTCacheReadContext &context, const SerializedAOTMethod &header) :
   _nextRecord(NULL),
   _definingClassChainRecord(context._classChainRecords[header.definingClassChainId()])
   {
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
CachedAOTMethod::setSubrecordPointers(const JITServerAOTCacheReadContext &context)
   {
   const char *invalidSubrecordName = "";
   size_t subrecordId = 0;

   // The defining class chain record pointer for the record will have been set by the CachedAOTMethod constructor at this point
   for (size_t i = 0; i < data().numRecords(); ++i)
      {
      const SerializedSCCOffset &sccOffset = data().offsets()[i];
      subrecordId = sccOffset.recordId();

      switch (sccOffset.recordType())
         {
         case AOTSerializationRecordType::ClassLoader:
            if ((subrecordId >= context._classLoaderRecords.size()) || !context._classLoaderRecords[subrecordId])
               {
               invalidSubrecordName = AOTCacheClassLoaderRecord::getRecordName();
               goto error;
               }
            records()[i] = context._classLoaderRecords[subrecordId];
            break;
         case AOTSerializationRecordType::Class:
            if ((subrecordId >= context._classRecords.size()) || !context._classRecords[subrecordId])
               {
               invalidSubrecordName = AOTCacheClassRecord::getRecordName();
               goto error;
               }
            records()[i] = context._classRecords[subrecordId];
            break;
         case AOTSerializationRecordType::Method:
            if ((subrecordId >= context._methodRecords.size()) || !context._methodRecords[subrecordId])
               {
               invalidSubrecordName = AOTCacheMethodRecord::getRecordName();
               goto error;
               }
            records()[i] = context._methodRecords[subrecordId];
            break;
         case AOTSerializationRecordType::ClassChain:
            if ((subrecordId >= context._classChainRecords.size()) || !context._classChainRecords[subrecordId])
               {
               invalidSubrecordName = AOTCacheClassChainRecord::getRecordName();
               goto error;
               }
            records()[i] = context._classChainRecords[subrecordId];
            break;
         case AOTSerializationRecordType::WellKnownClasses:
            if ((subrecordId >= context._wellKnownClassesRecords.size()) || !context._wellKnownClassesRecords[subrecordId])
               {
               invalidSubrecordName = AOTCacheWellKnownClassesRecord::getRecordName();
               goto error;
               }
            records()[i] = context._wellKnownClassesRecords[subrecordId];
            break;
         case AOTSerializationRecordType::AOTHeader: // never associated with an SCC offset
            invalidSubrecordName = AOTCacheAOTHeaderRecord::getRecordName();
            goto error;
         case AOTSerializationRecordType::Thunk:
            if ((subrecordId >= context._thunkRecords.size()) || !context._thunkRecords[subrecordId])
               {
               invalidSubrecordName = AOTCacheThunkRecord::getRecordName();
               goto error;
               }
            records()[i] = context._thunkRecords[subrecordId];
            break;
         default:
            invalidSubrecordName = "invalid";
            goto error;
         }
      }

   return true;

error:
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                                     "AOT cache: Invalid cached AOT method subrecord: type %s, ID %zu",
                                     invalidSubrecordName,
                                     subrecordId);
   return false;
   }

bool
JITServerAOTCache::StringKey::operator==(const StringKey &k) const
   {
   return J9UTF8_DATA_EQUALS(_string, _stringLength, k._string, k._stringLength);
   }

size_t
JITServerAOTCache::StringKey::Hash::operator()(const StringKey &k) const noexcept
   {
   size_t h = 0;
   for (size_t i = 0; i < k._stringLength; ++i)
      h = (h << 5) - h + k._string[i];
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
// Also insert it into the linked list traversal of the map defined by the
// given head and tail.
template<typename K, typename V, typename H> static void
addToMap(PersistentUnorderedMap<K, V *, H> &map,
         V *&traversalHead,
         V *&traversalTail,
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

   // Normally we would need a write barrier here to ensure that the record was fully written to memory before
   // adding it to this traversal. However, since we save the number of records to be written in writeRecordList,
   // we will never encounter such a partial record in the serializer, and so the write barrier is unnecessary.
   if (traversalTail == NULL)
      {
      traversalHead = value;
      }
   else
      {
      traversalTail->setNextRecord(value);
      }
   traversalTail = value;
   }

// Insert the value (which must be allocated with AOTCacheRecord::allocate())
// with the key into the map, avoiding memory leaks in case of exceptions.
// If the map insertion was successful, also insert it into the linked list traversal of
// the map defined by the given head and tail.
template<typename K, typename V, typename H> static bool
addToMap(PersistentUnorderedMap<K, V *, H> &map,
         V *&traversalHead,
         V *&traversalTail,
         const K &key, V *value)
   {
   bool insertSuccess = false;
   try
      {
      insertSuccess = map.insert({ key, value }).second;
      }
   catch (...)
      {
      AOTCacheRecord::free(value);
      throw;
      }

   if (!insertSuccess)
      return false;

   if (traversalTail == NULL)
      {
      traversalHead = value;
      }
   else
      {
      traversalTail->setNextRecord(value);
      }
   traversalTail = value;

   return true;
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
   _thunkMap(decltype(_thunkMap)::allocator_type(TR::Compiler->persistentGlobalAllocator())),
   _thunkHead(NULL),
   _thunkTail(NULL),
   _nextThunkId(1),// ID 0 is invalid
   _thunkMonitor(TR::Monitor::create("JIT-JITServerAOTCacheThunkMonitor")),
   _cachedMethodMap(decltype(_cachedMethodMap)::allocator_type(TR::Compiler->persistentGlobalAllocator())),
   _cachedMethodHead(NULL),
   _cachedMethodTail(NULL),
   _cachedMethodMonitor(TR::Monitor::create("JIT-JITServerAOTCacheCachedMethodMonitor")),
   _timePrevSaveOperation(0),
   _minNumAOTMethodsToSave(TR::Options::_aotCachePersistenceMinDeltaMethods),
   _saveOperationInProgress(false), // protected by the _cachedMethodMonitor
   _excludedFromSavingToFile(false),
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
   freeMapValues(_thunkMap);
   freeMapValues(_cachedMethodMap);

   TR::Monitor::destroy(_classMonitor);
   TR::Monitor::destroy(_classLoaderMonitor);
   TR::Monitor::destroy(_methodMonitor);
   TR::Monitor::destroy(_classChainMonitor);
   TR::Monitor::destroy(_wellKnownClassesMonitor);
   TR::Monitor::destroy(_aotHeaderMonitor);
   TR::Monitor::destroy(_thunkMonitor);
   TR::Monitor::destroy(_cachedMethodMonitor);
   }

JITServerAOTCacheReadContext::JITServerAOTCacheReadContext(const JITServerAOTCacheHeader &header, TR::StackMemoryRegion &stackMemoryRegion) :
   _classLoaderRecords(header._nextClassLoaderId, NULL, stackMemoryRegion),
   _classRecords(header._nextClassId, NULL, stackMemoryRegion),
   _methodRecords(header._nextMethodId, NULL, stackMemoryRegion),
   _classChainRecords(header._nextClassChainId, NULL, stackMemoryRegion),
   _wellKnownClassesRecords(header._nextWellKnownClassesId, NULL, stackMemoryRegion),
   _aotHeaderRecords(header._nextAOTHeaderId, NULL, stackMemoryRegion),
   _thunkRecords(header._nextThunkId, NULL, stackMemoryRegion)
   {
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
   addToMap(_classLoaderMap, _classLoaderHead, _classLoaderTail, it, getRecordKey(record), record);
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
   addToMap(_classMap, _classHead, _classTail, it, getRecordKey(record), record);
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
   OMR::CriticalSection cs(_methodMonitor);

   auto it = _methodMap.find({ definingClassRecord, index });
   if (it != _methodMap.end())
      return it->second;

   if (!JITServerAOTCacheMap::cacheHasSpace())
      {
      return NULL;
      }

   auto record = AOTCacheMethodRecord::create(_nextMethodId, definingClassRecord, index);
   addToMap(_methodMap, _methodHead, _methodTail, it, getRecordKey(record), record);
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
   addToMap(_classChainMap, _classChainHead, _classChainTail, it, getRecordKey(record), record);
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
   addToMap(_wellKnownClassesMap, _wellKnownClassesHead, _wellKnownClassesTail, it, getRecordKey(record), record);
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
   addToMap(_aotHeaderMap, _aotHeaderHead, _aotHeaderTail, it, getRecordKey(record), record);
   ++_nextAOTHeaderId;

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
         "AOT cache %s: created AOT header ID %zu for clientUID %llu",
         _name.c_str(), record->data().id(), (unsigned long long)clientUID
      );

   return record;
   }

const AOTCacheThunkRecord *
JITServerAOTCache::getThunkRecord(const uint8_t *signature, uint32_t signatureSize)
   {
   OMR::CriticalSection cs(_thunkMonitor);

   auto it = _thunkMap.find({ signature, signatureSize });
   if (it != _thunkMap.end())
      return it->second;

   return NULL;
   }

const AOTCacheThunkRecord *
JITServerAOTCache::createAndStoreThunk(const uint8_t *signature, uint32_t signatureSize, const uint8_t *thunkStart, uint32_t thunkSize)
   {
   OMR::CriticalSection cs(_thunkMonitor);

   auto it = _thunkMap.find({ signature, signatureSize });
   if (it != _thunkMap.end())
      return it->second;

   if (!JITServerAOTCacheMap::cacheHasSpace())
      return NULL;

   auto record = AOTCacheThunkRecord::create(_nextThunkId, signature, signatureSize, thunkStart, thunkSize);
   addToMap(_thunkMap, _thunkHead, _thunkTail, it, getRecordKey(record), record);
   ++_nextThunkId;

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
         "AOT cache %s: created thunk ID %zu -> %.*s thunkSize %u",
         _name.c_str(), record->data().id(), signatureSize, signature, thunkSize
      );

   return record;
   }


bool
JITServerAOTCache::storeMethod(const AOTCacheClassChainRecord *definingClassChainRecord, uint32_t index,
                               TR_Hotness optLevel, const AOTCacheAOTHeaderRecord *aotHeaderRecord,
                               const Vector<std::pair<const AOTCacheRecord *, uintptr_t/*reloDataOffset*/>> &records,
                               const void *code, size_t codeSize, const void *data, size_t dataSize,
                               const char *signature, uint64_t clientUID, const CachedAOTMethod *&methodRecord)
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
      methodRecord = it->second;
      return true;
      }

   auto method = CachedAOTMethod::create(definingClassChainRecord, index, optLevel, aotHeaderRecord,
                                         records, code, codeSize, data, dataSize);
   methodRecord = method;
   addToMap(_cachedMethodMap, _cachedMethodHead, _cachedMethodTail, it, key, method);

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


// Write at most numRecordsToWrite to the given stream from the linked list starting at head.
static bool
writeRecordList(FILE *f, const AOTCacheRecord *head, size_t numRecordsToWrite)
   {
   const AOTCacheRecord *current = head;
   size_t recordsWritten = 0;
   while (current && (recordsWritten < numRecordsToWrite))
      {
      const AOTSerializationRecord *record = current->dataAddr();
      if (1 != fwrite(record, record->size(), 1, f))
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: Unable to write record to cache file");
         return false;
         }
      ++recordsWritten;
      current = current->getNextRecord();
      }
   TR_ASSERT(recordsWritten == numRecordsToWrite, "Expected to write %zu records, wrote %zu", numRecordsToWrite, recordsWritten);

   return true;
   }

static bool
writeCachedMethodList(FILE *f, const CachedAOTMethod *head, size_t numRecordsToWrite)
   {
   const CachedAOTMethod *current = head;
   size_t recordsWritten = 0;
   while (current && (recordsWritten < numRecordsToWrite))
      {
      const SerializedAOTMethod *record = &current->data();
      if (1 != fwrite(record, record->size(), 1, f))
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: Unable to write record to cache file");
         return false;
         }
      ++recordsWritten;
      current = current->getNextRecord();
      }
   TR_ASSERT(recordsWritten == numRecordsToWrite, "Expected to write %zu records, wrote %zu", numRecordsToWrite, recordsWritten);

   return true;
   }

static void getCurrentAOTCacheVersion(JITServerAOTCacheVersion &version)
   {
   memcpy(version._eyeCatcher, JITSERVER_AOTCACHE_EYECATCHER, JITSERVER_AOTCACHE_EYECATCHER_LENGTH);
   version._snapshotVersion = JITSERVER_AOTCACHE_VERSION;
   version._jitserverVersion = JITServer::CommunicationStream::getJITServerFullVersion();
   }

// Write a full AOT cache snapshot to a stream. After the header information, the
// AOTSerializationRecord or SerializedAOTMethod data (depending on record type) in each
// record traversal is written directly to the stream in sections, since the full AOT record
// can be reconstructed from only this information. These sections are ordered so that, when
// reading the snapshot, the dependencies of each record will already have been read by the
// time we get to that record.
// Return the number of AOT methods written to the snapshot or 0 on failure.
size_t
JITServerAOTCache::writeCache(FILE *f) const
   {
   JITServerAOTCacheHeader header = {0};
   getCurrentAOTCacheVersion(header._version);
   header._serverUID = TR::CompilationInfo::get()->getPersistentInfo()->getServerUID();

   // It is possible for a record and its dependencies to be added between .size() calls,
   // so we must reverse the order in which we read the map sizes (compared to their write order)
   // to ensure that those dependencies are not excluded from serialization.
      {
      OMR::CriticalSection cs(_cachedMethodMonitor);
      header._numCachedAOTMethods = _cachedMethodMap.size();
      }
   if (header._numCachedAOTMethods == 0)
      {
      TR_ASSERT_FATAL(false, "Expected to write at least one method to the AOT cache file");
      return 0;
      }
      {
      OMR::CriticalSection cs(_thunkMonitor);
      header._numThunkRecords = _thunkMap.size();
      header._nextThunkId = _nextThunkId;
      }
      {
      OMR::CriticalSection cs(_aotHeaderMonitor);
      header._numAOTHeaderRecords = _aotHeaderMap.size();
      header._nextAOTHeaderId = _nextAOTHeaderId;
      }
      {
      OMR::CriticalSection cs(_wellKnownClassesMonitor);
      header._numWellKnownClassesRecords = _wellKnownClassesMap.size();
      header._nextWellKnownClassesId = _nextWellKnownClassesId;
      }
      {
      OMR::CriticalSection cs(_classChainMonitor);
      header._numClassChainRecords = _classChainMap.size();
      header._nextClassChainId = _nextClassChainId;
      }
      {
      OMR::CriticalSection cs(_methodMonitor);
      header._numMethodRecords = _methodMap.size();
      header._nextMethodId = _nextMethodId;
      }
      {
      OMR::CriticalSection cs(_classMonitor);
      header._numClassRecords = _classMap.size();
      header._nextClassId = _nextClassId;
      }
      {
      OMR::CriticalSection cs(_classLoaderMonitor);
      header._numClassLoaderRecords = _classLoaderMap.size();
      header._nextClassLoaderId = _nextClassLoaderId;
      }

   if (1 != fwrite(&header, sizeof(JITServerAOTCacheHeader), 1, f))
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: Unable to write cache file header");
      return 0;
      }

   if (!writeRecordList(f, _classLoaderHead, header._numClassLoaderRecords))
      return 0;
   if (!writeRecordList(f, _classHead, header._numClassRecords))
      return 0;
   if (!writeRecordList(f, _methodHead, header._numMethodRecords))
      return 0;
   if (!writeRecordList(f, _classChainHead, header._numClassChainRecords))
      return 0;
   if (!writeRecordList(f, _wellKnownClassesHead, header._numWellKnownClassesRecords))
      return 0;
   if (!writeRecordList(f, _aotHeaderHead, header._numAOTHeaderRecords))
      return 0;
   if (!writeRecordList(f, _thunkHead, header._numThunkRecords))
      return 0;
   if (!writeCachedMethodList(f, _cachedMethodHead, header._numCachedAOTMethods))
      return 0;

   return header._numCachedAOTMethods;
   }

// Tests whether or not the given AOT snapshot is compatible with the server.
static bool
isCompatibleSnapshotVersion(const JITServerAOTCacheVersion &version)
   {
   JITServerAOTCacheVersion currentVersion = {0};
   getCurrentAOTCacheVersion(currentVersion);

   return (0 == memcmp(version._eyeCatcher, currentVersion._eyeCatcher, sizeof(currentVersion._eyeCatcher))) &&
          (version._snapshotVersion == currentVersion._snapshotVersion) &&
          (version._jitserverVersion == currentVersion._jitserverVersion);
   }

// Read an AOT cache snapshot, returning NULL if the cache is ill-formed or
// incompatible with the running server.
JITServerAOTCache *
JITServerAOTCache::readCache(FILE *f, const std::string &name, TR_Memory &trMemory)
   {
   if (!JITServerAOTCacheMap::cacheHasSpace())
      return NULL;

   JITServerAOTCacheHeader header = {0};
   if (1 != fread(&header, sizeof(JITServerAOTCacheHeader), 1, f))
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: Unable to read cache file header");
      return NULL;
      }

   if (!isCompatibleSnapshotVersion(header._version))
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: Cache file header incompatible with running server");
      return NULL;
      }

   JITServerAOTCache *cache = NULL;
   try
      {
      cache = new (TR::Compiler->persistentGlobalMemory()) JITServerAOTCache(name);
      }
   catch (const std::exception &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache allocation failed with exception: %s", e.what());
         }
      }

   if (!cache)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: Unable to allocate new cache for reading");
      return NULL;
      }

   bool readSuccess = false;
   try
      {
      readSuccess = cache->readCache(f, header, trMemory);
      }
   catch (const std::exception &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache reading failed with exception: %s", e.what());
         }
      }

   if (!readSuccess)
      {
      cache->~JITServerAOTCache();
      TR::Compiler->persistentGlobalMemory()->freePersistentMemory(cache);
      cache = NULL;
      }

   return cache;
   }

// Read numRecordsToRead records of an AOTSerializationRecord subclass V from a stream, also
// updating the map, record traversal, and scratch Vector associated with V.
template<typename K, typename V, typename H> bool
JITServerAOTCache::readRecords(FILE *f,
                               JITServerAOTCacheReadContext &context,
                               size_t numRecordsToRead,
                               PersistentUnorderedMap<K, V *, H> &map,
                               V *&traversalHead,
                               V *&traversalTail,
                               Vector<V *> &records)
   {
   for (size_t i = 0; i < numRecordsToRead; ++i)
      {
      if (!JITServerAOTCacheMap::cacheHasSpace())
         return false;

      V *record = AOTCacheRecord::readRecord<V>(f, context);
      if (!record)
         return false;

      if ((record->data().id() >= records.size() ||
          records[record->data().id()]) ||
          !addToMap(map, traversalHead, traversalTail, getRecordKey(record), record))
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: Record of type %s has invalid or overlapping ID %zu",
                                           V::getRecordName(), record->data().id());
         AOTCacheRecord::free(record);
         return false;
         }

      records[record->data().id()] = record;
      }

   return true;
   }

bool
JITServerAOTCache::readCache(FILE *f, const JITServerAOTCacheHeader &header, TR_Memory &trMemory)
   {
   _classLoaderMap.reserve(header._numClassLoaderRecords);
   _classMap.reserve(header._numClassRecords);
   _methodMap.reserve(header._numMethodRecords);
   _classChainMap.reserve(header._numClassChainRecords);
   _wellKnownClassesMap.reserve(header._numWellKnownClassesRecords);
   _aotHeaderMap.reserve(header._numAOTHeaderRecords);
   _thunkMap.reserve(header._numThunkRecords);
   _cachedMethodMap.reserve(header._numCachedAOTMethods);

   _nextClassLoaderId = header._nextClassLoaderId;
   _nextClassId = header._nextClassId;
   _nextMethodId = header._nextMethodId;
   _nextClassChainId = header._nextClassChainId;
   _nextWellKnownClassesId = header._nextWellKnownClassesId;
   _nextAOTHeaderId = header._nextAOTHeaderId;
   _nextThunkId = header._nextThunkId;

   TR::StackMemoryRegion stackMemoryRegion(trMemory);
   JITServerAOTCacheReadContext context(header, stackMemoryRegion);

   if (!readRecords(f, context, header._numClassLoaderRecords, _classLoaderMap, _classLoaderHead, _classLoaderTail, context._classLoaderRecords))
      return false;
   if (!readRecords(f, context, header._numClassRecords, _classMap, _classHead, _classTail, context._classRecords))
      return false;
   if (!readRecords(f, context, header._numMethodRecords, _methodMap, _methodHead, _methodTail, context._methodRecords))
      return false;
   if (!readRecords(f, context, header._numClassChainRecords, _classChainMap, _classChainHead, _classChainTail, context._classChainRecords))
      return false;
   if (!readRecords(f, context, header._numWellKnownClassesRecords, _wellKnownClassesMap, _wellKnownClassesHead,
                    _wellKnownClassesTail, context._wellKnownClassesRecords))
      return false;
   if (!readRecords(f, context, header._numAOTHeaderRecords, _aotHeaderMap, _aotHeaderHead, _aotHeaderTail, context._aotHeaderRecords))
      return false;
   if (!readRecords(f, context, header._numThunkRecords, _thunkMap, _thunkHead, _thunkTail, context._thunkRecords))
      return false;

   for (size_t i = 0; i < header._numCachedAOTMethods; ++i)
      {
      if (!JITServerAOTCacheMap::cacheHasSpace())
         return false;

      auto record = AOTCacheRecord::readRecord<CachedAOTMethod>(f, context);
      if (!record)
         return false;

      CachedMethodKey key(record->definingClassChainRecord(),
                          record->data().index(),
                          record->data().optLevel(),
                          context._aotHeaderRecords[record->data().aotHeaderId()]);

      if (!addToMap(_cachedMethodMap, _cachedMethodHead, _cachedMethodTail, key, record))
         {
         AOTCacheRecord::free(record);
         return false;
         }
      }

   return true;
   }


size_t
JITServerAOTCache::getNumCachedMethods() const
   {
   OMR::CriticalSection cs(_cachedMethodMonitor);
   return _cachedMethodMap.size();
   }


bool
JITServerAOTCache::triggerAOTCacheStoreToFileIfNeeded()
   {
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get();
   auto aotCacheMap = compInfo->getJITServerAOTCacheMap();

      {
      OMR::CriticalSection cs(_cachedMethodMonitor);
      if (_saveOperationInProgress || _excludedFromSavingToFile)
         return false;

      // Check whether enough new methods were added to the in-memory cache to be worth attempting a save operation
      if (_cachedMethodMap.size() < _minNumAOTMethodsToSave)
         return false;

      // Prevent saving to file too often; wait some time between consecutive saves
      if (compInfo->getPersistentInfo()->getElapsedTime() < _timePrevSaveOperation + TR::Options::_aotCachePersistenceMinPeriodMs)
         return false;

      // Prevent other threads from starting a save operation
      // I can change my mind afterwards, but that's fine because
      // any other thread would have drawn the same conclusion
      _saveOperationInProgress = true;
      }

   // Memorize which AOT cache to save to file
   aotCacheMap->queueAOTCacheForSavingToFile(_name);

   // Place a special compilation request in the queue
   // Any I/O operation should be done asynchronously on a new compilation thread
   bool queuedRequest = false;
      {
      OMR::CriticalSection compilationMonitorLock(compInfo->getCompilationMonitor());
      if (!compInfo->getPersistentInfo()->getDisableFurtherCompilation())
         {
         // Setting the stream to SAVE_AOT_CACHE_REQUEST means that this is not a true
         // compilation request, but rather a request to save a cache to a file
         if (compInfo->addOutOfProcessMethodToBeCompiled(SAVE_AOTCACHE_REQUEST /*stream*/))
            {
            // Successfully queued the new entry, so notify a thread
            compInfo->getCompilationMonitor()->notifyAll();
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: t=%llu Queued comp request to save cache '%s' to file in the background",
                                              compInfo->getPersistentInfo()->getElapsedTime(), _name.c_str());
            queuedRequest = true;
            }
         }
      }
   if (!queuedRequest)
      {
      // We don't need to acquire the monitor here because only the thread that set
      // _saveOperationInProgress to true can reset it to false if a compilation request was not queued
      _saveOperationInProgress = false;
      }
   return queuedRequest;
   }

// This method is called at the end of an AOT cache save attempt to memorize the
// number of methods that were saved to file, and the time of the save operation.
// The flag _saveOperationInProgress is also reset
void
JITServerAOTCache::finalizeSaveOperation(bool success, size_t numMethodsSavedToFile)
   {
   OMR::CriticalSection cs(_cachedMethodMonitor);
   if (success)
      {
      _minNumAOTMethodsToSave = numMethodsSavedToFile + TR::Options::_aotCachePersistenceMinDeltaMethods;
      }
   // Overwite the time of the last save operation even if it failed
   // so that we don't try too often
   _timePrevSaveOperation = TR::CompilationInfo::get()->getPersistentInfo()->getElapsedTime();
   _saveOperationInProgress = false;
   }


bool
JITServerAOTCache::isAOTCacheBetterThanSnapshot(const std::string &cacheFileName, size_t numExtraMethods)
   {
   bool doSave = false;
   FILE *cacheFile = fopen(cacheFileName.c_str(), "rb");
   if (cacheFile)
      {
      // Read the header and extract the number of methods stored in the file
      JITServerAOTCacheHeader header = {0};
      if (1 == fread(&header, sizeof(JITServerAOTCacheHeader), 1, cacheFile))
         {
         // What do we do if the existing version is not compatible with ours?
         // Probably we should overwrite because ours is newer
         if (!isCompatibleSnapshotVersion(header._version))
            {
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: Found incompatible AOT cache file %s. Will overwrite.", cacheFileName.c_str());
            doSave = true;
            }
         else // Header is compatible, check the number of methods
            {
            if (getNumCachedMethods() >= header._numCachedAOTMethods + numExtraMethods)
               {
               // We have better data than the existing snaphot, so overwrite it
               doSave = true;
               }
            else // Existing snapshot has more methods (or same as us)
               {
               setMinNumAOTMethodsToSave(header._numCachedAOTMethods + TR::Options::_aotCachePersistenceMinDeltaMethods);
               if (TR::Options::getVerboseOption(TR_VerboseJITServer))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: Save operation aborted for cache '%s' because we don't have %zu more methods than existing snapshot: %zu vs %zu.",
                                                 name().c_str(), numExtraMethods, getNumCachedMethods(), header._numCachedAOTMethods);
               }
            }
         }
      else // Read error
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: Cannot read header from exiting cache file %s. Will overwrite existing file.", cacheFileName.c_str());
         // We should still proceed with the save operation, because the current cache is not usable
         doSave = true;
         }

      fclose(cacheFile);
      cacheFile = NULL;
      }
   else // Cannot open file
      {
      // If we cannot open the file for reading, the file might not exist
      // and this is the first attempt to save a cache. It's also possible
      // that the file exists, but we cannot read it. It's unlikely that
      // the file has write permissions, but not read permissions
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: Cannot open cache file %s for reading: %s", cacheFileName.c_str(), strerror(errno));
      doSave = true;
      }
   return doSave;
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
   _cachesBeingLoaded(decltype(_cachesBeingLoaded)::allocator_type(TR::Compiler->persistentGlobalAllocator())),
   _cachesToLoadQueue(decltype(_cachesToLoadQueue)::allocator_type(TR::Compiler->persistentGlobalAllocator())),
   _cachesExcludedFromLoading(decltype(_cachesExcludedFromLoading)::allocator_type(TR::Compiler->persistentGlobalAllocator())),
   _cachesToSaveQueue(decltype(_cachesToSaveQueue)::allocator_type(TR::Compiler->persistentGlobalAllocator())),
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


std::string
JITServerAOTCacheMap::buildCacheFileName(const std::string &cacheDir, const std::string &cacheName)
   {
   std::string cacheFileName;
   // If a directory is given, add it in the front of the cache name
   if (!cacheDir.empty())
      cacheFileName = cacheDir + "/";
   return cacheFileName + "JITServerAOTCache." + cacheName + ".J" + std::to_string(JAVA_SPEC_VERSION);
   }


void
JITServerAOTCacheMap::queueAOTCacheForSavingToFile(const std::string &cacheName)
   {
   OMR::CriticalSection cs(_monitor);
   _cachesToSaveQueue.push_back(cacheName);
   }


bool
JITServerAOTCacheMap::saveNextQueuedAOTCacheToFile()
   {
   std::string cacheName;
   JITServerAOTCache *cache = NULL;
      {
      OMR::CriticalSection cs(_monitor);

      // Extract the first entry from the queue
      // If there is nothing in the queue, then just return (another thread is working on it)
      if (_cachesToSaveQueue.empty())
         return false;
      cacheName = _cachesToSaveQueue.front();
      _cachesToSaveQueue.pop_front();
      // Retrieve the cache by name
      auto it = _map.find(cacheName);
      TR_ASSERT(it != _map.end(), "AOT Caches are never deleted in the current implementation");
      cache = it->second;
      }

   bool success = false;
   size_t numAOTMethodsWritten = 0;
   bool excludeCacheFromSaving = false;
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get();

   // Must catch any exceptions because we need to reset the _saveOperationInProgress flag
   // This is done in `finalizeSaveOperation()` which is called at the very end of this function
   try
      {
      std::string cacheFileName = buildCacheFileName(compInfo->getPersistentInfo()->getJITServerAOTCacheDir(), cacheName);

      // If a similarly named AOT cache file already exists, must determine if it's a better snapshot or not
      if (cache->isAOTCacheBetterThanSnapshot(cacheFileName, TR::Options::_aotCachePersistenceMinDeltaMethods))
         {
         PORT_ACCESS_FROM_JITCONFIG(compInfo->getJITConfig());
         OMRPORT_ACCESS_FROM_J9PORT(PORTLIB);
         uint64_t startTime = TR::Options::getVerboseOption(TR_VerboseJITServer) ? j9time_hires_clock() : 0;

         // Create a temporary file based on the UID of this server and the cache name
         std::string tempFileName = buildCacheFileName(compInfo->getPersistentInfo()->getJITServerAOTCacheDir(),
                                                       std::to_string(compInfo->getPersistentInfo()->getServerUID()) + "." + cacheName + ".tmp");
         FILE *newCacheFile = fopen(tempFileName.c_str(), "wb");
         if (newCacheFile)
            {
            if ((numAOTMethodsWritten = cache->writeCache(newCacheFile)) != 0)
               {
               fclose(newCacheFile);
               newCacheFile = NULL;

               // Before the rename operation, check again if our in-memory cache is still better than the existing snapshot
               if (cache->isAOTCacheBetterThanSnapshot(cacheFileName, 1))
                  {
                  // Rename the file to the final name
                  if (0 == rename(tempFileName.c_str(), cacheFileName.c_str()))
                     {
                     success = true;

                     if (TR::Options::getVerboseOption(TR_VerboseJITServer))
                        {
                        char timestamp[32];
                        uint64_t durationUsec = j9time_hires_delta(startTime, j9time_hires_clock(), J9PORT_TIME_DELTA_IN_MICROSECONDS);
                        omrstr_ftime_ex(timestamp, sizeof(timestamp), "%b-%d-%Y_%H:%M:%S ", j9time_current_time_millis(), OMRSTR_FTIME_FLAG_LOCAL);

                        TR_VerboseLog::CriticalSection vlogLock;
                        TR_VerboseLog::write(TR_Vlog_JITServer, "AOT cache: t=%llu Saved cache '%s' to file %s. %zu methods saved in %llu usec. Current time:",
                                             compInfo->getPersistentInfo()->getElapsedTime(), cacheName.c_str(), cacheFileName.c_str(), numAOTMethodsWritten, durationUsec);
                        TR_VerboseLog::writeLine(timestamp);
                        }
                     }
                  else // Renaming failed
                     {
                     if (TR::Options::getVerboseOption(TR_VerboseJITServer))
                        TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: Failed to rename temporary cache file %s to %s: %s",
                                                       tempFileName.c_str(), cacheFileName.c_str(), strerror(errno));
                     remove(tempFileName.c_str());
                     cache->excludeCacheFromSavingToFile();
                     }
                  }
               else // We changed our mind because some other JITServer instance has just saved a better snapshot than ours
                  {
                  if (TR::Options::getVerboseOption(TR_VerboseJITServer))
                     TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: Gave up renaming cache file %s to %s",
                                                    tempFileName.c_str(), cacheFileName.c_str());
                  remove(tempFileName.c_str());
                  }
               }
            else // Saving the AOT cache to file failed
               {
               fclose(newCacheFile);
               newCacheFile = NULL;
               if (TR::Options::getVerboseOption(TR_VerboseJITServer))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: Failed to serialize cache '%s' to file", cacheName.c_str());
               remove(tempFileName.c_str());
               cache->excludeCacheFromSavingToFile();
               }
            }
         else  // Cannot open file for writing; permissions?
            {
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: Cannot open new file %s for writing: %s", tempFileName.c_str(), strerror(errno));
            cache->excludeCacheFromSavingToFile();
            }
         }
      } // end try
   catch(const std::exception& e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: exception caught when trying to save cache '%s': %s", cacheName.c_str(), e.what());
         }
      }

   cache->finalizeSaveOperation(success, numAOTMethodsWritten);
   return success;
   }


void
JITServerAOTCacheMap::loadNextQueuedAOTCacheFromFile(J9::J9SegmentProvider &scratchSegmentProvider)
   {
   std::string cacheName;
      {
      OMR::CriticalSection cs(_monitor);
      TR_ASSERT(_cachesBeingLoaded.size() != 0, "There must be at least one AOT cache waiting to be loaded from file");

      // Extract the first entry from the queue
      // If there is nothing in the queue, then just return (another thread is working on it)
      if (_cachesToLoadQueue.empty())
         return;
      cacheName = _cachesToLoadQueue.front();
      _cachesToLoadQueue.pop_front();
      }

   JITServerAOTCache *cache = NULL;
   FILE *cacheFile = NULL;
   try
      {
      TR::CompilationInfo *compInfo = TR::CompilationInfo::get();
      std::string cacheFileName = buildCacheFileName(compInfo->getPersistentInfo()->getJITServerAOTCacheDir(), cacheName);

      // Open the AOT cache file and create a new JITServerAOTCache object
      FILE *cacheFile = fopen(cacheFileName.c_str(), "rb");
      if (cacheFile)
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: t=%llu Opened file %s to load cache '%s' from file",
                                           compInfo->getPersistentInfo()->getElapsedTime(), cacheFileName.c_str(), cacheName.c_str());
         size_t segmentSize = scratchSegmentProvider.getPreferredSegmentSize();
         if (!segmentSize)
            segmentSize = 1 << 24/*16 MB*/;
         TR::RawAllocator rawAllocator(compInfo->getJITConfig()->javaVM);
         J9::SystemSegmentProvider segmentProvider(1 << 16/*64 KB*/, segmentSize, TR::Options::getScratchSpaceLimit(), scratchSegmentProvider, rawAllocator);
         TR::Region region(segmentProvider, rawAllocator);
         TR_Memory trMemory(*compInfo->persistentMemory(), region);

         cache = JITServerAOTCache::readCache(cacheFile, cacheName, trMemory); // This should not throw
         fclose(cacheFile); // filestream not needed anymore
         cacheFile = NULL;

         if (cache)
            {
            // Update the number of AOT methods needed to be eligible for a save operation
            // No monitor is needed because no other thread knows about this cache yet
            cache->setMinNumAOTMethodsToSave(cache->getNumCachedMethods() + TR::Options::_aotCachePersistenceMinDeltaMethods);

            // My JITServerAOTCache was created and populated; now, insert it into the map
            OMR::CriticalSection cs(_monitor);
            _map.insert(std::make_pair(cacheName, cache));
            }
         else // Failed to create the AOT cache from file
            {
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: Failed to create cache '%s' from file", cacheName.c_str());
            }
         }
      else // Cannot open the AOT cache file
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: Failed to open cache file %s: %s", cacheFileName.c_str(), strerror(errno));
         }
      }
   catch(const std::exception& e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: exception caught when trying to read-in cache '%s': %s", cacheName.c_str(), e.what());
         }
      if (cacheFile)
         {
         fclose(cacheFile);
         cacheFile = NULL;
         }
      if (cache)
         {
         cache->~JITServerAOTCache();
         TR::Compiler->persistentGlobalMemory()->freePersistentMemory(cache);
         cache = NULL;
         // Do not rethrow the exception
         }
      }
   // Delete the entry from the set
   OMR::CriticalSection cs(_monitor);
   _cachesBeingLoaded.erase(cacheName);
   // If the cache loading operation failed, exclude the cache from future attempts
   if (!cache)
      _cachesExcludedFromLoading.insert(cacheName);
   }


JITServerAOTCache *
JITServerAOTCacheMap::get(const std::string &name, uint64_t clientUID, bool &pending)
   {
   OMR::CriticalSection cs(_monitor);

   auto it = _map.find(name);
   if (it != _map.end())
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: Using existing cache '%s' for clientUID %llu",
                                        name.c_str(), (unsigned long long)clientUID);
      return it->second;
      }

   if (!JITServerAOTCacheMap::cacheHasSpace())
      {
      return NULL;
      }

   // See if we can load the cache from file
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get();
   if (compInfo->getPersistentInfo()->getJITServerUseAOTCachePersistence() &&     // Persistence feature is enabled
       _cachesExcludedFromLoading.find(name) == _cachesExcludedFromLoading.end()) // I am allowed to load this cache from file
      {
      // Check that we don't have another request in progress for the same named cache
      if (_cachesBeingLoaded.find(name) != _cachesBeingLoaded.end())
         {
         // Another thread is already loading this cache
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: Another thread is already loading cache '%s' from file", name.c_str());
         pending = true;
         return NULL;
         }

      // Create special compilation request that will be used to deserialize the cache.
      // Note: we must ensure that, in all parts of the code, we acquire the compMonitor after the AOTCacheMap monitor
      OMR::CriticalSection compilationMonitorLock(compInfo->getCompilationMonitor());
      if (!compInfo->getPersistentInfo()->getDisableFurtherCompilation())
         {
         // Setting the stream to LOAD_AOT_CACHE_REQUEST means that this is not a true
         // compilation request, but rather a request to load a cache from file
         if (compInfo->addOutOfProcessMethodToBeCompiled(LOAD_AOTCACHE_REQUEST /*stream*/))
            {
            // Successfully queued the new entry, so notify a thread
            compInfo->getCompilationMonitor()->notifyAll();

            _cachesBeingLoaded.insert(name); // Prevent other threads from loading the same cache
            _cachesToLoadQueue.push_back(name);

            pending = true; // Tell the caller that we are loading the cache from file
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: t=%llu Queued comp request to load cache '%s' from file in the background",
                                                compInfo->getPersistentInfo()->getElapsedTime(), name.c_str());
            return NULL;
            }
         }
      }
   // If we reached this point, we need to create a new (empty) cache
   auto cache = new (TR::Compiler->persistentGlobalMemory()) JITServerAOTCache(name);
   if (!cache)
      throw std::bad_alloc();

   try
      {
      _map.insert(it, { name, cache });
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "AOT cache: Created empty cache '%s' for clientUID %llu",
                                        name.c_str(), (unsigned long long)clientUID);
      }
   catch (...)
      {
      cache->~JITServerAOTCache();
      TR::Compiler->persistentGlobalMemory()->freePersistentMemory(cache);
      throw;
      }

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
