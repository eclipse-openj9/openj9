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

#include "control/JITServerHelpers.hpp"
#include "compile/Compilation.hpp"
#include "env/ClassLoaderTable.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/VerboseLog.hpp"
#include "infra/CriticalSection.hpp"
#include "runtime/JITServerAOTDeserializer.hpp"


JITServerAOTDeserializer::JITServerAOTDeserializer(TR_PersistentClassLoaderTable *loaderTable) :
   _loaderTable(loaderTable), _sharedCache(NULL),
   _classLoaderIdMap(decltype(_classLoaderIdMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classLoaderPtrMap(decltype(_classLoaderPtrMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classLoaderMonitor(TR::Monitor::create("JIT-JITServerAOTDeserializerClassLoaderMonitor")),
   _classIdMap(decltype(_classIdMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classPtrMap(decltype(_classPtrMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classMonitor(TR::Monitor::create("JIT-JITServerAOTDeserializerClassMonitor")),
   _methodMap(decltype(_methodMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _methodMonitor(TR::Monitor::create("JIT-JITServerAOTDeserializerMethodMonitor")),
   _classChainMap(decltype(_classChainMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classChainMonitor(TR::Monitor::create("JIT-JITServerAOTDeserializerClassChainMonitor")),
   _wellKnownClassesMap(decltype(_wellKnownClassesMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _wellKnownClassesMonitor(TR::Monitor::create("JIT-JITServerAOTDeserializerWellKnownClassesMonitor")),
   _newKnownIds(decltype(_newKnownIds)::allocator_type(TR::Compiler->persistentAllocator())),
   _newKnownIdsMonitor(TR::Monitor::create("JIT-JITServerAOTDeserializerNewKnownIdsMonitor")),
   _resetInProgress(false),
   _resetMonitor(TR::Monitor::create("JIT-JITServerAOTDeserializerResetMonitor"))
   {
   bool allMonitors = _classLoaderMonitor && _classMonitor && _methodMonitor &&
                      _classChainMonitor && _wellKnownClassesMonitor && _resetMonitor;
   if (!allMonitors)
       throw std::bad_alloc();
   }

JITServerAOTDeserializer::~JITServerAOTDeserializer()
   {
   TR::Monitor::destroy(_classLoaderMonitor);
   TR::Monitor::destroy(_classMonitor);
   TR::Monitor::destroy(_methodMonitor);
   TR::Monitor::destroy(_classChainMonitor);
   TR::Monitor::destroy(_wellKnownClassesMonitor);
   TR::Monitor::destroy(_newKnownIdsMonitor);
   TR::Monitor::destroy(_resetMonitor);
   }


bool
JITServerAOTDeserializer::deserialize(SerializedAOTMethod *method, const std::vector<std::string> &records,
                                      TR::Compilation *comp)
   {
   TR_ASSERT((comp->j9VMThread()->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS) &&
             !comp->j9VMThread()->omrVMThread->exclusiveCount, "Must have shared VM access");

   TR::StackMemoryRegion stackMemoryRegion(*comp->trMemory());
   Vector<uintptr_t> newIds(Vector<uintptr_t>::allocator_type(comp->trMemory()->currentStackRegion()));
   newIds.reserve(records.size());
   bool wasReset = false;
   bool failed = false;

   // Deserialize/validate and cache serialization records, keeping track of IDs of the new ones.
   // Since the records are sorted in "dependency order", by the time a given record is about
   // to be cached, all the records that it depends on are already successfully cached.
   for (size_t i = 0; i < records.size(); ++i)
      {
      auto record = AOTSerializationRecord::get(records[i]);
      bool isNew = false;

      if (!cacheRecord(record, comp, isNew, wasReset))
         {
         failed = true;
         break;
         }
      if (isNew)
         newIds.push_back(AOTSerializationRecord::idAndType(record->id(), record->type()));
      }

   // Remember IDs of newly cached records to be sent to the server with the next
   // compilation request, unless caching a record failed because of a concurrent reset.
   // If we encountered an invalid record (i.e. adding it failed, but not because of a
   // concurrent reset), remember IDs of new records that were successfully cached so far.
   if (!wasReset)
      {
      OMR::CriticalSection cs(_newKnownIdsMonitor);
      // Check again that a reset operation has not started. Note that we need to read
      // _resetInProgress after acquiring the monitor (it implies the required memory barrier).
      if (!_resetInProgress)
         _newKnownIds.insert(newIds.begin(), newIds.end());
      }

   if (failed)
      return deserializationFailure(method, comp, wasReset);

   // Update SCC offsets in relocation data so that the method can be stored in the local SCC and AOT-loaded
   if (!updateSCCOffsets(method, comp, wasReset))
      return deserializationFailure(method, comp, wasReset);

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Deserialized AOT method %s", comp->signature());
   return true;
   }


// Invalidating classes and class loaders during GC can be done without locking since current thread
// has exclusive VM access, and compilation threads have shared VM access during deserialization.
static void
assertExclusiveVmAccess(J9VMThread *vmThread)
   {
   TR_ASSERT((vmThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS) && vmThread->omrVMThread->exclusiveCount,
             "Must have exclusive VM access");
   }

void
JITServerAOTDeserializer::invalidateClassLoader(J9VMThread *vmThread, J9ClassLoader *loader)
   {
   assertExclusiveVmAccess(vmThread);

   auto p_it = _classLoaderPtrMap.find(loader);
   if (p_it == _classLoaderPtrMap.end())// Not cached
      return;

   uintptr_t id = p_it->second;
   auto i_it = _classLoaderIdMap.find(id);
   TR_ASSERT(i_it != _classLoaderIdMap.end(), "Broken two-way map");

   // Mark entry as unloaded, but keep the (still valid) SCC offset
   i_it->second._loader = NULL;
   _classLoaderPtrMap.erase(p_it);

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Invalidated class loader %p ID %zu", loader, id);
   }

void
JITServerAOTDeserializer::invalidateClass(J9VMThread *vmThread, J9Class *ramClass)
   {
   assertExclusiveVmAccess(vmThread);

   auto p_it = _classPtrMap.find(ramClass);
   if (p_it == _classPtrMap.end())// Not cached
      return;

   uintptr_t id = p_it->second;
   auto i_it = _classIdMap.find(id);
   TR_ASSERT(i_it != _classIdMap.end(), "Broken two-way map");

   if (i_it->second._ramClass)
      {
      // Class ID is valid. Keep the entry. Mark it as unloaded, but keep the (still valid) SCC offsets.
      i_it->second._ramClass = NULL;
      }
   else
      {
      // Class ID is invalid. Remove the entry so that it can be replaced
      // if a matching version of the class is ever loaded in the future.
      _classIdMap.erase(i_it);
      }

   _classPtrMap.erase(p_it);
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Invalidated RAMClass %p ID %zu", ramClass, id);
   }


void
JITServerAOTDeserializer::reset()
   {
   // Only allow one thread at a time, so that other threads currently deserializing
   // methods can use a simple boolean flag to reliably detect a concurrent reset
   // (a second thread running reset() would possibly clear the flag too early).
   OMR::CriticalSection cs(_resetMonitor);
   _resetInProgress = true;

      {
      OMR::CriticalSection cs(_classLoaderMonitor);
      _classLoaderIdMap.clear();
      _classLoaderPtrMap.clear();
      }

      {
      OMR::CriticalSection cs(_classMonitor);
      _classIdMap.clear();
      _classPtrMap.clear();
      }

      {
      OMR::CriticalSection cs(_methodMonitor);
      _methodMap.clear();
      }

      {
      OMR::CriticalSection cs(_classChainMonitor);
      _classChainMap.clear();
      }

      {
      OMR::CriticalSection cs(_wellKnownClassesMonitor);
      _wellKnownClassesMap.clear();
      }

      {
      OMR::CriticalSection cs(_newKnownIdsMonitor);
      _newKnownIds.clear();
      }

   _resetInProgress = false;
   }


std::vector<uintptr_t>
JITServerAOTDeserializer::getNewKnownIds()
   {
   OMR::CriticalSection cs(_newKnownIdsMonitor);
   if (_resetInProgress)
      return std::vector<uintptr_t>();

   std::vector<uintptr_t> result(_newKnownIds.begin(), _newKnownIds.end());
   _newKnownIds.clear();
   return result;
   }


bool
JITServerAOTDeserializer::cacheRecord(const AOTSerializationRecord *record, TR::Compilation *comp,
                                      bool &isNew, bool &wasReset)
   {
   switch (record->type())
      {
      case ClassLoader:
         return cacheRecord((const ClassLoaderSerializationRecord *)record, isNew, wasReset);
      case Class:
         return cacheRecord((const ClassSerializationRecord *)record, comp, isNew, wasReset);
      case Method:
         return cacheRecord((const MethodSerializationRecord *)record, comp, isNew, wasReset);
      case ClassChain:
         return cacheRecord((const ClassChainSerializationRecord *)record, comp, isNew, wasReset);
      case WellKnownClasses:
         return cacheRecord((const WellKnownClassesSerializationRecord *)record, comp, isNew, wasReset);
      default:
         TR_ASSERT_FATAL(false, "Invalid record type: %u", record->type());
         return false;
      }
   }


#define RECORD_NAME(record) (int)(record)->nameLength(), (const char *)(record)->name()
#define LENGTH_AND_DATA(str) J9UTF8_LENGTH(str), (const char *)J9UTF8_DATA(str)
#define ROMCLASS_NAME(romClass) LENGTH_AND_DATA(J9ROMCLASS_CLASSNAME(romClass))
#define ROMMETHOD_NAS(romMethod) \
   LENGTH_AND_DATA(J9ROMMETHOD_NAME(romMethod)), LENGTH_AND_DATA(J9ROMMETHOD_SIGNATURE(romMethod))


bool
JITServerAOTDeserializer::isClassMatching(const ClassSerializationRecord *record,
                                          J9Class *ramClass, TR::Compilation *comp)
   {
   TR::StackMemoryRegion stackMemoryRegion(*comp->trMemory());

   size_t packedSize;
   J9ROMClass *packedROMClass = JITServerHelpers::packROMClass(ramClass->romClass, comp->trMemory(),
                                                               packedSize, record->romClassSize());
   if (!packedROMClass)
      {
      // Packed ROMClass size mismatch. Fail early (no need to compute the hash).
      TR_ASSERT(packedSize != record->romClassSize(), "packROMClass() returned NULL but expected size matches");
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "ERROR: ROMClass size mismatch for class %.*s ID %zu: %zu != %u",
            RECORD_NAME(record), record->id(), packedSize, record->romClassSize()
         );
      return false;
      }

   JITServerROMClassHash hash(packedROMClass);
   if (hash != record->hash())
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         {
         char actual[ROMCLASS_HASH_BYTES * 2 + 1];
         char expected[ROMCLASS_HASH_BYTES * 2 + 1];
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "ERROR: ROMClass hash mismatch for class %.*s ID %zu: %s != %s",
            RECORD_NAME(record), record->id(), hash.toString(actual, sizeof(actual)),
            record->hash().toString(expected, sizeof(expected))
         );
         }
      return false;
      }

   return true;
}


// Atomically (w.r.t. exceptions) insert into two maps. id is the key in map0 and the value in map1.
template<typename V0, typename K1> static void
addToMaps(PersistentUnorderedMap<uintptr_t, V0> &map0,
          PersistentUnorderedMap<K1, uintptr_t> &map1,
          typename PersistentUnorderedMap<uintptr_t, V0>::const_iterator it,
          uintptr_t id, const V0 &value0, const K1 &key1)
   {
   it = map0.insert(it, { id, value0 });
   try
      {
      map1.insert({ key1, id });
      }
   catch (...)
      {
      map0.erase(it);
      throw;
      }
   }


bool
JITServerAOTDeserializer::cacheRecord(const ClassLoaderSerializationRecord *record, bool &isNew, bool &wasReset)
   {
   OMR::CriticalSection cs(_classLoaderMonitor);
   if (isResetInProgress(wasReset))
      return false;

   auto it = _classLoaderIdMap.find(record->id());
   if (it != _classLoaderIdMap.end())
      return true;
   isNew = true;

   // Lookup the class loader using the name of the first class that it loaded
   auto result = _loaderTable->lookupClassLoaderAndChainAssociatedWithClassName(record->name(), record->nameLength());
   auto loader = (J9ClassLoader *)result.first;
   void *chain = result.second;
   if (!loader)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "ERROR: Failed to find class loader for first loaded class %.*s", RECORD_NAME(record)
         );
      return false;
      }

   uintptr_t offset = _sharedCache->offsetInSharedCacheFromPointer(chain);
   addToMaps(_classLoaderIdMap, _classLoaderPtrMap, it, record->id(), { loader, offset }, loader);

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
         "Cached class loader record ID %zu -> { %p, %zu } for first loaded class %.*s",
         record->id(), loader, offset, RECORD_NAME(record)
      );
   return true;
   }

bool
JITServerAOTDeserializer::cacheRecord(const ClassSerializationRecord *record, TR::Compilation *comp,
                                      bool &isNew, bool &wasReset)
   {
   OMR::CriticalSection cs(_classMonitor);
   if (isResetInProgress(wasReset))
      return false;

   auto it = _classIdMap.find(record->id());
   if (it != _classIdMap.end())
      {
      if (it->second._romClassSCCOffset != (uintptr_t)-1)
         return true;
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "ERROR: Mismatching class ID %zu", record->id());
      return false;
      }
   isNew = true;

   // Get the class loader for this class loader ID (which should already be cached)
   uintptr_t loaderOffset = (uintptr_t)-1;
   J9ClassLoader *loader = getClassLoader(record->classLoaderId(), loaderOffset, wasReset);
   if (!loader)
      return false;

   // Lookup the RAMClass by name in the class loader
   J9Class *ramClass = jitGetClassInClassloaderFromUTF8(comp->j9VMThread(), loader, (char *)record->name(),
                                                        record->nameLength());
   if (!ramClass)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "ERROR: Failed to find class %.*s ID %zu in class loader %p",
            RECORD_NAME(record), record->id(), loader
         );
      return false;
      }

   // Check that the ROMClass is in the SCC and get its SCC offset
   uintptr_t offset = (uintptr_t)-1;
   if (!_sharedCache->isROMClassInSharedCache(ramClass->romClass, &offset))
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "ERROR: ROMClass %p %.*s ID %zu is not in SCC",
                                        ramClass->romClass, RECORD_NAME(record), record->id());
      return false;
      }

   // Check that the ROMClass hash matches, otherwise remember that it doesn't
   if (!isClassMatching(record, ramClass, comp))
      {
      addToMaps(_classIdMap, _classPtrMap, it, record->id(), { ramClass, (uintptr_t)-1, (uintptr_t)-1 }, ramClass);
      return false;
      }

   addToMaps(_classIdMap, _classPtrMap, it, record->id(), { ramClass, offset, loaderOffset }, ramClass);

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
         "Cached class record ID %zu -> { %p, %zu, %zu } for class %.*s",
         record->id(), ramClass, offset, loaderOffset, RECORD_NAME(record)
      );
   return true;
   }

bool
JITServerAOTDeserializer::cacheRecord(const MethodSerializationRecord *record, TR::Compilation *comp,
                                      bool &isNew, bool &wasReset)
{
   OMR::CriticalSection cs(_methodMonitor);
   if (isResetInProgress(wasReset))
      return false;

   auto it = _methodMap.find(record->id());
   if (it != _methodMap.end())
      return true;
   isNew = true;

   // Get defining RAMClass using its ID (which should already be cached)
   J9Class *ramClass = getRAMClass(record->definingClassId(), comp, wasReset);
   if (!ramClass)
      return false;

   // Get RAMMethod (and its ROMMethod) in defining RAMClass by index
   TR_ASSERT(record->index() < ramClass->romClass->romMethodCount, "Invalid method index");
   J9Method *ramMethod = &ramClass->ramMethods[record->index()];
   J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);

   uintptr_t offset = _sharedCache->offsetInSharedCacheFromROMMethod(romMethod);
   _methodMap.insert(it, { record->id(), offset });

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
         "Cached method record ID %zu -> { %p, %zu } for method %.*s.%.*s%.*s",
         record->id(), ramMethod, offset, ROMCLASS_NAME(ramClass->romClass), ROMMETHOD_NAS(romMethod)
      );
   return true;
   }

