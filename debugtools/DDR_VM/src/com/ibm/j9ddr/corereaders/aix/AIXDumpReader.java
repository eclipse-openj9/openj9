/*******************************************************************************
 * Copyright (c) 2009, 2019 IBM Corp. and others
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
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Properties;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.imageio.stream.ImageInputStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.AbstractCoreReader;
import com.ibm.j9ddr.corereaders.ClosingFileReader;
import com.ibm.j9ddr.corereaders.ICore;
import com.ibm.j9ddr.corereaders.ILibraryDependentCore;
import com.ibm.j9ddr.corereaders.ILibraryResolver;
import com.ibm.j9ddr.corereaders.IModuleFile;
import com.ibm.j9ddr.corereaders.InvalidDumpFormatException;
import com.ibm.j9ddr.corereaders.LibraryDataSource;
import com.ibm.j9ddr.corereaders.LibraryResolverFactory;
import com.ibm.j9ddr.corereaders.Platform;
import com.ibm.j9ddr.corereaders.memory.DumpMemorySource;
import com.ibm.j9ddr.corereaders.memory.IAddressSpace;
import com.ibm.j9ddr.corereaders.memory.IMemoryRange;
import com.ibm.j9ddr.corereaders.memory.IMemorySource;
import com.ibm.j9ddr.corereaders.memory.IModule;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.corereaders.memory.ISymbol;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;
import com.ibm.j9ddr.corereaders.memory.MemoryRange;
import com.ibm.j9ddr.corereaders.memory.MissingFileModule;
import com.ibm.j9ddr.corereaders.memory.Module;
import com.ibm.j9ddr.corereaders.memory.UnbackedMemorySource;
import com.ibm.j9ddr.corereaders.osthread.IOSThread;
import com.ibm.j9ddr.corereaders.osthread.IRegister;
import com.ibm.j9ddr.corereaders.osthread.Register;

/**
 * @author ccristal
 */
