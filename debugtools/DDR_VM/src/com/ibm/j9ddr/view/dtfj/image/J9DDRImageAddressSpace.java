/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
import java.util.LinkedList;
import java.util.List;
import java.util.Properties;

import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.j9ddr.DataUnavailableException;
import com.ibm.j9ddr.corereaders.memory.IAddressSpace;
import com.ibm.j9ddr.corereaders.memory.IMemoryRange;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils;

public class J9DDRImageAddressSpace implements ImageAddressSpace {

	private final IAddressSpace as;
	
	public J9DDRImageAddressSpace(IAddressSpace as)
	{
		this.as = as;
	}
	
	public IAddressSpace getIAddressSpace() {
		return as;
	}
	
	public J9DDRImageProcess getCurrentProcess() {
		//TODO current thread/process support
		Iterator<J9DDRImageProcess> processesIt = getProcesses();

		J9DDRImageProcess first = null;
		
		while (processesIt.hasNext()) {
			J9DDRImageProcess p = processesIt.next();
			try {
				if (p.isFailingProcess()) {
					return p;
				}
			} catch (DataUnavailableException e) {
				// Carry on
			}
			if (first == null) {
				first = p;
			}
		}
		return first;
	}

	public Iterator<?> getImageSections() {
		Collection<? extends IMemoryRange> ranges = as.getMemoryRanges();
		
		List<ImageSection> sections = new ArrayList<ImageSection>(ranges.size());
		
		IProcess proc = getPointerProcess();
		
		//it may be the case that we can't determine the pointer size if there are no processes found in the address space
		if(null == proc) {
			return J9DDRDTFJUtils.emptyIterator();
		}
		
		for (IMemoryRange thisRange : ranges) {
			J9DDRImageSection section = new J9DDRImageSection(proc,thisRange.getBaseAddress(),thisRange.getSize(),thisRange.getName()); 
			sections.add(section);
		}
		
		return sections.iterator();
	}

	public ImagePointer getPointer(long address) {
		return new J9DDRImagePointer(getPointerProcess(), address);
	}

	private IProcess getPointerProcess()
	{
		// The implementation of J9DDRImagePointer is tied to an IProcess. On Windows, Linux and AIX there is only one process
		// in an address space, but on z/OS an address space can contain more than one process, and even a mix of 31 bit and 64
		// bit processes. In practice it does not matter which process we use when constructing and using an J9DDRImagePointer,
		// the compromise here is to take the first 64-bit process we find, else any process, else null.
		Iterator<? extends IProcess> procs = as.getProcesses().iterator();
		IProcess process = null;
		
		while (procs.hasNext()) {
			process = procs.next();
			if (process.bytesPerPointer() == 8) {
				// Found a 64-bit process, return it
				return process;
			}
		}
		// Return any 31/32 bit process we found, or null if no process in this address space
		return process;
	}

	public Iterator<J9DDRImageProcess> getProcesses() {
		Collection<? extends IProcess> processes = as.getProcesses();
		
		List<J9DDRImageProcess> dtfjList = new LinkedList<J9DDRImageProcess>();
		
		for (IProcess thisProcess : processes) {
			dtfjList.add(new J9DDRImageProcess(thisProcess));
		}
		
		return dtfjList.iterator();
	}
	
	public String getID() {
		return "0x" + Long.toHexString(as.getAddressSpaceId());
	}

	@Override
	public boolean equals(Object obj) {
		if((obj == null) || !(obj instanceof J9DDRImageAddressSpace)) {
			return false;
		}
		J9DDRImageAddressSpace space = (J9DDRImageAddressSpace) obj;
		return as.equals(space.as);		//compare underlying address spaces for equality
	}

	@Override
	public int hashCode() {
		return as.getAddressSpaceId();
	}

	@Override
	public String toString()
	{
		return "AddressSpace id 0x" + Integer.toHexString(as.getAddressSpaceId());
	}
	
	//currently returns no OS specific properties
	public Properties getProperties() {
		return new Properties();
	}
		
}
