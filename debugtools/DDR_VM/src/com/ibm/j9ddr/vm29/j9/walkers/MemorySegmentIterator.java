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
package com.ibm.j9ddr.vm29.j9.walkers;

import java.util.Iterator;
import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

import com.ibm.j9ddr.vm29.pointer.generated.J9MemorySegmentListPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MemorySegmentPointer;

@SuppressWarnings("unchecked")
public class MemorySegmentIterator implements Iterator {
	public static final int MEMORY_TYPE_RAM_CLASS = 0x10000;
	public static final int MEMORY_ALL_TYPES = 0xFFFFFFFF;
	protected J9MemorySegmentPointer segment;		//the segment which will be returned when next() is called
	private J9MemorySegmentPointer segmentPtr;		//segment pointer for moving over the segments
	protected final int flags;						//bits used to identify the type of memory segment
	protected final boolean useClassloaderSegments;
	private boolean hasNextSegment = false;			//flag to indicate if the walker has obtained the next segment to return
	private boolean EOS = false;					//flag to indicate if the End Of Segments has been reached by the walker
	private CorruptDataException cde = null;		//a stored corrupt data exception to raise
	
	public MemorySegmentIterator(J9MemorySegmentPointer source, int flags, boolean useClassloaderSegments) {
		segmentPtr = source;
		this.flags = flags;
		this.useClassloaderSegments = useClassloaderSegments;
		EOS = segmentPtr.isNull();					//guard against being passed a null pointer
	}
	
	public MemorySegmentIterator(J9MemorySegmentListPointer source, int flags, boolean useClassloaderSegments) {
		this.flags = flags;
		this.useClassloaderSegments = useClassloaderSegments;
		try {
			segmentPtr = source.nextSegment();		//attempt to get the first segment
			EOS = segmentPtr.isNull();				//guard against being passed a null pointer
		} catch (CorruptDataException e) {
			EOS = true;		//if the first segment cannot be located then we cannot perform any iterations
			raiseCorruptDataEvent("Could not locate the first segment", e, true);
		}
	}
	
	public boolean hasNext() {
		if(hasNextSegment) return true;		//the next segment has been located and ready for retrieval
		if(EOS) {
			if(cde != null) {				//see if there is a saved CDE to raise
				raiseCorruptDataEvent("Could not locate the next segment", cde, true);
			}
			return false;				//reached end of segments
		}
		try {								
			while(!hasNextSegment && !EOS) {
				if(segmentPtr.type().anyBitsIn(flags)) {	//need to locate the next segment of the correct type
					hasNextSegment = true;				//found a valid segment
					segment = segmentPtr;
				}
				if(useClassloaderSegments) {			//get the next segment
					segmentPtr = segmentPtr.nextSegmentInClassLoader();
				} else {
					segmentPtr = segmentPtr.nextSegment();
				}
				if(segmentPtr.isNull()) {			//EOS has been reached
					EOS = true;						//no more segments to walk
				}
			}
			return hasNextSegment;
		} catch (CorruptDataException e) {
			EOS = true;					//abort segment iterations if a CDE occurs
			if(hasNextSegment) {
				cde = e;				//store the CDE to raise after the current segment has been returned
			} else {
				raiseCorruptDataEvent("Could not locate the next segment", e, true);
			}
			return hasNextSegment;		//we may have found a segment before the CDE
		}
	}

	public Object next() {
		if(hasNext()) {
			hasNextSegment = false;		//indicate that we've given out the segment
			return segment;
		} else {
			throw new NoSuchElementException();
		}
	}

	public void remove() {
		throw new UnsupportedOperationException("This iterator is read only");
	}
}
