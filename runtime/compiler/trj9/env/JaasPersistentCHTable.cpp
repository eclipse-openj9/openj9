#include "JaasPersistentCHTable.hpp"
#include "compile/Compilation.hpp"
#include "control/CompilationRuntime.hpp"
#include "J9Server.h"
#include "env/ClassTableCriticalSection.hpp"   // for ClassTableCriticalSection


// plan: send the whole table once,
// then on the client mark entires as dirty when they are modified
// and send dirty entries to the server periodically


// SERVER

TR_JaasServerPersistentCHTable::TR_JaasServerPersistentCHTable(TR_PersistentMemory *trMemory)
   : TR_PersistentCHTable(trMemory)
   {
   }

TR_JaasServerPersistentCHTable::InternalData &TR_JaasServerPersistentCHTable::getData(TR::Compilation *comp)
   {
   auto stream = TR::CompilationInfo::getStream();
   uint64_t clientId = stream->getClientId();
   auto &data = _data[clientId];
   int64_t time = TR::Compiler->vm.getUSecClock()/1000000; // date +%s
   data.lastTime = time;
   return data;
   }

void TR_JaasServerPersistentCHTable::cleanUpExpiredData()
   {
   int64_t time = TR::Compiler->vm.getUSecClock()/1000000; // date +%s
   for (auto it = _data.begin(); it != _data.end();)
      {
      if (time - it->second.lastTime > 60 * 5) // entries are freed after 5 minutes
	 {
	 it = _data.erase(it);
	 }
      else
	 {
	 it++;
	 }
      }
   }

void TR_JaasServerPersistentCHTable::initializeIfNeeded(TR::Compilation *comp)
   {
   if (comp->getOption(TR_DisableCHOpts))
      return;

   auto& data = getData(comp);
   if (data.initialized)
      return;

   auto stream = TR::CompilationInfo::getStream();
   stream->write(JAAS::J9ServerMessageType::CHTable_getAllClassInfo, JAAS::Void());
   std::string rawData = std::get<0>(stream->read<std::string>());
   auto infos = FlatPersistentClassInfo::deserializeHierarchy(rawData);
   for (auto clazz : infos)
      {
      data.classMap.insert({clazz->getClassId(), clazz});
      }
   data.initialized = true;
   fprintf(stderr, "Initialized CHTable with %d classes\n", infos.size());
   }

void TR_JaasServerPersistentCHTable::doUpdate(TR::Compilation *comp)
   {
   if (comp->getOption(TR_DisableCHOpts))
      return;

      {
      TR::ClassTableCriticalSection doUpdate(comp->fe());
      initializeIfNeeded(comp);
      cleanUpExpiredData();
      }

   auto stream = TR::CompilationInfo::getStream();
   stream->write(JAAS::J9ServerMessageType::CHTable_getClassInfoUpdates, JAAS::Void());
   auto recv = stream->read<std::string, std::string>();
   std::string &removeStr = std::get<0>(recv);
   std::string &modifyStr = std::get<1>(recv);

   TR::ClassTableCriticalSection doUpdate(comp->fe());
   commitModifications(comp, modifyStr);
   commitRemoves(comp, removeStr);
   //if (modifyStr.size() + removeStr.size() != 0)
      //fprintf(stderr, "CHTable updated with %d bytes\n", modifyStr.size() + removeStr.size());
   }

void TR_JaasServerPersistentCHTable::commitRemoves(TR::Compilation *comp, std::string &rawData)
   {
   auto &data = getData(comp);
   TR_OpaqueClassBlock **ptr = (TR_OpaqueClassBlock**)&rawData[0];
   size_t num = rawData.size() / sizeof(TR_OpaqueClassBlock*);
   for (size_t i = 0; i < num; i++)
      {
      auto item = data.classMap[ptr[i]];
      data.classMap.erase(ptr[i]);
      if (item) // may have already been removed earlier in this update block
         jitPersistentFree(item);
      }
   }

