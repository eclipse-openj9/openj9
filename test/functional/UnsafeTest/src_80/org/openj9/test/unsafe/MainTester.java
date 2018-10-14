/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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
package org.openj9.test.unsafe;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.List;
import java.util.Vector;

import org.testng.annotations.Factory;
import org.testng.log4testng.Logger;

public class MainTester {

	private static Logger logger = Logger.getLogger(MainTester.class);
	public final static String REGULAR_RUN = "Regular";
	public final static String COMPILED = "Compiled";
	
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
		result.add(new TestUnsafeAccessOrdered(scenario));
		result.add(new TestUnsafeSetMemory(scenario));
		result.add(new TestUnsafeCopyMemory(scenario));
		result.add(new TestUnsafePutGetAddress(scenario));
		result.add(new TestCompareAndSwap(scenario));
		result.add(new TestUnsafeAllocateDirectByteBuffer(scenario));
		return result.toArray();
	}
	
	private static void compileClass() {
		try {
			String[] classNames = { "MainTester", "TestCompareAndSwap",
					"TestUnsafeAccess", "TestUnsafeAccessOrdered",
					"TestUnsafeAccessVolatile", "UnsafeTestBase",
					"TestUnsafeAllocateDirectByteBuffer" };

			for (int i = 0; i < classNames.length; i++) {
				Class clazz = Class.forName("org.openj9.test.unsafe."
						+ classNames[i]);
				if (clazz != null) {
					if (Compiler.compileClass(clazz) == false) {
						logger.error("Compilation of " + clazz.getName()
								+ " failed -- aborting");
					}else{
						logger.debug("Compiler.compileClass( "+  clazz.getName() + " )");
					}
				} else {
					logger.error("clazz is null");
				}
			}

		} catch (ClassNotFoundException ex) {
			logger.error(ex.toString(), ex);
		}
	}
}
