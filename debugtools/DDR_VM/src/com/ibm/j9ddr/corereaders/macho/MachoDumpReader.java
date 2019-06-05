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

import java.io.EOFException;
import java.io.File;
import java.io.IOException;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Properties;
import java.util.TreeMap;

import javax.imageio.stream.FileImageInputStream;
import javax.imageio.stream.ImageInputStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.DataUnavailableException;
import com.ibm.j9ddr.corereaders.AbstractCoreReader;
import com.ibm.j9ddr.corereaders.ICore;
import com.ibm.j9ddr.corereaders.ILibraryDependentCore;
import com.ibm.j9ddr.corereaders.InvalidDumpFormatException;
import com.ibm.j9ddr.corereaders.Platform;
import com.ibm.j9ddr.corereaders.macho.ThreadCommand.ThreadState;
import com.ibm.j9ddr.corereaders.memory.DumpMemorySource;
import com.ibm.j9ddr.corereaders.memory.IAddressSpace;
import com.ibm.j9ddr.corereaders.memory.IMemoryRange;
import com.ibm.j9ddr.corereaders.memory.IMemorySource;
import com.ibm.j9ddr.corereaders.memory.IModule;
import com.ibm.j9ddr.corereaders.memory.ISymbol;
import com.ibm.j9ddr.corereaders.memory.Module;
import com.ibm.j9ddr.corereaders.memory.UnbackedMemorySource;
import com.ibm.j9ddr.corereaders.osthread.IOSStackFrame;
import com.ibm.j9ddr.corereaders.osthread.IOSThread;
import com.ibm.j9ddr.corereaders.osthread.IRegister;
import com.ibm.j9ddr.corereaders.osthread.Register;

/**
 * There is an implicit assumption in this core reader that the Mach-O cores
 * are generated on a 64-bit Mac OSX machine, since OpenJ9 only supports OSX
 * out of the platforms using the Mach-O format.
 */
public class MachoDumpReader extends AbstractCoreReader implements ILibraryDependentCore
{

	//Mach-O identifiers
	private static final int MACHO_64 = 0xFEEDFACF;
	private static final int MACHO_64_REV = 0xCFFAEDFE;

	//Mach file types
	public static final int MH_OBJECT = 0x1;
	public static final int MH_EXECUTE = 0x2;
	public static final int MH_FVMLIB = 0x3;
	public static final int MH_CORE = 0x4;
	public static final int MH_PRELOAD = 0x5;
	public static final int MH_DYLIB = 0x6;
	public static final int MH_DYLINKER = 0x7;
	public static final int MH_BUNDLE = 0x8;
	public static final int MH_DYLIB_STUB = 0x9;
	public static final int MH_DSYM = 0xa;
	public static final int MH_KEXT_BUNDLE = 0xb;

	// Flags for the mach-o file, see /usr/include/mach-o/loader.h for descriptions
	private static final int MH_NOUNDEFS = 0x1;
	private static final int MH_INCRLINK = 0x2;
	private static final int MH_DYLDLINK = 0x4;
	private static final int MH_BINDATLOAD = 0x8;
	private static final int MH_PREBOUND = 0x10;
	private static final int MH_SPLIT_SEGS = 0x20;
	private static final int MH_LAZY_INIT = 0x40;
	private static final int MH_TWOLEVEL = 0x80;
	private static final int MH_FORCE_FLAT = 0x100;
	private static final int MH_NOMULTIDEFS = 0x200;
	private static final int MH_NOFIXPREBINDING = 0x400;
	private static final int MH_PREBINDABLE = 0x800;
	private static final int MH_ALLMODSBOUND = 0x1000;
	private static final int MH_SUBSECTIONS_VIA_SYMBOLS = 0x2000;
	private static final int MH_CANONICAL = 0x4000;
	private static final int MH_WEAK_DEFINES = 0x8000;
	private static final int MH_BINDS_TO_WEAK = 0x10000;
	private static final int MH_ALLOW_STACK_EXECUTION = 0x20000;
	private static final int MH_ROOT_SAFE = 0x40000;
	private static final int MH_SETUID_SAFE = 0x80000;
	private static final int MH_NO_REEXPORTED_DYLIBS = 0x100000;
	private static final int MH_PIE = 0x200000;
	private static final int MH_DEAD_STRIPPABLE_DYLIB = 0x400000;
	private static final int MH_HAS_TLV_DESCRIPTORS = 0x800000;
	private static final int MH_NO_HEAP_EXECUTION = 0x1000000;
	private static final int MH_APP_EXTENSION_SAFE = 0x02000000;