bool
JITServerAOTDeserializer::cacheRecord(const ClassChainSerializationRecord *record, TR::Compilation *comp,
                                      bool &isNew, bool &wasReset)
   {
   OMR::CriticalSection cs(_classChainMonitor);
   if (isResetInProgress(wasReset))
      return false;

   auto it = _classChainMap.find(record->id());
   if (it != _classChainMap.end())
      return true;
   isNew = true;

   // Get the RAMClasses for each class ID in the chain (which should already be cached)
   TR_ASSERT(record->list().length() <= TR_J9SharedCache::maxClassChainLength, "Class chain is too long");
   J9Class *ramClasses[TR_J9SharedCache::maxClassChainLength];
   for (size_t i = 0; i < record->list().length(); ++i)
      {
      ramClasses[i] = getRAMClass(record->list().ids()[i], comp, wasReset);
      if (!ramClasses[i])
         return false;
      }

   // Create the class chain in the local SCC or find the existing one
   uintptr_t *chain = _sharedCache->rememberClass((TR_OpaqueClassBlock *)ramClasses[0]);
   if (!chain)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "ERROR: Failed to get class chain ID %zu for class %.*s ID %zu",
            record->id(), ROMCLASS_NAME(ramClasses[0]->romClass), record->list().ids()[0]
         );
      return false;
      }

   uintptr_t offset = (uintptr_t)-1;
   if (!_sharedCache->isPointerInSharedCache(chain, &offset))
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "ERROR: Failed to get SCC offset for class chain %p ID %zu for class %.*s ID %zu",
            chain, record->id(), ROMCLASS_NAME(ramClasses[0]->romClass), record->list().ids()[0]
         );
      return false;
      }
   uintptr_t chainLength = chain[0] / sizeof(chain[0]) - 1;
   uintptr_t *chainItems = chain + 1;

   // Check that the local version of the class chain has the expected length
   if (record->list().length() != chainLength)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "ERROR: Class chain length mismatch for class %.*s ID %zu: %zu != %zu",
            ROMCLASS_NAME(ramClasses[0]->romClass), record->list().ids()[0], record->list().length(), chainLength
         );
      return false;
      }

   // Validate each ROMClass in the chain
   for (size_t i = 0; i < chainLength; ++i)
      {
      J9ROMClass *romClass = _sharedCache->romClassFromOffsetInSharedCache(chainItems[i]);
      if (romClass != ramClasses[i]->romClass)
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
               "ERROR: Class %.*s mismatch in class chain ID %zu for class %.*s ID %zu",
               ROMCLASS_NAME(ramClasses[i]->romClass), record->id(),
               ROMCLASS_NAME(ramClasses[0]->romClass), record->list().ids()[0]
            );
         return false;
         }
      }

   _classChainMap.insert(it, { record->id(), offset });

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
         "Cached class chain record ID %zu -> { %p, %zu } for class %.*s ID %zu",
         record->id(), ramClasses[0], offset, ROMCLASS_NAME(ramClasses[0]->romClass), record->list().ids()[0]
      );
   return true;
   }

