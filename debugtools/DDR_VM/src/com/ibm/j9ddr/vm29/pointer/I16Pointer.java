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

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.types.I16;
import com.ibm.j9ddr.vm29.types.Scalar;
import com.ibm.j9ddr.vm29.types.UDATA;

public class I16Pointer extends Pointer {

	public static final int SIZEOF = I16.SIZEOF;
	public static final I16Pointer NULL = new I16Pointer(0);

	// Do not call this constructor.  Use static method cast instead
	protected I16Pointer(long address) {
		super(address);
	}
	
	public static I16Pointer cast(AbstractPointer pointer) {
		return cast(pointer.getAddress());
	}
	
	public static I16Pointer cast(UDATA udata) {
		return cast(udata.longValue());
	}
	
	public static I16Pointer cast(long address) {
		if (address == 0) {
			return NULL;
		}
		return new I16Pointer(address);
	}
	
	public I16 at(long index) throws CorruptDataException {
		return new I16(getShortAtOffset(index * SIZEOF));
	}
	
	public I16 at(Scalar index) throws CorruptDataException {
		return at(index.longValue());
	}
	
	public I16Pointer untag() {
		return untag(SIZEOF - 1);
	}
	
	public I16Pointer untag(long mask) {
		return new I16Pointer(address & ~mask);
	}
	
	public I16Pointer add(long count) {
		return new I16Pointer(address + (SIZEOF * count));
	}
	
	public I16Pointer add(Scalar count) {
		return add(count.longValue());
	}
	
	public I16Pointer addOffset(long offset) {
		return new I16Pointer(address + offset);
	}
	
	public I16Pointer addOffset(Scalar offset) {
		return addOffset(offset.longValue());
	}

	@Override
	public I16Pointer sub(long count)
	{
		return new I16Pointer(address - (SIZEOF*count));
	}

	@Override
	public I16Pointer sub(Scalar count)
	{
		return sub(count.longValue());
	}

	@Override
	public I16Pointer subOffset(long offset)
	{
		return new I16Pointer(address - offset);
	}

	@Override
	public I16Pointer subOffset(Scalar offset)
	{
		return subOffset(offset.longValue());
	}

	@Override
	protected long sizeOfBaseType()
	{
		return SIZEOF;
	}
}
