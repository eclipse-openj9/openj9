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
MM_StandardAccessBarrier::newInstance(MM_EnvironmentBase *env, MM_MarkingScheme *markingScheme)
{
	MM_StandardAccessBarrier *barrier;
	
	barrier = (MM_StandardAccessBarrier *)env->getForge()->allocate(sizeof(MM_StandardAccessBarrier), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (barrier) {
		new(barrier) MM_StandardAccessBarrier(env, markingScheme);
		if (!barrier->initialize(env)) {
			barrier->kill(env);
			barrier = NULL;
		}
	}
	return barrier;
}

void
MM_StandardAccessBarrier::initializeForNewThread(MM_EnvironmentBase* env)
{
#if defined(OMR_GC_REALTIME)
	if (_extensions->usingSATBBarrier()) {
		_extensions->sATBBarrierRememberedSet->initializeFragment(env, &(((J9VMThread *)env->getLanguageVMThread())->sATBBarrierRememberedSetFragment));
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
	if (_markingScheme->markObject(env, object, true)) {
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
#if defined(OMR_GC_REALTIME)
	J9VMThread *vmThread = (J9VMThread *)env->getLanguageVMThread();
	_extensions->sATBBarrierRememberedSet->storeInFragment(env, &vmThread->sATBBarrierRememberedSetFragment, (UDATA *)object);
#endif /* OMR_GC_REALTIME */
}


bool
MM_StandardAccessBarrier::preObjectStoreImpl(J9VMThread *vmThread, J9Object *destObject, fj9object_t *destAddress, J9Object *value, bool isVolatile)
{
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);

	if (_extensions->isSATBBarrierActive()) {
		if (NULL != destObject) {
			J9Object *oldObject = NULL;
			protectIfVolatileBefore(vmThread, isVolatile, true, false);
			GC_SlotObject slotObject(vmThread->javaVM->omrVM, destAddress);
			oldObject = slotObject.readReferenceFromSlot();
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

	if (_extensions->isSATBBarrierActive()) {
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
MM_StandardAccessBarrier::postBatchObjectStore(J9VMThread *vmThread, J9Object *destObject, bool isVolatile)
{
	postBatchObjectStoreImpl(vmThread, destObject);
	
	return true;
}

bool 
MM_StandardAccessBarrier::postBatchObjectStore(J9VMThread *vmThread, J9Class *destClass, bool isVolatile)
{
	j9object_t destObject = J9VM_J9CLASS_TO_HEAPCLASS(destClass);
	
	postBatchObjectStoreImpl(vmThread, destObject);
	
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
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		if (_extensions->isConcurrentScavengerEnabled() && !_extensions->isScavengerBackOutFlagRaised()) {
			Assert_MM_false(_scavenger->isObjectInEvacuateMemory(dstObject));
			Assert_MM_false(_scavenger->isObjectInEvacuateMemory(srcObject));
		}
#endif /* OMR_GC_CONCURRENT_SCAVENGER */

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
		/* Call the concurrent write barrier if required */
		if(isIncrementalUpdateBarrierActive(vmThread) && _extensions->isOld(dstObject)) {
			concurrentPostWriteBarrierStore(vmThread->omrVMThread, dstObject, srcObject);
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
MM_StandardAccessBarrier::postBatchObjectStoreImpl(J9VMThread *vmThread, J9Object *dstObject)
{
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	Assert_MM_true(!_extensions->usingSATBBarrier());
	/* Call the concurrent write barrier if required */
	if(_extensions->concurrentMark && 
		(vmThread->privateFlags & J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE) &&
		_extensions->isOld(dstObject)) {
		concurrentPostWriteBarrierBatchStore(vmThread->omrVMThread, dstObject);
	}
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */

#if defined(J9VM_GC_GENERATIONAL)
	_generationalAccessBarrierComponent.postBatchObjectStore(vmThread, dstObject);
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
				extensions->setScavengerRememberedSetOverflowState();
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
	J9IndexableObject *arrayObject = (J9IndexableObject *)J9_JNI_UNWRAP_REFERENCE(array);
	GC_ArrayObjectModel *indexableObjectModel = &_extensions->indexableObjectModel;

	bool shouldCopy = false;
	bool alwaysCopyInCritical = (javaVM->runtimeFlags & J9_RUNTIME_ALWAYS_COPY_JNI_CRITICAL) == J9_RUNTIME_ALWAYS_COPY_JNI_CRITICAL;
	if (alwaysCopyInCritical) {
		shouldCopy = true;
	}

	if (shouldCopy) {
		VM_VMAccess::inlineEnterVMFromJNI(vmThread);
		copyArrayCritical(vmThread, indexableObjectModel, functions, &data, arrayObject, isCopy);
		VM_VMAccess::inlineExitVMToJNI(vmThread);
	} else {
		// acquire access and return a direct pointer
		MM_JNICriticalRegion::enterCriticalRegion(vmThread, false);
		data = (void *)indexableObjectModel->getDataPointerForContiguous(arrayObject);
		if (NULL != isCopy) {
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
	GC_ArrayObjectModel *indexableObjectModel = &_extensions->indexableObjectModel;
	J9IndexableObject *arrayObject = (J9IndexableObject *)J9_JNI_UNWRAP_REFERENCE(array);

	bool shouldCopy = false;
	bool alwaysCopyInCritical = (javaVM->runtimeFlags & J9_RUNTIME_ALWAYS_COPY_JNI_CRITICAL) == J9_RUNTIME_ALWAYS_COPY_JNI_CRITICAL;
	if (alwaysCopyInCritical) {
		shouldCopy = true;
	}

	if (shouldCopy) {
		VM_VMAccess::inlineEnterVMFromJNI(vmThread);
		copyBackArrayCritical(vmThread, indexableObjectModel, functions, elems, &arrayObject, mode);
		VM_VMAccess::inlineExitVMToJNI(vmThread);
	} else {
		/*
		 * Objects can not be moved if critical section is active
		 * This trace point will be generated if object has been moved or passed value of elems is corrupted
		 */
		void *data = (void *)indexableObjectModel->getDataPointerForContiguous(arrayObject);
		if (elems != data) {
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
	} else if (_extensions->isConcurrentScavengerEnabled()) {
		/* reading value field from String object may trigger object movement */
		VM_VMAccess::inlineEnterVMFromJNI(vmThread);
		hasVMAccess = true;
	}

	if (shouldCopy) {
		J9Object *stringObject = (J9Object*)J9_JNI_UNWRAP_REFERENCE(str);
		J9IndexableObject *valueObject = (J9IndexableObject*)J9VMJAVALANGSTRING_VALUE(vmThread, stringObject);

		if (IS_STRING_COMPRESSED(vmThread, stringObject)) {
				isCompressed = true;
		}

		copyStringCritical(vmThread, &_extensions->indexableObjectModel, functions, &data, javaVM, valueObject, stringObject, isCopy, isCompressed);
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
		freeStringCritical(vmThread, functions, elems);
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

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
I_32
MM_StandardAccessBarrier::doCopyContiguousBackwardWithReadBarrier(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots)
{
	srcIndex += lengthInSlots;
	destIndex += lengthInSlots;

	if (J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread)) {
		uint32_t *srcSlot = (uint32_t *)indexableEffectiveAddress(vmThread, srcObject, srcIndex, sizeof(uint32_t));
		uint32_t *destSlot = (uint32_t *)indexableEffectiveAddress(vmThread, destObject, destIndex, sizeof(uint32_t));
		uint32_t *srcEndSlot = srcSlot - lengthInSlots;

		while (srcSlot-- > srcEndSlot) {
			preObjectRead(vmThread, (J9Object *)srcObject, (fj9object_t*)srcSlot);

			*--destSlot = *srcSlot;
		}
	} else {
		uintptr_t *srcSlot = (uintptr_t *)indexableEffectiveAddress(vmThread, srcObject, srcIndex, sizeof(uintptr_t));
		uintptr_t *destSlot = (uintptr_t *)indexableEffectiveAddress(vmThread, destObject, destIndex, sizeof(uintptr_t));
		uintptr_t *srcEndSlot = srcSlot - lengthInSlots;

		while (srcSlot-- > srcEndSlot) {
			preObjectRead(vmThread, (J9Object *)srcObject, (fj9object_t*)srcSlot);

			*--destSlot = *srcSlot;
		}	
	}

	return ARRAY_COPY_SUCCESSFUL;
}

I_32
MM_StandardAccessBarrier::doCopyContiguousForwardWithReadBarrier(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots)
{
	if (J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread)) {
		uint32_t *srcSlot = (uint32_t *)indexableEffectiveAddress(vmThread, srcObject, srcIndex, sizeof(uint32_t));
		uint32_t *destSlot = (uint32_t *)indexableEffectiveAddress(vmThread, destObject, destIndex, sizeof(uint32_t));
		uint32_t *srcEndSlot = srcSlot + lengthInSlots;

		while (srcSlot < srcEndSlot) {
			preObjectRead(vmThread, (J9Object *)srcObject, (fj9object_t*)srcSlot);
	
			*destSlot++ = *srcSlot++;
		}
	} else {
		uintptr_t *srcSlot = (uintptr_t *)indexableEffectiveAddress(vmThread, srcObject, srcIndex, sizeof(uintptr_t));
		uintptr_t *destSlot = (uintptr_t *)indexableEffectiveAddress(vmThread, destObject, destIndex, sizeof(uintptr_t));
		uintptr_t *srcEndSlot = srcSlot + lengthInSlots;

		while (srcSlot < srcEndSlot) {
			preObjectRead(vmThread, (J9Object *)srcObject, (fj9object_t*)srcSlot);
	
			*destSlot++ = *srcSlot++;
		}
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
	/* TODO SATB re-enable opt? */
	if (_extensions->usingSATBBarrier()) {
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

		postBatchObjectStoreImpl(vmThread, (J9Object *)destObject);
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
	/* TODO SATB re-enable opt */
	I_32 retValue = ARRAY_COPY_NOT_DONE;

	if (_extensions->usingSATBBarrier()) {
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
		postBatchObjectStoreImpl(vmThread, (J9Object *)destObject);
	}
	return retValue;
}

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
			J9Class *j9class = J9GC_J9OBJECT_CLAZZ_THREAD(toConvert, vmThread);
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
MM_StandardAccessBarrier::preWeakRootSlotRead(J9VMThread *vmThread, j9object_t *srcAddress)
{
	omrobjectptr_t object = (omrobjectptr_t)*srcAddress;
	bool const compressed = compressObjectReferences();

	if ((NULL != _scavenger) && _scavenger->isObjectInEvacuateMemory(object)) {
		MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(vmThread->omrVMThread);
		Assert_MM_true(_scavenger->isConcurrentCycleInProgress());
		Assert_MM_true(_scavenger->isMutatorThreadInSyncWithCycle(env));

		MM_ForwardedHeader forwardHeader(object, compressed);
		omrobjectptr_t forwardPtr = forwardHeader.getForwardedObject();

		if (NULL != forwardPtr) {
			/* Object has been or is being copied - ensure the object is fully copied before exposing it, update the slot and return.
			 *  Slot update needs to be atomic, only if there is a mutator thread racing with a write operation to this same slot.
			 *  The barrier is typically used to update Monitor table entries, which should not be ever reinitialized by any mutator.
			 *  Updated can occur, but only by GC threads during STW clearing phase, thus no race with this barrier. */
			forwardHeader.copyOrWait(forwardPtr);
			*srcAddress = forwardPtr;
		}
		/* Do nothing if the object is not copied already, since it might be dead.
		 * This object is found without real reference to it,
		 * for example by iterating monitor table to dump info about all monitors */
	}

	return true;
}

bool
MM_StandardAccessBarrier::preWeakRootSlotRead(J9JavaVM *vm, j9object_t *srcAddress)
{
	omrobjectptr_t object = (omrobjectptr_t)*srcAddress;
	bool const compressed = compressObjectReferences();

	if ((NULL != _scavenger) && _scavenger->isObjectInEvacuateMemory(object)) {
		Assert_MM_true(_scavenger->isConcurrentCycleInProgress());

		MM_ForwardedHeader forwardHeader(object, compressed);
		omrobjectptr_t forwardPtr = forwardHeader.getForwardedObject();

		if (NULL != forwardPtr) {
			/* Object has been or is being copied - ensure the object is fully copied before exposing it, update the slot and return */
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
	bool const compressed = compressObjectReferences();

	if ((NULL != _scavenger) && _scavenger->isObjectInEvacuateMemory(object)) {
		MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(vmThread->omrVMThread);
		Assert_MM_true(_scavenger->isConcurrentCycleInProgress());
		Assert_MM_true(_scavenger->isMutatorThreadInSyncWithCycle(env));

		MM_ForwardedHeader forwardHeader(object, compressed);
		omrobjectptr_t forwardPtr = forwardHeader.getForwardedObject();

		if (NULL != forwardPtr) {
			/* Object has been strictly (remotely) forwarded. Ensure the object is fully copied before exposing it, update the slot and return. */
			forwardHeader.copyOrWait(forwardPtr);
			MM_AtomicOperations::lockCompareExchange((uintptr_t *)srcAddress, (uintptr_t)object, (uintptr_t)forwardPtr);
		} else {
			omrobjectptr_t destinationObjectPtr = _scavenger->copyObject(env, &forwardHeader);
			if (NULL == destinationObjectPtr) {
				/* Failure - the scavenger must back out the work it has done. Attempt to return the original object. */
				forwardPtr = forwardHeader.setSelfForwardedObject();
				if (forwardPtr != object) {
					/* Another thread successfully copied the object. Re-fetch forwarding pointer,
					 * and ensure the object is fully copied before exposing it. */
					MM_ForwardedHeader(object, compressed).copyOrWait(forwardPtr);
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
	/* with volatile cast, ensure that we are really getting a snapshot (instead of the slot being re-read at later points with possibly different values) */
	bool const compressed = compressObjectReferences();
	fomrobject_t objectToken = (fomrobject_t)(compressed ? (uintptr_t)*(volatile uint32_t *)srcAddress: *(volatile uintptr_t *)srcAddress);
	omrobjectptr_t object = convertPointerFromToken(objectToken);

	if (NULL != _scavenger) {
		MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(vmThread->omrVMThread);
		Assert_GC_true_with_message(env, !_scavenger->isObjectInEvacuateMemory((omrobjectptr_t)srcAddress) || _extensions->isScavengerBackOutFlagRaised(), "readObject %llx in Evacuate\n", srcAddress);
		if (_scavenger->isObjectInEvacuateMemory(object)) {
			Assert_GC_true_with_message2(env, _scavenger->isConcurrentCycleInProgress(),
					"CS not in progress, found a object in Survivor: slot %llx object %llx\n", srcAddress, object);
			Assert_MM_true(_scavenger->isMutatorThreadInSyncWithCycle(env));
			/* since object is still in evacuate, srcObject has not been scanned yet => we cannot assert
			 * if srcObject should (already) be remembered (even if it's old)
			 */

			env->_scavengerStats._readObjectBarrierUpdate += 1;
			if (GLOBAL_READ_BARRIR_STATS_UPDATE_THRESHOLD == env->_scavengerStats._readObjectBarrierUpdate) {
				MM_AtomicOperations::addU64(&_extensions->scavengerStats._readObjectBarrierUpdate, GLOBAL_READ_BARRIR_STATS_UPDATE_THRESHOLD);
				env->_scavengerStats._readObjectBarrierUpdate = 0;
			}

			GC_SlotObject slotObject(env->getOmrVM(), srcAddress);
			MM_ForwardedHeader forwardHeader(object, compressed);
			omrobjectptr_t forwardPtr = forwardHeader.getForwardedObject();
			if (NULL != forwardPtr) {
				/* Object has been strictly (remotely) forwarded. Ensure the object is fully copied before exposing it, update the slot and return. */
				forwardHeader.copyOrWait(forwardPtr);
				slotObject.atomicWriteReferenceToSlot(object, forwardPtr);
			} else {
				omrobjectptr_t destinationObjectPtr = _scavenger->copyObject(env, &forwardHeader);
				if (NULL == destinationObjectPtr) {
					/* We have no place to copy (or less likely, we lost to another thread forwarding it).
					 * We are forced to return the original location of the object.
					 * But we must prevent any other thread of making a copy of this object.
					 * So we will attempt to atomically self forward it.  */
					forwardPtr = forwardHeader.setSelfForwardedObject();
					if (forwardPtr != object) {
						/* Some other thread successfully copied this object. Re-fetch forwarding pointer,
						 * and ensure the object is fully copied before exposing it. */
						MM_ForwardedHeader(object, compressed).copyOrWait(forwardPtr);
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
	}
	return true;
}
#endif

void
MM_StandardAccessBarrier::forcedToFinalizableObject(J9VMThread* vmThread, J9Object* object)
{
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
	if (_extensions->isSATBBarrierActive()) {
		rememberObjectToRescan(env, object);
	}
}

/**
 * Override of referenceGet.  When the collector is tracing, it makes any gotten object "grey" to ensure
 * that it is eventually traced.
 *
 * @param refObject the SoftReference or WeakReference object on which get() is being called.
 *	This barrier must not be called for PhantomReferences.  The parameter must not be NULL.
 */
J9Object *
MM_StandardAccessBarrier::referenceGet(J9VMThread *vmThread, J9Object *refObject)
{
	J9Object *referent = J9VMJAVALANGREFREFERENCE_REFERENT_VM(vmThread->javaVM, refObject);

	/* SATB - Throughout tracing, we must turn any gotten reference into a root, because the
	 * thread doing the getting may already have been scanned.  However, since we are
	 * running on a mutator thread and not a gc thread we do this indirectly by putting
	 * the object in the barrier buffer.
	 *
	 * Do nothing exceptional for NULL or marked referents
	 */
	if ((_extensions->isSATBBarrierActive()) && (NULL != referent) && (!_markingScheme->isMarked(referent))) {
		MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
		rememberObjectToRescan(env, referent);
	}

	/* We must return the external reference */
	return referent;
}

void
MM_StandardAccessBarrier::referenceReprocess(J9VMThread *vmThread, J9Object *refObject)
{
	if (_extensions->usingSATBBarrier()) {
		referenceGet(vmThread, refObject);
	} else {
		postBatchObjectStore(vmThread, refObject);
	}
}

void
MM_StandardAccessBarrier::jniDeleteGlobalReference(J9VMThread *vmThread, J9Object *reference)
{
	if (_extensions->isSATBBarrierActive()) {
		MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
		rememberObjectToRescan(env, reference);
	}
}

void
MM_StandardAccessBarrier::stringConstantEscaped(J9VMThread *vmThread, J9Object *stringConst)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);

	if (_extensions->isSATBBarrierActive()) {
		rememberObjectToRescan(env, stringConst);
	}
}

bool
MM_StandardAccessBarrier::checkStringConstantsLive(J9JavaVM *javaVM, j9object_t stringOne, j9object_t stringTwo)
{
	if (_extensions->isSATBBarrierActive()) {
		J9VMThread *vmThread = javaVM->internalVMFunctions->currentVMThread(javaVM);
		stringConstantEscaped(vmThread, (J9Object *)stringOne);
		if (stringOne != stringTwo) {
			stringConstantEscaped(vmThread, (J9Object *)stringTwo);
		}
	}

	return true;
}

/**
 *  Equivalent to checkStringConstantsLive but for a single string constant
 */
bool
MM_StandardAccessBarrier::checkStringConstantLive(J9JavaVM *javaVM, j9object_t string)
{
	if (_extensions->isSATBBarrierActive()) {
		J9VMThread* vmThread =  javaVM->internalVMFunctions->currentVMThread(javaVM);
		stringConstantEscaped(vmThread, (J9Object *)string);
	}

	return true;
}

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
bool
MM_StandardAccessBarrier::checkClassLive(J9JavaVM *javaVM, J9Class *classPtr)
{
	bool result = true;

	if (_extensions->usingSATBBarrier()) {
		J9ClassLoader *classLoader = classPtr->classLoader;
		result = false;

		if ((0 == (classLoader->gcFlags & J9_GC_CLASS_LOADER_DEAD)) && (0 == (J9CLASS_FLAGS(classPtr) & J9AccClassDying))) {
			result = true;
			/* this class has not been discovered dead yet so mark it if necessary to force it to be alive */
			J9Object *classLoaderObject = classLoader->classLoaderObject;

			if (NULL != classLoaderObject) {
				/*  If mark is active but not completed yet force this class to be marked to survive this GC */
				J9VMThread* vmThread =  javaVM->internalVMFunctions->currentVMThread(javaVM);
				MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
				if (_extensions->isSATBBarrierActive()) {
					rememberObjectToRescan(env, classLoaderObject);
				}
			}
			/* else this class loader probably is in initialization process and class loader object has not been attached yet */
		}
	}

	return result;
}
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */

void
MM_StandardAccessBarrier::preMountContinuation(J9VMThread *vmThread, j9object_t contObject)
{
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	if (_extensions->isConcurrentScavengerInProgress()) {
		/* concurrent scavenger in active */
		MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(vmThread->omrVMThread);
		MM_ScavengeScanReason reason = SCAN_REASON_SCAVENGE;
		const bool beingMounted = true;
		_scavenger->getDelegate()->scanContinuationNativeSlots(env, contObject, reason, beingMounted);
	}
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
}

void
MM_StandardAccessBarrier::postUnmountContinuation(J9VMThread *vmThread, j9object_t contObject)
{
	/* Conservatively assume that via mutations of stack slots (which are not subject to access barriers),
	 * all post-write barriers have been triggered on this Continuation object, since it's been mounted.
	 */
	postBatchObjectStore(vmThread, contObject);
}
