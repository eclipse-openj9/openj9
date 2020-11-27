package org.openj9.test.java.lang;

/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import org.openj9.test.utilities.CustomClassLoader;
import org.openj9.test.utilities.SealedClassGenerator;
import org.openj9.test.java.pkgC.SuperClassSealed;
import org.openj9.test.java.pkgC.SuperInterfaceSealed;
import org.openj9.test.java.lang.SuperClassInSamePkg;
/**
 * Test cases for JEP 397: Sealed Classes (Second Preview)
 * 
 * Verify whether IncompatibleClassChangeError is thrown out when
 * the specified non-public subclass is in a different package from
 * the sealed super class/interface within the same unamed module.
 */
@Test(groups = { "level.sanity" })
public class Test_SubClass {
	@Test(expectedExceptions = java.lang.IncompatibleClassChangeError.class)
	public void test_subClassInTheDifferentPackageFromSealedSuperClass() {
		String subClassName = "TestSubclass";
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] subClassNameBytes = SealedClassGenerator.generateSubclassInDifferentPackageFromSealedSuper(subClassName, SuperClassSealed.class, null);
		Class<?> clazz = classloader.getClass(subClassName, subClassNameBytes);
	}
	
	@Test(expectedExceptions = java.lang.IncompatibleClassChangeError.class)
	public void test_subClassInTheDifferentPackageFromSealedSuperInterface() {
		String subClassName = "TestSubclass";
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] subClassNameBytes = SealedClassGenerator.generateSubclassInDifferentPackageFromSealedSuper(subClassName, SuperClassInSamePkg.class, SuperInterfaceSealed.class);
		Class<?> clazz = classloader.getClass(subClassName, subClassNameBytes);
	}
}
