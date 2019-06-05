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
import java.nio.charset.StandardCharsets;

import javax.imageio.stream.ImageInputStream;

public class LoadCommand
{

	public static final int LC_REQ_DYLD = 0x80000000;

	// load command constants
	public static final int LC_SEGMENT = 0x1;
	public static final int LC_SYMTAB = 0x2;
	public static final int LC_SYMSEG = 0x3;
	public static final int LC_THREAD = 0x4;
	public static final int LC_UNIXTHREAD = 0x5;
	public static final int LC_LOADFVMLIB = 0x6;
	public static final int LC_IDFVMLIB = 0x7;
	public static final int LC_IDENT = 0x8;
	public static final int LC_FVMFILE = 0x9;
	public static final int LC_PREPAGE = 0xa;
	public static final int LC_DYSYMTAB = 0xb;
	public static final int LC_LOAD_DYLIB = 0xc;
	public static final int LC_ID_DYLIB = 0xd;
	public static final int LC_LOAD_DYLINKER = 0xe;
	public static final int LC_ID_DYLINKER = 0xf;
	public static final int LC_PREBOUND_DYLIB = 0x10;
	public static final int LC_ROUTINES = 0x11;
	public static final int LC_SUB_FRAMEWORK = 0x12;
	public static final int LC_SUB_UMBRELLA = 0x13;
	public static final int LC_SUB_CLIENT = 0x14;
	public static final int LC_SUB_LIBRARY = 0x15;
	public static final int LC_TWOLEVEL_HINTS = 0x16;
	public static final int LC_PREBIND_CKSUM = 0x17;
	public static final int LC_LOAD_WEAK_DYLIB = (0x18 | LC_REQ_DYLD);
	public static final int LC_SEGMENT_64 = 0x19;
	public static final int LC_ROUTINES_64 = 0x1a;
	public static final int LC_UUID = 0x1b;
	public static final int LC_RPATH = (0x1c | LC_REQ_DYLD);
	public static final int LC_CODE_SIGNATURE = 0x1d;
	public static final int LC_SEGMENT_SPLIT_INFO = 0x1e;
	public static final int LC_REEXPORT_DYLIB = (0x1f | LC_REQ_DYLD);
	public static final int LC_LAZY_LOAD_DYLIB = 0x20;
	public static final int LC_ENCRYPTION_INFO = 0x21;
	public static final int LC_DYLD_INFO = 0x22;
	public static final int LC_DYLD_INFO_ONLY = (0x22|LC_REQ_DYLD);
	public static final int LC_LOAD_UPWARD_DYLIB = (0x23 | LC_REQ_DYLD);
	public static final int LC_VERSION_MIN_MACOSX = 0x24;
	public static final int LC_VERSION_MIN_IPHONEOS = 0x25;
	public static final int LC_FUNCTION_STARTS = 0x26;
	public static final int LC_DYLD_ENVIRONMENT = 0x27;
	public static final int LC_MAIN = (0x28|LC_REQ_DYLD);
	public static final int LC_DATA_IN_CODE = 0x29;
	public static final int LC_SOURCE_VERSION = 0x2A;
	public static final int LC_DYLIB_CODE_SIGN_DRS = 0x2B;
	public static final int LC_ENCRYPTION_INFO_64 = 0x2C;
	public static final int LC_LINKER_OPTION = 0x2D;
	public static final int LC_LINKER_OPTIMIZATION_HINT = 0x2E;
	public static final int LC_VERSION_MIN_TVOS = 0x2F;
	public static final int LC_VERSION_MIN_WATCHOS = 0x30;
	public static final int LC_NOTE = 0x31;
	public static final int LC_BUILD_VERSION = 0x32;

	public class LoadCommandString
	{
		long offset;
		String value;

		public LoadCommandString() {}

		public void readLcString(ImageInputStream stream) throws IOException {
			offset = stream.readInt();
			long currentOffset = stream.getStreamPosition();
			stream.seek(absoluteOffset + offset);
			int stringSize = (int) (cmdSize - offset);
			byte stringBytes[] = new byte[stringSize];
			stream.readFully(stringBytes);
			value = getStringFromAsciiChars(stringBytes);
			stream.seek(currentOffset);
		}
	}