void TR_JaasServerPersistentCHTable::commitModifications(TR::Compilation *comp, std::string &rawData)
   {
   auto &data = getData(comp);
   std::unordered_map<TR_OpaqueClassBlock*, std::pair<FlatPersistentClassInfo*, TR_PersistentClassInfo*>> infoMap;

   size_t bytesRead = 0;
   while (bytesRead != rawData.length())
      {
      TR_ASSERT(bytesRead < rawData.length(), "Corrupt CHTable!!");
      FlatPersistentClassInfo *info = (FlatPersistentClassInfo*)&rawData[bytesRead];
      TR_OpaqueClassBlock *classId = (TR_OpaqueClassBlock *) (((uintptr_t) info->_classId) & ~(uintptr_t)1);
      TR_PersistentClassInfo* clazz = findClassInfo(classId);
      if (!clazz)
         {
         clazz = new (PERSISTENT_NEW) TR_PersistentClassInfo(nullptr);
         data.classMap.insert({classId, clazz});
         }
      infoMap.insert({classId, {info, clazz}});
      bytesRead += FlatPersistentClassInfo::deserializeClassSimple(clazz, info);
      }

   // populate subclasses
   for (auto it : infoMap)
      {
      auto flat = it.second.first;
      auto persist = it.second.second;
      for (size_t i = 0; i < flat->_numSubClasses; i++)
         {
         persist->addSubClass(findClassInfo(flat->_subClasses[i]));
         }
      }
   }

TR_PersistentClassInfo *
TR_JaasServerPersistentCHTable::findClassInfo(TR_OpaqueClassBlock * classId)
   {
   initializeIfNeeded(TR::comp());
   auto& data = getData(TR::comp());
   auto it = data.classMap.find(classId);
   if (it != data.classMap.end())
      return it->second;
   return nullptr;
   }

