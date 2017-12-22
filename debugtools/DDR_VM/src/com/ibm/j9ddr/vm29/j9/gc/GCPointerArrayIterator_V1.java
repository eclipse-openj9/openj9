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
import com.ibm.j9ddr.vm29.j9.ObjectModel;
import com.ibm.j9ddr.vm29.pointer.ObjectReferencePointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;



class GCPointerArrayIterator_V1 extends GCObjectIterator
{
	protected ObjectReferencePointer data;
	protected int scanIndex;
	protected int scanLimit;
	
	protected GCPointerArrayIterator_V1(J9ObjectPointer object, boolean includeClassSlot) throws CorruptDataException
	{
		super(object, includeClassSlot);
		scanIndex = 0;
		scanLimit = ObjectModel.getSizeInElements(object).intValue();
		data = ObjectReferencePointer.cast(object.addOffset(ObjectModel.getHeaderSize(object)));
	}
	
	public boolean hasNext()
	{
		if(includeClassSlot) {
			return true;
		}
		return (scanIndex < scanLimit);
	}
	
	@Override
	public J9ObjectPointer next()
	{
		try {
			if(hasNext()) {
				if(includeClassSlot) {
					includeClassSlot = false;
					return J9ObjectHelper.clazz(object).classObject();
				}
			
				J9ObjectPointer next = J9ObjectPointer.cast(data.at(scanIndex));
				scanIndex++;
				return next;
			} else {
				throw new NoSuchElementException("There are no more items available through this iterator");
			}
		} catch (CorruptDataException e) {
			raiseCorruptDataEvent("Error getting next item", e, false);		//can try to recover from this
			return null;
		}
	}
	
	public VoidPointer nextAddress()
	{
		try {
			if(hasNext()) {
				if(includeClassSlot) {
					includeClassSlot = false;
					return VoidPointer.cast(J9ObjectHelper.clazz(object).classObjectEA());
				}
			
				VoidPointer next = VoidPointer.cast(data.add(scanIndex));
				scanIndex++;
				return next;
			} else {
				throw new NoSuchElementException("There are no more items available through this iterator");
			}
		} catch (CorruptDataException e) {
			raiseCorruptDataEvent("Error getting next item", e, false);		//can try to recover from this
			return null;
		}
	}
}