	public static String getStringFromAsciiChars(byte[] chars) throws UnsupportedEncodingException
	{
		return getStringFromAsciiChars(chars, 0);
	}

	public static String getStringFromAsciiChars(byte[] chars, int start)
	{
		if ((start < 0) || (start >= chars.length)) {
			return null;
		}
		int count = 0;
		for (;count < chars.length; count++) {
			if (0 == chars[start + count]) {
				break;
			}
		}
		return new String(chars, start, count, StandardCharsets.US_ASCII);
	}


	public int cmdType;
	public long cmdSize;
	public long absoluteOffset; // offset from start of core file stream
	public long segmentOffset; // offset from the start of the current memory segment

	protected LoadCommand() {}

	public LoadCommand(int type, long size, long offset)
	{
		cmdType = type;
		cmdSize = size;
		this.absoluteOffset = offset;
	}

	public LoadCommand readCommand(ImageInputStream stream, long streamSegmentOffset) throws IOException
	{
		absoluteOffset = stream.getStreamPosition();
		segmentOffset = streamSegmentOffset;
		cmdType = stream.readInt();
		cmdSize = stream.readUnsignedInt();
		return this;
	}

	public static LoadCommand readFullCommand(ImageInputStream stream, long streamOffset, long segmentOffset) throws IOException
	{
		LoadCommand command;
		stream.seek(streamOffset);
		int cmdType = stream.readInt();
		switch(cmdType) {
			case LC_SEGMENT_64:
				command = new SegmentCommand64();
				break;
			case LC_LOAD_DYLIB:
			case LC_LOAD_WEAK_DYLIB:
			case LC_ID_DYLIB:
			case LC_REEXPORT_DYLIB:
				command = new DylibCommand();
				break;
			case LC_SUB_FRAMEWORK:
			case LC_SUB_CLIENT:
			case LC_SUB_LIBRARY:
			case LC_SUB_UMBRELLA:
				command = new SubCommand();
				break;
			case LC_PREBOUND_DYLIB:
				command = new PreboundDylibCommand();
				break;
			case LC_LOAD_DYLINKER:
			case LC_ID_DYLINKER:
			case LC_DYLD_ENVIRONMENT:
				command = new DylinkerCommand();
				break;
			case LC_THREAD:
			case LC_UNIXTHREAD:
				command = new ThreadCommand();
				break;
			case LC_ROUTINES_64:
				command = new RoutinesCommand64();
				break;
			case LC_SYMTAB:
				command = new SymtabCommand();
				break;
			case LC_DYSYMTAB:
				command = new DSymtabCommand();
				break;
			case LC_TWOLEVEL_HINTS:
				command = new TwoLevelHintsCommand();
				break;
			case LC_PREBIND_CKSUM:
				command = new PrebindChecksumCommand();
				break;
			case LC_UUID:
				command = new UuidCommand();
				break;
			case LC_RPATH:
				command = new RpathCommand();
				break;
			case LC_CODE_SIGNATURE:
			case LC_SEGMENT_SPLIT_INFO:
			case LC_FUNCTION_STARTS:
			case LC_DATA_IN_CODE:
			case LC_DYLIB_CODE_SIGN_DRS:
			case LC_LINKER_OPTIMIZATION_HINT:
				command = new LinkeditDataCommand();
				break;
			case LC_ENCRYPTION_INFO_64:
				command = new EncryptionCommand64();
				break;
			case LC_VERSION_MIN_MACOSX:
				command = new VersionMinCommand();
				break;
			case LC_BUILD_VERSION:
				command = new BuildVersionCommand();
				break;
			case LC_DYLD_INFO:
			case LC_DYLD_INFO_ONLY:
				command = new DyldInfoCommand();
				break;
			case LC_LINKER_OPTION:
				command = new LinkerOptionCommand();
				break;
			case LC_MAIN:
				command = new EntryPointCommand();
				break;
			case LC_SOURCE_VERSION:
				command = new SourceVersionCommand();
				break;
			case LC_NOTE:
				command = new NoteCommand();
				break;
			default:
				command = new LoadCommand();
		}
		stream.seek(streamOffset);
		command.readCommand(stream, segmentOffset);
		return command;
	}
}
