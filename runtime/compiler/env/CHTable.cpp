/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "env/CompilerEnv.hpp"
#include "env/KnownObjectTable.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/VirtualGuard.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/CompilerEnv.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/PersistentInfo.hpp"
#include "env/jittypes.h"
#include "env/j9method.h"
#include "env/ClassTableCriticalSection.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "env/VMJ9.h"
#include "il/MethodSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"
#include "optimizer/PreExistence.hpp"
#if defined(J9VM_OPT_JITSERVER)
#include "env/j9methodServer.hpp"
#endif

void
TR_PreXRecompile::dumpInfo()
   {
   OMR::RuntimeAssumption::dumpInfo("TR_PreXRecompile");
   TR_VerboseLog::write(" startPC=%p", _startPC);
   }

void
TR::PatchNOPedGuardSite::dumpInfo()
   {
   OMR::RuntimeAssumption::dumpInfo("TR::PatchNOPedGuardSite");
   TR_VerboseLog::write(" location=%p destination=%p", _location, _destination);
   }

void
TR::PatchMultipleNOPedGuardSites::dumpInfo()
   {
   OMR::RuntimeAssumption::dumpInfo("TR::PatchMultipleNOPedGuardSites");
   for (int i = 0; i < _patchSites->getSize(); ++i)
      TR_VerboseLog::write(" %d location=%p destination=%p", i, _patchSites->getLocation(i), _patchSites->getDestination(i));
   }

void
TR_PatchJNICallSite::dumpInfo()
   {
   OMR::RuntimeAssumption::dumpInfo("TR_PatchJNICallSite");
   TR_VerboseLog::write(" pc=%p", _pc );
   }

TR_PatchNOPedGuardSiteOnClassExtend *TR_PatchNOPedGuardSiteOnClassExtend::make(
   TR_FrontEnd *fe, TR_PersistentMemory *pm, TR_OpaqueClassBlock *clazz, uint8_t *loc, uint8_t *dest, OMR::RuntimeAssumption **sentinel)
   {
   TR_PatchNOPedGuardSiteOnClassExtend *result = new (pm) TR_PatchNOPedGuardSiteOnClassExtend(pm, clazz, loc, dest);
   result->addToRAT(pm, RuntimeAssumptionOnClassExtend, fe, sentinel);
   return result;
   }

TR_PatchNOPedGuardSiteOnMethodOverride *TR_PatchNOPedGuardSiteOnMethodOverride::make(
   TR_FrontEnd *fe, TR_PersistentMemory *pm, TR_OpaqueMethodBlock *method, uint8_t *loc, uint8_t *dest, OMR::RuntimeAssumption **sentinel)
   {
   TR_PatchNOPedGuardSiteOnMethodOverride *result = new (pm) TR_PatchNOPedGuardSiteOnMethodOverride(pm, method, loc, dest);
   result->addToRAT(pm, RuntimeAssumptionOnMethodOverride, fe, sentinel);
   return result;
   }

TR_PatchNOPedGuardSiteOnClassRedefinition *TR_PatchNOPedGuardSiteOnClassRedefinition::make(
   TR_FrontEnd *fe, TR_PersistentMemory *pm, TR_OpaqueClassBlock *clazz, uint8_t *loc, uint8_t *dest, OMR::RuntimeAssumption **sentinel)
   {
   TR_PatchNOPedGuardSiteOnClassRedefinition *result = new (pm) TR_PatchNOPedGuardSiteOnClassRedefinition(pm, clazz, loc, dest);
   result->addToRAT(pm, RuntimeAssumptionOnClassRedefinitionNOP, fe, sentinel);
   return result;
   }

TR_PatchMultipleNOPedGuardSitesOnClassRedefinition *TR_PatchMultipleNOPedGuardSitesOnClassRedefinition::make(
   TR_FrontEnd *fe, TR_PersistentMemory *pm, TR_OpaqueClassBlock *clazz, TR::PatchSites *sites, OMR::RuntimeAssumption **sentinel)
   {
   TR_PatchMultipleNOPedGuardSitesOnClassRedefinition *result = new (pm) TR_PatchMultipleNOPedGuardSitesOnClassRedefinition(pm, clazz, sites);
   sites->addReference();
   result->addToRAT(pm, RuntimeAssumptionOnClassRedefinitionNOP, fe, sentinel);
   return result;
   }

TR_PatchNOPedGuardSiteOnStaticFinalFieldModification *TR_PatchNOPedGuardSiteOnStaticFinalFieldModification::make(
   TR_FrontEnd *fe, TR_PersistentMemory *pm, TR_OpaqueClassBlock *clazz, uint8_t *loc, uint8_t *dest, OMR::RuntimeAssumption **sentinel)
   {
   TR_PatchNOPedGuardSiteOnStaticFinalFieldModification *result = new (pm) TR_PatchNOPedGuardSiteOnStaticFinalFieldModification(pm, clazz, loc, dest);
   result->addToRAT(pm, RuntimeAssumptionOnStaticFinalFieldModification, fe, sentinel);
   return result;
   }

TR_PatchMultipleNOPedGuardSitesOnStaticFinalFieldModification *TR_PatchMultipleNOPedGuardSitesOnStaticFinalFieldModification::make(
   TR_FrontEnd *fe, TR_PersistentMemory *pm, TR_OpaqueClassBlock *clazz, TR::PatchSites *sites, OMR::RuntimeAssumption **sentinel)
   {
   TR_PatchMultipleNOPedGuardSitesOnStaticFinalFieldModification *result = new (pm) TR_PatchMultipleNOPedGuardSitesOnStaticFinalFieldModification(pm, clazz, sites);
   sites->addReference();
   result->addToRAT(pm, RuntimeAssumptionOnStaticFinalFieldModification, fe, sentinel);
   return result;
   }

TR_PatchJNICallSite *TR_PatchJNICallSite::make(
   TR_FrontEnd *fe, TR_PersistentMemory * pm, uintptr_t key, uint8_t *pc, OMR::RuntimeAssumption **sentinel)
   {
   TR_PatchJNICallSite *result = new (pm) TR_PatchJNICallSite(pm, key, pc);
   result->addToRAT(pm, RuntimeAssumptionOnRegisterNative, fe, sentinel);
   return result;
   }

TR_PreXRecompileOnClassExtend *TR_PreXRecompileOnClassExtend::make(
   TR_FrontEnd *fe, TR_PersistentMemory *pm, TR_OpaqueClassBlock *clazz, uint8_t *startPC, OMR::RuntimeAssumption **sentinel)
   {
   TR_PreXRecompileOnClassExtend *result = new (pm) TR_PreXRecompileOnClassExtend(pm, clazz, startPC);
   result->addToRAT(pm, RuntimeAssumptionOnClassExtend, fe, sentinel);
   return result;
   }

