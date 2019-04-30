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

#include "ReadBarrierVerifier.hpp"

#include "ObjectAccessBarrier.hpp"
#include "RootScannerReadBarrierVerifier.hpp"

#if defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS)

void
MM_RootScannerReadBarrierVerifier::doMonitorReference(J9ObjectMonitor *objectMonitor, GC_HashTableIterator *monitorReferenceIterator)
{
	J9ThreadAbstractMonitor * monitor = (J9ThreadAbstractMonitor*)objectMonitor->monitor;
	if (_poison) {
		((MM_ReadBarrierVerifier*)_extensions->accessBarrier)->poisonSlot(_env->getExtensions(), (omrobjectptr_t *)&monitor->userData);
	} else {
		((MM_ReadBarrierVerifier*)_extensions->accessBarrier)->healSlot(_env->getExtensions(), (omrobjectptr_t *)&monitor->userData);
	}
}

void
MM_RootScannerReadBarrierVerifier::doJNIWeakGlobalReference(omrobjectptr_t *slotPtr)
{
	if (_poison) {
		((MM_ReadBarrierVerifier*)_extensions->accessBarrier)->poisonSlot(_env->getExtensions(), slotPtr);
	} else {
		((MM_ReadBarrierVerifier*)_extensions->accessBarrier)->healSlot(_env->getExtensions(), slotPtr);
	}
}

void
MM_RootScannerReadBarrierVerifier::scanConstantPoolObjectSlots(MM_EnvironmentBase *env)
{
	OMR_VMThread *omrVMThread = env->getOmrVMThread();
	GC_SegmentIterator segmentIterator(static_cast<J9JavaVM*>(omrVMThread->_vm->_language_vm)->classMemorySegments, MEMORY_TYPE_RAM_CLASS);

	while (J9MemorySegment *segment = segmentIterator.nextSegment()) {
		GC_ClassHeapIterator classHeapIterator(static_cast<J9JavaVM*>(omrVMThread->_vm->_language_vm), segment);
		J9Class *clazz = NULL;

		while (NULL != (clazz = classHeapIterator.nextClass())) {

			volatile omrobjectptr_t *slotPtr = NULL;
			GC_ConstantPoolObjectSlotIterator constantPoolObjectSlotIterator((J9JavaVM*)omrVMThread->_vm->_language_vm, clazz, true);
			while (NULL != (slotPtr = (omrobjectptr_t*)constantPoolObjectSlotIterator.nextSlot())) {
				if (NULL != *slotPtr) {
					doConstantPoolObjectSlots((omrobjectptr_t *)slotPtr);
				}
			}
		}
	}
}

void
MM_RootScannerReadBarrierVerifier::doConstantPoolObjectSlots(omrobjectptr_t *slotPtr)
{
	if (_poison) {
		((MM_ReadBarrierVerifier*)_extensions->accessBarrier)->poisonSlot(_env->getExtensions(), slotPtr);
	} else {
		((MM_ReadBarrierVerifier*)_extensions->accessBarrier)->healSlot(_env->getExtensions(), slotPtr);
	}
}

void
MM_RootScannerReadBarrierVerifier::scanClassStaticSlots(MM_EnvironmentBase *env)
{
	OMR_VMThread *omrVMThread = env->getOmrVMThread();
	GC_SegmentIterator segmentIterator(static_cast<J9JavaVM*>(omrVMThread->_vm->_language_vm)->classMemorySegments, MEMORY_TYPE_RAM_CLASS);

	while (J9MemorySegment *segment = segmentIterator.nextSegment()) {
		GC_ClassHeapIterator classHeapIterator(static_cast<J9JavaVM*>(omrVMThread->_vm->_language_vm), segment);
		J9Class *clazz = NULL;

		while (NULL != (clazz = classHeapIterator.nextClass())) {

			volatile omrobjectptr_t *slotPtr = NULL;
			GC_ClassStaticsIterator classStaticsIterator(env, clazz);
			while (NULL != (slotPtr = (omrobjectptr_t*)classStaticsIterator.nextSlot())) {
				doClassStaticSlots((omrobjectptr_t *)slotPtr);
			}
		}
	}
}

void
MM_RootScannerReadBarrierVerifier::doClassStaticSlots(omrobjectptr_t *slotPtr)
{
	if (_poison) {
		((MM_ReadBarrierVerifier*)_extensions->accessBarrier)->poisonSlot(_env->getExtensions(), slotPtr);
	} else {
		((MM_ReadBarrierVerifier*)_extensions->accessBarrier)->healSlot(_env->getExtensions(), slotPtr);
	}
}

#endif /* defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS) */
