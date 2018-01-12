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

public class GCModronAllocateHintIteratorAOL extends GCModronAllocateHintIterator {
	protected J9ModronAllocateHintPointer _currentHint = null;
	protected GCMemoryPoolAddressOrderedList _memoryPool = null;
	
	public GCModronAllocateHintIteratorAOL(GCMemoryPoolAddressOrderedList memoryPool) throws CorruptDataException
	{
		_memoryPool = memoryPool;
		_currentHint = memoryPool.getFirstHint();
		if (_currentHint.isNull()) {
			_currentHint = null;
		}
	}
	
	private void advanceHint()
	{
		try {
			_currentHint = _currentHint.next();
			if (_currentHint.isNull()) {
				_currentHint = null;
			}
		} catch (CorruptDataException e) {
			raiseCorruptDataEvent("Freelist corruption detected", e, false);
			_currentHint = null;
		}
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
	public int getFreeListNumber()
	{
		return 0;
	}
	
	@Override
	public String toString()
	{
		/* Replicate this to prevent side-effects in toString */
		GCModronAllocateHintIteratorAOL tempIter = null;
		try {
			tempIter = new GCModronAllocateHintIteratorAOL(_memoryPool);
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
