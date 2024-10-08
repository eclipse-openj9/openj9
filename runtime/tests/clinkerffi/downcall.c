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
 * This file contains the native code used by the test cases via a Clinker FFI DownCall in java,
 * which come from:
 * org.openj9.test.jep389.downcall (JDK17)
 * org.openj9.test.jep434.downcall (JDK20)
 * org.openj9.test.jep442.downcall (JDK21+)
 *
 * Created by jincheng@ca.ibm.com
 */

#include "downcall.h"

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
bool
add2BoolsWithOr(bool boolArg1, bool boolArg2)
{
	bool result = (boolArg1 || boolArg2);
	return result;
}

/**
 * Add two booleans with the OR (||) operator (the 2nd one dereferenced from a pointer).
 *
 * @param boolArg1 a boolean
 * @param boolArg2 a pointer to boolean
 * @return the result of the OR operation
 */
bool
addBoolAndBoolFromPointerWithOr(bool boolArg1, bool *boolArg2)
{
	bool result = (boolArg1 || *boolArg2);
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
LONG
add2Longs(LONG longArg1, LONG longArg2)
{
	LONG longSum = longArg1 + longArg2;
	return longSum;
}

/**
 * Add two long integers (the 1st one dereferenced from a pointer).
 *
 * @param longArg1 a pointer to long integer
 * @param longArg2 a long integer
 * @return the sum of the two long integers
 */
LONG
addLongAndLongFromPointer(LONG *longArg1, LONG longArg2)
{
	LONG longSum = *longArg1 + longArg2;
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
 * Add a boolean and all boolean elements of a struct with the XOR (^) operator.
 *
 * @param arg1 a boolean
 * @param arg2 a struct with two booleans
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromStructWithXor(bool arg1, stru_2_Bools arg2)
{
	bool boolSum = arg1 ^ arg2.elem1 ^ arg2.elem2;
	return boolSum;
}

/**
 * Add a boolean (dereferenced from a pointer) and all boolean elements of
 * a struct with the XOR (^) operator.
 *
 * @param arg1 a pointer to boolean
 * @param arg2 a struct with two booleans
 * @return the XOR result of booleans
 */
bool
addBoolFromPointerAndBoolsFromStructWithXor(bool *arg1, stru_2_Bools arg2)
{
	bool boolSum = *arg1 ^ arg2.elem1 ^ arg2.elem2;
	return boolSum;
}

/**
 * Get a pointer to boolean by adding a boolean (dereferenced from a pointer)
 * and all boolean elements of a struct with the XOR (^) operator.
 *
 * @param arg1 a pointer to boolean
 * @param arg2 a struct with two booleans
 * @return a pointer to the XOR result of booleans
 */
bool *
addBoolFromPointerAndBoolsFromStructWithXor_returnBoolPointer(bool *arg1, stru_2_Bools arg2)
{
	*arg1 = *arg1 ^ arg2.elem1 ^ arg2.elem2;
	return arg1;
}

/**
 * Add a boolean and two booleans of a struct (dereferenced from a pointer) with the XOR (^) operator.
 *
 * @param arg1 a boolean
 * @param arg2 a pointer to struct with two booleans
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromStructPointerWithXor(bool arg1, stru_2_Bools *arg2)
{
	bool boolSum = arg1 ^ arg2->elem1 ^ arg2->elem2;
	return boolSum;
}

/**
 * Add a boolean and all booleans of a struct with a nested struct
 * and a boolean with the XOR (^) operator.
 *
 * @param arg1 a boolean
 * @param arg2 a struct with a nested struct and a boolean
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromNestedStructWithXor(bool arg1, stru_NestedStruct_Bool arg2)
{
	bool boolSum = arg1 ^ arg2.elem1.elem1 ^ arg2.elem1.elem2 ^ arg2.elem2;
	return boolSum;
}

/**
 * Add a boolean and all booleans of a struct with a boolean and a nested
 * struct (in reverse order) with the XOR (^) operator.
 *
 * @param arg1 a boolean
 * @param arg2 a struct with a boolean and a nested struct
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromNestedStructWithXor_reverseOrder(bool arg1, stru_Bool_NestedStruct arg2)
{
	bool boolSum = arg1 ^ arg2.elem2.elem1 ^ arg2.elem2.elem2 ^ arg2.elem1;
	return boolSum;
}

/**
 * Add a boolean and all booleans of a struct with a nested array
 * and a boolean with the XOR (^) operator.
 *
 * @param arg1 a boolean
 * @param arg2 a struct with a nested array and a boolean
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromStructWithNestedBoolArray(bool arg1, stru_NestedBoolArray_Bool arg2)
{
	bool boolSum = arg1 ^ arg2.elem1[0] ^ arg2.elem1[1] ^ arg2.elem2;
	return boolSum;
}

/**
 * Add a boolean and all booleans of a struct with a boolean and a nested array
 * (in reverse order) with the XOR (^) operator.
 *
 * @param arg1 a boolean
 * @param arg2 a struct with a boolean and a nested array
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromStructWithNestedBoolArray_reverseOrder(bool arg1, stru_Bool_NestedBoolArray arg2)
{
	bool boolSum = arg1 ^ arg2.elem1 ^ arg2.elem2[0] ^ arg2.elem2[1];
	return boolSum;
}

/**
 * Add a boolean and all booleans of a struct with a nested struct array
 * and a boolean with the XOR (^) operator.
 *
 * @param arg1 a boolean
 * @param arg2 a struct with a nested struct array and a boolean
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromStructWithNestedStructArray(bool arg1, stru_NestedStruArray_Bool arg2)
{
	bool boolSum = arg1 ^ arg2.elem2
			^ arg2.elem1[0].elem1 ^ arg2.elem1[0].elem2
			^ arg2.elem1[1].elem1 ^ arg2.elem1[1].elem2;
	return boolSum;
}

/**
 * Add a boolean and all booleans of a struct with a boolean and a nested struct array
 * (in reverse order) with the XOR (^) operator.
 *
 * @param arg1 a boolean
 * @param arg2 a struct with a boolean and a nested struct array
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromStructWithNestedStructArray_reverseOrder(bool arg1, stru_Bool_NestedStruArray arg2)
{
	bool boolSum = arg1 ^ arg2.elem1
			^ arg2.elem2[0].elem1 ^ arg2.elem2[0].elem2
			^ arg2.elem2[1].elem1 ^ arg2.elem2[1].elem2;
	return boolSum;
}

/**
 * Get a new struct by adding each boolean element of two structs with the XOR (^) operator.
 *
 * @param arg1 the 1st struct with two booleans
 * @param arg2 the 2nd struct with two booleans
 * @return a struct with two booleans
 */
stru_2_Bools
add2BoolStructsWithXor_returnStruct(stru_2_Bools arg1, stru_2_Bools arg2)
{
	stru_2_Bools boolStruct;
	boolStruct.elem1 = arg1.elem1 ^ arg2.elem1;
	boolStruct.elem2 = arg1.elem2 ^ arg2.elem2;
	return boolStruct;
}

/**
 * Get a pointer to struct by adding each boolean element of two structs with the XOR (^) operator.
 *
 * @param arg1 a pointer to the 1st struct with two booleans
 * @param arg2 the 2nd struct with two booleans
 * @return a pointer to struct with two booleans
 */
stru_2_Bools *
add2BoolStructsWithXor_returnStructPointer(stru_2_Bools *arg1, stru_2_Bools arg2)
{
	arg1->elem1 = arg1->elem1 ^ arg2.elem1;
	arg1->elem2 = arg1->elem2 ^ arg2.elem2;
	return arg1;
}

/**
 * Get a new struct by adding each boolean element of two structs with three boolean elements.
 *
 * @param arg1 the 1st struct with three booleans
 * @param arg2 the 2nd struct with three booleans
 * @return a struct with three booleans
 */
stru_3_Bools
add3BoolStructsWithXor_returnStruct(stru_3_Bools arg1, stru_3_Bools arg2)
{
	stru_3_Bools boolStruct;
	boolStruct.elem1 = arg1.elem1 ^ arg2.elem1;
	boolStruct.elem2 = arg1.elem2 ^ arg2.elem2;
	boolStruct.elem3 = arg1.elem3 ^ arg2.elem3;
	return boolStruct;
}

/**
 * Add a byte and two bytes of a struct.
 *
 * @param arg1 a byte
 * @param arg2 a struct with two bytes
 * @return the sum of bytes
 */
char
addByteAndBytesFromStruct(char arg1, stru_2_Bytes arg2)
{
	char byteSum = arg1 + arg2.elem1 + arg2.elem2;
	return byteSum;
}

/**
 * Add a byte (dereferenced from a pointer) and two bytes of a struct.
 *
 * @param arg1 a pointer to byte
 * @param arg2 a struct with two bytes
 * @return the sum of bytes
 */
char
addByteFromPointerAndBytesFromStruct(char *arg1, stru_2_Bytes arg2)
{
	char byteSum = *arg1 + arg2.elem1 + arg2.elem2;
	return byteSum;
}

/**
 * Get a pointer to byte by adding a byte (dereferenced from a pointer) and two bytes of a struct.
 *
 * @param arg1 a pointer to byte
 * @param arg2 a struct with two bytes
 * @return a pointer to the sum of bytes
 */
char *
addByteFromPointerAndBytesFromStruct_returnBytePointer(char *arg1, stru_2_Bytes arg2)
{
	*arg1 = *arg1 + arg2.elem1 + arg2.elem2;
	return arg1;
}

/**
 * Add a byte and two bytes of a struct (dereferenced from a pointer).
 *
 * @param arg1 a byte
 * @param arg2 a pointer to struct with two bytes
 * @return the sum of bytes
 */
char
addByteAndBytesFromStructPointer(char arg1, stru_2_Bytes *arg2)
{
	char byteSum = arg1 + arg2->elem1 + arg2->elem2;
	return byteSum;
}

/**
 * Add a byte and all bytes of a struct with a nested struct and a byte.
 *
 * @param arg1 a byte
 * @param arg2 a struct with a nested struct and a byte
 * @return the sum of bytes
 */
char
addByteAndBytesFromNestedStruct(char arg1, stru_NestedStruct_Byte arg2)
{
	char byteSum = arg1 + arg2.elem2 + arg2.elem1.elem1 + arg2.elem1.elem2;
	return byteSum;
}

/**
 * Add a byte and all bytes of a struct with a byte and a nested struct (in reverse order).
 *
 * @param arg1 a byte
 * @param arg2 a struct with a byte and a nested struct
 * @return the sum of bytes
 */
char
addByteAndBytesFromNestedStruct_reverseOrder(char arg1, stru_Byte_NestedStruct arg2)
{
	char byteSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return byteSum;
}


/**
 * Add a byte and all byte elements of a struct with a nested byte array and a byte.
 *
 * @param arg1 a byte
 * @param arg2 a struct with a nested byte array and a byte
 * @return the sum of bytes
 */
char
addByteAndBytesFromStructWithNestedByteArray(char arg1, stru_NestedByteArray_Byte arg2)
{
	char byteSum = arg1 + arg2.elem1[0] + arg2.elem1[1] + arg2.elem2;
	return byteSum;
}

/**
 * Add a byte and all byte elements of a struct with a byte
 * and a nested byte array (in reverse order).
 *
 * @param arg1 a byte
 * @param arg2 a struct with a byte and a nested byte array
 * @return the sum of bytes
 */
char
addByteAndBytesFromStructWithNestedByteArray_reverseOrder(char arg1, stru_Byte_NestedByteArray arg2)
{
	char byteSum = arg1 + arg2.elem2[0] + arg2.elem2[1] + arg2.elem1;
	return byteSum;
}

/**
 * Add a byte and all byte elements of a struct with a nested struct array and a byte.
 *
 * @param arg1 a byte
 * @param arg2 a struct with a nested struct array and a byte
 * @return the sum of bytes
 */
char
addByteAndBytesFromStructWithNestedStructArray(char arg1, stru_NestedStruArray_Byte arg2)
{
	char byteSum = arg1 + arg2.elem2
			+ arg2.elem1[0].elem1 + arg2.elem1[0].elem2
			+ arg2.elem1[1].elem1 + arg2.elem1[1].elem2;
	return byteSum;
}

/**
 * Add a byte and all byte elements of a struct with a byte
 * and a nested struct array (in reverse order).
 *
 * @param arg1 a byte
 * @param arg2 a struct with a byte and a nested byte array
 * @return the sum of bytes
 */
char
addByteAndBytesFromStructWithNestedStructArray_reverseOrder(char arg1, stru_Byte_NestedStruArray arg2)
{
	char byteSum = arg1 + arg2.elem1
			+ arg2.elem2[0].elem1 + arg2.elem2[0].elem2
			+ arg2.elem2[1].elem1 + arg2.elem2[1].elem2;
	return byteSum;
}

/**
 * Get a new struct by adding each byte element of two structs with two byte elements.
 *
 * @param arg1 the 1st struct with two bytes
 * @param arg2 the 2nd struct with two bytes
 * @return a struct with two bytes
 */
stru_2_Bytes
add2ByteStructs_returnStruct(stru_2_Bytes arg1, stru_2_Bytes arg2)
{
	stru_2_Bytes byteStruct;
	byteStruct.elem1 = arg1.elem1 + arg2.elem1;
	byteStruct.elem2 = arg1.elem2 + arg2.elem2;
	return byteStruct;
}

/**
 * Get a pointer to struct by adding each byte element of two structs with two byte elements.
 *
 * @param arg1 a pointer to the 1st struct with two bytes
 * @param arg2 the 2nd struct with two bytes
 * @return a pointer to struct with two bytes
 */
stru_2_Bytes *
add2ByteStructs_returnStructPointer(stru_2_Bytes *arg1, stru_2_Bytes arg2)
{
	arg1->elem1 = arg1->elem1 + arg2.elem1;
	arg1->elem2 = arg1->elem2 + arg2.elem2;
	return arg1;
}

/**
 * Get a new struct by adding each byte element of two structs with three byte elements.
 *
 * @param arg1 the 1st struct with three bytes
 * @param arg2 the 2nd struct with three bytes
 * @return a struct with three bytes
 */
stru_3_Bytes
add3ByteStructs_returnStruct(stru_3_Bytes arg1, stru_3_Bytes arg2)
{
	stru_3_Bytes byteStruct;
	byteStruct.elem1 = arg1.elem1 + arg2.elem1;
	byteStruct.elem2 = arg1.elem2 + arg2.elem2;
	byteStruct.elem3 = arg1.elem3 + arg2.elem3;
	return byteStruct;
}

/**
 * Generate a new char by adding a char and two chars of a struct.
 *
 * @param arg1 a char
 * @param arg2 a struct with two chars
 * @return a new char
 */
short
addCharAndCharsFromStruct(short arg1, stru_2_Chars arg2)
{
	short result = arg1 + arg2.elem1 + arg2.elem2 - (2 * 'A');
	return result;
}

/**
 * Generate a new char by adding a char (dereferenced from a pointer)
 * and two chars of a struct.
 *
 * @param arg1 a pointer to char
 * @param arg2 a struct with two chars
 * @return a new char
 */
short
addCharFromPointerAndCharsFromStruct(short *arg1, stru_2_Chars arg2)
{
	short result = *arg1 + arg2.elem1 + arg2.elem2 - (2 * 'A');
	return result;
}

/**
 * Get a pointer to a char by adding a char (dereferenced from a pointer)
 * and two chars of a struct.
 *
 * @param arg1 a pointer to char
 * @param arg2 a struct with two chars
 * @return a pointer to a char
 */
short *
addCharFromPointerAndCharsFromStruct_returnCharPointer(short *arg1, stru_2_Chars arg2)
{
	*arg1 = *arg1 + arg2.elem1 + arg2.elem2 - (2 * 'A');
	return arg1;
}

/**
 * Generate a new char by adding a char and two chars
 * of struct (dereferenced from a pointer).
 *
 * @param arg1 a char
 * @param arg2 a pointer to struct with two chars
 * @return a new char
 */
short
addCharAndCharsFromStructPointer(short arg1, stru_2_Chars *arg2)
{
	short result = arg1 + arg2->elem1 + arg2->elem2 - (2 * 'A');
	return result;
}

/**
 * Generate a new char by adding a char and all char elements of
 * a struct with a nested struct and a char.
 *
 * @param arg1 a char
 * @param arg2 a struct with a nested struct
 * @return a new char
 */
short
addCharAndCharsFromNestedStruct(short arg1, stru_NestedStruct_Char arg2)
{
	short result = arg1 + arg2.elem2 + arg2.elem1.elem1 + arg2.elem1.elem2 - (3 * 'A');
	return result;
}

/**
 * Generate a new char by adding a char and all char elements of a struct
 * with a char and a nested struct (in reverse order).
 *
 * @param arg1 a char
 * @param arg2 a struct with a char and a nested struct
 * @return a new char
 */
short
addCharAndCharsFromNestedStruct_reverseOrder(short arg1, stru_Char_NestedStruct arg2)
{
	short result = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2 - (3 * 'A');
	return result;
}

/**
 * Generate a new char by adding a char and all char elements
 * of a struct with a nested char array and a char.
 *
 * @param arg1 a char
 * @param arg2 a struct with a nested char array and a char
 * @return a new char
 */
short
addCharAndCharsFromStructWithNestedCharArray(short arg1, stru_NestedCharArray_Char arg2)
{
	short result = arg1 + arg2.elem1[0] + arg2.elem1[1] + arg2.elem2 - (3 * 'A');
	return result;
}

/**
 * Generate a new char by adding a char and all char elements of a struct
 * with a char and a nested char array (in reverse order).
 *
 * @param arg1 a char
 * @param arg2 a struct with a char and a nested char array
 * @return a new char
 */
short
addCharAndCharsFromStructWithNestedCharArray_reverseOrder(short arg1, stru_Char_NestedCharArray arg2)
{
	short result = arg1 + arg2.elem2[0] + arg2.elem2[1] + arg2.elem1 - (3 * 'A');
	return result;
}

/**
 * Generate a new char by adding a char and all char elements of a struct
 * with a nested struct array and a char.
 *
 * @param arg1 a char
 * @param arg2 a struct with a nested char array and a char
 * @return a new char
 */
short
addCharAndCharsFromStructWithNestedStructArray(short arg1, stru_NestedStruArray_Char arg2)
{
	short result = arg1 + arg2.elem2
			+ arg2.elem1[0].elem1 + arg2.elem1[0].elem2
			+ arg2.elem1[1].elem1 + arg2.elem1[1].elem2 - (5 * 'A');
	return result;
}

/**
 * Generate a new char by adding a char and all char elements of a struct with a char
 * and a nested struct array (in reverse order).
 *
 * @param arg1 a char
 * @param arg2 a struct with a char and a nested char array
 * @return a new char
 */
short
addCharAndCharsFromStructWithNestedStructArray_reverseOrder(short arg1, stru_Char_NestedStruArray arg2)
{
	short result = arg1 + arg2.elem1
			+ arg2.elem2[0].elem1 + arg2.elem2[0].elem2
			+ arg2.elem2[1].elem1 + arg2.elem2[1].elem2 - (5 * 'A');
	return result;
}

/**
 * Create a new struct by adding each char element of two structs.
 *
 * @param arg1 the 1st struct with two chars
 * @param arg2 the 2nd struct with two chars
 * @return a new struct of with two chars
 */
stru_2_Chars
add2CharStructs_returnStruct(stru_2_Chars arg1, stru_2_Chars arg2)
{
	stru_2_Chars charStruct;
	charStruct.elem1 = arg1.elem1 + arg2.elem1 - 'A';
	charStruct.elem2 = arg1.elem2 + arg2.elem2 - 'A';
	return charStruct;
}

/**
 * Get a pointer to a struct by adding each element of two structs.
 *
 * @param arg1 a pointer to the 1st struct with two chars
 * @param arg2 the 2nd struct with two chars
 * @return a pointer to a struct of with two chars
 */
stru_2_Chars *
add2CharStructs_returnStructPointer(stru_2_Chars *arg1, stru_2_Chars arg2)
{
	arg1->elem1 = arg1->elem1 + arg2.elem1 - 'A';
	arg1->elem2 = arg1->elem2 + arg2.elem2 - 'A';
	return arg1;
}

/**
 * Create a new struct by adding each char element of two structs with three chars.
 *
 * @param arg1 the 1st struct with three chars
 * @param arg2 the 2nd struct with three chars
 * @return a new struct of with three chars
 */
stru_3_Chars
add3CharStructs_returnStruct(stru_3_Chars arg1, stru_3_Chars arg2)
{
	stru_3_Chars charStruct;
	charStruct.elem1 = arg1.elem1 + arg2.elem1 - 'A';
	charStruct.elem2 = arg1.elem2 + arg2.elem2 - 'A';
	charStruct.elem3 = arg1.elem3 + arg2.elem3 - 'A';
	return charStruct;
}

/**
 * Add a short and two shorts of a struct.
 *
 * @param arg1 a short
 * @param arg2 a struct with two shorts
 * @return the sum of shorts
 */
short
addShortAndShortsFromStruct(short arg1, stru_2_Shorts arg2)
{
	short shortSum = arg1 + arg2.elem1 + arg2.elem2;
	return shortSum;
}

/**
 * Add a short (dereferenced from a pointer) and two shorts of a struct.
 *
 * @param arg1 a pointer to short
 * @param arg2 a struct with two shorts
 * @return the sum of shorts
 */
short
addShortFromPointerAndShortsFromStruct(short *arg1, stru_2_Shorts arg2)
{
	short shortSum = *arg1 + arg2.elem1 + arg2.elem2;
	return shortSum;
}

/**
 * Add a short (dereferenced from a pointer) and two shorts of a struct.
 *
 * @param arg1 a pointer to short
 * @param arg2 a struct with two shorts
 * @return a pointer to the sum of shorts
 */
short *
addShortFromPointerAndShortsFromStruct_returnShortPointer(short *arg1, stru_2_Shorts arg2)
{
	*arg1 = *arg1 + arg2.elem1 + arg2.elem2;
	return arg1;
}

/**
 * Add a short and two shorts of a struct (dereferenced from a pointer).
 *
 * @param arg1 a short
 * @param arg2 a pointer to struct with two shorts
 * @return the sum of shorts
 */
short
addShortAndShortsFromStructPointer(short arg1, stru_2_Shorts *arg2)
{
	short shortSum = arg1 + arg2->elem1 + arg2->elem2;
	return shortSum;
}

/**
 * Add a short and all short elements of a struct with a nested struct and a short.
 *
 * @param arg1 a short
 * @param arg2 a struct with a nested struct and a short
 * @return the sum of shorts
 */
short
addShortAndShortsFromNestedStruct(short arg1, stru_NestedStruct_Short arg2)
{
	short shortSum = arg1 + arg2.elem2 + arg2.elem1.elem1 + arg2.elem1.elem2;
	return shortSum;
}

/**
 * Add a short and all short elements of a struct with a short
 * and a nested struct (in reverse order).
 *
 * @param arg1 a short
 * @param arg2 a struct with a short and a nested struct
 * @return the sum of shorts
 */
short
addShortAndShortsFromNestedStruct_reverseOrder(short arg1, stru_Short_NestedStruct arg2)
{
	short shortSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return shortSum;
}

/**
 * Add a short and all short elements of a struct with a nested short array and a short.
 *
 * @param arg1 a short
 * @param arg2 a struct with a nested short array and a short
 * @return the sum of shorts
 */
short
addShortAndShortsFromStructWithNestedShortArray(short arg1, stru_NestedShortArray_Short arg2)
{
	short shortSum = arg1 + arg2.elem1[0] + arg2.elem1[1] + arg2.elem2;
	return shortSum;
}

/**
 * Add a short and all short elements of a struct with a short
 * and a nested short array (in reverse order).
 *
 * @param arg1 a short
 * @param arg2 a struct with a short and a nested short array
 * @return the sum of shorts
 */
short
addShortAndShortsFromStructWithNestedShortArray_reverseOrder(short arg1, stru_Short_NestedShortArray arg2)
{
	short shortSum = arg1 + arg2.elem2[0] + arg2.elem2[1] + arg2.elem1;
	return shortSum;
}

/**
 * Add a short and all short elements of a struct with a nested struct array and a short.
 *
 * @param arg1 a short
 * @param arg2 a struct with a nested short array and a short
 * @return the sum of shorts
 */
short
addShortAndShortsFromStructWithNestedStructArray(short arg1, stru_NestedStruArray_Short arg2)
{
	short shortSum = arg1 + arg2.elem2
			+ arg2.elem1[0].elem1 + arg2.elem1[0].elem2
			+ arg2.elem1[1].elem1 + arg2.elem1[1].elem2;
	return shortSum;
}

/**
 * Add a short and all short elements of a struct with a short
 * and a nested struct array (in reverse order).
 *
 * @param arg1 a short
 * @param arg2 a struct with a short and a nested short array
 * @return the sum of shorts
 */
short
addShortAndShortsFromStructWithNestedStructArray_reverseOrder(short arg1, stru_Short_NestedStruArray arg2)
{
	short shortSum = arg1 + arg2.elem1
			+ arg2.elem2[0].elem1 + arg2.elem2[0].elem2
			+ arg2.elem2[1].elem1 + arg2.elem2[1].elem2;
	return shortSum;
}

/**
 * Get a new struct by adding each short element of two structs with two short elements.
 *
 * @param arg1 the 1st struct with two shorts
 * @param arg2 the 2nd struct with two shorts
 * @return a struct with two shorts
 */
stru_2_Shorts
add2ShortStructs_returnStruct(stru_2_Shorts arg1, stru_2_Shorts arg2)
{
	stru_2_Shorts shortStruct;
	shortStruct.elem1 = arg1.elem1 + arg2.elem1;
	shortStruct.elem2 = arg1.elem2 + arg2.elem2;
	return shortStruct;
}

/**
 * Get a pointer to struct by adding each short element of two structs with two short elements.
 *
 * @param arg1 a pointer to the 1st struct with two shorts
 * @param arg2 the 2nd struct with two shorts
 * @return a pointer to struct with two shorts
 */
stru_2_Shorts *
add2ShortStructs_returnStructPointer(stru_2_Shorts *arg1, stru_2_Shorts arg2)
{
	arg1->elem1 = arg1->elem1 + arg2.elem1;
	arg1->elem2 = arg1->elem2 + arg2.elem2;
	return arg1;
}

/**
 * Get a new struct by adding each short element of two structs with three short elements.
 *
 * @param arg1 the 1st struct with three shorts
 * @param arg2 the 2nd struct with three shorts
 * @return a struct with three shorts
 */
stru_3_Shorts
add3ShortStructs_returnStruct(stru_3_Shorts arg1, stru_3_Shorts arg2)
{
	stru_3_Shorts shortStruct;
	shortStruct.elem1 = arg1.elem1 + arg2.elem1;
	shortStruct.elem2 = arg1.elem2 + arg2.elem2;
	shortStruct.elem3 = arg1.elem3 + arg2.elem3;
	return shortStruct;
}

/**
 * Add an integer and two integers of a struct.
 *
 * @param arg1 an integer
 * @param arg2 a struct with two integers
 * @return the sum of integers
 */
int
addIntAndIntsFromStruct(int arg1, stru_2_Ints arg2)
{
	int intSum = arg1 + arg2.elem1 + arg2.elem2;
	return intSum;
}

/**
 * Add an integer and all elements (integer & short) of a struct.
 *
 * @param arg1 an integer
 * @param arg2 a struct with an integer and a short
 * @return the sum of integers and short
 */
int
addIntAndIntShortFromStruct(int arg1, stru_Int_Short arg2)
{
	int intSum = arg1 + arg2.elem1 + arg2.elem2;
	return intSum;
}

/**
 * Add an integer and all elements (short & integer) of a struct.
 *
 * @param arg1 an integer
 * @param arg2 a struct with a short and an integer
 * @return the sum of integers and short
 */
int
addIntAndShortIntFromStruct(int arg1, stru_Short_Int arg2)
{
	int intSum = arg1 + arg2.elem1 + arg2.elem2;
	return intSum;
}

/**
 * Add an integer (dereferenced from a pointer) and two integers of a struct.
 *
 * @param arg1 a pointer to integer
 * @param arg2 a struct with two integers
 * @return the sum of integers
 */
int
addIntFromPointerAndIntsFromStruct(int *arg1, stru_2_Ints arg2)
{
	int intSum = *arg1 + arg2.elem1 + arg2.elem2;
	return intSum;
}

/**
 * Add an integer (dereferenced from a pointer) and two integers of a struct.
 *
 * @param arg1 a pointer to integer
 * @param arg2 a struct with two integers
 * @return a pointer to the sum of integers
 */
int *
addIntFromPointerAndIntsFromStruct_returnIntPointer(int *arg1, stru_2_Ints arg2)
{
	*arg1 += arg2.elem1 + arg2.elem2;
	return arg1;
}

/**
 * Add an integer and two integers of a struct (dereferenced from a pointer).
 *
 * @param arg1 an integer
 * @param arg2 a pointer to struct with two integers
 * @return the sum of integers
 */
int
addIntAndIntsFromStructPointer(int arg1, stru_2_Ints *arg2)
{
	int intSum = arg1 + arg2->elem1 + arg2->elem2;
	return intSum;
}

/**
 * Add an integer and all integer elements of a struct
 * with a nested struct and an integer.
 *
 * @param arg1 an integer
 * @param arg2 a struct with a nested struct and an integer
 * @return the sum of integers
 */
int
addIntAndIntsFromNestedStruct(int arg1, stru_NestedStruct_Int arg2)
{
	int intSum = arg1 + arg2.elem1.elem1 + arg2.elem1.elem2 + arg2.elem2;
	return intSum;
}

/**
 * Add an integer and all integer elements of a struct
 * with an integer and a nested struct (in reverse order).
 *
 * @param arg1 an integer
 * @param arg2 a struct with an integer and a nested struct
 * @return the sum of these integers
 */
int
addIntAndIntsFromNestedStruct_reverseOrder(int arg1, stru_Int_NestedStruct arg2)
{
	int intSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return intSum;
}

/**
 * Add an integer and all integer elements of a struct with a nested integer array and an integer.
 *
 * @param arg1 an integer
 * @param arg2 a struct with a nested integer array and an integer
 * @return the sum of integers
 */
int
addIntAndIntsFromStructWithNestedIntArray(int arg1, stru_NestedIntArray_Int arg2)
{
	int intSum = arg1 + arg2.elem1[0] + arg2.elem1[1] + arg2.elem2;
	return intSum;
}

/**
 * Add an integer and all integer elements of a struct with an integer
 * and a nested integer array (in reverse order).
 *
 * @param arg1 an integer
 * @param arg2 a struct with an integer and a nested integer array
 * @return the sum of integers
 */
int
addIntAndIntsFromStructWithNestedIntArray_reverseOrder(int arg1, stru_Int_NestedIntArray arg2)
{
	int intSum = arg1 + arg2.elem2[0] + arg2.elem2[1] + arg2.elem1;
	return intSum;
}

/**
 * Add an integer and all integer elements of a struct with a nested struct array and an integer.
 *
 * @param arg1 an integer
 * @param arg2 a struct with a nested integer array and an integer
 * @return the sum of integer and all elements of the struct
 */
int
addIntAndIntsFromStructWithNestedStructArray(int arg1, stru_NestedStruArray_Int arg2)
{
	int intSum = arg1 + arg2.elem2
			+ arg2.elem1[0].elem1 + arg2.elem1[0].elem2
			+ arg2.elem1[1].elem1 + arg2.elem1[1].elem2;
	return intSum;
}

/**
 * Add an integer and all integer elements of a struct with an integer
 * and a nested struct array (in reverse order).
 *
 * @param arg1 an integer
 * @param arg2 a struct with an integer and a nested integer array
 * @return the sum of integer and all elements of the struct
 */
int
addIntAndIntsFromStructWithNestedStructArray_reverseOrder(int arg1, stru_Int_NestedStruArray arg2)
{
	int intSum = arg1 + arg2.elem1
			+ arg2.elem2[0].elem1 + arg2.elem2[0].elem2
			+ arg2.elem2[1].elem1 + arg2.elem2[1].elem2;
	return intSum;
}

/**
 * Get a new struct by adding each integer element of two structs.
 *
 * @param arg1 the 1st struct with two integers
 * @param arg2 the 2nd struct with two integers
 * @return a struct with two integers
 */
stru_2_Ints
add2IntStructs_returnStruct(stru_2_Ints arg1, stru_2_Ints arg2)
{
	stru_2_Ints intStruct;
	intStruct.elem1 = arg1.elem1 + arg2.elem1;
	intStruct.elem2 = arg1.elem2 + arg2.elem2;
	return intStruct;
}

/**
 * Get a pointer to struct by adding each integer element of two structs.
 *
 * @param arg1 a pointer to the 1st struct with two integers
 * @param arg2 the 2nd struct with two integers
 * @return a pointer to struct with two integers
 */
stru_2_Ints *
add2IntStructs_returnStructPointer(stru_2_Ints *arg1, stru_2_Ints arg2)
{
	arg1->elem1 = arg1->elem1 + arg2.elem1;
	arg1->elem2 = arg1->elem2 + arg2.elem2;
	return arg1;
}

/**
 * Get a new struct by adding each integer element of two structs.
 *
 * @param arg1 the 1st struct with three integers
 * @param arg2 the 2nd struct with three integers
 * @return a struct with three integers
 */
stru_3_Ints
add3IntStructs_returnStruct(stru_3_Ints arg1, stru_3_Ints arg2)
{
	stru_3_Ints intStruct;
	intStruct.elem1 = arg1.elem1 + arg2.elem1;
	intStruct.elem2 = arg1.elem2 + arg2.elem2;
	intStruct.elem3 = arg1.elem3 + arg2.elem3;
	return intStruct;
}

/**
 * Add a long and two longs of a struct.
 *
 * @param arg1 a long
 * @param arg2 a struct with two longs
 * @return the sum of longs
 */
LONG
addLongAndLongsFromStruct(LONG arg1, stru_2_Longs arg2)
{
	LONG longSum = arg1 + arg2.elem1 + arg2.elem2;
	return longSum;
}

/**
 * Add an integer and all elements (int & long) of a struct.
 *
 * @param arg1 an int
 * @param arg2 a struct with an integer and a long
 * @return the sum of integers and long
 */
LONG
addIntAndIntLongFromStruct(int arg1, stru_Int_Long arg2)
{
	LONG longSum = arg1 + arg2.elem1 + arg2.elem2;
	return longSum;
}

/**
 * Add an integer and all elements (long & int) of a struct.
 *
 * @param arg1 an int
 * @param arg2 a struct with a long and an int
 * @return the sum of integers and long
 */
LONG
addIntAndLongIntFromStruct(int arg1, stru_Long_Int arg2)
{
	LONG longSum = arg1 + arg2.elem1 + arg2.elem2;
	return longSum;
}

/**
 * Add a long (dereferenced from a pointer) and two longs of a struct.
 *
 * @param arg1 a pointer to long
 * @param arg2 a struct with two longs
 * @return the sum of longs
 */
LONG
addLongFromPointerAndLongsFromStruct(LONG *arg1, stru_2_Longs arg2)
{
	LONG longSum = *arg1 + arg2.elem1 + arg2.elem2;
	return longSum;
}

/**
 * Add a long (dereferenced from a pointer) and two longs of a struct.
 *
 * @param arg1 a pointer to long
 * @param arg2 a struct with two longs
 * @return a pointer to the sum of longs
 */
LONG *
addLongFromPointerAndLongsFromStruct_returnLongPointer(LONG *arg1, stru_2_Longs arg2)
{
	*arg1 = *arg1 + arg2.elem1 + arg2.elem2;
	return arg1;
}

/**
 * Add a long and two longs of a struct (dereferenced from a pointer).
 *
 * @param arg1 a long
 * @param arg2 a pointer to struct with two longs
 * @return the sum of longs
 */
LONG
addLongAndLongsFromStructPointer(LONG arg1, stru_2_Longs *arg2)
{
	LONG longSum = arg1 + arg2->elem1 + arg2->elem2;
	return longSum;
}

/**
 * Add a long and all long elements of a struct with a nested struct and a long.
 *
 * @param arg1 a long
 * @param arg2 a struct with a nested struct and long
 * @return the sum of longs
 */
LONG
addLongAndLongsFromNestedStruct(LONG arg1, stru_NestedStruct_Long arg2)
{
	LONG longSum = arg1 + arg2.elem2 + arg2.elem1.elem1 + arg2.elem1.elem2;
	return longSum;
}

/**
 * Add a long and all long elements of a struct with
 * a long and a nested struct (in reverse order).
 *
 * @param arg1 a long
 * @param arg2 a struct with a long and a nested struct
 * @return the sum of longs
 */
LONG
addLongAndLongsFromNestedStruct_reverseOrder(LONG arg1, stru_Long_NestedStruct arg2)
{
	LONG longSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return longSum;
}

/**
 * Add a long and all long elements of a struct with a nested long array and a long.
 *
 * @param arg1 a long
 * @param arg2 a struct with a nested long array and a long
 * @return the sum of longs
 */
LONG
addLongAndLongsFromStructWithNestedLongArray(LONG arg1, stru_NestedLongArray_Long arg2)
{
	LONG longSum = arg1 + arg2.elem1[0] + arg2.elem1[1] + arg2.elem2;
	return longSum;
}

/**
 * Add a long and all long elements of a struct with a long
 * and a nested long array (in reverse order).
 *
 * @param arg1 a long
 * @param arg2 a struct with a long and a nested long array
 * @return the sum of longs
 */
LONG
addLongAndLongsFromStructWithNestedLongArray_reverseOrder(LONG arg1, stru_Long_NestedLongArray arg2)
{
	LONG longSum = arg1 + arg2.elem2[0] + arg2.elem2[1] + arg2.elem1;
	return longSum;
}

/**
 * Add a long and all long elements of a struct with a nested struct array and a long.
 *
 * @param arg1 a long
 * @param arg2 a struct with a nested long array and a long
 * @return the sum of long and all elements of the struct
 */
LONG
addLongAndLongsFromStructWithNestedStructArray(LONG arg1, stru_NestedStruArray_Long arg2)
{
	LONG longSum = arg1 + arg2.elem2
			+ arg2.elem1[0].elem1 + arg2.elem1[0].elem2
			+ arg2.elem1[1].elem1 + arg2.elem1[1].elem2;
	return longSum;
}

/**
 * Add a long and all long elements of a struct with a long
 * and a nested struct array (in reverse order).
 *
 * @param arg1 a long
 * @param arg2 a struct with a long and a nested long array
 * @return the sum of long and all elements of the struct
 */
LONG
addLongAndLongsFromStructWithNestedStructArray_reverseOrder(LONG arg1, stru_Long_NestedStruArray arg2)
{
	LONG longSum = arg1 + arg2.elem1
			+ arg2.elem2[0].elem1 + arg2.elem2[0].elem2
			+ arg2.elem2[1].elem1 + arg2.elem2[1].elem2;
	return longSum;
}

/**
 * Get a new struct by adding each long element of two structs.
 *
 * @param arg1 the 1st struct with two longs
 * @param arg2 the 2nd struct with two longs
 * @return a struct with two longs
 */
stru_2_Longs
add2LongStructs_returnStruct(stru_2_Longs arg1, stru_2_Longs arg2)
{
	stru_2_Longs longStruct;
	longStruct.elem1 = arg1.elem1 + arg2.elem1;
	longStruct.elem2 = arg1.elem2 + arg2.elem2;
	return longStruct;
}

/**
 * Get a pointer to struct by adding each long element of two structs.
 *
 * @param arg1 a pointer to the 1st struct with two longs
 * @param arg2 the 2nd struct with two longs
 * @return a pointer to struct with two longs
 */
stru_2_Longs *
add2LongStructs_returnStructPointer(stru_2_Longs *arg1, stru_2_Longs arg2)
{
	arg1->elem1 = arg1->elem1 + arg2.elem1;
	arg1->elem2 = arg1->elem2 + arg2.elem2;
	return arg1;
}

/**
 * Get a new struct by adding each long element of two structs.
 *
 * @param arg1 the 1st struct with three longs
 * @param arg2 the 2nd struct with three longs
 * @return a struct with three longs
 */
stru_3_Longs
add3LongStructs_returnStruct(stru_3_Longs arg1, stru_3_Longs arg2)
{
	stru_3_Longs longStruct;
	longStruct.elem1 = arg1.elem1 + arg2.elem1;
	longStruct.elem2 = arg1.elem2 + arg2.elem2;
	longStruct.elem3 = arg1.elem3 + arg2.elem3;
	return longStruct;
}

/**
 * Add a float and two floats of a struct.
 *
 * @param arg1 a float
 * @param arg2 a struct with two floats
 * @return the sum of floats
 */
float
addFloatAndFloatsFromStruct(float arg1, stru_2_Floats arg2)
{
	float floatSum = arg1 + arg2.elem1 + arg2.elem2;
	return floatSum;
}

/**
 * Add a float (dereferenced from a pointer) and two floats of a struct.
 *
 * @param arg1 a pointer to float
 * @param arg2 a struct with two floats
 * @return the sum of floats
 */
float
addFloatFromPointerAndFloatsFromStruct(float *arg1, stru_2_Floats arg2)
{
	float floatSum = *arg1 + arg2.elem1 + arg2.elem2;
	return floatSum;
}

/**
 * Add a float (dereferenced from a pointer) and two floats of a struct.
 *
 * @param arg1 a pointer to float
 * @param arg2 a struct with two floats
 * @return a pointer to the sum of floats
 */
float *
addFloatFromPointerAndFloatsFromStruct_returnFloatPointer(float *arg1, stru_2_Floats arg2)
{
	*arg1 = *arg1 + arg2.elem1 + arg2.elem2;
	return arg1;
}

/**
 * Add a float and two floats of a struct (dereferenced from a pointer).
 *
 * @param arg1 a float
 * @param arg2 a pointer to struct with two floats
 * @return the sum of floats
 */
float
addFloatAndFloatsFromStructPointer(float arg1, stru_2_Floats *arg2)
{
	float floatSum = arg1 + arg2->elem1 + arg2->elem2;
	return floatSum;
}

/**
 * Add a float and all float elements of a struct with a nested struct and a float.
 *
 * @param arg1 a float
 * @param arg2 a struct with a nested struct and a float
 * @return the sum of floats
 */
float
addFloatAndFloatsFromNestedStruct(float arg1, stru_NestedStruct_Float arg2)
{
	float floatSum = arg1 + arg2.elem2 + arg2.elem1.elem1 + arg2.elem1.elem2;
	return floatSum;
}

/**
 * Add a float and all float elements of a struct with a float and a nested struct (in reverse order).
 *
 * @param arg1 a float
 * @param arg2 a struct with a float and a nested struct
 * @return the sum of floats
 */
float
addFloatAndFloatsFromNestedStruct_reverseOrder(float arg1, stru_Float_NestedStruct arg2)
{
	float floatSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return floatSum;
}

/**
 * Add a float and all float elements of a struct with a nested float array and a float.
 *
 * @param arg1 a float
 * @param arg2 a struct with a nested float array and a float
 * @return the sum of floats
 */
float
addFloatAndFloatsFromStructWithNestedFloatArray(float arg1, stru_NestedFloatArray_Float arg2)
{
	float floatSum = arg1 + arg2.elem1[0] + arg2.elem1[1] + arg2.elem2;
	return floatSum;
}

/**
 * Add a float and all float elements of a struct with a float
 * and a nested float array (in reverse order).
 *
 * @param arg1 a float
 * @param arg2 a struct with a float and a nested float array
 * @return the sum of floats
 */
float
addFloatAndFloatsFromStructWithNestedFloatArray_reverseOrder(float arg1, stru_Float_NestedFloatArray arg2)
{
	float floatSum = arg1 + arg2.elem2[0] + arg2.elem2[1] + arg2.elem1;
	return floatSum;
}

/**
 * Add a float and all float elements of a struct with a nested struct array and a float.
 *
 * @param arg1 a float
 * @param arg2 a struct with a nested float array and a float
 * @return the sum of floats
 */
float
addFloatAndFloatsFromStructWithNestedStructArray(float arg1, stru_NestedStruArray_Float arg2)
{
	float floatSum = arg1 + arg2.elem2
			+ arg2.elem1[0].elem1 + arg2.elem1[0].elem2
			+ arg2.elem1[1].elem1 + arg2.elem1[1].elem2;
	return floatSum;
}

/**
 * Add a float and all float elements of a struct with a float
 * and a nested struct array (in reverse order).
 *
 * @param arg1 a float
 * @param arg2 a struct with a float and a nested float array
 * @return the sum of floats
 */
float
addFloatAndFloatsFromStructWithNestedStructArray_reverseOrder(float arg1, stru_Float_NestedStruArray arg2)
{
	float floatSum = arg1 + arg2.elem1
			+ arg2.elem2[0].elem1 + arg2.elem2[0].elem2
			+ arg2.elem2[1].elem1 + arg2.elem2[1].elem2;
	return floatSum;
}

/**
 * Create a new struct by adding each float element of two structs.
 *
 * @param arg1 the 1st struct with two floats
 * @param arg2 the 2nd struct with two floats
 * @return a struct with two floats
 */
stru_2_Floats
add2FloatStructs_returnStruct(stru_2_Floats arg1, stru_2_Floats arg2)
{
	stru_2_Floats floatStruct;
	floatStruct.elem1 = arg1.elem1 + arg2.elem1;
	floatStruct.elem2 = arg1.elem2 + arg2.elem2;
	return floatStruct;
}

/**
 * Get a pointer to struct by adding each float element of two structs.
 *
 * @param arg1 a pointer to the 1st struct with two floats
 * @param arg2 the 2nd struct with two floats
 * @return a pointer to struct with two floats
 */
stru_2_Floats *
add2FloatStructs_returnStructPointer(stru_2_Floats *arg1, stru_2_Floats arg2)
{
	arg1->elem1 = arg1->elem1 + arg2.elem1;
	arg1->elem2 = arg1->elem2 + arg2.elem2;
	return arg1;
}

/**
 * Create a new struct by adding each float element of two structs.
 *
 * @param arg1 the 1st struct with three floats
 * @param arg2 the 2nd struct with three floats
 * @return a struct with three floats
 */
stru_3_Floats
add3FloatStructs_returnStruct(stru_3_Floats arg1, stru_3_Floats arg2)
{
	stru_3_Floats floatStruct;
	floatStruct.elem1 = arg1.elem1 + arg2.elem1;
	floatStruct.elem2 = arg1.elem2 + arg2.elem2;
	floatStruct.elem3 = arg1.elem3 + arg2.elem3;
	return floatStruct;
}

/**
 * Add a double and two doubles of a struct.
 *
 * @param arg1 a double
 * @param arg2 a struct with two doubles
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromStruct(double arg1, stru_2_Doubles arg2)
{
	double doubleSum = arg1 + arg2.elem1 + arg2.elem2;
	return doubleSum;
}

/**
 * Add a double and all elements (float & double) of a struct.
 *
 * @param arg1 a double
 * @param arg2 a struct with a float and a double
 * @return the sum of doubles and float
 */
double
addDoubleAndFloatDoubleFromStruct(double arg1, stru_Float_Double arg2)
{
	double doubleSum = arg1 + arg2.elem1 + arg2.elem2;
	return doubleSum;
}

/**
 * Add a double and all elements (int & double) of a struct.
 *
 * @param arg1 a double
 * @param arg2 a struct with an int and a double
 * @return the sum of doubles and int
 */
double
addDoubleAndIntDoubleFromStruct(double arg1, stru_Int_Double arg2)
{
	double doubleSum = arg1 + arg2.elem1 + arg2.elem2;
	return doubleSum;
}


/**
 * Add a double and all elements (double & float) of a struct.
 *
 * @param arg1 a double
 * @param arg2 a struct with a double and a float
 * @return the sum of doubles and float
 */
double
addDoubleAndDoubleFloatFromStruct(double arg1, stru_Double_Float arg2)
{
	double doubleSum = arg1 + arg2.elem1 + arg2.elem2;
	return doubleSum;
}

/**
 * Add a double and all elements (double & int) of a struct.
 *
 * @param arg1 a double
 * @param arg2 a struct with a double and an int
 * @return the sum of doubles and int
 */
double
addDoubleAndDoubleIntFromStruct(double arg1, stru_Double_Int arg2)
{
	double doubleSum = arg1 + arg2.elem1 + arg2.elem2;
	return doubleSum;
}

/**
 * Add a double (dereferenced from a pointer) and two doubles of a struct.
 *
 * @param arg1 a pointer to double
 * @param arg2 a struct with two doubles
 * @return the sum of doubles
 */
double
addDoubleFromPointerAndDoublesFromStruct(double *arg1, stru_2_Doubles arg2)
{
	double doubleSum = *arg1 + arg2.elem1 + arg2.elem2;
	return doubleSum;
}

/**
 * Add a double (dereferenced from a pointer) and two doubles of a struct.
 *
 * @param arg1 a pointer to double
 * @param arg2 a struct with two doubles
 * @return a pointer to the sum of doubles
 */
double *
addDoubleFromPointerAndDoublesFromStruct_returnDoublePointer(double *arg1, stru_2_Doubles arg2)
{
	*arg1 = *arg1 + arg2.elem1 + arg2.elem2;
	return arg1;
}

/**
 * Add a double and two doubles of a struct (dereferenced from a pointer).
 *
 * @param arg1 a double
 * @param arg2 a pointer to struct with two doubles
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromStructPointer(double arg1, stru_2_Doubles *arg2)
{
	double doubleSum = arg1 + arg2->elem1 + arg2->elem2;
	return doubleSum;
}

/**
 * Add a double and all doubles of a struct with a nested struct and a double.
 *
 * @param arg1 a double
 * @param arg2 a struct with a nested struct and a double
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromNestedStruct(double arg1, stru_NestedStruct_Double arg2)
{
	double doubleSum = arg1 + arg2.elem2 + arg2.elem1.elem1 + arg2.elem1.elem2;
	return doubleSum;
}

/**
 * Add a double and all doubles of a struct with a double and a nested struct (in reverse order).
 *
 * @param arg1 a double
 * @param arg2 a struct with a double a nested struct
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromNestedStruct_reverseOrder(double arg1, stru_Double_NestedStruct arg2)
{
	double doubleSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return doubleSum;
}

/**
 * Add a double and all double elements of a struct with a nested double array and a double.
 *
 * @param arg1 a double
 * @param arg2 a struct with a nested double array and a double
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromStructWithNestedDoubleArray(double arg1, stru_NestedDoubleArray_Double arg2)
{
	double doubleSum = arg1 + arg2.elem1[0] + arg2.elem1[1] + arg2.elem2;
	return doubleSum;
}

/**
 * Add a double and all double elements of a struct with a double
 * and a nested double array (in reverse order).
 *
 * @param arg1 a double
 * @param arg2 a struct with a double and a nested double array
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromStructWithNestedDoubleArray_reverseOrder(double arg1, stru_Double_NestedDoubleArray arg2)
{
	double doubleSum = arg1 + arg2.elem2[0] + arg2.elem2[1] + arg2.elem1;
	return doubleSum;
}

/**
 * Add a double and all double elements of a struct with a nested struct array and a double.
 *
 * @param arg1 a double
 * @param arg2 a struct with a nested double array and a double
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromStructWithNestedStructArray(double arg1, stru_NestedStruArray_Double arg2)
{
	double doubleSum = arg1 + arg2.elem2
			+ arg2.elem1[0].elem1 + arg2.elem1[0].elem2
			+ arg2.elem1[1].elem1 + arg2.elem1[1].elem2;
	return doubleSum;
}

/**
 * Add a double and all double elements of a struct with a double
 * and a nested struct array (in reverse order).
 *
 * @param arg1 a double
 * @param arg2 a struct with a double and a nested double array
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromStructWithNestedStructArray_reverseOrder(double arg1, stru_Double_NestedStruArray arg2)
{
	double doubleSum = arg1 + arg2.elem1
			+ arg2.elem2[0].elem1 + arg2.elem2[0].elem2
			+ arg2.elem2[1].elem1 + arg2.elem2[1].elem2;
	return doubleSum;
}

/**
 * Create a new struct by adding each double element of two structs.
 *
 * @param arg1 the 1st struct with two doubles
 * @param arg2 the 2nd struct with two doubles
 * @return a struct with two doubles
 */
stru_2_Doubles
add2DoubleStructs_returnStruct(stru_2_Doubles arg1, stru_2_Doubles arg2)
{
	stru_2_Doubles doubleStruct;
	doubleStruct.elem1 = arg1.elem1 + arg2.elem1;
	doubleStruct.elem2 = arg1.elem2 + arg2.elem2;
	return doubleStruct;
}

/**
 * Get a pointer to struct by adding each double element of two structs.
 *
 * @param arg1 a pointer to the 1st struct with two doubles
 * @param arg2 the 2nd struct with two doubles
 * @return a pointer to struct with two doubles
 */
stru_2_Doubles *
add2DoubleStructs_returnStructPointer(stru_2_Doubles *arg1, stru_2_Doubles arg2)
{
	arg1->elem1 = arg1->elem1 + arg2.elem1;
	arg1->elem2 = arg1->elem2 + arg2.elem2;
	return arg1;
}

/**
 * Create a new struct by adding each double element of two structs.
 *
 * @param arg1 the 1st struct with three doubles
 * @param arg2 the 2nd struct with three doubles
 * @return a struct with three doubles
 */
stru_3_Doubles
add3DoubleStructs_returnStruct(stru_3_Doubles arg1, stru_3_Doubles arg2)
{
	stru_3_Doubles doubleStruct;
	doubleStruct.elem1 = arg1.elem1 + arg2.elem1;
	doubleStruct.elem2 = arg1.elem2 + arg2.elem2;
	doubleStruct.elem3 = arg1.elem3 + arg2.elem3;
	return doubleStruct;
}

/**
 * Validate that a null pointer of struct is successfully passed
 * from java to native in downcall.
 *
 * @param arg1 an integer
 * @param arg2 a pointer to struct with two integers
 * @return the value of arg1
 *
 * Note:
 * arg2 is a null pointer passed from java to native.
 */
int
validateNullAddrArgument(int arg1, stru_2_Ints *arg2)
{
	return arg1;
}

/**
 * Validate the linker option for the trivial downcall.
 *
 * @param arg1 an integer
 * @return the passed-in argument
 */
int
validateTrivialOption(int arg1)
{
	return arg1;
}

/**
 * Add a boolean and all boolean elements of a union with the XOR (^) operator.
 *
 * @param arg1 a boolean
 * @param arg2 a union with two booleans
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromUnionWithXor(bool arg1, union_2_Bools arg2)
{
	bool boolSum = arg1 ^ arg2.elem1 ^ arg2.elem2;
	return boolSum;
}

/**
 * Add a boolean and two booleans of a union (dereferenced from a pointer) with the XOR (^) operator.
 *
 * @param arg1 a boolean
 * @param arg2 a pointer to union with two booleans
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromUnionPtrWithXor(bool arg1, union_2_Bools *arg2)
{
	bool boolSum = arg1 ^ arg2->elem1 ^ arg2->elem2;
	return boolSum;
}

/**
 * Add a boolean and all booleans of a union with a nested union
 * and a boolean with the XOR (^) operator.
 *
 * @param arg1 a boolean
 * @param arg2 a union with a nested union and a boolean
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromNestedUnionWithXor(bool arg1, union_NestedUnion_Bool arg2)
{
	bool boolSum = arg1 ^ arg2.elem1.elem1 ^ arg2.elem1.elem2 ^ arg2.elem2;
	return boolSum;
}

/**
 * Add a boolean and all booleans of a union with a boolean and a nested
 * union (in reverse order) with the XOR (^) operator.
 *
 * @param arg1 a boolean
 * @param arg2 a union with a boolean and a nested union
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromNestedUnionWithXor_reverseOrder(bool arg1, union_Bool_NestedUnion arg2)
{
	bool boolSum = arg1 ^ arg2.elem2.elem1 ^ arg2.elem2.elem2 ^ arg2.elem1;
	return boolSum;
}

/**
 * Add a boolean and all booleans of a union with a nested array
 * and a boolean with the XOR (^) operator.
 *
 * @param arg1 a boolean
 * @param arg2 a union with a nested array and a boolean
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromUnionWithNestedBoolArray(bool arg1, union_NestedBoolArray_Bool arg2)
{
	bool boolSum = arg1 ^ arg2.elem1[0] ^ arg2.elem1[1] ^ arg2.elem2;
	return boolSum;
}

/**
 * Add a boolean and all booleans of a union with a boolean and a nested array
 * (in reverse order) with the XOR (^) operator.
 *
 * @param arg1 a boolean
 * @param arg2 a union with a boolean and a nested array
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromUnionWithNestedBoolArray_reverseOrder(bool arg1, union_Bool_NestedBoolArray arg2)
{
	bool boolSum = arg1 ^ arg2.elem1 ^ arg2.elem2[0] ^ arg2.elem2[1];
	return boolSum;
}

/**
 * Add a boolean and all booleans of a union with a nested union array
 * and a boolean with the XOR (^) operator.
 *
 * @param arg1 a boolean
 * @param arg2 a union with a nested union array and a boolean
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromUnionWithNestedUnionArray(bool arg1, union_NestedUnionArray_Bool arg2)
{
	bool boolSum = arg1 ^ arg2.elem2
			^ arg2.elem1[0].elem1 ^ arg2.elem1[0].elem2
			^ arg2.elem1[1].elem1 ^ arg2.elem1[1].elem2;
	return boolSum;
}

/**
 * Add a boolean and all booleans of a union with a boolean and a nested union array
 * (in reverse order) with the XOR (^) operator.
 *
 * @param arg1 a boolean
 * @param arg2 a union with a boolean and a nested union array
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromUnionWithNestedUnionArray_reverseOrder(bool arg1, union_Bool_NestedUnionArray arg2)
{
	bool boolSum = arg1 ^ arg2.elem1
			^ arg2.elem2[0].elem1 ^ arg2.elem2[0].elem2
			^ arg2.elem2[1].elem1 ^ arg2.elem2[1].elem2;
	return boolSum;
}

/**
 * Get a new union by adding each boolean element of two unions with the XOR (^) operator.
 *
 * @param arg1 the 1st union with two booleans
 * @param arg2 the 2nd union with two booleans
 * @return a union with two booleans
 */
union_2_Bools
add2BoolUnionsWithXor_returnUnion(union_2_Bools arg1, union_2_Bools arg2)
{
	union_2_Bools boolUnion;
	boolUnion.elem1 = arg1.elem1 ^ arg2.elem1;
	boolUnion.elem2 = arg1.elem2 ^ arg2.elem2;
	return boolUnion;
}

/**
 * Get a pointer to union by adding each boolean element of two unions with the XOR (^) operator.
 *
 * @param arg1 a pointer to the 1st union with two booleans
 * @param arg2 the 2nd union with two booleans
 * @return a pointer to union with two booleans
 */
union_2_Bools *
add2BoolUnionsWithXor_returnUnionPtr(union_2_Bools *arg1, union_2_Bools arg2)
{
	arg1->elem1 = arg1->elem1 ^ arg2.elem1;
	arg1->elem2 = arg1->elem2 ^ arg2.elem2;
	return arg1;
}

/**
 * Get a new union by adding each boolean element of two unions with three boolean elements.
 *
 * @param arg1 the 1st union with three booleans
 * @param arg2 the 2nd union with three booleans
 * @return a union with three booleans
 */
union_3_Bools
add3BoolUnionsWithXor_returnUnion(union_3_Bools arg1, union_3_Bools arg2)
{
	union_3_Bools boolUnion;
	boolUnion.elem1 = arg1.elem1 ^ arg2.elem1;
	boolUnion.elem2 = arg1.elem2 ^ arg2.elem2;
	boolUnion.elem3 = arg1.elem3 ^ arg2.elem3;
	return boolUnion;
}

/**
 * Add a byte and two bytes of a union.
 *
 * @param arg1 a byte
 * @param arg2 a union with two bytes
 * @return the sum of bytes
 */
char
addByteAndBytesFromUnion(char arg1, union_2_Bytes arg2)
{
	char byteSum = arg1 + arg2.elem1 + arg2.elem2;
	return byteSum;
}

/**
 * Add a byte and two bytes of a union (dereferenced from a pointer).
 *
 * @param arg1 a byte
 * @param arg2 a pointer to union with two bytes
 * @return the sum of bytes
 */
char
addByteAndBytesFromUnionPtr(char arg1, union_2_Bytes *arg2)
{
	char byteSum = arg1 + arg2->elem1 + arg2->elem2;
	return byteSum;
}

/**
 * Add a byte and all bytes of a union with a nested union and a byte.
 *
 * @param arg1 a byte
 * @param arg2 a union with a nested union and a byte
 * @return the sum of bytes
 */
char
addByteAndBytesFromNestedUnion(char arg1, union_NestedUnion_Byte arg2)
{
	char byteSum = arg1 + arg2.elem2 + arg2.elem1.elem1 + arg2.elem1.elem2;
	return byteSum;
}

/**
 * Add a byte and all bytes of a union with a byte and a nested union (in reverse order).
 *
 * @param arg1 a byte
 * @param arg2 a union with a byte and a nested union
 * @return the sum of bytes
 */
char
addByteAndBytesFromNestedUnion_reverseOrder(char arg1, union_Byte_NestedUnion arg2)
{
	char byteSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return byteSum;
}


/**
 * Add a byte and all byte elements of a union with a nested byte array and a byte.
 *
 * @param arg1 a byte
 * @param arg2 a union with a nested byte array and a byte
 * @return the sum of bytes
 */
char
addByteAndBytesFromUnionWithNestedByteArray(char arg1, union_NestedByteArray_Byte arg2)
{
	char byteSum = arg1 + arg2.elem1[0] + arg2.elem1[1] + arg2.elem2;
	return byteSum;
}

/**
 * Add a byte and all byte elements of a union with a byte
 * and a nested byte array (in reverse order).
 *
 * @param arg1 a byte
 * @param arg2 a union with a byte and a nested byte array
 * @return the sum of bytes
 */
char
addByteAndBytesFromUnionWithNestedByteArray_reverseOrder(char arg1, union_Byte_NestedByteArray arg2)
{
	char byteSum = arg1 + arg2.elem2[0] + arg2.elem2[1] + arg2.elem1;
	return byteSum;
}

/**
 * Add a byte and all byte elements of a union with a nested union array and a byte.
 *
 * @param arg1 a byte
 * @param arg2 a union with a nested union array and a byte
 * @return the sum of bytes
 */
char
addByteAndBytesFromUnionWithNestedUnionArray(char arg1, union_NestedUnionArray_Byte arg2)
{
	char byteSum = arg1 + arg2.elem2
			+ arg2.elem1[0].elem1 + arg2.elem1[0].elem2
			+ arg2.elem1[1].elem1 + arg2.elem1[1].elem2;
	return byteSum;
}

/**
 * Add a byte and all byte elements of a union with a byte
 * and a nested union array (in reverse order).
 *
 * @param arg1 a byte
 * @param arg2 a union with a byte and a nested byte array
 * @return the sum of bytes
 */
char
addByteAndBytesFromUnionWithNestedUnionArray_reverseOrder(char arg1, union_Byte_NestedUnionArray arg2)
{
	char byteSum = arg1 + arg2.elem1
			+ arg2.elem2[0].elem1 + arg2.elem2[0].elem2
			+ arg2.elem2[1].elem1 + arg2.elem2[1].elem2;
	return byteSum;
}

/**
 * Get a new union by adding each byte element of two unions with two byte elements.
 *
 * @param arg1 the 1st union with two bytes
 * @param arg2 the 2nd union with two bytes
 * @return a union with two bytes
 */
union_2_Bytes
add2ByteUnions_returnUnion(union_2_Bytes arg1, union_2_Bytes arg2)
{
	union_2_Bytes byteUnion;
	byteUnion.elem1 = arg1.elem1 + arg2.elem1;
	byteUnion.elem2 = arg1.elem2 + arg2.elem2;
	return byteUnion;
}

/**
 * Get a pointer to union by adding each byte element of two unions with two byte elements.
 *
 * @param arg1 a pointer to the 1st union with two bytes
 * @param arg2 the 2nd union with two bytes
 * @return a pointer to union with two bytes
 */
union_2_Bytes *
add2ByteUnions_returnUnionPtr(union_2_Bytes *arg1, union_2_Bytes arg2)
{
	arg1->elem1 = arg1->elem1 + arg2.elem1;
	arg1->elem2 = arg1->elem2 + arg2.elem2;
	return arg1;
}

/**
 * Get a new union by adding each byte element of two unions with three byte elements.
 *
 * @param arg1 the 1st union with three bytes
 * @param arg2 the 2nd union with three bytes
 * @return a union with three bytes
 */
union_3_Bytes
add3ByteUnions_returnUnion(union_3_Bytes arg1, union_3_Bytes arg2)
{
	union_3_Bytes byteUnion;
	byteUnion.elem1 = arg1.elem1 + arg2.elem1;
	byteUnion.elem2 = arg1.elem2 + arg2.elem2;
	byteUnion.elem3 = arg1.elem3 + arg2.elem3;
	return byteUnion;
}

/**
 * Generate a new char by adding a char and two chars of a union.
 *
 * @param arg1 a char
 * @param arg2 a union with two chars
 * @return a new char
 */
short
addCharAndCharsFromUnion(short arg1, union_2_Chars arg2)
{
	short result = arg1 + arg2.elem1 + arg2.elem2 - (2 * 'A');
	return result;
}

/**
 * Generate a new char by adding a char and two chars
 * of union (dereferenced from a pointer).
 *
 * @param arg1 a char
 * @param arg2 a pointer to union with two chars
 * @return a new char
 */
short
addCharAndCharsFromUnionPtr(short arg1, union_2_Chars *arg2)
{
	short result = arg1 + arg2->elem1 + arg2->elem2 - (2 * 'A');
	return result;
}

/**
 * Generate a new char by adding a char and all char elements of
 * a union with a nested union and a char.
 *
 * @param arg1 a char
 * @param arg2 a union with a nested union
 * @return a new char
 */
short
addCharAndCharsFromNestedUnion(short arg1, union_NestedUnion_Char arg2)
{
	short result = arg1 + arg2.elem2 + arg2.elem1.elem1 + arg2.elem1.elem2 - (3 * 'A');
	return result;
}

/**
 * Generate a new char by adding a char and all char elements of a union
 * with a char and a nested union (in reverse order).
 *
 * @param arg1 a char
 * @param arg2 a union with a char and a nested union
 * @return a new char
 */
short
addCharAndCharsFromNestedUnion_reverseOrder(short arg1, union_Char_NestedUnion arg2)
{
	short result = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2 - (3 * 'A');
	return result;
}

/**
 * Generate a new char by adding a char and all char elements
 * of a union with a nested char array and a char.
 *
 * @param arg1 a char
 * @param arg2 a union with a nested char array and a char
 * @return a new char
 */
short
addCharAndCharsFromUnionWithNestedCharArray(short arg1, union_NestedCharArray_Char arg2)
{
	short result = arg1 + arg2.elem1[0] + arg2.elem1[1] + arg2.elem2 - (3 * 'A');
	return result;
}

/**
 * Generate a new char by adding a char and all char elements of a union
 * with a char and a nested char array (in reverse order).
 *
 * @param arg1 a char
 * @param arg2 a union with a char and a nested char array
 * @return a new char
 */
short
addCharAndCharsFromUnionWithNestedCharArray_reverseOrder(short arg1, union_Char_NestedCharArray arg2)
{
	short result = arg1 + arg2.elem2[0] + arg2.elem2[1] + arg2.elem1 - (3 * 'A');
	return result;
}

/**
 * Generate a new char by adding a char and all char elements of a union
 * with a nested union array and a char.
 *
 * @param arg1 a char
 * @param arg2 a union with a nested char array and a char
 * @return a new char
 */
short
addCharAndCharsFromUnionWithNestedUnionArray(short arg1, union_NestedUnionArray_Char arg2)
{
	short result = arg1 + arg2.elem2
			+ arg2.elem1[0].elem1 + arg2.elem1[0].elem2
			+ arg2.elem1[1].elem1 + arg2.elem1[1].elem2 - (5 * 'A');
	return result;
}

/**
 * Generate a new char by adding a char and all char elements of a union with a char
 * and a nested union array (in reverse order).
 *
 * @param arg1 a char
 * @param arg2 a union with a char and a nested char array
 * @return a new char
 */
short
addCharAndCharsFromUnionWithNestedUnionArray_reverseOrder(short arg1, union_Char_NestedUnionArray arg2)
{
	short result = arg1 + arg2.elem1
			+ arg2.elem2[0].elem1 + arg2.elem2[0].elem2
			+ arg2.elem2[1].elem1 + arg2.elem2[1].elem2 - (5 * 'A');
	return result;
}

/**
 * Create a new union by adding each char element of two unions.
 *
 * @param arg1 the 1st union with two chars
 * @param arg2 the 2nd union with two chars
 * @return a new union of with two chars
 */
union_2_Chars
add2CharUnions_returnUnion(union_2_Chars arg1, union_2_Chars arg2)
{
	union_2_Chars charUnion;
	charUnion.elem1 = arg1.elem1 + arg2.elem1 - 'A';
	charUnion.elem2 = arg1.elem2 + arg2.elem2 - 'A';
	return charUnion;
}

/**
 * Get a pointer to a union by adding each element of two unions.
 *
 * @param arg1 a pointer to the 1st union with two chars
 * @param arg2 the 2nd union with two chars
 * @return a pointer to a union of with two chars
 */
union_2_Chars *
add2CharUnions_returnUnionPtr(union_2_Chars *arg1, union_2_Chars arg2)
{
	arg1->elem1 = arg1->elem1 + arg2.elem1 - 'A';
	arg1->elem2 = arg1->elem2 + arg2.elem2 - 'A';
	return arg1;
}

/**
 * Create a new union by adding each char element of two unions with three chars.
 *
 * @param arg1 the 1st union with three chars
 * @param arg2 the 2nd union with three chars
 * @return a new union of with three chars
 */
union_3_Chars
add3CharUnions_returnUnion(union_3_Chars arg1, union_3_Chars arg2)
{
	union_3_Chars charUnion;
	charUnion.elem1 = arg1.elem1 + arg2.elem1 - 'A';
	charUnion.elem2 = arg1.elem2 + arg2.elem2 - 'A';
	charUnion.elem3 = arg1.elem3 + arg2.elem3 - 'A';
	return charUnion;
}

/**
 * Add a short and two shorts of a union.
 *
 * @param arg1 a short
 * @param arg2 a union with two shorts
 * @return the sum of shorts
 */
short
addShortAndShortsFromUnion(short arg1, union_2_Shorts arg2)
{
	short shortSum = arg1 + arg2.elem1 + arg2.elem2;
	return shortSum;
}

/**
 * Add a short and two shorts of a union (dereferenced from a pointer).
 *
 * @param arg1 a short
 * @param arg2 a pointer to union with two shorts
 * @return the sum of shorts
 */
short
addShortAndShortsFromUnionPtr(short arg1, union_2_Shorts *arg2)
{
	short shortSum = arg1 + arg2->elem1 + arg2->elem2;
	return shortSum;
}

/**
 * Add a short and all short elements of a union with a nested union and a short.
 *
 * @param arg1 a short
 * @param arg2 a union with a nested union and a short
 * @return the sum of shorts
 */
short
addShortAndShortsFromNestedUnion(short arg1, union_NestedUnion_Short arg2)
{
	short shortSum = arg1 + arg2.elem2 + arg2.elem1.elem1 + arg2.elem1.elem2;
	return shortSum;
}

/**
 * Add a short and all short elements of a union with a short
 * and a nested union (in reverse order).
 *
 * @param arg1 a short
 * @param arg2 a union with a short and a nested union
 * @return the sum of shorts
 */
short
addShortAndShortsFromNestedUnion_reverseOrder(short arg1, union_Short_NestedUnion arg2)
{
	short shortSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return shortSum;
}

/**
 * Add a short and all short elements of a union with a nested short array and a short.
 *
 * @param arg1 a short
 * @param arg2 a union with a nested short array and a short
 * @return the sum of shorts
 */
short
addShortAndShortsFromUnionWithNestedShortArray(short arg1, union_NestedShortArray_Short arg2)
{
	short shortSum = arg1 + arg2.elem1[0] + arg2.elem1[1] + arg2.elem2;
	return shortSum;
}

/**
 * Add a short and all short elements of a union with a short
 * and a nested short array (in reverse order).
 *
 * @param arg1 a short
 * @param arg2 a union with a short and a nested short array
 * @return the sum of shorts
 */
short
addShortAndShortsFromUnionWithNestedShortArray_reverseOrder(short arg1, union_Short_NestedShortArray arg2)
{
	short shortSum = arg1 + arg2.elem2[0] + arg2.elem2[1] + arg2.elem1;
	return shortSum;
}

/**
 * Add a short and all short elements of a union with a nested union array and a short.
 *
 * @param arg1 a short
 * @param arg2 a union with a nested short array and a short
 * @return the sum of shorts
 */
short
addShortAndShortsFromUnionWithNestedUnionArray(short arg1, union_NestedUnionArray_Short arg2)
{
	short shortSum = arg1 + arg2.elem2
			+ arg2.elem1[0].elem1 + arg2.elem1[0].elem2
			+ arg2.elem1[1].elem1 + arg2.elem1[1].elem2;
	return shortSum;
}

/**
 * Add a short and all short elements of a union with a short
 * and a nested union array (in reverse order).
 *
 * @param arg1 a short
 * @param arg2 a union with a short and a nested short array
 * @return the sum of shorts
 */
short
addShortAndShortsFromUnionWithNestedUnionArray_reverseOrder(short arg1, union_Short_NestedUnionArray arg2)
{
	short shortSum = arg1 + arg2.elem1
			+ arg2.elem2[0].elem1 + arg2.elem2[0].elem2
			+ arg2.elem2[1].elem1 + arg2.elem2[1].elem2;
	return shortSum;
}

/**
 * Get a new union by adding each short element of two unions with two short elements.
 *
 * @param arg1 the 1st union with two shorts
 * @param arg2 the 2nd union with two shorts
 * @return a union with two shorts
 */
union_2_Shorts
add2ShortUnions_returnUnion(union_2_Shorts arg1, union_2_Shorts arg2)
{
	union_2_Shorts shortUnion;
	shortUnion.elem1 = arg1.elem1 + arg2.elem1;
	shortUnion.elem2 = arg1.elem2 + arg2.elem2;
	return shortUnion;
}

/**
 * Get a pointer to union by adding each short element of two unions with two short elements.
 *
 * @param arg1 a pointer to the 1st union with two shorts
 * @param arg2 the 2nd union with two shorts
 * @return a pointer to union with two shorts
 */
union_2_Shorts *
add2ShortUnions_returnUnionPtr(union_2_Shorts *arg1, union_2_Shorts arg2)
{
	arg1->elem1 = arg1->elem1 + arg2.elem1;
	arg1->elem2 = arg1->elem2 + arg2.elem2;
	return arg1;
}

/**
 * Get a new union by adding each short element of two unions with three short elements.
 *
 * @param arg1 the 1st union with three shorts
 * @param arg2 the 2nd union with three shorts
 * @return a union with three shorts
 */
union_3_Shorts
add3ShortUnions_returnUnion(union_3_Shorts arg1, union_3_Shorts arg2)
{
	union_3_Shorts shortUnion;
	shortUnion.elem1 = arg1.elem1 + arg2.elem1;
	shortUnion.elem2 = arg1.elem2 + arg2.elem2;
	shortUnion.elem3 = arg1.elem3 + arg2.elem3;
	return shortUnion;
}

/**
 * Add an integer and two integers of a union.
 *
 * @param arg1 an integer
 * @param arg2 a union with two integers
 * @return the sum of integers
 */
int
addIntAndIntsFromUnion(int arg1, union_2_Ints arg2)
{
	int intSum = arg1 + arg2.elem1 + arg2.elem2;
	return intSum;
}

/**
 * Add an integer and two integers of a union (dereferenced from a pointer).
 *
 * @param arg1 an integer
 * @param arg2 a pointer to union with two integers
 * @return the sum of integers
 */
int
addIntAndIntsFromUnionPtr(int arg1, union_2_Ints *arg2)
{
	int intSum = arg1 + arg2->elem1 + arg2->elem2;
	return intSum;
}

/**
 * Add an integer and all integer elements of a union
 * with a nested union and an integer.
 *
 * @param arg1 an integer
 * @param arg2 a union with a nested union and an integer
 * @return the sum of integers
 */
int
addIntAndIntsFromNestedUnion(int arg1, union_NestedUnion_Int arg2)
{
	int intSum = arg1 + arg2.elem1.elem1 + arg2.elem1.elem2 + arg2.elem2;
	return intSum;
}

/**
 * Add an integer and all integer elements of a union
 * with an integer and a nested union (in reverse order).
 *
 * @param arg1 an integer
 * @param arg2 a union with an integer and a nested union
 * @return the sum of these integers
 */
int
addIntAndIntsFromNestedUnion_reverseOrder(int arg1, union_Int_NestedUnion arg2)
{
	int intSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return intSum;
}

/**
 * Add an integer and all integer elements of a union with a nested integer array and an integer.
 *
 * @param arg1 an integer
 * @param arg2 a union with a nested integer array and an integer
 * @return the sum of integers
 */
int
addIntAndIntsFromUnionWithNestedIntArray(int arg1, union_NestedIntArray_Int arg2)
{
	int intSum = arg1 + arg2.elem1[0] + arg2.elem1[1] + arg2.elem2;
	return intSum;
}

/**
 * Add an integer and all integer elements of a union with an integer
 * and a nested integer array (in reverse order).
 *
 * @param arg1 an integer
 * @param arg2 a union with an integer and a nested integer array
 * @return the sum of integers
 */
int
addIntAndIntsFromUnionWithNestedIntArray_reverseOrder(int arg1, union_Int_NestedIntArray arg2)
{
	int intSum = arg1 + arg2.elem2[0] + arg2.elem2[1] + arg2.elem1;
	return intSum;
}

/**
 * Add an integer and all integer elements of a union with a nested union array and an integer.
 *
 * @param arg1 an integer
 * @param arg2 a union with a nested integer array and an integer
 * @return the sum of integer and all elements of the union
 */
int
addIntAndIntsFromUnionWithNestedUnionArray(int arg1, union_NestedUnionArray_Int arg2)
{
	int intSum = arg1 + arg2.elem2
			+ arg2.elem1[0].elem1 + arg2.elem1[0].elem2
			+ arg2.elem1[1].elem1 + arg2.elem1[1].elem2;
	return intSum;
}

/**
 * Add an integer and all integer elements of a union with an integer
 * and a nested union array (in reverse order).
 *
 * @param arg1 an integer
 * @param arg2 a union with an integer and a nested integer array
 * @return the sum of integer and all elements of the union
 */
int
addIntAndIntsFromUnionWithNestedUnionArray_reverseOrder(int arg1, union_Int_NestedUnionArray arg2)
{
	int intSum = arg1 + arg2.elem1
			+ arg2.elem2[0].elem1 + arg2.elem2[0].elem2
			+ arg2.elem2[1].elem1 + arg2.elem2[1].elem2;
	return intSum;
}

/**
 * Get a new union by adding each integer element of two unions.
 *
 * @param arg1 the 1st union with two integers
 * @param arg2 the 2nd union with two integers
 * @return a union with two integers
 */
union_2_Ints
add2IntUnions_returnUnion(union_2_Ints arg1, union_2_Ints arg2)
{
	union_2_Ints intUnion;
	intUnion.elem1 = arg1.elem1 + arg2.elem1;
	intUnion.elem2 = arg1.elem2 + arg2.elem2;
	return intUnion;
}

/**
 * Get a pointer to union by adding each integer element of two unions.
 *
 * @param arg1 a pointer to the 1st union with two integers
 * @param arg2 the 2nd union with two integers
 * @return a pointer to union with two integers
 */
union_2_Ints *
add2IntUnions_returnUnionPtr(union_2_Ints *arg1, union_2_Ints arg2)
{
	arg1->elem1 = arg1->elem1 + arg2.elem1;
	arg1->elem2 = arg1->elem2 + arg2.elem2;
	return arg1;
}

/**
 * Get a new union by adding each integer element of two unions.
 *
 * @param arg1 the 1st union with three integers
 * @param arg2 the 2nd union with three integers
 * @return a union with three integers
 */
union_3_Ints
add3IntUnions_returnUnion(union_3_Ints arg1, union_3_Ints arg2)
{
	union_3_Ints intUnion;
	intUnion.elem1 = arg1.elem1 + arg2.elem1;
	intUnion.elem2 = arg1.elem2 + arg2.elem2;
	intUnion.elem3 = arg1.elem3 + arg2.elem3;
	return intUnion;
}

/**
 * Add a long and two longs of a union.
 *
 * @param arg1 a long
 * @param arg2 a union with two longs
 * @return the sum of longs
 */
LONG
addLongAndLongsFromUnion(LONG arg1, union_2_Longs arg2)
{
	LONG longSum = arg1 + arg2.elem1 + arg2.elem2;
	return longSum;
}

/**
 * Add a long and two longs of a union (dereferenced from a pointer).
 *
 * @param arg1 a long
 * @param arg2 a pointer to union with two longs
 * @return the sum of longs
 */
LONG
addLongAndLongsFromUnionPtr(LONG arg1, union_2_Longs *arg2)
{
	LONG longSum = arg1 + arg2->elem1 + arg2->elem2;
	return longSum;
}

/**
 * Add a long and all long elements of a union with a nested union and a long.
 *
 * @param arg1 a long
 * @param arg2 a union with a nested union and long
 * @return the sum of longs
 */
LONG
addLongAndLongsFromNestedUnion(LONG arg1, union_NestedUnion_Long arg2)
{
	LONG longSum = arg1 + arg2.elem2 + arg2.elem1.elem1 + arg2.elem1.elem2;
	return longSum;
}

/**
 * Add a long and all long elements of a union with
 * a long and a nested union (in reverse order).
 *
 * @param arg1 a long
 * @param arg2 a union with a long and a nested union
 * @return the sum of longs
 */
LONG
addLongAndLongsFromNestedUnion_reverseOrder(LONG arg1, union_Long_NestedUnion arg2)
{
	LONG longSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return longSum;
}

/**
 * Add a long and all long elements of a union with a nested long array and a long.
 *
 * @param arg1 a long
 * @param arg2 a union with a nested long array and a long
 * @return the sum of longs
 */
LONG
addLongAndLongsFromUnionWithNestedLongArray(LONG arg1, union_NestedLongArray_Long arg2)
{
	LONG longSum = arg1 + arg2.elem1[0] + arg2.elem1[1] + arg2.elem2;
	return longSum;
}

/**
 * Add a long and all long elements of a union with a long
 * and a nested long array (in reverse order).
 *
 * @param arg1 a long
 * @param arg2 a union with a long and a nested long array
 * @return the sum of longs
 */
LONG
addLongAndLongsFromUnionWithNestedLongArray_reverseOrder(LONG arg1, union_Long_NestedLongArray arg2)
{
	LONG longSum = arg1 + arg2.elem2[0] + arg2.elem2[1] + arg2.elem1;
	return longSum;
}

/**
 * Add a long and all long elements of a union with a nested union array and a long.
 *
 * @param arg1 a long
 * @param arg2 a union with a nested long array and a long
 * @return the sum of long and all elements of the union
 */
LONG
addLongAndLongsFromUnionWithNestedUnionArray(LONG arg1, union_NestedUnionArray_Long arg2)
{
	LONG longSum = arg1 + arg2.elem2
			+ arg2.elem1[0].elem1 + arg2.elem1[0].elem2
			+ arg2.elem1[1].elem1 + arg2.elem1[1].elem2;
	return longSum;
}

/**
 * Add a long and all long elements of a union with a long
 * and a nested union array (in reverse order).
 *
 * @param arg1 a long
 * @param arg2 a union with a long and a nested long array
 * @return the sum of long and all elements of the union
 */
LONG
addLongAndLongsFromUnionWithNestedUnionArray_reverseOrder(LONG arg1, union_Long_NestedUnionArray arg2)
{
	LONG longSum = arg1 + arg2.elem1
			+ arg2.elem2[0].elem1 + arg2.elem2[0].elem2
			+ arg2.elem2[1].elem1 + arg2.elem2[1].elem2;
	return longSum;
}

/**
 * Get a new union by adding each long element of two unions.
 *
 * @param arg1 the 1st union with two longs
 * @param arg2 the 2nd union with two longs
 * @return a union with two longs
 */
union_2_Longs
add2LongUnions_returnUnion(union_2_Longs arg1, union_2_Longs arg2)
{
	union_2_Longs longUnion;
	longUnion.elem1 = arg1.elem1 + arg2.elem1;
	longUnion.elem2 = arg1.elem2 + arg2.elem2;
	return longUnion;
}

/**
 * Get a pointer to union by adding each long element of two unions.
 *
 * @param arg1 a pointer to the 1st union with two longs
 * @param arg2 the 2nd union with two longs
 * @return a pointer to union with two longs
 */
union_2_Longs *
add2LongUnions_returnUnionPtr(union_2_Longs *arg1, union_2_Longs arg2)
{
	arg1->elem1 = arg1->elem1 + arg2.elem1;
	arg1->elem2 = arg1->elem2 + arg2.elem2;
	return arg1;
}

/**
 * Get a new union by adding each long element of two unions.
 *
 * @param arg1 the 1st union with three longs
 * @param arg2 the 2nd union with three longs
 * @return a union with three longs
 */
union_3_Longs
add3LongUnions_returnUnion(union_3_Longs arg1, union_3_Longs arg2)
{
	union_3_Longs longUnion;
	longUnion.elem1 = arg1.elem1 + arg2.elem1;
	longUnion.elem2 = arg1.elem2 + arg2.elem2;
	longUnion.elem3 = arg1.elem3 + arg2.elem3;
	return longUnion;
}

/**
 * Add a float and two floats of a union.
 *
 * @param arg1 a float
 * @param arg2 a union with two floats
 * @return the sum of floats
 */
float
addFloatAndFloatsFromUnion(float arg1, union_2_Floats arg2)
{
	float floatSum = arg1 + arg2.elem1 + arg2.elem2;
	return floatSum;
}

/**
 * Add a float and two floats of a union (dereferenced from a pointer).
 *
 * @param arg1 a float
 * @param arg2 a pointer to union with two floats
 * @return the sum of floats
 */
float
addFloatAndFloatsFromUnionPtr(float arg1, union_2_Floats *arg2)
{
	float floatSum = arg1 + arg2->elem1 + arg2->elem2;
	return floatSum;
}

/**
 * Add a float and all float elements of a union with a nested union and a float.
 *
 * @param arg1 a float
 * @param arg2 a union with a nested union and a float
 * @return the sum of floats
 */
float
addFloatAndFloatsFromNestedUnion(float arg1, union_NestedUnion_Float arg2)
{
	float floatSum = arg1 + arg2.elem2 + arg2.elem1.elem1 + arg2.elem1.elem2;
	return floatSum;
}

/**
 * Add a float and all float elements of a union with a float and a nested union (in reverse order).
 *
 * @param arg1 a float
 * @param arg2 a union with a float and a nested union
 * @return the sum of floats
 */
float
addFloatAndFloatsFromNestedUnion_reverseOrder(float arg1, union_Float_NestedUnion arg2)
{
	float floatSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return floatSum;
}

/**
 * Add a float and all float elements of a union with a nested float array and a float.
 *
 * @param arg1 a float
 * @param arg2 a union with a nested float array and a float
 * @return the sum of floats
 */
float
addFloatAndFloatsFromUnionWithNestedFloatArray(float arg1, union_NestedFloatArray_Float arg2)
{
	float floatSum = arg1 + arg2.elem1[0] + arg2.elem1[1] + arg2.elem2;
	return floatSum;
}

/**
 * Add a float and all float elements of a union with a float
 * and a nested float array (in reverse order).
 *
 * @param arg1 a float
 * @param arg2 a union with a float and a nested float array
 * @return the sum of floats
 */
float
addFloatAndFloatsFromUnionWithNestedFloatArray_reverseOrder(float arg1, union_Float_NestedFloatArray arg2)
{
	float floatSum = arg1 + arg2.elem2[0] + arg2.elem2[1] + arg2.elem1;
	return floatSum;
}

/**
 * Add a float and all float elements of a union with a nested union array and a float.
 *
 * @param arg1 a float
 * @param arg2 a union with a nested float array and a float
 * @return the sum of floats
 */
float
addFloatAndFloatsFromUnionWithNestedUnionArray(float arg1, union_NestedUnionArray_Float arg2)
{
	float floatSum = arg1 + arg2.elem2
			+ arg2.elem1[0].elem1 + arg2.elem1[0].elem2
			+ arg2.elem1[1].elem1 + arg2.elem1[1].elem2;
	return floatSum;
}

/**
 * Add a float and all float elements of a union with a float
 * and a nested union array (in reverse order).
 *
 * @param arg1 a float
 * @param arg2 a union with a float and a nested float array
 * @return the sum of floats
 */
float
addFloatAndFloatsFromUnionWithNestedUnionArray_reverseOrder(float arg1, union_Float_NestedUnionArray arg2)
{
	float floatSum = arg1 + arg2.elem1
			+ arg2.elem2[0].elem1 + arg2.elem2[0].elem2
			+ arg2.elem2[1].elem1 + arg2.elem2[1].elem2;
	return floatSum;
}

/**
 * Create a new union by adding each float element of two unions.
 *
 * @param arg1 the 1st union with two floats
 * @param arg2 the 2nd union with two floats
 * @return a union with two floats
 */
union_2_Floats
add2FloatUnions_returnUnion(union_2_Floats arg1, union_2_Floats arg2)
{
	union_2_Floats floatUnion;
	floatUnion.elem1 = arg1.elem1 + arg2.elem1;
	floatUnion.elem2 = arg1.elem2 + arg2.elem2;
	return floatUnion;
}

/**
 * Get a pointer to union by adding each float element of two unions.
 *
 * @param arg1 a pointer to the 1st union with two floats
 * @param arg2 the 2nd union with two floats
 * @return a pointer to union with two floats
 */
union_2_Floats *
add2FloatUnions_returnUnionPtr(union_2_Floats *arg1, union_2_Floats arg2)
{
	arg1->elem1 = arg1->elem1 + arg2.elem1;
	arg1->elem2 = arg1->elem2 + arg2.elem2;
	return arg1;
}

/**
 * Create a new union by adding each float element of two unions.
 *
 * @param arg1 the 1st union with three floats
 * @param arg2 the 2nd union with three floats
 * @return a union with three floats
 */
union_3_Floats
add3FloatUnions_returnUnion(union_3_Floats arg1, union_3_Floats arg2)
{
	union_3_Floats floatUnion;
	floatUnion.elem1 = arg1.elem1 + arg2.elem1;
	floatUnion.elem2 = arg1.elem2 + arg2.elem2;
	floatUnion.elem3 = arg1.elem3 + arg2.elem3;
	return floatUnion;
}

/**
 * Add a double and two doubles of a union.
 *
 * @param arg1 a double
 * @param arg2 a union with two doubles
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromUnion(double arg1, union_2_Doubles arg2)
{
	double doubleSum = arg1 + arg2.elem1 + arg2.elem2;
	return doubleSum;
}

/**
 * Add a double and two doubles of a union (dereferenced from a pointer).
 *
 * @param arg1 a double
 * @param arg2 a pointer to union with two doubles
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromUnionPtr(double arg1, union_2_Doubles *arg2)
{
	double doubleSum = arg1 + arg2->elem1 + arg2->elem2;
	return doubleSum;
}

/**
 * Add a double and all doubles of a union with a nested union and a double.
 *
 * @param arg1 a double
 * @param arg2 a union with a nested union and a double
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromNestedUnion(double arg1, union_NestedUnion_Double arg2)
{
	double doubleSum = arg1 + arg2.elem2 + arg2.elem1.elem1 + arg2.elem1.elem2;
	return doubleSum;
}

/**
 * Add a double and all doubles of a union with a double and a nested union (in reverse order).
 *
 * @param arg1 a double
 * @param arg2 a union with a double a nested union
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromNestedUnion_reverseOrder(double arg1, union_Double_NestedUnion arg2)
{
	double doubleSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return doubleSum;
}

/**
 * Add a double and all double elements of a union with a nested double array and a double.
 *
 * @param arg1 a double
 * @param arg2 a union with a nested double array and a double
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromUnionWithNestedDoubleArray(double arg1, union_NestedDoubleArray_Double arg2)
{
	double doubleSum = arg1 + arg2.elem1[0] + arg2.elem1[1] + arg2.elem2;
	return doubleSum;
}

/**
 * Add a double and all double elements of a union with a double
 * and a nested double array (in reverse order).
 *
 * @param arg1 a double
 * @param arg2 a union with a double and a nested double array
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromUnionWithNestedDoubleArray_reverseOrder(double arg1, union_Double_NestedDoubleArray arg2)
{
	double doubleSum = arg1 + arg2.elem2[0] + arg2.elem2[1] + arg2.elem1;
	return doubleSum;
}

/**
 * Add a double and all double elements of a union with a nested union array and a double.
 *
 * @param arg1 a double
 * @param arg2 a union with a nested double array and a double
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromUnionWithNestedUnionArray(double arg1, union_NestedUnionArray_Double arg2)
{
	double doubleSum = arg1 + arg2.elem2
			+ arg2.elem1[0].elem1 + arg2.elem1[0].elem2
			+ arg2.elem1[1].elem1 + arg2.elem1[1].elem2;
	return doubleSum;
}

/**
 * Add a double and all double elements of a union with a double
 * and a nested union array (in reverse order).
 *
 * @param arg1 a double
 * @param arg2 a union with a double and a nested double array
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromUnionWithNestedUnionArray_reverseOrder(double arg1, union_Double_NestedUnionArray arg2)
{
	double doubleSum = arg1 + arg2.elem1
			+ arg2.elem2[0].elem1 + arg2.elem2[0].elem2
			+ arg2.elem2[1].elem1 + arg2.elem2[1].elem2;
	return doubleSum;
}

/**
 * Create a new union by adding each double element of two unions.
 *
 * @param arg1 the 1st union with two doubles
 * @param arg2 the 2nd union with two doubles
 * @return a union with two doubles
 */
union_2_Doubles
add2DoubleUnions_returnUnion(union_2_Doubles arg1, union_2_Doubles arg2)
{
	union_2_Doubles doubleUnion;
	doubleUnion.elem1 = arg1.elem1 + arg2.elem1;
	doubleUnion.elem2 = arg1.elem2 + arg2.elem2;
	return doubleUnion;
}

/**
 * Get a pointer to union by adding each double element of two unions.
 *
 * @param arg1 a pointer to the 1st union with two doubles
 * @param arg2 the 2nd union with two doubles
 * @return a pointer to union with two doubles
 */
union_2_Doubles *
add2DoubleUnions_returnUnionPtr(union_2_Doubles *arg1, union_2_Doubles arg2)
{
	arg1->elem1 = arg1->elem1 + arg2.elem1;
	arg1->elem2 = arg1->elem2 + arg2.elem2;
	return arg1;
}

/**
 * Create a new union by adding each double element of two unions.
 *
 * @param arg1 the 1st union with three doubles
 * @param arg2 the 2nd union with three doubles
 * @return a union with three doubles
 */
union_3_Doubles
add3DoubleUnions_returnUnion(union_3_Doubles arg1, union_3_Doubles arg2)
{
	union_3_Doubles doubleUnion;
	doubleUnion.elem1 = arg1.elem1 + arg2.elem1;
	doubleUnion.elem2 = arg1.elem2 + arg2.elem2;
	doubleUnion.elem3 = arg1.elem3 + arg2.elem3;
	return doubleUnion;
}

/**
 * Add a short and a short of a union (byte & short).
 *
 * @param arg1 a short
 * @param arg2 a union with a byte and a short
 * @return the sum of shorts
 */
short
addShortAndShortFromUnionWithByteShort(short arg1, union_Byte_Short arg2)
{
	short shortSum = arg1 + arg2.elem2;
	return shortSum;
}

/**
 * Add a short and a short of a union (short & byte).
 *
 * @param arg1 a short
 * @param arg2 a union with a short and a byte
 * @return the sum of shorts
 */
short
addShortAndShortFromUnionWithShortByte_reverseOrder(short arg1, union_Short_Byte arg2)
{
	short shortSum = arg1 + arg2.elem1;
	return shortSum;
}

/**
 * Add an int and a byte of a union (integer & byte).
 *
 * @param arg1 an int
 * @param arg2 a union with an int and a byte
 * @return the sum of int and byte
 */
int
addIntAndByteFromUnionWithIntByte(int arg1, union_Int_Byte arg2)
{
	int intSum = arg1 + arg2.elem2;
	return intSum;
}

/**
 * Add an int and an int of a union (byte & int).
 *
 * @param arg1 an int
 * @param arg2 a union with a byte and an int
 * @return the sum of ints
 */
int
addIntAndIntFromUnionWithByteInt(int arg1, union_Byte_Int arg2)
{
	int intSum = arg1 + arg2.elem2;
	return intSum;
}

/**
 * Add an int and a short of a union (int & short) .
 *
 * @param arg1 an int
 * @param arg2 a union with an int and a short
 * @return the sum of int and short
 */
int
addIntAndShortFromUnionWithIntShort(int arg1, union_Int_Short arg2)
{
	int intSum = arg1 + arg2.elem2;
	return intSum;
}

/**
 * Add an int and an int of a union (short & integer).
 *
 * @param arg1 an int
 * @param arg2 a union with a short and an int
 * @return the sum of ints
 */
int
addIntAndIntFromUnionWithShortInt(int arg1, union_Short_Int arg2)
{
	int intSum = arg1 + arg2.elem2;
	return intSum;
}

/**
 * Add an int and a byte of a union (int ,short and byte).
 *
 * @param arg1 an int
 * @param arg2 a union with an int, a short and a byte
 * @return the sum of int and byte
 */
int
addIntAndByteFromUnionWithIntShortByte(int arg1, union_Int_Short_Byte arg2)
{
	int intSum = arg1 + arg2.elem3;
	return intSum;
}

/**
 * Add an int and a short of a union (byte, short and int).
 *
 * @param arg1 an int
 * @param arg2 a union with a byte, a short and an int
 * @return the sum
 */
int
addIntAndShortFromUnionWithByteShortInt(int arg1, union_Byte_Short_Int arg2)
{
	int intSum = arg1 + arg2.elem2;
	return intSum;
}

/**
 * Add an int and a long of a union (int & long).
 *
 * @param arg1 an int
 * @param arg2 a union with an int and a long
 * @return the sum of int and long
 */
LONG
addIntAndLongFromUnionWithIntLong(int arg1, union_Int_Long arg2)
{
	LONG longSum = arg1 + arg2.elem2;
	return longSum;
}

/**
 * Add an int and a long of a union (long & int).
 *
 * @param arg1 an int
 * @param arg2 a union with a long and an int
 * @return the sum of int and long
 */
LONG
addIntAndLongFromUnionWithLongInt(int arg1, union_Long_Int arg2)
{
	LONG longSum = arg1 + arg2.elem1;
	return longSum;
}

/**
 * Add a float and a float of a union (short & float).
 *
 * @param arg1 a float
 * @param arg2 a union with a short and a float
 * @return the sum of floats
 */
float
addFloatAndFloatFromUnionWithShortFloat(float arg1, union_Short_Float arg2)
{
	float floatSum = arg1 + arg2.elem2;
	return floatSum;
}

/**
 * Add a float and a float of a union (float & short).
 *
 * @param arg1 a float
 * @param arg2 a union with a float and a short
 * @return the sum of floats
 */
float
addFloatAndFloatFromUnionWithFloatShort(float arg1, union_Float_Short arg2)
{
	float floatSum = arg1 + arg2.elem1;
	return floatSum;
}

/**
 * Add a float and a float of a union (int & float) .
 *
 * @param arg1 a float
 * @param arg2 a union with an int and a float
 * @return the sum of floats
 */
float
addFloatAndFloatFromUnionWithIntFloat(float arg1, union_Int_Float arg2)
{
	float floatSum = arg1 + arg2.elem2;
	return floatSum;
}

/**
 * Add a float and a float of a union (float & int).
 *
 * @param arg1 a float
 * @param arg2 a union with a float and an int
 * @return the sum of floats
 */
float
addFloatAndFloatFromUnionWithFloatInt(float arg1, union_Float_Int arg2)
{
	float floatSum = arg1 + arg2.elem1;
	return floatSum;
}

/**
 * Add a double and a float of a union (float & double).
 *
 * @param arg1 a double
 * @param arg2 a union with a float and a double
 * @return the sum of double and float
 */
double
addDoubleAndFloatFromUnionWithFloatDouble(double arg1, union_Float_Double arg2)
{
	double doubleSum = arg1 + arg2.elem1;
	return doubleSum;
}

/**
 * Add a double and a float of a union (double & float).
 *
 * @param arg1 a double
 * @param arg2 a union with a double and a float
 * @return the sum of double and float
 */
double
addDoubleAndFloatFromUnionWithDoubleFloat(double arg1, union_Double_Float arg2)
{
	double doubleSum = arg1 + arg2.elem2;
	return doubleSum;
}

/**
 * Add a double and an int of a union (int & double).
 *
 * @param arg1 a double
 * @param arg2 a union with an int and a double
 * @return the sum of double and int
 */
double
addDoubleAndIntFromUnionWithIntDouble(double arg1, union_Int_Double arg2)
{
	double doubleSum = arg1 + arg2.elem1;
	return doubleSum;
}

/**
 * Add a double and a double of a union (double & int).
 *
 * @param arg1 a double
 * @param arg2 a union with a double and an int
 * @return the sum of doubles
 */
double
addDoubleAndDoubleFromUnionWithDoubleInt(double arg1, union_Double_Int arg2)
{
	double doubleSum = arg1 + arg2.elem1;
	return doubleSum;
}

/**
 * Add a float and a float of a union (float & long).
 *
 * @param arg1 a float
 * @param arg2 a union with a float and a long
 * @return the sum of floats
 */
float
addFloatAndFloatFromUnionWithFloatLong(float arg1, union_Float_Long arg2)
{
	float floatSum = arg1 + arg2.elem1;
	return floatSum;
}

/**
 * Add a long and a long of a union (long & float).
 *
 * @param arg1 a long
 * @param arg2 a union with a long and a float
 * @return the sum of longs
 */
LONG
addLongAndLongFromUnionWithLongFloat(LONG arg1, union_Long_Float arg2)
{
	LONG longSum = arg1 + arg2.elem1;
	return longSum;
}

/**
 * Add a double and a double of a union (long & double).
 *
 * @param arg1 a double
 * @param arg2 a union with a long and a double
 * @return the sum of doubles
 */
double
addDoubleAndDoubleFromUnionWithLongDouble(double arg1, union_Long_Double arg2)
{
	double doubleSum = arg1 + arg2.elem2;
	return doubleSum;
}

/**
 * Add a double and a long of a union (double & long).
 *
 * @param arg1 a double
 * @param arg2 a union with a double and a long
 * @return the sum of double and long
 */
double
addDoubleAndLongFromUnionWithDoubleLong(double arg1, union_Double_Long arg2)
{
	double doubleSum = arg1 + arg2.elem2;
	return doubleSum;
}

/**
 * Add a double and a float of a union (int, float and double).
 *
 * @param arg1 a double
 * @param arg2 a union with an int, a float and a double
 * @return the sum of double and float
 */
double
addDoubleAndFloatFromUnionWithIntFloatDouble(double arg1, union_Int_Float_Double arg2)
{
	double doubleSum = arg1 + arg2.elem2;
	return doubleSum;
}

/**
 * Add a double and a float of a union (int, double and float).
 *
 * @param arg1 a double
 * @param arg2 a union with an int, a double and a float
 * @return the sum
 */
double
addDoubleAndFloatFromUnionWithIntDoubleFloat(double arg1, union_Int_Double_Float arg2)
{
	double doubleSum = arg1 + arg2.elem3;
	return doubleSum;
}

/**
 * Add a long and a long of a union (int, float and long).
 *
 * @param arg1 a long
 * @param arg2 a union with an int, a float and a long
 * @return the sum of longs
 */
LONG
addLongAndLongFromUnionWithIntFloatLong(LONG arg1, union_Int_Float_Long arg2)
{
	LONG longSum = arg1 + arg2.elem3;
	return longSum;
}

/**
 * Add a long and a long of a union (int, long and float).
 *
 * @param arg1 a long
 * @param arg2 a union with an int, a long and a float
 * @return the sum
 */
LONG
addLongAndLongFromUnionWithIntLongFloat(LONG arg1, union_Int_Long_Float arg2)
{
	LONG longSum = arg1 + arg2.elem2;
	return longSum;
}

/**
 * Add a double and a double of a union (float, long and double).
 *
 * @param arg1 a double
 * @param arg2 a union with a float, a long and a double
 * @return the sum of doubles
 */
double
addDoubleAndDoubleFromUnionWithFloatLongDouble(double arg1, union_Float_Long_Double arg2)
{
	double doubleSum = arg1 + arg2.elem3;
	return doubleSum;
}

/**
 * Add a double and a long of a union (int, double and long).
 *
 * @param arg1 a double
 * @param arg2 a union with an int, a double and a long
 * @return the sum double and long
 */
double
addDoubleAndLongFromUnionWithIntDoubleLong(double arg1, union_Int_Double_Long arg2)
{
	double doubleSum = arg1 + arg2.elem3;
	return doubleSum;
}

/**
 * Add a double and the pointer value of a union.
 *
 * @param arg1 a double
 * @param arg2 a union with an int, a double and a pointer
 * @return the sum
 */
double
addDoubleAndPtrValueFromUnion(double arg1, union_Int_Double_Ptr arg2)
{
	double doubleSum = arg1 + *(arg2.elem3);
	return doubleSum;
}

/**
 * Add a long and the pointer value from a union.
 *
 * @param arg1 a long
 * @param arg2 a union with an int, a float and a pointer
 * @return the sum
 */
LONG
addLongAndPtrValueFromUnion(LONG arg1, union_Int_Float_Ptr arg2)
{
	LONG longSum = arg1 + *(arg2.elem3);
	return longSum;
}



/**
 * Add a long and an int of a union (byte, short, int and long).
 *
 * @param arg1 a long
 * @param arg2 a union with a byte, a short, an int and a long
 * @return the sum
 */
LONG
addLongAndIntFromUnionWithByteShortIntLong(LONG arg1, union_Byte_Short_Int_Long arg2)
{
	LONG longSum = arg1 + arg2.elem3;
	return longSum;
}

/**
 * Add a long and an int of a union (long, int, short and byte).
 *
 * @param arg1 a long
 * @param arg2 a union with a long, an int, a short and a byte
 * @return the sum
 */
LONG
addLongAndIntFromUnionWithLongIntShortByte(LONG arg1, union_Long_Int_Short_Byte arg2)
{
	LONG longSum = arg1 + arg2.elem2;
	return longSum;
}

/**
 * Add a double and a short of a union (byte, short, float and double).
 *
 * @param arg1 a double
 * @param arg2 a union with a byte, a short, a float and a double
 * @return the sum of double and short
 */
double
addDoubleAndShortFromUnionWithByteShortFloatDouble(double arg1, union_Byte_Short_Float_Double arg2)
{
	double doubleSum = arg1 + arg2.elem2;
	return doubleSum;
}

/**
 * Add a double and a float of a union (double, float, short and byte).
 *
 * @param arg1 a double
 * @param arg2 a union with a double, a float, a short and a byte
 * @return the sum of double and float
 */
double
addDoubleAndFloatFromUnionWithDoubleFloatShortByte(double arg1, union_Double_Float_Short_Byte arg2)
{
	double doubleSum = arg1 + arg2.elem2;
	return doubleSum;
}

/**
 * Add a boolean and all boolean elements of a struct with a nested union (2 booleans)
 * with the XOR (^) operator.
 *
 * @param arg1 a boolean
 * @param arg2 a struct with a boolean and a nested union (2 booleans)
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromStructWithXor_Nested2BoolUnion(bool arg1, stru_Bool_NestedUnion_2_Bools arg2)
{
	bool boolSum = arg1 ^ arg2.elem1 ^ arg2.elem2.elem1 ^ arg2.elem2.elem2;
	return boolSum;
}

/**
 * Add a boolean and all booleans elements of a union with a nested struct (2 booleans)
 * with the XOR (^) operator.
 *
 * @param arg1 a boolean
 * @param arg2 a union with a boolean and a nested struct (2 booleans)
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromUnionWithXor_Nested2BoolStruct(bool arg1, union_Bool_NestedStruct_2_Bools arg2)
{
	bool boolSum = arg1 ^ arg2.elem1 ^ arg2.elem2.elem1 ^ arg2.elem2.elem2;
	return boolSum;
}

/**
 * Add a boolean and all booleans elements of a union with a nested struct (4 booleans)
 * with the XOR (^) operator.
 *
 * @param arg1 a boolean
 * @param arg2 a union with a boolean and a nested struct (4 booleans)
 * @return the XOR result of booleans
 */
bool
addBoolAndBoolsFromUnionWithXor_Nested4BoolStruct(bool arg1, union_Bool_NestedStruct_4_Bools arg2)
{
	bool boolSum = arg1 ^ arg2.elem1 ^ arg2.elem2.elem1
			^ arg2.elem2.elem2 ^ arg2.elem2.elem3 ^ arg2.elem2.elem4;
	return boolSum;
}

/**
 * Get a new struct by adding each boolean element of two structs with a nested union (2 booleans)
 * with the XOR (^) operator.
 *
 * @param arg1 the 1st struct with a boolean and a nested union (2 booleans)
 * @param arg2 the 2nd struct with a boolean and a nested union (2 booleans)
 * @return a struct with a boolean and a nested union (2 booleans)
 */
stru_Bool_NestedUnion_2_Bools
add2BoolStructsWithXor_returnStruct_Nested2BoolUnion(stru_Bool_NestedUnion_2_Bools arg1, stru_Bool_NestedUnion_2_Bools arg2)
{
	stru_Bool_NestedUnion_2_Bools boolStruct;
	boolStruct.elem1 = arg1.elem1 ^ arg2.elem1;
	boolStruct.elem2.elem1 = arg1.elem2.elem1 ^ arg2.elem2.elem1;
	boolStruct.elem2.elem2 = arg1.elem2.elem2 ^ arg2.elem2.elem2;
	return boolStruct;
}

/**
 * Get a new union by adding each boolean element of two unions with a nested struct (2 booleans)
 * with the XOR (^) operator.
 *
 * @param arg1 the 1st union with a bool and a nested struct (2 booleans)
 * @param arg2 the 2nd union with a bool and a nested struct (2 booleans)
 * @return a union with a bool and a nested struct (2 booleans)
 */
union_Bool_NestedStruct_2_Bools
add2BoolUnionsWithXor_returnUnion_Nested2BoolStruct(union_Bool_NestedStruct_2_Bools arg1, union_Bool_NestedStruct_2_Bools arg2)
{
	union_Bool_NestedStruct_2_Bools boolUnion;
	boolUnion.elem1 = arg1.elem1 ^ arg2.elem1;
	boolUnion.elem2.elem1 = arg1.elem2.elem1 ^ arg2.elem2.elem1;
	boolUnion.elem2.elem2 = arg1.elem2.elem2 ^ arg2.elem2.elem2;
	return boolUnion;
}

/**
 * Add a byte and all bytes of a struct with a nested union (2 bytes).
 *
 * @param arg1 a byte
 * @param arg2 a struct with a byte and a nested union (2 bytes)
 * @return the sum of bytes
 */
char
addByteAndBytesFromStruct_Nested2ByteUnion(char arg1, stru_Byte_NestedUnion_2_Bytes arg2)
{
	char byteSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return byteSum;
}

/**
 * Add a byte and all bytes of a union with a byte and a nested struct (2 bytes).
 *
 * @param arg1 a byte
 * @param arg2 a union with a byte and a nested struct (2 bytes)
 * @return the sum of bytes
 */
char
addByteAndBytesFromUnion_Nested2ByteStruct(char arg1, union_Byte_NestedStruct_2_Bytes arg2)
{
	char byteSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return byteSum;
}

/**
 * Add a byte and all bytes of a union with a byte and a nested struct (4 bytes).
 *
 * @param arg1 a byte
 * @param arg2 a union with a byte and a nested struct (4 bytes)
 * @return the sum of bytes
 */
char
addByteAndBytesFromUnion_Nested4ByteStruct(char arg1, union_Byte_NestedStruct_4_Bytes arg2)
{
	char byteSum = arg1 + arg2.elem1 + arg2.elem2.elem1
			+ arg2.elem2.elem2  + arg2.elem2.elem3  + arg2.elem2.elem4;
	return byteSum;
}

/**
 * Get a new struct by adding each byte element of two structs
 * with a byte and a nested union (2 bytes).
 *
 * @param arg1 the 1st struct with a byte and a nested union (2 bytes)
 * @param arg2 the 2nd struct with a byte and a nested union (2 bytes)
 * @return a struct with a byte and a nested union (2 bytes)
 */
stru_Byte_NestedUnion_2_Bytes
add2ByteStructs_returnStruct_Nested2ByteUnion(stru_Byte_NestedUnion_2_Bytes arg1, stru_Byte_NestedUnion_2_Bytes arg2)
{
	stru_Byte_NestedUnion_2_Bytes byteStruct;
	byteStruct.elem1 = arg1.elem1 + arg2.elem1;
	byteStruct.elem2.elem1 = arg1.elem2.elem1 + arg2.elem2.elem1;
	byteStruct.elem2.elem2 = arg1.elem2.elem2 + arg2.elem2.elem2;
	return byteStruct;
}

/**
 * Get a new union by adding each byte element of two unions with a byte and a nested struct (2 bytes).
 *
 * @param arg1 the 1st union with a byte and a nested struct (2 bytes)
 * @param arg2 the 2nd union with a byte and a nested struct (2 bytes)
 * @return a union with two bytes
 */
union_Byte_NestedStruct_2_Bytes
add2ByteUnions_returnUnion_Nested2ByteStruct(union_Byte_NestedStruct_2_Bytes arg1, union_Byte_NestedStruct_2_Bytes arg2)
{
	union_Byte_NestedStruct_2_Bytes byteUnion;
	byteUnion.elem1 = arg1.elem1 + arg2.elem1;
	byteUnion.elem2.elem1 = arg1.elem2.elem1 + arg2.elem2.elem1;
	byteUnion.elem2.elem2 = arg1.elem2.elem2 + arg2.elem2.elem2;
	return byteUnion;
}

/**
 * Generate a new char by adding a char and all chars of a struct with a nested union.
 *
 * @param arg1 a char
 * @param arg2 a struct with a char and a nested union (2 chars)
 * @return a new char
 */
short
addCharAndCharsFromStruct_Nested2CharUnion(short arg1, stru_Char_NestedUnion_2_Chars arg2)
{
	short result = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2 - (3 * 'A');
	return result;
}

/**
 * Generate a new char by adding a char and all chars of a union with a nested struct (2 chars).
 *
 * @param arg1 a char
 * @param arg2 a union with a char and a nested struct (2 chars)
 * @return a new char
 */
short
addCharAndCharsFromUnion_Nested2CharStruct(short arg1, union_Char_NestedStruct_2_Chars arg2)
{
	short result = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2 - (3 * 'A');
	return result;
}

/**
 * Generate a new char by adding a char and all chars of a union with a nested struct (4 chars).
 *
 * @param arg1 a char
 * @param arg2 a union with a char and a nested struct (4 chars)
 * @return a new char
 */
short
addCharAndCharsFromUnion_Nested4CharStruct(short arg1, union_Char_NestedStruct_4_Chars arg2)
{
	short result = arg1 + arg2.elem1 + arg2.elem2.elem1
			+ arg2.elem2.elem2 + arg2.elem2.elem3 + arg2.elem2.elem4 - (5 * 'A');
	return result;
}

/**
 * Create a new struct by adding each char element of two structs
 * with a char and a nested union (2 chars).
 *
 * @param arg1 the 1st struct with a char and a nested union (2 chars)
 * @param arg2 the 2nd struct with a char and a nested union (2 chars)
 * @return a new struct with a char and a nested union (2 chars)
 */
stru_Char_NestedUnion_2_Chars
add2CharStructs_returnStruct_Nested2CharUnion(stru_Char_NestedUnion_2_Chars arg1, stru_Char_NestedUnion_2_Chars arg2)
{
	stru_Char_NestedUnion_2_Chars charStruct;
	charStruct.elem1 = arg1.elem1 + arg2.elem1 - 'A';
	charStruct.elem2.elem1 = arg1.elem2.elem1 + arg2.elem2.elem1 - 'A';
	charStruct.elem2.elem2 = arg1.elem2.elem2 + arg2.elem2.elem2 - 'A';
	return charStruct;
}

/**
 * Create a new union by adding each char element of two unions with a char and a nested struct (2 chars).
 *
 * @param arg1 the 1st union with a char and a nested struct (2 chars)
 * @param arg2 the 2nd union with a char and a nested struct (2 chars)
 * @return a new union of with a char and a nested struct (2 chars)
 */
union_Char_NestedStruct_2_Chars
add2CharUnions_returnUnion_Nested2CharStruct(union_Char_NestedStruct_2_Chars arg1, union_Char_NestedStruct_2_Chars arg2)
{
	union_Char_NestedStruct_2_Chars charUnion;
	charUnion.elem1 = arg1.elem1 + arg2.elem1 - 'A';
	charUnion.elem2.elem1 = arg1.elem2.elem1 + arg2.elem2.elem1 - 'A';
	charUnion.elem2.elem2 = arg1.elem2.elem2 + arg2.elem2.elem2 - 'A';
	return charUnion;
}

/**
 * Add a short and all shorts of a struct with a short and a nested union (2 shorts).
 *
 * @param arg1 a short
 * @param arg2 a struct with a short and a nested union (2 shorts)
 * @return the sum of shorts
 */
short
addShortAndShortsFromStruct_Nested2ShortUnion(short arg1, stru_Short_NestedUnion_2_Shorts arg2)
{
	short shortSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return shortSum;
}

/**
 * Add a short and all shorts of a union with a short and a nested struct (2 shorts).
 *
 * @param arg1 a short
 * @param arg2 a union with a short and a nested struct (2 shorts)
 * @return the sum of shorts
 */
short
addShortAndShortsFromUnion_Nested2ShortStruct(short arg1, union_Short_NestedStruct_2_Shorts arg2)
{
	short shortSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return shortSum;
}

/**
 * Add a short and all shorts of a union with a short and a nested struct (4 shorts).
 *
 * @param arg1 a short
 * @param arg2 a union with a short and a nested struct (4 shorts)
 * @return the sum of shorts
 */
short
addShortAndShortsFromUnion_Nested4ShortStruct(short arg1, union_Short_NestedStruct_4_Shorts arg2)
{
	short shortSum = arg1 + arg2.elem1 + arg2.elem2.elem1
			+ arg2.elem2.elem2 + arg2.elem2.elem3 + arg2.elem2.elem4;
	return shortSum;
}

/**
 * Get a new struct by adding each short element of two structs
 * with a short and a nested union (2 shorts).
 *
 * @param arg1 the 1st struct with a short and a nested union (2 shorts)
 * @param arg2 the 2nd struct with a short and a nested union (2 shorts)
 * @return a struct with a short and a nested union (2 shorts)
 */
stru_Short_NestedUnion_2_Shorts
add2ShortStructs_returnStruct_Nested2ShortUnion(stru_Short_NestedUnion_2_Shorts arg1, stru_Short_NestedUnion_2_Shorts arg2)
{
	stru_Short_NestedUnion_2_Shorts shortStruct;
	shortStruct.elem1 = arg1.elem1 + arg2.elem1;
	shortStruct.elem2.elem1 = arg1.elem2.elem1 + arg2.elem2.elem1;
	shortStruct.elem2.elem2 = arg1.elem2.elem2 + arg2.elem2.elem2;
	return shortStruct;
}

/**
 * Get a new union by adding each short element of two unions
 * with a short and a nested struct (2 shorts).
 *
 * @param arg1 the 1st union with a short and a nested struct (2 shorts)
 * @param arg2 the 2nd union with a short and a nested struct (2 shorts)
 * @return a union with a short and a nested struct (2 shorts)
 */
union_Short_NestedStruct_2_Shorts
add2ShortUnions_returnUnion_Nested2ShortStruct(union_Short_NestedStruct_2_Shorts arg1, union_Short_NestedStruct_2_Shorts arg2)
{
	union_Short_NestedStruct_2_Shorts shortUnion;
	shortUnion.elem1 = arg1.elem1 + arg2.elem1;
	shortUnion.elem2.elem1 = arg1.elem2.elem1 + arg2.elem2.elem1;
	shortUnion.elem2.elem2 = arg1.elem2.elem2 + arg2.elem2.elem2;
	return shortUnion;
}

/**
 * Add an int and all ints of a struct with an int and a nested struct (2 ints).
 *
 * @param arg1 an int
 * @param arg2 a struct with an int and a nested struct (2 ints)
 * @return the sum of ints
 */
int
addIntAndIntsFromStruct_Nested2IntUnion(int arg1, stru_Int_NestedUnion_2_Ints arg2)
{
	int intSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return intSum;
}

/**
 * Add an int and all ints of a union with an int and a nested struct (2 ints).
 *
 * @param arg1 an int
 * @param arg2 a union with an int and a nested struct (2 ints)
 * @return the sum of ints
 */
int
addIntAndIntsFromUnion_Nested2IntStruct(int arg1, union_Int_NestedStruct_2_Ints arg2)
{
	int intSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return intSum;
}

/**
 * Add an int and all ints of a union with an int and a nested struct (4 ints).
 *
 * @param arg1 an int
 * @param arg2 a union with an int and a nested struct (4 ints)
 * @return the sum of ints
 */
int
addIntAndIntsFromUnion_Nested4IntStruct(int arg1, union_Int_NestedStruct_4_Ints arg2)
{
	int intSum = arg1 + arg2.elem1 + arg2.elem2.elem1
			+ arg2.elem2.elem2 + arg2.elem2.elem3 + arg2.elem2.elem4;
	return intSum;
}

/**
 * Get a new struct by adding each int element of two structs
 * with an int and a nested union (2 ints).
 *
 * @param arg1 the 1st struct with an int and a nested union (2 ints)
 * @param arg2 the 2nd struct with an int and a nested union (2 ints)
 * @return a struct with an int and a nested union (2 ints)
 */
stru_Int_NestedUnion_2_Ints
add2IntStructs_returnStruct_Nested2IntUnion(stru_Int_NestedUnion_2_Ints arg1, stru_Int_NestedUnion_2_Ints arg2)
{
	stru_Int_NestedUnion_2_Ints intStruct;
	intStruct.elem1 = arg1.elem1 + arg2.elem1;
	intStruct.elem2.elem1 = arg1.elem2.elem1 + arg2.elem2.elem1;
	intStruct.elem2.elem2 = arg1.elem2.elem2 + arg2.elem2.elem2;
	return intStruct;
}

/**
 * Get a new union by adding each int element of two unions
 * with an int and a nested struct (2 ints).
 *
 * @param arg1 the 1st union with an int and a nested struct (2 ints)
 * @param arg2 the 2nd union with an int and a nested struct (2 ints)
 * @return a union with an int and a nested struct (2 ints)
 */
union_Int_NestedStruct_2_Ints
add2IntUnions_returnUnion_Nested2IntStruct(union_Int_NestedStruct_2_Ints arg1, union_Int_NestedStruct_2_Ints arg2)
{
	union_Int_NestedStruct_2_Ints intUnion;
	intUnion.elem1 = arg1.elem1 + arg2.elem1;
	intUnion.elem2.elem1 = arg1.elem2.elem1 + arg2.elem2.elem1;
	intUnion.elem2.elem2 = arg1.elem2.elem2 + arg2.elem2.elem2;
	return intUnion;
}

/**
 * Add a long and all longs of a struct with a long and a nested union (2 longs).
 *
 * @param arg1 a long
 * @param arg2 a struct with a long and a nested union (2 longs)
 * @return the sum of longs
 */
LONG
addLongAndLongsFromStruct_Nested2LongUnion(LONG arg1, stru_Long_NestedUnion_2_Longs arg2)
{
	LONG longSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return longSum;
}

/**
 * Add a long and all longs of a union with a long and a nested struct (2 longs).
 *
 * @param arg1 a long
 * @param arg2 a union with a long and a nested struct (2 longs)
 * @return the sum of longs
 */
LONG
addLongAndLongsFromUnion_Nested2LongStruct(LONG arg1, union_Long_NestedStruct_2_Longs arg2)
{
	LONG longSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return longSum;
}

/**
 * Add a long and all longs of a union with a long and a nested struct (4 longs).
 *
 * @param arg1 a long
 * @param arg2 a union with a long and a nested struct (4 longs)
 * @return the sum of longs
 */
LONG
addLongAndLongsFromUnion_Nested4LongStruct(LONG arg1, union_Long_NestedStruct_4_Longs arg2)
{
	LONG longSum = arg1 + arg2.elem1 + arg2.elem2.elem1
			+ arg2.elem2.elem2 + arg2.elem2.elem3 + arg2.elem2.elem4;
	return longSum;
}

/**
 * Get a new struct by adding each long element of two structs
 * with a long and a nested union (2 longs).
 *
 * @param arg1 the 1st struct with a long and a nested union (2 longs)
 * @param arg2 the 2nd struct with a long and a nested union (2 longs)
 * @return a struct with a long and a nested union (2 longs)
 */
stru_Long_NestedUnion_2_Longs
add2LongStructs_returnStruct_Nested2LongUnion(stru_Long_NestedUnion_2_Longs arg1, stru_Long_NestedUnion_2_Longs arg2)
{
	stru_Long_NestedUnion_2_Longs longStruct;
	longStruct.elem1 = arg1.elem1 + arg2.elem1;
	longStruct.elem2.elem1 = arg1.elem2.elem1 + arg2.elem2.elem1;
	longStruct.elem2.elem2 = arg1.elem2.elem2 + arg2.elem2.elem2;
	return longStruct;
}

/**
 * Get a new union by adding each long element of two unions with a long and a nested struct (2 longs).
 *
 * @param arg1 the 1st union with a long and a nested struct (2 longs)
 * @param arg2 the 2nd union with a long and a nested struct (2 longs)
 * @return a union with a long and a nested struct (2 longs)
 */
union_Long_NestedStruct_2_Longs
add2LongUnions_returnUnion_Nested2LongStruct(union_Long_NestedStruct_2_Longs arg1, union_Long_NestedStruct_2_Longs arg2)
{
	union_Long_NestedStruct_2_Longs longUnion;
	longUnion.elem1 = arg1.elem1 + arg2.elem1;
	longUnion.elem2.elem1 = arg1.elem2.elem1 + arg2.elem2.elem1;
	longUnion.elem2.elem2 = arg1.elem2.elem2 + arg2.elem2.elem2;
	return longUnion;
}

/**
 * Add a float and all floats of a struct with a float and a nested union (2 floats).
 *
 * @param arg1 a float
 * @param arg2 a struct with a float and a nested union (2 floats)
 * @return the sum of floats
 */
float
addFloatAndFloatsFromStruct_Nested2FloatUnion(float arg1, stru_Float_NestedUnion_2_Floats arg2)
{
	float floatSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return floatSum;
}

/**
 * Add a float and all floats of a union with a float and a nested struct (2 floats).
 *
 * @param arg1 a float
 * @param arg2 a union with a float and a nested struct (2 floats)
 * @return the sum of floats
 */
float
addFloatAndFloatsFromUnion_Nested2FloatStruct(float arg1, union_Float_NestedStruct_2_Floats arg2)
{
	float floatSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return floatSum;
}

/**
 * Add a float and all floats of a union with a float and a nested struct (4 floats).
 *
 * @param arg1 a float
 * @param arg2 a union with a float and a nested struct (4 floats)
 * @return the sum of floats
 */
float
addFloatAndFloatsFromUnion_Nested4FloatStruct(float arg1, union_Float_NestedStruct_4_Floats arg2)
{
	float floatSum = arg1 + arg2.elem1 + arg2.elem2.elem1
			+ arg2.elem2.elem2 + + arg2.elem2.elem3 + arg2.elem2.elem4;
	return floatSum;
}

/**
 * Create a new struct by adding each float element of two structs with a float and a nested union (2 floats).
 *
 * @param arg1 the 1st struct with a float and a nested union (2 floats)
 * @param arg2 the 2nd struct with a float and a nested union (2 floats)
 * @return a struct with a float and a nested union (2 floats)
 */
stru_Float_NestedUnion_2_Floats
add2FloatStructs_returnStruct_Nested2FloatUnion(stru_Float_NestedUnion_2_Floats arg1, stru_Float_NestedUnion_2_Floats arg2)
{
	stru_Float_NestedUnion_2_Floats floatStruct;
	floatStruct.elem1 = arg1.elem1 + arg2.elem1;
	floatStruct.elem2.elem1 = arg1.elem2.elem1 + arg2.elem2.elem1;
	floatStruct.elem2.elem2 = arg1.elem2.elem2 + arg2.elem2.elem2;
	return floatStruct;
}

/**
 * Create a new union by adding each float element of two unions with a float and a nested struct (2 floats).
 *
 * @param arg1 the 1st union with a float and a nested struct (2 floats)
 * @param arg2 the 2nd union with a float and a nested struct (2 floats)
 * @return a union with a float and a nested struct (2 floats)
 */
union_Float_NestedStruct_2_Floats
add2FloatUnions_returnUnion_Nested2FloatStruct(union_Float_NestedStruct_2_Floats arg1, union_Float_NestedStruct_2_Floats arg2)
{
	union_Float_NestedStruct_2_Floats floatUnion;
	floatUnion.elem1 = arg1.elem1 + arg2.elem1;
	floatUnion.elem2.elem1 = arg1.elem2.elem1 + arg2.elem2.elem1;
	floatUnion.elem2.elem2 = arg1.elem2.elem2 + arg2.elem2.elem2;
	return floatUnion;
}

/**
 * Add a double and all doubles of a struct with a double and a nested union (2 doubles).
 *
 * @param arg1 a double
 * @param arg2 a struct with a double and a nested union (2 doubles)
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromStruct_Nested2DoubleUnion(double arg1, stru_Double_NestedUnion_2_Doubles arg2)
{
	double doubleSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return doubleSum;
}

/**
 * Add a double and two doubles of a union with a double and a nested union (2 doubles).
 *
 * @param arg1 a double
 * @param arg2 a union with a double and a nested union (2 doubles)
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromUnion_Nested2DoubleStruct(double arg1, union_Double_NestedStruct_2_Doubles arg2)
{
	double doubleSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return doubleSum;
}

/**
 * Add a double and two doubles of a union with a double and a nested union (4 doubles).
 *
 * @param arg1 a double
 * @param arg2 a union with a double and a nested union (4 doubles)
 * @return the sum of doubles
 */
double
addDoubleAndDoublesFromUnion_Nested4DoubleStruct(double arg1, union_Double_NestedStruct_4_Doubles arg2)
{
	double doubleSum = arg1 + arg2.elem1 + arg2.elem2.elem1
			+ arg2.elem2.elem2 + arg2.elem2.elem3 + arg2.elem2.elem4;
	return doubleSum;
}

/**
 * Create a new struct by adding each double element of two structs with a double and a nested union (2 doubles).
 *
 * @param arg1 the 1st struct with a double and a nested union (2 doubles)
 * @param arg2 the 2nd struct with a double and a nested union (2 doubles)
 * @return a struct with a double and a nested union (2 doubles)
 */
stru_Double_NestedUnion_2_Doubles
add2DoubleStructs_returnStruct_Nested2DoubleUnion(stru_Double_NestedUnion_2_Doubles arg1, stru_Double_NestedUnion_2_Doubles arg2)
{
	stru_Double_NestedUnion_2_Doubles doubleStruct;
	doubleStruct.elem1 = arg1.elem1 + arg2.elem1;
	doubleStruct.elem2.elem1 = arg1.elem2.elem1 + arg2.elem2.elem1;
	doubleStruct.elem2.elem2 = arg1.elem2.elem2 + arg2.elem2.elem2;
	return doubleStruct;
}

/**
 * Create a new union by adding each double element of two unions with a double and a nested struct (2 doubles).
 *
 * @param arg1 the 1st union with a double and a nested struct (2 doubles)
 * @param arg2 the 2nd union with a double and a nested struct (2 doubles)
 * @return a union with a double and a nested struct (2 doubles)
 */
union_Double_NestedStruct_2_Doubles
add2DoubleUnions_returnUnion_Nested2DoubleStruct(union_Double_NestedStruct_2_Doubles arg1, union_Double_NestedStruct_2_Doubles arg2)
{
	union_Double_NestedStruct_2_Doubles doubleUnion;
	doubleUnion.elem1 = arg1.elem1 + arg2.elem1;
	doubleUnion.elem2.elem1 = arg1.elem2.elem1 + arg2.elem2.elem1;
	doubleUnion.elem2.elem2 = arg1.elem2.elem2 + arg2.elem2.elem2;
	return doubleUnion;
}

/**
 * Add a short and all elements of a union with a short and a nested bytes (2 bytes).
 *
 * @param arg1 a short
 * @param arg2 a union with a short and a nested bytes (2 bytes)
 * @return the sum
 */
short
addShortAndShortByteFromUnion_Nested2ByteStruct(short arg1, union_Short_NestedStruct_2_Bytes arg2)
{
	short shortSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return shortSum;
}

/**
 * Add a short and all bytes of a union with a short and a nested struct (4 bytes).
 *
 * @param arg1 a short
 * @param arg2 a union with a short and a nested struct (4 bytes)
 * @return the sum
 */
short
addShortAndBytesFromUnion_Nested4ByteStruct(short arg1, union_Short_NestedStruct_4_Bytes arg2)
{
	short shortSum = arg1 + arg2.elem2.elem1
			+ arg2.elem2.elem2 + arg2.elem2.elem3 + arg2.elem2.elem4;
	return shortSum;
}

/**
 * Add an int and all elements of a union with an int and a nested structs (2 shorts).
 *
 * @param arg1 an int
 * @param arg2 a union with an int and a nested struct (2 shorts)
 * @return the sum
 */
int
addIntAndIntShortFromUnion_Nested2ShortStruct(int arg1, union_Int_NestedStruct_2_Shorts arg2)
{
	int intSum = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2;
	return intSum;
}

/**
 * Add an int and all shorts of a union with an int and a nested struct (4 shorts).
 *
 * @param arg1 an int
 * @param arg2 a union with an int and a nested struct (4 shorts)
 * @return the sum
 */
int
addIntAndShortsFromUnion_Nested4ShortStruct(int arg1, union_Int_NestedStruct_4_Shorts arg2)
{
	int intSum = arg1 + arg2.elem2.elem1
			+ arg2.elem2.elem2 + arg2.elem2.elem3 + + arg2.elem2.elem4;
	return intSum;
}

/**
 * Get a new struct by adding each boolean element of three structs
 * (dereferenced from pointers) with two booleans.
 *
 * @param arg1 a pointer to on-heap struct with two booleans
 * @param arg2 a pointer to off-heap struct with two booleans
 * @param arg3 a pointer to on-heap struct with two booleans
 * @return a struct with two booleans
 */
stru_2_Bools
addBoolsFromMultipleStructPtrs_returnStruct(stru_2_Bools *arg1, stru_2_Bools *arg2, stru_2_Bools *arg3)
{
	stru_2_Bools boolStruct;
	boolStruct.elem1 = arg1->elem1 ^ arg2->elem1 ^ arg3->elem1;
	boolStruct.elem2 = arg1->elem2 ^ arg2->elem2 ^ arg3->elem2;
	return boolStruct;
}

/**
 * Get a new struct by adding each byte element of three structs
 * (dereferenced from pointers) with two bytes.
 *
 * @param arg1 a pointer to on-heap struct with two bytes
 * @param arg2 a pointer to off-heap struct with two bytes
 * @param arg3 a pointer to on-heap struct with two bytes
 * @return a struct with two bytes
 */
stru_2_Bytes
addBytesFromMultipleStructPtrs_returnStruct(stru_2_Bytes *arg1, stru_2_Bytes *arg2, stru_2_Bytes *arg3)
{
	stru_2_Bytes byteStruct;
	byteStruct.elem1 = arg1->elem1 + arg2->elem1 + arg3->elem1;
	byteStruct.elem2 = arg1->elem2 + arg2->elem2 + arg3->elem2;
	return byteStruct;
}

/**
 * Get a new struct by adding each char element of three structs
 * (dereferenced from pointers) with two chars.
 *
 * @param arg1 a pointer to on-heap struct with two chars
 * @param arg2 a pointer to off-heap struct with two chars
 * @param arg3 a pointer to on-heap struct with two chars
 * @return a struct with two chars
 */
stru_2_Chars
addCharsFromMultipleStructPtrs_returnStruct(stru_2_Chars *arg1, stru_2_Chars *arg2, stru_2_Chars *arg3)
{
	stru_2_Chars charStruct;
	charStruct.elem1 = arg1->elem1 + arg2->elem1 + arg3->elem1 - (2 * 'A');
	charStruct.elem2 = arg1->elem2 + arg2->elem2 + arg3->elem2 - (2 * 'A');
	return charStruct;
}

/**
 * Get a new struct by adding each short element of three structs
 * (dereferenced from pointers) with two shorts.
 *
 * @param arg1 a pointer to on-heap struct with two shorts
 * @param arg2 a pointer to off-heap struct with two shorts
 * @param arg3 a pointer to on-heap struct with two shorts
 * @return a struct with two shorts
 */
stru_2_Shorts
addShortsFromMultipleStructPtrs_returnStruct(stru_2_Shorts *arg1, stru_2_Shorts *arg2, stru_2_Shorts *arg3)
{
	stru_2_Shorts shortStruct;
	shortStruct.elem1 = arg1->elem1 + arg2->elem1 + arg3->elem1;
	shortStruct.elem2 = arg1->elem2 + arg2->elem2 + arg3->elem2;
	return shortStruct;
}

/**
 * Get a new struct by adding each integer element of three structs
 * (dereferenced from pointers) with two integers.
 *
 * @param arg1 a pointer to on-heap struct with two integers
 * @param arg2 a pointer to off-heap struct with two integers
 * @param arg3 a pointer to on-heap struct with two integers
 * @return a struct with two integers
 */
stru_2_Ints
addIntsFromMultipleStructPtrs_returnStruct(stru_2_Ints *arg1, stru_2_Ints *arg2, stru_2_Ints *arg3)
{
	stru_2_Ints intStruct;
	intStruct.elem1 = arg1->elem1 + arg2->elem1 + arg3->elem1;
	intStruct.elem2 = arg1->elem2 + arg2->elem2 + arg3->elem2;
	return intStruct;
}

/**
 * Get a new struct by adding each long element of three structs
 * (dereferenced from pointers) with two longs.
 *
 * @param arg1 a pointers to on-heap struct with two longs
 * @param arg2 a pointers to off-heap struct with two longs
 * @param arg3 a pointers to on-heap struct with two longs
 * @return a struct with two longs
 */
stru_2_Longs
addLongsFromMultipleStructPtrs_returnStruct(stru_2_Longs *arg1, stru_2_Longs *arg2, stru_2_Longs *arg3)
{
	stru_2_Longs longStruct;
	longStruct.elem1 = arg1->elem1 + arg2->elem1 + arg3->elem1;
	longStruct.elem2 = arg1->elem2 + arg2->elem2 + arg3->elem2;
	return longStruct;
}

/**
 * Get a new struct by adding each float element of three structs
 * (dereferenced from pointers) with two floats.
 *
 * @param arg1 a pointers to on-heap struct with two floats
 * @param arg2 a pointers to off-heap struct with two floats
 * @param arg3 a pointers to on-heap struct with two floats
 * @return a struct with two floats
 */
stru_2_Floats
addFloatsFromMultipleStructPtrs_returnStruct(stru_2_Floats *arg1, stru_2_Floats *arg2, stru_2_Floats *arg3)
{
	stru_2_Floats floatStruct;
	floatStruct.elem1 = arg1->elem1 + arg2->elem1 + arg3->elem1;
	floatStruct.elem2 = arg1->elem2 + arg2->elem2 + arg3->elem2;
	return floatStruct;
}

/**
 * Get a new struct by adding each double element of three structs
 * (dereferenced from pointers) with two doubles.
 *
 * @param arg1 a pointers to on-heap struct with two doubles
 * @param arg2 a pointers to off-heap struct with two doubles
 * @param arg3 a pointers to on-heap struct with two doubles
 * @return a struct with two doubles
 */
stru_2_Doubles
addDoublesFromMultipleStructPtrs_returnStruct(stru_2_Doubles *arg1, stru_2_Doubles *arg2, stru_2_Doubles *arg3)
{
	stru_2_Doubles doubleStruct;
	doubleStruct.elem1 = arg1->elem1 + arg2->elem1 + arg3->elem1;
	doubleStruct.elem2 = arg1->elem2 + arg2->elem2 + arg3->elem2;
	return doubleStruct;
}

/**
 * Do xor operation on each boolean of a native array (dereferenced from a pointer to a native array)
 * and assign the result to the element of a on-heap array at the same index.
 *
 * @param size the array size
 * @param heapPtr a pointer to a on-heap array
 * @param nativePtr a pointer to a native array
 */
void
setBoolFromArrayPtrWithXor(int size, char *heapPtr, char *nativePtr)
{
	int index = 0;
	while (index < size) {
		heapPtr[index] = nativePtr[index];
		index += 1;
	}
}

/**
 * Add each byte of a native array (dereferenced from a pointer to a native array)
 * by 1 and assign the result to the element of a on-heap array at the same index.
 *
 * @param size the array size
 * @param heapPtr a pointer to a on-heap array
 * @param nativePtr a pointer to a native array
 */
void
addByteFromArrayPtrByOne(int size, char *heapPtr, char *nativePtr)
{
	int index = 0;
	while (index < size) {
		heapPtr[index] = nativePtr[index] + 1;
		index += 1;
	}
}

/**
 * Add each char of a native array (dereferenced from a pointer to a native array)
 * by 1 and assign the result to the element of a on-heap array at the same index.
 *
 * @param size the array size
 * @param heapPtr a pointer to a on-heap array
 * @param nativePtr a pointer to a native array
 */
void
addCharFromArrayPtrByOne(int size, short *heapPtr, short *nativePtr)
{
	int index = 0;
	while (index < size) {
		heapPtr[index] = nativePtr[index] + 1;
		index += 1;
	}
}

/**
 * Add each short of a native array (dereferenced from a pointer to a native array)
 * by 1 and assign the result to the element of a on-heap array at the same index.
 *
 * @param size the array size
 * @param heapPtr a pointer to a on-heap array
 * @param nativePtr a pointer to a native array
 */
void
addShortFromArrayPtrByOne(int size, short *heapPtr, short *nativePtr)
{
	int index = 0;
	while (index < size) {
		heapPtr[index] = nativePtr[index] + 1;
		index += 1;
	}
}

/**
 * Add each integer of a native array (dereferenced from a pointer to a native array)
 * by 1 and assign the result to the element of a on-heap array at the same index.
 *
 * @param size the array size
 * @param heapPtr a pointer to a on-heap array
 * @param nativePtr a pointer to a native array
 */
void
addIntFromArrayPtrByOne(int size, int *heapPtr, int *nativePtr)
{
	int index = 0;
	while (index < size) {
		heapPtr[index] = nativePtr[index] + 1;
		index += 1;
	}
}

/**
 * Add each long of a native array (dereferenced from a pointer to a native array)
 * by 1 and assign the result to the element of a on-heap array at the same index.
 *
 * @param size the array size
 * @param heapPtr a pointer to a on-heap array
 * @param nativePtr a pointer to a native array
 */
void
addLongFromArrayPtrByOne(int size, LONG *heapPtr, LONG *nativePtr)
{
	int index = 0;
	while (index < size) {
		heapPtr[index] = nativePtr[index] + 1;
		index += 1;
	}
}

/**
 * Add each float of a native array (dereferenced from a pointer to a native array)
 * by 1 and assign the result to the element of a on-heap array at the same index.
 *
 * @param size the array size
 * @param heapPtr a pointer to a on-heap array
 * @param nativePtr a pointer to a native array
 */
void
addFloatFromArrayPtrByOne(int size, float *heapPtr, float *nativePtr)
{
	int index = 0;
	while (index < size) {
		heapPtr[index] = nativePtr[index] + 1;
		index += 1;
	}
}

/**
 * Add each double of a native array (dereferenced from a pointer to a native array)
 * by 1 and assign the result to the element of a on-heap array at the same index.
 *
 * @param size the array size
 * @param heapPtr a pointer to a on-heap array
 * @param nativePtr a pointer to a native array
 */
void
addDoubleFromArrayPtrByOne(int size, double *heapPtr, double *nativePtr)
{
	int index = 0;
	while (index < size) {
		heapPtr[index] = nativePtr[index] + 1;
		index += 1;
	}
}

/**
 * Get a new struct by adding each boolean element of two structs
 * with the same boolean nested struct with the XOR (^) operator.
 *
 * @param arg1 the 1st struct with a nested boolean struct
 * @param arg2 the 2nd struct with a nested boolean struct
 * @return a struct with a nested boolean struct
 */
stru_NestedStruct_Bool
addNestedBoolStructsWithXor_dupStruct(stru_NestedStruct_Bool arg1, stru_NestedStruct_Bool arg2)
{
	stru_NestedStruct_Bool boolStruct;
	boolStruct.elem1.elem1 = arg1.elem1.elem1 ^ arg2.elem1.elem1;
	boolStruct.elem1.elem2 = arg1.elem1.elem2 ^ arg2.elem1.elem2;
	boolStruct.elem2 = arg1.elem2 ^ arg2.elem2;
	return boolStruct;
}

/**
 * Get a new struct by adding each boolean element of two structs with different elements
 * with the XOR (^) operator.
 *
 * @param arg1 the 1st struct with two booleans
 * @param arg2 the 2nd struct with a nested boolean struct
 * @return a struct with two booleans
 */
stru_2_Bools
addBoolStruct1AndNestedBoolStruct2WithXor_returnStruct1_dupStruct(stru_2_Bools arg1, stru_NestedStruct_Bool arg2)
{
	stru_2_Bools boolStruct;
	boolStruct.elem1 = arg1.elem1 ^ arg2.elem1.elem1;
	boolStruct.elem2 = arg1.elem2 ^ arg2.elem1.elem2 ^ arg2.elem2;
	return boolStruct;
}

/**
 * Get a new struct by adding each boolean element of two structs with different elements
 * with the XOR (^) operator.
 *
 * @param arg1 the 1st struct with two booleans
 * @param arg2 the 2nd struct with a nested boolean struct
 * @return a struct with a nested boolean struct
 */
stru_NestedStruct_Bool
addBoolStruct1AndNestedBoolStruct2WithXor_returnStruct2_dupStruct(stru_2_Bools arg1, stru_NestedStruct_Bool arg2)
{
	stru_NestedStruct_Bool boolStruct;
	boolStruct.elem1.elem1 = arg1.elem1 ^ arg2.elem1.elem1;
	boolStruct.elem1.elem2 = arg1.elem2 ^ arg2.elem1.elem2;
	boolStruct.elem2 = arg1.elem2 ^ arg2.elem2;
	return boolStruct;
}

/**
 * Get a new struct by adding each boolean element of two structs
 * with the same nested boolean array with the XOR (^) operator.
 *
 * @param arg1 the 1st struct with a nested boolean array
 * @param arg2 the 2nd struct with a nested boolean array
 * @return a struct with a nested boolean array
 */
stru_NestedBoolArray_Bool
addNestedBoolArrayStructsWithXor_dupStruct(stru_NestedBoolArray_Bool arg1, stru_NestedBoolArray_Bool arg2)
{
	stru_NestedBoolArray_Bool boolStruct;
	boolStruct.elem1[0] = arg1.elem1[0] ^ arg2.elem1[0];
	boolStruct.elem1[1] = arg1.elem1[1] ^ arg2.elem1[1];
	boolStruct.elem2 = arg1.elem2 ^ arg2.elem2;
	return boolStruct;
}

/**
 * Get a new struct by adding each boolean element of two structs with different elements
 * with the XOR (^) operator.
 *
 * @param arg1 the 1st struct with two booleans
 * @param arg2 the 2nd struct with a nested boolean array
 * @return a struct with two booleans
 */
stru_2_Bools
addBoolStruct1AndNestedBoolArrayStruct2WithXor_returnStruct1_dupStruct(stru_2_Bools arg1, stru_NestedBoolArray_Bool arg2)
{
	stru_2_Bools boolStruct;
	boolStruct.elem1 = arg1.elem1 ^ arg2.elem1[0];
	boolStruct.elem2 = arg1.elem2 ^ arg2.elem1[1] ^ arg2.elem2;
	return boolStruct;
}

/**
 * Get a new struct by adding each boolean element of two structs with different elements
 * with the XOR (^) operator.
 *
 * @param arg1 the 1st struct with two booleans
 * @param arg2 the 2nd struct with a nested boolean array
 * @return a struct with a nested boolean array
 */
stru_NestedBoolArray_Bool
addBoolStruct1AndNestedBoolArrayStruct2WithXor_returnStruct2_dupStruct(stru_2_Bools arg1, stru_NestedBoolArray_Bool arg2)
{
	stru_NestedBoolArray_Bool boolStruct;
	boolStruct.elem1[0] = arg1.elem1 ^ arg2.elem1[0];
	boolStruct.elem1[1] = arg1.elem2 ^ arg2.elem1[1];
	boolStruct.elem2 = arg1.elem2 ^ arg2.elem2;
	return boolStruct;
}

/**
 * Get a new struct by adding each boolean element of two structs
 * with the same nested boolean struct array with the XOR (^) operator.
 *
 * @param arg1 the 1st struct with a nested boolean struct array
 * @param arg2 the 2nd struct with a nested boolean struct array
 * @return a struct with a nested boolean struct array
 */
stru_NestedStruArray_Bool
addNestedBoolStructArrayStructsWithXor_dupStruct(stru_NestedStruArray_Bool arg1, stru_NestedStruArray_Bool arg2)
{
	stru_NestedStruArray_Bool boolStruct;
	boolStruct.elem1[0].elem1 = arg1.elem1[0].elem1 ^ arg2.elem1[0].elem1;
	boolStruct.elem1[0].elem2 = arg1.elem1[0].elem2 ^ arg2.elem1[0].elem2;
	boolStruct.elem1[1].elem1 = arg1.elem1[1].elem1 ^ arg2.elem1[1].elem1;
	boolStruct.elem1[1].elem2 = arg1.elem1[1].elem2 ^ arg2.elem1[1].elem2;
	boolStruct.elem2 = arg1.elem2 ^ arg2.elem2;
	return boolStruct;
}

/**
 * Get a new struct by adding each boolean element of two structs with different elements
 * with the XOR (^) operator.
 *
 * @param arg1 the 1st struct with two booleans
 * @param arg2 the 2nd struct with a nested boolean struct array
 * @return a struct with two booleans
 */
stru_2_Bools
addBoolStruct1AndNestedBoolStructArrayStruct2WithXor_returnStruct1_dupStruct(stru_2_Bools arg1, stru_NestedStruArray_Bool arg2)
{
	stru_2_Bools boolStruct;
	boolStruct.elem1 = arg1.elem1 ^ arg2.elem1[0].elem1 ^ arg2.elem1[1].elem1;
	boolStruct.elem2 = arg1.elem2 ^ arg2.elem1[0].elem2 ^ arg2.elem1[1].elem2 ^ arg2.elem2;
	return boolStruct;
}

/**
 * Get a new struct by adding each boolean element of two structs with different elements
 * with the XOR (^) operator.
 *
 * @param arg1 the 1st struct with two booleans
 * @param arg2 the 2nd struct with a nested boolean struct array
 * @return a struct with a nested boolean struct array
 */
stru_NestedStruArray_Bool
addBoolStruct1AndNestedBoolStructArrayStruct2WithXor_returnStruct2_dupStruct(stru_2_Bools arg1, stru_NestedStruArray_Bool arg2)
{
	stru_NestedStruArray_Bool boolStruct;
	boolStruct.elem1[0].elem1 = arg1.elem1 ^ arg2.elem1[0].elem1;
	boolStruct.elem1[0].elem2 = arg1.elem2 ^ arg2.elem1[0].elem2;
	boolStruct.elem1[1].elem1 = arg1.elem1 ^ arg2.elem1[1].elem1;
	boolStruct.elem1[1].elem2 = arg1.elem2 ^ arg2.elem1[1].elem2;
	boolStruct.elem2 = arg1.elem2 ^ arg2.elem2;
	return boolStruct;
}

/**
 * Get a new struct by adding each byte element of two structs
 * with the same nested byte struct.
 *
 * @param arg1 the 1st struct with a nested byte struct
 * @param arg2 the 2nd struct with a nested byte struct
 * @return a struct with a nested byte struct
 */
stru_NestedStruct_Byte
addNestedByteStructs_dupStruct(stru_NestedStruct_Byte arg1, stru_NestedStruct_Byte arg2)
{
	stru_NestedStruct_Byte byteStruct;
	byteStruct.elem1.elem1 = arg1.elem1.elem1 + arg2.elem1.elem1;
	byteStruct.elem1.elem2 = arg1.elem1.elem2 + arg2.elem1.elem2;
	byteStruct.elem2 = arg1.elem2 + arg2.elem2;
	return byteStruct;
}

/**
 * Get a new struct by adding each byte element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two bytes
 * @param arg2 the 2nd struct with a nested byte struct
 * @return a struct with two bytes
 */
stru_2_Bytes
addByteStruct1AndNestedByteStruct2_returnStruct1_dupStruct(stru_2_Bytes arg1, stru_NestedStruct_Byte arg2)
{
	stru_2_Bytes byteStruct;
	byteStruct.elem1 = arg1.elem1 + arg2.elem1.elem1;
	byteStruct.elem2 = arg1.elem2 + arg2.elem1.elem2 + arg2.elem2;
	return byteStruct;
}

/**
 * Get a new struct by adding each byte element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two bytes
 * @param arg2 the 2nd struct with a nested byte struct
 * @return a struct with a nested byte struct
 */
stru_NestedStruct_Byte
addByteStruct1AndNestedByteStruct2_returnStruct2_dupStruct(stru_2_Bytes arg1, stru_NestedStruct_Byte arg2)
{
	stru_NestedStruct_Byte byteStruct;
	byteStruct.elem1.elem1 = arg1.elem1 + arg2.elem1.elem1;
	byteStruct.elem1.elem2 = arg1.elem2 + arg2.elem1.elem2;
	byteStruct.elem2 = arg1.elem2 + arg2.elem2;
	return byteStruct;
}

/**
 * Get a new struct by adding each byte element of two structs
 * with the same nested byte array.
 *
 * @param arg1 the 1st struct with a nested byte array
 * @param arg2 the 2nd struct with a nested byte array
 * @return a struct with a nested byte array
 */
stru_NestedByteArray_Byte
addNestedByteArrayStructs_dupStruct(stru_NestedByteArray_Byte arg1, stru_NestedByteArray_Byte arg2)
{
	stru_NestedByteArray_Byte byteStruct;
	byteStruct.elem1[0] = arg1.elem1[0] + arg2.elem1[0];
	byteStruct.elem1[1] = arg1.elem1[1] + arg2.elem1[1];
	byteStruct.elem2 = arg1.elem2 + arg2.elem2;
	return byteStruct;
}

/**
 * Get a new struct by adding each byte element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two bytes
 * @param arg2 the 2nd struct with a nested byte array
 * @return a struct with two bytes
 */
stru_2_Bytes
addByteStruct1AndNestedByteArrayStruct2_returnStruct1_dupStruct(stru_2_Bytes arg1, stru_NestedByteArray_Byte arg2)
{
	stru_2_Bytes byteStruct;
	byteStruct.elem1 = arg1.elem1 + arg2.elem1[0];
	byteStruct.elem2 = arg1.elem2 + arg2.elem1[1] + arg2.elem2;
	return byteStruct;
}

/**
 * Get a new struct by adding each byte element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two bytes
 * @param arg2 the 2nd struct with a nested byte array
 * @return a struct with a nested byte array
 */
stru_NestedByteArray_Byte
addByteStruct1AndNestedByteArrayStruct2_returnStruct2_dupStruct(stru_2_Bytes arg1, stru_NestedByteArray_Byte arg2)
{
	stru_NestedByteArray_Byte byteStruct;
	byteStruct.elem1[0] = arg1.elem1 + arg2.elem1[0];
	byteStruct.elem1[1] = arg1.elem2 + arg2.elem1[1];
	byteStruct.elem2 = arg1.elem2 + arg2.elem2;
	return byteStruct;
}

/**
 * Get a new struct by adding each byte element of two structs
 * with the same nested byte struct array.
 *
 * @param arg1 the 1st struct with a nested byte struct array
 * @param arg2 the 2nd struct with a nested byte struct array
 * @return a struct with a nested byte struct array
 */
stru_NestedStruArray_Byte
addNestedByteStructArrayStructs_dupStruct(stru_NestedStruArray_Byte arg1, stru_NestedStruArray_Byte arg2)
{
	stru_NestedStruArray_Byte byteStruct;
	byteStruct.elem1[0].elem1 = arg1.elem1[0].elem1 + arg2.elem1[0].elem1;
	byteStruct.elem1[0].elem2 = arg1.elem1[0].elem2 + arg2.elem1[0].elem2;
	byteStruct.elem1[1].elem1 = arg1.elem1[1].elem1 + arg2.elem1[1].elem1;
	byteStruct.elem1[1].elem2 = arg1.elem1[1].elem2 + arg2.elem1[1].elem2;
	byteStruct.elem2 = arg1.elem2 + arg2.elem2;
	return byteStruct;
}

/**
 * Get a new struct by adding each byte element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two bytes
 * @param arg2 the 2nd struct with a nested byte struct array
 * @return a struct with two bytes
 */
stru_2_Bytes
addByteStruct1AndNestedByteStructArrayStruct2_returnStruct1_dupStruct(stru_2_Bytes arg1, stru_NestedStruArray_Byte arg2)
{
	stru_2_Bytes byteStruct;
	byteStruct.elem1 = arg1.elem1 + arg2.elem1[0].elem1 + arg2.elem1[1].elem1;
	byteStruct.elem2 = arg1.elem2 + arg2.elem1[0].elem2 + arg2.elem1[1].elem2 + arg2.elem2;
	return byteStruct;
}

/**
 * Get a new struct by adding each byte element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two bytes
 * @param arg2 the 2nd struct with a nested byte struct array
 * @return a struct with a nested byte struct array
 */
stru_NestedStruArray_Byte
addByteStruct1AndNestedByteStructArrayStruct2_returnStruct2_dupStruct(stru_2_Bytes arg1, stru_NestedStruArray_Byte arg2)
{
	stru_NestedStruArray_Byte byteStruct;
	byteStruct.elem1[0].elem1 = arg1.elem1 + arg2.elem1[0].elem1;
	byteStruct.elem1[0].elem2 = arg1.elem2 + arg2.elem1[0].elem2;
	byteStruct.elem1[1].elem1 = arg1.elem1 + arg2.elem1[1].elem1;
	byteStruct.elem1[1].elem2 = arg1.elem2 + arg2.elem1[1].elem2;
	byteStruct.elem2 = arg1.elem2 + arg2.elem2;
	return byteStruct;
}

/**
 * Get a new struct by adding each char element of two structs
 * with the same nested char struct.
 *
 * @param arg1 the 1st struct with a nested char struct
 * @param arg2 the 2nd struct with a nested char struct
 * @return a struct with a nested char struct
 */
stru_NestedStruct_Char
addNestedCharStructs_dupStruct(stru_NestedStruct_Char arg1, stru_NestedStruct_Char arg2)
{
	stru_NestedStruct_Char charStruct;
	charStruct.elem1.elem1 = arg1.elem1.elem1 + arg2.elem1.elem1 - 'A';
	charStruct.elem1.elem2 = arg1.elem1.elem2 + arg2.elem1.elem2 - 'A';
	charStruct.elem2 = arg1.elem2 + arg2.elem2 - 'A';
	return charStruct;
}

/**
 * Get a new struct by adding each char element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two chars
 * @param arg2 the 2nd struct with a nested char struct
 * @return a struct with two chars
 */
stru_2_Chars
addCharStruct1AndNestedCharStruct2_returnStruct1_dupStruct(stru_2_Chars arg1, stru_NestedStruct_Char arg2)
{
	stru_2_Chars charStruct;
	charStruct.elem1 = arg1.elem1 + arg2.elem1.elem1 - 'A';
	charStruct.elem2 = arg1.elem2 + arg2.elem1.elem2 + arg2.elem2 -  (2 * 'A');
	return charStruct;
}

/**
 * Get a new struct by adding each char element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two chars
 * @param arg2 the 2nd struct with a nested char struct
 * @return a struct with a nested char struct
 */
stru_NestedStruct_Char
addCharStruct1AndNestedCharStruct2_returnStruct2_dupStruct(stru_2_Chars arg1, stru_NestedStruct_Char arg2)
{
	stru_NestedStruct_Char charStruct;
	charStruct.elem1.elem1 = arg1.elem1 + arg2.elem1.elem1 - 'A';
	charStruct.elem1.elem2 = arg1.elem2 + arg2.elem1.elem2 - 'A';
	charStruct.elem2 = arg1.elem2 + arg2.elem2 - 'A';
	return charStruct;
}

/**
 * Get a new struct by adding each char element of two structs
 * with the same nested char array.
 *
 * @param arg1 the 1st struct with a nested char array
 * @param arg2 the 2nd struct with a nested char array
 * @return a struct with a nested char array
 */
stru_NestedCharArray_Char
addNestedCharArrayStructs_dupStruct(stru_NestedCharArray_Char arg1, stru_NestedCharArray_Char arg2)
{
	stru_NestedCharArray_Char charStruct;
	charStruct.elem1[0] = arg1.elem1[0] + arg2.elem1[0] - 'A';
	charStruct.elem1[1] = arg1.elem1[1] + arg2.elem1[1] - 'A';
	charStruct.elem2 = arg1.elem2 + arg2.elem2 - 'A';
	return charStruct;
}

/**
 * Get a new struct by adding each char element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two chars
 * @param arg2 the 2nd struct with a nested char array
 * @return a struct with two chars
 */
stru_2_Chars
addCharStruct1AndNestedCharArrayStruct2_returnStruct1_dupStruct(stru_2_Chars arg1, stru_NestedCharArray_Char arg2)
{
	stru_2_Chars charStruct;
	charStruct.elem1 = arg1.elem1 + arg2.elem1[0] - 'A';
	charStruct.elem2 = arg1.elem2 + arg2.elem1[1] + arg2.elem2 - (2 * 'A');
	return charStruct;
}

/**
 * Get a new struct by adding each char element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two chars
 * @param arg2 the 2nd struct with a nested char array
 * @return a struct with a nested char array
 */
stru_NestedCharArray_Char
addCharStruct1AndNestedCharArrayStruct2_returnStruct2_dupStruct(stru_2_Chars arg1, stru_NestedCharArray_Char arg2)
{
	stru_NestedCharArray_Char charStruct;
	charStruct.elem1[0] = arg1.elem1 + arg2.elem1[0] - 'A';
	charStruct.elem1[1] = arg1.elem2 + arg2.elem1[1] - 'A';
	charStruct.elem2 = arg1.elem2 + arg2.elem2 - 'A';
	return charStruct;
}

/**
 * Get a new struct by adding each char element of two structs
 * with the same nested char struct array.
 *
 * @param arg1 the 1st struct with a nested char struct array
 * @param arg2 the 2nd struct with a nested char struct array
 * @return a struct with a nested char struct array
 */
stru_NestedStruArray_Char
addNestedCharStructArrayStructs_dupStruct(stru_NestedStruArray_Char arg1, stru_NestedStruArray_Char arg2)
{
	stru_NestedStruArray_Char charStruct;
	charStruct.elem1[0].elem1 = arg1.elem1[0].elem1 + arg2.elem1[0].elem1 - 'A';
	charStruct.elem1[0].elem2 = arg1.elem1[0].elem2 + arg2.elem1[0].elem2 - 'A';
	charStruct.elem1[1].elem1 = arg1.elem1[1].elem1 + arg2.elem1[1].elem1 - 'A';
	charStruct.elem1[1].elem2 = arg1.elem1[1].elem2 + arg2.elem1[1].elem2 - 'A';
	charStruct.elem2 = arg1.elem2 + arg2.elem2 - 'A';
	return charStruct;
}

/**
 * Get a new struct by adding each char element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two chars
 * @param arg2 the 2nd struct with a nested char struct array
 * @return a struct with two chars
 */
stru_2_Chars
addCharStruct1AndNestedCharStructArrayStruct2_returnStruct1_dupStruct(stru_2_Chars arg1, stru_NestedStruArray_Char arg2)
{
	stru_2_Chars charStruct;
	charStruct.elem1 = arg1.elem1 + arg2.elem1[0].elem1 + arg2.elem1[1].elem1 - (2 * 'A');
	charStruct.elem2 = arg1.elem2 + arg2.elem1[0].elem2 + arg2.elem1[1].elem2 + arg2.elem2 - (3 * 'A');
	return charStruct;
}

/**
 * Get a new struct by adding each char element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two chars
 * @param arg2 the 2nd struct with a nested char struct array
 * @return a struct with a nested char struct array
 */
stru_NestedStruArray_Char
addCharStruct1AndNestedCharStructArrayStruct2_returnStruct2_dupStruct(stru_2_Chars arg1, stru_NestedStruArray_Char arg2)
{
	stru_NestedStruArray_Char charStruct;
	charStruct.elem1[0].elem1 = arg1.elem1 + arg2.elem1[0].elem1 - 'A';
	charStruct.elem1[0].elem2 = arg1.elem2 + arg2.elem1[0].elem2 - 'A';
	charStruct.elem1[1].elem1 = arg1.elem1 + arg2.elem1[1].elem1 - 'A';
	charStruct.elem1[1].elem2 = arg1.elem2 + arg2.elem1[1].elem2 - 'A';
	charStruct.elem2 = arg1.elem2 + arg2.elem2 - 'A';
	return charStruct;
}

/**
 * Get a new struct by adding each short element of two structs
 * with the same nested short struct.
 *
 * @param arg1 the 1st struct with a nested short struct
 * @param arg2 the 2nd struct with a nested short struct
 * @return a struct with a nested short struct
 */
stru_NestedStruct_Short
addNestedShortStructs_dupStruct(stru_NestedStruct_Short arg1, stru_NestedStruct_Short arg2)
{
	stru_NestedStruct_Short shortStruct;
	shortStruct.elem1.elem1 = arg1.elem1.elem1 + arg2.elem1.elem1;
	shortStruct.elem1.elem2 = arg1.elem1.elem2 + arg2.elem1.elem2;
	shortStruct.elem2 = arg1.elem2 + arg2.elem2;
	return shortStruct;
}

/**
 * Get a new struct by adding each short element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two shorts
 * @param arg2 the 2nd struct with a nested short struct
 * @return a struct with two shorts
 */
stru_2_Shorts
addShortStruct1AndNestedShortStruct2_returnStruct1_dupStruct(stru_2_Shorts arg1, stru_NestedStruct_Short arg2)
{
	stru_2_Shorts shortStruct;
	shortStruct.elem1 = arg1.elem1 + arg2.elem1.elem1;
	shortStruct.elem2 = arg1.elem2 + arg2.elem1.elem2 + arg2.elem2;
	return shortStruct;
}

/**
 * Get a new struct by adding each short element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two shorts
 * @param arg2 the 2nd struct with a nested short struct
 * @return a struct with a nested short struct
 */
stru_NestedStruct_Short
addShortStruct1AndNestedShortStruct2_returnStruct2_dupStruct(stru_2_Shorts arg1, stru_NestedStruct_Short arg2)
{
	stru_NestedStruct_Short shortStruct;
	shortStruct.elem1.elem1 = arg1.elem1 + arg2.elem1.elem1;
	shortStruct.elem1.elem2 = arg1.elem2 + arg2.elem1.elem2;
	shortStruct.elem2 = arg1.elem2 + arg2.elem2;
	return shortStruct;
}

/**
 * Get a new struct by adding each short element of two structs
 * with the same nested short array.
 *
 * @param arg1 the 1st struct with a nested short array
 * @param arg2 the 2nd struct with a nested short array
 * @return a struct with a nested short array
 */
stru_NestedShortArray_Short
addNestedShortArrayStructs_dupStruct(stru_NestedShortArray_Short arg1, stru_NestedShortArray_Short arg2)
{
	stru_NestedShortArray_Short shortStruct;
	shortStruct.elem1[0] = arg1.elem1[0] + arg2.elem1[0];
	shortStruct.elem1[1] = arg1.elem1[1] + arg2.elem1[1];
	shortStruct.elem2 = arg1.elem2 + arg2.elem2;
	return shortStruct;
}

/**
 * Get a new struct by adding each short element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two shorts
 * @param arg2 the 2nd struct with a nested short array
 * @return a struct with two shorts
 */
stru_2_Shorts
addShortStruct1AndNestedShortArrayStruct2_returnStruct1_dupStruct(stru_2_Shorts arg1, stru_NestedShortArray_Short arg2)
{
	stru_2_Shorts shortStruct;
	shortStruct.elem1 = arg1.elem1 + arg2.elem1[0];
	shortStruct.elem2 = arg1.elem2 + arg2.elem1[1] + arg2.elem2;
	return shortStruct;
}

/**
 * Get a new struct by adding each short element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two shorts
 * @param arg2 the 2nd struct with a nested short array
 * @return a struct with a nested short array
 */
stru_NestedShortArray_Short
addShortStruct1AndNestedShortArrayStruct2_returnStruct2_dupStruct(stru_2_Shorts arg1, stru_NestedShortArray_Short arg2)
{
	stru_NestedShortArray_Short shortStruct;
	shortStruct.elem1[0] = arg1.elem1 + arg2.elem1[0];
	shortStruct.elem1[1] = arg1.elem2 + arg2.elem1[1];
	shortStruct.elem2 = arg1.elem2 + arg2.elem2;
	return shortStruct;
}

/**
 * Get a new struct by adding each short element of two structs
 * with the same nested short struct array.
 *
 * @param arg1 the 1st struct with a nested short struct array
 * @param arg2 the 2nd struct with a nested short struct array
 * @return a struct with a nested short struct array
 */
stru_NestedStruArray_Short
addNestedShortStructArrayStructs_dupStruct(stru_NestedStruArray_Short arg1, stru_NestedStruArray_Short arg2)
{
	stru_NestedStruArray_Short shortStruct;
	shortStruct.elem1[0].elem1 = arg1.elem1[0].elem1 + arg2.elem1[0].elem1;
	shortStruct.elem1[0].elem2 = arg1.elem1[0].elem2 + arg2.elem1[0].elem2;
	shortStruct.elem1[1].elem1 = arg1.elem1[1].elem1 + arg2.elem1[1].elem1;
	shortStruct.elem1[1].elem2 = arg1.elem1[1].elem2 + arg2.elem1[1].elem2;
	shortStruct.elem2 = arg1.elem2 + arg2.elem2;
	return shortStruct;
}

/**
 * Get a new struct by adding each short element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two shorts
 * @param arg2 the 2nd struct with a nested short struct array
 * @return a struct with two shorts
 */
stru_2_Shorts
addShortStruct1AndNestedShortStructArrayStruct2_returnStruct1_dupStruct(stru_2_Shorts arg1, stru_NestedStruArray_Short arg2)
{
	stru_2_Shorts shortStruct;
	shortStruct.elem1 = arg1.elem1 + arg2.elem1[0].elem1 + arg2.elem1[1].elem1;
	shortStruct.elem2 = arg1.elem2 + arg2.elem1[0].elem2 + arg2.elem1[1].elem2 + arg2.elem2;
	return shortStruct;
}

/**
 * Get a new struct by adding each short element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two shorts
 * @param arg2 the 2nd struct with a nested short struct array
 * @return a struct with a nested short struct array
 */
stru_NestedStruArray_Short
addShortStruct1AndNestedShortStructArrayStruct2_returnStruct2_dupStruct(stru_2_Shorts arg1, stru_NestedStruArray_Short arg2)
{
	stru_NestedStruArray_Short shortStruct;
	shortStruct.elem1[0].elem1 = arg1.elem1 + arg2.elem1[0].elem1;
	shortStruct.elem1[0].elem2 = arg1.elem2 + arg2.elem1[0].elem2;
	shortStruct.elem1[1].elem1 = arg1.elem1 + arg2.elem1[1].elem1;
	shortStruct.elem1[1].elem2 = arg1.elem2 + arg2.elem1[1].elem2;
	shortStruct.elem2 = arg1.elem2 + arg2.elem2;
	return shortStruct;
}

/**
 * Get a new struct by adding each integer element of two structs
 * with the same nested integer struct.
 *
 * @param arg1 the 1st struct with a nested integer struct
 * @param arg2 the 2nd struct with a nested integer struct
 * @return a struct with a nested integer struct
 */
stru_NestedStruct_Int
addNestedIntStructs_dupStruct(stru_NestedStruct_Int arg1, stru_NestedStruct_Int arg2)
{
	stru_NestedStruct_Int intStruct;
	intStruct.elem1.elem1 = arg1.elem1.elem1 + arg2.elem1.elem1;
	intStruct.elem1.elem2 = arg1.elem1.elem2 + arg2.elem1.elem2;
	intStruct.elem2 = arg1.elem2 + arg2.elem2;
	return intStruct;
}

/**
 * Get a new struct by adding each integer element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two integers
 * @param arg2 the 2nd struct with a nested integer struct
 * @return a struct with two integers
 */
stru_2_Ints
addIntStruct1AndNestedIntStruct2_returnStruct1_dupStruct(stru_2_Ints arg1, stru_NestedStruct_Int arg2)
{
	stru_2_Ints intStruct;
	intStruct.elem1 = arg1.elem1 + arg2.elem1.elem1;
	intStruct.elem2 = arg1.elem2 + arg2.elem1.elem2 + arg2.elem2;
	return intStruct;
}

/**
 * Get a new struct by adding each integer element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two integers
 * @param arg2 the 2nd struct with a nested integer struct
 * @return a struct with a nested integer struct
 */
stru_NestedStruct_Int
addIntStruct1AndNestedIntStruct2_returnStruct2_dupStruct(stru_2_Ints arg1, stru_NestedStruct_Int arg2)
{
	stru_NestedStruct_Int intStruct;
	intStruct.elem1.elem1 = arg1.elem1 + arg2.elem1.elem1;
	intStruct.elem1.elem2 = arg1.elem2 + arg2.elem1.elem2;
	intStruct.elem2 = arg1.elem2 + arg2.elem2;
	return intStruct;
}

/**
 * Get a new struct by adding each integer element of two structs
 * with the same nested integer array.
 *
 * @param arg1 the 1st struct with a nested integer array
 * @param arg2 the 2nd struct with a nested integer array
 * @return a struct with a nested integer array
 */
stru_NestedIntArray_Int
addNestedIntArrayStructs_dupStruct(stru_NestedIntArray_Int arg1, stru_NestedIntArray_Int arg2)
{
	stru_NestedIntArray_Int intStruct;
	intStruct.elem1[0] = arg1.elem1[0] + arg2.elem1[0];
	intStruct.elem1[1] = arg1.elem1[1] + arg2.elem1[1];
	intStruct.elem2 = arg1.elem2 + arg2.elem2;
	return intStruct;
}

/**
 * Get a new struct by adding each integer element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two integers
 * @param arg2 the 2nd struct with a nested integer array
 * @return a struct with two integers
 */
stru_2_Ints
addIntStruct1AndNestedIntArrayStruct2_returnStruct1_dupStruct(stru_2_Ints arg1, stru_NestedIntArray_Int arg2)
{
	stru_2_Ints intStruct;
	intStruct.elem1 = arg1.elem1 + arg2.elem1[0];
	intStruct.elem2 = arg1.elem2 + arg2.elem1[1] + arg2.elem2;
	return intStruct;
}

/**
 * Get a new struct by adding each integer element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two integers
 * @param arg2 the 2nd struct with a nested integer array
 * @return a struct with a nested integer array
 */
stru_NestedIntArray_Int
addIntStruct1AndNestedIntArrayStruct2_returnStruct2_dupStruct(stru_2_Ints arg1, stru_NestedIntArray_Int arg2)
{
	stru_NestedIntArray_Int intStruct;
	intStruct.elem1[0] = arg1.elem1 + arg2.elem1[0];
	intStruct.elem1[1] = arg1.elem2 + arg2.elem1[1];
	intStruct.elem2 = arg1.elem2 + arg2.elem2;
	return intStruct;
}

/**
 * Get a new struct by adding each integer element of two structs
 * with the same nested integer struct array.
 *
 * @param arg1 the 1st struct with a nested integer struct array
 * @param arg2 the 2nd struct with a nested integer struct array
 * @return a struct with a nested integer struct array
 */
stru_NestedStruArray_Int
addNestedIntStructArrayStructs_dupStruct(stru_NestedStruArray_Int arg1, stru_NestedStruArray_Int arg2)
{
	stru_NestedStruArray_Int intStruct;
	intStruct.elem1[0].elem1 = arg1.elem1[0].elem1 + arg2.elem1[0].elem1;
	intStruct.elem1[0].elem2 = arg1.elem1[0].elem2 + arg2.elem1[0].elem2;
	intStruct.elem1[1].elem1 = arg1.elem1[1].elem1 + arg2.elem1[1].elem1;
	intStruct.elem1[1].elem2 = arg1.elem1[1].elem2 + arg2.elem1[1].elem2;
	intStruct.elem2 = arg1.elem2 + arg2.elem2;
	return intStruct;
}

/**
 * Get a new struct by adding each integer element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two integers
 * @param arg2 the 2nd struct with a nested integer struct array
 * @return a struct with two integers
 */
stru_2_Ints
addIntStruct1AndNestedIntStructArrayStruct2_returnStruct1_dupStruct(stru_2_Ints arg1, stru_NestedStruArray_Int arg2)
{
	stru_2_Ints intStruct;
	intStruct.elem1 = arg1.elem1 + arg2.elem1[0].elem1 + arg2.elem1[1].elem1;
	intStruct.elem2 = arg1.elem2 + arg2.elem1[0].elem2 + arg2.elem1[1].elem2 + arg2.elem2;
	return intStruct;
}

/**
 * Get a new struct by adding each integer element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two integers
 * @param arg2 the 2nd struct with a nested integer struct array
 * @return a struct with a nested integer struct array
 */
stru_NestedStruArray_Int
addIntStruct1AndNestedIntStructArrayStruct2_returnStruct2_dupStruct(stru_2_Ints arg1, stru_NestedStruArray_Int arg2)
{
	stru_NestedStruArray_Int intStruct;
	intStruct.elem1[0].elem1 = arg1.elem1 + arg2.elem1[0].elem1;
	intStruct.elem1[0].elem2 = arg1.elem2 + arg2.elem1[0].elem2;
	intStruct.elem1[1].elem1 = arg1.elem1 + arg2.elem1[1].elem1;
	intStruct.elem1[1].elem2 = arg1.elem2 + arg2.elem1[1].elem2;
	intStruct.elem2 = arg1.elem2 + arg2.elem2;
	return intStruct;
}

/**
 * Get a new struct by adding each long element of two structs
 * with the same nested long struct.
 *
 * @param arg1 the 1st struct with a nested long struct
 * @param arg2 the 2nd struct with a nested long struct
 * @return a struct with a nested long struct
 */
stru_NestedStruct_Long
addNestedLongStructs_dupStruct(stru_NestedStruct_Long arg1, stru_NestedStruct_Long arg2)
{
	stru_NestedStruct_Long longStruct;
	longStruct.elem1.elem1 = arg1.elem1.elem1 + arg2.elem1.elem1;
	longStruct.elem1.elem2 = arg1.elem1.elem2 + arg2.elem1.elem2;
	longStruct.elem2 = arg1.elem2 + arg2.elem2;
	return longStruct;
}

/**
 * Get a new struct by adding each long element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two longs
 * @param arg2 the 2nd struct with a nested long struct
 * @return a struct with two longs
 */
stru_2_Longs
addLongStruct1AndNestedLongStruct2_returnStruct1_dupStruct(stru_2_Longs arg1, stru_NestedStruct_Long arg2)
{
	stru_2_Longs longStruct;
	longStruct.elem1 = arg1.elem1 + arg2.elem1.elem1;
	longStruct.elem2 = arg1.elem2 + arg2.elem1.elem2 + arg2.elem2;
	return longStruct;
}

/**
 * Get a new struct by adding each long element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two longs
 * @param arg2 the 2nd struct with a nested long struct
 * @return a struct with a nested long struct
 */
stru_NestedStruct_Long
addLongStruct1AndNestedLongStruct2_returnStruct2_dupStruct(stru_2_Longs arg1, stru_NestedStruct_Long arg2)
{
	stru_NestedStruct_Long longStruct;
	longStruct.elem1.elem1 = arg1.elem1 + arg2.elem1.elem1;
	longStruct.elem1.elem2 = arg1.elem2 + arg2.elem1.elem2;
	longStruct.elem2 = arg1.elem2 + arg2.elem2;
	return longStruct;
}

/**
 * Get a new struct by adding each long element of two structs
 * with the same nested long array.
 *
 * @param arg1 the 1st struct with a nested long array
 * @param arg2 the 2nd struct with a nested long array
 * @return a struct with a nested long array
 */
stru_NestedLongArray_Long
addNestedLongArrayStructs_dupStruct(stru_NestedLongArray_Long arg1, stru_NestedLongArray_Long arg2)
{
	stru_NestedLongArray_Long longStruct;
	longStruct.elem1[0] = arg1.elem1[0] + arg2.elem1[0];
	longStruct.elem1[1] = arg1.elem1[1] + arg2.elem1[1];
	longStruct.elem2 = arg1.elem2 + arg2.elem2;
	return longStruct;
}

/**
 * Get a new struct by adding each long element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two longs
 * @param arg2 the 2nd struct with a nested long array
 * @return a struct with two longs
 */
stru_2_Longs
addLongStruct1AndNestedLongArrayStruct2_returnStruct1_dupStruct(stru_2_Longs arg1, stru_NestedLongArray_Long arg2)
{
	stru_2_Longs longStruct;
	longStruct.elem1 = arg1.elem1 + arg2.elem1[0];
	longStruct.elem2 = arg1.elem2 + arg2.elem1[1] + arg2.elem2;
	return longStruct;
}

/**
 * Get a new struct by adding each long element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two longs
 * @param arg2 the 2nd struct with a nested long array
 * @return a struct with a nested long array
 */
stru_NestedLongArray_Long
addLongStruct1AndNestedLongArrayStruct2_returnStruct2_dupStruct(stru_2_Longs arg1, stru_NestedLongArray_Long arg2)
{
	stru_NestedLongArray_Long longStruct;
	longStruct.elem1[0] = arg1.elem1 + arg2.elem1[0];
	longStruct.elem1[1] = arg1.elem2 + arg2.elem1[1];
	longStruct.elem2 = arg1.elem2 + arg2.elem2;
	return longStruct;
}

/**
 * Get a new struct by adding each long element of two structs
 * with the same nested long struct array.
 *
 * @param arg1 the 1st struct with a nested long struct array
 * @param arg2 the 2nd struct with a nested long struct array
 * @return a struct with a nested long struct array
 */
stru_NestedStruArray_Long
addNestedLongStructArrayStructs_dupStruct(stru_NestedStruArray_Long arg1, stru_NestedStruArray_Long arg2)
{
	stru_NestedStruArray_Long longStruct;
	longStruct.elem1[0].elem1 = arg1.elem1[0].elem1 + arg2.elem1[0].elem1;
	longStruct.elem1[0].elem2 = arg1.elem1[0].elem2 + arg2.elem1[0].elem2;
	longStruct.elem1[1].elem1 = arg1.elem1[1].elem1 + arg2.elem1[1].elem1;
	longStruct.elem1[1].elem2 = arg1.elem1[1].elem2 + arg2.elem1[1].elem2;
	longStruct.elem2 = arg1.elem2 + arg2.elem2;
	return longStruct;
}

/**
 * Get a new struct by adding each long element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two longs
 * @param arg2 the 2nd struct with a nested long struct array
 * @return a struct with two longs
 */
stru_2_Longs
addLongStruct1AndNestedLongStructArrayStruct2_returnStruct1_dupStruct(stru_2_Longs arg1, stru_NestedStruArray_Long arg2)
{
	stru_2_Longs longStruct;
	longStruct.elem1 = arg1.elem1 + arg2.elem1[0].elem1 + arg2.elem1[1].elem1;
	longStruct.elem2 = arg1.elem2 + arg2.elem1[0].elem2 + arg2.elem1[1].elem2 + arg2.elem2;
	return longStruct;
}

/**
 * Get a new struct by adding each long element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two longs
 * @param arg2 the 2nd struct with a nested long struct array
 * @return a struct with a nested long struct array
 */
stru_NestedStruArray_Long
addLongStruct1AndNestedLongStructArrayStruct2_returnStruct2_dupStruct(stru_2_Longs arg1, stru_NestedStruArray_Long arg2)
{
	stru_NestedStruArray_Long longStruct;
	longStruct.elem1[0].elem1 = arg1.elem1 + arg2.elem1[0].elem1;
	longStruct.elem1[0].elem2 = arg1.elem2 + arg2.elem1[0].elem2;
	longStruct.elem1[1].elem1 = arg1.elem1 + arg2.elem1[1].elem1;
	longStruct.elem1[1].elem2 = arg1.elem2 + arg2.elem1[1].elem2;
	longStruct.elem2 = arg1.elem2 + arg2.elem2;
	return longStruct;
}

/**
 * Get a new struct by adding each float element of two structs
 * with the same nested float struct.
 *
 * @param arg1 the 1st struct with a nested float struct
 * @param arg2 the 2nd struct with a nested float struct
 * @return a struct with a nested float struct
 */
stru_NestedStruct_Float
addNestedFloatStructs_dupStruct(stru_NestedStruct_Float arg1, stru_NestedStruct_Float arg2)
{
	stru_NestedStruct_Float floatStruct;
	floatStruct.elem1.elem1 = arg1.elem1.elem1 + arg2.elem1.elem1;
	floatStruct.elem1.elem2 = arg1.elem1.elem2 + arg2.elem1.elem2;
	floatStruct.elem2 = arg1.elem2 + arg2.elem2;
	return floatStruct;
}

/**
 * Get a new struct by adding each float element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two floats
 * @param arg2 the 2nd struct with a nested float struct
 * @return a struct with two floats
 */
stru_2_Floats
addFloatStruct1AndNestedFloatStruct2_returnStruct1_dupStruct(stru_2_Floats arg1, stru_NestedStruct_Float arg2)
{
	stru_2_Floats floatStruct;
	floatStruct.elem1 = arg1.elem1 + arg2.elem1.elem1;
	floatStruct.elem2 = arg1.elem2 + arg2.elem1.elem2 + arg2.elem2;
	return floatStruct;
}

/**
 * Get a new struct by adding each float element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two floats
 * @param arg2 the 2nd struct with a nested float struct
 * @return a struct with a nested float struct
 */
stru_NestedStruct_Float
addFloatStruct1AndNestedFloatStruct2_returnStruct2_dupStruct(stru_2_Floats arg1, stru_NestedStruct_Float arg2)
{
	stru_NestedStruct_Float floatStruct;
	floatStruct.elem1.elem1 = arg1.elem1 + arg2.elem1.elem1;
	floatStruct.elem1.elem2 = arg1.elem2 + arg2.elem1.elem2;
	floatStruct.elem2 = arg1.elem2 + arg2.elem2;
	return floatStruct;
}

/**
 * Get a new struct by adding each float element of two structs
 * with the same nested float array.
 *
 * @param arg1 the 1st struct with a nested float array
 * @param arg2 the 2nd struct with a nested float array
 * @return a struct with a nested float array
 */
stru_NestedFloatArray_Float
addNestedFloatArrayStructs_dupStruct(stru_NestedFloatArray_Float arg1, stru_NestedFloatArray_Float arg2)
{
	stru_NestedFloatArray_Float floatStruct;
	floatStruct.elem1[0] = arg1.elem1[0] + arg2.elem1[0];
	floatStruct.elem1[1] = arg1.elem1[1] + arg2.elem1[1];
	floatStruct.elem2 = arg1.elem2 + arg2.elem2;
	return floatStruct;
}

/**
 * Get a new struct by adding each float element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two floats
 * @param arg2 the 2nd struct with a nested float array
 * @return a struct with two floats
 */
stru_2_Floats
addFloatStruct1AndNestedFloatArrayStruct2_returnStruct1_dupStruct(stru_2_Floats arg1, stru_NestedFloatArray_Float arg2)
{
	stru_2_Floats floatStruct;
	floatStruct.elem1 = arg1.elem1 + arg2.elem1[0];
	floatStruct.elem2 = arg1.elem2 + arg2.elem1[1] + arg2.elem2;
	return floatStruct;
}

/**
 * Get a new struct by adding each float element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two floats
 * @param arg2 the 2nd struct with a nested float array
 * @return a struct with a nested float array
 */
stru_NestedFloatArray_Float
addFloatStruct1AndNestedFloatArrayStruct2_returnStruct2_dupStruct(stru_2_Floats arg1, stru_NestedFloatArray_Float arg2)
{
	stru_NestedFloatArray_Float floatStruct;
	floatStruct.elem1[0] = arg1.elem1 + arg2.elem1[0];
	floatStruct.elem1[1] = arg1.elem2 + arg2.elem1[1];
	floatStruct.elem2 = arg1.elem2 + arg2.elem2;
	return floatStruct;
}

/**
 * Get a new struct by adding each float element of two structs
 * with the same nested float struct array.
 *
 * @param arg1 the 1st struct with a nested float struct array
 * @param arg2 the 2nd struct with a nested float struct array
 * @return a struct with a nested float struct array
 */
stru_NestedStruArray_Float
addNestedFloatStructArrayStructs_dupStruct(stru_NestedStruArray_Float arg1, stru_NestedStruArray_Float arg2)
{
	stru_NestedStruArray_Float floatStruct;
	floatStruct.elem1[0].elem1 = arg1.elem1[0].elem1 + arg2.elem1[0].elem1;
	floatStruct.elem1[0].elem2 = arg1.elem1[0].elem2 + arg2.elem1[0].elem2;
	floatStruct.elem1[1].elem1 = arg1.elem1[1].elem1 + arg2.elem1[1].elem1;
	floatStruct.elem1[1].elem2 = arg1.elem1[1].elem2 + arg2.elem1[1].elem2;
	floatStruct.elem2 = arg1.elem2 + arg2.elem2;
	return floatStruct;
}

/**
 * Get a new struct by adding each float element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two floats
 * @param arg2 the 2nd struct with a nested float struct array
 * @return a struct with two floats
 */
stru_2_Floats
addFloatStruct1AndNestedFloatStructArrayStruct2_returnStruct1_dupStruct(stru_2_Floats arg1, stru_NestedStruArray_Float arg2)
{
	stru_2_Floats floatStruct;
	floatStruct.elem1 = arg1.elem1 + arg2.elem1[0].elem1 + arg2.elem1[1].elem1;
	floatStruct.elem2 = arg1.elem2 + arg2.elem1[0].elem2 + arg2.elem1[1].elem2 + arg2.elem2;
	return floatStruct;
}

/**
 * Get a new struct by adding each float element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two floats
 * @param arg2 the 2nd struct with a nested float struct array
 * @return a struct with a nested float struct array
 */
stru_NestedStruArray_Float
addFloatStruct1AndNestedFloatStructArrayStruct2_returnStruct2_dupStruct(stru_2_Floats arg1, stru_NestedStruArray_Float arg2)
{
	stru_NestedStruArray_Float floatStruct;
	floatStruct.elem1[0].elem1 = arg1.elem1 + arg2.elem1[0].elem1;
	floatStruct.elem1[0].elem2 = arg1.elem2 + arg2.elem1[0].elem2;
	floatStruct.elem1[1].elem1 = arg1.elem1 + arg2.elem1[1].elem1;
	floatStruct.elem1[1].elem2 = arg1.elem2 + arg2.elem1[1].elem2;
	floatStruct.elem2 = arg1.elem2 + arg2.elem2;
	return floatStruct;
}

/**
 * Get a new struct by adding each double element of two structs
 * with the same nested double struct.
 *
 * @param arg1 the 1st struct with a nested double struct
 * @param arg2 the 2nd struct with a nested double struct
 * @return a struct with a nested double struct
 */
stru_NestedStruct_Double
addNestedDoubleStructs_dupStruct(stru_NestedStruct_Double arg1, stru_NestedStruct_Double arg2)
{
	stru_NestedStruct_Double doubleStruct;
	doubleStruct.elem1.elem1 = arg1.elem1.elem1 + arg2.elem1.elem1;
	doubleStruct.elem1.elem2 = arg1.elem1.elem2 + arg2.elem1.elem2;
	doubleStruct.elem2 = arg1.elem2 + arg2.elem2;
	return doubleStruct;
}

/**
 * Get a new struct by adding each double element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two doubles
 * @param arg2 the 2nd struct with a nested double struct
 * @return a struct with two doubles
 */
stru_2_Doubles
addDoubleStruct1AndNestedDoubleStruct2_returnStruct1_dupStruct(stru_2_Doubles arg1, stru_NestedStruct_Double arg2)
{
	stru_2_Doubles doubleStruct;
	doubleStruct.elem1 = arg1.elem1 + arg2.elem1.elem1;
	doubleStruct.elem2 = arg1.elem2 + arg2.elem1.elem2 + arg2.elem2;
	return doubleStruct;
}

/**
 * Get a new struct by adding each double element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two doubles
 * @param arg2 the 2nd struct with a nested double struct
 * @return a struct with a nested double struct
 */
stru_NestedStruct_Double
addDoubleStruct1AndNestedDoubleStruct2_returnStruct2_dupStruct(stru_2_Doubles arg1, stru_NestedStruct_Double arg2)
{
	stru_NestedStruct_Double doubleStruct;
	doubleStruct.elem1.elem1 = arg1.elem1 + arg2.elem1.elem1;
	doubleStruct.elem1.elem2 = arg1.elem2 + arg2.elem1.elem2;
	doubleStruct.elem2 = arg1.elem2 + arg2.elem2;
	return doubleStruct;
}

/**
 * Get a new struct by adding each double element of two structs
 * with the same nested double array.
 *
 * @param arg1 the 1st struct with a nested double array
 * @param arg2 the 2nd struct with a nested double array
 * @return a struct with a nested double array
 */
stru_NestedDoubleArray_Double
addNestedDoubleArrayStructs_dupStruct(stru_NestedDoubleArray_Double arg1, stru_NestedDoubleArray_Double arg2)
{
	stru_NestedDoubleArray_Double doubleStruct;
	doubleStruct.elem1[0] = arg1.elem1[0] + arg2.elem1[0];
	doubleStruct.elem1[1] = arg1.elem1[1] + arg2.elem1[1];
	doubleStruct.elem2 = arg1.elem2 + arg2.elem2;
	return doubleStruct;
}

/**
 * Get a new struct by adding each double element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two doubles
 * @param arg2 the 2nd struct with a nested double array
 * @return a struct with two doubles
 */
stru_2_Doubles
addDoubleStruct1AndNestedDoubleArrayStruct2_returnStruct1_dupStruct(stru_2_Doubles arg1, stru_NestedDoubleArray_Double arg2)
{
	stru_2_Doubles doubleStruct;
	doubleStruct.elem1 = arg1.elem1 + arg2.elem1[0];
	doubleStruct.elem2 = arg1.elem2 + arg2.elem1[1] + arg2.elem2;
	return doubleStruct;
}

/**
 * Get a new struct by adding each double element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two doubles
 * @param arg2 the 2nd struct with a nested double array
 * @return a struct with a nested double array
 */
stru_NestedDoubleArray_Double
addDoubleStruct1AndNestedDoubleArrayStruct2_returnStruct2_dupStruct(stru_2_Doubles arg1, stru_NestedDoubleArray_Double arg2)
{
	stru_NestedDoubleArray_Double doubleStruct;
	doubleStruct.elem1[0] = arg1.elem1 + arg2.elem1[0];
	doubleStruct.elem1[1] = arg1.elem2 + arg2.elem1[1];
	doubleStruct.elem2 = arg1.elem2 + arg2.elem2;
	return doubleStruct;
}

/**
 * Get a new struct by adding each double element of two structs
 * with the same nested double struct array.
 *
 * @param arg1 the 1st struct with a nested double struct array
 * @param arg2 the 2nd struct with a nested double struct array
 * @return a struct with a nested double struct array
 */
stru_NestedStruArray_Double
addNestedDoubleStructArrayStructs_dupStruct(stru_NestedStruArray_Double arg1, stru_NestedStruArray_Double arg2)
{
	stru_NestedStruArray_Double doubleStruct;
	doubleStruct.elem1[0].elem1 = arg1.elem1[0].elem1 + arg2.elem1[0].elem1;
	doubleStruct.elem1[0].elem2 = arg1.elem1[0].elem2 + arg2.elem1[0].elem2;
	doubleStruct.elem1[1].elem1 = arg1.elem1[1].elem1 + arg2.elem1[1].elem1;
	doubleStruct.elem1[1].elem2 = arg1.elem1[1].elem2 + arg2.elem1[1].elem2;
	doubleStruct.elem2 = arg1.elem2 + arg2.elem2;
	return doubleStruct;
}

/**
 * Get a new struct by adding each double element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two doubles
 * @param arg2 the 2nd struct with a nested double struct array
 * @return a struct with two doubles
 */
stru_2_Doubles
addDoubleStruct1AndNestedDoubleStructArrayStruct2_returnStruct1_dupStruct(stru_2_Doubles arg1, stru_NestedStruArray_Double arg2)
{
	stru_2_Doubles doubleStruct;
	doubleStruct.elem1 = arg1.elem1 + arg2.elem1[0].elem1 + arg2.elem1[1].elem1;
	doubleStruct.elem2 = arg1.elem2 + arg2.elem1[0].elem2 + arg2.elem1[1].elem2 + arg2.elem2;
	return doubleStruct;
}

/**
 * Get a new struct by adding each double element of two structs with different elements.
 *
 * @param arg1 the 1st struct with two doubles
 * @param arg2 the 2nd struct with a nested double struct array
 * @return a struct with a nested double struct array
 */
stru_NestedStruArray_Double
addDoubleStruct1AndNestedDoubleStructArrayStruct2_returnStruct2_dupStruct(stru_2_Doubles arg1, stru_NestedStruArray_Double arg2)
{
	stru_NestedStruArray_Double doubleStruct;
	doubleStruct.elem1[0].elem1 = arg1.elem1 + arg2.elem1[0].elem1;
	doubleStruct.elem1[0].elem2 = arg1.elem2 + arg2.elem1[0].elem2;
	doubleStruct.elem1[1].elem1 = arg1.elem1 + arg2.elem1[1].elem1;
	doubleStruct.elem1[1].elem2 = arg1.elem2 + arg2.elem1[1].elem2;
	doubleStruct.elem2 = arg1.elem2 + arg2.elem2;
	return doubleStruct;
}
