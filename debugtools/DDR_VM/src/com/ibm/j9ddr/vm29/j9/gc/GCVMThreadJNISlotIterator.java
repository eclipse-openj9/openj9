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

import java.util.Iterator;
import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.Pool;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JNIReferenceFramePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9PoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;

public class GCVMThreadJNISlotIterator extends GCIterator
{
	protected J9JNIReferenceFramePointer jniFrame;
	protected Iterator<PointerPointer> poolIterator;

	protected GCVMThreadJNISlotIterator(J9VMThreadPointer vmThread) throws CorruptDataException
	{
		jniFrame = J9JNIReferenceFramePointer.cast(vmThread.jniLocalReferences());
		poolIterator = null;
	}

	public static GCVMThreadJNISlotIterator fromJ9VMThread(J9VMThreadPointer vmThread) throws CorruptDataException
	{
		return new GCVMThreadJNISlotIterator(vmThread);
	}
	
	public boolean hasNext()
	{
		if(jniFrame.isNull()) {
			// No more frames
			return false;
		}
		if(null == poolIterator) {
			// Haven't looked at the current frame yet
			try {
				J9PoolPointer poolPtr = J9PoolPointer.cast(jniFrame.references());
				poolIterator = Pool.fromJ9Pool(poolPtr, PointerPointer.class).iterator();
			} catch (CorruptDataException cde) {
				raiseCorruptDataEvent("Error looking at current frame", cde, false);		//can try to recover from this
				// Try moving to the previous frame
				try {
					jniFrame = jniFrame.previous();
					poolIterator = null;
					return hasNext();
				} catch (CorruptDataException cde2) {
					raiseCorruptDataEvent("Error looking at previous frame", cde2, true);		//cannot recover from this
					return false;
				}
			}
		}
		if(poolIterator.hasNext()) {
			return true;
		} else {
			// Move to the previous frame
			try {
				jniFrame = jniFrame.previous();
				poolIterator = null;
				return hasNext();
			} catch (CorruptDataException e) {
				raiseCorruptDataEvent("Error moving to previous frame", e, true);		//cannot recover from this
				return false;
			}
		}
	}

	public J9ObjectPointer next()
	{
		if(hasNext()) {
			try {
				PointerPointer next = poolIterator.next(); 
				return J9ObjectPointer.cast(next.at(0));
			} catch (CorruptDataException e) {
				raiseCorruptDataEvent("Error getting next item", e, false);		//can try to recover from this
				return null;
			}
		} else {
			throw new NoSuchElementException("There are no more items available through this iterator");
		}
	}
	
	public VoidPointer nextAddress()
	{
		if(hasNext()) {
			return VoidPointer.cast(poolIterator.next());
		} else {
			throw new NoSuchElementException("There are no more items available through this iterator");
		}
	}

}
