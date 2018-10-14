/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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
package com.ibm.j9.jsr292.api;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

public class SamePackageExample {

	/* The int array is placed at the head of the argument list */
	public int addIntArrayToSum(int arg1[], String arg2, long arg3, double arg4) {
		int sum = 0;
		int arrayCount = arg1.length;
		for (int i = 0; i < arrayCount; i++) {
			sum += arg1[i];
		}
		return sum;
	}

	/* The int array is placed in the middle of the argument list */
	public int addIntArrayToSum(long arg1, double arg2, int arg3[], String arg4) {
		int sum = 0;
		int arrayCount = arg3.length;
		for (int i = 0; i < arrayCount; i++) {
			sum += arg3[i];
		}
		return sum;
	}

	/* The int array is placed at the end of the argument list */
	public int addIntArrayToSum(String arg1, long arg2, double arg3, int arg4[]) {
		int sum = 0;
		int arrayCount = arg4.length;
		for (int i = 0; i < arrayCount; i++) {
			sum += arg4[i];
		}
		return sum;
	}

	/* The long array is placed at the head of the argument list */
	public long addLongArrayToSum(long arg1[], String arg2, long arg3, double arg4) {
		long sum = 0;
		int arrayCount = arg1.length;
		for (int i = 0; i < arrayCount; i++) {
			sum += arg1[i];
		}
		return sum;
	}

	/* The long array is placed in the middle of the argument list */
	public long addLongArrayToSum(int arg1, double arg2, long arg3[], String arg4) {
		long sum = 0;
		int arrayCount = arg3.length;
		for (int i = 0; i < arrayCount; i++) {
			sum += arg3[i];
		}
		return sum;
	}

	/* The long array is placed at the end of the argument list */
	public long addLongArrayToSum(int arg1, String arg2, double arg3, long arg4[]) {
		long sum = 0;
		int arrayCount = arg4.length;
		for (int i = 0; i < arrayCount; i++) {
			sum += arg4[i];
		}
		return sum;
	}

	/* The String array is placed at the head of the argument list */
	public String addStringArrayToString(String arg1[], String arg2, long arg3, double arg4) {
		String sum = "[";
		int arrayCount = arg1.length;
		for (int i = 0; i < arrayCount; i++) {
			sum += arg1[i];
		}
		return sum + "]";
	}

	/* The String array is placed in the middle of the argument list */
	public String addStringArrayToString(long arg1, int arg2, String arg3[], int arg4) {
		String sum = "[";
		int arrayCount = arg3.length;
		for (int i = 0; i < arrayCount; i++) {
			sum += arg3[i];
		}
		return sum + "]";
	}

	/* The String array is placed at the end of the argument list */
	public String addStringArrayToString(double arg1, long arg2, int arg3, String arg4[]) {
		String sum = "[";
		int arrayCount = arg4.length;
		for (int i = 0; i < arrayCount; i++) {
			sum += arg4[i];
		}
		return sum + "]";
	}

	/* The int array will spread its elements at the head of the argument list */
	public int addIntArrayToSum(int arg1, int arg2, int arg3, String arg4, long arg5, double arg6) {
		return arg1 + arg2 + arg3;
	}

	/* The int array will spread its elements in the middle of the argument list */
	public int addIntArrayToSum(long arg1, double arg2, int arg3, int arg4, int arg5, String arg6) {
		return arg3 + arg4 + arg5;
	}

	/* The int array will spread its elements at the end of the argument list */
	public int addIntArrayToSum(String arg1, long arg2, double arg3, int arg4, int arg5, int arg6) {
		return arg4 + arg5 + arg6;
	}

	/* The long array will spread its elements at the head of the argument list */
	public long addLongArrayToSum(long arg1, long arg2, String arg3, long arg4, double arg5) {
		return arg1 + arg2;
	}

	/* The long array will spread its elements in the middle of the argument list */
	public long addLongArrayToSum(int arg1, double arg2, long arg3,  long arg4, String arg5) {
		return arg3 + arg4;
	}

