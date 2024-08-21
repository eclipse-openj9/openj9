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

#include "jitprotos.h"
#include "AtomicSupport.hpp"
#include "control/CompilationThread.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/JITServerHelpers.hpp"
#include "compile/Compilation.hpp"
#include "env/ClassLoaderTable.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/VerboseLog.hpp"
#include "infra/CriticalSection.hpp"
#include "runtime/CodeCacheExceptions.hpp"
#include "runtime/JITServerAOTDeserializer.hpp"
#include "runtime/RelocationTarget.hpp"

#define RECORD_NAME(record) (int)(record)->nameLength(), (const char *)(record)->name()
#define LENGTH_AND_DATA(str) J9UTF8_LENGTH(str), (const char *)J9UTF8_DATA(str)
#define ROMCLASS_NAME(romClass) LENGTH_AND_DATA(J9ROMCLASS_CLASSNAME(romClass))
#define ROMMETHOD_NAS(romMethod) \
   LENGTH_AND_DATA(J9ROMMETHOD_NAME(romMethod)), LENGTH_AND_DATA(J9ROMMETHOD_SIGNATURE(romMethod))


JITServerAOTDeserializer::JITServerAOTDeserializer(TR_PersistentClassLoaderTable *loaderTable, J9JITConfig *jitConfig) :
   _loaderTable(loaderTable),
   _rawAllocator(jitConfig->javaVM),
   _segmentAllocator(MEMORY_TYPE_JIT_SCRATCH_SPACE | MEMORY_TYPE_VIRTUAL, *jitConfig->javaVM),
   _segmentProvider(64 * 1024, classLoadScratchMemoryLimit, classLoadScratchMemoryLimit, _segmentAllocator, _rawAllocator),
   _classLoadRegion(_segmentProvider, _rawAllocator),
   _classLoadTRMemory(*(TR_PersistentMemory *)jitConfig->scratchSegment, _classLoadRegion),
   _classLoaderMonitor(TR::Monitor::create("JIT-JITServerAOTDeserializerClassLoaderMonitor")),
   _classMonitor(TR::Monitor::create("JIT-JITServerAOTDeserializerClassMonitor")),
   _methodMonitor(TR::Monitor::create("JIT-JITServerAOTDeserializerMethodMonitor")),
   _classChainMonitor(TR::Monitor::create("JIT-JITServerAOTDeserializerClassChainMonitor")),
   _wellKnownClassesMonitor(TR::Monitor::create("JIT-JITServerAOTDeserializerWellKnownClassesMonitor")),
   _newKnownIds(decltype(_newKnownIds)::allocator_type(TR::Compiler->persistentAllocator())),
   _newKnownIdsMonitor(TR::Monitor::create("JIT-JITServerAOTDeserializerNewKnownIdsMonitor")),
   _resetMonitor(TR::Monitor::create("JIT-JITServerAOTDeserializerResetMonitor")),
   _jitConfig(jitConfig),
   _generatedClasses(decltype(_generatedClasses)::allocator_type(TR::Compiler->persistentAllocator())),
   _generatedClassesMonitor(TR::Monitor::create("JIT-JITServerAOTDeserializerGeneratedClassesMonitor")),
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


