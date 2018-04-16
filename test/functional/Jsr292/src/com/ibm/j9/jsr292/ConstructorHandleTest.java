/*******************************************************************************
 * Copyright (c) 2014, 2018 IBM Corp. and others
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
package com.ibm.j9.jsr292;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.reflect.Field;

/**
 * A class to check if a specific method is correctly compiled by JIT
 */
public class ConstructorHandleTest {

	@Test(groups = { "level.extended" })
	public void testIfConstructorHandleIsJitted() throws Throwable {
		try {
			new TestClass();
			MethodHandle mh = MethodHandles.lookup().findConstructor(TestClass.class, MethodType.methodType(void.class));
			AssertJUnit.assertTrue(mh.getClass().toString().equals("class java.lang.invoke.ConstructorHandle"));
			try {
				while (true) {
					@SuppressWarnings("unused") 
					TestClass t = (TestClass)mh.invokeExact();
				}
			} catch (Error t) {
				t.printStackTrace();
			}
		} catch (JittingDetector j) {
			AssertJUnit.assertTrue(true);
		}
	}
}

class TestClass {

	static long PC = 0;

	static final Field throwable_walkback;

	static {
		Field f;
		try {
			f = Throwable.class.getDeclaredField("walkback");
			f.setAccessible(true);
			throwable_walkback = f;
		} catch (NoSuchFieldException | SecurityException e) {
			throw new RuntimeException();
		}
	}

	public TestClass() throws Throwable {
		Throwable t = new Throwable();
		Object walkback = throwable_walkback.get(t);
		if (walkback instanceof int[]) {
			int[] iWalkback = (int[])walkback;
			if (PC == 0) {
				PC = iWalkback[0];
			} else if (PC != iWalkback[0]) {
				throw new JittingDetector("detected jitting");
			}
		} else {
			long[] iWalkback = (long[])walkback;
			if (PC == 0) {
				PC = iWalkback[0];
			} else if (PC != iWalkback[0]) {
				throw new JittingDetector("detected jitting");
			}
		}
	}
}
