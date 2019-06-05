/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#include "j9.h"
#include "j9cfg.h"
#include "j9port.h"
#include "modron.h"
#include "HeapIteratorAPI.h"
#include "ModronAssertions.h"

#include "ArrayletLeafIterator.hpp"
#include "GCExtensionsBase.hpp"
#include "HeapIteratorAPIRootIterator.hpp"
#include "HeapIteratorAPIBufferedIterator.hpp"
#include "HeapRegionDescriptor.hpp"
#if defined(OMR_GC_SEGREGATED_HEAP)
#include "HeapRegionDescriptorSegregated.hpp"
#endif /* defined(OMR_GC_SEGREGATED_HEAP) */
#include "HeapRegionIterator.hpp"
#include "HeapRegionManager.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "MixedObjectIterator.hpp"
#include "ObjectAccessBarrier.hpp"
#include "OwnableSynchronizerObjectList.hpp"
#include "PointerArrayIterator.hpp"
#include "SlotObject.hpp"
#include "VMInterface.hpp"

static void 
initializeRegionDescriptor(
		J9MM_IterateRegionDescriptor *descriptor, 
		const char* name, 
		UDATA id, 
		UDATA objectAlignment, 
		UDATA objectMinimumSize, 
		void* start, 
		UDATA size);

static void 
initializeRegionDescriptor(
		MM_GCExtensionsBase* extensions,
		J9MM_IterateRegionDescriptor *descriptor, 
		MM_HeapRegionDescriptor *region);
static void
initializeObjectDescriptor(
		J9JavaVM *javaVM, 
		J9MM_IterateObjectDescriptor *descriptor, 
		J9MM_IterateRegionDescriptor *regionDesc, 
		j9object_t object);

static jvmtiIterationControl
iterateRegions(
		J9JavaVM* vm,
		J9MM_IterateSpaceDescriptor *space,
		UDATA flags,
		jvmtiIterationControl (*func)(J9JavaVM* vm, J9MM_IterateRegionDescriptor *regionDesc, void *userData),
		void *userData);

static jvmtiIterationControl
iterateRegionObjects(
	J9JavaVM *vm,
	J9MM_IterateRegionDescriptor *region,
	UDATA flags,
	jvmtiIterationControl (*func)(J9JavaVM *vm, J9MM_IterateObjectDescriptor *objectDesc, void *userData),
	void *userData);

