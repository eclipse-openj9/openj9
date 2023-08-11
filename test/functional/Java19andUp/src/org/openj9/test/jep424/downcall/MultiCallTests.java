/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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
 *******************************************************************************/
package org.openj9.test.jep424.downcall;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;

import java.lang.invoke.MethodHandle;
import java.lang.foreign.Addressable;
import java.lang.foreign.Linker;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.SymbolLookup;
import static java.lang.foreign.ValueLayout.*;

/**
 * Test cases for JEP 424: Foreign Linker API (Preview) for primitive types in downcall,
 * which verifies multiple downcalls with the same or different layouts or argument/return types.
 */
@Test(groups = { "level.sanity" })
public class MultiCallTests {
	private static Linker linker = Linker.nativeLinker();

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();

	@Test
	public void test_twoCallsWithSameFuncDescriptor() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Ints").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);
		int result = (int)mh.invokeExact(112, 123);
		Assert.assertEquals(result, 235);

		mh = linker.downcallHandle(functionSymbol, fd);
		result = (int)mh.invokeExact(235, 439);
		Assert.assertEquals(result, 674);
	}

	@Test
	public void test_twoCallsWithDiffFuncDescriptor() throws Throwable {
		FunctionDescriptor fd1 = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT);
		Addressable functionSymbol1 = nativeLibLookup.lookup("add2Ints").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol1, fd1);
		int result = (int)mh.invokeExact(112, 123);
		Assert.assertEquals(result, 235);

		FunctionDescriptor fd2 = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT, JAVA_INT);
		Addressable functionSymbol2 = nativeLibLookup.lookup("add3Ints").get();
		mh = linker.downcallHandle(functionSymbol2, fd2);
		result = (int)mh.invokeExact(112, 123, 235);
		Assert.assertEquals(result, 470);
	}

	@Test
	public void test_multiCallsWithMixedFuncDescriptors() throws Throwable {
		FunctionDescriptor fd1 = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT);
		Addressable functionSymbol1 = nativeLibLookup.lookup("add2Ints").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol1, fd1);
		int result = (int)mh.invokeExact(112, 123);
		Assert.assertEquals(result, 235);

		FunctionDescriptor fd2 = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT, JAVA_INT);
		Addressable functionSymbol2 = nativeLibLookup.lookup("add3Ints").get();
		mh = linker.downcallHandle(functionSymbol2, fd2);
		result = (int)mh.invokeExact(112, 123, 235);
		Assert.assertEquals(result, 470);

		FunctionDescriptor fd3 = FunctionDescriptor.ofVoid(JAVA_INT, JAVA_INT);
		Addressable functionSymbol3 = nativeLibLookup.lookup("add2IntsReturnVoid").get();
		mh = linker.downcallHandle(functionSymbol3, fd3);
		mh.invokeExact(454, 398);

		mh = linker.downcallHandle(functionSymbol1, fd1);
		result = (int)mh.invokeExact(234, 567);
		Assert.assertEquals(result, 801);

		mh = linker.downcallHandle(functionSymbol2, fd2);
		result = (int)mh.invokeExact(312, 323, 334);
		Assert.assertEquals(result, 969);

		mh = linker.downcallHandle(functionSymbol3, fd3);
		mh.invokeExact(539, 672);
	}

	@Test
	public void test_twoCallsWithDiffReturnType() throws Throwable {
		FunctionDescriptor fd1 = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT);
		Addressable functionSymbol1 = nativeLibLookup.lookup("add2Ints").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol1, fd1);
		int result = (int)mh.invokeExact(112, 123);
		Assert.assertEquals(result, 235);

		FunctionDescriptor fd2 = FunctionDescriptor.ofVoid(JAVA_INT, JAVA_INT);
		Addressable functionSymbol2 = nativeLibLookup.lookup("add2IntsReturnVoid").get();
		mh = linker.downcallHandle(functionSymbol2, fd2);
		mh.invokeExact(454, 398);
	}

	@Test
	public void test_multiCallsWithSameArgLayouts() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT);
		Addressable functionSymbol1 = nativeLibLookup.lookup("add2Ints").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol1, fd);
		int intResult = (int)mh.invokeExact(112, 123);
		Assert.assertEquals(intResult, 235);

		mh = linker.downcallHandle(functionSymbol1, fd);
		intResult = (int)mh.invokeExact(234, 567);
		Assert.assertEquals(intResult, 801);

		FunctionDescriptor fd2 = FunctionDescriptor.ofVoid(JAVA_INT, JAVA_INT);
		Addressable functionSymbol2 = nativeLibLookup.lookup("add2IntsReturnVoid").get();
		mh = linker.downcallHandle(functionSymbol2, fd2);
		mh.invokeExact(454, 398);
	}
}
