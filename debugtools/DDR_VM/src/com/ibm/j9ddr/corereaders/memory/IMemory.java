/*******************************************************************************
 * Copyright (c) 2009, 2014 IBM Corp. and others
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

import java.util.Collection;
import java.util.Properties;

import com.ibm.j9ddr.corereaders.Platform;

/**
 * Common interface representing an area of addressable memory.
 * 
 * @author andhall
 * 
 */
public interface IMemory
{
	public Collection<? extends IMemoryRange> getMemoryRanges();

	public byte getByteAt(long address) throws MemoryFault;

	public short getShortAt(long address) throws MemoryFault;

	public int getIntAt(long address) throws MemoryFault;

	public long getLongAt(long address) throws MemoryFault;

	public int getBytesAt(long address, byte[] buffer) throws MemoryFault;
	
	public int getBytesAt(long address, byte[] buffer, int offset, int length) throws MemoryFault;

	public long findPattern(byte[] whatBytes, int alignment, long startFrom);

	public java.nio.ByteOrder getByteOrder();
	
	public boolean isExecutable(long address);

	public boolean isReadOnly(long address);

	public boolean isShared(long address);
	
	public Properties getProperties(long address);
	
	public Platform getPlatform();
}
