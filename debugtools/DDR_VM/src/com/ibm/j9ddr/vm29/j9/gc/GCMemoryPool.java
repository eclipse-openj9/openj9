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

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.events.IEventListener;
import com.ibm.j9ddr.vm29.events.EventManager;
import com.ibm.j9ddr.vm29.j9.ObjectModel;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_GCExtensionsPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapLinkedFreeHeaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_MemoryPoolPointer;
import com.ibm.j9ddr.vm29.types.UDATA;

public abstract class GCMemoryPool {
	public static enum MemoryPoolType {
		AOL, BUMP, SAOL, SEGREGATED, NULL // MPLO gets broken down into AOL or SAOL 
	};
	
	protected String _poolName = null;
	protected MemoryPoolType _memoryPoolType = MemoryPoolType.NULL;
	protected GCHeapRegionDescriptor _region = null;
	protected MM_MemoryPoolPointer _memoryPool = null;
	
	public GCMemoryPool(GCHeapRegionDescriptor region, MM_MemoryPoolPointer memoryPool) throws CorruptDataException
	{
		_poolName = memoryPool._poolName().getCStringAtOffset(0);
		_memoryPool = memoryPool;
		_region = region;
	}
	
	public static GCMemoryPool fromMemoryPoolPointerInRegion(GCHeapRegionDescriptor region, MM_MemoryPoolPointer memoryPool) throws CorruptDataException
	{
		MM_GCExtensionsPointer gcExtensions = GCBase.getExtensions();
		boolean splitFreeListsEnabled = gcExtensions.splitFreeListSplitAmount().gt(1);
		boolean isConcurrentSweepEnabled = gcExtensions.concurrentSweep();


		if (GCExtensions.isSegregatedHeap()) {
			/* Segregated heap may be used with a standard collector.  This check must be done first. */
			return new GCMemoryPoolAggregatedCellList(region, memoryPool);
		} else if (GCExtensions.isStandardGC()) {
			String poolName = memoryPool._poolName().getCStringAtOffset(0);
			if (poolName.equals("Allocate/Survivor1") || poolName.equals("Allocate/Survivor2")) {
				return new GCMemoryPoolAddressOrderedList(region, memoryPool);
			}
			/* This can be done without comparing poolName to the names but for consistency sake, compare them */
			if (poolName.equals("Unknown") || poolName.equals("LOA") || poolName.equals("Tenure")) {
				if (splitFreeListsEnabled) {
					if (isConcurrentSweepEnabled) {
						return new GCMemoryPoolAddressOrderedList(region, memoryPool);
					} else {
						return new GCMemoryPoolSplitAddressOrderedList(region, memoryPool);
					}
				} else {
					return new GCMemoryPoolAddressOrderedList(region, memoryPool);
				}
			}
			throw new CorruptDataException("Unreachable");
		} else if (GCExtensions.isVLHGC()) {
			throw new UnsupportedOperationException("Balanced GC not supported");
		}
		throw new CorruptDataException("Unreachable");
	}
	
	/* Memory Pool functions */
	public MemoryPoolType getType()
	{
		return _memoryPoolType;
	}
	
	public String getPoolName()
	{
		return _poolName;
	}
	
	public GCHeapRegionDescriptor getRegion()
	{
		return _region;
	}
	
	public MM_MemoryPoolPointer getMemoryPoolPointer()
	{
		return _memoryPool;
	}
	
	/**
	 * Check individual entries. This check is the same for ALL collectors
	 * @param freeListEntry A structure to a free entry for all collectors (except Balanced)
	 * @throws CorruptFreeEntryException This needs to be caught by a listener to be of any use
	 * @return None. This function throws exceptions which are caught by the listener in checkFreeLists()
	 */
	protected void freeEntryCheckGeneric(GCHeapLinkedFreeHeader freeListEntry) throws CorruptDataException, CorruptFreeEntryException
	{
		MM_HeapLinkedFreeHeaderPointer freeListEntryAddress = freeListEntry.getHeader();
		UDATA size = freeListEntry.getSize();
		GCHeapRegionDescriptor region = getRegion();
		U8Pointer entryEndingAddressInclusive = U8Pointer.cast(freeListEntry.getHeader()).add(size.sub(1));
		
		if (freeListEntry.getHeader().isNull()) {
			throw new CorruptFreeEntryException("freeEntryCorrupt", freeListEntryAddress);
		}
		
		/* Make sure its a multi-slot dead object */
		if (!(ObjectModel.isDeadObject(freeListEntry.getObject()) && !ObjectModel.isSingleSlotDeadObject(freeListEntry.getObject()))) {
			throw new CorruptFreeEntryException("freeEntryCorrupt", freeListEntryAddress);
		}
		
		/* Self checks */
		if (!region.isAddressInRegion(VoidPointer.cast(freeListEntryAddress))) {
			throw new CorruptFreeEntryException("freeEntryCorrupt", freeListEntryAddress);
		}
		
		if (!region.isAddressInRegion(VoidPointer.cast(entryEndingAddressInclusive))) {
			throw new CorruptFreeEntryException("sizeFieldInvalid", freeListEntryAddress);
		}
	}
	
	/* A wrapper that registers the listener with the Event Manager */
	public void checkFreeLists(IEventListener listener)
	{
		EventManager.register(listener);
		checkFreeListsImpl();
		EventManager.unregister(listener);
	}
	
	/**
	 * This iterates through the pool and calls the appropriate free entry validation functions
	 * @param listener Is the listener that will listen for the corrupt free entry events.
	 * @throws CorruptDataException
	 */
	void checkFreeListsImpl()
	{
		return;
	}
	
	@Override
	public String toString()
	{
		return String.format("Pool name: %s Pool Address: %s Region: %s", getPoolName(), _memoryPool.getHexAddress(), _region.getHeapRegionDescriptorPointer().getHexAddress());
		
	}
}
