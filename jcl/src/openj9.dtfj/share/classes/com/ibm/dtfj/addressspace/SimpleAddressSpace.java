/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.dtfj.addressspace;

import java.io.IOException;


import com.ibm.dtfj.corereaders.ClosingFileReader;
import com.ibm.dtfj.corereaders.MemoryAccessException;
import com.ibm.dtfj.corereaders.MemoryRange;

/**
 * @author jmdisher
 * Exists to provide a bridge by which to support the deprecated core readers while we use IAbstractAddressSpace
 * to enhance the capability of memory reading for more complicated platforms.
 * 
 * Ideally, this will be deprecated in the future.
 */
public class SimpleAddressSpace  extends CommonAddressSpace
{
	private ClosingFileReader _backing;
	
	
	public SimpleAddressSpace(MemoryRange[] deprecatedMemoryRanges, ClosingFileReader file, boolean isLittleEndian, boolean is64Bit)
	{
		super(deprecatedMemoryRanges, isLittleEndian, is64Bit);
		_backing = file;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.addressspace.IAbstractAddressSpace#isExecutable(int, long)
	 */
	public boolean isExecutable(int asid, long address)
			throws MemoryAccessException
	{
		// TODO Auto-generated method stub
		return false;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.addressspace.IAbstractAddressSpace#isReadOnly(int, long)
	 */
	public boolean isReadOnly(int asid, long address)
			throws MemoryAccessException
	{
		// TODO Auto-generated method stub
		return false;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.addressspace.IAbstractAddressSpace#isShared(int, long)
	 */
	public boolean isShared(int asid, long address)
			throws MemoryAccessException
	{
		// TODO Auto-generated method stub
		return false;
	}
	
	public int getBytesAt(int asid, long address, byte[] buffer) throws MemoryAccessException
	{
		MemoryRange resident = _residentRange(asid, address);
		long readLocation = resident.getFileOffset() + (address - resident.getVirtualAddress());
		try {
			_backing.seek(readLocation);
			_backing.readFully(buffer);
		} catch (IOException e) {
			throw new MemoryAccessException(asid, address);
		}
		return buffer.length;
	}
}
