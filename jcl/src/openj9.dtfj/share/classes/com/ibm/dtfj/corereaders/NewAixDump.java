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
import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Properties;
import java.util.Set;
import java.util.TreeSet;

import javax.imageio.stream.ImageInputStream;

import com.ibm.dtfj.addressspace.IAbstractAddressSpace;
import com.ibm.dtfj.addressspace.LayeredAddressSpace;
import com.ibm.dtfj.binaryreaders.ARReader;
import com.ibm.dtfj.binaryreaders.XCOFFReader;

public abstract class NewAixDump extends CoreReaderSupport {
	private static final long FAULTING_THREAD_OFFSET = 216; // offsetof(core_dumpx, c_flt)
	private static final int S64BIT = 0x00000001; // indicates a 64-bit process
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
	//private static final int POWER_RS64IV = POWER_4;
	private static final int POWER_MPC7450 = 0x1000;
	private static final int POWER_5 = 0x2000;

	protected NewAixDump(DumpReader reader)
	{
		super(reader);
	}
	
	private List _memoryRanges = new ArrayList();
	private Set _additionalFileNames = new TreeSet();
	private int _implementation;
	private int _threadCount;
	private long _threadOffset;
	private long _lastModified;
	private long _loaderOffset;
	private long _loaderSize;
	private LayeredAddressSpace _addressSpace;
	private MemoryRange _stackRange = null;
	//addresses required to find the bottom of the main thread's stack (for some reason called the "top of stack" in AIX documentation)
	private long _structTopOfStackVirtualAddress;
	
	private boolean _isTruncated = false;
	
	public static boolean isSupportedDump(ImageInputStream stream) throws IOException {
		// There are no magic signatures in an AIX dump header.  Some simple validation of the header
		// allows a best guess about validity of the file.
		stream.seek(8);
		long fdsinfox = stream.readLong();
		long loader = stream.readLong();
		long filesize = stream.length();
		return fdsinfox > 0 && loader > 0 && loader > fdsinfox && fdsinfox < filesize && loader < filesize;
	}

	public static ICoreFileReader dumpFromFile(ImageInputStream stream) throws IOException {
		assert isSupportedDump(stream);
		stream.seek(CORE_FILE_VERSION_OFFSET);
		int version = stream.readInt();
		
		boolean is64Bit = false;
		if (version == CORE_DUMP_X_VERSION) {
			//Could be a 64 bit if generated on a pre-AIX5 machine
			stream.seek(CORE_DUMP_X_PI_FLAGS_2_OFFSET);
			int flags = stream.readInt();
			
			is64Bit = (S64BIT & flags) != 0;
		} else if (version == CORE_DUMP_XX_VERSION) {
			//All core_dump_xx are 64 bit processes
			is64Bit = true;
		} else {
			throw new IOException("Unrecognised core file version: " + Long.toHexString(version));
		}
		
		DumpReader reader = new DumpReader(stream, is64Bit);
		if (is64Bit) {
			return new Aix64Dump(reader);
		} else {
			return new Aix32Dump(reader);
		}
	}
	
