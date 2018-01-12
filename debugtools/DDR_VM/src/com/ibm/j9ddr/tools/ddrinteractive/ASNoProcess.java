/*******************************************************************************
 * Copyright (c) 2011, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.tools.ddrinteractive;

import java.nio.ByteOrder;
import java.util.Collection;
import java.util.Collections;
import java.util.Properties;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.DataUnavailableException;
import com.ibm.j9ddr.corereaders.ICore;
import com.ibm.j9ddr.corereaders.Platform;
import com.ibm.j9ddr.corereaders.memory.IAddressSpace;
import com.ibm.j9ddr.corereaders.memory.IMemoryRange;
import com.ibm.j9ddr.corereaders.memory.IModule;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;
import com.ibm.j9ddr.corereaders.osthread.IOSThread;

public class ASNoProcess implements IAddressSpace, IProcess {
	private final IAddressSpace as;
	
	public ASNoProcess(IAddressSpace space) {
		as = space;
	}

	public Collection<? extends IMemoryRange> getMemoryRanges() {
		return as.getMemoryRanges();
	}

	public byte getByteAt(long address) throws MemoryFault {
		return as.getByteAt(address);
	}

	public short getShortAt(long address) throws MemoryFault {
		return as.getShortAt(address);
	}

	public int getAddressSpaceId() {
		return as.getAddressSpaceId();
	}

	public int getIntAt(long address) throws MemoryFault {
		return as.getIntAt(address);
	}

	public long getLongAt(long address) throws MemoryFault {
		return as.getLongAt(address);
	}

	public Collection<? extends IProcess> getProcesses() {
		return as.getProcesses();
	}

	public int getBytesAt(long address, byte[] buffer) throws MemoryFault {
		return as.getBytesAt(address, buffer);
	}

	public int getBytesAt(long address, byte[] buffer, int offset, int length)
			throws MemoryFault {
		return as.getBytesAt(address, buffer, offset, length);
	}

	public ICore getCore() {
		return as.getCore();
	}

	public long findPattern(byte[] whatBytes, int alignment, long startFrom) {
		return as.findPattern(whatBytes, alignment, startFrom);
	}

	public ByteOrder getByteOrder() {
		return as.getByteOrder();
	}

	public boolean isExecutable(long address) {
		return as.isExecutable(address);
	}

	public boolean isReadOnly(long address) {
		return as.isReadOnly(address);
	}

	public boolean isShared(long address) {
		return as.isShared(address);
	}
	
	public Properties getProperties(long address) {
		return as.getProperties(address);
	}

	public Platform getPlatform() {
		return as.getPlatform();
	}

	public IAddressSpace getAddressSpace() {
		return as;
	}

	public long getPointerAt(long address) throws MemoryFault {
		return 0;
	}

	public int bytesPerPointer() {
		return 0;
	}

	public String getCommandLine() throws DataUnavailableException,	CorruptDataException {
		return "Error no process in this address space";
	}

	public Properties getEnvironmentVariables()	throws DataUnavailableException, CorruptDataException {
		return new Properties();
	}

	public Collection<? extends IModule> getModules() throws CorruptDataException {
		return Collections.emptySet();
	}

	public IModule getExecutable() throws CorruptDataException {
		return null;
	}

	public long getProcessId() throws CorruptDataException {
		return -1;
	}

	public String getProcedureNameForAddress(long address)	throws DataUnavailableException, CorruptDataException {
		return getProcedureNameForAddress(address, false);
	}
	
	public String getProcedureNameForAddress(long address, boolean dtfjFormat)	throws DataUnavailableException, CorruptDataException {
		return "Error no process in this address space";
	}

	public Collection<? extends IOSThread> getThreads()	throws CorruptDataException {
		return Collections.emptySet();
	}

	public int getSignalNumber() throws DataUnavailableException {
		return 0;
	}

	public boolean isFailingProcess() throws DataUnavailableException {
		return false;
	}
	
}
