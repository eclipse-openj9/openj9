/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#include "JITServerPersistentCHTable.hpp"
#include "compile/Compilation.hpp"
#include "control/CompilationRuntime.hpp"
#include "net/ServerStream.hpp"
#include "env/ClassTableCriticalSection.hpp"   // for ClassTableCriticalSection
#include "control/CompilationThread.hpp"       // for TR::compInfoPT
#include "runtime/JITClientSession.hpp"


JITServerPersistentCHTable::JITServerPersistentCHTable(TR_PersistentMemory *trMemory)
   : TR_PersistentCHTable(trMemory)
   {
   }

PersistentUnorderedMap<TR_OpaqueClassBlock*, TR_PersistentClassInfo*> &JITServerPersistentCHTable::getData()
   {
   auto &data = TR::compInfoPT->getClientData()->getCHTableClassMap();
   return data;
   }

bool
JITServerPersistentCHTable::initializeIfNeeded(TR_J9VMBase *fej9)
   {
      {
      TR::ClassTableCriticalSection initializeIfNeeded(fej9);
      auto& data = getData();
      if (!data.empty())
         return false; // this is the most frequent path
      }

   auto stream = TR::CompilationInfo::getStream();
   stream->write(JITServer::MessageType::CHTable_getAllClassInfo, JITServer::Void());
   std::string rawData = std::get<0>(stream->read<std::string>());
   auto infos = FlatPersistentClassInfo::deserializeHierarchy(rawData);
      {
      TR::ClassTableCriticalSection initializeIfNeeded(fej9);
      auto& data = getData();
      if (data.empty()) // check again to prevent races
         {
         for (auto clazz : infos)
            data.insert({ clazz->getClassId(), clazz });
         CHTABLE_UPDATE_COUNTER(_numClassesUpdated, infos.size());
         return true;
         }
      }
      return false;
   }

void 
JITServerPersistentCHTable::doUpdate(TR_J9VMBase *fej9, const std::string &removeStr, const std::string &modifyStr)
   {
   TR::ClassTableCriticalSection doUpdate(fej9);
   if (!modifyStr.empty())
      commitModifications(modifyStr);
   if (!removeStr.empty())
      commitRemoves(removeStr);

#ifdef COLLECT_CHTABLE_STATS
   uint32_t nBytes = removeStr.size() + modifyStr.size();
   CHTABLE_UPDATE_COUNTER(_updateBytes, nBytes);
   CHTABLE_UPDATE_COUNTER(_numUpdates, 1);
   uint32_t prevMax = _maxUpdateBytes;
   _maxUpdateBytes = std::max(nBytes, _maxUpdateBytes);
#endif
   }

void 
JITServerPersistentCHTable::commitRemoves(const std::string &rawData)
   {
   auto &data = getData();
   TR_OpaqueClassBlock **ptr = (TR_OpaqueClassBlock**)&rawData[0];
   size_t num = rawData.size() / sizeof(TR_OpaqueClassBlock*);
   for (size_t i = 0; i < num; i++)
      {
      auto item = data[ptr[i]];
      data.erase(ptr[i]);
      if (item) // may have already been removed earlier in this update block
         jitPersistentFree(item);
      }
   CHTABLE_UPDATE_COUNTER(_numClassesRemoved, num);
   }

