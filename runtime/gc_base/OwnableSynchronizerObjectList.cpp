
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

#include "OwnableSynchronizerObjectList.hpp"

#include "AtomicOperations.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "ObjectAccessBarrier.hpp"

MM_OwnableSynchronizerObjectList::MM_OwnableSynchronizerObjectList()
	: MM_BaseNonVirtual()
	, _head(NULL)
	, _priorHead(NULL)
	, _nextList(NULL)
	, _previousList(NULL)
#if defined(J9VM_GC_VLHGC)
	, _objectCount(0)
#endif /* defined(J9VM_GC_VLHGC) */
{
	_typeId = __FUNCTION__;
}

MM_OwnableSynchronizerObjectList *
MM_OwnableSynchronizerObjectList::newInstanceArray(MM_EnvironmentBase *env, uintptr_t arrayElementsTotal, MM_OwnableSynchronizerObjectList *listsToCopy, uintptr_t arrayElementsToCopy)
{
	MM_OwnableSynchronizerObjectList *ownableSynchronizerObjectLists = NULL;

	ownableSynchronizerObjectLists = (MM_OwnableSynchronizerObjectList *)env->getForge()->allocate(sizeof(MM_OwnableSynchronizerObjectList) * arrayElementsTotal,  MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != ownableSynchronizerObjectLists) {
		Assert_MM_true(arrayElementsTotal >= arrayElementsToCopy);
		/* Check whether a new array instance in being created from an existing array. If so, copy over the elements first. */
		if (arrayElementsToCopy > 0) {
			for (uintptr_t index = 0; index < arrayElementsToCopy; index++) {
				ownableSynchronizerObjectLists[index] = listsToCopy[index];
				ownableSynchronizerObjectLists[index].initialize(env);
			}
		}

		for (uintptr_t index = arrayElementsToCopy; index < arrayElementsTotal; index++) {
			new(&ownableSynchronizerObjectLists[index]) MM_OwnableSynchronizerObjectList();
			ownableSynchronizerObjectLists[index].initialize(env);
		}
	}

	return ownableSynchronizerObjectLists;
}

bool
MM_OwnableSynchronizerObjectList::initialize(MM_EnvironmentBase *env)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);

	setNextList(extensions->getOwnableSynchronizerObjectLists());
	setPreviousList(NULL);
	if (NULL != extensions->getOwnableSynchronizerObjectLists()) {
		extensions->getOwnableSynchronizerObjectLists()->setPreviousList(this);
	}
	extensions->setOwnableSynchronizerObjectLists(this);

	return true;
}

void 
MM_OwnableSynchronizerObjectList::addAll(MM_EnvironmentBase* env, j9object_t head, j9object_t tail)
{
	Assert_MM_true(NULL != head);
	Assert_MM_true(NULL != tail);

	j9object_t previousHead = _head;
	while (previousHead != (j9object_t)MM_AtomicOperations::lockCompareExchange((volatile UDATA*)&_head, (UDATA)previousHead, (UDATA)head)) {
		previousHead = _head;
	}

	/* detect trivial cases which can inject cycles into the linked list */
	Assert_MM_true( (head != previousHead) && (tail != previousHead) );
	
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	extensions->accessBarrier->setOwnableSynchronizerLink(tail, previousHead);
}
