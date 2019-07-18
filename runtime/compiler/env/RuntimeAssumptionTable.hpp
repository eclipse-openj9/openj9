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

#ifndef RUNTIMEASSUMPTIONTABLE_HPP
#define RUNTIMEASSUMPTIONTABLE_HPP

#include <stddef.h>
#include <stdint.h>
#include "env/jittypes.h"

class TR_FrontEnd;
class TR_OpaqueClassBlock;
namespace OMR { class RuntimeAssumption; }

// Note !!!: routine findAssumptionHashTable() assumes that the types of
// of assumptions start at 0 and end up with LastAssumptionKind.
// No holes are allowed !
enum TR_RuntimeAssumptionKind
   {
   RuntimeAssumptionOnClassUnload = 0,
   RuntimeAssumptionOnClassPreInitialize,
   RuntimeAssumptionOnClassExtend,
   RuntimeAssumptionOnMethodOverride,
   RuntimeAssumptionOnRegisterNative,
   RuntimeAssumptionOnClassRedefinitionPIC,
   RuntimeAssumptionOnClassRedefinitionUPIC,
   RuntimeAssumptionOnClassRedefinitionNOP,
   RuntimeAssumptionOnStaticFinalFieldModification,
   RuntimeAssumptionOnMutableCallSiteChange,
   RuntimeAssumptionOnMethodBreakPoint,
   LastAssumptionKind,
   // If you add another kind, add its name to the runtimeAssumptionKindNames array
   RuntimeAssumptionSentinel // This is special as there is no hashtable associated with it and we only create one of them
   };

char const * const runtimeAssumptionKindNames[LastAssumptionKind] =
   {
   "ClassUnload",
   "ClassPreInitialize",
   "ClassExtend",
   "MethodOverride",
   "RegisterNative",
   "ClassRedefinitionPIC",
   "ClassRedefinitionUPIC",
   "ClassRedefinitionNOP",
   "StaticFinalFieldModification",
   "MutableCallSiteChange",
   "OnMethodBreakpoint",
   };

struct TR_RatHT
   {
   OMR::RuntimeAssumption ** _htSpineArray;
   uint32_t              * _markedforDetachCount;
   size_t                  _spineArraySize;
   };

/// prime number
//#define ASSUMPTIONTABLE_SIZE 251
//#define CLASS_EXTEND_ASSUMPTIONTABLE_SIZE 1543  // other choices: 3079 6151
class TR_RuntimeAssumptionTable
   {
   friend class TR_DebugExt;
   public:
   TR_RuntimeAssumptionTable() {} // constructor of TR_RuntimeAssumptionTable
   bool init();  // Must call this during bootstrap on a single thread because it is not MT safe
   static uintptrj_t hashCode(uintptrj_t key)
      {return (key >> 2) * 2654435761u; } // 2654435761u is the golden ratio of 2^32
   TR_RatHT* findAssumptionHashTable(TR_RuntimeAssumptionKind kind) { return (kind >= 0 && kind < LastAssumptionKind) ? _tables + kind : NULL; }

   OMR::RuntimeAssumption **getBucketPtr(TR_RuntimeAssumptionKind kind, uintptrj_t hashIndex);
   OMR::RuntimeAssumption *getBucket(TR_RuntimeAssumptionKind kind, uintptrj_t key)
      {
      return *getBucketPtr(kind, hashCode(key));
      }

   void purgeRATTable(TR_FrontEnd *fe);
   void purgeRATArray(TR_FrontEnd *fe, OMR::RuntimeAssumption **array, uint32_t size);
   void purgeAssumptionListHead(OMR::RuntimeAssumption *&assumptionList, TR_FrontEnd *fe);

   void reclaimAssumptions(OMR::RuntimeAssumption **sentinel, void * metaData, bool reclaimPrePrologueAssumptions = false);
   void reclaimAssumptions(void *reclaimedMetaData, bool reclaimPrePrologueAssumptions = false);
   void notifyClassUnloadEvent(TR_FrontEnd *vm, bool isSMP,
                               TR_OpaqueClassBlock *classOwningAssumption,
                               TR_OpaqueClassBlock *picKey);
   void notifyIllegalStaticFinalFieldModificationEvent(TR_FrontEnd *vm, void *key);
   void notifyClassRedefinitionEvent(TR_FrontEnd *vm, bool isSMP, void *oldKey, void *newKey);
   void notifyMutableCallSiteChangeEvent(TR_FrontEnd *vm, uintptrj_t cookie);

   int32_t getAssumptionCount(int32_t tableId) const { return assumptionCount[tableId]; }
   int32_t getReclaimedAssumptionCount(int32_t tableId) const { return reclaimedAssumptionCount[tableId]; }
   void incReclaimedAssumptionCount(int32_t tableId) { reclaimedAssumptionCount[tableId]++; }
   void markForDetachFromRAT(OMR::RuntimeAssumption *assumption);

   void markAssumptionsAndDetach(void *reclaimedMetaData, bool reclaimPrePrologueAssumptions = false);

   /**
    * Walk the table removing entries that have been marked as detached.
    *
    * If a clean-up count is specified at most that number of entries will be
    * removed from the table - this is used to rate limit clean-up activity
    * and amortize the cost over time.
    */
   void reclaimMarkedAssumptionsFromRAT(int32_t cleanupCount = -1);

   int32_t countRatAssumptions();

   private:
   friend class OMR::RuntimeAssumption;
   void addAssumption(OMR::RuntimeAssumption *a, TR_RuntimeAssumptionKind kind, TR_FrontEnd *fe, OMR::RuntimeAssumption **sentinel);
   int32_t reclaimAssumptions(void *md, OMR::RuntimeAssumption **hashTable, OMR::RuntimeAssumption **possiblyRelevantHashTable);

   // My table of hash tables; init() allocate memory and populate it
   TR_RatHT _tables[LastAssumptionKind];
   bool     _detachPending[LastAssumptionKind]; // Marks tables that have assumptions waiting to be removed
   uint32_t _marked;                            // Counts the number of assumptions waiting to be removed
   int32_t assumptionCount[LastAssumptionKind]; // this never gets decremented
   int32_t reclaimedAssumptionCount[LastAssumptionKind];
   };

#endif // RUNTIMEASSUMPTIONTABLE_HPP
