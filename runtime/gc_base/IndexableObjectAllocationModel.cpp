/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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

#include "ArrayletObjectModel.hpp"
#include "GCExtensions.hpp"
#include "IndexableObjectAllocationModel.hpp"
#include "Math.hpp"
#include "MemorySpace.hpp"
#if defined(J9VM_GC_ENABLE_DOUBLE_MAP)
#include "ArrayletLeafIterator.hpp"
#include "HeapRegionManager.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"
#include "Heap.hpp"
#endif /* J9VM_GC_ENABLE_DOUBLE_MAP */

/**
 * Allocation description and layout initialization. This is called before OMR allocates
 * (and possibly zeroes) the raw bytes for the arraylet spine.
 */
bool
MM_IndexableObjectAllocationModel::initializeAllocateDescription(MM_EnvironmentBase *env)
{
	/* prerequisite base class initialization of description */
	if (!isAllocatable()) {
		return false;
	}

	/* continue, with reservations */
	setAllocatable(false);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	uintptr_t spineBytes = extensions->indexableObjectModel.getSpineSize(_class, _layout, _numberOfArraylets, _dataSize, _alignSpineDataSection);
#if defined (J9VM_GC_MODRON_COMPACTION) || defined (J9VM_GC_GENERATIONAL)
	if (_allocateDescription.getPreHashFlag()) {
		if (spineBytes == extensions->indexableObjectModel.getHashcodeOffset(_class, _layout, _numberOfIndexedFields)) {
			/* Add extra uintptr_t for hash */
			spineBytes += sizeof(uintptr_t);
		}
	}
#endif /* defined (J9VM_GC_MODRON_COMPACTION) || defined (J9VM_GC_GENERATIONAL) */
	spineBytes = extensions->objectModel.adjustSizeInBytes(spineBytes);

	/* determine size of layout overhead (additional to spine bytes) and finalize allocation description */
	uintptr_t layoutSizeInBytes = 0;
	switch (_layout) {
	case GC_ArrayletObjectModel::Illegal:
		/* invalid layout - not allocatable */
		Assert_MM_unreachable();
		break;

	case GC_ArrayletObjectModel::InlineContiguous:
		/* all good */
		setAllocatable(true);
		break;

	case GC_ArrayletObjectModel::Discontiguous:
		/* non-empty discontiguous arrays require slow-path allocate */
		if (isGCAllowed() || (0 == _numberOfIndexedFields)) {
			/* _numberOfArraylets discontiguous leaves, all contains leaf size bytes */
			layoutSizeInBytes = _dataSize;
			_allocateDescription.setChunkedArray(true);
			Trc_MM_allocateAndConnectNonContiguousArraylet_Entry(env->getLanguageVMThread(),
					_numberOfIndexedFields, spineBytes, _numberOfArraylets);
			setAllocatable(true);
		}
		break;

	case GC_ArrayletObjectModel::Hybrid:
		Assert_MM_true(0 < _numberOfArraylets);
		/* hybrid arrays always require slow-path allocate */
		if (isGCAllowed()) {
			/* (_dataSize % leaf size) bytes in spine, ((n-1) * leaf size) bytes in (_numberOfArraylets - 1) leaves */
			layoutSizeInBytes = env->getOmrVM()->_arrayletLeafSize * (_numberOfArraylets - 1);
			_allocateDescription.setChunkedArray(true);
			Trc_MM_allocateAndConnectNonContiguousArraylet_Entry(env->getLanguageVMThread(),
					_numberOfIndexedFields, spineBytes, _numberOfArraylets);
			setAllocatable(true);
		}
		break;

	default:
		Assert_MM_unreachable();
		break;
	}

	if (isAllocatable()) {
		/* set total request size and layout metadata to finalize the description */
		_allocateDescription.setBytesRequested(spineBytes + layoutSizeInBytes);
		_allocateDescription.setNumArraylets(_numberOfArraylets);
		_allocateDescription.setSpineBytes(spineBytes);
		return true;
	}
	return false;
}

/**
 * Initializer. This is called after OMR has allocated raw (possibly zeroed) bytes for the spine
 */