	private static final int CPU_TYPE_X86 = 0x7;
	private static final int CPU_TYPE_X86_64 = 0x01000007;

	private static final int header64Size = 32;
	private static final int loadCommandSize = 8;

	private MachFile64 dumpFile;
	private MachFile64 executableMachFile;
	private MachFile64 dylinkerMachFile;
	private List<MachFile64> dylibMachFiles;

	private OSXProcessAddressSpace _process;
	private IModule _executable;
	private List<IModule> _modules;
	private int _signalNumber = -1;
	
	public class MachHeader64
	{
		public int magic;
		public int cpuType;
		public int cpuSubtype;
		public int fileType;
		public int numCommands;
		public long sizeCommands;
		public int flags;

		public MachHeader64() {}

		public MachHeader64(int magic, int cpuType, int cpuSubtype, int fileType, int numCommands, long sizeCommands, int flags)
		{
			this.magic = magic;
			this.cpuType = cpuType;
			this.cpuSubtype = cpuSubtype;
			this.fileType = fileType;
			this.numCommands = numCommands;
			this.sizeCommands = sizeCommands;
			this.flags = flags;
		}

	}

	public class MachFile64
	{
		public MachHeader64 header;
		public List<LoadCommand> loadCommands;
		public List<SegmentCommand64> segments;
		public List<LoadCommand> otherLoads;
		public List<ThreadCommand> threads;
		long streamOffset;

		public MachFile64() {}

		public Collection<? extends IMemorySource> getMemoryRangesWithOffset(long vmAddrOffset) throws IOException
		{
			List<IMemorySource> ranges = new LinkedList<>();
			for (SegmentCommand64 segment : segments) {
				if (segment.fileSize != 0) {
					// if we somehow can't get the stream length, the length() method returns -1 and we just create the memory source
					// otherwise, we check that memory segment indicated by the header doesn't run off the end of the file
					if ((MachoDumpReader.this._fileReader.length() < 0) || (streamOffset + segment.fileOffset + segment.fileSize <= MachoDumpReader.this._fileReader.length())) {
						ranges.add(new DumpMemorySource(segment.vmaddr + vmAddrOffset, segment.fileSize, streamOffset + segment.fileOffset, MachoDumpReader.this));
					} else {
						ranges.add(new UnbackedMemorySource(segment.vmaddr + vmAddrOffset, segment.fileSize, "segment is beyond file end"));
					}
				}
			}
			return ranges;
		}
	}

	public class OSXThread implements IOSThread
	{

		private final Map<String, Number> registers;
		private final Properties properties;
		private final long threadId;
		private final List<IMemoryRange> memoryRanges = new LinkedList<>();
		private final List<IOSStackFrame> stackFrames = new LinkedList<>();

		public OSXThread(long tid, ThreadCommand thread)
		{
			threadId = tid;
			registers = new TreeMap<>();
			// Since IOSThread doesn't have a sense of the different types of registers for a thread,
			// we add all the registers to the OSXThread. The registers all have different names, so 
			// there should be no conflict.
			for (ThreadState state : thread.states) {
				registers.putAll(state.registers);
			}
			properties = new Properties();
		}

		public long getThreadId() throws CorruptDataException
		{
			return threadId;
		}

		//TODO: unwind stack from the stack pointer
		public List<? extends IOSStackFrame> getStackFrames()
		{
			return null;
		}

		public Collection<? extends IRegister> getRegisters()
		{
			List<IRegister> regList = new ArrayList<IRegister>(registers.size());
			for (String regName : registers.keySet()) {
				Number value = registers.get(regName);

				regList.add(new Register(regName,value));
			}
			return regList;
		}

		public Collection<? extends IMemoryRange> getMemoryRanges()
		{
			return memoryRanges;
		}

		public long getInstructionPointer()
		{
			return registers.get("rip").longValue();
		}

		public long getBasePointer()
		{
			return registers.get("rbp").longValue();
		}

		public long getStackPointer()
		{
			return registers.get("rsp").longValue();
		}

