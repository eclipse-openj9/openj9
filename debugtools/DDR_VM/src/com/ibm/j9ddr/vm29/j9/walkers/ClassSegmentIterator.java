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

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

import java.util.Iterator;
import java.util.NoSuchElementException;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.logging.LoggerNames;
import com.ibm.j9ddr.vm29.j9.AlgorithmVersion;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MemorySegmentListPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MemorySegmentPointer;

@SuppressWarnings("unchecked")
public class ClassSegmentIterator implements Iterator {
	private static final AlgorithmVersion version;
	private final Logger log = Logger.getLogger(LoggerNames.LOGGER_WALKERS);
	private final Iterator segments;
	private J9MemorySegmentPointer segment;
	private boolean EOS = false;			//flag to indicate End Of Segments
	private boolean hasNextClass = false;	//flag to indicate if there is a class to retrieve
	private J9ClassPointer clazzPtr = J9ClassPointer.NULL;	//class pointer to return
	
	static {
		version = AlgorithmVersion.getVersionOf(AlgorithmVersion.POOL_VERSION);
	}
	
	/**
	 * Iterate over all segments in source, regardless of classloader.
	 * @param source
	 * @throws CorruptDataException
	 */
	public ClassSegmentIterator(J9MemorySegmentListPointer source) throws CorruptDataException {
		segments = new MemorySegmentIterator(source.nextSegment(), MemorySegmentIterator.MEMORY_TYPE_RAM_CLASS, false);
		EOS = !segments.hasNext();		//no memory segments means no classloaders
	}

	/**
	 * Iterate over all segments chained from source via J9MemorySegmentPointer.nextSegmentInClassLoader.
	 * @param source
	 * @throws CorruptDataException
	 */
	public ClassSegmentIterator(J9MemorySegmentPointer source) throws CorruptDataException {
		segments = new MemorySegmentIterator(source, MemorySegmentIterator.MEMORY_TYPE_RAM_CLASS, true);
		EOS = !segments.hasNext();		//no memory segments means no classloaders
	}
	
	public boolean hasNext() {
		if(hasNextClass) return true;	//next class already located
		if(EOS) return false;			//end of segments
		while(!hasNextClass) {
			try {
				if(clazzPtr.notNull()) {
					clazzPtr = clazzPtr.nextClassInSegment();
				}
				//locate either the first or the next valid segment to walk
				while(clazzPtr.isNull()) {					//class ptr not set
					EOS = !segments.hasNext();
					if(EOS) return false;										//reached EOS
					segment = (J9MemorySegmentPointer) segments.next();
					clazzPtr = J9ClassPointer.cast(PointerPointer.cast(segment.heapBase()).at(0));
				}
				//find the next class to return
				if(log.isLoggable(Level.FINE)) {
					log.fine(String.format("Class found : SEG=0x%016x : PTR=0x%016x", segment.getAddress() , clazzPtr.getAddress()));
				}
				hasNextClass = true;
			} catch (CorruptDataException e) {
				raiseCorruptDataEvent("Problem locating the next class", e, false);		//try and recover from this
				EOS = segments.hasNext();				//abort current segment iteration, see if there are more segments
				if(EOS) return false;					//no more segments available
				clazzPtr = J9ClassPointer.NULL;						//reset the class pointer
			}
		}
		return hasNextClass;
	}

	public Object next() {
		if(hasNext()) {
			hasNextClass = false;			//flag that we need to locate the next class
			return clazzPtr;
		} else {
			throw new NoSuchElementException();
		}
	}

	public void remove() {
		throw new UnsupportedOperationException("This iterator is read only");
	}
}
