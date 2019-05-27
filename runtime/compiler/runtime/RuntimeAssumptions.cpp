/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#include "runtime/RuntimeAssumptions.hpp"

#include "codegen/FrontEnd.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/CHTable.hpp"
#include "env/CompilerEnv.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/jittypes.h"
#include "infra/Monitor.hpp"
#include "infra/CriticalSection.hpp"
#include "control/CompilationRuntime.hpp"
#include "env/VMJ9.h"

namespace TR { class Monitor; }

extern "C" void _patchJNICallSite(J9Method *method, uint8_t *pc, uint8_t *newAddress, TR_FrontEnd *fe, int32_t smpFlag);

extern TR::Monitor *assumptionTableMutex;

TR_PatchNOPedGuardSiteOnClassPreInitialize *
TR_PatchNOPedGuardSiteOnClassPreInitialize::make(
   TR_FrontEnd *fe, TR_PersistentMemory * persistentMemory, char *sig, uint32_t sigLen, uint8_t *loc, uint8_t *dest, OMR::RuntimeAssumption ** sentinel)
   {
   char * persistentSig = (char*)persistentMemory->allocatePersistentMemory(sizeof(char) * sigLen);
   memcpy(persistentSig, sig, sigLen);
   TR_PatchNOPedGuardSiteOnClassPreInitialize *result = new (PERSISTENT_NEW) TR_PatchNOPedGuardSiteOnClassPreInitialize(persistentMemory, persistentSig, sigLen, loc, dest);
   result->addToRAT(persistentMemory, RuntimeAssumptionOnClassPreInitialize, fe, sentinel);
   return result;
   }


void
TR_PatchNOPedGuardSiteOnClassPreInitialize::compensate(TR_FrontEnd *fe, bool isSMP, void *data)
   {
   reclaim();
   TR::PatchNOPedGuardSite::compensate(fe, isSMP, data);
   }


uintptrj_t
TR_PatchNOPedGuardSiteOnClassPreInitialize::hashCode(char *sig, uint32_t sigLen)
   {
   uintptrj_t sum = 0;
   uintptrj_t scale = 1;
   bool skipFirstAndLastChars = false;
   if (sigLen > 0)
      {
      if ((sig[0] == 'L') && (sig[sigLen-1] == ';'))
         skipFirstAndLastChars = true;
      }

   int32_t firstChar;
   int32_t lastChar;
   if (skipFirstAndLastChars)
      {
      lastChar = sigLen - 2;
      firstChar = 1;
      }
   else
      {
      lastChar = sigLen - 1;
      firstChar = 0;
      }

   for (int32_t i = lastChar; i >= firstChar; --i, scale *= 31)
      sum += sig[i] * scale;
   return sum;
   }


bool
TR_PatchNOPedGuardSiteOnClassPreInitialize::matches(char *sig, uint32_t sigLen)
   {
   if (sigLen+2 != _sigLen) return false;
   char *mySig = (char*)getKey();
   uint32_t compareIndex = sigLen-1;
   for ( ; sigLen > 0; sigLen--)
      {
      if (mySig[compareIndex+1] != sig[compareIndex])
         return false;

      compareIndex--;
      }
   return true;
   }

TR_PatchNOPedGuardSiteOnMethodBreakPoint* TR_PatchNOPedGuardSiteOnMethodBreakPoint::make(
      TR_FrontEnd *fe, TR_PersistentMemory * pm, TR_OpaqueMethodBlock *method, uint8_t *location, uint8_t *destination,
      OMR::RuntimeAssumption **sentinel)
   {
   TR_PatchNOPedGuardSiteOnMethodBreakPoint *result = new (pm) TR_PatchNOPedGuardSiteOnMethodBreakPoint(pm, method, location, destination);
   result->addToRAT(pm, RuntimeAssumptionOnMethodBreakPoint, fe, sentinel);
   return result;
   }


void
TR_PreXRecompile::compensate(TR_FrontEnd *fe, bool, void *)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;

