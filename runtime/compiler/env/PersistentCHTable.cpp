/*******************************************************************************
 * Copyright (c) 2000, 2021 IBM Corp. and others
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

#include "env/PersistentCHTable.hpp"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CHTable.hpp"
#include "env/CompilerEnv.hpp"
#include "env/PersistentInfo.hpp"
#include "env/RuntimeAssumptionTable.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "env/ClassTableCriticalSection.hpp"
#include "env/VMJ9.h"
#include "env/VerboseLog.hpp"
#include "il/DataTypes.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "runtime/RuntimeAssumptions.hpp"

class TR_OpaqueClassBlock;

TR_PersistentCHTable::TR_PersistentCHTable(TR_PersistentMemory *trPersistentMemory)
   : _trPersistentMemory(trPersistentMemory)
   {
   /*
    * We want to avoid strange memory allocation failures that might occur in a
    * constructor. This can be done by making  _classes an array that gets
    * allocated as part of the TR_PersistentCHTable  class itself.  But that
    * invokes libstdc++. So making a byte array as part of the
    * TR_PersistentCHTable class and pointing _classes (static type casting) to
    * the byte array memory region, after initializing, also does the same work.
    */

   memset(_buffer, 0, sizeof(TR_LinkHead<TR_PersistentClassInfo>) * (CLASSHASHTABLE_SIZE + 1));
   _classes = static_cast<TR_LinkHead<TR_PersistentClassInfo> *>(static_cast<void *>(_buffer));

   setActive();
   }


void
TR_PersistentCHTable::commitSideEffectGuards(TR::Compilation *comp)
   {
   if (!isActive())
      {
      TR_ASSERT_FATAL(comp->getClassesThatShouldNotBeLoaded()->isEmpty(), "Classes that should not be loaded is not empty!");
      TR_ASSERT_FATAL(comp->getClassesThatShouldNotBeNewlyExtended()->isEmpty(), "Classes that should not be newly extended is not empty!");
      TR_ASSERT_FATAL(comp->getSideEffectGuardPatchSites()->empty(), "Side effect Guard patch sites not empty!");
      return;
      }

   TR::list<TR_VirtualGuardSite*> *sideEffectPatchSites = comp->getSideEffectGuardPatchSites();
   bool nopAssumptionIsValid = true;

   for (TR_ClassLoadCheck * clc = comp->getClassesThatShouldNotBeLoaded()->getFirst(); clc; clc = clc->getNext())
      {
      TR_OpaqueClassBlock *clazz = comp->fej9()->getClassFromSignature(clc->_name, clc->_length, comp->getCurrentMethod());
      if (clazz)
         {
         TR_PersistentClassInfo * classInfo = findClassInfoAfterLocking(clazz, comp);
         if (classInfo && classInfo->isInitialized())
            {
            nopAssumptionIsValid = false;
            break;
            }
         }
      }

   if (nopAssumptionIsValid)
      {
      for (TR_ClassExtendCheck * cec = comp->getClassesThatShouldNotBeNewlyExtended()->getFirst(); cec; cec = cec->getNext())
         {
         TR_OpaqueClassBlock *clazz = cec->_clazz;

         if (comp->fe()->classHasBeenExtended(clazz))
            {
            TR_PersistentClassInfo * cl = findClassInfo(clazz);
            TR_ScratchList<TR_PersistentClassInfo> subClasses(comp->trMemory());
            TR_ClassQueries::collectAllSubClasses(cl, &subClasses, comp);
            ListIterator<TR_PersistentClassInfo> it(&subClasses);
            TR_PersistentClassInfo *info;
            for (info = it.getFirst(); info; info = it.getNext())
               {
               TR_OpaqueClassBlock *subClass = info->getClassId();
               bool newSubClass = true;
               for (TR_ClassExtendCheck *currCec = comp->getClassesThatShouldNotBeNewlyExtended()->getFirst(); currCec; currCec = currCec->getNext())
                  {
                  if (subClass == currCec->_clazz)
                     {
                     newSubClass = false;
                     break;
                     }
                  }

               if (newSubClass)
                  {
                  break;
                  }
               }

            if (info)
               {
               nopAssumptionIsValid = false;
               break;
               }
            }
         }
      }

   if (nopAssumptionIsValid)
      {
      for (TR_ClassLoadCheck * clc = comp->getClassesThatShouldNotBeLoaded()->getFirst(); clc; clc = clc->getNext())
         {
         for (auto site = sideEffectPatchSites->begin(); site != sideEffectPatchSites->end(); ++site)
            {
            TR_ASSERT((*site)->getLocation(), "assertion failure");
            TR_PatchNOPedGuardSiteOnClassPreInitialize::make
               (comp->fe(), comp->trPersistentMemory(), clc->_name, clc->_length, (*site)->getLocation(),
                (*site)->getDestination(), comp->getMetadataAssumptionList());
            comp->setHasClassPreInitializeAssumptions();
            }
         }

      for (TR_ClassExtendCheck * cec = comp->getClassesThatShouldNotBeNewlyExtended()->getFirst(); cec; cec = cec->getNext())
         {
         TR_OpaqueClassBlock *clazz = cec->_clazz;
         TR_PersistentClassInfo * cl = findClassInfo(clazz);

         for (auto site = sideEffectPatchSites->begin(); site != sideEffectPatchSites->end(); ++site)
            {
            TR_ASSERT((*site)->getLocation(), "assertion failure");
            if (cl)
               {
               TR_PatchNOPedGuardSiteOnClassExtend::make(comp->fe(), comp->trPersistentMemory(), clazz,
                                                                        (*site)->getLocation(),
                                                                        (*site)->getDestination(),
                                                                        comp->getMetadataAssumptionList());
               comp->setHasClassExtendAssumptions();
               }
            else
               TR_ASSERT(0, "Could not find class info for class that should not be newly extended\n");
            }
         }
      }
   else
      {
      for (auto site = sideEffectPatchSites->begin(); site != sideEffectPatchSites->end(); ++site)
         TR::PatchNOPedGuardSite::compensate(0, (*site)->getLocation(), (*site)->getDestination());
      }
   }


