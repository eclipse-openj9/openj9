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
package com.ibm.dtfj.binaryreaders;

import java.io.IOException;

import java.util.Date;
import java.util.LinkedList;
import java.util.List;
import java.util.Properties;

import com.ibm.dtfj.corereaders.Builder;
import com.ibm.dtfj.corereaders.ClosingFileReader;

/**
 * @author jmdisher
 *
 */
public class XCOFFReader
{
	private static final short XCOFF_MAGIC_NUMBER_AIX32 = 0x01DF;
	private static final short XCOFF_MAGIC_NUMBER_AIX64_PRE43 = 0x01EF;
	private static final short XCOFF_MAGIC_NUMBER_AIX64_POST51 = 0x01F7;
	private ClosingFileReader _backing;
	private long _startOffset;
	private long _size;
	private boolean _is64Bit;
	
	private long _textSegmentOffset = -1;
	private long _textSegmentSize = -1;
	private long _sybolTableOffset = -1;
	
	//values held purely for module properties
	/**
	 * The creation time and date of the module in milliseconds since the epoch
	 */
	private long _timeAndDate = 0;
	private short _flags = 0;
	
	public XCOFFReader(ClosingFileReader backing, long offsetIntoFile, long size) throws IOException
	{
		_startOffset = offsetIntoFile;
		_size = size;
		_backing = backing;
		
		//by this point, the basic support for reading the file complete so on to looking into the file
		seekFileRelative(0);
		short magic = _backing.readShort();
		if (!((XCOFF_MAGIC_NUMBER_AIX32 == magic) || (XCOFF_MAGIC_NUMBER_AIX64_PRE43 == magic) || (XCOFF_MAGIC_NUMBER_AIX64_POST51 == magic))) {
			throw new IllegalArgumentException();
		}
		_is64Bit = ((XCOFF_MAGIC_NUMBER_AIX64_PRE43 == magic) || (XCOFF_MAGIC_NUMBER_AIX64_POST51 == magic));
		short numberOfSections = _backing.readShort();
		//time and date in XCOFF is _seconds_ since epoch while in Java it is _milliseconds_ since epoch
		_timeAndDate = 1000L * (0xFFFFFFFFL & _backing.readInt());
		//by this point, the file pointer is at 8 so get the offset to the symbol table
		_sybolTableOffset = (_is64Bit ? _backing.readLong() : (0xFFFFFFFFL & _backing.readInt()));
		seekFileRelative(16);
		short optionalHeaderSize = _backing.readShort();
		_flags = _backing.readShort();
		long nextSectionAddress = 20 + optionalHeaderSize;
		for(int x =0 ;x < numberOfSections; x++) {
			seekFileRelative(nextSectionAddress);
			byte[] buffer = new byte[8];
			_backing.readFully(buffer);
			String sectionName = new String(buffer);
			seekFileRelative(nextSectionAddress + (_is64Bit ? 24 : 16));
			long sectionSize = (_is64Bit ? _backing.readLong() : _backing.readInt());
			long fileOffset = (_is64Bit ? _backing.readLong() : _backing.readInt());
			nextSectionAddress += (_is64Bit ? 72 : 40);
			if (sectionName.startsWith(".text")) {
				_textSegmentOffset = fileOffset + _startOffset;
				_textSegmentSize = sectionSize;
			}
		}
	}
	
	public XCOFFReader(ClosingFileReader backing) throws IOException
	{
		this(backing, 0, backing.length());
	}
	
	public long getTextSegmentOffset()
	{
		return _textSegmentOffset;
	}
	
	private void seekFileRelative(long offset) throws IOException
	{
		_backing.seek(_startOffset + offset);
	}

	public long getTextSegmentSize()
	{
		return _textSegmentSize;
	}
	
	private long _symbolTableOffset() throws IOException
	{
		return _sybolTableOffset;
	}
	
	private int _numberOfSymbols() throws IOException
	{
		seekFileRelative(_is64Bit ? 20 : 12);
		return _backing.readInt();
	}
	
	private String _stringFromArray(byte[] rawData, int start)
	{
		int end = start;
		while ((end < rawData.length) && ('\0' != rawData[end])) {
			end++;
		}
		return new String(rawData, start, end - start);
	}

	public ClosingFileReader underlyingFile()
	{
		return _backing;
	}
	
	public long baseFileOffset()
	{
		return _startOffset;
	}
	
	public long logicalSize()
	{
		return _size;
	}

	private static final short F_RELFLG = 0x0001;
	private static final short F_EXEC = 0x0002;
	private static final short F_LNNO = 0x0004;
	private static final short F_AR32W = 0x0200;
	private static final short F_DYNLOAD = 0x1000;
	private static final short F_SHROBJ = 0x2000;
	private static final short F_LOADONLY = 0x4000;
	
