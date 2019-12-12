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
package com.ibm.j9ddr.corereaders.elf;

import java.io.IOException;
import java.util.Map;
import java.util.SortedMap;
import java.util.TreeMap;

import com.ibm.j9ddr.corereaders.InvalidDumpFormatException;

/**
 * @author Cheng Jin <jincheng@ca.ibm.com>
 *
 */
public class ELFRISCV64DumpReader extends ELFDumpReader
{
	private final static String[] registerNames = ("pc,ra,sp,gp,tp,t0,t1,t2,s0,s1,a0,a1,a2,a3,a4,a5,a6,a7,s2,s3,s4,s5,s6,s7,s8,s9,s10,s11"
					+ ",t3,t4,t5,t6,ft0,ft1,ft2,ft3,ft4,ft5,ft6,ft7,fs0,fs1,fa0,fa1,fa2,fa3,fa4,fa5,fa6,fa7"
					+ ",fs2,fs3,fs4,fs5,fs6,fs7,fs8,fs9,fs10,fs11,ft8,ft9,ft10,ft11,fcsr").split(",");

	protected ELFRISCV64DumpReader(ELFFileReader reader) throws IOException, InvalidDumpFormatException
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
		return "riscv64";
	}

	@Override
	protected long getBasePointerFrom(Map<String, Number> registers)
	{
		return getStackPointerFrom(registers);
	}

	@Override
	protected long getInstructionPointerFrom(Map<String, Number> registers)
	{
		return registers.get("pc").longValue();
	}

	@Override
	protected long getLinkRegisterFrom(Map<String, Number> registers)
	{
		return registers.get("ra").longValue();
	}

	@Override
	protected String getStackPointerRegisterName()
	{
		return "sp";
	}

	@Override
	protected String[] getDwarfRegisterKeys()
	{
		/* Need to consult the platform ABI to map register names to dwarf numbers */
		return registerNames.clone();
	}
	
	@Override
	protected SortedMap<String, Number> readRegisters() throws IOException
	{
		SortedMap<String, Number> registers = new TreeMap<String, Number>(new RegisterComparator());
		for (int i = 0; i < registerNames.length; i++)
			registers.put(registerNames[i], _reader.readLong());
		return registers;
	}
	
	@Override
	protected void readHighwordRegisters(DataEntry entry, Map<String, Number> registers) throws IOException, InvalidDumpFormatException
	{
		throw new InvalidDumpFormatException("Unexpected data entry in RISCV64 ELF dump");
	}

}
