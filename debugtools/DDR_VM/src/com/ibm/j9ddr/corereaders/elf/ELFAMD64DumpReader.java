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
import java.util.Map;
import java.util.SortedMap;
import java.util.TreeMap;

import com.ibm.j9ddr.corereaders.InvalidDumpFormatException;

/**
 * @author andhall
 *
 */
public class ELFAMD64DumpReader extends ELFDumpReader
{

	protected ELFAMD64DumpReader(ELFFileReader reader) throws IOException,
			InvalidDumpFormatException
	{
		super(reader);
	}

	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.corereaders.elf.ELFDumpReader#readUID()
	 */
	@Override
	protected long readUID() throws IOException
	{
		return _reader.readInt() & 0xffffffffL;
	}

	@Override
	protected String getProcessorType()
	{
		return "amd64";
	}

	@Override
	protected long getBasePointerFrom(Map<String, Number> registers)
	{
		return registers.get("rbp").longValue();
	}

	@Override
	protected long getInstructionPointerFrom(Map<String, Number> registers)
	{
		return registers.get("rip").longValue();
	}

	@Override
	protected long getLinkRegisterFrom(Map<String, Number> registers)
	{
		return 0;
	}


	@Override
	protected String getStackPointerRegisterName() {
		return "rsp";
	}

	@Override
	protected String[] getDwarfRegisterKeys() {
		// Needs to be overridden by each platform. Need to consult the platform ABI to map reg names to dwarf numbers.
		String[] registerNames = new String[17];
		// Set the default r+n strings.
		for( int i = 0; i < registerNames.length; i++ ) {
			registerNames[i] = "r"+i;
		}
		// Override the registers with special names.
		registerNames[0] = "rax";
		registerNames[1] = "rdx";
		registerNames[2] = "rcx";
		registerNames[3] = "rbx";
		registerNames[4] = "rsi";
		registerNames[5] = "rdi";
		registerNames[6] = "rbp";
		registerNames[7] = "rsp";
		registerNames[16] = "rip";
		return registerNames;
	}
	
	@Override
	protected SortedMap<String, Number> readRegisters() throws IOException
	{
		String[] names1 = { "r15", "r14", "r13", "r12", "rbp", "rbx", "r11",
				"r10", "r9", "r8", "rax", "rcx", "rdx", "rsi", "rdi" };
		String[] names2 = { "rip", "cs", "eflags", "rsp", "ss", "fs_base",
				"gs_base", "ds", "es", "fs", "gs" };
		SortedMap<String, Number> registers = new TreeMap<String, Number>(new RegisterComparator());
		for (int i = 0; i < names1.length; i++)
			registers.put(names1[i], _reader.readLong());
		_reader.readLong(); // Ignore originalEax
		for (int i = 0; i < names2.length; i++)
			registers.put(names2[i], _reader.readLong());
		return registers;
	}
	
	@Override
	protected void readHighwordRegisters(DataEntry entry, Map<String, Number> registers) 
					throws IOException, InvalidDumpFormatException {
		throw new InvalidDumpFormatException("Unexpected data entry in AMD64 ELF dump");
	}

}
