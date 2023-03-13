package org.openj9.test.modularity.pkgB;

/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import org.objectweb.asm.*;
import static org.objectweb.asm.Opcodes.ACC_PUBLIC;
import org.openj9.test.modularity.pkgA.SuperClassSealed;
import org.openj9.test.modularity.pkgA.SuperInterfaceSealed;
import org.openj9.test.modularity.pkgB.SuperClassFromPkgB;
import org.openj9.test.util.VersionCheck;

/**
 * Test cases for JEP 397: Sealed Classes (Second Preview)
 * 
 * Verify whether IncompatibleClassChangeError is thrown out when
 * the specified non-public subclass is in a different module from
 * the sealed super class/interface with a named module.
 */
 @Test(groups = { "level.sanity" })
public class Test_SubClass {
	/* sealed classes are NOT a preview feature since LTS jdk 17 */
	private static int latestVersion = VersionCheck.classFile();

	@Test(expectedExceptions = java.lang.IncompatibleClassChangeError.class)
	public void test_subClassInTheDifferentModuleFromSealedSuperClass() {
		String subClassName = "TestSubclass";
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] subClassNameBytes = generateSubclassInDifferentModuleFromSealedSuper(subClassName, SuperClassSealed.class, null);
		Class<?> clazz = classloader.getClass(subClassName, subClassNameBytes);
	}

	@Test(expectedExceptions = java.lang.IncompatibleClassChangeError.class)
	public void test_subClassInTheDifferentModuleFromSealedSuperInterface() {
		String subClassName = "TestSubclass";
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] subClassNameBytes = generateSubclassInDifferentModuleFromSealedSuper(subClassName, SuperClassFromPkgB.class, SuperInterfaceSealed.class);
		Class<?> clazz = classloader.getClass(subClassName, subClassNameBytes);
	}

	private static byte[] generateSubclassInDifferentModuleFromSealedSuper(String className, Class<?> superClass, Class<?> superInterface) {
		String superClassName = superClass.getName().replace('.', '/');
		String[] superInterfaceNames = null;
		if (superInterface != null) {
			String superInterfaceName = superInterface.getName().replace('.', '/');
			superInterfaceNames = new String[]{superInterfaceName};
		}
		
		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
		cw.visit(latestVersion, ACC_PUBLIC, className, null, superClassName, superInterfaceNames);
		cw.visitEnd();
		return cw.toByteArray();
	}
}

class CustomClassLoader extends ClassLoader {
	public Class<?> getClass(String name, byte[] bytes){
		return defineClass(name, bytes, 0, bytes.length);
	}
}
