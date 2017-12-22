
/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#include "j9.h"
#include "j9cfg.h"
#include "ModronAssertions.h"

#include "ConfigurationDelegate.hpp"
#include "EnvironmentBase.hpp"
#include "HeapRegionDescriptorStandard.hpp"
#include "UnfinalizedObjectBufferStandard.hpp"
#include "UnfinalizedObjectList.hpp"

MM_UnfinalizedObjectBufferStandard::MM_UnfinalizedObjectBufferStandard(MM_GCExtensions *extensions, UDATA maxObjectCount)
	: MM_UnfinalizedObjectBuffer(extensions, maxObjectCount)
	,_unfinalizedObjectListIndex(0)
{
	_typeId = __FUNCTION__;
}

MM_UnfinalizedObjectBufferStandard *
MM_UnfinalizedObjectBufferStandard::newInstance(MM_EnvironmentBase *env)
{
	MM_UnfinalizedObjectBufferStandard *unfinalizedObjectBuffer = NULL;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);

	unfinalizedObjectBuffer = (MM_UnfinalizedObjectBufferStandard *)env->getForge()->allocate(sizeof(MM_UnfinalizedObjectBufferStandard), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != unfinalizedObjectBuffer) {
		new(unfinalizedObjectBuffer) MM_UnfinalizedObjectBufferStandard(extensions, extensions->objectListFragmentCount);
		if (!unfinalizedObjectBuffer->initialize(env)) {
			unfinalizedObjectBuffer->kill(env);
			unfinalizedObjectBuffer = NULL;
		}
	}

	return unfinalizedObjectBuffer;
}

bool
MM_UnfinalizedObjectBufferStandard::initialize(MM_EnvironmentBase *base)
{
	return true;
}

void
MM_UnfinalizedObjectBufferStandard::tearDown(MM_EnvironmentBase *base)
{

}

void 
MM_UnfinalizedObjectBufferStandard::flushImpl(MM_EnvironmentBase* env)
{
	MM_HeapRegionDescriptorStandard *region = (MM_HeapRegionDescriptorStandard*)_region;
	MM_HeapRegionDescriptorStandardExtension *regionExtension = MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
	MM_UnfinalizedObjectList *list = &regionExtension->_unfinalizedObjectLists[_unfinalizedObjectListIndex];
	list->addAll(env, _head, _tail);
	_unfinalizedObjectListIndex += 1;
	if (regionExtension->_maxListIndex == _unfinalizedObjectListIndex) {
		_unfinalizedObjectListIndex = 0;
	}
}
