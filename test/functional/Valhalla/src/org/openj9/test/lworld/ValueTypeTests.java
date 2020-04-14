/*******************************************************************************
 * Copyright (c) 2018, 2020 IBM Corp. and others
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

import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
import java.lang.reflect.Method;
import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.util.ArrayList;
import sun.misc.Unsafe;
import org.testng.Assert;
import static org.testng.Assert.*;
import org.testng.annotations.Test;
import org.testng.annotations.BeforeClass;

/*
 * Instructions to run this test:
 * 
 * 1)  Build the JDK with the '--enable-inline-types' configure flag
 * 2)  cd [openj9-openjdk-dir]/openj9/test
 * 3)  git clone https://github.com/AdoptOpenJDK/TKG.git
 * 4)  cd TKG
 * 5)  export TEST_JDK_HOME=[openj9-openjdk-dir]/build/linux-x86_64-server-release/images/jdk
 * 6)  export JDK_VERSION=Valhalla
 * 7)  export SPEC=linux_x86-64_cmprssptrs
 * 8)  export BUILD_LIST=functional/Valhalla
 * 9)  export AUTO_DETECT=false
 * 10) export JDK_IMPL=openj9 
 * 11) make -f run_configure.mk && make compile && make _sanity
 */

@Test(groups = { "level.sanity" })
public class ValueTypeTests {
	static Lookup lookup = MethodHandles.lookup();
	static Unsafe myUnsafe = null;
	/* point2D */
	static Class point2DClass = null;
	static MethodHandle makePoint2D = null;
	static MethodHandle getX = null;
	static MethodHandle getY = null;
	static MethodHandle withX = null;
	static MethodHandle withY = null;
	/* line2D */
	static Class line2DClass = null;
	static MethodHandle makeLine2D = null;
	/* flattenedLine2D */
	static Class flattenedLine2DClass = null;
	static MethodHandle makeFlattenedLine2D = null;
	static MethodHandle getFlatSt = null;
	static MethodHandle withFlatSt = null;
 	static MethodHandle getFlatEn = null;
 	static MethodHandle withFlatEn = null;
	/* triangle2D */
	static Class triangle2DClass = null;
	static MethodHandle makeTriangle2D = null;
	static MethodHandle getV1 = null;
	static MethodHandle getV2 = null;
	static MethodHandle getV3 = null;
	/* valueLong */
	static Class valueLongClass = null;
	static MethodHandle makeValueLong = null;
	static MethodHandle getLong = null;
	static MethodHandle withLong = null;
	/* valueInt */
	static Class valueIntClass = null;
	static MethodHandle makeValueInt = null;
	static MethodHandle getInt = null;
	static MethodHandle withInt = null;
	/* valueDouble */
	static Class valueDoubleClass = null;
	static MethodHandle makeValueDouble = null;
	static MethodHandle getDouble = null;
	static MethodHandle withDouble = null;
	/* valueFloat */
	static Class valueFloatClass = null;
	static MethodHandle makeValueFloat = null;
	static MethodHandle getFloat = null;
	static MethodHandle withFloat = null;
	/* valueObject */
	static Class valueObjectClass = null;
	static MethodHandle makeValueObject = null;
	static MethodHandle getObject = null;
	static MethodHandle withObject = null;
	/* largeObject */
	static Class largeObjectValueClass = null;
	static MethodHandle makeLargeObjectValue = null;
	static MethodHandle[] getObjects = null;
	/* megaObject */
	static Class megaObjectValueClass = null;
	static MethodHandle makeMegaObjectValue = null;
	/* assortedRefWithLongAlignment */
	static Class assortedRefWithLongAlignmentClass = null;
	static MethodHandle makeAssortedRefWithLongAlignment = null;
	static MethodHandle[][] assortedRefWithLongAlignmentGetterAndSetter = null;
	/* assortedValueWithLongAlignment */
	static Class assortedValueWithLongAlignmentClass = null;
	static MethodHandle makeAssortedValueWithLongAlignment = null;
	static MethodHandle[][] assortedValueWithLongAlignmentGetterAndWither = null;
	/* assortedValueWithObjectAlignment */
	static Class assortedValueWithObjectAlignmentClass = null;
	static MethodHandle makeAssortedValueWithObjectAlignment = null;
	static MethodHandle[][] assortedValueWithObjectAlignmentGetterAndWither = null;
	/* assortedValueWithSingleAlignment */
	static Class assortedValueWithSingleAlignmentClass = null;
	static MethodHandle makeAssortedValueWithSingleAlignment = null;
	static MethodHandle[][] assortedValueWithSingleAlignmentGetterAndWither = null;
	/* fields */
	static String typeWithSingleAlignmentFields[] = {
		"tri:QTriangle2D;:value",
		"point:QPoint2D;:value",
		"line:QFlattenedLine2D;:value",
		"i:QValueInt;:value",
		"f:QValueFloat;:value",
		"tri2:QTriangle2D;:value"
	};
	static String typeWithObjectAlignmentFields[] = {
		"tri:QTriangle2D;:value",
		"point:QPoint2D;:value",
		"line:QFlattenedLine2D;:value",
		"o:QValueObject;:value",
		"i:QValueInt;:value",
		"f:QValueFloat;:value",
		"tri2:QTriangle2D;:value"
	};
	static String typeWithLongAlignmentFields[] = {
		"point:QPoint2D;:value",
		"line:QFlattenedLine2D;:value",
		"o:QValueObject;:value",
		"l:QValueLong;:value",
		"d:QValueDouble;:value",
		"i:QValueInt;:value",
		"tri:QTriangle2D;:value"
	};
	
	/* default values */
	static int[] defaultPointPositions1 = {0xFFEEFFEE, 0xAABBAABB};
	static int[] defaultPointPositions2 = {0xCCDDCCDD, 0x33443344};
	static int[] defaultPointPositions3 = {0x43211234, 0xABCDDCBA};
	static int[] defaultPointPositionsEmpty = {0, 0};
	static int[][] defaultLinePositions1 = {defaultPointPositions1, defaultPointPositions2};
	static int[][] defaultLinePositions2 = {defaultPointPositions2, defaultPointPositions3};
	static int[][] defaultLinePositions3 = {defaultPointPositions1, defaultPointPositions3};
	static int[][] defaultLinePositionsEmpty = {defaultPointPositionsEmpty, defaultPointPositionsEmpty};
	static int[][][] defaultTrianglePositions = {defaultLinePositions1, defaultLinePositions2, defaultLinePositions3};
	static long defaultLong = Long.MAX_VALUE;
	static int defaultInt = Integer.MAX_VALUE;
	static double defaultDouble = Double.MAX_VALUE;
	static float defaultFloat = Float.MAX_VALUE;
	static Object defaultObject = (Object)0xEEFFEEFF;
	static int[] defaultPointPositionsNew = {0xFF112233, 0xFF332211};
	static int[][] defaultLinePositionsNew = {defaultPointPositionsNew, defaultPointPositions1};
	static int[][][] defaultTrianglePositionsNew = {defaultLinePositionsNew, defaultLinePositions3, defaultLinePositions1};
	static int[][][] defaultTrianglePositionsEmpty = {defaultLinePositionsEmpty, defaultLinePositionsEmpty, defaultLinePositionsEmpty};
	static long defaultLongNew = -1234123L;
	static int defaultIntNew = -1234123234;
	static double defaultDoubleNew = -123412341.21341234d;
	static float defaultFloatNew = -123423.12341234f;
	static Object defaultObjectNew = (Object)0xFFEEFFEE;
	/* miscellaneous constants */
	static final int genericArraySize = 10;
	static final int objectGCScanningIterationCount = 10000;

	@BeforeClass
	static public void testSetUp() throws RuntimeException {
		try {
			Field f = Unsafe.class.getDeclaredField("theUnsafe");
			f.setAccessible(true);
			myUnsafe = (Unsafe)f.get(null);
		} catch (NoSuchFieldException | SecurityException | IllegalArgumentException | IllegalAccessException e) {
			throw new RuntimeException(e);
		}	
	}

	/*
	 * Create a value type
	 * 
	 * value Point2D {
	 * 	int x;
	 * 	int y;
	 * }
	 */
	@Test(priority=1)
	static public void testCreatePoint2D() throws Throwable {
		String fields[] = {"x:I", "y:I"};
		point2DClass = ValueTypeGenerator.generateValueClass("Point2D", fields);
		
		makePoint2D = lookup.findStatic(point2DClass, "makeValue", MethodType.methodType(point2DClass, int.class, int.class));
		
		getX = generateGetter(point2DClass, "x", int.class);
		withX = generateWither(point2DClass, "x", int.class);
		getY = generateGetter(point2DClass, "y", int.class);
		withY = generateWither(point2DClass, "y", int.class);

		int x = 0xFFEEFFEE;
		int y = 0xAABBAABB;
		int xNew = 0x11223344;
		int yNew = 0x99887766;
		
		Object point2D = makePoint2D.invoke(x, y);
		
		assertEquals(getX.invoke(point2D), x);
		assertEquals(getY.invoke(point2D), y);
		
		point2D = withX.invoke(point2D, xNew);
		point2D = withY.invoke(point2D, yNew);
		
		assertEquals(getX.invoke(point2D), xNew);
		assertEquals(getY.invoke(point2D), yNew);
	}

	/*
	 * Test array of Point2D
	 */
	@Test(priority=2)
	static public void testCreateArrayPoint2D() throws Throwable {
		int x1 = 0xFFEEFFEE;
		int y1 = 0xAABBAABB;
		int x2 = 0x00000011;
		int y2 = 0xABCDEF00;
		
		Object point2D_1 = makePoint2D.invoke(x1, y1);
		Object point2D_2 = makePoint2D.invoke(x2, y2);

		Object arrayObject = Array.newInstance(point2DClass, 3);
		Array.set(arrayObject, 1, point2D_1);
		Array.set(arrayObject, 2, point2D_2);

		Object point2D_1_check = Array.get(arrayObject, 1);
		Object point2D_2_check = Array.get(arrayObject, 2);
		assertEquals(getX.invoke(point2D_1_check), getX.invoke(point2D_1));
		assertEquals(getY.invoke(point2D_1_check), getY.invoke(point2D_1));
		assertEquals(getX.invoke(point2D_2_check), getX.invoke(point2D_2));
		assertEquals(getY.invoke(point2D_2_check), getY.invoke(point2D_2));
	}

	@Test(priority=5)
	static public void testGCFlattenedPoint2DArray() throws Throwable {
		int x1 = 0xFFEEFFEE;
		int y1 = 0xAABBAABB;
		Object point2D = makePoint2D.invoke(x1, y1);
		Object arrayObject = Array.newInstance(point2DClass, 8);

		for (int i = 0; i < 8; i++) {
			Array.set(arrayObject, i, point2D);
		}

		System.gc();
		System.gc();

		Object value = Array.get(arrayObject, 0);
	}

	@Test(priority=5)
	static public void testGCFlattenedValueArrayWithSingleAlignment() throws Throwable {
		Object array = Array.newInstance(assortedValueWithSingleAlignmentClass, 4);
		
		for (int i = 0; i < 4; i++) {
			Object object = createAssorted(makeAssortedValueWithSingleAlignment, typeWithSingleAlignmentFields);
			Array.set(array, i, object);
		}

		System.gc();
		System.gc();

		for (int i = 0; i < 4; i++) {
			checkFieldAccessMHOfAssortedType(assortedValueWithSingleAlignmentGetterAndWither, Array.get(array, i), typeWithSingleAlignmentFields, true);
		}
	}

	@Test(priority=5)
	static public void testGCFlattenedValueArrayWithObjectAlignment() throws Throwable {
		Object array = Array.newInstance(assortedValueWithObjectAlignmentClass, 4);
		
		for (int i = 0; i < 4; i++) {
			Object object = createAssorted(makeAssortedValueWithObjectAlignment, typeWithObjectAlignmentFields);
			Array.set(array, i, object);
		}

		System.gc();
		System.gc();

		for (int i = 0; i < 4; i++) {
			checkFieldAccessMHOfAssortedType(assortedValueWithObjectAlignmentGetterAndWither, Array.get(array, i), typeWithObjectAlignmentFields, true);
		}
	}

	@Test(priority=5)
	static public void testGCFlattenedValueArrayWithLongAlignment() throws Throwable {
		Object array = Array.newInstance(assortedValueWithLongAlignmentClass, 4);
		
		for (int i = 0; i < 4; i++) {
			Object object = createAssorted(makeAssortedValueWithLongAlignment, typeWithLongAlignmentFields);
			Array.set(array, i, object);
		}

		System.gc();
		System.gc();

		for (int i = 0; i < 4; i++) {
			checkFieldAccessMHOfAssortedType(assortedValueWithLongAlignmentGetterAndWither, Array.get(array, i), typeWithLongAlignmentFields, true);
		}
	}

	@Test(priority=5)
	static public void testGCFlattenedLargeObjectArray() throws Throwable {
		Object arrayObject = Array.newInstance(largeObjectValueClass, 4);
		Object largeObjectRef = createLargeObject(new Object());

		for (int i = 0; i < 4; i++) {
			Array.set(arrayObject, i, largeObjectRef);
		}

		System.gc();
		System.gc();

		Object value = Array.get(arrayObject, 0);
	}

	@Test(priority=5)
	static public void testGCFlattenedMegaObjectArray() throws Throwable {
		Object arrayObject = Array.newInstance(megaObjectValueClass, 4);
		Object megaObjectRef = createMegaObject(new Object());

		System.gc();
		System.gc();

		for (int i = 0; i < 4; i++) {
			Array.set(arrayObject, i, megaObjectRef);
		}
		System.gc();
		System.gc();

		Object value = Array.get(arrayObject, 0);
	}


	/*
	 * Create a value type with double slot primitive members
	 * 
	 * value Point2DComplex {
	 * 	double d;
	 * 	long j;
	 * }
	 */
	@Test(priority=1)
	static public void testCreatePoint2DComplex() throws Throwable {
		String fields[] = {"d:D", "j:J"};
		Class point2DComplexClass = ValueTypeGenerator.generateValueClass("Point2DComplex", fields);
		
		MethodHandle makePoint2DComplex = lookup.findStatic(point2DComplexClass, "makeValue", MethodType.methodType(point2DComplexClass, double.class, long.class));

		MethodHandle getD = generateGetter(point2DComplexClass, "d", double.class);
		MethodHandle withD = generateWither(point2DComplexClass, "d", double.class);
		MethodHandle getJ = generateGetter(point2DComplexClass, "j", long.class);
		MethodHandle withJ = generateWither(point2DComplexClass, "j", long.class);
		
		double d = Double.MAX_VALUE;
		long j = Long.MAX_VALUE;
		double dNew = Long.MIN_VALUE;
		long jNew = Long.MIN_VALUE;
		Object point2D = makePoint2DComplex.invoke(d, j);
		
		assertEquals(getD.invoke(point2D), d);
		assertEquals(getJ.invoke(point2D), j);
		
		point2D = withD.invoke(point2D, dNew);
		point2D = withJ.invoke(point2D, jNew);
		assertEquals(getD.invoke(point2D), dNew);
		assertEquals(getJ.invoke(point2D), jNew);


		MethodHandle getDGeneric = generateGenericGetter(point2DComplexClass, "d");
		MethodHandle withDGeneric = generateGenericWither(point2DComplexClass, "d");
		MethodHandle getJGeneric = generateGenericGetter(point2DComplexClass, "j");
		MethodHandle withJGeneric = generateGenericWither(point2DComplexClass, "j");
		
		point2D = withDGeneric.invoke(point2D, d);
		point2D = withJGeneric.invoke(point2D, j);
		assertEquals(getDGeneric.invoke(point2D), d);
		assertEquals(getJGeneric.invoke(point2D), j);
	}

