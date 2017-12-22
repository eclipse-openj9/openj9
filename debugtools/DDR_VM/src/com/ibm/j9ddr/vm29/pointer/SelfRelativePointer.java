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
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.types.I32;
import com.ibm.j9ddr.vm29.types.Scalar;
import com.ibm.j9ddr.vm29.types.UDATA;

public class SelfRelativePointer extends Pointer {
	public static final SelfRelativePointer NULL = new SelfRelativePointer(0);
	public static final long SIZEOF = I32Pointer.SIZEOF;
	
	protected SelfRelativePointer(long address) {
		super(address);
	}
	
	public static SelfRelativePointer cast(long address) {
		if (address == 0) {
			return NULL;
		}
		return new SelfRelativePointer(address);
	}
	
	public static SelfRelativePointer cast(UDATA address) {
		return cast(address.longValue());
	}
	
	public static SelfRelativePointer cast(AbstractPointer pointer) {
		return cast(pointer.address);
	}
	
	@Override
	public SelfRelativePointer add(long count) {
		return new SelfRelativePointer(address + (count * SIZEOF));
	}

	@Override
	public SelfRelativePointer add(Scalar count) {
		return add(count.longValue());
	}

	@Override
	public SelfRelativePointer addOffset(long offset) {
		return new SelfRelativePointer(address + offset);
	}

	@Override
	public SelfRelativePointer addOffset(Scalar offset) {
		return addOffset(offset.longValue());
	}

	@Override
	public DataType at(long index) throws CorruptDataException {
		throw new UnsupportedOperationException();
	}

	@Override
	public DataType at(Scalar index) throws CorruptDataException {
		throw new UnsupportedOperationException();
	}

	public SelfRelativePointer untag() {
		return untag(SIZEOF - 1);
	}
	
	public SelfRelativePointer untag(long mask) {
		return new SelfRelativePointer(address & ~mask);
	}

	@Override
	public SelfRelativePointer sub(long count)
	{
		return new SelfRelativePointer(address - (count * SIZEOF));
	}

	@Override
	public SelfRelativePointer sub(Scalar count)
	{
		return sub(count.longValue());
	}

	@Override
	public SelfRelativePointer subOffset(long offset)
	{
		return new SelfRelativePointer(address - offset);
	}

	@Override
	public SelfRelativePointer subOffset(Scalar offset)
	{
		return subOffset(offset.longValue());
	}
	
	public VoidPointer get() throws CorruptDataException
	{
		I32 offset = I32Pointer.cast(this).at(0);
		if(offset.eq(0)) {
			return VoidPointer.NULL;
		}
		return VoidPointer.cast(this.addOffset(offset));
	}

	@Override
	protected long sizeOfBaseType()
	{
		return SIZEOF;
	}
}
