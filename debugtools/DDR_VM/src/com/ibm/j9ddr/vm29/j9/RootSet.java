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
package com.ibm.j9ddr.vm29.j9;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.gc.GCIterator;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;

public class RootSet
{
	public enum RootSetType 
	{
		ALL(0), STRONG_REACHABLE(1), WEAK_REACHABLE(2), ALL_SLOTS_EXCLUDING_STACK_SLOTS(3), STRONG_REACHABLE_EXCLUDING_STACK_SLOTS(4);
		
        private final int value;

        private RootSetType(final int newValue) { value = newValue; }

        public int getValue() { return value; }	
	};
	
	private static RootSet[] _singletons = new RootSet[RootSetType.values().length];
	
	private ArrayList<J9ObjectPointer> _allRoots;
	private ArrayList<VoidPointer> _allAddresses;
	
	private class RootSetScanner extends SimpleRootScanner 
	{
		protected RootSetScanner() throws CorruptDataException 
		{
			super();
		}

		@Override
		protected void doClassSlot(J9ClassPointer slot, VoidPointer address)
		{
			if (slot.notNull()) {
				doClass(slot, address);
			}
		}
		
		@Override
		protected void doSlot(J9ObjectPointer slot, VoidPointer address)
		{
			if (slot.notNull()) {
				_allRoots.add(slot);
				_allAddresses.add(address);
			}
		}
	
		@Override
		protected void doClass(J9ClassPointer slot, VoidPointer address)
		{	
			J9ObjectPointer classObject = J9ObjectPointer.NULL;
			VoidPointer classObjectSlot = VoidPointer.NULL;
			try {
				classObject = slot.classObject();
				classObjectSlot = VoidPointer.cast(slot.classObjectEA());
			} catch(CorruptDataException cde) {
				 raiseCorruptDataEvent("Class: " + slot.getHexAddress() + " has invalid classObject slot", cde, false);
				 return;
			}
			
			doSlot(classObject, classObjectSlot);
		}
	}

	private RootSet(RootSetType rootSetType) throws CorruptDataException
	{
		super();
		_allRoots = new ArrayList<J9ObjectPointer>();
		_allAddresses = new ArrayList<VoidPointer>();
		RootSetScanner rootSetScanner = new RootSetScanner();
		
		switch(rootSetType) {
		case ALL:
			rootSetScanner.scanAllSlots();
			break;
		case STRONG_REACHABLE:
			rootSetScanner.scanRoots();
			break;
		case WEAK_REACHABLE:
			rootSetScanner.scanClearable();
			break;
		case ALL_SLOTS_EXCLUDING_STACK_SLOTS:
			rootSetScanner.setScanStackSlots(false);
			rootSetScanner.scanAllSlots();
			break;
		case STRONG_REACHABLE_EXCLUDING_STACK_SLOTS:
			rootSetScanner.setScanStackSlots(false);
			rootSetScanner.scanRoots();
		default:
			throw new UnsupportedOperationException("Invalid rootSetType");
		}
	}
	
	/*
	 * useSingleton parameter exists primarily so that we can be guaranteed to catch corruptData events (See EventManager) for users
	 * who want to notified.
	 */
	protected static RootSet from(RootSetType rootSetType, boolean useSingleton) throws CorruptDataException 
	{
		RootSet rootSetToReturn;
		
		if (useSingleton) {
			if (null == _singletons[rootSetType.getValue()]) {
				rootSetToReturn = new RootSet(rootSetType);
			} else {
				rootSetToReturn = _singletons[rootSetType.getValue()]; 
			}
		} else {
			rootSetToReturn = new RootSet(rootSetType);
		}
		
		if (null == _singletons[rootSetType.getValue()]) {
			_singletons[rootSetType.getValue()] = rootSetToReturn;
		}
		
		return rootSetToReturn;
	}
	
	public static List<J9ObjectPointer> allRoots(RootSetType rootSetType) throws CorruptDataException
	{
		RootSet rootSet = new RootSet(rootSetType);
		return Collections.unmodifiableList(rootSet._allRoots);
	}
	
	public GCIterator gcIterator(RootSetType rootSetType) throws CorruptDataException
	{
		final Iterator<J9ObjectPointer> rootSetIterator = _allRoots.iterator();
		final Iterator<VoidPointer> rootSetAddressIterator = _allAddresses.iterator();
		
		return new GCIterator() {
			public boolean hasNext() {
				return rootSetIterator.hasNext();
			}

			public VoidPointer nextAddress() {
				rootSetIterator.next();
				return rootSetAddressIterator.next();
			}

			public Object next() {
				rootSetAddressIterator.next();
				return rootSetIterator.next(); 
			}
		};
	}
	
	public static GCIterator rootIterator(RootSetType rootSetType) throws CorruptDataException
	{
		final RootSet rootSet = RootSet.from(rootSetType, true);
		return rootSet.gcIterator(rootSetType);
	}
}