	/*
	 * Test with nested values in reference type
	 * 
	 * value Line2D {
	 * 	Point2D st;
	 * 	Point2D en;
	 * }
	 * 
	 */
	@Test(priority=2)
	static public void testCreateLine2D() throws Throwable {
		String fields[] = {"st:LPoint2D;:value", "en:LPoint2D;:value"};
		line2DClass = ValueTypeGenerator.generateValueClass("Line2D", fields);
		
		makeLine2D = lookup.findStatic(line2DClass, "makeValue", MethodType.methodType(line2DClass, point2DClass, point2DClass));
		
		MethodHandle getSt = generateGetter(line2DClass, "st", point2DClass);
 		MethodHandle withSt = generateWither(line2DClass, "st", point2DClass);
 		MethodHandle getEn = generateGetter(line2DClass, "en", point2DClass);
 		MethodHandle withEn = generateWither(line2DClass, "en", point2DClass);
 		
		int x = 0xFFEEFFEE;
		int y = 0xAABBAABB;
		int xNew = 0x11223344;
		int yNew = 0x99887766;
		int x2 = 0xCCDDCCDD;
		int y2 = 0xAAFFAAFF;
		int x2New = 0x55337799;
		int y2New = 0x88662244;
		
		Object st = makePoint2D.invoke(x, y);
		Object en = makePoint2D.invoke(x2, y2);
		
		assertEquals(getX.invoke(st), x);
		assertEquals(getY.invoke(st), y);
		assertEquals(getX.invoke(en), x2);
		assertEquals(getY.invoke(en), y2);
		
		Object line2D = makeLine2D.invoke(st, en);
		
		assertEquals(getX.invoke(getSt.invoke(line2D)), x);
		assertEquals(getY.invoke(getSt.invoke(line2D)), y);
		assertEquals(getX.invoke(getEn.invoke(line2D)), x2);
		assertEquals(getY.invoke(getEn.invoke(line2D)), y2);
		
		Object stNew = makePoint2D.invoke(xNew, yNew);
		Object enNew = makePoint2D.invoke(x2New, y2New);
		
		line2D = withSt.invoke(line2D, stNew);
		line2D = withEn.invoke(line2D, enNew);
		
		assertEquals(getX.invoke(getSt.invoke(line2D)), xNew);
		assertEquals(getY.invoke(getSt.invoke(line2D)), yNew);
		assertEquals(getX.invoke(getEn.invoke(line2D)), x2New);
		assertEquals(getY.invoke(getEn.invoke(line2D)), y2New);
	}
	
	/*
	 * Test with nested values in reference type
	 * 
	 * value FlattenedLine2D {
	 * 	flattened Point2D st;
	 * 	flattened Point2D en;
	 * }
	 * 
	 */
	@Test(priority=2)
	static public void testCreateFlattenedLine2D() throws Throwable {
		String fields[] = {"st:QPoint2D;:value", "en:QPoint2D;:value"};
		flattenedLine2DClass = ValueTypeGenerator.generateValueClass("FlattenedLine2D", fields);
				
		makeFlattenedLine2D = lookup.findStatic(flattenedLine2DClass, "makeValueGeneric", MethodType.methodType(flattenedLine2DClass, Object.class, Object.class));
		
		getFlatSt = generateGenericGetter(flattenedLine2DClass, "st");
 		withFlatSt = generateGenericWither(flattenedLine2DClass, "st");
 		getFlatEn = generateGenericGetter(flattenedLine2DClass, "en");
 		withFlatEn = generateGenericWither(flattenedLine2DClass, "en");
 		
		int x = 0xFFEEFFEE;
		int y = 0xAABBAABB;
		int xNew = 0x11223344;
		int yNew = 0x99887766;
		int x2 = 0xCCDDCCDD;
		int y2 = 0xAAFFAAFF;
		int x2New = 0x55337799;
		int y2New = 0x88662244;
		
		Object st = makePoint2D.invoke(x, y);
		Object en = makePoint2D.invoke(x2, y2);
		
		assertEquals(getX.invoke(st), x);
		assertEquals(getY.invoke(st), y);
		assertEquals(getX.invoke(en), x2);
		assertEquals(getY.invoke(en), y2);
		
		Object line2D = makeFlattenedLine2D.invoke(st, en);
		
		assertEquals(getX.invoke(getFlatSt.invoke(line2D)), x);
		assertEquals(getY.invoke(getFlatSt.invoke(line2D)), y);
		assertEquals(getX.invoke(getFlatEn.invoke(line2D)), x2);
		assertEquals(getY.invoke(getFlatEn.invoke(line2D)), y2);
		
		Object stNew = makePoint2D.invoke(xNew, yNew);
		Object enNew = makePoint2D.invoke(x2New, y2New);
		
		line2D = withFlatSt.invoke(line2D, stNew);
		line2D = withFlatEn.invoke(line2D, enNew);
		
		assertEquals(getX.invoke(getFlatSt.invoke(line2D)), xNew);
		assertEquals(getY.invoke(getFlatSt.invoke(line2D)), yNew);
		assertEquals(getX.invoke(getFlatEn.invoke(line2D)), x2New);
		assertEquals(getY.invoke(getFlatEn.invoke(line2D)), y2New);
	}

	/*
	 * Test array of FlattenedLine2D
	 * 
	 * value FlattenedLine2D {
	 * 	flattened Point2D st;
	 * 	flattened Point2D en;
	 * }
	 * 
	 */
	@Test(priority=3)
	static public void testCreateArrayFlattenedLine2D() throws Throwable {
		int x = 0xFFEEFFEE;
		int y = 0xAABBAABB;
		int x2 = 0xCCDDCCDD;
		int y2 = 0xAAFFAAFF;
		int x3 = 0xFFABFFCD;
		int y3 = 0xBBAABBAA;
		int x4 = 0xCCBBAADD;
		int y4 = 0xAABBAACC;

		Object st1 = makePoint2D.invoke(x, y);
		Object en1 = makePoint2D.invoke(x2, y2);
		Object line2D_1 = makeFlattenedLine2D.invoke(st1, en1);		

		Object st2 = makePoint2D.invoke(x3, y3);
		Object en2 = makePoint2D.invoke(x4, y4);
		Object line2D_2 = makeFlattenedLine2D.invoke(st2, en2);

		Object arrayObject = Array.newInstance(flattenedLine2DClass, 3);
		Array.set(arrayObject, 1, line2D_1);
		Array.set(arrayObject, 2, line2D_2);

		Object line2D_1_check = Array.get(arrayObject, 1);
		Object line2D_2_check = Array.get(arrayObject, 2);

		assertEquals(getX.invoke(getFlatSt.invoke(line2D_1_check)), getX.invoke(getFlatSt.invoke(line2D_1)));
		assertEquals(getX.invoke(getFlatSt.invoke(line2D_2_check)), getX.invoke(getFlatSt.invoke(line2D_2)));
		assertEquals(getY.invoke(getFlatSt.invoke(line2D_1_check)), getY.invoke(getFlatSt.invoke(line2D_1)));
		assertEquals(getY.invoke(getFlatSt.invoke(line2D_2_check)), getY.invoke(getFlatSt.invoke(line2D_2)));
		assertEquals(getX.invoke(getFlatEn.invoke(line2D_1_check)), getX.invoke(getFlatEn.invoke(line2D_1)));
		assertEquals(getX.invoke(getFlatEn.invoke(line2D_2_check)), getX.invoke(getFlatEn.invoke(line2D_2)));
		assertEquals(getY.invoke(getFlatEn.invoke(line2D_1_check)), getY.invoke(getFlatEn.invoke(line2D_1)));
		assertEquals(getY.invoke(getFlatEn.invoke(line2D_2_check)), getY.invoke(getFlatEn.invoke(line2D_2)));
	}	

	/*
	 * Test with nested values
	 * 
	 * value InvalidField {
	 * 	flattened Point2D st;
	 * 	flattened Invalid x;
	 * }
	 * 
	 */
	@Test(priority=3)
	static public void testInvalidNestedField() throws Throwable {
		String fields[] = {"st:QPoint2D;:value", "x:QInvalid;:value"};

		try {
			Class<?> invalidField = ValueTypeGenerator.generateValueClass("InvalidField", fields);
			Assert.fail("should throw error. Nested class doesn't exist!");
		} catch (NoClassDefFoundError e) {}
	}
	
	/*
	 * Test with none value Qtype
	 * 
	 * value NoneValueQType {
	 * 	flattened Point2D st;
	 * 	flattened Object o;
	 * }
	 * 
	 */
	@Test(priority=3)
	static public void testNoneValueQTypeAsNestedField() throws Throwable {
		String fields[] = {"st:QPoint2D;:value", "o:Qjava/lang/Object;:value"};
		try {
			Class<?> noneValueQType = ValueTypeGenerator.generateValueClass("NoneValueQType", fields);
			Assert.fail("should throw error. j.l.Object is not a qtype!");
		} catch (IncompatibleClassChangeError e) {}
	}
	
	/*
	 * Test defaultValue with ref type
	 * 
	 * class DefaultValueWithNoneValueType {
	 * 	Object f1;
	 * 	Object f1;
	 * }
	 * 
	 */
	@Test(priority=3)
	static public void testDefaultValueWithNonValueType() throws Throwable {
		String fields[] = {"f1:Ljava/lang/Object;:value", "f2:Ljava/lang/Object;:value"};
		Class<?> defaultValueWithNonValueType = ValueTypeGenerator.generateRefClass("DefaultValueWithNonValueType", fields);
		MethodHandle makeDefaultValueWithNonValueType = lookup.findStatic(defaultValueWithNonValueType, "makeValue", MethodType.methodType(defaultValueWithNonValueType, Object.class, Object.class));
		try {
			makeDefaultValueWithNonValueType.invoke(null, null);
			Assert.fail("should throw error. Default value must be used with ValueType");
		} catch (IncompatibleClassChangeError e) {}
	}
	
	/*
	 * Test withField on non Value Type
	 * 
	 * class TestWithFieldOnNonValueType {
	 *  long longField
	 * }
	 */
	@Test(priority=1)
	static public void testWithFieldOnNonValueType() throws Throwable {
		String fields[] = {"longField:J"};
		Class<?> testWithFieldOnNonValueType = ValueTypeGenerator.generateRefClass("TestWithFieldOnNonValueType", fields);
		MethodHandle withFieldOnNonValueType = lookup.findStatic(testWithFieldOnNonValueType, "testWithFieldOnNonValueType", MethodType.methodType(Object.class));
		try {
			withFieldOnNonValueType.invoke();
			Assert.fail("should throw error. WithField must be used with ValueType");
		} catch (IncompatibleClassChangeError e) {}
	}
	
	/*
	 * Test withField on non Null type
	 * 
	 * class TestWithFieldOnNull {
	 *  long longField
	 * }
	 */
	@Test(priority=1)
	static public void testWithFieldOnNull() throws Throwable {
		String fields[] = {"longField:J"};
		Class<?> testWithFieldOnNull = ValueTypeGenerator.generateRefClass("TestWithFieldOnNull", fields);
		
		MethodHandle withFieldOnNull = lookup.findStatic(testWithFieldOnNull, "testWithFieldOnNull", MethodType.methodType(Object.class));
		try {
			withFieldOnNull.invoke();
			Assert.fail("should throw error. Objectref cannot be null");
		} catch (NullPointerException e) {}
	}
	
	/*
	 * Test withField on non existent class
	 * 
	 * class TestWithFieldOnNonExistentClass {
	 *  long longField
	 * }
	 */
	@Test(priority=1)
	static public void testWithFieldOnNonExistentClass() throws Throwable {
		String fields[] = {"longField:J"};
		Class<?> testWithFieldOnNonExistentClass = ValueTypeGenerator.generateRefClass("TestWithFieldOnNonExistentClass", fields);
		MethodHandle withFieldOnNonExistentClass = lookup.findStatic(testWithFieldOnNonExistentClass, "testWithFieldOnNonExistentClass", MethodType.methodType(Object.class));
		try {
			withFieldOnNonExistentClass.invoke();
			Assert.fail("should throw error. Class does not exist");
		} catch (NoClassDefFoundError e) {}
	}

	
	@Test(priority=2)
	static public void testBasicACMPTestOnIdentityTypes() throws Throwable {
		
		Object identityType1 = new String();
		Object identityType2 = new String();
		Object nullPointer = null;
		
		/* sanity test on identity classes */
		Assert.assertTrue((identityType1 == identityType1), "An identity (==) comparison on the same identityType should always return true");
		
		Assert.assertFalse((identityType2 == identityType1), "An identity (==) comparison on different identityTypes should always return false");
		
		Assert.assertFalse((identityType2 == nullPointer), "An identity (==) comparison on different identityTypes should always return false");
		
		Assert.assertTrue((nullPointer == nullPointer), "An identity (==) comparison on the same identityType should always return true");
		
		Assert.assertFalse((identityType1 != identityType1), "An identity (!=) comparison on the same identityType should always return false");
		
		Assert.assertTrue((identityType2 != identityType1), "An identity (!=) comparison on different identityTypes should always return true");
		
		Assert.assertTrue((identityType2 != nullPointer), "An identity (!=) comparison on different identityTypes should always return true");
		
		Assert.assertFalse((nullPointer != nullPointer), "An identity (!=) comparison on the same identityType should always return false");
		
	}
	
