/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
package org.openj9.test.varhandle;

import java.lang.invoke.VarHandle;
import java.nio.*;

import org.testng.*;

class ViewVarHandleTests {
	ByteOrder _byteOrder;
	ByteBuffer _buffer;
	
	// These should be static, but we use _byteOrder (test parameter) to create them.
	VarHandle vhChar;
	VarHandle vhDouble;
	VarHandle vhFloat;
	VarHandle vhInt;
	VarHandle vhLong;
	VarHandle vhShort;
	
	// Expected value of the first element in the buffer for each type 
	final char FIRST_CHAR;
	final double FIRST_DOUBLE;
	final float FIRST_FLOAT;
	final int FIRST_INT;
	final long FIRST_LONG;
	final short FIRST_SHORT;
	
	// The value we change to and expect to see after updates
	final char CHANGED_CHAR;
	final double CHANGED_DOUBLE;
	final float CHANGED_FLOAT;
	final int CHANGED_INT;
	final long CHANGED_LONG;
	final short CHANGED_SHORT;

	// Expected value of the last element in the buffer for each type 
	final char LAST_CHAR;
	final double LAST_DOUBLE;
	final float LAST_FLOAT;
	final int LAST_INT;
	final long LAST_LONG;
	final short LAST_SHORT;
	
	// Expected values of values at other indices
	final int INITIAL_INT_AT_INDEX_3;
	final int INITIAL_INT_AT_INDEX_4;
	final short INITIAL_SHORT_AT_INDEX_1;
	
	public ViewVarHandleTests(String byteOrder) {
		if (byteOrder.equals("bigEndian")) {
			_byteOrder = ByteOrder.BIG_ENDIAN;
			
			FIRST_CHAR = ConstantsHelper.FIRST_CHAR_BE;
			FIRST_DOUBLE = ConstantsHelper.FIRST_DOUBLE_BE;
			FIRST_FLOAT = ConstantsHelper.FIRST_FLOAT_BE;
			FIRST_INT = ConstantsHelper.FIRST_INT_BE;
			FIRST_LONG = ConstantsHelper.FIRST_LONG_BE;
			FIRST_SHORT = ConstantsHelper.FIRST_SHORT_BE;

			CHANGED_CHAR = ConstantsHelper.CHANGED_CHAR_BE;
			CHANGED_DOUBLE = ConstantsHelper.CHANGED_DOUBLE_BE;
			CHANGED_FLOAT = ConstantsHelper.CHANGED_FLOAT_BE;
			CHANGED_INT = ConstantsHelper.CHANGED_INT_BE;
			CHANGED_LONG = ConstantsHelper.CHANGED_LONG_BE;
			CHANGED_SHORT = ConstantsHelper.CHANGED_SHORT_BE;
			
			LAST_CHAR = ConstantsHelper.LAST_CHAR_BE;
			LAST_DOUBLE = ConstantsHelper.LAST_DOUBLE_BE;
			LAST_FLOAT = ConstantsHelper.LAST_FLOAT_BE;
			LAST_INT = ConstantsHelper.LAST_INT_BE;
			LAST_LONG = ConstantsHelper.LAST_LONG_BE;
			LAST_SHORT = ConstantsHelper.LAST_SHORT_BE;

			INITIAL_INT_AT_INDEX_3 = ConstantsHelper.INITIAL_INT_AT_INDEX_3_BE;
			INITIAL_INT_AT_INDEX_4 = ConstantsHelper.INITIAL_INT_AT_INDEX_4_BE;
			INITIAL_SHORT_AT_INDEX_1 = ConstantsHelper.INITIAL_SHORT_AT_INDEX_1_BE;
		} else if (byteOrder.equals("littleEndian")) {
			_byteOrder = ByteOrder.LITTLE_ENDIAN;
			
			FIRST_CHAR = ConstantsHelper.FIRST_CHAR_LE;
			FIRST_DOUBLE = ConstantsHelper.FIRST_DOUBLE_LE;
			FIRST_FLOAT = ConstantsHelper.FIRST_FLOAT_LE;
			FIRST_INT = ConstantsHelper.FIRST_INT_LE;
			FIRST_LONG = ConstantsHelper.FIRST_LONG_LE;
			FIRST_SHORT = ConstantsHelper.FIRST_SHORT_LE;

			CHANGED_CHAR = ConstantsHelper.CHANGED_CHAR_LE;
			CHANGED_DOUBLE = ConstantsHelper.CHANGED_DOUBLE_LE;
			CHANGED_FLOAT = ConstantsHelper.CHANGED_FLOAT_LE;
			CHANGED_INT = ConstantsHelper.CHANGED_INT_LE;
			CHANGED_LONG = ConstantsHelper.CHANGED_LONG_LE;
			CHANGED_SHORT = ConstantsHelper.CHANGED_SHORT_LE;
			
			LAST_CHAR = ConstantsHelper.LAST_CHAR_LE;
			LAST_DOUBLE = ConstantsHelper.LAST_DOUBLE_LE;
			LAST_FLOAT = ConstantsHelper.LAST_FLOAT_LE;
			LAST_INT = ConstantsHelper.LAST_INT_LE;
			LAST_LONG = ConstantsHelper.LAST_LONG_LE;
			LAST_SHORT = ConstantsHelper.LAST_SHORT_LE;

			INITIAL_INT_AT_INDEX_3 = ConstantsHelper.INITIAL_INT_AT_INDEX_3_LE;
			INITIAL_INT_AT_INDEX_4 = ConstantsHelper.INITIAL_INT_AT_INDEX_4_LE;
			INITIAL_SHORT_AT_INDEX_1 = ConstantsHelper.INITIAL_SHORT_AT_INDEX_1_LE;
		} else {
			throw new TestException(byteOrder + " is an invalid byte order. Should be either bigEndian or littleEndian.");
		}
	}
	