		public Properties getProperties()
		{
			return properties;
		}
	}

	public MachoDumpReader(ImageInputStream in) throws IOException, InvalidDumpFormatException
	{
		this.coreFile = null;
		this._modules = new ArrayList<>();
		this.dylibMachFiles = new ArrayList<>();
		setReader(in);
		readCore();
	}

	public static boolean isMACHO(byte[] data)
	{
		int magic = readInt(data, 0);
		return isMACHO(magic);
	}

	private static boolean isMACHO(int magic)
	{
		return (magic == MACHO_64) || (magic == MACHO_64_REV);
	}

	public static ICore getReaderForFile(File f) throws IOException, InvalidDumpFormatException
	{
		ImageInputStream in = new FileImageInputStream(f);
		return getReaderForFile(in);
	}

	public static ICore getReaderForFile(ImageInputStream in) throws IOException, InvalidDumpFormatException
	{
		int magic = in.readInt();
		if (!isMACHO(magic)) {
			throw new InvalidDumpFormatException("The supplied file is not an Mach-O core dump.");
		}
		return new MachoDumpReader(in);
	}

	public String getCommandLine() throws DataUnavailableException
	{
		throw new DataUnavailableException("No command line available on OSX");
	}

	public Platform getPlatform()
	{
		return Platform.OSX;
	}

	public String getDumpFormat()
	{
		return "macho";
	}

	public Collection<? extends IAddressSpace> getAddressSpaces()
	{
		return Collections.singletonList((IAddressSpace)_process);
	}

	public Properties getProperties()
	{
		Properties props = new Properties();
		
		props.setProperty(ICore.SYSTEM_TYPE_PROPERTY, "OSX");
		props.setProperty(ICore.PROCESSOR_TYPE_PROPERTY, getCpuType());
		props.setProperty(ICore.PROCESSOR_SUBTYPE_PROPERTY, "");
		
		return props;
	}

	public void executablePathHint(String path)
	{
		if (_executable != null) {
			try {
				_executable = new Module(_process, path, (List<? extends ISymbol>)_executable.getSymbols(), _executable.getMemoryRanges(), _executable.getLoadAddress(), _executable.getProperties());
			} catch (DataUnavailableException e) {
				//do nothing, since we're simply recreating the executable module
			}
		}
	}

	public IModule getExecutable()
	{
		return _executable;
	}

	public List<? extends IModule> getModules()
	{
		return _modules;
	}

	public long getProcessId()
	{
		return 0;
	}

	public List<? extends IOSThread> getThreads() throws CorruptDataException
	{
		List<IOSThread> threads = new ArrayList<>(dumpFile.threads.size());
		for (ThreadCommand command : dumpFile.threads) {
				OSXThread nativeThread = new OSXThread(0, command);
				threads.add(nativeThread);
				if (_signalNumber == -1) {
					// will use the first thread's exception values, which should be the primary thread
					if (nativeThread.registers.containsKey("err")) {
						_signalNumber = nativeThread.registers.get("err").intValue();
					} else {
						throw new CorruptDataException("Core dump missing thread exception registers.");
					}
			}
		}
		return threads;
	}

	public int getSignalNumber() throws DataUnavailableException
	{
		if (_signalNumber == -1) {
			try {
				getThreads();
			} catch (CorruptDataException e) {
				throw new DataUnavailableException("Core dump missing thread exception registers.", e);
			}
		}
		return _signalNumber;
	}

	/**
	 * A Mach-O core on OSX typically consists of a number of memory segments,
	 * preceded by the load commands for the core itself. Each memory segment
	 * either contains another Mach-O file or just data.
	 * Thus, we iterate through each segment in order to find the Mach files
	 * representing the executable and the shared libraries.
	 */
	private void readCore() throws IOException, InvalidDumpFormatException
	{
		dumpFile = readMachFile(0);
		if (dumpFile.header.fileType != MH_CORE) {
			throw new InvalidDumpFormatException("The supplied file is not an Mach-O core dump.");
		}
		_process = new OSXProcessAddressSpace(8, _fileReader.getByteOrder(), this);

		Collection<? extends IMemorySource> coreMemoryRanges = dumpFile.getMemoryRangesWithOffset(0);
		_process.addMemorySources(coreMemoryRanges);

		for (SegmentCommand64 segment : dumpFile.segments) {
			seek(segment.fileOffset);
			try {
				int magic = readInt();
				if (isMACHO(magic)) {
					MachFile64 innerFile = readMachFile(segment.fileOffset);
					switch (innerFile.header.fileType) {
						case MH_EXECUTE:
							executableMachFile = innerFile;
							_executable = processExecutableFile(innerFile, segment);
							break;
						case MH_DYLIB:
							dylibMachFiles.add(innerFile);
							_modules.add(processModuleFile(innerFile, segment));
							break;
						case MH_DYLINKER:
							dylinkerMachFile = innerFile;
							break;
						default:
							break;
					}
				}
			} catch (EOFException e) {
				// nothing to process if we have a truncated file
			}
		}
	}

