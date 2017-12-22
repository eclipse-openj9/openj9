
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
#include "ReferenceObjectBufferStandard.hpp"
#include "ReferenceObjectList.hpp"

MM_ReferenceObjectBufferStandard::MM_ReferenceObjectBufferStandard(UDATA maxObjectCount)
	: MM_ReferenceObjectBuffer(maxObjectCount)
	,_referenceObjectListIndex(0)
{
	_typeId = __FUNCTION__;
}

MM_ReferenceObjectBufferStandard *
MM_ReferenceObjectBufferStandard::newInstance(MM_EnvironmentBase *env)
{
	MM_ReferenceObjectBufferStandard *referenceObjectBuffer = NULL;

	referenceObjectBuffer = (MM_ReferenceObjectBufferStandard *)env->getForge()->allocate(sizeof(MM_ReferenceObjectBufferStandard), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != referenceObjectBuffer) {
		new(referenceObjectBuffer) MM_ReferenceObjectBufferStandard(MM_GCExtensions::getExtensions(env)->objectListFragmentCount);
		if (!referenceObjectBuffer->initialize(env)) {
			referenceObjectBuffer->kill(env);
			referenceObjectBuffer = NULL;
		}
	}

	return referenceObjectBuffer;
}

bool
MM_ReferenceObjectBufferStandard::initialize(MM_EnvironmentBase *base)
{
	return true;
}

void
MM_ReferenceObjectBufferStandard::tearDown(MM_EnvironmentBase *base)
{

}

void 
MM_ReferenceObjectBufferStandard::flushImpl(MM_EnvironmentBase* env)
{
	MM_HeapRegionDescriptorStandard *region = (MM_HeapRegionDescriptorStandard*)_region;
	MM_HeapRegionDescriptorStandardExtension *regionExtension = MM_ConfigurationDelegate::getHeapRegionDescriptorStandardExtension(env, region);
	MM_ReferenceObjectList *list = &regionExtension->_referenceObjectLists[_referenceObjectListIndex];
	list->addAll(env, _referenceObjectType, _head, _tail);
	_referenceObjectListIndex += 1;
	if (regionExtension->_maxListIndex == _referenceObjectListIndex) {
		_referenceObjectListIndex = 0;
	}
}
