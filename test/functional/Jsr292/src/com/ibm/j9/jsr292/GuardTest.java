/*******************************************************************************
 * Copyright (c) 2014, 2018 IBM Corp. and others
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
package com.ibm.j9.jsr292;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import static java.lang.invoke.MethodType.methodType;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.invoke.WrongMethodTypeException;
import java.util.Arrays;

/*
 * Testing MethodHandles.guardWithTest(MethodHandle test, MethodHandle target, MethodHandle fallback).
 * 
 * Tests guardWithTest with the following FieldTypes for (Parameter/Return Descriptors): B, S, C, I, F, J, D, Ljava/lang/Double;
 * 
 * For each FieldType, guardWithTest is tested for boundary and intermediate conditions.
 * 
 * Example:
 * 
 * Let FieldType = D
 * 
 * target method descriptor: (DDDDD)D
 * fallback method descriptor: (DDDDD)D
 * 
 * test method descriptor variants:
 * 		()Z
 * 		(D)Z
 * 		(DD)Z
 * 		(DDD)Z
 * 		(DDDD)Z
 * 		(DDDDD)Z
 */
public class GuardTest {
    private final static byte constByte1 = (byte) 11;
    private final static byte constByte2 = (byte) -20;
    private final static byte constByte3 = (byte) 116;
    private final static byte constByte4 = (byte) 63;
    private final static byte constByte5 = (byte) -80;
    private final static short constShort1 = (short) 30845;
    private final static short constShort2 = (short) -9542;
    private final static short constShort3 = (short) 12208;
    private final static short constShort4 = (short) -14948;
    private final static short constShort5 = (short) 30753;
    private final static char constCharacter1 = (char) 1263;
    private final static char constCharacter2 = (char) 34205;
    private final static char constCharacter3 = (char) 1967;
    private final static char constCharacter4 = (char) 19093;
    private final static char constCharacter5 = (char) 42319;
    private final static int constInteger1 = (int) 1556086783;
    private final static int constInteger2 = (int) 817758207;
    private final static int constInteger3 = (int) 369491967;
    private final static int constInteger4 = (int) -1745879040;
    private final static int constInteger5 = (int) -716963840;
    private final static long constLong1 = (long) -7.50299697919925e+018;
    private final static long constLong2 = (long) 4.06337276379503e+018;
    private final static long constLong3 = (long) -8.27592726524671e+018;
    private final static long constLong4 = (long) 9.0629313001297e+018;
    private final static long constLong5 = (long) -1.66126531254629e+018;
    private final static float constFloat1 = (float) 2.40039871833801e+038F;
    private final static float constFloat2 = (float) -2.79937478910828e+038F;
    private final static float constFloat3 = (float) 2.48908314427185e+038F;
    private final static float constFloat4 = (float) 3.37104664480591e+038F;
    private final static float constFloat5 = (float) -1.21208971838379e+038F;
    private final static double constDouble1 = (double) 4.22650998259866e+307;
    private final static double constDouble2 = (double) -1.66772685567259e+308;
    private final static double constDouble3 = (double) -6.05942403398263e+307;
    private final static double constDouble4 = (double) 4.75482373042349e+307;
    private final static double constDouble5 = (double) 1.21572509364468e+308;

	/* Test guardWithTest: data-type -> byte */
	@Test(groups = { "level.extended" })
	public void test_guardWithTest_Byte() throws WrongMethodTypeException, Throwable {
		MethodType mtTrueTarget = methodType(byte.class, byte.class, byte.class, byte.class, byte.class, byte.class);
		MethodHandle mhTrueTarget = MethodHandles.publicLookup().findStatic(GuardTest.class, "trueTargetByte", mtTrueTarget);
		
		MethodType mtFalseTarget = methodType(byte.class, byte.class, byte.class, byte.class, byte.class, byte.class);
		MethodHandle mhFalseTarget = MethodHandles.publicLookup().findStatic(GuardTest.class, "falseTargetByte", mtFalseTarget);
		
		MethodType mtTest = methodType(boolean.class);
		MethodHandle mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testByte", mtTest);
		MethodHandle mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardByte (0, constByte1, constByte2, constByte3, constByte4, Byte.MAX_VALUE), 
				(byte) mhGuard.invokeExact(constByte1, constByte2, constByte3, constByte4, Byte.MAX_VALUE));
		
		mtTest = methodType(boolean.class, byte.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testByte", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardByte (1, constByte2, constByte3, constByte4, Byte.MAX_VALUE, constByte5), 
				(byte) mhGuard.invokeExact(constByte2, constByte3, constByte4, Byte.MAX_VALUE, constByte5));
		
		mtTest = methodType(boolean.class, byte.class, byte.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testByte", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardByte (2, constByte2, constByte3, Byte.MIN_VALUE, constByte4, constByte1), 
				(byte) mhGuard.invokeExact(constByte2, constByte3, Byte.MIN_VALUE, constByte4, constByte1));
		
		mtTest = methodType(boolean.class, byte.class, byte.class, byte.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testByte", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardByte (3, Byte.MAX_VALUE, constByte3, Byte.MIN_VALUE, constByte4, constByte1), 
				(byte) mhGuard.invokeExact(Byte.MAX_VALUE, constByte3, Byte.MIN_VALUE, constByte4, constByte1));
		
