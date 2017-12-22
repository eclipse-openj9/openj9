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
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MemorySegmentListPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MemorySegmentPointer;


public class GCSegmentIterator extends GCIterator
{
	private J9MemorySegmentPointer memorySegment;
	private long flags;
	
	/* Do not instantiate. Use the factory */	
	protected GCSegmentIterator(J9MemorySegmentListPointer list, long flags) throws CorruptDataException 
	{
		memorySegment = list.nextSegment();
		this.flags = flags;
	}
	
	/**
	 * Factory method to construct an appropriate segment iterator.
	 * 
	 * @param list the J9MemorySegmentList to iterate
	 * @param flags only iterate segments that match these flags
	 * 
	 * @return an instance of GCSegmentIterator 
	 * @throws CorruptDataException 
	 */
	public static GCSegmentIterator fromJ9MemorySegmentList(J9MemorySegmentListPointer list, long flags) throws CorruptDataException
	{
		return new GCSegmentIterator(list, flags);
	}
	
	public boolean hasNext() 
	{
		try {
			while(memorySegment.notNull()) {
				if(memorySegment.type().allBitsIn(flags)) {
					return true;
				}
				memorySegment = memorySegment.nextSegment();
			}
			return false;
		} catch (CorruptDataException cde) {
			raiseCorruptDataEvent("Error checking segments", cde, true);		//cannot recover from this
			return false;
		}
	}

	public J9MemorySegmentPointer next() 
	{
		try {
			if(hasNext()) {
				J9MemorySegmentPointer currentMemorySegment = memorySegment;
				memorySegment = memorySegment.nextSegment();
				return currentMemorySegment;
			} else {
				throw new NoSuchElementException("There are no more items available through this iterator");
			}
		} catch(CorruptDataException cde) {
			raiseCorruptDataEvent("Error getting next item", cde, false);		//can try to recover from this
			return null;
		}
	}
	
	public VoidPointer nextAddress() 
	{
		// Does not make sense in this case
		throw new UnsupportedOperationException("This iterator cannot return addresses");
	}
}