	@Test(priority=2)
	static public void testBasicACMPTestOnValueTypes() throws Throwable {
		Object valueType1 = makePoint2D.invoke(1, 2);
		Object valueType2 = makePoint2D.invoke(1, 2);
		Object newValueType = makePoint2D.invoke(2, 1);
		Object identityType = new String();
		Object nullPointer = null;
		
		Assert.assertTrue((valueType1 == valueType1), "A substitutability (==) test on the same value should always return true");
		
		Assert.assertTrue((valueType1 == valueType2), "A substitutability (==) test on different value the same contents should always return true");
		
		Assert.assertFalse((valueType1 == newValueType), "A substitutability (==) test on different value should always return false");
		
		Assert.assertFalse((valueType1 == identityType), "A substitutability (==) test on different value with identity type should always return false");
		
		Assert.assertFalse((valueType1 == nullPointer), "A substitutability (==) test on different value with null pointer should always return false");
		
		Assert.assertFalse((valueType1 != valueType1), "A substitutability (!=) test on the same value should always return false");
		
		Assert.assertFalse((valueType1 != valueType2), "A substitutability (!=) test on different value the same contents should always return false");
		
		Assert.assertTrue((valueType1 != newValueType), "A substitutability (!=) test on different value should always return true");
		
		Assert.assertTrue((valueType1 != identityType), "A substitutability (!=) test on different value with identity type should always return true");
		
		Assert.assertTrue((valueType1 != nullPointer), "A substitutability (!=) test on different value with null pointer should always return true");
	}
	
	@Test(priority=4)
	static public void testACMPTestOnFastSubstitutableValueTypes() throws Throwable {
		Object valueType1 = createTriangle2D(defaultTrianglePositions);
		Object valueType2 = createTriangle2D(defaultTrianglePositions);
		Object newValueType = createTriangle2D(defaultTrianglePositionsNew);
		Object identityType = new String();
		Object nullPointer = null;
		
		Assert.assertTrue((valueType1 == valueType1), "A substitutability (==) test on the same value should always return true");
		
		Assert.assertTrue((valueType1 == valueType2), "A substitutability (==) test on different value the same contents should always return true");
		
		Assert.assertFalse((valueType1 == newValueType), "A substitutability (==) test on different value should always return false");
		
		Assert.assertFalse((valueType1 == identityType), "A substitutability (==) test on different value with identity type should always return false");
		
		Assert.assertFalse((valueType1 == nullPointer), "A substitutability (==) test on different value with null pointer should always return false");
		
		Assert.assertFalse((valueType1 != valueType1), "A substitutability (!=) test on the same value should always return false");
		
		Assert.assertFalse((valueType1 != valueType2), "A substitutability (!=) test on different value the same contents should always return false");
		
		Assert.assertTrue((valueType1 != newValueType), "A substitutability (!=) test on different value should always return true");
		
		Assert.assertTrue((valueType1 != identityType), "A substitutability (!=) test on different value with identity type should always return true");
		
		Assert.assertTrue((valueType1 != nullPointer), "A substitutability (!=) test on different value with null pointer should always return true");
	}
	
	@Test(priority=3)
	static public void testACMPTestOnFastSubstitutableValueTypesVer2() throws Throwable {
		/* these VTs will have array refs */
		String fields[] = {"x:I", "y:I", "z:I", "arr:[Ljava/lang/Object;"};
		Class<?> fastSubVT = ValueTypeGenerator.generateValueClass("FastSubVT", fields);
		
		MethodHandle makeFastSubVT = lookup.findStatic(fastSubVT, "makeValue", MethodType.methodType(fastSubVT, int.class, int.class, int.class, Object[].class));
		
		Object[] arr = {"foo", "bar", "baz"};
		Object[] arr2 = {"foozo", "barzo", "bazzo"};
		Object valueType1 = makeFastSubVT.invoke(1, 2, 3, arr);
		Object valueType2 = makeFastSubVT.invoke(1, 2, 3, arr);
		Object newValueType = makeFastSubVT.invoke(3, 2, 1, arr2);
		Object identityType = new String();
		Object nullPointer = null;
		
		Assert.assertTrue((valueType1 == valueType1), "A substitutability (==) test on the same value should always return true");
		
		Assert.assertTrue((valueType1 == valueType2), "A substitutability (==) test on different value the same contents should always return true");
		
		Assert.assertFalse((valueType1 == newValueType), "A substitutability (==) test on different value should always return false");
		
		Assert.assertFalse((valueType1 == identityType), "A substitutability (==) test on different value with identity type should always return false");
		
		Assert.assertFalse((valueType1 == nullPointer), "A substitutability (==) test on different value with null pointer should always return false");
		
		Assert.assertFalse((valueType1 != valueType1), "A substitutability (!=) test on the same value should always return false");
		
		Assert.assertFalse((valueType1 != valueType2), "A substitutability (!=) test on different value the same contents should always return false");
		
		Assert.assertTrue((valueType1 != newValueType), "A substitutability (!=) test on different value should always return true");
		
		Assert.assertTrue((valueType1 != identityType), "A substitutability (!=) test on different value with identity type should always return true");
		
		Assert.assertTrue((valueType1 != nullPointer), "A substitutability (!=) test on different value with null pointer should always return true");
	}
	
	@Test(priority=3)
	static public void testACMPTestOnRecursiveValueTypes() throws Throwable {
		String fields[] = {"l:J", "next:Ljava/lang/Object;", "i:I"};
		Class<?> nodeClass = ValueTypeGenerator.generateValueClass("Node", fields);
		MethodHandle makeNode = lookup.findStatic(nodeClass, "makeValue", MethodType.methodType(nodeClass, long.class, Object.class, int.class));
		
		Object list1 = makeNode.invoke(3, null, 3);
		Object list2 = makeNode.invoke(3, null, 3);
		Object list3sameAs1 = makeNode.invoke(3, null, 3);
		Object list4 = makeNode.invoke(3, null, 3);
		Object list5null = makeNode.invoke(3, null, 3);
		Object list6null = makeNode.invoke(3, null, 3);
		Object list7obj = makeNode.invoke(3, new Object(), 3);
		Object list8str = makeNode.invoke(3, "foo", 3);
		Object identityType = new String();
		Object nullPointer = null;
		
		for (int i = 0; i < 100; i++) {
			list1 = makeNode.invoke(3, list1, i);
			list2 = makeNode.invoke(3, list2, i + 1);
			list3sameAs1 = makeNode.invoke(3, list3sameAs1, i);
		}
		for (int i = 0; i < 50; i++) {
			list4 = makeNode.invoke(3, list4, i);
		}
		
		Assert.assertTrue((list1 == list1), "A substitutability (==) test on the same value should always return true");
		
		Assert.assertTrue((list5null == list5null), "A substitutability (==) test on the same value should always return true");
		
		Assert.assertTrue((list1 == list3sameAs1), "A substitutability (==) test on different value the same contents should always return true");
		
		Assert.assertTrue((list5null == list6null), "A substitutability (==) test on different value the same contents should always return true");
		
		Assert.assertFalse((list1 == list2), "A substitutability (==) test on different value should always return false");
		
		Assert.assertFalse((list1 == list4), "A substitutability (==) test on different value should always return false");
		
		Assert.assertFalse((list1 == list5null), "A substitutability (==) test on different value should always return false");
		
		Assert.assertFalse((list1 == list7obj), "A substitutability (==) test on different value should always return false");
		
		Assert.assertFalse((list1 == list8str), "A substitutability (==) test on different value should always return false");
		
		Assert.assertFalse((list7obj == list8str), "A substitutability (==) test on different value should always return false");
		
		Assert.assertFalse((list1 == identityType), "A substitutability (==) test on different value with identity type should always return false");
		
		Assert.assertFalse((list1 == nullPointer), "A substitutability (==) test on different value with null pointer should always return false");
		
		Assert.assertFalse((list1 != list1), "A substitutability (!=) test on the same value should always return false");
		
		Assert.assertFalse((list5null != list5null), "A substitutability (!=) test on the same value should always return false");
		
		Assert.assertFalse((list1 != list3sameAs1), "A substitutability (!=) test on different value the same contents should always return false");
		
		Assert.assertFalse((list5null != list6null), "A substitutability (!=) test on different value the same contents should always return false");
		
		Assert.assertTrue((list1 != list2), "A substitutability (!=) test on different value should always return true");
		
		Assert.assertTrue((list1 != list4), "A substitutability (!=) test on different value should always return true");
		
		Assert.assertTrue((list1 != list5null), "A substitutability (!=) test on different value should always return true");
		
		Assert.assertTrue((list1 != list7obj), "A substitutability (!=) test on different value should always return true");
		
		Assert.assertTrue((list1 != list8str), "A substitutability (!=) test on different value should always return true");
		
		Assert.assertTrue((list7obj != list8str), "A substitutability (!=) test on different value should always return true");
		
		Assert.assertTrue((list1 != identityType), "A substitutability (!=) test on different value with identity type should always return true");
		
		Assert.assertTrue((list1 != nullPointer), "A substitutability (!=) test on different value with null pointer should always return true");
		
	}
	
	@Test(priority=3)
	static public void testACMPTestOnValueFloat() throws Throwable {
		Object float1 = makeValueFloat.invoke(1.1f);
		Object float2 = makeValueFloat.invoke(-1.1f);
		Object float3 = makeValueFloat.invoke(12341.112341234f);
		Object float4sameAs1 = makeValueFloat.invoke(1.1f);
		Object nan = makeValueFloat.invoke(Float.NaN);
		Object nan2 = makeValueFloat.invoke(Float.NaN);
		Object positiveZero = makeValueFloat.invoke(0.0f);
		Object negativeZero = makeValueFloat.invoke(-0.0f);
		Object positiveInfinity = makeValueFloat.invoke(Float.POSITIVE_INFINITY);
		Object negativeInfinity = makeValueFloat.invoke(Float.NEGATIVE_INFINITY);
		
		Assert.assertTrue((float1 == float1), "A substitutability (==) test on the same value should always return true");

		Assert.assertTrue((float1 == float4sameAs1), "A substitutability (==) test on different value the same contents should always return true");

		Assert.assertTrue((nan == nan2), "A substitutability (==) test on different value the same contents should always return true");

		Assert.assertFalse((float1 == float2), "A substitutability (==) test on different value should always return false");

		Assert.assertFalse((float1 == float3), "A substitutability (==) test on different value should always return false");

		Assert.assertFalse((float1 == nan), "A substitutability (==) test on different value should always return false");

		Assert.assertFalse((float1 == positiveZero), "A substitutability (==) test on different value should always return false");

		Assert.assertFalse((float1 == negativeZero), "A substitutability (==) test on different value should always return false");

		Assert.assertFalse((float1 == positiveInfinity), "A substitutability (==) test on different value should always return false");

		Assert.assertFalse((float1 == negativeInfinity), "A substitutability (==) test on different value should always return false");

		Assert.assertFalse((positiveInfinity == negativeInfinity), "A substitutability (==) test on different value should always return false");

		Assert.assertFalse((positiveZero == negativeZero), "A substitutability (==) test on different value should always return false");

		Assert.assertFalse((float1 != float1), "A substitutability (!=) test on the same value should always return false");
				
		Assert.assertFalse((float1 != float4sameAs1), "A substitutability (!=) test on different value the same contents should always return false");
		
		Assert.assertFalse((nan != nan2), "A substitutability (!=) test on different value the same contents should always return false");

		Assert.assertTrue((float1 != float2), "A substitutability (!=) test on different value should always return true");
		
		Assert.assertTrue((float1 != float3), "A substitutability (!=) test on different value should always return true");

		Assert.assertTrue((float1 != nan), "A substitutability (!=) test on different value should always return true");

		Assert.assertTrue((float1 != positiveZero), "A substitutability (!=) test on different value should always return true");

		Assert.assertTrue((float1 != negativeZero), "A substitutability (!=) test on different value should always return true");

		Assert.assertTrue((float1 != positiveInfinity), "A substitutability (!=) test on different value should always return true");

		Assert.assertTrue((float1 != negativeInfinity), "A substitutability (!=) test on different value should always return true");

		Assert.assertTrue((positiveInfinity != negativeInfinity), "A substitutability (!=) test on different value should always return true");

		Assert.assertTrue((positiveZero != negativeZero), "A substitutability (!=) test on different value should always return true");
			
	}

	@Test(priority=3)
	static public void testACMPTestOnValueDouble() throws Throwable {
		Object double1 = makeValueDouble.invoke(1.1d);
		Object double2 = makeValueDouble.invoke(-1.1d);
		Object double3 = makeValueDouble.invoke(12341.112341234d);
		Object double4sameAs1 = makeValueDouble.invoke(1.1d);
		Object nan = makeValueDouble.invoke(Double.NaN);
		Object nan2 = makeValueDouble.invoke(Double.NaN);
		Object positiveZero = makeValueDouble.invoke(0.0f);
		Object negativeZero = makeValueDouble.invoke(-0.0f);
		Object positiveInfinity = makeValueDouble.invoke(Double.POSITIVE_INFINITY);
		Object negativeInfinity = makeValueDouble.invoke(Double.NEGATIVE_INFINITY);
		
		Assert.assertTrue((double1 == double1), "A substitutability (==) test on the same value should always return true");

		Assert.assertTrue((double1 == double4sameAs1), "A substitutability (==) test on different value the same contents should always return true");

		Assert.assertTrue((nan == nan2), "A substitutability (==) test on different value the same contents should always return true");

		Assert.assertFalse((double1 == double2), "A substitutability (==) test on different value should always return false");

		Assert.assertFalse((double1 == double3), "A substitutability (==) test on different value should always return false");

		Assert.assertFalse((double1 == nan), "A substitutability (==) test on different value should always return false");

		Assert.assertFalse((double1 == positiveZero), "A substitutability (==) test on different value should always return false");

		Assert.assertFalse((double1 == negativeZero), "A substitutability (==) test on different value should always return false");

		Assert.assertFalse((double1 == positiveInfinity), "A substitutability (==) test on different value should always return false");

		Assert.assertFalse((double1 == negativeInfinity), "A substitutability (==) test on different value should always return false");

		Assert.assertFalse((positiveInfinity == negativeInfinity), "A substitutability (==) test on different value should always return false");

		Assert.assertFalse((positiveZero == negativeZero), "A substitutability (==) test on different value should always return false");

		Assert.assertFalse((double1 != double1), "A substitutability (!=) test on the same value should always return false");
				
		Assert.assertFalse((double1 != double4sameAs1), "A substitutability (!=) test on different value the same contents should always return false");
		
		Assert.assertFalse((nan != nan2), "A substitutability (!=) test on different value the same contents should always return false");

		Assert.assertTrue((double1 != double2), "A substitutability (!=) test on different value should always return true");
		
		Assert.assertTrue((double1 != double3), "A substitutability (!=) test on different value should always return true");

		Assert.assertTrue((double1 != nan), "A substitutability (!=) test on different value should always return true");

		Assert.assertTrue((double1 != positiveZero), "A substitutability (!=) test on different value should always return true");

		Assert.assertTrue((double1 != negativeZero), "A substitutability (!=) test on different value should always return true");

		Assert.assertTrue((double1 != positiveInfinity), "A substitutability (!=) test on different value should always return true");

		Assert.assertTrue((double1 != negativeInfinity), "A substitutability (!=) test on different value should always return true");

		Assert.assertTrue((positiveInfinity != negativeInfinity), "A substitutability (!=) test on different value should always return true");

		Assert.assertTrue((positiveZero != negativeZero), "A substitutability (!=) test on different value should always return true");
	}
	
