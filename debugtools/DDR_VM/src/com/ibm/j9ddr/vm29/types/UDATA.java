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

import java.math.BigInteger;

import com.ibm.j9ddr.InvalidDataTypeException;
import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;

public class UDATA extends UScalar {

	// Constants
	public static final int SIZEOF = J9BuildFlags.env_data64 ? 8 : 4;
	public static final long MASK = SIZEOF == 8 ? 0xFFFFFFFFFFFFFFFFL : 0xFFFFFFFFL;
	public static final UDATA MIN = new UDATA(0L);
	public static final UDATA MAX = SIZEOF == 8 ? new UDATA(0xFFFFFFFFFFFFFFFFL) : new UDATA(0xFFFFFFFFL);

	public UDATA(long value) {
		super(value & MASK);
	}
	
	public UDATA(Scalar parameter) {
		super(parameter);
	}
	
	// Add
	
	public UDATA add(long parameter) {
		return new UDATA(data + parameter);
	}
	
	public UDATA add(UScalar parameter) {
		return new UDATA(data + parameter.data);
	}
	
	public U64 add(U64 parameter) {
		return new U64(this).add(parameter);
	}
	
	public UDATA add(IScalar parameter) {
		return this.add(new UDATA(parameter));
	}

	public boolean eq(IScalar parameter) {
		return eq(new UDATA(parameter));
	}
	
	// Subtract
	
	public UDATA sub(long parameter) {
		return new UDATA(data - parameter);
	}
	
	public UDATA sub(UScalar parameter) {
		return new UDATA(data - parameter.data);
	}
	
	public U64 sub(U64 parameter) {
		return new U64(this).sub(parameter);
	}

	public UDATA sub(I8 parameter) {
		return this.sub(new UDATA(parameter));
	}
	
	public UDATA sub(I16 parameter) {
		return this.sub(new UDATA(parameter));
	}
	
	public UDATA sub(I32 parameter) {
		return this.sub(new UDATA(parameter));
	}
	
	public UDATA sub(IDATA parameter) {
		return this.sub(new UDATA(parameter));
	}

	public int intValue() {
		long value = data;

		if (value < 0 || value > Integer.MAX_VALUE) {
			throw new InvalidDataTypeException("UDATA contains value larger than Integer.MAX_VALUE");
		}

		return (int) value;
	}
	
	// bitOr

	public UDATA bitOr(int parameter) {
		return new UDATA(data | parameter);
	}
	
	public UDATA bitOr(long parameter) {
		return new UDATA(data | parameter);
	}
	
	public UDATA bitOr(UScalar parameter) {
		return new UDATA(data | parameter.data);
	}
	
	public U64 bitOr(U64 parameter) {
		return new U64(this).bitOr(parameter);
	}
	
	public UDATA bitOr(I8 parameter) {
		return this.bitOr(new UDATA(parameter));
	}
	
	public UDATA bitOr(I16 parameter) {
		return this.bitOr(new UDATA(parameter));
	}
	
	public UDATA bitOr(I32 parameter) {
		return this.bitOr(new UDATA(parameter));
	}
	
	public UDATA bitOr(IDATA parameter) {
		return this.bitOr(new UDATA(parameter));
	}
	
	// bitXor

	public UDATA bitXor(int parameter) {
		return new UDATA(data ^ parameter);
	}
	
	public UDATA bitXor(long parameter) {
		return new UDATA(data ^ parameter);
	}

	public UDATA bitXor(Scalar parameter) {
		return bitXor(new UDATA(parameter));
	}
	
	public UDATA bitXor(UDATA parameter) {
		return new UDATA(data ^ parameter.data);
	}
	
	public I64 bitXor(I64 parameter) {
		return new I64(this).bitXor(parameter);
	}
	
	public U64 bitXor(U64 parameter) {
		return new U64(this).bitXor(parameter);
	}
	// bitAnd
	
	public UDATA bitAnd(int parameter) {
		return new UDATA(data & parameter);
	}
	
