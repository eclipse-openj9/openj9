/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

package com.ibm.j9.recreateclass.tests;

import java.io.*;
import java.nio.file.Files;

import junit.framework.TestCase;

import com.ibm.j9.recreateclass.testclasses.*;
import com.ibm.j9.recreateclass.utils.RecreateClassUtils;
import com.ibm.j9.test.tools.classfilecompare.ClassFileCompare;

/**
 * This class has unit tests to verify various class file attributes are recreated correctly
 * when transforming a J9ROMClass back to .class format.
 * It uses cfdump tool to transform J9ROMClass to .class format,
 * and then uses ClassFileCompare tool to compare original .class with recreated .class file.
 * 
 * Original and recreated class files are created in current working directory.
 * By default, these class files are deleted once the test is over.
 * To retain the .class files, set the property -Dcom.ibm.j9.recreateClass.retainClassFiles=true.
 * 
 * @author ashutosh
 */
public class RecreateClassCompareTest extends TestCase {
	private File classFile;
	private File j9classFile;

	private void deleteClassFiles() {
		RecreateClassUtils.deleteFile(classFile);
		RecreateClassUtils.deleteFile(j9classFile);
	}

	private void compareClassFiles(int expectedDiffCount) throws IOException {
		byte[] classBytes = Files.readAllBytes(classFile.toPath());
		byte[] j9classBytes = Files.readAllBytes(j9classFile.toPath());
		boolean shouldCompareDebugInfo = true;

		int diffCount = ClassFileCompare.compareClassFiles(classBytes,
				j9classBytes, shouldCompareDebugInfo);
		System.out.println("Mismatches spotted: " + diffCount);
		assertEquals("Incorrect number of mismatches found", expectedDiffCount,
				diffCount);
	}

	private void runCfdumpAndCompareTools(String classFileName,
			int expectedDiffCount) throws Exception {
		RecreateClassUtils.runCfdump(classFileName);
		classFile = new File(classFileName + ".class");
		j9classFile = new File(classFileName + ".j9class");
		assertTrue("cfdump succeeded but " + classFileName + ".j9class does not exist",
				j9classFile.exists());
		compareClassFiles(expectedDiffCount);
	}

	private void runTest(Class<?> testClass, int expectedDiffCount)
			throws Exception {
		runTest(testClass, testClass.getSimpleName(), expectedDiffCount);
	}

	private void runTest(Class<?> testClass, String classFileName,
			int expectedDiffCount) throws Exception {
		try {
			RecreateClassUtils.createClassFile(testClass, classFileName);
			runCfdumpAndCompareTools(classFileName, expectedDiffCount);
		} finally {
			deleteClassFiles();
		}
	}

	private void runTest(byte[] classData, String classFileName,
			int expectedDiffCount) throws Exception {
		try {
			RecreateClassUtils.createClassFile(classData, classFileName);
			runCfdumpAndCompareTools(classFileName, expectedDiffCount);
		} finally {
			deleteClassFiles();
		}
	}

	public void setUp() {
		System.out.println("\n");
	}
	
	public void testSingleInterface() throws Exception {
		System.out.println("Start testSingleInterface");
		runTest(SingleInterfaceTest.class, 0);
		System.out.println("End testSingleInterface");		
	}
	
	public void testMutlipleInterfaces() throws Exception {
		System.out.println("Start testMutlipleInterfaces");
		runTest(MultipleInterfacesTest.class, 0);
		System.out.println("End testMutlipleInterfaces");		
	}
	
	public void testConstantValue() throws Exception {
		System.out.println("Start testConstantValue");
		runTest(ConstantValueTest.class, 0);
		System.out.println("End testConstantValue");
	}

	public void testStackMapTable() throws Exception {
		System.out.println("Start testStackMapTable");
		runTest(StackMapTableTest.class, 0);
		System.out.println("End testStackMapTable");
	}

	public void testStackMapTableTestWithBooleanArray() throws Exception {
		System.out.println("Start testStackMapTableTestWithBooleanArray");
		/* To follow the Java 9 Spec in which boolean arrays are different from byte arrays
		 * in terms of verification type, recreated StackMapTableWithByteBooleanArrayTest is 
		 * expected to keep boolean array in verification_type array of StackMapTable.
		 */
		runTest(StackMapTableWithByteBooleanArrayTest.class, 0);
		System.out.println("End testStackMapTableTestWithBooleanArray");
	}
	
	public void testExceptions() throws Exception {
		System.out.println("Start testExceptions");
		runTest(ExceptionsTest.class, 0);
		System.out.println("End testExceptions");
	}

	public void testInnerClasses() throws Exception {
		System.out.println("Start testInnerClasses");
		/* J9ROMClass does not store:
		 * 	- access modifiers of inner class
		 * 	- anonymous inner class
		 * 	- local class
		 * So the recreated class is expected to have above parts missing in InnerClasses attribute.  
		 */ 
		runTest(InnerClassesTest.class, 1);
		runTest(InnerClassesTest.InnerClass.class,
				"InnerClassesTest$InnerClass", 0);
		
		/* For an anonymous inner class, J9ROMClass does not store any entry for the class in InnerClasses attribute */
		Class<?> anonymousInnerClass = InnerClassesTest.getAnonymousInnerClass();
		String fileName = anonymousInnerClass.getName();
		/* remove package name */
		fileName = fileName.substring(fileName.lastIndexOf('.') + 1);
		runTest(anonymousInnerClass, fileName, 0);
		System.out.println("End testInnerClasses");
	}

