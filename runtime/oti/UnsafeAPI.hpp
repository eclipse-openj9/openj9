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

#if !defined(UNSAFEAPI_HPP_)
#define UNSAFEAPI_HPP_

#include "j9cfg.h"

#if defined(OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES)
#if OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES
#define VM_UnsafeAPI VM_UnsafeAPICompressed
#else /* OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES */
#define VM_UnsafeAPI VM_UnsafeAPIFull
#endif /* OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES */
#endif /* OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES */

#include "j9.h"
#include "ArrayCopyHelpers.hpp"
#include "ObjectAccessBarrierAPI.hpp"
#include "VMHelpers.hpp"
#include "AtomicSupport.hpp"

#if defined(J9VM_INTERP_USE_UNSAFE_HELPER)
extern "C" {
void unsafePut64(I_64 *address, I_64 value);
void unsafePut32(I_32 *address, I_32 value);
void unsafePut16(I_16 *address, I_16 value);
void unsafePut8(I_8 *address, I_8 value);

I_64 unsafeGet64(I_64 *address);
I_32 unsafeGet32(I_32 *address);
I_16 unsafeGet16(I_16 *address);
I_8 unsafeGet8(I_8 *address);
}
#define UNSAFE_PUT64(address, value) unsafePut64((I_64 *)(address), (value))
#define UNSAFE_PUT32(address, value) unsafePut32((I_32 *)(address), (value))
#define UNSAFE_PUT16(address, value) unsafePut16((I_16 *)(address), (value))
#define UNSAFE_PUT8(address, value) unsafePut8((I_8 *)(address), (value))

#define UNSAFE_GET64(address) (unsafeGet64((I_64 *)(address)))
#define UNSAFE_GET32(address) (unsafeGet32((I_32 *)(address)))
#define UNSAFE_GET16(address) (unsafeGet16((I_16 *)(address)))
#define UNSAFE_GET8(address) (unsafeGet8((I_8 *)(address)))

#else /* J9VM_INTERP_USE_UNSAFE_HELPER */
#define UNSAFE_PUT64(address, value) (*(I_64 *)(address) = (value))
#define UNSAFE_PUT32(address, value) (*(I_32 *)(address) = (value))
#define UNSAFE_PUT16(address, value) (*(I_16 *)(address) = (value))
#define UNSAFE_PUT8(address, value) (*(I_8 *)(address) = (value))

#define UNSAFE_GET64(address) (*(I_64 *)(address))
#define UNSAFE_GET32(address) (*(I_32 *)(address))
#define UNSAFE_GET16(address) (*(I_16 *)(address))
#define UNSAFE_GET8(address) (*(I_8 *)(address))
#endif /* J9VM_INTERP_USE_UNSAFE_HELPER */

class VM_UnsafeAPI
{
/*
 * Data members
 */
private:

protected:

public:

/*
 * Function members
 */
private:

	static VMINLINE UDATA logFJ9ObjectSize(J9VMThread *currentThread) {
		return (4 == J9VMTHREAD_REFERENCE_SIZE(currentThread)) ? 2 : 3;
	}

	static VMINLINE bool
	offsetIsAlignedArrayIndex(J9VMThread *currentThread, U_64 offset, UDATA logElementSize)
	{
		return 0 == (((UDATA)offset - arrayBase(currentThread)) % ((UDATA)1 << logElementSize));
	}

	static VMINLINE UDATA
	convertOffsetToIndex(J9VMThread *currentThread, U_64 offset, UDATA logElementSize)
	{
		return (UDATA)(((UDATA)offset - arrayBase(currentThread)) >> logElementSize);
	}

