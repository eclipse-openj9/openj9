
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

#include "OwnableSynchronizerObjectBufferVLHGC.hpp"
#include "HeapRegionManager.hpp"

#include "HeapRegionDescriptorVLHGC.hpp"
#include "OwnableSynchronizerObjectList.hpp"

MM_OwnableSynchronizerObjectBufferVLHGC::MM_OwnableSynchronizerObjectBufferVLHGC(MM_GCExtensions *extensions, UDATA maxObjectCount)
	: MM_OwnableSynchronizerObjectBuffer(extensions, maxObjectCount)
{
	_typeId = __FUNCTION__;
}

MM_OwnableSynchronizerObjectBufferVLHGC *
MM_OwnableSynchronizerObjectBufferVLHGC::newInstance(MM_EnvironmentBase *env)
{
	MM_OwnableSynchronizerObjectBufferVLHGC *ownableObjectBuffer = NULL;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);

	ownableObjectBuffer = (MM_OwnableSynchronizerObjectBufferVLHGC *)env->getForge()->allocate(sizeof(MM_OwnableSynchronizerObjectBufferVLHGC), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != ownableObjectBuffer) {
		new(ownableObjectBuffer) MM_OwnableSynchronizerObjectBufferVLHGC(extensions, UDATA_MAX);
		if (!ownableObjectBuffer->initialize(env)) {
			ownableObjectBuffer->kill(env);
			ownableObjectBuffer = NULL;
		}
	}

	return ownableObjectBuffer;
}

bool
MM_OwnableSynchronizerObjectBufferVLHGC::initialize(MM_EnvironmentBase *base)
{
	return true;
}

void
MM_OwnableSynchronizerObjectBufferVLHGC::tearDown(MM_EnvironmentBase *base)
{

}

void 
MM_OwnableSynchronizerObjectBufferVLHGC::flushImpl(MM_EnvironmentBase* env)
{
	MM_HeapRegionDescriptorVLHGC *region = (MM_HeapRegionDescriptorVLHGC*)_region;	
	/* both addAll and incrementObjectCount are atomic, but getObjectCount could get inconsistent count, for current use case, it would be fine. */
	region->getOwnableSynchronizerObjectList()->addAll(env, _head, _tail);
	region->getOwnableSynchronizerObjectList()->incrementObjectCount(_objectCount);
}

void
MM_OwnableSynchronizerObjectBufferVLHGC::addForOnlyCompactedRegion(MM_EnvironmentBase* env, j9object_t object)
{
	Assert_MM_true(object != _head);
	Assert_MM_true(object != _tail);

	if ( (_objectCount < _maxObjectCount) && _region->isAddressInRegion(object) ) {
		/* object is permitted in this buffer */
		Assert_MM_true(NULL != _head);
		Assert_MM_true(NULL != _tail);

		_extensions->accessBarrier->setOwnableSynchronizerLink(object, _head);
		_head = object;
		_objectCount += 1;
	} else {
		MM_HeapRegionManager *regionManager = _extensions->getHeap()->getHeapRegionManager();
		MM_HeapRegionDescriptor *region = regionManager->regionDescriptorForAddress(object);
		Assert_GC_true_with_message(env, NULL != region, "Attempt to access ownable synchronizer object located outside of heap (stack allocated?) %p\n", object);

		if (((MM_HeapRegionDescriptorVLHGC *)region)->_compactData._shouldCompact) {
			/* flush the buffer and start fresh */
			flush(env);
			_extensions->accessBarrier->setOwnableSynchronizerLink(object, NULL);
			_head = object;
			_tail = object;
			_objectCount = 1;
			/* record the region which contains this object. Other objects will be permitted in the buffer if they are in the same region */
			_region = region;
		}
	}
}