	protected void readCore() throws IOException
	{
		//Note that this structure is core_dumpx defined in "/usr/include/sys/core.h"
		coreSeek(0);
		coreReadByte(); // Ignore signalNumber
		byte flags = coreReadByte(); // CORE_TRUNC == 0x80
		if ((flags & 0x80) != 0) {
			_isTruncated = true;
		}
		coreReadShort(); // Ignore entryCount
		coreReadInt(); // Ignore version (core file format number)
		coreReadLong(); // Ignore fdsinfox
		_loaderOffset = coreReadLong();
		_loaderSize = coreReadLong();
		_threadCount = coreReadInt();
		coreReadInt(); // Ignore padding
		_threadOffset = coreReadLong();
		long segmentCount = coreReadLong();
		long segmentOffset = coreReadLong();

		// Stack - this appears to be special handling for the main thread first attached to the address space
		//		   (this is helpful for us since that is how we can find command line and env vars)
		long stackOffset = coreReadLong();
		long stackVirtualAddress = coreReadLong();
		long stackSize = coreReadLong();
		_structTopOfStackVirtualAddress = stackVirtualAddress  + stackSize  - sizeofTopOfStack();
		
		MemoryRange memoryRange1 = new MemoryRange(stackVirtualAddress, stackOffset, stackSize);
		//memoryRange1.setDescription("stack");
		_memoryRanges.add(memoryRange1);
		
		// This is a temporary hack until we can understand signal handler frames better.
		_stackRange  = memoryRange1;

		// User data
		long dataOffset = coreReadLong();
		long dataVirtualAddress = coreReadLong();
		long dataSize = coreReadLong();
		MemoryRange memoryRange = new MemoryRange(dataVirtualAddress, dataOffset, dataSize);
		//memoryRange.setDescription("user data");
		_memoryRanges.add(memoryRange);

		coreReadLong();	// Ignore sdataVirtualAddress
		coreReadLong(); // Ignore sdataSize
		
		long vmRegionCount = coreReadLong();
		long vmRegionOffset = coreReadLong();
		_implementation = coreReadInt();
		coreReadInt(); // Ignore padding
		coreReadLong();	// Ignore checkpointRestartOffset
		coreReadLong();	// Ignore unsigned long long c_extctx;    (Extended Context Table offset)

		coreReadBytes(6 * 8); // Ignore padding (6 longs)
		
		//ignore struct thrdctx     c_flt;    (faulting thread's context)	//this is resolved later via that constant "FAULTING_THREAD_OFFSET"
		//ignore struct userx       c_u;       (copy of the user structure)
		//>> end of core_dumpx structure
		
		readVMRegions(vmRegionOffset, vmRegionCount);
		readSegments(segmentOffset, segmentCount);
		readLoaderInfoAsMemoryRanges();

		MemoryRange highestRange = getHighestRange();
		if (null != highestRange) {
			// in theory, we should be checking whether the last byte of the highest range is readable, instead of the 1st.
			// Unfortunately, it turns out that the last VM region in any AIX dump is always missing the last few KBytes.
			_isTruncated |= !coreCheckOffset(highestRange.getFileOffset());
		}
		
	}

	private MemoryRange getHighestRange() {
		long highestOffset = -1;
		MemoryRange highestRange = null;
		Iterator i = _memoryRanges.iterator();
		while (i.hasNext()) {
			MemoryRange range = (MemoryRange)(i.next());
			if (range.getFileOffset() > highestOffset) {
				highestOffset = range.getFileOffset();
				highestRange = range;
			}
		}
		return highestRange;
	}
	
	private void readVMRegions(long offset, long count) throws IOException {
		coreSeek(offset);
		for (int i = 0; i < count; i++) {
			long address = coreReadLong();
			long size = coreReadLong();
			long off = coreReadLong();
			// Anonymously mapped area
			MemoryRange memoryRange = new MemoryRange(address, off, size);
			//memoryRange.setDescription("anon vm region");
			_memoryRanges.add(memoryRange);
		}
	}

	private void readSegments(long offset, long count) throws IOException {
		coreSeek(offset);
		for (int i = 0; i < count; i++) {
			long address = coreReadLong();
			long size = coreReadLong();
			long off = coreReadLong();
			coreReadInt(); // Ignore segmentFlags
			coreReadInt(); // Ignore padding
			// Anonymously mapped area
			MemoryRange memoryRange = new MemoryRange(address, off, size);
			//memoryRange.setDescription("anon segment");
			_memoryRanges.add(memoryRange);
		}
	}
	
