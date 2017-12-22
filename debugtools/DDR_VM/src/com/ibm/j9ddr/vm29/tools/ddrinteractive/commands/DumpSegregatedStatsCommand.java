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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.commands;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.gc.GCExtensions;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapLinkedFreeHeader;
import com.ibm.j9ddr.vm29.pointer.StructurePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMGCSegregatedAllocationCacheEntryPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMGCSizeClassesPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_GCExtensionsPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapRegionDescriptorSegregatedPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapRegionListPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_LockingFreeHeapRegionListPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_LockingHeapRegionQueuePointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_MemoryPoolAggregatedCellListPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_RealtimeGCPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_RegionPoolSegregatedPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.structure.J9Consts;
import com.ibm.j9ddr.vm29.structure.MM_RegionPoolSegregated;
import com.ibm.j9ddr.vm29.types.UDATA;

public class DumpSegregatedStatsCommand extends Command
{
	public DumpSegregatedStatsCommand()
	{
		addCommand("dumpsegregatedstats", "", "Print segregated heap statistics, similiar to -XXgc:gcbugheap");
	}
	
	/**
	 * Based off of MM_HeapRegionQueue::getTotalRegions. Returns the number of regions.
	 * This function will calculate the number of regions differently according to the type of
	 * the actual subclass.
	 * @throws CorruptDataException 
	 */
	public long getTotalRegions(MM_HeapRegionListPointer heapRegionList) throws CorruptDataException
	{
		long count = 0;
		StructurePointer heapRegionQueue = heapRegionList.getAsRuntimeType();

		if (heapRegionQueue instanceof MM_LockingHeapRegionQueuePointer) {
			MM_LockingHeapRegionQueuePointer lockingHeapRegionQueue = (MM_LockingHeapRegionQueuePointer) heapRegionQueue;
			if (lockingHeapRegionQueue._singleRegionsOnly()) {
				count = lockingHeapRegionQueue._length().longValue();
			} else {
				MM_HeapRegionDescriptorSegregatedPointer heapRegionDescriptorSegregated = lockingHeapRegionQueue._head();

				while (heapRegionDescriptorSegregated.notNull()) {
					count += heapRegionDescriptorSegregated._regionsInSpan().longValue();
					heapRegionDescriptorSegregated = heapRegionDescriptorSegregated._next();
				}
			}
		} else if (heapRegionQueue instanceof MM_LockingFreeHeapRegionListPointer) {
			MM_LockingFreeHeapRegionListPointer lockingFreeHeapRegionQueue = (MM_LockingFreeHeapRegionListPointer) heapRegionQueue;
			MM_HeapRegionDescriptorSegregatedPointer heapRegionDescriptorSegregated = lockingFreeHeapRegionQueue._head();

			while (heapRegionDescriptorSegregated.notNull()) {
				count += heapRegionDescriptorSegregated._regionsInSpan().longValue();
				heapRegionDescriptorSegregated = heapRegionDescriptorSegregated._next();
			}

		} else {
			throw new CorruptDataException("Bad HeapRegionList type");
		}

		return count;
	}
	
	
	/**
	 * Count the number of free cells in the entire MM_HeapRegionList
	 * @throws CorruptDataException 
	 */
	public long getFreeCellCount(MM_HeapRegionListPointer heapRegionList) throws CorruptDataException
	{
		StructurePointer heapRegionQueue = heapRegionList.getAsRuntimeType();
		long freeCellCount = 0;

		if (heapRegionQueue instanceof MM_LockingHeapRegionQueuePointer) {
			MM_LockingHeapRegionQueuePointer lockingHeapRegionQueue = (MM_LockingHeapRegionQueuePointer) heapRegionQueue;
			MM_HeapRegionDescriptorSegregatedPointer heapRegionDescriptorSegregated = lockingHeapRegionQueue._head();
			while (heapRegionDescriptorSegregated.notNull()) {
				freeCellCount += getFreeCellCount(heapRegionDescriptorSegregated);
				heapRegionDescriptorSegregated = heapRegionDescriptorSegregated._next();
			}
		} else if (heapRegionQueue instanceof MM_LockingFreeHeapRegionListPointer) {
			MM_LockingFreeHeapRegionListPointer lockingFreeHeapRegionQueue = (MM_LockingFreeHeapRegionListPointer) heapRegionQueue;
			MM_HeapRegionDescriptorSegregatedPointer heapRegionDescriptorSegregated = lockingFreeHeapRegionQueue._head();

			while (heapRegionDescriptorSegregated.notNull()) {
				freeCellCount += getFreeCellCount(heapRegionDescriptorSegregated);
				heapRegionDescriptorSegregated = heapRegionDescriptorSegregated._next();
			}

		} else {
			throw new CorruptDataException("Bad HeapRegionList type");
		}

		return freeCellCount;
	}
	
