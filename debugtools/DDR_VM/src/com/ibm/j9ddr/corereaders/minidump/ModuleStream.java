/*******************************************************************************
 * Copyright (c) 2004, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders.minidump;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.Date;
import java.util.LinkedList;
import java.util.List;
import java.util.Properties;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.CorruptCoreException;
import com.ibm.j9ddr.corereaders.memory.IAddressSpace;
import com.ibm.j9ddr.corereaders.memory.IMemoryRange;
import com.ibm.j9ddr.corereaders.memory.IModule;
import com.ibm.j9ddr.corereaders.memory.ISymbol;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;
import com.ibm.j9ddr.corereaders.memory.MemoryRange;
import com.ibm.j9ddr.corereaders.memory.Module;
import com.ibm.j9ddr.corereaders.memory.Symbol;
import com.ibm.j9ddr.corereaders.minidump.unwind.RuntimeFunction;
import com.ibm.j9ddr.corereaders.minidump.unwind.UnwindModule;

class ModuleStream extends LateInitializedStream
{

	private static final Logger logger = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);
	
	public ModuleStream(int dataSize, int location)
	{
		super(dataSize, location);
	}

	/**
	 * Get the module name as a String from the UTF-16LE data held in the dump
	 * 
	 * @throws IOException
	 */
	private String getModuleName(MiniDumpReader dump, int position)
			throws IOException
	{
		dump.seek(position);
		int length = dump.readInt();
		if (length <= 0 || 512 <= length)
			length = 512;
		byte[] nameWide = dump.readBytes(length);
		// FIXME: Should UTF-16LE be hard coded here? Is the encoding platform
		// specific?
		return new String(nameWide, "UTF-16LE");
	}

	@Override
	public void readFrom(MiniDumpReader dump, IAddressSpace as,
			boolean is64Bit) throws IOException, CorruptDataException
	{
		dump.seek(getLocation());
		int numberOfModules = dump.readInt();
		
		if (numberOfModules > 1024) {
			throw new CorruptDataException("Improbably high number of modules found: " + numberOfModules + ", location = " + Long.toHexString(getLocation()));
		}
		
		class ModuleData {
			long imageBaseAddress;
			Properties properties;
			int nameAddress;
		}
		
		ModuleData[] moduleData = new ModuleData[numberOfModules];
		
		for (int i = 0; i < numberOfModules; i++) {
			moduleData[i] = new ModuleData();
			moduleData[i].imageBaseAddress = dump.readLong();
			int imageSize = dump.readInt();
			int checksum = dump.readInt();
			int timeDateStamp = dump.readInt();
			moduleData[i].nameAddress = dump.readInt();
			moduleData[i].properties = readProperties(dump, imageSize, checksum, timeDateStamp);
		}
		
		for (ModuleData thisModule : moduleData) {
			final long imageBaseAddress = thisModule.imageBaseAddress;
			final String moduleName = getModuleName(dump, thisModule.nameAddress);
			final Properties properties = thisModule.properties;
			
			short magic;
			try {
				magic = as.getShortAt(imageBaseAddress);
				if (0x5A4D != magic) {		
					logger.logp(Level.WARNING, "com.ibm.j9ddr.corereaders.minidump.ModuleStream", "readFrom", "Magic number was: " + Integer.toHexString(0xFFFF & magic) + " expected 0x5A4D");
				}
			} catch (MemoryFault e1) {
				logger.logp(Level.WARNING, "com.ibm.j9ddr.corereaders.minidump.ModuleStream", "readFrom", "MemoryFault reading magic number",e1);
			}
			
			long e_lfanewAddress = imageBaseAddress + 0x3c;
			// load the e_lfanew since that is the load-address-relative location of
			// the PE Header
			Collection<IMemoryRange> sections = new LinkedList<IMemoryRange>();
			try {
				long e_lfanew = 0xFFFFFFFFL & as.getIntAt(e_lfanewAddress);
				//push us to the start of the PE header
				long readingAddress = e_lfanew + imageBaseAddress;
				List<ISymbol> symbols = null;
				if (0 != e_lfanew) {
					loadModuleSections(as,imageBaseAddress,readingAddress,e_lfanew,sections);
					
					symbols = buildSymbols(dump,as, imageBaseAddress);
				}
				
				if (symbols == null) {
					symbols = new LinkedList<ISymbol>();
				}
				
				// Load the list of RUNTIME_FUNCTION structures that map code 
				// ranges to stack unwind information.
				List<RuntimeFunction> runtimeFunctionList = null;			
				runtimeFunctionList = buildRuntimeFunctionList(dump,as, imageBaseAddress);
				
				IModule module;
				if( runtimeFunctionList != null ) {
					module = new UnwindModule(as.getProcesses().iterator().next(),moduleName,symbols,sections, thisModule.imageBaseAddress, properties, runtimeFunctionList);
					// Uncomment to dump unwind info as we find it. This is very verbose.
					// ((UnwindModule)module).dumpUndwindInfo(System.err);
				} else {
					module = new Module(as.getProcesses().iterator().next(),moduleName,symbols,sections, thisModule.imageBaseAddress, properties);
				}
				if (moduleName.toLowerCase().endsWith(".exe")) {
					dump.setExecutable(module);
				} else {
					dump.addLibrary(module);
				}
			} catch (RuntimeException e) {
				//Don't want to prevent RTE's propagating
				throw e;
			} catch (Exception e) {
			//this needs to be here in order to not fail completely whenever we
			//encounter a strange record
				logger.logp(Level.WARNING, "com.ibm.j9ddr.corereaders.minidump.ModuleStream", "readFrom", "Problem reading symbols",e);
			}
		}
		
	}

	private void loadModuleSections(IAddressSpace as, long imageBaseAddress, long readingAddress, long e_lfanew, Collection<IMemoryRange> sections) throws MemoryFault
	{
		int pemagic = as.getIntAt(readingAddress); 
		readingAddress += 4;
		if (0x4550 != pemagic) {
			logger.logp(Level.WARNING, "com.ibm.j9ddr.corereaders.minidump.ModuleStream", "loadModuleSections", "PE Magic is: \"" + Integer.toHexString(pemagic));
		}
		//typedef struct _IMAGE_FILE_HEADER {
		readingAddress += 2;// dump.readShort(); //machine
		short numberOfSections = as.getShortAt(readingAddress);
		readingAddress +=2;
		readingAddress += 4;// dump.readInt(); //date/time stamp
		readingAddress += 4;// dump.readInt(); //pointerToSymbolTable
		readingAddress += 4;// dump.readInt(); //numberOfSymbols
		short sizeOfOptionalHeader = as.getShortAt(readingAddress);
		readingAddress += 2;
		readingAddress += 2;// dump.readShort(); //characteristics
						
		//now skip ahead to after the optional header since that is where section
		//headers can be found
		readingAddress = imageBaseAddress + e_lfanew + 24 /* sizeof PE header */ + (0xFFFFL & sizeOfOptionalHeader);
		for (int j = 0; j < numberOfSections; j++) {
			// typedef struct _IMAGE_SECTION_HEADER
			byte rawName[] = new byte[8];
			as.getBytesAt(readingAddress, rawName); 
			readingAddress += 8;
			long virtualSize = as.getIntAt(readingAddress); 
			readingAddress += 4;
			long virtualAddress = as.getIntAt(readingAddress); 
			readingAddress += 4;
			readingAddress+=4; // dump.readInt(); //sizeOfRawData
			readingAddress+=4; // dump.readInt(); //pointerToRawData
			readingAddress+=4; // dump.readInt(); //pointerToRelocations
			readingAddress+=4; // dump.readInt(); //pointerToLineNumbers
			readingAddress+=2; // dump.readShort(); //numberOfRelocations
			readingAddress+=2; // dump.readShort(); //numberOfLineNumbers
			readingAddress+=4; // dump.readInt(); //section_characteristics
										
			long relocatedAddress = virtualAddress + imageBaseAddress;
			
			String name;
			
			try {
				name = new String(rawName,"ASCII");
				// Trim off any trailing nulls.
				name = name.trim();
			} catch (UnsupportedEncodingException e) {
				throw new RuntimeException(e);
			}
						
			IMemoryRange section = new MemoryRange(as, relocatedAddress, virtualSize, name);
			sections.add(section);
		}
	}

	private Properties readProperties(MiniDumpReader dump, int imageSize, int checksum, int timeDateStamp)
			throws IOException
	{
		int versionInfoDwSignature = dump.readInt();
		int versionInfoDwStrucVersion = dump.readInt(); //versionInfoDwStrucVersion
		int versionInfoDwFileVersionMS = dump.readInt(); //versionInfoDwFileVersionMS
		int versionInfoDwFileVersionLS = dump.readInt(); //versionInfoDwFileVersionLS
		int versionInfoDwProductVersionMS = dump.readInt(); // versionInfoDwProductVersionMS
		int versionInfoDwProductVersionLS = dump.readInt(); // versionInfoDwProductVersionLS
		int versionInfoDwFileFlagsMask = dump.readInt(); // versionInfoDwFileFlagsMask
		int versionInfoDwFileFlags = dump.readInt(); // versionInfoDwFileFlags
		int versionInfoDwFileOS = dump.readInt(); // versionInfoDwFileOS
		int versionInfoDwFileType = dump.readInt(); // versionInfoDwFileType
		int versionInfoDwFileSubtype = dump.readInt(); // versionInfoDwFileSubtype
		int versionInfoDwFileDateMS = dump.readInt(); // versionInfoDwFileDateMS
		int versionInfoDwFileDateLS = dump.readInt(); // versionInfoDwFileDateLS
		
		dump.readInt(); // Ignore cvRecordDataSize
		dump.readInt(); // Ignore cvRecordDataRva
		dump.readInt(); // Ignore miscRecordDataSize
		dump.readInt(); // Ignore miscRecordDataRva

		dump.readLong(); // Ignore reserved0
		dump.readLong(); // Ignore reserved1
		
		//populate this module with some interesting properties
		Properties properties = new Properties();
		properties.setProperty("imageSize",Integer.toHexString(imageSize));
		properties.setProperty("checksum",Integer.toHexString(checksum));
		//note that, in Windows, timeDateStamp is seconds since Dec 31, 1969
		// @4:00 PM so we have to add 8 hours to have it match up with UCT and then
		// multiply by 1000 to make it the milliseconds that Java expects
		properties.setProperty("timeDateStamp", (new Date(1000L *(timeDateStamp + (3600L * 8L)))).toString());
		properties.setProperty("versionInfoDwSignature",Integer.toHexString(versionInfoDwSignature));

		properties.setProperty("versionInfoDwStrucVersion",Integer.toHexString(versionInfoDwStrucVersion));
		properties.setProperty("versionInfoDwFileVersionMS",Integer.toHexString(versionInfoDwFileVersionMS));
		properties.setProperty("versionInfoDwFileVersionLS",Integer.toHexString(versionInfoDwFileVersionLS));
		properties.setProperty("versionInfoDwProductVersionMS",Integer.toHexString(versionInfoDwProductVersionMS));
		properties.setProperty("versionInfoDwProductVersionLS",Integer.toHexString(versionInfoDwProductVersionLS));
		properties.setProperty("versionInfoDwFileFlagsMask",Integer.toHexString(versionInfoDwFileFlagsMask));
		properties.setProperty("versionInfoDwFileFlags",Integer.toHexString(versionInfoDwFileFlags));
		properties.setProperty("versionInfoDwFileOS",Integer.toHexString(versionInfoDwFileOS));
		properties.setProperty("versionInfoDwFileType",Integer.toHexString(versionInfoDwFileType));
		properties.setProperty("versionInfoDwFileSubtype",Integer.toHexString(versionInfoDwFileSubtype));
		properties.setProperty("versionInfoDwFileDateMS",Integer.toHexString(versionInfoDwFileDateMS));
		properties.setProperty("versionInfoDwFileDateLS",Integer.toHexString(versionInfoDwFileDateLS));
		
		return properties;
	}
	

	/**
	 * Looks at the module loaded at imageBase in the given dump and produces
	 a symbol table for which it returns the iterator.
	 * @param dump
	 * @param builder
	 * @param imageBase
	 * @param moduleLoadAddress
	 * @return
	 * @throws CorruptCoreException 
	 */
	private List<ISymbol> buildSymbols(MiniDumpReader dump, IAddressSpace as,
			long imageBase) throws CorruptDataException, CorruptCoreException
	{
		long moduleLoadAddress = imageBase;
		byte magic[] = new byte[2];
		as.getBytesAt(imageBase, magic);
		String magicStr = null;
		try {
			magicStr = new String(magic, "ASCII");
		} catch (UnsupportedEncodingException e) {
			throw new RuntimeException(e);
		}

		if (!magicStr.equals("MZ")) {
			throw new CorruptCoreException("Invalid image magic number: \"" + magicStr + "\" @ " + Long.toHexString(imageBase));
		}
		//look up the PE offset: it is base+3c
		int peInt = as.getIntAt(imageBase + 0x3cL);
		long peBase = (peInt & 0xFFFFFFFFL) + imageBase;
		// * typedef struct _IMAGE_NT_HEADERS {
	
		as.getBytesAt(peBase, magic); // DWORD Signature;
		try {
			magicStr = new String(magic, "UTF-8");
		} catch (UnsupportedEncodingException e) {
			// UTF-8 will be supported.
		}
		if (!magicStr.equals("PE")) {
			throw new CorruptCoreException("Invalid PE magic number: \"" + magicStr + "\" @ " + Long.toHexString(peBase));
		}
		long nextRead = peBase + 4;
		// IMAGE_FILE_HEADER FileHeader;
		// IMAGE_OPTIONAL_HEADER32 OptionalHeader;
					
	
		//cue us up to the optional header
		// typedef struct _IMAGE_FILE_HEADER {
		nextRead += 2; //short machine = dump.readShort(); // WORD Machine;
		nextRead += 2; //dump.readShort(); // WORD NumberOfSections;
		nextRead += 4; //dump.readInt(); // DWORD TimeDateStamp;
		nextRead += 4; //dump.readInt(); // DWORD PointerToSymbolTable;
		nextRead += 4; //dump.readInt(); // DWORD NumberOfSymbols;
		long imageOptionalHeaderSizeAddress = nextRead;
		short optionalHeaderSize = as.getShortAt(imageOptionalHeaderSizeAddress); // WORD SizeOfOptionalHeader;
		nextRead+= 2;
					
		nextRead += 2; //dump.readShort(); // WORD Characteristics;
		//cue us up to the first data directory
		if (224 == optionalHeaderSize) {
			//32-bit optional header
			// typedef struct _IMAGE_OPTIONAL_HEADER {
			short magicShort = as.getShortAt(nextRead);
			if (0x10b != magicShort) {
				throw new
				CorruptCoreException("Invalid IMAGE_OPTIONAL_HEADER magic number: \"0x" +
						Integer.toHexString(0xFFFF & magicShort) + "\" @ " +
						Long.toHexString(nextRead));
			}
			nextRead += 2; //dump.readBytes(2); // WORD Magic;
			nextRead += 1; //dump.readByte(); // BYTE MajorLinkerVersion;
			nextRead += 1; //dump.readByte(); // BYTE MinorLinkerVersion;
			nextRead += 4; //dump.readInt(); // DWORD SizeOfCode;
			nextRead += 4; //dump.readInt(); // DWORD SizeOfInitializedData;
			nextRead += 4; //dump.readInt(); // DWORD SizeOfUninitializedData;
			nextRead += 4; //dump.readInt(); // DWORD AddressOfEntryPoint;
			nextRead += 4; //dump.readInt(); // DWORD BaseOfCode;
			nextRead += 4; //dump.readInt(); // DWORD BaseOfData;
			nextRead += 4; //dump.readInt(); // DWORD ImageBase;
			nextRead += 4; //dump.readInt(); // DWORD SectionAlignment;
			nextRead += 4; //dump.readInt(); // DWORD FileAlignment;
			nextRead += 2; //dump.readShort(); // WORD MajorOperatingSystemVersion;
			nextRead += 2; //dump.readShort(); // WORD MinorOperatingSystemVersion;
			nextRead += 2; //dump.readShort(); // WORD MajorImageVersion;
			nextRead += 2; //dump.readShort(); // WORD MinorImageVersion;
			nextRead += 2; //dump.readShort(); // WORD MajorSubsystemVersion;
			nextRead += 2; //dump.readShort(); // WORD MinorSubsystemVersion;
			nextRead += 4; //dump.readInt(); // DWORD Win32VersionValue;
			nextRead += 4; //dump.readInt(); // DWORD SizeOfImage;
			nextRead += 4; //dump.readInt(); // DWORD SizeOfHeaders;
			nextRead += 4; //dump.readInt(); // DWORD CheckSum;
			nextRead += 2; //dump.readShort(); // WORD Subsystem;
			nextRead += 2; //dump.readShort(); // WORD DllCharacteristics;
			nextRead += 4; //dump.readInt(); // DWORD SizeOfStackReserve;
			nextRead += 4; //dump.readInt(); // DWORD SizeOfStackCommit;
			nextRead += 4; //dump.readInt(); // DWORD SizeOfHeapReserve;
			nextRead += 4; //dump.readInt(); // DWORD SizeOfHeapCommit;
			nextRead += 4; //dump.readInt(); // DWORD LoaderFlags;
			nextRead += 4; //dump.readInt(); // DWORD NumberOfRvaAndSizes;
			// IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
			// } IMAGE_OPTIONAL_HEADER32, *PIMAGE_OPTIONAL_HEADER32;
		} else if (240 == optionalHeaderSize) {
			//64-bit optional header
			// typedef struct _IMAGE_OPTIONAL_HEADER64 {
			short magicShort = as.getShortAt(nextRead);
			if (0x20b != magicShort) {
				throw new
				CorruptCoreException("Invalid IMAGE_OPTIONAL_HEADER64 magic number: \"0x"
						+ Integer.toHexString(0xFFFF & magicShort) + "\" @ " +
						Long.toHexString(nextRead));
			}
			nextRead += 2;// WORD Magic;
			nextRead += 1;// BYTE MajorLinkerVersion;
			nextRead += 1;// BYTE MinorLinkerVersion;
			nextRead += 4;// DWORD SizeOfCode;
			nextRead += 4;// DWORD SizeOfInitializedData;
			nextRead += 4;// DWORD SizeOfUninitializedData;
			nextRead += 4;// DWORD AddressOfEntryPoint;
			nextRead += 4;// DWORD BaseOfCode;
			nextRead += 8;// ULONGLONG ImageBase;
			nextRead += 4;// DWORD SectionAlignment;
			nextRead += 4;// DWORD FileAlignment;
			nextRead += 2;// WORD MajorOperatingSystemVersion;
			nextRead += 2;// WORD MinorOperatingSystemVersion;
			nextRead += 2;// WORD MajorImageVersion;
			nextRead += 2;// WORD MinorImageVersion;
			nextRead += 2;// WORD MajorSubsystemVersion;
			nextRead += 2;// WORD MinorSubsystemVersion;
			nextRead += 4;// DWORD Win32VersionValue;
			nextRead += 4;// DWORD SizeOfImage;
			nextRead += 4;// DWORD SizeOfHeaders;
			nextRead += 4;// DWORD CheckSum;
			nextRead += 2;// WORD Subsystem;
			nextRead += 2;// WORD DllCharacteristics;
			nextRead += 8;// ULONGLONG SizeOfStackReserve;
			nextRead += 8;// ULONGLONG SizeOfStackCommit;
			nextRead += 8;// ULONGLONG SizeOfHeapReserve;
			nextRead += 8;// ULONGLONG SizeOfHeapCommit;
			nextRead += 4;// DWORD LoaderFlags;
			nextRead += 4;// DWORD NumberOfRvaAndSizes;
			// IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
			//} IMAGE_OPTIONAL_HEADER64, *PIMAGE_OPTIONAL_HEADER64;
		} else {
			//invalid size
			throw new CorruptCoreException("Invalid IMAGE_OPTIONAL_HEADER size: \"" +
					optionalHeaderSize + "\" bytes @ " +
					Long.toHexString(imageOptionalHeaderSizeAddress));
		}
					
		//we should now be at the data directory
		//typedef struct _IMAGE_DATA_DIRECTORY
					
		//note that it is this first directory which we are interested in since
		//it is the export dir
		//read that pointer and size and calculate where to seek to to begin work
		//again
		int exportRVA = as.getIntAt(nextRead); // DWORD VirtualAddress;
		nextRead+=4;
		if (0 == exportRVA) {
			//this module has no exports so return empty
			return Collections.emptyList();
		}
		nextRead += 4; //int exportSize = dump.readInt(); // DWORD Size;
		nextRead = moduleLoadAddress + (exportRVA &0xFFFFFFFFL);
					
		// typedef struct _IMAGE_EXPORT_DIRECTORY
		nextRead += 4; //dump.readInt(); // ULONG Characteristics;
		nextRead += 4; //dump.readInt(); // ULONG TimeDateStamp;
		nextRead += 2; //dump.readShort(); // USHORT MajorVersion;
		nextRead += 2; //dump.readShort(); // USHORT MinorVersion;
		nextRead += 4; //dump.readInt(); // ULONG Name;
		nextRead += 4; //dump.readInt(); // ULONG Base;
		long numberOfFunctionsAddress = nextRead;
		int numberOfFunctions = as.getIntAt(numberOfFunctionsAddress); //ULONG NumberOfFunctions;
		nextRead+=4;
		int numberOfNames = as.getIntAt(nextRead); // ULONG NumberOfNames;
		nextRead+=4;
					
		//although it is theoretically possible for numberOfFunctions !=
		//numberOfNames, we have no notion of how to interpret that (all
		//documentation seems to assume that they must be equal) so it is a kind of
		//corruption
		if (numberOfFunctions < numberOfNames) {
			throw new
				CorruptCoreException("IMAGE_EXPORT_DIRECTORY NumberOfFunctions (" +
					numberOfFunctions + ") < NumberOfNames (" + numberOfNames + ") @ " +
					Long.toHexString(numberOfFunctionsAddress));
		}
		//Note: despite the fact that these are pointers, they appear to be 4
		//bytes in both 32-bit and 64-bit binaries
		long funcAddress = (as.getIntAt(nextRead) & 0xFFFFFFFFL); //PULONG *AddressOfFunctions;
		nextRead+=4;
		long nameAddress = (as.getIntAt(nextRead) & 0xFFFFFFFFL); //PULONG *AddressOfNames;
		nextRead+=4;
		long ordinalAddress = (as.getIntAt(nextRead) & 0xFFFFFFFFL); //PUSHORT *AddressOfNameOrdinals;
		nextRead+=4;
					
		int nameAddresses[] = new int[numberOfNames];
		nextRead = nameAddress + moduleLoadAddress;
		for (int x = 0; x < numberOfNames; x++) {
			nameAddresses[x] = as.getIntAt(nextRead);
			nextRead+=4;
		}

		//the function addresses after the first numberOfNames entries are not
		//addressable as symbols so this array could be made smaller if this is
		//ever found to be a problem (for the near-term, however, it seems more
		//correct to read them all since they should all be addressable)
		long addresses[] = new long[numberOfFunctions];
		nextRead = funcAddress + moduleLoadAddress;
		for (int x = 0; x < numberOfFunctions; x++) {
			addresses[x] = as.getIntAt(nextRead);
			nextRead+=4;
		}
					
		int ordinals[] = new int[numberOfNames];
		nextRead = ordinalAddress + moduleLoadAddress;
		for (int x = 0; x < numberOfNames; x++) {
			ordinals[x] = (0x0000FFFF & (int)as.getShortAt(nextRead));
			nextRead += 2;
		}
					
		String names[] = new String[numberOfNames];
		byte buffer[] = new byte[2048]; //Support symbols over 1Kb long. (We have seen some.)
		for (int x = 0; x < numberOfNames; x++) {
			nextRead = (nameAddresses[x] & 0xFFFFFFFFL) + moduleLoadAddress;
			Arrays.fill(buffer, (byte)0);
			int index = 0;
			byte thisByte = 0;
			do {
				thisByte = as.getByteAt(nextRead);
				nextRead+=1;
				buffer[index] = thisByte;
				index++;
			} while ((0 != thisByte) && (index < buffer.length));
			try {
				names[x] = new String (buffer, 0, index-1, "UTF-8");
			} catch (UnsupportedEncodingException e) {
				// UTF-8 will be supported.
			}
		}
	
		List<ISymbol> symbols = new ArrayList<ISymbol>(numberOfNames);
		for (int x = 0; x < numberOfNames; x++) {
			int index = ordinals[x];
			long relocatedFunctionAddress = addresses[index] + moduleLoadAddress;
			String functionName = names[x];
			ISymbol symbol = new Symbol(functionName,relocatedFunctionAddress);
			symbols.add(symbol);
		}
		return symbols;
	}
	
	/* Gather the runtime function list from the loaded dll/exe.
	 * Possibly this should be done in the unwind package but it's
	 * similar to the symbol gathering above so it makes sense to
	 * have it here.
	 */
	private List<RuntimeFunction> buildRuntimeFunctionList(MiniDumpReader dump,
			IAddressSpace as, long imageBase) throws CorruptDataException, CorruptCoreException {
		long moduleLoadAddress = imageBase;
		byte magic[] = new byte[2];
		as.getBytesAt(imageBase, magic);
		String magicStr = null;
		try {
			magicStr = new String(magic, "ASCII");
		} catch (UnsupportedEncodingException e) {
			throw new CorruptCoreException("Unable to decode magic number");
		}

		if (!magicStr.equals("MZ")) {
			throw new CorruptCoreException("Invalid image magic number: \"" + magicStr + "\" @ " + Long.toHexString(imageBase));
		}
		//look up the PE offset: it is base+3c
		int peInt = as.getIntAt(imageBase + 0x3cL);
		long peBase = (peInt & 0xFFFFFFFFL) + imageBase;
		// * typedef struct _IMAGE_NT_HEADERS {
	
		as.getBytesAt(peBase, magic); // DWORD Signature;
		try {
			magicStr = new String(magic, "UTF-8");
		} catch (UnsupportedEncodingException e) {
			// UTF-8 will be supported.
		}
		if (!magicStr.equals("PE")) {
			throw new CorruptCoreException("Invalid PE magic number: \"" + magicStr + "\" @ " + Long.toHexString(peBase));
		}
		long nextRead = peBase + 4;
		// IMAGE_FILE_HEADER FileHeader;
		// IMAGE_OPTIONAL_HEADER32 OptionalHeader;
					
	
		//cue us up to the optional header
		// typedef struct _IMAGE_FILE_HEADER {
		nextRead += 2; //short machine = dump.readShort(); // WORD Machine;
		nextRead += 2; //dump.readShort(); // WORD NumberOfSections;
		nextRead += 4; //dump.readInt(); // DWORD TimeDateStamp;
		nextRead += 4; //dump.readInt(); // DWORD PointerToSymbolTable;
		nextRead += 4; //dump.readInt(); // DWORD NumberOfSymbols;
		long imageOptionalHeaderSizeAddress = nextRead;
		short optionalHeaderSize = as.getShortAt(imageOptionalHeaderSizeAddress); // WORD SizeOfOptionalHeader;
		nextRead+= 2;
					
		nextRead += 2; //dump.readShort(); // WORD Characteristics;
		if (224 == optionalHeaderSize) {
			return null; // 32 bit processes don't need unwind information.
		//cue us up to the data directories
		} else if (240 == optionalHeaderSize) {
			//64-bit optional header
			// typedef struct _IMAGE_OPTIONAL_HEADER64 {
			short magicShort = as.getShortAt(nextRead);
			if (0x20b != magicShort) {
				throw new
				CorruptCoreException("Invalid IMAGE_OPTIONAL_HEADER64 magic number: \"0x"
						+ Integer.toHexString(0xFFFF & magicShort) + "\" @ " +
						Long.toHexString(nextRead));
			}
			nextRead += 2;// WORD Magic;
			nextRead += 1;// BYTE MajorLinkerVersion;
			nextRead += 1;// BYTE MinorLinkerVersion;
			nextRead += 4;// DWORD SizeOfCode;
			nextRead += 4;// DWORD SizeOfInitializedData;
			nextRead += 4;// DWORD SizeOfUninitializedData;
			nextRead += 4;// DWORD AddressOfEntryPoint;
			nextRead += 4;// DWORD BaseOfCode;
			nextRead += 8;// ULONGLONG ImageBase;
			nextRead += 4;// DWORD SectionAlignment;
			nextRead += 4;// DWORD FileAlignment;
			nextRead += 2;// WORD MajorOperatingSystemVersion;
			nextRead += 2;// WORD MinorOperatingSystemVersion;
			nextRead += 2;// WORD MajorImageVersion;
			nextRead += 2;// WORD MinorImageVersion;
			nextRead += 2;// WORD MajorSubsystemVersion;
			nextRead += 2;// WORD MinorSubsystemVersion;
			nextRead += 4;// DWORD Win32VersionValue;
			nextRead += 4;// DWORD SizeOfImage;
			nextRead += 4;// DWORD SizeOfHeaders;
			nextRead += 4;// DWORD CheckSum;
			nextRead += 2;// WORD Subsystem;
			nextRead += 2;// WORD DllCharacteristics;
			nextRead += 8;// ULONGLONG SizeOfStackReserve;
			nextRead += 8;// ULONGLONG SizeOfStackCommit;
			nextRead += 8;// ULONGLONG SizeOfHeapReserve;
			nextRead += 8;// ULONGLONG SizeOfHeapCommit;
			nextRead += 4;// DWORD LoaderFlags;
			nextRead += 4;// DWORD NumberOfRvaAndSizes;
			// IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
			//} IMAGE_OPTIONAL_HEADER64, *PIMAGE_OPTIONAL_HEADER64;
		} else {
			//invalid size
			throw new CorruptCoreException("Invalid IMAGE_OPTIONAL_HEADER size: \"" +
					optionalHeaderSize + "\" bytes @ " +
					Long.toHexString(imageOptionalHeaderSizeAddress));
		}
		// The exception data is index 3 in the directories.
		int IMAGE_DIRECTORY_ENTRY_EXCEPTION = 3;
		for( int i = 0; i < IMAGE_DIRECTORY_ENTRY_EXCEPTION; i++ ) {
			nextRead+=4;
			nextRead+=4;
		}
		int exceptionRVA = as.getIntAt(nextRead); // DWORD VirtualAddress;
		int exceptionSize = as.getIntAt(nextRead); // DWORD VirtualAddress;
			
		// Now walk the table of IMAGE_RUNTIME_FUNCTION_ENTRY's, each is 3 DWORDS so 3*4 12 bytes long.
		int entryCount = exceptionSize / 12;
		nextRead = moduleLoadAddress + (exceptionRVA &0xFFFFFFFFL);
		// In the dll's entryCount is exceptionSize / sizeOf(IMAGE_RUNTIME_FUNCTION_ENTRY's)
		// In the core files that doesn't appear to be true. Though they look like they are
		// null terminated.
		
		List<RuntimeFunction> rfList = new LinkedList<RuntimeFunction>();
		
		for( int i = 0; i < entryCount; i++) {
			// Create an entry in our list for each one of these.
			int start = as.getIntAt(nextRead);
			nextRead+=4;
			if( start == 0 ) {
				// Null entries appear to end the exception data.
				break;
			}
			int end = as.getIntAt(nextRead);
			nextRead+=4;
			int unwindAddress = as.getIntAt(nextRead);
			nextRead+=4;
			RuntimeFunction rf =  new RuntimeFunction(start, end, unwindAddress);
			rfList.add(rf);
		}
		
		return rfList;
	}
}