	static VMINLINE I_32
	get32OrLess(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, UDATA offset, bool isVolatile, UDATA logElementSize, bool isSigned)
	{
		I_32 value = 0;
		if (NULL == object) {
			/* Direct memory access */
			switch (logElementSize) {
			case 0:
				if (isSigned) {
					value = UNSAFE_GET8(offset);
				} else {
					value = (U_8)UNSAFE_GET8(offset);
				}
				break;
			case 1:
				if (isSigned) {
					value = UNSAFE_GET16(offset);
				} else {
					value = (U_16)UNSAFE_GET16(offset);
				}
				break;
			default:
				if (isSigned) {
					value = UNSAFE_GET32(offset);
				} else {
					value = (U_32)UNSAFE_GET32(offset);
				}
				break;
			}
		} else {
			if (VM_VMHelpers::objectIsArray(currentThread, object)) {
				/* Array access */
				if (offsetIsAlignedArrayIndex(currentThread, offset, logElementSize)) {
					/* Aligned array access */
					UDATA index = convertOffsetToIndex(currentThread, offset, logElementSize);
					switch (logElementSize) {
					case 0:
						if (isSigned) {
							value = objectAccessBarrier->inlineIndexableObjectReadI8(currentThread, object, index, isVolatile);
						} else {
							value = objectAccessBarrier->inlineIndexableObjectReadU8(currentThread, object, index, isVolatile);
						}
						break;
					case 1:
						if (isSigned) {
							value = objectAccessBarrier->inlineIndexableObjectReadI16(currentThread, object, index, isVolatile);
						} else {
							value = objectAccessBarrier->inlineIndexableObjectReadU16(currentThread, object, index, isVolatile);
						}
						break;
					default:
						if (isSigned) {
							value = objectAccessBarrier->inlineIndexableObjectReadI32(currentThread, object, index, isVolatile);
						} else {
							value = objectAccessBarrier->inlineIndexableObjectReadU32(currentThread, object, index, isVolatile);
						}
						break;
					}
				} else {
					/* Unaligned array access - unreachable for logElementSize == 0 */
					UDATA index = convertOffsetToIndex(currentThread, offset, 0);
					if (1 == logElementSize) {
						I_16 temp = 0;
						VM_ArrayCopyHelpers::memcpyFromArray(currentThread, object, (UDATA)0, index, (I_32)sizeof(temp), (void*)&temp);
						value = temp;
					} else {
						VM_ArrayCopyHelpers::memcpyFromArray(currentThread, object, (UDATA)0, index, (I_32)sizeof(value), (void*)&value);
					}
				}
			} else if (offset & J9_SUN_STATIC_FIELD_OFFSET_TAG) {
				/* Static field */
				J9Class *fieldClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, object);
				void *valueAddress = (void*)((UDATA)fieldClass->ramStatics + (offset & ~(UDATA)J9_SUN_FIELD_OFFSET_MASK));
				{
					value = (I_32)objectAccessBarrier->inlineStaticReadU32(currentThread, fieldClass, (U_32*)valueAddress, isVolatile);
				}
			} else {
				/* Instance field */
#if defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS)
				switch (logElementSize) {
				case 0:
					value = objectAccessBarrier->inlineMixedObjectReadI8(currentThread, object, offset, isVolatile);
					break;
				case 1:
					value = objectAccessBarrier->inlineMixedObjectReadI16(currentThread, object, offset, isVolatile);
					break;
				default:
					value = objectAccessBarrier->inlineMixedObjectReadI32(currentThread, object, offset, isVolatile);
					break;
				}
#else /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */
				value = objectAccessBarrier->inlineMixedObjectReadI32(currentThread, object, offset, isVolatile);
#endif /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */
			}
		}
		return value;
	}

	static VMINLINE void
	put32OrLess(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, U_64 offset, bool isVolatile, UDATA logElementSize, bool isSigned, I_32 value)
	{
		if (NULL == object) {
			/* Direct memory access */
			switch (logElementSize) {
			case 0:
				UNSAFE_PUT8(offset, (I_8)value);
				break;
			case 1:
				UNSAFE_PUT16(offset, (I_16)value);
				break;
			default:
				UNSAFE_PUT32(offset, value);
				break;
			}
		} else {
			if (VM_VMHelpers::objectIsArray(currentThread, object)) {
				/* Array access */
				if (offsetIsAlignedArrayIndex(currentThread, offset, logElementSize)) {
					/* Aligned array access */
					UDATA index = convertOffsetToIndex(currentThread, offset, logElementSize);
					switch (logElementSize) {
					case 0:
						if (isSigned) {
							objectAccessBarrier->inlineIndexableObjectStoreI8(currentThread, object, index, (I_8)value, isVolatile);
						} else {
							objectAccessBarrier->inlineIndexableObjectStoreU8(currentThread, object, index, (U_8)value, isVolatile);
						}
						break;
					case 1:
						if (isSigned) {
							objectAccessBarrier->inlineIndexableObjectStoreI16(currentThread, object, index, (I_16)value, isVolatile);
						} else {
							objectAccessBarrier->inlineIndexableObjectStoreU16(currentThread, object, index, (U_16)value, isVolatile);
						}
						break;
					default:
						if (isSigned) {
							objectAccessBarrier->inlineIndexableObjectStoreI32(currentThread, object, index, (I_32)value, isVolatile);
						} else {
							objectAccessBarrier->inlineIndexableObjectStoreU32(currentThread, object, index, (U_32)value, isVolatile);
						}
						break;
					}
				} else {
					/* Unaligned array access - unreachable for logElementSize == 0 */
					UDATA index = convertOffsetToIndex(currentThread, offset, 0);
					if (1 == logElementSize) {
						I_16 temp = (I_16)value;
						VM_ArrayCopyHelpers::memcpyToArray(currentThread, object, (UDATA)0, index, (I_32)sizeof(temp), (void*)&temp);
					} else {
						VM_ArrayCopyHelpers::memcpyToArray(currentThread, object, (UDATA)0, index, (I_32)sizeof(value), (void*)&value);
					}
				}
			} else if (offset & J9_SUN_STATIC_FIELD_OFFSET_TAG) {
				/* Static field */
				J9Class *fieldClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, object);
				if (J9_ARE_ANY_BITS_SET(offset, J9_SUN_FINAL_FIELD_OFFSET_TAG)) {
					VM_VMHelpers::reportFinalFieldModified(currentThread, fieldClass);
				}
				void *valueAddress = (void*)((UDATA)fieldClass->ramStatics + (offset & ~(UDATA)J9_SUN_FIELD_OFFSET_MASK));
				objectAccessBarrier->inlineStaticStoreU32(currentThread, fieldClass, (U_32*)valueAddress, (U_32)value, isVolatile);
			} else {
				/* Instance field */
#if defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS)
				switch (logElementSize) {
				case 0:
					objectAccessBarrier->inlineMixedObjectStoreI8(currentThread, object, (UDATA)offset, (I_8)value, isVolatile);
					break;
				case 1:
					objectAccessBarrier->inlineMixedObjectStoreI16(currentThread, object, (UDATA)offset, (I_16)value, isVolatile);
					break;
				default:
					objectAccessBarrier->inlineMixedObjectStoreI32(currentThread, object, (UDATA)offset, (I_32)value, isVolatile);
					break;
				}
#else /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */
				objectAccessBarrier->inlineMixedObjectStoreI32(currentThread, object, (UDATA)offset, (I_32)value, isVolatile);
#endif /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */
			}
		}
	}

	static VMINLINE I_64
	get64(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, UDATA offset, bool isVolatile)
	{
		UDATA logElementSize = 3;
		I_64 value = 0;
		if (NULL == object) {
			/* Direct memory access */
			value = UNSAFE_GET64(offset);
		} else {
			if (VM_VMHelpers::objectIsArray(currentThread, object)) {
				/* Array access */
				if (offsetIsAlignedArrayIndex(currentThread, offset, logElementSize)) {
					/* Aligned array access */
					UDATA index = convertOffsetToIndex(currentThread, offset, logElementSize);
					value = objectAccessBarrier->inlineIndexableObjectReadI64(currentThread, object, index, isVolatile);
				} else {
					/* Unaligned array access */
					UDATA index = convertOffsetToIndex(currentThread, offset, 0);
					VM_ArrayCopyHelpers::memcpyFromArray(currentThread, object, (UDATA)0, index, (I_32)sizeof(value), (void*)&value);				}
			} else if (offset & J9_SUN_STATIC_FIELD_OFFSET_TAG) {
				/* Static field */
				J9Class *fieldClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, object);
				void *valueAddress = (void*)((UDATA)fieldClass->ramStatics + (offset & ~(UDATA)J9_SUN_FIELD_OFFSET_MASK));
				{
					value = objectAccessBarrier->inlineStaticReadU64(currentThread, fieldClass, (U_64*)valueAddress, isVolatile);
				}
			} else {
				/* Instance field */
				value = objectAccessBarrier->inlineMixedObjectReadI64(currentThread, object, offset, isVolatile);
			}
		}
		return value;
	}

	static VMINLINE void
	put64(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, U_64 offset, bool isVolatile, I_64 value)
	{
		UDATA logElementSize = 3;
		if (NULL == object) {
			/* Direct memory access */
			UNSAFE_PUT64(offset, value);
		} else {
			if (VM_VMHelpers::objectIsArray(currentThread, object)) {
				/* Array access */
				if (offsetIsAlignedArrayIndex(currentThread, offset, logElementSize)) {
					/* Aligned array access */
					UDATA index = convertOffsetToIndex(currentThread, offset, logElementSize);
					objectAccessBarrier->inlineIndexableObjectStoreI64(currentThread, object, index, value, isVolatile);
				} else {
					/* Unaligned array access */
					UDATA index = convertOffsetToIndex(currentThread, offset, 0);
					VM_ArrayCopyHelpers::memcpyToArray(currentThread, object, (UDATA)0, index, (I_32)sizeof(value), (void*)&value);
				}
			} else if (offset & J9_SUN_STATIC_FIELD_OFFSET_TAG) {
				/* Static field */
				J9Class *fieldClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, object);
				if (J9_ARE_ANY_BITS_SET(offset, J9_SUN_FINAL_FIELD_OFFSET_TAG)) {
					VM_VMHelpers::reportFinalFieldModified(currentThread, fieldClass);
				}
				void *valueAddress = (void*)((UDATA)fieldClass->ramStatics + (offset & ~(UDATA)J9_SUN_FIELD_OFFSET_MASK));
				objectAccessBarrier->inlineStaticStoreU64(currentThread, fieldClass, (U_64*)valueAddress, (U_64)value, isVolatile);
			} else {
				/* Instance field */
				objectAccessBarrier->inlineMixedObjectStoreI64(currentThread, object, (UDATA)offset, value, isVolatile);
			}
		}
	}