		mtTest = methodType(boolean.class, byte.class, byte.class, byte.class, byte.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testByte", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardByte (4, constByte1, constByte2, constByte3, constByte4, constByte5), 
				(byte) mhGuard.invokeExact(constByte1, constByte2, constByte3, constByte4, constByte5));
		
		mtTest = methodType(boolean.class, byte.class, byte.class, byte.class, byte.class, byte.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testByte", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardByte (5, Byte.MIN_VALUE, constByte3, constByte5, constByte4, constByte1), 
				(byte) mhGuard.invokeExact(Byte.MIN_VALUE, constByte3, constByte5, constByte4, constByte1));
	}
	
	public static boolean testByte () {
		return testByteGen ();
	}
	
	public static boolean testByte (byte a) {
		return testByteGen (a);
	}
	
	public static boolean testByte (byte a, byte b) {
		return testByteGen (a, b);
	}
	
	public static boolean testByte (byte a, byte b, byte c) {
		return testByteGen (a, b, c);
	}
	
	public static boolean testByte (byte a, byte b, byte c, byte d) {
		return testByteGen (a, b, c, d);
	}
	
	public static boolean testByte (byte a, byte b, byte c, byte d, byte e) {
		return testByteGen (a, b, c, d, e);
	}
	
	public static byte trueTargetByte (byte a, byte b, byte c, byte d, byte e) {
		return largestByte (a, b, c, d, e);
	}
	
	public static byte falseTargetByte (byte a, byte b, byte c, byte d, byte e) {
		return smallestByte (a, b, c, d, e);
	}
	
	public static boolean testByteGen (byte... args) {
		if (0 == args.length) {
			return false;
		} else {
			byte last = args [args.length - 1];
			for (int i = 0; i < (args.length - 1); i++) {
				if (last < args [i]) {
					return false;
				}
			}
			return true;
		}
	}

	public static byte smallestByte (byte... args) {
		byte smallest = Byte.MAX_VALUE;
		for(byte arg : args) {
			if (smallest > arg) {
				smallest = arg;
			}
		}
		return smallest;
	}
	
	public static byte largestByte (byte... args) {
		byte largest = Byte.MIN_VALUE;
		for(byte arg : args) {
			if (largest < arg) {
				largest = arg;
			}
		}
		return largest;
	}
	
	public static byte verifyGuardByte (int type, byte... args) {
		byte[] testArgs;
		
		if (0 == type) {
			testArgs = null;
		} else {
			testArgs = Arrays.copyOfRange(args, 0, type);
		}
		
		if (null == testArgs) {
			return smallestByte (args);
		} else {
			if (true == testByteGen(testArgs)) {
				return largestByte (args);
			} else {
				return smallestByte (args);
			}
		}
	}
	/* Test guardWithTest: data-type -> short */
	@Test(groups = { "level.extended" })
	public void test_guardWithTest_Short() throws WrongMethodTypeException, Throwable {
		MethodType mtTrueTarget = methodType(short.class, short.class, short.class, short.class, short.class, short.class);
		MethodHandle mhTrueTarget = MethodHandles.publicLookup().findStatic(GuardTest.class, "trueTargetShort", mtTrueTarget);
		
		MethodType mtFalseTarget = methodType(short.class, short.class, short.class, short.class, short.class, short.class);
		MethodHandle mhFalseTarget = MethodHandles.publicLookup().findStatic(GuardTest.class, "falseTargetShort", mtFalseTarget);
		
		MethodType mtTest = methodType(boolean.class);
		MethodHandle mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testShort", mtTest);
		MethodHandle mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardShort (0, constShort1, constShort2, constShort3, constShort4, Short.MAX_VALUE), 
				(short) mhGuard.invokeExact(constShort1, constShort2, constShort3, constShort4, Short.MAX_VALUE));
		
		mtTest = methodType(boolean.class, short.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testShort", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardShort (1, constShort2, constShort3, constShort4, Short.MAX_VALUE, constShort5), 
				(short) mhGuard.invokeExact(constShort2, constShort3, constShort4, Short.MAX_VALUE, constShort5));
		
		mtTest = methodType(boolean.class, short.class, short.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testShort", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardShort (2, constShort2, constShort3, Short.MIN_VALUE, constShort4, constShort1), 
				(short) mhGuard.invokeExact(constShort2, constShort3, Short.MIN_VALUE, constShort4, constShort1));
		
		mtTest = methodType(boolean.class, short.class, short.class, short.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testShort", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardShort (3, Short.MAX_VALUE, constShort3, Short.MIN_VALUE, constShort4, constShort1), 
				(short) mhGuard.invokeExact(Short.MAX_VALUE, constShort3, Short.MIN_VALUE, constShort4, constShort1));
		
		mtTest = methodType(boolean.class, short.class, short.class, short.class, short.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testShort", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardShort (4, constShort1, constShort2, constShort3, constShort4, constShort5), 
				(short) mhGuard.invokeExact(constShort1, constShort2, constShort3, constShort4, constShort5));
		
		mtTest = methodType(boolean.class, short.class, short.class, short.class, short.class, short.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testShort", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardShort (5, Short.MIN_VALUE, constShort3, constShort5, constShort4, constShort1), 
				(short) mhGuard.invokeExact(Short.MIN_VALUE, constShort3, constShort5, constShort4, constShort1));
	}
	
	public static boolean testShort () {
		return testShortGen ();
	}
	
	public static boolean testShort (short a) {
		return testShortGen (a);
	}
	
	public static boolean testShort (short a, short b) {
		return testShortGen (a, b);
	}
	
	public static boolean testShort (short a, short b, short c) {
		return testShortGen (a, b, c);
	}
	
	public static boolean testShort (short a, short b, short c, short d) {
		return testShortGen (a, b, c, d);
	}
	
	public static boolean testShort (short a, short b, short c, short d, short e) {
		return testShortGen (a, b, c, d, e);
	}
	
	public static short trueTargetShort (short a, short b, short c, short d, short e) {
		return largestShort (a, b, c, d, e);
	}
	
	public static short falseTargetShort (short a, short b, short c, short d, short e) {
		return smallestShort (a, b, c, d, e);
	}
	
	public static boolean testShortGen (short... args) {
		if (0 == args.length) {
			return false;
		} else {
			short last = args [args.length - 1];
			for (int i = 0; i < (args.length - 1); i++) {
				if (last < args [i]) {
					return false;
				}
			}
			return true;
		}
	}

	public static short smallestShort (short... args) {
		short smallest = Short.MAX_VALUE;
		for(short arg : args) {
			if (smallest > arg) {
				smallest = arg;
			}
		}
		return smallest;
	}
	
	public static short largestShort (short... args) {
		short largest = Short.MIN_VALUE;
		for(short arg : args) {
			if (largest < arg) {
				largest = arg;
			}
		}
		return largest;
	}
	
	public static short verifyGuardShort (int type, short... args) {
		short[] testArgs;
		
		if (0 == type) {
			testArgs = null;
		} else {
			testArgs = Arrays.copyOfRange(args, 0, type);
		}
		
		if (null == testArgs) {
			return smallestShort (args);
		} else {
			if (true == testShortGen(testArgs)) {
				return largestShort (args);
			} else {
				return smallestShort (args);
			}
		}
	}
	/* Test guardWithTest: data-type -> char */
	@Test(groups = { "level.extended" })
	public void test_guardWithTest_Character() throws WrongMethodTypeException, Throwable {
		MethodType mtTrueTarget = methodType(char.class, char.class, char.class, char.class, char.class, char.class);
		MethodHandle mhTrueTarget = MethodHandles.publicLookup().findStatic(GuardTest.class, "trueTargetCharacter", mtTrueTarget);
		
		MethodType mtFalseTarget = methodType(char.class, char.class, char.class, char.class, char.class, char.class);
		MethodHandle mhFalseTarget = MethodHandles.publicLookup().findStatic(GuardTest.class, "falseTargetCharacter", mtFalseTarget);
		
		MethodType mtTest = methodType(boolean.class);
		MethodHandle mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testCharacter", mtTest);
		MethodHandle mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardCharacter (0, constCharacter1, constCharacter2, constCharacter3, constCharacter4, Character.MAX_VALUE), 
				(char) mhGuard.invokeExact(constCharacter1, constCharacter2, constCharacter3, constCharacter4, Character.MAX_VALUE));
		
		mtTest = methodType(boolean.class, char.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testCharacter", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardCharacter (1, constCharacter2, constCharacter3, constCharacter4, Character.MAX_VALUE, constCharacter5), 
				(char) mhGuard.invokeExact(constCharacter2, constCharacter3, constCharacter4, Character.MAX_VALUE, constCharacter5));
		
		mtTest = methodType(boolean.class, char.class, char.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testCharacter", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardCharacter (2, constCharacter2, constCharacter3, Character.MIN_VALUE, constCharacter4, constCharacter1), 
				(char) mhGuard.invokeExact(constCharacter2, constCharacter3, Character.MIN_VALUE, constCharacter4, constCharacter1));
		
		mtTest = methodType(boolean.class, char.class, char.class, char.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testCharacter", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardCharacter (3, Character.MAX_VALUE, constCharacter3, Character.MIN_VALUE, constCharacter4, constCharacter1), 
				(char) mhGuard.invokeExact(Character.MAX_VALUE, constCharacter3, Character.MIN_VALUE, constCharacter4, constCharacter1));
		
		mtTest = methodType(boolean.class, char.class, char.class, char.class, char.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testCharacter", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardCharacter (4, constCharacter1, constCharacter2, constCharacter3, constCharacter4, constCharacter5), 
				(char) mhGuard.invokeExact(constCharacter1, constCharacter2, constCharacter3, constCharacter4, constCharacter5));
		
		mtTest = methodType(boolean.class, char.class, char.class, char.class, char.class, char.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testCharacter", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardCharacter (5, Character.MIN_VALUE, constCharacter3, constCharacter5, constCharacter4, constCharacter1), 
				(char) mhGuard.invokeExact(Character.MIN_VALUE, constCharacter3, constCharacter5, constCharacter4, constCharacter1));
	}
	
	public static boolean testCharacter () {
		return testCharacterGen ();
	}
	
	public static boolean testCharacter (char a) {
		return testCharacterGen (a);
	}
	
	public static boolean testCharacter (char a, char b) {
		return testCharacterGen (a, b);
	}
	
	public static boolean testCharacter (char a, char b, char c) {
		return testCharacterGen (a, b, c);
	}
	
	public static boolean testCharacter (char a, char b, char c, char d) {
		return testCharacterGen (a, b, c, d);
	}
	
	public static boolean testCharacter (char a, char b, char c, char d, char e) {
		return testCharacterGen (a, b, c, d, e);
	}
	
	public static char trueTargetCharacter (char a, char b, char c, char d, char e) {
		return largestCharacter (a, b, c, d, e);
	}
	
	public static char falseTargetCharacter (char a, char b, char c, char d, char e) {
		return smallestCharacter (a, b, c, d, e);
	}
	
	public static boolean testCharacterGen (char... args) {
		if (0 == args.length) {
			return false;
		} else {
			char last = args [args.length - 1];
			for (int i = 0; i < (args.length - 1); i++) {
				if (last < args [i]) {
					return false;
				}
			}
			return true;
		}
	}

	public static char smallestCharacter (char... args) {
		char smallest = Character.MAX_VALUE;
		for(char arg : args) {
			if (smallest > arg) {
				smallest = arg;
			}
		}
		return smallest;
	}
	
	public static char largestCharacter (char... args) {
		char largest = Character.MIN_VALUE;
		for(char arg : args) {
			if (largest < arg) {
				largest = arg;
			}
		}
		return largest;
	}
	
	public static char verifyGuardCharacter (int type, char... args) {
		char[] testArgs;
		
		if (0 == type) {
			testArgs = null;
		} else {
			testArgs = Arrays.copyOfRange(args, 0, type);
		}
		
		if (null == testArgs) {
			return smallestCharacter (args);
		} else {
			if (true == testCharacterGen(testArgs)) {
				return largestCharacter (args);
			} else {
				return smallestCharacter (args);
			}
		}
	}
	/* Test guardWithTest: data-type -> int */
	@Test(groups = { "level.extended" })
	public void test_guardWithTest_Integer() throws WrongMethodTypeException, Throwable {
		MethodType mtTrueTarget = methodType(int.class, int.class, int.class, int.class, int.class, int.class);
		MethodHandle mhTrueTarget = MethodHandles.publicLookup().findStatic(GuardTest.class, "trueTargetInteger", mtTrueTarget);
		
		MethodType mtFalseTarget = methodType(int.class, int.class, int.class, int.class, int.class, int.class);
		MethodHandle mhFalseTarget = MethodHandles.publicLookup().findStatic(GuardTest.class, "falseTargetInteger", mtFalseTarget);
		
		MethodType mtTest = methodType(boolean.class);
		MethodHandle mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testInteger", mtTest);
		MethodHandle mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardInteger (0, constInteger1, constInteger2, constInteger3, constInteger4, Integer.MAX_VALUE), 
				(int) mhGuard.invokeExact(constInteger1, constInteger2, constInteger3, constInteger4, Integer.MAX_VALUE));
		
		mtTest = methodType(boolean.class, int.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testInteger", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardInteger (1, constInteger2, constInteger3, constInteger4, Integer.MAX_VALUE, constInteger5), 
				(int) mhGuard.invokeExact(constInteger2, constInteger3, constInteger4, Integer.MAX_VALUE, constInteger5));
		
		mtTest = methodType(boolean.class, int.class, int.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testInteger", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardInteger (2, constInteger2, constInteger3, Integer.MIN_VALUE, constInteger4, constInteger1), 
				(int) mhGuard.invokeExact(constInteger2, constInteger3, Integer.MIN_VALUE, constInteger4, constInteger1));
		
		mtTest = methodType(boolean.class, int.class, int.class, int.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testInteger", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardInteger (3, Integer.MAX_VALUE, constInteger3, Integer.MIN_VALUE, constInteger4, constInteger1), 
				(int) mhGuard.invokeExact(Integer.MAX_VALUE, constInteger3, Integer.MIN_VALUE, constInteger4, constInteger1));
		
		mtTest = methodType(boolean.class, int.class, int.class, int.class, int.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testInteger", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardInteger (4, constInteger1, constInteger2, constInteger3, constInteger4, constInteger5), 
				(int) mhGuard.invokeExact(constInteger1, constInteger2, constInteger3, constInteger4, constInteger5));
		
		mtTest = methodType(boolean.class, int.class, int.class, int.class, int.class, int.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testInteger", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardInteger (5, Integer.MIN_VALUE, constInteger3, constInteger5, constInteger4, constInteger1), 
				(int) mhGuard.invokeExact(Integer.MIN_VALUE, constInteger3, constInteger5, constInteger4, constInteger1));
	}
	
	public static boolean testInteger () {
		return testIntegerGen ();
	}
	
	public static boolean testInteger (int a) {
		return testIntegerGen (a);
	}
	
	public static boolean testInteger (int a, int b) {
		return testIntegerGen (a, b);
	}
	
	public static boolean testInteger (int a, int b, int c) {
		return testIntegerGen (a, b, c);
	}
	
	public static boolean testInteger (int a, int b, int c, int d) {
		return testIntegerGen (a, b, c, d);
	}
	
	public static boolean testInteger (int a, int b, int c, int d, int e) {
		return testIntegerGen (a, b, c, d, e);
	}
	
	public static int trueTargetInteger (int a, int b, int c, int d, int e) {
		return largestInteger (a, b, c, d, e);
	}
	
	public static int falseTargetInteger (int a, int b, int c, int d, int e) {
		return smallestInteger (a, b, c, d, e);
	}
	
	public static boolean testIntegerGen (int... args) {
		if (0 == args.length) {
			return false;
		} else {
			int last = args [args.length - 1];
			for (int i = 0; i < (args.length - 1); i++) {
				if (last < args [i]) {
					return false;
				}
			}
			return true;
		}
	}

	public static int smallestInteger (int... args) {
		int smallest = Integer.MAX_VALUE;
		for(int arg : args) {
			if (smallest > arg) {
				smallest = arg;
			}
		}
		return smallest;
	}
	
	public static int largestInteger (int... args) {
		int largest = Integer.MIN_VALUE;
		for(int arg : args) {
			if (largest < arg) {
				largest = arg;
			}
		}
		return largest;
	}
	
	public static int verifyGuardInteger (int type, int... args) {
		int[] testArgs;
		
		if (0 == type) {
			testArgs = null;
		} else {
			testArgs = Arrays.copyOfRange(args, 0, type);
		}
		
		if (null == testArgs) {
			return smallestInteger (args);
		} else {
			if (true == testIntegerGen(testArgs)) {
				return largestInteger (args);
			} else {
				return smallestInteger (args);
			}
		}
	}
	/* Test guardWithTest: data-type -> long */
	@Test(groups = { "level.extended" })
	public void test_guardWithTest_Long() throws WrongMethodTypeException, Throwable {
		MethodType mtTrueTarget = methodType(long.class, long.class, long.class, long.class, long.class, long.class);
		MethodHandle mhTrueTarget = MethodHandles.publicLookup().findStatic(GuardTest.class, "trueTargetLong", mtTrueTarget);
		
		MethodType mtFalseTarget = methodType(long.class, long.class, long.class, long.class, long.class, long.class);
		MethodHandle mhFalseTarget = MethodHandles.publicLookup().findStatic(GuardTest.class, "falseTargetLong", mtFalseTarget);
		
		MethodType mtTest = methodType(boolean.class);
		MethodHandle mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testLong", mtTest);
		MethodHandle mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardLong (0, constLong1, constLong2, constLong3, constLong4, Long.MAX_VALUE), 
				(long) mhGuard.invokeExact(constLong1, constLong2, constLong3, constLong4, Long.MAX_VALUE));
		
		mtTest = methodType(boolean.class, long.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testLong", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardLong (1, constLong2, constLong3, constLong4, Long.MAX_VALUE, constLong5), 
				(long) mhGuard.invokeExact(constLong2, constLong3, constLong4, Long.MAX_VALUE, constLong5));
		
		mtTest = methodType(boolean.class, long.class, long.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testLong", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardLong (2, constLong2, constLong3, Long.MIN_VALUE, constLong4, constLong1), 
				(long) mhGuard.invokeExact(constLong2, constLong3, Long.MIN_VALUE, constLong4, constLong1));
		
		mtTest = methodType(boolean.class, long.class, long.class, long.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testLong", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardLong (3, Long.MAX_VALUE, constLong3, Long.MIN_VALUE, constLong4, constLong1), 
				(long) mhGuard.invokeExact(Long.MAX_VALUE, constLong3, Long.MIN_VALUE, constLong4, constLong1));
		
		mtTest = methodType(boolean.class, long.class, long.class, long.class, long.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testLong", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardLong (4, constLong1, constLong2, constLong3, constLong4, constLong5), 
				(long) mhGuard.invokeExact(constLong1, constLong2, constLong3, constLong4, constLong5));
		
		mtTest = methodType(boolean.class, long.class, long.class, long.class, long.class, long.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testLong", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardLong (5, Long.MIN_VALUE, constLong3, constLong5, constLong4, constLong1), 
				(long) mhGuard.invokeExact(Long.MIN_VALUE, constLong3, constLong5, constLong4, constLong1));
	}
	
	public static boolean testLong () {
		return testLongGen ();
	}
	
	public static boolean testLong (long a) {
		return testLongGen (a);
	}
	
	public static boolean testLong (long a, long b) {
		return testLongGen (a, b);
	}
	
	public static boolean testLong (long a, long b, long c) {
		return testLongGen (a, b, c);
	}
	
	public static boolean testLong (long a, long b, long c, long d) {
		return testLongGen (a, b, c, d);
	}
	
	public static boolean testLong (long a, long b, long c, long d, long e) {
		return testLongGen (a, b, c, d, e);
	}
	
	public static long trueTargetLong (long a, long b, long c, long d, long e) {
		return largestLong (a, b, c, d, e);
	}
	
	public static long falseTargetLong (long a, long b, long c, long d, long e) {
		return smallestLong (a, b, c, d, e);
	}
	
	public static boolean testLongGen (long... args) {
		if (0 == args.length) {
			return false;
		} else {
			long last = args [args.length - 1];
			for (int i = 0; i < (args.length - 1); i++) {
				if (last < args [i]) {
					return false;
				}
			}
			return true;
		}
	}

	public static long smallestLong (long... args) {
		long smallest = Long.MAX_VALUE;
		for(long arg : args) {
			if (smallest > arg) {
				smallest = arg;
			}
		}
		return smallest;
	}
	
	public static long largestLong (long... args) {
		long largest = Long.MIN_VALUE;
		for(long arg : args) {
			if (largest < arg) {
				largest = arg;
			}
		}
		return largest;
	}
	
	public static long verifyGuardLong (int type, long... args) {
		long[] testArgs;
		
		if (0 == type) {
			testArgs = null;
		} else {
			testArgs = Arrays.copyOfRange(args, 0, type);
		}
		
		if (null == testArgs) {
			return smallestLong (args);
		} else {
			if (true == testLongGen(testArgs)) {
				return largestLong (args);
			} else {
				return smallestLong (args);
			}
		}
	}
	/* Test guardWithTest: data-type -> float */
	@Test(groups = { "level.extended" })
	public void test_guardWithTest_Float() throws WrongMethodTypeException, Throwable {
		MethodType mtTrueTarget = methodType(float.class, float.class, float.class, float.class, float.class, float.class);
		MethodHandle mhTrueTarget = MethodHandles.publicLookup().findStatic(GuardTest.class, "trueTargetFloat", mtTrueTarget);
		
		MethodType mtFalseTarget = methodType(float.class, float.class, float.class, float.class, float.class, float.class);
		MethodHandle mhFalseTarget = MethodHandles.publicLookup().findStatic(GuardTest.class, "falseTargetFloat", mtFalseTarget);
		
		MethodType mtTest = methodType(boolean.class);
		MethodHandle mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testFloat", mtTest);
		MethodHandle mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardFloat (0, constFloat1, constFloat2, constFloat3, constFloat4, Float.MAX_VALUE), 
				(float) mhGuard.invokeExact(constFloat1, constFloat2, constFloat3, constFloat4, Float.MAX_VALUE));
		
		mtTest = methodType(boolean.class, float.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testFloat", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardFloat (1, constFloat2, constFloat3, constFloat4, Float.MAX_VALUE, constFloat5), 
				(float) mhGuard.invokeExact(constFloat2, constFloat3, constFloat4, Float.MAX_VALUE, constFloat5));
		
		mtTest = methodType(boolean.class, float.class, float.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testFloat", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardFloat (2, constFloat2, constFloat3, Float.MIN_VALUE, constFloat4, constFloat1), 
				(float) mhGuard.invokeExact(constFloat2, constFloat3, Float.MIN_VALUE, constFloat4, constFloat1));
		
		mtTest = methodType(boolean.class, float.class, float.class, float.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testFloat", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardFloat (3, Float.MAX_VALUE, constFloat3, Float.MIN_VALUE, constFloat4, constFloat1), 
				(float) mhGuard.invokeExact(Float.MAX_VALUE, constFloat3, Float.MIN_VALUE, constFloat4, constFloat1));
		
		mtTest = methodType(boolean.class, float.class, float.class, float.class, float.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testFloat", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardFloat (4, constFloat1, constFloat2, constFloat3, constFloat4, constFloat5), 
				(float) mhGuard.invokeExact(constFloat1, constFloat2, constFloat3, constFloat4, constFloat5));
		
		mtTest = methodType(boolean.class, float.class, float.class, float.class, float.class, float.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testFloat", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardFloat (5, Float.MIN_VALUE, constFloat3, constFloat5, constFloat4, constFloat1), 
				(float) mhGuard.invokeExact(Float.MIN_VALUE, constFloat3, constFloat5, constFloat4, constFloat1));
	}
	
	public static boolean testFloat () {
		return testFloatGen ();
	}
	
	public static boolean testFloat (float a) {
		return testFloatGen (a);
	}
	
	public static boolean testFloat (float a, float b) {
		return testFloatGen (a, b);
	}
	
	public static boolean testFloat (float a, float b, float c) {
		return testFloatGen (a, b, c);
	}
	
	public static boolean testFloat (float a, float b, float c, float d) {
		return testFloatGen (a, b, c, d);
	}
	
	public static boolean testFloat (float a, float b, float c, float d, float e) {
		return testFloatGen (a, b, c, d, e);
	}
	
	public static float trueTargetFloat (float a, float b, float c, float d, float e) {
		return largestFloat (a, b, c, d, e);
	}
	
	public static float falseTargetFloat (float a, float b, float c, float d, float e) {
		return smallestFloat (a, b, c, d, e);
	}
	
	public static boolean testFloatGen (float... args) {
		if (0 == args.length) {
			return false;
		} else {
			float last = args [args.length - 1];
			for (int i = 0; i < (args.length - 1); i++) {
				if (last < args [i]) {
					return false;
				}
			}
			return true;
		}
	}

	public static float smallestFloat (float... args) {
		float smallest = Float.MAX_VALUE;
		for(float arg : args) {
			if (smallest > arg) {
				smallest = arg;
			}
		}
		return smallest;
	}
	
	public static float largestFloat (float... args) {
		float largest = Float.MIN_VALUE;
		for(float arg : args) {
			if (largest < arg) {
				largest = arg;
			}
		}
		return largest;
	}
	
	public static float verifyGuardFloat (int type, float... args) {
		float[] testArgs;
		
		if (0 == type) {
			testArgs = null;
		} else {
			testArgs = Arrays.copyOfRange(args, 0, type);
		}
		
		if (null == testArgs) {
			return smallestFloat (args);
		} else {
			if (true == testFloatGen(testArgs)) {
				return largestFloat (args);
			} else {
				return smallestFloat (args);
			}
		}
	}
	/* Test guardWithTest: data-type -> double */
	@Test(groups = { "level.extended" })
	public void test_guardWithTest_Double() throws WrongMethodTypeException, Throwable {
		MethodType mtTrueTarget = methodType(double.class, double.class, double.class, double.class, double.class, double.class);
		MethodHandle mhTrueTarget = MethodHandles.publicLookup().findStatic(GuardTest.class, "trueTargetDouble", mtTrueTarget);
		
		MethodType mtFalseTarget = methodType(double.class, double.class, double.class, double.class, double.class, double.class);
		MethodHandle mhFalseTarget = MethodHandles.publicLookup().findStatic(GuardTest.class, "falseTargetDouble", mtFalseTarget);
		
		MethodType mtTest = methodType(boolean.class);
		MethodHandle mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testDouble", mtTest);
		MethodHandle mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardDouble (0, constDouble1, constDouble2, constDouble3, constDouble4, Double.MAX_VALUE), 
				(double) mhGuard.invokeExact(constDouble1, constDouble2, constDouble3, constDouble4, Double.MAX_VALUE));
		
		mtTest = methodType(boolean.class, double.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testDouble", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardDouble (1, constDouble2, constDouble3, constDouble4, Double.MAX_VALUE, constDouble5), 
				(double) mhGuard.invokeExact(constDouble2, constDouble3, constDouble4, Double.MAX_VALUE, constDouble5));
		
		mtTest = methodType(boolean.class, double.class, double.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testDouble", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardDouble (2, constDouble2, constDouble3, Double.MIN_VALUE, constDouble4, constDouble1), 
				(double) mhGuard.invokeExact(constDouble2, constDouble3, Double.MIN_VALUE, constDouble4, constDouble1));
		
		mtTest = methodType(boolean.class, double.class, double.class, double.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testDouble", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardDouble (3, Double.MAX_VALUE, constDouble3, Double.MIN_VALUE, constDouble4, constDouble1), 
				(double) mhGuard.invokeExact(Double.MAX_VALUE, constDouble3, Double.MIN_VALUE, constDouble4, constDouble1));
		
		mtTest = methodType(boolean.class, double.class, double.class, double.class, double.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testDouble", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardDouble (4, constDouble1, constDouble2, constDouble3, constDouble4, constDouble5), 
				(double) mhGuard.invokeExact(constDouble1, constDouble2, constDouble3, constDouble4, constDouble5));
		
		mtTest = methodType(boolean.class, double.class, double.class, double.class, double.class, double.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testDouble", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardDouble (5, Double.MIN_VALUE, constDouble3, constDouble5, constDouble4, constDouble1), 
				(double) mhGuard.invokeExact(Double.MIN_VALUE, constDouble3, constDouble5, constDouble4, constDouble1));
	}
	
	public static boolean testDouble () {
		return testDoubleGen ();
	}
	
	public static boolean testDouble (double a) {
		return testDoubleGen (a);
	}
	
	public static boolean testDouble (double a, double b) {
		return testDoubleGen (a, b);
	}
	
	public static boolean testDouble (double a, double b, double c) {
		return testDoubleGen (a, b, c);
	}
	
	public static boolean testDouble (double a, double b, double c, double d) {
		return testDoubleGen (a, b, c, d);
	}
	
	public static boolean testDouble (double a, double b, double c, double d, double e) {
		return testDoubleGen (a, b, c, d, e);
	}
	
	public static double trueTargetDouble (double a, double b, double c, double d, double e) {
		return largestDouble (a, b, c, d, e);
	}
	
	public static double falseTargetDouble (double a, double b, double c, double d, double e) {
		return smallestDouble (a, b, c, d, e);
	}
	
	public static boolean testDoubleGen (double... args) {
		if (0 == args.length) {
			return false;
		} else {
			double last = args [args.length - 1];
			for (int i = 0; i < (args.length - 1); i++) {
				if (last < args [i]) {
					return false;
				}
			}
			return true;
		}
	}

	public static double smallestDouble (double... args) {
		double smallest = Double.MAX_VALUE;
		for(double arg : args) {
			if (smallest > arg) {
				smallest = arg;
			}
		}
		return smallest;
	}
	
	public static double largestDouble (double... args) {
		double largest = Double.MIN_VALUE;
		for(double arg : args) {
			if (largest < arg) {
				largest = arg;
			}
		}
		return largest;
	}
	
	public static double verifyGuardDouble (int type, double... args) {
		double[] testArgs;
		
		if (0 == type) {
			testArgs = null;
		} else {
			testArgs = Arrays.copyOfRange(args, 0, type);
		}
		
		if (null == testArgs) {
			return smallestDouble (args);
		} else {
			if (true == testDoubleGen(testArgs)) {
				return largestDouble (args);
			} else {
				return smallestDouble (args);
			}
		}
	}
	
	/* Test guardWithTest: Ljava/lang/Double */
	@Test(groups = { "level.extended" })
	public void test_guardWithTest_DoubleObject() throws WrongMethodTypeException, Throwable {
		MethodType mtTrueTarget = methodType(Double.class, Double.class, Double.class, Double.class, Double.class, Double.class);
		MethodHandle mhTrueTarget = MethodHandles.publicLookup().findStatic(GuardTest.class, "trueTargetDoubleObject", mtTrueTarget);
		
		MethodType mtFalseTarget = methodType(Double.class, Double.class, Double.class, Double.class, Double.class, Double.class);
		MethodHandle mhFalseTarget = MethodHandles.publicLookup().findStatic(GuardTest.class, "falseTargetDoubleObject", mtFalseTarget);
		
		Double d1 = new Double (constDouble1);
		Double d2 = new Double (constDouble2);
		Double d3 = new Double (constDouble3);
		Double d4 = new Double (constDouble4);
		Double d5 = new Double (constDouble5);
		Double dMax = new Double (Double.MAX_VALUE);
		Double dMin = new Double (Double.MIN_VALUE);
		
		MethodType mtTest = methodType(boolean.class);
		MethodHandle mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testDoubleObject", mtTest);
		MethodHandle mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardDouble (0, d1, d2, d3, d4, dMax), (Double) mhGuard.invokeExact(d1, d2, d3, d4, dMax));
		
		mtTest = methodType(boolean.class, Double.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testDoubleObject", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardDouble (1, d2, d3, d4, dMax, d5), (Double) mhGuard.invokeExact(d2, d3, d4, dMax, d5));
		
		mtTest = methodType(boolean.class, Double.class, Double.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testDoubleObject", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardDouble (2, d2, d3, dMin, d4, d1), (Double) mhGuard.invokeExact(d2, d3, dMin, d4, d1));
		
		mtTest = methodType(boolean.class, Double.class, Double.class, Double.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testDoubleObject", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardDouble (3, dMax, d3, dMin, d4, d1), (Double) mhGuard.invokeExact(dMax, d3, dMin, d4, d1));
		
		mtTest = methodType(boolean.class, Double.class, Double.class, Double.class, Double.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testDoubleObject", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardDouble (4, d1, d2, d3, d4, d5), (Double) mhGuard.invokeExact(d1, d2, d3, d4, d5));
		
		mtTest = methodType(boolean.class, Double.class, Double.class, Double.class, Double.class, Double.class);
		mhTest = MethodHandles.publicLookup().findStatic(GuardTest.class, "testDoubleObject", mtTest);
		mhGuard = MethodHandles.guardWithTest(mhTest, mhTrueTarget, mhFalseTarget);
		AssertJUnit.assertEquals(verifyGuardDouble (5, dMin, d3, d5, d4, d1), (Double) mhGuard.invokeExact(dMin, d3, d5, d4, d1));
	}
	
	public static boolean testDoubleObject () {
		return testDoubleObjectGen ();
	}
	
	public static boolean testDoubleObject (Double a) {
		return testDoubleObjectGen (a);
	}
	
	public static boolean testDoubleObject (Double a, Double b) {
		return testDoubleObjectGen (a, b);
	}
	
	public static boolean testDoubleObject (Double a, Double b, Double c) {
		return testDoubleObjectGen (a, b, c);
	}
	
	public static boolean testDoubleObject (Double a, Double b, Double c, Double d) {
		return testDoubleObjectGen (a, b, c, d);
	}
	
	public static boolean testDoubleObject (Double a, Double b, Double c, Double d, Double e) {
		return testDoubleObjectGen (a, b, c, d, e);
	}
	
	public static Double trueTargetDoubleObject (Double a, Double b, Double c, Double d, Double e) {
		return largestDoubleObject (a, b, c, d, e);
	}
	
	public static Double falseTargetDoubleObject (Double a, Double b, Double c, Double d, Double e) {
		return smallestDoubleObject (a, b, c, d, e);
	}
	
	public static boolean testDoubleObjectGen (Double... args) {
		if (0 == args.length) {
			return false;
		} else {
			Double last = args [args.length - 1];
			for (int i = 0; i < (args.length - 1); i++) {
				if (last.doubleValue() < args[i].doubleValue()) {
					return false;
				}
			}
			return true;
		}
	}

	public static Double smallestDoubleObject (Double... args) {
		Double smallest = new Double (Double.MAX_VALUE);
		for(Double arg : args) {
			if (smallest.doubleValue() > arg.doubleValue()) {
				smallest = arg;
			}
		}
		return smallest;
	}
	
	public static Double largestDoubleObject (Double... args) {
		Double largest = new Double (Double.MIN_VALUE);
		for(Double arg : args) {
			if (largest.doubleValue() < arg.doubleValue()) {
				largest = arg;
			}
		}
		return largest;
	}
	
	public static Double verifyGuardDoubleObject (int type, Double... args) {
		Double[] testArgs;
		
		if (0 == type) {
			testArgs = null;
		} else {
			testArgs = Arrays.copyOfRange(args, 0, type);
		}
		
		if (null == testArgs) {
			return smallestDoubleObject (args);
		} else {
			if (true == testDoubleObjectGen(testArgs)) {
				return largestDoubleObject (args);
			} else {
				return smallestDoubleObject (args);
			}
		}
	}
}
