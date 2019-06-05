/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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
/*******************************************************************************
 * Portions Copyright (c) 1999-2003 Apple Computer, Inc. All Rights
 * Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *******************************************************************************/
package com.ibm.j9ddr.corereaders.macho;

import java.io.UnsupportedEncodingException;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
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

public class OSXProcessAddressSpace extends ProcessAddressSpace
{

	private final MachoDumpReader reader;
	
	public OSXProcessAddressSpace(int pointerSizeBytes, ByteOrder byteOrder,
			MachoDumpReader reader)
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
	public String getCommandLine() throws DataUnavailableException
	{
		return reader.getCommandLine();
	}
	
	@Override
	public boolean equals(Object o)
	{
		if((o == null) || !(o instanceof OSXProcessAddressSpace)) {
			return false;
		}
		OSXProcessAddressSpace space = (OSXProcessAddressSpace) o;
		return reader.equals(space.reader);
	}

	@Override
	public int hashCode()
	{
		return reader.hashCode();
	}

	public Properties getEnvironmentVariables() throws CorruptDataException, DataUnavailableException
	{
		throw new DataUnavailableException("Can't get environment from core dump");
	}
	
	public IModule getExecutable() throws CorruptDataException
	{
		return reader.getExecutable();
	}

	public List<? extends IModule> getModules() throws CorruptDataException
	{
		return reader.getModules();
	}

	public Platform getPlatform()
	{
		return Platform.OSX;
	}

	public long getProcessId() throws CorruptDataException
	{
		return reader.getProcessId();
	}

	public List<? extends IOSThread> getThreads() throws CorruptDataException
	{
		return reader.getThreads();
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
		
		return new String(stringBuffer, StandardCharsets.US_ASCII);
	}

	IMemorySource getRangeForAddress(long address)
	{
		return memorySources.getRangeForAddress(address);
	}

	public boolean isFailingProcess() throws DataUnavailableException
	{
		throw new DataUnavailableException("Not available on this platform");
	}
	
}
