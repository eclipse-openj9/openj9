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

import java.lang.reflect.Field;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.types.Scalar;
import com.ibm.j9ddr.vm29.types.UDATA;

import java.lang.NoSuchFieldException;;

public class EnumPointer extends Pointer {

	public static final EnumPointer NULL = new EnumPointer(0, VoidPointer.class);
	private Class<?> enumClazz;
	private long enumSize = -1;
	
	// Do not call this constructor.  Use static method cast instead
	protected EnumPointer(long address, Class<?> enumClazz) {
		super(address);
		this.enumClazz = enumClazz;
	}

	public static EnumPointer cast(AbstractPointer pointer, Class<?> enumClazz) {
		return cast(pointer.getAddress(), enumClazz);
	}

	public static EnumPointer cast(UDATA udata, Class<?> enumClazz) {
		return cast(udata.longValue(), enumClazz);
	}
	
	public static EnumPointer cast(long address, Class<?> enumClazz) {
		if (address == 0) {
			return NULL;
		}
		return new EnumPointer(address, enumClazz);
	}
	
	public EnumPointer at(long index) throws CorruptDataException {
		throw new UnsupportedOperationException("Use longAt(long index)");
	}
	
	public EnumPointer at(Scalar index) throws CorruptDataException {
		return at(index.longValue());
	}
	
	public long longAt(long index) throws CorruptDataException {
		if (sizeOfBaseType() == 1) {
			return getByteAtOffset(index * sizeOfBaseType());
		} else if (sizeOfBaseType() == 2) {
			return getShortAtOffset(index * sizeOfBaseType());
		} else if (sizeOfBaseType() == 4) {
			return getIntAtOffset(index * sizeOfBaseType());
		} else if (sizeOfBaseType() == 8) {
			return getLongAtOffset(index * sizeOfBaseType());
		} else {
			throw new IllegalArgumentException("Unexpected ENUM size in core file");
		}
	}
	
	@Override
	protected long sizeOfBaseType() {
		if (enumSize == -1) {
			try {
				Field field = enumClazz.getField("SIZEOF");
				enumSize = field.getLong(null);
			} catch (NoSuchFieldException e) {
				throw new IllegalArgumentException(e.getMessage());
			} catch (IllegalAccessException e) {
				throw new IllegalArgumentException(e.getMessage());
			} 
		}
		return enumSize;
	}

	public double doubleAt(long index) throws CorruptDataException {
		return longAt(index);
	}
	
	public float floatAt(long index) throws CorruptDataException {
		return longAt(index);
	}
	
	public EnumPointer add(long count) {
		return new EnumPointer(address + (sizeOfBaseType() * count), enumClazz);
	}
	
	public EnumPointer add(Scalar count) {
		return add(count.longValue());
	}

	public EnumPointer addOffset(long offset) {
		return new EnumPointer(address + offset, enumClazz);
	}
	
	public EnumPointer addOffset(Scalar offset) {
		return addOffset(offset.longValue());
	}

	public EnumPointer sub(long count)
	{
		return new EnumPointer(address - (sizeOfBaseType() * count), enumClazz);
	}

	public EnumPointer sub(Scalar count)
	{
		return sub(count.longValue());
	}

	public EnumPointer subOffset(long offset)
	{
		return new EnumPointer(address - offset, enumClazz);
	}

	public EnumPointer subOffset(Scalar offset)
	{
		return subOffset(offset.longValue());
	}

	public EnumPointer untag() {
		return untag(SIZEOF - 1);
	}
	
	public EnumPointer untag(long mask) {
		return new EnumPointer(address & ~mask, enumClazz);
	}
}
