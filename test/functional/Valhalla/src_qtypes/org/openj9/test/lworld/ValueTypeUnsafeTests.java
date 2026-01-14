/*
 * Copyright IBM Corp. and others 2021
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package org.openj9.test.lworld;


import jdk.internal.misc.Unsafe;
import jdk.internal.value.ValueClass;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;
import java.lang.management.ManagementFactory;
import java.lang.management.RuntimeMXBean;
import java.util.List;
import static org.testng.Assert.*;
import org.testng.annotations.Test;
import org.testng.annotations.BeforeMethod;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.DataProvider;
import static org.openj9.test.lworld.ValueTypeTestClasses.*;


@Test(groups = { "level.sanity" })
public class ValueTypeUnsafeTests {
	static Unsafe myUnsafe = Unsafe.getUnsafe();
	static boolean isCompressedRefsEnabled = false;
	static boolean isDualHeaderShapeDisabled = false;
	static boolean isGcPolicyGencon = false;
	static boolean isFlatteningEnabled = false;
	static boolean isArrayFlatteningEnabled = false;

	static ValueTypePoint2D vtPoint;
	static ValueTypePoint2D[] vtPointAry;
	static ValueTypeInt[] vtIntAry;
	static long vtPointOffsetX;
	static long vtPointOffsetY;
	static long vtLongPointOffsetX;
	static long vtLongPointOffsetY;
	static long vtPointAryOffset0;
	static long vtPointAryOffset1;
	static long intWrapperOffsetVti;
	static long vtIntAryOffset0;
	static long vtIntAryOffset1;

	@BeforeClass
	static public void testSetUp() throws Throwable {
		List<String> arguments = ManagementFactory.getRuntimeMXBean().getInputArguments();
		isFlatteningEnabled = arguments.contains("-XX:ValueTypeFlatteningThreshold=99999");
		isArrayFlatteningEnabled = arguments.contains("-XX:+EnableArrayFlattening");
		isCompressedRefsEnabled = arguments.contains("-Xcompressedrefs");
		// If dual header shape is disabled array header will have dataAddr field, 8 bytes in size, regardless of GC policy
		isDualHeaderShapeDisabled = arguments.contains("-XXgc:disableIndexableDualHeaderShape");
		// Dual header shape affects Gencon GC only
		isGcPolicyGencon = arguments.contains("-Xgcpolicy:gencon")
			|| (!arguments.contains("-Xgcpolicy:optthruput")
				&& !arguments.contains("-Xgcpolicy:balanced")
				&& !arguments.contains("-Xgcpolicy:metronome")
				&& !arguments.contains("-Xgcpolicy:optavgpause"));

		vtPointOffsetX = myUnsafe.objectFieldOffset(ValueTypePoint2D.class.getDeclaredField("x"));
		vtPointOffsetY = myUnsafe.objectFieldOffset(ValueTypePoint2D.class.getDeclaredField("y"));
		vtLongPointOffsetX = myUnsafe.objectFieldOffset(ValueTypeLongPoint2D.class.getDeclaredField("x"));
		vtLongPointOffsetY = myUnsafe.objectFieldOffset(ValueTypeLongPoint2D.class.getDeclaredField("y"));
		vtPointAryOffset0 = myUnsafe.arrayBaseOffset(ValueTypePoint2D[].class);
		intWrapperOffsetVti = myUnsafe.objectFieldOffset(IntWrapper.class.getDeclaredField("vti"));
		vtIntAryOffset0 = myUnsafe.arrayBaseOffset(ValueTypeInt[].class);
	}

	// array must not be empty
	static private <T> long arrayElementSize(T[] array) {
		// don't simply use getObjectSize(array[0]) - valueHeaderSize(array[0].getClass()) because the amount of memory used to store an object may be different depending on if it is inside an array or not (for instance, when flattening is disabled)
		return (myUnsafe.getObjectSize(array) - myUnsafe.arrayBaseOffset(array.getClass())) / array.length;
	}

	@BeforeMethod
	static public void setUp() {
		vtPoint = new ValueTypePoint2D(new ValueTypeInt(7), new ValueTypeInt(8));

		vtPointAry = null;
		vtPointAry = (ValueTypePoint2D[]) ValueClass.newNullRestrictedAtomicArray(ValueTypePoint2D.class, 2,
				new ValueTypePoint2D(new ValueTypeInt(5), new ValueTypeInt(10)));
		vtPointAry[1] = new ValueTypePoint2D(new ValueTypeInt(10), new ValueTypeInt(20));

		vtPointAryOffset1 = vtPointAryOffset0 + arrayElementSize(vtPointAry);
		vtIntAry = (ValueTypeInt[]) ValueClass.newNullRestrictedAtomicArray(ValueTypeInt.class, 2,
				 new ValueTypeInt(1));
		vtIntAry[1] = new ValueTypeInt(2);
		vtIntAryOffset1 = vtIntAryOffset0 + arrayElementSize(vtIntAry);
	}

	public static class CompareAndDoSomethingFunction {
		private Method method;

		public CompareAndDoSomethingFunction(String methodName) throws NoSuchMethodException {
			this.method = myUnsafe.getClass().getMethod(
				methodName,
				new Class[]{Object.class, long.class, int.class, Class.class, Object.class, Object.class}
			);
		}

		public boolean execute(Object obj, long offset, int layout, Class<?> clz, Object v1, Object v2) throws Throwable {
			Object returned = null;
			try {
				returned = method.invoke(myUnsafe, obj, offset, layout, clz, v1, v2);
			} catch (InvocationTargetException exception) {
				throw exception.getCause();
			}
			boolean result = false;
			if (returned instanceof Boolean) {
				result = (Boolean)returned;
			} else {
				result = (returned == v1);
			}
			return result;
		}

		public String toString() {
			return "CompareAndDoSomethingFunction(" + method.getName() + ")";
		}
	}

	@DataProvider(name = "compareAndDoSomethingFuncs")
	static public Object[][] compareAndDoSomethingFuncs() throws NoSuchMethodException {
		return new Object[][] {
			{new CompareAndDoSomethingFunction("compareAndSetFlatValue")},
			{new CompareAndDoSomethingFunction("compareAndExchangeFlatValue")},
		};
	}

	@Test
	static public void testFlattenedFieldIsFlattened() throws Throwable {
		boolean isFlattened = myUnsafe.isFlatField(vtPoint.getClass().getDeclaredField("x"));
		assertEquals(isFlattened, isFlatteningEnabled);
	}

	@Test
	static public void testFlattenedArrayIsFlattened() throws Throwable {
		boolean isArrayFlattened = myUnsafe.isFlatArray(vtPointAry.getClass());
		assertEquals(isArrayFlattened, isArrayFlatteningEnabled);
	}

	@Test
	static public void testRegularArrayIsNotFlattened() throws Throwable {
		Point2D[] ary = {};
		assertTrue(ary.getClass().isArray());
		boolean isArrayFlattened = myUnsafe.isFlatArray(ary.getClass());
		assertFalse(isArrayFlattened);
	}

	@Test
	static public void testRegularFieldIsNotFlattened() throws Throwable {
		Point2D p = new Point2D(1, 1);
		boolean isFlattened = myUnsafe.isFlatField(p.getClass().getDeclaredField("x"));
		assertFalse(isFlattened);
	}

	@Test
	static public void testUnflattenableFieldInVTIsNotFlattened() throws Throwable {
		ValueClassPoint2D p = new ValueClassPoint2D(new ValueClassInt(1), new ValueClassInt(2));
		boolean isFlattened = myUnsafe.isFlatField(p.getClass().getDeclaredField("x"));
		assertFalse(isFlattened);
	}

	@Test
	static public void testFlattenedObjectIsNotFlattenedArray() throws Throwable {
		boolean isArrayFlattened = myUnsafe.isFlatArray(vtPoint.getClass());
		assertFalse(isArrayFlattened);
	}

	@Test
	static public void testNullIsNotFlattenedArray() throws Throwable {
		boolean isArrayFlattened = myUnsafe.isFlatArray(null);
		assertFalse(isArrayFlattened);
	}

	@Test
	static public void testPassingNullToIsFlattenedThrowsException() throws Throwable {
		assertThrows(NullPointerException.class, () -> {
			myUnsafe.isFlatField(null);
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
		long size = myUnsafe.valueHeaderSize(vtIntAry.getClass());
		assertEquals(size, 0);
	}

	@Test
	static public void testValueHeaderSizeOfArray() throws Throwable {
		Point2D[] ary = {};
		long size = myUnsafe.valueHeaderSize(ary.getClass());
		assertEquals(size, 0);
	}

	@Test
	static public void testNullObjGetValue() throws Throwable {
		if (isFlatteningEnabled) {
			int layout = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("x"));
			assertThrows(NullPointerException.class, () -> {
				myUnsafe.getFlatValue(null, 0, layout, ValueTypeInt.class);
			});
		}
	}

	@Test
	static public void testNullClzGetValue() throws Throwable {
		if (isFlatteningEnabled) {
			int layout = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("x"));
			assertThrows(NullPointerException.class, () -> {
				myUnsafe.getFlatValue(vtPoint, vtPointOffsetX, layout, null);
			});
		}
	}

	@Test
	static public void testNonVTClzGetValue() throws Throwable {
		if (isFlatteningEnabled) {
			int layout = myUnsafe.fieldLayout(IntWrapper.class.getDeclaredField("vti"));
			assertNull(myUnsafe.getFlatValue(vtPoint, intWrapperOffsetVti, layout, IntWrapper.class));
		}
	}

	@Test
	static public void testGetValuesOfArray() throws Throwable {
		if (isFlatteningEnabled) {
			int layout = myUnsafe.arrayLayout(vtPointAry.getClass());
			ValueTypePoint2D p = myUnsafe.getFlatValue(vtPointAry, vtPointAryOffset0, layout, ValueTypePoint2D.class);
			assertEquals(p.x.i, vtPointAry[0].x.i);
			assertEquals(p.y.i, vtPointAry[0].y.i);
			p = myUnsafe.getFlatValue(vtPointAry, vtPointAryOffset1, layout, ValueTypePoint2D.class);
			assertEquals(p.x.i, vtPointAry[1].x.i);
			assertEquals(p.y.i, vtPointAry[1].y.i);
		}
	}

	@Test
	static public void testGetValueOfZeroSizeVTArrayDoesNotCauseError() throws Throwable {
		if (isFlatteningEnabled) {
			ZeroSizeValueType[] zsvtAry = (ZeroSizeValueType[]) ValueClass.newNullRestrictedAtomicArray(ZeroSizeValueType.class,
					2,
					new ZeroSizeValueType());
			long zsvtAryOffset0 = myUnsafe.arrayBaseOffset(zsvtAry.getClass());
			int layout = myUnsafe.arrayLayout(zsvtAry.getClass());
			assertNotNull(myUnsafe.getFlatValue(zsvtAry, zsvtAryOffset0, layout, ZeroSizeValueType.class));
		}
	}

	@Test
	static public void testGetValueOfZeroSizeVTObjectDoesNotCauseError() throws Throwable {
		if (isFlatteningEnabled) {
			ZeroSizeValueTypeWrapper zsvtw = new ZeroSizeValueTypeWrapper();
			long zsvtwOffset0 = myUnsafe.objectFieldOffset(ZeroSizeValueTypeWrapper.class.getDeclaredField("z"));
			int layout = myUnsafe.fieldLayout(zsvtw.getClass().getDeclaredField("z"));
			assertNotNull(myUnsafe.getFlatValue(zsvtw, zsvtwOffset0, layout, ZeroSizeValueType.class));
		}
	}

	@Test
	static public void testGetValuesOfObject() throws Throwable {
		if (isFlatteningEnabled) {
			int layoutX = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("x"));
			int layoutY = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("y"));
			ValueTypeInt x = myUnsafe.getFlatValue(vtPoint, vtPointOffsetX, layoutX, ValueTypeInt.class);
			ValueTypeInt y = myUnsafe.getFlatValue(vtPoint, vtPointOffsetY, layoutY, ValueTypeInt.class);
			assertEquals(x.i, vtPoint.x.i);
			assertEquals(y.i, vtPoint.y.i);
		}
	}

	@Test
	static public void testGetValueOnVTWithLongFields() throws Throwable {
		ValueTypeLongPoint2D vtLongPoint = new ValueTypeLongPoint2D(123, 456);

		if (isFlatteningEnabled) {
			int layoutX = myUnsafe.fieldLayout(vtLongPoint.getClass().getDeclaredField("x"));
			int layoutY = myUnsafe.fieldLayout(vtLongPoint.getClass().getDeclaredField("y"));
			ValueTypeLong x = myUnsafe.getFlatValue(vtLongPoint, vtLongPointOffsetX, layoutX, ValueTypeLong.class);
			ValueTypeLong y = myUnsafe.getFlatValue(vtLongPoint, vtLongPointOffsetY, layoutY, ValueTypeLong.class);
			assertEquals(x.l, vtLongPoint.x.l);
			assertEquals(y.l, vtLongPoint.y.l);
		}

		assertEquals(vtLongPoint.x.l, 123);
		assertEquals(vtLongPoint.y.l, 456);
	}

	@Test
	static public void testGetValueOnNonVTObj() throws Throwable {
		if (isFlatteningEnabled) {
			IntWrapper iw = new IntWrapper(7);
			int layout = myUnsafe.fieldLayout(iw.getClass().getDeclaredField("vti"));
			ValueTypeInt vti = myUnsafe.getFlatValue(iw, intWrapperOffsetVti, layout, ValueTypeInt.class);
			assertEquals(vti.i, iw.vti.i);
			assertEquals(iw.vti.i, 7);
		}
	}

	@Test
	static public void testNullObjPutValue() throws Throwable {
		if (isFlatteningEnabled) {
			int layout = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("x"));
			assertThrows(NullPointerException.class, () -> {
				myUnsafe.putFlatValue(null, vtPointOffsetX, layout, ValueTypeInt.class, new ValueTypeInt(1));
			});
		}
	}

	@Test
	static public void testNullClzPutValue() throws Throwable {
		if (isFlatteningEnabled) {
			int layout = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("x"));
			assertThrows(NullPointerException.class, () -> {
				myUnsafe.putFlatValue(vtPoint, vtPointOffsetX, layout, null, new ValueTypeInt(1));
			});
		}
	}

	@Test
	static public void testNullValuePutValue() throws Throwable {
		if (isFlatteningEnabled) {
			int layout = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("x"));
			assertThrows(NullPointerException.class, () -> {
				myUnsafe.putFlatValue(vtPoint, vtPointOffsetX, layout, ValueTypeInt.class, null);
			});
		}
	}

	@Test
	static public void testPutValueWithNonVTclzAndValue() throws Throwable {
		if (isFlatteningEnabled) {
			int xBeforeTest = vtPoint.x.i;
			assertNotEquals(xBeforeTest, 10000);
			int layout = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("x"));
			myUnsafe.putFlatValue(vtPoint, vtPointOffsetX, layout, IntWrapper.class, new IntWrapper(10000));
			assertEquals(vtPoint.x.i, xBeforeTest);
		}
	}

	@Test
	static public void testPutValueWithNonVTClzButVTValue() throws Throwable {
		if (isFlatteningEnabled) {
			int xBeforeTest = vtPoint.x.i;
			assertNotEquals(xBeforeTest, 10000);
			int layout = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("x"));
			myUnsafe.putFlatValue(vtPoint, vtPointOffsetX, layout, IntWrapper.class, new ValueTypeInt(10000));
			assertEquals(vtPoint.x.i, xBeforeTest);
		}
	}

	@Test
	static public void testPutValueWithNonVTObj() throws Throwable {
		if (isFlatteningEnabled) {
			IntWrapper iw = new IntWrapper(7);
			ValueTypeInt newVal = new ValueTypeInt(5892);
			int layout = myUnsafe.fieldLayout(iw.getClass().getDeclaredField("vti"));
			myUnsafe.putFlatValue(iw, intWrapperOffsetVti, layout, ValueTypeInt.class, newVal);
			assertEquals(iw.vti.i, newVal.i);
			assertEquals(newVal.i, 5892);
		}
	}

	@Test
	static public void testPutValuesOfArray() throws Throwable {
		if (isFlatteningEnabled) {
			ValueTypePoint2D p = new ValueTypePoint2D(new ValueTypeInt(34857), new ValueTypeInt(784382));
			int layout = myUnsafe.arrayLayout(vtPointAry.getClass());
			myUnsafe.putFlatValue(vtPointAry, vtPointAryOffset0, layout, ValueTypePoint2D.class, p);
			assertEquals(vtPointAry[0].x.i, p.x.i);
			assertEquals(vtPointAry[0].y.i, p.y.i);
			myUnsafe.putFlatValue(vtPointAry, vtPointAryOffset1, layout, ValueTypePoint2D.class, p);
			assertEquals(vtPointAry[1].x.i, p.x.i);
			assertEquals(vtPointAry[1].y.i, p.y.i);
			assertEquals(p.x.i, 34857);
			assertEquals(p.y.i, 784382);
		}
	}

	@Test
	static public void testPutValueWithZeroSizeVTArrayDoesNotCauseError() throws Throwable {
		if (isFlatteningEnabled) {
			ZeroSizeValueType[] zsvtAry = (ZeroSizeValueType[]) ValueClass.newNullRestrictedAtomicArray(ZeroSizeValueType.class,
					2,
					new ZeroSizeValueType());
			long zsvtAryOffset0 = myUnsafe.arrayBaseOffset(zsvtAry.getClass());
			int layout = myUnsafe.arrayLayout(zsvtAry.getClass());
			myUnsafe.putFlatValue(zsvtAry, zsvtAryOffset0, layout, ZeroSizeValueType.class, new ZeroSizeValueType());
		}
	}

	@Test
	static public void testPutValueOfZeroSizeVTObjectDoesNotCauseError() throws Throwable {
		if (isFlatteningEnabled) {
			ZeroSizeValueTypeWrapper zsvtw = new ZeroSizeValueTypeWrapper();
			long zsvtwOffset0 = myUnsafe.objectFieldOffset(zsvtw.getClass().getDeclaredField("z"));
			int layout = myUnsafe.fieldLayout(zsvtw.getClass().getDeclaredField("z"));
			myUnsafe.putFlatValue(zsvtw, zsvtwOffset0, layout, ZeroSizeValueType.class, new ZeroSizeValueType());
		}
	}

	@Test
	static public void testPutValuesOfObject() throws Throwable {
		ValueTypeInt newI = new ValueTypeInt(47538);
		if (isFlatteningEnabled) {
			int layoutX = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("x"));
			int layoutY = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("y"));
			myUnsafe.putFlatValue(vtPoint, vtPointOffsetX, layoutX, ValueTypeInt.class, newI);
			myUnsafe.putFlatValue(vtPoint, vtPointOffsetY, layoutY, ValueTypeInt.class, newI);
			assertEquals(vtPoint.x.i, newI.i);
			assertEquals(vtPoint.y.i, newI.i);
		}
		assertEquals(newI.i, 47538);
	}

	@Test
	static public void testPutValueOnVTWithLongFields() throws Throwable {
		ValueTypeLongPoint2D vtLongPoint = new ValueTypeLongPoint2D(123, 456);
		ValueTypeLong newVal = new ValueTypeLong(23427);
		if (isFlatteningEnabled) {
			int layoutX = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("x"));
			int layoutY = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("y"));
			myUnsafe.putFlatValue(vtLongPoint, vtLongPointOffsetX, layoutX, ValueTypeLong.class, newVal);
			myUnsafe.putFlatValue(vtLongPoint, vtLongPointOffsetY, layoutY, ValueTypeLong.class, newVal);
			assertEquals(vtLongPoint.y.l, newVal.l);
			assertEquals(vtLongPoint.x.l, newVal.l);
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
		long size = myUnsafe.getObjectSize(vtIntAry);

		int headerSizeAdjustment = (!isDualHeaderShapeDisabled && isGcPolicyGencon) ? 8 : 0;
		if (isCompressedRefsEnabled) {
			assertEquals(size, 24 - headerSizeAdjustment);
		} else {
			assertEquals(size, 40 - headerSizeAdjustment);
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

	@Test(dataProvider = "compareAndDoSomethingFuncs")
	static public void testCompareAndSetNullObj(CompareAndDoSomethingFunction compareAndSwapValue) throws Throwable {
		if (isFlatteningEnabled) {
			int layout = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("x"));
			assertThrows(NullPointerException.class, () -> {
				compareAndSwapValue.execute(null, vtPointOffsetX, layout, ValueTypeInt.class, vtPoint.x, new ValueTypeInt(1));
			});
		}
	}

	@Test(dataProvider = "compareAndDoSomethingFuncs")
	static public void testCompareAndSetNullClz(CompareAndDoSomethingFunction compareAndSwapValue) throws Throwable {
		if (isFlatteningEnabled) {
			int layout = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("x"));
			assertThrows(NullPointerException.class, () -> {
				compareAndSwapValue.execute(vtPoint, vtPointOffsetX, layout, null, vtPoint.x, new ValueTypeInt(1));
			});
		}
	}

	@Test(dataProvider = "compareAndDoSomethingFuncs")
	static public void testCompareAndSetNullV1(CompareAndDoSomethingFunction compareAndSwapValue) throws Throwable {
		if (isFlatteningEnabled) {
			int original = vtPoint.x.i;
			int layout = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("x"));
			boolean success = compareAndSwapValue.execute(vtPoint, vtPointOffsetX, layout, ValueTypeInt.class, null, new ValueTypeInt(1));
			assertFalse(success);
			assertEquals(vtPoint.x.i, original);
		}
	}

	@Test(dataProvider = "compareAndDoSomethingFuncs")
	static public void testCompareAndSetNullV2(CompareAndDoSomethingFunction compareAndSwapValue) throws Throwable {
		if (isFlatteningEnabled) {
			int layout = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("x"));
			assertThrows(NullPointerException.class, () -> {
				compareAndSwapValue.execute(vtPoint, vtPointOffsetX, layout, ValueTypeInt.class, vtPoint.x, null);
			});
		}
	}

	@Test(dataProvider = "compareAndDoSomethingFuncs")
	static public void testCompareAndSetPointXSuccess(CompareAndDoSomethingFunction compareAndSwapValue) throws Throwable {
		if (isFlatteningEnabled) {
			int original = vtPoint.x.i;
			int layout = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("x"));
			ValueTypeInt newVti = new ValueTypeInt(328);
			boolean success = compareAndSwapValue.execute(vtPoint, vtPointOffsetX, layout, ValueTypeInt.class, vtPoint.x, newVti);
			assertEquals(newVti.i, 328);
			assertTrue(success);
			assertEquals(vtPoint.x.i, newVti.i);
		}
	}

	@Test(dataProvider = "compareAndDoSomethingFuncs")
	static public void testCompareAndSetPointYSuccess(CompareAndDoSomethingFunction compareAndSwapValue) throws Throwable {
		if (isFlatteningEnabled) {
			int original = vtPoint.y.i;
			int layout = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("y"));
			ValueTypeInt newVti = new ValueTypeInt(328);
			boolean success = compareAndSwapValue.execute(vtPoint, vtPointOffsetY, layout, ValueTypeInt.class, vtPoint.y, newVti);
			assertEquals(newVti.i, 328);
			assertTrue(success);
			assertEquals(vtPoint.y.i, newVti.i);
		}
	}

	@Test(dataProvider = "compareAndDoSomethingFuncs")
	static public void testCompareAndSetPointXFailure(CompareAndDoSomethingFunction compareAndSwapValue) throws Throwable {
		if (isFlatteningEnabled) {
			int original = vtPoint.x.i;
			int layout = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("x"));
			ValueTypeInt newVti = new ValueTypeInt(328);
			boolean success = compareAndSwapValue.execute(vtPoint, vtPointOffsetX, layout, ValueTypeInt.class, new ValueTypeInt(original + 1), newVti);
			assertEquals(newVti.i, 328);
			assertFalse(success);
			assertEquals(vtPoint.x.i, original);
		}
	}

	@Test(dataProvider = "compareAndDoSomethingFuncs")
	static public void testCompareAndSetPointYFailure(CompareAndDoSomethingFunction compareAndSwapValue) throws Throwable {
		if (isFlatteningEnabled) {
			int original = vtPoint.y.i;
			int layout = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("y"));
			ValueTypeInt newVti = new ValueTypeInt(328);
			boolean success = compareAndSwapValue.execute(vtPoint, vtPointOffsetY, layout, ValueTypeInt.class, new ValueTypeInt(original + 1), newVti);
			assertEquals(newVti.i, 328);
			assertFalse(success);
			assertEquals(vtPoint.y.i, original);
		}
	}

	@Test(dataProvider = "compareAndDoSomethingFuncs")
	static public void testCompareAndSetArray0Success(CompareAndDoSomethingFunction compareAndSwapValue) throws Throwable {
		if (isFlatteningEnabled) {
			int original = vtIntAry[0].i;
			ValueTypeInt newVti = new ValueTypeInt(456);
			int layout = myUnsafe.arrayLayout(vtIntAry.getClass());
			boolean success = compareAndSwapValue.execute(vtIntAry, vtIntAryOffset0, layout, ValueTypeInt.class, vtIntAry[0], newVti);
			assertEquals(newVti.i, 456);
			assertTrue(success);
			assertEquals(vtIntAry[0].i, newVti.i);
		}
	}

	@Test(dataProvider = "compareAndDoSomethingFuncs")
	static public void testCompareAndSetArray1Success(CompareAndDoSomethingFunction compareAndSwapValue) throws Throwable {
		if (isFlatteningEnabled) {
			int original = vtIntAry[1].i;
			ValueTypeInt newVti = new ValueTypeInt(456);
			int layout = myUnsafe.arrayLayout(vtIntAry.getClass());
			boolean success = compareAndSwapValue.execute(vtIntAry, vtIntAryOffset1, layout, ValueTypeInt.class, vtIntAry[1], newVti);
			assertEquals(newVti.i, 456);
			assertTrue(success);
			assertEquals(vtIntAry[1].i, newVti.i);
		}
	}

	@Test(dataProvider = "compareAndDoSomethingFuncs")
	static public void testCompareAndSetArray0Failure(CompareAndDoSomethingFunction compareAndSwapValue) throws Throwable {
		if (isFlatteningEnabled) {
			int original = vtIntAry[0].i;
			ValueTypeInt newVti = new ValueTypeInt(328);
			int layout = myUnsafe.arrayLayout(vtIntAry.getClass());
			boolean success = compareAndSwapValue.execute(vtIntAry, vtIntAryOffset0, layout, ValueTypeInt.class, new ValueTypeInt(original + 1), newVti);
			assertEquals(newVti.i, 328);
			assertFalse(success);
			assertEquals(vtIntAry[0].i, original);
		}
	}

	@Test(dataProvider = "compareAndDoSomethingFuncs")
	static public void testCompareAndSetArray1Failure(CompareAndDoSomethingFunction compareAndSwapValue) throws Throwable {
		if (isFlatteningEnabled) {
			int original = vtIntAry[1].i;
			ValueTypeInt newVti = new ValueTypeInt(328);
			int layout = myUnsafe.arrayLayout(vtIntAry.getClass());
			boolean success = compareAndSwapValue.execute(vtIntAry, vtIntAryOffset1, layout, ValueTypeInt.class, new ValueTypeInt(original + 1), newVti);
			assertEquals(newVti.i, 328);
			assertFalse(success);
			assertEquals(vtIntAry[1].i, original);
		}
	}

	@Test(dataProvider = "compareAndDoSomethingFuncs")
	static public void testCompareAndSetOnVTWithLongFields(CompareAndDoSomethingFunction compareAndSwapValue) throws Throwable {
		if (isFlatteningEnabled) {
			ValueTypeLongPoint2D vtLongPoint = new ValueTypeLongPoint2D(123, 456);
			assertEquals(vtLongPoint.x.l, 123);
			assertEquals(vtLongPoint.y.l, 456);

			long original = vtLongPoint.x.l;
			ValueTypeLong newVtl = new ValueTypeLong(372);

			int layout = myUnsafe.fieldLayout(vtLongPoint.getClass().getDeclaredField("x"));

			boolean success = compareAndSwapValue.execute(vtLongPoint, vtLongPointOffsetX, layout, ValueTypeLong.class, vtLongPoint.x, newVtl);
			assertEquals(newVtl.l, 372);
			assertTrue(success);
			assertEquals(vtLongPoint.x.l, newVtl.l);
		}
	}

	@Test(dataProvider = "compareAndDoSomethingFuncs")
	static public void testCompareAndSetWrongClz(CompareAndDoSomethingFunction compareAndSwapValue) throws Throwable {
		int original = vtPoint.x.i;
		ValueTypeInt newVti = new ValueTypeInt(328);
		assertEquals(newVti.i, 328);
		if (isFlatteningEnabled) {
			int layout = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("x"));
			boolean success = compareAndSwapValue.execute(vtPoint, vtPointOffsetX, layout, ValueTypeInt2.class, vtPoint.x, newVti);
			assertFalse(success);
			assertEquals(vtPoint.x.i, original);
		}
	}

	@Test
	static public void testGetAndSetNullObj() throws Throwable {
		assertThrows(NullPointerException.class, () -> {
			int layout = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("x"));
			myUnsafe.getAndSetFlatValue(null, vtPointOffsetX, layout, ValueTypeInt.class, new ValueTypeInt(1));
		});
	}

	@Test
	static public void testGetAndSetNullClz() throws Throwable {
		if (isFlatteningEnabled) {
			int layout = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("x"));
			assertThrows(NullPointerException.class, () -> {
				myUnsafe.getAndSetFlatValue(vtPoint, vtPointOffsetX, layout, null, new ValueTypeInt(1));
			});
		}
	}

	@Test
	static public void testGetAndSetNullV() throws Throwable {
		int original = vtPoint.x.i;
		if (isFlatteningEnabled) {
			int layout = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("x"));
			assertThrows(NullPointerException.class, () -> {
				myUnsafe.getAndSetFlatValue(vtPoint, vtPointOffsetX, layout, ValueTypeInt.class, null);
			});
		}
	}

	@Test
	static public void testGetAndSetPointX() throws Throwable {
		if (isFlatteningEnabled) {
			int layout = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("x"));
			int original = vtPoint.x.i;
			ValueTypeInt newVti = new ValueTypeInt(328);
			Object result = myUnsafe.getAndSetFlatValue(vtPoint, vtPointOffsetX, layout, ValueTypeInt.class, newVti);
			assertEquals(newVti.i, 328);
			assertEquals(((ValueTypeInt)result).i, original);
			assertEquals(vtPoint.x.i, newVti.i);
		}
	}

	@Test
	static public void testGetAndSetPointY() throws Throwable {
		if (isFlatteningEnabled) {
			int layout = myUnsafe.fieldLayout(vtPoint.getClass().getDeclaredField("y"));
			int original = vtPoint.y.i;
			ValueTypeInt newVti = new ValueTypeInt(328);
			Object result = myUnsafe.getAndSetFlatValue(vtPoint, vtPointOffsetY, layout, ValueTypeInt.class, newVti);
			assertEquals(newVti.i, 328);
			assertEquals(((ValueTypeInt)result).i, original);
			assertEquals(vtPoint.y.i, newVti.i);
		}
	}

	@Test
	static public void testGetAndSetArray0() throws Throwable {
		if (isFlatteningEnabled) {
			int layout = myUnsafe.arrayLayout(vtIntAry.getClass());
			int original = vtIntAry[0].i;
			ValueTypeInt newVti = new ValueTypeInt(456);
			Object result = myUnsafe.getAndSetFlatValue(vtIntAry, vtIntAryOffset0, layout, ValueTypeInt.class, newVti);
			assertEquals(newVti.i, 456);
			assertEquals(((ValueTypeInt)result).i, original);
			assertEquals(vtIntAry[0].i, newVti.i);
		}
	}

	@Test
	static public void testGetAndSetArray1() throws Throwable {
		if (isFlatteningEnabled) {
			int layout = myUnsafe.arrayLayout(vtIntAry.getClass());
			int original = vtIntAry[1].i;
			ValueTypeInt newVti = new ValueTypeInt(456);
			Object result = myUnsafe.getAndSetFlatValue(vtIntAry, vtIntAryOffset1, layout, ValueTypeInt.class, newVti);
			assertEquals(newVti.i, 456);
			assertEquals(((ValueTypeInt)result).i, original);
			assertEquals(vtIntAry[1].i, newVti.i);
		}
	}

	@Test
	static public void testGetAndSetOnVTWithLongFields() throws Throwable {
		ValueTypeLongPoint2D vtLongPoint = new ValueTypeLongPoint2D(123, 456);
		assertEquals(vtLongPoint.x.l, 123);
		assertEquals(vtLongPoint.y.l, 456);

		if (isFlatteningEnabled) {
			int layout = myUnsafe.fieldLayout(vtLongPoint.getClass().getDeclaredField("x"));
			long original = vtLongPoint.x.l;
			ValueTypeLong newVtl = new ValueTypeLong(372);
			Object result = myUnsafe.getAndSetFlatValue(vtLongPoint, vtLongPointOffsetX, layout, ValueTypeLong.class, newVtl);
			assertEquals(newVtl.l, 372);
			assertEquals(((ValueTypeLong)result).l, original);
			assertEquals(vtLongPoint.x.l, newVtl.l);
		}
	}
}
