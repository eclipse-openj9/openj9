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
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;

public class GCClassIteratorClassSlots extends GCIterator
{
	public final static int state_start = 0;
	public final static int state_constant_pool = 1;
	public final static int state_superclasses = 2;
	public final static int state_interfaces = 3;
	public final static int state_array_class_slots = 4;
	public final static int state_end = 5;
	
	protected int state;
	protected GCConstantPoolClassSlotIterator constantPoolSlotIterator;
	protected GCClassSuperclassesIterator superclassesIterator;
	protected GCClassLocalInterfaceIterator localInterfacesIterator;
	protected GCClassArrayClassSlotIterator arrayClassSlotIterator;
	
	protected GCClassIteratorClassSlots(J9ClassPointer clazz) throws CorruptDataException
	{
		state = state_start;
		constantPoolSlotIterator = GCConstantPoolClassSlotIterator.fromJ9Class(clazz);
		superclassesIterator = GCClassSuperclassesIterator.fromJ9Class(clazz);
		localInterfacesIterator = GCClassLocalInterfaceIterator.fromJ9Class(clazz);
		arrayClassSlotIterator = GCClassArrayClassSlotIterator.fromJ9Class(clazz);
	}

	public static GCClassIteratorClassSlots fromJ9Class(J9ClassPointer clazz) throws CorruptDataException
	{
		return new GCClassIteratorClassSlots(clazz); 
	}
	
	public boolean hasNext()
	{
		if(state == state_end) {
			return false;
		}
		switch(state) {
		
		case state_start:
			state++;
		
		case state_constant_pool:
			if(constantPoolSlotIterator.hasNext()) {
				return true;
			}
			state++;
			
		case state_superclasses:
			if(superclassesIterator.hasNext()) {
				return true;
			}
			state++;
			
		case state_interfaces:
			if(localInterfacesIterator.hasNext()) {
				return true;
			}
			state++;
			
		case state_array_class_slots:
			if(arrayClassSlotIterator.hasNext()) {
				return true;
			}
			state++;
		}
		return false;
	}

	public J9ClassPointer next()
	{
		if(hasNext()) {
			switch(state) {
			
			case state_constant_pool:
				return constantPoolSlotIterator.next();
				
			case state_superclasses:
				return superclassesIterator.next();
				
			case state_interfaces:
				return localInterfacesIterator.next();
				
			case state_array_class_slots:
				return arrayClassSlotIterator.next();
			}
		}
		throw new NoSuchElementException("There are no more items available through this iterator"); 
	}

	public VoidPointer nextAddress()
	{
		if(hasNext()) {
			switch(state) {
			
			case state_constant_pool:
				return constantPoolSlotIterator.nextAddress();
				
			case state_superclasses:
				return superclassesIterator.nextAddress();
				
			case state_interfaces:
				return localInterfacesIterator.nextAddress();
				
			case state_array_class_slots:
				return arrayClassSlotIterator.nextAddress();
			}
		}
		throw new NoSuchElementException("There are no more items available through this iterator"); 
	}

	public int getState()
	{
		return state;
	}
}
