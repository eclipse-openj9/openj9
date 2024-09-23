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
 * @ingroup GC_Base
 */

#include "ObjectAccessBarrier.hpp"

#include "j9protos.h"
#include "ModronAssertions.h"
#include "rommeth.h"

#include "ArrayletObjectModel.hpp"
#include "AtomicOperations.hpp"
#include "EnvironmentBase.hpp"
#include "HeapRegionManager.hpp"
#include "MemorySpace.hpp"
#include "ObjectAccessBarrierAPI.hpp"
#include "ObjectMonitor.hpp"
#include "VMHelpers.hpp"
#include "VMThreadListIterator.hpp"

bool 
MM_ObjectAccessBarrier::initialize(MM_EnvironmentBase *env)
{
	J9JavaVM *vm = (J9JavaVM*)env->getOmrVM()->_language_vm;
	OMR_VM *omrVM = env->getOmrVM();
	char *refSignature = (char*) "I";

	if (sizeof(U_64) == J9JAVAVM_REFERENCE_SIZE(vm)) {
		refSignature = (char *) "J";
	}

#if defined(OMR_GC_COMPRESSED_POINTERS)
	if (env->compressObjectReferences()) {

#if defined(J9VM_GC_REALTIME)
		/*
		 * Do not allow 4-bit shift for Metronome
		 * Cell Sizes Table for Segregated heap should be modified to have aligned to 16 values
		 */
		if (_extensions->isMetronomeGC()) {
			if (DEFAULT_LOW_MEMORY_HEAP_CEILING_SHIFT < omrVM->_compressedPointersShift) {
				/* Non-standard NLS message required */
				_extensions->heapInitializationFailureReason = MM_GCExtensionsBase::HEAP_INITIALIZATION_FAILURE_REASON_METRONOME_DOES_NOT_SUPPORT_4BIT_SHIFT;
				return false;
			}
		}
#endif /* J9VM_GC_REALTIME */

#if defined(OMR_GC_FULL_POINTERS)
		_compressObjectReferences = true;
#endif /* defined(OMR_GC_FULL_POINTERS) */
		_compressedPointersShift = omrVM->_compressedPointersShift;
		vm->compressedPointersShift = omrVM->_compressedPointersShift;
		Trc_MM_CompressedAccessBarrierInitialized(env->getLanguageVMThread(), 0, _compressedPointersShift);
	}
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */

	vm->objectAlignmentInBytes = omrVM->_objectAlignmentInBytes;
	vm->objectAlignmentShift = omrVM->_objectAlignmentShift;

	/* request an extra slot in java/lang/ref/Reference which we will use to maintain linked lists of reference objects */
	if (0 != vm->internalVMFunctions->addHiddenInstanceField(vm, "java/lang/ref/Reference", "gcLink", refSignature, &_referenceLinkOffset)) {
		return false;
	}
	/* request an extra slot in java/util/concurrent/locks/AbstractOwnableSynchronizer which we will use to maintain linked lists of ownable synchronizer objects */
	if (0 != vm->internalVMFunctions->addHiddenInstanceField(vm, "java/util/concurrent/locks/AbstractOwnableSynchronizer", "ownableSynchronizerLink", refSignature, &_ownableSynchronizerLinkOffset)) {
		return false;
	}
	/* request an extra slot in java/lang/VirtualThread$VThreadContinuation which we will use to maintain linked lists of continuation objects */
	if (0 != vm->internalVMFunctions->addHiddenInstanceField(vm, "jdk/internal/vm/Continuation", "continuationLink", refSignature, &_continuationLinkOffset)) {
		return false;
	}
	return true;
}

void 
MM_ObjectAccessBarrier::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

void
MM_ObjectAccessBarrier::tearDown(MM_EnvironmentBase *env)
{
}

void
MM_ObjectAccessBarrier::copyArrayCritical(J9VMThread *vmThread, GC_ArrayObjectModel *indexableObjectModel,
	J9InternalVMFunctions *functions, void **data, J9IndexableObject *arrayObject, jboolean *isCopy)
{
	int32_t sizeInElements = (int32_t)indexableObjectModel->getSizeInElements(arrayObject);
	uintptr_t sizeInBytes = indexableObjectModel->getDataSizeInBytes(arrayObject);
	*data = functions->jniArrayAllocateMemoryFromThread(vmThread, sizeInBytes);
	if (NULL == *data) {
		/* better error message here? */
		functions->setNativeOutOfMemoryError(vmThread, 0, 0);
	} else {
		indexableObjectModel->memcpyFromArray(*data, arrayObject, 0, sizeInElements);
		vmThread->jniCriticalCopyCount += 1;
		if (NULL != isCopy) {
			*isCopy = JNI_TRUE;
		}
	}
}

void
MM_ObjectAccessBarrier::copyBackArrayCritical(J9VMThread *vmThread, GC_ArrayObjectModel *indexableObjectModel,
	J9InternalVMFunctions *functions, void *elems, J9IndexableObject **arrayObject, jint mode)
{
	if (JNI_ABORT != mode) {
		I_32 sizeInElements = (I_32)indexableObjectModel->getSizeInElements(*arrayObject);
		indexableObjectModel->memcpyToArray(*arrayObject, 0, sizeInElements, elems);
	}

	/*
	 * Commit means copy the data but do not free the buffer.
	 * All other modes free the buffer.
	 */
	if (JNI_COMMIT != mode) {
		functions->jniArrayFreeMemoryFromThread(vmThread, elems);
	}

	if (vmThread->jniCriticalCopyCount > 0) {
		vmThread->jniCriticalCopyCount -= 1;
	} else {
		Assert_MM_invalidJNICall();
	}
}

void
MM_ObjectAccessBarrier::copyStringCritical(J9VMThread *vmThread, GC_ArrayObjectModel *indexableObjectModel,
	J9InternalVMFunctions *functions, jchar **data, J9JavaVM *javaVM,
	J9IndexableObject *valueObject, J9Object *stringObject, jboolean *isCopy, bool isCompressed)
{
	jint length = J9VMJAVALANGSTRING_LENGTH(vmThread, stringObject);
	uintptr_t sizeInBytes = length * sizeof(jchar);
	jchar *copyArr = (jchar *)functions->jniArrayAllocateMemoryFromThread(vmThread, sizeInBytes);
	if (NULL == copyArr) {
		/* better error message here? */
		functions->setNativeOutOfMemoryError(vmThread, 0, 0);
	} else {
		if (isCompressed) {
			for (jint i = 0; i < length; i++) {
				copyArr[i] = (jchar)(uint8_t)J9JAVAARRAYOFBYTE_LOAD(vmThread, (j9object_t)valueObject, i);
			}
		} else {
			if (J9_ARE_ANY_BITS_SET(javaVM->runtimeFlags, J9_RUNTIME_STRING_BYTE_ARRAY)) {
				/* This API determines the stride based on the type of valueObject so in the [B case we must passin the length in bytes */
				indexableObjectModel->memcpyFromArray(copyArr, valueObject, 0, (int32_t)sizeInBytes);
			} else {
				indexableObjectModel->memcpyFromArray(copyArr, valueObject, 0, length);
			}
		}
		if (NULL != isCopy) {
			*isCopy = JNI_TRUE;
		}
		*data = copyArr;
		vmThread->jniCriticalCopyCount += 1;
	}
}

void
MM_ObjectAccessBarrier::freeStringCritical(J9VMThread *vmThread,
	J9InternalVMFunctions *functions, const jchar *elems)
{
	/* String data is not copied back */
	functions->jniArrayFreeMemoryFromThread(vmThread, (void *)elems);

	if (vmThread->jniCriticalCopyCount > 0) {
		vmThread->jniCriticalCopyCount -= 1;
	} else {
		Assert_MM_invalidJNICall();
	}
}

/**
 * Read an object pointer from an object.
 * This function is only concerned with moving the actual data. Do not re-implement
 * unless the value is stored in a non-native format (e.g. compressed object pointers). 
 * See readObject() for higher-level actions.
 * By default, forwards to readU32Impl() on 32-bit platforms, and readU64Impl() on 64-bit.
 * @param srcObject the object being read from
 * @param srcAddress the address of the field to be read
 * @param isVolatile non-zero if the field is volatile, zero otherwise
 */
mm_j9object_t
MM_ObjectAccessBarrier::readObjectImpl(J9VMThread *vmThread, mm_j9object_t srcObject, fj9object_t *srcAddress, bool isVolatile)
{
	mm_j9object_t result = NULL;
	if (compressObjectReferences()) {
		result = convertPointerFromToken(*(uint32_t*)srcAddress);
	} else {
		result = (mm_j9object_t)*(uintptr_t*)srcAddress;
	}
	return result;
}

/**
 * Read a static object field.
 * This function is only concerned with moving the actual data. Do not re-implement
 * unless the value is stored in a non-native format (e.g. compressed object pointers). 
 * See staticReadObject() for higher-level actions.
 * By default, forwards to readU32Impl() on 32-bit platforms, and readU64Impl() on 64-bit.
 * @param clazz the class which contains the static field
 * @param srcAddress the address of the field to be read
 * @param isVolatile non-zero if the field is volatile, zero otherwise
 */
mm_j9object_t
MM_ObjectAccessBarrier::staticReadObjectImpl(J9VMThread *vmThread, J9Class *clazz, j9object_t *srcAddress, bool isVolatile)
{
	return *srcAddress;
}

/**
 * Read an object from an internal VM slot (J9VMThread, J9JavaVM, named field of J9Class).
 * This function is only concerned with moving the actual data. Do not re-implement
 * unless the value is stored in a non-native format (e.g. compressed object pointers). 
 * See readObjectFromInternalVMSlot() for higher-level actions.
 * By default, forwards to readU32Impl() on 32-bit platforms, and readU64Impl() on 64-bit.
 * @param srcAddress the address of the field to be read
 * @param isVolatile non-zero if the field is volatile, zero otherwise
 */
mm_j9object_t
MM_ObjectAccessBarrier::readObjectFromInternalVMSlotImpl(J9VMThread *vmThread, j9object_t *srcAddress, bool isVolatile)
{
	return *srcAddress;
}

/**
 * Store an object pointer into an object.
 * This function is only concerned with moving the actual data. Do not re-implement
 * unless the value is stored in a non-native format (e.g. compressed object pointers). 
 * See storeObject() for higher-level actions.
 * By default, forwards to storeU32Impl() on 32-bit platforms, and storeU64Impl() on 64-bit.
 * @param destObject the object to read from
 * @param destAddress the address of the field to be read
 * @param value the value to be stored
 * @param isVolatile non-zero if the field is volatile, zero otherwise
 */
void 
MM_ObjectAccessBarrier::storeObjectImpl(J9VMThread *vmThread, mm_j9object_t destObject, fj9object_t *destAddress, mm_j9object_t value, bool isVolatile)
{
	if (compressObjectReferences()) {
		*(uint32_t*)destAddress = (uint32_t)convertTokenFromPointer(value);
	} else {
		*(uintptr_t*)destAddress = (uintptr_t)value;
	}
}

/**
 * Store a static field.
 * This function is only concerned with moving the actual data. Do not re-implement
 * unless the value is stored in a non-native format (e.g. compressed object pointers). 
 * See storeObject() for higher-level actions.
 * By default, forwards to storeU32Impl() on 32-bit platforms, and storeU64Impl() on 64-bit.
 * @param clazz the class the field belongs to
 * @param destAddress the address of the field to be read
 * @param value the value to be stored
 * @param isVolatile non-zero if the field is volatile, zero otherwise
 */
void 
MM_ObjectAccessBarrier::staticStoreObjectImpl(J9VMThread *vmThread, J9Class* clazz, j9object_t *destAddress, mm_j9object_t value, bool isVolatile)
{
	*destAddress = value;
}

/**
 * Write an object to an internal VM slot (J9VMThread, J9JavaVM, named field of J9Class).
 * This function is only concerned with moving the actual data. Do not re-implement
 * unless the value is stored in a non-native format (e.g. compressed object pointers). 
 * See storeObject() for higher-level actions.
 * By default, forwards to storeU32Impl() on 32-bit platforms, and storeU64Impl() on 64-bit.
 * @param destAddress the address of the field to be read
 * @param value the value to be stored
 * @param isVolatile non-zero if the field is volatile, zero otherwise
 */
void 
MM_ObjectAccessBarrier::storeObjectToInternalVMSlotImpl(J9VMThread *vmThread, j9object_t *destAddress, mm_j9object_t value, bool isVolatile)
{
	*destAddress = value;
}

