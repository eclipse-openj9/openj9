/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2020 IBM Corp. and others
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
package com.ibm.dtfj.corereaders;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Properties;
import java.util.Set;
import java.util.TreeMap;

import javax.imageio.stream.ImageInputStream;

/**
 * struct and constant reference: /usr/include/elf.h
 */
public class NewElfDump extends CoreReaderSupport {
	private static final int ELF_NOTE_HEADER_SIZE = 12; // 3 32-bit words
	private final static int EI_NIDENT = 16;
	private final static int ELF_PRARGSZ = 80;

	private final static int ELFCLASS32 = 1;
	private final static int ELFCLASS64 = 2;

	private final static int ELFDATA2LSB = 1;
	private final static int ELFDATA2MSB = 2;

	private final static int ARCH_IA32 = 3;
	private final static int ARCH_PPC32 = 20;
	private final static int ARCH_PPC64 = 21;
	private final static int ARCH_S390 = 22;
	private final static int ARCH_ARM32 = 40;
	private final static int ARCH_IA64 = 50;
	private final static int ARCH_AMD64 = 62;
	private final static int ARCH_AARCH64 = 183;
	private final static int ARCH_RISCV64 = 243;

	private final static int DT_NULL = 0;
	private final static int DT_STRTAB = 5;
	private final static int DT_SONAME = 14;
	private final static int DT_DEBUG = 21;

	private static final int NT_PRSTATUS = 1; // prstatus_t
	private static final int NT_PRPSINFO = 3; // prpsinfo_t
	private static final int NT_AUXV = 6; // contains copy of auxv array
	public static final int NT_FILE = 0x46494c45; // contains list of mapped files

	private static final int AT_NULL = 0; // End of vector
	private static final int AT_PLATFORM = 15; // String identifying platform

	private static final String SYSTEM_PROP_EXECUTABLE = "com.ibm.dtfj.corereaders.executable"; //$NON-NLS-1$

	private static abstract class Address {
		private final long _value;
		Address(long value) {
			this._value = value;
		}
		long getValue() {
			return _value;
		}
		abstract Address add(long offset);
		boolean isNil() {
			return 0L == getValue();
		}
		abstract Number asNumber();
		abstract long asAddress();
		abstract Address nil();
	}

	private static class Address32 extends Address {
		Address32(int value) {
			super(value & 0xffffffffL);
		}
		@Override
		Address add(long offset) {
			long result = getValue() + offset;
			return new Address32((int) result);
		}
		@Override
		Number asNumber() {
			return Integer.valueOf((int) getValue());
		}
		@Override
		long asAddress() {
			return getValue();
		}
		@Override
		Address nil() {
			return new Address32(0);
		}
	}

	/**
	 * An Address31 is-a Address32 with the single difference that when
	 * called to give its value as an address, it masks out the top bit.
	 * It is only used for 31-bit S390 addresses.
	 *
	 * [The following comment for 159595 was originally above ArchS390_32.readRegisters]
	 * CMVC 159595 : Linux S390 DTFJ tests.junit.ImageStackFrameTest failure
	 * The cause of this problem is that SLES 11 now correctly represents the
	 * z/linux 31 bit environment, including setting the high bit. This means that the
	 * registers are not being read correctly and need to mask out the high bit.
	 * All calls to read a word as an address are now handled in a 31 bit manner, however
	 * this does not fix the issue of whether or not the register actually holds an address or not.
	 *
	 * CMVC 184757 : j2se_cmdLineTester_DTFJ fails
	 * The original fix for 159595 was masking out the top bit when the register value was
	 * read in. This meant that it was permanently removed and wrongly appearing with its
	 * top bit turned off in jdmpview's "info thread *" output.
	 * Now, keep the value and only turn off the top bit when the object is asked for its
	 * value as an address i.e. when asAddressValue() is called
	 */
	private static class Address31 extends Address32 {
		Address31(int value) {
			super(value);
		}

		@Override
		long asAddress() {
			int address = 0x7FFFFFFF & (int) getValue();
			return address;
		}
	}

	private static class Address64 extends Address {
		Address64(long value) {
			super(value);
		}

		@Override
		Address add(long offset) {
			return new Address64(getValue() + offset);
		}

		@Override
		Number asNumber() {
			return Long.valueOf(getValue());
		}

		@Override
		long asAddress() {
			return getValue();
		}

		@Override
		Address nil() {
			return new Address64(0);
		}
	}

	private static interface Arch {
		Map<String, Address> readRegisters(ElfFile file, Builder builder, Object addressSpace) throws IOException;
		long readUID(ElfFile file) throws IOException;
		Address getStackPointerFrom(Map<String, Address> registers);
		Address getBasePointerFrom(Map<String, Address> registers);
		Address getInstructionPointerFrom(Map<String, Address> registers);
		Address getLinkRegisterFrom(Map<String, Address> registers);
	}

	private static class ArchPPC64 implements Arch {
		ArchPPC64() {
			super();
		}

		@Override
		public Map<String, Address> readRegisters(ElfFile file, Builder builder, Object addressSpace) throws IOException {
			Map<String, Address> registers = new TreeMap<>();
			for (int i = 0; i < 32; i++) {
				registers.put("gpr" + i, file.readElfWordAsAddress()); //$NON-NLS-1$
			}

			registers.put("pc", file.readElfWordAsAddress()); //$NON-NLS-1$
			file.readElfWordAsAddress(); // skip
			file.readElfWordAsAddress(); // skip
			registers.put("ctr", file.readElfWordAsAddress()); //$NON-NLS-1$
			registers.put("lr", file.readElfWordAsAddress()); //$NON-NLS-1$

			// xer is a 32-bit register. Read 64 bits then shift right
			long l = (file.readLong() >> 32) & 0xffffffffL;
			registers.put("xer", new Address64(l)); //$NON-NLS-1$

			// cr is a 32-bit register. Read 64 bits then shift right
			l = (file.readLong() >> 32) & 0xffffffffL;
			registers.put("cr", new Address64(l)); //$NON-NLS-1$

			return registers;
		}

		@Override
		public Address getStackPointerFrom(Map<String, Address> registers) {
			return registers.get("gpr1"); //$NON-NLS-1$
		}

		@Override
		public Address getBasePointerFrom(Map<String, Address> registers) {
			// Note: Most RISC ISAs do not distinguish between base pointer and stack pointer
			return getStackPointerFrom(registers);
		}

		@Override
		public Address getInstructionPointerFrom(Map<String, Address> registers) {
			return registers.get("pc"); //$NON-NLS-1$
		}

		@Override
		public Address getLinkRegisterFrom(Map<String, Address> registers) {
			return registers.get("lr"); //$NON-NLS-1$
		}

		@Override
		public long readUID(ElfFile file) throws IOException {
			return file.readInt() & 0xffffffffL;
		}
	}

	private static class ArchPPC32 extends ArchPPC64 {
		ArchPPC32() {
			super();
		}

		@Override
		public Map<String, Address> readRegisters(ElfFile file, Builder builder, Object addressSpace) throws IOException {
			Map<String, Address> registers = new TreeMap<>();
			for (int i = 0; i < 32; i++) {
				registers.put("gpr" + i, file.readElfWordAsAddress()); //$NON-NLS-1$
			}

			registers.put("pc", file.readElfWordAsAddress()); //$NON-NLS-1$
			file.readElfWordAsAddress(); // skip
			file.readElfWordAsAddress(); // skip
			registers.put("ctr", file.readElfWordAsAddress()); //$NON-NLS-1$
			registers.put("lr", file.readElfWordAsAddress()); //$NON-NLS-1$
			registers.put("xer", file.readElfWordAsAddress()); //$NON-NLS-1$
			registers.put("cr", file.readElfWordAsAddress()); //$NON-NLS-1$

			return registers;
		}

		@Override
		public long readUID(ElfFile file) throws IOException {
			return file.readShort() & 0xffffL;
		}
	}

	private static class ArchS390 implements Arch {
		ArchS390() {
			super();
		}

		@Override
		public Map<String, Address> readRegisters(ElfFile file, Builder builder, Object addressSpace) throws IOException {
			Map<String, Address> registers = new TreeMap<>();
			registers.put("mask", file.readElfWordAsAddress()); //$NON-NLS-1$
			registers.put("addr", file.readElfWordAsAddress()); //$NON-NLS-1$
			for (int i = 0; i < 16; i++) {
				registers.put("gpr" + i, file.readElfWordAsAddress()); //$NON-NLS-1$
			}
			for (int i = 0; i < 16; i++) {
				registers.put("acr" + i, file.readElfWordAsAddress()); //$NON-NLS-1$
			}
			registers.put("origgpr2", file.readElfWordAsAddress()); //$NON-NLS-1$
			registers.put("trap", file.readElfWordAsAddress()); //$NON-NLS-1$
			return registers;
		}

		@Override
		public Address getStackPointerFrom(Map<String, Address> registers) {
			return registers.get("gpr15"); //$NON-NLS-1$
		}

		@Override
		public Address getBasePointerFrom(Map<String, Address> registers) {
			return getStackPointerFrom(registers);
		}

		@Override
		public Address getInstructionPointerFrom(Map<String, Address> registers) {
			return registers.get("addr"); //$NON-NLS-1$
		}

		@Override
		public Address getLinkRegisterFrom(Map<String, Address> registers) {
			return nil(registers);
		}

		@Override
		public long readUID(ElfFile file) throws IOException {
			return file.readInt() & 0xffffffffL;
		}

		private static Address nil(Map<String, Address> registers) {
			Address gpr15 = registers.get("gpr15"); //$NON-NLS-1$
			return gpr15.nil();
		}
	}

	private static class ArchS390_32 extends ArchS390 {
		ArchS390_32() {
			super();
		}

