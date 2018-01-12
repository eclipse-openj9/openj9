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
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.Map;
import java.util.TreeMap;

import javax.imageio.stream.ImageInputStream;

import com.ibm.j9ddr.corereaders.ClosingFileReader;
import com.ibm.j9ddr.corereaders.InvalidDumpFormatException;

class AIX64DumpReader extends AIXDumpReader
{
	private static final long THRDENTRY64_V1_SIZE = 424;
	private static final long THRDENTRY64_V2_SIZE = 512;
	private static final long THRDCTX64_V1 = 1000;
	private static final long THRDCTX64_V2 = 1088;
	
	private static final int GPR_COUNT = 32;
	private static final String VMID = "j9vmap64";
	
	private boolean hasVersionBeenDetermined = false;
	//set structure sizes to default values of version 1
	private long sizeofThreadCtx64 = THRDCTX64_V1;
	private long sizeofThreadEntry64 = THRDENTRY64_V1_SIZE;

	// private static final int FPR_COUNT = 32;

	public AIX64DumpReader(File file, ClosingFileReader fileReader) throws FileNotFoundException,
			InvalidDumpFormatException, IOException
	{
		this.coreFile = file;
		setReader(fileReader);
		readCore();
	}
	
	public AIX64DumpReader(ImageInputStream in) throws FileNotFoundException,
		InvalidDumpFormatException, IOException
	{
		this.coreFile = null;
		setReader(in);
		readCore();
	}

	public String getVMID()
	{
		return VMID;
	}

	protected int readLoaderInfoFlags() throws IOException
	{
		return _fileReader.readInt();
	}

	protected long userInfoOffset()
	{
		// offsetof(core_dumpxx, c_u)
		return 1216;
	}

	public boolean is64Bit()
	{
		return true;
	}

	protected Map<String, Number> readRegisters(long threadOffset)
			throws IOException
	{
		if(!hasVersionBeenDetermined) {
			calculateThreadStructureSizes(threadOffset);
		}
		_fileReader.seek(threadOffset + sizeofThreadEntry64);
		Map<String, Number> registers = new TreeMap<String, Number>();
		for (int i = 0; i < GPR_COUNT; i++)
			registers.put("gpr" + i, Long.valueOf(readLong()));
		registers.put("msr", Long.valueOf(readLong()));
		registers.put("iar", Long.valueOf(readLong()));
		registers.put("lr", Long.valueOf(readLong()));
		registers.put("ctr", Long.valueOf(readLong()));
		registers.put("cr", Integer.valueOf(readInt()));
		registers.put("xer", Integer.valueOf(readInt()));
		registers.put("fpscr", Integer.valueOf(readInt()));
		return registers;
	}
	
	/**
	 * Sniff test to see if the stack pointer register value is valid, and set struct sizes accordingly
	 * @param threadOffset
	 * @return
	 */
	public void calculateThreadStructureSizes(long threadOffset) {
		long address;
		try {
			_fileReader.seek(threadOffset + THRDENTRY64_V1_SIZE + 8);
			address = readLong();
		} catch (IOException e) {
			//error reading the offset and so return the default values of version 1
			return;
		}
		if(null == getProcess().getMemoryRangeForAddress(address)) {
			sizeofThreadEntry64 = THRDENTRY64_V2_SIZE;
			sizeofThreadCtx64 = THRDCTX64_V2;
		} else {
			sizeofThreadEntry64 = THRDENTRY64_V1_SIZE;
			sizeofThreadCtx64 = THRDCTX64_V1;
		}
		hasVersionBeenDetermined = true;
	}

	protected long threadSize(long threadOffset)
	{
		if(!hasVersionBeenDetermined) {
			calculateThreadStructureSizes(threadOffset);
		}
		return sizeofThreadCtx64;	// sizeof(thrdctx64)
	}

	protected long getStackPointerFrom(Map<String, Number> registers)
	{
		/*
		 * File jit/codert/SignalHandler.c:547 states that the java SP is r14. Does it matter for jit resolve frames that
		 * we're using the native stack pointer?
		 */
		return ((Long) registers.get("gpr1")).longValue();
	}

	protected long getInstructionPointerFrom(Map<String, Number> registers)
	{
		return ((Long) registers.get("iar")).longValue();
	}

	protected long getLinkRegisterFrom(Map<String, Number> registers)
	{
		return ((Long) registers.get("lr")).longValue();
	}

	protected int sizeofTopOfStack()
	{
		// see struct top_of_stack in execargs.h
		return 304; // this is sizeof(top_of_stack)
	}

	protected int pointerSize()
	{
		return 64;
	}

	protected long readAddress() throws IOException
	{
		return readLong();
	}
}
