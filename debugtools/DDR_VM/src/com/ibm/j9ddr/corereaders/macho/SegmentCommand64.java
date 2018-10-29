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
/*******************************************************************************
 * Portions Copyright (c) 1999-2003 Apple Computer, Inc. All Rights
 * Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *******************************************************************************/
package com.ibm.j9ddr.corereaders.macho;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.List;

import javax.imageio.stream.ImageInputStream;

public class SegmentCommand64 extends LoadCommand
{

	public static final int VM_PROT_READ = 0x1;
	public static final int VM_PROT_WRITE = 0x2;
	public static final int VM_PROT_EXECUTE = 0x4;

	public static class Section64
	{
		public static final int SECTION_TYPE = 0x000000ff;
		public static final int SECTION_ATTRIBUTES = 0xffffff00;
		// section types
		public static final int S_REGULAR = 0x0;
		public static final int S_ZEROFILL = 0x1;
		public static final int S_CSTRING_LITERALS = 0x2;
		public static final int S_4BYTE_LITERALS = 0x3;
		public static final int S_8BYTE_LITERALS = 0x4;
		public static final int S_LITERAL_POINTERS = 0x5;
		public static final int S_NON_LAZY_SYMBOL_POINTERS = 0x6;
		public static final int S_LAZY_SYMBOL_POINTERS = 0x7;
		public static final int S_SYMBOL_STUBS = 0x8;
		public static final int S_MOD_INIT_FUNC_POINTERS = 0x9;
		public static final int S_MOD_TERM_FUNC_POINTERS = 0xa;
		public static final int S_COALESCED = 0xb;
		public static final int S_GB_ZEROFILL = 0xc;
		public static final int S_INTERPOSING = 0xd;
		public static final int S_16BYTE_LITERALS = 0xe;
		public static final int S_DTRACE_DOF = 0xf;
		public static final int S_LAZY_DYLIB_SYMBOL_POINTERS = 0x10;
		public static final int S_THREAD_LOCAL_REGULAR = 0x11;
		public static final int S_THREAD_LOCAL_ZEROFILL = 0x12;
		public static final int S_THREAD_LOCAL_VARIABLES = 0x13;
		public static final int S_THREAD_LOCAL_VARIABLE_POINTERS = 0x14;
		public static final int S_THREAD_LOCAL_INIT_FUNCTION_POINTERS = 0x15;

		// other section attribute flags
		public static final int SECTION_ATTRIBUTES_USR = 0xff000000;
		public static final int S_ATTR_PURE_INSTRUCTIONS = 0x80000000;
		public static final int S_ATTR_NO_TOC = 0x40000000;
		public static final int S_ATTR_STRIP_STATIC_SYMS = 0x20000000;
		public static final int S_ATTR_NO_DEAD_STRIP =0x10000000;
		public static final int S_ATTR_LIVE_SUPPORT =0x08000000;
		public static final int S_ATTR_SELF_MODIFYING_CODE = 0x04000000;
		public static final int	S_ATTR_DEBUG = 0x02000000;
		public static final int SECTION_ATTRIBUTES_SYS = 0x00ffff00;
		public static final int S_ATTR_SOME_INSTRUCTIONS = 0x00000400;
		public static final int S_ATTR_EXT_RELOC = 0x00000200;
		public static final int S_ATTR_LOC_RELOC = 0x00000100;

		public byte[] sectionName;
		public byte[] segmentName;
		public long address;
		public long size;
		public int fileOffset;
		public int alignment;
		public int relocOffset;
		public int numReloc;
		public int flags;
		public int reserved1;
		public int reserved2;
		public int reserved3;
		
		public Section64()
		{
			sectionName = new byte[16];
			segmentName = new byte[16];
		}

		public String getSectionName() throws UnsupportedEncodingException
		{
			return getStringFromAsciiChars(sectionName);
		}

		public String getSegmentName() throws UnsupportedEncodingException
		{
			return getStringFromAsciiChars(segmentName);
		}
	}

	byte[] segmentName; //16 bytes max
	long vmaddr;
	long vmsize;
	long fileOffset;
	long fileSize;
	int maxProt;
	int initialProt;
	int numSections;
	int flags;
	List<Section64> sections;

	public SegmentCommand64()
	{
		segmentName = new byte[16];
	}

	public SegmentCommand64 readCommand(ImageInputStream stream, long streamSegmentOffset) throws IOException
	{
		super.readCommand(stream, streamSegmentOffset);
		stream.readFully(segmentName);
		vmaddr = stream.readLong();
		vmsize = stream.readLong();
		fileOffset = stream.readLong();
		fileSize = stream.readLong();
		maxProt = stream.readInt();
		initialProt = stream.readInt();
		numSections = stream.readInt();
		flags = stream.readInt();
		sections = new ArrayList<Section64>(numSections);

		for (int i = 0; i < numSections; i++) {
			Section64 section = new Section64();
			stream.readFully(section.sectionName);
			stream.readFully(section.segmentName);
			section.address = stream.readLong();
			section.size = stream.readLong();
			section.fileOffset = stream.readInt();
			section.alignment = stream.readInt();
			section.relocOffset = stream.readInt();
			section.numReloc = stream.readInt();
			section.flags = stream.readInt();
			section.reserved1 = stream.readInt();
			section.reserved2 = stream.readInt();
			section.reserved3 = stream.readInt();
			sections.add(section);
		}
		return this;
}

	public String getName() throws UnsupportedEncodingException
	{
		return getStringFromAsciiChars(segmentName);
	}

}
