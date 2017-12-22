
/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#include "ReferenceObjectBufferVLHGC.hpp"

#include "CycleState.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"
#include "ReferenceObjectList.hpp"

MM_ReferenceObjectBufferVLHGC::MM_ReferenceObjectBufferVLHGC(UDATA maxObjectCount)
	: MM_ReferenceObjectBuffer(maxObjectCount)
{
	_typeId = __FUNCTION__;
}

MM_ReferenceObjectBufferVLHGC *
MM_ReferenceObjectBufferVLHGC::newInstance(MM_EnvironmentBase *env)
{
	MM_ReferenceObjectBufferVLHGC *referenceObjectBuffer = NULL;

	referenceObjectBuffer = (MM_ReferenceObjectBufferVLHGC *)env->getForge()->allocate(sizeof(MM_ReferenceObjectBufferVLHGC), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != referenceObjectBuffer) {
		new(referenceObjectBuffer) MM_ReferenceObjectBufferVLHGC(UDATA_MAX);
		if (!referenceObjectBuffer->initialize(env)) {
			referenceObjectBuffer->kill(env);
			referenceObjectBuffer = NULL;
		}
	}

	return referenceObjectBuffer;
}

bool
MM_ReferenceObjectBufferVLHGC::initialize(MM_EnvironmentBase *base)
{
	return true;
}

void
MM_ReferenceObjectBufferVLHGC::tearDown(MM_EnvironmentBase *base)
{

}

void 
MM_ReferenceObjectBufferVLHGC::flushImpl(MM_EnvironmentBase* env)
{
	MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC*)_region;

	/* ensure that the region is part of the collection set */
	if (MM_CycleState::CT_PARTIAL_GARBAGE_COLLECTION == env->_cycleState->_collectionType) {
		if (env->_cycleState->_shouldRunCopyForward) {
			Assert_MM_true(region->_markData._shouldMark || region->isSurvivorRegion());
		} else {
			Assert_MM_true(region->_markData._shouldMark);
		}
	}

	region->getReferenceObjectList()->addAll(env, _referenceObjectType, _head, _tail);
}
