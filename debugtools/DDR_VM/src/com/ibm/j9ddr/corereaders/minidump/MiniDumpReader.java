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
package com.ibm.j9ddr.corereaders.minidump;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.Properties;
import java.util.Set;
import java.util.TreeSet;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.imageio.stream.ImageInputStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.DataUnavailableException;
import com.ibm.j9ddr.corereaders.AbstractCoreReader;
import com.ibm.j9ddr.corereaders.ClosingFileReader;
import com.ibm.j9ddr.corereaders.CoreReader;
import com.ibm.j9ddr.corereaders.CorruptCoreException;
import com.ibm.j9ddr.corereaders.ICore;
import com.ibm.j9ddr.corereaders.ICoreFileReader;
import com.ibm.j9ddr.corereaders.InvalidDumpFormatException;
import com.ibm.j9ddr.corereaders.Platform;
import com.ibm.j9ddr.corereaders.memory.IAddressSpace;
import com.ibm.j9ddr.corereaders.memory.IMemory;
import com.ibm.j9ddr.corereaders.memory.IMemorySource;
import com.ibm.j9ddr.corereaders.memory.IModule;
import com.ibm.j9ddr.corereaders.memory.ProcessAddressSpace;
import com.ibm.j9ddr.corereaders.osthread.IOSThread;

public class MiniDumpReader extends AbstractCoreReader implements ICoreFileReader
{
	private static final Logger logger = Logger.getLogger(J9DDR_CORE_READERS_LOGGER_NAME);
	
	// For Windows 2000, XP and Server 2003, the process information block is located at address 0x20000. The
	// command line is stored in this structure as a unicode string. A unicode string looks like:
	// unsigned short length;
	// unsigned short maxLength;
	// pointer buffer;
	// The offset is different on 32-bit and 64-bit platforms due to
	// the difference in pointer sizes and alignment (affecting previous
	// elements in the information block).
	private final static int INFO_BLOCK_ADDRESS = 0x20000;
	private static final int COMMAND_LINE_ADDRESS_ADDRESS_32 = INFO_BLOCK_ADDRESS + 0x44;
	private static final int COMMAND_LINE_ADDRESS_ADDRESS_64 = INFO_BLOCK_ADDRESS + 0x78;
	private static final int COMMAND_LINE_LENGTH_ADDRESS_32 = INFO_BLOCK_ADDRESS + 0x40;
	private static final int COMMAND_LINE_LENGTH_ADDRESS_64 = INFO_BLOCK_ADDRESS + 0x70;

	public static final String WINDOWS_BUILDNO_PROPERTY = "os.windows.buildno";

	private int _numberOfStreams = 0;
	private long _streamDirectoryRva = 0;
	private short _processorArchitecture = 0;
	private int _windowsMajorVersion = 0;
	private Set<String> _additionalFileNames = new TreeSet<String>();
	private int _pid;
	
	private final Properties properties = new Properties();
	
	
	//Late-initialized stream lists
	private List<LateInitializedStream> threadStreams = new LinkedList<LateInitializedStream>();
	private List<ThreadInfoStream> threadInfoStreams = new LinkedList<ThreadInfoStream>();
	private List<MemoryInfoStream> memoryInfoStreams = new LinkedList<MemoryInfoStream>();
	private List<LateInitializedStream> moduleStreams = new LinkedList<LateInitializedStream>();
	
	private boolean modulesRead = false;
	private List<IModule> modules;
	private IModule executable;
	
	private List<IOSThread> threads;
	
	private List<IAddressSpace> addressSpaces;

	private static boolean _is64bit = false;

	public ICore processDump(File file) throws FileNotFoundException,
			InvalidDumpFormatException, IOException
	{
		// MiniDump files can be Little Endian only.
		this.coreFile = file;
		ClosingFileReader fileReader = new ClosingFileReader(file,
				ByteOrder.LITTLE_ENDIAN);
		if (!isMiniDump(fileReader)) {
			throw new InvalidDumpFormatException("The file "
					+ file.getAbsolutePath() + " is not a valid MiniDump file.");
		}
		setReader(fileReader);
		return this;
	}
	
	public ICore processDump(ImageInputStream in) throws FileNotFoundException,
		InvalidDumpFormatException, IOException
	{
		// MiniDump files can be Little Endian only.
		in.setByteOrder(ByteOrder.LITTLE_ENDIAN);
		this.coreFile = null;
		if (!isMiniDump(in)) {
			throw new InvalidDumpFormatException("The passed input stream is not a valid MiniDump file.");
		}
		setReader(in);
		return this;
	}

