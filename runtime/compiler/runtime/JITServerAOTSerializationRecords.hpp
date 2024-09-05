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

#ifndef JITSERVER_AOT_SERIALIZATION_RECORDS_H
#define JITSERVER_AOT_SERIALIZATION_RECORDS_H

#include "OMR/Bytes.hpp"
#include "runtime/JITServerROMClassHash.hpp"
#include "runtime/RelocationRuntime.hpp"

struct JITServerAOTCacheReadContext;


enum AOTSerializationRecordType
   {
   // Used for class loader identification (by name of the first loaded class).
   // Associated with an SCC class chain identifying a class loader (class chain of its first loaded class).
   ClassLoader,
   // Associated with a ROMClass
   Class,
   // Associated with a ROMMethod
   Method,
   // Associated with an SCC class chain
   ClassChain,
   // Associated with an SCC "well-known classes" object
   WellKnownClasses,
   // Associated with a thunk
   Thunk,
   // Not associated with an SCC entity; corresponds to TR_AOTHeader struct used for checking AOT code compatibility
   AOTHeader,

   AOTSerializationRecordType_MAX
   };


// Base class for serialization records used in serialized AOT methods.
// These records are sent to clients which use them for deserialization.
// Each record is contiguous, and variable-sized for most record types.
// Records refer to other records that they depend on via unique record IDs,
// e.g. a class chain record contains a list of class record IDs.
struct AOTSerializationRecord
   {
public:
   // Disable copying since instances can be variable-sized
   AOTSerializationRecord(const AOTSerializationRecord &) = delete;
   void operator=(const AOTSerializationRecord &) = delete;

   size_t size() const { return _size; }
   //NOTE: 0 signifies an invalid record ID
   uintptr_t id() const { return getId(_idAndType); }
   AOTSerializationRecordType type() const { return getType(_idAndType); }
   uintptr_t idAndType() const { return _idAndType; }
   const uint8_t *end() const { return (const uint8_t *)this + size(); }

   static const AOTSerializationRecord *get(const std::string &str)
      {
      auto record = (const AOTSerializationRecord *)str.data();
      TR_ASSERT((str.size() >= sizeof(*record)) && (str.size() == record->_size), "Invalid size");
      return record;
      }

   // Record ID and type are stored in compact way in a single pointer-sized word
   static uintptr_t idAndType(uintptr_t id, AOTSerializationRecordType type)
      {
      TR_ASSERT(id, "ID 0 is invalid");
      TR_ASSERT(id <= (UINTPTR_MAX >> idShift), "ID overflow: %zu", id);
      return (id << idShift) | (uintptr_t)type;
      }

   static uintptr_t getId(uintptr_t idAndType)
      {
      return idAndType >> idShift;
      }

   static AOTSerializationRecordType getType(uintptr_t idAndType)
      {
      return (AOTSerializationRecordType)(idAndType & typeMask);
      }

protected:
   AOTSerializationRecord(size_t size, uintptr_t id, AOTSerializationRecordType type) :
      _size(size), _idAndType(idAndType(id, type)) { }

   // Check the well-formedness of the statically-sized portion of a serialization record.
   // The variable-sized portion can't be checked here, only in setSubrecordPointers, as it
   // is read based on the information in the statically-sized portion.
   bool isValidHeader(AOTSerializationRecordType type) const;

private:
   static const uintptr_t idShift = 3;
   static const uintptr_t typeMask = (1 << idShift) - 1;
   static_assert(AOTSerializationRecordType_MAX <= typeMask + 1, "Too many types");

   const size_t _size;
   const uintptr_t _idAndType;
   };


struct ClassLoaderSerializationRecord : public AOTSerializationRecord
   {
public:
   size_t nameLength() const { return _nameLength; }
   const uint8_t *name() const { return _name; }

private:
   friend class AOTCacheRecord;
   friend class AOTCacheClassLoaderRecord;

   ClassLoaderSerializationRecord(uintptr_t id, const uint8_t *name, size_t nameLength);
   ClassLoaderSerializationRecord();

   static size_t size(size_t nameLength)
      {
      return sizeof(ClassLoaderSerializationRecord) + OMR::alignNoCheck(nameLength, sizeof(size_t));
      }

   bool isValidHeader(const JITServerAOTCacheReadContext &context) const
      { return AOTSerializationRecord::isValidHeader(AOTSerializationRecordType::ClassLoader); }

   // Name of the 1st class loaded by the class loader
   const size_t _nameLength;
   uint8_t _name[];
   };


