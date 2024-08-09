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
import static jdk.incubator.foreign.CLinker.C_INT;
import static jdk.incubator.foreign.CLinker.C_POINTER;
import jdk.incubator.foreign.FunctionDescriptor;
import jdk.incubator.foreign.MemoryAccess;
import jdk.incubator.foreign.MemoryAddress;
import jdk.incubator.foreign.MemorySegment;
import jdk.incubator.foreign.ResourceScope;
import jdk.incubator.foreign.SymbolLookup;

/**
 * Test cases for JEP 389: Foreign Linker API (Incubator) for primitive types in downcall,
 * which verifies the downcalls with the same downcall handlder (cached as soft reference in OpenJDK)
 * in multithreading.
 */
@Test(groups = { "level.sanity" })
public class MultiThreadingTests1 implements Thread.UncaughtExceptionHandler {
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
	public void test_twoThreadsWithSameFuncDesc_SameDowncallHandler() throws Throwable {
		Thread thr1 = new Thread(){
			public void run() {
				try {
					try (ResourceScope scope = ResourceScope.newConfinedScope()) {
						MethodType mt = MethodType.methodType(int.class, int.class, MemoryAddress.class);
						FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_POINTER);
						Addressable functionSymbol = nativeLibLookup.lookup("addIntAndIntFromPointer").get();
						MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

						MemorySegment intSegmt = MemorySegment.allocateNative(C_INT, scope);
						MemoryAccess.setInt(intSegmt, 215);
						int result = (int)mh.invokeExact(321, intSegmt.address());
						Assert.assertEquals(result, 536);
					}
				} catch (Throwable t) {
					throw new RuntimeException(t);
				}
			}
		};

		Thread thr2 = new Thread(){
			public void run() {
				try {
					try (ResourceScope scope = ResourceScope.newConfinedScope()) {
						MethodType mt = MethodType.methodType(int.class, int.class, MemoryAddress.class);
						FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_POINTER);
						Addressable functionSymbol = nativeLibLookup.lookup("addIntAndIntFromPointer").get();
						MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

						MemorySegment intSegmt = MemorySegment.allocateNative(C_INT, scope);
						MemoryAccess.setInt(intSegmt, 215);
						int result = (int)mh.invokeExact(322, intSegmt.address());
						Assert.assertEquals(result, 537);
					}
				} catch (Throwable t) {
					throw new RuntimeException(t);
				}
			}
		};

		thr1.setUncaughtExceptionHandler(this);
		thr2.setUncaughtExceptionHandler(this);
		initException = null;

		thr1.start();
		thr2.start();

		thr1.join();
		thr2.join();


		if (initException != null){
			throw new RuntimeException(initException);
		}
	}
}
