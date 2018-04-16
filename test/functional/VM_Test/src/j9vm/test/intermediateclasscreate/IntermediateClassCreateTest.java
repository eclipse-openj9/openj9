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
package j9vm.test.intermediateclasscreate;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.instrument.ClassFileTransformer;
import java.lang.instrument.IllegalClassFormatException;
import java.lang.reflect.Array;
import java.lang.reflect.Method;
import java.nio.file.Files;
import java.nio.file.attribute.FileAttribute;
import java.security.ProtectionDomain;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;

import com.ibm.oti.shared.*;
import com.ibm.j9.javaagent.JavaAgent;

/**
 * Usage:
 * 		java [-javaagent:<agent>] IntermediateClassCreateTest <index> [enableRCT] [enableRIT]
 * where,
 * <index>		: integer value used to create directory for storing generated classes
 * enableRCT 	: registers a retransformation capable transformer with java agent
 * enableRIT	: registers a retransformation incapable transformer with java agent
 * 
 * Main purpose of this class is to load a class 'SampleClass' using a custom classloader.
 * If retransformation capable transformer is registered then
 *  1) custom classloader loads invalid classfile bytes,
 *  2) retransformation capable transformer returns valid classfile bytes.
 * 
 * If retransformation incapable transformer is registered then it always returns invalid classfile bytes.
 * 
 * If no agent is specified custom classloader loads valid classfile bytes.
 * 
 * @author Ashutosh
 *
 */
public class IntermediateClassCreateTest {

	public static final String SAMPLE_CLASS_NAME = "j9vm.test.intermediateclasscreate.SampleClass";
	public static final String ENABLE_RCT = "enableRCT";
	public static final String ENABLE_RIT = "enableRIT";

	/* this field indicates version of SampleClass to be used */
	public static int VERSION = 0; 

	private static boolean isJavaAgentPresent = false;
	private static boolean doRetransform = false;
	private static int commandIndex;

	public static void parseArgs(String args[]) {
		commandIndex = Integer.parseInt(args[0]);
		for (int i = 1; i < args.length; i++) {
			if (args[i].equals(ENABLE_RIT)) {
				JavaAgent.addTransformer(new RITransformer(), false);
				isJavaAgentPresent = true;
			} else if (args[i].equals(ENABLE_RCT)) {
				JavaAgent.addTransformer(new RCTransformer(), true);
				doRetransform = true;
				isJavaAgentPresent = true;
			}
		}
	}

	public static void main(String args[]) throws Exception {
		parseArgs(args);
		ClassLoader loader = new CustomClassLoader();
		Class<?> clazz = loader.loadClass(SAMPLE_CLASS_NAME);
		Method method = clazz.getMethod("foo", (Class[]) null);
		int result = (Integer) method.invoke(clazz.newInstance(), (Object[]) null);
		if (result != VERSION) {
			System.err.println("FAIL: SampleClass.foo returned " + result + ", expected " + VERSION);
		}

		if (doRetransform) {
			/* Now retransform SampleClass couple of times to ensure java agent
			 * gets expected classfile bytes and that new retransformed class is
			 * working as expected.
			 */
			JavaAgent.retransformClass(clazz);
			clazz = Class.forName(SAMPLE_CLASS_NAME, true, loader);
			method = clazz.getMethod("foo", (Class[]) null);
			result = (Integer) method.invoke(clazz.newInstance(), (Object[]) null);
			System.out.println("Invoking " + clazz.getName()+ ".foo() returned " + result);
			if (result != VERSION) {
				System.err.println("FAIL: SampleClass.foo returned " + result + ", expected " + VERSION);
			}

			JavaAgent.retransformClass(clazz);
			clazz = Class.forName(SAMPLE_CLASS_NAME, true, loader);
			method = clazz.getMethod("foo", (Class[]) null);
			result = (Integer) method.invoke(clazz.newInstance(), (Object[]) null);
			System.out.println("Invoking " + clazz.getName()+ ".foo() returned " + result);
			if (result != VERSION) {
				System.err.println("FAIL: SampleClass.foo returned " + result + ", expected " + VERSION);
			}
			System.out.println();
		}
	}

