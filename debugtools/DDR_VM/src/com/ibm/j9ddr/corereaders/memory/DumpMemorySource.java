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

import java.io.IOException;

import com.ibm.j9ddr.corereaders.AbstractCoreReader;

/**
 * Memory range backed by an AbstractCoreReader
 * 
 * @author andhall
 * 
 */
public class DumpMemorySource extends ProtectedMemoryRange implements IMemorySource
{
	private final long fileOffset;

	private final AbstractCoreReader coreReader;

	private final int addressSpaceId;
	
	private final String name;
	
	/* Available for subclasses */
	protected DumpMemorySource(DumpMemorySource d) {
		this(d.baseAddress, d.size, d.fileOffset,
				d.addressSpaceId, d.coreReader, d.name, d.shared, d.readOnly, d.executable);
	}
	
	public DumpMemorySource(long baseAddress, long size, long fileOffset,
			AbstractCoreReader reader)
	{
		this(baseAddress, size, fileOffset, 0, reader, null, false, false, true);
	}
	
	public DumpMemorySource(long baseAddress, long size, long fileOffset,
			AbstractCoreReader reader, boolean shared, boolean readOnly, boolean executable)
	{
		this(baseAddress, size, fileOffset, 0, reader, null, shared, readOnly, executable);
	}

	public DumpMemorySource(long baseAddress, long size, long fileOffset,
			int addressSpaceId, AbstractCoreReader reader, String name)
	{
		this(baseAddress, size, fileOffset, addressSpaceId, reader, name, false, false, true);
	}
	
	public DumpMemorySource(long baseAddress, long size, long fileOffset,
			int addressSpaceId, AbstractCoreReader reader, String name, boolean shared, boolean readOnly, boolean executable)
	{
		super(baseAddress,size);
		this.fileOffset = fileOffset;
		this.addressSpaceId = addressSpaceId;
		this.coreReader = reader;
		this.shared = shared;
		this.readOnly = readOnly;
		this.executable = executable;
		this.name = name;
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see
	 * com.ibm.dtfj.j9ddr.corereaders.memory.IMemoryRange#getAddressSpaceId()
	 */
	public int getAddressSpaceId()
	{
		return addressSpaceId;
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see com.ibm.dtfj.j9ddr.corereaders.memory.IMemoryRange#getBytes(long,
	 * byte[], int, int)
	 */
	public int getBytes(long address, byte[] buffer, int offset, int length)
			throws MemoryFault
	{

		long rangeOffset = address - baseAddress;
		
		if (rangeOffset < 0 || rangeOffset > size) {
			throw new IllegalArgumentException("Address "
					+ Long.toHexString(address) + " is not in this range");
		}

		try {
			coreReader.seek(fileOffset + rangeOffset);
			coreReader.readFully(buffer, offset, length);
		} catch (IOException ex) {
			throw new MemoryFault(address,
					"Memory fault caused by IOException reading dump.", ex);
		}
		return length;
	}

	public long getFileOffset()
	{
		return fileOffset;
	}

	public String getName()
	{
		return name;
	}

}
