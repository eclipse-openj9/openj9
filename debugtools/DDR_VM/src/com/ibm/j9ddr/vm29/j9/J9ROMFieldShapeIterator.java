/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9;

import java.util.Iterator;
import java.util.NoSuchElementException;

import com.ibm.j9ddr.AddressedCorruptDataException;
import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.CorruptPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMFieldShapePointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMFieldShapeHelper;
import com.ibm.j9ddr.vm29.types.UDATA;

public class J9ROMFieldShapeIterator implements Iterator, Iterable<J9ROMFieldShapePointer> {

	J9ROMFieldShapePointer firstField;
	J9ROMFieldShapePointer lastField;
	long fieldsLeft;
	long cursor = 0;
	
	public J9ROMFieldShapeIterator(J9ROMFieldShapePointer romFields, UDATA romFieldCount) {
		this.firstField = romFields;
		this.fieldsLeft = romFieldCount.longValue();
	}

	public boolean hasNext() {
		return cursor < fieldsLeft;
	}

	public Object next() {
		if (!hasNext()) {
			throw new NoSuchElementException();
		}
		
		if (lastField == null) {
			lastField = J9ROMFieldShapePointer.cast(firstField);
		} else {
			try {
				lastField = J9ROMFieldShapeHelper.next(lastField);
			}catch (AddressedCorruptDataException e) {
					return CorruptPointer.cast(e.getAddress());
			} catch (CorruptDataException e) {
				return CorruptPointer.NULL;
			}
		}
		
		cursor++;
		return lastField;
	}

	public void remove() {
		throw new UnsupportedOperationException();
	}

	public Iterator<J9ROMFieldShapePointer> iterator() {
		return this;
	}

}