extern "C" {

/* used by j9mm_iterate_all_objects */
static jvmtiIterationControl internalIterateHeaps(J9JavaVM *vm, J9MM_IterateHeapDescriptor *heap, void *userData);
static jvmtiIterationControl internalIterateSpaces(J9JavaVM *vm, J9MM_IterateSpaceDescriptor *space, void *userData);
static jvmtiIterationControl internalIterateRegions(J9JavaVM *vm, J9MM_IterateRegionDescriptor *region, void *userData);

typedef struct J9MM_CallbackDataHolderPrivate{
	jvmtiIterationControl (*func)(J9JavaVM *vm, J9MM_IterateObjectDescriptor *object, void *userData);
	void *userData;
	J9PortLibrary *portLibrary;
	UDATA flags;
} J9MM_CallbackDataHolderPrivate;


typedef enum J9MM_RegionType{
	j9mm_region_type_region = 0
} J9MM_RegionType;

typedef struct J9MM_IterateRegionDescriptorPrivate {
	J9MM_IterateRegionDescriptor descriptor; /**< Public region descriptor */
	J9MM_RegionType type; /**< Internal - type of region */
} J9MM_IterateRegionDescriptorPrivate;

#define HEAPITERATORAPI_REGION_NAME_FREE "Free Region"
#define HEAPITERATORAPI_REGION_NAME_RESERVED "Reserved Region"
#define HEAPITERATORAPI_REGION_NAME_ARRAYLET "Arraylet Region"
#define HEAPITERATORAPI_REGION_NAME_ADDRESS_ORDERED_TENURED "Tenured Region"
#define HEAPITERATORAPI_REGION_NAME_ADDRESS_ORDERED_NURSERY "Nursery Region"
#define HEAPITERATORAPI_REGION_NAME_SEGREGATED_SMALL "Small Region"
#define HEAPITERATORAPI_REGION_NAME_SEGREGATED_LARGE "Large Region"
#define HEAPITERATORAPI_REGION_NAME_SHARED "Shared Region"
#define HEAPITERATORAPI_REGION_NAME "Region"

/**
 * Walk all heaps, call user provided function.
 *
 * @param flags The flags describing the walk (unused currently)
 * @param func The function to call on each heap descriptor.
 * @param userData Pointer to storage for userData.
 */
jvmtiIterationControl
j9mm_iterate_heaps(
	J9JavaVM *vm,
	J9PortLibrary *portLibrary,
	UDATA flags,
	jvmtiIterationControl (*func)(J9JavaVM *vm, J9MM_IterateHeapDescriptor *heapDesc, void *userData),
	void *userData)
{
	/* There is only one heap currently.  Since we can not see the MM_Heap struct in
	 * OOP we will just use an id of 0x1.
	 */
	J9MM_IterateHeapDescriptor heapDescriptor;
	heapDescriptor.name = "Heap";
	heapDescriptor.id = 0x1;

	return func(vm, &heapDescriptor, userData);
}

/**
 * Walk all space for the given heap, call user provided function.
 *
 * @param heap The descriptor for the heap that should be walked
 * @param flags The flags describing the walk (unused currently)
 * @param func The function to call on each region descriptor.
 * @param userData Pointer to storage for userData.
 */
jvmtiIterationControl
j9mm_iterate_spaces(
	J9JavaVM *vm,
	J9PortLibrary *portLibrary,
	J9MM_IterateHeapDescriptor *heap,
	UDATA flags,
	jvmtiIterationControl (*func)(J9JavaVM *vm, J9MM_IterateSpaceDescriptor *spaceDesc, void *userData),
	void *userData)
{
	jvmtiIterationControl returnCode = JVMTI_ITERATION_CONTINUE;

	if (NULL == heap) {
		return JVMTI_ITERATION_CONTINUE;
	}

	void *defaultMemorySpace = vm->defaultMemorySpace;
	if (NULL != defaultMemorySpace) {
		J9MM_IterateSpaceDescriptor spaceDesc;
		spaceDesc.name = MM_MemorySpace::getMemorySpace(defaultMemorySpace)->getName();
		spaceDesc.id = (UDATA)defaultMemorySpace;
		spaceDesc.classPointerOffset = TMP_OFFSETOF_J9OBJECT_CLAZZ;
		spaceDesc.classPointerSize = sizeof(j9objectclass_t);
		spaceDesc.fobjectPointerDisplacement = 0;
#if defined(OMR_GC_COMPRESSED_POINTERS)
		if (J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm)) {
			spaceDesc.fobjectPointerScale = (UDATA)1 << vm->compressedPointersShift;
		} else
#endif /* OMR_GC_COMPRESSED_POINTERS */
		{
			spaceDesc.fobjectPointerScale = 1;
		}
		spaceDesc.fobjectSize = sizeof(fj9object_t);
		spaceDesc.memorySpace = defaultMemorySpace;

		returnCode = func(vm, &spaceDesc, userData);
	}

	return returnCode;
}

/**
 * Walk all roots in a heap, call user provided function.
 *
 * @param flags The flags describing the walk (unused currently)
 * @param func The function to call on each heap descriptor.
 * @param userData Pointer to storage for userData.
 */
jvmtiIterationControl
j9mm_iterate_roots(
		J9JavaVM *javaVM,
		J9PortLibrary *portLibrary,
		UDATA flags,
		rootIteratorCallBackFunc callBackFunc,
		void *userData)
{

	HeapIteratorAPI_RootIterator rootIterator(javaVM, callBackFunc, flags, userData);

	rootIterator.scanAllSlots();

	return JVMTI_ITERATION_ABORT;
}

/**
 * Walk all regions for the given heap, call user provided function.
 * A region may be a J9MemorySegment or a HeapIteratorAPI_Page.
 *
 * @param heap The descriptor for the heap that should be walked
 * @param flags The flags describing the walk: read only (region data only) or prepare heap for walk
 * @param func The function to call on each region descriptor.
 * @param userData Pointer to storage for userData.
 */
jvmtiIterationControl
j9mm_iterate_regions(
	J9JavaVM *vm,
	J9PortLibrary *portLibrary,
	J9MM_IterateSpaceDescriptor *space,
	UDATA flags,
	jvmtiIterationControl (*func)(J9JavaVM *vm, J9MM_IterateRegionDescriptor *regionDesc, void *userData),
	void *userData)
{
	if (NULL == space) {
		return JVMTI_ITERATION_CONTINUE;
	}

	if (j9mm_iterator_flag_regions_read_only != (flags & j9mm_iterator_flag_regions_read_only)) {
		/* It is not a read-only request - make sure the heap is walkable (flush TLH's, secure heap integrity) */
		vm->memoryManagerFunctions->j9gc_flush_caches_for_walk(vm);
	}

	return  iterateRegions(vm, space, flags, func, userData);

}