		/**
		 * CMVC 184757 : j2se_cmdLineTester_DTFJ fails
		 *
		 * Just an observation on the code that follows - it creates Address31 objects but really it
		 * cannot know whether the values are addresses or not. Really, it ought to be creating "RegisterValue"
		 * objects (or some such name) which are then sometimes interpreted as addresses when needed.
		 * However the confusion between addresses and values is pervasive throughout the code that
		 * reads registers, for all platforms, and is not worth fixing. It usually only matters on Z-31
		 */
		@Override
		public Map<String, Address> readRegisters(ElfFile file, Builder builder, Object addressSpace) throws IOException {
			Map<String, Address> registers = new TreeMap<>();
			registers.put("mask", new Address31(file.readInt())); //$NON-NLS-1$
			registers.put("addr", new Address31(file.readInt())); //$NON-NLS-1$
			for (int i = 0; i < 16; i++) {
				registers.put("gpr" + i, new Address31(file.readInt())); //$NON-NLS-1$
			}
			for (int i = 0; i < 16; i++) {
				registers.put("acr" + i, new Address31(file.readInt())); //$NON-NLS-1$
			}
			registers.put("origgpr2", new Address31(file.readInt())); //$NON-NLS-1$
			registers.put("trap", new Address31(file.readInt())); //$NON-NLS-1$
			registers.put("old_ilc", new Address31(file.readInt())); //$NON-NLS-1$
			return registers;
		}

		@Override
		public long readUID(ElfFile file) throws IOException {
			return file.readShort() & 0xffffL;
		}
	}

	private static class ArchIA32 implements Arch {
		ArchIA32() {
			super();
		}

		@Override
		public Map<String, Address> readRegisters(ElfFile file, Builder builder, Object addressSpace) throws IOException {
			@SuppressWarnings("nls")
			String[] names1 = { "ebx", "ecx", "edx", "esi", "edi", "ebp", "eax", "ds", "es", "fs", "gs" };
			@SuppressWarnings("nls")
			String[] names2 = { "eip", "cs", "efl", "esp", "ss" };
			Map<String, Address> registers = new TreeMap<>();
			for (String name : names1) {
				registers.put(name, new Address32(file.readInt()));
			}
			file.readInt(); // Ignore originalEax
			for (String name : names2) {
				registers.put(name, new Address32(file.readInt()));
			}
			return registers;
		}

		@Override
		public Address getStackPointerFrom(Map<String, Address> registers) {
			return registers.get("esp"); //$NON-NLS-1$
		}

		@Override
		public Address getBasePointerFrom(Map<String, Address> registers) {
			return registers.get("ebp"); //$NON-NLS-1$
		}

		@Override
		public Address getInstructionPointerFrom(Map<String, Address> registers) {
			return registers.get("eip"); //$NON-NLS-1$
		}

		@Override
		public Address getLinkRegisterFrom(Map<String, Address> registers) {
			return new Address32(0);
		}

		@Override
		public long readUID(ElfFile file) throws IOException {
			return file.readShort() & 0xffffL;
		}
	}

	private static class ArchIA64 implements Arch {
		ArchIA64() {
			super();
		}

		@Override
		public Map<String, Address> readRegisters(ElfFile file, Builder builder, Object addressSpace) throws IOException {
			// Ignore the R Set of 32 registers
			for (int i = 0; i < 32; i++)
				file.readLong();

			Map<String, Address> registers = new TreeMap<>();
			registers.put("nat", new Address64(file.readLong())); //$NON-NLS-1$
			registers.put("pr", new Address64(file.readLong())); //$NON-NLS-1$

			// Ignore the B Set of 8 registers
			for (int i = 0; i < 8; i++) {
				file.readLong();
			}

			@SuppressWarnings("nls")
			String[] names = { "ip", "cfm", "psr", "rsc", "bsp", "bspstore", "rnat", "ccv", "unat", "fpsr", "pfs", "lc", "ec" };
			for (String name : names) {
				registers.put(name, new Address64(file.readLong()));
			}

			// The register structure contains 128 words: 32 R, 8 B, 15 irregular and 73 reserved.
			// It is not really necessary to read the reserved words.
			// for (int i = 0; i < 73; i++)
			//     reader.readLong();

			return registers;
		}

		@Override
		public Address getStackPointerFrom(Map<String, Address> registers) {
			return new Address64(0);
		}

		@Override
		public Address getBasePointerFrom(Map<String, Address> registers) {
			return new Address64(0);
		}

		@Override
		public Address getInstructionPointerFrom(Map<String, Address> registers) {
			return new Address64(0);
		}

		@Override
		public Address getLinkRegisterFrom(Map<String, Address> registers) {
			return new Address64(0);
		}

		@Override
		public long readUID(ElfFile file) throws IOException {
			return file.readInt() & 0xffffffffL;
		}
	}

	private static class ArchAMD64 implements Arch {
		ArchAMD64() {
			super();
		}

		@Override
		public Map<String, Address> readRegisters(ElfFile file, Builder builder, Object addressSpace) throws IOException {
			@SuppressWarnings("nls")
			String[] names1 = { "r15", "r14", "r13", "r12", "rbp", "rbx", "r11", "r10", "r9", "r8", "rax", "rcx", "rdx", "rsi", "rdi" };
			@SuppressWarnings("nls")
			String[] names2 = { "rip", "cs", "eflags", "rsp", "ss", "fs_base", "gs_base", "ds", "es", "fs", "gs" };
			Map<String, Address> registers = new TreeMap<>();
			for (String name : names1) {
				registers.put(name, new Address64(file.readLong()));
			}
			file.readLong(); // Ignore originalEax
			for (String name : names2) {
				registers.put(name, new Address64(file.readLong()));
			}
			return registers;
		}

		@Override
		public Address getStackPointerFrom(Map<String, Address> registers) {
			return registers.get("rsp"); //$NON-NLS-1$
		}

		@Override
		public Address getBasePointerFrom(Map<String, Address> registers) {
			return registers.get("rbp"); //$NON-NLS-1$
		}

		@Override
		public Address getInstructionPointerFrom(Map<String, Address> registers) {
			return registers.get("rip"); //$NON-NLS-1$
		}

		@Override
		public Address getLinkRegisterFrom(Map<String, Address> registers) {
			return new Address64(0);
		}

		@Override
		public long readUID(ElfFile file) throws IOException {
			return file.readInt() & 0xffffffffL;
		}
	}

	private static class ArchARM32 implements Arch {
		ArchARM32() {
			super();
		}

		@Override
		public Map<String, Address> readRegisters(ElfFile file, Builder builder, Object addressSpace) throws IOException {
			Map<String, Address> registers = new TreeMap<>();
			for (int i = 0; i < 13; i++) {
				registers.put("r" + i, new Address32(file.readInt())); //$NON-NLS-1$
			}
			// The next registers have names
			registers.put("sp", new Address32(file.readInt())); //$NON-NLS-1$
			registers.put("lr", new Address32(file.readInt())); //$NON-NLS-1$
			registers.put("pc", new Address32(file.readInt())); //$NON-NLS-1$
			registers.put("spsr", new Address32(file.readInt())); //$NON-NLS-1$

			return registers;
		}

		@Override
		public Address getStackPointerFrom(Map<String, Address> registers) {
			return registers.get("sp"); //$NON-NLS-1$
		}

		@Override
		public Address getBasePointerFrom(Map<String, Address> registers) {
			return registers.get("sp"); //$NON-NLS-1$
		}

		@Override
		public Address getInstructionPointerFrom(Map<String, Address> registers) {
			return registers.get("pc"); //$NON-NLS-1$
		}

		@Override
		public Address getLinkRegisterFrom(Map<String, Address> registers) {
			return registers.get("lr"); //$NON-NLS-1$
		}

		@Override
		public long readUID(ElfFile file) throws IOException {
			return file.readInt() & 0xffffffffL;
		}
	}

	private static class ArchAARCH64 implements Arch {
		ArchAARCH64() {
			super();
		}

		@Override
		public Map<String, Address> readRegisters(ElfFile file, Builder builder, Object addressSpace) throws IOException {
			Map<String, Address> registers = new TreeMap<>();
			for (int i = 0; i < 30; i++) {
				registers.put("x" + i, new Address64(file.readLong())); //$NON-NLS-1$
			}
			// The next registers have names
			registers.put("lr", new Address64(file.readLong())); //$NON-NLS-1$
			registers.put("sp", new Address64(file.readLong())); //$NON-NLS-1$
			registers.put("pc", new Address64(file.readLong())); //$NON-NLS-1$
			registers.put("pstate", new Address64(file.readLong())); //$NON-NLS-1$

			return registers;
		}

		@Override
		public Address getStackPointerFrom(Map<String, Address> registers) {
			return registers.get("sp"); //$NON-NLS-1$
		}

		@Override
		public Address getBasePointerFrom(Map<String, Address> registers) {
			return getStackPointerFrom(registers);
		}

		@Override
		public Address getInstructionPointerFrom(Map<String, Address> registers) {
			return registers.get("pc"); //$NON-NLS-1$
		}

		@Override
		public Address getLinkRegisterFrom(Map<String, Address> registers) {
			return registers.get("lr"); //$NON-NLS-1$
		}

		@Override
		public long readUID(ElfFile file) throws IOException {
			return file.readInt() & 0xffffffffL;
		}
	}

	private static class ArchRISCV64 implements Arch {
		ArchRISCV64() {
			super();
		}

		@Override
		public Map<String, Address> readRegisters(ElfFile file, Builder builder, Object addressSpace) throws IOException {
			@SuppressWarnings("nls")
			String[] registerNames = { "pc", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0", "s1",
					"a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "s2", "s3",
					"s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4",
					"t5", "t6", "ft0", "ft1", "ft2", "ft3", "ft4", "ft5", "ft6", "ft7",
					"fs0", "fs1", "fa0", "fa1", "fa2", "fa3", "fa4", "fa5", "fa6", "fa7",
					"fs2", "fs3", "fs4", "fs5", "fs6", "fs7", "fs8", "fs9", "fs10", "fs11",
					"ft8", "ft9", "ft10", "ft11", "fcsr" };
			Map<String, Address> registers = new TreeMap<>();
			for (String name : registerNames) {
				registers.put(name, new Address64(file.readLong()));
			}
			return registers;
		}

		@Override
		public Address getStackPointerFrom(Map<String, Address> registers) {
			return registers.get("sp"); //$NON-NLS-1$
		}

		@Override
		public Address getBasePointerFrom(Map<String, Address> registers) {
			return getStackPointerFrom(registers);
		}

		@Override
		public Address getInstructionPointerFrom(Map<String, Address> registers) {
			return registers.get("pc"); //$NON-NLS-1$
		}

		@Override
		public Address getLinkRegisterFrom(Map<String, Address> registers) {
			return registers.get("ra"); //$NON-NLS-1$
		}

		@Override
		public long readUID(ElfFile file) throws IOException {
			return file.readInt() & 0xffffffffL;
		}
	}

	private static class DataEntry {
		final long offset;
		final long size;

