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
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;

public class GCClassIterator extends GCIterator
{
	public final static int state_start = 0;
	public final static int state_statics = 1;
	public final static int state_constant_pool = 2;
	public final static int state_slots = 3;
	public final static int state_end = 4;
	
	protected int state;
	protected GCClassStaticsIterator staticsIterator;
	protected GCConstantPoolObjectSlotIterator constantPoolSlotIterator;
	protected J9ClassPointer clazz;
	
	protected GCClassIterator(J9ClassPointer clazz) throws CorruptDataException
	{
		state = state_start;
		staticsIterator = GCClassStaticsIterator.fromJ9Class(clazz);
		constantPoolSlotIterator = GCConstantPoolObjectSlotIterator.fromJ9Class(clazz);
		this.clazz = clazz;
	}

	public static GCClassIterator fromJ9Class(J9ClassPointer clazz) throws CorruptDataException
	{
		return new GCClassIterator(clazz); 
	}
	
	public boolean hasNext()	
	{
		if(state == state_end) {
			return false;
		}
		switch(state) {
		
		case state_start:
			state++;
		
		case state_statics:
			if(staticsIterator.hasNext()) {
				return true;
			}
			state++;
		
		case state_constant_pool:
			if(constantPoolSlotIterator.hasNext()) {
				return true;
			}
			state++;

		case state_slots:
			if(clazz != null) {
				return true;
			}
			state++;
		}
		return false;
	}

	public J9ObjectPointer next()
	{
		if(hasNext()) {
			switch(state) {
			
			case state_statics:
				return staticsIterator.next();
			
			case state_constant_pool:
				return constantPoolSlotIterator.next();

			case state_slots:
				if(clazz != null) {
					J9ObjectPointer classObject = null;
					try {
						classObject = clazz.classObject();
					} catch (CorruptDataException e) {
						raiseCorruptDataEvent("Error fetching the class object", e, false);		//can try to recover from this
					}
					clazz = null;
					return classObject;
				}
			}
		}
		throw new NoSuchElementException("There are no more items available through this iterator"); 
	}

	public VoidPointer nextAddress()
	{
		if(hasNext()) {
			switch(state) {
			
			case state_statics:
				return staticsIterator.nextAddress();
			
			case state_constant_pool:
				return constantPoolSlotIterator.nextAddress();

			case state_slots:
				if(clazz != null) {
					VoidPointer ea = null;
					try {
						ea = VoidPointer.cast(clazz.classObjectEA());
					} catch (CorruptDataException e) {
						raiseCorruptDataEvent("Error fetching the class object", e, false);		//can try to recover from this
					}
					clazz = null;
					return ea;
				}
			}
		}
		throw new NoSuchElementException("There are no more items available through this iterator"); 
	}

	public int getState()
	{
		return state;
	}
}
