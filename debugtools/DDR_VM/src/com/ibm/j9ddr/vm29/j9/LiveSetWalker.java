/*
 * Copyright IBM Corp. and others 2001
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package com.ibm.j9ddr.vm29.j9; 

import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

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
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags;
import com.ibm.j9ddr.vm29.structure.J9ROMFieldOffsetWalkState;
import com.ibm.j9ddr.vm29.types.U32;

import static com.ibm.j9ddr.vm29.j9.J9ObjectFieldOffsetIterator.J9ObjectFieldOffsetIteratorFor;

/*
 * Not written as an Iterator like everyone else because there's no convenient way to save iteration state.
 */
public class LiveSetWalker {

	public interface ObjectVisitor {
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

	private static Long referentOffset;

	private static long getReferentOffset(J9ClassPointer j9class) throws CorruptDataException {
		long offset = -1;
		J9ClassPointer superclass;

		for (J9ClassPointer clazz = j9class; clazz.notNull(); clazz = superclass) {
			superclass = J9ClassHelper.superclass(clazz);

			if (clazz.classDepthAndFlags().anyBitsIn(J9JavaAccessFlags.J9AccClassReferenceMask)) {
				/* The current class is a (strict) subclass of java.lang.ref.Reference.
				 * Return the offset if known, otherwise move on to its superclass.
				 */
				if (referentOffset != null) {
					offset = referentOffset.longValue();
					break;
				}
			} else if ("java/lang/ref/Reference".equals(J9ClassHelper.getName(clazz))) {
				U32 fieldFlags = new U32(J9ROMFieldOffsetWalkState.J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE);
				Iterator<J9ObjectFieldOffset> fields = J9ObjectFieldOffsetIteratorFor(clazz, superclass, fieldFlags);

				while (fields.hasNext()) {
					J9ObjectFieldOffset field = fields.next();
					if ("referent".equals(field.getName())) {
						offset = J9ObjectHelper.headerSize() + field.getOffsetOrAddress().longValue();
						break;
					}
				}

				referentOffset = Long.valueOf(offset);
				break;
			} else {
				/* The current class is neither java.lang.ref.Reference
				 * nor a subclass: There is no referent field to consider.
				 */
				break;
			}
		}

		return offset;
	}

	/*
	 * Visits the LiveSet in preorder.
	 */
	public static void walkLiveSet(ObjectVisitor visitor, RootSetType rootSetType) throws CorruptDataException {
		/* TODO: lpnguyen, use something less stupid than a HashSet (markMap?) for this */
		Set<J9ObjectPointer> visitedObjects = new HashSet<>();

		/* Not using a singleton because we want to allow users to catch corruptData events.
		 * Not worrying about the perf loss taken by doing this as walking the live set will
		 * probably take an order of magnitude longer anyway.
		 */
		RootSet rootSet = RootSet.from(rootSetType, false);
		boolean onlyStronglyReachable = (rootSetType == RootSetType.STRONG_REACHABLE)
				|| (rootSetType == RootSetType.STRONG_REACHABLE_EXCLUDING_STACK_SLOTS);

		GCIterator rootIterator = rootSet.gcIterator(rootSetType);
		GCIterator rootAddressIterator = rootSet.gcIterator(rootSetType);

		while (rootIterator.hasNext()) {
			J9ObjectPointer nextObject = (J9ObjectPointer) rootIterator.next();
			VoidPointer nextAddress = rootAddressIterator.nextAddress();

			if (nextObject.notNull()) {
				scanObject(visitedObjects, visitor, nextObject, nextAddress, onlyStronglyReachable);
			}
		}
	}

	public static void walkLiveSet(ObjectVisitor visitor) throws CorruptDataException {
		walkLiveSet(visitor, RootSetType.STRONG_REACHABLE);
	}

	private static void scanObject(
			Set<J9ObjectPointer> visitedObjects,
			ObjectVisitor visitor, J9ObjectPointer object, VoidPointer address,
			boolean onlyStronglyReachable) {
		if (visitedObjects.contains(object)) {
			return;
		}

		if (!visitor.visit(object, address)) {
			return;
		}

		visitedObjects.add(object);

		try {
			J9ClassPointer j9class = J9ObjectHelper.clazz(object);
			long referentAddr = 0;

			if (onlyStronglyReachable) {
				long offset = getReferentOffset(j9class);

				if (offset >= 0) {
					referentAddr = object.getAddress() + offset;
				}
			}

			GCObjectIterator objectIterator = GCObjectIterator.fromJ9Object(object, true);
			GCObjectIterator addressIterator = GCObjectIterator.fromJ9Object(object, true);

			while (objectIterator.hasNext()) {
				J9ObjectPointer slot = objectIterator.next();
				VoidPointer addr = addressIterator.nextAddress();

				if (slot.notNull() && (addr.getAddress() != referentAddr)) {
					scanObject(visitedObjects, visitor, slot, addr, onlyStronglyReachable);
				}
			}

			String className = J9ClassHelper.getName(j9class);

			if (className.equals("java/lang/Class")) {
				J9ClassPointer clazz = ConstantPoolHelpers.J9VM_J9CLASS_FROM_HEAPCLASS(object);

				// Iterate the Object slots.
				GCClassIterator classIterator = GCClassIterator.fromJ9Class(clazz);
				GCClassIterator classAddressIterator = GCClassIterator.fromJ9Class(clazz);
				while (classIterator.hasNext()) {
					J9ObjectPointer slot = classIterator.next();
					VoidPointer addr = classAddressIterator.nextAddress();

					if (slot.notNull()) {
						scanObject(visitedObjects, visitor, slot, addr, onlyStronglyReachable);
					}
				}

				// Iterate the Class slots.
				GCClassIteratorClassSlots classSlotIterator = GCClassIteratorClassSlots.fromJ9Class(clazz);
				GCClassIteratorClassSlots classSlotAddressIterator = GCClassIteratorClassSlots.fromJ9Class(clazz);
				while (classSlotIterator.hasNext()) {
					J9ClassPointer slot = classSlotIterator.next();
					VoidPointer addr = classSlotAddressIterator.nextAddress();
					J9ObjectPointer classObject = ConstantPoolHelpers.J9VM_J9CLASS_TO_HEAPCLASS(slot);
					if (classObject.notNull()) {
						scanObject(visitedObjects, visitor, classObject, addr, onlyStronglyReachable);
					}
				}
			}
		} catch (CorruptDataException e) {
			EventManager.raiseCorruptDataEvent(
					"Corruption found while walking the live set, object: " + object.getHexAddress(), e, false);
		}

		visitor.finishVisit(object, address);
	}

}
