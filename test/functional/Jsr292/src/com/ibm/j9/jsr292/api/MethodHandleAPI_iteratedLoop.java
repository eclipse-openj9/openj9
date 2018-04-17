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
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;
import java.io.IOException;
import java.lang.invoke.MethodHandles.Lookup;
import static java.lang.invoke.MethodHandles.lookup;


public class MethodHandleAPI_iteratedLoop {
	final static Lookup lookup = lookup();
	
	static MethodHandle ITERATEDLOOP_ITERATOR;
	static MethodHandle ITERATEDLOOP_INIT;
	static MethodHandle ITERATEDLOOP_INIT_ALLTYPES;
	static MethodHandle ITERATEDLOOP_BODY;
	static {
		try {
			ITERATEDLOOP_ITERATOR = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Iterator", MethodType.methodType(Iterator.class, List.class, int.class));
			ITERATEDLOOP_INIT = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Init", MethodType.methodType(int.class, List.class, int.class));
			ITERATEDLOOP_BODY = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Body", MethodType.methodType(int.class, int.class, Integer.class));
			ITERATEDLOOP_INIT_ALLTYPES = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Init_AllTypes", MethodType.methodType(String.class));
		} catch (IllegalAccessException | NoSuchMethodException e) {
				throw new RuntimeException(e);
		}
	}
	
	/**
	 * iteratedLoop test for a null loop body.
	 */
	@Test(expectedExceptions = NullPointerException.class, groups = { "level.extended" })
	public static void test_iteratedLoop_NullBody() {
		MethodHandle mhIterator = ITERATEDLOOP_ITERATOR;
		MethodHandle mhInit = ITERATEDLOOP_INIT;
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(mhIterator, mhInit, null);
		Assert.fail("The test case failed to detect a null loop body");
	}
	