	public MachFile64 readMachFile(long fileOffset) throws IOException, InvalidDumpFormatException
	{
		MachFile64 machfile = new MachFile64();
		machfile.streamOffset = fileOffset;
		machfile.header = readHeader(fileOffset);
		if (machfile.header.numCommands > 0) {
			machfile.loadCommands = new ArrayList<>(machfile.header.numCommands);
			machfile.segments = new ArrayList<>();
			machfile.otherLoads = new ArrayList<>();
			machfile.threads = new ArrayList<>();
		}
		long currentOffset = fileOffset + header64Size;
		for (int i = 0; i < machfile.header.numCommands; i++) {
			LoadCommand command = LoadCommand.readFullCommand(_fileReader, currentOffset, fileOffset);
			if (command instanceof SegmentCommand64) {
				SegmentCommand64 segment = (SegmentCommand64) command;
				machfile.segments.add(segment);
			} else if (command instanceof ThreadCommand) {
				machfile.threads.add((ThreadCommand)command);
			} else {
				machfile.otherLoads.add(command);
			}
			machfile.loadCommands.add(command);
			currentOffset += command.cmdSize;
		}
		return machfile;
	}

	/**
	 * The two methods processing modules currently do not read in the symbols.
	 * The symbol data from the core is not necessary for DDR, as later stages
	 * can collect the symbols from the VM itself.
	 */
	private IModule processExecutableFile(MachFile64 executableFile, SegmentCommand64 container) throws IOException, InvalidDumpFormatException
	{
		List<ISymbol> symbols = new ArrayList<>();
		Collection<? extends IMemoryRange> memoryRanges = executableFile.getMemoryRangesWithOffset(container.vmaddr);
		Module m = new Module(_process, "executable", symbols, memoryRanges, executableFile.streamOffset, new Properties());
		return m;
	}

	private IModule processModuleFile(MachFile64 moduleFile, SegmentCommand64 container) throws IOException, InvalidDumpFormatException
	{
		DylibCommand dylib = (DylibCommand) moduleFile.otherLoads.stream().filter(c -> (c.cmdType == LoadCommand.LC_ID_DYLIB)).findFirst().get();
		String moduleName = dylib.dylib.name.value;
		List<ISymbol> symbols = new ArrayList<>();
		Collection<? extends IMemoryRange> memoryRanges = moduleFile.getMemoryRangesWithOffset(container.vmaddr);
		Module m = new Module(_process, moduleName, symbols, memoryRanges, moduleFile.streamOffset, new Properties());
		return m;
	}

	public MachHeader64 readHeader(long offset) throws IOException, InvalidDumpFormatException
	{
		seek(offset);
		MachHeader64 header = new MachHeader64();
		header.magic = readInt();
		if (header.magic == MACHO_64_REV) {
			_fileReader.setByteOrder((_fileReader.getByteOrder() == ByteOrder.BIG_ENDIAN) ? ByteOrder.LITTLE_ENDIAN : ByteOrder.BIG_ENDIAN);
		}
		header.cpuType = readInt();
		header.cpuSubtype = readInt();
		header.fileType = readInt();
		header.numCommands = readInt();
		header.sizeCommands = Integer.toUnsignedLong(readInt());
		header.flags = readInt();
		readInt(); //reserved section
		return header;
	}

	private String getCpuType()
	{
		if (dumpFile.header.cpuType == CPU_TYPE_X86_64) {
			return "X86_64";
		}
		else {
			return "";
		}
	}

}
