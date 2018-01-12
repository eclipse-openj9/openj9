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
#include "CheckOwnableSynchronizerList.hpp"
#include "ModronTypes.hpp"
#include "ObjectAccessBarrier.hpp"
#include "ScanFormatter.hpp"
#include "OwnableSynchronizerObjectList.hpp"
#include "HeapRegionManager.hpp"

GC_Check *
GC_CheckOwnableSynchronizerList::newInstance(J9JavaVM *javaVM, GC_CheckEngine *engine)
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(javaVM)->getForge();
	
	GC_CheckOwnableSynchronizerList *check = (GC_CheckOwnableSynchronizerList *) forge->allocate(sizeof(GC_CheckOwnableSynchronizerList), MM_AllocationCategory::DIAGNOSTIC, J9_GET_CALLSITE());
	if(NULL != check) {
		new(check) GC_CheckOwnableSynchronizerList(javaVM, engine);
	}
	return check;
}

void
GC_CheckOwnableSynchronizerList::kill()
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(_javaVM)->getForge();
	forge->free(this);
}

void
GC_CheckOwnableSynchronizerList::check()
{
	MM_ObjectAccessBarrier *barrier = _extensions->accessBarrier;
	MM_OwnableSynchronizerObjectList *ownableSynchronizerObjectList = _extensions->ownableSynchronizerObjectLists;

	MM_HeapRegionManager* heapRegionManager = _extensions->heapRegionManager;
	UDATA maximumOwnableSynchronizerCountOnHeap = heapRegionManager->getTotalHeapSizeInBytes()/J9_GC_MINIMUM_OBJECT_SIZE;
	UDATA ownableSynchronizerCount = 0;

	while (NULL != ownableSynchronizerObjectList) {
		J9Object *objectPtr = ownableSynchronizerObjectList->getHeadOfList();
		while (NULL != objectPtr) {
			if (J9MODRON_SLOT_ITERATOR_OK != _engine->checkSlotOwnableSynchronizerList(_javaVM, &objectPtr, ownableSynchronizerObjectList)) {
				return;
			}
			objectPtr = barrier->getOwnableSynchronizerLink(objectPtr);
			ownableSynchronizerCount += 1;
			if (ownableSynchronizerCount > maximumOwnableSynchronizerCountOnHeap) {
				PORT_ACCESS_FROM_PORT(_portLibrary);
				j9tty_printf(PORTLIB, "  <gc check: found that circular reference in the OwnableSynchronizerList=%p, maximum OwnableSynchronizerCount =%zu >\n", ownableSynchronizerObjectList, maximumOwnableSynchronizerCountOnHeap);
				return;
			}
		}
		ownableSynchronizerObjectList = ownableSynchronizerObjectList->getNextList();
	}
	/* call verifyOwnableSynchronizerObjectCounts() only at the end of CheckOwnableSynchronizerList,
	 * verifyOwnableSynchronizerObjectCounts need both the count calculated in CheckObjectHeap and in CheckOwnableSynchronizerList
	 * assumption: CheckOwnableSynchronizerList always happens after CheckObjectHeap in CheckCycle
	 */
	_engine->verifyOwnableSynchronizerObjectCounts();
}

void
GC_CheckOwnableSynchronizerList::print()
{
	MM_ObjectAccessBarrier *barrier = _extensions->accessBarrier;
	MM_OwnableSynchronizerObjectList *ownableSynchronizerObjectList = _extensions->ownableSynchronizerObjectLists;

	GC_ScanFormatter formatter(_portLibrary, "ownableSynchronizerObjectList");
	while(NULL != ownableSynchronizerObjectList) {
		formatter.section("list", (void *) ownableSynchronizerObjectList);
		J9Object *objectPtr = ownableSynchronizerObjectList->getHeadOfList();
		while (NULL != objectPtr) {
			formatter.entry((void *) objectPtr);
			objectPtr = barrier->getOwnableSynchronizerLink(objectPtr);
		}
		formatter.endSection();
		ownableSynchronizerObjectList = ownableSynchronizerObjectList->getNextList();
	}
	formatter.end("ownableSynchronizerObjectList");
}

