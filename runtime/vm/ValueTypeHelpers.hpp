/*******************************************************************************
 * Copyright IBM Corp. and others 2019
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

#if !defined(VALUETYPEHELPERS_HPP_)
#define VALUETYPEHELPERS_HPP_

#include "j9cfg.h"

#if defined(OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES)
#if OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES
#define VM_ValueTypeHelpers VM_ValueTypeHelpersCompressed
#else /* OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES */
#define VM_ValueTypeHelpers VM_ValueTypeHelpersFull
#endif /* OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES */
#endif /* OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES */

#include "j9.h"
#include "fltconst.h"
#include "ut_j9vm.h"

#include "ObjectAccessBarrierAPI.hpp"
#include "VMHelpers.hpp"
#include "vm_api.h"

class VM_ValueTypeHelpers {
	/*
	 * Data members
	 */
private:
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
	struct FieldComparisonFrame {
		j9object_t lhs;
		j9object_t rhs;
		J9Class *clazz;
		UDATA offset;
	};
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */

protected:

public:

	/*
	 * Function members
	 */
private:

#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)

	/**
	 * Resize the queue to store value type fields.
	 * Allocate a new queue with double the size of previous queue.
	 * Copy active elments from old queue to new queue.
	 *
	 * @param[in] currentThread the current thread
	 * @param[in] queue pointer to the current queue
	 * @param[in, out] queueSize the current queue capacity
	 * @param[in] head pointer to index of first active element
	 * @param[in] tail pointer to one index past the last active element
	 * @return pointer to resized queue, or NULL if allocation fails
	 */
	static VMINLINE FieldComparisonFrame *
	resizeQueue(J9VMThread *currentThread,
			FieldComparisonFrame *queue,
			UDATA *queueSize,
			UDATA *head,
			UDATA *tail)
	{
		J9JavaVM *vm = currentThread->javaVM;
		PORT_ACCESS_FROM_JAVAVM(vm);

		UDATA newQueueSize = (*queueSize) * 2;
		FieldComparisonFrame *newQueue = (FieldComparisonFrame *)j9mem_allocate_memory(sizeof(FieldComparisonFrame) * newQueueSize, OMRMEM_CATEGORY_VM);

		if (NULL == newQueue) {
			return NULL;
		}

		memcpy(newQueue, queue + (*head), ((*tail) - (*head)) * sizeof(FieldComparisonFrame));
		j9mem_free_memory(queue);
		*queueSize = newQueueSize;
		*tail = (*tail) - (*head);
		*head = 0;
		return newQueue;
	}

