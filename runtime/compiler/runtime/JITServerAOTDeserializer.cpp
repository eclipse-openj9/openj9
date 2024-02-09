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

#include "AtomicSupport.hpp"
#include "control/CompilationThread.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/JITServerHelpers.hpp"
#include "compile/Compilation.hpp"
#include "env/ClassLoaderTable.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/VerboseLog.hpp"
#include "infra/CriticalSection.hpp"
#include "runtime/JITServerAOTDeserializer.hpp"

#define RECORD_NAME(record) (int)(record)->nameLength(), (const char *)(record)->name()
#define LENGTH_AND_DATA(str) J9UTF8_LENGTH(str), (const char *)J9UTF8_DATA(str)
#define ROMCLASS_NAME(romClass) LENGTH_AND_DATA(J9ROMCLASS_CLASSNAME(romClass))
#define ROMMETHOD_NAS(romMethod) \
   LENGTH_AND_DATA(J9ROMMETHOD_NAME(romMethod)), LENGTH_AND_DATA(J9ROMMETHOD_SIGNATURE(romMethod))

JITServerAOTDeserializer::JITServerAOTDeserializer(TR_PersistentClassLoaderTable *loaderTable) :
   _loaderTable(loaderTable),
   _classLoaderMonitor(TR::Monitor::create("JIT-JITServerAOTDeserializerClassLoaderMonitor")),
   _classMonitor(TR::Monitor::create("JIT-JITServerAOTDeserializerClassMonitor")),
   _methodMonitor(TR::Monitor::create("JIT-JITServerAOTDeserializerMethodMonitor")),
   _classChainMonitor(TR::Monitor::create("JIT-JITServerAOTDeserializerClassChainMonitor")),
   _wellKnownClassesMonitor(TR::Monitor::create("JIT-JITServerAOTDeserializerWellKnownClassesMonitor")),
   _newKnownIds(decltype(_newKnownIds)::allocator_type(TR::Compiler->persistentAllocator())),
   _newKnownIdsMonitor(TR::Monitor::create("JIT-JITServerAOTDeserializerNewKnownIdsMonitor")),
   _resetMonitor(TR::Monitor::create("JIT-JITServerAOTDeserializerResetMonitor")),
   _numCacheBypasses(0), _numCacheHits(0), _numCacheMisses(0), _numDeserializedMethods(0),
   _numDeserializationFailures(0), _numClassSizeMismatches(0), _numClassHashMismatches(0)
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
JITServerAOTDeserializer::deserializerWasReset(TR::Compilation *comp, bool &wasReset)
   {
   return comp->fej9vm()->_compInfoPT->getDeserializerWasReset() ? (wasReset = true) : false;
   }

bool
JITServerAOTDeserializer::deserializationFailure(const SerializedAOTMethod *method,
                                                 TR::Compilation *comp, bool wasReset)
   {
   ++_numDeserializationFailures;

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
         "ERROR: Failed to deserialize AOT method %s%s",
         comp->signature(), wasReset ? " due to concurrent deserializer reset" : ""
      );
   return false;
   }

void
JITServerAOTDeserializer::reset(TR::CompilationInfoPerThread *compInfoPT)
   {
   // We acquire all the locks one by one according to the lock hierarchy so other compilation threads
   // can complete their deserializer operations, then be excluded from accessing the deserializer's caches
   // until the reset is complete.
   OMR::CriticalSection rcs(_resetMonitor);
   OMR::CriticalSection nkcs(_newKnownIdsMonitor);
   OMR::CriticalSection wkcs(_wellKnownClassesMonitor);
   OMR::CriticalSection cccs(_classChainMonitor);
   OMR::CriticalSection mcs(_methodMonitor);
   OMR::CriticalSection ccs(_classMonitor);

   // Notify each compilation thread that the deserializer was reset
   compInfoPT->getCompilationInfo()->notifyCompilationThreadsOfDeserializerReset();
   // This very thread is guaranteed not to be processing any AOT cache records from the old server,
   // so we can clear its deserializer reset flag.
   compInfoPT->clearDeserializerWasReset();

   clearCachedData();
   }

