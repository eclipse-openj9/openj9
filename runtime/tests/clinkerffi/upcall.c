/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

/**
 * This file contains the native code used by the test cases via a Clinker FFI Upcall in java,
 * which come from:
 * org.openj9.test.jep389.downcall (JDK17)
 * org.openj9.test.jep434.downcall (JDK20)
 * org.openj9.test.jep434.downcall (JDK21+)
 *
 * Created by jincheng@ca.ibm.com
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include "downcall.h"

/**
 * Add two booleans with the OR (||) operator by invoking an upcall method.
 *
 * @param boolArg1 the 1st boolean
 * @param boolArg2 the 2nd boolean
 * @param upcallMH the function pointer to the upcall method
 * @return the result value
 */
bool
add2BoolsWithOrByUpcallMH(bool boolArg1, bool boolArg2, bool (*upcallMH)(bool, bool))
{
	bool result = (*upcallMH)(boolArg1, boolArg2);
	return result;
}

/**
 * Add two booleans with the OR (||) operator (the 2nd one is dereferenced from a pointer)
 * by invoking an upcall method.
 *
 * @param boolArg1 a boolean
 * @param boolArg2 a pointer to boolean
 * @param upcallMH the function pointer to the upcall method
 * @return the result value
 */
bool
addBoolAndBoolFromPointerWithOrByUpcallMH(bool boolArg1, bool *boolArg2, bool (*upcallMH)(bool, bool *))
{
	bool result = (*upcallMH)(boolArg1, boolArg2);
	return result;
}

/**
 * Add two booleans (the 2nd one assigned in native) with
 * the OR (||) operator by invoking an upcall method.
 *
 * @param boolArg1 the 1st boolean
 * @param upcallMH the function pointer to the upcall method
 * @return the result value
 */
bool
addBoolAndBoolFromNativePtrWithOrByUpcallMH(bool boolArg1, bool (*upcallMH)(bool, bool *))
{
	bool boolArg2 = 1;
	bool result = (*upcallMH)(boolArg1, &boolArg2);
	return result;
}

/**
 * Add two booleans with the OR (||) operator (the 2nd one is dereferenced from a pointer)
 * by invoking an upcall method and return a pointer to the XOR result of booleans.
 *
 * @param boolArg1 the 1st boolean
 * @param boolArg2 a pointer to the 2nd boolean
 * @param upcallMH the function pointer to the upcall method
 * @return the resulting pointer to boolean
 */
bool *
addBoolAndBoolFromPtrWithOr_RetPtr_ByUpcallMH(bool boolArg1, bool *boolArg2, bool *(*upcallMH)(bool, bool *))
{
	bool *resultPtr = (*upcallMH)(boolArg1, boolArg2);
	return resultPtr;
}

/**
 * Add two bytes by invoking an upcall method.
 * Note:
 * the passed-in arguments are byte given the byte size
 * in Java is the same size as the char in C code.
 *
 * @param byteArg1 the 1st byte
 * @param byteArg2 the 2nd byte
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
char
add2BytesByUpcallMH(char byteArg1, char byteArg2, char (*upcallMH)(char, char))
{
	char byteSum = (*upcallMH)(byteArg1, byteArg2);
	return byteSum;
}

/**
 * Add two bytes (the 2nd one is dereferenced from a pointer)
 * by invoking an upcall method.
 * Note:
 * the passed-in arguments are byte given the byte size
 * in Java is the same size as the char in C code.
 *
 * @param byteArg1 the 1st byte
 * @param byteArg2 a pointer to the 2nd byte in char size
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
char
addByteAndByteFromPointerByUpcallMH(char byteArg1, char *byteArg2, char (*upcallMH)(char, char *))
{
	char byteSum = (*upcallMH)(byteArg1, byteArg2);
	return byteSum;
}

/**
 * Add two bytes (the 2nd one assigned in native) by invoking an upcall method.
 * Note: the passed-in arguments are byte given the byte size
 * in Java is the same size as the char in C code.
 *
 * @param byteArg1 the 1st byte
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
char
addByteAndByteFromNativePtrByUpcallMH(char byteArg1, char (*upcallMH)(char, char *))
{
	char byteArg2 = 55;
	char byteSum = (*upcallMH)(byteArg1, &byteArg2);
	return byteSum;
}

/**
 * Add two bytes (the 2nd one is dereferenced from a pointer) by invoking an upcall method
 * and return a pointer to the sum.
 * Note:
 * the passed-in arguments are byte given the byte size
 * in Java is the same size as the char in C code.
 *
 * @param byteArg1 the 1st byte
 * @param byteArg2 a pointer to the 2nd byte in char size
 * @param upcallMH the function pointer to the upcall method
 * @return the pointer to the sum
 */
char *
addByteAndByteFromPtr_RetPtr_ByUpcallMH(char byteArg1, char *byteArg2, char *(*upcallMH)(char, char *))
{
	char *byteSum = (*upcallMH)(byteArg1, byteArg2);
	return byteSum;
}

/**
 * Generate a new char by manipulating two chars by invoking an upcall method.
 *
 * @param charArg1 the 1st char
 * @param charArg2 the 2nd char
 * @param upcallMH the function pointer to the upcall method
 * @return the resulting char
 */
short
createNewCharFrom2CharsByUpcallMH(short charArg1, short charArg2, short (*upcallMH)(short, short))
{
	short result = (*upcallMH)(charArg1, charArg2);
	return result;
}

/**
 * Generate a new char by manipulating two chars (the 1st one dereferenced from a pointer)
 * by invoking an upcall method.
 *
 * @param charArg1 a pointer to the 1st char
 * @param charArg2 the 2nd char
 * @param upcallMH the function pointer to the upcall method
 * @return the resulting char
 */
short
createNewCharFromCharAndCharFromPointerByUpcallMH(short *charArg1, short charArg2, short (*upcallMH)(short *, short))
{
	short result = (*upcallMH)(charArg1, charArg2);
	return result;
}

/**
 * Generate a new char by manipulating two chars (the 1st one is assigned in native)
 * via invoking an upcall method.
 *
 * @param charArg2 the 2nd char
 * @param upcallMH the function pointer to the upcall method
 * @return the resulting char
 */
short
createNewCharFromCharAndCharFromNativePtrByUpcallMH(short charArg2, short (*upcallMH)(short *, short))
{
	short charArg1 = 'B';
	short result = (*upcallMH)(&charArg1, charArg2);
	return result;
}

/**
 * Generate a new char by manipulating two chars (the 1st one dereferenced from a pointer)
 * by invoking an upcall method and return a pointer to a new char.
 *
 * @param charArg1 a pointer to the 1st char
 * @param charArg2 the 2nd char
 * @param upcallMH the function pointer to the upcall method
 * @return the resulting pointer to a new char
 */
short *
createNewCharFromCharAndCharFromPtr_RetPtr_ByUpcallMH(short *charArg1, short charArg2, short *(*upcallMH)(short *, short))
{
	short *resultPtr = (*upcallMH)(charArg1, charArg2);
	return resultPtr;
}

/**
 * Add two shorts by invoking an upcall method.
 *
 * @param shortArg1 the 1st short
 * @param shortArg2 the 2nd short
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
short
add2ShortsByUpcallMH(short shortArg1, short shortArg2, short (*upcallMH)(short, short))
{
	short shortSum = (*upcallMH)(shortArg1, shortArg2);
	return shortSum;
}

/**
 * Add two shorts (the 1st one dereferenced from a pointer)
 * by invoking an upcall method.
 *
 * @param shortArg1 a pointer to the 1st short
 * @param shortArg2 the 2nd short
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
short
addShortAndShortFromPointerByUpcallMH(short *shortArg1, short shortArg2, short (*upcallMH)(short *, short))
{
	short shortSum = (*upcallMH)(shortArg1, shortArg2);
	return shortSum;
}

/**
 * Add two shorts (the 1st one is assigned in native) by invoking an upcall method.
 *
 * @param shortArg2 the 2nd short
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
short
addShortAndShortFromNativePtrByUpcallMH(short shortArg2, short (*upcallMH)(short *, short))
{
	short shortArg1 = 456;
	short shortSum = (*upcallMH)(&shortArg1, shortArg2);
	return shortSum;
}

/**
 * Add two shorts (the 1st one dereferenced from a pointer) by invoking an upcall method
 * and return a pointer to the sum.
 *
 * @param shortArg1 a pointer to the 1st short
 * @param shortArg2 the 2nd short
 * @param upcallMH the function pointer to the upcall method
 * @return the pointer to the sum
 */
short *
addShortAndShortFromPtr_RetPtr_ByUpcallMH(short *shortArg1, short shortArg2, short *(*upcallMH)(short *, short))
{
	short *shortSum = (*upcallMH)(shortArg1, shortArg2);
	return shortSum;
}

/**
 * Add two ints by invoking an upcall method.
 *
 * @param intArg1 the 1st int
 * @param intArg2 the 2nd int
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
int
add2IntsByUpcallMH(int intArg1, int intArg2, int (*upcallMH)(int, int))
{
	int intSum = (*upcallMH)(intArg1, intArg2);
	return intSum;
}

/**
 * Add two ints (the 2nd one is dereferenced from a pointer)
 * by invoking an upcall method.
 *
 * @param intArg1 the 1st int
 * @param intArg2 a pointer to the 2nd int
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
int
addIntAndIntFromPointerByUpcallMH(int intArg1, int *intArg2,  int (*upcallMH)(int, int *))
{
	int intSum = (*upcallMH)(intArg1, intArg2);
	return intSum;
}

/**
 * Add two ints (the 2nd one is assigned in native) by invoking an upcall method.
 *
 * @param intArg1 the 1st int
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
int
addIntAndIntFromNativePtrByUpcallMH(int intArg1, int (*upcallMH)(int, int *))
{
	int intArg2 = 444444;
	int intSum = (*upcallMH)(intArg1, &intArg2);
	return intSum;
}

/**
 * Add two ints (the 1st one is dereferenced from a pointer) by invoking an upcall method
 * and return a pointer to the sum.
 *
 * @param intArg1 the 1st int
 * @param intArg2 a pointer to the 2nd int
 * @param upcallMH the function pointer to the upcall method
 * @return the pointer to the sum
 */
int *
addIntAndIntFromPtr_RetPtr_ByUpcallMH(int intArg1, int *intArg2, int *(*upcallMH)(int, int *))
{
	int *intSum = (*upcallMH)(intArg1, intArg2);
	return intSum;
}

/**
 * Add three ints by invoking an upcall method.
 *
 * @param intArg1 the 1st int
 * @param intArg2 the 2nd int
 * @param intArg3 the 3rd int
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
int
add3IntsByUpcallMH(int intArg1, int intArg2, int intArg3, int (*upcallMH)(int, int, int))
{
	int intSum = (*upcallMH)(intArg1, intArg2, intArg3);
	return intSum;
}

/**
 * Add an int and a char by invoking an upcall method.
 *
 * @param intArg an int
 * @param charArg a char
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
int
addIntAndCharByUpcallMH(int intArg, char charArg,  int (*upcallMH)(int, char))
{
	int sum = (*upcallMH)(intArg, charArg);
	return sum;
}

/**
 * Add two ints without return value by invoking an upcall method.
 *
 * @param intArg1 the 1st int
 * @param intArg2 the 2nd int
 * @param upcallMH the function pointer to the upcall method
 * @return void
 */
void
add2IntsReturnVoidByUpcallMH(int intArg1, int intArg2, int (*upcallMH)(int, int))
{
	(*upcallMH)(intArg1, intArg2);
}

/**
 * Add two longs by invoking an upcall method.
 *
 * @param longArg1 the 1st long
 * @param longArg2 the 2nd long
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
LONG
add2LongsByUpcallMH(LONG longArg1, LONG longArg2, LONG (*upcallMH)(LONG, LONG))
{
	LONG longSum = (*upcallMH)(longArg1, longArg2);
	return longSum;
}

/**
 * Add two longs (the 1st one dereferenced from a pointer)
 * by invoking an upcall method.
 *
 * @param longArg1 a pointer to the 1stlong
 * @param longArg2 the 2nd long
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
LONG
addLongAndLongFromPointerByUpcallMH(LONG *longArg1, LONG longArg2, LONG (*upcallMH)(LONG *, LONG))
{
	LONG longSum = (*upcallMH)(longArg1, longArg2);
	return longSum;
}

/**
 * Add two longs (the 1st one is assigned in native) by invoking an upcall method.
 *
 * @param longArg2 the 2nd long
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
LONG
addLongAndLongFromNativePtrByUpcallMH(LONG longArg2, LONG (*upcallMH)(LONG *, LONG))
{
	LONG longArg1 = 3333333333;
	LONG longSum = (*upcallMH)(&longArg1, longArg2);
	return longSum;
}

/**
 * Add two longs (the 1st one dereferenced from a pointer) by invoking an upcall method
 * and return a pointer to the sum.
 *
 * @param longArg1 a pointer to the 1st long
 * @param longArg2 the 2nd long
 * @param upcallMH the function pointer to the upcall method
 * @return the pointer to the sum
 */
LONG *
addLongAndLongFromPtr_RetPtr_ByUpcallMH(LONG *longArg1, LONG longArg2, LONG *(*upcallMH)(LONG *, LONG))
{
	LONG *longSum = (*upcallMH)(longArg1, longArg2);
	return longSum;
}

/**
 * Add two floats by invoking an upcall method.
 *
 * @param floatArg1 the 1st float
 * @param floatArg2 the 2nd float
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
float
add2FloatsByUpcallMH(float floatArg1, float floatArg2, float (*upcallMH)(float, float))
{
	float floatSum = (*upcallMH)(floatArg1, floatArg2);
	return floatSum;
}

/**
 * Add two floats (the 2nd one is dereferenced from a pointer)
 * by invoking an upcall method.
 *
 * @param floatArg1 the 1st float
 * @param floatArg2 a pointer to the 2nd float
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
float
addFloatAndFloatFromPointerByUpcallMH(float floatArg1, float *floatArg2, float (*upcallMH)(float, float *))
{
	float floatSum = (*upcallMH)(floatArg1, floatArg2);
	return floatSum;
}

/**
 * Add two floats (the 2nd one is assigned in native) by invoking an upcall method.
 *
 * @param floatArg1 the 1st float
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
float
addFloatAndFloatFromNativePtrByUpcallMH(float floatArg1, float (*upcallMH)(float, float *))
{
	float floatArg2 = 6.79F;
	float floatSum = (*upcallMH)(floatArg1, &floatArg2);
	return floatSum;
}

/**
 * Add two floats (the 2nd one is dereferenced from a pointer) by invoking an upcall method
 * and return a pointer to the sum.
 *
 * @param floatArg1 the 1st float
 * @param floatArg2 a pointer to the 2nd float
 * @param upcallMH the function pointer to the upcall method
 * @return the pointer to the sum
 */
float *
addFloatAndFloatFromPtr_RetPtr_ByUpcallMH(float floatArg1, float *floatArg2, float *(*upcallMH)(float, float *))
{
	float *floatSum = (*upcallMH)(floatArg1, floatArg2);
	return floatSum;
}

/**
 * Add two doubles by invoking an upcall method.
 *
 * @param doubleArg1 the 1st double
 * @param doubleArg2 the 2nd double
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
add2DoublesByUpcallMH(double doubleArg1, double doubleArg2, double (*upcallMH)(double, double))
{
	double doubleSum = (*upcallMH)(doubleArg1, doubleArg2);
	return doubleSum;
}

/**
 * Add two doubles (the 1st one dereferenced from a pointer)
 * by invoking an upcall method.
 *
 * @param doubleArg1 a pointer to the 1st double
 * @param doubleArg2 the 2nd double
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAndDoubleFromPointerByUpcallMH(double *doubleArg1, double doubleArg2, double (*upcallMH)(double *, double))
{
	double doubleSum = (*upcallMH)(doubleArg1, doubleArg2);
	return doubleSum;
}

/**
 * Add two doubles (the 1st one is assigned in native) by invoking an upcall method.
 *
 * @param doubleArg2 the 2nd double
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAndDoubleFromNativePtrByUpcallMH(double doubleArg2, double (*upcallMH)(double *, double))
{
	double doubleArg1 = 1159.748;
	double doubleSum = (*upcallMH)(&doubleArg1, doubleArg2);
	return doubleSum;
}

/**
 * Add two doubles (the 1st one dereferenced from a pointer) by invoking an upcall method
 * and return a pointer to the sum.
 *
 * @param doubleArg1 a pointer to the 1st double
 * @param doubleArg2 the 2nd double
 * @param upcallMH the function pointer to the upcall method
 * @return the pointer to the sum
 */
double *
addDoubleAndDoubleFromPtr_RetPtr_ByUpcallMH(double *doubleArg1, double doubleArg2, double *(*upcallMH)(double *, double))
{
	double *doubleSum = (*upcallMH)(doubleArg1, doubleArg2);
	return doubleSum;
}

/**
 * Add a boolean and two booleans of a struct with the XOR (^) operator
 * by invoking an upcall method.
 *
 * @param arg1 a boolean
 * @param arg2 a struct with two booleans
 * @param upcallMH the function pointer to the upcall method
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromStructWithXorByUpcallMH(bool arg1, stru_2_Bools arg2, bool (*upcallMH)(bool, stru_2_Bools))
{
	bool boolSum = (*upcallMH)(arg1, arg2);
	return boolSum;
}

/**
 * Add a boolean and 20 booleans of a struct with the XOR (^) operator
 * by invoking an upcall method.
 *
 * @param arg1 a boolean
 * @param arg2 a struct with 20 booleans
 * @param upcallMH the function pointer to the upcall method
 * @return the XOR result of booleans
 */
bool
addBoolAnd20BoolsFromStructWithXorByUpcallMH(bool arg1, stru_20_Bools arg2, bool (*upcallMH)(bool, stru_20_Bools))
{
	bool boolSum = (*upcallMH)(arg1, arg2);
	return boolSum;
}

/**
 * Add a boolean (dereferenced from a pointer) and all booleans of
 * a struct with the XOR (^) operator by invoking an upcall method.
 *
 * @param arg1 a pointer to boolean
 * @param arg2 a struct with two booleans
 * @param upcallMH the function pointer to the upcall method
 * @return the XOR result of booleans
 */
bool
addBoolFromPointerAndBoolsFromStructWithXorByUpcallMH(bool *arg1, stru_2_Bools arg2, bool (*upcallMH)(bool *, stru_2_Bools))
{
	bool boolSum = (*upcallMH)(arg1, arg2);
	return boolSum;
}

/**
 * Get a pointer to boolean by adding a boolean (dereferenced from a pointer) and all booleans
 * of a struct with the XOR (^) operator by invoking an upcall method.
 *
 * @param arg1 a pointer to boolean
 * @param arg2 a struct with two booleans
 * @param upcallMH the function pointer to the upcall method
 * @return a pointer to the XOR result of booleans
 */
