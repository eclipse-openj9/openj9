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
package com.ibm.j9ddr.vm29.j9;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

import java.util.Iterator;
import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaStackPointer;


public class J9JavaStackIterator implements Iterator<J9JavaStackPointer> 
{
	protected J9JavaStackPointer initialStack;
	protected J9JavaStackPointer currentStack;
	protected boolean consumedInitial;
	
	protected J9JavaStackIterator(J9JavaStackPointer stack) throws CorruptDataException 
	{
		this.initialStack = stack;
		this.currentStack = stack;
	}
	
	/**
	 * Factory method to construct an appropriate stack list iterator.
	 * @param <T>
	 * 
	 * @param structure the head of the list
	 * 
	 * @return an instance of J9JavaStackIterator 
	 * @throws CorruptDataException 
	 */
	public static J9JavaStackIterator fromJ9JavaStack(J9JavaStackPointer stack) throws CorruptDataException
	{
		return new J9JavaStackIterator(stack);
	}
	
	public boolean hasNext() 
	{
		return currentStack.notNull();
	}

	public J9JavaStackPointer next() 
	{ 
		try {
			if(hasNext()) {
				J9JavaStackPointer next;
				next = currentStack;
				currentStack = currentStack.previous();
				return next;
			} else {
				throw new NoSuchElementException("There are no more items available through this iterator");
			}
		} catch(CorruptDataException cde) {
			currentStack = J9JavaStackPointer.NULL; // broken link to previous stack section, terminate the walk
			raiseCorruptDataEvent("Error getting next stack section", cde, false);
			return null;	
		}
	}
	
	public void remove() 
	{
		throw new UnsupportedOperationException("The image is read only and cannot be modified.");
	}
}
