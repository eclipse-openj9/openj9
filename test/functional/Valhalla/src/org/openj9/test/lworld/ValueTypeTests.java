/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
 * 8) make -f run_configure.mk && make compile && make _sanity
 */

@Test(groups = { "level.sanity" })
public class ValueTypeTests {
	static Lookup lookup = MethodHandles.lookup();
	static Class point2DClass = null;
	static Class line2DClass = null;
	static MethodHandle makePoint2D = null;
	static MethodHandle makeLine2D = null;
	static MethodHandle getX = null;
	static MethodHandle getY = null;

	
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
		MethodHandle setX = generateSetter(point2DClass, "x", int.class);
		getY = generateGetter(point2DClass, "y", int.class);
		MethodHandle setY = generateSetter(point2DClass, "y", int.class);

		int x = 0xFFEEFFEE;
		int y = 0xAABBAABB;
		int xNew = 0x11223344;
		int yNew = 0x99887766;
		
		Object point2D = makePoint2D.invoke(x, y);
		
		assertEquals(getX.invoke(point2D), x);
		assertEquals(getY.invoke(point2D), y);
		
		setX.invoke(point2D, xNew);
		setY.invoke(point2D, yNew);
		
		assertEquals(getX.invoke(point2D), xNew);
		assertEquals(getY.invoke(point2D), yNew);
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
 		MethodHandle setSt = generateSetter(line2DClass, "st", point2DClass);
 		MethodHandle getEn = generateGetter(line2DClass, "en", point2DClass);
 		MethodHandle setEn = generateSetter(line2DClass, "en", point2DClass);
 		
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
		
		setSt.invoke(line2D, stNew);
		setEn.invoke(line2D, enNew);
		
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
		Class flattenedLine2DClass = ValueTypeGenerator.generateValueClass("FlattenedLine2D", fields);
				
		MethodHandle makeFlattenedLine2D = lookup.findStatic(flattenedLine2DClass, "makeValueGeneric", MethodType.methodType(flattenedLine2DClass, Object.class, Object.class));
		
		
		/*
 		TODO need q signature support to do anything else with FalttenedLine2D
		
		MethodHandle getSt = generateGenericGetter(flattenedLine2DClass, "st");
 		MethodHandle setSt = generateGenericSetter(flattenedLine2DClass, "st");
 		MethodHandle getEn = generateGenericGetter(flattenedLine2DClass, "en");
 		MethodHandle setEn = generateGenericSetter(flattenedLine2DClass, "en");
 		
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
		
		assertEquals(getX.invoke(getSt.invoke(line2D)), x);
		assertEquals(getY.invoke(getSt.invoke(line2D)), y);
		assertEquals(getX.invoke(getEn.invoke(line2D)), x2);
		assertEquals(getY.invoke(getEn.invoke(line2D)), y2);
		
		Object stNew = makePoint2D.invoke(xNew, yNew);
		Object enNew = makePoint2D.invoke(x2New, y2New);
				
		setSt.invoke(line2D, stNew);
		setEn.invoke(line2D, enNew);
		
		assertEquals(getX.invoke(getSt.invoke(line2D)), xNew);
		assertEquals(getY.invoke(getSt.invoke(line2D)), yNew);
		assertEquals(getX.invoke(getEn.invoke(line2D)), x2New);
		assertEquals(getY.invoke(getEn.invoke(line2D)), y2New);
		
		*/

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

}
