
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

#include "ReferenceObjectList.hpp"

#include "AtomicOperations.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "ObjectAccessBarrier.hpp"

MM_ReferenceObjectList::MM_ReferenceObjectList()
	: MM_BaseNonVirtual()
	, _weakHead(NULL)
	, _softHead(NULL)
	, _phantomHead(NULL)
	, _priorWeakHead(NULL)
	, _priorSoftHead(NULL)
	, _priorPhantomHead(NULL)
{
	_typeId = __FUNCTION__;
}

void 
MM_ReferenceObjectList::addAll(MM_EnvironmentBase* env, UDATA referenceObjectType, j9object_t head, j9object_t tail)
{
	Assert_MM_true(NULL != head);
	Assert_MM_true(NULL != tail);

	volatile j9object_t *list = NULL;
	switch (referenceObjectType) {
	case J9AccClassReferenceWeak:
		list = &_weakHead;
		break;
	case J9AccClassReferenceSoft:
		list = &_softHead;
		break;
	case J9AccClassReferencePhantom:
		list = &_phantomHead;
		break;
	default:
		Assert_MM_unreachable();
	}
	
	j9object_t previousHead = *list;
	while (previousHead != (j9object_t)MM_AtomicOperations::lockCompareExchange((volatile UDATA*)list, (UDATA)previousHead, (UDATA)head)) {
		previousHead = *list;
	}

	/* detect trivial cases which can inject cycles into the linked list */
	Assert_MM_true( (head != previousHead) && (tail != previousHead) );
	
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	extensions->accessBarrier->setReferenceLink(tail, previousHead);
}