	/* The long array will spread its elements at the end of the argument list */
	public long addLongArrayToSum(int arg1, String arg2, double arg3, long arg4, long arg5) {
		return arg4 + arg5;
	}

	/* The String array will spread its elements at the head of the argument list */
	public String addStringArrayToString(String arg1, String arg2, String arg3, long arg4, double arg5) {
		return "[" + arg1 + arg2 + arg3 + "]";
	}

	/* The String array will spread its elements in the middle of the argument list */
	public String addStringArrayToString(long arg1, int arg2, String arg3, String arg4, String arg5, int arg6) {
		return "[" + arg3 + arg4 + arg5 + "]";
	}

	/* The String array will spread its elements at the end of the argument list */
	public String addStringArrayToString(double arg1, long arg2, int arg3, String arg4, String arg5, String arg6) {
		return "[" + arg4 + arg5 + arg6 + "]";
	}

	public static void combinerMethod(int arg) {
	}

	public static int combinerMethod(int arg1, int arg2) {
		return Math.addExact(arg1, arg2);
	}

	public static long combinerMethod(long arg1, long arg2) {
		return Math.addExact(arg1, arg2);
	}

	public static String combinerMethod(String arg1, String arg2) {
		return "[" + arg1 + arg2 + "]";
	}

	public static String targetMethod(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6) {
		return "[" + arg1 + " " + arg2 + " " + arg3 + " " + arg4 + " " + arg5 + " " + arg6 + "]";
	}

	public static String targetMethod(long arg1, long arg2, long arg3, long arg4, long arg5, long arg6) {
		return "[" + arg1 + " " + arg2 + " " + arg3 + " " + arg4 + " " + arg5 + " " + arg6 + "]";
	}

	public static String targetMethod(String arg1, String arg2, String arg3, String arg4, String arg5, String arg6) {
		return "[" + arg1 + " " + arg2 + " " + arg3 + " " + arg4 + " " + arg5 + " " + arg6 + "]";
	}

	public static int nonVarArityMethod(int arg1, int arg2) {
		return arg1 + arg2;
	}

	public static String fixedArityMethod(int arg1, String[] args) {
		String str = "[";
		int argSize = args.length;
		for(int i = 0; i < argSize; i++) {
			str += args[i];
			if ((i + 1) < argSize) {
				str += ", ";
			}
		}
		return str + "]";
	}

	public static String varArityMethod(int arg1, String... args) {
		String str = "[";
		int argSize = args.length;
		for(int i = 0; i < argSize; i++) {
			str += args[i];
			if ((i + 1) < argSize) {
				str += ", ";
			}
		}
		return str + "]";
	}

	public static void tryIntMethod_VoidReturnType(int arg1, int arg2, int arg3) {
	}

	public static void tryIntMethod_VoidReturnType_Throwable(int arg1, int arg2, int arg3) throws Throwable {
		throw new Throwable("tryIntMethod_VoidReturnType_Throwable");
	}

	public static void finallyIntMethod(Throwable t, int arg1, int arg2) throws Throwable {
		if (null == t) {
			throw new Throwable("[NO Throwable, " + arg1 + ", " + arg2 + "]");
		} else if (t.getMessage().equals("tryIntMethod_VoidReturnType_Throwable")){
			throw new Throwable("[Throwable, " + arg1 + ", " + arg2 + "]");
		} else {
			throw new Throwable("tryIntMethod_Unknown");
		}
	}

	public static int tryIntMethod_IntReturnType(int arg1, int arg2, int arg3) {
		return (arg1 + arg2 + arg3);
	}

	public static int tryIntMethod_IntReturnType_Throwable(int arg1, int arg2, int arg3) throws Throwable {
		throw new Throwable("tryIntMethod_IntReturnType_Throwable");
	}

	public static int finallyIntMethod(Throwable t, int result, int arg1, int arg2) throws Throwable {
		if (null == t) {
			return (result + arg1 + arg2);
		} else if (t.getMessage().equals("tryIntMethod_IntReturnType_Throwable")) {
			/* The value of result doesn't really exist in the case that exception is thrown out by the try handle */
			throw new Throwable("[Throwable, " + result + ", " + arg1 + ", " + arg2 + "]");
		} else {
			throw new Throwable("tryIntMethod_Unknown");
		}
	}

