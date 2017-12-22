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
 * A memory range that holds its data.
 * 
 * @author andhall
 *
 */
public interface IMemorySource extends IMemoryRange
{
	/**
	 * Reads data from the memory range
	 * 
	 * @param address
	 *            Starting address
	 * @param buffer
	 *            Buffer to read into
	 * @param offset
	 *            Offset in buffer to write to
	 * @param length
	 *            Number of bytes to read
	 * @return Bytes read
	 */
	public int getBytes(long address, byte[] buffer, int offset, int length)
			throws MemoryFault;
	
	/**
	 * 
	 * @return True if this memory range is backed with data (such that getBytes() won't always throw MemoryFault).
	 */
	public boolean isBacked();
}
