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
package com.ibm.j9.jsr292.api;

import org.testng.annotations.Test;
import org.testng.Assert;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.invoke.MethodHandles.Lookup;
import static java.lang.invoke.MethodHandles.lookup;

public class MethodHandleAPI_countedLoop {
	final static Lookup lookup = lookup();
	
	static MethodHandle COUNTEDLOOP_IDENTITY_INT_TYPE;
	static MethodHandle COUNTEDLOOP_IDENTITY_BYTE_TYPE;
	static MethodHandle COUNTEDLOOP_LOOP_INIT_5;
	static MethodHandle COUNTEDLOOP_LOOP_COUNT_20;
	static {
		COUNTEDLOOP_IDENTITY_INT_TYPE = MethodHandles.identity(int.class);
		COUNTEDLOOP_IDENTITY_BYTE_TYPE = MethodHandles.identity(byte.class);
		COUNTEDLOOP_LOOP_INIT_5 = MethodHandles.constant(int.class, 5);
		COUNTEDLOOP_LOOP_COUNT_20 = MethodHandles.constant(int.class, 20);
	}
	
	/* The test cases include countedLoop with the following different signatures:
	 * 1) countedLoop(MethodHandle startHandle, MethodHandle endHandle, MethodHandle initHandle, MethodHandle bodyHandle)
	 * 2) countedLoop(MethodHandle loopCountHandle, MethodHandle initHandle, MethodHandle bodyHandle)
	 */
	
	/* Test cases for countedLoop 1) */
	
	/**
	 * countedLoop test for a null start handle.
	 */
	@Test(expectedExceptions = NullPointerException.class, groups = { "level.extended" })
	public static void test_countedLoop_NullStart() {
		MethodHandle mhEnd = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhInit = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhBody = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(null, mhEnd, mhInit, mhBody);
		Assert.fail("The test case failed to detect a null start handle");
	}
	
