
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
#include "ContinuationObjectBufferStandard.hpp"
#include "ContinuationObjectList.hpp"

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
	if (regionExtension->_maxListIndex == _continuationObjectListIndex) {
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

	return true;
}
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
