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
import static java.lang.invoke.MethodHandles.Lookup;
import static java.lang.invoke.MethodHandles.lookup;

import java.io.IOException;

public class MethodHandleAPI_whileLoop {
	final static Lookup lookup = lookup();
	
	static MethodHandle WHILELOOP_INIT_INVALID;
	static MethodHandle WHILELOOP_PRED;
	static MethodHandle WHILELOOP_BODY;
	static {
		try {
			WHILELOOP_INIT_INVALID = lookup.findStatic(SamePackageExample.class, "whileLoop_Init_InvalidParamType", MethodType.methodType(int.class, byte.class, int.class));
			WHILELOOP_BODY = lookup.findStatic(SamePackageExample.class, "whileLoop_Body", MethodType.methodType(int.class, int.class, int.class));
			WHILELOOP_PRED = lookup.findStatic(SamePackageExample.class, "whileLoop_Pred", MethodType.methodType(boolean.class, int.class, int.class));
			} catch (IllegalAccessException | NoSuchMethodException e) {
				throw new RuntimeException(e);
		}
	}
	
	/**
	 * The following test cases cover both whileLoop and doWhileLoop.
	 */
	
	/* Test cases of whileLoop */
	
	/**
	 * whileLoop test for a null loop body.
	 */
	@Test(expectedExceptions = NullPointerException.class, groups = { "level.extended" })
	public static void test_whileLoop_NullBody() {
		MethodHandle mhInit = MethodHandles.zero(int.class);
		MethodHandle mhPred = MethodHandles.identity(boolean.class);
		MethodHandle mhWhileLoop = MethodHandles.whileLoop(mhInit, mhPred, null);
		Assert.fail("The test case failed to detect a null loop body");
	}
	
	/**
	 * whileLoop test for an invalid loop body in which
	 * the first parameter type is inconsistent with the return type.
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_whileLoop_InconsistentParamType_Body() {
		MethodHandle mhInit = MethodHandles.zero(int.class);
		MethodHandle mhPred = WHILELOOP_PRED;
		MethodHandle mhBody = MethodHandles.dropArguments(WHILELOOP_BODY, 0, byte.class);
		MethodHandle mhWhileLoop = MethodHandles.whileLoop(mhInit, mhPred, mhBody);
		Assert.fail("The test case failed to detect that the first parameter type of body is inconsistent with the return type");
	}
	
	/**
	 * whileLoop test for init with a return type inconsistent with that of Body.
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_whileLoop_InconsistentReturnType_InitBody() {
		MethodHandle mhInit = MethodHandles.zero(char.class);
		MethodHandle mhPred = WHILELOOP_PRED;
		MethodHandle mhBody = WHILELOOP_BODY;
		MethodHandle mhWhileLoop = MethodHandles.whileLoop(mhInit, mhPred, mhBody);
		Assert.fail("The test case failed to detect the inconsistent return type of init");
	}
	
	/**
	 * whileLoop test for init with invalid parameter types against the external parameter types of loop body.
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_whileLoop_InvalidParamType_Init() {
		MethodHandle mhInit = WHILELOOP_INIT_INVALID;;
		MethodHandle mhPred = WHILELOOP_PRED;
		MethodHandle mhBody = WHILELOOP_BODY;
		MethodHandle mhWhileLoop = MethodHandles.whileLoop(mhInit, mhPred, mhBody);
		Assert.fail("The test case failed to detect the parameter types of init are inconsistent with the parameter types of loop body");
	}
	
	/**
	 * whileLoop test for a null predicate.
	 */
	@Test(expectedExceptions = NullPointerException.class, groups = { "level.extended" })
	public static void test_whileLoop_NullPredicate() {
		MethodHandle mhInit = MethodHandles.zero(int.class);
		MethodHandle mhBody = WHILELOOP_BODY;
		MethodHandle mhWhileLoop = MethodHandles.whileLoop(mhInit, null, mhBody);
		Assert.fail("The test case failed to detect a null predicate");
	}
	
