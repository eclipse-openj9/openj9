
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

#include "UnfinalizedObjectList.hpp"

#include "AtomicOperations.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "ObjectAccessBarrier.hpp"

MM_UnfinalizedObjectList::MM_UnfinalizedObjectList()
	: MM_BaseNonVirtual()
	, _head(NULL)
	, _priorHead(NULL)
	, _nextList(NULL)
	, _previousList(NULL)
{
	_typeId = __FUNCTION__;
}

MM_UnfinalizedObjectList *
MM_UnfinalizedObjectList::newInstanceArray(MM_EnvironmentBase *env, uintptr_t arrayElementsTotal, MM_UnfinalizedObjectList *listsToCopy, uintptr_t arrayElementsToCopy)
{
	MM_UnfinalizedObjectList *unfinalizedObjectLists = NULL;

	unfinalizedObjectLists = (MM_UnfinalizedObjectList *)env->getForge()->allocate(sizeof(MM_UnfinalizedObjectList) * arrayElementsTotal,  MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (NULL != unfinalizedObjectLists) {
		Assert_MM_true(arrayElementsTotal >= arrayElementsToCopy);
		/* Check whether a new array instance in being created from an existing array. If so, copy over the elements first. */
		if (arrayElementsToCopy > 0) {
			for (uintptr_t index = 0; index < arrayElementsToCopy; index++) {
				unfinalizedObjectLists[index] = listsToCopy[index];
				unfinalizedObjectLists[index].initialize(env);
			}
		}

		for (uintptr_t index = arrayElementsToCopy; index < arrayElementsTotal; index++) {
			new(&unfinalizedObjectLists[index]) MM_UnfinalizedObjectList();
			unfinalizedObjectLists[index].initialize(env);
		}
	}

	return unfinalizedObjectLists;
}

bool
MM_UnfinalizedObjectList::initialize(MM_EnvironmentBase *env)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);

	setNextList(extensions->unfinalizedObjectLists);
	setPreviousList(NULL);
	if (NULL != extensions->unfinalizedObjectLists) {
		extensions->unfinalizedObjectLists->setPreviousList(this);
	}
	extensions->unfinalizedObjectLists = this;

	return true;
}

void 
MM_UnfinalizedObjectList::addAll(MM_EnvironmentBase* env, j9object_t head, j9object_t tail)
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
	extensions->accessBarrier->setFinalizeLink(tail, previousHead);
}
