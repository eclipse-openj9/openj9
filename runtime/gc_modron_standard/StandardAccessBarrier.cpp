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

#include "StandardAccessBarrier.hpp"
#include "AtomicOperations.hpp"
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
#include "ConcurrentGC.hpp"
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
#include "Debug.hpp"
#include "EnvironmentStandard.hpp"
#include "mmhook_internal.h"
#include "GCExtensions.hpp"
#include "HeapRegionManager.hpp"
#include "JNICriticalRegion.hpp"
#include "ObjectModel.hpp"
#include "Scavenger.hpp"
#include "SublistFragment.hpp"

MM_StandardAccessBarrier *
MM_StandardAccessBarrier::newInstance(MM_EnvironmentBase *env)
{
	MM_StandardAccessBarrier *barrier;
	
	barrier = (MM_StandardAccessBarrier *)env->getForge()->allocate(sizeof(MM_StandardAccessBarrier), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (barrier) {
		new(barrier) MM_StandardAccessBarrier(env);
		if (!barrier->initialize(env)) {
			barrier->kill(env);
			barrier = NULL;
		}
	}
	return barrier;
}

/**
 * Enables the double barrier on the provided thread.
 */
void
MM_StandardAccessBarrier::setDoubleBarrierActiveOnThread(MM_EnvironmentBase* env)
{
	MM_GCExtensions::getExtensions(env)->sATBBarrierRememberedSet->preserveLocalFragmentIndex(env, &(((J9VMThread *)env->getLanguageVMThread())->sATBBarrierRememberedSetFragment));
}

void
MM_StandardAccessBarrier::initializeForNewThread(MM_EnvironmentBase* env)
{
#if defined(OMR_GC_REALTIME)
	if (_extensions->configuration->isSnapshotAtTheBeginningBarrierEnabled()) {

		_extensions->sATBBarrierRememberedSet->initializeFragment(env, &(((J9VMThread *)env->getLanguageVMThread())->sATBBarrierRememberedSetFragment));
		if (isDoubleBarrierActive()) {
			setDoubleBarrierActiveOnThread(env);
		}
	}
#endif /* OMR_GC_REALTIME */
}

bool 
MM_StandardAccessBarrier::initialize(MM_EnvironmentBase *env)
{
#if defined(J9VM_GC_GENERATIONAL)
	if (!_generationalAccessBarrierComponent.initialize(env)) {
		return false;
	}
#endif /* J9VM_GC_GENERATIONAL */
	
	return MM_ObjectAccessBarrier::initialize(env);
}

void 
MM_StandardAccessBarrier::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

void
MM_StandardAccessBarrier::tearDown(MM_EnvironmentBase *env)
{
#if defined(J9VM_GC_GENERATIONAL)
	_generationalAccessBarrierComponent.tearDown(env);
#endif /* J9VM_GC_GENERATIONAL */
	
	MM_ObjectAccessBarrier::tearDown(env);
}

/**
 * Unmarked, heap reference, about to be deleted (or overwritten), while marking
 * is in progress is to be remembered for later marking and scanning.
 */
void
MM_StandardAccessBarrier::rememberObjectToRescan(MM_EnvironmentBase *env, J9Object *object)
{
	MM_ParallelGlobalGC *globalCollector = (MM_ParallelGlobalGC *)_extensions->getGlobalCollector();

	if (globalCollector->getMarkingScheme()->markObject(env, object, true)) {
		rememberObjectImpl(env, object);
	}
}

/**
 * Unmarked, heap reference, about to be deleted (or overwritten), while marking
 * is in progress is to be remembered for later marking and scanning.
 * This method is called by MM_StandardAccessBarrier::rememberObject()
 */
void
MM_StandardAccessBarrier::rememberObjectImpl(MM_EnvironmentBase *env, J9Object* object)
{
	J9VMThread *vmThread = (J9VMThread *)env->getLanguageVMThread();
	_extensions->sATBBarrierRememberedSet->storeInFragment(env, &vmThread->sATBBarrierRememberedSetFragment, (UDATA *)object);
}


bool
MM_StandardAccessBarrier::preObjectStoreImpl(J9VMThread *vmThread, J9Object *destObject, fj9object_t *destAddress, J9Object *value, bool isVolatile)
{
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);

	if (isSATBBarrierActive(env)) {
		if (NULL != destObject) {
			if (isDoubleBarrierActiveOnThread(vmThread)) {
				rememberObjectToRescan(env, value);
			}

			J9Object *oldObject = NULL;
			protectIfVolatileBefore(vmThread, isVolatile, true, false);
			oldObject = mmPointerFromToken(vmThread, *destAddress);
			protectIfVolatileAfter(vmThread, isVolatile, true, false);
			rememberObjectToRescan(env, oldObject);
		}
	}

	return true;
}