bool
JITServerAOTDeserializer::cacheRecord(const WellKnownClassesSerializationRecord *record,
                                      TR::Compilation *comp, bool &isNew, bool &wasReset)
   {
   OMR::CriticalSection cs(_wellKnownClassesMonitor);
   if (isResetInProgress(wasReset))
      return false;

   auto it = _wellKnownClassesMap.find(record->id());
   if (it != _wellKnownClassesMap.end())
      return true;
   isNew = true;

   // Initialize the "well-known classes" object (see SymbolValidationManager::populateWellKnownClasses()).
   TR_ASSERT(record->list().length() <= WELL_KNOWN_CLASS_COUNT, "Too many well-known classes");
   uintptr_t chainOffsets[1 + WELL_KNOWN_CLASS_COUNT] = { record->list().length() };
   // Get the class chain SCC offsets for each class chain ID (which should already be cached).
   for (size_t i = 0; i < record->list().length(); ++i)
      {
      chainOffsets[1 + i] = getSCCOffset(AOTSerializationRecordType::ClassChain, record->list().ids()[i], wasReset);
      if (chainOffsets[1 + i] == (uintptr_t)-1)
         return false;
      }

   // Get the key that it will be stored under in the local SCC
   char key[128];
   TR::SymbolValidationManager::getWellKnownClassesSCCKey(key, sizeof(key), record->includedClasses());
   J9SharedDataDescriptor desc = { .address = (uint8_t *)chainOffsets,
                                   .length = (1 + record->list().length()) * sizeof(chainOffsets[0]),
                                   .type = J9SHR_DATA_TYPE_JITHINT };

   // Store the "well-known classes" object in the local SCC or find the existing one
   const void *wkcOffsets = _sharedCache->storeSharedData(comp->j9VMThread(), key, &desc);
   if (!wkcOffsets)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "ERROR: Failed to get well-known classes ID %zu",
                                        record->id());
      return false;
      }

   uintptr_t offset = (uintptr_t)-1;
   if (!_sharedCache->isPointerInSharedCache((void *)wkcOffsets, &offset))
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "ERROR: Failed to get SCC offset for well-known classes %p ID %zu", wkcOffsets, record->id()
         );
      return false;
      }
   _wellKnownClassesMap.insert(it, { record->id(), offset });

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Cached well-known classes record ID %zu -> %zu",
                                     record->id(), offset);
   return true;
   }


