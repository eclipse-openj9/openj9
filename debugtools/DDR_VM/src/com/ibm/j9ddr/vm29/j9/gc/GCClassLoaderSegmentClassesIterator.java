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
package com.ibm.j9ddr.vm29.j9.gc;

import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassLoaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_GCExtensionsPointer;
import com.ibm.j9ddr.vm29.structure.J9MemorySegment;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

/**
 * Iterator over the classes in the class segments of a class loader,
 */
public class GCClassLoaderSegmentClassesIterator extends GCIterator
{
	private GCClassHeapIterator _classHeapIterator;
	private GCClassLoaderSegmentIterator _segmentIterator;
	private J9ClassPointer _nextClass;
	
	public static GCClassLoaderSegmentClassesIterator from(J9ClassLoaderPointer classLoader) throws CorruptDataException
	{
		return new GCClassLoaderSegmentClassesIterator(GCBase.getExtensions(), classLoader);
	}

	private GCClassLoaderSegmentClassesIterator(MM_GCExtensionsPointer extensions, J9ClassLoaderPointer classLoader) throws CorruptDataException
	{
		_segmentIterator = GCClassLoaderSegmentIterator.fromJ9ClassLoader(classLoader, J9MemorySegment.MEMORY_TYPE_RAM_CLASS);
		if (_segmentIterator.hasNext()) {
			_classHeapIterator = GCClassHeapIterator.fromJ9MemorySegment(_segmentIterator.next());
			advanceIterator();
		} else {
			_nextClass = null;
		}
	}

	public VoidPointer nextAddress()
	{
		throw new UnsupportedOperationException("This iterator cannot return addresses.");
	}

	public boolean hasNext()
	{
		return _nextClass != null;
	}

	public J9ClassPointer next()
	{
		if (!hasNext()) {
			throw new NoSuchElementException("There are no more items available through this iterator"); 
		}

		J9ClassPointer next = _nextClass;
		advanceIterator();
		return next;
	}
	
	private void advanceIterator()
	{
		_nextClass = null;
		while (_nextClass == null) {
			if (_classHeapIterator.hasNext()) {
				// get the next class from a segment
				_nextClass = _classHeapIterator.next();
			} else if (_segmentIterator.hasNext()) {
				// no more in heap iterator, go to next segment
				try {
					_classHeapIterator = GCClassHeapIterator.fromJ9MemorySegment(_segmentIterator.next());
				} catch (CorruptDataException e) {
					// Try to continue iterating, _classHeapIterator should have its old value still
					raiseCorruptDataEvent("Problem locating the next class", e, false);
				}
			} else {
				// no more classes in this ClassLoader
				break;
			}
		}
	}
}
