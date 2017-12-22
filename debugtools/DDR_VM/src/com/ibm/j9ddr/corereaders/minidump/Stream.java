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
package com.ibm.j9ddr.corereaders.minidump;

abstract class Stream
{
	final static int PROCESSOR_ARCHITECTURE_INTEL = 0;
	// PROCESSOR_ARCHITECTURE_MIPS = 1;
	// PROCESSOR_ARCHITECTURE_ALPHA = 2;
	// PROCESSOR_ARCHITECTURE_PPC = 3;
	// PROCESSOR_ARCHITECTURE_SHX = 4;
	// PROCESSOR_ARCHITECTURE_ARM = 5;
	final static int PROCESSOR_ARCHITECTURE_IA64 = 6;
	final static int PROCESSOR_ARCHITECTURE_ALPHA64 = 7;
	// PROCESSOR_ARCHITECTURE_MSIL = 8;
	final static int PROCESSOR_ARCHITECTURE_AMD64 = 9;
	final static int PROCESSOR_ARCHITECTURE_IA32_ON_WIN64 = 10;

	// Stream types
	// UNUSED = 0;
	// RESERVED0 = 1;
	// RESERVED1 = 2;
	public final static int THREADLIST = 3;
	public final static int MODULELIST = 4;
	// MEMORYLIST = 5;
	// EXCEPTIONLIST = 6;
	public final static int SYSTEMINFO = 7;
	// THREADEXLIST = 8;
	public final static int MEMORY64LIST = 9;
	public final static int MISCINFO = 15;
	public final static int MEMORYINFO = 16;
	public final static int THREADINFO = 17;

	private int _dataSize;

	private long _location;

	protected Stream(int dataSize, long location)
	{
		_dataSize = dataSize;
		_location = location;
	}

	protected int getDataSize()
	{
		return _dataSize;
	}

	protected long getLocation()
	{
		return _location;
	}

	/**
	 * This is part of a hack which allows us to find out the native word size
	 * of a Windows MiniDump since it doesn't seem to expose that in any nice
	 * way.
	 * 
	 * @return 0, unless this is a stream which can determine the value, in that
	 *         case it returns the value (32 or 64)
	 */
	public int readPtrSize(MiniDumpReader reader)
	{
		return 0;
	}

}