	private void readLoaderInfoAsMemoryRanges() throws IOException {
		int next = 0;
		long current = _loaderOffset;
		do {
			current += next;
			coreSeek(current);
			next = coreReadInt();
			readLoaderInfoFlags(); // Ignore flags
			long dataOffset = coreReadAddress();
			coreReadAddress(); // Ignore textOrigin
			coreReadAddress(); // Ignore textSize
			long dataVirtualAddress = coreReadAddress();
			long dataSize = coreReadAddress();
			//String name = readString();
			//String subname = readString();
			//if (0 < subname.length())
			//	name += "(" + subname + ")";
			if (0 != dataOffset) {
				MemoryRange memoryRange = new MemoryRange(dataVirtualAddress, dataOffset, dataSize);
				//memoryRange.setDescription(name + " " + Long.toHexString(textOrigin) + "-" + Long.toHexString(textOrigin + textSize));
				_memoryRanges.add(memoryRange);
			}
		} while (0 != next && current + next < _loaderOffset + _loaderSize);
	}
	
	private List readLoaderInfoAsModules(Builder builder, Object addressSpace) throws IOException
	{
		List modules = new ArrayList();
		int next = 0;
		long current = _loaderOffset;
		do {
			current += next;
			coreSeek(current);
			next = coreReadInt();
			readLoaderInfoFlags(); // Ignore flags
			coreReadAddress(); // Ignore dataOffset
			long textVirtualAddress = coreReadAddress();
			long textSize = coreReadAddress();
			long dataVirtualAddress = coreReadAddress();
			long dataSize = coreReadAddress();
			String fileName = readString();
			String objectName = readString();
			String moduleName = fileName;
			if (0 < objectName.length()) {
				moduleName += "(" + objectName + ")";
			}
			Object text = builder.buildModuleSection(addressSpace, ".text", textVirtualAddress, textVirtualAddress + textSize);
			Object data = builder.buildModuleSection(addressSpace, ".data", dataVirtualAddress, dataVirtualAddress + dataSize);
			List sections = new ArrayList();
			sections.add(text);
			sections.add(data);
			XCOFFReader libraryReader = _openLibrary(builder, fileName, objectName);
			_additionalFileNames.add(fileName);
			if (null != libraryReader) {
				Properties properties = libraryReader.moduleProperties();
				List symbols = libraryReader.buildSymbols(builder, addressSpace, textVirtualAddress);
				modules.add(builder.buildModule(moduleName, properties, sections.iterator(), symbols.iterator(),textVirtualAddress));
				internalAddressSpace().mapRegion(textVirtualAddress, libraryReader.underlyingFile(), libraryReader.baseFileOffset(), textSize);
			} else {
				List symbols = Collections.singletonList(builder.buildCorruptData(addressSpace, "unable to find module " + moduleName, textVirtualAddress));
				//here we will pass out a null for properties which can be thrown, from the other side, as DataUnavailable
				modules.add(builder.buildModule(moduleName, null, sections.iterator(), symbols.iterator(),textVirtualAddress));
			}
		} while (0 != next && current + next < _loaderOffset + _loaderSize);
		return modules;
	}

	/**
	 * Returns an XCOFFReader for the library at the specified fileName.  If the library is an archive, the objectName is used to find the library
	 * inside the archive and the reader will be for that "file"
	 * 
	 * Returns null if the library cannot be found or is not an understood XCOFF or AR archive
	 * 
	 * @param fileName
	 * @param objectName
	 * @return
	 */
	private XCOFFReader _openLibrary(Builder builder, String fileName, String objectName)
	{
		//Note:  objectName is required to look up the actual module inside a library
		XCOFFReader libraryReader = null;
		ClosingFileReader library = null;
		try {
			library = builder.openFile(fileName);
		} catch (IOException e) {
			//this is safe since we will just produce incomplete data
		}
		
		if (null != library) {
			//we found the library so figure out what it is and open it
			long fileOffset = 0;
			long fileSize = 0;
			try {
				try {
					libraryReader = new XCOFFReader(library);
					fileSize = library.length();
				} catch (IllegalArgumentException e) {
					//this will happen if we are looking at a .a file, for example
					try {
						ARReader archive = new ARReader(library);
						fileSize = archive.sizeOfModule(objectName);
						fileOffset = archive.offsetOfModule(objectName);
						libraryReader = new XCOFFReader(library, fileOffset, fileSize);
					} catch (IllegalArgumentException e2) {
						//this might occur if we are on a different platform but have the same libraries around
						libraryReader = null;
					}
				}
			} catch (IOException e) {
			}
		}
		return libraryReader;
	}