	void checkUpdated2(int offset) {
		Assert.assertEquals((byte)10, _buffer.get(offset + 0));
		Assert.assertEquals((byte)20, _buffer.get(offset + 1));
	}
	
	void checkUpdated4(int offset) {
		Assert.assertEquals((byte)10, _buffer.get(offset + 0));
		Assert.assertEquals((byte)20, _buffer.get(offset + 1));
		Assert.assertEquals((byte)30, _buffer.get(offset + 2));
		Assert.assertEquals((byte)40, _buffer.get(offset + 3));
	}
	
	void checkNotUpdated4(int offset) {
		Assert.assertEquals((byte)(offset + 1), _buffer.get(offset + 0));
		Assert.assertEquals((byte)(offset + 2), _buffer.get(offset + 1));
		Assert.assertEquals((byte)(offset + 3), _buffer.get(offset + 2));
		Assert.assertEquals((byte)(offset + 4), _buffer.get(offset + 3));
	}
	
	void checkUpdated8(int offset) {
		Assert.assertEquals((byte)10, _buffer.get(offset + 0));
		Assert.assertEquals((byte)20, _buffer.get(offset + 1));
		Assert.assertEquals((byte)30, _buffer.get(offset + 2));
		Assert.assertEquals((byte)40, _buffer.get(offset + 3));
		Assert.assertEquals((byte)50, _buffer.get(offset + 4));
		Assert.assertEquals((byte)60, _buffer.get(offset + 5));
		Assert.assertEquals((byte)70, _buffer.get(offset + 6));
		Assert.assertEquals((byte)80, _buffer.get(offset + 7));
	}
	
	void checkNotUpdated8() {
		Assert.assertEquals((byte)1, _buffer.get(0));
		Assert.assertEquals((byte)2, _buffer.get(1));
		Assert.assertEquals((byte)3, _buffer.get(2));
		Assert.assertEquals((byte)4, _buffer.get(3));
		Assert.assertEquals((byte)5, _buffer.get(4));
		Assert.assertEquals((byte)6, _buffer.get(5));
		Assert.assertEquals((byte)7, _buffer.get(6));
		Assert.assertEquals((byte)8, _buffer.get(7));
	}
	