protected:

public:
	static VMINLINE UDATA arrayBase(J9VMThread *currentThread) {
		return J9VMTHREAD_UNSAFE_INDEXABLE_HEADER_SIZE(currentThread);
	}

	static VMINLINE void loadFence()
	{
		VM_AtomicSupport::readBarrier();
	}

	static VMINLINE void storeFence()
	{
		VM_AtomicSupport::writeBarrier();
	}

	static VMINLINE void fullFence()
	{
		VM_AtomicSupport::readWriteBarrier();
	}

	static VMINLINE I_32 arrayBaseOffset(J9VMThread *currentThread, J9ArrayClass *arrayClass)
	{
		return (I_32)arrayBase(currentThread);
	}

	static VMINLINE I_32 arrayIndexScale(J9ArrayClass *arrayClass)
	{
		return (I_32)J9ARRAYCLASS_GET_STRIDE((J9Class*)arrayClass);
	}

	static VMINLINE I_32 addressSize()
	{
		return sizeof(UDATA);
	}

	static VMINLINE I_64 getAddress(I_64 address)
	{
#if defined(J9VM_ENV_DATA64)
		return (I_64)UNSAFE_GET64(address);
#else
		return (I_64)((U_32)UNSAFE_GET32(address));
#endif /* defined(J9VM_ENV_DATA64) */
	}

	static VMINLINE void putAddress(I_64 address, I_64 value)
	{
#if defined(J9VM_ENV_DATA64)
		UNSAFE_PUT64(address, value);
#else
		UNSAFE_PUT32(address, (I_32)value);
#endif /* defined(J9VM_ENV_DATA64) */
	}

	static VMINLINE I_8
	getByte(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, UDATA offset, bool isVolatile)
	{
		return (I_8)get32OrLess(currentThread, objectAccessBarrier, object, offset, isVolatile, 0, true);
	}

	static VMINLINE void
	putByte(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, U_64 offset, bool isVolatile, I_8 value)
	{
		put32OrLess(currentThread, objectAccessBarrier, object, offset, isVolatile, 0, true, (I_32)value);
	}

	static VMINLINE U_8
	getBoolean(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, UDATA offset, bool isVolatile)
	{
		I_32 value = get32OrLess(currentThread, objectAccessBarrier, object, offset, isVolatile, 0, false);
		return (U_8)(0 != value);
	}

	static VMINLINE void
	putBoolean(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, U_64 offset, bool isVolatile, U_8 value)
	{
		put32OrLess(currentThread, objectAccessBarrier, object, offset, isVolatile, 0, false, (I_32)(0 != value));
	}

	static VMINLINE I_16
	getShort(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, UDATA offset, bool isVolatile)
	{
		return (I_16)get32OrLess(currentThread, objectAccessBarrier, object, offset, isVolatile, 1, true);
	}

	static VMINLINE void
	putShort(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, U_64 offset, bool isVolatile, I_16 value)
	{
		put32OrLess(currentThread, objectAccessBarrier, object, offset, isVolatile, 1, true, (I_32)value);
	}

	static VMINLINE U_16
	getChar(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, UDATA offset, bool isVolatile)
	{
		return (U_16)get32OrLess(currentThread, objectAccessBarrier, object, offset, isVolatile, 1, false);
	}

	static VMINLINE void
	putChar(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, U_64 offset, bool isVolatile, U_16 value)
	{
		put32OrLess(currentThread, objectAccessBarrier, object, offset, isVolatile, 1, false, (I_32)value);
	}

	static VMINLINE I_32
	getInt(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, UDATA offset, bool isVolatile)
	{
		return (I_32)get32OrLess(currentThread, objectAccessBarrier, object, offset, isVolatile, 2, true);
	}

	static VMINLINE void
	putInt(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, U_64 offset, bool isVolatile, I_32 value)
	{
		put32OrLess(currentThread, objectAccessBarrier, object, offset, isVolatile, 2, true, (I_32)value);
	}

	static VMINLINE U_32
	getFloat(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, UDATA offset, bool isVolatile)
	{
		return (U_32)get32OrLess(currentThread, objectAccessBarrier, object, offset, isVolatile, 2, false);
	}

	static VMINLINE void
	putFloat(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, U_64 offset, bool isVolatile, U_32 value)
	{
		put32OrLess(currentThread, objectAccessBarrier, object, offset, isVolatile, 2, false, (I_32)value);
	}

	static VMINLINE I_64
	getLong(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, UDATA offset, bool isVolatile)
	{
		return (I_64)get64(currentThread, objectAccessBarrier, object, offset, isVolatile);
	}

	static VMINLINE void
	putLong(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, U_64 offset, bool isVolatile, I_64 value)
	{
		put64(currentThread, objectAccessBarrier, object, offset, isVolatile, (I_64)value);
	}

	static VMINLINE U_64
	getDouble(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, UDATA offset, bool isVolatile)
	{
		return (U_64)get64(currentThread, objectAccessBarrier, object, offset, isVolatile);
	}

	static VMINLINE void
	putDouble(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, U_64 offset, bool isVolatile, U_64 value)
	{
		put64(currentThread, objectAccessBarrier, object, offset, isVolatile, (I_64)value);
	}

	static VMINLINE j9object_t
	getObject(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, UDATA offset, bool isVolatile)
	{
		j9object_t value = NULL;
		if (VM_VMHelpers::objectIsArray(currentThread, object)) {
			UDATA index = convertOffsetToIndex(currentThread, offset, logFJ9ObjectSize(currentThread));
			value = objectAccessBarrier->inlineIndexableObjectReadObject(currentThread, object, index, isVolatile);
		} else if (offset & J9_SUN_STATIC_FIELD_OFFSET_TAG) {
			/* Static field */
			J9Class *fieldClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, object);
			void *valueAddress = (void*)((UDATA)fieldClass->ramStatics + (offset & ~(UDATA)J9_SUN_FIELD_OFFSET_MASK));
			{
				value = objectAccessBarrier->inlineStaticReadObject(currentThread, fieldClass, (j9object_t*)valueAddress, isVolatile);
			}
		} else {
			/* Instance field */
			value = objectAccessBarrier->inlineMixedObjectReadObject(currentThread, object, offset, isVolatile);
		}
		return value;
	}

	static VMINLINE void
	putObject(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, U_64 offset, bool isVolatile, j9object_t *value)
	{
		if (VM_VMHelpers::objectIsArray(currentThread, object)) {
			UDATA index = convertOffsetToIndex(currentThread, offset, logFJ9ObjectSize(currentThread));
			objectAccessBarrier->inlineIndexableObjectStoreObject(currentThread, object, index, *value, isVolatile);
		} else if (offset & J9_SUN_STATIC_FIELD_OFFSET_TAG) {
			/* Static field */
			J9Class *fieldClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, object);
			if (J9_ARE_ANY_BITS_SET(offset, J9_SUN_FINAL_FIELD_OFFSET_TAG)) {
				VM_VMHelpers::reportFinalFieldModified(currentThread, fieldClass);
			}
			void *valueAddress = (void*)((UDATA)fieldClass->ramStatics + (offset & ~(UDATA)J9_SUN_FIELD_OFFSET_MASK));
			objectAccessBarrier->inlineStaticStoreObject(currentThread, fieldClass, (j9object_t*)valueAddress, *value, isVolatile);
		} else {
			/* Instance field */
			objectAccessBarrier->inlineMixedObjectStoreObject(currentThread, object, (UDATA)offset, *value, isVolatile);
		}
	}

	static VMINLINE bool
	compareAndSwapObject(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, UDATA offset, j9object_t *compareValue, j9object_t *swapValue)
	{
		bool result = false;
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
		bool isValueType = (NULL != *compareValue) && J9_IS_J9CLASS_VALUETYPE(J9OBJECT_CLAZZ(currentThread, *compareValue));

		if (isValueType) {
			/* For value types, send the existing object at offset to CAS
			 * if acmp considers it equal to compareValue. Value types should
			 * be compared by their values, not identity.
			 */
			while (true) {
				j9object_t existingValue = getObject(currentThread, objectAccessBarrier, object, offset, true);
				if (currentThread->javaVM->internalVMFunctions->valueTypeCapableAcmp(currentThread, *compareValue, existingValue)) {
					if (VM_VMHelpers::objectIsArray(currentThread, object)) {
						UDATA index = convertOffsetToIndex(currentThread, offset, logFJ9ObjectSize(currentThread));
						result = objectAccessBarrier->inlineIndexableObjectCompareAndSwapObject(currentThread, object, index, existingValue, *swapValue, true);
					} else if (offset & J9_SUN_STATIC_FIELD_OFFSET_TAG) {
						/* Static field */
						J9Class *fieldClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, object);
						if (J9_ARE_ANY_BITS_SET(offset, J9_SUN_FINAL_FIELD_OFFSET_TAG)) {
							VM_VMHelpers::reportFinalFieldModified(currentThread, fieldClass);
						}
						void *valueAddress = (void*)((UDATA)fieldClass->ramStatics + (offset & ~(UDATA)J9_SUN_FIELD_OFFSET_MASK));
						result = objectAccessBarrier->inlineStaticCompareAndSwapObject(currentThread, fieldClass, (j9object_t*)valueAddress, existingValue, *swapValue, true);
					} else {
						/* Instance field */
						result = objectAccessBarrier->inlineMixedObjectCompareAndSwapObject(currentThread, object, offset, existingValue, *swapValue, true);
					}
					if (result) {
						break;
					}
				} else {
					break;
				}
			}
		} else
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
		{
			if (VM_VMHelpers::objectIsArray(currentThread, object)) {
				UDATA index = convertOffsetToIndex(currentThread, offset, logFJ9ObjectSize(currentThread));
				result = objectAccessBarrier->inlineIndexableObjectCompareAndSwapObject(currentThread, object, index, *compareValue, *swapValue, true);
			} else if (offset & J9_SUN_STATIC_FIELD_OFFSET_TAG) {
				/* Static field */
				J9Class *fieldClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, object);
				if (J9_ARE_ANY_BITS_SET(offset, J9_SUN_FINAL_FIELD_OFFSET_TAG)) {
					VM_VMHelpers::reportFinalFieldModified(currentThread, fieldClass);
				}
				void *valueAddress = (void*)((UDATA)fieldClass->ramStatics + (offset & ~(UDATA)J9_SUN_FIELD_OFFSET_MASK));
				result = objectAccessBarrier->inlineStaticCompareAndSwapObject(currentThread, fieldClass, (j9object_t*)valueAddress, *compareValue, *swapValue, true);
			} else {
				/* Instance field */
				result = objectAccessBarrier->inlineMixedObjectCompareAndSwapObject(currentThread, object, offset, *compareValue, *swapValue, true);
			}
		}
		return result;
	}

	static VMINLINE bool
	compareAndSwapLong(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, UDATA offset, U_64 compareValue, U_64 swapValue)
	{
		UDATA logElementSize = 3;
		bool result = false;

		if (NULL == object) {
			result = (compareValue == VM_AtomicSupport::lockCompareExchangeU64((U_64*)offset, compareValue, swapValue));
		} else {
			if (VM_VMHelpers::objectIsArray(currentThread, object)) {
				/* Aligned array access */
				UDATA index = convertOffsetToIndex(currentThread, offset, logElementSize);
				result = objectAccessBarrier->inlineIndexableObjectCompareAndSwapU64(currentThread, object, index, compareValue, swapValue, true);
			} else if (offset & J9_SUN_STATIC_FIELD_OFFSET_TAG) {
				/* Static field */
				J9Class *fieldClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, object);
				if (J9_ARE_ANY_BITS_SET(offset, J9_SUN_FINAL_FIELD_OFFSET_TAG)) {
					VM_VMHelpers::reportFinalFieldModified(currentThread, fieldClass);
				}
				void *valueAddress = (void*)((UDATA)fieldClass->ramStatics + (offset & ~(UDATA)J9_SUN_FIELD_OFFSET_MASK));
				result = objectAccessBarrier->inlineStaticCompareAndSwapU64(currentThread, fieldClass, (U_64*)valueAddress, compareValue, swapValue, true);
			} else {
				/* Instance field */
				result = objectAccessBarrier->inlineMixedObjectCompareAndSwapU64(currentThread, object, offset, compareValue, swapValue, true);
			}
		}
		return result;
	}

	static VMINLINE bool
	compareAndSwapInt(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, UDATA offset, U_32 compareValue, U_32 swapValue)
	{
		UDATA logElementSize = 2;
		bool result = false;

		if (NULL == object) {
			result = (compareValue == VM_AtomicSupport::lockCompareExchangeU32((U_32*)offset, compareValue, swapValue));
		} else {
			if (VM_VMHelpers::objectIsArray(currentThread, object)) {
				/* Aligned array access */
				UDATA index = convertOffsetToIndex(currentThread, offset, logElementSize);
				result = objectAccessBarrier->inlineIndexableObjectCompareAndSwapU32(currentThread, object, index, compareValue, swapValue, true);
			} else if (offset & J9_SUN_STATIC_FIELD_OFFSET_TAG) {
				/* Static field */
				J9Class *fieldClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, object);
				if (J9_ARE_ANY_BITS_SET(offset, J9_SUN_FINAL_FIELD_OFFSET_TAG)) {
					VM_VMHelpers::reportFinalFieldModified(currentThread, fieldClass);
				}
				void *valueAddress = (void*)((UDATA)fieldClass->ramStatics + (offset & ~(UDATA)J9_SUN_FIELD_OFFSET_MASK));
				result = objectAccessBarrier->inlineStaticCompareAndSwapU32(currentThread, fieldClass, (U_32*)valueAddress, compareValue, swapValue, true);
			} else {
				/* Instance field */
				result = objectAccessBarrier->inlineMixedObjectCompareAndSwapU32(currentThread, object, offset, compareValue, swapValue, true);
			}
		}
		return result;
	}

	static VMINLINE j9object_t
	compareAndExchangeObject(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, UDATA offset, j9object_t *compareValue, j9object_t *swapValue)
	{
		j9object_t result = NULL;
		if (J9_UNEXPECTED(NULL == object)) {
			currentThread->javaVM->internalVMFunctions->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
			return result;
		}

#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
		bool isValueType = (NULL != *compareValue) && J9_IS_J9CLASS_VALUETYPE(J9OBJECT_CLAZZ(currentThread, *compareValue));

		if (isValueType) {
			/* For value types, send the existing object at offset to CAE
			 * if acmp considers it equal to compareValue. Value types should
			 * be compared by their values, not identity.
			 */
			while (true) {
				j9object_t existingValue = getObject(currentThread, objectAccessBarrier, object, offset, true);
				if (currentThread->javaVM->internalVMFunctions->valueTypeCapableAcmp(currentThread, *compareValue, existingValue)) {
					if (VM_VMHelpers::objectIsArray(currentThread, object)) {
						/* Aligned array access */
						UDATA index = convertOffsetToIndex(currentThread, offset, logFJ9ObjectSize(currentThread));
						result = objectAccessBarrier->inlineIndexableObjectCompareAndExchangeObject(currentThread, object, index, existingValue, *swapValue, true);
					} else if (offset & J9_SUN_STATIC_FIELD_OFFSET_TAG) {
						/* Static field */
						J9Class *fieldClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, object);
						if (J9_ARE_ANY_BITS_SET(offset, J9_SUN_FINAL_FIELD_OFFSET_TAG)) {
							VM_VMHelpers::reportFinalFieldModified(currentThread, fieldClass);
						}
						void *valueAddress = (void*)((UDATA)fieldClass->ramStatics + (offset & ~(UDATA)J9_SUN_FIELD_OFFSET_MASK));
						result = objectAccessBarrier->inlineStaticCompareAndExchangeObject(currentThread, fieldClass, (j9object_t*)valueAddress, existingValue, *swapValue, true);
					} else {
						/* Instance field */
						result = objectAccessBarrier->inlineMixedObjectCompareAndExchangeObject(currentThread, object, offset, existingValue, *swapValue, true);
					}
					if (result == existingValue) {
						break;
					}
				} else {
					result = existingValue;
					break;
				}
			}
		} else
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
		{
			if (VM_VMHelpers::objectIsArray(currentThread, object)) {
				/* Aligned array access */
				UDATA index = convertOffsetToIndex(currentThread, offset, logFJ9ObjectSize(currentThread));
				result = objectAccessBarrier->inlineIndexableObjectCompareAndExchangeObject(currentThread, object, index, *compareValue, *swapValue, true);
			} else if (offset & J9_SUN_STATIC_FIELD_OFFSET_TAG) {
				/* Static field */
				J9Class *fieldClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, object);
				if (J9_ARE_ANY_BITS_SET(offset, J9_SUN_FINAL_FIELD_OFFSET_TAG)) {
					VM_VMHelpers::reportFinalFieldModified(currentThread, fieldClass);
				}
				void *valueAddress = (void*)((UDATA)fieldClass->ramStatics + (offset & ~(UDATA)J9_SUN_FIELD_OFFSET_MASK));
				result = objectAccessBarrier->inlineStaticCompareAndExchangeObject(currentThread, fieldClass, (j9object_t*)valueAddress, *compareValue, *swapValue, true);
			} else {
				/* Instance field */
				result = objectAccessBarrier->inlineMixedObjectCompareAndExchangeObject(currentThread, object, offset, *compareValue, *swapValue, true);
			}
		}
		return result;
	}

