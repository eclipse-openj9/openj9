/*******************************************************************************
 * Copyright (c) 2010, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders.memory;

/**
 * An optionally named range of memory with permissions.
 * 
 * @author andhall
 *
 */
public interface IMemoryRange extends Comparable<IMemoryRange>
{
	/**
	 * 
	 * @return Address space ID that this range belongs to.
	 */
	public int getAddressSpaceId();

	/**
	 * 
	 * @return Base address of this memory range
	 */
	public long getBaseAddress();

	/**
	 * @return Largest address in this memory range
	 */
	public long getTopAddress();

	/**
	 * 
	 * @return Size of this memory range, bytes
	 */
	public long getSize();
	
	public boolean isShared();

	public boolean isExecutable();

	public boolean isReadOnly();
	
	public boolean isBacked();
	
	/**
	 * Checks whether an address is present in this memory range
	 * 
	 * @param address
	 *            Address to test
	 * @return True if the memory range contains address, false otherwise.
	 */
	public boolean contains(long address);
	
	/**
	 * 
	 * @param other
	 * @return True if other shares any addresses with this range.
	 */
	public boolean overlaps(IMemoryRange other);
	
	/**
	 * 
	 * @param other
	 * @return True if other models a memory range that sits entirely within this range
	 */
	public boolean isSubRange(IMemoryRange other);

	/**
	 * 
	 * @return Name of this range (e.g. .text or stack), or null if range is unnamed.
	 */
	public String getName();
	
}