J9ClassLoader *
JITServerAOTDeserializer::getClassLoader(uintptr_t id, uintptr_t &loaderSCCOffset, bool &wasReset)
   {
   OMR::CriticalSection cs(_classLoaderMonitor);
   if (isResetInProgress(wasReset))
      return NULL;

   auto it = _classLoaderIdMap.find(id);
   if (it == _classLoaderIdMap.end())
      {
      // Class loader ID should be cached by now, unless it was removed by a concurrent reset
      wasReset = true;
      return NULL;
      }

   // Check if the cached entry has a valid class loader pointer
   if (it->second._loader)
      {
      loaderSCCOffset = it->second._loaderChainSCCOffset;
      return it->second._loader;
      }

   // Class loader was unloaded. Try to lookup a new version using the identifying class chain SCC offset.
   void *chain = _sharedCache->pointerFromOffsetInSharedCache(it->second._loaderChainSCCOffset);
   auto loader = (J9ClassLoader *)_loaderTable->lookupClassLoaderAssociatedWithClassChain(chain);
   if (!loader)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "ERROR: Failed to get class loader ID %zu for identifying class chain %p", id, chain
         );
      return NULL;
      }

   // Put the new class loader pointer back into the maps
   _classLoaderPtrMap.insert({ loader, id });
   it->second._loader = loader;

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Re-cached class loader ID %zu -> { %p, %zu }",
                                     id, loader, it->second._loaderChainSCCOffset);
   loaderSCCOffset = it->second._loaderChainSCCOffset;
   return loader;
   }