/**
 * Walk all objects for the given region, call user provided function.
 *
 * @param heap The descriptor for the region that should be walked
 * @param flags The flags describing the walk (0 or j9mm_iterator_objects_flag_fix_dead_objects)
 * @param func The function to call on each object descriptor.
 * @param userData Pointer to storage for userData.
 */
jvmtiIterationControl
j9mm_iterate_region_objects(
	J9JavaVM *vm,
	J9PortLibrary *portLibrary,
	J9MM_IterateRegionDescriptor *region,
	UDATA flags,
	jvmtiIterationControl (*func)(J9JavaVM *vm, J9MM_IterateObjectDescriptor *objectDesc, void *userData),
	void *userData)
{
	J9MM_IterateRegionDescriptorPrivate *privateRegion = (J9MM_IterateRegionDescriptorPrivate *)region;
	jvmtiIterationControl returnCode = JVMTI_ITERATION_CONTINUE;

	if (NULL == privateRegion) {
		return JVMTI_ITERATION_CONTINUE;
	}

	switch (privateRegion->type) {
	case j9mm_region_type_region:
		returnCode = iterateRegionObjects(vm, region, flags, func, userData);
		break;
	default:
		Assert_MM_unreachable();
		break;
	}

	return returnCode;
}

jvmtiIterationControl static
iterateObjectSlotDo(
		J9JavaVM *javaVM,
		GC_SlotObject *slotObject,
		J9MM_IterateObjectDescriptor *object,
		J9MM_IteratorObjectRefType type,
		UDATA flags,
		jvmtiIterationControl (*func)(J9JavaVM *javaVM, J9MM_IterateObjectDescriptor *objectDesc, J9MM_IterateObjectRefDescriptor *refDesc, void *userData),
		void *userData)
{
	jvmtiIterationControl returnCode = JVMTI_ITERATION_CONTINUE;
	J9Object *actualObject = slotObject->readReferenceFromSlot();
	if ( (NULL != actualObject) || (0 == (flags & j9mm_iterator_flag_exclude_null_refs)) ) {
		J9MM_IterateObjectRefDescriptor refDescriptor;
		refDescriptor.object = actualObject;
		refDescriptor.id = (UDATA)actualObject;
		refDescriptor.fieldAddress = slotObject->readAddressFromSlot();
		refDescriptor.type = type;
		returnCode = func(javaVM, object, &refDescriptor, userData);
		slotObject->writeReferenceToSlot(refDescriptor.object);
	}
	return returnCode;
}

jvmtiIterationControl static
iterateMixedObjectSlots(
		J9JavaVM *javaVM,
		J9Object *objectPtr,
		J9MM_IterateObjectDescriptor *object,
		UDATA flags,
		jvmtiIterationControl (*func)(J9JavaVM *javaVM, J9MM_IterateObjectDescriptor *objectDesc, J9MM_IterateObjectRefDescriptor *refDesc, void *userData),
		void *userData)
{
	GC_MixedObjectIterator mixedObjectIterator(javaVM->omrVM, objectPtr);
	GC_SlotObject *slotObject;
	jvmtiIterationControl returnCode = JVMTI_ITERATION_CONTINUE;

	while ((slotObject = mixedObjectIterator.nextSlot()) != NULL) {
		returnCode = iterateObjectSlotDo(javaVM, slotObject, object, j9mm_iterator_object_ref_type_object, flags, func, userData);
		if (JVMTI_ITERATION_ABORT == returnCode) {
			break;
		}
	}
	return returnCode;
}

jvmtiIterationControl static
iterateArrayObjectSlots(
		J9JavaVM *javaVM,
		J9Object *objectPtr,
		J9MM_IterateObjectDescriptor *object,
		UDATA flags,
		jvmtiIterationControl (*func)(J9JavaVM *javaVM, J9MM_IterateObjectDescriptor *objectDesc, J9MM_IterateObjectRefDescriptor *refDesc, void *userData),
		void *userData)
{
	GC_PointerArrayIterator pointerArrayIterator(javaVM, objectPtr);
	GC_SlotObject *slotObject;
	jvmtiIterationControl returnCode = JVMTI_ITERATION_CONTINUE;

	while ((slotObject = pointerArrayIterator.nextSlot()) != NULL) {
		returnCode = iterateObjectSlotDo(javaVM, slotObject, object, j9mm_iterator_object_ref_type_object, flags, func, userData);
		if (JVMTI_ITERATION_ABORT == returnCode) {
			break;
		}
	}
	return returnCode;
}

