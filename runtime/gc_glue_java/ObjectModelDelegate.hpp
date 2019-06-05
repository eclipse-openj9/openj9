/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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

#ifndef OBJECTMODELDELEGATE_HPP_
#define OBJECTMODELDELEGATE_HPP_

#include "j9nonbuilder.h"
#include "omr.h"
#include "objectdescription.h"
#include "util_api.h"

#include "ArrayObjectModel.hpp"
#include "ForwardedHeader.hpp"
#include "MixedObjectModel.hpp"

class MM_AllocateInitialization;
class MM_EnvironmentBase;

#define CLI_THREAD_TYPE J9VMThread

struct CLI_THREAD_TYPE;

/**
 * GC_ObjectModelBase is inaccessible from this class. Some factors are redefined here.
 */
class GC_ObjectModelDelegate
{
/*
 * Member data and types
 */
private:
	static const uintptr_t _objectHeaderSlotOffset = 0;
	static const uintptr_t _objectHeaderSlotFlagsShift = 0;

	const fomrobject_t _delegateHeaderSlotFlagsMask;

	GC_ArrayObjectModel *_arrayObjectModel;
	GC_MixedObjectModel *_mixedObjectModel;

protected:
public:

/*
 * Member functions
 */
private:
protected:
public:
	/**
	 * Set the array object model.
	 */
	MMINLINE void
	setArrayObjectModel(GC_ArrayObjectModel *arrayObjectModel)
	{
		_arrayObjectModel = arrayObjectModel;
	}

	/**
	 * Get the array object model.
	 */
	MMINLINE GC_ArrayObjectModel*
	getArrayObjectModel()
	{
		return _arrayObjectModel;
	}

	/**
	 * Set the array object model.
	 */
	MMINLINE void
	setMixedObjectModel(GC_MixedObjectModel *mixedObjectModel)
	{
		_mixedObjectModel = mixedObjectModel;
	}

	/**
	 * Get the array object model.
	 */
	MMINLINE GC_MixedObjectModel*
	getMixedObjectModel()
	{
		return _mixedObjectModel;
	}

	/**
	 * This method is called for each heap object during heap walks. If the received object holds
	 * an indirect object reference (ie a reference to an object that might not otherwise be walked),
	 * a pointer to the indirect object should be returned here.
	 *
	 * @param objectPtr the object to obtain indirect reference from
	 * @return a pointer to the indirect object, or NULL if none
	 */
	MMINLINE omrobjectptr_t
	getIndirectObject(omrobjectptr_t objectPtr)
	{
		J9Class *clazz = J9GC_J9OBJECT_CLAZZ(objectPtr);
		return J9VM_J9CLASS_TO_HEAPCLASS(clazz);
	}

	/**
	 * Get the byte offset from object address to object hash code slot.
	 */
	MMINLINE uintptr_t
	getHashcodeOffset(omrobjectptr_t objectPtr) {
		UDATA offset = 0;

		if (isIndexable(objectPtr)) {
			offset = _arrayObjectModel->getHashcodeOffset((J9IndexableObject *)objectPtr);
		} else {
			offset = _mixedObjectModel->getHashcodeOffset(objectPtr);
		}

		return offset;
	}

	/**
	 * Get the fomrobjectptr_t offset of the slot containing the object header.
	 */
	MMINLINE uintptr_t
	getObjectHeaderSlotOffset()
	{
		return _objectHeaderSlotOffset;
	}

	/**
	 * Get the bit offset to the flags byte in object headers.
	 */
	MMINLINE uintptr_t
	getObjectHeaderSlotFlagsShift()
	{
		return _objectHeaderSlotFlagsShift;
	}

	/**
	 * Get the exact size of the object header, in bytes.
	 */
	MMINLINE uintptr_t
	getObjectHeaderSizeInBytes(omrobjectptr_t objectPtr)
	{
		uintptr_t headerSize = 0;

		if (isIndexable(objectPtr)) {
			headerSize = _arrayObjectModel->getHeaderSize((J9IndexableObject *)objectPtr);
		} else {
			headerSize =  _mixedObjectModel->getHeaderSize(objectPtr);
		}

		return headerSize;
	}

	/**
	 * Get the exact size of the object data, in bytes. This excludes the size of the object header,
	 * hash code and any bytes added for object alignment. If the object has a discontiguous representation,
	 * this method should return the size of the root object that the discontiguous parts depend from.
	 *
	 * @param[in] objectPtr points to the object to determine size for
	 * @return the exact size of an object, in bytes, excluding padding bytes and header bytes
	 */
	MMINLINE uintptr_t
	getObjectSizeInBytesWithoutHeader(omrobjectptr_t objectPtr)
	{
		uintptr_t dataSize = 0;

		if (isIndexable(objectPtr)) {
			dataSize = _arrayObjectModel->getSizeInBytesWithoutHeader((J9IndexableObject *)objectPtr);
		} else {
			dataSize = _mixedObjectModel->getSizeInBytesWithoutHeader(objectPtr);
		}

		return dataSize;
	}