		DataEntry(long offset, long size) {
			this.offset = offset;
			this.size = size;
		}
	}

	private static class Symbol {
		private static final byte STT_FUNC = 2;
		private static final byte ST_TYPE_MASK = 0xf;

		final long name;
		final long value;
		private final byte _info;

		/**
		 * Construct a Symbol.
		 *
		 * @param name - the offset of the symbol name in the strings section
		 * @param value - the value of the symbol
		 * @param size - unused
		 * @param info - identifies the type of the symbol
		 * @param other - unused
		 * @param sectionIndex - unused
		 */
		Symbol(long name, long value, long size, byte info, byte other, int sectionIndex) {
			this.name = name;
			this.value = value;
			this._info = info;
		}

		boolean isFunction() {
			return STT_FUNC == (_info & ST_TYPE_MASK);
		}
	}

	private static class SectionHeaderEntry {
		private static final int SHT_SYMTAB = 2; // Symbol table in this section
		private static final int SHT_DYNSYM = 11; // Symbol table in this section

		final long _name; // index into the section header string table
		final long _type;
		final long offset;
		final long size;
		final long link;

		/**
		 * Construct a SectionHeaderEntry.
		 *
		 * @param name - the index into the section header string table
		 * @param type - the type of section
		 * @param flags - unused
		 * @param address - unused
		 * @param offset - the file offset of the section
		 * @param size - the size of the section
		 * @param link - the index of a related section
		 * @param info - unused
		 */
		SectionHeaderEntry(long name, long type, long flags, long address, long offset, long size, long link, long info) {
			this._name = name;
			this._type = type;
			this.offset = offset;
			this.size = size;
			this.link = link;
		}

		boolean isSymbolTable() {
			return SHT_SYMTAB == _type || SHT_DYNSYM == _type;
		}
	}

	private static class ProgramHeaderEntry {
		// Program Headers defined in /usr/include/linux/elf.h
		// type constants
		private static final int PT_LOAD = 1;
		private static final int PT_DYNAMIC = 2;
		private static final int PT_NOTE = 4;
		private static final int PF_X = 1;
		private static final int PF_W = 2;

		// PHE structure definitions:
		private int _type;
		final long fileOffset;
		final long fileSize; // if fileSize == 0 then we have to map memory from external lib file.
		final long virtualAddress;
		final long memorySize;
		private int _flags;

		/**
		 * Construct a ProgramHeaderEntry.
		 *
		 * @param type - the type of program header
		 * @param fileOffset - the file offset of the related section
		 * @param fileSize - the size in bytes in the file image
		 * @param virtualAddress - the virtual address of the segment
		 * @param physicalAddress - unused
		 * @param memorySize - the size in bytes in virtual memory
		 * @param flags - the program header flags
		 * @param alignment - unused
		 */
		ProgramHeaderEntry(int type, long fileOffset, long fileSize, long virtualAddress, long physicalAddress, long memorySize, int flags, long alignment) {
			this._type = type;
			this.fileOffset = fileOffset;
			this.fileSize = fileSize;
			this.virtualAddress = virtualAddress;
			this.memorySize = memorySize;
			this._flags = flags;
		}

		boolean isEmpty() {
			// If the core is not a complete dump of the address space, there will be missing pieces.
			// These are manifested as program header entries with a real memory size but a zero file size.
			// That is, even though they do occupy space in memory, they are represented in none of the code.
			return 0 == fileSize;
		}

		boolean isDynamic() {
			return PT_DYNAMIC == _type;
		}

		boolean isLoadable() {
			return PT_LOAD == _type;
		}

		boolean isNote() {
			return PT_NOTE == _type;
		}

		MemoryRange asMemoryRange() {
			// if (0 == fileSize) {
			//     // See comments to CMVC 137737
			//     return null;
			// }
			boolean w = (_flags & PF_W) != 0;
			boolean x = (_flags & PF_X) != 0;
			return new MemoryRange(virtualAddress, fileOffset, memorySize, 0, false, !w, x, !isEmpty());
		}

		boolean validInProcess(long address) {
			return virtualAddress <= address && address < virtualAddress + memorySize;
		}

		boolean contains(long address) {
			return false == isEmpty() && validInProcess(address);
		}
	}

	private static abstract class ElfFile {
		final DumpReader _reader;
		private final long _offset;
		Arch _arch = null;
		private long _programHeaderOffset = -1;
		private long _sectionHeaderOffset = -1;
		private short _programHeaderEntrySize = 0;
		private short _programHeaderCount = 0;
		private short _sectionHeaderEntrySize = 0;
		private short _sectionHeaderCount = 0;
		// Maps to a set of paths of loaded shared libraries for a particular 'soname'.
		final Map<String, Set<String>> _librariesBySOName = new HashMap<>();
		private List<DataEntry> _processEntries = new ArrayList<>();
		private List<DataEntry> _threadEntries = new ArrayList<>();
		private List<DataEntry> _auxiliaryVectorEntries = new ArrayList<>();
		private List<ProgramHeaderEntry> _programHeaderEntries = new ArrayList<>();
		private List<SectionHeaderEntry> _sectionHeaderEntries = new ArrayList<>();

		private short _objectType = 0;
		private int _version = 0;
		private int _e_flags = 0;

		protected abstract long padToWordBoundary(long l);
		protected abstract ProgramHeaderEntry readProgramHeaderEntry() throws IOException;
		protected abstract long readElfWord() throws IOException;
		protected abstract Address readElfWordAsAddress() throws IOException;

		ElfFile(DumpReader reader) {
			this(reader, 0);
		}

		ElfFile(DumpReader reader, long offset) {
			_reader = reader;
			_offset = offset;
		}

		void close() throws IOException {
			_reader.releaseResources();
		}

		Iterator<DataEntry> processEntries() {
			return _processEntries.iterator();
		}

		Iterator<DataEntry> threadEntries() {
			return _threadEntries.iterator();
		}

		Iterator<DataEntry> auxiliaryVectorEntries() {
			return _auxiliaryVectorEntries.iterator();
		}

		Iterator<ProgramHeaderEntry> programHeaderEntries() {
			return _programHeaderEntries.iterator();
		}

		Iterator<SectionHeaderEntry> sectionHeaderEntries() {
			return _sectionHeaderEntries.iterator();
		}

		SectionHeaderEntry sectionHeaderEntryAt(int i) {
			return _sectionHeaderEntries.get(i);
		}

		protected void readFile() throws IOException {
			seek(0);
			readHeader();
			readSectionHeader();
			readProgramHeader();
		}

		public static boolean isELF(byte[] signature) {
			// 0x7F, 'E', 'L', 'F'
			return (0x7F == signature[0] && 0x45 == signature[1] && 0x4C == signature[2] && 0x46 == signature[3]);
		}

		private void readHeader() throws IOException {
			byte[] signature = readBytes(EI_NIDENT);

			if (!isELF(signature)) {
				throw new IOException("Missing ELF magic number"); //$NON-NLS-1$
			}

			_objectType = readShort(); //  objectType
			short machineType = readShort();
			_arch = architectureFor(machineType);
			_version = readInt(); // Ignore version
			readElfWord(); // Ignore entryPoint
			_programHeaderOffset = readElfWord();
			_sectionHeaderOffset = readElfWord();
			_e_flags = readInt(); //  e_flags
			readShort(); // Ignore elfHeaderSize
			_programHeaderEntrySize = readShort();
			_programHeaderCount = readShort();
			_sectionHeaderEntrySize = readShort();
			_sectionHeaderCount = readShort();
			readShort(); // Ignore stringTable
		}

		protected Arch architectureFor(short s) {
			switch (s) {
			case ARCH_PPC32:
				return new ArchPPC32();
			case ARCH_PPC64:
				return new ArchPPC64();
			case ARCH_IA64:
				return new ArchIA64();
			case ARCH_S390:
				return (addressSize() == 32) ? new ArchS390_32() : new ArchS390();
			case ARCH_IA32:
				return new ArchIA32();
			case ARCH_AMD64:
				return new ArchAMD64();
			case ARCH_ARM32:
				return new ArchARM32();
			case ARCH_AARCH64:
				return new ArchAARCH64();
			case ARCH_RISCV64:
				return new ArchRISCV64();
			default:
				// TODO throw exception for unsupported arch?
				return null;
			}
		}

		private SectionHeaderEntry readSectionHeaderEntry() throws IOException {
			long name = readInt() & 0xffffffffL;
			long type = readInt() & 0xffffffffL;
			long flags = readElfWord();
			long address = readElfWord();
			long offset = readElfWord();
			long size = readElfWord();
			long link = readInt() & 0xffffffffL;
			long info = readInt() & 0xffffffffL;
			return new SectionHeaderEntry(name, type, flags, address, offset, size, link, info);
		}

		private void readSectionHeader() throws IOException {
			for (int i = 0; i < _sectionHeaderCount; i++) {
				seek(_sectionHeaderOffset + i * _sectionHeaderEntrySize);
				SectionHeaderEntry entry = readSectionHeaderEntry();
				_sectionHeaderEntries.add(entry);
			}
		}

		private void readProgramHeader() throws IOException {
			for (int i = 0; i < _programHeaderCount; i++) {
				seek(_programHeaderOffset + i * _programHeaderEntrySize);
				_programHeaderEntries.add(readProgramHeaderEntry());
			}
			for (Iterator<ProgramHeaderEntry> iter = _programHeaderEntries.iterator(); iter.hasNext();) {
				ProgramHeaderEntry entry = iter.next();
				if (entry.isNote()) {
					readNotes(entry);
				}
			}
		}

		private void readNotes(ProgramHeaderEntry entry) throws IOException {
			long offset = entry.fileOffset;
			long limit = offset + entry.fileSize;
			while (offset < limit) {
				offset = readNote(offset);
			}
		}

		private long readNote(long offset) throws IOException {
			seek(offset);
			long nameLength = readInt();
			long dataSize = readInt();
			long type = readInt();

			long dataOffset = offset + ELF_NOTE_HEADER_SIZE + padToWordBoundary(nameLength);

			if (NT_PRSTATUS == type) {
				addThreadEntry(dataOffset, dataSize);
			} else if (NT_PRPSINFO == type) {
				addProcessEntry(dataOffset, dataSize);
			} else if (NT_AUXV == type) {
				addAuxiliaryVectorEntry(dataOffset, dataSize);
			} else if (NT_FILE == type) {
				readFileNotes(dataOffset);
			}

			return dataOffset + padToWordBoundary(dataSize);
		}

