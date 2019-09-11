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


/**
 * @file
 * @ingroup GC_Modron_Base
 */

#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "j9protos.h"
#include "ModronAssertions.h"

#include "VLHGCAccessBarrier.hpp"
#include "AtomicOperations.hpp"
#include "CardTable.hpp"
#include "Debug.hpp"
#include "EnvironmentVLHGC.hpp"
#include "mmhook_internal.h"
#include "GCExtensions.hpp"
#include "HeapRegionManager.hpp"
#include "IncrementalGenerationalGC.hpp"
#include "JNICriticalRegion.hpp"
#include "ObjectModel.hpp"
#include "SublistFragment.hpp"

MM_VLHGCAccessBarrier *
MM_VLHGCAccessBarrier::newInstance(MM_EnvironmentBase *env)
{
	MM_VLHGCAccessBarrier *barrier;
	
	barrier = (MM_VLHGCAccessBarrier *)env->getForge()->allocate(sizeof(MM_VLHGCAccessBarrier), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (barrier) {
		new(barrier) MM_VLHGCAccessBarrier(env);
		if (!barrier->initialize(env)) {
			barrier->kill(env);
			barrier = NULL;
		}
	}
	return barrier;
}

bool 
MM_VLHGCAccessBarrier::initialize(MM_EnvironmentBase *env)
{
	return MM_ObjectAccessBarrier::initialize(env);
}

void 
MM_VLHGCAccessBarrier::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

void
MM_VLHGCAccessBarrier::tearDown(MM_EnvironmentBase *env)
{
	MM_ObjectAccessBarrier::tearDown(env);
}

/**
 * Called after an object is stored into another object.
 */
void
MM_VLHGCAccessBarrier::postObjectStore(J9VMThread *vmThread, J9Object *destObject, fj9object_t *destAddress, J9Object *value, bool isVolatile)
{
	postObjectStoreImpl(vmThread, destObject, value);
}

/**
 * Called after an object is stored into a class.
 */
void
MM_VLHGCAccessBarrier::postObjectStore(J9VMThread *vmThread, J9Class *destClass, J9Object **destAddress, J9Object *value, bool isVolatile)
{
	j9object_t destObject = J9VM_J9CLASS_TO_HEAPCLASS(destClass);

	/* destObject is guaranteed to be in old space, so the common code path will remember objects appropriately here */
	postObjectStoreImpl(vmThread, destObject, value);
}

bool 
MM_VLHGCAccessBarrier::preBatchObjectStore(J9VMThread *vmThread, J9Object *destObject, bool isVolatile)
{
	preBatchObjectStoreImpl(vmThread, destObject);
	
	return true;
}

bool 
MM_VLHGCAccessBarrier::preBatchObjectStore(J9VMThread *vmThread, J9Class *destClass, bool isVolatile)
{
	j9object_t destObject = J9VM_J9CLASS_TO_HEAPCLASS(destClass);
	
	preBatchObjectStoreImpl(vmThread, destObject);
	
	return true;
}

/**
 * Generational write barrier call when a single object is stored into another.
 * The remembered set system consists of a physical list of objects in the OLD area that
 * may contain references to the new area.  The mutator is responsible for adding these old
 * area objects to the remembered set; the collectors are responsible for removing these objects
 * from the list when they no longer contain references.  Objects that are to be remembered have their
 * REMEMBERED bit set in the flags field.  For performance reasons, sublists are used to maintain the
 * remembered set.
 * 
 * @param vmThread The current thread that has performed the store.
 * @param dstObject The object which is being stored into.
 * @param srcObject The object being stored.
 * 
 * @note The write barrier can be called with minimal, all, or no validation checking.
 * @note Any object that contains a new reference MUST have its REMEMBERED bit set.
 */
void
MM_VLHGCAccessBarrier::postObjectStoreImpl(J9VMThread *vmThread, J9Object *dstObject, J9Object *srcObject)
{
	/* If the source object is NULL, there is no need for a write barrier. */
	if(NULL != srcObject) {
		MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(vmThread);
		_extensions->cardTable->dirtyCard(env, dstObject);
	}
}

/**
 * Generational write barrier call when a group of objects are stored into a single object.
 * The remembered set system consists of a physical list of objects in the OLD area that
 * may contain references to the new area.  The mutator is responsible for adding these old
 * area objects to the remembered set; the collectors are responsible for removing these objects
 * from the list when they no longer contain references.  Objects that are to be remembered have their
 * REMEMBERED bit set in the flags field.  For performance reasons, sublists are used to maintain the
 * remembered set.
 * 
 * @param vmThread The current thread that has performed the store.
 * @param dstObject The object which is being stored into.
 * 
 * @note The write barrier can be called with minimal, all, or no validation checking.
 * @note Any object that contains a new reference MUST have its REMEMBERED bit set.
 * @note This call is typically used by array copies, when it may be more efficient
 * to optimistically add an object to the remembered set without checking too hard.
 */
void 
MM_VLHGCAccessBarrier::preBatchObjectStoreImpl(J9VMThread *vmThread, J9Object *dstObject)
{
	MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(vmThread);
	_extensions->cardTable->dirtyCard(env, dstObject);
}


/**
 * Finds opportunities for doing the copy without or partially executing writeBarrier.
 * @return ARRAY_COPY_SUCCESSFUL if copy was successful, ARRAY_COPY_NOT_DONE no copy is done
 */
I_32
MM_VLHGCAccessBarrier::backwardReferenceArrayCopyIndex(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots)
{
	MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(vmThread);
	I_32 retValue = ARRAY_COPY_NOT_DONE;
	
	/* a high level caller ensured destObject == srcObject */
	Assert_MM_true(destObject == srcObject);
	if (_extensions->indexableObjectModel.isInlineContiguousArraylet(destObject)) {
		 retValue = doCopyContiguousBackward(vmThread, srcObject, destObject, srcIndex, destIndex, lengthInSlots);

		Assert_MM_true(retValue == ARRAY_COPY_SUCCESSFUL);
		if (((MM_IncrementalGenerationalGC *)_extensions->getGlobalCollector())->isGlobalMarkPhaseRunning()) {
			_extensions->cardTable->dirtyCard(env, (J9Object *)destObject);
		}
	}
	
	return retValue;
}

/**
 * Finds opportunities for doing the copy without or partially executing writeBarrier.
 * @return ARRAY_COPY_SUCCESSFUL if copy was successful, ARRAY_COPY_NOT_DONE no copy is done
 */
I_32
MM_VLHGCAccessBarrier::forwardReferenceArrayCopyIndex(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots)
{
	MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(vmThread);
	I_32 retValue = ARRAY_COPY_NOT_DONE;

	if (_extensions->indexableObjectModel.isInlineContiguousArraylet(destObject) && _extensions->indexableObjectModel.isInlineContiguousArraylet(srcObject)) {
		retValue = doCopyContiguousForward(vmThread, srcObject, destObject, srcIndex, destIndex, lengthInSlots);

		Assert_MM_true(retValue == ARRAY_COPY_SUCCESSFUL);
		if ((destObject != srcObject) || ((MM_IncrementalGenerationalGC *)_extensions->getGlobalCollector())->isGlobalMarkPhaseRunning()) {
			_extensions->cardTable->dirtyCard(env, (J9Object *)destObject);
		}

	}
	
	return retValue;
}

/**
 * VMDESIGN 2048
 * Special barrier for auto-remembering stack-referenced objects. This must be called 
 * in two cases:
 * 1) an object which was allocated directly into old space.
 * 2) an object which is being constructed via JNI
 * 
 * @param vmThread[in] the current thread
 * @param object[in] the object to be remembered 
 */
void 
MM_VLHGCAccessBarrier::recentlyAllocatedObject(J9VMThread *vmThread, J9Object *dstObject) 
{ 
	/* TODO: we may need to revive this if we apply VMDESIGN 2048 to Tarok specs */
}

void 
MM_VLHGCAccessBarrier::postStoreClassToClassLoader(J9VMThread *vmThread, J9ClassLoader* destClassLoader, J9Class* srcClass)
{
	J9Object* classLoaderObject = destClassLoader->classLoaderObject;
	/* unlikely, but during bootstrap the classLoaderObject can be NULL until j.l.ClassLoader is available */
	if (NULL == classLoaderObject) {
		/* only bootstrap classes can appear here, and it's safe to miss barriers for them since they're roots */
		Assert_MM_true(srcClass->classLoader == vmThread->javaVM->systemClassLoader);
	} else {
		J9Object* classObject = J9VM_J9CLASS_TO_HEAPCLASS(srcClass);
		postObjectStoreImpl(vmThread, classLoaderObject, classObject);
	}
}

void*
MM_VLHGCAccessBarrier::jniGetPrimitiveArrayCritical(J9VMThread* vmThread, jarray array, jboolean *isCopy)
{
	void *data = NULL;
	J9JavaVM *javaVM = vmThread->javaVM;
	J9InternalVMFunctions *functions = javaVM->internalVMFunctions;
	VM_VMAccess::inlineEnterVMFromJNI(vmThread);

	J9IndexableObject *arrayObject = (J9IndexableObject*)J9_JNI_UNWRAP_REFERENCE(array);
	bool shouldCopy = false;
	bool alwaysCopyInCritical = (javaVM->runtimeFlags & J9_RUNTIME_ALWAYS_COPY_JNI_CRITICAL) == J9_RUNTIME_ALWAYS_COPY_JNI_CRITICAL;
	if (alwaysCopyInCritical) {
		shouldCopy = true;
	} else if (!_extensions->indexableObjectModel.isInlineContiguousArraylet(arrayObject)) {
		/* an array having discontiguous extents is another reason to force the critical section to be a copy */
		shouldCopy = true;
	}

	if (shouldCopy) {
		GC_ArrayObjectModel* indexableObjectModel = &_extensions->indexableObjectModel;
		I_32 sizeInElements = (I_32)indexableObjectModel->getSizeInElements(arrayObject);
		UDATA sizeInBytes = indexableObjectModel->getDataSizeInBytes(arrayObject);
		data = functions->jniArrayAllocateMemoryFromThread(vmThread, sizeInBytes);
		if(NULL == data) {
			functions->setNativeOutOfMemoryError(vmThread, 0, 0);	// better error message here?
		} else {
			indexableObjectModel->memcpyFromArray(data, arrayObject, 0, sizeInElements);
			if(NULL != isCopy) {
				*isCopy = JNI_TRUE;
			}
		}
		vmThread->jniCriticalCopyCount += 1;
	} else {
		// acquire access and return a direct pointer
		MM_JNICriticalRegion::enterCriticalRegion(vmThread, true);
		Assert_MM_true(vmThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS);
		arrayObject = (J9IndexableObject*)J9_JNI_UNWRAP_REFERENCE(array);
		data = (void *)_extensions->indexableObjectModel.getDataPointerForContiguous(arrayObject);
		if(NULL != isCopy) {
			*isCopy = JNI_FALSE;
		}
#if defined(J9VM_GC_MODRON_COMPACTION) || defined(J9VM_GC_MODRON_SCAVENGER)
		/* we need to increment this region's critical count so that we know not to compact it */
		UDATA volatile *criticalCount = &(((MM_HeapRegionDescriptorVLHGC *)_heap->getHeapRegionManager()->regionDescriptorForAddress(arrayObject))->_criticalRegionsInUse);
		MM_AtomicOperations::add(criticalCount, 1);
#endif /* defined(J9VM_GC_MODRON_COMPACTION) || defined(J9VM_GC_MODRON_SCAVENGER)*/
	}
	VM_VMAccess::inlineExitVMToJNI(vmThread);
	return data;
}

void
MM_VLHGCAccessBarrier::jniReleasePrimitiveArrayCritical(J9VMThread* vmThread, jarray array, void * elems, jint mode)
{
	J9JavaVM *javaVM = vmThread->javaVM;
	J9InternalVMFunctions *functions = javaVM->internalVMFunctions;
	VM_VMAccess::inlineEnterVMFromJNI(vmThread);

	J9IndexableObject *arrayObject = (J9IndexableObject*)J9_JNI_UNWRAP_REFERENCE(array);
	bool shouldCopy = false;
	bool alwaysCopyInCritical = (javaVM->runtimeFlags & J9_RUNTIME_ALWAYS_COPY_JNI_CRITICAL) == J9_RUNTIME_ALWAYS_COPY_JNI_CRITICAL;
	if (alwaysCopyInCritical) {
		shouldCopy = true;
	} else if (!_extensions->indexableObjectModel.isInlineContiguousArraylet(arrayObject)) {
		/* an array having discontiguous extents is another reason to force the critical section to be a copy */
		shouldCopy = true;
	}

	if(shouldCopy) {
		if(JNI_ABORT != mode) {
			GC_ArrayObjectModel* indexableObjectModel = &_extensions->indexableObjectModel;
			I_32 sizeInElements = (I_32)indexableObjectModel->getSizeInElements(arrayObject);
			_extensions->indexableObjectModel.memcpyToArray(arrayObject, 0, sizeInElements, elems);
		}

		// Commit means copy the data but do not free the buffer.
		// All other modes free the buffer.
		if(JNI_COMMIT != mode) {
			functions->jniArrayFreeMemoryFromThread(vmThread, elems);
		}

		if(vmThread->jniCriticalCopyCount > 0) {
			vmThread->jniCriticalCopyCount -= 1;
		} else {
			Assert_MM_invalidJNICall();
		}
	} else {
		/*
		 * Objects can not be moved if critical section is active
		 * This trace point will be generated if object has been moved or passed value of elems is corrupted
		 */
		void *data = (void *)_extensions->indexableObjectModel.getDataPointerForContiguous(arrayObject);
		if(elems != data) {
			Trc_MM_JNIReleasePrimitiveArrayCritical_invalid(vmThread, arrayObject, elems, data);
		}
#if defined(J9VM_GC_MODRON_COMPACTION) || defined(J9VM_GC_MODRON_SCAVENGER)
		/* we need to decrement this region's critical count */
		UDATA volatile *criticalCount = &(((MM_HeapRegionDescriptorVLHGC *)_heap->getHeapRegionManager()->regionDescriptorForAddress(arrayObject))->_criticalRegionsInUse);
		Assert_MM_true((*criticalCount) > 0);
		MM_AtomicOperations::subtract(criticalCount, 1);
#endif /* defined(J9VM_GC_MODRON_COMPACTION) || defined(J9VM_GC_MODRON_SCAVENGER)*/
		MM_JNICriticalRegion::exitCriticalRegion(vmThread, true);
	}
	VM_VMAccess::inlineExitVMToJNI(vmThread);
}

const jchar*
MM_VLHGCAccessBarrier::jniGetStringCritical(J9VMThread* vmThread, jstring str, jboolean *isCopy)
{
	jchar *data = NULL;
	J9JavaVM *javaVM = vmThread->javaVM;
	J9InternalVMFunctions *functions = javaVM->internalVMFunctions;
	VM_VMAccess::inlineEnterVMFromJNI(vmThread);

	J9Object *stringObject = (J9Object*)J9_JNI_UNWRAP_REFERENCE(str);
	J9IndexableObject *valueObject = (J9IndexableObject*)J9VMJAVALANGSTRING_VALUE(vmThread, stringObject);
	bool shouldCopy = false;
	bool isCompressed = false;

	/* If the string bytes are in compressed UNICODE, then we need to copy to decompress */	
	if (IS_STRING_COMPRESSED(vmThread, stringObject)) {
		isCompressed = true;
		shouldCopy = true;
	}

	bool alwaysCopyInCritical = (javaVM->runtimeFlags & J9_RUNTIME_ALWAYS_COPY_JNI_CRITICAL) == J9_RUNTIME_ALWAYS_COPY_JNI_CRITICAL;
	if (alwaysCopyInCritical) {
		shouldCopy = true;
	} else if (!_extensions->indexableObjectModel.isInlineContiguousArraylet(valueObject)) {
		/* an array having discontiguous extents is another reason to force the critical section to be a copy */
		shouldCopy = true;
	}

	if (shouldCopy) {
		jint length = J9VMJAVALANGSTRING_LENGTH(vmThread, stringObject);
		UDATA sizeInBytes = length * sizeof(jchar);
		data = (jchar*)functions->jniArrayAllocateMemoryFromThread(vmThread, sizeInBytes);
		if (NULL == data) {
			functions->setNativeOutOfMemoryError(vmThread, 0, 0);	// better error message here?
		} else {
			if (isCompressed) {
				jint i;
				
				for (i = 0; i < length; i++) {
					data[i] = (jchar)J9JAVAARRAYOFBYTE_LOAD(vmThread, (j9object_t)valueObject, i) & (jchar)0xFF;
				}
			} else {
				GC_ArrayObjectModel* indexableObjectModel = &_extensions->indexableObjectModel;
				
				if (J9_ARE_ANY_BITS_SET(javaVM->runtimeFlags, J9_RUNTIME_STRING_BYTE_ARRAY)) {
					// This API determines the stride based on the type of valueObject so in the [B case we must passin the length in bytes
					indexableObjectModel->memcpyFromArray(data, valueObject, 0, (I_32)sizeInBytes);
				} else {
					indexableObjectModel->memcpyFromArray(data, valueObject, 0, length);
				}
			}
			if (NULL != isCopy) {
				*isCopy = JNI_TRUE;
			}
		}
		vmThread->jniCriticalCopyCount += 1;
	} else {
		// acquire access and return a direct pointer
		MM_JNICriticalRegion::enterCriticalRegion(vmThread, true);
		Assert_MM_true(vmThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS);
		data = (jchar*)_extensions->indexableObjectModel.getDataPointerForContiguous(valueObject);

		if (NULL != isCopy) {
			*isCopy = JNI_FALSE;
		}
#if defined(J9VM_GC_MODRON_COMPACTION) || defined(J9VM_GC_MODRON_SCAVENGER)
		/* we need to increment this region's critical count so that we know not to compact it */
		UDATA volatile *criticalCount = &(((MM_HeapRegionDescriptorVLHGC *)_heap->getHeapRegionManager()->regionDescriptorForAddress(valueObject))->_criticalRegionsInUse);
		MM_AtomicOperations::add(criticalCount, 1);
#endif /* defined(J9VM_GC_MODRON_COMPACTION) || defined(J9VM_GC_MODRON_SCAVENGER)*/
	}
	VM_VMAccess::inlineExitVMToJNI(vmThread);
	return data;
}

void
MM_VLHGCAccessBarrier::jniReleaseStringCritical(J9VMThread* vmThread, jstring str, const jchar* elems)
{
	J9JavaVM *javaVM = vmThread->javaVM;
	J9InternalVMFunctions *functions = javaVM->internalVMFunctions;
	VM_VMAccess::inlineEnterVMFromJNI(vmThread);

	J9Object *stringObject = (J9Object*)J9_JNI_UNWRAP_REFERENCE(str);
	J9IndexableObject *valueObject = (J9IndexableObject*)J9VMJAVALANGSTRING_VALUE(vmThread, stringObject);
	bool shouldCopy = false;
	if (IS_STRING_COMPRESSION_ENABLED_VM(javaVM)) {
		shouldCopy = true;
	}

	bool alwaysCopyInCritical = (javaVM->runtimeFlags & J9_RUNTIME_ALWAYS_COPY_JNI_CRITICAL) == J9_RUNTIME_ALWAYS_COPY_JNI_CRITICAL;
	if (alwaysCopyInCritical) {
		shouldCopy = true;
	} else if (!_extensions->indexableObjectModel.isInlineContiguousArraylet(valueObject)) {
		/* an array having discontiguous extents is another reason to force the critical section to be a copy */
		shouldCopy = true;
	}

	if (shouldCopy) {
		// String data is not copied back
		functions->jniArrayFreeMemoryFromThread(vmThread, (void*)elems);

		if(vmThread->jniCriticalCopyCount > 0) {
			vmThread->jniCriticalCopyCount -= 1;
		} else {
			Assert_MM_invalidJNICall();
		}
	} else {
		// direct pointer, just drop access
#if defined(J9VM_GC_MODRON_COMPACTION) || defined(J9VM_GC_MODRON_SCAVENGER)
		/* we need to decrement this region's critical count */
		UDATA volatile *criticalCount = &(((MM_HeapRegionDescriptorVLHGC *)_heap->getHeapRegionManager()->regionDescriptorForAddress(valueObject))->_criticalRegionsInUse);
		Assert_MM_true((*criticalCount) > 0);
		MM_AtomicOperations::subtract(criticalCount, 1);
#endif /* defined(J9VM_GC_MODRON_COMPACTION) || defined(J9VM_GC_MODRON_SCAVENGER)*/
		MM_JNICriticalRegion::exitCriticalRegion(vmThread, true);
	}
	VM_VMAccess::inlineExitVMToJNI(vmThread);
}