/**
 * Read a U_8 from an object.
 * This function is only concerned with moving the actual data. Do not re-implement
 * unless the value is stored in a non-native format (e.g. compressed object pointers). 
 *
 * @param srcObject the object being read from
 * @param srcAddress the address of the field to be read
 * @param isVolatile non-zero if the field is volatile, zero otherwise
 */
U_8 
MM_ObjectAccessBarrier::readU8Impl(J9VMThread *vmThread, mm_j9object_t srcObject, U_8 *srcAddress, bool isVolatile)
{
	return *srcAddress;
}

/**
 * Store a U_8 into an object.
 * This function is only concerned with moving the actual data. Do not re-implement
 * unless the value is stored in a non-native format (e.g. compressed object pointers). 
 *
 * @param destObject the object to read from
 * @param destAddress the address of the field to be read
 * @param value the value to be stored
 * @param isVolatile non-zero if the field is volatile, zero otherwise
 */
void 
MM_ObjectAccessBarrier::storeU8Impl(J9VMThread *vmThread, mm_j9object_t destObject, U_8 *destAddress, U_8 value, bool isVolatile)
{
	*destAddress = value;
}

/**
 * Read a I_8 from an object.
 * This function is only concerned with moving the actual data. Do not re-implement
 * unless the value is stored in a non-native format (e.g. compressed object pointers). 
 *
 * @param srcObject the object being read from
 * @param srcAddress the address of the field to be read
 * @param isVolatile non-zero if the field is volatile, zero otherwise
 */
 I_8 
MM_ObjectAccessBarrier::readI8Impl(J9VMThread *vmThread, mm_j9object_t srcObject, I_8 *srcAddress, bool isVolatile)
{
	return *srcAddress;
}

/**
 * Store a I_8 into an object.
 * This function is only concerned with moving the actual data. Do not re-implement
 * unless the value is stored in a non-native format (e.g. compressed object pointers). 
 *
 * @param destObject the object to read from
 * @param destAddress the address of the field to be read
 * @param value the value to be stored
 * @param isVolatile non-zero if the field is volatile, zero otherwise
 */
void 
MM_ObjectAccessBarrier::storeI8Impl(J9VMThread *vmThread, mm_j9object_t destObject, I_8 *destAddress, I_8 value, bool isVolatile)
{
	*destAddress = value;
}

/**
 * Read a U_16 from an object.
 * This function is only concerned with moving the actual data. Do not re-implement
 * unless the value is stored in a non-native format (e.g. compressed object pointers). 
 *
 * @param srcObject the object being read from
 * @param srcAddress the address of the field to be read
 * @param isVolatile non-zero if the field is volatile, zero otherwise
 */
 U_16 
MM_ObjectAccessBarrier::readU16Impl(J9VMThread *vmThread, mm_j9object_t srcObject, U_16 *srcAddress, bool isVolatile)
{
	return *srcAddress;
}

/**
 * Store a U_16 into an object.
 * This function is only concerned with moving the actual data. Do not re-implement
 * unless the value is stored in a non-native format (e.g. compressed object pointers). 
 *
 * @param destObject the object to read from
 * @param destAddress the address of the field to be read
 * @param value the value to be stored
 * @param isVolatile non-zero if the field is volatile, zero otherwise
 */
void 
MM_ObjectAccessBarrier::storeU16Impl(J9VMThread *vmThread, mm_j9object_t destObject, U_16 *destAddress, U_16 value, bool isVolatile)
{
	*destAddress = value;
}

/**
 * Read a I_16 from an object.
 * This function is only concerned with moving the actual data. Do not re-implement
 * unless the value is stored in a non-native format (e.g. compressed object pointers). 
 *
 * @param srcObject the object being read from
 * @param srcAddress the address of the field to be read
 * @param isVolatile non-zero if the field is volatile, zero otherwise
 */
 I_16 
MM_ObjectAccessBarrier::readI16Impl(J9VMThread *vmThread, mm_j9object_t srcObject, I_16 *srcAddress, bool isVolatile)
{
	return *srcAddress;
}

/**
 * Store a I_16 into an object.
 * This function is only concerned with moving the actual data. Do not re-implement
 * unless the value is stored in a non-native format (e.g. compressed object pointers). 
 *
 * @param destObject the object to read from
 * @param destAddress the address of the field to be read
 * @param value the value to be stored
 * @param isVolatile non-zero if the field is volatile, zero otherwise
 */
void 
MM_ObjectAccessBarrier::storeI16Impl(J9VMThread *vmThread, mm_j9object_t destObject, I_16 *destAddress, I_16 value, bool isVolatile)
{
	*destAddress = value;
}

/**
 * Read a U_32 from an object.
 * This function is only concerned with moving the actual data. Do not re-implement
 * unless the value is stored in a non-native format (e.g. compressed object pointers). 
 * See readU32() for higher-level actions.
 * @param srcObject the object being read from
 * @param srcAddress the address of the field to be read
 * @param isVolatile non-zero if the field is volatile, zero otherwise
 */
 U_32 
MM_ObjectAccessBarrier::readU32Impl(J9VMThread *vmThread, mm_j9object_t srcObject, U_32 *srcAddress, bool isVolatile)
{
	return *srcAddress;
}

/**
 * Store a U_32 into an object.
 * This function is only concerned with moving the actual data. Do not re-implement
 * unless the value is stored in a non-native format (e.g. compressed object pointers). 
 * See storeU32() for higher-level actions.
 * @param destObject the object to read from
 * @param destAddress the address of the field to be read
 * @param value the value to be stored
 * @param isVolatile non-zero if the field is volatile, zero otherwise
 */
void 
MM_ObjectAccessBarrier::storeU32Impl(J9VMThread *vmThread, mm_j9object_t destObject, U_32 *destAddress, U_32 value, bool isVolatile)
{
	*destAddress = value;
}

/**
 * Read a I_32 from an object.
 * This function is only concerned with moving the actual data. Do not re-implement
 * unless the value is stored in a non-native format (e.g. compressed object pointers). 
 * See readI32() for higher-level actions.
 * By default, forwards to readU32Impl, and casts the return value.
 * @param srcObject the object being read from
 * @param srcAddress the address of the field to be read
 * @param isVolatile non-zero if the field is volatile, zero otherwise
 */
 I_32 
MM_ObjectAccessBarrier::readI32Impl(J9VMThread *vmThread, mm_j9object_t srcObject, I_32 *srcAddress, bool isVolatile)
{
	return *srcAddress;
}

/**
 * Store a U_32 into an object.
 * This function is only concerned with moving the actual data. Do not re-implement
 * unless the value is stored in a non-native format (e.g. compressed object pointers). 
 * See storeI32() for higher-level actions.
 * By default, casts the value to unsigned and forwards to storeU32().
 * @param destObject the object to read from
 * @param destAddress the address of the field to be read
 * @param value the value to be stored
 * @param isVolatile non-zero if the field is volatile, zero otherwise
 */
void 
MM_ObjectAccessBarrier::storeI32Impl(J9VMThread *vmThread, mm_j9object_t destObject, I_32 *destAddress, I_32 value, bool isVolatile)
{
	*destAddress = value;
}

/**
 * Read a U_64 from an object.
 * This function is only concerned with moving the actual data. Do not re-implement
 * unless the value is stored in a non-native format (e.g. compressed object pointers). 
 * See readU64() for higher-level actions.
 * @param srcObject the object being read from
 * @param srcAddress the address of the field to be read
 * @param isVolatile non-zero if the field is volatile, zero otherwise
 */
U_64 
MM_ObjectAccessBarrier::readU64Impl(J9VMThread *vmThread, mm_j9object_t srcObject, U_64 *srcAddress, bool isVolatile)
{
#if !defined(J9VM_ENV_DATA64)
	if (isVolatile) {
		return longVolatileRead(vmThread, srcAddress);
	} 
#endif /* J9VM_ENV_DATA64 */
	return *srcAddress;
}

/**
 * Store a U_64 into an object.
 * This function is only concerned with moving the actual data. Do not re-implement
 * unless the value is stored in a non-native format (e.g. compressed object pointers). 
 * See storeU64() for higher-level actions.
 * @param destObject the object to read from
 * @param destAddress the address of the field to be read
 * @param value the value to be stored
 * @param isVolatile non-zero if the field is volatile, zero otherwise
 */
void 
MM_ObjectAccessBarrier::storeU64Impl(J9VMThread *vmThread, mm_j9object_t destObject, U_64 *destAddress, U_64 value, bool isVolatile)
{
#if !defined(J9VM_ENV_DATA64)
	if (isVolatile) {
		longVolatileWrite(vmThread, destAddress, &value);
		return;
	}
#endif /* J9VM_ENV_DATA64 */
	*destAddress = value;
}

/**
 * Read a I_64 from an object.
 * This function is only concerned with moving the actual data. Do not re-implement
 * unless the value is stored in a non-native format (e.g. compressed object pointers). 
 * See readI64() for higher-level actions.
 * By default, forwards to readU64Impl, and casts the return value.
 * @param srcObject the object being read from
 * @param srcAddress the address of the field to be read
 * @param isVolatile non-zero if the field is volatile, zero otherwise
 */
 I_64 
MM_ObjectAccessBarrier::readI64Impl(J9VMThread *vmThread, mm_j9object_t srcObject, I_64 *srcAddress, bool isVolatile)
{
#if !defined(J9VM_ENV_DATA64)
	if (isVolatile) {
		return longVolatileRead(vmThread, (U_64 *)srcAddress);
	}
#endif /* J9VM_ENV_DATA64 */
	return *srcAddress;
}

/**
 * Store an I_64 into an object.
 * This function is only concerned with moving the actual data. Do not re-implement
 * unless the value is stored in a non-native format (e.g. compressed object pointers). 
 * See storeI64() for higher-level actions.
 * By default, casts the value to unsigned and forwards to storeU64Impl().
 * @param destObject the object to read from
 * @param destAddress the address of the field to be read
 * @param value the value to be stored
 * @param isVolatile non-zero if the field is volatile, zero otherwise
 */
void 
MM_ObjectAccessBarrier::storeI64Impl(J9VMThread *vmThread, mm_j9object_t destObject, I_64 *destAddress, I_64 value, bool isVolatile)
{
#if !defined(J9VM_ENV_DATA64)
	if (isVolatile) {
		longVolatileWrite(vmThread, (U_64 *)destAddress, (U_64 *)&value);
		return;
	} 
#endif /* J9VM_ENV_DATA64 */
	*destAddress = value;
}

/**
 * Read a non-object address (pointer to internal VM data) from an object.
 * This function is only concerned with moving the actual data. Do not re-implement
 * unless the value is stored in a non-native format (e.g. compressed object pointers). 
 * See readAddress() for higher-level actions.
 * By default, forwards to readU32Impl() on 32-bit platforms, and readU64Impl() on 64-bit.
 * @param srcObject the object being read from
 * @param srcAddress the address of the field to be read
 * @param isVolatile non-zero if the field is volatile, zero otherwise
 */
void *
MM_ObjectAccessBarrier::readAddressImpl(J9VMThread *vmThread, mm_j9object_t srcObject, void **srcAddress, bool isVolatile)
{
	return *srcAddress;	
}

/**
 * Store a non-object address into an object.
 * This function is only concerned with moving the actual data. Do not re-implement
 * unless the value is stored in a non-native format (e.g. compressed object pointers). 
 * See storeAddress() for higher-level actions.
 * By default, forwards to storeU32Impl on 32-bit platforms and storeU64Impl on 64-bit.
 * @param destObject the object to read from
 * @param destAddress the address of the field to be read
 * @param value the value to be stored
 * @param isVolatile non-zero if the field is volatile, zero otherwise
 */
void 
MM_ObjectAccessBarrier::storeAddressImpl(J9VMThread *vmThread, mm_j9object_t destObject, void **destAddress, void *value, bool isVolatile)
{
	*destAddress = value;
}

/**
 * Call before a read or write of a possibly volatile field.
 * @note This must be used in tandem with protectIfVolatileAfter()
 * @param isVolatile true if the field is volatile, false otherwise
 * @param isRead true if the field is being read, false if it is being written
 * @param isWide true if the field is wide (64-bit), false otherwise
 */
void
MM_ObjectAccessBarrier::protectIfVolatileBefore(J9VMThread *vmThread, bool isVolatile, bool isRead, bool isWide)
{
	if (isVolatile) {
		/* need to insert a sync instruction
		 * As this is inline, compiler should optimize this away when not necessary.
		 */
		if (!isRead) {
			/* atomic writeBarrier */
			MM_AtomicOperations::storeSync();
		}	
	}
}

