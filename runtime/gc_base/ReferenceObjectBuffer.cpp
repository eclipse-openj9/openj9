
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

#include "j9.h"
#include "j9cfg.h"
#include "ModronAssertions.h"

#include "ReferenceObjectBuffer.hpp"

#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "HeapRegionDescriptor.hpp"
#include "HeapRegionManager.hpp"
#include "ObjectAccessBarrier.hpp"
#include "ReferenceObjectList.hpp"

MM_ReferenceObjectBuffer::MM_ReferenceObjectBuffer(UDATA maxObjectCount)
	: MM_BaseVirtual()
	, _maxObjectCount(maxObjectCount)
{
	_typeId = __FUNCTION__;
	reset();
}

void
MM_ReferenceObjectBuffer::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

void 
MM_ReferenceObjectBuffer::reset()
{
	_head = NULL;
	_tail = NULL;
	_region = NULL;
	_referenceObjectType = 0;
	/* set object count to appear full so that we force initialization on the next add */
	_objectCount = _maxObjectCount;
}

void 
MM_ReferenceObjectBuffer::flush(MM_EnvironmentBase* env)
{
	if (NULL != _head) {
		flushImpl(env);
		reset();
	}
}

UDATA
MM_ReferenceObjectBuffer::getReferenceObjectType(j9object_t object) 
{ 
	return J9CLASS_FLAGS(J9GC_J9OBJECT_CLAZZ(object)) & J9AccClassReferenceMask;
}

void
MM_ReferenceObjectBuffer::add(MM_EnvironmentBase* env, j9object_t object)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);

	if ( (_objectCount < _maxObjectCount) && _region->isAddressInRegion(object) && (getReferenceObjectType(object) == _referenceObjectType)) {
		/* object is permitted in this buffer */
		Assert_MM_true(NULL != _head);
		Assert_MM_true(NULL != _tail);

		extensions->accessBarrier->setReferenceLink(object, _head);
		_head = object;
		_objectCount += 1;
	} else {
		MM_HeapRegionDescriptor *region = _region;

		/* flush the buffer and start fresh */
		flush(env);
		
		extensions->accessBarrier->setReferenceLink(object, NULL);
		_head = object;
		_tail = object;
		_objectCount = 1;
		if (NULL == region || !region->isAddressInRegion(object)) {
			/* record the type of object and the region which contains this object. Other objects will be permitted in the buffer if they match */
			MM_HeapRegionManager *regionManager = extensions->getHeap()->getHeapRegionManager();
			region = regionManager->regionDescriptorForAddress(object);
			Assert_MM_true(NULL != region);
		}
		_region = region;
		_referenceObjectType = getReferenceObjectType(object);

	}
}

