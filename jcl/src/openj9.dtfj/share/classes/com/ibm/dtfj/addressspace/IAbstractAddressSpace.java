/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2020 IBM Corp. and others
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
package com.ibm.dtfj.addressspace;

import java.nio.ByteOrder;
import java.util.Iterator;

import com.ibm.dtfj.corereaders.MemoryAccessException;
import com.ibm.dtfj.corereaders.MemoryRange;

/**
 * @author jmdisher
 * The interface for the abstract interface.  Note that there will probably only ever be one implementer of 
 * this interface so it will probably be flattened once it is frozen
 */
public interface IAbstractAddressSpace 
{
	/**
	 * @return An iterator of the MemoryRange objects making up the address space
	 * @see MemoryRange
	 */
	public Iterator getMemoryRanges();

	/**
	 * @param asid an address space ID
	 * @param address a byte-offset into the asid
	 * @return true if this memory address is within an executable page
	 * @throws MemoryAccessException if the memory cannot be read
	 */
	public boolean isExecutable(int asid, long address) throws MemoryAccessException;

	/**
	 * @param asid an address space ID
	 * @param address a byte-offset into the asid
	 * @return true if write access to this memory address was disabled in the image
	 * @throws MemoryAccessException if the memory cannot be read
	 */
	public boolean isReadOnly(int asid, long address) throws MemoryAccessException;

	/**
	 * @param asid an address space ID
	 * @param address a byte-offset into the asid
	 * @return true if this memory address is shared between processes
	 * @throws MemoryAccessException if the memory cannot be read
	 */
	public boolean isShared(int asid, long address) throws MemoryAccessException;

	/**
	 * @param asid an address space ID
	 * @param address a byte-offset into the asid
	 * @return the 64-bit long stored at address in asid
	 * @throws MemoryAccessException if the memory cannot be read
	 */
	public long getLongAt(int asid, long address) throws MemoryAccessException;

	/**
	 * @param asid an address space ID
	 * @param address a byte-offset into the asid
	 * @return the 32-bit int stored at address in asid
	 * @throws MemoryAccessException if the memory cannot be read
	 */
	public int getIntAt(int asid, long address) throws MemoryAccessException;

	/**
	 * @param asid an address space ID
	 * @param address a byte-offset into the asid
	 * @return the 16-bit short stored at address in asid
	 * @throws MemoryAccessException if the memory cannot be read
	 */
	public short getShortAt(int asid, long address) throws MemoryAccessException;

	/**
	 * Return the byte order of this address space.
	 *
	 * @return the byte order of this address space
	 */
	public ByteOrder getByteOrder();

	/**
	 * @param asid an address space ID
	 * @param address a byte-offset into the asid
	 * @return the 8-bit byte stored at address in asid
	 * @throws MemoryAccessException if the memory cannot be read
	 */
	public byte getByteAt(int asid, long address) throws MemoryAccessException;
	
	/**
	 * @param asid an address space ID
	 * @param address a byte-offset into the asid
	 * @return the pointer stored at address in asid
	 * @throws MemoryAccessException if the memory cannot be read
	 */
	public long getPointerAt(int asid, long address) throws MemoryAccessException;
	
	/**
	 * @param asid an address space ID
	 * @param address a byte-offset into the asid
	 * @param buffer a byte array to receive the bytes
	 * @return the number of bytes read into buffer
	 * @throws MemoryAccessException if the memory cannot be read
	 */
	public int getBytesAt(int asid, long address, byte[] buffer) throws MemoryAccessException;
	
	/**
	 * Provided so that callers can determine more complicated memory geometry than what can be expressed
	 * with offsets and the above scalar data readers.
	 * @param asid The address space id.
	 * @return The number of bytes which are required to express a native pointer in the underlying address
	 * space. 
	 */
	public int bytesPerPointer(int asid);

	/**
	 * This method is provided to appease JExtract by emulating part of the old API which is used
	 * by the JExtract natives. Also used to search for the J9RAS structure in J9RASReader.java
	 * 
	 * @param whatBytes The pattern to search for
	 * @param alignment The alignment boundary where the pattern can be expected to start
	 * @param startFrom The first memory address to start searching in
	 * @return
	 */
	public long findPattern(byte[] whatBytes, int alignment, long startFrom);

	/**
	 * This method is provided to appease JExtract by emulating part of the old API which is used
	 * by the JExtract natives.
	 * 
	 * @param vaddr
	 * @param size
	 * @return
	 */
	public byte[] getMemoryBytes(long vaddr, int size);
}
