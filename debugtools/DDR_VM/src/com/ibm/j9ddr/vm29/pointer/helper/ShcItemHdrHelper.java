/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.pointer.helper;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.ShcItemHdrPointer;
import com.ibm.j9ddr.vm29.structure.ShcItemHdr;
import com.ibm.j9ddr.vm29.types.U32;

public class ShcItemHdrHelper {
	/* ->itemLen should never be set or read without these macros as the lower bit is used to indicate staleness */
	public static U32 CCITEMLEN(ShcItemHdrPointer ptr) throws CorruptDataException {
		return new U32(ptr.itemLen()).bitAnd(0xFFFFFFFE);
	}
	
	// #define CCITEM(ih) (((U_8*)(ih)) - (CCITEMLEN(ih) - sizeof(ShcItemHdr)))
	public static U8Pointer CCITEM(ShcItemHdrPointer ptr) throws CorruptDataException {
		return U8Pointer.cast(ptr).sub(CCITEMLEN(ptr).sub(ShcItemHdr.SIZEOF));
	}

	// #define CCITEMNEXT(ih) ((ShcItemHdr*)(CCITEM(ih) - sizeof(ShcItemHdr)))
	public static ShcItemHdrPointer CCITEMNEXT(ShcItemHdrPointer ptr) throws CorruptDataException {
		return ShcItemHdrPointer.cast(CCITEM(ptr).sub(ShcItemHdr.SIZEOF));
	}
	
	//	#define CCITEMSTALE(ih) (J9SHR_READMEM((ih)->itemLen) & 0x1)	
	public static boolean CCITEMSTALE(ShcItemHdrPointer ptr) throws CorruptDataException {
		if(ptr.itemLen().bitAnd(0x1).eq(0)) {
			return false;
		} else {
			return true;
		}
	}
}