		private void readFileNotes(long offset) throws IOException {
			/*
			 * Format of NT_FILE note:
			 *
			 * long count    -- how many files are mapped
			 * long pageSize -- units for file_offset_in_pages
			 * array of [COUNT] elements of
			 *   long start
			 *   long end
			 *   long file_offset_in_pages
			 * followed by COUNT filenames in ASCII: "FILE1" NUL "FILE2" NUL...
			 */

			seek(offset);

			long count = readElfWord();
			readElfWord(); // skip page size
			long wordSizeBytes = addressSize() / 8;
			long fixedOffset = offset + (2 * wordSizeBytes);
			long stringsOffset = fixedOffset + (count * 3 * wordSizeBytes);
			Set<String> fileNames = new HashSet<>();

			_reader.seek(stringsOffset);

			for (int index = 0; index < count; ++index) {
				String fileName = readString();

				fileNames.add(fileName);
			}

			for (String fileName : fileNames) {
				File file = new File(fileName);

				try (ClosingFileReader fileReader = new ClosingFileReader(file)) {
					ElfFile result = elfFileFrom(fileReader, this);

					if (result == null) {
						// not an existing ELF file
						continue;
					}

					String soname = result.getSONAME();

					if (soname != null) {
						/*
						 * Often the file name matches the soname, but some system libraries are
						 * installed such that the soname is a symbolic link to the latest version
						 * of that shared library (in the same directory). The file note in this
						 * situation may refer to the file rather than the symbolic link. We need
						 * to use the symbolic link (if it exists and refers to the same file) so
						 * jdmpview can resolve the reference using the soname.
						 * An example may make this clearer. A recent Linux distribution includes
						 * the following for soname 'libc.so.6':
						 *     /lib/x86_64-linux-gnu/libc-2.31.so
						 *     /lib/x86_64-linux-gnu/libc.so.6
						 * with the latter being a symbolic link to the former.
						 */
						File sofile = new File(file.getParentFile(), soname);

						if (isSameFile(file, sofile)) {
							Set<String> paths = _librariesBySOName.computeIfAbsent(soname, key -> new HashSet<>());

							paths.add(sofile.getAbsolutePath());
						}
					}

					result.close();
				} catch (IOException e) {
					// ignore
				}
			}
		}

		private static boolean isSameFile(File file, File sofile) {
			try {
				return Files.isSameFile(file.toPath(), sofile.toPath());
			} catch (IOException e) {
				return false;
			}
		}

		protected void addProcessEntry(long offset, long size) {
			_processEntries.add(new DataEntry(offset, size));
		}

		protected void addThreadEntry(long offset, long size) {
			_threadEntries.add(new DataEntry(offset, size));
		}

		private void addAuxiliaryVectorEntry(long offset, long size) {
			_auxiliaryVectorEntries.add(new DataEntry(offset, size));
		}

		protected byte readByte() throws IOException {
			return _reader.readByte();
		}

		protected byte[] readBytes(int n) throws IOException {
			return _reader.readBytes(n);
		}

		private String readString() throws IOException {
			for (ByteArrayOutputStream buffer = new ByteArrayOutputStream();;) {
				byte ascii = readByte();

				if (ascii != 0) {
					buffer.write(ascii);
				} else {
					return new String(buffer.toByteArray(), StandardCharsets.US_ASCII);
				}
			}
		}

		private String readStringAtAddress(long address) throws IOException {
			seekToAddress(address);

			return readString();
		}

		protected int readInt() throws IOException {
			return _reader.readInt();
		}

		protected long readLong() throws IOException {
			return _reader.readLong();
		}

		protected short readShort() throws IOException {
			return _reader.readShort();
		}

		protected void seek(long position) throws IOException {
			_reader.seek(position + _offset);
		}

		long readUID() throws IOException {
			return _arch.readUID(this);
		}

		Address getStackPointerFrom(Map<String, Address> registers) {
			return _arch.getStackPointerFrom(registers);
		}

		Address getBasePointerFrom(Map<String, Address> registers) {
			return _arch.getBasePointerFrom(registers);
		}

		Address getInstructionPointerFrom(Map<String, Address> registers) {
			return _arch.getInstructionPointerFrom(registers);
		}

		Address getLinkRegisterFrom(Map<String, Address> registers) {
			return _arch.getLinkRegisterFrom(registers);
		}

		Map<String, Address> readRegisters(Builder builder, Object addressSpace) throws IOException {
			return _arch.readRegisters(this, builder, addressSpace);
		}

		abstract Iterator<Symbol> readSymbolsAt(SectionHeaderEntry entry) throws IOException;
		abstract int addressSize();

		/* file types corresponding to the _objectType field */
		private final static short ET_NONE = 0;             /* No file type */
		private final static short ET_REL = 1;              /* Relocatable file */
		private final static short ET_EXEC = 2;             /* Executable file */
		private final static short ET_DYN = 3;              /* Shared object file */
		private final static short ET_CORE = 4;             /* Core file */
		private final static short ET_NUM = 5;              /* Number of defined types */
		/* these are unsigned values so the constants can't be shorts */
		private final static int ET_LOOS = 0xfe00;          /* OS-specific range start */
		private final static int ET_HIOS = 0xfeff;          /* OS-specific range end */
		private final static int ET_LOPROC = 0xff00;        /* Processor-specific range start */
		private final static int ET_HIPROC = 0xffff;        /* Processor-specific range end */

		public Properties getProperties() {
			Properties props = new Properties();
			props.setProperty("Object file type", _nameForFileType(_objectType)); //$NON-NLS-1$
			props.setProperty("Object file version", Integer.toHexString(_version)); //$NON-NLS-1$
			props.setProperty("Processor-specific flags", Integer.toHexString(_e_flags)); //$NON-NLS-1$

			return props;
		}

		private static String _nameForFileType(short type) {
			String fileType = "Unknown"; //$NON-NLS-1$
			int typeAsInt = 0xFFFF & type;

			if (ET_NONE == type) {
				fileType = "No file type"; //$NON-NLS-1$
			} else if (ET_REL == type) {
				fileType = "Relocatable file"; //$NON-NLS-1$
			} else if (ET_EXEC == type) {
				fileType = "Executable file"; //$NON-NLS-1$
			} else if (ET_DYN == type) {
				fileType = "Shared object file"; //$NON-NLS-1$
			} else if (ET_CORE == type) {
				fileType = "Core file"; //$NON-NLS-1$
			} else if (ET_NUM == type) {
				fileType = "Number of defined types"; //$NON-NLS-1$
			} else if ((ET_LOOS <= typeAsInt) && (typeAsInt <= ET_HIOS)) {
				fileType = "OS-specific (" + Integer.toHexString(typeAsInt) + ")"; //$NON-NLS-1$ //$NON-NLS-2$
			} else if ((ET_LOPROC <= typeAsInt) && (typeAsInt <= ET_HIPROC)) {
				fileType = "Processor-specific (" + Integer.toHexString(typeAsInt) + ")"; //$NON-NLS-1$ //$NON-NLS-2$
			}

			return fileType;
		}

		private void seekToAddress(long address) throws IOException {
			ProgramHeaderEntry matchedEntry = null;
			for (Iterator<ProgramHeaderEntry> iter = programHeaderEntries(); iter.hasNext();) {
				ProgramHeaderEntry element = iter.next();
				if (element.contains(address)) {
					assert (null == matchedEntry) : "Multiple mappings for the same address"; //$NON-NLS-1$
					matchedEntry = element;
				}
			}
			if (null == matchedEntry) {
				throw new IOException("No ProgramHeaderEntry found for address"); //$NON-NLS-1$
			} else {
				seek(matchedEntry.fileOffset + (address - matchedEntry.virtualAddress));
			}
		}

		public String getSONAME() throws IOException {
			String soname = null;
			for (ProgramHeaderEntry entry : _programHeaderEntries) {
				if (!entry.isDynamic()) {
					continue;
				}
				soname = readSONAMEFromProgramHeader(entry);
				if (soname != null) {
					// System.err.println(String.format("Found module name %s, adding to _additionalFileNames", soname));
					break;
				}
			}
			return soname;
		}

		private String readSONAMEFromProgramHeader(ProgramHeaderEntry entry) throws IOException {
			long imageStart = entry.fileOffset;

			// Seek to the start of the dynamic section
			seek(imageStart);

			// Loop reading the dynamic section tag-value/pointer pairs until a 'debug'
			// entry is found
			long tag;

			long sonameAddress = -1;
			long strtabAddress = -1;
			do {
				// Read the tag and the value/pointer (only used after the loop has terminated)
				tag = readElfWord();
				long address = readElfWord();

				// Check that the tag is valid. As there may be some debate about the
				// set of valid values, a message will be issued but reading will continue
				/*
				 * CMVC 161449 - SVT:70:jextract invalid tag error
				 * The range of valid values in the header has been expanded, so increasing the valid range
				 * http://refspecs.freestandards.org/elf/gabi4+/ch5.dynamic.html
				 * DT_RUNPATH           29  d_val        optional     optional
				 * DT_FLAGS             30  d_val        optional     optional
				 * DT_ENCODING          32  unspecified  unspecified  unspecified
				 * DT_PREINIT_ARRAY     32  d_ptr        optional     ignored
				 * DT_PREINIT_ARRAYSZ   33  d_val        optional     ignored
				 */
				if (   !((tag >= 0         ) && (tag <= 33        ))
					&& !((tag >= 0x60000000) && (tag <= 0x6FFFFFFF))
					&& !((tag >= 0x70000000) && (tag <= 0x7FFFFFFF))) {
					break;
				}
				if (tag == DT_SONAME) {
					sonameAddress = address;
				} else if (tag == DT_STRTAB) {
					strtabAddress = address;
				}

				// System.err.println("Found dynamic section tag 0x" + Long.toHexString(tag));
			} while ((tag != DT_NULL));

			// If there isn't SONAME entry we can't get the shared object name.
			// The actual SONAME pointed to is in the string table so we need that too.
			if (sonameAddress > -1 && strtabAddress > -1) {
				String soname = readStringAtAddress(strtabAddress + sonameAddress);
				return soname;
			} else {
				return null;
			}
		}

	}