	public static void tryStringMethod_VoidReturnType(String arg1, String arg2, String arg3) {
	}

	public static void tryStringMethod_VoidReturnType_Throwable(String arg1, String arg2, String arg3) throws Throwable {
		throw new Throwable("tryStringMethod_VoidReturnType_Throwable");
	}

	public static void finallyStringMethod(Throwable t, String arg1, String arg2) throws Throwable {
		if (null == t) {
			throw new Throwable("[NO Throwable, " + arg1 + ", " + arg2 + "]");
		} else if (t.getMessage().equals("tryStringMethod_VoidReturnType_Throwable")){
			throw new Throwable("[Throwable, " + arg1 + ", " + arg2 + "]");
		} else {
			throw new Throwable("tryStringMethod_Unknown");
		}
	}

	public static String tryStringMethod_StringReturnType(String arg1, String arg2, String arg3) {
		return "[" + arg1 + "-" + arg2 + "-" + arg3 + "]";
	}

	public static String tryStringMethod_StringReturnType_Throwable(String arg1, String arg2, String arg3) throws Throwable {
		throw new Throwable("tryStringMethod_StringReturnType_Throwable");
	}

	public static String finallyStringMethod(Throwable t, String result, String arg1, String arg2) throws Throwable {
		if (null == t) {
			return "[" + result + ", " + arg1 + ", " + arg2 + "]";
		} else if (t.getMessage().equals("tryStringMethod_StringReturnType_Throwable")) {
			/* The value of result doesn't really exist in the case that exception is thrown out by the try handle */
			throw new Throwable("[Throwable, " + result + ", " + arg1 + ", " + arg2 + "]");
		} else {
			throw new Throwable("tryStringMethod_Unknown");
		}
	}

	public static void tryBooleanMethod_VoidReturnType(boolean arg1, boolean arg2, boolean arg3) {
	}

	public static void tryBooleanMethod_VoidReturnType_Throwable(boolean arg1, boolean arg2, boolean arg3) throws Throwable {
		throw new Throwable("tryBooleanMethod_VoidReturnType_Throwable");
	}

	public static void finallyBooleanMethod(Throwable t, boolean arg1, boolean arg2) throws Throwable {
		if (null == t) {
			throw new Throwable("[NO Throwable, " + arg1 + ", " + arg2 + "]");
		} else if (t.getMessage().equals("tryBooleanMethod_VoidReturnType_Throwable")){
			throw new Throwable("[Throwable, " + arg1 + ", " + arg2 + "]");
		} else {
			throw new Throwable("tryBooleanMethod_Unknown");
		}
	}

	public static boolean tryBooleanMethod_BooleanReturnType(boolean arg1, boolean arg2, boolean arg3) {
		return (arg1 || arg2 || arg3);
	}

	public static boolean tryBooleanMethod_BooleanReturnType_Throwable(boolean arg1, boolean arg2, boolean arg3) throws Throwable {
		throw new Throwable("tryBooleanMethod_BooleanReturnType_Throwable");
	}

	public static boolean finallyBooleanMethod(Throwable t, boolean result, boolean arg1, boolean arg2) throws Throwable {
		if (null == t) {
			return (result || arg1 || arg2);
		} else if (t.getMessage().equals("tryBooleanMethod_BooleanReturnType_Throwable")) {
			/* The value of result doesn't really exist in the case that exception is thrown out by the try handle */
			throw new Throwable("[Throwable, " + result + ", " + arg1 + ", " + arg2 + "]");
		} else {
			throw new Throwable("tryIntMethod_Unknown");
		}
	}

	public static void tryCharMethod_VoidReturnType(char arg1, char arg2, char arg3) {
	}

	public static void tryCharMethod_VoidReturnType_Throwable(char arg1, char arg2, char arg3) throws Throwable {
		throw new Throwable("tryCharMethod_VoidReturnType_Throwable");
	}

