/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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
import org.testng.AssertJUnit;
import java.lang.invoke.MethodHandle;
import static java.lang.invoke.MethodHandles.Lookup;
import static java.lang.invoke.MethodHandles.lookup;
import static java.lang.invoke.MethodHandles.foldArguments;
import static java.lang.invoke.MethodType.methodType;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class MethodHandleAPI_foldArguments {
	final Lookup lookup = lookup();
	final String MHClassName = "java.lang.invoke.MethodHandles";

	/**
	 * foldArguments test with foldPosition specified at the head of the argument list
	 * in the case of a combinerHandle with void return type.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_CombinerHandle_VoidReturnType_FoldAtHead() throws Throwable {
		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(void.class, int.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, int.class, int.class, int.class, int.class, int.class, int.class));

		/* The fold position is specified at the head of argument list */
		MethodHandle mhResult = foldArguments(mhTarget, 0, mhCombiner);
		AssertJUnit.assertEquals("[1 2 3 4 5 6]", (String)mhResult.invokeExact(1, 2, 3, 4, 5, 6));
	}

	/**
	 * foldArguments test with foldPosition specified in the middle of argument list
	 * in the case of a combinerHandle with void return type.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_CombinerHandle_VoidReturnType_FoldInTheMiddle() throws Throwable {
		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(void.class, int.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, int.class, int.class, int.class, int.class, int.class, int.class));

		/* The fold position is specified in the middle of argument list */
		MethodHandle mhResult = foldArguments(mhTarget, 2, mhCombiner);
		AssertJUnit.assertEquals("[1 2 3 4 5 6]", (String)mhResult.invokeExact(1, 2, 3, 4, 5, 6));
	}

	/**
	 * foldArguments test with foldPosition specified at the end of the argument list
	 * in the case of a combinerHandle with void return type.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_CombinerHandle_VoidReturnType_FoldToEnd() throws Throwable {
		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(void.class, int.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, int.class, int.class, int.class, int.class, int.class, int.class));

		/* The parameters of combinerHandle start from the fold position to the end of argument list */
		MethodHandle mhResult = foldArguments(mhTarget, 5, mhCombiner);
		AssertJUnit.assertEquals("[1 2 3 4 5 6]", (String)mhResult.invokeExact(1, 2, 3, 4, 5, 6));
	}

	/**
	 * foldArguments test with foldPosition specified at the head of the argument list
	 * in the case of a combinerHandle with int return type and int parameters.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_CombinerHandle_IntReturnType_IntParam_FoldAtHead() throws Throwable {
		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(int.class, int.class, int.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, int.class, int.class, int.class, int.class, int.class, int.class));

		/* The fold position is specified at the head of argument list */
		MethodHandle mhResult = foldArguments(mhTarget, 0, mhCombiner);
		AssertJUnit.assertEquals("[3 1 2 3 4 5]", (String)mhResult.invokeExact(1, 2, 3, 4, 5));
	}

	/**
	 * foldArguments test with foldPosition specified in the middle of the argument list
	 * in the case of a combinerHandle with int return type and int parameters.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_CombinerHandle_IntReturnType_IntParam_FoldInTheMiddle() throws Throwable {
		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(int.class, int.class, int.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, int.class, int.class, int.class, int.class, int.class, int.class));

		/* The fold position is specified in the middle of argument list */
		MethodHandle mhResult = foldArguments(mhTarget, 1, mhCombiner);
		AssertJUnit.assertEquals("[1 5 2 3 4 5]", (String)mhResult.invokeExact(1, 2, 3, 4, 5));
	}

	/**
	 * foldArguments test with foldPosition specified close to the end of the argument list
	 * in the case of a combinerHandle with int return type and int parameters.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_CombinerHandle_IntReturnType_IntParam_FoldToEnd() throws Throwable {
		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(int.class, int.class, int.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, int.class, int.class, int.class, int.class, int.class, int.class));

		/* The parameters of combinerHandle start from the fold position to the end of argument list */
		MethodHandle mhResult = foldArguments(mhTarget, 3, mhCombiner);
		AssertJUnit.assertEquals("[1 2 3 9 4 5]", (String)mhResult.invokeExact(1, 2, 3, 4, 5));
	}

	/**
	 * foldArguments test with foldPosition specified at the head of the argument list
	 * in the case of a combinerHandle with long return type and long parameters.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_CombinerHandle_LongReturnType_LongParam_FoldAtHead() throws Throwable {
		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(long.class, long.class, long.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, long.class, long.class, long.class, long.class, long.class, long.class));

		/* The fold position is specified at the head of argument list */
		MethodHandle mhResult = foldArguments(mhTarget, 0, mhCombiner);
		AssertJUnit.assertEquals("[30 10 20 30 40 50]", (String)mhResult.invokeExact(10L, 20L, 30L, 40L, 50L));
	}

	/**
	 * foldArguments test with foldPosition specified in the middle of the argument list
	 * in the case of a combinerHandle with long return type and long parameters.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_CombinerHandle_LongReturnType_LongParam_FoldInTheMiddle() throws Throwable {
		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(long.class, long.class, long.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, long.class, long.class, long.class, long.class, long.class, long.class));

		/* The fold position is specified in the middle of argument list */
		MethodHandle mhResult = foldArguments(mhTarget, 1, mhCombiner);
		AssertJUnit.assertEquals("[10 50 20 30 40 50]", (String)mhResult.invokeExact(10L, 20L, 30L, 40L, 50L));
	}

	/**
	 * foldArguments test with foldPosition specified close to the end of the argument list
	 * in the case of a combinerHandle with long return type and long parameters.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_CombinerHandle_LongReturnType_LongParam_FoldToEnd() throws Throwable {
		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(long.class, long.class, long.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, long.class, long.class, long.class, long.class, long.class, long.class));

		/* The parameters of combinerHandle start from the fold position to the end of argument list */
		MethodHandle mhResult = foldArguments(mhTarget, 3, mhCombiner);
		AssertJUnit.assertEquals("[10 20 30 90 40 50]", (String)mhResult.invokeExact(10L, 20L, 30L, 40L, 50L));
	}

	/**
	 * foldArguments test with foldPosition specified at the head of the argument list
	 * in the case of a combinerHandle with String return type and String parameters.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_CombinerHandle_StringReturnType_StringParam_FoldAtHead() throws Throwable {
		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(String.class, String.class, String.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, String.class, String.class, String.class, String.class, String.class, String.class));

		/* The fold position is specified at the head of argument list */
		MethodHandle mhResult = foldArguments(mhTarget, 0, mhCombiner);
		AssertJUnit.assertEquals("[[arg1arg2] arg1 arg2 arg3 arg4 arg5]", (String)mhResult.invokeExact("arg1", "arg2", "arg3", "arg4", "arg5"));
	}

	/**
	 * foldArguments test with foldPosition specified in the middle of the argument list
	 * in the case of a combinerHandle with String return type and String parameters.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_CombinerHandle_StringReturnType_StringParam_FoldInTheMiddle() throws Throwable {
		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(String.class, String.class, String.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, String.class, String.class, String.class, String.class, String.class, String.class));

		/* The fold position is specified in the middle of argument list */
		MethodHandle mhResult = foldArguments(mhTarget, 1, mhCombiner);
		AssertJUnit.assertEquals("[arg1 [arg2arg3] arg2 arg3 arg4 arg5]", (String)mhResult.invokeExact("arg1", "arg2", "arg3", "arg4", "arg5"));
	}

	/**
	 * foldArguments test with foldPosition specified close to the end of the argument list
	 * in the case of a combinerHandle with String return type and String parameters.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_CombinerHandle_StringReturnType_StringParam_FoldToEnd() throws Throwable {
		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(String.class, String.class, String.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, String.class, String.class, String.class, String.class, String.class, String.class));

		/* The parameters of combinerHandle start from the fold position to the end of argument list */
		MethodHandle mhResult = foldArguments(mhTarget, 3, mhCombiner);
		AssertJUnit.assertEquals("[arg1 arg2 arg3 [arg4arg5] arg4 arg5]", (String)mhResult.invokeExact("arg1", "arg2", "arg3", "arg4", "arg5"));
	}

	/**
	 * The following test cases are intended for foldArguments with argument indices specified
	 * anywhere in the argument list of target handle. In such case, foldPosition is only
	 * used to specify the location of the return value from combiner handle in the argument list
	 * of target handle.
	 */

	/**
	 * foldArguments test with foldPosition specified in the middle of argument list
	 * in the case of a combinerHandle with void return type.
	 * Note: the test is to verify it throws out an exception if the passed-in argument index
	 * is the same location as the fold position.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_ArgIndices_CombinerHandle_VoidReturnType_InvalidArgIndex() throws Throwable {
		Class<?> cls = Class.forName(MHClassName);
		Method  method = cls.getDeclaredMethod("foldArguments", MethodHandle.class, int.class, MethodHandle.class, int[].class);
		method.setAccessible(true);

		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(int.class, int.class, int.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, int.class, int.class, int.class, int.class, int.class, int.class));

		/* The fold position is specified at the head of argument list */
		try {
			MethodHandle mhResult = (MethodHandle)method.invoke(cls, mhTarget, 2, mhCombiner, new int[]{2});
			AssertJUnit.fail("The test case must throw out an exception for the invaild argument index");
		} catch (InvocationTargetException e) {
			/* Success */
		}
	}

	/**
	 * foldArguments test with foldPosition specified in the middle of argument list
	 * in the case of a combinerHandle with void return type.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_ArgIndices_CombinerHandle_VoidReturnType_FoldInTheMiddle() throws Throwable {
		Class<?> cls = Class.forName(MHClassName);
		Method  method = cls.getDeclaredMethod("foldArguments", MethodHandle.class, int.class, MethodHandle.class, int[].class);
		method.setAccessible(true);

		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(void.class, int.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, int.class, int.class, int.class, int.class, int.class, int.class));

		/* The fold position is specified at the head of argument list */
		MethodHandle mhResult = (MethodHandle)method.invoke(cls, mhTarget, 3, mhCombiner, new int[]{4});
		AssertJUnit.assertEquals("[1 2 3 4 5 6]", (String)mhResult.invokeExact(1, 2, 3, 4, 5, 6));
	}

	/**
	 * foldArguments test with foldPosition specified at the head of the argument list
	 * in the case of a combinerHandle with int return type and int parameters.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_ArgIndices_CombinerHandle_IntReturnType_IntParam_FoldAtHead() throws Throwable {
		Class<?> cls = Class.forName(MHClassName);
		Method  method = cls.getDeclaredMethod("foldArguments", MethodHandle.class, int.class, MethodHandle.class, int[].class);
		method.setAccessible(true);

		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(int.class, int.class, int.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, int.class, int.class, int.class, int.class, int.class, int.class));

		/* The fold position is specified at the head of argument list */
		MethodHandle mhResult = (MethodHandle)method.invoke(cls, mhTarget, 0, mhCombiner, new int[]{4, 2});
		AssertJUnit.assertEquals("[6 1 2 3 4 5]", (String)mhResult.invokeExact(1, 2, 3, 4, 5));
	}

	/**
	 * foldArguments test with foldPosition specified in the middle of the argument list
	 * in the case of a combinerHandle with int return type and int parameters.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_ArgIndices_CombinerHandle_IntReturnType_IntParam_FoldInTheMiddle() throws Throwable {
		Class<?> cls = Class.forName(MHClassName);
		Method  method = cls.getDeclaredMethod("foldArguments", MethodHandle.class, int.class, MethodHandle.class, int[].class);
		method.setAccessible(true);

		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(int.class, int.class, int.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, int.class, int.class, int.class, int.class, int.class, int.class));

		/* The fold position is specified in the middle of argument list */
		MethodHandle mhResult = (MethodHandle)method.invoke(cls, mhTarget, 2, mhCombiner, new int[]{3, 1});
		AssertJUnit.assertEquals("[1 2 5 3 4 5]", (String)mhResult.invokeExact(1, 2, 3, 4, 5));
	}

	/**
	 * foldArguments test with foldPosition specified at the end of the argument list
	 * in the case of a combinerHandle with int return type and int parameters.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_ArgIndices_CombinerHandle_IntReturnType_IntParam_FoldAtEnd() throws Throwable {
		Class<?> cls = Class.forName(MHClassName);
		Method  method = cls.getDeclaredMethod("foldArguments", MethodHandle.class, int.class, MethodHandle.class, int[].class);
		method.setAccessible(true);

		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(int.class, int.class, int.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, int.class, int.class, int.class, int.class, int.class, int.class));

		/*  The fold position is specified at the end of argument list */
		MethodHandle mhResult = (MethodHandle)method.invoke(cls, mhTarget, 4, mhCombiner, new int[]{0, 1});
		AssertJUnit.assertEquals("[1 2 3 4 3 5]", (String)mhResult.invokeExact(1, 2, 3, 4, 5));
	}

	/**
	 * foldArguments test with foldPosition specified at the head of the argument list
	 * in the case of a combinerHandle with long return type and long parameters.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_ArgIndices_CombinerHandle_LongReturnType_LongParam_FoldAtHead() throws Throwable {
		Class<?> cls = Class.forName(MHClassName);
		Method  method = cls.getDeclaredMethod("foldArguments", MethodHandle.class, int.class, MethodHandle.class, int[].class);
		method.setAccessible(true);

		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(long.class, long.class, long.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, long.class, long.class, long.class, long.class, long.class, long.class));

		/* The fold position is specified at the head of argument list */
		MethodHandle mhResult = (MethodHandle)method.invoke(cls, mhTarget, 0, mhCombiner, new int[]{4, 2});
		AssertJUnit.assertEquals("[60 10 20 30 40 50]", (String)mhResult.invokeExact(10L, 20L, 30L, 40L, 50L));
	}

	/**
	 * foldArguments test with foldPosition specified in the middle of the argument list
	 * in the case of a combinerHandle with long return type and long parameters.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_ArgIndices_CombinerHandle_LongReturnType_LongParam_FoldInTheMiddle() throws Throwable {
		Class<?> cls = Class.forName(MHClassName);
		Method  method = cls.getDeclaredMethod("foldArguments", MethodHandle.class, int.class, MethodHandle.class, int[].class);
		method.setAccessible(true);

		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(long.class, long.class, long.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, long.class, long.class, long.class, long.class, long.class, long.class));

		/* The fold position is specified in the middle of argument list */
		MethodHandle mhResult = (MethodHandle)method.invoke(cls, mhTarget, 2, mhCombiner, new int[]{3, 1});
		AssertJUnit.assertEquals("[10 20 50 30 40 50]", (String)mhResult.invokeExact(10L, 20L, 30L, 40L, 50L));
	}

	/**
	 * foldArguments test with foldPosition specified at the end of the argument list
	 * in the case of a combinerHandle with long return type and long parameters.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_ArgIndices_CombinerHandle_LongReturnType_LongParam_FoldAtEnd() throws Throwable {
		Class<?> cls = Class.forName(MHClassName);
		Method  method = cls.getDeclaredMethod("foldArguments", MethodHandle.class, int.class, MethodHandle.class, int[].class);
		method.setAccessible(true);

		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(long.class, long.class, long.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, long.class, long.class, long.class, long.class, long.class, long.class));

		/*  The fold position is specified at the end of argument list */
		MethodHandle mhResult = (MethodHandle)method.invoke(cls, mhTarget, 4, mhCombiner, new int[]{0, 1});
		AssertJUnit.assertEquals("[10 20 30 40 30 50]", (String)mhResult.invokeExact(10L, 20L, 30L, 40L, 50L));
	}

	/**
	 * foldArguments test with foldPosition specified at the head of the argument list
	 * in the case of a combinerHandle with String return type and String parameters.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_ArgIndices_CombinerHandle_StringReturnType_StringParam_FoldAtHead() throws Throwable {
		Class<?> cls = Class.forName(MHClassName);
		Method  method = cls.getDeclaredMethod("foldArguments", MethodHandle.class, int.class, MethodHandle.class, int[].class);
		method.setAccessible(true);

		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(String.class, String.class, String.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, String.class, String.class, String.class, String.class, String.class, String.class));

		/* The fold position is specified at the head of argument list */
		MethodHandle mhResult = (MethodHandle)method.invoke(cls, mhTarget, 0, mhCombiner, new int[]{4, 2});
		AssertJUnit.assertEquals("[[arg4arg2] arg1 arg2 arg3 arg4 arg5]", (String)mhResult.invokeExact("arg1", "arg2", "arg3", "arg4", "arg5"));
	}

	/**
	 * foldArguments test with foldPosition specified in the middle of the argument list
	 * in the case of a combinerHandle with String return type and String parameters.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_ArgIndices_CombinerHandle_StringReturnType_StringParam_FoldInTheMiddle() throws Throwable {
		Class<?> cls = Class.forName(MHClassName);
		Method  method = cls.getDeclaredMethod("foldArguments", MethodHandle.class, int.class, MethodHandle.class, int[].class);
		method.setAccessible(true);

		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(String.class, String.class, String.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, String.class, String.class, String.class, String.class, String.class, String.class));

		/* The fold position is specified in the middle of argument list */
		MethodHandle mhResult = (MethodHandle)method.invoke(cls, mhTarget, 2, mhCombiner, new int[]{3, 1});
		AssertJUnit.assertEquals("[arg1 arg2 [arg3arg2] arg3 arg4 arg5]", (String)mhResult.invokeExact("arg1", "arg2", "arg3", "arg4", "arg5"));
	}

	/**
	 * foldArguments test with foldPosition specified at the end of the argument list
	 * in the case of a combinerHandle with String return type and String parameters.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_ArgIndices_CombinerHandle_StringReturnType_StringParam_FoldAtEnd() throws Throwable {
		Class<?> cls = Class.forName(MHClassName);
		Method  method = cls.getDeclaredMethod("foldArguments", MethodHandle.class, int.class, MethodHandle.class, int[].class);
		method.setAccessible(true);

		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(String.class, String.class, String.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, String.class, String.class, String.class, String.class, String.class, String.class));

		/*  The fold position is specified at the end of argument list */
		MethodHandle mhResult = (MethodHandle)method.invoke(cls, mhTarget, 4, mhCombiner, new int[]{0, 1});
		AssertJUnit.assertEquals("[arg1 arg2 arg3 arg4 [arg1arg2] arg5]", (String)mhResult.invokeExact("arg1", "arg2", "arg3", "arg4", "arg5"));
	}

	/**
	 * foldArguments test with foldPosition specified in the middle of the argument list
	 * in the case of a combinerHandle with int return type and int parameters.
	 * Note: The specified argument indices in this case are equal to the indices starting from
	 * the fold position, in which case VM/JIT addresses it in the same way as an empty array
	 * of argument indices.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_ArgPos_EquivalentOfEmptyArray_CombinerHandle_IntReturnType_IntParam_FoldInTheMiddle() throws Throwable {
		Class<?> cls = Class.forName(MHClassName);
		Method  method = cls.getDeclaredMethod("foldArguments", MethodHandle.class, int.class, MethodHandle.class, int[].class);
		method.setAccessible(true);

		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(int.class, int.class, int.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, int.class, int.class, int.class, int.class, int.class, int.class));

		/* The fold position is specified in the middle of argument list */
		MethodHandle mhResult = (MethodHandle)method.invoke(cls, mhTarget, 2, mhCombiner, new int[]{3, 4});
		AssertJUnit.assertEquals("[1 2 7 3 4 5]", (String)mhResult.invokeExact(1, 2, 3, 4, 5));
	}

	/**
	 * foldArguments test with foldPosition specified in the middle of the argument list
	 * in the case of a combinerHandle with long return type and long parameters.
	 * Note: The specified argument indices in this case are equal to the indices starting from
	 * the fold position, in which case VM addresses it in the same way as an empty array
	 * of argument indices.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_ArgPos_EquivalentOfEmptyArray_CombinerHandle_LongReturnType_LongParam_FoldInTheMiddle() throws Throwable {
		Class<?> cls = Class.forName(MHClassName);
		Method  method = cls.getDeclaredMethod("foldArguments", MethodHandle.class, int.class, MethodHandle.class, int[].class);
		method.setAccessible(true);

		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(long.class, long.class, long.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, long.class, long.class, long.class, long.class, long.class, long.class));

		/* The fold position is specified in the middle of argument list */
		MethodHandle mhResult = (MethodHandle)method.invoke(cls, mhTarget, 2, mhCombiner, new int[]{3, 4});
		AssertJUnit.assertEquals("[10 20 70 30 40 50]", (String)mhResult.invokeExact(10L, 20L, 30L, 40L, 50L));
	}

	/**
	 * foldArguments test with foldPosition specified in the middle of the argument list
	 * in the case of a combinerHandle with String return type and String parameters.
	 * Note: The specified argument indices in this case are equal to the indices starting from
	 * the fold position, in which case VM addresses it in the same way as an empty array
	 * of argument indices.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_foldArguments_ArgPos_EquivalentOfEmptyArray_CombinerHandle_StringReturnType_StringParam_FoldInTheMiddle() throws Throwable {
		Class<?> cls = Class.forName(MHClassName);
		Method  method = cls.getDeclaredMethod("foldArguments", MethodHandle.class, int.class, MethodHandle.class, int[].class);
		method.setAccessible(true);

		MethodHandle mhCombiner = lookup.findStatic(SamePackageExample.class, "combinerMethod", methodType(String.class, String.class, String.class));
		MethodHandle mhTarget = lookup.findStatic(SamePackageExample.class, "targetMethod", methodType(String.class, String.class, String.class, String.class, String.class, String.class, String.class));

		/* The fold position is specified in the middle of argument list */
		MethodHandle mhResult = (MethodHandle)method.invoke(cls, mhTarget, 2, mhCombiner, new int[]{3, 4});
		AssertJUnit.assertEquals("[arg1 arg2 [arg3arg4] arg3 arg4 arg5]", (String)mhResult.invokeExact("arg1", "arg2", "arg3", "arg4", "arg5"));
	}
}
