package org.openj9.test.jep358;

/*******************************************************************************
 * Copyright (c) 2020, 2021 IBM Corp. and others
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

import jdk.internal.org.objectweb.asm.ClassWriter;
import jdk.internal.org.objectweb.asm.Label;
import jdk.internal.org.objectweb.asm.MethodVisitor;
import jdk.internal.org.objectweb.asm.Opcodes;
import jdk.internal.org.objectweb.asm.Type;

import static java.lang.invoke.MethodHandles.lookup;

import static jdk.internal.org.objectweb.asm.Opcodes.ACC_PUBLIC;
import static jdk.internal.org.objectweb.asm.Opcodes.ACC_SUPER;
import static jdk.internal.org.objectweb.asm.Opcodes.ACONST_NULL;
import static jdk.internal.org.objectweb.asm.Opcodes.ALOAD;
import static jdk.internal.org.objectweb.asm.Opcodes.ASTORE;
import static jdk.internal.org.objectweb.asm.Opcodes.GETFIELD;
import static jdk.internal.org.objectweb.asm.Opcodes.GETSTATIC;
import static jdk.internal.org.objectweb.asm.Opcodes.ICONST_1;
import static jdk.internal.org.objectweb.asm.Opcodes.INVOKESPECIAL;
import static jdk.internal.org.objectweb.asm.Opcodes.INVOKEVIRTUAL;
import static jdk.internal.org.objectweb.asm.Opcodes.IRETURN;
import static jdk.internal.org.objectweb.asm.Opcodes.RETURN;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.invoke.WrongMethodTypeException;
import java.lang.invoke.MethodHandles.Lookup;

interface E0 {
	public int throwNPEOne(F fVal);

	public void throwNPETwo(String str);

	public void throwNPEThree(String str1, String str2, String str3, String str4);
}

class F {
	int iVal;
}

@Test(groups = { "level.sanity" })
public class NPEMessageTests {
	private static Object nullStaticField;
	private NPEMessageTests nullField = null;
	private static boolean hasDebugInfo = Boolean.getBoolean("hasDebugInfo");

	class A {
		public B bClz;

		public B getBClz() {
			return bClz;
		}
	}

	class B {
		public C cClz;
		public B bClz;

		public C getCClz() {
			return cClz;
		}

		public B getBClzfromBClz() {
			return bClz;
		}
	}

	class C {
		public D dClz;

		public D getDClz() {
			return dClz;
		}
	}

	class D {
		public int intNum;
		public int[][] intArray;
	}

	static class NullReturn {
		public static Object returnNullOne(boolean bVal) {
			return null;
		}

		public Object returnNullTwo(double dVal, long lVal, short sVal) {
			return null;
		}
	}

	private static void checkMessage(String actualMsg, String expectedMsgWithDebugInfo, String expectedMsgNoDebugInfo) {
		Assert.assertEquals(actualMsg, hasDebugInfo ? expectedMsgWithDebugInfo : expectedMsgNoDebugInfo);
	}

	public void test_iaload() {
		try {
			int[] intArray = null;
			int temp = intArray[0];
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot load from int array because \"intArray\" is null",
					"Cannot load from int array because \"<local1>\" is null");
		}
	}

	public void test_iastore() {
		try {
			int[] intArray = null;
			intArray[0] = 0;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to int array because \"intArray\" is null",
					"Cannot store to int array because \"<local1>\" is null");
		}
	}

	public void test_laload() {
		try {
			long[] longArray = null;
			long temp = longArray[0];
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot load from long array because \"longArray\" is null",
					"Cannot load from long array because \"<local1>\" is null");
		}
	}

	public void test_lastore() {
		try {
			long[] longArray = null;
			longArray[0] = 0;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to long array because \"longArray\" is null",
					"Cannot store to long array because \"<local1>\" is null");
		}
	}

	public void test_faload() {
		try {
			float[] floatArray = null;
			float temp = floatArray[0];
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot load from float array because \"floatArray\" is null",
					"Cannot load from float array because \"<local1>\" is null");
		}
	}

	public void test_fastore() {
		try {
			float[] floatArray = null;
			floatArray[0] = 0.7f;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to float array because \"floatArray\" is null",
					"Cannot store to float array because \"<local1>\" is null");
		}
	}

	public void test_daload() {
		try {
			double[] doubleArray = null;
			double temp = doubleArray[0];
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot load from double array because \"doubleArray\" is null",
					"Cannot load from double array because \"<local1>\" is null");
		}
	}

	public void test_dastore() {
		try {
			double[] doubleArray = null;
			doubleArray[0] = 0;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to double array because \"doubleArray\" is null",
					"Cannot store to double array because \"<local1>\" is null");
		}
	}

	public void test_aaload() {
		try {
			Object[] objectArray = null;
			Object temp = objectArray[0];
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot load from object array because \"objectArray\" is null",
					"Cannot load from object array because \"<local1>\" is null");
		}
	}

	public void test_aastore() {
		try {
			Object[] objectArray = null;
			objectArray[0] = new Object();
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to object array because \"objectArray\" is null",
					"Cannot store to object array because \"<local1>\" is null");
		}
	}

	public void test_baload_boolean() {
		try {
			boolean[] booleanArray = null;
			boolean temp = booleanArray[0];
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot load from byte/boolean array because \"booleanArray\" is null",
					"Cannot load from byte/boolean array because \"<local1>\" is null");
		}
	}

	public void test_bastore_boolean() {
		try {
			boolean[] booleanArray = null;
			booleanArray[0] = false;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to byte/boolean array because \"booleanArray\" is null",
					"Cannot store to byte/boolean array because \"<local1>\" is null");
		}
	}

	public void test_baload_byte() {
		try {
			byte[] byteArrray = null;
			byte temp = byteArrray[0];
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot load from byte/boolean array because \"byteArrray\" is null",
					"Cannot load from byte/boolean array because \"<local1>\" is null");
		}
	}

	public void test_bastore_byte() {
		try {
			byte[] byteArrray = null;
			byteArrray[0] = 0;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to byte/boolean array because \"byteArrray\" is null",
					"Cannot store to byte/boolean array because \"<local1>\" is null");
		}
	}

	public void test_caload() {
		try {
			char[] charArray = null;
			char temp = charArray[0];
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot load from char array because \"charArray\" is null",
					"Cannot load from char array because \"<local1>\" is null");
		}
	}

	public void test_castore() {
		try {
			char[] charArray = null;
			charArray[0] = 0;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to char array because \"charArray\" is null",
					"Cannot store to char array because \"<local1>\" is null");
		}
	}

	public void test_saload() {
		try {
			short[] shortArray = null;
			short temp = shortArray[0];
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot load from short array because \"shortArray\" is null",
					"Cannot load from short array because \"<local1>\" is null");
		}
	}

	public void test_sastore() {
		try {
			short[] shortArray = null;
			shortArray[0] = 0;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to short array because \"shortArray\" is null",
					"Cannot store to short array because \"<local1>\" is null");
		}
	}

	public void test_arraylength() {
		try {
			boolean[] booleanArray = null;
			int temp = booleanArray.length;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot read the array length because \"booleanArray\" is null",
					"Cannot read the array length because \"<local1>\" is null");
		}
	}

	public void test_athrow() {
		try {
			RuntimeException rt = null;
			throw rt;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot throw exception because \"rt\" is null",
					"Cannot throw exception because \"<local1>\" is null");
		}
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot throw exception because \"null\" is null")
	public void test_athrow_aconst_null() {
		throw null;
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot enter synchronized block because \"this.nullField\" is null")
	public void test_monitorenter() {
		synchronized (nullField) {
		}
	}
	// monitorexit
	// No test available

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot read field \"nullField\" because \"this.nullField\" is null")
	public void test_getfield() {
		Object temp = nullField.nullField;
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot assign field \"nullField\" because \"this.nullField\" is null")
	public void test_putfield() {
		nullField.nullField = new NPEMessageTests();
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot invoke \"java.lang.invoke.MethodHandle.(invokeExact\\(java.lang.String\\)\"|linkToVirtual\\(java.lang.Object, java.lang.invoke.MemberName\\)\" because \"<parameter1>\" is null)")
	public void test_invokehandle() throws WrongMethodTypeException, Throwable {
		String msg = null;
		MethodHandle mh = MethodHandles.lookup().findVirtual(String.class, "length", MethodType.methodType(int.class));
		int len = (int) mh.invokeExact(msg);
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot invoke \"java.lang.invoke.MethodHandle.(invoke\\(java.lang.String\\)\"|linkToVirtual\\(java.lang.Object, java.lang.invoke.MemberName\\)\" because \"<parameter1>\" is null)")
	public void test_invokehandlegeneric() throws WrongMethodTypeException, ClassCastException, Throwable {
		String msg = null;
		MethodHandle mh = MethodHandles.lookup().findVirtual(String.class, "length", MethodType.methodType(int.class));
		mh.invoke(msg);
	}

	public void test_iload_0() {
		try {
			int i0 = 0;
			int[][] a = new int[6][];
			a[i0][0] = 77;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to int array because \"a[i0]\" is null",
					"Cannot store to int array because \"<local2>[<local1>]\" is null");
		}
	}

	public void test_iload_1() {
		try {
			int i1 = 1;
			int[][] a = new int[6][];
			a[i1][0] = 77;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to int array because \"a[i1]\" is null",
					"Cannot store to int array because \"<local2>[<local1>]\" is null");
		}
	}

	public void test_iload_2() {
		try {
			int i2 = 2;
			int[][] a = new int[6][];
			a[i2][0] = 77;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to int array because \"a[i2]\" is null",
					"Cannot store to int array because \"<local2>[<local1>]\" is null");
		}
	}

	public void test_iload_3() {
		try {
			int i3 = 3;
			int[][] a = new int[6][];
			a[i3][0] = 77;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to int array because \"a[i3]\" is null",
					"Cannot store to int array because \"<local2>[<local1>]\" is null");
		}
	}

	public void test_iload() {
		try {
			int i5 = 5;
			int[][] a = new int[6][];
			a[i5][0] = 77;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to int array because \"a[i5]\" is null",
					"Cannot store to int array because \"<local2>[<local1>]\" is null");
		}
	}

	public void test_lload_0() {
		try {
			long long0 = 0L;
			int[][] a = new int[6][];
			a[(int) long0][0] = 77;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to int array because \"a[long0]\" is null",
					"Cannot store to int array because \"<local3>[<local1>]\" is null");
		}
	}

	public void test_lload_1() {
		try {
			long long1 = 1L;
			int[][] a = new int[6][];
			a[(int) long1][0] = 77;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to int array because \"a[long1]\" is null",
					"Cannot store to int array because \"<local3>[<local1>]\" is null");
		}
	}

	public void test_lload_2() {
		try {
			long long2 = 2L;
			int[][] a = new int[6][];
			a[(int) long2][0] = 77;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to int array because \"a[long2]\" is null",
					"Cannot store to int array because \"<local3>[<local1>]\" is null");
		}
	}

	public void test_lload_3() {
		try {
			long long3 = 3L;
			int[][] a = new int[6][];
			a[(int) long3][0] = 77;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to int array because \"a[long3]\" is null",
					"Cannot store to int array because \"<local3>[<local1>]\" is null");
		}
	}

	public void test_lload() {
		try {
			long long5 = 5L;
			int[][] a = new int[6][];
			a[(int) long5][0] = 77;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to int array because \"a[long5]\" is null",
					"Cannot store to int array because \"<local3>[<local1>]\" is null");
		}
	}

	public void test_fload_0() {
		try {
			float f0 = 0.0f;
			int[][] a = new int[6][];
			a[(int) f0][0] = 77;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to int array because \"a[f0]\" is null",
					"Cannot store to int array because \"<local2>[<local1>]\" is null");
		}
	}

	public void test_fload_1() {
		try {
			float f1 = 1.0f;
			int[][] a = new int[6][];
			a[(int) f1][0] = 77;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to int array because \"a[f1]\" is null",
					"Cannot store to int array because \"<local2>[<local1>]\" is null");
		}
	}

	public void test_fload_2() {
		try {
			float f2 = 2.0f;
			int[][] a = new int[6][];
			a[(int) f2][0] = 77;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to int array because \"a[f2]\" is null",
					"Cannot store to int array because \"<local2>[<local1>]\" is null");
		}
	}

	public void test_fload_3() {
		try {
			float f3 = 3.0f;
			int[][] a = new int[6][];
			a[(int) f3][0] = 77;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to int array because \"a[f3]\" is null",
					"Cannot store to int array because \"<local2>[<local1>]\" is null");
		}
	}

	public void test_fload() {
		try {
			float f5 = 5.0f;
			int[][] a = new int[6][];
			a[(int) f5][0] = 77;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to int array because \"a[f5]\" is null",
					"Cannot store to int array because \"<local2>[<local1>]\" is null");
		}
	}

	public void test_aload_0() {
		try {
			F f0 = null;
			f0.iVal = 33;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot assign field \"iVal\" because \"f0\" is null",
					"Cannot assign field \"iVal\" because \"<local1>\" is null");
		}
	}

	public void test_aload_1() {
		try {
			F f1 = null;
			f1.iVal = 33;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot assign field \"iVal\" because \"f1\" is null",
					"Cannot assign field \"iVal\" because \"<local1>\" is null");
		}
	}

	public void test_aload_2() {
		try {
			F f2 = null;
			f2.iVal = 33;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot assign field \"iVal\" because \"f2\" is null",
					"Cannot assign field \"iVal\" because \"<local1>\" is null");
		}
	}

	public void test_aload_3() {
		try {
			F f3 = null;
			f3.iVal = 33;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot assign field \"iVal\" because \"f3\" is null",
					"Cannot assign field \"iVal\" because \"<local1>\" is null");
		}
	}

	public void test_aload() {
		try {
			F f5 = null;
			f5.iVal = 33;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot assign field \"iVal\" because \"f5\" is null",
					"Cannot assign field \"iVal\" because \"<local1>\" is null");
		}
	}

	public void test_iload_iconst_0() {
		try {
			int[][] a = new int[820][];
			a[0][0] = 77;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to int array because \"a[0]\" is null",
					"Cannot store to int array because \"<local1>[0]\" is null");
		}
	}

	public void test_iload_iconst_1() {
		try {
			int[][] a = new int[820][];
			a[1][0] = 77;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to int array because \"a[1]\" is null",
					"Cannot store to int array because \"<local1>[1]\" is null");
		}
	}

	public void test_iload_iconst_2() {
		try {
			int[][] a = new int[820][];
			a[2][0] = 77;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to int array because \"a[2]\" is null",
					"Cannot store to int array because \"<local1>[2]\" is null");
		}
	}

	public void test_iload_iconst_3() {
		try {
			int[][] a = new int[820][];
			a[3][0] = 77;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to int array because \"a[3]\" is null",
					"Cannot store to int array because \"<local1>[3]\" is null");
		}
	}

	public void test_iload_iconst_4() {
		try {
			int[][] a = new int[820][];
			a[4][0] = 77;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to int array because \"a[4]\" is null",
					"Cannot store to int array because \"<local1>[4]\" is null");
		}
	}

	public void test_iload_iconst_5() {
		try {
			int[][] a = new int[820][];
			a[5][0] = 77;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to int array because \"a[5]\" is null",
					"Cannot store to int array because \"<local1>[5]\" is null");
		}
	}

	public void test_iload_long_to_iconst() {
		try {
			int[][] a = new int[820][];
			a[(int) 0L][0] = 77;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to int array because \"a[0]\" is null",
					"Cannot store to int array because \"<local1>[0]\" is null");
		}
	}

	public void test_iload_bipush() {
		try {
			int[][] a = new int[820][];
			a[139][0] = 77;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to int array because \"a[139]\" is null",
					"Cannot store to int array because \"<local1>[139]\" is null");
		}
	}

	public void test_iload_sipush() {
		try {
			int[][] a = new int[820][];
			a[819][0] = 77;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to int array because \"a[819]\" is null",
					"Cannot store to int array because \"<local1>[819]\" is null");
		}
	}

	public void test_aaload_a() {
		try {
			int[][][][][][] a = null;
			a[0][0][0][0][0][0] = 99;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot load from object array because \"a\" is null",
					"Cannot load from object array because \"<local1>\" is null");
		}
	}

	public void test_aaload_a1() {
		try {
			int[][][][][][] a = new int[1][][][][][];
			a[0][0][0][0][0][0] = 99;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot load from object array because \"a[0]\" is null",
					"Cannot load from object array because \"<local1>[0]\" is null");
		}
	}

	public void test_aaload_a2() {
		try {
			int[][][][][][] a = new int[1][][][][][];
			a[0] = new int[1][][][][];
			a[0][0][0][0][0][0] = 99;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot load from object array because \"a[0][0]\" is null",
					"Cannot load from object array because \"<local1>[0][0]\" is null");
		}
	}

	public void test_aaload_a3() {
		try {
			int[][][][][][] a = new int[1][][][][][];
			a[0] = new int[1][][][][];
			a[0][0] = new int[1][][][];
			a[0][0][0][0][0][0] = 99;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot load from object array because \"a[0][0][0]\" is null",
					"Cannot load from object array because \"<local1>[0][0][0]\" is null");
		}
	}

	public void test_arraylength_a4() {
		try {
			int[][][][][][] a = new int[1][][][][][];
			a[0] = new int[1][][][][];
			a[0][0] = new int[1][][][];
			System.out.println(a[0][0][0].length);
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot read the array length because \"a[0][0][0]\" is null",
					"Cannot read the array length because \"<local1>[0][0][0]\" is null");
		}
	}

	public void test_arraylength_a5() {
		try {
			int[][][][][][] a = new int[1][][][][][];
			a[0] = new int[1][][][][];
			a[0][0] = new int[1][][][];
			a[0][0][0] = new int[1][][];
			a[0][0][0][0][0][0] = 99;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot load from object array because \"a[0][0][0][0]\" is null",
					"Cannot load from object array because \"<local1>[0][0][0][0]\" is null");
		}
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot load from float array because \"org.openj9.test.jep358.NPEMessageTests.nullStaticField\" is null")
	public void test_getstatic() {
		boolean bVal = (((float[]) nullStaticField)[0] == 1.0f);
	}

	public void test_getfield_1() {
		try {
			A aClz = null;
			aClz.bClz.cClz.dClz.intNum = 99;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot read field \"bClz\" because \"aClz\" is null",
					"Cannot read field \"bClz\" because \"<local1>\" is null");
		}
	}

	public void test_getfield_2() {
		try {
			A aClz = new A();
			aClz.bClz.cClz.dClz.intNum = 99;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot read field \"cClz\" because \"aClz.bClz\" is null",
					"Cannot read field \"cClz\" because \"<local1>.bClz\" is null");
		}
	}

	public void test_getfield_3() {
		try {
			A aClz = new A();
			aClz.bClz = new B();
			aClz.bClz.cClz.dClz.intNum = 99;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot read field \"dClz\" because \"aClz.bClz.cClz\" is null",
					"Cannot read field \"dClz\" because \"<local1>.bClz.cClz\" is null");
		}
	}

	public void test_getfield_4() {
		try {
			A aClz = new A();
			aClz.bClz = new B();
			aClz.bClz.cClz = new C();
			aClz.bClz.cClz.dClz.intNum = 99;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot assign field \"intNum\" because \"aClz.bClz.cClz.dClz\" is null",
					"Cannot assign field \"intNum\" because \"<local1>.bClz.cClz.dClz\" is null");
		}
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot load from char array because the return value of \"org.openj9.test.jep358.NPEMessageTests\\$NullReturn.returnNullOne\\(boolean\\)\" is null")
	public void test_invokestatic() {
		char cVal = ((char[]) NullReturn.returnNullOne(false))[0];
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot load from char array because the return value of \"org.openj9.test.jep358.NPEMessageTests\\$NullReturn.returnNullTwo\\(double, long, short\\)\" is null")
	public void test_invokevirtual_2() {
		char cVal = ((char[]) (new NullReturn().returnNullTwo(1, 1, (short) 1)))[0];
	}

	private Object returnNull(String[][] strArray, int[][][] intArray, float fVal) {
		return null;
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot load from double array because the return value of \"org.openj9.test.jep358.NPEMessageTests.returnNull\\(java.lang.String\\[\\]\\[\\], int\\[\\]\\[\\]\\[\\], float\\)\" is null")
	public void test_invokevirtual_3() {
		double dVal = ((double[]) returnNull(null, null, 1f))[0];
	}

	public void test_method_chasing_1() {
		try {
			A aClz = null;
			aClz.getBClz().getBClzfromBClz().getCClz().getDClz().intNum = 99;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(),
					"Cannot invoke \"org.openj9.test.jep358.NPEMessageTests$A.getBClz()\" because \"aClz\" is null",
					"Cannot invoke \"org.openj9.test.jep358.NPEMessageTests$A.getBClz()\" because \"<local1>\" is null");
		}
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot invoke \"org.openj9.test.jep358.NPEMessageTests\\$B.getBClzfromBClz\\(\\)\" because the return value of \"org.openj9.test.jep358.NPEMessageTests\\$A.getBClz\\(\\)\" is null")
	public void test_method_chasing_2() {
		A aClz = null;
		aClz = new A();
		aClz.getBClz().getBClzfromBClz().getCClz().getDClz().intNum = 99;
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot invoke \"org.openj9.test.jep358.NPEMessageTests\\$B.getCClz\\(\\)\" because the return value of \"org.openj9.test.jep358.NPEMessageTests\\$B.getBClzfromBClz\\(\\)\" is null")
	public void test_method_chasing_3() {
		A aClz = null;
		aClz = new A();
		aClz.bClz = new B();
		aClz.getBClz().getBClzfromBClz().getCClz().getDClz().intNum = 99;
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot invoke \"org.openj9.test.jep358.NPEMessageTests\\$C.getDClz\\(\\)\" because the return value of \"org.openj9.test.jep358.NPEMessageTests\\$B.getCClz\\(\\)\" is null")
	public void test_method_chasing_4() {
		A aClz = null;
		aClz = new A();
		aClz.bClz = new B();
		aClz.bClz.bClz = new B();
		aClz.getBClz().getBClzfromBClz().getCClz().getDClz().intNum = 99;
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot assign field \"intNum\" because the return value of \"org.openj9.test.jep358.NPEMessageTests\\$C.getDClz\\(\\)\" is null")
	public void test_method_chasing_5() {
		A aClz = null;
		aClz = new A();
		aClz.bClz = new B();
		aClz.bClz.bClz = new B();
		aClz.bClz.bClz.cClz = new C();
		aClz.getBClz().getBClzfromBClz().getCClz().getDClz().intNum = 99;
	}

	public void test_mixed_chasing_1() {
		try {
			A aClz = null;
			aClz.getBClz().getBClzfromBClz().cClz.dClz.intArray[0][0] = 99;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(),
					"Cannot invoke \"org.openj9.test.jep358.NPEMessageTests$A.getBClz()\" because \"aClz\" is null",
					"Cannot invoke \"org.openj9.test.jep358.NPEMessageTests$A.getBClz()\" because \"<local1>\" is null");
		}
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot invoke \"org.openj9.test.jep358.NPEMessageTests\\$B.getBClzfromBClz\\(\\)\" because the return value of \"org.openj9.test.jep358.NPEMessageTests\\$A.getBClz\\(\\)\" is null")
	public void test_mixed_chasing_2() {
		A aClz = null;
		aClz = new A();
		aClz.getBClz().getBClzfromBClz().cClz.dClz.intArray[0][0] = 99;
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot read field \"cClz\" because the return value of \"org.openj9.test.jep358.NPEMessageTests\\$B.getBClzfromBClz\\(\\)\" is null")
	public void test_mixed_chasing_3() {
		A aClz = null;
		aClz = new A();
		aClz.bClz = new B();
		aClz.getBClz().getBClzfromBClz().cClz.dClz.intArray[0][0] = 99;
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot read field \"dClz\" because \"org.openj9.test.jep358.NPEMessageTests\\$B.getBClzfromBClz\\(\\).cClz\" is null")
	public void test_mixed_chasing_4() {
		A aClz = null;
		aClz = new A();
		aClz.bClz = new B();
		aClz.bClz.bClz = new B();
		aClz.getBClz().getBClzfromBClz().cClz.dClz.intArray[0][0] = 99;
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot read field \"intArray\" because \"org.openj9.test.jep358.NPEMessageTests\\$B.getBClzfromBClz\\(\\).cClz.dClz\" is null")
	public void test_mixed_chasing_5() {
		A aClz = null;
		aClz = new A();
		aClz.bClz = new B();
		aClz.bClz.bClz = new B();
		aClz.bClz.bClz.cClz = new C();
		aClz.getBClz().getBClzfromBClz().cClz.dClz.intArray[0][0] = 99;
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot load from object array because \"org.openj9.test.jep358.NPEMessageTests\\$B.getBClzfromBClz\\(\\).cClz.dClz.intArray\" is null")
	public void test_mixed_chasing_6() {
		A aClz = null;
		aClz = new A();
		aClz.bClz = new B();
		aClz.bClz.bClz = new B();
		aClz.bClz.bClz.cClz = new C();
		aClz.bClz.bClz.cClz.dClz = new D();
		aClz.getBClz().getBClzfromBClz().cClz.dClz.intArray[0][0] = 99;
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot load from object array because \"org.openj9.test.jep358.NPEMessageTests\\$C.getDClz\\(\\).intArray\" is null")
	public void test_mixed_chasing_7() {
		A aClz = null;
		aClz = new A();
		aClz.bClz = new B();
		aClz.bClz.bClz = new B();
		aClz.bClz.bClz.cClz = new C();
		aClz.bClz.bClz.cClz.dClz = new D();
		aClz.getBClz().getBClzfromBClz().getCClz().getDClz().intArray[0][0] = 99;
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot store to int array because \"org.openj9.test.jep358.NPEMessageTests\\$B.getBClzfromBClz\\(\\).cClz.dClz.intArray\\[0\\]\" is null")
	public void test_mixed_chasing_8() {
		A aClz = null;
		aClz = new A();
		aClz.bClz = new B();
		aClz.bClz.bClz = new B();
		aClz.bClz.bClz.cClz = new C();
		aClz.bClz.bClz.cClz.dClz = new D();
		aClz.bClz.bClz.cClz.dClz.intArray = new int[1][];
		aClz.getBClz().getBClzfromBClz().cClz.dClz.intArray[0][0] = 99;
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot store to int array because \"org.openj9.test.jep358.NPEMessageTests\\$C.getDClz\\(\\).intArray\\[0\\]\" is null")
	public void test_mixed_chasing_9() {
		A aClz = null;
		aClz = new A();
		aClz.bClz = new B();
		aClz.bClz.bClz = new B();
		aClz.bClz.bClz.cClz = new C();
		aClz.bClz.bClz.cClz.dClz = new D();
		aClz.bClz.bClz.cClz.dClz.intArray = new int[1][];
		aClz.getBClz().getBClzfromBClz().getCClz().getDClz().intArray[0][0] = 99;
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot invoke \"org.openj9.test.jep358.NPEMessageTests\\$B.getBClzfromBClz\\(\\)\" because the return value of \"org.openj9.test.jep358.NPEMessageTests\\$A.getBClz\\(\\)\" is null")
	public void test_mixed_chasing_10() {
		A aClz = new A();
		aClz.getBClz().getBClzfromBClz().cClz.dClz.intArray[0][0] = 99;
	}

	private void methodArgs_1(A aClz, double placeholder, B bClz, Integer i) {
		aClz.bClz.cClz.dClz.intNum = 99;
	}

	public void test_parameters_1() {
		try {
			A aClz = new A();
			aClz.bClz = new B();
			methodArgs_1(aClz, 0.0, null, null);
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot read field \"dClz\" because \"aClz.bClz.cClz\" is null",
					"Cannot read field \"dClz\" because \"<parameter1>.bClz.cClz\" is null");
		}
	}

	private void methodArgs_2(A aClz, double placeholder, B bClz, Integer i) {
		bClz.cClz.dClz.intNum = 99;
	}

	public void test_parameters_2() {
		try {
			A aClz = new A();
			aClz.bClz = new B();
			methodArgs_2(aClz, 0.0, null, null);
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot read field \"cClz\" because \"bClz\" is null",
					"Cannot read field \"cClz\" because \"<parameter3>\" is null");
		}
	}

	private void methodArgs_3(A aClz, double placeholder, B bClz, Integer i) {
		int iVal = i;
	}

	public void test_parameters_3() {
		try {
			A aClz = new A();
			aClz.bClz = new B();
			methodArgs_3(aClz, 0.0, null, null);
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot invoke \"java.lang.Integer.intValue()\" because \"i\" is null",
					"Cannot invoke \"java.lang.Integer.intValue()\" because \"<parameter4>\" is null");
		}
	}

	private String manyArgs(int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8, int i9, int i10, int i11,
			int i12, int i13, int i14, int i15, int i16, int i17, int i18, int i19, int i20, int i21, int i22, int i23,
			int i24, int i25, int i26, int i27, int i28, int i29, int i30, int i31, int i32, int i33, int i34, int i35,
			int i36, int i37, int i38, int i39, int i40, int i41, int i42, int i43, int i44, int i45, int i46, int i47,
			int i48, int i49, int i50, int i51, int i52, int i53, int i54, int i55, int i56, int i57, int i58, int i59,
			int i60, int i61, int i62, int i63, int i64, int i65, int i66, int i67, int i68, int i69, int i70) {
		String[][][][] strArray = new String[1][1][1][1];
		int[][][] intArray3 = new int[1][1][1];
		int[][] intArray2 = new int[1][1];
		return strArray[i70][intArray2[i65][i64]][intArray3[i63][i62][i47]][intArray3[intArray2[i33][i32]][i31][i17]]
				.substring(2);
	}

	public void test_parameters_4() {
		try {
			manyArgs(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
					0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
					0, 0, 0, 0);
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(),
					"Cannot invoke \"java.lang.String.substring(int)\" because \"strArray[i70][intArray2[i65][i64]][intArray3[i63][i62][i47]][intArray3[intArray2[i33][i32]][i31][i17]]\" is null",
					"Cannot invoke \"java.lang.String.substring(int)\" because \"<local71>[<parameter70>][<local73>[<parameter65>][<parameter64>]][<local72>[<parameter63>][<parameter62>][<parameter47>]][<local72>[<local73>[<parameter33>][<parameter32>]][<parameter31>][<parameter17>]]\" is null");
		}
	}

	public void test_creation() throws Exception {
		Assert.assertNull(new NullPointerException().getMessage(),
				"new NullPointerException().getMessage() is not null!");
		Assert.assertNull(new NullPointerException(null).getMessage(),
				"new NullPointerException(null).getMessage() is not null!");
		String npeMsg = new String("NPE creation messsage");
		Assert.assertEquals(new NullPointerException(npeMsg).getMessage(), npeMsg);
		Exception exception = NullPointerException.class.getDeclaredConstructor().newInstance();
		Assert.assertNull(exception.getMessage());
	}

	public void test_native() throws Exception {
		try {
			Class.forName(null);
			Assert.fail("NullPointerException should have been thrown!");
		} catch (NullPointerException npeEx) {
			Assert.assertNull(npeEx.getMessage());
		}
	}

	public void test_same_msg() throws Exception {
		Object nullObj = null;
		try {
			nullObj.hashCode();
			Assert.fail("NullPointerException should have been thrown!");
		} catch (NullPointerException npeEx) {
			String npeMsg1 = npeEx.getMessage();
			checkMessage(npeMsg1, "Cannot invoke \"java.lang.Object.hashCode()\" because \"nullObj\" is null",
					"Cannot invoke \"java.lang.Object.hashCode()\" because \"<local1>\" is null");
			String npeMsg2 = npeEx.getMessage();
			Assert.assertEquals(npeMsg1, npeMsg2);
		}
	}

	public void test_serialization() throws Exception {
		Object obj1 = new NullPointerException();
		ByteArrayOutputStream baos1 = new ByteArrayOutputStream();
		ObjectOutputStream oos1 = new ObjectOutputStream(baos1);
		oos1.writeObject(obj1);
		ByteArrayInputStream bais1 = new ByteArrayInputStream(baos1.toByteArray());
		ObjectInputStream ois1 = new ObjectInputStream(bais1);
		Exception ex1 = (Exception) ois1.readObject();
		Assert.assertNull(ex1.getMessage());

		String msg2 = "NPE serialization messsage";
		Object obj2 = new NullPointerException(msg2);
		ByteArrayOutputStream baos2 = new ByteArrayOutputStream();
		ObjectOutputStream oos2 = new ObjectOutputStream(baos2);
		oos2.writeObject(obj2);
		ByteArrayInputStream bais2 = new ByteArrayInputStream(baos2.toByteArray());
		ObjectInputStream ois2 = new ObjectInputStream(bais2);
		Exception ex2 = (Exception) ois2.readObject();
		Assert.assertEquals(ex2.getMessage(), msg2);

		Object nullObj3 = null;
		Object obj3 = null;
		String msg3 = null;
		try {
			int hc = nullObj3.hashCode();
			Assert.fail("NullPointerException should have been thrown!");
		} catch (NullPointerException npe3) {
			obj3 = npe3;
			msg3 = npe3.getMessage();
			checkMessage(msg3, "Cannot invoke \"java.lang.Object.hashCode()\" because \"nullObj3\" is null",
					"Cannot invoke \"java.lang.Object.hashCode()\" because \"<local14>\" is null");
		}
		ByteArrayOutputStream baos3 = new ByteArrayOutputStream();
		ObjectOutputStream oos3 = new ObjectOutputStream(baos3);
		oos3.writeObject(obj3);
		ByteArrayInputStream bais3 = new ByteArrayInputStream(baos3.toByteArray());
		ObjectInputStream ois3 = new ObjectInputStream(bais3);
		Exception ex3 = (Exception) ois3.readObject();
		Assert.assertNull(ex3.getMessage());
	}

	private static byte[] generateTestClass() {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;
		cw.visit(50, ACC_SUPER, "org/openj9/test/jep358/E", null, "java/lang/Object",
				new String[] { "org/openj9/test/jep358/E0" });

		{
			mv = cw.visitMethod(0, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}

		{
			mv = cw.visitMethod(ACC_PUBLIC, "throwNPEOne", "(Lorg/openj9/test/jep358/F;)I", null, null);
			mv.visitCode();
			Label label0 = new Label();
			mv.visitLabel(label0);
			mv.visitLineNumber(118, label0);
			mv.visitVarInsn(ALOAD, 1);
			mv.visitFieldInsn(GETFIELD, "org/openj9/test/jep358/F", "iVal", "I");
			mv.visitInsn(IRETURN);
			Label label1 = new Label();
			mv.visitLabel(label1);
			mv.visitLocalVariable("this", "Lorg/openj9/test/jep358/E;", null, label0, label1, 0);
			mv.visitLocalVariable("f", "Lorg/openj9/test/jep358/E;", null, label0, label1, 1);
			mv.visitMaxs(1, 2);
			mv.visitEnd();
		}

		{
			mv = cw.visitMethod(ACC_PUBLIC, "throwNPETwo", "(Ljava/lang/String;)V", null, null);
			mv.visitCode();
			Label label0 = new Label();
			mv.visitLabel(label0);
			mv.visitLineNumber(7, label0);
			mv.visitFieldInsn(GETSTATIC, "java/lang/System", "out", "Ljava/io/PrintStream;");
			mv.visitVarInsn(ALOAD, 1);
			mv.visitInsn(ICONST_1);
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/String", "substring", "(I)Ljava/lang/String;", false);
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/io/PrintStream", "println", "(Ljava/lang/String;)V", false);
			Label label1 = new Label();
			mv.visitLabel(label1);
			mv.visitLineNumber(8, label1);
			mv.visitInsn(ACONST_NULL);
			mv.visitVarInsn(ASTORE, 1);
			Label label2 = new Label();
			mv.visitLabel(label2);
			mv.visitLineNumber(9, label2);
			mv.visitFieldInsn(GETSTATIC, "java/lang/System", "out", "Ljava/io/PrintStream;");
			mv.visitVarInsn(ALOAD, 1);
			mv.visitInsn(ICONST_1);
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/String", "substring", "(I)Ljava/lang/String;", false);
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/io/PrintStream", "println", "(Ljava/lang/String;)V", false);
			Label label3 = new Label();
			mv.visitLabel(label3);
			mv.visitLineNumber(10, label3);
			mv.visitInsn(RETURN);
			mv.visitMaxs(3, 3);
			mv.visitEnd();
		}

		{
			mv = cw.visitMethod(ACC_PUBLIC, "throwNPEThree",
					"(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V", null, null);
			mv.visitCode();
			Label label0 = new Label();
			mv.visitLabel(label0);
			mv.visitLineNumber(12, label0);
			mv.visitFieldInsn(GETSTATIC, "java/lang/System", "out", "Ljava/io/PrintStream;");
			mv.visitVarInsn(ALOAD, 4);
			mv.visitInsn(ICONST_1);
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/String", "substring", "(I)Ljava/lang/String;", false);
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/io/PrintStream", "println", "(Ljava/lang/String;)V", false);
			Label label1 = new Label();
			mv.visitLabel(label1);
			mv.visitLineNumber(13, label1);
			mv.visitInsn(ACONST_NULL);
			mv.visitVarInsn(ASTORE, 4);
			Label label2 = new Label();
			mv.visitLabel(label2);
			mv.visitLineNumber(14, label2);
			mv.visitFieldInsn(GETSTATIC, "java/lang/System", "out", "Ljava/io/PrintStream;");
			mv.visitVarInsn(ALOAD, 4);
			mv.visitInsn(ICONST_1);
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/String", "substring", "(I)Ljava/lang/String;", false);
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/io/PrintStream", "println", "(Ljava/lang/String;)V", false);
			Label label3 = new Label();
			mv.visitLabel(label3);
			mv.visitLineNumber(15, label3);
			mv.visitInsn(RETURN);
			mv.visitMaxs(3, 6);
			mv.visitEnd();
		}

		cw.visitEnd();

		return cw.toByteArray();
	}

	private void changeParameter(String str) {
		str.substring(1);
		str = null;
		str.substring(2);
	}

	public void test_generatedcode() throws Exception {
		byte[] bytes = generateTestClass();
		Lookup lookup = lookup();
		Class<?> clazz = lookup.defineClass(bytes);
		E0 e = (E0) clazz.getDeclaredConstructor().newInstance();
		try {
			e.throwNPEOne(null);
			Assert.fail("NullPointerException should have been thrown!");
		} catch (NullPointerException npe) {
			Assert.assertEquals(npe.getMessage(), "Cannot read field \"iVal\" because \"f\" is null");
		}

		try {
			e.throwNPETwo(null);
		} catch (NullPointerException npe) {
			Assert.assertEquals(npe.getMessage(),
					"Cannot invoke \"java.lang.String.substring(int)\" because \"<parameter1>\" is null");
		}

		try {
			e.throwNPETwo("a1");
		} catch (NullPointerException npe) {
			Assert.assertEquals(npe.getMessage(),
					"Cannot invoke \"java.lang.String.substring(int)\" because \"<local1>\" is null");
		}

		try {
			e.throwNPEThree("a1", "b2", "c3", null);
		} catch (NullPointerException npe) {
			Assert.assertEquals(npe.getMessage(),
					"Cannot invoke \"java.lang.String.substring(int)\" because \"<parameter4>\" is null");
		}

		try {
			e.throwNPEThree("a1", "b3", "c3", "d4");
		} catch (NullPointerException npe) {
			Assert.assertEquals(npe.getMessage(),
					"Cannot invoke \"java.lang.String.substring(int)\" because \"<local4>\" is null");
		}

		if (!hasDebugInfo) {
			try {
				changeParameter(null);
			} catch (NullPointerException npe) {
				Assert.assertEquals(npe.getMessage(),
						"Cannot invoke \"java.lang.String.substring(int)\" because \"<parameter1>\" is null");
			}
			try {
				changeParameter("abc");
			} catch (NullPointerException npe) {
				Assert.assertEquals(npe.getMessage(),
						"Cannot invoke \"java.lang.String.substring(int)\" because \"<local1>\" is null");
			}
		}
	}

	private static long[][] staticLongArray = new long[1000][];

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot store to long array because \"org.openj9.test.jep358.NPEMessageTests.staticLongArray\\[0\\]\" is null")
	public void test_complex_1() {
		staticLongArray[0][0] = 2L;
	}

	private DoubleArrayGen dagClz;

	static interface DoubleArrayGen {
		public double[] getArray();
	}

	static class DoubleArrayGenImpl implements DoubleArrayGen {
		public double[] getArray() {
			return null;
		}
	}

	public void test_complex_2() {
		try {
			NPEMessageTests npeObj = this;
			Object obj = npeObj.dagClz.getArray().clone();
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(),
					"Cannot invoke \"org.openj9.test.jep358.NPEMessageTests$DoubleArrayGen.getArray()\" because \"npeObj.dagClz\" is null",
					"Cannot invoke \"org.openj9.test.jep358.NPEMessageTests$DoubleArrayGen.getArray()\" because \"<local1>.dagClz\" is null");
		}
	}

	public void test_complex_3() {
		try {
			int intArray[] = new int[1];
			NPEMessageTests[] npeObjs = new NPEMessageTests[] { this };
			Object obj = npeObjs[intArray[0]].nullField.returnNull(null, null, 1f);
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(),
					"Cannot invoke \"org.openj9.test.jep358.NPEMessageTests.returnNull(java.lang.String[][], int[][][], float)\" because \"npeObjs[intArray[0]].nullField\" is null",
					"Cannot invoke \"org.openj9.test.jep358.NPEMessageTests.returnNull(java.lang.String[][], int[][][], float)\" because \"<local2>[<local1>[0]].nullField\" is null");
		}
	}

	public void test_complex_4() {
		try {
			int intArray[] = new int[1];
			NPEMessageTests[][] npeObjs = new NPEMessageTests[][] { new NPEMessageTests[] { this } };
			synchronized (npeObjs[intArray[0]][0].nullField) {
			}
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(),
					"Cannot enter synchronized block because \"npeObjs[intArray[0]][0].nullField\" is null",
					"Cannot enter synchronized block because \"<local2>[<local1>[0]][0].nullField\" is null");
		}
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot invoke \"java.lang.String.getBytes\\(\\)\"")
	public void test_complex_5() {
		String str = null;
		byte[] byteArray = (Math.random() < 0.5 ? str : (new String[1])[0]).getBytes();
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot load from int array")
	public void test_complex_6() {
		int[][] intArray1 = new int[1][];
		int[][] intArray2 = new int[2][];
		long index = 0;
		int intVal = (Math.random() < 0.5 ? intArray1[(int) index] : intArray2[(int) index])[13];
	}

	public void test_complex_7() {
		try {
			int[][] intArray1 = new int[1][];
			int[][] intArray2 = new int[2][];
			long index = 0;
			int intVal = (Math.random() < 0.5 ? intArray1 : intArray2)[(int) index][13];
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot load from int array because \"<array>[index]\" is null",
					"Cannot load from int array because \"<array>[<local3>]\" is null");
		}
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot assign field \"intNum\" because \"dClz\" is null")
	public void test_complex_8() {
		C cClz1 = new C();
		C cClz2 = new C();
		(Math.random() < 0.5 ? cClz1 : cClz2).dClz.intNum = 77;
	}

	private static int index17 = 17;

	private int getIndex17() {
		return 17;
	};

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot store to long array because \"org.openj9.test.jep358.NPEMessageTests.staticLongArray\\[org.openj9.test.jep358.NPEMessageTests.index17\\]\" is null")
	public void test_complex_9() {
		staticLongArray[index17][0] = 2L;
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot store to long array because \"org.openj9.test.jep358.NPEMessageTests.staticLongArray\\[org.openj9.test.jep358.NPEMessageTests.getIndex17\\(\\)\\]\" is null")
	public void test_complex_10() {
		staticLongArray[getIndex17()][0] = 2L;
	}

	public void test_complex_11() {
		try {
			Integer aInteger = null;
			int intVal = aInteger;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(),
					"Cannot invoke \"java.lang.Integer.intValue()\" because \"aInteger\" is null",
					"Cannot invoke \"java.lang.Integer.intValue()\" because \"<local1>\" is null");
		}
	}

	public void test_complex_12() {
		try {
			Integer aInteger = null;
			int intVal = aInteger.intValue();
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(),
					"Cannot invoke \"java.lang.Integer.intValue()\" because \"aInteger\" is null",
					"Cannot invoke \"java.lang.Integer.intValue()\" because \"<local1>\" is null");
		}
	}

	static interface InterfaceHelper {
		public int get();
	}

	private InterfaceHelper ifHelper;

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot invoke \"org.openj9.test.jep358.NPEMessageTests\\$InterfaceHelper.get\\(\\)\" because \"this.ifHelper\" is null")
	public void test_invokeinterface() {
		Object obj = this.ifHelper.get();
	}

	static class SuperClass {
		public void testMethod() {
		}

		/*
		 * This generates a class which throws NPE at invokespecial class SubClass
		 * extends SuperClass { public void testMethod() { super = null; return
		 * super.testMethod(); } }
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

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot invoke \"org.openj9.test.jep358.NPEMessageTests\\$SuperClass.testMethod\\(\\)\" because \"null\" is null")
	public void test_invokespecial() throws Exception {
		Class<?> thisClass = this.getClass();
		String subClassInternalName = Type.getInternalName(thisClass).replace(thisClass.getSimpleName(), "SubClass");
		byte[] byteArray = SuperClass.generateSubClass(subClassInternalName);
		Class<?> clazz = MethodHandles.lookup().defineClass(byteArray);
		SuperClass sc = (SuperClass) clazz.getDeclaredConstructor().newInstance();
		sc.testMethod();
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot invoke \"java.lang.Object.toString\\(\\)\" because \"this.nullField\" is null")
	public void test_invokevirtual() {
		String str = nullField.toString();
	}

	private void testMethodPara(String str, String[] strs1, String[][] strs2, StringBuffer sb, StringBuilder sb2,
			Thread thread, NPEMessageTests npemt, int i) {
	};

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot invoke \"org.openj9.test.jep358.NPEMessageTests.testMethodPara\\(java.lang.String, java.lang.String\\[\\], java.lang.String\\[\\]\\[\\], java.lang.StringBuffer, java.lang.StringBuilder, java.lang.Thread, org.openj9.test.jep358.NPEMessageTests, int\\)\" because \"this.nullField\" is null")
	public void test_parameters() {
		nullField.testMethodPara(null, null, null, null, null, null, null, 0);
	}

	private String fool(String[] strs1, String[][] strs2, String[][][] strs3, Void v, byte b, String str1, String str2,
			int a, int i, char c, double d, float f, long l, Object obj, short s, boolean flag) {
		return null;
	}

	private void foo(int i, int j) {
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot invoke \"java.lang.String.getBytes\\(\\)\" because \"java.lang.String\\[...\\]\" is null")
	public void test_branch_1() {
		byte[] byteArray = ((new String[2])[Math.random() < 0.5 ? 0 : 1]).getBytes();
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot invoke \"org.openj9.test.jep358.NPEMessageTests.foo\\(int, int\\)\" because \"this.nullField\" is null")
	public void test_branch_2() {
		nullField.foo(Math.random() < 0.5 ? 1 : 2, 3);
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot invoke \"org.openj9.test.jep358.NPEMessageTests.foo\\(int, int\\)\"")
	public void test_branch_3() {
		NPEMessageTests nonNullField = this;
		(Math.random() < 2 ? nullField : nonNullField).foo(Math.random() > 0.6 ? 1 : 2, 3);
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot invoke \"java.lang.String.getBytes\\(\\)\"")
	public void test_branch_4() {
		String str = null;
		byte[] byteArray = (Math.random() < 0.5 ? str : (new String[1])[0]).getBytes();
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot invoke \"java.lang.String.getBytes\\(\\)\" because \"java.lang.String\\[0\\]\" is null")
	public void test_branch_5() {
		byte[] byteArray = (new String[1])[0].getBytes();
	}

	public void test_npeInsideException() {
		int[][][][] intsMultiDimen = new int[1][][][];
		intsMultiDimen[0] = new int[2][][];
		intsMultiDimen[0][1] = new int[3][];
		intsMultiDimen[0][1][2] = new int[4];
		try {
			throw new RuntimeException("a RuntimeException");
		} catch (RuntimeException re) {
			try {
				intsMultiDimen[0][1][Math.random() < 0.5 ? 0 : 1][2] = 0;
				Assert.fail("NullPointerException should have been thrown!");
			} catch (NullPointerException npe) {
				checkMessage(npe.getMessage(),
						"Cannot store to int array because \"intsMultiDimen[0][1][...]\" is null",
						"Cannot store to int array because \"<local1>[0][1][...]\" is null");
			}
		}
		try {
			intsMultiDimen[0][1][Math.random() < 0.5 ? 0 : 1][2] = 0;
			Assert.fail("NullPointerException should have been thrown!");
		} catch (NullPointerException e) {
			try {
				intsMultiDimen[0][1][Math.random() < 0.5 ? 0 : 1][2] = 0;
				Assert.fail("NullPointerException should have been thrown!");
			} catch (NullPointerException npe) {
				checkMessage(npe.getMessage(),
						"Cannot store to int array because \"intsMultiDimen[0][1][...]\" is null",
						"Cannot store to int array because \"<local1>[0][1][...]\" is null");
			}
		}
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot invoke \"java.lang.invoke.MethodHandle.(invoke\\(java.lang.String\\)\"|linkToVirtual\\(java.lang.Object, java.lang.invoke.MemberName\\)\" because \"<parameter1>\" is null)")
	public void test_MHinvoke() throws Throwable {
		String msg = null;
		MethodHandle mh = MethodHandles.lookup().findVirtual(String.class, "length", MethodType.methodType(int.class));
		mh.invoke(msg);
	}

	@Test(expectedExceptions = NullPointerException.class, expectedExceptionsMessageRegExp = "Cannot invoke \"java.lang.invoke.MethodHandle.(invokeExact\\(java.lang.String\\)\"|linkToVirtual\\(java.lang.Object, java.lang.invoke.MemberName\\)\" because \"<parameter1>\" is null)")
	public void test_MHinvokeExact() throws Throwable {
		String msg = null;
		MethodHandle mh = MethodHandles.lookup().findVirtual(String.class, "length", MethodType.methodType(int.class));
		int len = (int) mh.invokeExact(msg);
	}

	private int getOperatorOne() {
		return 1;
	}

	private int getOperatorTwo() {
		return 0;
	}

	public void test_operators() {
		try {
			Object[][] objsTwoDimen = new Object[2][];
			objsTwoDimen[getOperatorOne() + getOperatorTwo()][1] = this;
		} catch (NullPointerException npe) {
			checkMessage(npe.getMessage(), "Cannot store to object array because \"objsTwoDimen[...]\" is null",
					"Cannot store to object array because \"<local1>[...]\" is null");
		}
	}
}