void 
JITServerPersistentCHTable::commitModifications(const std::string &rawData)
   {
   auto &data = getData();
   std::unordered_map<TR_OpaqueClassBlock*, std::pair<FlatPersistentClassInfo*, TR_PersistentClassInfo*>> infoMap;

   // First, process all TR_PersistentClassInfo entries that have been
   // modified at the client without worrying about subclasses
   size_t bytesRead = 0;
   size_t count = 0;
   while (bytesRead != rawData.length())
      {
      TR_ASSERT(bytesRead < rawData.length(), "Corrupt CHTable!!");
      FlatPersistentClassInfo *info = (FlatPersistentClassInfo*)&rawData[bytesRead];
      TR_OpaqueClassBlock *classId = (TR_OpaqueClassBlock *) (((uintptr_t) info->_classId) & ~(uintptr_t)1);
      TR_ASSERT(classId, "Class cannot be null (on server)");
      TR_PersistentClassInfo* clazz = findClassInfo(classId);
      if (!clazz)
         {
         clazz = new (PERSISTENT_NEW) TR_PersistentClassInfo(NULL);
         data.insert({classId, clazz});
         }
      infoMap.insert({classId, {info, clazz}});
      // Overwrite existing TR_PersistentClassInfo entry with info from client
      // Do not touch the subclasses list at this point
      bytesRead += FlatPersistentClassInfo::deserializeClassSimple(clazz, info);
      count++;
      }

   // populate subclasses
   for (auto it : infoMap)
      {
      auto flat = it.second.first;
      auto persist = it.second.second;
      persist->removeSubClasses();
      for (size_t i = 0; i < flat->_numSubClasses; i++)
         {
         auto classInfo = findClassInfo(flat->_subClasses[i]);

         TR_ASSERT_FATAL(classInfo, "subclass info cannot be null: ensure subclasses are loaded before superclass");
         if (classInfo)
            persist->addSubClass(classInfo);
         else
            fprintf(stderr, "CHTable WARNING: classInfo for subclass %p of %p is null, ignoring\n", flat->_subClasses[i], flat->_classId);
         }
      }
   CHTABLE_UPDATE_COUNTER(_numClassesUpdated, count);
   }

TR_PersistentClassInfo *
JITServerPersistentCHTable::findClassInfo(TR_OpaqueClassBlock * classId)
   {
   CHTABLE_UPDATE_COUNTER(_numQueries, 1);
   auto& data = getData();
   // It is possible that the persistentCHTable on the client side is not populated,
   // such as when recompilation is not allowed on the client side.
   if (!data.empty())
      {
      auto it = data.find(classId);
      if (it != data.end())
         return it->second;
      }
   return NULL;
   }

TR_PersistentClassInfo *
JITServerPersistentCHTable::findClassInfoAfterLocking(
      TR_OpaqueClassBlock *classId,
      TR::Compilation *comp,
      bool returnClassInfoForAOT)
   {
   if (comp->compileRelocatableCode() && !returnClassInfoForAOT)
      return NULL;

   if (comp->getOption(TR_DisableCHOpts))
      return NULL;

   TR_PersistentClassInfo *classInfo = NULL;
      {
      TR::ClassTableCriticalSection findClassInfoAfterLocking(comp->fe());
      classInfo = findClassInfo(classId);
      }

   return classInfo;
   }

std::string
JITClientPersistentCHTable::serializeRemoves()
   {
   size_t outputSize = _remove.size() * sizeof(TR_OpaqueClassBlock*);
   std::string data(outputSize, '\0');
   TR_OpaqueClassBlock** ptr = (TR_OpaqueClassBlock**) &data[0];

   size_t count = 0;
   for (auto clazz : _remove)
      {
      *ptr++ = clazz;
      count++;
      }
   _remove.clear();
   CHTABLE_UPDATE_COUNTER(_numClassesRemoved, count);

   return data;
   }

std::string
JITClientPersistentCHTable::serializeModifications()
   {
   size_t numBytes = 0;
   for (auto classId : _dirty)
      {
      auto clazz = findClassInfo(classId);
      if (!clazz) continue;
      size_t size = FlatPersistentClassInfo::classSize(clazz);
      numBytes += size;
      }

   std::string data(numBytes, '\0');

   size_t bytesWritten = 0;
   size_t count = 0;
   for (auto classId : _dirty)
      {
      auto clazz = findClassInfo(classId);
      if (!clazz) continue;
      FlatPersistentClassInfo* info = (FlatPersistentClassInfo*)&data[bytesWritten];
      bytesWritten += FlatPersistentClassInfo::serializeClass(clazz, info);
      count++;
      }
   CHTABLE_UPDATE_COUNTER(_numClassesUpdated, count);

   _dirty.clear();
   return data;
   }

