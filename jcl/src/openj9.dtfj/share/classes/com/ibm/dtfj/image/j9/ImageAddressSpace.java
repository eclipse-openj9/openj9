/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2020 IBM Corp. and others
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
package com.ibm.dtfj.image.j9;

import java.nio.ByteOrder;
import java.util.Iterator;
import java.util.Properties;
import java.util.Vector;

import com.ibm.dtfj.addressspace.IAbstractAddressSpace;
import com.ibm.dtfj.corereaders.MemoryRange;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.image.MemoryAccessException;

/**
 * @author jmdisher
 *
 */
public class ImageAddressSpace implements com.ibm.dtfj.image.ImageAddressSpace
{
	private Vector _processes = new Vector();
	private IAbstractAddressSpace _shadow;
	private int _asid = 0;	// This is used for z/OS multi-spatial support
	
	public ImageAddressSpace(IAbstractAddressSpace memory, int asid)
	{
		_shadow = memory;
		_asid = asid;
	}

	@Override
	public ByteOrder getByteOrder()
	{
		return _shadow.getByteOrder();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageAddressSpace#getCurrentProcess()
	 */
	public ImageProcess getCurrentProcess()
	{
		ImageProcess currentProc = null;
		
		if (_processes.size() > 0) {
			//TODO: sniff this from the core
			currentProc = (ImageProcess) _processes.elementAt(0);
		}
		return currentProc;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageAddressSpace#getProcesses()
	 */
	public Iterator getProcesses()
	{
		return _processes.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageAddressSpace#getPointer(long)
	 */
	public ImagePointer getPointer(long address)
	{
		return new com.ibm.dtfj.image.j9.ImagePointer(this, address);
	}

	public void addProcess(ImageProcess proc)
	{
		_processes.add(proc);
	}

	public byte readByteAtIndex(long address) throws MemoryAccessException
	{
		try {
			return _shadow.getByteAt(_asid, address);
		} catch (com.ibm.dtfj.corereaders.MemoryAccessException e) {
			throw new MemoryAccessException(getPointer(address));
		}
	}

	public ImagePointer readPointerAtIndex(long address) throws MemoryAccessException
	{
		try {
			long ptr = _shadow.getPointerAt(_asid, address);
			return getPointer(ptr);
		} catch (com.ibm.dtfj.corereaders.MemoryAccessException e) {
			throw new MemoryAccessException(getPointer(address));
		}
	}

	public int readIntAtIndex(long address) throws MemoryAccessException
	{
		try {
			return _shadow.getIntAt(_asid, address);
		} catch (com.ibm.dtfj.corereaders.MemoryAccessException e) {
			throw new MemoryAccessException(getPointer(address));
		}
	}

	public long readLongAtIndex(long address) throws MemoryAccessException
	{
		try {
			return _shadow.getLongAt(_asid, address);
		} catch (com.ibm.dtfj.corereaders.MemoryAccessException e) {
			throw new MemoryAccessException(getPointer(address));
		}
	}

	public short readShortAtIndex(long address) throws MemoryAccessException
	{
		try {
			return _shadow.getShortAt(_asid, address);
		} catch (com.ibm.dtfj.corereaders.MemoryAccessException e) {
			throw new MemoryAccessException(getPointer(address));
		}
	}

	public Iterator getImageSections()
	{
		Iterator rangeWalker = _shadow.getMemoryRanges();
		Vector sections = new Vector();
		
		while (rangeWalker.hasNext()) {
			MemoryRange range = (MemoryRange) rangeWalker.next();
			if (range.getAsid() == _asid) {
				long virtualAddress = range.getVirtualAddress();
				RawImageSection section = new RawImageSection(getPointer(virtualAddress), range.getSize());
				sections.add(section);
			}
		}
		return sections.iterator();
	}
	
	//these methods are provided so that pointers in the address space can look up their permission bits
	boolean isExecutableAt(long address) throws DataUnavailable
	{
		try {
			return _shadow.isExecutable(_asid, address);
		} catch (com.ibm.dtfj.corereaders.MemoryAccessException e) {
			throw new DataUnavailable();
		}
	}
	
	boolean isReadOnly(long address) throws DataUnavailable
	{
		try {
			return _shadow.isReadOnly(_asid, address);
		} catch (com.ibm.dtfj.corereaders.MemoryAccessException e) {
			throw new DataUnavailable();
		}
	}

	boolean isShared(long address) throws DataUnavailable
	{
		try {
			return _shadow.isShared(_asid, address);
		} catch (com.ibm.dtfj.corereaders.MemoryAccessException e) {
			throw new DataUnavailable();
		}
	}
	
	/**
	 * This shouldn't be added to the public API but it is required by the current hashCode algorithm in JavaObject.
	 * It may be possible to remove this method if that callsite is changed to a less hackish solution
	 * @return the number of bytes in a pointer (4 or 8).
	 */
	public int bytesPerPointer()
	{
		return _shadow.bytesPerPointer(_asid);
	}
	
	/**
	 * If available, show the ASID as toString.
	 * Change this once getID() is available.
	 */
	public String toString() {
		return _asid != 0 ? "0x"+Integer.toHexString(_asid) : super.toString();
	}

	public String getID() throws DataUnavailable, CorruptDataException {
		return "0x" + Long.toHexString(_asid);
	}

	//currently returns no OS specific properties
	public Properties getProperties() {
		return new Properties();
	}
}
