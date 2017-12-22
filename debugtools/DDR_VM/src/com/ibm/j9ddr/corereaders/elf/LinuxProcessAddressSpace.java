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

import java.io.UnsupportedEncodingException;
import java.nio.ByteOrder;
import java.util.List;
import java.util.Properties;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.DataUnavailableException;
import com.ibm.j9ddr.corereaders.ICore;
import com.ibm.j9ddr.corereaders.Platform;
import com.ibm.j9ddr.corereaders.memory.IMemorySource;
import com.ibm.j9ddr.corereaders.memory.IModule;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;
import com.ibm.j9ddr.corereaders.memory.ProcessAddressSpace;
import com.ibm.j9ddr.corereaders.osthread.IOSThread;

/**
 * @author andhall
 *
 */
public class LinuxProcessAddressSpace extends ProcessAddressSpace
{

	private final ELFDumpReader reader;
	
	public LinuxProcessAddressSpace(int pointerSizeBytes, ByteOrder byteOrder,
			ELFDumpReader reader)
	{
		super(pointerSizeBytes, byteOrder, reader);
		this.reader = reader;
	}

	public ICore getCore() {
		return reader;
	}

	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.corereaders.memory.IProcess#getCommandLine()
	 */
	public String getCommandLine() throws CorruptDataException
	{
		return reader.getCommandLine();
	}
	
	@Override
	public boolean equals(Object o) {
		if((o == null) || !(o instanceof LinuxProcessAddressSpace)) {
			return false;
		}
		LinuxProcessAddressSpace space = (LinuxProcessAddressSpace) o;
		return reader.equals(space.reader);
	}

	@Override
	public int hashCode() {
		return reader.hashCode();
	}

	public Properties getEnvironmentVariables() throws CorruptDataException, DataUnavailableException
	{
		throw new DataUnavailableException("Can't get environment from an ELF core dump");
	}
	
	public IModule getExecutable() throws CorruptDataException
	{
		return reader.getExecutable();
	}

	public List<? extends IModule> getModules() throws CorruptDataException
	{
		return reader.getModules();
	}

	public long getProcessId() throws CorruptDataException
	{
		return reader.getProcessId();
	}

	public List<? extends IOSThread> getThreads() throws CorruptDataException
	{
		return reader.getThreads();
	}

	public Platform getPlatform()
	{
		return Platform.LINUX;
	}
	
	public int getSignalNumber() throws DataUnavailableException
	{
		return reader.getSignalNumber();
	}

	public String readStringAt(long nameAddress) throws MemoryFault
	{
		long ptr = nameAddress;
		
		while (getByteAt(ptr) != 0) {
			ptr++;
		}
		
		int stringLength = (int)(ptr - nameAddress);
		
		byte[] stringBuffer = new byte[stringLength];
		
		getBytesAt(nameAddress, stringBuffer);
		
		try {
			return new String(stringBuffer, "ASCII");
		} catch (UnsupportedEncodingException e) {
			throw new RuntimeException(e);
		}
	}

	IMemorySource getRangeForAddress(long address)
	{
		return memorySources.getRangeForAddress(address);
	}

	public boolean isFailingProcess() throws DataUnavailableException {
		throw new DataUnavailableException("Not available on this platform");
	}
	
}
