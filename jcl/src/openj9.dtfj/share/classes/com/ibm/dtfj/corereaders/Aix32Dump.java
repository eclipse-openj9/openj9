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

//CMVC 166477 : moved this from a static inner class to it's own class 

public class Aix32Dump extends NewAixDump {
	private static final int CONTEXT_OFFSET_IN_THREAD = 200; // offsetof(thrdctx, hctx)
	private static final int IAR_OFFSET_IN_CONTEXT = 24; // offsetof(mstsave, iar)
	private static final int GPR_OFFSET_IN_CONTEXT = 208; // offsetof(mstsave, gpr)
	private static final int GPR_COUNT = 32;
	//private static final int FPR_COUNT = 32;
	
	protected Aix32Dump(DumpReader reader) throws IOException
	{
		super(reader);
		readCore();
	}

	protected int readLoaderInfoFlags() throws IOException {
		return 0; // Do nothing
	}

	protected long userInfoOffset() {
		// offsetof(core_dumpx, c_u)
		return 1008;
	}

	protected int pointerSize() {
		return 32;
	}

	protected Map readRegisters(long threadOffset) throws IOException {
		Map registers = new TreeMap();
		coreSeek(threadOffset + CONTEXT_OFFSET_IN_THREAD + IAR_OFFSET_IN_CONTEXT);
		registers.put("iar", Integer.valueOf(coreReadInt()));
		registers.put("msr", Integer.valueOf(coreReadInt()));
		registers.put("cr", Integer.valueOf(coreReadInt()));
		registers.put("lr", Integer.valueOf(coreReadInt()));
		registers.put("ctr", Integer.valueOf(coreReadInt()));
		registers.put("xer", Integer.valueOf(coreReadInt()));
		registers.put("mq", Integer.valueOf(coreReadInt()));
		registers.put("tid", Integer.valueOf(coreReadInt()));
		registers.put("fpscr", Integer.valueOf(coreReadInt()));
		coreSeek(threadOffset + CONTEXT_OFFSET_IN_THREAD + GPR_OFFSET_IN_CONTEXT);
		for (int i = 0; i < GPR_COUNT; i++)
			registers.put("gpr" + i, Integer.valueOf(coreReadInt()));
		return registers;
	}

	protected long threadSize(long threadOffset) {
		// sizeof(thrdctx)
		return 792;
	}

	protected long getStackPointerFrom(Map registers) {
		return ((Integer) registers.get("gpr1")).intValue() & 0xffffffffL;
	}

	protected long getInstructionPointerFrom(Map registers) {
		return ((Integer) registers.get("iar")).intValue() & 0xffffffffL;
	}

	protected long getLinkRegisterFrom(Map registers) {
		return ((Integer) registers.get("lr")).intValue() & 0xffffffffL;
	}
	
	protected int sizeofTopOfStack() {
		// see struct top_of_stack in execargs.h
		return 140 + 4;	//this is sizeof(top_of_stack) aligned to 8 bytes since that seems to be an unspoken requirement of this structure
	}
}
