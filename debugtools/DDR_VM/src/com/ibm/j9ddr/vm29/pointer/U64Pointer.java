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
import com.ibm.j9ddr.vm29.types.Scalar;
import com.ibm.j9ddr.vm29.types.U64;
import com.ibm.j9ddr.vm29.types.UDATA;

public class U64Pointer extends UDATAPointer {

	public static final int SIZEOF = U64.SIZEOF;
	public static final U64Pointer NULL = new U64Pointer(0);

	// Do not call this constructor.  Use static method cast instead
	protected U64Pointer(long address) {
		super(address);
	}
	
	public static U64Pointer cast(AbstractPointer pointer) {
		return cast(pointer.getAddress());
	}
	
	public static U64Pointer cast(UDATA udata) {
		return cast(udata.longValue());
	}
	
	public static U64Pointer cast(long address) {
		if (address == 0) {
			return NULL;
		}
		return new U64Pointer(address);
	}
	
	public U64 at(long index) throws CorruptDataException {
		return new U64(getLongAtOffset(index * SIZEOF));
	}
	
	public U64 at(Scalar index) throws CorruptDataException {
		return at(index.longValue());
	}
	
	public U64Pointer untag() {
		return untag(SIZEOF - 1);
	}
	
	public U64Pointer untag(long mask) {
		return new U64Pointer(address & ~mask);
	}
	
	public U64Pointer add(long count) {
		return new U64Pointer(address + (SIZEOF * count));
	}
	
	public U64Pointer add(Scalar count) {
		return add(count.longValue());
	}
	
	public U64Pointer addOffset(long offset) {
		return new U64Pointer(address + offset);
	}
	
	public U64Pointer addOffset(Scalar offset) {
		return addOffset(offset.longValue());
	}
	
	@Override
	public U64Pointer sub(long count)
	{
		return new U64Pointer(address - (SIZEOF*count));
	}

	@Override
	public U64Pointer sub(Scalar count)
	{
		return sub(count.longValue());
	}

	@Override
	public U64Pointer subOffset(long offset)
	{
		return new U64Pointer(address - offset);
	}

	@Override
	public U64Pointer subOffset(Scalar offset)
	{
		return subOffset(offset.longValue());
	}
	
	@Override
	protected long sizeOfBaseType()
	{
		return SIZEOF;
	}
}