	public boolean validDump(byte[] data, long filesize)
	{
		try {
			return new String(data, 0, 4, "UTF-8").equalsIgnoreCase("mdmp");
		} catch (UnsupportedEncodingException e) {
			// UTF-8 will be supported 
			return false;
		}
	}
	
	public ICore processDump(String path) throws FileNotFoundException,
			InvalidDumpFormatException, IOException
	{
		File file = new File(path);
		// MiniDump files can be Little Endian only.
		this.coreFile = file;
		ClosingFileReader fileReader = new ClosingFileReader(file,
				ByteOrder.LITTLE_ENDIAN);
		if (!isMiniDump(fileReader)) {
			throw new InvalidDumpFormatException("The file "
					+ file.getAbsolutePath() + " is not a valid MiniDump file.");
		}
		setReader(fileReader);
		return this;
	}

	public DumpTestResult testDump(String path) throws IOException
	{
		if (! new File(path).exists()) {
			return DumpTestResult.FILE_NOT_FOUND;
		}
		
		byte header[] = CoreReader.getFileHeader(path);
		
		try {
			return new String(header, 0, 4, "ASCII").equalsIgnoreCase("mdmp") ? DumpTestResult.RECOGNISED_FORMAT : DumpTestResult.UNRECOGNISED_FORMAT;
		} catch (UnsupportedEncodingException e) {
			//Shouldn't happen
			throw new RuntimeException(e);
		}
	}
	
	public DumpTestResult testDump(ImageInputStream in) throws IOException
	{		
		byte header[] = CoreReader.getFileHeader(in);
		
		try {
			return new String(header, 0, 4, "ASCII").equalsIgnoreCase("mdmp") ? DumpTestResult.RECOGNISED_FORMAT : DumpTestResult.UNRECOGNISED_FORMAT;
		} catch (UnsupportedEncodingException e) {
			//Shouldn't happen
			throw new RuntimeException(e);
		}
	}

	@Override
	public void setReader(ImageInputStream reader) throws IOException
	{
		reader.setByteOrder(ByteOrder.LITTLE_ENDIAN); // make sure that the
														// reader is set to the
														// correct bte order for
														// this dump
		super.setReader(reader); // set the reader on the super class
		try {
			readCore(); // read in some preliminary data from the core file
		} catch (CorruptCoreException e) {
			// TODO add in logging of the original method
			throw new IOException("Failed to read the core : " + e.getMessage());
		}
	}

	private void readCore() throws IOException, CorruptCoreException
	{
		parseHeader();
		parseStreams();
	}

	private void parseHeader() throws IOException
	{
		seek(0);
		readBytes(4); // Ignore signature: MDMP
		readInt(); // Ignore version
		_numberOfStreams = readInt();
		_streamDirectoryRva = readInt();
		readInt(); // Ignore checkSum
		int timeAndDate = readInt();
		properties.setProperty(ICore.CORE_CREATE_TIME_PROPERTY, Long.toString((long)timeAndDate * 1000L));
	}

