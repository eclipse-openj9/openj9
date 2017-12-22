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
package com.ibm.j9ddr.vm29.j9.walkers;

import static com.ibm.j9ddr.vm29.structure.J9MemorySegment.MEMORY_TYPE_ROM_CLASS;

import java.io.PrintStream;
import java.util.Iterator;
import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.generated.J9MemorySegmentListPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MemorySegmentPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;

public class ROMClassesIterator implements Iterator<J9ROMClassPointer> {

	private J9MemorySegmentPointer nextSegment;

	protected J9ROMClassPointer nextClass;
	protected final PrintStream out;
	
	public ROMClassesIterator(PrintStream out, J9MemorySegmentListPointer segmentList) {
		this.out = out;
		if (segmentList == null) {
			nextSegment = J9MemorySegmentPointer.NULL;
		} else {
			try {
				nextSegment = segmentList.nextSegment();
			} catch (CorruptDataException e) {
				nextSegment = J9MemorySegmentPointer.NULL;
			}
		}
		nextClass = J9ROMClassPointer.NULL;
	}
	
	public boolean hasNext() {
		return (J9ROMClassPointer.NULL != getNextClass().getClassPointer());
	}

	public J9ROMClassPointer next() {
		ClassAndSegment classAndSegment = getNextClass();
		J9ROMClassPointer clazz = classAndSegment.getClassPointer();
		
		if (clazz != J9ROMClassPointer.NULL) {
			nextClass = clazz;
			nextSegment = classAndSegment.getSegmentPointer();
		} else {
			throw new NoSuchElementException();
		}
		return nextClass;
	}
	
	private ClassAndSegment getNextClass() {
		J9ROMClassPointer newNextClass = J9ROMClassPointer.NULL;
		J9MemorySegmentPointer nextSegmentPtr = nextSegment;
		
		try {
			if (!nextSegment.isNull()) {
				long newHeapPtr = 0;
				
				if (nextClass == J9ROMClassPointer.NULL) {
					newHeapPtr = nextSegmentPtr.heapBase().longValue();
				} else {
					newHeapPtr = nextClass.getAddress() + nextClass.romSize().longValue();
				}
			
				do {
					if (nextSegmentPtr.type().anyBitsIn(MEMORY_TYPE_ROM_CLASS)) {
						if (newHeapPtr < nextSegmentPtr.heapAlloc().longValue()) {
							newNextClass = J9ROMClassPointer.cast(newHeapPtr);
							try {
								if (newNextClass.romSize().eq(0)) {
									out.append("Size of ROMClass at " + newNextClass.getHexAddress() + "is invalid. Skipping to next segment.\n");
									newNextClass = J9ROMClassPointer.NULL;
								} else {
									return new ClassAndSegment(newNextClass, nextSegmentPtr);
								}
							} catch (CorruptDataException e) {
								out.append("Unable to read size of ROMClass at " + newNextClass.getHexAddress() + ". Skipping to next segment.\n");
								newNextClass = J9ROMClassPointer.NULL;
							}
						}
					}
					/* move to next segment */
					nextSegmentPtr = nextSegmentPtr.nextSegment();
					if (!nextSegmentPtr.isNull()) {
						newHeapPtr = nextSegmentPtr.heapBase().longValue();
					}
				} while (!nextSegmentPtr.isNull());
			}
		} catch (CorruptDataException e) {
			newNextClass = J9ROMClassPointer.NULL;
		}
		
		return new ClassAndSegment(newNextClass, nextSegmentPtr);
	}

	public J9MemorySegmentPointer getMemorySegmentPointer() {
		return nextSegment;
	}
	
	public void remove() {
		// not implemented
	}
	
	class ClassAndSegment {
		J9ROMClassPointer classPointer;
		J9MemorySegmentPointer segmentPointer;
		
		ClassAndSegment(J9ROMClassPointer clazz, J9MemorySegmentPointer segment) {
			this.classPointer = clazz;
			this.segmentPointer = segment;
		}
		
		public J9ROMClassPointer getClassPointer() {
			return classPointer;
		}
		
		public J9MemorySegmentPointer getSegmentPointer() {
			return segmentPointer;
		}
	}
}
