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
package org.openj9.test.booleans;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.reflect.Field;
import org.testng.Assert;
import static org.testng.Assert.*;
import org.testng.annotations.Test;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.AfterClass;
import java.util.*;
import java.io.*;

import sun.misc.Unsafe;

@Test(groups = { "level.sanity" })
public class BooleanTest {
	
	static native boolean setUp(Class<RuntimeException> clazz);
	static native boolean tearDown();
	static native void putInstanceBoolean(Class<?> clazz, Object instance, Field field, int value);
	static native void putStaticBoolean(Class<?> clazz, Field field, int value);
	static native int getInstanceBoolean(Class<?> clazz, Object instance, Field field);
	static native int getStaticBoolean(Class<?> clazz, Field field);
	public static native boolean returnBooleanValue(int val);
	
	public static class TestBool {
		public boolean z;
		public static boolean zStatic;
	}
	
	static Unsafe unsafe;
	static Class testBoolTester;
	static Field f;
	static Field fStatic;
	static long zOffset;
	
	static boolean debug = false;
	
	@BeforeClass
	public static void setUpTest() {
		System.loadLibrary("j9ben");
		
		try {
			Field f = sun.misc.Unsafe.class.getDeclaredField("theUnsafe");
			f.setAccessible(true);
			unsafe = (sun.misc.Unsafe) f.get(null);
		} catch (NoSuchFieldException | SecurityException | IllegalArgumentException | IllegalAccessException e) {
			fail("Failed to accessm Unsafe\n\n" + e.getMessage());
		}	
		
		if (!setUp(RuntimeException.class)) {
			fail("Failed to load natives");
		}
		try {
			byte[] classBytes = BooleanTestGenerator.generateTestBoolTester();
			
			if (debug) {
				FileOutputStream fos = new FileOutputStream( "TestBoolTester.class" );
				fos.write(classBytes);
				fos.flush();
				fos.close();
			}
			
			testBoolTester = BooleanTestGenerator.generateClass("TestBoolTester", classBytes);
			f = TestBool.class.getDeclaredField("z");
			fStatic = TestBool.class.getDeclaredField("zStatic");
			zOffset = unsafe.objectFieldOffset(f);
		} catch (Throwable t) {
			fail("Failed to generate test class\n\n" + t.getMessage() + "\n\n" + t.getCause().getMessage());
		}
	}
	
	@AfterClass
	public static void tearDownTest() {
		tearDown();
	}
	
	@Test
	public static void testJNISetAndGetBooleanField() {
		TestBool instance = new TestBool();
		testJNIInstanceBoolean(instance, 0xFF, 1);
		testJNIInstanceBoolean(instance, 0x08, 0);
		testJNIInstanceBoolean(instance, 0x00, 0);
	}
	
	@Test
	public static void testJNISetAndGetBooleanStaticField() {
		TestBool instance = new TestBool();
		testJNIStaticBoolean(0xFF, 1);
		testJNIStaticBoolean(0x08, 0);
		testJNIStaticBoolean(0x00, 0);
	}
	
	@Test
	public static void testJNINativeBoolReturn() {
		testJNINativeBoolReturn(0xFF, 1);
		testJNINativeBoolReturn(0x08, 1);
		testJNINativeBoolReturn(0x00, 0);
	}
	
	@Test
	public static void testJNINativeBoolReturnFromGeneratedByteCodes() throws Throwable {
		testJNINativeBoolReturnWithASM(testBoolTester, 0xFF, 1);
		testJNINativeBoolReturnWithASM(testBoolTester, 0x08, 1);
		testJNINativeBoolReturnWithASM(testBoolTester, 0x00, 0);
	}
	
	@Test
	public static void testUnsafeGetAndPutBoolean() {
		testUnsafeGetAndPut(0xFF, 1);
		testUnsafeGetAndPut(0x08, 1);
		testUnsafeGetAndPut(0x00, 0);
	}
	
	@Test
	public static void testUnsafeGetAndPutBooleanOnObject() {
		TestBool instance = new TestBool();
		testUnsafeGetAndPutBoolOnObject(instance, zOffset, 0xFF, 1);
		testUnsafeGetAndPutBoolOnObject(instance, zOffset, 0x08, 1);
		testUnsafeGetAndPutBoolOnObject(instance, zOffset, 0x00, 0);
	}
	
	@Test
	public static void testUnsafeGet() throws Throwable {
		testUnsafeGet(testBoolTester, 0xFF, 1);
		testUnsafeGet(testBoolTester, 0x08, 1);
		testUnsafeGet(testBoolTester, 0x00, 0);
	}
	
	@Test
	public static void testUnsafePut() throws Throwable {
		testUnsafePut(testBoolTester, 0xFF, 1);
		testUnsafePut(testBoolTester, 0x08, 1);
		testUnsafePut(testBoolTester, 0x00, 0);
	}
	