#if defined(J9VM_GC_ARRAYLETS)
jvmtiIterationControl static
iterateArrayletSlots(
		J9JavaVM *javaVM,
		J9Object *objectPtr,
		J9MM_IterateObjectDescriptor *object,
		UDATA flags,
		jvmtiIterationControl (*func)(J9JavaVM *javaVM, J9MM_IterateObjectDescriptor *objectDesc, J9MM_IterateObjectRefDescriptor *refDesc, void *userData),
		void *userData)
{
	jvmtiIterationControl returnCode = JVMTI_ITERATION_CONTINUE;
	if (j9mm_iterator_flag_include_arraylet_leaves == (flags & j9mm_iterator_flag_include_arraylet_leaves) ) {
		if (MM_GCExtensions::getExtensions(javaVM->omrVM)->indexableObjectModel.hasArrayletLeafPointers((J9IndexableObject *)objectPtr)) {
			GC_ArrayletLeafIterator arrayletLeafIterator(javaVM, (J9IndexableObject*)objectPtr);
			GC_SlotObject *slotObject = NULL;

			while (NULL != (slotObject = arrayletLeafIterator.nextLeafPointer())) {
				returnCode = iterateObjectSlotDo(javaVM, slotObject, object, j9mm_iterator_object_ref_type_arraylet_leaf, flags, func, userData);
				if (JVMTI_ITERATION_ABORT == returnCode) {
					break;
				}
			}
		}
	}
	return returnCode;
}
#endif /* J9VM_GC_ARRAYLETS */

/**
 * Walk all object slots for the given object, call user provided function.
 *
 * @param object The descriptor for the object that should be walked
 * @param flags The flags describing the walk (unused currently)
 * @param func The function to call on each object descriptor.
 * @param userData Pointer to storage for userData.
 */
jvmtiIterationControl
j9mm_iterate_object_slots(
	J9JavaVM *javaVM,
	J9PortLibrary *portLibrary,
	J9MM_IterateObjectDescriptor *object,
	UDATA flags,
	jvmtiIterationControl (*func)(J9JavaVM *javaVM, J9MM_IterateObjectDescriptor *objectDesc, J9MM_IterateObjectRefDescriptor *refDesc, void *userData),
	void *userData)
{
	jvmtiIterationControl returnCode = JVMTI_ITERATION_CONTINUE;
	J9Object *objectPtr = (J9Object *)(object->id);
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(javaVM->omrVM);
	
	switch(extensions->objectModel.getScanType(objectPtr)) {
	case GC_ObjectModel::SCAN_ATOMIC_MARKABLE_REFERENCE_OBJECT:
	case GC_ObjectModel::SCAN_MIXED_OBJECT:
	case GC_ObjectModel::SCAN_OWNABLESYNCHRONIZER_OBJECT:
	case GC_ObjectModel::SCAN_CLASS_OBJECT:
	case GC_ObjectModel::SCAN_CLASSLOADER_OBJECT:
	case GC_ObjectModel::SCAN_REFERENCE_MIXED_OBJECT:
		returnCode = iterateMixedObjectSlots(javaVM, objectPtr, object, flags, func, userData);
		break;

	case GC_ObjectModel::SCAN_POINTER_ARRAY_OBJECT:
		returnCode = iterateArrayObjectSlots(javaVM, objectPtr, object, flags, func, userData);
#if defined(J9VM_GC_ARRAYLETS)
		if (JVMTI_ITERATION_CONTINUE == returnCode) {
			returnCode = iterateArrayletSlots(javaVM, objectPtr, object, flags, func, userData);
		}
#endif /* J9VM_GC_ARRAYLETS */
		break;

	case GC_ObjectModel::SCAN_PRIMITIVE_ARRAY_OBJECT:
#if defined(J9VM_GC_ARRAYLETS)
		returnCode = iterateArrayletSlots(javaVM, objectPtr, object, flags, func, userData);
#endif /* J9VM_GC_ARRAYLETS */
		break;

	default:
		Assert_MM_unreachable();
	}

	return returnCode;
}


/**
 * Provide the arraylet identification bitmask.  For builds that do not
 * support arraylets all values are set to 0.
 *
 * @return arrayletLeafSize
 * @return offset
 * @return width
 * @return mask
 * @return result
 * @return 0 on success, non-0 on failure.
 */