	public static void finallyCharMethod(Throwable t, char arg1, char arg2) throws Throwable {
		if (null == t) {
			throw new Throwable("[NO Throwable, " + arg1 + ", " + arg2 + "]");
		} else if (t.getMessage().equals("tryCharMethod_VoidReturnType_Throwable")){
			throw new Throwable("[Throwable, " + arg1 + ", " + arg2 + "]");
		} else {
			throw new Throwable("tryCharMethod_Unknown");
		}
	}

	public static char tryCharMethod_CharReturnType(char arg1, char arg2, char arg3) {
		return arg3;
	}

	public static char tryCharMethod_CharReturnType_Throwable(char arg1, char arg2, char arg3) throws Throwable {
		throw new Throwable("tryCharMethod_CharReturnType_Throwable");
	}

	public static char finallyCharMethod(Throwable t, char result, char arg1, char arg2) throws Throwable {
		if (null == t) {
			return result;
		} else if (t.getMessage().equals("tryCharMethod_CharReturnType_Throwable")) {
			/* The value of result doesn't exist in the case exception thrown by the try handle */
			throw new Throwable("[Throwable, " + (('\0' == result) ? '0' : result) + ", " + arg1 + ", " + arg2 + "]");
		} else {
			throw new Throwable("tryCharMethod_Unknown");
		}
	}

	public static void tryByteMethod_VoidReturnType(byte arg1, byte arg2, byte arg3) {
	}

	public static void tryByteMethod_VoidReturnType_Throwable(byte arg1, byte arg2, byte arg3) throws Throwable {
		throw new Throwable("tryByteMethod_VoidReturnType_Throwable");
	}

	public static void finallyByteMethod(Throwable t, byte arg1, byte arg2) throws Throwable {
		if (null == t) {
			throw new Throwable("[NO Throwable, " + arg1 + ", " + arg2 + "]");
		} else if (t.getMessage().equals("tryByteMethod_VoidReturnType_Throwable")){
			throw new Throwable("[Throwable, " + arg1 + ", " + arg2 + "]");
		} else {
			throw new Throwable("tryByteMethod_Unknown");
		}
	}

	public static byte tryByteMethod_ByteReturnType(byte arg1, byte arg2, byte arg3) {
		return arg3;
	}

	public static byte tryByteMethod_ByteReturnType_Throwable(byte arg1, byte arg2, byte arg3) throws Throwable {
		throw new Throwable("tryByteMethod_ByteReturnType_Throwable");
	}

	public static byte finallyByteMethod(Throwable t, byte result, byte arg1, byte arg2) throws Throwable {
		if (null == t) {
			return result;
		} else if (t.getMessage().equals("tryByteMethod_ByteReturnType_Throwable")) {
			/* The value of result doesn't exist in the case exception thrown by the try handle */
			throw new Throwable("[Throwable, " + result + ", " + arg1 + ", " + arg2 + "]");
		} else {
			throw new Throwable("tryByteMethod_Unknown");
		}
	}

	public static void tryShortMethod_VoidReturnType(short arg1, short arg2, short arg3) {
	}

	public static void tryShortMethod_VoidReturnType_Throwable(short arg1, short arg2, short arg3) throws Throwable {
		throw new Throwable("tryShortMethod_VoidReturnType_Throwable");
	}

	public static void finallyShortMethod(Throwable t, short arg1, short arg2) throws Throwable {
		if (null == t) {
			throw new Throwable("[NO Throwable, " + arg1 + ", " + arg2 + "]");
		} else if (t.getMessage().equals("tryShortMethod_VoidReturnType_Throwable")){
			throw new Throwable("[Throwable, " + arg1 + ", " + arg2 + "]");
		} else {
			throw new Throwable("tryShortMethod_Unknown");
		}
	}

	public static short tryShortMethod_ShortReturnType(short arg1, short arg2, short arg3) {
		return arg3;
	}

	public static short tryShortMethod_ShortReturnType_Throwable(short arg1, short arg2, short arg3) throws Throwable {
		throw new Throwable("tryShortMethod_ShortReturnType_Throwable");
	}