	private void readUserInfo(Builder builder, Object addressSpace) throws IOException {
		
		// Get the process ID
		coreSeek(userInfoOffset());
		int pid = coreReadInt();
		
		// Get the process command line. For AIX dumps we find the registers in the initial (top - this is to be consistent with AIX headers) 
		// stack frame. Register 3 has the number of command line arguments (argc), register 4 has 
		// a pointer to a list of pointers to the command line arguments (argv), and register 5 contains the pointer to the environment vars
		String commandLine = "";
		try
		{
			coreSeekVirtual(_structTopOfStackVirtualAddress);
		}
		catch (MemoryAccessException e)
		{
			//we will be unable to collect data from the core so return without populating (it is probably incomplete and provides no mapping for this part of memory)
			return;
		}
		coreReadAddress();	//ignore GPR0
		coreReadAddress();	//ignore GPR1
		coreReadAddress();	//ignore GPR2
		long reg3 = coreReadAddress();	//argc
		long reg4 = coreReadAddress();	//argv
		long reg5 = coreReadAddress();	//env
		int argc = (int)reg3; // truncate to integer
		
		if (argc > 0) {
			long[] addresses = new long[argc];
			try
			{
				coreSeekVirtual(reg4);
				for (int i = 0; i < reg3; i++) {
					addresses[i] = coreReadAddress();
				}
				for (int i = 0; i < reg3; i++) {
					try
					{
						coreSeekVirtual(addresses[i]);
						commandLine += readString() + " ";
					}
					catch (MemoryAccessException e)
					{
						//this one is missing but we still want to collect the others - shouldn't happen
					}
				}
			}
			catch (MemoryAccessException e1)
			{
				//failed to read the command line so set it null so that we can detect it wasn't just empty (which would also be wrong but it a less obvious way)
				commandLine = null;
			}
		} else {
			// argc is zero or negative.
			commandLine = null;
		}
		
		// NOTE modules must be read before thread information in order to properly support stack traces.
		// Thread information is required before building the process, so the following order must be maintained:
		// 1) Read module information.
		// 2) Read thread information.
		// 3) Build process.
		List modules = readLoaderInfoAsModules(builder, addressSpace);
		Object executable = null;
		Iterator libraries = modules.iterator();
		if (libraries.hasNext()) {
			executable = libraries.next();
		}
		
		List threads = readThreads(builder, addressSpace);
		//the current thread is parsed before the rest so it is put first
		Object currentThread = threads.get(0);

		// Get the process environment variables
		Properties environment = new Properties();
		//on AIX, we should trust the env var pointer found in the core and ignore whatever the XML told us it is since this internal value is more likely correct
		environment = getEnvironmentVariables(reg5);
		
		builder.buildProcess(addressSpace, String.valueOf(pid), commandLine, environment, currentThread, threads.iterator(), executable, libraries, pointerSize());
	}
	
