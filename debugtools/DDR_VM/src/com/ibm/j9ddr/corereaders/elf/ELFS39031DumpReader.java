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

public class ELFS39031DumpReader extends ELFDumpReader {

	protected ELFS39031DumpReader(ELFFileReader reader) throws IOException,	InvalidDumpFormatException {
		super(reader);
	}

	@Override
	protected long getBasePointerFrom(Map<String, Number> registers) {
		return getStackPointerFrom(registers);
	}

	@Override
	protected long getInstructionPointerFrom(Map<String, Number> registers) {
		return registers.get("addr").longValue();
	}

	@Override
	protected long getLinkRegisterFrom(Map<String, Number> registers) {
		return 0;
	}

	@Override
	protected String getProcessorType() {
		return "s390";
	}

	@Override
	protected String getStackPointerRegisterName() {
		return "addr";
	}

	@Override
	protected SortedMap<String, Number> readRegisters() throws IOException {
		SortedMap<String, Number> registers = new TreeMap<String, Number>(new RegisterComparator());
		registers.put("mask", Long.valueOf(_reader.readInt() & 0x7FFFFFFFL));
		registers.put("addr", Long.valueOf(_reader.readInt() & 0x7FFFFFFFL));
		for (int i = 0; i < 16; i++)
			registers.put("gpr" + i, Long.valueOf(_reader.readInt() & 0x7FFFFFFFL));
		for (int i = 0; i < 16; i++)
			registers.put("acr" + i, Long.valueOf(_reader.readInt() & 0x7FFFFFFFL));
		registers.put("origgpr2", Long.valueOf(_reader.readInt() & 0x7FFFFFFFL));
		registers.put("trap", Long.valueOf(_reader.readInt() & 0x7FFFFFFFL));
		return registers;
	}
	
	/*
	 * https://refspecs.linuxbase.org/ELF/zSeries/lzsabi0_s390.html#DWARFREG
	 * @see com.ibm.j9ddr.corereaders.elf.ELFDumpReader#getDwarfRegisterKeys()
	 */
	protected String[] getDwarfRegisterKeys() {
		// Needs to be overridden by each platform. Need to consult the platform ABI to map reg names to dwarf numbers.
		String[] registerNames = new String[66];
		// Set the default r+n strings.
		for( int i = 0; i < 16; i++ ) {
			registerNames[i] = "gpr"+i;
		}
		registerNames[16] = "fpr0";
		registerNames[17] = "fpr2";
		registerNames[18] = "fpr4";
		registerNames[19] = "fpr6";
		registerNames[20] = "fpr1";
		registerNames[21] = "fpr3";
		registerNames[22] = "fpr5";
		registerNames[23] = "fpr7";
		registerNames[24] = "fpr8";
		registerNames[25] = "fpr10";
		registerNames[26] = "fpr12";
		registerNames[27] = "fpr14";
		registerNames[28] = "fpr9";
		registerNames[29] = "fpr11";
		registerNames[30] = "fpr13";
		registerNames[31] = "fpr15";

		for( int i = 0; i < 16; i++ ) {
			registerNames[i+32] = "cr"+i;
		}
		for( int i = 0; i < 16; i++ ) {
			registerNames[i+48] = "ar"+i;
		}

		registerNames[64] = "mask";

		registerNames[65] = "addr";

		return registerNames;
	}
	
	@Override
	protected void readHighwordRegisters(DataEntry entry, Map<String, Number> registers) 
					throws IOException, InvalidDumpFormatException {
		_reader.seek(entry.offset);
		for (int i = 0; i < 16; i++)
			registers.put("hgpr" + i, _reader.readInt());
		return;
	}

	@Override
	protected long readUID() throws IOException {
		return _reader.readShort() & 0xffffL;
	}

	@Override
	protected long maskInstructionPointer(long pointer) {
		return pointer & 0x7FFFFFFF;
	}
	
}