J9Class *
JITServerAOTDeserializer::getRAMClass(uintptr_t id, TR::Compilation *comp, bool &wasReset)
   {
   OMR::CriticalSection cs(_classMonitor);
   if (isResetInProgress(wasReset))
      return NULL;

   auto it = _classIdMap.find(id);
   if (it == _classIdMap.end())
      {
      // Class ID should be cached by now, unless it was removed by a concurrent reset
      wasReset = true;
      return NULL;
      }

   // Check if the cached entry has a valid RAMClass pointer
   if (it->second._ramClass)
      {
      if (it->second._romClassSCCOffset == (uintptr_t)-1)
         {
         // This entry is for a previously cached mismatching class ID
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "ERROR: Mismatching class ID %zu", id);
         return NULL;
         }
      return it->second._ramClass;
      }

   // Class was unloaded. Try to lookup a new version of its class loader using the identifying class chain SCC offset.
   void *chain = _sharedCache->pointerFromOffsetInSharedCache(it->second._loaderChainSCCOffset);
   auto loader = (J9ClassLoader *)_loaderTable->lookupClassLoaderAssociatedWithClassChain(chain);
   if (!loader)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "ERROR: Failed to get class loader for identifying class chain %p", chain
         );
      return NULL;
      }

   // Get the class name using the cached ROMClass SCC offset, which is guaranteed to be valid at this point.
   // Note that romClassFromOffsetInSharedCache() already has a fatal assertion that the offset is valid.
   J9ROMClass *romClass = _sharedCache->romClassFromOffsetInSharedCache(it->second._romClassSCCOffset);
   const J9UTF8 *name = J9ROMCLASS_CLASSNAME(romClass);

   // Try to lookup a new version of the class in its class loader by name
   J9Class *ramClass = jitGetClassInClassloaderFromUTF8(comp->j9VMThread(), loader, (char *)J9UTF8_DATA(name),
                                                        J9UTF8_LENGTH(name));
   if (!ramClass)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "ERROR: Failed to find class %.*s ID %zu in class loader %p", ROMCLASS_NAME(romClass), id, loader
         );
      return NULL;
      }

   // Check that the ROMClass is still the same
   if (ramClass->romClass != romClass)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "ERROR: ROMClass mismatch for %.*s ID %zu",
                                        ROMCLASS_NAME(romClass), id);
      return NULL;
      }

   // Put the new RAMClass pointer back into the maps
   _classPtrMap.insert({ ramClass, id });
   it->second._ramClass = ramClass;

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Re-cached class ID %zu -> { %p, %zu, %zu }",
                                     id, ramClass, it->second._romClassSCCOffset, it->second._loaderChainSCCOffset);
   return ramClass;
   }


