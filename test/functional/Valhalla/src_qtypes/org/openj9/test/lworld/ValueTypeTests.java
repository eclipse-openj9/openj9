/*
 * Copyright IBM Corp. and others 2018
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

import static org.objectweb.asm.Opcodes.*;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
import java.lang.reflect.Method;
import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.util.ArrayList;
import java.util.Arrays;
import jdk.internal.value.ValueClass;
import org.testng.Assert;
import static org.testng.Assert.*;
import org.testng.annotations.Test;
import org.testng.annotations.BeforeClass;
import static org.openj9.test.lworld.ValueTypeTestClasses.*;

/*
 * Instructions to run this test:
 *
 * 1)  Build the JDK with the '--enable-inline-types' configure flag
 * 2)  cd [openj9-openjdk-dir]/openj9/test
 * 3)  git clone https://github.com/adoptium/TKG.git
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
	/* point2D */
	static Class<?> point2DClass = null;
	static MethodHandle makePoint2D = null;
	static MethodHandle getX = null;
	static MethodHandle getY = null;
	static MethodHandle withX = null;
	static MethodHandle withY = null;
	/* line2D */
	static Class<?> line2DClass = null;
	static MethodHandle makeLine2D = null;
	/* flattenedLine2D */
	static Class<?> flattenedLine2DClass = null;
	static MethodHandle makeFlattenedLine2D = null;
	static MethodHandle getFlatSt = null;
	static MethodHandle withFlatSt = null;
 	static MethodHandle getFlatEn = null;
 	static MethodHandle withFlatEn = null;
	/* triangle2D */
	static Class<?> triangle2DClass = null;
	static MethodHandle makeTriangle2D = null;
	static MethodHandle getV1 = null;
	static MethodHandle getV2 = null;
	static MethodHandle getV3 = null;
	/* valueLong */
	static Class<?> valueLongClass = null;
	static MethodHandle makeValueLong = null;
	static MethodHandle getLong = null;
	static MethodHandle withLong = null;
	/* valueInt */
	static Class<?> valueIntClass = null;
	static MethodHandle makeValueInt = null;
	static MethodHandle getInt = null;
	static MethodHandle withInt = null;
	/* valueDouble */
	static Class<?> valueDoubleClass = null;
	static MethodHandle makeValueDouble = null;
	static MethodHandle getDouble = null;
	static MethodHandle withDouble = null;
	/* valueFloat */
	static Class<?> valueFloatClass = null;
	static MethodHandle makeValueFloat = null;
	static MethodHandle getFloat = null;
	static MethodHandle withFloat = null;
	/* valueObject */
	static Class<?> valueObjectClass = null;
	static MethodHandle makeValueObject = null;
	static MethodHandle getObject = null;
	static MethodHandle withObject = null;
	/* largeObject */
	static Class<?> largeObjectValueClass = null;
	static MethodHandle makeLargeObjectValue = null;
	static MethodHandle[] getObjects = null;
	/* megaObject */
	static Class<?> megaObjectValueClass = null;
	static MethodHandle makeMegaObjectValue = null;
	/* assortedRefWithLongAlignment */
	static Class<?> assortedRefWithLongAlignmentClass = null;
	static MethodHandle makeAssortedRefWithLongAlignment = null;
	static MethodHandle[][] assortedRefWithLongAlignmentGetterAndSetter = null;
	/* assortedRefWithObjectAlignment */
	static Class<?> assortedRefWithObjectAlignmentClass = null;
	static MethodHandle makeAssortedRefWithObjectAlignment = null;
	static MethodHandle[][] assortedRefWithObjectAlignmentGetterList = null;
	/* assortedRefWithSingleAlignment */
	static Class<?> assortedRefWithSingleAlignmentClass = null;
	static MethodHandle makeAssortedRefWithSingleAlignment = null;
	static MethodHandle[][] assortedRefWithSingleAlignmentGetterList = null;
	/* assortedValueWithLongAlignment */
	static Class<?> assortedValueWithLongAlignmentClass = null;
	static MethodHandle makeAssortedValueWithLongAlignment = null;
	static MethodHandle[][] assortedValueWithLongAlignmentGetterList = null;
	static Class<?> classWithOnlyStaticFieldsWithLongAlignment = null;
	static MethodHandle[][] staticFieldsWithLongAlignmentGenericGetterAndSetter = null;
	/* assortedValueWithObjectAlignment */
	static Class<?> assortedValueWithObjectAlignmentClass = null;
	static MethodHandle makeAssortedValueWithObjectAlignment = null;
	static MethodHandle[][] assortedValueWithObjectAlignmentGetterList = null;
	static Class<?> classWithOnlyStaticFieldsWithObjectAlignment = null;
	static MethodHandle[][] staticFieldsWithObjectAlignmentGenericGetterAndSetter = null;
	/* assortedValueWithSingleAlignment */
	static Class<?> assortedValueWithSingleAlignmentClass = null;
	static MethodHandle makeAssortedValueWithSingleAlignment = null;
	static MethodHandle[][] assortedValueWithSingleAlignmentGetterList = null;
	static Class<?> classWithOnlyStaticFieldsWithSingleAlignment = null;
	static MethodHandle[][] staticFieldsWithSingleAlignmentGenericGetterAndSetter = null;
	/* LayoutsWithPrimitives classes */
	static Class<?> singleBackfillClass = null;
	static MethodHandle makeSingleBackfillClass = null;
	static MethodHandle getSingleI = null;
	static MethodHandle getSingleO = null;
	static MethodHandle getSingleL = null;
	static Class<?> objectBackfillClass = null;
	static MethodHandle makeObjectBackfillClass = null;
	static MethodHandle getObjectO = null;
	static MethodHandle getObjectL = null;
	/* LayoutsWithValueTypes classes */
	static Class<?> flatSingleBackfillClass = null;
	static MethodHandle makeFlatSingleBackfillClass = null;
	static MethodHandle getVTSingleI = null;
	static MethodHandle getVTSingleO = null;
	static MethodHandle getVTSingleL = null;
	static Class<?> flatObjectBackfillClass = null;
	static MethodHandle makeFlatObjectBackfillClass = null;
	static MethodHandle getVTObjectO = null;
	static MethodHandle getVTObjectL = null;
	static Class<?> flatUnAlignedSingleClass = null;
	static MethodHandle makeFlatUnAlignedSingleClass = null;
	static MethodHandle getUnAlignedSingleI = null;
	static MethodHandle getUnAlignedSingleI2 = null;
	static Class<?> flatUnAlignedSingleBackfillClass = null;
	static MethodHandle makeFlatUnAlignedSingleBackfillClass = null;
	static MethodHandle getUnAlignedSingleflatSingleBackfillInstanceO = null;
	static MethodHandle getUnAlignedSingleflatSingleBackfillInstanceSingles = null;
	static MethodHandle getUnAlignedSingleflatSingleBackfillInstanceL = null;
	static Class<?> flatUnAlignedSingleBackfillClass2 = null;
	static MethodHandle makeFlatUnAlignedSingleBackfillClass2 = null;
	static MethodHandle getUnAlignedSingleflatSingleBackfill2InstanceSingles = null;
	static MethodHandle getUnAlignedSingleflatSingleBackfill2InstanceSingles2 = null;
	static MethodHandle getUnAlignedSingleflatSingleBackfill2InstanceL = null;
	static Class<?> flatUnAlignedObjectClass = null;
	static MethodHandle makeFlatUnAlignedObjectClass = null;
	static MethodHandle getUnAlignedObjectO = null;
	static MethodHandle getUnAlignedObjectO2 = null;
	static Class<?> flatUnAlignedObjectBackfillClass = null;
	static MethodHandle makeFlatUnAlignedObjectBackfillClass = null;
	static MethodHandle getUnAlignedObjectflatObjectBackfillInstanceObjects = null;
	static MethodHandle getUnAlignedObjectflatObjectBackfillInstanceObjects2 = null;
	static MethodHandle getUnAlignedObjectflatObjectBackfillInstanceL = null;
	static Class<?> flatUnAlignedObjectBackfillClass2 = null;
	static MethodHandle makeFlatUnAlignedObjectBackfillClass2 = null;
	static MethodHandle getUnAlignedObjectflatObjectBackfill2InstanceO = null;
	static MethodHandle getUnAlignedObjectflatObjectBackfill2InstanceObjects = null;
	static MethodHandle getUnAlignedObjectflatObjectBackfill2InstanceL = null;

	/* fields */
	static String[] typeWithSingleAlignmentFields = {
		"tri:LTriangle2D;:NR", /* NR means null-restricted */
		"point:LPoint2D;:NR",
		"line:LFlattenedLine2D;:NR",
		"i:LValueInt;:NR",
		"f:LValueFloat;:NR",
		"tri2:LTriangle2D;:NR"
	};
	static String[] typeWithObjectAlignmentFields = {
		"tri:LTriangle2D;:NR",
		"point:LPoint2D;:NR",
		"line:LFlattenedLine2D;:NR",
		"o:LValueObject;:NR",
		"i:LValueInt;:NR",
		"f:LValueFloat;:NR",
		"tri2:LTriangle2D;:NR"
	};
	static String[] typeWithLongAlignmentFields = {
		"point:LPoint2D;:NR",
		"line:LFlattenedLine2D;:NR",
		"o:LValueObject;:NR",
		"l:LValueLong;:NR",
		"d:LValueDouble;:NR",
		"i:LValueInt;:NR",
		"tri:LTriangle2D;:NR"
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
	static long defaultLong = 0xFAFBFCFD11223344L;
	static int defaultInt = 0x12123434;
	static double defaultDouble = Double.MAX_VALUE;
	static float defaultFloat = Float.MAX_VALUE;
	static Object defaultObject = (Object)0xEEFFEEFF;
	static int[] defaultPointPositionsNew = {0xFF112233, 0xFF332211};
	static int[][] defaultLinePositionsNew = {defaultPointPositionsNew, defaultPointPositions1};
	static int[][][] defaultTrianglePositionsNew = {defaultLinePositionsNew, defaultLinePositions3, defaultLinePositions1};
	static int[][][] defaultTrianglePositionsEmpty = {defaultLinePositionsEmpty, defaultLinePositionsEmpty, defaultLinePositionsEmpty};
	static long defaultLongNew = 0x11551155AAEEAAEEL;
	static long defaultLongNew2 = 0x22662266BBFFBBFFL;
	static long defaultLongNew3 = 0x33773377CC00CC00L;
	static long defaultLongNew4 = 0x44884488DD11DD11L;
	static long defaultLongNew5 = 0x55995599EE22EE22L;
	static int defaultIntNew = 0x45456767;
	static double defaultDoubleNew = -123412341.21341234d;
	static float defaultFloatNew = -123423.12341234f;
	static Object defaultObjectNew = (Object)0xFFEEFFEE;
	/* miscellaneous constants */
	static final int genericArraySize = 10;
	static final int objectGCScanningIterationCount = 1000;

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
		String[] fields = {"x:I", "y:I"};
		point2DClass = ValueTypeGenerator.generateValueClass("Point2D", fields);
		
		makePoint2D = lookup.findStatic(point2DClass, "makeObjectGeneric", MethodType.methodType(Object.class, Object.class, Object.class));
		
		/* Replace typed getters/setters to generic due to current lack of support for ValueTypes and OJDK method handles */
		getX = generateGenericGetter(point2DClass, "x");
		getY = generateGenericGetter(point2DClass, "y");

		int x = 0xFFEEFFEE;
		int y = 0xAABBAABB;
		
		Object point2D = makePoint2D.invoke(x, y);
		
		assertEquals(getX.invoke(point2D), x);
		assertEquals(getY.invoke(point2D), y);
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
		Object[] array = ValueClass.newNullRestrictedArray(point2DClass, 8);

		for (int i = 0; i < 8; i++) {
			array[i] = point2D;
		}

		System.gc();

		Object value = array[0];
	}

	@Test(priority=5)
	static public void testGCFlattenedValueArrayWithSingleAlignment() throws Throwable {
		Object[] array = ValueClass.newNullRestrictedArray(assortedValueWithSingleAlignmentClass, 4);
		
		for (int i = 0; i < 4; i++) {
			Object object = createAssorted(makeAssortedValueWithSingleAlignment, typeWithSingleAlignmentFields);
			array[i] = object;
		}

		System.gc();

		for (int i = 0; i < 4; i++) {
			checkFieldAccessMHOfAssortedType(assortedValueWithSingleAlignmentGetterList, array[i], typeWithSingleAlignmentFields, true);
		}
	}

	@Test(priority=5)
	static public void testGCFlattenedValueArrayWithObjectAlignment() throws Throwable {
		Object[] array = ValueClass.newNullRestrictedArray(assortedValueWithObjectAlignmentClass, 4);
		
		for (int i = 0; i < 4; i++) {
			Object object = createAssorted(makeAssortedValueWithObjectAlignment, typeWithObjectAlignmentFields);
			array[i] = object;
		}

		System.gc();

		for (int i = 0; i < 4; i++) {
			checkFieldAccessMHOfAssortedType(assortedValueWithObjectAlignmentGetterList, array[i], typeWithObjectAlignmentFields, true);
		}
	}

	@Test(priority=5)
	static public void testGCFlattenedValueArrayWithLongAlignment() throws Throwable {
		Object[] array = ValueClass.newNullRestrictedArray(assortedValueWithLongAlignmentClass, genericArraySize);
		
		for (int i = 0; i < genericArraySize; i++) {
			Object object = createAssorted(makeAssortedValueWithLongAlignment, typeWithLongAlignmentFields);
			array[i] = object;
		}

		System.gc();

		for (int i = 0; i < genericArraySize; i++) {
			checkFieldAccessMHOfAssortedType(assortedValueWithLongAlignmentGetterList, array[i], typeWithLongAlignmentFields, true);
		}
	}

	@Test(priority=5)
	static public void testGCFlattenedLargeObjectArray() throws Throwable {
		Object[] array = ValueClass.newNullRestrictedArray(largeObjectValueClass, 4);
		Object largeObjectRef = createLargeObject(new Object());

		for (int i = 0; i < 4; i++) {
			array[i] = largeObjectRef;
		}

		System.gc();

		Object value = array[0];
	}

	@Test(priority=5)
	static public void testGCFlattenedMegaObjectArray() throws Throwable {
		Object[] array = ValueClass.newNullRestrictedArray(megaObjectValueClass, 4);
		Object megaObjectRef = createMegaObject(new Object());

		System.gc();

		for (int i = 0; i < 4; i++) {
			array[i] = megaObjectRef;
		}
		System.gc();

		Object value = array[0];
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
		String[] fields = {"d:D", "j:J"};
		Class<?> point2DComplexClass = ValueTypeGenerator.generateValueClass("Point2DComplex", fields);
		
		MethodHandle makePoint2DComplex = lookup.findStatic(point2DComplexClass, "makeObjectGeneric", MethodType.methodType(Object.class, Object.class, Object.class));

		/* Replace typed getters/setters to generic due to current lack of support for ValueTypes and OJDK method handles */
		MethodHandle getD = generateGenericGetter(point2DComplexClass, "d");
		MethodHandle getJ = generateGenericGetter(point2DComplexClass, "j");
		
		double d = Double.MAX_VALUE;
		long j = Long.MAX_VALUE;
		Object point2D = makePoint2DComplex.invoke(d, j);
		
		assertEquals(getD.invoke(point2D), d);
		assertEquals(getJ.invoke(point2D), j);
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
		String[] fields = {"st:LPoint2D;:NR", "en:LPoint2D;:NR"};
		line2DClass = ValueTypeGenerator.generateValueClass("Line2D", fields);
		
		makeLine2D = lookup.findStatic(line2DClass, "makeObjectGeneric", MethodType.methodType(Object.class, Object.class, Object.class));
		
		/* Replace typed getters/setters to generic due to current lack of support for ValueTypes and OJDK method handles */
		MethodHandle getSt = generateGenericGetter(line2DClass, "st");
		MethodHandle getEn = generateGenericGetter(line2DClass, "en");

		int x = 0xFFEEFFEE;
		int y = 0xAABBAABB;
		int x2 = 0xCCDDCCDD;
		int y2 = 0xAAFFAAFF;
		
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
		String[] fields = {"st:LPoint2D;:NR", "en:LPoint2D;:NR"};
		flattenedLine2DClass = ValueTypeGenerator.generateValueClass("FlattenedLine2D", fields);
				
		makeFlattenedLine2D = lookup.findStatic(flattenedLine2DClass, "makeObjectGeneric", MethodType.methodType(Object.class, Object.class, Object.class));
		
		/* Replace typed getters/setters to generic due to current lack of support for ValueTypes and OJDK method handles */
		getFlatSt = generateGenericGetter(flattenedLine2DClass, "st");
		getFlatEn = generateGenericGetter(flattenedLine2DClass, "en");

		int x = 0xFFEEFFEE;
		int y = 0xAABBAABB;
		int x2 = 0xCCDDCCDD;
		int y2 = 0xAAFFAAFF;
		
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
	@Test(priority=3, invocationCount=2)
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

		Object[] arrayObject = ValueClass.newNullRestrictedArray(flattenedLine2DClass, 2);
		arrayObject[0] = line2D_1;
		arrayObject[1] = line2D_2;
		Object line2D_1_check = arrayObject[0];
		Object line2D_2_check = arrayObject[1];

		assertEquals(getX.invoke(getFlatSt.invoke(line2D_1_check)), getX.invoke(getFlatSt.invoke(line2D_1)));
		assertEquals(getX.invoke(getFlatSt.invoke(line2D_2_check)), getX.invoke(getFlatSt.invoke(line2D_2)));
		assertEquals(getY.invoke(getFlatSt.invoke(line2D_1_check)), getY.invoke(getFlatSt.invoke(line2D_1)));
		assertEquals(getY.invoke(getFlatSt.invoke(line2D_2_check)), getY.invoke(getFlatSt.invoke(line2D_2)));
		assertEquals(getX.invoke(getFlatEn.invoke(line2D_1_check)), getX.invoke(getFlatEn.invoke(line2D_1)));
		assertEquals(getX.invoke(getFlatEn.invoke(line2D_2_check)), getX.invoke(getFlatEn.invoke(line2D_2)));
		assertEquals(getY.invoke(getFlatEn.invoke(line2D_1_check)), getY.invoke(getFlatEn.invoke(line2D_1)));
		assertEquals(getY.invoke(getFlatEn.invoke(line2D_2_check)), getY.invoke(getFlatEn.invoke(line2D_2)));
	}
	
	@Test(priority=2, invocationCount=2)
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
	
	@Test(priority=2, invocationCount=2)
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
		Object[] arr = {"foo", "bar", "baz"};
		Object[] arr2 = {"foozo", "barzo", "bazzo"};
		Object valueType1 = new ValueTypeFastSubVT(1, 2, 3, arr);
		Object valueType2 = new ValueTypeFastSubVT(1, 2, 3, arr);
		Object newValueType = new ValueTypeFastSubVT(3, 2, 1, arr2);
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
		String[] fields = {"l:J", "next:Ljava/lang/Object;", "i:I"};
		Class<?> nodeClass = ValueTypeGenerator.generateValueClass("Node", fields);
		MethodHandle makeNode = lookup.findStatic(nodeClass, "makeObjectGeneric", MethodType.methodType(Object.class, Object.class, Object.class, Object.class));
		
		Object list1 = makeNode.invoke(3L, null, 3);
		Object list2 = makeNode.invoke(3L, null, 3);
		Object list3sameAs1 = makeNode.invoke(3L, null, 3);
		Object list4 = makeNode.invoke(3L, null, 3);
		Object list5null = makeNode.invoke(3L, null, 3);
		Object list6null = makeNode.invoke(3L, null, 3);
		Object list7obj = makeNode.invoke(3L, new Object(), 3);
		Object list8str = makeNode.invoke(3L, "foo", 3);
		Object identityType = new String();
		Object nullPointer = null;
		
		for (int i = 0; i < 100; i++) {
			list1 = makeNode.invoke(3L, list1, i);
			list2 = makeNode.invoke(3L, list2, i + 1);
			list3sameAs1 = makeNode.invoke(3L, list3sameAs1, i);
		}
		for (int i = 0; i < 50; i++) {
			list4 = makeNode.invoke(3L, list4, i);
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
	
	@Test(priority=3, invocationCount=2)
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

	@Test(priority=3, invocationCount=2)
	static public void testACMPTestOnValueDouble() throws Throwable {
		Object double1 = makeValueDouble.invoke(1.1d);
		Object double2 = makeValueDouble.invoke(-1.1d);
		Object double3 = makeValueDouble.invoke(12341.112341234d);
		Object double4sameAs1 = makeValueDouble.invoke(1.1d);
		Object nan = makeValueDouble.invoke(Double.NaN);
		Object nan2 = makeValueDouble.invoke(Double.NaN);
		Object positiveZero = makeValueDouble.invoke(0.0d);
		Object negativeZero = makeValueDouble.invoke(-0.0d);
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
		Object[] newLongFields = {
			defaultPointPositionsNew,
			defaultLinePositionsNew,
			defaultObjectNew,
			defaultLongNew,
			defaultDoubleNew,
			defaultIntNew,
			defaultTrianglePositionsNew
		};
		Object[] newObjectFields = {
			defaultTrianglePositionsNew,
			defaultPointPositionsNew,
			defaultLinePositionsNew,
			defaultObjectNew,
			defaultIntNew,
			defaultFloatNew,
			defaultTrianglePositionsNew
		};

		Object assortedValueWithLongAlignment = createAssorted(makeAssortedValueWithLongAlignment, typeWithLongAlignmentFields);
		Object assortedValueWithLongAlignment2 = createAssorted(makeAssortedValueWithLongAlignment, typeWithLongAlignmentFields);
		Object assortedValueWithLongAlignment3 = createAssorted(makeAssortedValueWithLongAlignment, typeWithLongAlignmentFields, newLongFields);
		
		Object assortedValueWithObjectAlignment = createAssorted(makeAssortedValueWithObjectAlignment, typeWithObjectAlignmentFields);
		Object assortedValueWithObjectAlignment2 = createAssorted(makeAssortedValueWithObjectAlignment, typeWithObjectAlignmentFields);
		Object assortedValueWithObjectAlignment3 = createAssorted(makeAssortedValueWithObjectAlignment, typeWithObjectAlignmentFields, newObjectFields);
		
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
	 * Test monitorExit on valueType
	 *
	 * class TestMonitorExitOnValueType {
	 *  long longField
	 * }
	 */
	@Test(priority=2)
	static public void testMonitorEnterAndExitOnValueType() throws Throwable {
		int x = 1;
		int y = 1;
		Object valueType = makePoint2D.invoke(x, y);
		Object valueTypeArray = Array.newInstance(flattenedLine2DClass, genericArraySize);

		String[] fields = {"longField:J"};
		Class<?> testMonitorExitOnValueType = ValueTypeGenerator.generateRefClass("TestMonitorExitOnValueType", fields);
		MethodHandle monitorEnterAndExitOnValueType = lookup.findStatic(testMonitorExitOnValueType, "testMonitorEnterAndExitWithRefType", MethodType.methodType(void.class, Object.class));
		try {
			monitorEnterAndExitOnValueType.invoke(valueType);
			Assert.fail("should throw exception. MonitorExit cannot be used with ValueType");
		} catch (IdentityException e) {}
		
		try {
			monitorEnterAndExitOnValueType.invoke(valueTypeArray);
		} catch (IllegalMonitorStateException e) {
			Assert.fail("Should not throw exception. MonitorExit can be used with ValueType arrays");
		}
	}


	/* The non-static synchronized method case is covered by
	 * ValueTypeTests.testValueTypeHasSynchMethods.
	 */
	@Test(priority=2)
	static public void testStaticSynchMethodsOnValueTypes() throws Throwable {
		int x = 1;
		int y = 1;
		Object valueType = makePoint2D.invoke(x, y);
		MethodHandle staticSyncMethod = lookup.findStatic(point2DClass, "staticSynchronizedMethodReturnInt", MethodType.methodType(int.class));

		try {
			staticSyncMethod.invoke();
		} catch (IllegalMonitorStateException e) {
			Assert.fail("should not throw exception. Synchronized static methods can be used with ValueType");
		}
	}
	
	@Test(priority=2)
	static public void testSynchMethodsOnRefTypes() throws Throwable {
		String[] fields = {"longField:J"};
		Class<?> refTypeClass = ValueTypeGenerator.generateRefClass("RefType", fields);
		MethodHandle makeObject = lookup.findStatic(refTypeClass, "makeObject", MethodType.methodType(refTypeClass, long.class));
		Object refType = makeObject.invoke(1L);
		
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
	 * Test monitorExit with refType
	 *
	 * class TestMonitorExitWithRefType {
	 *  long longField
	 * }
	 */
	@Test(priority=1)
	static public void testMonitorExitWithRefType() throws Throwable {
		Object refType = new Object();
		
		String[] fields = {"longField:J"};
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
		Object refType = new Object();
		
		String[] fields = {"longField:J"};
		Class<?> testMonitorEnterAndExitWithRefType = ValueTypeGenerator.generateRefClass("TestMonitorEnterAndExitWithRefType", fields);
		MethodHandle monitorEnterAndExitWithRefType = lookup.findStatic(testMonitorEnterAndExitWithRefType, "testMonitorEnterAndExitWithRefType", MethodType.methodType(void.class, Object.class));
		try {
			monitorEnterAndExitWithRefType.invoke(refType);
		} catch (IllegalMonitorStateException e) {
			Assert.fail("shouldn't throw exception. MonitorEnter and MonitorExit should be used with refType");
		}
	}

	@Test(priority=1)
	static public void testMigratedValueClasses() {
		Integer a = new Integer(1);
		Integer b = new Integer(1);

		Assert.assertTrue(a.getClass().isValue(),
			"java/lang/Integer should be migrated to a value class.");
		Assert.assertTrue(a.hashCode() == b.hashCode(),
			"Two objects of value type classes with the same values should have the same hash code.");
		Assert.assertTrue(a == b,
			"Two objects of value type classes with the same values should be equal.");
	}

	@Test(priority=1)
	static public void testMigratedValueClassesMonitorEnterAndExit() throws Throwable {
		Integer i = new Integer(1);
		Object refType = (Object)i;

		Class<?> testMigratedValueClassesMonitorEnterAndExit = ValueTypeGenerator.generateRefClass("TestMigratedValueClassesMonitorEnterAndExit");
		MethodHandle monitorEnterAndExitWithRefType = lookup.findStatic(testMigratedValueClassesMonitorEnterAndExit, "testMonitorEnterAndExitWithRefType", MethodType.methodType(void.class, Object.class));
		try {
			monitorEnterAndExitWithRefType.invoke(refType);
			fail("Synchronization attempts on migrated value classes should fail.");
		} catch (IdentityException e) {
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
		String[] fields = {"v1:LFlattenedLine2D;:NR", "v2:LFlattenedLine2D;:NR", "v3:LFlattenedLine2D;:NR"};
		triangle2DClass = ValueTypeGenerator.generateValueClass("Triangle2D", fields);

		makeTriangle2D = lookup.findStatic(triangle2DClass, "makeObjectGeneric", MethodType.methodType(Object.class, Object.class, Object.class, Object.class));

		/* Replace typed getters/setters to generic due to current lack of support for ValueTypes and OJDK method handles */
		getV1 = generateGenericGetter(triangle2DClass, "v1");
		getV2 = generateGenericGetter(triangle2DClass, "v2");
		getV3 = generateGenericGetter(triangle2DClass, "v3");

		Object triangle2D = createTriangle2D(defaultTrianglePositions);
		checkEqualTriangle2D(triangle2D, defaultTrianglePositions);
	}
	
	
	@Test(priority=4, invocationCount=2)
	static public void testCreateArrayTriangle2D() throws Throwable {
		Object arrayObject = Array.newInstance(triangle2DClass, 10);
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
		String[] fields = {"j:J"};
		valueLongClass = ValueTypeGenerator.generateValueClass("ValueLong", fields);
		makeValueLong = lookup.findStatic(valueLongClass, "makeObjectGeneric",
				MethodType.methodType(Object.class, Object.class));

		/* Replace typed getters/setters to generic due to current lack of support for ValueTypes and OJDK method handles */
		getLong = generateGenericGetter(valueLongClass, "j");

		long j = Long.MAX_VALUE;
		Object valueLong = makeValueLong.invoke(j);

		assertEquals(getLong.invoke(valueLong), j);
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
		String[] fields = {"i:I"};
		valueIntClass = ValueTypeGenerator.generateValueClass("ValueInt", fields);

		makeValueInt = lookup.findStatic(valueIntClass, "makeObjectGeneric", MethodType.methodType(Object.class, Object.class));

		/* Replace typed getters/setters to generic due to current lack of support for ValueTypes and OJDK method handles */
		getInt = generateGenericGetter(valueIntClass, "i");

		int i = Integer.MAX_VALUE;
		Object valueInt = makeValueInt.invoke(i);

		assertEquals(getInt.invoke(valueInt), i);
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
		String[] fields = {"d:D"};
		valueDoubleClass = ValueTypeGenerator.generateValueClass("ValueDouble", fields);

		makeValueDouble = lookup.findStatic(valueDoubleClass, "makeObjectGeneric",
				MethodType.methodType(Object.class, Object.class));

		/* Replace typed getters/setters to generic due to current lack of support for ValueTypes and OJDK method handles */
		getDouble = generateGenericGetter(valueDoubleClass, "d");

		double d = Double.MAX_VALUE;
		Object valueDouble = makeValueDouble.invoke(d);

		assertEquals(getDouble.invoke(valueDouble), d);
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
		String[] fields = {"f:F"};
		valueFloatClass = ValueTypeGenerator.generateValueClass("ValueFloat", fields);

		makeValueFloat = lookup.findStatic(valueFloatClass, "makeObjectGeneric",
				MethodType.methodType(Object.class, Object.class));

		/* Replace typed getters/setters to generic due to current lack of support for ValueTypes and OJDK method handles */
		getFloat = generateGenericGetter(valueFloatClass, "f");

		float f = Float.MAX_VALUE;
		Object valueFloat = makeValueFloat.invoke(f);

		assertEquals(getFloat.invoke(valueFloat), f);
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
		String[] fields = {"val:Ljava/lang/Object;"};

		valueObjectClass = ValueTypeGenerator.generateValueClass("ValueObject", fields);

		makeValueObject = lookup.findStatic(valueObjectClass, "makeObjectGeneric",
				MethodType.methodType(Object.class, Object.class));

		Object val = (Object)0xEEFFEEFF;

		/* Replace typed getters/setters to generic due to current lack of support for ValueTypes and OJDK method handles */
		getObject = generateGenericGetter(valueObjectClass, "val");

		Object valueObject = makeValueObject.invoke(val);

		assertEquals(getObject.invoke(valueObject), val);
	}
	
	/*	
	 * Create a valueType with four valueType members including 2 volatile.
	 *
	 * value valueWithVolatile {
	 *  flattened ValueInt i;
	 *  flattened ValueInt i2;
	 *  flattened Point2D point;   <--- 8 bytes, will be flattened.
	 *  volatile Point2D vpoint;   <--- volatile 8 bytes, will be flattened.
	 *  flattened Line2D 1ine;     <--- 16 bytes, will be flattened.
	 *  volatile Line2D vline;     <--- volatile 16 bytes, will not be flattened.
	 * }
	 */
	@Test(priority=3)
	static public Object createValueTypeWithVolatileFields() throws Throwable {
		String[] fields = {"i:LValueInt;:NR", "i2:LValueInt;:NR", "point:LPoint2D;:NR", "vpoint:LPoint2D;:NR:volatile",
				"line:LFlattenedLine2D;:NR", "vline:LFlattenedLine2D;:NR:volatile"};
		Class<?> ValueTypeWithVolatileFieldsClass = ValueTypeGenerator.generateValueClass("ValueTypeWithVolatileFields", fields);
		MethodHandle valueWithVolatile = lookup.findStatic(ValueTypeWithVolatileFieldsClass, "makeObjectGeneric", MethodType.methodType(Object.class, Object.class, Object.class, Object.class,
				Object.class, Object.class, Object.class));
		MethodHandle[][] getterList = generateGenericGetterList(ValueTypeWithVolatileFieldsClass, fields);
		Object valueWithVolatileObj = createAssorted(valueWithVolatile, fields);
		checkFieldAccessMHOfAssortedType(getterList, valueWithVolatileObj, fields, true);
		return valueWithVolatileObj;
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
				"makeObjectGeneric", MethodType.methodType(Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class, Object.class));
		/*
		 * Getters are created in array getterList[i][0] according to the order of fields i
		 */
		assortedValueWithLongAlignmentGetterList = generateGenericGetterList(assortedValueWithLongAlignmentClass, typeWithLongAlignmentFields);
	}

	@Test(priority=5, invocationCount=2)
	static public void testAssortedValueWithLongAlignment() throws Throwable {
		Object assortedValueWithLongAlignment = createAssorted(makeAssortedValueWithLongAlignment, typeWithLongAlignmentFields);
		checkFieldAccessMHOfAssortedType(assortedValueWithLongAlignmentGetterList, assortedValueWithLongAlignment, typeWithLongAlignmentFields, true);
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
				"makeObjectGeneric", MethodType.methodType(Object.class, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class));

		/*
		 * Getters are created in array getterAndSetter[i][0] according to the order of fields i
		 * Setters are created in array getterAndSetter[i][1] according to the order of fields i
		 */
		assortedRefWithLongAlignmentGetterAndSetter = generateGenericGetterAndSetter(assortedRefWithLongAlignmentClass, typeWithLongAlignmentFields);
	}

	@Test(priority=5, invocationCount=2)
	static public void testAssortedRefWithLongAlignment() throws Throwable {
		Object assortedRefWithLongAlignment = createAssorted(makeAssortedRefWithLongAlignment, typeWithLongAlignmentFields);
		checkFieldAccessMHOfAssortedType(assortedRefWithLongAlignmentGetterAndSetter, assortedRefWithLongAlignment, typeWithLongAlignmentFields, false);
	}

	@Test(priority=2)
	static public void testCreateLayoutsWithPrimitives() throws Throwable {
		String[] singleBackfill = {"l:J", "o:Ljava/lang/Object;", "i:I"};
		singleBackfillClass = ValueTypeGenerator.generateValueClass("SingleBackfill", singleBackfill);
		makeSingleBackfillClass = lookup.findStatic(singleBackfillClass, "makeObjectGeneric", MethodType.methodType(Object.class, Object.class, Object.class, Object.class));
		/* Replace typed getters/setters to generic due to current lack of support for ValueTypes and OJDK method handles */
		getSingleI = generateGenericGetter(singleBackfillClass, "i");
		getSingleO = generateGenericGetter(singleBackfillClass, "o");
		getSingleL = generateGenericGetter(singleBackfillClass, "l");
		
		String[] objectBackfill = {"l:J", "o:Ljava/lang/Object;"};
		objectBackfillClass = ValueTypeGenerator.generateValueClass("ObjectBackfill", objectBackfill);
		makeObjectBackfillClass = lookup.findStatic(objectBackfillClass, "makeObjectGeneric", MethodType.methodType(Object.class, Object.class, Object.class));
		/* Replace typed getters/setters to generic due to current lack of support for ValueTypes and OJDK method handles */
		getObjectO = generateGenericGetter(objectBackfillClass, "o");
		getObjectL = generateGenericGetter(objectBackfillClass, "l");
	}
	
	@Test(priority=3, invocationCount=2)
	static public void testLayoutsWithPrimitives() throws Throwable {
		Object singleBackfillInstance = makeSingleBackfillClass.invoke(defaultLong, defaultObject, defaultInt);
		assertEquals(getSingleI.invoke(singleBackfillInstance), defaultInt);
		assertEquals(getSingleO.invoke(singleBackfillInstance), defaultObject);
		assertEquals(getSingleL.invoke(singleBackfillInstance), defaultLong);
		
		Object objectBackfillInstance = makeObjectBackfillClass.invoke(defaultLong, defaultObject);
		assertEquals(getObjectO.invoke(objectBackfillInstance), defaultObject);
		assertEquals(getObjectL.invoke(objectBackfillInstance), defaultLong);
	}
	
	@Test(priority=4)
	static public void testCreateFlatLayoutsWithValueTypes() throws Throwable {
		String[] flatSingleBackfill = {"l:LValueLong;:NR", "o:LValueObject;:NR", "i:LValueInt;:NR"};
		flatSingleBackfillClass = ValueTypeGenerator.generateValueClass("FlatSingleBackfill", flatSingleBackfill);
		makeFlatSingleBackfillClass = lookup.findStatic(flatSingleBackfillClass, "makeObjectGeneric", MethodType.methodType(Object.class, Object.class, Object.class, Object.class));
		getVTSingleI = generateGenericGetter(flatSingleBackfillClass, "i");
		getVTSingleO = generateGenericGetter(flatSingleBackfillClass, "o");
		getVTSingleL = generateGenericGetter(flatSingleBackfillClass, "l");
		
		String[] flatObjectBackfill = {"l:LValueLong;:NR", "o:LValueObject;:NR"};
		flatObjectBackfillClass = ValueTypeGenerator.generateValueClass("FlatObjectBackfill", flatObjectBackfill);
		makeFlatObjectBackfillClass = lookup.findStatic(flatObjectBackfillClass, "makeObjectGeneric", MethodType.methodType(Object.class, Object.class, Object.class));
		getVTObjectO = generateGenericGetter(flatObjectBackfillClass, "o");
		getVTObjectL = generateGenericGetter(flatObjectBackfillClass, "l");
		
		String[] flatUnAlignedSingle = {"i:LValueInt;:NR", "i2:LValueInt;:NR"};
		flatUnAlignedSingleClass = ValueTypeGenerator.generateValueClass("FlatUnAlignedSingle", flatUnAlignedSingle);
		makeFlatUnAlignedSingleClass = lookup.findStatic(flatUnAlignedSingleClass, "makeObjectGeneric", MethodType.methodType(Object.class, Object.class, Object.class));
		getUnAlignedSingleI = generateGenericGetter(flatUnAlignedSingleClass, "i");
		getUnAlignedSingleI2 = generateGenericGetter(flatUnAlignedSingleClass, "i2");
		
		String[] flatUnAlignedSingleBackfill = {"l:LValueLong;:NR","singles:LFlatUnAlignedSingle;:NR", "o:LValueObject;:NR"};
		flatUnAlignedSingleBackfillClass = ValueTypeGenerator.generateValueClass("FlatUnAlignedSingleBackfill", flatUnAlignedSingleBackfill);
		makeFlatUnAlignedSingleBackfillClass = lookup.findStatic(flatUnAlignedSingleBackfillClass, "makeObjectGeneric", MethodType.methodType(Object.class, Object.class, Object.class, Object.class));
		getUnAlignedSingleflatSingleBackfillInstanceO = generateGenericGetter(flatUnAlignedSingleBackfillClass, "o");
		getUnAlignedSingleflatSingleBackfillInstanceSingles = generateGenericGetter(flatUnAlignedSingleBackfillClass, "singles");
		getUnAlignedSingleflatSingleBackfillInstanceL = generateGenericGetter(flatUnAlignedSingleBackfillClass, "l");
		
		String[] flatUnAlignedSingleBackfill2 = {"l:LValueLong;:NR","singles:LFlatUnAlignedSingle;:NR", "singles2:LFlatUnAlignedSingle;:NR"};
		flatUnAlignedSingleBackfillClass2 = ValueTypeGenerator.generateValueClass("FlatUnAlignedSingleBackfill2", flatUnAlignedSingleBackfill2);
		makeFlatUnAlignedSingleBackfillClass2 = lookup.findStatic(flatUnAlignedSingleBackfillClass2, "makeObjectGeneric", MethodType.methodType(Object.class, Object.class, Object.class, Object.class));
		getUnAlignedSingleflatSingleBackfill2InstanceSingles = generateGenericGetter(flatUnAlignedSingleBackfillClass2, "singles");
		getUnAlignedSingleflatSingleBackfill2InstanceSingles2 = generateGenericGetter(flatUnAlignedSingleBackfillClass2, "singles2");
		getUnAlignedSingleflatSingleBackfill2InstanceL = generateGenericGetter(flatUnAlignedSingleBackfillClass2, "l");
		
		String[] flatUnAlignedObject = {"o:LValueObject;:NR", "o2:LValueObject;:NR"};
		flatUnAlignedObjectClass = ValueTypeGenerator.generateValueClass("FlatUnAlignedObject", flatUnAlignedObject);
		makeFlatUnAlignedObjectClass = lookup.findStatic(flatUnAlignedObjectClass, "makeObjectGeneric", MethodType.methodType(Object.class, Object.class, Object.class));
		getUnAlignedObjectO = generateGenericGetter(flatUnAlignedObjectClass, "o");
		getUnAlignedObjectO2 = generateGenericGetter(flatUnAlignedObjectClass, "o2");
		
		String[] flatUnAlignedObjectBackfill = {"objects:LFlatUnAlignedObject;:NR", "objects2:LFlatUnAlignedObject;:NR", "l:LValueLong;:NR"};
		flatUnAlignedObjectBackfillClass = ValueTypeGenerator.generateValueClass("FlatUnAlignedObjectBackfill", flatUnAlignedObjectBackfill);
		makeFlatUnAlignedObjectBackfillClass = lookup.findStatic(flatUnAlignedObjectBackfillClass, "makeObjectGeneric", MethodType.methodType(Object.class, Object.class, Object.class, Object.class));
		getUnAlignedObjectflatObjectBackfillInstanceObjects = generateGenericGetter(flatUnAlignedObjectBackfillClass, "objects");
		getUnAlignedObjectflatObjectBackfillInstanceObjects2 = generateGenericGetter(flatUnAlignedObjectBackfillClass, "objects2");
		getUnAlignedObjectflatObjectBackfillInstanceL = generateGenericGetter(flatUnAlignedObjectBackfillClass, "l");
		
		String[] flatUnAlignedObjectBackfill2 = {"o:LValueObject;:NR", "objects:LFlatUnAlignedObject;:NR", "l:LValueLong;:NR"};
		flatUnAlignedObjectBackfillClass2 = ValueTypeGenerator.generateValueClass("FlatUnAlignedObjectBackfill2", flatUnAlignedObjectBackfill2);
		makeFlatUnAlignedObjectBackfillClass2 = lookup.findStatic(flatUnAlignedObjectBackfillClass2, "makeObjectGeneric", MethodType.methodType(Object.class, Object.class, Object.class, Object.class));
		getUnAlignedObjectflatObjectBackfill2InstanceO = generateGenericGetter(flatUnAlignedObjectBackfillClass2, "o");
		getUnAlignedObjectflatObjectBackfill2InstanceObjects = generateGenericGetter(flatUnAlignedObjectBackfillClass2, "objects");
		getUnAlignedObjectflatObjectBackfill2InstanceL = generateGenericGetter(flatUnAlignedObjectBackfillClass2, "l");
	}
	
	@Test(priority=5, invocationCount=2)
	static public void testFlatLayoutsWithValueTypes() throws Throwable {	
		Object flatSingleBackfillInstance = makeFlatSingleBackfillClass.invoke(makeValueLong.invoke(defaultLong), makeValueObject.invoke(defaultObject), makeValueInt.invoke(defaultInt));
		assertEquals(getInt.invoke(getVTSingleI.invoke(flatSingleBackfillInstance)), defaultInt);
		assertEquals(getObject.invoke(getVTSingleO.invoke(flatSingleBackfillInstance)), defaultObject);
		assertEquals(getLong.invoke(getVTSingleL.invoke(flatSingleBackfillInstance)), defaultLong);
		
		Object objectBackfillInstance = makeFlatObjectBackfillClass.invoke(makeValueLong.invoke(defaultLong), makeValueObject.invoke(defaultObject));
		assertEquals(getObject.invoke(getVTObjectO.invoke(objectBackfillInstance)), defaultObject);
		assertEquals(getLong.invoke(getVTObjectL.invoke(objectBackfillInstance)), defaultLong);
		
		Object flatUnAlignedSingleBackfillInstance = makeFlatUnAlignedSingleBackfillClass.invoke(makeValueLong.invoke(defaultLong), makeFlatUnAlignedSingleClass.invoke(makeValueInt.invoke(defaultInt), makeValueInt.invoke(defaultIntNew)), makeValueObject.invoke(defaultObject));
		assertEquals(getLong.invoke(getUnAlignedSingleflatSingleBackfillInstanceL.invoke(flatUnAlignedSingleBackfillInstance)), defaultLong);
		assertEquals(getObject.invoke(getUnAlignedSingleflatSingleBackfillInstanceO.invoke(flatUnAlignedSingleBackfillInstance)), defaultObject);
		assertEquals(getInt.invoke(getUnAlignedSingleI.invoke(getUnAlignedSingleflatSingleBackfillInstanceSingles.invoke(flatUnAlignedSingleBackfillInstance))), defaultInt);
		assertEquals(getInt.invoke(getUnAlignedSingleI2.invoke(getUnAlignedSingleflatSingleBackfillInstanceSingles.invoke(flatUnAlignedSingleBackfillInstance))), defaultIntNew);
		
		Object flatUnAlignedSingleBackfill2Instance = makeFlatUnAlignedSingleBackfillClass2.invoke(makeValueLong.invoke(defaultLong), makeFlatUnAlignedSingleClass.invoke(makeValueInt.invoke(defaultInt), makeValueInt.invoke(defaultIntNew)), makeFlatUnAlignedSingleClass.invoke(makeValueInt.invoke(defaultInt), makeValueInt.invoke(defaultIntNew)));
		assertEquals(getLong.invoke(getUnAlignedSingleflatSingleBackfill2InstanceL.invoke(flatUnAlignedSingleBackfill2Instance)), defaultLong);
		assertEquals(getInt.invoke(getUnAlignedSingleI.invoke(getUnAlignedSingleflatSingleBackfill2InstanceSingles.invoke(flatUnAlignedSingleBackfill2Instance))), defaultInt);
		assertEquals(getInt.invoke(getUnAlignedSingleI2.invoke(getUnAlignedSingleflatSingleBackfill2InstanceSingles.invoke(flatUnAlignedSingleBackfill2Instance))), defaultIntNew);
		assertEquals(getInt.invoke(getUnAlignedSingleI.invoke(getUnAlignedSingleflatSingleBackfill2InstanceSingles2.invoke(flatUnAlignedSingleBackfill2Instance))), defaultInt);
		assertEquals(getInt.invoke(getUnAlignedSingleI2.invoke(getUnAlignedSingleflatSingleBackfill2InstanceSingles2.invoke(flatUnAlignedSingleBackfill2Instance))), defaultIntNew);
		
		Object flatUnAlignedObjectBackfillInstance = makeFlatUnAlignedObjectBackfillClass.invoke(makeFlatUnAlignedObjectClass.invoke(makeValueObject.invoke(defaultObject), makeValueObject.invoke(defaultObjectNew)), makeFlatUnAlignedObjectClass.invoke(makeValueObject.invoke(defaultObject), makeValueObject.invoke(defaultObjectNew)), makeValueLong.invoke(defaultLong));
		assertEquals(getLong.invoke(getUnAlignedObjectflatObjectBackfillInstanceL.invoke(flatUnAlignedObjectBackfillInstance)), defaultLong);
		assertEquals(getObject.invoke(getUnAlignedObjectO.invoke(getUnAlignedObjectflatObjectBackfillInstanceObjects.invoke(flatUnAlignedObjectBackfillInstance))), defaultObject);
		assertEquals(getObject.invoke(getUnAlignedObjectO2.invoke(getUnAlignedObjectflatObjectBackfillInstanceObjects.invoke(flatUnAlignedObjectBackfillInstance))), defaultObjectNew);
		assertEquals(getObject.invoke(getUnAlignedObjectO.invoke(getUnAlignedObjectflatObjectBackfillInstanceObjects2.invoke(flatUnAlignedObjectBackfillInstance))), defaultObject);
		assertEquals(getObject.invoke(getUnAlignedObjectO2.invoke(getUnAlignedObjectflatObjectBackfillInstanceObjects2.invoke(flatUnAlignedObjectBackfillInstance))), defaultObjectNew);
		
		Object flatUnAlignedObjectBackfill2Instance = makeFlatUnAlignedObjectBackfillClass2.invoke(makeValueObject.invoke(defaultObject), makeFlatUnAlignedObjectClass.invoke(makeValueObject.invoke(defaultObject), makeValueObject.invoke(defaultObjectNew)), makeValueLong.invoke(defaultLong));
		assertEquals(getLong.invoke(getUnAlignedObjectflatObjectBackfill2InstanceL.invoke(flatUnAlignedObjectBackfill2Instance)), defaultLong);
		assertEquals(getObject.invoke(getUnAlignedObjectO.invoke(getUnAlignedObjectflatObjectBackfill2InstanceObjects.invoke(flatUnAlignedObjectBackfill2Instance))), defaultObject);
		assertEquals(getObject.invoke(getUnAlignedObjectO2.invoke(getUnAlignedObjectflatObjectBackfill2InstanceObjects.invoke(flatUnAlignedObjectBackfill2Instance))), defaultObjectNew);
		assertEquals(getObject.invoke(getUnAlignedObjectflatObjectBackfill2InstanceO.invoke(flatUnAlignedObjectBackfill2Instance)), defaultObject);		
	}

	@Test(priority=4, invocationCount=2)
	static public void testFlatLayoutsWithRecursiveLongs() throws Throwable {
		ValueTypeDoubleLong doubleLongInstance = new ValueTypeDoubleLong(new ValueTypeLong(defaultLong), defaultLongNew);
		assertEquals(doubleLongInstance.getL().getL(), defaultLong);
		assertEquals(doubleLongInstance.getL2(), defaultLongNew);

		ValueTypeQuadLong quadLongInstance = new ValueTypeQuadLong(doubleLongInstance, new ValueTypeLong(defaultLongNew2), defaultLongNew3);
		assertEquals(quadLongInstance.getL().getL().getL(), defaultLong);
		assertEquals(quadLongInstance.getL().getL2(), defaultLongNew);
		assertEquals(quadLongInstance.getL2().getL(), defaultLongNew2);
		assertEquals(quadLongInstance.getL3(), defaultLongNew3);

		ValueTypeDoubleQuadLong doubleQuadLongInstance = new ValueTypeDoubleQuadLong(quadLongInstance, doubleLongInstance, new ValueTypeLong(defaultLongNew4), defaultLongNew5);

		assertEquals(doubleQuadLongInstance.getL().getL().getL().getL(), defaultLong);
		assertEquals(doubleQuadLongInstance.getL().getL().getL2(), defaultLongNew);
		assertEquals(doubleQuadLongInstance.getL().getL2().getL(), defaultLongNew2);
		assertEquals(doubleQuadLongInstance.getL().getL3(), defaultLongNew3);
		assertEquals(doubleQuadLongInstance.getL2().getL().getL(), defaultLong);
		assertEquals(doubleQuadLongInstance.getL2().getL2(), defaultLongNew);
		assertEquals(doubleQuadLongInstance.getL3().getL(), defaultLongNew4);
		assertEquals(doubleQuadLongInstance.getL4(), defaultLongNew5);
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
			"makeObjectGeneric", MethodType.methodType(Object.class, Object.class,
					Object.class, Object.class, Object.class, Object.class, Object.class, Object.class));
		/*
		 * Getters are created in array getterAndSetter[i][0] according to the order of fields i
		 * Setters are created in array getterAndSetter[i][1] according to the order of fields i
		 */
		assortedValueWithObjectAlignmentGetterList = generateGenericGetterList(assortedValueWithObjectAlignmentClass, typeWithObjectAlignmentFields);
	}

	@Test(priority=5, invocationCount=2)
	static public void testAssortedValueWithObjectAlignment() throws Throwable {
		Object assortedValueWithObjectAlignment = createAssorted(makeAssortedValueWithObjectAlignment, typeWithObjectAlignmentFields);
		checkFieldAccessMHOfAssortedType(assortedValueWithObjectAlignmentGetterList, assortedValueWithObjectAlignment, typeWithObjectAlignmentFields, true);
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
		assortedRefWithObjectAlignmentClass = ValueTypeGenerator.generateRefClass("AssortedRefWithObjectAlignment", typeWithObjectAlignmentFields);

		makeAssortedRefWithObjectAlignment = lookup.findStatic(assortedRefWithObjectAlignmentClass,
				"makeObjectGeneric", MethodType.methodType(Object.class, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class));
		/*
		 * Getters are created in array getterAndSetter[i][0] according to the order of fields i
		 * Setters are created in array getterAndSetter[i][1] according to the order of fields i
		 */
		assortedRefWithObjectAlignmentGetterList = generateGenericGetterAndSetter(assortedRefWithObjectAlignmentClass, typeWithObjectAlignmentFields);
	}

	@Test(priority=5, invocationCount=2)
	static public void testAssortedRefWithObjectAlignment() throws Throwable {
		Object assortedRefWithObjectAlignment = createAssorted(makeAssortedRefWithObjectAlignment, typeWithObjectAlignmentFields);
		checkFieldAccessMHOfAssortedType(assortedRefWithObjectAlignmentGetterList, assortedRefWithObjectAlignment, typeWithObjectAlignmentFields, false);
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
			"makeObjectGeneric", MethodType.methodType(Object.class, Object.class,
					Object.class, Object.class, Object.class, Object.class, Object.class));
		/*
		 * Getters are created in array getterAndSetter[i][0] according to the order of fields i
		 * Setters are created in array getterAndSetter[i][1] according to the order of fields i
		 */
		assortedValueWithSingleAlignmentGetterList = generateGenericGetterList(assortedValueWithSingleAlignmentClass, typeWithSingleAlignmentFields);
	}

	@Test(priority=5, invocationCount=2)
	static public void testAssortedValueWithSingleAlignment() throws Throwable {
		Object assortedValueWithSingleAlignment = createAssorted(makeAssortedValueWithSingleAlignment, typeWithSingleAlignmentFields);
		checkFieldAccessMHOfAssortedType(assortedValueWithSingleAlignmentGetterList, assortedValueWithSingleAlignment, typeWithSingleAlignmentFields, true);
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
		assortedRefWithSingleAlignmentClass = ValueTypeGenerator.generateRefClass("AssortedRefWithSingleAlignment", typeWithSingleAlignmentFields);

		makeAssortedRefWithSingleAlignment = lookup.findStatic(assortedRefWithSingleAlignmentClass,
				"makeObjectGeneric", MethodType.methodType(Object.class, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class));
		/*
		 * Getters are created in array getterAndSetter[i][0] according to the order of fields i
		 * Setters are created in array getterAndSetter[i][1] according to the order of fields i
		 */
		assortedRefWithSingleAlignmentGetterList = generateGenericGetterAndSetter(assortedRefWithSingleAlignmentClass, typeWithSingleAlignmentFields);
	}

	@Test(priority=5, invocationCount=2)
	static public void testAssortedRefWithSingleAlignment() throws Throwable {
		Object assortedRefWithSingleAlignment = createAssorted(makeAssortedRefWithSingleAlignment, typeWithSingleAlignmentFields);
		checkFieldAccessMHOfAssortedType(assortedRefWithSingleAlignmentGetterList, assortedRefWithSingleAlignment, typeWithSingleAlignmentFields, false);
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
		String[] largeFields = {
				"val1:LValueObject;:NR",
				"val2:LValueObject;:NR",
				"val3:LValueObject;:NR",
				"val4:LValueObject;:NR",
				"val5:LValueObject;:NR",
				"val6:LValueObject;:NR",
				"val7:LValueObject;:NR",
				"val8:LValueObject;:NR",
				"val9:LValueObject;:NR",
				"val10:LValueObject;:NR",
				"val11:LValueObject;:NR",
				"val12:LValueObject;:NR",
				"val13:LValueObject;:NR",
				"val14:LValueObject;:NR",
				"val15:LValueObject;:NR",
				"val16:LValueObject;:NR"};

		largeObjectValueClass = ValueTypeGenerator.generateValueClass("LargeObject", largeFields);
		makeLargeObjectValue = lookup.findStatic(largeObjectValueClass, "makeObjectGeneric",
				MethodType.methodType(Object.class, Object.class, Object.class, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class, Object.class));
		/*
		 * Getters are created in array getterList[i][0] according to the order of fields i
		 */
		MethodHandle[][] getterList = generateGenericGetterList(largeObjectValueClass, largeFields);

		getObjects = new MethodHandle[16];
		for (int i = 0; i < 16; i++) {
			getObjects[i] = getterList[i][0];
		}

		Object largeObjectValue = createAssorted(makeLargeObjectValue, largeFields);
		checkFieldAccessMHOfAssortedType(getterList, largeObjectValue, largeFields, true);

		/*
		 * create MegaObject
		 */
		String[] megaFields = {
				"val1:LLargeObject;:NR",
				"val2:LLargeObject;:NR",
				"val3:LLargeObject;:NR",
				"val4:LLargeObject;:NR",
				"val5:LLargeObject;:NR",
				"val6:LLargeObject;:NR",
				"val7:LLargeObject;:NR",
				"val8:LLargeObject;:NR",
				"val9:LLargeObject;:NR",
				"val10:LLargeObject;:NR",
				"val11:LLargeObject;:NR",
				"val12:LLargeObject;:NR",
				"val13:LLargeObject;:NR",
				"val14:LLargeObject;:NR",
				"val15:LLargeObject;:NR",
				"val16:LLargeObject;:NR"};

		megaObjectValueClass = ValueTypeGenerator.generateValueClass("MegaObject", megaFields);
		makeMegaObjectValue = lookup.findStatic(megaObjectValueClass, "makeObjectGeneric",
				MethodType.methodType(Object.class, Object.class, Object.class, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class, Object.class));

		/*
		 * Getters are created in array getterList[i][0] according to the order of fields i
		 */
		MethodHandle[][] megaGetterList = generateGenericGetterList(megaObjectValueClass, megaFields);
		Object megaObject = createAssorted(makeMegaObjectValue, megaFields);
		checkFieldAccessMHOfAssortedType(megaGetterList, megaObject, megaFields, true);
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
		String[] largeFields = {
				"val1:LValueObject;:NR",
				"val2:LValueObject;:NR",
				"val3:LValueObject;:NR",
				"val4:LValueObject;:NR",
				"val5:LValueObject;:NR",
				"val6:LValueObject;:NR",
				"val7:LValueObject;:NR",
				"val8:LValueObject;:NR",
				"val9:LValueObject;:NR",
				"val10:LValueObject;:NR",
				"val11:LValueObject;:NR",
				"val12:LValueObject;:NR",
				"val13:LValueObject;:NR",
				"val14:LValueObject;:NR",
				"val15:LValueObject;:NR",
				"val16:LValueObject;:NR"};

		Class<?> largeRefClass = ValueTypeGenerator.generateRefClass("LargeRef", largeFields);
		MethodHandle makeLargeObjectRef = lookup.findStatic(largeRefClass, "makeObjectGeneric",
				MethodType.methodType(Object.class, Object.class, Object.class, Object.class, Object.class,
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
		String[] megaFields = {
				"val1:LLargeObject;:NR",
				"val2:LLargeObject;:NR",
				"val3:LLargeObject;:NR",
				"val4:LLargeObject;:NR",
				"val5:LLargeObject;:NR",
				"val6:LLargeObject;:NR",
				"val7:LLargeObject;:NR",
				"val8:LLargeObject;:NR",
				"val9:LLargeObject;:NR",
				"val10:LLargeObject;:NR",
				"val11:LLargeObject;:NR",
				"val12:LLargeObject;:NR",
				"val13:LLargeObject;:NR",
				"val14:LLargeObject;:NR",
				"val15:LLargeObject;:NR",
				"val16:LLargeObject;:NR"};
		Class<?> megaObjectClass = ValueTypeGenerator.generateRefClass("MegaRef", megaFields);
		MethodHandle makeMegaObjectRef = lookup.findStatic(megaObjectClass, "makeObjectGeneric",
				MethodType.methodType(Object.class, Object.class, Object.class, Object.class, Object.class,
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
	 * NULL should never be observed for null-restricted fields.
	 */
	@Test(priority=4)
	static public void testDefaultValues() throws Throwable {
		/* Test with assorted value object with long alignment */
		MethodHandle makeValueTypeDefaultValueWithLong = lookup.findStatic(assortedValueWithLongAlignmentClass,
				"makeDefaultValue", MethodType.methodType(assortedValueWithLongAlignmentClass));

		Object assortedValueWithLongAlignment = makeValueTypeDefaultValueWithLong.invoke();
		for (int i = 0; i < 7; i++) {
			assertNotNull(assortedValueWithLongAlignmentGetterList[i][0].invoke(assortedValueWithLongAlignment));
		}

		/* Test with assorted ref object with long alignment */
		MethodHandle makeRefDefaultValueWithLong = lookup.findStatic(assortedRefWithLongAlignmentClass,
				"makeDefaultValue", MethodType.methodType(assortedRefWithLongAlignmentClass));
		Object assortedRefWithLongAlignment = makeRefDefaultValueWithLong.invoke();
		for (int i = 0; i < 7; i++) {
			assertNotNull(assortedRefWithLongAlignmentGetterAndSetter[i][0].invoke(assortedRefWithLongAlignment));
		}

		/* Test with flattened line 2D */
		MethodHandle makeDefaultValueFlattenedLine2D = lookup.findStatic(flattenedLine2DClass, "makeDefaultValue", MethodType.methodType(flattenedLine2DClass));
		Object lineObject = makeDefaultValueFlattenedLine2D.invoke();
		assertNotNull(getFlatSt.invoke(lineObject));
		assertNotNull(getFlatEn.invoke(lineObject));

		/* Test with triangle 2D */
		MethodHandle makeDefaultValueTriangle2D = lookup.findStatic(triangle2DClass, "makeDefaultValue", MethodType.methodType(triangle2DClass));
		Object triangleObject = makeDefaultValueTriangle2D.invoke();
		assertNotNull(getV1.invoke(triangleObject));
		assertNotNull(getV2.invoke(triangleObject));
		assertNotNull(getV3.invoke(triangleObject));
	}
	
	@Test(priority=5, invocationCount=2)
	static public void testStaticFieldsWithObjectAlignmenDefaultValues() throws Throwable {
		for (MethodHandle getterAndSetter[] : staticFieldsWithObjectAlignmentGenericGetterAndSetter) {
			assertNotNull(getterAndSetter[0].invoke());
		}
	}
	
	@Test(priority=5, invocationCount=2)
	static public void testStaticFieldsWithLongAlignmenDefaultValues() throws Throwable {
		for (MethodHandle getterAndSetter[] : staticFieldsWithLongAlignmentGenericGetterAndSetter) {
			assertNotNull(getterAndSetter[0].invoke());
		}
	}
	
	@Test(priority=5, invocationCount=2)
	static public void testStaticFieldsWithSingleAlignmenDefaultValues() throws Throwable {
		for (MethodHandle getterAndSetter[] : staticFieldsWithSingleAlignmentGenericGetterAndSetter) {
			assertNotNull(getterAndSetter[0].invoke());
		}
	}
	
	@Test(priority=1)
	static public void testCyclicalStaticFieldDefaultValues1() throws Throwable {
		String[] cycleA1 = { "val1:LCycleB1;:static" };
		String[] cycleB1 = { "val1:LCycleA1;:static" };
		
		Class<?> cycleA1Class = ValueTypeGenerator.generateValueClass("CycleA1", cycleA1);
		Class<?> cycleB1Class = ValueTypeGenerator.generateValueClass("CycleB1", cycleB1);
		
		MethodHandle makeCycleA1 = lookup.findStatic(cycleA1Class, "makeObjectGeneric", MethodType.methodType(Object.class));
		MethodHandle makeCycleB1 = lookup.findStatic(cycleB1Class, "makeObjectGeneric", MethodType.methodType(Object.class));
		
		makeCycleA1.invoke();
		makeCycleB1.invoke();
	}
	
	@Test(priority=1)
	static public void testCyclicalStaticFieldDefaultValues2() throws Throwable {
		String[] cycleA2 = { "val1:LCycleB2;:static" };
		String[] cycleB2 = { "val1:LCycleC2;:static" };
		String[] cycleC2 = { "val1:LCycleA2;:static" };
		
		Class<?> cycleA2Class = ValueTypeGenerator.generateValueClass("CycleA2", cycleA2);
		Class<?> cycleB2Class = ValueTypeGenerator.generateValueClass("CycleB2", cycleB2);
		Class<?> cycleC2Class = ValueTypeGenerator.generateValueClass("CycleC2", cycleC2);
		
		MethodHandle makeCycleA2 = lookup.findStatic(cycleA2Class, "makeObjectGeneric", MethodType.methodType(Object.class));
		MethodHandle makeCycleB2 = lookup.findStatic(cycleB2Class, "makeObjectGeneric", MethodType.methodType(Object.class));
		MethodHandle makeCycleC2 = lookup.findStatic(cycleB2Class, "makeObjectGeneric", MethodType.methodType(Object.class));
		
		makeCycleA2.invoke();
		makeCycleB2.invoke();
		makeCycleC2.invoke();
	}
	
	@Test(priority=1)
	static public void testCyclicalStaticFieldDefaultValues3() throws Throwable {
		String[] cycleA3 = { "val1:LCycleA3;:static" };
		
		Class<?> cycleA3Class = ValueTypeGenerator.generateValueClass("CycleA3", cycleA3);
		
		MethodHandle makeCycleA3 = lookup.findStatic(cycleA3Class, "makeObjectGeneric", MethodType.methodType(Object.class));
		
		makeCycleA3.invoke();
	}

	@Test(priority=4)
	static public void testStaticFieldsWithSingleAlignment() throws Throwable {
		String[] fields = {
			"tri:LTriangle2D;:static",
			"point:LPoint2D;:static",
			"line:LFlattenedLine2D;:static",
			"i:LValueInt;:static",
			"f:LValueFloat;:static",
			"tri2:LTriangle2D;:static"};
		classWithOnlyStaticFieldsWithSingleAlignment = ValueTypeGenerator.generateValueClass("ClassWithOnlyStaticFieldsWithSingleAlignment", fields);
		staticFieldsWithSingleAlignmentGenericGetterAndSetter = generateStaticGenericGetterAndSetter(classWithOnlyStaticFieldsWithSingleAlignment, fields);
		
		initializeStaticFields(classWithOnlyStaticFieldsWithSingleAlignment, staticFieldsWithSingleAlignmentGenericGetterAndSetter, fields);
		checkFieldAccessMHOfStaticType(staticFieldsWithSingleAlignmentGenericGetterAndSetter, fields);
	}

	@Test(priority=4)
	static public void testStaticFieldsWithLongAlignment() throws Throwable {
		String[] fields = {
			"point:LPoint2D;:static",
			"line:LFlattenedLine2D;:static",
			"o:LValueObject;:static",
			"l:LValueLong;:static",
			"d:LValueDouble;:static",
			"i:LValueInt;:static",
			"tri:LTriangle2D;:static"};
		classWithOnlyStaticFieldsWithLongAlignment = ValueTypeGenerator.generateValueClass("ClassWithOnlyStaticFieldsWithLongAlignment", fields);
		staticFieldsWithLongAlignmentGenericGetterAndSetter = generateStaticGenericGetterAndSetter(classWithOnlyStaticFieldsWithLongAlignment, fields);
		
		initializeStaticFields(classWithOnlyStaticFieldsWithLongAlignment, staticFieldsWithLongAlignmentGenericGetterAndSetter, fields);
		checkFieldAccessMHOfStaticType(staticFieldsWithLongAlignmentGenericGetterAndSetter, fields);
	}

	@Test(priority=4)
	static public void testStaticFieldsWithObjectAlignment() throws Throwable {
		String[] fields = {
			"tri:LTriangle2D;:static",
			"point:LPoint2D;:static",
			"line:LFlattenedLine2D;:static",
			"o:LValueObject;:static",
			"i:LValueInt;:static",
			"f:LValueFloat;:static",
			"tri2:LTriangle2D;:static"};
		classWithOnlyStaticFieldsWithObjectAlignment = ValueTypeGenerator.generateValueClass("ClassWithOnlyStaticFieldsWithObjectAlignment", fields);
		staticFieldsWithObjectAlignmentGenericGetterAndSetter = generateStaticGenericGetterAndSetter(classWithOnlyStaticFieldsWithObjectAlignment, fields);

		initializeStaticFields(classWithOnlyStaticFieldsWithObjectAlignment, staticFieldsWithObjectAlignmentGenericGetterAndSetter, fields);
		checkFieldAccessMHOfStaticType(staticFieldsWithObjectAlignmentGenericGetterAndSetter, fields);
	}

	/*
	 * Create large number of value types and instantiate them
	 *
	 * value Point2D {
	 * 	int x;
	 * 	int y;
	 * }
	 */
	@Test(priority=1)
	static public void testCreateLargeNumberOfPoint2D() throws Throwable {
		String[] fields = {"x:I", "y:I"};
		for (int valueIndex = 0; valueIndex < objectGCScanningIterationCount; valueIndex++) {
			String className = "Point2D" + valueIndex;		
			Class<?> point2DXClass = ValueTypeGenerator.generateValueClass(className, fields);
			/* findStatic will trigger class resolution */
			MethodHandle makePoint2DX = lookup.findStatic(point2DXClass, "makeObjectGeneric", MethodType.methodType(Object.class, Object.class, Object.class));
			if (0 == (valueIndex % 100)) {
				System.gc();
			}
		}
	}

	/*
	 * Create Array Objects with Point Class without initialization
	 * The array should be set to a Default Value.
	 */
	@Test(priority=4, invocationCount=2)
	static public void testDefaultValueInPointArray() throws Throwable {
		Object pointArray = Array.newInstance(point2DClass, genericArraySize);
		for (int i = 0; i < genericArraySize; i++) {
			Object pointObject = Array.get(pointArray, i);
			assertNotNull(pointObject);
		}
	}

	/**
	 * Create multi dimensional array with Point Class without initialization.
	 * Set each array value to a default value and check field access method handler.
	 */
	@Test(priority=4)
	static public void testDefaultValueInPointInstanceMultiArray() throws Throwable {
		Object pointArray = Array.newInstance(point2DClass, new int[]{genericArraySize, genericArraySize});
		String[] fields = {"x:I", "y:I"};
		MethodHandle[][] getterList = generateGenericGetterList(point2DClass, fields);
		for (int i = 0; i < genericArraySize; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				Object pointNew = createPoint2D(defaultPointPositions1);
				Array.set(Array.get(pointArray,i),j, pointNew);
			}
		}
		for (int i = 0; i < genericArraySize; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				Object pointObject = Array.get(Array.get(pointArray,i),j);
				checkFieldAccessMHOfAssortedType(getterList, pointObject, fields, true);
			}
		}
	}

	/**
	 * Create multi dimensional array with Point Class using bytecode instruction
	 * multianewarray.
	 * Set each array value to a default value and check field access method handler.
	 */
	@Test(priority=4)
	static public void testDefaultValueInPointByteCodeMultiArray() throws Throwable {
		MethodHandle makePointArray = lookup.findStatic(point2DClass, "generate2DMultiANewArray", MethodType.methodType(Object.class, int.class, int.class));
		Object pointArray = makePointArray.invoke(genericArraySize, genericArraySize);
		String[] fields = {"x:I", "y:I"};
		MethodHandle[][] getterList = generateGenericGetterList(point2DClass, fields);
		for (int i = 0; i < genericArraySize; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				Object pointNew = createPoint2D(defaultPointPositions1);
				Array.set(Array.get(pointArray,i),j, pointNew);
			}
		}
		for (int i = 0; i < genericArraySize; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				Object pointObject = Array.get(Array.get(pointArray,i),j);
				checkFieldAccessMHOfAssortedType(getterList, pointObject, fields, true);
			}
		}
	}

	/*
	 * Create Array Objects with Flattened Line without initialization
	 * Check the fields of each element in arrays. No field should be NULL.
	 */
	@Test(priority=4, invocationCount=2)
	static public void testDefaultValueInLineArray() throws Throwable {
		Object flattenedLineArray = Array.newInstance(flattenedLine2DClass, genericArraySize);
		for (int i = 0; i < genericArraySize; i++) {
			Object lineObject = Array.get(flattenedLineArray, i);
			assertNotNull(lineObject);
			assertNotNull(getFlatSt.invoke(lineObject));
			assertNotNull(getFlatEn.invoke(lineObject));
		}
	}

	/**
	 * Create multi dimensional array with Line Class without initialization.
	 * Set each array value to a default value and check field access method handler.
	 */
	@Test(priority=4)
	static public void testDefaultValueInLineInstanceMultiArray() throws Throwable {
		Object flattenedLineArray = Array.newInstance(flattenedLine2DClass, new int[]{genericArraySize, genericArraySize});
		String[] fields = {"st:LPoint;:NR","en:LPoint;:NR"};
		MethodHandle[][] getterList = generateGenericGetterList(flattenedLine2DClass, fields);
		for (int i = 0; i < genericArraySize; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				Object lineNew = createFlattenedLine2D(defaultLinePositions1);
				Array.set(Array.get(flattenedLineArray,i),j, lineNew);
			}
		}
		for (int i = 0; i < genericArraySize; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				Object lineObject = Array.get(Array.get(flattenedLineArray,i),j);
				checkFieldAccessMHOfAssortedType(getterList, lineObject, fields, true);
			}
		}
	}

	/**
	 * Create multi dimensional array with Line Class using bytecode instruction
	 * multianewarray.
	 * Set each array value to a default value and check field access method handler.
	 */
	@Test(priority=4)
	static public void testDefaultValueInLineByteCodeMultiArray() throws Throwable {
		MethodHandle makeLineArray = lookup.findStatic(flattenedLine2DClass, "generate2DMultiANewArray", MethodType.methodType(Object.class, int.class, int.class));
		Object flattenedLineArray = makeLineArray.invoke(genericArraySize, genericArraySize);
		String[] fields = {"st:LPoint;:NR","en:LPoint;:NR"};
		MethodHandle[][] getterList = generateGenericGetterList(flattenedLine2DClass, fields);
		for (int i = 0; i < genericArraySize; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				Object lineNew = createFlattenedLine2D(defaultLinePositions1);
				Array.set(Array.get(flattenedLineArray,i),j, lineNew);
			}
		}
		for (int i = 0; i < genericArraySize; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				Object lineObject = Array.get(Array.get(flattenedLineArray,i),j);
				checkFieldAccessMHOfAssortedType(getterList, lineObject, fields, true);
			}
		}
	}

	/*
	 * Create Array Objects with triangle class without initialization
	 * Check the fields of each element in arrays. No field should be NULL.
	 */
	@Test(priority=4, invocationCount=2)
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

	/**
	 * Create multi dimensional array with Triangle Class without initialization.
	 * Set each array value to a default value and check field access method handler.
	 */
	@Test(priority=4)
	static public void testDefaultValueInTriangleInstanceMultiArray() throws Throwable {
		Object triangleArray = Array.newInstance(triangle2DClass, new int[]{genericArraySize, genericArraySize});
		String[] fields = {"v1:LFlattenedLine2D;:NR", "v2:LFlattenedLine2D;:NR", "v3:LFlattenedLine2D;:NR"};
		MethodHandle[][] getterList = generateGenericGetterList(triangle2DClass, fields);
		for (int i = 0; i < genericArraySize; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				Object triNew = createTriangle2D(new int[][][] {defaultLinePositions1, defaultLinePositions1, defaultLinePositions1});
				Array.set(Array.get(triangleArray,i),j, triNew);
			}
		}
		for (int i = 0; i < genericArraySize; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				Object triangleObject = Array.get(Array.get(triangleArray,i),j);
				checkFieldAccessMHOfAssortedType(getterList, triangleObject, fields, true);
			}
		}
	}

	/**
	 * Create multi dimensional array with Triangle Class using bytecode instruction
	 * multianewarray.
	 * Set each array value to a default value and check field access method handler.
	 */
	@Test(priority=4)
	static public void testDefaultValueInTriangleByteCodeMultiArray() throws Throwable {
		MethodHandle makeTriangleArray = lookup.findStatic(triangle2DClass, "generate2DMultiANewArray", MethodType.methodType(Object.class, int.class, int.class));
		Object triangleArray = makeTriangleArray.invoke(genericArraySize, genericArraySize);
		String[] fields = {"v1:LFlattenedLine2D;:NR", "v2:LFlattenedLine2D;:NR", "v3:LFlattenedLine2D;:NR"};
		MethodHandle[][] getterList = generateGenericGetterList(triangle2DClass, fields);
		for (int i = 0; i < genericArraySize; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				Object triNew = createTriangle2D(new int[][][] {defaultLinePositions1, defaultLinePositions1, defaultLinePositions1});
				Array.set(Array.get(triangleArray,i),j, triNew);
			}
		}
		for (int i = 0; i < genericArraySize; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				Object triangleObject = Array.get(Array.get(triangleArray,i),j);
				checkFieldAccessMHOfAssortedType(getterList, triangleObject, fields, true);
			}
		}
	}

	/*
	 * Create an Array Object with assortedValueWithLongAlignment class without initialization
	 * Check the fields of each element in arrays. No field should be NULL.
	 */
	@Test(priority=4, invocationCount=2)
	static public void testDefaultValueInAssortedValueWithLongAlignmentArray() throws Throwable {
		Object assortedValueWithLongAlignmentArray = Array.newInstance(assortedValueWithLongAlignmentClass, genericArraySize);
		for (int i = 0; i < genericArraySize; i++) {
			Object assortedValueWithLongAlignmentObject = Array.get(assortedValueWithLongAlignmentArray, i);
			assertNotNull(assortedValueWithLongAlignmentObject);
			for (int j = 0; j < 7; j++) {
				assertNotNull(assortedValueWithLongAlignmentGetterList[j][0].invoke(assortedValueWithLongAlignmentObject));
			}
		}
	}

	/**
	 * Create multi dimensional array with assortedValueWithLongAlignment without initialization.
	 * Set each array value to a default value and check field access method handler.
	 */
	@Test(priority=5)
	static public void testDefaultValueInAssortedValueWithLongAlignmenInstanceMultiArray() throws Throwable {
		Object assortedValueWithLongAlignmentArray = Array.newInstance(assortedValueWithLongAlignmentClass, new int[]{genericArraySize, genericArraySize});
		for (int i = 0; i < genericArraySize; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				Object assortedValueWithLongAlignment = createAssorted(makeAssortedValueWithLongAlignment, typeWithLongAlignmentFields);
				Array.set(Array.get(assortedValueWithLongAlignmentArray,i),j, assortedValueWithLongAlignment);
			}
		}
		for (int i = 0; i < genericArraySize; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				Object assortedValueWithLongAlignmentObject = Array.get(Array.get(assortedValueWithLongAlignmentArray,i),j);
				checkFieldAccessMHOfAssortedType(assortedValueWithLongAlignmentGetterList, assortedValueWithLongAlignmentObject, typeWithLongAlignmentFields, true);
			}
		}
	}

	/**
	 * Create multi dimensional array with assortedValueWithLongAlignment using bytecode instruction
	 * multianewarray.
	 * Set each array value to a default value and check field access method handler.
	 */
	@Test(priority=5)
	static public void testDefaultValueInAssortedValueWithLongAlignmentByteCodeMultiArray() throws Throwable {
		MethodHandle makeAssortedValueWithLongAlignmentArray = lookup.findStatic(assortedValueWithLongAlignmentClass, "generate2DMultiANewArray", MethodType.methodType(Object.class, int.class, int.class));
		Object assortedValueWithLongAlignmentArray = makeAssortedValueWithLongAlignmentArray.invoke(genericArraySize, genericArraySize);
		for (int i = 0; i < genericArraySize; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				Object assortedValueWithLongAlignment = createAssorted(makeAssortedValueWithLongAlignment, typeWithLongAlignmentFields);
				Array.set(Array.get(assortedValueWithLongAlignmentArray,i),j, assortedValueWithLongAlignment);
			}
		}
		for (int i = 0; i < genericArraySize; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				Object assortedValueWithLongAlignmentObject = Array.get(Array.get(assortedValueWithLongAlignmentArray,i),j);
				checkFieldAccessMHOfAssortedType(assortedValueWithLongAlignmentGetterList, assortedValueWithLongAlignmentObject, typeWithLongAlignmentFields, true);
			}
		}
	}

	/*
	 * Create a 2D array of valueTypes, verify that the default elements are null.
	 */
	@Test(priority=5, invocationCount=2)
	static public void testMultiDimentionalArrays() throws Throwable {
		Class<?> assortedValueWithLongAlignment2DClass = Array.newInstance(assortedValueWithLongAlignmentClass, 1).getClass();
		Class<?> assortedValueWithSingleAlignment2DClass = Array.newInstance(assortedValueWithSingleAlignmentClass, 1).getClass();
		
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
	@Test(priority=4, invocationCount=2)
	static public void testDefaultValueInAssortedRefWithLongAlignmentArray() throws Throwable {
		Object assortedRefWithLongAlignmentArray = Array.newInstance(assortedRefWithLongAlignmentClass, genericArraySize);
		for (int i = 0; i < genericArraySize; i++) {
			Object assortedRefWithLongAlignmentObject = Array.get(assortedRefWithLongAlignmentArray, i);
			assertNull(assortedRefWithLongAlignmentObject);
		}
	}

	/*
	 * Re-enable when CheckedType support is available.
	 * https://github.com/eclipse-openj9/openj9/issues/19764
	 *
	 * Ensure that casting null to a value type class will throw a null pointer exception
	 * This test is disabled since the latest spec from
	 * https://cr.openjdk.org/~dlsmith/jep401/jep401-20230519/specs/types-cleanup-jvms.html
	 * no longer requires null check on the objectref for null-restricted value type class
	 */
	@Test(enabled=false, priority=1, expectedExceptions=NullPointerException.class)
	static public void testCheckCastNullRestrictedTypeOnNull() throws Throwable {
		String[] fields = {"longField:J"};
		Class<?> valueClass = ValueTypeGenerator.generateValueClass("TestCheckCastNullRestrictedTypeOnNull", fields);
		MethodHandle checkCastValueTypeOnNull = lookup.findStatic(valueClass, "testCheckCastNullRestrictedTypeOnNull", MethodType.methodType(Object.class));
		checkCastValueTypeOnNull.invoke();
	}

	/*
	 * Ensure that casting a non null value type to a valid value type will pass
	 */
	@Test(priority=1)
	static public void testCheckCastNullRestrictedTypeOnNonNullType() throws Throwable {
		String[] fields = {"longField:J"};
		Class<?> valueClass = ValueTypeGenerator.generateValueClass("TestCheckCastNullRestrictedTypeOnNonNullType", fields);
		MethodHandle checkCastValueTypeOnNonNullType = lookup.findStatic(valueClass, "testCheckCastNullRestrictedTypeOnNonNullType", MethodType.methodType(Object.class));
		checkCastValueTypeOnNonNullType.invoke();
	}

	/*
	 * Ensure that casting null to a reference type class will pass
	 */
	@Test(priority=1)
	static public void testCheckCastNullableTypeOnNull() throws Throwable {
		String[] fields = {"longField:J"};
		Class<?> refClass = ValueTypeGenerator.generateRefClass("TestCheckCastNullableTypeOnNull", fields);
		MethodHandle checkCastRefClassOnNull = lookup.findStatic(refClass, "testCheckCastNullableTypeOnNull", MethodType.methodType(Object.class));
		checkCastRefClassOnNull.invoke();
	}

	@Test(priority=1)
	static public void testClassIsInstanceNullableArrays() throws Throwable {
		ValueTypePoint2D[] nonNullableArray = (ValueTypePoint2D[]) ValueClass.newNullRestrictedArray(ValueTypePoint2D.class, 1);
		ValueTypePoint2D[] nullableArray = new ValueTypePoint2D[1];

		assertTrue(nonNullableArray.getClass().isInstance(nonNullableArray));
		assertTrue(nullableArray.getClass().isInstance(nullableArray));
		assertFalse(nonNullableArray.getClass().isInstance(nullableArray));
		assertTrue(nullableArray.getClass().isInstance(nonNullableArray));
	}

	/*
	 * Maintain a buffer of flattened arrays with long-aligned valuetypes while keeping a certain amount of classes alive at any
	 * single time. This forces the GC to unload the classes.
	 */
	@Test(priority=5, invocationCount=2)
	static public void testValueWithLongAlignmentGCScanning() throws Throwable {
		ArrayList<Object> longAlignmentArrayList = new ArrayList<Object>(objectGCScanningIterationCount);
		for (int i = 0; i < objectGCScanningIterationCount; i++) {
			Object newLongAlignmentArray = ValueClass.newNullRestrictedArray(assortedValueWithLongAlignmentClass, genericArraySize);
			for (int j = 0; j < genericArraySize; j++) {
				Object assortedValueWithLongAlignment = createAssorted(makeAssortedValueWithLongAlignment, typeWithLongAlignmentFields);
				Array.set(newLongAlignmentArray, j, assortedValueWithLongAlignment);
			}
			longAlignmentArrayList.add(newLongAlignmentArray);
		}

		System.gc();

		for (int i = 0; i < objectGCScanningIterationCount; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				checkFieldAccessMHOfAssortedType(assortedValueWithLongAlignmentGetterList, Array.get(longAlignmentArrayList.get(i), j), typeWithLongAlignmentFields, true);
			}
		}
	}

	/*
	 * Maintain a buffer of flattened arrays with object-aligned valuetypes while keeping a certain amount of classes alive at any
	 * single time. This forces the GC to unload the classes.
	 */
	@Test(priority=5, invocationCount=2)
	static public void testValueWithObjectAlignmentGCScanning() throws Throwable {
		ArrayList<Object> objectAlignmentArrayList = new ArrayList<Object>(objectGCScanningIterationCount);
		for (int i = 0; i < objectGCScanningIterationCount; i++) {
			Object newObjectAlignmentArray = ValueClass.newNullRestrictedArray(assortedValueWithObjectAlignmentClass, genericArraySize);
			for (int j = 0; j < genericArraySize; j++) {
				Object assortedValueWithObjectAlignment = createAssorted(makeAssortedValueWithObjectAlignment, typeWithObjectAlignmentFields);
				Array.set(newObjectAlignmentArray, j, assortedValueWithObjectAlignment);
			}
			objectAlignmentArrayList.add(newObjectAlignmentArray);
		}

		System.gc();

		for (int i = 0; i < objectGCScanningIterationCount; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				checkFieldAccessMHOfAssortedType(assortedValueWithObjectAlignmentGetterList, Array.get(objectAlignmentArrayList.get(i), j), typeWithObjectAlignmentFields, true);
			}
		}
	}

	/*
	 * Maintain a buffer of flattened arrays with single-aligned valuetypes while keeping a certain amount of classes alive at any
	 * single time. This forces the GC to unload the classes.
	 */
	@Test(priority=5, invocationCount=2)
	static public void testValueWithSingleAlignmentGCScanning() throws Throwable {
		ArrayList<Object> singleAlignmentArrayList = new ArrayList<Object>(objectGCScanningIterationCount);
		for (int i = 0; i < objectGCScanningIterationCount; i++) {
			Object newSingleAlignmentArray = ValueClass.newNullRestrictedArray(assortedValueWithSingleAlignmentClass, genericArraySize);
			for (int j = 0; j < genericArraySize; j++) {
				Object assortedValueWithSingleAlignment = createAssorted(makeAssortedValueWithSingleAlignment, typeWithSingleAlignmentFields);
				Array.set(newSingleAlignmentArray, j, assortedValueWithSingleAlignment);
			}
			singleAlignmentArrayList.add(newSingleAlignmentArray);
		}

		System.gc();

		for (int i = 0; i < objectGCScanningIterationCount; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				checkFieldAccessMHOfAssortedType(assortedValueWithSingleAlignmentGetterList, Array.get(singleAlignmentArrayList.get(i), j), typeWithSingleAlignmentFields, true);
			}
		}
	}

	@Test(priority=1)
	static public void testFlattenedFieldInitSequence() throws Throwable {
		String[] fields = {"x:I", "y:I"};
		Class<?> nestAClass = ValueTypeGenerator.generateValueClass("NestedA", fields);
		
		String[] fields2 = {"a:LNestedA;:NR", "b:LNestedA;:NR"};
		Class<?> nestBClass = ValueTypeGenerator.generateValueClass("NestedB", fields2);
		
		String[] fields3 = {"c:LNestedB;:NR", "d:LNestedB;:NR"};
		Class<?> containerCClass = ValueTypeGenerator.generateValueClass("ContainerC", fields3);
		
		MethodHandle defaultValueContainerC = lookup.findStatic(containerCClass, "makeDefaultValue", MethodType.methodType(containerCClass));
		
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
	
	/*
	 * Test initialization for a value type class that has not been resolved.
	 * The method is first called so that <init> will not be executed,
	 * and the class not resolved, and then called so that the <init>
	 * and class resolution is triggered.
	 */
	@Test(priority=1)
	static public void testUnresolvedDefaultValueUse() throws Throwable {
		/*
		 * Set up classes that look roughly like this:
		 *
		 * public inline class UnresolvedA {
		 *     public final int x;
		 *     public final int y;
		 *     public Object getGenericX(int x) {return Integer.valueOf(x);}
		 *     public Object getGenericY(int x) {return Integer.valueOf(y);}
		 * }
		 *
		 * public class UsingUnresolvedA {
		 *     public Object testUnresolvedValueTypeDefaultValue(int doDefaultValue) {
		 *         // Passing in non-zero triggers resolution of UnresolvedA class
		 *         return (doDefaultValue != 0) ? (UnresolvedA.<init>) : null;
		 *     }
		 * }
		 */
		String[] fields = {"x:I", "y:I"};
		Class<?> valueClass = ValueTypeGenerator.generateValueClass("UnresolvedA", fields);
		String[] fields2 = {};
		Class<?> usingClass = ValueTypeGenerator.generateRefClass("UsingUnresolvedA", fields2, "UnresolvedA");

		MethodHandle defaultValueUnresolved = lookup.findStatic(usingClass, "testUnresolvedValueTypeDefaultValue", MethodType.methodType(Object.class, int.class));

		for (int i = 0; i < 10; i++) {
			/*
			 * Pass zero to avoid initialization of value type class
			 */
			assertNull(defaultValueUnresolved.invoke(0));
		}

		MethodHandle getX = generateGenericGetter(valueClass, "x");
		MethodHandle getY = generateGenericGetter(valueClass, "y");

		for (int i = 0; i < 10; i++) {
			/*
			 * Pass one to force initialization of value type class
			 */
			Object defaultValue = defaultValueUnresolved.invoke(1);
			assertNotNull(defaultValue);
			assertEquals(getX.invoke(defaultValue), Integer.valueOf(0));
			assertEquals(getY.invoke(defaultValue), Integer.valueOf(0));
		}
	}

	private static class UnresolvedClassDesc {
		public String name;
		public String[] fields;
		public UnresolvedClassDesc(String name, String[] fields) {
			this.name = name;
			this.fields = fields;
		}
	}

	/*
	 * Test use of GETFIELD operations on the fields of a container class, where
	 * the fields are of value type classes that have not been resolved.
	 *
	 * The method is first called so that the GETFIELD will not be executed, and
	 * the class not resolved, and then called so that the GETFIELD and class
	 * resolution is triggered.
	 */
	@Test(priority=1)
	static public void testUnresolvedGetFieldUse() throws Throwable {
		/*
		 * Set up classes that look roughly like this:
		 *
		 * public inline class UnresolvedB1 {
		 *     public final int a;
		 *     public final int b;
		 * }
		 *
		 * public inline class UnresolvedB2 {
		 *     public final int c;
		 *     public final int d;
		 * }
		 *
		 * public inline class UnresolvedB3 {
		 *     public final int e;
		 *     public final int f;
		 * }
		 *
		 * public class ContainerForUnresolvedB {
		 *     public UnresolvedB1 v1;
		 *     public UnresolvedB2 v2;
		 *     public UnresolvedB3 v3;
		 * }
		 *
		 * public class UsingUnresolvedB {
		 *     public Object testUnresolvedValueTypeGetField(int fieldNum, ContainerForUnresolvedB container) {
		 *         // Passing in a value in the range [0..2] triggers execution of a GETFIELD
		 *         // operation on the corresponding field of "container", triggering
		 *         // resolution of the field.  Passing in a value outside that range, delays
		 *         // triggering resolution of the field
		 *         //
		 *         switch (fieldNum) {
		 *         case 0: return container.v1;
		 *         case 1: return container.v2;
		 *         case 2: return container.v3;
		 *         default: return null;
		 *         }
		 *     }
		 * }
		 */
		UnresolvedClassDesc[] uclassDescArr = new UnresolvedClassDesc[] {
								new UnresolvedClassDesc("UnresolvedB1", new String[] {"a:I", "b:I"}),
								new UnresolvedClassDesc("UnresolvedB2", new String[] {"c:I", "d:I"}),
								new UnresolvedClassDesc("UnresolvedB3", new String[] {"e:I", "f:I"})};

		Class<?>[] valueClassArr = new Class<?>[uclassDescArr.length];
		String[] containerFields = new String[uclassDescArr.length];
		MethodHandle[][] valueFieldGetters = new MethodHandle[uclassDescArr.length][];

		for (int i = 0; i < uclassDescArr.length; i++) {
			UnresolvedClassDesc desc = uclassDescArr[i];
			valueClassArr[i] = ValueTypeGenerator.generateValueClass(desc.name, desc.fields);
			valueFieldGetters[i] = new MethodHandle[desc.fields.length];
			containerFields[i] = "v"+(i+1)+":L"+desc.name+";:NR";

			for (int j = 0; j < desc.fields.length; j++) {
				String[] nameAndSig = desc.fields[j].split(":");
				valueFieldGetters[i][j] = generateGenericGetter(valueClassArr[i], nameAndSig[0]);
			}
		}

		Class<?> containerClass = ValueTypeGenerator.generateRefClass("ContainerForUnresolvedB", containerFields);

		String[] fieldsUsing = {};
		Class<?> usingClass = ValueTypeGenerator.generateRefClass("UsingUnresolvedB", fieldsUsing, "ContainerForUnresolvedB", containerFields);

		MethodHandle getFieldUnresolved = lookup.findStatic(usingClass, "testUnresolvedValueTypeGetField",
															MethodType.methodType(Object.class, new Class<?>[] {int.class, containerClass}));

		for (int i = 0; i < 10; i++) {
			/*
			 * Pass -1 to avoid execution of GETFIELD against field that has a value type class
			 * In turn that delays the resolution of the value type class
			 */
			assertNull(getFieldUnresolved.invoke(-1, null));
		}

		Object containerObject = containerClass.newInstance();
		for (int i = 0; i < uclassDescArr.length; i++) {
			/*
			 * Pass 0 or more to trigger execution of GETFIELD against field that has a value type class
			 * In turn that triggers the resolution of the associated value type classes
			 */
			Object fieldVal = getFieldUnresolved.invoke(i, containerObject);
			assertNotNull(fieldVal);

			for (int j = 0; j < valueFieldGetters[i].length; j++) {
				assertEquals(valueFieldGetters[i][j].invoke(fieldVal), Integer.valueOf(0));
			}
		}
	}

	/*
	 * Test use of PUTFIELD operations on the fields of a container class, where
	 * the fields are of value type classes that have not been resolved.
	 *
	 * The method is first called so that the PUTFIELD will not be executed, and
	 * the class not resolved, and then called so that the PUTFIELD and class
	 * resolution is triggered.
	 */
	@Test(priority=1)
	static public void testUnresolvedPutFieldUse() throws Throwable {
		/*
		 * Set up classes that look roughly like this:
		 *
		 * public inline class UnresolvedC1 {
		 *     public final int a;
		 *     public final int b;
		 * }
		 *
		 * public inline class UnresolvedC2 {
		 *     public final int c;
		 *     public final int d;
		 * }
		 *
		 * public inline class UnresolvedC3 {
		 *     public final int e;
		 *     public final int f;
		 * }
		 *
		 * public class ContainerForUnresolvedC {
		 *     public UnresolvedC1 v1;
		 *     public UnresolvedC2 v2;
		 *     public UnresolvedC3 v3;
		 * }
		 *
		 * public class UsingUnresolvedC {
		 *     public Object testUnresolvedValueTypePutField(int fieldNum, ContainerForUnresolvedC container, Object val) {
		 *         // Passing in a value in the range [0..2] triggers execution of a PUTFIELD
		 *         // operation on the corresponding field of "container", triggering
		 *         // resolution of the class and field.  Passing in a value outside that range,
		 *         // delays triggering that resolution
		 *         //
		 *         switch (fieldNum) {
		 *         case 0: container.v1 = (UnresolvedC3) val; break;
		 *         case 1: container.v2 = (UnresolvedC2) val; break;
		 *         case 2: container.v3 = (UnresolvedC3) val; break;
		 *         default: break;
		 *         }
		 *     }
		 * }
		 */
		UnresolvedClassDesc[] uclassDescArr = new UnresolvedClassDesc[] {
								new UnresolvedClassDesc("UnresolvedC1", new String[] {"a:I", "b:I"}),
								new UnresolvedClassDesc("UnresolvedC2", new String[] {"c:I", "d:I"}),
								new UnresolvedClassDesc("UnresolvedC3", new String[] {"e:I", "f:I"})};

		Class<?>[] valueClassArr = new Class<?>[uclassDescArr.length];
		String[] containerFields = new String[uclassDescArr.length];
		MethodHandle[][] valueFieldGetters = new MethodHandle[uclassDescArr.length][];
		MethodHandle[] containerFieldGetters = new MethodHandle[uclassDescArr.length];

		for (int i = 0; i < uclassDescArr.length; i++) {
			UnresolvedClassDesc desc = uclassDescArr[i];
			valueClassArr[i] = ValueTypeGenerator.generateValueClass(desc.name, desc.fields);
			valueFieldGetters[i] = new MethodHandle[desc.fields.length];
			containerFields[i] = "v"+(i+1)+":L"+desc.name+";:NR";

			for (int j = 0; j < desc.fields.length; j++) {
				String[] nameAndSig = desc.fields[j].split(":");
				valueFieldGetters[i][j] = generateGenericGetter(valueClassArr[i], nameAndSig[0]);
			}
		}

		Class<?> containerClass = ValueTypeGenerator.generateRefClass("ContainerForUnresolvedC", containerFields);

		for (int i = 0; i < uclassDescArr.length; i++) {
			String[] nameAndSig = containerFields[i].split(":");
			containerFieldGetters[i] = generateGenericGetter(containerClass, nameAndSig[0]);
		}

		String[] fieldsUsing = {};
		Class<?> usingClass = ValueTypeGenerator.generateRefClass("UsingUnresolvedC", fieldsUsing, "ContainerForUnresolvedC", containerFields);

		MethodHandle putFieldUnresolved = lookup.findStatic(usingClass, "testUnresolvedValueTypePutField",
												MethodType.methodType(void.class, new Class<?>[] {int.class, containerClass, Object.class}));

		for (int i = 0; i < 10; i++) {
			/*
			 * Pass -1 to avoid execution of PUTFIELD into field that has a value type class
			 * In turn that delays the resolution of the value type class
			 */
			putFieldUnresolved.invoke(-1, null, null);
		}

		Object containerObject = containerClass.newInstance();
		for (int i = 0; i < uclassDescArr.length; i++) {
			MethodHandle makeDefaultValueGeneric = lookup.findStatic(valueClassArr[i], "makeDefaultValueGeneric", MethodType.methodType(Object.class));
			Object valueObject = makeDefaultValueGeneric.invoke();
			/*
			 * Pass 0 or more to trigger execution of PUTFIELD against field that has a value type class
			 * In turn that triggers the resolution of the associated value type classes
			 */
			putFieldUnresolved.invoke(i, containerObject, valueObject);
		}

		for (int i = 0; i < containerFieldGetters.length; i++) {
			Object containerFieldValue = containerFieldGetters[i].invoke(containerObject);

			for (int j = 0; j < valueFieldGetters[i].length; j++) {
				assertEquals(valueFieldGetters[i][j].invoke(containerFieldValue), Integer.valueOf(0));
			}
		}
	}

	@Test(priority = 1)
	static public void testIdentityObjectOnValueType() throws Throwable {
		String[] fields = {"longField:J"};
		Class<?> valueClass = ValueTypeGenerator.generateValueClass("testIdentityObjectOnValueType", fields);
		assertTrue(valueClass.isValue());
		assertFalse(valueClass.isIdentity());
	}

	@Test(priority=1)
	static public void testIdentityObjectOnAbstractClass() throws Throwable {
		assertFalse(AbstractClass.class.isValue());
		assertTrue(AbstractClass.class.isIdentity());
	}
	public static abstract class AbstractClass {
		/* Abstract class that has fields */
		private int x;
		private int y;
		public int getX(){
			return x;
		}
		public int getY(){
			return y;
		}
	}
	
	public static abstract class AbstractClass1 {
		/* Abstract class that has non-empty constructors */
		public AbstractClass1() {
			System.out.println("This class is AbstractClass1");
		}
	}
	
	public static abstract class AbstractClass2 {
		/* Abstract class that has synchronized methods */
		public synchronized void printClassName() {
			System.out.println("This class is AbstractClass2");
		}
	}
	
	public static abstract class AbstractClass3 {
		/* Abstract class that has initializer */
		{ System.out.println("This class is AbstractClass3"); }
	}
	
	public static abstract class AbstractClass4 {
		
	}

	private static abstract class InvisibleAbstractClass {
		
	}
	
	static private void valueTypeIdentityObjectTestHelper(String testName, String superClassName, int extraClassFlags) throws Throwable {
		String[] fields = {"longField:J"};
		if (null == superClassName) {
			superClassName = "java/lang/Object";
		}
		Class<?> valueClass = ValueTypeGenerator.generateValueClass(testName, superClassName, fields, extraClassFlags);
		assertTrue(valueClass.isValue());
	}

	@Test(priority=1, expectedExceptions=IncompatibleClassChangeError.class)
	static public void testValueTypeSubClassAbstractClass() throws Throwable {
		String superClassName = AbstractClass.class.getName().replace('.', '/');
		valueTypeIdentityObjectTestHelper("testValueTypeSubClassAbstractClass", superClassName, 0);
	}
	
	@Test(priority=1, expectedExceptions=IncompatibleClassChangeError.class)
	static public void testValueTypeSubClassAbstractClass1() throws Throwable {
		String superClassName = AbstractClass1.class.getName().replace('.', '/');
		valueTypeIdentityObjectTestHelper("testValueTypeSubClassAbstractClass1", superClassName, 0);
	}
	
	@Test(priority=1, expectedExceptions=IncompatibleClassChangeError.class)
	static public void testValueTypeSubClassAbstractClass2() throws Throwable {
		String superClassName = AbstractClass2.class.getName().replace('.', '/');
		valueTypeIdentityObjectTestHelper("testValueTypeSubClassAbstractClass2", superClassName, 0);
	}
	
	@Test(priority=1, expectedExceptions=IncompatibleClassChangeError.class)
	static public void testValueTypeSubClassAbstractClass3() throws Throwable {
		String superClassName = AbstractClass3.class.getName().replace('.', '/');
		valueTypeIdentityObjectTestHelper("testValueTypeSubClassAbstractClass3", superClassName, 0);
	}

	@Test(priority=1, expectedExceptions=IncompatibleClassChangeError.class)
	static public void testValueTypeSubClassAbstractClass4() throws Throwable {
		String superClassName = AbstractClass4.class.getName().replace('.', '/');
		valueTypeIdentityObjectTestHelper("testValueTypeSubClassAbstractClass4", superClassName, 0);
	}

	@Test(priority=1, expectedExceptions=ClassFormatError.class)
	/* RI throws ClassFormatError for this case. */
	static public void testInterfaceValueType() throws Throwable {
		valueTypeIdentityObjectTestHelper("testInterfaceValueType", "java/lang/Object", ACC_INTERFACE | ACC_ABSTRACT);
	}

	@Test(priority=1, expectedExceptions=ClassFormatError.class)
	/* RI throws ClassFormatError for this case. */
	static public void testAbstractValueType() throws Throwable {
		valueTypeIdentityObjectTestHelper("testAbstractValueType", "java/lang/Object", ACC_ABSTRACT);
	}

	@Test(priority=1, expectedExceptions=IllegalAccessError.class)
	static public void testValueTypeSubClassInvisibleAbstractClass() throws Throwable {
		String superClassName = InvisibleAbstractClass.class.getName().replace('.', '/');
		valueTypeIdentityObjectTestHelper("testValueTypeSubClassInvisibleAbstractClass", superClassName, 0);
	}

	@Test(priority=1, expectedExceptions=ClassFormatError.class)
	static public void testValueTypeHasSynchMethods() throws Throwable {
		String[] fields = {"longField:J"};
		Class<?> valueClass = ValueTypeGenerator.generateIllegalValueClassWithSynchMethods("testValueTypeHasSynchMethods", fields);
	}

	@Test(priority = 1)
	static public void testIdentityObjectOnJLObject() throws Throwable {
		assertFalse(Object.class.isValue());
		assertTrue(Object.class.isIdentity());
	}

	@Test(priority = 1)
	static public void testIdentityObjectOnRef() throws Throwable {
		String[] fields = {"longField:J"};
		Class<?> refClass = ValueTypeGenerator.generateRefClass("testIdentityObjectOnRef", fields);
		assertFalse(refClass.isValue());
		assertTrue(refClass.isIdentity());
	}

	@Test(priority=1)
	static public void testIsValueClassOnInterface() throws Throwable {
		assertFalse(TestInterface.class.isValue());
		assertFalse(TestInterface.class.isIdentity());
	}

	private interface TestInterface {

	}

	@Test(priority=1)
	static public void testIsValueClassOnAbstractClass() throws Throwable {
		assertFalse(AbstractClass.class.isValue());
	}

	@Test(priority=1)
	static public void testIsValueOnValueArrayClass() throws Throwable {
		String[] fields = {"longField:J"};
		Class<?> valueClass = ValueTypeGenerator.generateValueClass("testIsPrimitiveOnValueArrayClass", fields);
		assertFalse(valueClass.arrayType().isValue());
		assertTrue(valueClass.arrayType().isIdentity());
	}

	@Test(priority=1)
	static public void testIsValueOnRefArrayClass() throws Throwable {
		String[] fields = {"longField:J"};
		Class<?> refClass = ValueTypeGenerator.generateRefClass("testIsPrimitiveOnRefArrayClass", fields);
		assertFalse(refClass.arrayType().isValue());
		assertTrue(refClass.arrayType().isIdentity());
	}

	@Test(priority=1)
	static public void testMethodTypeDescriptorRef() throws Throwable {
		String[] fields = {"longField:J"};
		Class<?> refClass = ValueTypeGenerator.generateRefClass("testMethodTypeDescriptorRef", fields);
		assertEquals(MethodType.methodType(refClass).toMethodDescriptorString(), "()LtestMethodTypeDescriptorRef;");
		assertEquals(refClass.descriptorString(), "LtestMethodTypeDescriptorRef;");
	}

	@Test(priority=1)
	static public void testMethodTypeDescriptorValue() throws Throwable {
		String[] fields = {"longField:J"};
		Class<?> valueClass = ValueTypeGenerator.generateValueClass("testMethodTypeDescriptorValue", fields);
		assertEquals(MethodType.methodType(valueClass).toMethodDescriptorString(), "()LtestMethodTypeDescriptorValue;");
		assertEquals(valueClass.descriptorString(), "LtestMethodTypeDescriptorValue;");
	}

	@Test(priority=1)
	static public void testValueClassHashCode() throws Throwable {
		Object p1 = new ValueClassPoint2D(new ValueClassInt(1), new ValueClassInt(2));
		Object p2 = new ValueClassPoint2D(new ValueClassInt(1), new ValueClassInt(2));
		Object p3 = new ValueClassPoint2D(new ValueClassInt(2), new ValueClassInt(2));
		Object p4 = new ValueClassPoint2D(new ValueClassInt(2), new ValueClassInt(1));
		Object p5 = new ValueClassPoint2D(new ValueClassInt(3), new ValueClassInt(4));

		int h1 = p1.hashCode();
		int h2 = p2.hashCode();
		int h3 = p3.hashCode();
		int h4 = p4.hashCode();
		int h5 = p5.hashCode();

		assertEquals(h1, h2);
		assertNotEquals(h1, h3);
		assertNotEquals(h1, h4);
		assertNotEquals(h1, h5);
		assertEquals(h1, System.identityHashCode(p1));
	}

	@Test(priority = 1, expectedExceptions = ClassFormatError.class,
		expectedExceptionsMessageRegExp = ".*A non-interface class must have at least one of ACC_FINAL, ACC_IDENTITY, or ACC_ABSTRACT flags set.*")
	static public void testNonInterfaceClassMustHaveFinalIdentityAbstractSet() throws Throwable {
		ValueTypeGenerator.generateNonInterfaceClassWithMissingFlags("testNonInterfaceClassMustHaveFinalIdentityAbstractSet");
	}

	/* A field must not have set both ACC_FINAL and ACC_VOLATILE */
	@Test(expectedExceptions = java.lang.ClassFormatError.class, expectedExceptionsMessageRegExp = ".*field cannot be both final and volatile.*")
	static public void testFieldCantHaveAccFinalAndAccVolatile() {
		ValueTypeGenerator.generateTestFieldCantHaveAccFinalAndAccVolatile();
	}

	/* A field must not have set both ACC_STRICT and ACC_STATIC */
	@Test(expectedExceptions = java.lang.ClassFormatError.class, expectedExceptionsMessageRegExp = ".*A field must not have both ACC_STRICT and ACC_STATIC flags set.*")
	static public void testFieldCantHaveAccStrictAndAccStatic() {
		ValueTypeGenerator.generateTestFieldCantHaveAccStrictAndAccStatic();
	}

	/* A field that has set ACC_STRICT must also have set ACC_FINAL */
	@Test(expectedExceptions = java.lang.ClassFormatError.class, expectedExceptionsMessageRegExp = ".*A field with ACC_STRICT set must also set ACC_FINAL.*")
	static public void testAccStrictFieldMustHaveAccFinal() {
		ValueTypeGenerator.generateTestAccStrictFieldMustHaveAccFinal();
	}

	/* Each field of a value class must have exactly one of its ACC_STATIC or ACC_STRICT flags set */
	@Test(expectedExceptions = java.lang.ClassFormatError.class, expectedExceptionsMessageRegExp = ".*Value class fields must have either ACC_STATIC or ACC_STRICT set.*")
	static public void testValueClassFieldMustHaveAccStaticOrAccStrict() {
		ValueTypeGenerator.generateTestValueClassFieldMustHaveAccStaticOrAccStrict();
	}

	/* putfield is not allowed outside <init> for ACC_STRICT fields */
	@Test(expectedExceptions = VerifyError.class)
	static public void testValueClassWithIllegalSetters() throws Throwable {
		String[] fields = {"x:I"};
		Class<?> valueClass = ValueTypeGenerator.generateValueClassWithIllegalSetters("TestValueClassWithIllegalSetters", fields);
		valueClass.newInstance();
	}

	/* putfield not allowed in <init> after class initialization for ACC_STRICT fields */
	@Test(expectedExceptions = VerifyError.class)
	static public void testValueClassPutStrictFieldAfterInitialization() throws Throwable {
		Class<?> valueClass = ValueTypeGenerator.generateTestValueClassPutStrictFieldAfterInitialization();
		valueClass.newInstance();
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

	static MethodHandle generateSetter(Class<?> clazz, String fieldName, Class<?> fieldType) {
		try {
			return lookup.findVirtual(clazz, "set"+fieldName, MethodType.methodType(void.class, fieldType));
		} catch (IllegalAccessException | SecurityException | NullPointerException | NoSuchMethodException e) {
			e.printStackTrace();
		}
		return null;
	}
	
	static MethodHandle generateGenericSetter(Class<?> clazz, String fieldName) {
		try {
			return lookup.findVirtual(clazz, "setGeneric"+fieldName, MethodType.methodType(void.class, Object.class));
		} catch (IllegalAccessException | SecurityException | NullPointerException | NoSuchMethodException e) {
			e.printStackTrace();
		}
		return null;
	}

	static MethodHandle generateStaticGenericSetter(Class<?> clazz, String fieldName) {
		try {
			return lookup.findStatic(clazz, "setStaticGeneric"+fieldName, MethodType.methodType(void.class, Object.class));
		} catch (IllegalAccessException | SecurityException | NullPointerException | NoSuchMethodException e) {
			e.printStackTrace();
		}
		return null;
	}

	static MethodHandle[][] generateGenericGetterList(Class<?> clazz, String[] fields) {
		MethodHandle[][] getterList = new MethodHandle[fields.length][2];
		for (int i = 0; i < fields.length; i++) {
			String field = (fields[i].split(":"))[0];
			getterList[i][0] = generateGenericGetter(clazz, field);
		}
		return getterList;
	}

	static MethodHandle[][] generateGenericGetterAndSetter(Class<?> clazz, String[] fields) {
		MethodHandle[][] getterAndSetter = new MethodHandle[fields.length][2];
		for (int i = 0; i < fields.length; i++) {
			String field = (fields[i].split(":"))[0];
			getterAndSetter[i][0] = generateGenericGetter(clazz, field);
			getterAndSetter[i][1] = generateGenericSetter(clazz, field);
		}
		return getterAndSetter;
	}

	static MethodHandle[][] generateStaticGenericGetterAndSetter(Class<?> clazz, String[] fields) {
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
			String[] nameAndSigValue = fields[i].split(":");
			String signature = nameAndSigValue[1];
			switch (signature) {
			case "LPoint2D;":
				args[i] = createPoint2D(useInitFields ? (int[])initFields[i] : defaultPointPositions1);
				break;
			case "LFlattenedLine2D;":
				args[i] = createFlattenedLine2D(useInitFields ? (int[][])initFields[i] : defaultLinePositions1);
				break;
			case "LTriangle2D;":
				args[i] = createTriangle2D(useInitFields ? (int[][][])initFields[i] : defaultTrianglePositions);
				break;
			case "LValueInt;":
				args[i] = makeValueInt.invoke(useInitFields ? (int)initFields[i] : defaultInt);
				break;
			case "LValueFloat;":
				args[i] = makeValueFloat.invoke(useInitFields ? (float)initFields[i] : defaultFloat);
				break;
			case "LValueDouble;":
				args[i] = makeValueDouble.invoke(useInitFields ? (double)initFields[i] : defaultDouble);
				break;
			case "LValueObject;":
				args[i] = makeValueObject.invoke(useInitFields ? (Object)initFields[i] : defaultObject);
				break;
			case "LValueLong;":
				args[i] = makeValueLong.invoke(useInitFields ? (long)initFields[i] : defaultLong);
				break;
			case "LLargeObject;":
				args[i] = createLargeObject(useInitFields ? (Object)initFields[i] : defaultObject);
				break;
			default:
				args[i] = null;
				break;
			}
		}
		return makeMethod.invokeWithArguments(args);
	}

	static void initializeStaticFields(Class<?> clazz, MethodHandle[][] getterAndSetter, String[] fields) throws Throwable {
		Object defaultValue = null;
		for (int i = 0; i < fields.length; i++) {
			String signature = (fields[i].split(":"))[1];
			switch (signature) {
			case "LPoint2D;":
				defaultValue = createPoint2D(defaultPointPositions1);
				break;
			case "LFlattenedLine2D;":
				defaultValue = createFlattenedLine2D(defaultLinePositions1);
				break;
			case "LTriangle2D;":
				defaultValue = createTriangle2D(defaultTrianglePositions);
				break;
			case "LValueInt;":
				defaultValue = makeValueInt.invoke(defaultInt);
				break;
			case "LValueFloat;":
				defaultValue = makeValueFloat.invoke(defaultFloat);
				break;
			case "LValueDouble;":
				defaultValue = makeValueDouble.invoke(defaultDouble);
				break;
			case "LValueObject;":
				defaultValue = makeValueObject.invoke(defaultObject);
				break;
			case "LValueLong;":
				defaultValue = makeValueLong.invoke(defaultLong);
				break;
			case "LLargeObject;":
				defaultValue = createLargeObject(defaultObject);
				break;
			default:
				defaultValue = null;
				break;
			}
			getterAndSetter[i][1].invoke(defaultValue);
		}
	}

	/*
	 * For identity classes fieldAccessMHs contains getters in [0] column
	 * and setters in [1] column.
	 * For value classes (isValue is true) fieldAccessMHs has getters in [0] column
	 * and nothing in the [1] column. Value class instance fields cannot be overwritten.
	 */
	static Object checkFieldAccessMHOfAssortedType(MethodHandle[][] fieldAccessMHs, Object instance, String[] fields,
			boolean isValue)
			throws Throwable {
		for (int i = 0; i < fields.length; i++) {
			String[] nameAndSigValue = fields[i].split(":");
			String signature = nameAndSigValue[1];
			switch (signature) {
			case "LPoint2D;":
				checkEqualPoint2D(fieldAccessMHs[i][0].invoke(instance), defaultPointPositions1);
				Object pointNew = createPoint2D(defaultPointPositionsNew);
				if (!isValue) {
					fieldAccessMHs[i][1].invoke(instance, pointNew);
					checkEqualPoint2D(fieldAccessMHs[i][0].invoke(instance), defaultPointPositionsNew);
				}
				break;
			case "LFlattenedLine2D;":
				checkEqualFlattenedLine2D(fieldAccessMHs[i][0].invoke(instance), defaultLinePositions1);
				Object lineNew = createFlattenedLine2D(defaultLinePositionsNew);
				if (!isValue) {
					fieldAccessMHs[i][1].invoke(instance, lineNew);
					checkEqualFlattenedLine2D(fieldAccessMHs[i][0].invoke(instance), defaultLinePositionsNew);
				}
				break;
			case "LTriangle2D;":
				checkEqualTriangle2D(fieldAccessMHs[i][0].invoke(instance), defaultTrianglePositions);
				Object triNew = createTriangle2D(defaultTrianglePositionsNew);
				if (!isValue) {
					fieldAccessMHs[i][1].invoke(instance, triNew);
					checkEqualTriangle2D(fieldAccessMHs[i][0].invoke(instance), defaultTrianglePositionsNew);
				}
				break;
			case "LValueInt;":
				assertEquals(getInt.invoke(fieldAccessMHs[i][0].invoke(instance)), defaultInt);
				Object iNew = makeValueInt.invoke(defaultIntNew);
				if (!isValue) {
					fieldAccessMHs[i][1].invoke(instance, iNew);
					assertEquals(getInt.invoke(fieldAccessMHs[i][0].invoke(instance)), defaultIntNew);
				}
					break;
			case "LValueFloat;":
				assertEquals(getFloat.invoke(fieldAccessMHs[i][0].invoke(instance)), defaultFloat);
				Object fNew = makeValueFloat.invoke(defaultFloatNew);
				if (!isValue) {
					fieldAccessMHs[i][1].invoke(instance, fNew);
					assertEquals(getFloat.invoke(fieldAccessMHs[i][0].invoke(instance)), defaultFloatNew);
				}
				break;
			case "LValueDouble;":
				assertEquals(getDouble.invoke(fieldAccessMHs[i][0].invoke(instance)), defaultDouble);
				Object dNew = makeValueDouble.invoke(defaultDoubleNew);
				if (!isValue) {
					fieldAccessMHs[i][1].invoke(instance, dNew);
					assertEquals(getDouble.invoke(fieldAccessMHs[i][0].invoke(instance)), defaultDoubleNew);
				}
				break;
			case "LValueObject;":
				assertEquals(getObject.invoke(fieldAccessMHs[i][0].invoke(instance)), defaultObject);
				Object oNew = makeValueObject.invoke(defaultObjectNew);
				if (!isValue) {
					fieldAccessMHs[i][1].invoke(instance, oNew);
					assertEquals(getObject.invoke(fieldAccessMHs[i][0].invoke(instance)), defaultObjectNew);
				}
				break;
			case "LValueLong;":
				assertEquals(getLong.invoke(fieldAccessMHs[i][0].invoke(instance)), defaultLong);
				Object lNew = makeValueLong.invoke(defaultLongNew);
				if (!isValue) {
					fieldAccessMHs[i][1].invoke(instance, lNew);
					assertEquals(getLong.invoke(fieldAccessMHs[i][0].invoke(instance)), defaultLongNew);
				}
				break;
			case "LLargeObject;":
				checkEqualLargeObject(fieldAccessMHs[i][0].invoke(instance), defaultObject);
				Object largeNew = createLargeObject(defaultObjectNew);
				if (!isValue) {
					fieldAccessMHs[i][1].invoke(instance, largeNew);
					checkEqualLargeObject(fieldAccessMHs[i][0].invoke(instance), defaultObjectNew);
				}
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
			String[] nameAndSigValue = fields[i].split(":");
			String signature = nameAndSigValue[1];
			switch (signature) {
			case "LPoint2D;":
				checkEqualPoint2D(fieldAccessMHs[i][0].invoke(), defaultPointPositions1);
				Object pointNew = createPoint2D(defaultPointPositionsNew);
				fieldAccessMHs[i][1].invoke(pointNew);
				checkEqualPoint2D(fieldAccessMHs[i][0].invoke(), defaultPointPositionsNew);
				break;
			case "LFlattenedLine2D;":
				checkEqualFlattenedLine2D(fieldAccessMHs[i][0].invoke(), defaultLinePositions1);
				Object lineNew = createFlattenedLine2D(defaultLinePositionsNew);
				fieldAccessMHs[i][1].invoke(lineNew);
				checkEqualFlattenedLine2D(fieldAccessMHs[i][0].invoke(), defaultLinePositionsNew);
				break;
			case "LTriangle2D;":
				checkEqualTriangle2D(fieldAccessMHs[i][0].invoke(), defaultTrianglePositions);
				Object triNew = createTriangle2D(defaultTrianglePositionsNew);
				fieldAccessMHs[i][1].invoke(triNew);
				checkEqualTriangle2D(fieldAccessMHs[i][0].invoke(), defaultTrianglePositionsNew);
				break;
			case "LValueInt;":
				assertEquals(getInt.invoke(fieldAccessMHs[i][0].invoke()), defaultInt);
				Object iNew = makeValueInt.invoke(defaultIntNew);
				fieldAccessMHs[i][1].invoke(iNew);
				assertEquals(getInt.invoke(fieldAccessMHs[i][0].invoke()), defaultIntNew);
				break;
			case "LValueFloat;":
				assertEquals(getFloat.invoke(fieldAccessMHs[i][0].invoke()), defaultFloat);
				Object fNew = makeValueFloat.invoke(defaultFloatNew);
				fieldAccessMHs[i][1].invoke(fNew);
				assertEquals(getFloat.invoke(fieldAccessMHs[i][0].invoke()), defaultFloatNew);
				break;
			case "LValueDouble;":
				assertEquals(getDouble.invoke(fieldAccessMHs[i][0].invoke()), defaultDouble);
				Object dNew = makeValueDouble.invoke(defaultDoubleNew);
				fieldAccessMHs[i][1].invoke(dNew);
				assertEquals(getDouble.invoke(fieldAccessMHs[i][0].invoke()), defaultDoubleNew);
				break;
			case "LValueObject;":
				assertEquals(getObject.invoke(fieldAccessMHs[i][0].invoke()), defaultObject);
				Object oNew = makeValueObject.invoke(defaultObjectNew);
				fieldAccessMHs[i][1].invoke(oNew);
				assertEquals(getObject.invoke(fieldAccessMHs[i][0].invoke()), defaultObjectNew);
				break;
			case "LValueLong;":
				assertEquals(getLong.invoke(fieldAccessMHs[i][0].invoke()), defaultLong);
				Object lNew = makeValueLong.invoke(defaultLongNew);
				fieldAccessMHs[i][1].invoke(lNew);
				assertEquals(getLong.invoke(fieldAccessMHs[i][0].invoke()), defaultLongNew);
				break;
			case "LLargeObject;":
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
