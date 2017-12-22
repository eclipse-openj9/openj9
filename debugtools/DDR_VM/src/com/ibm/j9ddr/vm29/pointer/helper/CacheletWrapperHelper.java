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
import com.ibm.j9ddr.vm29.pointer.generated.CacheletWrapperPointer;
import com.ibm.j9ddr.vm29.structure.CacheletWrapper;

public class CacheletWrapperHelper {
//	#define CLETHINTS(clet) (((U_8*)(clet)) + sizeof(CacheletWrapper))
	public static U8Pointer CLETHINTS(CacheletWrapperPointer ptr) {
		return U8Pointer.cast(ptr).add(CacheletWrapper.SIZEOF);
	}
	
//	#define CLETDATA(clet) (((U_8*)(clet)) + J9SHR_READSRP((clet)->dataStart))	
	public static U8Pointer CLETDATA(CacheletWrapperPointer ptr) throws CorruptDataException {
		return U8Pointer.cast(ptr).add(ptr.dataStart());
	}
}
