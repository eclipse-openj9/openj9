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
import static java.lang.invoke.MethodHandles.filterArguments;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.WrongMethodTypeException;

/*
 * Tests MethodHandles.filterArguments(...) with the following FieldTypes for (Parameter/Return Descriptors): Z, B, S, C, I, F, J, D, Ljava/lang/Double;
 * 
 * For each FieldType, MethodHandles.filterArguments(...) is tested for boundary and intermediate conditions.
 * 
 * For the following FieldTypes -> B, S, C, I, F, J, D, Ljava/lang/Double;
 * target -> returns average of input parameters
 * filter0 -> divide by 2
 * filter1 -> subtract 1
 * filter2 -> add 1
 * filter3 -> If FieldType is double or long, then convert to 1 stackSlot primitive data-type. Otherwise, convert to 2 stackSlot primitive data-type.
 * filter4 -> If FieldType is an object, then convert to primitive data-type. Otherwise, covert to an object. 
 * 
 * For the following FieldType -> Z
 * target -> returns last boolean input parameter
 * filter0 -> returns inverse of the input parameter
 * 
 * Null Elements in filters[] are tested for the following cases: leading, intermediate and trailing positions.
 * 
 * The above target and filters are used below to test the native implementation of MethodHandles.filterArguments(...).
 */
public class FilterArgumentsTest {
    private final static byte constByte1 = (byte) -51;
    private final static byte constByte2 = (byte) -111;
    private final static byte constByte3 = (byte) 69;
    private final static byte constByte4 = (byte) 93;
    private final static byte constByte5 = (byte) 10;
    private final static byte constByte6 = (byte) -84;
    private final static short constShort1 = (short) 8251;
    private final static short constShort2 = (short) -27250;
    private final static short constShort3 = (short) -26136;
    private final static short constShort4 = (short) 17182;
    private final static short constShort5 = (short) -16094;
    private final static short constShort6 = (short) 22485;
    private final static char constCharacter1 = (char) 13293;
    private final static char constCharacter2 = (char) 56753;
    private final static char constCharacter3 = (char) 44491;
    private final static char constCharacter4 = (char) 22845;
    private final static char constCharacter5 = (char) 7781;
    private final static char constCharacter6 = (char) 36265;
    private final static int constInteger1 = (int) 917766143;
    private final static int constInteger2 = (int) -1442709503;
    private final static int constInteger3 = (int) 346030079;
    private final static int constInteger4 = (int) -653656063;
    private final static int constInteger5 = (int) -892731391;
    private final static int constInteger6 = (int) 460718079;
    private final static long constLong1 = (long) 6.14572464150046e+018;
    private final static long constLong2 = (long) -8.31702261184646e+018;
    private final static long constLong3 = (long) 7.53789987631137e+018;
    private final static long constLong4 = (long) -1.35445758793168e+018;
    private final static long constLong5 = (long) -3.49423036088608e+018;
    private final static long constLong6 = (long) 7.02786721851166e+018;
    private final static float constFloat1 = (float) 1.52965057846069e+038F;
    private final static float constFloat2 = (float) -1.83288069992065e+038F;
    private final static float constFloat3 = (float) -3.18744703701782e+038F;
    private final static float constFloat4 = (float) -1.18882822961426e+038F;
    private final static float constFloat5 = (float) 3.27924684091187e+038F;
    private final static float constFloat6 = (float) 1.06847079441834e+038F;
    private final static double constDouble1 = (double) -9.17279944302305e+306;
    private final static double constDouble2 = (double) 3.50673050477293e+307;
    private final static double constDouble3 = (double) -1.47812640785508e+308;
    private final static double constDouble4 = (double) -9.3176331184392e+307;
    private final static double constDouble5 = (double) 1.7250019909508e+308;
    private final static double constDouble6 = (double) 4.88045597159886e+307;
    private final static boolean constBoolMax = true;
    private final static boolean constBoolMin = false;
    
    /* Test filterArguments: data-type -> byte */
	@Test(groups = { "level.extended" })
	public void test_filterArguments_Byte() throws WrongMethodTypeException, Throwable {
		MethodHandle target = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "targetByte", 
				methodType(byte.class, byte.class, byte.class, byte.class, byte.class, long.class, Byte.class,  byte.class, byte.class));
		
		/* Filters (Ex: f0 - filter0) */
		MethodHandle f0 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterByte0", methodType(byte.class, byte.class));
		MethodHandle f1 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterByte1", methodType(byte.class, byte.class));
		MethodHandle f2 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterByte2", methodType(byte.class, byte.class));
		MethodHandle f3 = MethodHandles.identity(byte.class).asType(methodType(long.class, byte.class)); /* Convert into 2 stack-slot data-type */
		MethodHandle f4 = MethodHandles.identity(byte.class).asType(methodType(Byte.class, byte.class)); /* Convert into object */
		
		/* Leading null filters */
		MethodHandle out = filterArguments(target, 0, null, null, null, f2, f3, f4);
		AssertJUnit.assertEquals (targetByte(Byte.MAX_VALUE, constByte1, constByte2, filterByte2(constByte3), (long) constByte4, new Byte(constByte5), Byte.MIN_VALUE, constByte6), 
				(byte) out.invokeExact(Byte.MAX_VALUE, constByte1, constByte2, constByte3, constByte4, constByte5, Byte.MIN_VALUE, constByte6));
		
		/* Intermediate null filters */
		out = filterArguments(target, 1, f0, null, null, f3, f4);
		AssertJUnit.assertEquals (targetByte(Byte.MAX_VALUE, filterByte0(constByte1), constByte2, constByte3, (long) constByte4, new Byte(constByte5), Byte.MIN_VALUE, constByte6), 
				(byte) out.invokeExact(Byte.MAX_VALUE, constByte1, constByte2, constByte3, constByte4, constByte5, Byte.MIN_VALUE, constByte6));
		
		/* Trailing null filters */
		out = filterArguments(target, 2, f1, f2, f3, f4, null, null);
		AssertJUnit.assertEquals (targetByte(constByte1, Byte.MAX_VALUE, filterByte1(constByte2), filterByte2(constByte3), (long) constByte4, new Byte(constByte5), Byte.MIN_VALUE, constByte6), 
				(byte) out.invokeExact(constByte1, Byte.MAX_VALUE, constByte2, constByte3, constByte4, constByte5, Byte.MIN_VALUE, constByte6));
		
		/* Leading and intermediate null filters */
		out = filterArguments(target, 0, null, f0, null, null, f3, f4);
		AssertJUnit.assertEquals (targetByte(Byte.MAX_VALUE, filterByte0(constByte1), constByte2, constByte3, (long) constByte4, new Byte(constByte5), Byte.MIN_VALUE, constByte6), 
				(byte) out.invokeExact(Byte.MAX_VALUE, constByte1, constByte2, constByte3, constByte4, constByte5, Byte.MIN_VALUE, constByte6));
		
		/* Leading and trailing null filters */
		out = filterArguments(target, 0, null, null, null, f2, f3, f4, null, null);
		AssertJUnit.assertEquals (targetByte(Byte.MAX_VALUE, constByte1, constByte2, filterByte2(constByte3), (long) constByte4, new Byte(constByte5), Byte.MIN_VALUE, constByte6), 
				(byte) out.invokeExact(Byte.MAX_VALUE, constByte1, constByte2, constByte3, constByte4, constByte5, Byte.MIN_VALUE, constByte6));
		
		/* Intermediate and trailing null filters */
		out = filterArguments(target, 1, f0, null, null, f3, f4, null, null);
		AssertJUnit.assertEquals (targetByte(Byte.MAX_VALUE, filterByte0(constByte1), constByte2, constByte3, (long) constByte4, new Byte(constByte5), Byte.MIN_VALUE, constByte6), 
				(byte) out.invokeExact(Byte.MAX_VALUE, constByte1, constByte2, constByte3, constByte4, constByte5, Byte.MIN_VALUE, constByte6));
		
		/* Leading, intermediate and trailing null filters */
		out = filterArguments(target, 0, null, null, f0, null, f3, f4, null, null);
		AssertJUnit.assertEquals (targetByte(Byte.MAX_VALUE, constByte1, filterByte0(constByte2), constByte3, (long) constByte4, new Byte(constByte5), Byte.MIN_VALUE, constByte6), 
				(byte) out.invokeExact(Byte.MAX_VALUE, constByte1, constByte2, constByte3, constByte4, constByte5, Byte.MIN_VALUE, constByte6));
		
		/* Only null filters */
		out = filterArguments(target, 0, null, null, null, null, null, null, null, null);
		AssertJUnit.assertEquals (targetByte(Byte.MAX_VALUE, constByte1, constByte2, constByte3, (long) constByte4, new Byte(constByte5), Byte.MIN_VALUE, constByte6), 
				(byte) out.invokeExact(Byte.MAX_VALUE, constByte1, constByte2, constByte3, (long) constByte4, new Byte(constByte5), Byte.MIN_VALUE, constByte6));
		
		out = filterArguments(target, 4, null, null, null, null);
		AssertJUnit.assertEquals (targetByte(Byte.MAX_VALUE, constByte1, constByte2, constByte3, (long) constByte4, new Byte(constByte5), Byte.MIN_VALUE, constByte6), 
				(byte) out.invokeExact(Byte.MAX_VALUE, constByte1, constByte2, constByte3, (long) constByte4, new Byte(constByte5), Byte.MIN_VALUE, constByte6));
		