bool *
addBoolFromPointerAndBoolsFromStructWithXor_returnBoolPointerByUpcallMH(bool *arg1, stru_2_Bools arg2, bool * (*upcallMH)(bool *, stru_2_Bools))
{
	arg1 = (*upcallMH)(arg1, arg2);
	return arg1;
}

/**
 * Add a boolean and two booleans of a struct (dereferenced from a pointer)
 * with the XOR (^) operator by invoking an upcall method.
 *
 * @param arg1 a boolean
 * @param arg2 a pointer to struct with two booleans
 * @param upcallMH the function pointer to the upcall method
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromStructPointerWithXorByUpcallMH(bool arg1, stru_2_Bools *arg2, bool (*upcallMH)(bool, stru_2_Bools *))
{
	bool boolSum = (*upcallMH)(arg1, arg2);
	return boolSum;
}

/**
 * Add a boolean and all booleans of a struct with a nested struct and a boolean
 * with the XOR (^) operator by invoking an upcall method.
 *
 * @param arg1 a boolean
 * @param arg2 a struct with a nested struct and a boolean
 * @param upcallMH the function pointer to the upcall method
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromNestedStructWithXorByUpcallMH(bool arg1, stru_NestedStruct_Bool arg2, bool (*upcallMH)(bool, stru_NestedStruct_Bool))
{
	bool boolSum = (*upcallMH)(arg1, arg2);
	return boolSum;
}

/**
 * Add a boolean and all booleans of a struct with a boolean and a nested struct (in reverse order)
 * with the XOR (^) operator by invoking an upcall method.
 *
 * @param arg1 a boolean
 * @param arg2 a struct with a boolean and a nested struct
 * @param upcallMH the function pointer to the upcall method
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromNestedStructWithXor_reverseOrderByUpcallMH(bool arg1, stru_Bool_NestedStruct arg2, bool (*upcallMH)(bool, stru_Bool_NestedStruct))
{
	bool boolSum = (*upcallMH)(arg1, arg2);
	return boolSum;
}

/**
 * Add a boolean and all booleans of a struct with a nested array and a boolean
 * with the XOR (^) operator by invoking an upcall method.
 *
 * @param arg1 a boolean
 * @param arg2 a struct with a nested array and a boolean
 * @param upcallMH the function pointer to the upcall method
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromStructWithNestedBoolArrayByUpcallMH(bool arg1, stru_NestedBoolArray_Bool arg2, bool (*upcallMH)(bool, stru_NestedBoolArray_Bool))
{
	bool boolSum = (*upcallMH)(arg1, arg2);
	return boolSum;
}

/**
 * Add a boolean and all booleans of a struct with a boolean and a nested array (in reverse order)
 * with the XOR (^) operator by invoking an upcall method.
 *
 * @param arg1 a boolean
 * @param arg2 a struct with a boolean and a nested array
 * @param upcallMH the function pointer to the upcall method
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromStructWithNestedBoolArray_reverseOrderByUpcallMH(bool arg1, stru_Bool_NestedBoolArray arg2, bool (*upcallMH)(bool, stru_Bool_NestedBoolArray))
{
	bool boolSum = (*upcallMH)(arg1, arg2);
	return boolSum;
}

/**
 * Add a boolean and all booleans of a struct with a nested struct array and a boolean
 * with the XOR (^) operator by invoking an upcall method.
 *
 * @param arg1 a boolean
 * @param arg2 a struct with a nested struct array and a boolean
 * @param upcallMH the function pointer to the upcall method
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromStructWithNestedStructArrayByUpcallMH(bool arg1, stru_NestedStruArray_Bool arg2, bool (*upcallMH)(bool, stru_NestedStruArray_Bool))
{
	bool boolSum = (*upcallMH)(arg1, arg2);
	return boolSum;
}

/**
 * Add a boolean and all booleans of a struct with a boolean and a nested struct array
 * (in reverse order) with the XOR (^) operator by invoking an upcall method.
 *
 * @param arg1 a boolean
 * @param arg2 a struct with a boolean and a nested struct array
 * @param upcallMH the function pointer to the upcall method
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromStructWithNestedStructArray_reverseOrderByUpcallMH(bool arg1, stru_Bool_NestedStruArray arg2, bool (*upcallMH)(bool, stru_Bool_NestedStruArray))
{
	bool boolSum = (*upcallMH)(arg1, arg2);
	return boolSum;
}

/**
 * Get a new struct by adding each boolean of two structs
 * with the XOR (^) operator by invoking an upcall method.
 *
 * @param arg1 the 1st struct with two booleans
 * @param arg2 the 2nd struct with two booleans
 * @param upcallMH the function pointer to the upcall method
 * @return a struct with two booleans
 */
stru_2_Bools
add2BoolStructsWithXor_returnStructByUpcallMH(stru_2_Bools arg1, stru_2_Bools arg2, stru_2_Bools (*upcallMH)(stru_2_Bools, stru_2_Bools))
{
	stru_2_Bools boolStruct = (*upcallMH)(arg1, arg2);
	return boolStruct;
}

/**
 * Get a pointer to struct by adding each boolean of two structs
 * with the XOR (^) operator by invoking an upcall method.
 *
 * @param arg1 a pointer to the 1st struct with two booleans
 * @param arg2 the 2nd struct with two booleans
 * @param upcallMH the function pointer to the upcall method
 * @return a pointer to struct with two booleans
 */
stru_2_Bools *
add2BoolStructsWithXor_returnStructPointerByUpcallMH(stru_2_Bools *arg1, stru_2_Bools arg2, stru_2_Bools * (*upcallMH)(stru_2_Bools *, stru_2_Bools))
{
	arg1 = (*upcallMH)(arg1, arg2);
	return arg1;
}

/**
 * Get a new struct by adding each boolean of two structs with
 * three booleans by invoking an upcall method.
 *
 * @param arg1 the 1st struct with three booleans
 * @param arg2 the 2nd struct with three booleans
 * @param upcallMH the function pointer to the upcall method
 * @return a struct with three booleans
 */
stru_3_Bools
add3BoolStructsWithXor_returnStructByUpcallMH(stru_3_Bools arg1, stru_3_Bools arg2, stru_3_Bools (*upcallMH)(stru_3_Bools, stru_3_Bools))
{
	stru_3_Bools boolStruct = (*upcallMH)(arg1, arg2);
	return boolStruct;
}

/**
 * Add a byte and two bytes of a struct by invoking an upcall method.
 *
 * @param arg1 a byte
 * @param arg2 a struct with two bytes
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
char
addByteAndBytesFromStructByUpcallMH(char arg1, stru_2_Bytes arg2, char (*upcallMH)(char, stru_2_Bytes))
{
	char byteSum = (*upcallMH)(arg1, arg2);
	return byteSum;
}

/**
 * Add a byte and 20 bytes of a struct by invoking an upcall method.
 *
 * @param arg1 a byte
 * @param arg2 a struct with 20 bytes
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
char
addByteAnd20BytesFromStructByUpcallMH(char arg1, stru_20_Bytes arg2, char (*upcallMH)(char, stru_20_Bytes))
{
	char byteSum = (*upcallMH)(arg1, arg2);
	return byteSum;
}

/**
 * Add a byte (dereferenced from a pointer) and two bytes
 * of a struct by invoking an upcall method.
 *
 * @param arg1 a pointer to byte
 * @param arg2 a struct with two bytes
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
char
addByteFromPointerAndBytesFromStructByUpcallMH(char *arg1, stru_2_Bytes arg2, char (*upcallMH)(char *, stru_2_Bytes))
{
	char byteSum = (*upcallMH)(arg1, arg2);
	return byteSum;
}

/**
 * Get a pointer to byte by adding a byte (dereferenced from a pointer)
 * and two bytes of a struct by invoking an upcall method.
 *
 * @param arg1 a pointer to byte
 * @param arg2 a struct with two bytes
 * @param upcallMH the function pointer to the upcall method
 * @return a pointer to the sum
 */
char *
addByteFromPointerAndBytesFromStruct_returnBytePointerByUpcallMH(char *arg1, stru_2_Bytes arg2, char * (*upcallMH)(char *, stru_2_Bytes))
{
	arg1 = (*upcallMH)(arg1, arg2);
	return arg1;
}

/**
 * Add a byte and two bytes of a struct (dereferenced from a pointer)
 * by invoking an upcall method.
 *
 * @param arg1 a byte
 * @param arg2 a pointer to struct with two bytes
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
char
addByteAndBytesFromStructPointerByUpcallMH(char arg1, stru_2_Bytes *arg2, char (*upcallMH)(char, stru_2_Bytes *))
{
	char byteSum = (*upcallMH)(arg1, arg2);
	return byteSum;
}

/**
 * Add a byte and all bytes of a struct with a nested struct
 * and a byte by invoking an upcall method.
 *
 * @param arg1 a byte
 * @param arg2 a struct with a nested struct and a byte
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
char
addByteAndBytesFromNestedStructByUpcallMH(char arg1, stru_NestedStruct_Byte arg2, char (*upcallMH)(char, stru_NestedStruct_Byte))
{
	char byteSum = (*upcallMH)(arg1, arg2);
	return byteSum;
}

/**
 * Add a byte and all bytes of a struct with a byte and a nested struct
 * (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a byte
 * @param arg2 a struct with a byte and a nested struct
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
char
addByteAndBytesFromNestedStruct_reverseOrderByUpcallMH(char arg1, stru_Byte_NestedStruct arg2, char (*upcallMH)(char, stru_Byte_NestedStruct))
{
	char byteSum = (*upcallMH)(arg1, arg2);
	return byteSum;
}

/**
 * Add a byte and all byte elements of a struct with a nested byte array
 * and a byte by invoking an upcall method.
 *
 * @param arg1 a byte
 * @param arg2 a struct with a nested byte array and a byte
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
char
addByteAndBytesFromStructWithNestedByteArrayByUpcallMH(char arg1, stru_NestedByteArray_Byte arg2, char (*upcallMH)(char, stru_NestedByteArray_Byte))
{
	char byteSum = (*upcallMH)(arg1, arg2);
	return byteSum;
}

/**
 * Add a byte and all byte elements of a struct with a byte and a nested byte array
 * (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a byte
 * @param arg2 a struct with a byte and a nested byte array
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
char
addByteAndBytesFromStructWithNestedByteArray_reverseOrderByUpcallMH(char arg1, stru_Byte_NestedByteArray arg2, char (*upcallMH)(char, stru_Byte_NestedByteArray))
{
	char byteSum = (*upcallMH)(arg1, arg2);
	return byteSum;
}

/**
 * Add a byte and all byte elements of a struct with a nested struct array
 * and a byte by invoking an upcall method.
 *
 * @param arg1 a byte
 * @param arg2 a struct with a nested struct array and a byte
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
char
addByteAndBytesFromStructWithNestedStructArrayByUpcallMH(char arg1, stru_NestedStruArray_Byte arg2, char (*upcallMH)(char, stru_NestedStruArray_Byte))
{
	char byteSum = (*upcallMH)(arg1, arg2);
	return byteSum;
}

/**
 * Add a byte and all byte elements of a struct with a byte and a nested
 * struct array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a byte
 * @param arg2 a struct with a byte and a nested byte array
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
char
addByteAndBytesFromStructWithNestedStructArray_reverseOrderByUpcallMH(char arg1, stru_Byte_NestedStruArray arg2, char (*upcallMH)(char, stru_Byte_NestedStruArray))
{
	char byteSum = (*upcallMH)(arg1, arg2);
	return byteSum;
}

/**
 * Get a new struct by adding each byte element of two structs with
 * two byte elements by invoking an upcall method.
 *
 * @param arg1 the 1st struct with one byte
 * @param arg2 the 2nd struct with one byte
 * @param upcallMH the function pointer to the upcall method
 * @return a struct with one byte
 */
stru_Byte
add1ByteStructs_returnStructByUpcallMH(stru_Byte arg1, stru_Byte arg2, stru_Byte (*upcallMH)(stru_Byte, stru_Byte))
{
	stru_Byte byteStruct = (*upcallMH)(arg1, arg2);
	return byteStruct;
}

/**
 * Get a new struct by adding each byte element of two structs with
 * two byte elements by invoking an upcall method.
 *
 * @param arg1 the 1st struct with two bytes
 * @param arg2 the 2nd struct with two bytes
 * @param upcallMH the function pointer to the upcall method
 * @return a struct with two bytes
 */
stru_2_Bytes
add2ByteStructs_returnStructByUpcallMH(stru_2_Bytes arg1, stru_2_Bytes arg2, stru_2_Bytes (*upcallMH)(stru_2_Bytes, stru_2_Bytes))
{
	stru_2_Bytes byteStruct = (*upcallMH)(arg1, arg2);
	return byteStruct;
}

/**
 * Get a pointer to struct by adding each byte element of two structs
 * with two byte elements by invoking an upcall method.
 *
 * @param arg1 a pointer to the 1st struct with two bytes
 * @param arg2 the 2nd struct with two bytes
 * @param upcallMH the function pointer to the upcall method
 * @return a pointer to struct with two bytes
 */
stru_2_Bytes *
add2ByteStructs_returnStructPointerByUpcallMH(stru_2_Bytes *arg1, stru_2_Bytes arg2, stru_2_Bytes * (*upcallMH)(stru_2_Bytes *, stru_2_Bytes))
{
	arg1 = (*upcallMH)(arg1, arg2);
	return arg1;
}

/**
 * Get a new struct by adding each byte element of two structs with
 * three byte elements by invoking an upcall method.
 *
 * @param arg1 the 1st struct with three bytes
 * @param arg2 the 2nd struct with three bytes
 * @param upcallMH the function pointer to the upcall method
 * @return a struct with three bytes
 */
stru_3_Bytes
add3ByteStructs_returnStructByUpcallMH(stru_3_Bytes arg1, stru_3_Bytes arg2, stru_3_Bytes (*upcallMH)(stru_3_Bytes, stru_3_Bytes))
{
	stru_3_Bytes byteStruct = (*upcallMH)(arg1, arg2);
	return byteStruct;
}

/**
 * Generate a new char by adding a char and two chars of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a char
 * @param arg2 a struct with two chars
 * @param upcallMH the function pointer to the upcall method
 * @return a new char
 */
short
addCharAndCharsFromStructByUpcallMH(short arg1, stru_2_Chars arg2, short (*upcallMH)(short, stru_2_Chars))
{
	short result = (*upcallMH)(arg1, arg2);
	return result;
}

/**
 * Generate a new char by adding a char and 10 chars of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a char
 * @param arg2 a struct with 10 chars
 * @param upcallMH the function pointer to the upcall method
 * @return a new char
 */
short
addCharAnd10CharsFromStructByUpcallMH(short arg1, stru_10_Chars arg2, short (*upcallMH)(short, stru_10_Chars))
{
	short result = (*upcallMH)(arg1, arg2);
	return result;
}


/**
 * Generate a new char by adding a char (dereferenced from a pointer)
 * and two chars of a struct by invoking an upcall method.
 *
 * @param arg1 a pointer to char
 * @param arg2 a struct with two chars
 * @param upcallMH the function pointer to the upcall method
 * @return a new char
 */
short
addCharFromPointerAndCharsFromStructByUpcallMH(short *arg1, stru_2_Chars arg2, short (*upcallMH)(short *, stru_2_Chars))
{
	short result = (*upcallMH)(arg1, arg2);
	return result;
}

/**
 * Get a pointer to char by adding a char (dereferenced from a pointer)
 * and two chars of a struct by invoking an upcall method.
 *
 * @param arg1 a pointer to char
 * @param arg2 a struct with two chars
 * @param upcallMH the function pointer to the upcall method
 * @return a pointer to a new char
 */
short *
addCharFromPointerAndCharsFromStruct_returnCharPointerByUpcallMH(short *arg1, stru_2_Chars arg2, short * (*upcallMH)(short *, stru_2_Chars))
{
	arg1 = (*upcallMH)(arg1, arg2);
	return arg1;
}

/**
 * Generate a new char by adding a char and two chars of struct (dereferenced from a pointer)
 * by invoking an upcall method.
 *
 * @param arg1 a char
 * @param arg2 a pointer to struct with two chars
 * @param upcallMH the function pointer to the upcall method
 * @return a new char
 */
short
addCharAndCharsFromStructPointerByUpcallMH(short arg1, stru_2_Chars *arg2, short (*upcallMH)(short, stru_2_Chars *))
{
	short result = (*upcallMH)(arg1, arg2);
	return result;
}

/**
 * Generate a new char by adding a char and all char elements of a struct
 * with a nested struct and a char by invoking an upcall method.
 *
 * @param arg1 a char
 * @param arg2 a struct with a nested struct
 * @param upcallMH the function pointer to the upcall method
 * @return a new char
 */
short
addCharAndCharsFromNestedStructByUpcallMH(short arg1, stru_NestedStruct_Char arg2, short (*upcallMH)(short, stru_NestedStruct_Char))
{
	short result = (*upcallMH)(arg1, arg2);
	return result;
}

/**
 * Generate a new char by adding a char and all char elements of a struct with a char
 * and a nested struct (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a char
 * @param arg2 a struct with a char and a nested struct
 * @param upcallMH the function pointer to the upcall method
 * @return a new char
 */
short
addCharAndCharsFromNestedStruct_reverseOrderByUpcallMH(short arg1, stru_Char_NestedStruct arg2, short (*upcallMH)(short, stru_Char_NestedStruct))
{
	short result = (*upcallMH)(arg1, arg2);
	return result;
}

/**
 * Generate a new char by adding a char and all char elements of a struct with
 * a nested char array and a char by invoking an upcall method.
 *
 * @param arg1 a char
 * @param arg2 a struct with a nested char array and a char
 * @param upcallMH the function pointer to the upcall method
 * @return a new char
 */
short
addCharAndCharsFromStructWithNestedCharArrayByUpcallMH(short arg1, stru_NestedCharArray_Char arg2, short (*upcallMH)(short, stru_NestedCharArray_Char))
{
	short result = (*upcallMH)(arg1, arg2);
	return result;
}

/**
 * Generate a new char by adding a char and all char elements of a struct with a char
 * and a nested char array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a char
 * @param arg2 a struct with a char and a nested char array
 * @param upcallMH the function pointer to the upcall method
 * @return a new char
 */
short
addCharAndCharsFromStructWithNestedCharArray_reverseOrderByUpcallMH(short arg1, stru_Char_NestedCharArray arg2, short (*upcallMH)(short, stru_Char_NestedCharArray))
{
	short result = (*upcallMH)(arg1, arg2);
	return result;
}

/**
 * Generate a new char by adding a char and all char elements of a struct with
 * a nested struct array and a char by invoking an upcall method.
 *
 * @param arg1 a char
 * @param arg2 a struct with a nested char array and a char
 * @param upcallMH the function pointer to the upcall method
 * @return a new char
 */
