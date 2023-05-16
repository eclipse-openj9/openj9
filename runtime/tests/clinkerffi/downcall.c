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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

/**
 * This file contains the native code used by the test cases via a Clinker FFI DownCall in java,
 * which come from:
 * org.openj9.test.jep389.downcall (JDK16/17)
 * org.openj9.test.jep419.downcall (JDK18)
 * org.openj9.test.jep424.downcall (JDK19+)
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
addBoolAndBoolsFromStructWithXor(bool arg1, stru_Bool_Bool arg2)
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
addBoolFromPointerAndBoolsFromStructWithXor(bool *arg1, stru_Bool_Bool arg2)
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
addBoolFromPointerAndBoolsFromStructWithXor_returnBoolPointer(bool *arg1, stru_Bool_Bool arg2)
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
addBoolAndBoolsFromStructPointerWithXor(bool arg1, stru_Bool_Bool *arg2)
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
stru_Bool_Bool
add2BoolStructsWithXor_returnStruct(stru_Bool_Bool arg1, stru_Bool_Bool arg2)
{
	stru_Bool_Bool boolStruct;
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
stru_Bool_Bool *
add2BoolStructsWithXor_returnStructPointer(stru_Bool_Bool *arg1, stru_Bool_Bool arg2)
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
stru_Bool_Bool_Bool
add3BoolStructsWithXor_returnStruct(stru_Bool_Bool_Bool arg1, stru_Bool_Bool_Bool arg2)
{
	stru_Bool_Bool_Bool boolStruct;
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
addByteAndBytesFromStruct(char arg1, stru_Byte_Byte arg2)
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
addByteFromPointerAndBytesFromStruct(char *arg1, stru_Byte_Byte arg2)
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
addByteFromPointerAndBytesFromStruct_returnBytePointer(char *arg1, stru_Byte_Byte arg2)
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
addByteAndBytesFromStructPointer(char arg1, stru_Byte_Byte *arg2)
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
stru_Byte_Byte
add2ByteStructs_returnStruct(stru_Byte_Byte arg1, stru_Byte_Byte arg2)
{
	stru_Byte_Byte byteStruct;
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
stru_Byte_Byte *
add2ByteStructs_returnStructPointer(stru_Byte_Byte *arg1, stru_Byte_Byte arg2)
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
stru_Byte_Byte_Byte
add3ByteStructs_returnStruct(stru_Byte_Byte_Byte arg1, stru_Byte_Byte_Byte arg2)
{
	stru_Byte_Byte_Byte byteStruct;
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
addCharAndCharsFromStruct(short arg1, stru_Char_Char arg2)
{
	short result = arg1 + arg2.elem1 + arg2.elem2 - 2 * 'A';
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
addCharFromPointerAndCharsFromStruct(short *arg1, stru_Char_Char arg2)
{
	short result = *arg1 + arg2.elem1 + arg2.elem2 - 2 * 'A';
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
addCharFromPointerAndCharsFromStruct_returnCharPointer(short *arg1, stru_Char_Char arg2)
{
	*arg1 = *arg1 + arg2.elem1 + arg2.elem2 - 2 * 'A';
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
addCharAndCharsFromStructPointer(short arg1, stru_Char_Char *arg2)
{
	short result = arg1 + arg2->elem1 + arg2->elem2 - 2 * 'A';
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
	short result = arg1 + arg2.elem2 + arg2.elem1.elem1 + arg2.elem1.elem2 - 3 * 'A';
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
	short result = arg1 + arg2.elem1 + arg2.elem2.elem1 + arg2.elem2.elem2 - 3 * 'A';
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
	short result = arg1 + arg2.elem1[0] + arg2.elem1[1] + arg2.elem2 - 3 * 'A';
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
	short result = arg1 + arg2.elem2[0] + arg2.elem2[1] + arg2.elem1 - 3 * 'A';
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
			+ arg2.elem1[1].elem1 + arg2.elem1[1].elem2 - 5 * 'A';
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
			+ arg2.elem2[1].elem1 + arg2.elem2[1].elem2 - 5 * 'A';
	return result;
}

/**
 * Create a new struct by adding each char element of two structs.
 *
 * @param arg1 the 1st struct with two chars
 * @param arg2 the 2nd struct with two chars
 * @return a new struct of with two chars
 */
