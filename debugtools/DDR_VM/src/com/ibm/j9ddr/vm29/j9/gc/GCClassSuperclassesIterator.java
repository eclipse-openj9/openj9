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

import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.types.UDATA;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

public class GCClassSuperclassesIterator extends GCIterator
{
	protected PointerPointer superclasses;
	protected UDATA classDepth;
	protected UDATA index;

	protected GCClassSuperclassesIterator(J9ClassPointer clazz) throws CorruptDataException
	{
		superclasses = clazz.superclasses();
		classDepth = J9ClassHelper.classDepth(clazz);
		index = new UDATA(0);
	}

	public static GCClassSuperclassesIterator fromJ9Class(J9ClassPointer clazz) throws CorruptDataException
	{
		return new GCClassSuperclassesIterator(clazz); 
	}
	
	public boolean hasNext()
	{
		if(classDepth.eq(0)) {
			return false;
		}
		return index.lt(classDepth);
	}

	public J9ClassPointer next()
	{
		if(hasNext()) {
			try {
				J9ClassPointer next = J9ClassPointer.cast(superclasses.at(index));
				index = index.add(1);
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
			VoidPointer next = VoidPointer.cast(superclasses.add(index));
			index = index.add(1);
			return next;
		} else {
			throw new NoSuchElementException("There are no more items available through this iterator");
		}
	}
}
