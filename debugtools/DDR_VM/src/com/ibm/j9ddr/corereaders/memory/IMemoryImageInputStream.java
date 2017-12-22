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

import java.io.IOException;

import javax.imageio.stream.ImageInputStreamImpl;

/**
 * InputStream that takes its data from an IMemory instance.
 * 
 * Will read until it hits unreadable memory - then it will return EOF.
 * 
 * @author andhall
 *
 */
public class IMemoryImageInputStream extends ImageInputStreamImpl
{	
	private final IMemory memory;
	
	private long address;
	
	public IMemoryImageInputStream(IMemory memory, long startingAddress)
	{
		this.memory = memory;
		this.address = startingAddress;
		
		this.setByteOrder(memory.getByteOrder());
	}

	@Override
	public int read(byte[] buffer, int offset, int length) throws IOException
	{
		int read = 0;
		
		try {
			read = memory.getBytesAt(address, buffer,offset,length);
		} catch (MemoryFault f) {
			if (f.getAddress() == address) {
				/* Couldn't read anything - EOF */
				return -1;
			} else {
				read = (int)(f.getAddress() - address);
			}
		}
		
		address += read;
		
		return read;
	}

	@Override
	public int read() throws IOException
	{
		int toReturn = 0;
		try {
			toReturn = (0x000000FF & memory.getByteAt(address));
		} catch (MemoryFault e) {
			return -1;
		}
		
		address++;
		
		return toReturn;
	}

	@Override
	public long getStreamPosition() throws IOException
	{
		return address;
	}

	@Override
	public void seek(long pos) throws IOException
	{
		address = pos;
	}
	

}
