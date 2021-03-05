/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

/**
 * This file contains the native code used by org.openj9.test.jep389.downcall.PrimitiveTypeTests
 * via a Clinker FFI DownCall.
 *
 * Created by jincheng@ca.ibm.com
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

/**
 * Add two integers.
 *
 * @param intArg1 the 1st integer to add
 * @param intArg2 the 2nd integer to add
 * @return the sum of the two integers
 */
int
add2Ints(int intArg1, int intArg2)
{
	int intSum = intArg1 + intArg2;
	return intSum;
}

/**
 * Add two integers (the 2nd one dereferenced from a pointer).
 *
 * @param intArg1 an integer to add
 * @param intArg2 a pointer to integer
 * @return the sum of the 2 integers
 */
int
addIntAndIntFromPointer(int intArg1, int *intArg2)
{
	int intSum = intArg1 + *intArg2;
	return intSum;
}

/**
 * Add three integers.
 *
 * @param intArg1 the 1st integer to add
 * @param intArg2 the 2nd integer to add
 * @param intArg3 the 3rd integer to add
 * @return the sum of the 3 integers
 */
int
add3Ints(int intArg1, int intArg2, int intArg3)
{
	int intSum = intArg1 + intArg2 + intArg3;
	return intSum;
}

/**
 * Add integers from the va_list with the specified count
 *
 * @param intCount the count of the integers
 * @param intArgList the integer va_list
 * @return the sum of integers from the va_list
 */
int
addIntsFromVaList(int intCount, va_list intVaList)
{
	int intSum = 0;
	while (intCount > 0) {
		intSum += va_arg(intVaList, int);
		intCount--;
	}
	return intSum;
}

/**
 * Add an integer and a character.
 *
 * @param intArg the integer to add
 * @param charArg the character to add
 * @return the sum of the integer and the character
 */
int
addIntAndChar(int intArg, char charArg)
{
	int sum = intArg + charArg;
	return sum;
}

/**
 * Add two integers without return value.
 *
 * @param intArg1 the 1st integer to add
 * @param intArg2 the 2nd integer to add
 * @return void
 */
void
add2IntsReturnVoid(int intArg1, int intArg2)
{
	int intSum = intArg1 + intArg2;
}

/**
 * Add two booleans with the OR (||) operator.
 *
 * @param boolArg1 the 1st boolean
 * @param boolArg2 the 2nd boolean
 * @return the result of the OR operation
 */
int
add2BoolsWithOr(int boolArg1, int boolArg2)
{
	int result = (boolArg1 || boolArg2);
	return result;
}

/**
 * Add two booleans with the OR (||) operator (the 2nd one dereferenced from a pointer).
 *
 * @param boolArg1 a boolean
 * @param boolArg2 a pointer to boolean
 * @return the result of the OR operation
 */
int
addBoolAndBoolFromPointerWithOr(int boolArg1, int *boolArg2)
{
	int result = (boolArg1 || *boolArg2);
	return result;
}

/**
 * Generate a new character by manipulating two characters.
 *
 * @param charArg1 the 1st character
 * @param charArg2 the 2nd character
 * @return the resulting character generated with the 2 characters
 */
short
createNewCharFrom2Chars(short charArg1, short charArg2)
{
	int diff = (charArg2 >= charArg1) ? (charArg2 - charArg1) : (charArg1 - charArg2);
	diff = (diff > 5) ? 5 : diff;
	short result = diff + 'A';
	return result;
}

/**
 * Generate a new character by manipulating two characters (the 1st one dereferenced from a pointer).
 *
 * @param charArg1 a pointer to character
 * @param charArg2 the 2nd character
 * @return the resulting character generated with the 2 characters
 */
short
createNewCharFromCharAndCharFromPointer(short *charArg1, short charArg2)
{
	int diff = (charArg2 >= *charArg1) ? (charArg2 - *charArg1) : (*charArg1 - charArg2);
	diff = (diff > 5) ? 5 : diff;
	short result = diff + 'A';
	return result;
}

/**
 * Add two bytes.
 *
 * Note: the passed-in arguments are byte given the byte size
 * in Java is the same size as the character in C code.
 *
 * @param byteArg1 the 1st byte to add
 * @param byteArg2 the 2nd byte to add
 * @return the sum of the two bytes
 */
