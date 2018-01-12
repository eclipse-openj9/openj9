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
package com.ibm.j9ddr.corereaders.memory;

import java.util.Properties;

/**
 * Memory source for storage range that is declared, but not backed.
 * 
 * Any attempt to read storage results in a MemoryFault
 * 
 * @author andhall
 * 
 */
public class UnbackedMemorySource extends ProtectedMemoryRange implements IMemorySource, IDetailedMemoryRange
{

	private final int addressSpaceId;

	private final String explanation;
	
	private final String name;
	
	private Properties props;
	
	/**
	 * 
	 * @param base
	 *            Base address for this range
	 * @param size
	 *            Size of this range
	 * @param explanation
	 *            String message explaining why this section isn't backed with
	 *            storage
	 * @param asid
	 *            Address space id
	 */
	public UnbackedMemorySource(long base, long size, String explanation,
			int asid, String name)
	{
		super(base,size);
		this.explanation = explanation;
		this.addressSpaceId = asid;
		this.name = name;
	}

	/**
	 * 
	 * @param base
	 *            Base address for this range
	 * @param size
	 *            Size of this range
	 * @param explanation
	 *            String message explaining why this section isn't backed with
	 *            storage
	 */
	public UnbackedMemorySource(long base, long size, String explanation)
	{
		this(base, size, explanation, 0, null);
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see com.ibm.j9ddr.corereaders.memory.IMemoryRange#getAddressSpaceId()
	 */
	public int getAddressSpaceId()
	{
		return addressSpaceId;
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see com.ibm.j9ddr.corereaders.memory.IMemoryRange#getBytes(long, byte[],
	 * int, int)
	 */
	public int getBytes(long address, byte[] buffer, int offset, int length)
			throws MemoryFault
	{
		throw new MemoryFault(address, explanation + "; range: "
				+ Long.toHexString(address) + " size: "
				+ Long.toHexString(getSize()));
	}

	public String getName()
	{
		return name;
	}

	@Override
	public boolean isBacked()
	{
		return false;
	}

	public Properties getProperties() {
		if( props == null ) {
			props = new Properties();
		}
		return props;
	}

	public boolean isExecutable() {
		if( props == null ) {
			return false;
		}
		return Boolean.TRUE.toString().equals(props.get(IDetailedMemoryRange.EXECUTABLE));
	}

	/* Assuming that just because something is executable doesn't stop it being read only.
	 * (ReadOnly is not actually a very good property to expose since it relies on the lack
	 * of other properties.)
	 */
	public boolean isReadOnly() {
		if( props == null ) {
			return false;
		}
		return Boolean.TRUE.toString().equals(props.get(IDetailedMemoryRange.READABLE)) &&
				!(Boolean.TRUE.toString().equals(props.get(IDetailedMemoryRange.WRITABLE)) );
	}
	
}