	private void parseStreams() throws IOException, CorruptCoreException
	{
		seek(_streamDirectoryRva);
		List<EarlyInitializedStream> earlyStreams = new LinkedList<EarlyInitializedStream>();
		for (int i = 0; i < _numberOfStreams; i++) {
			int streamType = readInt();
			int dataSize = readInt();
			// TODO: should location be a long?
			// long location = (readInt() << 32) + rva1;
			int location = readInt();

			switch (streamType) {
			case Stream.THREADLIST:
				threadStreams.add(new ThreadStream(dataSize, location));
				break;
			case Stream.MODULELIST:
				moduleStreams.add(new ModuleStream(dataSize, location));
				break;
			case Stream.SYSTEMINFO:
				//Needs to go at the front - we rely on it to tell us the 
				//pointer size
				earlyStreams.add(0, new SystemInfoStream(dataSize, location));
				break;
			case Stream.MEMORY64LIST:
				earlyStreams.add(new Memory64Stream(dataSize, location));
				break;
			case Stream.MISCINFO:
				earlyStreams.add(new MiscInfoStream(dataSize, location));
				break;
			case Stream.THREADINFO:
				threadInfoStreams.add(new ThreadInfoStream(dataSize, location));
				break;
			case Stream.MEMORYINFO:
				memoryInfoStreams.add(new MemoryInfoStream(dataSize, location));
				break;
			}
		}

		for (EarlyInitializedStream entry : earlyStreams) {
			entry.readFrom(this);
			int ptrSize = entry.readPtrSize(this);
			if (0 != ptrSize) {
				_is64bit = ptrSize == 64 ? true : false;
			}
		}
		
		for (MemoryInfoStream entry : memoryInfoStreams) {
			entry.readFrom(this, is64Bit(), _memoryRanges);
		}
		
		// CMVC 168026: 32 bit dumps taken from within the VM on 64 bit machines are fine due to API redirection. 
		// Taking the dump from the Task Manager (or other methods) uses a 64 bit process to make the dump, and so
		// identify the processor architecture as 64bit, despite a 32 bit process. Sniff the module list
		// for WOW64 DLLs, and if found go back to 32 bits. There are 3 WOW64 DLLs + the paths of kernel32 
		// and user32, so any WOW64 process will have at least these 5 loaded. 
		if (_is64bit) {
			int wowCount = 0;
			for (IModule module : getModules()) {
				try {
					String modName = module.getName().toLowerCase();
					if (modName.contains("syswow64")) {
						wowCount++;
					}
				} catch (CorruptDataException e) {
					// Silently ignore for now, and report when the user actually wants to read the modules.
				}
			}

			if (wowCount > 5) {
				_is64bit = false;
				// A side effect of loading the modules above is that we must (later) reconstruct the address spaces.
				addressSpaces = null;
			}
		}
	}

		
	String getCommandLine() throws CorruptDataException, DataUnavailableException {
		
		// For dumps from Windows Vista, Server 2008 and later, we have no way to find the command line
		if ( _windowsMajorVersion >= 6 ) {
			throw new DataUnavailableException("Command line not available from Windows core dump");
		}
		
		// For dumps from older Windows versions, we can get the command line from the known PEB location
		String commandLine = null;
		
		try {
			IMemory memory = getMemory();
			short length;
			
			long commandAddress;
			
			if (is64Bit()) {
				length = memory.getShortAt(COMMAND_LINE_LENGTH_ADDRESS_64);
				commandAddress = memory.getLongAt(COMMAND_LINE_ADDRESS_ADDRESS_64);
			} else {
				length = memory.getShortAt(COMMAND_LINE_LENGTH_ADDRESS_32);
				commandAddress = 0xFFFFFFFF & memory.getIntAt(COMMAND_LINE_ADDRESS_ADDRESS_32);
			}
			
			if (commandAddress == 0) {
				throw new DataUnavailableException("Command line not in core file");
			}
			
			byte[] buf = new byte[length];
			memory.getBytesAt(commandAddress,buf);
			// FIXME: Should UTF-16LE be hard coded here? Is the encoding platform
			//specific?
			commandLine = new String(buf, "UTF-16LE");
			
			return commandLine;
		} catch (Exception e) {
			//throw a corrupt data exception as we expect the command line to be present so DataUnavailable is not appropriate
			throw new CorruptDataException("Failed to read command line from core file", e);
		}
	}

	private static boolean isMiniDump(ImageInputStream f) throws IOException
	{
		f.seek(0);
		byte[] signature = new byte[4];
		f.readFully(signature);
		return new String(signature, "UTF-8").equalsIgnoreCase("mdmp");
	}

	public boolean is64Bit()
	{
		return _is64bit;
	}

	protected void setProcessorArchitecture(short processorArchitecture,
			String procSubtype, int numberOfProcessors)
	{
		_processorArchitecture = processorArchitecture;
		properties.setProperty(ICore.PROCESSOR_COUNT_PROPERTY, Integer.toString(numberOfProcessors));
		if (processorArchitecture == Stream.PROCESSOR_ARCHITECTURE_INTEL) {
			properties.setProperty(ICore.PROCESSOR_TYPE_PROPERTY, "x86");
		} else if (processorArchitecture == Stream.PROCESSOR_ARCHITECTURE_AMD64) {
			properties.setProperty(ICore.PROCESSOR_TYPE_PROPERTY, "AMD64");
		} else {
			properties.setProperty(ICore.PROCESSOR_TYPE_PROPERTY, "Unknown: " + processorArchitecture);
		}
		
		properties.setProperty(ICore.PROCESSOR_SUBTYPE_PROPERTY, procSubtype);
	}

