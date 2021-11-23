/*******************************************************************************
 * Copyright (c) 1991, 2021 IBM Corp. and others
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

#include "StandardSATBAccessBarrier.hpp"
#include "AtomicOperations.hpp"
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
#include "ConcurrentGC.hpp"
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
#include "Debug.hpp"
#include "EnvironmentStandard.hpp"
#include "mmhook_internal.h"
#include "GCExtensions.hpp"
#include "ObjectModel.hpp"
#include "SublistFragment.hpp"

MM_StandardSATBAccessBarrier *
MM_StandardSATBAccessBarrier::newInstance(MM_EnvironmentBase *env)
{
	MM_StandardSATBAccessBarrier *barrier;

	barrier = (MM_StandardSATBAccessBarrier *)env->getForge()->allocate(sizeof(MM_StandardSATBAccessBarrier), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (barrier) {
		new(barrier) MM_StandardSATBAccessBarrier(env);
		if (!barrier->initialize(env)) {
			barrier->kill(env);
			barrier = NULL;
		}
	}
	return barrier;
}

void
MM_StandardSATBAccessBarrier::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

void
MM_StandardSATBAccessBarrier::initializeForNewThread(MM_EnvironmentBase* env)
{
#if defined(OMR_GC_REALTIME)
	_extensions->sATBBarrierRememberedSet->initializeFragment(env, &(((J9VMThread *)env->getLanguageVMThread())->sATBBarrierRememberedSetFragment));
#endif /* OMR_GC_REALTIME */
}

/**
 * Unmarked, heap reference, about to be deleted (or overwritten), while marking
 * is in progress is to be remembered for later marking and scanning.
 */
void
MM_StandardSATBAccessBarrier::rememberObjectToRescan(MM_EnvironmentBase *env, J9Object *object)
{
	MM_ParallelGlobalGC *globalCollector = (MM_ParallelGlobalGC *)_extensions->getGlobalCollector();

	if (globalCollector->getMarkingScheme()->markObject(env, object, true)) {
		rememberObjectImpl(env, object);
	}
}

/**
 * Unmarked, heap reference, about to be deleted (or overwritten), while marking
 * is in progress is to be remembered for later marking and scanning.
 * This method is called by MM_StandardSATBAccessBarrier::rememberObjectToRescan()
 */
void
MM_StandardSATBAccessBarrier::rememberObjectImpl(MM_EnvironmentBase *env, J9Object* object)
{
	J9VMThread *vmThread = (J9VMThread *)env->getLanguageVMThread();
#if defined(OMR_GC_REALTIME)
	_extensions->sATBBarrierRememberedSet->storeInFragment(env, &vmThread->sATBBarrierRememberedSetFragment, (UDATA *)object);
#endif /* OMR_GC_REALTIME */
}

/**
 * @copydoc MM_ObjectAccessBarrier::preObjectStore()
 *
 * Barrier for snapshot-at-the-beginning CM, the barrier is active through the end of
 * tracing. For any thread performing a store, the old value is remembered by the collector
 * before being overwritten (thus this barrier must be positioned as a pre-store barrier).
 **/
bool
MM_StandardSATBAccessBarrier::preObjectStore(J9VMThread *vmThread, J9Object *destObject, fj9object_t *destAddress, J9Object *value, bool isVolatile)
{
	return preObjectStoreImpl(vmThread, destObject, destAddress, value, isVolatile);
}

/**
 * @copydoc MM_StandardSATBAccessBarrier::preObjectStore()
 *
 * Used for stores into classes
 */
bool
MM_StandardSATBAccessBarrier::preObjectStore(J9VMThread *vmThread, J9Object *destClass, J9Object **destAddress, J9Object *value, bool isVolatile)
{
	return preObjectStoreImpl(vmThread, destAddress, value, isVolatile);
}

/**
 * @copydoc MM_StandardSATBAccessBarrier::preObjectStore()
 *
 * Used for stores into internal structures
 */
bool
MM_StandardSATBAccessBarrier::preObjectStore(J9VMThread *vmThread, J9Object **destAddress, J9Object *value, bool isVolatile)
{
	return preObjectStoreImpl(vmThread, destAddress, value, isVolatile);
}

bool
MM_StandardSATBAccessBarrier::preObjectStoreImpl(J9VMThread *vmThread, J9Object *destObject, fj9object_t *destAddress, J9Object *value, bool isVolatile)
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
MM_StandardSATBAccessBarrier::preObjectStoreImpl(J9VMThread *vmThread, J9Object **destAddress, J9Object *value, bool isVolatile)
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
 * Finds opportunities for doing the copy without or partially executing writeBarrier.
 * @return ARRAY_COPY_SUCCESSFUL if copy was successful, ARRAY_COPY_NOT_DONE no copy is done
 */
I_32
MM_StandardSATBAccessBarrier::backwardReferenceArrayCopyIndex(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots)
{
	/* TODO SATB re-enable opt? */
	return ARRAY_COPY_NOT_DONE;
}