/**
 * Call after a read or write of a possibly volatile field.
 * @note This must be used in tandem with protectIfVolatileBefore()
 * @param isVolatile true if the field is volatile, false otherwise
 * @param isRead true if the field is being read, false if it is being written
 * @param isWide true if the field is wide (64-bit), false otherwise
 */
void
MM_ObjectAccessBarrier::protectIfVolatileAfter(J9VMThread *vmThread, bool isVolatile, bool isRead, bool isWide)
{
	if (isVolatile) {
		/* need to insert a sync instruction
		 * As this is inline, compiler should optimize this away when not necessary.
		 */
		if (isRead) {
			/* atomic readBarrier */
			MM_AtomicOperations::loadSync();
		} else {
			/* atomic readWriteBarrier */
			MM_AtomicOperations::sync();	
		}
	}
}

/**
 * Read an object field: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param srcObject The object being used.
 * @param srcOffset The offset of the field.
 * @param isVolatile non-zero if the field is volatile.
 */
J9Object *
MM_ObjectAccessBarrier::mixedObjectReadObject(J9VMThread *vmThread, J9Object *srcObject, UDATA srcOffset, bool isVolatile)
{
	fj9object_t *actualAddress = J9OAB_MIXEDOBJECT_EA(srcObject, srcOffset, fj9object_t);
	J9Object *result = NULL;

	if (preObjectRead(vmThread, srcObject, actualAddress)) {
		protectIfVolatileBefore(vmThread, isVolatile, true, false);
		result = readObjectImpl(vmThread, srcObject, actualAddress, isVolatile);
		protectIfVolatileAfter(vmThread, isVolatile, true, false);

		if (!postObjectRead(vmThread, srcObject, actualAddress)) {
			result = NULL;
		}
	}
	
	/* This must always be called to massage the return value */
	return result;
}

/**
 * Read an object field: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param srcObject The object being used.
 * @param srcOffset The offset of the field.
 * @param isVolatile non-zero if the field is volatile.
 */
void *
MM_ObjectAccessBarrier::mixedObjectReadAddress(J9VMThread *vmThread, J9Object *srcObject, UDATA srcOffset, bool isVolatile)
{
	void **actualAddress = J9OAB_MIXEDOBJECT_EA(srcObject, srcOffset,void *);
	protectIfVolatileBefore(vmThread, isVolatile, true, false);
	void *result = readAddressImpl(vmThread, srcObject, actualAddress, isVolatile);
	protectIfVolatileAfter(vmThread, isVolatile, true, false);
	return result;
}

/**
 * Read an object field: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param srcObject The object being used.
 * @param srcOffset The offset of the field.
 * @param isVolatile non-zero if the field is volatile.
 */
 U_32
MM_ObjectAccessBarrier::mixedObjectReadU32(J9VMThread *vmThread, J9Object *srcObject, UDATA srcOffset, bool isVolatile)
{
	U_32 *actualAddress = J9OAB_MIXEDOBJECT_EA(srcObject, srcOffset, U_32);
	protectIfVolatileBefore(vmThread, isVolatile, true, false);
	U_32 result = readU32Impl(vmThread, srcObject, actualAddress, isVolatile);
	protectIfVolatileAfter(vmThread, isVolatile, true, false);
	return result;
}

/**
 * Read an object field: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param srcObject The object being used.
 * @param srcOffset The offset of the field.
 * @param isVolatile non-zero if the field is volatile.
 */
 I_32
MM_ObjectAccessBarrier::mixedObjectReadI32(J9VMThread *vmThread, J9Object *srcObject, UDATA srcOffset, bool isVolatile)
{
	I_32 *actualAddress = J9OAB_MIXEDOBJECT_EA(srcObject, srcOffset, I_32);
	protectIfVolatileBefore(vmThread, isVolatile, true, false);
	I_32 result = readI32Impl(vmThread, srcObject, actualAddress, isVolatile);
	protectIfVolatileAfter(vmThread, isVolatile, true, false);
	return result;
}

/**
 * Read an object field: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param srcObject The object being used.
 * @param srcOffset The offset of the field.
 * @param isVolatile non-zero if the field is volatile.
 */
 U_64
MM_ObjectAccessBarrier::mixedObjectReadU64(J9VMThread *vmThread, J9Object *srcObject, UDATA srcOffset, bool isVolatile)
{
	U_64 *actualAddress = J9OAB_MIXEDOBJECT_EA(srcObject, srcOffset, U_64);
	protectIfVolatileBefore(vmThread, isVolatile, true, true);
	U_64 result = readU64Impl(vmThread, srcObject, actualAddress, isVolatile);
	protectIfVolatileAfter(vmThread, isVolatile, true, true);
	return result;
}

/**
 * Read an object field: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param srcObject The object being used.
 * @param srcOffset The offset of the field.
 * @param isVolatile non-zero if the field is volatile.
 */
 I_64
MM_ObjectAccessBarrier::mixedObjectReadI64(J9VMThread *vmThread, J9Object *srcObject, UDATA srcOffset, bool isVolatile)
{
	I_64 *actualAddress = J9OAB_MIXEDOBJECT_EA(srcObject, srcOffset, I_64);
	protectIfVolatileBefore(vmThread, isVolatile, true, true);
	I_64 result = readI64Impl(vmThread, srcObject, actualAddress, isVolatile);
	protectIfVolatileAfter(vmThread, isVolatile, true, true);
	return result;
}

/**
 * Store an object field: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param destObject The object being used.
 * @param destOffset The offset of the field.
 * @param value The value to be stored
 * @param isVolatile non-zero if the field is volatile.
 */
void
MM_ObjectAccessBarrier::mixedObjectStoreObject(J9VMThread *vmThread, J9Object *destObject, UDATA destOffset, J9Object *value, bool isVolatile)
{
	fj9object_t *actualAddress = J9OAB_MIXEDOBJECT_EA(destObject, destOffset, fj9object_t);

	if (preObjectStore(vmThread, destObject, actualAddress, value, isVolatile)) {
		protectIfVolatileBefore(vmThread, isVolatile, false, false);
		storeObjectImpl(vmThread, destObject, actualAddress, value, isVolatile);
		protectIfVolatileAfter(vmThread, isVolatile, false, false);
	
		postObjectStore(vmThread, destObject, actualAddress, value, isVolatile);
	}
}

/**
 * Store an object field: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param destObject The object being used.
 * @param destOffset The offset of the field.
 * @param value The value to be stored
 * @param isVolatile non-zero if the field is volatile.
 */
void
MM_ObjectAccessBarrier::mixedObjectStoreAddress(J9VMThread *vmThread, J9Object *destObject, UDATA destOffset,void *value, bool isVolatile)
{
	void **actualAddress = J9OAB_MIXEDOBJECT_EA(destObject, destOffset,void *);
	protectIfVolatileBefore(vmThread, isVolatile, false, false);
	storeAddressImpl(vmThread, destObject, actualAddress, value, isVolatile);
	protectIfVolatileAfter(vmThread, isVolatile, false, false);
}

/**
 * Store an object field: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param destObject The object being used.
 * @param destOffset The offset of the field.
 * @param value The value to be stored
 * @param isVolatile non-zero if the field is volatile.
 */
void
MM_ObjectAccessBarrier::mixedObjectStoreU32(J9VMThread *vmThread, J9Object *destObject, UDATA destOffset, U_32 value, bool isVolatile)
{
	U_32 *actualAddress = J9OAB_MIXEDOBJECT_EA(destObject, destOffset, U_32);
	protectIfVolatileBefore(vmThread, isVolatile, false, false);
	storeU32Impl(vmThread, destObject, actualAddress, value, isVolatile);
	protectIfVolatileAfter(vmThread, isVolatile, false, false);
}

/**
 * Store an object field: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param destObject The object being used.
 * @param destOffset The offset of the field.
 * @param value The value to be stored
 * @param isVolatile non-zero if the field is volatile.
 */
void
MM_ObjectAccessBarrier::mixedObjectStoreI32(J9VMThread *vmThread, J9Object *destObject, UDATA destOffset, I_32 value, bool isVolatile)
{
	I_32 *actualAddress = J9OAB_MIXEDOBJECT_EA(destObject, destOffset, I_32);
	protectIfVolatileBefore(vmThread, isVolatile, false, false);
	storeI32Impl(vmThread, destObject, actualAddress, value, isVolatile);
	protectIfVolatileAfter(vmThread, isVolatile, false, false);
}

/**
 * Store an object field: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param destObject The object being used.
 * @param destOffset The offset of the field.
 * @param value The value to be stored
 * @param isVolatile non-zero if the field is volatile.
 */
void
MM_ObjectAccessBarrier::mixedObjectStoreU64(J9VMThread *vmThread, J9Object *destObject, UDATA destOffset, U_64 value, bool isVolatile)
{
	U_64 *actualAddress = J9OAB_MIXEDOBJECT_EA(destObject, destOffset, U_64);
	protectIfVolatileBefore(vmThread, isVolatile, false, true);
	storeU64Impl(vmThread, destObject, actualAddress, value, isVolatile);
	protectIfVolatileAfter(vmThread, isVolatile, false, true);
}

/**
 * Store an object field: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param destObject The object being used.
 * @param destOffset The offset of the field.
 * @param value The value to be stored
 * @param isVolatile non-zero if the field is volatile.
 */
void
MM_ObjectAccessBarrier::mixedObjectStoreI64(J9VMThread *vmThread, J9Object *destObject, UDATA destOffset, I_64 value, bool isVolatile)
{
	I_64 *actualAddress = J9OAB_MIXEDOBJECT_EA(destObject, destOffset, I_64);
	protectIfVolatileBefore(vmThread, isVolatile, false, true);
	storeI64Impl(vmThread, destObject, actualAddress, value, isVolatile);
	protectIfVolatileAfter(vmThread, isVolatile, false, true);
}

/**
 * Read an array element: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param srcObject The array being used.
 * @param srcIndex The element index
 */
J9Object *
MM_ObjectAccessBarrier::indexableReadObject(J9VMThread *vmThread, J9IndexableObject *srcObject, I_32 srcIndex, bool isVolatile)
{
	UDATA const referenceSize = J9VMTHREAD_REFERENCE_SIZE(vmThread);
	fj9object_t *actualAddress = (fj9object_t *)indexableEffectiveAddress(vmThread, srcObject, srcIndex, referenceSize);
	J9Object *result = NULL;

	/* No preObjectRead yet because no barrier require it */
	if (preObjectRead(vmThread, (J9Object *)srcObject, actualAddress)) {
		protectIfVolatileBefore(vmThread, isVolatile, true, false);
		result = readObjectImpl(vmThread, (J9Object*)srcObject, actualAddress);
		protectIfVolatileAfter(vmThread, isVolatile, true, false);

		if (!postObjectRead(vmThread, (J9Object *)srcObject, actualAddress)) {
			result = NULL;
		}
	}

	/* This must always be called to massage the return value */
	return result;
}

/**
 * Read an array element: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param srcObject The array being used.
 * @param srcIndex The element index
 */
void *
MM_ObjectAccessBarrier::indexableReadAddress(J9VMThread *vmThread, J9IndexableObject *srcObject, I_32 srcIndex, bool isVolatile)
{
	void **actualAddress = (void **)indexableEffectiveAddress(vmThread, srcObject, srcIndex, sizeof(void *));
	protectIfVolatileBefore(vmThread, isVolatile, true, false);
	void *result = readAddressImpl(vmThread, (J9Object*)srcObject, actualAddress);
	protectIfVolatileAfter(vmThread, isVolatile, true, false);
	return result;
}

/**
 * Read an array element: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param srcObject The array being used.
 * @param srcIndex The element index
 */
 U_8
MM_ObjectAccessBarrier::indexableReadU8(J9VMThread *vmThread, J9IndexableObject *srcObject, I_32 srcIndex, bool isVolatile)
{
	U_8 *actualAddress = (U_8 *)indexableEffectiveAddress(vmThread, srcObject, srcIndex, sizeof(U_8));
	protectIfVolatileBefore(vmThread, isVolatile, true, false);
	U_8 result = readU8Impl(vmThread, (mm_j9object_t)srcObject, actualAddress);
	protectIfVolatileAfter(vmThread, isVolatile, true, false);
	return result;
}

/**
 * Read an array element: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param srcObject The array being used.
 * @param srcIndex The element index
 */
 I_8