	public UDATA bitAnd(long parameter) {
		return new UDATA(data & parameter);
	}
	
	public UDATA bitAnd(UScalar parameter) {
		return new UDATA(data & parameter.data);
	}
	
	public U64 bitAnd(U64 parameter) {
		return new U64(this).bitAnd(parameter);
	}
	
	public UDATA bitAnd(I8 parameter) {
		return this.bitAnd(new UDATA(parameter));
	}
	
	public UDATA bitAnd(I16 parameter) {
		return this.bitAnd(new UDATA(parameter));
	}
	
	public UDATA bitAnd(I32 parameter) {
		return this.bitAnd(new UDATA(parameter));
	}
	
	public UDATA bitAnd(IDATA parameter) {
		return this.bitAnd(new UDATA(parameter));
	}
	
	// leftShift
	public UDATA leftShift(int i) {
		return new UDATA(data << i);
	}
	
	public UDATA leftShift(UDATA i) {
		return new UDATA(data << i.longValue());
	}
	
	// rightShift
	public UDATA rightShift(int i) {
		return new UDATA(data >>> i);
	}
	
	public UDATA rightShift(UDATA i) {
		return new UDATA(data >>> i.longValue());
	}
	
	// bitNot
	public UDATA bitNot() {
		return new UDATA(~data);
	}
	
	public UDATA mult(int parameter) {
		return new UDATA(data * parameter);
	}
	
	public UDATA mult(long parameter) {
		return new UDATA(data * parameter);
	}
	
	public UDATA mult(UDATA parameter) {
		return mult(parameter.longValue());
	}

	/* mod */
	public UDATA mod(int parameter) {
		return mod((long) parameter);
	}
	
	public UDATA mod(long parameter) {
		/*
		 * In case when data in native udata is larger than what we can represent with java's long
		 * we need to do math differently.
		 * C % X = L  =>  (A + B) % X = L => (A % X + B % X) % X = L
		 */
		if(parameter < 0) {
			throw new IllegalArgumentException("mod by negative number is not supported, yet");
		}
		if(data < 0 && 8 == sizeof()) {
			long lowBit = data & 0x1;
			long half = data >>> 1;			
			long result = ((half % parameter) * 2 + lowBit) % parameter;
			return new UDATA(result);
		} else {		
			return new UDATA(data % parameter);
		}
	}
	
	public UDATA mod(Scalar parameter) {
		return mod(parameter.longValue());
	}
	
	public UDATA div(long divisor) 
	{
		long dividend = data;
		// optimization for longs that don't have left-most bit set.
		if (divisor >= 0 && dividend >= 0) {
			return new UDATA(dividend / divisor);
		} else {
			// TODO:Iouri Goussev this might be slow but we don't expect this case to happen often.
			BigInteger x = new BigInteger(Long.toHexString(dividend), 16);
			BigInteger y = new BigInteger(Long.toHexString(divisor), 16);
			return new UDATA(x.divide(y).longValue());
		}
	}
	
	public UDATA div(Scalar parameter) {
		return div(parameter.longValue());
	}
	
	public int numberOfLeadingZeros()
	{
		int leadingZeroes = Long.numberOfLeadingZeros(data);
		int extraLeadingZeroes = Scalar.bitsPerLong - (UDATA.SIZEOF * Scalar.bitsPerBytes);
		
		return leadingZeroes - extraLeadingZeroes;
	}
	
	public int numberOfTrailingZeros()
	{
		if (0 == data) {
			return UDATA.SIZEOF * Scalar.bitsPerBytes;
		} else {
			int trailingZeroes = Long.numberOfTrailingZeros(data);
			return trailingZeroes;
		}
	}

	public static UDATA cast(AbstractPointer ptr)
	{
		if (ptr != null) {
			return new UDATA(ptr.getAddress());
		} else {
			return new UDATA(0);
		}
	}
	
	@Override
	public int sizeof()
	{
		return SIZEOF;
	}
}