std::pair<std::string, std::string> 
JITClientPersistentCHTable::serializeUpdates()
   {
   TR::ClassTableCriticalSection serializeUpdates(TR::comp()->fe());
   std::string removes = serializeRemoves(); // must be called first
   std::string mods = serializeModifications();
#ifdef COLLECT_CHTABLE_STATS
   uint32_t nBytes = removes.size() + mods.size();
   CHTABLE_UPDATE_COUNTER(_updateBytes, nBytes);
   CHTABLE_UPDATE_COUNTER(_numUpdates, 1);
   uint32_t prevMax = _maxUpdateBytes;
   _maxUpdateBytes = std::max(nBytes, _maxUpdateBytes);
#endif
   TR::compInfoPT->getCompilationInfo()->markCHTableUpdateDone(TR::compInfoPT->getCompThreadId());
   return {removes, mods};
   }

void 
collectHierarchy(std::unordered_set<TR_PersistentClassInfo*> &out, TR_PersistentClassInfo *root)
   {
   out.insert(root);
   for (TR_SubClass *c = root->getFirstSubclass(); c; c = c->getNext())
      collectHierarchy(out, c->getClassInfo());
   }

size_t 
FlatPersistentClassInfo::classSize(TR_PersistentClassInfo *clazz)
   {
   auto numSubClasses = clazz->_subClasses.getSize();
   return sizeof(FlatPersistentClassInfo) + numSubClasses * sizeof(TR_OpaqueClassBlock*);
   }

size_t 
FlatPersistentClassInfo::serializeClass(TR_PersistentClassInfo *clazz, FlatPersistentClassInfo* info)
   {
   info->_classId = clazz->_classId;
   info->_visitedStatus = clazz->_visitedStatus;
   info->_prexAssumptions = clazz->_prexAssumptions;
   info->_timeStamp = clazz->_timeStamp;
   info->_nameLength = clazz->_nameLength;
   info->_flags = clazz->_flags;
   TR_ASSERT(clazz->_flags.getValue() < 0x40, "corrupted flags");
   TR_ASSERT(!clazz->getFieldInfo(), "field info not supported");
   info->_shouldNotBeNewlyExtended = clazz->_shouldNotBeNewlyExtended;
   int idx = 0;
   for (TR_SubClass *c = clazz->getFirstSubclass(); c; c = c->getNext())
      {
      TR_ASSERT(c->getClassInfo(), "Subclass info cannot be null (on client)");
      TR_ASSERT(c->getClassInfo()->getClassId(), "Subclass cannot be null (on client)");
      info->_subClasses[idx++] = c->getClassInfo()->getClassId();
      }
   info->_numSubClasses = idx;
   return sizeof(TR_OpaqueClassBlock*) * idx + sizeof(FlatPersistentClassInfo);
   }

std::string 
FlatPersistentClassInfo::serializeHierarchy(TR_PersistentClassInfo *root)
   {
   if (!root)
      return "";

   // walk class hierarchy to find all referenced classes
   TR::ClassTableCriticalSection serializePersistentClassInfo(TR::comp()->fe());
   std::unordered_set<TR_PersistentClassInfo*> classes;
   collectHierarchy(classes, root);

   // Find output size
   size_t numBytes = 0;
   for (auto clazz : classes)
      {
      size_t size = classSize(clazz);
      numBytes += size;
      }

   // Serialize to string
   std::string data(numBytes, '\0');
   size_t bytesWritten = 0;
   for (auto clazz : classes)
      {
      FlatPersistentClassInfo* info = (FlatPersistentClassInfo*)&data[bytesWritten];
      bytesWritten += serializeClass(clazz, info);
      }
   return data;
   }

