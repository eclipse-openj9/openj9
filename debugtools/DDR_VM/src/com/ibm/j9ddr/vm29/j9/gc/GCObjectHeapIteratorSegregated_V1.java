/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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

import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.ObjectModel;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMGCSegregatedAllocationCacheEntryPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.types.UDATA;
import com.ibm.j9ddr.vm29.structure.MM_HeapRegionDescriptor$RegionType;



class GCObjectHeapIteratorSegregated_V1 extends GCObjectHeapIterator
{
	protected J9ObjectPointer currentObject;
	protected U8Pointer scanPtr;
	protected U8Pointer scanPtrTop;
	protected J9ObjectPointer smallPtrTop;
	protected long type;
	protected UDATA cellSize;
	protected U8Pointer[][] allocationCacheRanges;
	protected int currentAllocationCacheRange;
	
	protected static ArrayList<U8Pointer[]> threadAllocationCacheRanges;
	protected static Comparator<U8Pointer[]> rangeSorter = new Comparator<U8Pointer[]>()
	{
		public int compare(U8Pointer[] o1, U8Pointer[] o2)
		{
			return o1[0].compare(o2[0]);
		}
	};
	
	static {
		// Collect the set of active allocation cache ranges once
		try {
			ArrayList<U8Pointer[]> ranges = new ArrayList<U8Pointer[]>();
			GCVMThreadListIterator threadIterator = new GCVMThreadListIterator();
			while (threadIterator.hasNext()) {
				J9VMThreadPointer vmThread = threadIterator.next();
				
				/* Big hack in order to get this code to promote through.  Problem is that we won't see 
				 * J9Consts.J9VMGC_SIZECLASSES_NUM_SMALL until vm promotes.  But iBuild won't promote until 
				 * this change is in. */
				long smallSizeClassesNumVersion1 = 0;
				long smallSizeClassesNumVersion2 = 0;
				
				/* TODO: use DataType.getStructurePackageName() instead of "com.ibm.j9ddr.vm29.structure" */
				try {
					Class<?> j9vmgcSizeClass = Class.forName(DataType.getStructurePackageName() + "." + "J9VMGCSizeClasses");
					Field fieldVersion1 = j9vmgcSizeClass.getDeclaredField("J9VMGC_SIZECLASSES_NUM_SMALL");
					smallSizeClassesNumVersion1 = fieldVersion1.getLong(null);
				} catch (NoSuchFieldException noSuchFieldException) {
					/* Do nothing hope we can find the field in the other class */
				} catch (ClassNotFoundException classNotFoundException) {
					/* Do nothing, hope we can find the other class */
				} catch (IllegalAccessException illegalAccessException) {
					throw new UnsupportedOperationException("This shouldn't happen considering how DDR works");
				}
				
				try {
					Class<?> j9ConstsClass = Class.forName(DataType.getStructurePackageName() + "." + "J9Consts");
					Field sizeClassNumfield2 = j9ConstsClass.getDeclaredField("J9VMGC_SIZECLASSES_NUM_SMALL");
					smallSizeClassesNumVersion2 = sizeClassNumfield2.getLong(null);
				} catch (NoSuchFieldException noSuchFieldException) {
					/* Do nothing hope we can find the field in the other class */
				} catch (ClassNotFoundException classNotFoundException) {
					/* Do nothing, hope we can find the other class */
				} catch (IllegalAccessException illegalAccessException) {
					throw new UnsupportedOperationException("This shouldn't happen considering how DDR works");
				}
				
				if ((smallSizeClassesNumVersion1 == 0) && (smallSizeClassesNumVersion2 == 0)) {
					throw new UnsupportedOperationException("This cores seems invalid.. J9VMGC_SIZECLASSES_NUM_SMALL must be defined in either J9Consts or J9VMGCSizeClasses");
				}
				
				if ((smallSizeClassesNumVersion1 != 0) && (smallSizeClassesNumVersion2 != 0)) {
					throw new UnsupportedOperationException("This cores seems invalid.. J9VMGC_SIZECLASSES_NUM_SMALL can't be defined in two places");
				}
				
				long smallSizeClassesNum = (smallSizeClassesNumVersion1 != 0) ? smallSizeClassesNumVersion1 : smallSizeClassesNumVersion2;   

				for (int sizeClass = 0; sizeClass < smallSizeClassesNum+1; sizeClass++) {
					J9VMGCSegregatedAllocationCacheEntryPointer allocationCache = vmThread.segregatedAllocationCache();
					allocationCache = allocationCache.add(sizeClass);
					U8Pointer heapCurrent = U8Pointer.cast(allocationCache.current());
					U8Pointer heapTop = U8Pointer.cast(allocationCache.top());
					if(heapCurrent.lt(heapTop)) {
						ranges.add(new U8Pointer[] {heapCurrent, heapTop});
					}
				}
			}
	
			Collections.sort(ranges, rangeSorter);
			threadAllocationCacheRanges = ranges;
		} catch(CorruptDataException e) {
			raiseCorruptDataEvent("Error calculating active allocation cache ranges", e, true);
		}
	}
	