UDATA
j9mm_arraylet_identification(
	J9JavaVM *javaVM,
	UDATA *arrayletLeafSize,
	UDATA *offset,
	UDATA *width,
	UDATA *mask,
	UDATA *result)
{
#if defined(J9VM_GC_ARRAYLETS)
	/*
	 * This a temporary fix, this place should be modified
	 * OBJECT_HEADER_INDEXABLE is stored in RAM class in classDepthAndFlags field
	 * and should be taken from there
	 * (this is correct for non-SWH specifications as well)
	 * Correspondent DTFJ code must be changed
	 */
	*arrayletLeafSize = javaVM->arrayletLeafSize;
	*offset = 0;
	*width = 0;
	*mask = 0;
	*result = 0;
#else /* J9VM_GC_ARRAYLETS */
	*arrayletLeafSize = 0;
	*offset = 0;
	*width = 0;
	*mask = 0;
	*result = 0;
#endif /* J9VM_GC_ARRAYLETS */
	return 0;
}

/**
 * Initialize a descriptor for the specified object.
 * This descriptor may subsequently be used with j9mm_iterate_object_slots or other iterator APIs.
 *
 * @return descriptor The descriptor to be initialized
 * @param object The object to store into the descriptor
 */
void
j9mm_initialize_object_descriptor(J9JavaVM *javaVM, J9MM_IterateObjectDescriptor *descriptor, j9object_t object)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(javaVM->omrVM);
	
	descriptor->id = (UDATA)object;
	descriptor->object = object;
	descriptor->size = extensions->objectModel.getConsumedSizeInBytesWithHeader(object);
	descriptor->isObject = TRUE;
}

/**
 * Walk all objects reachable under the given VM, call user provided function.
 * @param flags The flags describing the walk (unused currently)
 * @param func The function to call on each object descriptor.
 * @param userData Pointer to storage for userData.
 */
jvmtiIterationControl
j9mm_iterate_all_objects(J9JavaVM *vm, J9PortLibrary *portLibrary, UDATA flags, jvmtiIterationControl (*func)(J9JavaVM *vm, J9MM_IterateObjectDescriptor *object, void *userData), void *userData)
{
	J9MM_CallbackDataHolderPrivate data;
	data.func = func;
	data.userData = userData;
	data.portLibrary = portLibrary;
	data.flags = flags;
	return j9mm_iterate_heaps(vm, portLibrary, flags, &internalIterateHeaps, &data);
}

/* used by j9mm_iterate_all_objects */
static jvmtiIterationControl
internalIterateHeaps(J9JavaVM *vm, J9MM_IterateHeapDescriptor *heap, void *userData)
{
	J9MM_CallbackDataHolderPrivate *data = (J9MM_CallbackDataHolderPrivate *)userData;
	return j9mm_iterate_spaces(vm, data->portLibrary, heap, data->flags, &internalIterateSpaces, userData);
}

static jvmtiIterationControl
internalIterateSpaces(J9JavaVM *vm, J9MM_IterateSpaceDescriptor *space, void *userData)
{
	J9MM_CallbackDataHolderPrivate *data = (J9MM_CallbackDataHolderPrivate *)userData;
	return j9mm_iterate_regions(vm, data->portLibrary, space, data->flags, internalIterateRegions, userData);
}

static jvmtiIterationControl
internalIterateRegions(J9JavaVM *vm, J9MM_IterateRegionDescriptor *region, void *userData)
{
	J9MM_CallbackDataHolderPrivate *data = (J9MM_CallbackDataHolderPrivate *)userData;
	return j9mm_iterate_region_objects(vm, data->portLibrary, region, data->flags, data->func, data->userData);
}

/**
 * Walk all ownable synchronizer object, call user provided function.
 * @param flags The flags describing the walk (unused currently)
 * @param func The function to call on each object descriptor.
 * @param userData Pointer to storage for userData.
 * @return return 0 on successfully iterating entire list, return user provided function call if it did not return JVMTI_ITERATION_CONTINUE
 */
