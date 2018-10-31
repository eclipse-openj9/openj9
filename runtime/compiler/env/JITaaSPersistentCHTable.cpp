/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#include "JITaaSPersistentCHTable.hpp"
#include "compile/Compilation.hpp"
#include "control/CompilationRuntime.hpp"
#include "J9Server.hpp"
#include "env/ClassTableCriticalSection.hpp"   // for ClassTableCriticalSection
#include "control/CompilationThread.hpp"       // for TR::compInfoPT
#include "control/JITaaSCompilationThread.hpp"   // for ClientSessionData
#include "env/VerboseLog.hpp"
#include "control/MethodToBeCompiled.hpp"


// plan: send the whole table once,
// then on the client mark entires as dirty when they are modified
// and send dirty entries to the server periodically


// SERVER

TR_JITaaSServerPersistentCHTable::TR_JITaaSServerPersistentCHTable(TR_PersistentMemory *trMemory)
   : TR_PersistentCHTable(trMemory)
   {
   }

PersistentUnorderedMap<TR_OpaqueClassBlock*, TR_PersistentClassInfo*> &TR_JITaaSServerPersistentCHTable::getData()
   {
   auto &data = TR::compInfoPT->getClientData()->getCHTableClassMap();
   return data;
   }

void 
TR_JITaaSServerPersistentCHTable::initializeIfNeeded()
   {
   auto& data = getData();
   if (!data.empty())
      return;

   auto stream = TR::CompilationInfo::getStream();
   stream->write(JITaaS::J9ServerMessageType::CHTable_getAllClassInfo, JITaaS::Void());
   std::string rawData = std::get<0>(stream->read<std::string>());
   auto infos = FlatPersistentClassInfo::deserializeHierarchy(rawData);
   for (auto clazz : infos)
      {
      data.insert({clazz->getClassId(), clazz});
      }
   CHTABLE_UPDATE_COUNTER(_numClassesUpdated, infos.size());
   }

void 
TR_JITaaSServerPersistentCHTable::doUpdate(TR::Compilation *comp)
   {
   if (comp->getOption(TR_DisableCHOpts))
      return;

      {
      TR::ClassTableCriticalSection doUpdate(comp->fe());
      initializeIfNeeded();
      }

   auto stream = TR::CompilationInfo::getStream();
   stream->write(JITaaS::J9ServerMessageType::CHTable_getClassInfoUpdates, JITaaS::Void());
   auto recv = stream->read<std::string, std::string, uint32_t>();
   std::string &removeStr = std::get<0>(recv);
   std::string &modifyStr = std::get<1>(recv);
   auto commitNumber = std::get<2>(recv);
   TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "clientUID=%llu commitNumber=%lu", ((TR_J9VM *) comp->fe())->_compInfoPT->getMethodBeingCompiled()->getClientUID(),commitNumber);

   TR::ClassTableCriticalSection doUpdate(comp->fe());
   commitModifications(modifyStr);
   commitRemoves(removeStr);

#ifdef COLLECT_CHTABLE_STATS
   uint32_t nBytes = removeStr.size() + modifyStr.size();
   CHTABLE_UPDATE_COUNTER(_updateBytes, nBytes);
   CHTABLE_UPDATE_COUNTER(_numUpdates, 1);
   uint32_t prevMax = _maxUpdateBytes;
   _maxUpdateBytes = std::max(nBytes, _maxUpdateBytes);
#endif
   //if (modifyStr.size() + removeStr.size() != 0)
      //fprintf(stderr, "CHTable updated with %d bytes\n", modifyStr.size() + removeStr.size());
   }

