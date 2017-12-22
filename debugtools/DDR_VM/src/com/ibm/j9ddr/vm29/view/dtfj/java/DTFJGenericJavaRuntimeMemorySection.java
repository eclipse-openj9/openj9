/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.view.dtfj.java;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.java.JavaRuntimeMemoryCategory;

/**
 * JavaRuntimeMemorySection used for everything but J9MemTag sections.
 * 
 * @see DTFJMemoryTagRuntimeMemorySection
 */
public class DTFJGenericJavaRuntimeMemorySection extends DTFJJavaRuntimeMemorySectionBase
{

	private final long baseAddress;
	
	private final long size;
	
	private final int allocationType;
	
	private final String allocator;
	
	private final JavaRuntimeMemoryCategory category;
	
	private final String name;
	
	public DTFJGenericJavaRuntimeMemorySection(long baseAddress, long size, int allocationType, String name, String allocator, JavaRuntimeMemoryCategory category)
	{
		this.baseAddress = baseAddress;
		this.size = size;
		this.allocationType = allocationType;
		this.name = name;
		this.allocator = allocator;
		this.category = category;
	}
	
	DTFJGenericJavaRuntimeMemorySection(long baseAddress, long size, int allocationType, String name)
	{
		this(baseAddress,size,allocationType,name,null,null);
	}
	
	@Override
	protected long getBaseAddressAsLong()
	{
		return baseAddress;
	}

	public int getAllocationType()
	{
		return allocationType;
	}

	public String getAllocator() throws CorruptDataException, DataUnavailable
	{
		if (allocator != null) {
			return allocator;
		} else {
			throw new DataUnavailable();
		}
	}

	public JavaRuntimeMemoryCategory getMemoryCategory()
			throws CorruptDataException, DataUnavailable
	{
		if (category != null) {
			return category;
		} else {
			throw new DataUnavailable();
		}
	}

	public String getName()
	{
		return name;
	}

	public long getSize()
	{
		return size;
	}

	@Override
	public int hashCode()
	{
		final int prime = 31;
		int result = 1;
		result = prime * result + allocationType;
		result = prime * result
				+ ((allocator == null) ? 0 : allocator.hashCode());
		result = prime * result + (int) (baseAddress ^ (baseAddress >>> 32));
		result = prime * result
				+ ((category == null) ? 0 : category.hashCode());
		result = prime * result + ((name == null) ? 0 : name.hashCode());
		result = prime * result + (int) (size ^ (size >>> 32));
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
		if (!(obj instanceof DTFJGenericJavaRuntimeMemorySection)) {
			return false;
		}
		DTFJGenericJavaRuntimeMemorySection other = (DTFJGenericJavaRuntimeMemorySection) obj;
		if (allocationType != other.allocationType) {
			return false;
		}
		if (allocator == null) {
			if (other.allocator != null) {
				return false;
			}
		} else if (!allocator.equals(other.allocator)) {
			return false;
		}
		if (baseAddress != other.baseAddress) {
			return false;
		}
		if (category == null) {
			if (other.category != null) {
				return false;
			}
		} else if (!category.equals(other.category)) {
			return false;
		}
		if (name == null) {
			if (other.name != null) {
				return false;
			}
		} else if (!name.equals(other.name)) {
			return false;
		}
		if (size != other.size) {
			return false;
		}
		return true;
	}

}
