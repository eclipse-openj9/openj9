/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
package com.ibm.dtfj.image.j9.corrupt;

import java.util.Properties;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.image.j9.CorruptData;

/**
 * Class to represent a corrupt pointer and can be used to populate a corrupt data exception.
 * It will throw exceptions if an attempt is made to read data using it.
 *  
 * @author Adam Pilkington
 *
 */
public class CorruptImagePointer implements ImagePointer {
	private long address = 0;
	private ImageAddressSpace memory = null;
	
	public CorruptImagePointer(long address, ImageAddressSpace memory) {
		this.address = address;
		this.memory = memory;
	}
	
	public ImagePointer add(long offset) {
		return memory.getPointer(address + offset);		
	}

	public long getAddress() {
		return address;
	}

	public ImageAddressSpace getAddressSpace() {
		return memory;
	}
	
	public String toString() {
		return "0x" + Long.toHexString(address);
	}

	public byte getByteAt(long index) throws MemoryAccessException, CorruptDataException {
		throw new CorruptDataException(new CorruptData("Data cannot be read from this corrupt pointer", null));
	}

	public double getDoubleAt(long index) throws MemoryAccessException,	CorruptDataException {
		throw new CorruptDataException(new CorruptData("Data cannot be read from this corrupt pointer", null));
	}

	public float getFloatAt(long index) throws MemoryAccessException, CorruptDataException {
		throw new CorruptDataException(new CorruptData("Data cannot be read from this corrupt pointer", null));
	}

	public int getIntAt(long index) throws MemoryAccessException, CorruptDataException {
		throw new CorruptDataException(new CorruptData("Data cannot be read from this corrupt pointer", null));
	}

	public long getLongAt(long index) throws MemoryAccessException,	CorruptDataException {
		throw new CorruptDataException(new CorruptData("Data cannot be read from this corrupt pointer", null));
	}

	public ImagePointer getPointerAt(long index) throws MemoryAccessException, CorruptDataException {
		throw new CorruptDataException(new CorruptData("Data cannot be read from this corrupt pointer", null));
	}

	public short getShortAt(long index) throws MemoryAccessException, CorruptDataException {
		throw new CorruptDataException(new CorruptData("Data cannot be read from this corrupt pointer", null));
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

}
