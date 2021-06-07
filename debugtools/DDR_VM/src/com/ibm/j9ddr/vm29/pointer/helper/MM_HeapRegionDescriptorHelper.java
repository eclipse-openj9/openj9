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
package com.ibm.j9ddr.vm29.pointer.helper;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapRegionDescriptorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_MemoryPoolBumpPointerPointer;
import com.ibm.j9ddr.vm29.structure.J9MemorySegment;
import com.ibm.j9ddr.vm29.structure.MM_HeapRegionDescriptor$RegionType;
import com.ibm.j9ddr.vm29.types.IDATA;
import com.ibm.j9ddr.vm29.types.UDATA;

public class MM_HeapRegionDescriptorHelper {
	
	public static VoidPointer getLowAddress(MM_HeapRegionDescriptorPointer region) throws CorruptDataException 
	{
		return region._lowAddress();
	}

	public static VoidPointer getHighAddress(MM_HeapRegionDescriptorPointer region) throws CorruptDataException 
	{
		VoidPointer result = VoidPointer.NULL;
		long regionsInSpan = region._regionsInSpan().longValue(); 
		if(regionsInSpan == 0) {
			result = region._highAddress();
		} else {
			UDATA low = UDATA.cast(region._lowAddress());
			UDATA high = UDATA.cast(region._highAddress());
			UDATA delta = high.sub(low);
			UDATA spanningSize = delta.mult((int)regionsInSpan);
			result = VoidPointer.cast(low.add(spanningSize));
		}
		return result;
	}
	
	public static UDATA getSize(MM_HeapRegionDescriptorPointer region) throws CorruptDataException
	{
		// TODO : somebody should make pointer math work properly
		IDATA delta = U8Pointer.cast(getHighAddress(region)).sub(U8Pointer.cast(getLowAddress(region)));
		if(delta.lt(0)) {
			throw new CorruptDataException("Negative sized heap region");
		}
		return new UDATA(delta);
	}
	
	public static long getTypeFlags(MM_HeapRegionDescriptorPointer region) throws CorruptDataException
	{
		return J9MemorySegment.MEMORY_TYPE_RAM | region._memorySubSpace()._memoryType().longValue();
	}
	
	public static boolean hasValidMarkMap(MM_HeapRegionDescriptorPointer region) throws CorruptDataException
	{
		boolean result = false;
		long regionType = region._regionType();
		if (
				(MM_HeapRegionDescriptor$RegionType.ADDRESS_ORDERED_MARKED == regionType) 
				|| (MM_HeapRegionDescriptor$RegionType.BUMP_ALLOCATED_MARKED == regionType)
			) {
			result = true;
		}
		return result;
	}

	public static boolean containsObjects(MM_HeapRegionDescriptorPointer region) throws CorruptDataException
	{
		boolean result = false;
		long regionType = region._regionType();
		if (
				(MM_HeapRegionDescriptor$RegionType.ADDRESS_ORDERED == regionType) 
				|| (MM_HeapRegionDescriptor$RegionType.ADDRESS_ORDERED_MARKED == regionType)
				|| (MM_HeapRegionDescriptor$RegionType.BUMP_ALLOCATED == regionType)
				|| (MM_HeapRegionDescriptor$RegionType.BUMP_ALLOCATED_MARKED == regionType)
			) {
			result = true;
		}
		return result;
	}

	public static VoidPointer getWalkableHighAddress(MM_HeapRegionDescriptorPointer region) throws CorruptDataException
	{
		VoidPointer top = null;
		long regionType = region._regionType();
		if (
				(MM_HeapRegionDescriptor$RegionType.BUMP_ALLOCATED == regionType)
				|| (MM_HeapRegionDescriptor$RegionType.BUMP_ALLOCATED_MARKED == regionType)
			) {
			top = MM_MemoryPoolBumpPointerPointer.cast(region._memoryPool())._allocatePointer();
		} else {
			top = getHighAddress(region);
		}
		return top;
	}
}