	public static short finallyShortMethod(Throwable t, short result, short arg1, short arg2) throws Throwable {
		if (null == t) {
			return result;
		} else if (t.getMessage().equals("tryShortMethod_ShortReturnType_Throwable")) {
			/* The value of result doesn't exist in the case exception thrown by the try handle */
			throw new Throwable("[Throwable, " + result + ", " + arg1 + ", " + arg2 + "]");
		} else {
			throw new Throwable("tryShortMethod_Unknown");
		}
	}

	public static void tryLongMethod_VoidReturnType(long arg1, long arg2, long arg3) {
	}

	public static void tryLongMethod_VoidReturnType_Throwable(long arg1, long arg2, long arg3) throws Throwable {
		throw new Throwable("tryLongMethod_VoidReturnType_Throwable");
	}

	public static void finallyLongMethod(Throwable t, long arg1, long arg2) throws Throwable {
		if (null == t) {
			throw new Throwable("[NO Throwable, " + arg1 + ", " + arg2 + "]");
		} else if (t.getMessage().equals("tryLongMethod_VoidReturnType_Throwable")){
			throw new Throwable("[Throwable, " + arg1 + ", " + arg2 + "]");
		} else {
			throw new Throwable("tryLongMethod_Unknown");
		}
	}

	public static long tryLongMethod_LongReturnType(long arg1, long arg2, long arg3) {
		return arg3;
	}

	public static long tryLongMethod_LongReturnType_Throwable(long arg1, long arg2, long arg3) throws Throwable {
		throw new Throwable("tryLongMethod_LongReturnType_Throwable");
	}

	public static long finallyLongMethod(Throwable t, long result, long arg1, long arg2) throws Throwable {
		if (null == t) {
			return result;
		} else if (t.getMessage().equals("tryLongMethod_LongReturnType_Throwable")) {
			/* The value of result doesn't exist in the case exception thrown by the try handle */
			throw new Throwable("[Throwable, " + result + ", " + arg1 + ", " + arg2 + "]");
		} else {
			throw new Throwable("tryLongMethod_Unknown");
		}
	}

	public static void tryFloatMethod_VoidReturnType(float arg1, float arg2, float arg3) {
	}

	public static void tryFloatMethod_VoidReturnType_Throwable(float arg1, float arg2, float arg3) throws Throwable {
		throw new Throwable("tryFloatMethod_VoidReturnType_Throwable");
	}

	public static void finallyFloatMethod(Throwable t, float arg1, float arg2) throws Throwable {
		if (null == t) {
			throw new Throwable("[NO Throwable, " + arg1 + ", " + arg2 + "]");
		} else if (t.getMessage().equals("tryFloatMethod_VoidReturnType_Throwable")){
			throw new Throwable("[Throwable, " + arg1 + ", " + arg2 + "]");
		} else {
			throw new Throwable("tryFloatMethod_Unknown");
		}
	}

	public static float tryFloatMethod_FloatReturnType(float arg1, float arg2, float arg3) {
		return arg3;
	}

	public static float tryFloatMethod_FloatReturnType_Throwable(float arg1, float arg2, float arg3) throws Throwable {
		throw new Throwable("tryFloatMethod_FloatReturnType_Throwable");
	}

	public static float finallyFloatMethod(Throwable t, float result, float arg1, float arg2) throws Throwable {
		if (null == t) {
			return result;
		} else if (t.getMessage().equals("tryFloatMethod_FloatReturnType_Throwable")) {
			/* The value of result doesn't exist in the case exception thrown by the try handle */
			throw new Throwable("[Throwable, " + result + ", " + arg1 + ", " + arg2 + "]");
		} else {
			throw new Throwable("tryFloatMethod_Unknown");
		}
	}

	public static void tryDoubleMethod_VoidReturnType(double arg1, double arg2, double arg3) {
	}

	public static void tryDoubleMethod_VoidReturnType_Throwable(double arg1, double arg2, double arg3) throws Throwable {
		throw new Throwable("tryDoubleMethod_VoidReturnType_Throwable");
	}

