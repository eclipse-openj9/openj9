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
import com.ibm.j9ddr.vm29.types.IDATA;
import com.ibm.j9ddr.vm29.types.Scalar;
import com.ibm.j9ddr.vm29.types.UDATA;

/*
 * J9DDR analogue of J9WSRP
 */
public class WideSelfRelativePointer extends Pointer {
	public static final WideSelfRelativePointer NULL = new WideSelfRelativePointer(0);
	public static final long SIZEOF = IDATAPointer.SIZEOF;
	
	protected WideSelfRelativePointer(long address) {
		super(address);
	}
	
	public static WideSelfRelativePointer cast(long address) {
		if (address == 0) {
			return NULL;
		}
		return new WideSelfRelativePointer(address);
	}
	
	public static WideSelfRelativePointer cast(UDATA address) {
		return cast(address.longValue());
	}
	
	public static WideSelfRelativePointer cast(AbstractPointer pointer) {
		return cast(pointer.address);
	}
	
	@Override
	public WideSelfRelativePointer add(long count) {
		return new WideSelfRelativePointer(address + (count * SIZEOF));
	}

	@Override
	public WideSelfRelativePointer add(Scalar count) {
		return add(count.longValue());
	}

	@Override
	public WideSelfRelativePointer addOffset(long offset) {
		return new WideSelfRelativePointer(address + offset);
	}

	@Override
	public WideSelfRelativePointer addOffset(Scalar offset) {
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

	public WideSelfRelativePointer untag() {
		return untag(SIZEOF - 1);
	}
	
	public WideSelfRelativePointer untag(long mask) {
		return new WideSelfRelativePointer(address & ~mask);
	}

	@Override
	public WideSelfRelativePointer sub(long count)
	{
		return new WideSelfRelativePointer(address - (count * SIZEOF));
	}

	@Override
	public WideSelfRelativePointer sub(Scalar count)
	{
		return sub(count.longValue());
	}

	@Override
	public WideSelfRelativePointer subOffset(long offset)
	{
		return new WideSelfRelativePointer(address - offset);
	}

	@Override
	public WideSelfRelativePointer subOffset(Scalar offset)
	{
		return subOffset(offset.longValue());
	}
	
	public VoidPointer get() throws CorruptDataException
	{
		IDATA offset = IDATAPointer.cast(this).at(0);
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
