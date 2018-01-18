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

import java.util.ArrayList;
import java.util.Iterator;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.J9ROMFieldShapeIterator;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.structure.J9JavaClassFlags;
import com.ibm.j9ddr.vm29.types.UDATA;

public class GCClassStaticsIterator extends GCIterator
{
	Iterator<J9ObjectPointer> slotsIterator;
	Iterator<VoidPointer> addressIterator;

	protected GCClassStaticsIterator(J9ClassPointer clazz) throws CorruptDataException
	{
		J9ROMFieldShapeIterator romFieldIterator = null;
		
		ArrayList<J9ObjectPointer> statics = new ArrayList<J9ObjectPointer>();
		ArrayList<VoidPointer> addresses = new ArrayList<VoidPointer>();
		
		long objectStaticCount = clazz.romClass().objectStaticCount().longValue();
		UDATAPointer staticPtr = clazz.ramStatics();
		
		/* Classes which have been hotswapped by fast HCR still have the ramStatics field
		 * set.  Statics must be walked only once, so only walk them for the most current
		 * version of the class.
		 */
		if (staticPtr.isNull()) {
			objectStaticCount = 0;
		}
		try {
			 if (J9ClassHelper.extendedClassFlags(clazz).allBitsIn(J9JavaClassFlags.J9ClassReusedStatics)) {
				 objectStaticCount = 0;
			 }
		} catch (NoSuchFieldError e) {
			/* Flag must be missing from the core -- do old check */
			if (J9ClassHelper.isSwappedOut(clazz) && !J9ClassHelper.areExtensionsEnabled()) {
				objectStaticCount = 0;
			}
		}
				
		while(objectStaticCount > 0) {
			//actual object slot, add it to iterators
			UDATA slot = staticPtr.at(0);
			if(!slot.eq(0)) {
				statics.add(J9ObjectPointer.cast(slot));
				addresses.add(VoidPointer.cast(staticPtr));
			}

			staticPtr = staticPtr.add(1); /* advance to next static slot */
			objectStaticCount -= 1;
		}
			
		slotsIterator = statics.iterator();
		addressIterator = addresses.iterator();
	}

	public static GCClassStaticsIterator fromJ9Class(J9ClassPointer clazz) throws CorruptDataException
	{
		return new GCClassStaticsIterator(clazz);
	}
	
	public boolean hasNext()
	{
		return slotsIterator.hasNext();
	}

	public J9ObjectPointer next()
	{
		addressIterator.next();			// Keep iterators in sync
		return slotsIterator.next();
	}
	
	public VoidPointer nextAddress()
	{
		slotsIterator.next();			// Keep iterators in sync
		return addressIterator.next();
	}
}