bool
MM_StandardAccessBarrier::preObjectStoreImpl(J9VMThread *vmThread, J9Object **destAddress, J9Object *value, bool isVolatile)
{
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);

	if (isSATBBarrierActive(env)) {
		if (isDoubleBarrierActiveOnThread(vmThread)) {
			rememberObjectToRescan(env, value);
		}
		J9Object* oldObject = NULL;
		protectIfVolatileBefore(vmThread, isVolatile, true, false);
		oldObject = *destAddress;
		protectIfVolatileAfter(vmThread, isVolatile, true, false);
		rememberObjectToRescan(env, oldObject);
	}

	return true;
}
/**
 * @copydoc MM_ObjectAccessBarrier::preObjectStore()
 *
 * Metronome uses a snapshot-at-the-beginning algorithm, but with a fuzzy snapshot in the
 * sense that threads are allowed to run during the root scan.  This requires a "double
 * barrier."  The barrier is active from the start of root scanning through the end of
 * tracing.  For an unscanned thread performing a store, the new value is remembered by
 * the collector.  For any thread performing a store (whether scanned or not), the old
 * value is remembered by the collector before being overwritten (thus this barrier must be
 * positioned as a pre-store barrier).  For the latter ("Yuasa barrier") aspect of the
 * double barrier, only the first overwritten value needs to be remembered (remembering
 * others is harmless but not needed), and so we omit synchronization on the reading of the
 * old value.
 **/
bool
MM_StandardAccessBarrier::preObjectStore(J9VMThread *vmThread, J9Object *destObject, fj9object_t *destAddress, J9Object *value, bool isVolatile)
{
	return preObjectStoreImpl(vmThread, destObject, destAddress, value, isVolatile);
}

/**
 * @copydoc MM_MetronomeAccessBarrier::preObjectStore()
 *
 * Used for stores into classes
 */
bool
MM_StandardAccessBarrier::preObjectStore(J9VMThread *vmThread, J9Object *destClass, J9Object **destAddress, J9Object *value, bool isVolatile)
{
	return preObjectStoreImpl(vmThread, destAddress, value, isVolatile);
}

/**
 * @copydoc MM_MetronomeAccessBarrier::preObjectStore()
 *
 * Used for stores into internal structures
 */
bool
MM_StandardAccessBarrier::preObjectStore(J9VMThread *vmThread, J9Object **destAddress, J9Object *value, bool isVolatile)
{
	return preObjectStoreImpl(vmThread, destAddress, value, isVolatile);
}
/**
 * Called after an object is stored into another object.
 */
void
MM_StandardAccessBarrier::postObjectStore(J9VMThread *vmThread, J9Object *destObject, fj9object_t *destAddress, J9Object *value, bool isVolatile)
{
	postObjectStoreImpl(vmThread, destObject, value);

}

/**
 * Called after an object is stored into a class.
 */
void
MM_StandardAccessBarrier::postObjectStore(J9VMThread *vmThread, J9Class *destClass, J9Object **destAddress, J9Object *value, bool isVolatile)
{
	j9object_t destObject = J9VM_J9CLASS_TO_HEAPCLASS(destClass);

	/* destObject is guaranteed to be in old space, so the common code path will remember objects appropriately here */
	postObjectStoreImpl(vmThread, destObject, value);
}

bool 
MM_StandardAccessBarrier::preBatchObjectStore(J9VMThread *vmThread, J9Object *destObject, bool isVolatile)
{
	preBatchObjectStoreImpl(vmThread, destObject);
	
	return true;
}

