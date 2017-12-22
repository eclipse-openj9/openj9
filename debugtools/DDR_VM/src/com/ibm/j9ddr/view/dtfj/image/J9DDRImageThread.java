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

import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.List;
import java.util.Properties;

import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageRegister;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.image.ImageStackFrame;
import com.ibm.dtfj.image.ImageThread;
import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.memory.IMemoryRange;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.corereaders.osthread.IOSStackFrame;
import com.ibm.j9ddr.corereaders.osthread.IOSThread;
import com.ibm.j9ddr.corereaders.osthread.IRegister;

/**
 * Adapter for IOSThread objects to make them implement ImageThread
 * 
 * @author andhall
 *
 */
public class J9DDRImageThread extends J9DDRBaseImageThread implements ImageThread
{
	private final IOSThread thread;
	private long overriddenThreadId;
	private boolean threadIdOverridden;
	
	public J9DDRImageThread(IProcess process, IOSThread thread) 
	{
		super(process);
		this.thread = thread;
		threadIdOverridden = false;
	}
	
	public J9DDRImageThread(IProcess process, IOSThread thread, long threadId)
	{
		this(process,thread);
		threadIdOverridden = true;
		overriddenThreadId = threadId;
	}

	public long getThreadId() throws com.ibm.j9ddr.CorruptDataException
	{
		if (threadIdOverridden) {
			return overriddenThreadId;
		} else {
			return thread.getThreadId();
		}
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageThread#getProperties()
	 */
	public Properties getProperties()
	{
		return thread.getProperties();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageThread#getRegisters()
	 */
	public Iterator<?> getRegisters()
	{
		Collection<? extends IRegister> registers = thread.getRegisters();
		
		List<ImageRegister> dtfjRegisters = new ArrayList<ImageRegister>(registers.size());
		
		for (IRegister thisRegister : registers) {
			dtfjRegisters.add(new J9DDRImageRegister(thisRegister));
		}
		
		return dtfjRegisters.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageThread#getStackFrames()
	 */
	public Iterator<?> getStackFrames() throws DataUnavailable
	{
		List<? extends IOSStackFrame> frames = thread.getStackFrames();
		
		List<ImageStackFrame> dtfjFrames = new ArrayList<ImageStackFrame>(frames.size());
		
		for (IOSStackFrame thisFrame : frames) {
			dtfjFrames.add(new J9DDRImageStackFrame(process, thisFrame, this));
		}
		
		return dtfjFrames.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageThread#getStackSections()
	 */
	public Iterator<?> getStackSections()
	{
		Collection<? extends IMemoryRange> ranges = thread.getMemoryRanges();
		
		List<ImageSection> sections = new ArrayList<ImageSection>(ranges.size());
		
		for (IMemoryRange range : ranges) {
			sections.add(new J9DDRImageSection(process, range.getBaseAddress(), range.getSize(), "stack section at " + Long.toHexString(range.getBaseAddress())));
		}
		
		return sections.iterator();
	}

	@Override
	public int hashCode()
	{
		final int prime = 31;
		int result = 1;
		result = prime * result + ((thread == null) ? 0 : thread.hashCode());
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
		if (!(obj instanceof J9DDRImageThread)) {
			return false;
		}
		J9DDRImageThread other = (J9DDRImageThread) obj;
		if (thread == null) {
			if (other.thread != null) {
				return false;
			}
		} else if (!thread.equals(other.thread)) {
			return false;
		}
		return true;
	}

	@Override
	public String toString() {
		StringBuilder data = new StringBuilder();
		data.append("ImageThread 0x");
		try {
			data.append(Long.toHexString(thread.getThreadId()));
		} catch (CorruptDataException e) {
			data.append("[corrupt ID]");
		}
		return data.toString();
	}
	
	

}