MM_ObjectAccessBarrier::indexableReadI8(J9VMThread *vmThread, J9IndexableObject *srcObject, I_32 srcIndex, bool isVolatile)
{
	I_8 *actualAddress = (I_8 *)indexableEffectiveAddress(vmThread, srcObject, srcIndex, sizeof(I_8));
	protectIfVolatileBefore(vmThread, isVolatile, true, false);
	I_8 result = readI8Impl(vmThread, (mm_j9object_t)srcObject, actualAddress);
	protectIfVolatileAfter(vmThread, isVolatile, true, false);
	return result;
}

/**
 * Read an array element: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param srcObject The array being used.
 * @param srcIndex The element index
 */
 U_16
MM_ObjectAccessBarrier::indexableReadU16(J9VMThread *vmThread, J9IndexableObject *srcObject, I_32 srcIndex, bool isVolatile)
{
	U_16 *actualAddress = (U_16 *)indexableEffectiveAddress(vmThread, srcObject, srcIndex,sizeof(U_16));
	protectIfVolatileBefore(vmThread, isVolatile, true, false);
	U_16 result = readU16Impl(vmThread, (mm_j9object_t)srcObject, actualAddress);
	protectIfVolatileAfter(vmThread, isVolatile, true, false);
	return result;
}

/**
 * Read an array element: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param srcObject The array being used.
 * @param srcIndex The element index
 */
 I_16
MM_ObjectAccessBarrier::indexableReadI16(J9VMThread *vmThread, J9IndexableObject *srcObject, I_32 srcIndex, bool isVolatile)
{
	I_16 *actualAddress = (I_16 *)indexableEffectiveAddress(vmThread, srcObject, srcIndex, sizeof(I_16));
	protectIfVolatileBefore(vmThread, isVolatile, true, false);
	I_16 result = readI16Impl(vmThread, (mm_j9object_t)srcObject, actualAddress);
	protectIfVolatileAfter(vmThread, isVolatile, true, false);
	return result;
}

/**
 * Read an array element: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param srcObject The array being used.
 * @param srcIndex The element index
 */
 U_32
MM_ObjectAccessBarrier::indexableReadU32(J9VMThread *vmThread, J9IndexableObject *srcObject, I_32 srcIndex, bool isVolatile)
{
	U_32 *actualAddress = (U_32 *)indexableEffectiveAddress(vmThread, srcObject, srcIndex, sizeof(U_32));
	protectIfVolatileBefore(vmThread, isVolatile, true, false);
	U_32 result = readU32Impl(vmThread, (mm_j9object_t)srcObject, actualAddress);
	protectIfVolatileAfter(vmThread, isVolatile, true, false);
	return result;
}

/**
 * Read an array element: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param srcObject The array being used.
 * @param srcIndex The element index
 */
 I_32
MM_ObjectAccessBarrier::indexableReadI32(J9VMThread *vmThread, J9IndexableObject *srcObject, I_32 srcIndex, bool isVolatile)
{
	I_32 *actualAddress = (I_32 *)indexableEffectiveAddress(vmThread, srcObject, srcIndex, sizeof(I_32));
	protectIfVolatileBefore(vmThread, isVolatile, true, false);
	I_32 result = readI32Impl(vmThread, (mm_j9object_t)srcObject, actualAddress);
	protectIfVolatileAfter(vmThread, isVolatile, true, false);
	return result;
}

/**
 * Read an array element: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param srcObject The array being used.
 * @param srcIndex The element index
 */
 U_64
MM_ObjectAccessBarrier::indexableReadU64(J9VMThread *vmThread, J9IndexableObject *srcObject, I_32 srcIndex, bool isVolatile)
{
	U_64 *actualAddress = (U_64 *)indexableEffectiveAddress(vmThread, srcObject, srcIndex, sizeof(U_64));
	protectIfVolatileBefore(vmThread, isVolatile, true, true);
	U_64 result = readU64Impl(vmThread, (mm_j9object_t)srcObject, actualAddress, isVolatile);
	protectIfVolatileAfter(vmThread, isVolatile, true, true);
	return result;
}

/**
 * Read an array element: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param srcObject The array being used.
 * @param srcIndex The element index
 */
 I_64
MM_ObjectAccessBarrier::indexableReadI64(J9VMThread *vmThread, J9IndexableObject *srcObject, I_32 srcIndex, bool isVolatile)
{
	I_64 *actualAddress = (I_64 *)indexableEffectiveAddress(vmThread, srcObject, srcIndex, sizeof(I_64));
	protectIfVolatileBefore(vmThread, isVolatile, true, true);
	I_64 result = readI64Impl(vmThread, (mm_j9object_t)srcObject, actualAddress, isVolatile);
	protectIfVolatileAfter(vmThread, isVolatile, true, true);
	return result;
}


/**
 * Store an array element: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param destObject The array being used.
 * @param destIndex The element index
 * @param value The value to store.
 */
void
MM_ObjectAccessBarrier::indexableStoreObject(J9VMThread *vmThread, J9IndexableObject *destObject, I_32 destIndex, J9Object *value, bool isVolatile)
{
	UDATA const referenceSize = J9VMTHREAD_REFERENCE_SIZE(vmThread);
	fj9object_t *actualAddress = (fj9object_t *)indexableEffectiveAddress(vmThread, destObject, destIndex, referenceSize);
	
	if (preObjectStore(vmThread, (J9Object *)destObject, actualAddress, value)) {
		protectIfVolatileBefore(vmThread, isVolatile, false, false);
		storeObjectImpl(vmThread, (J9Object*)destObject, actualAddress, value);
		protectIfVolatileAfter(vmThread, isVolatile, false, false);
		postObjectStore(vmThread, (J9Object *)destObject, actualAddress, value);
	}
}

/**
 * Store an array element: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param destObject The array being used.
 * @param destIndex The element index
 * @param value The value to store.
 */
void
MM_ObjectAccessBarrier::indexableStoreAddress(J9VMThread *vmThread, J9IndexableObject *destObject, I_32 destIndex,void *value, bool isVolatile)
{
	void **actualAddress = (void **)indexableEffectiveAddress(vmThread, destObject, destIndex, sizeof(void *));
	protectIfVolatileBefore(vmThread, isVolatile, false, false);
	storeAddressImpl(vmThread, (mm_j9object_t)destObject, actualAddress, value);
	protectIfVolatileAfter(vmThread, isVolatile, false, false);
}

/**
 * Store an array element: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param destObject The array being used.
 * @param destIndex The element index
 * @param value The value to store.
 */
void
MM_ObjectAccessBarrier::indexableStoreU8(J9VMThread *vmThread, J9IndexableObject *destObject, I_32 destIndex, U_8 value, bool isVolatile)
{
	U_8 *actualAddress = (U_8 *)indexableEffectiveAddress(vmThread, destObject, destIndex, sizeof(U_8));
	protectIfVolatileBefore(vmThread, isVolatile, false, false);
	storeU8Impl(vmThread, (mm_j9object_t)destObject, actualAddress, value);
	protectIfVolatileAfter(vmThread, isVolatile, false, false);
}

/**
 * Store an array element: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param destObject The array being used.
 * @param destIndex The element index
 * @param value The value to store.
 */
void
MM_ObjectAccessBarrier::indexableStoreI8(J9VMThread *vmThread, J9IndexableObject *destObject, I_32 destIndex, I_8 value, bool isVolatile)
{
	I_8 *actualAddress = (I_8 *)indexableEffectiveAddress(vmThread, destObject, destIndex, sizeof(I_8));
	protectIfVolatileBefore(vmThread, isVolatile, false, false);
	storeI8Impl(vmThread, (mm_j9object_t)destObject, actualAddress, value);
	protectIfVolatileAfter(vmThread, isVolatile, false, false);
}

/**
 * Store an array element: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param destObject The array being used.
 * @param destIndex The element index
 * @param value The value to store.
 */
void
MM_ObjectAccessBarrier::indexableStoreU16(J9VMThread *vmThread, J9IndexableObject *destObject, I_32 destIndex, U_16 value, bool isVolatile)
{
	U_16 *actualAddress = (U_16 *)indexableEffectiveAddress(vmThread, destObject, destIndex, sizeof(U_16));
	protectIfVolatileBefore(vmThread, isVolatile, false, false);
	storeU16Impl(vmThread, (mm_j9object_t)destObject, actualAddress, value);
	protectIfVolatileAfter(vmThread, isVolatile, false, false);
}

/**
 * Store an array element: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param destObject The array being used.
 * @param destIndex The element index
 * @param value The value to store.
 */
void
MM_ObjectAccessBarrier::indexableStoreI16(J9VMThread *vmThread, J9IndexableObject *destObject, I_32 destIndex, I_16 value, bool isVolatile)
{
	I_16 *actualAddress = (I_16 *)indexableEffectiveAddress(vmThread, destObject, destIndex, sizeof(I_16));
	protectIfVolatileBefore(vmThread, isVolatile, false, false);
	storeI16Impl(vmThread, (mm_j9object_t)destObject, actualAddress, value);
	protectIfVolatileAfter(vmThread, isVolatile, false, false);
}

/**
 * Store an array element: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param destObject The array being used.
 * @param destIndex The element index
 * @param value The value to store.
 */
void
MM_ObjectAccessBarrier::indexableStoreU32(J9VMThread *vmThread, J9IndexableObject *destObject, I_32 destIndex, U_32 value, bool isVolatile)
{
	U_32 *actualAddress = (U_32 *)indexableEffectiveAddress(vmThread, destObject, destIndex, sizeof(U_32));
	protectIfVolatileBefore(vmThread, isVolatile, false, false);
	storeU32Impl(vmThread, (mm_j9object_t)destObject, actualAddress, value);
	protectIfVolatileAfter(vmThread, isVolatile, false, false);
}

/**
 * Store an array element: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param destObject The array being used.
 * @param destIndex The element index
 * @param value The value to store.
 */
void
MM_ObjectAccessBarrier::indexableStoreI32(J9VMThread *vmThread, J9IndexableObject *destObject, I_32 destIndex, I_32 value, bool isVolatile)
{
	I_32 *actualAddress = (I_32 *)indexableEffectiveAddress(vmThread, destObject, destIndex, sizeof(I_32));
	protectIfVolatileBefore(vmThread, isVolatile, false, false);
	storeI32Impl(vmThread, (mm_j9object_t)destObject, actualAddress, value);
	protectIfVolatileAfter(vmThread, isVolatile, false, false);
}

/**
 * Store an array element: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param destObject The array being used.
 * @param destIndex The element index
 * @param value The value to store.
 */
void
MM_ObjectAccessBarrier::indexableStoreU64(J9VMThread *vmThread, J9IndexableObject *destObject, I_32 destIndex, U_64 value, bool isVolatile)
{
	U_64 *actualAddress = (U_64 *)indexableEffectiveAddress(vmThread, destObject, destIndex, sizeof(U_64));
	protectIfVolatileBefore(vmThread, isVolatile, false, true);
	storeU64Impl(vmThread, (mm_j9object_t)destObject, actualAddress, value, isVolatile);
	protectIfVolatileAfter(vmThread, isVolatile, false, true);
}

/**
 * Store an array element: perform any pre-use barriers, calculate an effective address
 * and perform the work.
 * @param destObject The array being used.
 * @param destIndex The element index
 * @param value The value to store.
 */
void
MM_ObjectAccessBarrier::indexableStoreI64(J9VMThread *vmThread, J9IndexableObject *destObject, I_32 destIndex, I_64 value, bool isVolatile)
{
	I_64 *actualAddress = (I_64 *)indexableEffectiveAddress(vmThread, destObject, destIndex, sizeof(I_64));
	protectIfVolatileBefore(vmThread, isVolatile, false, true);
	storeI64Impl(vmThread, (mm_j9object_t)destObject, actualAddress, value, isVolatile);
	protectIfVolatileAfter(vmThread, isVolatile, false, true);
}

/**
 * Copy object fields into flattened array element
 *
 * @param vmThread thread token
 * @param arrayClazz array J9Class
 * @param srcObject object whose fields will be copied
 * @param arrayRef array object
 * @param index index of array element where fields are copied to
 */
void
MM_ObjectAccessBarrier::copyObjectFieldsToFlattenedArrayElement(J9VMThread *vmThread, J9ArrayClass *arrayClazz, j9object_t srcObject, J9IndexableObject *arrayRef, I_32 index)
{
	UDATA elementStartOffset = J9VMTHREAD_OBJECT_HEADER_SIZE(vmThread);
	U_8 *elementAddress = (U_8*)indexableEffectiveAddress(vmThread, arrayRef, index, J9ARRAYCLASS_GET_STRIDE((J9Class *) arrayClazz));
	IDATA elementOffset = (elementAddress - (U_8*)arrayRef);
	J9Class *elementClazz = J9GC_J9OBJECT_CLAZZ_THREAD(srcObject, vmThread);
	Assert_MM_true(J9_IS_J9CLASS_ALLOW_DEFAULT_VALUE(elementClazz));
	Assert_MM_true(elementClazz == arrayClazz->leafComponentType);

	elementStartOffset += J9CLASS_PREPADDING_SIZE(elementClazz);

	copyObjectFields(vmThread, elementClazz, srcObject, elementStartOffset, (j9object_t) arrayRef, elementOffset);
}

