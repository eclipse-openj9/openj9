/*******************************************************************************
 * Copyright (c) 2021, 2022 IBM Corp. and others
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
package org.openj9.test.lworld;


import jdk.internal.misc.Unsafe;
import java.lang.reflect.Field;
import java.lang.management.ManagementFactory;
import java.lang.management.RuntimeMXBean;
import java.util.List;
import static org.testng.Assert.*;
import org.testng.annotations.Test;
import org.testng.annotations.BeforeMethod;
import org.testng.annotations.BeforeClass;
import static org.openj9.test.lworld.ValueTypeUnsafeTestClasses.*;


@Test(groups = { "level.sanity" })
public class ValueTypeUnsafeTests {
	static Unsafe myUnsafe = Unsafe.getUnsafe();
	static boolean isCompressedRefsEnabled = false;
	static boolean isFlatteningEnabled = false;
	static boolean isArrayFlatteningEnabled = false;

	static ValueTypePoint2D vtPoint;
	static ValueTypePoint2D[] vtPointAry;
	static long vtPointOffsetX;
	static long vtPointOffsetY;
	static long vtLongPointOffsetX;
	static long vtLongPointOffsetY;
	static long vtPointAryOffset0;
	static long vtPointAryOffset1;
	static long intWrapperOffsetI;

	@BeforeClass
	static public void testSetUp() throws Throwable {
		List<String> arguments = ManagementFactory.getRuntimeMXBean().getInputArguments();
		isFlatteningEnabled = arguments.contains("-XX:ValueTypeFlatteningThreshold=99999");
		isArrayFlatteningEnabled = arguments.contains("-XX:+EnableArrayFlattening");
		isCompressedRefsEnabled = arguments.contains("-Xcompressedrefs");

		vtPointOffsetX = myUnsafe.objectFieldOffset(ValueTypePoint2D.class.getDeclaredField("x"));
		vtPointOffsetY = myUnsafe.objectFieldOffset(ValueTypePoint2D.class.getDeclaredField("y"));
		vtLongPointOffsetX = myUnsafe.objectFieldOffset(ValueTypeLongPoint2D.class.getDeclaredField("x"));
		vtLongPointOffsetY = myUnsafe.objectFieldOffset(ValueTypeLongPoint2D.class.getDeclaredField("y"));
		vtPointAryOffset0 = myUnsafe.arrayBaseOffset(ValueTypePoint2D[].class);
		intWrapperOffsetI = myUnsafe.objectFieldOffset(IntWrapper.class.getDeclaredField("i"));
	}

	// array must not be empty
	static private <T> long arrayElementSize(T[] array) {
		// don't simply use getObjectSize(array[0]) - valueHeaderSize(array[0].getClass()) because the amount of memory used to store an object may be different depending on if it is inside an array or not (for instance, when flattening is disabled)
		return (myUnsafe.getObjectSize(array) - myUnsafe.arrayBaseOffset(array.getClass())) / array.length;
	}

	@BeforeMethod
	static public void setUp() {
		vtPoint = new ValueTypePoint2D(new ValueTypeInt(7), new ValueTypeInt(8));
		vtPointAry = new ValueTypePoint2D[] {
			new ValueTypePoint2D(new ValueTypeInt(5), new ValueTypeInt(10)),
			new ValueTypePoint2D(new ValueTypeInt(10), new ValueTypeInt(20))
		};
		vtPointAryOffset1 = vtPointAryOffset0 + arrayElementSize(vtPointAry);
	}

	@Test
	static public void testFlattenedFieldIsFlattened() throws Throwable {
		boolean isFlattened = myUnsafe.isFlattened(vtPoint.getClass().getDeclaredField("x"));
		assertEquals(isFlattened, isFlatteningEnabled);
	}

	@Test
	static public void testFlattenedArrayIsFlattened() throws Throwable {
		boolean isArrayFlattened = myUnsafe.isFlattenedArray(vtPointAry.getClass());
		assertEquals(isArrayFlattened, isArrayFlatteningEnabled);
	}

	@Test
	static public void testRegularArrayIsNotFlattened() throws Throwable {
		Point2D[] ary = {};
		assertTrue(ary.getClass().isArray());
		boolean isArrayFlattened = myUnsafe.isFlattenedArray(ary.getClass());
		assertFalse(isArrayFlattened);
	}

	@Test
	static public void testRegularFieldIsNotFlattened() throws Throwable {
		Point2D p = new Point2D(1, 1);
		boolean isFlattened = myUnsafe.isFlattened(p.getClass().getDeclaredField("x"));
		assertFalse(isFlattened);
	}

	@Test
	static public void testFlattenedObjectIsNotFlattenedArray() throws Throwable {
		boolean isArrayFlattened = myUnsafe.isFlattenedArray(vtPoint.getClass());
		assertFalse(isArrayFlattened);
	}

	@Test
	static public void testNullIsNotFlattenedArray() throws Throwable {
		boolean isArrayFlattened = myUnsafe.isFlattenedArray(null);
		assertFalse(isArrayFlattened);
	}

	@Test
	static public void testPassingNullToIsFlattenedThrowsException() throws Throwable {
		assertThrows(NullPointerException.class, () -> {
			myUnsafe.isFlattened(null);
		});
	}

	@Test
	static public void testValueHeaderSizeOfValueType() throws Throwable {
		long headerSize = myUnsafe.valueHeaderSize(ValueTypePoint2D.class);
		if (isCompressedRefsEnabled) {
			assertEquals(headerSize, 4);
		} else {
			assertEquals(headerSize, 8);
		}
	}

	@Test
	static public void testValueHeaderSizeOfClassWithLongField() throws Throwable {
		long headerSize = myUnsafe.valueHeaderSize(ValueTypeWithLongField.class);
		if (isCompressedRefsEnabled && !isFlatteningEnabled) {
			assertEquals(headerSize, 4);
		} else {
			// In compressed refs mode, when the class has 8 byte field(s) only, 4 bytes of padding are added to the class. These padding bytes are included in the headerSize.
			assertEquals(headerSize, 8);
		}
	}

	@Test
	static public void testValueHeaderSizeIsDifferenceOfGetObjectSizeAndDataSize() {
		long dataSize = isCompressedRefsEnabled ? 8 : 16; // when compressed refs are enabled, references and flattened ints will take up 4 bytes of space each. When compressed refs are not enabled they will each be 8 bytes long.
		long headerSize = isCompressedRefsEnabled ? 4 : 8; // just like data size, the header size will also depend on whether compressed refs are enabled
		assertEquals(myUnsafe.valueHeaderSize(vtPoint.getClass()), headerSize);
		assertEquals(myUnsafe.valueHeaderSize(vtPoint.getClass()), myUnsafe.getObjectSize(vtPoint) - dataSize);
	}

	@Test
	static public void testValueHeaderSizeOfNonValueType() throws Throwable {
		assertEquals(myUnsafe.valueHeaderSize(Point2D.class), 0);
	}

	@Test
	static public void testValueHeaderSizeOfNull() throws Throwable {
		assertEquals(myUnsafe.valueHeaderSize(null), 0);
	}

	@Test
	static public void testValueHeaderSizeOfVTArray() throws Throwable {
		ValueTypeInt[] vtAry = {};
		long size = myUnsafe.valueHeaderSize(vtAry.getClass());
		assertEquals(size, 0);
	}

	@Test
	static public void testValueHeaderSizeOfArray() throws Throwable {
		Point2D[] ary = {};
		long size = myUnsafe.valueHeaderSize(ary.getClass());
		assertEquals(size, 0);
	}

	@Test
	static public void testUninitializedDefaultValueOfValueType() throws Throwable {
		// create a new ValueTypePoint2D to ensure that the class has been initialized befoere myUnsafe.uninitializedDefaultValue is called
		ValueTypePoint2D newPoint = new ValueTypePoint2D(new ValueTypeInt(1), new ValueTypeInt(1));
		ValueTypePoint2D p = myUnsafe.uninitializedDefaultValue(newPoint.getClass());
		assertEquals(p.x.i, 0);
		assertEquals(p.y.i, 0);
	}

	@Test
	static public void testUninitializedDefaultValueOfNonValueType() throws Throwable {
		Point2D p = myUnsafe.uninitializedDefaultValue(Point2D.class);
		assertNull(p);
	}

	@Test
	static public void testUninitializedDefaultValueOfNull() throws Throwable {
		Object o = myUnsafe.uninitializedDefaultValue(null);
		assertNull(o);
	}

	@Test
	static public void testUninitializedDefaultValueOfVTArray() throws Throwable {
		ValueTypeInt[] vtAry = {};
		Object o = myUnsafe.uninitializedDefaultValue(vtAry.getClass());
		assertNull(o);
	}

	@Test
	static public void testUninitializedDefaultValueOfArray() throws Throwable {
		Point2D[] ary = {};
		Object o = myUnsafe.uninitializedDefaultValue(ary.getClass());
		assertNull(o);
	}

	@Test
	static public void testuninitializedVTClassHasNullDefaultValue() {
		primitive class NeverInitialized {
			final ValueTypeInt i;
			NeverInitialized(ValueTypeInt i) { this.i = i; }
		}
		assertNull(myUnsafe.uninitializedDefaultValue(NeverInitialized.class));
	}

	@Test
	static public void testNullObjGetValue() throws Throwable {
		assertThrows(NullPointerException.class, () -> {
			myUnsafe.getValue(null, 0, ValueTypeInt.class);
		});
	}

	@Test
	static public void testNullClzGetValue() throws Throwable {
		assertThrows(NullPointerException.class, () -> {
			myUnsafe.getValue(vtPoint, vtPointOffsetX, null);
		});
	}

	@Test
	static public void testNonVTClzGetValue() throws Throwable {
		assertNull(myUnsafe.getValue(vtPoint, vtPointOffsetX, IntWrapper.class));
	}

	@Test
	static public void testGetValuesOfArray() throws Throwable {
		if (isFlatteningEnabled) {
			ValueTypePoint2D p = myUnsafe.getValue(vtPointAry, vtPointAryOffset0, ValueTypePoint2D.class);
			assertEquals(p.x.i, vtPointAry[0].x.i);
			assertEquals(p.y.i, vtPointAry[0].y.i);
			p = myUnsafe.getValue(vtPointAry, vtPointAryOffset1, ValueTypePoint2D.class);
			assertEquals(p.x.i, vtPointAry[1].x.i);
			assertEquals(p.y.i, vtPointAry[1].y.i);
		} else {
			ValueTypeInt[] vtIntAry = new ValueTypeInt[] { new ValueTypeInt(1), new ValueTypeInt(2) };
			long vtIntAryOffset0 = myUnsafe.arrayBaseOffset(vtIntAry.getClass());
			long vtIntAryOffset1 = vtIntAryOffset0 + arrayElementSize(vtIntAry);
			ValueTypeInt vti = myUnsafe.getValue(vtIntAry, vtIntAryOffset0, ValueTypeInt.class);
			assertEquals(vti.i, myUnsafe.getInt(vtIntAry, vtIntAryOffset0));
			vti = myUnsafe.getValue(vtIntAry, vtIntAryOffset1, ValueTypeInt.class);
			assertEquals(vti.i, myUnsafe.getInt(vtIntAry, vtIntAryOffset1));
		}
	}

	@Test
	static public void testGetValueOfZeroSizeVTArrayDoesNotCauseError() throws Throwable {
		ZeroSizeValueType[] zsvtAry = new ZeroSizeValueType[] {
			new ZeroSizeValueType(),
			new ZeroSizeValueType()
		};
		long zsvtAryOffset0 = myUnsafe.arrayBaseOffset(zsvtAry.getClass());
		assertNotNull(myUnsafe.getValue(zsvtAry, zsvtAryOffset0, ZeroSizeValueType.class));
	}

	@Test
	static public void testGetValueOfZeroSizeVTObjectDoesNotCauseError() throws Throwable {
		ZeroSizeValueTypeWrapper zsvtw = new ZeroSizeValueTypeWrapper();
		long zsvtwOffset0 = myUnsafe.objectFieldOffset(ZeroSizeValueTypeWrapper.class.getDeclaredField("z"));
		assertNotNull(myUnsafe.getValue(zsvtw, zsvtwOffset0, ZeroSizeValueType.class));
	}

	@Test
	static public void testGetValuesOfObject() throws Throwable {
		ValueTypeInt x = myUnsafe.getValue(vtPoint, vtPointOffsetX, ValueTypeInt.class);
		ValueTypeInt y = myUnsafe.getValue(vtPoint, vtPointOffsetY, ValueTypeInt.class);
		if (isFlatteningEnabled) {
			assertEquals(x.i, vtPoint.x.i);
			assertEquals(y.i, vtPoint.y.i);
		} else {
			assertEquals(x.i, myUnsafe.getInt(vtPoint, vtPointOffsetX));
			assertEquals(y.i, myUnsafe.getInt(vtPoint, vtPointOffsetY));
		}
	}

	@Test
	static public void testGetValueOnVTWithLongFields() throws Throwable {
		ValueTypeLongPoint2D vtLongPoint = new ValueTypeLongPoint2D(123, 456);
		ValueTypeLong x = myUnsafe.getValue(vtLongPoint, vtLongPointOffsetX, ValueTypeLong.class);
		ValueTypeLong y = myUnsafe.getValue(vtLongPoint, vtLongPointOffsetY, ValueTypeLong.class);

		if (isFlatteningEnabled) {
			assertEquals(x.l, vtLongPoint.x.l);
			assertEquals(y.l, vtLongPoint.y.l);
		} else {
			assertEquals(x.l, myUnsafe.getLong(vtLongPoint, vtLongPointOffsetX));
			assertEquals(y.l, myUnsafe.getLong(vtLongPoint, vtLongPointOffsetY));
		}

		assertEquals(vtLongPoint.x.l, 123);
		assertEquals(vtLongPoint.y.l, 456);
	}

	@Test
	static public void testGetValueOnNonVTObj() throws Throwable {
		IntWrapper i = new IntWrapper(7);
		ValueTypeInt vti = myUnsafe.getValue(i, intWrapperOffsetI, ValueTypeInt.class);
		assertEquals(vti.i, i.i);
		assertEquals(i.i, 7);
	}

	@Test
	static public void testNullObjPutValue() throws Throwable {
		assertThrows(NullPointerException.class, () -> {
			myUnsafe.putValue(null, vtPointOffsetX, ValueTypeInt.class, new ValueTypeInt(1));
		});
	}

	@Test
	static public void testNullClzPutValue() throws Throwable {
		assertThrows(NullPointerException.class, () -> {
			myUnsafe.putValue(vtPoint, vtPointOffsetX, null, new ValueTypeInt(1));
		});
	}

	@Test
	static public void testNullValuePutValue() throws Throwable {
		assertThrows(NullPointerException.class, () -> {
			myUnsafe.putValue(vtPoint, vtPointOffsetX, ValueTypeInt.class, null);
		});
	}

	@Test
	static public void testPutValueWithNonVTclzAndValue() throws Throwable {
		int xBeforeTest = vtPoint.x.i;
		assertNotEquals(xBeforeTest, 10000);
		myUnsafe.putValue(vtPoint, vtPointOffsetX, IntWrapper.class, new IntWrapper(10000));
		assertEquals(vtPoint.x.i, xBeforeTest);
	}

	@Test
	static public void testPutValueWithNonVTClzButVTValue() throws Throwable {
		int xBeforeTest = vtPoint.x.i;
		assertNotEquals(xBeforeTest, 10000);
		myUnsafe.putValue(vtPoint, vtPointOffsetX, IntWrapper.class, new ValueTypeInt(10000));
		assertEquals(vtPoint.x.i, xBeforeTest);
	}

	@Test
	static public void testPutValueWithVTClzButNonVTValue() throws Throwable {
		IntWrapper newVal = new IntWrapper(7392);
		myUnsafe.putValue(vtPoint, vtPointOffsetX, ValueTypeInt.class, newVal);
		int expectedValue = myUnsafe.getInt(newVal, myUnsafe.valueHeaderSize(ValueTypeInt.class));
		if (isFlatteningEnabled) {
			assertEquals(vtPoint.x.i, expectedValue);
		} else {
			assertEquals(myUnsafe.getInt(vtPoint, vtPointOffsetX), expectedValue);
		}
		assertEquals(newVal.i, 7392);
	}

	@Test
	static public void testPutValueWithNonVTObj() throws Throwable {
		IntWrapper i = new IntWrapper(7);
		ValueTypeInt newVal = new ValueTypeInt(5892);
		myUnsafe.putValue(i, intWrapperOffsetI, ValueTypeInt.class, newVal);
		assertEquals(i.i, newVal.i);
		assertEquals(newVal.i, 5892);
	}

	@Test
	static public void testPutValuesOfArray() throws Throwable {
		if (isFlatteningEnabled) {
			ValueTypePoint2D p = new ValueTypePoint2D(new ValueTypeInt(34857), new ValueTypeInt(784382));
			myUnsafe.putValue(vtPointAry, vtPointAryOffset0, ValueTypePoint2D.class, p);
			assertEquals(vtPointAry[0].x.i, p.x.i);
			assertEquals(vtPointAry[0].y.i, p.y.i);
			myUnsafe.putValue(vtPointAry, vtPointAryOffset1, ValueTypePoint2D.class, p);
			assertEquals(vtPointAry[1].x.i, p.x.i);
			assertEquals(vtPointAry[1].y.i, p.y.i);
			assertEquals(p.x.i, 34857);
			assertEquals(p.y.i, 784382);
		} else {
			ValueTypeInt[] vtIntAry = new ValueTypeInt[] { new ValueTypeInt(1), new ValueTypeInt(2) };
			long vtIntAryOffset0 = myUnsafe.arrayBaseOffset(vtIntAry.getClass());
			long vtIntAryOffset1 = vtIntAryOffset0 + arrayElementSize(vtIntAry);
			ValueTypeInt vti = new ValueTypeInt(50);
			myUnsafe.putValue(vtIntAry, vtIntAryOffset0, ValueTypeInt.class, vti);
			assertEquals(myUnsafe.getInt(vtIntAry, vtIntAryOffset0), vti.i);
			myUnsafe.putValue(vtIntAry, vtIntAryOffset1, ValueTypeInt.class, vti);
			assertEquals(myUnsafe.getInt(vtIntAry, vtIntAryOffset1), vti.i);
			assertEquals(vti.i, 50);
		}
	}

	@Test
	static public void testPutValueWithZeroSizeVTArrayDoesNotCauseError() throws Throwable {
		ZeroSizeValueType[] zsvtAry = new ZeroSizeValueType[] {
			new ZeroSizeValueType(),
			new ZeroSizeValueType()
		};
		long zsvtAryOffset0 = myUnsafe.arrayBaseOffset(zsvtAry.getClass());
		myUnsafe.putValue(zsvtAry, zsvtAryOffset0, ZeroSizeValueType.class, new ZeroSizeValueType());
	}

	@Test
	static public void testPutValueOfZeroSizeVTObjectDoesNotCauseError() throws Throwable {
		ZeroSizeValueTypeWrapper zsvtw = new ZeroSizeValueTypeWrapper();
		long zsvtwOffset0 = myUnsafe.objectFieldOffset(ZeroSizeValueTypeWrapper.class.getDeclaredField("z"));
		myUnsafe.putValue(zsvtw, zsvtwOffset0, ZeroSizeValueType.class, new ZeroSizeValueType());
	}

	@Test
	static public void testPutValuesOfObject() throws Throwable {
		ValueTypeInt newI = new ValueTypeInt(47538);
		myUnsafe.putValue(vtPoint, vtPointOffsetX, ValueTypeInt.class, newI);
		myUnsafe.putValue(vtPoint, vtPointOffsetY, ValueTypeInt.class, newI);
		if (isFlatteningEnabled) {
			assertEquals(vtPoint.x.i, newI.i);
			assertEquals(vtPoint.y.i, newI.i);
		} else {
			assertEquals(myUnsafe.getInt(vtPoint, vtPointOffsetX), newI.i);
			assertEquals(myUnsafe.getInt(vtPoint, vtPointOffsetY), newI.i);
		}
		assertEquals(newI.i, 47538);
	}

	@Test
	static public void testPutValueOnVTWithLongFields() throws Throwable {
		ValueTypeLongPoint2D vtLongPoint = new ValueTypeLongPoint2D(123, 456);
		ValueTypeLong newVal = new ValueTypeLong(23427);
		myUnsafe.putValue(vtLongPoint, vtLongPointOffsetX, ValueTypeLong.class, newVal);
		myUnsafe.putValue(vtLongPoint, vtLongPointOffsetY, ValueTypeLong.class, newVal);
		if (isFlatteningEnabled) {
			assertEquals(vtLongPoint.y.l, newVal.l);
			assertEquals(vtLongPoint.x.l, newVal.l);
		} else if (isCompressedRefsEnabled) {
			assertEquals(myUnsafe.getInt(vtLongPoint, vtLongPointOffsetX), newVal.l);
			assertEquals(myUnsafe.getInt(vtLongPoint, vtLongPointOffsetY), newVal.l);
		} else {
			assertEquals(myUnsafe.getLong(vtLongPoint, vtLongPointOffsetX), newVal.l);
			assertEquals(myUnsafe.getLong(vtLongPoint, vtLongPointOffsetY), newVal.l);
		}
		assertEquals(newVal.l, 23427);
	}

	@Test
	static public void testGetSizeOfVT() {
		long size = myUnsafe.getObjectSize(vtPoint);
		if (isCompressedRefsEnabled) {
			assertEquals(size, 12);
		} else {
			assertEquals(size, 24);
		}
	}

	@Test
	static public void testGetSizeOfNonVT() {
		long size = myUnsafe.getObjectSize(new IntWrapper(5));
		if (isCompressedRefsEnabled) {
			assertEquals(size, 16);
		} else {
			assertEquals(size, 24);
		}
	}

	@Test
	static public void testGetSizeOfVTWithLongFields() {
		long size = myUnsafe.getObjectSize(new ValueTypeLongPoint2D(123, 456));
		if (isCompressedRefsEnabled && !isFlatteningEnabled) {
			assertEquals(size, 12);
		} else {
			assertEquals(size, 24);
		}
	}

	@Test
	static public void testGetSizeOfArray() {
		ValueTypeInt[] vtiAry = new ValueTypeInt[] { new ValueTypeInt(1), new ValueTypeInt(2) };
		long size = myUnsafe.getObjectSize(vtiAry);
		if (isCompressedRefsEnabled) {
			assertEquals(size, 24);
		} else {
			assertEquals(size, 40);
		}
	}

	@Test
	static public void testGetSizeOfZeroSizeVT() {
		long size = myUnsafe.getObjectSize(new ZeroSizeValueType());
		if (isCompressedRefsEnabled) {
			assertEquals(size, 4);
		} else {
			assertEquals(size, 8);
		}
	}
}