// class used to determine the only jitted implementer of a virtual method
class FindSingleJittedImplementer : public TR_SubclassVisitor
   {
   public:

   FindSingleJittedImplementer(
         TR::Compilation * comp,
         TR_OpaqueClassBlock *topClassId,
         TR_ResolvedMethod *callerMethod,
         int32_t slotOrIndex) :
      TR_SubclassVisitor(comp)
      {
      _topClassId = topClassId;
      _implementer = 0;
      _callerMethod = callerMethod;
      _slotOrIndex = slotOrIndex;
      _topClassIsInterface = TR::Compiler->cls.isInterfaceClass(comp, topClassId);
      _maxNumVisitedSubClasses = comp->getOptions()->getMaxNumVisitedSubclasses();
      _numVisitedSubClasses = 0;
      }

   virtual bool visitSubclass(TR_PersistentClassInfo *cl);

   TR_ResolvedMethod *getJittedImplementer() const { return _implementer; }

   private:

   TR_OpaqueClassBlock *_topClassId;
   TR_ResolvedMethod *_implementer;
   TR_ResolvedMethod *_callerMethod;
   int32_t _slotOrIndex;
   bool _topClassIsInterface;
   int32_t _maxNumVisitedSubClasses;
   int32_t _numVisitedSubClasses;
   };