std::vector<uintptr_t>
JITServerAOTDeserializer::getNewKnownIds(TR::Compilation *comp)
   {
   OMR::CriticalSection cs(_newKnownIdsMonitor);
   bool wasReset = false;
   if (deserializerWasReset(comp, wasReset))
      return std::vector<uintptr_t>();

   std::vector<uintptr_t> result(_newKnownIds.begin(), _newKnownIds.end());
   _newKnownIds.clear();
   return result;
   }

bool
JITServerAOTDeserializer::deserialize(SerializedAOTMethod *method, const std::vector<std::string> &records,
                                      TR::Compilation *comp, bool &usesSVM)
   {
   TR_ASSERT((comp->j9VMThread()->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS) &&
             !comp->j9VMThread()->omrVMThread->exclusiveCount, "Must have shared VM access");
   ++_numCacheHits;

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
      OMR::CriticalSection cs(getNewKnownIdsMonitor());
      if (!deserializerWasReset(comp, wasReset))
         _newKnownIds.insert(newIds.begin(), newIds.end());
      }

   if (failed)
      return deserializationFailure(method, comp, wasReset);

   // Update SCC offsets in relocation data so that the method can be stored in the local SCC and AOT-loaded
   if (!updateSCCOffsets(method, comp, wasReset, usesSVM))
      return deserializationFailure(method, comp, wasReset);

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Deserialized AOT method %s", comp->signature());
   ++_numDeserializedMethods;
   return true;
   }

bool
JITServerAOTDeserializer::cacheRecord(const AOTSerializationRecord *record, TR::Compilation *comp,
                                      bool &isNew, bool &wasReset)
   {
   switch (record->type())
      {
      case ClassLoader:
         return cacheRecord((const ClassLoaderSerializationRecord *)record, comp, isNew, wasReset);
      case Class:
         return cacheRecord((const ClassSerializationRecord *)record, comp, isNew, wasReset);
      case Method:
         return cacheRecord((const MethodSerializationRecord *)record, comp, isNew, wasReset);
      case ClassChain:
         return cacheRecord((const ClassChainSerializationRecord *)record, comp, isNew, wasReset);
      case WellKnownClasses:
         return cacheRecord((const WellKnownClassesSerializationRecord *)record, comp, isNew, wasReset);
      case Thunk:
         return cacheRecord((const ThunkSerializationRecord *)record, comp, isNew, wasReset);
      default:
         TR_ASSERT_FATAL(false, "Invalid record type: %u", record->type());
         return false;
      }
   }

void
JITServerAOTDeserializer::printStats(FILE *f) const
   {
   fprintf(f,
      "JITServer AOT cache statistics:\n"
      "\tcache bypasses: %zu\n"
      "\tcache hits: %zu\n"
      "\tcache misses: %zu\n"
      "\tdeserialized methods: %zu\n"
      "\tdeserialization failures: %zu\n"
      "\tclass size mismatches: %zu\n"
      "\tclass hash mismatches: %zu\n",
      _numCacheBypasses,
      _numCacheHits,
      _numCacheMisses,
      _numDeserializedMethods,
      _numDeserializationFailures,
      _numClassSizeMismatches,
      _numClassHashMismatches
   );
   }

