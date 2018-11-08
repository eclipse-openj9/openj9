/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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
import org.testng.AssertJUnit;

import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;

import com.ibm.j9.jsr292.helpers.*;

interface Helper_Interface {
	public static Lookup interfaceLookupObject = MethodHandles.lookup();
}

public class LookupAPITests_FindClass {
	public final static Lookup lookupObjectThisPackage = MethodHandles.lookup();
	public final static Lookup lookupObjectPublic = MethodHandles.publicLookup();
	public final static Lookup lookupObjectHelperPackage = Helper_LookupAPI_OtherPackage.lookupObject;
	public final static Lookup lookupObjectHelperPackageDiffNest = Helper_LookupAPI_OtherPackage.difflookupObject_SamePackage;
	private final static String HELPER_PACKAGE = Helper_LookupAPI_OtherPackage.class.getPackageName() + ".";

	/**
	 * Calls findClass() to search a nonexistent class.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindClass_NonExistent() throws Throwable {
		try {
			lookupObjectHelperPackage.findClass(HELPER_PACKAGE + "nonExistentClass");
			Assert.fail("The class is not supposed to be found");
		} catch (ClassNotFoundException e) {
			/* Success */
		}
	}

	/**
	 * Call findClass() to search a class with public access.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindNestedClass_PublicAccess_SameNest() throws Throwable {
		Class<?> targetClass = lookupObjectHelperPackage.findClass(HELPER_PACKAGE + "Helper_LookupAPI_OtherPackage$PublicNestedClass");

		AssertJUnit.assertFalse(Helper_LookupAPI_OtherPackage.initializedPublicNestedClass);
		AssertJUnit.assertSame(Helper_LookupAPI_OtherPackage.PublicNestedClass.class, targetClass);
	}

	/**
	 * Call findClass() to search a class with private access via the lookup class only with public mode.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindNestedClass_PrivateAccess_PublicMode_DiffPackage() throws Throwable {
		try {
			lookupObjectPublic.findClass(HELPER_PACKAGE + "Helper_LookupAPI_OtherPackage$PrivateNestedClass");
			Assert.fail("The class is not supposed to be found");
		} catch (ClassNotFoundException e) {
			/* Success */
		}
	}

	/**
	 * Call findClass() to search the lookup class itself with private access.
	 * The test is used to verify the case when the target class to be found is the lookup class.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindLookupClass_PrivateAccess_SameClass() throws Throwable {
		Class<?> expectedClass = Helper_LookupAPI_OtherPackage.privateNestedClass;
		Class<?> targetClass = lookupObjectHelperPackage.in(expectedClass).findClass(HELPER_PACKAGE + "Helper_LookupAPI_OtherPackage$PrivateNestedClass");

		AssertJUnit.assertFalse(Helper_LookupAPI_OtherPackage.initializedPrivateNestedClass);
		AssertJUnit.assertSame(expectedClass, targetClass);
	}

	/**
	 * Call findClass() to search a nested class with private access.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindNestedClass_PrivateAccess_SameNest() throws Throwable {
		Class<?> targetClass = lookupObjectHelperPackage.findClass(HELPER_PACKAGE + "Helper_LookupAPI_OtherPackage$PrivateNestedClass");

		AssertJUnit.assertFalse(Helper_LookupAPI_OtherPackage.initializedPrivateNestedClass);
		AssertJUnit.assertSame(Helper_LookupAPI_OtherPackage.privateNestedClass, targetClass);
	}

	/**
	 * Call findClass() to search a double-nested class with private access.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindDoubleNestedClass_PrivateAccess_SameNest() throws Throwable {
		Class<?> targetClass = lookupObjectHelperPackage.findClass(HELPER_PACKAGE + "Helper_LookupAPI_OtherPackage$PrivateNestedClass$PrivateDoubleNestedClass");

		AssertJUnit.assertFalse(Helper_LookupAPI_OtherPackage.initializedPrivateDoubleNestedClass);
		AssertJUnit.assertSame(Helper_LookupAPI_OtherPackage.privateDoubleNestedClass, targetClass);
	}

	/**
	 * Call findClass() to search a class with private access in the same package.
	 * The test is used to verify the case when the target class is
	 * not nested in this lookup class but in the same package.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindNestedClass_PrivateAccess_DiffLookup_SamePackage() throws Throwable {
		Class<?> targetClass = lookupObjectHelperPackageDiffNest.findClass(HELPER_PACKAGE + "Helper_LookupAPI_OtherPackage$PrivateNestedClass");

		AssertJUnit.assertFalse(Helper_LookupAPI_OtherPackage.initializedPrivateNestedClass);
		AssertJUnit.assertSame(Helper_LookupAPI_OtherPackage.privateNestedClass, targetClass);
	}

	/**
	 * Call findClass() to search a private class from a different package.
	 * Note: The lookup class must come with private mode.
	 * So lookup.in() with a known class (e.g. String.class) as passed-in argument
	 * is not workable in this case as it ends up with public & package mode and
	 * no private mode bit gets setting up.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindNestedClass_PrivateAccess_SameNest_DiffPackage() throws Throwable {
		try {
			lookupObjectThisPackage.findClass(HELPER_PACKAGE + "Helper_LookupAPI_OtherPackage$PrivateNestedClass");
			Assert.fail("The class is not supposed to be found");
		}  catch (ClassNotFoundException | IllegalAccessException e) {
			/* Success */
		}
	}

	/**
	 * Call findClass() to search a protected class via the lookup class only with public mode.
	 * Note: The test case is different from the test with lookup.accessClass()
	 * as lookup.findClass() needs to call Class.forName() to load the target class
	 * which is expected to fail to do so.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindNestedClass_ProtectedAccess_PublicMode_DiffPackage() throws Throwable {
		try {
			lookupObjectPublic.findClass(HELPER_PACKAGE + "Helper_LookupAPI_OtherPackage$ProtectedNestedClass");
			Assert.fail("The class is not supposed to be found");
		} catch (ClassNotFoundException e) {
			/* Success */
		}
	}

	/**
	 * Call findClass() to search a protected class with protected access.
	 * The test case is used to verify the case when the target class is
	 * not nested in this lookup class but in the same package.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindNestedClass_ProtectedAccess_DiffLookup_SamePackage() throws Throwable {
		Class<?> targetClass = lookupObjectHelperPackageDiffNest.findClass(HELPER_PACKAGE + "Helper_LookupAPI_OtherPackage$ProtectedNestedClass");

		AssertJUnit.assertFalse(Helper_LookupAPI_OtherPackage.initializedProtectedNestedClass);
		AssertJUnit.assertSame(Helper_LookupAPI_OtherPackage.protectedNestedClass, targetClass);
	}

	/**
	 * Call findClass() to search a class with protected access
	 * via the lookup class from a different package.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindNestedClass_ProtectedAccess_DiffPackage() throws Throwable {
		Class<?> targetClass = lookupObjectThisPackage.findClass(HELPER_PACKAGE + "Helper_LookupAPI_OtherPackage$ProtectedNestedClass");

		AssertJUnit.assertFalse(Helper_LookupAPI_OtherPackage.initializedProtectedNestedClass);
		AssertJUnit.assertSame(Helper_LookupAPI_OtherPackage.protectedNestedClass, targetClass);
	}

	/**
	 * Call findClass() to search a class with protected access via the lookup
	 * which is actually an interface from a different package.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindNestClass_ProtectedAccess_InterfaceLookup_DiffPackage() throws Throwable {
		Helper_Interface.interfaceLookupObject.findClass(HELPER_PACKAGE + "Helper_LookupAPI_OtherPackage$ProtectedNestedClass");
	}

	/**
	 * Call findClass() to search a class with package access in the same package.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindClass_PackageAccess_DiffLookup_SamePackage() throws Throwable {
		Class<?> targetClass = lookupObjectHelperPackage.findClass(HELPER_PACKAGE + "Helper_Class_PackageAccess");

		AssertJUnit.assertFalse(Helper_LookupAPI_OtherPackage.initializedClassPackageAccess);
		AssertJUnit.assertSame(Helper_LookupAPI_OtherPackage.class_PackageAccess, targetClass);
	}

	/**
	 * Call findClass() to search a class with package access from a different package.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_FindClass_PackageAccess_DiffPackage() throws Throwable {
		try {
			lookupObjectThisPackage.findClass(HELPER_PACKAGE + "Helper_Class_PackageAccess");
			Assert.fail("The class is not supposed to be found");
		} catch (ClassNotFoundException | IllegalAccessException e) {
			/* Success */
		}
	}
}