#if defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS)
	static VMINLINE U_8
	compareAndExchangeBoolean(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, UDATA offset, U_8 compareValue, U_8 swapValue)
	{
		UDATA logElementSize = 0;
		U_8 result = 0;

		if (NULL == object) {
			result = VM_AtomicSupport::lockCompareExchangeU8((U_8*)offset, compareValue, swapValue);
		} else {
			if (VM_VMHelpers::objectIsArray(currentThread, object)) {
				/* Aligned array access */
				UDATA index = convertOffsetToIndex(currentThread, offset, logElementSize);
				result = objectAccessBarrier->inlineIndexableObjectCompareAndExchangeU8(currentThread, object, index, compareValue, swapValue, true);
			} else if (J9_ARE_ANY_BITS_SET(offset, J9_SUN_STATIC_FIELD_OFFSET_TAG)) {
				/* Compact layouts are not available for static fields. Access as 32 bits instead. */
				J9Class *fieldClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, object);

				if (J9_ARE_ANY_BITS_SET(offset, J9_SUN_FINAL_FIELD_OFFSET_TAG)) {
					VM_VMHelpers::reportFinalFieldModified(currentThread, fieldClass);
				}

				void *valueAddress = (void*)((UDATA)fieldClass->ramStatics + (offset & ~(UDATA)J9_SUN_FIELD_OFFSET_MASK));
				result = (U_8)objectAccessBarrier->inlineStaticCompareAndExchangeU32(currentThread, fieldClass, (U_32*)valueAddress, compareValue, swapValue, true);
			} else {
				/* Instance field */
				result = objectAccessBarrier->inlineMixedObjectCompareAndExchangeU8(currentThread, object, offset, compareValue, swapValue, true);
			}
		}
		return result;
	}

	static VMINLINE I_8
	compareAndExchangeByte(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, UDATA offset, I_8 compareValue, I_8 swapValue)
	{
		UDATA logElementSize = 0;
		I_8 result = 0;

		if (NULL == object) {
			result = VM_AtomicSupport::lockCompareExchangeU8((U_8*)offset, compareValue, swapValue);
		} else {
			if (VM_VMHelpers::objectIsArray(currentThread, object)) {
				/* Aligned array access */
				UDATA index = convertOffsetToIndex(currentThread, offset, logElementSize);
				result = objectAccessBarrier->inlineIndexableObjectCompareAndExchangeU8(currentThread, object, index, compareValue, swapValue, true);
			} else if (J9_ARE_ANY_BITS_SET(offset, J9_SUN_STATIC_FIELD_OFFSET_TAG)) {
				/* Compact layouts are not available for static fields. Access as 32 bits instead. */
				J9Class *fieldClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, object);

				if (J9_ARE_ANY_BITS_SET(offset, J9_SUN_FINAL_FIELD_OFFSET_TAG)) {
					VM_VMHelpers::reportFinalFieldModified(currentThread, fieldClass);
				}

				void *valueAddress = (void*)((UDATA)fieldClass->ramStatics + (offset & ~(UDATA)J9_SUN_FIELD_OFFSET_MASK));
				result = (I_8)objectAccessBarrier->inlineStaticCompareAndExchangeU32(currentThread, fieldClass, (U_32*)valueAddress, (I_32)compareValue, (I_32)swapValue, true);
			} else {
				/* Instance field */
				result = objectAccessBarrier->inlineMixedObjectCompareAndExchangeU8(currentThread, object, offset, compareValue, swapValue, true);
			}
		}
		return result;
	}

	static VMINLINE U_16
	compareAndExchangeChar(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, UDATA offset, U_16 compareValue, U_16 swapValue)
	{
		UDATA logElementSize = 1;
		U_16 result = 0;

		if (NULL == object) {
			result = VM_AtomicSupport::lockCompareExchangeU16((U_16*)offset, compareValue, swapValue);
		} else {
			if (VM_VMHelpers::objectIsArray(currentThread, object)) {
				/* Aligned array access */
				UDATA index = convertOffsetToIndex(currentThread, offset, logElementSize);
				result = objectAccessBarrier->inlineIndexableObjectCompareAndExchangeU16(currentThread, object, index, compareValue, swapValue, true);
			} else if (J9_ARE_ANY_BITS_SET(offset, J9_SUN_STATIC_FIELD_OFFSET_TAG)) {
				/* Compact layouts are not available for static fields. Access as 32 bits instead. */
				J9Class *fieldClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, object);

				if (J9_ARE_ANY_BITS_SET(offset, J9_SUN_FINAL_FIELD_OFFSET_TAG)) {
					VM_VMHelpers::reportFinalFieldModified(currentThread, fieldClass);
				}

				void *valueAddress = (void*)((UDATA)fieldClass->ramStatics + (offset & ~(UDATA)J9_SUN_FIELD_OFFSET_MASK));
				result = (U_16)objectAccessBarrier->inlineStaticCompareAndExchangeU32(currentThread, fieldClass, (U_32*)valueAddress, compareValue, swapValue, true);
			} else {
				/* Instance field */
				result = objectAccessBarrier->inlineMixedObjectCompareAndExchangeU16(currentThread, object, offset, compareValue, swapValue, true);
			}
		}
		return result;
	}

	static VMINLINE I_16
	compareAndExchangeShort(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, UDATA offset, I_16 compareValue, I_16 swapValue)
	{
		UDATA logElementSize = 1;
		I_16 result = 0;

		if (NULL == object) {
			result = VM_AtomicSupport::lockCompareExchangeU16((U_16*)offset, compareValue, swapValue);
		} else {
			if (VM_VMHelpers::objectIsArray(currentThread, object)) {
				/* Aligned array access */
				UDATA index = convertOffsetToIndex(currentThread, offset, logElementSize);
				result = objectAccessBarrier->inlineIndexableObjectCompareAndExchangeU16(currentThread, object, index, compareValue, swapValue, true);
			} else if (J9_ARE_ANY_BITS_SET(offset, J9_SUN_STATIC_FIELD_OFFSET_TAG)) {
				/* Compact layouts are not available for static fields. Access as 32 bits instead. */
				J9Class *fieldClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, object);

				if (J9_ARE_ANY_BITS_SET(offset, J9_SUN_FINAL_FIELD_OFFSET_TAG)) {
					VM_VMHelpers::reportFinalFieldModified(currentThread, fieldClass);
				}

				void *valueAddress = (void*)((UDATA)fieldClass->ramStatics + (offset & ~(UDATA)J9_SUN_FIELD_OFFSET_MASK));
				result = (I_16)objectAccessBarrier->inlineStaticCompareAndExchangeU32(currentThread, fieldClass, (U_32*)valueAddress, (I_32)compareValue, (I_32)swapValue, true);
			} else {
				/* Instance field */
				result = objectAccessBarrier->inlineMixedObjectCompareAndExchangeU16(currentThread, object, offset, compareValue, swapValue, true);
			}
		}
		return result;
	}