bool
JITServerAOTDeserializer::isClassMatching(const ClassSerializationRecord *record,
                                          J9Class *ramClass, TR::Compilation *comp)
   {
   TR::StackMemoryRegion stackMemoryRegion(*comp->trMemory());

   size_t packedSize;
   J9ROMClass *packedROMClass = JITServerHelpers::packROMClass(ramClass->romClass, comp->trMemory(), comp->fej9(),
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
      ++_numClassSizeMismatches;
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
      ++_numClassHashMismatches;
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

// Find a cached entry for the given ID in the map
template<typename V> V
JITServerAOTDeserializer::findInMap(const PersistentUnorderedMap<uintptr_t, V> &map, uintptr_t id, TR::Monitor *monitor, TR::Compilation *comp, bool &wasReset)
   {
   OMR::CriticalSection cs(monitor);
   if (deserializerWasReset(comp, wasReset))
      return V();

   auto it = map.find(id);
   if (it != map.end())
      return it->second;

   // This record ID can only be missing from the cache if it was removed by a concurrent reset
   // TODO: is this true any more? The deserializerWasReset above should guarantee that we haven't
   // reset by this point.
   wasReset = true;
   return V();
   }

// Invalidating classes and class loaders during GC can be done without locking since current thread
// has exclusive VM access, and compilation threads have shared VM access during deserialization.
static void
assertExclusiveVmAccess(J9VMThread *vmThread)
   {
   TR_ASSERT((vmThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS) && vmThread->omrVMThread->exclusiveCount,
             "Must have exclusive VM access");
   }

JITServerLocalSCCAOTDeserializer::JITServerLocalSCCAOTDeserializer(TR_PersistentClassLoaderTable *loaderTable) :
   JITServerAOTDeserializer(loaderTable),
   _sharedCache(loaderTable->getSharedCache()),
   _classLoaderIdMap(decltype(_classLoaderIdMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classLoaderPtrMap(decltype(_classLoaderPtrMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classIdMap(decltype(_classIdMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classPtrMap(decltype(_classPtrMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _methodMap(decltype(_methodMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classChainMap(decltype(_classChainMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _wellKnownClassesMap(decltype(_wellKnownClassesMap)::allocator_type(TR::Compiler->persistentAllocator()))
   { }

void
JITServerLocalSCCAOTDeserializer::clearCachedData()
   {
   _classLoaderIdMap.clear();
   _classLoaderPtrMap.clear();

   _classIdMap.clear();
   _classPtrMap.clear();

   _methodMap.clear();

   _classChainMap.clear();

   _wellKnownClassesMap.clear();

   getNewKnownIds().clear();
   }

void
JITServerLocalSCCAOTDeserializer::invalidateClassLoader(J9VMThread *vmThread, J9ClassLoader *loader)
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
JITServerLocalSCCAOTDeserializer::invalidateClass(J9VMThread *vmThread, J9Class *ramClass)
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

bool
JITServerLocalSCCAOTDeserializer::cacheRecord(const ClassLoaderSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset)
   {
   OMR::CriticalSection cs(getClassLoaderMonitor());
   if (deserializerWasReset(comp, wasReset))
      return false;

   auto it = _classLoaderIdMap.find(record->id());
   if (it != _classLoaderIdMap.end())
      return true;
   isNew = true;

   // Lookup the class loader using the name of the first class that it loaded
   auto result = getLoaderTable()->lookupClassLoaderAndChainAssociatedWithClassName(record->name(), record->nameLength());
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
   else if (!chain)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "ERROR: Found class loader but not chain for first loaded class %.*s", RECORD_NAME(record)
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
JITServerLocalSCCAOTDeserializer::cacheRecord(const ClassSerializationRecord *record, TR::Compilation *comp,
                                              bool &isNew, bool &wasReset)
   {
   OMR::CriticalSection cs(getClassMonitor());
   if (deserializerWasReset(comp, wasReset))
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
   J9ClassLoader *loader = getClassLoader(record->classLoaderId(), loaderOffset, comp, wasReset);
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
   if (!_sharedCache->isClassInSharedCache(ramClass, &offset))
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
JITServerLocalSCCAOTDeserializer::cacheRecord(const MethodSerializationRecord *record, TR::Compilation *comp,
                                              bool &isNew, bool &wasReset)
{
   OMR::CriticalSection cs(getMethodMonitor());
   if (deserializerWasReset(comp, wasReset))
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
JITServerLocalSCCAOTDeserializer::cacheRecord(const ClassChainSerializationRecord *record, TR::Compilation *comp,
                                              bool &isNew, bool &wasReset)
   {
   OMR::CriticalSection cs(getClassChainMonitor());
   if (deserializerWasReset(comp, wasReset))
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
   uintptr_t offset = _sharedCache->rememberClass((TR_OpaqueClassBlock *)ramClasses[0]);
   if (TR_SharedCache::INVALID_CLASS_CHAIN_OFFSET == offset)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "ERROR: Failed to get class chain ID %zu for class %.*s ID %zu",
            record->id(), ROMCLASS_NAME(ramClasses[0]->romClass), record->list().ids()[0]
         );
      return false;
      }

   uintptr_t *chain = (uintptr_t *)_sharedCache->pointerFromOffsetInSharedCache(offset);
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
JITServerLocalSCCAOTDeserializer::cacheRecord(const WellKnownClassesSerializationRecord *record,
                                              TR::Compilation *comp, bool &isNew, bool &wasReset)
   {
   OMR::CriticalSection cs(getWellKnownClassesMonitor());
   if (deserializerWasReset(comp, wasReset))
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
      chainOffsets[1 + i] = getSCCOffset(AOTSerializationRecordType::ClassChain, record->list().ids()[i], comp, wasReset);
      if (chainOffsets[1 + i] == (uintptr_t)-1)
         return false;
      }

   // Store the "well-known classes" object in the local SCC or find the existing one
   const void *wkcOffsets = _sharedCache->storeWellKnownClasses(comp->j9VMThread(), chainOffsets, 1 + record->list().length(), record->includedClasses());
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


bool
JITServerLocalSCCAOTDeserializer::cacheRecord(const ThunkSerializationRecord *record,
                                              TR::Compilation *comp, bool &isNew, bool &wasReset)
   {
   // Unlike the rest of the cacheRecord functions, we do not need to acquire a monitor here, as we can rely on
   // the internal synchronization of getJ2IThunk and setJ2IThunk. We use a read barrier here for deserializerWasReset().
   VM_AtomicSupport::readBarrier();
   if (deserializerWasReset(comp, wasReset))
      return false;

   auto fej9vm = comp->fej9vm();
   void *thunk = fej9vm->getJ2IThunk((char *)record->signature(), record->signatureSize(), comp);
   if (thunk)
      return true;
   isNew = true;

   fej9vm->setJ2IThunk((char *)record->signature(), record->signatureSize(), record->thunkAddress(), comp);

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Cached thunk record ID %zu -> for thunk %.*s",
                                     record->id(), record->signatureSize(), record->signature());
   return true;
   }


J9ClassLoader *
JITServerLocalSCCAOTDeserializer::getClassLoader(uintptr_t id, uintptr_t &loaderSCCOffset, TR::Compilation *comp, bool &wasReset)
   {
   OMR::CriticalSection cs(getClassLoaderMonitor());
   if (deserializerWasReset(comp, wasReset))
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
   auto loader = (J9ClassLoader *)getLoaderTable()->lookupClassLoaderAssociatedWithClassChain(chain);
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
JITServerLocalSCCAOTDeserializer::getRAMClass(uintptr_t id, TR::Compilation *comp, bool &wasReset)
   {
   OMR::CriticalSection cs(getClassMonitor());
   if (deserializerWasReset(comp, wasReset))
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
   auto loader = (J9ClassLoader *)getLoaderTable()->lookupClassLoaderAssociatedWithClassChain(chain);
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

uintptr_t
JITServerLocalSCCAOTDeserializer::getSCCOffset(AOTSerializationRecordType type, uintptr_t id, TR::Compilation *comp, bool &wasReset)
   {
   switch (type)
      {
      case ClassLoader:
         {
         uintptr_t offset = findInMap(_classLoaderIdMap, id, getClassLoaderMonitor(), comp, wasReset)._loaderChainSCCOffset;
         return wasReset ? (uintptr_t)-1 : offset;
         }
      case Class:
         {
         uintptr_t offset = findInMap(_classIdMap, id, getClassMonitor(), comp, wasReset)._romClassSCCOffset;
         // Check if this cached ID is for a valid class
         if ((offset == (uintptr_t)-1) && TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "ERROR: Mismatching class ID %zu", id);
         return wasReset ? (uintptr_t)-1 : offset;
         }
      case Method:
         {
         uintptr_t offset = findInMap(_methodMap, id, getMethodMonitor(), comp, wasReset);
         return wasReset ? (uintptr_t)-1 : offset;
         }
      case ClassChain:
         {
         uintptr_t offset = findInMap(_classChainMap, id, getClassChainMonitor(), comp, wasReset);
         return wasReset ? (uintptr_t)-1 : offset;
         }
      case WellKnownClasses:
         {
         uintptr_t offset = findInMap(_wellKnownClassesMap, id, getWellKnownClassesMonitor(), comp, wasReset);
         return wasReset ? (uintptr_t)-1 : offset;
         }
      default:
         TR_ASSERT_FATAL(false, "Invalid record type: %u", type);
         return (uintptr_t)-1;
      }
   }

bool
JITServerLocalSCCAOTDeserializer::updateSCCOffsets(SerializedAOTMethod *method, TR::Compilation *comp,
                                                   bool &wasReset, bool &usesSVM)
   {
   //NOTE: Defining class chain record is validated by now; there is no corresponding SCC offset to be updated

   auto header = (const TR_AOTMethodHeader *)(method->data() + sizeof(J9JITDataCacheHeader));
   TR_ASSERT_FATAL((header->minorVersion == TR_AOTMethodHeader_MinorVersion) &&
                   (header->majorVersion == TR_AOTMethodHeader_MajorVersion),
                   "Invalid TR_AOTMethodHeader version: %d.%d", header->majorVersion, header->minorVersion);
   TR_ASSERT_FATAL((header->offsetToRelocationDataItems != 0) || (method->numRecords() == 0),
                   "Unexpected %zu serialization records in serialized method %s with no relocation data",
                   method->numRecords(), comp->signature());
   usesSVM = (header->flags & TR_AOTMethodHeader_UsesSymbolValidationManager) != 0;

   uint8_t *start = method->data() + header->offsetToRelocationDataItems;
   uint8_t *end = start + *(uintptr_t *)start;// Total size of relocation data is stored in the first word

   for (size_t i = 0; i < method->numRecords(); ++i)
      {
      // Get the SCC offset of the corresponding entity in the local SCC
      const SerializedSCCOffset &serializedOffset = method->offsets()[i];

      // Thunks do not use their SCC offset entries
      if (serializedOffset.recordType() == AOTSerializationRecordType::Thunk)
         continue;

      uintptr_t sccOffset = getSCCOffset(serializedOffset.recordType(), serializedOffset.recordId(), comp, wasReset);
      if (sccOffset == (uintptr_t)-1)
         return false;

      // Update the SCC offset stored in AOT method relocation data
      uint8_t *ptr = start + serializedOffset.reloDataOffset();
      TR_ASSERT_FATAL((ptr >= start + sizeof(uintptr_t)/*skip the size word*/) && (ptr < end),
                      "Out-of-bounds relocation data offset %zu in serialized method %s",
                      serializedOffset.reloDataOffset(), comp->signature());
#if defined(DEBUG)
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "Updating SCC offset %zu -> %zu for record type %u ID %zu at relo data offset %zu in serialized method %s",
            *(uintptr_t *)ptr, sccOffset, serializedOffset.recordType(), serializedOffset.recordId(),
            serializedOffset.reloDataOffset(), comp->signature()
         );
#endif /* defined(DEBUG) */
      *(uintptr_t *)ptr = sccOffset;
      }

   return true;
   }

JITServerNoSCCAOTDeserializer::JITServerNoSCCAOTDeserializer(TR_PersistentClassLoaderTable *loaderTable) :
   JITServerAOTDeserializer(loaderTable),
   _classLoaderIdMap(decltype(_classLoaderIdMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classLoaderPtrMap(decltype(_classLoaderPtrMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classIdMap(decltype(_classIdMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classPtrMap(decltype(_classPtrMap)::allocator_type(TR::Compiler->persistentAllocator()))
   { }


void
JITServerNoSCCAOTDeserializer::invalidateClassLoader(J9VMThread *vmThread, J9ClassLoader *loader)
   {
   assertExclusiveVmAccess(vmThread);

   auto p_it = _classLoaderPtrMap.find(loader);
   if (p_it == _classLoaderPtrMap.end())// Not cached
      return;

   uintptr_t id = p_it->second;
   auto i_it = _classLoaderIdMap.find(id);
   TR_ASSERT(i_it != _classLoaderIdMap.end(), "Broken two-way map");

   // Mark entry as unloaded
   i_it->second = NULL;
   _classLoaderPtrMap.erase(p_it);

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Invalidated class loader %p ID %zu", loader, id);
   }

void
JITServerNoSCCAOTDeserializer::invalidateClass(J9VMThread *vmThread, J9Class *ramClass)
   {
   assertExclusiveVmAccess(vmThread);

   auto p_it = _classPtrMap.find(ramClass);
   if (p_it == _classPtrMap.end()) // Not cached or already marked invalid
      return;

   uintptr_t id = p_it->second;
   auto i_it = _classIdMap.find(id);
   TR_ASSERT(i_it != _classIdMap.end(), "Broken two-way map");

   if (i_it->second._ramClass)
      {
      // Class ID is valid. Keep the entry, but mark it as unloaded.
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
JITServerNoSCCAOTDeserializer::clearCachedData()
   {
   _classLoaderIdMap.clear();
   _classLoaderPtrMap.clear();

   _classIdMap.clear();
   _classPtrMap.clear();

   getNewKnownIds().clear();
   }

bool
JITServerNoSCCAOTDeserializer::cacheRecord(const ClassLoaderSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset)
   {
   OMR::CriticalSection cs(getClassLoaderMonitor());
   if (deserializerWasReset(comp, wasReset))
      return false;

   auto it = _classLoaderIdMap.find(record->id());
   if (it != _classLoaderIdMap.end())
      return true;
   isNew = true;

   // Lookup the class loader using the name of the first class that it loaded
   auto loader = (J9ClassLoader *)getLoaderTable()->lookupClassLoaderAssociatedWithClassName(record->name(), record->nameLength());
   if (!loader)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "ERROR: Failed to find class loader for first loaded class %.*s", RECORD_NAME(record)
         );
      return false;
      }

   addToMaps(_classLoaderIdMap, _classLoaderPtrMap, it, record->id(), loader, loader);

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
         "Cached class loader record ID %zu -> { %p } for first loaded class %.*s",
         record->id(), loader, RECORD_NAME(record)
      );
   return true;
   }

bool
JITServerNoSCCAOTDeserializer::cacheRecord(const ClassSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset)
   {
   OMR::CriticalSection cs(getClassMonitor());
   if (deserializerWasReset(comp, wasReset))
      return false;

   auto it = _classIdMap.find(record->id());
   if (it != _classIdMap.end())
      {
      if (it->second._ramClass)
         {
         return true;
         }
      else
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "ERROR: Mismatching class ID %zu", record->id());
         return false;
         }
      }
   isNew = true;

   // The class loader for this class record should already have been deserialized, so if we can't find a
   // loader for this ID then it must have been marked as unloaded. We don't support loader reloading, so
   // we simply fail to deserialize here.
   auto loader = findInMap(_classLoaderIdMap, record->classLoaderId(), getClassLoaderMonitor(), comp, wasReset);
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

   // Check that the ROMClass hash matches, otherwise remember that it doesn't
   if (!isClassMatching(record, ramClass, comp))
      {
      _classIdMap.insert(it, { record->id(), { NULL, record->classLoaderId() } });
      return false;
      }

   addToMaps(_classIdMap, _classPtrMap, it, record->id(), { ramClass, record->classLoaderId() }, ramClass);

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
         "Cached class record ID %zu -> { %p, %zu } for class %.*s",
         record->id(), ramClass, record->classLoaderId(), RECORD_NAME(record)
      );
   return true;
   }

bool
JITServerNoSCCAOTDeserializer::cacheRecord(const MethodSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset)
   {
   return false;
   }

bool
JITServerNoSCCAOTDeserializer::cacheRecord(const ClassChainSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset)
   {
   return false;
   }

bool
JITServerNoSCCAOTDeserializer::cacheRecord(const WellKnownClassesSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset)
   {
   return false;
   }

bool
JITServerNoSCCAOTDeserializer::cacheRecord(const ThunkSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset)
   {
   return false;
   }

bool
JITServerNoSCCAOTDeserializer::updateSCCOffsets(SerializedAOTMethod *method, TR::Compilation *comp, bool &wasReset, bool &usesSVM)
   {
   return false;
   }