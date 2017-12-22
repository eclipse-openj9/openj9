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

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.AlgorithmVersion;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapRegionDescriptorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_MemoryPoolBumpPointerPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_MemoryPoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_MemorySubSpacePointer;
import com.ibm.j9ddr.vm29.structure.J9MemorySegment;
import com.ibm.j9ddr.vm29.structure.MM_HeapRegionDescriptor$RegionType;
import com.ibm.j9ddr.vm29.types.UDATA;

class GCHeapRegionDescriptor_V1 extends GCHeapRegionDescriptor
{
	protected VoidPointer lowAddress;
	protected VoidPointer highAddress;
	protected UDATA regionsInSpan;
	protected long regionType;
	protected UDATA typeFlags;
	protected MM_MemorySubSpacePointer memorySubSpace;
	protected MM_MemoryPoolPointer memoryPool;
	protected GCHeapRegionDescriptor headOfSpan;
		
	public GCHeapRegionDescriptor_V1(MM_HeapRegionDescriptorPointer heapRegionDescriptorPointer) throws CorruptDataException 
	{
		super(heapRegionDescriptorPointer);
		heapRegionDescriptor = heapRegionDescriptorPointer;
		init();
	}
	
	protected void init() throws CorruptDataException
	{
		lowAddress = heapRegionDescriptor._lowAddress();
		regionsInSpan = heapRegionDescriptor._regionsInSpan();
		regionType = heapRegionDescriptor._regionType();
		MM_MemorySubSpacePointer subSpace = heapRegionDescriptor._memorySubSpace();
		if(subSpace.notNull()) {
			typeFlags = subSpace._memoryType().bitOr(J9MemorySegment.MEMORY_TYPE_RAM);
		} else {
			typeFlags = new UDATA(0);
		}
		memorySubSpace = heapRegionDescriptor._memorySubSpace(); 
		memoryPool = heapRegionDescriptor._memoryPool();
		
		if (regionsInSpan.eq(0)) {
			highAddress  = heapRegionDescriptor._highAddress();
		} else {
			UDATA delta = UDATA.cast(heapRegionDescriptor._highAddress()).sub(UDATA.cast(lowAddress));
			highAddress = lowAddress.addOffset(regionsInSpan.mult(delta));
		}
		
		MM_HeapRegionDescriptorPointer head = heapRegionDescriptor._headOfSpan();
		if(head.isNull() || head.eq(heapRegionDescriptor)) {
			headOfSpan = this;
		} else {
			headOfSpan = GCHeapRegionDescriptor.fromHeapRegionDescriptor(head);
		}		
	}
	
	@Override
	public VoidPointer getLowAddress()
	{
		return lowAddress;
	}
	
	@Override
	public VoidPointer getHighAddress()
	{
		return highAddress;
	}

	@Override	
	public UDATA getSize()
	{
		return UDATA.cast(getHighAddress()).sub(UDATA.cast(getLowAddress()));
	}
	
	@Override
	public UDATA getTypeFlags()
	{
		return typeFlags;
	}
	
	/**
	 * @return true if the region has an up-to-date mark map
	 */
 	@Override
	public boolean hasValidMarkMap()
	{
		if ((MM_HeapRegionDescriptor$RegionType.ADDRESS_ORDERED_MARKED == regionType) || 
			(MM_HeapRegionDescriptor$RegionType.BUMP_ALLOCATED_MARKED == regionType)) {
			return true;
		} else {
			return false;
		}
	}
	
	@Override
	public boolean containsObjects()
	{
		if ((J9BuildFlags.gc_realtime && 
				((MM_HeapRegionDescriptor$RegionType.SEGREGATED_LARGE == regionType) ||
				 (MM_HeapRegionDescriptor$RegionType.SEGREGATED_SMALL == regionType))
			 ) ||
			(MM_HeapRegionDescriptor$RegionType.ADDRESS_ORDERED == regionType) || 
			(MM_HeapRegionDescriptor$RegionType.ADDRESS_ORDERED_MARKED == regionType) ||
			(MM_HeapRegionDescriptor$RegionType.BUMP_ALLOCATED == regionType) ||
			(MM_HeapRegionDescriptor$RegionType.BUMP_ALLOCATED_MARKED == regionType)) {
			return true;
		} else {
			return false;
		}
	}

	protected VoidPointer getWalkableHighAddress() throws CorruptDataException
	{
		if ((MM_HeapRegionDescriptor$RegionType.BUMP_ALLOCATED == regionType) ||
			(MM_HeapRegionDescriptor$RegionType.BUMP_ALLOCATED_MARKED == regionType)) {
			return MM_MemoryPoolBumpPointerPointer.cast(heapRegionDescriptor._memoryPool())._allocatePointer();
		} else {
			return getHighAddress();
		}
	}
	