	/**
	 * Determine if the two valueTypes are substitutable when rhs.class equals lhs.class
	 * and rhs and lhs are not null.
	 * Use breadth-first search with a queue to handle nested flattened value types.
	 *
	 * @param[in] currentThread the current thread
	 * @param[in] objectAccessBarrier the access barrier
	 * @param[in] lhs the lhs object address
	 * @param[in] rhs the rhs object address
	 * @param[in] startOffset the initial offset for the object
	 * @param[in] clazz the value type class
	 * @return 1 if values are substitutable, 0 if they differ and -1 if an error occurred
	 */
	static VMINLINE I_32
	isSubstitutable(J9VMThread *currentThread,
			MM_ObjectAccessBarrierAPI objectAccessBarrier,
			j9object_t lhs,
			j9object_t rhs,
			UDATA startOffset,
			J9Class *clazz)
	{
		J9JavaVM *vm = currentThread->javaVM;
		PORT_ACCESS_FROM_JAVAVM(vm);
		I_32 result = 1;
		U_32 walkFlags = J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE;

		UDATA queueSize = 128;
		UDATA head = 0;
		UDATA tail = 0;
		FieldComparisonFrame *queue = NULL;
		FieldComparisonFrame frame = {lhs, rhs, clazz, startOffset};

		while (true) {
			if (J9_ARE_ALL_BITS_SET(frame.clazz->classFlags, J9ClassCanSupportFastSubstitutability)) {
				bool compare = objectAccessBarrier.structuralFlattenedCompareObjects(currentThread, frame.clazz, frame.lhs, frame.rhs, frame.offset);
				if (false == compare) {
					result = 0;
					goto done;
				}
			} else {
				J9ROMFieldOffsetWalkState state;
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
				J9ROMFieldOffsetWalkResult *walkResult = fieldOffsetsStartDo(vm, frame.clazz->romClass, VM_VMHelpers::getSuperclass(frame.clazz), &state, walkFlags, frame.clazz->flattenedClassCache);
#else /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
				J9ROMFieldOffsetWalkResult *walkResult = fieldOffsetsStartDo(vm, frame.clazz->romClass, VM_VMHelpers::getSuperclass(frame.clazz), &state, walkFlags);
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
				while (NULL != walkResult->field) {
					J9UTF8 *signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(&walkResult->field->nameAndSignature);
					U_8 *sigChar = J9UTF8_DATA(signature);
					UDATA fieldOffset = frame.offset + walkResult->offset;

					switch (*sigChar) {
					case 'Z': /* boolean */
					case 'B': /* byte */
					case 'C': /* char */
					case 'I': /* int */
					case 'S': /* short */ {
						U_32 lhsValue = objectAccessBarrier.inlineMixedObjectReadU32(currentThread, frame.lhs, fieldOffset);
						U_32 rhsValue = objectAccessBarrier.inlineMixedObjectReadU32(currentThread, frame.rhs, fieldOffset);

						if (lhsValue != rhsValue) {
							result = 0;
							goto done;
						}
						break;
					}
					case 'F': /* float */ {
						U_32 lhsBits = objectAccessBarrier.inlineMixedObjectReadU32(currentThread, frame.lhs, fieldOffset);
						U_32 rhsBits = objectAccessBarrier.inlineMixedObjectReadU32(currentThread, frame.rhs, fieldOffset);

						if (0 != helperFloatCompareFloat((jfloat *)&lhsBits, (jfloat *)&rhsBits)) {
							result = 0;
							goto done;
						}
						break;
					}

					case 'J': /* long */ {
						U_64 lhsValue = objectAccessBarrier.inlineMixedObjectReadU64(currentThread, frame.lhs, fieldOffset);
						U_64 rhsValue = objectAccessBarrier.inlineMixedObjectReadU64(currentThread, frame.rhs, fieldOffset);

						if (lhsValue != rhsValue) {
							result = 0;
							goto done;
						}
						break;
					}
					case 'D': /* double */ {
						U_64 lhsBits = objectAccessBarrier.inlineMixedObjectReadU64(currentThread, frame.lhs, fieldOffset);
						U_64 rhsBits = objectAccessBarrier.inlineMixedObjectReadU64(currentThread, frame.rhs, fieldOffset);

						if (0 != helperDoubleCompareDouble((jdouble *)&lhsBits, (jdouble *)&rhsBits)) {
							result = 0;
							goto done;
						}
						break;
					}

					case '[': {
						j9object_t lhsObject = objectAccessBarrier.inlineMixedObjectReadObject(currentThread, frame.lhs, fieldOffset);
						j9object_t rhsObject = objectAccessBarrier.inlineMixedObjectReadObject(currentThread, frame.rhs, fieldOffset);

						if (lhsObject != rhsObject) {
							result = 0;
							goto done;
						}
						break;
					}

					case 'L': {
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
						bool flattened = false;
						J9Class *fieldClass = NULL;

						if (J9ROMFIELD_IS_NULL_RESTRICTED(walkResult->field)) {
							fieldClass = findJ9ClassInFlattenedClassCache(frame.clazz->flattenedClassCache, sigChar + 1, J9UTF8_LENGTH(signature) - 2);
							flattened = J9_IS_FIELD_FLATTENED(fieldClass, walkResult->field);
						}
						if (flattened) {
							if (NULL == queue) {
								queue = (FieldComparisonFrame *)j9mem_allocate_memory(sizeof(FieldComparisonFrame) * queueSize, OMRMEM_CATEGORY_VM);
								if (NULL == queue) {
									result = -1;
									goto done;
								}
							} else {
								Assert_VM_true(tail <= queueSize);
								if (tail == queueSize) {
									FieldComparisonFrame *newQueue = resizeQueue(currentThread, queue, &queueSize, &head, &tail);
									if (NULL == newQueue) {
										result = -1;
										goto done;
									}
									queue = newQueue;
								}
							}
							queue[tail].lhs = frame.lhs;
							queue[tail].rhs = frame.rhs;
							queue[tail].clazz = fieldClass;
							queue[tail].offset = fieldOffset;
							tail++;
						} else
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
						{
							j9object_t lhsFieldObject = objectAccessBarrier.inlineMixedObjectReadObject(currentThread, frame.lhs, fieldOffset);
							j9object_t rhsFieldObject = objectAccessBarrier.inlineMixedObjectReadObject(currentThread, frame.rhs, fieldOffset);

							if (lhsFieldObject != rhsFieldObject) {
								if ((NULL != lhsFieldObject) && (NULL != rhsFieldObject)) {
									J9Class *lhsFieldClass = J9OBJECT_CLAZZ(currentThread, lhsFieldObject);
									J9Class *rhsFieldClass = J9OBJECT_CLAZZ(currentThread, rhsFieldObject);
									if (J9_IS_J9CLASS_VALUETYPE(lhsFieldClass) && (lhsFieldClass == rhsFieldClass)) {
										if (NULL == queue) {
											queue = (FieldComparisonFrame *)j9mem_allocate_memory(sizeof(FieldComparisonFrame) * queueSize, OMRMEM_CATEGORY_VM);
											if (NULL == queue) {
												result = -1;
												goto done;
											}
										} else {
											Assert_VM_true(tail <= queueSize);
											if (tail == queueSize) {
												FieldComparisonFrame *newQueue = resizeQueue(currentThread, queue, &queueSize, &head, &tail);
												if (NULL == newQueue) {
													result = -1;
													goto done;
												}
												queue = newQueue;
											}
										}
										queue[tail].lhs = lhsFieldObject;
										queue[tail].rhs = rhsFieldObject;
										queue[tail].clazz = lhsFieldClass;
										queue[tail].offset = J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
										tail++;
									} else {
										result = 0;
										goto done;
									}
								} else {
									result = 0;
									goto done;
								}
							}
						}
						break;
					}
					default:
						Assert_VM_unreachable();
						break;
					}
					walkResult = fieldOffsetsNextDo(&state);
				}
			}
			if ((NULL != queue) && (head < tail)) {
				frame = queue[head++];
			} else {
				goto done;
			}
		}
done:
		j9mem_free_memory(queue);
		return result;
	}
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */

protected:

public:
	static VMINLINE I_32
	acmp(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI objectAccessBarrier, j9object_t lhs, j9object_t rhs)
	{
		I_32 acmpResult = (rhs == lhs);
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
		if (0 == acmpResult) {
			if ((NULL != rhs) && (NULL != lhs)) {
				J9Class * lhsClass = J9OBJECT_CLAZZ(currentThread, lhs);
				J9Class * rhsClass = J9OBJECT_CLAZZ(currentThread, rhs);
				if (J9_IS_J9CLASS_VALUETYPE(lhsClass)
					&& (rhsClass == lhsClass)
				) {
					acmpResult = isSubstitutable(currentThread, objectAccessBarrier, lhs, rhs, J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread), lhsClass);
				}
			}
		}
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */
		return acmpResult;
	}

	/**
	 * Determines if null-restricted attribute is set on a field or not.
	 *
	 * @param[in] field The field to be checked
	 *
	 * @return TRUE if the field has null-restricted attribute set, FALSE otherwise
	 */
	static VMINLINE bool
	isFieldNullRestricted(J9ROMFieldShape *field)
	{
		Assert_VM_notNull(field);
		return J9_ARE_ALL_BITS_SET(field->modifiers, J9FieldFlagIsNullRestricted);
	}

	/**
	 * Performs a getfield operation on an object. Handles flattened and non-flattened cases.
	 * This helper assumes that the cpIndex points to a resolved null-restricted fieldRef. This helper
	 * also assumes that the cpIndex points to an instance field.
	 *
	 * @param currentThread thread token
	 * @param objectAccessBarrier access barrier
	 * @param objectAllocate allocator
	 * @param cpEntry the RAM cpEntry for the field, needs to be resolved
	 * @param receiver receiver object
	 * @param fastPath performs fastpath allocation, no GC. If this is false
	 * 			frame must be built before calling as GC may occur
	 *
	 * @return NULL if allocation fails, valuetype otherwise
	 */
	static VMINLINE j9object_t
	getFlattenableField(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI objectAccessBarrier, MM_ObjectAllocationAPI objectAllocate, J9RAMFieldRef *cpEntry, j9object_t receiver, bool fastPath)
	{
		UDATA const objectHeaderSize = J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
		UDATA const flags = cpEntry->flags;
		j9object_t returnObjectRef = NULL;

#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
		if (flags & J9FieldFlagFlattened) {
			J9FlattenedClassCacheEntry *cache = (J9FlattenedClassCacheEntry *) cpEntry->valueOffset;
			J9Class *flattenedFieldClass = J9_VM_FCC_CLASS_FROM_ENTRY(cache);

			returnObjectRef = getFlattenedFieldAtOffset(
				currentThread,
				objectAccessBarrier,
				objectAllocate,
				flattenedFieldClass,
				receiver,
				cache->offset + J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread),
				fastPath);
		} else
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
		{
			bool isVolatile = (0 != (flags & J9AccVolatile));
			returnObjectRef = objectAccessBarrier.inlineMixedObjectReadObject(currentThread, receiver, cpEntry->valueOffset + objectHeaderSize, isVolatile);
		}
		return returnObjectRef;
	}