short
addCharAndCharsFromStructWithNestedStructArrayByUpcallMH(short arg1, stru_NestedStruArray_Char arg2, short (*upcallMH)(short, stru_NestedStruArray_Char))
{
	short result = (*upcallMH)(arg1, arg2);
	return result;
}

/**
 * Generate a new char by adding a char and all char elements of a struct with a char
 * and a nested struct array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a char
 * @param arg2 a struct with a char and a nested char array
 * @param upcallMH the function pointer to the upcall method
 * @return a new char
 */
short
addCharAndCharsFromStructWithNestedStructArray_reverseOrderByUpcallMH(short arg1, stru_Char_NestedStruArray arg2, short (*upcallMH)(short, stru_Char_NestedStruArray))
{
	short result = (*upcallMH)(arg1, arg2);
	return result;
}

/**
 * Create a new struct by adding each char element of two structs
 * by invoking an upcall method.
 *
 * @param arg1 the 1st struct with two chars
 * @param arg2 the 2nd struct with two chars
 * @param upcallMH the function pointer to the upcall method
 * @return a new struct of with two chars
 */
stru_2_Chars
add2CharStructs_returnStructByUpcallMH(stru_2_Chars arg1, stru_2_Chars arg2, stru_2_Chars (*upcallMH)(stru_2_Chars, stru_2_Chars))
{
	stru_2_Chars charStruct = (*upcallMH)(arg1, arg2);
	return charStruct;
}

/**
 * Get a pointer to a struct by adding each element of two structs
 * by invoking an upcall method.
 *
 * @param arg1 a pointer to the 1st struct with two chars
 * @param arg2 the 2nd struct with two chars
 * @param upcallMH the function pointer to the upcall method
 * @return a pointer to a struct of with two chars
 */
stru_2_Chars *
add2CharStructs_returnStructPointerByUpcallMH(stru_2_Chars *arg1, stru_2_Chars arg2, stru_2_Chars * (*upcallMH)(stru_2_Chars *, stru_2_Chars))
{
	arg1 = (*upcallMH)(arg1, arg2);
	return arg1;
}

/**
 * Create a new struct by adding each char element of two structs
 * with three chars by invoking an upcall method.
 *
 * @param arg1 the 1st struct with three chars
 * @param arg2 the 2nd struct with three chars
 * @param upcallMH the function pointer to the upcall method
 * @return a new struct of with three chars
 */
stru_3_Chars
add3CharStructs_returnStructByUpcallMH(stru_3_Chars arg1, stru_3_Chars arg2, stru_3_Chars (*upcallMH)(stru_3_Chars, stru_3_Chars))
{
	stru_3_Chars charStruct = (*upcallMH)(arg1, arg2);
	return charStruct;
}

/**
 * Add a short and two shorts of a struct by invoking an upcall method.
 *
 * @param arg1 a short
 * @param arg2 a struct with two shorts
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
short
addShortAndShortsFromStructByUpcallMH(short arg1, stru_2_Shorts arg2, short (*upcallMH)(short, stru_2_Shorts))
{
	short shortSum = (*upcallMH)(arg1, arg2);
	return shortSum;
}

/**
 * Add a short and 10 shorts of a struct by invoking an upcall method.
 *
 * @param arg1 a short
 * @param arg2 a struct with 10 shorts
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
short
addShortAnd10ShortsFromStructByUpcallMH(short arg1, stru_10_Shorts arg2, short (*upcallMH)(short, stru_10_Shorts))
{
	short shortSum = (*upcallMH)(arg1, arg2);
	return shortSum;
}

/**
 * Add a short (dereferenced from a pointer) and two shorts of
 * a struct by invoking an upcall method.
 *
 * @param arg1 a pointer to short
 * @param arg2 a struct with two shorts
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
short
addShortFromPointerAndShortsFromStructByUpcallMH(short *arg1, stru_2_Shorts arg2, short (*upcallMH)(short *, stru_2_Shorts))
{
	short shortSum = (*upcallMH)(arg1, arg2);
	return shortSum;
}

/**
 * Add a short (dereferenced from a pointer) and two shorts
 * of a struct by invoking an upcall method.
 *
 * @param arg1 a pointer to short
 * @param arg2 a struct with two shorts
 * @param upcallMH the function pointer to the upcall method
 * @return a pointer to the sum
 */
short *
addShortFromPointerAndShortsFromStruct_returnShortPointerByUpcallMH(short *arg1, stru_2_Shorts arg2, short * (*upcallMH)(short *, stru_2_Shorts))
{
	arg1 = (*upcallMH)(arg1, arg2);
	return arg1;
}

/**
 * Add a short and two shorts of a struct (dereferenced from a pointer)
 * by invoking an upcall method.
 *
 * @param arg1 a short
 * @param arg2 a pointer to struct with two shorts
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
short
addShortAndShortsFromStructPointerByUpcallMH(short arg1, stru_2_Shorts *arg2, short (*upcallMH)(short, stru_2_Shorts *))
{
	short shortSum = (*upcallMH)(arg1, arg2);
	return shortSum;
}

/**
 * Add a short and all short elements of a struct with a nested struct
 * and a short by invoking an upcall method.
 *
 * @param arg1 a short
 * @param arg2 a struct with a nested struct and a short
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
short
addShortAndShortsFromNestedStructByUpcallMH(short arg1, stru_NestedStruct_Short arg2, short (*upcallMH)(short, stru_NestedStruct_Short))
{
	short shortSum = (*upcallMH)(arg1, arg2);
	return shortSum;
}

/**
 * Add a short and all short elements of a struct with a short and a nested struct
 * (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a short
 * @param arg2 a struct with a short and a nested struct
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
short
addShortAndShortsFromNestedStruct_reverseOrderByUpcallMH(short arg1, stru_Short_NestedStruct arg2, short (*upcallMH)(short, stru_Short_NestedStruct))
{
	short shortSum = (*upcallMH)(arg1, arg2);
	return shortSum;
}

/**
 * Add a short and all short elements of a struct with a nested short array
 * and a short by invoking an upcall method.
 *
 * @param arg1 a short
 * @param arg2 a struct with a nested short array and a short
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
short
addShortAndShortsFromStructWithNestedShortArrayByUpcallMH(short arg1, stru_NestedShortArray_Short arg2, short (*upcallMH)(short, stru_NestedShortArray_Short))
{
	short shortSum = (*upcallMH)(arg1, arg2);
	return shortSum;
}

/**
 * Add a short and all short elements of a struct with a short and a nested
 * short array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a short
 * @param arg2 a struct with a short and a nested short array
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
short
addShortAndShortsFromStructWithNestedShortArray_reverseOrderByUpcallMH(short arg1, stru_Short_NestedShortArray arg2, short (*upcallMH)(short, stru_Short_NestedShortArray))
{
	short shortSum = (*upcallMH)(arg1, arg2);
	return shortSum;
}

/**
 * Add a short and all short elements of a struct with a nested struct
 * array and a short by invoking an upcall method.
 *
 * @param arg1 a short
 * @param arg2 a struct with a nested short array and a short
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
short
addShortAndShortsFromStructWithNestedStructArrayByUpcallMH(short arg1, stru_NestedStruArray_Short arg2, short (*upcallMH)(short, stru_NestedStruArray_Short))
{
	short shortSum = (*upcallMH)(arg1, arg2);
	return shortSum;
}

/**
 * Add a short and all short elements of a struct with a short and a nested
 * struct array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a short
 * @param arg2 a struct with a short and a nested short array
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
short
addShortAndShortsFromStructWithNestedStructArray_reverseOrderByUpcallMH(short arg1, stru_Short_NestedStruArray arg2, short (*upcallMH)(short, stru_Short_NestedStruArray))
{
	short shortSum = (*upcallMH)(arg1, arg2);
	return shortSum;
}

/**
 * Get a new struct by adding each short element of two structs
 * with two short elements by invoking an upcall method.
 *
 * @param arg1 the 1st struct with two shorts
 * @param arg2 the 2nd struct with two shorts
 * @param upcallMH the function pointer to the upcall method
 * @return a struct with two shorts
 */
stru_2_Shorts
add2ShortStructs_returnStructByUpcallMH(stru_2_Shorts arg1, stru_2_Shorts arg2, stru_2_Shorts (*upcallMH)(stru_2_Shorts, stru_2_Shorts))
{
	stru_2_Shorts shortStruct = (*upcallMH)(arg1, arg2);
	return shortStruct;
}

/**
 * Get a pointer to struct by adding each short element of two structs
 * with two short elements by invoking an upcall method.
 *
 * @param arg1 a pointer to the 1st struct with two shorts
 * @param arg2 the 2nd struct with two shorts
 * @param upcallMH the function pointer to the upcall method
 * @return a pointer to struct with two shorts
 */
stru_2_Shorts *
add2ShortStructs_returnStructPointerByUpcallMH(stru_2_Shorts *arg1, stru_2_Shorts arg2, stru_2_Shorts * (*upcallMH)(stru_2_Shorts *, stru_2_Shorts))
{
	arg1 = (*upcallMH)(arg1, arg2);
	return arg1;
}

/**
 * Get a new struct by adding each short element of two structs with
 * three short elements by invoking an upcall method.
 *
 * @param arg1 the 1st struct with three shorts
 * @param arg2 the 2nd struct with three shorts
 * @param upcallMH the function pointer to the upcall method
 * @return a struct with three shorts
 */
stru_3_Shorts
add3ShortStructs_returnStructByUpcallMH(stru_3_Shorts arg1, stru_3_Shorts arg2, stru_3_Shorts (*upcallMH)(stru_3_Shorts, stru_3_Shorts))
{
	stru_3_Shorts shortStruct = (*upcallMH)(arg1, arg2);
	return shortStruct;
}

/**
 * Add an int and two ints of a struct
 * by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a struct with two ints
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
int
addIntAndIntsFromStructByUpcallMH(int arg1, stru_2_Ints arg2, int (*upcallMH)(int, stru_2_Ints))
{
	int intSum = (*upcallMH)(arg1, arg2);
	return intSum;
}

/**
 * Add an int and 5 ints of a struct
 * by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a struct with 5 ints
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
int
addIntAnd5IntsFromStructByUpcallMH(int arg1, stru_5_Ints arg2, int (*upcallMH)(int, stru_5_Ints))
{
	int intSum = (*upcallMH)(arg1, arg2);
	return intSum;
}

/**
 * Add an int (dereferenced from a pointer) and two ints
 * of a struct by invoking an upcall method.
 *
 * @param arg1 a pointer to int
 * @param arg2 a struct with two ints
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
int
addIntFromPointerAndIntsFromStructByUpcallMH(int *arg1, stru_2_Ints arg2,  int (*upcallMH)(int *, stru_2_Ints))
{
	int intSum = (*upcallMH)(arg1, arg2);
	return intSum;
}

/**
 * Add an int (dereferenced from a pointer) and two ints
 * of a struct by invoking an upcall method.
 *
 * @param arg1 a pointer to int
 * @param arg2 a struct with two ints
 * @param upcallMH the function pointer to the upcall method
 * @return a pointer to the sum
 */
int *
addIntFromPointerAndIntsFromStruct_returnIntPointerByUpcallMH(int *arg1, stru_2_Ints arg2, int *(*upcallMH)(int *, stru_2_Ints))
{
	arg1 = (*upcallMH)(arg1, arg2);
	return arg1;
}

/**
 * Add an int and two ints of a struct (dereferenced from a pointer)
 * by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a pointer to struct with two ints
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
int
addIntAndIntsFromStructPointerByUpcallMH(int arg1, stru_2_Ints *arg2, int (*upcallMH)(int, stru_2_Ints *))
{
	int intSum = (*upcallMH)(arg1, arg2);
	return intSum;
}

/**
 * Add an int and all int elements of a struct with a nested struct
 * and an int by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a struct with a nested struct and an int
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
int
addIntAndIntsFromNestedStructByUpcallMH(int arg1, stru_NestedStruct_Int arg2, int (*upcallMH)(int, stru_NestedStruct_Int))
{
	int intSum = (*upcallMH)(arg1, arg2);
	return intSum;
}

/**
 * Add an int and all int elements of a struct with an int and
 * a nested struct (in reverse order) by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a struct with an int and a nested struct
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
int
addIntAndIntsFromNestedStruct_reverseOrderByUpcallMH(int arg1, stru_Int_NestedStruct arg2, int (*upcallMH)(int, stru_Int_NestedStruct))
{
	int intSum = (*upcallMH)(arg1, arg2);
	return intSum;
}

/**
 * Add an int and all int elements of a struct with a nested int array
 * and an int by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a struct with a nested int array and an int
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
int
addIntAndIntsFromStructWithNestedIntArrayByUpcallMH(int arg1, stru_NestedIntArray_Int arg2, int (*upcallMH)(int, stru_NestedIntArray_Int))
{
	int intSum = (*upcallMH)(arg1, arg2);
	return intSum;
}

/**
 * Add an int and all int elements of a struct with an int and a
 * nested int array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a struct with an int and a nested int array
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
int
addIntAndIntsFromStructWithNestedIntArray_reverseOrderByUpcallMH(int arg1, stru_Int_NestedIntArray arg2, int (*upcallMH)(int, stru_Int_NestedIntArray))
{
	int intSum = (*upcallMH)(arg1, arg2);
	return intSum;
}

/**
 * Add an int and all int elements of a struct with a nested struct array
 * and an int by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a struct with a nested int array and an int
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
int
addIntAndIntsFromStructWithNestedStructArrayByUpcallMH(int arg1, stru_NestedStruArray_Int arg2, int (*upcallMH)(int, stru_NestedStruArray_Int))
{
	int intSum = (*upcallMH)(arg1, arg2);
	return intSum;
}

/**
 * Add an int and all int elements of a struct with an int and a nested
 * struct array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a struct with an int and a nested int array
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
int
addIntAndIntsFromStructWithNestedStructArray_reverseOrderByUpcallMH(int arg1, stru_Int_NestedStruArray arg2, int (*upcallMH)(int, stru_Int_NestedStruArray))
{
	int intSum = (*upcallMH)(arg1, arg2);
	return intSum;
}

/**
 * Get a new struct by adding each int element of two structs
 * by invoking an upcall method.
 *
 * @param arg1 the 1st struct with two ints
 * @param arg2 the 2nd struct with two ints
 * @param upcallMH the function pointer to the upcall method
 * @return a struct with two ints
 */
stru_2_Ints
add2IntStructs_returnStructByUpcallMH(stru_2_Ints arg1, stru_2_Ints arg2, stru_2_Ints (*upcallMH)(stru_2_Ints, stru_2_Ints))
{
	stru_2_Ints intStruct = (*upcallMH)(arg1, arg2);
	return intStruct;
}

/**
 * Get a pointer to struct by adding each int element of two structs
 * by invoking an upcall method.
 *
 * @param arg1 a pointer to the 1st struct with two ints
 * @param arg2 the 2nd struct with two ints
 * @param upcallMH the function pointer to the upcall method
 * @return a pointer to struct with two ints
 */
stru_2_Ints *
add2IntStructs_returnStructPointerByUpcallMH(stru_2_Ints *arg1, stru_2_Ints arg2, stru_2_Ints * (*upcallMH)(stru_2_Ints *, stru_2_Ints))
{
	arg1 = (*upcallMH)(arg1, arg2);
	return arg1;
}

/**
 * Get a new struct by adding each int element of two structs
 * by invoking an upcall method.
 *
 * @param arg1 the 1st struct with three ints
 * @param arg2 the 2nd struct with three ints
 * @param upcallMH the function pointer to the upcall method
 * @return a struct with three ints
 */
stru_3_Ints
add3IntStructs_returnStructByUpcallMH(stru_3_Ints arg1, stru_3_Ints arg2, stru_3_Ints (*upcallMH)(stru_3_Ints, stru_3_Ints))
{
	stru_3_Ints intStruct = (*upcallMH)(arg1, arg2);
	return intStruct;
}

/**
 * Add a long and two longs of a struct by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a struct with two longs
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
LONG
addLongAndLongsFromStructByUpcallMH(LONG arg1, stru_2_Longs arg2, LONG (*upcallMH)(LONG, stru_2_Longs))
{
	LONG longSum = (*upcallMH)(arg1, arg2);
	return longSum;
}

/**
 * Add a long (dereferenced from a pointer) and two longs
 * of a struct by invoking an upcall method.
 *
 * @param arg1 a pointer to long
 * @param arg2 a struct with two longs
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
LONG
addLongFromPointerAndLongsFromStructByUpcallMH(LONG *arg1, stru_2_Longs arg2, LONG (*upcallMH)(LONG *, stru_2_Longs))
{
	LONG longSum = (*upcallMH)(arg1, arg2);
	return longSum;
}

/**
 * Add a long (dereferenced from a pointer) and two longs
 * of a struct by invoking an upcall method.
 *
 * @param arg1 a pointer to long
 * @param arg2 a struct with two longs
 * @param upcallMH the function pointer to the upcall method
 * @return a pointer to the sum
 */
LONG *
addLongFromPointerAndLongsFromStruct_returnLongPointerByUpcallMH(LONG *arg1, stru_2_Longs arg2, LONG * (*upcallMH)(LONG *, stru_2_Longs))
{
	arg1 = (*upcallMH)(arg1, arg2);
	return arg1;
}

/**
 * Add a long and two longs of a struct (dereferenced from a pointer)
 * by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a pointer to struct with two longs
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
LONG
addLongAndLongsFromStructPointerByUpcallMH(LONG arg1, stru_2_Longs *arg2, LONG (*upcallMH)(LONG, stru_2_Longs *))
{
	LONG longSum = (*upcallMH)(arg1, arg2);
	return longSum;
}

/**
 * Add a long and all long elements of a struct with a nested struct
 * and a long by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a struct with a nested struct and long
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
LONG
addLongAndLongsFromNestedStructByUpcallMH(LONG arg1, stru_NestedStruct_Long arg2, LONG (*upcallMH)(LONG, stru_NestedStruct_Long))
{
	LONG longSum = (*upcallMH)(arg1, arg2);
	return longSum;
}

/**
 * Add a long and all long elements of a struct with a long and a nested
 * struct (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a struct with a long and a nested struct
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
LONG
addLongAndLongsFromNestedStruct_reverseOrderByUpcallMH(LONG arg1, stru_Long_NestedStruct arg2, LONG (*upcallMH)(LONG, stru_Long_NestedStruct))
{
	LONG longSum = (*upcallMH)(arg1, arg2);
	return longSum;
}

/**
 * Add a long and all long elements of a struct with a nested long
 * array and a long by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a struct with a nested long array and a long
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
LONG
addLongAndLongsFromStructWithNestedLongArrayByUpcallMH(LONG arg1, stru_NestedLongArray_Long arg2, LONG (*upcallMH)(LONG, stru_NestedLongArray_Long))
{
	LONG longSum = (*upcallMH)(arg1, arg2);
	return longSum;
}

/**
 * Add a long and all long elements of a struct with a long and a nested
 * long array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a struct with a long and a nested long array
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
LONG
addLongAndLongsFromStructWithNestedLongArray_reverseOrderByUpcallMH(LONG arg1, stru_Long_NestedLongArray arg2, LONG (*upcallMH)(LONG, stru_Long_NestedLongArray))
{
	LONG longSum = (*upcallMH)(arg1, arg2);
	return longSum;
}

/**
 * Add a long and all long elements of a struct with a nested struct
 * array and a long by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a struct with a nested long array and a long
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
LONG
addLongAndLongsFromStructWithNestedStructArrayByUpcallMH(LONG arg1, stru_NestedStruArray_Long arg2, LONG (*upcallMH)(LONG, stru_NestedStruArray_Long))
{
	LONG longSum = (*upcallMH)(arg1, arg2);
	return longSum;
}

/**
 * Add a long and all long elements of a struct with a long and a nested
 * struct array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a struct with a long and a nested long array
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
LONG
addLongAndLongsFromStructWithNestedStructArray_reverseOrderByUpcallMH(LONG arg1, stru_Long_NestedStruArray arg2, LONG (*upcallMH)(LONG, stru_Long_NestedStruArray))
{
	LONG longSum = (*upcallMH)(arg1, arg2);
	return longSum;
}

/**
 * Get a new struct by adding each long element of two structs
 * by invoking an upcall method.
 *
 * @param arg1 the 1st struct with two longs
 * @param arg2 the 2nd struct with two longs
 * @param upcallMH the function pointer to the upcall method
 * @return a struct with two longs
 */
