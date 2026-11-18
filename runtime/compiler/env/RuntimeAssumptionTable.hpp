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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef RUNTIMEASSUMPTIONTABLE_HPP
#define RUNTIMEASSUMPTIONTABLE_HPP

#include <stddef.h>
#include <stdint.h>
#include "env/jittypes.h"
#include "env/PersistentAllocator.hpp"

class TR_FrontEnd;
class TR_OpaqueClassBlock;
class TR_PersistentMemory;

namespace OMR {
class RuntimeAssumption;
}

// Note !!!: routine findAssumptionHashTable() assumes that the types of
// of assumptions start at 0 and end up with LastAssumptionKind.
// No holes are allowed !
enum TR_RuntimeAssumptionKind {
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
    RuntimeAssumptionOnPatchJProfBlockFreqCounters,
    LastAssumptionKind,
    // If you add another kind, add its name to the runtimeAssumptionKindNames array
    RuntimeAssumptionSentinel // This is special as there is no hashtable associated with it and we only create one of
                              // them
};

char const * const runtimeAssumptionKindNames[LastAssumptionKind] = {
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

struct TR_RatHT {
    OMR::RuntimeAssumption **_htSpineArray;
    uint32_t *_markedforDetachCount;
    size_t _spineArraySize;
};

/// prime number
// #define ASSUMPTIONTABLE_SIZE 251
// #define CLASS_EXTEND_ASSUMPTIONTABLE_SIZE 1543  // other choices: 3079 6151
class TR_RuntimeAssumptionTable {
public:
    TR_RuntimeAssumptionTable() {} // constructor of TR_RuntimeAssumptionTable

    bool init(); // Must call this during bootstrap on a single thread because it is not MT safe

    /**
     * @brief Get the dedicated PersistentAllocator for RuntimeAssumptions
     *
     * Returns either a dedicated allocator (if TR_UseDedicatedRuntimeAssumptionAllocator is set)
     * or the global persistent allocator (default).
     *
     * @return TR::PersistentAllocator* Pointer to the RuntimeAssumption persistent allocator
     */
    static TR::PersistentAllocator *getRAPersistentAllocator() { return _raPersistentAllocator; }

    /**
     * @brief Allocate persistent memory for RuntimeAssumptions
     *
     * This method provides a centralized allocation point for all RuntimeAssumption objects,
     * making it easy to track and validate allocations. All allocations are tracked under
     * the TR_Memory::Assumption category.
     *
     * @param size Size of memory to allocate in bytes
     * @return void* Pointer to allocated memory
     */
    static void *allocateRAPersistentMemory(size_t size);

    /**
     * @brief Free persistent memory allocated for RuntimeAssumptions
     *
     * This method provides a centralized deallocation point for all RuntimeAssumption objects,
     * ensuring the correct allocator is used and enabling validation.
     *
     * @param mem Pointer to memory to free
     */
    static void freeRAPersistentMemory(void *mem);

    /**
     * @brief Get the number of segments allocated by the RuntimeAssumption allocator
     *
     * This method returns the number of memory segments that have been allocated
     * by the dedicated RuntimeAssumption persistent allocator. Useful for memory
     * usage statistics and debugging.
     *
     * @return int Number of segments allocated
     */
    static int getRANumSegments() { return _raPersistentAllocator ? _raPersistentAllocator->getNumSegments() : 0; }

    static uintptr_t hashCode(uintptr_t key)
    {
        return (key >> 2) * 2654435761u;
    } // 2654435761u is the golden ratio of 2^32

    TR_RatHT *findAssumptionHashTable(TR_RuntimeAssumptionKind kind)
    {
        return (kind >= 0 && kind < LastAssumptionKind) ? _tables + kind : NULL;
    }

    OMR::RuntimeAssumption **getBucketPtr(TR_RuntimeAssumptionKind kind, uintptr_t hashIndex);

    OMR::RuntimeAssumption *getBucket(TR_RuntimeAssumptionKind kind, uintptr_t key)
    {
        return *getBucketPtr(kind, hashCode(key));
    }

    void purgeRATTable(TR_FrontEnd *fe);
    void purgeRATArray(TR_FrontEnd *fe, OMR::RuntimeAssumption **array, uint32_t size);
    void purgeAssumptionListHead(OMR::RuntimeAssumption *&assumptionList, TR_FrontEnd *fe);

    void reclaimAssumptions(OMR::RuntimeAssumption **sentinel, void *metaData,
        bool reclaimPrePrologueAssumptions = false);
    void reclaimAssumptions(void *reclaimedMetaData, bool reclaimPrePrologueAssumptions = false);
    void notifyClassUnloadEvent(TR_FrontEnd *vm, bool isSMP, TR_OpaqueClassBlock *classOwningAssumption,
        TR_OpaqueClassBlock *picKey);
    void notifyIllegalStaticFinalFieldModificationEvent(TR_FrontEnd *vm, void *key);
    void notifyClassRedefinitionEvent(TR_FrontEnd *vm, bool isSMP, void *oldKey, void *newKey);
    void notifyMutableCallSiteChangeEvent(TR_FrontEnd *vm, uintptr_t cookie);
    void notifyMethodBreakpointed(TR_FrontEnd *fe, TR_OpaqueMethodBlock *method);

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
    void addAssumption(OMR::RuntimeAssumption *a, TR_RuntimeAssumptionKind kind, TR_FrontEnd *fe,
        OMR::RuntimeAssumption **sentinel);
    int32_t reclaimAssumptions(void *md, OMR::RuntimeAssumption **hashTable,
        OMR::RuntimeAssumption **possiblyRelevantHashTable);

    // My table of hash tables; init() allocate memory and populate it
    TR_RatHT _tables[LastAssumptionKind];
    bool _detachPending[LastAssumptionKind]; // Marks tables that have assumptions waiting to be removed
    uint32_t _marked; // Counts the number of assumptions waiting to be removed
    int32_t assumptionCount[LastAssumptionKind]; // this never gets decremented
    int32_t reclaimedAssumptionCount[LastAssumptionKind];

    /**
     * @brief Static persistent allocator for RuntimeAssumptions
     *
     * This can point to either:
     * - A dedicated PersistentAllocator instance (when TR_UseDedicatedRuntimeAssumptionAllocator env var is set)
     * - The global persistent allocator from TR::Compiler->persistentMemory() (default behavior)
     *
     * Using a direct PersistentAllocator reference (instead of TR_PersistentMemory) avoids
     * creating a separate PersistentInfo object, which would cause confusion since
     * RuntimeAssumption code needs to access the global PersistentInfo for the RuntimeAssumptionTable.
     *
     * Initialized in init() during JVM bootstrap.
     */
    static TR::PersistentAllocator *_raPersistentAllocator;
};

#endif // RUNTIMEASSUMPTIONTABLE_HPP