bool
FindSingleJittedImplementer::visitSubclass(TR_PersistentClassInfo *cl)
   {
   TR_OpaqueClassBlock * classId = cl->getClassId();

   if (TR::Compiler->cls.isConcreteClass(comp(), classId))
      {
      TR_ResolvedMethod *method;
      if (_topClassIsInterface)
         method = _callerMethod->getResolvedInterfaceMethod(comp(), classId, _slotOrIndex);
      else
         method = _callerMethod->getResolvedVirtualMethod(comp(), classId, _slotOrIndex);

      ++_numVisitedSubClasses;
      if ((_numVisitedSubClasses > _maxNumVisitedSubClasses) || !method)
         {
         stopTheWalk();
         _implementer = 0; // signal failure
         return false;
         }

      // check to see if there are any duplicates
      if (!method->isInterpretedForHeuristics())
         {
         if (_implementer)
            {
            if (!method->isSameMethod(_implementer))
               {
               //found two compiled implementers
               stopTheWalk();
               _implementer = 0; // signal failure
               return false;
               }
            }
         else // add this compiled implementer to the list
            {
            _implementer = method;
            }
         }
      }
   return true;
   }


TR_ResolvedMethod *
TR_PersistentCHTable::findSingleJittedImplementer(
      TR_OpaqueClassBlock *thisClass,
      int32_t vftSlot,
      TR_ResolvedMethod *callerMethod,
      TR::Compilation *comp,
      TR::ResolvedMethodSymbol *calleeSymbol,
      bool locked)
   {
   if (comp->fej9()->isAOT_DEPRECATED_DO_NOT_USE())
      return 0;

   if (comp->getOption(TR_DisableCHOpts))
      return 0;

   if (comp->getSymRefTab()->findObjectNewInstanceImplSymbol() &&
       comp->getSymRefTab()->findObjectNewInstanceImplSymbol()->getSymbol() == calleeSymbol)
      return 0;

   TR_ResolvedMethod *resolvedMethod = NULL;

      {
      TR::ClassTableCriticalSection findSingleJittedImplementer(comp->fe(), locked);

      TR_PersistentClassInfo * classInfo = findClassInfo(thisClass);
      if (!classInfo)
         return 0;

      FindSingleJittedImplementer collector(comp, thisClass, callerMethod, vftSlot);
      collector.visitSubclass(classInfo);
      collector.visit(thisClass, true);

      resolvedMethod = collector.getJittedImplementer();
      }

   return resolvedMethod;
   }

/**
 * Find persistent JIT class information for a given class.
 * The caller must insure that class table lock is obtained
 */
TR_PersistentClassInfo *
TR_PersistentCHTable::findClassInfo(TR_OpaqueClassBlock * classId)
   {
   if (!isAccessible())
      return NULL;

   TR_PersistentClassInfo *cl = _classes[TR_RuntimeAssumptionTable::hashCode((uintptr_t)classId) % CLASSHASHTABLE_SIZE].getFirst();
   while (cl &&
          cl->getClassId() != classId)
      cl = cl->getNext();
   return cl;
   }

/**
 * Find persistent JIT class information for a given class.
 * The class table lock is used to synchronize use of this method
 */
TR_PersistentClassInfo *
TR_PersistentCHTable::findClassInfoAfterLocking(TR_OpaqueClassBlock *classId,
      TR::Compilation *comp,
      bool returnClassInfoForAOT)
   {
   if (!isActive())
      return NULL;

   // for AOT do not use the class hierarchy
   if (comp->compileRelocatableCode() && !returnClassInfoForAOT)
      {
      return NULL;
      }

   if (comp->getOption(TR_DisableCHOpts))
      return NULL;

   TR_PersistentClassInfo *classInfo = findClassInfoAfterLocking(classId, comp->fe(), returnClassInfoForAOT);
   return classInfo;
   }

/**
 * Find persistent JIT class information for a given class.
 * The class table lock is used to synchronize use of this method
 */
TR_PersistentClassInfo *
TR_PersistentCHTable::findClassInfoAfterLocking(
      TR_OpaqueClassBlock *classId,
      TR_FrontEnd *fe,
      bool returnClassInfoForAOT)
   {
   if (!isActive())
      return NULL;

   TR::ClassTableCriticalSection findClassInfoAfterLocking(fe);
   return findClassInfo(classId);
   }

