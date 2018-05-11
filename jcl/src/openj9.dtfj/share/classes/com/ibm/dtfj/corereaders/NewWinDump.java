/*[INCLUDE-IF Sidecar18-SE]*/
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
package com.ibm.dtfj.corereaders;

import java.io.IOException;

import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Date;
import java.util.Iterator;
import java.util.List;
import java.util.Properties;
import java.util.Set;
import java.util.TreeSet;
import java.util.Vector;

import javax.imageio.stream.ImageInputStream;

import com.ibm.dtfj.addressspace.IAbstractAddressSpace;

public abstract class NewWinDump extends CoreReaderSupport {
	public String _processorSubtypeDescription;
	protected boolean _is64Bit;
	
	private static class MiniDump extends NewWinDump {
		private static abstract class Stream {
			protected final static int PROCESSOR_ARCHITECTURE_INTEL = 0;
			// PROCESSOR_ARCHITECTURE_MIPS = 1;
			// PROCESSOR_ARCHITECTURE_ALPHA = 2;
			// PROCESSOR_ARCHITECTURE_PPC = 3;
			// PROCESSOR_ARCHITECTURE_SHX = 4;
			// PROCESSOR_ARCHITECTURE_ARM = 5;
			protected final static int PROCESSOR_ARCHITECTURE_IA64 = 6;
			protected final static int PROCESSOR_ARCHITECTURE_ALPHA64 = 7;
			// PROCESSOR_ARCHITECTURE_MSIL = 8;
			protected final static int PROCESSOR_ARCHITECTURE_AMD64 = 9;
			protected final static int PROCESSOR_ARCHITECTURE_IA32_ON_WIN64 = 10;

			// Stream types
			// UNUSED			= 0;
			// RESERVED0		= 1;
			// RESERVED1		= 2;
			private final static int THREADLIST		= 3;
			private final static int MODULELIST		= 4;
			// MEMORYLIST		= 5;
			// EXCEPTIONLIST	= 6;
			private final static int SYSTEMINFO		= 7;
			// THREADEXLIST	= 8;
			private final static int MEMORY64LIST	= 9;
			private final static int MISCINFO		= 15;

			public static Stream create(int streamType, int dataSize, int location) {
				switch (streamType) {
				case THREADLIST: 
					return new ThreadStream(dataSize, location);
				case MODULELIST: 
					return new ModuleStream(dataSize, location);
				case SYSTEMINFO: 
					return new SystemInfoStream(dataSize, location);
				case MEMORY64LIST: 
					return new Memory64Stream(dataSize, location);
				case MISCINFO: 
					return new MiscInfoStream(dataSize, location);
				default: 
					return null;
				}
			}
			private int _dataSize;

			private long _location;

			protected Stream(int dataSize, long location) {
				_dataSize = dataSize;
				_location = location;
			}
			protected int getDataSize() {
				return _dataSize;
			}

			protected long getLocation() {
				return _location;
			}
			
			/**
			 * This is part of a hack which allows us to find out the native word size of a Windows MiniDump since it doesn't seem to expose that
			 * in any nice way.
			 * 
			 * @return 0, unless this is a stream which can determine the value, in that case it returns the value (32 or 64)
			 */
			public int readPtrSize(ImageInputStream stream)
			{
				return 0;
			}
			
			public abstract void readFrom(MiniDump dump) throws IOException;
			
			public abstract void readFrom(MiniDump dump, Builder builder, Object addressSpace, IAbstractAddressSpace memory, boolean is64Bit) throws IOException;
		}

		private static class MiscInfoStream extends Stream {
			private final static int MISC1_PROCESS_ID = 1;

			public MiscInfoStream(int dataSize, int location) {
				super(dataSize, location);
			}
			
			public void readFrom(MiniDump dump) throws IOException {
				// Do nothing
			}

			public void readFrom(MiniDump dump, Builder builder, Object addressSpace, IAbstractAddressSpace memory, boolean is64Bit) throws IOException {
				dump.coreSeek(getLocation());
				if (getDataSize() < 4)
					return;
				int infoSize = dump.coreReadInt();
				if (infoSize > getDataSize())
					infoSize = getDataSize();
				infoSize = infoSize / 4 - 1;
				if (infoSize <= 0)
					return;
				int[] misc = new int[infoSize];
				for (int i = 0; i < infoSize; i++)
					misc[i] = dump.coreReadInt();
				int flags = misc[0];
				if ((flags & MISC1_PROCESS_ID) != 0 && infoSize > 1)
					dump.setProcessID(String.valueOf(misc[1]));
				// TODO are we interested in exposing process times information?
			}
		}

		private static class Memory64Stream extends Stream {
			public Memory64Stream(int dataSize, int location) {
				super(dataSize, location);
			}
			
			public void readFrom(MiniDump dump) throws IOException {
				long location = getLocation();
				dump.coreSeek(location);
				long numberOfMemoryRanges = dump.coreReadLong();
				long baseAddress = dump.coreReadLong();
				List memoryRanges = new ArrayList();
				MemoryRange memoryRange = null;
				for (int i = 0; i < numberOfMemoryRanges; i++) {
					long start = dump.is64Bit() ? dump.coreReadLong(): dump.coreReadLong() & 0x00000000FFFFFFFFL;
					long size =  dump.is64Bit() ? dump.coreReadLong(): dump.coreReadLong() & 0x00000000FFFFFFFFL;
					if (null == memoryRange) {
						// Allocate the first memory range starting at baseAddress in the dump file
						memoryRange = new MemoryRange(start, baseAddress, size, 0, false, false, true);
					} else if (memoryRange.getVirtualAddress() + memoryRange.getSize() == start) {
						// Combine contiguous regions
						memoryRange = new MemoryRange(memoryRange.getVirtualAddress(),
													  memoryRange.getFileOffset(),
													  memoryRange.getSize() + size, 0, false, false, true);
					} else {
						// Add the previous MemoryRange and start the next one
						memoryRanges.add(memoryRange);
						memoryRange = new MemoryRange(start,
								  					  memoryRange.getFileOffset() + memoryRange.getSize(),
								  					  size, 0, false, false, true);
					}
				}
				
				if (null != memoryRange)
					memoryRanges.add(memoryRange);
				
				dump.setMemoryRanges(memoryRanges);
			}

			public void readFrom(MiniDump dump, Builder builder, Object addressSpace, IAbstractAddressSpace memory, boolean is64Bit) throws IOException {
				// Do nothing
			}
		}
		
		private static class ModuleStream extends Stream {

