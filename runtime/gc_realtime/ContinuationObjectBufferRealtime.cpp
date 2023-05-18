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

#include "ContinuationObjectBufferRealtime.hpp"

#include "ContinuationObjectList.hpp"
#include "RealtimeGC.hpp"
#include "MetronomeDelegate.hpp"

MM_ContinuationObjectBufferRealtime::MM_ContinuationObjectBufferRealtime(MM_GCExtensions *extensions, uintptr_t maxObjectCount)
	: MM_ContinuationObjectBuffer(extensions, maxObjectCount)
	,_continuationObjectListIndex(0)
{
	_typeId = __FUNCTION__;
}

MM_ContinuationObjectBufferRealtime *
MM_ContinuationObjectBufferRealtime::newInstance(MM_EnvironmentBase *env)
{
	MM_ContinuationObjectBufferRealtime *continuationObjectBuffer = NULL;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);

	continuationObjectBuffer = (MM_ContinuationObjectBufferRealtime *)env->getForge()->allocate(sizeof(MM_ContinuationObjectBufferRealtime), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != continuationObjectBuffer) {
		new(continuationObjectBuffer) MM_ContinuationObjectBufferRealtime(extensions, extensions->objectListFragmentCount);
		if (!continuationObjectBuffer->initialize(env)) {
			continuationObjectBuffer->kill(env);
			continuationObjectBuffer = NULL;
		}
	}

	return continuationObjectBuffer;
}

bool
MM_ContinuationObjectBufferRealtime::initialize(MM_EnvironmentBase *base)
{
	return true;
}

void
MM_ContinuationObjectBufferRealtime::tearDown(MM_EnvironmentBase *base)
{

}

void
MM_ContinuationObjectBufferRealtime::flushImpl(MM_EnvironmentBase* env)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	MM_ContinuationObjectList *continuationObjectList = &extensions->getContinuationObjectLists()[_continuationObjectListIndex];
	continuationObjectList->addAll(env, _head, _tail);
	_continuationObjectListIndex += 1;
	if (extensions->realtimeGC->getRealtimeDelegate()->getContinuationObjectListCount(env) == _continuationObjectListIndex) {
		_continuationObjectListIndex = 0;
	}
}
