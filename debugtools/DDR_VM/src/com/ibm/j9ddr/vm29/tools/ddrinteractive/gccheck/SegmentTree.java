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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.gccheck;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.events.EventManager;
import com.ibm.j9ddr.vm29.j9.gc.GCSegmentIterator;
import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MemorySegmentListPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MemorySegmentPointer;

class SegmentTree
{
	private J9MemorySegmentPointer[] cache;
	
	public SegmentTree(J9MemorySegmentListPointer segmentList)
	{
		setSegmentList(segmentList);
	}
	
	protected void setSegmentList(J9MemorySegmentListPointer segmentList)
	{
		ArrayList<J9MemorySegmentPointer> segments = new ArrayList<J9MemorySegmentPointer>();
		
		try {
			// Batch up the entries
			GCSegmentIterator iterator = GCSegmentIterator.fromJ9MemorySegmentList(segmentList, 0);
			while (iterator.hasNext()) {
				segments.add(iterator.next());			
			}
		} catch (CorruptDataException e) {
			EventManager.raiseCorruptDataEvent("Corrupted segment in list", e, false);
			cache = new J9MemorySegmentPointer[0];
		}
		
		// And sort them
		Collections.sort(segments, new Comparator<J9MemorySegmentPointer>() {
			public int compare(J9MemorySegmentPointer seg1, J9MemorySegmentPointer seg2)
			{
				try {
					return seg1.eq(seg2) ? 0 : seg1.heapBase().lt(seg2.heapBase()) ? -1 : 1;
				} catch (CorruptDataException e) {
					EventManager.raiseCorruptDataEvent("Corrupted segment in list", e, false);
					return -1;
				}
			}});
		
		cache = segments.toArray(new J9MemorySegmentPointer[segments.size()]);
	}
	
	public J9MemorySegmentPointer findSegment(AbstractPointer key)
	{
		try {
			return findSegment(0, cache.length, key);
		} catch (CorruptDataException e) {
			// Fall back to linear
			return linearSearch(0, cache.length, key);
		}
	}
	
	private J9MemorySegmentPointer findSegment(int start, int end, AbstractPointer key) throws CorruptDataException
	{
		if((end - start) < 4) {
			// Linear search small ranges
			return linearSearch(start, end, key);
		} else {
			// Binary search
			int mid = (start + end) / 2;
			J9MemorySegmentPointer segment = cache[mid];
			if(key.gte(segment.heapBase())) {
				if(key.lt(segment.heapAlloc())) {
					return segment;
				} else {
					return findSegment(mid, end, key);
				}
			} else {
				return findSegment(start, mid, key);
			}	
		}
	}

	private J9MemorySegmentPointer linearSearch(int start, int end, AbstractPointer key)
	{
		for(int i = start; i < end; i++) {
			if(cache[i] != null) {
				if(includesKey(cache[i], key)) {
					return cache[i];
				}
			}
		}
		return null;
	}

	private boolean includesKey(J9MemorySegmentPointer segment, AbstractPointer key)
	{
		try {
			return key.gte(segment.heapBase()) && key.lt(segment.heapAlloc());
		} catch (CorruptDataException e) {
			EventManager.raiseCorruptDataEvent("Corrupted segment in list", e, false);
			return false;
		}
	}
}
