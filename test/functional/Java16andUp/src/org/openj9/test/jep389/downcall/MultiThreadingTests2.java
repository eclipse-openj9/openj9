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
 * which verifies the downcalls with the diffrent layouts and arguments/return types in multithreading.
 */
@Test(groups = { "level.sanity" })
public class MultiThreadingTests2 implements Thread.UncaughtExceptionHandler {
	private static LibraryLookup nativeLib = LibraryLookup.ofLibrary("clinkerffitests");
	private static CLinker clinker = CLinker.getInstance();
	private volatile Throwable initException;
	
	@Test(enabled=false)
	@Override
	public void uncaughtException(Thread thr, Throwable t) {
		initException =  t;
	}
	
	@Test
	public void test_twoThreadsWithDiffFuncDescriptor() throws Throwable {
		Thread thr1 = new Thread(){
			public void run() {
				try {
					MethodType mt = MethodType.methodType(int.class, int.class, int.class);
					FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT);
					Symbol functionSymbol = nativeLib.lookup("add2Ints").get();
					MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
					int result = (int)mh.invokeExact(112, 123);
					Assert.assertEquals(result, 235);
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
					int result = (int)mh.invokeExact(112, 123, 235);
					Assert.assertEquals(result, 470);
				} catch (Throwable t) {
					throw new RuntimeException(t);
				}
			}
		};
		
		thr1.setUncaughtExceptionHandler(this);
		thr2.setUncaughtExceptionHandler(this);
		
		thr1.start();
		thr2.start();
		
		thr1.join();
		thr2.join();
		
		if (initException != null){
			throw new RuntimeException(initException);
		}
	}
}
