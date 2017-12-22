/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

public class FloatPointer extends Pointer {

	public static final int SIZEOF = 4;  // 32-Bit IEEE 754-1985 floating point number
	public static final FloatPointer NULL = new FloatPointer(0);

	// Do not call this constructor.  Use static method cast instead
	protected FloatPointer(long address) {
		super(address);
	}

	public static FloatPointer cast(AbstractPointer pointer) {
		return cast(pointer.getAddress());
	}
	
	public static FloatPointer cast(UDATA udata) {
		return cast(udata.longValue());
	}
	
	public static FloatPointer cast(long address) {
		if (address == 0) {
			return NULL;
		}
		return new FloatPointer(address);
	}

	public FloatPointer at(long index) throws CorruptDataException {
		throw new UnsupportedOperationException("Use floatAt(long index)");
	}
	
	public FloatPointer at(Scalar index) throws CorruptDataException {
		return at(index.longValue());
	}
	
	public long longAt(long index) throws CorruptDataException {
		return (long) floatAt(index);
	}
	
	public double doubleAt(long index) throws CorruptDataException {
		return (double) floatAt(index);
	}
	
	public float floatAt(long index) throws CorruptDataException {
		return getFloatAtOffset(index * SIZEOF);
	}
	
	public FloatPointer add(long count) {
		return new FloatPointer(address + (SIZEOF * count));
	}
	
	public FloatPointer add(Scalar count) {
		return add(count.longValue());
	}

	public FloatPointer addOffset(long offset) {
		return new FloatPointer(address + offset);
	}
	
	public FloatPointer addOffset(Scalar offset) {
		return addOffset(offset.longValue());
	}

	public FloatPointer sub(long count)
	{
		return new FloatPointer(address - (SIZEOF*count));
	}

	public FloatPointer sub(Scalar count)
	{
		return sub(count.longValue());
	}

	public FloatPointer subOffset(long offset)
	{
		return new FloatPointer(address - offset);
	}

	public FloatPointer subOffset(Scalar offset)
	{
		return subOffset(offset.longValue());
	}

	public FloatPointer untag() {
		return untag(SIZEOF - 1);
	}
	
	public FloatPointer untag(long mask) {
		return new FloatPointer(address & ~mask);
	}
	
	@Override
	protected long sizeOfBaseType()
	{
		return SIZEOF;
	}
}
