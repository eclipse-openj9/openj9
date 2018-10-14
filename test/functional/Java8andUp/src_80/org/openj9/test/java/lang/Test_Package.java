/*******************************************************************************
 * Copyright (c) 1998, 2018 IBM Corp. and others
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
package org.openj9.test.java.lang;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.Assert;
import java.lang.annotation.*;

import static org.testng.Assert.assertFalse;
import static org.testng.Assert.assertTrue;

import java.io.*;
import sun.misc.Unsafe;
import java.lang.reflect.Field;

@Test(groups = { "level.sanity" })
public class Test_Package {

	private static final Logger logger = Logger.getLogger(Test_Package.class);
	
	/**
	 * @throws SecurityException 
	 * @throws NoSuchFieldException 
	 * @tests java.lang.Package#getPackages()
	 */
	@Test
	public void test_GetPackages() {

		try {
			final boolean isJava8 = System.getProperty("java.specification.version").equals("1.8");
			String PACKAGE_NAME = "org.openj9.resources.packagetest";
			String PACKAGE = "/org/openj9/resources/packagetest/";
			
			InputStream classFile = Test_Package.class.getResourceAsStream(PACKAGE + "ClassGenerated.class");
			byte[] classFileBytes = new byte[classFile.available()];
			classFile.read(classFileBytes);
			
			Field theUnsafeInstance = Unsafe.class.getDeclaredField("theUnsafe");
			theUnsafeInstance.setAccessible(true);
			Unsafe unsafe = (Unsafe) theUnsafeInstance.get(Unsafe.class);

			/* Create a generated class from org.openj9.resources.packagetest package */
			Class<?> generatedClass = unsafe.defineClass("org.openj9.resources.packagetest.ClassGenerated", classFileBytes, 0, classFileBytes.length, null, null);
			if (generatedClass == null) {
				Assert.fail("Failed to generate class - org.openj9.resources.packagetest.ClassGenerated");
			}
			if (isJava8) {
				/* NullPointerException should not be thrown */
				assertFalse(findPackage(PACKAGE_NAME),
						"For generated classes, package information should not be available"); //$NON-NLS-1$
			} else { /* Java 9 adds all classes to a package */
				assertTrue(findPackage(PACKAGE_NAME),
						"Package information should be available for all classes"); //$NON-NLS-1$
			}
			
			/* Load a class from org.openj9.resources.packagetest package - not generated */			
			Class<?> nonGeneratedClass = Class.forName("org.openj9.resources.packagetest.ClassNonGenerated");
			if (nonGeneratedClass == null) {
				Assert.fail("Failed to load non-generated class - org.openj9.resources.packagetest.ClassNonGenerated");
			}
			if (!findPackage(PACKAGE_NAME)) {
				Assert.fail("For non-generated classes, package information should be available");
			}
			
			/* Generate a proxy class - Example: com/sun/proxy/$Proxy */
			AnnoPackageString.class.getAnnotations();
			
			/* NullPointerException should not be thrown when executing Package.getPackages() */
			Package.getPackages();
		} catch (IOException 
				| NoSuchFieldException 
				| SecurityException 
				| IllegalArgumentException 
				| IllegalAccessException 
				| ClassNotFoundException ex) {
			Assert.fail("Unexpected exception thrown: " + ex.getMessage()); //$NON-NLS-1$
		}
	}
	
	/**
	 * Check if a package is available in Package.getPackages().
	 * Make it private to prevent warnings from testNG.
	 */
	private static boolean findPackage(String name) {
		boolean rc = false;
		Package[] pack = Package.getPackages();
		for (int i = 0; i < pack.length; i++) {
			if (pack[i].toString().toLowerCase().contains(name.toLowerCase())) {
				rc = true;
				break;
			}
		}
		return rc;
	}
	
	/**
	 * Annotation - PackageAnn
	 */
	@Retention(RetentionPolicy.RUNTIME)
	@interface PackageAnn {
		String value();
	}

	/**
	 * AnnoPackageString class that uses the above annotation 
	 * It will be used to generate a com/sun/proxy/$Proxy class
	 */
	@PackageAnn("PackageAnn")
	class AnnoPackageString {

	}
}
