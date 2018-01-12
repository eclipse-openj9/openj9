#include "JaasPersistentCHTable.hpp"
#include "compile/Compilation.hpp"
#include "control/CompilationRuntime.hpp"
#include "J9Server.h"
#include "env/ClassTableCriticalSection.hpp"   // for ClassTableCriticalSection


// plan: send the whole table once,
// then on the client mark entires as dirty when they are modified
// and send dirty entries to the server periodically

TR_JaasServerPersistentCHTable::TR_JaasServerPersistentCHTable(TR_PersistentMemory *trMemory)
   : TR_PersistentCHTable(trMemory)
   {
   }

TR_JaasServerPersistentCHTable::InternalData &TR_JaasServerPersistentCHTable::getData()
   {
   return _data[TR::comp()->getCompThreadID()];
   }

void TR_JaasServerPersistentCHTable::initializeIfNeeded()
   {
   auto& data = getData();
   if (data.initialized)
      return;

   auto stream = TR::CompilationInfo::getStream();
   stream->write(JAAS::J9ServerMessageType::CHTable_getAllClassInfo, JAAS::Void());
   std::string rawData = std::get<0>(stream->read<std::string>());
   auto infos = FlatPersistentClassInfo::deserialize(rawData);
   for (auto clazz : infos)
      {
      data.classMap.insert({clazz->getClassId(), clazz});
      }
   data.initialized = true;
   fprintf(stderr, "Initialized CHTable with %d classes\n", infos.size());
   }

TR_PersistentClassInfo *
TR_JaasServerPersistentCHTable::findClassInfo(TR_OpaqueClassBlock * classId)
   {
   return findClassInfoAfterLocking(classId, TR::comp());
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

   initializeIfNeeded();
   auto& data = getData();
   auto it = data.classMap.find(classId);
   if (it != data.classMap.end())
      return it->second;
   return nullptr;
   }

void collectHierarchy(std::unordered_set<TR_PersistentClassInfo*> &out, TR_PersistentClassInfo *root)
   {
   out.insert(root);
   for (TR_SubClass *c = root->getFirstSubclass(); c; c = c->getNext())
      collectHierarchy(out, c->getClassInfo());
   }

std::string FlatPersistentClassInfo::serialize(TR_PersistentClassInfo *root)
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
      auto numSubClasses = clazz->_subClasses.getSize();
      size_t size = sizeof(FlatPersistentClassInfo) + numSubClasses * sizeof(TR_OpaqueClassBlock*);
      numBytes += size;
      }

   // Serialize to string
   std::string data(numBytes, '\0');
   size_t bytesWritten = 0;
   for (auto clazz : classes)
      {
      FlatPersistentClassInfo* info = (FlatPersistentClassInfo*)&data[bytesWritten];
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
      bytesWritten += sizeof(TR_OpaqueClassBlock*) * idx;
      bytesWritten += sizeof(FlatPersistentClassInfo);
      }
   return data;
   }

std::vector<TR_PersistentClassInfo*> FlatPersistentClassInfo::deserialize(std::string& data)
   {
   std::vector<TR_PersistentClassInfo*> out;
   std::unordered_map<TR_OpaqueClassBlock*, std::pair<FlatPersistentClassInfo*, TR_PersistentClassInfo*>> infoMap;

   size_t bytesRead = 0;
   while (bytesRead != data.length())
      {
      TR_ASSERT(bytesRead < data.length(), "Corrupt CHTable!!");
      FlatPersistentClassInfo* info = (FlatPersistentClassInfo*)&data[bytesRead];
      TR_PersistentClassInfo *clazz = new (PERSISTENT_NEW) TR_PersistentClassInfo(nullptr);
      clazz->_classId = info->_classId;
      clazz->_visitedStatus = info->_visitedStatus;
      clazz->_prexAssumptions = info->_prexAssumptions;
      clazz->_timeStamp = info->_timeStamp;
      clazz->_nameLength = info->_nameLength;
      clazz->_flags = info->_flags;
      clazz->_shouldNotBeNewlyExtended = info->_shouldNotBeNewlyExtended;
      out.push_back(clazz);
      infoMap.insert({clazz->getClassId(), {info, clazz}});
      bytesRead += sizeof(FlatPersistentClassInfo) + info->_numSubClasses * sizeof(TR_OpaqueClassBlock*);
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
