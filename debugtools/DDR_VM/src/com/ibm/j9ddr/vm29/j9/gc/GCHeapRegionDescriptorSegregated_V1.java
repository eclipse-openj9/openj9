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
package com.ibm.j9ddr.vm29.j9.gc;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.AlgorithmVersion;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapRegionDescriptorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapRegionDescriptorSegregatedPointer;
import com.ibm.j9ddr.vm29.types.UDATA;

public class GCHeapRegionDescriptorSegregated_V1 extends GCHeapRegionDescriptor_V1
{
	protected UDATA sizeClass;
	protected UDATA cellSize;
	
	public GCHeapRegionDescriptorSegregated_V1(MM_HeapRegionDescriptorSegregatedPointer region) throws CorruptDataException
	{
		super(MM_HeapRegionDescriptorPointer.cast(region));
		sizeClass = region._sizeClass();
		cellSize = region._segregatedSizeClasses().smallCellSizesEA().at(sizeClass);
	}
	
	public UDATA getSizeClass() 
	{
		return sizeClass;
	}

	public UDATA getCellSize() throws CorruptDataException 
	{
		return cellSize;
	}
	
	
	@Override
	public GCObjectHeapIterator objectIterator(boolean includeLiveObjects, boolean includeDeadObjects) throws CorruptDataException
	{
		AlgorithmVersion version = AlgorithmVersion.getVersionOf(AlgorithmVersion.GC_OBJECT_HEAP_ITERATOR_SEGREGATED_VERSION);
		switch (version.getAlgorithmVersion()) {
			// Add case statements for new algorithm versions
			default:
				return new GCObjectHeapIteratorSegregated_V1(U8Pointer.cast(getLowAddress()), U8Pointer.cast(getWalkableHighAddress()), getRegionType(), getCellSize(), includeLiveObjects, includeDeadObjects);
		} 
	}
}
