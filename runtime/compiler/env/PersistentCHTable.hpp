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

#ifndef TR_PERSISTENTCHTABLE_INCL
#define TR_PERSISTENTCHTABLE_INCL

#include <stdint.h>
#include "compile/CompilationTypes.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "infra/Link.hpp"

#define CLASSHASHTABLE_SIZE  (4001) // close to 8000 classes will be loaded in WebSphere

class TR_FrontEnd;
class TR_OpaqueClassBlock;
class TR_OpaqueMethodBlock;
class TR_PersistentClassInfo;
class TR_ResolvedMethod;
namespace OMR { class RuntimeAssumption; }
class TR_RuntimeAssumptionTable;
namespace TR { class Compilation; }
namespace TR { class ResolvedMethodSymbol; }

class TR_PersistentCHTable
   {
   public:
   TR_ALLOC(TR_Memory::PersistentCHTable)

   TR_PersistentCHTable(TR_PersistentMemory *);

   TR_PersistentClassInfo * findClassInfo(TR_OpaqueClassBlock * classId);

   TR_PersistentClassInfo * findClassInfoAfterLocking(TR_OpaqueClassBlock * classId, TR::Compilation *, bool returnClassInfoForAOT = false, bool validate = true);
   TR_PersistentClassInfo * findClassInfoAfterLocking(TR_OpaqueClassBlock * classId, TR_FrontEnd *, bool returnClassInfoForAOT = false);

   void dumpMethodCounts(TR_FrontEnd *fe, TR_Memory &trMemory);         // A routine to dump initial method compilation counts

   void commitSideEffectGuards(TR::Compilation *c);
   bool isOverriddenInThisHierarchy(TR_ResolvedMethod *, TR_OpaqueClassBlock *, int32_t, TR::Compilation *comp, bool locked = false);

   TR_ResolvedMethod * findSingleInterfaceImplementer(TR_OpaqueClassBlock *, int32_t, TR_ResolvedMethod *, TR::Compilation *, bool locked = false, bool validate = true);
   TR_ResolvedMethod * findSingleAbstractImplementer(TR_OpaqueClassBlock *, int32_t, TR_ResolvedMethod *, TR::Compilation *, bool locked = false, bool validate = true);

   // profiler
   bool isKnownToHaveMoreThanTwoInterfaceImplementers(TR_OpaqueClassBlock *, int32_t, TR_ResolvedMethod *, TR::Compilation *, bool locked = false);

   // optimizer
   TR_OpaqueClassBlock *findSingleConcreteSubClass(TR_OpaqueClassBlock *, TR::Compilation *, bool validate = true);
   TR_ResolvedMethod * findSingleImplementer(TR_OpaqueClassBlock * thisClass, int32_t cpIndexOrVftSlot, TR_ResolvedMethod * callerMethod, TR::Compilation * comp, bool locked, TR_YesNoMaybe useGetResolvedInterfaceMethod, bool validate = true);

   bool hasThreeOrMoreCompiledImplementors(TR_OpaqueClassBlock *, int32_t, TR_ResolvedMethod *, TR::Compilation *, TR_Hotness hotness = warm, bool locked = false);
   int32_t findnInterfaceImplementers(TR_OpaqueClassBlock *, int32_t, TR_ResolvedMethod *implArray[], int32_t, TR_ResolvedMethod *, TR::Compilation *, bool locked = false);
   TR_ResolvedMethod * findSingleJittedImplementer(TR_OpaqueClassBlock *, int32_t, TR_ResolvedMethod *, TR::Compilation *, TR::ResolvedMethodSymbol*, bool locked = false);

   // J9 below

   void methodGotOverridden(TR_FrontEnd *, TR_PersistentMemory *, TR_OpaqueMethodBlock *overriddingMethod, TR_OpaqueMethodBlock *overriddenMethod, int32_t smpFlag);
   TR_PersistentClassInfo *classGotLoaded(TR_FrontEnd *, TR_OpaqueClassBlock *classId);
   bool classGotInitialized(TR_FrontEnd* vm, TR_PersistentMemory *, TR_OpaqueClassBlock *classId, TR_PersistentClassInfo *clazz = 0);
   bool classGotExtended(TR_FrontEnd *vm, TR_PersistentMemory *, TR_OpaqueClassBlock *superClassId, TR_OpaqueClassBlock *subClassId);
   void classGotUnloaded(TR_FrontEnd *vm, TR_OpaqueClassBlock *classId);
   void classGotUnloadedPost(TR_FrontEnd *fe, TR_OpaqueClassBlock *classId);
   void classGotRedefined(TR_FrontEnd *vm, TR_OpaqueClassBlock *oldClassId, TR_OpaqueClassBlock *newClassId);
   void removeClass(TR_FrontEnd *, TR_OpaqueClassBlock *classId, TR_PersistentClassInfo *info, bool removeInfo);
   void resetVisitedClasses(); // highly time consuming

#ifdef DEBUG
   void dumpStats(TR_FrontEnd *);
#endif

   protected:
   void removeAssumptionFromRAT(OMR::RuntimeAssumption *assumption);

   private:
   uint8_t _buffer[sizeof(TR_LinkHead<TR_PersistentClassInfo>) * (CLASSHASHTABLE_SIZE + 1)];
   TR_LinkHead<TR_PersistentClassInfo> *_classes;
   TR_PersistentMemory *_trPersistentMemory;
   };

#endif
