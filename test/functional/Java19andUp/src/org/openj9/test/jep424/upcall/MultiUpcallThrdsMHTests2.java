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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package org.openj9.test.jep424.upcall;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;

import java.lang.invoke.MethodHandle;
import java.lang.foreign.Addressable;
import java.lang.foreign.Linker;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.MemoryAddress;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.MemorySession;
import java.lang.foreign.SymbolLookup;
import java.lang.foreign.ValueLayout;
import static java.lang.foreign.ValueLayout.*;

/**
 * Test cases for JEP 424: Foreign Linker API (Preview) intended for
 * the situation when the multi-threading specific upcalls happen to the same
 * upcall method handle within the same memory session, in which case the upcall
 * metadata and the generated thunk are only allocated once and shared among
 * these threads.
 */
@Test(groups = { "level.sanity" })
public class MultiUpcallThrdsMHTests2 implements Thread.UncaughtExceptionHandler {
	private volatile Throwable initException;
	private static Linker linker = Linker.nativeLinker();
	private static MemorySession session = MemorySession.openImplicit();

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
	public void test_multiUpcallThrdsWithSameSession() throws Throwable {
		Thread thr1 = new Thread(){
			public void run() {
				try {
					FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT, ADDRESS);
					Addressable functionSymbol = nativeLibLookup.lookup("add2IntsByUpcallMH").get();
					MethodHandle mh = linker.downcallHandle(functionSymbol, fd);
					MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2Ints,
							FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT), session);
					int result = (int)mh.invoke(111112, 111123, upcallFuncAddr);
					Assert.assertEquals(result, 222235);
				} catch (Throwable t) {
					throw new RuntimeException(t);
				}
			}
		};
		thr1.setUncaughtExceptionHandler(this);
		thr1.start();

		Thread thr2 = new Thread(){
			public void run() {
				try {
					FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT, ADDRESS);
					Addressable functionSymbol = nativeLibLookup.lookup("add2IntsByUpcallMH").get();
					MethodHandle mh = linker.downcallHandle(functionSymbol, fd);
					MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2Ints,
							FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT), session);
					int result = (int)mh.invoke(111113, 111124, upcallFuncAddr);
					Assert.assertEquals(result, 222237);
				} catch (Throwable t) {
					throw new RuntimeException(t);
				}
			}
		};
		thr2.setUncaughtExceptionHandler(this);
		thr2.start();

		Thread thr3 = new Thread(){
			public void run() {
				try {
					FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT, ADDRESS);
					Addressable functionSymbol = nativeLibLookup.lookup("add2IntsByUpcallMH").get();
					MethodHandle mh = linker.downcallHandle(functionSymbol, fd);
					MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2Ints,
							FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT), session);
					int result = (int)mh.invoke(111114, 111125, upcallFuncAddr);
					Assert.assertEquals(result, 222239);
				} catch (Throwable t) {
					throw new RuntimeException(t);
				}
			}
		};
		thr3.setUncaughtExceptionHandler(this);
		thr3.start();

		thr1.join();
		thr2.join();
		thr3.join();

		if (initException != null){
			throw new RuntimeException(initException);
		}
	}
}
