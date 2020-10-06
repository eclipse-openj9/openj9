package org.openj9.test.jep358;

/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

import jdk.internal.org.objectweb.asm.ClassWriter;
import jdk.internal.org.objectweb.asm.MethodVisitor;
import jdk.internal.org.objectweb.asm.Opcodes;
import jdk.internal.org.objectweb.asm.Type;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.invoke.WrongMethodTypeException;

@Test(groups = { "level.sanity" })
public class NPEMessageTests {

	NPEMessageTests nullField = null;

	int[] intArray = null;
	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot load from int array")
	public void test_iaload() {
		int temp = intArray[0];
	}
	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot store to int array")
	public void test_iatore() {
		intArray[0] = 0;
	}

	long[] longArray = null;
	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot load from long array")
	public void test_laload() {
		long temp = longArray[0];
	}
	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot store to long array")
	public void test_latore() {
		longArray[0] = 0;
	}

	float[] floatArray = null;
	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot load from float array")
	public void test_faload() {
		float temp = floatArray[0];
	}
	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot store to float array")
	public void test_fatore() {
		floatArray[0] = 0.7f;
	}

	double[] doubleArray = null;
	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot load from double array")
	public void test_daload() {
		double temp = doubleArray[0];
	}
	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot store to double array")
	public void test_datore() {
		doubleArray[0] = 0;
	}

	Object[] objectArray = null;
	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot load from object array")
	public void test_aaload() {
		Object temp = objectArray[0];
	}
	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot store to object array")
	public void test_aatore() {
		objectArray[0] = new Object();
	}

	boolean[] booleanArray = null;
	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot load from byte/boolean array")
	public void test_baload_boolean() {
		boolean temp = booleanArray[0];
	}
	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot store to byte/boolean array")
	public void test_batore_boolean() {
		booleanArray[0] = false;
	}

	byte[] byteArrray = null;
	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot load from byte/boolean array")
	public void test_baload_byte() {
		byte temp = byteArrray[0];
	}
	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot store to byte/boolean array")
	public void test_batore_byte() {
		byteArrray[0] = 0;
	}

	char[] charArray = null;
	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot load from char array")
	public void test_caload() {
		char temp = charArray[0];
	}
	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot store to char array")
	public void test_catore() {
		charArray[0] = 0;
	}

	short[] shortArray = null;
	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot load from short array")
	public void test_saload() {
		short temp = shortArray[0];
	}
	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot store to short array")
	public void test_satore() {
		shortArray[0] = 0;
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot read the array length")
	public void test_arraylength() {
		int temp = booleanArray.length;
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot throw exception")
	public void test_athrow() {
		RuntimeException rt = null;
		throw rt;
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot enter synchronized block")
	public void test_monitorenter() {
		synchronized (nullField) {
		}
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot read field \"nullField\"")
	public void test_getfield() {
		Object temp = nullField.nullField;
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot assign field \"nullField\"")
	public void test_putfield() {
		nullField.nullField = new NPEMessageTests();
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot invoke \"java.lang.invoke.MethodHandle.invokeExact\\(java.lang.String\\)\"")
	public void test_invokehandle() throws WrongMethodTypeException, Throwable {
		String msg = null;
		MethodHandle mh = MethodHandles.lookup().findVirtual(String.class, "length", MethodType.methodType(int.class));
		int len = (int) mh.invokeExact(msg);
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot invoke \"java.lang.invoke.MethodHandle.invoke\\(java.lang.String\\)\"")
	public void test_invokehandlegeneric() throws WrongMethodTypeException, ClassCastException, Throwable {
		String msg = null;
		MethodHandle mh = MethodHandles.lookup().findVirtual(String.class, "length", MethodType.methodType(int.class));
		mh.invoke(msg);
	}

	static interface InterfaceHelper {
		public int get();
	}
	InterfaceHelper ifHelper;
	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot invoke \"org.openj9.test.jep358.NPEMessageTests\\$InterfaceHelper.get\\(\\)\"")
	public void test_invokeinterface() {
		Object temp = this.ifHelper.get();
	}

	static class SuperClass {
		public void testMethod() {}

		/*
		 * This generates a class which throws NPE at invokespecial
		 * class SubClass extends SuperClass {
		 *   public void testMethod() {
		 *     super = null;
		 *     return super.testMethod();
		 *   }
		 * }
		 */
		static byte[] generateSubClass(String subClassInternalName) {
			ClassWriter cw = new ClassWriter(0);
			MethodVisitor mv;
			String superClassInternalName = Type.getInternalName(SuperClass.class);
			
			cw.visit(Opcodes.V1_8, Opcodes.ACC_SUPER, subClassInternalName, null, superClassInternalName, null);

			// Generate method <init>()V
			mv = cw.visitMethod(0, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(Opcodes.ALOAD, 0);
			mv.visitMethodInsn(Opcodes.INVOKESPECIAL, superClassInternalName, "<init>", "()V", false);
			mv.visitInsn(Opcodes.RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
			
			// Generate method testMethod()V
			mv = cw.visitMethod(Opcodes.ACC_PUBLIC, "testMethod", "()V", null, null);
			mv.visitCode();
			mv.visitInsn(Opcodes.ACONST_NULL);
			mv.visitMethodInsn(Opcodes.INVOKESPECIAL, superClassInternalName, "testMethod", "()V", false);
			mv.visitInsn(Opcodes.RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();

			cw.visitEnd();

			return cw.toByteArray();
		}
	}
	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot invoke \"org.openj9.test.jep358.NPEMessageTests\\$SuperClass.testMethod\\(\\)\"")
	public void test_invokespecial() throws Exception {
		Class<?> thisClass = this.getClass();
		String subClassInternalName = Type.getInternalName(thisClass).replace(thisClass.getSimpleName(), "SubClass");
		byte[] bytes = SuperClass.generateSubClass(subClassInternalName);
		Class<?> clazz = MethodHandles.lookup().defineClass(bytes);
		SuperClass sc = (SuperClass)clazz.getDeclaredConstructor().newInstance();
		sc.testMethod();
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot invoke \"java.lang.Object.toString\\(\\)\"")
	public void test_invokevirtual() {
		String temp = nullField.toString();
	}

	void testMethodPara(String str, String[] strs1, String[][] strs2, StringBuffer sb, StringBuilder sb2, Thread thread, NPEMessageTests npemt, int i) {};
	@Test(expectedExceptions = NullPointerException.class, 
		expectedExceptionsMessageRegExp = "Cannot invoke \"org.openj9.test.jep358.NPEMessageTests.testMethodPara\\(java.lang.String, java.lang.String\\[\\], java.lang.String\\[\\]\\[\\], java.lang.StringBuffer, java.lang.StringBuilder, java.lang.Thread, org.openj9.test.jep358.NPEMessageTests, int\\)\"")
	public void test_parameters() {
		nullField.testMethodPara(null, null, null, null, null, null, null, 0);
	}

}