TR_Memory &
JITServerAOTDeserializer::classLoadTRMemory()
   {
   TR_ASSERT(TR::MonitorTable::get()->getClassTableMutex()->owned_by_self(), "Must hold classTableMutex");
   return _classLoadTRMemory;
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
   OMR::CriticalSection clcs(_classLoaderMonitor);

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


// Invalidating classes and class loaders during GC can be done without locking since current thread
// has exclusive VM access, and compilation threads have shared VM access during deserialization.
static void
assertExclusiveVmAccess(J9VMThread *vmThread)
   {
   TR_ASSERT((vmThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS) && vmThread->omrVMThread->exclusiveCount,
             "Must have exclusive VM access");
   }

static void
assertSharedVmAccess(J9VMThread *vmThread)
   {
   TR_ASSERT((vmThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS) && !vmThread->omrVMThread->exclusiveCount,
             "Must have shared VM access");
   }


void
JITServerAOTDeserializer::onClassLoad(J9Class *ramClass, J9VMThread *vmThread)
   {
   assertSharedVmAccess(vmThread);
   TR_ASSERT(TR::MonitorTable::get()->getClassTableMutex()->owned_by_self(), "Must hold classTableMutex");

   const J9UTF8 *name = J9ROMCLASS_CLASSNAME(ramClass->romClass);
   const uint8_t *nameStr = J9UTF8_DATA(name);
   int nameLen = J9UTF8_LENGTH(name);

   // Class load JIT hook doesn't expect this function to throw exceptions
   try
      {
      if (auto prefixLength = JITServerHelpers::getGeneratedClassNamePrefixLength(name))
         {
         // Add this runtime-generated RAMClass to the map so that it can be found when deserializing AOT methods
         OMR::CriticalSection cs(_generatedClassesMonitor);

         // Find the entry for this class loader and deterministic class name prefix
         auto it = _generatedClasses.find({ ramClass->classLoader, StringKey(nameStr, prefixLength) });
         if (it == _generatedClasses.end())
            {
            // Allocate a persistent copy of the name prefix for the new GeneratedClassMap instance
            auto namePrefix = (uint8_t *)jitPersistentAlloc(prefixLength);
            if (!namePrefix)
               throw std::bad_alloc();
            memcpy(namePrefix, nameStr, prefixLength);

            try
               {
               // Add a new GeneratedClassMap instance for this class loader and name prefix
               it = _generatedClasses.emplace_hint(it, std::piecewise_construct,
                  std::forward_as_tuple(ramClass->classLoader, StringKey(namePrefix, prefixLength)),
                  std::forward_as_tuple(namePrefix)
               );
               }
            catch (...)
               {
               jitPersistentFree(namePrefix);
               throw;
               }
            }

         try
            {
            // Compute the deterministic ROMClass hash for this class
            JITServerROMClassHash hash(ramClass->romClass, classLoadTRMemory(), TR_J9VMBase::get(_jitConfig, vmThread), true);
            // Add a new entry to the GeneratedClassMap for this RAMClass
            auto h_r = it->second._classHashMap.insert({ hash, ramClass });
            if (h_r.second)
               {
               try
                  {
                  // Establish two-way mapping from the RAMClass so that it can be invalidated at class unload
                  auto p_r = it->second._classPtrMap.insert({ ramClass, hash });
                  TR_ASSERT_FATAL(p_r.second, "Duplicate generated class %p", ramClass);

                  if (TR::Options::getVerboseOption(TR_VerboseJITServer))
                     {
                     char buffer[ROMCLASS_HASH_BYTES * 2 + 1];
                     TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                        "Loaded generated class %.*s %p ROMClass %p hash %s", ROMCLASS_NAME(ramClass->romClass),
                        ramClass, ramClass->romClass, hash.toString(buffer, sizeof(buffer))
                     );
                     }
                  }
               catch (...)
                  {
                  it->second._classHashMap.erase(h_r.first);
                  throw;
                  }
               }
            else if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               {
               char buffer[ROMCLASS_HASH_BYTES * 2 + 1];
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                  "ERROR: Duplicate generated class %.*s %p ROMClass %p hash %s", ROMCLASS_NAME((ramClass)->romClass),
                  ramClass, ramClass->romClass, hash.toString(buffer, sizeof(buffer))
               );
               }
            }
         catch (...)
            {
            TR_ASSERT_FATAL(it->second._classHashMap.size() == it->second._classPtrMap.size(),
                            "Broken two-way map for generated class %p", ramClass);
            if (it->second._classHashMap.empty())
               _generatedClasses.erase(it);
            throw;
            }
         }
      }

   catch (const std::exception &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "ERROR: AOT deserializer failed to handle class load for %.*s %p: %s", nameLen, nameStr, ramClass, e.what()
         );
      }
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


J9Class *
JITServerAOTDeserializer::findGeneratedClass(J9ClassLoader *loader, const uint8_t *namePrefix, size_t namePrefixLength,
                                             const JITServerROMClassHash &hash, J9VMThread *vmThread)
   {
   assertSharedVmAccess(vmThread);
   OMR::CriticalSection cs(_generatedClassesMonitor);

   auto n_it = _generatedClasses.find({ loader, StringKey(namePrefix, namePrefixLength) });
   if (n_it == _generatedClasses.end())
      return NULL;

   auto h_it = n_it->second._classHashMap.find(hash);
   return (h_it != n_it->second._classHashMap.end()) ? h_it->second : NULL;
   }

std::string
JITServerAOTDeserializer::findGeneratedClassHash(J9ClassLoader *loader, J9Class *ramClass, TR_J9VM *fe, J9VMThread *vmThread)
   {
   assertSharedVmAccess(vmThread);

   size_t namePrefixLength = JITServerHelpers::getGeneratedClassNamePrefixLength(ramClass->romClass);
   if (namePrefixLength == 0)
      return std::string();
   const uint8_t *name = J9UTF8_DATA(J9ROMCLASS_CLASSNAME(ramClass->romClass));

   OMR::CriticalSection cs(_generatedClassesMonitor);

   auto n_it = _generatedClasses.find({ loader, StringKey(name, namePrefixLength) });
   if (n_it == _generatedClasses.end())
      return std::string();

   auto h_it = n_it->second._classPtrMap.find(ramClass);
   if (h_it == n_it->second._classPtrMap.end())
      return std::string();
   return std::string((const char *)&h_it->second, sizeof(h_it->second));
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

   if (TR::Options::isAnyVerboseOptionSet(TR_VerboseJITServer, TR_VerbosePerformance))
      TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "AOT deserializer class load mem=[region=%llu system=%llu]KB",
                                     (unsigned long long)_segmentProvider.regionBytesAllocated() / 1024,
                                     (unsigned long long)_segmentProvider.systemBytesAllocated() / 1024);
   }