	/*
	 * ELF 32-bit header
	 *
	 * typedef struct
	 * {
	 *   unsigned char e_ident[EI_NIDENT]; // Magic number and other info
	 *   Elf32_Half    e_type;             // Object file type
	 *   Elf32_Half    e_machine;          // Architecture
	 *   Elf32_Word    e_version;          // Object file version
	 *   Elf32_Addr    e_entry;            // Entry point virtual address
	 *   Elf32_Off     e_phoff;            // Program header table file offset
	 *   Elf32_Off     e_shoff;            // Section header table file offset
	 *   Elf32_Word    e_flags;            // Processor-specific flags
	 *   Elf32_Half    e_ehsize;           // ELF header size in bytes
	 *   Elf32_Half    e_phentsize;        // Program header table entry size
	 *   Elf32_Half    e_phnum;            // Program header table entry count
	 *   Elf32_Half    e_shentsize;        // Section header table entry size
	 *   Elf32_Half    e_shnum;            // Section header table entry count
	 *   Elf32_Half    e_shstrndx;         // Section header string table index
	 * } Elf32_Ehdr;
	 */
	private static class Elf32File extends ElfFile {

		Elf32File(DumpReader reader) throws IOException {
			super(reader);
			readFile();
		}

		Elf32File(DumpReader reader, long offset) throws IOException {
			super(reader, offset);
			readFile();
		}

		@Override
		protected ProgramHeaderEntry readProgramHeaderEntry() throws IOException {
			int type = readInt();
			long fileOffset = unsigned(readInt());
			long virtualAddress = unsigned(readInt());
			long physicalAddress = unsigned(readInt());
			long fileSize = unsigned(readInt());
			long memorySize = unsigned(readInt());
			int flags = readInt();
			long alignment = unsigned(readInt());
			return new ProgramHeaderEntry(type, fileOffset, fileSize, virtualAddress, physicalAddress, memorySize, flags, alignment);
		}

		private static long unsigned(int i) {
			return i & 0xffffffffL;
		}

		@Override
		protected long padToWordBoundary(long l) {
			return (l + 3L) & ~3L;
		}

		@Override
		protected long readElfWord() throws IOException {
			return readInt() & 0xffffffffL;
		}

		@Override
		protected Address readElfWordAsAddress() throws IOException {
			return new Address32(readInt());
		}

		@Override
		int addressSize() {
			return 32;
		}

		@Override
		Iterator<Symbol> readSymbolsAt(SectionHeaderEntry entry) throws IOException {
			seek(entry.offset);
			List<Symbol> symbols = new ArrayList<>();
			long count = entry.size / 16;
			for (long i = 0; i < count; i++) {
				long name = readInt() & 0xffffffffL;
				long value = readInt() & 0xffffffffL;
				long size = readInt() & 0xffffffffL;
				byte info = readByte();
				byte other = readByte();
				int sectionIndex = readShort() & 0xffff;
				symbols.add(new Symbol(name, value, size, info, other, sectionIndex));
			}
			return symbols.iterator();
		}
	}

	/*
	 * ELF 64-bit header
	 *
	 * typedef struct
	 * {
	 *   unsigned char e_ident[EI_NIDENT]; // Magic number and other info
	 *   Elf64_Half    e_type;             // Object file type
	 *   Elf64_Half    e_machine;          // Architecture
	 *   Elf64_Word    e_version;          // Object file version
	 *   Elf64_Addr    e_entry;            // Entry point virtual address
	 *   Elf64_Off     e_phoff;            // Program header table file offset
	 *   Elf64_Off     e_shoff;            // Section header table file offset
	 *   Elf64_Word    e_flags;            // Processor-specific flags
	 *   Elf64_Half    e_ehsize;           // ELF header size in bytes
	 *   Elf64_Half    e_phentsize;        // Program header table entry size
	 *   Elf64_Half    e_phnum;            // Program header table entry count
	 *   Elf64_Half    e_shentsize;        // Section header table entry size
	 *   Elf64_Half    e_shnum;            // Section header table entry count
	 *   Elf64_Half    e_shstrndx;         // Section header string table index
	 * } Elf64_Ehdr;
	 */
	private static class Elf64File extends ElfFile {
		Elf64File(DumpReader reader) throws IOException {
			super(reader);
			readFile();
		}

		Elf64File(DumpReader reader, long offset) throws IOException {
			super(reader, offset);
			readFile();
		}

		@Override
		protected ProgramHeaderEntry readProgramHeaderEntry() throws IOException {
			int type = readInt();
			int flags = readInt();
			long fileOffset = readLong();
			long virtualAddress = readLong();
			long physicalAddress = readLong();
			long fileSize = readLong();
			long memorySize = readLong();
			long alignment = readLong();
			return new ProgramHeaderEntry(type, fileOffset, fileSize, virtualAddress, physicalAddress, memorySize, flags, alignment);
		}

		@Override
		protected long padToWordBoundary(long l) {
			return (l + 7L) & ~7L;
		}

		@Override
		protected long readElfWord() throws IOException {
			return readLong();
		}

		@Override
		protected Address readElfWordAsAddress() throws IOException {
			return new Address64(readLong());
		}

		@Override
		int addressSize() {
			return 64;
		}

		@Override
		Iterator<Symbol> readSymbolsAt(SectionHeaderEntry entry) throws IOException {
			seek(entry.offset);
			List<Symbol> symbols = new ArrayList<>();
			long count = entry.size / 24;
			for (long i = 0; i < count; i++) {
				long name = readInt() & 0xffffffffL;
				byte info = readByte();
				byte other = readByte();
				int sectionIndex = readShort() & 0xffff;
				long value = readLong();
				long size = readLong();
				symbols.add(new Symbol(name, value, size, info, other, sectionIndex));
			}
			return symbols.iterator();
		}
	}

	private List<MemoryRange> _memoryRanges = new ArrayList<>();
	private Set<String> _additionalFileNames = new HashSet<>();
	private long _platformIdAddress = 0;
	private ElfFile _file;
	private boolean _isLittleEndian;
	private boolean _is64Bit;
	private boolean _verbose;

	private Object readProcess(DataEntry entry, Builder builder, Object addressSpace, List<Object> threads) throws IOException, CorruptCoreException, MemoryAccessException {
		_file.seek(entry.offset);
		_file.readByte(); // Ignore state
		_file.readByte(); // Ignore sname
		_file.readByte(); // Ignore zombie
		_file.readByte(); // Ignore nice
		_file.seek(entry.offset + _file.padToWordBoundary(4));
		_file.readElfWord(); // Ignore flags
		readUID(); // Ignore uid
		readUID(); // Ignore gid
		long elfPID = _file.readInt() & 0xffffffffL;

		_file.readInt(); // Ignore ppid
		_file.readInt(); // Ignore pgrp
		_file.readInt(); // Ignore sid

		// Ignore filler.  Command-line is last 80 bytes (defined by ELF_PRARGSZ in elfcore.h).
		// Command-name is 16 bytes before command-line.
		_file.seek(entry.offset + entry.size - 96);
		_file.readBytes(16); // Ignore command
		String dumpCommandLine = new String(_file.readBytes(ELF_PRARGSZ), StandardCharsets.US_ASCII).trim();

		Properties environment = getEnvironmentVariables(builder);
		String alternateCommandLine = ""; //$NON-NLS-1$
		if (null != environment) {
			alternateCommandLine = environment.getProperty("IBM_JAVA_COMMAND_LINE", ""); //$NON-NLS-1$ //$NON-NLS-2$
		}

		String commandLine = dumpCommandLine.length() >= alternateCommandLine.length() ? dumpCommandLine : alternateCommandLine;

		int space = commandLine.indexOf(' ');
		String executable = commandLine;
		if (0 < space) {
			executable = executable.substring(0, space);
		}

		// On Linux, the VM forks before dumping, and it's the forked process which generates the dump.
		// Therefore, the PID reported in the ELF file is different from the original Java process PID.
		// To work around this problem, the PID of the original process and the TID of the original failing
		// thread have been added to the J9RAS structure. However, the ImageThread corresponding to the
		// original failing thread has to be faked up, since it isn't one of the threads read from the dump
		// (which are relative to the forked process).

		String pid = Long.toString(elfPID);
		Object failingThread = threads.get(0);

		// Get the PID from the J9RAS structure, if possible
		// otherwise use the one of the forked process
		if (null != _j9rasReader) {
			boolean didFork = true;
			try {
				long rasPID = _j9rasReader.getProcessID();
				if (elfPID != rasPID) {
					// PIDs are different, therefore fork+dump has happened.
					// In this case, rasPID is the correct PID to report
					pid = Long.toString(rasPID);
				} else {
					// PIDs are equal, so the dump must have been generated externally.
					// In this case, the failing thread is the one reported by ELF,
					// not the one read from J9RAS.
					didFork = false;
				}
			} catch (UnsupportedOperationException use) {
				// ignore
			}

			if (didFork) {
				try {
					long tid = _j9rasReader.getThreadID();
					// fake a thread with no other useful info than the TID.
					failingThread = builder.buildThread(Long.toString(tid), Collections.EMPTY_LIST.iterator(), Collections.EMPTY_LIST.iterator(), Collections.EMPTY_LIST.iterator(), new Properties(), 0);
					threads.add(0, failingThread);
				} catch (UnsupportedOperationException uoe) {
					// ignore
				}
			}
		}

		return buildProcess(builder, addressSpace, pid, commandLine,
					getEnvironmentVariables(builder), failingThread,
					threads.iterator(), executable);
	}

	private Object buildProcess(Builder builder, Object addressSpace, String pid, String commandLine,
			Properties environmentVariables, Object currentThread, Iterator<Object> threads, String executableName)
			throws IOException, MemoryAccessException {
		List<?> modules = readModules(builder, addressSpace, executableName);
		Iterator<?> libraries = modules.iterator();
		Object executable = null;
		if (libraries.hasNext()) {
			executable = libraries.next();
		}
		return builder.buildProcess(addressSpace, pid, commandLine, environmentVariables, currentThread, threads, executable, libraries, _file.addressSize());
	}

