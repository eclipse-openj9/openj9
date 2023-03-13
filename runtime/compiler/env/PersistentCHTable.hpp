/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef TR_PERSISTENTCHTABLE_INCL
#define TR_PERSISTENTCHTABLE_INCL

#include <stdint.h>
#include "compile/CompilationTypes.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "infra/Link.hpp"
#include "infra/TRlist.hpp"
#include "runtime/RuntimeAssumptions.hpp"

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
namespace TR { class CompilationInfo; }

class TR_PersistentCHTable
   {
   private:
   /**
    * @brief Enum to denote the state of the CH Table:
    *
    *        reset:            CH Table is not active.
    *        active:           Data in the CH Table can be used and updated.
    *        activating:       CH Table is in the process of being activated;
    *                          this state is used when the CH Table is
    *                          being populated in the restore run.
    *        activationFailed: CH Table failed activation.
    */
   enum Status
      {
      reset            = 0x0,
      active           = 0x1,
      activating       = 0x2,
      activationFailed = 0x3
      };

   public:
   TR_ALLOC(TR_Memory::PersistentCHTable)

   TR_PersistentCHTable(TR_PersistentMemory *);

   virtual TR_PersistentClassInfo * findClassInfo(TR_OpaqueClassBlock * classId);

   virtual TR_PersistentClassInfo * findClassInfoAfterLocking(TR_OpaqueClassBlock * classId, TR::Compilation *, bool returnClassInfoForAOT = false);
   virtual TR_PersistentClassInfo * findClassInfoAfterLocking(TR_OpaqueClassBlock * classId, TR_FrontEnd *, bool returnClassInfoForAOT = false);

   void dumpMethodCounts(TR_FrontEnd *fe, TR_Memory &trMemory);         // A routine to dump initial method compilation counts

   virtual void commitSideEffectGuards(TR::Compilation *c);
   virtual bool isOverriddenInThisHierarchy(TR_ResolvedMethod *, TR_OpaqueClassBlock *, int32_t, TR::Compilation *comp, bool locked = false);

   virtual TR_ResolvedMethod * findSingleInterfaceImplementer(TR_OpaqueClassBlock *, int32_t, TR_ResolvedMethod *, TR::Compilation *, bool locked = false, bool validate = true);
   virtual TR_ResolvedMethod * findSingleAbstractImplementer(TR_OpaqueClassBlock *, int32_t, TR_ResolvedMethod *, TR::Compilation *, bool locked = false, bool validate = true);

   // profiler
   virtual bool isKnownToHaveMoreThanTwoInterfaceImplementers(TR_OpaqueClassBlock *, int32_t, TR_ResolvedMethod *, TR::Compilation *, bool locked = false);

   // optimizer
   virtual TR_OpaqueClassBlock *findSingleConcreteSubClass(TR_OpaqueClassBlock *, TR::Compilation *, bool validate = true);
   virtual TR_ResolvedMethod * findSingleImplementer(TR_OpaqueClassBlock * thisClass, int32_t cpIndexOrVftSlot, TR_ResolvedMethod * callerMethod, TR::Compilation * comp, bool locked, TR_YesNoMaybe useGetResolvedInterfaceMethod, bool validate = true);

   virtual bool hasThreeOrMoreCompiledImplementors(TR_OpaqueClassBlock *, int32_t, TR_ResolvedMethod *, TR::Compilation *, TR_Hotness hotness = warm, bool locked = false);
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
   virtual void resetVisitedClasses(); // highly time consuming


   template <class Alloc = TR::PersistentAllocator>
   class VisitTracker
      {
      public:
      VisitTracker(Alloc &alloc) : _visited(alloc) {}
      ~VisitTracker()
         {
         for (auto iter = _visited.begin(); iter != _visited.end(); iter++)
            {
            (*iter)->resetVisited();
            }
         }
      void visit(TR_PersistentClassInfo* info)
         {
         _visited.push_front(info);
         info->setVisited();
         }

      private:
      TR::list<TR_PersistentClassInfo *, Alloc&> _visited;
      };

   typedef TR::list<TR_PersistentClassInfo *, TR::PersistentAllocator&> ClassList;

   /**
    * @brief Collects all subclasses of a given class into the ClassList container passed in
    *
    * @param clazz The class whose list of subclasses is required
    * @param classList the container to hold the list of subclasses
    * @param comp TR_J9VMBase object
    * @param locked indicates where the class hierarchy mutex has already been acquired
    */
   void collectAllSubClasses(TR_PersistentClassInfo *clazz, ClassList &classList, TR_J9VMBase *fej9, bool locked = false);

   /**
    * @brief Resets the Cached CCV Result for class passed in as well as all
    *        subclasses
    *
    * @param fej9 TR_J9VMBase object
    * @param clazz The class whose cached CCV Result (as well as those of its subclasses) is to be reset.
    */
   void resetCachedCCVResult(TR_J9VMBase *fej9, TR_OpaqueClassBlock *clazz);

   /**
    * @brief API to activate the CH Table. This API should only be used on the restore run. It also
    *        should only be run with exclusive access in hand; this is because this method goes through
    *        all J9Classes, runs the necessary JIT Hooks, and adds the classes to the CH Table.
    *
    * @param vmThread J9VMThread pointer
    * @param fej9     TR_J9VMBase pointer
    * @param compInfo TR::CompilationInfo pointer
    *
    * @return true if successfully activated CH Table, false otherwise.
    */
   bool activate(J9VMThread *vmThread, TR_J9VMBase *fej9, TR::CompilationInfo *compInfo);

#ifdef DEBUG
   void dumpStats(TR_FrontEnd *);
#endif

   bool isActive() { return _status == Status::active; }
   bool isActivating() { return _status == Status::activating; }
   bool isAccessible() { return (isActive() || isActivating()); }
   bool failedToActivate() { return _status == Status::activationFailed; }

   protected:
   void removeAssumptionFromRAT(OMR::RuntimeAssumption *assumption);
   TR_LinkHead<TR_PersistentClassInfo> *getClasses() const { return _classes; }

   private:
   Status _status;

   /**
    * @brief Collects all subclasses of a given class into the ClassList container passed in; assumes
    *        that the class hierarchy mutex has been acquired
    *
    * @param clazz The class whose list of subclasses is required
    * @param classList the container to hold the list of subclasses
    * @param marked structure to track visited subclasses
    */
   void collectAllSubClassesLocked(TR_PersistentClassInfo *clazz, ClassList &classList, VisitTracker<> &marked);

   /**
    * @brief Recursive method used to add a class to the CH Table. The reason for the recursion is
    *        because of the way CH Table and JIT Hook code is structured - it assumes that the parent
    *        of a class is already in the CH Table. Therefore, this method does the following:
    *
    *          1. Add superclasses (recursive invocation)
    *          2. Add interfaces implemented (recursive invocation)
    *          3. Trigger class load hook
    *          4. Trigger class preinitialize hook
    *          5. Add array class (recursive invocation)
    *
    * @param vmThread J9VMThread pointer
    * @param jitConfig J9JITConfig pointer
    * @param j9clazz Pointer to J9Class of class to be added to CH Table
    * @param compInfo TR::CompilationInfo pointer
    *
    * @return true if successfully added class to CH Table, false otherwise.
    */
   bool addClassToTable(J9VMThread *vmThread,
                        J9JITConfig *jitConfig,
                        J9Class *j9clazz,
                        TR::CompilationInfo *compInfo);

   void resetStatus() { _status = Status::reset; }
   void setActive() { _status = Status::active; }
   void setActivating() { _status = Status::activating; }
   void setFailedToActivate() { _status = Status::activationFailed; }

   uint8_t _buffer[sizeof(TR_LinkHead<TR_PersistentClassInfo>) * (CLASSHASHTABLE_SIZE + 1)];
   TR_LinkHead<TR_PersistentClassInfo> *_classes;

   protected:
   TR_PersistentMemory *_trPersistentMemory;
   };

#endif
