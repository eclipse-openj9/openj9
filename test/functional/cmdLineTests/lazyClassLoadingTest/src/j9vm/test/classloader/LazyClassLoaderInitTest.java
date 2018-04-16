/*******************************************************************************
 * Copyright (c) 2013, 2018 IBM Corp. and others
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
package j9vm.test.classloader;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.HashSet;
import java.util.Set;

/**
 * Test for lazy classloader init (see Design 61796).
 *
 * @author braddill@ca.ibm.com Created 1 February 2013.
 */
public class LazyClassLoaderInitTest {

	static {
		try {
			System.loadLibrary("j9lazyClassLoad");
		} catch (UnsatisfiedLinkError ule) {
			ule.printStackTrace();
		}
	}

	/**
	 * Main test entry point.
	 *
	 * @param args
	 *            command line args (none defined).
	 */
	public static void main(String[] args) {
		/* Set of test names to run. */
		Set<String> testNameSet = new HashSet<String>();

		if (args.length == 0) {
			/* Print command line instructions. */
			System.out.println("No args - run all tests");
			System.out.println("To run specific tests, add the test name to the command line args.");
			System.out.println("Test name                   - description");
			System.out.println("testDefineClass             - tests EmptyClassLoader using Java only");
			System.out.println("testJniDefineClass          - DefineClass on EmptyClassLoader within a JNI call");
			System.out.println("testJvmtiCountLoadedClasses - verify EmptyClassLoader has loaded 0 classes using JVMTI");
			System.out.println("testJvmFindClass            - find Object class using internal JVM_FindClassFromClassLoader");

			/* Add all tests to set. */
			testNameSet.add("testDefineClass");
			testNameSet.add("testJniDefineClass");
			testNameSet.add("testJvmtiCountLoadedClasses");
			testNameSet.add("testJvmFindClass");
		} else {
			/* Add each test named in an argument to the set. */
			for(int argN = 0; argN < args.length; argN++) {
				testNameSet.add(args[argN]);
			}
		}

		/* Run each test named in the set. */
		LazyClassLoaderInitTest suite = new LazyClassLoaderInitTest();
		if (testNameSet.contains("testDefineClass")) {
			suite.testDefineClass();
		}
		if (testNameSet.contains("testJniDefineClass")) {
			suite.testJniDefineClass();
		}
		if (testNameSet.contains("testJvmtiCountLoadedClasses")) {
			suite.testJvmtiCountLoadedClasses();
		}
		if (testNameSet.contains("testJvmFindClass")) {
			suite.testJvmFindClass();
		}
	}

	/**
	 * Test that EmptyClassLoader works. This has nothing to do with
	 * lazyclassloading and is just a design verification that EmptyClassLoaded
	 * works at all.
	 */
	public void testDefineClass() {
		try {
			System.out.println("Begining testDefineClass");
			ClassLoader ecl = new EmptyClassLoader();
			@SuppressWarnings("rawtypes")
			Class clazz = ecl.loadClass("j9vm.test.classloader.Dummy");
			System.out.println(clazz.getName());
			System.out.println("Test PASSED");
		} catch (ClassNotFoundException e) {
			System.out.println("Test FAILED");
			e.printStackTrace();
		}
	}

	/**
	 * Call a native with a name, class file bytes and an empty loader and have
	 * it define and return the class. Use the class upon return from the
	 * native.
	 */
	public void testJniDefineClass() {
		try {
			System.out.println("Begining testJniDefineClass");
			ClassLoader ecl = new EmptyClassLoader();
			byte[] dummyBytecode = EmptyClassLoader.getDummyBytecode();
			assert dummyBytecode != null;
			assert dummyBytecode.length > 0;
			@SuppressWarnings("rawtypes")
			Class clazz = jniDefineClass("j9vm/test/classloader/Dummy", dummyBytecode, ecl);
			System.out.println(clazz.getName());
			System.out.println("Test PASSED");
		} catch (Exception e) {
			System.out.println("Test FAILED");
			e.printStackTrace();
		}
	}

	/**
	 * Call a native with an empty class loader which calls
	 * JVM_FindClassFromClassLoader_Impl to find a known class like Object using
	 * the loader.
	 */
	public void testJvmtiCountLoadedClasses() {
		try {
			System.out.println("Begining testJvmtiCountLoadedClasses");
			ClassLoader ecl = new EmptyClassLoader();
			int numClasses = jvmtiCountLoadedClasses(ecl);
			if (0 == numClasses) {
				System.out.println("Test PASSED");
			} else {
				System.out.println("number of loaded classes=" + numClasses);
				System.out.println("Should be 0 loaded classes in unused loader.");
				System.out.println("Test FAILED");
			}
		} catch (Exception e) {
			System.out.println("Test FAILED");
			e.printStackTrace();
		}
	}

	/**
	 * Call a native with an empty class loader which calls
	 * JVM_FindClassFromClassLoader_Impl to find a known class like Object using
	 * the loader.
	 */
	public void testJvmFindClass() {
		try {
			System.out.println("Begining testJvmFindClass");
			ClassLoader ecl = new EmptyClassLoader();
			@SuppressWarnings("rawtypes")
			Class clazz = (Class) jvmFindClass("java/lang/Object", ecl);
			System.out.println(clazz.getName());
			System.out.println("Test PASSED");
		} catch (Exception e) {
			System.out.println("Test FAILED");
			e.printStackTrace();
		}
	}

	/**
	 * Define a class from within a JNI call.
	 *
	 * @param classname
	 *            the name of the class to define.
	 * @param bytecode
	 *            the bytecode to use for the class.
	 * @param classloader
	 *            the classloader to use.
	 * @return the defined Class.
	 */
	private native Class<Object> jniDefineClass(String classname, byte[] bytecode, ClassLoader classloader);

	/**
	 * Find a Class using the internal function
	 * JVM_FindClassFromClassLoader_Impl.
	 *
	 * @param classname
	 *            the class name to find.
	 * @param classloader
	 *            the class loader to use.
	 * @return the class found.
	 */
	private native Object jvmFindClass(String classname, ClassLoader classloader);

	/**
	 * Use JVMTI to test and verify an empty class loader.
	 *
	 * @param classloaded
	 *            the classloader to check.
	 *
	 * @return true if successful.
	 */
	private native int jvmtiCountLoadedClasses(ClassLoader classloader);

	/**
	 * Print the bytes from a .class file. This was used to build the source
	 * code in EmptyClassLoader.java.
	 *
	 * @param classFilename
	 *            the class file to print.
	 */
	private static void printClassBytes(String classFilename) {
		try {
			File f = new File(classFilename);
			FileInputStream fis = new FileInputStream(f);
			long l = f.length();
			while (l > 0) {
				try {
					int i = fis.read();
					System.out.printf("0x%x, ", i);
				} catch (IOException ioe) {
					ioe.printStackTrace();
				}
				l--;
			}

		} catch (FileNotFoundException e) {
			e.printStackTrace();
		}
	}
}