stru_2_Longs
add2LongStructs_returnStructByUpcallMH(stru_2_Longs arg1, stru_2_Longs arg2, stru_2_Longs (*upcallMH)(stru_2_Longs, stru_2_Longs))
{
	stru_2_Longs longStruct = (*upcallMH)(arg1, arg2);
	return longStruct;
}

/**
 * Get a pointer to struct by adding each long element of two structs
 * by invoking an upcall method.
 *
 * @param arg1 a pointer to the 1st struct with two longs
 * @param arg2 the 2nd struct with two longs
 * @param upcallMH the function pointer to the upcall method
 * @return a pointer to struct with two longs
 */
stru_2_Longs *
add2LongStructs_returnStructPointerByUpcallMH(stru_2_Longs *arg1, stru_2_Longs arg2, stru_2_Longs * (*upcallMH)(stru_2_Longs *, stru_2_Longs))
{
	arg1 = (*upcallMH)(arg1, arg2);
	return arg1;
}

/**
 * Get a new struct by adding each long element of two structs
 * by invoking an upcall method.
 *
 * @param arg1 the 1st struct with three longs
 * @param arg2 the 2nd struct with three longs
 * @param upcallMH the function pointer to the upcall method
 * @return a struct with three longs
 */
stru_3_Longs
add3LongStructs_returnStructByUpcallMH(stru_3_Longs arg1, stru_3_Longs arg2, stru_3_Longs (*upcallMH)(stru_3_Longs, stru_3_Longs))
{
	stru_3_Longs longStruct = (*upcallMH)(arg1, arg2);
	return longStruct;
}

/**
 * Add a float and two floats of a struct by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a struct with two floats
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
float
addFloatAndFloatsFromStructByUpcallMH(float arg1, stru_2_Floats arg2, float (*upcallMH)(float, stru_2_Floats))
{
	float floatSum = (*upcallMH)(arg1, arg2);
	return floatSum;
}

/**
 * Add a float and 5 floats of a struct by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a struct with 5 floats
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
float
addFloatAnd5FloatsFromStructByUpcallMH(float arg1, stru_5_Floats arg2, float (*upcallMH)(float, stru_5_Floats))
{
	float floatSum = (*upcallMH)(arg1, arg2);
	return floatSum;
}

/**
 * Add a float (dereferenced from a pointer) and two floats
 * of a struct by invoking an upcall method.
 *
 * @param arg1 a pointer to float
 * @param arg2 a struct with two floats
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
float
addFloatFromPointerAndFloatsFromStructByUpcallMH(float *arg1, stru_2_Floats arg2, float (*upcallMH)(float *, stru_2_Floats))
{
	float floatSum = (*upcallMH)(arg1, arg2);
	return floatSum;
}

/**
 * Add a float (dereferenced from a pointer) and two floats
 * of a struct by invoking an upcall method.
 *
 * @param arg1 a pointer to float
 * @param arg2 a struct with two floats
 * @param upcallMH the function pointer to the upcall method
 * @return a pointer to the sum
 */
float *
addFloatFromPointerAndFloatsFromStruct_returnFloatPointerByUpcallMH(float *arg1, stru_2_Floats arg2, float * (*upcallMH)(float *, stru_2_Floats))
{
	arg1 = (*upcallMH)(arg1, arg2);
	return arg1;
}

/**
 * Add a float and two floats of a struct (dereferenced from a pointer)
 * by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a pointer to struct with two floats
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
float
addFloatAndFloatsFromStructPointerByUpcallMH(float arg1, stru_2_Floats *arg2, float (*upcallMH)(float, stru_2_Floats *))
{
	float floatSum = (*upcallMH)(arg1, arg2);
	return floatSum;
}

/**
 * Add a float and all float elements of a struct with a nested
 * struct and a float by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a struct with a nested struct and a float
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
float
addFloatAndFloatsFromNestedStructByUpcallMH(float arg1, stru_NestedStruct_Float arg2, float (*upcallMH)(float, stru_NestedStruct_Float))
{
	float floatSum = (*upcallMH)(arg1, arg2);
	return floatSum;
}

/**
 * Add a float and all float elements of a struct with a float and a nested struct
 * (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a struct with a float and a nested struct
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
float
addFloatAndFloatsFromNestedStruct_reverseOrderByUpcallMH(float arg1, stru_Float_NestedStruct arg2, float (*upcallMH)(float, stru_Float_NestedStruct))
{
	float floatSum = (*upcallMH)(arg1, arg2);
	return floatSum;
}

/**
 * Add a float and all float elements of a struct with a nested
 * float array and a float by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a struct with a nested float array and a float
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
float
addFloatAndFloatsFromStructWithNestedFloatArrayByUpcallMH(float arg1, stru_NestedFloatArray_Float arg2, float (*upcallMH)(float, stru_NestedFloatArray_Float))
{
	float floatSum = (*upcallMH)(arg1, arg2);
	return floatSum;
}

/**
 * Add a float and all float elements of a struct with a float and a nested
 * float array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a struct with a float and a nested float array
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
float
addFloatAndFloatsFromStructWithNestedFloatArray_reverseOrderByUpcallMH(float arg1, stru_Float_NestedFloatArray arg2, float (*upcallMH)(float, stru_Float_NestedFloatArray))
{
	float floatSum = (*upcallMH)(arg1, arg2);
	return floatSum;
}

/**
 * Add a float and all float elements of a struct with a nested
 * struct array and a float by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a struct with a nested float array and a float
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
float
addFloatAndFloatsFromStructWithNestedStructArrayByUpcallMH(float arg1, stru_NestedStruArray_Float arg2, float (*upcallMH)(float, stru_NestedStruArray_Float))
{
	float floatSum = (*upcallMH)(arg1, arg2);
	return floatSum;
}

/**
 * Add a float and all float elements of a struct with a float and a nested
 * struct array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a struct with a float and a nested float array
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
float
addFloatAndFloatsFromStructWithNestedStructArray_reverseOrderByUpcallMH(float arg1, stru_Float_NestedStruArray arg2, float (*upcallMH)(float, stru_Float_NestedStruArray))
{
	float floatSum = (*upcallMH)(arg1, arg2);
	return floatSum;
}

/**
 * Create a new struct by adding each float element of two structs
 * by invoking an upcall method.
 *
 * @param arg1 the 1st struct with two floats
 * @param arg2 the 2nd struct with two floats
 * @param upcallMH the function pointer to the upcall method
 * @return a struct with two floats
 */
stru_2_Floats
add2FloatStructs_returnStructByUpcallMH(stru_2_Floats arg1, stru_2_Floats arg2, stru_2_Floats (*upcallMH)(stru_2_Floats, stru_2_Floats))
{
	stru_2_Floats floatStruct = (*upcallMH)(arg1, arg2);
	return floatStruct;
}

/**
 * Get a pointer to struct by adding each float element of two structs
 * by invoking an upcall method.
 *
 * @param arg1 a pointer to the 1st struct with two floats
 * @param arg2 the 2nd struct with two floats
 * @param upcallMH the function pointer to the upcall method
 * @return a pointer to struct with two floats
 */
stru_2_Floats *
add2FloatStructs_returnStructPointerByUpcallMH(stru_2_Floats *arg1, stru_2_Floats arg2, stru_2_Floats * (*upcallMH)(stru_2_Floats *, stru_2_Floats))
{
	arg1 = (*upcallMH)(arg1, arg2);
	return arg1;
}

/**
 * Create a new struct by adding each float element of two structs
 * by invoking an upcall method.
 *
 * @param arg1 the 1st struct with three floats
 * @param arg2 the 2nd struct with three floats
 * @param upcallMH the function pointer to the upcall method
 * @return a struct with three floats
 */
stru_3_Floats
add3FloatStructs_returnStructByUpcallMH(stru_3_Floats arg1, stru_3_Floats arg2, stru_3_Floats (*upcallMH)(stru_3_Floats, stru_3_Floats))
{
	stru_3_Floats floatStruct = (*upcallMH)(arg1, arg2);
	return floatStruct;
}

/**
 * Add a double and two doubles of a struct by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a struct with two doubles
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAndDoublesFromStructByUpcallMH(double arg1, stru_2_Doubles arg2, double (*upcallMH)(double, stru_2_Doubles))
{
	double doubleSum = (*upcallMH)(arg1, arg2);
	return doubleSum;
}

/**
 * Add a double (dereferenced from a pointer) and two doubles
 * of a struct by invoking an upcall method.
 *
 * @param arg1 a pointer to double
 * @param arg2 a struct with two doubles
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleFromPointerAndDoublesFromStructByUpcallMH(double *arg1, stru_2_Doubles arg2, double (*upcallMH)(double *, stru_2_Doubles))
{
	double doubleSum = (*upcallMH)(arg1, arg2);
	return doubleSum;
}

/**
 * Add a double (dereferenced from a pointer) and two doubles
 * of a struct by invoking an upcall method.
 *
 * @param arg1 a pointer to double
 * @param arg2 a struct with two doubles
 * @param upcallMH the function pointer to the upcall method
 * @return a pointer to the sum
 */
double *
addDoubleFromPointerAndDoublesFromStruct_returnDoublePointerByUpcallMH(double *arg1, stru_2_Doubles arg2, double * (*upcallMH)(double *, stru_2_Doubles))
{
	arg1 = (*upcallMH)(arg1, arg2);
	return arg1;
}

/**
 * Add a double and two doubles of a struct (dereferenced from a pointer)
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a pointer to struct with two doubles
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAndDoublesFromStructPointerByUpcallMH(double arg1, stru_2_Doubles *arg2, double (*upcallMH)(double, stru_2_Doubles *))
{
	double doubleSum = (*upcallMH)(arg1, arg2);
	return doubleSum;
}

/**
 * Add a double and all doubles of a struct with a nested struct
 * and a double by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a struct with a nested struct and a double
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAndDoublesFromNestedStructByUpcallMH(double arg1, stru_NestedStruct_Double arg2, double (*upcallMH)(double, stru_NestedStruct_Double))
{
	double doubleSum = (*upcallMH)(arg1, arg2);
	return doubleSum;
}

/**
 * Add a double and all doubles of a struct with a double and a nested struct
 * (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a struct with a double a nested struct
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAndDoublesFromNestedStruct_reverseOrderByUpcallMH(double arg1, stru_Double_NestedStruct arg2, double (*upcallMH)(double, stru_Double_NestedStruct))
{
	double doubleSum = (*upcallMH)(arg1, arg2);
	return doubleSum;
}

/**
 * Add a double and all double elements of a struct with a nested
 * double array and a double by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a struct with a nested double array and a double
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAndDoublesFromStructWithNestedDoubleArrayByUpcallMH(double arg1, stru_NestedDoubleArray_Double arg2, double (*upcallMH)(double, stru_NestedDoubleArray_Double))
{
	double doubleSum = (*upcallMH)(arg1, arg2);
	return doubleSum;
}

/**
 * Add a double and all double elements of a struct with a double and a nested
 * double array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a struct with a double and a nested double array
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAndDoublesFromStructWithNestedDoubleArray_reverseOrderByUpcallMH(double arg1, stru_Double_NestedDoubleArray arg2, double (*upcallMH)(double, stru_Double_NestedDoubleArray))
{
	double doubleSum = (*upcallMH)(arg1, arg2);
	return doubleSum;
}

/**
 * Add a double and all double elements of a struct with a nested struct array
 * and a double by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a struct with a nested double array and a double
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAndDoublesFromStructWithNestedStructArrayByUpcallMH(double arg1, stru_NestedStruArray_Double arg2, double (*upcallMH)(double, stru_NestedStruArray_Double))
{
	double doubleSum = (*upcallMH)(arg1, arg2);
	return doubleSum;
}

/**
 * Add a double and all double elements of a struct with a double and a nested
 * struct array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a struct with a double and a nested double array
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAndDoublesFromStructWithNestedStructArray_reverseOrderByUpcallMH(double arg1, stru_Double_NestedStruArray arg2, double (*upcallMH)(double, stru_Double_NestedStruArray))
{
	double doubleSum = (*upcallMH)(arg1, arg2);
	return doubleSum;
}

/**
 * Create a new struct by adding each double element of two structs
 * by invoking an upcall method.
 *
 * @param arg1 the 1st struct with two doubles
 * @param arg2 the 2nd struct with two doubles
 * @param upcallMH the function pointer to the upcall method
 * @return a struct with two doubles
 */
stru_2_Doubles
add2DoubleStructs_returnStructByUpcallMH(stru_2_Doubles arg1, stru_2_Doubles arg2, stru_2_Doubles (*upcallMH)(stru_2_Doubles, stru_2_Doubles))
{
	stru_2_Doubles doubleStruct = (*upcallMH)(arg1, arg2);
	return doubleStruct;
}

/**
 * Get a pointer to struct by adding each double element of two structs
 * by invoking an upcall method.
 *
 * @param arg1 a pointer to the 1st struct with two doubles
 * @param arg2 the 2nd struct with two doubles
 * @param upcallMH the function pointer to the upcall method
 * @return a pointer to struct with two doubles
 */
stru_2_Doubles *
add2DoubleStructs_returnStructPointerByUpcallMH(stru_2_Doubles *arg1, stru_2_Doubles arg2, stru_2_Doubles * (*upcallMH)(stru_2_Doubles *, stru_2_Doubles))
{
	arg1 = (*upcallMH)(arg1, arg2);
	return arg1;
}

/**
 * Create a new struct by adding each double element of two structs
 * by invoking an upcall method.
 *
 * @param arg1 the 1st struct with three doubles
 * @param arg2 the 2nd struct with three doubles
 * @param upcallMH the function pointer to the upcall method
 * @return a struct with three doubles
 */
stru_3_Doubles
add3DoubleStructs_returnStructByUpcallMH(stru_3_Doubles arg1, stru_3_Doubles arg2, stru_3_Doubles (*upcallMH)(stru_3_Doubles, stru_3_Doubles))
{
	stru_3_Doubles doubleStruct = (*upcallMH)(arg1, arg2);
	return doubleStruct;
}

