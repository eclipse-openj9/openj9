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

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Properties;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.memory.DumpMemorySource;
import com.ibm.j9ddr.corereaders.memory.IAddressSpace;
import com.ibm.j9ddr.corereaders.memory.IMemoryRange;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.corereaders.osthread.IRegister;
import com.ibm.j9ddr.corereaders.osthread.Register;

class ThreadStream extends LateInitializedStream
{
	public ThreadStream(int dataSize, int location)
	{
		super(dataSize, location);
	}

	private static class WindowsOSThread extends BaseWindowsOSThread
	{
		private final long threadId;
		private final long stackStart;
		private final long stackSize;
		private final long stackRva;
		private final long contextRva;
		private final Properties properties;
		private final MiniDumpReader reader;
		private List<IRegister> registers;
		
		public WindowsOSThread(int threadId, long stackStart, long stackSize,
				long stackRva, long contextRva, Properties properties, MiniDumpReader reader, IProcess process)
		{
			super(process);
			this.threadId = threadId;
			this.stackStart = stackStart;
			this.stackSize = stackSize;
			this.stackRva = stackRva;
			this.contextRva = contextRva;
			this.properties = properties;
			this.reader = reader;
		}

		public Collection<? extends IMemoryRange> getMemoryRanges()
		{
			return Collections.singletonList(new DumpMemorySource(stackStart, stackSize, stackRva, 0, reader, "Stack"));
		}

		public List<IRegister> getRegisters()
		{
			if (null == registers) {
				registers = new ArrayList<IRegister>();
				try {
					switch (reader.getProcessorArchitecture()) {
						case PROCESSOR_ARCHITECTURE_AMD64:
							readAmd64Registers();
							break;
						case PROCESSOR_ARCHITECTURE_INTEL:
							readIntelRegisters();
							break;
						default:
							throw new RuntimeException("Unknown processor architecture: " + reader.getProcessorArchitecture());
					}
				} catch (IOException ex) {
					//TODO handle IOException
				}
			}
			return Collections.unmodifiableList(registers);
		}
		
		/*
		 * #define SIZE_OF_80387_REGISTERS 80
		 * 
		 * typedef struct _FLOATING_SAVE_AREA { DWORD ControlWord; DWORD StatusWord;
		 * DWORD TagWord; DWORD ErrorOffset; DWORD ErrorSelector; DWORD DataOffset;
		 * DWORD DataSelector; BYTE RegisterArea[SIZE_OF_80387_REGISTERS]; DWORD
		 * Cr0NpxState; } FLOATING_SAVE_AREA;
		 * 
		 * typedef struct _CONTEXT { DWORD ContextFlags; DWORD Dr0; DWORD Dr1; DWORD
		 * Dr2; DWORD Dr3; DWORD Dr6; DWORD Dr7; FLOATING_SAVE_AREA FloatSave; //
		 * 112 bytes DWORD SegGs; DWORD SegFs; DWORD SegEs; DWORD SegDs; DWORD Edi;
		 * DWORD Esi; DWORD Ebx; DWORD Edx; DWORD Ecx; DWORD Eax; DWORD Ebp; DWORD
		 * Eip; DWORD SegCs; DWORD EFlags; DWORD Esp; DWORD SegSs; BYTE
		 * ExtendedRegisters[MAXIMUM_SUPPORTED_EXTENSION]; } CONTEXT;
		 */
		private void readIntelRegisters() throws IOException {
			// We capture segment registers, flags, integer registers and instruction pointer
			reader.seek(contextRva + 140);
			registers.add(new Register("gs",reader.readInt()));
			registers.add(new Register("fs",reader.readInt()));
			registers.add(new Register("es",reader.readInt()));
			registers.add(new Register("ds",reader.readInt()));
			registers.add(new Register("edi",reader.readInt()));
			registers.add(new Register("esi",reader.readInt()));
			registers.add(new Register("ebx",reader.readInt()));
			registers.add(new Register("edx",reader.readInt()));
			registers.add(new Register("ecx",reader.readInt()));
			registers.add(new Register("eax",reader.readInt()));
			registers.add(new Register("ebp",reader.readInt()));
			registers.add(new Register("eip",reader.readInt()));
			registers.add(new Register("cs",reader.readInt()));
			registers.add(new Register("flags",reader.readInt()));
			registers.add(new Register("esp",reader.readInt()));
			registers.add(new Register("ss",reader.readInt()));
		}

