#ifndef JAAS_PERSISTENT_CHTABLE_H
#define JAAS_PERSISTENT_CHTABLE_H

#include "env/PersistentCHTable.hpp"
#include "env/CHTable.hpp"
#include <unordered_map>
#include <unordered_set>

class TR_JaasServerPersistentCHTable : public TR_PersistentCHTable
   {
public:
   TR_ALLOC(TR_Memory::PersistentCHTable)

   TR_JaasServerPersistentCHTable(TR_PersistentMemory *);

   void initializeIfNeeded(TR::Compilation *comp);
   void doUpdate(TR::Compilation *comp);

   virtual TR_PersistentClassInfo * findClassInfo(TR_OpaqueClassBlock * classId);
   virtual TR_PersistentClassInfo * findClassInfoAfterLocking(TR_OpaqueClassBlock * classId, TR::Compilation *, bool returnClassInfoForAOT = false);

   /*
   virtual void commitSideEffectGuards(TR::Compilation *c);
   virtual bool isOverriddenInThisHierarchy(TR_ResolvedMethod *, TR_OpaqueClassBlock *, int32_t, TR::Compilation *comp, bool locked = false);

   virtual TR_ResolvedMethod * findSingleInterfaceImplementer(TR_OpaqueClassBlock *, int32_t, TR_ResolvedMethod *, TR::Compilation *, bool locked = false);
   virtual TR_ResolvedMethod * findSingleAbstractImplementer(TR_OpaqueClassBlock *, int32_t, TR_ResolvedMethod *, TR::Compilation *, bool locked = false);

   // profiler
   virtual bool isKnownToHaveMoreThanTwoInterfaceImplementers(TR_OpaqueClassBlock *, int32_t, TR_ResolvedMethod *, TR::Compilation *, bool locked = false);

   // optimizer
   virtual TR_OpaqueClassBlock *findSingleConcreteSubClass(TR_OpaqueClassBlock *, TR::Compilation *);
   virtual TR_ResolvedMethod * findSingleImplementer(TR_OpaqueClassBlock * thisClass, int32_t cpIndexOrVftSlot, TR_ResolvedMethod * callerMethod, TR::Compilation * comp, bool locked, TR_YesNoMaybe useGetResolvedInterfaceMethod);

   virtual bool hasTwoOrMoreCompiledImplementors(TR_OpaqueClassBlock *, int32_t, TR_ResolvedMethod *, TR::Compilation *, TR_Hotness hotness = warm, bool locked = false);
   virtual int32_t findnInterfaceImplementers(TR_OpaqueClassBlock *, int32_t, TR_ResolvedMethod *implArray[], int32_t, TR_ResolvedMethod *, TR::Compilation *, bool locked = false);
   virtual TR_ResolvedMethod * findSingleJittedImplementer(TR_OpaqueClassBlock *, int32_t, TR_ResolvedMethod *, TR::Compilation *, TR::ResolvedMethodSymbol*, bool locked = false);

   // J9 below

   virtual void methodGotOverridden(TR_FrontEnd *, TR_PersistentMemory *, TR_OpaqueMethodBlock *overriddingMethod, TR_OpaqueMethodBlock *overriddenMethod, int32_t smpFlag);
   virtual TR_PersistentClassInfo *classGotLoaded(TR_FrontEnd *, TR_OpaqueClassBlock *classId);
   virtual bool classGotInitialized(TR_FrontEnd* vm, TR_PersistentMemory *, TR_OpaqueClassBlock *classId, TR_PersistentClassInfo *clazz = 0);
   virtual bool classGotExtended(TR_FrontEnd *vm, TR_PersistentMemory *, TR_OpaqueClassBlock *superClassId, TR_OpaqueClassBlock *subClassId);
   virtual void classGotUnloaded(TR_FrontEnd *vm, TR_OpaqueClassBlock *classId);
   virtual void classGotUnloadedPost(TR_FrontEnd *fe, TR_OpaqueClassBlock *classId);
   virtual void classGotRedefined(TR_FrontEnd *vm, TR_OpaqueClassBlock *oldClassId, TR_OpaqueClassBlock *newClassId);
   virtual void removeClass(TR_FrontEnd *, TR_OpaqueClassBlock *classId, TR_PersistentClassInfo *info, bool removeInfo);
   virtual void resetVisitedClasses(); // highly time consumming
*/
private:
   void commitRemoves(TR::Compilation *comp, std::string &data);
   void commitModifications(TR::Compilation *comp, std::string &data);
   void cleanUpExpiredData();

   struct InternalData
      {
      bool initialized = false;
      int64_t lastTime = 0;
      std::unordered_map<TR_OpaqueClassBlock*, TR_PersistentClassInfo*> classMap;
      };

   InternalData &getData(TR::Compilation *comp);

   // one per compilation thread
   std::unordered_map<uint64_t, InternalData> _data;
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
/*
   virtual void methodGotOverridden(TR_FrontEnd *, TR_PersistentMemory *, TR_OpaqueMethodBlock *overriddingMethod, TR_OpaqueMethodBlock *overriddenMethod, int32_t smpFlag);
   virtual bool classGotInitialized(TR_FrontEnd* vm, TR_PersistentMemory *, TR_OpaqueClassBlock *classId, TR_PersistentClassInfo *clazz = 0);
   virtual bool classGotExtended(TR_FrontEnd *vm, TR_PersistentMemory *, TR_OpaqueClassBlock *superClassId, TR_OpaqueClassBlock *subClassId);
   virtual void classGotUnloaded(TR_FrontEnd *vm, TR_OpaqueClassBlock *classId);
*/
   virtual TR_PersistentClassInfo *classGotLoaded(TR_FrontEnd *, TR_OpaqueClassBlock *classId);
   virtual void classGotRedefined(TR_FrontEnd *vm, TR_OpaqueClassBlock *oldClassId, TR_OpaqueClassBlock *newClassId);
   virtual void classGotUnloadedPost(TR_FrontEnd *fe, TR_OpaqueClassBlock *classId);
   virtual void removeClass(TR_FrontEnd *, TR_OpaqueClassBlock *classId, TR_PersistentClassInfo *info, bool removeInfo);
   virtual void resetVisitedClasses(); // highly time consumming

private:
   void markForRemoval(TR_OpaqueClassBlock *clazz);
   void markDirty(TR_OpaqueClassBlock *clazz);

   std::string serializeRemoves();
   std::string serializeModifications();

   std::unordered_set<TR_OpaqueClassBlock*> _dirty;
   std::unordered_set<TR_OpaqueClassBlock*> _remove;
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