	@Test(priority=6)
	static public void testACMPTestOnAssortedValues() throws Throwable {
		Object assortedValueWithLongAlignment = createAssorted(makeAssortedValueWithLongAlignment, typeWithLongAlignmentFields);
		Object assortedValueWithLongAlignment2 = createAssorted(makeAssortedValueWithLongAlignment, typeWithLongAlignmentFields);
		Object assortedValueWithLongAlignment3 = createAssorted(makeAssortedValueWithLongAlignment, typeWithLongAlignmentFields);
		assortedValueWithLongAlignment3 = checkFieldAccessMHOfAssortedType(assortedValueWithLongAlignmentGetterAndWither, assortedValueWithLongAlignment3, typeWithLongAlignmentFields, true);
		
		Object assortedValueWithObjectAlignment = createAssorted(makeAssortedValueWithObjectAlignment, typeWithObjectAlignmentFields);
		Object assortedValueWithObjectAlignment2 = createAssorted(makeAssortedValueWithObjectAlignment, typeWithObjectAlignmentFields);
		Object assortedValueWithObjectAlignment3 = createAssorted(makeAssortedValueWithObjectAlignment, typeWithObjectAlignmentFields);
		assortedValueWithObjectAlignment3 = checkFieldAccessMHOfAssortedType(assortedValueWithObjectAlignmentGetterAndWither, assortedValueWithObjectAlignment3, typeWithObjectAlignmentFields, true);
		
		Assert.assertTrue((assortedValueWithLongAlignment == assortedValueWithLongAlignment), "A substitutability (==) test on the same value should always return true");

		Assert.assertTrue((assortedValueWithObjectAlignment == assortedValueWithObjectAlignment), "A substitutability (==) test on the same value should always return true");
		
		Assert.assertTrue((assortedValueWithLongAlignment == assortedValueWithLongAlignment2), "A substitutability (==) test on different value the same contents should always return true");

		Assert.assertTrue((assortedValueWithObjectAlignment == assortedValueWithObjectAlignment2), "A substitutability (==) test on different value the same contents should always return true");

		Assert.assertFalse((assortedValueWithLongAlignment == assortedValueWithLongAlignment3), "A substitutability (==) test on different value should always return false");

		Assert.assertFalse((assortedValueWithLongAlignment == assortedValueWithObjectAlignment), "A substitutability (==) test on different value should always return false");

		Assert.assertFalse((assortedValueWithObjectAlignment == assortedValueWithObjectAlignment3), "A substitutability (==) test on different value should always return false");

		Assert.assertFalse((assortedValueWithLongAlignment != assortedValueWithLongAlignment), "A substitutability (!=) test on the same value should always return false");
		
		Assert.assertFalse((assortedValueWithObjectAlignment != assortedValueWithObjectAlignment), "A substitutability (!=) test on the same value should always return false");
				
		Assert.assertFalse((assortedValueWithLongAlignment != assortedValueWithLongAlignment2), "A substitutability (!=) test on different value the same contents should always return false");
		
		Assert.assertFalse((assortedValueWithObjectAlignment != assortedValueWithObjectAlignment2), "A substitutability (!=) test on different value the same contents should always return false");

		Assert.assertTrue((assortedValueWithLongAlignment != assortedValueWithLongAlignment3), "A substitutability (!=) test on different value the same contents should always return false");

		Assert.assertTrue((assortedValueWithLongAlignment != assortedValueWithObjectAlignment), "A substitutability (!=) test on different value the same contents should always return false");

		Assert.assertTrue((assortedValueWithObjectAlignment != assortedValueWithObjectAlignment3), "A substitutability (!=) test on different value the same contents should always return false");

	}
	
	/*
	 * Test monitorEnter on valueType
	 * 
	 * class TestMonitorEnterOnValueType {
	 *  long longField
	 * }
	 */
	@Test(priority=2)
	static public void testMonitorEnterOnValueType() throws Throwable {
		int x = 0;
		int y = 0;
		Object valueType = makePoint2D.invoke(x, y);
		Object valueTypeArray = Array.newInstance(flattenedLine2DClass, genericArraySize);

		String fields[] = {"longField:J"};
		Class<?> testMonitorEnterOnValueType = ValueTypeGenerator.generateRefClass("TestMonitorEnterOnValueType", fields);
		MethodHandle monitorEnterOnValueType = lookup.findStatic(testMonitorEnterOnValueType, "testMonitorEnterOnObject", MethodType.methodType(void.class, Object.class));
		try {
			monitorEnterOnValueType.invoke(valueType);
			Assert.fail("should throw exception. MonitorEnter cannot be used with ValueType");
		} catch (IllegalMonitorStateException e) {}
		
		try {
			monitorEnterOnValueType.invoke(valueTypeArray);
		} catch (IllegalMonitorStateException e) {
			Assert.fail("Should not throw exception. MonitorEnter can be used with ValueType arrays");
		}
	}

	/*
	 * Test monitorExit on valueType
	 * 
	 * class TestMonitorExitOnValueType {
	 *  long longField
	 * }
	 */
	@Test(priority=2)
	static public void testMonitorExitOnValueType() throws Throwable {
		int x = 1;
		int y = 1;
		Object valueType = makePoint2D.invoke(x, y);
		Object valueTypeArray = Array.newInstance(flattenedLine2DClass, genericArraySize);

		String fields[] = {"longField:J"};
		Class<?> testMonitorExitOnValueType = ValueTypeGenerator.generateRefClass("TestMonitorExitOnValueType", fields);
		MethodHandle monitorEnterOnValueType = lookup.findStatic(testMonitorExitOnValueType, "testMonitorEnterOnObject", MethodType.methodType(void.class, Object.class));
		MethodHandle monitorExitOnValueType = lookup.findStatic(testMonitorExitOnValueType, "testMonitorExitOnObject", MethodType.methodType(void.class, Object.class));
		try {
			monitorEnterOnValueType.invoke(valueType);
			monitorExitOnValueType.invoke(valueType);
			Assert.fail("should throw exception. MonitorExit cannot be used with ValueType");
		} catch (IllegalMonitorStateException e) {}
		
		try {
			monitorEnterOnValueType.invoke(valueTypeArray);
			monitorExitOnValueType.invoke(valueTypeArray);
		} catch (IllegalMonitorStateException e) {
			Assert.fail("Should not throw exception. MonitorExit can be used with ValueType arrays");
		}
	}


	@Test(priority=2)
	static public void testSynchMethodsOnValueTypes() throws Throwable {
		int x = 1;
		int y = 1;
		Object valueType = makePoint2D.invoke(x, y);
		MethodHandle syncMethod = lookup.findVirtual(point2DClass, "synchronizedMethodReturnInt", MethodType.methodType(int.class));
		MethodHandle staticSyncMethod = lookup.findStatic(point2DClass, "staticSynchronizedMethodReturnInt", MethodType.methodType(int.class));
		
		try {
			syncMethod.invoke(valueType);
			Assert.fail("should throw exception. Synchronized methods cannot be used with ValueType");
		} catch (IllegalMonitorStateException e) {}
		
		try {
			staticSyncMethod.invoke();
		} catch (IllegalMonitorStateException e) {
			Assert.fail("should not throw exception. Synchronized static methods can be used with ValueType");
		}
	}
	
	@Test(priority=2)
	static public void testSynchMethodsOnRefTypes() throws Throwable {
		String fields[] = {"longField:J"};
		Class<?> refTypeClass = ValueTypeGenerator.generateRefClass("RefType", fields);
		MethodHandle makeRef = lookup.findStatic(refTypeClass, "makeRef", MethodType.methodType(refTypeClass, long.class));
		Object refType = makeRef.invoke(1L);
		
		MethodHandle syncMethod = lookup.findVirtual(refTypeClass, "synchronizedMethodReturnInt", MethodType.methodType(int.class));
		MethodHandle staticSyncMethod = lookup.findStatic(refTypeClass, "staticSynchronizedMethodReturnInt", MethodType.methodType(int.class));
		
		try {
			syncMethod.invoke(refType);
		} catch (IllegalMonitorStateException e) {
			Assert.fail("should not throw exception. Synchronized static methods can be used with RefType");
		}
		
		try {
			staticSyncMethod.invoke();
		} catch (IllegalMonitorStateException e) {
			Assert.fail("should not throw exception. Synchronized static methods can be used with RefType");
		}
	}
	
	
	/*
	 * Test monitorEnter with refType
	 * 
	 * class TestMonitorEnterWithRefType {
	 *  long longField
	 * }
	 */
	@Test(priority=1)
	static public void testMonitorEnterWithRefType() throws Throwable {
		int x = 0;
		Object refType = (Object) x;
		
		String fields[] = {"longField:J"};
		Class<?> testMonitorEnterWithRefType = ValueTypeGenerator.generateRefClass("TestMonitorEnterWithRefType", fields);
		MethodHandle monitorEnterWithRefType = lookup.findStatic(testMonitorEnterWithRefType, "testMonitorEnterOnObject", MethodType.methodType(void.class, Object.class));
		try {
			monitorEnterWithRefType.invoke(refType);
		} catch (IllegalMonitorStateException e) { 
			Assert.fail("shouldn't throw exception. MonitorEnter should be used with refType");
		}
	}

	/*
	 * Test monitorExit with refType
	 * 
	 * class TestMonitorExitWithRefType {
	 *  long longField
	 * }
	 */
	@Test(priority=1)
	static public void testMonitorExitWithRefType() throws Throwable {
		int x = 1;
		Object refType = (Object) x;
		
		String fields[] = {"longField:J"};
		Class<?> testMonitorExitWithRefType = ValueTypeGenerator.generateRefClass("TestMonitorExitWithRefType", fields);
		MethodHandle monitorExitWithRefType = lookup.findStatic(testMonitorExitWithRefType, "testMonitorExitOnObject", MethodType.methodType(void.class, Object.class));
		try {
			monitorExitWithRefType.invoke(refType);
			Assert.fail("should throw exception. MonitorExit doesn't have a matching MonitorEnter");
		} catch (IllegalMonitorStateException e) {}
	}

	/*
	 * Test monitorEnterAndExit with refType
	 * 
	 * class TestMonitorEnterAndExitWithRefType {
	 *  long longField
	 * }
	 */
	@Test(priority=1)
	static public void testMonitorEnterAndExitWithRefType() throws Throwable {
		int x = 2;
		Object refType = (Object) x;
		
		String fields[] = {"longField:J"};
		Class<?> testMonitorEnterAndExitWithRefType = ValueTypeGenerator.generateRefClass("TestMonitorEnterAndExitWithRefType", fields);
		MethodHandle monitorEnterAndExitWithRefType = lookup.findStatic(testMonitorEnterAndExitWithRefType, "testMonitorEnterAndExitWithRefType", MethodType.methodType(void.class, Object.class));
		try {
			monitorEnterAndExitWithRefType.invoke(refType);
		} catch (IllegalMonitorStateException e) {
			Assert.fail("shouldn't throw exception. MonitorEnter and MonitorExit should be used with refType");
		}
	}
	
	/*	
	 * Create a valueType with three valueType members
	 * 
	 * value Triangle2D {
	 *  flattened Line2D v1;
	 *  flattened Line2D v2;
	 *  flattened Line2D v3;
	 * }
	 */
	@Test(priority=3)
	static public void testCreateTriangle2D() throws Throwable {
		String fields[] = {"v1:QFlattenedLine2D;:value", "v2:QFlattenedLine2D;:value", "v3:QFlattenedLine2D;:value"};
		triangle2DClass = ValueTypeGenerator.generateValueClass("Triangle2D", fields);

		makeTriangle2D = lookup.findStatic(triangle2DClass, "makeValueGeneric", MethodType.methodType(triangle2DClass, Object.class, Object.class, Object.class));

		getV1 = generateGenericGetter(triangle2DClass, "v1");
		MethodHandle withV1 = generateGenericWither(triangle2DClass, "v1");
		getV2 = generateGenericGetter(triangle2DClass, "v2");
		MethodHandle withV2 = generateGenericWither(triangle2DClass, "v2");
		getV3 = generateGenericGetter(triangle2DClass, "v3");
		MethodHandle withV3 = generateGenericWither(triangle2DClass, "v3");

		MethodHandle[][] getterAndWither = {{getV1, withV1}, {getV2, withV2}, {getV3, withV3}};
		Object triangle2D = createTriangle2D(defaultTrianglePositions);
		checkEqualTriangle2D(triangle2D, defaultTrianglePositions);
		
		for (int i = 0; i < getterAndWither.length; i++) {
			triangle2D = getterAndWither[i][1].invoke(triangle2D, createFlattenedLine2D(defaultTrianglePositionsNew[i]));
		}
		
		checkEqualTriangle2D(triangle2D, defaultTrianglePositionsNew);
	}
	
	
	@Test(priority=4)
	static public void testCreateArrayTriangle2D() throws Throwable {
		Object arrayObject = Array.newInstance(triangle2DClass, genericArraySize);
		Object triangle1 = createTriangle2D(defaultTrianglePositions);
		Object triangle2 = createTriangle2D(defaultTrianglePositionsNew);
		Object triangleEmpty = createTriangle2D(defaultTrianglePositionsEmpty);
		
		Array.set(arrayObject, 0, triangle1);
		Array.set(arrayObject, 1, triangleEmpty);
		Array.set(arrayObject, 2, triangle2);
		Array.set(arrayObject, 3, triangleEmpty);
		Array.set(arrayObject, 4, triangle1);
		Array.set(arrayObject, 5, triangleEmpty);
		Array.set(arrayObject, 6, triangle2);
		Array.set(arrayObject, 7, triangleEmpty);
		Array.set(arrayObject, 8, triangle1);
		Array.set(arrayObject, 9, triangleEmpty);
		
		checkEqualTriangle2D(Array.get(arrayObject, 0), defaultTrianglePositions);
		checkEqualTriangle2D(Array.get(arrayObject, 1), defaultTrianglePositionsEmpty);
		checkEqualTriangle2D(Array.get(arrayObject, 2), defaultTrianglePositionsNew);
		checkEqualTriangle2D(Array.get(arrayObject, 3), defaultTrianglePositionsEmpty);
		checkEqualTriangle2D(Array.get(arrayObject, 4), defaultTrianglePositions);
		checkEqualTriangle2D(Array.get(arrayObject, 5), defaultTrianglePositionsEmpty);
		checkEqualTriangle2D(Array.get(arrayObject, 6), defaultTrianglePositionsNew);
		checkEqualTriangle2D(Array.get(arrayObject, 7), defaultTrianglePositionsEmpty);
		checkEqualTriangle2D(Array.get(arrayObject, 8), defaultTrianglePositions);
		checkEqualTriangle2D(Array.get(arrayObject, 9), defaultTrianglePositionsEmpty);
	}