	/**
	 * Returns a list of the ImageModules in the address space.  The executable will be element 0
	 * @param builder
	 * @param addressSpace
	 * @param executableName
	 * @return
	 * @throws IOException
	 * @throws MemoryAccessException
	 */
	private List<?> readModules(Builder builder, Object addressSpace, String executableName) throws IOException, MemoryAccessException {
		// System.err.println("NewElfDump.readModules() entered en=" + executableName);
		List<Object> modules = new ArrayList<>();

		String overrideExecutableName = System.getProperty(SYSTEM_PROP_EXECUTABLE);
		String classPath = System.getProperty("java.class.path", "."); //$NON-NLS-1$ //$NON-NLS-2$
		ClosingFileReader file;
		if (overrideExecutableName == null) {
			file = _findFileInPath(builder, executableName, classPath);
		} else {
			// Use override for the executable name. This supports the jextract -f <executable> option, for
			// cases where the launcher path+name is truncated by the 80 character OS limit, AND it was a
			// custom launcher, so the alternative IBM_JAVA_COMMAND_LINE property was not set.
			file = _findFileInPath(builder, overrideExecutableName, classPath);
		}

		if (null != file) {
			// System.err.println("NewElfDump.readModules() opened file");
			ElfFile executable = elfFileFrom(file);

			if (null != executable) {
				// System.err.println("NewElfDump.readModules() found executable object");
				ProgramHeaderEntry dynamic = null;

				for (Iterator<ProgramHeaderEntry> iter = executable.programHeaderEntries(); iter.hasNext();) {
					ProgramHeaderEntry entry = iter.next();

					if (entry.isDynamic()) {
						dynamic = entry;
						break;
					}
				}

				if (null != dynamic) {
					// System.err.println("NewElfDump.readModules() found 'dynamic' program header object");
					long imageStart = dynamic.virtualAddress;
					List<?> symbols = readSymbolsFrom(builder, addressSpace, executable, imageStart);

					if (isValidAddress(imageStart)) {
						Iterator<?> sections = _buildModuleSections(builder, addressSpace, executable, dynamic.virtualAddress);
						Properties properties = executable.getProperties();

						modules.add(builder.buildModule(executableName, properties, sections, symbols.iterator(), imageStart));
						modules.addAll(readLibrariesAt(builder, addressSpace, imageStart));
						_additionalFileNames.add(executableName);
					}
				}
			} else {
				// call in here if we can't find the executable
				builder.setExecutableUnavailable("Executable file \"" + executableName + "\" not found"); //$NON-NLS-1$ //$NON-NLS-2$
			}
			file.close();
		} else {
			// call in here if we can't find the executable
			builder.setExecutableUnavailable("File \"" + executableName + "\" not found"); //$NON-NLS-1$ //$NON-NLS-2$
			if (_verbose) {
				System.err.println("Warning: executable " + executableName //$NON-NLS-1$
						+ " not found, unable to collect libraries." //$NON-NLS-1$
						+ " Please retry with jextract -f option."); //$NON-NLS-1$
			}
		}

		// Get all the loaded modules .so names to make sure we have the
		// full list of libraries for jextract to zip up. CMVC 194366
		for (Iterator<ProgramHeaderEntry> iter = _file.programHeaderEntries(); iter.hasNext();) {
			ProgramHeaderEntry entry = iter.next();

			if (!entry.isLoadable()) {
				continue;
			}

			try {
				ElfFile moduleFile;

				if (_is64Bit) {
					moduleFile = new Elf64File(_file._reader, entry.fileOffset);
				} else {
					moduleFile = new Elf32File(_file._reader, entry.fileOffset);
				}

				// Now search the dynamic entries for this so file to get it's SONAME.
				// "lib.so" is returned for the executable and should be ignored.
				String soname = moduleFile.getSONAME();

				if (soname == null || "lib.so".equals(soname)) { //$NON-NLS-1$
					continue;
				}

				// add all matching names found in the file notes
				Set<String> libs = _file._librariesBySOName.get(soname);

				if (libs == null || libs.isEmpty()) {
					// use soname if we could't find something better in the file notes
					_additionalFileNames.add(soname);
				} else {
					_additionalFileNames.addAll(libs);
				}
			} catch (Exception ex) {
				// We can't tell a loaded module from a loaded something else without trying to open it
				// as an ElfFile so this will happen. We are looking for extra library names, it's not
				// critical if this fails.
			}
		}

		return modules;
	}

	/**
	 * Attempts to track down the file with given filename in :-delimited path
	 *
	 * @param builder
	 * @param executableName
	 * @param property
	 * @return The file if it is found or null if the search failed
	 */
	private static ClosingFileReader _findFileInPath(Builder builder, String filename, String path) {
		try {
			// first try it as whatever we were given first
			return builder.openFile(filename);
		} catch (IOException e) {
			// it wasn't there so fall back to the path
			for (String component : path.split(File.pathSeparator)) {
				try {
					// first try it as whatever we were given
					return builder.openFile(component + File.separator + filename);
				} catch (IOException e2) {
					// do nothing, most of these will be file not found
				}
			}
		}

		return null;
	}

	private static final long SHT_STRTAB = 3;

	private Iterator<?> _buildModuleSections(Builder builder, Object addressSpace, ElfFile executable, long loadedBaseAddress) throws IOException {
		List<Object> sections = new ArrayList<>();
		Iterator<SectionHeaderEntry> shentries = executable.sectionHeaderEntries();
		byte[] strings = null;

		// Find the string table by looking for a section header entry of type string
		// table which, when resolved against itself, has the name ".shstrtab".
		// NB : If the ELF file is invalid, the method stringFromBytesAt() may return null.
		while (shentries.hasNext()) {
			SectionHeaderEntry entry = shentries.next();
			if (SHT_STRTAB == entry._type) {
				executable.seek(entry.offset);
				byte[] attempt = executable.readBytes((int) entry.size);
				String peek = stringFromBytesAt(attempt, entry._name);
				if (peek != null) {
					if (peek.equals(".shstrtab")) { //$NON-NLS-1$
						strings = attempt;
						break; // found it
					}
				} else {
					if (_verbose) {
						System.err.println("\tError reading section header name at file offset=0x" + Long.toHexString(entry.offset)); //$NON-NLS-1$
					}
				}
			}
		}

		// Loop through the section header entries building a module section for each one
		// NB : If the ELF file is invalid, the method stringFromBytesAt() may return null.
		shentries = executable.sectionHeaderEntries();
		while (shentries.hasNext()) {
			SectionHeaderEntry entry = shentries.next();
			String name = ""; //$NON-NLS-1$

			if (null != strings) {
				name = stringFromBytesAt(strings, entry._name);
				if (name == null) {
					if (_verbose) {
						System.err.println("\tError reading section header name at file offset=0x" + Long.toHexString(entry.offset)); //$NON-NLS-1$
					}
					name = ""; //$NON-NLS-1$
				}
			}

			Object section = builder.buildModuleSection(addressSpace, name, loadedBaseAddress + entry.offset, loadedBaseAddress + entry.offset + entry.size);
			sections.add(section);
		}
		return sections.iterator();
	}

	/**
	 * Is this virtual address valid and present in the core file.
	 * @param address Virtual address to test
	 * @return true if valid and present
	 */
	private boolean isValidAddress(long address) {
		for (Iterator<ProgramHeaderEntry> iter = _file.programHeaderEntries(); iter.hasNext();) {
			ProgramHeaderEntry element = iter.next();
			if (element.contains(address)) {
				return true;
			}
		}
		return false;
	}

	/**
	 * Is this virtual address valid in the process, though perhaps not present in the core file.
	 * @param address Virtual address to test
	 * @return if valid
	 */
	private boolean isValidAddressInProcess(long address) {
		for (Iterator<ProgramHeaderEntry> iter = _file.programHeaderEntries(); iter.hasNext();) {
			ProgramHeaderEntry element = iter.next();
			if (element.validInProcess(address)) {
				return true;
			}
		}
		return false;
	}

	private List<?> readLibrariesAt(Builder builder, Object addressSpace, long imageStart) throws IOException {
		// System.err.println("NewElfDump.readLibrariesAt() entered is=0x" + Long.toHexString(imageStart));

		// Create the method return value
		List<Object> libraries = new ArrayList<>();

		// Seek to the start of the dynamic section
		seekToAddress(imageStart);

		// Loop reading the dynamic section tag-value/pointer pairs until a 'debug'
		// entry is found
		long tag;
		long address;

		do {
			// Read the tag and the value/pointer (only used after the loop has terminated)
			tag = _file.readElfWord();
			address = _file.readElfWord();

			// Check that the tag is valid. As there may be some debate about the
			// set of valid values, a message will be issued but reading will continue
			/*
			 * CMVC 161449 - SVT:70:jextract invalid tag error
			 * The range of valid values in the header has been expanded, so increasing the valid range
			 * http://refspecs.freestandards.org/elf/gabi4+/ch5.dynamic.html
			 * DT_RUNPATH           29  d_val        optional     optional
			 * DT_FLAGS             30  d_val        optional     optional
			 * DT_ENCODING          31  unspecified  unspecified  unspecified
			 * DT_PREINIT_ARRAY     32  d_ptr        optional     ignored
			 * DT_PREINIT_ARRAYSZ   33  d_val        optional     ignored
			 */
			if (_verbose) {
				if (   !((tag >= 0         ) && (tag <= 33        ))
					&& !((tag >= 0x60000000) && (tag <= 0x6FFFFFFF))
					&& !((tag >= 0x70000000) && (tag <= 0x7FFFFFFF))) {
					System.err.println("Error reading dynamic section. Invalid tag value 0x" + Long.toHexString(tag)); //$NON-NLS-1$
				}
			}
			// System.err.println("Found dynamic section tag 0x" + Long.toHexString(tag));
		} while ((tag != DT_NULL) && (tag != DT_DEBUG));

		// If there is no debug section, there is nothing to do
		if (tag != DT_DEBUG) {
			return libraries;
		}

		// Seek to the start of the debug data
		seekToAddress(address);

		// NOTE the rendezvous structure is described in /usr/include/link.h
		// struct r_debug {
		//     int r_version;
		//     struct link_map *r_map;
		//     ElfW(Addr) r_brk;       /* Really a function pointer */
		//     enum { ... } r_state;
		//     ElfW(Addr) r_ldbase;
		// };
		// struct link_map {
		//     ElfW(Addr) l_addr;      /* Base address shared object is loaded at. */
		//     char *l_name;           /* Absolute file name object was found in. */
		//     ElfW(Dyn) *l_ld;        /* Dynamic section of the shared object. */
		//     struct link_map *l_next, *l_prev; /* Chain of loaded objects. */
		// };

		_file.readElfWord(); // Ignore version (and alignment padding)
		long next = _file.readElfWord();
		while (0 != next) {
			seekToAddress(next);
			long loadedBaseAddress = _file.readElfWord();
			long nameAddress = _file.readElfWord();
			_file.readElfWord(); // Ignore dynamicSectionAddress
			next = _file.readElfWord();
			if (0 != loadedBaseAddress) {
				MemoryRange range = memoryRangeFor(loadedBaseAddress);

				// NOTE There is an apparent bug in the link editor on Linux x86-64.
				// The loadedBaseAddress for libnuma.so is invalid, although it is
				// correctly loaded and its symbols are resolved.  The library is loaded
				// around 0x2a95c4f000 but its base is recorded in the link map as
				// 0xfffffff09f429000.

				Properties properties = new Properties();
				List<Object> symbols = new ArrayList<>();
				String name = readStringAt(nameAddress);
				Iterator<?> sections = Collections.emptyIterator();

				// If we have a valid link_map entry, open up the actual library file to get the symbols and
				// library sections.
				// Note: on SLES 10 we have seen a link_map entry with a null name, in non-corrupt dumps, so we now
				// ignore link_map entries with null names rather than report them as corrupt. See defect 132140
				// Note : on SLES 11 we have seen a link_map entry with a module name of "7". Using the base address and correlating
				// with the libraries we find by iterating through the program header table of the core file, we know that
				// this is in fact linux-vdso64.so.1
				// Detect this by spotting that the name does not begin with "/" - the names should
				// always be full pathnames. In this case just ignore this one.
				// See defect CMVC 184115
				if ((null != range) && (range.getVirtualAddress() == loadedBaseAddress) && (null != name) && (name.length() > 0) && (name.startsWith("/"))) { //$NON-NLS-1$
					try {
						ClosingFileReader file = builder.openFile(name);
						if (null != file) {
							if (_verbose) {
								System.err.println("Reading library file " + name); //$NON-NLS-1$
							}
							ElfFile library = elfFileFrom(file);
							if (range.isInCoreFile() == false) {
								// the memory range has not been loaded into the core dump, but it should be
								// resident in the library file on disk, so add a reference to the real file.
								range.setLibraryReader(library._reader);
							}
							_additionalFileNames.add(name);
							if (null != library) {
								symbols = readSymbolsFrom(builder, addressSpace, library, loadedBaseAddress);
								properties = library.getProperties();
								sections = _buildModuleSections(builder, addressSpace, library, loadedBaseAddress);
							} else {
								symbols.add(builder.buildCorruptData(addressSpace, "unable to find module " + name, loadedBaseAddress)); //$NON-NLS-1$
							}
							file.close();
						}
					} catch (FileNotFoundException exc) {
						// As for null file return above, if we can't find the library file we follow the normal
						// DTFJ convention and leave the properties, sections and symbols iterators empty.
					}
					libraries.add(builder.buildModule(name, properties, sections, symbols.iterator(), loadedBaseAddress));
				}
			}
		}

		return libraries;
	}

