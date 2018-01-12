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

import java.io.UnsupportedEncodingException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.types.Scalar;
import com.ibm.j9ddr.vm29.types.U8;
import com.ibm.j9ddr.vm29.types.UDATA;

public class U8Pointer extends Pointer {

	public static final int SIZEOF = U8.SIZEOF;
	public static final U8Pointer NULL = new U8Pointer(0);

	// Do not call this constructor.  Use static method cast instead
	protected U8Pointer(long address) {
		super(address);
	}
	
	public static U8Pointer cast(AbstractPointer pointer) {
		return cast(pointer.getAddress());
	}
	
	public static U8Pointer cast(UDATA udata) {
		return cast(udata.longValue());
	}
	
	public static U8Pointer cast(long address) {
		if (address == 0) {
			return NULL;
		}
		return new U8Pointer(address);
	}
	
	public U8 at(long index) throws CorruptDataException {
		return new U8(getByteAtOffset(index * SIZEOF));
	}
	
	public U8 at(Scalar index) throws CorruptDataException {
		return at(index.longValue());
	}
	
	public U8Pointer untag() {
		throw new UnsupportedOperationException("Use UPointer.untag(long mask) instead.");
	}
	
	public U8Pointer untag(long mask) {
		return new U8Pointer(address & ~mask);
	}
	
	public U8Pointer add(long count) {
		return new U8Pointer(address + (SIZEOF * count));
	}
	
	public U8Pointer add(Scalar count) {
		return add(count.longValue());
	}
	
	public U8Pointer addOffset(long offset) {
		return new U8Pointer(address + offset);
	}
	
	public U8Pointer addOffset(Scalar offset) {
		return addOffset(offset.longValue());
	}
	
	public String getCStringAtOffset(long offset) throws CorruptDataException {
		return getCStringAtOffset(offset, Long.MAX_VALUE);
	}
	
	public String getCStringAtOffset(long offset, long maxLength) throws CorruptDataException {
		int length = 0;
		while(0 != getByteAtOffset(offset + length) && length < maxLength) {
			length++;
		}
		byte[] buffer = new byte[length];
		getBytesAtOffset(offset, buffer);
		
		try {
			return new String(buffer,"UTF-8");
		} catch (UnsupportedEncodingException e) {
			throw new RuntimeException(e);
		}
	}
	
	@Override
	public U8Pointer sub(long count)
	{
		return new U8Pointer(address - (SIZEOF*count));
	}

	@Override
	public U8Pointer sub(Scalar count)
	{
		return sub(count.longValue());
	}

	@Override
	public U8Pointer subOffset(long offset)
	{
		return new U8Pointer(address - offset);
	}

	@Override
	public U8Pointer subOffset(Scalar offset)
	{
		return subOffset(offset.longValue());
	}

	@Override
	protected long sizeOfBaseType()
	{
		return SIZEOF;
	}
	
}