/**
 * Add an int and all elements (int & short) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a struct with an int and a short
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
int
addIntAndIntShortFromStructByUpcallMH(int arg1, stru_Int_Short arg2, int (*upcallMH)(int, stru_Int_Short))
{
	int intSum = (*upcallMH)(arg1, arg2);
	return intSum;
}

/**
 * Add an int and all elements (short & int) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a struct with a short and an int
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
int
addIntAndShortIntFromStructByUpcallMH(int arg1, stru_Short_Int arg2, int (*upcallMH)(int, stru_Short_Int))
{
	int intSum = (*upcallMH)(arg1, arg2);
	return intSum;
}

/**
 * Add an int and all elements (int & long) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a struct with an int and a long
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
LONG
addIntAndIntLongFromStructByUpcallMH(int arg1, stru_Int_Long arg2, LONG (*upcallMH)(int, stru_Int_Long))
{
	LONG longSum = (*upcallMH)(arg1, arg2);
	return longSum;
}

/**
 * Add an int and all elements (long & int) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a struct with a long and an int
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
LONG
addIntAndLongIntFromStructByUpcallMH(int arg1, stru_Long_Int arg2, LONG (*upcallMH)(int, stru_Long_Int))
{
	LONG longSum = (*upcallMH)(arg1, arg2);
	return longSum;
}

/**
 * Add a double and all elements (double & int) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a struct with a double and an int
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAndDoubleIntFromStructByUpcallMH(double arg1, stru_Double_Int arg2, double (*upcallMH)(double, stru_Double_Int))
{
	double doubleSum = (*upcallMH)(arg1, arg2);
	return doubleSum;
}

/**
 * Add a double and all elements (int & double) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a struct with an int and a double
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAndIntDoubleFromStructByUpcallMH(double arg1, stru_Int_Double arg2, double (*upcallMH)(double, stru_Int_Double))
{
	double doubleSum = (*upcallMH)(arg1, arg2);
	return doubleSum;
}

/**
 * Add a double and all elements (float & double) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a struct with a float and a double
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAndFloatDoubleFromStructByUpcallMH(double arg1, stru_Float_Double arg2, double (*upcallMH)(double, stru_Float_Double))
{
	double doubleSum = (*upcallMH)(arg1, arg2);
	return doubleSum;
}

/**
 * Add a double and all elements (double & float) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a struct with a double and a float
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAndDoubleFloatFromStructByUpcallMH(double arg1, stru_Double_Float arg2, double (*upcallMH)(double, stru_Double_Float))
{
	double doubleSum = (*upcallMH)(arg1, arg2);
	return doubleSum;
}

/**
 * Add a double and all elements (2 floats & a double) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a struct with 2 floats and a double
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAnd2FloatsDoubleFromStructByUpcallMH(double arg1, stru_2_Floats_Double arg2, double (*upcallMH)(double, stru_2_Floats_Double))
{
	double doubleSum = (*upcallMH)(arg1, arg2);
	return doubleSum;
}

/**
 * Add a double and all elements (a double & 2 floats) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a struct with a double and 2 floats
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAndDouble2FloatsFromStructByUpcallMH(double arg1, stru_Double_Float_Float arg2, double (*upcallMH)(double, stru_Double_Float_Float))
{
	double doubleSum = (*upcallMH)(arg1, arg2);
	return doubleSum;
}

/**
 * Add a float and all elements (int & 2 floats) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a struct with an int and 2 floats
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
float
addFloatAndInt2FloatsFromStructByUpcallMH(float arg1, stru_Int_Float_Float arg2, float (*upcallMH)(float, stru_Int_Float_Float))
{
	float floatSum = (*upcallMH)(arg1, arg2);
	return floatSum;
}

/**
 * Add a float and all elements (float, int and float) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a struct with a float, an int and a float
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
float
addFloatAndFloatIntFloatFromStructByUpcallMH(float arg1, stru_Float_Int_Float arg2, float (*upcallMH)(float, stru_Float_Int_Float))
{
	float floatSum = (*upcallMH)(arg1, arg2);
	return floatSum;
}

/**
 * Add a double and all elements (int, float & double) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a struct with an int, a float and a double
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAndIntFloatDoubleFromStructByUpcallMH(double arg1, stru_Int_Float_Double arg2, double (*upcallMH)(double, stru_Int_Float_Double))
{
	double doubleSum = (*upcallMH)(arg1, arg2);
	return doubleSum;
}

/**
 * Add a double and all elements (float, int & double) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a struct with a float, an int and a double
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAndFloatIntDoubleFromStructByUpcallMH(double arg1, stru_Float_Int_Double arg2, double (*upcallMH)(double, stru_Float_Int_Double))
{
	double doubleSum = (*upcallMH)(arg1, arg2);
	return doubleSum;
}

/**
 * Add a double and all elements (long & double) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a struct with a long and a double
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAndLongDoubleFromStructByUpcallMH(double arg1, stru_Long_Double arg2, double (*upcallMH)(double, stru_Long_Double))
{
	double doubleSum = (*upcallMH)(arg1, arg2);
	return doubleSum;
}

/**
 * Add a float and all elements (int & 3 floats) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a struct with an int and 3 floats
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
float
addFloatAndInt3FloatsFromStructByUpcallMH(float arg1, stru_Int_3_Floats arg2, float (*upcallMH)(float, stru_Int_3_Floats))
{
	float floatSum = (*upcallMH)(arg1, arg2);
	return floatSum;
}

/**
 * Add a long and all elements (long & 2 floats) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a struct with a long and 2 floats
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
LONG
addLongAndLong2FloatsFromStructByUpcallMH(LONG arg1, stru_Long_2_Floats arg2, LONG (*upcallMH)(LONG, stru_Long_2_Floats))
{
	LONG longSum = (*upcallMH)(arg1, arg2);
	return longSum;
}

/**
 * Add a float and all elements (3 floats & int) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a struct with 3 floats and an int
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
float
addFloatAnd3FloatsIntFromStructByUpcallMH(float arg1, stru_3_Floats_Int arg2, float (*upcallMH)(float, stru_3_Floats_Int))
{
	float floatSum = (*upcallMH)(arg1, arg2);
	return floatSum;
}

/**
 * Add a long and all elements (float & long) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a struct with a float and a long
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
LONG
addLongAndFloatLongFromStructByUpcallMH(LONG arg1, stru_Float_Long arg2, LONG (*upcallMH)(LONG, stru_Float_Long))
{
	LONG longSum = (*upcallMH)(arg1, arg2);
	return longSum;
}

/**
 * Add a double and all elements (double, float & int) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a struct with a double, a float and an int
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAndDoubleFloatIntFromStructByUpcallMH(double arg1, stru_Double_Float_Int arg2, double (*upcallMH)(double, stru_Double_Float_Int))
{
	double doubleSum = (*upcallMH)(arg1, arg2);
	return doubleSum;
}

/**
 * Add a double and all elements (double & long) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a struct with a double and a long
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAndDoubleLongFromStructByUpcallMH(double arg1, stru_Double_Long arg2, double (*upcallMH)(double, stru_Double_Long))
{
	double doubleSum = (*upcallMH)(arg1, arg2);
	return doubleSum;
}

/**
 * Add a long and all elements (2 floats & a long) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a struct with 2 floats and a long
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
LONG
addLongAnd2FloatsLongFromStructByUpcallMH(LONG arg1, stru_2_Floats_Long arg2, LONG (*upcallMH)(LONG, stru_2_Floats_Long))
{
	LONG longSum = (*upcallMH)(arg1, arg2);
	return longSum;
}

/**
 * Add a short and all elements (an array of 3 shorts & a char) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a short
 * @param arg2 a struct with an array of 3 shorts & a char
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
short
addShortAnd3ShortsCharFromStructByUpcallMH(short arg1, stru_3_Shorts_Char arg2, short (*upcallMH)(short, stru_3_Shorts_Char))
{
	short shortSum = (*upcallMH)(arg1, arg2);
	return shortSum;
}

/**
 * Add a float and all elements (int, float, int & float) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a struct with an int, a float, an int and a float
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
float
addFloatAndIntFloatIntFloatFromStructByUpcallMH(float arg1, stru_Int_Float_Int_Float arg2, float (*upcallMH)(float, stru_Int_Float_Int_Float))
{
	float floatSum = (*upcallMH)(arg1, arg2);
	return floatSum;
}

/**
 * Add a double and all elements (int, double, float) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a struct with an int, a double and a float
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAndIntDoubleFloatFromStructByUpcallMH(double arg1, stru_Int_Double_Float arg2, double (*upcallMH)(double, stru_Int_Double_Float))
{
	double doubleSum = (*upcallMH)(arg1, arg2);
	return doubleSum;
}

/**
 * Add a double and all elements (float, double, int) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a struct with a float, a double and an int
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAndFloatDoubleIntFromStructByUpcallMH(double arg1, stru_Float_Double_Int arg2, double (*upcallMH)(double, stru_Float_Double_Int))
{
	double doubleSum = (*upcallMH)(arg1, arg2);
	return doubleSum;
}

/**
 * Add a double and all elements (int, double, int) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a struct with an int, a double and an int
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAndIntDoubleIntFromStructByUpcallMH(double arg1, stru_Int_Double_Int arg2, double (*upcallMH)(double, stru_Int_Double_Int))
{
	double doubleSum = (*upcallMH)(arg1, arg2);
	return doubleSum;
}

/**
 * Add a double and all elements (float, double, float) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a struct with a float, a double and a float
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAndFloatDoubleFloatFromStructByUpcallMH(double arg1, stru_Float_Double_Float arg2, double (*upcallMH)(double, stru_Float_Double_Float))
{
	double doubleSum = (*upcallMH)(arg1, arg2);
	return doubleSum;
}

/**
 * Add a double and all elements (int, double and long) of a struct
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a struct with an int, a double and a long
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoubleAndIntDoubleLongFromStructByUpcallMH(double arg1, stru_Int_Double_Long arg2, double (*upcallMH)(double, stru_Int_Double_Long))
{
	double doubleSum = (*upcallMH)(arg1, arg2);
	return doubleSum;
}

/**
 * Return a struct with 254 bytes by invoking an upcall method.
 *
 * @param arg a struct with 254 bytes
 * @param upcallMH the function pointer to the upcall method
 * @return the struct
 */
stru_254_Bytes
return254BytesFromStructByUpcallMH(stru_254_Bytes (*upcallMH)())
{
	return (*upcallMH)();
}

/**
 * Return a struct with 4K bytes by invoking an upcall method.
 *
 * @param arg a struct with 4K bytes
 * @param upcallMH the function pointer to the upcall method
 * @return the struct
 */
stru_4K_Bytes
return4KBytesFromStructByUpcallMH(stru_4K_Bytes (*upcallMH)())
{
	return (*upcallMH)();
}

/**
 * Validate that a null pointer is successfully returned
 * from the upcall method to native.
 *
 * @param arg1 a pointer to the 1st struct with two ints
 * @param arg2 the 2nd struct with two ints
 * @param upcallMH the function pointer to the upcall method
 * @return a pointer to struct with two ints (arg1)
 *
 * Note:
 * A null pointer is returned from upcallMH().
 */
stru_2_Ints *
validateReturnNullAddrByUpcallMH(stru_2_Ints *arg1, stru_2_Ints arg2, stru_2_Ints * (*upcallMH)(stru_2_Ints *, stru_2_Ints))
{
	(*upcallMH)(arg1, arg2);
	return arg1;
}

/**
 * Add negative bytes from a struct by invoking an upcall method.
 *
 * @param arg1 a negative byte
 * @param arg2 a struct with two negative bytes
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
char
addNegBytesFromStructByUpcallMH(char arg1, stru_2_Bytes arg2, char (*upcallMH)(char, stru_2_Bytes, char, char))
{
	char byteSum = (*upcallMH)(arg1, arg2, arg2.elem1, arg2.elem2);
	return byteSum;
}

/**
 * Add negative shorts from a struct by invoking an upcall method.
 *
 * @param arg1 a negative short
 * @param arg2 a struct with two negative shorts
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
short
addNegShortsFromStructByUpcallMH(short arg1, stru_2_Shorts arg2, short (*upcallMH)(short, stru_2_Shorts, short, short))
{
	short shortSum = (*upcallMH)(arg1, arg2, arg2.elem1, arg2.elem2);
	return shortSum;
}

/**
 * Capture the linker option for the trivial downcall during the upcall.
 *
 * @param arg1 an integer
 * @return the passed-in argument
 *
 * Note:
 * The upcall is invalid in the case of the trivial downcall.
 */
int
captureTrivialOptionByUpcallMH(int arg1, int (*upcallMH)(int))
{
	return (*upcallMH)(arg1);
}

/**
 * Add a boolean and all boolean elements of a union with the XOR (^) operator
 * by invoking an upcall method.
 *
 * @param arg1 a boolean
 * @param arg2 a union with two booleans
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromUnionWithXorByUpcallMH(bool arg1, union_2_Bools arg2, bool (*upcallMH)(bool, union_2_Bools))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a boolean and two booleans of a union (dereferenced from a pointer)
 * with the XOR (^) operator by invoking an upcall method.
 *
 * @param arg1 a boolean
 * @param arg2 a pointer to union with two booleans
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromUnionPtrWithXorByUpcallMH(bool arg1, union_2_Bools *arg2, bool (*upcallMH)(bool, union_2_Bools *))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a boolean and all booleans of a union with a nested union and
 * a boolean with the XOR (^) operator by invoking an upcall method.
 *
 * @param arg1 a boolean
 * @param arg2 a union with a nested union and a boolean
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromNestedUnionWithXorByUpcallMH(bool arg1, union_NestedUnion_Bool arg2, bool (*upcallMH)(bool, union_NestedUnion_Bool))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a boolean and all booleans of a union with a boolean and a nested union (in reverse order)
 * with the XOR (^) operator by invoking an upcall method.
 *
 * @param arg1 a boolean
 * @param arg2 a union with a boolean and a nested union
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromNestedUnionWithXor_reverseOrderByUpcallMH(bool arg1, union_Bool_NestedUnion arg2, bool (*upcallMH)(bool, union_Bool_NestedUnion))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a boolean and all booleans of a union with a nested array and
 * a boolean with the XOR (^) operator by invoking an upcall method.
 *
 * @param arg1 a boolean
 * @param arg2 a union with a nested array and a boolean
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromUnionWithNestedBoolArrayByUpcallMH(bool arg1, union_NestedBoolArray_Bool arg2, bool (*upcallMH)(bool, union_NestedBoolArray_Bool))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a boolean and all booleans of a union with a boolean and a nested array
 * (in reverse order) with the XOR (^) operator by invoking an upcall method.
 *
 * @param arg1 a boolean
 * @param arg2 a union with a boolean and a nested array
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromUnionWithNestedBoolArray_reverseOrderByUpcallMH(bool arg1, union_Bool_NestedBoolArray arg2, bool (*upcallMH)(bool, union_Bool_NestedBoolArray))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a boolean and all booleans of a union with a nested union array and
 * a boolean with the XOR (^) operator by invoking an upcall method.
 *
 * @param arg1 a boolean
 * @param arg2 a union with a nested union array and a boolean
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromUnionWithNestedUnionArrayByUpcallMH(bool arg1, union_NestedUnionArray_Bool arg2, bool (*upcallMH)(bool, union_NestedUnionArray_Bool))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a boolean and all booleans of a union with a boolean and a nested union array
 * (in reverse order) with the XOR (^) operator by invoking an upcall method.
 *
 * @param arg1 a boolean
 * @param arg2 a union with a boolean and a nested union array
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromUnionWithNestedUnionArray_reverseOrderByUpcallMH(bool arg1, union_Bool_NestedUnionArray arg2, bool (*upcallMH)(bool, union_Bool_NestedUnionArray))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a new union by adding each boolean element of two unions
 * with the XOR (^) operator by invoking an upcall method.
 *
 * @param arg1 the 1st union with two booleans
 * @param arg2 the 2nd union with two booleans
 * @return a union with two booleans
 */
union_2_Bools
add2BoolUnionsWithXor_returnUnionByUpcallMH(union_2_Bools arg1, union_2_Bools arg2, union_2_Bools (*upcallMH)(union_2_Bools, union_2_Bools))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a pointer to union by adding each boolean element of two unions
 * with the XOR (^) operator by invoking an upcall method.
 *
 * @param arg1 a pointer to the 1st union with two booleans
 * @param arg2 the 2nd union with two booleans
 * @return a pointer to union with two booleans
 */
union_2_Bools *
add2BoolUnionsWithXor_returnUnionPtrByUpcallMH(union_2_Bools *arg1, union_2_Bools arg2, union_2_Bools * (*upcallMH)(union_2_Bools *, union_2_Bools))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a new union by adding each boolean element of two unions
 * with three boolean elements by invoking an upcall method.
 *
 * @param arg1 the 1st union with three booleans
 * @param arg2 the 2nd union with three booleans
 * @return a union with three booleans
 */
union_3_Bools
add3BoolUnionsWithXor_returnUnionByUpcallMH(union_3_Bools arg1, union_3_Bools arg2, union_3_Bools (*upcallMH)(union_3_Bools, union_3_Bools))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a byte and two bytes of a union by invoking an upcall method.
 *
 * @param arg1 a byte
 * @param arg2 a union with two bytes
 * @return the sum of bytes
 */
char
addByteAndBytesFromUnionByUpcallMH(char arg1, union_2_Bytes arg2, char (*upcallMH)(char, union_2_Bytes))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a byte and two bytes of a union (dereferenced from a pointer)
 * by invoking an upcall method.
 *
 * @param arg1 a byte
 * @param arg2 a pointer to union with two bytes
 * @return the sum of bytes
 */
char
addByteAndBytesFromUnionPtrByUpcallMH(char arg1, union_2_Bytes *arg2, char (*upcallMH)(char, union_2_Bytes *))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a byte and all bytes of a union with a nested union and a byte
 * by invoking an upcall method.
 *
 * @param arg1 a byte
 * @param arg2 a union with a nested union and a byte
 * @return the sum of bytes
 */
char
addByteAndBytesFromNestedUnionByUpcallMH(char arg1, union_NestedUnion_Byte arg2, char (*upcallMH)(char, union_NestedUnion_Byte))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a byte and all bytes of a union with a byte and a nested union (in reverse order)
 * by invoking an upcall method.
 *
 * @param arg1 a byte
 * @param arg2 a union with a byte and a nested union
 * @return the sum of bytes
 */
char
addByteAndBytesFromNestedUnion_reverseOrderByUpcallMH(char arg1, union_Byte_NestedUnion arg2, char (*upcallMH)(char, union_Byte_NestedUnion))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a byte and all byte elements of a union with a nested byte array
 * and a byte by invoking an upcall method.
 *
 * @param arg1 a byte
 * @param arg2 a union with a nested byte array and a byte
 * @return the sum of bytes
 */
char
addByteAndBytesFromUnionWithNestedByteArrayByUpcallMH(char arg1, union_NestedByteArray_Byte arg2, char (*upcallMH)(char, union_NestedByteArray_Byte))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a byte and all byte elements of a union with a byte and a nested byte
 * array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a byte
 * @param arg2 a union with a byte and a nested byte array
 * @return the sum of bytes
 */
char
addByteAndBytesFromUnionWithNestedByteArray_reverseOrderByUpcallMH(char arg1, union_Byte_NestedByteArray arg2, char (*upcallMH)(char, union_Byte_NestedByteArray))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a byte and all byte elements of a union with a nested union array
 * and a byte by invoking an upcall method.
 *
 * @param arg1 a byte
 * @param arg2 a union with a nested union array and a byte
 * @return the sum of bytes
 */
char
addByteAndBytesFromUnionWithNestedUnionArrayByUpcallMH(char arg1, union_NestedUnionArray_Byte arg2, char (*upcallMH)(char, union_NestedUnionArray_Byte))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a byte and all byte elements of a union with a byte and a nested union
 * array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a byte
 * @param arg2 a union with a byte and a nested byte array
 * @return the sum of bytes
 */
char
addByteAndBytesFromUnionWithNestedUnionArray_reverseOrderByUpcallMH(char arg1, union_Byte_NestedUnionArray arg2, char (*upcallMH)(char, union_Byte_NestedUnionArray))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a new union by adding each byte element of two unions
 * with two byte elements by invoking an upcall method.
 *
 * @param arg1 the 1st union with two bytes
 * @param arg2 the 2nd union with two bytes
 * @return a union with two bytes
 */
union_2_Bytes
add2ByteUnions_returnUnionByUpcallMH(union_2_Bytes arg1, union_2_Bytes arg2, union_2_Bytes (*upcallMH)(union_2_Bytes, union_2_Bytes))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a pointer to union by adding each byte element of two unions
 * with two byte elements by invoking an upcall method.
 *
 * @param arg1 a pointer to the 1st union with two bytes
 * @param arg2 the 2nd union with two bytes
 * @return a pointer to union with two bytes
 */
union_2_Bytes *
add2ByteUnions_returnUnionPtrByUpcallMH(union_2_Bytes *arg1, union_2_Bytes arg2, union_2_Bytes * (*upcallMH)(union_2_Bytes *, union_2_Bytes))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a new union by adding each byte element of two unions
 * with three byte elements by invoking an upcall method.
 *
 * @param arg1 the 1st union with three bytes
 * @param arg2 the 2nd union with three bytes
 * @return a union with three bytes
 */
union_3_Bytes
add3ByteUnions_returnUnionByUpcallMH(union_3_Bytes arg1, union_3_Bytes arg2, union_3_Bytes (*upcallMH)(union_3_Bytes, union_3_Bytes))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Generate a new char by adding a char and two chars of a union
 * by invoking an upcall method.
 *
 * @param arg1 a char
 * @param arg2 a union with two chars
 * @return a new char
 */
short
addCharAndCharsFromUnionByUpcallMH(short arg1, union_2_Chars arg2, short (*upcallMH)(short, union_2_Chars))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Generate a new char by adding a char and two chars of union
 * (dereferenced from a pointer) by invoking an upcall method.
 *
 * @param arg1 a char
 * @param arg2 a pointer to union with two chars
 * @return a new char
 */