bool
TR_PersistentCHTable::isOverriddenInThisHierarchy(
      TR_ResolvedMethod *method,
      TR_OpaqueClassBlock *thisClass,
      int32_t vftSlot,
      TR::Compilation *comp,
      bool locked)
   {
   if (comp->getOption(TR_DisableCHOpts))
      return true; // fake answer to disable any optimizations based on CHTable

   if (thisClass == method->classOfMethod())
      return method->virtualMethodIsOverridden();

   TR_PersistentClassInfo * classInfo = findClassInfoAfterLocking(thisClass, comp);
   if (!classInfo)
      return true;

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(method->fe());

   if (debug("traceOverriddenInHierarchy"))
      {
      printf("virtual method %s\n", method->signature(comp->trMemory()));
      printf("offset %d\n", vftSlot);
      int32_t len; char * s = TR::Compiler->cls.classNameChars(comp, thisClass, len);
      printf("thisClass %.*s\n", len, s);
      }

   if (fej9->getResolvedVirtualMethod(thisClass, vftSlot) != method->getPersistentIdentifier())
      return true;

   if (!fej9->classHasBeenExtended(thisClass))
      return false;

   TR_ScratchList<TR_PersistentClassInfo> leafs(comp->trMemory());
   TR_ClassQueries::collectLeafs(classInfo, leafs, comp, locked);
   ListIterator<TR_PersistentClassInfo> i(&leafs);
   for (classInfo = i.getFirst(); classInfo; classInfo = i.getNext())
      {
      if (debug("traceOverriddenInHierarchy"))
         {
         int32_t len; char * s = TR::Compiler->cls.classNameChars(comp, classInfo->getClassId(), len);
         printf("leaf %.*s\n", len, s);
         }
      if (fej9->getResolvedVirtualMethod(classInfo->getClassId(), vftSlot) != method->getPersistentIdentifier())
         return true;
      }
   return false;
   }

TR_ResolvedMethod * TR_PersistentCHTable::findSingleImplementer(
      TR_OpaqueClassBlock * thisClass,
      int32_t cpIndexOrVftSlot,
      TR_ResolvedMethod * callerMethod,
      TR::Compilation * comp,
      bool locked,
      TR_YesNoMaybe useGetResolvedInterfaceMethod,
      bool validate)
   {
   if (comp->getOption(TR_DisableCHOpts))
      return 0;



   TR_PersistentClassInfo * classInfo = comp->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(thisClass, comp, true);
   if (!classInfo)
      {
      return 0;
      }

   TR_ResolvedMethod *implArray[2]; // collect maximum 2 implementers if you can
   comp->enterHeuristicRegion();
   int32_t implCount = TR_ClassQueries::collectImplementorsCapped(classInfo, implArray, 2, cpIndexOrVftSlot, callerMethod, comp, locked, useGetResolvedInterfaceMethod);
   comp->exitHeuristicRegion();

   TR_ResolvedMethod *implementer = NULL;
   if (implCount == 1)
      implementer =implArray[0];

   if (implementer && comp->getOption(TR_UseSymbolValidationManager) && validate)
      {
      TR::SymbolValidationManager *svm = comp->getSymbolValidationManager();

      bool validated = svm->addMethodFromSingleImplementerRecord(implementer->getPersistentIdentifier(),
                                                                 thisClass,
                                                                 cpIndexOrVftSlot,
                                                                 callerMethod->getPersistentIdentifier(),
                                                                 useGetResolvedInterfaceMethod);
      if (validated)
         SVM_ASSERT_ALREADY_VALIDATED(svm, implementer->classOfMethod());
      else
         implementer = NULL;
      }

   return implementer;
   }