/**
 * Finds opportunities for doing the copy without or partially executing writeBarrier.
 * @return ARRAY_COPY_SUCCESSFUL if copy was successful, ARRAY_COPY_NOT_DONE no copy is done
 */
I_32
MM_StandardSATBAccessBarrier::forwardReferenceArrayCopyIndex(J9VMThread *vmThread, J9IndexableObject *srcObject, J9IndexableObject *destObject, I_32 srcIndex, I_32 destIndex, I_32 lengthInSlots)
{
	/* TODO SATB re-enable opt? */
	return ARRAY_COPY_NOT_DONE;
}

bool
MM_StandardSATBAccessBarrier::preBatchObjectStore(J9VMThread *vmThread, J9Object *destObject, bool isVolatile)
{
	/* Assert should be removed once ReferenceArrayCopyIndex opts and JIT is enabled for SATB */
	Assert_MM_unreachable();
	return false;
}

bool
MM_StandardSATBAccessBarrier::preBatchObjectStore(J9VMThread *vmThread, J9Class *destClass, bool isVolatile)
{
	/* Assert should be removed once ReferenceArrayCopyIndex opts and JIT is enabled for SATB */
	Assert_MM_unreachable();
	return false;
}

void
MM_StandardSATBAccessBarrier::forcedToFinalizableObject(J9VMThread* vmThread, J9Object* object)
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
MM_StandardSATBAccessBarrier::referenceGet(J9VMThread *vmThread, J9Object *refObject)
{
	MM_ParallelGlobalGC *globalCollector = (MM_ParallelGlobalGC *)_extensions->getGlobalCollector();
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);

	UDATA offset = J9VMJAVALANGREFREFERENCE_REFERENT_OFFSET(vmThread);
	J9Object *referent = mixedObjectReadObject(vmThread, refObject, offset, false);

	/* Do nothing exceptional for NULL or marked referents */
	if ((NULL == referent) || (globalCollector->getMarkingScheme()->isMarked(referent))) {
		return referent;
	}

	/* Throughout tracing, we must turn any gotten reference into a root, because the
	 * thread doing the getting may already have been scanned.  However, since we are
	 * running on a mutator thread and not a gc thread we do this indirectly by putting
	 * the object in the barrier buffer.
	 */
	if (_extensions->isSATBBarrierActive()) {
		rememberObjectToRescan(env, referent);
	}

	/* We must return the external reference */
	return referent;
}

void
MM_StandardSATBAccessBarrier::referenceReprocess(J9VMThread *vmThread, J9Object *refObject)
{
	referenceGet(vmThread, refObject);
}

void
MM_StandardSATBAccessBarrier::jniDeleteGlobalReference(J9VMThread *vmThread, J9Object *reference)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
	rememberObjectToRescan(env, reference);
}

void
MM_StandardSATBAccessBarrier::storeObjectToInternalVMSlotImpl(J9VMThread *vmThread, j9object_t *destAddress, mm_j9object_t value, bool isVolatile)
{
	if (_extensions->isSATBBarrierActive()) {
		MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
		rememberObjectToRescan(env, value);
	}

	*destAddress = value;
}

mm_j9object_t
MM_StandardSATBAccessBarrier::readObjectFromInternalVMSlotImpl(J9VMThread *vmThread, j9object_t *srcAddress, bool isVolatile)
{
	mm_j9object_t object = *srcAddress;
	if (NULL != vmThread) {
		if (_extensions->isSATBBarrierActive()) {
			MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
			rememberObjectToRescan(env, object);
		}
	}
	return object;
}

void
MM_StandardSATBAccessBarrier::stringConstantEscaped(J9VMThread *vmThread, J9Object *stringConst)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);

	if (_extensions->isSATBBarrierActive()) {
		rememberObjectToRescan(env, stringConst);
	}
}

bool
MM_StandardSATBAccessBarrier::checkStringConstantsLive(J9JavaVM *javaVM, j9object_t stringOne, j9object_t stringTwo)
{
	if (_extensions->isSATBBarrierActive()) {
		J9VMThread* vmThread =  javaVM->internalVMFunctions->currentVMThread(javaVM);
		stringConstantEscaped(vmThread, (J9Object *)stringOne);
		stringConstantEscaped(vmThread, (J9Object *)stringTwo);
	}
	return true;
}

/**
 *  Equivalent to checkStringConstantsLive but for a single string constant
 */
bool
MM_StandardSATBAccessBarrier::checkStringConstantLive(J9JavaVM *javaVM, j9object_t string)
{
	if (_extensions->isSATBBarrierActive()) {
		J9VMThread* vmThread =  javaVM->internalVMFunctions->currentVMThread(javaVM);
		stringConstantEscaped(vmThread, (J9Object *)string);
	}
	return true;
}

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
bool
MM_StandardSATBAccessBarrier::checkClassLive(J9JavaVM *javaVM, J9Class *classPtr)
{
	J9ClassLoader *classLoader = classPtr->classLoader;
	bool result = false;

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

	return result;
}
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