omrobjectptr_t
MM_IndexableObjectAllocationModel::initializeIndexableObject(MM_EnvironmentBase *env, void *allocatedBytes)
{
	/* Set array object header and size (in elements) and set  description spine pointer */
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	J9IndexableObject *spine = (J9IndexableObject*)initializeJavaObject(env, allocatedBytes);
	_allocateDescription.setSpine(spine);
	if (NULL != spine) {
		/* Set the array size */
		if (getAllocateDescription()->isChunkedArray()) {
			extensions->indexableObjectModel.setSizeInElementsForDiscontiguous(spine, _numberOfIndexedFields);
		} else {
			extensions->indexableObjectModel.setSizeInElementsForContiguous(spine, _numberOfIndexedFields);
		}
	}


	/* Lay out arraylet and arrayoid pointers */
	switch (_layout) {
	case GC_ArrayletObjectModel::InlineContiguous:
		Assert_MM_true(1 == _numberOfArraylets);
		break;

	case GC_ArrayletObjectModel::Discontiguous:
	case GC_ArrayletObjectModel::Hybrid:
		if (NULL != spine) {
			if(0 == _numberOfIndexedFields) {
				/* Don't try to initialize the arrayoid for an empty NUA */
				Trc_MM_allocateAndConnectNonContiguousArraylet_Exit(env->getLanguageVMThread(), spine);
				break;
			}
			Trc_MM_allocateAndConnectNonContiguousArraylet_Summary(env->getLanguageVMThread(),
					_numberOfIndexedFields, getAllocateDescription()->getContiguousBytes(), _numberOfArraylets);
			spine = layoutDiscontiguousArraylet(env, spine);
			Trc_MM_allocateAndConnectNonContiguousArraylet_Exit(env->getLanguageVMThread(), spine);
		} else {
			Trc_MM_allocateAndConnectNonContiguousArraylet_spineFailure(env->getLanguageVMThread());
		}
		break;

	default:
		Assert_MM_unreachable();
		break;
	}

	if (NULL != spine) {
		/* Initialize hashcode slot */
		if (getAllocateDescription()->getPreHashFlag()) {
			env->getExtensions()->objectModel.initializeHashSlot((J9JavaVM*)env->getLanguageVM(), (omrobjectptr_t)spine);
		}
		Assert_MM_true(env->getExtensions()->objectModel.isIndexable((omrobjectptr_t)spine));
	}

	Assert_MM_true(spine == _allocateDescription.getSpine());
	return (omrobjectptr_t)spine;
}

/**
 * For contiguous arraylet all data is in the spine but arrayoid pointers must still be laid down.
 *
 * @return initialized arraylet spine with its arraylet pointers initialized.
 */
MMINLINE J9IndexableObject *
MM_IndexableObjectAllocationModel::layoutContiguousArraylet(MM_EnvironmentBase *env, J9IndexableObject *spine)
{
	Assert_MM_true(_numberOfArraylets == _allocateDescription.getNumArraylets());
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	bool const compressed = env->compressObjectReferences();

	/* set arraylet pointers in the spine. these all point into the data part of the spine */
	fj9object_t *arrayoidPtr = extensions->indexableObjectModel.getArrayoidPointer(spine);
	uintptr_t leafOffset = (uintptr_t)GC_SlotObject::addToSlotAddress(arrayoidPtr, _numberOfArraylets, compressed);
	if (_alignSpineDataSection) {
		leafOffset = MM_Math::roundToCeiling(sizeof(uint64_t), leafOffset);
	}
	uintptr_t arrayletLeafSize = env->getOmrVM()->_arrayletLeafSize;
	for (uintptr_t i = 0; i < _numberOfArraylets; i++) {
		GC_SlotObject slotObject(env->getOmrVM(), arrayoidPtr);
		slotObject.writeReferenceToSlot((omrobjectptr_t)leafOffset);
		leafOffset += arrayletLeafSize;
		arrayoidPtr = GC_SlotObject::addToSlotAddress(arrayoidPtr, 1, compressed);
	}

	return spine;
}

/**
 * For discontiguous or hybrid arraylet spine is allocated first and leaves are sequentially
 * allocated and attached to the spine. The allocation description saves and restores the
 * spine pointer in case a GC occurs while allocating the leaves.
 *
 * If a leaf allocation fails the spine and preceding arraylets are abandoned as floating
 * garbage and NULL is returned.
 *
 * @return initialized arraylet spine with attached arraylet leaves, or NULL
 */