	/**
	 * Get the exact total size of the object data, in bytes. This includes the size of the object
	 * header and hash code but excludes bytes added for object alignment. If the object has a
	 * discontiguous representation, this method should return the size of the root object that the
	 * discontiguous parts depend from.
	 *
	 * @param[in] objectPtr points to the object to determine size for
	 * @return the exact size of an object, in bytes, excluding padding bytes and header bytes
	 */
	MMINLINE uintptr_t
	getObjectSizeInBytesWithHeader(omrobjectptr_t objectPtr, bool includeHashCode)
	{
		uintptr_t dataSize = 0;

		/* get header + data size */
		if (isIndexable(objectPtr)) {
			dataSize = _arrayObjectModel->getSizeInBytesWithoutHeader((J9IndexableObject *)objectPtr);
			dataSize += _arrayObjectModel->getHeaderSize((J9IndexableObject *)objectPtr);
		} else {
			dataSize = _mixedObjectModel->getSizeInBytesWithoutHeader(objectPtr);
			dataSize += _mixedObjectModel->getHeaderSize(objectPtr);
		}

		/* include space for hash code reservation */
		if (includeHashCode) {
			if (getHashcodeOffset(objectPtr) == dataSize) {
				dataSize += sizeof(uintptr_t);
			}
		}

		return dataSize;
	}

	/**
	 * Get the total footprint of an object, in bytes, including the object header and all data.
	 * If the object has a discontiguous representation, this method should return the size of
	 * the root object plus the total of all the discontiguous parts of the object.
	 *
	 * @param[in] objectPtr points to the object to determine size for
	 * @return the total size of an object, in bytes, including discontiguous parts
	 */
	MMINLINE uintptr_t
	getTotalFootprintInBytes(omrobjectptr_t objectPtr)
	{
		uintptr_t dataSize = getObjectSizeInBytesWithHeader(objectPtr);

		if (isIndexable(objectPtr)) {
			// add size of arraylet leaves (if any)
			dataSize += _arrayObjectModel->externalArrayletsSize((J9IndexableObject *)objectPtr);
		}

		return dataSize;
	}

	/**
	 * Get the exact size of the object data, in bytes. This includes the size of the object header and
	 * hash slot (if allocated) and excludes any bytes added for object alignment. If the object has a
	 * discontiguous representation, this method should return the size of the root object that the
	 * discontiguous parts depend from.
	 *
	 * @param[in] objectPtr points to the object to determine size for
	 * @return the exact size of an object, in bytes, excluding padding bytes and header bytes
	 */
	MMINLINE uintptr_t
	getObjectSizeInBytesWithHeader(omrobjectptr_t objectPtr)
	{
		uintptr_t flags = ((*((fomrobject_t *)objectPtr)) >> getObjectHeaderSlotFlagsShift()) & _delegateHeaderSlotFlagsMask;
		bool hasBeenMoved = OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS == (flags & OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS);
		return getObjectSizeInBytesWithHeader(objectPtr, hasBeenMoved);
	}

	/**
	 * If object initialization fails for any reason, this method must return NULL. In that case, the heap
	 * memory allocated for the object will become floating garbage in the heap and will be recovered in
	 * the next GC cycle.
	 *
	 * @param[in] env points to the environment for the calling thread
	 * @param[in] allocatedBytes points to the heap memory allocated for the object
	 * @param[in] allocateInitialization points to the MM_AllocateInitialization instance used to allocate the heap memory
	 * @return pointer to the initialized object, or NULL if initialization fails
	 */
	omrobjectptr_t initializeAllocation(MM_EnvironmentBase *env, void *allocatedBytes, MM_AllocateInitialization *allocateInitialization);

	/**
	 * Returns TRUE if an object is indexable, FALSE otherwise. Languages that support indexable objects
	 * (eg, arrays) must provide an implementation that distinguished indexable from scalar objects.
	 *
	 * @param objectPtr pointer to the object
	 * @return TRUE if object is indexable, FALSE otherwise
	 */
	MMINLINE bool
	isIndexable(omrobjectptr_t objectPtr)
	{
		J9Class* clazz = J9GC_J9OBJECT_CLAZZ(objectPtr);
		return J9GC_CLASS_IS_ARRAY(clazz);
	}

