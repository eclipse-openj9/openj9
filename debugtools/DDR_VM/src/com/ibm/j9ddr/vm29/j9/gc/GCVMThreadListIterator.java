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
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;


public class GCVMThreadListIterator extends GCIterator 
{
	protected J9VMThreadPointer initialVMThread;
	protected J9VMThreadPointer currentVMThread;
	protected boolean consumedInitial;
	
	protected GCVMThreadListIterator() throws CorruptDataException 
	{
		this.initialVMThread = getJavaVM().mainThread();
		this.currentVMThread = initialVMThread;
	}
	
	/**
	 * Factory method to construct an appropriate vm thread list iterator.
	 * @param structure the J9JavaVM
	 * @param <T>
	 * 
	 * @return an instance of GCVMThreadListIterator 
	 * @throws CorruptDataException 
	 */
	public static GCVMThreadListIterator from() throws CorruptDataException
	{
		return new GCVMThreadListIterator();
	}
	
	public boolean hasNext() 
	{
		return currentVMThread.notNull();
	}

	public J9VMThreadPointer next() 
	{ 
		try {
			if(hasNext()) {
				J9VMThreadPointer next;
				next = currentVMThread;
				currentVMThread = currentVMThread.linkNext();
				if(currentVMThread.equals(initialVMThread)) {
					currentVMThread = J9VMThreadPointer.NULL;
				}
				return next;
			} else {
				throw new NoSuchElementException("There are no more items available through this iterator");
			}
		} catch(CorruptDataException cde) {
			currentVMThread = J9VMThreadPointer.NULL; // link to next vm thread is broken, terminate iterator
			raiseCorruptDataEvent("Error getting next item", cde, false);
			return null;	
		}
	}
	
	public VoidPointer nextAddress() 
	{
		// Does not make sense in this case
		throw new UnsupportedOperationException("This iterator cannot return addresses");
	}
}
