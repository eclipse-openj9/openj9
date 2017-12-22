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
package com.ibm.j9ddr.vm29.j9.gc;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_GCExtensionsPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_IncrementalGenerationalGCPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_MarkMapManagerPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_MarkMapPointer;
import com.ibm.j9ddr.vm29.structure.MM_HeapMap;
import com.ibm.j9ddr.vm29.types.UDATA;

public class GCObjectHeapIteratorMarkMapIterator_V1 extends GCObjectHeapIterator
{
	private static final int HEAP_BYTES_PER_MAP_BIT = (int)MM_HeapMap.J9MODRON_HEAP_SLOTS_PER_HEAPMAP_BIT * UDATA.SIZEOF;
	private static final int HEAP_BYTES_PER_MAP_SLOT = HEAP_BYTES_PER_MAP_BIT * (int)MM_HeapMap.BITS_IN_BYTE * UDATA.SIZEOF;
	
	//the base of the Java heap since it is required in order to convert an offset into the heap into a concrete object pointer
	protected VoidPointer _heapBase;
	//the raw data representing the mark map.  Required for reading mark map words directly
	protected UDATAPointer _heapMapBits;
	//the current mark map word.  The least significant bit is the least significant corresponding heap address.  This is modified as we walk through the word
	protected UDATA _markWordCache;
	//the index of the next mark map word
	protected long _nextSlotIndex;
	//the index of the mark map word after the range we want to view (a "top" pointer used for comparing _nextSlotIndex to determine when we are done)
	protected long _topSlotIndex;
	//the number of times we have shifted _nextSlotIndex right since reading it
	protected int _shiftCount;
	// The next object to be returned by the iterator, or null if none is cached
	protected J9ObjectPointer _nextObject;

	protected GCObjectHeapIteratorMarkMapIterator_V1(GCHeapRegionDescriptor hrd) throws CorruptDataException
	{
		super(true, false);
		MM_GCExtensionsPointer extensions = getExtensions();
		MM_IncrementalGenerationalGCPointer collector = MM_IncrementalGenerationalGCPointer.cast(extensions._globalCollector());
		MM_MarkMapManagerPointer markMapManager = collector._markMapManager();
		MM_MarkMapPointer markMap = markMapManager._previousMarkMap();
		_heapBase = markMap._heapBase();
		_heapMapBits = markMap._heapMapBits();
		VoidPointer regionBase = hrd.getLowAddress();
		_nextSlotIndex = convertAddressToMapSlotIndex(_heapBase, regionBase);
		VoidPointer regionTop = hrd.getHighAddress();
		_topSlotIndex = convertAddressToMapSlotIndex(_heapBase, regionTop);
		_markWordCache = new UDATA(0);
		_shiftCount = 0;
	}

	private long convertAddressToMapSlotIndex(VoidPointer heapBase, VoidPointer address) throws CorruptDataException
	{
		long heapDelta = address.longValue() - heapBase.longValue();
		return heapDelta / HEAP_BYTES_PER_MAP_SLOT;
	}

	@Override
	public void advance(UDATA size)
	{
		throw new UnsupportedOperationException("Not implemented");
	}
	
	@Override
	public J9ObjectPointer next()
	{
		if(hasNext()) {
			J9ObjectPointer next;
			if(null == _nextObject) {
				nextImpl();
			}
			next = _nextObject;
			_nextObject = null;
			if(null != next) {
				return next;
			}
		}
		throw new NoSuchElementException("No more objects");
	}
	
	@Override
	public J9ObjectPointer peek()
	{
		if(hasNext()) {
			J9ObjectPointer next;
			if(null == _nextObject) {
				nextImpl();
			}
			next = _nextObject;
			if(null != next) {
				return next;
			}
		}
		throw new NoSuchElementException("No more objects");
	}
	
	protected void nextImpl()
	{
		_nextObject = null;
		while ((null == _nextObject) && ((_nextSlotIndex != _topSlotIndex) || (!_markWordCache.eq(0)))) {
			while ((null == _nextObject) && (!_markWordCache.eq(0))) {
				if (_markWordCache.allBitsIn(1)) {
					//this is an object
					_nextObject = convertToObject(_nextSlotIndex - 1, _shiftCount);
				}
				_shiftCount += 1;
				_markWordCache = _markWordCache.rightShift(1);
			}
			if (null == _nextObject) {
				try {
					recacheMarkMapWord();
				} catch (CorruptDataException e) {
					raiseCorruptDataEvent("Error getting next item", e, false);
					throw new NoSuchElementException("Failed to continue reading objects from region");
				}
			}
		}
	}

	private void recacheMarkMapWord() throws CorruptDataException
	{
		while ((_markWordCache.eq(0)) && (_nextSlotIndex != _topSlotIndex)) {
			//recache the mark map word
			_markWordCache = _heapMapBits.at(_nextSlotIndex);
			_nextSlotIndex += 1;
			_shiftCount = 0;
		}
	}

	private J9ObjectPointer convertToObject(long heapMapSlotIndex, int heapMapShiftCount)
	{
		long offsetIntoHeap = (HEAP_BYTES_PER_MAP_SLOT * heapMapSlotIndex) + (HEAP_BYTES_PER_MAP_BIT * heapMapShiftCount); 
		return J9ObjectPointer.cast(_heapBase.addOffset(offsetIntoHeap));
	}

	public boolean hasNext()
	{
		if(null != _nextObject) {
			// Next object is precached
			return true;
		} else {
			boolean hasNext = false;
			try {
				recacheMarkMapWord();
				hasNext = !_markWordCache.eq(0);
			} catch (CorruptDataException e) {
				raiseCorruptDataEvent("Error getting next item", e, false);
			}
			return hasNext;
		}
	}
}
