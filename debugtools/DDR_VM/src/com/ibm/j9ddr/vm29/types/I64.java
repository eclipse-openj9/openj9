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

public class I64 extends IDATA {
	
	// Constants
	public static final int SIZEOF = 8;
	public static final long MASK = 0xFFFFFFFFFFFFFFFFL;
	public static final I64 MIN = new I64(0x8000000000000000L);
	public static final I64 MAX = new I64(0x7FFFFFFFFFFFFFFFL);

	// Constructors
	public I64(long value) {
		super(value);
		// When working with 32-bit core files, the constructor for IDATA
		// will truncate the value to 32 bits: we need all 64-bits here.
		this.data = value;
	}
	
	public I64(Scalar parameter) {
		super(parameter);
	}

	// API
	
	// ADD
	public I64 add(int number) {
		return new I64(data + number);
	}

	public I64 add(long number) {
		return new I64(data + number);
	}

	public I64 add(U8 parameter) {
		return add(new I64(parameter));
	}
	
	public I64 add(U16 parameter) {
		return add(new I64(parameter));
	}
	
	public I64 add(U32 parameter) {
		return add(new I64(parameter));
	}
	
	public U64 add(U64 parameter) {
		return new U64(this).add(new I64(parameter));
	}
	
	public boolean eq(U64 parameter) {
		return new U64(this).eq(new I64(parameter));
	}
	
	public I64 add(I8 parameter) {
		return add(new I64(parameter));
	}
	
	public I64 add(I16 parameter) {
		return add(new I64(parameter));
	}
	
	public I64 add(I32 parameter) {
		return add(new I64(parameter));
	}
	
	public I64 add(I64 parameter) {
		return new I64(data + parameter.data);
	}
	
	public I64 add(IDATA parameter) {
		return add(new I64(parameter));
	}
	
	//SUB
	public I64 sub(int number) {
		return new I64(data - number);
	}

	public I64 sub(long number) {
		return new I64(data - number);
	}

	public I64 sub(U8 parameter) {
		return sub(new I64(parameter));
	}
	
	public I64 sub(U16 parameter) {
		return sub(new I64(parameter));
	}
	
	public I64 sub(U32 parameter) {
		return sub(new I64(parameter));
	}
	
	public U64 sub(U64 parameter) {
		return new U64(this).sub(new I64(parameter));
	}

	public I64 sub(I8 parameter) {
		return sub(new I64(parameter));
	}
	
	public I64 sub(I16 parameter) {
		return sub(new I64(parameter));
	}
	
	public I64 sub(I32 parameter) {
		return sub(new I64(parameter));
	}
	
	public I64 sub(I64 parameter) {
		return new I64(data - parameter.data);
	}
	
	public I64 sub(IDATA parameter) {
		return sub(new I64(parameter));
	}

	public int intValue() {
		int value = (int) data;

		if (data != value) {
			throw new InvalidDataTypeException("I64: conversion to int would lose data");
		}

		return value;
	}

	public long longValue() {
		// When working with 32-bit core files, IDATA.longValue() will
		// truncate the value to 32 bits: we must return all 64-bits here.
		return data;
	}

	// bitOr

	public I64 bitOr(int number) {
		return new I64(data | number);
	}
	
	public I64 bitOr(long number) {
		return new I64(data | number);
	}
	
	public I64 bitOr(U8 parameter) {
		return bitOr(new I64(parameter));
	}
	
	public I64 bitOr(U16 parameter) {
		return bitOr(new I64(parameter));
	}
	
	public I64 bitOr(U32 parameter) {
		return bitOr(new I64(parameter));
	}
	
	public U64 bitOr(U64 parameter) {
		return new U64(this).bitOr(new I64(parameter));
	}
	
	public I64 bitOr(I8 parameter) {
		return bitOr(new I64(parameter));
	}
	
	public I64 bitOr(I16 parameter) {
		return bitOr(new I64(parameter));
	}
	
	public I64 bitOr(I32 parameter) {
		return bitOr(new I64(parameter));
	}
	
	public I64 bitOr(I64 parameter) {
		return new I64(data | parameter.data);
	}
	
	public I64 bitOr(IDATA parameter) {
		return bitOr(new I64(parameter));
	}
	
	// bitXor
	
	public I64 bitXor(int number) {
		return new I64(data ^ number);
	}
	
	public I64 bitXor(long number) {
		return new I64(data ^ number);
	}
		
	public I64 bitXor(Scalar parameter) {
		return bitXor(new I64(parameter));
	}
	
	public I64 bitXor(I64 parameter) {
		return new I64(data ^ parameter.data);
	}
	
	public U64 bitXor(U64 parameter) {
		return new U64(this).bitXor(parameter);
	}
	
	// bitAnd
	
	public I64 bitAnd(int number) {
		return new I64(data & number);
	}
	
	public I64 bitAnd(long number) {
		return new I64(data & number);
	}
	
	public I64 bitAnd(U8 parameter) {
		return bitAnd(new I64(parameter));
	}
	
	public I64 bitAnd(U16 parameter) {
		return bitAnd(new I64(parameter));
	}
	
	public I64 bitAnd(U32 parameter) {
		return bitAnd(new I64(parameter));
	}
	
	public U64 bitAnd(U64 parameter) {
		return new U64(this).bitAnd(new I64(parameter));
	}
	
	public I64 bitAnd(I8 parameter) {
		return bitAnd(new I64(parameter));
	}
	
	public I64 bitAnd(I16 parameter) {
		return bitAnd(new I64(parameter));
	}
	
	public I64 bitAnd(I32 parameter) {
		return bitAnd(new I64(parameter));
	}
	
	public I64 bitAnd(I64 parameter) {
		return new I64(data & parameter.data);
	}
	
	public I64 bitAnd(IDATA parameter) {
		return bitAnd(new I64(parameter));
	}
	
	// leftShift
	public I64 leftShift(int i) {
		return new I64(data << i);
	}
	
	// rightShift
	public I64 rightShift(int i) {
		return new I64(data >> i);
	}
	
	// bitNot
	public I64 bitNot() {
		return new I64(~data);
	}
	
	public I64 mult(int parameter) {
		return new I64(data * parameter);
	}

	public I64 mult(long parameter) {
		return new I64(data * parameter);
	}

	public boolean lt(I64 parameter)
	{
		return this.data < parameter.data;
	}
	
	public boolean gt(I64 parameter)
	{
		return this.data > parameter.data;
	}
	
	@Override
	public int sizeof()
	{
		return SIZEOF;
	}
}
