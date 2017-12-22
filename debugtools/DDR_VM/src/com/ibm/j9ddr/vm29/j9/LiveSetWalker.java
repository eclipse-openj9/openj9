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

import java.util.HashSet;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.events.EventManager;
import com.ibm.j9ddr.vm29.j9.RootSet.RootSetType;
import com.ibm.j9ddr.vm29.j9.gc.GCClassIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCClassIteratorClassSlots;
import com.ibm.j9ddr.vm29.j9.gc.GCIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCObjectIterator;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;

/*
 * Not written as an Iterator like everyone else because there's no convenient way to save iteration state
 */
public class LiveSetWalker  
{
	
	public interface ObjectVisitor
	{
		/**
		 * @param object object we're visiting
		 * @param address address of the object we're visiting
		 * @return whether we should visit this object or not
		 */
		public boolean visit(J9ObjectPointer object, VoidPointer address);
		
		/**
		 * Called when we are finished visiting an object (we have visited itself and all it's children) in a pre-order walk
		 * 
		 * @param object object we're visiting
		 * @param address address of the object we're visiting
		 */
		public void finishVisit(J9ObjectPointer object, VoidPointer address);
	}

	/*
	 * Walks the LiveSet in preorder, returns a HashSet of all walked objects.  The HashSet is generated as a byproduct of the walk.
	 * 
	 * @return a HashSet containing all visited objects 
	 */
	public static void walkLiveSet(ObjectVisitor visitor, RootSetType rootSetType) throws CorruptDataException
	{
		/* TODO: lpnguyen, use something less stupid than a hashSet (markMap?) for this */
		HashSet<J9ObjectPointer> visitedObjects = new HashSet<J9ObjectPointer>();
		
		/* Not using a singleton because we want to allow users to catch corruptData events.  Not worrying about the perf loss taken
		 * by doing this as walking the live set will probably take an order of magnitude longer anyway
		 */
		RootSet rootSet = RootSet.from(rootSetType, false);
		
		GCIterator rootIterator = rootSet.gcIterator(rootSetType);
		GCIterator rootAddressIterator = rootSet.gcIterator(rootSetType);
		
		while (rootIterator.hasNext()) {
			J9ObjectPointer nextObject = (J9ObjectPointer) rootIterator.next();
			VoidPointer nextAddress = rootAddressIterator.nextAddress();
			
			if (nextObject.notNull()) {
				scanObject(visitedObjects, visitor, nextObject, nextAddress);
			}
		}
	}
	
	public static void walkLiveSet(ObjectVisitor visitor) throws CorruptDataException
	{
		/* TODO: lpnguyen Walking strongly reachable roots but still walking weakly reachable slots (e.g. weak reference referent slot) is somewhat misleading.
		 * We should add the ability for the recursive walk (i.e. scanObject method) to ignore slots containing a weak reference
		 */
		walkLiveSet(visitor, RootSetType.STRONG_REACHABLE);
	}
	
	private static void scanObject(HashSet<J9ObjectPointer> visitedObjects, ObjectVisitor visitor, J9ObjectPointer object, VoidPointer address)
	{	
		if (visitedObjects.contains(object)) {
			return;
		}

		if (!visitor.visit(object, address)) {
			return;
		}
		
		visitedObjects.add(object);
		
		
		try {
			GCObjectIterator objectIterator = GCObjectIterator.fromJ9Object(object, true);
			GCObjectIterator addressIterator = GCObjectIterator.fromJ9Object(object, true);
			while (objectIterator.hasNext()) {
				J9ObjectPointer slot = objectIterator.next();
				VoidPointer addr = addressIterator.nextAddress();
				if (slot.notNull()) {
					scanObject(visitedObjects, visitor, slot, addr);
				}
			}
			
			if (J9ObjectHelper.getClassName(object).equals("java/lang/Class")) {
				J9ClassPointer clazz = ConstantPoolHelpers.J9VM_J9CLASS_FROM_HEAPCLASS(object);
				
				// Iterate the Object slots
				GCClassIterator classIterator = GCClassIterator.fromJ9Class(clazz);
				GCClassIterator classAddressIterator = GCClassIterator.fromJ9Class(clazz);
				while(classIterator.hasNext()) {
					J9ObjectPointer slot = classIterator.next();
					VoidPointer addr = classAddressIterator.nextAddress();
					
					if (slot.notNull()) {
						scanObject(visitedObjects, visitor, slot, addr);
					}				
				}
				
				// Iterate the Class slots
				GCClassIteratorClassSlots classSlotIterator = GCClassIteratorClassSlots.fromJ9Class(clazz);
				GCClassIteratorClassSlots classSlotAddressIterator = GCClassIteratorClassSlots.fromJ9Class(clazz);
				while (classSlotIterator.hasNext()) {
					J9ClassPointer slot = classSlotIterator.next();
					VoidPointer addr = classSlotAddressIterator.nextAddress();
					J9ObjectPointer classObject = ConstantPoolHelpers.J9VM_J9CLASS_TO_HEAPCLASS(slot);
					if (classObject.notNull()) {
						scanObject(visitedObjects, visitor, classObject, addr);
					}				
				}
			}
		} catch (CorruptDataException e) {
			EventManager.raiseCorruptDataEvent("Corruption found while walking the live set, object: " + object.getHexAddress(), e, false);
		}
		
		visitor.finishVisit(object, address);
	}
}