			public ModuleStream(int dataSize, int location) {
				super(dataSize, location);
			}

			public void readFrom(MiniDump dump) throws IOException {
				// Do nothing
			}

			public void readFrom(MiniDump dump, Builder builder, Object addressSpace, IAbstractAddressSpace memory, boolean is64Bit) throws IOException {
				class Module {
					int nameAddress = -1;
					long imageBaseAddress = -1;
					Properties properties = new Properties();
				}
				
				dump.coreSeek(getLocation());
				int numberOfModules = dump.coreReadInt();
				Module[] modules = new Module[numberOfModules];
				for (int i = 0; i < numberOfModules; i++) {
					modules[i] = new Module();
					modules[i].imageBaseAddress = dump.coreReadLong();
					int imageSize = dump.coreReadInt();
					int checksum = dump.coreReadInt();
					int timeDateStamp = dump.coreReadInt();
					modules[i].nameAddress = dump.coreReadInt();
					int versionInfoDwSignature = dump.coreReadInt();
					int versionInfoDwStrucVersion = dump.coreReadInt();
					int versionInfoDwFileVersionMS = dump.coreReadInt();
					int versionInfoDwFileVersionLS = dump.coreReadInt();
					int versionInfoDwProductVersionMS = dump.coreReadInt();
					int versionInfoDwProductVersionLS = dump.coreReadInt();
					int versionInfoDwFileFlagsMask = dump.coreReadInt();
					int versionInfoDwFileFlags = dump.coreReadInt();
					int versionInfoDwFileOS = dump.coreReadInt();
					int versionInfoDwFileType = dump.coreReadInt();
					int versionInfoDwFileSubtype = dump.coreReadInt();
					int versionInfoDwFileDateMS = dump.coreReadInt();
					int versionInfoDwFileDateLS = dump.coreReadInt();

					dump.coreReadInt(); // Ignore cvRecordDataSize
					dump.coreReadInt(); // Ignore cvRecordDataRva
					dump.coreReadInt(); // Ignore miscRecordDataSize
					dump.coreReadInt(); // Ignore miscRecordDataRva
					
					dump.coreReadLong(); // Ignore reserved0
					dump.coreReadLong(); // Ignore reserved1
					
					//populate this module with some interesting properties
					modules[i].properties.setProperty("imageSize", Integer.toHexString(imageSize));
					modules[i].properties.setProperty("checksum", Integer.toHexString(checksum));
					//note that, in Windows, timeDateStamp is seconds since Dec 31, 1969 @4:00 PM so we have to add 8 hours to have it match up with UCT and then multiply by 1000 to make it the milliseconds that Java expects
					modules[i].properties.setProperty("timeDateStamp", (new Date(1000L * (timeDateStamp + (3600L * 8L)))).toString());
					modules[i].properties.setProperty("versionInfoDwSignature", Integer.toHexString(versionInfoDwSignature));

					modules[i].properties.setProperty("versionInfoDwStrucVersion", Integer.toHexString(versionInfoDwStrucVersion));
					modules[i].properties.setProperty("versionInfoDwFileVersionMS", Integer.toHexString(versionInfoDwFileVersionMS));
					modules[i].properties.setProperty("versionInfoDwFileVersionLS", Integer.toHexString(versionInfoDwFileVersionLS));
					modules[i].properties.setProperty("versionInfoDwProductVersionMS", Integer.toHexString(versionInfoDwProductVersionMS));
					modules[i].properties.setProperty("versionInfoDwProductVersionLS", Integer.toHexString(versionInfoDwProductVersionLS));
					modules[i].properties.setProperty("versionInfoDwFileFlagsMask", Integer.toHexString(versionInfoDwFileFlagsMask));
					modules[i].properties.setProperty("versionInfoDwFileFlags", Integer.toHexString(versionInfoDwFileFlags));
					modules[i].properties.setProperty("versionInfoDwFileOS", Integer.toHexString(versionInfoDwFileOS));
					modules[i].properties.setProperty("versionInfoDwFileType", Integer.toHexString(versionInfoDwFileType));
					modules[i].properties.setProperty("versionInfoDwFileSubtype", Integer.toHexString(versionInfoDwFileSubtype));
					modules[i].properties.setProperty("versionInfoDwFileDateMS", Integer.toHexString(versionInfoDwFileDateMS));
					modules[i].properties.setProperty("versionInfoDwFileDateLS", Integer.toHexString(versionInfoDwFileDateLS));
				}
				
				//NOTE:  by this point, we are no longer in the module stream so it makes sense to use the memory argument to seek more freely
				for (int i = 0; i < modules.length; i++) {
					String moduleName = getModuleName(dump, modules[i].nameAddress);
					short magic;
					try {
						magic = memory.getShortAt(0, modules[i].imageBaseAddress);
						if (0x5A4D != magic) {
							System.err.println("Magic number was: " + Integer.toHexString(0xFFFF & magic));
						}
					} catch (MemoryAccessException e1) {
						// TODO Auto-generated catch block
						e1.printStackTrace();
					}
					long e_lfanewAddress = modules[i].imageBaseAddress + 0x3c;
					//load the e_lfanew since that is the load-address-relative location of the PE Header
					List sections = new Vector();
					try {
						long e_lfanew = 0xFFFFFFFFL & memory.getIntAt(0, e_lfanewAddress);
						//push us to the start of the PE header
						long readingAddress = e_lfanew + modules[i].imageBaseAddress;
						if (0 != e_lfanew) {
							int pemagic = memory.getIntAt(0, readingAddress);	readingAddress += 4;
							if (0x4550 != pemagic) {
								System.err.println("PE Magic is: \"" + Integer.toHexString(pemagic));
							}
							readingAddress += 2;	//machine
							short numberOfSections = memory.getShortAt(0, readingAddress); readingAddress +=2;
							readingAddress += 4;	//date/time stamp
							readingAddress += 4;	//pointerToSymbolTable
							readingAddress += 4;	//numberOfSymbols
							short sizeOfOptionalHeader = memory.getShortAt(0, readingAddress);	readingAddress += 2;
							readingAddress += 2;	//characteristics
							
							//now skip ahead to after the optional header since that is where section headers can be found
							readingAddress = modules[i].imageBaseAddress + e_lfanew + 24 /* sizeof PE header */ + (0xFFFFL & sizeOfOptionalHeader);
							for (int j = 0; j < numberOfSections; j++) {

								byte rawName[] = new byte[8];
								memory.getBytesAt(0, readingAddress, rawName);	readingAddress += 8;
								long virtualSize = memory.getIntAt(0, readingAddress);	readingAddress += 4;
								long virtualAddress = memory.getIntAt(0, readingAddress);	readingAddress += 4;
								readingAddress+=4;	//sizeOfRawData
								readingAddress+=4;	//pointerToRawData
								readingAddress+=4;	//pointerToRelocations
								readingAddress+=4;	//pointerToLineNumbers
								readingAddress+=2;	//numberOfRelocations
								readingAddress+=2;	//numberOfLineNumbers
								readingAddress+=4;	//section_characteristics
								
								long relocatedAddress = virtualAddress + modules[i].imageBaseAddress;
								Object oneSection = builder.buildModuleSection(addressSpace, new String(rawName).trim(), relocatedAddress, relocatedAddress + virtualSize);
								sections.add(oneSection);
							}
						}
						Object module = builder.buildModule(moduleName,
														  modules[i].properties,
														  sections.iterator(),
														  _buildSymbolIterator(dump, memory, builder, addressSpace, modules[i].imageBaseAddress, modules[i].imageBaseAddress),
														  modules[i].imageBaseAddress);
						if (moduleName.toLowerCase().endsWith(".exe")) {
							dump.setExecutable(module);
						} else {
							dump.addLibrary(module);
						}
					} catch (MemoryAccessException e) {
						//this needs to be here in order to not fail completely whenever we encounter a strange record
					}
					dump.addModule(moduleName);
				}
			}

