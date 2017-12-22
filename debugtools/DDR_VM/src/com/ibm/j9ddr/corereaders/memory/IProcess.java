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

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.DataUnavailableException;
import com.ibm.j9ddr.corereaders.osthread.IOSThread;

/**
 * Models an OS process.
 * 
 * IProcess is-a IMemorySpace because a process knows how big a pointer is (on
 * zOS 32 and 64 bit processes can share the same 64 bit address space). It
 * would be unpleasant to ask an IProcess how big its pointers were, then ask
 * for its address space and then ask that for its memory.
 * 
 * @author andhall
 * 
 */
public interface IProcess extends IMemory
{

	/**
	 * 
	 * @return Address space this process uses.
	 */
	public IAddressSpace getAddressSpace();

	public long getPointerAt(long address) throws MemoryFault;

	/**
	 * 
	 * @return Number of bytes in a pointer
	 */
	public int bytesPerPointer();
	
	/**
	 * @return Process command line or null if the data is unavailable
	 * @throws CorruptDataException
	 */
	public String getCommandLine() throws DataUnavailableException, CorruptDataException;
	
	/**
	 * 
	 * @return Properties containing environment variables name=value pairs
	 * @throws CorruptDataException
	 */
	public Properties getEnvironmentVariables() throws DataUnavailableException, CorruptDataException;
	
	public Collection<? extends IModule> getModules() throws CorruptDataException;
	
	public IModule getExecutable() throws CorruptDataException;
	
	public long getProcessId() throws CorruptDataException;
	
	/**
	 * Equivalent to getProcedureNameForAddress(address, false).
	 * Default behaviour is to return DDR format strings for symbols.
	 */
	public String getProcedureNameForAddress(long address) throws DataUnavailableException, CorruptDataException;
	
	public String getProcedureNameForAddress(long address, boolean dtfjFormat) throws DataUnavailableException, CorruptDataException;
	
	public Collection<? extends IOSThread> getThreads() throws CorruptDataException;

	public int getSignalNumber() throws DataUnavailableException;
	
	public boolean isFailingProcess() throws DataUnavailableException;
}