TR_ResolvedMethod *
TR_PersistentCHTable::findSingleInterfaceImplementer(
      TR_OpaqueClassBlock *thisClass,
      int32_t cpIndex,
      TR_ResolvedMethod *callerMethod,
      TR::Compilation *comp,
      bool locked,
      bool validate)
   {
   if (comp->getOption(TR_DisableCHOpts))
      return 0;

   if (!TR::Compiler->cls.isInterfaceClass(comp, thisClass))
      {
      return 0;
      }

   TR_PersistentClassInfo * classInfo = findClassInfoAfterLocking(thisClass, comp, true);
   if (!classInfo)
      {
      return 0;
      }

   TR_ResolvedMethod *implArray[2]; // collect maximum 2 implementers if you can
   comp->enterHeuristicRegion();
   int32_t implCount = TR_ClassQueries::collectImplementorsCapped(classInfo, implArray, 2, cpIndex, callerMethod, comp, locked);
   comp->exitHeuristicRegion();

   TR_ResolvedMethod *implementer = NULL;
   if (implCount == 1)
      implementer = implArray[0];

   if (implementer && comp->getOption(TR_UseSymbolValidationManager) && validate)
      {
      TR::SymbolValidationManager *svm = comp->getSymbolValidationManager();
      bool validated = svm->addMethodFromSingleInterfaceImplementerRecord(implementer->getPersistentIdentifier(),
                                                                          thisClass,
                                                                          cpIndex,
                                                                          callerMethod->getPersistentIdentifier());

      if (validated)
         SVM_ASSERT_ALREADY_VALIDATED(svm, implementer->classOfMethod());
      else
         implementer = NULL;
      }

   return implementer;
   }

bool
TR_PersistentCHTable::hasThreeOrMoreCompiledImplementors(
   TR_OpaqueClassBlock * thisClass, int32_t cpIndex, TR_ResolvedMethod * callerMethod, TR::Compilation * comp, TR_Hotness hotness, bool locked)
   {
   if (comp->getOption(TR_DisableCHOpts))
      return false;

   if (!TR::Compiler->cls.isInterfaceClass(comp, thisClass))
      return false;

   TR_PersistentClassInfo * classInfo = findClassInfoAfterLocking(thisClass, comp, true);
   if (!classInfo) return false;

   TR_ResolvedMethod *implArray[3];
   return TR_ClassQueries::collectCompiledImplementorsCapped(classInfo,implArray,3,cpIndex,callerMethod,comp,hotness,locked) == 3;
   }

int32_t
TR_PersistentCHTable::findnInterfaceImplementers(
      TR_OpaqueClassBlock *thisClass,
      int32_t n,
      TR_ResolvedMethod *implArray[],
      int32_t cpIndex,
      TR_ResolvedMethod *callerMethod,
      TR::Compilation *comp,
      bool locked)
   {
   if (comp->getOption(TR_DisableCHOpts))
      return 0;

   if (!TR::Compiler->cls.isInterfaceClass(comp, thisClass))
      return 0;

   TR_PersistentClassInfo * classInfo = findClassInfoAfterLocking(thisClass, comp, true);
   if (!classInfo) return 0;

   int32_t implCount = TR_ClassQueries::collectImplementorsCapped(classInfo, implArray,n,cpIndex,callerMethod,comp,locked);
   return implCount;
   }


bool
TR_PersistentCHTable::isKnownToHaveMoreThanTwoInterfaceImplementers(
      TR_OpaqueClassBlock *thisClass,
      int32_t cpIndex,
      TR_ResolvedMethod *callerMethod,
      TR::Compilation * comp,
      bool locked)
   {
   if (comp->getOption(TR_DisableCHOpts))
      return true; // conservative answer if we want to disable optimization based on CHtable

   TR_PersistentClassInfo * classInfo = findClassInfoAfterLocking(thisClass, comp);
   if (!classInfo)
      return false;

   TR_ResolvedMethod *implArray[3]; // collect maximum 3 implementers if you can
   int32_t implCount = TR_ClassQueries::collectImplementorsCapped(classInfo, implArray, 3, cpIndex, callerMethod, comp, locked);
   return (implCount == 3);
   }


