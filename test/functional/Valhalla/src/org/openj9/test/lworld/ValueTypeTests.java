/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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
import java.lang.reflect.Field;
import sun.misc.Unsafe;

import org.testng.Assert;
import static org.testng.Assert.*;
import org.testng.annotations.Test;

/*
 * Instructions to run this test:
 * 
 * 1) recompile the JVM with J9VM_OPT_VALHALLA_VALUE_TYPES flag turned on in j9cfg.h.ftl (or j9cfg.h.in when cmake is turned on)
 * 2) cd [openj9-openjdk-dir]/openj9/test/TestConfig
 * 3) export JAVA_BIN=[openj9-openjdk-dir]/build/linux-x86_64-normal-server-release/images/jdk/bin
 * 4) export PATH=$JAVA_BIN:$PATH
 * 5) export JDK_VERSION=Valhalla
 * 6) export SPEC=linux_x86-64_cmprssptrs
 * 7) export BUILD_LIST=functional/Valhalla
 * 8) export AUTO_DETECT=false
 * 9) export JDK_IMPL=openj9
 * 10) make -f run_configure.mk && make compile && make _sanity
 */

@Test(groups = { "level.sanity" })
public class ValueTypeTests {
	//order by class
	static Lookup lookup = MethodHandles.lookup();
	static Unsafe myUnsafe = createUnsafeInstance();
	//point2DClass: make sure point2DClass is not null, getX is updated
	static Class point2DClass = null;
	static MethodHandle makePoint2D = null;	
	static MethodHandle getX = null;
	static MethodHandle getY = null;
	//line2DClass
	static Class line2DClass = null;
	static MethodHandle makeLine2D = null;
	static MethodHandle getSt = null;
	static MethodHandle getEn = null;
	//flattenLine2DClass
	static Class flattenedLine2DClass = null;
	static MethodHandle makeFlattenedLine2D = null;
	static MethodHandle getStFlat = null;
	static MethodHandle getEnFlat = null;
	//triangleClass
	static Class triangle2DClass = null;
	static MethodHandle makeTriangle2D = null;
	static MethodHandle getV1 = null;
	static MethodHandle getV2 = null;
	static MethodHandle getV3 = null;
	//valueLongClass
	static Class valueLongClass = null;
	static MethodHandle makeValueLong = null;
	static MethodHandle getLong = null;
	static MethodHandle withLong = null;
	//valueIntClass
	static Class valueIntClass = null;
	static MethodHandle makeValueInt = null;
	static MethodHandle getInt = null;
	static MethodHandle withInt = null;
	//valueDoubleClass
	static Class valueDoubleClass = null;
	static MethodHandle makeValueDouble = null;
	static MethodHandle getDouble = null;
	static MethodHandle withDouble = null;
	//valueFloatClass
	static Class valueFloatClass =null;
	static MethodHandle makeValueFloat = null;
	static MethodHandle getFloat = null;
	static MethodHandle withFloat = null;
	//valueObejctClass
	static Class valueObjectClass = null;
	static MethodHandle makeValueObject = null;
	static MethodHandle getObject = null;
	static MethodHandle withObject = null;
	//largeObjectValueClass
	static Class largeObjectValueClass = null;
	static MethodHandle makeLargeValue = null;
	

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
		MethodHandle withX = generateWither(point2DClass, "x", int.class);
		getY = generateGetter(point2DClass, "y", int.class);
		MethodHandle withY = generateWither(point2DClass, "y", int.class);

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
		