			/**
			 * Looks at the module loaded at imageBase in the given dump and produces a symbol table for which it returns the iterator.
			 * @param dump
			 * @param builder
			 * @param imageBase
			 * @param moduleLoadAddress
			 * @return
			 */
			private Iterator _buildSymbolIterator(MiniDump dump, IAbstractAddressSpace memory, Builder builder, Object addressSpace, long imageBase, long moduleLoadAddress)
			{
				try {
					byte magic[] = new byte[2];
					memory.getBytesAt(0, imageBase, magic);
					String stuff = new String(magic);
					if (!stuff.equals("MZ")) {
						return Collections.singleton(builder.buildCorruptData(addressSpace, "Invalid image magic number: \"" + stuff + "\"", imageBase)).iterator();
					}
					//look up the PE offset:  it is base+3c
					int peInt = memory.getIntAt(0, imageBase + 0x3cL);
					long peBase = (peInt & 0xFFFFFFFFL) + imageBase;
    
					memory.getBytesAt(0, peBase, magic);
					stuff = new String(magic);
					if (!stuff.equals("PE")) {
						return Collections.singleton(builder.buildCorruptData(addressSpace, "Invalid PE magic number: \"" + stuff + "\"", peBase)).iterator();
					}
					long nextRead = peBase + 4;
					

					//cue us up to the optional header
					nextRead += 2;	// Machine;
					nextRead += 2;	// NumberOfSections;
					nextRead += 4;	// TimeDateStamp;
					nextRead += 4;	// PointerToSymbolTable;
					nextRead += 4;	// NumberOfSymbols;
					long imageOptionalHeaderSizeAddress = nextRead;
					short optionalHeaderSize = memory.getShortAt(0, imageOptionalHeaderSizeAddress);
					nextRead+= 2;
					
					nextRead += 2;	// Characteristics;
					//cue us up to the first data directory
					if (224 == optionalHeaderSize) {
						//32-bit optional header
						short magicShort = memory.getShortAt(0, nextRead);
						if (0x10b != magicShort) {
							return Collections.singleton(builder.buildCorruptData(addressSpace, "Invalid IMAGE_OPTIONAL_HEADER magic number: \"0x" + Integer.toHexString(0xFFFF & magicShort) + "\"", nextRead)).iterator();
						}
						nextRead += 2;	// Magic;
						nextRead += 1;	// MajorLinkerVersion;
						nextRead += 1;	// MinorLinkerVersion;
						nextRead += 4;	// SizeOfCode;
						nextRead += 4;	// SizeOfInitializedData;
						nextRead += 4;	// SizeOfUninitializedData;
						nextRead += 4;	// AddressOfEntryPoint;
						nextRead += 4;	// BaseOfCode;
						nextRead += 4;	// BaseOfData;
						nextRead += 4;	// ImageBase;
						nextRead += 4;	// SectionAlignment;
						nextRead += 4;	// FileAlignment;
						nextRead += 2;	// MajorOperatingSystemVersion;
						nextRead += 2;	// MinorOperatingSystemVersion;
						nextRead += 2;	// MajorImageVersion;
						nextRead += 2;	// MinorImageVersion;
						nextRead += 2;	// MajorSubsystemVersion;
						nextRead += 2;	// MinorSubsystemVersion;
						nextRead += 4;	// Win32VersionValue;
						nextRead += 4;	// SizeOfImage;
						nextRead += 4;	// SizeOfHeaders;
						nextRead += 4;	// CheckSum;
						nextRead += 2;	// Subsystem;
						nextRead += 2;	// DllCharacteristics;
						nextRead += 4;	// SizeOfStackReserve;
						nextRead += 4;	// SizeOfStackCommit;
						nextRead += 4;	// SizeOfHeapReserve;
						nextRead += 4;	// SizeOfHeapCommit;
						nextRead += 4;	// LoaderFlags;
						nextRead += 4;	// NumberOfRvaAndSizes;
					} else if (240 == optionalHeaderSize) {
						//64-bit optional header
						short magicShort = memory.getShortAt(0, nextRead);
						if (0x20b != magicShort) {
							return Collections.singleton(builder.buildCorruptData(addressSpace, "Invalid IMAGE_OPTIONAL_HEADER64 magic number: \"0x" + Integer.toHexString(0xFFFF & magicShort) + "\"", nextRead)).iterator();
						}
						nextRead += 2;// Magic;
						nextRead += 1;// MajorLinkerVersion;
						nextRead += 1;// MinorLinkerVersion;
						nextRead += 4;// SizeOfCode;
						nextRead += 4;// SizeOfInitializedData;
						nextRead += 4;// SizeOfUninitializedData;
						nextRead += 4;// AddressOfEntryPoint;
						nextRead += 4;// BaseOfCode;
						nextRead += 8;// ImageBase;
						nextRead += 4;// SectionAlignment;
						nextRead += 4;// FileAlignment;
						nextRead += 2;// MajorOperatingSystemVersion;
						nextRead += 2;// MinorOperatingSystemVersion;
						nextRead += 2;// MajorImageVersion;
						nextRead += 2;// MinorImageVersion;
						nextRead += 2;// MajorSubsystemVersion;
						nextRead += 2;// MinorSubsystemVersion;
						nextRead += 4;// Win32VersionValue;
						nextRead += 4;// SizeOfImage;
						nextRead += 4;// SizeOfHeaders;
						nextRead += 4;// CheckSum;
						nextRead += 2;// Subsystem;
						nextRead += 2;// DllCharacteristics;
						nextRead += 8;// SizeOfStackReserve;
						nextRead += 8;// SizeOfStackCommit;
						nextRead += 8;// SizeOfHeapReserve;
						nextRead += 8;// SizeOfHeapCommit;
						nextRead += 4;// LoaderFlags;
						nextRead += 4;// NumberOfRvaAndSizes;
					} else {
						//invalid size
						return Collections.singleton(builder.buildCorruptData(addressSpace, "Invalid IMAGE_OPTIONAL_HEADER size: \"" + optionalHeaderSize + "\" bytes", imageOptionalHeaderSizeAddress)).iterator();
					}
					
					//we should now be at the data directory
					
					//note that it is this first directory which we are interested in since it is the export dir
					//read that pointer and size and calculate where to seek to to begin work again
					int exportRVA = memory.getIntAt(0, nextRead);
					nextRead+=4;
					if (0 == exportRVA) {
						//this module has no exports so return empty
						return Collections.EMPTY_LIST.iterator();
					}
					nextRead += 4;	// Size;
					nextRead = moduleLoadAddress + (exportRVA & 0xFFFFFFFFL);//dump.seekToAddress(moduleLoadAddress + (exportRVA & 0xFFFFFFFFL));
					
					nextRead += 4;	// Characteristics;
					nextRead += 4;	// TimeDateStamp;
					nextRead += 2;	// MajorVersion;
					nextRead += 2;	// MinorVersion;
					nextRead += 4;	// Name;
					nextRead += 4;	// Base;
					long numberOfFunctionsAddress = nextRead;
					int numberOfFunctions = memory.getIntAt(0, numberOfFunctionsAddress);
					nextRead+=4;
					int numberOfNames = memory.getIntAt(0, nextRead);
					nextRead+=4;
					
					//although it is theoretically possible for numberOfFunctions != numberOfNames, we have no notion of how to interpret that (all documentation seems to assume that they must be equal) so it is a kind of corruption
					if (numberOfFunctions < numberOfNames) {
						return Collections.singleton(builder.buildCorruptData(addressSpace, "IMAGE_EXPORT_DIRECTORY NumberOfFunctions (" + numberOfFunctions + ") < NumberOfNames (" + numberOfNames + ")", numberOfFunctionsAddress)).iterator();						
					}
					//Note:  despite the fact that these are pointers, they appear to be 4 bytes in both 32-bit and 64-bit binaries
					long funcAddress = (memory.getIntAt(0, nextRead) & 0xFFFFFFFFL);
					nextRead+=4;
					long nameAddress = (memory.getIntAt(0, nextRead) & 0xFFFFFFFFL);
					nextRead+=4;
					long ordinalAddress = (memory.getIntAt(0, nextRead) & 0xFFFFFFFFL);
					nextRead+=4;
					
					int nameAddresses[] = new int[numberOfNames];
					nextRead = nameAddress + moduleLoadAddress;
					for (int x = 0; x < numberOfNames; x++) {
						nameAddresses[x] = memory.getIntAt(0, nextRead);
						nextRead+=4;
					}
					
					//the function addresses after the first numberOfNames entries are not addressable as symbols so this array could be made smaller if this is ever found to be a problem (for the near-term, however, it seems more correct to read them all since they should all be addressable)
					long addresses[] = new long[numberOfFunctions];
					nextRead = funcAddress + moduleLoadAddress;
					for (int x = 0; x < numberOfFunctions; x++) {
						addresses[x] = memory.getIntAt(0, nextRead);
						nextRead+=4;
					}
					
					short ordinals[] = new short[numberOfNames];
					nextRead = ordinalAddress + moduleLoadAddress;
					for (int x = 0; x < numberOfNames; x++) {
						ordinals[x] = memory.getShortAt(0, nextRead);
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
							thisByte = memory.getByteAt(0, nextRead);
							nextRead+=1;
							buffer[index] = thisByte;
							index++;
						} while (0 != thisByte && (index < buffer.length));
						names[x] = new String (buffer, 0, index-1);
					}

					Vector symbols = new Vector();
					for (int x = 0; x < numberOfNames; x++) {
						short index = ordinals[x];
						long relocatedFunctionAddress = addresses[index] + moduleLoadAddress;
						String functionName = names[x];
						Object symbol = builder.buildSymbol(addressSpace, functionName, relocatedFunctionAddress);
						symbols.add(symbol);
					}
					return symbols.iterator();
				} catch (MemoryAccessException e) {
					// this exception is unexpected so return an iterator over a corrupt data object if this happens
					return Collections.singleton(builder.buildCorruptData(addressSpace, e.getMessage(), e.getAddress())).iterator();
				}
			}

