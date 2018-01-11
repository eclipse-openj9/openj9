#include "env/PersistentCHTable.hpp"

class TR_JaasPersistentCHTable : public TR_PersistentCHTable
   {
public:
   TR_ALLOC(TR_Memory::PersistentCHTable)

   TR_JaasPersistentCHTable(TR_PersistentMemory *);

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
   };
