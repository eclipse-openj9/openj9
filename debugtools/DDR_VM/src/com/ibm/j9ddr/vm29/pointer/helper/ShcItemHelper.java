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
package com.ibm.j9ddr.vm29.pointer.helper;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.ShcItemPointer;
import com.ibm.j9ddr.vm29.structure.ShcItem;

public class ShcItemHelper {
//	#define ITEMJVMID(i) (J9SHR_READMEM((i)->jvmID))

//	#define ITEMDATALEN(i) (J9SHR_READMEM((i)->dataLen) - sizeof(ShcItem))

//	#define ITEMDATA(i) (((U_8*)(i)) + sizeof(ShcItem))
	public static U8Pointer ITEMDATA(ShcItemPointer ptr) {
		return U8Pointer.cast(ptr).add(ShcItem.SIZEOF);
	}

	// #define ITEMEND(i) (((U_8*)(i)) + J9SHR_READMEM((i)->dataLen))
	public static U8Pointer ITEMEND(ShcItemPointer ptr) throws CorruptDataException {
		return U8Pointer.cast(ptr).add(ptr.dataLen());
	}
}