// NOTE: does not populate subclasses, but does include them in the returned size
size_t 
FlatPersistentClassInfo::deserializeClassSimple(TR_PersistentClassInfo *clazz, FlatPersistentClassInfo *info)
   {
   // we do not overwrite _prexAssumptions here because it's updated only by the server
   // so the number of assumptions on the server is correct, but on the client it's always 0
   clazz->_classId = info->_classId;
   clazz->_visitedStatus = info->_visitedStatus;
   clazz->_timeStamp = info->_timeStamp;
   clazz->_nameLength = info->_nameLength;
   clazz->_flags = info->_flags;
   clazz->_shouldNotBeNewlyExtended = info->_shouldNotBeNewlyExtended;
   clazz->_fieldInfo = NULL;
   return sizeof(FlatPersistentClassInfo) + info->_numSubClasses * sizeof(TR_OpaqueClassBlock*);
   }

std::vector<TR_PersistentClassInfo*> 
FlatPersistentClassInfo::deserializeHierarchy(std::string& data)
   {
   std::vector<TR_PersistentClassInfo*> out;
   std::unordered_map<TR_OpaqueClassBlock*, std::pair<FlatPersistentClassInfo*, TR_PersistentClassInfo*>> infoMap;

   size_t bytesRead = 0;
   while (bytesRead != data.length())
      {
      TR_ASSERT(bytesRead < data.length(), "Corrupt CHTable!!");
      FlatPersistentClassInfo* info = (FlatPersistentClassInfo*)&data[bytesRead];
      TR_PersistentClassInfo *clazz = new (PERSISTENT_NEW) TR_PersistentClassInfo(NULL);
      bytesRead += deserializeClassSimple(clazz, info);
      out.push_back(clazz);
      infoMap.insert({clazz->getClassId(), {info, clazz}});
      }

   // populate subclasses
   for (auto it : infoMap)
      {
      auto flat = it.second.first;
      auto persist = it.second.second;
      for (size_t i = 0; i < flat->_numSubClasses; i++)
         {
         persist->addSubClass(infoMap[flat->_subClasses[i]].second);
         }
      }

   return out;
   }


// JITClient
// TODO: check for race conditions with _dirty/_remove

JITClientPersistentCHTable::JITClientPersistentCHTable(TR_PersistentMemory *trMemory)
   : TR_PersistentCHTable(trMemory)
   , _dirty(decltype(_dirty)::allocator_type(TR::Compiler->persistentAllocator()))
   , _remove(decltype(_remove)::allocator_type(TR::Compiler->persistentAllocator()))
   {
   }

TR_PersistentClassInfo *
JITClientPersistentCHTable::findClassInfo(TR_OpaqueClassBlock * classId)
   {
   return TR_PersistentCHTable::findClassInfo(classId);
   }

TR_PersistentClassInfo *
JITClientPersistentCHTable::findClassInfoAfterLocking(
      TR_OpaqueClassBlock *classId,
      TR::Compilation *comp,
      bool returnClassInfoForAOT)
   {
   return TR_PersistentCHTable::findClassInfoAfterLocking(classId, comp, returnClassInfoForAOT);
   }

void
JITClientPersistentCHTable::classGotUnloaded(
      TR_FrontEnd *fe,
      TR_OpaqueClassBlock *classId)
   {
   TR_PersistentCHTable::classGotUnloaded(fe, classId);
   }

void
JITClientPersistentCHTable::classGotUnloadedPost(
      TR_FrontEnd *fe,
      TR_OpaqueClassBlock *classId)
   {
   TR_PersistentCHTable::classGotUnloadedPost(fe, classId);
   }

void
JITClientPersistentCHTable::classGotRedefined(
      TR_FrontEnd *fe,
      TR_OpaqueClassBlock *oldClassId,
      TR_OpaqueClassBlock *newClassId)
   {
   TR_PersistentCHTable::classGotRedefined(fe, oldClassId, newClassId);
   }