void 
TR_JITaaSServerPersistentCHTable::commitRemoves(std::string &rawData)
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
TR_JITaaSServerPersistentCHTable::commitModifications(std::string &rawData)
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
         clazz = new (PERSISTENT_NEW) TR_PersistentClassInfo(nullptr);
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
      if (flat->_addAll)
         persist->removeSubClasses();
      for (size_t i = 0; i < flat->_numSubClasses; i++)
         {
         auto classInfo = findClassInfo(flat->_subClasses[i]);

         // For some reason the subclass info is still null sometimes. This may be indicative of a larger problem or it could be harmless.
         // JITaaS TODO: figure out why this is happening
         TR_ASSERT(classInfo, "subclass info cannot be null: ensure subclasses are loaded before superclass");
         if (classInfo)
            persist->addSubClass(classInfo);
         else
            fprintf(stderr, "CHTable WARNING: classInfo for subclass %p of %p is null, ignoring\n", flat->_subClasses[i], flat->_classId);
         }
      }
   CHTABLE_UPDATE_COUNTER(_numClassesUpdated, count);
   }

TR_PersistentClassInfo *
TR_JITaaSServerPersistentCHTable::findClassInfo(TR_OpaqueClassBlock * classId)
   {
   CHTABLE_UPDATE_COUNTER(_numQueries, 1);
   auto& data = getData();
   TR_ASSERT(!data.empty(), "CHTable at the server should be initialized by now");
   auto it = data.find(classId);
   if (it != data.end())
      return it->second;
   return nullptr;
   }

TR_PersistentClassInfo *
TR_JITaaSServerPersistentCHTable::findClassInfoAfterLocking(
      TR_OpaqueClassBlock *classId,
      TR::Compilation *comp,
      bool returnClassInfoForAOT)
   {
   if (comp->fej9()->isAOT_DEPRECATED_DO_NOT_USE() && !returnClassInfoForAOT) // for AOT do not use the class hierarchy
      return nullptr;

   if (comp->getOption(TR_DisableCHOpts))
      return nullptr;

   TR::ClassTableCriticalSection findClassInfoAfterLocking(comp->fe());
   return findClassInfo(classId);
   }

std::string
TR_JITaaSClientPersistentCHTable::serializeRemoves()
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
TR_JITaaSClientPersistentCHTable::serializeModifications()
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
      bytesWritten += FlatPersistentClassInfo::serializeClass(clazz, info, false, getAddedSubClasses(clazz->getClassId()));
      count++;
      }
   CHTABLE_UPDATE_COUNTER(_numClassesUpdated, count);

   _dirty.clear();
   _extended.clear();
   return data;
   }

std::pair<std::string, std::string> 
TR_JITaaSClientPersistentCHTable::serializeUpdates()
   {
   TR::ClassTableCriticalSection serializeUpdates(TR::comp()->fe());
   _commitNumber++;
   TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "commitNumber=%lu", _commitNumber);
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
FlatPersistentClassInfo::classSize(TR_PersistentClassInfo *clazz, int32_t numSubClasses)
   {
   // default case, serialize all subclasses
   if (numSubClasses == -1)
      numSubClasses = clazz->getSubClassesCount();
   return sizeof(FlatPersistentClassInfo) + numSubClasses * sizeof(TR_OpaqueClassBlock*);
   }

