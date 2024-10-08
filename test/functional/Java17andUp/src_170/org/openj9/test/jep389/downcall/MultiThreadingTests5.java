/*
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
 */
package org.openj9.test.jep389.downcall;

import org.testng.Assert;
import org.testng.annotations.Test;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;

import jdk.incubator.foreign.Addressable;
import jdk.incubator.foreign.CLinker;
import static jdk.incubator.foreign.CLinker.C_CHAR;
import static jdk.incubator.foreign.CLinker.C_INT;
import jdk.incubator.foreign.FunctionDescriptor;
import jdk.incubator.foreign.SymbolLookup;

/**
 * Test cases for JEP 389: Foreign Linker API (Incubator) for primitive types in downcall,
 * which verifies multiple downcalls combined with the diffrent layouts/arguments/return types in multithreading.
 */
@Test(groups = { "level.sanity" })
public class MultiThreadingTests5 implements Thread.UncaughtExceptionHandler {
	private volatile Throwable initException;
	private static CLinker clinker = CLinker.getInstance();

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();

	@Test(enabled=false)
	@Override
	public void uncaughtException(Thread thr, Throwable t) {
		initException =  t;
	}

	@Test
	public void test_multiThreadsWithMixedFuncDescriptors() throws Throwable {
		Thread thr1 = new Thread(){
			public void run() {
				try {
					MethodType mt = MethodType.methodType(int.class, int.class, int.class);
					FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT);
					Addressable functionSymbol = nativeLibLookup.lookup("add2Ints").get();
					MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
					int result = (int)mh.invokeExact(128, 246);
					Assert.assertEquals(result, 374);
				} catch (Throwable t) {
					throw new RuntimeException(t);
				}
			}
		};

		Thread thr2 = new Thread(){
			public void run() {
				try {
					MethodType mt = MethodType.methodType(int.class, int.class, int.class, int.class);
					FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT, C_INT);
					Addressable functionSymbol = nativeLibLookup.lookup("add3Ints").get();
					MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
					int result = (int)mh.invokeExact(112, 642, 971);
					Assert.assertEquals(result, 1725);
				} catch (Throwable t) {
					throw new RuntimeException(t);
				}
			}
		};

		Thread thr3 = new Thread(){
			public void run() {
				try {
					MethodType mt = MethodType.methodType(boolean.class, boolean.class, boolean.class);
					FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR);
					Addressable functionSymbol = nativeLibLookup.lookup("add2BoolsWithOr").get();
					MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
					boolean result = (boolean)mh.invokeExact(true, false);
					Assert.assertEquals(result, true);
				} catch (Throwable t) {
					throw new RuntimeException(t);
				}
			}
		};

		Thread thr4 = new Thread(){
			public void run() {
				try {
					MethodType mt = MethodType.methodType(int.class, int.class, int.class);
					FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT);
					Addressable functionSymbol = nativeLibLookup.lookup("add2Ints").get();
					MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
					int result = (int)mh.invokeExact(416, 728);
					Assert.assertEquals(result, 1144);
				} catch (Throwable t) {
					throw new RuntimeException(t);
				}
			}
		};

		Thread thr5 = new Thread(){
			public void run() {
				try {
					MethodType mt = MethodType.methodType(int.class, int.class, int.class, int.class);
					FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT, C_INT);
					Addressable functionSymbol = nativeLibLookup.lookup("add3Ints").get();
					MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
					int result = (int)mh.invokeExact(1012, 1023, 2035);
					Assert.assertEquals(result, 4070);
				} catch (Throwable t) {
					throw new RuntimeException(t);
				}
			}
		};

		Thread thr6 = new Thread(){
			public void run() {
				try {
					MethodType mt = MethodType.methodType(boolean.class, boolean.class, boolean.class);
					FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR);
					Addressable functionSymbol = nativeLibLookup.lookup("add2BoolsWithOr").get();
					MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
					boolean result = (boolean)mh.invokeExact(false, false);
					Assert.assertEquals(result, false);
				} catch (Throwable t) {
					throw new RuntimeException(t);
				}
			}
		};

		thr1.setUncaughtExceptionHandler(this);
		thr2.setUncaughtExceptionHandler(this);
		thr3.setUncaughtExceptionHandler(this);
		thr4.setUncaughtExceptionHandler(this);
		thr5.setUncaughtExceptionHandler(this);
		thr6.setUncaughtExceptionHandler(this);
		initException = null;

		thr1.start();
		thr2.start();
		thr3.start();
		thr4.start();
		thr5.start();
		thr6.start();

		thr6.join();
		thr5.join();
		thr4.join();
		thr3.join();
		thr2.join();
		thr1.join();

		if (initException != null){
			throw new RuntimeException(initException);
		}
	}
}