#endif /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */

	static VMINLINE U_32
	compareAndExchangeInt(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, UDATA offset, U_32 compareValue, U_32 swapValue)
	{
		UDATA logElementSize = 2;
		U_32 result = 0;

		if (NULL == object) {
			result = VM_AtomicSupport::lockCompareExchangeU32((U_32*)offset, compareValue, swapValue);
		} else {
			if (VM_VMHelpers::objectIsArray(currentThread, object)) {
				/* Aligned array access */
				UDATA index = convertOffsetToIndex(currentThread, offset, logElementSize);
				result = objectAccessBarrier->inlineIndexableObjectCompareAndExchangeU32(currentThread, object, index, compareValue, swapValue, true);
			} else if (offset & J9_SUN_STATIC_FIELD_OFFSET_TAG) {
				/* Static field */
				J9Class *fieldClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, object);

				if (J9_ARE_ANY_BITS_SET(offset, J9_SUN_FINAL_FIELD_OFFSET_TAG)) {
					VM_VMHelpers::reportFinalFieldModified(currentThread, fieldClass);
				}

				void *valueAddress = (void*)((UDATA)fieldClass->ramStatics + (offset & ~(UDATA)J9_SUN_FIELD_OFFSET_MASK));
				result = objectAccessBarrier->inlineStaticCompareAndExchangeU32(currentThread, fieldClass, (U_32*)valueAddress, compareValue, swapValue, true);
			} else {
				/* Instance field */
				result = objectAccessBarrier->inlineMixedObjectCompareAndExchangeU32(currentThread, object, offset, compareValue, swapValue, true);
			}
		}
		return result;
	}

	static VMINLINE U_64
	compareAndExchangeLong(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI *objectAccessBarrier, j9object_t object, UDATA offset, U_64 compareValue, U_64 swapValue)
	{
		UDATA logElementSize = 3;
		U_64 result = 0;

		if (NULL == object) {
			result = VM_AtomicSupport::lockCompareExchangeU64((U_64*)offset, compareValue, swapValue);
		} else {
			if (VM_VMHelpers::objectIsArray(currentThread, object)) {
				/* Aligned array access */
				UDATA index = convertOffsetToIndex(currentThread, offset, logElementSize);
				result = objectAccessBarrier->inlineIndexableObjectCompareAndExchangeU64(currentThread, object, index, compareValue, swapValue, true);
			} else if (offset & J9_SUN_STATIC_FIELD_OFFSET_TAG) {
				/* Static field */
				J9Class *fieldClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, object);
				if (J9_ARE_ANY_BITS_SET(offset, J9_SUN_FINAL_FIELD_OFFSET_TAG)) {
					VM_VMHelpers::reportFinalFieldModified(currentThread, fieldClass);
				}
				void *valueAddress = (void*)((UDATA)fieldClass->ramStatics + (offset & ~(UDATA)J9_SUN_FIELD_OFFSET_MASK));
				result = objectAccessBarrier->inlineStaticCompareAndExchangeU64(currentThread, fieldClass, (U_64*)valueAddress, compareValue, swapValue, true);
			} else {
				/* Instance field */
				result = objectAccessBarrier->inlineMixedObjectCompareAndExchangeU64(currentThread, object, offset, compareValue, swapValue, true);
			}
		}
		return result;
	}

};

#endif /* UNSAFEAPI_HPP_ */