			/**
			 * Get the module name as a String from the UTF-16LE data held in the dump
			 * @throws IOException 
			 */    
			private String getModuleName(MiniDump dump, int position) throws IOException {
				dump.coreSeek(position);
				int length = dump.coreReadInt();
				if (length <= 0 || 512 <= length)
					length = 512;
				byte[] nameWide = dump.coreReadBytes(length);
				return new String(nameWide, "UTF-16LE");
			}
		}
		
		private static class SystemInfoStream extends Stream {
			public SystemInfoStream(int dataSize, int location) {
				super(dataSize, location);
			}

			public void readFrom(MiniDump dump) throws IOException {
				dump.coreSeek(getLocation());
				short processorArchitecture = dump.coreReadShort();
				short processorLevel = dump.coreReadShort();
				short processorRevision = dump.coreReadShort();
				//this probably shouldn't be formatted into a string here but it is the only way that the data is currently used so it simplifies things a bit
				// we need an updated key of what processorLevel means.  It is something like "Pentium 4", for example
				//we can, however, assume that processorRevision is in the xxyy layout so the bytes can be interpreted
				byte model = (byte)((processorRevision >> 8) & 0xFF);
				byte stepping = (byte)(processorRevision & 0xFF);
				String procSubtype = "Level " + processorLevel + " Model " + model + " Stepping " + stepping;
				
				dump.setProcessorArchitecture(processorArchitecture, procSubtype);
				
				dump.coreReadByte(); // skip NumberOfProcessors
				dump.coreReadByte(); // skip ProductType
				int majorVersion = dump.coreReadInt();
				dump.setWindowsMajorVersion(majorVersion);
			}