short
addCharAndCharsFromUnionPtrByUpcallMH(short arg1, union_2_Chars *arg2, short (*upcallMH)(short, union_2_Chars *))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Generate a new char by adding a char and all char elements of a union
 * with a nested union and a char by invoking an upcall method.
 *
 * @param arg1 a char
 * @param arg2 a union with a nested union
 * @return a new char
 */
short
addCharAndCharsFromNestedUnionByUpcallMH(short arg1, union_NestedUnion_Char arg2, short (*upcallMH)(short, union_NestedUnion_Char))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Generate a new char by adding a char and all char elements of a union with a char
 * and a nested union (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a char
 * @param arg2 a union with a char and a nested union
 * @return a new char
 */
short
addCharAndCharsFromNestedUnion_reverseOrderByUpcallMH(short arg1, union_Char_NestedUnion arg2, short (*upcallMH)(short, union_Char_NestedUnion))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Generate a new char by adding a char and all char elements of a union
 * with a nested char array and a char by invoking an upcall method.
 *
 * @param arg1 a char
 * @param arg2 a union with a nested char array and a char
 * @return a new char
 */
short
addCharAndCharsFromUnionWithNestedCharArrayByUpcallMH(short arg1, union_NestedCharArray_Char arg2, short (*upcallMH)(short, union_NestedCharArray_Char))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Generate a new char by adding a char and all char elements of a union with a char
 * and a nested char array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a char
 * @param arg2 a union with a char and a nested char array
 * @return a new char
 */
short
addCharAndCharsFromUnionWithNestedCharArray_reverseOrderByUpcallMH(short arg1, union_Char_NestedCharArray arg2, short (*upcallMH)(short, union_Char_NestedCharArray))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Generate a new char by adding a char and all char elements of a union
 * with a nested union array and a char by invoking an upcall method.
 *
 * @param arg1 a char
 * @param arg2 a union with a nested char array and a char
 * @return a new char
 */
short
addCharAndCharsFromUnionWithNestedUnionArrayByUpcallMH(short arg1, union_NestedUnionArray_Char arg2, short (*upcallMH)(short, union_NestedUnionArray_Char))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Generate a new char by adding a char and all char elements of a union with a char
 * and a nested union array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a char
 * @param arg2 a union with a char and a nested char array
 * @return a new char
 */
short
addCharAndCharsFromUnionWithNestedUnionArray_reverseOrderByUpcallMH(short arg1, union_Char_NestedUnionArray arg2, short (*upcallMH)(short, union_Char_NestedUnionArray))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Create a new union by adding each char element of two unions
 * by invoking an upcall method.
 *
 * @param arg1 the 1st union with two chars
 * @param arg2 the 2nd union with two chars
 * @return a new union of with two chars
 */
union_2_Chars
add2CharUnions_returnUnionByUpcallMH(union_2_Chars arg1, union_2_Chars arg2, union_2_Chars (*upcallMH)(union_2_Chars, union_2_Chars))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a pointer to a union by adding each element of two unions
 * by invoking an upcall method.
 *
 * @param arg1 a pointer to the 1st union with two chars
 * @param arg2 the 2nd union with two chars
 * @return a pointer to a union of with two chars
 */
union_2_Chars *
add2CharUnions_returnUnionPtrByUpcallMH(union_2_Chars *arg1, union_2_Chars arg2, union_2_Chars * (*upcallMH)(union_2_Chars *, union_2_Chars))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Create a new union by adding each char element of two unions
 * with three chars by invoking an upcall method.
 *
 * @param arg1 the 1st union with three chars
 * @param arg2 the 2nd union with three chars
 * @return a new union of with three chars
 */
union_3_Chars
add3CharUnions_returnUnionByUpcallMH(union_3_Chars arg1, union_3_Chars arg2, union_3_Chars (*upcallMH)(union_3_Chars, union_3_Chars))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a short and two shorts of a union by invoking an upcall method.
 *
 * @param arg1 a short
 * @param arg2 a union with two shorts
 * @return the sum of shorts
 */
short
addShortAndShortsFromUnionByUpcallMH(short arg1, union_2_Shorts arg2, short (*upcallMH)(short, union_2_Shorts))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a short and two shorts of a union (dereferenced from a pointer)
 * by invoking an upcall method.
 *
 * @param arg1 a short
 * @param arg2 a pointer to union with two shorts
 * @return the sum of shorts
 */
short
addShortAndShortsFromUnionPtrByUpcallMH(short arg1, union_2_Shorts *arg2, short (*upcallMH)(short, union_2_Shorts *))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a short and all short elements of a union with a nested union
 * and a short by invoking an upcall method.
 *
 * @param arg1 a short
 * @param arg2 a union with a nested union and a short
 * @return the sum of shorts
 */
short
addShortAndShortsFromNestedUnionByUpcallMH(short arg1, union_NestedUnion_Short arg2, short (*upcallMH)(short, union_NestedUnion_Short))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a short and all short elements of a union with a short and a nested
 * union (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a short
 * @param arg2 a union with a short and a nested union
 * @return the sum of shorts
 */
short
addShortAndShortsFromNestedUnion_reverseOrderByUpcallMH(short arg1, union_Short_NestedUnion arg2, short (*upcallMH)(short, union_Short_NestedUnion))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a short and all short elements of a union with a nested short
 * array and a short by invoking an upcall method.
 *
 * @param arg1 a short
 * @param arg2 a union with a nested short array and a short
 * @return the sum of shorts
 */
short
addShortAndShortsFromUnionWithNestedShortArrayByUpcallMH(short arg1, union_NestedShortArray_Short arg2, short (*upcallMH)(short, union_NestedShortArray_Short))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a short and all short elements of a union with a short and a nested
 * short array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a short
 * @param arg2 a union with a short and a nested short array
 * @return the sum of shorts
 */
short
addShortAndShortsFromUnionWithNestedShortArray_reverseOrderByUpcallMH(short arg1, union_Short_NestedShortArray arg2, short (*upcallMH)(short, union_Short_NestedShortArray))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a short and all short elements of a union with a nested union array
 * and a short by invoking an upcall method.
 *
 * @param arg1 a short
 * @param arg2 a union with a nested short array and a short
 * @return the sum of shorts
 */
short
addShortAndShortsFromUnionWithNestedUnionArrayByUpcallMH(short arg1, union_NestedUnionArray_Short arg2, short (*upcallMH)(short, union_NestedUnionArray_Short))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a short and all short elements of a union with a short and a nested
 * union array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a short
 * @param arg2 a union with a short and a nested short array
 * @return the sum of shorts
 */
short
addShortAndShortsFromUnionWithNestedUnionArray_reverseOrderByUpcallMH(short arg1, union_Short_NestedUnionArray arg2, short (*upcallMH)(short, union_Short_NestedUnionArray))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a new union by adding each short element of two unions
 * with two short elements by invoking an upcall method.
 *
 * @param arg1 the 1st union with two shorts
 * @param arg2 the 2nd union with two shorts
 * @return a union with two shorts
 */
union_2_Shorts
add2ShortUnions_returnUnionByUpcallMH(union_2_Shorts arg1, union_2_Shorts arg2, union_2_Shorts (*upcallMH)(union_2_Shorts, union_2_Shorts))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a pointer to union by adding each short element of two unions
 * with two short elements by invoking an upcall method.
 *
 * @param arg1 a pointer to the 1st union with two shorts
 * @param arg2 the 2nd union with two shorts
 * @return a pointer to union with two shorts
 */
union_2_Shorts *
add2ShortUnions_returnUnionPtrByUpcallMH(union_2_Shorts *arg1, union_2_Shorts arg2, union_2_Shorts * (*upcallMH)(union_2_Shorts *, union_2_Shorts))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a new union by adding each short element of two unions
 * with three short elements by invoking an upcall method.
 *
 * @param arg1 the 1st union with three shorts
 * @param arg2 the 2nd union with three shorts
 * @return a union with three shorts
 */
union_3_Shorts
add3ShortUnions_returnUnionByUpcallMH(union_3_Shorts arg1, union_3_Shorts arg2, union_3_Shorts (*upcallMH)(union_3_Shorts, union_3_Shorts))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add an integer and two integers of a union by invoking an upcall method.
 *
 * @param arg1 an integer
 * @param arg2 a union with two integers
 * @return the sum of integers
 */
int
addIntAndIntsFromUnionByUpcallMH(int arg1, union_2_Ints arg2, int (*upcallMH)(int, union_2_Ints))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add an integer and two integers of a union (dereferenced from a pointer)
 * by invoking an upcall method.
 *
 * @param arg1 an integer
 * @param arg2 a pointer to union with two integers
 * @return the sum of integers
 */
int
addIntAndIntsFromUnionPtrByUpcallMH(int arg1, union_2_Ints *arg2, int (*upcallMH)(int, union_2_Ints *))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add an integer and all integer elements of a union with a nested union
 * and an integer by invoking an upcall method.
 *
 * @param arg1 an integer
 * @param arg2 a union with a nested union and an integer
 * @return the sum of integers
 */
int
addIntAndIntsFromNestedUnionByUpcallMH(int arg1, union_NestedUnion_Int arg2, int (*upcallMH)(int, union_NestedUnion_Int))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add an integer and all integer elements of a union with an integer and
 * a nested union (in reverse order) by invoking an upcall method.
 *
 * @param arg1 an integer
 * @param arg2 a union with an integer and a nested union
 * @return the sum of these integers
 */
int
addIntAndIntsFromNestedUnion_reverseOrderByUpcallMH(int arg1, union_Int_NestedUnion arg2, int (*upcallMH)(int, union_Int_NestedUnion))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add an integer and all integer elements of a union with a nested
 * integer array and an integer by invoking an upcall method.
 *
 * @param arg1 an integer
 * @param arg2 a union with a nested integer array and an integer
 * @return the sum of integers
 */
int
addIntAndIntsFromUnionWithNestedIntArrayByUpcallMH(int arg1, union_NestedIntArray_Int arg2, int (*upcallMH)(int, union_NestedIntArray_Int))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add an integer and all integer elements of a union with an integer and
 * a nested integer array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 an integer
 * @param arg2 a union with an integer and a nested integer array
 * @return the sum of integers
 */
int
addIntAndIntsFromUnionWithNestedIntArray_reverseOrderByUpcallMH(int arg1, union_Int_NestedIntArray arg2, int (*upcallMH)(int, union_Int_NestedIntArray))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add an integer and all integer elements of a union with a nested union array
 * and an integer by invoking an upcall method.
 *
 * @param arg1 an integer
 * @param arg2 a union with a nested integer array and an integer
 * @return the sum of integer and all elements of the union
 */
int
addIntAndIntsFromUnionWithNestedUnionArrayByUpcallMH(int arg1, union_NestedUnionArray_Int arg2, int (*upcallMH)(int, union_NestedUnionArray_Int))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add an integer and all integer elements of a union with an integer and
 * a nested union array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 an integer
 * @param arg2 a union with an integer and a nested integer array
 * @return the sum of integer and all elements of the union
 */
int
addIntAndIntsFromUnionWithNestedUnionArray_reverseOrderByUpcallMH(int arg1, union_Int_NestedUnionArray arg2, int (*upcallMH)(int, union_Int_NestedUnionArray))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a new union by adding each integer element of
 * two unions by invoking an upcall method.
 *
 * @param arg1 the 1st union with two integers
 * @param arg2 the 2nd union with two integers
 * @return a union with two integers
 */
union_2_Ints
add2IntUnions_returnUnionByUpcallMH(union_2_Ints arg1, union_2_Ints arg2, union_2_Ints (*upcallMH)(union_2_Ints, union_2_Ints))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a pointer to union by adding each integer element
 * of two unions by invoking an upcall method.
 *
 * @param arg1 a pointer to the 1st union with two integers
 * @param arg2 the 2nd union with two integers
 * @return a pointer to union with two integers
 */
union_2_Ints *
add2IntUnions_returnUnionPtrByUpcallMH(union_2_Ints *arg1, union_2_Ints arg2, union_2_Ints * (*upcallMH)(union_2_Ints *, union_2_Ints))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a new union by adding each integer element of
 * two unions by invoking an upcall method.
 *
 * @param arg1 the 1st union with three integers
 * @param arg2 the 2nd union with three integers
 * @return a union with three integers
 */
union_3_Ints
add3IntUnions_returnUnionByUpcallMH(union_3_Ints arg1, union_3_Ints arg2, union_3_Ints (*upcallMH)(union_3_Ints, union_3_Ints))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a long and two longs of a union by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a union with two longs
 * @return the sum of longs
 */
LONG
addLongAndLongsFromUnionByUpcallMH(LONG arg1, union_2_Longs arg2, LONG (*upcallMH)(LONG, union_2_Longs))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a long and two longs of a union (dereferenced from a pointer)
 * by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a pointer to union with two longs
 * @return the sum of longs
 */
LONG
addLongAndLongsFromUnionPtrByUpcallMH(LONG arg1, union_2_Longs *arg2, LONG (*upcallMH)(LONG, union_2_Longs *))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a long and all long elements of a union with a nested union
 * and a long by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a union with a nested union and long
 * @return the sum of longs
 */
LONG
addLongAndLongsFromNestedUnionByUpcallMH(LONG arg1, union_NestedUnion_Long arg2, LONG (*upcallMH)(LONG, union_NestedUnion_Long))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a long and all long elements of a union with a long and a nested
 * union (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a union with a long and a nested union
 * @return the sum of longs
 */
LONG
addLongAndLongsFromNestedUnion_reverseOrderByUpcallMH(LONG arg1, union_Long_NestedUnion arg2, LONG (*upcallMH)(LONG, union_Long_NestedUnion))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a long and all long elements of a union with a nested
 * long array and a long by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a union with a nested long array and a long
 * @return the sum of longs
 */
LONG
addLongAndLongsFromUnionWithNestedLongArrayByUpcallMH(LONG arg1, union_NestedLongArray_Long arg2, LONG (*upcallMH)(LONG, union_NestedLongArray_Long))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a long and all long elements of a union with a long and a nested
 * long array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a union with a long and a nested long array
 * @return the sum of longs
 */
LONG
addLongAndLongsFromUnionWithNestedLongArray_reverseOrderByUpcallMH(LONG arg1, union_Long_NestedLongArray arg2, LONG (*upcallMH)(LONG, union_Long_NestedLongArray))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a long and all long elements of a union with a nested
 * union array and a long by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a union with a nested long array and a long
 * @return the sum of long and all elements of the union
 */
LONG
addLongAndLongsFromUnionWithNestedUnionArrayByUpcallMH(LONG arg1, union_NestedUnionArray_Long arg2, LONG (*upcallMH)(LONG, union_NestedUnionArray_Long))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a long and all long elements of a union with a long and a nested
 * union array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a union with a long and a nested long array
 * @return the sum of long and all elements of the union
 */
LONG
addLongAndLongsFromUnionWithNestedUnionArray_reverseOrderByUpcallMH(LONG arg1, union_Long_NestedUnionArray arg2, LONG (*upcallMH)(LONG, union_Long_NestedUnionArray))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a new union by adding each long element of two unions
 * by invoking an upcall method.
 *
 * @param arg1 the 1st union with two longs
 * @param arg2 the 2nd union with two longs
 * @return a union with two longs
 */
union_2_Longs
add2LongUnions_returnUnionByUpcallMH(union_2_Longs arg1, union_2_Longs arg2, union_2_Longs (*upcallMH)(union_2_Longs, union_2_Longs))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a pointer to union by adding each long element of
 * two unions by invoking an upcall method.
 *
 * @param arg1 a pointer to the 1st union with two longs
 * @param arg2 the 2nd union with two longs
 * @return a pointer to union with two longs
 */
union_2_Longs *
add2LongUnions_returnUnionPtrByUpcallMH(union_2_Longs *arg1, union_2_Longs arg2, union_2_Longs * (*upcallMH)(union_2_Longs *, union_2_Longs))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a new union by adding each long element of
 * two unions by invoking an upcall method.
 *
 * @param arg1 the 1st union with three longs
 * @param arg2 the 2nd union with three longs
 * @return a union with three longs
 */
union_3_Longs
add3LongUnions_returnUnionByUpcallMH(union_3_Longs arg1, union_3_Longs arg2, union_3_Longs (*upcallMH)(union_3_Longs, union_3_Longs))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a float and two floats of a union by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a union with two floats
 * @return the sum of floats
 */
float
addFloatAndFloatsFromUnionByUpcallMH(float arg1, union_2_Floats arg2, float (*upcallMH)(float, union_2_Floats))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a float and two floats of a union (dereferenced from a pointer)
 * by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a pointer to union with two floats
 * @return the sum of floats
 */
float
addFloatAndFloatsFromUnionPtrByUpcallMH(float arg1, union_2_Floats *arg2, float (*upcallMH)(float, union_2_Floats *))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a float and all float elements of a union with a nested union
 * and a float by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a union with a nested union and a float
 * @return the sum of floats
 */
float
addFloatAndFloatsFromNestedUnionByUpcallMH(float arg1, union_NestedUnion_Float arg2, float (*upcallMH)(float, union_NestedUnion_Float))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a float and all float elements of a union with a float and a nested
 * union (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a union with a float and a nested union
 * @return the sum of floats
 */
float
addFloatAndFloatsFromNestedUnion_reverseOrderByUpcallMH(float arg1, union_Float_NestedUnion arg2, float (*upcallMH)(float, union_Float_NestedUnion))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a float and all float elements of a union with a nested float
 * array and a float by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a union with a nested float array and a float
 * @return the sum of floats
 */
float
addFloatAndFloatsFromUnionWithNestedFloatArrayByUpcallMH(float arg1, union_NestedFloatArray_Float arg2, float (*upcallMH)(float, union_NestedFloatArray_Float))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a float and all float elements of a union with a float and a nested
 * float array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a union with a float and a nested float array
 * @return the sum of floats
 */
float
addFloatAndFloatsFromUnionWithNestedFloatArray_reverseOrderByUpcallMH(float arg1, union_Float_NestedFloatArray arg2, float (*upcallMH)(float, union_Float_NestedFloatArray))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a float and all float elements of a union with a nested union array
 * and a float by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a union with a nested float array and a float
 * @return the sum of floats
 */
float
addFloatAndFloatsFromUnionWithNestedUnionArrayByUpcallMH(float arg1, union_NestedUnionArray_Float arg2, float (*upcallMH)(float, union_NestedUnionArray_Float))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a float and all float elements of a union with a float and a nested
 * union array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a union with a float and a nested float array
 * @return the sum of floats
 */
float
addFloatAndFloatsFromUnionWithNestedUnionArray_reverseOrderByUpcallMH(float arg1, union_Float_NestedUnionArray arg2, float (*upcallMH)(float, union_Float_NestedUnionArray))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Create a new union by adding each float element
 * of two unions by invoking an upcall method.
 *
 * @param arg1 the 1st union with two floats
 * @param arg2 the 2nd union with two floats
 * @return a union with two floats
 */
union_2_Floats
add2FloatUnions_returnUnionByUpcallMH(union_2_Floats arg1, union_2_Floats arg2, union_2_Floats (*upcallMH)(union_2_Floats, union_2_Floats))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a pointer to union by adding each float element
 * of two unions by invoking an upcall method.
 *
 * @param arg1 a pointer to the 1st union with two floats
 * @param arg2 the 2nd union with two floats
 * @return a pointer to union with two floats
 */
union_2_Floats *
add2FloatUnions_returnUnionPtrByUpcallMH(union_2_Floats *arg1, union_2_Floats arg2, union_2_Floats * (*upcallMH)(union_2_Floats *, union_2_Floats))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Create a new union by adding each float element
 * of two unions by invoking an upcall method.
 *
 * @param arg1 the 1st union with three floats
 * @param arg2 the 2nd union with three floats
 * @return a union with three floats
 */
union_3_Floats
add3FloatUnions_returnUnionByUpcallMH(union_3_Floats arg1, union_3_Floats arg2, union_3_Floats (*upcallMH)(union_3_Floats, union_3_Floats))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a double and two doubles of a union by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a union with two doubles
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromUnionByUpcallMH(double arg1, union_2_Doubles arg2, double (*upcallMH)(double, union_2_Doubles))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a double and two doubles of a union (dereferenced from a pointer)
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a pointer to union with two doubles
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromUnionPtrByUpcallMH(double arg1, union_2_Doubles *arg2, double (*upcallMH)(double, union_2_Doubles *))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a double and all doubles of a union with a nested union
 * and a double by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a union with a nested union and a double
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromNestedUnionByUpcallMH(double arg1, union_NestedUnion_Double arg2, double (*upcallMH)(double, union_NestedUnion_Double))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a double and all doubles of a union with a double and a nested
 * union (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a union with a double a nested union
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromNestedUnion_reverseOrderByUpcallMH(double arg1, union_Double_NestedUnion arg2, double (*upcallMH)(double, union_Double_NestedUnion))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a double and all double elements of a union with a nested
 * double array and a double by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a union with a nested double array and a double
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromUnionWithNestedDoubleArrayByUpcallMH(double arg1, union_NestedDoubleArray_Double arg2, double (*upcallMH)(double, union_NestedDoubleArray_Double))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a double and all double elements of a union with a double and a nested
 * double array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a union with a double and a nested double array
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromUnionWithNestedDoubleArray_reverseOrderByUpcallMH(double arg1, union_Double_NestedDoubleArray arg2, double (*upcallMH)(double, union_Double_NestedDoubleArray))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a double and all double elements of a union with a nested
 * union array and a double by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a union with a nested double array and a double
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromUnionWithNestedUnionArrayByUpcallMH(double arg1, union_NestedUnionArray_Double arg2, double (*upcallMH)(double, union_NestedUnionArray_Double))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a double and all double elements of a union with a double and a nested
 * union array (in reverse order) by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a union with a double and a nested double array
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromUnionWithNestedUnionArray_reverseOrderByUpcallMH(double arg1, union_Double_NestedUnionArray arg2, double (*upcallMH)(double, union_Double_NestedUnionArray))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Create a new union by adding each double element of
 * two unions by invoking an upcall method.
 *
 * @param arg1 the 1st union with two doubles
 * @param arg2 the 2nd union with two doubles
 * @return a union with two doubles
 */
union_2_Doubles
add2DoubleUnions_returnUnionByUpcallMH(union_2_Doubles arg1, union_2_Doubles arg2, union_2_Doubles (*upcallMH)(union_2_Doubles, union_2_Doubles))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a pointer to union by adding each double element of
 * two unions by invoking an upcall method.
 *
 * @param arg1 a pointer to the 1st union with two doubles
 * @param arg2 the 2nd union with two doubles
 * @return a pointer to union with two doubles
 */
union_2_Doubles *
add2DoubleUnions_returnUnionPtrByUpcallMH(union_2_Doubles *arg1, union_2_Doubles arg2, union_2_Doubles * (*upcallMH)(union_2_Doubles *, union_2_Doubles))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Create a new union by adding each double element of
 * two unions by invoking an upcall method.
 *
 * @param arg1 the 1st union with three doubles
 * @param arg2 the 2nd union with three doubles
 * @return a union with three doubles
 */
union_3_Doubles
add3DoubleUnions_returnUnionByUpcallMH(union_3_Doubles arg1, union_3_Doubles arg2, union_3_Doubles (*upcallMH)(union_3_Doubles, union_3_Doubles))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a short and a short of a union (byte & short)
 * by invoking an upcall method.
 *
 * @param arg1 a short
 * @param arg2 a union with a byte and a short
 * @return the sum of shorts
 */
short
addShortAndShortFromUnionWithByteShortByUpcallMH(short arg1, union_Byte_Short arg2, short (*upcallMH)(short, union_Byte_Short))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add an int and a byte of a union (integer & byte)
 * by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a union with an int and a byte
 * @return the sum of int and byte
 */
int
addIntAndByteFromUnionWithIntByteByUpcallMH(int arg1, union_Int_Byte arg2, int (*upcallMH)(int, union_Int_Byte))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add an int and an int of a union (byte & int)
 * by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a union with a byte and an int
 * @return the sum of ints
 */
int
addIntAndIntFromUnionWithByteIntByUpcallMH(int arg1, union_Byte_Int arg2, int (*upcallMH)(int, union_Byte_Int))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add an int and a short of a union (int & short)
 * by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a union with an int and a short
 * @return the sum of int and short
 */
int
addIntAndShortFromUnionWithIntShortByUpcallMH(int arg1, union_Int_Short arg2, int (*upcallMH)(int, union_Int_Short))
{
	int intSum = arg1 + arg2.elem2;
	return intSum;
}

/**
 * Add an int and an int of a union (short & integer)
 * by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a union with a short and an int
 * @return the sum of ints
 */
int
addIntAndIntFromUnionWithShortIntByUpcallMH(int arg1, union_Short_Int arg2, int (*upcallMH)(int, union_Short_Int))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add an int and a byte of a union (int ,short and byte)
 * by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a union with an int, a short and a byte
 * @return the sum of int and byte
 */
int
addIntAndByteFromUnionWithIntShortByteByUpcallMH(int arg1, union_Int_Short_Byte arg2, int (*upcallMH)(int, union_Int_Short_Byte))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add an int and a short of a union (byte, short and int)
 * by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a union with a byte, a short and an int
 * @return the sum
 */
int
addIntAndShortFromUnionWithByteShortIntByUpcallMH(int arg1, union_Byte_Short_Int arg2, int (*upcallMH)(int, union_Byte_Short_Int))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add an int and a long of a union (int & long)
 * by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a union with an int and a long
 * @return the sum of int and long
 */
LONG
addIntAndLongFromUnionWithIntLongByUpcallMH(int arg1, union_Int_Long arg2, LONG (*upcallMH)(int, union_Int_Long))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add an int and a long of a union (LONG & int)
 * by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a union with a long and an int
 * @return the sum of int and long
 */
LONG
addIntAndLongFromUnionWithLongIntByUpcallMH(int arg1, union_Long_Int arg2, LONG (*upcallMH)(int, union_Long_Int))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a float and a float of a union (short & float)
 * by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a union with a short and a float
 * @return the sum of floats
 */
float
addFloatAndFloatFromUnionWithShortFloatByUpcallMH(float arg1, union_Short_Float arg2, float (*upcallMH)(float, union_Short_Float))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a float and a float of a union (float & short)
 * by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a union with a float and a short
 * @return the sum of floats
 */
float
addFloatAndFloatFromUnionWithFloatShortByUpcallMH(float arg1, union_Float_Short arg2, float (*upcallMH)(float, union_Float_Short))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a float and a float of a union (int & float)
 * by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a union with an int and a float
 * @return the sum of floats
 */
float
addFloatAndFloatFromUnionWithIntFloatByUpcallMH(float arg1, union_Int_Float arg2, float (*upcallMH)(float, union_Int_Float))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a float and a float of a union (float & int)
 * by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a union with a float and an int
 * @return the sum of floats
 */
float
addFloatAndFloatFromUnionWithFloatIntByUpcallMH(float arg1, union_Float_Int arg2, float (*upcallMH)(float, union_Float_Int))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a double and a float of a union (float & double)
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a union with a float and a double
 * @return the sum of double and float
 */
double
addDoubleAndFloatFromUnionWithFloatDoubleByUpcallMH(double arg1, union_Float_Double arg2, double (*upcallMH)(double, union_Float_Double))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a double and a float of a union (double & float)
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a union with a double and a float
 * @return the sum of double and float
 */
double
addDoubleAndFloatFromUnionWithDoubleFloatByUpcallMH(double arg1, union_Double_Float arg2, double (*upcallMH)(double, union_Double_Float))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a double and an int of a union (int & double)
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a union with an int and a double
 * @return the sum of double and int
 */
double
addDoubleAndIntFromUnionWithIntDoubleByUpcallMH(double arg1, union_Int_Double arg2, double (*upcallMH)(double, union_Int_Double))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a double and a double of a union (double & int)
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a union with a double and an int
 * @return the sum of doubles
 */
double
addDoubleAndDoubleFromUnionWithDoubleIntByUpcallMH(double arg1, union_Double_Int arg2, double (*upcallMH)(double, union_Double_Int))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a float and a float of a union (float & long)
 * by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a union with a float and a long
 * @return the sum of floats
 */
float
addFloatAndFloatFromUnionWithFloatLongByUpcallMH(float arg1, union_Float_Long arg2, float (*upcallMH)(float, union_Float_Long))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a long and a long of a union (LONG & float)
 * by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a union with a long and a float
 * @return the sum of longs and float
 */
LONG
addLongAndLongFromUnionWithLongFloatByUpcallMH(LONG arg1, union_Long_Float arg2, LONG (*upcallMH)(LONG, union_Long_Float))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a double and a double of a union (LONG & double)
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a union with a long and a double
 * @return the sum of doubles
 */
double
addDoubleAndDoubleFromUnionWithLongDoubleByUpcallMH(double arg1, union_Long_Double arg2, double (*upcallMH)(double, union_Long_Double))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a double and a long a union (double & long)
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a union with a double and a long
 * @return the sum of double and long
 */
double
addDoubleAndLongFromUnionWithDoubleLongByUpcallMH(double arg1, union_Double_Long arg2, double (*upcallMH)(double, union_Double_Long))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a double and a float of a union (int, float and double)
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a union with an int, a float and a double
 * @return the sum of double and float
 */
double
addDoubleAndFloatFromUnionWithIntFloatDoubleByUpcallMH(double arg1, union_Int_Float_Double arg2, double (*upcallMH)(double, union_Int_Float_Double))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a double and a float of a union (int, double and float)
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a union with an int, a double and a float
 * @return the sum
 */
double
addDoubleAndFloatFromUnionWithIntDoubleFloatByUpcallMH(double arg1, union_Int_Double_Float arg2, double (*upcallMH)(double, union_Int_Double_Float))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a long and a long of a union (int, float and long)
 * by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a union with an int, a float and a long
 * @return the sum of longs
 */
LONG
addLongAndLongFromUnionWithIntFloatLongByUpcallMH(LONG arg1, union_Int_Float_Long arg2, LONG (*upcallMH)(LONG, union_Int_Float_Long))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a long and a long of a union (int, long and float)
 * by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a union with an int, a long and a float
 * @return the sum
 */
LONG
addLongAndLongFromUnionWithIntLongFloatByUpcallMH(LONG arg1, union_Int_Long_Float arg2, LONG (*upcallMH)(LONG, union_Int_Long_Float))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a double and a double of a union (float, long and double)
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a union with a float, a long and a double
 * @return the sum of doubles
 */
double
addDoubleAndDoubleFromUnionWithFloatLongDoubleByUpcallMH(double arg1, union_Float_Long_Double arg2, double (*upcallMH)(double, union_Float_Long_Double))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a double and a long of a union (int, double and long)
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a union with an int, a double and a long
 * @return the sum double and long
 */
double
addDoubleAndLongFromUnionWithIntDoubleLongByUpcallMH(double arg1, union_Int_Double_Long arg2, double (*upcallMH)(double, union_Int_Double_Long))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a double and the pointer value of a union
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a union with an int, a double and a pointer
 * @return the sum
 */
double
addDoubleAndPtrValueFromUnionByUpcallMH(double arg1, union_Int_Double_Ptr arg2, double (*upcallMH)(double, union_Int_Double_Ptr))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a long and the pointer value from a union
 * by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a union with an int, a float and a pointer
 * @return the sum
 */
LONG
addLongAndPtrValueFromUnionByUpcallMH(LONG arg1, union_Int_Float_Ptr arg2, LONG (*upcallMH)(LONG, union_Int_Float_Ptr))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a long and an int of a union (byte, short, int and long)
 * by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a union with a byte, a short, an int and a long
 * @return the sum
 */
LONG
addLongAndIntFromUnionWithByteShortIntLongByUpcallMH(LONG arg1, union_Byte_Short_Int_Long arg2, LONG (*upcallMH)(LONG, union_Byte_Short_Int_Long))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a long and an int of a union (long, int, short and byte)
 * by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a union with a long, an int, a short and a byte
 * @return the sum
 */
LONG
addLongAndIntFromUnionWithLongIntShortByteByUpcallMH(LONG arg1, union_Long_Int_Short_Byte arg2, LONG (*upcallMH)(LONG, union_Long_Int_Short_Byte))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a double and a short of a union (byte, short, float and double)
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a union with a byte, a short, a float and a double
 * @return the sum of double and short
 */
double
addDoubleAndShortFromUnionWithByteShortFloatDoubleByUpcallMH(double arg1, union_Byte_Short_Float_Double arg2, double (*upcallMH)(double, union_Byte_Short_Float_Double))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a double and a float of a union (double, float, short and byte)
 * by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a union with a double, a float, a short and a byte
 * @return the sum of double and float
 */
double
addDoubleAndFloatFromUnionWithDoubleFloatShortByteByUpcallMH(double arg1, union_Double_Float_Short_Byte arg2, double (*upcallMH)(double, union_Double_Float_Short_Byte))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a boolean and all boolean elements of a struct with a nested union (2 booleans)
 * with the XOR (^) operator by invoking an upcall method.
 *
 * @param arg1 a boolean
 * @param arg2 a struct with a boolean and a nested union (2 booleans)
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromStructWithXor_Nested2BoolUnionByUpcallMH(bool arg1, stru_Bool_NestedUnion_2_Bools arg2, bool (*upcallMH)(bool, stru_Bool_NestedUnion_2_Bools))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a boolean and all booleans elements of a union with a nested struct (2 booleans)
 * with the XOR (^) operator by invoking an upcall method.
 *
 * @param arg1 a boolean
 * @param arg2 a union with a boolean and a nested struct (2 booleans)
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromUnionWithXor_Nested2BoolStructByUpcallMH(bool arg1, union_Bool_NestedStruct_2_Bools arg2, bool (*upcallMH)(bool, union_Bool_NestedStruct_2_Bools))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a boolean and all booleans elements of a union with a nested struct (4 booleans)
 * with the XOR (^) operator by invoking an upcall method.
 *
 * @param arg1 a boolean
 * @param arg2 a union with a boolean and a nested struct (4 booleans)
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromUnionWithXor_Nested4BoolStructByUpcallMH(bool arg1, union_Bool_NestedStruct_4_Bools arg2, bool (*upcallMH)(bool, union_Bool_NestedStruct_4_Bools))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a new struct by adding each boolean element of two structs with a nested union (2 booleans)
 * with the XOR (^) operator by invoking an upcall method.
 *
 * @param arg1 the 1st struct with a boolean and a nested union (2 booleans)
 * @param arg2 the 2nd struct with a boolean and a nested union (2 booleans)
 * @return a struct with a boolean and a nested union (2 booleans)
 */
stru_Bool_NestedUnion_2_Bools
add2BoolStructsWithXor_returnStruct_Nested2BoolUnionByUpcallMH(stru_Bool_NestedUnion_2_Bools arg1, stru_Bool_NestedUnion_2_Bools arg2,
		stru_Bool_NestedUnion_2_Bools (*upcallMH)(stru_Bool_NestedUnion_2_Bools, stru_Bool_NestedUnion_2_Bools))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a new union by adding each boolean element of two unions with a nested struct (2 booleans)
 * with the XOR (^) operator by invoking an upcall method.
 *
 * @param arg1 the 1st union with a bool and a nested struct (2 booleans)
 * @param arg2 the 2nd union with a bool and a nested struct (2 booleans)
 * @return a union with a bool and a nested struct (2 booleans)
 */
union_Bool_NestedStruct_2_Bools
add2BoolUnionsWithXor_returnUnion_Nested2BoolStructByUpcallMH(union_Bool_NestedStruct_2_Bools arg1, union_Bool_NestedStruct_2_Bools arg2,
		union_Bool_NestedStruct_2_Bools (*upcallMH)(union_Bool_NestedStruct_2_Bools, union_Bool_NestedStruct_2_Bools))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a byte and two bytes of a struct with a nested union (2 bytes)
 * by invoking an upcall method.
 *
 * @param arg1 a byte
 * @param arg2 a struct with a byte and a nested union (2 bytes)
 * @return the sum of bytes
 */
char
addByteAndBytesFromStruct_Nested2ByteUnionByUpcallMH(char arg1, stru_Byte_NestedUnion_2_Bytes arg2, char (*upcallMH)(char, stru_Byte_NestedUnion_2_Bytes))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a byte and all bytes of a union with a byte and a nested struct (2 bytes)
 * by invoking an upcall method.
 *
 * @param arg1 a byte
 * @param arg2 a union with a byte and a nested struct (2 bytes)
 * @return the sum of bytes
 */
char
addByteAndBytesFromUnion_Nested2ByteStructByUpcallMH(char arg1, union_Byte_NestedStruct_2_Bytes arg2, char (*upcallMH)(char, union_Byte_NestedStruct_2_Bytes))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a byte and all bytes of a union with a byte and a nested struct (4 bytes)
 * by invoking an upcall method.
 *
 * @param arg1 a byte
 * @param arg2 a union with a byte and a nested struct (4 bytes)
 * @return the sum of bytes
 */
char
addByteAndBytesFromUnion_Nested4ByteStructByUpcallMH(char arg1, union_Byte_NestedStruct_4_Bytes arg2, char (*upcallMH)(char, union_Byte_NestedStruct_4_Bytes))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a new struct by adding each byte element of two structs with a byte
 * and a nested union (2 bytes) by invoking an upcall method.
 *
 * @param arg1 the 1st struct with a byte and a nested union (2 bytes)
 * @param arg2 the 2nd struct with a byte and a nested union (2 bytes)
 * @return a struct with a byte and a nested union (2 bytes)
 */
stru_Byte_NestedUnion_2_Bytes
add2ByteStructs_returnStruct_Nested2ByteUnionByUpcallMH(stru_Byte_NestedUnion_2_Bytes arg1, stru_Byte_NestedUnion_2_Bytes arg2,
		stru_Byte_NestedUnion_2_Bytes (*upcallMH)(stru_Byte_NestedUnion_2_Bytes, stru_Byte_NestedUnion_2_Bytes))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a new union by adding each byte element of two unions with a byte
 * and a nested struct (2 bytes) by invoking an upcall method.
 *
 * @param arg1 the 1st union with a byte and a nested struct (2 bytes)
 * @param arg2 the 2nd union with a byte and a nested struct (2 bytes)
 * @return a union with two bytes
 */
union_Byte_NestedStruct_2_Bytes
add2ByteUnions_returnUnion_Nested2ByteStructByUpcallMH(union_Byte_NestedStruct_2_Bytes arg1, union_Byte_NestedStruct_2_Bytes arg2,
		union_Byte_NestedStruct_2_Bytes (*upcallMH)(union_Byte_NestedStruct_2_Bytes, union_Byte_NestedStruct_2_Bytes))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Generate a new char by adding a char and all chars of a struct
 * with a nested union by invoking an upcall method.
 *
 * @param arg1 a char
 * @param arg2 a struct with a char and a nested union (2 chars)
 * @return a new char
 */
short
addCharAndCharsFromStruct_Nested2CharUnionByUpcallMH(short arg1, stru_Char_NestedUnion_2_Chars arg2, short (*upcallMH)(short, stru_Char_NestedUnion_2_Chars))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Generate a new char by adding a char and all chars of a union
 * with a nested struct (2 chars) by invoking an upcall method.
 *
 * @param arg1 a char
 * @param arg2 a union with a char and a nested struct (2 chars)
 * @return a new char
 */
short
addCharAndCharsFromUnion_Nested2CharStructByUpcallMH(short arg1, union_Char_NestedStruct_2_Chars arg2, short (*upcallMH)(short, union_Char_NestedStruct_2_Chars))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Generate a new char by adding a char and all chars of a union
 * with a nested struct (4 chars) by invoking an upcall method.
 *
 * @param arg1 a char
 * @param arg2 a union with a char and a nested struct (4 chars)
 * @return a new char
 */
short
addCharAndCharsFromUnion_Nested4CharStructByUpcallMH(short arg1, union_Char_NestedStruct_4_Chars arg2, short (*upcallMH)(short, union_Char_NestedStruct_4_Chars))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Create a new struct by adding each char element of two structs with
 * a char and a nested union (2 chars) by invoking an upcall method.
 *
 * @param arg1 the 1st struct with a char and a nested union (2 chars)
 * @param arg2 the 2nd struct with a char and a nested union (2 chars)
 * @return a new struct with a char and a nested union (2 chars)
 */
stru_Char_NestedUnion_2_Chars
add2CharStructs_returnStruct_Nested2CharUnionByUpcallMH(stru_Char_NestedUnion_2_Chars arg1, stru_Char_NestedUnion_2_Chars arg2,
		stru_Char_NestedUnion_2_Chars (*upcallMH)(stru_Char_NestedUnion_2_Chars, stru_Char_NestedUnion_2_Chars))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Create a new union by adding each char element of two unions with a char
 * and a nested struct (2 chars) by invoking an upcall method.
 *
 * @param arg1 the 1st union with a char and a nested struct (2 chars)
 * @param arg2 the 2nd union with a char and a nested struct (2 chars)
 * @return a new union of with a char and a nested struct (2 chars)
 */
union_Char_NestedStruct_2_Chars
add2CharUnions_returnUnion_Nested2CharStructByUpcallMH(union_Char_NestedStruct_2_Chars arg1, union_Char_NestedStruct_2_Chars arg2,
		union_Char_NestedStruct_2_Chars (*upcallMH)(union_Char_NestedStruct_2_Chars, union_Char_NestedStruct_2_Chars))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a short and all shorts of a struct with a short and a nested
 * union (2 shorts) by invoking an upcall method.
 *
 * @param arg1 a short
 * @param arg2 a struct with a short and a nested union (2 shorts)
 * @return the sum of shorts
 */
short
addShortAndShortsFromStruct_Nested2ShortUnionByUpcallMH(short arg1, stru_Short_NestedUnion_2_Shorts arg2, short (*upcallMH)(short, stru_Short_NestedUnion_2_Shorts))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a short and all shorts of a union with a short and a nested
 * struct (2 shorts) by invoking an upcall method.
 *
 * @param arg1 a short
 * @param arg2 a union with a short and a nested struct (2 shorts)
 * @return the sum of shorts
 */
short
addShortAndShortsFromUnion_Nested2ShortStructByUpcallMH(short arg1, union_Short_NestedStruct_2_Shorts arg2, short (*upcallMH)(short, union_Short_NestedStruct_2_Shorts))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a short and all shorts of a union with a short and a nested
 * struct (4 shorts) by invoking an upcall method.
 *
 * @param arg1 a short
 * @param arg2 a union with a short and a nested struct (4 shorts)
 * @return the sum of shorts
 */
short
addShortAndShortsFromUnion_Nested4ShortStructByUpcallMH(short arg1, union_Short_NestedStruct_4_Shorts arg2, short (*upcallMH)(short, union_Short_NestedStruct_4_Shorts))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a new struct by adding each short element of two structs with a short
 * and a nested union (2 shorts) by invoking an upcall method.
 *
 * @param arg1 the 1st struct with a short and a nested union (2 shorts)
 * @param arg2 the 2nd struct with a short and a nested union (2 shorts)
 * @return a struct with a short and a nested union (2 shorts)
 */
stru_Short_NestedUnion_2_Shorts
add2ShortStructs_returnStruct_Nested2ShortUnionByUpcallMH(stru_Short_NestedUnion_2_Shorts arg1, stru_Short_NestedUnion_2_Shorts arg2,
		stru_Short_NestedUnion_2_Shorts (*upcallMH)(stru_Short_NestedUnion_2_Shorts, stru_Short_NestedUnion_2_Shorts))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a new union by adding each short element of two unions with a short
 * and a nested struct (2 shorts) by invoking an upcall method.
 *
 * @param arg1 the 1st union with a short and a nested struct (2 shorts)
 * @param arg2 the 2nd union with a short and a nested struct (2 shorts)
 * @return a union with a short and a nested struct (2 shorts)
 */
union_Short_NestedStruct_2_Shorts
add2ShortUnions_returnUnion_Nested2ShortStructByUpcallMH(union_Short_NestedStruct_2_Shorts arg1, union_Short_NestedStruct_2_Shorts arg2,
		union_Short_NestedStruct_2_Shorts (*upcallMH)(union_Short_NestedStruct_2_Shorts, union_Short_NestedStruct_2_Shorts))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add an int and all ints of a struct with an int and a nested
 * struct (2 ints) by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a struct with an int and a nested struct (2 ints)
 * @return the sum of ints
 */
int
addIntAndIntsFromStruct_Nested2IntUnionByUpcallMH(int arg1, stru_Int_NestedUnion_2_Ints arg2, int (*upcallMH)(int, stru_Int_NestedUnion_2_Ints))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add an int and all ints of a union with an int and a nested
 * struct (2 ints) by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a union with an int and a nested struct (2 ints)
 * @return the sum of ints
 */
int
addIntAndIntsFromUnion_Nested2IntStructByUpcallMH(int arg1, union_Int_NestedStruct_2_Ints arg2, int (*upcallMH)(int, union_Int_NestedStruct_2_Ints))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add an int and all ints of a union with an int and a nested
 * struct (4 ints) by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a union with an int and a nested struct (4 ints)
 * @return the sum of ints
 */
int
addIntAndIntsFromUnion_Nested4IntStructByUpcallMH(int arg1, union_Int_NestedStruct_4_Ints arg2, int (*upcallMH)(int, union_Int_NestedStruct_4_Ints))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a new struct by adding each int element of two structs with an int
 * and a nested union (2 ints) by invoking an upcall method.
 *
 * @param arg1 the 1st struct with an int and a nested union (2 ints)
 * @param arg2 the 2nd struct with an int and a nested union (2 ints)
 * @return a struct with an int and a nested union (2 ints)
 */
stru_Int_NestedUnion_2_Ints
add2IntStructs_returnStruct_Nested2IntUnionByUpcallMH(stru_Int_NestedUnion_2_Ints arg1, stru_Int_NestedUnion_2_Ints arg2,
		stru_Int_NestedUnion_2_Ints (*upcallMH)(stru_Int_NestedUnion_2_Ints, stru_Int_NestedUnion_2_Ints))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a new union by adding each int element of two unions with an int
 * and a nested struct (2 ints) by invoking an upcall method.
 *
 * @param arg1 the 1st union with an int and a nested struct (2 ints)
 * @param arg2 the 2nd union with an int and a nested struct (2 ints)
 * @return a union with an int and a nested struct (2 ints)
 */
union_Int_NestedStruct_2_Ints
add2IntUnions_returnUnion_Nested2IntStructByUpcallMH(union_Int_NestedStruct_2_Ints arg1, union_Int_NestedStruct_2_Ints arg2,
		union_Int_NestedStruct_2_Ints (*upcallMH)(union_Int_NestedStruct_2_Ints, union_Int_NestedStruct_2_Ints))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a long and all longs of a struct with a long and a nested
 * union (2 longs) by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a struct with a long and a nested union (2 longs)
 * @return the sum of longs
 */
LONG
addLongAndLongsFromStruct_Nested2LongUnionByUpcallMH(LONG arg1, stru_Long_NestedUnion_2_Longs arg2, LONG (*upcallMH)(LONG, stru_Long_NestedUnion_2_Longs))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a long and all longs of a union with a long and a nested
 * struct (2 longs) by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a union with a long and a nested struct (2 longs)
 * @return the sum of longs
 */
LONG
addLongAndLongsFromUnion_Nested2LongStructByUpcallMH(LONG arg1, union_Long_NestedStruct_2_Longs arg2, LONG (*upcallMH)(LONG, union_Long_NestedStruct_2_Longs))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a long and all longs of a union with a long and a nested
 * struct (4 longs) by invoking an upcall method.
 *
 * @param arg1 a long
 * @param arg2 a union with a long and a nested struct (4 longs)
 * @return the sum of longs
 */
LONG
addLongAndLongsFromUnion_Nested4LongStructByUpcallMH(LONG arg1, union_Long_NestedStruct_4_Longs arg2, LONG (*upcallMH)(LONG, union_Long_NestedStruct_4_Longs))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a new struct by adding each long element of two structs with a long
 * and a nested union (2 longs) by invoking an upcall method.
 *
 * @param arg1 the 1st struct with a long and a nested union (2 longs)
 * @param arg2 the 2nd struct with a long and a nested union (2 longs)
 * @return a struct with a long and a nested union (2 longs)
 */
stru_Long_NestedUnion_2_Longs
add2LongStructs_returnStruct_Nested2LongUnionByUpcallMH(stru_Long_NestedUnion_2_Longs arg1, stru_Long_NestedUnion_2_Longs arg2,
		stru_Long_NestedUnion_2_Longs (*upcallMH)(stru_Long_NestedUnion_2_Longs, stru_Long_NestedUnion_2_Longs))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Get a new union by adding each long element of two unions with a long
 * and a nested struct (2 longs) by invoking an upcall method.
 *
 * @param arg1 the 1st union with a long and a nested struct (2 longs)
 * @param arg2 the 2nd union with a long and a nested struct (2 longs)
 * @return a union with a long and a nested struct (2 longs)
 */
union_Long_NestedStruct_2_Longs
add2LongUnions_returnUnion_Nested2LongStructByUpcallMH(union_Long_NestedStruct_2_Longs arg1, union_Long_NestedStruct_2_Longs arg2,
		union_Long_NestedStruct_2_Longs (*upcallMH)(union_Long_NestedStruct_2_Longs, union_Long_NestedStruct_2_Longs))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a float and all floats of a struct with a float and a nested
 * union (2 floats) by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a struct with a float and a nested union (2 floats)
 * @return the sum of floats
 */
float
addFloatAndFloatsFromStruct_Nested2FloatUnionByUpcallMH(float arg1, stru_Float_NestedUnion_2_Floats arg2, float (*upcallMH)(float, stru_Float_NestedUnion_2_Floats))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a float and all floats of a union with a float and a nested
 * struct (2 floats) by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a union with a float and a nested struct (2 floats)
 * @return the sum of floats
 */
float
addFloatAndFloatsFromUnion_Nested2FloatStructByUpcallMH(float arg1, union_Float_NestedStruct_2_Floats arg2, float (*upcallMH)(float, union_Float_NestedStruct_2_Floats))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a float and all floats of a union with a float and a nested
 * struct (4 floats) by invoking an upcall method.
 *
 * @param arg1 a float
 * @param arg2 a union with a float and a nested struct (4 floats)
 * @return the sum of floats
 */
float
addFloatAndFloatsFromUnion_Nested4FloatStructByUpcallMH(float arg1, union_Float_NestedStruct_4_Floats arg2, float (*upcallMH)(float, union_Float_NestedStruct_4_Floats))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Create a new struct by adding each float element of two structs with a float
 * and a nested union (2 floats) by invoking an upcall method.
 *
 * @param arg1 the 1st struct with a float and a nested union (2 floats)
 * @param arg2 the 2nd struct with a float and a nested union (2 floats)
 * @return a struct with a float and a nested union (2 floats)
 */
stru_Float_NestedUnion_2_Floats
add2FloatStructs_returnStruct_Nested2FloatUnionByUpcallMH(stru_Float_NestedUnion_2_Floats arg1, stru_Float_NestedUnion_2_Floats arg2
		, stru_Float_NestedUnion_2_Floats (*upcallMH)(stru_Float_NestedUnion_2_Floats, stru_Float_NestedUnion_2_Floats))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Create a new union by adding each float element of two unions with a float
 * and a nested struct (2 floats) by invoking an upcall method.
 *
 * @param arg1 the 1st union with a float and a nested struct (2 floats)
 * @param arg2 the 2nd union with a float and a nested struct (2 floats)
 * @return a union with a float and a nested struct (2 floats)
 */
union_Float_NestedStruct_2_Floats
add2FloatUnions_returnUnion_Nested2FloatStructByUpcallMH(union_Float_NestedStruct_2_Floats arg1, union_Float_NestedStruct_2_Floats arg2,
		union_Float_NestedStruct_2_Floats (*upcallMH)(union_Float_NestedStruct_2_Floats, union_Float_NestedStruct_2_Floats))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a double and all doubles of a struct with a double and a nested
 * union (2 doubles) by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a struct with a double and a nested union (2 doubles)
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromStruct_Nested2DoubleUnionByUpcallMH(double arg1, stru_Double_NestedUnion_2_Doubles arg2,
		double (*upcallMH)(double, stru_Double_NestedUnion_2_Doubles))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a double and two doubles of a union with a double and a nested
 * union (2 doubles) by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a union with a double and a nested union (2 doubles)
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromUnion_Nested2DoubleStructByUpcallMH(double arg1, union_Double_NestedStruct_2_Doubles arg2,
		double (*upcallMH)(double, union_Double_NestedStruct_2_Doubles))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a double and two doubles of a union with a double and a nested
 * union (4 doubles) by invoking an upcall method.
 *
 * @param arg1 a double
 * @param arg2 a union with a double and a nested union (4 doubles)
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromUnion_Nested4DoubleStructByUpcallMH(double arg1, union_Double_NestedStruct_4_Doubles arg2,
		double (*upcallMH)(double, union_Double_NestedStruct_4_Doubles))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Create a new struct by adding each double element of two structs with a double
 * and a nested union (2 doubles) by invoking an upcall method.
 *
 * @param arg1 the 1st struct with a double and a nested union (2 doubles)
 * @param arg2 the 2nd struct with a double and a nested union (2 doubles)
 * @return a struct with a double and a nested union (2 doubles)
 */
stru_Double_NestedUnion_2_Doubles
add2DoubleStructs_returnStruct_Nested2DoubleUnionByUpcallMH(stru_Double_NestedUnion_2_Doubles arg1, stru_Double_NestedUnion_2_Doubles arg2,
		stru_Double_NestedUnion_2_Doubles (*upcallMH)(stru_Double_NestedUnion_2_Doubles, stru_Double_NestedUnion_2_Doubles))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Create a new union by adding each double element of two unions with a double
 * and a nested struct (2 doubles) by invoking an upcall method.
 *
 * @param arg1 the 1st union with a double and a nested struct (2 doubles)
 * @param arg2 the 2nd union with a double and a nested struct (2 doubles)
 * @return a union with a double and a nested struct (2 doubles)
 */
union_Double_NestedStruct_2_Doubles
add2DoubleUnions_returnUnion_Nested2DoubleStructByUpcallMH(union_Double_NestedStruct_2_Doubles arg1, union_Double_NestedStruct_2_Doubles arg2
		, union_Double_NestedStruct_2_Doubles (*upcallMH)(union_Double_NestedStruct_2_Doubles, union_Double_NestedStruct_2_Doubles))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a short and all elements of a union with a short and a nested
 * bytes (2 bytes) by invoking an upcall method.
 *
 * @param arg1 a short
 * @param arg2 a union with a short and a nested bytes (2 bytes)
 * @return the sum
 */
short
addShortAndShortByteFromUnion_Nested2ByteStructByUpcallMH(short arg1, union_Short_NestedStruct_2_Bytes arg2,
		short (*upcallMH)(short, union_Short_NestedStruct_2_Bytes))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add a short and all bytes of a union with a short and a nested
 * struct (4 bytes) by invoking an upcall method.
 *
 * @param arg1 a short
 * @param arg2 a union with a short and a nested struct (4 bytes)
 * @return the sum
 */
short
addShortAndBytesFromUnion_Nested4ByteStructByUpcallMH(short arg1, union_Short_NestedStruct_4_Bytes arg2, short (*upcallMH)(short, union_Short_NestedStruct_4_Bytes))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add an int and all elements of a union with an int and a nested
 * structs (2 shorts) by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a union with an int and a nested struct (2 shorts)
 * @return the sum
 */
int
addIntAndIntShortFromUnion_Nested2ShortStructByUpcallMH(int arg1, union_Int_NestedStruct_2_Shorts arg2, int (*upcallMH)(int, union_Int_NestedStruct_2_Shorts))
{
	return (*upcallMH)(arg1, arg2);
}

/**
 * Add an int and all shorts of a union with an int and a nested
 * struct (4 shorts) by invoking an upcall method.
 *
 * @param arg1 an int
 * @param arg2 a union with an int and a nested struct (4 shorts)
 * @return the sum
 */
int
addIntAndShortsFromUnion_Nested4ShortStructByUpcallMH(int arg1, union_Int_NestedStruct_4_Shorts arg2, int (*upcallMH)(int, union_Int_NestedStruct_4_Shorts))
{
	return (*upcallMH)(arg1, arg2);
}
