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
package com.ibm.j9ddr.vm29.j9.walkers;

import java.util.Iterator;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.events.EventManager;
import com.ibm.j9ddr.vm29.j9.AlgorithmVersion;
import com.ibm.j9ddr.vm29.j9.OptInfo;
import com.ibm.j9ddr.vm29.pointer.I32Pointer;
import com.ibm.j9ddr.vm29.pointer.SelfRelativePointer;
import com.ibm.j9ddr.vm29.pointer.U16Pointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodDebugInfoPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9UTF8Pointer;
import com.ibm.j9ddr.vm29.structure.J9UTF8;
import com.ibm.j9ddr.vm29.types.I32;
import com.ibm.j9ddr.vm29.types.U16;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.U8;
import com.ibm.j9ddr.vm29.structure.J9NonbuilderConstants;

public abstract class LocalVariableTableIterator implements Iterator<LocalVariableTable> {

	public static void checkVariableTableVersion() throws CorruptDataException {
		int version = AlgorithmVersion.getVersionOf("VM_LOCAL_VARIABLE_TABLE_VERSION").getAlgorithmVersion();
		if (version < 1) {
			throw new CorruptDataException("Unexpected local variable table version " + version);
		}
	}

	public static LocalVariableTableIterator localVariableTableIteratorFor(J9MethodDebugInfoPointer methodInfo) throws CorruptDataException {
		checkVariableTableVersion();
		return new LocalVariableTableIterator_V1(methodInfo);
	}

	public void remove() {
		throw new UnsupportedOperationException();
	}

	public abstract U8Pointer getLocalVariableTablePtr();

	private static class LocalVariableTableIterator_V1 extends LocalVariableTableIterator {
		private J9MethodDebugInfoPointer methodInfo;
		private U32 count = new U32(0);
		private U8Pointer localVariableTablePtr;
		private U32 slotNumber = new U32(0);
		private U32 startVisibility = new U32(0);
		private U32 visibilityLength = new U32(0);
		
		public LocalVariableTableIterator_V1(J9MethodDebugInfoPointer methodInfo) throws CorruptDataException {
			this.methodInfo = methodInfo;
			localVariableTablePtr = OptInfo.getV1VariableTableForMethodDebugInfo(methodInfo);
		}

		public boolean hasNext() {
			try {
				return count.lt(methodInfo.varInfoCount());
			} catch (CorruptDataException e) {
				return false;
			}
		}