	/*
	 * Create a value type with a long primitive member
	 * 
	 * value ValueLong {
	 * 	long j;
	 * }
	 */
	@Test(priority=1)
	static public void testCreateValueLong() throws Throwable {
		String fields[] = {"j:J"};
		valueLongClass = ValueTypeGenerator.generateValueClass("ValueLong", fields);
		makeValueLong = lookup.findStatic(valueLongClass, "makeValue",
				MethodType.methodType(valueLongClass, long.class));

		getLong = generateGetter(valueLongClass, "j", long.class);
		withLong = generateWither(valueLongClass, "j", long.class);

		long j = Long.MAX_VALUE;
		long jNew = Long.MIN_VALUE;
		Object valueLong = makeValueLong.invoke(j);

		assertEquals(getLong.invoke(valueLong), j);

		valueLong = withLong.invoke(valueLong, jNew);
		assertEquals(getLong.invoke(valueLong), jNew);
	}

	/*
	 * Create a value type with a int primitive member
	 * 
	 * value ValueInt {
	 * 	int i;
	 * }
	 */
	@Test(priority=1)
	static public void testCreateValueInt() throws Throwable {
		String fields[] = {"i:I"};
		valueIntClass = ValueTypeGenerator.generateValueClass("ValueInt", fields);

		makeValueInt = lookup.findStatic(valueIntClass, "makeValue", MethodType.methodType(valueIntClass, int.class));

		getInt = generateGetter(valueIntClass, "i", int.class);
		withInt = generateWither(valueIntClass, "i", int.class);

		int i = Integer.MAX_VALUE;
		int iNew = Integer.MIN_VALUE;
		Object valueInt = makeValueInt.invoke(i);

		assertEquals(getInt.invoke(valueInt), i);

		valueInt = withInt.invoke(valueInt, iNew);
		assertEquals(getInt.invoke(valueInt), iNew);
	}

	/*
	 * Create a value type with a double primitive member
	 * 
	 * value ValueDouble {
	 * 	double d;
	 * }
	 */
	@Test(priority=1)
	static public void testCreateValueDouble() throws Throwable {
		String fields[] = {"d:D"};
		valueDoubleClass = ValueTypeGenerator.generateValueClass("ValueDouble", fields);

		makeValueDouble = lookup.findStatic(valueDoubleClass, "makeValue",
				MethodType.methodType(valueDoubleClass, double.class));

		getDouble = generateGetter(valueDoubleClass, "d", double.class);
		withDouble = generateWither(valueDoubleClass, "d", double.class);

		double d = Double.MAX_VALUE;
		double dNew = Double.MIN_VALUE;
		Object valueDouble = makeValueDouble.invoke(d);

		assertEquals(getDouble.invoke(valueDouble), d);

		valueDouble = withDouble.invoke(valueDouble, dNew);
		assertEquals(getDouble.invoke(valueDouble), dNew);
	}

	/*
	 * Create a value type with a float primitive member
	 * 
	 * value ValueFloat {
	 * 	float f;
	 * }
	 */
	@Test(priority=1)
	static public void testCreateValueFloat() throws Throwable {
		String fields[] = {"f:F"};
		valueFloatClass = ValueTypeGenerator.generateValueClass("ValueFloat", fields);

		makeValueFloat = lookup.findStatic(valueFloatClass, "makeValue",
				MethodType.methodType(valueFloatClass, float.class));

		getFloat = generateGetter(valueFloatClass, "f", float.class);
		withFloat = generateWither(valueFloatClass, "f", float.class);

		float f = Float.MAX_VALUE;
		float fNew = Float.MIN_VALUE;
		Object valueFloat = makeValueFloat.invoke(f);

		assertEquals(getFloat.invoke(valueFloat), f);

		valueFloat = withFloat.invoke(valueFloat, fNew);
		assertEquals(getFloat.invoke(valueFloat), fNew);
	}

	/*
	 * Create a value type with a reference member
	 * 
	 * value ValueObject {
	 * 	Object val;
	 * }
	 */
	@Test(priority=1)
	static public void testCreateValueObject() throws Throwable {
		String fields[] = {"val:Ljava/lang/Object;:value"};

		valueObjectClass = ValueTypeGenerator.generateValueClass("ValueObject", fields);

		makeValueObject = lookup.findStatic(valueObjectClass, "makeValue",
				MethodType.methodType(valueObjectClass, Object.class));

		Object val = (Object)0xEEFFEEFF;
		Object valNew = (Object)0xFFEEFFEE;

		getObject = generateGetter(valueObjectClass, "val", Object.class);
		withObject = generateWither(valueObjectClass, "val", Object.class);

		Object valueObject = makeValueObject.invoke(val);

		assertEquals(getObject.invoke(valueObject), val);

		valueObject = withObject.invoke(valueObject, valNew);
		assertEquals(getObject.invoke(valueObject), valNew);
	}

	/*
	 * Create an assorted value type with long alignment
	 * 
	 * value AssortedValueWithLongAlignment {
	 *	flattened Point2D point;
	 *	flattened Line2D line;
	 *	flattened ValueObject o;
	 *	flattened ValueLong l;
	 *	flattened ValueDouble d;
	 *	flattened ValueInt i;
	 *	flattened Triangle2D tri;
	 * }
	 */
	@Test(priority=4)
	static public void testCreateAssortedValueWithLongAlignment() throws Throwable {
		assortedValueWithLongAlignmentClass = ValueTypeGenerator.generateValueClass("AssortedValueWithLongAlignment", typeWithLongAlignmentFields);

		makeAssortedValueWithLongAlignment = lookup.findStatic(assortedValueWithLongAlignmentClass,
				"makeValueGeneric", MethodType.methodType(assortedValueWithLongAlignmentClass, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class, Object.class));
		/*
		 * Getters are created in array getterAndWither[i][0] according to the order of fields i
		 * Withers are created in array getterAndWither[i][1] according to the order of fields i
		 */
		assortedValueWithLongAlignmentGetterAndWither = generateGenericGetterAndWither(assortedValueWithLongAlignmentClass, typeWithLongAlignmentFields);
		Object assortedValueWithLongAlignment = createAssorted(makeAssortedValueWithLongAlignment, typeWithLongAlignmentFields);
		checkFieldAccessMHOfAssortedType(assortedValueWithLongAlignmentGetterAndWither, assortedValueWithLongAlignment, typeWithLongAlignmentFields, true);
	}

	/*
	 * Create an assorted refType with long alignment
	 * 
	 * class AssortedReftWithLongAlignment {
	 *	flattened Point2D point;
	 *	flattened Line2D line;
	 *	flattened ValueObject o;
	 *	flattened ValueLong l;
	 *	flattened ValueDouble d;
	 *	flattened ValueInt i;
	 *	flattened Triangle2D tri;
	 * }
	 */
	@Test(priority=4)
	static public void testCreateAssortedRefWithLongAlignment() throws Throwable {
		assortedRefWithLongAlignmentClass = ValueTypeGenerator.generateRefClass("AssortedRefWithLongAlignment", typeWithLongAlignmentFields);

		makeAssortedRefWithLongAlignment = lookup.findStatic(assortedRefWithLongAlignmentClass,
				"makeRefGeneric", MethodType.methodType(assortedRefWithLongAlignmentClass, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class));

		/*
		 * Getters are created in array getterAndSetter[i][0] according to the order of fields i
		 * Setters are created in array getterAndSetter[i][1] according to the order of fields i
		 */
		assortedRefWithLongAlignmentGetterAndSetter = generateGenericGetterAndSetter(assortedRefWithLongAlignmentClass, typeWithLongAlignmentFields);
		Object assortedRefWithLongAlignment = createAssorted(makeAssortedRefWithLongAlignment, typeWithLongAlignmentFields);
		checkFieldAccessMHOfAssortedType(assortedRefWithLongAlignmentGetterAndSetter, assortedRefWithLongAlignment, typeWithLongAlignmentFields, false);
	}

	/*
	 * Create an assorted value type with object alignment 
	 * 
	 * value AssortedValueWithObjectAlignment {
	 * 	flattened Triangle2D tri;
	 * 	flattened Point2D point;
	 * 	flattened Line2D line;
	 * 	flattened ValueObject o;
	 * 	flattened ValueInt i;
	 * 	flattened ValueFloat f;
	 * 	flattened Triangle2D tri2;
	 * }
	 */
	@Test(priority=4)
	static public void testCreateAssortedValueWithObjectAlignment() throws Throwable {
		assortedValueWithObjectAlignmentClass = ValueTypeGenerator
			.generateValueClass("AssortedValueWithObjectAlignment", typeWithObjectAlignmentFields);

		makeAssortedValueWithObjectAlignment = lookup.findStatic(assortedValueWithObjectAlignmentClass,
			"makeValueGeneric", MethodType.methodType(assortedValueWithObjectAlignmentClass, Object.class,
					Object.class, Object.class, Object.class, Object.class, Object.class, Object.class));
		/*
		 * Getters are created in array getterAndSetter[i][0] according to the order of fields i
		 * Setters are created in array getterAndSetter[i][1] according to the order of fields i
		 */
		assortedValueWithObjectAlignmentGetterAndWither = generateGenericGetterAndWither(assortedValueWithObjectAlignmentClass, typeWithObjectAlignmentFields);

		Object assortedValueWithObjectAlignment = createAssorted(makeAssortedValueWithObjectAlignment, typeWithObjectAlignmentFields);
		checkFieldAccessMHOfAssortedType(assortedValueWithObjectAlignmentGetterAndWither, assortedValueWithObjectAlignment, typeWithObjectAlignmentFields, true);
	}

	/*
	 * Create an assorted refType with object alignment 
	 * 
	 * class AssortedRefWithObjectAlignment {
	 * 	flattened Triangle2D tri;
	 * 	flattened Point2D point;
	 * 	flattened Line2D line;
	 * 	flattened ValueObject o;
	 * 	flattened ValueInt i;
	 * 	flattened ValueFloat f;
	 * 	flattened Triangle2D tri2;
	 * }
	 */
	@Test(priority=4)
	static public void testCreateAssortedRefWithObjectAlignment() throws Throwable {
		Class assortedRefWithObjectAlignmentClass = ValueTypeGenerator.generateRefClass("AssortedRefWithObjectAlignment", typeWithObjectAlignmentFields);

		MethodHandle makeAssortedRefWithObjectAlignment = lookup.findStatic(assortedRefWithObjectAlignmentClass,
				"makeRefGeneric", MethodType.methodType(assortedRefWithObjectAlignmentClass, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class));
		/*
		 * Getters are created in array getterAndSetter[i][0] according to the order of fields i
		 * Setters are created in array getterAndSetter[i][1] according to the order of fields i
		 */
		MethodHandle[][] getterAndSetter = generateGenericGetterAndSetter(assortedRefWithObjectAlignmentClass, typeWithObjectAlignmentFields);

		Object assortedRefWithObjectAlignment = createAssorted(makeAssortedRefWithObjectAlignment, typeWithObjectAlignmentFields);
		checkFieldAccessMHOfAssortedType(getterAndSetter, assortedRefWithObjectAlignment, typeWithObjectAlignmentFields, false);
	}

	/*
	 * Create an assorted value type with single alignment 
	 * 
	 * value AssortedValueWithSingleAlignment {
	 * 	flattened Triangle2D tri;
	 * 	flattened Point2D point;
	 * 	flattened Line2D line;
	 * 	flattened ValueInt i;
	 * 	flattened ValueFloat f;
	 * 	flattened Triangle2D tri2;
	 * }
	 */
	@Test(priority=4)
	static public void testCreateAssortedValueWithSingleAlignment() throws Throwable {
		assortedValueWithSingleAlignmentClass = ValueTypeGenerator.generateValueClass("AssortedValueWithSingleAlignment", typeWithSingleAlignmentFields);

		makeAssortedValueWithSingleAlignment = lookup.findStatic(assortedValueWithSingleAlignmentClass,
			"makeValueGeneric", MethodType.methodType(assortedValueWithSingleAlignmentClass, Object.class,
					Object.class, Object.class, Object.class, Object.class, Object.class));
		/*
		 * Getters are created in array getterAndSetter[i][0] according to the order of fields i
		 * Setters are created in array getterAndSetter[i][1] according to the order of fields i
		 */
		assortedValueWithSingleAlignmentGetterAndWither = generateGenericGetterAndWither(assortedValueWithSingleAlignmentClass, typeWithSingleAlignmentFields);

		Object assortedValueWithSingleAlignment = createAssorted(makeAssortedValueWithSingleAlignment, typeWithSingleAlignmentFields);
		checkFieldAccessMHOfAssortedType(assortedValueWithSingleAlignmentGetterAndWither, assortedValueWithSingleAlignment, typeWithSingleAlignmentFields, true);
	}

	/*
	 * Create an assorted refType with single alignment 
	 * 
	 * class AssortedRefWithSingleAlignment {
	 * 	flattened Triangle2D tri;
	 * 	flattened Point2D point;
	 * 	flattened Line2D line;
	 * 	flattened ValueInt i;
	 * 	flattened ValueFloat f;
	 * 	flattened Triangle2D tri2;
	 * }
	 */
	@Test(priority=4)
	static public void testCreateAssortedRefWithSingleAlignment() throws Throwable {
		Class assortedRefWithSingleAlignmentClass = ValueTypeGenerator.generateRefClass("AssortedRefWithSingleAlignment", typeWithSingleAlignmentFields);

		MethodHandle makeAssortedRefWithSingleAlignment = lookup.findStatic(assortedRefWithSingleAlignmentClass,
				"makeRefGeneric", MethodType.methodType(assortedRefWithSingleAlignmentClass, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class));
		/*
		 * Getters are created in array getterAndSetter[i][0] according to the order of fields i
		 * Setters are created in array getterAndSetter[i][1] according to the order of fields i
		 */
		MethodHandle[][] getterAndSetter = generateGenericGetterAndSetter(assortedRefWithSingleAlignmentClass, typeWithSingleAlignmentFields);

		Object assortedRefWithSingleAlignment = createAssorted(makeAssortedRefWithSingleAlignment, typeWithSingleAlignmentFields);
		checkFieldAccessMHOfAssortedType(getterAndSetter, assortedRefWithSingleAlignment, typeWithSingleAlignmentFields, false);
	}