#if (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
   TR::Recompilation::invalidateMethodBody(_startPC, fe);
   // Generate a trace point
   fej9->reportPrexInvalidation(_startPC);
#else
   TR_ASSERT(0, "preexistence is not implemented on this platform yet");
#endif
   }


void
TR_PatchJNICallSite::compensate(
      TR_FrontEnd *fe,
      bool isSMP,
      void *newAddress)
   {
#if (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
   _patchJNICallSite((J9Method*)getKey(), getPc(), (uint8_t*) newAddress, fe, isSMP);
#else
   TR_ASSERT(0, "Direct JNI support not present on this platform yet");
#endif
   }



/* This method is called at the classes unload event hook.
   It is called for each class that wes unloaded during the per-class phase.
   It performs a walk over all superclasses of the class and removes all subclasses that were unloaded.
   We mark the superclasses/interfaces seen, so we will not visit it again .
*/

void
TR_PersistentCHTable::classGotUnloadedPost(
      TR_FrontEnd *fe,
      TR_OpaqueClassBlock *classId)
   {
   TR_PersistentClassInfo * cl;
   int classDepth;
   J9Class *clazzPtr;
   J9Class *superCl;
   TR_OpaqueClassBlock *superClId;
   TR_PersistentClassInfo * scl;
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;

   bool p = TR::Options::getVerboseOption(TR_VerboseHookDetailsClassUnloading);
   if (p)
      {
      TR_VerboseLog::writeLineLocked(TR_Vlog_HD, "subClasses clean up for unloaded class 0x%p \n", classId);
      }

   cl = findClassInfo(classId);
   classDepth = TR::Compiler->cls.classDepthOf(classId) - 1;
   uintptrj_t hashPos = TR_RuntimeAssumptionTable::hashCode((uintptrj_t)classId) % CLASSHASHTABLE_SIZE;
   _classes[hashPos].remove(cl);

   if ((classDepth >= 0) &&
       (cl->isInitialized() || fej9->isInterfaceClass(classId)))
      {
      clazzPtr = TR::Compiler->cls.convertClassOffsetToClassPtr(classId);
      superCl = clazzPtr->superclasses[classDepth];
      superClId = fej9->convertClassPtrToClassOffset(superCl);
      scl = findClassInfo(superClId);

      if (scl  && !(scl->hasBeenVisited()))
         {
         scl->removeUnloadedSubClasses();
         scl->setVisited(); //resetting is done in rossa
         _trPersistentMemory->getPersistentInfo()->addSuperClass(superClId);
         }

      for (J9ITable * iTableEntry = (J9ITable *)clazzPtr->iTable; iTableEntry; iTableEntry = iTableEntry->next)
         {
         superCl = iTableEntry->interfaceClass;
         if (superCl != clazzPtr)
            {
            superClId = fej9->convertClassPtrToClassOffset(superCl);
            scl = findClassInfo(superClId);
            if (scl && !(scl->hasBeenVisited()))
               {
               scl->removeUnloadedSubClasses();
               scl->setVisited(); //resetting is done in rossa
               _trPersistentMemory->getPersistentInfo()->addSuperClass(superClId);
               }
            }
         }

      }

   // cl was removed from all superclass/interfaces lists so we can free the memory now
   jitPersistentFree(cl);
   }