void
JITClientPersistentCHTable::removeClass(
      TR_FrontEnd *fe,
      TR_OpaqueClassBlock *classId,
      TR_PersistentClassInfo *info,
      bool removeInfo)
   {
   if (!info)
      return;

   TR_PersistentCHTable::removeClass(fe, classId, info, removeInfo);
   }

TR_PersistentClassInfo *
JITClientPersistentCHTable::classGotLoaded(
      TR_FrontEnd *fe,
      TR_OpaqueClassBlock *classId)
   {
   TR_ASSERT(!findClassInfo(classId), "Should not add duplicates to hash table\n");
   TR_PersistentClassInfo *clazz = new (PERSISTENT_NEW) TR_JITClientPersistentClassInfo(classId, this);
   if (clazz)
      {
      auto classes = getClasses();
      classes[TR_RuntimeAssumptionTable::hashCode((uintptrj_t) classId) % CLASSHASHTABLE_SIZE].add(clazz);
      }
   return clazz;
   }

bool
JITClientPersistentCHTable::classGotInitialized(
      TR_FrontEnd *fe,
      TR_PersistentMemory *persistentMemory,
      TR_OpaqueClassBlock *classId,
      TR_PersistentClassInfo *clazz)
   {
   return TR_PersistentCHTable::classGotInitialized(fe, persistentMemory, classId, clazz);
   }

bool
JITClientPersistentCHTable::classGotExtended(
      TR_FrontEnd *fe,
      TR_PersistentMemory *persistentMemory,
      TR_OpaqueClassBlock *superClassId,
      TR_OpaqueClassBlock *subClassId)
   {
   return TR_PersistentCHTable::classGotExtended(fe, persistentMemory, superClassId, subClassId);
   }


// these two tables should be mutually exclusive - we only keep the most recent entry.
void 
JITClientPersistentCHTable::markForRemoval(TR_OpaqueClassBlock *clazz)
   {
   _remove.insert(clazz);
   _dirty.erase(clazz);
   }

void 
JITClientPersistentCHTable::markDirty(TR_OpaqueClassBlock *clazz)
   {
   _dirty.insert(clazz);
   _remove.erase(clazz);
   }


// TR_JITClientPersistentClassInfo
JITClientPersistentCHTable *TR_JITClientPersistentClassInfo::_chTable = NULL;

TR_JITClientPersistentClassInfo::TR_JITClientPersistentClassInfo(TR_OpaqueClassBlock *id, JITClientPersistentCHTable *chTable) :
   TR_PersistentClassInfo(id)
   {
   // assign pointer to the CH table, if it's the first class info created
   if (!TR_JITClientPersistentClassInfo::_chTable)
      TR_JITClientPersistentClassInfo::_chTable = chTable;
   TR_JITClientPersistentClassInfo::_chTable->markDirty(id);
   }

void
TR_JITClientPersistentClassInfo::setInitialized(TR_PersistentMemory *persistentMemory)
   {
   TR_JITClientPersistentClassInfo::_chTable->markDirty(getClassId());
   TR_PersistentClassInfo::setInitialized(persistentMemory);
   }

void
TR_JITClientPersistentClassInfo::setClassId(TR_OpaqueClassBlock *newClass)
   {
   TR_JITClientPersistentClassInfo::_chTable->markDirty(getClassId());
   TR_PersistentClassInfo::setClassId(newClass);
   }

void TR_JITClientPersistentClassInfo::setFirstSubClass(TR_SubClass *sc)
   {
   TR_JITClientPersistentClassInfo::_chTable->markDirty(getClassId());
   TR_PersistentClassInfo::setFirstSubClass(sc);
   }

void TR_JITClientPersistentClassInfo::setFieldInfo(TR_PersistentClassInfoForFields *i)
   {
   TR_JITClientPersistentClassInfo::_chTable->markDirty(getClassId());
   TR_PersistentClassInfo::setFieldInfo(i);
   }