// Find a cached entry for the given ID in the map
template<typename V>
V findInMap(const PersistentUnorderedMap<uintptr_t, V> &map, uintptr_t id, TR::Monitor *monitor, bool &wasReset)
   {
   OMR::CriticalSection cs(monitor);

   auto it = map.find(id);
   if (it != map.end())
      return it->second;

   // This record ID can only be missing from the cache if it was removed by a concurrent reset
   wasReset = true;
   return V();
   }

uintptr_t
JITServerAOTDeserializer::getSCCOffset(AOTSerializationRecordType type, uintptr_t id, bool &wasReset)
   {
   switch (type)
      {
      case ClassLoader:
         {
         uintptr_t offset = findInMap(_classLoaderIdMap, id, _classLoaderMonitor, wasReset)._loaderChainSCCOffset;
         return wasReset ? (uintptr_t)-1 : offset;
         }
      case Class:
         {
         uintptr_t offset = findInMap(_classIdMap, id, _classMonitor, wasReset)._romClassSCCOffset;
         // Check if this cached ID is for a valid class
         if ((offset == (uintptr_t)-1) && TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "ERROR: Mismatching class ID %zu", id);
         return wasReset ? (uintptr_t)-1 : offset;
         }
      case Method:
         {
         uintptr_t offset = findInMap(_methodMap, id, _methodMonitor, wasReset);
         return wasReset ? (uintptr_t)-1 : offset;
         }
      case ClassChain:
         {
         uintptr_t offset = findInMap(_classChainMap, id, _classChainMonitor, wasReset);
         return wasReset ? (uintptr_t)-1 : offset;
         }
      case WellKnownClasses:
         {
         uintptr_t offset = findInMap(_wellKnownClassesMap, id, _wellKnownClassesMonitor, wasReset);
         return wasReset ? (uintptr_t)-1 : offset;
         }
      default:
         TR_ASSERT_FATAL(false, "Invalid record type: %u", type);
         return (uintptr_t)-1;
      }
   }


