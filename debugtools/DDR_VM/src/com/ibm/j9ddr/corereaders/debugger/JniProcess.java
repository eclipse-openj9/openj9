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

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Properties;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.DataUnavailableException;
import com.ibm.j9ddr.corereaders.memory.IAddressSpace;
import com.ibm.j9ddr.corereaders.memory.IModule;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;
import com.ibm.j9ddr.corereaders.osthread.IOSThread;

public class JniProcess extends JniSearchableMemory implements IProcess 
{	
	/**
	 * Following information should match the target architecture values in runtime/ddrext/ddr.h
	 * 
	 * For instance: 
	 * TARGET_ARCH_X86_32 should match TARGET_TYPE_X86_32
	 * 
	 * 
	 */
	public static final int TARGET_TYPE_UNKNOWN	= 0;
	public static final int TARGET_TYPE_X86_32	= 1;
	public static final int TARGET_TYPE_X86_64	= 2;
	public static final int TARGET_TYPE_S390_31	= 3;
	public static final int TARGET_TYPE_S390_64	= 4;
	public static final int TARGET_TYPE_PPC_32	= 5;
	public static final int TARGET_TYPE_PPC_64	= 6;
	public static final int TARGET_TYPE_ARM		= 7;
	public static final int TARGET_TYPE_IA64	= 8;
	
	private final JniAddressSpace addressSpace;
	
	private JniRegisters    _registers; 
	private List<DataEntry> _threadEntries = new ArrayList<DataEntry>();


	public native int bytesPerPointer();
	public native int getSignalNumber() throws DataUnavailableException;
	public native long getProcessId() throws CorruptDataException;
	public native long[] getThreadIds();
	public native int getTargetArchitecture();
	
	
	
	public JniProcess(JniAddressSpace addressSpace)
	{
		this.addressSpace = addressSpace;

		try {
			_registers = getRegisterFile();
		} catch (DataUnavailableException e) {
			PrintWriter outputStream = new PrintWriter(new JniOutputStream());
			e.printStackTrace(outputStream);
		}
	}

	private JniRegisters getRegisterFile() throws DataUnavailableException
	{
		int targetProcessorType = getTargetArchitecture();
		
		switch (targetProcessorType) {
			case TARGET_TYPE_X86_32:
				return new JniRegistersIA32();				
			case TARGET_TYPE_X86_64:
				return new JniRegistersAMD64();
			case TARGET_TYPE_PPC_32:
				return new JniRegistersPPC32();
			case TARGET_TYPE_PPC_64:
				return new JniRegistersPPC64();
			case TARGET_TYPE_S390_31:
				return new JniRegistersS39031();
			case TARGET_TYPE_S390_64:
				return new JniRegistersS39064();
			default:
				throw new DataUnavailableException("Architecture type " + targetProcessorType + " not implemented");
		}
		
	}
	
	public long getPointerAt(long address) throws MemoryFault
	{
		return super.getPointerAt(address, this.bytesPerPointer());		
	}
	

	public String getCommandLine() throws DataUnavailableException, CorruptDataException 
	{
		throw new DataUnavailableException("not implemented yet");
	}

	public Properties getEnvironmentVariables() throws DataUnavailableException, CorruptDataException 
	{
		throw new DataUnavailableException("not implemented yet");
	}

	public String getProcedureNameForAddress(long address) throws DataUnavailableException, CorruptDataException 
	{
		throw new DataUnavailableException("not implemented yet");
	}
	
	public String getProcedureNameForAddress(long address, boolean dtfjFormat) throws DataUnavailableException, CorruptDataException 
	{
		throw new DataUnavailableException("not implemented yet");
	}

	public IAddressSpace getAddressSpace() 
	{
		return addressSpace;
	}

	public IModule getExecutable() throws CorruptDataException 
	{
		throw new CorruptDataException("Unimplemented");
	}

	public Collection<? extends IModule> getModules() throws CorruptDataException 
	{
		throw new CorruptDataException("Unimplemented");
	}

	public Collection<? extends IOSThread> getThreads() throws CorruptDataException 
	{
		
		List<IOSThread> threads = new ArrayList<IOSThread>(_threadEntries.size());
				
		/* go get a list of current thread ids */
		long tidList[] = getThreadIds();		
		if (tidList == null) {
			return null;
		}
				
		for (long tid : tidList) {			
			/* Get current registers for given thread */
			_registers.readRegisters(tid);
			/* Inflate the thread */	
			IOSThread thread = new JniThread(tid, _registers, null);
			threads.add(thread);
		}

		return threads;
	}
	
	public boolean isFailingProcess() throws DataUnavailableException 
	{
		throw new DataUnavailableException("Not available here");
	}
}
