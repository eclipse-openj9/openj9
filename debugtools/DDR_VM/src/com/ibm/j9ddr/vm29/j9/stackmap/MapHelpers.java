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
package com.ibm.j9ddr.vm29.j9.stackmap;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.U16Pointer;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.types.U16;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.U8;

/**
 * Bucket for routines shared between LocalMap and StackMap
 * 
 * @author andhall
 *
 */
class MapHelpers
{

	/* In the native, the PARALLEL_TYPE is U32. In Java it is 32 bit integer */
	public static final int PARALLEL_BYTES = 4;
	public static final int PARALLEL_BITS = PARALLEL_BYTES * 8;
	public static final int PARALLEL_WRITES = 1;
	
	public static U8 PARAM_8(AbstractPointer ptr, int offset) throws CorruptDataException
	{
		U8Pointer castPtr = U8Pointer.cast(ptr.addOffset(offset));
		
		return castPtr.at(0);
	}
	
	public static U16 PARAM_16(AbstractPointer ptr, int offset) throws CorruptDataException
	{
		U16Pointer castPtr = U16Pointer.cast(ptr.addOffset(offset));
		
		return castPtr.at(0);
	}
	
	public static U32 PARAM_32(AbstractPointer ptr, int offset) throws CorruptDataException
	{
		U32Pointer castPtr = U32Pointer.cast(ptr.addOffset(offset));
		
		return castPtr.at(0);
	}
	
}