	@Override
	public long getRegionType()
	{
		return regionType;
	}
	
	@Override
	public MM_MemorySubSpacePointer getSubSpace() 
	{
		return memorySubSpace;
	}

	@Override	
	public MM_MemoryPoolPointer getMemoryPool() 
	{
		return memoryPool;
	}

	@Override	
	public boolean isAddressInRegion(AbstractPointer address)
	{
		return address.gte(getLowAddress()) && address.lt(getHighAddress());
	}

	@Override	
	public GCObjectHeapIterator objectIterator(boolean includeLiveObjects, boolean includeDeadObjects) throws CorruptDataException
	{
		GCObjectHeapIterator iterator = null;
		if (hasValidMarkMap()) {
			if (includeLiveObjects) {
				AlgorithmVersion version = AlgorithmVersion.getVersionOf(AlgorithmVersion.GC_OBJECT_HEAP_ITERATOR_MARK_MAP_VERSION);
				switch (version.getAlgorithmVersion()) {
					// Add case statements for new algorithm versions
					default:	
						iterator = new GCObjectHeapIteratorMarkMapIterator_V1(this);
				}
			} else {
				iterator = new GCObjectHeapIteratorNullIterator();
			}
		} else if (containsObjects()) {
			// contains objects and not marked so use an address ordered list heap iterator
			AlgorithmVersion version = AlgorithmVersion.getVersionOf(AlgorithmVersion.GC_OBJECT_HEAP_ITERATOR_ADDRESS_ORDERED_LIST_VERSION);
			switch (version.getAlgorithmVersion()) {
				// Add case statements for new algorithm versions
				default:
					iterator = new GCObjectHeapIteratorAddressOrderedList_V1(U8Pointer.cast(getLowAddress()), U8Pointer.cast(getWalkableHighAddress()), includeLiveObjects, includeDeadObjects);
			}
		} else {
			//there is nothing we can walk here (generally means it is either free, uncommitted, or an arraylet leaf)
			iterator = new GCObjectHeapIteratorNullIterator();
		}
		
		return iterator;
	}
	
	@Override
	public boolean isImmortal()
	{
		if(getTypeFlags().allBitsIn(J9MemorySegment.MEMORY_TYPE_IMMORTAL)) {
			return true;
		} else {
			return false;
		}
	}
	
	@Override
	public boolean isScoped()
	{
		if(getTypeFlags().allBitsIn(J9MemorySegment.MEMORY_TYPE_SCOPED)) {
			return true;
		} else {
			return false;
		}
	}

	@Override
	public GCHeapRegionDescriptor getHeadOfSpan()
	{
		return headOfSpan;
	}

	@Override
	public String descriptionString()
	{
		if(MM_HeapRegionDescriptor$RegionType.RESERVED == regionType) {
			return "RESERVED";
		}
		if(MM_HeapRegionDescriptor$RegionType.FREE == regionType) {
			return "FREE";
		}
		if(J9BuildFlags.gc_realtime) {
			if(MM_HeapRegionDescriptor$RegionType.SEGREGATED_SMALL == regionType) {
				return "SEGREGATED_SMALL";
			}
			if(MM_HeapRegionDescriptor$RegionType.SEGREGATED_LARGE == regionType) {
				return "SEGREGATED_LARGE";
			}
		}
		if(J9BuildFlags.gc_arraylets) {
			if(MM_HeapRegionDescriptor$RegionType.ARRAYLET_LEAF == regionType) {
				return "ARRAYLET_LEAF";
			}
		}
		if(MM_HeapRegionDescriptor$RegionType.ADDRESS_ORDERED == regionType) {
			return "ADDRESS_ORDERED";
		}
		if(MM_HeapRegionDescriptor$RegionType.ADDRESS_ORDERED_IDLE == regionType) {
			return "ADDRESS_ORDERED_IDLE";
		}
		if(MM_HeapRegionDescriptor$RegionType.ADDRESS_ORDERED_MARKED == regionType) {
			return "ADDRESS_ORDERED_MARKED";
		}
		if(MM_HeapRegionDescriptor$RegionType.BUMP_ALLOCATED == regionType) {
			return "BUMP_ALLOCATED";
		}
		if(MM_HeapRegionDescriptor$RegionType.BUMP_ALLOCATED_IDLE == regionType) {
			return "BUMP_ALLOCATED_IDLE";
		}
		if(MM_HeapRegionDescriptor$RegionType.BUMP_ALLOCATED_MARKED == regionType) {
			return "BUMP_ALLOCATED_MARKED";
		}
		
		return "UNKNOWN";
	}
	
	

}