	@Test
	public static void testPutfieldBytecode() throws Throwable {
		TestBool instance = new TestBool();
		testBytecodeInstanceGetAndPutfield(testBoolTester, instance, 0xFF, 1);
		testBytecodeInstanceGetAndPutfield(testBoolTester, instance, 0x08, 0);
		testBytecodeInstanceGetAndPutfield(testBoolTester, instance, 0x00, 0);
	}
	
	@Test
	public static void testPutStaticfieldBytecode() throws Throwable {
		testBytecodeStaticGetAndPutfield(testBoolTester, 0xFF, 1);
		testBytecodeStaticGetAndPutfield(testBoolTester, 0x08, 0);
		testBytecodeStaticGetAndPutfield(testBoolTester, 0x00, 0);
	}
	
	@Test
	public static void testReturnBooleanBytecode()  throws Throwable{
		testReturnBoolean(testBoolTester, 0xFF, true);
		testReturnBoolean(testBoolTester, 0x08, false);
		testReturnBoolean(testBoolTester, 0x00, false);
	}
	
	/*
	 * Sets and gets an instance boolean field via the [Get|Set]BooleanField JNI functions.
	 * 
	 * This test uses native functions `/runtime/tests/jni/booleantest.c` to excercise the 
	 * JNI method behaviour.
	 * 
	 * This is done to test boolean trunction of JNI field access methods
	 */
	static void testJNIInstanceBoolean(TestBool instance, int value, int expected) {
		putInstanceBoolean(TestBool.class, instance, f, value); 
		int res = getInstanceBoolean(TestBool.class, instance, f);	
		assertEquals(res, expected);
	}
	
	/*
	 * Sets and gets an static boolean field via the [Get|Set]StaticBooleanField JNI functions.
	 * 
	 * This test uses native functions `/runtime/tests/jni/booleantest.c` to excercise the 
	 * JNI method behaviour.
	 * 
	 * This is done to test boolean trunction of JNI field access methods
	 */
	static void testJNIStaticBoolean(int value, int expected) {
		TestBool.zStatic = false;
		putStaticBoolean(TestBool.class, fStatic, value); 
		int res = getStaticBoolean(TestBool.class, fStatic);
		assertEquals(res, expected);
	}
	
	/*
	 * Returns a boolean value from a JNI function.
	 * 
	 * This test calls a JNI native passing in an integer parameter
	 * which is returned as a boolean.
	 * 
	 * This is done to test boolean truncation of JNI returns.
	 */
	static void testJNINativeBoolReturn(int value, int expected) {
		long addr = unsafe.allocateMemory(8);
		
		/* call native passing native and return value as a boolean */
		boolean bRes = returnBooleanValue(value);
		
		/* In order to view boolean in its raw form, write it out to memory and read it as an int */
		
		/* memset the memory to zero */
		unsafe.putLong(addr, 0L);
		
		/* read boolean and compare result */
		unsafe.putBoolean(null, addr, bRes);
		int res = unsafe.getInt(null, addr);
		assertEquals(res, expected);
		unsafe.freeMemory(addr);
	}
	