		/*
		 * typedef struct DECLSPEC_ALIGN(16) _CONTEXT { DWORD64 P1Home; DWORD64
		 * P2Home; DWORD64 P3Home; DWORD64 P4Home; DWORD64 P5Home; DWORD64 P6Home;
		 * DWORD ContextFlags; DWORD MxCsr; WORD SegCs; WORD SegDs; WORD SegEs; WORD
		 * SegFs; WORD SegGs; WORD SegSs; DWORD EFlags; DWORD64 Dr0; DWORD64 Dr1;
		 * DWORD64 Dr2; DWORD64 Dr3; DWORD64 Dr6; DWORD64 Dr7; DWORD64 Rax; DWORD64
		 * Rcx; DWORD64 Rdx; DWORD64 Rbx; DWORD64 Rsp; DWORD64 Rbp; DWORD64 Rsi;
		 * DWORD64 Rdi; DWORD64 R8; DWORD64 R9; DWORD64 R10; DWORD64 R11; DWORD64
		 * R12; DWORD64 R13; DWORD64 R14; DWORD64 R15; DWORD64 Rip; M128 Xmm0; M128
		 * Xmm1; M128 Xmm2; M128 Xmm3; M128 Xmm4; M128 Xmm5; M128 Xmm6; M128 Xmm7;
		 * M128 Xmm8; M128 Xmm9; M128 Xmm10; M128 Xmm11; M128 Xmm12; M128 Xmm13;
		 * M128 Xmm14; M128 Xmm15; LEGACY_SAVE_AREA FltSave; DWORD Fill; DWORD64
		 * DebugControl; DWORD64 LastBranchToRip; DWORD64 LastBranchFromRip; DWORD64
		 * LastExceptionToRip; DWORD64 LastExceptionFromRip; DWORD64 Fill1; }
		 * CONTEXT, *PCONTEXT;
		 */
		private void readAmd64Registers() throws IOException {
		// We capture segment registers, flags, integer registers and instruction pointer
			reader.seek(contextRva + 56);
			registers.add(new Register("cs", reader.readShort()));
			registers.add(new Register("ds", reader.readShort()));
			registers.add(new Register("es", reader.readShort()));
			registers.add(new Register("fs", reader.readShort()));
			registers.add(new Register("gs", reader.readShort()));
			registers.add(new Register("ss", reader.readShort()));
			registers.add(new Register("flags", reader.readInt()));
			reader.seek(contextRva + 120);
			registers.add(new Register("rax", reader.readLong()));
			registers.add(new Register("rcx", reader.readLong()));
			registers.add(new Register("rdx", reader.readLong()));
			registers.add(new Register("rbx", reader.readLong()));
			registers.add(new Register("rsp", reader.readLong()));
			registers.add(new Register("rbp", reader.readLong()));
			registers.add(new Register("rsi", reader.readLong()));
			registers.add(new Register("rdi", reader.readLong()));
			registers.add(new Register("r8", reader.readLong()));
			registers.add(new Register("r9", reader.readLong()));
			registers.add(new Register("r10", reader.readLong()));
			registers.add(new Register("r11", reader.readLong()));
			registers.add(new Register("r12", reader.readLong()));
			registers.add(new Register("r13", reader.readLong()));
			registers.add(new Register("r14", reader.readLong()));
			registers.add(new Register("r15", reader.readLong()));
			registers.add(new Register("rip", reader.readLong()));
		}


		public long getThreadId()
		{
			return threadId;
		}

		@Override
		protected long getStackEnd()
		{
			return stackStart + stackSize;
		}

		@Override
		protected long getStackStart()
		{
			return stackStart;
		}

		public Properties getProperties()
		{
			return properties;
		}

		public long getInstructionPointer() {
			switch (reader.getProcessorArchitecture()) {
				case PROCESSOR_ARCHITECTURE_AMD64:
					return getValueOfNamedRegister(getRegisters(), "rip");
			case PROCESSOR_ARCHITECTURE_INTEL:
					return getValueOfNamedRegister(getRegisters(), "eip");
			default:
					String msg = "Unknow processor architecture: " + reader.getProcessorArchitecture();
					throw new RuntimeException(msg);
			}
		}

		public long getBasePointer() {
			switch (reader.getProcessorArchitecture()) {
			case PROCESSOR_ARCHITECTURE_AMD64:
				return getValueOfNamedRegister(getRegisters(), "rbp");
			case PROCESSOR_ARCHITECTURE_INTEL:
					return getValueOfNamedRegister(getRegisters(), "ebp");
			default:
					String msg = "Unknow processor architecture: " + reader.getProcessorArchitecture();
					throw new RuntimeException(msg);
			}
		}

		public long getStackPointer() {
			switch (reader.getProcessorArchitecture()) {
			case PROCESSOR_ARCHITECTURE_AMD64:
				return getValueOfNamedRegister(getRegisters(), "rsp");
			case PROCESSOR_ARCHITECTURE_INTEL:
					return getValueOfNamedRegister(getRegisters(), "esp");
			default:
					String msg = "Unknow processor architecture: " + reader.getProcessorArchitecture();
					throw new RuntimeException(msg);
			}
		}
	}
	
	@Override
	public void readFrom(MiniDumpReader dump, IAddressSpace addressSpace,
			boolean is64Bit) throws CorruptDataException, IOException
	{
		dump.seek(getLocation());
		
		int numberOfThreads = dump.readInt();
		
		if (numberOfThreads > 100000) {
			throw new CorruptDataException("Unlikely number of threads found in dump: " + numberOfThreads + ". Suspect data corruption");
		}
		
		for (int i = 0; i < numberOfThreads; i++) {
			dump.seek(getLocation() + 4 + i * 48);
			int threadId = dump.readInt();
			dump.readInt(); // Ignore suspendCount
			int priorityClass = dump.readInt();
			int priority = dump.readInt();
			dump.readLong(); // Ignore teb
			long stackStart = dump.readLong();
			long stackSize = 0xFFFFFFFFL & dump.readInt();
			long stackRva = 0xFFFFFFFFL & dump.readInt();
			dump.readInt(); //contextDataSize
			long contextRva = 0xFFFFFFFFL & dump.readInt();
			
			Properties properties = new Properties();
			properties.setProperty("priorityClass", String.valueOf(priorityClass));
			properties.setProperty("priority", String.valueOf(priority));
			
			dump.addThread(new WindowsOSThread(threadId, stackStart, stackSize, stackRva, contextRva, properties, dump, (IProcess)addressSpace));
		}
		
	}

}
