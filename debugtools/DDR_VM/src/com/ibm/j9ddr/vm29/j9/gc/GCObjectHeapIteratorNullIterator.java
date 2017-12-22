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
import com.ibm.j9ddr.vm29.types.UDATA;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;

public class GCObjectHeapIteratorNullIterator extends GCObjectHeapIterator {

	protected GCObjectHeapIteratorNullIterator() throws CorruptDataException
	{
		// we don't want to see any objects so just pass false for both live and dead objects
		super(false, false);
	}

	@Override
	public J9ObjectPointer next()
	{
		throw new NoSuchElementException("This iterator sees no objects");
	}
	
	@Override
	public J9ObjectPointer peek()
	{
		throw new NoSuchElementException("This iterator sees no objects");
	}

	@Override
	public void advance(UDATA size)
	{
		throw new NoSuchElementException("This iterator sees no objects");
	}
		
	public boolean hasNext()
	{
		return false;
	}

}
