/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2019 IBM Corp. and others
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

import java.util.Properties;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.j9.ImageAddressSpace;
import com.ibm.dtfj.image.MemoryAccessException;

/**
 * @author jmdisher
 * This will have to be revisited to support z/OS
 */
public class ImagePointer implements com.ibm.dtfj.image.ImagePointer
{
	private long _underlyingAddress;
	private ImageAddressSpace _residentDomain;
	
	
	public ImagePointer(ImageAddressSpace resident, long localAddress)
	{
		//note that we can't possibly be in a non-existent address space
		if (null == resident) {
			throw new IllegalArgumentException("Pointer cannot exist in null address space");
		}
		_residentDomain = resident;
		_underlyingAddress = localAddress;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#getAddress()
	 */
	public long getAddress()
	{
		return _underlyingAddress;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#getAddressSpace()
	 */
	public com.ibm.dtfj.image.ImageAddressSpace getAddressSpace()
	{
		return _residentDomain;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#add(long)
	 */
	public com.ibm.dtfj.image.ImagePointer add(long offset)
	{
		return _residentDomain.getPointer(getAddress() + offset);
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#isExecutable()
	 */
	public boolean isExecutable() throws DataUnavailable
	{
		return _residentDomain.isExecutableAt(_underlyingAddress);
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#isReadOnly()
	 */
	public boolean isReadOnly() throws DataUnavailable
	{
		return _residentDomain.isReadOnly(_underlyingAddress);
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#isShared()
	 */
	public boolean isShared() throws DataUnavailable
	{
		return _residentDomain.isShared(_underlyingAddress);
	}

	public Properties getProperties() {
		return new Properties();
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#getPointerAt(long)
	 */
	public com.ibm.dtfj.image.ImagePointer getPointerAt(long index)
			throws MemoryAccessException, CorruptDataException
	{
		return _residentDomain.readPointerAtIndex(getAddress() + index);
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#getLongAt(long)
	 */
	public long getLongAt(long index) throws MemoryAccessException,
			CorruptDataException
	{
		return _residentDomain.readLongAtIndex(getAddress() + index);
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#getIntAt(long)
	 */
	public int getIntAt(long index) throws MemoryAccessException,
			CorruptDataException
	{
		return _residentDomain.readIntAtIndex(getAddress() + index);
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#getShortAt(long)
	 */
	public short getShortAt(long index) throws MemoryAccessException,
			CorruptDataException
	{
		return _residentDomain.readShortAtIndex(getAddress() + index);
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#getByteAt(long)
	 */
	public byte getByteAt(long index) throws MemoryAccessException,
			CorruptDataException
	{
		return _residentDomain.readByteAtIndex(getAddress() + index);
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#getFloatAt(long)
	 */
	public float getFloatAt(long index) throws MemoryAccessException,
			CorruptDataException
	{
		return Float.intBitsToFloat(getIntAt(index));
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImagePointer#getDoubleAt(long)
	 */
	public double getDoubleAt(long index) throws MemoryAccessException,
			CorruptDataException
	{
		return Double.longBitsToDouble(getLongAt(index));
	}
	
	public boolean equals(Object obj)
	{
		boolean isEqual = false;
		
		if (obj instanceof ImagePointer) {
			ImagePointer local = (ImagePointer) obj;
			isEqual = (_residentDomain.equals(local._residentDomain) && (_underlyingAddress == local._underlyingAddress));
		}
		return isEqual;
	}
	
	public int hashCode()
	{
		return ((_residentDomain.hashCode()) ^ (((int)_underlyingAddress) ^ ((int)(_underlyingAddress >> 32))));
	}
	
	public String toString()
	{
		return Long.toHexString(_underlyingAddress);
	}
}