	/**
	 * Count the number of free cells in the MM_HeapRegionDescriptorSegregatedPointer freelist
	 * @throws CorruptDataException 
	 */
	public long
	getFreeCellCount (MM_HeapRegionDescriptorSegregatedPointer heapRegionDescriptor) throws CorruptDataException
	{
		/* TODO assumes a small region */
		MM_MemoryPoolAggregatedCellListPointer memoryPoolACL = heapRegionDescriptor._memoryPoolACL();
		GCHeapLinkedFreeHeader heapLinkedFreeHeader = GCHeapLinkedFreeHeader.fromLinkedFreeHeaderPointer(memoryPoolACL._freeListHead());
		
		/* Get the size classes */
		J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
		J9VMGCSizeClassesPointer sizeClasses = vm.realtimeSizeClasses();
		UDATA sizeClassIndex = heapRegionDescriptor._sizeClass();
		long cellSize = sizeClasses.smallCellSizesEA().at(sizeClassIndex).longValue();

		long freeCellCount = 0;
		
		while (heapLinkedFreeHeader.getHeader().notNull()) {
			freeCellCount += heapLinkedFreeHeader.getSize().longValue() / cellSize;
			heapLinkedFreeHeader = heapLinkedFreeHeader.getNext();
		}
		
		return freeCellCount;
	}

	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		if (!GCExtensions.isSegregatedHeap()) {
			out.append("Only valid for a segregated heap\n");
			return;
		}