	/*
	 * Create a large valueType with 16 valuetypes as its members
	 * Create a mega valueType with 16 large valuetypes as its members 
	 *
	 * value LargeObject {
	 *	flattened ValueObject val1;
	 *	flattened ValueObject val2;
	 *	...
	 *	flattened ValueObject val16;
	 * }
	 *
	 * value MegaObject {
	 *	flattened LargeObject val1;
	 *	... 
	 *	flattened LargeObject val16;
	 * }
	 */
	@Test(priority=2)
	static public void testCreateLargeObjectAndMegaValue() throws Throwable {
		String largeFields[] = {
				"val1:QValueObject;:value",
				"val2:QValueObject;:value",
				"val3:QValueObject;:value",
				"val4:QValueObject;:value",
				"val5:QValueObject;:value",
				"val6:QValueObject;:value",
				"val7:QValueObject;:value",
				"val8:QValueObject;:value",
				"val9:QValueObject;:value",
				"val10:QValueObject;:value",
				"val11:QValueObject;:value",
				"val12:QValueObject;:value",
				"val13:QValueObject;:value",
				"val14:QValueObject;:value",
				"val15:QValueObject;:value",
				"val16:QValueObject;:value"};

		largeObjectValueClass = ValueTypeGenerator.generateValueClass("LargeObject", largeFields);
		makeLargeObjectValue = lookup.findStatic(largeObjectValueClass, "makeValueGeneric",
				MethodType.methodType(largeObjectValueClass, Object.class, Object.class, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class, Object.class));
		/*
		 * Getters are created in array getterAndWither[i][0] according to the order of fields i
		 * Withers are created in array getterAndWither[i][1] according to the order of fields i
		 */
		MethodHandle[][] getterAndWither = generateGenericGetterAndWither(largeObjectValueClass, largeFields);

		getObjects = new MethodHandle[16];
		for (int i = 0; i < 16; i++) {
			getObjects[i] = getterAndWither[i][0];
		}

		Object largeObjectValue = createAssorted(makeLargeObjectValue, largeFields);
		checkFieldAccessMHOfAssortedType(getterAndWither, largeObjectValue, largeFields, true);

		/*
		 * create MegaObject
		 */
		String megaFields[] = {
				"val1:QLargeObject;:value",
				"val2:QLargeObject;:value",
				"val3:QLargeObject;:value",
				"val4:QLargeObject;:value",
				"val5:QLargeObject;:value",
				"val6:QLargeObject;:value",
				"val7:QLargeObject;:value",
				"val8:QLargeObject;:value",
				"val9:QLargeObject;:value",
				"val10:QLargeObject;:value",
				"val11:QLargeObject;:value",
				"val12:QLargeObject;:value",
				"val13:QLargeObject;:value",
				"val14:QLargeObject;:value",
				"val15:QLargeObject;:value",
				"val16:QLargeObject;:value"};

		megaObjectValueClass = ValueTypeGenerator.generateValueClass("MegaObject", megaFields);
		makeMegaObjectValue = lookup.findStatic(megaObjectValueClass, "makeValueGeneric",
				MethodType.methodType(megaObjectValueClass, Object.class, Object.class, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class, Object.class));

		/*
		 * Getters are created in array getterAndWither[i][0] according to the order of fields i
		 * Withers are created in array getterAndWither[i][1] according to the order of fields i
		 */
		MethodHandle[][] megaGetterAndWither = generateGenericGetterAndWither(megaObjectValueClass, megaFields);
		Object megaObject = createAssorted(makeMegaObjectValue, megaFields);
		checkFieldAccessMHOfAssortedType(megaGetterAndWither, megaObject, megaFields, true);
	}

	/*
	 * Create a large refType with 16 valuetypes as its members
	 * Create a mega refType with 16 large valuetypes as its members 
	 *
	 * class LargeRef {
	 *	flattened ValObject val1;
	 *	flattened ValObject val2;
	 *	...
	 *	flattened ValObject val16;
	 * }
	 *
	 * class MegaObject {
	 *	flattened LargeObject val1;
	 *	... 
	 *	flattened LargeObject val16;
	 * }
	 */
	@Test(priority=3)
	static public void testCreateLargeObjectAndMegaRef() throws Throwable {
		String largeFields[] = {
				"val1:QValueObject;:value",
				"val2:QValueObject;:value",
				"val3:QValueObject;:value",
				"val4:QValueObject;:value",
				"val5:QValueObject;:value",
				"val6:QValueObject;:value",
				"val7:QValueObject;:value",
				"val8:QValueObject;:value",
				"val9:QValueObject;:value",
				"val10:QValueObject;:value",
				"val11:QValueObject;:value",
				"val12:QValueObject;:value",
				"val13:QValueObject;:value",
				"val14:QValueObject;:value",
				"val15:QValueObject;:value",
				"val16:QValueObject;:value"};

		Class largeRefClass = ValueTypeGenerator.generateRefClass("LargeRef", largeFields);
		MethodHandle makeLargeObjectRef = lookup.findStatic(largeRefClass, "makeRefGeneric",
				MethodType.methodType(largeRefClass, Object.class, Object.class, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class, Object.class));
		/*
		 * Getters are created in array getterAndSetter[i][0] according to the order of fields i
		 * Setters are created in array getterAndSetter[i][1] according to the order of fields i
		 */
		MethodHandle[][] getterAndSetter = new MethodHandle[largeFields.length][2];
		for (int i = 0; i < largeFields.length; i++) {
			String field = (largeFields[i].split(":"))[0];
			getterAndSetter[i][0] = generateGenericGetter(largeRefClass, field);
			getterAndSetter[i][1] = generateGenericSetter(largeRefClass, field);
		}

		Object largeObjectRef = createAssorted(makeLargeObjectRef, largeFields);
		checkFieldAccessMHOfAssortedType(getterAndSetter, largeObjectRef, largeFields, false);

		/*
		 * create MegaObject
		 */
		String megaFields[] = {
				"val1:QLargeObject;:value",
				"val2:QLargeObject;:value",
				"val3:QLargeObject;:value",
				"val4:QLargeObject;:value",
				"val5:QLargeObject;:value",
				"val6:QLargeObject;:value",
				"val7:QLargeObject;:value",
				"val8:QLargeObject;:value",
				"val9:QLargeObject;:value",
				"val10:QLargeObject;:value",
				"val11:QLargeObject;:value",
				"val12:QLargeObject;:value",
				"val13:QLargeObject;:value",
				"val14:QLargeObject;:value",
				"val15:QLargeObject;:value",
				"val16:QLargeObject;:value"};
		Class megaObjectClass = ValueTypeGenerator.generateRefClass("MegaRef", megaFields);
		MethodHandle makeMegaObjectRef = lookup.findStatic(megaObjectClass, "makeRefGeneric",
				MethodType.methodType(megaObjectClass, Object.class, Object.class, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class, Object.class));

		/*
		 * Getters are created in array getterAndSetter[i][0] according to the order of fields i
		 * Setters are created in array getterAndSetter[i][1] according to the order of fields i
		 */
		MethodHandle[][] megaGetterAndSetter = new MethodHandle[megaFields.length][2];
		for (int i = 0; i < megaFields.length; i++) {
			String field = (megaFields[i].split(":"))[0];
			megaGetterAndSetter[i][0] = generateGenericGetter(megaObjectClass, field);
			megaGetterAndSetter[i][1] = generateGenericSetter(megaObjectClass, field);
		}

		Object megaObjectRef = createAssorted(makeMegaObjectRef, megaFields);
		checkFieldAccessMHOfAssortedType(megaGetterAndSetter, megaObjectRef, megaFields, false);
	}

	/*
	 * Create a value type and read the fields before
	 * they are set. The test should Verify that the
	 * flattenable fields are set to the default values.
	 * NULL should never be observed for Qtypes.
	 */
	@Test(priority=4)
	static public void testDefaultValues() throws Throwable {
		/* Test with assorted value object with long alignment */
		MethodHandle makeValueTypeDefaultValueWithLong = lookup.findStatic(assortedValueWithLongAlignmentClass,
				"makeValueTypeDefaultValue", MethodType.methodType(assortedValueWithLongAlignmentClass));

		Object assortedValueWithLongAlignment = makeValueTypeDefaultValueWithLong.invoke();
		for (int i = 0; i < 7; i++) {
			assertNotNull(assortedValueWithLongAlignmentGetterAndWither[i][0].invoke(assortedValueWithLongAlignment));
		}

		/* Test with assorted ref object with long alignment */
		MethodHandle makeRefDefaultValueWithLong = lookup.findStatic(assortedRefWithLongAlignmentClass,
				"makeRefDefaultValue", MethodType.methodType(assortedRefWithLongAlignmentClass));
		Object assortedRefWithLongAlignment = makeRefDefaultValueWithLong.invoke();
		for (int i = 0; i < 7; i++) {
			assertNotNull(assortedRefWithLongAlignmentGetterAndSetter[i][0].invoke(assortedRefWithLongAlignment));
		}

		/* Test with flattened line 2D */
		MethodHandle makeDefaultValueFlattenedLine2D = lookup.findStatic(flattenedLine2DClass, "makeValueTypeDefaultValue", MethodType.methodType(flattenedLine2DClass));
		Object lineObject = makeDefaultValueFlattenedLine2D.invoke();
		assertNotNull(getFlatSt.invoke(lineObject));
		assertNotNull(getFlatEn.invoke(lineObject));

		/* Test with triangle 2D */
		MethodHandle makeDefaultValueTriangle2D = lookup.findStatic(triangle2DClass, "makeValueTypeDefaultValue", MethodType.methodType(triangle2DClass));
		Object triangleObject = makeDefaultValueTriangle2D.invoke();
		assertNotNull(getV1.invoke(triangleObject));
		assertNotNull(getV2.invoke(triangleObject));
		assertNotNull(getV3.invoke(triangleObject));
	}
	

	@Test(priority=4)
	static public void testStaticFieldsWithSingleAlignment() throws Throwable {
		String fields[] = {
			"tri:QTriangle2D;:static",
			"point:QPoint2D;:static",
			"line:QFlattenedLine2D;:static",
			"i:QValueInt;:static",
			"f:QValueFloat;:static",
			"tri2:QTriangle2D;:static"};
		Class ClassWithOnlyStaticFieldsWithSingleAlignment = ValueTypeGenerator.generateValueClass("ClassWithOnlyStaticFieldsWithSingleAlignment", fields);
		MethodHandle[][] StaticFieldsWithSingleAlignmentGenericGetterAndSetter = generateStaticGenericGetterAndSetter(ClassWithOnlyStaticFieldsWithSingleAlignment, fields);
		
		initializeStaticFields(ClassWithOnlyStaticFieldsWithSingleAlignment, StaticFieldsWithSingleAlignmentGenericGetterAndSetter, fields);
		checkFieldAccessMHOfStaticType(StaticFieldsWithSingleAlignmentGenericGetterAndSetter, fields);
	}

	@Test(priority=4)
	static public void testStaticFieldsWithLongAlignment() throws Throwable {
		String fields[] = {
			"point:QPoint2D;:static",
			"line:QFlattenedLine2D;:static",
			"o:QValueObject;:static",
			"l:QValueLong;:static",
			"d:QValueDouble;:static",
			"i:QValueInt;:static",
			"tri:QTriangle2D;:static"};
		Class ClassWithOnlyStaticFieldsWithLongAlignment = ValueTypeGenerator.generateValueClass("ClassWithOnlyStaticFieldsWithLongAlignment", fields);
		MethodHandle[][] StaticFieldsWithLongAlignmentGenericGetterAndSetter = generateStaticGenericGetterAndSetter(ClassWithOnlyStaticFieldsWithLongAlignment, fields);
		
		initializeStaticFields(ClassWithOnlyStaticFieldsWithLongAlignment, StaticFieldsWithLongAlignmentGenericGetterAndSetter, fields);
		checkFieldAccessMHOfStaticType(StaticFieldsWithLongAlignmentGenericGetterAndSetter, fields);
	}

	@Test(priority=4)
	static public void testStaticFieldsWithObjectAlignment() throws Throwable {
		String fields[] = {
			"tri:QTriangle2D;:static",
			"point:QPoint2D;:static",
			"line:QFlattenedLine2D;:static",
			"o:QValueObject;:static",
			"i:QValueInt;:static",
			"f:QValueFloat;:static",
			"tri2:QTriangle2D;:static"};
		Class ClassWithOnlyStaticFieldsWithObjectAlignment = ValueTypeGenerator.generateValueClass("ClassWithOnlyStaticFieldsWithObjectAlignment", fields);
		MethodHandle[][] StaticFieldsWithObjectAlignmentGenericGetterAndSetter = generateStaticGenericGetterAndSetter(ClassWithOnlyStaticFieldsWithObjectAlignment, fields);

		initializeStaticFields(ClassWithOnlyStaticFieldsWithObjectAlignment, StaticFieldsWithObjectAlignmentGenericGetterAndSetter, fields);
		checkFieldAccessMHOfStaticType(StaticFieldsWithObjectAlignmentGenericGetterAndSetter, fields);
	}

	/*
	 * Create large number of value types and instantiate them 
	 * 
	 * value Point2D {
	 * 	int x;
	 * 	int y;
	 * }
	 */
	@Test(enabled = false, priority=1)
	static public void testCreateLargeNumberOfPoint2D() throws Throwable {
		String fields[] = {"x:I", "y:I"};
		String className = "Point2D";
		for (int valueIndex = 0; valueIndex < 200000; valueIndex++) {
			className =  "Point2D" + valueIndex;		
			point2DClass = ValueTypeGenerator.generateValueClass(className, fields);
			/* findStatic will trigger class resolution */
			makePoint2D = lookup.findStatic(point2DClass, "makeValue", MethodType.methodType(point2DClass, int.class, int.class));
		}
	}

	/*
	 * Create Array Objects with Point Class without initialization
	 * The array should be set to a Default Value.
	 */
	@Test(priority=4)
	static public void testDefaultValueInPointArray() throws Throwable {
		Object pointArray = Array.newInstance(point2DClass, genericArraySize);
		for (int i = 0; i < genericArraySize; i++) {
			Object pointObject = Array.get(pointArray, i);
			assertNotNull(pointObject);
		}
	}

	/*
	 * Create Array Objects with Flattened Line without initialization
	 * Check the fields of each element in arrays. No field should be NULL.
	 */
	@Test(priority=4)
	static public void testDefaultValueInLineArray() throws Throwable {
		Object flattenedLineArray = Array.newInstance(flattenedLine2DClass, genericArraySize);
		for (int i = 0; i < genericArraySize; i++) {
			Object lineObject = Array.get(flattenedLineArray, i);
			assertNotNull(lineObject);
			assertNotNull(getFlatSt.invoke(lineObject));
			assertNotNull(getFlatEn.invoke(lineObject));
		}
	}