	public static void finallyDoubleMethod(Throwable t, double arg1, double arg2) throws Throwable {
		if (null == t) {
			throw new Throwable("[NO Throwable, " + arg1 + ", " + arg2 + "]");
		} else if (t.getMessage().equals("tryDoubleMethod_VoidReturnType_Throwable")){
			throw new Throwable("[Throwable, " + arg1 + ", " + arg2 + "]");
		} else {
			throw new Throwable("tryDoubleMethod_Unknown");
		}
	}

	public static double tryDoubleMethod_DoubleReturnType(double arg1, double arg2, double arg3) {
		return arg3;
	}

	public static double tryDoubleMethod_DoubleReturnType_Throwable(double arg1, double arg2, double arg3) throws Throwable {
		throw new Throwable("tryDoubleMethod_DoubleReturnType_Throwable");
	}

	public static double finallyDoubleMethod(Throwable t, double result, double arg1, double arg2) throws Throwable {
		if (null == t) {
			return result;
		} else if (t.getMessage().equals("tryDoubleMethod_DoubleReturnType_Throwable")) {
			/* The value of result doesn't exist in the case exception thrown by the try handle */
			throw new Throwable("[Throwable, " + result + ", " + arg1 + ", " + arg2 + "]");
		} else {
			throw new Throwable("tryDoubleMethod_Unknown");
		}
	}
	
	public static int loop_Init(int a) {
		return 1;
	}
	
	public static int loop_Step(int v1, int v2, int a) {
		return v1 + 1;
	}
	
	public static boolean loop_Pred(int v1, int v2, int a) {
		return (v1 < a);
	}
	
	public static int loop_Fini(int v1, int v2, int a) {
		return v2;
	}
	
	public static int loop_Step2(int v2) {
		return (v2 + 3);
	}
	
	public static boolean loop_Pred2(int v2, int a) {
		return (v2 < a);
	}
	
	public static int loop_Fini2(int v2) {
		return v2;
	}
	
	public static short loop_Step1_ShortType(short v1) {
		return (short)(v1 + 1);
	}
	
	public static short loop_Step2_ShortType(short v1, short v2) {
		return (short)(v1 + v2);
	}
	
	public static boolean loop_Pred2_ShortType(short v1, short v2, short a1, short a2) {
		return (v1 < a1) && (v2 < a2);
	}
	
	public static short loop_Fini2_ShortType(short v1, short v2) {
		return v2;
	}
	
	public static byte loop_Step1_ByteType(byte v1) {
		return (byte)(v1 + 1);
	}
	
	public static byte loop_Step2_ByteType(byte v1, byte v2) {
		return (byte)(v1 + v2);
	}
	
	public static boolean loop_Pred2_ByteType(byte v1, byte v2, byte a1, byte a2) {
		return (v1 < a1) && (v2 < a2);
	}
	
	public static byte loop_Fini2_ByteType(byte v1, byte v2) {
		return v2;
	}
	
	public static char loop_Step1_CharType(char v1) {
		return (char)(v1 + 1);
	}
	
	public static char loop_Step2_CharType(char v1, char v2) {
		return (char)(v1 + v2);
	}
	
	public static boolean loop_Pred2_CharType(char v1, char v2, char a1, char a2) {
		return (v1 < a1) && (v2 < a2);
	}
	
	public static char loop_Fini2_CharType(char v1, char v2) {
		return v2;
	}
	
	public static int loop_Step1_IntType(int v1) {
		return (v1 + 1);
	}
	
	public static int loop_Step2_IntType(int v1, int v2) {
		return (v1 + v2);
	}
	
	public static boolean loop_Pred2_IntType(int v1, int v2, int a1, int a2) {
		return (v1 < a1) && (v2 < a2);
	}
	
	public static int loop_Fini2_IntType(int v1, int v2) {
		return v2;
	}
	
	public static long loop_Step1_LongType(long v1) {
		return (v1 + 1);
	}
	
	public static long loop_Step2_LongType(long v1, long v2) {
		return (v1 + v2);
	}
	
	public static boolean loop_Pred2_LongType(long v1, long v2, long a1, long a2) {
		return (v1 < a1) && (v2 < a2);
	}
	