MMINLINE J9IndexableObject *
MM_IndexableObjectAllocationModel::layoutDiscontiguousArraylet(MM_EnvironmentBase *env, J9IndexableObject *spine)
{
	Assert_MM_true(_numberOfArraylets == _allocateDescription.getNumArraylets());

	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	GC_ArrayObjectModel *indexableObjectModel = &extensions->indexableObjectModel;
	bool const compressed = env->compressObjectReferences();

	/* determine how many bytes to allocate outside of the spine (in arraylet leaves) */
	const uintptr_t arrayletLeafSize = env->getOmrVM()->_arrayletLeafSize;
	Assert_MM_true(_allocateDescription.getBytesRequested() >= _allocateDescription.getContiguousBytes());
	uintptr_t bytesRemaining = _allocateDescription.getBytesRequested() - _allocateDescription.getContiguousBytes();
	Assert_MM_true((0 == (bytesRemaining % arrayletLeafSize)) || (GC_ArrayletObjectModel::Hybrid != _layout));
	/* hybrid arraylets store _dataSize % arrayletLeafSize bytes in the spine, remainder in _numberOfArraylets-1 leaves */

	/* allocate leaf for each arraylet and attach it to its leaf pointer in the spine */
	uintptr_t arrayoidIndex = 0;
	fj9object_t *arrayoidPtr = indexableObjectModel->getArrayoidPointer(spine);
	while (0 < bytesRemaining) {
		/* allocate the next arraylet leaf */
		void *leaf = env->_objectAllocationInterface->allocateArrayletLeaf(env, &_allocateDescription,
				_allocateDescription.getMemorySpace(), true);

		/* if leaf allocation failed set the result to NULL and return */
		if (NULL == leaf) {
			/* spine and preceding arraylets are now floating garbage */
			Trc_MM_allocateAndConnectNonContiguousArraylet_leafFailure(env->getLanguageVMThread());
			_allocateDescription.setSpine(NULL);
			spine = NULL;
			break;
		}

		/* refresh the spine -- it might move if we GC while allocating the leaf */
		spine = _allocateDescription.getSpine();
		arrayoidPtr = indexableObjectModel->getArrayoidPointer(spine);

		/* set the arrayoid pointer in the spine to point to the new leaf */
		GC_SlotObject slotObject(env->getOmrVM(), GC_SlotObject::addToSlotAddress(arrayoidPtr, arrayoidIndex, compressed));
		slotObject.writeReferenceToSlot((omrobjectptr_t)leaf);

		bytesRemaining -= OMR_MIN(bytesRemaining, arrayletLeafSize);
		arrayoidIndex += 1;
	}

	if (NULL != spine) {
		switch (_layout) {
		case GC_ArrayletObjectModel::Discontiguous:
			indexableObjectModel->AssertArrayletIsDiscontiguous(spine);
			Assert_MM_true(arrayoidIndex == _numberOfArraylets);
#if defined(J9VM_GC_ENABLE_DOUBLE_MAP)
			if (indexableObjectModel->isDoubleMappingEnabled()) {
				/**
				 * There are some special cases where double mapping an arraylet is
				 * not necessary; isArrayletDataDiscontiguous() details those cases.
				 */
				if (indexableObjectModel->isArrayletDataDiscontiguous(spine)) {
					doubleMapArraylets(env, (J9Object *)spine);
				}
			}
#endif /* J9VM_GC_ENABLE_DOUBLE_MAP */
			break;

		case GC_ArrayletObjectModel::Hybrid:
#if defined(J9VM_GC_ENABLE_DOUBLE_MAP)
			/* Unreachable if double map is enabled */
			if (indexableObjectModel->isDoubleMappingEnabled()) {
				Assert_MM_double_map_unreachable();
			}
#endif /* J9VM_GC_ENABLE_DOUBLE_MAP */
			/* last arrayoid points to end of arrayoid array in spine header (object-aligned if
			 * required). (data size % leaf size) bytes of data are stored here (may be empty).
			 */
			Assert_MM_true(arrayoidIndex == (_numberOfArraylets - 1));
			{
				uintptr_t leafOffset = (uintptr_t)GC_SlotObject::addToSlotAddress(arrayoidPtr, _numberOfArraylets, compressed);
				if (_alignSpineDataSection) {
					leafOffset = MM_Math::roundToCeiling(env->getObjectAlignmentInBytes(), leafOffset);
				}
				/* set the last arrayoid pointer to point to remainder data */
				GC_SlotObject slotObject(env->getOmrVM(), GC_SlotObject::addToSlotAddress(arrayoidPtr, arrayoidIndex, compressed));
				slotObject.writeReferenceToSlot((omrobjectptr_t)leafOffset);
			}
			break;

		default:
			Assert_MM_unreachable();
			break;
		}
	}

	return spine;
}

