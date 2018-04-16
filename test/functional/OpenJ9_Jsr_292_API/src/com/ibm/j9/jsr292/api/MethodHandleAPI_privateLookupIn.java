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
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;
import static java.lang.invoke.MethodHandles.privateLookupIn;
import mods.modulea.package1.ModuleExample;
import mods.modulea.package1.AnotherModuleExample;
import mods.modulea.package2.SameModuleExample;
import mods.moduleb.package1.DifferentModuleExample1;
import mods.modulec.package1.DifferentModuleExample2;

public class MethodHandleAPI_privateLookupIn {
	final static Lookup callerLookup = ModuleExample.getLookup();
	final static Lookup callerPublicLookup = ModuleExample.getPublicLookup();
	
	/**
	 * privateLookupIn test for a null target class.
	 */
	@Test(expectedExceptions = NullPointerException.class, groups = { "level.sanity" })
	public static void test_privateLookupIn_NullTargetClass() throws Throwable {
		Lookup mhprivateLookupIn = privateLookupIn(null, callerLookup);
		Assert.fail("The test case failed to detect a null target class");
	}
	
	/**
	 * privateLookupIn test for a null caller lookup.
	 */
	@Test(expectedExceptions = NullPointerException.class, groups = { "level.sanity" })
	public static void test_privateLookupIn_NullcallerLookup() throws Throwable {
		Lookup mhprivateLookupIn = privateLookupIn(Object.class, null);
		Assert.fail("The test case failed to detect a null caller lookup");
	}
	
	/**
	 * privateLookupIn test for a target class with a primitive type.
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.sanity" })
	public static void test_privateLookupIn_TargetClass_PrimitiveType() throws Throwable {
		Lookup mhprivateLookupIn = privateLookupIn(int.class, callerLookup);
		Assert.fail("The test case failed to detect a target class with a primitive type");
	}
	
	/**
	 * privateLookupIn test for a target class with the array type.
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.sanity" })
	public static void test_privateLookupIn_TargetClass_ArrayType() throws Throwable {
		Lookup mhprivateLookupIn = privateLookupIn(Object[].class, callerLookup);
		Assert.fail("The test case failed to detect a target class with the array type");
	}
	
	/**
	 * privateLookupIn test for a named module containing the caller lookup
	 */
	@Test(expectedExceptions = IllegalAccessException.class, groups = { "level.sanity" })
	public static void test_privateLookupIn_callerLookup_NamedModule() throws Throwable {
		Lookup mhprivateLookupIn = privateLookupIn(DifferentModuleExample2.class, callerLookup);
		Assert.fail("The test case failed to detect that the module containing the target class is unreadable by"
				+ "the named module containing the caller lookup");
	}
	
	/**
	 * privateLookupIn test for an package (containing the target class) which is not opened to
	 * the module containing the caller lookup.
	 */
	@Test(expectedExceptions = IllegalAccessException.class, groups = { "level.sanity" })
	public static void test_privateLookupIn_TargetClass_UnOpenedPackage() throws Throwable {
		Lookup mhprivateLookupIn = privateLookupIn(Object.class, callerLookup);
		Assert.fail("The test case failed to detect the unopened package containing the target class");
	}
	
	/**
	 * privateLookupIn test for a caller lookup without the MODULE access mode
	 */
	@Test(expectedExceptions = IllegalAccessException.class, groups = { "level.sanity" })
	public static void test_privateLookupIn_Lookup_NoModuleMode() throws Throwable {
		Lookup mhprivateLookupIn = privateLookupIn(callerPublicLookup.lookupClass(), callerPublicLookup);
		Assert.fail("The test case failed to detect a caller lookup without the MODULE access mode");
	}
	
	/**
	 * privateLookupIn test for a target class which is the caller class in an unnamed module
	 */
	@Test(groups = { "level.sanity" })
	public static void test_privateLookupIn_TargetClass_Itself_UnnamedModule() throws Throwable {
		Lookup lookup = MethodHandles.lookup();
		Class<?> targetClass = lookup.lookupClass();
		Lookup mhprivateLookupIn = privateLookupIn(targetClass, lookup);
		Assert.assertEquals(mhprivateLookupIn.lookupClass(), targetClass);
		Assert.assertEquals(mhprivateLookupIn.lookupModes() & Lookup.PRIVATE, Lookup.PRIVATE);
	}
	
	/**
	 * privateLookupIn test for a target class which is the caller class in a named module
	 */
	@Test(groups = { "level.sanity" })
	public static void test_privateLookupIn_TargetClass_Itself_NamedModule() throws Throwable {
		Lookup mhprivateLookupIn = privateLookupIn(ModuleExample.class, callerLookup);
		Assert.assertEquals(mhprivateLookupIn.lookupClass(), ModuleExample.class);
		Assert.assertEquals(mhprivateLookupIn.lookupModes() & Lookup.PRIVATE, Lookup.PRIVATE);
	}
	
	/**
	 * privateLookupIn test for a target class and a caller lookup within the same package
	 */
	@Test(groups = { "level.sanity" })
	public static void test_privateLookupIn_TargetClass_callerLookup_SamePackage_SameModule() throws Throwable {
		Lookup mhprivateLookupIn = privateLookupIn(AnotherModuleExample.class, callerLookup);
		Assert.assertEquals(mhprivateLookupIn.lookupClass(), AnotherModuleExample.class);
		Assert.assertEquals(mhprivateLookupIn.lookupModes() & Lookup.PRIVATE, Lookup.PRIVATE);
	}
	
	/**
	 * privateLookupIn test for a target class and a caller lookup from different packages within the same module
	 */
	@Test(groups = { "level.sanity" })
	public static void test_privateLookupIn_TargetClass_callerLookup_DiffferentPackage_SameModule() throws Throwable {
		Lookup mhprivateLookupIn = privateLookupIn(SameModuleExample.class, callerLookup);
		Assert.assertEquals(mhprivateLookupIn.lookupClass(), SameModuleExample.class);
		Assert.assertEquals(mhprivateLookupIn.lookupModes() & Lookup.PRIVATE, Lookup.PRIVATE);
	}
	
	/**
	 * privateLookupIn test for a target class and a caller lookup from different modules
	 */
	@Test(groups = { "level.sanity" })
	public static void test_privateLookupIn_TargetClass_callerLookup_DifferentModule() throws Throwable {
		Lookup mhprivateLookupIn = privateLookupIn(DifferentModuleExample1.class, callerLookup);
		Assert.assertEquals(mhprivateLookupIn.lookupClass(), DifferentModuleExample1.class);
		Assert.assertEquals(mhprivateLookupIn.lookupModes() & Lookup.PRIVATE, Lookup.PRIVATE);
	}
}
