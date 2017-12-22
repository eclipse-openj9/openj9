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
package com.ibm.j9ddr.vm29.pointer;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.types.Scalar;
import com.ibm.j9ddr.vm29.types.UDATA;

public class DoublePointer extends Pointer {

	public static final int SIZEOF = 8;  // 64-Bit IEEE 754-1985 floating point number
	public static final DoublePointer NULL = new DoublePointer(0);

	// Do not call this constructor.  Use static method cast instead
	protected DoublePointer(long address) {
		super(address);
	}

	public static DoublePointer cast(AbstractPointer pointer) {
		return cast(pointer.getAddress());
	}
	
	public static DoublePointer cast(UDATA udata) {
		return cast(udata.longValue());
	}
	
	public static DoublePointer cast(long address) {
		if (address == 0) {
			return NULL;
		}
		return new DoublePointer(address);
	}

	public DoublePointer at(long index) throws CorruptDataException {
		throw new UnsupportedOperationException("Use doubleAt(long index)");
	}
	
	public DoublePointer at(Scalar index) throws CorruptDataException {
		return at(index.longValue());
	}
	
	public long longAt(long index) throws CorruptDataException {
		return (long) doubleAt(index);
	}
	
	public double doubleAt(long index) throws CorruptDataException {
		return getDoubleAtOffset(index * SIZEOF);
	}
	
	public float floatAt(long index) throws CorruptDataException {
		return (float) doubleAt(index);
	}
	
	public DoublePointer add(long count) {
		return new DoublePointer(address + (SIZEOF * count));
	}
	
	public DoublePointer add(Scalar count) {
		return add(count.longValue());
	}

	public DoublePointer addOffset(long offset) {
		return new DoublePointer(address + offset);
	}
	
	public DoublePointer addOffset(Scalar offset) {
		return addOffset(offset.longValue());
	}

	public DoublePointer sub(long count)
	{
		return new DoublePointer(address - (SIZEOF*count));
	}

	public DoublePointer sub(Scalar count)
	{
		return sub(count.longValue());
	}

	public DoublePointer subOffset(long offset)
	{
		return new DoublePointer(address - offset);
	}

	public DoublePointer subOffset(Scalar offset)
	{
		return subOffset(offset.longValue());
	}

	public DoublePointer untag() {
		return untag(SIZEOF - 1);
	}
	
	public DoublePointer untag(long mask) {
		return new DoublePointer(address & ~mask);
	}
	
	@Override
	protected long sizeOfBaseType()
	{
		return SIZEOF;
	}
}