#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
	/**
	 * Performs a getfield operation on an object given an offset in bytes. Only Handles flattened cases.
	 *
	 * @param currentThread thread token
	 * @param objectAccessBarrier access barrier
	 * @param objectAllocate allocator
	 * @param returnObjectClass the class of the field being retrieved
	 * @param srcObject the object that the field is being retrieved from
	 * @param srcOffset where in srcObject the field is located (in bytes)
	 * @param fastPath performs fastpath allocation, no GC. If this is false
	 * 			frame must be built before calling as GC may occur
	 *
	 * @return NULL if allocation fails, valuetype otherwise
	 */
	static VMINLINE j9object_t
	getFlattenedFieldAtOffset(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI objectAccessBarrier, MM_ObjectAllocationAPI objectAllocate, J9Class *returnObjectClass, j9object_t srcObject, UDATA srcOffset, bool fastPath)
	{
		j9object_t returnObjectRef = NULL;

		if (fastPath) {
			returnObjectRef = objectAllocate.inlineAllocateObject(currentThread, returnObjectClass, false, false);
		} else {
			VM_VMHelpers::pushObjectInSpecialFrame(currentThread, srcObject);
			returnObjectRef = currentThread->javaVM->memoryManagerFunctions->J9AllocateObject(
				currentThread, returnObjectClass, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
			srcObject = VM_VMHelpers::popObjectInSpecialFrame(currentThread);
			returnObjectClass = VM_VMHelpers::currentClass(returnObjectClass);
		}

		if (NULL != returnObjectRef) {
			UDATA returnObjectOffset = 0;
			if (J9CLASS_HAS_4BYTE_PREPADDING(returnObjectClass)) {
				returnObjectOffset += sizeof(U_32);
			}

			objectAccessBarrier.copyObjectFields(
				currentThread,
				returnObjectClass,
				srcObject,
				srcOffset,
				returnObjectRef,
				returnObjectOffset + J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread));
		}

		return returnObjectRef;
	}

	/**
	 * Stores a valuetype at a specified offset in a specified object. Only Handles flattened cases.
	 *
	 * @param currentThread thread token
	 * @param objectAccessBarrier access barrier
	 * @param destObjectClass the class of srcObject
	 * @param srcObject the object being stored
	 * @param destObject the object that srcObject is being stored in
	 * @param destOffset where in destObject to store srcObject (in bytes)
	 */
	static VMINLINE void
	putFlattenedFieldAtOffset(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI objectAccessBarrier, J9Class *destObjectClass, j9object_t srcObject, j9object_t destObject, UDATA destOffset)
	{
		UDATA sourceObjectOffset = 0;
		if (J9CLASS_HAS_4BYTE_PREPADDING(destObjectClass)) {
			sourceObjectOffset += sizeof(U_32);
		}

		objectAccessBarrier.copyObjectFields(
			currentThread,
			destObjectClass,
			srcObject,
			sourceObjectOffset + J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread),
			destObject,
			destOffset);
	}
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */

	/**
	 * Performs a clone operation on an object.
	 *
	 * @param currentThread thread token
	 * @param objectAccessBarrier access barrier
	 * @param objectAllocate allocator
	 * @param receiverClass j9class of original object
	 * @param original object to be cloned
	 * @param fastPath performs fastpath allocation, no GC. If this is false
	 * 			frame must be built before calling as GC may occur
	 *
	 * @return NULL if allocation fails, valuetype otherwise
	 */
	static VMINLINE j9object_t
	cloneValueType(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI objectAccessBarrier, MM_ObjectAllocationAPI objectAllocate, J9Class *receiverClass, j9object_t original, bool fastPath)
	{
		j9object_t clone = NULL;
		J9JavaVM *vm = currentThread->javaVM;

		/* need to zero memset the memory so padding bytes are zeroed for memcmp-like comparisons */
		if (fastPath) {
			clone = objectAllocate.inlineAllocateObject(currentThread, receiverClass, true, false);
			if (NULL == clone) {
				goto done;
			}
		} else {
			VM_VMHelpers::pushObjectInSpecialFrame(currentThread, original);
			clone = vm->memoryManagerFunctions->J9AllocateObject(currentThread, receiverClass, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
			original = VM_VMHelpers::popObjectInSpecialFrame(currentThread);
			if (J9_UNEXPECTED(NULL == clone)) {
				goto done;
			}
			receiverClass = VM_VMHelpers::currentClass(receiverClass);
		}

		objectAccessBarrier.cloneObject(currentThread, original, clone, receiverClass);

done:
		return clone;
	}

	/**
	 * Performs a putfield operation on an object. Handles flattened and non-flattened cases.
	 * This helper assumes that the cpIndex points to a resolved null-restricted fieldRef. This helper
	 * also assumes that the cpIndex points to an instance field.
	 *
	 * @param currentThread thread token
	 * @param objectAccessBarrier access barrier
	 * @param cpEntry the RAM cpEntry for the field, needs to be resolved
	 * @param receiver receiver object
	 * @param paramObject parameter object
	 */
	static VMINLINE void
	putFlattenableField(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI objectAccessBarrier, J9RAMFieldRef *cpEntry, j9object_t receiver, j9object_t paramObject)
	{
		UDATA const objectHeaderSize = J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
		UDATA const flags = cpEntry->flags;

#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
		if (J9_ARE_ALL_BITS_SET(flags, J9FieldFlagFlattened)) {
			J9FlattenedClassCacheEntry *cache = (J9FlattenedClassCacheEntry *) cpEntry->valueOffset;
			UDATA fromObjectOffset = 0;
			J9Class *clazz = J9_VM_FCC_CLASS_FROM_ENTRY(cache);

			if (J9CLASS_HAS_4BYTE_PREPADDING(clazz)) {
				fromObjectOffset += sizeof(U_32);
			}

			objectAccessBarrier.copyObjectFields(currentThread,
								clazz,
								paramObject,
								objectHeaderSize + fromObjectOffset,
								receiver,
								cache->offset + objectHeaderSize);
		} else
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
		{
			bool isVolatile = (0 != (flags & J9AccVolatile));
			objectAccessBarrier.inlineMixedObjectStoreObject(currentThread, receiver, cpEntry->valueOffset + objectHeaderSize, paramObject, isVolatile);
		}
	}

	/**
	 * Performs an aastore operation on an object. Handles flattened and non-flattened cases.
	 *
	 * Assumes recieverObject and paramObject are not null.
	 * All AIOB exceptions must be thrown before calling.
	 *
	 * @param[in] currentThread thread token
	 * @param[in] _objectAccessBarrier access barrier
	 * @param[in] receiverObject arrayObject
	 * @param[in] index array index
	 * @param[in] paramObject obj arg
	 */
	static VMINLINE void
	storeFlattenableArrayElement(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI _objectAccessBarrier, j9object_t receiverObject, U_32 index, j9object_t paramObject)
	{
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
		J9ArrayClass *arrayrefClass = (J9ArrayClass *) J9OBJECT_CLAZZ(currentThread, receiverObject);
		if (J9_IS_J9CLASS_FLATTENED(arrayrefClass)) {
			_objectAccessBarrier.copyObjectFieldsToFlattenedArrayElement(currentThread, arrayrefClass, paramObject, (J9IndexableObject *) receiverObject, index);
		} else
#endif /* if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
		{
			_objectAccessBarrier.inlineIndexableObjectStoreObject(currentThread, receiverObject, index, paramObject);
		}
	}

	/**
	 * Performs an aaload operation on an object. Handles flattened and non-flattened cases.
	 * This function could call into J9AllocateObject() which might trigger GC in the slow path, so receiverObject might be moved by GC.
	 * If the caller caches receiverObject and uses it after calling this function in the slow path, it needs to re-read receiverObject.
	 *
	 * Assumes recieverObject is not null.
	 * All AIOB exceptions must be thrown before calling.
	 *
	 * Returns null if newObjectRef retrieval fails.
	 *
	 * @param[in] currentThread thread token
	 * @param[in] _objectAccessBarrier access barrier
	 * @param[in] _objectAllocate allocator
	 * @param[in] receiverObject arrayobject
	 * @param[in] index array index
	 * @param[in] fast Fast path if true. Slow path if false.
	 *
	 * @return array element
	 */
	static VMINLINE j9object_t
	loadFlattenableArrayElement(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI _objectAccessBarrier, MM_ObjectAllocationAPI _objectAllocate, j9object_t receiverObject, U_32 index, bool fast)
	{
		j9object_t value = NULL;
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
		j9object_t newObjectRef = NULL;
		J9Class *arrayrefClass = J9OBJECT_CLAZZ(currentThread, receiverObject);
		if (J9_IS_J9CLASS_FLATTENED(arrayrefClass)) {
			if (fast) {
				newObjectRef = _objectAllocate.inlineAllocateObject(currentThread, ((J9ArrayClass*)arrayrefClass)->leafComponentType, false, false);
				if(J9_UNEXPECTED(NULL == newObjectRef)) {
					goto done;
				}
			} else {
				VM_VMHelpers::pushObjectInSpecialFrame(currentThread, receiverObject);
				newObjectRef = currentThread->javaVM->memoryManagerFunctions->J9AllocateObject(currentThread, ((J9ArrayClass*)arrayrefClass)->leafComponentType, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
				receiverObject = VM_VMHelpers::popObjectInSpecialFrame(currentThread);
				if (J9_UNEXPECTED(NULL == newObjectRef)) {
					goto done;
				}
				arrayrefClass = VM_VMHelpers::currentClass(arrayrefClass);
			}
			_objectAccessBarrier.copyObjectFieldsFromFlattenedArrayElement(currentThread, (J9ArrayClass *) arrayrefClass, newObjectRef, (J9IndexableObject *) receiverObject, index);
			value = newObjectRef;
		} else
#endif /* if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
		{
			value = _objectAccessBarrier.inlineIndexableObjectReadObject(currentThread, receiverObject, index);
		}
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
done:
#endif /* if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
		return value;
	}

#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
	/**
	 * Copies an array of non-primitive objects
	 * Handles flattened and non-flattened cases
	 * A generic special stack frame must be built before calling this function
	 *
	 * Assumes srcObject and destObject are not null
	 * Assumes array bounds (srcIndex to (srcIndex + lengthInSlots), and destIndex to (destIndex + lengthInSlots)) are valid
	 * Assumes a generic special stack frame has been built on the stack
	 *
	 * @param[in] currentThread thread token
	 * @param[in] objectAccessBarrier access barrier
	 * @param[in] objectAllocate allocator
	 * @param[in] srcObject the source array to copy objects from
	 * @param[out] destObject the destination array in which objects should be stored
	 * @param[in] srcIndex the index in the source array to begin copying objects from
	 * @param[in] destIndex the index in the destination array to begin storing objects
	 * @param[in] lengthInSlots the number of elements to copy
	 *
	 * @return 0 if copy was successful, -1 if there was an array store error, -2 if there was a null pointer exception
	 */
	static VMINLINE I_32
	copyFlattenableArray(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI objectAccessBarrier, MM_ObjectAllocationAPI objectAllocate, j9object_t srcObject, j9object_t destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots)
	{
		I_32 srcEndIndex = srcIndex + lengthInSlots;
		J9Class *srcClazz = J9OBJECT_CLAZZ(currentThread, srcObject);
		J9Class *destClazz = J9OBJECT_CLAZZ(currentThread, destObject);

		/* Array elements must be copied backwards if source and destination overlap in memory and source is before destination */
		if ((srcObject == destObject) && (srcIndex < destIndex) && ((srcIndex + lengthInSlots) > destIndex)) {
			srcEndIndex = srcIndex;
			srcIndex += lengthInSlots;
			destIndex += lengthInSlots;

			while (srcIndex > srcEndIndex) {
				srcIndex--;
				destIndex--;

				j9object_t copyObject = loadFlattenableArrayElement(currentThread, objectAccessBarrier, objectAllocate, srcObject, srcIndex, true);

				/*
				When the return value of loadFlattenableArrayElement is NULL, 2 things are possible:
					1: The array element at that index is actually NULL
					2: There was an allocation failure
				But loadFlattenableArrayElement only tries to allocate when srcClazz is flattened, and so if copyObject is NULL and srcClazz is flattened then there has been an allocation failure
				*/
				if ((NULL == copyObject) && J9_IS_J9CLASS_FLATTENED(srcClazz)) {
					VM_VMHelpers::pushObjectInSpecialFrame(currentThread, srcObject);
					VM_VMHelpers::pushObjectInSpecialFrame(currentThread, destObject);
					copyObject = loadFlattenableArrayElement(currentThread, objectAccessBarrier, objectAllocate, srcObject, srcIndex, false);
					destObject = VM_VMHelpers::popObjectInSpecialFrame(currentThread);
					srcObject = VM_VMHelpers::popObjectInSpecialFrame(currentThread);
				}

				/* No type checks required since srcObject == destObject */

				storeFlattenableArrayElement(currentThread, objectAccessBarrier, destObject, destIndex, copyObject);
			}
		} else {

			UDATA typeChecksRequired = !isSameOrSuperClassOf(destClazz, srcClazz);

			while (srcIndex < srcEndIndex) {
				j9object_t copyObject = loadFlattenableArrayElement(currentThread, objectAccessBarrier, objectAllocate, srcObject, srcIndex, true);

				/*
				When the return value of loadFlattenableArrayElement is NULL, 2 things are possible:
					1: The array element at that index is actually NULL
					2: There was an allocation failure
				But loadFlattenableArrayElement only tries to allocate when srcClazz is flattened, and so if copyObject is NULL and srcClazz is flattened then there has been an allocation failure
				*/
				if ((NULL == copyObject) && J9_IS_J9CLASS_FLATTENED(srcClazz)) {
					VM_VMHelpers::pushObjectInSpecialFrame(currentThread, srcObject);
					VM_VMHelpers::pushObjectInSpecialFrame(currentThread, destObject);
					copyObject = loadFlattenableArrayElement(currentThread, objectAccessBarrier, objectAllocate, srcObject, srcIndex, false);
					destObject = VM_VMHelpers::popObjectInSpecialFrame(currentThread);
					srcObject = VM_VMHelpers::popObjectInSpecialFrame(currentThread);
				}

				if (typeChecksRequired) {
					if (!VM_VMHelpers::objectArrayStoreAllowed(currentThread, destObject, copyObject)) {
						return -1;
					}
				}

				storeFlattenableArrayElement(currentThread, objectAccessBarrier, destObject, destIndex, copyObject);

				srcIndex++;
				destIndex++;
			}
		}

		return 0;
	}
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */

#if defined(J9VM_OPT_VALHALLA_STRICT_FIELDS)
	/**
	 * Finds the J9FlattenedClassCacheEntry in the ramClass corresponding
	 * to an offset. Asserts that an entry exists to a strict field.
	 * @param ramClass class with a flattened class cache to search for field entry
	 * @param matchOffset static field offset
	 * @return J9FlattenedClassCacheEntry corresponding to matchOffset, or
	 * null if no match exists
	 */
	static VMINLINE J9FlattenedClassCacheEntry *
	findJ9FlattenedClassCacheEntryForStaticAddress(J9Class *ramClass, UDATA matchOffset)
	{
		J9FlattenedClassCacheEntry *entry = NULL;
		UDATA numberOfEntries = ramClass->flattenedClassCache->numberOfEntries;
		for (UDATA i = 0; i < numberOfEntries; i++) {
			J9FlattenedClassCacheEntry *tempEntry = J9_VM_FCC_ENTRY_FROM_CLASS(ramClass, i);
			if (J9_VM_FCC_ENTRY_IS_STATIC_FIELD(tempEntry)
				&& (tempEntry->offset == matchOffset)
			) {
				Assert_VM_true(J9ROMFIELD_IS_STRICT(tempEntry->field->modifiers));
				entry = tempEntry;
				break;
			}
		}
		return entry;
	}
#endif /* defined(J9VM_OPT_VALHALLA_STRICT_FIELDS) */
};

#endif /* VALUETYPEHELPERS_HPP_ */
