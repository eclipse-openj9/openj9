/*******************************************************************************
 * Copyright (c) 2010, 2018 IBM Corp. and others
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
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.java.JavaRuntimeMemoryCategory;
import com.ibm.dtfj.java.JavaRuntimeMemorySection;
import com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils;
import com.ibm.j9ddr.vm29.pointer.generated.J9MemTagPointer;
import com.ibm.j9ddr.vm29.types.UDATA;
import com.ibm.j9ddr.vm29.view.dtfj.DTFJContext;

import static com.ibm.j9ddr.vm29.structure.J9PortLibrary.J9MEMTAG_EYECATCHER_ALLOC_HEADER;
import static com.ibm.j9ddr.vm29.pointer.helper.J9MemTagHelper.*;

/**
 * A JavaRuntimeMemorySection based on a J9MemTag malloced data chunk.
 * 
 * For all other memory tags @see DTFJGenericJavaRuntimeMemorySection
 *
 */
public class DTFJMemoryTagRuntimeMemorySection extends DTFJJavaRuntimeMemorySectionBase implements JavaRuntimeMemorySection
{

	private final J9MemTagPointer memoryTag;
	private final JavaRuntime runtime;
	
	public DTFJMemoryTagRuntimeMemorySection(JavaRuntime runtime, J9MemTagPointer memoryTag)
	{
		this.memoryTag = memoryTag;
		this.runtime = runtime;
	}

	public int getAllocationType()
	{
		try {
			UDATA eyeCatcher = memoryTag.eyeCatcher();
			
			if (eyeCatcher.eq(J9MEMTAG_EYECATCHER_ALLOC_HEADER)) {
				return JavaRuntimeMemorySection.ALLOCATION_TYPE_MALLOC_LIVE; 
			} else {
				return JavaRuntimeMemorySection.ALLOCATION_TYPE_MALLOC_FREED;
			}
			
		} catch (Exception e) {
			J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), e);
		} catch (NoSuchFieldError e) {
			J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), e);
		}
		return JavaRuntimeMemorySection.ALLOCATION_TYPE_MALLOC_LIVE;
	}

	public String getAllocator() throws CorruptDataException, DataUnavailable
	{
		try {
			return memoryTag.callSite().getCStringAtOffset(0);
		} catch (Exception e) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), e);
		} catch (NoSuchFieldError e) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), e);
		}
	}

	public JavaRuntimeMemoryCategory getMemoryCategory()
			throws CorruptDataException, DataUnavailable
	{
		try {
			return new DTFJJavaRuntimeMemoryCategory(runtime, memoryTag.category());
		} catch (Exception e) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), e);
		} catch (NoSuchFieldError e) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), e);
		}
	}

	public String getName()
	{
		StringBuilder b = new StringBuilder();
		
		b.append("JVM malloc'd storage allocated by ");
		
		try {
			String name = memoryTag.callSite().getCStringAtOffset(0);
			b.append(name);
		} catch (com.ibm.j9ddr.CorruptDataException e) {
			b.append("unknown (callsite unreadable)");
		}
		
		return b.toString();
	}
	
	public long getSize()
	{
		try {
			return memoryTag.allocSize().longValue();
		} catch (com.ibm.j9ddr.CorruptDataException e) {
			//Unlikely to happen - we've validated the memoryTag checksums OK already
		} catch (Exception e) {
			J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), e);
		} catch (NoSuchFieldError e) {
			J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), e);
		}
		return 0;
	}

	@Override
	protected long getBaseAddressAsLong()
	{
		//Return the memory returned from j9mem_allocate_memory, rather than the top of the eyecatchered block
		return j9mem_get_memory_base(memoryTag).getAddress();
	}

	@Override
	public int hashCode()
	{
		final int prime = 31;
		int result = 1;
		result = prime * result
				+ ((memoryTag == null) ? 0 : memoryTag.hashCode());
		result = prime * result + ((runtime == null) ? 0 : runtime.hashCode());
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
		if (!(obj instanceof DTFJMemoryTagRuntimeMemorySection)) {
			return false;
		}
		DTFJMemoryTagRuntimeMemorySection other = (DTFJMemoryTagRuntimeMemorySection) obj;
		if (memoryTag == null) {
			if (other.memoryTag != null) {
				return false;
			}
		} else if (!memoryTag.equals(other.memoryTag)) {
			return false;
		}
		if (runtime == null) {
			if (other.runtime != null) {
				return false;
			}
		} else if (!runtime.equals(other.runtime)) {
			return false;
		}
		return true;
	}
	
}
