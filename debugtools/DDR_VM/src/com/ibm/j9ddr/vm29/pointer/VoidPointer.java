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
package com.ibm.j9ddr.vm29.pointer;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.NullPointerDereference;
import com.ibm.j9ddr.vm29.types.Scalar;
import com.ibm.j9ddr.vm29.types.UDATA;

public class VoidPointer extends Pointer {
	
	public static final VoidPointer NULL = new VoidPointer(0);

	// Do not call this constructor.  Use static method cast instead
	protected VoidPointer(long address) {
		super(address);
	}
	
	public static VoidPointer cast(AbstractPointer pointer) {
		return cast(pointer.getAddress());
	}
	
	public static VoidPointer cast(UDATA udata) {
		return cast(udata.longValue());
	}
	
	public static VoidPointer cast(long address) {
		if (address == 0) {
			return NULL;
		}
		return new VoidPointer(address);
	}
	
	public Scalar at(long index) throws CorruptDataException {
		throw new NullPointerDereference("Can not dereference a void *");
	}
	
	public Scalar at(Scalar index) throws CorruptDataException {
		return at(index.longValue());
	}
	
	public VoidPointer untag() {
		return untag(SIZEOF - 1);
	}
	
	public VoidPointer untag(long mask) {
		return new VoidPointer(address & ~mask);
	}
	
	public VoidPointer add(long count) {
		return new VoidPointer(address + (SIZEOF * count));
	}
	
	public VoidPointer add(Scalar count) {
		return add(count.longValue());
	}
	
	public VoidPointer addOffset(long offset) {
		return new VoidPointer(address + offset);
	}
	
	public VoidPointer addOffset(Scalar offset) {
		return addOffset(offset.longValue());
	}

	@Override
	public AbstractPointer sub(long count) {
		throw new UnsupportedOperationException("Pointer math not supported on void pointers");
	}

	@Override
	public AbstractPointer sub(Scalar count) {
		throw new UnsupportedOperationException("Pointer math not supported on void pointers");
	}

	@Override
	public VoidPointer subOffset(long offset) {
		return new VoidPointer(address - offset);
	}

	@Override
	public VoidPointer subOffset(Scalar offset) {
		return subOffset(offset.longValue());
	}
	
	@Override
	protected long sizeOfBaseType()
	{
		throw new UnsupportedOperationException();
	}
}