	protected void setWindowsType(byte type, int major, int minor, int buildNo)
	{
		_windowsMajorVersion = major;
		
		properties.setProperty(ICore.SYSTEM_TYPE_PROPERTY, decodeWindowsVersionNumber(major, minor, buildNo,type));
		properties.setProperty(ICore.SYSTEM_SUBTYPE_PROPERTY, major + "." + minor + " build " + buildNo);
		properties.setProperty(WINDOWS_BUILDNO_PROPERTY, Integer.toString(buildNo));
	}

	private String decodeWindowsVersionNumber(int major, int minor, int buildNo, byte productType)
	{
		switch (major) {
			case 5:
				switch (minor) {
				case 0:
					return "Windows 2000";
				case 1:
					return "Windows XP";
				case 2:
					return "Windows Server 2003";
				}
				break;
			case 6:
				switch (minor) {
				case 0:
					switch (productType) {
					case 1:
						return "Windows Vista";
					case 3:
						return "Windows Server 2008";
					}
				case 1:
					switch (productType) {
					case 1:
						return "Windows 7";
					case 3:
						return "Windows Server 2008 R2";
					}
				}
				break;
		}
		return "Unknown Windows variant: " + major + "." + minor + "." + buildNo + " type " + productType;
	}

	public void setProcessID(int pid)
	{
		_pid = pid;
	}

	public void addModule(String moduleName)
	{
		_additionalFileNames.add(moduleName);
	}

	protected short getProcessorArchitecture()
	{
		return _processorArchitecture;
	}

	protected void setMemorySources(Collection<? extends IMemorySource> ranges)
	{
		_memoryRanges = ranges;
	}

	public String getDumpFormat()
	{
		return "minidump";
	}

	public List<IAddressSpace> getAddressSpaces()
	{
		if (null == addressSpaces) {
			ProcessAddressSpace addressSpace = new WindowsProcessAddressSpace(
				is64Bit() ? 8 : 4, ByteOrder.LITTLE_ENDIAN, this);
			
			addressSpace.addMemorySources(_memoryRanges);
			
			addressSpaces = new ArrayList<IAddressSpace>(1);
			addressSpaces.add(addressSpace);
		}

		return addressSpaces;
	}
	
	private IMemory getMemory()
	{
		return getAddressSpaces().get(0);
	}
	
	public Platform getPlatform()
	{
		return Platform.WINDOWS;
	}

	public IModule getExecutable()
	{
		readModulesInfo();
		
		return executable;
	}
	
	public List<IModule> getModules()
	{
		readModulesInfo();
		return modules;
	}

	private void readModulesInfo()
	{
		if (! modulesRead) {
			modules = new LinkedList<IModule>();
			
			for (LateInitializedStream stream : moduleStreams) {
				try {
					stream.readFrom(this, this.getAddressSpaces().get(0), is64Bit());
				} catch (RuntimeException ex) {
					throw ex;
				} catch (Exception ex) {
					logger.logp(Level.WARNING,"MiniDumpReader","readModulesInfo","Problem processing module stream",ex);
				}
			}
			
			modulesRead = true;
		}
	}

	//Callbacks from ModuleStream
	void setExecutable(IModule module)
	{
		executable = module;
	}
	
	void addLibrary(IModule module)
	{
		modules.add(module);
	}
	
	public int getPid() {
		return _pid;
	}

	public List<IOSThread> getThreads()
	{
		if (threads == null) {
			threads = new LinkedList<IOSThread>();
			for (LateInitializedStream stream : threadStreams) {
				try {
					stream.readFrom(this, this.getAddressSpaces().get(0), is64Bit());
				} catch (RuntimeException ex) {
					throw ex;
				} catch (Exception ex) {
					logger.logp(Level.WARNING,"MiniDumpReader","getThreads","Problem processing thread stream",ex);
				}
			}
			/* Add thread info (may or may not be in the dump) */
			/* Relies on threads having been initialized as it adds properties so must be done second. */
			for (ThreadInfoStream stream : threadInfoStreams) {
				try {
					stream.readFrom(this, this.getAddressSpaces().get(0), is64Bit(), threads);
				} catch (RuntimeException ex) {
					throw ex;
				} catch (Exception ex) {
					logger.logp(Level.WARNING,"MiniDumpReader","getThreads","Problem processing thread info stream",ex);
				}
			}
		}
		
		return Collections.unmodifiableList(threads);
	}

	void addThread(IOSThread thread)
	{
		threads.add(thread);
	}

	public Properties getProperties()
	{
		return properties;
	}

}