TR_ResolvedMethod *
TR_PersistentCHTable::findSingleAbstractImplementer(
   TR_OpaqueClassBlock * thisClass,
      int32_t vftSlot,
      TR_ResolvedMethod * callerMethod,
      TR::Compilation * comp,
      bool locked,
      bool validate)
   {
   if (comp->getOption(TR_DisableCHOpts))
      return 0;
   bool allowForAOT = comp->getOption(TR_UseSymbolValidationManager);
   TR_PersistentClassInfo * classInfo = findClassInfoAfterLocking(thisClass, comp, allowForAOT);
   if (!classInfo) return 0;

   if (TR::Compiler->cls.isInterfaceClass(comp, thisClass))
      return 0;

   TR_ResolvedMethod *implArray[2]; // collect maximum 2 implementers if you can
   comp->enterHeuristicRegion();
   int32_t implCount = TR_ClassQueries::collectImplementorsCapped(classInfo, implArray, 2, vftSlot, callerMethod, comp, locked);
   comp->exitHeuristicRegion();

   TR_ResolvedMethod *implementer = NULL;
   if(implCount == 1)
      implementer = implArray[0];

   if (implementer && comp->getOption(TR_UseSymbolValidationManager) && validate)
      {
      TR::SymbolValidationManager *svm = comp->getSymbolValidationManager();
      bool validated = svm->addMethodFromSingleAbstractImplementerRecord(implementer->getPersistentIdentifier(),
                                                                         thisClass,
                                                                         vftSlot,
                                                                         callerMethod->getPersistentIdentifier());

      if (validated)
         SVM_ASSERT_ALREADY_VALIDATED(svm, implementer->classOfMethod());
      else
         implementer = NULL;
      }

   return implementer;
   }


TR_OpaqueClassBlock *
TR_PersistentCHTable::findSingleConcreteSubClass(
      TR_OpaqueClassBlock *opaqueClass,
      TR::Compilation *comp,
      bool validate)
   {
   TR_OpaqueClassBlock *concreteSubClass = NULL;
   if (comp->getOption(TR_DisableCHOpts))
      return 0;

   bool allowForAOT = comp->getOption(TR_UseSymbolValidationManager);

   TR_PersistentClassInfo *classInfo = comp->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(opaqueClass, comp, allowForAOT);
   if (classInfo)
      {
      TR_ScratchList<TR_PersistentClassInfo> subClasses(comp->trMemory());
      TR_ClassQueries::collectAllSubClasses(classInfo, &subClasses, comp);
      ListIterator<TR_PersistentClassInfo> subClassesIt(&subClasses);
      for (TR_PersistentClassInfo *subClassInfo = subClassesIt.getFirst(); subClassInfo; subClassInfo = subClassesIt.getNext())
         {
         TR_OpaqueClassBlock *subClass = (TR_OpaqueClassBlock *) subClassInfo->getClassId();
         if (TR::Compiler->cls.isConcreteClass(comp, subClass))
            {
            if (concreteSubClass)
               return NULL;
            concreteSubClass = subClass;
            }
         }
      }

   if (validate && comp->getOption(TR_UseSymbolValidationManager))
      {
      if (!comp->getSymbolValidationManager()->addConcreteSubClassFromClassRecord(concreteSubClass, opaqueClass))
         concreteSubClass = NULL;
      }

   return concreteSubClass;
   }


#ifdef DEBUG
void
TR_PersistentCHTable::dumpStats(TR_FrontEnd * fe)
   {
   }
#endif


