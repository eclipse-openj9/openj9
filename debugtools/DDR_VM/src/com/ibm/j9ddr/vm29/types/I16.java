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
package com.ibm.j9ddr.vm29.types;


public class I16 extends IScalar {
	
	// Constants
	public static final int SIZEOF = 2;
	public static final long MASK = 0xFFFFL;
	public static final I16 MIN = new I16(0x8000L);
	public static final I16 MAX = new I16(0x7FFFL);
	
	// Constructors
	public I16(long value) {
		super(value & MASK);
	}
	
	public I16(Scalar parameter) {
		super(parameter);
	}
	
	// API
	
	// ADD
	
	public I16 add(int number) {
		return new I16(data + number);
	}
	
	public I32 add(U8 parameter) {
		return new I32(this).add(parameter);
	}
	
	public boolean eq(U8 parameter) {
		return new I32(this).eq(parameter);
	}
	
	public I32 add(U16 parameter) {
		return new I32(this).add(parameter);
	}
	
	public boolean eq(U16 parameter) {
		return new I32(this).eq(parameter);
	}
	
	public U32 add(U32 parameter) {
		return new U32(this).add(parameter);
	}
	
	public boolean eq(U32 parameter) {
		return new U32(this).eq(parameter);
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
	
	public I32 add(I8 parameter) {
		return new I32(this).add(parameter);
	}
	
	public I16 add(I16 parameter) {
		return new I16(data + parameter.data);
	}
	
	public I32 add(I32 parameter) {
		return new I32(this).add(parameter);
	}
	
	public boolean eq(I32 parameter) {
		return new I32(this).eq(parameter);
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
	
	public boolean eq(IDATA parameter) {
		return new IDATA(this).eq(parameter);
	}
	//SUB
	
	public I16 sub(int number) {
		return new I16(data - number);
	}
	
	public I32 sub(U8 parameter) {
		return new I32(this).sub(parameter);
	}
	
	public I32 sub(U16 parameter) {
		return new I32(this).sub(parameter);
	}
	
	public U32 sub(U32 parameter) {
		return new U32(this).sub(parameter);
	}
	
	public U64 sub(U64 parameter) {
		return new U64(this).sub(parameter);
	}
	
	public UDATA sub(UDATA parameter) {
		return new UDATA(this).sub(parameter);
	}
	
	public I32 sub(I8 parameter) {
		return new I32(this).sub(parameter);
	}
	
	public I16 sub(I16 parameter) {
		return new I16(data - parameter.data);
	}
	
	public I32 sub(I32 parameter) {
		return new I32(this).sub(parameter);
	}
	
	public I64 sub(I64 parameter) {
		return new I64(this).sub(parameter);
	}
	
	public IDATA sub(IDATA parameter) {
		return new IDATA(this).sub(parameter);
	}

	
	public long longValue() {
		return (short) data;
	}
	
	public int intValue() {
		return (short) data;
	}
	
	// bitOr
	
	public I16 bitOr(int number) {
		return new I16(data | number);
	}
	
	public I16 bitOr(long number) {
		return new I16(data | number);
	}
	
	public I32 bitOr(U8 parameter) {
		return new I32(this).bitOr(parameter);
	}
	
	public I32 bitOr(U16 parameter) {
		return new I32(this).bitOr(parameter);
	}
	
	public U32 bitOr(U32 parameter) {
		return new U32(this).bitOr(parameter);
	}
	
	public U64 bitOr(U64 parameter) {
		return new U64(this).bitOr(parameter);
	}
	
	public UDATA bitOr(UDATA parameter) {
		return new UDATA(this).bitOr(parameter);
	}
	
	public I32 bitOr(I8 parameter) {
		return new I32(this).bitOr(parameter);
	}
	
	public I16 bitOr(I16 parameter) {
		return new I16(data | parameter.data);
	}
	
	public I32 bitOr(I32 parameter) {
		return new I32(this).bitOr(parameter);
	}
	
	public I64 bitOr(I64 parameter) {
		return new I64(this).bitOr(parameter);
	}
	
	public IDATA bitOr(IDATA parameter) {
		return new IDATA(this).bitOr(parameter);
	}
	
	// bitAnd
	
	public I16 bitAnd(int number) {
		return new I16(data & number);
	}
	
	public I16 bitAnd(long number) {
		return new I16(data & number);
	}
	
	public I32 bitAnd(U8 parameter) {
		return new I32(this).bitAnd(parameter);
	}
	
	public I32 bitAnd(U16 parameter) {
		return new I32(this).bitAnd(parameter);
	}
	
	public U32 bitAnd(U32 parameter) {
		return new U32(this).bitAnd(parameter);
	}
	
	public U64 bitAnd(U64 parameter) {
		return new U64(this).bitAnd(parameter);
	}
	
	public UDATA bitAnd(UDATA parameter) {
		return new UDATA(this).bitAnd(parameter);
	}
	
	public I32 bitAnd(I8 parameter) {
		return new I32(this).bitAnd(parameter);
	}
	
	public I16 bitAnd(I16 parameter) {
		return new I16(data & parameter.data);
	}
	
	public I32 bitAnd(I32 parameter) {
		return new I32(this).bitAnd(parameter);
	}
	
	public I64 bitAnd(I64 parameter) {
		return new I64(this).bitAnd(parameter);
	}
	
	public IDATA bitAnd(IDATA parameter) {
		return new IDATA(this).bitAnd(parameter);
	}
	
	// leftShift
	public I16 leftShift(int i) {
		return new I16(data << i);
	}
	
	// rightShift
	public I16 rightShift(int i) {
		long newData = data >>> i;
		if ((data & 0x8000) != 0) {
			newData |= (0xFFFF << (Math.max(0, 16 - i)));
		}
		return new I16(newData);
	}
	
	// bitNot
	public I16 bitNot() {
		return new I16(~data);
	}
	
	public I16 mult(int parameter) {
		return new I16(data * parameter);
	}
	
	@Override
	public int sizeof()
	{
		return SIZEOF;
	}
}
