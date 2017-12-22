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

import java.util.Iterator;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.Pool;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9PoolPointer;

public class GCJNIGlobalReferenceIterator extends GCIterator
{
	protected Iterator<PointerPointer> iterator;
	
	protected GCJNIGlobalReferenceIterator(J9PoolPointer globalRefs) throws CorruptDataException
	{
		iterator = Pool.fromJ9Pool(globalRefs, PointerPointer.class).iterator(); 
	}

	public static GCJNIGlobalReferenceIterator from() throws CorruptDataException
	{
		return new GCJNIGlobalReferenceIterator(getJavaVM().jniGlobalReferences());
	}
	
	public boolean hasNext()
	{
		return iterator.hasNext();
	}

	public J9ObjectPointer next()
	{
		try {
			PointerPointer next = iterator.next(); 
			return J9ObjectPointer.cast(next.at(0));
		} catch (CorruptDataException e) {
			raiseCorruptDataEvent("Error getting next item", e, false);		//can try to recover from this
			return null;
		}
	}

	public VoidPointer nextAddress()
	{
		return VoidPointer.cast(iterator.next());
	}
}