size_t 
FlatPersistentClassInfo::serializeClass(TR_PersistentClassInfo *clazz, FlatPersistentClassInfo* info, bool addAll, std::vector<TR_OpaqueClassBlock *> *subClasses)
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

   // if there are any subclasses, serialize them now
   info->_addAll = addAll;
   if (!addAll && subClasses)
      {
      // send incremental update of subclasses, i.e. send only subclasses
      // added since last commit
      for (auto it = subClasses->begin(); it != subClasses->end(); it++)
         {
         info->_subClasses[idx++] = *it;
         }
      }
   else
      {
      // send all subclasses. This case is triggered either during the first
      // commit, or after a class is removed and then readded in-between commits
      for (TR_SubClass *c = clazz->getFirstSubclass(); c; c = c->getNext())
         {
         TR_ASSERT(c->getClassInfo(), "Subclass info cannot be null (on client)");
         TR_ASSERT(c->getClassInfo()->getClassId(), "Subclass cannot be null (on client)");
         info->_subClasses[idx++] = c->getClassInfo()->getClassId();
         }
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
   clazz->_classId = info->_classId;
   clazz->_visitedStatus = info->_visitedStatus;
   clazz->_prexAssumptions = info->_prexAssumptions;
   clazz->_timeStamp = info->_timeStamp;
   clazz->_nameLength = info->_nameLength;
   clazz->_flags = info->_flags;
   clazz->_shouldNotBeNewlyExtended = info->_shouldNotBeNewlyExtended;
   clazz->_fieldInfo = nullptr;
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
      TR_PersistentClassInfo *clazz = new (PERSISTENT_NEW) TR_PersistentClassInfo(nullptr);
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



// CLIENT
// TODO: check for race conditions with _dirty/_remove

TR_JITaaSClientPersistentCHTable::TR_JITaaSClientPersistentCHTable(TR_PersistentMemory *trMemory)
   : TR_PersistentCHTable(trMemory)
   , _dirty(decltype(_dirty)::allocator_type(TR::Compiler->persistentAllocator()))
   , _remove(decltype(_remove)::allocator_type(TR::Compiler->persistentAllocator()))
   , _extended(decltype(_extended)::allocator_type(TR::Compiler->persistentAllocator()))
   {
   }

TR_PersistentClassInfo *
TR_JITaaSClientPersistentCHTable::findClassInfo(TR_OpaqueClassBlock * classId)
   {
   markDirty(classId);
   return TR_PersistentCHTable::findClassInfo(classId);
   }

TR_PersistentClassInfo *
TR_JITaaSClientPersistentCHTable::findClassInfoAfterLocking(
      TR_OpaqueClassBlock *classId,
      TR::Compilation *comp,
      bool returnClassInfoForAOT)
   {
   return TR_PersistentCHTable::findClassInfoAfterLocking(classId, comp, returnClassInfoForAOT);
   }

void
TR_JITaaSClientPersistentCHTable::classGotUnloaded(
      TR_FrontEnd *fe,
      TR_OpaqueClassBlock *classId)
   {
   TR_PersistentCHTable::classGotUnloaded(fe, classId);
   }

void
TR_JITaaSClientPersistentCHTable::markSuperClassesAsDirty(
      TR_FrontEnd *fe,
      TR_OpaqueClassBlock *classId)
   {
   J9Class *clazzPtr;
   J9Class *superCl;
   TR_OpaqueClassBlock *superClId;
   int classDepth = TR::Compiler->cls.classDepthOf(classId) - 1;
   if (classDepth >= 0)
      {
      clazzPtr = TR::Compiler->cls.convertClassOffsetToClassPtr(classId);
      superCl = clazzPtr->superclasses[classDepth];
      superClId = ((TR_J9VMBase *)fe)->convertClassPtrToClassOffset(superCl);
      markDirty(superClId);

      for (J9ITable * iTableEntry = (J9ITable *)clazzPtr->iTable; iTableEntry; iTableEntry = iTableEntry->next)
         {
         superCl = iTableEntry->interfaceClass;
         if (superCl != clazzPtr)
            {
            superClId = ((TR_J9VMBase *)fe)->convertClassPtrToClassOffset(superCl);
            markDirty(superClId);
            }
         }
      }
   }

void
TR_JITaaSClientPersistentCHTable::classGotUnloadedPost(
      TR_FrontEnd *fe,
      TR_OpaqueClassBlock *classId)
   {
   markSuperClassesAsDirty(fe, classId);
   TR_PersistentCHTable::classGotUnloadedPost(fe, classId);
   markForRemoval(classId);
   }

void
TR_JITaaSClientPersistentCHTable::classGotRedefined(
      TR_FrontEnd *fe,
      TR_OpaqueClassBlock *oldClassId,
      TR_OpaqueClassBlock *newClassId)
   {
   TR_PersistentCHTable::classGotRedefined(fe, oldClassId, newClassId);
   markDirty(oldClassId);
   markDirty(newClassId);
   }

void
TR_JITaaSClientPersistentCHTable::removeClass(
      TR_FrontEnd *fe,
      TR_OpaqueClassBlock *classId,
      TR_PersistentClassInfo *info,
      bool removeInfo)
   {
   if (!info)
      return;

   markSuperClassesAsDirty(fe, classId);
   TR_PersistentCHTable::removeClass(fe, classId, info, removeInfo);

   if (removeInfo)
      markForRemoval(classId);
   else
      markDirty(classId);
   }

TR_PersistentClassInfo *
TR_JITaaSClientPersistentCHTable::classGotLoaded(
      TR_FrontEnd *fe,
      TR_OpaqueClassBlock *classId)
   {
   markDirty(classId);
   return TR_PersistentCHTable::classGotLoaded(fe, classId);
   }

bool
TR_JITaaSClientPersistentCHTable::classGotInitialized(
      TR_FrontEnd *fe,
      TR_PersistentMemory *persistentMemory,
      TR_OpaqueClassBlock *classId,
      TR_PersistentClassInfo *clazz)
   {
   markDirty(classId);
   return TR_PersistentCHTable::classGotInitialized(fe, persistentMemory, classId, clazz);
   }

bool
TR_JITaaSClientPersistentCHTable::classGotExtended(
      TR_FrontEnd *fe,
      TR_PersistentMemory *persistentMemory,
      TR_OpaqueClassBlock *superClassId,
      TR_OpaqueClassBlock *subClassId)
   {
   markDirty(superClassId);
   bool subClassAdded = TR_PersistentCHTable::classGotExtended(fe, persistentMemory, superClassId, subClassId);
   // if (subClassAdded)
      // markExtended(superClassId, subClassId);
   return subClassAdded;
   }


// these two tables should be mutually exclusive - we only keep the most recent entry.
void 
TR_JITaaSClientPersistentCHTable::markForRemoval(TR_OpaqueClassBlock *clazz)
   {
   _remove.insert(clazz);
   _dirty.erase(clazz);
   }

void 
TR_JITaaSClientPersistentCHTable::markDirty(TR_OpaqueClassBlock *clazz)
   {
   _dirty.insert(clazz);
   _remove.erase(clazz);
   // if ((auto it = _extended.find(clazz)) != _extended.end())
      // {
      // // class marked dirty after subclasses were added in-between CH table commits.
      // // this can happen when a subclass gets added and then removed in-between commits.
      // // in this case, send all of the subclasses
      // it->second.clear();
      // for (TR_SubClass *c = classInfo->getFirstSubclass(); c; c = c->getNext())
         // {
         // TR_ASSERT(c->getClassInfo(), "Subclass info cannot be null (on client)");
         // TR_ASSERT(c->getClassInfo()->getClassId(), "Subclass cannot be null (on client)");
         // markExtended(clazz, c->getClassInfo()->getClassId());
         // }
      // }
   }

void 
TR_JITaaSClientPersistentCHTable::markExtended(TR_OpaqueClassBlock *parentClass, TR_OpaqueClassBlock *subClass)
   {
   // increment the number of added subclasses
   auto it = _extended.find(parentClass);
   if (it == _extended.end())
      {
      std::vector<TR_OpaqueClassBlock *> subClassList;
      subClassList.push_back(subClass);
      _extended.insert({parentClass, subClassList});
      }
   else
      it->second.push_back(subClass);
   }

uint32_t
TR_JITaaSClientPersistentCHTable::getNumAddedSubClasses(TR_OpaqueClassBlock *clazz)
   {
   auto it = _extended.find(clazz);
   return it != _extended.end() ? it->second.size() : 0;
   }

std::vector<TR_OpaqueClassBlock *> *
TR_JITaaSClientPersistentCHTable::getAddedSubClasses(TR_OpaqueClassBlock *clazz)
   {
   auto it = _extended.find(clazz);
   return it != _extended.end() ? &it->second : nullptr; 
   }
