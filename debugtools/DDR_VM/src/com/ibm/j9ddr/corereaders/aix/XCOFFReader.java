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
package com.ibm.j9ddr.corereaders.aix;

import java.io.File;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.nio.ByteOrder;

import java.util.Date;
import java.util.LinkedList;
import java.util.List;
import java.util.Properties;

import javax.imageio.stream.ImageInputStream;

import com.ibm.j9ddr.corereaders.ClosingFileReader;
import com.ibm.j9ddr.corereaders.IModuleFile;
import com.ibm.j9ddr.corereaders.memory.IMemorySource;
import com.ibm.j9ddr.corereaders.memory.ISymbol;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;
import com.ibm.j9ddr.corereaders.memory.ProtectedMemoryRange;
import com.ibm.j9ddr.corereaders.memory.Symbol;

/**
 * XCOFF spec here: http://publib16.boulder.ibm.com/pseries/en_US/files/aixfiles/XCOFF.htm
 * @author jmdisher
 *
 */
public class XCOFFReader implements IModuleFile
{

	public static final short F_RELFLG = 0x0001;
	public static final short F_EXEC = 0x0002;
	public static final short F_LNNO = 0x0004;
	public static final short F_AR32W = 0x0200;
	public static final short F_DYNLOAD = 0x1000;
	public static final short F_SHROBJ = 0x2000;
	public static final short F_LOADONLY = 0x4000;
	
	//From scnhdr.h
	public static final int STYP_REG =      0x00;    /* "regular" section */
	public static final int STYP_PAD =      0x08;    /* "padding" section */
	public static final int STYP_TEXT =     0x20;    /* section contains text only */
	public static final int STYP_DATA =     0x40;    /* section contains data only */
	public static final int STYP_BSS =      0x80;    /* section contains bss only */
	public static final int STYP_EXCEPT =   0x0100;  /* Exception section */
	public static final int STYP_INFO =     0x0200;  /* Comment section */
	public static final int STYP_TDATA =    0x0400;  /* Thread-local data section */
	public static final int STYP_TBSS =     0x0800;  /* Thread-local bss section */
	public static final int STYP_LOADER =   0x1000;  /* Loader section */
	public static final int STYP_DEBUG =    0x2000;  /* Debug section */
	public static final int STYP_TYPCHK =   0x4000;  /* Type check section */
	public static final int STYP_OVRFLO =   0x8000;  /* Overflow section header
	                                   for handling relocation and
	                                   line number count overflows */

	
	private static final short XCOFF_MAGIC_NUMBER_AIX32 = 0x01DF;
	private static final short XCOFF_MAGIC_NUMBER_AIX64_PRE43 = 0x01EF;
	private static final short XCOFF_MAGIC_NUMBER_AIX64_POST51 = 0x01F7;
	private final ImageInputStream _backing;
	private long _startOffset;
	private long _size;
	private boolean _is64Bit;
	private final String _library;
	
	private long _symbolTableOffset = -1;
	
	//values held purely for module properties
	/**
	 * The creation time and date of the module in milliseconds since the epoch
	 */
	private long _timeAndDate = 0;
	private short _flags = 0;
	
	public XCOFFReader(File library) throws IOException
	{
		this(library, 0, (int)library.length());
	}
	
	public XCOFFReader(File library, long l, long m) throws IOException
	{
		_startOffset = l;
		_size = m;
		_library = library.getName();
		_backing = new ClosingFileReader(library, ByteOrder.BIG_ENDIAN);
		
		init();

	}
	
	public XCOFFReader(String libraryName, ImageInputStream in, long l, long m) throws IOException {
		_backing = in;
		_library = libraryName;
		_startOffset = l;
		_size = m;
		
		init();
	}
	
