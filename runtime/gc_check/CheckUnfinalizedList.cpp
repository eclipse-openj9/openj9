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

#include "CheckEngine.hpp"
#include "CheckUnfinalizedList.hpp"
#include "ModronTypes.hpp"
#include "ObjectAccessBarrier.hpp"
#include "ScanFormatter.hpp"
#include "UnfinalizedObjectList.hpp"

#if defined(J9VM_GC_FINALIZATION)

GC_Check *
GC_CheckUnfinalizedList::newInstance(J9JavaVM *javaVM, GC_CheckEngine *engine)
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(javaVM)->getForge();
	
	GC_CheckUnfinalizedList *check = (GC_CheckUnfinalizedList *) forge->allocate(sizeof(GC_CheckUnfinalizedList), MM_AllocationCategory::DIAGNOSTIC, J9_GET_CALLSITE());
	if(check) {
		new(check) GC_CheckUnfinalizedList(javaVM, engine);
	}
	return check;
}

void
GC_CheckUnfinalizedList::kill()
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(_javaVM)->getForge();
	forge->free(this);
}

void
GC_CheckUnfinalizedList::check()
{
	MM_ObjectAccessBarrier *barrier = _extensions->accessBarrier;
	MM_UnfinalizedObjectList *unfinalizedObjectList = _extensions->unfinalizedObjectLists;

	while(NULL != unfinalizedObjectList) {
		J9Object *objectPtr = unfinalizedObjectList->getHeadOfList();
		while (NULL != objectPtr) {
			if (_engine->checkSlotUnfinalizedList(_javaVM, &objectPtr, unfinalizedObjectList) != J9MODRON_SLOT_ITERATOR_OK ){
				return;
			}
			objectPtr = barrier->getFinalizeLink(objectPtr);
		}
		unfinalizedObjectList = unfinalizedObjectList->getNextList();
	}
}

void
GC_CheckUnfinalizedList::print()
{
	MM_ObjectAccessBarrier *barrier = _extensions->accessBarrier;
	MM_UnfinalizedObjectList *unfinalizedObjectList = _extensions->unfinalizedObjectLists;

	GC_ScanFormatter formatter(_portLibrary, "unfinalizedObjectList");
	while(NULL != unfinalizedObjectList) {
		formatter.section("list", (void *) unfinalizedObjectList);
		J9Object *objectPtr = unfinalizedObjectList->getHeadOfList();
		while (NULL != objectPtr) {
			formatter.entry((void *) objectPtr);
			objectPtr = barrier->getFinalizeLink(objectPtr);
		}
		formatter.endSection();
		unfinalizedObjectList = unfinalizedObjectList->getNextList();
	}
	formatter.end("unfinalizedObjectList");
}

#endif /* J9VM_GC_FINALIZATION */
