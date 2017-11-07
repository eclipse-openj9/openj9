/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https:www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https:www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https:www.gnu.org/software/classpath/license.html
 * [2] http:openjdk.java.net/legal/assembly-exception.html
 *******************************************************************************/
package org.openj9.test.constantPoolTags;

import java.lang.reflect.Field;
import java.util.*;

import org.testng.annotations.*;
import org.testng.log4testng.Logger;
import org.testng.*;

import sun.misc.Unsafe;

import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.ModuleVisitor;
import org.objectweb.asm.Opcodes;

/**
 * Validate that classes with Java 9 CONSTANT_Module and CONSTANT_Package
 * constant pool tags cannot be loaded, while modules with these tags can.
 * 
 * @author Theresa Mammarella
 */
@Test(groups = { "level.extended" })
public class ConstantPoolTagTests {

	private static Logger logger = Logger.getLogger(ConstantPoolTagTests.class);

	private static final String JAVA_VERSION = System.getProperty("java.version");

	private static final int classFileVersionJava8 = 52;
	private static final int classFileVersionJava9 = 53; 

	public static final int CONSTANT_Module = 19;
	public static final int CONSTANT_Package = 20; 

	private int getJavaVersion() {
		return isJava8() ? classFileVersionJava8 : classFileVersionJava9;
	}

	private boolean isJava8() {
		return JAVA_VERSION.startsWith("1.8.0");
	}

	private boolean isJava9() {
		return JAVA_VERSION.startsWith("1.9.0") || JAVA_VERSION.startsWith("9");
	}

	/**
	 * Creates byte code for a class or module with constant pool
	 * entries containing specified tags.
	 * @param cpTags - list of dummy constant pool tags to include in class
	 * @param module - create module if true
	 * @return byte array of class
	 */
	public static byte[] generateClass(String className, int version, int[] cpTags) {
		ClassWriter cw = new ClassWriter(0);

		/* insert tags into constant pool */
		for (int i = 0; i < cpTags.length; i++) {
			if (CONSTANT_Module == cpTags[i]) {
				cw.newModule("module_test");
			} else if (CONSTANT_Package == cpTags[i]) {
				cw.newPackage("package_test");
			}
		}

		cw.visit(version, Opcodes.ACC_PUBLIC, className, null, "java/lang/object", null);
		cw.visitEnd();

		return cw.toByteArray();
	}

	/**
	 * Attempts to load class file, throws an exception if it fails.
	 * @param className - name of generated class to load
	 * @param classBytes - class bytes to load
	 * @return true if class loading was successful, false otherwise
	 */
	private boolean loadClass(String className, byte[] classBytes) {
		boolean result = false;

		try {
			Field theUnsafeInstance = Unsafe.class.getDeclaredField("theUnsafe");
			theUnsafeInstance.setAccessible(true);
			Unsafe unsafe = (Unsafe) theUnsafeInstance.get(Unsafe.class);
			ClassLoader extLoader = ConstantPoolTagTests.class.getClassLoader()
					.getParent();

			Class<?> test = unsafe.defineClass(className, classBytes, 0,
					classBytes.length, extLoader,
					ConstantPoolTagTests.class.getProtectionDomain());
			result = true;

		} catch (ClassFormatError e) {
			logger.debug("ClassFormatError message is: " + e.getMessage());
		} catch (Exception e) {}

		return result;
	}

	@Test
	public void testPackageTagClass() {
		String className = "PackageTagClassTest";
		int[] tagList = {CONSTANT_Package};

		logger.debug(className);

		final byte[] classBytes = generateClass(className, getJavaVersion(), tagList);

		Assert.assertFalse(loadClass(className, classBytes));
	}

	@Test
	public void testModuleTagClass() {
		String className = "ModuleTagClassTest";
		int[] tagList = {CONSTANT_Module};

		logger.debug(className);

		final byte[] classBytes = generateClass(className, getJavaVersion(), tagList);

		Assert.assertFalse(loadClass(className, classBytes));
	}

	@Test
	public void testModulePackageTagClass() {
		String className = "ModulePackageTagClassTest";
		int[] tagList = {CONSTANT_Module, CONSTANT_Package};

		logger.debug(className);

		final byte[] classBytes = generateClass(className, getJavaVersion(), tagList);

		Assert.assertFalse(loadClass(className, classBytes));
	}

}
