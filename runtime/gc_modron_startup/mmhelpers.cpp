
/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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

 /**
 * @file
 * @ingroup GC_Modron_Startup
 */

#include <string.h>

#include "j9.h"
#include "j9cfg.h"
#include "j9comp.h"
#include "j9consts.h"
#include "j9modron.h"
#include "ModronAssertions.h"
#include "omr.h"
#include "omrcfg.h"
#include "VerboseGCInterface.h"

#if defined(J9VM_GC_FINALIZATION)
#include "FinalizeListManager.hpp"
#endif /* J9VM_GC_FINALIZATION */
#include "GCExtensions.hpp"
#include "GlobalCollector.hpp"
#include "Heap.hpp"
#include "HeapRegionManager.hpp"
#if defined(OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD)
#include "HeapRegionStateTable.hpp"
#endif /* defined(OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD) */
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"

extern "C" {
extern J9MemoryManagerFunctions MemoryManagerFunctions;
extern void initializeVerboseFunctionTableWithDummies(J9MemoryManagerVerboseInterface *table);

/**
 * sets the mode where TLH pages are zeroed
 * @param flag if non zero, TLH page zeroing will be enabled
 */
#if defined(J9VM_GC_BATCH_CLEAR_TLH)
void
allocateZeroedTLHPages(J9JavaVM *javaVM, UDATA flag)
{
	MM_GCExtensions::getExtensions(javaVM)->batchClearTLH = (flag != 0) ? 1 : 0;
}

/**
 * checks if zeroing TLH pages is enabled
 */
UDATA
isAllocateZeroedTLHPagesEnabled(J9JavaVM *javaVM)
{
	return MM_GCExtensions::getExtensions(javaVM)->batchClearTLH;
}
#endif /* J9VM_GC_BATCH_CLEAR_TLH */

UDATA
isStaticObjectAllocateFlags(J9JavaVM *javaVM)
{
	return 1;
}

/**
 * Depending on the configuration (scav or not) returns the object allocate flags
 * @return OBJECT_HEADER_OLD or 0
 */
UDATA
getStaticObjectAllocateFlags(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	UDATA result = extensions->heap->getDefaultMemorySpace()->getDefaultMemorySubSpace()->getObjectFlags();

	/* No static flags supposed to be set for flags in clazz slot */
	Assert_MM_true(0 == result);

	return result;
}

/**
 * Query JIT string de-duplication strategy
 */
I_32
j9gc_get_jit_string_dedup_policy(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	if (MM_GCExtensions::J9_JIT_STRING_DEDUP_POLICY_UNDEFINED == extensions->stringDedupPolicy) {
		MM_GCExtensions::JitStringDeDupPolicy result = MM_GCExtensions::J9_JIT_STRING_DEDUP_POLICY_DISABLED;
		if (extensions->isStandardGC()) {
			result = MM_GCExtensions::J9_JIT_STRING_DEDUP_POLICY_FAVOUR_LOWER;
#if defined(J9VM_GC_MODRON_SCAVENGER)
			if (extensions->scavengerEnabled) {
				if (!extensions->dynamicNewSpaceSizing) {
					result = MM_GCExtensions::J9_JIT_STRING_DEDUP_POLICY_FAVOUR_HIGHER;
				}
			}
#endif /* J9VM_GC_MODRON_SCAVENGER */
		}
		return (I_32) result;
	} else {
		return (I_32) extensions->stringDedupPolicy;
	}
}

/**
 * Query if scavenger is enabled
 * @return 1 if scavenger enabled, 0 otherwise
 */
UDATA
j9gc_scavenger_enabled(J9JavaVM *javaVM)
{
#if defined(J9VM_GC_MODRON_SCAVENGER)
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	return extensions->scavengerEnabled ? 1 : 0;
#else /* J9VM_GC_MODRON_SCAVENGER */
	return 0;
#endif /* J9VM_GC_MODRON_SCAVENGER */
}

UDATA
j9gc_concurrent_scavenger_enabled(J9JavaVM *javaVM)
{
	return MM_GCExtensions::getExtensions(javaVM)->isConcurrentScavengerEnabled() ? 1 : 0;
}

UDATA
j9gc_software_read_barrier_enabled(J9JavaVM *javaVM)
{
	return MM_GCExtensions::getExtensions(javaVM)->isSoftwareRangeCheckReadBarrierEnabled() ? 1 : 0;
}

/**
 * Query if hot reference field is reqired for scavenger dynamicBreadthFirstScanOrdering
 *  @return true if scavenger dynamicBreadthFirstScanOrdering is enabled, 0 otherwise 
 */
BOOLEAN
j9gc_hot_reference_field_required(J9JavaVM *javaVM)
{
#if defined(J9VM_GC_MODRON_SCAVENGER) || defined(OMR_GC_VLHGC)
	return MM_GCExtensions::OMR_GC_SCAVENGER_SCANORDERING_DYNAMIC_BREADTH_FIRST == MM_GCExtensions::getExtensions(javaVM)->scavengerScanOrdering;
#else /* J9VM_GC_MODRON_SCAVENGER || OMR_GC_VLHGC */
	return FALSE;
#endif /* defined(J9VM_GC_MODRON_SCAVENGER) || defined(OMR_GC_VLHGC) */
}

/**
 * Query if off heap allocation for large objects is enabled.
 *
 * @param javaVM pointer to J9JavaVM
 * @return true if extensions flag isVirtualLargeObjectHeapEnabled is set, 0 otherwise
 */
BOOLEAN
j9gc_off_heap_allocation_enabled(J9JavaVM *javaVM)
{
<<<<<<< Upstream, based on Upstream/master
#if defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION)
	return MM_GCExtensions::getExtensions(javaVM)->isVirtualLargeObjectHeapEnabled;
#else /* defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION) */
	return FALSE;
#endif /* defined(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION) */
=======
#if defined(J9VM_ENV_DATA64)
	return MM_GCExtensions::getExtensions(javaVM)->isVirtualLargeObjectHeapEnabled;
#else /* defined(J9VM_ENV_DATA64) */
	return FALSE;
#endif /* defined(J9VM_ENV_DATA64) */
>>>>>>> 2736c22 Introduce Off-Heap Technology for Large Arrays
}

/**
 * Query for the max hot field list length that a class is allowed to have.
 * Valid if dynamicBreadthFirstScanOrdering is enabled.
 */
uint32_t
j9gc_max_hot_field_list_length(J9JavaVM *javaVM)
{
	return MM_GCExtensions::getExtensions(javaVM)->maxHotFieldListLength;
}

/**
 * Depending on the configuration (scav vs concurr etc) returns the type of the write barrier
 * @return write barrier type
 */
UDATA
j9gc_modron_getWriteBarrierType(J9JavaVM *javaVM)
{
	Assert_MM_true(j9gc_modron_wrtbar_illegal != javaVM->gcWriteBarrierType);
	return javaVM->gcWriteBarrierType;
}

/**
 * Depending on the configuration returns the type of the read barrier
 * @return read barrier type
 */
UDATA
j9gc_modron_getReadBarrierType(J9JavaVM *javaVM)
{
	Assert_MM_true(j9gc_modron_readbar_illegal != javaVM->gcReadBarrierType);
	return javaVM->gcReadBarrierType;
}

/**
 * Is the JIT allowed to inline allocations?
 * @return non-zero if inline allocation is allowed, 0 otherwise
 */
UDATA
j9gc_jit_isInlineAllocationSupported(J9JavaVM *javaVM)
{
	return 1;
}

#if defined(J9VM_GC_FINALIZATION)
/**
 * Return the number of objects currently on the finalize queue.
 * This is to support a Java 5.0 API that allows java code to query this number.
 * @return number of objects pending finalization
 */
UDATA
j9gc_get_objects_pending_finalization_count(J9JavaVM *javaVM)
{
	return MM_GCExtensions::getExtensions(javaVM)->finalizeListManager->getJobCount();
}
#endif /* J9VM_GC_FINALIZATION */

UDATA
j9gc_ext_is_marked(J9JavaVM *javaVM, J9Object *objectPtr)
{
	return MM_GCExtensions::getExtensions(javaVM)->getGlobalCollector()->isMarked(objectPtr);
}

UDATA
j9gc_modron_isFeatureSupported(J9JavaVM *javaVM, UDATA feature)
{
	J9GCFeatureType typedFeature = (J9GCFeatureType) feature;
	UDATA featureSupported = FALSE;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);

	if (j9gc_modron_feature_inline_reference_get == typedFeature) {
		if (extensions->isMetronomeGC()) {
			featureSupported = FALSE;
		} else {
			featureSupported = TRUE;
		}
	}
	return featureSupported;
}