bool 
MM_StandardAccessBarrier::preBatchObjectStore(J9VMThread *vmThread, J9Class *destClass, bool isVolatile)
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
MM_StandardAccessBarrier::postObjectStoreImpl(J9VMThread *vmThread, J9Object *dstObject, J9Object *srcObject)
{
	/* If the source object is NULL, there is no need for a write barrier. */
	if(NULL != srcObject) {
		if (_extensions->isConcurrentScavengerEnabled() && !_extensions->isScavengerBackOutFlagRaised()) {
			Assert_MM_false(_extensions->scavenger->isObjectInEvacuateMemory(dstObject));
			Assert_MM_false(_extensions->scavenger->isObjectInEvacuateMemory(srcObject));
		}

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
		/* Call the concurrent write barrier if required */
		if(isIncrementalUpdateBarrierActive(vmThread) && _extensions->isOld(dstObject)) {
			J9ConcurrentWriteBarrierStore(vmThread->omrVMThread, dstObject, srcObject);
		}
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
	
#if defined(J9VM_GC_GENERATIONAL)
		_generationalAccessBarrierComponent.postObjectStore(vmThread, dstObject, srcObject);
#endif /* J9VM_GC_GENERATIONAL */
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
MM_StandardAccessBarrier::preBatchObjectStoreImpl(J9VMThread *vmThread, J9Object *dstObject)
{
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	/* Call the concurrent write barrier if required */
	if(_extensions->concurrentMark && 
		(vmThread->privateFlags & J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE) &&
		_extensions->isOld(dstObject)) {
		J9ConcurrentWriteBarrierBatchStore(vmThread->omrVMThread, dstObject);
	}
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */

#if defined(J9VM_GC_GENERATIONAL)
	_generationalAccessBarrierComponent.preBatchObjectStore(vmThread, dstObject);
#endif /* J9VM_GC_GENERATIONAL */
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
MM_StandardAccessBarrier::recentlyAllocatedObject(J9VMThread *vmThread, J9Object *dstObject) 
{ 
#if defined(J9VM_GC_GENERATIONAL)
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(vmThread);

	if(extensions->scavengerEnabled && !extensions->isConcurrentScavengerEnabled() && extensions->isOld(dstObject) && !extensions->objectModel.isPrimitiveArray(dstObject)) {

		Trc_MM_StandardAccessBarrier_treatObjectAsRecentlyAllocated(vmThread, dstObject);

		if(extensions->objectModel.atomicSwitchReferencedState(dstObject, OMR_TENURED_STACK_OBJECT_CURRENTLY_REFERENCED)) {
			/* Successfully set the remembered bit in the object.  Now allocate an entry from the
			 * remembered set fragment of the current thread and store the destination object into
			 * the remembered set. */
			MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(vmThread->omrVMThread);
			MM_SublistFragment fragment((J9VMGC_SublistFragment*)&vmThread->gcRememberedSet);
			if (!fragment.add(env, (UDATA)dstObject )) {
				/* No slot was available from any fragment.  Set the remembered set overflow flag.
				 * The REMEMBERED bit is kept in the object for optimization purposes (only scan objects
				 * whose REMEMBERED bit is set in an overflow scan)
				 */
				extensions->setRememberedSetOverflowState();
#if defined(J9VM_GC_MODRON_EVENTS)			
				reportRememberedSetOverflow(vmThread);
#endif /* J9VM_GC_MODRON_EVENTS */
			}
		}
	}
#endif /* J9VM_GC_GENERATIONAL */
}

void*
MM_StandardAccessBarrier::jniGetPrimitiveArrayCritical(J9VMThread* vmThread, jarray array, jboolean *isCopy)
{
	void *data = NULL;
	J9JavaVM *javaVM = vmThread->javaVM;
	J9InternalVMFunctions *functions = javaVM->internalVMFunctions;

	bool shouldCopy = false;
	bool alwaysCopyInCritical = (javaVM->runtimeFlags & J9_RUNTIME_ALWAYS_COPY_JNI_CRITICAL) == J9_RUNTIME_ALWAYS_COPY_JNI_CRITICAL;
	if (alwaysCopyInCritical) {
		shouldCopy = true;
	}

	if(shouldCopy) {
		VM_VMAccess::inlineEnterVMFromJNI(vmThread);
		J9IndexableObject *arrayObject = (J9IndexableObject*)J9_JNI_UNWRAP_REFERENCE(array);
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
		VM_VMAccess::inlineExitVMToJNI(vmThread);
	} else {
		// acquire access and return a direct pointer
		MM_JNICriticalRegion::enterCriticalRegion(vmThread, false);
		J9IndexableObject *arrayObject = (J9IndexableObject*)J9_JNI_UNWRAP_REFERENCE(array);
		data = (void *)_extensions->indexableObjectModel.getDataPointerForContiguous(arrayObject);
		if(NULL != isCopy) {
			*isCopy = JNI_FALSE;
		}
	}
	return data;
}

void
MM_StandardAccessBarrier::jniReleasePrimitiveArrayCritical(J9VMThread* vmThread, jarray array, void * elems, jint mode)
{
	J9JavaVM *javaVM = vmThread->javaVM;
	J9InternalVMFunctions *functions = javaVM->internalVMFunctions;

	bool shouldCopy = false;
	bool alwaysCopyInCritical = (javaVM->runtimeFlags & J9_RUNTIME_ALWAYS_COPY_JNI_CRITICAL) == J9_RUNTIME_ALWAYS_COPY_JNI_CRITICAL;
	if (alwaysCopyInCritical) {
		shouldCopy = true;
	}

	if(shouldCopy) {
		VM_VMAccess::inlineEnterVMFromJNI(vmThread);
		if(JNI_ABORT != mode) {
			J9IndexableObject *arrayObject = (J9IndexableObject*)J9_JNI_UNWRAP_REFERENCE(array);
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

		VM_VMAccess::inlineExitVMToJNI(vmThread);
	} else {
		/*
		 * Objects can not be moved if critical section is active
		 * This trace point will be generated if object has been moved or passed value of elems is corrupted
		 */
		J9IndexableObject *arrayObject = (J9IndexableObject*)J9_JNI_UNWRAP_REFERENCE(array);
		void *data = (void *)_extensions->indexableObjectModel.getDataPointerForContiguous(arrayObject);
		if(elems != data) {
			Trc_MM_JNIReleasePrimitiveArrayCritical_invalid(vmThread, arrayObject, elems, data);
		}

		MM_JNICriticalRegion::exitCriticalRegion(vmThread, false);
	}
}

const jchar*
MM_StandardAccessBarrier::jniGetStringCritical(J9VMThread* vmThread, jstring str, jboolean *isCopy)
{
	jchar *data = NULL;
	J9JavaVM *javaVM = vmThread->javaVM;
	J9InternalVMFunctions *functions = javaVM->internalVMFunctions;
	bool isCompressed = false;
	bool shouldCopy = false;
	bool hasVMAccess = false;

	if ((javaVM->runtimeFlags & J9_RUNTIME_ALWAYS_COPY_JNI_CRITICAL) == J9_RUNTIME_ALWAYS_COPY_JNI_CRITICAL) {
		VM_VMAccess::inlineEnterVMFromJNI(vmThread);
		hasVMAccess = true;
		shouldCopy = true;
	} else if (IS_STRING_COMPRESSION_ENABLED_VM(javaVM)) {
		/* If the string bytes are in compressed UNICODE, then we need to copy to decompress */
		VM_VMAccess::inlineEnterVMFromJNI(vmThread);
		hasVMAccess = true;
		J9Object *stringObject = (J9Object*)J9_JNI_UNWRAP_REFERENCE(str);
		if (IS_STRING_COMPRESSED(vmThread,stringObject)) {
			isCompressed = true;
			shouldCopy = true;
		}
	}

	if (shouldCopy) {
		J9Object *stringObject = (J9Object*)J9_JNI_UNWRAP_REFERENCE(str);
		J9IndexableObject *valueObject = (J9IndexableObject*)J9VMJAVALANGSTRING_VALUE(vmThread, stringObject);
		jint length = J9VMJAVALANGSTRING_LENGTH(vmThread, stringObject);
		UDATA sizeInBytes = length * sizeof(jchar);

		if (IS_STRING_COMPRESSED(vmThread, stringObject)) {
				isCompressed = true;
		}
		
		data = (jchar*)functions->jniArrayAllocateMemoryFromThread(vmThread, sizeInBytes);
		if (NULL == data) {
			functions->setNativeOutOfMemoryError(vmThread, 0, 0);	// better error message here?
		} else {
			GC_ArrayObjectModel* indexableObjectModel = &_extensions->indexableObjectModel;
			if (isCompressed) {
				jint i;
				
				for (i = 0; i < length; i++) {
					data[i] = (jchar)J9JAVAARRAYOFBYTE_LOAD(vmThread, (j9object_t)valueObject, i) & (jchar)0xFF;
				}
			} else {
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
		MM_JNICriticalRegion::enterCriticalRegion(vmThread, hasVMAccess);
		J9Object *stringObject = (J9Object*)J9_JNI_UNWRAP_REFERENCE(str);
		J9IndexableObject *valueObject = (J9IndexableObject*)J9VMJAVALANGSTRING_VALUE(vmThread, stringObject);

		data = (jchar*)_extensions->indexableObjectModel.getDataPointerForContiguous(valueObject);

		if (NULL != isCopy) {
			*isCopy = JNI_FALSE;
		}
	}
	if (hasVMAccess) {
		VM_VMAccess::inlineExitVMToJNI(vmThread);
	}
	return data;
}

void
MM_StandardAccessBarrier::jniReleaseStringCritical(J9VMThread* vmThread, jstring str, const jchar* elems)
{
	J9JavaVM *javaVM = vmThread->javaVM;
	J9InternalVMFunctions *functions = javaVM->internalVMFunctions;
	bool hasVMAccess = false;
	bool shouldCopy = false;

	if ((javaVM->runtimeFlags & J9_RUNTIME_ALWAYS_COPY_JNI_CRITICAL) == J9_RUNTIME_ALWAYS_COPY_JNI_CRITICAL) {
		shouldCopy = true;
	} else if (IS_STRING_COMPRESSION_ENABLED_VM(javaVM)) {
		VM_VMAccess::inlineEnterVMFromJNI(vmThread);
		hasVMAccess = true;
		J9Object *stringObject = (J9Object*)J9_JNI_UNWRAP_REFERENCE(str);
		if (IS_STRING_COMPRESSED(vmThread, stringObject)) {
			shouldCopy = true;
		}
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
		/*
		 * We have not put assertion here to check is elems valid for str similar way as in jniReleasePrimitiveArrayCritical
		 * because of complexity of required code
		 */
		// direct pointer, just drop access
		MM_JNICriticalRegion::exitCriticalRegion(vmThread, hasVMAccess);
	}

	if (hasVMAccess) {
		VM_VMAccess::inlineExitVMToJNI(vmThread);
	}
}

UDATA
MM_StandardAccessBarrier::getJNICriticalRegionCount(MM_GCExtensions *extensions)
{
	/* TODO kmt : This is probably the slowest way to get this information */
	GC_VMThreadListIterator threadIterator(((J9JavaVM *)extensions->getOmrVM()->_language_vm));
	J9VMThread *walkThread;
	UDATA activeCriticals = 0;

	// TODO kmt : Should get public flags mutex here -- worst case is a false positive
	while((walkThread = threadIterator.nextVMThread()) != NULL) {
		activeCriticals += walkThread->jniCriticalDirectCount;
	}
	return activeCriticals;
}

#if defined(J9VM_GC_ARRAYLETS)
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
I_32
MM_StandardAccessBarrier::doCopyContiguousBackwardWithReadBarrier(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots)
{
	srcIndex += lengthInSlots;
	destIndex += lengthInSlots;

	fj9object_t *srcSlot = (fj9object_t *)indexableEffectiveAddress(vmThread, srcObject, srcIndex, sizeof(fj9object_t));
	fj9object_t *destSlot = (fj9object_t *)indexableEffectiveAddress(vmThread, destObject, destIndex, sizeof(fj9object_t));
	fj9object_t *srcEndSlot = srcSlot - lengthInSlots;

	while (srcSlot-- > srcEndSlot) {
		preObjectRead(vmThread, (J9Object *)srcObject, srcSlot);

		*--destSlot = *srcSlot;
	}

	return ARRAY_COPY_SUCCESSFUL;
}

I_32
MM_StandardAccessBarrier::doCopyContiguousForwardWithReadBarrier(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots)
{
	fj9object_t *srcSlot = (fj9object_t *)indexableEffectiveAddress(vmThread, srcObject, srcIndex, sizeof(fj9object_t));
	fj9object_t *destSlot = (fj9object_t *)indexableEffectiveAddress(vmThread, destObject, destIndex, sizeof(fj9object_t));
	fj9object_t *srcEndSlot = srcSlot + lengthInSlots;

	while (srcSlot < srcEndSlot) {
		preObjectRead(vmThread, (J9Object *)srcObject, srcSlot);

		*destSlot++ = *srcSlot++;
	}

	return ARRAY_COPY_SUCCESSFUL;
}
#endif /* OMR_GC_CONCURRENT_SCAVENGER */

/**
 * Finds opportunities for doing the copy without or partially executing writeBarrier.
 * @return ARRAY_COPY_SUCCESSFUL if copy was successful, ARRAY_COPY_NOT_DONE no copy is done
 */
I_32
MM_StandardAccessBarrier::backwardReferenceArrayCopyIndex(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots)
{
	I_32 retValue = ARRAY_COPY_NOT_DONE;
	//TODO SATB re-enable opt?
	if (_extensions->configuration->isSnapshotAtTheBeginningBarrierEnabled()) {
		return retValue;
	}

	if(0 == lengthInSlots) {
		retValue = ARRAY_COPY_SUCCESSFUL;
	} else {
		/* a high level caller ensured destObject == srcObject */
		Assert_MM_true(destObject == srcObject);
		Assert_MM_true(_extensions->indexableObjectModel.isInlineContiguousArraylet(destObject));

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		if (_extensions->isConcurrentScavengerInProgress()) {
			/* During active CS cycle, we need a RB for every slot being copied.
			 * For WB same rules apply - just need the final batch barrier.
			 */
			retValue = doCopyContiguousBackwardWithReadBarrier(vmThread, srcObject, destObject, srcIndex, destIndex, lengthInSlots);
		} else
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
		{
			retValue = doCopyContiguousBackward(vmThread, srcObject, destObject, srcIndex, destIndex, lengthInSlots);
		}
		Assert_MM_true(retValue == ARRAY_COPY_SUCCESSFUL);

		preBatchObjectStoreImpl(vmThread, (J9Object *)destObject);
	}
	return retValue;
}

/**
 * Finds opportunities for doing the copy without or partially executing writeBarrier.
 * @return ARRAY_COPY_SUCCESSFUL if copy was successful, ARRAY_COPY_NOT_DONE no copy is done
 */
I_32
MM_StandardAccessBarrier::forwardReferenceArrayCopyIndex(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots)
{
	//TODO SATB re-enable opt
	I_32 retValue = ARRAY_COPY_NOT_DONE;

	if (_extensions->configuration->isSnapshotAtTheBeginningBarrierEnabled()) {
		return retValue;
	}

	if(0 == lengthInSlots) {
		retValue = ARRAY_COPY_SUCCESSFUL;
	} else {
		Assert_MM_true(_extensions->indexableObjectModel.isInlineContiguousArraylet(destObject));
		Assert_MM_true(_extensions->indexableObjectModel.isInlineContiguousArraylet(srcObject));

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		if (_extensions->isConcurrentScavengerInProgress()) {
			/* During active CS cycle, we need a RB for every slot being copied.
			 * For WB same rules apply - just need the final batch barrier.
			 */
			retValue = doCopyContiguousForwardWithReadBarrier(vmThread, srcObject, destObject, srcIndex, destIndex, lengthInSlots);
		} else
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
		{
			retValue = doCopyContiguousForward(vmThread, srcObject, destObject, srcIndex, destIndex, lengthInSlots);
		}

		Assert_MM_true(retValue == ARRAY_COPY_SUCCESSFUL);
		preBatchObjectStoreImpl(vmThread, (J9Object *)destObject);
	}
	return retValue;
}
#endif /* J9VM_GC_ARRAYLETS */

J9Object*
MM_StandardAccessBarrier::asConstantPoolObject(J9VMThread *vmThread, J9Object* toConvert, UDATA allocationFlags)
{
	j9object_t cpObject = toConvert;

	Assert_MM_true(allocationFlags & (J9_GC_ALLOCATE_OBJECT_TENURED | J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE));

	if (NULL != toConvert) {
		Assert_MM_false(_extensions->objectModel.isIndexable(toConvert));
		if (!_extensions->isOld(toConvert)) {
			MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
			if (!env->saveObjects(toConvert)) {
				Assert_MM_unreachable();
			}
			J9Class *j9class = J9GC_J9OBJECT_CLAZZ(toConvert);
			cpObject = J9AllocateObject(vmThread, j9class, allocationFlags);
			env->restoreObjects(&toConvert);
			if (cpObject != NULL) {
				cloneObject(vmThread, toConvert, cpObject);
			}
		}
	}
	return cpObject;
}

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
bool
MM_StandardAccessBarrier::preMonitorTableSlotRead(J9VMThread *vmThread, j9object_t *srcAddress)
{
	omrobjectptr_t object = (omrobjectptr_t)*srcAddress;

	if ((NULL != _extensions->scavenger) && _extensions->scavenger->isObjectInEvacuateMemory(object)) {
		MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(vmThread->omrVMThread);
		Assert_MM_true(_extensions->scavenger->isConcurrentInProgress());
		Assert_MM_true(_extensions->scavenger->isMutatorThreadInSyncWithCycle(env));

		MM_ForwardedHeader forwardHeader(object);
		omrobjectptr_t forwardPtr = forwardHeader.getForwardedObject();

		if (NULL != forwardPtr) {
			/* Object has been copied - ensure the object is fully copied before exposing it, update the slot and return.
			 *  Slot update needs to be atomic, only if there is a mutator thread racing with a write operation to this same slot.
			 *  ATM, this barrier is only used to update Monitor table entries, which should not be ever reinitialized by any mutator.
			 *  Updated can occur, but only by GC threads during STW clearing phase, thus no race with this barrier. */
			forwardHeader.copyOrWait(forwardPtr);
			*srcAddress = forwardPtr;
		} else {
			Assert_GC_true_with_message2(env, forwardHeader.isSelfForwardedPointer(),
				"Monitor object, not forwarded, in Evacuate: slot %llx object %llx\n", srcAddress, object);
			/* A typical usage of this barrier is to update monitor table slot for blocking object (blockingEnterObject).
			 * Such object is a hard root, hence copied during initial roots scanning. We should never need to copy it via this barrier.
			 * If this assert eventually triggers it means the barrier is used for some other objects that are not hard roots
			 * (for example, iterating monitor table to dump info about all monitors). If so, try using the other API (that's using VM rather
			 * than Thread argument) instead (since that API expects accessing even dead objects and does not impose the assert).
			 */
		}
	}

	return true;
}

bool
MM_StandardAccessBarrier::preMonitorTableSlotRead(J9JavaVM *vm, j9object_t *srcAddress)
{
	omrobjectptr_t object = (omrobjectptr_t)*srcAddress;

	if ((NULL != _extensions->scavenger) && _extensions->scavenger->isObjectInEvacuateMemory(object)) {
		Assert_MM_true(_extensions->scavenger->isConcurrentInProgress());

		MM_ForwardedHeader forwardHeader(object);
		omrobjectptr_t forwardPtr = forwardHeader.getForwardedObject();

		if (NULL != forwardPtr) {
			/* Object has been copied - ensure the object is fully copied before exposing it, update the slot and return */
			forwardHeader.copyOrWait(forwardPtr);
			*srcAddress = forwardPtr;
		}
		/* Do nothing if the object is not copied already, since it might be dead.
		 * This object is found unintentionally (without real reference to it),
		 * for example by iterating colliding entries in monitor hash table */
	}

	return true;
}

bool
MM_StandardAccessBarrier::preObjectRead(J9VMThread *vmThread, J9Class *srcClass, j9object_t *srcAddress)
{
	omrobjectptr_t object = *(volatile omrobjectptr_t *)srcAddress;

	if ((NULL !=_extensions->scavenger) && _extensions->scavenger->isObjectInEvacuateMemory(object)) {
		MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(vmThread->omrVMThread);
		Assert_MM_true(_extensions->scavenger->isConcurrentInProgress());
		Assert_MM_true(_extensions->scavenger->isMutatorThreadInSyncWithCycle(env));

		MM_ForwardedHeader forwardHeader(object);
		omrobjectptr_t forwardPtr = forwardHeader.getForwardedObject();

		if (NULL != forwardPtr) {
			/* Object has been strictly (remotely) forwarded. Ensure the object is fully copied before exposing it, update the slot and return. */
			forwardHeader.copyOrWait(forwardPtr);
			MM_AtomicOperations::lockCompareExchange((uintptr_t *)srcAddress, (uintptr_t)object, (uintptr_t)forwardPtr);
		} else {
			omrobjectptr_t destinationObjectPtr = _extensions->scavenger->copyObject(env, &forwardHeader);
			if (NULL == destinationObjectPtr) {
				/* Failure - the scavenger must back out the work it has done. Attempt to return the original object. */
				forwardPtr = forwardHeader.setSelfForwardedObject();
				if (forwardPtr != object) {
					/* Another thread successfully copied the object. Re-fetch forwarding pointer,
					 * and ensure the object is fully copied before exposing it. */
					MM_ForwardedHeader(object).copyOrWait(forwardPtr);
					MM_AtomicOperations::lockCompareExchange((uintptr_t *)srcAddress, (uintptr_t)object, (uintptr_t)forwardPtr);
				}
			} else {
				/* Update the slot. copyObject() ensures that the object is fully copied. */
				MM_AtomicOperations::lockCompareExchange((uintptr_t *)srcAddress, (uintptr_t)object, (uintptr_t)destinationObjectPtr);
			}
		}
	}

	return true;
}


#define GLOBAL_READ_BARRIR_STATS_UPDATE_THRESHOLD 32

bool
MM_StandardAccessBarrier::preObjectRead(J9VMThread *vmThread, J9Object *srcObject, fj9object_t *srcAddress)
{
	MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(vmThread->omrVMThread);
	Assert_GC_true_with_message(env, !_extensions->scavenger->isObjectInEvacuateMemory((omrobjectptr_t)srcAddress) || _extensions->isScavengerBackOutFlagRaised(), "readObject %llx in Evacuate\n", srcAddress);

	/* with volatile cast, ensure that we are really getting a snapshot (instead of the slot being re-read at later points with possibly different values) */
	fomrobject_t objectToken = *(volatile fomrobject_t *)srcAddress;
	omrobjectptr_t object = convertPointerFromToken(objectToken);
	if (_extensions->scavenger->isObjectInEvacuateMemory(object)) {
		Assert_GC_true_with_message2(env, _extensions->scavenger->isConcurrentInProgress(),
				"CS not in progress, found a object in Survivor: slot %llx object %llx\n", srcAddress, object);
		Assert_MM_true(_extensions->scavenger->isMutatorThreadInSyncWithCycle(env));
		/* since object is still in evacuate, srcObject has not been scanned yet => we cannot assert
		 * if srcObject should (already) be remembered (even if it's old)
		 */

		env->_scavengerStats._readObjectBarrierUpdate += 1;
		if (GLOBAL_READ_BARRIR_STATS_UPDATE_THRESHOLD == env->_scavengerStats._readObjectBarrierUpdate) {
			MM_AtomicOperations::addU64(&_extensions->scavengerStats._readObjectBarrierUpdate, GLOBAL_READ_BARRIR_STATS_UPDATE_THRESHOLD);
			env->_scavengerStats._readObjectBarrierUpdate = 0;
		}

		GC_SlotObject slotObject(env->getOmrVM(), srcAddress);
		MM_ForwardedHeader forwardHeader(object);
		omrobjectptr_t forwardPtr = forwardHeader.getForwardedObject();
		if (NULL != forwardPtr) {
			/* Object has been strictly (remotely) forwarded. Ensure the object is fully copied before exposing it, update the slot and return. */
			forwardHeader.copyOrWait(forwardPtr);
			slotObject.atomicWriteReferenceToSlot(object, forwardPtr);
		} else {
			omrobjectptr_t destinationObjectPtr = _extensions->scavenger->copyObject(env, &forwardHeader);
			if (NULL == destinationObjectPtr) {
				/* We have no place to copy (or less likely, we lost to another thread forwarding it).
				 * We are forced to return the original location of the object.
				 * But we must prevent any other thread of making a copy of this object.
				 * So we will attempt to atomically self forward it.  */
				forwardPtr = forwardHeader.setSelfForwardedObject();
				if (forwardPtr != object) {
					/* Some other thread successfully copied this object. Re-fetch forwarding pointer,
					 * and ensure the object is fully copied before exposing it. */
					MM_ForwardedHeader(object).copyOrWait(forwardPtr);
					slotObject.atomicWriteReferenceToSlot(object, forwardPtr);
				}
				/* ... else it's self-forwarded -> no need to update the src slot */
			} else {
				/* Successfully copied (or copied by another thread). copyObject() ensures that the object is fully copied. */
				slotObject.atomicWriteReferenceToSlot(object, destinationObjectPtr);

				env->_scavengerStats._readObjectBarrierCopy += 1;
				if (GLOBAL_READ_BARRIR_STATS_UPDATE_THRESHOLD == env->_scavengerStats._readObjectBarrierCopy) {
					MM_AtomicOperations::addU64(&_extensions->scavengerStats._readObjectBarrierCopy, GLOBAL_READ_BARRIR_STATS_UPDATE_THRESHOLD);
					env->_scavengerStats._readObjectBarrierCopy = 0;
				}

			}
		}
	}
	return true;
}
#endif

