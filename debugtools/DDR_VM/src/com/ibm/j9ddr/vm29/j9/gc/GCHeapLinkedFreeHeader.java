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
package com.ibm.j9ddr.vm29.j9.gc;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.AlgorithmVersion;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.types.UDATA;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapLinkedFreeHeaderPointer;

public abstract class GCHeapLinkedFreeHeader {
	protected MM_HeapLinkedFreeHeaderPointer heapLinkedFreeHeaderPointer;
	
	/* Do not instantiate. Use the factory */
	protected GCHeapLinkedFreeHeader(UDATA udata) throws CorruptDataException
	{
		heapLinkedFreeHeaderPointer = MM_HeapLinkedFreeHeaderPointer.cast(udata.longValue());
	}
	
	/* Do not instantiate. Use the factory */
	protected GCHeapLinkedFreeHeader(MM_HeapLinkedFreeHeaderPointer heapLinkedFreeHeaderPointer)
	{
		this.heapLinkedFreeHeaderPointer = heapLinkedFreeHeaderPointer;
	}
	
	/**
	 * Factory method to construct an appropriate heap linked free header
	 * 
	 * @param object the "J9Object" structure to view as a HeapLinkedFreeHeader
	 * 
	 * @return an instance of GCHeapLinkedFreeHeader
	 * @throws CorruptDataException
	 */
	public static GCHeapLinkedFreeHeader fromJ9Object(J9ObjectPointer object) throws CorruptDataException
	{
		AlgorithmVersion version = AlgorithmVersion.getVersionOf(AlgorithmVersion.GC_HEAP_LINKED_FREE_HEADER_VERSION);
		switch (version.getAlgorithmVersion()) {
		// Add case statements for new algorithm versions
			default:
				return fromLinkedFreeHeaderPointer(MM_HeapLinkedFreeHeaderPointer.cast(object));
		}
	}
	
	public static GCHeapLinkedFreeHeader fromLinkedFreeHeaderPointer(MM_HeapLinkedFreeHeaderPointer heapLinkedFreeHeaderPointer)
	{
		AlgorithmVersion version = AlgorithmVersion.getVersionOf(AlgorithmVersion.GC_HEAP_LINKED_FREE_HEADER_VERSION);
		switch (version.getAlgorithmVersion()) {
		// Add case statements for new algorithm versions
			default:
				return new GCHeapLinkedFreeHeader_V1(heapLinkedFreeHeaderPointer);
		}
	}
	
	/**
	 * Get the size in bytes of this free entry. The size is measured
	 * from the beginning of the header.
	 * @return size in bytes
	 * @throws CorruptDataException
	 */
	public abstract UDATA getSize() throws CorruptDataException;
	
	/**
	 * Get the next HeapLinkedFreeHeader in the free list.
	 * @return size in bytes
	 * @throws CorruptDataException
	 */
	public abstract GCHeapLinkedFreeHeader getNext() throws CorruptDataException;
	
	/**
	 * Get the object represented by this free list entry.
	 * @return object the free list entry cast to a J9ObjectPointer
	 */
	public J9ObjectPointer getObject()
	{
		return J9ObjectPointer.cast(getHeader());
	}
	
	public MM_HeapLinkedFreeHeaderPointer getHeader()
	{
		return heapLinkedFreeHeaderPointer;
	}
	
	@Override
	public String toString()
	{
		return heapLinkedFreeHeaderPointer.getHexAddress();
	}
}
