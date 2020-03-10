/*******************************************************************************
 * Copyright (c) 2016, 2020 IBM Corp. and others
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

import mods.modulea.package1.ModuleA_Package1_Example1;
import mods.modulea.package1.ModuleA_Package1_Example2;
import mods.modulea.package2.ModuleA_Package2_Example1;
import mods.moduleb.package1.ModuleB_Package1_Example1;
import mods.moduleb.package1.ModuleB_Package1_Example2;
import mods.moduleb.package2.ModuleB_Package2_Example1;
import mods.modulec.package1.ModuleC_Package1_Example1;

interface Helper_Interface {
	public static Lookup interfaceLookupObject = MethodHandles.lookup();
}

public class LookupAPITests_AccessClass {
	static final int NO_ACCESS = 0;
	static final int MODULE_PUBLIC_MODE =  Lookup.MODULE | Lookup.PUBLIC; // 17
	
	public final static Lookup lookupObjectThisPackage = MethodHandles.lookup();
	public final static Lookup lookupObjectPublic = MethodHandles.publicLookup();
	public final static Lookup lookupObjectHelperPackage = Helper_LookupAPI_OtherPackage.lookupObject;
	public final static Lookup lookupObjectHelperPackageDiffNest = Helper_LookupAPI_OtherPackage.difflookupObject_SamePackage;

	public final static Lookup lookupFromModuleA = ModuleA_Package1_Example1.getLookup();
	public final static Lookup lookupFromModuleC = ModuleC_Package1_Example1.getLookup();
	
	/**
	 * Tell whether a class with public access is accessible to the lookup class.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_AccessNestedClass_PublicAccess_SameNest() throws Throwable {
		Class<?> expectedClass = Helper_LookupAPI_OtherPackage.PublicNestedClass;
		Class<?> targetClass = lookupObjectHelperPackage.accessClass(expectedClass);

		AssertJUnit.assertFalse(Helper_LookupAPI_OtherPackage.initializedPublicNestedClass);
		AssertJUnit.assertSame(expectedClass, targetClass);
	}

	/**
	 * Tell whether a class with private access is accessible to the lookup class only with public mode.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" }, expectedExceptions = IllegalAccessException.class)
	public void test_AccessNestedClass_PrivateAccess_SameNest_PublicMode() throws Throwable {
		Assert.assertEquals(Lookup.UNCONDITIONAL, lookupObjectPublic.lookupModes() & Lookup.UNCONDITIONAL);
		lookupObjectPublic.accessClass(Helper_LookupAPI_OtherPackage.privateNestedClass);
		Assert.fail("The private class shouldn't be accessible to the lookup class only with public mode");
	}

	/**
	 * Tell whether the lookup class with private access is accessible to itself.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
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
	@Test(groups = { "level.sanity" })
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
	@Test(groups = { "level.sanity" })
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
	@Test(groups = { "level.sanity" })
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
	@Test(groups = { "level.sanity" }, expectedExceptions = IllegalAccessException.class)
	public void test_AccessNestedClass_PrivateAccess_DiffPackage() throws Throwable {
		int fullPrivilegeAccess = (Lookup.MODULE | Lookup.PRIVATE);
		Assert.assertEquals(fullPrivilegeAccess, lookupObjectThisPackage.lookupModes() & fullPrivilegeAccess);
		lookupObjectThisPackage.accessClass(Helper_LookupAPI_OtherPackage.privateNestedClass);
		Assert.fail("The private class shouldn't be accessible to the lookup class from a different package");
	}

	/**
	 * Tell whether a class with protected access is accessible to the lookup class.
	 * The test case is used to verify the case when the target class is
	 * not nested in this lookup class but in the same package.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
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
	@Test(groups = { "level.sanity" })
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
	@Test(groups = { "level.sanity" })
	public void test_AccessNestedClass_ProtectedAccess_InterfaceLookup_DiffPackage() throws Throwable {
		Helper_Interface.interfaceLookupObject.accessClass(Helper_LookupAPI_OtherPackage.protectedNestedClass);
	}

	/**
	 * Tell whether a class with package access is accessible to the lookup class
	 * in the same package.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
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
	@Test(groups = { "level.sanity" }, expectedExceptions = IllegalAccessException.class)
	public void test_AccessClass_PackageAccess_DiffPackage() throws Throwable {
		lookupObjectThisPackage.accessClass(Helper_LookupAPI_OtherPackage.class_PackageAccess);
		Assert.fail("The class shouldn't be accessible to the lookup class from a different package");
	}
	
	/**
	 * Tell whether a public class is accessible to the lookup class with module access 
	 * from a different package in the same module.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_AccessClass_ModuleAccess_PublicType_DifferentPackage_SameModule() throws Throwable {
		Class<?> expectedClass = Helper_LookupAPI_OtherPackage.PublicNestedClass;
		Lookup lookupWithModuleAccess = lookupObjectThisPackage.dropLookupMode(Lookup.PACKAGE);
		AssertJUnit.assertSame(lookupWithModuleAccess.lookupModes() , MODULE_PUBLIC_MODE);
		
		Class<?> targetClass = lookupWithModuleAccess.accessClass(expectedClass);
		AssertJUnit.assertFalse(Helper_LookupAPI_OtherPackage.initializedPublicNestedClass);
		AssertJUnit.assertSame(expectedClass, targetClass);
	}
	
	/**
	 * Tell whether a private class is accessible to the lookup class with module access 
	 * in the same module.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" }, expectedExceptions = IllegalAccessException.class)
	public void test_AccessClass_ModuleAccess_PrivateType_DifferentPackage_SameModule() throws Throwable {
		Lookup lookupWithModuleAccess = lookupObjectThisPackage.dropLookupMode(Lookup.PACKAGE);
		AssertJUnit.assertSame(lookupWithModuleAccess.lookupModes() , MODULE_PUBLIC_MODE);
		
		Class<?> targetClass = lookupWithModuleAccess.accessClass(Helper_LookupAPI_OtherPackage.privateNestedClass);
		Assert.fail("The private class shouldn't be accessible to the lookup class with module access in the same module");
	}
	
	/**
	 * Tell whether a public class is accessible to the lookup class with public access 
	 * from a different package in the same module.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_AccessClass_PublicAccess_PublicType_DifferentPackage_SameModule() throws Throwable {
		Class<?> expectedClass = Helper_LookupAPI_OtherPackage.PublicNestedClass;
		Lookup lookupWithPublicAccess = lookupObjectThisPackage.dropLookupMode(Lookup.MODULE);
		AssertJUnit.assertSame(lookupWithPublicAccess.lookupModes(), Lookup.PUBLIC);
		
		Class<?> targetClass = lookupWithPublicAccess.accessClass(expectedClass);
		AssertJUnit.assertFalse(Helper_LookupAPI_OtherPackage.initializedPublicNestedClass);
		AssertJUnit.assertSame(expectedClass, targetClass);
	}
	
	/**
	 * Tell whether a private class is accessible to the lookup class with public access 
	 * from a different package in the same module.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" }, expectedExceptions = IllegalAccessException.class)
	public void test_AccessClass_PublicAccess_PrivateType_DifferentPackage_SameModule() throws Throwable {
		Lookup lookupWithPublicAccess = lookupObjectThisPackage.dropLookupMode(Lookup.MODULE);
		AssertJUnit.assertSame(lookupWithPublicAccess.lookupModes(), Lookup.PUBLIC);
		
		Class<?> targetClass = lookupWithPublicAccess.accessClass(Helper_LookupAPI_OtherPackage.privateNestedClass);
		Assert.fail("The private class shouldn't be accessible to the lookup class with public access in the same module");
	}
	
	/**
	 * Tell whether a public class is accessible to the lookup class with unconditional access 
	 * from a different package in the same module.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_AccessClass_UnconditionalAccess_PublicType_DifferentPackage_SameModule() throws Throwable {
		AssertJUnit.assertSame(lookupObjectPublic.lookupModes(), Lookup.UNCONDITIONAL);
		Class<?> expectedClass = Helper_LookupAPI_OtherPackage.PublicNestedClass;
		Class<?> targetClass = lookupObjectPublic.accessClass(expectedClass);
		AssertJUnit.assertFalse(Helper_LookupAPI_OtherPackage.initializedPublicNestedClass);
		AssertJUnit.assertSame(expectedClass, targetClass);
	}
	
	/**
	 * Tell whether a private class is accessible to the lookup class with unconditional access 
	 * from a different package in the same module.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" }, expectedExceptions = IllegalAccessException.class)
	public void test_AccessClass_UnconditionalAccess_PrivateType_DifferentPackage_SameModule() throws Throwable {
		AssertJUnit.assertSame(lookupObjectPublic.lookupModes(), Lookup.UNCONDITIONAL);
		Class<?> targetClass = lookupObjectPublic.accessClass(Helper_LookupAPI_OtherPackage.privateNestedClass);
		Assert.fail("The private class shouldn't be accessible to the lookup class with unconditional access in the same module");
	}
	
	/**
	 * Tell whether a public class is accessible to the lookup class without public access 
	 * from a different module.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" }, expectedExceptions = IllegalAccessException.class)
	public void test_AccessClass_WithoutPublicAccess_PublicType_DifferentModule() throws Throwable {
		Lookup lookupWithoutPublicAccess = lookupFromModuleA.dropLookupMode(Lookup.PUBLIC);
		AssertJUnit.assertSame(lookupWithoutPublicAccess.lookupModes(), NO_ACCESS);
		Class<?> targetClass = lookupWithoutPublicAccess.accessClass(ModuleC_Package1_Example1.class);
		Assert.fail("The public class shouldn't be accessible to the lookup class without public access from a different module");
	}
	
	/**
	 * Tell whether a public class is accessible to the lookup without previous lookup class 
	 * from a different module.
	 * Note: 1) the lookup's module reads the public class's module.
	 *       2) the public class's package is exported to at least the lookup's module.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_AccessClass_NullPreviousLookup_WithRead_PublicType_DifferentModule() throws Throwable {
		AssertJUnit.assertSame(lookupFromModuleA.previousLookupClass(), null);
		Class<?> expectedClass = ModuleB_Package1_Example1.class;
		Class<?> targetClass = lookupFromModuleA.accessClass(expectedClass);
		AssertJUnit.assertSame(expectedClass, targetClass);
	}
	
	/**
	 * Tell whether a target class is accessible to the lookup from a different module
	 * with a previous lookup class in the same module as the target class.
	 * Note: 1) the lookup's module reads the target class's module.
	 *       2) the target class's package is exported to at least the lookup's module.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_AccessClass_PreviousLookup_TargetClass_SameModule() throws Throwable {
		AssertJUnit.assertSame(lookupFromModuleA.previousLookupClass(), null);
		Lookup LookupIntoModuleB = lookupFromModuleA.in(ModuleB_Package1_Example1.class);
		AssertJUnit.assertSame(LookupIntoModuleB.lookupClass(), ModuleB_Package1_Example1.class);
		AssertJUnit.assertSame(LookupIntoModuleB.previousLookupClass(), ModuleA_Package1_Example1.class);
		
		Class<?> expectedClass = ModuleA_Package2_Example1.class;
		Class<?> targetClass = LookupIntoModuleB.accessClass(expectedClass);
		AssertJUnit.assertSame(expectedClass, targetClass);
	}
	
	/**
	 * Tell whether a target class is accessible to the lookup (in the same module as
	 * the target class) with a previous lookup class from a different module.
	 * Note: 1) the previous lookup class's module reads the target class's module.
	 *       2) the target class's package is exported to at least the previous lookup class's module.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_PreviousLookup_AccessClass_TargetClass_SameModule() throws Throwable {
		AssertJUnit.assertSame(lookupFromModuleA.previousLookupClass(), null);
		Lookup LookupIntoModuleB = lookupFromModuleA.in(ModuleB_Package1_Example1.class);
		AssertJUnit.assertSame(LookupIntoModuleB.lookupClass(), ModuleB_Package1_Example1.class);
		AssertJUnit.assertSame(LookupIntoModuleB.previousLookupClass(), ModuleA_Package1_Example1.class);
		
		Class<?> expectedClass = ModuleB_Package2_Example1.class;
		Class<?> targetClass = LookupIntoModuleB.accessClass(expectedClass);
		AssertJUnit.assertSame(expectedClass, targetClass);
	}
	
	/**
	 * Tell whether a target class from a third module C is accessible to the lookup from a module B
	 * with a previous lookup class from a module A.
	 * Note: 1) both module A and Module B reads the target class's module C.
	 *       2) the target class's package is exported to at least module A and module B.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_AccessClass_PreviousLookup_TargetClass_ThirdModule() throws Throwable {
		AssertJUnit.assertSame(lookupFromModuleA.previousLookupClass(), null);
		Lookup LookupIntoModuleB = lookupFromModuleA.in(ModuleB_Package1_Example1.class);
		AssertJUnit.assertSame(LookupIntoModuleB.lookupClass(), ModuleB_Package1_Example1.class);
		AssertJUnit.assertSame(LookupIntoModuleB.previousLookupClass(), ModuleA_Package1_Example1.class);
		
		Class<?> expectedClass = ModuleC_Package1_Example1.class;
		Class<?> targetClass = LookupIntoModuleB.accessClass(expectedClass);
		AssertJUnit.assertSame(expectedClass, targetClass);
	}
}
