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
package com.ibm.j9ddr.vm29.j9.gc;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

import java.util.HashMap;
import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.ObjectModel;
import com.ibm.j9ddr.vm29.pointer.ObjectReferencePointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.types.UDATA;


class GCMixedObjectIterator_V1 extends GCObjectIterator
{
	protected final static HashMap<J9ClassPointer, boolean[]> descriptionCache = new HashMap<J9ClassPointer, boolean[]>();
	protected ObjectReferencePointer data;
	protected boolean[] descriptionArray;
	protected int scanIndex;
	protected int scanLimit;
	protected int bytesInObjectSlot;
	protected int objectsInDescriptionSlot;
	
	protected static void setCache(J9ClassPointer clazz, boolean[] description)
	{
		descriptionCache.put(clazz, description);
	}

	protected static boolean[] checkCache(J9ClassPointer clazz)
	{
		return descriptionCache.get(clazz);
	}
	
	protected GCMixedObjectIterator_V1(J9ObjectPointer object, boolean includeClassSlot) throws CorruptDataException
	{
		super(object, includeClassSlot);
		
		J9ClassPointer clazz = J9ObjectHelper.clazz(object);		
		bytesInObjectSlot = (int)ObjectReferencePointer.SIZEOF;
		objectsInDescriptionSlot = UDATA.SIZEOF * 8;	// 8 bits per byte
		
		VoidPointer data = VoidPointer.cast(object.addOffset(ObjectModel.getHeaderSize(object)));
		initialize(clazz, data);
	}

	protected GCMixedObjectIterator_V1(J9ClassPointer clazz, VoidPointer addr) throws CorruptDataException
	{
		super(null, false); // includeClassSlot = false
		bytesInObjectSlot = (int)ObjectReferencePointer.SIZEOF;
		objectsInDescriptionSlot = UDATA.SIZEOF * 8;	// 8 bits per byte
		
		initialize(clazz, addr);
	}

	private void initialize(J9ClassPointer clazz, VoidPointer addr)	throws CorruptDataException 
	{
		data = ObjectReferencePointer.cast(addr); 
		scanIndex = 0;

		scanLimit = clazz.totalInstanceSize().intValue() / bytesInObjectSlot;
		descriptionArray = checkCache(clazz);
		if (null == descriptionArray) {
			descriptionArray = new boolean[(clazz.totalInstanceSize().intValue() + bytesInObjectSlot - 1) / bytesInObjectSlot];
			if ((scanLimit > 0) && clazz.instanceDescription().notNull()) {
				initializeDescriptionArray(clazz);
				setCache(clazz, descriptionArray);
			}
		}
	}
	
	protected void initializeDescriptionArray(J9ClassPointer clazz) throws CorruptDataException
	{
		UDATAPointer descriptionPtr = clazz.instanceDescription();
		long tempDescription;
		
		if (descriptionPtr.anyBitsIn(1)) {
			// Immediate
			tempDescription = descriptionPtr.getAddress() >>> 1;
			initializeDescriptionArray(tempDescription, 0);
		} else {
			int descriptionSlot = 0;
			int descriptionIndex = 0;
			while (descriptionIndex < scanLimit) {
				tempDescription = descriptionPtr.at(descriptionSlot++).longValue();
				initializeDescriptionArray(tempDescription, descriptionIndex);
				descriptionIndex += objectsInDescriptionSlot;
			}
		}
	}

	private void initializeDescriptionArray(long tempDescription, int offset)
	{
		for(int i = 0; i < objectsInDescriptionSlot; i++) {
			if (1 == (tempDescription & 1)) {
				descriptionArray[offset + i] = true;
			}
			tempDescription >>>= 1;
		}
	}
	
	public boolean hasNext()
	{
		if (object != null && includeClassSlot) {
			return true;
		}
		
		while (scanIndex < scanLimit) {
			if (descriptionArray[scanIndex]) {
				return true;
			}
			scanIndex++;
		}
		return false;
	}

	@Override
	public J9ObjectPointer next()
	{
		try {
			if (hasNext()) {
				if (object != null && includeClassSlot) {
					includeClassSlot = false;
					return J9ObjectHelper.clazz(object).classObject();
				}
				J9ObjectPointer next = J9ObjectPointer.cast(data.at(scanIndex));	
				scanIndex++;
				return next;
			} else {
				throw new NoSuchElementException("There are no more items available through this iterator");
			}
		} catch (CorruptDataException e) {
			raiseCorruptDataEvent("Error getting next item", e, false);		//can try to recover from this
			return null;
		} 		
	}

	public VoidPointer nextAddress()
	{
		try {
			if (hasNext()) {
				if (object != null && includeClassSlot) {
					includeClassSlot = false;
					return VoidPointer.cast(J9ObjectHelper.clazz(object).classObjectEA());
				}
				VoidPointer next = VoidPointer.cast(data.add(scanIndex));	
				scanIndex++;
				return next;
			} else {
				throw new NoSuchElementException("There are no more items available through this iterator");
			}
		} catch (CorruptDataException e) {
			raiseCorruptDataEvent("Error getting next item", e, false);		//can try to recover from this
			return null;
		} 		
	}
}
