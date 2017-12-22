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

/**
 * Abstract base class containing common logic for IMemoryRange
 * 
 * @author andhall
 * 
 */
public abstract class BaseMemoryRange implements IMemoryRange
{	
	private final boolean fastPathContains;
	private int alignment;
	private long baseAddressTestValue;
	
	protected final long baseAddress;
	protected final long size;
	
	protected BaseMemoryRange(long baseAddress,long size)
	{
		//For ranges that are as wide as they are aligned, we can fast-path the contains() operation with an and().
		this.baseAddress = baseAddress;
		this.size = size;
		
		alignment = 0;
		
		long workingAddress = baseAddress;
		for (int i=0; i < 64; i++) {
			if ((workingAddress & 1) == 0) {
				alignment++;
			} else {
				break;
			}
			
			workingAddress >>>= 1;
		}
		
		baseAddressTestValue = baseAddress >>> alignment;
		
		long topAddress = getTopAddress();
		
		fastPathContains = (baseAddressTestValue == (topAddress >>> alignment) && baseAddressTestValue != ((topAddress + 1) >>> alignment));
	}
	
	/*
	 * (non-Javadoc)
	 * 
	 * @see com.ibm.dtfj.j9ddr.corereaders.memory.IMemoryRange#contains(long)
	 */
	public boolean contains(long address)
	{
		if (fastPathContains) {
			return baseAddressTestValue == address >>> alignment;
		} else {
			return Addresses.greaterThanOrEqual(address, this.getBaseAddress())
				&& Addresses.lessThanOrEqual(address, this.getTopAddress());
		}
	}

	public int compareTo(IMemoryRange o)
	{

		if (o.getAddressSpaceId() != this.getAddressSpaceId()) {
			return this.getAddressSpaceId() - o.getAddressSpaceId();
		}

		if (Addresses.greaterThan(this.getBaseAddress(), o.getBaseAddress())) {
			return 1;
		} else if (Addresses
				.lessThan(this.getBaseAddress(), o.getBaseAddress())) {
			return -1;
		} else {
			if (Addresses.greaterThan(this.getTopAddress(), o.getTopAddress())) {
				return 1;
			} else if (Addresses.lessThan(this.getTopAddress(), o.getTopAddress())) {
				return -1;
			} else {
				return 0;
			}
		}
	}
	
	@Override
	public String toString()
	{
		StringBuffer buffer = new StringBuffer();
		
		buffer.append("MemoryRange ");
		buffer.append(this.getClass().getName());
		buffer.append(" from ");
		buffer.append(Long.toHexString(this.getBaseAddress()));
		buffer.append(" to ");
		buffer.append(Long.toHexString(this.getTopAddress()));

		return buffer.toString();
	}
	
	public boolean isSubRange(IMemoryRange other)
	{
		boolean baseAddressInRange = Addresses.greaterThanOrEqual(other.getBaseAddress(), this.getBaseAddress());
		boolean topAddressInRange = Addresses.greaterThanOrEqual(this.getTopAddress(), other.getTopAddress());
		return baseAddressInRange && topAddressInRange;
	}

	public boolean overlaps(IMemoryRange other)
	{
		return this.contains(other.getBaseAddress())
		|| this.contains(other.getTopAddress())
		|| other.contains(this.getBaseAddress())
		|| other.contains(this.getTopAddress());
	}

	public final long getBaseAddress()
	{
		return baseAddress;
	}

	public final long getSize()
	{
		return size;
	}

	public final long getTopAddress()
	{
		if( size == 0 ) {
			return baseAddress;
		}
		return baseAddress + size - 1;
	}

	public boolean isBacked()
	{
		return true;
	}
	
}
