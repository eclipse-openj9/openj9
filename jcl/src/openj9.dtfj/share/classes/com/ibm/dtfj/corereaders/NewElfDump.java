/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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

import java.io.FileNotFoundException;
import java.io.IOException;

import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Properties;
import java.util.Set;
import java.util.TreeMap;
import java.util.Vector;

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
	
	private final static int DT_NULL = 0;
	private final static int DT_STRTAB = 5;
	private final static int DT_SONAME = 14;
	private final static int DT_DEBUG = 21;

    private static final int NT_PRSTATUS = 1; // prstatus_t
    //private static final int NT_PRFPREG  = 2; // prfpregset_t
    private static final int NT_PRPSINFO = 3; // prpsinfo_t
    //private static final int NT_TASKSTRUCT = 4;
    private static final int NT_AUXV = 6; // Contains copy of auxv array
    //private static final int NT_PRXFPREG = 0x46e62b7f; // User_xfpregs

    private static final int AT_NULL = 0; // End of vector
    private static final int AT_ENTRY = 9; // Entry point of program
    private static final int AT_PLATFORM = 15; // String identifying platform
    private static final int AT_HWCAP = 16; // Machine dependent hints about processor capabilities

    private static final String SYSTEM_PROP_EXECUTABLE="com.ibm.dtfj.corereaders.executable";
    
    private static abstract class Address {
    	private long _value;
    	private Address(long value) {
    		_value = value;
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
    	Address add(long offset) {
    		long result = getValue() + offset;
    		return new Address32((int) result);
    	}
    	Number asNumber() {
    		return new Integer((int) getValue());
    	}
    	long asAddress() {
    		return getValue();
    	}
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
     *  
     */
    private static class Address31 extends Address32 {
    	Address31(int value) {
    		super(value);
    	}
    	long asAddress() {
			int address = 0x7FFFFFFF & (int) getValue();		
    		return (long) address;
    		
    	}
    }

    private static class Address64 extends Address {
    	Address64(long value) {
    		super(value);
    	}
    	Address add(long offset) {
    		return new Address64(getValue() + offset);
    	}
    	Number asNumber() {
    		return new Long(getValue());
    	}
    	long asAddress() {
    		return getValue();
    	}
    	Address nil() {
    		return new Address64(0);
    	}
    }
    
    private static interface Arch {
    	Map readRegisters(ElfFile file, Builder builder, Object addressSpace) throws IOException;
    	long readUID(ElfFile file) throws IOException;
		Address getStackPointerFrom(Map registers);
		Address getBasePointerFrom(Map registers);
		Address getInstructionPointerFrom(Map registers);
		Address getLinkRegisterFrom(Map registers);
    }
    
    private static class ArchPPC64 implements Arch {
		public Map readRegisters(ElfFile file, Builder builder, Object addressSpace) throws IOException {
			Map registers = new TreeMap();
			for (int i = 0; i < 32; i++) {
				registers.put("gpr" + i, file.readElfWordAsAddress());
			}

			registers.put("pc", file.readElfWordAsAddress());
			file.readElfWordAsAddress(); // skip
			file.readElfWordAsAddress(); // skip
			registers.put("ctr", file.readElfWordAsAddress());
			registers.put("lr", file.readElfWordAsAddress());
			
			// xer is a 32-bit register. Read 64 bits then shift right
			long l = (file.readLong() >> 32) & 0xffffffffL;
			registers.put("xer", new Address64(l));
			
			// cr is a 32-bit register. Read 64 bits then shift right
			l = (file.readLong() >> 32) & 0xffffffffL;
			registers.put("cr", new Address64(l));
			
			return registers;
		}

		public Address getStackPointerFrom(Map registers) {
			return (Address) registers.get("gpr1");
		}

		public Address getBasePointerFrom(Map registers) {
			//Note:  Most RISC ISAs do not distinguish between base pointer and stack pointer
			return getStackPointerFrom(registers);
		}

		public Address getInstructionPointerFrom(Map registers) {
			return (Address) registers.get("pc");
		}

		public Address getLinkRegisterFrom(Map registers) {
			return (Address) registers.get("lr");
		}

		public long readUID(ElfFile file) throws IOException {
			return file.readInt() & 0xffffffffL;
		}
    }
    
    private static class ArchPPC32 extends ArchPPC64 {
    	public Map readRegisters(ElfFile file, Builder builder, Object addressSpace) throws IOException {
			Map registers = new TreeMap();
			for (int i = 0; i < 32; i++) {
				registers.put("gpr" + i, file.readElfWordAsAddress());
			}

			registers.put("pc", file.readElfWordAsAddress());
			file.readElfWordAsAddress(); // skip
			file.readElfWordAsAddress(); // skip
			registers.put("ctr", file.readElfWordAsAddress());
			registers.put("lr", file.readElfWordAsAddress());
			registers.put("xer", file.readElfWordAsAddress());
			registers.put("cr", file.readElfWordAsAddress());
			
			return registers;
    	}
    	
    	public long readUID(ElfFile file) throws IOException {
			return file.readShort() & 0xffffL;
		}
    }
    
    private static class ArchS390 implements Arch {
		public Map readRegisters(ElfFile file, Builder builder, Object addressSpace) throws IOException {
			Map registers = new TreeMap();
			registers.put("mask", file.readElfWordAsAddress());
			registers.put("addr", file.readElfWordAsAddress());
			for (int i = 0; i < 16; i++)
				registers.put("gpr" + i, file.readElfWordAsAddress());
			for (int i = 0; i < 16; i++)
				registers.put("acr" + i, file.readElfWordAsAddress());
			registers.put("origgpr2", file.readElfWordAsAddress());
			registers.put("trap", file.readElfWordAsAddress());
			return registers;
		}

		public Address getStackPointerFrom(Map registers) {
			return (Address) registers.get("gpr15");
		}

		public Address getBasePointerFrom(Map registers) {
			return getStackPointerFrom(registers);
		}

		public Address getInstructionPointerFrom(Map registers) {
			return (Address) registers.get("addr");
		}

		public Address getLinkRegisterFrom(Map registers) {
			return nil(registers);
		}

		public long readUID(ElfFile file) throws IOException {
			return file.readInt() & 0xffffffffL;
		}
		
		private Address nil(Map registers) {
			Address gpr15 = (Address) registers.get("gpr15");
			return gpr15.nil();
		}
    }
    
    private static class ArchS390_32 extends ArchS390 {

    	/**
    	 * CMVC 184757 : j2se_cmdLineTester_DTFJ fails
    	 * 
    	 * Just an observation on the code that follows - it creates Address31 objects but really it
    	 * cannot know whether the values are addresses or not. Really, it ought to be creating "RegisterValue"
    	 * objects (or some such name) which are then sometimes interpreted as addresses when needed. 
    	 * However the confusion between addresses and values is pervasive throughout the code that 
    	 * reads registers, for all platforms, and is not worth fixing. It usually only matters on Z-31
    	 */
    	public Map readRegisters(ElfFile file, Builder builder, Object addressSpace) throws IOException {
			Map registers = new TreeMap();
			registers.put("mask", new Address31(file.readInt())); 
			registers.put("addr", new Address31(file.readInt()));		
			for (int i = 0; i < 16; i++)
				registers.put("gpr" + i, new Address31(file.readInt()));
			for (int i = 0; i < 16; i++)
				registers.put("acr" + i, new Address31(file.readInt()));
			registers.put("origgpr2", new Address31(file.readInt()));
			registers.put("trap", new Address31(file.readInt()));
    		registers.put("old_ilc", new Address31(file.readInt()));
    		return registers;
    	}
		
		private Address readElfWordAsAddress(ElfFile file) throws IOException {
			int address = file.readInt();	
			return new Address32(address);
		}
		
		public long readUID(ElfFile file) throws IOException {
			return file.readShort() & 0xffffL;
		}
    }
    
    private static class ArchIA32 implements Arch {
		public Map readRegisters(ElfFile file, Builder builder, Object addressSpace) throws IOException {
			String[] names1 = {"ebx", "ecx", "edx", "esi", "edi", "ebp", "eax", "ds", "es", "fs", "gs"};
			String[] names2 = {"eip", "cs", "efl", "esp", "ss"};
			Map registers = new TreeMap();
			for (int i = 0; i < names1.length; i++)
				registers.put(names1[i], new Address32(file.readInt()));
			file.readInt(); // Ignore originalEax
			for (int i = 0; i < names2.length; i++)
				registers.put(names2[i], new Address32(file.readInt()));
			return registers;
		}

		public Address getStackPointerFrom(Map registers) {
			return (Address) registers.get("esp");
		}

		public Address getBasePointerFrom(Map registers) {
			return (Address) registers.get("ebp");
		}

		public Address getInstructionPointerFrom(Map registers) {
			return (Address) registers.get("eip");
		}

		public Address getLinkRegisterFrom(Map registers) {
			return new Address32(0);
		}

		public long readUID(ElfFile file) throws IOException {
			return file.readShort() & 0xffffL;
		}
    }
    
    private static class ArchIA64 implements Arch {
		public Map readRegisters(ElfFile file, Builder builder, Object addressSpace) throws IOException {
			// Ignore the R Set of 32 registers
			for (int i = 0; i < 32; i++)
				file.readLong();
			
			Map registers = new TreeMap();
			registers.put("nat", new Address64(file.readLong()));
			registers.put("pr", new Address64(file.readLong()));
    		
			// Ignore the B Set of 8 registers
			for (int i = 0; i < 8; i++)
				file.readLong();

			String[] names = {"ip", "cfm", "psr", "rsc", "bsp", "bspstore", "rnat", "ccv", "unat", "fpsr", "pfs", "lc", "ec"};
			for (int i = 0; i < names.length; i++)
				registers.put(names[i], new Address64(file.readLong()));

			// The register structure contains 128 words: 32 R, 8 B, 15 irregular and 73 reserved.
			// It is not really necessary to read the reserved words.
			// for (int i = 0; i < 73; i++)
			//		reader.readLong();

			return registers;
		}

		public Address getStackPointerFrom(Map registers) {
			return new Address64(0);
		}

		public Address getBasePointerFrom(Map registers) {
			return new Address64(0);
		}

		public Address getInstructionPointerFrom(Map registers) {
			return new Address64(0);
		}

		public Address getLinkRegisterFrom(Map registers) {
			return new Address64(0);
		}

		public long readUID(ElfFile file) throws IOException {
			return file.readInt() & 0xffffffffL;
		}
    }

    private static class ArchAMD64 implements Arch {
		public Map readRegisters(ElfFile file, Builder builder, Object addressSpace) throws IOException {
			String[] names1 = {"r15", "r14", "r13", "r12", "rbp", "rbx", "r11", "r10", "r9", "r8", "rax", "rcx", "rdx", "rsi", "rdi"};
			String[] names2 = {"rip", "cs", "eflags", "rsp", "ss", "fs_base", "gs_base", "ds", "es", "fs", "gs"};
			Map registers = new TreeMap();
			for (int i = 0; i < names1.length; i++)
				registers.put(names1[i], new Address64(file.readLong()));
			file.readLong(); // Ignore originalEax
			for (int i = 0; i < names2.length; i++)
				registers.put(names2[i], new Address64(file.readLong()));
			return registers;
		}

		public Address getStackPointerFrom(Map registers) {
			return (Address) registers.get("rsp");
		}

		public Address getBasePointerFrom(Map registers) {
			return (Address) registers.get("rbp");
		}

		public Address getInstructionPointerFrom(Map registers) {
			return (Address) registers.get("rip");
		}

		public Address getLinkRegisterFrom(Map registers) {
			return new Address64(0);
		}

		public long readUID(ElfFile file) throws IOException {
			return file.readInt() & 0xffffffffL;
		}
    }

    private static class ArchARM32 implements Arch {
		public Map readRegisters(ElfFile file, Builder builder, Object addressSpace) throws IOException {
			Map registers = new TreeMap();	
			for (int i = 0; i < 13; i++) {
				registers.put( "r"+i, new Address32(file.readInt()));
			}
			// The next registers have names
			registers.put( "sp", new Address32(file.readInt()));
			registers.put( "lr", new Address32(file.readInt()));
			registers.put( "pc", new Address32(file.readInt()));
			registers.put( "spsr", new Address32(file.readInt()));
			
			return registers;
		}

		public Address getStackPointerFrom(Map registers) {
			return (Address) registers.get("sp");
		}

		public Address getBasePointerFrom(Map registers) {
			return (Address) registers.get("sp");
		}

		public Address getInstructionPointerFrom(Map registers) {
			return (Address) registers.get("pc");
		}

		public Address getLinkRegisterFrom(Map registers) {
			return (Address) registers.get("lr");
		}

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
    	//private long _size;
    	private byte _info;
    	//private byte _other;
    	//private int _sectionIndex;
    	
    	Symbol(long name, long value, long size, byte info, byte other, int sectionIndex) {
    		this.name = name;
    		this.value = value;
    		//_size = size;
    		_info = info;
    		//_other = other;
    		//_sectionIndex = sectionIndex;
    	}

		boolean isFunction() {
			return STT_FUNC == (_info & ST_TYPE_MASK);
		}
    }
    
    private static class SectionHeaderEntry {
		private static final int SHT_SYMTAB = 2; // Symbol table in this section
		//private static final int SHT_STRTAB = 3; // String table in this section
		private static final int SHT_DYNSYM = 11; // Symbol table in this section
		//private static final int SHT_DYNSTR = 99; // String table in this section

    	private long _name; // index into section header string table 
    	private long _type;
    	//private long _flags;
    	//private long _address;
    	final long offset;
    	final long size;
    	final long link;
    	//private long _info;

    	SectionHeaderEntry(long name, long type, long flags, long address, long offset, long size, long link, long info) {
			_name = name;
			_type = type;
			//_flags = flags;
			//_address = address;
			this.offset = offset;
			this.size = size;
			this.link = link;
			//_info = info;
		}
    	
    	boolean isSymbolTable() {
    		return SHT_SYMTAB == _type || SHT_DYNSYM == _type;
    	}
    }

	private static class ProgramHeaderEntry {
		// Program Headers defined in /usr/include/linux/elf.h
		//type constants
		//private static final int PT_NULL = 0;
		private static final int PT_LOAD = 1;
		private static final int PT_DYNAMIC = 2;
		//private static final int PT_INTERP = 3;
		private static final int PT_NOTE = 4;
		//private static final int PT_SHLIB = 5;
		//private static final int PT_PHDR = 6;
		//private static final int PT_TLS = 7;
		private static final int PF_X = 1;
		private static final int PF_W = 2;
		//private static final int PF_R = 4;

		//PHE structure definitions:
		private int _type;
		final long fileOffset;
		final long fileSize; // if fileSize == 0 then we have to map memory from external lib file.
		final long virtualAddress;
		//private long _physicalAddress;
		final long memorySize;
		private int _flags;
		//private long _alignment;

		ProgramHeaderEntry(int type, long fileOffset, long fileSize, long virtualAddress, long physicalAddress, long memorySize, int flags, long alignment) {
			_type = type;
			this.fileOffset = fileOffset;
			this.fileSize = fileSize;
			this.virtualAddress = virtualAddress;
			//_physicalAddress = physicalAddress;
			this.memorySize = memorySize;
			_flags = flags;
			//_alignment = alignment;
		}

		boolean isEmpty() {
			//if the core is not a complete dump of the address space, there will be missing pieces.  These are manifested as program header entries with a real memory size but a zero file size
			//	That is, even though they do occupy space in memory, they are represented in none of the code
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
//			if (0 == fileSize) {
//				// See comments to CMVC 137737
//				return null;
//			}
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
		private DumpReader _reader;
		private long _offset = 0;
		private Arch _arch = null;
		private long _programHeaderOffset = -1;
		private long _sectionHeaderOffset = -1;
		private short _programHeaderEntrySize = 0;
		private short _programHeaderCount = 0;
		private short _sectionHeaderEntrySize = 0;
		private short _sectionHeaderCount = 0;
		private List _processEntries = new ArrayList();
		private List _threadEntries = new ArrayList();
		private List _auxiliaryVectorEntries = new ArrayList();
		private List _programHeaderEntries = new ArrayList();
		private List _sectionHeaderEntries = new ArrayList();
		
		private short _objectType = 0;
		private int _version = 0;
		private int _e_flags = 0;

		protected abstract long padToWordBoundary(long l);
		protected abstract ProgramHeaderEntry readProgramHeaderEntry() throws IOException;
		protected abstract long readElfWord() throws IOException;
		protected abstract Address readElfWordAsAddress() throws IOException;

		ElfFile(DumpReader reader) {
			_reader = reader;
		}
		
		ElfFile(DumpReader reader, long offset) {
			_reader = reader;
			_offset = offset;
		}
		
		Iterator processEntries() {
			return _processEntries.iterator();
		}
		
		Iterator threadEntries() {
			return _threadEntries.iterator();
		}
		
		Iterator auxiliaryVectorEntries() {
			return _auxiliaryVectorEntries.iterator();
		}

		Iterator programHeaderEntries() {
			return _programHeaderEntries.iterator();
		}
		
		Iterator sectionHeaderEntries() {
			return _sectionHeaderEntries.iterator();
		}
		
		SectionHeaderEntry sectionHeaderEntryAt(int i) {
			return (SectionHeaderEntry) _sectionHeaderEntries.get(i);
		}

		protected void readFile() throws IOException
		{
			seek(0);
			readHeader();
			readSectionHeader();
			readProgramHeader();
		}

		public static boolean isELF(byte[] signature)
		{
			// 0x7F, 'E', 'L', 'F'
			return (0x7F == signature[0] && 0x45 == signature[1]
					&& 0x4C == signature[2] && 0x46 == signature[3]);
		}
		
		private void readHeader() throws IOException
		{
			byte[] signature = readBytes(EI_NIDENT);
			
			if (! isELF(signature)) {
				throw new IOException("Missing ELF magic number");
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
				return new ArchS390();
			case ARCH_IA32:
				return new ArchIA32();
			case ARCH_AMD64:
				return new ArchAMD64();
			case ARCH_ARM32:
				return new ArchARM32();
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
			for (Iterator iter = _programHeaderEntries.iterator(); iter.hasNext();) {
				ProgramHeaderEntry entry = (ProgramHeaderEntry) iter.next();
				if (entry.isNote())
					readNotes(entry);
			}
		}

		private void readNotes(ProgramHeaderEntry entry) throws IOException {
			long offset = entry.fileOffset;
			long limit = offset + entry.fileSize;
			while (offset < limit)
				offset = readNote(offset);
		}

		private long readNote(long offset) throws IOException {
			seek(offset);
			long nameLength = padToWordBoundary(readInt());
			long dataSize = readInt();
			long type = readInt();
			
			long dataOffset = offset + ELF_NOTE_HEADER_SIZE + nameLength;
			
			if (NT_PRSTATUS == type)
				addThreadEntry(dataOffset, dataSize);
			else if (NT_PRPSINFO == type)
				addProcessEntry(dataOffset, dataSize);
			else if (NT_AUXV == type)
				addAuxiliaryVectorEntry(dataOffset, dataSize);
			
			return dataOffset + padToWordBoundary(dataSize);
		}

		protected void addProcessEntry(long offset, long size) {
			_processEntries.add(new DataEntry(offset, size));
		}

		protected void addThreadEntry(long offset, long size) {
			_threadEntries.add(new DataEntry(offset, size));
		}

		private void addAuxiliaryVectorEntry(long offset, long size) {
			_auxiliaryVectorEntries .add(new DataEntry(offset, size));
		}

		protected byte readByte() throws IOException {
			return _reader.readByte();
		}

		protected byte[] readBytes(int n) throws IOException {
			return _reader.readBytes(n);
		}

		private String readStringAtAddress(long address) throws IOException
		{
			String toReturn = null;
			
			StringBuffer buf = new StringBuffer();
			seekToAddress(address);
			byte b = readByte();
			for (long i = 1; 0 != b; i++) {
				buf.append(new String(new byte[] {b}, "ASCII"));
				b = readByte();
			}
			toReturn = buf.toString();
			return toReturn;
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
		
		Address getStackPointerFrom(Map registers) {
			return _arch.getStackPointerFrom(registers);
		}
		
		Address getBasePointerFrom(Map registers) {
			return _arch.getBasePointerFrom(registers);
		}
		
		Address getInstructionPointerFrom(Map registers) {
			return _arch.getInstructionPointerFrom(registers);
		}
		
		Address getLinkRegisterFrom(Map registers) {
			return _arch.getLinkRegisterFrom(registers);
		}
		
		Map readRegisters(Builder builder, Object addressSpace) throws IOException {
			return _arch.readRegisters(this, builder, addressSpace);
		}

		abstract Iterator readSymbolsAt(SectionHeaderEntry entry) throws IOException;
		abstract int addressSize();
		
		/* file types corresponding to the _objectType field */
		private final static short ET_NONE = 0;               /* No file type */
		private final static short ET_REL = 1;               /* Relocatable file */
		private final static short ET_EXEC = 2;               /* Executable file */
		private final static short ET_DYN = 3;               /* Shared object file */
		private final static short ET_CORE = 4;               /* Core file */
		private final static short ET_NUM = 5;               /* Number of defined types */
		//these are unsigned values so the constants can't be shorts
		private final static int ET_LOOS = 0xfe00;          /* OS-specific range start */
		private final static int ET_HIOS = 0xfeff;          /* OS-specific range end */
		private final static int ET_LOPROC = 0xff00;          /* Processor-specific range start */
		private final static int ET_HIPROC = 0xffff;         /* Processor-specific range end */
		
		public Properties getProperties()
		{
			Properties props = new Properties();
			props.setProperty("Object file type", _nameForFileType(_objectType));
			props.setProperty("Object file version", Integer.toHexString(_version));
			props.setProperty("Processor-specific flags", Integer.toHexString(_e_flags));
			
			return props;
		}
		
		private String _nameForFileType(short type)
		{
			String fileType = "Unknown";
			int typeAsInt = 0xFFFF & type;
			
			if (ET_NONE == type) {
				fileType = "No file type";
			} else if (ET_REL == type) {
				fileType = "Relocatable file";
			} else if (ET_EXEC == type) {
				fileType = "Executable file";
			} else if (ET_DYN == type) {
				fileType = "Shared object file";
			} else if (ET_CORE == type) {
				fileType = "Core file";
			} else if (ET_NUM == type) {
				fileType = "Number of defined types";
			} else if ((ET_LOOS <= typeAsInt) && (typeAsInt <= ET_HIOS)) {
				fileType = "OS-specific (" + Integer.toHexString(typeAsInt) + ")";
			} else if ((ET_LOPROC <= typeAsInt) && (typeAsInt <= ET_HIPROC)) {
				fileType = "Processor-specific (" + Integer.toHexString(typeAsInt) + ")";
			}
			return fileType;
		}
		
		private void seekToAddress(long address) throws IOException {
			ProgramHeaderEntry matchedEntry = null;
			
			for (Iterator iter = programHeaderEntries(); iter.hasNext();) {
				ProgramHeaderEntry element = (ProgramHeaderEntry) iter.next();
				if (element.contains(address)) {
					assert (null == matchedEntry) : "Multiple mappings for the same address";
					matchedEntry = element;
				}
			}
			if (null == matchedEntry) {
				throw new IOException("No ProgramHeaderEntry found for address");
			} else {
				seek(matchedEntry.fileOffset + (address - matchedEntry.virtualAddress));
			}
		}
		
		public String getSONAME() throws IOException {
			String soname = null;
			for (Object e : _programHeaderEntries) {
				ProgramHeaderEntry entry  = (ProgramHeaderEntry)e;
				if( !entry.isDynamic() ) {
					continue;
				}
				soname = readSONAMEFromProgramHeader(entry);
				if( soname != null ) {
					//System.err.println(String.format("Found module name %s, adding to _additionalFileNames", soname));
					break;
				}
			}
			return soname;
		}
		
		private String readSONAMEFromProgramHeader( ProgramHeaderEntry entry ) throws IOException {

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
				 * DT_RUNPATH 			29 	d_val 	optional 	optional
				 * DT_FLAGS 			30 	d_val 	optional 	optional
				 * DT_ENCODING 			32 	unspecified 	unspecified 	unspecified
				 * DT_PREINIT_ARRAY 	32 	d_ptr 	optional 	ignored
				 * DT_PREINIT_ARRAYSZ 	33 	d_val 	optional 	ignored
				 */
				if (!((tag >= 0         ) && (tag <= 33        )) &&
						!((tag >= 0x60000000) && (tag <= 0x6FFFFFFF)) &&
						!((tag >= 0x70000000) && (tag <= 0x7FFFFFFF))    ) {
					break;
				}
				if( tag == DT_SONAME ) {
					sonameAddress = address;
				} else if( tag == DT_STRTAB ) {
					strtabAddress = address;
				}

				//System.err.println("Found dynamic section tag 0x" + Long.toHexString(tag));
			} while ((tag != DT_NULL));

			// If there isn't SONAME entry we can't get the shared object name.
			// The actual SONAME pointed to is in the string table so we need that too.
			if ( sonameAddress > -1 && strtabAddress > -1 ) {
				String soname = readStringAtAddress((strtabAddress + sonameAddress));
				return soname;				
			} else {
				return null;
			}
		}
		
	}
	
	/*
	 * 32-bit Elf header
	 *
	 *	typedef struct
	 *	{
	 *	  unsigned char e_ident[EI_NIDENT];     // Magic number and other info
	 *	  Elf32_Half    e_type;                 // Object file type
	 *	  Elf32_Half    e_machine;              // Architecture
	 *	  Elf32_Word    e_version;              // Object file version
	 *	  Elf32_Addr    e_entry;                // Entry point virtual address
	 *	  Elf32_Off     e_phoff;                // Program header table file offset
	 *	  Elf32_Off     e_shoff;                // Section header table file offset
	 *	  Elf32_Word    e_flags;                // Processor-specific flags
	 *	  Elf32_Half    e_ehsize;               // ELF header size in bytes
	 *	  Elf32_Half    e_phentsize;            // Program header table entry size
	 *	  Elf32_Half    e_phnum;                // Program header table entry count
	 *	  Elf32_Half    e_shentsize;            // Section header table entry size
	 *	  Elf32_Half    e_shnum;                // Section header table entry count
	 *	  Elf32_Half    e_shstrndx;             // Section header string table index
	 *	} Elf32_Ehdr;
	 */
	private static class Elf32File extends ElfFile {
		protected Arch architectureFor(short s) {
			if (ARCH_S390 == s)
				return new ArchS390_32();
			return super.architectureFor(s);
		}

		Elf32File(DumpReader reader) throws IOException {
			super(reader);
			readFile();
		}
		
		Elf32File(DumpReader reader, long offset) throws IOException {
			super(reader, offset);
			readFile();
		}

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

		private long unsigned(int i) {
			return i & 0xffffffffL;
		}

		protected long padToWordBoundary(long l) {
			return ((l + 3) / 4) * 4;
		}

		protected long readElfWord() throws IOException {
			return readInt() & 0xffffffffL;
		}

		protected Address readElfWordAsAddress() throws IOException {
			return new Address32(readInt());
		}

		int addressSize() {
			return 32;
		}

		Iterator readSymbolsAt(SectionHeaderEntry entry) throws IOException {
			seek(entry.offset);
			List symbols = new ArrayList();
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
	 * Elf 64-bit header
	 * 
	 *	typedef struct
	 *	{
	 *	  unsigned char e_ident[EI_NIDENT];     // Magic number and other info
	 *	  Elf64_Half    e_type;                 // Object file type
	 *	  Elf64_Half    e_machine;              // Architecture
	 *	  Elf64_Word    e_version;              // Object file version
	 *	  Elf64_Addr    e_entry;                // Entry point virtual address
	 *	  Elf64_Off     e_phoff;                // Program header table file offset
	 *	  Elf64_Off     e_shoff;                // Section header table file offset
	 *	  Elf64_Word    e_flags;                // Processor-specific flags
	 *	  Elf64_Half    e_ehsize;               // ELF header size in bytes
	 *	  Elf64_Half    e_phentsize;            // Program header table entry size
	 *	  Elf64_Half    e_phnum;                // Program header table entry count
	 *	  Elf64_Half    e_shentsize;            // Section header table entry size
	 *	  Elf64_Half    e_shnum;                // Section header table entry count
	 *	  Elf64_Half    e_shstrndx;             // Section header string table index
	 *	} Elf64_Ehdr;
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

		protected long padToWordBoundary(long l) {
			return ((l + 7) / 8) * 8;
		}

		protected long readElfWord() throws IOException {
			return readLong();
		}

		protected Address readElfWordAsAddress() throws IOException {
			return new Address64(readLong());
		}

		int addressSize() {
			return 64;
		}

		Iterator readSymbolsAt(SectionHeaderEntry entry) throws IOException {
			seek(entry.offset);
			List symbols = new ArrayList();
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

	private List _memoryRanges = new ArrayList();
	private Set _additionalFileNames = new HashSet();
	private long _platformIdAddress = 0;
	private ElfFile _file;
	private boolean _isLittleEndian;
	private boolean _is64Bit;
	private boolean _verbose;

	private Object readProcess(DataEntry entry, Builder builder, Object addressSpace, List threads) throws IOException, CorruptCoreException, MemoryAccessException {
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
		String dumpCommandLine = new String(_file.readBytes(ELF_PRARGSZ), "ASCII").trim();
		
		Properties environment = getEnvironmentVariables(builder);
		String alternateCommandLine = ""; 
		if (null != environment) {
			alternateCommandLine = environment.getProperty("IBM_JAVA_COMMAND_LINE","");
		}
		
		String commandLine = dumpCommandLine.length() >= alternateCommandLine.length() ? dumpCommandLine : alternateCommandLine; 
		
		int space = commandLine.indexOf(" ");
		String executable = commandLine;
		if (0 < space)
			executable = executable.substring(0, space);

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
			} 
		
			if (didFork) {
				try {
					long tid = _j9rasReader.getThreadID();
					// fake a thread with no other useful info than the TID.
					failingThread = builder.buildThread(Long.toString(tid), Collections.EMPTY_LIST.iterator(), Collections.EMPTY_LIST.iterator(), Collections.EMPTY_LIST.iterator(), new Properties(), 0);
					threads.add(0,failingThread);
				} catch (UnsupportedOperationException uoe) {
				}
			}
		}

		
		
        return buildProcess(builder, addressSpace, pid, commandLine,
				 			getEnvironmentVariables(builder), failingThread,
				 			threads.iterator(), executable);
	}

	private Object buildProcess(Builder builder, Object addressSpace, String pid, String commandLine, Properties environmentVariables, Object currentThread, Iterator threads, String executableName) throws IOException, MemoryAccessException
	{
		List modules = readModules(builder, addressSpace, executableName);
		Iterator libraries = modules.iterator();
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
	private List readModules(Builder builder, Object addressSpace, String executableName) throws IOException, MemoryAccessException
	{
		// System.err.println("NewElfDump.readModules() entered en=" + executableName);
		List modules = new ArrayList();
		
		ClosingFileReader file;
		String overrideExecutableName = System.getProperty(SYSTEM_PROP_EXECUTABLE);
		if (overrideExecutableName == null) {
			file = _findFileInPath(builder, executableName, System.getProperty("java.class.path","."));
		} else {
			// Use override for the executable name. This supports the jextract -f <executable> option, for
			// cases where the launcher path+name is truncated by the 80 character OS limit, AND it was a 
			// custom launcher, so the alternative IBM_JAVA_COMMAND_LINE property was not set.
			file =_findFileInPath(builder, overrideExecutableName, System.getProperty("java.class.path","."));
		}
				
		if (null != file) {
			// System.err.println("NewElfDump.readModules() opened file");
			
			ElfFile executable = elfFileFrom(file);
			if (null != executable) {
				// System.err.println("NewElfDump.readModules() found executable object");
				
				ProgramHeaderEntry dynamic = null;
				for (Iterator iter = executable.programHeaderEntries(); null == dynamic && iter.hasNext();) {
					ProgramHeaderEntry entry = (ProgramHeaderEntry) iter.next();
					if (entry.isDynamic()) {
						dynamic = entry;
					}
				}
				
				if (null != dynamic) {
					// System.err.println("NewElfDump.readModules() found 'dynamic' program header object");

					List symbols = readSymbolsFrom(builder, addressSpace, executable, dynamic.virtualAddress);
					long imageStart = dynamic.virtualAddress;
					if (isValidAddress(imageStart)) {
						Iterator sections = _buildModuleSections(builder, addressSpace, executable, dynamic.virtualAddress);
						Properties properties = executable.getProperties();
						modules.add(builder.buildModule(executableName, properties, sections, symbols.iterator(),imageStart));
						modules.addAll(readLibrariesAt(builder, addressSpace, imageStart));
						_additionalFileNames.add(executableName);
					}
				}
			} else {
				//call in here if we can't find the executable
				builder.setExecutableUnavailable("Executable file \"" + executableName + "\" not found");
			}
		} else {
			//call in here if we can't find the executable
			builder.setExecutableUnavailable("File \"" + executableName + "\" not found");
			if (_verbose) {
				System.err.println("Warning: executable " + executableName + " not found, unable to collect libraries." +
					" Please retry with jextract -f option.");
			}
		}
		
		// Get all the loaded modules .so names to make sure we have the
		// full list of libraries for jextract to zip up. CMVC 194366
		for (Object e : _file._programHeaderEntries) {
			ProgramHeaderEntry entry = (ProgramHeaderEntry)e;
			if( !entry.isLoadable() ) {
				continue;
			}
			try {
				ElfFile moduleFile = null;
				if( _is64Bit ) {
					moduleFile = new Elf64File(_file._reader, entry.fileOffset);
				} else {
					moduleFile = new Elf32File(_file._reader, entry.fileOffset);
				}
				// Now search the dynamic entries for this so file to get it's SONAME.
				// "lib.so" is returned for the executable and should be ignored.
				String soname = moduleFile.getSONAME();
				if( soname != null && !"lib.so".equals(soname)) {
					_additionalFileNames.add(soname);
				}
			} catch( Exception ex) {
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
	private ClosingFileReader _findFileInPath(Builder builder, String filename, String path)
	{
		ClosingFileReader file = null;
		
		try{
			//first try it as whatever we were given first
			file = builder.openFile(filename);
		} catch (IOException e) {
			//it wasn't there so fall back to the path
			Iterator components = _componentsSeparatedBy(path, ":").iterator();
			
			while ((null == file) && (components.hasNext())) {
				String component = (String)(components.next());
				try{
					//first try it as whatever we were given first
					file = builder.openFile(component + "/" + filename);
				} catch (IOException e2) {
					//do nothing, most of these will be file not found
				}
			}
		}
		return file;
	}

	/**
	 * Due to our build environment, we can't use the 1.4.2 split function so this is a similar method
	 * 
	 * @param path
	 * @param separator
	 * @return
	 */
	private static List _componentsSeparatedBy(String path, String separator)
	{
		Vector list = new Vector();
		
		while (null != path) {
			int index = path.indexOf(separator);
			
			if (index >= 0) {
				String component = path.substring(0, index);
				
				list.add(component);
				path = path.substring(index + 1);
			} else {
				path = null;
			}
		}
		return list;
	}

	private static final long SHT_STRTAB = 3;
	
	
	private Iterator _buildModuleSections(Builder builder, Object addressSpace, ElfFile executable, long loadedBaseAddress) throws IOException
	{
		Vector sections = new Vector();
		Iterator shentries = executable.sectionHeaderEntries();
		byte[] strings = null;
		
		// Find the string table by looking for a section header entry of type string  
		// table which, when resolved against itself, has the name ".shstrtab". 
		// NB : If the ELF file is invalid, the method stringFromBytesAt() may return null.
		while (shentries.hasNext()) {
			SectionHeaderEntry entry = (SectionHeaderEntry) shentries.next();
			if (SHT_STRTAB == entry._type) {
				executable.seek(entry.offset);
				byte[] attempt = executable.readBytes((int)(entry.size));
				String peak = stringFromBytesAt(attempt, entry._name);
				if (peak != null) {
					if (peak.equals(".shstrtab")) {
						strings = attempt;
						break;	// found it
					}
				} else {
					if (_verbose) {
						System.err.println("\tError reading section header name at file offset=0x" + Long.toHexString(entry.offset));
					}
				}
			}
		}
		
		// Loop through the section header entries building a module section for each one
		// NB : If the ELF file is invalid, the method stringFromBytesAt() may return null.
		shentries = executable.sectionHeaderEntries();
		while (shentries.hasNext()) {
			SectionHeaderEntry entry = (SectionHeaderEntry) shentries.next();
			String name = "";
			
			if (null != strings) {
				name = stringFromBytesAt(strings, entry._name);
				if (name == null) {
					if (_verbose) {
						System.err.println("\tError reading section header name at file offset=0x" + Long.toHexString(entry.offset));
					}
					name = "";
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
		for (Iterator iter = _file.programHeaderEntries(); iter.hasNext();) {
			ProgramHeaderEntry element = (ProgramHeaderEntry) iter.next();
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
		for (Iterator iter = _file.programHeaderEntries(); iter.hasNext();) {
			ProgramHeaderEntry element = (ProgramHeaderEntry) iter.next();
			if (element.validInProcess(address)) {
				return true;
			}
		}
		return false;
	}

	private List readLibrariesAt(Builder builder, Object addressSpace, long imageStart) throws MemoryAccessException, IOException {
	
		//System.err.println("NewElfDump.readLibrariesAt() entered is=0x" + Long.toHexString(imageStart));
		
		// Create the method return value
		List libraries = new ArrayList();
		
		// Seek to the start of the dynamic section
		seekToAddress(imageStart);

		// Loop reading the dynamic section tag-value/pointer pairs until a 'debug'
		// entry is found
		long tag;
		long address;

		do {
			// Read the tag and the value/pointer (only used after the loop has terminated)
			tag     = _file.readElfWord();
			address = _file.readElfWord();
			
			// Check that the tag is valid. As there may be some debate about the
			// set of valid values, a message will be issued but reading will continue
			/*
			 * CMVC 161449 - SVT:70:jextract invalid tag error
			 * The range of valid values in the header has been expanded, so increasing the valid range
			 * http://refspecs.freestandards.org/elf/gabi4+/ch5.dynamic.html
			 * DT_RUNPATH 			29 	d_val 	optional 	optional
			 * DT_FLAGS 			30 	d_val 	optional 	optional
			 * DT_ENCODING 			31 	unspecified 	unspecified 	unspecified
			 * DT_PREINIT_ARRAY 	32 	d_ptr 	optional 	ignored
			 * DT_PREINIT_ARRAYSZ 	33 	d_val 	optional 	ignored
			 */
			if (_verbose) {
				if (!((tag >= 0         ) && (tag <= 33        )) &&
					!((tag >= 0x60000000) && (tag <= 0x6FFFFFFF)) &&
					!((tag >= 0x70000000) && (tag <= 0x7FFFFFFF))    ) {
					System.err.println("Error reading dynamic section. Invalid tag value '0x" + Long.toHexString(tag));
				}
			}
			
			// System.err.println("Found dynamic section tag 0x" + Long.toHexString(tag));
		} while ((tag != DT_NULL) && (tag != DT_DEBUG));

		// If there is no debug section, there is nothing to do
		if ( tag != DT_DEBUG) {
			return libraries;
		}

		// Seek to the start of the debug data
		seekToAddress(address);
			
		// NOTE the rendezvous structure is described in /usr/include/link.h
		//	struct r_debug {
		//		int r_version;
		//		struct link_map *r_map;
		//		ElfW(Addr) r_brk;		/* Really a function pointer */
		//		enum { ... } r_state;
		//		ElfW(Addr) r_ldbase;
		//	};
		//	struct link_map {
		   //		ElfW(Addr) l_addr;		/* Base address shared object is loaded at.  */
		   //		char *l_name;			/* Absolute file name object was found in.  */
		   //		ElfW(Dyn) *l_ld;		/* Dynamic section of the shared object.  */
		   //		struct link_map *l_next, *l_prev;	/* Chain of loaded objects.  */
		//	};
			
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
				List symbols = new ArrayList();
				String name = readStringAt(nameAddress);
				Iterator sections = (new Vector()).iterator();

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
				if ((null != range) && (range.getVirtualAddress() == loadedBaseAddress) && (null != name) && (name.length() > 0) && (name.startsWith("/"))) {
					try {
						ClosingFileReader file = builder.openFile(name);
						if (null != file) {
							if (_verbose) {
								System.err.println("Reading library file " + name);
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
								symbols.add(builder.buildCorruptData(addressSpace, "unable to find module " + name, loadedBaseAddress));
							}
						}
					} catch (FileNotFoundException exc) {
						// As for null file return above, if we can't find the library file we follow the normal
						// DTFJ convention and leave the properties, sections and symbols iterators empty. 
					}
					libraries.add(builder.buildModule(name, properties, sections, symbols.iterator(),loadedBaseAddress));
				}
			}
		}

		return libraries;
	}

	private MemoryRange memoryRangeFor(long address)
	{
		//TODO:  change the callers of this method to use some mechanism which will work better within the realm of the new address spaces
		Iterator ranges = _memoryRanges.iterator();
		MemoryRange match = null;
		
		while ((null == match) && ranges.hasNext()) {
			MemoryRange range = (MemoryRange) ranges.next();
			
			if (range.contains(address)) {
				match = range;
			}
		}
		return match;
	}

	private void seekToAddress(long address) throws IOException {
		ProgramHeaderEntry matchedEntry = null;
		
		for (Iterator iter = _file.programHeaderEntries(); iter.hasNext();) {
			ProgramHeaderEntry element = (ProgramHeaderEntry) iter.next();
			if (element.contains(address)) {
				assert (null == matchedEntry) : "Multiple mappings for the same address";
				matchedEntry = element;
			}
		}
		if (null == matchedEntry) {
			throw new IOException("No ProgramHeaderEntry found for address");
		} else {
			coreSeek(matchedEntry.fileOffset + (address - matchedEntry.virtualAddress));
		}
	}

	private List readSymbolsFrom(Builder builder, Object addressSpace, ElfFile file, long baseAddress) throws IOException {
		List symbols = new ArrayList();
		for (Iterator iter = file.sectionHeaderEntries(); iter.hasNext();) {
			SectionHeaderEntry entry = (SectionHeaderEntry) iter.next();
			if (entry.isSymbolTable())
				symbols.addAll(readSymbolsFrom(builder, addressSpace, file, entry, baseAddress));
		}
		return symbols;
	}

	private List readSymbolsFrom(Builder builder, Object addressSpace, ElfFile file, SectionHeaderEntry entry, long baseAddress) throws IOException {
		List symbols = new ArrayList();
		SectionHeaderEntry stringTable = file.sectionHeaderEntryAt((int) entry.link);
		file.seek(stringTable.offset);
		byte[] strings = file.readBytes((int) stringTable.size);
		for (Iterator iter = file.readSymbolsAt(entry); iter.hasNext();) {
			Symbol sym = (Symbol) iter.next();
			if (sym.isFunction()) {
				String name = stringFromBytesAt(strings, sym.name);
				if (name != null) {
					if (sym.value != 0) {
						symbols.add(builder.buildSymbol(addressSpace, name, baseAddress + sym.value));
					}
				} else {
					if (_verbose) {
						System.err.println("\tError reading section header name at file offset=0x" + Long.toHexString(stringTable.offset));
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
				System.err.println("\tError in string table offset, value=0x" + Long.toHexString(offset));
			}
			return null;
		}
		
		// As the offset is now known to be small (definitely fits in 32 bits), it
		// can be safely cast to an int
		int startOffset = (int)offset;
		int endOffset   = startOffset;
		
		// Scan along the characters checking they are valid (else undefined behaviour
		// converting to a string) and locating the terminating null (if any!!)
		for (endOffset = startOffset; endOffset < stringTableBytes.length; endOffset++) {
			if (stringTableBytes[endOffset] >= 128) {
				if (_verbose) {
					System.err.println("\tError in string table. Non ASCII character encountered.");
				}
				return null;				
			} else if (stringTableBytes[endOffset] == 0) {
				break;
			}
		}
		
		// Check that the string is null terminated
		if (stringTableBytes[endOffset] != 0) {
			if (_verbose) {
				System.err.println("\tError in string table. The string is not terminated.");
			}
			return null;			
		}

		// Convert the bytes to a string
		try {
			String result = new String(stringTableBytes, startOffset, endOffset - startOffset, "ASCII");
			// System.err.println("Read string '" + result + "' from string table");
			return result;
		} catch (UnsupportedEncodingException exception) {
			System.err.println("Error (UnsupportedEncodingException) converting string table characters. The core file is invalid and the results may unpredictable");
		} catch (IndexOutOfBoundsException exception) {
			System.err.println("Error (IndexOutOfBoundsException) converting string table characters. The core file is invalid and the results may unpredictable");
		}
		
		return null;
	}
	
	private Properties getEnvironmentVariables(Builder builder) throws MemoryAccessException, IOException {
		// builder.getEnvironmentAddress() points to a pointer to a null
		// terminated list of addresses which point to strings of form x=y

		// Get environment variable addresses
		long environmentAddress = builder.getEnvironmentAddress();
		if (0 == environmentAddress)
			return null;

		List addresses = new ArrayList();
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
			//CMVC 161796
			//catch the IO exception, give up trying to extract the env vars and attempt to carry on parsing the core file
			
			//CMVC 164739
			//Cannot propagate detected condition upwards without changing external (and internal) APIs.
		}

		// Get environment variables
		Properties environment = new Properties();
		for (Iterator iter = addresses.iterator(); iter.hasNext();) {
			Address variable = (Address) iter.next();
			StringBuffer buffer = new StringBuffer();
			try {
				seekToAddress(variable.asAddress());
			} catch (IOException e) {
				// catch here rather than propagate up to method extract() - we can 
				// carry on getting other environment variables
				continue;
			}
	
			byte b = coreReadByte();
			while (0 != b) {
				buffer.append(new String(new byte[] {b}, "ASCII"));
				b = coreReadByte();
			}
			// Now buffer is the x=y string
			String pair = buffer.toString();
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
	private Object readThread(DataEntry entry, Builder builder, Object addressSpace) throws IOException, MemoryAccessException {
		_file.seek(entry.offset);
		int signalNumber = _file.readInt(); //  signalNumber
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
	    
	    long utimeSec   = _file.readElfWord(); // utime_sec
	    long utimeUSec  = _file.readElfWord(); // utime_usec
	    long stimeSec   = _file.readElfWord(); // stime_sec
	    long stimeUSec  = _file.readElfWord(); // stime_usec
	    _file.readElfWord(); // Ignore cutime_sec
	    _file.readElfWord(); // Ignore cutime_usec
	    _file.readElfWord(); // Ignore cstime_sec
	    _file.readElfWord(); // Ignore cstime_usec
	    
	    Map registers = _file.readRegisters(builder, addressSpace);
	    Properties properties = new Properties();
	    properties.setProperty("Thread user time secs", Long.toString(utimeSec));
	    properties.setProperty("Thread user time usecs", Long.toString(utimeUSec));
	    properties.setProperty("Thread sys time secs", Long.toString(stimeSec));
	    properties.setProperty("Thread sys time usecs", Long.toString(stimeUSec));
	    
	    return buildThread(builder, addressSpace, String.valueOf(pid), registers, properties, signalNumber);
	}

	private Object buildThread(Builder builder, Object addressSpace, String pid, Map registers, Properties properties, int signalNumber) throws MemoryAccessException, IOException
	{
		List frames = new ArrayList();
		List sections = new ArrayList();
		long stackPointer = _file.getStackPointerFrom(registers).asAddress();
		long basePointer = _file.getBasePointerFrom(registers).asAddress();
		long instructionPointer = _file.getInstructionPointerFrom(registers).asAddress();
		long previousBasePointer = 0;
		
		if (0 == instructionPointer || false == isValidAddressInProcess(instructionPointer))
			instructionPointer = _file.getLinkRegisterFrom(registers).asAddress();
		
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
				//CMVC 161796
				//catch the IO exception, give up trying to extract the frames and attempt to carry on parsing the core file
				
				//CMVC 164739
				//Keep going instead.
				frames.add(builder.buildCorruptData(addressSpace, "Linux ELF core dump corruption "+
						"detected during native stack frame walk", basePointer));
		
			}
		}
		return builder.buildThread(pid, registersAsList(builder, registers).iterator(),
								   sections.iterator(), frames.iterator(), properties, signalNumber);
	}

	private List registersAsList(Builder builder, Map registers) {
		List list = new ArrayList();
		for (Iterator iter = registers.entrySet().iterator(); iter.hasNext();) {
			Map.Entry entry = (Map.Entry) iter.next();
			Address value = (Address) entry.getValue();
			list.add(builder.buildRegister((String) entry.getKey(), value.asNumber()));
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
		for (Iterator iter = _file.programHeaderEntries(); iter.hasNext();) {
			ProgramHeaderEntry entry = (ProgramHeaderEntry) iter.next();
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
		file.seek(0);
		byte[] signature = new byte[EI_NIDENT];
		file.readFully(signature);
		if (-1 != new String(signature).toLowerCase().indexOf("elf")) {
			DumpReader reader = readerForEndianess(signature[5], signature[4], file);
			ElfFile result = fileForClass(signature[4], reader);
			if (result.getClass().isInstance(_file))
				return result;
		}
		return null;
	}

	private static DumpReader readerForEndianess(byte endianess, byte clazz, ImageInputStream stream) throws IOException
	{
		boolean is64Bit = (ELFCLASS64 == clazz);
		
		if (!is64Bit && (ELFCLASS32 != clazz)) {
			throw new IOException("Unexpected class flag " + clazz + " detected in ELF file.");
		}		
		if (ELFDATA2LSB == endianess) {
			return new LittleEndianDumpReader(stream, is64Bit);
		} else if (ELFDATA2MSB == endianess) {
			return new DumpReader(stream, is64Bit);
		} else {
			throw new IOException("Unknown endianess flag " + endianess + " in ELF core file");
		}
	}

	private static ElfFile fileForClass(byte clazz, DumpReader reader) throws IOException {
		if (ELFCLASS32 == clazz)
			return new Elf32File(reader);
		else if (ELFCLASS64 == clazz)
			return new Elf64File(reader);
		else
			throw new IOException("Unexpected class flag " + clazz + " detected in ELF file.");
	}

	/* (non-Javadoc)
	 * @see com.ibm.jvm.j9.dump.systemdump.Dump#extract(com.ibm.jvm.j9.dump.systemdump.Builder)
	 */
	public void extract(Builder builder) {
		// System.err.println("NewElfDump.extract() entered");
		
		try {
			Object addressSpace = builder.buildAddressSpace("ELF Address Space", 0);
			List threads = new ArrayList();
			for (Iterator iter = _file.threadEntries(); iter.hasNext();) {
				DataEntry entry = (DataEntry) iter.next();
				threads.add(readThread(entry, builder, addressSpace));
			}
			List processes = new ArrayList();
			for (Iterator iter = _file.processEntries(); iter.hasNext();) {
				DataEntry entry = (DataEntry) iter.next();
				processes.add(readProcess(entry, builder, addressSpace, threads));
			}
			for (Iterator iter = _file.auxiliaryVectorEntries(); iter.hasNext();) {
				DataEntry entry = (DataEntry) iter.next();
				readAuxiliaryVector(entry);
			}
			builder.setOSType("ELF");
			builder.setCPUType(_file._arch.toString());
			builder.setCPUSubType(readStringAt(_platformIdAddress));
		} catch (IOException e) {
			// TODO throw exception or notify builder?
		} catch (MemoryAccessException e) {
			// TODO throw exception or notify builder?
		} catch (CorruptCoreException cce) {
			// TODO throw exception or notify builder?
		}
		
		// System.err.println("NewElfDump.extract() exited");
	}

	private void readAuxiliaryVector(DataEntry entry) throws IOException {
		_file.seek(entry.offset);
		if (0 != entry.size) {
			long type = _file.readElfWord();
			while (AT_NULL != type) {
				if (AT_PLATFORM == type)
					_platformIdAddress = _file.readElfWord();
				else if (AT_ENTRY == type)
					_file.readElfWord();
				else if (AT_HWCAP == type)
					_file.readElfWord(); // TODO extract features in some useful fashion
				else
					_file.readElfWord(); // Ignore
				type = _file.readElfWord();
			}
		}
	}

	private String readStringAt(long address) throws IOException
	{
		String toReturn = null;
		
		if (isValidAddress(address)) {
			StringBuffer buf = new StringBuffer();
			seekToAddress(address);
			byte b = coreReadByte();
			for (long i = 1; 0 != b; i++) {
				buf.append(new String(new byte[] {b}, "ASCII"));
				b = coreReadByte();
			}
			toReturn = buf.toString();
		}
		return toReturn;
	}

	public static boolean isSupportedDump(ImageInputStream stream) throws IOException {
		stream.seek(0);
		byte[] signature = new byte[EI_NIDENT];
		stream.readFully(signature);
		return -1 != new String(signature).toLowerCase().indexOf("elf");
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

	public Iterator getAdditionalFileNames() {
		return _additionalFileNames.iterator();
	}

	protected MemoryRange[] getMemoryRangesAsArray() {
		return (MemoryRange[])_memoryRanges.toArray(new MemoryRange[_memoryRanges.size()]);
	}

	protected boolean isLittleEndian()
	{
		return _isLittleEndian;
	}

	protected boolean is64Bit()
	{
		return _is64Bit;
	}
}
