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

/*
 * Tests MethodHandles.foldArguments() with the following FieldTypes for (Parameter/Return Descriptors): Z, B, S, C, I, F, J, D, Ljava/lang/Double;
 * 
 * For each FieldType, MethodHandles.foldArguments() is tested for boundary and intermediate conditions.
 * 
 * Example:
 * 
 * Let FieldType = D
 * 
 * Tests MethodHandles.foldArguments() with the following FoldHandle.next method descriptor: 
 * 		(DDDDD)D
 * 
 * Tests MethodHandles.foldArguments() with the following FoldHandle.combiner method descriptor variants:
 * 		()D
 * 		(D)D
 * 		(DD)D
 * 		(DDD)D
 * 		(DDDD)D
 * 		(DDDD)V
 */
public class FoldArgumentsTest {
    private final static byte constByte1 = (byte) -111;
    private final static byte constByte2 = (byte) -61;
    private final static byte constByte3 = (byte) -83;
    private final static byte constByte4 = (byte) 84;
    private final static short constShort1 = (short) -12096;
    private final static short constShort2 = (short) -21510;
    private final static short constShort3 = (short) -1622;
    private final static short constShort4 = (short) -19784;
    private final static char constCharacter1 = (char) 3763;
    private final static char constCharacter2 = (char) 31911;
    private final static char constCharacter3 = (char) 58509;
    private final static char constCharacter4 = (char) 55609;
    private final static int constInteger1 = (int) -491651072;
    private final static int constInteger2 = (int) -1993736192;
    private final static int constInteger3 = (int) 1213988863;
    private final static int constInteger4 = (int) 326893567;
    private final static long constLong1 = (long) 1.26663739519795e+018;
    private final static long constLong2 = (long) 4.21368040135852e+018;
    private final static long constLong3 = (long) 8.94696360972491e+018;
    private final static long constLong4 = (long) 1.20527585027503e+018;
    private final static float constFloat1 = (float) 2.29572201887512e+038F;
    private final static float constFloat2 = (float) 6.12483306976318e+037F;
    private final static float constFloat3 = (float) 1.53640056404114e+038F;
    private final static float constFloat4 = (float) 6.22037132720947e+036F;
    private final static double constDouble1 = (double) 1.10819706189633e+307;
    private final static double constDouble2 = (double) 1.78024726032355e+307;
    private final static double constDouble3 = (double) 6.78194657384276e+307;
    private final static double constDouble4 = (double) 7.33110759312901e+307;
    private final static boolean constBoolMax = true;
    private final static boolean constBoolMin = false;
     
 	/* Test foldArguments: Combiner's data-type -> byte */
	@Test(groups = { "level.extended" })
	public void test_foldArguments_Byte() throws WrongMethodTypeException, Throwable {
		MethodType nextMT = methodType(byte.class, byte.class, byte.class, byte.class, byte.class, byte.class);
		MethodHandle mh1 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "nextByte", nextMT);
		
		MethodType combinerMT = methodType(byte.class);
		MethodHandle mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerByte", combinerMT);
		