	/**
	 * whileLoop test for the body handle with the void return type.
	 * Note: we force it to throw out an exception from the loop body
	 * to determine whether the loop body gets executed.
	 * @throws Throwable
	 */
	@Test(expectedExceptions = IOException.class, expectedExceptionsMessageRegExp = "while_Body_VoidReturnType", groups = { "level.extended" })
	public static void test_whileLoop_VoidReturnType_Body() throws Throwable {
		MethodHandle mhInit = lookup.findStatic(SamePackageExample.class, "while_Init_VoidReturnType", MethodType.methodType(void.class, int.class));
		MethodHandle mhPred = MethodHandles.constant(boolean.class, true);
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "while_Body_VoidReturnType", MethodType.methodType(void.class, int.class));
		MethodHandle mhWhileLoop = MethodHandles.whileLoop(mhInit, mhPred, mhBody);
		mhWhileLoop.invokeExact(1);
		Assert.fail("The test case failed to execute the loop body with the void return type");
	}
	
	/**
	 * whileLoop test for valid handles to implement a while loop with byte parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * byte sum = (byte)1;
	 * while (sum < (byte)20)) {
	 *     sum = sum + 2;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_whileLoop_ByteType() throws Throwable {
		MethodHandle mhInit = lookup.findStatic(SamePackageExample.class, "while_Init_ByteType", MethodType.methodType(byte.class, byte.class));
		MethodHandle mhPred = lookup.findStatic(SamePackageExample.class, "while_Pred_ByteType", MethodType.methodType(boolean.class, byte.class, byte.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "while_Body_ByteType", MethodType.methodType(byte.class, byte.class, byte.class));

		MethodHandle mhWhileLoop = MethodHandles.whileLoop(mhInit, mhPred, mhBody);
		Assert.assertEquals((byte)21, (byte)mhWhileLoop.invokeExact((byte)20));
	}
	
	/**
	 * whileLoop test for valid handles to implement a while loop with char parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * char sum = 'A';
	 * while (sum < 'G')) {
	 *     sum = sum + 1;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_whileLoop_CharType() throws Throwable {
		MethodHandle mhInit = lookup.findStatic(SamePackageExample.class, "while_Init_CharType", MethodType.methodType(char.class, char.class));
		MethodHandle mhPred = lookup.findStatic(SamePackageExample.class, "while_Pred_CharType", MethodType.methodType(boolean.class, char.class, char.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "while_Body_CharType", MethodType.methodType(char.class, char.class, char.class));
		
		MethodHandle mhWhileLoop = MethodHandles.whileLoop(mhInit, mhPred, mhBody);
		Assert.assertEquals('G', (char)mhWhileLoop.invokeExact('G'));
	}
	
	/**
	 * whileLoop test for valid handles to implement a while loop with short parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * short sum = (short)1;
	 * while (sum < (short)20)) {
	 *     sum = sum + 2;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_whileLoop_ShortType() throws Throwable {
		MethodHandle mhInit = lookup.findStatic(SamePackageExample.class, "while_Init_ShortType", MethodType.methodType(short.class, short.class));
		MethodHandle mhPred = lookup.findStatic(SamePackageExample.class, "while_Pred_ShortType", MethodType.methodType(boolean.class, short.class, short.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "while_Body_ShortType", MethodType.methodType(short.class, short.class, short.class));

		MethodHandle mhWhileLoop = MethodHandles.whileLoop(mhInit, mhPred, mhBody);
		Assert.assertEquals((short)21, (short)mhWhileLoop.invokeExact((short)20));
	}
	
	/**
	 * whileLoop test for valid handles to implement a while loop with int parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * int sum = 1;
	 * while (sum < 20)) {
	 *     sum = sum + 2;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_whileLoop_IntType() throws Throwable {
		MethodHandle mhInit = lookup.findStatic(SamePackageExample.class, "while_Init_IntType", MethodType.methodType(int.class, int.class));
		MethodHandle mhPred = lookup.findStatic(SamePackageExample.class, "while_Pred_IntType", MethodType.methodType(boolean.class, int.class, int.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "while_Body_IntType", MethodType.methodType(int.class, int.class, int.class));

		MethodHandle mhWhileLoop = MethodHandles.whileLoop(mhInit, mhPred, mhBody);
		Assert.assertEquals(21, (int)mhWhileLoop.invokeExact(20));
	}
	
	/**
	 * whileLoop test for valid handles to implement a while loop with long parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * long sum = (long)1;
	 * while (sum < (long)20)) {
	 *     sum = sum + 2;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_whileLoop_LongType() throws Throwable {
		MethodHandle mhInit = lookup.findStatic(SamePackageExample.class, "while_Init_LongType", MethodType.methodType(long.class, long.class));
		MethodHandle mhPred = lookup.findStatic(SamePackageExample.class, "while_Pred_LongType", MethodType.methodType(boolean.class, long.class, long.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "while_Body_LongType", MethodType.methodType(long.class, long.class, long.class));

		MethodHandle mhWhileLoop = MethodHandles.whileLoop(mhInit, mhPred, mhBody);
		Assert.assertEquals(21L, (long)mhWhileLoop.invokeExact(20L));
	}
	
	/**
	 * whileLoop test for valid handles to implement a while loop with float parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * float sum = (float)1;
	 * while (sum < (float)20.5)) {
	 *     sum = sum + 2;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_whileLoop_FloatType() throws Throwable {
		MethodHandle mhInit = lookup.findStatic(SamePackageExample.class, "while_Init_FloatType", MethodType.methodType(float.class, float.class));
		MethodHandle mhPred = lookup.findStatic(SamePackageExample.class, "while_Pred_FloatType", MethodType.methodType(boolean.class, float.class, float.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "while_Body_FloatType", MethodType.methodType(float.class, float.class, float.class));

		MethodHandle mhWhileLoop = MethodHandles.whileLoop(mhInit, mhPred, mhBody);
		Assert.assertEquals(21F, (float)mhWhileLoop.invokeExact(20.5F));
	}
	
	/**
	 * whileLoop test for valid handles to implement a while loop with double parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * double sum = (double)1;
	 * while (sum < (double)20.5)) {
	 *     sum = sum + 2;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_whileLoop_DoubleType() throws Throwable {
		MethodHandle mhInit = lookup.findStatic(SamePackageExample.class, "while_Init_DoubleType", MethodType.methodType(double.class, double.class));
		MethodHandle mhPred = lookup.findStatic(SamePackageExample.class, "while_Pred_DoubleType", MethodType.methodType(boolean.class, double.class, double.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "while_Body_DoubleType", MethodType.methodType(double.class, double.class, double.class));

		MethodHandle mhWhileLoop = MethodHandles.whileLoop(mhInit, mhPred, mhBody);
		Assert.assertEquals(21D, (double)mhWhileLoop.invokeExact(20.5D));
	}
	
	/**
	 * whileLoop test for valid handles to implement a while loop with String parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * String sum = "str";
	 * while (!sum.contentEquals("str str str str str")) {
	 *     sum = sum + " " + "str";
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_whileLoop_StringType() throws Throwable {
		MethodHandle mhInit = lookup.findStatic(SamePackageExample.class, "while_Init_StringType", MethodType.methodType(String.class, String.class));
		MethodHandle mhPred = lookup.findStatic(SamePackageExample.class, "while_Pred_StringType", MethodType.methodType(boolean.class, String.class, String.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "while_Body_StringType", MethodType.methodType(String.class, String.class, String.class));

		MethodHandle mhWhileLoop = MethodHandles.whileLoop(mhInit, mhPred, mhBody);
		Assert.assertEquals("str str str str str", (String)mhWhileLoop.invokeExact("str"));
	}

	/* Test cases of doWhileLoop */
	
	/**
	 * doWhileLoop test for a null loop body.
	 */
	@Test(expectedExceptions = NullPointerException.class, groups = { "level.extended" })
	public static void test_doWhileLoop_NullBody() {
		MethodHandle mhInit = MethodHandles.zero(int.class);
		MethodHandle mhPred = MethodHandles.identity(boolean.class);
		MethodHandle mhWhileLoop = MethodHandles.doWhileLoop(mhInit, null, mhPred);
		Assert.fail("The test case failed to detect a null loop body");
	}
	
	/**
	 * doWhileLoop test for an invalid loop body in which the first parameter type
	 * is inconsistent with the return type.
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_doWhileLoop_InconsistentParamType_Body() {
		MethodHandle mhInit = MethodHandles.zero(int.class);
		MethodHandle mhPred = WHILELOOP_PRED;
		MethodHandle mhBody = MethodHandles.dropArguments(WHILELOOP_BODY, 0, byte.class);
		MethodHandle mhWhileLoop = MethodHandles.doWhileLoop(mhInit, mhBody, mhPred);
		Assert.fail("The test case failed to detect that the first parameter type of loop body is inconsistent with the return type");
	}
	
	/**
	 * doWhileLoop test for init with a return type inconsistent with that of the loop body.
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_doWhileLoop_InconsistentReturnType_InitBody() {
		MethodHandle mhInit = MethodHandles.zero(char.class);
		MethodHandle mhPred = WHILELOOP_PRED;
		MethodHandle mhBody = WHILELOOP_BODY;
		MethodHandle mhWhileLoop = MethodHandles.doWhileLoop(mhInit, mhBody, mhPred);
		Assert.fail("The test case failed to detect the return type of init is inconsistent with that of the loop body");
	}
	
	/**
	 * doWhileLoop test for init with invalid parameter types against the external parameter types of loop body.
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_doWhileLoop_InvalidParamType_Init() {
		MethodHandle mhInit = WHILELOOP_INIT_INVALID;
		MethodHandle mhPred = WHILELOOP_PRED;
		MethodHandle mhBody = WHILELOOP_BODY;
		MethodHandle mhWhileLoop = MethodHandles.doWhileLoop(mhInit, mhBody, mhPred);
		Assert.fail("The test case failed to detect the parameter types of init are inconsistent with the parameter types of loop body");
	}
	
	/**
	 * doWhileLoop test for a null predicate.
	 */
	@Test(expectedExceptions = NullPointerException.class, groups = { "level.extended" })
	public static void test_doWhileLoop_NullPredicate() {
		MethodHandle mhInit = MethodHandles.zero(int.class);
		MethodHandle mhPred = WHILELOOP_PRED;
		MethodHandle mhBody = WHILELOOP_BODY;
		MethodHandle mhWhileLoop = MethodHandles.doWhileLoop(mhInit, mhBody, null);
		Assert.fail("The test case failed to detect a null predicate");
	}
	
	/**
	 * doWhileLoop test for the body handle with the void return type.
	 * Note: we force it to throw out an exception from the loop body
	 * to determine whether the loop body gets executed.
	 * @throws Throwable
	 */
	@Test(expectedExceptions = IOException.class, expectedExceptionsMessageRegExp = "while_Body_VoidReturnType", groups = { "level.extended" })
	public static void test_doWhileLoop_VoidReturnType_Body() throws Throwable {
		MethodHandle mhInit = lookup.findStatic(SamePackageExample.class, "while_Init_VoidReturnType", MethodType.methodType(void.class, int.class));
		MethodHandle mhPred = MethodHandles.constant(boolean.class, true);
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "while_Body_VoidReturnType", MethodType.methodType(void.class, int.class));
		MethodHandle mhWhileLoop = MethodHandles.doWhileLoop(mhInit, mhBody, mhPred);
		mhWhileLoop.invokeExact(1);
		Assert.fail("The test case failed to execute the loop body with the void return type");
	}
	
	/**
	 * doWhileLoop test for valid handles to implement a do-while loop with byte parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * byte sum = (byte)1;
	 * do {
	 *  sum = sum + 2;
	 * } while (sum < (byte)20));
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_doWhileLoop_ByteType() throws Throwable {
		MethodHandle mhInit = lookup.findStatic(SamePackageExample.class, "while_Init_ByteType", MethodType.methodType(byte.class, byte.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "while_Body_ByteType", MethodType.methodType(byte.class, byte.class, byte.class));
		MethodHandle mhPred = lookup.findStatic(SamePackageExample.class, "while_Pred_ByteType", MethodType.methodType(boolean.class, byte.class, byte.class));

		MethodHandle mhWhileLoop = MethodHandles.doWhileLoop(mhInit, mhBody, mhPred);
		Assert.assertEquals((byte)21, (byte)mhWhileLoop.invokeExact((byte)20));
	}
	
	/**
	 * doWhileLoop test for valid handles to implement a do-while loop with char parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * char sum = 'A';
	 * do {
	 *  sum = sum + 2;
	 * } while (sum < 'G'));
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_doWhileLoop_CharType() throws Throwable {
		MethodHandle mhInit = lookup.findStatic(SamePackageExample.class, "while_Init_CharType", MethodType.methodType(char.class, char.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "while_Body_CharType", MethodType.methodType(char.class, char.class, char.class));
		MethodHandle mhPred = lookup.findStatic(SamePackageExample.class, "while_Pred_CharType", MethodType.methodType(boolean.class, char.class, char.class));

		MethodHandle mhWhileLoop = MethodHandles.doWhileLoop(mhInit, mhBody, mhPred);
		Assert.assertEquals('G', (char)mhWhileLoop.invokeExact('G'));
	}
	
	/**
	 * doWhileLoop test for valid handles to implement a do-while loop with short parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * short sum = (short)1;
	 * do {
	 *  sum = sum + 2;
	 * } while (sum < (short)20));
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_doWhileLoop_ShortType() throws Throwable {
		MethodHandle mhInit = lookup.findStatic(SamePackageExample.class, "while_Init_ShortType", MethodType.methodType(short.class, short.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "while_Body_ShortType", MethodType.methodType(short.class, short.class, short.class));
		MethodHandle mhPred = lookup.findStatic(SamePackageExample.class, "while_Pred_ShortType", MethodType.methodType(boolean.class, short.class, short.class));

		MethodHandle mhWhileLoop = MethodHandles.doWhileLoop(mhInit, mhBody, mhPred);
		Assert.assertEquals((short)21, (short)mhWhileLoop.invokeExact((short)20));
	}
	
	/**
	 * doWhileLoop test for valid handles to implement a do-while loop with int parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * int sum = 1;
	 * do {
	 *  sum = sum + 2;
	 * } while (sum < 20));
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_doWhileLoop_IntType() throws Throwable {
		MethodHandle mhInit = lookup.findStatic(SamePackageExample.class, "while_Init_IntType", MethodType.methodType(int.class, int.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "while_Body_IntType", MethodType.methodType(int.class, int.class, int.class));
		MethodHandle mhPred = lookup.findStatic(SamePackageExample.class, "while_Pred_IntType", MethodType.methodType(boolean.class, int.class, int.class));

		MethodHandle mhWhileLoop = MethodHandles.doWhileLoop(mhInit, mhBody, mhPred);
		Assert.assertEquals(21, (int)mhWhileLoop.invokeExact(20));
	}
	
	/**
	 * doWhileLoop test for valid handles to implement a do-while loop with long parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * long sum = (long)1;
	 * do {
	 *  sum = sum + 2;
	 * } while (sum < (long)20));
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_doWhileLoop_LongType() throws Throwable {
		MethodHandle mhInit = lookup.findStatic(SamePackageExample.class, "while_Init_LongType", MethodType.methodType(long.class, long.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "while_Body_LongType", MethodType.methodType(long.class, long.class, long.class));
		MethodHandle mhPred = lookup.findStatic(SamePackageExample.class, "while_Pred_LongType", MethodType.methodType(boolean.class, long.class, long.class));

		MethodHandle mhWhileLoop = MethodHandles.doWhileLoop(mhInit, mhBody, mhPred);
		Assert.assertEquals(21L, (long)mhWhileLoop.invokeExact(20L));
	}
	
	/**
	 * doWhileLoop test for valid handles to implement a do-while loop with float parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * float sum = (float)1;
	 * do {
	 *  sum = sum + 2;
	 * } while (sum < (float)20.5));
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_doWhileLoop_FloatType() throws Throwable {
		MethodHandle mhInit = lookup.findStatic(SamePackageExample.class, "while_Init_FloatType", MethodType.methodType(float.class, float.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "while_Body_FloatType", MethodType.methodType(float.class, float.class, float.class));
		MethodHandle mhPred = lookup.findStatic(SamePackageExample.class, "while_Pred_FloatType", MethodType.methodType(boolean.class, float.class, float.class));

		MethodHandle mhWhileLoop = MethodHandles.doWhileLoop(mhInit, mhBody, mhPred);
		Assert.assertEquals(21F, (float)mhWhileLoop.invokeExact(20.5F));
	}
	
	/**
	 * doWhileLoop test for valid handles to implement a do-while loop with double parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * double sum = (double)1;
	 * do {
	 *  sum = sum + 2;
	 * } while (sum < (double)20.5));
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_doWhileLoop_DoubleType() throws Throwable {
		MethodHandle mhInit = lookup.findStatic(SamePackageExample.class, "while_Init_DoubleType", MethodType.methodType(double.class, double.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "while_Body_DoubleType", MethodType.methodType(double.class, double.class, double.class));
		MethodHandle mhPred = lookup.findStatic(SamePackageExample.class, "while_Pred_DoubleType", MethodType.methodType(boolean.class, double.class, double.class));

		MethodHandle mhWhileLoop = MethodHandles.doWhileLoop(mhInit, mhBody, mhPred);
		Assert.assertEquals(21D, (double)mhWhileLoop.invokeExact(20.5D));
	}
	
	/**
	 * doWhileLoop test for valid handles to implement a do-while loop with String parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * String sum = "str";
	 * do {
	 *   sum = sum + " " + "str";
	 * } while (!sum.contentEquals("str str str str str"));
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_doWhileLoop_StringType() throws Throwable {
		MethodHandle mhInit = lookup.findStatic(SamePackageExample.class, "while_Init_StringType", MethodType.methodType(String.class, String.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "while_Body_StringType", MethodType.methodType(String.class, String.class, String.class));
		MethodHandle mhPred = lookup.findStatic(SamePackageExample.class, "while_Pred_StringType", MethodType.methodType(boolean.class, String.class, String.class));

		MethodHandle mhWhileLoop = MethodHandles.doWhileLoop(mhInit, mhBody, mhPred);
		Assert.assertEquals("str str str str str", (String)mhWhileLoop.invokeExact("str"));
	}
}