void
TR_PersistentCHTable::dumpMethodCounts(TR_FrontEnd *fe, TR_Memory &trMemory)
   {
   TR_ASSERT_FATAL(isActive(), "Should not be called if table is not active!");
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
   for (int32_t i = 0; i < CLASSHASHTABLE_SIZE; i++)
      {
      for (TR_PersistentClassInfo *pci = _classes[i].getFirst(); pci; pci = pci->getNext())
         {
         TR_ScratchList<TR_ResolvedMethod> resolvedMethodsInClass(&trMemory);
         fej9->getResolvedMethods(&trMemory, pci->getClassId(), &resolvedMethodsInClass);
         ListIterator<TR_ResolvedMethod> resolvedIt(&resolvedMethodsInClass);
         TR_ResolvedMethod *resolvedMethod;
         for (resolvedMethod = resolvedIt.getFirst(); resolvedMethod; resolvedMethod = resolvedIt.getNext())
            {
            const char *signature = resolvedMethod->signature(&trMemory);
            int32_t count = resolvedMethod->getInvocationCount();

            printf("Final: Signature %s Count %d\n",signature,count);fflush(stdout);
            }
         }
      }
   }


void
TR_PersistentCHTable::resetVisitedClasses() // highly time consuming
   {
   TR_ASSERT_FATAL(isAccessible(), "Should not be called if table is not accessible!");
   for (int32_t i = 0; i <= CLASSHASHTABLE_SIZE; ++i)
      {
      TR_PersistentClassInfo * cl = _classes[i].getFirst();
      while (cl)
        {
        cl->resetVisited();
        cl = cl->getNext();
        }
      }
   }



void
TR_PersistentCHTable::classGotUnloaded(
      TR_FrontEnd *fe,
      TR_OpaqueClassBlock *classId)
   {
   TR_ASSERT_FATAL(isActive(), "Should not be called if table is not active!");
   TR_PersistentClassInfo * cl = findClassInfo(classId);

   bool p = TR::Options::getVerboseOption(TR_VerboseHookDetailsClassUnloading);
   if (p)
      {
      TR_VerboseLog::writeLineLocked(TR_Vlog_HD, "setting class 0x%p as unloaded\n", classId);
      }

   //if the class was not fully loaded, it might not be in the RAT.
   if(cl)
      cl->setUnloaded();
   }


TR_PersistentClassInfo *
TR_PersistentCHTable::classGotLoaded(
      TR_FrontEnd *fe,
      TR_OpaqueClassBlock *classId)
   {
   TR_ASSERT_FATAL(isAccessible(), "Should not be called if table is not accessible!");
   TR_ASSERT(!findClassInfo(classId), "Should not add duplicates to hash table\n");
   TR_PersistentClassInfo *clazz = new (PERSISTENT_NEW) TR_PersistentClassInfo(classId);
   if (clazz)
      {
      _classes[TR_RuntimeAssumptionTable::hashCode((uintptr_t) classId) % CLASSHASHTABLE_SIZE].add(clazz);
      }
   return clazz;
   }

void
TR_PersistentCHTable::collectAllSubClasses(TR_PersistentClassInfo *clazz, ClassList &classList, TR_J9VMBase *fej9, bool locked)
   {
   TR_ASSERT_FATAL(isActive(), "Should not be called if table is not active!");
   TR::ClassTableCriticalSection collectSubClasses(fej9, locked);

   VisitTracker<> marked(TR::Compiler->persistentAllocator());

   collectAllSubClassesLocked(clazz, classList, marked);
   }

void
TR_PersistentCHTable::collectAllSubClassesLocked(TR_PersistentClassInfo *clazz, ClassList &classList, VisitTracker<> &marked)
   {
   TR_ASSERT_FATAL(isActive(), "Should not be called if table is not active!");
   for (auto subClass = clazz->getFirstSubclass(); subClass; subClass = subClass->getNext())
      {
      if (!subClass->getClassInfo()->hasBeenVisited())
         {
         TR_PersistentClassInfo *sc = subClass->getClassInfo();
         classList.push_front(sc);
         marked.visit(sc);
         collectAllSubClassesLocked(sc, classList, marked);
         }
      }
   }