/**
 * Copy object fields into flattened array element
 *
 * @param vmThread thread token
 * @param arrayClazz array J9Class
 * @param destObject object where array element fields will be copied to
 * @param arrayRef array object
 * @param index index of array element where fields are copied to
 */
void
MM_ObjectAccessBarrier::copyObjectFieldsFromFlattenedArrayElement(J9VMThread *vmThread, J9ArrayClass *arrayClazz, j9object_t destObject, J9IndexableObject *arrayRef, I_32 index)
{
	UDATA elementStartOffset = J9VMTHREAD_OBJECT_HEADER_SIZE(vmThread);
	U_8 *elementAddress = (U_8*)indexableEffectiveAddress(vmThread, arrayRef, index, J9ARRAYCLASS_GET_STRIDE((J9Class *) arrayClazz));
	IDATA elementOffset = (elementAddress - (U_8*)arrayRef);
	J9Class *elementClazz = J9GC_J9OBJECT_CLAZZ_THREAD(destObject, vmThread);
	Assert_MM_true(J9_IS_J9CLASS_ALLOW_DEFAULT_VALUE(elementClazz));
	Assert_MM_true(elementClazz == arrayClazz->leafComponentType);

	elementStartOffset += J9CLASS_PREPADDING_SIZE(elementClazz);

	copyObjectFields(vmThread, elementClazz, (j9object_t) arrayRef, elementOffset, destObject, elementStartOffset);
}


/**
 * Read a static field.
 * @param srcSlot The static field slot.
 * @param isVolatile non-zero if the field is volatile.
 */
J9Object *
MM_ObjectAccessBarrier::staticReadObject(J9VMThread *vmThread, J9Class *clazz, J9Object **srcSlot, bool isVolatile)
{
	J9Object *result = NULL;

	if (preObjectRead(vmThread, clazz, srcSlot)) {
		protectIfVolatileBefore(vmThread, isVolatile, true, true);
		result = staticReadObjectImpl(vmThread, clazz, srcSlot, isVolatile);
		protectIfVolatileAfter(vmThread, isVolatile, true, true);

		if (!postObjectRead(vmThread, clazz, srcSlot)) {
			result = NULL;
		}
	}

	/* This must always be called to massage the return value */
	return result;
}

/**
 * Read a static field.
 * @param srcSlot The static field slot.
 * @param isVolatile non-zero if the field is volatile.
 */
void *
MM_ObjectAccessBarrier::staticReadAddress(J9VMThread *vmThread, J9Class *clazz, void **srcSlot, bool isVolatile)
{
	protectIfVolatileBefore(vmThread, isVolatile, true, true);
	void *result = readAddressImpl(vmThread, NULL, srcSlot, isVolatile);
	protectIfVolatileAfter(vmThread, isVolatile, true, true);
	return result;
}

/**
 * Read a static field.
 * @param srcSlot The static field slot.
 * @param isVolatile non-zero if the field is volatile.
 */
 U_32
MM_ObjectAccessBarrier::staticReadU32(J9VMThread *vmThread, J9Class *clazz, U_32 *srcSlot, bool isVolatile)
{
	protectIfVolatileBefore(vmThread, isVolatile, true, true);
	U_32 result = readU32Impl(vmThread, NULL, srcSlot, isVolatile);
	protectIfVolatileAfter(vmThread, isVolatile, true, true);
	return result;
}

/**
 * Read a static field.
 * @param srcSlot The static field slot.
 * @param isVolatile non-zero if the field is volatile.
 */
 I_32
MM_ObjectAccessBarrier::staticReadI32(J9VMThread *vmThread, J9Class *clazz, I_32 *srcSlot, bool isVolatile)
{
	protectIfVolatileBefore(vmThread, isVolatile, true, true);
	I_32 result = readI32Impl(vmThread, NULL, srcSlot, isVolatile);
	protectIfVolatileAfter(vmThread, isVolatile, true, true);
	return result;
}

/**
 * Read a static field.
 * @param srcSlot The static field slot.
 * @param isVolatile non-zero if the field is volatile.
 */
 U_64
MM_ObjectAccessBarrier::staticReadU64(J9VMThread *vmThread, J9Class *clazz, U_64 *srcSlot, bool isVolatile)
{
	protectIfVolatileBefore(vmThread, isVolatile, true, true);
	U_64 result = readU64Impl(vmThread, NULL, srcSlot, isVolatile);
	protectIfVolatileAfter(vmThread, isVolatile, true, true);
	return result;
}

/**
 * Read a static field.
 * @param srcSlot The static field slot.
 * @param isVolatile non-zero if the field is volatile.
 */
 I_64
MM_ObjectAccessBarrier::staticReadI64(J9VMThread *vmThread, J9Class *clazz, I_64 *srcSlot, bool isVolatile)
{
	protectIfVolatileBefore(vmThread, isVolatile, true, true);
	I_64 result = readI64Impl(vmThread, NULL, srcSlot, isVolatile);
	protectIfVolatileAfter(vmThread, isVolatile, true, true);
	return result;
}

/**
 * Store a static field.
 * @param destSlot The static slot being used.
 * @param value The value to be stored
 * @param isVolatile non-zero if the field is volatile.
 */
void
MM_ObjectAccessBarrier::staticStoreObject(J9VMThread *vmThread, J9Class *clazz, J9Object **destSlot, J9Object *value, bool isVolatile)
{
	j9object_t destObject = J9VM_J9CLASS_TO_HEAPCLASS(clazz);
	if (preObjectStore(vmThread, destObject, destSlot, value, isVolatile)) {
		protectIfVolatileBefore(vmThread, isVolatile, false, true);
		staticStoreObjectImpl(vmThread, clazz, destSlot, value, isVolatile);
		protectIfVolatileAfter(vmThread, isVolatile, false, true);
	
		postObjectStore(vmThread, clazz, destSlot, value, isVolatile);
	}
}

/**
 * Store a static field.
 * @param destSlot The static slot being used.
 * @param value The value to be stored
 * @param isVolatile non-zero if the field is volatile.
 */
void
MM_ObjectAccessBarrier::staticStoreAddress(J9VMThread *vmThread, J9Class *clazz, void **destSlot,void *value, bool isVolatile)
{
	protectIfVolatileBefore(vmThread, isVolatile, false, true);
	storeAddressImpl(vmThread, NULL, destSlot, value, isVolatile);
	protectIfVolatileAfter(vmThread, isVolatile, false, true);
}

/**
 * Store a static field.
 * @param destSlot The static slot being used.
 * @param value The value to be stored
 * @param isVolatile non-zero if the field is volatile.
 */
void
MM_ObjectAccessBarrier::staticStoreU32(J9VMThread *vmThread, J9Class *clazz, U_32 *destSlot, U_32 value, bool isVolatile)
{
	protectIfVolatileBefore(vmThread, isVolatile, false, true);
	storeU32Impl(vmThread, NULL, destSlot, value, isVolatile);
	protectIfVolatileAfter(vmThread, isVolatile, false, true);
}

/**
 * Store a static field.
 * @param destSlot The static slot being used.
 * @param value The value to be stored
 * @param isVolatile non-zero if the field is volatile.
 */
void
MM_ObjectAccessBarrier::staticStoreI32(J9VMThread *vmThread, J9Class *clazz, I_32 *destSlot, I_32 value, bool isVolatile)
{
	protectIfVolatileBefore(vmThread, isVolatile, false, true);
	storeI32Impl(vmThread, NULL, destSlot, value, isVolatile);
	protectIfVolatileAfter(vmThread, isVolatile, false, true);
}

/**
 * Store a static field.
 * @param destSlot The static slot being used.
 * @param value The value to be stored
 * @param isVolatile non-zero if the field is volatile.
 */
void
MM_ObjectAccessBarrier::staticStoreU64(J9VMThread *vmThread, J9Class *clazz, U_64 *destSlot, U_64 value, bool isVolatile)
{
	protectIfVolatileBefore(vmThread, isVolatile, false, true);
	storeU64Impl(vmThread, NULL, destSlot, value, isVolatile);
	protectIfVolatileAfter(vmThread, isVolatile, false, true);
}

/**
 * Store a static field.
 * @param destSlot The static slot being used.
 * @param value The value to be stored
 * @param isVolatile non-zero if the field is volatile.
 */
void
MM_ObjectAccessBarrier::staticStoreI64(J9VMThread *vmThread, J9Class *clazz, I_64 *destSlot, I_64 value, bool isVolatile)
{
	protectIfVolatileBefore(vmThread, isVolatile, false, true);
	storeI64Impl(vmThread, NULL, destSlot, value, isVolatile);
	protectIfVolatileAfter(vmThread, isVolatile, false, true);
}

/**
 * Return a pointer to the first byte of an array's object data
 * @param arrayObject the base pointer of the array object
 */
 U_8 *
MM_ObjectAccessBarrier::getArrayObjectDataAddress(J9VMThread *vmThread, J9IndexableObject *arrayObject)
{
	 if (_extensions->indexableObjectModel.isInlineContiguousArraylet(arrayObject)) {
		 return (U_8 *)_extensions->indexableObjectModel.getDataPointerForContiguous(arrayObject);
	 } else {
		 return (U_8 *)_extensions->indexableObjectModel.getArrayoidPointer(arrayObject);
	 }
}

/**
 * Return the address of the lockword for the given object, or NULL if it 
 * does not have an inline lockword.
 */
j9objectmonitor_t *
MM_ObjectAccessBarrier::getLockwordAddress(J9VMThread *vmThread, J9Object *object)
{
	j9objectmonitor_t *lockwordAddress = NULL;
	J9Class *clazz = J9OBJECT_CLAZZ(vmThread, object);
	if (!J9_IS_J9CLASS_VALUETYPE(clazz)) {
		UDATA lockOffset = clazz->lockOffset;
		if ((IDATA) lockOffset >= 0) {
			lockwordAddress = (j9objectmonitor_t *)(((U_8 *)object) + lockOffset);
		}
	}
	return lockwordAddress;
}

/**
 * Copy all of the fields of an object into another object.
 * The new object was just allocated inside the VM, so all fields are NULL.
 * @TODO This does not currently check if the fields that it is reading are volatile.
 */
void
MM_ObjectAccessBarrier::cloneObject(J9VMThread *vmThread, J9Object *srcObject, J9Object *destObject)
{
	UDATA elementStartOffset = J9VMTHREAD_OBJECT_HEADER_SIZE(vmThread);
	J9Class *cloneClazz = J9GC_J9OBJECT_CLAZZ_THREAD(srcObject, vmThread);

	elementStartOffset += J9CLASS_PREPADDING_SIZE(cloneClazz);

	copyObjectFields(vmThread, cloneClazz, srcObject, elementStartOffset, destObject, elementStartOffset);
}

BOOLEAN
MM_ObjectAccessBarrier::structuralCompareFlattenedObjects(J9VMThread *vmThread, J9Class *valueClass, j9object_t lhsObject, j9object_t rhsObject, UDATA startOffset)
{
	bool result = true;
	bool const compressed = J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread);
	UDATA const referenceSize = J9VMTHREAD_REFERENCE_SIZE(vmThread);
	bool hasReferences = J9CLASS_HAS_REFERENCES(valueClass);
	/* for non value-types this is just the instance size */
	UDATA limit = J9CLASS_UNPADDED_INSTANCE_SIZE(valueClass);
	UDATA offset = 0;

	Assert_MM_true(J9_IS_J9CLASS_ALLOW_DEFAULT_VALUE(valueClass));

	if (hasReferences) {
		UDATA descriptionIndex = J9_OBJECT_DESCRIPTION_SIZE - 1;
		const UDATA *descriptionPtr = (UDATA *) valueClass->instanceDescription;
		UDATA descriptionBits = 0;

		if (((UDATA)descriptionPtr) & 1) {
			descriptionBits = ((UDATA)descriptionPtr) >> 1;
		} else {
			descriptionBits = *descriptionPtr++;
		}

		while (offset < limit) {
			/* Determine if the slot contains an object pointer or not */
			if (descriptionBits & 1) {
				if (mixedObjectReadObject(vmThread, lhsObject, startOffset + offset, false) != mixedObjectReadObject(vmThread, rhsObject, startOffset + offset, false)) {
					result = false;
					break;
				}
			} else {
				fomrobject_t lhsValue = GC_SlotObject::readSlot((fomrobject_t*)((UDATA)lhsObject + startOffset + offset), compressed);
				fomrobject_t rhsValue = GC_SlotObject::readSlot((fomrobject_t*)((UDATA)rhsObject + startOffset + offset), compressed);
				if (lhsValue != rhsValue) {
					result = false;
					break;
				}
			}
			descriptionBits >>= 1;
			if (descriptionIndex-- == 0) {
				descriptionBits = *descriptionPtr++;
				descriptionIndex = J9_OBJECT_DESCRIPTION_SIZE - 1;
			}
			offset += referenceSize;
		}
	} else {
		/* no instanceDescription bits needed on this path */

		while (offset < limit) {
			fomrobject_t lhsValue = GC_SlotObject::readSlot((fomrobject_t*)((UDATA)lhsObject + startOffset + offset), compressed);
			fomrobject_t rhsValue = GC_SlotObject::readSlot((fomrobject_t*)((UDATA)rhsObject + startOffset + offset), compressed);
			if (lhsValue != rhsValue) {
				result = false;
				break;
			}
			offset += referenceSize;
		}
	}

	return result;
}

