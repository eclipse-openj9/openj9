
/*******************************************************************************
 * Copyright IBM Corp. and others 2022
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

#include "HeapWalkerDelegate.hpp"

#include "j9.h"
#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "ObjectModel.hpp"
#include "VMHelpers.hpp"
#include "VMThreadStackSlotIterator.hpp"
#include "HeapWalker.hpp"


void
MM_HeapWalkerDelegate::objectSlotsDo(OMR_VMThread *omrVMThread, omrobjectptr_t objectPtr, MM_HeapWalkerSlotFunc function, void *userData)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrVMThread);
	switch(_objectModel->getScanType(objectPtr)) {
	case GC_ObjectModel::SCAN_CONTINUATION_OBJECT:
		doContinuationNativeSlots(env, objectPtr, function, userData);
		break;
	default:
		break;
	}
}

/**
 * @todo Provide function documentation
 */
void
stackSlotIteratorForHeapWalker(J9JavaVM *javaVM, J9Object **slotPtr, void *localData, J9StackWalkState *walkState, const void *stackLocation)
{
	StackIteratorData4HeapWalker *data = (StackIteratorData4HeapWalker *)localData;
	data->heapWalker->heapWalkerSlotCallback(data->env, slotPtr, data->function, data->userData);
}

void
MM_HeapWalkerDelegate::doContinuationNativeSlots(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, MM_HeapWalkerSlotFunc function, void *userData)
{
	J9VMThread *currentThread = (J9VMThread *)env->getLanguageVMThread();

	const bool isConcurrentGC = false;
	const bool isGlobalGC = true;
	const bool beingMounted = false;
	if (MM_GCExtensions::needScanStacksForContinuationObject(currentThread, objectPtr, isConcurrentGC, isGlobalGC, beingMounted)) {
		StackIteratorData4HeapWalker localData;
		localData.heapWalker = _heapWalker;
		localData.env = env;
		localData.fromObject = objectPtr;
		localData.function = function;
		localData.userData = userData;

		GC_VMThreadStackSlotIterator::scanContinuationSlots(currentThread, objectPtr, (void *)&localData, stackSlotIteratorForHeapWalker, false, false);
	}
}