	void checkIntAddition(byte value, int offset) {
		if (_byteOrder == ByteOrder.BIG_ENDIAN) {
			Assert.assertEquals((byte)(offset + 1), _buffer.get(offset + 0));
			Assert.assertEquals((byte)(offset + 4 + value), _buffer.get(offset + 3));
		} else {
			Assert.assertEquals((byte)(offset + 1 + value), _buffer.get(offset + 0));
			Assert.assertEquals((byte)(offset + 4), _buffer.get(offset + 3));
		}
		Assert.assertEquals((byte)(offset + 2), _buffer.get(offset + 1));
		Assert.assertEquals((byte)(offset + 3), _buffer.get(offset + 2));
	}
	
	void checkIntBitwiseAnd(int mask, int offset) {
		if (_byteOrder == ByteOrder.BIG_ENDIAN) {
			Assert.assertEquals((byte)((offset + 1) & (mask & 0xFF000000)), _buffer.get(offset + 0));
			Assert.assertEquals((byte)((offset + 2) & (mask & 0x00FF0000)), _buffer.get(offset + 1));
			Assert.assertEquals((byte)((offset + 3) & (mask & 0x0000FF00)), _buffer.get(offset + 2));
			Assert.assertEquals((byte)((offset + 4) & (mask & 0x000000FF)), _buffer.get(offset + 3));
		} else {
			Assert.assertEquals((byte)((offset + 1) & (mask & 0x000000FF)), _buffer.get(offset + 0));
			Assert.assertEquals((byte)((offset + 2) & (mask & 0x0000FF00)), _buffer.get(offset + 1));
			Assert.assertEquals((byte)((offset + 3) & (mask & 0x00FF0000)), _buffer.get(offset + 2));
			Assert.assertEquals((byte)((offset + 4) & (mask & 0xFF000000)), _buffer.get(offset + 3));
		}
	}
	
	void checkIntBitwiseOr(int mask, int offset) {
		if (_byteOrder == ByteOrder.BIG_ENDIAN) {
			Assert.assertEquals((byte)((offset + 1) | (mask & 0xFF000000)), _buffer.get(offset + 0));
			Assert.assertEquals((byte)((offset + 2) | (mask & 0x00FF0000)), _buffer.get(offset + 1));
			Assert.assertEquals((byte)((offset + 3) | (mask & 0x0000FF00)), _buffer.get(offset + 2));
			Assert.assertEquals((byte)((offset + 4) | (mask & 0x000000FF)), _buffer.get(offset + 3));
		} else {
			Assert.assertEquals((byte)((offset + 1) | (mask & 0x000000FF)), _buffer.get(offset + 0));
			Assert.assertEquals((byte)((offset + 2) | (mask & 0x0000FF00)), _buffer.get(offset + 1));
			Assert.assertEquals((byte)((offset + 3) | (mask & 0x00FF0000)), _buffer.get(offset + 2));
			Assert.assertEquals((byte)((offset + 4) | (mask & 0xFF000000)), _buffer.get(offset + 3));
		}
	}
	
	void checkIntBitwiseXor(int mask, int offset) {
		if (_byteOrder == ByteOrder.BIG_ENDIAN) {
			Assert.assertEquals((byte)((offset + 1) ^ (mask & 0xFF000000)), _buffer.get(offset + 0));
			Assert.assertEquals((byte)((offset + 2) ^ (mask & 0x00FF0000)), _buffer.get(offset + 1));
			Assert.assertEquals((byte)((offset + 3) ^ (mask & 0x0000FF00)), _buffer.get(offset + 2));
			Assert.assertEquals((byte)((offset + 4) ^ (mask & 0x000000FF)), _buffer.get(offset + 3));
		} else {
			Assert.assertEquals((byte)((offset + 1) ^ (mask & 0x000000FF)), _buffer.get(offset + 0));
			Assert.assertEquals((byte)((offset + 2) ^ (mask & 0x0000FF00)), _buffer.get(offset + 1));
			Assert.assertEquals((byte)((offset + 3) ^ (mask & 0x00FF0000)), _buffer.get(offset + 2));
			Assert.assertEquals((byte)((offset + 4) ^ (mask & 0xFF000000)), _buffer.get(offset + 3));
		}
	}
	