/**
 * Copy all of the fields of a value class instance to another value class instance.
 * The source or destination may be a flattened value within another object, meaning
 * srcOffset and destOffset need not be equal. This is based on cloneObject(...). If
 * Type has pre-padding the size of the object will be adjusted to remove the padding
 * bytes. The caller of this API must ensure that the starting offset provided does
 * not include pre-padding.
 * @TODO This does not currently check if the fields that it is reading are volatile.
 *
 * @oaram objectClass The j9class.
 * @param srcObject The object containing the value class instance fields being copied.
 * @param srcOffset The offset of the value class instance fields in srcObject.
 * @param destValue The object containing the value class instance fields being copied to.
 * @param destOffset The offset of the value class instance fields in destObject.
 * @param objectMapFunction Function to allow replacement of object fields
 * @param objectMapData Data to pass to objectMapFunction
 * @param initializeLockWord true to initialize inline lockword, false to copy it
 */
void
MM_ObjectAccessBarrier::copyObjectFields(J9VMThread *vmThread, J9Class *objectClass, J9Object *srcObject, UDATA srcOffset, J9Object *destObject, UDATA destOffset, MM_objectMapFunction objectMapFunction, void *objectMapData, bool initializeLockWord)
{
	/* For valueTypes we currently do not make a distinction between values that only contain
	 * primitives and values that may contain a reference (ie. value vs mixed-value
	 * in packedObject terminology). As a result, we will treat all values as if
	 * they may contain references. In the future this may change.
	 *
	 * Value types have no need or lockwords, however, they are still present in the
	 * current implementation. For now we will just skip over them by specifying
	 * appropriate offsets. We will also skip over the bit in the instance description.
	 */
	bool isValueType = J9_IS_J9CLASS_VALUETYPE(objectClass);

	j9objectmonitor_t *lockwordAddress = NULL;
	I_32 hashCode = 0;
	bool isDestObjectPreHashed = false;

	isDestObjectPreHashed = _extensions->objectModel.hasBeenHashed(destObject);
	if (isDestObjectPreHashed) {
		hashCode = _extensions->objectModel.getObjectHashCode(vmThread->javaVM, destObject);
	}

	UDATA offset = 0;
	/* for non value-types this is just the instance size */
	UDATA limit = J9CLASS_UNPADDED_INSTANCE_SIZE(objectClass);
	UDATA const referenceSize = J9VMTHREAD_REFERENCE_SIZE(vmThread);
	bool hasReferences = J9CLASS_HAS_REFERENCES(objectClass);

	if (hasReferences) {
		const UDATA *descriptionPtr = (UDATA *) objectClass->instanceDescription;
		UDATA descriptionBits = 0;
		if (((UDATA)descriptionPtr) & 1) {
			descriptionBits = ((UDATA)descriptionPtr) >> 1;
		} else {
			descriptionBits = *descriptionPtr++;
		}

		UDATA descriptionIndex = J9_OBJECT_DESCRIPTION_SIZE - 1;

		while (offset < limit) {
			/* Determine if the slot contains an object pointer or not */
			if (descriptionBits & 1) {
				J9Object *objectPtr = mixedObjectReadObject(vmThread, srcObject, srcOffset + offset, false);
				if (NULL != objectMapFunction) {
					objectPtr = objectMapFunction(vmThread, objectPtr, objectMapData);
				}
				mixedObjectStoreObject(vmThread, destObject, destOffset + offset, objectPtr, false);
			} else {
				UDATA srcAddress = (UDATA)srcObject + srcOffset + offset;
				UDATA destAddress = (UDATA)destObject + destOffset + offset;
				bool copy64Bits = false;
				UDATA descriptionBitsNext = descriptionBits;
				const UDATA *descriptionPtrNext = descriptionPtr;
				UDATA descriptionIndexNext = descriptionIndex;
				if (isValueType
					&& (sizeof(uint32_t) == referenceSize)
					&& (0 == (srcAddress & 7))
					&& ((offset + referenceSize) < limit)
				) {
					descriptionBitsNext >>= 1;
					if (descriptionIndexNext-- == 0) {
						descriptionBitsNext = *(descriptionPtrNext++);
						descriptionIndexNext = J9_OBJECT_DESCRIPTION_SIZE - 1;
					}
					if (0 == (descriptionBitsNext & 1)) {
						copy64Bits = true;
					}
				}
				if (copy64Bits) {
					*(uint64_t *)destAddress = *(uint64_t *)srcAddress;
					descriptionBits = descriptionBitsNext;
					descriptionPtr = descriptionPtrNext;
					descriptionIndex = descriptionIndexNext;
					/**
					 * When doing 64-bit copy, offset needs to be advanced 8 bytes. referenceSize is 4 bytes here.
					 * Advance offset 4 bytes here. Another 4 bytes are advanced at the end of the while loop below.
					 */
					offset += referenceSize;
				} else if (sizeof(uint32_t) == referenceSize) {
					*(uint32_t *)destAddress = *(uint32_t *)srcAddress;
				} else {
					*(uintptr_t *)destAddress = *(uintptr_t *)srcAddress;
				}
			}
			descriptionBits >>= 1;
			if (descriptionIndex-- == 0) {
				descriptionBits = *descriptionPtr++;
				descriptionIndex = J9_OBJECT_DESCRIPTION_SIZE - 1;
			}
			offset += referenceSize;
		}
	} else {
		/* no instanceDescription bits needed on this path */
		while (offset < limit) {
			UDATA srcAddress = (UDATA)srcObject + srcOffset + offset;
			UDATA destAddress = (UDATA)destObject + destOffset + offset;
			/* prefer to copy 64 bits at a time if possible */
			if ((sizeof(uint64_t) == referenceSize)
				|| (isValueType && (0 == (srcAddress & 7)) && ((offset + referenceSize) < limit))
			) {
				*(uint64_t *)destAddress = *(uint64_t *)srcAddress;
				offset += sizeof(uint64_t);
			} else {
				*(uint32_t *)destAddress = *(uint32_t *)srcAddress;
				offset += sizeof(uint32_t);
			}
		}
	}

	/* If an object was pre-hashed and a hash was stored within the fields of the object restore it.*/
	if (isDestObjectPreHashed) {
		UDATA hashcodeOffset = _extensions->mixedObjectModel.getHashcodeOffset(destObject);
		if (hashcodeOffset <= limit) {
			I_32 *hashcodePointer = (I_32*)((U_8*)destObject + hashcodeOffset);
			*hashcodePointer = hashCode;
		}
	}
	if (!isValueType) {
		if (initializeLockWord) {
			/* initialize lockword, if present */
			lockwordAddress = getLockwordAddress(vmThread, destObject);
			if (NULL != lockwordAddress) {
				j9objectmonitor_t lwValue = VM_ObjectMonitor::getInitialLockword(vmThread->javaVM, objectClass);
				J9_STORE_LOCKWORD(vmThread, lockwordAddress, lwValue);
			}
		}
	}
}

/**
 * Copy all of the fields of an indexable object into another indexable object.
 * The new object was just allocated inside the VM, so all fields are NULL.
 * @TODO This does not currently check if the fields that it is reading are volatile.
 */
void 
MM_ObjectAccessBarrier::cloneIndexableObject(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, MM_objectMapFunction objectMapFunction, void *objectMapData)
{
	bool isObjectArray = _extensions->objectModel.isObjectArray(srcObject);

	if (_extensions->objectModel.hasBeenHashed((J9Object*)destObject)) {
		/* this assertion should never be triggered because we never pre-hash arrays  */
		Assert_MM_unreachable();
	}
	
	if (isObjectArray) {
		I_32 size = (I_32)_extensions->indexableObjectModel.getSizeInElements(srcObject);
		for (I_32 i = 0; i < size; i++) {
			J9Object *objectPtr = J9JAVAARRAYOFOBJECT_LOAD(vmThread, srcObject, i);
			if (NULL != objectMapFunction) {
				objectPtr = objectMapFunction(vmThread, objectPtr, objectMapData);
			}
			J9JAVAARRAYOFOBJECT_STORE(vmThread, destObject, i, objectPtr);
		}
	} else {
		_extensions->indexableObjectModel.memcpyArray(destObject, srcObject);
	}

}


/* Return an j9object_t that can be stored in the constantpool.
 *
 * Not all collectors scan the constantpool on every GC and therefore for
 * these collectors the objects must be in tenure space.
 *
 * Note, the stack must be walkable as a GC may occur during this function.
 * 
 * Note, this doesn't handle arrays.
 *
 * @param vmThread The current vmThread
 * @param toConvert The object to convert to a constantpool allowed form.
 *
 * @return an object that can be put in the constantpool or null if OOM.
 */
J9Object*
MM_ObjectAccessBarrier::asConstantPoolObject(J9VMThread *vmThread, J9Object* toConvert, UDATA allocationFlags)
{
	Assert_MM_true(allocationFlags & (J9_GC_ALLOCATE_OBJECT_TENURED | J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE));
	return toConvert;
}

/**
 * Write an object to an internal VM slot (J9VMThread, J9JavaVM, named field of J9Class).
 * @param destSlot the slot to be used
 * @param value the value to be stored
 */
void 
MM_ObjectAccessBarrier::storeObjectToInternalVMSlot(J9VMThread *vmThread, J9Object** destSlot, J9Object *value)
{
	if (preObjectStore(vmThread, destSlot, value, false)) {
		storeObjectToInternalVMSlotImpl(vmThread, destSlot, value, false);
		postObjectStore(vmThread, destSlot, value, false);
	}
}
 
/**
 * Read an object from an internal VM slot (J9VMThread, J9JavaVM, named field of J9Class).
 * @param srcSlot the slot to be used
 */
J9Object *
MM_ObjectAccessBarrier::readObjectFromInternalVMSlot(J9VMThread *vmThread, J9Object **srcSlot)
{
	return readObjectFromInternalVMSlotImpl(vmThread, srcSlot, false);
}

/**
 * compareAndSwapObject performs an atomic compare-and-swap on an object field or array element.
 * @param destObject the object containing the field being swapped into
 * @param destAddress the address of the destination field of the operation
 * @param compareObject the object to be compared with contents of destSlot
 * @param swapObject the object to be stored in the destSlot if compareObject is there now
 * @todo This should be converted to take the offset, not the address
 **/ 
bool 
MM_ObjectAccessBarrier::compareAndSwapObject(J9VMThread *vmThread, J9Object *destObject, fj9object_t *destAddress, J9Object *compareObject, J9Object *swapObject)
{
	fj9object_t *actualDestAddress;
	fj9object_t compareValue = convertTokenFromPointer(compareObject);
	fj9object_t swapValue = convertTokenFromPointer(swapObject);
	bool result = false;

	/* TODO: To make this API more consistent, it should probably be split into separate
	 * indexable and non-indexable versions. Currently, when called on an indexable object,
	 * the REAL address is passed. For non-indexable objects, the address is the shadow
	 * address
	 */
	if (_extensions->objectModel.isIndexable(destObject)) {
		actualDestAddress = destAddress;
	} else {
		actualDestAddress = J9OAB_MIXEDOBJECT_EA(destObject, ((UDATA)destAddress - (UDATA)destObject), fj9object_t);
	}

	if (preObjectRead(vmThread, destObject, actualDestAddress)) {
		/* Note: This is a bit of a special case -- we call preObjectStore even though the store
		 * may not actually occur. This is safe and correct for Metronome.
		 */
		preObjectStore(vmThread, destObject, actualDestAddress, swapObject, true);
		protectIfVolatileBefore(vmThread, true, false, false);

		if (J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread)) {
			result = ((U_32)(UDATA)compareValue == MM_AtomicOperations::lockCompareExchangeU32((U_32 *)actualDestAddress, (U_32)(UDATA)compareValue, (U_32)(UDATA)swapValue));
		} else {
			result = ((UDATA)compareValue == MM_AtomicOperations::lockCompareExchange((UDATA *)actualDestAddress, (UDATA)compareValue, (UDATA)swapValue));
		}
		protectIfVolatileAfter(vmThread, true, false, false);
		if (result) {
			postObjectStore(vmThread, destObject, actualDestAddress, swapObject, true);
		}
	}

	return result;
}

