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
import java.lang.invoke.MethodHandles.Lookup;
import static java.lang.invoke.MethodHandles.lookup;

public class MethodHandleAPI_dropLookupMode {
	final static Lookup lookup = lookup();
	
	/**
	 * dropLookupMode test for an invalid access mode
	 * @throws Throwable
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_dropLookupMode_InvalidAccessMode() throws Throwable {
		int invalidAccessMode = 0x100;
		Lookup invalidLookup = lookup.dropLookupMode(invalidAccessMode);
		Assert.fail("The test case failed to detect an invalid access mode");
	}
	
	/**
	 * dropLookupMode test for dropping the protected access
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
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
	@Test(groups = { "level.extended" })
	public static void test_dropLookupMode_DropUnconditionalAccess() throws Throwable {
		int droppedAccessModes = Lookup.PROTECTED | Lookup.UNCONDITIONAL;
		int expectedAccessModes = lookup.lookupModes() & ~droppedAccessModes;
		Lookup newLookup = lookup.dropLookupMode(Lookup.UNCONDITIONAL);
		Assert.assertEquals(expectedAccessModes, newLookup.lookupModes());
	}
	
	/**
	 * dropLookupMode test for dropping the private access
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
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
	@Test(groups = { "level.extended" })
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
	@Test(groups = { "level.extended" })
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
	@Test(groups = { "level.extended" })
	public static void test_dropLookupMode_DropPublicAccess() throws Throwable {
		Lookup newLookup = lookup.dropLookupMode(Lookup.PUBLIC);
		Assert.assertEquals(0, newLookup.lookupModes());
	}
}
