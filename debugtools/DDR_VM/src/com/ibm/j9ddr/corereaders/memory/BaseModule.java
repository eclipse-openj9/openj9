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

import java.util.Collection;
import java.util.Collections;

public abstract class BaseModule implements IModule
{

	protected final String name;
	protected final Collection<? extends IMemoryRange> memoryRanges;
	protected final IProcess process;
	protected final long loadAddress;

	public BaseModule(IProcess process, String name, Collection<? extends IMemoryRange> memoryRanges, long loadAddress)
	{
		this.name = name;
		this.memoryRanges = Collections.unmodifiableCollection(memoryRanges);
		this.process = process;
		this.loadAddress = loadAddress;
	}

	public Collection<? extends IMemoryRange> getMemoryRanges()
	{
		return memoryRanges;
	}

	public String getName()
	{
		return name;
	}


	@Override
	public int hashCode()
	{
		final int prime = 31;
		int result = 1;
		result = prime * result
				+ ((memoryRanges == null) ? 0 : memoryRanges.hashCode());
		result = prime * result + ((name == null) ? 0 : name.hashCode());
		result = prime * result + ((process == null) ? 0 : process.hashCode());
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
		if (!(obj instanceof BaseModule)) {
			return false;
		}
		BaseModule other = (BaseModule) obj;
		if (memoryRanges == null) {
			if (other.memoryRanges != null) {
				return false;
			}
		} else if (!memoryRanges.equals(other.memoryRanges)) {
			return false;
		}
		if (name == null) {
			if (other.name != null) {
				return false;
			}
		} else if (!name.equals(other.name)) {
			return false;
		}
		if (process == null) {
			if (other.process != null) {
				return false;
			}
		} else if (!process.equals(other.process)) {
			return false;
		}
		return true;
	}

	public long getLoadAddress() {
		return loadAddress;
	}

	
	
}
