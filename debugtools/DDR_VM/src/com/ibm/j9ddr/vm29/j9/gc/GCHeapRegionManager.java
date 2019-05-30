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

import java.util.Arrays;
import java.util.Iterator;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapRegionDescriptorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapRegionManagerPointer;
import com.ibm.j9ddr.vm29.types.UDATA;

public class GCHeapRegionManager
{
	protected static GCHeapRegionManager singleton;
	
	protected MM_HeapRegionManagerPointer _heapRegionManager;
	
	GCHeapRegionDescriptor[] _auxRegionDescriptorList; /**< address ordered doubly linked list for auxiliary heap regions */
	protected int _auxRegionCount; /**< number of heap regions on the auxiliary list */
	
	/* Heap Region Table data */
	protected UDATA _regionSize; /**< the size, in bytes, of a region in the _regionTable */
	protected UDATA _regionShift;  /**< the shift value to use against pointers to determine the corresponding region index */
	protected GCHeapRegionDescriptor[] _regionTable; /**< the raw array of fixed-sized regions for representing a flat heap */
	protected int _tableRegionCount; /**< number of heap regions on the fixed-sized table (_regionTable) */
	protected VoidPointer _lowTableEdge; /**< the first (lowest address) byte of heap which is addressable by the table */
	protected VoidPointer _highTableEdge; /**< the first byte AFTER the heap range which is addressable by the table */
	protected UDATA _tableDescriptorSize; /**< The size, in bytes, of the HeapRegionDescriptor subclass used by this manager */
	protected UDATA _totalHeapSize;	/**< The size, in bytes, of all currently active regions on the heap (that is, both table descriptors attached to subspaces and aux descriptors in the list) */


	/* Do not instantiate. Use the factory */
	protected GCHeapRegionManager(MM_HeapRegionManagerPointer hrm) throws CorruptDataException
	{
		_heapRegionManager = hrm;
		_auxRegionCount = hrm._auxRegionCount().intValue();
		_regionSize = hrm._regionSize();
		_regionShift = hrm._regionShift();
		_tableRegionCount = hrm._tableRegionCount().intValue();
		_lowTableEdge = hrm._lowTableEdge();
		_highTableEdge = hrm._highTableEdge();
		_tableDescriptorSize = hrm._tableDescriptorSize();
		_totalHeapSize = hrm._totalHeapSize();
		
		initializeTableRegionDescriptors();
		initializeAuxRegionDescriptors();
	}

	public static GCHeapRegionManager fromHeapRegionManager(MM_HeapRegionManagerPointer hrm) throws CorruptDataException
	{
		if(null != singleton) {
			return singleton;
		}

		singleton = new GCHeapRegionManager(hrm);
		return singleton;
	}
	
	protected void initializeAuxRegionDescriptors() throws CorruptDataException
	{
		GCHeapRegionDescriptor[] auxRegions  = new GCHeapRegionDescriptor[_auxRegionCount];

		if(auxRegions.length > 0) {
			MM_HeapRegionDescriptorPointer current = _heapRegionManager._auxRegionDescriptorList();
			for(int i = 0; i < _auxRegionCount; i++) {
				auxRegions[i] = GCHeapRegionDescriptor.fromHeapRegionDescriptor(current);
				current = current._nextRegion();
			}
		}
		_auxRegionDescriptorList = auxRegions;
	}
	
	protected void initializeTableRegionDescriptors() throws CorruptDataException
	{
		GCHeapRegionDescriptor[] table  = new GCHeapRegionDescriptor[_tableRegionCount];

		if(table.length > 0) {
			MM_HeapRegionDescriptorPointer current = _heapRegionManager._regionTable();
			for(int i = 0; i < _tableRegionCount; i++) {
				table[i] = GCHeapRegionDescriptor.fromHeapRegionDescriptor(current);
				current = current.addOffset(_tableDescriptorSize);
			}
		}
		_regionTable = table;
	}
	
	
	/********************************************************************************/
	
	public UDATA getTotalHeapSize()
	{
		return _totalHeapSize;
	}
	
	public UDATA getRegionSize()
	{
		return _regionSize;
	}
	
	public int getTableRegionCount()
	{
		return _tableRegionCount;
	}
	
	public UDATA getHeapSize()
	{
		return new UDATA(_highTableEdge.sub(_lowTableEdge));
	}
	
	public Iterator<GCHeapRegionDescriptor> getAuxiliaryRegions() throws CorruptDataException
	{
		return Arrays.asList(_auxRegionDescriptorList).iterator();
	}

	public Iterator<GCHeapRegionDescriptor> getTableRegions() throws CorruptDataException
	{
		return Arrays.asList(_regionTable).iterator();
	}
		
	public GCHeapRegionDescriptor regionDescriptorForAddress(AbstractPointer heapAddress)
	{
		GCHeapRegionDescriptor result = null;
		if(heapAddress.gte(_lowTableEdge) && heapAddress.lt(_highTableEdge)) {
			result = tableDescriptorForAddress(heapAddress);
		} else {
			result = auxiliaryDescriptorForAddress(heapAddress);
		}
		return result;
	}
	
	public GCHeapRegionDescriptor auxiliaryDescriptorForAddress(AbstractPointer heapAddress)
	{
		for(int i = 0; i < _auxRegionCount; i++) {
			GCHeapRegionDescriptor region = _auxRegionDescriptorList[i];
			if(region.isAddressInRegion(heapAddress)) {
				return region;
			}
		}
		return null;
	}

	public GCHeapRegionDescriptor tableDescriptorForIndex(int regionIndex) 
	{
		return _regionTable[regionIndex].getHeadOfSpan();
	}
	
	public GCHeapRegionDescriptor physicalTableDescriptorForIndex(int regionIndex) 
	{
		return _regionTable[regionIndex];
	}

	protected int physicalTableDescriptorIndexForAddress(AbstractPointer heapAddress)
	{
		UDATA heapDelta = UDATA.cast(heapAddress).sub(UDATA.cast(_regionTable[0].getLowAddress()));
		return heapDelta.rightShift(_regionShift).intValue();				
	}
	
	public GCHeapRegionDescriptor tableDescriptorForAddress(AbstractPointer heapAddress)
	{
		int index = physicalTableDescriptorIndexForAddress(heapAddress);
		return tableDescriptorForIndex(index); 
	}
}