struct ClassSerializationRecord : public AOTSerializationRecord
   {
public:
   uintptr_t classLoaderId() const { return _classLoaderId; }
   const JITServerROMClassHash &hash() const { return _hash; }
   uint32_t romClassSize() const { return _romClassSize & ~AOTCACHE_CLASS_RECORD_GENERATED; }
   bool isGenerated() const { return _romClassSize & AOTCACHE_CLASS_RECORD_GENERATED; }
   uint32_t nameLength() const { return _nameLength; }
   const uint8_t *name() const { return _name; }

private:
   friend class AOTCacheRecord;
   friend class AOTCacheClassRecord;

   static const uint32_t AOTCACHE_CLASS_RECORD_GENERATED = 1;

   // The `generated` flag refers to runtime-generated classes such as lambdas
   ClassSerializationRecord(uintptr_t id, uintptr_t classLoaderId, const JITServerROMClassHash &hash,
                            uint32_t romClassSize, bool generated, const J9ROMClass *romClass,
                            const J9ROMClass *baseComponent, uint32_t numDimensions, uint32_t nameLength);
   ClassSerializationRecord();

   static size_t size(uint32_t nameLength)
      {
      return sizeof(ClassSerializationRecord) + OMR::alignNoCheck(nameLength, sizeof(size_t));
      }

   bool isValidHeader(const JITServerAOTCacheReadContext &context) const;

   const uintptr_t _classLoaderId;
   const JITServerROMClassHash _hash;
   // Used to quickly detect class mismatches (without computing the hash) when ROMClass size is different
   const uint32_t _romClassSize; // isGenerated flag is encoded in LSB
   // Class name string
   const uint32_t _nameLength;
   uint8_t _name[];
   };


struct MethodSerializationRecord : public AOTSerializationRecord
   {
public:
   uintptr_t definingClassId() const { return _definingClassId; }
   uint32_t index() const { return _index; }

private:
   friend class AOTCacheRecord;
   friend class AOTCacheMethodRecord;

   MethodSerializationRecord(uintptr_t id, uintptr_t definingClassId, uint32_t index);
   MethodSerializationRecord();

   bool isValidHeader(const JITServerAOTCacheReadContext &context) const;

   const uintptr_t _definingClassId;
   // Index in the array of methods of the defining class
   const uint32_t _index;
   };


struct IdList
   {
public:
   IdList(size_t length) : _length(length) { }

   size_t length() const { return _length; }
   const uintptr_t *ids() const { return _ids; }
   uintptr_t *ids() { return _ids; }

   static size_t size(size_t length) { return sizeof(IdList) + length * sizeof(uintptr_t); }

private:
   const size_t _length;
   uintptr_t _ids[];
   };


struct ClassChainSerializationRecord : public AOTSerializationRecord
   {
public:
   const IdList &list() const { return _list; }

private:
   friend class AOTCacheRecord;
   friend class AOTCacheClassChainRecord;

   ClassChainSerializationRecord(uintptr_t id, size_t length);
   ClassChainSerializationRecord();

   IdList &list() { return _list; }

   static size_t size(size_t length)
      {
      return offsetof(ClassChainSerializationRecord, _list) + IdList::size(length);
      }

   bool isValidHeader(const JITServerAOTCacheReadContext &context) const
      { return AOTSerializationRecord::isValidHeader(AOTSerializationRecordType::ClassChain); }

   // List of class IDs
   IdList _list;
   };


struct WellKnownClassesSerializationRecord : public AOTSerializationRecord
   {
public:
   uintptr_t includedClasses() const { return _includedClasses; }
   const IdList &list() const { return _list; }

private:
   friend class AOTCacheRecord;
   friend class AOTCacheWellKnownClassesRecord;

   WellKnownClassesSerializationRecord(uintptr_t id, size_t length, uintptr_t includedClasses);
   WellKnownClassesSerializationRecord();

   IdList &list() { return _list; }

   static size_t size(size_t length)
      {
      return offsetof(WellKnownClassesSerializationRecord, _list) + IdList::size(length);
      }

   bool isValidHeader(const JITServerAOTCacheReadContext &context) const
      { return AOTSerializationRecord::isValidHeader(AOTSerializationRecordType::WellKnownClasses); }

   // Bit mask representing which classes out of the predefined well-known set are included
   const uintptr_t _includedClasses;
   // List of class chain IDs
   IdList _list;
   };


struct AOTHeaderSerializationRecord : public AOTSerializationRecord
   {
public:
   const TR_AOTHeader *header() const { return &_header; }

private:
   friend class AOTCacheRecord;
   friend class AOTCacheAOTHeaderRecord;

   AOTHeaderSerializationRecord(uintptr_t id, const TR_AOTHeader *header);
   AOTHeaderSerializationRecord();

   bool isValidHeader(const JITServerAOTCacheReadContext &context) const
      { return AOTSerializationRecord::isValidHeader(AOTSerializationRecordType::AOTHeader); }

   const TR_AOTHeader _header;
   };


struct ThunkSerializationRecord : public AOTSerializationRecord
{
public:
   uint32_t signatureSize() const { return _signatureSize; }
   uint32_t thunkSize() const { return _thunkSize; }
   const uint8_t *signature() const { return _varSizedData; }
   const uint8_t *thunkStart() const { return _varSizedData + _signatureSize; }
   void *thunkAddress() const { return (void *)(thunkStart() + 8); }

private:
   friend class AOTCacheRecord;
   friend class AOTCacheThunkRecord;

