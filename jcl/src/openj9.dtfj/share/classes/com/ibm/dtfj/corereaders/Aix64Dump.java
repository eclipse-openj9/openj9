/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
import java.util.Map;
import java.util.TreeMap;

/*
 * CMVC 166477 : 
 * 	- moved the AIX 64 bit subclass from an inner class
 * 	- created known offsets for an arbitrarily named version 1 and 2 of the structures
 * 	- added function to determine the size of the structure by testing the validity of the stack pointer
 * 		which has been derived from a register. These structures are not versioned so there is no definitive way
 * 		to determine the shape of the structure.
 */

public class Aix64Dump extends NewAixDump {
	private static final long THRDENTRY64_V1_SIZE = 424;
	private static final long THRDENTRY64_V2_SIZE = 512;
	private static final long THRDCTX64_V1 = 1000;
	private static final long THRDCTX64_V2 = 1088;
	
	private static final int GPR_COUNT = 32;
	
	private boolean hasVersionBeenDetermined = false;
	//set structure sizes to default values of version 1
	private long sizeofThreadCtx64 = THRDCTX64_V1;
	private long sizeofThreadEntry64 = THRDENTRY64_V1_SIZE;

	protected Aix64Dump(DumpReader reader) throws IOException
	{
		super(reader);
		readCore();
	}

	protected int readLoaderInfoFlags() throws IOException {
		return coreReadInt();
	}
	
	protected long userInfoOffset() {
		// offsetof(core_dumpxx, c_u)
		return 1216;
	}

	protected int pointerSize() {
		return 64;
	}

	protected Map readRegisters(long threadOffset) throws IOException {
		if(!hasVersionBeenDetermined) {
			calculateThreadStructureSizes(threadOffset);
		}
		coreSeek(threadOffset + sizeofThreadEntry64);
		Map registers = new TreeMap();
		for (int i = 0; i < GPR_COUNT; i++)
			registers.put("gpr" + i, Long.valueOf(coreReadLong()));
		registers.put("msr", Long.valueOf(coreReadLong()));
		registers.put("iar", Long.valueOf(coreReadLong()));
		registers.put("lr", Long.valueOf(coreReadLong()));
		registers.put("ctr", Long.valueOf(coreReadLong()));
		registers.put("cr", Integer.valueOf(coreReadInt()));
		registers.put("xer", Integer.valueOf(coreReadInt()));
		registers.put("fpscr", Integer.valueOf(coreReadInt()));
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
			coreSeek(threadOffset + THRDENTRY64_V1_SIZE + 8);
			address = coreReadLong();
		} catch (IOException e) {
			//error reading the offset and so return the default values of version 1
			return;
		}
		if(null == memoryRangeFor(address)) {
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

	protected long getStackPointerFrom(Map registers) {
		return ((Long) registers.get("gpr1")).longValue();
	}

	protected long getInstructionPointerFrom(Map registers) {
		return ((Long) registers.get("iar")).longValue();
	}

	protected long getLinkRegisterFrom(Map registers) {
		return ((Long) registers.get("lr")).longValue();
	}
	
	protected int sizeofTopOfStack() {
		// see struct top_of_stack in execargs.h
		return 304;	//this is sizeof(top_of_stack)
	}
}