TR_PreXRecompileOnMethodOverride *TR_PreXRecompileOnMethodOverride::make(
   TR_FrontEnd *fe, TR_PersistentMemory *pm, TR_OpaqueMethodBlock *method, uint8_t *startPC, OMR::RuntimeAssumption **sentinel)
   {
   TR_PreXRecompileOnMethodOverride *result = new (pm) TR_PreXRecompileOnMethodOverride(pm, method, startPC);
   result->addToRAT(pm, RuntimeAssumptionOnMethodOverride, fe, sentinel);
   return result;
   }

TR_PatchNOPedGuardSiteOnMutableCallSiteChange *TR_PatchNOPedGuardSiteOnMutableCallSiteChange::make(
      TR_FrontEnd *fe, TR_PersistentMemory *pm, uintptr_t key, uint8_t *location, uint8_t *destination, OMR::RuntimeAssumption **sentinel)
   {
   TR_PatchNOPedGuardSiteOnMutableCallSiteChange *result = new (pm) TR_PatchNOPedGuardSiteOnMutableCallSiteChange(pm, key, location, destination);
   result->addToRAT(pm, RuntimeAssumptionOnMutableCallSiteChange, fe, sentinel);
   return result;
   }

bool TR_CHTable::recompileOnClassExtend(TR::Compilation *c, TR_OpaqueClassBlock * classId)
   {
   // return true or false
   // depending on whether the class was added to the list
   //
   c->setUsesPreexistence(true);
   if (!_classes)
      _classes = new (c->trHeapMemory()) TR_Array<TR_OpaqueClassBlock *>(c->trMemory(), 8);
   if (!_classes->contains(classId))
      _classes->add(classId);
   else return false;
   return true;
   }


bool TR_CHTable::recompileOnNewClassExtend(TR::Compilation *c, TR_OpaqueClassBlock * classId)
   {
   // return true or false
   // depending on whether the class was added to the list
   //
   c->setUsesPreexistence(true);
   if (!_classesThatShouldNotBeNewlyExtended)
      _classesThatShouldNotBeNewlyExtended = new (c->trHeapMemory()) TR_Array<TR_OpaqueClassBlock *>(c->trMemory(), 8);
   if (!_classesThatShouldNotBeNewlyExtended->contains(classId))
      _classesThatShouldNotBeNewlyExtended->add(classId);
   else return false;
   return true;
   }



bool TR_CHTable::recompileOnMethodOverride(TR::Compilation *c, TR_ResolvedMethod *method)
   {
   // return true or false
   // depending on whether the method was added to the list
   //
   c->setUsesPreexistence(true);
   if (!_preXMethods)
      _preXMethods = new (c->trHeapMemory()) TR_Array<TR_ResolvedMethod*>(c->trMemory(), 16);

   int32_t last = _preXMethods->lastIndex();
   bool addIt = true;
   for (int32_t i = 0; i <= last; ++i)
      {
      if (_preXMethods->element(i)->getPersistentIdentifier() == method->getPersistentIdentifier())
         {
         addIt = false;
         break;
         }
      }

   if (addIt)
      _preXMethods->add(method);
   else
      return false;

   return true;
   }


// Must hold classTableMonitor when calling this method
void TR_CHTable::cleanupNewlyExtendedInfo(TR::Compilation *comp)
   {
   if (_classesThatShouldNotBeNewlyExtended)
      {
      TR_PersistentCHTable *table = comp->getPersistentInfo()->getPersistentCHTable();
      int32_t last = _classesThatShouldNotBeNewlyExtended->lastIndex();
      for (int32_t i = 0; i <= last; ++i)
         {
         TR_OpaqueClassBlock * classId = _classesThatShouldNotBeNewlyExtended->element(i);
         TR_PersistentClassInfo * cl = table->findClassInfo(classId);
         // The class may have been unloaded during this compilation and the search may return NULL
         // This method is called even if we abort the compilation. Hence, killing the
         // compilation after a class unload (which we already do) is not enough
         // If we get really unlucky a new class could be unloaded exactly in the place of the
         // unloaded one (the window of time is very small for this to occur). However, resetting
         // the flag for the newly loaded class is not going to create any problems (it's
         // supposed to be reset to start with)
         if (cl)
            cl->resetShouldNotBeNewlyExtended(comp->getCompThreadID());
         }
      }
   }