	/*
	 * Returns a boolean value from a JNI function.
	 * 
	 * This test calls a JNI native `returnBooleanValue` via generated bytecodes. The bytecodes treat the value as
	 * an int, only the JNI function returns the value as a bool.
	 * 
	 * This is done to test boolean truncation of JNI returns without the use of unsafe.
	 */
	static void testJNINativeBoolReturnWithASM(Class testBoolGen, int value, int expected) throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findStatic(testBoolGen, "callReturnBooleanValue", MethodType.methodType(int.class, int.class));
		int res = (int) mh.invokeExact(value);
		assertEquals(res, expected);
	}

	/* 
	 * Reads and writes a boolean value value to memory via Unsafe 
	 * 
	 * This test write a value as an int to memory, then reads it as
	 * a boolean. The boolean is then written back into memory and then read back as an int.
	 * 
	 * This is done to test truncation of unsafe.put
	 */
	static void testUnsafeGetAndPut(int value, int expected) {
		long addr = unsafe.allocateMemory(16);
		
		/* memset the memory to zero */
		unsafe.putLong(addr, 0L);
		unsafe.putLong(addr + 8, 0L);
		
		/* write the value and read it as bool */
		unsafe.putInt(addr, value);
		boolean bVal = unsafe.getBoolean(null, addr);
		
		/* write the bool value and compare the result */
		unsafe.putBoolean(null, addr + 8, bVal);
		int res = unsafe.getInt(null, addr + 8);
		assertEquals(res, expected);
		
		unsafe.freeMemory(addr);
	}
	
	/* 
	 * Reads and writes a boolean value value to an object via Unsafe 
	 * 
	 * This test write a value as an int to memory, then reads it as
	 * a boolean. The boolean is then written back into an object and then read back as an int.
	 * 
	 * This is done to test truncation of unsafe.put in objects
	 */
	static void testUnsafeGetAndPutBoolOnObject(TestBool instance, long zOffset, int value, int expected) {
		long addr = unsafe.allocateMemory(8);
		
		/* memset the values */
		instance.z = false;
		unsafe.putLong(addr, 0L);
		
		/* write a boolean to memory and read it back as a bool */
		unsafe.putInt(addr, value);
		boolean bVal = unsafe.getBoolean(null, addr);
		
		/* write the bool value and compare the result */
		unsafe.putBoolean(instance, zOffset, bVal);
		int res = unsafe.getInt(instance, zOffset);
		assertEquals(res, expected);
		
		unsafe.freeMemory(addr);
	}
	
	/*
	 * Returns a boolean value from a Java method.
	 * 
	 * This test calls a generate java method that returns a boolean. The bytecodes treat the value as
	 * an int, no conversions are done. Only the method signature indicates that a boolean is being returned
	 * 
	 * This is done to test boolean returns in a java method.
	 */
	static void testReturnBoolean(Class testBoolTester, int value, boolean expected) throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findStatic(testBoolTester, "returnBoolean", MethodType.methodType(boolean.class, int.class));
		boolean res =  (boolean) mh.invokeExact(value);
		assertEquals(res, expected);
	}
	
	/*
	 * Reads a boolean value from memory
	 * 
	 * This test calls a generated function that calls unsafe.getBoolean and returns the value as an int
	 * 
	 * This is to test truncation in Unsafe.getBoolean without using any other unsafe boolean functions
	 */
	static void testUnsafeGet(Class testBoolTester, int value, int expected) throws Throwable {
		long addr = unsafe.allocateMemory(8);
		
		/* memset the memory */
		unsafe.putLong(addr, 0L);
		
		unsafe.putLong(addr, value);
		MethodHandle mh = MethodHandles.lookup().findStatic(testBoolTester, "testSMUnsafeBooleanGet", MethodType.methodType(int.class, sun.misc.Unsafe.class, long.class));
		int res =  (int) mh.invokeExact(unsafe, addr);
		assertEquals(res, expected);
		
		unsafe.freeMemory(addr);
	}
	
	/*
	 * Writes a boolean value to memory
	 * 
	 * This test calls a generated function that calls unsafe.putBoolean
	 * 
	 * This is to test truncation in Unsafe.putBoolean without using any other unsafe boolean functions
	 */
	static void testUnsafePut(Class testBoolTester, int value, int expected) throws Throwable {
		long addr = unsafe.allocateMemory(8);
		
		/* memset the memory */
		unsafe.putLong(addr, 0L);
		
		
		MethodHandle mh = MethodHandles.lookup().findStatic(testBoolTester, "testSMUnsafeBooleanPut", MethodType.methodType(void.class, sun.misc.Unsafe.class, long.class, int.class));
		mh.invokeExact(unsafe, addr, value);
		long res = unsafe.getInt(null, addr);
		assertEquals(res, expected);
		
		unsafe.freeMemory(addr);
	}

	/*
	 * Read and writes a boolean with java bytecodes
	 * 
	 * Calls a generated function that takes a integer value and writes it as a boolean using putfield.
	 * It then reads the value back with getfield.
	 * 
	 * This is done to test the behaviour of the putfield bytecode
	 */
	static void testBytecodeInstanceGetAndPutfield(Class testBoolTester, TestBool instance, int value, int expected) throws Throwable {
		/* initialize bool */
		instance.z = false;
		
		/* call getAndPutfield on bool instance field and compare the result */ 
		MethodHandle mh = MethodHandles.lookup().findStatic(testBoolTester, "callSetBool", MethodType.methodType(int.class, TestBool.class, int.class));
		int res = (int) mh.invokeExact(instance, value);
		assertEquals(res, expected);
	}
	
	/*
	 * Read and writes a boolean with java bytecodes
	 * 
	 * Calls a generated function that takes a integer value and writes it as a boolean using putStaticfield.
	 * It then reads the value back with getStaticfield.
	 * 
	 * This is done to test the behaviour of the putStaticfield bytecode
	 */
	static void testBytecodeStaticGetAndPutfield(Class testBoolTester, int value, int expected) throws Throwable {
		/* initialize bool */
		TestBool.zStatic = false;
		
		/* call getAndPutStatic field on bool instance field and compare the result */ 
		MethodHandle mh = MethodHandles.lookup().findStatic(testBoolTester, "callSetBoolStatic", MethodType.methodType(int.class, int.class));
		int res = (int) mh.invokeExact(value);
		assertEquals(res, expected);
	}
	
}
