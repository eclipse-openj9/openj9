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

import com.ibm.dtfj.corereaders.DumpReader;
import com.ibm.dtfj.corereaders.MemoryAccessException;
import com.ibm.dtfj.corereaders.MemoryRange;

/**
 * @author jmdisher
 *
 */
public class DumpReaderAddressSpace extends CommonAddressSpace
{
	private DumpReader _reader;
	
	public DumpReaderAddressSpace(MemoryRange[] ranges, DumpReader reader, boolean isLittleEndian, boolean is64Bit)
	{
		super(ranges, isLittleEndian, is64Bit);
		_reader = reader;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.addressspace.IAbstractAddressSpace#isExecutable(int, long)
	 */
	public boolean isExecutable(int asid, long address)
			throws MemoryAccessException
	{
		MemoryRange match = _residentRange(asid, address);
		return match.isExecutable();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.addressspace.IAbstractAddressSpace#isReadOnly(int, long)
	 */
	public boolean isReadOnly(int asid, long address)
			throws MemoryAccessException
	{
		MemoryRange match = _residentRange(asid, address);
		return match.isReadOnly();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.addressspace.IAbstractAddressSpace#isShared(int, long)
	 */
	public boolean isShared(int asid, long address)
			throws MemoryAccessException
	{
		MemoryRange match = _residentRange(asid, address);
		return match.isShared();
	}
	
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.addressspace.IAbstractAddressSpace#getBytesAt(int, long, byte[])
	 */
	public int getBytesAt(int asid, long address, byte[] buffer) throws MemoryAccessException
	{
		int size, bytesRead=0;
		while (bytesRead < buffer.length) {
			long adjustedAddress = address + ((long)bytesRead & 0xFFFFFFFF);
			MemoryRange match = _residentRange(asid, adjustedAddress);
			long offset = adjustedAddress - match.getVirtualAddress();
			long bytesAvailable = match.getSize() - offset;

			// For 64-bit dumps, need to safely limit byte array size
			if (bytesAvailable > (long)(Integer.MAX_VALUE)) {
				size = buffer.length - bytesRead;
			} else {
				size = Math.min((int)bytesAvailable, buffer.length - bytesRead);
			}
			try {
				if (match.isInCoreFile() == true) {
					_reader.seek(match.getFileOffset() + offset);
					byte[] temp = _reader.readBytes(size);
					System.arraycopy(temp, 0, buffer, bytesRead, temp.length);
				} else {
					/*
					 * This memory range is not mapped into the core file, but is resident in
					 * an external library file, so read it from there.
					 */
					DumpReader reader = match.getLibraryReader();
					if (reader != null) {
						reader.seek(offset);
						byte[] temp = reader.readBytes(size);
						System.arraycopy(temp, 0, buffer, bytesRead, temp.length);
					} else {
						// No valid reader for this memory
						throw new MemoryAccessException(asid, adjustedAddress, "Cannot read memory external to core file" );
					}
				}
			} catch (IOException e) {
				throw new MemoryAccessException(asid, adjustedAddress, "IOException reading core file: " + e.getMessage());
			}
			bytesRead += size;
		}
		return bytesRead;
	}
	
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.addressspace.IAbstractAddressSpace#getLongAt(int, long)
	 */
	public long getLongAt(int asid, long address) throws MemoryAccessException
	{
		MemoryRange match = _residentRange(asid, address);
		
		if (null != match) {
			if (match.isInCoreFile()) {
				long fileOffset = match.getFileOffset() + (address - match.getVirtualAddress());
				try {
					_reader.seek(fileOffset);
					return _reader.readLong();
				} catch (IOException e) {
					throw new MemoryAccessException(asid, address);
				}
			} else {
				return super.getLongAt(asid, address);
			}
		} else {
			throw new MemoryAccessException(asid, address);
		}
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.addressspace.IAbstractAddressSpace#getIntAt(int, long)
	 */
	public int getIntAt(int asid, long address) throws MemoryAccessException
	{
		MemoryRange match = _residentRange(asid, address);
		
		if (null != match) {
			if (match.isInCoreFile()) {
				long fileOffset = match.getFileOffset() + (address - match.getVirtualAddress());
				try {
					_reader.seek(fileOffset);
					return _reader.readInt();
				} catch (IOException e) {
					throw new MemoryAccessException(asid, address);
				}
			} else {
				return super.getIntAt(asid, address);
			}
		} else {
			throw new MemoryAccessException(asid, address);
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.addressspace.IAbstractAddressSpace#getShortAt(int, long)
	 */
	public short getShortAt(int asid, long address)
			throws MemoryAccessException
	{
		MemoryRange match = _residentRange(asid, address);
		
		if (null != match) {
			if (match.isInCoreFile()) {
				long fileOffset = match.getFileOffset() + (address - match.getVirtualAddress());
				try {
					_reader.seek(fileOffset);
					return _reader.readShort();
				} catch (IOException e) {
					throw new MemoryAccessException(asid, address);
				}
			} else {
				return super.getShortAt(asid, address);
			}
		} else {
			throw new MemoryAccessException(asid, address);
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.addressspace.IAbstractAddressSpace#getByteAt(int, long)
	 */
	public byte getByteAt(int asid, long address) throws MemoryAccessException
	{
		
		MemoryRange match = _residentRange(asid, address);
		
		if (null != match) {
			if (match.isInCoreFile()) {
				long fileOffset = match.getFileOffset() + (address - match.getVirtualAddress());
				try {
					_reader.seek(fileOffset);
					return _reader.readByte();
				} catch (IOException e) {
					throw new MemoryAccessException(asid, address);
				}
			} else {
				return super.getByteAt(asid, address);
			}
		} else {
			throw new MemoryAccessException(asid, address);
		}
	}
}