	/*
	 * Create Array Objects with triangle class without initialization
	 * Check the fields of each element in arrays. No field should be NULL.
	 */
	@Test(priority=4)
	static public void testDefaultValueInTriangleArray() throws Throwable {
		Object triangleArray = Array.newInstance(triangle2DClass, genericArraySize);
		for (int i = 0; i < genericArraySize; i++) {
			Object triangleObject = Array.get(triangleArray, i);
			assertNotNull(triangleObject);
			assertNotNull(getV1.invoke(triangleObject));
			assertNotNull(getV2.invoke(triangleObject));
			assertNotNull(getV3.invoke(triangleObject));
		}
	}

	/*
	 * Create an Array Object with assortedValueWithLongAlignment class without initialization
	 * Check the fields of each element in arrays. No field should be NULL.
	 */
	@Test(priority=4)
	static public void testDefaultValueInAssortedValueWithLongAlignmentArray() throws Throwable {
		Object assortedValueWithLongAlignmentArray = Array.newInstance(assortedValueWithLongAlignmentClass, genericArraySize);
		for (int i = 0; i < genericArraySize; i++) {
			Object assortedValueWithLongAlignmentObject = Array.get(assortedValueWithLongAlignmentArray, i);
			assertNotNull(assortedValueWithLongAlignmentObject);
			for (int j = 0; j < 7; j++) {
				assertNotNull(assortedValueWithLongAlignmentGetterAndWither[j][0].invoke(assortedValueWithLongAlignmentObject));
			}
		}
	}

	/*
	 * Create a 2D array of valueTypes, verify that the default elements are null. 
	 */
	@Test(priority=5)
	static public void testMultiDimentionalArrays() throws Throwable {
		Class assortedValueWithLongAlignment2DClass = Array.newInstance(assortedValueWithLongAlignmentClass, 1).getClass();
		Class assortedValueWithSingleAlignment2DClass = Array.newInstance(assortedValueWithSingleAlignmentClass, 1).getClass();
		
		Object assortedRefWithLongAlignment2DArray = Array.newInstance(assortedValueWithLongAlignment2DClass, genericArraySize);
		Object assortedRefWithSingleAlignment2DArray = Array.newInstance(assortedValueWithSingleAlignment2DClass, genericArraySize);
		
		for (int i = 0; i < genericArraySize; i++) {
			Object ref = Array.get(assortedRefWithLongAlignment2DArray, i);
			assertNull(ref);
			
			ref = Array.get(assortedRefWithSingleAlignment2DArray, i);
			assertNull(ref);
		}
	}
	
	/*
	 * Create an assortedRefWithLongAlignment Array
	 * Since it's ref type, the array should be filled with nullptrs
	 */
	@Test(priority=4)
	static public void testDefaultValueInAssortedRefWithLongAlignmentArray() throws Throwable {
		Object assortedRefWithLongAlignmentArray = Array.newInstance(assortedRefWithLongAlignmentClass, genericArraySize);
		for (int i = 0; i < genericArraySize; i++) {
			Object assortedRefWithLongAlignmentObject = Array.get(assortedRefWithLongAlignmentArray, i);
			assertNull(assortedRefWithLongAlignmentObject);
		}
	}

	/*
	 * Ensure that casting null to a value type class will throw a null pointer exception 
	 */
	@Test(priority=1, expectedExceptions=NullPointerException.class)
	static public void testCheckCastValueTypeOnNull() throws Throwable {
		String fields[] = {"longField:J"};
		Class valueClass = ValueTypeGenerator.generateValueClass("TestCheckCastValueTypeOnNull", fields);
		MethodHandle checkCastValueTypeOnNull = lookup.findStatic(valueClass, "testCheckCastValueTypeOnNull", MethodType.methodType(Object.class));
		checkCastValueTypeOnNull.invoke();
	}

	/*
	 * Ensure that casting a non null value type to a valid value type will pass
	 */
	@Test(priority=1)
	static public void testCheckCastValueTypeOnNonNullType() throws Throwable {
		String fields[] = {"longField:J"};
		Class valueClass = ValueTypeGenerator.generateValueClass("TestCheckCastValueTypeOnNonNullType", fields);
		MethodHandle checkCastValueTypeOnNonNullType = lookup.findStatic(valueClass, "testCheckCastValueTypeOnNonNullType", MethodType.methodType(Object.class));
		checkCastValueTypeOnNonNullType.invoke();
	}

	/*
	 * Ensure that casting null to a reference type class will pass
	 */
	@Test(priority=1)
	static public void testCheckCastRefClassOnNull() throws Throwable {
		String fields[] = {"longField:J"};
		Class refClass = ValueTypeGenerator.generateRefClass("TestCheckCastRefClassOnNull", fields);
		MethodHandle checkCastRefClassOnNull = lookup.findStatic(refClass, "testCheckCastRefClassOnNull", MethodType.methodType(Object.class));
		checkCastRefClassOnNull.invoke();
	}


