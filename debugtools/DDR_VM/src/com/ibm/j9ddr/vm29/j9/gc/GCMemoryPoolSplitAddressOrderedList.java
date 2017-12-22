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
package com.ibm.j9ddr.vm29.j9.gc;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

import java.util.Collections;
import java.util.Comparator;
import java.util.Iterator;
import java.util.List;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.util.IteratorHelpers;
import com.ibm.j9ddr.vm29.pointer.generated.J9ModronAllocateHintPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ModronFreeListPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapLinkedFreeHeaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_MemoryPoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_MemoryPoolSplitAddressOrderedListPointer;
import com.ibm.j9ddr.vm29.types.UDATA;

public class GCMemoryPoolSplitAddressOrderedList extends GCMemoryPool {
	protected UDATA _heapFreeListCount = new UDATA(0);
	protected MM_MemoryPoolSplitAddressOrderedListPointer _memoryPool = null;
	protected J9ModronFreeListPointer _heapFreeLists = null;
	protected GCHeapRegionDescriptor _region = null;
	
	protected GCMemoryPoolSplitAddressOrderedList(GCHeapRegionDescriptor region, MM_MemoryPoolPointer memoryPool) throws CorruptDataException
	{
		super(region, memoryPool);
		init(region, memoryPool);
	}
	
	private void init(GCHeapRegionDescriptor region, MM_MemoryPoolPointer memoryPool) throws CorruptDataException
	{
		_region = region;
		_memoryPoolType = MemoryPoolType.SAOL;
		_memoryPool = MM_MemoryPoolSplitAddressOrderedListPointer.cast(memoryPool);
		_heapFreeLists = _memoryPool._heapFreeLists();
		_heapFreeListCount = _memoryPool._heapFreeListCount();
	}
	
	public UDATA getFreeListCount() throws CorruptDataException
	{
		return _heapFreeListCount;
	}
	
	public GCModronAllocateHintIterator hintIterator() throws CorruptDataException
	{
		return new GCModronAllocateHintIteratorSAOL(this);
	}
	
	public GCHeapLinkedFreeHeader getFirstEntryAtFreeListIndex(UDATA freeListArrayIndex) throws CorruptDataException
	{
		return GCHeapLinkedFreeHeader.fromLinkedFreeHeaderPointer(_heapFreeLists.add(freeListArrayIndex)._freeList());
	}
	
	public J9ModronAllocateHintPointer getFirstHintForFreeList(UDATA freeListArrayIndex) throws CorruptDataException
	{
		return _heapFreeLists.add(freeListArrayIndex)._hintActive();
	}
	
	private void freeEntryCheck(GCHeapLinkedFreeHeader freeListEntry, GCHeapLinkedFreeHeader previousFreeEntry) throws CorruptFreeEntryException, CorruptDataException
	{
		/* Checks Standard GC specific parameters */
		MM_HeapLinkedFreeHeaderPointer freeEntryPointer = freeListEntry.getHeader();
		MM_HeapLinkedFreeHeaderPointer previousFreeEntryPointer = MM_HeapLinkedFreeHeaderPointer.NULL;
		
		freeEntryCheckGeneric(freeListEntry); /* Do generic checks */
		
		/* Checks ordering of AOL and SAOL entries */
		if (null != previousFreeEntry) {
			previousFreeEntryPointer = previousFreeEntry.getHeader();
			/* Validate list ordering.
			 * (This is done in GC code at runtime as an assert0 but just in case) */
			if (previousFreeEntryPointer.gte(freeEntryPointer)) {
				throw new CorruptFreeEntryException("invalidOrdering", freeEntryPointer);
			}
		}
		
		/* Validate size :
		 * Check if size less than 512 / 768 bytes */
		if (freeListEntry.getSize().lt(GCBase.getExtensions().tlhMinimumSize())) {
			throw new CorruptFreeEntryException("sizeFieldInvalid", freeListEntry.getHeader());
		}
	}
	
	public GCFreeListHeapIterator freeListIterator() throws CorruptDataException
	{
		return new GCFreeListHeapIteratorSplitAddressOrderedList(this);
	}
	
	/* Validates hints and free list entries without needing additional storage for the free list entries */
	@Override
	public void checkFreeListsImpl()
	{
		try {
			/* Get free list and hint iterators for self */
			GCFreeListHeapIterator freeEntryIterator = freeListIterator();
			GCHeapLinkedFreeHeader previousFreeEntry = null;
			J9ModronAllocateHintPointer hint = null;
			MM_HeapLinkedFreeHeaderPointer freeEntryFromHint = null;
			
			/* Needed to return hints in ascending order of free entry address
			 * Here, we convert : iterator -> list -> (sort list) -> iterator */
			@SuppressWarnings("unchecked") List<J9ModronAllocateHintPointer> hints = IteratorHelpers.toList(hintIterator());
			Collections.sort(hints, new Comparator<J9ModronAllocateHintPointer>() {
				public int compare(J9ModronAllocateHintPointer o1, J9ModronAllocateHintPointer o2)
				{
					try {
						if (o2.heapFreeHeader().gt(o1.heapFreeHeader())) {
							return -1;
						} else if (o2.heapFreeHeader().eq(o1.heapFreeHeader())) {
							return 0;
						} else {
							return 1;
						}
					} catch (CorruptDataException e) {
						raiseCorruptDataEvent("corruption detected in allocation hints", e, false);
					}
					throw new UnsupportedOperationException("Unreachable");
				}
			});
			
			Iterator<J9ModronAllocateHintPointer> allocHintIterator = hints.iterator();
			
			if (allocHintIterator.hasNext()) {
				hint = allocHintIterator.next();
				freeEntryFromHint = hint.heapFreeHeader();
			}
			
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
				
				/* Check hint is in Free List */
				MM_HeapLinkedFreeHeaderPointer freeListEntryPointer = freeListEntry.getHeader();
				
				/* null is the success condition. It indicated that the free entry from the hint was found in the pools free list.
				 * If we find a entry from hint that is less than current free entry in pools list, fail. */
				while ((null != freeEntryFromHint) && freeEntryFromHint.lte(freeListEntryPointer)) {
					if (freeEntryFromHint.lt(freeListEntryPointer)) {
						throw new CorruptHintException("allocHintFreeEntryCorrupt", hint);
					} else {
						if (allocHintIterator.hasNext()) {
							hint = allocHintIterator.next();
							freeEntryFromHint = hint.heapFreeHeader();
						} else {
							freeEntryFromHint = null;
						}
					}
				}
			}
			
			/* If the FLE from the hint was never found in the pools FL, it was a bogus entry */
			if (null != freeEntryFromHint) {
				raiseCorruptDataEvent("Hint corruption detected ", new CorruptHintException("allocHintFreeEntryCorrupt", hint), false);
			}
		} catch (CorruptDataException e) {
			raiseCorruptDataEvent("Data corruption detected while validating freelists", e, false);
		}
	}
}
