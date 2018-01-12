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

import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapRegionDescriptorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapRegionManagerPointer;

public class GCHeapSpanRegionIterator extends GCIterator
{
	private MM_HeapRegionDescriptorPointer _currentRegion;
	private long _tableDescriptorSize; 
	private long _regionsLeft;
	
	/* Do not instantiate. Use the factory */	
	protected GCHeapSpanRegionIterator(MM_HeapRegionManagerPointer manager, MM_HeapRegionDescriptorPointer region) throws CorruptDataException 
	{
		_currentRegion = region;
		_regionsLeft = region._regionsInSpan().longValue();
		_tableDescriptorSize = manager._tableDescriptorSize().longValue();
		
		if(0 == _regionsLeft) {
			_regionsLeft = 1;
		}
	}
	
	/**
	 * Factory method to construct an appropriate segment iterator.
	 * 
	 * @param manager the MM_HeapRegionManagerPointer to iterate
	 * @param region the MM_HeapRegionDescriptorPointer representing the spanning region 
	 * 
	 * @return an instance of GCHeapSpanRegionIterator 
	 * @throws CorruptDataException 
	 */
	public static GCHeapSpanRegionIterator fromMMHeapRegionDescriptor(MM_HeapRegionManagerPointer manager, MM_HeapRegionDescriptorPointer region) throws CorruptDataException
	{
		return new GCHeapSpanRegionIterator(manager, region);
	}
	
	public boolean hasNext()
	{
		return _regionsLeft > 0;
	}

	public MM_HeapRegionDescriptorPointer next()
	{
		if (_regionsLeft > 0) {
			MM_HeapRegionDescriptorPointer region = _currentRegion;
			_regionsLeft--;
			_currentRegion = _currentRegion.addOffset(_tableDescriptorSize);
			return region; 
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