		out = filterArguments(target, 2, null, null);
		AssertJUnit.assertEquals (targetByte(Byte.MAX_VALUE, constByte1, constByte2, constByte3, (long) constByte4, new Byte(constByte5), Byte.MIN_VALUE, constByte6), 
				(byte) out.invokeExact(Byte.MAX_VALUE, constByte1, constByte2, constByte3, (long) constByte4, new Byte(constByte5), Byte.MIN_VALUE, constByte6));
		
		/* No Null filters */
		out = filterArguments(target, 0, f0, f1, f2, f2, f3, f4, f0, f1);
		AssertJUnit.assertEquals (targetByte(filterByte0(Byte.MAX_VALUE), filterByte1(constByte1), filterByte2(constByte2), filterByte2(constByte3), (long) constByte4, new Byte(constByte5), filterByte0(Byte.MIN_VALUE), filterByte1(constByte6)), 
				(byte) out.invokeExact(Byte.MAX_VALUE, constByte1, constByte2, constByte3, constByte4, constByte5, Byte.MIN_VALUE, constByte6));
		
		out = filterArguments(target, 3, f2, f3, f4, f0, f1);
		AssertJUnit.assertEquals (targetByte(Byte.MAX_VALUE, constByte1, constByte2, filterByte2(constByte3), (long) constByte4, new Byte(constByte5), filterByte0(Byte.MIN_VALUE), filterByte1(constByte6)), 
				(byte) out.invokeExact(Byte.MAX_VALUE, constByte1, constByte2, constByte3, constByte4, constByte5, Byte.MIN_VALUE, constByte6));
		