/**
 * staticCompareAndSwapObject performs an atomic compare-and-swap on a static object field.
 * @param destClass the class containing the statics field being swapped into
 * @param destAddress the address of the destination field of the operation
 * @param compareObject the object to be compared with contents of destSlot
 * @param swapObject the object to be stored in the destSlot if compareObject is there now
 * @todo This should be converted to take the offset, not the address
 **/
bool
MM_ObjectAccessBarrier::staticCompareAndSwapObject(J9VMThread *vmThread, J9Class *destClass, j9object_t *destAddress, J9Object *compareObject, J9Object *swapObject)
{
	bool result = false;

	if (preObjectRead(vmThread, destClass, destAddress)) {
		/* Note: This is a bit of a special case -- we call preObjectStore even though the store
		 * may not actually occur. This is safe and correct for Metronome.
		 */
		preObjectStore(vmThread, (J9Object *)J9VM_J9CLASS_TO_HEAPCLASS(destClass), destAddress, swapObject, true);
		protectIfVolatileBefore(vmThread, true, false, false);

		result = ((UDATA)compareObject == MM_AtomicOperations::lockCompareExchange((UDATA *)destAddress, (UDATA)compareObject, (UDATA)swapObject));

		protectIfVolatileAfter(vmThread, true, false, false);
		if (result) {
			postObjectStore(vmThread, destClass, destAddress, swapObject, true);
		}
	}

	return result;
}

/**
 * Performs an atomic compare-and-swap on an int field of a mixed object
 * @param destObject the object containing the field being swapped into
 * @param destAddress the address of the destination field of the operation
 * @param compareValue the value to be compared with contents of destSlot
 * @param swapValue the value to be stored in the destSlot if compareValue is there now
 **/ 
bool 
MM_ObjectAccessBarrier::mixedObjectCompareAndSwapInt(J9VMThread *vmThread, J9Object *destObject, UDATA offset, U_32 compareValue, U_32 swapValue)
{	
	U_32 *actualDestAddress = J9OAB_MIXEDOBJECT_EA(destObject, offset, U_32);

	protectIfVolatileBefore(vmThread, true, false, false);
	bool result = (compareValue == MM_AtomicOperations::lockCompareExchangeU32(actualDestAddress, compareValue, swapValue));
	protectIfVolatileAfter(vmThread, true, false, false);
	return result;
}

/**
 * Performs an atomic compare-and-swap on a static int field.
 * @param destClass the class containing the statics field being swapped into
 * @param destAddress the address of the destination field of the operation
 * @param compareValue the value to be compared with contents of destSlot
 * @param swapValue the value to be stored in the destSlot if compareValue is there now
 **/ 
bool 
MM_ObjectAccessBarrier::staticCompareAndSwapInt(J9VMThread *vmThread, J9Class *destClass, U_32 *destAddress, U_32 compareValue, U_32 swapValue)
{	
	protectIfVolatileBefore(vmThread, true, false, false);
	bool result = (compareValue == MM_AtomicOperations::lockCompareExchangeU32(destAddress, compareValue, swapValue));
	protectIfVolatileAfter(vmThread, true, false, false);
	return result;
}

/**
 * Performs an atomic compare-and-swap on an long field of a mixed object
 * @param destObject the object containing the field being swapped into
 * @param destAddress the address of the destination field of the operation
 * @param compareValue the value to be compared with contents of destSlot
 * @param swapValue the value to be stored in the destSlot if compareValue is there now
 **/ 
bool 
MM_ObjectAccessBarrier::mixedObjectCompareAndSwapLong(J9VMThread *vmThread, J9Object *destObject, UDATA offset, U_64 compareValue, U_64 swapValue)
{	
	U_64 *actualDestAddress = J9OAB_MIXEDOBJECT_EA(destObject, offset, U_64);

	protectIfVolatileBefore(vmThread, true, false, true);
	bool result = (compareValue == MM_AtomicOperations::lockCompareExchangeU64(actualDestAddress, compareValue, swapValue));
	protectIfVolatileAfter(vmThread, true, false, true);
	return result;
}

/**
 * Performs an atomic compare-and-swap on a static long field.
 * @param destClass the class containing the statics field being swapped into
 * @param destAddress the address of the destination field of the operation
 * @param compareValue the value to be compared with contents of destSlot
 * @param swapValue the value to be stored in the destSlot if compareValue is there now
 **/ 
bool 
MM_ObjectAccessBarrier::staticCompareAndSwapLong(J9VMThread *vmThread, J9Class *destClass, U_64 *destAddress, U_64 compareValue, U_64 swapValue)
{	
	protectIfVolatileBefore(vmThread, true, false, true);
	bool result = (compareValue == MM_AtomicOperations::lockCompareExchangeU64(destAddress, compareValue, swapValue));
	protectIfVolatileAfter(vmThread, true, false, true);
	return result;
}

/**
 * Performs an atomic compare-and-exchange on an object field or array element.
 * @param destObject the object containing the field being swapped into
 * @param destAddress the address of the destination field of the operation
 * @param compareObject the object to be compared with contents of destSlot
 * @param swapObject the object to be stored in the destSlot if compareObject is there now
 * @return the object stored in the object field before the update
 * @todo This should be converted to take the offset, not the address
 **/
J9Object *
MM_ObjectAccessBarrier::compareAndExchangeObject(J9VMThread *vmThread, J9Object *destObject, fj9object_t *destAddress, J9Object *compareObject, J9Object *swapObject)
{
	fj9object_t *actualDestAddress;
	fj9object_t compareValue = convertTokenFromPointer(compareObject);
	fj9object_t swapValue = convertTokenFromPointer(swapObject);
	J9Object *result = NULL;

	/* TODO: To make this API more consistent, it should probably be split into separate
	 * indexable and non-indexable versions. Currently, when called on an indexable object,
	 * the REAL address is passed. For non-indexable objects, the address is the shadow
	 * address
	 */
	if (_extensions->objectModel.isIndexable(destObject)) {
		actualDestAddress = destAddress;
	} else {
		actualDestAddress = J9OAB_MIXEDOBJECT_EA(destObject, ((UDATA)destAddress - (UDATA)destObject), fj9object_t);
	}

	if (preObjectRead(vmThread, destObject, actualDestAddress)) {
		/* Note: This is a bit of a special case -- we call preObjectStore even though the store
		 * may not actually occur. This is safe and correct for Metronome.
		 */
		preObjectStore(vmThread, destObject, actualDestAddress, swapObject, true);
		protectIfVolatileBefore(vmThread, true, false, false);

		if (J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread)) {
			result = (J9Object *)(UDATA)MM_AtomicOperations::lockCompareExchangeU32((U_32 *)actualDestAddress, (U_32)(UDATA)compareValue, (U_32)(UDATA)swapValue);
		} else {
			result = (J9Object *)MM_AtomicOperations::lockCompareExchange((UDATA *)actualDestAddress, (UDATA)compareValue, (UDATA)swapValue);
		}

		protectIfVolatileAfter(vmThread, true, false, false);
		if (result) {
			postObjectStore(vmThread, destObject, actualDestAddress, swapObject, true);
		}
	}

	return result;
}

/**
 * Performs an atomic compare-and-exchange on a static object field.
 * @param destClass the class containing the statics field being swapped into
 * @param destAddress the address of the destination field of the operation
 * @param compareObject the object to be compared with contents of destSlot
 * @param swapObject the object to be stored in the destSlot if compareObject is there now
 * @return the object stored in the object field before the update
 * @todo This should be converted to take the offset, not the address
 **/
J9Object *
MM_ObjectAccessBarrier::staticCompareAndExchangeObject(J9VMThread *vmThread, J9Class *destClass, j9object_t *destAddress, J9Object *compareObject, J9Object *swapObject)
{
	J9Object *result = NULL;

	if (preObjectRead(vmThread, destClass, destAddress)) {
		/* Note: This is a bit of a special case -- we call preObjectStore even though the store
		 * may not actually occur. This is safe and correct for Metronome.
		 */
		preObjectStore(vmThread, (J9Object *)J9VM_J9CLASS_TO_HEAPCLASS(destClass), destAddress, swapObject, true);
		protectIfVolatileBefore(vmThread, true, false, false);

		result = (J9Object *)MM_AtomicOperations::lockCompareExchange((UDATA *)destAddress, (UDATA)compareObject, (UDATA)swapObject);

		protectIfVolatileAfter(vmThread, true, false, false);
		if (result) {
			postObjectStore(vmThread, destClass, destAddress, swapObject, true);
		}
	}

	return result;
}

/**
 * Performs an atomic compare-and-exchange on an int field of a mixed object
 * @param destObject the object containing the field being swapped into
 * @param destAddress the address of the destination field of the operation
 * @param compareValue the value to be compared with contents of destSlot
 * @param swapValue the value to be stored in the destSlot if compareValue is there now
 * @return the int stored in the object field before the update
 **/
U_32
MM_ObjectAccessBarrier::mixedObjectCompareAndExchangeInt(J9VMThread *vmThread, J9Object *destObject, UDATA offset, U_32 compareValue, U_32 swapValue)
{
	U_32 *actualDestAddress = J9OAB_MIXEDOBJECT_EA(destObject, offset, U_32);

	protectIfVolatileBefore(vmThread, true, false, false);
	U_32 result = MM_AtomicOperations::lockCompareExchangeU32(actualDestAddress, compareValue, swapValue);
	protectIfVolatileAfter(vmThread, true, false, false);
	return result;
}

/**
 * Performs an atomic compare-and-exchange on a static int field.
 * @param destClass the class containing the statics field being swapped into
 * @param destAddress the address of the destination field of the operation
 * @param compareValue the value to be compared with contents of destSlot
 * @param swapValue the value to be stored in the destSlot if compareValue is there now
 * @return the int stored in the object field before the update
 **/
U_32
MM_ObjectAccessBarrier::staticCompareAndExchangeInt(J9VMThread *vmThread, J9Class *destClass, U_32 *destAddress, U_32 compareValue, U_32 swapValue)
{
	protectIfVolatileBefore(vmThread, true, false, false);
	U_32 result = MM_AtomicOperations::lockCompareExchangeU32(destAddress, compareValue, swapValue);
	protectIfVolatileAfter(vmThread, true, false, false);
	return result;
}

/**
 * Performs an atomic compare-and-exchange on an long field of a mixed object
 * @param destObject the object containing the field being swapped into
 * @param destAddress the address of the destination field of the operation
 * @param compareValue the value to be compared with contents of destSlot
 * @param swapValue the value to be stored in the destSlot if compareValue is there now
 * @return the long stored in the object field before the update
 **/
U_64
MM_ObjectAccessBarrier::mixedObjectCompareAndExchangeLong(J9VMThread *vmThread, J9Object *destObject, UDATA offset, U_64 compareValue, U_64 swapValue)
{
	U_64 *actualDestAddress = J9OAB_MIXEDOBJECT_EA(destObject, offset, U_64);

	protectIfVolatileBefore(vmThread, true, false, true);
	U_64 result = MM_AtomicOperations::lockCompareExchangeU64(actualDestAddress, compareValue, swapValue);
	protectIfVolatileAfter(vmThread, true, false, true);
	return result;
}

/**
 * Performs an atomic compare-and-exchange on a static long field.
 * @param destClass the class containing the statics field being swapped into
 * @param destAddress the address of the destination field of the operation
 * @param compareValue the value to be compared with contents of destSlot
 * @param swapValue the value to be stored in the destSlot if compareValue is there now
 * @return the long stored in the object field before the update
 **/
