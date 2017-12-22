/*******************************************************************************
 * Copyright (c) 2004, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders.minidump;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import com.ibm.j9ddr.corereaders.memory.DumpMemorySource;
import com.ibm.j9ddr.corereaders.memory.IMemorySource;

class Memory64Stream extends EarlyInitializedStream
{
	public Memory64Stream(int dataSize, int location)
	{
		super(dataSize, location);
	}

	public void readFrom(MiniDumpReader dump) throws IOException
	{
		long location = getLocation();
		dump.seek(location);
		long numberOfMemoryRanges = dump.readLong();
		long baseAddress = dump.readLong();
		List<IMemorySource> memorySources = new ArrayList<IMemorySource>();
		DumpMemorySource memoryRange = null;
		for (int i = 0; i < numberOfMemoryRanges; i++) {
			long start = dump.is64Bit() ? dump.readLong()
					: dump.readLong() & 0x00000000FFFFFFFFL;
			long size = dump.is64Bit() ? dump.readLong()
					: dump.readLong() & 0x00000000FFFFFFFFL;
			if (null == memoryRange) {
				// Allocate the first memory range starting at baseAddress in
				// the dump file
				memoryRange = new DumpMemorySource(start, size, baseAddress,
						dump, false, false, true);
			} else if (memoryRange.getBaseAddress() + memoryRange.getSize() == start) {
				// Combine contiguous regions
				memoryRange = new DumpMemorySource(memoryRange.getBaseAddress(),
						memoryRange.getSize() + size, memoryRange
								.getFileOffset(), dump, false, false, true);
			} else {
				// Add the previous MemoryRange and start the next one
				memorySources.add(memoryRange);
				memoryRange = new DumpMemorySource(start, size, memoryRange
						.getFileOffset()
						+ memoryRange.getSize(), dump, false, false, true);
			}

			// public DumpMemoryRange(long baseAddress, long size, long
			// fileOffset, AbstractCoreReader reader)
		}

		if (null != memoryRange) {
			memorySources.add(memoryRange);
		}

		dump.setMemorySources(memorySources);
	}
}
