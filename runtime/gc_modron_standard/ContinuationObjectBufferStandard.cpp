
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

#include "j9.h"
#include "j9cfg.h"
#include "ModronAssertions.h"

#include "ConfigurationDelegate.hpp"
#include "EnvironmentBase.hpp"
#include "HeapRegionDescriptorStandard.hpp"
#include "HeapRegionIteratorStandard.hpp"

#include "ContinuationObjectBufferStandard.hpp"
#include "ContinuationObjectList.hpp"
#include "ParallelTask.hpp"
#if JAVA_SPEC_VERSION >= 19
#include "ContinuationHelpers.hpp"
#endif /* JAVA_SPEC_VERSION >= 19 */

MM_ContinuationObjectBufferStandard::MM_ContinuationObjectBufferStandard(MM_GCExtensions *extensions, uintptr_t maxObjectCount)
	: MM_ContinuationObjectBuffer(extensions, maxObjectCount)
	,_continuationObjectListIndex(0)
{
	_typeId = __FUNCTION__;
}

MM_ContinuationObjectBufferStandard *
MM_ContinuationObjectBufferStandard::newInstance(MM_EnvironmentBase *env)
{
	MM_ContinuationObjectBufferStandard *continuationObjectBuffer = NULL;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);

	continuationObjectBuffer = (MM_ContinuationObjectBufferStandard *)env->getForge()->allocate(sizeof(MM_ContinuationObjectBufferStandard), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != continuationObjectBuffer) {
		new(continuationObjectBuffer) MM_ContinuationObjectBufferStandard(extensions, extensions->objectListFragmentCount);
		if (!continuationObjectBuffer->initialize(env)) {
			continuationObjectBuffer->kill(env);
			continuationObjectBuffer = NULL;
		}
	}

	return continuationObjectBuffer;
}

bool
MM_ContinuationObjectBufferStandard::initialize(MM_EnvironmentBase *base)
{
	return true;
}

void
MM_ContinuationObjectBufferStandard::tearDown(MM_EnvironmentBase *base)
{

}

void
MM_ContinuationObjectBufferStandard::flushImpl(MM_EnvironmentBase *env)
{
	MM_HeapRegionDescriptorStandard *region = (MM_HeapRegionDescriptorStandard*)_region;
	MM_HeapRegionDescriptorStandardExtension *regionExtension = MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
	MM_ContinuationObjectList *list = &regionExtension->_continuationObjectLists[_continuationObjectListIndex];

	list->addAll(env, _head, _tail);
	_continuationObjectListIndex += 1;
	if (_continuationObjectListIndex >= regionExtension->_maxListIndex) {
		_continuationObjectListIndex = 0;
	}
}

#if defined(J9VM_OPT_CRIU_SUPPORT)
bool
MM_ContinuationObjectBufferStandard::reinitializeForRestore(MM_EnvironmentBase *env)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);

	Assert_MM_true(_maxObjectCount > 0);
	Assert_MM_true(extensions->objectListFragmentCount > 0);

	_maxObjectCount = extensions->objectListFragmentCount;

	flush(env);

	/* This reset is necessary to ensure the object counter is reset based on the the new _maxObjectCount value.
	 * Specifically to handle the case when the buffer was previously emptied and reset based on previous _maxObjectCount. */
	reset();

	return true;
}
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

void
MM_ContinuationObjectBufferStandard::iterateAllContinuationObjects(MM_EnvironmentBase *env)
{
#if JAVA_SPEC_VERSION >= 19
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	MM_HeapRegionDescriptorStandard *region = NULL;
	GC_HeapRegionIteratorStandard regionIterator(extensions->heapRegionManager);
	GC_Environment *gcEnv = env->getGCEnvironment();

	/* to make sure that previous pruning phase of continuation list(scanContinuationObjects()) is complete */
	env->_currentTask->synchronizeGCThreads(env, UNIQUE_ID);

	while (NULL != (region = regionIterator.nextRegion())) {
		MM_HeapRegionDescriptorStandardExtension *regionExtension = MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
		for (uintptr_t i = 0; i < regionExtension->_maxListIndex; i++) {
			MM_ContinuationObjectList *list = &regionExtension->_continuationObjectLists[i];
			if (NULL != list->getHeadOfList()) {
				if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {

					omrobjectptr_t object = list->getHeadOfList();
					while (NULL != object) {
						gcEnv->_continuationStats._total += 1;
						omrobjectptr_t next = extensions->accessBarrier->getContinuationLink(object);
						ContinuationState volatile *continuationStatePtr = VM_ContinuationHelpers::getContinuationStateAddress((J9VMThread *)env->getLanguageVMThread(), object);
						if (VM_ContinuationHelpers::isActive(*continuationStatePtr)) {
							gcEnv->_continuationStats._started += 1;
							TRIGGER_J9HOOK_MM_WALKCONTINUATION(extensions->hookInterface, (J9VMThread *)env->getLanguageVMThread(), object);
						}
						object = next;
					}
				}
			}
		}
	}
#endif /* JAVA_SPEC_VERSION >= 19 */
}