	private void init() throws IOException {
		//by this point, the basic support for reading the file complete so on to looking into the file
		seekFileRelative(0);
		short magic = _backing.readShort();
		if (!((XCOFF_MAGIC_NUMBER_AIX32 == magic) || (XCOFF_MAGIC_NUMBER_AIX64_PRE43 == magic) || (XCOFF_MAGIC_NUMBER_AIX64_POST51 == magic))) {
			throw new IllegalArgumentException();
		}
		_is64Bit = ((XCOFF_MAGIC_NUMBER_AIX64_PRE43 == magic) || (XCOFF_MAGIC_NUMBER_AIX64_POST51 == magic));
		_backing.readShort(); // numberOfSections
		//time and date in XCOFF is _seconds_ since epoch while in Java it is _milliseconds_ since epoch
		_timeAndDate = 1000L * (0xFFFFFFFFL & _backing.readInt());
		//by this point, the file pointer is at 8 so get the offset to the symbol table
		_symbolTableOffset = (_is64Bit ? _backing.readLong() : (0xFFFFFFFFL & _backing.readInt()));
		seekFileRelative(16);
		_backing.readShort(); // optionalHeaderSize
		_flags = _backing.readShort();		
	}
	
	private void seekFileRelative(long offset) throws IOException
	{
		_backing.seek(_startOffset + offset);
	}
	
	private long _symbolTableOffset() throws IOException
	{
		return _symbolTableOffset;
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
		try {
			return new String(rawData, start, end - start, "UTF-8");
		} catch (UnsupportedEncodingException e) {
			// UTF-8 will be supported.
			return null;
		}
	}
	
	public long baseFileOffset()
	{
		return _startOffset;
	}
	
	public long logicalSize()
	{
		return _size;
	}

	
	public Properties getProperties()
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

	public IMemorySource getTextSegment(long virtualAddress, long virtualSize)
	{
		return new TextSegment(virtualAddress, virtualSize);
	}
	
	private class TextSegment extends ProtectedMemoryRange implements IMemorySource
	{
		
		public TextSegment(long baseAddress, long size)
		{
			super(baseAddress, size);
			this.readOnly = true;
			this.executable = true;
			this.shared = false;
		}

		public String getName()
		{
			return ".text";
		}

		public int getAddressSpaceId()
		{
			return 0;
		}

		public int getBytes(long address, byte[] buffer, int offset, int length)
				throws MemoryFault
		{
			if (address < baseAddress) {
				throw new MemoryFault(address,"Address out of range. Segment starts at 0x" + Long.toHexString(baseAddress));
			}
			
			long topAddress = address + length - 1;
			
			if (topAddress > getTopAddress()) {
				throw new MemoryFault(address,"Address out of range (overflow). Top address is 0x" + Long.toHexString(getTopAddress()));
			}
			
			if ((topAddress - baseAddress) > size) {
				throw new MemoryFault(address,"Address out of range (overflow). Top backed address is 0x" + Long.toHexString(baseAddress + size - 1));
			}
			
			long startPoint = address - baseAddress;
			
			try {
				_backing.seek(_startOffset + startPoint);
				
				_backing.readFully(buffer, offset, length);
			} catch (IOException e) {
				throw new MemoryFault(address,"Memory fault caused by IOException reading file " + _library);
			}
			
			return length;
		}
		
	}

	public List<? extends ISymbol> getSymbols(long relocationBase) throws IOException
	{
		List<ISymbol> symbols = new LinkedList<ISymbol>();
		
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
							| ((0xFFL & symbolTableEntry[1]) << 48)
							| ((0xFFL & symbolTableEntry[2]) << 40)
							| ((0xFFL & symbolTableEntry[3]) << 32)
							| ((0xFFL & symbolTableEntry[4]) << 24)
							| ((0xFFL & symbolTableEntry[5]) << 16)
							| ((0xFFL & symbolTableEntry[6]) << 8)
							| ((0xFFL & symbolTableEntry[7]));
						} else {
							value = ((0xFF & symbolTableEntry[8]) << 24)
							| ((0xFF & symbolTableEntry[9]) << 16)
							| ((0xFF & symbolTableEntry[10]) << 8)
							| ((0xFF & symbolTableEntry[11]));
						}
						symbols.add(new Symbol(symbolName, value + relocationBase));
						skipNext = symbolTableEntry[17];
					}
				}
			}
		}

		return symbols;
	}

	public void close() throws IOException {
		if(_backing != null) {
			_backing.close();
		}
		
	}
}