	/*
	 * Creates .class file on disk for the given class. The .class file is
	 * generated in default temporary-file directory obtained by accessing property
	 * 'java.io.tmpdir' 
	 * e.g. if className is j9vm.test.intermediateclasscreate.SampleClass, SampleClass.class is
	 * generated in tmpdir/intermediateClassCreateTest/<commandIndex>/j9vm/test/intermediateclasscreate
	 */
	public static void writeToFile(String className, byte[] data) throws IOException {
		FileOutputStream fos = null;
		BufferedOutputStream bos = null;
		/* className is of the form j9vm.test.intermediateclasscreate.SampleClass */
		String simpleName = className.substring(className.lastIndexOf('.') + 1) + ".class";
		String pkgName = className.substring(0, className.lastIndexOf('.')).replace('.', File.separatorChar);
		String tmpDir = System.getProperty("java.io.tmpdir");
		String classDir = tmpDir + File.separator + "intermediateClassCreateTest" + File.separator + commandIndex + File.separator + pkgName;

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

	static class CustomClassLoader extends ClassLoader {
		SharedClassTokenHelper helper = null;

		public CustomClassLoader() throws Exception {
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
				classbytes = helper.findSharedClass(SAMPLE_CLASS_NAME, SAMPLE_CLASS_NAME);
			}
			if (classbytes == null) {
				VERSION = 0;
				if (isJavaAgentPresent) {
					classbytes = ClassGenerator.generateClassData(VERSION, false /* isValidClassFormat */);
					try {
						IntermediateClassCreateTest.writeToFile(name + "_Invalid_" + VERSION, classbytes);
					} catch (IOException e) {
						/*
						 * Ignore any exception in writing class file as it is only for
						 * debugging purpose and not required for running the test
						 */
						System.err.println("Note: This exception does not cause test to fail. Writing generated class file failed with exception " + e);
					}
				} else {
					classbytes = ClassGenerator.generateClassData(VERSION, true);
					try {
						IntermediateClassCreateTest.writeToFile(name + "_Valid_" + VERSION, classbytes);
					} catch (IOException e) {
						/*
						 * Ignore any exception in writing class file as it is only for
						 * debugging purpose and not required for running the test
						 */
						System.err.println("Note: This exception does not cause test to fail. Writing generated class file failed with exception " + e);	
					}
				}
				clazz = defineClass(name, classbytes, 0, classbytes.length);
				if (clazz == null) {
					throw new ClassNotFoundException("FAIL: Failed to define class");
				}
				if (helper != null) {
					helper.storeSharedClass(SAMPLE_CLASS_NAME, clazz);
				}
			} else {
				clazz = defineClass(name, classbytes, 0, classbytes.length);
				if (clazz == null) {
					throw new ClassNotFoundException("FAIL: Failed to define class");
				}
			}

			return clazz;
		}
	}

	static class RITransformer implements ClassFileTransformer {

		public byte[] transform(ClassLoader loader, 
								String className,
								Class<?> classBeingRedefined,
								ProtectionDomain protectionDomain, 
								byte[] classfileBuffer)	throws IllegalClassFormatException {

			String binaryName = className.replace('/', '.');
			if (binaryName.equals(IntermediateClassCreateTest.SAMPLE_CLASS_NAME)) {
				System.out.println("RITransformer.transform called for " + binaryName);
				VERSION += 1;
				byte[] classBytes = ClassGenerator.generateClassData(VERSION, false /* isValidClassFormat */);
				try {
					IntermediateClassCreateTest.writeToFile(binaryName + "_Invalid_" + VERSION, classBytes);
				} catch (IOException e) {
					/*
					 * Ignore any exception in writing class file as it is only for
					 * debugging purpose and not required for running the test
					 */
					System.err.println("Note: This exception does not cause test to fail. Writing generated class file failed with exception " + e);
				}
				return classBytes;
			}
			return null;
		}
	}

	static class RCTransformer implements ClassFileTransformer {

		public byte[] transform(ClassLoader loader, 
								String className,
								Class<?> classBeingRedefined,
								ProtectionDomain protectionDomain, 
								byte[] classfileBuffer) throws IllegalClassFormatException {

			String binaryName = className.replace('/', '.');
			if (binaryName.equals(IntermediateClassCreateTest.SAMPLE_CLASS_NAME)) {
				System.out.println("RCTransformer.transform called for " + binaryName);
				VERSION += 1;
				byte[] validClassData = ClassGenerator.generateClassData(VERSION, true);
				try {
					IntermediateClassCreateTest.writeToFile(binaryName + "_Valid_" + VERSION, validClassData);
				} catch (IOException e) {
					/*
					 * Ignore any exception in writing class file as it is only for
					 * debugging purpose and not required for running the test
					 */
					System.err.println("Note: This exception does not cause test to fail. Writing generated class file failed with exception " + e);
				}				
				return validClassData;
			}
			return null;
		}
	}
}
