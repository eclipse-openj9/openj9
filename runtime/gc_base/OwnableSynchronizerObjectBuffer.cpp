
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

#include "OwnableSynchronizerObjectBuffer.hpp"

#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "HeapRegionDescriptor.hpp"
#include "HeapRegionManager.hpp"
#include "ObjectAccessBarrier.hpp"

MM_OwnableSynchronizerObjectBuffer::MM_OwnableSynchronizerObjectBuffer(MM_GCExtensions *extensions, UDATA maxObjectCount)
	: MM_BaseVirtual()
	, _maxObjectCount(maxObjectCount)
	, _extensions(extensions)
{
	_typeId = __FUNCTION__;
	reset();
}

void
MM_OwnableSynchronizerObjectBuffer::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

void 
MM_OwnableSynchronizerObjectBuffer::reset()
{
	_head = NULL;
	_tail = NULL;
	_region = NULL;
	/* set object count to appear full so that we force initialization on the next add */
	_objectCount = _maxObjectCount;
}

void 
MM_OwnableSynchronizerObjectBuffer::flush(MM_EnvironmentBase* env)
{
	if (NULL != _head) {
		/* call the virtual flush implementation function */
		flushImpl(env);
		reset();
	}
}

void
MM_OwnableSynchronizerObjectBuffer::add(MM_EnvironmentBase* env, j9object_t object)
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
		MM_HeapRegionDescriptor *region = _region;

		/* flush the buffer and start fresh */
		flush(env);
		_extensions->accessBarrier->setOwnableSynchronizerLink(object, NULL);
		_head = object;
		_tail = object;
		_objectCount = 1;
		if (NULL == region || !region->isAddressInRegion(object)) {
			/* record the region which contains this object. Other objects will be permitted in the buffer if they are in the same region */
			MM_HeapRegionManager *regionManager = _extensions->getHeap()->getHeapRegionManager();
			region = regionManager->regionDescriptorForAddress(object);

			Assert_GC_true_with_message(env, NULL != region, "Attempt to access ownable synchronizer object located outside of heap (stack allocated?) %p\n", object);
		}
		_region = region;
	}
	Assert_MM_true(_region->isAddressInRegion(object));
}

/* 
 * temporary place holder implementation - just delegate to the old fragment code.
 */
void 
MM_OwnableSynchronizerObjectBuffer::flushImpl(MM_EnvironmentBase* env)
{
	Assert_MM_unreachable();
}