			public int readPtrSize(ImageInputStream f)
			{
				short processorArchitecture = 0;
				
				try {
					f.seek(getLocation());
					byte[] buffer = new byte[2];
					f.readFully(buffer);
					processorArchitecture = (short)(((0xFF & buffer[1]) << 8) | (0xFF & buffer[0]));
				} catch (IOException e) {
					return 0;
				}
				return (PROCESSOR_ARCHITECTURE_AMD64 == processorArchitecture || 
						PROCESSOR_ARCHITECTURE_IA64 == processorArchitecture ||
						PROCESSOR_ARCHITECTURE_ALPHA64 == processorArchitecture ||
						PROCESSOR_ARCHITECTURE_IA32_ON_WIN64 == processorArchitecture) ? 64 : 32;
			}

			public void readFrom(MiniDump dump, Builder builder, Object addressSpace, IAbstractAddressSpace memory, boolean is64Bit) throws IOException {
				// Do nothing
			}
		}

		private static class ThreadStream extends Stream {
			public ThreadStream(int dataSize, int location) {
				super(dataSize, location);
			}

			public void readFrom(MiniDump dump) throws IOException {
				// Do nothing
			}

			public void readFrom(MiniDump dump, Builder builder, Object addressSpace, IAbstractAddressSpace memory, boolean is64Bit) throws IOException {
				
				//get the thread ID of the failing thread
				long tid = -1;
				if (null != dump._j9rasReader) {
					try {
						tid = dump._j9rasReader.getThreadID();
					} catch (UnsupportedOperationException uoe) {
						//do nothing
					} catch (MemoryAccessException mae) {
						//do nothing
					} catch (CorruptCoreException cce) {
						// do nothing
					}
				}
				
				dump.coreSeek(getLocation());
				int numberOfThreads = dump.coreReadInt();
				List threads = new ArrayList();
				for (int i = 0; i < numberOfThreads; i++) {
					dump.coreSeek(getLocation() + 4 + i * 48);
					int threadId = dump.coreReadInt();
					dump.coreReadInt(); // Ignore suspendCount
					int priorityClass = dump.coreReadInt();
					int priority = dump.coreReadInt();
					dump.coreReadLong(); // Ignore teb
					long stackStart = dump.coreReadLong();
					long stackSize = 0xFFFFFFFFL & dump.coreReadInt();
					long stackRva = 0xFFFFFFFFL & dump.coreReadInt();
					long contextDataSize = 0xFFFFFFFFL & dump.coreReadInt();
					long contextRva = 0xFFFFFFFFL & dump.coreReadInt();

					Properties properties = new Properties();
					properties.setProperty("priorityClass", String.valueOf(priorityClass));
					properties.setProperty("priority", String.valueOf(priority));
					
					List registers = readRegisters(dump, builder, contextRva, contextDataSize);
					long stackEnd = stackStart + stackSize;
					Object section = builder.buildStackSection(addressSpace, stackStart, stackEnd);
					List frames = readStackFrames(dump, builder, addressSpace, stackStart, stackEnd, stackRva, registers, memory, is64Bit);
				    //TODO: find the signal number!
				    int signalNumber = 0;
				    Object thread = builder.buildThread(String.valueOf(threadId), registers.iterator(),
														Collections.singletonList(section).iterator(),
														frames.iterator(), properties, signalNumber);
					threads.add(thread);
					if (-1 != tid && tid == threadId) {
						dump._failingThread = thread;
					}
				}
				
				dump.addThreads(threads);

//				for (int i=0;i<numberOfThreads;i++) {
//					GenericThread gt = new GenericThread(("0x"+ Integer.toHexString(minidumpThreads[i].threadId)),
//							minidumpThreads[i].stackStartOfMemoryRange,
//							minidumpThreads[i].stackDataSize,
//							minidumpThreads[i].stackRva);
//					Register ebpReg = gt.getNamedRegister("ebp");
//					Register espReg = gt.getNamedRegister("esp");
//					analyseStack(gt,ebpReg,espReg);
//				}
			}

			private List readStackFrames(MiniDump dump, Builder builder, Object addressSpace, long stackStart, long stackEnd, long stackRva, List registers, IAbstractAddressSpace memory, boolean is64Bit)
			{
				// Windows stack frames can be read by following the ebp to the base of the stack: old ebp is at ebp(0) and and return address to parent context is ebp(sizeof(void*))
				// 1) find the ebp in the register file
				long ebp = builder.getValueOfNamedRegister(registers, "ebp");
				long eip = builder.getValueOfNamedRegister(registers, "eip");
				long esp = builder.getValueOfNamedRegister(registers, "esp");
				List frames = new ArrayList();
				
				// eip may be -1 if we're in a system call
				if (-1 == eip && stackStart <= esp && esp < stackEnd) {
					try {
						eip = memory.getPointerAt(0, esp);
					} catch (MemoryAccessException e) {
						// ignore
					}
				}
				
				int bytesPerPointer = (is64Bit ? 8 : 4);
				
				// Add the current frame first.  If ebp doesn't point into the stack, try esp.
				if (stackStart <= ebp && ebp < stackEnd) {
					frames.add(builder.buildStackFrame(addressSpace, ebp, eip));
				} else if (stackStart <= esp && esp < stackEnd) {
					frames.add(builder.buildStackFrame(addressSpace, esp, eip));
					ebp = esp + bytesPerPointer;
				}
				
				while (stackStart <= ebp && ebp < stackEnd) {
					try {
						long newBP = memory.getPointerAt(0, ebp);
						long retAddress = memory.getPointerAt(0, ebp + bytesPerPointer);
						frames.add(builder.buildStackFrame(addressSpace, newBP, retAddress));
						ebp = newBP;
					} catch (MemoryAccessException e) {
						//stop trying to read meaningless memory
						break;
					}
				}
				return frames;
			}

			private List readRegisters(MiniDump dump, Builder builder, long contextRva, long contextDataSize) throws IOException {
				switch (dump.getProcessorArchitecture()) {
				case PROCESSOR_ARCHITECTURE_AMD64:
					return readAmd64Registers(dump, builder, contextRva, contextDataSize);
				case PROCESSOR_ARCHITECTURE_INTEL:
					return readIntelRegisters(dump, builder, contextRva, contextDataSize);
				default:
					return Collections.EMPTY_LIST;
				}
			}

