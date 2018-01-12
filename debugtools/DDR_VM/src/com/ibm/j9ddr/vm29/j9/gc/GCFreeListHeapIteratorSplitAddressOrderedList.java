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
import com.ibm.j9ddr.vm29.types.UDATA;

class GCFreeListHeapIteratorSplitAddressOrderedList extends GCFreeListHeapIterator {
	protected UDATA _freeListIndex = new UDATA(0);
	protected GCMemoryPoolSplitAddressOrderedList _memoryPool = null;
	protected GCHeapLinkedFreeHeader _currentFreeEntry = null;
	protected UDATA _totalListCount = null;
	
	public GCFreeListHeapIteratorSplitAddressOrderedList(GCMemoryPoolSplitAddressOrderedList memoryPool) throws CorruptDataException
	{
		_memoryPool = memoryPool;
		_totalListCount = _memoryPool.getFreeListCount();
		advanceFreeListEntry();
	}
	
	private void advanceFreeListEntry()
	{
		try {
			// If next FLE is null, it may indicated current FLE is last in current list. Check if other lists exist.
			if (_currentFreeEntry != null) {
				if (_currentFreeEntry.getNext().getHeader().notNull()) {
					_currentFreeEntry = _currentFreeEntry.getNext();
					return;
				} else {
					_freeListIndex = _freeListIndex.add(1);
				}
			}
			
			for (; _freeListIndex.lt(_totalListCount); _freeListIndex = _freeListIndex.add(1)) {
				_currentFreeEntry = _memoryPool.getFirstEntryAtFreeListIndex(_freeListIndex);
				if (_currentFreeEntry.getHeader().notNull()) {
					return;
				}
			}
			_currentFreeEntry = null;
		} catch (CorruptDataException e) {
			raiseCorruptDataEvent("Freelist corruption detected", e, false);
			_currentFreeEntry = null;
		}
	}
	
	public boolean hasNext()
	{
		return null != _currentFreeEntry;
	}
	
	public GCHeapLinkedFreeHeader next() throws NoSuchElementException
	{
		if (hasNext()) {
			GCHeapLinkedFreeHeader next = _currentFreeEntry;
			advanceFreeListEntry();
			return next;
		} else {
			throw new NoSuchElementException("There are no more items available through this iterator");
		}
	}
	
	public int getFreeListNumber()
	{
		return _freeListIndex.intValue();
	}
	
	@Override
	public String toString()
	{
		/* Replicate this to prevent side-effects in toString */
		GCFreeListHeapIteratorSplitAddressOrderedList tempIter = null;
		try {
			tempIter = new GCFreeListHeapIteratorSplitAddressOrderedList(_memoryPool);
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