	/**
	 * countedLoop test for a null end handle.
	 */
	@Test(expectedExceptions = NullPointerException.class, groups = { "level.extended" })
	public static void test_countedLoop_NullEnd() {
		MethodHandle mhStart = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhInit = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhBody = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhStart, null, mhInit, mhBody);
		Assert.fail("The test case failed to detect a null end handle");
	}
	
	/**
	 * countedLoop test for a null init handle.
	 * Note:
	 * The logic of loop is listed as follows:
	 * int sum = 0 (by default when init is null);
	 * for (int i = 5(start); i < 20(end); i++) {
	 *     sum = sum + 1;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_countedLoop_NullInit() throws Throwable {
		MethodHandle mhStart = COUNTEDLOOP_LOOP_INIT_5;
		MethodHandle mhEnd = COUNTEDLOOP_LOOP_COUNT_20;
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_NullInit",  MethodType.methodType(int.class, int.class, int.class));
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhStart, mhEnd, null, mhBody);
		Assert.assertEquals((int)15, (int)mhCountedLoop.invokeExact());
	}
	
	/**
	 * countedLoop test for a null loop body.
	 */
	@Test(expectedExceptions = NullPointerException.class, groups = { "level.extended" })
	public static void test_countedLoop_NullBody() {
		MethodHandle mhStart = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhEnd = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhInit = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhStart, mhEnd, mhInit, null);
		Assert.fail("The test case failed to detect a null loop body");
	}
	
	/**
	 * countedLoop test for the invalid return type of the end handle which is inconsistent with the start handle
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_countedLoop_InvalidReturnType_End() {
		MethodHandle mhStart = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhEnd = COUNTEDLOOP_IDENTITY_BYTE_TYPE;
		MethodHandle mhInit = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhBody = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhStart, mhEnd, mhInit, mhBody);
		Assert.fail("The test case failed to detect that the return type of the end handle is inconsistent with the start handle");
	}
	
	/**
	 * countedLoop test for the body handle with invalid parameter types and the void return type.
	 * Note: if the return type of the body handle is void, its parameter list should be
	 * (I, A...) in which the first parameter of the body handle should be int type.
	 * @throws Throwable
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_countedLoop_InvalidParamType_VoidReturnType_Body() throws Throwable {
		MethodHandle mhStart = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhEnd = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhInit = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_InvalidParamType_VoidReturnType", MethodType.methodType(void.class, char.class));
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhStart, mhEnd, mhInit, mhBody);
		Assert.fail("The test case failed to detect that the first parameter type of the body handle (void return type) is not int");
	}
	
	/**
	 * countedLoop test for the body handle with the void return type.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_countedLoop_VoidReturnType_Body() throws Throwable {
		MethodHandle mhStart = COUNTEDLOOP_LOOP_INIT_5;
		MethodHandle mhEnd = COUNTEDLOOP_LOOP_COUNT_20;
		MethodHandle mhInit = MethodHandles.zero(void.class);
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_VoidReturnType", MethodType.methodType(void.class, int.class));
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhStart, mhEnd, mhInit, mhBody);
		mhCountedLoop.invokeExact();
	}
	
	/**
	 * countedLoop test for the body handle with an int return type.
	 * Note: the first parameter type of the body handle should be consistent with the return type.
	 * @throws Throwable
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_countedLoop_InvalidLeadingParamType_IntReturnType_Body() throws Throwable {
		MethodHandle mhStart = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhEnd = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhInit = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_IntReturnType", MethodType.methodType(int.class, byte.class));
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhStart, mhEnd, mhInit, mhBody);
		Assert.fail("The test case failed to detect that the first parameter type of the body handle is inconsistent with the return type");
	}
	
	/**
	 * countedLoop test for the body handle with an int return type plus only one parameter.
	 * Note: the second parameter (int) must exist for the body handle with the non-void return type.
	 * @throws Throwable
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_countedLoop_MissingIntParamType_IntReturnType_Body() throws Throwable {
		MethodHandle mhStart = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhEnd = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhInit = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_IntReturnType", MethodType.methodType(int.class, int.class));
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhStart, mhEnd, mhInit, mhBody);
		Assert.fail("The test case failed to detect that the second parameter of the body handle doesn't exist");
	}
	
	/**
	 * countedLoop test for the body handle with an int return type and two parameters.
	 * Note: the second parameter type must be int for the body handle with the non-void return type.
	 * @throws Throwable
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_countedLoop_Invalid2ndParamType_IntReturnType_Body() throws Throwable {
		MethodHandle mhStart = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhEnd = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhInit = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_IntReturnType", MethodType.methodType(int.class, int.class, byte.class));
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhStart, mhEnd, mhInit, mhBody);
		Assert.fail("The test case failed to detect the second parameter type of the body handle (non-void return type) is not int");
	}
	
	/**
	 * countedLoop test for the body handle with byte parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * byte sum = (byte)1;
	 * for (int i = 5(start); i < 20(end); i++) {
	 *     sum = sum + 1;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_countedLoop_ByteType() throws Throwable {
		MethodHandle mhStart = COUNTEDLOOP_LOOP_INIT_5;
		MethodHandle mhEnd = COUNTEDLOOP_LOOP_COUNT_20;
		MethodHandle mhInit = COUNTEDLOOP_IDENTITY_BYTE_TYPE;
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_ByteType",  MethodType.methodType(byte.class, byte.class, int.class, byte.class));
		
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhStart, mhEnd, mhInit, mhBody);
		Assert.assertEquals((byte)16, (byte)mhCountedLoop.invokeExact((byte)1));
	}
	
	/**
	 * countedLoop test for the body handle with char parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * char sum = 'A';
	 * for (int i = 5(start); i < 20(end); i++) {
	 *     sum = sum + 1;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_countedLoop_CharType() throws Throwable {
		MethodHandle mhStart = COUNTEDLOOP_LOOP_INIT_5;
		MethodHandle mhEnd = COUNTEDLOOP_LOOP_COUNT_20;
		MethodHandle mhInit = MethodHandles.identity(char.class);
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_CharType",  MethodType.methodType(char.class, char.class, int.class, char.class));
		
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhStart, mhEnd, mhInit, mhBody);
		Assert.assertEquals('P', (char)mhCountedLoop.invokeExact('A'));
	}
	
	/**
	 * countedLoop test for the body handle with short parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * short sum = (short)1;
	 * for (int i = 5(start); i < 20(end); i++) {
	 *     sum = sum + 1;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_countedLoop_ShortType() throws Throwable {
		MethodHandle mhStart = COUNTEDLOOP_LOOP_INIT_5;
		MethodHandle mhEnd = COUNTEDLOOP_LOOP_COUNT_20;
		MethodHandle mhInit = MethodHandles.identity(short.class);
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_ShortType",  MethodType.methodType(short.class, short.class, int.class, short.class));
		
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhStart, mhEnd, mhInit, mhBody);
		Assert.assertEquals((short)16, (short)mhCountedLoop.invokeExact((short)1));
	}
	
	/**
	 * countedLoop test for the body handle with int parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * int sum = (int)1;
	 * for (int i = 5(start); i < 20(end); i++) {
	 *     sum = sum + 1;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_countedLoop_IntType() throws Throwable {
		MethodHandle mhStart = COUNTEDLOOP_LOOP_INIT_5;
		MethodHandle mhEnd = COUNTEDLOOP_LOOP_COUNT_20;
		MethodHandle mhInit = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_IntType",  MethodType.methodType(int.class, int.class, int.class, int.class));
		
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhStart, mhEnd, mhInit, mhBody);
		Assert.assertEquals((int)16, (int)mhCountedLoop.invokeExact((int)1));
	}
	
	/**
	 * countedLoop test for the body handle with long parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * long sum = (long)1;
	 * for (int i = 5(start); i < 20(end); i++) {
	 *     sum = sum + 1;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_countedLoop_LongType() throws Throwable {
		MethodHandle mhStart = COUNTEDLOOP_LOOP_INIT_5;
		MethodHandle mhEnd = COUNTEDLOOP_LOOP_COUNT_20;
		MethodHandle mhInit = MethodHandles.identity(long.class);
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_LongType",  MethodType.methodType(long.class, long.class, int.class, long.class));
		
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhStart, mhEnd, mhInit, mhBody);
		Assert.assertEquals(16l, (long)mhCountedLoop.invokeExact(1l));
	}
	
	/**
	 * countedLoop test for the body handle with float parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * float sum = (float)1;
	 * for (int i = 5(start); i < 20(end); i++) {
	 *     sum = sum + 0.5;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_countedLoop_FloatType() throws Throwable {
		MethodHandle mhStart = COUNTEDLOOP_LOOP_INIT_5;
		MethodHandle mhEnd = COUNTEDLOOP_LOOP_COUNT_20;
		MethodHandle mhInit = MethodHandles.identity(float.class);
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_FloatType",  MethodType.methodType(float.class, float.class, int.class, float.class));
		
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhStart, mhEnd, mhInit, mhBody);
		Assert.assertEquals(8.5f, (float)mhCountedLoop.invokeExact(1f));
	}
	
	/**
	 * countedLoop test for the body handle with double parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * double sum = (double)1;
	 * for (int i = 5(start); i < 20(end); i++) {
	 *     sum = sum + 0.5;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_countedLoop_DoubleType() throws Throwable {
		MethodHandle mhStart = COUNTEDLOOP_LOOP_INIT_5;
		MethodHandle mhEnd = COUNTEDLOOP_LOOP_COUNT_20;
		MethodHandle mhInit = MethodHandles.identity(double.class);
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_DoubleType",  MethodType.methodType(double.class, double.class, int.class, double.class));
		
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhStart, mhEnd, mhInit, mhBody);
		Assert.assertEquals(8.5d, (double)mhCountedLoop.invokeExact(1d));
	}
	
	/**
	 * countedLoop test for the body handle with String parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * String sum = "str";
	 * for (int i = 5(start); i < 10(end); i++) {
	 *     sum = sum + " " + i;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_countedLoop_StringType() throws Throwable {
		MethodHandle mhStart = COUNTEDLOOP_LOOP_INIT_5;
		MethodHandle mhEnd = MethodHandles.constant(int.class, 10);
		MethodHandle mhInit = MethodHandles.identity(String.class);
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_StringType",  MethodType.methodType(String.class, String.class, int.class, String.class));
		
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhStart, mhEnd, mhInit, mhBody);
		Assert.assertEquals("str 5 6 7 8 9", (String)mhCountedLoop.invokeExact("str"));
	}
	
	/* Test cases for countedLoop 2) */
	
	/**
	 * countedLoop test (LoopCount) for a null loopCount handle.
	 */
	@Test(expectedExceptions = NullPointerException.class, groups = { "level.extended" })
	public static void test_countedLoop_LoopCount_NullCount() {
		MethodHandle mhInit = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhBody = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(null, mhInit, mhBody);
		Assert.fail("The test case failed to detect a null loopCount handle");
	}
	
	/**
	 * countedLoop test (LoopCount) for the invalid return type of the loopCount handle
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_countedLoop_LoopCount_InvalidReturnType_Count() {
		MethodHandle mhLoopCount = COUNTEDLOOP_IDENTITY_BYTE_TYPE;
		MethodHandle mhInit = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhBody = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhLoopCount, mhInit, mhBody);
		Assert.fail("The test case failed to detect the return type of the loopCount handle is not int");
	}
	
	/**
	 * countedLoop test (LoopCount) for a null init handle
	 * Note:
	 * The logic of loop is listed as follows:
	 * int sum = 0 (by default if init is null);
	 * for (int i = 0(default); i < 20(loop count); i++) {
	 *     sum = sum + 1;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_countedLoop_LoopCount_NullInit() throws Throwable {
		MethodHandle mhLoopCount = COUNTEDLOOP_LOOP_COUNT_20;
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_NullInit",  MethodType.methodType(int.class, int.class, int.class));
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhLoopCount, null, mhBody);
		Assert.assertEquals(20, (int)mhCountedLoop.invokeExact());
	}
	
	/**
	 * countedLoop test (LoopCount) for a null loop body.
	 */
	@Test(expectedExceptions = NullPointerException.class, groups = { "level.extended" })
	public static void test_countedLoop_LoopCount_NullBody() {
		MethodHandle mhLoopCount = COUNTEDLOOP_LOOP_COUNT_20;
		MethodHandle mhInit = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhLoopCount, mhInit, null);
		Assert.fail("The test case failed to detect a null loop body");
	}
	
	/**
	 * countedLoop test (LoopCount) for the body handle with invalid parameter types and the void return type.
	 * Note: if the return type of the body handle is void, its parameter list should be
	 * (I, A...) in which the first parameter of the body handle should be int type.
	 * @throws Throwable
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_countedLoop_LoopCount_InvalidParamType_VoidReturnType_Body() throws Throwable {
		MethodHandle mhLoopCount = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhInit = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_InvalidParamType_VoidReturnType", MethodType.methodType(void.class, char.class));
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhLoopCount, mhInit, mhBody);
		Assert.fail("The test case failed to detect the first parameter type of the body handle (void return type) is not int");
	}
	
	/**
	 * countedLoop test (LoopCount) for the body handle with the void return type.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_countedLoop_LoopCount_VoidReturnType_Body() throws Throwable {
		MethodHandle mhLoopCount = COUNTEDLOOP_LOOP_COUNT_20;
		MethodHandle mhInit = MethodHandles.zero(void.class);
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_VoidReturnType", MethodType.methodType(void.class, int.class));
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhLoopCount, mhInit, mhBody);
		mhCountedLoop.invokeExact();
	}
	
	/**
	 * countedLoop test (LoopCount) for the body handle with an int return type.
	 * Note: the first parameter type of the body handle should be consistent with the return type.
	 * @throws Throwable
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_countedLoop_LoopCount_InvalidLeadingParamType_IntReturnType_Body() throws Throwable {
		MethodHandle mhLoopCount = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhInit = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_IntReturnType", MethodType.methodType(int.class, byte.class));
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhLoopCount, mhInit, mhBody);
		Assert.fail("The test case failed to detect that the first parameter type of the body handle is inconsistent with the return type");
	}
	
	/**
	 * countedLoop test (LoopCount) for the body handle with an int return type plus only one parameter.
	 * Note: the second parameter (int) must exist for the body handle with the non-void return type.
	 * @throws Throwable
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_countedLoop_LoopCount_MissingIntParamType_IntReturnType_Body() throws Throwable {
		MethodHandle mhLoopCount = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhInit = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_IntReturnType", MethodType.methodType(int.class, int.class));
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhLoopCount, mhInit, mhBody);
		Assert.fail("The test case failed to detect that the second parameter of the body handle (non-void return type) doesn't exist");
	}
	
	/**
	 * countedLoop test (LoopCount) for the body handle with an int return type and two parameters.
	 * Note: the second parameter type must be int for the body handle with the non-void return type.
	 * @throws Throwable
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_countedLoop_LoopCount_Invalid2ndParamType_IntReturnType_Body() throws Throwable {
		MethodHandle mhLoopCount = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhInit = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_IntReturnType", MethodType.methodType(int.class, int.class, byte.class));
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhLoopCount, mhInit, mhBody);
		Assert.fail("The test case failed to detect the second parameter type of the body handle (non-void return type) is not int");
	}
	
	/**
	 * countedLoop test (LoopCount) for the body handle with byte parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * byte sum = (byte)1;
	 * for (int i = 0(default); i < 20(loop count); i++) {
	 *     sum = sum + 1;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_countedLoop_LoopCount_ByteType() throws Throwable {
		MethodHandle mhLoopCount = COUNTEDLOOP_LOOP_COUNT_20;
		MethodHandle mhInit = COUNTEDLOOP_IDENTITY_BYTE_TYPE;
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_ByteType",  MethodType.methodType(byte.class, byte.class, int.class, byte.class));
		
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhLoopCount, mhInit, mhBody);
		Assert.assertEquals((byte)21, (byte)mhCountedLoop.invokeExact((byte)1));
	}
	
	/**
	 * countedLoop test (LoopCount) for the body handle with char parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * char sum = 'A';
	 * for (int i = 0(default); i < 20(loop count); i++) {
	 *     sum = sum + 1;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_countedLoop_LoopCount_CharType() throws Throwable {
		MethodHandle mhLoopCount = COUNTEDLOOP_LOOP_COUNT_20;
		MethodHandle mhInit = MethodHandles.identity(char.class);
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_CharType",  MethodType.methodType(char.class, char.class, int.class, char.class));
		
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhLoopCount, mhInit, mhBody);
		Assert.assertEquals('U', (char)mhCountedLoop.invokeExact('A'));
	}
	
	/**
	 * countedLoop test (LoopCount) for the body handle with short parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * short sum = (short)1;
	 * for (int i = 0(default); i < 20(loop count); i++) {
	 *     sum = sum + 1;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_countedLoop_LoopCount_ShortType() throws Throwable {
		MethodHandle mhLoopCount = COUNTEDLOOP_LOOP_COUNT_20;
		MethodHandle mhInit = MethodHandles.identity(short.class);
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_ShortType",  MethodType.methodType(short.class, short.class, int.class, short.class));
		
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhLoopCount, mhInit, mhBody);
		Assert.assertEquals((short)21, (short)mhCountedLoop.invokeExact((short)1));
	}
	
	/**
	 * countedLoop test (LoopCount) for the body handle with int parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * int sum = 1;
	 * for (int i = 0(default); i < 20(loop count); i++) {
	 *     sum = sum + 1;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_countedLoop_LoopCount_IntType() throws Throwable {
		MethodHandle mhLoopCount = COUNTEDLOOP_LOOP_COUNT_20;
		MethodHandle mhInit = COUNTEDLOOP_IDENTITY_INT_TYPE;
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_IntType",  MethodType.methodType(int.class, int.class, int.class, int.class));
		
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhLoopCount, mhInit, mhBody);
		Assert.assertEquals(21, (int)mhCountedLoop.invokeExact(1));
	}
	
	/**
	 * countedLoop test (LoopCount) for the body handle with long parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * long sum = (long)1;
	 * for (int i = 0(default); i < 20(loop count); i++) {
	 *     sum = sum + 1;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_countedLoop_LoopCount_LongType() throws Throwable {
		MethodHandle mhLoopCount = COUNTEDLOOP_LOOP_COUNT_20;
		MethodHandle mhInit = MethodHandles.identity(long.class);
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_LongType",  MethodType.methodType(long.class, long.class, int.class, long.class));
		
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhLoopCount, mhInit, mhBody);
		Assert.assertEquals(21l, (long)mhCountedLoop.invokeExact(1l));
	}
	
	/**
	 * countedLoop test (LoopCount) for the body handle with float parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * float sum = (float)1.5;
	 * for (int i = 0(default); i < 20(loop count); i++) {
	 *     sum = sum + 1;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_countedLoop_LoopCount_FloatType() throws Throwable {
		MethodHandle mhLoopCount = COUNTEDLOOP_LOOP_COUNT_20;
		MethodHandle mhInit = MethodHandles.identity(float.class);
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_FloatType",  MethodType.methodType(float.class, float.class, int.class, float.class));
		
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhLoopCount, mhInit, mhBody);
		Assert.assertEquals(11.5f, (float)mhCountedLoop.invokeExact(1.5f));
	}
	
	/**
	 * countedLoop test (LoopCount) for the body handle with double parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * double sum = (double)1.5;
	 * for (int i = 0(default); i < 20(loop count); i++) {
	 *     sum = sum + 1;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_countedLoop_LoopCount_DoubleType() throws Throwable {
		MethodHandle mhLoopCount = COUNTEDLOOP_LOOP_COUNT_20;
		MethodHandle mhInit = MethodHandles.identity(double.class);
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_DoubleType",  MethodType.methodType(double.class, double.class, int.class, double.class));
		
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhLoopCount, mhInit, mhBody);
		Assert.assertEquals(11.5d, (double)mhCountedLoop.invokeExact(1.5d));
	}
	
	/**
	 * countedLoop test (LoopCount) for the body handle with String parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * String sum = "str";
	 * for (int i = 0(default); i < 5(loop count); i++) {
	 *     sum = sum + " " + i;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_countedLoop_LoopCount_StringType() throws Throwable {
		MethodHandle mhLoopCount = COUNTEDLOOP_LOOP_INIT_5;
		MethodHandle mhInit = MethodHandles.identity(String.class);
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "countedLoop_Body_StringType",  MethodType.methodType(String.class, String.class, int.class, String.class));
		
		MethodHandle mhCountedLoop = MethodHandles.countedLoop(mhLoopCount, mhInit, mhBody);
		Assert.assertEquals("str 0 1 2 3 4", (String)mhCountedLoop.invokeExact("str"));
	}
}
