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
import com.ibm.j9ddr.NullPointerDereference;
import com.ibm.j9ddr.vm29.types.Scalar;
import com.ibm.j9ddr.vm29.types.UDATA;

public class CorruptPointer extends AbstractPointer {
	public static final long SIZEOF = process.bytesPerPointer();
	
	public static final CorruptPointer NULL = new CorruptPointer(0);
	
	private String message;

	protected CorruptPointer(long address) {
		super(address);
	}
	
	public CorruptPointer(String message, long address) {
		this(address);
		this.message = message;
	}
	
	public static CorruptPointer cast(AbstractPointer pointer) {
		return cast(pointer.getAddress());
	}
	
	public static CorruptPointer cast(UDATA udata) {
		return cast(udata.longValue());
	}
	
	public static CorruptPointer cast(long address) {
		return new CorruptPointer(address);
	}

	public CorruptPointer at(long index) throws CorruptDataException {
		throw new NullPointerDereference("Can not dereference a CorruptPointer");
	}
	
	public CorruptPointer at(Scalar index) throws CorruptDataException {
		return at(index.longValue());
	}
	
	public AbstractPointer add(long count) {
		throw new UnsupportedOperationException();
	}

	public AbstractPointer add(Scalar count) {
		throw new UnsupportedOperationException();
	}
	
	public CorruptPointer addOffset(long offset) {
		return new CorruptPointer(message, address + offset);
	}

	public CorruptPointer addOffset(Scalar offset) {
		return addOffset(offset.longValue());
	}

	public CorruptPointer untag(long tagBits) {
		return new CorruptPointer(message, address & tagBits);
	}

	public AbstractPointer subOffset(long offset) {
		return new CorruptPointer(message, address - offset);
	}

	public AbstractPointer subOffset(Scalar offset) {
		return subOffset(offset.longValue());
	}

	public AbstractPointer sub(long count) {
		throw new UnsupportedOperationException();
	}

	public AbstractPointer sub(Scalar count) {
		throw new UnsupportedOperationException();
	}

	public AbstractPointer untag() {
		throw new UnsupportedOperationException();
	}
	
	public String getMessage() {
		return message;
	}
	
	@Override
	protected long sizeOfBaseType()
	{
		throw new UnsupportedOperationException();
	}
}