	/**
	 * The environment structure is a null-terminated list of addresses which point to strings of form x=y
	 * 
	 * @param environmentAddress The pointer to the environment variables data structure
	 * @return A property list of the key-value pairs found for the env vars
	 * @throws IOException Failure reading the underlying core file
	 */
	private Properties getEnvironmentVariables(long environmentAddress) throws IOException
	{
		//make sure that the pointer is valid
		if (0 == environmentAddress) {
			return null;
 		}
		
		// Get the environment variable addresses
		List addresses = new ArrayList();
		try
		{
			coreSeekVirtual(environmentAddress);
		}
		catch (MemoryAccessException e1)
		{
			//the env var structure appears not to be mapped or the pointer is corrupt
			return null;
		}
		long address = coreReadAddress();
		while (address != 0) {
			addresses.add(Long.valueOf(address));
			address = coreReadAddress();
		}

         // Get the environment variables
         Properties environment = new Properties();
         for (Iterator iter = addresses.iterator(); iter.hasNext();) {
        	 Long varAddress = (Long)iter.next();
        	 String pair = null;
             try {
            	 coreSeekVirtual(varAddress.longValue());
            	 pair = readString();
             } catch (MemoryAccessException e) {
                 // catch errors here, we can carry on getting other environment variables - unlikely to be missing only some of these
                 continue;
             }
             if (null != pair)
             {
                 // Pair is the x=y string, now break it into key and value
                 int equalSignIndex = pair.indexOf('=');
                 if (equalSignIndex >= 0)
                 {
                     String key = pair.substring(0, equalSignIndex);
                     String value = pair.substring(equalSignIndex + 1);
                     environment.put(key, value);
                 }
             }
         }

         return environment;
 }

	private List readThreads(Builder builder, Object addressSpace) throws IOException {
		List threads = new ArrayList();
		//use the faulting thread to set the correct structure sizes in the reader
		long sizeofThread = threadSize(FAULTING_THREAD_OFFSET);
		threads.add(readThread(builder, addressSpace, FAULTING_THREAD_OFFSET));
		for (int i = 0; i < _threadCount; i++) {
			threads.add(readThread(builder, addressSpace, _threadOffset + i * sizeofThread));
		}
		return threads;
	}