		assertEquals(getFieldOffset(point2DClass, "x"), 4);
		assertEquals(myUnsafe.getInt(point2D, 4L), xNew);
		assertEquals(getFieldOffset(point2DClass, "y"), 8);
		assertEquals(myUnsafe.getInt(point2D, 8L), yNew);
	}

	/*
	 * Create a value type with double slot primative members
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
		

		assertEquals(getFieldOffset(point2DComplexClass, "d"), 8);
		assertEquals(myUnsafe.getDouble(point2D, 8L), d);
		assertEquals(getFieldOffset(point2DComplexClass, "j"), 16);
		assertEquals(myUnsafe.getLong(point2D, 16L), j);
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
		
		getSt = generateGetter(line2DClass, "st", point2DClass);
 		MethodHandle withSt = generateWither(line2DClass, "st", point2DClass);
 		getEn = generateGetter(line2DClass, "en", point2DClass);
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
		
		assertEquals(getFieldOffset(line2DClass, "st"), 4);
		assertEquals(myUnsafe.getObject(line2D, 4L), stNew);
		assertEquals(getFieldOffset(line2DClass, "en"), 8);
		assertEquals(myUnsafe.getObject(line2D, 8L), enNew);
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
		
		getStFlat = generateGenericGetter(flattenedLine2DClass, "st");
 		MethodHandle withSt = generateGenericWither(flattenedLine2DClass, "st");
 		getEnFlat = generateGenericGetter(flattenedLine2DClass, "en");
 		MethodHandle withEn = generateGenericWither(flattenedLine2DClass, "en");
 		
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
		
		assertEquals(getX.invoke(getStFlat.invoke(line2D)), x);
		assertEquals(getY.invoke(getStFlat.invoke(line2D)), y);
		assertEquals(getX.invoke(getEnFlat.invoke(line2D)), x2);
		assertEquals(getY.invoke(getEnFlat.invoke(line2D)), y2);
		
		Object stNew = makePoint2D.invoke(xNew, yNew);
		Object enNew = makePoint2D.invoke(x2New, y2New);
		
		line2D = withSt.invoke(line2D, stNew);
		line2D = withEn.invoke(line2D, enNew);
		
		assertEquals(getX.invoke(getStFlat.invoke(line2D)), xNew);
		assertEquals(getY.invoke(getStFlat.invoke(line2D)), yNew);
		assertEquals(getX.invoke(getEnFlat.invoke(line2D)), x2New);
		assertEquals(getY.invoke(getEnFlat.invoke(line2D)), y2New);
		
		assertEquals(getFieldOffset(flattenedLine2DClass, "st"), 4);
		assertEquals(myUnsafe.getInt(line2D, 4L), xNew);
		assertEquals(myUnsafe.getInt(line2D, 8L), yNew);
		assertEquals(getFieldOffset(flattenedLine2DClass, "en"), 12);
		assertEquals(myUnsafe.getInt(line2D, 12L), x2New);
		assertEquals(myUnsafe.getInt(line2D, 16L), y2New);
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

	/*
	 * TODO: behaviour of the test between two valueTypes will depend on the new spec(not finialized)
	 * 
	 * Test ifacmp on value class
	 * 
	 * class TestIfacmpOnValueClass {}
	 *
	 *
	 *	@Test(priority=2)
	 *	static public void TestIfacmpOnValueClass() throws Throwable {
	 *	int x = 0;
	 *	int y = 0;
	 *
	 *	Object valueType = makePoint2D.invoke(x, y);
	 *	Object refType = (Object) x;
	 *
	 *	Assert.assertFalse((valueType == refType), "An identity (==) comparison that contains a valueType should always return false");
	 *
	 *	Assert.assertFalse((refType == valueType), "An identity (==) comparison that contains a valueType should always return false");
	 *
	 *	Assert.assertFalse((valueType == valueType), "An identity (==) comparison that contains a valueType should always return false");
	 *
	 *	Assert.assertTrue((refType == refType), "An identity (==) comparison on the same refType should always return true");
	 *
	 *	Assert.assertTrue((valueType != refType), "An identity (!=) comparison that contains a valueType should always return true");
	 *
	 *	Assert.assertTrue((refType != valueType), "An identity (!=) comparison that contains a valueType should always return true");
	 *
	 *	Assert.assertTrue((valueType != valueType), "An identity (!=) comparison that contains a valueType should always return true");
	 *
	 *	Assert.assertFalse((refType != refType), "An identity (!=) comparison on the same refType should always return false");
	 *	}
	 */	
	
	/*	
	 * Create a valueType with three valueType members
	 * 
	 * value Triangle2D {
	 *  flattened Line2D v1;
	 *  flattened Line2D v2;
	 *  flattened Line2D v3;
	 * }
	 */
	@Test(priority = 3)
	static public void testCreateTriangle2D() throws Throwable {
		String fields[] = { "v1:QFlattenedLine2D;:value", "v2:QFlattenedLine2D;:value", "v3:QFlattenedLine2D;:value" };
		triangle2DClass = ValueTypeGenerator.generateValueClass("Triangle2D", fields);

		makeTriangle2D = lookup.findStatic(triangle2DClass, "makeValueGeneric",
				MethodType.methodType(triangle2DClass, Object.class, Object.class, Object.class));

		getV1 = generateGenericGetter(triangle2DClass, "v1");
		MethodHandle withV1 = generateGenericWither(triangle2DClass, "v1");
		getV2 = generateGenericGetter(triangle2DClass, "v2");
		MethodHandle withV2 = generateGenericWither(triangle2DClass, "v2");
		getV3 = generateGenericGetter(triangle2DClass, "v3");
		MethodHandle withV3 = generateGenericWither(triangle2DClass, "v3");

		int xSt = 0xFFEEFFEE;
		int ySt = 0xAABBAABB;
		int xEn = 0xEEFFEEFF;
		int yEn = 0xBBAABBAA;
		int x2St = 0xCCDDCCDD;
		int y2St = 0xAAFFAAFF;
		int x2En = 0xDDCCDDCC;
		int y2En = 0xFFAAFFAA;
		int x3St = 0xBBAABBDD;
		int y3St = 0xEECCEECC;
		int x3En = 0xCCEECCEE;
		int y3En = 0xDDAADDAA;

		int xStNew = 0xFFEEAABB;
		int yStNew = 0xAABBCCDD;
		int xEnNew = 0xEEFFAABB;
		int yEnNew = 0xBBAADDCC;
		int x2StNew = 0xCCDDAABB;
		int y2StNew = 0xAAFFEEEE;
		int x2EnNew = 0xDDCCDDEE;
		int y2EnNew = 0xFFAAAACC;
		int x3StNew = 0xBBAABBEE;
		int y3StNew = 0xEECCEEAA;
		int x3EnNew = 0xCCEECCDD;
		int y3EnNew = 0xDDAADDCC;

		Object v1 = makeFlattenedLine2D.invoke(makePoint2D.invoke(xSt, ySt), makePoint2D.invoke(xEn, yEn));
		Object v2 = makeFlattenedLine2D.invoke(makePoint2D.invoke(x2St, y2St), makePoint2D.invoke(x2En, y2En));
		Object v3 = makeFlattenedLine2D.invoke(makePoint2D.invoke(x3St, y3St), makePoint2D.invoke(x3En, y3En));

		int[][] v1Positions = { { xSt, ySt }, { xEn, yEn } };
		int[][] v2Positions = { { x2St, y2St }, { x2En, y2En } };
		int[][] v3Positions = { { x3St, y3St }, { x3En, y3En } };
		int[][][] positions = { v1Positions, v2Positions, v3Positions };

		checkEqualLine2D(v1, v1Positions, true);
		checkEqualLine2D(v2, v2Positions, true);
		checkEqualLine2D(v3, v3Positions, true);

		Object triangle2D = makeTriangle2D.invoke(v1, v2, v3);

		checkEqualTriangle2D(triangle2D, positions);

		Object v1New = makeFlattenedLine2D.invoke(makePoint2D.invoke(xStNew, yStNew),
				makePoint2D.invoke(xEnNew, yEnNew));
		Object v2New = makeFlattenedLine2D.invoke(makePoint2D.invoke(x2StNew, y2StNew),
				makePoint2D.invoke(x2EnNew, y2EnNew));
		Object v3New = makeFlattenedLine2D.invoke(makePoint2D.invoke(x3StNew, y3StNew),
				makePoint2D.invoke(x3EnNew, y3EnNew));

		triangle2D = withV1.invoke(triangle2D, v1New);
		triangle2D = withV2.invoke(triangle2D, v2New);
		triangle2D = withV3.invoke(triangle2D, v3New);

		int[][] v1PositionsNew = { { xStNew, yStNew }, { xEnNew, yEnNew } };
		int[][] v2PositionsNew = { { x2StNew, y2StNew }, { x2EnNew, y2EnNew } };
		int[][] v3PositionsNew = { { x3StNew, y3StNew }, { x3EnNew, y3EnNew } };
		int[][][] positionsNew = { v1PositionsNew, v2PositionsNew, v3PositionsNew };

		checkEqualTriangle2D(triangle2D, positionsNew);

		assertEquals(getFieldOffset(triangle2DClass, "v1"), 4);
		assertEquals(getFieldOffset(triangle2DClass, "v2"), 20);
		assertEquals(getFieldOffset(triangle2DClass, "v3"), 36);

		long offset = 4L;
		for (int i = 0; i < positionsNew.length; i++) {
			int[][] currentLine = positionsNew[i];
			for (int j = 0; j < currentLine.length; j++) {
				int[] currentPoint = currentLine[j];
				for (int k = 0; k < currentPoint.length; k++) {
					assertEquals(myUnsafe.getInt(triangle2D, offset), currentPoint[k]);
					offset = 4 + offset;
				}
			}
		}
	}

	/*
	 * Create a value type with a long primitive member
	 * 
	 * value ValueLong {
	 * 	long j;
	 * }
	 */
	@Test(priority = 1)
	static public void testCreateValueLong() throws Throwable {
		String fields[] = { "j:J" };
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

		assertEquals(getFieldOffset(valueLongClass, "j"), 8);
		assertEquals(myUnsafe.getLong(valueLong, 8L), jNew);
	}

	/*
	 * Create a value type with a int primitive member
	 * 
	 * value valueInt {
	 * 	int i;
	 * }
	 */
	@Test(priority = 1)
	static public void testCreatevalueInt() throws Throwable {
		String fields[] = { "i:I" };
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

		assertEquals(getFieldOffset(valueIntClass, "i"), 4);
		assertEquals(myUnsafe.getInt(valueInt, 4L), iNew);
	}

	/*
	 * Create a value type with a double primitive member
	 * 
	 * value ValueDouble {
	 * 	double d;
	 * }
	 */
	@Test(priority = 1)
	static public void testCreateValueDouble() throws Throwable {
		String fields[] = { "d:D" };
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

		assertEquals(getFieldOffset(valueDoubleClass, "d"), 8);
		assertEquals(myUnsafe.getDouble(valueDouble, 8L), dNew);
	}

	/*
	 * Create a value type with a float primitive member
	 * 
	 * value ValueFloat {
	 * 	float f;
	 * }
	 */
	@Test(priority = 1)
	static public void testCreateValueFloat() throws Throwable {
		String fields[] = { "f:F" };
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

		assertEquals(getFieldOffset(valueFloatClass, "f"), 4);
		assertEquals(myUnsafe.getFloat(valueFloat, 4L), fNew);
	}

	/*
	 * Create a value type with a reference member
	 * 
	 * value ValueObject {
	 * 	Object val;
	 * }
	 */
	@Test(priority = 1)
	static public void testCreateValueObject() throws Throwable {
		String fields[] = { "val:Ljava/lang/Object;:value" };

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

		assertEquals(getFieldOffset(valueObjectClass, "val"), 4);
		assertEquals(myUnsafe.getObject(valueObject, 4L), valNew);
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
		String fields[] = {"point:QPoint2D;:value", "line:QFlattenedLine2D;:value", "o:QValueObject;:value", "l:QValueLong;:value",
				"d:QValueDouble;:value", "i:QValueInt;:value", "tri:QTriangle2D;:value"};
		Class assortedValueClass = ValueTypeGenerator.generateValueClass("AssortedValueWithLongAlignment", fields);

		MethodHandle makeAssorted = lookup.findStatic(assortedValueClass, "makeValueGeneric", MethodType.methodType(assortedValueClass, Object.class, Object.class, Object.class, Object.class, Object.class, Object.class, Object.class));
		
		/*
		 * Getters are created in array getterAndWither[i][0] according to the order of fields i
		 * Withers are created in array getterAndWither[i][1] according to the order of fields i
		 */
		MethodHandle[][] getterAndWither = new MethodHandle[fields.length][2];
		for(int i = 0; i < fields.length; i++) {
			String field = (fields[i].split(":"))[0];
			getterAndWither[i][0] = generateGenericGetter(assortedValueClass, field);
			getterAndWither[i][1] = generateGenericWither(assortedValueClass, field);
		}
		
		/*
		 * create field objects
		 */
		
		/*
		 * point
		 */
		int x = 0xFFEEFFEE;
		int y = 0xAABBAABB;
		/*
		 * line
		 */
		int stX = 0xFFEEFFEE;
		int stY = 0xAABBAABB;
		int enX = 0xCCAFFAAA;
		int enY = 0xBBAAAACC;
		/*
		 * triangle
		 */
		int v1StX = 0xAADDAADD;
		int v1StY = 0xAACCAACC;
		int v1EnX = 0xEEAAEEEE;
		int v1EnY = 0xAAAAAAAA;
		int v2StX = 0xBBBCCCAA;
		int v2StY = 0xEEAAEEEA;
		int v2EnX = 0xFFFFFAAA;
		int v2EnY = 0xAAAAAAAC;		
		int v3StX = 0xCCCCCCCA;
		int v3StY = 0xEEEEEEEC;
		int v3EnX = 0xAAAAADDD;
		int v3EnY = 0xDDDDBBBB;		
		/*
		 * o
		 */
		Object oValue = (Object) 0xEEFAEEFF;
		/*
		 * l
		 */
		long lValue = 2121212121L;
		/*
		 * d
		 */
		double dValue = 123.43555;
		/*
		 * i
		 */
		int iValue = 0xABCDDCBA;
		
		Object point = makePoint2D.invoke(x, y);
		Object line = makeFlattenedLine2D.invoke(makePoint2D.invoke(stX, stY), makePoint2D.invoke(enX, enY));
		Object o = makeValueObject.invoke(oValue);
		Object l = makeValueLong.invoke(lValue);
		Object d = makeValueDouble.invoke(dValue);
		Object i = makeValueInt.invoke(iValue);
		Object tri = makeTriangle2D.invoke(makeFlattenedLine2D.invoke(makePoint2D.invoke(v1StX, v1StY), makePoint2D.invoke(v1EnX, v1EnY)), 
				makeFlattenedLine2D.invoke(makePoint2D.invoke(v2StX, v2StY), makePoint2D.invoke(v2EnX, v2EnY)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(v3StX, v3StY), makePoint2D.invoke(v3EnX, v3EnY)));
		

		int[] pointFields = {x, y};
		checkEqualPoint2D(point, pointFields);

		int[][] lineFields = {{stX, stY}, {enX, enY}};
		checkEqualLine2D(line, lineFields, true);

		assertEquals(getObject.invoke(o), oValue);

		assertEquals(getLong.invoke(l), lValue);

		assertEquals(getDouble.invoke(d), dValue);

		assertEquals(getInt.invoke(i), iValue);

		int[][][] triangleFields = {{{v1StX, v1StY}, {v1EnX, v1EnY}}, 
				{{v2StX, v2StY}, {v2EnX, v2EnY}},
				{{v3StX, v3StY}, {v3EnX, v3EnY}}};
		
		checkEqualTriangle2D(tri, triangleFields);

		Object assortedValue = makeAssorted.invoke(point, line, o, l, d, i, tri);

		checkEqualPoint2D(getterAndWither[0][0].invoke(assortedValue), pointFields);
		checkEqualLine2D(getterAndWither[1][0].invoke(assortedValue), lineFields, true);
		assertEquals(getObject.invoke(getterAndWither[2][0].invoke(assortedValue)), oValue);
		assertEquals(getLong.invoke(getterAndWither[3][0].invoke(assortedValue)), lValue);
		assertEquals(getDouble.invoke(getterAndWither[4][0].invoke(assortedValue)), dValue);
		assertEquals(getInt.invoke(getterAndWither[5][0].invoke(assortedValue)), iValue);
		checkEqualTriangle2D(getterAndWither[6][0].invoke(assortedValue), triangleFields);

		/*
		 * point
		 */
		int xNew = 0xFFEEFFAA;
		int yNew = 0xAABBAADD;
		/*
		 * line
		 */
		int stXNew = 0x22EE22EE;
		int stYNew = 0x11441144;
		int enXNew = 0x33122111;
		int enYNew = 0x44111133;
		/*
		 * triangle
		 */
		int v1StXNew = 0x11DD11DD;
		int v1StYNew = 0x11331133;
		int v1EnXNew = 0xEE11EEEE;
		int v1EnYNew = 0x11111111;
		int v2StXNew = 0x44433311;
		int v2StYNew = 0xEE11EEE1;
		int v2EnXNew = 0x22222111;
		int v2EnYNew = 0x11111113;		
		int v3StXNew = 0x33333331;
		int v3StYNew = 0xEEEEEEE3;
		int v3EnXNew = 0x11111DDD;
		int v3EnYNew = 0xDDDD4444;
		/*
		 * o
		 */
		Object oValueNew = (Object) 0xEECAEEFF;
		/*
		 * l
		 */
		long lValueNew = 2133213321L;
		/*
		 * d
		 */
		double dValueNew = 123.42222;
		/*
		 * i
		 */
		int iValueNew = 0x123DDCBA;
		
		Object pointNew = makePoint2D.invoke(xNew, yNew);
		Object lineNew = makeFlattenedLine2D.invoke(makePoint2D.invoke(stXNew, stYNew), makePoint2D.invoke(enXNew, enYNew));
		Object oNew = makeValueObject.invoke(oValueNew);
		Object lNew = makeValueLong.invoke(lValueNew);
		Object dNew = makeValueDouble.invoke(dValueNew);
		Object iNew = makeValueInt.invoke(iValueNew);
		Object triNew = makeTriangle2D.invoke(makeFlattenedLine2D.invoke(makePoint2D.invoke(v1StXNew, v1StYNew), makePoint2D.invoke(v1EnXNew, v1EnYNew)), 
				makeFlattenedLine2D.invoke(makePoint2D.invoke(v2StXNew, v2StYNew), makePoint2D.invoke(v2EnXNew, v2EnYNew)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(v3StXNew, v3StYNew), makePoint2D.invoke(v3EnXNew, v3EnYNew)));
		
		assortedValue = getterAndWither[0][1].invoke(assortedValue, pointNew);
		assortedValue = getterAndWither[1][1].invoke(assortedValue, lineNew);
		assortedValue = getterAndWither[2][1].invoke(assortedValue, oNew);
		assortedValue = getterAndWither[3][1].invoke(assortedValue, lNew);
		assortedValue = getterAndWither[4][1].invoke(assortedValue, dNew);
		assortedValue = getterAndWither[5][1].invoke(assortedValue, iNew);
		assortedValue = getterAndWither[6][1].invoke(assortedValue, triNew);

		int[] pointFieldsNew = {xNew, yNew};
		checkEqualPoint2D(getterAndWither[0][0].invoke(assortedValue), pointFieldsNew);
		
		int[][] lineFieldNew = {{stXNew, stYNew}, {enXNew, enYNew}};
		checkEqualLine2D(getterAndWither[1][0].invoke(assortedValue), lineFieldNew, true);
		assertEquals(getObject.invoke(getterAndWither[2][0].invoke(assortedValue)), oValueNew);
		assertEquals(getLong.invoke(getterAndWither[3][0].invoke(assortedValue)), lValueNew);
		assertEquals(getDouble.invoke(getterAndWither[4][0].invoke(assortedValue)), dValueNew);
		assertEquals(getInt.invoke(getterAndWither[5][0].invoke(assortedValue)), iValueNew);
		
		int[][][] triangleFieldsNew = {{{v1StXNew, v1StYNew}, {v1EnXNew, v1EnYNew}}, 
				{{v2StXNew, v2StYNew}, {v2EnXNew, v2EnYNew}},
				{{v3StXNew, v3StYNew}, {v3EnXNew, v3EnYNew}}};
		
		checkEqualTriangle2D(getterAndWither[6][0].invoke(assortedValue), triangleFieldsNew);

		/*
		 * TODO:check object
		 */
		assertEquals(getFieldOffset(assortedValueClass, "l"), 4);
		assertEquals(getFieldOffset(assortedValueClass, "d"), 12);
		assertEquals(getFieldOffset(assortedValueClass, "o"), 24);
		assertEquals(getFieldOffset(assortedValueClass, "point"), 28);
		assertEquals(getFieldOffset(assortedValueClass, "line"), 36);
		assertEquals(getFieldOffset(assortedValueClass, "i"), 52);
		assertEquals(getFieldOffset(assortedValueClass, "tri"), 56);
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
	@Test(priority = 4)
	static public void testCreateAssortedReftWithLongAlignment() throws Throwable {
		String fields[] = {
				"point:QPoint2D;:value",
				"line:QFlattenedLine2D;:value",
				"o:QValueObject;:value",
				"l:QValueLong;:value",
				"d:QValueDouble;:value",
				"i:QValueInt;:value",
				"tri:QTriangle2D;:value" };
		Class assortedRefClass = ValueTypeGenerator.generateRefClass("AssortedRefWithLongAlignment", fields);

		MethodHandle makeAssorted = lookup.findStatic(assortedRefClass, "makeRefGeneric", MethodType
				.methodType(assortedRefClass,Object.class, Object.class, Object.class, Object.class, Object.class, Object.class, Object.class));

		/*
		 * Getters are created in array getterAndSetter[i][0] according to the order of fields i
		 * Setters are created in array getterAndSetter[i][1] according to the order of fields i
		 */
		MethodHandle[][] getterAndSetter = new MethodHandle[fields.length][2];
		for (int i = 0; i < fields.length; i++) {
			String field = (fields[i].split(":"))[0];
			getterAndSetter[i][0] = generateGenericGetter(assortedRefClass, field);
			getterAndSetter[i][1] = generateGenericSetter(assortedRefClass, field);
		}

		/*
		 * create field objects
		 */

		/*
		 * point
		 */
		int x = 0xFFEEFFEE;
		int y = 0xAABBAABB;
		/*
		 * line
		 */
		int stX = 0xFFEEFFEE;
		int stY = 0xAABBAABB;
		int enX = 0xCCAFFAAA;
		int enY = 0xBBAAAACC;
		/*
		 * triangle
		 */
		int v1StX = 0xAADDAADD;
		int v1StY = 0xAACCAACC;
		int v1EnX = 0xEEAAEEEE;
		int v1EnY = 0xAAAAAAAA;
		int v2StX = 0xBBBCCCAA;
		int v2StY = 0xEEAAEEEA;
		int v2EnX = 0xFFFFFAAA;
		int v2EnY = 0xAAAAAAAC;
		int v3StX = 0xCCCCCCCA;
		int v3StY = 0xEEEEEEEC;
		int v3EnX = 0xAAAAADDD;
		int v3EnY = 0xDDDDBBBB;
		/*
		 * o
		 */
		Object oValue = (Object)0xEEFAEEFF;
		/*
		 * l
		 */
		long lValue = 2121212121L;
		/*
		 * d
		 */
		double dValue = 123.43555;
		/*
		 * i
		 */
		int iValue = 0xABCDDCBA;

		Object point = makePoint2D.invoke(x, y);
		Object line = makeFlattenedLine2D.invoke(makePoint2D.invoke(stX, stY), makePoint2D.invoke(enX, enY));
		Object o = makeValueObject.invoke(oValue);
		Object l = makeValueLong.invoke(lValue);
		Object d = makeValueDouble.invoke(dValue);
		Object i = makeValueInt.invoke(iValue);
		Object tri = makeTriangle2D.invoke(
				makeFlattenedLine2D.invoke(makePoint2D.invoke(v1StX, v1StY), makePoint2D.invoke(v1EnX, v1EnY)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(v2StX, v2StY), makePoint2D.invoke(v2EnX, v2EnY)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(v3StX, v3StY), makePoint2D.invoke(v3EnX, v3EnY)));

		int[] pointFields = { x, y };
		checkEqualPoint2D(point, pointFields);

		int[][] lineFields = { { stX, stY }, { enX, enY } };
		checkEqualLine2D(line, lineFields, true);

		assertEquals(getObject.invoke(o), oValue);

		assertEquals(getLong.invoke(l), lValue);

		assertEquals(getDouble.invoke(d), dValue);

		assertEquals(getInt.invoke(i), iValue);

		int[][][] triangleFields = {
				{ { v1StX, v1StY }, { v1EnX, v1EnY } },
				{ { v2StX, v2StY }, { v2EnX, v2EnY } },
				{ { v3StX, v3StY }, { v3EnX, v3EnY } } };

		checkEqualTriangle2D(tri, triangleFields);

		Object assortedRef = makeAssorted.invoke(point, line, o, l, d, i, tri);

		checkEqualPoint2D(getterAndSetter[0][0].invoke(assortedRef), pointFields);
		checkEqualLine2D(getterAndSetter[1][0].invoke(assortedRef), lineFields, true);
		assertEquals(getObject.invoke(getterAndSetter[2][0].invoke(assortedRef)), oValue);
		assertEquals(getLong.invoke(getterAndSetter[3][0].invoke(assortedRef)), lValue);
		assertEquals(getDouble.invoke(getterAndSetter[4][0].invoke(assortedRef)), dValue);
		assertEquals(getInt.invoke(getterAndSetter[5][0].invoke(assortedRef)), iValue);
		checkEqualTriangle2D(getterAndSetter[6][0].invoke(assortedRef), triangleFields);

		/*
		 * point
		 */
		int xNew = 0xFFEEFFAA;
		int yNew = 0xAABBAADD;
		/*
		 * line
		 */
		int stXNew = 0x22EE22EE;
		int stYNew = 0x11441144;
		int enXNew = 0x33122111;
		int enYNew = 0x44111133;
		/*
		 * triangle
		 */
		int v1StXNew = 0x11DD11DD;
		int v1StYNew = 0x11331133;
		int v1EnXNew = 0xEE11EEEE;
		int v1EnYNew = 0x11111111;
		int v2StXNew = 0x44433311;
		int v2StYNew = 0xEE11EEE1;
		int v2EnXNew = 0x22222111;
		int v2EnYNew = 0x11111113;
		int v3StXNew = 0x33333331;
		int v3StYNew = 0xEEEEEEE3;
		int v3EnXNew = 0x11111DDD;
		int v3EnYNew = 0xDDDD4444;
		/*
		 * o
		 */
		Object oValueNew = (Object)0xEECAEEFF;
		/*
		 * l
		 */
		long lValueNew = 2133213321L;
		/*
		 * d
		 */
		double dValueNew = 123.42222;
		/*
		 * i
		 */
		int iValueNew = 0x123DDCBA;

		Object pointNew = makePoint2D.invoke(xNew, yNew);
		Object lineNew = makeFlattenedLine2D.invoke(makePoint2D.invoke(stXNew, stYNew),
				makePoint2D.invoke(enXNew, enYNew));
		Object oNew = makeValueObject.invoke(oValueNew);
		Object lNew = makeValueLong.invoke(lValueNew);
		Object dNew = makeValueDouble.invoke(dValueNew);
		Object iNew = makeValueInt.invoke(iValueNew);
		Object triNew = makeTriangle2D.invoke(
				makeFlattenedLine2D.invoke(makePoint2D.invoke(v1StXNew, v1StYNew),
						makePoint2D.invoke(v1EnXNew, v1EnYNew)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(v2StXNew, v2StYNew),
						makePoint2D.invoke(v2EnXNew, v2EnYNew)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(v3StXNew, v3StYNew),
						makePoint2D.invoke(v3EnXNew, v3EnYNew)));

		getterAndSetter[0][1].invoke(assortedRef, pointNew);
		getterAndSetter[1][1].invoke(assortedRef, lineNew);
		getterAndSetter[2][1].invoke(assortedRef, oNew);
		getterAndSetter[3][1].invoke(assortedRef, lNew);
		getterAndSetter[4][1].invoke(assortedRef, dNew);
		getterAndSetter[5][1].invoke(assortedRef, iNew);
		getterAndSetter[6][1].invoke(assortedRef, triNew);

		int[] pointFieldsNew = { xNew, yNew };
		checkEqualPoint2D(getterAndSetter[0][0].invoke(assortedRef), pointFieldsNew);

		int[][] lineFieldNew = { { stXNew, stYNew }, { enXNew, enYNew } };
		checkEqualLine2D(getterAndSetter[1][0].invoke(assortedRef), lineFieldNew, true);
		assertEquals(getObject.invoke(getterAndSetter[2][0].invoke(assortedRef)), oValueNew);
		assertEquals(getLong.invoke(getterAndSetter[3][0].invoke(assortedRef)), lValueNew);
		assertEquals(getDouble.invoke(getterAndSetter[4][0].invoke(assortedRef)), dValueNew);
		assertEquals(getInt.invoke(getterAndSetter[5][0].invoke(assortedRef)), iValueNew);

		int[][][] triangleFieldsNew = {
				{ { v1StXNew, v1StYNew }, { v1EnXNew, v1EnYNew } },
				{ { v2StXNew, v2StYNew }, { v2EnXNew, v2EnYNew } },
				{ { v3StXNew, v3StYNew }, { v3EnXNew, v3EnYNew } } };

		checkEqualTriangle2D(getterAndSetter[6][0].invoke(assortedRef), triangleFieldsNew);

		/*
		 * TODO:check object
		 */
		/* error here
		*/
		assertEquals(getFieldOffset(assortedRefClass, "l"), 4);
		assertEquals(getFieldOffset(assortedRefClass, "d"), 12);
		assertEquals(getFieldOffset(assortedRefClass, "o"), 24);
		assertEquals(getFieldOffset(assortedRefClass, "point"), 28);
		assertEquals(getFieldOffset(assortedRefClass, "line"), 36);
		assertEquals(getFieldOffset(assortedRefClass, "i"), 52);
		assertEquals(getFieldOffset(assortedRefClass, "tri"), 56);
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
	 * 	flattened ValFloat f;
	 * 	flattened Triangle2D tri2;
	 * }
	 */	 
	@Test(priority = 4)
	static public void testCreateAssortedValueWithObjectAlignment() throws Throwable {
		String fields[] = {
				"tri:QTriangle2D;:value",
				"point:QPoint2D;:value",
				"line:QFlattenedLine2D;:value",
				"o:QValueObject;:value",
				"i:QValueInt;:value",
				"f:QValueFloat;:value",
				"tri2:QTriangle2D;:value" };
		Class assortedValueClass = ValueTypeGenerator.generateValueClass("AssortedValueWithObjectAlignment", fields);

		MethodHandle makeAssorted = lookup.findStatic(assortedValueClass, "makeValueGeneric",
				MethodType.methodType(assortedValueClass, Object.class, Object.class, Object.class, Object.class,
						Object.class, Object.class, Object.class));
		/*
		 * Getters are created in array getterAndSetter[i][0] according to the order of fields i
		 * Setters are created in array getterAndSetter[i][1] according to the order of fields i
		 */
		MethodHandle[][] getterAndWither = new MethodHandle[fields.length][2];
		for (int i = 0; i < fields.length; i++) {
			String field = (fields[i].split(":"))[0];
			getterAndWither[i][0] = generateGenericGetter(assortedValueClass, field);
			getterAndWither[i][1] = generateGenericWither(assortedValueClass, field);
		}

		/*
		 * create field objects
		 */

		/*
		 * tri1
		 */
		int triV1StX = 0xAADDAADD;
		int triV1StY = 0xAACCAACC;
		int triV1EnX = 0xEEAAEEEE;
		int triV1EnY = 0xAAAAAAAA;
		int triV2StX = 0xBBBCCCAA;
		int triV2StY = 0xEEAAEEEA;
		int triV2EnX = 0xFFFFFAAA;
		int triV2EnY = 0xAAAAAAAC;
		int triV3StX = 0xCCCCCCCA;
		int triV3StY = 0xEEEEEEEC;
		int triV3EnX = 0xAAAAADDD;
		int triV3EnY = 0xDDDDBBBB;
		/*
		 * point
		 */
		int x = 0xFFEEFFEE;
		int y = 0xAABBAABB;
		/*
		 * line
		 */
		int stX = 0xFFEEFFEE;
		int stY = 0xAABBAABB;
		int enX = 0xCCAFFAAA;
		int enY = 0xBBAAAACC;
		/*
		 * tri2
		 */
		int tri2V1StX = 0x44554455;
		int tri2V1StY = 0x44CC44CC;
		int tri2V1EnX = 0xEE44EEEE;
		int tri2V1EnY = 0x44444444;
		int tri2V2StX = 0xBBBCCC44;
		int tri2V2StY = 0xEE44EEE4;
		int tri2V2EnX = 0xFFFFF444;
		int tri2V2EnY = 0x4444444C;
		int tri2V3StX = 0xCCCCCCC4;
		int tri2V3StY = 0xEEEEEEEC;
		int tri2V3EnX = 0x44444555;
		int tri2V3EnY = 0x5555BBBB;
		/*
		 * o
		 */
		Object oValue = (Object)0xEEFAEEFF;
		/*
		 * f
		 */
		float fValue = Float.MAX_VALUE;
		/*
		 * i
		 */
		int iValue = 0xABCDDCBA;

		Object point = makePoint2D.invoke(x, y);
		Object line = makeFlattenedLine2D.invoke(makePoint2D.invoke(stX, stY), makePoint2D.invoke(enX, enY));
		Object o = makeValueObject.invoke(oValue);
		Object f = makeValueFloat.invoke(fValue);
		Object i = makeValueInt.invoke(iValue);
		Object tri = makeTriangle2D.invoke(
				makeFlattenedLine2D.invoke(makePoint2D.invoke(triV1StX, triV1StY),
						makePoint2D.invoke(triV1EnX, triV1EnY)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(triV2StX, triV2StY),
						makePoint2D.invoke(triV2EnX, triV2EnY)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(triV3StX, triV3StY),
						makePoint2D.invoke(triV3EnX, triV3EnY)));

		Object tri2 = makeTriangle2D.invoke(
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri2V1StX, tri2V1StY),
						makePoint2D.invoke(tri2V1EnX, tri2V1EnY)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri2V2StX, tri2V2StY),
						makePoint2D.invoke(tri2V2EnX, tri2V2EnY)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri2V3StX, tri2V3StY),
						makePoint2D.invoke(tri2V3EnX, tri2V3EnY)));

		/*
		 * point
		 */
		int[] pointFields = { x, y };
		checkEqualPoint2D(point, pointFields);
		/*
		 * line
		 */
		int[][] lineFields = { { stX, stY }, { enX, enY } };
		checkEqualLine2D(line, lineFields, true);
		/*
		 * o
		 */
		assertEquals(getObject.invoke(o), oValue);
		/*
		 * f
		 */
		assertEquals(getFloat.invoke(f), fValue);
		/*
		 * i
		 */
		assertEquals(getInt.invoke(i), iValue);
		/*
		 * tri1
		 */
		int[][][] triangleFields = {
				{ { triV1StX, triV1StY }, { triV1EnX, triV1EnY } },
				{ { triV2StX, triV2StY }, { triV2EnX, triV2EnY } },
				{ { triV3StX, triV3StY }, { triV3EnX, triV3EnY } } };

		checkEqualTriangle2D(tri, triangleFields);
		/*
		 * tri2
		 */
		int[][][] triangle2Fields = {
				{ { tri2V1StX, tri2V1StY }, { tri2V1EnX, tri2V1EnY } },
				{ { tri2V2StX, tri2V2StY }, { tri2V2EnX, tri2V2EnY } },
				{ { tri2V3StX, tri2V3StY }, { tri2V3EnX, tri2V3EnY } } };

		checkEqualTriangle2D(tri2, triangle2Fields);

		Object assortedValue = makeAssorted.invoke(tri, point, line, o, i, f, tri2);

		checkEqualTriangle2D(getterAndWither[0][0].invoke(assortedValue), triangleFields);
		checkEqualPoint2D(getterAndWither[1][0].invoke(assortedValue), pointFields);
		checkEqualLine2D(getterAndWither[2][0].invoke(assortedValue), lineFields, true);
		assertEquals(getObject.invoke(getterAndWither[3][0].invoke(assortedValue)), oValue);
		assertEquals(getInt.invoke(getterAndWither[4][0].invoke(assortedValue)), iValue);
		assertEquals(getFloat.invoke(getterAndWither[5][0].invoke(assortedValue)), fValue);
		checkEqualTriangle2D(getterAndWither[6][0].invoke(assortedValue), triangle2Fields);

		/*
		 * point
		 */
		int xNew = 0xFFEEFFAA;
		int yNew = 0xAABBAADD;
		/*
		 * line
		 */
		int stXNew = 0x22EE22EE;
		int stYNew = 0x11441144;
		int enXNew = 0x33122111;
		int enYNew = 0x44111133;
		/*
		 * tri1
		 */
		int tri1V1StXNew = 0x11DD11DD;
		int tri1V1StYNew = 0x11331133;
		int tri1V1EnXNew = 0xEE11EEEE;
		int tri1V1EnYNew = 0x11111111;
		int tri1V2StXNew = 0x44433311;
		int tri1V2StYNew = 0xEE11EEE1;
		int tri1V2EnXNew = 0x22222111;
		int tri1V2EnYNew = 0x11111113;
		int tri1V3StXNew = 0x33333331;
		int tri1V3StYNew = 0xEEEEEEE3;
		int tri1V3EnXNew = 0x11111DDD;
		int tri1V3EnYNew = 0xDDDD4444;
		/*
		 * tri2
		 */
		int tri2V1StXNew = 0x11DD11DD;
		int tri2V1StYNew = 0x11331133;
		int tri2V1EnXNew = 0xEE11EEEE;
		int tri2V1EnYNew = 0x11111111;
		int tri2V2StXNew = 0x44433311;
		int tri2V2StYNew = 0xEE11EEE1;
		int tri2V2EnXNew = 0x22222111;
		int tri2V2EnYNew = 0x11111113;
		int tri2V3StXNew = 0x33333331;
		int tri2V3StYNew = 0xEEEEEEE3;
		int tri2V3EnXNew = 0x11111DDD;
		int tri2V3EnYNew = 0xDDDD4444;
		/*
		 * o
		 */
		Object oValueNew = (Object)0xEECAEEFF;
		/*
		 * f
		 */
		float fValueNew = 5.356f;
		/*
		 * i
		 */
		int iValueNew = 0x123DDCBA;

		Object pointNew = makePoint2D.invoke(xNew, yNew);
		Object lineNew = makeFlattenedLine2D.invoke(makePoint2D.invoke(stXNew, stYNew),
				makePoint2D.invoke(enXNew, enYNew));
		Object oNew = makeValueObject.invoke(oValueNew);
		Object fNew = makeValueFloat.invoke(fValueNew);
		Object iNew = makeValueInt.invoke(iValueNew);
		Object tri1New = makeTriangle2D.invoke(
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri1V1StXNew, tri1V1StYNew),
						makePoint2D.invoke(tri1V1EnXNew, tri1V1EnYNew)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri1V2StXNew, tri1V2StYNew),
						makePoint2D.invoke(tri1V2EnXNew, tri1V2EnYNew)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri1V3StXNew, tri1V3StYNew),
						makePoint2D.invoke(tri1V3EnXNew, tri1V3EnYNew)));

		Object tri2New = makeTriangle2D.invoke(
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri2V1StXNew, tri2V1StYNew),
						makePoint2D.invoke(tri2V1EnXNew, tri2V1EnYNew)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri2V2StXNew, tri2V2StYNew),
						makePoint2D.invoke(tri2V2EnXNew, tri2V2EnYNew)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri2V3StXNew, tri2V3StYNew),
						makePoint2D.invoke(tri2V3EnXNew, tri2V3EnYNew)));

		assortedValue = getterAndWither[0][1].invoke(assortedValue, tri1New);
		assortedValue = getterAndWither[1][1].invoke(assortedValue, pointNew);
		assortedValue = getterAndWither[2][1].invoke(assortedValue, lineNew);
		assortedValue = getterAndWither[3][1].invoke(assortedValue, oNew);
		assortedValue = getterAndWither[4][1].invoke(assortedValue, iNew);
		assortedValue = getterAndWither[5][1].invoke(assortedValue, fNew);
		assortedValue = getterAndWither[6][1].invoke(assortedValue, tri2New);

		int[][][] triangleFieldsNew = {
				{ { tri1V1StXNew, tri1V1StYNew }, { tri1V1EnXNew, tri1V1EnYNew } },
				{ { tri1V2StXNew, tri1V2StYNew }, { tri1V2EnXNew, tri1V2EnYNew } },
				{ { tri1V3StXNew, tri1V3StYNew }, { tri1V3EnXNew, tri1V3EnYNew } } };

		checkEqualTriangle2D(getterAndWither[0][0].invoke(assortedValue), triangleFieldsNew);

		int[] pointFieldsNew = { xNew, yNew };
		checkEqualPoint2D(getterAndWither[1][0].invoke(assortedValue), pointFieldsNew);

		int[][] lineFieldNew = { { stXNew, stYNew }, { enXNew, enYNew } };
		checkEqualLine2D(getterAndWither[2][0].invoke(assortedValue), lineFieldNew, true);

		assertEquals(getObject.invoke(getterAndWither[3][0].invoke(assortedValue)), oValueNew);
		assertEquals(getInt.invoke(getterAndWither[4][0].invoke(assortedValue)), iValueNew);
		assertEquals(getFloat.invoke(getterAndWither[5][0].invoke(assortedValue)), fValueNew);

		int[][][] triangle2FieldsNew = {
				{ { tri2V1StXNew, tri2V1StYNew }, { tri2V1EnXNew, tri2V1EnYNew } },
				{ { tri2V2StXNew, tri2V2StYNew }, { tri2V2EnXNew, tri2V2EnYNew } },
				{ { tri2V3StXNew, tri2V3StYNew }, { tri2V3EnXNew, tri2V3EnYNew } } };

		checkEqualTriangle2D(getterAndWither[6][0].invoke(assortedValue), triangle2FieldsNew);

		/*
		 * TODO:check object
		 */
		assertEquals(getFieldOffset(assortedValueClass, "o"), 4);
		assertEquals(getFieldOffset(assortedValueClass, "tri"), 8);
		assertEquals(getFieldOffset(assortedValueClass, "point"), 56);
		assertEquals(getFieldOffset(assortedValueClass, "line"), 64);
		assertEquals(getFieldOffset(assortedValueClass, "i"), 80);
		assertEquals(getFieldOffset(assortedValueClass, "f"), 84);
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
	 * 	flattened ValFloat f;
	 * 	flattened Triangle2D tri2;
	 * }
	 */	 
	@Test(priority = 4)
	static public void testCreateAssortedRefWithObjectAlignment() throws Throwable {
		String fields[] = {
				"tri:QTriangle2D;:value",
				"point:QPoint2D;:value",
				"line:QFlattenedLine2D;:value",
				"o:QValueObject;:value",
				"i:QValueInt;:value",
				"f:QValueFloat;:value",
				"tri2:QTriangle2D;:value" };
		Class assortedRefClass = ValueTypeGenerator.generateRefClass("AssortedRefWithObjectAlignment", fields);

		MethodHandle makeAssorted = lookup.findStatic(assortedRefClass, "makeRefGeneric",
				MethodType.methodType(assortedRefClass, Object.class, Object.class, Object.class, Object.class,
						Object.class, Object.class, Object.class));
		/*
		 * Getters are created in array getterAndSetter[i][0] according to the order of fields i
		 * Setters are created in array getterAndSetter[i][1] according to the order of fields i
		 */
		MethodHandle[][] getterAndSetter = new MethodHandle[fields.length][2];
		for (int i = 0; i < fields.length; i++) {
			String field = (fields[i].split(":"))[0];
			getterAndSetter[i][0] = generateGenericGetter(assortedRefClass, field);
			getterAndSetter[i][1] = generateGenericSetter(assortedRefClass, field);
		}
		
		/*
		 * create field objects
		 */
		
		/*
		 * tri1
		 */
		int triV1StX = 0xAADDAADD;
		int triV1StY = 0xAACCAACC;
		int triV1EnX = 0xEEAAEEEE;
		int triV1EnY = 0xAAAAAAAA;
		int triV2StX = 0xBBBCCCAA;
		int triV2StY = 0xEEAAEEEA;
		int triV2EnX = 0xFFFFFAAA;
		int triV2EnY = 0xAAAAAAAC;
		int triV3StX = 0xCCCCCCCA;
		int triV3StY = 0xEEEEEEEC;
		int triV3EnX = 0xAAAAADDD;
		int triV3EnY = 0xDDDDBBBB;
		/*
		 * point
		 */
		int x = 0xFFEEFFEE;
		int y = 0xAABBAABB;
		/*
		 * line
		 */
		int stX = 0xFFEEFFEE;
		int stY = 0xAABBAABB;
		int enX = 0xCCAFFAAA;
		int enY = 0xBBAAAACC;
		/*
		 * tri2
		 */
		int tri2V1StX = 0x44554455;
		int tri2V1StY = 0x44CC44CC;
		int tri2V1EnX = 0xEE44EEEE;
		int tri2V1EnY = 0x44444444;
		int tri2V2StX = 0xBBBCCC44;
		int tri2V2StY = 0xEE44EEE4;
		int tri2V2EnX = 0xFFFFF444;
		int tri2V2EnY = 0x4444444C;
		int tri2V3StX = 0xCCCCCCC4;
		int tri2V3StY = 0xEEEEEEEC;
		int tri2V3EnX = 0x44444555;
		int tri2V3EnY = 0x5555BBBB;
		/*
		 * o
		 */
		Object oValue = (Object)0xEEFAEEFF;
		/*
		 * f
		 */
		float fValue = Float.MAX_VALUE;
		/*
		 * i
		 */
		int iValue = 0xABCDDCBA;

		Object point = makePoint2D.invoke(x, y);
		Object line = makeFlattenedLine2D.invoke(makePoint2D.invoke(stX, stY), makePoint2D.invoke(enX, enY));
		Object o = makeValueObject.invoke(oValue);
		Object f = makeValueFloat.invoke(fValue);
		Object i = makeValueInt.invoke(iValue);
		Object tri = makeTriangle2D.invoke(
				makeFlattenedLine2D.invoke(makePoint2D.invoke(triV1StX, triV1StY),
						makePoint2D.invoke(triV1EnX, triV1EnY)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(triV2StX, triV2StY),
						makePoint2D.invoke(triV2EnX, triV2EnY)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(triV3StX, triV3StY),
						makePoint2D.invoke(triV3EnX, triV3EnY)));

		Object tri2 = makeTriangle2D.invoke(
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri2V1StX, tri2V1StY),
						makePoint2D.invoke(tri2V1EnX, tri2V1EnY)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri2V2StX, tri2V2StY),
						makePoint2D.invoke(tri2V2EnX, tri2V2EnY)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri2V3StX, tri2V3StY),
						makePoint2D.invoke(tri2V3EnX, tri2V3EnY)));
		
		/*
		 * point
		 */
		int[] pointFields = { x, y };
		checkEqualPoint2D(point, pointFields);
		/*
		 * line
		 */
		int[][] lineFields = { { stX, stY }, { enX, enY } };
		checkEqualLine2D(line, lineFields, true);
		/*
		 * o
		 */
		assertEquals(getObject.invoke(o), oValue);
		/*
		 * f
		 */
		assertEquals(getFloat.invoke(f), fValue);
		/*
		 * i
		 */
		assertEquals(getInt.invoke(i), iValue);
		/*
		 * tri1
		 */
		int[][][] triangleFields = {
				{ { triV1StX, triV1StY }, { triV1EnX, triV1EnY } },
				{ { triV2StX, triV2StY }, { triV2EnX, triV2EnY } },
				{ { triV3StX, triV3StY }, { triV3EnX, triV3EnY } } };

		checkEqualTriangle2D(tri, triangleFields);
		/*
		 * tri2
		 */
		int[][][] triangle2Fields = {
				{ { tri2V1StX, tri2V1StY }, { tri2V1EnX, tri2V1EnY } },
				{ { tri2V2StX, tri2V2StY }, { tri2V2EnX, tri2V2EnY } },
				{ { tri2V3StX, tri2V3StY }, { tri2V3EnX, tri2V3EnY } } };

		checkEqualTriangle2D(tri2, triangle2Fields);
		
		Object assortedRef = makeAssorted.invoke(tri, point, line, o, i, f, tri2);

		checkEqualTriangle2D(getterAndSetter[0][0].invoke(assortedRef), triangleFields);
		checkEqualPoint2D(getterAndSetter[1][0].invoke(assortedRef), pointFields);
		checkEqualLine2D(getterAndSetter[2][0].invoke(assortedRef), lineFields, true);
		assertEquals(getObject.invoke(getterAndSetter[3][0].invoke(assortedRef)), oValue);
		assertEquals(getInt.invoke(getterAndSetter[4][0].invoke(assortedRef)), iValue);
		assertEquals(getFloat.invoke(getterAndSetter[5][0].invoke(assortedRef)), fValue);
		checkEqualTriangle2D(getterAndSetter[6][0].invoke(assortedRef), triangle2Fields);

		/*
		 * point
		 */
		int xNew = 0xFFEEFFAA;
		int yNew = 0xAABBAADD;
		/*
		 * line
		 */
		int stXNew = 0x22EE22EE;
		int stYNew = 0x11441144;
		int enXNew = 0x33122111;
		int enYNew = 0x44111133;
		/*
		 * tri1
		 */
		int tri1V1StXNew = 0x11DD11DD;
		int tri1V1StYNew = 0x11331133;
		int tri1V1EnXNew = 0xEE11EEEE;
		int tri1V1EnYNew = 0x11111111;
		int tri1V2StXNew = 0x44433311;
		int tri1V2StYNew = 0xEE11EEE1;
		int tri1V2EnXNew = 0x22222111;
		int tri1V2EnYNew = 0x11111113;
		int tri1V3StXNew = 0x33333331;
		int tri1V3StYNew = 0xEEEEEEE3;
		int tri1V3EnXNew = 0x11111DDD;
		int tri1V3EnYNew = 0xDDDD4444;
		/*
		 * tri2
		 */
		int tri2V1StXNew = 0x11DD11DD;
		int tri2V1StYNew = 0x11331133;
		int tri2V1EnXNew = 0xEE11EEEE;
		int tri2V1EnYNew = 0x11111111;
		int tri2V2StXNew = 0x44433311;
		int tri2V2StYNew = 0xEE11EEE1;
		int tri2V2EnXNew = 0x22222111;
		int tri2V2EnYNew = 0x11111113;
		int tri2V3StXNew = 0x33333331;
		int tri2V3StYNew = 0xEEEEEEE3;
		int tri2V3EnXNew = 0x11111DDD;
		int tri2V3EnYNew = 0xDDDD4444;
		/*
		 * o
		 */
		Object oValueNew = (Object)0xEECAEEFF;
		/*
		 * f
		 */
		float fValueNew = 5.356f;
		/*
		 * i
		 */
		int iValueNew = 0x123DDCBA;
		
		Object pointNew = makePoint2D.invoke(xNew, yNew);
		Object lineNew = makeFlattenedLine2D.invoke(makePoint2D.invoke(stXNew, stYNew),
				makePoint2D.invoke(enXNew, enYNew));
		Object oNew = makeValueObject.invoke(oValueNew);
		Object fNew = makeValueFloat.invoke(fValueNew);
		Object iNew = makeValueInt.invoke(iValueNew);
		Object tri1New = makeTriangle2D.invoke(
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri1V1StXNew, tri1V1StYNew),
						makePoint2D.invoke(tri1V1EnXNew, tri1V1EnYNew)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri1V2StXNew, tri1V2StYNew),
						makePoint2D.invoke(tri1V2EnXNew, tri1V2EnYNew)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri1V3StXNew, tri1V3StYNew),
						makePoint2D.invoke(tri1V3EnXNew, tri1V3EnYNew)));
		
		Object tri2New = makeTriangle2D.invoke(
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri2V1StXNew, tri2V1StYNew),
						makePoint2D.invoke(tri2V1EnXNew, tri2V1EnYNew)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri2V2StXNew, tri2V2StYNew),
						makePoint2D.invoke(tri2V2EnXNew, tri2V2EnYNew)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri2V3StXNew, tri2V3StYNew),
						makePoint2D.invoke(tri2V3EnXNew, tri2V3EnYNew)));
		
		getterAndSetter[0][1].invoke(assortedRef, tri1New);
		getterAndSetter[1][1].invoke(assortedRef, pointNew);
		getterAndSetter[2][1].invoke(assortedRef, lineNew);
		getterAndSetter[3][1].invoke(assortedRef, oNew);
		getterAndSetter[4][1].invoke(assortedRef, iNew);
		getterAndSetter[5][1].invoke(assortedRef, fNew);
		getterAndSetter[6][1].invoke(assortedRef, tri2New);

		int[][][] triangleFieldsNew = {
				{ { tri1V1StXNew, tri1V1StYNew }, { tri1V1EnXNew, tri1V1EnYNew } },
				{ { tri1V2StXNew, tri1V2StYNew }, { tri1V2EnXNew, tri1V2EnYNew } },
				{ { tri1V3StXNew, tri1V3StYNew }, { tri1V3EnXNew, tri1V3EnYNew } } };

		checkEqualTriangle2D(getterAndSetter[0][0].invoke(assortedRef), triangleFieldsNew);

		int[] pointFieldsNew = { xNew, yNew };
		checkEqualPoint2D(getterAndSetter[1][0].invoke(assortedRef), pointFieldsNew);

		int[][] lineFieldNew = { { stXNew, stYNew }, { enXNew, enYNew } };
		checkEqualLine2D(getterAndSetter[2][0].invoke(assortedRef), lineFieldNew, true);
		
		assertEquals(getObject.invoke(getterAndSetter[3][0].invoke(assortedRef)), oValueNew);
		assertEquals(getInt.invoke(getterAndSetter[4][0].invoke(assortedRef)), iValueNew);
		assertEquals(getFloat.invoke(getterAndSetter[5][0].invoke(assortedRef)), fValueNew);
		
		int[][][] triangle2FieldsNew = {
				{ { tri2V1StXNew, tri2V1StYNew }, { tri2V1EnXNew, tri2V1EnYNew } },
				{ { tri2V2StXNew, tri2V2StYNew }, { tri2V2EnXNew, tri2V2EnYNew } },
				{ { tri2V3StXNew, tri2V3StYNew }, { tri2V3EnXNew, tri2V3EnYNew } } };

		checkEqualTriangle2D(getterAndSetter[6][0].invoke(assortedRef), triangle2FieldsNew);

		/*
		 * TODO:check object
		 */
		assertEquals(getFieldOffset(assortedRefClass, "o"), 8);
		assertEquals(getFieldOffset(assortedRefClass, "tri"), 12);
		assertEquals(getFieldOffset(assortedRefClass, "point"), 60);
		assertEquals(getFieldOffset(assortedRefClass, "line"), 68);
		assertEquals(getFieldOffset(assortedRefClass, "i"), 84);
		assertEquals(getFieldOffset(assortedRefClass, "f"), 88);
	}
	
	/*
	 * Create an assorted value type with single alignment 
	 * 
	 * value AssortedValueWithSingleAlignment {
	 * 	flattened Triangle2D tri;
	 * 	flattened Point2D point;
	 * 	flattened Line2D line;
	 * 	flattened ValueInt i;
	 * 	flattened ValFloat f;
	 * 	flattened Triangle2D tri2;
	 * }
	 */
	@Test(priority = 4)
	static public void testCreateAssortedValueWithSingleAlignment() throws Throwable {
		String fields[] = {
				"tri:QTriangle2D;:value",
				"point:QPoint2D;:value",
				"line:QFlattenedLine2D;:value",
				"i:QValueInt;:value",
				"f:QValueFloat;:value",
				"tri2:QTriangle2D;:value" };
		Class assortedValueClass = ValueTypeGenerator.generateValueClass("AssortedValueWithSingleAlignment", fields);

		MethodHandle makeAssorted = lookup.findStatic(assortedValueClass, "makeValueGeneric",
				MethodType.methodType(assortedValueClass, Object.class, Object.class, Object.class, Object.class,
						Object.class, Object.class));
		/*
		 * Getters are created in array getterAndSetter[i][0] according to the order of fields i
		 * Setters are created in array getterAndSetter[i][1] according to the order of fields i
		 */
		MethodHandle[][] getterAndWither = new MethodHandle[fields.length][2];
		for (int i = 0; i < fields.length; i++) {
			String field = (fields[i].split(":"))[0];
			getterAndWither[i][0] = generateGenericGetter(assortedValueClass, field);
			getterAndWither[i][1] = generateGenericWither(assortedValueClass, field);
		}

		/*
		 * create field objects
		 */

		/*
		 * tri1
		 */
		int triV1StX = 0xAADDAADD;
		int triV1StY = 0xAACCAACC;
		int triV1EnX = 0xEEAAEEEE;
		int triV1EnY = 0xAAAAAAAA;
		int triV2StX = 0xBBBCCCAA;
		int triV2StY = 0xEEAAEEEA;
		int triV2EnX = 0xFFFFFAAA;
		int triV2EnY = 0xAAAAAAAC;
		int triV3StX = 0xCCCCCCCA;
		int triV3StY = 0xEEEEEEEC;
		int triV3EnX = 0xAAAAADDD;
		int triV3EnY = 0xDDDDBBBB;
		/*
		 * point
		 */
		int x = 0xFFEEFFEE;
		int y = 0xAABBAABB;
		/*
		 * line
		 */
		int stX = 0xFFEEFFEE;
		int stY = 0xAABBAABB;
		int enX = 0xCCAFFAAA;
		int enY = 0xBBAAAACC;
		/*
		 * tri2
		 */
		int tri2V1StX = 0x44554455;
		int tri2V1StY = 0x44CC44CC;
		int tri2V1EnX = 0xEE44EEEE;
		int tri2V1EnY = 0x44444444;
		int tri2V2StX = 0xBBBCCC44;
		int tri2V2StY = 0xEE44EEE4;
		int tri2V2EnX = 0xFFFFF444;
		int tri2V2EnY = 0x4444444C;
		int tri2V3StX = 0xCCCCCCC4;
		int tri2V3StY = 0xEEEEEEEC;
		int tri2V3EnX = 0x44444555;
		int tri2V3EnY = 0x5555BBBB;
		/*
		 * f
		 */
		float fValue = Float.MAX_VALUE;
		/*
		 * i
		 */
		int iValue = 0xABCDDCBA;

		Object point = makePoint2D.invoke(x, y);
		Object line = makeFlattenedLine2D.invoke(makePoint2D.invoke(stX, stY), makePoint2D.invoke(enX, enY));
		Object f = makeValueFloat.invoke(fValue);
		Object i = makeValueInt.invoke(iValue);
		Object tri = makeTriangle2D.invoke(
				makeFlattenedLine2D.invoke(makePoint2D.invoke(triV1StX, triV1StY),
						makePoint2D.invoke(triV1EnX, triV1EnY)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(triV2StX, triV2StY),
						makePoint2D.invoke(triV2EnX, triV2EnY)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(triV3StX, triV3StY),
						makePoint2D.invoke(triV3EnX, triV3EnY)));

		Object tri2 = makeTriangle2D.invoke(
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri2V1StX, tri2V1StY),
						makePoint2D.invoke(tri2V1EnX, tri2V1EnY)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri2V2StX, tri2V2StY),
						makePoint2D.invoke(tri2V2EnX, tri2V2EnY)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri2V3StX, tri2V3StY),
						makePoint2D.invoke(tri2V3EnX, tri2V3EnY)));

		/*
		 * point
		 */
		int[] pointFields = { x, y };
		checkEqualPoint2D(point, pointFields);
		/*
		 * line
		 */
		int[][] lineFields = { { stX, stY }, { enX, enY } };
		checkEqualLine2D(line, lineFields, true);
		/*
		 * f
		 */
		assertEquals(getFloat.invoke(f), fValue);
		/*
		 * tri1
		 */
		int[][][] triangleFields = {
				{ { triV1StX, triV1StY }, { triV1EnX, triV1EnY } },
				{ { triV2StX, triV2StY }, { triV2EnX, triV2EnY } },
				{ { triV3StX, triV3StY }, { triV3EnX, triV3EnY } } };

		checkEqualTriangle2D(tri, triangleFields);
		/*
		 * tri2
		 */
		int[][][] triangle2Fields = {
				{ { tri2V1StX, tri2V1StY }, { tri2V1EnX, tri2V1EnY } },
				{ { tri2V2StX, tri2V2StY }, { tri2V2EnX, tri2V2EnY } },
				{ { tri2V3StX, tri2V3StY }, { tri2V3EnX, tri2V3EnY } } };

		checkEqualTriangle2D(tri2, triangle2Fields);

		Object assortedValue = makeAssorted.invoke(tri, point, line, i, f, tri2);

		checkEqualTriangle2D(getterAndWither[0][0].invoke(assortedValue), triangleFields);
		checkEqualPoint2D(getterAndWither[1][0].invoke(assortedValue), pointFields);
		checkEqualLine2D(getterAndWither[2][0].invoke(assortedValue), lineFields, true);
		assertEquals(getInt.invoke(getterAndWither[3][0].invoke(assortedValue)), iValue);
		assertEquals(getFloat.invoke(getterAndWither[4][0].invoke(assortedValue)), fValue);
		checkEqualTriangle2D(getterAndWither[5][0].invoke(assortedValue), triangle2Fields);

		/*
		 * point
		 */
		int xNew = 0xFFEEFFAA;
		int yNew = 0xAABBAADD;
		/*
		 * line
		 */
		int stXNew = 0x22EE22EE;
		int stYNew = 0x11441144;
		int enXNew = 0x33122111;
		int enYNew = 0x44111133;
		/*
		 * tri1
		 */
		int tri1V1StXNew = 0x11DD11DD;
		int tri1V1StYNew = 0x11331133;
		int tri1V1EnXNew = 0xEE11EEEE;
		int tri1V1EnYNew = 0x11111111;
		int tri1V2StXNew = 0x44433311;
		int tri1V2StYNew = 0xEE11EEE1;
		int tri1V2EnXNew = 0x22222111;
		int tri1V2EnYNew = 0x11111113;
		int tri1V3StXNew = 0x33333331;
		int tri1V3StYNew = 0xEEEEEEE3;
		int tri1V3EnXNew = 0x11111DDD;
		int tri1V3EnYNew = 0xDDDD4444;
		/*
		 * tri2
		 */
		int tri2V1StXNew = 0x11DD11DD;
		int tri2V1StYNew = 0x11331133;
		int tri2V1EnXNew = 0xEE11EEEE;
		int tri2V1EnYNew = 0x11111111;
		int tri2V2StXNew = 0x44433311;
		int tri2V2StYNew = 0xEE11EEE1;
		int tri2V2EnXNew = 0x22222111;
		int tri2V2EnYNew = 0x11111113;
		int tri2V3StXNew = 0x33333331;
		int tri2V3StYNew = 0xEEEEEEE3;
		int tri2V3EnXNew = 0x11111DDD;
		int tri2V3EnYNew = 0xDDDD4444;
		/*
		 * f
		 */
		float fValueNew = 5.356f;
		/*
		 * i
		 */
		int iValueNew = 0x123DDCBA;

		Object pointNew = makePoint2D.invoke(xNew, yNew);
		Object lineNew = makeFlattenedLine2D.invoke(makePoint2D.invoke(stXNew, stYNew),
				makePoint2D.invoke(enXNew, enYNew));
		Object fNew = makeValueFloat.invoke(fValueNew);
		Object iNew = makeValueInt.invoke(iValueNew);
		Object tri1New = makeTriangle2D.invoke(
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri1V1StXNew, tri1V1StYNew),
						makePoint2D.invoke(tri1V1EnXNew, tri1V1EnYNew)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri1V2StXNew, tri1V2StYNew),
						makePoint2D.invoke(tri1V2EnXNew, tri1V2EnYNew)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri1V3StXNew, tri1V3StYNew),
						makePoint2D.invoke(tri1V3EnXNew, tri1V3EnYNew)));

		Object tri2New = makeTriangle2D.invoke(
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri2V1StXNew, tri2V1StYNew),
						makePoint2D.invoke(tri2V1EnXNew, tri2V1EnYNew)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri2V2StXNew, tri2V2StYNew),
						makePoint2D.invoke(tri2V2EnXNew, tri2V2EnYNew)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri2V3StXNew, tri2V3StYNew),
						makePoint2D.invoke(tri2V3EnXNew, tri2V3EnYNew)));

		assortedValue = getterAndWither[0][1].invoke(assortedValue, tri1New);
		assortedValue = getterAndWither[1][1].invoke(assortedValue, pointNew);
		assortedValue = getterAndWither[2][1].invoke(assortedValue, lineNew);
		assortedValue = getterAndWither[3][1].invoke(assortedValue, iNew);
		assortedValue = getterAndWither[4][1].invoke(assortedValue, fNew);
		assortedValue = getterAndWither[5][1].invoke(assortedValue, tri2New);

		int[][][] triangleFieldsNew = {
				{ { tri1V1StXNew, tri1V1StYNew }, { tri1V1EnXNew, tri1V1EnYNew } },
				{ { tri1V2StXNew, tri1V2StYNew }, { tri1V2EnXNew, tri1V2EnYNew } },
				{ { tri1V3StXNew, tri1V3StYNew }, { tri1V3EnXNew, tri1V3EnYNew } } };

		checkEqualTriangle2D(getterAndWither[0][0].invoke(assortedValue), triangleFieldsNew);

		int[] pointFieldsNew = { xNew, yNew };
		checkEqualPoint2D(getterAndWither[1][0].invoke(assortedValue), pointFieldsNew);

		int[][] lineFieldNew = { { stXNew, stYNew }, { enXNew, enYNew } };
		checkEqualLine2D(getterAndWither[2][0].invoke(assortedValue), lineFieldNew, true);

		assertEquals(getInt.invoke(getterAndWither[3][0].invoke(assortedValue)), iValueNew);
		assertEquals(getFloat.invoke(getterAndWither[4][0].invoke(assortedValue)), fValueNew);

		int[][][] triangle2FieldsNew = {
				{ { tri2V1StXNew, tri2V1StYNew }, { tri2V1EnXNew, tri2V1EnYNew } },
				{ { tri2V2StXNew, tri2V2StYNew }, { tri2V2EnXNew, tri2V2EnYNew } },
				{ { tri2V3StXNew, tri2V3StYNew }, { tri2V3EnXNew, tri2V3EnYNew } } };

		checkEqualTriangle2D(getterAndWither[5][0].invoke(assortedValue), triangle2FieldsNew);

		/*
		 * TODO:check object
		 */
		assertEquals(getFieldOffset(assortedValueClass, "tri"), 4);
		assertEquals(getFieldOffset(assortedValueClass, "point"), 52);
		assertEquals(getFieldOffset(assortedValueClass, "line"), 60);
		assertEquals(getFieldOffset(assortedValueClass, "i"), 76);
		assertEquals(getFieldOffset(assortedValueClass, "f"), 80);
		assertEquals(getFieldOffset(assortedValueClass, "tri2"), 84);
	}
	
	/*
	 * Create an assorted refType with single alignment 
	 * 
	 * class AssortedRefWithSingleAlignment {
	 * 	flattened Triangle2D tri;
	 * 	flattened Point2D point;
	 * 	flattened Line2D line;
	 * 	flattened ValueInt i;
	 * 	flattened ValFloat f;
	 * 	flattened Triangle2D tri2;
	 * }
	 */	 
	@Test(priority = 4)
	static public void testCreateAssortedRefWithSingleAlignment() throws Throwable {
		String fields[] = {
				"tri:QTriangle2D;:value",
				"point:QPoint2D;:value",
				"line:QFlattenedLine2D;:value",
				"i:QValueInt;:value",
				"f:QValueFloat;:value",
				"tri2:QTriangle2D;:value" };
		Class assortedRefClass = ValueTypeGenerator.generateRefClass("AssortedRefWithSingleAlignment", fields);

		MethodHandle makeAssorted = lookup.findStatic(assortedRefClass, "makeRefGeneric",
				MethodType.methodType(assortedRefClass, Object.class, Object.class, Object.class, Object.class,
						Object.class, Object.class));
		/*
		 * Getters are created in array getterAndSetter[i][0] according to the order of fields i
		 * Setters are created in array getterAndSetter[i][1] according to the order of fields i
		 */
		MethodHandle[][] getterAndSetter = new MethodHandle[fields.length][2];
		for (int i = 0; i < fields.length; i++) {
			String field = (fields[i].split(":"))[0];
			getterAndSetter[i][0] = generateGenericGetter(assortedRefClass, field);
			getterAndSetter[i][1] = generateGenericSetter(assortedRefClass, field);
		}
		
		/*
		 * create field objects
		 */
		
		/*
		 * tri1
		 */
		int tri1V1StX = 0xAADDAADD;
		int tri1V1StY = 0xAACCAACC;
		int tri1V1EnX = 0xEEAAEEEE;
		int tri1V1EnY = 0xAAAAAAAA;
		int tri1V2StX = 0xBBBCCCAA;
		int tri1V2StY = 0xEEAAEEEA;
		int tri1V2EnX = 0xFFFFFAAA;
		int tri1V2EnY = 0xAAAAAAAC;
		int tri1V3StX = 0xCCCCCCCA;
		int tri1V3StY = 0xEEEEEEEC;
		int tri1V3EnX = 0xAAAAADDD;
		int tri1V3EnY = 0xDDDDBBBB;
		/*
		 * point
		 */
		int x = 0xFFEEFFEE;
		int y = 0xAABBAABB;
		/*
		 * line
		 */
		int stX = 0xFFEEFFEE;
		int stY = 0xAABBAABB;
		int enX = 0xCCAFFAAA;
		int enY = 0xBBAAAACC;
		/*
		 * tri2
		 */
		int tri2V1StX = 0x44554455;
		int tri2V1StY = 0x44CC44CC;
		int tri2V1EnX = 0xEE44EEEE;
		int tri2V1EnY = 0x44444444;
		int tri2V2StX = 0xBBBCCC44;
		int tri2V2StY = 0xEE44EEE4;
		int tri2V2EnX = 0xFFFFF444;
		int tri2V2EnY = 0x4444444C;
		int tri2V3StX = 0xCCCCCCC4;
		int tri2V3StY = 0xEEEEEEEC;
		int tri2V3EnX = 0x44444555;
		int tri2V3EnY = 0x5555BBBB;
		/*
		 * f
		 */
		float fValue = Float.MAX_VALUE;
		/*
		 * i
		 */
		int iValue = 0xABCDDCBA;

		Object point = makePoint2D.invoke(x, y);
		Object line = makeFlattenedLine2D.invoke(makePoint2D.invoke(stX, stY), makePoint2D.invoke(enX, enY));
		Object f = makeValueFloat.invoke(fValue);
		Object i = makeValueInt.invoke(iValue);
		Object tri = makeTriangle2D.invoke(
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri1V1StX, tri1V1StY),
						makePoint2D.invoke(tri1V1EnX, tri1V1EnY)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri1V2StX, tri1V2StY),
						makePoint2D.invoke(tri1V2EnX, tri1V2EnY)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri1V3StX, tri1V3StY),
						makePoint2D.invoke(tri1V3EnX, tri1V3EnY)));

		Object tri2 = makeTriangle2D.invoke(
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri2V1StX, tri2V1StY),
						makePoint2D.invoke(tri2V1EnX, tri2V1EnY)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri2V2StX, tri2V2StY),
						makePoint2D.invoke(tri2V2EnX, tri2V2EnY)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri2V3StX, tri2V3StY),
						makePoint2D.invoke(tri2V3EnX, tri2V3EnY)));
		
		/*
		 * point
		 */
		int[] pointFields = { x, y };
		checkEqualPoint2D(point, pointFields);
		/*
		 * line
		 */
		int[][] lineFields = { { stX, stY }, { enX, enY } };
		checkEqualLine2D(line, lineFields, true);
		/*
		 * f
		 */
		assertEquals(getFloat.invoke(f), fValue);
		/*
		 * i
		 */
		assertEquals(getInt.invoke(i), iValue);
		/*
		 * tri1
		 */
		int[][][] triangleFields = {
				{ { tri1V1StX, tri1V1StY }, { tri1V1EnX, tri1V1EnY } },
				{ { tri1V2StX, tri1V2StY }, { tri1V2EnX, tri1V2EnY } },
				{ { tri1V3StX, tri1V3StY }, { tri1V3EnX, tri1V3EnY } } };

		checkEqualTriangle2D(tri, triangleFields);
		/*
		 * tri2
		 */
		int[][][] triangle2Fields = {
				{ { tri2V1StX, tri2V1StY }, { tri2V1EnX, tri2V1EnY } },
				{ { tri2V2StX, tri2V2StY }, { tri2V2EnX, tri2V2EnY } },
				{ { tri2V3StX, tri2V3StY }, { tri2V3EnX, tri2V3EnY } } };

		checkEqualTriangle2D(tri2, triangle2Fields);
		
		Object assortedRef = makeAssorted.invoke(tri, point, line, i, f, tri2);

		checkEqualTriangle2D(getterAndSetter[0][0].invoke(assortedRef), triangleFields);
		checkEqualPoint2D(getterAndSetter[1][0].invoke(assortedRef), pointFields);
		checkEqualLine2D(getterAndSetter[2][0].invoke(assortedRef), lineFields, true);
		assertEquals(getInt.invoke(getterAndSetter[3][0].invoke(assortedRef)), iValue);
		assertEquals(getFloat.invoke(getterAndSetter[4][0].invoke(assortedRef)), fValue);
		checkEqualTriangle2D(getterAndSetter[5][0].invoke(assortedRef), triangle2Fields);

		/*
		 * point
		 */
		int xNew = 0xFFEEFFAA;
		int yNew = 0xAABBAADD;
		/*
		 * line
		 */
		int stXNew = 0x22EE22EE;
		int stYNew = 0x11441144;
		int enXNew = 0x33122111;
		int enYNew = 0x44111133;
		/*
		 * tri1
		 */
		int tri1V1StXNew = 0x11DD11DD;
		int tri1V1StYNew = 0x11331133;
		int tri1V1EnXNew = 0xEE11EEEE;
		int tri1V1EnYNew = 0x11111111;
		int tri1V2StXNew = 0x44433311;
		int tri1V2StYNew = 0xEE11EEE1;
		int tri1V2EnXNew = 0x22222111;
		int tri1V2EnYNew = 0x11111113;
		int tri1V3StXNew = 0x33333331;
		int tri1V3StYNew = 0xEEEEEEE3;
		int tri1V3EnXNew = 0x11111DDD;
		int tri1V3EnYNew = 0xDDDD4444;
		/*
		 * tri2
		 */
		int tri2V1StXNew = 0x11DD11DD;
		int tri2V1StYNew = 0x11331133;
		int tri2V1EnXNew = 0xEE11EEEE;
		int tri2V1EnYNew = 0x11111111;
		int tri2V2StXNew = 0x44433311;
		int tri2V2StYNew = 0xEE11EEE1;
		int tri2V2EnXNew = 0x22222111;
		int tri2V2EnYNew = 0x11111113;
		int tri2V3StXNew = 0x33333331;
		int tri2V3StYNew = 0xEEEEEEE3;
		int tri2V3EnXNew = 0x11111DDD;
		int tri2V3EnYNew = 0xDDDD4444;
		/*
		 * f
		 */
		float fValueNew = 5.356f;
		/*
		 * i
		 */
		int iValueNew = 0x123DDCBA;
		
		Object pointNew = makePoint2D.invoke(xNew, yNew);
		Object lineNew = makeFlattenedLine2D.invoke(makePoint2D.invoke(stXNew, stYNew),
				makePoint2D.invoke(enXNew, enYNew));
		Object fNew = makeValueFloat.invoke(fValueNew);
		Object iNew = makeValueInt.invoke(iValueNew);
		Object tri1New = makeTriangle2D.invoke(
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri1V1StXNew, tri1V1StYNew),
						makePoint2D.invoke(tri1V1EnXNew, tri1V1EnYNew)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri1V2StXNew, tri1V2StYNew),
						makePoint2D.invoke(tri1V2EnXNew, tri1V2EnYNew)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri1V3StXNew, tri1V3StYNew),
						makePoint2D.invoke(tri1V3EnXNew, tri1V3EnYNew)));
		
		Object tri2New = makeTriangle2D.invoke(
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri2V1StXNew, tri2V1StYNew),
						makePoint2D.invoke(tri2V1EnXNew, tri2V1EnYNew)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri2V2StXNew, tri2V2StYNew),
						makePoint2D.invoke(tri2V2EnXNew, tri2V2EnYNew)),
				makeFlattenedLine2D.invoke(makePoint2D.invoke(tri2V3StXNew, tri2V3StYNew),
						makePoint2D.invoke(tri2V3EnXNew, tri2V3EnYNew)));
		
		getterAndSetter[0][1].invoke(assortedRef, tri1New);
		getterAndSetter[1][1].invoke(assortedRef, pointNew);
		getterAndSetter[2][1].invoke(assortedRef, lineNew);
		getterAndSetter[3][1].invoke(assortedRef, iNew);
		getterAndSetter[4][1].invoke(assortedRef, fNew);
		getterAndSetter[5][1].invoke(assortedRef, tri2New);

		int[][][] triangleFieldsNew = {
				{ { tri1V1StXNew, tri1V1StYNew }, { tri1V1EnXNew, tri1V1EnYNew } },
				{ { tri1V2StXNew, tri1V2StYNew }, { tri1V2EnXNew, tri1V2EnYNew } },
				{ { tri1V3StXNew, tri1V3StYNew }, { tri1V3EnXNew, tri1V3EnYNew } } };

		checkEqualTriangle2D(getterAndSetter[0][0].invoke(assortedRef), triangleFieldsNew);

		int[] pointFieldsNew = { xNew, yNew };
		checkEqualPoint2D(getterAndSetter[1][0].invoke(assortedRef), pointFieldsNew);

		int[][] lineFieldNew = { { stXNew, stYNew }, { enXNew, enYNew } };
		checkEqualLine2D(getterAndSetter[2][0].invoke(assortedRef), lineFieldNew, true);
		
		assertEquals(getInt.invoke(getterAndSetter[3][0].invoke(assortedRef)), iValueNew);
		assertEquals(getFloat.invoke(getterAndSetter[4][0].invoke(assortedRef)), fValueNew);
		
		int[][][] triangle2FieldsNew = {
				{ { tri2V1StXNew, tri2V1StYNew }, { tri2V1EnXNew, tri2V1EnYNew } },
				{ { tri2V2StXNew, tri2V2StYNew }, { tri2V2EnXNew, tri2V2EnYNew } },
				{ { tri2V3StXNew, tri2V3StYNew }, { tri2V3EnXNew, tri2V3EnYNew } } };

		checkEqualTriangle2D(getterAndSetter[5][0].invoke(assortedRef), triangle2FieldsNew);

		/*
		 * TODO:check object
		 */
		assertEquals(getFieldOffset(assortedRefClass, "tri"), 8);
		assertEquals(getFieldOffset(assortedRefClass, "point"), 56);
		assertEquals(getFieldOffset(assortedRefClass, "line"), 64);
		assertEquals(getFieldOffset(assortedRefClass, "i"), 80);
		assertEquals(getFieldOffset(assortedRefClass, "f"), 84);
		assertEquals(getFieldOffset(assortedRefClass, "tri2"), 88);
	}
	
	/*
	 * Create a large valueType with 16 valuetypes as its members
	 * Create a mega valueType with 16 large valuetypes as its members 
	 *
	 * value LargeObject {
	 *	flattened ValObject val1;
	 *	flattened ValObject val2;
	 *	...
	 *	flattened ValObject val16;
	 * }
	 *
	 * value MegaObject {
	 *	flattened LargeObject val1;
	 *	... 
	 *	flattened LargeObject val16;
	 * }
	 */
	@Test(priority = 2)
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
				"val16:QValueObject;:value" };

		largeObjectValueClass = ValueTypeGenerator.generateValueClass("LargeObject", largeFields);
		makeLargeValue = lookup.findStatic(largeObjectValueClass, "makeValueGeneric",
				MethodType.methodType(largeObjectValueClass, Object.class, Object.class, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class, Object.class));
		/*
		 * Getters are created in array getterAndWither[i][0] according to the order of fields i
		 * Withers are created in array getterAndWither[i][1] according to the order of fields i
		 */
		MethodHandle[][] getterAndWither = new MethodHandle[largeFields.length][2];
		for (int i = 0; i < largeFields.length; i++) {
			String field = (largeFields[i].split(":"))[0];
			getterAndWither[i][0] = generateGenericGetter(largeObjectValueClass, field);
			getterAndWither[i][1] = generateGenericWither(largeObjectValueClass, field);
		}
		
		/*
		 * create field objects
		 */
		Object[] originObjects = new Object[16];
		Object[] valueObjects = new Object[16];
		for (int i = 0; i < 16; i++) {
			originObjects[i] = (Object)(Integer.MIN_VALUE + i);
			valueObjects[i] = makeValueObject.invoke(originObjects[i]);
		}

		Object largeObject = makeLargeValue.invoke(valueObjects[0], valueObjects[1], valueObjects[2], valueObjects[3],
				valueObjects[4], valueObjects[5], valueObjects[6], valueObjects[7], valueObjects[8], valueObjects[9],
				valueObjects[10], valueObjects[11], valueObjects[12], valueObjects[13], valueObjects[14],
				valueObjects[15]);

		for (int i = 0; i < 16; i++) {
			assertEquals(getObject.invoke(getterAndWither[i][0].invoke(largeObject)), originObjects[i]);
		}

		//create new objects
		Object[] originObjectsNew = new Object[16];
		Object[] valueObjectsNew = new Object[16];
		for (int i = 0; i < 16; i++) {
			originObjectsNew[i] = (Object)(Integer.MAX_VALUE - i);
			valueObjectsNew[i] = makeValueObject.invoke(originObjectsNew[i]);
			largeObject = getterAndWither[i][1].invoke(largeObject, valueObjectsNew[i]);
			assertEquals(getObject.invoke(getterAndWither[i][0].invoke(largeObject)), originObjectsNew[i]);
		}

		long offset = 4;
		for (int i = 0; i < 16; i++) {
			String field = largeFields[i].split(":")[0];
			assertEquals(getFieldOffset(largeObjectValueClass, field), offset);
			assertEquals(myUnsafe.getObject(largeObject, offset), originObjectsNew[i]);
			offset = 4 + offset;
		}
		
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
				"val16:QLargeObject;:value" };
		Class megaObjectClass = ValueTypeGenerator.generateValueClass("MegaObject", megaFields);
		MethodHandle makeMega = lookup.findStatic(megaObjectClass, "makeValueGeneric",
				MethodType.methodType(megaObjectClass, Object.class, Object.class, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class, Object.class));
		
		/*
		 * Getters are created in array getterAndWither[i][0] according to the order of fields i
		 * Withers are created in array getterAndWither[i][1] according to the order of fields i
		 */
		MethodHandle[][] megaGetterAndWither = new MethodHandle[megaFields.length][2];
		for (int i = 0; i < megaFields.length; i++) {
			String field = (megaFields[i].split(":"))[0];
			megaGetterAndWither[i][0] = generateGenericGetter(megaObjectClass, field);
			megaGetterAndWither[i][1] = generateGenericWither(megaObjectClass, field);
		}
		
		/*
		 * create field objects
		 */
		Object[][] values = new Object[16][16];
		Object[] largeObjects = new Object[16];
		for (int i = 0; i < 16; i++) {
			Object[] valObjects = new Object[16];
			for (int j = 0; j < 16; j++) {
				values[i][j] = (Object)(Integer.MIN_VALUE + i * j);
				valObjects[j] = makeValueObject.invoke(values[i][j]);
			}
			largeObjects[i] = makeLargeValue.invoke(valObjects[0], valObjects[1], valObjects[2], valObjects[3],
					valObjects[4], valObjects[5], valObjects[6], valObjects[7], valObjects[8], valObjects[9],
					valObjects[10], valObjects[11], valObjects[12], valObjects[13], valObjects[14], valObjects[15]);
		}

		Object megaObject = makeMega.invoke(largeObjects[0], largeObjects[1], largeObjects[2], largeObjects[3],
				largeObjects[4], largeObjects[5], largeObjects[6], largeObjects[7], largeObjects[8], largeObjects[9],
				largeObjects[10], largeObjects[11], largeObjects[12], largeObjects[13], largeObjects[14],
				largeObjects[15]);

		for (int i = 0; i < megaFields.length; i++) {
			for(int j = 0; j < largeFields.length; j++) {
				Object objectFromGetter = getObject.invoke(getterAndWither[j][0].invoke(megaGetterAndWither[i][0].invoke(megaObject)));
				assertEquals(objectFromGetter, values[i][j]);
			}
		}
		Object[][] valuesNew = new Object[16][16];
		Object[] largeObjectsNew = new Object[16];
		for (int i = 0; i < 16; i++) {
			Object[] valObjects = new Object[16];
			for (int j = 0; j < 16; j++) {
				valuesNew[i][j] = (Object)(Integer.MAX_VALUE - i * j);
				valObjects[j] = makeValueObject.invoke(valuesNew[i][j]);
			}
			largeObjectsNew[i] = makeLargeValue.invoke(valObjects[0], valObjects[1], valObjects[2], valObjects[3],
					valObjects[4], valObjects[5], valObjects[6], valObjects[7], valObjects[8], valObjects[9],
					valObjects[10], valObjects[11], valObjects[12], valObjects[13], valObjects[14], valObjects[15]);
			megaObject = megaGetterAndWither[i][1].invoke(megaObject, largeObjectsNew[i]);
		}
		
		for (int i = 0; i < megaFields.length; i++) {
			for(int j = 0; j < largeFields.length; j++) {
				Object objectFromGetter = getObject.invoke(getterAndWither[j][0].invoke(megaGetterAndWither[i][0].invoke(megaObject)));
				assertEquals(objectFromGetter, valuesNew[i][j]);
			}
		}
		
		offset = 4L;
		for (int i = 0; i < 16; i++) {
			String field = megaFields[i].split(":")[0];
			assertEquals(getFieldOffset(megaObjectClass, field), offset);
			for (int j = 0; j < 16; j++) {
				assertEquals(myUnsafe.getObject(megaObject, offset), valuesNew[i][j]);
				offset = 4 + offset;
			}
		}
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
	@Test(priority = 3)
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
				"val16:QValueObject;:value" };

		Class largeRefClass = ValueTypeGenerator.generateRefClass("LargeRef", largeFields);
		MethodHandle makeLarge = lookup.findStatic(largeRefClass, "makeRefGeneric",
				MethodType.methodType(largeRefClass, Object.class, Object.class, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class, Object.class));
		/*
		for(int i = 1; i <= 16; i++) {
			System.out.println(getFieldOffset(largeRefClass, "val" + i));
		}*/
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

		/*
		 * field objects
		 */
		Object[] originObjects = new Object[16];
		Object[] valueObjects = new Object[16];
		for (int i = 0; i < 16; i++) {
			originObjects[i] = (Object)(Integer.MIN_VALUE + i);
			valueObjects[i] = makeValueObject.invoke(originObjects[i]);
		}

		Object largeObject = makeLarge.invoke(valueObjects[0], valueObjects[1], valueObjects[2], valueObjects[3],
				valueObjects[4], valueObjects[5], valueObjects[6], valueObjects[7], valueObjects[8], valueObjects[9],
				valueObjects[10], valueObjects[11], valueObjects[12], valueObjects[13], valueObjects[14],
				valueObjects[15]);

		for (int i = 0; i < 16; i++) {
			assertEquals(getObject.invoke(getterAndSetter[i][0].invoke(largeObject)), originObjects[i]);
		}
		
		/*Object testObject = (Object)(Integer.MAX_VALUE);
		assertEquals(myUnsafe.getObject(largeObject, 8L), originObjects[0]);
		Object valueTest = makeValueObject.invoke(testObject);
		getterAndSetter[0][1].invoke(largeObject, valueTest);
		assertEquals(getFieldOffset(largeRefClass, "val1"), 8);
		if(largeObject == null) {
			System.out.println("g");
		}
		//if(getterAndSetter[0][0].invoke(largeObject) == null) {
		//	System.out.println("ss");
		//}
		assertEquals(getObject.invoke(getterAndSetter[0][0].invoke(largeObject)), testObject);
		assertEquals(myUnsafe.getObject(largeObject, 8L), originObjects[0]);
	//	assertEquals(myUnsafe.getObject(largeObject, 8), testObject);
	//	assertEquals(getObject.invoke(getterAndSetter[0][0].invoke(largeObject)), testObject);	
		/*
		 * create new field objects
		 */
		Object[] originObjectsNew = new Object[16];
		Object[] valueObjectsNew = new Object[16];
		for (int i = 0; i < 16; i++) {
			originObjectsNew[i] = (Object)(Integer.MAX_VALUE - i);
			valueObjectsNew[i] = makeValueObject.invoke(originObjectsNew[i]);
			getterAndSetter[i][1].invoke(largeObject, valueObjectsNew[i]);
			
			assertEquals(getObject.invoke(getterAndSetter[i][0].invoke(largeObject)), originObjectsNew[i]);
		}
		
		long offset = 8;
		for (int i = 0; i < 16; i++) {
			String field = largeFields[i].split(":")[0];
			assertEquals(getFieldOffset(largeRefClass, field), offset);
			assertEquals(myUnsafe.getObject(largeObject, offset), originObjectsNew[i]);
			offset = 4 + offset;
		}
		
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
				"val16:QLargeObject;:value" };
		Class megaObjectClass = ValueTypeGenerator.generateRefClass("MegaRef", megaFields);
		MethodHandle makeMega = lookup.findStatic(megaObjectClass, "makeRefGeneric",
				MethodType.methodType(megaObjectClass, Object.class, Object.class, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class, Object.class,
						Object.class, Object.class, Object.class, Object.class, Object.class, Object.class));
		
		MethodHandle[][] getterAndWither = new MethodHandle[largeFields.length][2];
		for (int i = 0; i < largeFields.length; i++) {
			String field = (largeFields[i].split(":"))[0];
			getterAndWither[i][0] = generateGenericGetter(largeObjectValueClass, field);
			getterAndWither[i][1] = generateGenericWither(largeObjectValueClass, field);
		}
		
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

		/*
		 * create field objects
		 */
		Object[][] values = new Object[16][16];
		Object[] largeObjects = new Object[16];
		for (int i = 0; i < 16; i++) {
			Object[] valObjects = new Object[16];
			for (int j = 0; j < 16; j++) {
				values[i][j] = (Object)(Integer.MIN_VALUE + i * j);
				valObjects[j] = makeValueObject.invoke(values[i][j]);
			}
			largeObjects[i] = makeLargeValue.invoke(valObjects[0], valObjects[1], valObjects[2], valObjects[3],
					valObjects[4], valObjects[5], valObjects[6], valObjects[7], valObjects[8], valObjects[9],
					valObjects[10], valObjects[11], valObjects[12], valObjects[13], valObjects[14], valObjects[15]);
		}

		Object megaObject = makeMega.invoke(largeObjects[0], largeObjects[1], largeObjects[2], largeObjects[3],
				largeObjects[4], largeObjects[5], largeObjects[6], largeObjects[7], largeObjects[8], largeObjects[9],
				largeObjects[10], largeObjects[11], largeObjects[12], largeObjects[13], largeObjects[14],
				largeObjects[15]);

		for (int i = 0; i < megaFields.length; i++) {
			for(int j = 0; j < largeFields.length; j++) {
				Object objectFromGetter = getObject.invoke(getterAndWither[j][0].invoke(megaGetterAndSetter[i][0].invoke(megaObject)));
				assertEquals(objectFromGetter, values[i][j]);
			}
		}
		Object[][] valuesNew = new Object[16][16];
		Object[] largeObjectsNew = new Object[16];
		for (int i = 0; i < 16; i++) {
			Object[] valObjects = new Object[16];
			for (int j = 0; j < 16; j++) {
				valuesNew[i][j] = (Object)(Integer.MAX_VALUE - i * j);
				valObjects[j] = makeValueObject.invoke(valuesNew[i][j]);
			}
			largeObjectsNew[i] = makeLargeValue.invoke(valObjects[0], valObjects[1], valObjects[2], valObjects[3],
					valObjects[4], valObjects[5], valObjects[6], valObjects[7], valObjects[8], valObjects[9],
					valObjects[10], valObjects[11], valObjects[12], valObjects[13], valObjects[14], valObjects[15]);
			megaGetterAndSetter[i][1].invoke(megaObject, largeObjectsNew[i]);
		}
		
		for (int i = 0; i < megaFields.length; i++) {
			for(int j = 0; j < largeFields.length; j++) {
				Object objectFromGetter = getObject.invoke(getterAndWither[j][0].invoke(megaGetterAndSetter[i][0].invoke(megaObject)));
				assertEquals(objectFromGetter, valuesNew[i][j]);
			}
		}
		
		offset = 8L;
		for (int i = 0; i < 16; i++) {
			String field = megaFields[i].split(":")[0];
			assertEquals(getFieldOffset(megaObjectClass, field), offset);
			for (int j = 0; j < 16; j++) {
				assertEquals(myUnsafe.getObject(megaObject, offset), valuesNew[i][j]);
				offset = 4 + offset;
			}
		}
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

	static Unsafe createUnsafeInstance() {
		try {
			Field[] staticFields = Unsafe.class.getDeclaredFields();
			for (Field field : staticFields) {
				if (field.getType() == Unsafe.class) {
					field.setAccessible(true);
					return (Unsafe)field.get(Unsafe.class);
					}
				}
		} catch(IllegalAccessException e) {
		}
		throw new Error("Unable to find an instance of Unsafe");
	}
	
	static long getFieldOffset(Class clazz, String field) {
		try {
			Field f = clazz.getDeclaredField(field);
			return myUnsafe.objectFieldOffset(f);
		} catch (Throwable t) {
			throw new RuntimeException(t);
		}
	}
	

	static void checkEqualPoint2D(Object point, int[] positions) throws Throwable {
		if(point == null) {
			Assert.fail("Point Obejct is null!");
		}
		assertEquals(getX.invoke(point), positions[0]);
		assertEquals(getY.invoke(point), positions[1]);
	}

	static void checkEqualLine2D(Object line, int[][] positions, boolean flatten) throws Throwable {
		if(line == null) {
			throw new Error("Line2D Obejct is null!");
		}
		if(flatten == true) {
			checkEqualPoint2D(getStFlat.invoke(line), positions[0]);
			checkEqualPoint2D(getEnFlat.invoke(line), positions[1]);
		} else {
			checkEqualPoint2D(getSt.invoke(line), positions[0]);
			checkEqualPoint2D(getEn.invoke(line), positions[1]);
		}
	}

	static void checkEqualTriangle2D(Object triangle, int[][][] positions) throws Throwable {
		if(triangle == null) {
			throw new Error("Triangle Object is null!");
		}
		checkEqualLine2D(getV1.invoke(triangle), positions[0], true);
		checkEqualLine2D(getV2.invoke(triangle), positions[1], true);		
		checkEqualLine2D(getV3.invoke(triangle), positions[2], true);		
	}

}