   ThunkSerializationRecord(uintptr_t id, const uint8_t *signature, uint32_t signatureSize, const uint8_t *thunkStart,  uint32_t thunkSize);
   ThunkSerializationRecord();

   static size_t size(uint32_t signatureSize, uint32_t thunkSize)
      {
      return sizeof(ThunkSerializationRecord) + OMR::alignNoCheck(signatureSize + thunkSize, sizeof(size_t));
      }

   bool isValidHeader(const JITServerAOTCacheReadContext &context) const
      { return AOTSerializationRecord::isValidHeader(AOTSerializationRecordType::Thunk); }

   const uint32_t _signatureSize;
   const uint32_t _thunkSize;
   // Layout: uint8_t _signature[_signatureSize], uint8_t _thunkStart[_thunkSize]
   uint8_t _varSizedData[];
};


// Represents an SCC offset stored in AOT method relocation data that will be updated during deserialization
struct SerializedSCCOffset
   {
public:
   SerializedSCCOffset(uintptr_t recordId, AOTSerializationRecordType recordType, uintptr_t reloDataOffset) :
      _recordIdAndType(AOTSerializationRecord::idAndType(recordId, recordType)), _reloDataOffset(reloDataOffset)
      {
      TR_ASSERT(recordType < AOTSerializationRecordType::AOTHeader, "Invalid record type: %u", recordType);
      }

   uintptr_t recordId() const { return AOTSerializationRecord::getId(_recordIdAndType); }
   AOTSerializationRecordType recordType() const { return AOTSerializationRecord::getType(_recordIdAndType); }
   uintptr_t reloDataOffset() const { return _reloDataOffset; }

private:
   // ID and type of the corresponding serialization record
   const uintptr_t _recordIdAndType;
   // Offset into AOT method relocation data where the SCC offset to be updated is stored
   const uintptr_t _reloDataOffset;
   };


struct SerializedAOTMethod
   {
public:
   SerializedAOTMethod(const SerializedAOTMethod &) = delete;
   void operator=(const SerializedAOTMethod &) = delete;

   size_t size() const { return _size; }
   uintptr_t definingClassChainId() const { return _definingClassChainId; }
   uint32_t index() const { return _index; }
   TR_Hotness optLevel() const { return _optLevel; }
   uintptr_t aotHeaderId() const { return _aotHeaderId; }
   size_t numRecords() const { return _numRecords; }
   size_t codeSize() const { return _codeSize; }
   size_t dataSize() const { return _dataSize; }
   size_t signatureSize() const { return _signatureSize; }
   const SerializedSCCOffset *offsets() const { return (const SerializedSCCOffset *)_varSizedData; }
   SerializedSCCOffset *offsets() { return (SerializedSCCOffset *)_varSizedData; }
   const uint8_t *code() const { return (const uint8_t *)(offsets() + _numRecords); }
   const uint8_t *data() const { return code() + _codeSize; }
   uint8_t *data() { return (uint8_t *)(code() + _codeSize); }

   const char *signature() const { return (char *)(data() + _dataSize); }

   const uint8_t *end() const { return (const uint8_t *)this + size(); }

   static SerializedAOTMethod *get(std::string &str)
      {
      auto method = (SerializedAOTMethod *)str.data();
      TR_ASSERT((str.size() >= sizeof(*method)) && (str.size() == method->_size), "Invalid size");
      return method;
      }

private:
   friend class AOTCacheRecord;
   friend class CachedAOTMethod;

   SerializedAOTMethod(uintptr_t definingClassChainId, uint32_t index,
                       TR_Hotness optLevel, uintptr_t aotHeaderId, size_t numRecords,
                       const void *code, size_t codeSize,
                       const void *data, size_t dataSize,
                       const char *signature, size_t signatureSize);
   SerializedAOTMethod();

   static size_t size(size_t numRecords, size_t codeSize, size_t dataSize, size_t signatureSize)
      {
      return sizeof(SerializedAOTMethod) + numRecords * sizeof(SerializedSCCOffset) +
             OMR::alignNoCheck(codeSize + dataSize + signatureSize, sizeof(size_t));
      }

   bool isValidHeader(const JITServerAOTCacheReadContext &context) const;

   const size_t _size;
   const uintptr_t _definingClassChainId;
   // Index in the array of methods of the defining class
   const uint32_t _index;
   const TR_Hotness _optLevel;
   // Represents the TR_AOTHeader of the client JVM that this method was originally compiled for
   const uintptr_t _aotHeaderId;
   // Number of serialization records and corresponding SCC offsets
   const size_t _numRecords;
   const size_t _codeSize;
   const size_t _dataSize;

   const size_t _signatureSize;
   // Layout: SerializedSCCOffset offsets[_numRecords]
   //         uint8_t             code[_codeSize]
   //         uint8_t             data[_dataSize]
   //         char*               signature[_signatureSize]
   uint8_t _varSizedData[];
   };


#endif /* defined(JITSERVER_AOT_SERIALIZATION_RECORDS_H) */
