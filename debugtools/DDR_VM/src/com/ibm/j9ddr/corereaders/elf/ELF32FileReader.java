/*******************************************************************************
 * Copyright (c) 2004, 2014 IBM Corp. and others
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

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import javax.imageio.stream.ImageInputStream;

import com.ibm.j9ddr.corereaders.InvalidDumpFormatException;

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

public class ELF32FileReader extends ELFFileReader
{

	public ELF32FileReader(File f, ByteOrder byteOrder, long offset) throws FileNotFoundException, IOException, InvalidDumpFormatException
	{
		super(f, byteOrder, offset);
	}

	public ELF32FileReader(ImageInputStream in, long offset) throws IOException, InvalidDumpFormatException
	{
		super(in, offset);
	}
	
	public boolean validDump(byte[] data, long filesize)
	{
		return (0x7F == data[0] && 0x45 == data[1] && 0x4C == data[2]
				&& 0x46 == data[3] && ELFCLASS32 == data[4]);
	}

	protected ProgramHeaderEntry readProgramHeaderEntry() throws IOException
	{
		int type = is.readInt();
		long fileOffset = unsigned(is.readInt());
		long virtualAddress = unsigned(is.readInt());
		long physicalAddress = unsigned(is.readInt());
		long fileSize = unsigned(is.readInt());
		long memorySize = unsigned(is.readInt());
		int flags = is.readInt();
		long alignment = unsigned(is.readInt());
		return new ProgramHeaderEntry(type, fileOffset, fileSize,
				virtualAddress, physicalAddress, memorySize, flags, alignment,
				this);
	}

	private long unsigned(int i)
	{
		return i & 0xffffffffL;
	}

	protected long padToWordBoundary(long l)
	{
		return ((l + 3) / 4) * 4;
	}

	protected long readElfWord() throws IOException
	{
		return is.readInt() & 0xffffffffL;
	}

	protected Address readElfWordAsAddress() throws IOException
	{
		return new Address32(is.readInt());
	}

	protected int addressSizeBits()
	{
		return 32;
	}

	protected List<ELFSymbol> readSymbolsAt(SectionHeaderEntry entry)
			throws IOException {
		// The size of an entry is 16 bytes, entry.size must be a multiple of that
		// or we know this SectionHeaderEntry is corrupt.
		if (0 != entry.size % 16l) {
			return Collections.emptyList();
		}
		seek(entry.offset);
		long count = entry.size / 16l;
		// maximum 8096 symbols, for protection against excessive or infinite
		// loops in corrupt dumps, catch corrupt negative counts with abs.
		count = Math.min(Math.abs(count), 8096);
		List<ELFSymbol> symbols = new ArrayList<ELFSymbol>((int)count);
		for (long i = 0; i < count; i++) {
			long name = is.readInt() & 0xffffffffL;
			long value = is.readInt() & 0xffffffffL;
			long size = is.readInt() & 0xffffffffL;
			byte info = is.readByte();
			byte other = is.readByte();
			int sectionIndex = is.readShort() & 0xffff;
			symbols.add(new ELFSymbol(name, value, size, info, other,
					sectionIndex));
		}
		return symbols;
	}
}
