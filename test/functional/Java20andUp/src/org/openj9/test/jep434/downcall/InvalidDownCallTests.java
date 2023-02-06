/*******************************************************************************
 * Copyright IBM Corp. and others 2023
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
package org.openj9.test.jep434.downcall;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import static org.testng.Assert.fail;

import java.lang.invoke.MethodHandle;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.VarHandle;

import java.lang.foreign.Arena;
import java.lang.foreign.Linker;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.GroupLayout;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemoryLayout.PathElement;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.SegmentAllocator;
import java.lang.foreign.SequenceLayout;
import java.lang.foreign.SymbolLookup;
import static java.lang.foreign.ValueLayout.*;

/**
 * Test cases for JEP 434: Foreign Linker API (Second Preview) for primitive types in downcall,
 * which verify the illegal cases including unsupported layouts, etc.
 * Note: the majority of illegal cases are removed given the corresponding method type
 * is deduced from the function descriptor which is verified in OpenJDK.
 */
@Test(groups = { "level.sanity" })
public class InvalidDownCallTests {
	private static Linker linker = Linker.nativeLinker();

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();
	private static final SymbolLookup defaultLibLookup = linker.defaultLookup();

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Unsupported layout.*")
	public void test_invalidMemoryLayoutForIntType() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(JAVA_INT, MemoryLayout.paddingLayout(32));
		MemorySegment functionSymbol = nativeLibLookup.find("add2IntsReturnVoid").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the invalid MemoryLayout");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Unsupported layout.*")
	public void test_invalidMemoryLayoutForMemAddr() throws Throwable {
		MemorySegment functionSymbol = defaultLibLookup.find("strlen").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, MemoryLayout.paddingLayout(64));
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the invalid MemoryLayout");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Unsupported layout.*")
	public void test_invalidMemoryLayoutForReturnType() throws Throwable {
		MemorySegment functionSymbol = defaultLibLookup.find("strlen").get();
		FunctionDescriptor fd = FunctionDescriptor.of(MemoryLayout.paddingLayout(64), JAVA_LONG);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the invalid MemoryLayout");
	}
}
