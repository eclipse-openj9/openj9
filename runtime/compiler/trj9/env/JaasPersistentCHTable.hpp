#ifndef JAAS_PERSISTENT_CHTABLE_H
#define JAAS_PERSISTENT_CHTABLE_H

#include "env/PersistentCHTable.hpp"
#include "env/PersistentCollections.hpp"
#include "env/CHTable.hpp"
#include <unordered_map>
#include <unordered_set>

#define COLLECT_CHTABLE_STATS

#ifdef COLLECT_CHTABLE_STATS
#define CHTABLE_UPDATE_COUNTER(counter, value) counter += value
#else
#define CHTABLE_UPDATE_COUNTER(counter, value)
#endif


class TR_JaasServerPersistentCHTable : public TR_PersistentCHTable
   {
public:
   TR_ALLOC(TR_Memory::PersistentCHTable)

   TR_JaasServerPersistentCHTable(TR_PersistentMemory *);

   void initializeIfNeeded(TR::Compilation *comp);
   void doUpdate(TR::Compilation *comp);

   virtual TR_PersistentClassInfo * findClassInfo(TR_OpaqueClassBlock * classId);
   virtual TR_PersistentClassInfo * findClassInfoAfterLocking(TR_OpaqueClassBlock * classId, TR::Compilation *, bool returnClassInfoForAOT = false);

#ifdef COLLECT_CHTABLE_STATS
   // Statistical counters
   uint32_t _numUpdates; // aka numCompilations
   uint32_t _numClassesUpdated;
   uint32_t _numClassesRemoved;
   uint32_t _numQueries;
   uint32_t _updateBytes;
   uint32_t _maxUpdateBytes;
#endif

private:
   void commitRemoves(TR::Compilation *comp, std::string &data);
   void commitModifications(TR::Compilation *comp, std::string &data);

   PersistentUnorderedMap<TR_OpaqueClassBlock*, TR_PersistentClassInfo*> &getData(TR::Compilation *comp);
   };

class TR_JaasClientPersistentCHTable : public TR_PersistentCHTable
   {
public:
   TR_ALLOC(TR_Memory::PersistentCHTable)

   TR_JaasClientPersistentCHTable(TR_PersistentMemory *);

   std::pair<std::string, std::string> serializeUpdates();

   virtual TR_PersistentClassInfo * findClassInfo(TR_OpaqueClassBlock * classId);
   virtual TR_PersistentClassInfo * findClassInfoAfterLocking(TR_OpaqueClassBlock * classId, TR::Compilation *, bool returnClassInfoForAOT = false);
   TR_PersistentClassInfo * findClassInfoConst(TR_OpaqueClassBlock * classId);
   TR_PersistentClassInfo * findClassInfoAfterLockingConst(TR_OpaqueClassBlock * classId, TR::Compilation *, bool returnClassInfoForAOT = false);

   virtual TR_PersistentClassInfo *classGotLoaded(TR_FrontEnd *, TR_OpaqueClassBlock *classId);
   virtual void classGotRedefined(TR_FrontEnd *vm, TR_OpaqueClassBlock *oldClassId, TR_OpaqueClassBlock *newClassId);
   virtual void classGotUnloadedPost(TR_FrontEnd *fe, TR_OpaqueClassBlock *classId);
   virtual void removeClass(TR_FrontEnd *, TR_OpaqueClassBlock *classId, TR_PersistentClassInfo *info, bool removeInfo);
   virtual bool classGotExtended(TR_FrontEnd *vm, TR_PersistentMemory *, TR_OpaqueClassBlock *superClassId, TR_OpaqueClassBlock *subClassId);
   virtual void resetVisitedClasses(); // highly time consumming

#ifdef COLLECT_CHTABLE_STATS
   uint32_t _numUpdates; // aka numCompilations
   uint32_t _numCommitFailures;
   uint32_t _numClassesUpdated;
   uint32_t _numClassesRemoved;
   uint32_t _updateBytes;
   uint32_t _maxUpdateBytes;
#endif

private:
   void markForRemoval(TR_OpaqueClassBlock *clazz);
   void markDirty(TR_OpaqueClassBlock *clazz);

   std::string serializeRemoves();
   std::string serializeModifications();

   PersistentUnorderedSet<TR_OpaqueClassBlock*> _dirty;
   PersistentUnorderedSet<TR_OpaqueClassBlock*> _remove;
   };

class FlatPersistentClassInfo
   {
public:
   static std::string serializeHierarchy(TR_PersistentClassInfo *orig);
   static std::vector<TR_PersistentClassInfo*> deserializeHierarchy(std::string& data);
   static size_t classSize(TR_PersistentClassInfo *clazz);
   static size_t serializeClass(TR_PersistentClassInfo *clazz, FlatPersistentClassInfo* info);
   static size_t deserializeClassSimple(TR_PersistentClassInfo *clazz, FlatPersistentClassInfo *info);

   TR_OpaqueClassBlock                *_classId;

   union
      {
      uintptrj_t _visitedStatus;
      TR_PersistentClassInfoForFields *_fieldInfo;
      };
   int16_t                             _prexAssumptions;
   uint16_t                            _timeStamp;
   int32_t                             _nameLength;
   flags8_t                            _flags;
   flags8_t                            _shouldNotBeNewlyExtended; // one bit for each possible compilation thread

   uint32_t                            _numSubClasses;
   TR_OpaqueClassBlock                *_subClasses[0];
   };

#endif // JAAS_PERSISTENT_CHTABLE_H
