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
package org.openj9.test.jep389.upcall;

import org.testng.Assert;
import org.testng.annotations.Test;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;

import jdk.incubator.foreign.Addressable;
import jdk.incubator.foreign.CLinker;
import static jdk.incubator.foreign.CLinker.C_INT;
import static jdk.incubator.foreign.CLinker.C_POINTER;
import jdk.incubator.foreign.FunctionDescriptor;
import jdk.incubator.foreign.MemoryAddress;
import jdk.incubator.foreign.ResourceScope;
import jdk.incubator.foreign.SymbolLookup;

/**
 * Test cases for JEP 389: Foreign Linker API (Incubator) intended for
 * the situation when the multi-threading specific upcalls happen to the same
 * upcall method handle within different resource scopes, in which case the upcall
 * metadata and the generated thunk are allocated separately.
 */
@Test(groups = { "level.sanity" })
public class MultiUpcallThrdsMHTests1 implements Thread.UncaughtExceptionHandler {
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
	public void test_multiUpcallThrdsWithDiffScopes() throws Throwable {
		Thread thr1 = new Thread(){
			public void run() {
				try {
					MethodType mt = MethodType.methodType(int.class, int.class, int.class, MemoryAddress.class);
					FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT, C_POINTER);
					Addressable functionSymbol = nativeLibLookup.lookup("add2IntsByUpcallMH").get();
					MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

					try (ResourceScope scope = ResourceScope.newConfinedScope()) {
						MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2Ints,
								FunctionDescriptor.of(C_INT, C_INT, C_INT), scope);
						int result = (int)mh.invokeExact(111112, 111123, upcallFuncAddr);
						Assert.assertEquals(result, 222235);
					}
				} catch (Throwable t) {
					throw new RuntimeException(t);
				}
			}
		};
		thr1.setUncaughtExceptionHandler(this);
		initException = null;
		thr1.start();

		Thread thr2 = new Thread(){
			public void run() {
				try {
					MethodType mt = MethodType.methodType(int.class, int.class, int.class, MemoryAddress.class);
					FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT, C_POINTER);
					Addressable functionSymbol = nativeLibLookup.lookup("add2IntsByUpcallMH").get();
					MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

					try (ResourceScope scope = ResourceScope.newConfinedScope()) {
						MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2Ints,
								FunctionDescriptor.of(C_INT, C_INT, C_INT), scope);
						int result = (int)mh.invokeExact(111113, 111124, upcallFuncAddr);
						Assert.assertEquals(result, 222237);
					}
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
					MethodType mt = MethodType.methodType(int.class, int.class, int.class, MemoryAddress.class);
					FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT, C_POINTER);
					Addressable functionSymbol = nativeLibLookup.lookup("add2IntsByUpcallMH").get();
					MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

					try (ResourceScope scope = ResourceScope.newConfinedScope()) {
						MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2Ints,
								FunctionDescriptor.of(C_INT, C_INT, C_INT), scope);
						int result = (int)mh.invokeExact(111114, 111125, upcallFuncAddr);
						Assert.assertEquals(result, 222239);
					}
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
