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

class GCTarokMemoryPoolIterator extends GCMemoryPoolIterator {
	protected MM_MemoryPoolPointer _currentMemoryPool = null;
	protected GCHeapRegionIterator _regionIterator = null;
	protected GCHeapRegionDescriptor _region = null;
	
	public GCTarokMemoryPoolIterator() throws CorruptDataException
	{
		_regionIterator = GCHeapRegionIterator.from();
		advancePool();
	}
	
	private void advancePool()
	{
		_currentMemoryPool = null;
		_region = null;
		
		while (_regionIterator.hasNext()) {
			_region = _regionIterator.next();
			MM_MemoryPoolPointer tempPool = _region.getMemoryPool();
			
			if (tempPool.notNull()) {
				_currentMemoryPool = tempPool;
				break;
			}
		}
	}
	
	public GCMemoryPool next()
	{
		try {
			if (hasNext()) {
				/* We keep internal pointers as generic memory pool pointers as long as possible so we have to convert them here */
				GCMemoryPool next = GCMemoryPool.fromMemoryPoolPointerInRegion(_region, _currentMemoryPool);
				advancePool();
				return next;
			}
		} catch (CorruptDataException e) {
			raiseCorruptDataEvent("Memory Pool corruption detected", e, false);
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
		GCTarokMemoryPoolIterator tempIter = null;
		try {
			tempIter = new GCTarokMemoryPoolIterator();
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