/**
 * Used for checking the value of some piece of externally visible information (instead of adding new API to request every kind of
 * information when each would have roughly the same shape).  The key parameter is of type "J9GCConfigurationKey" (presented as
 * UDATA due to Builder compatibility fears).  If the given key is found, the resulting value is stored into "value" as whatever
 * type the key defines and TRUE is returned.  FALSE is returned and value is untouched if the key is invalid for this configuration
 * and an unreachable assertion is thrown if the key is unknown.
 * @param javaVM[in] The JavaVM
 * @param key[in] An enumerated constant representing the configuration key to query
 * @param value[out] The value the configuration holds for the given key (actual pointer type is key-defined)
 * @return TRUE if the key was found and value was populated, FALSE if the key is invalid for this configuration and an unreachable
 * assertion is thrown if the key is unknown
 */
UDATA
j9gc_modron_getConfigurationValueForKey(J9JavaVM *javaVM, UDATA key, void *value)
{
	UDATA keyFound = FALSE;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);

	switch (key) {
	case j9gc_modron_configuration_none:
		/* this is the "safe" invalid value - just return false */
		keyFound = FALSE;
		break;
	case j9gc_modron_configuration_heapAddressToCardAddressShift:
#if defined (J9VM_GC_HEAP_CARD_TABLE)
		if (NULL != extensions->cardTable) {
			*((UDATA *)value) = CARD_SIZE_SHIFT;
			keyFound = TRUE;
		} else {
			/* this is an invalid (but understood) call if there is no card table */
			keyFound = FALSE;
		}
#else /* defined (J9VM_GC_HEAP_CARD_TABLE) */
		keyFound = FALSE;
#endif /* defined (J9VM_GC_HEAP_CARD_TABLE) */
		break;
	case j9gc_modron_configuration_heapBaseForBarrierRange0_isVariable:
		if (extensions->isVLHGC()) {
			*((UDATA *)value) = FALSE;
			keyFound = TRUE;
		} else if (extensions->isStandardGC()) {
			*((UDATA *)value) = FALSE;
			keyFound = TRUE;
		} else {
			keyFound = FALSE;
		}
		break;
	case j9gc_modron_configuration_activeCardTableBase_isVariable:
		if (extensions->isVLHGC()) {
			*((UDATA *)value) = FALSE;
			keyFound = TRUE;
		} else if (extensions->isStandardGC()) {
			*((UDATA *)value) = FALSE;
			keyFound = TRUE;
		} else {
			keyFound = FALSE;
		}
		break;
	case j9gc_modron_configuration_heapSizeForBarrierRange0_isVariable:
		if (extensions->isVLHGC()) {
			*((UDATA *)value) = FALSE;
			keyFound = TRUE;
		} else if (extensions->isStandardGC()) {
			if (extensions->minOldSpaceSize == extensions->maxOldSpaceSize) {
				*((UDATA *)value) = FALSE;
			} else {
				*((UDATA *)value) = TRUE;
			}
			keyFound = TRUE;
		} else {
			keyFound = FALSE;
		}
		break;
	case j9gc_modron_configuration_minimumObjectSize:
#if defined(J9VM_GC_MINIMUM_OBJECT_SIZE)
		*((UDATA *)value) = J9_GC_MINIMUM_OBJECT_SIZE;
		keyFound = TRUE;
#else
		keyFound = FALSE;
#endif /* defined(J9VM_GC_MINIMUM_OBJECT_SIZE) */
		break;
	case j9gc_modron_configuration_objectAlignment:
		*((UDATA *)value) = extensions->getObjectAlignmentInBytes();
		keyFound = TRUE;
		break;
	case j9gc_modron_configuration_allocationType:
		Assert_MM_true(j9gc_modron_allocation_type_illegal != javaVM->gcAllocationType);
		*((UDATA *)value) = javaVM->gcAllocationType;
		keyFound = TRUE;
		break;
	case j9gc_modron_configuration_discontiguousArraylets:
		*((UDATA *)value) = (UDATA_MAX != extensions->getOmrVM()->_arrayletLeafSize) ? TRUE : FALSE;
		keyFound = TRUE;
		break;
	case j9gc_modron_configuration_gcThreadCount:
		*((UDATA *)value) = extensions->gcThreadCount;
		keyFound = TRUE;
		break;
	case j9gc_modron_configuration_compressObjectReferences:
		*((UDATA *)value) = extensions->compressObjectReferences();
		keyFound = TRUE;
		break;
	case j9gc_modron_configuration_heapRegionShift:
		if (extensions->isVLHGC()) {
			*((UDATA *)value) = extensions->heapRegionManager->getRegionShift();
			keyFound = TRUE;
		} else {
			*((UDATA *)value) = 0;
			keyFound = FALSE;
		}
		break;
	case j9gc_modron_configuration_heapRegionStateTable:
#if defined(OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD)
		if (extensions->isConcurrentCopyForwardEnabled()) {
			*((UDATA *)value) = (UDATA) extensions->heapRegionStateTable->getTable();
			keyFound = TRUE;
		} else {
			*((UDATA *)value) = 0;
			keyFound = FALSE;
		}
#else /* defined(OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD) */
		*((UDATA *)value) = 0;
		keyFound = FALSE;
#endif /* defined(OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD) */

		break;
	default:
		/* key is either invalid or unknown for this configuration - should not have been requested */
		Assert_MM_unreachable();
	}
	return keyFound;
}

} /* extern "C" */