#if defined(J9VM_GC_ENABLE_DOUBLE_MAP)
#if !(defined(LINUX) && defined(J9VM_ENV_DATA64))
/* Double map is only supported on LINUX 64 bit Systems for now */
#error "Platform not supported by Double Map API"
#endif /* !(defined(LINUX) && defined(J9VM_ENV_DATA64)) */
void * 
MM_IndexableObjectAllocationModel::doubleMapArraylets(MM_EnvironmentBase *env, J9Object *objectPtr) 
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	J9JavaVM *javaVM = extensions->getJavaVM();

	GC_ArrayletLeafIterator arrayletLeafIterator(javaVM, (J9IndexableObject *)objectPtr);
	MM_Heap *heap = extensions->getHeap();
	UDATA arrayletLeafSize = env->getOmrVM()->_arrayletLeafSize;
	UDATA arrayletLeafCount = MM_Math::roundToCeiling(arrayletLeafSize, _dataSize) / arrayletLeafSize;
	Trc_MM_double_map_Entry(env->getLanguageVMThread(), (void *)objectPtr, arrayletLeafSize, arrayletLeafCount);

	void *result = NULL;

#define ARRAYLET_ALLOC_THRESHOLD 64
	void *leaves[ARRAYLET_ALLOC_THRESHOLD];
	void **arrayletLeaveAddrs = leaves;
	if (arrayletLeafCount > ARRAYLET_ALLOC_THRESHOLD) {
		arrayletLeaveAddrs = (void **)env->getForge()->allocate(arrayletLeafCount * sizeof(uintptr_t), MM_AllocationCategory::GC_HEAP, J9_GET_CALLSITE());
	}

	if (NULL == arrayletLeaveAddrs) {
		return NULL;
	}

	GC_SlotObject *slotObject = NULL;
	uintptr_t count = 0;

	while (NULL != (slotObject = arrayletLeafIterator.nextLeafPointer())) {
		void *currentLeaf = slotObject->readReferenceFromSlot();
		arrayletLeaveAddrs[count] = currentLeaf;
		count++;
	}

	/* Number of arraylet leaves in the iterator must match the number of leaves calculated */
	Assert_MM_true(arrayletLeafCount == count);

	GC_SlotObject objectSlot(env->getOmrVM(), extensions->indexableObjectModel.getArrayoidPointer((J9IndexableObject *)objectPtr));
	J9Object *firstLeafSlot = objectSlot.readReferenceFromSlot();

	MM_HeapRegionDescriptorVLHGC *firstLeafRegionDescriptor = (MM_HeapRegionDescriptorVLHGC *)heap->getHeapRegionManager()->tableDescriptorForAddress(firstLeafSlot);

	/* Retrieve actual page size */
	UDATA pageSize = heap->getPageSize();

	/* Get heap and from there call an OMR API that will doble map everything */
	result = heap->doubleMapArraylet(env, arrayletLeaveAddrs, count, arrayletLeafSize, _dataSize,
				&firstLeafRegionDescriptor->_arrayletDoublemapID,
				pageSize);

	if (arrayletLeafCount > ARRAYLET_ALLOC_THRESHOLD) {
		env->getForge()->free((void *)arrayletLeaveAddrs);
	}

	/*
	 * Double map failed.
	 * If doublemap fails the caller must handle it appropriately. The only case being
	 * JNI critical, where it will fall back to copying each element of the array to
	 * a temporary array (logic handled by JNI Critical). It might hurt performance
	 * but execution won't halt.
	 */
	if (NULL == firstLeafRegionDescriptor->_arrayletDoublemapID.address) {
		Trc_MM_double_map_Failed(env->getLanguageVMThread());
		result = NULL;
	}

	Trc_MM_double_map_Exit(env->getLanguageVMThread(), result);
	return result;
}
#endif /* J9VM_GC_ENABLE_DOUBLE_MAP */

