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
package com.ibm.j9ddr.corereaders.elf;

import java.io.IOException;
import java.util.Properties;

import com.ibm.j9ddr.corereaders.memory.Addresses;
import com.ibm.j9ddr.corereaders.memory.IDetailedMemoryRange;
import com.ibm.j9ddr.corereaders.memory.IMemorySource;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;
import com.ibm.j9ddr.corereaders.memory.ProtectedMemoryRange;

/**
 * Memory source that gets its data from an ELFFile.
 * @author andhall
 *
 */
public class ELFMemorySource extends ProtectedMemoryRange implements IMemorySource, IDetailedMemoryRange
{
	private final long fileOffset;
	private final ELFFileReader reader;
	private final String name;
	private Properties props;
	
	ELFMemorySource(long baseAddress, long size,
			long fileOffset, ELFFileReader reader, String name, boolean executable)
	{
		super(baseAddress, size);
		this.fileOffset = fileOffset;
		this.reader = reader;
		this.name = name;
		this.executable = executable;
	}

	ELFMemorySource(long baseAddress, long size,
			long fileOffset, ELFFileReader reader, String name)
	{
		this(baseAddress, size, fileOffset, reader, name, true);
	}
	
	ELFMemorySource(long baseAddress, long size, long fileOffset, ELFFileReader reader)
	{
		this(baseAddress,size,fileOffset,reader,"");
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.corereaders.memory.IMemoryRange#getAddressSpaceId()
	 */
	public int getAddressSpaceId()
	{
		return 0;
	}

	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.corereaders.memory.IMemoryRange#getBytes(long, byte[], int, int)
	 */
	public int getBytes(long address, byte[] buffer, int offset, int length)
			throws MemoryFault
	{
		if (Addresses.greaterThan(address + length - 1, getTopAddress())) {
			throw new MemoryFault(address + length, "Address out of range of memory range (overflow): " + this.toString());
		}
		
		if (Addresses.lessThan(address, baseAddress)) {
			throw new MemoryFault(address, "Address out of range of memory range (underflow): " + this.toString());
		}
		
		long rangeOffset = address - baseAddress;
		long seekAddress = fileOffset + rangeOffset;
		
		try {
			reader.seek(seekAddress);
			reader.readFully(buffer,offset,length);
		} catch (IOException e) {
			throw new MemoryFault(address, "IOException accessing ELF storage in " + reader,e);
		}
		
		return length;
	}

	public String getName()
	{
		return name;
	}

	public Properties getProperties() {
		if( props == null ) {
			props = new Properties();
		}
		return props;
	}
	
	public boolean isExecutable() {
		if( props == null ) {
			return false;
		}
		return Boolean.TRUE.toString().equals(props.get(IDetailedMemoryRange.EXECUTABLE));
	}

	/** Assuming that just because something is executable doesn't stop it being read only.
	 * (ReadOnly is not actually a very good property to expose since it relies on the lack
	 * of other properties.)
	 */
	public boolean isReadOnly() {
		if( props == null ) {
			return false;
		}
		return Boolean.TRUE.toString().equals(props.get(IDetailedMemoryRange.READABLE)) &&
				!(Boolean.TRUE.toString().equals(props.get(IDetailedMemoryRange.WRITABLE)) );
	}
}
