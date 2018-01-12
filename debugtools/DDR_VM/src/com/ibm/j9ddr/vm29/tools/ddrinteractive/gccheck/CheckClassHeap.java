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

import com.ibm.j9ddr.CorruptDataException;

import com.ibm.j9ddr.vm29.j9.gc.GCClassHeapIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCSegmentIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MemorySegmentPointer;
import com.ibm.j9ddr.vm29.structure.J9MemorySegment;

class CheckClassHeap extends Check
{
	@Override
	public void check()
	{
		try {
			GCSegmentIterator segmentIterator = GCSegmentIterator.fromJ9MemorySegmentList(_javaVM.classMemorySegments(), J9MemorySegment.MEMORY_TYPE_RAM_CLASS);
			while(segmentIterator.hasNext()) {
				J9MemorySegmentPointer segment = segmentIterator.next();
				_engine.clearPreviousObjects();
				GCClassHeapIterator classHeapIterator = GCClassHeapIterator.fromJ9MemorySegment(segment);
				while(classHeapIterator.hasNext()) {
					J9ClassPointer clazz = classHeapIterator.next();
					if(_engine.checkClassHeap(clazz, segment) != CheckBase.J9MODRON_SLOT_ITERATOR_OK) {
						return;
					}
					_engine.pushPreviousClass(clazz);
				}
			}
		} catch (CorruptDataException e) {
			// TODO: handle exception
		}
	}

	@Override
	public String getCheckName()
	{
		return "CLASS HEAP";
	}

	@Override
	public void print()
	{
		getReporter().println("Printing of class heap not currently supported");
	}

}
