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
package com.ibm.j9ddr.vm29.view.dtfj.java.j9;

import java.util.Iterator;
import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionDescriptor;
import com.ibm.j9ddr.vm29.j9.walkers.HeapWalker;
import com.ibm.j9ddr.vm29.j9.walkers.HeapWalkerEvents;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.view.dtfj.DTFJContext;
import com.ibm.j9ddr.vm29.view.dtfj.java.DTFJJavaHeap;
import com.ibm.j9ddr.vm29.view.dtfj.java.DTFJJavaObject;

@SuppressWarnings("unchecked")
public class HeapObjectIterator implements HeapWalkerEvents, Iterator {
	private final DTFJJavaHeap heap;
	private final HeapWalker walker;
	private Object object = null;

	public HeapObjectIterator(DTFJJavaHeap heap, GCHeapRegionDescriptor region) throws CorruptDataException {
		this.heap = heap;
		walker = new HeapWalker(DTFJContext.getVm(), region, this);
	}
		
	public boolean hasNext() {
		while((object == null) && walker.hasNext()) {
			walker.walk();						//walk the heap looking for the next live object
		}
		return (object != null);	//did we find an object ?
	}
	
	public void doDeadObject(J9ObjectPointer object) {
		//this is a no-op
	}

	public void doLiveObject(J9ObjectPointer ptr) {
		object = new DTFJJavaObject(heap, ptr);
	}

	public void doSectionEnd(long address) {
		// this is a no-op
	}

	public void doSectionStart(long address) {
		// this is a no-op
	}

	public void remove() {
		throw new UnsupportedOperationException("This iterator is read only");
	}

	public Object next() {
		if(hasNext()) {			//this call will check if there are any more objects + move to the next valid one
			Object temp = object;
			object = null;
			return temp;
		}
		throw new NoSuchElementException();
	}

	public void doCorruptData(CorruptDataException e) {
		object = J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), e);	//CDE so this is what is returned
	}

}
