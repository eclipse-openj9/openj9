/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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
package org.openj9.test.jep389.downcall;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;
import jdk.incubator.foreign.CLinker;
import static jdk.incubator.foreign.CLinker.*;
import jdk.incubator.foreign.FunctionDescriptor;
import jdk.incubator.foreign.LibraryLookup;
import static jdk.incubator.foreign.LibraryLookup.Symbol;

/**
 * Test cases for JEP 389: Foreign Linker API (Incubator) DownCall for primitive types,
 * which verifies multiple downcalls combined with the diffrent layouts/arguments/return types in multithreading.
 */
@Test(groups = { "level.sanity" })
public class MultiThreadingTests4 implements Thread.UncaughtExceptionHandler {
	private static LibraryLookup nativeLib = LibraryLookup.ofLibrary("clinkerffitests");
	private static CLinker clinker = CLinker.getInstance();
	private volatile Throwable initException;
	
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
					Symbol functionSymbol = nativeLib.lookup("add2Ints").get();
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
					Symbol functionSymbol = nativeLib.lookup("add3Ints").get();
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
					FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT);
					Symbol functionSymbol = nativeLib.lookup("add2BoolsWithOr").get();
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
					Symbol functionSymbol = nativeLib.lookup("add2Ints").get();
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
					Symbol functionSymbol = nativeLib.lookup("add3Ints").get();
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
					FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT);
					Symbol functionSymbol = nativeLib.lookup("add2BoolsWithOr").get();
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
