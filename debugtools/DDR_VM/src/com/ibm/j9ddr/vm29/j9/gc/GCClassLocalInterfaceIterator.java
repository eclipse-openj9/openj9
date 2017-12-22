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
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ITablePointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;

public class GCClassLocalInterfaceIterator extends GCIterator
{
	protected J9ITablePointer iTable;
	protected J9ITablePointer superclassITable;
	
	protected GCClassLocalInterfaceIterator(J9ClassPointer clazz) throws CorruptDataException
	{
		iTable = J9ITablePointer.cast(clazz.iTable());
		J9ClassPointer superclass = J9ClassHelper.superclass(clazz);
		if(superclass.isNull()) {
			superclassITable = J9ITablePointer.NULL;
		} else {
			superclassITable = J9ITablePointer.cast(superclass.iTable());
		}
	}

	public static GCClassLocalInterfaceIterator fromJ9Class(J9ClassPointer clazz) throws CorruptDataException
	{
		return new GCClassLocalInterfaceIterator(clazz); 
	}
	
	public boolean hasNext()
	{
		if(iTable.eq(superclassITable)) {
			return false;
		}
		return true;
	}

	public J9ClassPointer next()
	{
		if(hasNext()) {
			J9ClassPointer next;
			try {
				next = iTable.interfaceClass();
				iTable = iTable.next();
				return next;
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
			try {
				VoidPointer next = VoidPointer.cast(iTable.interfaceClassEA());
				iTable = iTable.next();
				return next;
			} catch (CorruptDataException e) {
				raiseCorruptDataEvent("Error getting next item", e, false);		//can try to recover from this
				return null;
			}
		} else {
			throw new NoSuchElementException("There are no more items available through this iterator"); 
		}
	}

}