	public Properties moduleProperties()
	{
		Properties props = new Properties();
		props.put("Time and Date", (new Date(_timeAndDate)).toString());
		String flagRep = ((0 != (_flags & F_RELFLG)) ? "F_RELFLG " : "")
							+ ((0 != (_flags & F_EXEC)) ? "F_EXEC " : "")
							+ ((0 != (_flags & F_LNNO)) ? "F_LNNO " : "")
							+ ((0 != (_flags & F_AR32W)) ? "F_AR32W " : "")
							+ ((0 != (_flags & F_DYNLOAD)) ? "F_DYNLOAD " : "")
							+ ((0 != (_flags & F_SHROBJ)) ? "F_SHROBJ " : "")
							+ ((0 != (_flags & F_LOADONLY)) ? "F_LOADONLY " : "");
		props.put("Flags", flagRep);
		return props;
	}

	public List buildSymbols(Builder builder, Object addressSpace, long relocationBase)
	{
		LinkedList symbols = new LinkedList();
		
		try {
			long symbolTableOffset = _symbolTableOffset();
			int numberOfSymbols = _numberOfSymbols();

			if (0 != symbolTableOffset) {
				//read the strings first
				seekFileRelative(symbolTableOffset + (18 * numberOfSymbols));
				int stringTableLength = _backing.readInt();
				// "If a string table is not used, it may be omitted entirely, or a string table
				//  consisting of only the length field (containing a value of 0 or 4) may be used.
				//  A value of 4 is preferable."
				if (4 != stringTableLength && 0 != stringTableLength) {
					byte[] rawStringTable = new byte[stringTableLength-4];
					_backing.readFully(rawStringTable);
					
					//now read the symbol table
					seekFileRelative(symbolTableOffset);
					byte[] symbolTableEntry = new byte[18];
					int skipNext = 0;
					for (int x = 0; x < numberOfSymbols; x++) {
						_backing.readFully(symbolTableEntry);
						if (skipNext > 0) {
							skipNext--;
						} else {
							int stringTableOffset = 0;
							String symbolName = null;
							
							if (_is64Bit) {
								stringTableOffset = ((0xFF & symbolTableEntry[8]) << 24)
										| ((0xFF & symbolTableEntry[9]) << 16)
										| ((0xFF & symbolTableEntry[10]) << 8)
										| ((0xFF & symbolTableEntry[11]));
							} else {
								if ((0 == symbolTableEntry[0]) && (0 == symbolTableEntry[1])
										&& (0 == symbolTableEntry[2]) && (0 == symbolTableEntry[3])) {
									// string offset
									stringTableOffset = ((0xFF & symbolTableEntry[4]) << 24)
											| ((0xFF & symbolTableEntry[5]) << 16)
											| ((0xFF & symbolTableEntry[6]) << 8)
											| ((0xFF & symbolTableEntry[7]));
								} else {
									// literal
									symbolName = _stringFromArray(symbolTableEntry, 0);
								}
							}
							if ((null == symbolName) && (0 == (symbolTableEntry[16] & 0x80)) && (0 != stringTableOffset) && (stringTableOffset < (stringTableLength - 4))) {
								//read the string from the table
								symbolName = _stringFromArray(rawStringTable, stringTableOffset - 4);
							} else if ((0 == (symbolTableEntry[16] & 0x80)) && (stringTableOffset > (stringTableLength - 4))) {
								//XXX: this problem is known to happen when parsing the 64-bit /usr/lib/libiconv.a file.
								// Currently, the cause is unknown so this work-around is put in until we can find the real cause
								symbolName = "(string out of table bounds)";
							}
							if (null == symbolName) {
								symbolName = "";
							}
							long value = 0;
							if (_is64Bit) {
								value = ((0xFFL & symbolTableEntry[0]) << 56)
								//CMVC 161794 : changed to declare a long if shifting more than 32 bits
								| ((0xFFL & symbolTableEntry[1]) << 48)
								| ((0xFFL & symbolTableEntry[2]) << 40)
								| ((0xFFL & symbolTableEntry[3]) << 32)
								| ((0xFFL & symbolTableEntry[4]) << 24)
								| ((0xFF & symbolTableEntry[5]) << 16)
								| ((0xFF & symbolTableEntry[6]) << 8)
								| ((0xFF & symbolTableEntry[7]));
							} else {
								value = ((0xFFL & symbolTableEntry[8]) << 24)
								| ((0xFF & symbolTableEntry[9]) << 16)
								| ((0xFF & symbolTableEntry[10]) << 8)
								| ((0xFF & symbolTableEntry[11]));
							}
							symbols.add(builder.buildSymbol(addressSpace, symbolName, value + relocationBase));
							skipNext = symbolTableEntry[17];
						}
					}
				}
			}
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		return symbols;
	}
}
