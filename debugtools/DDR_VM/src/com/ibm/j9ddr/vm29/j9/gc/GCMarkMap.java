/*******************************************************************************
 * Copyright (c) 2001, 2015 IBM Corp. and others
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
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapMapPointer;
import com.ibm.j9ddr.vm29.structure.MM_HeapMap;
import com.ibm.j9ddr.vm29.types.UDATA;

public class GCMarkMap extends GCHeapMap
{
	public static final UDATA J9MODRON_HEAP_SLOTS_PER_MARK_BIT = new UDATA(MM_HeapMap.J9MODRON_HEAP_SLOTS_PER_HEAPMAP_BIT);
	public static final UDATA J9MODRON_HEAP_SLOTS_PER_MARK_SLOT = J9MODRON_HEAP_SLOTS_PER_HEAPMAP_SLOT;
	public static final UDATA J9MODRON_HEAP_BYTES_PER_MARK_BYTE = J9MODRON_HEAP_BYTES_PER_HEAPMAP_BYTE;
	
	
	public GCMarkMap(MM_HeapMapPointer markMap) throws CorruptDataException 
	{
		super(markMap);
	}
}
