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
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.types.Scalar;
import com.ibm.j9ddr.vm29.types.UDATA;

public class PointerPointer extends Pointer {

	public static final PointerPointer NULL = new PointerPointer(0);

	// Do not call this constructor.  Use static method cast instead
	protected PointerPointer(long address) {
		super(address);
	}

	public static PointerPointer cast(AbstractPointer pointer) {
		return cast(pointer.getAddress());
	}
	
	public static PointerPointer cast(UDATA udata) {
		return cast(udata.longValue());
	}
	
	public static PointerPointer cast(long address) {
		if (address == 0) {
			return NULL;
		}
		return new PointerPointer(address);
	}
	
	public VoidPointer at(long index) throws CorruptDataException {
		return new VoidPointer(getPointerAtOffset(index * SIZEOF));
	}
	
	public VoidPointer at(Scalar index) throws CorruptDataException {
		return at(index.longValue());
	}
	
	public PointerPointer untag() {
		return untag(SIZEOF - 1);
	}
	
	public PointerPointer untag(long mask) {
		return new PointerPointer(address & ~mask);
	}
	
	public PointerPointer add(long count) {
		return new PointerPointer(address + (SIZEOF * count));
	}
	
	public PointerPointer add(Scalar count) {
		return add(count.longValue());
	}
	
	public PointerPointer addOffset(long offset) {
		return new PointerPointer(address + offset);
	}
	
	public PointerPointer addOffset(Scalar offset) {
		return addOffset(offset.longValue());
	}
	
	@Override
	public PointerPointer sub(long count)
	{
		return new PointerPointer(address - (SIZEOF*count));
	}

	@Override
	public PointerPointer sub(Scalar count)
	{
		return sub(count.longValue());
	}

	@Override
	public PointerPointer subOffset(long offset)
	{
		return new PointerPointer(address - offset);
	}

	@Override
	public PointerPointer subOffset(Scalar offset)
	{
		return subOffset(offset.longValue());
	}
	
	@Override
	protected long sizeOfBaseType()
	{
		return DataType.process.bytesPerPointer();
	}
}
