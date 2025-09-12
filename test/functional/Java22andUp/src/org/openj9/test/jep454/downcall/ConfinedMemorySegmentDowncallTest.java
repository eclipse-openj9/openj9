/*
 * Copyright IBM Corp. and others 2025
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
package org.openj9.test.jep454.downcall;

import org.testng.annotations.Test;

import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.GroupLayout;
import java.lang.foreign.Linker;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemoryLayout.PathElement;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.SymbolLookup;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.VarHandle;

import static java.lang.foreign.ValueLayout.ADDRESS;
import static java.lang.foreign.ValueLayout.JAVA_INT;
import static org.testng.Assert.fail;

/**
 * Test case for JEP 454: Verify WrongThreadException when accessing a
 * confined Arena from a different thread than the one it was created on.
 */
@Test(groups = { "level.sanity" })
public class ConfinedMemorySegmentDowncallTest {

	static {
		System.loadLibrary("clinkerffitests");
	}

	private static final Linker linker = Linker.nativeLinker();
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();

	@Test
	public void test_confinedSegmentWrongThreadAccess() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(
				JAVA_INT.withName("num1"),
				JAVA_INT.withName("num2")
		);
		VarHandle vh1 = structLayout.varHandle(PathElement.groupElement("num1"));
		VarHandle vh2 = structLayout.varHandle(PathElement.groupElement("num2"));

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegment = arena.allocate(structLayout);
			vh1.set(structSegment, 0L, 123);
			vh2.set(structSegment, 0L, 456);
			Object[] result = new Object[1];

			Thread t1 = new Thread(() -> {
				MemorySegment functionSymbol =
						nativeLibLookup.find("addIntAndIntsFromStructPointer").get();
				FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS);
				MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

				try {
					result[0] = (int) mh.invokeExact(42, structSegment);
				} catch (Throwable t) {
					result[0] = t;
				}
			});
			t1.start();
			t1.join();

			if (result[0] instanceof WrongThreadException) {
				// expected
			} else if (result[0] instanceof Throwable) {
				fail("Unexpected exception type: " + result[0]);
			} else {
				fail("Expected WrongThreadException was not thrown (result=" + result[0] + ")");
			}
		}
	}
}