char
add2Bytes(char byteArg1, char byteArg2)
{
	char byteSum = byteArg1 + byteArg2;
	return byteSum;
}

/**
 * Add two bytes (the 2nd one dereferenced from a pointer).
 *
 * Note: the passed-in arguments are byte given the byte size
 * in Java is the same size as the character in C code.
 *
 * @param byteArg1 a byte to add
 * @param byteArg2 a pointer to byte in char size
 * @return the sum of the two bytes
 */
char
addByteAndByteFromPointer(char byteArg1, char *byteArg2)
{
	char byteSum = byteArg1 + *byteArg2;
	return byteSum;
}

/**
 * Add two short integers.
 *
 * @param shortArg1 the 1st short integer to add
 * @param shortArg2 the 2nd short integer to add
 * @return the sum of the two short integers
 */
short
add2Shorts(short shortArg1, short shortArg2)
{
	short shortSum = shortArg1 + shortArg2;
	return shortSum;
}

/**
 * Add two short integers (the 1st one dereferenced from a pointer).
 *
 * @param shortArg1 a pointer to short integer
 * @param shortArg2 a short integer
 * @return the sum of the two short integers
 */
short
addShortAndShortFromPointer(short *shortArg1, short shortArg2)
{
	short shortSum = *shortArg1 + shortArg2;
	return shortSum;
}

/**
 * Add two long integers.
 *
 * @param longArg1 the 1st long integer to add
 * @param longArg2 the 2nd long integer to add
 * @return the sum of the two long integers
 */
long
add2Longs(long longArg1, long longArg2)
{
	long longSum = longArg1 + longArg2;
	return longSum;
}

/**
 * Add two long integers (the 1st one dereferenced from a pointer).
 *
 * @param longArg1 a pointer to long integer
 * @param longArg2 a long integer
 * @return the sum of the two long integers
 */
long
addLongAndLongFromPointer(long *longArg1, long longArg2)
{
	long longSum = *longArg1 + longArg2;
	return longSum;
}

/**
 * Add long integers from the va_list with the specified count
 *
 * @param longCount the count of the long integers
 * @param longArgList the long va_list
 * @return the sum of long integers from the va_list
 */
long
addLongsFromVaList(int longCount, va_list longVaList)
{
	long longSum = 0;
	while (longCount > 0) {
		longSum += va_arg(longVaList, long);
		longCount--;
	}
	return longSum;
}

/**
 * Add two floats.
 *
 * @param floatArg1 the 1st float to add
 * @param floatArg2 the 2nd float to add
 * @return the sum of the two floats
 */
float
add2Floats(float floatArg1, float floatArg2)
{
	float floatSum = floatArg1 + floatArg2;
	return floatSum;
}

/**
 * Add two floats (the 2nd one dereferenced from a pointer).
 *
 * @param floatArg1 a float
 * @param floatArg2 a pointer to float
 * @return the sum of the two floats
 */
float
addFloatAndFloatFromPointer(float floatArg1, float *floatArg2)
{
	float floatSum = floatArg1 + *floatArg2;
	return floatSum;
}

/**
 * Add two doubles.
 *
 * @param doubleArg1 the 1st double to add
 * @param doubleArg2 the 2nd double to add
 * @return the sum of the two doubles
 */
double
add2Doubles(double doubleArg1, double doubleArg2)
{
	double doubleSum = doubleArg1 + doubleArg2;
	return doubleSum;
}

/**
 * Add two doubles (the 1st one dereferenced from a pointer).
 *
 * @param doubleArg1 a pointer to double
 * @param doubleArg2 a double
 * @return the sum of the two doubles
 */
double
addDoubleAndDoubleFromPointer(double *doubleArg1, double doubleArg2)
{
	double doubleSum = *doubleArg1 + doubleArg2;
	return doubleSum;
}

/**
 * Add doubles from the va_list with the specified count
 *
 * @param doubleCount the count of the double arguments
 * @param doubleArgList the double va_list
 * @return the sum of doubles from the va_list
 */
double
addDoublesFromVaList(int doubleCount, va_list doubleVaList)
{
	double doubleSum = 0;
	while (doubleCount > 0) {
		doubleSum += va_arg(doubleVaList, double);
		doubleCount--;
	}
	return doubleSum;
}
