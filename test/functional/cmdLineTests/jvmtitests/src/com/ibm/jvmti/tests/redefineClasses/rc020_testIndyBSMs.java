/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

package com.ibm.jvmti.tests.redefineClasses;

import java.lang.invoke.CallSite;
import java.lang.invoke.ConstantCallSite;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;

public class rc020_testIndyBSMs {
	private static int bsmCount = 0;

	public static CallSite bootstrapMethod1(MethodHandles.Lookup lUnused, String nUnused, MethodType tUnused) throws IllegalAccessException, NoSuchMethodException {
		MethodHandles.Lookup lookup = MethodHandles.lookup();
		MethodType type = MethodType.methodType(void.class);
		MethodHandle handle = lookup.findStatic(lookup.lookupClass(), "testMethod", type);

		/* Incremement bootstrap call counter */
		bsmCount++;
		
		return new ConstantCallSite(handle);
	}

	public static CallSite bootstrapMethod2(MethodHandles.Lookup lUnused, String nUnused, MethodType tUnused) throws IllegalAccessException, NoSuchMethodException {
		MethodHandles.Lookup lookup = MethodHandles.lookup();
		MethodType type = MethodType.methodType(void.class);
		MethodHandle handle = lookup.findStatic(lookup.lookupClass(), "testMethod", type);

		/* Incremement bootstrap call counter */
		bsmCount++;
		
		return new ConstantCallSite(handle);
	}

	public static CallSite bootstrapMethod3(MethodHandles.Lookup lUnused, String nUnused, MethodType tUnused, int iUnused) throws IllegalAccessException, NoSuchMethodException {
		MethodHandles.Lookup lookup = MethodHandles.lookup();
		MethodType type = MethodType.methodType(void.class);
		MethodHandle handle = lookup.findStatic(lookup.lookupClass(), "testMethod", type);

		/* Incremement bootstrap call counter */
		bsmCount++;
		
		return new ConstantCallSite(handle);
	}

	public static CallSite bootstrapMethod4(MethodHandles.Lookup lUnused, String nUnused, MethodType tUnused, String sUnused, int iUnused) throws IllegalAccessException, NoSuchMethodException {
		MethodHandles.Lookup lookup = MethodHandles.lookup();
		MethodType type = MethodType.methodType(void.class);
		MethodHandle handle = lookup.findStatic(lookup.lookupClass(), "testMethod", type);

		/* Incremement bootstrap call counter */
		bsmCount++;
		
		return new ConstantCallSite(handle);
	}

	public static CallSite bootstrapMethod5(MethodHandles.Lookup lUnused, String nUnused, MethodType tUnused, String sUnused, double dUnused) throws IllegalAccessException, NoSuchMethodException {
		MethodHandles.Lookup lookup = MethodHandles.lookup();
		MethodType type = MethodType.methodType(void.class);
		MethodHandle handle = lookup.findStatic(lookup.lookupClass(), "testMethod", type);

		/* Incremement bootstrap call counter */
		bsmCount++;
		
		return new ConstantCallSite(handle);
	}

	public static CallSite bootstrapMethod5(MethodHandles.Lookup lUnused, String nUnused, MethodType tUnused, int iUnused, double dUnused, String sUnused) throws IllegalAccessException, NoSuchMethodException {
		MethodHandles.Lookup lookup = MethodHandles.lookup();
		MethodType type = MethodType.methodType(void.class);
		MethodHandle handle = lookup.findStatic(lookup.lookupClass(), "testMethod", type);

		/* Incremement bootstrap call counter */
		bsmCount++;
		
		return new ConstantCallSite(handle);
	}

	static void testMethod() {
		/* do nothing */
	}

	/**
	 * Getter for number of time a bootstrap method 
	 * from this class has been invoked.
	 * 
	 * @return number of invoked bootstrap methods
	 */
	public static int bsmCallCount() {
		return bsmCount;
	}

	/**
	 * Reset the bootstrap method count to zero.
	 */
	public static void resetBsmCallCount() {
		bsmCount = 0;
	}
}