		/* Verify Byte.Max_VALUE Boundary Condition */
		MethodHandle foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Byte.MAX_VALUE, (byte) foldMH.invokeExact(constByte1, constByte2, constByte3, constByte4));
		
		combinerMT = methodType(byte.class, byte.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerByte", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Byte.MAX_VALUE, (byte) foldMH.invokeExact(Byte.MAX_VALUE, constByte2, constByte3, constByte4));
		
		combinerMT = methodType(byte.class, byte.class, byte.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerByte", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Byte.MAX_VALUE, (byte) foldMH.invokeExact(constByte1, Byte.MAX_VALUE, constByte3, constByte4));
		
		combinerMT = methodType(byte.class, byte.class, byte.class, byte.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerByte", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Byte.MAX_VALUE, (byte) foldMH.invokeExact(constByte1, constByte2, Byte.MAX_VALUE, constByte4));
		
		combinerMT = methodType(byte.class, byte.class, byte.class, byte.class, byte.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerByte", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Byte.MAX_VALUE, (byte) foldMH.invokeExact(constByte1, constByte2, constByte3, Byte.MAX_VALUE));
		
		/* Verify Byte.MIN_VALUE boundary condition */
		combinerMT = methodType(byte.class, byte.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerByte", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Byte.MIN_VALUE, (byte) foldMH.invokeExact(Byte.MIN_VALUE, constByte2, constByte3, constByte4));
		
		combinerMT = methodType(byte.class, byte.class, byte.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerByte", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constByte1, (byte) foldMH.invokeExact(constByte1, Byte.MIN_VALUE, constByte3, constByte4));
		
		combinerMT = methodType(byte.class, byte.class, byte.class, byte.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerByte", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constByte2, (byte) foldMH.invokeExact(constByte1, constByte2, Byte.MIN_VALUE, constByte4));
		
		combinerMT = methodType(byte.class, byte.class, byte.class, byte.class, byte.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerByte", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constByte2, (byte) foldMH.invokeExact(constByte1, constByte2, constByte3, Byte.MIN_VALUE));
		
		/* Verify intermediate condition */
		combinerMT = methodType(byte.class, byte.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerByte", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constByte1, (byte) foldMH.invokeExact(constByte1, constByte2, constByte3, constByte4));
		
		combinerMT = methodType(byte.class, byte.class, byte.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerByte", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constByte2, (byte) foldMH.invokeExact(constByte1, constByte2, constByte3, constByte4));
		
		combinerMT = methodType(byte.class, byte.class, byte.class, byte.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerByte", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constByte2, (byte) foldMH.invokeExact(constByte1, constByte2, constByte3, constByte4));
		
		combinerMT = methodType(byte.class, byte.class, byte.class, byte.class, byte.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerByte", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constByte4, (byte) foldMH.invokeExact(constByte1, constByte2, constByte3, constByte4));
		
		/* void return case */
		combinerMT = methodType(void.class, byte.class, byte.class, byte.class, byte.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerByteVoid", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constByte1, (byte) foldMH.invokeExact(constByte1, constByte2, constByte3, constByte4, Byte.MIN_VALUE));
	}
	
	public static byte combinerByte () {
		return Byte.MAX_VALUE;
	}
	
	public static byte combinerByte (byte a) {
		return largestByte (a);
	}
	
	public static byte combinerByte (byte a, byte b) {
		return largestByte (a, b);
	}
	
	public static byte combinerByte (byte a, byte b, byte c) {
		return largestByte (a, b, c);
	}
	
	public static byte combinerByte (byte a, byte b, byte c, byte d) {
		return largestByte (a, b, c, d);
	}
	
	public static void combinerByteVoid (byte a, byte b, byte c, byte d) {
		byte largest = largestByte (a, b, c, d);
	}
	
	public static byte nextByte (byte a, byte b, byte c, byte d, byte e) {
		return a;
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
	
 	/* Test foldArguments: Combiner's data-type -> short */
	@Test(groups = { "level.extended" })
	public void test_foldArguments_Short() throws WrongMethodTypeException, Throwable {
		MethodType nextMT = methodType(short.class, short.class, short.class, short.class, short.class, short.class);
		MethodHandle mh1 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "nextShort", nextMT);
		
		MethodType combinerMT = methodType(short.class);
		MethodHandle mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerShort", combinerMT);
		
		/* Verify Short.Max_VALUE Boundary Condition */
		MethodHandle foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Short.MAX_VALUE, (short) foldMH.invokeExact(constShort1, constShort2, constShort3, constShort4));
		
		combinerMT = methodType(short.class, short.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerShort", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Short.MAX_VALUE, (short) foldMH.invokeExact(Short.MAX_VALUE, constShort2, constShort3, constShort4));
		
		combinerMT = methodType(short.class, short.class, short.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerShort", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Short.MAX_VALUE, (short) foldMH.invokeExact(constShort1, Short.MAX_VALUE, constShort3, constShort4));
		
		combinerMT = methodType(short.class, short.class, short.class, short.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerShort", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Short.MAX_VALUE, (short) foldMH.invokeExact(constShort1, constShort2, Short.MAX_VALUE, constShort4));
		
		combinerMT = methodType(short.class, short.class, short.class, short.class, short.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerShort", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Short.MAX_VALUE, (short) foldMH.invokeExact(constShort1, constShort2, constShort3, Short.MAX_VALUE));
		
		/* Verify Short.MIN_VALUE boundary condition */
		combinerMT = methodType(short.class, short.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerShort", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Short.MIN_VALUE, (short) foldMH.invokeExact(Short.MIN_VALUE, constShort2, constShort3, constShort4));
		
		combinerMT = methodType(short.class, short.class, short.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerShort", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constShort1, (short) foldMH.invokeExact(constShort1, Short.MIN_VALUE, constShort3, constShort4));
		
		combinerMT = methodType(short.class, short.class, short.class, short.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerShort", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constShort1, (short) foldMH.invokeExact(constShort1, constShort2, Short.MIN_VALUE, constShort4));
		
		combinerMT = methodType(short.class, short.class, short.class, short.class, short.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerShort", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constShort3, (short) foldMH.invokeExact(constShort1, constShort2, constShort3, Short.MIN_VALUE));
		
		/* Verify intermediate condition */
		combinerMT = methodType(short.class, short.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerShort", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constShort1, (short) foldMH.invokeExact(constShort1, constShort2, constShort3, constShort4));
		
		combinerMT = methodType(short.class, short.class, short.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerShort", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constShort1, (short) foldMH.invokeExact(constShort1, constShort2, constShort3, constShort4));
		
		combinerMT = methodType(short.class, short.class, short.class, short.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerShort", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constShort3, (short) foldMH.invokeExact(constShort1, constShort2, constShort3, constShort4));
		
		combinerMT = methodType(short.class, short.class, short.class, short.class, short.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerShort", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constShort3, (short) foldMH.invokeExact(constShort1, constShort2, constShort3, constShort4));
		
		/* void return case */
		combinerMT = methodType(void.class, short.class, short.class, short.class, short.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerShortVoid", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constShort1, (short) foldMH.invokeExact(constShort1, constShort2, constShort3, constShort4, Short.MIN_VALUE));
	}
	
	public static short combinerShort () {
		return Short.MAX_VALUE;
	}
	
	public static short combinerShort (short a) {
		return largestShort (a);
	}
	
	public static short combinerShort (short a, short b) {
		return largestShort (a, b);
	}
	
	public static short combinerShort (short a, short b, short c) {
		return largestShort (a, b, c);
	}
	
	public static short combinerShort (short a, short b, short c, short d) {
		return largestShort (a, b, c, d);
	}
	
	public static void combinerShortVoid (short a, short b, short c, short d) {
		short largest = largestShort (a, b, c, d);
	}
	
	public static short nextShort (short a, short b, short c, short d, short e) {
		return a;
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
	
 	/* Test foldArguments: Combiner's data-type -> char */
	@Test(groups = { "level.extended" })
	public void test_foldArguments_Character() throws WrongMethodTypeException, Throwable {
		MethodType nextMT = methodType(char.class, char.class, char.class, char.class, char.class, char.class);
		MethodHandle mh1 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "nextCharacter", nextMT);
		
		MethodType combinerMT = methodType(char.class);
		MethodHandle mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerCharacter", combinerMT);
		
		/* Verify Character.Max_VALUE Boundary Condition */
		MethodHandle foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Character.MAX_VALUE, (char) foldMH.invokeExact(constCharacter1, constCharacter2, constCharacter3, constCharacter4));
		
		combinerMT = methodType(char.class, char.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerCharacter", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Character.MAX_VALUE, (char) foldMH.invokeExact(Character.MAX_VALUE, constCharacter2, constCharacter3, constCharacter4));
		
		combinerMT = methodType(char.class, char.class, char.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerCharacter", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Character.MAX_VALUE, (char) foldMH.invokeExact(constCharacter1, Character.MAX_VALUE, constCharacter3, constCharacter4));
		
		combinerMT = methodType(char.class, char.class, char.class, char.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerCharacter", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Character.MAX_VALUE, (char) foldMH.invokeExact(constCharacter1, constCharacter2, Character.MAX_VALUE, constCharacter4));
		
		combinerMT = methodType(char.class, char.class, char.class, char.class, char.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerCharacter", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Character.MAX_VALUE, (char) foldMH.invokeExact(constCharacter1, constCharacter2, constCharacter3, Character.MAX_VALUE));
		
		/* Verify Character.MIN_VALUE boundary condition */
		combinerMT = methodType(char.class, char.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerCharacter", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Character.MIN_VALUE, (char) foldMH.invokeExact(Character.MIN_VALUE, constCharacter2, constCharacter3, constCharacter4));
		
		combinerMT = methodType(char.class, char.class, char.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerCharacter", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constCharacter1, (char) foldMH.invokeExact(constCharacter1, Character.MIN_VALUE, constCharacter3, constCharacter4));
		
		combinerMT = methodType(char.class, char.class, char.class, char.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerCharacter", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constCharacter2, (char) foldMH.invokeExact(constCharacter1, constCharacter2, Character.MIN_VALUE, constCharacter4));
		
		combinerMT = methodType(char.class, char.class, char.class, char.class, char.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerCharacter", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constCharacter3, (char) foldMH.invokeExact(constCharacter1, constCharacter2, constCharacter3, Character.MIN_VALUE));
		
		/* Verify intermediate condition */
		combinerMT = methodType(char.class, char.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerCharacter", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constCharacter1, (char) foldMH.invokeExact(constCharacter1, constCharacter2, constCharacter3, constCharacter4));
		
		combinerMT = methodType(char.class, char.class, char.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerCharacter", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constCharacter2, (char) foldMH.invokeExact(constCharacter1, constCharacter2, constCharacter3, constCharacter4));
		
		combinerMT = methodType(char.class, char.class, char.class, char.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerCharacter", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constCharacter3, (char) foldMH.invokeExact(constCharacter1, constCharacter2, constCharacter3, constCharacter4));
		
		combinerMT = methodType(char.class, char.class, char.class, char.class, char.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerCharacter", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constCharacter3, (char) foldMH.invokeExact(constCharacter1, constCharacter2, constCharacter3, constCharacter4));
		
		/* void return case */
		combinerMT = methodType(void.class, char.class, char.class, char.class, char.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerCharacterVoid", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constCharacter1, (char) foldMH.invokeExact(constCharacter1, constCharacter2, constCharacter3, constCharacter4, Character.MIN_VALUE));
	}
	
	public static char combinerCharacter () {
		return Character.MAX_VALUE;
	}
	
	public static char combinerCharacter (char a) {
		return largestCharacter (a);
	}
	
	public static char combinerCharacter (char a, char b) {
		return largestCharacter (a, b);
	}
	
	public static char combinerCharacter (char a, char b, char c) {
		return largestCharacter (a, b, c);
	}
	
	public static char combinerCharacter (char a, char b, char c, char d) {
		return largestCharacter (a, b, c, d);
	}
	
	public static void combinerCharacterVoid (char a, char b, char c, char d) {
		char largest = largestCharacter (a, b, c, d);
	}
	
	public static char nextCharacter (char a, char b, char c, char d, char e) {
		return a;
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
	
 	/* Test foldArguments: Combiner's data-type -> int */
	@Test(groups = { "level.extended" })
	public void test_foldArguments_Integer() throws WrongMethodTypeException, Throwable {
		MethodType nextMT = methodType(int.class, int.class, int.class, int.class, int.class, int.class);
		MethodHandle mh1 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "nextInteger", nextMT);
		
		MethodType combinerMT = methodType(int.class);
		MethodHandle mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerInteger", combinerMT);
		
		/* Verify Integer.Max_VALUE Boundary Condition */
		MethodHandle foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Integer.MAX_VALUE, (int) foldMH.invokeExact(constInteger1, constInteger2, constInteger3, constInteger4));
		
		combinerMT = methodType(int.class, int.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerInteger", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Integer.MAX_VALUE, (int) foldMH.invokeExact(Integer.MAX_VALUE, constInteger2, constInteger3, constInteger4));
		
		combinerMT = methodType(int.class, int.class, int.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerInteger", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Integer.MAX_VALUE, (int) foldMH.invokeExact(constInteger1, Integer.MAX_VALUE, constInteger3, constInteger4));
		
		combinerMT = methodType(int.class, int.class, int.class, int.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerInteger", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Integer.MAX_VALUE, (int) foldMH.invokeExact(constInteger1, constInteger2, Integer.MAX_VALUE, constInteger4));
		
		combinerMT = methodType(int.class, int.class, int.class, int.class, int.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerInteger", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Integer.MAX_VALUE, (int) foldMH.invokeExact(constInteger1, constInteger2, constInteger3, Integer.MAX_VALUE));
		
		/* Verify Integer.MIN_VALUE boundary condition */
		combinerMT = methodType(int.class, int.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerInteger", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Integer.MIN_VALUE, (int) foldMH.invokeExact(Integer.MIN_VALUE, constInteger2, constInteger3, constInteger4));
		
		combinerMT = methodType(int.class, int.class, int.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerInteger", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constInteger1, (int) foldMH.invokeExact(constInteger1, Integer.MIN_VALUE, constInteger3, constInteger4));
		
		combinerMT = methodType(int.class, int.class, int.class, int.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerInteger", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constInteger1, (int) foldMH.invokeExact(constInteger1, constInteger2, Integer.MIN_VALUE, constInteger4));
		
		combinerMT = methodType(int.class, int.class, int.class, int.class, int.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerInteger", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constInteger3, (int) foldMH.invokeExact(constInteger1, constInteger2, constInteger3, Integer.MIN_VALUE));
		
		/* Verify intermediate condition */
		combinerMT = methodType(int.class, int.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerInteger", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constInteger1, (int) foldMH.invokeExact(constInteger1, constInteger2, constInteger3, constInteger4));
		
		combinerMT = methodType(int.class, int.class, int.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerInteger", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constInteger1, (int) foldMH.invokeExact(constInteger1, constInteger2, constInteger3, constInteger4));
		
		combinerMT = methodType(int.class, int.class, int.class, int.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerInteger", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constInteger3, (int) foldMH.invokeExact(constInteger1, constInteger2, constInteger3, constInteger4));
		
		combinerMT = methodType(int.class, int.class, int.class, int.class, int.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerInteger", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constInteger3, (int) foldMH.invokeExact(constInteger1, constInteger2, constInteger3, constInteger4));
		
		/* void return case */
		combinerMT = methodType(void.class, int.class, int.class, int.class, int.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerIntegerVoid", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constInteger1, (int) foldMH.invokeExact(constInteger1, constInteger2, constInteger3, constInteger4, Integer.MIN_VALUE));
	}
	
	public static int combinerInteger () {
		return Integer.MAX_VALUE;
	}
	
	public static int combinerInteger (int a) {
		return largestInteger (a);
	}
	
	public static int combinerInteger (int a, int b) {
		return largestInteger (a, b);
	}
	
	public static int combinerInteger (int a, int b, int c) {
		return largestInteger (a, b, c);
	}
	
	public static int combinerInteger (int a, int b, int c, int d) {
		return largestInteger (a, b, c, d);
	}
	
	public static void combinerIntegerVoid (int a, int b, int c, int d) {
		int largest = largestInteger (a, b, c, d);
	}
	
	public static int nextInteger (int a, int b, int c, int d, int e) {
		return a;
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
	
 	/* Test foldArguments: Combiner's data-type -> long */
	@Test(groups = { "level.extended" })
	public void test_foldArguments_Long() throws WrongMethodTypeException, Throwable {
		MethodType nextMT = methodType(long.class, long.class, long.class, long.class, long.class, long.class);
		MethodHandle mh1 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "nextLong", nextMT);
		
		MethodType combinerMT = methodType(long.class);
		MethodHandle mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerLong", combinerMT);
		
		/* Verify Long.Max_VALUE Boundary Condition */
		MethodHandle foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Long.MAX_VALUE, (long) foldMH.invokeExact(constLong1, constLong2, constLong3, constLong4));
		
		combinerMT = methodType(long.class, long.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerLong", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Long.MAX_VALUE, (long) foldMH.invokeExact(Long.MAX_VALUE, constLong2, constLong3, constLong4));
		
		combinerMT = methodType(long.class, long.class, long.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerLong", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Long.MAX_VALUE, (long) foldMH.invokeExact(constLong1, Long.MAX_VALUE, constLong3, constLong4));
		
		combinerMT = methodType(long.class, long.class, long.class, long.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerLong", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Long.MAX_VALUE, (long) foldMH.invokeExact(constLong1, constLong2, Long.MAX_VALUE, constLong4));
		
		combinerMT = methodType(long.class, long.class, long.class, long.class, long.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerLong", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Long.MAX_VALUE, (long) foldMH.invokeExact(constLong1, constLong2, constLong3, Long.MAX_VALUE));
		
		/* Verify Long.MIN_VALUE boundary condition */
		combinerMT = methodType(long.class, long.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerLong", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Long.MIN_VALUE, (long) foldMH.invokeExact(Long.MIN_VALUE, constLong2, constLong3, constLong4));
		
		combinerMT = methodType(long.class, long.class, long.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerLong", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constLong1, (long) foldMH.invokeExact(constLong1, Long.MIN_VALUE, constLong3, constLong4));
		
		combinerMT = methodType(long.class, long.class, long.class, long.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerLong", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constLong2, (long) foldMH.invokeExact(constLong1, constLong2, Long.MIN_VALUE, constLong4));
		
		combinerMT = methodType(long.class, long.class, long.class, long.class, long.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerLong", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constLong3, (long) foldMH.invokeExact(constLong1, constLong2, constLong3, Long.MIN_VALUE));
		
		/* Verify intermediate condition */
		combinerMT = methodType(long.class, long.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerLong", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constLong1, (long) foldMH.invokeExact(constLong1, constLong2, constLong3, constLong4));
		
		combinerMT = methodType(long.class, long.class, long.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerLong", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constLong2, (long) foldMH.invokeExact(constLong1, constLong2, constLong3, constLong4));
		
		combinerMT = methodType(long.class, long.class, long.class, long.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerLong", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constLong3, (long) foldMH.invokeExact(constLong1, constLong2, constLong3, constLong4));
		
		combinerMT = methodType(long.class, long.class, long.class, long.class, long.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerLong", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constLong3, (long) foldMH.invokeExact(constLong1, constLong2, constLong3, constLong4));
		
		/* void return case */
		combinerMT = methodType(void.class, long.class, long.class, long.class, long.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerLongVoid", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constLong1, (long) foldMH.invokeExact(constLong1, constLong2, constLong3, constLong4, Long.MIN_VALUE));
	}
	
	public static long combinerLong () {
		return Long.MAX_VALUE;
	}
	
	public static long combinerLong (long a) {
		return largestLong (a);
	}
	
	public static long combinerLong (long a, long b) {
		return largestLong (a, b);
	}
	
	public static long combinerLong (long a, long b, long c) {
		return largestLong (a, b, c);
	}
	
	public static long combinerLong (long a, long b, long c, long d) {
		return largestLong (a, b, c, d);
	}
	
	public static void combinerLongVoid (long a, long b, long c, long d) {
		long largest = largestLong (a, b, c, d);
	}
	
	public static long nextLong (long a, long b, long c, long d, long e) {
		return a;
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
	
 	/* Test foldArguments: Combiner's data-type -> float */
	@Test(groups = { "level.extended" })
	public void test_foldArguments_Float() throws WrongMethodTypeException, Throwable {
		MethodType nextMT = methodType(float.class, float.class, float.class, float.class, float.class, float.class);
		MethodHandle mh1 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "nextFloat", nextMT);
		
		MethodType combinerMT = methodType(float.class);
		MethodHandle mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerFloat", combinerMT);
		
		/* Verify Float.Max_VALUE Boundary Condition */
		MethodHandle foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Float.MAX_VALUE, (float) foldMH.invokeExact(constFloat1, constFloat2, constFloat3, constFloat4));
		
		combinerMT = methodType(float.class, float.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerFloat", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Float.MAX_VALUE, (float) foldMH.invokeExact(Float.MAX_VALUE, constFloat2, constFloat3, constFloat4));
		
		combinerMT = methodType(float.class, float.class, float.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerFloat", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Float.MAX_VALUE, (float) foldMH.invokeExact(constFloat1, Float.MAX_VALUE, constFloat3, constFloat4));
		
		combinerMT = methodType(float.class, float.class, float.class, float.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerFloat", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Float.MAX_VALUE, (float) foldMH.invokeExact(constFloat1, constFloat2, Float.MAX_VALUE, constFloat4));
		
		combinerMT = methodType(float.class, float.class, float.class, float.class, float.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerFloat", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Float.MAX_VALUE, (float) foldMH.invokeExact(constFloat1, constFloat2, constFloat3, Float.MAX_VALUE));
		
		/* Verify Float.MIN_VALUE boundary condition */
		combinerMT = methodType(float.class, float.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerFloat", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Float.MIN_VALUE, (float) foldMH.invokeExact(Float.MIN_VALUE, constFloat2, constFloat3, constFloat4));
		
		combinerMT = methodType(float.class, float.class, float.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerFloat", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constFloat1, (float) foldMH.invokeExact(constFloat1, Float.MIN_VALUE, constFloat3, constFloat4));
		
		combinerMT = methodType(float.class, float.class, float.class, float.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerFloat", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constFloat1, (float) foldMH.invokeExact(constFloat1, constFloat2, Float.MIN_VALUE, constFloat4));
		
		combinerMT = methodType(float.class, float.class, float.class, float.class, float.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerFloat", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constFloat1, (float) foldMH.invokeExact(constFloat1, constFloat2, constFloat3, Float.MIN_VALUE));
		
		/* Verify intermediate condition */
		combinerMT = methodType(float.class, float.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerFloat", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constFloat1, (float) foldMH.invokeExact(constFloat1, constFloat2, constFloat3, constFloat4));
		
		combinerMT = methodType(float.class, float.class, float.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerFloat", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constFloat1, (float) foldMH.invokeExact(constFloat1, constFloat2, constFloat3, constFloat4));
		
		combinerMT = methodType(float.class, float.class, float.class, float.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerFloat", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constFloat1, (float) foldMH.invokeExact(constFloat1, constFloat2, constFloat3, constFloat4));
		
		combinerMT = methodType(float.class, float.class, float.class, float.class, float.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerFloat", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constFloat1, (float) foldMH.invokeExact(constFloat1, constFloat2, constFloat3, constFloat4));
		
		/* void return case */
		combinerMT = methodType(void.class, float.class, float.class, float.class, float.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerFloatVoid", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constFloat1, (float) foldMH.invokeExact(constFloat1, constFloat2, constFloat3, constFloat4, Float.MIN_VALUE));
	}
	
	public static float combinerFloat () {
		return Float.MAX_VALUE;
	}
	
	public static float combinerFloat (float a) {
		return largestFloat (a);
	}
	
	public static float combinerFloat (float a, float b) {
		return largestFloat (a, b);
	}
	
	public static float combinerFloat (float a, float b, float c) {
		return largestFloat (a, b, c);
	}
	
	public static float combinerFloat (float a, float b, float c, float d) {
		return largestFloat (a, b, c, d);
	}
	
	public static void combinerFloatVoid (float a, float b, float c, float d) {
		float largest = largestFloat (a, b, c, d);
	}
	
	public static float nextFloat (float a, float b, float c, float d, float e) {
		return a;
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
	
 	/* Test foldArguments: Combiner's data-type -> double */
	@Test(groups = { "level.extended" })
	public void test_foldArguments_Double() throws WrongMethodTypeException, Throwable {
		MethodType nextMT = methodType(double.class, double.class, double.class, double.class, double.class, double.class);
		MethodHandle mh1 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "nextDouble", nextMT);
		
		MethodType combinerMT = methodType(double.class);
		MethodHandle mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDouble", combinerMT);
		
		/* Verify Double.Max_VALUE Boundary Condition */
		MethodHandle foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Double.MAX_VALUE, (double) foldMH.invokeExact(constDouble1, constDouble2, constDouble3, constDouble4));
		
		combinerMT = methodType(double.class, double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDouble", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Double.MAX_VALUE, (double) foldMH.invokeExact(Double.MAX_VALUE, constDouble2, constDouble3, constDouble4));
		
		combinerMT = methodType(double.class, double.class, double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDouble", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Double.MAX_VALUE, (double) foldMH.invokeExact(constDouble1, Double.MAX_VALUE, constDouble3, constDouble4));
		
		combinerMT = methodType(double.class, double.class, double.class, double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDouble", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Double.MAX_VALUE, (double) foldMH.invokeExact(constDouble1, constDouble2, Double.MAX_VALUE, constDouble4));
		
		combinerMT = methodType(double.class, double.class, double.class, double.class, double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDouble", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Double.MAX_VALUE, (double) foldMH.invokeExact(constDouble1, constDouble2, constDouble3, Double.MAX_VALUE));
		
		/* Verify Double.MIN_VALUE boundary condition */
		combinerMT = methodType(double.class, double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDouble", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(Double.MIN_VALUE, (double) foldMH.invokeExact(Double.MIN_VALUE, constDouble2, constDouble3, constDouble4));
		
		combinerMT = methodType(double.class, double.class, double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDouble", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constDouble1, (double) foldMH.invokeExact(constDouble1, Double.MIN_VALUE, constDouble3, constDouble4));
		
		combinerMT = methodType(double.class, double.class, double.class, double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDouble", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constDouble2, (double) foldMH.invokeExact(constDouble1, constDouble2, Double.MIN_VALUE, constDouble4));
		
		combinerMT = methodType(double.class, double.class, double.class, double.class, double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDouble", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constDouble3, (double) foldMH.invokeExact(constDouble1, constDouble2, constDouble3, Double.MIN_VALUE));
		
		/* Verify intermediate condition */
		combinerMT = methodType(double.class, double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDouble", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constDouble1, (double) foldMH.invokeExact(constDouble1, constDouble2, constDouble3, constDouble4));
		
		combinerMT = methodType(double.class, double.class, double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDouble", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constDouble2, (double) foldMH.invokeExact(constDouble1, constDouble2, constDouble3, constDouble4));
		
		combinerMT = methodType(double.class, double.class, double.class, double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDouble", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constDouble3, (double) foldMH.invokeExact(constDouble1, constDouble2, constDouble3, constDouble4));
		
		combinerMT = methodType(double.class, double.class, double.class, double.class, double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDouble", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constDouble4, (double) foldMH.invokeExact(constDouble1, constDouble2, constDouble3, constDouble4));
		
		/* void return case */
		combinerMT = methodType(void.class, double.class, double.class, double.class, double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDoubleVoid", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constDouble1, (double) foldMH.invokeExact(constDouble1, constDouble2, constDouble3, constDouble4, Double.MIN_VALUE));
	}
	
	public static double combinerDouble () {
		return Double.MAX_VALUE;
	}
	
	public static double combinerDouble (double a) {
		return largestDouble (a);
	}
	
	public static double combinerDouble (double a, double b) {
		return largestDouble (a, b);
	}
	
	public static double combinerDouble (double a, double b, double c) {
		return largestDouble (a, b, c);
	}
	
	public static double combinerDouble (double a, double b, double c, double d) {
		return largestDouble (a, b, c, d);
	}
	
	public static void combinerDoubleVoid (double a, double b, double c, double d) {
		double largest = largestDouble (a, b, c, d);
	}
	
	public static double nextDouble (double a, double b, double c, double d, double e) {
		return a;
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
	
 	/* Test foldArguments: Combiner's data-type -> Double (Object) */
	@Test(groups = { "level.extended" })
	public void test_foldArguments_DoubleObject() throws WrongMethodTypeException, Throwable {
		MethodType nextMT = methodType(Double.class, Double.class, Double.class, Double.class, Double.class, Double.class);
		MethodHandle mh1 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "nextDoubleObject", nextMT);
		
		MethodType combinerMT = methodType(Double.class);
		MethodHandle mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDoubleObject", combinerMT);
		
		Double d1 = new Double (constDouble1);
		Double d2 = new Double (constDouble2);
		Double d3 = new Double (constDouble3);
		Double d4 = new Double (constDouble4);
		Double dMax = new Double (Double.MAX_VALUE);
		Double dMin = new Double (Double.MIN_VALUE);
		
		/* Verify Double.Max_VALUE Boundary Condition */
		MethodHandle foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(dMax, (Double) foldMH.invokeExact(d1, d2, d3, d4));
		
		combinerMT = methodType(Double.class, Double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDoubleObject", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(dMax, (Double) foldMH.invokeExact(dMax, d2, d3, d4));
		
		combinerMT = methodType(Double.class, Double.class, Double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDoubleObject", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(dMax, (Double) foldMH.invokeExact(d1, dMax, d3, d4));
		
		combinerMT = methodType(Double.class, Double.class, Double.class, Double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDoubleObject", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(dMax, (Double) foldMH.invokeExact(d1, d2, dMax, d4));
		
		combinerMT = methodType(Double.class, Double.class, Double.class, Double.class, Double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDoubleObject", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(dMax, (Double) foldMH.invokeExact(d1, d2, d3, dMax));
		
		/* Verify dMin boundary condition */
		combinerMT = methodType(Double.class, Double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDoubleObject", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(dMin, (Double) foldMH.invokeExact(dMin, d2, d3, d4));
		
		combinerMT = methodType(Double.class, Double.class, Double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDoubleObject", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(d1, (Double) foldMH.invokeExact(d1, dMin, d3, d4));
		
		combinerMT = methodType(Double.class, Double.class, Double.class, Double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDoubleObject", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(d2, (Double) foldMH.invokeExact(d1, d2, dMin, d4));
		
		combinerMT = methodType(Double.class, Double.class, Double.class, Double.class, Double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDoubleObject", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(d3, (Double) foldMH.invokeExact(d1, d2, d3, dMin));
		
		/* Verify intermediate condition */
		combinerMT = methodType(Double.class, Double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDoubleObject", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(d1, (Double) foldMH.invokeExact(d1, d2, d3, d4));
		
		combinerMT = methodType(Double.class, Double.class, Double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDoubleObject", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(d2, (Double) foldMH.invokeExact(d1, d2, d3, d4));
		
		combinerMT = methodType(Double.class, Double.class, Double.class, Double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDoubleObject", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(d3, (Double) foldMH.invokeExact(d1, d2, d3, d4));
		
		combinerMT = methodType(Double.class, Double.class, Double.class, Double.class, Double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDoubleObject", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(d4, (Double) foldMH.invokeExact(d1, d2, d3, d4));
		
		/* void return case */
		combinerMT = methodType(void.class, Double.class, Double.class, Double.class, Double.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerDoubleObjectVoid", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(d1, (Double) foldMH.invokeExact(d1, d2, d3, d4, dMin));
	}
	
	public static Double combinerDoubleObject () {
		Double ret = new Double (Double.MAX_VALUE);
		return ret;
	}
	
	public static Double combinerDoubleObject (Double a) {
		return largestDoubleObject (a);
	}
	
	public static Double combinerDoubleObject (Double a, Double b) {
		return largestDoubleObject (a, b);
	}
	
	public static Double combinerDoubleObject (Double a, Double b, Double c) {
		return largestDoubleObject (a, b, c);
	}
	
	public static Double combinerDoubleObject (Double a, Double b, Double c, Double d) {
		return largestDoubleObject (a, b, c, d);
	}
	
	public static void combinerDoubleObjectVoid (Double a, Double b, Double c, Double d) {
		Double largest = largestDoubleObject (a, b, c, d);
	}
	
	public static Double nextDoubleObject (Double a, Double b, Double c, Double d, Double e) {
		return a;
	}
	
	public static Double largestDoubleObject (Double... args) {
		Double largest = Double.MIN_VALUE;
		for(Double arg : args) {
			if (largest.doubleValue() < arg.doubleValue()) {
				largest = arg;
			}
		}
		return largest;
	}
	
	/* Test foldArguments: Combiner's data-type -> boolean */
	@Test(groups = { "level.extended" })
	public void test_foldArguments_Boolean() throws WrongMethodTypeException, Throwable {
		MethodType nextMT = methodType(boolean.class, boolean.class, boolean.class, boolean.class);
		MethodHandle mh1 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "nextBoolean", nextMT);
		
		MethodType combinerMT = methodType(boolean.class);
		MethodHandle mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerBoolean", combinerMT);
		
		MethodHandle foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constBoolMax, (boolean) foldMH.invokeExact(constBoolMax, constBoolMin));
		
		combinerMT = methodType(boolean.class, boolean.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerBoolean", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constBoolMax, (boolean) foldMH.invokeExact(constBoolMax, constBoolMin));
		
		combinerMT = methodType(boolean.class, boolean.class, boolean.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerBoolean", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constBoolMin, (boolean) foldMH.invokeExact(constBoolMax, constBoolMin));
		
		/* void return case */
		combinerMT = methodType(void.class, boolean.class, boolean.class);
		mh2 = MethodHandles.publicLookup().findStatic(FoldArgumentsTest.class, "combinerBooleanVoid", combinerMT);
		foldMH = MethodHandles.foldArguments(mh1, mh2);
		AssertJUnit.assertEquals(constBoolMin, (boolean) foldMH.invokeExact(constBoolMin, constBoolMax, constBoolMin));
	}
	
	public static boolean combinerBoolean () {
		return constBoolMax;
	}
	
	public static boolean combinerBoolean (boolean a) {
		return lastBoolean (a);
	}
	
	public static boolean combinerBoolean (boolean a, boolean b) {
		return lastBoolean (a, b);
	}
	
	public static void combinerBooleanVoid (boolean a, boolean b) {
		boolean last = lastBoolean (a, b);
	}
	
	public static boolean nextBoolean (boolean a, boolean b, boolean c) {
		return a;
	}
	
	public static boolean lastBoolean (boolean... args) {
		boolean last = constBoolMin;
		for(boolean arg: args) {
			last = arg;
		}
		return last;
	}
}
