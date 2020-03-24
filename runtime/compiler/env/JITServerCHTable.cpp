/*******************************************************************************
 * Copyright (c) 2018, 2020 IBM Corp. and others
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

#include "env/CHTable.hpp"
#include "env/JITServerPersistentCHTable.hpp"
#include "env/j9methodServer.hpp"
#include "infra/List.hpp"                      // for TR::list
#include "compile/VirtualGuard.hpp"            // for TR_VirtualGuard
#include "il/SymbolReference.hpp"              // for SymbolReference
#include "codegen/CodeGenerator.hpp"           // for CodeGenerator
#include "env/VMAccessCriticalSection.hpp"     // for VMAccessCriticalSection

void
JITClientCommitVirtualGuard(const VirtualGuardInfoForCHTable *info, std::vector<TR_VirtualGuardSite> &sites,
                            TR_PersistentCHTable *table, TR::Compilation *comp);

void
JITClientCommitOSRVirtualGuards(TR::Compilation *comp, std::vector<VirtualGuardForCHTable> &vguards);

void
JITClientAddAnAssumptionForEachSubClass(TR_PersistentCHTable *table,
                                        TR_PersistentClassInfo *clazz,
                                        std::vector<TR_VirtualGuardSite> &list,
                                        TR::Compilation *comp)
   {
   // Gather the subtree of classes rooted at clazz
   TR_ScratchList<TR_PersistentClassInfo> subTree(comp->trMemory());
   TR_ClassQueries::collectAllSubClasses(clazz, &subTree, comp);

   // Add the root to the list
   subTree.add(clazz);

   for (TR_VirtualGuardSite &site : list)
      {
      ListIterator<TR_PersistentClassInfo> classIt(&subTree);
      for (TR_PersistentClassInfo *sc = classIt.getFirst(); sc; sc = classIt.getNext())
         {
         TR_ASSERT(sc == table->findClassInfo(sc->getClassId()), "Class ID mismatch");
         TR_PatchNOPedGuardSiteOnClassExtend::make(comp->fe(), comp->trPersistentMemory(), sc->getClassId(),
                                                   site.getLocation(),
                                                   site.getDestination(),
                                                   comp->getMetadataAssumptionList());
         comp->setHasClassExtendAssumptions();
         }
      }
   }

// Must hold classTableMonitor when calling this method
void cleanupNewlyExtendedInfo(TR::Compilation *comp, std::vector<TR_OpaqueClassBlock*> &classesThatShouldNotBeNewlyExtended)
   {
   TR_PersistentCHTable *table = comp->getPersistentInfo()->getPersistentCHTable();
   for (auto classId : classesThatShouldNotBeNewlyExtended)
      {
      TR_PersistentClassInfo * cl = table->findClassInfo(classId);
      // The class may have been unloaded during this compilation and the search may return NULL
      // This method is called even if we abort the compilation. Hence, killing the
      // compilation after a class unload (which we already do) is not ehough
      // If we get really unlucky a new class could be unloaded exactly in the place of the
      // unloaded one (the window of time is very small for this to occur). However, resetting
      // the flag for the newly loaded class is not going to create any problems (it's
      // supposed to be reset to start with)
      if (cl)
         cl->resetShouldNotBeNewlyExtended(comp->getCompThreadID());
      }
   }

bool JITClientCHTableCommit(
   TR::Compilation *comp,
   TR_MethodMetaData *metaData,
   CHTableCommitData &data)
   {
   TR_ASSERT(!comp->fej9()->isAOT_DEPRECATED_DO_NOT_USE(), "CHTable is not expected to be used in AOT mode");

   std::vector<TR_OpaqueClassBlock*> &classes = std::get<0>(data);
   std::vector<TR_OpaqueClassBlock*> &classesThatShouldNotBeNewlyExtended = std::get<1>(data);
   std::vector<TR_ResolvedMethod*> &preXMethods = std::get<2>(data);
   std::vector<TR_VirtualGuardSite> &sideEffectPatchSites = std::get<3>(data);
   std::vector<VirtualGuardForCHTable> &vguards = std::get<4>(data);
   FlatClassLoadCheck &compClassesThatShouldNotBeLoaded = std::get<5>(data);
   FlatClassExtendCheck &compClassesThatShouldNotBeNewlyExtended = std::get<6>(data);
   std::vector<TR_OpaqueClassBlock*> &classesForOSRRedefinition = std::get<7>(data);
   uint8_t *serverStartPC = std::get<8>(data);
   uint8_t *startPC = (uint8_t*) metaData->startPC;
   
   if (vguards.empty() && sideEffectPatchSites.empty() && preXMethods.empty() && classes.empty() && classesThatShouldNotBeNewlyExtended.empty())
      return true;

   cleanupNewlyExtendedInfo(comp, classesThatShouldNotBeNewlyExtended);
   if (comp->getFailCHTableCommit())
      return false;

   JITClientPersistentCHTable *table = (JITClientPersistentCHTable*) comp->getPersistentInfo()->getPersistentCHTable();
   TR_ResolvedMethod  *currentMethod = comp->getCurrentMethod();
   TR_Hotness hotness                = comp->getMethodHotness();

   if (!preXMethods.empty())
      {
      for (TR_ResolvedMethod *method : preXMethods)
         if (method->virtualMethodIsOverridden())
            return false;

      for (TR_ResolvedMethod *resolvedMethod : preXMethods)
         {
         TR_OpaqueMethodBlock *method = resolvedMethod->getPersistentIdentifier();
         TR_PreXRecompileOnMethodOverride::make(comp->fe(), comp->trPersistentMemory(), method, startPC, comp->getMetadataAssumptionList());
         comp->setHasMethodOverrideAssumptions();
         }
      }

   if (!classes.empty())
      {
      for (size_t i = 0; i < classes.size(); ++i)
         {
         TR_OpaqueClassBlock * classId = classes[i];

         // check if we may have already added this class to the persistent table
         //
         bool alreadyAdded = false;
         for (size_t j = 0; j < i && !alreadyAdded; ++j)
            {
            TR_OpaqueClassBlock * other = classes[j];
            if (other == classId)
               alreadyAdded = true;
            }

         if (!alreadyAdded)
            {
            if (comp->fe()->classHasBeenExtended(classId))
               return false;

            TR_PreXRecompileOnClassExtend::make(comp->fe(), comp->trPersistentMemory(), classId, startPC, comp->getMetadataAssumptionList());
            comp->setHasClassExtendAssumptions();
            }
         }
      }

   if (!classesThatShouldNotBeNewlyExtended.empty())
      {
      // keep a list of classes that were set visited so that we can
      // easily reset the visited flag later-on
      TR_CHTable::VisitTracker tracker(comp->trMemory());
      for (size_t i = 0; i < classesThatShouldNotBeNewlyExtended.size(); ++i)
         {
         TR_OpaqueClassBlock * classId = classesThatShouldNotBeNewlyExtended[i];
         TR_PersistentClassInfo * cl = table->findClassInfo(classId);
         if (cl && !cl->hasBeenVisited()) // is it possible to have cl==0
            {
            tracker.visit(cl);
            }
         }

      bool invalidAssumption = false;
      auto it = tracker.iterator();
      for (auto cl = it.getFirst(); cl; cl = it.getNext())
         {
         // If the class has been extended verify that was included in the original list
         // Otherwise it means that the extension happened afterwards and we should fail this compilation
         if (comp->fe()->classHasBeenExtended(cl->getClassId()))
            {
            for (TR_SubClass *subClass = cl->getFirstSubclass(); subClass; subClass = subClass->getNext())
               {
               TR_PersistentClassInfo *subClassInfo = subClass->getClassInfo();
               if (!subClassInfo->hasBeenVisited())
                  {
                  invalidAssumption = true;
                  break;
                  }
               }
            }

         if (invalidAssumption)
            break;

         TR_PreXRecompileOnClassExtend::make(comp->fe(), comp->trPersistentMemory(), cl->getClassId(), startPC, comp->getMetadataAssumptionList());
         comp->setHasClassExtendAssumptions();
         }

      if (invalidAssumption) return false;
      } //  if (classesThatShouldNotBeNewlyExtended)

   comp->getClassesForOSRRedefinition()->clear();
   for (auto &clazz : classesForOSRRedefinition)
      comp->addClassForOSRRedefinition(clazz);

   if (!vguards.empty())
      {
      for (auto &guard : vguards)
         {
         auto &info = std::get<0>(guard);
         auto &sites = std::get<1>(guard);
         auto &innerAssumptions = std::get<2>(guard);
         if (sites.empty())
            continue;
         for (auto &site : sites)
            {
            site.setDestination(site.getDestination() - serverStartPC + startPC);
            site.setLocation(site.getLocation() - serverStartPC + startPC);
            //fprintf(stderr, "\nsite: location=%p dest=%p, serverStartPC=%p, startPc=%p\n", site.getLocation(), site.getDestination(), serverStartPC, startPC);
            }
         // Commit the virtual guard itself
         //
         JITClientCommitVirtualGuard(&info, sites, table, comp);

         // Commit any inner guards that are assuming on this guard
         //
         for (auto &inner : innerAssumptions)
            JITClientCommitVirtualGuard(&inner, sites, table, comp);
         }
      
      // osr guards need to be processed after all the guards, because sites' location/destination
      // need to be patched first
      static bool dontGroupOSRAssumptions = (feGetEnv("TR_DontGroupOSRAssumptions") != NULL);
      if (!dontGroupOSRAssumptions)
         JITClientCommitOSRVirtualGuards(comp, vguards);
      }

   TR::list<TR_VirtualGuardSite*> *sites = comp->getSideEffectGuardPatchSites();
   sites->clear();
   for (auto site : sideEffectPatchSites)
      {
      TR_VirtualGuardSite *newSite = new (comp->trHeapMemory()) TR_VirtualGuardSite;
      newSite->setDestination(site.getDestination() - serverStartPC + startPC);
      newSite->setLocation(site.getLocation() - serverStartPC + startPC);
      sites->push_front(newSite);
      }
   comp->getClassesThatShouldNotBeLoaded()->setFirst(NULL); // clear
   comp->getClassesThatShouldNotBeNewlyExtended()->setFirst(NULL); // clear
   for (auto &clc : compClassesThatShouldNotBeLoaded)
      comp->getClassesThatShouldNotBeLoaded()->add(new (comp->trHeapMemory()) TR_ClassLoadCheck(&clc[0], clc.size()));
   for (auto &cec : compClassesThatShouldNotBeNewlyExtended)
      comp->getClassesThatShouldNotBeNewlyExtended()->add(new (comp->trHeapMemory()) TR_ClassExtendCheck(cec));

   table->commitSideEffectGuards(comp);

   // Clear again because string pointers are only valid in this block
   comp->getClassesThatShouldNotBeLoaded()->setFirst(NULL);

   return true;
   }

void
JITClientCommitOSRVirtualGuards(TR::Compilation *comp, std::vector<VirtualGuardForCHTable> &vguards)
   {
   // Count patch sites with OSR assumptions
   int osrSites = 0;
   TR_VirtualGuardSite *onlySite = NULL;
   for (auto &guard : vguards)
      {
      auto &info = std::get<0>(guard);
      auto &sites = std::get<1>(guard);
      if (info._kind == TR_OSRGuard || info._mergedWithOSRGuard)
         {
         if (sites.size() > 0)
            onlySite = &sites[0];
         osrSites += sites.size();
         }
      }

   TR_Array<TR_OpaqueClassBlock*> *clazzes = comp->getClassesForOSRRedefinition();
   if (osrSites == 0 || clazzes->size() == 0)
      return;
   else if (osrSites == 1)
      {
      // Only one patch point, create an assumption for each class
      for (int i = 0; i < clazzes->size(); ++i)
         TR_PatchNOPedGuardSiteOnClassRedefinition
            ::make(comp->fe(), comp->trPersistentMemory(), (*clazzes)[i], onlySite->getLocation(), onlySite->getDestination(), comp->getMetadataAssumptionList());
      }
   else if (osrSites > 1)
      {
      // Several points to patch, create collection
      TR::PatchSites *points = new (comp->trPersistentMemory()) TR::PatchSites(comp->trPersistentMemory(), osrSites);
      for (auto &guard : vguards)
         {
         auto &info = std::get<0>(guard);
         auto &sites = std::get<1>(guard);
         if (info._kind == TR_OSRGuard || info._mergedWithOSRGuard)
            { 
            for (auto &site : sites)
               points->add(site.getLocation(), site.getDestination());
            }
         }
 
      for (int i = 0; i < clazzes->size(); ++i)
         TR_PatchMultipleNOPedGuardSitesOnClassRedefinition
            ::make(comp->fe(), comp->trPersistentMemory(), (*clazzes)[i], points, comp->getMetadataAssumptionList());
      }

   comp->setHasClassRedefinitionAssumptions();
   return;
   }

void
JITClientCommitVirtualGuard(const VirtualGuardInfoForCHTable *info, std::vector<TR_VirtualGuardSite> &sites,
                            TR_PersistentCHTable *table, TR::Compilation *comp)
   {
   // If this is an OSR guard or another kind that has been marked as necessary to patch
   // in OSR, add a runtime assumption for every class that generated fear
   //
   if (info->_kind == TR_OSRGuard || info->_mergedWithOSRGuard)
      {
      static bool dontGroupOSRAssumptions = (feGetEnv("TR_DontGroupOSRAssumptions") != NULL);
      if (dontGroupOSRAssumptions)
         {
         TR_Array<TR_OpaqueClassBlock*> *clazzes = comp->getClassesForOSRRedefinition();
         if (clazzes)
            {
            for (TR_VirtualGuardSite &site : sites)
               {
               for (uint32_t i = 0; i < clazzes->size(); ++i)
                  TR_PatchNOPedGuardSiteOnClassRedefinition
                     ::make(comp->fe(), comp->trPersistentMemory(), (*clazzes)[i], site.getLocation(), site.getDestination(), comp->getMetadataAssumptionList());

               if (clazzes->size() > 0)
                  comp->setHasClassRedefinitionAssumptions();
               }
            }
         }

      // if it's not a real OSR guard, then we need to register
      // both the OSR site and the guard
      if (!info->_mergedWithOSRGuard)
         return;
      if (info->_kind == TR_ProfiledGuard) // !isNopable
         return;
      }

   TR_ResolvedMethod     *guardedMethod        = 0;
   TR_OpaqueClassBlock     *guardedClass         = 0;
   bool                     nopAssumptionIsValid = true;
   int32_t                  cpIndex              = info->_cpIndex;
   TR_ResolvedMethod     *owningMethod         = info->_owningMethod;

   if ((info->_kind == TR_HCRGuard) || info->_mergedWithHCRGuard)
      {
      guardedClass = info->_thisClass;
      for (TR_VirtualGuardSite &site : sites)
         {
         TR_ASSERT(site.getLocation(), "assertion failure");
         TR_PatchNOPedGuardSiteOnClassRedefinition
            ::make(comp->fe(), comp->trPersistentMemory(), guardedClass, site.getLocation(), site.getDestination(), comp->getMetadataAssumptionList());
         comp->setHasClassRedefinitionAssumptions();
         }
      // if it's not real HCR guard then we need to register
      // both the HCR site and the guard
      if (!info->_mergedWithHCRGuard)
         return;
      if (info->_kind == TR_ProfiledGuard) // !isNopable
         return;
      }

   if (info->_kind == TR_DummyGuard)
      {
      // nothing to do
      }
   else if (info->_kind == TR_MutableCallSiteTargetGuard)
      {
      static char *dontInvalidateMCSTargetGuards = feGetEnv("TR_dontInvalidateMCSTargetGuards");
      if (!dontInvalidateMCSTargetGuards)
         {
         uintptr_t *mcsReferenceLocation = info->_mutableCallSiteObject;
         TR::KnownObjectTable *knot = comp->getKnownObjectTable();
         TR_ASSERT(knot, "MutableCallSiteTargetGuard requires the Known Object Table");
         void *cookiePointer = comp->trPersistentMemory()->allocatePersistentMemory(1);
         uintptr_t potentialCookie = (uintptr_t)(uintptr_t)cookiePointer;
         uintptr_t cookie = 0;

         TR::KnownObjectTable::Index currentIndex;

            {
            TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
            // JITServer KOT:
            // This method is called by JITClientCHTableCommit() at the client.
            // Although accessing VM is not an issue, getOrCreateIndex() could update the KOT
            // at the client directly and the KOT at the server could be out of sync.
            // However, JITClientCHTableCommit() is called at the end of compilation,
            // and therefore it cannot cause any issues.
            TR::VMAccessCriticalSection invalidateMCSTargetGuards(fej9);
            // TODO: Code duplication with TR_InlinerBase::findInlineTargets
            currentIndex = TR::KnownObjectTable::UNKNOWN;
            uintptr_t currentEpoch = fej9->getVolatileReferenceField(*mcsReferenceLocation, "epoch", "Ljava/lang/invoke/MethodHandle;");
            if (currentEpoch)
               currentIndex = knot->getOrCreateIndex(currentEpoch);
            if (info->_mutableCallSiteEpoch == currentIndex)
               cookie = fej9->mutableCallSiteCookie(*mcsReferenceLocation, potentialCookie);
            else
               nopAssumptionIsValid = false;
            }

         if (cookie != potentialCookie)
            comp->trPersistentMemory()->freePersistentMemory(cookiePointer);

         if (nopAssumptionIsValid)
            {
            for (TR_VirtualGuardSite &site : sites)
               {
               TR_ASSERT(site.getLocation(), "assertion failure");
               TR_PatchNOPedGuardSiteOnMutableCallSiteChange
                  ::make(comp->fe(), comp->trPersistentMemory(), cookie, site.getLocation(), site.getDestination(), comp->getMetadataAssumptionList());
               }
            }
         else if (comp->getOption(TR_TraceCG))
            traceMsg(comp, "MutableCallSiteTargetGuard is already invalid.  Expected epoch: obj%d  Found: obj%d\n", info->_mutableCallSiteEpoch, currentIndex);
         }
      }
   else if ((info->_kind == TR_MethodEnterExitGuard) || (info->_kind == TR_DirectMethodGuard))
      {
      // nothing to do
      }
   else if (info->_kind == TR_BreakpointGuard)
      {
      if (comp->getOption(TR_DisableNopBreakpointGuard))
         return;
      TR_ResolvedMethod *breakpointedMethod = info->_inlinedResolvedMethod;
      TR_OpaqueMethodBlock *method = breakpointedMethod->getPersistentIdentifier();
      if (comp->fej9()->isMethodBreakpointed(method))
         nopAssumptionIsValid = false;
      for (TR_VirtualGuardSite &site : sites)
         {
         TR_PatchNOPedGuardSiteOnMethodBreakPoint
            ::make(comp->fe(), comp->trPersistentMemory(), method, site.getLocation(), site.getDestination(), comp->getMetadataAssumptionList());
         }
      }
   else if (info->_kind == TR_ArrayStoreCheckGuard)
      {
      guardedClass = info->_thisClass;
      if (comp->fe()->classHasBeenExtended(guardedClass))
         nopAssumptionIsValid = false;
      }
   else if (!info->_hasResolvedMethodSymbol)
      {
      TR_ASSERT(info->_isInterface && info->_kind == TR_InterfaceGuard, "assertion failure");
      TR_OpaqueClassBlock *thisClass = info->_thisClass;
      TR_ASSERT(thisClass, "assertion failure");
      
      TR_ResolvedMethod *implementer = table->findSingleImplementer(thisClass, cpIndex, owningMethod, comp, true, TR_yes);
      if (!implementer ||
          (info->_testType == TR_VftTest && comp->fe()->classHasBeenExtended(implementer->containingClass())))
         nopAssumptionIsValid = false;
      else
         JITClientAddAnAssumptionForEachSubClass(table, table->findClassInfo(thisClass), sites, comp);
      }
   else if (info->_kind == TR_NonoverriddenGuard && info->_testType != TR_VftTest)
      {
      TR_ASSERT(info->_testType == TR_NonoverriddenTest, "unexpected test type for a non-overridden guard.");

      guardedMethod = info->_guardedMethod;
      if (guardedMethod->virtualMethodIsOverridden())
         nopAssumptionIsValid = false;
      }
   else if ((!info->_isInlineGuard &&
          !TR::Compiler->cls.isAbstractClass(comp, info->_guardedMethod->containingClass())) ||
          (info->_isInlineGuard && info->_kind == TR_HierarchyGuard && info->_testType == TR_MethodTest))
      {
      guardedMethod = info->_guardedMethod;
      TR_ASSERT(guardedMethod, "Method must have been found when creating assumption\n");

      TR_OpaqueClassBlock *thisClass = info->_guardedMethodThisClass;

     //temp fix for defect 57599 until we find root cause
      if (table->isOverriddenInThisHierarchy(guardedMethod, thisClass, info->_offset, comp))
         nopAssumptionIsValid = false;
      }
   else if (info->_isInlineGuard &&
            info->_testType == TR_VftTest &&
            (info->_kind == TR_NonoverriddenGuard || info->_kind == TR_HierarchyGuard))
      {
      guardedClass = info->_thisClass;
      if (comp->fe()->classHasBeenExtended(guardedClass))
         nopAssumptionIsValid = false;
      }
   else if ((!info->_isInlineGuard &&
             TR::Compiler->cls.isAbstractClass(comp, info->_guardedMethod->containingClass())) ||
            (info->_isInlineGuard && info->_kind == TR_AbstractGuard && info->_testType == TR_MethodTest))
      {
      TR_OpaqueClassBlock *thisClass = info->_isInlineGuard ? thisClass = info->_thisClass
         : info->_guardedMethod->containingClass();
      if (table->findSingleAbstractImplementer(thisClass, info->_offset, owningMethod, comp, true))
         JITClientAddAnAssumptionForEachSubClass(table, table->findClassInfo(thisClass), sites, comp);
      else
         nopAssumptionIsValid = false;
      }
   else
      {
      TR_ASSERT(false, "should be unreachable");
      nopAssumptionIsValid = false;
      }

   if (nopAssumptionIsValid)
      {
      for (TR_VirtualGuardSite &site : sites)
         {
         TR_ASSERT(site.getLocation(), "assertion failure");
         if (guardedClass)
            {
            TR_PatchNOPedGuardSiteOnClassExtend
               ::make(comp->fe(), comp->trPersistentMemory(), guardedClass, site.getLocation(), site.getDestination(), comp->getMetadataAssumptionList());
            comp->setHasClassExtendAssumptions();
            }
         if (guardedMethod)
            {
            TR_PatchNOPedGuardSiteOnMethodOverride
               ::make(comp->fe(), comp->trPersistentMemory(), guardedMethod->getPersistentIdentifier(),
                site.getLocation(), site.getDestination(), comp->getMetadataAssumptionList());
            comp->setHasMethodOverrideAssumptions();
            }
         }
      }
   else // assumption is invalid
      {
      for (TR_VirtualGuardSite &site : sites)
         {
         if (comp->getOption(TR_TraceCG))
            traceMsg(comp, "   Patching %p to %p\n", site.getLocation(), site.getDestination());
         TR::PatchNOPedGuardSite::compensate(0, site.getLocation(), site.getDestination());
         }
      }
   }
