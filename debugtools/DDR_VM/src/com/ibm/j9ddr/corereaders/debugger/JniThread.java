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

import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedList;
import java.util.List;
import java.util.Properties;
import java.util.SortedMap;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.memory.IMemoryRange;
import com.ibm.j9ddr.corereaders.osthread.IOSStackFrame;
import com.ibm.j9ddr.corereaders.osthread.IOSThread;
import com.ibm.j9ddr.corereaders.osthread.IRegister;
import com.ibm.j9ddr.corereaders.osthread.Register;

class JniThread implements IOSThread
{
	private final JniRegisters registers;
	
	private final Properties properties;
	
	private final long tid;
	
	private final List<IMemoryRange> memoryRanges = new LinkedList<IMemoryRange>();
	
	private final List<IOSStackFrame> stackFrames = new LinkedList<IOSStackFrame>();
	
	private boolean stackWalked = false;
	
	JniThread(long tid, JniRegisters registers, Properties properties)
	{
		this.registers = registers;
		this.properties = properties;
		this.tid = tid;
	}
	
	public Collection<? extends IMemoryRange> getMemoryRanges()
	{
		if (! stackWalked) {
			walkStack();
		}
		
		return memoryRanges;
	}

	public Properties getProperties()
	{
		return properties;
	}

	
	public List<? extends IRegister> getRegisters()
	{
		SortedMap<String, Number> registerMap = registers.getRegisters();
		
		List<IRegister> regList = new ArrayList<IRegister>(registers.size());
		
		for (String regName : registerMap.keySet()) {
			Number value  = registerMap.get(regName);
			System.out.println("getRegisters: name " + regName + " val " + value);
			regList.add(new Register(regName,value));
		}
		
		return regList;
	}
	
	
	public long getInstructionPointer() 
	{	
		return registers.getInstructionPointer();
	}

	public List<? extends IOSStackFrame> getStackFrames()
	{
		if (! stackWalked) {
			walkStack();
		}
				
		return stackFrames;
	}
	
	private void walkStack()
	{	
		System.err.println("walkStack not implemented yet");
	}

	public long getThreadId() throws CorruptDataException
	{
		return tid;
	}

	public long getBasePointer() {
		//return getBasePointerFrom(registers);
		System.err.println("getBasePointer not implemented yet");
		return 0;
	}

	public long getStackPointer() {
		//return getStackPointerFrom(registers);
		System.err.println("getStackPointer not implemented yet");
		return 0;
	}
	
}
