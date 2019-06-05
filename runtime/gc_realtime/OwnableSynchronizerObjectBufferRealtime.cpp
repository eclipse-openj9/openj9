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

#include "OwnableSynchronizerObjectBufferRealtime.hpp"

#include "OwnableSynchronizerObjectList.hpp"
#include "RealtimeGC.hpp"
#include "MetronomeDelegate.hpp"

MM_OwnableSynchronizerObjectBufferRealtime::MM_OwnableSynchronizerObjectBufferRealtime(MM_GCExtensions *extensions, UDATA maxObjectCount)
	: MM_OwnableSynchronizerObjectBuffer(extensions, maxObjectCount)
	,_ownableSynchronizerObjectListIndex(0)
{
	_typeId = __FUNCTION__;
}

MM_OwnableSynchronizerObjectBufferRealtime *
MM_OwnableSynchronizerObjectBufferRealtime::newInstance(MM_EnvironmentBase *env)
{
	MM_OwnableSynchronizerObjectBufferRealtime *ownableObjectBuffer = NULL;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);

	ownableObjectBuffer = (MM_OwnableSynchronizerObjectBufferRealtime *)env->getForge()->allocate(sizeof(MM_OwnableSynchronizerObjectBufferRealtime), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != ownableObjectBuffer) {
		new(ownableObjectBuffer) MM_OwnableSynchronizerObjectBufferRealtime(extensions, extensions->objectListFragmentCount);
		if (!ownableObjectBuffer->initialize(env)) {
			ownableObjectBuffer->kill(env);
			ownableObjectBuffer = NULL;
		}
	}

	return ownableObjectBuffer;
}

bool
MM_OwnableSynchronizerObjectBufferRealtime::initialize(MM_EnvironmentBase *base)
{
	return true;
}

void
MM_OwnableSynchronizerObjectBufferRealtime::tearDown(MM_EnvironmentBase *base)
{

}

void
MM_OwnableSynchronizerObjectBufferRealtime::flushImpl(MM_EnvironmentBase* env)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	MM_OwnableSynchronizerObjectList *ownableSynchronizerObjectList = &extensions->ownableSynchronizerObjectLists[_ownableSynchronizerObjectListIndex];
	ownableSynchronizerObjectList->addAll(env, _head, _tail);
	_ownableSynchronizerObjectListIndex += 1;
	if (extensions->realtimeGC->getRealtimeDelegate()->getOwnableSynchronizerObjectListCount(env) == _ownableSynchronizerObjectListIndex) {
		_ownableSynchronizerObjectListIndex = 0;
	}
}