// Returning false here will fail this compilation!
//
bool TR_CHTable::commit(TR::Compilation *comp)
   {
#if defined(J9VM_OPT_JITSERVER)
   if (comp->isOutOfProcessCompilation())
      {
      return true; // Handled in outOfProcessCompilationEnd instead
      }
#endif

   TR::list<TR_VirtualGuard*> &vguards = comp->getVirtualGuards();
   TR::list<TR_VirtualGuardSite*> *sideEffectPatchSites = comp->getSideEffectGuardPatchSites();

   if (comp->fej9()->isAOT_DEPRECATED_DO_NOT_USE())
      return true;
   if (vguards.empty() && sideEffectPatchSites->empty() && !_preXMethods && !_classes && !_classesThatShouldNotBeNewlyExtended)
      return true;

   cleanupNewlyExtendedInfo(comp);
   if (comp->getFailCHTableCommit())
      return false;

   TR_PersistentCHTable *table       = comp->getPersistentInfo()->getPersistentCHTable();
   TR_ResolvedMethod  *currentMethod = comp->getCurrentMethod();
   uint8_t   *startPC                = comp->cg()->getCodeStart();
   TR_Hotness hotness                = comp->getMethodHotness();

   if (_preXMethods)
      {
      int32_t last = _preXMethods->lastIndex(), i;
      for (i = 0; i <= last; ++i)
         if (_preXMethods->element(i)->virtualMethodIsOverridden())
            return false;

      for (i = 0; i <= last; ++i)
         {
         TR_ResolvedMethod *resolvedMethod = _preXMethods->element(i);
         TR_OpaqueMethodBlock *method  = resolvedMethod->getPersistentIdentifier();
         TR_PreXRecompileOnMethodOverride::make(comp->fe(), comp->trPersistentMemory(), method, startPC, comp->getMetadataAssumptionList());
         comp->setHasMethodOverrideAssumptions();
         }
      }

   if (_classes)
      {
      int32_t last = _classes->lastIndex();
      for (int32_t i = 0; i <= last; ++i)
         {
         TR_OpaqueClassBlock * classId = _classes->element(i);

         // check if we may have already added this class to the persistent table
         //
         bool alreadyAdded = false;
         for (int32_t j = 0; j < i && !alreadyAdded; ++j)
            {
            TR_OpaqueClassBlock * other = _classes->element(j);
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

    if (_classesThatShouldNotBeNewlyExtended)
      {
      int32_t last = _classesThatShouldNotBeNewlyExtended->lastIndex();

      // keep a list of classes that were set visited so that we can
      // easily reset the visited flag later-on
      VisitTracker tracker(comp->trMemory());
      for (int32_t i = 0; i <= last; ++i)
         {
         TR_OpaqueClassBlock * classId = _classesThatShouldNotBeNewlyExtended->element(i);
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
      } //  if (_classesThatShouldNotBeNewlyExtended)

   // Check if the assumptions for static final field are still valid
   // Returning false will cause CHTable opts to be disabled in the next compilation of this method,
   // thus we abort the compilation here to avoid causing performance issues
   TR_Array<TR_OpaqueClassBlock*> *clazzesOnStaticFinalFieldModification = comp->getClassesForStaticFinalFieldModification();
   for (int i = 0; i < clazzesOnStaticFinalFieldModification->size(); ++i)
      {
      TR_OpaqueClassBlock* clazz = (*clazzesOnStaticFinalFieldModification)[i];
      if (TR::Compiler->cls.classHasIllegalStaticFinalFieldModification(clazz))
         {
         if (TR::Options::isAnyVerboseOptionSet(TR_VerboseRuntimeAssumptions, TR_VerboseCompileEnd, TR_VerbosePerformance, TR_VerboseCompFailure))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE, "Failure while commiting static final field assumption for class %p for %s", clazz, comp->signature());
            }
         comp->failCompilation<TR::CompilationInterrupted>("Compilation interrupted: Static final field of a class has been modified");
         }
      }

   if (!vguards.empty())
      {
      static bool dontGroupOSRAssumptions = (feGetEnv("TR_DontGroupOSRAssumptions") != NULL);
      if (!dontGroupOSRAssumptions)
         commitOSRVirtualGuards(comp, vguards);

      for (auto info = vguards.begin(); info != vguards.end(); ++info)
         {
         List<TR_VirtualGuardSite> &sites = (*info)->getNOPSites();
         if (sites.isEmpty())
            continue;

         // Commit the virtual guard itself
         //
         commitVirtualGuard(*info, sites, table, comp);

         // Commit any inner guards that are assuming on this guard
         //
         ListIterator<TR_InnerAssumption> it(&((*info)->getInnerAssumptions()));
         for (TR_InnerAssumption *inner = it.getFirst(); inner; inner = it.getNext())
            commitVirtualGuard(inner->_guard, sites, table, comp);
         }
      }

   if (!sideEffectPatchSites->empty())
      table->commitSideEffectGuards(comp);

   return true;
   }

void
TR_CHTable::commitOSRVirtualGuards(TR::Compilation *comp, TR::list<TR_VirtualGuard*> &vguards)
   {
   // Count patch sites with OSR assumptions
   int osrSites = 0;
   TR_VirtualGuardSite *onlySite = NULL;
   for (auto info = vguards.begin(); info != vguards.end(); ++info)
      {
      if ((*info)->getKind() == TR_OSRGuard || (*info)->mergedWithOSRGuard())
         {
         List<TR_VirtualGuardSite> &sites = (*info)->getNOPSites();
         if (sites.getSize() > 0)
            onlySite = sites.getListHead()->getData();
         osrSites += sites.getSize();
         }
      }

   TR_Array<TR_OpaqueClassBlock*> *clazzesForOSRRedefinition = comp->getClassesForOSRRedefinition();
   TR_Array<TR_OpaqueClassBlock*> *clazzesForStaticFinalFieldModification = comp->getClassesForStaticFinalFieldModification();
   if (osrSites == 0
       || (clazzesForOSRRedefinition->size() == 0
           && clazzesForStaticFinalFieldModification->size() == 0))
      {
      return;
      }
   else if (osrSites == 1)
      {
      // Only one patch point, create an assumption for each class
      for (int i = 0; i < clazzesForOSRRedefinition->size(); ++i)
         TR_PatchNOPedGuardSiteOnClassRedefinition
            ::make(comp->fe(), comp->trPersistentMemory(), (*clazzesForOSRRedefinition)[i], onlySite->getLocation(), onlySite->getDestination(), comp->getMetadataAssumptionList());

      for (int i = 0; i < clazzesForStaticFinalFieldModification->size(); ++i)
         TR_PatchNOPedGuardSiteOnStaticFinalFieldModification
            ::make(comp->fe(), comp->trPersistentMemory(), (*clazzesForStaticFinalFieldModification)[i], onlySite->getLocation(), onlySite->getDestination(), comp->getMetadataAssumptionList());
      }
   else if (osrSites > 1)
      {
      // Several points to patch, create collection
      TR::PatchSites *points = new (comp->trPersistentMemory()) TR::PatchSites(comp->trPersistentMemory(), osrSites);
      for (auto info = vguards.begin(); info != vguards.end(); ++info)
         {
         if ((*info)->getKind() == TR_OSRGuard || (*info)->mergedWithOSRGuard())
            {
            List<TR_VirtualGuardSite> &sites = (*info)->getNOPSites();
            ListIterator<TR_VirtualGuardSite> it(&sites);
            for (TR_VirtualGuardSite *site = it.getFirst(); site; site = it.getNext())
               points->add(site->getLocation(), site->getDestination());
            }
         }

      for (int i = 0; i < clazzesForOSRRedefinition->size(); ++i)
         TR_PatchMultipleNOPedGuardSitesOnClassRedefinition
            ::make(comp->fe(), comp->trPersistentMemory(), (*clazzesForOSRRedefinition)[i], points, comp->getMetadataAssumptionList());

      for (int i = 0; i < clazzesForStaticFinalFieldModification->size(); ++i)
         TR_PatchMultipleNOPedGuardSitesOnStaticFinalFieldModification
            ::make(comp->fe(), comp->trPersistentMemory(), (*clazzesForStaticFinalFieldModification)[i], points, comp->getMetadataAssumptionList());
      }

   if (clazzesForOSRRedefinition->size() > 0)
      comp->setHasClassRedefinitionAssumptions();
   return;
   }

void
TR_CHTable::commitVirtualGuard(TR_VirtualGuard *info, List<TR_VirtualGuardSite> &sites,
                               TR_PersistentCHTable *table, TR::Compilation *comp)
   {
   // If this is an OSR guard or another kind that has been marked as necessary to patch
   // in OSR, add a runtime assumption for every class that generated fear
   //
   if (info->getKind() == TR_OSRGuard || info->mergedWithOSRGuard())
      {
      static bool dontGroupOSRAssumptions = (feGetEnv("TR_DontGroupOSRAssumptions") != NULL);
      if (dontGroupOSRAssumptions)
         {
         TR_Array<TR_OpaqueClassBlock*> *clazzesForRedefinition = comp->getClassesForOSRRedefinition();
         TR_Array<TR_OpaqueClassBlock*> *clazzesForStaticFinalFieldModification = comp->getClassesForStaticFinalFieldModification();

         if (clazzesForRedefinition || clazzesForStaticFinalFieldModification)
            {
            ListIterator<TR_VirtualGuardSite> it(&sites);
            for (TR_VirtualGuardSite *site = it.getFirst(); site; site = it.getNext())
               {
               for (uint32_t i = 0; i < clazzesForRedefinition->size(); ++i)
                  TR_PatchNOPedGuardSiteOnClassRedefinition
                     ::make(comp->fe(), comp->trPersistentMemory(), (*clazzesForRedefinition)[i], site->getLocation(), site->getDestination(), comp->getMetadataAssumptionList());

               if (clazzesForRedefinition->size() > 0)
                  comp->setHasClassRedefinitionAssumptions();

               // Add assumption for static final field folding
               for (uint32_t i = 0; i < clazzesForStaticFinalFieldModification->size(); ++i)
                  TR_PatchNOPedGuardSiteOnStaticFinalFieldModification
                     ::make(comp->fe(), comp->trPersistentMemory(), (*clazzesForStaticFinalFieldModification)[i], site->getLocation(), site->getDestination(), comp->getMetadataAssumptionList());
               }
            }
         }

      // if it's not real OSR guard then we need to register
      // both the OSR site and the guard
      if (!info->mergedWithOSRGuard())
         return;
      if (!info->isNopable())
         return;
      }

   TR::SymbolReference      *symRef               = info->getSymbolReference();
   TR::MethodSymbol         *methodSymbol         = symRef->getSymbol()->castToMethodSymbol();
   TR::ResolvedMethodSymbol *resolvedMethodSymbol = methodSymbol->getResolvedMethodSymbol();
   TR_ResolvedMethod     *guardedMethod        = 0;
   TR_OpaqueClassBlock     *guardedClass         = 0;
   bool                     nopAssumptionIsValid = true;
   int32_t                  cpIndex              = symRef->getCPIndex();
   TR_ResolvedMethod     *owningMethod         = symRef->getOwningMethod(comp);

   if ((info->getKind() == TR_HCRGuard) || info->mergedWithHCRGuard())
      {
      guardedClass = info->getThisClass();
      ListIterator<TR_VirtualGuardSite> it(&sites);
      for (TR_VirtualGuardSite *site = it.getFirst(); site; site = it.getNext())
         {
         TR_ASSERT(site->getLocation(), "assertion failure");
         TR_PatchNOPedGuardSiteOnClassRedefinition
            ::make(comp->fe(), comp->trPersistentMemory(), guardedClass, site->getLocation(), site->getDestination(), comp->getMetadataAssumptionList());
         comp->setHasClassRedefinitionAssumptions();
         }
      // if it's not real HCR guard then we need to register
      // both the HCR site and the guard
      if (!info->mergedWithHCRGuard())
         return;
      if (!info->isNopable())
         return;
      }

   if (info->getKind() == TR_DummyGuard)
      {
      /* nothing to do */
      }
   else if (info->getKind() == TR_MutableCallSiteTargetGuard)
      {
      static char *dontInvalidateMCSTargetGuards = feGetEnv("TR_dontInvalidateMCSTargetGuards");
      if (!dontInvalidateMCSTargetGuards)
         {
#if defined(J9VM_OPT_JITSERVER)
         // JITServer KOT: At the moment this method is called only by TR_CHTable::commit().
         // TR_CHTable::commit() already checks comp->isOutOfProcessCompilation().
         // Adding the following check as a precaution in case commitVirtualGuard() is called
         // outside TR_CHTable::commit() in the future.
         TR_ASSERT(!comp->isOutOfProcessCompilation(), "TR_CHTable::commitVirtualGuard() should not be called at the server\n");
#endif /* defined(J9VM_OPT_JITSERVER) */
         uintptr_t *mcsReferenceLocation = info->mutableCallSiteObject();
         TR::KnownObjectTable *knot = comp->getKnownObjectTable();
         TR_ASSERT(knot, "MutableCallSiteTargetGuard requires the Known Object Table");
         void *cookiePointer = comp->trPersistentMemory()->allocatePersistentMemory(1);
         uintptr_t potentialCookie = (uintptr_t)(uintptr_t)cookiePointer;
         uintptr_t cookie = 0;

         TR::KnownObjectTable::Index currentIndex;

            {
            TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
            TR::VMAccessCriticalSection invalidateMCSTargetGuards(fej9);
            // TODO: Code duplication with TR_InlinerBase::findInlineTargets
            currentIndex = TR::KnownObjectTable::UNKNOWN;
            uintptr_t currentEpoch = fej9->getVolatileReferenceField(*mcsReferenceLocation, "epoch", "Ljava/lang/invoke/MethodHandle;");
            if (currentEpoch)
               currentIndex = knot->getOrCreateIndex(currentEpoch);
            if (info->mutableCallSiteEpoch() == currentIndex)
               cookie = fej9->mutableCallSiteCookie(*mcsReferenceLocation, potentialCookie);
            else
               nopAssumptionIsValid = false;
            }

         if (cookie != potentialCookie)
            comp->trPersistentMemory()->freePersistentMemory(cookiePointer);

         if (nopAssumptionIsValid)
            {
            ListIterator<TR_VirtualGuardSite> it(&sites);
            for (TR_VirtualGuardSite *site = it.getFirst(); site; site = it.getNext())
               {
               TR_ASSERT(site->getLocation(), "assertion failure");
               TR_PatchNOPedGuardSiteOnMutableCallSiteChange
                  ::make(comp->fe(), comp->trPersistentMemory(), cookie, site->getLocation(), site->getDestination(), comp->getMetadataAssumptionList());
               }
            }
         else if (comp->getOption(TR_TraceCG))
            traceMsg(comp, "MutableCallSiteTargetGuard is already invalid.  Expected epoch: obj%d  Found: obj%d\n", info->mutableCallSiteEpoch(), currentIndex);
         }
      }
   else if ((info->getKind() == TR_MethodEnterExitGuard) || (info->getKind() == TR_DirectMethodGuard))
      {
      /* nothing to do */
      }
   else if (info->getKind() == TR_BreakpointGuard)
      {
      if (comp->getOption(TR_DisableNopBreakpointGuard))
         return;
      TR_ResolvedMethod *breakpointedMethod = comp->getInlinedResolvedMethod(info->getCalleeIndex());
      TR_OpaqueMethodBlock *method = breakpointedMethod->getPersistentIdentifier();
      if (comp->fej9()->isMethodBreakpointed(method))
         nopAssumptionIsValid = false;
      ListIterator<TR_VirtualGuardSite> it(&sites);
      for (TR_VirtualGuardSite *site = it.getFirst(); site; site = it.getNext())
         {
         TR_PatchNOPedGuardSiteOnMethodBreakPoint
            ::make(comp->fe(), comp->trPersistentMemory(), method, site->getLocation(), site->getDestination(), comp->getMetadataAssumptionList());
         }
      }
   else if (info->getKind() == TR_ArrayStoreCheckGuard)
      {
      guardedClass = info->getThisClass();
      if (comp->fe()->classHasBeenExtended(guardedClass))
         nopAssumptionIsValid = false;
      }
   else if (!resolvedMethodSymbol)
      {
      TR_ASSERT(methodSymbol->isInterface() && info->getKind() == TR_InterfaceGuard, "assertion failure");
      TR_OpaqueClassBlock *thisClass = info->getThisClass();
      TR_ASSERT(thisClass, "assertion failure");

      TR_ResolvedMethod *implementer = table->findSingleImplementer(thisClass, cpIndex, owningMethod, comp, true, TR_yes);
      if (!implementer ||
          (info->getTestType() == TR_VftTest && comp->fe()->classHasBeenExtended(implementer->containingClass())))
         nopAssumptionIsValid = false;
      else
         TR_ClassQueries::addAnAssumptionForEachSubClass(table, table->findClassInfo(thisClass), sites, comp);
      }
   else if (info->getKind() == TR_NonoverriddenGuard && info->getTestType() != TR_VftTest)
      {
      TR_ASSERT(info->getTestType() == TR_NonoverriddenTest, "unexpected test type for a non-overridden guard.");

      guardedMethod = resolvedMethodSymbol->getResolvedMethod();
      if (guardedMethod->virtualMethodIsOverridden())
         nopAssumptionIsValid = false;
      }
   else if ((!info->isInlineGuard() &&
          !TR::Compiler->cls.isAbstractClass(comp, resolvedMethodSymbol->getResolvedMethod()->containingClass())) ||
          (info->isInlineGuard() && info->getKind() == TR_HierarchyGuard && info->getTestType() == TR_MethodTest))
      {
      guardedMethod = resolvedMethodSymbol->getResolvedMethod();
      TR_ASSERT(guardedMethod, "Method must have been found when creating assumption\n");

      TR_DevirtualizedCallInfo *dc;
      TR_OpaqueClassBlock *thisClass = info->isInlineGuard() ? info->getThisClass()
         : ((dc = comp->findDevirtualizedCall(info->getCallNode())) ? dc->_thisType :
          guardedMethod->classOfMethod());

     //temp fix for defect 57599 until we find root cause
      if (table->isOverriddenInThisHierarchy(guardedMethod, thisClass, symRef->getOffset(), comp))
         nopAssumptionIsValid = false;
      }
   else if (info->isInlineGuard() &&
            info->getTestType() == TR_VftTest &&
            (info->getKind() == TR_NonoverriddenGuard || info->getKind() == TR_HierarchyGuard))
      {
      guardedClass = info->getThisClass();
      if (comp->fe()->classHasBeenExtended(guardedClass))
         nopAssumptionIsValid = false;
      }
   else if ((!info->isInlineGuard() &&
             TR::Compiler->cls.isAbstractClass(comp, resolvedMethodSymbol->getResolvedMethod()->containingClass())) ||
            (info->isInlineGuard() && info->getKind() == TR_AbstractGuard && info->getTestType() == TR_MethodTest))
      {
      TR_OpaqueClassBlock *thisClass = info->isInlineGuard() ? thisClass = info->getThisClass()
         : resolvedMethodSymbol->getResolvedMethod()->containingClass();
      if (table->findSingleAbstractImplementer(thisClass, symRef->getOffset(), owningMethod, comp, true))
         TR_ClassQueries::addAnAssumptionForEachSubClass(table, table->findClassInfo(thisClass), sites, comp);
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
      ListIterator<TR_VirtualGuardSite> it(&sites);
      for (TR_VirtualGuardSite *site = it.getFirst(); site; site = it.getNext())
         {
         TR_ASSERT(site->getLocation(), "assertion failure");
         if (guardedClass)
            {
            TR_PatchNOPedGuardSiteOnClassExtend
               ::make(comp->fe(), comp->trPersistentMemory(), guardedClass, site->getLocation(), site->getDestination(), comp->getMetadataAssumptionList());
            comp->setHasClassExtendAssumptions();
            }
         if (guardedMethod)
            {
            TR_PatchNOPedGuardSiteOnMethodOverride
               ::make(comp->fe(), comp->trPersistentMemory(), guardedMethod->getPersistentIdentifier(),
                site->getLocation(), site->getDestination(), comp->getMetadataAssumptionList());
            comp->setHasMethodOverrideAssumptions();
            }
         }
      }
   else // assumption is invalid
      {
      ListIterator<TR_VirtualGuardSite> it(&sites);
      for (TR_VirtualGuardSite *site = it.getFirst(); site; site = it.getNext())
         {
         if (comp->getOption(TR_TraceCG))
            traceMsg(comp, "   Patching %p to %p\n", site->getLocation(), site->getDestination());
         TR::PatchNOPedGuardSite::compensate(0, site->getLocation(), site->getDestination());
         }
      }
   }

#if defined(J9VM_OPT_JITSERVER)
VirtualGuardInfoForCHTable getImportantVGuardInfo(TR::Compilation *comp, TR_VirtualGuard *vguard)
   {
   VirtualGuardInfoForCHTable info;
   info._testType = vguard->getTestType();
   info._kind = vguard->getKind();
   info._calleeIndex = vguard->getCalleeIndex();
   info._byteCodeIndex = vguard->getByteCodeIndex();

   // info below is not used if there are no nop sites
   if (vguard->getNOPSites().isEmpty())
      return info;

   TR::SymbolReference *symRef = vguard->getSymbolReference();
   if (symRef)
      {
      TR::MethodSymbol *methodSymbol = symRef->getSymbol()->castToMethodSymbol();
      TR::ResolvedMethodSymbol *resolvedMethodSymbol = methodSymbol->getResolvedMethodSymbol();
      info._hasResolvedMethodSymbol = resolvedMethodSymbol != NULL;
      info._cpIndex = symRef->getCPIndex();
      info._owningMethod = static_cast<TR_ResolvedJ9JITServerMethod*>(symRef->getOwningMethod(comp))->getRemoteMirror();
      info._isInterface = methodSymbol->isInterface();
      info._guardedMethod = resolvedMethodSymbol ? static_cast<TR_ResolvedJ9JITServerMethod*>(resolvedMethodSymbol->getResolvedMethod())->getRemoteMirror() : NULL;
      info._offset = symRef->getOffset();

      info._isInlineGuard = vguard->isInlineGuard();
      TR_DevirtualizedCallInfo *dc;

      if (resolvedMethodSymbol)
         info._guardedMethodThisClass = vguard->isInlineGuard()
            ? vguard->getThisClass()
            : ((dc = comp->findDevirtualizedCall(vguard->getCallNode()))
               ? dc->_thisType
               : resolvedMethodSymbol->getResolvedMethod()->classOfMethod());
      }

   info._thisClass = vguard->getThisClass();
   info._mergedWithHCRGuard = vguard->mergedWithHCRGuard();
   info._mergedWithOSRGuard = vguard->mergedWithOSRGuard();

   info._mutableCallSiteObject = info._kind == TR_MutableCallSiteTargetGuard ? vguard->mutableCallSiteObject() : NULL;
   info._mutableCallSiteEpoch = info._kind == TR_MutableCallSiteTargetGuard ? vguard->mutableCallSiteEpoch() : -1;

   info._inlinedResolvedMethod = info._kind == TR_BreakpointGuard
      ? static_cast<TR_ResolvedJ9JITServerMethod *>(comp->getInlinedResolvedMethod(info._calleeIndex))->getRemoteMirror()
      : NULL;

   return info;
   }

CHTableCommitData
TR_CHTable::computeDataForCHTableCommit(TR::Compilation *comp)
   {
   // collect info from TR_CHTable
   std::vector<TR_OpaqueClassBlock*> classes = _classes
      ? std::vector<TR_OpaqueClassBlock*>(&(_classes->element(0)), (&(_classes->element(_classes->lastIndex()))) + 1)
      : std::vector<TR_OpaqueClassBlock*>();
   std::vector<TR_OpaqueClassBlock*> classesThatShouldNotBeNewlyExtended = _classesThatShouldNotBeNewlyExtended
      ? std::vector<TR_OpaqueClassBlock*>(&_classesThatShouldNotBeNewlyExtended->element(0), &_classesThatShouldNotBeNewlyExtended->element(_classesThatShouldNotBeNewlyExtended->lastIndex()) + 1)
      : std::vector<TR_OpaqueClassBlock*>();
   std::vector<TR_ResolvedMethod*> preXMethods(_preXMethods ? _preXMethods->size() : 0);
   for (size_t i = 0; i < preXMethods.size(); i++)
      {
      TR_ResolvedMethod *method = _preXMethods->element(i);
      TR_ResolvedJ9JITServerMethod *JITServerMethod = static_cast<TR_ResolvedJ9JITServerMethod*>(method);
      preXMethods[i] = JITServerMethod->getRemoteMirror();
      }

   cleanupNewlyExtendedInfo(comp);

   // collect virtual guard info
   TR::list<TR_VirtualGuard*> &vguards = comp->getVirtualGuards();
   std::vector<VirtualGuardForCHTable> serialVGuards;
   serialVGuards.reserve(vguards.size());

   for (TR_VirtualGuard* vguard : vguards)
      {
      VirtualGuardInfoForCHTable info = getImportantVGuardInfo(comp, vguard);

      std::vector<TR_VirtualGuardSite> nopSites;

         {
         List<TR_VirtualGuardSite> &sites = vguard->getNOPSites();
         ListIterator<TR_VirtualGuardSite> it(&sites);
         for (TR_VirtualGuardSite *site = it.getFirst(); site; site = it.getNext())
            nopSites.push_back(*site);
         }

      std::vector<VirtualGuardInfoForCHTable> innerAssumptions;
         {
         ListIterator<TR_InnerAssumption> it(&vguard->getInnerAssumptions());
         for (TR_InnerAssumption *inner = it.getFirst(); inner; inner = it.getNext())
            {
            VirtualGuardInfoForCHTable innerInfo = getImportantVGuardInfo(comp, vguard);
            innerAssumptions.push_back(innerInfo);
            }
         }

      serialVGuards.push_back(std::make_tuple(info, nopSites, innerAssumptions));
      }

   TR::list<TR_VirtualGuardSite*> &sideEffectPatchSites = *comp->getSideEffectGuardPatchSites();
   std::vector<TR_VirtualGuardSite> sideEffectPatchSitesVec;
   for (TR_VirtualGuardSite *site : sideEffectPatchSites)
      sideEffectPatchSitesVec.push_back(*site);

   FlatClassLoadCheck compClassesThatShouldNotBeLoaded;
   for (TR_ClassLoadCheck * clc = comp->getClassesThatShouldNotBeLoaded()->getFirst(); clc; clc = clc->getNext())
      compClassesThatShouldNotBeLoaded.emplace_back(std::string(clc->_name, clc->_length));

   FlatClassExtendCheck compClassesThatShouldNotBeNewlyExtended;
   for (TR_ClassExtendCheck * cec = comp->getClassesThatShouldNotBeNewlyExtended()->getFirst(); cec; cec = cec->getNext())
      compClassesThatShouldNotBeNewlyExtended.emplace_back(cec->_clazz);

   auto *compClassesForOSRRedefinition = comp->getClassesForOSRRedefinition();
   std::vector<TR_OpaqueClassBlock*> classesForOSRRedefinition(compClassesForOSRRedefinition->size());
   for (int i = 0; i < compClassesForOSRRedefinition->size(); ++i)
      classesForOSRRedefinition[i] = (*compClassesForOSRRedefinition)[i];

   uint8_t *startPC = comp->cg()->getCodeStart();

   return std::make_tuple(classes,
                          classesThatShouldNotBeNewlyExtended,
                          preXMethods,
                          sideEffectPatchSitesVec,
                          serialVGuards,
                          compClassesThatShouldNotBeLoaded,
                          compClassesThatShouldNotBeNewlyExtended,
                          classesForOSRRedefinition,
                          startPC);
   }
#endif

// class used to collect all different implementations of a method
class CollectImplementors : public TR_SubclassVisitor
   {
public:
   CollectImplementors(TR::Compilation * comp, TR_OpaqueClassBlock *topClassId, TR_ResolvedMethod **implArray, int32_t maxCount,
                       TR_ResolvedMethod *callerMethod, int32_t slotOrIndex, TR_YesNoMaybe useGetResolvedInterfaceMethod = TR_maybe) : TR_SubclassVisitor(comp)
      {
      _comp          = comp;
      _topClassId    = topClassId;
      _implArray     = implArray;
      _callerMethod  = callerMethod;
      _maxCount      = maxCount;
      _slotOrIndex   = slotOrIndex;
      _count         = 0;
      _topClassIsInterface = TR::Compiler->cls.isInterfaceClass(comp, topClassId);
      _maxNumVisitedSubClasses = comp->getOptions()->getMaxNumVisitedSubclasses();
      _numVisitedSubClasses = 0;
      _useGetResolvedInterfaceMethod = useGetResolvedInterfaceMethod;
      }

   virtual bool visitSubclass(TR_PersistentClassInfo *cl);
   int32_t getCount() const { return _count;}

protected:
   TR_OpaqueClassBlock * _topClassId;
   TR_ResolvedMethod **  _implArray;
   TR_ResolvedMethod *   _callerMethod;
   int32_t               _maxCount;
   int32_t               _slotOrIndex;
   int32_t               _count;
   bool                  _topClassIsInterface;
   int32_t               _maxNumVisitedSubClasses;
   int32_t               _numVisitedSubClasses;
   TR_YesNoMaybe         _useGetResolvedInterfaceMethod;
   };// end of class CollectImplementors


bool CollectImplementors::visitSubclass(TR_PersistentClassInfo *cl)
   {
   TR_OpaqueClassBlock *classId = cl->getClassId();
   // verify that our subclass meets all conditions
   if (!TR::Compiler->cls.isAbstractClass(comp(), classId) && !TR::Compiler->cls.isInterfaceClass(comp(), classId))
      {
      int32_t length;
      char *clazzName = TR::Compiler->cls.classNameChars(comp(), classId, length);
      TR_ResolvedMethod *method;

      bool useGetResolvedInterfaceMethod = _topClassIsInterface;

      //refine if requested by a caller
      if (_useGetResolvedInterfaceMethod != TR_maybe)
         useGetResolvedInterfaceMethod = _useGetResolvedInterfaceMethod == TR_yes ? true : false;

      if (useGetResolvedInterfaceMethod)
         {
         method = _callerMethod->getResolvedInterfaceMethod(comp(), classId, _slotOrIndex);
         }
      else
         {
         method = _callerMethod->getResolvedVirtualMethod(comp(), classId, _slotOrIndex);
         }

      ++_numVisitedSubClasses;
      if ((_numVisitedSubClasses > _maxNumVisitedSubClasses) || !method)
         {
         // set count greater than maxCount, to indicate failure and force exit
         _count = _maxCount + 1;
         stopTheWalk();
         return false;
         }
      // check to see if there are any duplicates
      int32_t i;
      for (i=0; i < _count; i++)
         if (method->isSameMethod(_implArray[i]))
            break;  // we already listed this method
      if (i >= _count) // brand new implementer
         {
         _implArray[_count++] = method;
         if (_count >= _maxCount)
            stopTheWalk();
         }
      }
   return true;
   }

class CollectCompiledImplementors : public CollectImplementors
   {
   public:
   CollectCompiledImplementors(TR::Compilation * comp, TR_OpaqueClassBlock *topClassId, TR_ResolvedMethod **implArray, int32_t maxCount,
                       TR_ResolvedMethod *callerMethod, int32_t slotOrIndex, TR_Hotness hotness, TR_YesNoMaybe useGetResolvedInterfaceMethod = TR_maybe) : CollectImplementors(comp, topClassId, implArray, maxCount+1, callerMethod, slotOrIndex, useGetResolvedInterfaceMethod)
      {
      _hotness = hotness;
      }
   virtual bool visitSubclass(TR_PersistentClassInfo *cl);
   private:
   TR_Hotness _hotness;
   };

bool CollectCompiledImplementors::visitSubclass(TR_PersistentClassInfo *cl)
   {
   int32_t currentCount = _count;
   if (CollectImplementors::visitSubclass(cl))
      {
      if (currentCount < _count)
         {
         if (!TR::Compiler->mtd.isCompiledMethod((_implArray[_count-1])->getPersistentIdentifier()))
            {
            _count -= 1;
            }
         else
            {
            TR_PersistentJittedBodyInfo * bodyInfo = ((TR_ResolvedJ9Method*) _implArray[_count - 1])->getExistingJittedBodyInfo();
            if (!bodyInfo || bodyInfo->getHotness() < _hotness)
               {
               _count -= 1;
               }
            if (_count >= _maxCount - 1)
               {
               stopTheWalk();
               }
            }
         }
      return true;
      }
   return false;
   }

void
TR_ClassQueries::getSubClasses(TR_PersistentClassInfo *clazz,
                               TR_ScratchList<TR_PersistentClassInfo> &list, TR_FrontEnd *fe, bool locked)
   {
   TR::ClassTableCriticalSection getSubClasses(fe, locked);

   for (TR_SubClass * subClass = clazz->_subClasses.getFirst(); subClass; subClass = subClass->getNext())
      list.add(subClass->getClassInfo());
   }


// this method will return the number of implementers stored in the given implArray
// NOTE: if the number is larger than maxCount, it means that the collection failed
// and we should not check the content of the implArray as it may contain bogus data
int32_t
TR_ClassQueries::collectImplementorsCapped(TR_PersistentClassInfo *clazz,
                                           TR_ResolvedMethod **implArray,
                                           int32_t maxCount, int32_t slotOrIndex,
                                           TR_ResolvedMethod *callerMethod,
                                           TR::Compilation *comp, bool locked,
                                           TR_YesNoMaybe useGetResolvedInterfaceMethod)
   {
   if (comp->getOption(TR_DisableCHOpts))
      return maxCount+1; // fail the collection
   CollectImplementors collector(comp, clazz->getClassId(), implArray, maxCount, callerMethod, slotOrIndex, useGetResolvedInterfaceMethod);
   collector.visitSubclass(clazz);
   collector.visit(clazz->getClassId(), locked);
   return collector.getCount(); // return the number of implementers in the implArray
   }

int32_t
TR_ClassQueries::collectCompiledImplementorsCapped(TR_PersistentClassInfo *clazz,
                                           TR_ResolvedMethod **implArray,
                                           int32_t maxCount, int32_t slotOrIndex,
                                           TR_ResolvedMethod *callerMethod,
                                           TR::Compilation *comp,
                                           TR_Hotness hotness, bool locked,
                                           TR_YesNoMaybe useGetResolvedInterfaceMethod)
   {
   if (comp->getOption(TR_DisableCHOpts))
      return maxCount+1; // fail the collection
   CollectCompiledImplementors collector(comp, clazz->getClassId(), implArray, maxCount, callerMethod, slotOrIndex, hotness, useGetResolvedInterfaceMethod);
   collector.visitSubclass(clazz);
   collector.visit(clazz->getClassId(), locked);
   return collector.getCount();
   }

void
TR_ClassQueries::collectLeafs(TR_PersistentClassInfo *clazz,
                             TR_ScratchList<TR_PersistentClassInfo> &leafs, TR::Compilation *comp, bool locked)
   {
   TR::ClassTableCriticalSection collectLeafs(comp->fe(), locked);

   // keep a list of classes that were set visited so that we can
   // easily reset the visited flag later-on
   TR_CHTable::VisitTracker marked(comp->trMemory());

   for (TR_SubClass *subClass = clazz->_subClasses.getFirst(); subClass; subClass = subClass->getNext())
      if (!subClass->getClassInfo()->hasBeenVisited())
         TR_ClassQueries::collectLeafsLocked(subClass->getClassInfo(), leafs, marked);
   }

void
TR_ClassQueries::collectLeafsLocked(TR_PersistentClassInfo*                 clazz,
                                    TR_ScratchList<TR_PersistentClassInfo>& leafs,
                                    TR_CHTable::VisitTracker&               marked)
   {
   marked.visit(clazz);
   TR_SubClass* subClass = clazz->_subClasses.getFirst();
   if (!subClass)
      leafs.add(clazz);
   else // visit all non-visited classes
      for (; subClass; subClass = subClass->getNext())
         if (!subClass->getClassInfo()->hasBeenVisited())
            TR_ClassQueries::collectLeafsLocked(subClass->getClassInfo(), leafs, marked);
   }

void
TR_ClassQueries::collectAllSubClasses(TR_PersistentClassInfo *clazz,
                                      TR_ScratchList<TR_PersistentClassInfo> *leafs,
                                      TR::Compilation *comp, bool locked)
   {
   TR::ClassTableCriticalSection collectAllSubClasses(comp->fe(), locked);

   // Defect 180426 We used to use the same 'leafs' list for both the result set and to track the list
   // of classes on which to call resetVisited(). This raises concerns about possible interactions
   // between invocations now that the reset list is held in TR::Compilation. To avoid problems
   // we're separating the two issues and using two lists.
   TR_CHTable::VisitTracker marked(comp->trMemory());

   TR_ClassQueries::collectAllSubClassesLocked(clazz, leafs, marked); // walk
   }

/*
 * Recursively walk the class tree to find subclasses.
 *
 * clazz        - the current class to find subclasses of.
 * leafs        - the result set
 * marked        - list of visited classes (used to clear the visited flags)
 */
void
TR_ClassQueries::collectAllSubClassesLocked(TR_PersistentClassInfo*                 clazz,
                                            TR_ScratchList<TR_PersistentClassInfo>* leafs,
                                            TR_CHTable::VisitTracker&               marked)
   {
   TR_SubClass * subClass = clazz->_subClasses.getFirst();

   for (; subClass; subClass = subClass->getNext())
      {
      if (!subClass->getClassInfo()->hasBeenVisited())
         {
         TR_PersistentClassInfo *sc = subClass->getClassInfo();
         leafs->add(sc);
         marked.visit(sc);
         TR_ClassQueries::collectAllSubClassesLocked(sc, leafs, marked);
         }
      }
   }

// class used to collect first layer of non-interface subclasses
class CollectNonIFSubClasses : public TR_SubclassVisitor
   {
public:
   CollectNonIFSubClasses(TR::Compilation * comp, TR_ScratchList<TR_PersistentClassInfo> &leafs) :
                     TR_SubclassVisitor(comp), _collection(leafs) {}
   virtual bool visitSubclass(TR_PersistentClassInfo *cl)
      {
      return (TR::Compiler->cls.isInterfaceClass(comp(), cl->getClassId()) ? true : (_collection.add(cl), false));
      }
private:
   TR_ScratchList<TR_PersistentClassInfo> &_collection;
   };// end of class CollectNonIFSubClasses

void
TR_ClassQueries::collectAllNonIFSubClasses(TR_PersistentClassInfo *clazz,
                                           TR_ScratchList<TR_PersistentClassInfo> &leafs, TR::Compilation *comp, bool locked)
   {
   CollectNonIFSubClasses collector(comp, leafs);
   collector.visit(clazz->getClassId(), locked);
   }

void
TR_ClassQueries::addAnAssumptionForEachSubClass(TR_PersistentCHTable   *table,
                                                TR_PersistentClassInfo *clazz,
                                                List<TR_VirtualGuardSite> &list,
                                                TR::Compilation *comp)
   {
   // Gather the subtree of classes rooted at clazz
   TR_ScratchList<TR_PersistentClassInfo> subTree(comp->trMemory());
   collectAllSubClasses(clazz, &subTree, comp);

   // Add the root to the list
   subTree.add(clazz);

   ListIterator<TR_VirtualGuardSite> it(&list);
   for (TR_VirtualGuardSite *site = it.getFirst(); site; site = it.getNext())
      {
      ListIterator<TR_PersistentClassInfo> classIt(&subTree);
      for (TR_PersistentClassInfo *sc = classIt.getFirst(); sc; sc = classIt.getNext())
         {
         TR_ASSERT(sc == table->findClassInfo(sc->getClassId()), "Class ID mismatch");
         TR_PatchNOPedGuardSiteOnClassExtend::make(comp->fe(), comp->trPersistentMemory(), sc->getClassId(),
                                                                  site->getLocation(),
                                                                  site->getDestination(),
                                                                  comp->getMetadataAssumptionList());
         comp->setHasClassExtendAssumptions();
         }
      }
   }

// class used to count all non-interface subclasses up to a maximum
class CountNonIFSubClasses : public TR_SubclassVisitor
   {
public:
   CountNonIFSubClasses(TR::Compilation * comp, int32_t maxCount) :
                     TR_SubclassVisitor(comp), _maxCount(maxCount), _count(0) {}
   virtual bool visitSubclass(TR_PersistentClassInfo *cl)
      {
      if (!TR::Compiler->cls.isInterfaceClass(comp(), cl->getClassId()))
         if (++_count >= _maxCount)
            stopTheWalk();
      return true;
      }
   int32_t getCount() const { return _count; }
private:
   int32_t _maxCount, _count;
   };// end of class CountNonIFSubClasses

int32_t
TR_ClassQueries::countAllNonIFSubClassesWithDepth(TR_PersistentClassInfo *clazz,
                                                  TR::Compilation *comp, int32_t maxcount, bool locked)
   {
   CountNonIFSubClasses collector(comp, maxcount);
   collector.visit(clazz->getClassId(), locked);
   return collector.getCount();
   }


TR_SubclassVisitor::TR_SubclassVisitor(TR::Compilation * comp)
   : _comp(comp),
     _stopTheWalk(false),
     _depth(0)
   {
   static char * trace = feGetEnv("TR_TraceSubclassVisitor");
   _trace = (trace != 0);
   }

void
TR_SubclassVisitor::visit(TR_OpaqueClassBlock * clazz, bool locked)
   {
   TR::ClassTableCriticalSection visit(fe(), locked);

   TR_PersistentClassInfo * classInfo = comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfo(clazz);
   if (!classInfo)
      return;

   // A class can be visited more than once if we're starting from an interface class or if we're starting from
   // java/lang/Object.  The latter is because all interfaces extend java/lang/Object.
   // java/lang/Object has the property that it doesn't have a super class (classdepth == 0).
   //
   _mightVisitAClassMoreThanOnce = TR::Compiler->cls.isInterfaceClass(comp(), clazz) || TR::Compiler->cls.classDepthOf(clazz) == 0;

   if (_trace && classInfo->getFirstSubclass())
      {
      int32_t len; char * s = TR::Compiler->cls.classNameChars(comp(), clazz, len);
      TR_VerboseLog::writeLine(TR_Vlog_INFO,"visiting subclasses for %.*s", len, s);
      }

   TR_CHTable::VisitTracker visited(comp()->trMemory());
   visitSubclasses(classInfo, visited);
   }

void
TR_SubclassVisitor::visitSubclasses(TR_PersistentClassInfo * classInfo, TR_CHTable::VisitTracker& visited)
   {
   ++_depth;
   for (TR_SubClass * subclass = classInfo->getFirstSubclass(); subclass; subclass = subclass->getNext())
      {
      TR_PersistentClassInfo * subclassInfo = subclass->getClassInfo();
      if (!subclassInfo->hasBeenVisited())
         {
         if (_trace)
            {
            int32_t len; char * s = TR::Compiler->cls.classNameChars(comp(), subclassInfo->getClassId(), len);
            TR_VerboseLog::writeLine(TR_Vlog_INFO,"%*s%.*s", _depth, " ", len, s);
            }

         if (_mightVisitAClassMoreThanOnce)
            {
            visited.visit(subclassInfo);
            }

         bool recurse = visitSubclass(subclassInfo);
         if (recurse && !_stopTheWalk)
            visitSubclasses(subclassInfo, visited);

         if (_stopTheWalk)
            break;
         }
      }
   --_depth;
   }