	/**
	 * The following methods (defined(OMR_GC_MODRON_SCAVENGER)) are required if generational GC is
 	 * configured for the build (--enable-OMR_GC_MODRON_SCAVENGER in configure_includes/configure_*.mk).
 	 * They typically involve a MM_ForwardedHeader object, and allow information about the forwarded
 	 * object to be obtained.
	 */
#if defined(OMR_GC_MODRON_SCAVENGER)
	/**
	 * Returns TRUE if the object referred to by the forwarded header is indexable.
	 *
	 * @param forwardedHeader pointer to the MM_ForwardedHeader instance encapsulating the object
	 * @return TRUE if object is indexable, FALSE otherwise
	 */
	MMINLINE bool
	isIndexable(MM_ForwardedHeader *forwardedHeader)
	{
		J9Class* clazz = (J9Class *)((uintptr_t)(forwardedHeader->getPreservedSlot()) & ~(UDATA)_delegateHeaderSlotFlagsMask);
		return J9GC_CLASS_IS_ARRAY(clazz);
	}

	/**
	 * Get the instance size (total) in bytes of a forwarded object from the forwarding pointer.
	 *
	 * @param[in] forwardedHeader pointer to the MM_ForwardedHeader instance encapsulating the object
	 * @return The instance size (total) in bytes of the forwarded object
	 */
	MMINLINE uintptr_t
	getForwardedObjectSizeInBytes(MM_ForwardedHeader *forwardedHeader)
	{
		uintptr_t size = 0;
		uintptr_t preservedSlot = (uintptr_t)(forwardedHeader->getPreservedSlot());
		J9Class* clazz = (J9Class *)(preservedSlot & ~(UDATA)_delegateHeaderSlotFlagsMask);

		if (J9GC_CLASS_IS_ARRAY(clazz)) {
#if defined (OMR_GC_COMPRESSED_POINTERS)
			uintptr_t elements = forwardedHeader->getPreservedOverlap();
#else /* defined (OMR_GC_COMPRESSED_POINTERS) */
			uintptr_t elements = ((J9IndexableObjectContiguous *)forwardedHeader->getObject())->size;
#endif /* defined (OMR_GC_COMPRESSED_POINTERS) */
#if defined(J9VM_GC_HYBRID_ARRAYLETS)
			if (0 == elements) {
				elements = ((J9IndexableObjectDiscontiguous *)forwardedHeader->getObject())->size;
			}
#endif
			uintptr_t dataSize = _arrayObjectModel->getDataSizeInBytes(clazz, elements);
			GC_ArrayletObjectModel::ArrayLayout layout = _arrayObjectModel->getArrayletLayout(clazz, dataSize);
			size = _arrayObjectModel->getSizeInBytesWithHeader(clazz, layout, elements);
		} else {
			size = _mixedObjectModel->getSizeInBytesWithoutHeader(clazz) + sizeof(J9Object);
		}

		return size;
	}

	/**
	 * Return true if the object holds references to heap objects not reachable from reference graph. For
	 * example, an object may be associated with a class and the class may have associated meta-objects
	 * that are in the heap but not directly reachable from the root set. This method is called to
	 * determine whether or not any such objects exist.
	 *
	 * @param env points to environment for the thread
	 * @param objectPtr points to an object
	 * @return true if object holds indirect references to heap objects
	 */
	MMINLINE bool
	hasIndirectObjectReferents(CLI_THREAD_TYPE *thread, omrobjectptr_t objectPtr)
	{
		return J9GC_IS_INITIALIZED_HEAPCLASS(thread, objectPtr);
	}

	/**
	 * Calculate the actual object size and the size adjusted to object alignment. The calculated object size
	 * includes any expansion bytes allocated if the object will grow when moved.
	 *
	 * @param[in] env points to the environment for the calling thread
	 * @param[in] forwardedHeader pointer to the MM_ForwardedHeader instance encapsulating the object
	 * @param[out] objectCopySizeInBytes actual object size
	 * @param[out] objectReserveSizeInBytes size adjusted to object alignment
	 * @param[out] hotFieldAlignmentDescriptor pointer to hot field alignment descriptor for class (or NULL)
	 */
	void calculateObjectDetailsForCopy(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedHeader, uintptr_t *objectCopySizeInBytes, uintptr_t *objectReserveSizeInBytes, uintptr_t *hotFieldAlignmentDescriptor);
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */

	/**
	 * Constructor receives a copy of OMR's object flags mask, normalized to low order byte. Delegate
	 * realigns it for internal use.
	 */
	GC_ObjectModelDelegate(fomrobject_t omrHeaderSlotFlagsMask)
		: _delegateHeaderSlotFlagsMask(omrHeaderSlotFlagsMask << _objectHeaderSlotFlagsShift)
	{}
};

#endif /* OBJECTMODELDELEGATE_HPP_ */
