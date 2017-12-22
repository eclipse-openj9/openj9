/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.types.Scalar;
import com.ibm.j9ddr.vm29.types.UDATA;

public class BoolPointer extends Pointer {

	public static final int SIZEOF = DataType.SIZEOF_BOOL; 
	public static final BoolPointer NULL = new BoolPointer(0);
	
	// Do not call this constructor.  Use static method cast instead
	protected BoolPointer(long address) {
		super(address);
	}

	public static BoolPointer cast(AbstractPointer pointer) {
		return cast(pointer.getAddress());
	}
	
	public static BoolPointer cast(UDATA udata) {
		return cast(udata.longValue());
	}
	
	public static BoolPointer cast(long address) {
		if (address == 0) {
			return NULL;
		}
		return new BoolPointer(address);
	}

	public boolean boolAt(long index) throws CorruptDataException {
		return getBoolAtOffset(SIZEOF * index);
	}
	
	public BoolPointer at(long index) throws CorruptDataException {
		throw new UnsupportedOperationException("Use boolAt(long index)");
	}
	
	public BoolPointer at(Scalar index) throws CorruptDataException {
		return at(index.longValue());
	}
	
	public BoolPointer add(long count) {
		return new BoolPointer(address + (SIZEOF * count));
	}
	
	public BoolPointer add(Scalar count) {
		return add(count.longValue());
	}

	public BoolPointer addOffset(long offset) {
		return new BoolPointer(address + offset);
	}
	
	public BoolPointer addOffset(Scalar offset) {
		return addOffset(offset.longValue());
	}

	public BoolPointer sub(long count)
	{
		return new BoolPointer(address - (SIZEOF*count));
	}

	public BoolPointer sub(Scalar count)
	{
		return sub(count.longValue());
	}

	public BoolPointer subOffset(long offset)
	{
		return new BoolPointer(address - offset);
	}

	public BoolPointer subOffset(Scalar offset)
	{
		return subOffset(offset.longValue());
	}

	public BoolPointer untag() {
		throw new UnsupportedOperationException("Use BoolPointer.untag(long mask) insetead");
	}
	
	public BoolPointer untag(long mask) {
		return new BoolPointer(address & ~mask);
	}
	
	@Override
	protected long sizeOfBaseType()
	{
		return SIZEOF;
	}
}
