/*******************************************************************************
 * Copyright (c) 2017, 2020 IBM Corp. and others
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
import org.testng.Assert;
import java.lang.invoke.MethodHandles.Lookup;
import static java.lang.invoke.MethodHandles.lookup;
import static java.lang.invoke.MethodHandles.publicLookup;
import static java.lang.invoke.MethodHandles.privateLookupIn;
import mods.modulea.package1.ModuleA_Package1_Example1;
import mods.modulea.package2.ModuleA_Package2_Example1;
import mods.moduleb.package1.ModuleB_Package1_Example1;
import mods.modulec.package1.ModuleC_Package1_Example1;

public class LookupAPITests_DropLookupMode {
	final static Lookup lookup = lookup();
	final static Lookup publicLookup = publicLookup();
	final static Lookup callerLookup = ModuleA_Package1_Example1.getLookup();
	final static Lookup callerPublicLookup = ModuleA_Package1_Example1.getPublicLookup();
	final static int fullPrivilegeAccess = Lookup.MODULE | Lookup.PRIVATE;
	final static int NO_ACCESS = 0;
	
	/**
	 * dropLookupMode test for an invalid access mode
	 * @throws Throwable
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.sanity" })
	public static void test_dropLookupMode_InvalidAccessMode() throws Throwable {
		int invalidAccessMode = 0x100;
		Lookup invalidLookup = lookup.dropLookupMode(invalidAccessMode);
		Assert.fail("The test case failed to detect an invalid access mode");
	}
	
	/**
	 * dropLookupMode test for dropping the protected access
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public static void test_dropLookupMode_DropProtectedAccess() throws Throwable {
		int droppedAccessModes = Lookup.PROTECTED | Lookup.UNCONDITIONAL;
		int expectedAccessModes = lookup.lookupModes() & ~droppedAccessModes;
		Lookup newLookup = lookup.dropLookupMode(Lookup.PROTECTED);
		Assert.assertEquals(expectedAccessModes, newLookup.lookupModes());
	}
	
	/**
	 * dropLookupMode test for dropping the unconditional access
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public static void test_dropLookupMode_DropUnconditionalAccess() throws Throwable {
		int droppedAccessModes = Lookup.PROTECTED | Lookup.UNCONDITIONAL;
		int expectedAccessModes = lookup.lookupModes() & ~droppedAccessModes;
		Lookup newLookup = lookup.dropLookupMode(Lookup.UNCONDITIONAL);
		Assert.assertEquals(expectedAccessModes, newLookup.lookupModes());
	}
	
	/**
	 * dropLookupMode test for dropping the unconditional access from a public lookup
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public static void test_dropLookupMode_DropUnconditionalAccess_PublicLookup() throws Throwable {
		Lookup newLookup = publicLookup.dropLookupMode(Lookup.UNCONDITIONAL);
		Assert.assertEquals(NO_ACCESS, newLookup.lookupModes());
	}
	
	/**
	 * dropLookupMode test for dropping the public access from a public lookup
	 * Note: it should return the same lookup object given there is no mode change
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public static void test_dropLookupMode_DropPublicAccess_PublicLookup_NoModeChange() throws Throwable {
		Lookup newLookup = publicLookup.dropLookupMode(Lookup.PUBLIC);
		Assert.assertEquals(publicLookup, newLookup);
		Assert.assertEquals(publicLookup.previousLookupClass(), newLookup.previousLookupClass());
		Assert.assertEquals(publicLookup.lookupModes(), newLookup.lookupModes());
	}
	
	/**
	 * dropLookupMode test for dropping the private access
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public static void test_dropLookupMode_DropPrivateAccess() throws Throwable {
		int droppedAccessModes = Lookup.PRIVATE | Lookup.PROTECTED | Lookup.UNCONDITIONAL;
		int expectedAccessModes = lookup.lookupModes() & ~droppedAccessModes;
		Lookup newLookup = lookup.dropLookupMode(Lookup.PRIVATE);
		Assert.assertEquals(expectedAccessModes, newLookup.lookupModes());
	}
	
	/**
	 * dropLookupMode test for dropping the package access
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public static void test_dropLookupMode_DropPackageAccess() throws Throwable {
		int droppedAccessModes = Lookup.PACKAGE | Lookup.PRIVATE | Lookup.PROTECTED | Lookup.UNCONDITIONAL;
		int expectedAccessModes = lookup.lookupModes() & ~droppedAccessModes;
		Lookup newLookup = lookup.dropLookupMode(Lookup.PACKAGE);
		Assert.assertEquals(expectedAccessModes, newLookup.lookupModes());
	}
	
	/**
	 * dropLookupMode test for dropping the module access
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public static void test_dropLookupMode_DropModuleAccess() throws Throwable {
		int droppedAccessModes = Lookup.MODULE | Lookup.PACKAGE | Lookup.PRIVATE | Lookup.PROTECTED | Lookup.UNCONDITIONAL;
		int expectedAccessModes = lookup.lookupModes() & ~droppedAccessModes;
		Lookup newLookup = lookup.dropLookupMode(Lookup.MODULE);
		Assert.assertEquals(expectedAccessModes, newLookup.lookupModes());
	}
	
	/**
	 * dropLookupMode test for dropping the public access
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public static void test_dropLookupMode_DropPublicAccess() throws Throwable {
		Lookup newLookup = lookup.dropLookupMode(Lookup.PUBLIC);
		Assert.assertEquals(NO_ACCESS, newLookup.lookupModes());
	}
	
	/**
	 * dropLookupMode test for dropping the protected access
	 * Note: it should return the same lookup object given there is no mode change
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public static void test_dropLookupMode_DropProtectedAccess_NoModeChange() throws Throwable {
		Lookup newLookup1 = lookup.dropLookupMode(Lookup.UNCONDITIONAL);
		Lookup newLookup2 = newLookup1.dropLookupMode(Lookup.PROTECTED);
		Assert.assertEquals(newLookup2, newLookup1);
		Assert.assertEquals(newLookup2.previousLookupClass(), newLookup1.previousLookupClass());
		Assert.assertEquals(newLookup2.lookupModes(), newLookup1.lookupModes());
	}
	
	/**
	 * dropLookupMode test for dropping the protected access from a lookup with private access
	 * that is created with a target class and a caller lookup coming from the same module
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public static void test_dropLookupMode_DropProtectedAccess_PrivateLookup_SameModule() throws Throwable {
		int droppedAccessModes = Lookup.PROTECTED;
		int expectedAccessModes = callerLookup.lookupModes() & ~droppedAccessModes;
		Lookup privateLookup = privateLookupIn(ModuleA_Package2_Example1.class, callerLookup);
		Lookup newLookup = privateLookup.dropLookupMode(droppedAccessModes);
		Assert.assertEquals(expectedAccessModes, newLookup.lookupModes());
	}
	
	/**
	 * dropLookupMode test for dropping the private access from a lookup with private access
	 * that is created with a target class and a caller lookup coming from the same module
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public static void test_dropLookupMode_DropPrivateAccess_PrivateLookup_SameModule() throws Throwable {
		int droppedAccessModes = Lookup.PROTECTED | Lookup.PRIVATE;
		int expectedAccessModes = callerLookup.lookupModes() & ~droppedAccessModes;
		Lookup privateLookup = privateLookupIn(ModuleA_Package2_Example1.class, callerLookup);
		Lookup newLookup = privateLookup.dropLookupMode(Lookup.PRIVATE);
		Assert.assertEquals(expectedAccessModes, newLookup.lookupModes());
	}
	
	/**
	 * dropLookupMode test for dropping the package access from a lookup with private access
	 * that is created with a target class and a caller lookup coming from the same module
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public static void test_dropLookupMode_DropPackageAccess_PrivateLookup_SameModule() throws Throwable {
		int droppedAccessModes = Lookup.PROTECTED | Lookup.PRIVATE | Lookup.PACKAGE;
		int expectedAccessModes = callerLookup.lookupModes() & ~droppedAccessModes;
		Lookup privateLookup = privateLookupIn(ModuleA_Package2_Example1.class, callerLookup);
		Lookup newLookup = privateLookup.dropLookupMode(Lookup.PACKAGE);
		Assert.assertEquals(expectedAccessModes, newLookup.lookupModes());
	}
	
	/**
	 * dropLookupMode test for dropping the module access from a lookup with private access
	 * that is created with a target class and a caller lookup coming from the same module
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public static void test_dropLookupMode_DropModuleAccess_PrivateLookup_SameModule() throws Throwable {
		int droppedAccessModes = Lookup.PROTECTED | Lookup.PRIVATE | Lookup.PACKAGE | Lookup.MODULE;
		int expectedAccessModes = callerLookup.lookupModes() & ~droppedAccessModes;
		Lookup privateLookup = privateLookupIn(ModuleA_Package2_Example1.class, callerLookup);
		Lookup newLookup = privateLookup.dropLookupMode(Lookup.MODULE);
		Assert.assertEquals(expectedAccessModes, newLookup.lookupModes());
	}
	
	/**
	 * dropLookupMode test for dropping the public access from a lookup with private access
	 * that is created with a target class and a caller lookup coming from the same module
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public static void test_dropLookupMode_DropPublicAccess_PrivateLookup_SameModule() throws Throwable {
		int droppedAccessModes = Lookup.PROTECTED | Lookup.PRIVATE | Lookup.PACKAGE | Lookup.MODULE | Lookup.PUBLIC;
		int expectedAccessModes = callerLookup.lookupModes() & ~droppedAccessModes;
		Lookup privateLookup = privateLookupIn(ModuleA_Package2_Example1.class, callerLookup);
		Lookup newLookup = privateLookup.dropLookupMode(Lookup.PUBLIC);
		Assert.assertEquals(expectedAccessModes, newLookup.lookupModes());
	}
	
	/**
	 * dropLookupMode test for dropping the protected access from a lookup with private access
	 * that is created with a target class and a caller lookup coming from the different module
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public static void test_dropLookupMode_DropProtectedAccess_PrivateLookup_DifferentModule() throws Throwable {
		int droppedAccessModes = Lookup.MODULE | Lookup.PROTECTED;
		int expectedAccessModes = callerLookup.lookupModes() & ~droppedAccessModes;
		Lookup privateLookup = privateLookupIn(ModuleB_Package1_Example1.class, callerLookup);
		Lookup newLookup = privateLookup.dropLookupMode(Lookup.PROTECTED);
		Assert.assertEquals(expectedAccessModes, newLookup.lookupModes());
	}
	
	/**
	 * dropLookupMode test for dropping the private access from a lookup with private access
	 * that is created with a target class and a caller lookup coming from the different module
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public static void test_dropLookupMode_DropPrivateAccess_PrivateLookup_DifferentModule() throws Throwable {
		int droppedAccessModes = Lookup.MODULE | Lookup.PROTECTED | Lookup.PRIVATE;
		int expectedAccessModes = callerLookup.lookupModes() & ~droppedAccessModes;
		Lookup privateLookup = privateLookupIn(ModuleB_Package1_Example1.class, callerLookup);
		Lookup newLookup = privateLookup.dropLookupMode(Lookup.PRIVATE);
		Assert.assertEquals(expectedAccessModes, newLookup.lookupModes());
	}
	
	/**
	 * dropLookupMode test for dropping the package access from a lookup with private access
	 * that is created with a target class and a caller lookup coming from the different module
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public static void test_dropLookupMode_DropPackageAccess_PrivateLookup_DifferentModule() throws Throwable {
		int droppedAccessModes = Lookup.MODULE | Lookup.PROTECTED | Lookup.PRIVATE | Lookup.PACKAGE;
		int expectedAccessModes = callerLookup.lookupModes() & ~droppedAccessModes;
		Lookup privateLookup = privateLookupIn(ModuleB_Package1_Example1.class, callerLookup);
		Lookup newLookup = privateLookup.dropLookupMode(Lookup.PACKAGE);
		Assert.assertEquals(expectedAccessModes, newLookup.lookupModes());
	}
	
	/**
	 * dropLookupMode test for dropping the module access from a lookup with private access
	 * that is created with a target class and a caller lookup coming from the different module
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public static void test_dropLookupMode_DropModuleAccess_PrivateLookup_DifferentModule() throws Throwable {
		int droppedAccessModes = Lookup.MODULE | Lookup.PROTECTED | Lookup.PRIVATE | Lookup.PACKAGE;
		int expectedAccessModes = callerLookup.lookupModes() & ~droppedAccessModes;
		Lookup privateLookup = privateLookupIn(ModuleB_Package1_Example1.class, callerLookup);
		Lookup newLookup = privateLookup.dropLookupMode(Lookup.MODULE);
		Assert.assertEquals(expectedAccessModes, newLookup.lookupModes());
	}
	
	/**
	 * dropLookupMode test for dropping the public access from a lookup with private access
	 * that is created with a target class and a caller lookup coming from the different module
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public static void test_dropLookupMode_DropPublicAccess_PrivateLookup_DifferentModule() throws Throwable {
		int droppedAccessModes = Lookup.MODULE | Lookup.PROTECTED | Lookup.PRIVATE | Lookup.PACKAGE | Lookup.PUBLIC;
		int expectedAccessModes = callerLookup.lookupModes() & ~droppedAccessModes;
		Lookup privateLookup = privateLookupIn(ModuleB_Package1_Example1.class, callerLookup);
		Lookup newLookup = privateLookup.dropLookupMode(Lookup.PUBLIC);
		Assert.assertEquals(expectedAccessModes, newLookup.lookupModes());
	}
}
