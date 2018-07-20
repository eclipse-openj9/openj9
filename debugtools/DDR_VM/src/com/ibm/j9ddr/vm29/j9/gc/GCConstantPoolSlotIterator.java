/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_ANNOTATION_UTF8;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_CLASS;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_METHODHANDLE;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_METHOD_TYPE;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_STRING;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9_CP_BITS_PER_DESCRIPTION;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9_CP_DESCRIPTIONS_PER_U32;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9_CP_DESCRIPTION_MASK;

import java.util.ArrayList;
import java.util.Iterator;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMClassRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMMethodHandleRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMMethodTypeRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMStringRefPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMClassHelper;
import com.ibm.j9ddr.vm29.structure.J9RAMConstantPoolItem;

public class GCConstantPoolSlotIterator extends GCIterator
{
	protected Iterator<AbstractPointer> slotIterator;
	protected Iterator<VoidPointer> addressIterator;

	/* Do not instantiate directly. Use the factory */
	protected GCConstantPoolSlotIterator(J9ClassPointer clazz, boolean returnClassSlots, boolean returnObjectSlots) throws CorruptDataException
	{
		// TODO: Make a decision here on what version to use
		initializeSlots_V1(clazz, returnClassSlots, returnObjectSlots);
	}

	public static GCConstantPoolSlotIterator fromJ9Class(J9ClassPointer clazz, boolean returnClassSlots, boolean returnObjectSlots) throws CorruptDataException
	{
		return new GCConstantPoolSlotIterator(clazz, returnClassSlots, returnObjectSlots);
	}
	
	protected void initializeSlots_V1(J9ClassPointer clazz, boolean returnClassSlots, boolean returnObjectSlots) throws CorruptDataException
	{
		U32Pointer cpDescriptionSlots = J9ROMClassHelper.cpShapeDescription(clazz.romClass());
		PointerPointer cpEntry = PointerPointer.cast(clazz.ramConstantPool());
		long cpDescription = 0;
		long cpEntryCount = clazz.romClass().ramConstantPoolCount().longValue();
		long cpDescriptionIndex = 0;
		
		ArrayList<AbstractPointer> slots = new ArrayList<AbstractPointer>();
		ArrayList<VoidPointer> addresses = new ArrayList<VoidPointer>();
		while(cpEntryCount > 0) {
			if(0 == cpDescriptionIndex) {
				// Load a new description word
				cpDescription = cpDescriptionSlots.at(0).longValue();
				cpDescriptionSlots = cpDescriptionSlots.add(1);
				cpDescriptionIndex = J9_CP_DESCRIPTIONS_PER_U32;
			}
			long slotType = cpDescription & J9_CP_DESCRIPTION_MASK;
			if((slotType == J9CPTYPE_STRING) || (slotType == J9CPTYPE_ANNOTATION_UTF8)) {
				if(returnObjectSlots) {
					J9RAMStringRefPointer ref = J9RAMStringRefPointer.cast(cpEntry);
					J9ObjectPointer slot = ref.stringObject();
					if(slot.notNull()) {
						slots.add(slot);
						addresses.add(VoidPointer.cast(ref.stringObjectEA()));
					}
				}
			} else if(slotType == J9CPTYPE_METHOD_TYPE) {
				if(returnObjectSlots) {
					J9RAMMethodTypeRefPointer ref = J9RAMMethodTypeRefPointer.cast(cpEntry);
					J9ObjectPointer slot = ref.type();
					if(slot.notNull()) {
						slots.add(slot);
						addresses.add(VoidPointer.cast(ref.typeEA()));
					}
				}
			} else if(slotType == J9CPTYPE_METHODHANDLE) {
				if(returnObjectSlots) {
					J9RAMMethodHandleRefPointer ref = J9RAMMethodHandleRefPointer.cast(cpEntry);
					J9ObjectPointer slot = ref.methodHandle();
					if(slot.notNull()) {
						slots.add(slot);
						addresses.add(VoidPointer.cast(ref.methodHandleEA()));
					}
				}
			} else if(slotType == J9CPTYPE_CLASS) {
				if(returnClassSlots) {
					J9RAMClassRefPointer ref = J9RAMClassRefPointer.cast(cpEntry);
					J9ClassPointer slot = ref.value();
					if(slot.notNull()) {
						slots.add(slot);
						addresses.add(VoidPointer.cast(ref.valueEA()));
					}
				}				
			}
			cpEntry = cpEntry.addOffset(J9RAMConstantPoolItem.SIZEOF);
			cpEntryCount -= 1;
			cpDescription >>= J9_CP_BITS_PER_DESCRIPTION;
			cpDescriptionIndex -= 1;
		}
		slotIterator = slots.iterator();
		addressIterator = addresses.iterator();
	}

	public boolean hasNext()
	{
		return slotIterator.hasNext();
	}

	public AbstractPointer next()
	{
		addressIterator.next();			// Keep the iterators in sync
		return slotIterator.next();
	}
	
	public VoidPointer nextAddress()
	{
		slotIterator.next();			// Keep the iterators in sync
		return addressIterator.next();
	}
}
