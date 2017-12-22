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

/**
 * Here, for reference, is the section header table for one of our libraries, libj9jit24.so
 * Note a couple of things about it:
 * There is a null entry at the start
 * The addr is not always the same as the off: that is the sections will be 
 * more spaced out in memory 
 * The readelf commands shows how the sections are mapped to segments:
    Section to Segment mapping:
    Segment Sections...
     00     .hash .dynsym .dynstr .gnu.version .gnu.version_r .rel.dyn .rel.plt .init .plt .text .fini .rodata .eh_frame_hdr .eh_frame
     01     .ctors .dtors .jcr .data.rel.ro .dynamic .got .got.plt .data .bss
     02     .dynamic
     03     .eh_frame_hdr
 * Note that some of the later entries are not mapped to segments - although they are on 
 * disk it is not safe to assume that they are in memory
 * Some of the later entries have an addr of 0 - these are the ones that may not
 * be mapped. 
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            00000000 000000 000000 00      0   0  0
  [ 1] .hash             HASH            000000d4 0000d4 000424 04   A  2   0  4
  [ 2] .dynsym           DYNSYM          000004f8 0004f8 000840 10   A  3  13  4
  [ 3] .dynstr           STRTAB          00000d38 000d38 000772 00   A  0   0  1
  [ 4] .gnu.version      VERSYM          000014aa 0014aa 000108 02   A  2   0  2
  [ 5] .gnu.version_r    VERNEED         000015b4 0015b4 000060 00   A  3   2  4
  [ 6] .rel.dyn          REL             00001614 001614 02e4e0 08   A  2   0  4
  [ 7] .rel.plt          REL             0002faf4 02faf4 000328 08   A  2   9  4
  [ 8] .init             PROGBITS        0002fe1c 02fe1c 000017 00  AX  0   0  4
  [ 9] .plt              PROGBITS        0002fe34 02fe34 000660 04  AX  0   0  4
  [10] .text             PROGBITS        000304a0 0304a0 56df04 00  AX  0   0 16
  [11] .fini             PROGBITS        0059e3a4 59e3a4 00001c 00  AX  0   0  4
  [12] .rodata           PROGBITS        0059e3c0 59e3c0 061244 00   A  0   0 32
  [13] .eh_frame_hdr     PROGBITS        005ff604 5ff604 00002c 00   A  0   0  4
  [14] .eh_frame         PROGBITS        005ff630 5ff630 00009c 00   A  0   0  4
  [15] .ctors            PROGBITS        006006cc 5ff6cc 00001c 00  WA  0   0  4
  [16] .dtors            PROGBITS        006006e8 5ff6e8 000008 00  WA  0   0  4
  [17] .jcr              PROGBITS        006006f0 5ff6f0 000004 00  WA  0   0  4
  [18] .data.rel.ro      PROGBITS        00600700 5ff700 0141a0 00  WA  0   0 32
  [19] .dynamic          DYNAMIC         006148a0 6138a0 000100 08  WA  3   0  4
  [20] .got              PROGBITS        006149a0 6139a0 00128c 04  WA  0   0  4
  [21] .got.plt          PROGBITS        00615c2c 614c2c 0001a0 04  WA  0   0  4
  [22] .data             PROGBITS        00615de0 614de0 014d74 00  WA  0   0 32
  [23] .bss              NOBITS          0062ab60 629b54 002e24 00  WA  0   0 32
  [24] .comment          PROGBITS        00000000 629b54 002f58 00      0   0  1
  [25] .note             NOTE            00000000 62caac 0000b8 00      0   0  1
  [26] .gnu_debuglink    PROGBITS        00000000 62cb64 000018 00      0   0  1
  [27] .shstrtab         STRTAB          00000000 62cb7c 0000f1 00      0   0  1
  [28] .symtab           SYMTAB          00000000 62d120 04b800 10     29 19209  4
  [29] .strtab           STRTAB          00000000 678920 0d7504 00      0   0  1
 */

class SectionHeaderEntry
{
	static final int SHT_SYMTAB = 2; // Symbol table in this section
	static final int SHT_STRTAB = 3; // String table in this section
	static final int SHT_NOBITS	= 8; 
	static final int SHT_DYNSYM = 11; // Symbol table in this section
	// private static final int SHT_DYNSTR = 99; // String table in this section

	static final int SHF_ALLOC = 0x2; // Section occupies memory during process execution.

	long _name; // index into section header string table
	long _type;
	final long _flags;
	final long address;
	final long offset;
	final long size;
	final long link;

	// private long _info;

	SectionHeaderEntry(long name, long type, long flags, long address,
			long offset, long size, long link, long info)
	{
		this._name = name;
		this._type = type;
		this._flags = flags;
		this.address = address;
		this.offset = offset;
		this.size = size;
		this.link = link;
		// _info = info;
	}

	boolean isSymbolTable()
	{
		return SHT_SYMTAB == _type || SHT_DYNSYM == _type;
	}

	public boolean isStringTable()
	{
		return SHT_STRTAB == _type;
	}
	
	public boolean isNoBits() {
		return SHT_NOBITS == _type;
	}

	public boolean isAllocated() {
		return (_flags & SHF_ALLOC) == SHF_ALLOC;
	}
	
	public String toString() {
		return String.format("Name %d, Type %d, Flags 0x%x, Address 0x%x, Offset 0x%x, Size 0x%x", _name, _type, _flags, address, offset, size);
	}
}