	protected GCObjectHeapIteratorSegregated_V1(U8Pointer base, U8Pointer top, long type, UDATA cellSize, boolean includeLiveObjects, boolean includeDeadObjects) throws CorruptDataException
	{
		super(includeLiveObjects, includeDeadObjects);
		
		currentObject = null;
		scanPtr = base;
		scanPtrTop = top;
		this.type = type;
		this.cellSize = cellSize;
		currentAllocationCacheRange = 0;
		calculateActualScanPtrTop();
		
		// Add the requested allocation cache range to the active ranges
		ArrayList<U8Pointer[]> allocationCacheRangeList = new ArrayList<U8Pointer[]>(threadAllocationCacheRanges);
		allocationCacheRangeList.add(new U8Pointer[] {scanPtrTop, scanPtrTop});
		Collections.sort(allocationCacheRangeList, rangeSorter);
		allocationCacheRanges = new U8Pointer[allocationCacheRangeList.size()][2];
		allocationCacheRangeList.toArray(allocationCacheRanges);
	}

	public boolean hasNext()
	{
		try {
			if (null != currentObject) {
				return true;
			}
			
			while (scanPtr.lt(scanPtrTop)) {

				while (scanPtr.gt(allocationCacheRanges[currentAllocationCacheRange][1])) {
					currentAllocationCacheRange++;
				}
				if (scanPtr.gte(allocationCacheRanges[currentAllocationCacheRange][0])) {
					// We're in an unused region. 
					// TODO : this should report as a hole
					scanPtr = U8Pointer.cast(allocationCacheRanges[currentAllocationCacheRange][1]);
					currentAllocationCacheRange++;
					continue;
				}
				
				currentObject = J9ObjectPointer.cast(scanPtr);
				if (MM_HeapRegionDescriptor$RegionType.SEGREGATED_SMALL == type) {
					if (!ObjectModel.isDeadObject(currentObject)) { 
						UDATA sizeInBytes = ObjectModel.getConsumedSizeInBytesWithHeader(currentObject);
						if (sizeInBytes.gt(cellSize)) {
							/* The size of an object should never be greater than the size of its containing cell. */
							currentObject = null;
							throw new CorruptDataException("Allocated object at " + scanPtr.getHexAddress() + " has an invalid size of " + sizeInBytes.getHexValue());
						}
						scanPtr = scanPtr.add(cellSize);
						if(includeLiveObjects) {
							return true;
						}
					} else {
						UDATA sizeInBytes = ObjectModel.getSizeInBytesDeadObject(currentObject);
						if (sizeInBytes.lt(cellSize)) {
							/* The size of a hole should always be at least the size of a cell. */
							currentObject = null;
							throw new CorruptDataException("GCHeapLinkedFreeHeader at " + scanPtr.getHexAddress() + " has an invalid size of " + sizeInBytes.getHexValue());
						}
						scanPtr = scanPtr.addOffset(sizeInBytes);
						if (includeDeadObjects) {
							return true;
						}
					}
				} else if(MM_HeapRegionDescriptor$RegionType.SEGREGATED_LARGE == type) {
					scanPtr = U8Pointer.cast(scanPtrTop);
					if(includeLiveObjects) {
						return true;
					}
				}  else {
					throw new CorruptDataException("Invalid region type");
				}
			}
			return false;
		} catch (CorruptDataException e) {
			raiseCorruptDataEvent("Error getting next item", e, false);		//can try to recover from this
			return false;
		}
	}

	public void advance(UDATA size)
	{
		throw new UnsupportedOperationException("Not implemented");
	}
	
	public J9ObjectPointer next()
	{
		if (hasNext()) {
			J9ObjectPointer next = currentObject;
			currentObject = null;
			return next;
		} else {
			throw new NoSuchElementException("There are no more items available through this iterator");
		}
	}
	
	private void calculateActualScanPtrTop()
	{
		if (MM_HeapRegionDescriptor$RegionType.SEGREGATED_SMALL == type) {
			UDATA cellCount = UDATA.cast(scanPtrTop).sub(UDATA.cast(scanPtr)).div(cellSize);
			UDATA actualSize = cellCount.mult(cellSize);
			scanPtrTop = scanPtr.add(actualSize);
		}
	}

	@Override
	public J9ObjectPointer peek()
	{
		if(hasNext()) {
			return currentObject;
		} else {
			throw new NoSuchElementException("There are no more items available through this iterator");
		}		
	}
}
