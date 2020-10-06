/*******************************************************************************
 * Copyright (c) 2009, 2020 IBM Corp. and others
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
import java.io.IOException;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import javax.imageio.stream.ImageInputStream;

import com.ibm.j9ddr.corereaders.InvalidDumpFormatException;

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

public class ELF64FileReader extends ELFFileReader {

	public ELF64FileReader(File file, ByteOrder byteOrder)
			throws IOException, InvalidDumpFormatException {
		super(file, byteOrder);
	}

	public ELF64FileReader(ImageInputStream in, long offset, long limit)
			throws IOException, InvalidDumpFormatException {
		super(in, offset, limit);
	}

	public boolean validDump(byte[] data, long filesize) {
		return (0x7F == data[0] && 0x45 == data[1] && 0x4C == data[2] && 0x46 == data[3] && ELFCLASS64 == data[4]);
	}

	@Override
	protected ProgramHeaderEntry readProgramHeaderEntry() throws IOException {
		int type = is.readInt();
		int flags = is.readInt();
		long fileOffset = is.readLong();
		long virtualAddress = is.readLong();
		long physicalAddress = is.readLong();
		long fileSize = is.readLong();
		long memorySize = is.readLong();
		long alignment = is.readLong();
		return new ProgramHeaderEntry(type, fileOffset, fileSize, virtualAddress, physicalAddress, memorySize, flags,
				alignment, this);
	}

	@Override
	protected long padToWordBoundary(long l) {
		return (l + 7) & ~7;
	}

	@Override
	protected long readElfWord() throws IOException {
		return is.readLong();
	}

	@Override
	protected Address readElfWordAsAddress() throws IOException {
		return new Address64(is.readLong());
	}

	@Override
	protected int addressSizeBits() {
		return 64;
	}

	protected List<ELFSymbol> readSymbolsAt(SectionHeaderEntry entry) throws IOException {
		// The size of an entry is 24 bytes, entry.size must be a multiple of that
		// or we know this SectionHeaderEntry is corrupt.
		if (0 != entry.size % 24L) {
			return Collections.emptyList();
		}
		seek(entry.offset);
		// maximum 8096 symbols, for protection against excessive or infinite
		// loops in corrupt dumps, catch corrupt negative counts with abs.
		int count = (int) Math.min(Math.abs(entry.size / 24L), 8096);
		List<ELFSymbol> symbols = new ArrayList<>(count);
		for (long i = 0; i < count; i++) {
			long name = is.readInt() & 0xffffffffL;
			byte info = is.readByte();
			byte other = is.readByte();
			int sectionIndex = is.readShort() & 0xffff;
			long value = is.readLong();
			long size = is.readLong();
			symbols.add(new ELFSymbol(name, value, size, info, other, sectionIndex));
		}
		return symbols;
	}

}
