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
package com.ibm.dtfj.corereaders;

/**
 * Dumps contain lots of memory - segmented across a range of virtual 
 * addresses....... 
 */
public class MemoryRange implements Comparable {
	private static final String printableEBCDIC[] = {
		"40"," ",
		"F0","0",	"F1","1",	"F2","2",	"F3","3",	"F4","4",	"F5","5",
		"F6","6",	"F7","7",	"F8","8",	"F9","9",	 	
		"C1","A",	"C2","B",	"C3","C",	"C4","D",	"C5","E",
		"C6","F",	"C7","G",	"C8","H",	"C9","I",	"D1","J",	"D2","K",
		"D3","L",	"D4","M",	"D5","N",	"D6","O",	"D7","P",	"D8","Q",
		"D9","R",	"E2","S",	"E3","T",	"E4","U",	"E5","V",	"E6","W",
		"E7","X",	"E8","Y",	"E9","Z",
		"81","a",	"82","b",	"83","c",	"84","d",	"85","e",
		"86","f",	"87","g",	"88","h",	"89","i",	"91","j",	"92","k",
		"93","l",	"94","m",	"95","n",	"96","o",	"97","p",	"98","q",
		"99","r",	"A2","s",	"A3","t",	"A4","u",	"A5","v",	"A6","w",
		"A7","x",	"A8","y",	"A9","z"
	};
	
	private DumpReader _libraryReader = null;
	private long _virtualAddress;
	private long _fileOffset;
	private long _size;
	private boolean _inCoreFile = true;
	private int _asid = 0;
	private boolean _shared = false;
	private boolean _readOnly = false;
	private boolean _executable = true;
	private boolean _permissionsSupported = false;	//this assumes that all permissions are either supported, or not.  This may not be valid

	public MemoryRange(long virtualAddress, long fileOffset, long size)
	{
		_virtualAddress = virtualAddress;
		_fileOffset = fileOffset;
		_size = size;
	}

	public MemoryRange(long virtualAddress, long fileOffset, long size, int asid)
	{
		this(virtualAddress, fileOffset, size);
		_asid = asid;
	}

	public MemoryRange(long virtualAddress, long fileOffset, long size, int asid, boolean isShared, boolean isReadOnly, boolean isExecutable)
	{
		this(virtualAddress, fileOffset, size, asid);
		_shared = isShared;
		_readOnly = isReadOnly;
		_executable = isExecutable;
		_permissionsSupported = true;
	}

	public MemoryRange(long virtualAddress, long fileOffset, long size, int asid, boolean isShared, boolean isReadOnly, boolean isExecutable, boolean inCoreFile)
	{
		this(virtualAddress, fileOffset, size, asid, isShared, isReadOnly, isExecutable);
		_inCoreFile = inCoreFile;
	}
	
	/**
	 * Copy constructor, used when copying shared memory ranges into another address space
	 * 
	 * @return the new copy of the MemoryRange
	 */
	public MemoryRange(MemoryRange range, int asid)
	{
		this(range.getVirtualAddress(), range.getFileOffset(), range.getSize(), asid);
		
		_permissionsSupported = true;
		try {
			_shared = range.isShared();
			_readOnly = range.isReadOnly();
			_executable = range.isExecutable();
			_inCoreFile = range.isInCoreFile();
			_libraryReader = range.getLibraryReader();
		} catch (MemoryAccessException exc) {
			_permissionsSupported = false;
		}
	}

	public boolean contains(long address)
	{
		return getVirtualAddress() <= address && address < getVirtualAddress() + getSize();
	}

	
	public boolean contains(int asid, long address)
	{
		return (asid == _asid) && contains(address);
	}
	
	/**
	 * @return the file offset
	 */
	public long getFileOffset()
	{
		return _fileOffset;
	}

	/**
	 * @return the number of bytes in this MemoryRange
	 */
	public long getSize()
	{
		return _size;
	}
	
	/**
	 * @return the base virtual address
	 */
	public long getVirtualAddress()
	{
		return _virtualAddress;
	}