		out = filterArguments(target, 5, f4, f0, f1);
		AssertJUnit.assertEquals (targetByte(Byte.MAX_VALUE, constByte1, constByte2, constByte3, (long) constByte4, new Byte(constByte5), filterByte0(Byte.MIN_VALUE), filterByte1(constByte6)), 
				(byte) out.invokeExact(Byte.MAX_VALUE, constByte1, constByte2, constByte3, (long) constByte4, constByte5, Byte.MIN_VALUE, constByte6));
	}
	
	/* Divide by 2 */
	public static byte filterByte0 (byte a) {
		System.gc();
		return (byte) (a/2);
	}
	
	/* Subtract 1 */
	public static byte filterByte1 (byte a) {
		System.gc();
		return (byte) (a-1); 
	}
	
	/* Add 1 */
	public static byte filterByte2 (byte a) {
		System.gc();
		return (byte) (a+1); 
	}

	/* Target - returns average of input parameters */
	public static byte targetByte (byte a, byte b, byte c, byte d, long e, Byte f, byte g, byte h) {
		System.gc();
		return averageByte (a, b, c, d, (byte) e, f.byteValue(), g, h);
	}
	
	public static byte averageByte (byte... args) {
		byte total = 0;
		for(byte arg : args) {
			total += (arg/(args.length));
		}
		return total;
	}
	
    /* Test filterArguments: data-type -> short */
	@Test(groups = { "level.extended" })
	public void test_filterArguments_Short() throws WrongMethodTypeException, Throwable {
		MethodHandle target = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "targetShort", 
				methodType(short.class, short.class, short.class, short.class, short.class, long.class, Short.class,  short.class, short.class));
		
		/* Filters (Ex: f0 - filter0) */
		MethodHandle f0 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterShort0", methodType(short.class, short.class));
		MethodHandle f1 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterShort1", methodType(short.class, short.class));
		MethodHandle f2 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterShort2", methodType(short.class, short.class));
		MethodHandle f3 = MethodHandles.identity(short.class).asType(methodType(long.class, short.class)); /* Convert into 2 stack-slot data-type */
		MethodHandle f4 = MethodHandles.identity(short.class).asType(methodType(Short.class, short.class)); /* Convert into object */
		
		/* Leading null filters */
		MethodHandle out = filterArguments(target, 0, null, null, null, f2, f3, f4);
		AssertJUnit.assertEquals (targetShort(Short.MAX_VALUE, constShort1, constShort2, filterShort2(constShort3), (long) constShort4, new Short(constShort5), Short.MIN_VALUE, constShort6), 
				(short) out.invokeExact(Short.MAX_VALUE, constShort1, constShort2, constShort3, constShort4, constShort5, Short.MIN_VALUE, constShort6));
		
		/* Intermediate null filters */
		out = filterArguments(target, 1, f0, null, null, f3, f4);
		AssertJUnit.assertEquals (targetShort(Short.MAX_VALUE, filterShort0(constShort1), constShort2, constShort3, (long) constShort4, new Short(constShort5), Short.MIN_VALUE, constShort6), 
				(short) out.invokeExact(Short.MAX_VALUE, constShort1, constShort2, constShort3, constShort4, constShort5, Short.MIN_VALUE, constShort6));
		
		/* Trailing null filters */
		out = filterArguments(target, 2, f1, f2, f3, f4, null, null);
		AssertJUnit.assertEquals (targetShort(constShort1, Short.MAX_VALUE, filterShort1(constShort2), filterShort2(constShort3), (long) constShort4, new Short(constShort5), Short.MIN_VALUE, constShort6), 
				(short) out.invokeExact(constShort1, Short.MAX_VALUE, constShort2, constShort3, constShort4, constShort5, Short.MIN_VALUE, constShort6));
		
		/* Leading and intermediate null filters */
		out = filterArguments(target, 0, null, f0, null, null, f3, f4);
		AssertJUnit.assertEquals (targetShort(Short.MAX_VALUE, filterShort0(constShort1), constShort2, constShort3, (long) constShort4, new Short(constShort5), Short.MIN_VALUE, constShort6), 
				(short) out.invokeExact(Short.MAX_VALUE, constShort1, constShort2, constShort3, constShort4, constShort5, Short.MIN_VALUE, constShort6));
		
		/* Leading and trailing null filters */
		out = filterArguments(target, 0, null, null, null, f2, f3, f4, null, null);
		AssertJUnit.assertEquals (targetShort(Short.MAX_VALUE, constShort1, constShort2, filterShort2(constShort3), (long) constShort4, new Short(constShort5), Short.MIN_VALUE, constShort6), 
				(short) out.invokeExact(Short.MAX_VALUE, constShort1, constShort2, constShort3, constShort4, constShort5, Short.MIN_VALUE, constShort6));
		
		/* Intermediate and trailing null filters */
		out = filterArguments(target, 1, f0, null, null, f3, f4, null, null);
		AssertJUnit.assertEquals (targetShort(Short.MAX_VALUE, filterShort0(constShort1), constShort2, constShort3, (long) constShort4, new Short(constShort5), Short.MIN_VALUE, constShort6), 
				(short) out.invokeExact(Short.MAX_VALUE, constShort1, constShort2, constShort3, constShort4, constShort5, Short.MIN_VALUE, constShort6));
		
		/* Leading, intermediate and trailing null filters */
		out = filterArguments(target, 0, null, null, f0, null, f3, f4, null, null);
		AssertJUnit.assertEquals (targetShort(Short.MAX_VALUE, constShort1, filterShort0(constShort2), constShort3, (long) constShort4, new Short(constShort5), Short.MIN_VALUE, constShort6), 
				(short) out.invokeExact(Short.MAX_VALUE, constShort1, constShort2, constShort3, constShort4, constShort5, Short.MIN_VALUE, constShort6));
		
		/* Only null filters */
		out = filterArguments(target, 0, null, null, null, null, null, null, null, null);
		AssertJUnit.assertEquals (targetShort(Short.MAX_VALUE, constShort1, constShort2, constShort3, (long) constShort4, new Short(constShort5), Short.MIN_VALUE, constShort6), 
				(short) out.invokeExact(Short.MAX_VALUE, constShort1, constShort2, constShort3, (long) constShort4, new Short(constShort5), Short.MIN_VALUE, constShort6));
		
		out = filterArguments(target, 4, null, null, null, null);
		AssertJUnit.assertEquals (targetShort(Short.MAX_VALUE, constShort1, constShort2, constShort3, (long) constShort4, new Short(constShort5), Short.MIN_VALUE, constShort6), 
				(short) out.invokeExact(Short.MAX_VALUE, constShort1, constShort2, constShort3, (long) constShort4, new Short(constShort5), Short.MIN_VALUE, constShort6));
		
		out = filterArguments(target, 2, null, null);
		AssertJUnit.assertEquals (targetShort(Short.MAX_VALUE, constShort1, constShort2, constShort3, (long) constShort4, new Short(constShort5), Short.MIN_VALUE, constShort6), 
				(short) out.invokeExact(Short.MAX_VALUE, constShort1, constShort2, constShort3, (long) constShort4, new Short(constShort5), Short.MIN_VALUE, constShort6));
		
		/* No Null filters */
		out = filterArguments(target, 0, f0, f1, f2, f2, f3, f4, f0, f1);
		AssertJUnit.assertEquals (targetShort(filterShort0(Short.MAX_VALUE), filterShort1(constShort1), filterShort2(constShort2), filterShort2(constShort3), (long) constShort4, new Short(constShort5), filterShort0(Short.MIN_VALUE), filterShort1(constShort6)), 
				(short) out.invokeExact(Short.MAX_VALUE, constShort1, constShort2, constShort3, constShort4, constShort5, Short.MIN_VALUE, constShort6));
		
		out = filterArguments(target, 3, f2, f3, f4, f0, f1);
		AssertJUnit.assertEquals (targetShort(Short.MAX_VALUE, constShort1, constShort2, filterShort2(constShort3), (long) constShort4, new Short(constShort5), filterShort0(Short.MIN_VALUE), filterShort1(constShort6)), 
				(short) out.invokeExact(Short.MAX_VALUE, constShort1, constShort2, constShort3, constShort4, constShort5, Short.MIN_VALUE, constShort6));
		
		out = filterArguments(target, 5, f4, f0, f1);
		AssertJUnit.assertEquals (targetShort(Short.MAX_VALUE, constShort1, constShort2, constShort3, (long) constShort4, new Short(constShort5), filterShort0(Short.MIN_VALUE), filterShort1(constShort6)), 
				(short) out.invokeExact(Short.MAX_VALUE, constShort1, constShort2, constShort3, (long) constShort4, constShort5, Short.MIN_VALUE, constShort6));
	}
	
	/* Divide by 2 */
	public static short filterShort0 (short a) {
		System.gc();
		return (short) (a/2);
	}
	
	/* Subtract 1 */
	public static short filterShort1 (short a) {
		System.gc();
		return (short) (a-1); 
	}
	
	/* Add 1 */
	public static short filterShort2 (short a) {
		System.gc();
		return (short) (a+1); 
	}

	/* Target - returns average of input parameters */
	public static short targetShort (short a, short b, short c, short d, long e, Short f, short g, short h) {
		System.gc();
		return averageShort (a, b, c, d, (short) e, f.shortValue(), g, h);
	}
	
	public static short averageShort (short... args) {
		short total = 0;
		for(short arg : args) {
			total += (arg/(args.length));
		}
		return total;
	}
	
    /* Test filterArguments: data-type -> char */
	@Test(groups = { "level.extended" })
	public void test_filterArguments_Character() throws WrongMethodTypeException, Throwable {
		MethodHandle target = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "targetCharacter", 
				methodType(char.class, char.class, char.class, char.class, char.class, long.class, Character.class,  char.class, char.class));
		
		/* Filters (Ex: f0 - filter0) */
		MethodHandle f0 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterCharacter0", methodType(char.class, char.class));
		MethodHandle f1 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterCharacter1", methodType(char.class, char.class));
		MethodHandle f2 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterCharacter2", methodType(char.class, char.class));
		MethodHandle f3 = MethodHandles.identity(char.class).asType(methodType(long.class, char.class)); /* Convert into 2 stack-slot data-type */
		MethodHandle f4 = MethodHandles.identity(char.class).asType(methodType(Character.class, char.class)); /* Convert into object */
		
		/* Leading null filters */
		MethodHandle out = filterArguments(target, 0, null, null, null, f2, f3, f4);
		AssertJUnit.assertEquals (targetCharacter(Character.MAX_VALUE, constCharacter1, constCharacter2, filterCharacter2(constCharacter3), (long) constCharacter4, new Character(constCharacter5), Character.MIN_VALUE, constCharacter6), 
				(char) out.invokeExact(Character.MAX_VALUE, constCharacter1, constCharacter2, constCharacter3, constCharacter4, constCharacter5, Character.MIN_VALUE, constCharacter6));
		
		/* Intermediate null filters */
		out = filterArguments(target, 1, f0, null, null, f3, f4);
		AssertJUnit.assertEquals (targetCharacter(Character.MAX_VALUE, filterCharacter0(constCharacter1), constCharacter2, constCharacter3, (long) constCharacter4, new Character(constCharacter5), Character.MIN_VALUE, constCharacter6), 
				(char) out.invokeExact(Character.MAX_VALUE, constCharacter1, constCharacter2, constCharacter3, constCharacter4, constCharacter5, Character.MIN_VALUE, constCharacter6));
		
		/* Trailing null filters */
		out = filterArguments(target, 2, f1, f2, f3, f4, null, null);
		AssertJUnit.assertEquals (targetCharacter(constCharacter1, Character.MAX_VALUE, filterCharacter1(constCharacter2), filterCharacter2(constCharacter3), (long) constCharacter4, new Character(constCharacter5), Character.MIN_VALUE, constCharacter6), 
				(char) out.invokeExact(constCharacter1, Character.MAX_VALUE, constCharacter2, constCharacter3, constCharacter4, constCharacter5, Character.MIN_VALUE, constCharacter6));
		
		/* Leading and intermediate null filters */
		out = filterArguments(target, 0, null, f0, null, null, f3, f4);
		AssertJUnit.assertEquals (targetCharacter(Character.MAX_VALUE, filterCharacter0(constCharacter1), constCharacter2, constCharacter3, (long) constCharacter4, new Character(constCharacter5), Character.MIN_VALUE, constCharacter6), 
				(char) out.invokeExact(Character.MAX_VALUE, constCharacter1, constCharacter2, constCharacter3, constCharacter4, constCharacter5, Character.MIN_VALUE, constCharacter6));
		
		/* Leading and trailing null filters */
		out = filterArguments(target, 0, null, null, null, f2, f3, f4, null, null);
		AssertJUnit.assertEquals (targetCharacter(Character.MAX_VALUE, constCharacter1, constCharacter2, filterCharacter2(constCharacter3), (long) constCharacter4, new Character(constCharacter5), Character.MIN_VALUE, constCharacter6), 
				(char) out.invokeExact(Character.MAX_VALUE, constCharacter1, constCharacter2, constCharacter3, constCharacter4, constCharacter5, Character.MIN_VALUE, constCharacter6));
		
		/* Intermediate and trailing null filters */
		out = filterArguments(target, 1, f0, null, null, f3, f4, null, null);
		AssertJUnit.assertEquals (targetCharacter(Character.MAX_VALUE, filterCharacter0(constCharacter1), constCharacter2, constCharacter3, (long) constCharacter4, new Character(constCharacter5), Character.MIN_VALUE, constCharacter6), 
				(char) out.invokeExact(Character.MAX_VALUE, constCharacter1, constCharacter2, constCharacter3, constCharacter4, constCharacter5, Character.MIN_VALUE, constCharacter6));
		
		/* Leading, intermediate and trailing null filters */
		out = filterArguments(target, 0, null, null, f0, null, f3, f4, null, null);
		AssertJUnit.assertEquals (targetCharacter(Character.MAX_VALUE, constCharacter1, filterCharacter0(constCharacter2), constCharacter3, (long) constCharacter4, new Character(constCharacter5), Character.MIN_VALUE, constCharacter6), 
				(char) out.invokeExact(Character.MAX_VALUE, constCharacter1, constCharacter2, constCharacter3, constCharacter4, constCharacter5, Character.MIN_VALUE, constCharacter6));
		
		/* Only null filters */
		out = filterArguments(target, 0, null, null, null, null, null, null, null, null);
		AssertJUnit.assertEquals (targetCharacter(Character.MAX_VALUE, constCharacter1, constCharacter2, constCharacter3, (long) constCharacter4, new Character(constCharacter5), Character.MIN_VALUE, constCharacter6), 
				(char) out.invokeExact(Character.MAX_VALUE, constCharacter1, constCharacter2, constCharacter3, (long) constCharacter4, new Character(constCharacter5), Character.MIN_VALUE, constCharacter6));
		
		out = filterArguments(target, 4, null, null, null, null);
		AssertJUnit.assertEquals (targetCharacter(Character.MAX_VALUE, constCharacter1, constCharacter2, constCharacter3, (long) constCharacter4, new Character(constCharacter5), Character.MIN_VALUE, constCharacter6), 
				(char) out.invokeExact(Character.MAX_VALUE, constCharacter1, constCharacter2, constCharacter3, (long) constCharacter4, new Character(constCharacter5), Character.MIN_VALUE, constCharacter6));
		
		out = filterArguments(target, 2, null, null);
		AssertJUnit.assertEquals (targetCharacter(Character.MAX_VALUE, constCharacter1, constCharacter2, constCharacter3, (long) constCharacter4, new Character(constCharacter5), Character.MIN_VALUE, constCharacter6), 
				(char) out.invokeExact(Character.MAX_VALUE, constCharacter1, constCharacter2, constCharacter3, (long) constCharacter4, new Character(constCharacter5), Character.MIN_VALUE, constCharacter6));
		
		/* No Null filters */
		out = filterArguments(target, 0, f0, f1, f2, f2, f3, f4, f0, f1);
		AssertJUnit.assertEquals (targetCharacter(filterCharacter0(Character.MAX_VALUE), filterCharacter1(constCharacter1), filterCharacter2(constCharacter2), filterCharacter2(constCharacter3), (long) constCharacter4, new Character(constCharacter5), filterCharacter0(Character.MIN_VALUE), filterCharacter1(constCharacter6)), 
				(char) out.invokeExact(Character.MAX_VALUE, constCharacter1, constCharacter2, constCharacter3, constCharacter4, constCharacter5, Character.MIN_VALUE, constCharacter6));
		
		out = filterArguments(target, 3, f2, f3, f4, f0, f1);
		AssertJUnit.assertEquals (targetCharacter(Character.MAX_VALUE, constCharacter1, constCharacter2, filterCharacter2(constCharacter3), (long) constCharacter4, new Character(constCharacter5), filterCharacter0(Character.MIN_VALUE), filterCharacter1(constCharacter6)), 
				(char) out.invokeExact(Character.MAX_VALUE, constCharacter1, constCharacter2, constCharacter3, constCharacter4, constCharacter5, Character.MIN_VALUE, constCharacter6));
		
		out = filterArguments(target, 5, f4, f0, f1);
		AssertJUnit.assertEquals (targetCharacter(Character.MAX_VALUE, constCharacter1, constCharacter2, constCharacter3, (long) constCharacter4, new Character(constCharacter5), filterCharacter0(Character.MIN_VALUE), filterCharacter1(constCharacter6)), 
				(char) out.invokeExact(Character.MAX_VALUE, constCharacter1, constCharacter2, constCharacter3, (long) constCharacter4, constCharacter5, Character.MIN_VALUE, constCharacter6));
	}
	
	/* Divide by 2 */
	public static char filterCharacter0 (char a) {
		System.gc();
		return (char) (a/2);
	}
	
	/* Subtract 1 */
	public static char filterCharacter1 (char a) {
		System.gc();
		return (char) (a-1); 
	}
	
	/* Add 1 */
	public static char filterCharacter2 (char a) {
		System.gc();
		return (char) (a+1); 
	}

	/* Target - returns average of input parameters */
	public static char targetCharacter (char a, char b, char c, char d, long e, Character f, char g, char h) {
		System.gc();
		return averageCharacter (a, b, c, d, (char) e, f.charValue(), g, h);
	}
	
	public static char averageCharacter (char... args) {
		char total = 0;
		for(char arg : args) {
			total += (arg/(args.length));
		}
		return total;
	}
	
    /* Test filterArguments: data-type -> int */
	@Test(groups = { "level.extended" })
	public void test_filterArguments_Integer() throws WrongMethodTypeException, Throwable {
		MethodHandle target = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "targetInteger", 
				methodType(int.class, int.class, int.class, int.class, int.class, long.class, Integer.class,  int.class, int.class));
		
		/* Filters (Ex: f0 - filter0) */
		MethodHandle f0 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterInteger0", methodType(int.class, int.class));
		MethodHandle f1 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterInteger1", methodType(int.class, int.class));
		MethodHandle f2 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterInteger2", methodType(int.class, int.class));
		MethodHandle f3 = MethodHandles.identity(int.class).asType(methodType(long.class, int.class)); /* Convert into 2 stack-slot data-type */
		MethodHandle f4 = MethodHandles.identity(int.class).asType(methodType(Integer.class, int.class)); /* Convert into object */
		
		/* Leading null filters */
		MethodHandle out = filterArguments(target, 0, null, null, null, f2, f3, f4);
		AssertJUnit.assertEquals (targetInteger(Integer.MAX_VALUE, constInteger1, constInteger2, filterInteger2(constInteger3), (long) constInteger4, new Integer(constInteger5), Integer.MIN_VALUE, constInteger6), 
				(int) out.invokeExact(Integer.MAX_VALUE, constInteger1, constInteger2, constInteger3, constInteger4, constInteger5, Integer.MIN_VALUE, constInteger6));
		
		/* Intermediate null filters */
		out = filterArguments(target, 1, f0, null, null, f3, f4);
		AssertJUnit.assertEquals (targetInteger(Integer.MAX_VALUE, filterInteger0(constInteger1), constInteger2, constInteger3, (long) constInteger4, new Integer(constInteger5), Integer.MIN_VALUE, constInteger6), 
				(int) out.invokeExact(Integer.MAX_VALUE, constInteger1, constInteger2, constInteger3, constInteger4, constInteger5, Integer.MIN_VALUE, constInteger6));
		
		/* Trailing null filters */
		out = filterArguments(target, 2, f1, f2, f3, f4, null, null);
		AssertJUnit.assertEquals (targetInteger(constInteger1, Integer.MAX_VALUE, filterInteger1(constInteger2), filterInteger2(constInteger3), (long) constInteger4, new Integer(constInteger5), Integer.MIN_VALUE, constInteger6), 
				(int) out.invokeExact(constInteger1, Integer.MAX_VALUE, constInteger2, constInteger3, constInteger4, constInteger5, Integer.MIN_VALUE, constInteger6));
		
		/* Leading and intermediate null filters */
		out = filterArguments(target, 0, null, f0, null, null, f3, f4);
		AssertJUnit.assertEquals (targetInteger(Integer.MAX_VALUE, filterInteger0(constInteger1), constInteger2, constInteger3, (long) constInteger4, new Integer(constInteger5), Integer.MIN_VALUE, constInteger6), 
				(int) out.invokeExact(Integer.MAX_VALUE, constInteger1, constInteger2, constInteger3, constInteger4, constInteger5, Integer.MIN_VALUE, constInteger6));
		
		/* Leading and trailing null filters */
		out = filterArguments(target, 0, null, null, null, f2, f3, f4, null, null);
		AssertJUnit.assertEquals (targetInteger(Integer.MAX_VALUE, constInteger1, constInteger2, filterInteger2(constInteger3), (long) constInteger4, new Integer(constInteger5), Integer.MIN_VALUE, constInteger6), 
				(int) out.invokeExact(Integer.MAX_VALUE, constInteger1, constInteger2, constInteger3, constInteger4, constInteger5, Integer.MIN_VALUE, constInteger6));
		
		/* Intermediate and trailing null filters */
		out = filterArguments(target, 1, f0, null, null, f3, f4, null, null);
		AssertJUnit.assertEquals (targetInteger(Integer.MAX_VALUE, filterInteger0(constInteger1), constInteger2, constInteger3, (long) constInteger4, new Integer(constInteger5), Integer.MIN_VALUE, constInteger6), 
				(int) out.invokeExact(Integer.MAX_VALUE, constInteger1, constInteger2, constInteger3, constInteger4, constInteger5, Integer.MIN_VALUE, constInteger6));
		
		/* Leading, intermediate and trailing null filters */
		out = filterArguments(target, 0, null, null, f0, null, f3, f4, null, null);
		AssertJUnit.assertEquals (targetInteger(Integer.MAX_VALUE, constInteger1, filterInteger0(constInteger2), constInteger3, (long) constInteger4, new Integer(constInteger5), Integer.MIN_VALUE, constInteger6), 
				(int) out.invokeExact(Integer.MAX_VALUE, constInteger1, constInteger2, constInteger3, constInteger4, constInteger5, Integer.MIN_VALUE, constInteger6));
		
		/* Only null filters */
		out = filterArguments(target, 0, null, null, null, null, null, null, null, null);
		AssertJUnit.assertEquals (targetInteger(Integer.MAX_VALUE, constInteger1, constInteger2, constInteger3, (long) constInteger4, new Integer(constInteger5), Integer.MIN_VALUE, constInteger6), 
				(int) out.invokeExact(Integer.MAX_VALUE, constInteger1, constInteger2, constInteger3, (long) constInteger4, new Integer(constInteger5), Integer.MIN_VALUE, constInteger6));
		
		out = filterArguments(target, 4, null, null, null, null);
		AssertJUnit.assertEquals (targetInteger(Integer.MAX_VALUE, constInteger1, constInteger2, constInteger3, (long) constInteger4, new Integer(constInteger5), Integer.MIN_VALUE, constInteger6), 
				(int) out.invokeExact(Integer.MAX_VALUE, constInteger1, constInteger2, constInteger3, (long) constInteger4, new Integer(constInteger5), Integer.MIN_VALUE, constInteger6));
		
		out = filterArguments(target, 2, null, null);
		AssertJUnit.assertEquals (targetInteger(Integer.MAX_VALUE, constInteger1, constInteger2, constInteger3, (long) constInteger4, new Integer(constInteger5), Integer.MIN_VALUE, constInteger6), 
				(int) out.invokeExact(Integer.MAX_VALUE, constInteger1, constInteger2, constInteger3, (long) constInteger4, new Integer(constInteger5), Integer.MIN_VALUE, constInteger6));
		
		/* No Null filters */
		out = filterArguments(target, 0, f0, f1, f2, f2, f3, f4, f0, f1);
		AssertJUnit.assertEquals (targetInteger(filterInteger0(Integer.MAX_VALUE), filterInteger1(constInteger1), filterInteger2(constInteger2), filterInteger2(constInteger3), (long) constInteger4, new Integer(constInteger5), filterInteger0(Integer.MIN_VALUE), filterInteger1(constInteger6)), 
				(int) out.invokeExact(Integer.MAX_VALUE, constInteger1, constInteger2, constInteger3, constInteger4, constInteger5, Integer.MIN_VALUE, constInteger6));
		
		out = filterArguments(target, 3, f2, f3, f4, f0, f1);
		AssertJUnit.assertEquals (targetInteger(Integer.MAX_VALUE, constInteger1, constInteger2, filterInteger2(constInteger3), (long) constInteger4, new Integer(constInteger5), filterInteger0(Integer.MIN_VALUE), filterInteger1(constInteger6)), 
				(int) out.invokeExact(Integer.MAX_VALUE, constInteger1, constInteger2, constInteger3, constInteger4, constInteger5, Integer.MIN_VALUE, constInteger6));
		
		out = filterArguments(target, 5, f4, f0, f1);
		AssertJUnit.assertEquals (targetInteger(Integer.MAX_VALUE, constInteger1, constInteger2, constInteger3, (long) constInteger4, new Integer(constInteger5), filterInteger0(Integer.MIN_VALUE), filterInteger1(constInteger6)), 
				(int) out.invokeExact(Integer.MAX_VALUE, constInteger1, constInteger2, constInteger3, (long) constInteger4, constInteger5, Integer.MIN_VALUE, constInteger6));
	}
	
	/* Divide by 2 */
	public static int filterInteger0 (int a) {
		System.gc();
		return (int) (a/2);
	}
	
	/* Subtract 1 */
	public static int filterInteger1 (int a) {
		System.gc();
		return (int) (a-1); 
	}
	
	/* Add 1 */
	public static int filterInteger2 (int a) {
		System.gc();
		return (int) (a+1); 
	}

	/* Target - returns average of input parameters */
	public static int targetInteger (int a, int b, int c, int d, long e, Integer f, int g, int h) {
		System.gc();
		return averageInteger (a, b, c, d, (int) e, f.intValue(), g, h);
	}
	
	public static int averageInteger (int... args) {
		int total = 0;
		for(int arg : args) {
			total += (arg/(args.length));
		}
		return total;
	}
	
	/* Test filterArguments: data-type -> long */
	@Test(groups = { "level.extended" })
	public void test_filterArguments_Long() throws WrongMethodTypeException, Throwable {
		MethodHandle target = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "targetLong", 
				methodType(long.class, long.class, long.class, long.class, long.class, int.class, Long.class,  long.class, long.class));
		
		/* Filters (Ex: f0 - filter0) */
		MethodHandle f0 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterLong0", methodType(long.class, long.class));
		MethodHandle f1 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterLong1", methodType(long.class, long.class));
		MethodHandle f2 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterLong2", methodType(long.class, long.class));
		MethodHandle f3 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterLong3", methodType(int.class, long.class)); 
		MethodHandle f4 = MethodHandles.identity(long.class).asType(methodType(Long.class, long.class)); /* Convert into object */
		
		/* Leading null filters */
		MethodHandle out = filterArguments(target, 0, null, null, null, f2, f3, f4);
		AssertJUnit.assertEquals (targetLong(Long.MAX_VALUE, constLong1, constLong2, filterLong2(constLong3), constInteger4, new Long(constLong5), Long.MIN_VALUE, constLong6), 
				(long) out.invokeExact(Long.MAX_VALUE, constLong1, constLong2, constLong3, (long) constInteger4, constLong5, Long.MIN_VALUE, constLong6));
		
		/* Intermediate null filters */
		out = filterArguments(target, 1, f0, null, null, f3, f4);
		AssertJUnit.assertEquals (targetLong(Long.MAX_VALUE, filterLong0(constLong1), constLong2, constLong3, constInteger4, new Long(constLong5), Long.MIN_VALUE, constLong6), 
				(long) out.invokeExact(Long.MAX_VALUE, constLong1, constLong2, constLong3, (long) constInteger4, constLong5, Long.MIN_VALUE, constLong6));
		
		/* Trailing null filters */
		out = filterArguments(target, 2, f1, f2, f3, f4, null, null);
		AssertJUnit.assertEquals (targetLong(constLong1, Long.MAX_VALUE, filterLong1(constLong2), filterLong2(constLong3), constInteger4, new Long(constLong5), Long.MIN_VALUE, constLong6), 
				(long) out.invokeExact(constLong1, Long.MAX_VALUE, constLong2, constLong3, (long) constInteger4, constLong5, Long.MIN_VALUE, constLong6));
		
		/* Leading and intermediate null filters */
		out = filterArguments(target, 0, null, f0, null, null, f3, f4);
		AssertJUnit.assertEquals (targetLong(Long.MAX_VALUE, filterLong0(constLong1), constLong2, constLong3, constInteger4, new Long(constLong5), Long.MIN_VALUE, constLong6), 
				(long) out.invokeExact(Long.MAX_VALUE, constLong1, constLong2, constLong3, (long) constInteger4, constLong5, Long.MIN_VALUE, constLong6));
		
		/* Leading and trailing null filters */
		out = filterArguments(target, 0, null, null, null, f2, f3, f4, null, null);
		AssertJUnit.assertEquals (targetLong(Long.MAX_VALUE, constLong1, constLong2, filterLong2(constLong3), constInteger4, new Long(constLong5), Long.MIN_VALUE, constLong6), 
				(long) out.invokeExact(Long.MAX_VALUE, constLong1, constLong2, constLong3, (long) constInteger4, constLong5, Long.MIN_VALUE, constLong6));
		
		/* Intermediate and trailing null filters */
		out = filterArguments(target, 1, f0, null, null, f3, f4, null, null);
		AssertJUnit.assertEquals (targetLong(Long.MAX_VALUE, filterLong0(constLong1), constLong2, constLong3, constInteger4, new Long(constLong5), Long.MIN_VALUE, constLong6), 
				(long) out.invokeExact(Long.MAX_VALUE, constLong1, constLong2, constLong3, (long) constInteger4, constLong5, Long.MIN_VALUE, constLong6));
		
		/* Leading, intermediate and trailing null filters */
		out = filterArguments(target, 0, null, null, f0, null, f3, f4, null, null);
		AssertJUnit.assertEquals (targetLong(Long.MAX_VALUE, constLong1, filterLong0(constLong2), constLong3, constInteger4, new Long(constLong5), Long.MIN_VALUE, constLong6), 
				(long) out.invokeExact(Long.MAX_VALUE, constLong1, constLong2, constLong3, (long) constInteger4, constLong5, Long.MIN_VALUE, constLong6));
		
		/* Only null filters */
		out = filterArguments(target, 0, null, null, null, null, null, null, null, null);
		AssertJUnit.assertEquals (targetLong(Long.MAX_VALUE, constLong1, constLong2, constLong3, constInteger4, new Long(constLong5), Long.MIN_VALUE, constLong6), 
				(long) out.invokeExact(Long.MAX_VALUE, constLong1, constLong2, constLong3, constInteger4, new Long(constLong5), Long.MIN_VALUE, constLong6));
		
		out = filterArguments(target, 4, null, null, null, null);
		AssertJUnit.assertEquals (targetLong(Long.MAX_VALUE, constLong1, constLong2, constLong3, constInteger4, new Long(constLong5), Long.MIN_VALUE, constLong6), 
				(long) out.invokeExact(Long.MAX_VALUE, constLong1, constLong2, constLong3, constInteger4, new Long(constLong5), Long.MIN_VALUE, constLong6));
		
		out = filterArguments(target, 2, null, null);
		AssertJUnit.assertEquals (targetLong(Long.MAX_VALUE, constLong1, constLong2, constLong3, constInteger4, new Long(constLong5), Long.MIN_VALUE, constLong6), 
				(long) out.invokeExact(Long.MAX_VALUE, constLong1, constLong2, constLong3, constInteger4, new Long(constLong5), Long.MIN_VALUE, constLong6));
		
		/* No Null filters */
		out = filterArguments(target, 0, f0, f1, f2, f2, f3, f4, f0, f1);
		AssertJUnit.assertEquals (targetLong(filterLong0(Long.MAX_VALUE), filterLong1(constLong1), filterLong2(constLong2), filterLong2(constLong3), constInteger4, new Long(constLong5), filterLong0(Long.MIN_VALUE), filterLong1(constLong6)), 
				(long) out.invokeExact(Long.MAX_VALUE, constLong1, constLong2, constLong3, (long) constInteger4, constLong5, Long.MIN_VALUE, constLong6));
		
		out = filterArguments(target, 3, f2, f3, f4, f0, f1);
		AssertJUnit.assertEquals (targetLong(Long.MAX_VALUE, constLong1, constLong2, filterLong2(constLong3), constInteger4, new Long(constLong5), filterLong0(Long.MIN_VALUE), filterLong1(constLong6)), 
				(long) out.invokeExact(Long.MAX_VALUE, constLong1, constLong2, constLong3, (long) constInteger4, constLong5, Long.MIN_VALUE, constLong6));
		
		out = filterArguments(target, 5, f4, f0, f1);
		AssertJUnit.assertEquals (targetLong(Long.MAX_VALUE, constLong1, constLong2, constLong3, constInteger4, new Long(constLong5), filterLong0(Long.MIN_VALUE), filterLong1(constLong6)), 
				(long) out.invokeExact(Long.MAX_VALUE, constLong1, constLong2, constLong3, constInteger4, constLong5, Long.MIN_VALUE, constLong6));
	}
	
	/* Divide by 2 */
	public static long filterLong0 (long a) {
		System.gc();
		return (long) (a/2);
	}
	
	/* Subtract 1 */
	public static long filterLong1 (long a) {
		System.gc();
		return (long) (a-1); 
	}
	
	/* Add 1 */
	public static long filterLong2 (long a) {
		System.gc();
		return (long) (a+1); 
	}

	/* Convert into 1 stack-slot data-type */
	public static int filterLong3 (long a) {
		System.gc();
		return (int) (a); 
	}
	
	/* Target - returns average of input parameters */
	public static long targetLong (long a, long b, long c, long d, int e, Long f, long g, long h) {
		System.gc();
		return averageLong (a, b, c, d, (long) e, f.longValue(), g, h);
	}
	
	public static long averageLong (long... args) {
		long total = 0;
		for(long arg : args) {
			total += (arg/(args.length));
		}
		return total;
	}
	
    /* Test filterArguments: data-type -> float */
	@Test(groups = { "level.extended" })
	public void test_filterArguments_Float() throws WrongMethodTypeException, Throwable {
		MethodHandle target = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "targetFloat", 
				methodType(float.class, float.class, float.class, float.class, float.class, double.class, Float.class,  float.class, float.class));
		
		/* Filters (Ex: f0 - filter0) */
		MethodHandle f0 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterFloat0", methodType(float.class, float.class));
		MethodHandle f1 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterFloat1", methodType(float.class, float.class));
		MethodHandle f2 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterFloat2", methodType(float.class, float.class));
		MethodHandle f3 = MethodHandles.identity(float.class).asType(methodType(double.class, float.class)); /* Convert into 2 stack-slot data-type */
		MethodHandle f4 = MethodHandles.identity(float.class).asType(methodType(Float.class, float.class)); /* Convert into object */
		
		/* Leading null filters */
		MethodHandle out = filterArguments(target, 0, null, null, null, f2, f3, f4);
		AssertJUnit.assertEquals (targetFloat(Float.MAX_VALUE, constFloat1, constFloat2, filterFloat2(constFloat3), (double) constFloat4, new Float(constFloat5), Float.MIN_VALUE, constFloat6), 
				(float) out.invokeExact(Float.MAX_VALUE, constFloat1, constFloat2, constFloat3, constFloat4, constFloat5, Float.MIN_VALUE, constFloat6));
		
		/* Intermediate null filters */
		out = filterArguments(target, 1, f0, null, null, f3, f4);
		AssertJUnit.assertEquals (targetFloat(Float.MAX_VALUE, filterFloat0(constFloat1), constFloat2, constFloat3, (double) constFloat4, new Float(constFloat5), Float.MIN_VALUE, constFloat6), 
				(float) out.invokeExact(Float.MAX_VALUE, constFloat1, constFloat2, constFloat3, constFloat4, constFloat5, Float.MIN_VALUE, constFloat6));
		
		/* Trailing null filters */
		out = filterArguments(target, 2, f1, f2, f3, f4, null, null);
		AssertJUnit.assertEquals (targetFloat(constFloat1, Float.MAX_VALUE, filterFloat1(constFloat2), filterFloat2(constFloat3), (double) constFloat4, new Float(constFloat5), Float.MIN_VALUE, constFloat6), 
				(float) out.invokeExact(constFloat1, Float.MAX_VALUE, constFloat2, constFloat3, constFloat4, constFloat5, Float.MIN_VALUE, constFloat6));
		
		/* Leading and intermediate null filters */
		out = filterArguments(target, 0, null, f0, null, null, f3, f4);
		AssertJUnit.assertEquals (targetFloat(Float.MAX_VALUE, filterFloat0(constFloat1), constFloat2, constFloat3, (double) constFloat4, new Float(constFloat5), Float.MIN_VALUE, constFloat6), 
				(float) out.invokeExact(Float.MAX_VALUE, constFloat1, constFloat2, constFloat3, constFloat4, constFloat5, Float.MIN_VALUE, constFloat6));
		
		/* Leading and trailing null filters */
		out = filterArguments(target, 0, null, null, null, f2, f3, f4, null, null);
		AssertJUnit.assertEquals (targetFloat(Float.MAX_VALUE, constFloat1, constFloat2, filterFloat2(constFloat3), (double) constFloat4, new Float(constFloat5), Float.MIN_VALUE, constFloat6), 
				(float) out.invokeExact(Float.MAX_VALUE, constFloat1, constFloat2, constFloat3, constFloat4, constFloat5, Float.MIN_VALUE, constFloat6));
		
		/* Intermediate and trailing null filters */
		out = filterArguments(target, 1, f0, null, null, f3, f4, null, null);
		AssertJUnit.assertEquals (targetFloat(Float.MAX_VALUE, filterFloat0(constFloat1), constFloat2, constFloat3, (double) constFloat4, new Float(constFloat5), Float.MIN_VALUE, constFloat6), 
				(float) out.invokeExact(Float.MAX_VALUE, constFloat1, constFloat2, constFloat3, constFloat4, constFloat5, Float.MIN_VALUE, constFloat6));
		
		/* Leading, intermediate and trailing null filters */
		out = filterArguments(target, 0, null, null, f0, null, f3, f4, null, null);
		AssertJUnit.assertEquals (targetFloat(Float.MAX_VALUE, constFloat1, filterFloat0(constFloat2), constFloat3, (double) constFloat4, new Float(constFloat5), Float.MIN_VALUE, constFloat6), 
				(float) out.invokeExact(Float.MAX_VALUE, constFloat1, constFloat2, constFloat3, constFloat4, constFloat5, Float.MIN_VALUE, constFloat6));
		
		/* Only null filters */
		out = filterArguments(target, 0, null, null, null, null, null, null, null, null);
		AssertJUnit.assertEquals (targetFloat(Float.MAX_VALUE, constFloat1, constFloat2, constFloat3, (double) constFloat4, new Float(constFloat5), Float.MIN_VALUE, constFloat6), 
				(float) out.invokeExact(Float.MAX_VALUE, constFloat1, constFloat2, constFloat3, (double) constFloat4, new Float(constFloat5), Float.MIN_VALUE, constFloat6));
		
		out = filterArguments(target, 4, null, null, null, null);
		AssertJUnit.assertEquals (targetFloat(Float.MAX_VALUE, constFloat1, constFloat2, constFloat3, (double) constFloat4, new Float(constFloat5), Float.MIN_VALUE, constFloat6), 
				(float) out.invokeExact(Float.MAX_VALUE, constFloat1, constFloat2, constFloat3, (double) constFloat4, new Float(constFloat5), Float.MIN_VALUE, constFloat6));
		
		out = filterArguments(target, 2, null, null);
		AssertJUnit.assertEquals (targetFloat(Float.MAX_VALUE, constFloat1, constFloat2, constFloat3, (double) constFloat4, new Float(constFloat5), Float.MIN_VALUE, constFloat6), 
				(float) out.invokeExact(Float.MAX_VALUE, constFloat1, constFloat2, constFloat3, (double) constFloat4, new Float(constFloat5), Float.MIN_VALUE, constFloat6));
		
		/* No Null filters */
		out = filterArguments(target, 0, f0, f1, f2, f2, f3, f4, f0, f1);
		AssertJUnit.assertEquals (targetFloat(filterFloat0(Float.MAX_VALUE), filterFloat1(constFloat1), filterFloat2(constFloat2), filterFloat2(constFloat3), (double) constFloat4, new Float(constFloat5), filterFloat0(Float.MIN_VALUE), filterFloat1(constFloat6)), 
				(float) out.invokeExact(Float.MAX_VALUE, constFloat1, constFloat2, constFloat3, constFloat4, constFloat5, Float.MIN_VALUE, constFloat6));
		
		out = filterArguments(target, 3, f2, f3, f4, f0, f1);
		AssertJUnit.assertEquals (targetFloat(Float.MAX_VALUE, constFloat1, constFloat2, filterFloat2(constFloat3), (double) constFloat4, new Float(constFloat5), filterFloat0(Float.MIN_VALUE), filterFloat1(constFloat6)), 
				(float) out.invokeExact(Float.MAX_VALUE, constFloat1, constFloat2, constFloat3, constFloat4, constFloat5, Float.MIN_VALUE, constFloat6));
		
		out = filterArguments(target, 5, f4, f0, f1);
		AssertJUnit.assertEquals (targetFloat(Float.MAX_VALUE, constFloat1, constFloat2, constFloat3, (double) constFloat4, new Float(constFloat5), filterFloat0(Float.MIN_VALUE), filterFloat1(constFloat6)), 
				(float) out.invokeExact(Float.MAX_VALUE, constFloat1, constFloat2, constFloat3, (double) constFloat4, constFloat5, Float.MIN_VALUE, constFloat6));
	}
	
	/* Divide by 2 */
	public static float filterFloat0 (float a) {
		System.gc();
		return (float) (a/2);
	}
	
	/* Subtract 1 */
	public static float filterFloat1 (float a) {
		System.gc();
		return (float) (a-1); 
	}
	
	/* Add 1 */
	public static float filterFloat2 (float a) {
		System.gc();
		return (float) (a+1); 
	}

	/* Target - returns average of input parameters */
	public static float targetFloat (float a, float b, float c, float d, double e, Float f, float g, float h) {
		System.gc();
		return averageFloat (a, b, c, d, (float) e, f.floatValue(), g, h);
	}
	
	public static float averageFloat (float... args) {
		float total = 0;
		for(float arg : args) {
			total += (arg/(args.length));
		}
		return total;
	}
	
    /* Test filterArguments: data-type -> double */
	@Test(groups = { "level.extended" })
	public void test_filterArguments_Double() throws WrongMethodTypeException, Throwable {
		MethodHandle target = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "targetDouble", 
				methodType(double.class, double.class, double.class, double.class, double.class, float.class, Double.class,  double.class, double.class));
		
		/* Filters (Ex: f0 - filter0) */
		MethodHandle f0 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterDouble0", methodType(double.class, double.class));
		MethodHandle f1 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterDouble1", methodType(double.class, double.class));
		MethodHandle f2 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterDouble2", methodType(double.class, double.class));
		MethodHandle f3 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterDouble3", methodType(float.class, double.class)); 
		MethodHandle f4 = MethodHandles.identity(double.class).asType(methodType(Double.class, double.class)); /* Convert into object */
		
		/* Leading null filters */
		MethodHandle out = filterArguments(target, 0, null, null, null, f2, f3, f4);
		AssertJUnit.assertEquals (targetDouble(Double.MAX_VALUE, constDouble1, constDouble2, filterDouble2(constDouble3), constFloat4, new Double(constDouble5), Double.MIN_VALUE, constDouble6), 
				(double) out.invokeExact(Double.MAX_VALUE, constDouble1, constDouble2, constDouble3, (double) constFloat4, constDouble5, Double.MIN_VALUE, constDouble6));
		
		/* Intermediate null filters */
		out = filterArguments(target, 1, f0, null, null, f3, f4);
		AssertJUnit.assertEquals (targetDouble(Double.MAX_VALUE, filterDouble0(constDouble1), constDouble2, constDouble3, constFloat4, new Double(constDouble5), Double.MIN_VALUE, constDouble6), 
				(double) out.invokeExact(Double.MAX_VALUE, constDouble1, constDouble2, constDouble3, (double) constFloat4, constDouble5, Double.MIN_VALUE, constDouble6));
		
		/* Trailing null filters */
		out = filterArguments(target, 2, f1, f2, f3, f4, null, null);
		AssertJUnit.assertEquals (targetDouble(constDouble1, Double.MAX_VALUE, filterDouble1(constDouble2), filterDouble2(constDouble3), constFloat4, new Double(constDouble5), Double.MIN_VALUE, constDouble6), 
				(double) out.invokeExact(constDouble1, Double.MAX_VALUE, constDouble2, constDouble3, (double) constFloat4, constDouble5, Double.MIN_VALUE, constDouble6));
		
		/* Leading and intermediate null filters */
		out = filterArguments(target, 0, null, f0, null, null, f3, f4);
		AssertJUnit.assertEquals (targetDouble(Double.MAX_VALUE, filterDouble0(constDouble1), constDouble2, constDouble3, constFloat4, new Double(constDouble5), Double.MIN_VALUE, constDouble6), 
				(double) out.invokeExact(Double.MAX_VALUE, constDouble1, constDouble2, constDouble3, (double) constFloat4, constDouble5, Double.MIN_VALUE, constDouble6));
		
		/* Leading and trailing null filters */
		out = filterArguments(target, 0, null, null, null, f2, f3, f4, null, null);
		AssertJUnit.assertEquals (targetDouble(Double.MAX_VALUE, constDouble1, constDouble2, filterDouble2(constDouble3), constFloat4, new Double(constDouble5), Double.MIN_VALUE, constDouble6), 
				(double) out.invokeExact(Double.MAX_VALUE, constDouble1, constDouble2, constDouble3, (double) constFloat4, constDouble5, Double.MIN_VALUE, constDouble6));
		
		/* Intermediate and trailing null filters */
		out = filterArguments(target, 1, f0, null, null, f3, f4, null, null);
		AssertJUnit.assertEquals (targetDouble(Double.MAX_VALUE, filterDouble0(constDouble1), constDouble2, constDouble3, constFloat4, new Double(constDouble5), Double.MIN_VALUE, constDouble6), 
				(double) out.invokeExact(Double.MAX_VALUE, constDouble1, constDouble2, constDouble3, (double) constFloat4, constDouble5, Double.MIN_VALUE, constDouble6));
		
		/* Leading, intermediate and trailing null filters */
		out = filterArguments(target, 0, null, null, f0, null, f3, f4, null, null);
		AssertJUnit.assertEquals (targetDouble(Double.MAX_VALUE, constDouble1, filterDouble0(constDouble2), constDouble3, constFloat4, new Double(constDouble5), Double.MIN_VALUE, constDouble6), 
				(double) out.invokeExact(Double.MAX_VALUE, constDouble1, constDouble2, constDouble3, (double) constFloat4, constDouble5, Double.MIN_VALUE, constDouble6));
		
		/* Only null filters */
		out = filterArguments(target, 0, null, null, null, null, null, null, null, null);
		AssertJUnit.assertEquals (targetDouble(Double.MAX_VALUE, constDouble1, constDouble2, constDouble3, constFloat4, new Double(constDouble5), Double.MIN_VALUE, constDouble6), 
				(double) out.invokeExact(Double.MAX_VALUE, constDouble1, constDouble2, constDouble3, constFloat4, new Double(constDouble5), Double.MIN_VALUE, constDouble6));
		
		out = filterArguments(target, 4, null, null, null, null);
		AssertJUnit.assertEquals (targetDouble(Double.MAX_VALUE, constDouble1, constDouble2, constDouble3, constFloat4, new Double(constDouble5), Double.MIN_VALUE, constDouble6), 
				(double) out.invokeExact(Double.MAX_VALUE, constDouble1, constDouble2, constDouble3, constFloat4, new Double(constDouble5), Double.MIN_VALUE, constDouble6));
		
		out = filterArguments(target, 2, null, null);
		AssertJUnit.assertEquals (targetDouble(Double.MAX_VALUE, constDouble1, constDouble2, constDouble3, constFloat4, new Double(constDouble5), Double.MIN_VALUE, constDouble6), 
				(double) out.invokeExact(Double.MAX_VALUE, constDouble1, constDouble2, constDouble3, constFloat4, new Double(constDouble5), Double.MIN_VALUE, constDouble6));
		
		/* No Null filters */
		out = filterArguments(target, 0, f0, f1, f2, f2, f3, f4, f0, f1);
		AssertJUnit.assertEquals (targetDouble(filterDouble0(Double.MAX_VALUE), filterDouble1(constDouble1), filterDouble2(constDouble2), filterDouble2(constDouble3), constFloat4, new Double(constDouble5), filterDouble0(Double.MIN_VALUE), filterDouble1(constDouble6)), 
				(double) out.invokeExact(Double.MAX_VALUE, constDouble1, constDouble2, constDouble3, (double) constFloat4, constDouble5, Double.MIN_VALUE, constDouble6));
		
		out = filterArguments(target, 3, f2, f3, f4, f0, f1);
		AssertJUnit.assertEquals (targetDouble(Double.MAX_VALUE, constDouble1, constDouble2, filterDouble2(constDouble3), constFloat4, new Double(constDouble5), filterDouble0(Double.MIN_VALUE), filterDouble1(constDouble6)), 
				(double) out.invokeExact(Double.MAX_VALUE, constDouble1, constDouble2, constDouble3, (double) constFloat4, constDouble5, Double.MIN_VALUE, constDouble6));
		
		out = filterArguments(target, 5, f4, f0, f1);
		AssertJUnit.assertEquals (targetDouble(Double.MAX_VALUE, constDouble1, constDouble2, constDouble3, constFloat4, new Double(constDouble5), filterDouble0(Double.MIN_VALUE), filterDouble1(constDouble6)), 
				(double) out.invokeExact(Double.MAX_VALUE, constDouble1, constDouble2, constDouble3, constFloat4, constDouble5, Double.MIN_VALUE, constDouble6));
	}
	
	/* Divide by 2 */
	public static double filterDouble0 (double a) {
		System.gc();
		return (double) (a/2);
	}
	
	/* Subtract 1 */
	public static double filterDouble1 (double a) {
		System.gc();
		return (double) (a-1); 
	}
	
	/* Add 1 */
	public static double filterDouble2 (double a) {
		System.gc();
		return (double) (a+1); 
	}

	/* Convert into 1 stack-slot data-type */
	public static float filterDouble3 (double a) {
		System.gc();
		return (float) (a); 
	}
	
	/* Target - returns average of input parameters */
	public static double targetDouble (double a, double b, double c, double d, float e, Double f, double g, double h) {
		System.gc();
		return averageDouble (a, b, c, d, (double) e, f.doubleValue(), g, h);
	}
	
	public static double averageDouble (double... args) {
		double total = 0;
		for(double arg : args) {
			total += (arg/(args.length));
		}
		return total;
	}
	
    /* Test filterArguments: data-type -> Double Object */
	@Test(groups = { "level.extended" })
	public void test_filterArguments_DoubleObject() throws WrongMethodTypeException, Throwable {
		MethodHandle target = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "targetDoubleObject", 
				methodType(Double.class, Double.class, Double.class, Double.class, Double.class, float.class, double.class,  Double.class, Double.class));
		
		/* Filters (Ex: f0 - filter0) */
		MethodHandle f0 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterDoubleObject0", methodType(Double.class, Double.class));
		MethodHandle f1 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterDoubleObject1", methodType(Double.class, Double.class));
		MethodHandle f2 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterDoubleObject2", methodType(Double.class, Double.class));
		MethodHandle f3 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterDoubleObject3", methodType(float.class, Double.class)); 
		MethodHandle f4 = MethodHandles.identity(Double.class).asType(methodType(double.class, Double.class)); /* Convert into 2 stack-slot primitive data-type */
		
		Double d1 = new Double (constDouble1);
		Double d2 = new Double (constDouble2);
		Double d3 = new Double (constDouble3);
		Double d5 = new Double (constDouble5);
		Double d6 = new Double (constDouble6);
		Double dMax = new Double (Double.MAX_VALUE);
		Double dMin = new Double (Double.MIN_VALUE);
		
		/* Leading null filters */
		MethodHandle out = filterArguments(target, 0, null, null, null, f2, f3, f4);
		AssertJUnit.assertEquals (targetDoubleObject(dMax, d1, d2, filterDoubleObject2(d3), constFloat4, d5.doubleValue(), dMin, d6), 
				(Double) out.invokeExact(dMax, d1, d2, d3, new Double ((double) constFloat4), d5, dMin, d6));
		
		/* Intermediate null filters */
		out = filterArguments(target, 1, f0, null, null, f3, f4);
		AssertJUnit.assertEquals (targetDoubleObject(dMax, filterDoubleObject0(d1), d2, d3, constFloat4, d5.doubleValue(), dMin, d6), 
				(Double) out.invokeExact(dMax, d1, d2, d3, new Double ((double) constFloat4), d5, dMin, d6));
		
		/* Trailing null filters */
		out = filterArguments(target, 2, f1, f2, f3, f4, null, null);
		AssertJUnit.assertEquals (targetDoubleObject(d1, dMax, filterDoubleObject1(d2), filterDoubleObject2(d3), constFloat4, d5.doubleValue(), dMin, d6), 
				(Double) out.invokeExact(d1, dMax, d2, d3, new Double ((double) constFloat4), d5, dMin, d6));
		
		/* Leading and intermediate null filters */
		out = filterArguments(target, 0, null, f0, null, null, f3, f4);
		AssertJUnit.assertEquals (targetDoubleObject(dMax, filterDoubleObject0(d1), d2, d3, constFloat4, d5.doubleValue(), dMin, d6), 
				(Double) out.invokeExact(dMax, d1, d2, d3, new Double ((double) constFloat4), d5, dMin, d6));
		
		/* Leading and trailing null filters */
		out = filterArguments(target, 0, null, null, null, f2, f3, f4, null, null);
		AssertJUnit.assertEquals (targetDoubleObject(dMax, d1, d2, filterDoubleObject2(d3), constFloat4, d5.doubleValue(), dMin, d6), 
				(Double) out.invokeExact(dMax, d1, d2, d3, new Double ((double) constFloat4), d5, dMin, d6));
		
		/* Intermediate and trailing null filters */
		out = filterArguments(target, 1, f0, null, null, f3, f4, null, null);
		AssertJUnit.assertEquals (targetDoubleObject(dMax, filterDoubleObject0(d1), d2, d3, constFloat4, d5.doubleValue(), dMin, d6), 
				(Double) out.invokeExact(dMax, d1, d2, d3, new Double ((double) constFloat4), d5, dMin, d6));
		
		/* Leading, intermediate and trailing null filters */
		out = filterArguments(target, 0, null, null, f0, null, f3, f4, null, null);
		AssertJUnit.assertEquals (targetDoubleObject(dMax, d1, filterDoubleObject0(d2), d3, constFloat4, d5.doubleValue(), dMin, d6), 
				(Double) out.invokeExact(dMax, d1, d2, d3, new Double ((double) constFloat4), d5, dMin, d6));
		
		/* Only null filters */
		out = filterArguments(target, 0, null, null, null, null, null, null, null, null);
		AssertJUnit.assertEquals (targetDoubleObject(dMax, d1, d2, d3, constFloat4, d5.doubleValue(), dMin, d6), 
				(Double) out.invokeExact(dMax, d1, d2, d3, constFloat4, d5.doubleValue(), dMin, d6));
		
		out = filterArguments(target, 4, null, null, null, null);
		AssertJUnit.assertEquals (targetDoubleObject(dMax, d1, d2, d3, constFloat4, d5.doubleValue(), dMin, d6), 
				(Double) out.invokeExact(dMax, d1, d2, d3, constFloat4, d5.doubleValue(), dMin, d6));
		
		out = filterArguments(target, 2, null, null);
		AssertJUnit.assertEquals (targetDoubleObject(dMax, d1, d2, d3, constFloat4, d5.doubleValue(), dMin, d6), 
				(Double) out.invokeExact(dMax, d1, d2, d3, constFloat4, d5.doubleValue(), dMin, d6));
		
		/* No Null filters */
		out = filterArguments(target, 0, f0, f1, f2, f2, f3, f4, f0, f1);
		AssertJUnit.assertEquals (targetDoubleObject(filterDoubleObject0(dMax), filterDoubleObject1(d1), filterDoubleObject2(d2), filterDoubleObject2(d3), constFloat4, d5.doubleValue(), filterDoubleObject0(dMin), filterDoubleObject1(d6)), 
				(Double) out.invokeExact(dMax, d1, d2, d3, new Double ((double) constFloat4), d5, dMin, d6));
		
		out = filterArguments(target, 3, f2, f3, f4, f0, f1);
		AssertJUnit.assertEquals (targetDoubleObject(dMax, d1, d2, filterDoubleObject2(d3), constFloat4, d5.doubleValue(), filterDoubleObject0(dMin), filterDoubleObject1(d6)), 
				(Double) out.invokeExact(dMax, d1, d2, d3, new Double ((double) constFloat4), d5, dMin, d6));
		
		out = filterArguments(target, 5, f4, f0, f1);
		AssertJUnit.assertEquals (targetDoubleObject(dMax, d1, d2, d3, constFloat4, d5.doubleValue(), filterDoubleObject0(dMin), filterDoubleObject1(d6)), 
				(Double) out.invokeExact(dMax, d1, d2, d3, constFloat4, d5, dMin, d6));
	}
	
	/* Divide by 2 */
	public static Double filterDoubleObject0 (Double a) {
		System.gc();
		return new Double(a/2);
	}
	
	/* Subtract 1 */
	public static Double filterDoubleObject1 (Double a) {
		System.gc();
		return new Double(a-1); 
	}
	
	/* Add 1 */
	public static Double filterDoubleObject2 (Double a) {
		System.gc();
		return new Double(a+1); 
	}

	/* Convert into 1 stack-slot primitive data-type */
	public static float filterDoubleObject3 (Double a) {
		System.gc();
		return a.floatValue(); 
	}
	
	/* Target - returns average of input parameters */
	public static Double targetDoubleObject (Double a, Double b, Double c, Double d, float e, double f, Double g, Double h) {
		System.gc();
		return averageDoubleObject (a, b, c, d, new Double((double) e), new Double(f), g, h);
	}
	
	public static Double averageDoubleObject (Double... args) {
		double total = 0;
		for(Double arg : args) {
			total += (arg.doubleValue()/(args.length));
		}
		return new Double(total);
	}
	
   /* Test filterArguments: data-type -> boolean */
	@Test(groups = { "level.extended" })
	public void test_filterArguments_Boolean() throws WrongMethodTypeException, Throwable {
		MethodHandle target = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "targetBoolean", 
				methodType(boolean.class, boolean.class, boolean.class, boolean.class, boolean.class, boolean.class));
		
		/* Filters (Ex: f0 - filter0) */
		MethodHandle f0 = MethodHandles.publicLookup().findStatic(FilterArgumentsTest.class, "filterBoolean0", methodType(boolean.class, boolean.class));
		
		/* Leading null filters */
		MethodHandle out = filterArguments(target, 0, null, null, null, f0);
		AssertJUnit.assertEquals (targetBoolean(constBoolMin, constBoolMax, constBoolMin, filterBoolean0(constBoolMax), constBoolMin), 
				(boolean) out.invokeExact(constBoolMin, constBoolMax, constBoolMin, constBoolMax, constBoolMin));
		
		/* Intermediate null filters */
		out = filterArguments(target, 0, f0, null, null, null, f0);
		AssertJUnit.assertEquals (targetBoolean(filterBoolean0(constBoolMin), constBoolMax, constBoolMin, constBoolMax, filterBoolean0(constBoolMin)), 
				(boolean) out.invokeExact(constBoolMin, constBoolMax, constBoolMin, constBoolMax, constBoolMin));
		
		/* Trailing null filters */
		out = filterArguments(target, 0, f0, null, null, null);
		AssertJUnit.assertEquals (targetBoolean(filterBoolean0(constBoolMin), constBoolMax, constBoolMin, constBoolMax, constBoolMin), 
				(boolean) out.invokeExact(constBoolMin, constBoolMax, constBoolMin, constBoolMax, constBoolMin));
		
		/* Leading and intermediate null filters */
		out = filterArguments(target, 0, null, null, f0, null, f0);
		AssertJUnit.assertEquals (targetBoolean(constBoolMin, constBoolMax, filterBoolean0(constBoolMin), constBoolMax, filterBoolean0(constBoolMin)), 
				(boolean) out.invokeExact(constBoolMin, constBoolMax, constBoolMin, constBoolMax, constBoolMin));

		/* Leading and trailing null filters */
		out = filterArguments(target, 0, null, null, f0, null, null);
		AssertJUnit.assertEquals (targetBoolean(constBoolMin, constBoolMax, filterBoolean0(constBoolMin), constBoolMax, constBoolMin), 
				(boolean) out.invokeExact(constBoolMin, constBoolMax, constBoolMin, constBoolMax, constBoolMin));
		
		/* Intermediate and trailing null filters */
		out = filterArguments(target, 0, f0, null, f0, null, null);
		AssertJUnit.assertEquals (targetBoolean(filterBoolean0(constBoolMin), constBoolMax, filterBoolean0(constBoolMin), constBoolMax, constBoolMin), 
				(boolean) out.invokeExact(constBoolMin, constBoolMax, constBoolMin, constBoolMax, constBoolMin));
		
		/* Leading, intermediate and trailing null filters */
		out = filterArguments(target, 0, null, f0, null, f0, null);
		AssertJUnit.assertEquals (targetBoolean(constBoolMin, filterBoolean0(constBoolMax), constBoolMin, filterBoolean0(constBoolMax), constBoolMin), 
				(boolean) out.invokeExact(constBoolMin, constBoolMax, constBoolMin, constBoolMax, constBoolMin));
		
		/* Only null filters */
		out = filterArguments(target, 0, null, null, null, null, null);
		AssertJUnit.assertEquals (targetBoolean(filterBoolean0(constBoolMin), constBoolMax, constBoolMin, constBoolMax, constBoolMin), 
				(boolean) out.invokeExact(constBoolMin, constBoolMax, constBoolMin, constBoolMax, constBoolMin));
		
		/* No Null filters */
		out = filterArguments(target, 1, f0, f0, f0, f0);
		AssertJUnit.assertEquals (targetBoolean(constBoolMin, filterBoolean0(constBoolMax), filterBoolean0(constBoolMin), filterBoolean0(constBoolMax), filterBoolean0(constBoolMin)), 
				(boolean) out.invokeExact(constBoolMin, constBoolMax, constBoolMin, constBoolMax, constBoolMin));
	}
	
	/* Returns inverse of the input parameter */
	public static boolean filterBoolean0 (boolean a) {
		System.gc();
		if (a == true) {
			return false;
		} else {
			return true;
		}
	}
	
	/* Target - returns last boolean */
	public static boolean targetBoolean (boolean a, boolean b, boolean c, boolean d, boolean e) {
		System.gc();
		return lastBoolean (a, b, c, d, e);
	}
	
	public static boolean lastBoolean (boolean... args) {
		boolean last = constBoolMin;
		for(boolean arg: args) {
			last = arg;
		}
		return last;
	}
}
