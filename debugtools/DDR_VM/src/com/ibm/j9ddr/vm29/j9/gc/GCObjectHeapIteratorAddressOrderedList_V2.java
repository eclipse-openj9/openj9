/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ModronThreadLocalHeapPointer;

import java.util.ArrayList;

class GCObjectHeapIteratorAddressOrderedList_V2 extends GCObjectHeapIteratorAddressOrderedList_V1 {

	protected GCObjectHeapIteratorAddressOrderedList_V2(U8Pointer base, U8Pointer top, boolean includeLiveObjects, boolean includeDeadObjects) throws CorruptDataException
	{
		super(base, top, includeLiveObjects, includeDeadObjects);
	}

	@Override
	protected U8Pointer getRealHeapTop(J9ModronThreadLocalHeapPointer threadLocalHeap, U8Pointer heapTop) throws CorruptDataException
	{
		return threadLocalHeap.realHeapTop();
	}

	@Override
	protected U8Pointer getRealHeapAlloc(J9ModronThreadLocalHeapPointer threadLocalHeap, U8Pointer heapAlloc) throws CorruptDataException
	{
		return heapAlloc;
	}
}