JITServerAOTDeserializer::GeneratedClassMap::GeneratedClassMap(uint8_t *namePrefix) :
   _namePrefix(namePrefix),
   _classHashMap(decltype(_classHashMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classPtrMap(decltype(_classPtrMap)::allocator_type(TR::Compiler->persistentAllocator()))
   {
   }

JITServerAOTDeserializer::GeneratedClassMap::~GeneratedClassMap()
   {
   jitPersistentFree(_namePrefix);
   }


bool
JITServerAOTDeserializer::isClassMatching(const ClassSerializationRecord *record,
                                          J9Class *ramClass, TR::Compilation *comp)
   {
   int32_t numDimensions = 0;
   // Use base (non-SCC) front-end method to avoid needless validations
   auto baseComponent = (J9Class *)TR_J9VMBase::staticGetBaseComponentClass((TR_OpaqueClassBlock *)ramClass, numDimensions);
   TR_ASSERT(numDimensions >= 0, "Invalid number of array dimensions: %d", numDimensions);

   TR::StackMemoryRegion stackMemoryRegion(*comp->trMemory());

   size_t packedSize;
   J9ROMClass *packedROMClass = JITServerHelpers::packROMClass((numDimensions ? baseComponent : ramClass)->romClass,
                                                               comp->trMemory(), comp->fej9(), packedSize,
                                                               numDimensions ? 0 : record->romClassSize());
   if (!packedROMClass)
      {
      // Packed ROMClass size mismatch. Fail early (no need to compute the hash).
      TR_ASSERT(!numDimensions && (packedSize != record->romClassSize()),
                "packROMClass() returned NULL but expected size matches");
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "ERROR: ROMClass size mismatch for class %.*s ID %zu: %zu != %u",
            RECORD_NAME(record), record->id(), packedSize, record->romClassSize()
         );
      ++_numClassSizeMismatches;
      return false;
      }

   JITServerROMClassHash hash(packedROMClass);
   if (numDimensions)
      {
      auto &arrayHash = JITServerROMClassHash::getObjectArrayHash(ramClass->romClass, *comp->trMemory(), comp->fej9());
      hash = JITServerROMClassHash(arrayHash, hash, numDimensions);
      }

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

   return V();
   }


bool
JITServerAOTDeserializer::invalidateGeneratedClass(J9Class *ramClass)
   {
   size_t prefixLength = JITServerHelpers::getGeneratedClassNamePrefixLength(ramClass->romClass);
   if (!prefixLength)
      return false;

   const uint8_t *name = J9UTF8_DATA(J9ROMCLASS_CLASSNAME(ramClass->romClass));
   //NOTE: No need to acquire the _generatedClassesMonitor because this function is only called with
   // exclusive VM access, whereas all other accesses to _generatedClasses are done with shared VM access.
   auto n_it = _generatedClasses.find({ ramClass->classLoader, StringKey(name, prefixLength) });
   if (n_it == _generatedClasses.end())
      return false;

   auto c_it = n_it->second._classPtrMap.find(ramClass);
   if (c_it == n_it->second._classPtrMap.end())
      return false;

   size_t count = n_it->second._classHashMap.erase(c_it->second);
   TR_ASSERT_FATAL(count == 1, "Broken two-way map for generated class %p", ramClass);
   n_it->second._classPtrMap.erase(c_it);

   TR_ASSERT_FATAL(n_it->second._classHashMap.size() == n_it->second._classPtrMap.size(),
                   "Broken two-way map for generated class %p", ramClass);
   if (n_it->second._classHashMap.empty())
      _generatedClasses.erase(n_it);
   return true;
   }


JITServerLocalSCCAOTDeserializer::JITServerLocalSCCAOTDeserializer(TR_PersistentClassLoaderTable *loaderTable,
                                                                   J9JITConfig *jitConfig) :
   JITServerAOTDeserializer(loaderTable, jitConfig),
   _sharedCache(loaderTable->getSharedCache()),
   _classLoaderIdMap(decltype(_classLoaderIdMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classLoaderPtrMap(decltype(_classLoaderPtrMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classIdMap(decltype(_classIdMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classPtrMap(decltype(_classPtrMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _methodMap(decltype(_methodMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classChainMap(decltype(_classChainMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _wellKnownClassesMap(decltype(_wellKnownClassesMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _generatedClassesSccMap(decltype(_generatedClassesSccMap)::allocator_type(TR::Compiler->persistentAllocator()))
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
JITServerLocalSCCAOTDeserializer::invalidateClass(J9VMThread *vmThread, J9Class *oldClass, J9Class *newClass)
   {
   // The oldClass will always be the RAM class that needs to be invalidated. Since we do not
   // need to deal with cached RAM method pointers when not ignoring the local SCC, the
   // newClass parameter is unused here.
   assertExclusiveVmAccess(vmThread);

   if (invalidateGeneratedClass(oldClass))
      {
      uintptr_t offset;
      if (_sharedCache->isROMClassInSharedCache(oldClass->romClass, &offset))
         _generatedClassesSccMap.erase({ oldClass->classLoader, offset });
      }

   auto p_it = _classPtrMap.find(oldClass);
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
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Invalidated RAMClass %p ID %zu", oldClass, id);
   }


J9Class *
JITServerLocalSCCAOTDeserializer::getGeneratedClass(J9ClassLoader *loader, uintptr_t romClassSccOffset,
                                                    TR::Compilation *comp)
   {
   assertSharedVmAccess(comp->j9VMThread());
   OMR::CriticalSection cs(getClassMonitor());

   auto it = _generatedClassesSccMap.find({ loader, romClassSccOffset });
   return (it != _generatedClassesSccMap.end()) ? it->second : NULL;
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
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "ERROR: Class loader for class %.*s ID %zu was marked invalid",
            RECORD_NAME(record), record->id()
         );
      return false;
      }

   // Lookup the RAMClass by name in the class loader, or in the generated classes map if the class is runtime-generated
   J9Class *ramClass = record->isGenerated()
      ? findGeneratedClass(loader, record->name(), record->nameLength(), record->hash(), comp->j9VMThread())
      : jitGetClassInClassloaderFromUTF8(comp->j9VMThread(), loader, (char *)record->name(), record->nameLength());
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

   // Check that the ROMClass hash matches, otherwise remember that it doesn't. Note that for generated classes,
   // the hash is already guaranteed to be valid since their RAMClasses are looked up based on the hash.
   if (!record->isGenerated() && !isClassMatching(record, ramClass, comp))
      {
      addToMaps(_classIdMap, _classPtrMap, it, record->id(), { ramClass, (uintptr_t)-1, (uintptr_t)-1 }, ramClass);
      return false;
      }

   addToMaps(_classIdMap, _classPtrMap, it, record->id(), { ramClass, offset, loaderOffset }, ramClass);

   // Doesn't have to be atomic with the above w.r.t. exceptions
   if (record->isGenerated())
      _generatedClassesSccMap.insert({{ loader, offset }, ramClass });

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
      if (auto prefixLength = JITServerHelpers::getGeneratedClassNamePrefixLength(ramClass->romClass))
         {
         // Try to lookup a new version of the generated class using its deterministic ROMClass hash
         JITServerROMClassHash hash(romClass, *comp->trMemory(), comp->fej9(), true);
         ramClass = findGeneratedClass(loader, J9UTF8_DATA(name), prefixLength, hash, comp->j9VMThread());
         }
      }

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

JITServerNoSCCAOTDeserializer::JITServerNoSCCAOTDeserializer(TR_PersistentClassLoaderTable *loaderTable,
                                                             J9JITConfig *jitConfig) :
   JITServerAOTDeserializer(loaderTable, jitConfig),
   _classLoaderIdMap(decltype(_classLoaderIdMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classLoaderPtrMap(decltype(_classLoaderPtrMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classIdMap(decltype(_classIdMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classPtrMap(decltype(_classPtrMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _methodIdMap(decltype(_methodIdMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _methodPtrMap(decltype(_methodPtrMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classChainMap(decltype(_classChainMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _wellKnownClassesMap(decltype(_wellKnownClassesMap)::allocator_type(TR::Compiler->persistentAllocator()))
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
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Invalidated class loader %p ID %zu in the deserializer cache", loader, id);
   }

void
JITServerNoSCCAOTDeserializer::invalidateClass(J9VMThread *vmThread, J9Class *oldClass, J9Class *newClass)
   {
   assertExclusiveVmAccess(vmThread);

   invalidateGeneratedClass(oldClass);

   auto p_it = _classPtrMap.find(oldClass);
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

   // Invalidate any methods that might have this RAM class recorded as their
   // defining class. If the class is being invalidated because of an in-place
   // class redefinition, then these invalid methods could be in newClass, so we
   // invalidate both classes' methods to make sure we get them all.
   for (uint32_t i = 0; i < oldClass->romClass->romMethodCount; i++)
      invalidateMethod(&oldClass->ramMethods[i]);

   if (newClass)
      {
      for (uint32_t i = 0; i < newClass->romClass->romMethodCount; i++)
         invalidateMethod(&newClass->ramMethods[i]);
      }

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Invalidated RAMClass %p ID %zu in the deserializer cache", oldClass, id);
   }

void
JITServerNoSCCAOTDeserializer::invalidateMethod(J9Method *method)
   {
   auto p_it = _methodPtrMap.find(method);
   if (p_it == _methodPtrMap.end()) // Not cached or already marked as invalid
      return;

   uintptr_t id = p_it->second;
   auto i_it = _methodIdMap.find(id);
   TR_ASSERT(i_it != _methodIdMap.end(), "Broken two-way map");

   i_it->second = NULL;

   _methodPtrMap.erase(p_it);

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Invalidated RAMMethod %p ID %zu in the deserializer cache", method, id);
   }


J9Class *
JITServerNoSCCAOTDeserializer::getGeneratedClass(J9ClassLoader *loader, uintptr_t romClassSccOffset,
                                                 TR::Compilation *comp)
   {
   bool wasReset = false;
   J9Class *ramClass = classFromOffset(romClassSccOffset, comp, wasReset);
   if (wasReset)
      comp->failCompilation<J9::AOTDeserializerReset>("Deserializer reset during relocation of method %s",
                                                      comp->signature());
   return ramClass;
   }


void
JITServerNoSCCAOTDeserializer::clearCachedData()
   {
   _classLoaderIdMap.clear();
   _classLoaderPtrMap.clear();

   _classIdMap.clear();
   _classPtrMap.clear();

   _methodIdMap.clear();
   _methodPtrMap.clear();

   for (auto &kv : _classChainMap)
      TR::Compiler->persistentGlobalMemory()->freePersistentMemory(kv.second);
   _classChainMap.clear();

   for (auto &kv : _wellKnownClassesMap)
      TR::Compiler->persistentGlobalMemory()->freePersistentMemory(kv.second);
   _wellKnownClassesMap.clear();

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
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "ERROR: Class loader for class %.*s ID %zu was marked invalid",
            RECORD_NAME(record), record->id()
         );
      return false;
      }

   // Lookup the RAMClass by name in the class loader, or in the generated classes map if the class is runtime-generated
   J9Class *ramClass = record->isGenerated()
      ? findGeneratedClass(loader, record->name(), record->nameLength(), record->hash(), comp->j9VMThread())
      : jitGetClassInClassloaderFromUTF8(comp->j9VMThread(), loader, (char *)record->name(), record->nameLength());
   if (!ramClass)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "ERROR: Failed to find class %.*s ID %zu in class loader %p",
            RECORD_NAME(record), record->id(), loader
         );
      return false;
      }

   // Check that the ROMClass hash matches, otherwise remember that it doesn't. Note that for generated classes,
   // the hash is already guaranteed to be valid since their RAMClasses are looked up based on the hash.
   if (!record->isGenerated() && !isClassMatching(record, ramClass, comp))
      {
      // We add {ID, NULL} and {ramClass, ID} to their respective maps because
      //
      // 1. We need to record the fact that the ramClass we just found doesn't match, to avoid looking up the exact
      //    same RAM class in future deserializations of this class record. (We don't report this class record ID as being
      //    new to the server if we fail to cache it, so the server will keep sending us this exact class record).
      // 2. If ramClass ever gets unloaded, we want invalidateClass to be able to remove both entries from the maps, so that
      //    in subsequent deserializations we can attempt to find a new candidate ramClass that might end up matching after all.
      addToMaps(_classIdMap, _classPtrMap, it, record->id(), { NULL, record->classLoaderId() }, ramClass);
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
   OMR::CriticalSection cs(getMethodMonitor());
   if (deserializerWasReset(comp, wasReset))
      return false;

   auto it = _methodIdMap.find(record->id());
   if (it != _methodIdMap.end())
      {
      if (it->second)
         {
         return true;
         }
      else
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "ERROR: Mismatching or unloaded method ID %zu", record->id());
         return false;
         }
      }
   isNew = true;

   // Get the defining RAM class for this method using its ID. If it can't be found,
   // it was marked as invalid. We don't support reloading, so simply fail here in
   // that case.
   auto ramClass = findInMap(_classIdMap, record->definingClassId(), getClassMonitor(), comp, wasReset)._ramClass;
   if (!ramClass)
      return false;

   // Get RAMMethod (and its ROMMethod) in defining RAMClass by index
   TR_ASSERT(record->index() < ramClass->romClass->romMethodCount, "Invalid method index");
   J9Method *ramMethod = &ramClass->ramMethods[record->index()];
   J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);

   addToMaps(_methodIdMap, _methodPtrMap, it, record->id(), ramMethod, ramMethod);

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
         "Cached method record ID %zu -> { %p, %zu } for method %.*s.%.*s%.*s",
         record->id(), ramMethod, record->definingClassId(), ROMCLASS_NAME(ramClass->romClass), ROMMETHOD_NAS(romMethod)
      );
   return true;
   }

void
JITServerNoSCCAOTDeserializer::getRAMClassChain(TR::Compilation *comp, J9Class *clazz, J9Class **chainBuffer, size_t &chainLength)
   {
   chainLength = comp->fej9()->necessaryClassChainLength(clazz) - 1;

   J9Class **cursor = chainBuffer;
   *cursor++ = clazz;
   for (size_t i = 0; i < J9CLASS_DEPTH(clazz); ++i)
      *cursor++ = clazz->superclasses[i];
   for (auto it = (J9ITable *)clazz->iTable; it; it = it->next)
      *cursor++ = it->interfaceClass;
   TR_ASSERT((cursor - chainBuffer) == chainLength, "Invalid RAM class chain length: %zu != %zu", cursor - chainBuffer, chainLength);
   }

// Add a value (which must have been allocated with TR::Compiler->persistentGlobalMemory()->allocatePersistentMemory) to the given map
template<typename K, typename V, typename H> static void
addToChainMap(PersistentUnorderedMap<K, V *, H> &map,
              const typename PersistentUnorderedMap<K, V *, H>::const_iterator &it,
              const K &key, V *value)
   {
   try
      {
      map.insert(it, { key, value });
      }
   catch (...)
      {
      TR::Compiler->persistentGlobalMemory()->freePersistentMemory(value);
      throw;
      }
   }

bool
JITServerNoSCCAOTDeserializer::cacheRecord(const ClassChainSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset)
   {
   OMR::CriticalSection cs(getClassChainMonitor());
   if (deserializerWasReset(comp, wasReset))
      return false;

   auto it = _classChainMap.find(record->id());
   if (it != _classChainMap.end())
      return true;
   isNew = true;

   // Get the RAM class chain for the first class referenced in the class chain serialization record
   auto firstClass = findInMap(_classIdMap, record->list().ids()[0], getClassMonitor(), comp, wasReset)._ramClass;
   if (!firstClass)
      return false;
   J9Class *ramClassChain[TR_J9SharedCache::maxClassChainLength];
   size_t ramClassChainLength = 0;
   getRAMClassChain(comp, firstClass, ramClassChain, ramClassChainLength);

   // Check that it has the expected length
   if (record->list().length() != ramClassChainLength)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "ERROR: Class chain length mismatch for class %.*s ID %zu: %zu != %zu",
            ROMCLASS_NAME(firstClass->romClass), record->list().ids()[0], ramClassChainLength, record->list().length()
         );
      return false;
      }

   // Validate each class in the server's chain (which should all be cached by now)
   for (size_t i = 0; i < ramClassChainLength; ++i)
      {
      auto ramClass = findInMap(_classIdMap, record->list().ids()[i], getClassMonitor(), comp, wasReset)._ramClass;
      if (!ramClass)
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
               "ERROR: Class %.*s ID %zu mismatch or invalidation in class chain ID %zu for class %.*s ID %zu",
               ROMCLASS_NAME(ramClassChain[i]->romClass), record->list().ids()[i], record->id(),
               ROMCLASS_NAME(firstClass->romClass), record->list().ids()[0]
            );
         return false;
         }

      if (ramClass != ramClassChain[i])
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
               "ERROR: Class %.*s mismatch in class chain ID %zu for class %.*s ID %zu",
               ROMCLASS_NAME(ramClassChain[i]->romClass), record->id(),
               ROMCLASS_NAME(firstClass->romClass), record->list().ids()[0]
            );
         return false;
         }
      }

   // Chain validation is now complete
   size_t chainSize = (record->list().length() + 1) * sizeof(uintptr_t); // offset chains are expected to store their size in bytes in their first entry
   auto deserializerChain = (uintptr_t *)TR::Compiler->persistentGlobalMemory()->allocatePersistentMemory(chainSize);
   if (!deserializerChain)
      throw std::bad_alloc();

   deserializerChain[0] = chainSize;
   uintptr_t *deserializerChainData = deserializerChain + 1;
   for (size_t i = 0; i < record->list().length(); ++i)
      deserializerChainData[i] = encodeClassOffset(record->list().ids()[i]);
   addToChainMap(_classChainMap, it, record->id(), deserializerChain);

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
         "Cached class chain record ID %zu -> { %p } for class %.*s ID %zu",
         record->id(), firstClass, ROMCLASS_NAME(firstClass->romClass), record->list().ids()[0]
      );
   return true;
   }

bool
JITServerNoSCCAOTDeserializer::cacheRecord(const WellKnownClassesSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset)
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
   // The first entry of the chain offsets contains the number of entries (and not the size in bytes as with class chains)
   auto chainOffsets = (uintptr_t *)TR::Compiler->persistentGlobalMemory()->allocatePersistentMemory((1 + record->list().length()) * sizeof(uintptr_t));
   chainOffsets[0] = record->list().length();
   auto chainData = chainOffsets + 1;
   // Get the class chain SCC offsets for each class chain ID (which should already be cached and (re)validated).
   for (size_t i = 0; i < record->list().length(); ++i)
      chainData[i] = encodeClassChainOffset(record->list().ids()[i]);

   addToChainMap(_wellKnownClassesMap, it, record->id(), chainOffsets);

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Cached well-known classes record ID %zu", record->id());
   return true;
   }

bool
JITServerNoSCCAOTDeserializer::cacheRecord(const ThunkSerializationRecord *record, TR::Compilation *comp, bool &isNew, bool &wasReset)
   {
   // Unlike the rest of the cacheRecord functions, we do not need to acquire a monitor here, as we can rely on
   // the internal synchronization of getJ2IThunk and setJ2IThunk. Since we don't touch any internal caches, we can
   // skip checking for a reset.

   auto fej9vm = comp->fej9vm();
   void *thunk = fej9vm->getJ2IThunk((char *)record->signature(), record->signatureSize(), comp);
   if (thunk)
      return true;
   isNew = true;

   TR::CompilationInfoPerThread *compInfoPT = fej9vm->_compInfoPT;
   uint8_t *thunkStart = TR_JITServerRelocationRuntime::copyDataToCodeCache(record->thunkStart(), record->thunkSize(), fej9vm);
   if (!thunkStart)
      compInfoPT->getCompilation()->failCompilation<TR::CodeCacheError>("Failed to allocate space in the code cache");

   auto thunkAddress = thunkStart + 8;
   void *vmHelper = j9ThunkVMHelperFromSignature(fej9vm->_jitConfig, record->signatureSize(), (char *)record->signature());
   compInfoPT->reloRuntime()->reloTarget()->performThunkRelocation(reinterpret_cast<uint8_t *>(thunkAddress), (uintptr_t)vmHelper);

   fej9vm->setJ2IThunk((char *)record->signature(), record->signatureSize(), thunkAddress, comp);

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Cached thunk record ID %zu -> for thunk %.*s",
                                     record->id(), record->signatureSize(), record->signature());
   return true;
   }

bool
JITServerNoSCCAOTDeserializer::updateSCCOffsets(SerializedAOTMethod *method, TR::Compilation *comp,
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

      if (!revalidateRecord(serializedOffset.recordType(), serializedOffset.recordId(), comp, wasReset))
         return false;
      uintptr_t offset = encodeOffset(serializedOffset);

      // Update the SCC offset stored in AOT method relocation data
      uint8_t *ptr = start + serializedOffset.reloDataOffset();
      TR_ASSERT_FATAL((ptr >= start + sizeof(uintptr_t)/*skip the size word*/) && (ptr < end),
                      "Out-of-bounds relocation data offset %zu in serialized method %s",
                      serializedOffset.reloDataOffset(), comp->signature());
#if defined(DEBUG)
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "Updating offset %zu -> %zu for record type %u ID %zu at relo data offset %zu in serialized method %s",
            *(uintptr_t *)ptr, offset, serializedOffset.recordType(), serializedOffset.recordId(),
            serializedOffset.reloDataOffset(), comp->signature()
         );
#endif /* defined(DEBUG) */
      *(uintptr_t *)ptr = offset;
      }

   return true;
   }

bool
JITServerNoSCCAOTDeserializer::revalidateRecord(AOTSerializationRecordType type, uintptr_t id, TR::Compilation *comp, bool &wasReset)
   {
   switch (type)
      {
      case ClassLoader:
         {
         auto loader = findInMap(_classLoaderIdMap, id, getClassLoaderMonitor(), comp, wasReset);
         return !wasReset && (loader != NULL);
         }
      case Class:
         {
         auto clazz = findInMap(_classIdMap, id, getClassMonitor(), comp, wasReset)._ramClass;
         return !wasReset && (clazz != NULL);
         }
      case Method:
         {
         auto method = findInMap(_methodIdMap, id, getMethodMonitor(), comp, wasReset);
         return !wasReset && (method != NULL);
         }
      case ClassChain:
         {
         // Why do we need to revalidate the entire chain here?
         // Note that only serialization records the server thinks the client needs are sent to the client,
         // and only offsets used in the method's relocation records get serialized SCC offsets. That means
         // that we aren't guaranteed to have revalidated every class mentioned in this class chain, and so
         // we must check the entire chain here.
         OMR::CriticalSection cs(getClassChainMonitor());
         if (deserializerWasReset(comp, wasReset))
            return false;

         auto it = _classChainMap.find(id);
         if (it == _classChainMap.end() || !it->second)
            return false;

         size_t chainLength = it->second[0] / sizeof(it->second[0]) - 1;
         uintptr_t *chainData = it->second + 1;
         for (size_t i = 0; i < chainLength; ++i)
            {
            auto ramClass = findInMap(_classIdMap, offsetId(chainData[i]), getClassMonitor(), comp, wasReset)._ramClass;
            if (!ramClass)
               {
               TR::Compiler->persistentGlobalMemory()->freePersistentMemory(it->second);
               it->second = NULL;
               if (TR::Options::getVerboseOption(TR_VerboseJITServer))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                     "Invalidated cached class chain record ID %zu", id
                  );
               return false;
               }
            }
         return true;
         }
      case WellKnownClasses:
         {
         // See the note for case ClassChain
         OMR::CriticalSection cs(getWellKnownClassesMonitor());
         if (deserializerWasReset(comp, wasReset))
            return false;

         auto it = _wellKnownClassesMap.find(id);
         if ((it == _wellKnownClassesMap.end()) || !it->second)
            return false;

         size_t chainLength = it->second[0];
         uintptr_t *chainData = it->second + 1;
         for (size_t i = 0; i < chainLength; ++i)
            {
            auto classChain = findInMap(_classChainMap, offsetId(chainData[i]), getClassChainMonitor(), comp, wasReset);
            if (!classChain)
               {
               TR::Compiler->persistentGlobalMemory()->freePersistentMemory(it->second);
               it->second = NULL;
               if (TR::Options::getVerboseOption(TR_VerboseJITServer))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                     "Invalidated cached well-known classes record ID %zu", id
                  );
               return false;
               }
            }
         return true;
         }
      case Thunk:
         {
         // Thunks are not cached, so do not need to be revalidated
         return true;
         }
      default:
         {
         TR_ASSERT_FATAL(false, "Invalid record type: %u", type);
         return false;
         }
      }
   }

J9ROMClass *
JITServerNoSCCAOTDeserializer::romClassFromOffsetInSharedCache(uintptr_t offset, TR::Compilation *comp, bool &wasReset)
   {
   auto clazz = classFromOffset(offset, comp, wasReset);
   if (clazz)
      return clazz->romClass;

   return NULL;
   }

J9Class *
JITServerNoSCCAOTDeserializer::classFromOffset(uintptr_t offset, TR::Compilation *comp, bool &wasReset)
   {
   TR_ASSERT_FATAL(offsetType(offset) == AOTSerializationRecordType::Class, "Offset %zu must be to a class", offset);
   return findInMap(_classIdMap, offsetId(offset), getClassMonitor(), comp, wasReset)._ramClass;
   }

// Return a pointer to the entity referred to by the given offset. Note that for class chains identifying class loaders,
// we simply return the cached (J9ClassLoader *) directly.
void *
JITServerNoSCCAOTDeserializer::pointerFromOffsetInSharedCache(uintptr_t offset, TR::Compilation *comp, bool &wasReset)
   {
   auto id = offsetId(offset);
   auto ty = AOTSerializationRecord::getType(offset);

   void *ptr = NULL;

   switch (ty)
      {
      case AOTSerializationRecordType::ClassLoader:
         ptr = findInMap(_classLoaderIdMap, id, getClassLoaderMonitor(), comp, wasReset);
         break;

      case AOTSerializationRecordType::ClassChain:
         ptr = findInMap(_classChainMap, id, getClassChainMonitor(), comp, wasReset);
         break;

      case AOTSerializationRecordType::WellKnownClasses:
         ptr = findInMap(_wellKnownClassesMap, id, getWellKnownClassesMonitor(), comp, wasReset);
         break;

      default:
         // Only the above types of offsets are ever looked up during relocation
         TR_ASSERT_FATAL(false, "Offset %zu ID %zu type %zu into deserializer cache is not a supported type");
         break;
      }

   return ptr;
   }

J9ROMMethod *
JITServerNoSCCAOTDeserializer::romMethodFromOffsetInSharedCache(uintptr_t offset, TR::Compilation *comp, bool &wasReset)
   {
   TR_ASSERT_FATAL(offsetType(offset) == AOTSerializationRecordType::Method, "Offset %zu must be to a method", offset);
   auto romMethod = findInMap(_methodIdMap, offsetId(offset), getMethodMonitor(), comp, wasReset);
   if (romMethod)
      return J9_ROM_METHOD_FROM_RAM_METHOD(romMethod);

   return NULL;
   }
