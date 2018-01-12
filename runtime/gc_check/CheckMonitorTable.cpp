
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

#include "CheckEngine.hpp"
#include "CheckMonitorTable.hpp"
#include "ModronTypes.hpp"
#include "ScanFormatter.hpp"

GC_Check *
GC_CheckMonitorTable::newInstance(J9JavaVM *javaVM, GC_CheckEngine *engine)
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(javaVM)->getForge();
	
	GC_CheckMonitorTable *check = (GC_CheckMonitorTable *) forge->allocate(sizeof(GC_CheckMonitorTable), MM_AllocationCategory::DIAGNOSTIC, J9_GET_CALLSITE());
	if(check) {
		new(check) GC_CheckMonitorTable(javaVM, engine);
	}
	return check;
}

void
GC_CheckMonitorTable::kill()
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(_javaVM)->getForge();
	forge->free(this);
}

void
GC_CheckMonitorTable::check()
{
	J9ObjectMonitor *objectMonitor = NULL;
	J9MonitorTableListEntry *monitorTableList = _javaVM->monitorTableList;
	while (NULL != monitorTableList) {
		J9HashTable *table = monitorTableList->monitorTable;
		if (NULL != table) {
			GC_HashTableIterator iterator(table);
			while (NULL != (objectMonitor = (J9ObjectMonitor*)iterator.nextSlot())) {
				J9ThreadAbstractMonitor * monitor = (J9ThreadAbstractMonitor *)objectMonitor->monitor;
				J9Object **slotPtr = (J9Object**)&monitor->userData;
				if(_engine->checkSlotPool(_javaVM, slotPtr, table) != J9MODRON_SLOT_ITERATOR_OK ) {
					return;
				}
			}
		}
		monitorTableList = monitorTableList->next;
	}
}

void
GC_CheckMonitorTable::print()
{
	J9ObjectMonitor *objectMonitor = NULL;
	J9MonitorTableListEntry *monitorTableList = _javaVM->monitorTableList;

	GC_ScanFormatter formatter(_portLibrary, "MonitorTableList", (void *)monitorTableList);


	while (NULL != monitorTableList) {
		J9HashTable *table = monitorTableList->monitorTable;
		if (NULL != table) {
			formatter.section("MonitorTable", (void *)table);

			GC_HashTableIterator iterator(table);
			while (NULL != (objectMonitor = (J9ObjectMonitor*)iterator.nextSlot())) {
				J9ThreadAbstractMonitor * monitor = (J9ThreadAbstractMonitor *)objectMonitor->monitor;
				J9Object *objectPtr = (J9Object *)monitor->userData;
				formatter.entry((void *) objectPtr);
			}
			formatter.endSection();
		}
		monitorTableList = monitorTableList->next;
	}

	formatter.end("MonitorTableList", (void *)monitorTableList);
}
