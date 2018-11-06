/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
package org.openj9.test.condy;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import javax.management.RuntimeErrorException;

public class GarbageCollectionCondyTest {
	@Test(groups = { "level.sanity" })
	public void testPrimitiveCondyUnloading() {
		byte[] classBytes = DefineCondyClass.getPrimitiveCondyClassBytes("test/primitiveClass");
		runTest(classBytes, "primitiveClass");
		return;
	}

	@Test(groups = { "level.sanity" })
	public void testNullCondyUnloading() {
		byte[] classBytes = DefineCondyClass.getNullCondyClassBytes("test/nullClass");
		runTest(classBytes, "nullClass");
		return;
	}


	@Test(groups = { "level.sanity" })
	public void testExceptionCondyUnloading() {
		byte[] classBytes = DefineCondyClass.getExceptionCondyClassBytes("test/exceptionClass");
		runTest(classBytes, "exceptionClass");
		return;
	}
	
	private void runTest(byte[] classBytes, String className) {
		Class<?> condyClass = new CondyClassLoader().load("test." + className, classBytes);
		Object result1 = "Result1 initial value";
		try {
			Method condyMethod = condyClass.getDeclaredMethod("getCondy");
			result1 = condyMethod.invoke(condyClass);
		} catch (NoSuchMethodException e) {
			new RuntimeException("Error locating method: getCondy()");
		} catch (IllegalAccessException | IllegalArgumentException | InvocationTargetException e) {
			 new RuntimeException("Error calling method: getCondy()" + e);
		}
		System.gc();
		System.gc();
		try {
			Method condyMethod = condyClass.getDeclaredMethod("getCondy");
			Object result2 = "Result2 initial value";
			result2 = condyMethod.invoke(condyClass);
			Assert.assertEquals(result1, result2);

		} catch (NoSuchMethodException e) {
			new RuntimeException("Error locating method: getCondy()");
		} catch (IllegalAccessException | IllegalArgumentException | InvocationTargetException e) {
			new RuntimeException("Error calling method: getCondy()" + e);
		}
	}
}
