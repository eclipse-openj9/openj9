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
import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;

public class IDATA extends IScalar {
	// Constants
	public static final int SIZEOF = J9BuildFlags.env_data64 ? 8 : 4;;
	public static final long MASK = SIZEOF == 8 ? 0xFFFFFFFFFFFFFFFFL : 0xFFFFFFFFL;
	public static final IDATA MIN = SIZEOF == 8 ? new IDATA(0x8000000000000000L) : new IDATA(0x80000000L);
	public static final IDATA MAX = SIZEOF == 8 ? new IDATA(0x7FFFFFFFFFFFFFFFL) : new IDATA(0x7FFFFFFFL);

	// Constructors
	public IDATA(long value) {
		super(value & MASK);
	}
	
	public IDATA(Scalar parameter) {
		super(parameter);
	}
	
	// API

	// ADD
	
	public IDATA add(U8 parameter) {
		return add(new IDATA(parameter));
	}
	
	public IDATA add(U16 parameter) {
		return add(new IDATA(parameter));
	}
	
	public IDATA add(U32 parameter) {
		return add(new IDATA(parameter));
	}
	
	public boolean eq(U32 parameter) {
		return eq(new IDATA(parameter));
	}
	
	public UDATA add(UDATA parameter) {
		return new UDATA(this).add(parameter);
	}
	
	public boolean eq(UDATA parameter) {
		return new UDATA(this).eq(parameter);
	}
	
	public IDATA add(IScalar parameter) {
		return add(new IDATA(parameter));
	}
	
	public IDATA add(IDATA parameter) {
		return new IDATA(data + parameter.data);
	}
	
	public I64 add(I64 parameter) {
		return new I64(this).add(parameter);
	}
	
	//SUB
	
	public IDATA sub(U8 parameter) {
		return sub(new IDATA(parameter));
	}
	
	public IDATA sub(U16 parameter) {
		return sub(new IDATA(parameter));
	}
	
	public IDATA sub(U32 parameter) {
		return sub(new IDATA(parameter));
	}
	
	public UDATA sub(UDATA parameter) {
		return new UDATA(this).sub(parameter);
	}
	
	public IDATA sub(IScalar parameter) {
		return sub(new IDATA(parameter));
	}
	
	public IDATA sub(IDATA parameter) {
		return new IDATA(data - parameter.data);
	}
	
	public IDATA sub(long parameter) {
		return new IDATA(data - parameter);
	}
	
	public I64 sub(I64 parameter) {
		return new I64(this).sub(parameter);
	}

	public int intValue() {
		int value = (int) data;

		if (SIZEOF == I64.SIZEOF && data != value) {
			throw new InvalidDataTypeException("IDATA is 64 bits wide: conversion to int would lose data");
		}

		return value;
	}

	public long longValue() {
		if (SIZEOF == I32.SIZEOF) {
			return (int) data;
		} else {
			return data;
		}
	}
	
	// bitOr
	
	public IDATA bitOr(int parameter) {
		return new IDATA(data | parameter);
	}
	
	public IDATA bitOr(U8 parameter) {
		return bitOr(new IDATA(parameter));
	}
	
	public IDATA bitOr(U16 parameter) {
		return bitOr(new IDATA(parameter));
	}
	
	public IDATA bitOr(U32 parameter) {
		return bitOr(new IDATA(parameter));
	}
	
	public UDATA bitOr(UDATA parameter) {
		return new UDATA(this).bitOr(parameter);
	}
	
	public IDATA bitOr(IScalar parameter) {
		return bitOr(new IDATA(parameter));
	}
	
	public IDATA bitOr(IDATA parameter) {
		return new IDATA(data | parameter.data);
	}
	
	public I64 bitOr(I64 parameter) {
		return new I64(this).bitOr(parameter);
	}
	
	// bitXor
	
	public IDATA bitXor(int parameter) {
		return new IDATA(data ^ parameter);
	}
	
	public IDATA bitXor(long parameter) {
		return new IDATA(data ^ parameter);
	}

	public IDATA bitXor(Scalar parameter) {
		return bitXor(new IDATA(parameter));
	}
	
	public IDATA bitXor(IDATA parameter) {
		return new IDATA(data ^ parameter.data);
	}
	
	public UDATA bitXor(UDATA parameter) {
		return new UDATA(this).bitXor(parameter);
	}
	
	public I64 bitXor(I64 parameter) {
		return new I64(this).bitXor(parameter);
	}
	
	public U64 bitXor(U64 parameter) {
		return new U64(this).bitXor(parameter);
	}
	
	// bitAnd
	
	public IDATA bitAnd(int parameter) {
		return new IDATA(data & parameter);
	}
	
	public IDATA bitAnd(U8 parameter) {
		return bitAnd(new IDATA(parameter));
	}
	
	public IDATA bitAnd(U16 parameter) {
		return bitAnd(new IDATA(parameter));
	}
	
	public IDATA bitAnd(U32 parameter) {
		return bitAnd(new IDATA(parameter));
	}
	
	public UDATA bitAnd(UDATA parameter) {
		return new UDATA(this).bitAnd(parameter);
	}
	
	public IDATA bitAnd(IScalar parameter) {
		return bitAnd(new IDATA(parameter));
	}
	
	public IDATA bitAnd(IDATA parameter) {
		return new IDATA(data & parameter.data);
	}
	
	public I64 bitAnd(I64 parameter) {
		return new I64(this).bitAnd(parameter);
	}
	
	// leftShift
	public IDATA leftShift(int i) {
		return new IDATA(data << i);
	}
	
	public IDATA rightShift(int i) {
		if (SIZEOF == 4) {
			long newData = data >>> i;
			if ((data & 0x80000000) != 0) {
				newData |= (0xFFFFFFFF << (Math.max(0, 32 - i)));
			}
			return new IDATA(newData);
		} else {
			return new IDATA(data >> i);
		}
	}
	
	// bitNot
	public IDATA bitNot() {
		return new IDATA(~data);
	}
	
	public IDATA mult(int parameter) {
		return new IDATA(data * parameter);
	}
	
	/* mod */
	public IDATA mod(int parameter) {
		return mod((long) parameter);
	}
	
	public IDATA mod(long parameter) {
		return new IDATA(data % parameter);
	}
	
	public IDATA mod(Scalar parameter) {
		return mod(parameter.longValue());
	}
	
	public IDATA div(long parameter) {
		return new IDATA(data/parameter);
	}
	
	public IDATA div(Scalar parameter) {
		return div(parameter.longValue());
	}
	
	@Override
	public int sizeof()
	{
		return SIZEOF;
	}
	
	public static IDATA cast(AbstractPointer ptr)
	{
		if (ptr != null) {
			return new IDATA(ptr.getAddress());
		} else {
			return new IDATA(0);
		}
	}
}
