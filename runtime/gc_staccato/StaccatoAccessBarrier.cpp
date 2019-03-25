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


#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "StaccatoAccessBarrier.hpp"
#include "StaccatoGC.hpp"

#include "RealtimeMarkingScheme.hpp"

/**
 * Static method for instantiating the access barrier.
 */
MM_StaccatoAccessBarrier *
MM_StaccatoAccessBarrier::newInstance(MM_EnvironmentBase *env)
{
	MM_StaccatoAccessBarrier *barrier;
	
	barrier = (MM_StaccatoAccessBarrier *)env->getForge()->allocate(sizeof(MM_StaccatoAccessBarrier), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (barrier) {
		new(barrier) MM_StaccatoAccessBarrier(env);
		if (!barrier->initialize(env)) {
			barrier->kill(env);
			barrier = NULL;
		}
	}
	return barrier;
}

/**
 * Initializes the access barrier, returning false will cause the instantiation to fail.
 */
bool 
MM_StaccatoAccessBarrier::initialize(MM_EnvironmentBase *env)
{
	return MM_RealtimeAccessBarrier::initialize(env);
}

void 
MM_StaccatoAccessBarrier::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

void
MM_StaccatoAccessBarrier::tearDown(MM_EnvironmentBase *env)
{
	MM_RealtimeAccessBarrier::tearDown(env);
}

/**
 * Unmarked, heap reference, about to be deleted (or overwritten), while marking
 * is in progress is to be remembered for later marking and scanning.
 * This method is called by MM_RealtimeAccessBarrier::rememberObject()
 */
void
MM_StaccatoAccessBarrier::rememberObjectImpl(MM_EnvironmentBase *env, J9Object* object)
{
	J9VMThread *vmThread = (J9VMThread *)env->getLanguageVMThread();
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(vmThread->javaVM);

	extensions->sATBBarrierRememberedSet->storeInFragment(env, &vmThread->sATBBarrierRememberedSetFragment, (UDATA *)object);
}

void
MM_StaccatoAccessBarrier::forcedToFinalizableObject(J9VMThread* vmThread, J9Object* object)
{
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
	if (isBarrierActive(env)) {
		rememberObject(env, object);
	}
}

/**
 * @copydoc MM_ObjectAccessBarrier::preObjectStore()
 * 
 * This is the implementation of the staccato write barrier.
 * 
 * Staccato uses a snapshot-at-the-beginning algorithm, but with a fuzzy snapshot in the
 * sense that threads are allowed to run during the root scan.  This requires a "double
 * barrier."  The barrier is active from the start of root scanning through the end of
 * tracing.  For an unscanned thread performing a store, the new value is remembered by
 * the collector.  For any thread performing a store (whether scanned or not), the old
 * value is remembered by the collector before being overwritten (thus this barrier must be
 * positioned as a pre-store barrier).  For the latter ("Yuasa barrier") aspect of the
 * double barrier, only the first overwritten value needs to be remembered (remembering 
 * others is harmless but not needed), and so we omit synchronization on the reading of the 
 * old value.
 */
bool
MM_StaccatoAccessBarrier::preObjectStoreInternal(J9VMThread *vmThread, J9Object *destObject, fj9object_t *destAddress, J9Object *value, bool isVolatile)
{
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
	
	if (isBarrierActive(env)) {
		if (NULL != destObject) {
			if (isDoubleBarrierActiveOnThread(vmThread)) {
				rememberObject(env, value);
			}
		
			J9Object *oldObject = NULL;
			protectIfVolatileBefore(vmThread, isVolatile, true, false);
			oldObject = mmPointerFromToken(vmThread, *destAddress);
			protectIfVolatileAfter(vmThread, isVolatile, true, false);
			rememberObject(env, oldObject);
		}
	}

	return true;
}

/**
 * @copydoc MM_ObjectAccessBarrier::preObjectStore()
 * 
 * This is the implementation of the staccato write barrier.
 * 
 * Staccato uses a snapshot-at-the-beginning algorithm, but with a fuzzy snapshot in the
 * sense that threads are allowed to run during the root scan.  This requires a "double
 * barrier."  The barrier is active from the start of root scanning through the end of
 * tracing.  For an unscanned thread performing a store, the new value is remembered by
 * the collector.  For any thread performing a store (whether scanned or not), the old
 * value is remembered by the collector before being overwritten (thus this barrier must be
 * positioned as a pre-store barrier).  For the latter ("Yuasa barrier") aspect of the
 * double barrier, only the first overwritten value needs to be remembered (remembering 
 * others is harmless but not needed), and so we omit synchronization on the reading of the 
 * old value.
 */
bool
MM_StaccatoAccessBarrier::preObjectStoreInternal(J9VMThread *vmThread, J9Object **destAddress, J9Object *value, bool isVolatile)
{
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
	
	if (isBarrierActive(env)) {
		if (isDoubleBarrierActiveOnThread(vmThread)) {
			rememberObject(env, value);
		}
		J9Object* oldObject = NULL;
		protectIfVolatileBefore(vmThread, isVolatile, true, false);
		oldObject = *destAddress;
		protectIfVolatileAfter(vmThread, isVolatile, true, false);
		rememberObject(env, oldObject);
	}
	
	return true;
}

bool
MM_StaccatoAccessBarrier::preObjectStoreInternal(J9VMThread *vmThread, J9Object* destClass, J9Object **destAddress, J9Object *value, bool isVolatile)
{
	/* the destClass argument is ignored, so just call the generic slot version */
	return preObjectStoreInternal(vmThread, destAddress, value, isVolatile);
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
MM_StaccatoAccessBarrier::preObjectStore(J9VMThread *vmThread, J9Object *destObject, fj9object_t *destAddress, J9Object *value, bool isVolatile)
{
	return preObjectStoreInternal(vmThread, destObject, destAddress, value, isVolatile);
}

/**
 * @copydoc MM_MetronomeAccessBarrier::preObjectStore()
 * 
 * Used for stores into classes
 */
bool
MM_StaccatoAccessBarrier::preObjectStore(J9VMThread *vmThread, J9Object *destClass, J9Object **destAddress, J9Object *value, bool isVolatile)
{
	return preObjectStoreInternal(vmThread, destClass, destAddress, value, isVolatile);
}

/**
 * @copydoc MM_MetronomeAccessBarrier::preObjectStore()
 * 
 * Used for stores into internal structures
 */
bool
MM_StaccatoAccessBarrier::preObjectStore(J9VMThread *vmThread, J9Object **destAddress, J9Object *value, bool isVolatile)
{
	return preObjectStoreInternal(vmThread, destAddress, value, isVolatile);
}

/**
 * Enables the double barrier on the provided thread.
 */
void
MM_StaccatoAccessBarrier::setDoubleBarrierActiveOnThread(MM_EnvironmentBase* env)
{
	MM_GCExtensions::getExtensions(env)->sATBBarrierRememberedSet->preserveLocalFragmentIndex(env, &(((J9VMThread *)env->getLanguageVMThread())->sATBBarrierRememberedSetFragment));
}

/**
 * Disables the double barrier on the provided thread.
 */
void
MM_StaccatoAccessBarrier::setDoubleBarrierInactiveOnThread(MM_EnvironmentBase* env)
{
	MM_GCExtensions::getExtensions(env)->sATBBarrierRememberedSet->restoreLocalFragmentIndex(env, &(((J9VMThread *)env->getLanguageVMThread())->sATBBarrierRememberedSetFragment));
}

void
MM_StaccatoAccessBarrier::initializeForNewThread(MM_EnvironmentBase* env)
{
	MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(env);
	extensions->sATBBarrierRememberedSet->initializeFragment(env, &(((J9VMThread *)env->getLanguageVMThread())->sATBBarrierRememberedSetFragment));
	if (isDoubleBarrierActive()) {
		setDoubleBarrierActiveOnThread(env);
	}
}

/* TODO: meter this scanning and include into utilization tracking */
void
MM_StaccatoAccessBarrier::scanContiguousArray(MM_EnvironmentRealtime *env, J9IndexableObject *objectPtr)
{
	J9JavaVM *vm = (J9JavaVM *)env->getLanguageVM();
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	if(_realtimeGC->getRealtimeDelegate()->isDynamicClassUnloadingEnabled()) {
		rememberObject(env, (J9Object *)objectPtr);
	}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */		

#if !defined(J9VM_GC_HYBRID_ARRAYLETS)
	/* Since we are already in gc_staccato, not defined GC_HYBRID_ARRAYLET implies GC_ARRAYLETS is defined.
	 * first (and only) arraylet address
	 */
	fj9object_t *arrayoid = _extensions->indexableObjectModel.getArrayoidPointer(objectPtr);
	GC_SlotObject slotObject(vm->omrVM, &arrayoid[0]);
	fj9object_t *startScanPtr = (fj9object_t*) (slotObject.readReferenceFromSlot());

	if (NULL != startScanPtr) {
		/* Very small arrays cannot be set as scanned (no scanned bit in Mark Map reserved for them) */
		UDATA arrayletSize = _extensions->indexableObjectModel.arrayletSize(objectPtr, /* arraylet index */ 0);		

		fj9object_t *endScanPtr = startScanPtr + arrayletSize / sizeof(fj9object_t);
		fj9object_t *scanPtr = startScanPtr;
#else /* J9VM_GC_HYBRID_ARRAYLETS */
		/* if NUA is enabled, separate path for contiguous arrays */
		fj9object_t *scanPtr = (fj9object_t*) _extensions->indexableObjectModel.getDataPointerForContiguous(objectPtr);
		fj9object_t *endScanPtr = scanPtr + _extensions->indexableObjectModel.getSizeInElements(objectPtr);
#endif /* J9VM_GC_HYBRID_ARRAYLETS */
		while(scanPtr < endScanPtr) {
			/* since this is done from an external thread, we do not markObject, but rememberObject */
			GC_SlotObject slotObject(vm->omrVM, scanPtr);
			J9Object *field = slotObject.readReferenceFromSlot();
			rememberObject(env, field);
			scanPtr++;
		}
		
		/* this method assumes the array is large enough to set scan bit */
		_markingScheme->setScanAtomic((J9Object *)objectPtr);
#if !defined(J9VM_GC_HYBRID_ARRAYLETS)
	}
#endif /* J9VM_GC_HYBRID_ARRAYLETS */
}

bool 
MM_StaccatoAccessBarrier::markAndScanContiguousArray(MM_EnvironmentRealtime *env, J9IndexableObject *objectPtr)
{
	UDATA arrayletSize = _extensions->indexableObjectModel.arrayletSize(objectPtr, /* arraylet index */ 0);
	
	/* Sufficiently large to have a scan bit? */
	if (arrayletSize < _extensions->minArraySizeToSetAsScanned) {
		return false;
	} else if (!_markingScheme->isScanned((J9Object *)objectPtr)) {
		/* No, not scanned yet. We are going to mark it and scan right away */
		_markingScheme->markAtomic((J9Object *)objectPtr);
		/* The array might have been marked already (which means it will be scanned soon,
		 * or even being scanned at the moment). Regardless, we will proceed with scanning it */
		scanContiguousArray(env, objectPtr);
	}
	
	return true;
}

/**
 * Finds opportunities for doing the copy without executing Metronome WriteBarrier.
 * @return ARRAY_COPY_SUCCESSFUL if copy was succesful, ARRAY_COPY_NOT_DONE no copy is done
 */
I_32
MM_StaccatoAccessBarrier::backwardReferenceArrayCopyIndex(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots)
{
	MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(vmThread->omrVMThread);
	
	/* a high level caller ensured destObject == srcObject */
		
	if (_extensions->indexableObjectModel.isInlineContiguousArraylet(destObject)) {
		
		if (isBarrierActive(env)) {

			if (!markAndScanContiguousArray(env, destObject)) {
				return ARRAY_COPY_NOT_DONE;
			}
		}

		return doCopyContiguousBackward(vmThread, srcObject, destObject, srcIndex, destIndex, lengthInSlots);

	}
	
	return -2;
}

/**
 * Finds opportunities for doing the copy without executing Metronome WriteBarrier.
 * @return ARRAY_COPY_SUCCESSFUL if copy was succesful, ARRAY_COPY_NOT_DONE no copy is done
 */
I_32
MM_StaccatoAccessBarrier::forwardReferenceArrayCopyIndex(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots)
{
	MM_EnvironmentRealtime *env = MM_EnvironmentRealtime::getEnvironment(vmThread->omrVMThread);

	if (_extensions->indexableObjectModel.isInlineContiguousArraylet(destObject)
			&& _extensions->indexableObjectModel.isInlineContiguousArraylet(srcObject)) {
		
		if (isBarrierActive(env) ) {
			
			if ((destObject != srcObject) && isDoubleBarrierActiveOnThread(vmThread)) {
				return ARRAY_COPY_NOT_DONE;
			} else {
				if (markAndScanContiguousArray(env, destObject)) {
					return doCopyContiguousForward(vmThread, srcObject, destObject, srcIndex, destIndex, lengthInSlots);
				}
			}
			
		} else {

			return doCopyContiguousForward(vmThread, srcObject, destObject, srcIndex, destIndex, lengthInSlots);
		
		}
	}

	return -2;
}



