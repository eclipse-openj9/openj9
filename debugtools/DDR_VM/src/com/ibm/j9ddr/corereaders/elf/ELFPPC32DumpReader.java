/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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



public class ELFPPC32DumpReader extends ELFDumpReader {
	
	protected ELFPPC32DumpReader(ELFFileReader reader) throws IOException, InvalidDumpFormatException {
		super(reader);
	}

	@Override
	protected long getBasePointerFrom(Map<String, Number> registers) {
		//Note:  Most RISC ISAs do not distinguish between base pointer and stack pointer
		return getStackPointerFrom(registers);
	}

	@Override
	protected long getInstructionPointerFrom(Map<String, Number> registers) {
		return registers.get("pc").longValue();
	}

	@Override
	protected long getLinkRegisterFrom(Map<String, Number> registers) {
		return registers.get("lr").longValue();
	}

	@Override
	protected String getProcessorType() {
		return "ppc";
	}

	@Override
	protected String getStackPointerRegisterName() {
		return "gpr1";
	}

	@Override
	protected SortedMap<String, Number> readRegisters() throws IOException {
		SortedMap<String, Number> registers = new TreeMap<String, Number>(new RegisterComparator());
		for (int i = 0; i < 32; i++) {
			registers.put("gpr" + i, Long.valueOf(_reader.readInt() & 0xFFFFFFFFL));
		}

		registers.put("pc", Long.valueOf(_reader.readInt() & 0xFFFFFFFFL));
		_reader.readInt(); // skip
		_reader.readInt(); // skip
		registers.put("ctr", Long.valueOf(_reader.readInt() & 0xFFFFFFFFL));
		registers.put("lr", Long.valueOf(_reader.readInt() & 0xFFFFFFFFL));
		registers.put("xer", Long.valueOf(_reader.readInt() & 0xFFFFFFFFL));
		registers.put("cr", Long.valueOf(_reader.readInt() & 0xFFFFFFFFL));
		
		return registers;
	}

	/*
	 * http://refspecs.linuxfoundation.org/ELF/ppc64/PPC-elf64abi-1.9.html#DW-REG
	 * @see com.ibm.j9ddr.corereaders.elf.ELFDumpReader#getDwarfRegisterKeys()
	 * (32 bit DWARF ids seem to be the same as 32 bit.)
	 */
	protected String[] getDwarfRegisterKeys() {
		// Needs to be overridden by each platform. Need to consult the platform ABI to map reg names to dwarf numbers.
		String[] registerNames = new String[132];
		
		// Set the default r+n strings.
		for( int i = 0; i < 32; i++ ) {
			registerNames[i] = "gpr"+i;
		}
		for( int i = 32; i < 64; i++ ) {
			registerNames[i] = "fpr"+i;
		}

		registerNames[64] = "ctr";
		registerNames[65] = "lr";
		
//		registerNames[108] = "lr";
//		registerNames[109] = "ctr";
	
		// Special purpose registers (which may have other names) are at 100 + regnum.
		// (32 may be enough!)
		for( int i = 100; i < 132; i++ ) {
			registerNames[i] = "spr"+i;
		}
		
		return registerNames;
	}
	
	@Override
	protected void readHighwordRegisters(DataEntry entry, Map<String, Number> registers)
		throws IOException, InvalidDumpFormatException {
			throw new InvalidDumpFormatException("Unexpected data entry in PPC32 ELF dump");
	}

	@Override
	protected long readUID() throws IOException {
		return _reader.readShort() & 0xffffL;
	}
	
}
