/*******************************************************************************
 * Copyright (c) 2010, 2014 IBM Corp. and others
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
 * Simple memory range class.
 * 
 * @author andhall
 *
 */
public class MemoryRange extends BaseMemoryRange implements IMemoryRange
{
	private final IAddressSpace addressSpace;
	private final String name;
	
	public MemoryRange(IAddressSpace addressSpace, long baseAddress, long size, String name)
	{
		super(baseAddress, size);
		this.addressSpace = addressSpace;
		this.name = name;
	}
	
	/**
	 * Constructor to build a memory range with the same base and size as an existing memory range,
	 * but with a different name
	 */
	public MemoryRange(IAddressSpace addressSpace, IMemoryRange range, String name)
	{
		this(addressSpace, range.getBaseAddress(), range.getSize(), name);
	}

	public int getAddressSpaceId()
	{
		return addressSpace.getAddressSpaceId();
	}

	public String getName()
	{
		return name;
	}

	public boolean isExecutable()
	{
		return addressSpace.isExecutable(this.getBaseAddress());
	}

	public boolean isReadOnly()
	{
		return addressSpace.isReadOnly(this.getBaseAddress());
	}

	public boolean isShared()
	{
		return addressSpace.isShared(this.getBaseAddress());
	}

	@Override
	public int hashCode()
	{
		final int prime = 31;
		int result = 1;
		result = prime * result
				+ ((addressSpace == null) ? 0 : addressSpace.hashCode());
		result = prime * result + ((name == null) ? 0 : name.hashCode());
		return result;
	}

	@Override
	public boolean equals(Object obj)
	{
		if (this == obj) {
			return true;
		}
		if (obj == null) {
			return false;
		}
		if (!(obj instanceof MemoryRange)) {
			return false;
		}
		MemoryRange other = (MemoryRange) obj;
		if (addressSpace == null) {
			if (other.addressSpace != null) {
				return false;
			}
		} else if (!addressSpace.equals(other.addressSpace)) {
			return false;
		}
		if (name == null) {
			if (other.name != null) {
				return false;
			}
		} else if (!name.equals(other.name)) {
			return false;
		}
		return true;
	}

}
