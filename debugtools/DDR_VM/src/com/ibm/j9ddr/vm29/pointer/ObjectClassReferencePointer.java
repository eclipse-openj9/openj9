/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.types.Scalar;
import com.ibm.j9ddr.vm29.types.UDATA;

public class ObjectClassReferencePointer extends Pointer
{
	public static final ObjectClassReferencePointer NULL = new ObjectClassReferencePointer(0);
	public static final long SIZEOF = J9BuildFlags.gc_compressedPointers ? U32.SIZEOF : UDATA.SIZEOF;
	
	protected ObjectClassReferencePointer(long address)
	{
		super(address);
	}
	
	// Construction Methods
	public static ObjectClassReferencePointer cast(AbstractPointer pointer) {
		return cast(pointer.getAddress());
	}
	
	public static ObjectClassReferencePointer cast(UDATA udata) {
		return cast(udata.longValue());
	}
	
	public static ObjectClassReferencePointer cast(long address) {
		if (address == 0) {
			return NULL;
		}
		return new ObjectClassReferencePointer(address);
	}

	@Override
	public ObjectClassReferencePointer add(long count)
	{
		return new ObjectClassReferencePointer(address + (SIZEOF * count));
	}

	@Override
	public ObjectClassReferencePointer add(Scalar count)
	{
		return add(count.longValue());
	}

	@Override
	public ObjectClassReferencePointer addOffset(long offset)
	{
		return new ObjectClassReferencePointer(address + offset);
	}

	@Override
	public ObjectClassReferencePointer addOffset(Scalar offset)
	{
		return addOffset(offset.longValue());
	}

	@Override
	public J9ClassPointer at(long index) throws CorruptDataException
	{
		if(J9BuildFlags.gc_compressedPointers) {
			return J9ClassPointer.cast(0xFFFFFFFFL & (long)getIntAtOffset(index * SIZEOF));
		} else {
			return J9ClassPointer.cast(getUDATAAtOffset(SIZEOF * index));
		}
	}

	@Override
	public J9ClassPointer at(Scalar index) throws CorruptDataException
	{
		return at(index.longValue());
	}

	@Override
	protected long sizeOfBaseType()
	{
		return SIZEOF;
	}

	@Override
	public ObjectClassReferencePointer sub(long count)
	{
		return new ObjectClassReferencePointer(address - (SIZEOF*count));
	}

	@Override
	public ObjectClassReferencePointer sub(Scalar count)
	{
		return sub(count.longValue());
	}

	@Override
	public ObjectClassReferencePointer subOffset(long offset)
	{
		return new ObjectClassReferencePointer(address - offset);
	}

	@Override
	public ObjectClassReferencePointer subOffset(Scalar offset)
	{
		return subOffset(offset.longValue());
	}

	@Override
	public ObjectClassReferencePointer untag(long tagBits)
	{
		return new ObjectClassReferencePointer(address & ~tagBits);
	}

	@Override
	public ObjectClassReferencePointer untag()
	{
		return untag(SIZEOF - 1);
	}

}