jvmtiIterationControl
j9mm_iterate_all_ownable_synchronizer_objects(J9VMThread *vmThread, J9PortLibrary *portLibrary, UDATA flags, jvmtiIterationControl (*func)(J9VMThread *vmThread, J9MM_IterateObjectDescriptor *object, void *userData), void *userData)
{
	J9JavaVM *javaVM = vmThread->javaVM;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM->omrVM);
	MM_ObjectAccessBarrier *barrier = extensions->accessBarrier;
	MM_OwnableSynchronizerObjectList *ownableSynchronizerObjectList = extensions->ownableSynchronizerObjectLists;

	Assert_MM_true(NULL != ownableSynchronizerObjectList);

	J9MM_IterateObjectDescriptor objectDescriptor;
	J9MM_IterateRegionDescriptor regionDesc;
	jvmtiIterationControl returnCode = JVMTI_ITERATION_CONTINUE;

	while (NULL != ownableSynchronizerObjectList) {
		J9Object *objectPtr = ownableSynchronizerObjectList->getHeadOfList();
		while (NULL != objectPtr) {
			UDATA regionFound = j9mm_find_region_for_pointer(javaVM, objectPtr, &regionDesc);
			if (0 != regionFound) {
				initializeObjectDescriptor(javaVM, &objectDescriptor, &regionDesc, objectPtr);
				returnCode = func(vmThread, &objectDescriptor, userData);
				if (JVMTI_ITERATION_ABORT == returnCode) {
					return returnCode;
				}
			} else {
				Assert_MM_unreachable();
			}
			objectPtr = barrier->getOwnableSynchronizerLink(objectPtr);
		}
		ownableSynchronizerObjectList = ownableSynchronizerObjectList->getNextList();
	}
	return returnCode;
}

} /* extern "C" */

/**
 * Initialize the specified descriptor with the specified values.
 * Invariant fields are initialized to the appropriate values for the JVM.
 *
 * @param[in] extensions - a pointer to the GC extensions
 * @param[out] descriptor - the descriptor to initialize
 * @param[in] region - The region to use for descriptor initialization
 */
static void
initializeRegionDescriptor(MM_GCExtensionsBase* extensions, J9MM_IterateRegionDescriptor *descriptor, MM_HeapRegionDescriptor *region)
{
	UDATA objectAlignment = extensions->getObjectAlignmentInBytes();
	UDATA objectMinimumSize = 0;
	const char *regionName = NULL;
#if defined(J9VM_GC_MINIMUM_OBJECT_SIZE)
	objectMinimumSize = J9_GC_MINIMUM_OBJECT_SIZE;
#else /* J9VM_GC_MINIMUM_OBJECT_SIZE */
	objectMinimumSize = sizeof(J9Object);
#endif /* J9VM_GC_MINIMUM_OBJECT_SIZE */
	
	switch(region->getRegionType()) {
	case MM_HeapRegionDescriptor::RESERVED:
		regionName = HEAPITERATORAPI_REGION_NAME_RESERVED;
		objectAlignment = 0;
		objectMinimumSize = 0;
		break;
	case MM_HeapRegionDescriptor::FREE:
	case MM_HeapRegionDescriptor::ADDRESS_ORDERED_IDLE:
	case MM_HeapRegionDescriptor::BUMP_ALLOCATED_IDLE:
		/* (for all intents and purposes, an IDLE region is the same as a FREE region) */
		regionName = HEAPITERATORAPI_REGION_NAME_FREE;
		objectAlignment = 0;
		objectMinimumSize = 0;
		break;
#if defined(J9VM_GC_ARRAYLETS)
	case MM_HeapRegionDescriptor::ARRAYLET_LEAF:
		regionName = HEAPITERATORAPI_REGION_NAME_ARRAYLET;
		objectAlignment = 0;
		objectMinimumSize = 0;
		break;
#endif /* defined(J9VM_GC_ARRAYLETS) */
#if defined(J9VM_GC_SEGREGATED_HEAP)
	case MM_HeapRegionDescriptor::SEGREGATED_SMALL:
		regionName = HEAPITERATORAPI_REGION_NAME_SEGREGATED_SMALL;
		objectMinimumSize = ((MM_HeapRegionDescriptorSegregated *)region)->getCellSize();
		break;
	case MM_HeapRegionDescriptor::SEGREGATED_LARGE:
		regionName = HEAPITERATORAPI_REGION_NAME_SEGREGATED_LARGE;
		objectMinimumSize = region->getSize();
		break;
#endif /* J9VM_GC_SEGREGATED_HEAP */
	case MM_HeapRegionDescriptor::ADDRESS_ORDERED:
	case MM_HeapRegionDescriptor::BUMP_ALLOCATED:
		if (extensions->isVLHGC()) {
#if defined(J9VM_GC_VLHGC)
			regionName = HEAPITERATORAPI_REGION_NAME_ADDRESS_ORDERED_NURSERY;
#endif /* J9VM_GC_VLHGC */
		} else {
			if (MEMORY_TYPE_NEW == (region->getTypeFlags() & MEMORY_TYPE_NEW)) {
				regionName = HEAPITERATORAPI_REGION_NAME_ADDRESS_ORDERED_NURSERY;
			} else if (MEMORY_TYPE_OLD == (region->getTypeFlags() & MEMORY_TYPE_OLD)) {
				regionName = HEAPITERATORAPI_REGION_NAME_ADDRESS_ORDERED_TENURED;
			} else {
				regionName = HEAPITERATORAPI_REGION_NAME;
			}
		}
		break;
#if defined(J9VM_GC_VLHGC)
	case MM_HeapRegionDescriptor::ADDRESS_ORDERED_MARKED:
	case MM_HeapRegionDescriptor::BUMP_ALLOCATED_MARKED:
		regionName = HEAPITERATORAPI_REGION_NAME_ADDRESS_ORDERED_TENURED;
		break;
#endif /* J9VM_GC_VLHGC */
	default:
		Assert_MM_unreachable();
	}
	
	initializeRegionDescriptor(descriptor, regionName, (UDATA)region, objectAlignment, objectMinimumSize, region->getLowAddress(), region->getSize());
}