TR_PersistentClassInfo *
TR_JaasServerPersistentCHTable::findClassInfoAfterLocking(
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
TR_JaasClientPersistentCHTable::serializeRemoves()
   {
   size_t outputSize = _remove.size() * sizeof(TR_OpaqueClassBlock*);
   std::string data(outputSize, '\0');
   TR_OpaqueClassBlock** ptr = (TR_OpaqueClassBlock**) &data[0];

   for (auto clazz : _remove)
      {
      *ptr++ = clazz;
      // Also remove removes from dirty table so they don't get recreated
      _dirty.erase(clazz);
      }
   _remove.clear();

   return data;
   }

std::string
TR_JaasClientPersistentCHTable::serializeModifications()
   {
   size_t numBytes = 0;
   for (auto classId : _dirty)
      {
      auto clazz = findClassInfo(classId);
      size_t size = FlatPersistentClassInfo::classSize(clazz);
      numBytes += size;
      }

   std::string data(numBytes, '\0');

   size_t bytesWritten = 0;
   for (auto classId : _dirty)
      {
      auto clazz = findClassInfo(classId);
      FlatPersistentClassInfo* info = (FlatPersistentClassInfo*)&data[bytesWritten];
      bytesWritten += FlatPersistentClassInfo::serializeClass(clazz, info);
      }

   _dirty.clear();
   return data;
   }

std::pair<std::string, std::string> TR_JaasClientPersistentCHTable::serializeUpdates()
   {
   TR::ClassTableCriticalSection serializeUpdates(TR::comp()->fe());
   std::string removes = serializeRemoves(); // must be called first
   std::string mods = serializeModifications();
   return {removes, mods};
   }

void collectHierarchy(std::unordered_set<TR_PersistentClassInfo*> &out, TR_PersistentClassInfo *root)
   {
   out.insert(root);
   for (TR_SubClass *c = root->getFirstSubclass(); c; c = c->getNext())
      collectHierarchy(out, c->getClassInfo());
   }

size_t FlatPersistentClassInfo::classSize(TR_PersistentClassInfo *clazz)
   {
   auto numSubClasses = clazz->_subClasses.getSize();
   return sizeof(FlatPersistentClassInfo) + numSubClasses * sizeof(TR_OpaqueClassBlock*);
   }
size_t FlatPersistentClassInfo::serializeClass(TR_PersistentClassInfo *clazz, FlatPersistentClassInfo* info)
   {
   info->_classId = clazz->_classId;
   info->_visitedStatus = clazz->_visitedStatus;
   info->_prexAssumptions = clazz->_prexAssumptions;
   info->_timeStamp = clazz->_timeStamp;
   info->_nameLength = clazz->_nameLength;
   info->_flags = clazz->_flags;
   info->_shouldNotBeNewlyExtended = clazz->_shouldNotBeNewlyExtended;
   int idx = 0;
   for (TR_SubClass *c = clazz->getFirstSubclass(); c; c = c->getNext())
      {
      info->_subClasses[idx++] = c->getClassInfo()->getClassId();
      }
   info->_numSubClasses = idx;
   return sizeof(TR_OpaqueClassBlock*) * idx + sizeof(FlatPersistentClassInfo);
   }

std::string FlatPersistentClassInfo::serializeHierarchy(TR_PersistentClassInfo *root)
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
size_t FlatPersistentClassInfo::deserializeClassSimple(TR_PersistentClassInfo *clazz, FlatPersistentClassInfo *info)
   {
   clazz->_classId = info->_classId;
   clazz->_visitedStatus = info->_visitedStatus;
   clazz->_prexAssumptions = info->_prexAssumptions;
   clazz->_timeStamp = info->_timeStamp;
   clazz->_nameLength = info->_nameLength;
   clazz->_flags = info->_flags;
   clazz->_shouldNotBeNewlyExtended = info->_shouldNotBeNewlyExtended;
   return sizeof(FlatPersistentClassInfo) + info->_numSubClasses * sizeof(TR_OpaqueClassBlock*);
   }

std::vector<TR_PersistentClassInfo*> FlatPersistentClassInfo::deserializeHierarchy(std::string& data)
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

TR_JaasClientPersistentCHTable::TR_JaasClientPersistentCHTable(TR_PersistentMemory *trMemory)
   : TR_PersistentCHTable(trMemory)
   {
   }

TR_PersistentClassInfo *
TR_JaasClientPersistentCHTable::findClassInfoConst(TR_OpaqueClassBlock * classId)
   {
   return TR_PersistentCHTable::findClassInfo(classId);
   }

TR_PersistentClassInfo *
TR_JaasClientPersistentCHTable::findClassInfoAfterLockingConst(
      TR_OpaqueClassBlock *classId,
      TR::Compilation *comp,
      bool returnClassInfoForAOT)
   {
   return TR_PersistentCHTable::findClassInfoAfterLocking(classId, comp, returnClassInfoForAOT);
   }

TR_PersistentClassInfo *
TR_JaasClientPersistentCHTable::findClassInfo(TR_OpaqueClassBlock * classId)
   {
      //{
      //TR::ClassTableCriticalSection findClassInfo(TR::comp()->fe());
      //markDirty(classId);
      //}
   return TR_PersistentCHTable::findClassInfo(classId);
   }

TR_PersistentClassInfo *
TR_JaasClientPersistentCHTable::findClassInfoAfterLocking(
      TR_OpaqueClassBlock *classId,
      TR::Compilation *comp,
      bool returnClassInfoForAOT)
   {
      {
      TR::ClassTableCriticalSection findClassInfo(comp->fe());
      markDirty(classId);
      }
   return TR_PersistentCHTable::findClassInfoAfterLocking(classId, comp, returnClassInfoForAOT);
   }

void
TR_JaasClientPersistentCHTable::classGotUnloadedPost(
      TR_FrontEnd *fe,
      TR_OpaqueClassBlock *classId)
   {
   TR_PersistentCHTable::classGotUnloadedPost(fe, classId);

   markForRemoval(classId);
   }

void
TR_JaasClientPersistentCHTable::classGotRedefined(
      TR_FrontEnd *fe,
      TR_OpaqueClassBlock *oldClassId,
      TR_OpaqueClassBlock *newClassId)
   {
   TR_ASSERT(false, "redefining classes is currently not supported");
   }

void
TR_JaasClientPersistentCHTable::removeClass(
      TR_FrontEnd *fe,
      TR_OpaqueClassBlock *classId,
      TR_PersistentClassInfo *info,
      bool removeInfo)
   {
   if (!info)
      return;

   TR_PersistentCHTable::removeClass(fe, classId, info, removeInfo);

   if (removeInfo)
      markForRemoval(classId);
   else
      markDirty(classId);
   }

TR_PersistentClassInfo *
TR_JaasClientPersistentCHTable::classGotLoaded(
      TR_FrontEnd *fe,
      TR_OpaqueClassBlock *classId)
   {
   markDirty(classId);
   return TR_PersistentCHTable::classGotLoaded(fe, classId);
   }

void
TR_JaasClientPersistentCHTable::resetVisitedClasses() // highly time consumming
   {
   TR_ASSERT(false, "should not call resetVisitedClasses on client");
   }


void TR_JaasClientPersistentCHTable::markForRemoval(TR_OpaqueClassBlock *clazz)
   {
   //TR::ClassTableCriticalSection markForRemoval(TR::comp()->fe());
   _remove.insert(clazz);
   }
void TR_JaasClientPersistentCHTable::markDirty(TR_OpaqueClassBlock *clazz)
   {
   //TR::ClassTableCriticalSection markDirty(TR::comp()->fe());
   _dirty.insert(clazz);
   }
