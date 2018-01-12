#include "env/PersistentCHTable.hpp"
#include "env/CHTable.hpp"
#include <unordered_map>

class TR_JaasServerPersistentCHTable : public TR_PersistentCHTable
   {
public:
   TR_ALLOC(TR_Memory::PersistentCHTable)

   TR_JaasServerPersistentCHTable(TR_PersistentMemory *);

   void initializeIfNeeded();

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
   struct InternalData
      {
      bool initialized = false;
      std::unordered_map<TR_OpaqueClassBlock*, TR_PersistentClassInfo*> classMap;
      };

   InternalData &getData();

   // one per compilation thread
   InternalData _data[8];
   };



class FlatPersistentClassInfo
   {
public:
   static std::string serialize(TR_PersistentClassInfo *orig);
   static std::vector<TR_PersistentClassInfo*> deserialize(std::string& data);


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