U_64
MM_ObjectAccessBarrier::staticCompareAndExchangeLong(J9VMThread *vmThread, J9Class *destClass, U_64 *destAddress, U_64 compareValue, U_64 swapValue)
{
	protectIfVolatileBefore(vmThread, true, false, true);
	U_64 result = MM_AtomicOperations::lockCompareExchangeU64(destAddress, compareValue, swapValue);
	protectIfVolatileAfter(vmThread, true, false, true);
	return result;
}

/**
 * Called before an object is stored into another object.
 * @return true if the store should proceed, false otherwise
 */
bool
MM_ObjectAccessBarrier::preObjectStore(J9VMThread *vmThread, J9Object *destObject, fj9object_t *destAddress, J9Object *value, bool isVolatile)
{
	return true;
}

/**
 * Called before an object is stored into a class.
 * @return true if the store should proceed, false otherwise
 */
bool
MM_ObjectAccessBarrier::preObjectStore(J9VMThread *vmThread, J9Object *destObject, J9Object **destAddress, J9Object *value, bool isVolatile)
{
	return true;
}

/**
 * Called before an object is stored into an internal VM structure.
 * @return true if the store should proceed, false otherwise
 */
bool 
MM_ObjectAccessBarrier::preObjectStore(J9VMThread *vmThread, J9Object **destAddress, J9Object *value, bool isVolatile)
{
	return true;
}

/**
 * Called after an object is stored into another object.
 */
void
MM_ObjectAccessBarrier::postObjectStore(J9VMThread *vmThread, J9Object *destObject, fj9object_t *destAddress, J9Object *value, bool isVolatile)
{
}

/**
 * Called after an object is stored into a class.
 */
void
MM_ObjectAccessBarrier::postObjectStore(J9VMThread *vmThread, J9Class *destObject, J9Object **destAddress, J9Object *value, bool isVolatile)
{
}

/**
 * Called after an object is stored into an internal VM structure.
 */
void 
MM_ObjectAccessBarrier::postObjectStore(J9VMThread *vmThread, J9Object **destAddress, J9Object *value, bool isVolatile)
{
}

/**
 * TODO: This should probably be postBatchObjectStore, not pre-.
 */
bool
MM_ObjectAccessBarrier::postBatchObjectStore(J9VMThread *vmThread, J9Object *destObject, bool isVolatile)
{
#if defined(J9VM_GC_COMBINATION_SPEC)
	/* (assert here to verify that we aren't defaulting to this implementation through some unknown path - delete once combination is stable) */
	Assert_MM_unreachable();
#endif /* defined(J9VM_GC_COMBINATION_SPEC) */
	return true;
}

bool
MM_ObjectAccessBarrier::postBatchObjectStore(J9VMThread *vmThread, J9Class *destClass, bool isVolatile)
{
#if defined(J9VM_GC_COMBINATION_SPEC)
	/* (assert here to verify that we aren't defaulting to this implementation through some unknown path - delete once combination is stable) */
	Assert_MM_unreachable();
#endif /* defined(J9VM_GC_COMBINATION_SPEC) */
	return true;
}

bool
MM_ObjectAccessBarrier::preObjectRead(J9VMThread *vmThread, J9Object *srcObject, fj9object_t *srcAddress)
{
	return true;
}

bool
MM_ObjectAccessBarrier::preWeakRootSlotRead(J9VMThread *vmThread, j9object_t *srcAddress)
{
	return true;
}

bool
MM_ObjectAccessBarrier::preWeakRootSlotRead(J9JavaVM *vm, j9object_t *srcAddress)
{
	return true;
}


bool
MM_ObjectAccessBarrier::preObjectRead(J9VMThread *vmThread, J9Class *srcClass, J9Object **srcAddress)
{
	return true;
}

bool
MM_ObjectAccessBarrier::postObjectRead(J9VMThread *vmThread, J9Object *srcObject, fj9object_t *srcAddress)
{
	return true;
}

bool
MM_ObjectAccessBarrier::postObjectRead(J9VMThread *vmThread, J9Class *srcClass, J9Object **srcAddress)
{	
	return true;
}

/**
 * Fills array (or part of array) with specific object value.
 * Example, compressed pointers access barrier will mangle the value
 * pointer before filling up the array with it.
 */
void 
MM_ObjectAccessBarrier::fillArrayOfObjects(J9VMThread *vmThread, j9array_t destObject, I_32 destIndex, I_32 count, j9object_t value)
{
#if defined(J9VM_GC_COMBINATION_SPEC)
	/* (assert here to verify that we aren't defaulting to this implementation through some unknown path - delete once combination is stable) */
	Assert_MM_unreachable();
#endif /* defined(J9VM_GC_COMBINATION_SPEC) */
	if (J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread)) {
		uint32_t *destPtr = (uint32_t *)indexableEffectiveAddress(vmThread, destObject, destIndex, sizeof(uint32_t));
		uint32_t actualValue = (uint32_t)convertTokenFromPointer(value);
		uint32_t *endPtr = destPtr + count;
	
		while (destPtr < endPtr) {
			*destPtr++ = actualValue;	
		}
	} else {
		uintptr_t *destPtr = (uintptr_t *)indexableEffectiveAddress(vmThread, destObject, destIndex, sizeof(uintptr_t));
		uintptr_t actualValue = (uintptr_t)convertTokenFromPointer(value);
		uintptr_t *endPtr = destPtr + count;
	
		while (destPtr < endPtr) {
			*destPtr++ = actualValue;	
		}
	}
}

/**
 * Returns the shadow heap base for compressed pointers.
 * Used by the JIT in the interim solution until we store J9 class objects
 * on the heap.
 * @return shadow heap base.
 */
UDATA
MM_ObjectAccessBarrier::compressedPointersShadowHeapBase(J9VMThread *vmThread)
{
	assume0(false);	
	return 0;
}

/**
 * Returns the shadow heap base for compressed pointers.
 * Used by the JIT in the interim solution until we store J9 class objects
 * on the heap.
 * @return shadow heap top.
 */
UDATA
MM_ObjectAccessBarrier::compressedPointersShadowHeapTop(J9VMThread *vmThread)
{
	assume0(false);	
	return 0;
}

/**
 * @return this cannot fail (overloaded can) => returns ARRAY_COPY_NOT_DONE
 */
I_32
MM_ObjectAccessBarrier::doCopyContiguousBackward(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots)
{
	srcIndex += lengthInSlots;
	destIndex += lengthInSlots;

	if (J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread)) {
		uint32_t *srcSlot = (uint32_t *)indexableEffectiveAddress(vmThread, srcObject, srcIndex, sizeof(uint32_t));
		uint32_t *destSlot = (uint32_t *)indexableEffectiveAddress(vmThread, destObject, destIndex, sizeof(uint32_t));
		uint32_t *srcEndSlot = srcSlot - lengthInSlots;
		
		while (srcSlot > srcEndSlot) {
			*--destSlot = *--srcSlot;
		}
	} else {
		uintptr_t *srcSlot = (uintptr_t *)indexableEffectiveAddress(vmThread, srcObject, srcIndex, sizeof(uintptr_t));
		uintptr_t *destSlot = (uintptr_t *)indexableEffectiveAddress(vmThread, destObject, destIndex, sizeof(uintptr_t));
		uintptr_t *srcEndSlot = srcSlot - lengthInSlots;
		
		while (srcSlot > srcEndSlot) {
			*--destSlot = *--srcSlot;
		}	
	}
	
	return ARRAY_COPY_SUCCESSFUL;
}

/**
 * @return this cannot fail (overloaded can) => returns ARRAY_COPY_NOT_DONE
 */
I_32
MM_ObjectAccessBarrier::doCopyContiguousForward(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots)
{
	if (J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread)) {
		uint32_t *srcSlot = (uint32_t *)indexableEffectiveAddress(vmThread, srcObject, srcIndex, sizeof(uint32_t));
		uint32_t *destSlot = (uint32_t *)indexableEffectiveAddress(vmThread, destObject, destIndex, sizeof(uint32_t));
		uint32_t *srcEndSlot = srcSlot + lengthInSlots;
		
		while (srcSlot < srcEndSlot) {
			*destSlot++ = *srcSlot++;
		}
	} else {
		uintptr_t *srcSlot = (uintptr_t *)indexableEffectiveAddress(vmThread, srcObject, srcIndex, sizeof(uintptr_t));
		uintptr_t *destSlot = (uintptr_t *)indexableEffectiveAddress(vmThread, destObject, destIndex, sizeof(uintptr_t));
		uintptr_t *srcEndSlot = srcSlot + lengthInSlots;
		
		while (srcSlot < srcEndSlot) {
			*destSlot++ = *srcSlot++;
		}
	}
	
	return ARRAY_COPY_SUCCESSFUL;	
}

I_32
MM_ObjectAccessBarrier::getObjectHashCode(J9JavaVM *vm, J9Object *object)
{
	return _extensions->objectModel.getObjectHashCode(vm, object);
}

void
MM_ObjectAccessBarrier::setFinalizeLink(j9object_t object, j9object_t value)
{
	fj9object_t* finalizeLink = getFinalizeLinkAddress(object);
	GC_SlotObject slot(_extensions->getOmrVM(), finalizeLink);
	slot.writeReferenceToSlot(value);
}

void 
MM_ObjectAccessBarrier::setReferenceLink(j9object_t object, j9object_t value)
{
	Assert_MM_true(NULL != object);
	UDATA linkOffset = _referenceLinkOffset; 
	/* offset will be UDATA_MAX until java/lang/ref/Reference is loaded */
	Assert_MM_true(UDATA_MAX != linkOffset);
	fj9object_t *referenceLink = (fj9object_t*)((UDATA)object + linkOffset);
	GC_SlotObject slot(_extensions->getOmrVM(), referenceLink);
	slot.writeReferenceToSlot(value);
}

void
MM_ObjectAccessBarrier::setOwnableSynchronizerLink(j9object_t object, j9object_t value)
{
	Assert_MM_true(NULL != object);
	UDATA linkOffset = _ownableSynchronizerLinkOffset;
	/* offset will be UDATA_MAX until java/util/concurrent/locks/AbstractOwnableSynchronizer is loaded */
	Assert_MM_true(UDATA_MAX != linkOffset);
	if (NULL == value) {
		/* set the last object in the list pointing to itself */
		value = object;
	}
	fj9object_t *ownableSynchronizerLink = (fj9object_t*)((UDATA)object + linkOffset);
	GC_SlotObject slot(_extensions->getOmrVM(), ownableSynchronizerLink);
	slot.writeReferenceToSlot(value);
}

void
MM_ObjectAccessBarrier::setContinuationLink(j9object_t object, j9object_t value)
{
	Assert_MM_true(NULL != object);
	UDATA linkOffset = _continuationLinkOffset;
	/* offset will be UDATA_MAX until Continuation is loaded */
	Assert_MM_true(UDATA_MAX != linkOffset);
	fj9object_t *continuationLink = (fj9object_t*)((UDATA)object + linkOffset);
	GC_SlotObject slot(_extensions->getOmrVM(), continuationLink);
	slot.writeReferenceToSlot(value);
}

void
MM_ObjectAccessBarrier::printNativeMethod(J9VMThread* vmThread)
{
	J9SFJNINativeMethodFrame *nativeMethodFrame = VM_VMHelpers::findNativeMethodFrame(vmThread);
	J9Method *method = nativeMethodFrame->method;
	J9JavaVM *javaVM = vmThread->javaVM;
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	if (NULL != method) {
		J9UTF8 * className = J9ROMCLASS_CLASSNAME(J9_CP_FROM_METHOD(method)->ramClass->romClass);
		J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
		J9UTF8 * name = J9ROMMETHOD_NAME(romMethod);
		J9UTF8 * sig = J9ROMMETHOD_SIGNATURE(romMethod);

		j9tty_printf(PORTLIB, "%p: Native Method %p (%.*s.%.*s%.*s)\n",
				vmThread, method,
				(U_32) J9UTF8_LENGTH(className), J9UTF8_DATA(className), (U_32) J9UTF8_LENGTH(name), J9UTF8_DATA(name), (U_32) J9UTF8_LENGTH(sig), J9UTF8_DATA(sig));

		Trc_MM_ObjectAccessBarrier_printNativeMethod(vmThread, method,
				(U_32) J9UTF8_LENGTH(className), J9UTF8_DATA(className), (U_32) J9UTF8_LENGTH(name), J9UTF8_DATA(name), (U_32) J9UTF8_LENGTH(sig), J9UTF8_DATA(sig));
	} else {
		j9tty_printf(PORTLIB, "%p: Native Method Unknown\n", vmThread);
		Trc_MM_ObjectAccessBarrier_printNativeMethodUnknown(vmThread);
	}
}


