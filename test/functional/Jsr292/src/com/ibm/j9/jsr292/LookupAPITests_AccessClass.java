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

public class LookupAPITests_AccessClass {
	public final static Lookup lookupObjectThisPackage = MethodHandles.lookup();
	public final static Lookup lookupObjectPublic = MethodHandles.publicLookup();
	public final static Lookup lookupObjectHelperPackage = Helper_LookupAPI_OtherPackage.lookupObject;
	public final static Lookup lookupObjectHelperPackageDiffNest = Helper_LookupAPI_OtherPackage.difflookupObject_SamePackage;

	/**
	 * Tell whether a class with public access is accessible to the lookup class.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_AccessNestedClass_PublicAccess_SameNest() throws Throwable {
		Class<?> expectedClass = Helper_LookupAPI_OtherPackage.PublicNestedClass.class;
		Class<?> targetClass = lookupObjectHelperPackage.accessClass(expectedClass);

		AssertJUnit.assertFalse(Helper_LookupAPI_OtherPackage.initializedPublicNestedClass);
		AssertJUnit.assertSame(expectedClass, targetClass);
	}

	/**
	 * Tell whether a class with private access is accessible to the lookup class only with public mode.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_AccessNestedClass_PrivateAccess_SameNest_PublicMode() throws Throwable {
		try {
			lookupObjectPublic.accessClass(Helper_LookupAPI_OtherPackage.privateNestedClass);
			Assert.fail("The private class shouldn't be accessible to the lookup class only with public mode");
		} catch (IllegalAccessException e) {
			/* Success */
		}
	}

	/**
	 * Tell whether the lookup class with private access is accessible to itself.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_AccessLookupClass_PrivateAccess_SameClass() throws Throwable {
		Class<?> expectedClass = Helper_LookupAPI_OtherPackage.privateNestedClass;
		Class<?> targetClass = lookupObjectHelperPackage.in(expectedClass).accessClass(expectedClass);

		AssertJUnit.assertFalse(Helper_LookupAPI_OtherPackage.initializedPrivateNestedClass);
		AssertJUnit.assertSame(expectedClass, targetClass);
	}

	/**
	 * Tell whether a nested class with private access is accessible to the lookup class.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_AccessNestedClass_PrivateAccess_SameNest() throws Throwable {
		Class<?> expectedClass = Helper_LookupAPI_OtherPackage.privateNestedClass;
		Class<?> targetClass = lookupObjectHelperPackage.accessClass(expectedClass);

		AssertJUnit.assertFalse(Helper_LookupAPI_OtherPackage.initializedPrivateNestedClass);
		AssertJUnit.assertSame(expectedClass, targetClass);
	}

	/**
	 * Tell whether a double-nested class with private access is accessible to the lookup class.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_AccessDoubleNestedClass_PrivateAccess_SameNest() throws Throwable {
		Class<?> expectedClass = Helper_LookupAPI_OtherPackage.privateDoubleNestedClass;
		Class<?> targetClass = lookupObjectHelperPackage.accessClass(expectedClass);

		AssertJUnit.assertFalse(Helper_LookupAPI_OtherPackage.initializedPrivateDoubleNestedClass);
		AssertJUnit.assertSame(expectedClass, targetClass);
	}

	/**
	 * Tell whether a nested class with private access is accessible to the lookup class
	 * in the same package.
	 * The test case is used to verify the case when the target class is not nested
	 * in the lookup class but in the same package.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_AccessNestedClass_PrivateAccess_DiffLookup_SamePackage() throws Throwable {
		Class<?> expectedClass = Helper_LookupAPI_OtherPackage.privateNestedClass;
		Class<?> targetClass = lookupObjectHelperPackageDiffNest.accessClass(expectedClass);

		AssertJUnit.assertFalse(Helper_LookupAPI_OtherPackage.initializedPrivateNestedClass);
		AssertJUnit.assertSame(expectedClass, targetClass);
	}

	/**
	 * Tell whether a class with private access is accessible to the lookup class
	 * from a different package.
	 * Note: The lookup class must come with private mode.
	 * So lookup.in() with a known class (e.g. String.class) as passed-in argument
	 * is not workable in this case as it ends up with public & package mode and
	 * no private mode bit gets setting up.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_AccessNestedClass_PrivateAccess_DiffPackage() throws Throwable {
		try {
			lookupObjectThisPackage.accessClass(Helper_LookupAPI_OtherPackage.privateNestedClass);
			Assert.fail("The private class shouldn't be accessible to the lookup class from a different package");
		} catch (IllegalAccessException e) {
			/* Success */
		}
	}

	/**
	 * Tell whether a class with protected access is accessible to the lookup class.
	 * The test case is used to verify the case when the target class is
	 * not nested in this lookup class but in the same package.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_AccessNestedClass_ProtectedAccess_SameNest_SamePackage() throws Throwable {
		Class<?> expectedClass = Helper_LookupAPI_OtherPackage.protectedNestedClass;
		Class<?> targetClass = lookupObjectHelperPackageDiffNest.accessClass(expectedClass);

		AssertJUnit.assertFalse(Helper_LookupAPI_OtherPackage.initializedProtectedNestedClass);
		AssertJUnit.assertSame(expectedClass, targetClass);
	}

	/**
	 * Tell whether a class with protected access is accessible to the lookup class
	 * from a different package.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_AccessNestedClass_ProtectedAccess_DiffPackage() throws Throwable {
		Class<?> expectedClass = Helper_LookupAPI_OtherPackage.protectedNestedClass;
		Class<?> targetClass = lookupObjectThisPackage.accessClass(expectedClass);

		AssertJUnit.assertFalse(Helper_LookupAPI_OtherPackage.initializedProtectedNestedClass);
		AssertJUnit.assertSame(expectedClass, targetClass);
	}

	/**
	 * Tell whether a class with protected access is accessible to the lookup
	 * which is actually an interface from a different package.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_AccessNestedClass_ProtectedAccess_InterfaceLookup_DiffPackage() throws Throwable {
		Helper_Interface.interfaceLookupObject.accessClass(Helper_LookupAPI_OtherPackage.protectedNestedClass);
	}

	/**
	 * Tell whether a class with package access is accessible to the lookup class
	 * in the same package.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_AccessClass_PackageAccess_DiffLookup_SamePackage() throws Throwable {
		Class<?> expectedClass = Helper_LookupAPI_OtherPackage.class_PackageAccess;
		Class<?> targetClass = lookupObjectHelperPackage.accessClass(expectedClass);

		AssertJUnit.assertFalse(Helper_LookupAPI_OtherPackage.initializedClassPackageAccess);
		AssertJUnit.assertSame(expectedClass, targetClass);
	}

	/**
	 * Tell whether a class with package access is accessible to the lookup class
	 * from a different package.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_AccessClass_PackageAccess_DiffPackage() throws Throwable {
		try {
			lookupObjectThisPackage.accessClass(Helper_LookupAPI_OtherPackage.class_PackageAccess);
			Assert.fail("The class shouldn't be accessible to the lookup class from a different package");
		} catch (IllegalAccessException e) {
			/* Success */
		}
	}
}
