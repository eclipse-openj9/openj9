/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.j9ddr.corereaders.memory.IProcess;

public class J9DDRImageSection implements ImageSection {
	private final IProcess proc;
	private final long address;
	private J9DDRImagePointer baseAddress = null;
	private long size = 0;
	private final String name;
	
	public J9DDRImageSection(IProcess proc, long address, String name)
	{
		this.proc = proc;
		this.address = address;
		if( name != null ) {
			this.name = name;
		} else {
			this.name = "Image section @ " + Long.toHexString(getBaseAddress().getAddress());
		}
	}
	
	public J9DDRImageSection(IProcess proc, long address, long size, String name)
	{
		this.proc = proc;
		this.address = address;
		this.size = size;
		if( name != null ) {
			this.name = name;
		} else {
			this.name = "Image section @ " + Long.toHexString(address) + " (" + getSize() + " bytes)";
		}
	}

	public J9DDRImagePointer getBaseAddress() {
		if(baseAddress == null) {
			baseAddress = new J9DDRImagePointer(proc, address);
		}
		return baseAddress; 
	}
/*
 * DTFJ API - visible to clients
 */
	public String getName() {
		return name;
	}

	public long getSize() {
		return size;
	}

	public boolean isExecutable() throws DataUnavailable {
		return getBaseAddress().isExecutable();
	}

	public boolean isReadOnly() throws DataUnavailable {
		return getBaseAddress().isReadOnly();
	}

	public boolean isShared() throws DataUnavailable {
		return getBaseAddress().isShared();
	}
	
	public Properties getProperties() {
		return getBaseAddress().getProperties();
	}

/*
 * J9DDR API - not visible to clients
 */

	public void setSize(long size) {
		this.size = size;
	}

	@Override
	public String toString() {
		StringBuilder data = new StringBuilder();
		data.append("ImageSection [0x");
		data.append(Long.toHexString(address));
		data.append(" - 0x");
		data.append(Long.toHexString(address + size));
		data.append("]");
		return data.toString();
	}

	
	
}