TR_SubClass *TR_JITClientPersistentClassInfo::addSubClass(TR_PersistentClassInfo *subClass)
   {
   TR_JITClientPersistentClassInfo::_chTable->markDirty(getClassId());
   return TR_PersistentClassInfo::addSubClass(subClass);
   }

void TR_JITClientPersistentClassInfo::removeSubClasses()
   {
   TR_JITClientPersistentClassInfo::_chTable->markDirty(getClassId());
   TR_PersistentClassInfo::removeSubClasses();
   }

void TR_JITClientPersistentClassInfo::removeASubClass(TR_PersistentClassInfo *subClass)
   {
   TR_JITClientPersistentClassInfo::_chTable->markDirty(getClassId());
   TR_PersistentClassInfo::removeASubClass(subClass);
   }

void TR_JITClientPersistentClassInfo::removeUnloadedSubClasses()
   {
   TR_JITClientPersistentClassInfo::_chTable->markDirty(getClassId());
   TR_PersistentClassInfo::removeUnloadedSubClasses();
   }

void TR_JITClientPersistentClassInfo::setUnloaded()
   {
   // The only method that marks for removal
   TR_JITClientPersistentClassInfo::_chTable->markForRemoval(getClassId());
   TR_PersistentClassInfo::setUnloaded();
   }

void TR_JITClientPersistentClassInfo::incNumPrexAssumptions()
   {
   TR_JITClientPersistentClassInfo::_chTable->markDirty(getClassId());
   TR_PersistentClassInfo::incNumPrexAssumptions();
   }
   
void TR_JITClientPersistentClassInfo::setReservable(bool v)
   {
   TR_JITClientPersistentClassInfo::_chTable->markDirty(getClassId());
   TR_PersistentClassInfo::setReservable(v);
   }

void TR_JITClientPersistentClassInfo::setShouldNotBeNewlyExtended(int32_t ID)
   {
   TR_JITClientPersistentClassInfo::_chTable->markDirty(getClassId());
   TR_PersistentClassInfo::setShouldNotBeNewlyExtended(ID);
   }

void TR_JITClientPersistentClassInfo::resetShouldNotBeNewlyExtended(int32_t ID)
   {
   TR_JITClientPersistentClassInfo::_chTable->markDirty(getClassId());
   TR_PersistentClassInfo::resetShouldNotBeNewlyExtended(ID);
   }

void TR_JITClientPersistentClassInfo::clearShouldNotBeNewlyExtended()
   {
   TR_JITClientPersistentClassInfo::_chTable->markDirty(getClassId());
   TR_PersistentClassInfo::clearShouldNotBeNewlyExtended();
   }

void TR_JITClientPersistentClassInfo::setHasRecognizedAnnotations(bool v)
   {
   TR_JITClientPersistentClassInfo::_chTable->markDirty(getClassId());
   TR_PersistentClassInfo::setHasRecognizedAnnotations(v);
   }

void TR_JITClientPersistentClassInfo::setAlreadyCheckedForAnnotations(bool v)
   {
   TR_JITClientPersistentClassInfo::_chTable->markDirty(getClassId());
   TR_PersistentClassInfo::setAlreadyCheckedForAnnotations(v);
   }

void TR_JITClientPersistentClassInfo::setCannotTrustStaticFinal(bool v)
   {
   TR_JITClientPersistentClassInfo::_chTable->markDirty(getClassId());
   TR_PersistentClassInfo::setCannotTrustStaticFinal(v);
   }

void TR_JITClientPersistentClassInfo::setClassHasBeenRedefined(bool v)
   {
   TR_JITClientPersistentClassInfo::_chTable->markDirty(getClassId());
   TR_PersistentClassInfo::setClassHasBeenRedefined(v);
   }

void TR_JITClientPersistentClassInfo::setNameLength(int32_t length)
   {
   TR_JITClientPersistentClassInfo::_chTable->markDirty(getClassId());
   TR_PersistentClassInfo::setNameLength(length);
   }