	private MemoryRange memoryRangeFor(long address) {
		// TODO: change the callers of this method to use some mechanism which will work better within the realm of the new address spaces
		Iterator<MemoryRange> ranges = _memoryRanges.iterator();

		while (ranges.hasNext()) {
			MemoryRange range = ranges.next();

			if (range.contains(address)) {
				return range;
			}
		}

		return null;
	}

	private void seekToAddress(long address) throws IOException {
		ProgramHeaderEntry matchedEntry = null;
		for (Iterator<ProgramHeaderEntry> iter = _file.programHeaderEntries(); iter.hasNext();) {
			ProgramHeaderEntry element = iter.next();
			if (element.contains(address)) {
				assert (null == matchedEntry) : "Multiple mappings for the same address"; //$NON-NLS-1$
				matchedEntry = element;
			}
		}
		if (null == matchedEntry) {
			throw new IOException("No ProgramHeaderEntry found for address"); //$NON-NLS-1$
		} else {
			coreSeek(matchedEntry.fileOffset + (address - matchedEntry.virtualAddress));
		}
	}

	private List<Object> readSymbolsFrom(Builder builder, Object addressSpace, ElfFile file, long baseAddress) throws IOException {
		List<Object > symbols = new ArrayList<>();
		for (Iterator<SectionHeaderEntry> iter = file.sectionHeaderEntries(); iter.hasNext();) {
			SectionHeaderEntry entry = iter.next();
			if (entry.isSymbolTable()) {
				symbols.addAll(readSymbolsFrom(builder, addressSpace, file, entry, baseAddress));
			}
		}
		return symbols;
	}

	private List<?> readSymbolsFrom(Builder builder, Object addressSpace, ElfFile file, SectionHeaderEntry entry, long baseAddress) throws IOException {
		List<Object> symbols = new ArrayList<>();
		SectionHeaderEntry stringTable = file.sectionHeaderEntryAt((int) entry.link);
		file.seek(stringTable.offset);
		byte[] strings = file.readBytes((int) stringTable.size);
		for (Iterator<Symbol> iter = file.readSymbolsAt(entry); iter.hasNext();) {
			Symbol sym = iter.next();
			if (sym.isFunction()) {
				String name = stringFromBytesAt(strings, sym.name);
				if (name != null) {
					if (sym.value != 0) {
						symbols.add(builder.buildSymbol(addressSpace, name, baseAddress + sym.value));
					}
				} else {
					if (_verbose) {
						System.err.println("\tError reading section header name at file offset=0x" + Long.toHexString(stringTable.offset)); //$NON-NLS-1$
					}
				}
			}
		}
		return symbols;
	}

	/**
	 * Method for extracting a string from a string table
	 *
	 * @param strings : The byte array from which the strings are to be extracted
	 * @param offset  : The offset of the required string
	 * @return
	 */
	private String stringFromBytesAt(byte[] stringTableBytes, long offset) {
		// Check that the offset is valid
		if ((offset < 0) || (offset >= stringTableBytes.length)) {
			if (_verbose) {
				System.err.println("\tError in string table offset, value=0x" + Long.toHexString(offset)); //$NON-NLS-1$
			}
			return null;
		}

		// As the offset is now known to be small (definitely fits in 32 bits), it
		// can be safely cast to an int
		int startOffset = (int) offset;
		int endOffset = startOffset;

		// Scan along the characters checking they are valid (else undefined behaviour
		// converting to a string) and locating the terminating null (if any!!)
		for (endOffset = startOffset; endOffset < stringTableBytes.length; endOffset++) {
			if (stringTableBytes[endOffset] >= 128) {
				if (_verbose) {
					System.err.println("\tError in string table. Non ASCII character encountered."); //$NON-NLS-1$
				}
				return null;
			} else if (stringTableBytes[endOffset] == 0) {
				break;
			}
		}

		// Check that the string is null terminated
		if (stringTableBytes[endOffset] != 0) {
			if (_verbose) {
				System.err.println("\tError in string table. The string is not terminated."); //$NON-NLS-1$
			}
			return null;
		}

		// Convert the bytes to a string
		try {
			String result = new String(stringTableBytes, startOffset, endOffset - startOffset, StandardCharsets.US_ASCII);
			// System.err.println("Read string '" + result + "' from string table");
			return result;
		} catch (IndexOutOfBoundsException exception) {
			System.err.println("Error (IndexOutOfBoundsException) converting string table characters. The core file is invalid and the results may unpredictable"); //$NON-NLS-1$
		}

		return null;
	}

	private Properties getEnvironmentVariables(Builder builder) throws IOException {
		// builder.getEnvironmentAddress() points to a pointer to a null
		// terminated list of addresses which point to strings of form x=y

		// Get environment variable addresses
		long environmentAddress = builder.getEnvironmentAddress();
		if (0 == environmentAddress) {
			return null;
		}

		List<Address> addresses = new ArrayList<>();
		seekToAddress(environmentAddress);
		Address address = _file.readElfWordAsAddress();
		try {
			seekToAddress(address.asAddress());
			address = _file.readElfWordAsAddress();
			while (false == address.isNil()) {
				addresses.add(address);
				address = _file.readElfWordAsAddress();
			}
		} catch (IOException e) {
			// CMVC 161796
			// catch the IO exception, give up trying to extract the env vars and attempt to carry on parsing the core file

			// CMVC 164739
			// Cannot propagate detected condition upwards without changing external (and internal) APIs.
		}

		// Get environment variables
		Properties environment = new Properties();
		for (Iterator<Address> iter = addresses.iterator(); iter.hasNext();) {
			Address variable = iter.next();
			try {
				seekToAddress(variable.asAddress());
			} catch (IOException e) {
				// catch here rather than propagate up to method extract()
				// we can carry on getting other environment variables
				continue;
			}

			String pair;

			for (ByteArrayOutputStream buffer = new ByteArrayOutputStream();;) {
				byte ascii = coreReadByte();

				if (ascii != 0) {
					buffer.write(ascii);
				} else {
					pair = new String(buffer.toByteArray(), StandardCharsets.US_ASCII);
					break;
				}
			}

			// Now pair is the x=y string

			int equal = pair.indexOf('=');
			if (equal != -1) {
				String key = pair.substring(0, equal);
				String value = pair.substring(equal + 1);
				environment.put(key, value);
			}
		}

		return environment;
	}

	private long readUID() throws IOException {
		return _file.readUID();
	}

	/*
	 * NOTE: We only ever see one native thread due to the way we create core dumps.
	 */
	private Object readThread(DataEntry entry, Builder builder, Object addressSpace) throws IOException {
		_file.seek(entry.offset);
		int signalNumber = _file.readInt(); // signalNumber
		_file.readInt(); // Ignore code
		_file.readInt(); // Ignore errno
		_file.readShort(); // Ignore cursig
		_file.readShort(); // Ignore dummy
		_file.readElfWord(); // Ignore pending
		_file.readElfWord(); // Ignore blocked
		long pid = _file.readInt() & 0xffffffffL;
		_file.readInt(); // Ignore ppid
		_file.readInt(); // Ignore pgroup
		_file.readInt(); // Ignore session

		long utimeSec = _file.readElfWord(); // utime_sec
		long utimeUSec = _file.readElfWord(); // utime_usec
		long stimeSec = _file.readElfWord(); // stime_sec
		long stimeUSec = _file.readElfWord(); // stime_usec
		_file.readElfWord(); // Ignore cutime_sec
		_file.readElfWord(); // Ignore cutime_usec
		_file.readElfWord(); // Ignore cstime_sec
		_file.readElfWord(); // Ignore cstime_usec

		Map<String, Address> registers = _file.readRegisters(builder, addressSpace);
		Properties properties = new Properties();
		properties.setProperty("Thread user time secs", Long.toString(utimeSec)); //$NON-NLS-1$
		properties.setProperty("Thread user time usecs", Long.toString(utimeUSec)); //$NON-NLS-1$
		properties.setProperty("Thread sys time secs", Long.toString(stimeSec)); //$NON-NLS-1$
		properties.setProperty("Thread sys time usecs", Long.toString(stimeUSec)); //$NON-NLS-1$

		return buildThread(builder, addressSpace, String.valueOf(pid), registers, properties, signalNumber);
	}