		public LocalVariableTable next() {
			count = count.add(1);
			J9UTF8Pointer name;
			J9UTF8Pointer signature;
			J9UTF8Pointer genericSignature;
			try {
				U8 firstByte = localVariableTablePtr.at(0);
				if (firstByte.bitAnd(0x80).eq(0)) {
					/* 0xZZZZZZ */
					localVariableTablePtr = localVariableTablePtr.add(U8.SIZEOF);

					slotNumber = slotNumber.add(firstByte.rightShift(6));
					visibilityLength = visibilityLength.add(signExtend(new I32(firstByte.bitAnd(0x3F)), 6));//(((firstByte & 0x3F) ^ m) - m); /* sign extend from 6bit */;
				} else if (firstByte.bitAnd(0xC0).eq(0x80)) {
					/* 10xYYYYY ZZZZZZZZ */
					localVariableTablePtr = localVariableTablePtr.add(U8.SIZEOF);

					slotNumber = slotNumber.add(firstByte.rightShift(5).bitAnd(1));
					startVisibility = startVisibility.add(signExtend(new I32(firstByte.bitAnd(0x1F)), 5)); /* sign extend from 5bit */;
					visibilityLength = visibilityLength.add(signExtend(new I32(localVariableTablePtr.at(0)), 8)); /* sign extend from 8bit */;
					localVariableTablePtr = localVariableTablePtr.add(U8.SIZEOF);
				} else if (firstByte.bitAnd(0xE0).eq(0xC0)) {
					/* 110xYYYY YYYYYZZZ ZZZZZZZZ */
					localVariableTablePtr = localVariableTablePtr.add(U8.SIZEOF);

					slotNumber = slotNumber.add(firstByte.rightShift(4).bitAnd(1));
					U32 result = new U32(firstByte).leftShift(16);
					result = result.bitOr(U16Pointer.cast(localVariableTablePtr).at(0));
					localVariableTablePtr = localVariableTablePtr.add(U16.SIZEOF);

					startVisibility = startVisibility.add(signExtend(new I32(result.rightShift(11).bitAnd(0x1FF)), 9)); /* sign extend from 9bit */;
					visibilityLength = visibilityLength.add(signExtend(new I32(result.bitAnd(0x7FF)), 11)); /* sign extend from 11bit */;
				} else if (firstByte.bitAnd(0xF0).eq(0xE0)) {
					/* 1110xxZZ ZZZZZZZZ ZZZZZZZZ YYYYYYYY YYYYYYYY */
					U32 result = new U32(firstByte);
					U32 visibilityLengthUnsigned;
					localVariableTablePtr = localVariableTablePtr.add(U8.SIZEOF);

					slotNumber = slotNumber.add(result.rightShift(2).bitAnd(3));
					visibilityLengthUnsigned = result.bitAnd(3).leftShift(16);
					visibilityLengthUnsigned = visibilityLengthUnsigned.bitOr(U16Pointer.cast(localVariableTablePtr).at(0));
					localVariableTablePtr = localVariableTablePtr.add(U16.SIZEOF);
					visibilityLength = visibilityLength.add(signExtend(new I32(visibilityLengthUnsigned), 18)); /* sign extend from 18bit */;

					startVisibility = startVisibility.add(signExtend(new I32(U16Pointer.cast(localVariableTablePtr).at(0)), 16)); /* sign extend from 16bit */;
					localVariableTablePtr = localVariableTablePtr.add(U16.SIZEOF);
				} else if (firstByte.eq(0xF0)) {
					/* 11110000	FULL DATA in case it overflow in some classes */
					localVariableTablePtr = localVariableTablePtr.add(U8.SIZEOF);
					slotNumber = slotNumber.add(I32Pointer.cast(localVariableTablePtr).at(0));
					localVariableTablePtr = localVariableTablePtr.add(I32.SIZEOF);
					startVisibility = startVisibility.add(I32Pointer.cast(localVariableTablePtr).at(0));
					localVariableTablePtr = localVariableTablePtr.add(I32.SIZEOF);
					visibilityLength = visibilityLength.add(I32Pointer.cast(localVariableTablePtr).at(0));
					localVariableTablePtr = localVariableTablePtr.add(I32.SIZEOF);
				} else {
					return null;
				}
				name = J9UTF8Pointer.cast(SelfRelativePointer.cast(localVariableTablePtr).get());
				localVariableTablePtr = localVariableTablePtr.add(J9UTF8.SIZEOF);
				
				signature = J9UTF8Pointer.cast(SelfRelativePointer.cast(localVariableTablePtr).get());
				localVariableTablePtr = localVariableTablePtr.add(J9UTF8.SIZEOF);

				if (visibilityLength.anyBitsIn(J9NonbuilderConstants.J9_ROMCLASS_OPTINFO_VARIABLE_TABLE_HAS_GENERIC)) {
					genericSignature = J9UTF8Pointer.cast(SelfRelativePointer.cast(localVariableTablePtr).get());
					localVariableTablePtr = localVariableTablePtr.add(J9UTF8.SIZEOF);
				} else {
					genericSignature = J9UTF8Pointer.NULL;
				}

				visibilityLength = visibilityLength.bitAnd(~J9NonbuilderConstants.J9_ROMCLASS_OPTINFO_VARIABLE_TABLE_HAS_GENERIC);
			} catch (CorruptDataException ex) {
				EventManager.raiseCorruptDataEvent("CorruptData encountered walking local variable table.", ex, false);
				return null;
			}
			return new LocalVariableTable(slotNumber, startVisibility, visibilityLength, genericSignature, name, signature);
		}
		private static I32 signExtend(I32 i32, int numberBits){
			int shiftAmount = (I32.SIZEOF * 8) - numberBits;
			return i32.leftShift(shiftAmount).rightShift(shiftAmount);
		}

		@Override
		public U8Pointer getLocalVariableTablePtr() {
			return localVariableTablePtr;
		}
	}
}
