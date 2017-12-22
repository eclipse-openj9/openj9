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

class AIX32DumpReader extends AIXDumpReader
{
	private static final int CONTEXT_OFFSET_IN_THREAD = 200; // offsetof(thrdctx,
																// hctx)
	private static final int IAR_OFFSET_IN_CONTEXT = 24; // offsetof(mstsave,
															// iar)
	private static final int GPR_OFFSET_IN_CONTEXT = 208; // offsetof(mstsave,
															// gpr)
	private static final int GPR_COUNT = 32;
	private static final String VMID = "j9vmap32";

	// private static final int FPR_COUNT = 32;

	public AIX32DumpReader(File file, ClosingFileReader fileReader) throws FileNotFoundException,
			InvalidDumpFormatException, IOException
	{
		this.coreFile = file;
		setReader(fileReader);
		readCore();
	}
	
	public AIX32DumpReader(ImageInputStream in) throws FileNotFoundException,
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

	public boolean is64Bit()
	{
		return false;
	}

	protected int readLoaderInfoFlags() throws IOException
	{
		return 0; // Do nothing
	}

	protected long userInfoOffset()
	{
		// offsetof(core_dumpx, c_u)
		return 1008;
	}

	protected int pointerSize()
	{
		return 32;
	}

	protected long readAddress() throws IOException
	{
		return 0xFFFFFFFFL & readInt();
	}

	protected Map<String, Number> readRegisters(long threadOffset)
			throws IOException
	{
		Map<String, Number> registers = new TreeMap<String, Number>();
		seek(threadOffset + CONTEXT_OFFSET_IN_THREAD + IAR_OFFSET_IN_CONTEXT);
		registers.put("iar", Integer.valueOf(readInt()));
		registers.put("msr", Integer.valueOf(readInt()));
		registers.put("cr", Integer.valueOf(readInt()));
		registers.put("lr", Integer.valueOf(readInt()));
		registers.put("ctr", Integer.valueOf(readInt()));
		registers.put("xer", Integer.valueOf(readInt()));
		registers.put("mq", Integer.valueOf(readInt()));
		registers.put("tid", Integer.valueOf(readInt()));
		registers.put("fpscr", Integer.valueOf(readInt()));
		seek(threadOffset + CONTEXT_OFFSET_IN_THREAD + GPR_OFFSET_IN_CONTEXT);
		for (int i = 0; i < GPR_COUNT; i++)
			registers.put("gpr" + i, Integer.valueOf(readInt()));
		return registers;
	}

	protected long threadSize(long threadOffset)
	{
		// sizeof(thrdctx)
		return 792;
	}

	protected long getStackPointerFrom(Map<String, Number> registers)
	{
		/*
		 * File jit/codert/SignalHandler.c:547 states that the java SP is r14. Does it matter for jit resolve frames that
		 * we're using the native stack pointer?
		 */
		return registers.get("gpr1").intValue() & 0xffffffffL;
	}

	protected long getInstructionPointerFrom(Map<String, Number> registers)
	{
		return registers.get("iar").intValue() & 0xffffffffL;
	}

	protected long getLinkRegisterFrom(Map<String, Number> registers)
	{
		return registers.get("lr").intValue() & 0xffffffffL;
	}

	protected int sizeofTopOfStack()
	{
		// see struct top_of_stack in execargs.h
		return 140 + 4; // this is sizeof(top_of_stack) aligned to 8 bytes since
						// that seems to be an unspoken requirement of this
						// structure
	}

}
