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
#include "CheckStringTable.hpp"
#include "ModronTypes.hpp"
#include "ScanFormatter.hpp"
#include "StringTable.hpp"

GC_Check *
GC_CheckStringTable::newInstance(J9JavaVM *javaVM, GC_CheckEngine *engine)
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(javaVM)->getForge();
	
	GC_CheckStringTable *check = (GC_CheckStringTable *) forge->allocate(sizeof(GC_CheckStringTable), MM_AllocationCategory::DIAGNOSTIC, J9_GET_CALLSITE());
	if(check) {
		new(check) GC_CheckStringTable(javaVM, engine);
	}
	return check;
}

void
GC_CheckStringTable::kill()
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(_javaVM)->getForge();
	forge->free(this);
}

void
GC_CheckStringTable::check()
{
	MM_StringTable *stringTable = MM_GCExtensions::getExtensions(_javaVM)->getStringTable();
	for (UDATA tableIndex = 0; tableIndex < stringTable->getTableCount(); tableIndex++) {
		GC_HashTableIterator stringTableIterator(stringTable->getTable(tableIndex));
		J9Object **slot;

		while((slot = (J9Object **)stringTableIterator.nextSlot()) != NULL) {
			if (_engine->checkSlotPool(_javaVM, slot, stringTable->getTable(tableIndex)) != J9MODRON_SLOT_ITERATOR_OK ){
				return;
			}
		}
	}
}

void
GC_CheckStringTable::print()
{
	MM_StringTable *stringTable = MM_GCExtensions::getExtensions(_javaVM)->getStringTable();
	GC_ScanFormatter formatter(_portLibrary, "StringTable", (void *)stringTable);
	for (UDATA tableIndex = 0; tableIndex < stringTable->getTableCount(); tableIndex++) {
		GC_HashTableIterator stringTableIterator(stringTable->getTable(tableIndex));
		J9Object **slot;

		while((slot = (J9Object **)stringTableIterator.nextSlot()) != NULL) {
			formatter.entry((void *)*slot);
		}
	}
	formatter.end("StringTable", (void *)stringTable);
}