	// The three following tests can be used to verify that flattened value types in arrays are handled properly by the GC. In the
	// current state, these tests should pass when flattened is disabled but failed when it is enabled.
	/*
	 * Maintain a buffer of flattened arrays with long-aligned valuetypes while keeping a certain amount of classes alive at any 
	 * single time. This forces the GC to unload the classes.
	 */
	@Test(enabled = false, priority=5)
	static public void testValueWithLongAlignmentGCScanning() throws Throwable {
		ArrayList<Object> longAlignmentArrayList = new ArrayList<Object>(objectGCScanningIterationCount);
		for (int i = 0; i < objectGCScanningIterationCount; i++) {
			Object newLongAlignmentArray = Array.newInstance(assortedValueWithLongAlignmentClass, genericArraySize);
			for (int j = 0; j < genericArraySize; j++) {
				Object assortedValueWithLongAlignment = createAssorted(makeAssortedValueWithLongAlignment, typeWithLongAlignmentFields);
				Array.set(newLongAlignmentArray, j, assortedValueWithLongAlignment);
			}
			longAlignmentArrayList.add(newLongAlignmentArray);
		}

		System.gc();
		System.gc();

		for (int i = 0; i < objectGCScanningIterationCount; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				checkFieldAccessMHOfAssortedType(assortedValueWithLongAlignmentGetterAndWither, Array.get(longAlignmentArrayList.get(i), j), typeWithLongAlignmentFields, true);
			}
		}
	}

	/*
	 * Maintain a buffer of flattened arrays with object-aligned valuetypes while keeping a certain amount of classes alive at any 
	 * single time. This forces the GC to unload the classes.
	 */
	@Test(enabled = false, priority=5)
	static public void testValueWithObjectAlignmentGCScanning() throws Throwable {
		ArrayList<Object> objectAlignmentArrayList = new ArrayList<Object>(objectGCScanningIterationCount);
		for (int i = 0; i < objectGCScanningIterationCount; i++) {
			Object newObjectAlignmentArray = Array.newInstance(assortedValueWithObjectAlignmentClass, genericArraySize);
			for (int j = 0; j < genericArraySize; j++) {
				Object assortedValueWithObjectAlignment = createAssorted(makeAssortedValueWithObjectAlignment, typeWithObjectAlignmentFields);
				Array.set(newObjectAlignmentArray, j, assortedValueWithObjectAlignment);
			}
			objectAlignmentArrayList.add(newObjectAlignmentArray);
		}

		System.gc();
		System.gc();

		for (int i = 0; i < objectGCScanningIterationCount; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				checkFieldAccessMHOfAssortedType(assortedValueWithObjectAlignmentGetterAndWither, Array.get(objectAlignmentArrayList.get(i), j), typeWithObjectAlignmentFields, true);
			}
		}
	}

	/*
	 * Maintain a buffer of flattened arrays with single-aligned valuetypes while keeping a certain amount of classes alive at any 
	 * single time. This forces the GC to unload the classes.
	 */
	@Test(enabled = false, priority=5)
	static public void testValueWithSingleAlignmentGCScanning() throws Throwable {
		ArrayList<Object> singleAlignmentArrayList = new ArrayList<Object>(objectGCScanningIterationCount);
		for (int i = 0; i < objectGCScanningIterationCount; i++) {
			Object newSingleAlignmentArray = Array.newInstance(assortedValueWithSingleAlignmentClass, genericArraySize);
			for (int j = 0; j < genericArraySize; j++) {
				Object assortedValueWithSingleAlignment = createAssorted(makeAssortedValueWithSingleAlignment, typeWithSingleAlignmentFields);
				Array.set(newSingleAlignmentArray, j, assortedValueWithSingleAlignment);
			}
			singleAlignmentArrayList.add(newSingleAlignmentArray);
		}

		System.gc();
		System.gc();

		for (int i = 0; i < objectGCScanningIterationCount; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				checkFieldAccessMHOfAssortedType(assortedValueWithSingleAlignmentGetterAndWither, Array.get(singleAlignmentArrayList.get(i), j), typeWithSingleAlignmentFields, true);
			}
		}
	}

	@Test(priority=1)
	static public void testFlattenedFieldInitSequence() throws Throwable {
		String fields[] = {"x:I", "y:I"};
		Class nestAClass = ValueTypeGenerator.generateValueClass("NestedA", fields);
		
		String fields2[] = {"a:QNestedA;", "b:QNestedA;"};
		Class nestBClass = ValueTypeGenerator.generateValueClass("NestedB", fields2);
		
		String fields3[] = {"c:QNestedB;", "d:QNestedB;"};
		Class containerCClass = ValueTypeGenerator.generateValueClass("ContainerC", fields3);
		
		MethodHandle defaultValueContainerC = lookup.findStatic(containerCClass, "makeValueTypeDefaultValue", MethodType.methodType(containerCClass));
		
		Object containerC = defaultValueContainerC.invoke();
		
		MethodHandle getC = generateGenericGetter(containerCClass, "c");
		MethodHandle getD = generateGenericGetter(containerCClass, "d");
		MethodHandle getA = generateGenericGetter(nestBClass, "a");
		MethodHandle getB = generateGenericGetter(nestBClass, "b");
		
		assertNotNull(getC.invoke(containerC));
		assertNotNull(getA.invoke(getC.invoke(containerC)));
		assertNotNull(getB.invoke(getC.invoke(containerC)));
		
		assertNotNull(getD.invoke(containerC));
		assertNotNull(getA.invoke(getD.invoke(containerC)));
		assertNotNull(getB.invoke(getD.invoke(containerC)));
	}
	
	
	static MethodHandle generateGetter(Class<?> clazz, String fieldName, Class<?> fieldType) {
		try {
			return lookup.findVirtual(clazz, "get"+fieldName, MethodType.methodType(fieldType));
		} catch (IllegalAccessException | SecurityException | NullPointerException | NoSuchMethodException e) {
			e.printStackTrace();
		}
		return null;
	}
	
	static MethodHandle generateGenericGetter(Class<?> clazz, String fieldName) {
		try {
			return lookup.findVirtual(clazz, "getGeneric"+fieldName, MethodType.methodType(Object.class));
		} catch (IllegalAccessException | SecurityException | NullPointerException | NoSuchMethodException e) {
			e.printStackTrace();
		}
		return null;
	}
	
	static MethodHandle generateStaticGenericGetter(Class<?> clazz, String fieldName) {
		try {
			return lookup.findStatic(clazz, "getStaticGeneric"+fieldName, MethodType.methodType(Object.class));
		} catch (IllegalAccessException | SecurityException | NullPointerException | NoSuchMethodException e) {
			e.printStackTrace();
		}
		return null;
	}

	static MethodHandle generateSetter(Class clazz, String fieldName, Class fieldType) {
		try {
			return lookup.findVirtual(clazz, "set"+fieldName, MethodType.methodType(void.class, fieldType));
		} catch (IllegalAccessException | SecurityException | NullPointerException | NoSuchMethodException e) {
			e.printStackTrace();
		}
		return null;
	}
	
	static MethodHandle generateGenericSetter(Class clazz, String fieldName) {
		try {
			return lookup.findVirtual(clazz, "setGeneric"+fieldName, MethodType.methodType(void.class, Object.class));
		} catch (IllegalAccessException | SecurityException | NullPointerException | NoSuchMethodException e) {
			e.printStackTrace();
		}
		return null;
	}

	static MethodHandle generateStaticGenericSetter(Class clazz, String fieldName) {
		try {
			return lookup.findStatic(clazz, "setStaticGeneric"+fieldName, MethodType.methodType(void.class, Object.class));
		} catch (IllegalAccessException | SecurityException | NullPointerException | NoSuchMethodException e) {
			e.printStackTrace();
		}
		return null;
	}
	
	static MethodHandle generateWither(Class clazz, String fieldName, Class fieldType) {
		try {
			return lookup.findVirtual(clazz, "with"+fieldName, MethodType.methodType(clazz, fieldType));
		} catch (IllegalAccessException | SecurityException | NullPointerException | NoSuchMethodException e) {
			e.printStackTrace();
		}
		return null;
	}

	static MethodHandle generateGenericWither(Class clazz, String fieldName) {
		try {
			return lookup.findVirtual(clazz, "withGeneric"+fieldName, MethodType.methodType(clazz, Object.class));
		} catch (IllegalAccessException | SecurityException | NullPointerException | NoSuchMethodException e) {
			e.printStackTrace();
		}
		return null;
	}

	static long getFieldOffset(Class clazz, String field) {
		try {
			Field f = clazz.getDeclaredField(field);
			return myUnsafe.objectFieldOffset(f);
		} catch (Throwable t) {
			throw new RuntimeException(t);
		}
	}

	static MethodHandle[][] generateGenericGetterAndWither(Class clazz, String[] fields) {
		MethodHandle[][] getterAndWither = new MethodHandle[fields.length][2];
		for (int i = 0; i < fields.length; i++) {
			String field = (fields[i].split(":"))[0];
			getterAndWither[i][0] = generateGenericGetter(clazz, field);
			getterAndWither[i][1] = generateGenericWither(clazz, field);
		}
		return getterAndWither;
	}

	static MethodHandle[][] generateGenericGetterAndSetter(Class clazz, String[] fields) {
		MethodHandle[][] getterAndSetter = new MethodHandle[fields.length][2];
		for (int i = 0; i < fields.length; i++) {
			String field = (fields[i].split(":"))[0];
			getterAndSetter[i][0] = generateGenericGetter(clazz, field);
			getterAndSetter[i][1] = generateGenericSetter(clazz, field);
		}
		return getterAndSetter;
	}

	static MethodHandle[][] generateStaticGenericGetterAndSetter(Class clazz, String[] fields) {
		MethodHandle[][] getterAndSetter = new MethodHandle[fields.length][2];
		for (int i = 0; i < fields.length; i++) {
			String field = (fields[i].split(":"))[0];
			getterAndSetter[i][0] = generateStaticGenericGetter(clazz, field);
			getterAndSetter[i][1] = generateStaticGenericSetter(clazz, field);
		}
		return getterAndSetter;
	}

	static Object createPoint2D(int[] positions) throws Throwable {
		return makePoint2D.invoke(positions[0], positions[1]);
	}

	static Object createFlattenedLine2D(int[][] positions) throws Throwable {
		return makeFlattenedLine2D.invoke(makePoint2D.invoke(positions[0][0], positions[0][1]),
				makePoint2D.invoke(positions[1][0], positions[1][1]));
	}

	static Object createTriangle2D(int[][][] positions) throws Throwable {
		return makeTriangle2D.invoke(
				makeFlattenedLine2D.invoke(makePoint2D.invoke(positions[0][0][0], positions[0][0][1]),
						makePoint2D.invoke(positions[0][1][0], positions[0][1][1])),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(positions[1][0][0], positions[1][0][1]),
						makePoint2D.invoke(positions[1][1][0], positions[1][1][1])),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(positions[2][0][0], positions[2][0][1]),
						makePoint2D.invoke(positions[2][1][0], positions[2][1][1])));
	}

	static Object createLargeObject(Object arg) throws Throwable {
		Object[] args = new Object[16];
		for(int i = 0; i < 16; i++) {
			Object valueObject = makeValueObject.invoke(arg);
			args[i] = valueObject;
		}
		return makeLargeObjectValue.invokeWithArguments(args);
	}

	static Object createMegaObject(Object arg) throws Throwable {
		Object[] args = new Object[16];
		for(int i = 0; i < 16; i++) {
			Object valueObject = createLargeObject(arg);
			args[i] = valueObject;
		}
		return makeMegaObjectValue.invokeWithArguments(args);
	}

	static Object createAssorted(MethodHandle makeMethod, String[] fields) throws Throwable {
		return createAssorted(makeMethod, fields, null);
	}
	static Object createAssorted(MethodHandle makeMethod, String[] fields, Object[] initFields) throws Throwable {
		Object[] args = new Object[fields.length];
		boolean useInitFields = initFields != null;
		for (int i = 0; i < fields.length; i++) {
			String nameAndSigValue[] = fields[i].split(":");
			String signature = nameAndSigValue[1];
			switch (signature) {
			case "QPoint2D;":
				args[i] = createPoint2D(useInitFields ? (int[])initFields[i] : defaultPointPositions1);
				break;
			case "QFlattenedLine2D;":
				args[i] = createFlattenedLine2D(useInitFields ? (int[][])initFields[i] : defaultLinePositions1);
				break;
			case "QTriangle2D;":
				args[i] = createTriangle2D(useInitFields ? (int[][][])initFields[i] : defaultTrianglePositions);
				break;
			case "QValueInt;":
				args[i] = makeValueInt.invoke(useInitFields ? (int)initFields[i] : defaultInt);
				break;
			case "QValueFloat;":
				args[i] = makeValueFloat.invoke(useInitFields ? (float)initFields[i] : defaultFloat);
				break;
			case "QValueDouble;":
				args[i] = makeValueDouble.invoke(useInitFields ? (double)initFields[i] : defaultDouble);
				break;
			case "QValueObject;":
				args[i] = makeValueObject.invoke(useInitFields ? (Object)initFields[i] : defaultObject);
				break;
			case "QValueLong;":
				args[i] = makeValueLong.invoke(useInitFields ? (long)initFields[i] : defaultLong);
				break;
			case "QLargeObject;":
				args[i] = createLargeObject(useInitFields ? (Object)initFields[i] : defaultObject);
				break;
			default:
				args[i] = null;
				break;
			}
		}
		return makeMethod.invokeWithArguments(args);
	}

	static void initializeStaticFields(Class clazz, MethodHandle[][] getterAndSetter, String[] fields) throws Throwable {
		Object defaultValue = null;
		for (int i = 0; i < fields.length; i++) {
			String signature = (fields[i].split(":"))[1];
			switch (signature) {
			case "QPoint2D;":
				defaultValue = createPoint2D(defaultPointPositions1);
				break;
			case "QFlattenedLine2D;":
				defaultValue = createFlattenedLine2D(defaultLinePositions1);
				break;
			case "QTriangle2D;":
				defaultValue = createTriangle2D(defaultTrianglePositions);
				break;
			case "QValueInt;":
				defaultValue = makeValueInt.invoke(defaultInt);
				break;
			case "QValueFloat;":
				defaultValue = makeValueFloat.invoke(defaultFloat);
				break;
			case "QValueDouble;":
				defaultValue = makeValueDouble.invoke(defaultDouble);
				break;
			case "QValueObject;":
				defaultValue = makeValueObject.invoke(defaultObject);
				break;
			case "QValueLong;":
				defaultValue = makeValueLong.invoke(defaultLong);
				break;
			case "QLargeObject;":
				defaultValue = createLargeObject(defaultObject);
				break;
			default:
				defaultValue = null;
				break;
			}
			getterAndSetter[i][1].invoke(defaultValue);
		}
	}

	static Object checkFieldAccessMHOfAssortedType(MethodHandle[][] fieldAccessMHs, Object instance, String[] fields,
			boolean ifValue)
			throws Throwable {
		for (int i = 0; i < fields.length; i++) {
			String nameAndSigValue[] = fields[i].split(":");
			String signature = nameAndSigValue[1];
			switch (signature) {
			case "QPoint2D;":
				checkEqualPoint2D(fieldAccessMHs[i][0].invoke(instance), defaultPointPositions1);
				Object pointNew = createPoint2D(defaultPointPositionsNew);
				if (ifValue) {
					instance = fieldAccessMHs[i][1].invoke(instance, pointNew);
				} else {
					fieldAccessMHs[i][1].invoke(instance, pointNew);
				}
				checkEqualPoint2D(fieldAccessMHs[i][0].invoke(instance), defaultPointPositionsNew);
				break;
			case "QFlattenedLine2D;":
				checkEqualFlattenedLine2D(fieldAccessMHs[i][0].invoke(instance), defaultLinePositions1);
				Object lineNew = createFlattenedLine2D(defaultLinePositionsNew);
				if (ifValue) {
					instance = fieldAccessMHs[i][1].invoke(instance, lineNew);
				} else {
					fieldAccessMHs[i][1].invoke(instance, lineNew);
				}
				checkEqualFlattenedLine2D(fieldAccessMHs[i][0].invoke(instance), defaultLinePositionsNew);
				break;
			case "QTriangle2D;":
				checkEqualTriangle2D(fieldAccessMHs[i][0].invoke(instance), defaultTrianglePositions);
				Object triNew = createTriangle2D(defaultTrianglePositionsNew);
				if (ifValue) {
					instance = fieldAccessMHs[i][1].invoke(instance, triNew);
				} else {
					fieldAccessMHs[i][1].invoke(instance, triNew);
				}
				checkEqualTriangle2D(fieldAccessMHs[i][0].invoke(instance), defaultTrianglePositionsNew);
				break;
			case "QValueInt;":
				assertEquals(getInt.invoke(fieldAccessMHs[i][0].invoke(instance)), defaultInt);
				Object iNew = makeValueInt.invoke(defaultIntNew);
				if (ifValue) {
					instance = fieldAccessMHs[i][1].invoke(instance, iNew);
				} else {
					fieldAccessMHs[i][1].invoke(instance, iNew);
				}
				assertEquals(getInt.invoke(fieldAccessMHs[i][0].invoke(instance)), defaultIntNew);
				break;
			case "QValueFloat;":
				assertEquals(getFloat.invoke(fieldAccessMHs[i][0].invoke(instance)), defaultFloat);
				Object fNew = makeValueFloat.invoke(defaultFloatNew);
				if (ifValue) {
					instance = fieldAccessMHs[i][1].invoke(instance, fNew);
				} else {
					fieldAccessMHs[i][1].invoke(instance, fNew);
				}
				assertEquals(getFloat.invoke(fieldAccessMHs[i][0].invoke(instance)), defaultFloatNew);
				break;
			case "QValueDouble;":
				assertEquals(getDouble.invoke(fieldAccessMHs[i][0].invoke(instance)), defaultDouble);
				Object dNew = makeValueDouble.invoke(defaultDoubleNew);
				if (ifValue) {
					instance = fieldAccessMHs[i][1].invoke(instance, dNew);
				} else {
					fieldAccessMHs[i][1].invoke(instance, dNew);
				}
				assertEquals(getDouble.invoke(fieldAccessMHs[i][0].invoke(instance)), defaultDoubleNew);
				break;
			case "QValueObject;":
				assertEquals(getObject.invoke(fieldAccessMHs[i][0].invoke(instance)), defaultObject);
				Object oNew = makeValueObject.invoke(defaultObjectNew);
				if (ifValue) {
					instance = fieldAccessMHs[i][1].invoke(instance, oNew);
				} else {
					fieldAccessMHs[i][1].invoke(instance, oNew);
				}
				assertEquals(getObject.invoke(fieldAccessMHs[i][0].invoke(instance)), defaultObjectNew);
				break;
			case "QValueLong;":
				assertEquals(getLong.invoke(fieldAccessMHs[i][0].invoke(instance)), defaultLong);
				Object lNew = makeValueLong.invoke(defaultLongNew);
				if (ifValue) {
					instance = fieldAccessMHs[i][1].invoke(instance, lNew);
				} else {
					fieldAccessMHs[i][1].invoke(instance, lNew);
				}
				assertEquals(getLong.invoke(fieldAccessMHs[i][0].invoke(instance)), defaultLongNew);
				break;
			case "QLargeObject;":
				checkEqualLargeObject(fieldAccessMHs[i][0].invoke(instance), defaultObject);
				Object largeNew = createLargeObject(defaultObjectNew);
				if (ifValue) {
					instance = fieldAccessMHs[i][1].invoke(instance, largeNew);
				} else {
					fieldAccessMHs[i][1].invoke(instance, largeNew);
				}
				checkEqualLargeObject(fieldAccessMHs[i][0].invoke(instance), defaultObjectNew);
				break;
			default:
				break;
			}
		}
		return instance;
	}

	static void checkFieldAccessMHOfStaticType(MethodHandle[][] fieldAccessMHs, String[] fields)
			throws Throwable {
		for (int i = 0; i < fields.length; i++) {
			String nameAndSigValue[] = fields[i].split(":");
			String signature = nameAndSigValue[1];
			switch (signature) {
			case "QPoint2D;":
				checkEqualPoint2D(fieldAccessMHs[i][0].invoke(), defaultPointPositions1);
				Object pointNew = createPoint2D(defaultPointPositionsNew);
				fieldAccessMHs[i][1].invoke(pointNew);
				checkEqualPoint2D(fieldAccessMHs[i][0].invoke(), defaultPointPositionsNew);
				break;
			case "QFlattenedLine2D;":
				checkEqualFlattenedLine2D(fieldAccessMHs[i][0].invoke(), defaultLinePositions1);
				Object lineNew = createFlattenedLine2D(defaultLinePositionsNew);
				fieldAccessMHs[i][1].invoke(lineNew);
				checkEqualFlattenedLine2D(fieldAccessMHs[i][0].invoke(), defaultLinePositionsNew);
				break;
			case "QTriangle2D;":
				checkEqualTriangle2D(fieldAccessMHs[i][0].invoke(), defaultTrianglePositions);
				Object triNew = createTriangle2D(defaultTrianglePositionsNew);
				fieldAccessMHs[i][1].invoke(triNew);
				checkEqualTriangle2D(fieldAccessMHs[i][0].invoke(), defaultTrianglePositionsNew);
				break;
			case "QValueInt;":
				assertEquals(getInt.invoke(fieldAccessMHs[i][0].invoke()), defaultInt);
				Object iNew = makeValueInt.invoke(defaultIntNew);
				fieldAccessMHs[i][1].invoke(iNew);
				assertEquals(getInt.invoke(fieldAccessMHs[i][0].invoke()), defaultIntNew);
				break;
			case "QValueFloat;":
				assertEquals(getFloat.invoke(fieldAccessMHs[i][0].invoke()), defaultFloat);
				Object fNew = makeValueFloat.invoke(defaultFloatNew);
				fieldAccessMHs[i][1].invoke(fNew);
				assertEquals(getFloat.invoke(fieldAccessMHs[i][0].invoke()), defaultFloatNew);
				break;
			case "QValueDouble;":
				assertEquals(getDouble.invoke(fieldAccessMHs[i][0].invoke()), defaultDouble);
				Object dNew = makeValueDouble.invoke(defaultDoubleNew);
				fieldAccessMHs[i][1].invoke(dNew);
				assertEquals(getDouble.invoke(fieldAccessMHs[i][0].invoke()), defaultDoubleNew);
				break;
			case "QValueObject;":
				assertEquals(getObject.invoke(fieldAccessMHs[i][0].invoke()), defaultObject);
				Object oNew = makeValueObject.invoke(defaultObjectNew);
				fieldAccessMHs[i][1].invoke(oNew);
				assertEquals(getObject.invoke(fieldAccessMHs[i][0].invoke()), defaultObjectNew);
				break;
			case "QValueLong;":
				assertEquals(getLong.invoke(fieldAccessMHs[i][0].invoke()), defaultLong);
				Object lNew = makeValueLong.invoke(defaultLongNew);
				fieldAccessMHs[i][1].invoke(lNew);
				assertEquals(getLong.invoke(fieldAccessMHs[i][0].invoke()), defaultLongNew);
				break;
			case "QLargeObject;":
				checkEqualLargeObject(fieldAccessMHs[i][0].invoke(), defaultObject);
				Object largeNew = createLargeObject(defaultObjectNew);
				fieldAccessMHs[i][1].invoke(largeNew);
				checkEqualLargeObject(fieldAccessMHs[i][0].invoke(), defaultObjectNew);
				break;
			default:
				break;
			}
		}
	}

	static void checkEqualPoint2D(Object point, int[] positions) throws Throwable {
		assertEquals(getX.invoke(point), positions[0]);
		assertEquals(getY.invoke(point), positions[1]);
	}

	static void checkEqualFlattenedLine2D(Object line, int[][] positions) throws Throwable {
		checkEqualPoint2D(getFlatSt.invoke(line), positions[0]);
		checkEqualPoint2D(getFlatEn.invoke(line), positions[1]);
	}

	static void checkEqualTriangle2D(Object triangle, int[][][] positions) throws Throwable {
		checkEqualFlattenedLine2D(getV1.invoke(triangle), positions[0]);
		checkEqualFlattenedLine2D(getV2.invoke(triangle), positions[1]);
		checkEqualFlattenedLine2D(getV3.invoke(triangle), positions[2]);
	}

	static void checkEqualLargeObject(Object largeObject, Object contentObject) throws Throwable {
		for (int i = 0; i < 16; i++) {
			assertEquals(getObject.invoke(getObjects[i].invoke(largeObject)), contentObject);
		}
	}
	
	static void checkObject(Object ...objects) {
		com.ibm.jvm.Dump.SystemDump();
	}
}
