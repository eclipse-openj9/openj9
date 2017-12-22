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
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.types.Scalar;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.UDATA;

public class ObjectReferencePointer extends Pointer
{
	public static final ObjectReferencePointer NULL = new ObjectReferencePointer(0);
	public static final long SIZEOF = J9BuildFlags.gc_compressedPointers ? U32.SIZEOF : UDATA.SIZEOF;
	
	protected ObjectReferencePointer(long address)
	{
		super(address);
	}
	
	// Construction Methods
	public static ObjectReferencePointer cast(AbstractPointer pointer) {
		return cast(pointer.getAddress());
	}
	
	public static ObjectReferencePointer cast(UDATA udata) {
		return cast(udata.longValue());
	}
	
	public static ObjectReferencePointer cast(long address) {
		if (address == 0) {
			return NULL;
		}
		return new ObjectReferencePointer(address);
	}
	
	@Override
	public ObjectReferencePointer add(long count)
	{
		return new ObjectReferencePointer(address + (SIZEOF * count));
	}

	@Override
	public ObjectReferencePointer add(Scalar count)
	{
		return add(count.longValue());
	}

	@Override
	public ObjectReferencePointer addOffset(long offset)
	{
		return new ObjectReferencePointer(address + offset);
	}

	@Override
	public ObjectReferencePointer addOffset(Scalar offset)
	{
		return addOffset(offset.longValue());
	}

	@Override
	public J9ObjectPointer at(long index) throws CorruptDataException
	{
		return getObjectReferenceAtOffset(index * SIZEOF);
	}

	@Override
	public J9ObjectPointer at(Scalar index) throws CorruptDataException
	{
		return at(index.longValue());
	}

	@Override
	protected long sizeOfBaseType()
	{
		return SIZEOF;
	}

	@Override
	public ObjectReferencePointer sub(long count)
	{
		return new ObjectReferencePointer(address - (SIZEOF*count));
	}

	@Override
	public ObjectReferencePointer sub(Scalar count)
	{
		return sub(count.longValue());
	}

	@Override
	public ObjectReferencePointer subOffset(long offset)
	{
		return new ObjectReferencePointer(address - offset);
	}

	@Override
	public ObjectReferencePointer subOffset(Scalar offset)
	{
		return subOffset(offset.longValue());
	}

	@Override
	public ObjectReferencePointer untag(long tagBits)
	{
		return new ObjectReferencePointer(address & ~tagBits);
	}

	@Override
	public ObjectReferencePointer untag()
	{
		return untag(SIZEOF - 1);
	}

}
