/*******************************************************************************
 * Copyright (c) 2001, 2015 IBM Corp. and others
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

import java.util.ArrayList;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_GCExtensionsPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapMapPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_IncrementalGenerationalGCPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_MarkMapPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_ParallelGlobalGCPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_SegregatedGCPointer;
import com.ibm.j9ddr.vm29.structure.J9Consts;
import com.ibm.j9ddr.vm29.structure.MM_HeapMap;
import com.ibm.j9ddr.vm29.types.UDATA;

public abstract class GCHeapMap 
{
	public static final long J9BITS_BITS_IN_SLOT = UDATA.SIZEOF * 8L;
	public static final long BITS_IN_BYTES = 8L;
	public static final UDATA J9MODRON_HEAP_SLOTS_PER_HEAPMAP_SLOT = new UDATA(MM_HeapMap.J9MODRON_HEAP_SLOTS_PER_HEAPMAP_BIT * J9BITS_BITS_IN_SLOT);
	public static final UDATA J9MODRON_HEAP_BYTES_PER_HEAPMAP_BIT = new UDATA(MM_HeapMap.J9MODRON_HEAP_SLOTS_PER_HEAPMAP_BIT * UDATA.SIZEOF);
	public static final UDATA J9MODRON_HEAP_BYTES_PER_HEAPMAP_BYTE = J9MODRON_HEAP_BYTES_PER_HEAPMAP_BIT.mult(BITS_IN_BYTES);
	public static final UDATA J9MODRON_HEAP_BYTES_PER_HEAPMAP_SLOT = J9MODRON_HEAP_BYTES_PER_HEAPMAP_BYTE.mult(UDATA.SIZEOF);
	
	protected MM_HeapMapPointer _heapMap;
	protected VoidPointer _heapBase;
	protected VoidPointer _heapTop;
	protected UDATAPointer _heapMapBits;
	protected UDATA _maxOffset;
	protected UDATA _heapMapIndexShift;	/* number of low-order bits to be shifted out of heap address to obtain heap map slot index */
	protected UDATA _heapMapBitMask;		/* bit mask for capturing bit index within heap map slot from (unshifted) heap address */
	protected UDATA _heapMapBitShift;		/* number of low-order bits to be shifted out of captured bit index to obtain actual bit index */
	
	public static class MarkedObject
	{
		public final J9ObjectPointer object;
		public final UDATAPointer markBitsSlot;
		public final J9ObjectPointer relocatedObject;
		
		public MarkedObject(J9ObjectPointer object, UDATAPointer markBitsSlot)
		{
			this(object, markBitsSlot, null); 
		}
		
		public MarkedObject(J9ObjectPointer object, UDATAPointer markBitsSlot, J9ObjectPointer relocatedObject)
		{
			this.object = object;
			this.markBitsSlot = markBitsSlot;
			this.relocatedObject = relocatedObject;
		}
		
		public boolean wasRelocated()
		{
			return relocatedObject != null;
		}
	}
	
	public static GCHeapMap from() throws CorruptDataException 
	{
		MM_GCExtensionsPointer extensions = GCExtensions.getGCExtensionsPointer();
		if (GCExtensions.isStandardGC()) {
			if ( !GCExtensions.isSegregatedHeap()) {
				MM_ParallelGlobalGCPointer pgc = MM_ParallelGlobalGCPointer.cast(extensions._globalCollector());
				MM_MarkMapPointer markMap = pgc._markingScheme()._markMap();
				return new GCMarkMapStandard(markMap);
			} else {
				MM_SegregatedGCPointer sgc = MM_SegregatedGCPointer.cast(extensions._globalCollector());
				MM_MarkMapPointer markMap = sgc._markingScheme()._markMap();
				return new GCMarkMap(markMap);
			}
		} else if (GCExtensions.isVLHGC()) {
			// probably needs a proper subclass
			MM_IncrementalGenerationalGCPointer igc = MM_IncrementalGenerationalGCPointer.cast(extensions._globalCollector());
			MM_MarkMapPointer markMap = igc._markMapManager()._previousMarkMap();
			return new GCMarkMap(markMap);
		} else if (GCExtensions.isMetronomeGC()) {
			MM_MarkMapPointer markMap = extensions.realtimeGC()._markingScheme()._markMap();
			return new GCMarkMap(markMap);
		} else {
			throw new UnsupportedOperationException("GC policy not supported");
		}
	}
	
	public static GCHeapMap fromHeapMap(MM_HeapMapPointer heapMap) throws CorruptDataException 
	{
		if (GCExtensions.isStandardGC()) {
			if (!GCExtensions.isSegregatedHeap()) {
				return new GCMarkMapStandard(heapMap);
			} else {
				return new GCMarkMap(heapMap);
			}
		} else if (GCExtensions.isVLHGC() || GCExtensions.isMetronomeGC()) {
			return new GCMarkMap(heapMap);
		} else {
			throw new UnsupportedOperationException("GC policy not supported");
		}	
	}
	
	public GCHeapMap(MM_HeapMapPointer heapMap) throws CorruptDataException
	{
		_heapMap = heapMap;
		_heapBase = heapMap._heapBase();
		_heapTop = heapMap._heapTop();
		_heapMapBits = heapMap._heapMapBits();
		_maxOffset = UDATA.cast(_heapTop).sub(UDATA.cast(_heapBase));
		try {
			_heapMapIndexShift = heapMap._heapMapIndexShift();
			_heapMapBitMask = heapMap._heapMapBitMask();
			_heapMapBitShift = heapMap._heapMapBitShift();
		} catch (NoSuchFieldError e) {
			if (!GCExtensions.isMetronomeGC()) {
				_heapMapIndexShift = new UDATA(MM_HeapMap.J9MODRON_HEAPMAP_INDEX_SHIFT);
				_heapMapBitMask = new UDATA(MM_HeapMap.J9MODRON_HEAPMAP_BIT_MASK);
				_heapMapBitShift = new UDATA(MM_HeapMap.J9MODRON_HEAPMAP_BIT_SHIFT);
			} else {
				_heapMapIndexShift = new UDATA(MM_HeapMap.J9MODRON_HEAPMAP_LOG_SIZEOF_UDATA + J9Consts.J9VMGC_SIZECLASSES_LOG_SMALLEST);
				_heapMapBitMask = new UDATA(1).leftShift(_heapMapIndexShift).sub(1);
				_heapMapBitShift = new UDATA(J9Consts.J9VMGC_SIZECLASSES_LOG_SMALLEST);
			}
		}
	}

	public MM_HeapMapPointer getHeapMap()
	{
		return _heapMap;
	}
	
	public VoidPointer getHeapBase()
	{
		return _heapBase;
	}
	
	public UDATAPointer getHeapMapBits()
	{
		return _heapMapBits;
	}
	
	public VoidPointer getHeapTop()
	{
		return _heapTop;
	}
	
	public int getObjectGrain()
	{
		return (1 << _heapMapBitShift.intValue());
	}
	
	public int getPageSize(J9ObjectPointer object)
	{
		return getObjectGrain() * 8 * UDATA.SIZEOF;
	}

	/**
	 * Return the index into the heap map bits array and the bitmask to use.
	 * @param object
	 * @return an array containing the slot index and bit mask
	 */
	public UDATA[] getSlotIndexAndMask(J9ObjectPointer object)
	{
		UDATA heapBaseOffset = UDATA.cast(object).sub(UDATA.cast(_heapBase));
		if (heapBaseOffset.gte(_maxOffset)) {
			throw new IllegalArgumentException("Object is not in heap: " + object);
		}
		
		UDATA bitIndex = heapBaseOffset.bitAnd(_heapMapBitMask);
		bitIndex = bitIndex.rightShift(_heapMapBitShift);
		UDATA bitMask = new UDATA(1).leftShift(bitIndex);
		
		UDATA slotIndex = heapBaseOffset.rightShift(new UDATA(_heapMapIndexShift));
		
		UDATA[] result = new UDATA[2];
		result[0] = slotIndex;
		result[1] = bitMask;
		return result;
	}

	
	/**
	 * Check if a bit is set in the GCHeapMap
	 * 
	 * @param address Must be a pointer into the heap. The pointer must point to
	 *            committed heap memory.
	 * @return True if the bit is set. False otherwise.
	 * @throws CorruptDataException
	 */
	public boolean isBitSet(J9ObjectPointer address) throws CorruptDataException
	{
		if (address.gte(_heapTop) || address.lt(_heapBase)) {
			throw new IllegalArgumentException("Pointer outside of the committed heap");
		}

		return isBitSetNoCheck(address);
	}
	
	/**
	 * Check if a bit is set in the GCHeapMap. Differs from isBitSet(J9Object)
	 * in that it will return true if the address is not in the heap.
	 * 
	 * @param address An address. If the address is not in the Heap, will return
	 *            True.
	 * @return True if the bit is set. True if the address of the object is not
	 *         in the heap. False otherwise.
	 */
	public boolean isMarked(J9ObjectPointer address) throws CorruptDataException
	{
		if (address.gte(_heapTop) || address.lt(_heapBase)) {
			return true;
		}

		return isBitSetNoCheck(address);
	}
	
	/**
	 * Check if a bit is set without checking the pointer. Will throw an
	 * exception if the pointer is forces HeapMap to read from uncommitted
	 * memory.
	 * 
	 * @param object
	 * @throws CorruptDataException
	 */
	protected boolean isBitSetNoCheck(J9ObjectPointer object) throws CorruptDataException
	{
		UDATA[] indexAndMask = getSlotIndexAndMask(object);
		boolean result = _heapMapBits.add(indexAndMask[0]).at(0).bitAnd(indexAndMask[1]).gt(0);
		return result;
	}
	
	/**
	 * Query the mark map to see if the specified object is found.
	 * If so return a MarkedObject describing it, otherwise null.
	 * @param object The object to query
	 * @return A MarkedObject, or null
	 * @throws CorruptDataException
	 */
	public MarkedObject queryObject(J9ObjectPointer object) throws CorruptDataException
	{
		MarkedObject[] result = queryRange(object, object.addOffset(getObjectGrain()));
		if ((result.length > 0) && (result[0].object.eq(object))) {
			return result[0];
		} else {
			return null;
		}
	}
	
	/**
	 * Query the mark map to see what objects are marked within the specified range.
	 * Return an array of MarkedObject records describing each entry found.
	 * @param base Bottom of the range to query (inclusive)
	 * @param top Top of the range to query (exclusive)
	 * @return A MarkedObject[] containing the results
	 * @throws CorruptDataException
	 */
	public MarkedObject[] queryRange(J9ObjectPointer base, J9ObjectPointer top) throws CorruptDataException
	{
		 /* Naive implementation; should work but slow */
		ArrayList<MarkedObject> results = new ArrayList<MarkedObject>();
		if (base.lt(_heapBase)) {
			base = J9ObjectPointer.cast(_heapBase);
		}
		if (top.gt(_heapTop)) {
			top = J9ObjectPointer.cast(_heapTop);
		}
		if (base.gt(top)) {
			base = top;
		}
		J9ObjectPointer cursor = base;
		while (cursor.lt(top)) {
			UDATA[] indexAndMask = getSlotIndexAndMask(cursor);
			UDATAPointer slot = _heapMapBits.add(indexAndMask[0]);
			if (slot.at(0).bitAnd(indexAndMask[1]).gt(0)) {
				results.add(new MarkedObject(cursor, slot));
			}
			
			cursor = cursor.addOffset(getObjectGrain());
		}
		
		return results.toArray(new MarkedObject[results.size()]);
	}
}
