
/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#if !defined(J9MODRON_H__)
#define J9MODRON_H__

/* @ddr_namespace: map_to_type=J9ModronConstants */

#include "j9.h"
#include "j9consts.h"
#include "omrgcconsts.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Write barrier type definitions.
 * @ingroup GC_Include
 * @{
 */
typedef enum {
	j9gc_modron_wrtbar_illegal = gc_modron_wrtbar_illegal,
	j9gc_modron_wrtbar_none = gc_modron_wrtbar_none,
	j9gc_modron_wrtbar_always = gc_modron_wrtbar_always,
	j9gc_modron_wrtbar_oldcheck = gc_modron_wrtbar_oldcheck,
	j9gc_modron_wrtbar_cardmark = gc_modron_wrtbar_cardmark,
	j9gc_modron_wrtbar_cardmark_incremental = gc_modron_wrtbar_cardmark_incremental,
	j9gc_modron_wrtbar_cardmark_and_oldcheck = gc_modron_wrtbar_cardmark_and_oldcheck,
	j9gc_modron_wrtbar_realtime = gc_modron_wrtbar_realtime,
	j9gc_modron_wrtbar_count = gc_modron_wrtbar_count
} J9WriteBarrierType;

/**
 * Read barrier type definitions.
 * @ingroup GC_Include
 * @{
 */
typedef enum {
	j9gc_modron_readbar_illegal = gc_modron_readbar_illegal,
	j9gc_modron_readbar_none = gc_modron_readbar_none,
	j9gc_modron_readbar_evacuate = gc_modron_readbar_evacuate,
	j9gc_modron_readbar_always = gc_modron_readbar_always,
	j9gc_modron_readbar_count = gc_modron_readbar_count
} J9ReadBarrierType;

typedef enum {
	j9gc_modron_allocation_type_illegal = OMR_GC_ALLOCATION_TYPE_ILLEGAL,
	j9gc_modron_allocation_type_tlh = OMR_GC_ALLOCATION_TYPE_TLH,
	j9gc_modron_allocation_type_segregated = OMR_GC_ALLOCATION_TYPE_SEGREGATED,
	j9gc_modron_allocation_type_count /* Total number of write barriers */
} J9GCAllocationType;

/**
 * GC Feature type definitions (used by "j9gc_modron_isFeatureSupported")
 */
typedef enum {
	j9gc_modron_feature_none = 0,
	j9gc_modron_feature_inline_reference_get,
	j9gc_modron_feature_count /* Total number of known features */
} J9GCFeatureType;

/**
 * GC Configuration type definitions (used by "j9gc_modron_getConfigurationValueForKey")
 */
typedef enum {
	j9gc_modron_configuration_none = 0,
	j9gc_modron_configuration_heapAddressToCardAddressShift,	/* a UDATA representing the shift amount to convert from heap granularity to card table granularity */
	j9gc_modron_configuration_heapBaseForBarrierRange0_isVariable,	/* a UDATA (TRUE or FALSE) representing whether or not the J9VMThread->heapBaseForBarrierRange0 can change during the run (FALSE implies constant) */
	j9gc_modron_configuration_activeCardTableBase_isVariable,	/* a UDATA (TRUE or FALSE) representing whether or not the J9VMThread->activeCardTableBase can change during the run (FALSE implies constant) */
	j9gc_modron_configuration_heapSizeForBarrierRange0_isVariable,	/* a UDATA (TRUE or FALSE) representing whether or not the J9VMThread->heapSizeForBarrierRange0 can change during the run (FALSE implies constant) */
	j9gc_modron_configuration_minimumObjectSize, /* a UDATA representing the minimum object size */
	j9gc_modron_configuration_allocationType, /* a UDATA representing the allocation type see J9GCAllocationType enum for possible types */
	j9gc_modron_configuration_discontiguousArraylets,  /* a UDATA (TRUE or FALSE) representing whether or not discontiguousArraylets are enabled */
	j9gc_modron_configuration_gcThreadCount,  /* a UDATA representing the MAX number of GC threads being used */
	j9gc_modron_configuration_objectAlignment, /* a UDATA representing the alignment of the object in heap */
	/* Add new values before this comment */
	j9gc_modron_configuration_count /* Total number of known configuration keys */
} J9GCConfigurationKey;


/**
 * Card state definitions
 * @note CARD_DIRTY 0x01 has been hardcoded in builder see J9VMObjectAccessBarrier>>#dirtyCard
 * @ingroup GC_Include
 * @{
 */

/* means that this card requires no special handling in the next collection as a result of previous collection or mutator activity */
#define CARD_CLEAN 0x00
/* means that this card has been changed by the mutator since the collector last processed it */
#define CARD_DIRTY 0x01
/* means that a PGC has viewed the card but the next GMP increment (or SYS/OOM fall-back) must also process the card (since it could have hidden objects) */
#define CARD_GMP_MUST_SCAN 0x02
/* means that a GMP has viewed the card but the next PGC increment must also process the card (since it might have unseen inter-region references - the GMP has a smaller live set so it wouldn't see them nor does it update the RSLists) */
#define CARD_PGC_MUST_SCAN 0x03
/* largely equivalent to CARD_PGC_MUST_SCAN but can only exist between RSCL flushing and the end of PGC mark and has the special connotation that the only objects, in this card, which refer to other regions have their remembered bit set */
#define CARD_REMEMBERED 0x04
/* largely equivalent to CARD_DIRTY but can only exist between RSCL flushing and the end of PGC mark and has the special connotation that the only objects, in this card, which refer to other regions have their remembered bit set */
#define CARD_REMEMBERED_AND_GMP_SCAN 0x05
/* used to state that this card contains objects which refer to other regions but all of those who do have their remembered bit set and are not yet in the region's RSCL - the compact treats this much like PGC_MUST_SCAN in that it may promote it to GMP_MUST_SCAN if a GMP is active */
#define CARD_MARK_COMPACT_TRANSITION 0x06
/* used when a value must be passed which is considered uninitialized or otherwise invalid */
#define CARD_INVALID 0xFF
	
/**
 * Signature for the callback function passed to <code>MM_ReferenceChainWalker</code>
 */
typedef jvmtiIterationControl J9MODRON_REFERENCE_CHAIN_WALKER_CALLBACK(J9Object **slotPtr, J9Object *sourcePtr, void *userData, IDATA type, IDATA index, IDATA wasReportedBefore);

/**
 * @}
 */

#define J9GC_ROOT_TYPE_UNKNOWN 1 /**< root that fell through a default state in an iterator, or called via non abstracted doSlot() */
#define J9GC_ROOT_TYPE_CLASS 2
#define J9GC_ROOT_TYPE_JNI_LOCAL 3
#define J9GC_ROOT_TYPE_JNI_GLOBAL 4
#define J9GC_ROOT_TYPE_JNI_WEAK_GLOBAL 5
#define J9GC_ROOT_TYPE_THREAD_SLOT 6
#define J9GC_ROOT_TYPE_MONITOR 7
#define J9GC_ROOT_TYPE_STRING_TABLE 8
#define J9GC_ROOT_TYPE_DEBUG_REFERENCE 9
#define J9GC_ROOT_TYPE_DEBUG_CLASS_REFERENCE 10
#define J9GC_ROOT_TYPE_FINALIZABLE_OBJECT 11
#define J9GC_ROOT_TYPE_UNFINALIZED_OBJECT 12
#define J9GC_ROOT_TYPE_WEAK_REFERENCE 13
#define J9GC_ROOT_TYPE_SOFT_REFERENCE 14
#define J9GC_ROOT_TYPE_PHANTOM_REFERENCE 15
#define J9GC_ROOT_TYPE_THREAD_MONITOR 16
#define J9GC_ROOT_TYPE_REMEMBERED_SET 17
#define J9GC_ROOT_TYPE_CLASSLOADER 18
#define J9GC_ROOT_TYPE_VM_CLASS_SLOT 19
#define J9GC_ROOT_TYPE_STACK_SLOT 20
#define J9GC_ROOT_TYPE_JVMTI_TAG_REF 21
#define J9GC_ROOT_TYPE_OWNABLE_SYNCHRONIZER_OBJECT 22

#define J9GC_REFERENCE_TYPE_UNKNOWN -1 /**< reference to an object that fell through a default state in an iterator */
#define J9GC_REFERENCE_TYPE_FIELD -2	/**< field reference to an object */
#define J9GC_REFERENCE_TYPE_STATIC -3	/**< static field reference to an object */
#define J9GC_REFERENCE_TYPE_CLASS -4	/**< reference to an object's class */
#define J9GC_REFERENCE_TYPE_ARRAY -5	/**< reference to an object from an array */
#define J9GC_REFERENCE_TYPE_WEAK_REFERENCE -6 /**< field reference to an object from a weak reference */
#define J9GC_REFERENCE_TYPE_CONSTANT_POOL -7 /**< reference to an object in a constant pool */
#define J9GC_REFERENCE_TYPE_PROTECTION_DOMAIN -8 /**< reference to a class' protection domain */
#define J9GC_REFERENCE_TYPE_SUPERCLASS -9 /**< reference to a class' superclass */
#define J9GC_REFERENCE_TYPE_INTERFACE -10 /**< reference to an interface implemented by a class */
#define J9GC_REFERENCE_TYPE_CLASSLOADER -11 /**< reference to the classloader of the class */
#define J9GC_REFERENCE_TYPE_CLASS_ARRAY_CLASS -12 /**< reference to an array class' component type class */
#define J9GC_REFERENCE_TYPE_CLASS_NAME_STRING -13 /**< reference to a class' name string */
#define J9GC_REFERENCE_TYPE_CALL_SITE -14 /**< call site reference to an object */

/**
 * #defines representing the vmState that should be set during various GC activities
 * @note J9VMSTATE_GC is the "major mask" representing a "GC" activity - we OR in a minor mask
 * representing the specific activity.
 * @ingroup GC_Include
 * @{
 */
#define J9VMSTATE_GC_GLOBALGC (J9VMSTATE_GC | 0x0001)
#define J9VMSTATE_GC_MARK (J9VMSTATE_GC | 0x0002)
#define J9VMSTATE_GC_SWEEP (J9VMSTATE_GC | 0x0003)
#define J9VMSTATE_GC_COMPACT (J9VMSTATE_GC | 0x0004)
#define J9VMSTATE_GC_CONCURRENT_MARK_PREPARE_CARD_TABLE (J9VMSTATE_GC | 0x0006)
#define J9VMSTATE_GC_COMPACT_FIX_HEAP_FOR_WALK (J9VMSTATE_GC | 0x0007)
#define J9VMSTATE_GC_CONCURRENT_MARK_CLEAR_NEW_MARKBITS (J9VMSTATE_GC | 0x0008)
#define J9VMSTATE_GC_CONCURRENT_MARK_SCAN_REMEMBERED_SET (J9VMSTATE_GC | 0x0009)
#define J9VMSTATE_GC_CONCURRENT_MARK_FINAL_CLEAN_CARDS (J9VMSTATE_GC | 0x000A)
#define J9VMSTATE_GC_CONCURRENT_SWEEP_COMPLETE_SWEEP (J9VMSTATE_GC | 0x000B)
#define J9VMSTATE_GC_CONCURRENT_SWEEP_FIND_MINIMUM_SIZE_FREE (J9VMSTATE_GC | 0x000C)
#define J9VMSTATE_GC_PARALLEL_OBJECT_DO (J9VMSTATE_GC | 0x000D)
#define J9VMSTATE_GC_PARALLEL_VMSLOTS_DO (J9VMSTATE_GC | 0x000E)
#define J9VMSTATE_GC_SCAVENGE (J9VMSTATE_GC | 0x000F)
#define J9VMSTATE_GC_SCRUB_CARD_TABLE (J9VMSTATE_GC | 0x0010)
#define J9VMSTATE_GC_UNLOADING_DEAD_CLASSLOADERS (J9VMSTATE_GC | 0x0011)
#define J9VMSTATE_GC_COLLECTOR_CONCURRENTGC (J9VMSTATE_GC | 0x0012)
#define J9VMSTATE_GC_COLLECTOR_CONCURRENTSWEEPGC (J9VMSTATE_GC | 0x0013)
#define J9VMSTATE_GC_COLLECTOR_GLOBALGC (J9VMSTATE_GC | 0x0014)
#define J9VMSTATE_GC_COLLECTOR_SCAVENGER (J9VMSTATE_GC | 0x0015)
#define J9VMSTATE_GC_CONCURRENT_MARK_TRACE (J9VMSTATE_GC | 0x0016)
#define J9VMSTATE_GC_CONCURRENT_SWEEP (J9VMSTATE_GC | 0x0017)
#define J9VMSTATE_GC_COLLECTOR_METRONOME (J9VMSTATE_GC | 0x0018)
#define J9VMSTATE_GC_ALLOCATE_OBJECT (J9VMSTATE_GC | 0x0019)
#define J9VMSTATE_GC_ALLOCATE_INDEXABLE_OBJECT (J9VMSTATE_GC | 0x001A)
#define J9VMSTATE_GC_TLH_ALLOCATE (J9VMSTATE_GC | 0x001B)
#define J9VMSTATE_GC_TLH_FLUSH (J9VMSTATE_GC | 0x001C)
#define J9VMSTATE_GC_TLH_RESET (J9VMSTATE_GC | 0x001D)
#define J9VMSTATE_GC_CONCURRENT_MARK_COMPLETE_TRACING (J9VMSTATE_GC | 0x001E)
#define J9VMSTATE_GC_CHECK_RESIZE (J9VMSTATE_GC | 0x0020)
#define J9VMSTATE_GC_PERFORM_RESIZE (J9VMSTATE_GC | 0x0021)
#define J9VMSTATE_GC_CHECK_BEFORE_GC (J9VMSTATE_GC | 0x0022)
#define J9VMSTATE_GC_CHECK_AFTER_GC (J9VMSTATE_GC | 0x0023)
#define J9VMSTATE_GC_TGC (J9VMSTATE_GC | 0x0024)
#define J9VMSTATE_GC_DISPATCHER_IDLE (J9VMSTATE_GC | 0x0025)
#define J9VMSTATE_GC_CONCURRENT_SCAVENGER (J9VMSTATE_GC | 0x0026)

#define J9VMSTATE_GC_COPY_FORWARD_GMP_CARD_CLEANER (J9VMSTATE_GC | 0x0102)
#define J9VMSTATE_GC_COPY_FORWARD_NO_GMP_CARD_CLEANER (J9VMSTATE_GC | 0x0103)
#define J9VMSTATE_GC_GLOBAL_COLLECTION_CARD_CLEANER (J9VMSTATE_GC | 0x0104)
#define J9VMSTATE_GC_GLOBAL_COLLECTION_NO_SCAN_CARD_CLEANER (J9VMSTATE_GC | 0x0105)
#define J9VMSTATE_GC_GLOBAL_MARK_CARD_CLEANER (J9VMSTATE_GC | 0x0106)
#define J9VMSTATE_GC_GLOBAL_MARK_CARD_SCRUBBER (J9VMSTATE_GC | 0x0107)
#define J9VMSTATE_GC_GLOBAL_MARK_NO_SCAN_CARD_CLEANER (J9VMSTATE_GC | 0x0108)
#define J9VMSTATE_GC_PARTIAL_MARK_GMP_CARD_CLEANER (J9VMSTATE_GC | 0x0109)
#define J9VMSTATE_GC_PARTIAL_MARK_NO_GMP_CARD_CLEANER (J9VMSTATE_GC | 0x010A)
#define J9VMSTATE_GC_WRITE_ONCE_FIXUP_CARD_CLEANER (J9VMSTATE_GC | 0x010B)
/**
 * @}
 */

/**
 * #defines representing the results from j9gc_ext_check_is_valid_heap_object.
 * @ingroup GC_Include
 * @{
 */
#define J9OBJECTCHECK_OBJECT 0
#define J9OBJECTCHECK_CLASS 1
#define J9OBJECTCHECK_FORWARDED 2
#define J9OBJECTCHECK_INVALID 3
/**
 * @}
 */

/**
 * #defines representing tags used in the Remembered Set
 * @ingroup GC_Include
 * @{
 */
#define DEFERRED_RS_REMOVE_FLAG 			0x1	
/**
 * @}
 */


/**
 * @ingroup GC_Include
 * @name Class loader flags
 * Borrowed from builder - should be made collector specific 
 * @{
 */
/* TODO Should these be deleted? or changed? */
#define J9_GC_CLASS_LOADER_SCANNED 0x1
#define J9_GC_CLASS_LOADER_DEAD 0x2
#define J9_GC_CLASS_LOADER_UNLOADING 0x4
#define J9_GC_CLASS_LOADER_ENQ_UNLOAD 0x8
#define J9_GC_CLASS_LOADER_REMEMBERED 0x10

/** @} */

/**
 * @ingroup GC_Include
 * @name Metronome CPU utilization component type flags.
 * Any JVM component that hooks to GC events and consumes significant CPU cycles
 * should be assigned a new component type so that GC can keep track of its utilization.
 * @{
 */
#define J9_GC_METRONOME_UTILIZATION_COMPONENT_MUTATOR 0
#define J9_GC_METRONOME_UTILIZATION_COMPONENT_GC 1
#define J9_GC_METRONOME_UTILIZATION_COMPONENT_JIT 2
/** @} */


#define J9GC_HASH_SALT_COUNT_STANDARD 1
#define J9GC_HASH_SALT_NURSERY_INDEX 0

#define J9_GC_MANAGEMENT_POOL_JAVAHEAP	 				0x1
#define J9_GC_MANAGEMENT_POOL_TENURED 				0x2
#define J9_GC_MANAGEMENT_POOL_TENURED_SOA 			0x4
#define J9_GC_MANAGEMENT_POOL_TENURED_LOA 			0x8
#define J9_GC_MANAGEMENT_POOL_NURSERY_ALLOCATE		0x10
#define J9_GC_MANAGEMENT_POOL_NURSERY_SURVIVOR		0x20
#define J9_GC_MANAGEMENT_POOL_REGION_OLD 				0x40
#define J9_GC_MANAGEMENT_POOL_REGION_EDEN 				0x80
#define J9_GC_MANAGEMENT_POOL_REGION_SURVIVOR 			0x100
#define J9_GC_MANAGEMENT_POOL_REGION_RESERVED 			0x200
#define J9_GC_MANAGEMENT_MAX_POOL						10

#define J9_GC_MANAGEMENT_COLLECTOR_SCAVENGE 			0x1
#define J9_GC_MANAGEMENT_COLLECTOR_GLOBAL 				0x2
#define J9_GC_MANAGEMENT_COLLECTOR_PGC 					0x4
#define J9_GC_MANAGEMENT_COLLECTOR_GGC 					0x8
#define J9_GC_MANAGEMENT_MAX_COLLECTOR 					4

#ifdef __cplusplus
} /* extern "C" { */
#endif /* __cplusplus */

#endif /* J9MODRON_H__ */

