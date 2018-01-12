/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.dtfj.corereaders;

import java.util.Iterator;

public interface Dump extends ICoreFileReader
{
	/**
	 * @return the precise model of the CPU (note that this can be an empty string but not null).
	 * <br>
	 * e.g. "Pentium IV step 4"
	 */
	String getProcessorSubtype();
	
	/**
	 * Determines when the image was created
	 * @return the time in milliseconds since 1970
	 */
	long getCreationTime();

	/**
	 * @param asid an address space ID
	 * @param address a byte-offset into the asid
	 * @return true if this memory address is within an executable page
	 * @throws MemoryAccessException if the memory cannot be read
	 */
	boolean isExecutable(int asid, long address) throws MemoryAccessException;

	/**
	 * @param asid an address space ID
	 * @param address a byte-offset into the asid
	 * @return true if write access to this memory address was disabled in the image
	 * @throws MemoryAccessException if the memory cannot be read
	 */
	boolean isReadOnly(int asid, long address) throws MemoryAccessException;

	/**
	 * @param asid an address space ID
	 * @param address a byte-offset into the asid
	 * @return true if this memory address is shared between processes
	 * @throws MemoryAccessException if the memory cannot be read
	 */
	boolean isShared(int asid, long address) throws MemoryAccessException;

	/**
	 * @param asid an address space ID
	 * @param address a byte-offset into the asid
	 * @return the 64-bit long stored at address in asid
	 * @throws MemoryAccessException if the memory cannot be read
	 */
	long getLongAt(int asid, long address) throws MemoryAccessException;

	/**
	 * @param asid an address space ID
	 * @param address a byte-offset into the asid
	 * @return the 32-bit int stored at address in asid
	 * @throws MemoryAccessException if the memory cannot be read
	 */
	int getIntAt(int asid, long address) throws MemoryAccessException;

	/**
	 * @param asid an address space ID
	 * @param address a byte-offset into the asid
	 * @return the 16-bit short stored at address in asid
	 * @throws MemoryAccessException if the memory cannot be read
	 */
	short getShortAt(int asid, long address) throws MemoryAccessException;

	/**
	 * @param asid an address space ID
	 * @param address a byte-offset into the asid
	 * @return the 8-bit byte stored at address in asid
	 * @throws MemoryAccessException if the memory cannot be read
	 */
	byte getByteAt(int asid, long address) throws MemoryAccessException;
	
	/**
	 * @return An iterator of the MemoryRange objects making up the address space
	 * @see MemoryRange
	 */
	public Iterator getMemoryRanges();

	/**
	 * @return An iterator of String object specifying names of additional files needed by the Dump
	 */
	Iterator getAdditionalFileNames();
}
