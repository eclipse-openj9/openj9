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

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.AlgorithmVersion;
import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapRegionDescriptorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapRegionDescriptorSegregatedPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_MemoryPoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_MemorySubSpacePointer;
import com.ibm.j9ddr.vm29.structure.MM_HeapRegionDescriptor$RegionType;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionDescriptorSegregated_V1;
import com.ibm.j9ddr.vm29.types.UDATA;


public abstract class GCHeapRegionDescriptor 
{
	protected MM_HeapRegionDescriptorPointer heapRegionDescriptor;
	
	/* Do not instantiate. Use the factory */
	protected GCHeapRegionDescriptor(MM_HeapRegionDescriptorPointer hrd) throws CorruptDataException 
	{
		heapRegionDescriptor = hrd;
	}
	
	/* TODO: lpnguyen delete this (and all callers of this), temporary method for a silo dance */
	public static GCHeapRegionDescriptor fromHeapRegionDescriptor(GCHeapRegionDescriptor hrd) throws CorruptDataException
	{
		return hrd;
	}
	

	/**
	 * Factory method to construct an appropriate GCHeapRegionDescriptor
	 * 
	 * @param structure the MM_HeapRegionDescriptorPointer structure to view as a GCHeapRegionDescriptor 
	 * 
	 * @return an instance of GCHeapRegionDescriptor 
	 * @throws CorruptDataException 
	 */
	public static GCHeapRegionDescriptor fromHeapRegionDescriptor(MM_HeapRegionDescriptorPointer hrd) throws CorruptDataException
	{
		AlgorithmVersion version = AlgorithmVersion.getVersionOf(AlgorithmVersion.GC_HEAP_REGION_DESCRIPTOR_VERSION);
		switch (version.getAlgorithmVersion()) {
			// Add case statements for new algorithm versions
			default:
				if(isRegionSegregrated(hrd)) {
					return new GCHeapRegionDescriptorSegregated_V1(MM_HeapRegionDescriptorSegregatedPointer.cast(hrd));
				} else {
					return new GCHeapRegionDescriptor_V1(hrd);
				}
		}
	}

	public MM_HeapRegionDescriptorPointer getHeapRegionDescriptorPointer()
	{
		return heapRegionDescriptor;
	}
	
	private static boolean isRegionSegregrated(MM_HeapRegionDescriptorPointer hrd) throws CorruptDataException
	{
		long regionType = hrd._regionType();
		if (J9BuildFlags.gc_realtime) {
			if ((MM_HeapRegionDescriptor$RegionType.SEGREGATED_LARGE == regionType) ||
				(MM_HeapRegionDescriptor$RegionType.SEGREGATED_SMALL == regionType)) {
				return true;
			}
		} 
		return false;
	}

	/**
	 * Get the low address of this region
	 *
	 * @return the lowest address in the region
	 */
	public abstract VoidPointer getLowAddress();
	
	/**
	 * Get the high address of this region
	 *
	 * @return the first address beyond the end of the region
	 */
	public abstract VoidPointer getHighAddress();
	
	/**
	 * @return The number of contiguous bytes represented by the receiver
	 * @throws CorruptDataException
	 */
	public abstract UDATA getSize();
	
	/**
	 * A helper to request the type flags from the Subspace associated with the receiving region.
	 *
	 * @return The type flags of the Subspace associated with this region
	 * @throws CorruptDataException
	 */
	public abstract UDATA getTypeFlags();
	
	/**
	 * @return this region's current type
	 * @throws CorruptDataException
	 */
	public abstract long getRegionType();
	
	/**
	 * @return the memory subspace associated with this region 
	 */
	public abstract MM_MemorySubSpacePointer getSubSpace();
	
	/**
	 * @return the memory subspace associated with this region
	 */
	public abstract MM_MemoryPoolPointer getMemoryPool();
	
	/**
	 * Determine if the specified address is in the region
	 * @parm address - the address to test
	 * @return true if address is within the receiver, false otherwise
	 */
	public abstract boolean isAddressInRegion(AbstractPointer address);
	
	/**
	 * @return true if the region has up-to-date mark map
	 * @throws CorruptDataException
	 */
	public abstract boolean hasValidMarkMap();
	
	/** 
	 * @return true if the region contains objects
	 * @throws CorruptDataException
	 */
	public abstract boolean containsObjects();
	
	/** 
	 * Create an iterator which iterates over all objects (if any) in the region
     *
	 * @parm includeLiveObjects whether to include live objects in the iterator
	 * @parm includeDeadObjects whether to include dead objects in the iterator
	 * @return an GCObjectHeapIterator which iterates over all objects in the region 
	 * @throws CorruptDataException
	 */
	public abstract GCObjectHeapIterator objectIterator(boolean includeLiveObjects, boolean includeDeadObjects) throws CorruptDataException;
	
	/**
	 * @return true if the region is associated with a subspace with immortal memory type
	 */
	public abstract boolean isImmortal();
	
	/**
	 * @return true if the region is associated with a subspace with scoped memory type
	 */
	public abstract boolean isScoped();
	
	/**
	 * @return in a spanning region, return to the HEAD of the span;  in non-spanning region return this
	 */
	public abstract GCHeapRegionDescriptor getHeadOfSpan();
	
	public abstract String descriptionString();
	
	public String toString()
	{
		String format; 
		if(J9BuildFlags.env_data64) {
			format = "%s [0x%016X-0x%016X]";
		} else {
			format = "%s [0x%08X-0x%08X]";
		}
		return String.format(format, descriptionString(), getLowAddress().getAddress(), getHighAddress().getAddress());
	}
}