	public static long loop_Fini2_LongType(long v1, long v2) {
		return v2;
	}
	
	public static float loop_Step1_FloatType(float v1) {
		return (v1 + 1);
	}
	
	public static float loop_Step2_FloatType(float v1, float v2) {
		return (v1 + v2);
	}
	
	public static boolean loop_Pred2_FloatType(float v1, float v2, float a1, float a2) {
		return (v1 < a1) && (v2 < a2);
	}
	
	public static float loop_Fini2_FloatType(float v1, float v2) {
		return v2;
	}
	
	public static double loop_Step1_DoubleType(double v1) {
		return (v1 + 1);
	}
	
	public static double loop_Step2_DoubleType(double v1, double v2) {
		return (v1 + v2);
	}
	
	public static boolean loop_Pred2_DoubleType(double v1, double v2, double a1, double a2) {
		return (v1 < a1) && (v2 < a2);
	}
	
	public static double loop_Fini2_DoubleType(double v1, double v2) {
		return v2;
	}
	
	public static int whileLoop_Init_InvalidParamType(byte a1, int a2) {
		return 1;
	}
	
	public static boolean whileLoop_Pred(int v, int a) {
		return (v < a);
	}
	
	public static int whileLoop_Body(int v, int a) {
		return (v + 1);
	}
	
	public static byte while_Init_ByteType(byte a) {
		return 1;
	}
	
	public static boolean while_Pred_ByteType(byte v, byte a) {
		return (v < a);
	}
	
	public static byte while_Body_ByteType(byte v, byte a) {
		return (byte) (v + 2);
	}
	
	public static char while_Init_CharType(char a) {
		return 1;
	}
	
	public static boolean while_Pred_CharType(char v, char a) {
		return (v < a);
	}
	
	public static char while_Body_CharType(char v, char a) {
		return (char) (v + 1);
	}
	
	public static short while_Init_ShortType(short a) {
		return 1;
	}
	
	public static boolean while_Pred_ShortType(short v, short a) {
		return (v < a);
	}
	
	public static short while_Body_ShortType(short v, short a) {
		return (short) (v + 2);
	}
	
	public static int while_Init_IntType(int a) {
		return 1;
	}
	
	public static boolean while_Pred_IntType(int v, int a) {
		return (v < a);
	}
	
	public static int while_Body_IntType(int v, int a) {
		return (v + 2);
	}
	
	public static long while_Init_LongType(long a) {
		return 1;
	}
	
	public static boolean while_Pred_LongType(long v, long a) {
		return (v < a);
	}
	
	public static long while_Body_LongType(long v, long a) {
		return (v + 2);
	}
	
	public static float while_Init_FloatType(float a) {
		return 1;
	}
	
	public static boolean while_Pred_FloatType(float v, float a) {
		return (v < a);
	}
	
	public static float while_Body_FloatType(float v, float a) {
		return (v + 2);
	}
	
	public static double while_Init_DoubleType(double a) {
		return 1;
	}
	
	public static boolean while_Pred_DoubleType(double v, double a) {
		return (v < a);
	}
	
	public static double while_Body_DoubleType(double v, double a) {
		return (v + 2);
	}
	
	public static String while_Init_StringType(String a) {
		return a;
	}
	
	public static boolean while_Pred_StringType(String v, String a) {
		return (!v.contentEquals("str str str str str"));
	}
	
	public static String while_Body_StringType(String v, String a) {
		return (v + " " + a);
	}
	
	public static void while_Init_VoidReturnType(int a) {
	}
	
	public static void while_Body_VoidReturnType(int a) throws IOException {
		throw new IOException("while_Body_VoidReturnType");
	}
	
	public static void countedLoop_Body_InvalidParamType_VoidReturnType(char v) {
	}
	
	public static void countedLoop_Body_VoidReturnType(int counter) {
	}
	
	public static int countedLoop_Body_IntReturnType(byte v) {
		return (int)v;
	}
	
	public static int countedLoop_Body_IntReturnType(int v) {
		return v;
	}
	
	public static int countedLoop_Body_IntReturnType(int v, byte counter) {
		return counter;
	}
	
