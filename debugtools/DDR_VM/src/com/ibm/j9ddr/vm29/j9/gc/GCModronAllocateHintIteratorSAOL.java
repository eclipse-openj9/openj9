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
import com.ibm.j9ddr.vm29.pointer.generated.J9ModronAllocateHintPointer;
import com.ibm.j9ddr.vm29.types.UDATA;

public class GCModronAllocateHintIteratorSAOL extends GCModronAllocateHintIterator {
	protected UDATA _totalListCount = null;
	protected UDATA _freeListIndex = null;
	protected J9ModronAllocateHintPointer _currentHint = null;
	protected GCMemoryPoolSplitAddressOrderedList _memoryPool = null;
	
	public GCModronAllocateHintIteratorSAOL(GCMemoryPoolSplitAddressOrderedList memoryPool) throws CorruptDataException
	{
		_freeListIndex = new UDATA(0);
		_memoryPool = memoryPool;
		_totalListCount = memoryPool.getFreeListCount();
		advanceHint();
	}
	
	private void advanceHint()
	{
		try {
			if (_currentHint != null) {
				if (_currentHint.next().notNull()) {
					_currentHint = _currentHint.next();
					return;
				} else {
					_freeListIndex = _freeListIndex.add(1);
				}
			}
			
			/* If next FLE is null, it may indicated current FLE is last in current list. Find a hint in the next list. */
			for (; _freeListIndex.lt(_totalListCount); _freeListIndex = _freeListIndex.add(1)) {
				_currentHint = _memoryPool.getFirstHintForFreeList(_freeListIndex);
				if (_currentHint.notNull()) {
					return;
				}
			}
			/* If nothing found, reset current hint */
			_currentHint = null;
		} catch (CorruptDataException e) {
			raiseCorruptDataEvent("Hint corruption detected", e, false);
			_currentHint = null;
		}
	}
	
	@Override
	public int getFreeListNumber()
	{
		return _freeListIndex.intValue();
	}
	
	@Override
	public boolean hasNext()
	{
		return null != _currentHint;
	}
	
	@Override
	public J9ModronAllocateHintPointer next()
	{
		if (hasNext()) {
			J9ModronAllocateHintPointer next = _currentHint;
			advanceHint();
			return next;
		} else {
			throw new NoSuchElementException("There are no more items available through this iterator");
		}
	}
	
	@Override
	public String toString()
	{
		/* Replicate this to prevent side-effects in toString */
		GCModronAllocateHintIteratorSAOL tempIter = null;
		try {
			tempIter = new GCModronAllocateHintIteratorSAOL(_memoryPool);
		} catch (CorruptDataException e) {
			raiseCorruptDataEvent("Corruption detected", e, false);
			return e.toString();
		}
		
		StringBuilder builder = new StringBuilder();
		String NEW_LINE = System.getProperty("line.separator");
		
		while (tempIter.hasNext()) {
			builder.append(tempIter.next().getHexAddress() + NEW_LINE);
		}
		return builder.toString();
	}
}