/**
 * Initialize the specified descriptor with the specified values.
 * Invariant fields are initialized to the appropriate values for the JVM.
 *
 * @param[out] descriptor - the descriptor to initialize
 * @param[in] name - name of the region
 * @param[in] id - id of the region
 * @param[in] start - start address of the region
 * @param[in] size - size (in bytes) of the region
 */
static void
initializeRegionDescriptor(J9MM_IterateRegionDescriptor *descriptor, const char* name, UDATA id, UDATA objectAlignment, UDATA objectMinimumSize, void* start, UDATA size)
{
	descriptor->name = name;
	descriptor->id = id;
	descriptor->objectAlignment = objectAlignment;
	descriptor->objectMinimumSize = objectMinimumSize;
	descriptor->regionStart = start;
	descriptor->regionSize = size;
}

/**
 * Initialize a descriptor for the specified object.
 * This descriptor may subsequently be used with j9mm_iterate_object_slots or other iterator APIs.
 *
 * @return descriptor The descriptor to be initialized
 * @param regionDesc the region to use to round object to minimum object size
 * @param object The object to store into the descriptor
 */
static void
initializeObjectDescriptor(
		J9JavaVM *javaVM, 
		J9MM_IterateObjectDescriptor *descriptor, 
		J9MM_IterateRegionDescriptor *regionDesc, 
		j9object_t object)
{
	j9mm_initialize_object_descriptor(javaVM, descriptor, object);

#if defined(J9VM_GC_SEGREGATED_HEAP)
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(javaVM->omrVM);

	if (extensions->isSegregatedHeap()) {
		/* TODO remove this once the objectModel properly handles Segregated Heaps */
		if (descriptor->size < regionDesc->objectMinimumSize) {
			descriptor->size = regionDesc->objectMinimumSize;
		}
	}
#endif /* J9VM_GC_SEGREGATED_HEAP */
}

static jvmtiIterationControl
iterateRegions(
		J9JavaVM *vm,
		J9MM_IterateSpaceDescriptor *space,
		UDATA flags,
		jvmtiIterationControl (*func)(J9JavaVM *vm, J9MM_IterateRegionDescriptor *regionDesc, void *userData),
		void *userData)
{
	jvmtiIterationControl returnCode = JVMTI_ITERATION_CONTINUE;

	MM_MemorySpace *memorySpace = MM_MemorySpace::getMemorySpace((void *)space->id);
	MM_HeapRegionManager *manager = memorySpace->getHeap()->getHeapRegionManager();
	manager->lock();

	GC_HeapRegionIterator regionIterator(manager, memorySpace);
	MM_HeapRegionDescriptor *region = NULL;
	MM_GCExtensionsBase* extensions = MM_GCExtensionsBase::getExtensions(vm->omrVM);
	while(NULL != (region = regionIterator.nextRegion())) {
		J9MM_IterateRegionDescriptorPrivate regionDescription;
		regionDescription.type = j9mm_region_type_region;
		initializeRegionDescriptor(extensions, &regionDescription.descriptor, region);
		returnCode = func(vm, &(regionDescription.descriptor), userData);
		if (JVMTI_ITERATION_ABORT == returnCode) {
			break;
		}
	}
	manager->unlock();

	return returnCode;
}

