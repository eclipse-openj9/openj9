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
package com.ibm.j9ddr.corereaders.debugger;

import java.nio.ByteOrder;
import java.util.Collection;
import java.util.LinkedList;
import java.util.List;
import java.util.Properties;

import com.ibm.j9ddr.corereaders.Platform;
import com.ibm.j9ddr.corereaders.memory.IMemory;
import com.ibm.j9ddr.corereaders.memory.IMemoryRange;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;
import com.ibm.j9ddr.corereaders.memory.MemoryRange;


/**
 * Access memory of application or memory dump using attached debugger. 
 */
public abstract class JniMemory implements IMemory {
	private long searchSize;

	public native byte getByteAt(long address) throws MemoryFault;

	public native ByteOrder getByteOrder();

	public native int getBytesAt(long address, byte[] buffer) throws MemoryFault;

	public native int getBytesAt(long address, byte[] buffer, int offset, int length) throws MemoryFault;

	public native int getIntAt(long address) throws MemoryFault;

	public native long getLongAt(long address) throws MemoryFault;

	public native short getShortAt(long address) throws MemoryFault;

	public Properties getProperties(long address) {
		return new Properties();
	}

	public Platform getPlatform() {
		return JniNatives.getPlatform();
	}

	public native boolean isExecutable(long address);

	public native boolean isReadOnly(long address);

	public native boolean isShared(long address);

	public long getPointerAt(long address, int bytesPerPointer) throws MemoryFault
	{
		if (bytesPerPointer == 8) {
			return getLongAt(address);
		} else {
			return getIntAt(address);
		}
	}

	public native long readAddress(long address);

	public JniMemory() {
		searchSize = Integer.MAX_VALUE;
	}

	public Collection<? extends IMemoryRange> getMemoryRanges() {
		List<MemoryRange> ranges = new LinkedList<MemoryRange>();

		MemoryRange region = getValidRegionVirtual(0, searchSize);

		while (region != null) {
			ranges.add(region);
			region = getValidRegionVirtual(region.getBaseAddress() + region.getSize(), searchSize);
		}

		return ranges;
	}

	/**
	 * method locates the first valid region of memory in a specified memory
	 * range.
	 * 
	 * @param base
	 *            address of the beginning of the memory range to search for
	 *            valid memory.
	 * @param searchSize
	 *            size, in bytes, of the memory range to search.
	 * @return MemoryRegion or null if no valid memory found in range.
	 */
	private static native MemoryRange getValidRegionVirtual(long base, long searchSize);
}
