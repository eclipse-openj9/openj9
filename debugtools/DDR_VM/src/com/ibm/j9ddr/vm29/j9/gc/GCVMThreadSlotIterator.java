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

import java.util.ArrayList;
import java.util.Iterator;
import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.StructurePointer.StructureField;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;

public class GCVMThreadSlotIterator extends GCIterator
{
	protected J9VMThreadPointer vmThread;
	protected Iterator<StructureField> slotIterator;
	
	protected GCVMThreadSlotIterator(J9VMThreadPointer vmThread)
	{
		this.vmThread = vmThread;
		StructureField[] fields = vmThread.getStructureFields();
		ArrayList<StructureField> objectFields = new ArrayList<StructureField>();
		for (StructureField structureField : fields) {
			// Look for object-typed fields
			if(structureField.type.equals("j9object_t")) {
				// With a non-null value or a fault (for reporting)
				if(structureField.value != null && ((J9ObjectPointer)structureField.value).notNull()) {
					objectFields.add(structureField);
				} else if(structureField.cde != null) {
					objectFields.add(structureField);
				}
			}
		}
		slotIterator = objectFields.iterator();
	}

	public static GCVMThreadSlotIterator fromJ9VMThread(J9VMThreadPointer vmThread) throws CorruptDataException
	{
		return new GCVMThreadSlotIterator(vmThread);
	}

	public boolean hasNext()
	{
		return slotIterator.hasNext();
	}

	public J9ObjectPointer next()
	{
		if(hasNext()) {
			StructureField field = slotIterator.next();
			if(field.cde == null) {
				return (J9ObjectPointer)field.value;
			} else {
				raiseCorruptDataEvent("Unable to retrieve thread slot", field.cde, false);
				return null;
			}
		} else {
			throw new NoSuchElementException("There are no more items available through this iterator");
		}
	}

	public VoidPointer nextAddress()
	{
		if(hasNext()) {
			StructureField field = slotIterator.next();
			if(field.cde == null) {
				return VoidPointer.cast(vmThread.addOffset(field.offset));
			} else {
				raiseCorruptDataEvent("Unable to retrieve thread slot", field.cde, false);
				return null;
			}
		} else {
			throw new NoSuchElementException("There are no more items available through this iterator");
		}
	}
}