bool
TR_PersistentCHTable::classGotExtended(
      TR_FrontEnd *fe,
      TR_PersistentMemory *persistentMemory,
      TR_OpaqueClassBlock *superClassId,
      TR_OpaqueClassBlock *subClassId)
   {
   TR_PersistentClassInfo * cl = findClassInfo(superClassId);
   TR_PersistentClassInfo * subClass = findClassInfo(subClassId); // This is actually the class that got loaded extending the superclass
   // should have an assume0(cl && subClass) here - but assume does not work rt-code

   TR_SubClass *sc = cl->addSubClass(subClass); // Updating the hierarchy

   if (!sc)
      return false;

   TR_RuntimeAssumptionTable *table = persistentMemory->getPersistentInfo()->getRuntimeAssumptionTable();
   if (cl->shouldNotBeNewlyExtended())
      {
      TR::CompilationInfo *compInfo = TR::CompilationInfo::get();
      uint8_t mask = cl->getShouldNotBeNewlyExtendedMask().getValue();
      for (int32_t ID = 0; mask; mask>>=1, ++ID)
         {
         if (mask & 0x1)
            {
            // Determine the compilation thread that has this ID
            // and set the fail flag into its corresponding compilation object
            TR::Compilation *comp = compInfo->getCompilationWithID(ID);
            if (comp)
               comp->setFailCHTableCommit(true);
            else
               {
               // The compilation that set the bit has vanished
               // This can actually happen due to IPA which does not add the class
               // to the list of classes that should not be newly extended
               //TR_ASSERT(false, "Compilation has vanished class %p", superClassId);
               }
            }
         }
      cl->clearShouldNotBeNewlyExtended(); // flags are not needed anymore
      }

      {
      OMR::CriticalSection classGotExtended(assumptionTableMutex);
      OMR::RuntimeAssumption ** headPtr = table->getBucketPtr(RuntimeAssumptionOnClassExtend,
                                         TR_RuntimeAssumptionTable::hashCode((uintptrj_t) superClassId));
      for (OMR::RuntimeAssumption *cursor = *headPtr; cursor; cursor = cursor->getNext())
         {
         if (cursor->matches((uintptrj_t) superClassId))
            {
            cursor->compensate(fe, 0, 0);
            removeAssumptionFromRAT(cursor);
            }
         }
      }

   return true;
   }


void
TR_PersistentCHTable::removeClass(
      TR_FrontEnd *fe,
      TR_OpaqueClassBlock *classId,
      TR_PersistentClassInfo *info,
      bool removeInfo)
   {
   if (!info)
      return;

   TR_SubClass * subcl = info->getFirstSubclass();
   while (subcl)
      {
      TR_SubClass *nextScl = subcl->getNext();
      jitPersistentFree(subcl);
      subcl = nextScl;
      }

   J9Class *clazzPtr;
   J9Class *superCl;
   TR_OpaqueClassBlock *superClId;
   TR_PersistentClassInfo * scl;

   int classDepth = TR::Compiler->cls.classDepthOf(classId) - 1;
   uintptrj_t hashPos = TR_RuntimeAssumptionTable::hashCode((uintptrj_t)classId) % CLASSHASHTABLE_SIZE;

   if (classDepth >= 0)
      {
      clazzPtr = TR::Compiler->cls.convertClassOffsetToClassPtr(classId);
      superCl = clazzPtr->superclasses[classDepth];
      superClId = ((TR_J9VMBase *)fe)->convertClassPtrToClassOffset(superCl);
      scl = findClassInfo(superClId);
      if (scl)
         scl->removeASubClass(info);

      for (J9ITable * iTableEntry = (J9ITable *)clazzPtr->iTable; iTableEntry; iTableEntry = iTableEntry->next)
         {
         superCl = iTableEntry->interfaceClass;
         if (superCl != clazzPtr)
            {
            superClId = ((TR_J9VMBase *)fe)->convertClassPtrToClassOffset(superCl);
            scl = findClassInfo(superClId);
            if (scl)
               scl->removeASubClass(info);
            }
         }

      }

   if (!removeInfo)
      info->setFirstSubClass(0);
   else
      {
      _classes[hashPos].remove(info);
      info->removeSubClasses();
      jitPersistentFree(info);
      }
   }


