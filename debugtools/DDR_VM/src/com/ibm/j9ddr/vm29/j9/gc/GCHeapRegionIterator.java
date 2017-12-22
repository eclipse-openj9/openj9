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
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_GCExtensionsPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapRegionDescriptorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapRegionManagerPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_MemorySpacePointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_MemorySubSpacePointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;

public class GCHeapRegionIterator extends GCIterator
{
	private MM_MemorySpacePointer _space; /**< The MemorySpace to which all regions returned by the iterator must belong (can be NULL if this check is not needed) */
	private MM_HeapRegionDescriptorPointer _auxRegion; /**< The auxiliary region we will process next */
	private MM_HeapRegionDescriptorPointer _tableRegion; /**< The table region we will process next */
	private MM_HeapRegionManagerPointer _regionManager; /**< The HeapRegionManager from which this iterator requests its regions */
	private long _tableDescriptorSize;
	private GCHeapRegionDescriptor currentRegion;
//	private long _tableRegionTop;
	
	/* Do not instantiate. Use the factory */	
	protected GCHeapRegionIterator(MM_HeapRegionManagerPointer manager, MM_MemorySpacePointer space, boolean includeTableRegions, boolean includeAuxRegions) throws CorruptDataException 
	{
		_regionManager = manager;
		_space = space;
		if (includeAuxRegions) {
			_auxRegion = _regionManager._auxRegionDescriptorList();
			if(_auxRegion.isNull()) {
				_auxRegion = null;
			}
		}
		if (includeTableRegions) {
			_tableRegion = _regionManager._regionTable();
			if(_tableRegion.isNull()) {
				_tableRegion = null;
			}
			_tableDescriptorSize = _regionManager._tableDescriptorSize().longValue();
//			_tableRegionTop = _regionManager._regionTable().longValue() + (_tableDescriptorSize * _regionManager._tableRegionCount().longValue());
		}
	}
	
	/**
	 * Factory method to construct an appropriate segment iterator.
	 * 
	 * @param manager the MM_HeapRegionManagerPointer to iterate
	 * @param space only iterate regions which belong to the specified memory space
	 * @param includeTableRegions include table regions in the iteration
	 * @param includeAuxRegions include auxiliary regions in the iteration
	 * 
	 * @return an instance of GCHeapRegionIterator 
	 * @throws CorruptDataException 
	 */
	public static GCHeapRegionIterator fromMMHeapRegionManager(MM_HeapRegionManagerPointer manager, MM_MemorySpacePointer space, boolean includeTableRegions, boolean includeAuxRegions) throws CorruptDataException
	{
		return new GCHeapRegionIterator(manager, space, includeAuxRegions, includeTableRegions);
	}
	
	public static GCHeapRegionIterator from() throws CorruptDataException
	{
		J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
		MM_GCExtensionsPointer gcext = GCExtensions.getGCExtensionsPointer();
		MM_HeapRegionManagerPointer hrm = gcext.heapRegionManager();
		return fromMMHeapRegionManager(hrm, true, true);
	}

	
	/**
	 * Factory method to construct an appropriate segment iterator.
	 * 
	 * @param manager the MM_HeapRegionManagerPointer to iterate
	 * @param space only iterate regions which belong to the specified memory space
	 * 
	 * @return an instance of GCHeapRegionIterator 
	 * @throws CorruptDataException 
	 */
	public static GCHeapRegionIterator fromMMHeapRegionManager(MM_HeapRegionManagerPointer manager, MM_MemorySpacePointer space) throws CorruptDataException
	{
		return new GCHeapRegionIterator(manager, space, true, true);
	}
	
	/**
	 * Factory method to construct an appropriate segment iterator.
	 * 
	 * @param manager the MM_HeapRegionManagerPointer to iterate
	 * @param includeTableRegions include table regions in the iteration
	 * @param includeAuxRegions include auxiliary regions in the iteration
	 * 
	 * @return an instance of GCHeapRegionIterator 
	 * @throws CorruptDataException 
	 */
	public static GCHeapRegionIterator fromMMHeapRegionManager(MM_HeapRegionManagerPointer manager, boolean includeTableRegions, boolean includeAuxRegions) throws CorruptDataException
	{
		return new GCHeapRegionIterator(manager, null, includeTableRegions, includeAuxRegions);
	}
	
	/**
	 * Determine if the specified region should be included or skipped.
	 * @return true if the region should be included, false otherwise
	 * @throws CorruptDataException 
	 */
	protected boolean shouldIncludeRegion(MM_HeapRegionDescriptorPointer region) throws CorruptDataException
	{
		if (null != _space) {
			MM_MemorySubSpacePointer subspace = region._memorySubSpace();
			if (subspace.notNull()) {
				return _space.eq(subspace._memorySpace());
			} else {
				return false;
			}
		} else {
			return true;
		}
	}
	
	protected MM_HeapRegionDescriptorPointer getNextAuxiliaryRegion(MM_HeapRegionDescriptorPointer heapRegion) throws CorruptDataException
	{
		MM_HeapRegionDescriptorPointer next = heapRegion._nextRegion();
		if(next.isNull()) {
			return null;
		}
		return next;
	}

	protected MM_HeapRegionDescriptorPointer getNextTableRegion(MM_HeapRegionDescriptorPointer heapRegion) throws CorruptDataException
	{
		MM_HeapRegionDescriptorPointer next = heapRegion.addOffset(_tableDescriptorSize * heapRegion._regionsInSpan().longValue());
		if(next.isNull()) {
			return null;
		}
		
		MM_HeapRegionDescriptorPointer top = MM_HeapRegionDescriptorPointer.cast(_regionManager._regionTable().longValue() + (_tableDescriptorSize * _regionManager._tableRegionCount().longValue()));
		MM_HeapRegionDescriptorPointer usedRegion = MM_HeapRegionDescriptorPointer.NULL;
		MM_HeapRegionDescriptorPointer current = next;
		while(usedRegion.isNull() && current.lt(top)) {
			if(current._isAllocated()) {
				usedRegion = current;
			} else {
				current = current.addOffset(_tableDescriptorSize * current._regionsInSpan().longValue());
			}
		}
		if(usedRegion.isNull()) {
			return null;
		}
		return usedRegion;
	}
	
	public boolean hasNext()
	{
		try {
			if(null != currentRegion) {
				return true;
			}
			
			while ((null != _auxRegion) || (null != _tableRegion)) {
				MM_HeapRegionDescriptorPointer region = null;
				
				/* we need to return these in-order */
				if ((null != _auxRegion) && ((null == _tableRegion) || (_auxRegion.lt(_tableRegion)))) {
					region = _auxRegion;
					_auxRegion = getNextAuxiliaryRegion(_auxRegion);
				} else if (null != _tableRegion) {
					region = _tableRegion;
					_tableRegion = getNextTableRegion(_tableRegion);
				} else {
					break;
				}
				if (shouldIncludeRegion(region)) {
					currentRegion = GCHeapRegionDescriptor.fromHeapRegionDescriptor(region);
					return true;
				}
			}
			return false;
		} catch(CorruptDataException cde) {
			raiseCorruptDataEvent("Error getting next item", cde, false);		//can try to recover from this
			return false;
		}
	}

	public GCHeapRegionDescriptor next()
	{
		if(hasNext()) {
			GCHeapRegionDescriptor next = currentRegion;
			currentRegion = null;
			return next;
		} else {
			throw new NoSuchElementException("There are no more items available through this iterator");
		}
	}

	public VoidPointer nextAddress() 
	{
		// Does not make sense in this case
		throw new UnsupportedOperationException("This iterator cannot return addresses");
	}
}
