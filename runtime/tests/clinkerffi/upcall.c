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
 * org.openj9.test.jep389.upcall (JDK16/17)
 * org.openj9.test.jep419.upcall (JDK18)
 * org.openj9.test.jep424.upcall (JDK19+)
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
addBoolAndBoolsFromStructWithXorByUpcallMH(bool arg1, stru_Bool_Bool arg2, bool (*upcallMH)(bool, stru_Bool_Bool))
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
addBoolFromPointerAndBoolsFromStructWithXorByUpcallMH(bool *arg1, stru_Bool_Bool arg2, bool (*upcallMH)(bool *, stru_Bool_Bool))
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
addBoolFromPointerAndBoolsFromStructWithXor_returnBoolPointerByUpcallMH(bool *arg1, stru_Bool_Bool arg2, bool * (*upcallMH)(bool *, stru_Bool_Bool))
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
addBoolAndBoolsFromStructPointerWithXorByUpcallMH(bool arg1, stru_Bool_Bool *arg2, bool (*upcallMH)(bool, stru_Bool_Bool *))
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
stru_Bool_Bool
add2BoolStructsWithXor_returnStructByUpcallMH(stru_Bool_Bool arg1, stru_Bool_Bool arg2, stru_Bool_Bool (*upcallMH)(stru_Bool_Bool, stru_Bool_Bool))
{
	stru_Bool_Bool boolStruct = (*upcallMH)(arg1, arg2);
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
stru_Bool_Bool *
add2BoolStructsWithXor_returnStructPointerByUpcallMH(stru_Bool_Bool *arg1, stru_Bool_Bool arg2, stru_Bool_Bool * (*upcallMH)(stru_Bool_Bool *, stru_Bool_Bool))
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
stru_Bool_Bool_Bool
add3BoolStructsWithXor_returnStructByUpcallMH(stru_Bool_Bool_Bool arg1, stru_Bool_Bool_Bool arg2, stru_Bool_Bool_Bool (*upcallMH)(stru_Bool_Bool_Bool, stru_Bool_Bool_Bool))
{
	stru_Bool_Bool_Bool boolStruct = (*upcallMH)(arg1, arg2);
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
addByteAndBytesFromStructByUpcallMH(char arg1, stru_Byte_Byte arg2, char (*upcallMH)(char, stru_Byte_Byte))
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
addByteFromPointerAndBytesFromStructByUpcallMH(char *arg1, stru_Byte_Byte arg2, char (*upcallMH)(char *, stru_Byte_Byte))
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
addByteFromPointerAndBytesFromStruct_returnBytePointerByUpcallMH(char *arg1, stru_Byte_Byte arg2, char * (*upcallMH)(char *, stru_Byte_Byte))
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
addByteAndBytesFromStructPointerByUpcallMH(char arg1, stru_Byte_Byte *arg2, char (*upcallMH)(char, stru_Byte_Byte *))
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
stru_Byte_Byte
add2ByteStructs_returnStructByUpcallMH(stru_Byte_Byte arg1, stru_Byte_Byte arg2, stru_Byte_Byte (*upcallMH)(stru_Byte_Byte, stru_Byte_Byte))
{
	stru_Byte_Byte byteStruct = (*upcallMH)(arg1, arg2);
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
stru_Byte_Byte *
add2ByteStructs_returnStructPointerByUpcallMH(stru_Byte_Byte *arg1, stru_Byte_Byte arg2, stru_Byte_Byte * (*upcallMH)(stru_Byte_Byte *, stru_Byte_Byte))
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
stru_Byte_Byte_Byte
add3ByteStructs_returnStructByUpcallMH(stru_Byte_Byte_Byte arg1, stru_Byte_Byte_Byte arg2, stru_Byte_Byte_Byte (*upcallMH)(stru_Byte_Byte_Byte, stru_Byte_Byte_Byte))
{
	stru_Byte_Byte_Byte byteStruct = (*upcallMH)(arg1, arg2);
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
addCharAndCharsFromStructByUpcallMH(short arg1, stru_Char_Char arg2, short (*upcallMH)(short, stru_Char_Char))
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
addCharFromPointerAndCharsFromStructByUpcallMH(short *arg1, stru_Char_Char arg2, short (*upcallMH)(short *, stru_Char_Char))
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
addCharFromPointerAndCharsFromStruct_returnCharPointerByUpcallMH(short *arg1, stru_Char_Char arg2, short * (*upcallMH)(short *, stru_Char_Char))
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
addCharAndCharsFromStructPointerByUpcallMH(short arg1, stru_Char_Char *arg2, short (*upcallMH)(short, stru_Char_Char *))
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
stru_Char_Char
add2CharStructs_returnStructByUpcallMH(stru_Char_Char arg1, stru_Char_Char arg2, stru_Char_Char (*upcallMH)(stru_Char_Char, stru_Char_Char))
{
	stru_Char_Char charStruct = (*upcallMH)(arg1, arg2);
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
stru_Char_Char *
add2CharStructs_returnStructPointerByUpcallMH(stru_Char_Char *arg1, stru_Char_Char arg2, stru_Char_Char * (*upcallMH)(stru_Char_Char *, stru_Char_Char))
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
stru_Char_Char_Char
add3CharStructs_returnStructByUpcallMH(stru_Char_Char_Char arg1, stru_Char_Char_Char arg2, stru_Char_Char_Char (*upcallMH)(stru_Char_Char_Char, stru_Char_Char_Char))
{
	stru_Char_Char_Char charStruct = (*upcallMH)(arg1, arg2);
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
addShortAndShortsFromStructByUpcallMH(short arg1, stru_Short_Short arg2, short (*upcallMH)(short, stru_Short_Short))
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
addShortFromPointerAndShortsFromStructByUpcallMH(short *arg1, stru_Short_Short arg2, short (*upcallMH)(short *, stru_Short_Short))
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
addShortFromPointerAndShortsFromStruct_returnShortPointerByUpcallMH(short *arg1, stru_Short_Short arg2, short * (*upcallMH)(short *, stru_Short_Short))
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
addShortAndShortsFromStructPointerByUpcallMH(short arg1, stru_Short_Short *arg2, short (*upcallMH)(short, stru_Short_Short *))
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
stru_Short_Short
add2ShortStructs_returnStructByUpcallMH(stru_Short_Short arg1, stru_Short_Short arg2, stru_Short_Short (*upcallMH)(stru_Short_Short, stru_Short_Short))
{
	stru_Short_Short shortStruct = (*upcallMH)(arg1, arg2);
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
stru_Short_Short *
add2ShortStructs_returnStructPointerByUpcallMH(stru_Short_Short *arg1, stru_Short_Short arg2, stru_Short_Short * (*upcallMH)(stru_Short_Short *, stru_Short_Short))
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
stru_Short_Short_Short
add3ShortStructs_returnStructByUpcallMH(stru_Short_Short_Short arg1, stru_Short_Short_Short arg2, stru_Short_Short_Short (*upcallMH)(stru_Short_Short_Short, stru_Short_Short_Short))
{
	stru_Short_Short_Short shortStruct = (*upcallMH)(arg1, arg2);
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
addIntAndIntsFromStructByUpcallMH(int arg1, stru_Int_Int arg2, int (*upcallMH)(int, stru_Int_Int))
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
addIntFromPointerAndIntsFromStructByUpcallMH(int *arg1, stru_Int_Int arg2,  int (*upcallMH)(int *, stru_Int_Int))
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
addIntFromPointerAndIntsFromStruct_returnIntPointerByUpcallMH(int *arg1, stru_Int_Int arg2, int *(*upcallMH)(int *, stru_Int_Int))
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
addIntAndIntsFromStructPointerByUpcallMH(int arg1, stru_Int_Int *arg2, int (*upcallMH)(int, stru_Int_Int *))
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
stru_Int_Int
add2IntStructs_returnStructByUpcallMH(stru_Int_Int arg1, stru_Int_Int arg2, stru_Int_Int (*upcallMH)(stru_Int_Int, stru_Int_Int))
{
	stru_Int_Int intStruct = (*upcallMH)(arg1, arg2);
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
stru_Int_Int *
add2IntStructs_returnStructPointerByUpcallMH(stru_Int_Int *arg1, stru_Int_Int arg2, stru_Int_Int * (*upcallMH)(stru_Int_Int *, stru_Int_Int))
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
stru_Int_Int_Int
add3IntStructs_returnStructByUpcallMH(stru_Int_Int_Int arg1, stru_Int_Int_Int arg2, stru_Int_Int_Int (*upcallMH)(stru_Int_Int_Int, stru_Int_Int_Int))
{
	stru_Int_Int_Int intStruct = (*upcallMH)(arg1, arg2);
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
addLongAndLongsFromStructByUpcallMH(LONG arg1, stru_Long_Long arg2, LONG (*upcallMH)(LONG, stru_Long_Long))
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
addLongFromPointerAndLongsFromStructByUpcallMH(LONG *arg1, stru_Long_Long arg2, LONG (*upcallMH)(LONG *, stru_Long_Long))
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
addLongFromPointerAndLongsFromStruct_returnLongPointerByUpcallMH(LONG *arg1, stru_Long_Long arg2, LONG * (*upcallMH)(LONG *, stru_Long_Long))
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
addLongAndLongsFromStructPointerByUpcallMH(LONG arg1, stru_Long_Long *arg2, LONG (*upcallMH)(LONG, stru_Long_Long *))
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
stru_Long_Long
add2LongStructs_returnStructByUpcallMH(stru_Long_Long arg1, stru_Long_Long arg2, stru_Long_Long (*upcallMH)(stru_Long_Long, stru_Long_Long))
{
	stru_Long_Long longStruct = (*upcallMH)(arg1, arg2);
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
stru_Long_Long *
add2LongStructs_returnStructPointerByUpcallMH(stru_Long_Long *arg1, stru_Long_Long arg2, stru_Long_Long * (*upcallMH)(stru_Long_Long *, stru_Long_Long))
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
stru_Long_Long_Long
add3LongStructs_returnStructByUpcallMH(stru_Long_Long_Long arg1, stru_Long_Long_Long arg2, stru_Long_Long_Long (*upcallMH)(stru_Long_Long_Long, stru_Long_Long_Long))
{
	stru_Long_Long_Long longStruct = (*upcallMH)(arg1, arg2);
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
addFloatAndFloatsFromStructByUpcallMH(float arg1, stru_Float_Float arg2, float (*upcallMH)(float, stru_Float_Float))
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
addFloatFromPointerAndFloatsFromStructByUpcallMH(float *arg1, stru_Float_Float arg2, float (*upcallMH)(float *, stru_Float_Float))
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
addFloatFromPointerAndFloatsFromStruct_returnFloatPointerByUpcallMH(float *arg1, stru_Float_Float arg2, float * (*upcallMH)(float *, stru_Float_Float))
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
addFloatAndFloatsFromStructPointerByUpcallMH(float arg1, stru_Float_Float *arg2, float (*upcallMH)(float, stru_Float_Float *))
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
stru_Float_Float
add2FloatStructs_returnStructByUpcallMH(stru_Float_Float arg1, stru_Float_Float arg2, stru_Float_Float (*upcallMH)(stru_Float_Float, stru_Float_Float))
{
	stru_Float_Float floatStruct = (*upcallMH)(arg1, arg2);
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
stru_Float_Float *
add2FloatStructs_returnStructPointerByUpcallMH(stru_Float_Float *arg1, stru_Float_Float arg2, stru_Float_Float * (*upcallMH)(stru_Float_Float *, stru_Float_Float))
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
stru_Float_Float_Float
add3FloatStructs_returnStructByUpcallMH(stru_Float_Float_Float arg1, stru_Float_Float_Float arg2, stru_Float_Float_Float (*upcallMH)(stru_Float_Float_Float, stru_Float_Float_Float))
{
	stru_Float_Float_Float floatStruct = (*upcallMH)(arg1, arg2);
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
addDoubleAndDoublesFromStructByUpcallMH(double arg1, stru_Double_Double arg2, double (*upcallMH)(double, stru_Double_Double))
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
addDoubleFromPointerAndDoublesFromStructByUpcallMH(double *arg1, stru_Double_Double arg2, double (*upcallMH)(double *, stru_Double_Double))
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
addDoubleFromPointerAndDoublesFromStruct_returnDoublePointerByUpcallMH(double *arg1, stru_Double_Double arg2, double * (*upcallMH)(double *, stru_Double_Double))
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
addDoubleAndDoublesFromStructPointerByUpcallMH(double arg1, stru_Double_Double *arg2, double (*upcallMH)(double, stru_Double_Double *))
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
stru_Double_Double
add2DoubleStructs_returnStructByUpcallMH(stru_Double_Double arg1, stru_Double_Double arg2, stru_Double_Double (*upcallMH)(stru_Double_Double, stru_Double_Double))
{
	stru_Double_Double doubleStruct = (*upcallMH)(arg1, arg2);
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
stru_Double_Double *
add2DoubleStructs_returnStructPointerByUpcallMH(stru_Double_Double *arg1, stru_Double_Double arg2, stru_Double_Double * (*upcallMH)(stru_Double_Double *, stru_Double_Double))
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
stru_Double_Double_Double
add3DoubleStructs_returnStructByUpcallMH(stru_Double_Double_Double arg1, stru_Double_Double_Double arg2, stru_Double_Double_Double (*upcallMH)(stru_Double_Double_Double, stru_Double_Double_Double))
{
	stru_Double_Double_Double doubleStruct = (*upcallMH)(arg1, arg2);
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
addDoubleAnd2FloatsDoubleFromStructByUpcallMH(double arg1, stru_Float_Float_Double arg2, double (*upcallMH)(double, stru_Float_Float_Double))
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
addLongAnd2FloatsLongFromStructByUpcallMH(LONG arg1, stru_Float_Float_Long arg2, LONG (*upcallMH)(LONG, stru_Float_Float_Long))
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
stru_Int_Int *
validateReturnNullAddrByUpcallMH(stru_Int_Int *arg1, stru_Int_Int arg2, stru_Int_Int * (*upcallMH)(stru_Int_Int *, stru_Int_Int))
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
addNegBytesFromStructByUpcallMH(char arg1, stru_Byte_Byte arg2, char (*upcallMH)(char, stru_Byte_Byte, char, char))
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
addNegShortsFromStructByUpcallMH(short arg1, stru_Short_Short arg2, short (*upcallMH)(short, stru_Short_Short, short, short))
{
	short shortSum = (*upcallMH)(arg1, arg2, arg2.elem1, arg2.elem2);
	return shortSum;
}