bool
TR_PersistentCHTable::classGotInitialized(
      TR_FrontEnd *fe,
      TR_PersistentMemory *persistentMemory,
      TR_OpaqueClassBlock *classId,
      TR_PersistentClassInfo *clazz)
   {
   if (!clazz) clazz = findClassInfo(classId);

   clazz->setInitialized(persistentMemory);

   int32_t sigLen;
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
   char *sig = fej9->getClassNameChars(classId, sigLen);

   if (!sig)
      return false;

      {
      OMR::CriticalSection classGotInitialized(assumptionTableMutex);
      TR_RuntimeAssumptionTable *table = persistentMemory->getPersistentInfo()->getRuntimeAssumptionTable();
      OMR::RuntimeAssumption ** headPtr = table->getBucketPtr(RuntimeAssumptionOnClassPreInitialize, TR_PatchNOPedGuardSiteOnClassPreInitialize::hashCode(sig, sigLen));

      for (OMR::RuntimeAssumption *cursor = *headPtr; cursor; cursor = cursor->getNext())
         {
         if (cursor->matches(sig, sigLen))
            {
            cursor->compensate(fej9, 0, 0);
            removeAssumptionFromRAT(cursor);
            }
         }
      }

   return true;
   }


void
TR_PersistentCHTable::methodGotOverridden(
      TR_FrontEnd *fe,
      TR_PersistentMemory *persistentMemory,
      TR_OpaqueMethodBlock *overriddingMethod,
      TR_OpaqueMethodBlock *overriddenMethod,
      int32_t smpFlag)
   {
   OMR::CriticalSection methodGotOverridden(assumptionTableMutex);
   TR_RuntimeAssumptionTable *table = persistentMemory->getPersistentInfo()->getRuntimeAssumptionTable();
   OMR::RuntimeAssumption ** headPtr = table->getBucketPtr(RuntimeAssumptionOnMethodOverride,
                                        TR_RuntimeAssumptionTable::hashCode((uintptrj_t)overriddenMethod));
   for (OMR::RuntimeAssumption *cursor = *headPtr; cursor; cursor = cursor->getNext())
      {
      if (cursor->matches((uintptrj_t) overriddenMethod))
         {
         cursor->compensate(fe, 0, 0);
         removeAssumptionFromRAT(cursor);
         }
      }
   }


void
TR_PersistentCHTable::classGotRedefined(
      TR_FrontEnd *fe,
      TR_OpaqueClassBlock *oldClassId,
      TR_OpaqueClassBlock *newClassId)
   {
   TR_PersistentClassInfo *oldClass = findClassInfo(oldClassId);

   OMR::CriticalSection classGotRedefined(assumptionTableMutex);

   //
   // 1. Conservatively pretend the old class got extended
   //

   TR_RuntimeAssumptionTable *table = _trPersistentMemory->getPersistentInfo()->getRuntimeAssumptionTable();
   OMR::RuntimeAssumption **headPtr = table->getBucketPtr(RuntimeAssumptionOnClassExtend,
                                      TR_RuntimeAssumptionTable::hashCode((uintptrj_t) oldClassId));
   for (OMR::RuntimeAssumption *cursor = *headPtr; cursor; cursor = cursor->getNext())
      {
      if (cursor->matches((uintptrj_t) oldClassId))
         {
         cursor->compensate(fe, 0, 0);
         removeAssumptionFromRAT(cursor);
         }
      }

   //
   // 2. Swap the old and new classes in the hierarchy, and re-hash
   //

   TR_PersistentClassInfo *newClass = findClassInfo(newClassId);
   uintptrj_t oldIndex = TR_RuntimeAssumptionTable::hashCode((uintptrj_t)oldClassId) % CLASSHASHTABLE_SIZE;
   uintptrj_t newIndex = TR_RuntimeAssumptionTable::hashCode((uintptrj_t)newClassId) % CLASSHASHTABLE_SIZE;
   _classes[oldIndex].remove(oldClass);
   oldClass->setClassId(newClassId);
   _classes[newIndex].add(oldClass);

   // The new class should have had a class load event that would create a CHTable entry.
   // We'll use it to represent the moribund old class.
   //
   if (newClass)
      {
      _classes[newIndex].remove(newClass);
      newClass->setClassId(oldClassId);
      _classes[oldIndex].add(newClass);
      }
   }


void
TR_PersistentCHTable::removeAssumptionFromRAT(OMR::RuntimeAssumption *assumption)
   {
   TR_RuntimeAssumptionTable *rat = _trPersistentMemory->getPersistentInfo()->getRuntimeAssumptionTable();
   rat->markForDetachFromRAT(assumption);
   }