			private List readIntelRegisters(MiniDump dump, Builder builder, long contextRva, long contextDataSize) throws IOException {
				// We capture segment registers, flags, integer registers and instruction pointer
				dump.coreSeek(contextRva + 140);
				List registers = new ArrayList();
				registers.add(builder.buildRegister("gs", Integer.valueOf(dump.coreReadInt())));
				registers.add(builder.buildRegister("fs", Integer.valueOf(dump.coreReadInt())));
				registers.add(builder.buildRegister("es", Integer.valueOf(dump.coreReadInt())));
				registers.add(builder.buildRegister("ds", Integer.valueOf(dump.coreReadInt())));
				registers.add(builder.buildRegister("edi", Integer.valueOf(dump.coreReadInt())));
				registers.add(builder.buildRegister("esi", Integer.valueOf(dump.coreReadInt())));
				registers.add(builder.buildRegister("ebx", Integer.valueOf(dump.coreReadInt())));
				registers.add(builder.buildRegister("edx", Integer.valueOf(dump.coreReadInt())));
				registers.add(builder.buildRegister("ecx", Integer.valueOf(dump.coreReadInt())));
				registers.add(builder.buildRegister("eax", Integer.valueOf(dump.coreReadInt())));
				registers.add(builder.buildRegister("ebp", Integer.valueOf(dump.coreReadInt())));
				registers.add(builder.buildRegister("eip", Integer.valueOf(dump.coreReadInt())));
				registers.add(builder.buildRegister("cs", Integer.valueOf(dump.coreReadInt())));
				registers.add(builder.buildRegister("flags", Integer.valueOf(dump.coreReadInt())));
				registers.add(builder.buildRegister("esp", Integer.valueOf(dump.coreReadInt())));
				registers.add(builder.buildRegister("ss", Integer.valueOf(dump.coreReadInt())));
				return registers;
			}

			private List readAmd64Registers(MiniDump dump, Builder builder, long contextRva, long contextDataSize) throws IOException {
				// We capture segment registers, flags, integer registers and instruction pointer
				dump.coreSeek(contextRva + 56);
				List registers = new ArrayList();
				registers.add(builder.buildRegister("cs", Short.valueOf(dump.coreReadShort())));
				registers.add(builder.buildRegister("ds", Short.valueOf(dump.coreReadShort())));
				registers.add(builder.buildRegister("es", Short.valueOf(dump.coreReadShort())));
				registers.add(builder.buildRegister("fs", Short.valueOf(dump.coreReadShort())));
				registers.add(builder.buildRegister("gs", Short.valueOf(dump.coreReadShort())));
				registers.add(builder.buildRegister("ss", Short.valueOf(dump.coreReadShort())));
				registers.add(builder.buildRegister("flags", Integer.valueOf(dump.coreReadInt())));
				dump.coreSeek(contextRva + 120);
				registers.add(builder.buildRegister("eax", Long.valueOf(dump.coreReadLong())));
				registers.add(builder.buildRegister("ecx", Long.valueOf(dump.coreReadLong())));
				registers.add(builder.buildRegister("edx", Long.valueOf(dump.coreReadLong())));
				registers.add(builder.buildRegister("ebx", Long.valueOf(dump.coreReadLong())));
				registers.add(builder.buildRegister("esp", Long.valueOf(dump.coreReadLong())));
				registers.add(builder.buildRegister("ebp", Long.valueOf(dump.coreReadLong())));
				registers.add(builder.buildRegister("esi", Long.valueOf(dump.coreReadLong())));
				registers.add(builder.buildRegister("edi", Long.valueOf(dump.coreReadLong())));
				registers.add(builder.buildRegister("r8", Long.valueOf(dump.coreReadLong())));
				registers.add(builder.buildRegister("r9", Long.valueOf(dump.coreReadLong())));
				registers.add(builder.buildRegister("r10", Long.valueOf(dump.coreReadLong())));
				registers.add(builder.buildRegister("r11", Long.valueOf(dump.coreReadLong())));
				registers.add(builder.buildRegister("r12", Long.valueOf(dump.coreReadLong())));
				registers.add(builder.buildRegister("r13", Long.valueOf(dump.coreReadLong())));
				registers.add(builder.buildRegister("r14", Long.valueOf(dump.coreReadLong())));
				registers.add(builder.buildRegister("r15", Long.valueOf(dump.coreReadLong())));
				registers.add(builder.buildRegister("ip", Long.valueOf(dump.coreReadLong())));
				return registers;
			}
		}

		private List _directory = null;
		private int _numberOfStreams = 0;
		private long _streamDirectoryRva = 0;
		private int _timeAndDate = 0;
		private List _memoryRanges = null;
		private short _processorArchitecture = 0;
		private int _windowsMajorVersion = 0;
		private Object _executable = null;
		private List _threads = new ArrayList();
		private Object _failingThread = null;
		private List _libraries = new ArrayList();
		private Set _additionalFileNames = new TreeSet();
		private String _pid = null;

		public MiniDump(ImageInputStream stream) throws IOException {
			super(stream, _pointerSizeForMiniDump(stream));
			parseHeader();
			parseStreams();
		}

		private static int _pointerSizeForMiniDump(ImageInputStream stream) throws IOException
		{
			stream.seek(0);
			byte[] sig = new byte[4];
			stream.readFully(sig); // Ignore signature: MDMP
			littleEndianReadInt(stream); // Ignore version
			int numberOfStreams = littleEndianReadInt(stream);
			int streamDirectoryRva = littleEndianReadInt(stream);
			littleEndianReadInt(stream); // Ignore checkSum
			littleEndianReadInt(stream); //ignore time and date
			
			stream.seek(streamDirectoryRva);
			Vector entries = new Vector();
			for (int i = 0; i < numberOfStreams; i++) {
				int streamType = littleEndianReadInt(stream);
				int dataSize = littleEndianReadInt(stream);
				// TODO: should location be a long?
				// long location = (readInt() << 32) + rva1;
				int location = littleEndianReadInt(stream);
				Stream entry = Stream.create(streamType, dataSize, location);
				// (CMVC 105355) the core may include streams which we aren't interested in interpreting so we will fail to create a Stream object, in those cases
				if (null != entry) {
					entries.add(entry);
				}
			}
			
			Iterator accessEntries = entries.iterator();
			while (accessEntries.hasNext()) {
				Stream entry = (Stream) accessEntries.next();
				int ptr = entry.readPtrSize(stream);
				if (0 != ptr) {
					return ptr;
				}
			}
			return 0;
		}

