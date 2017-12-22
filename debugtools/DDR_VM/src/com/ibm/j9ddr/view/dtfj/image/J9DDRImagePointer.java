/*******************************************************************************
 * Copyright (c) 2009, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.view.dtfj.image;

import java.util.Properties;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;

/**
 * @author andhall
 *
 */
public class J9DDRImagePointer implements ImagePointer
{
	private final IProcess proc;
	
	private final long address;
	
	public J9DDRImagePointer(IProcess proc, long address)
	{
		this.proc = proc;
		this.address = address;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#add(long)
	 */
	public ImagePointer add(long offset)
	{
		return new J9DDRImagePointer(proc, address + offset);
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#getAddress()
	 */
	public long getAddress()
	{
		return address;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#getAddressSpace()
	 */
	public ImageAddressSpace getAddressSpace()
	{
		return new J9DDRImageAddressSpace(proc.getAddressSpace());
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#getByteAt(long)
	 */
	public byte getByteAt(long index) throws MemoryAccessException,
			CorruptDataException
	{
		try {
			return proc.getByteAt(address + index);
		} catch (MemoryFault e) {
			MemoryAccessException toThrow = new MemoryAccessException(add(index));
			toThrow.initCause(e);
			throw toThrow;
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#getDoubleAt(long)
	 */
	public double getDoubleAt(long index) throws MemoryAccessException,
			CorruptDataException
	{
		try {
			long data = proc.getLongAt(address + index);
			return Double.longBitsToDouble(data);
		} catch (MemoryFault e) {
			MemoryAccessException toThrow = new MemoryAccessException(add(index));
			toThrow.initCause(e);
			throw toThrow;
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#getFloatAt(long)
	 */
	public float getFloatAt(long index) throws MemoryAccessException,
			CorruptDataException
	{
		try {
			int data = proc.getIntAt(address+ index);
			return Float.intBitsToFloat(data);
		} catch (MemoryFault e) {
			MemoryAccessException toThrow = new MemoryAccessException(add(index));
			toThrow.initCause(e);
			throw toThrow;
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#getIntAt(long)
	 */
	public int getIntAt(long index) throws MemoryAccessException,
			CorruptDataException
	{
		try {
			return proc.getIntAt(address + index);
		} catch (MemoryFault e) {
			MemoryAccessException toThrow = new MemoryAccessException(add(index));
			toThrow.initCause(e);
			throw toThrow;
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#getLongAt(long)
	 */
	public long getLongAt(long index) throws MemoryAccessException,
			CorruptDataException
	{
		try {
			return proc.getLongAt(address + index);
		} catch (MemoryFault e) {
			MemoryAccessException toThrow = new MemoryAccessException(add(index));
			toThrow.initCause(e);
			throw toThrow;
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#getPointerAt(long)
	 */
	public ImagePointer getPointerAt(long index) throws MemoryAccessException,
			CorruptDataException
	{
		try {
			long ptr = proc.getPointerAt(address + index);
			return new J9DDRImagePointer(proc, ptr);
		} catch (MemoryFault e) {
			MemoryAccessException toThrow = new MemoryAccessException(add(index));
			toThrow.initCause(e);
			throw toThrow;
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#getShortAt(long)
	 */
	public short getShortAt(long index) throws MemoryAccessException,
			CorruptDataException
	{
		try {
			return proc.getShortAt(address + index);
		} catch (MemoryFault e) {
			MemoryAccessException toThrow = new MemoryAccessException(add(index));
			toThrow.initCause(e);
			throw toThrow;
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#isExecutable()
	 */
	public boolean isExecutable() throws DataUnavailable
	{
		return proc.isExecutable(address);
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#isReadOnly()
	 */
	public boolean isReadOnly() throws DataUnavailable
	{
		return proc.isReadOnly(address);
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#isShared()
	 */
	public boolean isShared() throws DataUnavailable
	{
		return proc.isShared(address);
	}

	@Override
	public String toString() {
		return Long.toHexString(address);
	}

	@Override
	public boolean equals(Object obj)
	{
		if (obj instanceof J9DDRImagePointer) {
			J9DDRImagePointer other = (J9DDRImagePointer)obj;
			
			if (other.address != this.address) {
				return false;
			}
			
			if (! other.proc.getAddressSpace().equals(this.proc.getAddressSpace())) {
				return false;
			}
			
			return true;
		} else {
			return false;
		}
	}

	@Override
	public int hashCode()
	{
		return (int)(address >>> 3);
	}

	public Properties getProperties() {
		return proc.getProperties(address);
	}
	
}