	public static byte countedLoop_Body_ByteType(byte v, int counter, byte initValue) {
		return (byte) (v + 1);
	}
	
	public static char countedLoop_Body_CharType(char v, int counter, char initValue) {
		return (char) (v + 1);
	}
	
	public static short countedLoop_Body_ShortType(short v, int counter, short initValue) {
		return (short) (v + 1);
	}
	
	public static int countedLoop_Body_IntType(int v, int counter, int initValue) {
		return (v + 1);
	}
	
	public static int countedLoop_Body_NullInit(int v, int counter) {
		return (v + 1);
	}
	
	public static long countedLoop_Body_LongType(long v, int counter, long initValue) {
		return (v + 1l);
	}
	
	public static float countedLoop_Body_FloatType(float v, int counter, float initValue) {
		return (v + 0.5f);
	}
	
	public static double countedLoop_Body_DoubleType(double v, int counter, double initValue) {
		return (v + 0.5);
	}
	
	public static String countedLoop_Body_StringType(String v, int counter, String initValue) {
		return (v + " " + counter);
	}
	
		
	public static Iterator iteratedLoop_Iterator(List a1, int a2) {
		return a1.iterator();
	}
	
	public static int iteratedLoop_Init(List a1, int a2) {
		return (int)a1.get(a2);
	}
	
	public static int iteratedLoop_Body(int v, Integer t) {
		return (v + t);
	}
	
	public static byte iteratedLoop_Init_InvalidReturnType(List a1, int a2) {
		return (byte)a1.get(a2);
	}
	
	public static int iteratedLoop_Iterator_InvalidReturnType(Iterator itr, int a) {
		return a;
	}
	
	public static String iteratedLoop_Init_AllTypes() {
		return "";
	}
	
	public static Iterator<Byte> iteratedLoop_Iterator_ByteType(List<Byte> a) {
		return a.iterator();
	}
	
	public static String iteratedLoop_Body_ByteType(String v, Byte t) {
		return t + " " + v;
	}
	
	public static Iterator<Character> iteratedLoop_Iterator_CharType(List<Character> a) {
		return a.iterator();
	}
	
	public static String iteratedLoop_Body_CharType(String v, Character t) {
		return t + " " + v;
	}
	
	public static Iterator<Short> iteratedLoop_Iterator_ShortType(List<Short> a) {
		return a.iterator();
	}
	
	public static String iteratedLoop_Body_ShortType(String v, Short t) {
		return t + " " + v;
	}
	
	public static Iterator<Integer> iteratedLoop_Iterator_IntType(List<Integer> a) {
		return a.iterator();
	}
	
	public static String iteratedLoop_Body_IntType(String v, Integer t) {
		return t + " " + v;
	}
	
	public static Iterator<Long> iteratedLoop_Iterator_LongType(List<Long> a) {
		return a.iterator();
	}
	
	public static String iteratedLoop_Body_LongType(String v, Long t) {
		return t + " " + v;
	}
	
	public static Iterator<Float> iteratedLoop_Iterator_FloatType(List<Float> a) {
		return a.iterator();
	}
	
	public static String iteratedLoop_Body_FloatType(String v, Float t) {
		return t + " " + v;
	}
	
	public static Iterator<Double> iteratedLoop_Iterator_DoubleType(List<Double> a) {
		return a.iterator();
	}
	
	public static String iteratedLoop_Body_DoubleType(String v, Double t) {
		return t + " " + v;
	}
	
	public static List<String> iteratedLoop_Init_StringType() {
		List<String> list = new ArrayList<String>();
		return list;
	}
	
	public static Iterator<String> iteratedLoop_Iterator_StringType(List<String> a) {
		return a.iterator();
	}
	
	public static List<String> iteratedLoop_Body_StringType(List<String> v, String t) {
		v.add(0, t.toUpperCase());
		return v;
	}
	
	public static void iteratedLoop_Init_VoidReturnType() {
	}
	
	public static void iteratedLoop_Body_VoidReturnType(Integer t) throws IOException {
		throw new IOException("iteratedLoop_Body_VoidReturnType");
	}
}
