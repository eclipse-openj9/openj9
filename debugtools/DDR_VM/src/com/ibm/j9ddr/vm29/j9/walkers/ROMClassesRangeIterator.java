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

import java.io.PrintStream;
import java.util.Iterator;
import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;

public class ROMClassesRangeIterator extends ROMClassesIterator implements Iterator<J9ROMClassPointer> {
	private long startAddr;
	private long endAddr;
	
	/*
	 * This iterator returns all J9ROMClasses which have their starting address in the range specified by 'startAddr' and 'endAddr'.
	 * 'startAddr' should be a valid J9ROMClass address.
	 * No such restriction exists on 'endAddr'.
	 */
	public ROMClassesRangeIterator(PrintStream out, U8Pointer startAddr, U8Pointer endAddr) throws CorruptDataException {
		super(out, null);
		this.startAddr = startAddr.longValue();
		this.endAddr = endAddr.longValue();
	}
	
	public boolean hasNext() {
		return (J9ROMClassPointer.NULL != getNextClass());
	}
	
	public J9ROMClassPointer next() {
		J9ROMClassPointer clazz = getNextClass();
		if (clazz != J9ROMClassPointer.NULL) {
			nextClass = clazz;
		} else {
			throw new NoSuchElementException();
		}
		return nextClass;
	}

	private J9ROMClassPointer getNextClass() {
		long newHeapPtr = 0;
		J9ROMClassPointer newNextClass = J9ROMClassPointer.NULL;
		
		if (nextClass == J9ROMClassPointer.NULL) {
			newHeapPtr = startAddr;
		} else {
			try {
				newHeapPtr = nextClass.getAddress() + nextClass.romSize().longValue();
			} catch (CorruptDataException e) {
				/* This shouldn't happen, but if it does then it's likely an internal error */ 
				out.append("Unabled to read size of ROMClass at " + nextClass.getHexAddress() + ".\n");
				newNextClass = J9ROMClassPointer.NULL;
			}
		}
		if ((newHeapPtr != 0) && (newHeapPtr < endAddr)) {
			/* test if the next ROMClass is valid */
			newNextClass = J9ROMClassPointer.cast(newHeapPtr);
			try {
				if (newNextClass.romSize().eq(0)) {
					out.append("Size of ROMClass at " + newNextClass.getHexAddress() + "is invalid.\n");
					newNextClass = J9ROMClassPointer.NULL;
				}
			} catch (CorruptDataException e) {
				out.append("Unable to read size of ROMClass at " + newNextClass.getHexAddress() + ".\n");
				newNextClass = J9ROMClassPointer.NULL;
			}
		}
		return newNextClass;
	}
}
