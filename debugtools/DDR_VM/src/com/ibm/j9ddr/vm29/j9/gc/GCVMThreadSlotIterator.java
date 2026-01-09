/*
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
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

public class GCVMThreadSlotIterator extends GCIterator {

	/**
	 * In core files, typedef types may be expanded: This helper method
	 * recognizes types that are equivalent to j9object_t:
	 *     typedef struct J9Object *j9object_t;
	 */
	private static boolean isObjectTyped(String type) {
		/*
		 * Because ddrgen doesn't distinguish between class and struct types
		 * when reading .pdb files, we recognize both flavours here.
		 */
		switch (type) {
		case "j9object_t":
		case "class J9Object*":
		case "struct J9Object*":
			return true;
		default:
			return false;
		}
	}

	protected final J9VMThreadPointer vmThread;
	protected final Iterator<StructureField> slotIterator;

	protected GCVMThreadSlotIterator(J9VMThreadPointer vmThread) {
		this.vmThread = vmThread;
		StructureField[] fields = vmThread.getStructureFields();
		ArrayList<StructureField> objectFields = new ArrayList<>();
		for (StructureField structureField : fields) {
			// look for object-typed fields
			if (isObjectTyped(structureField.type)) {
				// with a fault (for reporting) or a non-null value
				if (structureField.cde != null) {
					objectFields.add(structureField);
				} else if ((structureField.value != null) && ((J9ObjectPointer) structureField.value).notNull()) {
					objectFields.add(structureField);
				}
			}
		}
		slotIterator = objectFields.iterator();
	}

	public static GCVMThreadSlotIterator fromJ9VMThread(J9VMThreadPointer vmThread) throws CorruptDataException {
		return new GCVMThreadSlotIterator(vmThread);
	}

	@Override
	public boolean hasNext() {
		return slotIterator.hasNext();
	}

	@Override
	public J9ObjectPointer next() {
		if (hasNext()) {
			StructureField field = slotIterator.next();
			if (field.cde == null) {
				return (J9ObjectPointer) field.value;
			} else {
				raiseCorruptDataEvent("Unable to retrieve thread slot", field.cde, false);
				return null;
			}
		} else {
			throw new NoSuchElementException("There are no more items available through this iterator");
		}
	}

	@Override
	public VoidPointer nextAddress() {
		if (hasNext()) {
			StructureField field = slotIterator.next();
			if (field.cde == null) {
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
