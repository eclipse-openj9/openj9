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
package com.ibm.j9ddr.vm29.j9.gc;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.ObjectAccessBarrier;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_UnfinalizedObjectListPointer;

public class GCUnfinalizedObjectListIterator extends GCIterator
{
	protected MM_UnfinalizedObjectListPointer head;
	protected MM_UnfinalizedObjectListPointer currentList;
	protected J9ObjectPointer currentObject;
	protected J9ObjectPointer next;
	
	protected GCUnfinalizedObjectListIterator(MM_UnfinalizedObjectListPointer unfinalizedObjectLists) throws CorruptDataException
	{
		head = unfinalizedObjectLists;
		currentList = head;
		if(currentList.isNull()) {
			currentObject = J9ObjectPointer.NULL;
			next = null;
		} else {
			currentObject = currentList._head();
			next = currentObject;
		}
	}

	public static GCUnfinalizedObjectListIterator from() throws CorruptDataException
	{
		return new GCUnfinalizedObjectListIterator(getExtensions().unfinalizedObjectLists());
	}

	public boolean hasNext()
	{
		if (null != next) {
			return true;
		}
		
		while (currentList.notNull()) {
			//get the next object on the list
			try {
				if (currentObject.notNull()) {
					currentObject = ObjectAccessBarrier.getFinalizeLink(currentObject);
				}
			} catch (CorruptDataException cde) {
				raiseCorruptDataEvent("Error in unfinalized object list", cde, false);
				currentObject = J9ObjectPointer.NULL;
			}
			
			if (currentObject.notNull()) {
				next = currentObject;
				return true;
			}
			
			//move to the next list
			try {
				currentList = currentList._nextList();
			} catch (CorruptDataException cde) {
				raiseCorruptDataEvent("Error in unfinalized object list", cde, true);
				currentList = MM_UnfinalizedObjectListPointer.NULL;
			}
			
			try {
				if (currentList.notNull()) {
					currentObject = currentList._head();
					next = currentObject;
					return true;
				}
			} catch (CorruptDataException cde) {
				raiseCorruptDataEvent("Error in unfinalized object list", cde, false);
				currentObject = J9ObjectPointer.NULL;
			}
		}
		
		return false;
	}

	public J9ObjectPointer next()
	{
		if (hasNext()) {
			J9ObjectPointer tmp = next;
			next = null;
			return tmp;
		} else {
			throw new NoSuchElementException("There are no more items available through this iterator");
		}
	}
	
	public VoidPointer nextAddress() 
	{
		// Does not make sense in this case
		throw new UnsupportedOperationException("This iterator cannot return addresses");
	}
}