	/**
	 * @return true if this MemoryRange is located in the core file
	 */
	public boolean isInCoreFile()
	{
		return _inCoreFile;
	}
	
	/**
	 * @return the DumpReader for this MemoryRange
	 */
	public DumpReader getLibraryReader()
	{
		return _libraryReader;
	}
	
	/**
	 * Set the DumpReader for this MemoryRange
	 * @return void
	 */
	public void setLibraryReader(DumpReader libraryReader)
	{
		_libraryReader = libraryReader;
	}

	public String toString()
	{
		StringBuffer sb = new StringBuffer();

		sb.append("Addr: 0x" + Long.toHexString(getVirtualAddress()));
		sb.append("   Size: 0x" + Long.toHexString(getSize()) + " (" + getSize() + ")");
		sb.append("   File Offset: 0x" + Long.toHexString(getFileOffset()));
		sb.append(" (" + getFileOffset() + ")");

		if (_asid != 0) {
			String tempASID = Integer.toHexString(_asid);
			// OK we look to see it the complete ASID is in valid ebcdic 
			// printable chars ..... if so convert to string so we can see the
			// asid "name" such as SYS1 CICS etc.....
			boolean bIsReadable = false;
			StringBuffer sBuff = new StringBuffer("");
			if (8 == tempASID.length()) {
				bIsReadable = true;
				for (int i = 0; i < 8 && bIsReadable; i = i + 2) {

					String x = tempASID.substring(i, i + 2);
					String y = isPrintableEbcdic(x);
					if (null == y) {
						bIsReadable = false;
					} else {
						sBuff.append(y);
					}

				}
			}
			if (true == bIsReadable) {
				sb.append("   asid: " + sBuff);
			} else {
				sb.append("   asid: 0x" + Integer.toHexString(_asid));
			}
		}

		return sb.toString();
	}

	// very simplistic and inefficient way of figuring out an ebcdic 
	// printable char from hex characters
	private String isPrintableEbcdic(String in)
	{
		in = in.toUpperCase();
		for (int i = 0; i < printableEBCDIC.length; i = i + 2) {
			if (in.equals(printableEBCDIC[i])) {
				return printableEBCDIC[i + 1];
			}

		}

		return null;
	}

	/**
	 * @return
	 */
	public int getAsid()
	{
		return _asid;
	}

	/**
	 * @return true if this MemoryRange is marked executable
	 * @throws MemoryAccessException 
	 */
	public boolean isExecutable() throws MemoryAccessException
	{
		if (_permissionsSupported) {
			return _executable;
		} else {
			throw new MemoryAccessException(getAsid(), getVirtualAddress());
		}
	}

	/**
	 * @return true if this MemoryRange is marked read-only
	 * @throws MemoryAccessException 
	 */
	public boolean isReadOnly() throws MemoryAccessException
	{
		if (_permissionsSupported) {
			return _readOnly;
		} else {
			throw new MemoryAccessException(getAsid(), getVirtualAddress());
		}
	}

	/**
	 * @return true if this MemoryRange is marked shared
	 * @throws MemoryAccessException 
	 */
	public boolean isShared() throws MemoryAccessException
	{
		if (_permissionsSupported) {
			return _shared;
		} else {
			throw new MemoryAccessException(getAsid(), getVirtualAddress());
		}
	}

	/**
	 * @return Virtual address comparator.
	 */
	public int compareTo(Object o)
	{
		MemoryRange rhs = (MemoryRange) o;
		// Take account of address spaces
		if (_asid < rhs._asid) return -1;
		if (_asid > rhs._asid) return 1;
		
		// Then compare addresses as unsigned quantities
		if (_virtualAddress == rhs._virtualAddress) {
			return 0;
		} else if ((_virtualAddress >= 0 && rhs._virtualAddress >= 0)
				|| (_virtualAddress < 0 && rhs._virtualAddress < 0)) {
			return _virtualAddress < rhs._virtualAddress ? -1 : 1;
		} else {
			return _virtualAddress < rhs._virtualAddress ? 1 : -1;
		}
	}
}
