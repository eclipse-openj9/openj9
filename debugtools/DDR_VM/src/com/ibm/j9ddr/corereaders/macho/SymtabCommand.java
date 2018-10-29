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
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

import javax.imageio.stream.ImageInputStream;

public class SymtabCommand extends LoadCommand
{
	int symtabOffset;
	int numSymbols;
	int stringsOffset;
	int stringsSize;
	List<SymtabEntry64> entries;
	byte stringsBytes[];
	List<String> strings;

	public SymtabCommand() {}

	public SymtabCommand readCommand(ImageInputStream stream, long streamSegmentOffset) throws IOException
	{
		super.readCommand(stream, streamSegmentOffset);
		symtabOffset = stream.readInt();
		numSymbols = stream.readInt();
		stringsOffset = stream.readInt();
		stringsSize = stream.readInt();
		stringsBytes = null;
		return this;
	}

	public void readSymbolTable(ImageInputStream stream) throws IOException
	{
		stream.seek(symtabOffset + segmentOffset);
		entries = new ArrayList<>(numSymbols);
		for (int i = 0; i < numSymbols; i++) {
			SymtabEntry64 entry = new SymtabEntry64();
			entry.strIndex = stream.readInt();
			entry.nType = stream.readByte();
			entry.nSect = stream.readByte();
			entry.nDesc = stream.readShort();
			entry.nValue = stream.readLong();
			entries.add(entry);
		}

		stream.seek(stringsOffset + segmentOffset);
		strings = new ArrayList<>();
		stringsBytes = new byte[stringsSize];
		stream.readFully(stringsBytes);
		for (int i = 0; i < stringsBytes.length;) {
			int j = i;
			while (j < stringsBytes.length && stringsBytes[j] != 0) {
				j++;
			}
			String stringEntry = new String(stringsBytes, i, j - i, StandardCharsets.US_ASCII);
			strings.add(stringEntry);
			i = j + 1;
		}
	}

	public static class SymtabEntry64
	{
		int strIndex;
		byte nType;
		byte nSect;
		short nDesc;
		long nValue;

		//type from getType()
		public static final int	N_UNDF = 0x0;		/* undefined, n_sect == NO_SECT */
		public static final int	N_ABS = 0x2;		/* absolute, n_sect == NO_SECT */
		public static final int	N_SECT = 0xe;		/* defined in section number n_sect */
		public static final int	N_PBUD = 0xc;		/* prebound undefined (defined in a dylib) */
		public static final int N_INDR = 0xa;		/* indirect */

		//reference type to support lazy binding of undefined symbols
		//from getReferenceType()
		public static final int REFERENCE_FLAG_UNDEFINED_NON_LAZY = 0;
		public static final int REFERENCE_FLAG_UNDEFINED_LAZY = 1;
		public static final int REFERENCE_FLAG_DEFINED = 2;
		public static final int REFERENCE_FLAG_PRIVATE_DEFINED = 3;
		public static final int REFERENCE_FLAG_PRIVATE_UNDEFINED_NON_LAZY = 4;
		public static final int REFERENCE_FLAG_PRIVATE_UNDEFINED_LAZY = 5;

		public boolean isStab()
		{
			return 0 != (nType & (byte)0xe0);
		}

		public byte getStab()
		{
			return (byte)(nType & (byte)0xe0);
		}

		public boolean isPrivateExternal()
		{
			return 0 != (nType & (byte)0x10);
		}

		public byte getType()
		{
			return (byte)(nType & (byte)0x0e);
		}

		public boolean isExternal()
		{
			return 0 != (nType & (byte)0x01);
		}

		public int getCommonAlignment()
		{
			return (nDesc >>> 8) & 0x0f;
		}

		public int getReferenceType()
		{
			return nDesc & 0x7;
		}

	}
}
