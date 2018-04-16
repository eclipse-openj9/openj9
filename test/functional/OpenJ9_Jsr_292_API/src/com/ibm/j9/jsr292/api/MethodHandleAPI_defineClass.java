/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
package com.ibm.j9.jsr292.api;

import org.testng.annotations.Test;
import org.testng.Assert;
import java.lang.reflect.Method;
import java.lang.invoke.MethodHandles.Lookup;
import static java.lang.invoke.MethodHandles.lookup;
import com.ibm.j9.jsr292.ClassBytes;

@Test(groups = { "level.sanity" })
public class MethodHandleAPI_defineClass {
	final static Lookup lookup = lookup();
	
	/**
	 * defineClass test for a null array of class bytes
	 */
	@Test(expectedExceptions = NullPointerException.class)
	public static void test_defineClass_ClassBytes_NullArray() throws Throwable {
		Class<?> targetClass = lookup.defineClass(null);
		Assert.fail("The test case failed to detect a null array of class bytes");
	}
	
	/**
	 * defineClass test for a valid class byte array from a package different from the lookup class
	 */
	@Test(expectedExceptions = IllegalArgumentException.class)
	public static void test_defineClass_ClassBytes_DifferentPackage() throws Throwable {
		Class<?> targetClass = lookup.defineClass(ClassBytes.classBytes_DifferentPackage());
		Assert.fail("The test case failed to detect that the class byte array belongs to a package different from the lookup class");
	}
	
	/**
	 * defineClass test for a lookup without the PACKAGE access mode
	 */
	@Test(expectedExceptions = IllegalAccessException.class)
	public static void test_defineClass_Lookup_WithoutPackageMode() throws Throwable {
		Lookup newLookup = lookup.dropLookupMode(Lookup.PACKAGE);
		Class<?> targetClass = newLookup.defineClass(ClassBytes.validClassBytes_SamePackage());
		Assert.fail("The test case failed to detect that the lookup doesn't have the PACKAGE access mode");
	}
	
	/**
	 * defineClass test for a corrupted class byte array
	 */
	@Test(expectedExceptions = LinkageError.class)
	public static void test_defineClass_ClassBytes_CorruptedArray() throws Throwable {
		Class<?> targetClass = lookup.defineClass(ClassBytes.invalidClassBytes_SamePackage());
		Assert.fail("The test case failed to detect a corrupted class byte array");
	}
	
	/**
	 * defineClass test for a valid class byte array that shares the same class loader,
	 * the same runtime package and the protection domain as the lookup class.
	 * @throws Throwable
	 */
	public static void test_defineClass_ClassBytes_ValidArray() throws Throwable {
		Class<?> lookupClass = lookup.lookupClass();
		Class<?> targetClass = lookup.defineClass(ClassBytes.validClassBytes_SamePackage());
		
		Assert.assertEquals(targetClass.getPackage(), lookupClass.getPackage());
		Assert.assertEquals(targetClass.getClassLoader(), lookupClass.getClassLoader());
		Assert.assertEquals(targetClass.getProtectionDomain(), lookupClass.getProtectionDomain());
		
		Method targetMethod = targetClass.getDeclaredMethod("getClassName");
		Assert.assertEquals((String)targetMethod.invoke(targetClass), "com.ibm.j9.jsr292.api.SamePackageExample3");
	}
}