	private Object buildThread(Builder builder, Object addressSpace, String pid, Map<String, Address> registers, Properties properties, int signalNumber) {
		List<Object> frames = new ArrayList<>();
		List<Object> sections = new ArrayList<>();
		long stackPointer = _file.getStackPointerFrom(registers).asAddress();
		long basePointer = _file.getBasePointerFrom(registers).asAddress();
		long instructionPointer = _file.getInstructionPointerFrom(registers).asAddress();
		long previousBasePointer = 0;

		if (0 == instructionPointer || false == isValidAddressInProcess(instructionPointer)) {
			instructionPointer = _file.getLinkRegisterFrom(registers).asAddress();
		}

		if (0 != instructionPointer && 0 != basePointer && isValidAddressInProcess(instructionPointer) && isValidAddressInProcess(stackPointer)) {
			MemoryRange range = memoryRangeFor(stackPointer);
			sections.add(builder.buildStackSection(addressSpace, range.getVirtualAddress(), range.getVirtualAddress() + range.getSize()));
			frames.add(builder.buildStackFrame(addressSpace, basePointer, instructionPointer));
			// Loop through stack frames, starting from current frame base pointer
			try {
				while (range.contains(basePointer) && (basePointer != previousBasePointer)) {
					previousBasePointer = basePointer;
					seekToAddress(basePointer);
					basePointer = coreReadAddress();
					instructionPointer = coreReadAddress();
					frames.add(builder.buildStackFrame(addressSpace, basePointer, instructionPointer));
				}
			} catch (IOException e) {
				// CMVC 161796
				// catch the IO exception, give up trying to extract the frames and attempt to carry on parsing the core file

				// CMVC 164739
				// Keep going instead.
				frames.add(builder.buildCorruptData(addressSpace, "Linux ELF core dump corruption " //$NON-NLS-1$
						+ "detected during native stack frame walk", basePointer)); //$NON-NLS-1$
			}
		}
		return builder.buildThread(pid, registersAsList(builder, registers).iterator(),
				sections.iterator(), frames.iterator(), properties, signalNumber);
	}

	private static List<?> registersAsList(Builder builder, Map<String, Address> registers) {
		List<Object> list = new ArrayList<>();
		for (Map.Entry<String, Address> entry : registers.entrySet()) {
			String name = entry.getKey();
			Address value = entry.getValue();
			list.add(builder.buildRegister(name, value.asNumber()));
		}
		return list;
	}

	private NewElfDump(ElfFile file, DumpReader reader, boolean isLittleEndian, boolean is64Bit, boolean verbose) {
		super(reader);
		_file = file;
		_isLittleEndian = isLittleEndian;
		_is64Bit = is64Bit;
		_verbose = verbose;

		// FIXME will we ever see multiple PHEs mapped to the same address?
		// FIXME if so, should the first or the last entry seen take precedence?

		/* Printing the memory segments is debug-only, not useful to customers, and swamps the -verbose output
		if (_verbose) {
			System.err.println("Memory segments...");
			System.err.println("  Start               End                 File Size");
		}
		*/
		for (Iterator<ProgramHeaderEntry> iter = _file.programHeaderEntries(); iter.hasNext();) {
			ProgramHeaderEntry entry = iter.next();
			/* Printing the memory segments is debug-only, not useful to customers, and swamps the -verbose output
			if (_verbose) {
				// print out information about the memory segments :
				// virtual address start, virtual address end
				// and the filesize (0 if not in core).
				int colWidth = 20;
				String padding = " ";
				StringBuffer sb = new StringBuffer();

				String vaStart = "0x" + Long.toHexString(entry.virtualAddress);
				String vaEnd = "0x" + Long.toHexString(entry.virtualAddress + entry.memorySize);
				String fileSize = "0x" + Long.toHexString(entry.fileSize);

				sb.append("  " + vaStart);
				for (int i = 0; i < colWidth - vaStart.length(); i++) {
					sb.append(padding);
				}

				sb.append(vaEnd);
				for (int i = 0; i < colWidth - vaEnd.length(); i++) {
					sb.append(padding);
				}

				sb.append(fileSize);
				for (int i = 0; i < colWidth - fileSize.length(); i++) {
					sb.append(padding);
				}
				System.err.println(sb);
			}
			*/
			MemoryRange range = entry.asMemoryRange();
			if (null != range) {
				// See comments to CMVC 137737
				_memoryRanges.add(range);
			}
		}
		/* Printing the memory segments is debug-only, not useful to customers, and swamps the -verbose output
		if (_verbose) {
			System.err.println();
		}
		*/
	}

	private ElfFile elfFileFrom(ClosingFileReader file) throws IOException {
		return elfFileFrom(file, _file);
	}

	static ElfFile elfFileFrom(ClosingFileReader file, ElfFile container) throws IOException {
		byte[] signature = new byte[EI_NIDENT];

		file.seek(0);
		file.readFully(signature);

		if (ElfFile.isELF(signature)) {
			DumpReader reader = readerForEndianess(signature[5], signature[4], file);
			ElfFile result = fileForClass(signature[4], reader);

			if (result.getClass().isInstance(container)) {
				return result;
			}
		}

		return null;
	}

	private static DumpReader readerForEndianess(byte endianess, byte clazz, ImageInputStream stream)
			throws IOException {
		boolean is64Bit;

		switch (clazz) {
		case ELFCLASS32:
			is64Bit = false;
			break;
		case ELFCLASS64:
			is64Bit = true;
			break;
		default:
			throw new IOException("Unexpected class flag " + clazz + " detected in ELF file."); //$NON-NLS-1$ //$NON-NLS-2$
		}

		switch (endianess) {
		case ELFDATA2LSB:
			return new LittleEndianDumpReader(stream, is64Bit);
		case ELFDATA2MSB:
			return new DumpReader(stream, is64Bit);
		default:
			throw new IOException("Unknown endianess flag " + endianess + " in ELF core file"); //$NON-NLS-1$ //$NON-NLS-2$
		}
	}

	private static ElfFile fileForClass(byte clazz, DumpReader reader) throws IOException {
		if (ELFCLASS32 == clazz) {
			return new Elf32File(reader);
		} else if (ELFCLASS64 == clazz) {
			return new Elf64File(reader);
		} else {
			throw new IOException("Unexpected class flag " + clazz + " detected in ELF file."); //$NON-NLS-1$ //$NON-NLS-2$
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.jvm.j9.dump.systemdump.Dump#extract(com.ibm.jvm.j9.dump.systemdump.Builder)
	 */
	@Override
	public void extract(Builder builder) {
		// System.err.println("NewElfDump.extract() entered");
		try {
			Object addressSpace = builder.buildAddressSpace("ELF Address Space", 0); //$NON-NLS-1$
			List<Object> threads = new ArrayList<>();
			for (Iterator<DataEntry> iter = _file.threadEntries(); iter.hasNext();) {
				DataEntry entry = iter.next();
				threads.add(readThread(entry, builder, addressSpace));
			}
			List<Object> processes = new ArrayList<>();
			for (Iterator<DataEntry> iter = _file.processEntries(); iter.hasNext();) {
				DataEntry entry = iter.next();
				processes.add(readProcess(entry, builder, addressSpace, threads));
			}
			for (Iterator<DataEntry> iter = _file.auxiliaryVectorEntries(); iter.hasNext();) {
				DataEntry entry = iter.next();
				readAuxiliaryVector(entry);
			}
			builder.setOSType("ELF"); //$NON-NLS-1$
			builder.setCPUType(_file._arch.toString());
			builder.setCPUSubType(readStringAt(_platformIdAddress));
		} catch (CorruptCoreException | IOException | MemoryAccessException e) {
			// TODO throw exception or notify builder?
		}

		// System.err.println("NewElfDump.extract() exited");
	}

	private void readAuxiliaryVector(DataEntry entry) throws IOException {
		_file.seek(entry.offset);
		if (0 != entry.size) {
			for (;;) {
				long type = _file.readElfWord();

				if (AT_NULL == type) {
					break;
				}

				long value = _file.readElfWord();

				if (AT_PLATFORM == type) {
					_platformIdAddress = value;
				}
			}
		}
	}

	private String readStringAt(long address) throws IOException {
		if (isValidAddress(address)) {
			seekToAddress(address);

			for (ByteArrayOutputStream buffer = new ByteArrayOutputStream();;) {
				byte ascii = coreReadByte();

				if (ascii != 0) {
					buffer.write(ascii);
				} else {
					return new String(buffer.toByteArray(), StandardCharsets.US_ASCII);
				}
			}
		}

		return null;
	}

	public static boolean isSupportedDump(ImageInputStream stream) throws IOException {
		byte[] signature = new byte[EI_NIDENT];

		stream.seek(0);
		stream.readFully(signature);

		return ElfFile.isELF(signature);
	}

	public static ICoreFileReader dumpFromFile(ImageInputStream stream, boolean verbose) throws IOException {
		assert isSupportedDump(stream);

		stream.seek(0);
		byte[] signature = new byte[EI_NIDENT];
		stream.readFully(signature);
		boolean isLittleEndian = (ELFDATA2LSB == signature[5]);
		boolean is64Bit = (ELFCLASS64 == signature[4]);
		DumpReader reader = readerForEndianess(signature[5], signature[4], stream);
		ElfFile file = fileForClass(signature[4], reader);
		return new NewElfDump(file, reader, isLittleEndian, is64Bit, verbose);
	}

	@Override
	public Iterator<String> getAdditionalFileNames() {
		return _additionalFileNames.iterator();
	}

	@Override
	protected MemoryRange[] getMemoryRangesAsArray() {
		return _memoryRanges.toArray(new MemoryRange[_memoryRanges.size()]);
	}

	@Override
	protected boolean isLittleEndian() {
		return _isLittleEndian;
	}

	@Override
	protected boolean is64Bit() {
		return _is64Bit;
	}
}
