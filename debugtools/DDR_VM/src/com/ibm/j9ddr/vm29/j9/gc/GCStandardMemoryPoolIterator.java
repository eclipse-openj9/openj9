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

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.generated.MM_MemoryPoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_MemorySubSpaceGenericPointer;

class GCStandardMemoryPoolIterator extends GCMemoryPoolIterator {
	enum IteratorState {
		mm_heapmp_iterator_next_region, /* Get to next subspace */
		mm_heapmp_iterator_next_memory_pool, /* Get to next pool */
		NULL
	}
	
	protected MM_MemoryPoolPointer _currentMemoryPool = null;
	protected GCHeapRegionIterator _regionIterator = null;
	protected GCHeapRegionDescriptor _region = null;
	
	private IteratorState _state = IteratorState.NULL;
	
	public GCStandardMemoryPoolIterator() throws CorruptDataException
	{
		_state = IteratorState.mm_heapmp_iterator_next_region;
		_regionIterator = GCHeapRegionIterator.from();
		advancePool();
	}
	
	/* Analog to the HeapMemoryPoolIterator.cpp:nextPool() */
	private void advancePool()
	{
		try {
			boolean poolFound = false;
			MM_MemoryPoolPointer memoryPool = null;
			
			while (!poolFound && _regionIterator.hasNext()) {
				
				switch (_state) {
					case mm_heapmp_iterator_next_region:
						_region = _regionIterator.next();
						/* Based on current Modron architecture, the leafs of the sub spaces in standard collectors
						 * are of type SubSpaceGeneric. This might change in the future */
						MM_MemorySubSpaceGenericPointer subSpaceGeneric = MM_MemorySubSpaceGenericPointer.cast(_region.getSubSpace());
						
						if (subSpaceGeneric.notNull()) {
							memoryPool = subSpaceGeneric._memoryPool();
							
							if (memoryPool.notNull()) {
								/* Does this Memory pool have children ? */
								if (memoryPool._children().notNull()) {
									/* Yes ..So we only return details of its children */
									memoryPool = memoryPool._children();
								}
								_state = IteratorState.mm_heapmp_iterator_next_memory_pool;
								poolFound = true;
							}
						}
						break;
					
					case mm_heapmp_iterator_next_memory_pool:
						/* Any more children ? */
						memoryPool = _currentMemoryPool._next();
						if (memoryPool.isNull()) {
							_state = IteratorState.mm_heapmp_iterator_next_region;
						} else {
							poolFound = true;
						}
						break;
				}
			}
			
			if (poolFound) {
				/* Set currentMemoryPool to null to indicate we can't find any more pools */
				_currentMemoryPool = memoryPool;
			} else {
				_currentMemoryPool = null;
			}
		} catch (CorruptDataException e) {
			raiseCorruptDataEvent("Memory Pool corruption detected", e, false);
			_currentMemoryPool = null;
		}
	}
	
	public GCMemoryPool next()
	{
		GCMemoryPool next = null;
		
		if (hasNext()) {
			try {
				next = GCMemoryPool.fromMemoryPoolPointerInRegion(_region, _currentMemoryPool);
			} catch (CorruptDataException e) {
				raiseCorruptDataEvent("Memory Pool corruption detected", e, false);
			}
			advancePool();
			return next;
		}
		
		throw new NoSuchElementException("There are no more items available through this iterator");
	}
	
	public boolean hasNext()
	{
		return null != _currentMemoryPool;
	}
	
	@Override
	public String toString()
	{
		/* Replicate this to prevent side-effects in toString */
		GCStandardMemoryPoolIterator tempIter = null;
		try {
			tempIter = new GCStandardMemoryPoolIterator();
		} catch (CorruptDataException e) {
			raiseCorruptDataEvent("Corruption detected", e, false);
			return e.toString();
		}
		
		StringBuilder builder = new StringBuilder();
		String NEW_LINE = System.getProperty("line.separator");
		
		while (tempIter.hasNext()) {
			builder.append(tempIter.next() + NEW_LINE);
		}
		return builder.toString();
	}
}