	public void testEnclosingMethod() throws Exception {
		System.out.println("Start testEnclosingMethod");
		Class<?> localClass = InnerClassesTest.getLocalClass();
		runTest(localClass, "InnerClassesTest$1LocalClass", 0);
		
		/* For an anonymous local class, J9ROMClass does not store any entry for the class in InnerClasses attribute */
		Class<?> anonymousLocalClass = InnerClassesTest.getAnonymousLocalClass();
		String fileName = anonymousLocalClass.getName();
		/* remove package name */
		fileName = fileName.substring(fileName.lastIndexOf('.') + 1);
		runTest(localClass, fileName, 0);
		System.out.println("End testEnclosingMethod");
	}
	
	/* For some reason, if the class has Synthetic attribute, ASM sets a flag 0x40000 in access_flag.
	 * ASM doc says it corresponds to org.objectweb.asm.Opcodes.ASM4.
	 * This extra flag incorrectly results in ClassFileCompare to report mismatches.
	 * Once this issue is addressed (probably in ClassFileCompare tool), this test can then be enabled.
	 */
	public void testSynthetic() throws Exception {
		System.out.println("Start testSyncthetic");
		byte classData[] = SyntheticTestGenerator.generateClassData();
		runTest(classData, "SyntheticTest", 0);
		System.out.println("End testSyncthetic");
	}

	public void testSignature() throws Exception {
		System.out.println("Start testSignature");
		runTest(SignatureTest.class, 0);
		System.out.println("End testSignature");
	}
	
	public void testSourceDebugExtension() throws Exception {
		System.out.println("Start testSourceDebugExtension");
		byte classData[] = SourceDebugExtensionTestGenerator.generateClassData();
		runTest(classData, "SourceDebugExtensionTest", 0);
		System.out.println("End testSourceDebugExtension");
	}
	
	public void testDeprecated() throws Exception {
		System.out.println("Start testDeprecated");
		/* Deprecated attribute is not considered by ClassFileCompare explicitly
		 * but ASM adds its own flag (org.objectweb.asm.Opcodes.ACC_DEPRECATED) 
		 * in ClassFile access_flags if the class file has Deprecated attribute.
		 * Since the recreated class file does have Deprecated attribute, 
		 * ClassFileCompare reports a mismatch in ClassFile access_flags.
		 */
		runTest(DeprecatedTest.class, 0);
		System.out.println("End testDeprecated");
	}

	public void testSingleRuntimeVisibleAnnotationsTest() throws Exception {
		System.out.println("Start testSingleRuntimeVisibleAnnotationsTest");
		runTest(SingleRuntimeVisibleAnnotationsTest.class, 0);
		System.out.println("End testSingleRuntimeVisibleAnnotationsTest");
	}

	public void testMultipleRuntimeVisibleAnnotationsTest() throws Exception {
		System.out.println("Start testMultipleRuntimeVisibleAnnotationsTest");
		runTest(MultipleRuntimeVisibleAnnotationsTest.class, 0);
		System.out.println("End testMultipleRuntimeVisibleAnnotationsTest");
	}
	
	public void testRuntimeInvisibleAnnotationsTest() throws Exception {
		System.out.println("Start testRuntimeInvisibleAnnotationTest");
		/* RuntimeInvisibleAnnotations is not compared by ClassFileCompare */
		runTest(RuntimeInvisibleAnnotationsTest.class, 0);
		System.out.println("End testRuntimeInvisibleAnnotationsTest");
	}
	
	public void testMixedAnnotationsTest() throws Exception {
		System.out.println("Start testMixedAnnotationsTest");
		runTest(MixedAnnotationsTest.class, 0);
		System.out.println("End testMixedAnnotationsTest");
	}
	
	public void testAnnotationDefault() throws Exception {
		System.out.println("Start testAnnotationDefault");
		runTest(AnnotationDefaultTest.class, 0);
		System.out.println("End testAnnotationDefault");
	}

	public void testInvokeDynamic() throws Exception {
		System.out.println("Start testInvokeDynamic");
		byte classData[] = InvokeDynamicTestGenerator.generateClassData();
		runTest(classData, "InvokeDynamicTest", 0);
		System.out.println("End testInvokeDynamic");
	}

	public void testSharedInvokers() throws Exception {
		System.out.println("Start testSharedInvokers");
		byte classData[] = SharedInvokersTestIntfGenerator.generateClassData();
		RecreateClassUtils.createClassFile(classData, "SharedInvokersTestIntf");
		classData = SharedInvokersTestGenerator.generateClassData();
		runTest(classData, "SharedInvokersTest", 0);
		/* Delete interface class file which is not done by runTest() */
		RecreateClassUtils.deleteFile(new File("SharedInvokersTestIntf" + ".class"));
		System.out.println("End testSharedInvokers");
	}
	
	/* This test fails as ClassFileCompare tool does not handle JSR inlining */
	/*
	public void testJSRRET() throws Exception {
		System.out.println("Start testJSRRET");
		byte classData[] = JSRRETTestGenerator.generateClassData();
		runTest(classData, "JSRRETTest", 0);
		System.out.println("End testJSRRET");
	}
	*/
}