public abstract class AIXDumpReader extends AbstractCoreReader implements ILibraryDependentCore
{
	private static final Logger logger = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);
	
	private static final long FAULTING_THREAD_OFFSET = 216; // offsetof(core_dumpx,
															// c_flt)
	protected static final int S64BIT = 0x00000001; // indicates a 64-bit
													// process
	
	private static final long CORE_FILE_VERSION_OFFSET = 4;
	
	//See /usr/include/core.h on an AIX box for the core structures
	private static final int CORE_DUMP_XX_VERSION = 0xFEEDDB2;
	private static final int CORE_DUMP_X_VERSION = 0xFEEDDB1;
	private static final long CORE_DUMP_X_PI_FLAGS_2_OFFSET = 0x420;

	private static final int POWER_RS1 = 0x0001;
	private static final int POWER_RSC = 0x0002;
	private static final int POWER_RS2 = 0x0004;
	private static final int POWER_601 = 0x0008;
	private static final int POWER_603 = 0x0020;
	private static final int POWER_604 = 0x0010;
	private static final int POWER_620 = 0x0040;
	private static final int POWER_630 = 0x0080;
	private static final int POWER_A35 = 0x0100;
	private static final int POWER_RS64II = 0x0200;
	private static final int POWER_RS64III = 0x0400;
	private static final int POWER_4 = 0x0800;
	// private static final int POWER_RS64IV = POWER_4;
	private static final int POWER_MPC7450 = 0x1000;
	private static final int POWER_5 = 0x2000;

	protected abstract Map<String, Number> readRegisters(long threadOffset)
			throws IOException;

	protected abstract int readLoaderInfoFlags() throws IOException;

	protected abstract long userInfoOffset();

	protected abstract long threadSize(long threadOffset);

	protected abstract int pointerSize();

	protected abstract long getStackPointerFrom(Map<String, Number> registers);

	protected abstract long getInstructionPointerFrom(
			Map<String, Number> registers);

	protected abstract long getLinkRegisterFrom(Map<String, Number> registers);

	protected abstract int sizeofTopOfStack();

	private int _implementation;
	private int _threadCount;
	private long _threadOffset;
	private long _loaderOffset;
	private long _loaderSize;
	private AIXProcessAddressSpace _process = new AIXProcessAddressSpace(is64Bit() ? 8 : 4, ByteOrder.BIG_ENDIAN, this);
	long _highestOffset = -1;
	private Properties props = new Properties();

	// addresses required to find the bottom of the main thread's stack (for
	// some reason called the "top of stack" in AIX documentation)
	private long _structTopOfStackVirtualAddress;

	private boolean _isTruncated = false;

	private ILibraryResolver resolver = null; 	//LibraryResolverFactory.getResolverForCoreFile(this.coreFile);
	private ArrayList<XCOFFReader> openFileTracker = new ArrayList<XCOFFReader>();
	
	protected static boolean isAIXDump(ClosingFileReader f) throws IOException
	{
		// There are no magic signatures in an AIX dump header. Some simple
		// validation of the header
		// allows a best guess about validity of the file.
		f.seek(8);
		long fdsinfox = f.readLong();
		long loader = f.readLong();
		long filesize = f.length();
		return fdsinfox > 0 && loader > 0 && loader > fdsinfox
				&& fdsinfox < filesize && loader < filesize;
	}

	public boolean validDump(byte[] data, long filesize)
	{
		//keep public method for backwards compatibility but delegate it to the static
		return isAIXDump(data, filesize);
	}

	protected void readCore() throws IOException
	{
		// Note that this structure is core_dumpx defined in
		// "/usr/include/sys/core.h"
		seek(0);
		readByte(); // Ignore signalNumber
		byte flags = readByte(); // CORE_TRUNC == 0x80
		if ((flags & 0x80) != 0) {
			_isTruncated = true;
		}
		readShort(); // Ignore entryCount
		readInt(); // Ignore version (core file format number)
		readLong(); // Ignore fdsinfox
		_loaderOffset = readLong();
		_loaderSize = readLong();
		_threadCount = readInt();
		readInt(); // Ignore padding
		_threadOffset = readLong();
		long segmentCount = readLong();
		long segmentOffset = readLong();

		// Stack - this appears to be special handling for the main thread first
		// attached to the address space
		// (this is helpful for us since that is how we can find command line
		// and env vars)
		long stackOffset = readLong();
		long stackVirtualAddress = readLong();
		long stackSize = readLong();
		_structTopOfStackVirtualAddress = stackVirtualAddress + stackSize - sizeofTopOfStack();

		IMemorySource memoryRange1 = new DumpMemorySource(stackVirtualAddress, stackSize, stackOffset, 0, this, "stack", false, false, false);
		addMemorySource(memoryRange1);

		// User data
		long dataOffset = readLong();
		long dataVirtualAddress = readLong();
		long dataSize = readLong();
		IMemorySource memoryRange = new DumpMemorySource(dataVirtualAddress, dataSize, dataOffset, this);
		addMemorySource(memoryRange);

		readLong(); // Ignore sdataVirtualAddress
		readLong(); // Ignore sdataSize

		long vmRegionCount = readLong();
		long vmRegionOffset = readLong();
		_implementation = readInt();
		readInt(); // Ignore padding
		readLong(); // Ignore checkpointRestartOffset
		readLong(); // Ignore unsigned long long c_extctx; (Extended Context
					// Table offset)

		readBytes(6 * 8); // Ignore padding (6 longs)

		// ignore struct thrdctx c_flt; (faulting thread's context) //this is
		// resolved later via that constant "FAULTING_THREAD_OFFSET"
		// ignore struct userx c_u; (copy of the user structure)
		// >> end of core_dumpx structure

		readVMRegions(vmRegionOffset, vmRegionCount);
		readSegments(segmentOffset, segmentCount);
		readLoaderInfoAsMemoryRanges();
		readModules();

		_isTruncated |= checkHighestOffset();
		createProperties();
	}
	
	/**
	 * Create the properties for this core file.
	 */
	private void createProperties() {
		props.put(ICore.SYSTEM_TYPE_PROPERTY, "AIX");		
		props.put(ICore.PROCESSOR_TYPE_PROPERTY, getCPUType());
		props.put(ICore.PROCESSOR_SUBTYPE_PROPERTY, getCPUSubType());
		props.put(ICore.CORE_CREATE_TIME_PROPERTY, getCreationTime());
	}

	public String getCPUType()
	{
		return "PowerPC";
	}

	public String getCPUSubType()
	{
		switch (_implementation) {
		case POWER_RS1:
			return "RS1";
		case POWER_RSC:
			return "RSC";
		case POWER_RS2:
			return "RS2";
		case POWER_601:
			return "601";
		case POWER_603:
			return "603";
		case POWER_604:
			return "604";
		case POWER_620:
			return "620";
		case POWER_630:
			return "630";
		case POWER_A35:
			return "A35";
		case POWER_RS64II:
			return "RS64-II";
		case POWER_RS64III:
			return "RS64-III";
		case POWER_4:
			return "POWER 4";
		case POWER_MPC7450:
			return "MPC7450";
		case POWER_5:
			return "POWER 5";
		default:
			return "";
		}
	}

	public long getCreationTime()
	{
		return 0;
	}

	private void addMemorySource(IMemorySource source)
	{
		if (source instanceof DumpMemorySource) {
			DumpMemorySource castedRange = (DumpMemorySource) source;
			if (castedRange.getFileOffset() > _highestOffset) {
				_highestOffset = castedRange.getFileOffset();
			}
		}
		
		_process.addMemorySource(source);
	}
	
	private boolean checkHighestOffset() throws IOException
	{
		if (_highestOffset != -1) {
			return !checkOffset(_highestOffset);
		} else {
			return false;
		}
	}

	private void readVMRegions(long offset, long count) throws IOException
	{
		seek(offset);
		for (int i = 0; i < count; i++) {
			long address = readLong();
			long size = readLong();
			long off = readLong();
			// Anonymously mapped area
			IMemorySource memoryRange = new DumpMemorySource(address, size, off, 0, this, "anon vm region");
			// memoryRange.setDescription("anon vm region");
			addMemorySource(memoryRange);
		}
	}

	private void readSegments(long offset, long count) throws IOException
	{
		seek(offset);
		for (int i = 0; i < count; i++) {
			long address = readLong();
			long size = readLong();
			long off = readLong();
			readInt(); // Ignore segmentFlags
			readInt(); // Ignore padding
			// Anonymously mapped area
			IMemorySource memoryRange = new DumpMemorySource(address, size, off, 0, this, "anon segment");
			addMemorySource(memoryRange);
		}
	}

	private void readLoaderInfoAsMemoryRanges() throws IOException
	{
		int next = 0;
		long current = _loaderOffset;
		do {
			current += next;
			seek(current);
			next = readInt();
			readLoaderInfoFlags(); // Ignore flags
			long dataOffset = readAddress();
			readAddress(); // Ignore textOrigin
			readAddress(); // Ignore textSize
			long dataVirtualAddress = readAddress();
			long dataSize = readAddress();
			// String name = readString();
			// String subname = readString();

			if (0 != dataOffset) {
				IMemorySource memoryRange = new DumpMemorySource(
						dataVirtualAddress, dataSize, dataOffset, this);
				// memoryRange.setDescription(name + " " +
				// Long.toHexString(textOrigin) + "-" +
				// Long.toHexString(textOrigin + textSize));
				addMemorySource(memoryRange);
			}
		} while (0 != next && current + next < _loaderOffset + _loaderSize);
	}

	protected abstract boolean is64Bit();

	protected abstract long readAddress() throws IOException;
	
	private boolean _userInfoLoaded = false;
	private int _pid;
	private int _argc;
	private long _argv;
	private long _environmentHandle;
	private final List<IModule> _modules = new LinkedList<IModule>();
	private IModule _executable;
	private IMemorySource _executableTextSection;
	
	private void loadUserInfo() throws IOException, MemoryFault
	{
		if (!_userInfoLoaded) {
			seek(userInfoOffset());
			_pid = readInt();
					
			// Get the process command line. For AIX dumps we find the registers in
			// the initial (top - this is to be consistent with AIX headers)
			// stack frame. Register 3 has the number of command line arguments
			//(argc), register 4 has
			// a pointer to a list of pointers to the command line arguments (argv),
			//and register 5 contains the pointer to the environment vars
			
			IProcess memory = getProcess();
			long workingAddress = _structTopOfStackVirtualAddress;
			int pointerSize = pointerSize() / 8;
			
			//Ignore GPR0-2
			workingAddress += (2 * pointerSize);
			long reg3 = memory.getPointerAt(workingAddress += pointerSize); //argc
			_argv = memory.getPointerAt(workingAddress += pointerSize); //argv
			_environmentHandle = memory.getPointerAt(workingAddress += pointerSize); //env
			_argc = (int)reg3; // truncate to integer
			_userInfoLoaded = true;
		}
	}

	String getCommandLine() throws CorruptDataException
	{
		try {
			loadUserInfo();
		} catch (IOException e1) {
			throw new CorruptDataException(e1);
		}
		
		IProcess memory = getProcess();
		int pointerSize = pointerSize() / 8;
		
		if (_argc > 100) {
			throw new CorruptDataException("Argc too high. Likely corrupt data. Argc=" + _argc + " structTopOfStackVirtualAddress = 0x" + Long.toHexString(_structTopOfStackVirtualAddress));
		}
		
		long[] addresses = new long[_argc];
		for (int i = 0; i < _argc; i++) {
			addresses[i] = memory.getPointerAt(_argv + (i * pointerSize));
		}
		
		StringBuffer commandLine = new StringBuffer();
		for (int i = 0; i < _argc; i++) {
			try
			{
				long startingAddress = addresses[i];
				long workingAddress = startingAddress;
				
				while (memory.getByteAt(workingAddress) != 0) {
					workingAddress++;
				}
				
				int stringLength = (int)(workingAddress - startingAddress);
				byte[] buffer = new byte[stringLength];
				memory.getBytesAt(startingAddress, buffer);
				commandLine.append(new String(buffer,"ASCII"));
				commandLine.append(" ");
			} catch (MemoryFault e) {
				commandLine.append(" <Fault reading argv[" + i + "] at 0x" + Long.toHexString(addresses[i]) + ">");
			} catch (UnsupportedEncodingException e) {
				throw new RuntimeException(e);
			}
		}
		
		return commandLine.toString();
	}

	//		
	// // NOTE modules must be read before thread information in order to
	// properly support stack traces.
	// // Thread information is required before building the process, so the
	// following order must be maintained:
	// // 1) Read module information.
	// // 2) Read thread information.
	// // 3) Build process.
	// List<J9DDRImageModule> modules = getModules(addressSpace);
	// J9DDRImageModule executable = null;
	// if (!modules.isEmpty()) {
	// executable = modules.get(0);
	// }
	// List<J9DDRImageThread> threads = readThreads(addressSpace);
	// //the current thread is parsed before the rest so it is put first
	// J9DDRImageThread currentThread = threads.get(0);
	//
	// // Get the process environment variables
	// Properties environment = new Properties();
	// //on AIX, we should trust the env var pointer found in the core and
	// ignore whatever the XML told us it is since this internal value is more
	// likely correct
	// environment = getEnvironmentVariables(reg5);
	//		
	// J9DDRImageProcess process = new J9DDRImageProcess(String.valueOf(pid),
	// commandLine, environment, currentThread, threads, executable, modules,
	// pointerSize());
	// process.setAddressSpace(addressSpace);
	// process.setMemory(getMemory());
	// return process;
	// }

	private void readModules() throws IOException
	{
		int next = 0;
		long current = _loaderOffset;
		
		if(coreFile == null) {
			resolver = LibraryResolverFactory.getResolverForCoreFile(_fileReader);
		} else {
			resolver = LibraryResolverFactory.getResolverForCoreFile(coreFile);
		}
		
		IProcess proc = getProcess();

		boolean firstModule = true;
		do {
			current += next;
			seek(current);
			next = readInt();
			readLoaderInfoFlags(); // Ignore flags
			readAddress(); // Ignore dataOffset
			long textVirtualAddress = readAddress();
			long textSize = readAddress();
			long dataVirtualAddress = readAddress();
			long dataSize = readAddress();
			String fileName = readString();
			String objectName = readString();
			String moduleName = fileName;
			if (0 < objectName.length()) {
				moduleName += "(" + objectName + ")";
			}
			loadModule(resolver, proc, textVirtualAddress, textSize,
					dataVirtualAddress, dataSize, fileName, objectName,
					moduleName, firstModule);
			
			firstModule = false;
		} while (0 != next && current + next < _loaderOffset + _loaderSize);
	}

	//Passback of executable path. If we couldn't load the executable the first time, try again now.
	public void executablePathHint(String path)
	{
		if (_executable instanceof MissingFileModule) {
			//Check the hint points at the same file name
			try {
				if (! getFileName(_executable.getName()).equals(getFileName(path))) {
					return;
				}
			} catch (CorruptDataException e1) {
				return;
			}
			
			//Remove placeholder memory source
			_process.removeMemorySource(_executableTextSection);
			
			//Remember the current placeholder in case this one fails too
			IModule currentExecutable = _executable;
			IMemorySource currentExecutableTextSection = _executableTextSection;
			
			ILibraryResolver resolver = LibraryResolverFactory.getResolverForCoreFile(this.coreFile);
			IProcess proc = getProcess();
			try {
				seek(_loaderOffset);
				readInt();
				readLoaderInfoFlags(); // Ignore flags
				readAddress(); // Ignore dataOffset
				long textVirtualAddress = readAddress();
				long textSize = readAddress();
				long dataVirtualAddress = readAddress();
				long dataSize = readAddress();
				readString(); // Disregard fileName from core
				String fileName = path;
				String objectName = readString();
				String moduleName = fileName;
				if (0 < objectName.length()) {
					moduleName += "(" + objectName + ")";
				}
				
				loadModule(resolver, proc, textVirtualAddress, textSize,
						dataVirtualAddress, dataSize, fileName, objectName,
						moduleName, true);
			} catch (IOException e) {
				//TODO handle
			}
			
			//If the executable is still a MissingFileModule, the hint didn't work
			if (_executable instanceof MissingFileModule) {
				_process.removeMemorySource(_executableTextSection);
				_executableTextSection = currentExecutableTextSection;
				_process.addMemorySource(_executableTextSection);
				_executable = currentExecutable;
			}
		}
	}
	
	private String getFileName(String name)
	{
		File f = new File(name);
		
		return f.getName();
	}

	private void loadModule(ILibraryResolver resolver, IProcess proc,
			long textVirtualAddress, long textSize, long dataVirtualAddress,
			long dataSize, String fileName, String objectName, String moduleName, boolean loadingExecutable)
	{
		//.data range will be loaded in core. .text range will be in the module itself.
		IMemoryRange data = new MemoryRange(proc.getAddressSpace(), dataVirtualAddress,dataSize,".data");
		IModule module;
		IMemorySource text;
		try {
			LibraryDataSource library = null;
			if (loadingExecutable) {
				library = resolver.getLibrary(fileName, true);
			} else {
				library = resolver.getLibrary(fileName);
			}
			IModuleFile moduleFile = loadModuleFile(library, objectName);
			
			text = moduleFile.getTextSegment(textVirtualAddress, textSize);
			List<? extends ISymbol> symbols = moduleFile.getSymbols(textVirtualAddress);
			
			List<IMemoryRange> moduleMemoryRanges = new LinkedList<IMemoryRange>();
			moduleMemoryRanges.add(text);
			moduleMemoryRanges.add(data);
			
			module = new Module(proc,moduleName,symbols,moduleMemoryRanges, textVirtualAddress, moduleFile.getProperties());
		} catch (Exception e) {
			//Create a non-backed memory range for the text segment (so we know it exists)
			text = new UnbackedMemorySource(textVirtualAddress, textSize, "Native library " + moduleName + " couldn't be found",0,".text");

			List<IMemoryRange> moduleMemoryRanges = new LinkedList<IMemoryRange>();
			moduleMemoryRanges.add(text);
			moduleMemoryRanges.add(data);
			
			//Can't find the library - put a stub in with the information we have.
			module = new MissingFileModule(proc,moduleName,moduleMemoryRanges);
		}
		
		//Add .text to _memoryRanges so it becomes part of the native address space.
		addMemorySource(text);
		
		if (loadingExecutable) {
			_executable = module;
			_executableTextSection = text;
		} else {
			_modules.add(module);
		}
	}

	private IModuleFile loadModuleFile(LibraryDataSource source, String objectName) throws IOException
	{
		XCOFFReader xcoff = null;
		try {
			switch(source.getType()) {
				case FILE :
					xcoff = new XCOFFReader(source.getFile());
					break;
				case STREAM :
					xcoff = new XCOFFReader(source.getName(), source.getStream(), 0, source.getStream().length());
					break;
				default :
					String msg = String.format("The library %s could not be read from a source type of %s", source.getName(), source.getType().name());
					throw new IllegalArgumentException(msg);
			}
		} catch (IllegalArgumentException e) {
			ARReader reader = null;
			switch(source.getType()) {
				case FILE :
					reader = new ARReader(source.getFile());
					break;
				case STREAM :
					reader = new ARReader(source.getStream());
					break;
				default :
					String msg = String.format("The library %s could not be read from a source type of %s", source.getName(), source.getType().name());
					throw new IllegalArgumentException(msg);
			}
			
			long offset = reader.offsetOfModule(objectName);
			long size = reader.offsetOfModule(objectName);
			
			if (offset < 0 || size < 0) {
				throw new IOException("Can't find object " + objectName + " in module " + source.getName() + ". offset = " + offset + ", size = " + size);
			}
			
			switch(source.getType()) {
				case FILE :
					xcoff = new XCOFFReader(source.getFile(), offset, size);
					break;
				case STREAM :
					xcoff = new XCOFFReader(source.getName(), source.getStream(), offset, size);
					break;
				default :
					String msg = String.format("The library %s could not be read from a source type of %s", source.getName(), source.getType().name());
					throw new IllegalArgumentException(msg);
			}
		}
		openFileTracker.add(xcoff);			//track the file to close when this dump reader is closed
		return xcoff;
	}
	
	List<? extends IOSThread> getThreads()
	{
		List<IOSThread> threads = new LinkedList<IOSThread>();
		
		//use the faulting thread to set the correct structure sizes in the reader
		long sizeofThread = threadSize(FAULTING_THREAD_OFFSET);
		
		try {
			threads.add(readThread(FAULTING_THREAD_OFFSET));
		} catch (IOException ex) {
			//TODO handle
		}
		
		for (int i=0; i < _threadCount; i++) {
			try {
				threads.add(readThread(_threadOffset + (i* sizeofThread)));
			} catch (IOException ex) {
				logger.logp(Level.WARNING,"com.ibm.j9ddr.corereaders.aix.AIXDumpReader","getThreads","Error adding thread",ex);
			}
		}
		
		return threads;
	}

	// private J9DDRImageThread readThread(J9DDRImageAddressSpace addressSpace,
	// long offset) throws IOException
	private IOSThread readThread(long offset) throws IOException
	{
		//Note that we appear to be parsing a "struct thrdsinfo" here which is
		//described in "/usr/include/procinfo.h"
		// however, the shape for 64-bit is somewhat guessed since the structs
		// thrdentry64 and thrdsinfo64 (for 64 and 32 bit dumps, respectively),
		// which are listed in the documentation, don't appear to be the actual
		// structures at work here
		seek(offset);
		//tid is 64-bits one 64-bit platforms but 32-bits on 32-bit platforms
		long tid = readAddress(); // unsigned long ti_tid; /* thread identifier */
		readInt(); // unsigned long ti_pid; /* process identifier */
		/* scheduler information */
		int ti_policy = readInt(); // unsigned long ti_policy; /* scheduling policy */
		int ti_pri = readInt(); // unsigned long ti_pri; /* current effective priority */
		int ti_state = readInt(); // unsigned long ti_state; /* thread state -- from thread.h */
		int ti_flag = readInt(); // unsigned long ti_flag; /* thread flags -- from thread.h */
		int ti_scount = readInt(); // unsigned long ti_scount; /* suspend count */
		int ti_wtype = readInt(); // unsigned long ti_wtype; /* type of thread wait */
		int ti_wchan = readInt(); // unsigned long ti_wchan; /* wait channel */
		int ti_cpu = readInt(); // unsigned long ti_cpu; /* processor usage */
		int ti_cpuid = readInt(); // cpu_t ti_cpuid; /* processor on which I'm bound */
		readSigset(); // sigset_t ti_sigmask /* sigs to be blocked */
		readSigset(); // sigset_t ti_sig; /* pending sigs */
		readInt(); // unsigned long ti_code; /* iar of exception */
		readAddress(); // struct sigcontext *ti_scp; /* sigctx location in userspace */
		int signalNumber = 0xFF & readByte(); // char ti_cursig; /* current/last signal taken */
			
		Map<String,Number> registers = readRegisters(offset);
		Properties properties = new Properties();
		properties.put("scheduling policy", Integer.toHexString(ti_policy));
		properties.put("current effective priority",Integer.toHexString(ti_pri));
		properties.put("thread state", Integer.toHexString(ti_state));
		properties.put("thread flags", Integer.toHexString(ti_flag));
		properties.put("suspend count", Integer.toHexString(ti_scount));
		properties.put("type of thread wait", Integer.toHexString(ti_wtype));
		properties.put("wait channel", Integer.toHexString(ti_wchan));
		properties.put("processor usage", Integer.toHexString(ti_cpu));
		properties.put("processor on which I'm bound", Integer.toHexString(ti_cpuid));
		//the signal number should probably be exposed through a real API for,
		//for now, it is interesting info so just return it
		properties.put("current/last signal taken", Integer.toHexString(signalNumber));
		
		long stackPointer = getStackPointerFrom(registers);
		IMemoryRange range = getProcess().getMemoryRangeForAddress(stackPointer);
		if (null == range) {
			throw new IOException("Cannot find memory range for stackPointer " + Long.toHexString(stackPointer));
		}
		IMemoryRange stackRange = new MemoryRange(getProcess().getAddressSpace(), range, "stack");
		
		return new AIXOSThread(getProcess(), tid, Collections.singletonList(stackRange), properties, registers);
	}

	class AIXOSThread extends BaseAIXOSThread
	{
		private final long tid;
		private final List<? extends IMemoryRange> memoryRanges;
		private final Properties properties;
		private final Map<String,Number> registers;
		
		protected AIXOSThread(IProcess process, long tid, List<? extends IMemoryRange> memoryRanges, Properties properties, Map<String,Number> registers)
		{
			super(process);
			this.tid = tid;
			this.memoryRanges = memoryRanges;
			this.properties = properties;
			this.registers = registers;
		}

		public Collection<? extends IMemoryRange> getMemoryRanges()
		{
			return Collections.unmodifiableList(memoryRanges);
		}

		public Properties getProperties()
		{
			return this.properties;
		}

		public List<? extends IRegister> getRegisters()
		{
			List<IRegister> toReturn = new ArrayList<IRegister>(registers.size());
			
			for (String name : registers.keySet()) {
				Number value = registers.get(name);
				
				toReturn.add(new Register(name,value));
			}
			
			return toReturn;
		}

		public long getThreadId() throws CorruptDataException
		{
			return tid;
		}

		public long getInstructionPointer()
		{
			return getInstructionPointerFrom(registers); 
		}

		public long getBasePointer()
		{
			return getLinkRegisterFrom(registers);
		}

		public long getStackPointer()
		{
			return getStackPointerFrom(registers);
		}
	}

	private void readSigset() throws IOException
	{
		readBytes(is64Bit() ? 32 : 8);
	}

	public String getDumpFormat()
	{
		return "xcoff";
	}

	protected AIXProcessAddressSpace getProcess()
	{
		return _process;
	}
	
	public List<IAddressSpace> getAddressSpaces()
	{
		return Collections.singletonList((IAddressSpace)_process);
	}
	

	public Platform getPlatform()
	{
		return Platform.AIX;
	}

	public Properties getProperties()
	{
		return props;
	}

	long getEnvironmentHandle() throws CorruptDataException
	{
		try {
			loadUserInfo();
		} catch (IOException e) {
			throw new CorruptDataException(e);
		}
		return _environmentHandle;
	}

	List<IModule> getModules()
	{
		return _modules;
	}

	IModule getExecutable()
	{
		return _executable;
	}

	long getProcessId()
	{
		return _pid;
	}
	
	public static boolean isAIXDump(byte[] data, long filesize)
	{
		int version = readInt(data, (int)CORE_FILE_VERSION_OFFSET);
		if((version != CORE_DUMP_X_VERSION) && (version != CORE_DUMP_XX_VERSION)) {
			return false;		//unrecognised core file version
		}
		long fdsinfox = readLong(data, 8);
		long loader = readLong(data, 16);
		return fdsinfox > 0 && loader > 0 && loader > fdsinfox && fdsinfox < filesize && loader < filesize;
	}

	public static ICore getReaderForFile(File file) throws IOException, InvalidDumpFormatException
	{
		ClosingFileReader reader = new ClosingFileReader(file, ByteOrder.BIG_ENDIAN);
		
		//Read first few bytes to check this is an AIX dump
		byte[] buffer = new byte[128];
		
		reader.readFully(buffer);
		
		if (! isAIXDump(buffer, file.length()) ) {
			reader.close();
			throw new InvalidDumpFormatException("File " + file.getAbsolutePath() + " is not an AIX dump");
		}
		
		reader.seek(CORE_FILE_VERSION_OFFSET);
		int version = reader.readInt();
		
		boolean is64Bit = false;
		if (version == CORE_DUMP_X_VERSION) {
			//Could be a 64 bit if generated on a pre-AIX5 machine
			reader.seek(CORE_DUMP_X_PI_FLAGS_2_OFFSET);
			int flags = reader.readInt();
			
			is64Bit = (S64BIT & flags) != 0;
		} else if (version == CORE_DUMP_XX_VERSION) {
			//All core_dump_xx are 64 bit processes
			is64Bit = true;
		} else {
			throw new InvalidDumpFormatException("Unrecognised core file version: " + Long.toHexString(version));
		}
		
		if (is64Bit) {
			return new AIX64DumpReader(file, reader);
		} else {
			return new AIX32DumpReader(file, reader);
		}
	}
	
	public static ICore getReaderForFile(ImageInputStream in) throws IOException, InvalidDumpFormatException
	{	
		//Read first few bytes to check this is an AIX dump
		byte[] buffer = new byte[128];
		in.seek(0);
		in.read(buffer);
		
		//because of the stream parameter we can't check the file length, so this weakens an already weak AIX validation
		if (! isAIXDump(buffer, Long.MAX_VALUE) ) {
			throw new InvalidDumpFormatException("The supplied input stream is not an AIX dump");
		}
		
		in.seek(CORE_FILE_VERSION_OFFSET);
		int version = in.readInt();
		
		boolean is64Bit = false;
		if (version == CORE_DUMP_X_VERSION) {
			//Could be a 64 bit if generated on a pre-AIX5 machine
			in.seek(CORE_DUMP_X_PI_FLAGS_2_OFFSET);
			int flags = in.readInt();
			
			is64Bit = (S64BIT & flags) != 0;
		} else if (version == CORE_DUMP_XX_VERSION) {
			//All core_dump_xx are 64 bit processes
			is64Bit = true;
		} else {
			throw new InvalidDumpFormatException("Unrecognised core file version: " + Long.toHexString(version));
		}
		
		if (is64Bit) {
			return new AIX64DumpReader(in);
		} else {
			return new AIX32DumpReader(in);
		}
	}

	@Override
	public void close() throws IOException {
		for(XCOFFReader reader : openFileTracker) {
			reader.close();
		}
		//release any resources acquired for library resolution
		resolver.dispose();
		super.close();
	}	
}
