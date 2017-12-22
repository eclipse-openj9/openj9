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
package com.ibm.j9ddr.corereaders.memory;

import java.util.Properties;

/**
 * Base class for delegating memory ranges
 * @author andhall
 *
 */
class DelegatingMemorySource implements IMemorySource, IDetailedMemoryRange
{
	protected final IMemorySource delegate;

	public DelegatingMemorySource(IMemorySource range)
	{
		this.delegate = range;
	}

	public boolean contains(long address)
	{
		return delegate.contains(address);
	}

	public int getAddressSpaceId()
	{
		return delegate.getAddressSpaceId();
	}

	public long getBaseAddress()
	{
		return delegate.getBaseAddress();
	}

	public int getBytes(long address, byte[] buffer, int offset, int length)
			throws MemoryFault
	{
		return delegate.getBytes(address, buffer, offset, length);
	}

	public long getSize()
	{
		return delegate.getSize();
	}

	public long getTopAddress()
	{
		return delegate.getTopAddress();
	}

	public boolean isExecutable()
	{
		return delegate.isExecutable();
	}

	public boolean isReadOnly()
	{
		return delegate.isReadOnly();
	}

	public boolean isShared()
	{
		return delegate.isShared();
	}

	public boolean isSubRange(IMemoryRange other)
	{
		return delegate.isSubRange(other);
	}

	public boolean overlaps(IMemoryRange other)
	{
		return delegate.overlaps(other);
	}

	public int compareTo(IMemoryRange arg0)
	{
		return delegate.compareTo(arg0);
	}

	public String toString()
	{
		return this.getClass().getSimpleName() + ": " + delegate.toString();
	}

	@Override
	public boolean equals(Object o)
	{
		if (o instanceof DelegatingMemorySource) {
			DelegatingMemorySource other = (DelegatingMemorySource)o;
			
			if (! other.getClass().equals(this.getClass())) {
				return false;
			}
			
			return delegate.equals(other.delegate);
		} else {
			return false;
		}
	}

	@Override
	public int hashCode()
	{
		return delegate.hashCode();
	}

	public boolean isBacked()
	{
		return delegate.isBacked();
	}

	public String getName()
	{
		return delegate.getName();
	}

	public Properties getProperties() {
		if( delegate instanceof IDetailedMemoryRange ) {
			return ((IDetailedMemoryRange)delegate).getProperties();
		} else {
			return new Properties();
		}
	}
}