bool
JITServerAOTDeserializer::deserializationFailure(const SerializedAOTMethod *method,
                                                 TR::Compilation *comp, bool wasReset)
   {
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
         "ERROR: Failed to deserialize AOT method %s%s",
         comp->signature(), wasReset ? " due to concurrent deserializer reset" : ""
      );
   return false;
   }

bool
JITServerAOTDeserializer::updateSCCOffsets(SerializedAOTMethod *method, TR::Compilation *comp, bool &wasReset)
   {
   //NOTE: Defining class chain record is validated by now

   auto header = (TR_AOTMethodHeader *)(method->data() + sizeof(J9JITDataCacheHeader));
   if (header->offsetToRelocationDataItems == 0)
      {
      TR_ASSERT_FATAL(method->numRecords() == 0, "Unexpected serialization records in serialized method %s",
                      comp->signature());
      return true;
      }

   for (size_t i = 0; i < method->numRecords(); ++i)
      {
      // Get the SCC offset of the corresponding entity in the local SCC
      const SerializedSCCOffset &serializedOffset = method->offsets()[i];
      uintptr_t sccOffset = getSCCOffset(serializedOffset.recordType(), serializedOffset.recordId(), wasReset);
      if (sccOffset == (uintptr_t)-1)
         return false;

      // Update the SCC offset stored in AOT method relocation data
      uint8_t *ptr = method->data() + header->offsetToRelocationDataItems + serializedOffset.reloDataOffset();
      TR_ASSERT_FATAL(ptr < method->data() + method->dataSize(),
                      "Out-of-bounds relocation data offset in serialized method %s", comp->signature());
      *(uintptr_t *)ptr = sccOffset;
      }

   return true;
   }
