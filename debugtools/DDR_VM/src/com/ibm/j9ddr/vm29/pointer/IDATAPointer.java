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
package com.ibm.j9ddr.vm29.pointer;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.types.IDATA;
import com.ibm.j9ddr.vm29.types.Scalar;
import com.ibm.j9ddr.vm29.types.UDATA;

public class IDATAPointer extends Pointer {

	public static final int SIZEOF = IDATA.SIZEOF;
	public static final IDATAPointer NULL = new IDATAPointer(0);

	// Do not call this constructor.  Use static method cast instead
	protected IDATAPointer(long address) {
		super(address);
	}
	
	public static IDATAPointer cast(AbstractPointer pointer) {
		return cast(pointer.getAddress());
	}
	
	public static IDATAPointer cast(UDATA udata) {
		return cast(udata.longValue());
	}
	
	public static IDATAPointer cast(long address) {
		if (address == 0) {
			return NULL;
		}
		return new IDATAPointer(address);
	}
	
	public IDATA at(long index) throws CorruptDataException {
		return new IDATA(getIDATAAtOffset(index * SIZEOF));
	}
	
	public IDATA at(Scalar index) throws CorruptDataException {
		return at(index.longValue());
	}
	
	public IDATAPointer untag() {
		return untag(SIZEOF - 1);
	}
	
	public IDATAPointer untag(long mask) {
		return new IDATAPointer(address & ~mask);
	}
	
	public IDATAPointer add(long count) {
		return new IDATAPointer(address + (SIZEOF * count));
	}
	
	public IDATAPointer add(Scalar count) {
		return add(count.longValue());
	}
	
	public IDATAPointer addOffset(long offset) {
		return new IDATAPointer(address + offset);
	}
	
	public IDATAPointer addOffset(Scalar offset) {
		return addOffset(offset.longValue());
	}
	
	@Override
	public IDATAPointer sub(long count)
	{
		return new IDATAPointer(address - (SIZEOF*count));
	}

	@Override
	public IDATAPointer sub(Scalar count)
	{
		return sub(count.longValue());
	}

	public IDATA sub(IDATAPointer pointer) {
		long baseSize = sizeOfBaseType();

		if (baseSize != pointer.sizeOfBaseType()) {
			throw new UnsupportedOperationException(
				"Cannot subtract pointers to types of different sizes; "
					+ "this type = " + getClass()
					+ ", parameter type = " + pointer.getClass());
		}

		IDATA diff = new IDATA(this.address).sub(new IDATA(pointer.address));

		return new IDATA(diff.longValue() / baseSize);
	}

	@Override
	public IDATAPointer subOffset(long offset)
	{
		return new IDATAPointer(address - offset);
	}

	@Override
	public IDATAPointer subOffset(Scalar offset)
	{
		return subOffset(offset.longValue());
	}
	
	@Override
	protected long sizeOfBaseType()
	{
		return SIZEOF;
	}
}
