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
package com.ibm.dtfj.addressspace;

import java.io.IOException;

import java.util.Iterator;
import java.util.TreeMap;
import java.util.Vector;

import com.ibm.dtfj.corereaders.ClosingFileReader;
import com.ibm.dtfj.corereaders.MemoryAccessException;
import com.ibm.dtfj.corereaders.MemoryRange;

/**
 * @author jmdisher
 *
 */
public class LayeredAddressSpace extends CommonAddressSpace
{
	private TreeMap _moduleRanges = new TreeMap();
	private IAbstractAddressSpace _base;
	private MemoryRange[] _moduleRangesArray = null; 
	private Integer _lastModuleRange = Integer.valueOf(0);
	
	public LayeredAddressSpace(IAbstractAddressSpace base, boolean isLittleEndian, boolean is64Bit)
	{
		super(_extractRanges(base.getMemoryRanges()), isLittleEndian, is64Bit);
		_base = base;
	}

	private static MemoryRange[] _extractRanges(Iterator memoryRanges)
	{
		Vector ranges = new Vector();
		while (memoryRanges.hasNext()) {
			ranges.add(memoryRanges.next());
		}
		return (MemoryRange[])ranges.toArray(new MemoryRange[ranges.size()]);
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.addressspace.IAbstractAddressSpace#getMemoryRanges()
	 */
	public Iterator getMemoryRanges()
	{
		//TODO:  hook in here to stick on the extra ranges which have been added at the beginning of the iterator
		return super.getMemoryRanges();
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

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.addressspace.IAbstractAddressSpace#getBytesAt(int, long, byte[])
	 */
	public int getBytesAt(int asid, long address, byte[] buffer)
			throws MemoryAccessException
	{
		if ( null == _moduleRangesArray) {
			_moduleRangesArray = (MemoryRange[]) _moduleRanges.keySet().toArray(new MemoryRange[0]);
		}

		int retI = findWhichMemoryRange(asid, address, _moduleRangesArray, _lastModuleRange, false);
		
		if (retI > -1) {
			MemoryRange range = (MemoryRange) _moduleRangesArray[retI];
			if (range.contains(address)) {
				ClosingFileReader readable = (ClosingFileReader) _moduleRanges.get(range);
				try {
					long fileOffset = range.getFileOffset() + (address - range.getVirtualAddress());
					readable.seek(fileOffset);
					readable.readFully(buffer);
					return buffer.length;
				} catch (IOException ex) {
					System.out.println(">> Memory access exception in getBytesAt");
					throw new MemoryAccessException(asid, address);
				}
			}
		}
		
		//this must not be in one of the newer regions
		return _base.getBytesAt(asid, address, buffer);
	}


	public void mapRegion(long virtualAddress, ClosingFileReader residentFile, long fileOffset, long size)
	{
		MemoryRange range = new MemoryRange(virtualAddress, fileOffset, size);
		_moduleRanges.put(range,residentFile);
	}
}