	private Object readThread(Builder builder, Object addressSpace, long offset) throws IOException
	{
		//Note that we appear to be parsing a "struct thrdsinfo" here which is described in "/usr/include/procinfo.h"
		//	however, the shape for 64-bit is somewhat guessed since the structs thrdentry64 and thrdsinfo64 (for 64 and 32 bit dumps, respectively), which are listed in the documentation, don't appear to be the actual structures at work here
		coreSeek(offset);
		//tid is 64-bits one 64-bit platforms but 32-bits on 32-bit platforms
		long tid = coreReadAddress();	//	unsigned long            ti_tid;      /* thread identifier */
		coreReadInt();	//	unsigned long            ti_pid;      /* process identifier */
        /* scheduler information */
		int ti_policy = coreReadInt();	//	unsigned long               ti_policy;   /* scheduling policy */
		int ti_pri = coreReadInt();	//	unsigned long               ti_pri;      /* current effective priority */
		int ti_state = coreReadInt();	//	unsigned long               ti_state;    /* thread state -- from thread.h */
		int ti_flag = coreReadInt();	//	unsigned long               ti_flag;     /* thread flags -- from thread.h */
		int ti_scount = coreReadInt();	//	unsigned long               ti_scount;   /* suspend count */
		int ti_wtype = coreReadInt();	//	unsigned long               ti_wtype;    /* type of thread wait */
		int ti_wchan = coreReadInt();	//	unsigned long   ti_wchan;       /* wait channel */
		int ti_cpu = coreReadInt();	//	unsigned long               ti_cpu;      /* processor usage */
		int ti_cpuid = coreReadInt();	//	cpu_t              ti_cpuid;    /* processor on which I'm bound */
		coreReadSigset();	//  sigset_t		ti_sigmask		/* sigs to be blocked */
		coreReadSigset();	//  sigset_t        ti_sig;         /* pending sigs */
		coreReadInt();	//  unsigned long   ti_code;        /* iar of exception */
		coreReadAddress();	//  struct sigcontext *ti_scp;      /* sigctx location in user space */
		int signalNumber = 0xFF & coreReadByte();	//  char            ti_cursig;      /* current/last signal taken */
		
	    Map registers = readRegisters(offset);
	    Properties properties = new Properties();
	    properties.put("scheduling policy", Integer.toHexString(ti_policy));
	    properties.put("current effective priority", Integer.toHexString(ti_pri));
	    properties.put("thread state", Integer.toHexString(ti_state));
	    properties.put("thread flags", Integer.toHexString(ti_flag));
	    properties.put("suspend count", Integer.toHexString(ti_scount));
	    properties.put("type of thread wait", Integer.toHexString(ti_wtype));
	    properties.put("wait channel", Integer.toHexString(ti_wchan));
	    properties.put("processor usage", Integer.toHexString(ti_cpu));
	    properties.put("processor on which I'm bound", Integer.toHexString(ti_cpuid));
	    //the signal number should probably be exposed through a real API for, for now, it is interesting info so just return it
	    properties.put("current/last signal taken", Integer.toHexString(signalNumber));
	    
	    List sections = new ArrayList();
	    List frames = new ArrayList();
	    long stackPointer = getStackPointerFrom(registers);
	    long instructionPointer = getInstructionPointerFrom(registers);
	    if (0 == instructionPointer || false == isValidAddress(instructionPointer)) {
	    		instructionPointer = getLinkRegisterFrom(registers);
	    }
	    try {
			if (0 != instructionPointer && 0 != stackPointer && isValidAddress(instructionPointer) && isValidAddress(stackPointer)) {
				MemoryRange range = memoryRangeFor(stackPointer);
				if (range != null) {					
					sections.add(builder.buildStackSection(addressSpace, range.getVirtualAddress(), range.getVirtualAddress() + range.getSize()));
					frames.add(builder.buildStackFrame(addressSpace, stackPointer, instructionPointer));
					IAbstractAddressSpace memory = getAddressSpace();
					
					long previousStackPointer = -1;
					
					// Sometimes stack frames reference themselves. We need to avoid an infinite cycle in those cases
					// In case of other infinite loops or damaged stacks we limit stack size to 1024 frames.
					while (range.contains(stackPointer) && previousStackPointer != stackPointer && frames.size() < 1024) {
						previousStackPointer = stackPointer;
						stackPointer = memory.getPointerAt(0, stackPointer);
						long stepping = ((64 == pointerSize()) ? 8 : 4);
						long addressToRead = stackPointer + stepping;
						addressToRead += stepping;//readAddress(); // Ignore conditionRegister
						instructionPointer = memory.getPointerAt(0, addressToRead);
						frames.add(builder.buildStackFrame(addressSpace, stackPointer, instructionPointer));
					}
				}
			} else {
				// This is a temporary hack until we can understand signal handler stacks better
	    			sections.add(builder.buildStackSection(addressSpace, _stackRange.getVirtualAddress(), _stackRange.getVirtualAddress() + _stackRange.getSize()));
			}
	    } catch (MemoryAccessException e) {
	    		// Ignore
	    }
		return builder.buildThread(String.valueOf(tid), registersAsList(builder, registers).iterator(),
				   				   sections.iterator(), frames.iterator(), properties, signalNumber);
	}
	
	private void coreReadSigset() throws IOException
	{
		coreReadBytes(is64Bit() ? 32 : 8);
	}

	private boolean isValidAddress(long address)
	{
		try {
			getAddressSpace().getByteAt(0, address);
			return true;
		} catch (MemoryAccessException e) {
			return false;
		}
	}
	
	protected MemoryRange memoryRangeFor(long address)
	{
		//TODO:  change the callers of this method to use some mechanism which will work better within the realm of the new address spaces
		Iterator ranges = _memoryRanges.iterator();
		MemoryRange match = null;
		
		while ((null == match) && ranges.hasNext()) {
			MemoryRange range = (MemoryRange) ranges.next();
			
			if (range.contains(address)) {
				match = range;
			}
		}
		return match;
	}
	
	private String readString() throws IOException {
		StringBuffer buffer = new StringBuffer();
		byte b = coreReadByte();
		while (0 != b) {
			buffer.append(new String(new byte[]{b}, "ASCII"));
			b = coreReadByte();
		}
		return buffer.toString();
	}

