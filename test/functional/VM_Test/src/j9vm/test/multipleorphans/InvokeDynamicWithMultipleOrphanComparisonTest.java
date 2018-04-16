/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package j9vm.test.multipleorphans;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.reflect.Method;

import com.ibm.oti.shared.Shared;
import com.ibm.oti.shared.SharedClassHelperFactory;
import com.ibm.oti.shared.SharedClassTokenHelper;

/**
 * This class is used to load a class containing invokedynamic bytecode.
 * The class is generated on the fly using ASM library. 
 * Each invocation of the main method causes a new classfile data to be generated using InvokeDynamicTestGenerator. 
 * The class is loaded using custom classloader which uses SharedClassTokenHelper to store the class in shared class cache.
 * 
 * @author Ashutosh
 */
public class InvokeDynamicWithMultipleOrphanComparisonTest {
	public static String PASS_STRING = "Test Passed";
	public static String FAIL_STRING = "Test Failed";
	
	public static void main(String args[]) throws Exception {
		GeneratedClassLoader classLoader = new GeneratedClassLoader();
		Class<?> cls = Class.forName(InvokeDynamicTestGenerator.GENERATED_CLASS_NAME, true, classLoader);
		Method m = cls.getMethod("addIntegers", new Class[] { int.class, int.class });
		try {
			int result = ((Integer)m.invoke(null, new Object[] { new Integer(10), new Integer(10) })).intValue();
			System.err.println("Result = " + result);
			System.err.println(PASS_STRING);
		} catch (Exception e) {
			e.printStackTrace();
			System.err.println(FAIL_STRING);
		}
	}
	
	/*
	 * Creates .class file on disk for the given class. The .class file is
	 * generated in current working directory obtained by accessing property
	 * 'java.io.tmpdir' 
	 * e.g. if className is j9vm.test.multipleorphans.InvokeDynamicTest then InvokeDynamicTest.class is
	 * generated in <cwd>/multipleorphanstest/j9vm/test/multipleorphans
	 */
	public static void writeToFile(String className, byte[] data) throws IOException {
		FileOutputStream fos = null;
		BufferedOutputStream bos = null;
		/* className is of the form j9vm.test.multipleorphans.InvokeDynamicTest */
		String simpleName = className.substring(className.lastIndexOf('.') + 1) + ".class";
		String pkgName = className.substring(0, className.lastIndexOf('.')).replace('.', File.separatorChar);
		String cwd = System.getProperty("user.dir");
		String classDir = cwd + File.separator + "multipleorphanstest" + File.separator + pkgName;

		System.err.println("Class file " + simpleName + " is created in dir " + classDir);
		File dir = new File(classDir);
		if (dir.exists() || dir.mkdirs()) {
			File file = new File(dir, simpleName);
			fos = new FileOutputStream(file);
			bos = new BufferedOutputStream(fos);
			bos.write(data);
			bos.close();
		}
	}
	
	static class GeneratedClassLoader extends ClassLoader {
		SharedClassTokenHelper helper = null;

		public GeneratedClassLoader() throws Exception {
			if (Shared.isSharingEnabled()) {
				SharedClassHelperFactory factory = Shared.getSharedClassHelperFactory();
				if (factory != null) {
					helper = factory.getTokenHelper(this);
				} else {
					throw new NullPointerException("Shared.getSharedClassHelperFactory() returned null");
				}
			} else {
				/* Not running with -Xshareclasses */
			}
		}

		public Class<?> findClass(String name) throws ClassNotFoundException {
			Class<?> clazz = null;
			byte[] classbytes = null;

			if (helper != null) {
				classbytes = helper.findSharedClass(name, name);
			}
			if (classbytes == null) {
				long currentTime = System.currentTimeMillis();
				int randomValue = (int)(currentTime % 1000) + (int)((currentTime / 1000)) % 1000;
				classbytes = InvokeDynamicTestGenerator.generateClassData(randomValue);
				try {
					InvokeDynamicWithMultipleOrphanComparisonTest.writeToFile(name + "_" + randomValue, classbytes);
				} catch (IOException e) {
					/*
					 * Ignore any exception in writing class file as it is only for
					 * debugging purpose and not required for running the test
					 */
					System.err.println("Note: This exception does not cause test to fail. Writing generated class file failed with exception " + e);
				}
				clazz = defineClass(name, classbytes, 0, classbytes.length);
				if (clazz == null) {
					throw new ClassNotFoundException("FAIL: Failed to define class");
				}
				/* Skip the call to storeSharedClass() so that the class is stored in shared cache as ORPHAN only */
				/*
				if (helper != null) {
					helper.storeSharedClass(name, clazz);
				}
				*/
			} else {
				clazz = defineClass(name, classbytes, 0, classbytes.length);
				if (clazz == null) {
					throw new ClassNotFoundException("FAIL: Failed to define class");
				}
			}
			return clazz;
		}
	}

}
