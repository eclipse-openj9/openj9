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
package com.ibm.j9ddr.vm29.types;

import com.ibm.j9ddr.InvalidDataTypeException;

public class U32 extends UDATA {
	// Constants
	public static final int SIZEOF = 4;
	public static final long MASK = 0xFFFFFFFFL;
	public static final U32 MIN = new U32(0x00000000L);
	public static final U32 MAX = new U32(0xFFFFFFFFL);

	// Constructor
	public U32(long value) {
		super(value & MASK);
	}
	
	public U32(Scalar parameter) {
		super(parameter);
	}
	
	// API
	
	// Add
	
	public U32 add(int number) {
		return new U32(data + number);
	}

	public U32 add(UScalar parameter) {
		return add(new U32(parameter));
	}

	public U32 add(I32 parameter) {
		return new U32(data + parameter.data);
	}

	public U32 add(U32 parameter) {
		return new U32(data + parameter.data);
	}
	
	public U64 add(U64 parameter) {
		return new U64(this).add(parameter);
	}
	
	public boolean eq(U64 parameter) {
		return new U64(this).eq(parameter);
	}
	
	public UDATA add(UDATA parameter) {
		return new UDATA(this).add(parameter);
	}
	
	public boolean eq(UDATA parameter) {
		return new UDATA(this).eq(parameter);
	}
	
	public U32 add(IScalar parameter) {
		return add(new U32(parameter));
	}
	
	public boolean eq(IScalar parameter) {
		return eq(new U32(parameter));
	}
	
	public I64 add(I64 parameter) {
		return new I64(this).add(parameter);
	}
	
	public boolean eq(I64 parameter) {
		return new I64(this).eq(parameter);
	}
	
	public IDATA add(IDATA parameter) {
		return new IDATA(this).add(parameter);
	}
	
	// Sub
	
	public U32 sub(int number) {
		return new U32(data - number);
	}

	public U32 sub(UScalar parameter) {
		return sub(new U32(parameter));
	}
	
	public U32 sub(U32 parameter) {
		return new U32(data - parameter.data);
	}
	
	public U64 sub(U64 parameter) {
		return new U64(this).sub(parameter);
	}
	
	public UDATA sub(UDATA parameter) {
		return new UDATA(this).sub(parameter);
	}
	
	public U32 sub(IScalar parameter) {
		return sub(new U32(parameter));
	}
	
	public I64 sub(I64 parameter) {
		return new I64(this).sub(parameter);
	}
	
	public UDATA sub(IDATA parameter) {
		return new UDATA(this).sub(parameter);
	}
	
	public int intValue() {
		long value = data;

		if (value < 0 || value > Integer.MAX_VALUE) {
			throw new InvalidDataTypeException("U32 contains value larger than Integer.MAX_VALUE");
		}

		return (int) value;
	}
	
	// bitOr
	
	public U32 bitOr(int number) {
		return new U32(data | number);
	}
	
	public U32 bitOr(long number) {
		return new U32(data | number);
	}

	public U32 bitOr(UScalar parameter) {
		return bitOr(new U32(parameter));
	}
	
	public U32 bitOr(U32 parameter) {
		return new U32(data | parameter.data);
	}
	
	public U64 bitOr(U64 parameter) {
		return new U64(this).bitOr(parameter);
	}
	
	public UDATA bitOr(UDATA parameter) {
		return new UDATA(this).bitOr(parameter);
	}
	
	public U32 bitOr(IScalar parameter) {
		return bitOr(new U32(parameter));
	}
	
	public I64 bitOr(I64 parameter) {
		return new I64(this).bitOr(parameter);
	}
	
	public UDATA bitOr(IDATA parameter) {
		return new UDATA(this).bitOr(parameter);
	}
	
	// bitXor
	
	public U32 bitXor(int number) {
		return new U32(data ^ number);
	}
	
	public U32 bitXor(long number) {
		return new U32(data ^ number);
	}
	
	public U32 bitXor(Scalar parameter) {
		return bitXor(new U32(parameter));
	}
	
	public U32 bitXor(U32 parameter) {
		return new U32(data ^ parameter.data);
	}
	
	public UDATA bitXor(UDATA parameter) {
		return new UDATA(this).bitXor(parameter);
	}
	
	public IDATA bitXor(IDATA parameter) {
		return new IDATA(this).bitXor(parameter);
	}

	public U64 bitXor(U64 parameter) {
		return new U64(this).bitXor(parameter);
	}
	
	public I64 bitXor(I64 parameter) {
		return new I64(this).bitXor(parameter);
	}
	
	// bitAnd
	
	public U32 bitAnd(int number) {
		return new U32(data & number);
	}
	
	public U32 bitAnd(long number) {
		return new U32(data & number);
	}

	public U32 bitAnd(UScalar parameter) {
		return bitAnd(new U32(parameter));
	}
	
	public U32 bitAnd(U32 parameter) {
		return new U32(data & parameter.data);
	}
	
	public U64 bitAnd(U64 parameter) {
		return new U64(this).bitAnd(parameter);
	}
	
	public UDATA bitAnd(UDATA parameter) {
		return new UDATA(this).bitAnd(parameter);
	}
	
	public U32 bitAnd(IScalar parameter) {
		return bitAnd(new U32(parameter));
	}
	
	public I64 bitAnd(I64 parameter) {
		return new I64(this).bitAnd(parameter);
	}
	
	public UDATA bitAnd(IDATA parameter) {
		return new UDATA(this).bitAnd(parameter);
	}
	
	// leftShift
	public U32 leftShift(int i) {
		return new U32(data << i);
	}
	
	// rightShift
	public U32 rightShift(int i) {
		return new U32(data >>> i);
	}
	
	// bitNot
	public U32 bitNot() {
		return new U32(~data);
	}
	
	public U32 mult(int parameter) {
		return new U32(data * parameter);
	}
	
	public U32 mult(U32 parameter) {
		return new U32(data * parameter.data);
	}
	
	@Override
	public int sizeof()
	{
		return SIZEOF;
	}

}