	private List registersAsList(Builder builder, Map registers) {
		List list = new ArrayList();
		for (Iterator iter = registers.entrySet().iterator(); iter.hasNext();) {
			Map.Entry entry = (Map.Entry) iter.next();
			list.add(builder.buildRegister((String) entry.getKey(), (Number) entry.getValue()));
		}
		return list;
	}

	protected abstract Map readRegisters(long threadOffset) throws IOException;
	protected abstract int readLoaderInfoFlags() throws IOException;
	protected abstract long userInfoOffset();
	protected abstract long threadSize(long threadOffset);
	protected abstract int pointerSize();
	protected abstract long getStackPointerFrom(Map registers);
	protected abstract long getInstructionPointerFrom(Map registers);
	protected abstract long getLinkRegisterFrom(Map registers);
	protected abstract int sizeofTopOfStack();

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.corereaders.Dump#getMemoryRanges()
	 */
	public Iterator getMemoryRanges()
	{
		return _memoryRanges.iterator();
	}

	public void extract(Builder builder) {
		try {
			Object addressSpace = builder.buildAddressSpace("AIX Address Space", 0);
			readUserInfo(builder, addressSpace);
		} catch (IOException e) {
			// Ignore
		}
		builder.setOSType("AIX");
		builder.setCPUType("PowerPC");
		builder.setCPUSubType(getProcessorSubtype());
		builder.setCreationTime(getCreationTime());
	}

	public String getProcessorSubtype() {
		switch (_implementation) {
		case POWER_RS1: return "RS1";
		case POWER_RSC: return "RSC";
		case POWER_RS2: return "RS2";
		case POWER_601: return "601";
		case POWER_603: return "603";
		case POWER_604: return "604";
		case POWER_620: return "620";
		case POWER_630: return "630";
		case POWER_A35: return "A35";
		case POWER_RS64II: return "RS64-II";
		case POWER_RS64III: return "RS64-III";
		case POWER_4: return "POWER 4";
		case POWER_MPC7450: return "MPC7450";
		case POWER_5: return "POWER 5";
		default: return "";
		}
	}

	public long getCreationTime() {
		return _lastModified;
	}

	public Iterator getAdditionalFileNames() {
		return _additionalFileNames.iterator();
	}
	
	protected MemoryRange[] getMemoryRangesAsArray() {
		return (MemoryRange[])_memoryRanges.toArray(new MemoryRange[_memoryRanges.size()]);
	}
	
	private LayeredAddressSpace internalAddressSpace()
	{
		if (null == _addressSpace) {
			IAbstractAddressSpace base = super.getAddressSpace();
			_addressSpace = new LayeredAddressSpace(base, false, (64 == pointerSize()));
		}
		return _addressSpace;
	}
	
	public IAbstractAddressSpace getAddressSpace()
	{
		return internalAddressSpace();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.corereaders.CoreReaderSupport#is64Bit()
	 */
	protected boolean is64Bit()
	{
		return (64 == pointerSize());
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.corereaders.CoreReaderSupport#isLittleEndian()
	 */
	protected boolean isLittleEndian()
	{
		//currently, all the AIX platforms that we work with are big endian
		return false;
	}
	
	/**
	 * Seeks the core file reader to the on-disk location of the given virtual address
	 * @param virtualAddress The address we wish to seek to in the process's virtual address space
	 * @throws MemoryAccessException If the virtual address does not map to a location on disk (either invalid location or unmapped in the core)
	 * @throws IOException If the underlying file seek operation fails
	 */
	private void coreSeekVirtual(long virtualAddress) throws MemoryAccessException, IOException
	{
		MemoryRange range = memoryRangeFor(virtualAddress);
		if (null == range)
		{
			throw new MemoryAccessException(0, virtualAddress);
		}
		else
		{
			long onDisk = range.getFileOffset() + (virtualAddress - range.getVirtualAddress());
			coreSeek(onDisk);
		}
	}
	
	public boolean isTruncated() {
		return _isTruncated;
	}
}
