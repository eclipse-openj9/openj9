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
import com.ibm.j9ddr.vm29.pointer.U16Pointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9LineNumberPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodDebugInfoPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9MethodDebugInfoHelper;
import com.ibm.j9ddr.vm29.types.I16;
import com.ibm.j9ddr.vm29.types.I32;
import com.ibm.j9ddr.vm29.types.U16;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.U8;
import com.ibm.j9ddr.vm29.events.EventManager;
import com.ibm.j9ddr.vm29.j9.AlgorithmVersion;

public abstract class LineNumberIterator implements Iterator<LineNumber> {

	public static LineNumberIterator lineNumberIteratorFor(J9MethodDebugInfoPointer methodInfo) throws CorruptDataException {
		/* If the version is right */
		if (AlgorithmVersion.getVersionOf("VM_LINE_NUMBER_TABLE_VERSION").getAlgorithmVersion() < 1) {
			return new LineNumberIterator_V0(methodInfo);
		} else {
			return new LineNumberIterator_V1(methodInfo);
		}
	}
	public abstract U8Pointer getLineNumberTablePtr();
	public void remove() {
		throw new UnsupportedOperationException();
	}

	private static class LineNumberIterator_V0 extends LineNumberIterator {
		private J9MethodDebugInfoPointer methodInfo;
		private J9LineNumberPointer lineNumberPtr;
		private U32 count = new U32(0);
		private U32 location = new U32(0);

		public LineNumberIterator_V0(J9MethodDebugInfoPointer methodInfo) throws CorruptDataException {
			this.methodInfo = methodInfo;
			this.lineNumberPtr = J9MethodDebugInfoHelper.getLineNumberTableForROMClass(methodInfo);
		}
		
		public boolean hasNext() {
			try {
				return count.lt(methodInfo.lineNumberCount());
			} catch (CorruptDataException e) {
				return false;
			}
		}

		public LineNumber next() {
			count = count.add(1);
			try {
				//FIXME : change in 27 code
				//location = location.add(lineNumberPtr.offsetLocation());
				LineNumber toReturn = new LineNumber(new U32(lineNumberPtr.lineNumber()), location);
				lineNumberPtr = lineNumberPtr.add(1);
				return toReturn;
			} catch (CorruptDataException ex) {
				EventManager.raiseCorruptDataEvent("CorruptData encountered walking port LineNumbers.", ex, false);
				return null;
			}
		}

		@Override
		public U8Pointer getLineNumberTablePtr() {
			return U8Pointer.cast(lineNumberPtr);
		}
	}
	private static class LineNumberIterator_V1 extends LineNumberIterator {
		private J9MethodDebugInfoPointer methodInfo;
		private U32 count = new U32(0);
		private U8Pointer lineNumberTablePtr;
		private U32 lineNumber = new U32(0);
		private U32 location = new U32(0);
		
		public LineNumberIterator_V1(J9MethodDebugInfoPointer methodInfo) throws CorruptDataException {
			this.methodInfo = methodInfo;
			lineNumberTablePtr = J9MethodDebugInfoHelper.getCompressedLineNumberTableForROMClassV1(methodInfo);
		}

		public boolean hasNext() {
			try {
				return count.lt(J9MethodDebugInfoHelper.getLineNumberCount(methodInfo));
			} catch (CorruptDataException e) {
				return false;
			}
		}
		
		public U8Pointer getLineNumberTablePtr() {
			return lineNumberTablePtr;
		}

		public LineNumber next() {
			count = count.add(1);
			try {
				U8 firstByte = lineNumberTablePtr.at(0);
				if (firstByte.bitAnd(0x80).eq(0)) {
					// 1 byte encoded : 0xxxxxyy
					location = location.add(firstByte.rightShift(2).bitAnd(0x1F));
					lineNumber = lineNumber.add(firstByte.bitAnd(0x3));

					lineNumberTablePtr = lineNumberTablePtr.add(1); // advance the pointer by one byte
				} else if (firstByte.bitAnd(0xC0).eq(0x80)) {
					// 2 bytes encoded : 10xxxxxY YYYYYYYY
					U16 encoded = new U16(firstByte).leftShift(8);
					lineNumberTablePtr = lineNumberTablePtr.add(1);
					encoded = new U16(encoded.bitOr(lineNumberTablePtr.at(0)));

					location = location.add(encoded.rightShift(9).bitAnd(0x1F));
					lineNumber = lineNumber.add(signExtend(new I16(encoded.bitAnd(0x1FF)), 9));

					lineNumberTablePtr = lineNumberTablePtr.add(1);
				} else if (firstByte.bitAnd(0xE0).eq(0xC0)) {
					// 3 bytes encoded : 110xxxxx xxYYYYYY YYYYYYYY
					U32 encoded = new U32(firstByte).leftShift(16);
					lineNumberTablePtr = lineNumberTablePtr.add(1);
					encoded = encoded.bitOr(U16Pointer.cast(lineNumberTablePtr).at(0));

					location = location.add(encoded.rightShift(14).bitAnd(0x7F));
					lineNumber = lineNumber.add(signExtend(new I16(encoded.bitAnd(0x3FFF)), 14));
					
					lineNumberTablePtr = lineNumberTablePtr.add(2);
				} else if (firstByte.bitAnd(0xF0).eq(0xE0)) {
					// 5 bytes encoded : 1110000Y xxxxxxxx xxxxxxxx YYYYYYYY YYYYYYYY
					lineNumberTablePtr = lineNumberTablePtr.add(1);
					location = location.add(U16Pointer.cast(lineNumberTablePtr).at(0));

					lineNumberTablePtr = lineNumberTablePtr.add(2);

					I32 lineNumberOffset = new I32(U16Pointer.cast(lineNumberTablePtr).at(0));
					if (firstByte.bitAnd(0x1).eq(0x1)) {
						// If the Y bit in 1110000Y is 1, then the lineNumberOffset is negative
						// set the sign bit to 1
						lineNumberOffset = lineNumberOffset.bitAnd(0xFFFF);
					}
					lineNumber = lineNumber.add(lineNumberOffset);

					lineNumberTablePtr = lineNumberTablePtr.add(2);
				} else {
					return null;
				}
			} catch (CorruptDataException ex) {
				EventManager.raiseCorruptDataEvent("CorruptData encountered walking port LineNumbers.", ex, false);
				return null;
			}
			return new LineNumber(lineNumber, location);
		}
		private static I16 signExtend(I16 number, int numberBits){
			int shiftAmount = (I16.SIZEOF * 8) - numberBits;
			return number.leftShift(shiftAmount).rightShift(shiftAmount);
		}
	}
}
