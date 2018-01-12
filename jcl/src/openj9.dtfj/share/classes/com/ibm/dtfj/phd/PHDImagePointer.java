/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2008, 2017 IBM Corp. and others
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
package com.ibm.dtfj.phd;

import java.util.Properties;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.MemoryAccessException;

/** 
 * @author ajohnson
 */
class PHDImagePointer implements ImagePointer {

	private final ImageAddressSpace space;
	private final long addr;
	
	PHDImagePointer(ImageAddressSpace imageAddress, long arg0) {
		space = imageAddress;
		addr = arg0;
	}

	public ImagePointer add(long arg0) {
		return new PHDImagePointer(space,addr+arg0);
	}

	public long getAddress() {
		return addr;
	}

	public ImageAddressSpace getAddressSpace() {
		return space;
	}

	public byte getByteAt(long arg0) throws MemoryAccessException,
			CorruptDataException {
		throw new MemoryAccessException(add(arg0));
	}

	public double getDoubleAt(long arg0) throws MemoryAccessException,
			CorruptDataException {
		throw new MemoryAccessException(add(arg0));
	}

	public float getFloatAt(long arg0) throws MemoryAccessException,
			CorruptDataException {
		throw new MemoryAccessException(add(arg0));
	}

	public int getIntAt(long arg0) throws MemoryAccessException,
			CorruptDataException {
		throw new MemoryAccessException(add(arg0));
	}

	public long getLongAt(long arg0) throws MemoryAccessException,
			CorruptDataException {
		throw new MemoryAccessException(add(arg0));
	}

	public ImagePointer getPointerAt(long arg0) throws MemoryAccessException,
			CorruptDataException {
		throw new MemoryAccessException(add(arg0));
	}

	public short getShortAt(long arg0) throws MemoryAccessException,
			CorruptDataException {
		throw new MemoryAccessException(add(arg0));
	}

	public boolean isExecutable() throws DataUnavailable {
		throw new DataUnavailable();
	}

	public boolean isReadOnly() throws DataUnavailable {
		throw new DataUnavailable();
	}

	public boolean isShared() throws DataUnavailable {
		throw new DataUnavailable();
	}
	
	public Properties getProperties() {
		return new Properties();
	}

	public boolean equals(Object o) {
		if (o instanceof PHDImagePointer) {
			PHDImagePointer to = (PHDImagePointer)o;
			if (space.equals(to.space) && addr == to.addr) return true;
		}
		return false;
	}
	
	public int hashCode() {
		return space.hashCode() ^ (int)addr ^ (int)(addr >>> 32);
	}
}
