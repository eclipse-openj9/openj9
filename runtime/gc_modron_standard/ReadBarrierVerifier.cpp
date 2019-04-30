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


#include "ReadBarrierVerifier.hpp"

#include "AtomicOperations.hpp"
#include "Debug.hpp"
#include "EnvironmentStandard.hpp"
#include "HeapRegionManager.hpp"
#include "j9cfg.h"
#include "mmhook_internal.h"
#include "ModronAssertions.h"

#include "RootScannerReadBarrierVerifier.hpp"
#include "Scavenger.hpp"
#include "SublistFragment.hpp"

#if defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS)

MM_ReadBarrierVerifier *
MM_ReadBarrierVerifier::newInstance(MM_EnvironmentBase *env)
{
	MM_ReadBarrierVerifier *barrier;

	barrier = (MM_ReadBarrierVerifier *)env->getForge()->allocate(sizeof(MM_ReadBarrierVerifier), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (barrier) {
		new(barrier) MM_ReadBarrierVerifier(env);
		if (!barrier->initialize(env)) {
			barrier->kill(env);
			barrier = NULL;
		}
	}

	return barrier;
}

bool
MM_ReadBarrierVerifier::initialize(MM_EnvironmentBase *env)
{
	return MM_StandardAccessBarrier::initialize(env);
}

void 
MM_ReadBarrierVerifier::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

void
MM_ReadBarrierVerifier::tearDown(MM_EnvironmentBase *env)
{
	MM_StandardAccessBarrier::tearDown(env);
}
bool
MM_ReadBarrierVerifier::preObjectRead(J9VMThread *vmThread, J9Object *srcObject, fj9object_t *srcAddress)
{
	Assert_MM_true(vmThread->javaVM->internalVMFunctions->currentVMThread(vmThread->javaVM) == vmThread);
	healSlot(_extensions, srcAddress);

	return true;
}

void
MM_ReadBarrierVerifier::poisonSlot(MM_GCExtensionsBase *extensions, omrobjectptr_t *slot)
{
	uintptr_t heapBase = (uintptr_t)extensions->heap->getHeapBase();
	uintptr_t heapTop = (uintptr_t)extensions->heap->getHeapTop();
	uintptr_t referenceFromSlot = (uintptr_t)*slot;

	if ((heapTop > referenceFromSlot) && (heapBase <= referenceFromSlot)) {
		uintptr_t shadowHeapBase = (uintptr_t)extensions->shadowHeapBase;
		*slot = (omrobjectptr_t) (shadowHeapBase + (referenceFromSlot - heapBase));
	}
}

void
MM_ReadBarrierVerifier::poisonJniWeakReferenceSlots(MM_EnvironmentBase *env)
{
	MM_RootScannerReadBarrierVerifier scanner(env, true, true);
	scanner.scanJNIWeakGlobalReferences(env);
}

void
MM_ReadBarrierVerifier::poisonMonitorReferenceSlots(MM_EnvironmentBase *env)
{
	MM_RootScannerReadBarrierVerifier scanner(env, true, true);
	scanner.scanMonitorReferences(env);
}

void
MM_ReadBarrierVerifier::poisonStaticClassSlots(MM_EnvironmentBase *env)
{
	MM_RootScannerReadBarrierVerifier scanner(env, true, true);
	scanner.scanClassStaticSlots(env);
}

void
MM_ReadBarrierVerifier::poisonConstantPoolObjects(MM_EnvironmentBase *env)
{
	MM_RootScannerReadBarrierVerifier scanner(env, true, true);
	scanner.scanConstantPoolObjectSlots(env);
}

void
MM_ReadBarrierVerifier::poisonSlots(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	if (1 == extensions->fvtest_enableJNIGlobalWeakReadBarrierVerification) {
		poisonJniWeakReferenceSlots(env);
	}
	if (1 == extensions->fvtest_enableMonitorObjectsReadBarrierVerification) {
		poisonMonitorReferenceSlots(env);
	}
	if (1 == extensions->fvtest_enableClassStaticsReadBarrierVerification) {
		poisonStaticClassSlots(env);
		poisonConstantPoolObjects(env);
	}
}

void
MM_ReadBarrierVerifier::healSlot(MM_GCExtensionsBase *extensions, fomrobject_t *srcAddress)
{
	uintptr_t shadowHeapBase = (uintptr_t)extensions->shadowHeapBase;
	uintptr_t shadowHeapTop = (uintptr_t)extensions->shadowHeapTop;
	omrobjectptr_t object = convertPointerFromToken(*srcAddress);

	if ((shadowHeapTop > (uintptr_t)object) && (shadowHeapBase <= (uintptr_t)object)) {

		uintptr_t heapBase = (uintptr_t)extensions->heap->getHeapBase();
		uintptr_t healedAddress = heapBase + ((uintptr_t)object - shadowHeapBase);

		GC_SlotObject slotObject(extensions->getOmrVM(), srcAddress);
		/* We don't care if the write fails, some other thread probably wrote a healed value to the slot */
		slotObject.atomicWriteReferenceToSlot(object, (omrobjectptr_t)healedAddress);

	}
}

void
MM_ReadBarrierVerifier::healSlot(MM_GCExtensionsBase *extensions, omrobjectptr_t *slot)
{
	uintptr_t shadowHeapBase = (uintptr_t)extensions->shadowHeapBase;
	uintptr_t shadowHeapTop = (uintptr_t)extensions->shadowHeapTop;
	uintptr_t referenceFromSlot = (uintptr_t)*slot;

	if ((shadowHeapTop > referenceFromSlot) && (shadowHeapBase <= referenceFromSlot)) {

		uintptr_t heapBase = (uintptr_t)extensions->heap->getHeapBase();
		uintptr_t healedHeapAddress = heapBase + (referenceFromSlot - shadowHeapBase);

		/* We don't care if the write fails, some other thread probably wrote a healed value to the slot */
		MM_AtomicOperations::lockCompareExchange((uintptr_t *)slot, referenceFromSlot, healedHeapAddress);

	}
}

void
MM_ReadBarrierVerifier::healJniWeakReferenceSlots(MM_EnvironmentBase *env)
{
	MM_RootScannerReadBarrierVerifier scanner(env, true);
	scanner.scanJNIWeakGlobalReferences(env);
}

void
MM_ReadBarrierVerifier::healMonitorReferenceSlots(MM_EnvironmentBase *env)
{
	MM_RootScannerReadBarrierVerifier scanner(env, true);
	scanner.scanMonitorReferences(env);
}

void
MM_ReadBarrierVerifier::healStaticClassSlots(MM_EnvironmentBase *env)
{
	MM_RootScannerReadBarrierVerifier scanner(env, true);
	scanner.scanClassStaticSlots(env);
}

void
MM_ReadBarrierVerifier::healConstantPoolObjects(MM_EnvironmentBase *env)
{
	MM_RootScannerReadBarrierVerifier scanner(env, true);
	scanner.scanConstantPoolObjectSlots(env);
}

void
MM_ReadBarrierVerifier::healSlots(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	if (1 == extensions->fvtest_enableJNIGlobalWeakReadBarrierVerification) {
		healJniWeakReferenceSlots(env);
	}
	if (1 == extensions->fvtest_enableMonitorObjectsReadBarrierVerification) {
		healMonitorReferenceSlots(env);
	}
	if (1 == extensions->fvtest_enableClassStaticsReadBarrierVerification) {
		healStaticClassSlots(env);
		healConstantPoolObjects(env);
	}

}

bool
MM_ReadBarrierVerifier::preObjectRead(J9VMThread *vmThread, J9Class *srcClass, j9object_t *srcAddress)
{
	Assert_MM_true(vmThread->javaVM->internalVMFunctions->currentVMThread(vmThread->javaVM) == vmThread);
	healSlot(_extensions, srcAddress);
	return true;
}

bool
MM_ReadBarrierVerifier::preMonitorTableSlotRead(J9VMThread *vmThread, j9object_t *srcAddress)
{
	Assert_MM_true(vmThread->javaVM->internalVMFunctions->currentVMThread(vmThread->javaVM) == vmThread);
	healSlot(_extensions, srcAddress);
	return true;
}

bool
MM_ReadBarrierVerifier::preMonitorTableSlotRead(J9JavaVM *vm, j9object_t *srcAddress)
{
	healSlot(_extensions, srcAddress);
	return true;
}

#endif /* defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS) */

