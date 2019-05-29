/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.structure.MM_HeapMap;
import com.ibm.j9ddr.vm29.types.UDATA;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapMap;


public class GCHeapMapWordIterator extends GCIterator {
	
	public static UDATA J9MODRON_HEAP_BYTES_PER_UDATA_OF_HEAP_MAP = new UDATA(MM_HeapMap.J9MODRON_HEAP_SLOTS_PER_HEAPMAP_BIT * UDATA.SIZEOF * UDATA.SIZEOF * GCHeapMap.BITS_IN_BYTES);
	
	private UDATA _cache;
	private UDATAPointer _heapSlotCurrent;
	private J9ObjectPointer _next = null;
	
	public GCHeapMapWordIterator(UDATA heapMapWord, VoidPointer heapCardAddress)
	{
		_cache = heapMapWord;
		_heapSlotCurrent = UDATAPointer.cast(heapCardAddress);
		
		_next = nextObject();
	}
	
	public GCHeapMapWordIterator(GCHeapMap heapMap, VoidPointer heapCardAddress) throws CorruptDataException
	{
		UDATA heapOffsetInBytes = UDATA.cast(heapCardAddress).sub(UDATA.cast(heapMap.getHeapBase()));
		U8Pointer heapMapBits = U8Pointer.cast(heapMap.getHeapMapBits());
		UDATAPointer mapPointer = UDATAPointer.cast(heapMapBits.add(heapOffsetInBytes.div(GCHeapMap.J9MODRON_HEAP_SLOTS_PER_HEAPMAP_SLOT)));
//		Assert_MM_true(0 == ((UDATA)mapPointer & (sizeof(UDATA) - 1)));
		_cache = mapPointer.at(0);
		_heapSlotCurrent = UDATAPointer.cast(heapCardAddress);
		
		_next = nextObject();
	}
	
	private J9ObjectPointer nextObject()
	{
		J9ObjectPointer nextObject = null;
		
		if (!_cache.eq(0)) {
			/* TODO: lpnguyen, Note that numberOfTrailingZeros in java refers == numberOfLeadingZeros in C version of this.. */
			int trailingZeros = _cache.numberOfTrailingZeros();
			long slotsToSkip = (trailingZeros * MM_HeapMap.J9MODRON_HEAP_SLOTS_PER_HEAPMAP_BIT);
			_heapSlotCurrent = _heapSlotCurrent.add(slotsToSkip);
			_cache = _cache.rightShift(trailingZeros);
			nextObject = J9ObjectPointer.cast(_heapSlotCurrent);
			/* skip to the next bit in _heapSlotCurrent and in _cache */
			_heapSlotCurrent = _heapSlotCurrent.add(MM_HeapMap.J9MODRON_HEAP_SLOTS_PER_HEAPMAP_BIT);
			_cache = _cache.rightShift(1);
		}
		
		return nextObject;
	}
	
	public boolean hasNext()
	{
		return (null != _next);
	}
	
	public VoidPointer nextAddress()
	{
		throw new UnsupportedOperationException("This iterator cannot return addresses");
	}
	
	public J9ObjectPointer next()
	{
		if (hasNext()) {
			J9ObjectPointer objectToReturn = _next;
			_next = nextObject();
			return objectToReturn;	
		} else {
			throw new NoSuchElementException("There are no more items available through this iterator");
		}
		
	}

}