	void checkLongAddition() {
		if (_byteOrder == ByteOrder.BIG_ENDIAN) {
			Assert.assertEquals((byte)1, _buffer.get(0));
			Assert.assertEquals((byte)10, _buffer.get(7));
		} else {
			Assert.assertEquals((byte)3, _buffer.get(0));
			Assert.assertEquals((byte)8, _buffer.get(7));
		}
		Assert.assertEquals((byte)2, _buffer.get(1));
		Assert.assertEquals((byte)3, _buffer.get(2));
		Assert.assertEquals((byte)4, _buffer.get(3));
		Assert.assertEquals((byte)5, _buffer.get(4));
		Assert.assertEquals((byte)6, _buffer.get(5));
		Assert.assertEquals((byte)7, _buffer.get(6));
	}
	
	void checkLongBitwiseAnd() {
		if (_byteOrder == ByteOrder.BIG_ENDIAN) {
			Assert.assertEquals((byte)0, _buffer.get(0));
			Assert.assertEquals((byte)8, _buffer.get(7));
		} else {
			Assert.assertEquals((byte)1, _buffer.get(0));
			Assert.assertEquals((byte)0, _buffer.get(7));
		}
		Assert.assertEquals((byte)0, _buffer.get(1));
		Assert.assertEquals((byte)0, _buffer.get(2));
		Assert.assertEquals((byte)0, _buffer.get(3));
		Assert.assertEquals((byte)0, _buffer.get(4));
		Assert.assertEquals((byte)0, _buffer.get(5));
		Assert.assertEquals((byte)0, _buffer.get(6));
	}
	
	void checkLongBitwiseOr() {
		if (_byteOrder == ByteOrder.BIG_ENDIAN) {
			Assert.assertEquals((byte)1, _buffer.get(0));
			Assert.assertEquals((byte)10, _buffer.get(7));
		} else {
			Assert.assertEquals((byte)3, _buffer.get(0));
			Assert.assertEquals((byte)8, _buffer.get(7));
		}
		Assert.assertEquals((byte)2, _buffer.get(1));
		Assert.assertEquals((byte)3, _buffer.get(2));
		Assert.assertEquals((byte)4, _buffer.get(3));
		Assert.assertEquals((byte)5, _buffer.get(4));
		Assert.assertEquals((byte)6, _buffer.get(5));
		Assert.assertEquals((byte)7, _buffer.get(6));
	}
	
	void checkLongBitwiseXor() {
		if (_byteOrder == ByteOrder.BIG_ENDIAN) {
			Assert.assertEquals((byte)1, _buffer.get(0));
			Assert.assertEquals((byte)2, _buffer.get(7));
		} else {
			Assert.assertEquals((byte)11, _buffer.get(0));
			Assert.assertEquals((byte)8, _buffer.get(7));
		}
		Assert.assertEquals((byte)2, _buffer.get(1));
		Assert.assertEquals((byte)3, _buffer.get(2));
		Assert.assertEquals((byte)4, _buffer.get(3));
		Assert.assertEquals((byte)5, _buffer.get(4));
		Assert.assertEquals((byte)6, _buffer.get(5));
		Assert.assertEquals((byte)7, _buffer.get(6));
	}

	/**
	 * Calculates the last aligned index that can store an element of size {@code bytes}.
	 *  
	 * @param bytes The size in bytes of the data type being stored
	 * @return
	 */
	int lastCompleteIndex(int bytes) {
		return bytes * (Math.floorDiv(_buffer.capacity(), bytes) - 1);
	}
	
	static void failUnalignedAccess() {
		Assert.fail("Unaligned access. Expected IllegalStateException.");
	}
	
	static void failReadOnlyAccess() {
		Assert.fail("Modified read-only buffer. Expected ReadOnlyBufferException.");
	}
	
	static void assertEquals(int expected, int actual) {
		Assert.assertEquals(Integer.toHexString(actual), Integer.toHexString(expected));
	}
	
	static void assertEquals(long expected, long actual) {
		Assert.assertEquals(Long.toHexString(actual), Long.toHexString(expected));
	}
	
	static void assertEquals(float expected, float actual) {
		Assert.assertEquals(Integer.toHexString(Float.floatToRawIntBits(actual)), Integer.toHexString(Float.floatToRawIntBits(expected)));
	}
	
	static void assertEquals(double expected, double actual) {
		Assert.assertEquals(Long.toHexString(Double.doubleToRawLongBits(actual)), Long.toHexString(Double.doubleToRawLongBits(expected)));
	}
}
