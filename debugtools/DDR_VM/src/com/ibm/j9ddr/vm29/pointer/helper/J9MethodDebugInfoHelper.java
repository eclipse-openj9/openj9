/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
import com.ibm.j9ddr.vm29.j9.AlgorithmVersion;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9LineNumberPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodDebugInfoPointer;
import com.ibm.j9ddr.vm29.types.U32;

public class J9MethodDebugInfoHelper {

	public static U32 getLineNumberCount(J9MethodDebugInfoPointer ptr) throws CorruptDataException {
		U32 lineNumberCount = new U32(ptr.lineNumberCount());
		if (AlgorithmVersion.getVersionOf("VM_LINE_NUMBER_TABLE_VERSION").getAlgorithmVersion() < 1) {
			return lineNumberCount;
		} else {
			if (lineNumberCount.bitAnd(1).eq(0)) {
				return lineNumberCount.rightShift(1).bitAnd(0x7FFF);
			} else {
				return lineNumberCount.rightShift(1);
			}
		}
	}

	public static J9LineNumberPointer getLineNumberTableForROMClass(J9MethodDebugInfoPointer ptr) throws CorruptDataException {
		if (!ptr.lineNumberCount().eq(0)) {
			return J9LineNumberPointer.cast(ptr.add(1));
		}
		return J9LineNumberPointer.NULL;
	}
	public static U8Pointer getCompressedLineNumberTableForROMClassV1(J9MethodDebugInfoPointer ptr) throws CorruptDataException {
		if (!getLineNumberCount(ptr).eq(0)) {
			if (ptr.lineNumberCount().bitAnd(1).eq(1)) {
				return U8Pointer.cast(ptr.add(1)).add(U32.SIZEOF);
			} else {
				return U8Pointer.cast(ptr.add(1));
			}
		}
		return U8Pointer.NULL;
	}
	
	public static U32 getLineNumberCompressedSize(J9MethodDebugInfoPointer ptr) throws CorruptDataException {
		U32 lineNumberCount = new U32(ptr.lineNumberCount());
		if (lineNumberCount.bitAnd(1).eq(0)) {
			return lineNumberCount.rightShift(16).bitAnd(0xFFFF);
		} else {
			return U32Pointer.cast(ptr.add(1)).at(0);
		}
	}
}
