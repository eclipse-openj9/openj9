/*
 * Copyright IBM Corp. and others 2001
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
package org.openj9.test.unsafe;

import java.util.ArrayList;
import java.util.List;

import org.openj9.test.util.CompilerAccess;
import org.testng.AssertJUnit;
import org.testng.annotations.Factory;
import org.testng.log4testng.Logger;

public class MainTester {

	private static final Logger logger = Logger.getLogger(MainTester.class);
	public static final String REGULAR_RUN = "Regular";
	public static final String COMPILED = "Compiled";

	@Factory
	public Object[] createInstances() {
		String scenario = System.getProperty("Scenario");
		if (scenario.equals(COMPILED)) {
			logger.info("Compiling classes to ensure they are jitted.");
			compileClass();
		}
		List<Object> result = new ArrayList<>();
		result.add(new TestUnsafeAccess(scenario));
		result.add(new TestUnsafeAccessVolatile(scenario));
		result.add(new TestUnsafeAccessOpaque(scenario));
		result.add(new TestUnsafeAccessOrdered(scenario));
		result.add(new TestUnsafeAccessUnaligned(scenario));
		result.add(new TestUnsafeSetMemory(scenario));
		result.add(new TestUnsafeCopyMemory(scenario));
		result.add(new TestUnsafeCopySwapMemory(scenario));
		result.add(new TestUnsafePutGetAddress(scenario));
		result.add(new TestUnsafeCompareAndExchange(scenario));
		result.add(new TestUnsafeCompareAndSet(scenario));
		result.add(new TestUnsafeAllocateDirectByteBuffer(scenario));
		result.add(new TestUnsafeGetAndOp(scenario));
		return result.toArray();
	}

	private static void compileClass() {
		Class<?>[] classes = {MainTester.class, TestUnsafeAccess.class, TestUnsafeAccessOpaque.class,
				TestUnsafeAccessOrdered.class, TestUnsafeAccessVolatile.class, TestUnsafeAllocateDirectByteBuffer.class,
				TestUnsafeCopyMemory.class, TestUnsafeGetAndOp.class, TestUnsafePutGetAddress.class,
				TestUnsafeCompareAndExchange.class, TestUnsafeCompareAndSet.class, TestUnsafeSetMemory.class, UnsafeTestBase.class,
				TestUnsafeAccessUnaligned.class };

		for (Class<?> clazz : classes) {
			if (CompilerAccess.compileClass(clazz)) {
				logger.debug("Compiler.compileClass( " + clazz.getName() + " )");
			} else {
				logger.error("Compilation of " + clazz.getName() + " failed or compiler is not available -- aborting");
				AssertJUnit.fail();
			}
		}
	}

}
