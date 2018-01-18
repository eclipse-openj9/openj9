/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9.gc;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.generated.MM_MemoryPoolAggregatedCellListPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_MemoryPoolPointer;

public class GCMemoryPoolAggregatedCellList extends GCMemoryPool {
	protected MM_MemoryPoolAggregatedCellListPointer _memoryPool = null;
	
	protected GCMemoryPoolAggregatedCellList(GCHeapRegionDescriptor region, MM_MemoryPoolPointer memoryPool) throws CorruptDataException
	{
		super(region, memoryPool);
		init(region);
	}
	
	private void init(GCHeapRegionDescriptor region) throws CorruptDataException
	{
		_region = region;
		_memoryPoolType = MemoryPoolType.SEGREGATED;
		_memoryPool = MM_MemoryPoolAggregatedCellListPointer.cast(region.getMemoryPool());
	}
	
	public GCHeapLinkedFreeHeader getFirstFreeEntry() throws CorruptDataException
	{
		return GCHeapLinkedFreeHeader.fromLinkedFreeHeaderPointer(_memoryPool._freeListHead());
	}
	
	public GCFreeListHeapIterator freeListIterator() throws CorruptDataException
	{
		return new GCFreeListIteratorAggregatedCellList(this);
	}
	
	@Override
	public void checkFreeListsImpl()
	{
		try {
			GCFreeListHeapIterator freeEntryIterator = freeListIterator();
			GCHeapLinkedFreeHeader previousFreeEntry = null;
			
			while (freeEntryIterator.hasNext()) {
				GCHeapLinkedFreeHeader freeListEntry = freeEntryIterator.next();
				
				/* Free List Entry(FLE) checks */
				try {
					/* Call the generic check from base class */
					freeEntryCheck(freeListEntry, previousFreeEntry);
					/* Cache the previousFreeEntry for ordering checks */
					previousFreeEntry = freeListEntry;
				} catch (CorruptFreeEntryException e) {
					raiseCorruptDataEvent("Free list corruption detected", e, false);
				} catch (CorruptDataException e) {
					raiseCorruptDataEvent("Corruption detected in free entry", e, false);
				}
			}
		} catch (CorruptDataException e) {
			raiseCorruptDataEvent("Data corruption detected while validating freelists", e, false);
		}
	}
	
	private void freeEntryCheck(GCHeapLinkedFreeHeader freeListEntry, GCHeapLinkedFreeHeader previousFreeEntry) throws CorruptFreeEntryException, CorruptDataException
	{
		freeEntryCheckGeneric(freeListEntry);
		if (previousFreeEntry != null) {
			/* Do the free list entries have correct ordering ? In Metronome, they go in descending order */
			if (previousFreeEntry.getHeader().lte(freeListEntry.getHeader())) {
				throw new CorruptFreeEntryException("invalidOrdering", freeListEntry.getHeader());
			}
		}
	}
}
