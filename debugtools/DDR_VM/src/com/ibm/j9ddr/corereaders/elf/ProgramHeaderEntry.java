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
package com.ibm.j9ddr.corereaders.elf;

import java.util.Properties;

import com.ibm.j9ddr.corereaders.memory.IDetailedMemoryRange;
import com.ibm.j9ddr.corereaders.memory.IMemorySource;
import com.ibm.j9ddr.corereaders.memory.UnbackedMemorySource;

public class ProgramHeaderEntry
{
	// Program Headers defined in /usr/include/linux/elf.h
	// type constants
	// private static final int PT_NULL = 0;
	private static final int PT_LOAD = 1;
	private static final int PT_DYNAMIC = 2;
	// private static final int PT_INTERP = 3;
	private static final int PT_NOTE = 4;
	private static final int PT_GNU_EH_FRAME = 0x6474e550;
	// private static final int PT_SHLIB = 5;
	// private static final int PT_PHDR = 6;
	// private static final int PT_TLS = 7;
	private static final int PF_X = 1;
	private static final int PF_W = 2;
	private static final int PF_R = 4;

	// PHE structure definitions:
	private int _type;
	public final long fileOffset;
	public final long fileSize; // if fileSize == 0 then we have to map memory from
							// external lib file.
	public final long virtualAddress;
	public final long physicalAddress;
	public final long memorySize;
	private int _flags;
	private final ELFFileReader reader;

	// private long _alignment;

	ProgramHeaderEntry(int type, long fileOffset, long fileSize,
			long virtualAddress, long physicalAddress, long memorySize,
			int flags, long alignment, ELFFileReader reader)
	{
		_type = type;
		this.fileOffset = fileOffset;
		this.fileSize = fileSize;
		this.virtualAddress = virtualAddress;
		this.physicalAddress = physicalAddress;
		this.memorySize = memorySize;
		_flags = flags;
		this.reader = reader;
		// _alignment = alignment;
	}

	boolean isEmpty()
	{
		// if the core is not a complete dump of the address space, there will
		// be missing pieces. These are manifested as program header entries
		// with a real memory size but a zero file size
		// That is, even though they do occupy space in memory, they are
		// represented in none of the code
		return 0 == fileSize;
	}

	boolean isDynamic()
	{
		return PT_DYNAMIC == _type;
	}
	
	boolean isLoadable()
	{
		return PT_LOAD == _type;
	}

	boolean isNote()
	{
		return PT_NOTE == _type;
	}

	public boolean isEhFrame() {
		return PT_GNU_EH_FRAME == _type;
	}
	
	IMemorySource asMemorySource()
	{
		IMemorySource source = null;
		if (!isEmpty()) {
			boolean isExecutable = (_flags & PF_X) != 0;
			source = new ELFMemorySource(virtualAddress, memorySize, fileOffset, reader);
		} else {
			source = new UnbackedMemorySource(virtualAddress, memorySize,
					"ELF ProgramHeaderEntry storage declared but data not included");
		}
		Properties memoryProps = ((IDetailedMemoryRange)source).getProperties();
		memoryProps.setProperty("IN_CORE", ""+(!isEmpty()));
		if( (_flags & PF_W) != 0 ) {
			memoryProps.setProperty(IDetailedMemoryRange.WRITABLE, Boolean.TRUE.toString());
		}
		if( (_flags & PF_X) != 0 ) {
			memoryProps.setProperty(IDetailedMemoryRange.EXECUTABLE, Boolean.TRUE.toString());
		}
		if( (_flags & PF_R) != 0 ) {
			memoryProps.setProperty(IDetailedMemoryRange.READABLE, Boolean.TRUE.toString());
		}
		return source;
	}

	boolean validInProcess(long address)
	{
		return virtualAddress <= address
				&& address < virtualAddress + memorySize;
	}

	boolean contains(long address)
	{
		return false == isEmpty() && validInProcess(address);
	}
}