void
TR_PersistentCHTable::resetCachedCCVResult(TR_J9VMBase *fej9, TR_OpaqueClassBlock *clazz)
   {
   TR_ASSERT_FATAL(isActive(), "Should not be called if table is not active!");
   TR::ClassTableCriticalSection collectSubClasses(fej9);

   ClassList classList(TR::Compiler->persistentAllocator());
   TR_PersistentClassInfo *classInfo = findClassInfo(clazz);

   classList.push_front(classInfo);
   collectAllSubClasses(classInfo, classList, fej9, true);

   for (auto iter = classList.begin(); iter != classList.end(); iter++)
      {
      (*iter)->setCCVResult(CCVResult::notYetValidated);
      }
   }

bool
TR_PersistentCHTable::addClassToTable(J9VMThread *vmThread,
                                     J9JITConfig *jitConfig,
                                     J9Class *j9clazz,
                                     TR::CompilationInfo *compInfo)
   {
   TR_OpaqueClassBlock *opaqueClazz = TR::Compiler->cls.convertClassPtrToClassOffset(j9clazz);

   /* Class is already in table */
   if (findClassInfo(opaqueClazz))
      return true;

   /* j/l/Object has a NULL at superclasses[-1] */
   J9Class *superClazz = j9clazz->superclasses[J9CLASS_DEPTH(j9clazz) - 1];

   /* Add super class first */
   if (superClazz)
      if (!addClassToTable(vmThread, jitConfig, superClazz, compInfo))
         return false;

   /* Add interfaces */
   J9ITable *element = TR::Compiler->cls.iTableOf(opaqueClazz);
   while (element != NULL)
      {
      /* If j9clazz is an interface class, then the first element
       * will refer back to itself...
       */
      if (element->interfaceClass != j9clazz)
         if (!addClassToTable(vmThread, jitConfig, element->interfaceClass, compInfo))
            return false;

      element = TR::Compiler->cls.iTableNext(element);
      }

   UDATA eventFailed = 0;

   /* Trigger Class Load Hook */
   jitHookClassLoadHelper(vmThread, jitConfig, j9clazz, compInfo, &eventFailed);
   if (eventFailed)
      return false;

   /* Trigger Pre-initialize Hook */
   if ((j9clazz->initializeStatus & J9ClassInitStatusMask) != J9ClassInitNotInitialized)
      {
      jitHookClassPreinitializeHelper(vmThread, jitConfig, j9clazz, &eventFailed);
      if (eventFailed)
         return false;
      }

   /* Add array class if it exists */
   if (j9clazz->arrayClass)
      if (!addClassToTable(vmThread, jitConfig, j9clazz->arrayClass, compInfo))
         return false;

   return true;
   }

bool
TR_PersistentCHTable::activate(J9VMThread *vmThread, TR_J9VMBase *fej9, TR::CompilationInfo *compInfo)
   {
   TR_ASSERT_FATAL(!isAccessible(), "CH table is already accessible!");

   TR::ClassTableCriticalSection activateTable(fej9);

   if (TR::Options::getVerboseOption(TR_VerbosePerformance))
      TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "Activating CH Table...");

   setActivating();

   bool success = true;

   PORT_ACCESS_FROM_JAVAVM(vmThread->javaVM);
   J9ClassWalkState classWalkState;
   J9InternalVMFunctions *vmFuncs = vmThread->javaVM->internalVMFunctions;

   J9Class *j9clazz = vmFuncs->allClassesStartDo(&classWalkState, vmThread->javaVM, NULL);
   while (j9clazz)
      {
      if (!addClassToTable(vmThread, fej9->getJ9JITConfig(), j9clazz, compInfo))
         {
         success = false;
         break;
         }

      j9clazz = vmFuncs->allClassesNextDo(&classWalkState);
      }
   vmFuncs->allClassesEndDo(&classWalkState);

   if (success)
      {
      setActive();
      if (TR::Options::getVerboseOption(TR_VerbosePerformance))
         TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "Finished activating CH Table...");
      }
   else
      {
      setFailedToActivate();
      if (TR::Options::getVerboseOption(TR_VerbosePerformance))
         TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "Failed to activate CH Table...");
      }

   return success;
   }
