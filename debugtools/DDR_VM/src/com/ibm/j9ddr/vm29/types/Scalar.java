/*******************************************************************************
 * Copyright (c) 2001, 2015 IBM Corp. and others
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

import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.pointer.ObjectReferencePointer;

public abstract class Scalar extends DataType {
	protected long data;
	protected static final int bitsPerBytes = 8;
	protected static final int bitsPerLong = 64;
	
	public Scalar(long value) {
		super();
		this.data = value;
	}
	
	public Scalar(Scalar value) {
		this(value.data);
		
		if (value.sizeof() > this.sizeof()) {
			trimExcessBytes();
		} else if (value.sizeof() < this.sizeof()){
			signExtend(value);
		}
	}

	public Scalar() {
		super();
	}
	
	public byte byteValue() {
		return (byte)data;
	}
	
	public short shortValue() {
		return (short)data;
	}
	
	public int intValue() {
		return (int) data;
	}
	
	public long longValue() {
		return data;
	}
	
	public String getHexValue() {
		return String.format("0x%0" + sizeof() * 2 + "X", data);
	}
	
	protected String toStringPattern;

	@Override
	public String toString() 
	{
		if (null == toStringPattern) {
			toStringPattern = getClass().getSimpleName() + "(0x%0" + (sizeof() * 2) + "X)";
		}
		return String.format(toStringPattern, data);
	}
	
	/**
	 * This is a class based equals.  Objects can only be equal if they are the same class.
	 * It is meant for use with Hash based collections.  I.E.  U16.equals(I16) is never true, regardless
	 * of the values represented by the objects.
	 * 
	 * For mathematical equality use .eq(Scalar)
	 */
	public boolean equals(Object parameter) {
		if (parameter == null) return false;
		if (!parameter.getClass().isInstance(this)) { 
			return false;
		}
		return data == ((Scalar) parameter).data;
	}
	
	public boolean eq(Scalar parameter) {
		if (parameter instanceof U64) {
			return parameter.eq(this);
		} else {
			return parameter.longValue() == longValue();
		}
	}

	public boolean eq(long parameter) {
		return parameter == longValue();
	}
	
	public int hashCode() {
		return (int) data;
	}
	
	protected void checkComparisonValid(Scalar parameter)
	{
		if (this.isSigned() ^ parameter.isSigned()) {
			throw new UnsupportedOperationException("Can't compare signed and unsigned Scalars");
		}
	}
	
	public boolean gt(int parameter)
	{
		return gt(new I32(parameter));
	}
	
	public boolean gt(long parameter)
	{
		return gt(new I64(parameter));
	}
	
	public boolean lt(int parameter)
	{
		return lt(new I32(parameter));
	}
	
	public boolean lt(long parameter)
	{
		return lt(new I64(parameter));
	}
	
	public boolean isZero() {
		return eq(0);
	}
	
	//lt & gt are specialised on UScalar to cope with 64 bit unsigned values
	public boolean gt(Scalar parameter) {
		checkComparisonValid(parameter);
		
		return longValue() > parameter.longValue();
	}
	
	public boolean gte(Scalar parameter) {
		return this.eq(parameter) || this.gt(parameter);
	}
	
	public boolean lt (Scalar parameter) {
		checkComparisonValid(parameter);
		
		return longValue() < parameter.longValue();
	}
	
	public boolean lte(Scalar parameter) {
		return this.eq(parameter) || this.lt(parameter);
	}
	
	public static UDATA convertBytesToSlots(UDATA size) 
	{
		if(4 == UDATA.SIZEOF) {
			return size.rightShift(2);
		} else {
			return size.rightShift(3);
		}
	}

	public static UDATA convertSlotsToBytes(UDATA size) 
	{
		if(4 == UDATA.SIZEOF) {
			return size.leftShift(2);
		} else {
			return size.leftShift(3);
		}
	}

	public static UDATA roundToSizeofUDATA(UDATA value)
	{
		return roundTo(value, UDATA.SIZEOF);
	}
	
	public static UDATA roundToSizeToObjectReference(UDATA value) 
	{		
		return roundTo(value, ObjectReferencePointer.SIZEOF);
	}
	
	public static UDATA roundToSizeofU32(UDATA value)
	{
		return roundTo(value, U32.SIZEOF);
	}
	
	protected static UDATA roundTo(UDATA value, long size) 
	{
		long result = (value.longValue() + (size - 1)) & ~(size - 1);
		return new UDATA(result);
	}

	public boolean allBitsIn(long bitmask) {
		return bitmask == (data & bitmask);
	}
	
	public boolean anyBitsIn(long bitmask) {
		return 0 != (data & bitmask);
	}
	
	public boolean maskAndCompare(long bitmask, long compareValue) {
		return compareValue == (data & bitmask);
	}
	
	/**
	 * Used when casting a larger type to a smaller one.
	 */
	private void trimExcessBytes()
	{
		int bytesToEndOfLong = 8 - sizeof();
		int bitsToShift = 8 * bytesToEndOfLong;
		
		data = (data<<bitsToShift)>>>bitsToShift;
	}
	
	/**
	 * Called when constructing a type from another type.
	 * 
	 * If the sourceType is signed and smaller than this type, we'll sign
	 * extend where necessary
	 * @param sourceType Value passed in
	 */
	private void signExtend(Scalar sourceType) {
		if (sourceType.isSigned() && sourceType.signBitSet()) {
			int bytesToEndOfLong = 8 - sourceType.sizeof();
			int bytesDifference = sizeof() - sourceType.sizeof();
			//Left shift to align the source data with the left hand side of the double-word
			data = data << 8*bytesToEndOfLong;
			//Right signed shift to do the necessary amount of sign-extending
			data = data >> 8*bytesDifference;
			//Right unsigned shift to move the data back to the right of the double-word.
			data = data >>> 8*(bytesToEndOfLong - bytesDifference);
		}
	}
	
	protected boolean signBitSet() {
		return data >>> ((sizeof() * 8) - 1) != 0;
	}
	
	public abstract int sizeof();
	
	public abstract boolean isSigned();
}
