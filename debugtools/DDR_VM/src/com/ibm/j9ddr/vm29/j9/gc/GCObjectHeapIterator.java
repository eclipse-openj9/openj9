/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapRegionDescriptorPointer;
import com.ibm.j9ddr.vm29.types.UDATA;

public abstract class GCObjectHeapIterator extends GCIterator
{
	protected boolean includeLiveObjects;
	protected boolean includeDeadObjects;

	/* Do not instantiate. Use the factory */
	protected GCObjectHeapIterator(boolean includeLiveObjects, boolean includeDeadObjects) throws CorruptDataException 
	{
		this.includeLiveObjects = includeLiveObjects;
		this.includeDeadObjects = includeDeadObjects;
	}
	
	//TODO: lpnguyen remove me later. Needed for silo-dance
	public static GCObjectHeapIterator fromHeapRegionDescriptor(MM_HeapRegionDescriptorPointer hrd, boolean includeLiveObjects, boolean includeDeadObjects) throws CorruptDataException
	{
		return fromHeapRegionDescriptor(GCHeapRegionDescriptor.fromHeapRegionDescriptor(hrd), includeLiveObjects, includeDeadObjects);
	}

	public static GCObjectHeapIterator fromHeapRegionDescriptor(GCHeapRegionDescriptor hrd, boolean includeLiveObjects, boolean includeDeadObjects) throws CorruptDataException
	{
		//TODO: lpnguyen is this factory method worth it?
		return hrd.objectIterator(includeLiveObjects, includeDeadObjects);
	}

	public abstract J9ObjectPointer next();
	
	public abstract J9ObjectPointer peek();
	
	public VoidPointer nextAddress() 
	{
		// Does not make sense in this case
		throw new UnsupportedOperationException("This iterator cannot return addresses");
	}

	public abstract void advance(UDATA size);
}