		public void addModule(String moduleName) {
			_additionalFileNames.add(moduleName);
		}
		
		public Iterator getAdditionalFileNames() {
			return _additionalFileNames.iterator();
		}

		public void addLibrary(Object module) {
			_libraries.add(module);
		}

		public void setExecutable(Object module) {
			_executable = module;
		}

		public void setProcessID(String pid) {
			_pid = pid;
		}

		public void addThreads(List threads) {
			_threads.addAll(threads);
		}

		protected void setProcessorArchitecture(short processorArchitecture, String procSubtype) {
			_processorArchitecture = processorArchitecture;
			_processorSubtypeDescription = procSubtype;
		}

		protected void setWindowsMajorVersion(int majorVersion) {
			_windowsMajorVersion = majorVersion;
		}

		private long getCreationTime() {
			// Windows time_t is a signed 32-bit integer recording seconds,
			// rather than milliseconds, since the epoch (00:00:00 GMT, January
			// 1, 1970)
			return _timeAndDate * 1000L;
		}
		
		protected short getProcessorArchitecture() {
			return _processorArchitecture;
		}
		
		protected void setMemoryRanges(List memoryRanges) {
			_memoryRanges = memoryRanges;
		}

		private void parseHeader() throws IOException {
			coreSeek(0);
			coreReadBytes(4); // Ignore signature: MDMP
			coreReadInt(); // Ignore version
			_numberOfStreams = coreReadInt();
			_streamDirectoryRva = coreReadInt();
			coreReadInt(); // Ignore checkSum
			_timeAndDate = coreReadInt();
		}

		private void parseStreams() throws IOException {
			coreSeek(_streamDirectoryRva);
			_directory = new ArrayList();
			for (int i = 0; i < _numberOfStreams; i++) {
				int streamType = coreReadInt();
				int dataSize = coreReadInt();
				// TODO: should location be a long?
				// long location = (readInt() << 32) + rva1;
				int location = coreReadInt();
				Stream entry = Stream.create(streamType, dataSize, location);
				if (null != entry)
					_directory.add(entry);
			}
			
			for (Iterator iter = _directory.iterator(); iter.hasNext();) {
				Stream entry = (Stream) iter.next();
				entry.readFrom(this);
			}
		}

		public void extract(Builder builder) {
			try {
				Object addressSpace = builder.buildAddressSpace("Windows MiniDump Address Space", 0);
				for (Iterator iter = _directory.iterator(); iter.hasNext();) {
					Stream entry = (Stream) iter.next();
					entry.readFrom(this, builder, addressSpace, getAddressSpace(), is64Bit());
				}
				String commandLine = _extractCommandLine();
				String pid = _pid;
				if (null != _j9rasReader) {
					try {
						pid = Long.toString(_j9rasReader.getProcessID());
					} catch (UnsupportedOperationException use) {
					}
				}
				 
				builder.buildProcess(addressSpace, pid, commandLine, getEnvironmentVariables(builder),
									 _failingThread, _threads.iterator(), _executable,
									 _libraries.iterator(), _is64Bit ? 64 : 32);
			} catch (IOException e) {
				// TODO throw exception or notify builder?
			} catch (MemoryAccessException e) {
				// TODO throw exception or notify builder?
			} catch (CorruptCoreException cce) {
				// TODO throw exception or notify builder?
			}
			builder.setOSType("Windows");
			builder.setCPUType("x86");
			builder.setCPUSubType(_processorSubtypeDescription);
			builder.setCreationTime(getCreationTime());
		}

		// For Windows XP/Server 2003 and earlier, the process information block is located at address 0x20000.  The
		// command line is stored in this structure as a unicode string.  A
		// unicode string looks like:
		//		unsigned short length;
		//		unsigned short maxLength;
		//		pointer buffer;
		// The offset is different on 32-bit and 64-bit platforms due to
		// the difference in pointer sizes and alignment (affecting previous
		// elements in the information block).
		private final static int INFO_BLOCK_ADDRESS = 0x20000;
		private static final int COMMAND_LINE_ADDRESS_ADDRESS_32 = INFO_BLOCK_ADDRESS + 0x44;
		private static final int COMMAND_LINE_ADDRESS_ADDRESS_64 = INFO_BLOCK_ADDRESS + 0x78;
		private static final int COMMAND_LINE_LENGTH_ADDRESS_32 = INFO_BLOCK_ADDRESS + 0x40;
		private static final int COMMAND_LINE_LENGTH_ADDRESS_64 = INFO_BLOCK_ADDRESS + 0x70;
		
		private String _extractCommandLine()
		{
			// Windows Vista, Server 2008 and later, we have no way to find the command line
			if ( _windowsMajorVersion >= 6 ) {
				return "[unknown]";
			}
			
			// Older Windows versions, we can get the command line from the known PEB location
			String commandLine = null;
			
			try {
				IAbstractAddressSpace memory = getAddressSpace();
				if (null != memory) {
				short length = memory.getShortAt(0, is64Bit() ? COMMAND_LINE_LENGTH_ADDRESS_64 : COMMAND_LINE_LENGTH_ADDRESS_32);
				long commandAddress = memory.getPointerAt(0, is64Bit() ? COMMAND_LINE_ADDRESS_ADDRESS_64 : COMMAND_LINE_ADDRESS_ADDRESS_32);
				byte[] buf = new byte[length];
				memory.getBytesAt(0, commandAddress, buf);
				commandLine = new String(buf, "UTF-16LE");
				}
			} catch (MemoryAccessException e) {
			} catch (UnsupportedEncodingException e) {
			} catch (Exception e) {
				//rather than fail silently or throw a runtime exception when there is the possibility of continuing 
				//just record that the cmd line is unknown - as per other comments there should be a way to indicate to the builder
				//that an error has occurred and allow it to determine if processing should continue.
				//Additionally the returned command line is not subsequently used by jextract
				return "[unknown]";
			}
			return commandLine;
		}
		
		public boolean is64Bit()
		{
			return _is64Bit;
		}
		
