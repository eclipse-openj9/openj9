/*******************************************************************************
 * Copyright (c) 2004, 2018 IBM Corp. and others
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
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Properties;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.imageio.stream.FileImageInputStream;
import javax.imageio.stream.ImageInputStream;

import com.ibm.j9ddr.corereaders.InvalidDumpFormatException;
import com.ibm.j9ddr.corereaders.memory.IMemorySource;
import com.ibm.j9ddr.corereaders.memory.ISymbol;
import com.ibm.j9ddr.corereaders.memory.Symbol;

/**
 * File in ELF format.
 */

/**
 * @author matthew
 *
 */
public abstract class ELFFileReader 
{
	private static final Logger logger = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);
	
	public static final int ELF_NOTE_HEADER_SIZE = 12; // 3 32-bit words
	public static final int EI_NIDENT = 16;
	public static final byte ELFDATA2LSB = 1;
	public static final byte ELFDATA2MSB = 2;

	public final static int ELFCLASS32 = 1;
	public final static int ELFCLASS64 = 2;

	public final static int ELF_PRARGSZ = 80;

	public final static int ARCH_IA32 = 3;
	public final static int ARCH_PPC32 = 20;
	public final static int ARCH_PPC64 = 21;
	public final static int ARCH_S390 = 22;
	public final static int ARCH_ARM = 40;
	public final static int ARCH_IA64 = 50;
	public final static int ARCH_AMD64 = 62;

	public final static int DT_NULL = 0;
	public final static int DT_DEBUG = 21;
	public final static int DT_SONAME = 14;
	public final static int DT_STRTAB = 5;

	/* file types corresponding to the _objectType field */
	public final static short ET_NONE = 0; /* No file type */
	public final static short ET_REL = 1; /* Relocatable file */
	public final static short ET_EXEC = 2; /* Executable file */
	public final static short ET_DYN = 3; /* Shared object file */
	public final static short ET_CORE = 4; /* Core file */
	public final static short ET_NUM = 5; /* Number of defined types */
	// these are unsigned values so the constants can't be shorts
	public final static int ET_LOOS = 0xfe00; /* OS-specific range start */
	public final static int ET_HIOS = 0xfeff; /* OS-specific range end */
	public final static int ET_LOPROC = 0xff00; /*
												 * Processor-specific range
												 * start
												 */
	public final static int ET_HIPROC = 0xffff; /* Processor-specific range end */

	public static final int NT_PRSTATUS = 1; // prstatus_t
	// private static final int NT_PRFPREG = 2; // prfpregset_t
	public static final int NT_PRPSINFO = 3; // prpsinfo_t
	// private static final int NT_TASKSTRUCT = 4;
	public static final int NT_AUXV = 6; // Contains copy of auxv array
	// private static final int NT_PRXFPREG = 0x46e62b7f; // User_xfpregs
	public static final int NT_HGPRS = 0x300; // High word registers

	public static final int AT_NULL = 0; // End of vector
	public static final int AT_ENTRY = 9; // Entry point of program
	public static final int AT_PLATFORM = 15; // String identifying platform
	public static final int AT_HWCAP = 16; // Machine dependent hints about
											// processor capabilities

	private long _programHeaderOffset = -1;
	private long _sectionHeaderOffset = -1;
	private short _programHeaderEntrySize = 0;
	private short _programHeaderCount = 0;
	private short _sectionHeaderEntrySize = 0;
	private short _sectionHeaderCount = 0;

	private short _objectType = 0;
	private int _version = 0;
	private int _e_flags = 0;
	private short _machineType;
	
	private long baseOffset = 0;

	private final File _file;
	
	private List<ProgramHeaderEntry> _programHeaderEntries = new LinkedList<ProgramHeaderEntry>();
	private List<SectionHeaderEntry> _sectionHeaderEntries = new LinkedList<SectionHeaderEntry>();
	

	protected abstract long padToWordBoundary(long l);

	protected abstract ProgramHeaderEntry readProgramHeaderEntry()
			throws IOException;

	protected abstract long readElfWord() throws IOException;

	protected abstract Address readElfWordAsAddress() throws IOException;

	protected abstract List<ELFSymbol> readSymbolsAt(SectionHeaderEntry entry)
			throws IOException;

	protected abstract int addressSizeBits();
	
	protected ImageInputStream is;
	
	protected String sourceName;

	//Use openELFFile to get an ELFFile instance.
	protected ELFFileReader(File file, ByteOrder byteOrder, long offset) throws IOException, FileNotFoundException, InvalidDumpFormatException
	{
		try {
			is = new FileImageInputStream(file);
			is.setByteOrder(byteOrder);
			this._file = file;
			sourceName = file.getAbsolutePath();
			this.baseOffset = offset;
			initializeReader(offset);
		} catch ( IOException e ) {
			// Don't leak file handles if we fail to create this reader.
			if( is != null ) {
				is.close();
			}
			throw e;
		} catch ( InvalidDumpFormatException e ) {
			// Don't leak file handles if we fail to create this reader.
			if( is != null ) {
				is.close();
			}
			throw e;
		}
	}
	
	public void close() throws IOException {
		if(is != null) {
			is.close();
		}
	}
	
	protected ELFFileReader(ImageInputStream in, long offset)  throws IOException, InvalidDumpFormatException {
		_file = null;
		is = in;
		this.baseOffset = offset;
		sourceName = "internal data stream";
		initializeReader(offset);
	}
	
	public String getSourceName() {
		return sourceName;
	}
	
	private void initializeReader(long offset) throws IOException, InvalidDumpFormatException {
		is.seek(offset);
		readHeader();
		readSectionHeader();
		readProgramHeader();
	}
	
	// ELF files can be either Big Endian (for example on Linux/PPC) or Little
	// Endian (Linux/IA).
	// Let's check whether it's actually an ELF file. We'll sort out the byte
	// order later.

	public static ELFFileReader getELFFileReaderWithOffset(File f, long offset) throws IOException, InvalidDumpFormatException
	{
		//Figure out which combination of bitness and architecture we are
		ImageInputStream in = new FileImageInputStream(f);
		
		if (! isFormatValid(in)) {
			throw new InvalidDumpFormatException("File " + f.getAbsolutePath() + " is not an ELF file");
		}
		int bitness = in.read();
		ByteOrder byteOrder = getByteOrder(in);
		in.close();							//no longer need the input stream as passing the File descriptor into the reader
		if (ELFCLASS64 == bitness) {
			return new ELF64FileReader(f, byteOrder, offset); 
		} else {
			return new ELF32FileReader(f, byteOrder, offset);
		}
	}

	
	public static ELFFileReader getELFFileReader(File f) throws IOException, InvalidDumpFormatException
	{
		return getELFFileReaderWithOffset(f, 0);
	}
	
	public static ELFFileReader getELFFileReader(ImageInputStream in) throws IOException, InvalidDumpFormatException {
		return getELFFileReaderWithOffset(in, 0);
	}
	
	public static ELFFileReader getELFFileReaderWithOffset(ImageInputStream in, long offset) throws IOException, InvalidDumpFormatException {
		in.mark();							//mark the stream as the validation and bitsize determinations move the underlying pointer
		in.seek(offset);
		if (! isFormatValid(in)) {
			throw new InvalidDumpFormatException("The input stream is not an ELF file");
		}
		int bitness = in.read();
		ByteOrder byteOrder = getByteOrder(in);
		in.setByteOrder(byteOrder);
		in.reset();							//reset the stream so that the reader reads from the start
		if (ELFCLASS64 == bitness) {
			return new ELF64FileReader(in, offset); 
		} else {
			return new ELF32FileReader(in, offset);
		}
	}
		
	private static ByteOrder getByteOrder(ImageInputStream in) throws IOException {
		int endian = in.read();
		ByteOrder byteOrder = null;
		if (ELFDATA2MSB == endian) {
			byteOrder = ByteOrder.BIG_ENDIAN;
		} else {
			byteOrder = ByteOrder.LITTLE_ENDIAN;
		}
		return byteOrder;
	}
	
	private static boolean isFormatValid(ImageInputStream in) throws IOException {
		byte[] headerData = new byte[4];
		in.readFully(headerData);
		
		return isELF(headerData);
	}
	
	public static boolean isELF(byte[] signature)
	{
		// 0x7F, 'E', 'L', 'F'
		return (0x7F == signature[0] && 0x45 == signature[1]
				&& 0x4C == signature[2] && 0x46 == signature[3]);
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
			fileType = "Processor-specific (" + Integer.toHexString(typeAsInt)
					+ ")";
		}
		return fileType;
	}

	private void readHeader() throws IOException, InvalidDumpFormatException
	{
		byte[] signature = readBytes(EI_NIDENT);
		
		if (! isELF(signature)) {
			throw new InvalidDumpFormatException("Missing ELF magic number");
		}
		
		_objectType = is.readShort(); // objectType
		_machineType = is.readShort();
		_version = is.readInt(); // Ignore version
		readElfWord(); // Ignore entryPoint
		_programHeaderOffset = readElfWord();
		_sectionHeaderOffset = readElfWord();
		_e_flags = is.readInt(); // e_flags
		is.readShort(); // Ignore elfHeaderSize
		_programHeaderEntrySize = is.readShort();
		_programHeaderCount = is.readShort();
		_sectionHeaderEntrySize = is.readShort();
		_sectionHeaderCount = is.readShort();
		is.readShort(); // Ignore stringTable
	}

	private SectionHeaderEntry readSectionHeaderEntry() throws IOException
	{
		long name = is.readInt() & 0xffffffffL;
		long type = is.readInt() & 0xffffffffL;
		long flags = readElfWord();
		long address = readElfWord();
		long offset = readElfWord();
		long size = readElfWord();
		long link = is.readInt() & 0xffffffffL;
		long info = is.readInt() & 0xffffffffL;
		return new SectionHeaderEntry(name, type, flags, address, offset, size,
				link, info);
	}

	/**
	 * Read the section header table but being careful about whether the data we are reading looks valid or
	 * whether we might just have been pointed at junk.  
	 * <p>
	 * The ELF spec says that the section header table is optional and we often find that in a core file many of the 
	 * modules do not have valid section header tables. In the ELF header there is a plausible count of the number of 
	 * entries and a pointer to a plausible address in an area near the bottom of the module, but the 
	 * data is not valid. The modules on disk do have valid section header tables but some parts of the section header
	 * tables are often overwritten once the module is loaded into memory.
	 * <p>
	 * However the ELF header does not tell you this - it just points at the place in memory where the table 
	 * should be (perhaps was, once) and says how many entries should be there. So we test whether it
	 * looks valid. The likely scenarios are:
	 * 1. we are pointing at non-zero junk. The first entry in the section header table should be zeros so we just return if that is 
	 * not the case. 
	 * 2. we are pointing at a block of 0s. Look at the second entry also and return if the size field is zero.
	 * If we pass these two tests, all well and good, go ahead and read in the rest.
	 * <p>
	 * This not the final line of defence though. It is fairly common that the first 
	 * part of the section header table is fine but the later entries have been overwritten.
	 * Later on, in getMemoryRanges, we see if we can find the section header string table and 
	 * get a valid name for it Since the section header string table is one of the last sections 
	 * this is usually a good indicator as to whether the section table is valid.  
	 *     
	 * @throws IOException
	 */
	private void readSectionHeader() throws IOException
	{
		if (_sectionHeaderCount < 3) {
			return; // not enough entries to even check what we have got
			// one special time this happens but is absolutely fine is when opening the 
			// core file itself - it has no section table and the header count is 0 
		}
		seek(_sectionHeaderOffset);
		SectionHeaderEntry firstEntry = readSectionHeaderEntry();
		if (firstEntry.offset != 0 || firstEntry.size != 0 || firstEntry._type != 0 || firstEntry._name != 0) {
			return; // first entry should be all zeros
		}
		seek(_sectionHeaderOffset + _sectionHeaderEntrySize);
		SectionHeaderEntry secondEntry = readSectionHeaderEntry();
		if (secondEntry.size == 0) {
			return; // second entry should not have zero size
		}	
		_sectionHeaderEntries.add(firstEntry);	
		_sectionHeaderEntries.add(secondEntry);	
		// read in the third and subsequent entries - hence start the for loop with i = 2
		for (int i = 2; i < _sectionHeaderCount; i++) {
			seek(_sectionHeaderOffset + (i * _sectionHeaderEntrySize));
			SectionHeaderEntry entry = readSectionHeaderEntry();
			_sectionHeaderEntries.add(entry);
		}
	}

	private void readProgramHeader() throws IOException
	{
		for (int i = 0; i < _programHeaderCount; i++) {
			seek(_programHeaderOffset + i * _programHeaderEntrySize);
			_programHeaderEntries.add(readProgramHeaderEntry());
		}
	}
	
	/**
	 * Seek to the point in the ELF file that corresponds to the given memory address.
	 * <p>	 
	 * Note that it uses the first program header table entry that will resolve the address.
	 * There may be more than one - it is quite common to have two or more
	 * entries which can resolve an address - e.g. when called with the address
	 * of the dynamic table, it is quite common to find that the first matching entry
	 * is the entry for the dynamic table itself (it usually has its own entry) 
	 * and a later entry with a broader range also encompasses the address; 
	 * however they invariably resolve to the same file offset so it does not matter
	 * which you use. 
	 * 
	 * @param address
	 * @throws IOException
	 */
	void seekToAddress(long address) throws IOException
	{
		ProgramHeaderEntry matchedEntry = null;

		for (ProgramHeaderEntry element : _programHeaderEntries) {
			if (element.contains(address)) {
				matchedEntry = element;
				break;
			}
		}
		if (null == matchedEntry) {
			throw new IOException("No ProgramHeaderEntry found for address");
		} else {
			seek(matchedEntry.fileOffset
					+ (address - matchedEntry.virtualAddress));
		}
	}
	
	/**
	 * Search the program header table to see whether it can successfully
	 * resolve an address into a file offset.
	 * <p>
	 * If it returns true then it will be safe to call seekToAddress without an 
	 * IOException being thrown. If it returns false, it will not. 
	 * 
	 * @param address virtual address
	 * @return true or false according to whether the address will resolve
	 */
	public boolean canResolveAddress(long address) {
		ProgramHeaderEntry matchedEntry = null;

		for (ProgramHeaderEntry element : _programHeaderEntries) {
			if (element.contains(address)) {
				matchedEntry = element;
				break;
			}
		}
		if (null == matchedEntry) {
			return false;
		} else {
			return true;
		}
	}	
	
	/** Obtain the symbols from a core reader, offset by baseAddress.
	 *  
	 * @param baseAddress the base address to offset symbols from.
	 * @param useUnallocatedSections whether to include symbols from unallocated sections. (true if reading from a library on disk, false if from a loaded library in the core file.)
	 * @return
	 * @throws IOException
	 */
	public List<? extends ISymbol> getSymbols(long baseAddress, boolean useUnallocatedSections ) throws IOException {
		List<ISymbol> symbols = new LinkedList<ISymbol>();
		for (SectionHeaderEntry entry : _sectionHeaderEntries) {
			if( useUnallocatedSections ||  entry.isAllocated() ) {
				if (entry.isSymbolTable()) {
					symbols.addAll(readSymbolsFrom(entry, baseAddress));
				}
			}
		}
		return symbols;
	}
	
	/**
	 * Iterate through the sections that were already loaded and create a list of memory source objects for them.
	 * Abandon the process if it is not possible to find the section header string table. This is one of the last
	 * sections and if it cannot be found it is a good indicator that at least part of the section header table
	 * has been overwritten. Since it is usually impossible to say where the overwriting starts it is safest to 
	 * abandon it all. This is only an issue when working with a dump that does not have the original library files
	 * attached to it.  
	 *  
	 * @param baseAddress
	 * @return List of memory sources
	 * @throws IOException
	 */
	public Collection<? extends IMemorySource> getMemoryRanges(long baseAddress, List<SectionHeaderEntry> sectionHeaderEntries, Map<Long, String> sectionHeaderStringTable) throws IOException
	{
		List<IMemorySource> sections = new LinkedList<IMemorySource>();
		if (sectionHeaderStringTable == null  || sectionHeaderEntries == null) {
			return sections; // abandon the process, return empty list
		}
		
		// Loop through the section header entries building a module section for each one
		// NB : If the core file is invalid, the method readString() may return null.
		for (SectionHeaderEntry entry : sectionHeaderEntries) {
			
			// - Sections are only loaded if they are contained in a program header.
			// - NOBITS sections (.bss) occupy space in memory but not in the library
			//   (since they define sections that are generally initialized to 0's).
			// - entry.address == 0 (sh_addr) also indicates a section that isn't loaded
			// - entry.size == 0 is always true for the first (null) section header
			if (!sectionHeaderMapsToProgramHeader(entry) || entry.isNoBits()
					|| entry.address == 0 || entry.size == 0) {
				continue;
			}
			
			String name = sectionHeaderStringTable.get(entry._name);

			// We may have read this section header from the core or the library
			// and the library may have been reloacted so create it at
			// baseAddress + entry.offset (not entry.address)
			sections.add(new ELFMemorySource(baseAddress + entry.offset, entry.size, entry.offset, this, name));
		}
		return sections;
	}
	
	public boolean sectionHeaderMapsToProgramHeader(SectionHeaderEntry section) {
		
		// To check if a section loaded you need to find out if it's in a range
		// covered by a program header.
		// (The readelf output for section to segment mappings only lists the
		// sections that fall in a header. The others aren't actually loaded.)
		for( ProgramHeaderEntry phe: _programHeaderEntries ) {
			long headerBase = phe.fileOffset;
			long headerTop = phe.fileOffset+phe.fileSize;
			if( section.offset >= headerBase && section.offset < headerTop ) {
				return true;
			}
		}
		
		return false;
	}
	
	public Map<Long, String> getSectionHeaderStringTable() throws IOException {
		SectionHeaderEntry shstrtabEntry = null;
		
		// Find the section header string table by looking for a section header entry of type string  
		// table which, when resolved against itself, has the name ".shstrtab". 
		// NB : If the core file is invalid, the method stringFromBytesAt() may return null.
		for (SectionHeaderEntry entry : _sectionHeaderEntries) {
			if (entry.isStringTable()) {
				seek(entry.offset + entry._name);
				String peek = readString();
				if (peek != null) {
					if (peek.equals(".shstrtab")) {
						shstrtabEntry = entry;
					}
				} else {
					logger.log(Level.FINER, "Error reading section header name. The core file is invalid and the results may unpredictable");
				}
			}
		}
		
		if (shstrtabEntry == null) {
			return null;
		}
		
		Map<Long, String> sectionHeaderStringTable =  new HashMap<Long, String>();
		for (SectionHeaderEntry entry : _sectionHeaderEntries) {
			
			if (null != shstrtabEntry) {
				String name = "";
				seek(shstrtabEntry.offset + entry._name);
				name = readString();
				sectionHeaderStringTable.put(entry._name, name);					
				if (name == null) {
					logger.log(Level.FINER, "Error reading section header name. The core file is invalid and the results may unpredictable");
				}
			}
		}
		return sectionHeaderStringTable;
	}

	/**
	 * Given a section header table entry that points to a symbol table, construct a list
	 * of the symbols.
	 * 
	 * The previous code here was adding the base address to everything. However,
	 * the symbols for the executable and system libraries are absolute addresses not 
	 * relative so they should not have the base address added to them. 
	 * The symbols for the j9 shared libraries though look like offsets though whether they are
	 * offsets from the base address or offsets from the section that contains them is
	 * hard to say: interpreting symbols is hard - see the System V ABI abi386-4.pdf
	 * One thing is pretty sure, though which is that if the symbol table value is already
	 * greater than the base address, it is probably already a virtual address. Do not add the base address,
	 * which is an improvement on the previous code.

	 * @param entry - section header table entry
	 * @param baseAddress - of the module
	 * @return list of the symbols
	 * @throws IOException
	 */
	private List<ISymbol> readSymbolsFrom(SectionHeaderEntry entry, long baseAddress) throws IOException {
		List<ISymbol> symbols = new LinkedList<ISymbol>();
		SectionHeaderEntry stringTable;
		try {
			stringTable = _sectionHeaderEntries.get((int)entry.link);
		} catch (IndexOutOfBoundsException e) {
			logger.log(Level.FINER, "Invalid link value " + entry.link + " when reading section header table of length " + _sectionHeaderEntries.size());
			return symbols;
		}
		seek(stringTable.offset);
		for (ELFSymbol sym : readSymbolsAt(entry)) {
			if (sym.isFunction()) {
				seek(stringTable.offset + sym.name);
				String name = readString();
				if (name != null) {
					if (sym.value != 0) {
						// Always take care comparing 64 bit numbers for >, as we are using Java long as if it were unsigned
						// i.e. numbers in the top half of 64 bit memory (> 2^63 - 1) appear as negative.  
						// If they are both positive or both negative, the comparison is the natural one.
						// If just one is negative, it appears smaller to > but is in fact bigger so the comparison is reversed.
						// Draw the truth table of the four cases if that is confusing.
						if ((sym.value > 0 && baseAddress > 0) 
								|| (sym.value < 0 && baseAddress < 0)) { // comparison is the natural one
							if (sym.value >= baseAddress) {
								symbols.add(new Symbol(name, sym.value));							
							} else {
								symbols.add(new Symbol(name, baseAddress + sym.value));							
							}
						} else {
							if (sym.value < baseAddress) { // reverse the comparison
								symbols.add(new Symbol(name, sym.value));							
							} else {
								symbols.add(new Symbol(name, baseAddress + sym.value));							
							}
							
						}
					}
				} else {
					logger.log(Level.FINER, "Error reading section header name. The core file is invalid and the results may unpredictable");
				}
			}
		}
		return symbols;
	}

	public boolean is64Bit()
	{
		return 64 == addressSizeBits();
	}
	
	public byte[] readBytes(int len) throws IOException
	{
		byte[] buffer = new byte[len];
		is.readFully(buffer);
		return buffer;
	}
	
	public short getMachineType()
	{
		return _machineType;
	}

	public List<? extends ProgramHeaderEntry> getProgramHeaderEntries()
	{
		return Collections.unmodifiableList(_programHeaderEntries);
	}
	
	public List<SectionHeaderEntry> getSectionHeaderEntries()
	{
		return Collections.unmodifiableList(_sectionHeaderEntries);
	}
	
	/**
	 * Gets the data stream for this reader
	 * @return an ImageInputStream for this reader
	 * @throws IOException
	 */
	public ImageInputStream getStream() throws IOException {
		if(is != null) {
			return is;
		} else {
			FileImageInputStream fis = new FileImageInputStream(_file);
			return fis;
		}
	}

	/**
	 * Returns the file from which this reader is reading.
	 * @return File or null if this reader is reading from a stream
	 */
	public File getFile()
	{
		return _file;
	}

	public Properties getProperties()
	{
		Properties props = new Properties();
		props.setProperty("Object file type", _nameForFileType(_objectType));
		props.setProperty("Object file version", Integer.toHexString(_version));
		props.setProperty("Processor-specific flags", Integer.toHexString(_e_flags));
		
		return props;
	}
	
	public int readInt() throws IOException {
		return is.readInt();
	}
	
	public byte readByte() throws IOException {
		return is.readByte();
	}
	
	public void seek(long pos) throws IOException {
		try {
			is.seek(baseOffset + pos);
		} catch (IndexOutOfBoundsException e) {
			throw new IOException("Seek index out of bounds in " + getSourceName());
		}
	}
	
	public ByteOrder getByteOrder() {
		return is.getByteOrder();
	}

	public short readShort() throws IOException {
		return is.readShort();
	}

	public long readLong() throws IOException {
		return is.readLong();
	}

	public void readFully(byte[] b, int off, int len) throws IOException {
		is.readFully(b, off, len);
	}
	
	/**
	 * Reads a string from the readers current position until
	 * it is terminated by a null (0) byte.
	 * Bytes are read and converted to an ASCII string.
	 * string.
	 * 
	 * If readByte throws an exception, null will be returned.
	 *  
	 * @return the null terminated sting at the readers current position or null
	 * @throws IOException
	 */
	public String readString()
	{
		String toReturn = null;
		try {
			StringBuffer buf = new StringBuffer();
			byte b = readByte();
			for (long i = 1; 0 != b; i++) {
				buf.append(new String(new byte[] {b}, "ASCII"));
				b = readByte();
			}
			toReturn = buf.toString();
		} catch (Exception e) {
		}
		return toReturn;
	}
	
	/**
	 * Get the base address of an executable or library, given the ELF file. 
	 * This is the lowest virtual address of the Program Header Table entries of type PT_LOAD, which
	 * is probably the first but we search them all just in case. 
	 * The following is summarised from the System V Application Binary Interface,
	 * gabi41.pdf, section Base Address
	 * 
	 *    "... to compute the base address, one determines the memory address associated with 
	 *    the lowest p_vaddr value for a PT_LOAD segment. This address is truncated ..."
	 *    
	 * The truncation is to do with page sizes and in practice not found to be needed. 
	 * 
	 * 
	 * @return base address for the executable or library
	 */
	public long getBaseAddress() {
		long lowestVirtualAddressSoFar = Long.MAX_VALUE;
		for (ProgramHeaderEntry entry : getProgramHeaderEntries()) {
			if (entry.isLoadable()) {
				if (entry.virtualAddress < lowestVirtualAddressSoFar) {
					lowestVirtualAddressSoFar = entry.virtualAddress;
				}
			}
		}
		return lowestVirtualAddressSoFar;
	}
	
	/**
	 * Search the program header table for the dynamic entry. There should be only one of these
	 * Typically it is within the first few entries, often the third, so this is not expensive
	 * 
	 * @return the program header table entry for the dynamic table
	 */
	public ProgramHeaderEntry getDynamicTableEntry() {
		for (ProgramHeaderEntry entry : getProgramHeaderEntries()) {
			if (entry.isDynamic()) {
				// We only expect to find one per module.
				return entry;
			}
		}
		return null;
	}

	/**
	 * Examine the ELF header to determine if this file is an executable or not.
	 * Often it will instead be a shared library.
	 * 
	 * @return true if the file is an executable
	 */
	public boolean isExecutable() {
		return (_objectType == ET_EXEC);
	}
	
	
	/**
	 * Assume the entry given refers to a loaded library or program and
	 * find and return its name or null if it couldn't be determined.
	 * 
	 * This method does not throw exceptions as we are seeking around the
	 * core file which could be damaged or may come from a version of linux
	 * which doesn't leave this information available, just do our best.
	 * 
	 * @param ELFFileReader for the core file as a whole
	 * @return the SONAME (library name) or null
	 */
	public String readSONAME(ELFFileReader coreFileReader) {
		ProgramHeaderEntry dynamicTableEntry = getDynamicTableEntry();		
		try {
			long imageStart = dynamicTableEntry.virtualAddress;

			// Seek to the start of the dynamic section
			seekToAddress(imageStart);

			// Loop reading the dynamic section tag-value/pointer pairs until a 'debug'
			// entry is found
			long tag;
			long sonameOffset = -1;
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
				if ( !((tag >= 0         ) && (tag <= 33        )) &&
						!((tag >= 0x60000000) && (tag <= 0x6FFFFFFF)) &&
						!((tag >= 0x70000000) && (tag <= 0x7FFFFFFF))    ) {
					logger.log(Level.FINER,
						"Error reading SONAME. Invalid tag value '0x" + 
						Long.toHexString(tag) +
						"'. The core file is invalid and the results may unpredictable"
					);
				}
				if( tag == DT_SONAME ) {
					sonameOffset = address;
				} else if( tag == DT_STRTAB ) {
					strtabAddress = address;
				}

			} while ((tag != DT_NULL));

			// If we have the soname pointer and the string table pointer
			// look up the soname in the string table.
			// According to the elf spec, the address of the string table (strtabAddress) is always a virtual address.
			// So, take care to use ELFFileReader's seekToAddress(virtual address) here and not seek(offset), even though 
			// for some of the J9 modules, e.g. libjvm.so, the address in the dynamic table looks like an offset. 
			// That is an artifact of the way that the addresses need to be interpreted through the program header table 
			// for the module. For example, in the dynamic table in a given dump, the address of the string table was 
			// 0x2630. This looks like an offset and in fact could be treated as such but is really an address because 
			// in that dump the program header table for libjvm.so contains the following entry:
			//   Type           Offset   VirtAddr           PhysAddr           FileSiz  MemSiz   Flg Align
			//   LOAD           0x000000 0x0000000000000000 0x0000000000000000 0x025e94 0x025e94 R E 0x100000
			// This indicates that addresses in the range 0 to 0x025e94 should be understood to be resolved against offset 0 
			// within the file, so address 0x2630 also means offset 0x2630.  
			// By contrast, for a different module in the same dump the address of the string table was 0x494754 which is 
			// definitely not an offset. In this case the program header table contains the following row:
			//   Type           Offset   VirtAddr           PhysAddr           FileSiz  MemSiz   Flg Align
			//   LOAD           0x000000 0x00493000 0x00493000 0x147a8 0x147a8 R E 0x1000
			// so that the address 0x494754 was at offset (0x494754 - 0x493000) = 0x1754 in the file.  
			// Always treat the address as an address and use seekToAddress which hunts through the program header table entries
			// to resolve, and all will be well, regardless of whether the address looks like an offset or not.
			// As a further complication, though, sometimes the address can only be resolved using the 
			// program header table of the core file, not the module. Hence we try to resolve and seek to the address
			// first using the module's program header table (elfFile) then the core file's (_reader).
			// For further details see CMVC 181593
			if( sonameOffset != -1 && strtabAddress != -1 ) {
				String name;
				if (canResolveAddress(strtabAddress + sonameOffset)) {
					seekToAddress(strtabAddress + sonameOffset);
					name = readString();
				} else {
					coreFileReader.seekToAddress(strtabAddress + sonameOffset);
					name = coreFileReader.readString();
				}
				if((name == null) || (name.length() == 0)) {
					// If we don't read a string back, or if the string we do get is empty then return null
					// as the SONAME is unknown
					return null;
				}
				return name;
			} else {
				// Even if we do everything right, sometimes we do not find a name. For example, for  
				// the library libj9dmp26.so, for some reason, the dynamic table pointer we are passed usually
				// points at something that is clearly not a valid dynamic table - just some different bit of 
				// storage. Hence the loop above iterates through until it hits a DT_NULL - zero - or throws a 
				// IOException - and we come to here with both sonameAddress and strtabAddress == -1.  
				return null;				
			}
		} catch (IOException e) {
			logger
					.info("ProgramHeaderEntry @ "
							+ Long.toHexString(dynamicTableEntry.virtualAddress)
							+ " does not appear point to a module with a valid SONAME tag.");
		}
		return null;
	}
	
	/**
	 * Perform a quick check on two ELF readers to see if they represent the same library.
	 * otherReader is the one we hope is better that this reader.
	 * 
	 * This is not the same as checking they are equal as we hope the other reader actually
	 * has more information in.
	 * 
	 * @param otherReader another ELFFileReader to compare against this one.
	 */
	public boolean isCompatibleWith (ELFFileReader otherReader) {
		
		if( null == otherReader ) {
			return false;
		}
		
		/*
		 * First sanity check the libraries came from the same type of machine.
		 * (We aren't opening a z/Linux dump on Linux AMD64 and picking up the
		 * wrong libpthread.so for example.)
		 */
		if( this.getMachineType() != otherReader.getMachineType() ) {
			return false;
		}
		
		if( this._programHeaderCount != otherReader._programHeaderCount) {
			return false;
		}
		
		/*
		 * The prelink process means it's common for libc.so.6 (and probably
		 * other system libraries) to be exactly the same size but to load
		 * things at very different addresses. Even between two machines running
		 * the same level of the same OS on the same architecture. This causes
		 * problems with resolving addresses so we check that for any headers
		 * that exist in both sources they have the same addresses.
		 */
		Iterator<? extends ProgramHeaderEntry> primaryHeaders = this.getProgramHeaderEntries().iterator();
		Iterator<? extends ProgramHeaderEntry> secondaryHeaders = otherReader.getProgramHeaderEntries().iterator();
		while( primaryHeaders.hasNext() && secondaryHeaders.hasNext() ) {
			ProgramHeaderEntry primaryEntry = primaryHeaders.next();
			ProgramHeaderEntry secondaryEntry = secondaryHeaders.next();
			if( primaryEntry.virtualAddress != secondaryEntry.virtualAddress ) {
				return false;
			}
		}
		
		if( this._sectionHeaderCount != otherReader._sectionHeaderCount) {
			return false;
		}
		
		if( this._version != otherReader._version) {
			return false;
		}
		
		return true;
	}


}
