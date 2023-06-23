
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

#include "j9.h"
#include "j9cfg.h"
#include "ModronAssertions.h"

#include "ConfigurationDelegate.hpp"
#include "EnvironmentBase.hpp"
#include "HeapRegionDescriptorStandard.hpp"
#include "OwnableSynchronizerObjectBufferStandard.hpp"
#include "OwnableSynchronizerObjectList.hpp"

MM_OwnableSynchronizerObjectBufferStandard::MM_OwnableSynchronizerObjectBufferStandard(MM_GCExtensions *extensions, uintptr_t maxObjectCount)
	: MM_OwnableSynchronizerObjectBuffer(extensions, maxObjectCount)
	,_ownableSynchronizerObjectListIndex(0)
{
	_typeId = __FUNCTION__;
}

MM_OwnableSynchronizerObjectBufferStandard *
MM_OwnableSynchronizerObjectBufferStandard::newInstance(MM_EnvironmentBase *env)
{
	MM_OwnableSynchronizerObjectBufferStandard *ownableObjectBuffer = NULL;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);

	ownableObjectBuffer = (MM_OwnableSynchronizerObjectBufferStandard *)env->getForge()->allocate(sizeof(MM_OwnableSynchronizerObjectBufferStandard), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != ownableObjectBuffer) {
		new(ownableObjectBuffer) MM_OwnableSynchronizerObjectBufferStandard(extensions, extensions->objectListFragmentCount);
		if (!ownableObjectBuffer->initialize(env)) {
			ownableObjectBuffer->kill(env);
			ownableObjectBuffer = NULL;
		}
	}

	return ownableObjectBuffer;
}

bool
MM_OwnableSynchronizerObjectBufferStandard::initialize(MM_EnvironmentBase *base)
{
	return true;
}

void
MM_OwnableSynchronizerObjectBufferStandard::tearDown(MM_EnvironmentBase *base)
{

}

void 
MM_OwnableSynchronizerObjectBufferStandard::flushImpl(MM_EnvironmentBase *env)
{
	MM_HeapRegionDescriptorStandard *region = (MM_HeapRegionDescriptorStandard*)_region;
	MM_HeapRegionDescriptorStandardExtension *regionExtension = MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
	MM_OwnableSynchronizerObjectList *list = &regionExtension->_ownableSynchronizerObjectLists[_ownableSynchronizerObjectListIndex];

	list->addAll(env, _head, _tail);
	_ownableSynchronizerObjectListIndex += 1;
	if (_ownableSynchronizerObjectListIndex >= regionExtension->_maxListIndex) {
		_ownableSynchronizerObjectListIndex = 0;
	}
}

#if defined(J9VM_OPT_CRIU_SUPPORT)
bool
MM_OwnableSynchronizerObjectBufferStandard::reinitializeForRestore(MM_EnvironmentBase *env)
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
