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
import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_SublistPuddlePointer;

abstract class MMSublistSlotIterator extends GCIterator
{
	private MM_SublistPuddlePointer sublistPuddle;
	private UDATAPointer scanPtr;
	
	protected MMSublistSlotIterator(MM_SublistPuddlePointer sublistPuddle) throws CorruptDataException 
	{
		this.sublistPuddle = sublistPuddle;
		scanPtr = sublistPuddle._listBase();
	}
	
	public boolean hasNext()
	{
		try {
			return scanPtr.lt(sublistPuddle._listCurrent());
		} catch (CorruptDataException e) {
			raiseCorruptDataEvent("Error iterating slots", e, true);		//cannot recover from this
			return false;
		}
	}

	public AbstractPointer next()
	{
		if(hasNext()) {
			VoidPointer currentScanPtr = VoidPointer.cast(scanPtr);
			scanPtr = scanPtr.add(1);
			return currentScanPtr;
		} else {
			throw new NoSuchElementException("There are no more items available through this iterator");
		}
	}
}