		try {
			/* Get the region pool */
			MM_GCExtensionsPointer extensions = GCExtensions.getGCExtensionsPointer();
			MM_RealtimeGCPointer realtimeGC = MM_RealtimeGCPointer.cast(extensions._globalCollector());
			MM_RegionPoolSegregatedPointer regionPool = realtimeGC._memoryPool()._regionPool();

			/* Get the size classes */
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			J9VMGCSizeClassesPointer sizeClasses = vm.realtimeSizeClasses();

			long countTotal = 0;
			long countAvailableSmallTotal = 0;
			long countFullSmallTotal = 0;
			long darkMatterBytesTotal = 0;
			long allocCacheBytesTotal = 0;

			/* arrayOffset is the total offset into a two dimensional array.  Used to walk the 
			 * the array linearly */
			long arrayOffset = J9Consts.J9VMGC_SIZECLASSES_MIN_SMALL * MM_RegionPoolSegregated.NUM_DEFRAG_BUCKETS;

			out.append("sizeClass | full | available           | total | free cell count | dark | cache\n");
			out.append("===============================================================================\n");

			for (long sizeClassIndex = J9Consts.J9VMGC_SIZECLASSES_MIN_SMALL;
					sizeClassIndex <= J9Consts.J9VMGC_SIZECLASSES_MAX_SMALL;
					sizeClassIndex++) {

				/* Print the sizeclass */
				UDATA cellSize = sizeClasses.smallCellSizesEA().at(sizeClassIndex);
				out.format("%2d: %5d | ", sizeClassIndex, cellSize.longValue());
				
				MM_HeapRegionListPointer heapRegionQueue = MM_HeapRegionListPointer.cast(regionPool._smallFullRegionsEA().at(sizeClassIndex));					
				long countSmall = getTotalRegions(heapRegionQueue);
				countFullSmallTotal += countSmall;

				/* Print the number of full regions of this size class */
				out.format("%4d | ", countSmall);
				
				/* The number of free cells of this sizeclass */
				long freeCellCount = 0;

				for (long i = 0; i < MM_RegionPoolSegregated.NUM_DEFRAG_BUCKETS; i++) {
					long count = 0;
					MM_LockingHeapRegionQueuePointer heapRegionList = MM_LockingHeapRegionQueuePointer.cast(regionPool._smallAvailableRegionsEA().add(arrayOffset).at(0));
					for (long j = 0; j < regionPool._splitAvailableListSplitCount().longValue(); j++) {
						count += getTotalRegions(heapRegionList);
						freeCellCount += getFreeCellCount(heapRegionList);
						heapRegionList = heapRegionList.add(1);
					}

					/* increment to the next list */
					arrayOffset += 1;
					countSmall += count;
					countAvailableSmallTotal += count;
					
					/* Print the number of available regions in this defrag bucket */
					out.format("%4d ", count);
				}

				countTotal += countSmall;
				
				/* Print the total number of regions of this size class, and the number of free cells */
				out.format("| %5d | %15d |", countSmall, freeCellCount);

				/* Print the percentage of darkmatter in this region */
				long darkMatterCellCount = regionPool._darkMatterCellCountEA().at(sizeClassIndex).longValue();
				darkMatterBytesTotal += darkMatterCellCount * cellSize.longValue();
				out.format("%%%3d | ", countSmall == 0 ? 0 : darkMatterCellCount / (countSmall * cellSize.longValue()));
				
				/* Calculate the number of bytes in allocation caches */
				long allocCacheSize = 0;
				J9VMThreadPointer mainThread = vm.mainThread();

				if (mainThread.notNull()) {
					J9VMThreadPointer threadCursor = vm.mainThread();

					do {
						J9VMGCSegregatedAllocationCacheEntryPointer cache = threadCursor.segregatedAllocationCache();
						cache = cache.add(sizeClassIndex);
						allocCacheSize += cache.top().longValue() - cache.current().longValue();
						threadCursor = threadCursor.linkNext();
					} while (!threadCursor.eq(mainThread));
				}
				
				out.format("%5d\n", allocCacheSize);
				allocCacheBytesTotal += allocCacheSize;
			}
			
			long regionSize = extensions.heap()._heapRegionManager()._regionSize().longValue();
			out.format("region size %d\n", regionSize);
			
			long arrayletLeafSize = extensions._omrVM()._arrayletLeafSize().longValue();
			out.format("arraylet leaf size %d\n", arrayletLeafSize);
			
			out.format("small total (full, available) region count %d (%d, %d)\n", countTotal, countFullSmallTotal, countAvailableSmallTotal);

			long countFullArraylet = getTotalRegions(regionPool._arrayletFullRegions());
			long countAvailArraylet = getTotalRegions(regionPool._arrayletAvailableRegions());
			long countTotalArraylet = countFullArraylet + countAvailArraylet;
			countTotal += countTotalArraylet;
			out.format("arraylet total (full, available) region count %d (%d %d)\n", countTotalArraylet, countFullArraylet, countAvailArraylet);
			
			long countLarge = getTotalRegions(regionPool._largeFullRegions());
			countTotal += countLarge;
			out.format("large full region count %d\n", countLarge);

			long countFree = getTotalRegions(regionPool._singleFreeList());
			countTotal += countFree;
			out.format("free region count %d\n", countFree);
			
			long countMultiFree = getTotalRegions(regionPool._multiFreeList());
			countTotal += countMultiFree;
			out.format("multiFree region count %d\n", countMultiFree);
			
			long countCoalesce = getTotalRegions(regionPool._coalesceFreeList());
			countTotal += countCoalesce;
			out.format("coalesce region count %d\n", countCoalesce);

			long heapSize = countTotal * regionSize;

			out.format("total region count %d, total heap size %d \n", countTotal, heapSize);
			out.format("dark matter total bytes %d (%2.2f%% of heap)\n", darkMatterBytesTotal, 100.0 * darkMatterBytesTotal / heapSize);
			out.format("allocation cache total bytes %d (%2.2f%% of heap)\n", allocCacheBytesTotal, 100.0 * allocCacheBytesTotal / heapSize);

		} catch (CorruptDataException e) {
			e.printStackTrace();
		}
	}
}