stru_Char_Char
add2CharStructs_returnStruct(stru_Char_Char arg1, stru_Char_Char arg2)
{
	stru_Char_Char charStruct;
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
stru_Char_Char *
add2CharStructs_returnStructPointer(stru_Char_Char *arg1, stru_Char_Char arg2)
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
stru_Char_Char_Char
add3CharStructs_returnStruct(stru_Char_Char_Char arg1, stru_Char_Char_Char arg2)
{
	stru_Char_Char_Char charStruct;
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
addShortAndShortsFromStruct(short arg1, stru_Short_Short arg2)
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
addShortFromPointerAndShortsFromStruct(short *arg1, stru_Short_Short arg2)
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
addShortFromPointerAndShortsFromStruct_returnShortPointer(short *arg1, stru_Short_Short arg2)
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
addShortAndShortsFromStructPointer(short arg1, stru_Short_Short *arg2)
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
stru_Short_Short
add2ShortStructs_returnStruct(stru_Short_Short arg1, stru_Short_Short arg2)
{
	stru_Short_Short shortStruct;
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
stru_Short_Short *
add2ShortStructs_returnStructPointer(stru_Short_Short *arg1, stru_Short_Short arg2)
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
stru_Short_Short_Short
add3ShortStructs_returnStruct(stru_Short_Short_Short arg1, stru_Short_Short_Short arg2)
{
	stru_Short_Short_Short shortStruct;
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
addIntAndIntsFromStruct(int arg1, stru_Int_Int arg2)
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
addIntFromPointerAndIntsFromStruct(int *arg1, stru_Int_Int arg2)
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
addIntFromPointerAndIntsFromStruct_returnIntPointer(int *arg1, stru_Int_Int arg2)
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
addIntAndIntsFromStructPointer(int arg1, stru_Int_Int *arg2)
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
stru_Int_Int
add2IntStructs_returnStruct(stru_Int_Int arg1, stru_Int_Int arg2)
{
	stru_Int_Int intStruct;
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
stru_Int_Int *
add2IntStructs_returnStructPointer(stru_Int_Int *arg1, stru_Int_Int arg2)
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
stru_Int_Int_Int
add3IntStructs_returnStruct(stru_Int_Int_Int arg1, stru_Int_Int_Int arg2)
{
	stru_Int_Int_Int intStruct;
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
addLongAndLongsFromStruct(LONG arg1, stru_Long_Long arg2)
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
addLongFromPointerAndLongsFromStruct(LONG *arg1, stru_Long_Long arg2)
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
addLongFromPointerAndLongsFromStruct_returnLongPointer(LONG *arg1, stru_Long_Long arg2)
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
addLongAndLongsFromStructPointer(LONG arg1, stru_Long_Long *arg2)
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
stru_Long_Long
add2LongStructs_returnStruct(stru_Long_Long arg1, stru_Long_Long arg2)
{
	stru_Long_Long longStruct;
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
stru_Long_Long *
add2LongStructs_returnStructPointer(stru_Long_Long *arg1, stru_Long_Long arg2)
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
stru_Long_Long_Long
add3LongStructs_returnStruct(stru_Long_Long_Long arg1, stru_Long_Long_Long arg2)
{
	stru_Long_Long_Long longStruct;
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
addFloatAndFloatsFromStruct(float arg1, stru_Float_Float arg2)
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
addFloatFromPointerAndFloatsFromStruct(float *arg1, stru_Float_Float arg2)
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
addFloatFromPointerAndFloatsFromStruct_returnFloatPointer(float *arg1, stru_Float_Float arg2)
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
addFloatAndFloatsFromStructPointer(float arg1, stru_Float_Float *arg2)
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
stru_Float_Float
add2FloatStructs_returnStruct(stru_Float_Float arg1, stru_Float_Float arg2)
{
	stru_Float_Float floatStruct;
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
stru_Float_Float *
add2FloatStructs_returnStructPointer(stru_Float_Float *arg1, stru_Float_Float arg2)
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
stru_Float_Float_Float
add3FloatStructs_returnStruct(stru_Float_Float_Float arg1, stru_Float_Float_Float arg2)
{
	stru_Float_Float_Float floatStruct;
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
addDoubleAndDoublesFromStruct(double arg1, stru_Double_Double arg2)
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
addDoubleFromPointerAndDoublesFromStruct(double *arg1, stru_Double_Double arg2)
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
addDoubleFromPointerAndDoublesFromStruct_returnDoublePointer(double *arg1, stru_Double_Double arg2)
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
addDoubleAndDoublesFromStructPointer(double arg1, stru_Double_Double *arg2)
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
stru_Double_Double
add2DoubleStructs_returnStruct(stru_Double_Double arg1, stru_Double_Double arg2)
{
	stru_Double_Double doubleStruct;
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
stru_Double_Double *
add2DoubleStructs_returnStructPointer(stru_Double_Double *arg1, stru_Double_Double arg2)
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
stru_Double_Double_Double
add3DoubleStructs_returnStruct(stru_Double_Double_Double arg1, stru_Double_Double_Double arg2)
{
	stru_Double_Double_Double doubleStruct;
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
validateNullAddrArgument(int arg1, stru_Int_Int *arg2)
{
	return arg1;
}
