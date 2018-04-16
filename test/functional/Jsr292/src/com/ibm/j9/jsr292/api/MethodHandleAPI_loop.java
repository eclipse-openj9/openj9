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

public class MethodHandleAPI_loop {
	final static Lookup lookup = lookup();
	
	/**
	 * loop test for a clause array with a null value.
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_loop_NullClauses() {
		MethodHandle[][] clauses = null;
		MethodHandle mhLoop = MethodHandles.loop(clauses);
		Assert.fail("The test case failed to detect a null clause array");
	}
	
	/**
	 * loop test for a clause array without any element.
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_loop_EmptyClauses() {
		MethodHandle[][] clauses = new MethodHandle[][]{};
		MethodHandle mhLoop = MethodHandles.loop(clauses);
		Assert.fail("The test case failed to detect a clause array without elements");
	}
	
	/**
	 * loop test for a clause array with a null element.
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_loop_NullElement_Clauses() {
		MethodHandle[][] clauses = new MethodHandle[][]{null};
		MethodHandle mhLoop = MethodHandles.loop(clauses);
		Assert.fail("The test case failed to detect a clause array with a null element");
	}
	
	/**
	 * loop test for a clause array with one clause (no handle at all).
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_loop_EmptyClause_Clauses() {
		MethodHandle[][] clauses = new MethodHandle[][]{{}};
		MethodHandle mhLoop = MethodHandles.loop(clauses);
		Assert.fail("The test case failed to detect a clause array without method handle in each clause");
	}
	
	/**
	 * loop test for a clause array with one clause (only a null handle exists in the clause).
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_loop_NullHandle_Clauses() {
		MethodHandle[][] clauses = new MethodHandle[][]{{null}};
		MethodHandle mhLoop = MethodHandles.loop(clauses);
		Assert.fail("The test case failed to detect a clause array without the predicate handle");
	}
	
	/**
	 * loop test for a clause array with one clause including more handles than required.
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_loop_ExcessiveHandles_Clauses() {
		MethodHandle mhPred = MethodHandles.identity(boolean.class);
		MethodHandle[][] clauses = new MethodHandle[][]{{null, null, mhPred, null, null}};
		MethodHandle mhLoop = MethodHandles.loop(clauses);
		Assert.fail("The test case failed to detect more than 4 method handles in a clause");
	}
	
	/**
	 * loop test for a clause array with an invalid predicate handle (invalid return type) in one of clauses.
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_loop_InvalidReturnType_Predicate_Clauses() {
		MethodHandle mhPred = MethodHandles.identity(int.class);
		MethodHandle[][] clauses = new MethodHandle[][]{{null, null, mhPred, null}};
		MethodHandle mhLoop = MethodHandles.loop(clauses);
		Assert.fail("The test case failed to detect the return type of an predicate handle is not boolean");
	}
	
	/**
	 * loop test for a clause array in which the return type of the initializer handle 
	 * is inconsistent with that of the step handle in one of clauses.
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_loop_InconsistentReturnType_Init_Step_Clauses() {
		MethodHandle mhInit = MethodHandles.identity(int.class);
		MethodHandle mhStep = MethodHandles.identity(byte.class);
		MethodHandle mhPred = MethodHandles.identity(boolean.class);
		MethodHandle mhFini = MethodHandles.identity(int.class);
		MethodHandle[][] clauses = new MethodHandle[][]{{mhInit, mhStep, mhPred, mhFini}};
		MethodHandle mhLoop = MethodHandles.loop(clauses);
		Assert.fail("The test case failed to detect the inconsistent return types between the intializer handle and the step handle");
	}
	
	/**
	 * loop test for a clause array in which the return types of the 'fini' handles are different from each other.
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_loop_InconsistentReturnType_Fini_Clauses() {
		MethodHandle mhInit = MethodHandles.identity(int.class);
		MethodHandle mhStep = MethodHandles.identity(int.class);
		MethodHandle mhPred = MethodHandles.identity(boolean.class);
		MethodHandle mhFini1 = MethodHandles.identity(int.class);
		MethodHandle mhFini2 = MethodHandles.identity(long.class);
		
		MethodHandle[] clause1 = {mhInit, mhStep, mhPred, mhFini1};
		MethodHandle[] clause2 = {mhInit, mhStep, mhPred, mhFini2};
		MethodHandle mhLoop = MethodHandles.loop(clause1, clause2);
		Assert.fail("The test case failed to detect the 'fini' handles with different return type");
	}
	
	/**
	 * loop test for a clause array in which the prefix of parameter types of the step
	 * handle is inconsistent with the iteration variable types (V...).
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_loop_InvalidPrefixOfParameterTypes_Step_Clauses() throws Throwable {
		MethodHandle mhInit = lookup.findStatic(SamePackageExample.class, "loop_Init", MethodType.methodType(int.class, int.class));
		MethodHandle mhStep1 = lookup.findStatic(SamePackageExample.class, "loop_Step", MethodType.methodType(int.class, int.class, int.class, int.class));
		MethodHandle mhStep2 = MethodHandles.dropArguments(mhStep1, 1, char.class);
		MethodHandle mhPred2 = lookup.findStatic(SamePackageExample.class, "loop_Pred", MethodType.methodType(boolean.class, int.class, int.class, int.class));
		MethodHandle mhFini2 = lookup.findStatic(SamePackageExample.class, "loop_Fini", MethodType.methodType(int.class, int.class, int.class, int.class));
		
		MethodHandle[] clause1 = new MethodHandle[]{null, mhStep1};
		MethodHandle[] clause2 = new MethodHandle[]{mhInit, mhStep2, mhPred2, mhFini2};
		MethodHandle mhLoop = MethodHandles.loop(clause1, clause2);
		Assert.fail("The test case failed to detect an step handle with the invalid prefix of its parameter types");
	}
	
	/**
	 * loop test for a clause array in which the suffixes of parameter types 
	 * of non-init handles are not effectively identical to each other.
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_loop_InvalidSuffixOfParameterTypes_NonInitHandle_Clauses() throws Throwable {
		MethodHandle mhInit = lookup.findStatic(SamePackageExample.class, "loop_Init", MethodType.methodType(int.class, int.class));
		MethodHandle mhStep = lookup.findStatic(SamePackageExample.class, "loop_Step", MethodType.methodType(int.class, int.class, int.class, int.class));
		MethodHandle mhPred = lookup.findStatic(SamePackageExample.class, "loop_Pred", MethodType.methodType(boolean.class, int.class, int.class, int.class));
		mhPred = MethodHandles.dropArguments(mhPred, 2, long.class);
		MethodHandle mhFini = lookup.findStatic(SamePackageExample.class, "loop_Fini", MethodType.methodType(int.class, int.class, int.class, int.class));
		
		MethodHandle[] clause1 = new MethodHandle[]{null, mhStep};
		MethodHandle[] clause2 = new MethodHandle[]{mhInit, mhStep, mhPred, mhFini};
		MethodHandle mhLoop = MethodHandles.loop(clause1, clause2);
		Assert.fail("The test case failed to detect non-init handles with inconsistent suffix of paramter types");
	}
	
	/**
	 * loop test for a clause array with an valid predicate handle (always returns false) in a clause.
	 * Note:
	 * The logic of loop is listed as follows:
	 * for ( ; false; ) {
	 * (nothing in the loop)
	 * }
	 * The test is to verify whether the test correctly exits when both init and step are null.
	 * In such case, there is no loop parameter (A...) existing in the loop handle.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_loop_NullInitStep_ReturnFalse_Predicate_Clauses() throws Throwable {
		MethodHandle mhPred = MethodHandles.constant(boolean.class, false);
		MethodHandle mhFini = MethodHandles.constant(int.class, 1);
		MethodHandle[] clause = new MethodHandle[]{null, null, mhPred, mhFini};
		
		MethodHandle mhLoop = MethodHandles.loop(clause);
		Assert.assertEquals(1, (int)mhLoop.invokeExact());
	}
	
	/**
	 * loop test for a clause array with an valid predicate handle (always returns true) in a clause.
	 * Note:
	 * The logic of loop is listed as follows:
	 * int sum = 0;
	 * for ( ; true; ) {
	 *   sum += 3;
	 * }
	 * where "sum" contains the return value.
	 * The test is to verify whether the test correctly exits when both init and step are null.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_loop_NullInitStep_ReturnTrue_Predicate_Clauses() throws Throwable {
		MethodHandle mhPred1 = MethodHandles.constant(boolean.class, true);
		MethodHandle mhStep2 = lookup.findStatic(SamePackageExample.class, "loop_Step2", MethodType.methodType(int.class, int.class));
		MethodHandle mhPred2 = lookup.findStatic(SamePackageExample.class, "loop_Pred2", MethodType.methodType(boolean.class, int.class, int.class));
		MethodHandle mhFini2 = lookup.findStatic(SamePackageExample.class, "loop_Fini2", MethodType.methodType(int.class, int.class));
		
		MethodHandle[] clause1 = new MethodHandle[]{null, null, mhPred1, null};
		MethodHandle[] clause2 = new MethodHandle[]{null, mhStep2, mhPred2, mhFini2};
		MethodHandle mhLoop = MethodHandles.loop(clause1, clause2);
		Assert.assertEquals(21, (int)mhLoop.invokeExact(20));
	}
	
	/**
	 * loop test for a clause array with only one clause containing valid handles.
	 * Note:
	 * The logic of loop is listed as follows:
	 * int sum = 0;
	 * for ( ; sum < 20; ) {
	 *   sum += 3;
	 * }
	 * where "sum" contains the return value.
	 * The test is to verify whether the test correctly exits in the case of only one clause.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_loop_OneClause_ValidHandles_Clauses() throws Throwable {
		MethodHandle mhStep = lookup.findStatic(SamePackageExample.class, "loop_Step2", MethodType.methodType(int.class, int.class));
		MethodHandle mhPred = lookup.findStatic(SamePackageExample.class, "loop_Pred2", MethodType.methodType(boolean.class, int.class, int.class));
		MethodHandle mhFini = lookup.findStatic(SamePackageExample.class, "loop_Fini2", MethodType.methodType(int.class, int.class));
		
		MethodHandle[] clause = new MethodHandle[]{null, mhStep, mhPred, mhFini};
		MethodHandle mhLoop = MethodHandles.loop(clause);
		Assert.assertEquals(21, (int)mhLoop.invokeExact(20));
	}
	
	/**
	 * loop test for a clause array with one clause containing valid handles plus one clause with all nulls.
	 * Note:
	 * The logic of loop is listed as follows:
	 * int sum = 0;
	 * for ( ; sum < 20; ) {
	 *   sum += 3;
	 * }
	 * where "sum" contains the return value.
	 * The test is to verify whether the test correctly exits when both init and step are null.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_loop_OneClause_NullHandles_Clauses() throws Throwable {
		MethodHandle mhStep2 = lookup.findStatic(SamePackageExample.class, "loop_Step2", MethodType.methodType(int.class, int.class));
		MethodHandle mhPred2 = lookup.findStatic(SamePackageExample.class, "loop_Pred2", MethodType.methodType(boolean.class, int.class, int.class));
		MethodHandle mhFini2 = lookup.findStatic(SamePackageExample.class, "loop_Fini2", MethodType.methodType(int.class, int.class));
		
		MethodHandle[] clause1 = new MethodHandle[]{null, null, null, null};
		MethodHandle[] clause2 = new MethodHandle[]{null, mhStep2, mhPred2, mhFini2};
		MethodHandle mhLoop = MethodHandles.loop(clause1, clause2);
		Assert.assertEquals(21, (int)mhLoop.invokeExact(20));
	}
	
	/**
	 * loop test for a clause array to implement a loop with byte parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * byte sum = (byte)0;
	 * for (byte i = (byte)0; (i < (byte)10) && (sum < (byte)20); i++) {
	 *     sum = sum + i;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_loop_ByteType_Clauses() throws Throwable {
		MethodHandle mhStep1 = lookup.findStatic(SamePackageExample.class, "loop_Step1_ByteType", MethodType.methodType(byte.class, byte.class));
		MethodHandle mhInit2 =  MethodHandles.constant(byte.class, (byte)0);
		MethodHandle mhStep2 = lookup.findStatic(SamePackageExample.class, "loop_Step2_ByteType", MethodType.methodType(byte.class, byte.class, byte.class));
		MethodHandle mhPred2 = lookup.findStatic(SamePackageExample.class, "loop_Pred2_ByteType", MethodType.methodType(boolean.class, byte.class, byte.class, byte.class, byte.class));
		MethodHandle mhFini2 = lookup.findStatic(SamePackageExample.class, "loop_Fini2_ByteType",  MethodType.methodType(byte.class, byte.class, byte.class));
		
		MethodHandle[] clause1 = new MethodHandle[]{null, mhStep1};
		MethodHandle[] clause2 = new MethodHandle[]{mhInit2, mhStep2, mhPred2, mhFini2};
		MethodHandle mhLoop = MethodHandles.loop(clause1, clause2);
		Assert.assertEquals((byte)21, (byte)mhLoop.invokeExact((byte)10, (byte)20));
	}
	
	/**
	 * loop test for a clause array to implement a loop with char parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * char sum = 'A';
	 * for (char i = (char)0; (i < (char)10) && (sum < 'G'); i++) {
	 *     sum = sum + i;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_loop_CharType_Clauses() throws Throwable {
		MethodHandle mhStep1 = lookup.findStatic(SamePackageExample.class, "loop_Step1_CharType", MethodType.methodType(char.class, char.class));
		MethodHandle mhInit2 =  MethodHandles.constant(char.class, 'A');
		MethodHandle mhStep2 = lookup.findStatic(SamePackageExample.class, "loop_Step2_CharType", MethodType.methodType(char.class, char.class, char.class));
		MethodHandle mhPred2 = lookup.findStatic(SamePackageExample.class, "loop_Pred2_CharType", MethodType.methodType(boolean.class, char.class, char.class, char.class, char.class));
		MethodHandle mhFini2 = lookup.findStatic(SamePackageExample.class, "loop_Fini2_CharType",  MethodType.methodType(char.class, char.class, char.class));
		
		MethodHandle[] clause1 = new MethodHandle[]{null, mhStep1};
		MethodHandle[] clause2 = new MethodHandle[]{mhInit2, mhStep2, mhPred2, mhFini2};
		MethodHandle mhLoop = MethodHandles.loop(clause1, clause2);
		Assert.assertEquals('G', (char)mhLoop.invokeExact((char)10, 'G'));
	}
	
	/**
	 * loop test for a clause array to implement a loop with short parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * short sum = 0;
	 * for (short i = 0; (i < 10) && (sum < 20); i++) {
	 *     sum = sum + i;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_loop_ShortType_Clauses() throws Throwable {
		MethodHandle mhStep1 = lookup.findStatic(SamePackageExample.class, "loop_Step1_ShortType", MethodType.methodType(short.class, short.class));
		MethodHandle mhInit2 =  MethodHandles.constant(short.class, (short)0);
		MethodHandle mhStep2 = lookup.findStatic(SamePackageExample.class, "loop_Step2_ShortType", MethodType.methodType(short.class, short.class, short.class));
		MethodHandle mhPred2 = lookup.findStatic(SamePackageExample.class, "loop_Pred2_ShortType", MethodType.methodType(boolean.class, short.class, short.class, short.class, short.class));
		MethodHandle mhFini2 = lookup.findStatic(SamePackageExample.class, "loop_Fini2_ShortType",  MethodType.methodType(short.class, short.class, short.class));
		
		MethodHandle[] clause1 = new MethodHandle[]{null, mhStep1};
		MethodHandle[] clause2 = new MethodHandle[]{mhInit2, mhStep2, mhPred2, mhFini2};
		MethodHandle mhLoop = MethodHandles.loop(clause1, clause2);
		Assert.assertEquals((short)21, (short)mhLoop.invokeExact((short)10, (short)20));
	}
	
	/**
	 * loop test for a clause array to implement a loop with int parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * int sum = 0;
	 * for (int i = 0; (i < 10) && (sum < 20); i++) {
	 *     sum = sum + i;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_loop_IntType_Clauses() throws Throwable {
		MethodHandle mhStep1 = lookup.findStatic(SamePackageExample.class, "loop_Step1_IntType", MethodType.methodType(int.class, int.class));
		MethodHandle mhInit2 =  MethodHandles.constant(int.class, 0);
		MethodHandle mhStep2 = lookup.findStatic(SamePackageExample.class, "loop_Step2_IntType", MethodType.methodType(int.class, int.class, int.class));
		MethodHandle mhPred2 = lookup.findStatic(SamePackageExample.class, "loop_Pred2_IntType", MethodType.methodType(boolean.class, int.class, int.class, int.class, int.class));
		MethodHandle mhFini2 = lookup.findStatic(SamePackageExample.class, "loop_Fini2_IntType",  MethodType.methodType(int.class, int.class, int.class));
		
		MethodHandle[] clause1 = new MethodHandle[]{null, mhStep1};
		MethodHandle[] clause2 = new MethodHandle[]{mhInit2, mhStep2, mhPred2, mhFini2};
		MethodHandle mhLoop = MethodHandles.loop(clause1, clause2);
		Assert.assertEquals(21, (int)mhLoop.invokeExact(10, 20));
	}
	
	/**
	 * loop test for a clause array to implement a loop with long parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * long sum = 0;
	 * for (long i = 0; (i < 10) && (sum < 20); i++) {
	 *     sum = sum + i;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_loop_LongType_Clauses() throws Throwable {
		MethodHandle mhStep1 = lookup.findStatic(SamePackageExample.class, "loop_Step1_LongType", MethodType.methodType(long.class, long.class));
		MethodHandle mhInit2 =  MethodHandles.constant(long.class, 0);
		MethodHandle mhStep2 = lookup.findStatic(SamePackageExample.class, "loop_Step2_LongType", MethodType.methodType(long.class, long.class, long.class));
		MethodHandle mhPred2 = lookup.findStatic(SamePackageExample.class, "loop_Pred2_LongType", MethodType.methodType(boolean.class, long.class, long.class, long.class, long.class));
		MethodHandle mhFini2 = lookup.findStatic(SamePackageExample.class, "loop_Fini2_LongType",  MethodType.methodType(long.class, long.class, long.class));
		
		MethodHandle[] clause1 = new MethodHandle[]{null, mhStep1};
		MethodHandle[] clause2 = new MethodHandle[]{mhInit2, mhStep2, mhPred2, mhFini2};
		MethodHandle mhLoop = MethodHandles.loop(clause1, clause2);
		Assert.assertEquals((long)21, (long)mhLoop.invokeExact((long)10, (long)20));
	}
	
	/**
	 * loop test for a clause array to implement a loop with float parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * float sum = 0;
	 * for (float i = 0; (i < 10.5) && (sum < 20.5); i++) {
	 *     sum = sum + i;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_loop_FloatType_Clauses() throws Throwable {
		MethodHandle mhStep1 = lookup.findStatic(SamePackageExample.class, "loop_Step1_FloatType", MethodType.methodType(float.class, float.class));
		MethodHandle mhInit2 =  MethodHandles.constant(float.class, 0);
		MethodHandle mhStep2 = lookup.findStatic(SamePackageExample.class, "loop_Step2_FloatType", MethodType.methodType(float.class, float.class, float.class));
		MethodHandle mhPred2 = lookup.findStatic(SamePackageExample.class, "loop_Pred2_FloatType", MethodType.methodType(boolean.class, float.class, float.class, float.class, float.class));
		MethodHandle mhFini2 = lookup.findStatic(SamePackageExample.class, "loop_Fini2_FloatType",  MethodType.methodType(float.class, float.class, float.class));
		
		MethodHandle[] clause1 = new MethodHandle[]{null, mhStep1};
		MethodHandle[] clause2 = new MethodHandle[]{mhInit2, mhStep2, mhPred2, mhFini2};
		MethodHandle mhLoop = MethodHandles.loop(clause1, clause2);
		Assert.assertEquals(21F, (float)mhLoop.invokeExact(10.5F, 20.5F));
	}
	
	/**
	 * loop test for a clause array to implement a loop with double parameters/return type
	 * Note:
	 * The logic of loop is listed as follows:
	 * double sum = 0;
	 * for (double i = 0; (i < 10.5) && (sum < 20.5); i++) {
	 *     sum = sum + i;
	 * }
	 * where sum contains the return value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_loop_DoubleType_Clauses() throws Throwable {
		MethodHandle mhStep1 = lookup.findStatic(SamePackageExample.class, "loop_Step1_DoubleType", MethodType.methodType(double.class, double.class));
		MethodHandle mhInit2 =  MethodHandles.constant(double.class, 0);
		MethodHandle mhStep2 = lookup.findStatic(SamePackageExample.class, "loop_Step2_DoubleType", MethodType.methodType(double.class, double.class, double.class));
		MethodHandle mhPred2 = lookup.findStatic(SamePackageExample.class, "loop_Pred2_DoubleType", MethodType.methodType(boolean.class, double.class, double.class, double.class, double.class));
		MethodHandle mhFini2 = lookup.findStatic(SamePackageExample.class, "loop_Fini2_DoubleType",  MethodType.methodType(double.class, double.class, double.class));
		
		MethodHandle[] clause1 = new MethodHandle[]{null, mhStep1};
		MethodHandle[] clause2 = new MethodHandle[]{mhInit2, mhStep2, mhPred2, mhFini2};
		MethodHandle mhLoop = MethodHandles.loop(clause1, clause2);
		Assert.assertEquals(21D, (double)mhLoop.invokeExact(10.5D, 20.5D));
	}
}