	/**
	 * iteratedLoop test for a loop body (non-void return type) with only one parameter for V (T is missing).
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_iteratedLoop_MissingParamType_Body() {
		MethodHandle mhIterator = ITERATEDLOOP_ITERATOR;
		MethodHandle mhInit = ITERATEDLOOP_INIT;
		MethodHandle mhBody = MethodHandles.identity(int.class);
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(mhIterator, mhInit, mhBody);
		Assert.fail("The test case failed to detect that the parameter type T is missing in the loop body (non-void return type)");
	}
	
	/**
	 * iteratedLoop test for an invalid loop body in which the first parameter type
	 * is inconsistent with the return type.
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_iteratedLoop_InconsistentParamType_Body() {
		MethodHandle mhIterator = ITERATEDLOOP_ITERATOR;
		MethodHandle mhInit = ITERATEDLOOP_INIT;
		MethodHandle mhBody = MethodHandles.dropArguments(ITERATEDLOOP_BODY, 0, char.class);
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(mhIterator, mhInit, mhBody);
		Assert.fail("The test case failed to detect that the leading parameter type of loop body is inconsistent with the return type");
	}
	
	/**
	 * iteratedLoop test for iterator with an invalid return type (neither Iterator nor its subtype).
	 * @throws Throwable
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_iteratedLoop_InvalidReturnType_Iterator() throws Throwable {
		MethodHandle mhIterator = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Iterator_InvalidReturnType", MethodType.methodType(int.class, Iterator.class, int.class));
		MethodHandle mhInit = ITERATEDLOOP_INIT;
		MethodHandle mhBody = ITERATEDLOOP_BODY;
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(mhIterator, mhInit, mhBody);
		Assert.fail("The test case failed to detect the return type of the iterator handle is neither Iterator nor its subtype");
	}
	
	/**
	 * iteratedLoop test for loop body with the external parameter types inconsistent with
	 * init/iterator when both init and iterator are exists.
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_iteratedLoop_InvalidExternalParamType_Body() {
		MethodHandle mhIterator = ITERATEDLOOP_ITERATOR;
		MethodHandle mhInit = ITERATEDLOOP_INIT;
		MethodHandle mhBody = MethodHandles.dropArguments(ITERATEDLOOP_BODY, 2, List.class);
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(mhIterator, mhInit, mhBody);
		Assert.fail("The test case failed to detect that the external parameter types of loop body are inconsistent with init/iterator");
	}
	
	/**
	 * iteratedLoop test for loop body with an invalid leading external parameter type
	 * when the iterator handle is null.
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_iteratedLoop_InvalidLeadingExternalParamType_Body_NullIterator() {
		MethodHandle mhIterator = null;
		MethodHandle mhInit = ITERATEDLOOP_INIT;
		MethodHandle mhBody = MethodHandles.dropArguments(ITERATEDLOOP_BODY, 2, char.class);
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(mhIterator, mhInit, mhBody);
		Assert.fail("The test case failed to detect that the leading external parameter of loop body is neither Iterable nor its subtype");
	}
	
	/**
	 * iteratedLoop test for loop body with the external parameter types inconsistent with init
	 * when iterator is null.
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_iteratedLoop_InvalidExternalParamType_Body_NullIterator() {
		MethodHandle mhIterator = null;
		MethodHandle mhInit = ITERATEDLOOP_INIT;
		MethodHandle mhBody = MethodHandles.dropArguments(ITERATEDLOOP_BODY, 2, List.class);
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(mhIterator, mhInit, mhBody);
		Assert.fail("The test case failed to detect that the external parameter types of loop body are inconsistent with init");
	}
	
	/**
	 * iteratedLoop test for init with the return type inconsistent with that of loop body.
	 * @throws Throwable
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_iteratedLoop_InvalidReturnType_Init() throws Throwable {
		MethodHandle mhIterator = ITERATEDLOOP_ITERATOR;
		MethodHandle mhInit = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Init_InvalidReturnType", MethodType.methodType(byte.class, List.class, int.class));
		MethodHandle mhBody = ITERATEDLOOP_BODY;
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(mhIterator, mhInit, mhBody);
		Assert.fail("The test case failed to detect that the return type of init is inconsistent with that of loop body");
	}
	
	/**
	 * iteratedLoop test for init with an invalid leading parameter type when the iterator handle is null.
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_iteratedLoop_InvalidLeadingParamType_Init_NullIterator() {
		MethodHandle mhIterator = null;
		MethodHandle mhInit = MethodHandles.dropArguments(ITERATEDLOOP_INIT, 0, byte.class);
		MethodHandle mhBody = ITERATEDLOOP_BODY;
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(mhIterator, mhInit, mhBody);
		Assert.fail("The test case failed to detect that the leading parameter types of init is neither Iterable nor its subtype");
	}
	
	/**
	 * iteratedLoop test for init with the parameter types inconsistent with the external
	 * parameter types of loop body when the iterator handle is null.
	 * @throws Throwable
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_iteratedLoop_InconsistentParamTypes_Init_NullIterator() throws Throwable {
		MethodHandle mhIterator = null;
		MethodHandle mhInit = MethodHandles.dropArguments(ITERATEDLOOP_INIT, 2, byte.class);
		MethodHandle mhBody = MethodHandles.dropArguments(ITERATEDLOOP_BODY, 2, List.class, int.class, int.class);
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(mhIterator, mhInit, mhBody);
		Assert.fail("The test case failed to detect that the parameter types of init are inconsistent with the external parameter types of loop body");
	}
	
	/**
	 * iteratedLoop test for init with parameter types more than that of iterator.
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_iteratedLoop_MoreParamTypes_Init_NonNullIterator() {
		MethodHandle mhIterator = ITERATEDLOOP_ITERATOR;
		MethodHandle mhInit = MethodHandles.dropArguments(ITERATEDLOOP_INIT, 2, char.class);
		MethodHandle mhBody = ITERATEDLOOP_BODY;
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(mhIterator, mhInit, mhBody);
		Assert.fail("The test case failed to detect that the parameter types of init is more than iterator");
	}
	
	/**
	 * iteratedLoop test for init with a parameter list less than that of iterator.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_iteratedLoop_LessParamTypes_Init_NonNullIterator() throws Throwable {
		MethodHandle mhIterator = MethodHandles.dropArguments(ITERATEDLOOP_ITERATOR, 2, char.class);
		MethodHandle mhInit = ITERATEDLOOP_INIT;
		MethodHandle mhBody = ITERATEDLOOP_BODY;
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(mhIterator, mhInit, mhBody);
	}
	
	/**
	 * iteratedLoop test for iterator & init without parameter
	 * Note: this is a normal case without any exception
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_iteratedLoop_EmptyParamTypes_Iterator_Init() throws Throwable {
		MethodHandle mhIterator = MethodHandles.zero(Iterator.class);
		MethodHandle mhInit = MethodHandles.zero(int.class);
		MethodHandle mhBody = ITERATEDLOOP_BODY;
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(mhIterator, mhInit, mhBody);
	}
	
	/**
	 * iteratedLoop test for loop body with the void return type
	 * @throws Throwable
	 */
	@Test(expectedExceptions = IOException.class, expectedExceptionsMessageRegExp = "iteratedLoop_Body_VoidReturnType", groups = { "level.extended" }, invocationCount = 2)
	public static void test_iteratedLoop_VoidReturnType_Body() throws Throwable {
		MethodHandle mhIterator = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Iterator_IntType", MethodType.methodType(Iterator.class, List.class));
		MethodHandle mhInit = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Init_VoidReturnType", MethodType.methodType(void.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Body_VoidReturnType", MethodType.methodType(void.class, Integer.class));
		List<Integer> list = Arrays.asList(10);
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(mhIterator, mhInit, mhBody);
		mhIteratedLoop.invokeExact(list);
		Assert.fail("The test case failed to execute the loop body with the void return type");
	}
	
	/**
	 * iteratedLoop test for loop body with the Byte parameter.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_iteratedLoop_ByteType_NullIterator() throws Throwable {
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Body_ByteType", MethodType.methodType(String.class, String.class, Byte.class));
		List<Byte> list = Arrays.asList((byte)1, (byte)2, (byte)3, (byte)4,(byte)5);
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(null, ITERATEDLOOP_INIT_ALLTYPES, mhBody);
		Assert.assertEquals("5 4 3 2 1 ", (String)mhIteratedLoop.invokeExact((Iterable<Byte>)list));
	}
	
	/**
	 * iteratedLoop test for loop body with the Byte parameter when iterator exists.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_iteratedLoop_ByteType_NonNullIterator() throws Throwable {
		MethodHandle mhIterator = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Iterator_ByteType", MethodType.methodType(Iterator.class, List.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Body_ByteType", MethodType.methodType(String.class, String.class, Byte.class));
		List<Byte> list = Arrays.asList((byte)1, (byte)2, (byte)3, (byte)4,(byte)5);
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(mhIterator, ITERATEDLOOP_INIT_ALLTYPES, mhBody);
		Assert.assertEquals("5 4 3 2 1 ", (String)mhIteratedLoop.invokeExact(list));
	}
	
	/**
	 * iteratedLoop test for loop body with the Character parameter.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_iteratedLoop_CharType_NullIterator() throws Throwable {
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Body_CharType", MethodType.methodType(String.class, String.class, Character.class));
		List<Character> list = Arrays.asList('A', 'B', 'C', 'D','E');
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(null, ITERATEDLOOP_INIT_ALLTYPES, mhBody);
		Assert.assertEquals("E D C B A ", (String)mhIteratedLoop.invokeExact((Iterable<Character>)list));
	}
	
	/**
	 * iteratedLoop test for loop body with the Character parameter when iterator exists.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_iteratedLoop_CharType_NonNullIterator() throws Throwable {
		MethodHandle mhIterator = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Iterator_CharType", MethodType.methodType(Iterator.class, List.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Body_CharType", MethodType.methodType(String.class, String.class, Character.class));
		List<Character> list = Arrays.asList('A', 'B', 'C', 'D','E');
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(mhIterator, ITERATEDLOOP_INIT_ALLTYPES, mhBody);
		Assert.assertEquals("E D C B A ", (String)mhIteratedLoop.invokeExact(list));
	}
	
	/**
	 * iteratedLoop test for loop body with the Short parameter.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_iteratedLoop_ShortType_NullIterator() throws Throwable {
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Body_ShortType", MethodType.methodType(String.class, String.class, Short.class));
		List<Short> list = Arrays.asList((short)1, (short)2, (short)3, (short)4,(short)5);
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(null, ITERATEDLOOP_INIT_ALLTYPES, mhBody);
		Assert.assertEquals("5 4 3 2 1 ", (String)mhIteratedLoop.invokeExact((Iterable<Short>)list));
	}
	
	/**
	 * iteratedLoop test for loop body with the Short parameter when iterator exists.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_iteratedLoop_ShortType_NonNullIterator() throws Throwable {
		MethodHandle mhIterator = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Iterator_ShortType", MethodType.methodType(Iterator.class, List.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Body_ShortType", MethodType.methodType(String.class, String.class, Short.class));
		List<Short> list = Arrays.asList((short)1, (short)2, (short)3, (short)4,(short)5);
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(mhIterator, ITERATEDLOOP_INIT_ALLTYPES, mhBody);
		Assert.assertEquals("5 4 3 2 1 ", (String)mhIteratedLoop.invokeExact(list));
	}
	
	/**
	 * iteratedLoop test for loop body with the Integer parameter.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_iteratedLoop_IntType_NullIterator() throws Throwable {
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Body_IntType", MethodType.methodType(String.class, String.class, Integer.class));
		List<Integer> list = Arrays.asList(10, 20, 30, 40, 50);
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(null, ITERATEDLOOP_INIT_ALLTYPES, mhBody);
		Assert.assertEquals("50 40 30 20 10 ", (String)mhIteratedLoop.invokeExact((Iterable<Integer>)list));
	}
	
	/**
	 * iteratedLoop test for loop body with the Short parameter when iterator exists.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_iteratedLoop_IntType_NonNullIterator() throws Throwable {
		MethodHandle mhIterator = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Iterator_IntType", MethodType.methodType(Iterator.class, List.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Body_IntType", MethodType.methodType(String.class, String.class, Integer.class));
		List<Integer> list = Arrays.asList(10, 20, 30, 40, 50);
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(mhIterator, ITERATEDLOOP_INIT_ALLTYPES, mhBody);
		Assert.assertEquals("50 40 30 20 10 ", (String)mhIteratedLoop.invokeExact(list));
	}
	
	/**
	 * iteratedLoop test for loop body with the Long parameter.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_iteratedLoop_LongType_NullIterator() throws Throwable {
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Body_LongType", MethodType.methodType(String.class, String.class, Long.class));
		List<Long> list = Arrays.asList(100l, 200l, 300l, 400l, 500l);
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(null, ITERATEDLOOP_INIT_ALLTYPES, mhBody);
		Assert.assertEquals("500 400 300 200 100 ", (String)mhIteratedLoop.invokeExact((Iterable<Long>)list));
	}
	
	/**
	 * iteratedLoop test for loop body with the Long parameter when iterator exists.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_iteratedLoop_LongType_NonNullIterator() throws Throwable {
		MethodHandle mhIterator = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Iterator_LongType", MethodType.methodType(Iterator.class, List.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Body_LongType", MethodType.methodType(String.class, String.class, Long.class));
		List<Long> list = Arrays.asList(100l, 200l, 300l, 400l, 500l);
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(mhIterator, ITERATEDLOOP_INIT_ALLTYPES, mhBody);
		Assert.assertEquals("500 400 300 200 100 ", (String)mhIteratedLoop.invokeExact(list));
	}
	
	/**
	 * iteratedLoop test for loop body with the Float parameter.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_iteratedLoop_FloatType_NullIterator() throws Throwable {
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Body_FloatType", MethodType.methodType(String.class, String.class, Float.class));
		List<Float> list = Arrays.asList(1.2f, 3.4f, 5.6f, 7.8f, 9.9f);
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(null, ITERATEDLOOP_INIT_ALLTYPES, mhBody);
		Assert.assertEquals("9.9 7.8 5.6 3.4 1.2 ", (String)mhIteratedLoop.invokeExact((Iterable<Float>)list));
	}
	
	/**
	 * iteratedLoop test for loop body with the Float parameter when iterator exists.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_iteratedLoop_FloatType_NonNullIterator() throws Throwable {
		MethodHandle mhIterator = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Iterator_FloatType", MethodType.methodType(Iterator.class, List.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Body_FloatType", MethodType.methodType(String.class, String.class, Float.class));
		List<Float> list = Arrays.asList(1.2f, 3.4f, 5.6f, 7.8f, 9.9f);
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(mhIterator, ITERATEDLOOP_INIT_ALLTYPES, mhBody);
		Assert.assertEquals("9.9 7.8 5.6 3.4 1.2 ", (String)mhIteratedLoop.invokeExact(list));
	}
	
	/**
	 * iteratedLoop test for loop body with the Double parameter.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_iteratedLoop_DoubleType_NullIterator() throws Throwable {
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Body_DoubleType", MethodType.methodType(String.class, String.class, Double.class));
		List<Double> list = Arrays.asList(1.22d, 3.44d, 5.66d, 7.88d, 9.91d);
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(null, ITERATEDLOOP_INIT_ALLTYPES, mhBody);
		Assert.assertEquals("9.91 7.88 5.66 3.44 1.22 ", (String)mhIteratedLoop.invokeExact((Iterable<Double>)list));
	}
	
	/**
	 * iteratedLoop test for loop body with the Double parameter when iterator exists.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_iteratedLoop_DoubleType_NonNullIterator() throws Throwable {
		MethodHandle mhIterator = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Iterator_DoubleType", MethodType.methodType(Iterator.class, List.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Body_DoubleType", MethodType.methodType(String.class, String.class, Double.class));
		List<Double> list = Arrays.asList(1.22d, 3.44d, 5.66d, 7.88d, 9.91d);
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(mhIterator, ITERATEDLOOP_INIT_ALLTYPES, mhBody);
		Assert.assertEquals("9.91 7.88 5.66 3.44 1.22 ", (String)mhIteratedLoop.invokeExact(list));
	}
	
	/**
	 * iteratedLoop test for loop body with the String parameter.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_iteratedLoop_StringType_NullIterator() throws Throwable {
		MethodHandle mhInit = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Init_StringType", MethodType.methodType(List.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Body_StringType", MethodType.methodType(List.class, List.class, String.class));
		List<String> list = Arrays.asList("str1", "str2", "str3", "str4","str5");
		List<String> expectedList = Arrays.asList("STR5", "STR4", "STR3", "STR2", "STR1");
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(null, mhInit, mhBody);
		Assert.assertEquals(expectedList, (List<String>)mhIteratedLoop.invokeExact((Iterable<String>)list));
	}
	
	/**
	 * iteratedLoop test for loop body with the String parameter when iterator exists.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_iteratedLoop_StringType_NonNullIterator() throws Throwable {
		MethodHandle mhIterator = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Iterator_StringType", MethodType.methodType(Iterator.class, List.class));
		MethodHandle mhInit = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Init_StringType", MethodType.methodType(List.class));
		MethodHandle mhBody = lookup.findStatic(SamePackageExample.class, "iteratedLoop_Body_StringType", MethodType.methodType(List.class, List.class, String.class));
		List<String> list = Arrays.asList("str1", "str2", "str3", "str4","str5");
		List<String> expectedList = Arrays.asList("STR5", "STR4", "STR3", "STR2", "STR1");
		MethodHandle mhIteratedLoop = MethodHandles.iteratedLoop(mhIterator, mhInit, mhBody);
		Assert.assertEquals(expectedList, (List<String>)mhIteratedLoop.invokeExact(list));
	}
}