		protected MemoryRange[] getMemoryRangesAsArray() {
			if (null == _memoryRanges) {
				return null;
			}
			return (MemoryRange[])_memoryRanges.toArray(new MemoryRange[_memoryRanges.size()]);
		}
	}

	private static class UserDump extends NewWinDump {
		private int _dataOffset;
		private List _memoryRanges;
		private int _regionCount;
		private int _regionOffset;

		public UserDump(ImageInputStream stream) throws IOException {
			super(stream, 32);	//XXX - make this the correct pointer size if we ever care about user dumps
			parseHeader();
			parseMemory();
		}

		public long getCreationTime() {
			// TODO figure out how to extract creation time
			return 0;
		}

		/* (non-Javadoc)
		 * @see com.ibm.dtfj.corereaders.Dump#getMemoryRanges()
		 */
		public Iterator getMemoryRanges()
		{
			return _memoryRanges.iterator();
		}

		private void parseHeader() throws IOException {
			coreSeek(0);
			coreReadBytes(8); // Ignore signature
			coreReadInt(); // Ignore unknown 1
			coreReadInt(); // Ignore unknown 2
			coreReadInt(); // Ignore unknown 3
			
			coreReadInt(); // Ignore threadCount
			coreReadInt(); // Ignore moduleCount
			_regionCount = coreReadInt();
			coreReadInt(); // Ignore contextOffset
			coreReadInt(); // Ignore moduleOffset
			_dataOffset = coreReadInt();
			_regionOffset = coreReadInt();
			coreReadInt(); // Ignore debugOffset
			coreReadInt(); // Ignore threadOffset
			coreReadInt(); // Ignore versionInfoOffset
		}

		private void parseMemory() throws IOException {
			coreSeek(_regionOffset);
			_memoryRanges = new ArrayList();
			MemoryRange memoryRange = null;
			for (int i = 0; i < _regionCount; i++) {
				long start = coreReadInt();
				coreReadInt(); // Ignore allocationBase
				coreReadInt(); // Ignore allocationProtect
				int size = coreReadInt();
				coreReadInt(); // Ignore state
				coreReadInt(); // Ignore protect
				coreReadInt(); // Ignore type
				
				if (null == memoryRange) {
					// Allocate the first memory range starting at _dataOffset in the dump file
					memoryRange = new MemoryRange(start, _dataOffset, size);
				} else if (memoryRange.getVirtualAddress() + memoryRange.getSize() == start) {
					// Combine contiguous regions
					memoryRange = new MemoryRange(memoryRange.getVirtualAddress(),
												  memoryRange.getFileOffset(),
												  memoryRange.getSize() + size);
				} else {
					// Add the previous MemoryRange and start the next one
					_memoryRanges.add(memoryRange);
					memoryRange = new MemoryRange(start,
							  					  memoryRange.getFileOffset() + memoryRange.getSize(),
							  					  size);
				}
			}
			
			if (null != memoryRange)
				_memoryRanges.add(memoryRange);
		}

		public void extract(Builder builder) {
			// TODO: figure out how to extract process information from User Dumps
			
			String pid = "";
			if (null != _j9rasReader) {
				try {
					pid = Long.toString(_j9rasReader.getProcessID());
				} catch (Exception e) {
				}
			}
			
			try {
				builder.buildProcess(builder.buildAddressSpace("Windows UserDump Address Space", 0), pid, "",
									 getEnvironmentVariables(builder), null,
									 Collections.EMPTY_LIST.iterator(), null,
									 Collections.EMPTY_LIST.iterator(), 32);
			} catch (MemoryAccessException e) {
				// TODO throw exception or notify builder?
			}
		}

		public Iterator getAdditionalFileNames() {
			// TODO Auto-generated method stub
			return Collections.EMPTY_LIST.iterator();
		}

		protected MemoryRange[] getMemoryRangesAsArray() {
			return (MemoryRange[])_memoryRanges.toArray(new MemoryRange[_memoryRanges.size()]);
		}

		protected boolean is64Bit()
		{
			//XXX : Make this correct if we ever care about user dumps
			return false;
		}
	}

	public static ICoreFileReader dumpFromFile(ImageInputStream stream) throws IOException {
		assert isSupportedDump(stream);
		
		if (isMiniDump(stream))
			return new MiniDump(stream);
		else if (isUserDump(stream))
			return new UserDump(stream);

		return null;
	}
	private static boolean isMiniDump(ImageInputStream stream) throws IOException {
		stream.seek(0);
		byte[] signature = new byte[4];
		stream.readFully(signature);
		return new String(signature).equalsIgnoreCase("mdmp");
	}

	public static boolean isSupportedDump(ImageInputStream stream) throws IOException {
		return isMiniDump(stream) || isUserDump(stream);
	}

	private static boolean isUserDump(ImageInputStream stream) throws IOException {
		stream.seek(0);
		byte[] signature = new byte[8];
		stream.readFully(signature);
		return new String(signature).equalsIgnoreCase("userdump");
	}

	private NewWinDump(ImageInputStream stream, int bitsPerPointer) throws IOException {
		super(new LittleEndianDumpReader(stream, 64 == bitsPerPointer));
		_is64Bit = (64 == bitsPerPointer);
	}

	protected Properties getEnvironmentVariables(Builder builder) throws MemoryAccessException
	{
		if (0 == builder.getEnvironmentAddress()) return null;
		Properties envVars = new Properties();
		// envAddress points to a pointer to a null terminated list of addresses which point to strings of form x=y
		IAbstractAddressSpace memory = getAddressSpace();
		if (null != memory) {
		long address = memory.getPointerAt(0, builder.getEnvironmentAddress());//(0xFFFFFFFFL & getIntAt(0, builder.getEnvironmentAddress()));
		long target = memory.getPointerAt(0, address);//(0xFFFFFFFF & getIntAt(0, address));
		while (0 != target) {
			//assemble the string at this address
			//XXX: does it make sense to treat these as ASCII chars?
			StringBuffer buffer = new StringBuffer();
			byte oneByte = memory.getByteAt(0, target);
			while (0 != oneByte) {
				buffer.append(new String(new byte[] {oneByte}));
				target += 1;
				oneByte = memory.getByteAt(0, target);
			}
			//now buffer is the x=y string
			String pair = buffer.toString();
			int equal = pair.indexOf('=');
			String variable = pair.substring(0, equal);
			String value = pair.substring(equal+1, pair.length());
			envVars.put(variable, value);
			target = memory.getPointerAt(0, address);//(0xFFFFFFFFL & getIntAt(0, address));
			address += is64Bit() ? 8 : 4;
			}
		}
		return envVars;
	}
	
	public boolean isLittleEndian()
	{
		return true;
	}
	
	private static int littleEndianReadInt(ImageInputStream stream) throws IOException
	{
		byte[] buffer = new byte[4];
		stream.readFully(buffer);
		return ((0xFF & buffer[3]) << 24)
				| ((0xFF & buffer[2]) << 16)
				| ((0xFF & buffer[1]) << 8)
				| (0xFF & buffer[0]);
	}
}
