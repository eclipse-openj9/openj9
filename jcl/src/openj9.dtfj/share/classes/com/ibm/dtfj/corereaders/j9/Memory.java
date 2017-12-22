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
package com.ibm.dtfj.corereaders.j9;

import java.util.Iterator;

import com.ibm.dtfj.addressspace.IAbstractAddressSpace;
import com.ibm.dtfj.corereaders.MemoryAccessException;
import com.ibm.dtfj.corereaders.MemoryRange;

/**
 * Memory adapter which removes the need for a component to know the address space ID that it is working with.
 * This is set when the adapter is created.
 * 
 * @author Adam Pilkington
 *
 */
public class Memory {
	private IAbstractAddressSpace space = null;
	private int asid = 0;			//address space ID

	/**
	 * Create a memory representation
	 * @param space the underlying address space 
	 */
	public Memory(IAbstractAddressSpace space) {
		this.space = space;
		if (null != space) {				//determine the ASID from the first memory range
			Iterator ranges = space.getMemoryRanges(); 
			if (ranges.hasNext()) {
				MemoryRange firstRange = (MemoryRange)ranges.next();
				asid = firstRange.getAsid();
			}
		}
	}
	
	/**
	 * Identify the memory as 32 or 64 bit
	 * @return 4 for 31/32 bit, 8 for 64 bit
	 */
	public int bytesPerPointer() {
		return space.bytesPerPointer(asid);
	}

	/**
	 * Search the memory for a specific byte pattern
	 * @param whatBytes what to search for
	 * @param alignment byte alignment
	 * @param startFrom position to start the search from
	 * @return the address of the bytes if found or -1
	 */
	public long findPattern(byte[] whatBytes, int alignment, long startFrom) {
		return space.findPattern(whatBytes, alignment, startFrom);
	}

	/**
	 * Read a byte from the specified address
	 * @param address address to read from
	 * @return the data
	 * @throws MemoryAccessException
	 */
	public byte getByteAt(long address) throws MemoryAccessException {
		return space.getByteAt(asid, address);
	}

	/**
	 * Read a byte from the specified address
	 * @param address address to read from
	 * @return the data
	 * @throws MemoryAccessException
	 */
	public int getBytesAt(long address, byte[] buffer) throws MemoryAccessException {
		return space.getBytesAt(asid, address, buffer);
	}

	/**
	 * Read an int from the specified address
	 * @param address address to read from
	 * @return the data
	 * @throws MemoryAccessException
	 */
	public int getIntAt(long address) throws MemoryAccessException {
		return space.getIntAt(asid, address);
	}

	/**
	 * Read a long from the specified address
	 * @param address address to read from
	 * @return the data
	 * @throws MemoryAccessException
	 */
	public long getLongAt(long address) throws MemoryAccessException {
		return space.getLongAt(asid, address);
	}

	/**
	 * Read bytes from the specified address
	 * @param address address to read from
	 * @param size number of bytes to read
	 * @return the data
	 */
	public byte[] getMemoryBytes(long address, int size) {
		return space.getMemoryBytes(address, size);
	}

	/**
	 * Get the memory ranges for this address space
	 */
	public Iterator getMemoryRanges() {
		return space.getMemoryRanges();
	}

	/**
	 * Read a pointer at the specified address
	 * @param address address to read from
	 * @return the data
	 * @throws MemoryAccessException
	 */
	public long getPointerAt(long address) throws MemoryAccessException {
		return space.getPointerAt(asid, address);
	}

	/**
	 * Read a short from the specified address
	 * @param address address to read from
	 * @return the data
	 * @throws MemoryAccessException
	 */
	public short getShortAt(long address) throws MemoryAccessException {
		return space.getShortAt(asid, address);
	}

	/**
	 * Flag for executable memory
	 * @return true if it is executable
	 * @throws MemoryAccessException
	 */
	public boolean isExecutable(long address)	throws MemoryAccessException {
		return space.isExecutable(asid, address);
	}

	/**
	 * Flag to indicate if the memory is read only
	 * @return true if it is read only
	 * @throws MemoryAccessException
	 */
	public boolean isReadOnly(long address) throws MemoryAccessException {
		return space.isReadOnly(asid, address);
	}

	/**
	 * Flag to indicate if the memory is shared
	 * @return true if it is shared
	 * @throws MemoryAccessException
	 */
	public boolean isShared(long address) throws MemoryAccessException {
		return space.isShared(asid, address);
	}

}