static jvmtiIterationControl
iterateRegionObjects(
	J9JavaVM *vm,
	J9MM_IterateRegionDescriptor *region,
	UDATA flags,
	jvmtiIterationControl (*func)(J9JavaVM *vm, J9MM_IterateObjectDescriptor *objectDesc, void *userData),
	void *userData)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	jvmtiIterationControl returnCode = JVMTI_ITERATION_CONTINUE;
	/* Iterate over live and dead objects */
	MM_HeapRegionDescriptor* heapRegion = (MM_HeapRegionDescriptor*)region->id;
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(vm->omrVM);
	HeapIteratorAPI_BufferedIterator objectHeapIterator(vm, PORTLIB, heapRegion, true);
	J9Object* object = NULL;
	while(NULL != (object = objectHeapIterator.nextObject())) {
		J9MM_IterateObjectDescriptor objectDescriptor;
		if ((extensions->objectModel.isDeadObject(object)) || (0 != (J9CLASS_FLAGS(J9GC_J9OBJECT_CLAZZ(object)) & J9AccClassDying))) {
			if (0 != (flags & j9mm_iterator_flag_include_holes)) {
				if (extensions->objectModel.isDeadObject(object)) {
					objectDescriptor.id = (UDATA)object;
					objectDescriptor.object = object;
					objectDescriptor.size = extensions->objectModel.getSizeInBytesDeadObject(object);
				} else {
					/* this object is not marked as a hole, but its class has been partially unloaded so it's treated like a hole here */
					j9mm_initialize_object_descriptor(vm, &objectDescriptor, object);
				}
				objectDescriptor.isObject = FALSE;
				returnCode = func(vm, &objectDescriptor, userData);
				if (JVMTI_ITERATION_ABORT == returnCode) {
					break;
				}
			}
		} else {
			initializeObjectDescriptor(vm, &objectDescriptor, region, object);
			returnCode = func(vm, &objectDescriptor, userData);
			if (JVMTI_ITERATION_ABORT == returnCode) {
				break;
			}
		}
	}

	return returnCode;
}

/**
 * Find the Region that the pointer belongs too.
 * Returns true if region found.
 */
UDATA
j9mm_find_region_for_pointer(J9JavaVM* javaVM, void *pointer, J9MM_IterateRegionDescriptor *regionDesc)
{
	MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(javaVM->omrVM);
	bool found = false;
	MM_HeapRegionManager *regionManager = extensions->heap->getHeapRegionManager();
	/* See if the pointer is being managed by the RegionManager */
	MM_HeapRegionDescriptor *region = regionManager->regionDescriptorForAddress(pointer);
	
	if (NULL != region) {
		initializeRegionDescriptor(extensions, regionDesc, region);
		found = true;
	}

	return (UDATA) found; /* TODO: fix this, we really return bool */
}

/**
 * Convert the memory that is currently represented as an object to dark matter.
 * This routine is typically used by GC tools such as gccheck and tgc to ensure that only valid (i.e. reachable)
 * objects remain on the heap after a sweep. An 'dead' object may contain dangling references to heap addresses
 * which no longer contain walkable objects. The problem is solved by converting the invalid objects to dark matter.
 *
 * @param[in] javaVM the JavaVM
 * @param[in] region the region in which the object exists
 * @param[in] objectDesc the object to be abandoned
 * @return 0 on success, non-0 on failure.
 */
UDATA
j9mm_abandon_object(
	J9JavaVM *javaVM,
	J9MM_IterateRegionDescriptor *region,
	J9MM_IterateObjectDescriptor *objectDesc)
{
	UDATA rc = 0;
	J9MM_IterateRegionDescriptorPrivate *privateRegion = (J9MM_IterateRegionDescriptorPrivate *)region;

	if (j9mm_region_type_region == privateRegion->type) {
		MM_HeapRegionDescriptor* heapRegion = (MM_HeapRegionDescriptor*)region->id;
		MM_MemorySubSpace* memorySubSpace = heapRegion->getSubSpace();

		UDATA deadObjectByteSize = MM_GCExtensions::getExtensions(javaVM)->objectModel.getConsumedSizeInBytesWithHeader(objectDesc->object);
		memorySubSpace->abandonHeapChunk(objectDesc->object, ((U_8*)objectDesc->object) + deadObjectByteSize);

	} else {
		rc = 1;
	}

	return rc;
}
