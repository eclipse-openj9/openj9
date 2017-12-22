/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.gccheck;

import static com.ibm.j9ddr.vm29.tools.ddrinteractive.gccheck.CheckBase.J9MODRON_SLOT_ITERATOR_OK;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.HashTable;
import com.ibm.j9ddr.vm29.j9.SlotIterator;
import com.ibm.j9ddr.vm29.j9.StringTable;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9HashTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_StringTablePointer;

class CheckStringTable extends Check
{
	@Override
	public void check()
	{
		// TODO : wouldn't it be easier to use the iterator?
		try {
			MM_StringTablePointer stringTable = getGCExtensions().stringTable();
			long tableCount = stringTable._tableCount().longValue();
			for (long tableIndex = 0; tableIndex < tableCount; tableIndex++) {
				J9HashTablePointer hashTable = J9HashTablePointer.cast(stringTable._table().at(tableIndex));
				SlotIterator<PointerPointer> stringTableIterator = HashTable.fromJ9HashTable(hashTable,
						true,
						PointerPointer.class,
						new StringTable.StringHashFunction<PointerPointer>(), 
						new StringTable.StringComparatorFunction<PointerPointer>()).iterator();
				while (stringTableIterator.hasNext()) {
					PointerPointer slot = stringTableIterator.next();
					if (slot.notNull()) {
						if (_engine.checkSlotPool(slot, VoidPointer.cast(hashTable)) != J9MODRON_SLOT_ITERATOR_OK ){
							return;
						}
					}
				}
			}
		} catch (CorruptDataException e) {
			// TODO: handle exception
		}
		
		// TODO : what about the cache?
	}

	@Override
	public String getCheckName()
	{
		return "STRING TABLE";
	}

	@Override
	public void print()
	{
		// TODO : wouldn't it be easier to use the iterator?
		try {
			MM_StringTablePointer stringTable = getGCExtensions().stringTable();
			long tableCount = stringTable._tableCount().longValue();
			ScanFormatter formatter = new ScanFormatter(this, "StringTable", stringTable);
			for (long tableIndex = 0; tableIndex < tableCount; tableIndex++) {
				J9HashTablePointer hashTablePtr = J9HashTablePointer.cast(stringTable._table().at(tableIndex));
				HashTable<PointerPointer> hashTable = HashTable.fromJ9HashTable(
						hashTablePtr, 
						true,
						PointerPointer.class,						
						new StringTable.StringHashFunction<PointerPointer>(), 
						new StringTable.StringComparatorFunction<PointerPointer>());
				SlotIterator<PointerPointer> stringTableIterator = hashTable.iterator();
				while (stringTableIterator.hasNext()) {
					PointerPointer slot = stringTableIterator.next();
					if (slot.notNull()) {
						formatter.entry(slot.at(0));
					}
				}
			}
			formatter.end("StringTable", stringTable);
		} catch (CorruptDataException e) {
			// TODO: handle exception
		}
		
		// TODO : what about the cache?
	}

}
