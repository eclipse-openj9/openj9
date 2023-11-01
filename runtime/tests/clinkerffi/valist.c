/*******************************************************************************
 * Copyright IBM Corp. and others 2022
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
 * This file contains the native code used by the VaList specific test cases
 * via a Clinker FFI Downcall & Upcall in java, which come from:
 * org.openj9.test.jep389.valist (JDK16/17)
 * org.openj9.test.jep419.valist (JDK18)
 * org.openj9.test.jep424.valist (JDK19+)
 *
 * Created by jincheng@ca.ibm.com
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include "downcall.h"

/**
 * Add ints from the va_list with the specified count
 *
 * @param argCount the count of the ints
 * @param intArgList the integer va_list
 * @return the sum of ints from the va_list
 */
int
addIntsFromVaList(int argCount, va_list intVaList)
{
	int intSum = 0;
	while (argCount > 0) {
		intSum += va_arg(intVaList, int);
		argCount--;
	}
	return intSum;
}

/**
 * Add longs from the va_list with the specified count
 *
 * @param argCount the count of longs
 * @param longArgList the long va_list
 * @return the sum of longs from the va_list
 */
LONG
addLongsFromVaList(int argCount, va_list longVaList)
{
	LONG longSum = 0;
	while (argCount > 0) {
		longSum += va_arg(longVaList, LONG);
		argCount--;
	}
	return longSum;
}

/**
 * Add doubles from the va_list with the specified count
 *
 * @param argCount the count of the doubles
 * @param doubleArgList the double va_list
 * @return the sum of doubles from the va_list
 */
double
addDoublesFromVaList(int argCount, va_list doubleVaList)
{
	double doubleSum = 0;
	while (argCount > 0) {
		doubleSum += va_arg(doubleVaList, double);
		argCount--;
	}
	return doubleSum;
}

/**
 * Add arguments with different types from the va_list
 *
 * @param argVaList the va_list with mixed arguments
 * @return the sum of arguments from the va_list
 */
double
addMixedArgsFromVaList(va_list argVaList)
{
	double argSum = va_arg(argVaList, int) + va_arg(argVaList, long) + va_arg(argVaList, double);
	return argSum;
}

/**
 * Add more arguments with different types from the va_list
 *
 * @param argVaList the va_list with mixed arguments
 * @return the sum of arguments from the va_list
 */
double
addMoreMixedArgsFromVaList(va_list argVaList)
{
	double argSum = va_arg(argVaList, int) + va_arg(argVaList, long) + va_arg(argVaList, int)
		+ va_arg(argVaList, long) + va_arg(argVaList, int) + va_arg(argVaList, long)
		+ va_arg(argVaList, int) + va_arg(argVaList, double) + va_arg(argVaList, int)
		+ va_arg(argVaList, double) + va_arg(argVaList, int) + va_arg(argVaList, double)
		+ va_arg(argVaList, int) + va_arg(argVaList, double) + va_arg(argVaList, int)
		+ va_arg(argVaList, double);
	return argSum;
}

/**
 * Add ints (accessed by pointers) from the va_list
 *
 * @param argCount the count of integer pointers in the va_list
 * @param ptrVaList the passed-in va_list containing the integer pointers
 * @return the sum of ints
 */
int
addIntsByPtrFromVaList(int argCount, va_list ptrVaList)
{
	int intSum = 0;
	while (argCount > 0) {
		intSum += *va_arg(ptrVaList, int *);
		argCount--;
	}
	return intSum;
}

/**
 * Add longs (accessed by pointers) from the va_list
 *
 * @param argCount the count of long pointers in the va_list
 * @param ptrVaList the passed-in va_list containing the long pointers
 * @return the sum of longs
 */
LONG
addLongsByPtrFromVaList(int argCount, va_list ptrVaList)
{
	LONG longSum = 0;
	while (argCount > 0) {
		longSum += *va_arg(ptrVaList, LONG *);
		argCount--;
	}
	return longSum;
}

/**
 * Add doubles (accessed by pointers) from the va_list
 *
 * @param argCount the count of double pointers in the va_list
 * @param ptrVaList the passed-in va_list containing the double pointers
 * @return the sum of doubles
 */
double
addDoublesByPtrFromVaList(int argCount, va_list ptrVaList)
{
	double doubleSum = 0;
	while (argCount > 0) {
		doubleSum += *va_arg(ptrVaList, double *);
		argCount--;
	}
	return doubleSum;
}

/**
 * Add the only one element(byte) of structs from the va_list
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @return the sum
 */
char
add1ByteOfStructsFromVaList(int argCount, va_list struVaList)
{
	char byteSum = 0;
	while (argCount > 0) {
		stru_Byte struArg = va_arg(struVaList, stru_Byte);
		byteSum += struArg.elem1;
		argCount--;
	}
	return byteSum;
}

/**
 * Add bytes of structs with two elements from the va_list
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @return the sum
 */
char
add2BytesOfStructsFromVaList(int argCount, va_list struVaList)
{
	char byteSum = 0;
	while (argCount > 0) {
		stru_2_Bytes struArg = va_arg(struVaList, stru_2_Bytes);
		byteSum += struArg.elem1 + struArg.elem2;
		argCount--;
	}
	return byteSum;
}

/**
 * Add bytes of structs with three elements from the va_list
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @return the sum
 */
char
add3BytesOfStructsFromVaList(int argCount, va_list struVaList)
{
	char byteSum = 0;
	while (argCount > 0) {
		stru_3_Bytes struArg = va_arg(struVaList, stru_3_Bytes);
		byteSum += struArg.elem1 + struArg.elem2 + struArg.elem3;
		argCount--;
	}
	return byteSum;
}

/**
 * Add bytes of structs with five elements from the va_list
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @return the sum
 */
char
add5BytesOfStructsFromVaList(int argCount, va_list struVaList)
{
	char byteSum = 0;
	while (argCount > 0) {
		stru_5_Bytes struArg = va_arg(struVaList, stru_5_Bytes);
		byteSum += struArg.elem1 + struArg.elem2 + struArg.elem3 + struArg.elem4 + struArg.elem5;
		argCount--;
	}
	return byteSum;
}

/**
 * Add bytes of structs with seven elements from the va_list
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @return the sum
 */
char
add7BytesOfStructsFromVaList(int argCount, va_list struVaList)
{
	char byteSum = 0;
	while (argCount > 0) {
		stru_7_Bytes struArg = va_arg(struVaList, stru_7_Bytes);
		byteSum += struArg.elem1 + struArg.elem2 + struArg.elem3
					+ struArg.elem4 + struArg.elem5 + struArg.elem6 + struArg.elem7;
		argCount--;
	}
	return byteSum;
}

/**
 * Add the only one element(short) of structs from the va_list
 *
 * @param argCount the count of structs in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @return the sum
 */
short
add1ShortOfStructsFromVaList(int argCount, va_list struVaList)
{
	short shortSum = 0;
	while (argCount > 0) {
		stru_Short struArg = va_arg(struVaList, stru_Short);
		shortSum += struArg.elem1;
		argCount--;
	}
	return shortSum;
}

/**
 * Add shorts of structs with two elements from the va_list
 *
 * @param argCount the count of structs in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @return the sum
 */
short
add2ShortsOfStructsFromVaList(int argCount, va_list struVaList)
{
	short shortSum = 0;
	while (argCount > 0) {
		stru_2_Shorts struArg = va_arg(struVaList, stru_2_Shorts);
		shortSum += struArg.elem1 + struArg.elem2;
		argCount--;
	}
	return shortSum;
}

/**
 * Add shorts of structs with three elements from the va_list
 *
 * @param argCount the count of structs in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @return the sum
 */
short
add3ShortsOfStructsFromVaList(int argCount, va_list struVaList)
{
	short shortSum = 0;
	while (argCount > 0) {
		stru_3_Shorts struArg = va_arg(struVaList, stru_3_Shorts);
		shortSum += struArg.elem1 + struArg.elem2 + struArg.elem3;
		argCount--;
	}
	return shortSum;
}

/**
 * Add the only one element(int) of structs from the va_list
 *
 * @param argCount the count of structs in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @return the sum
 */
int
add1IntOfStructsFromVaList(int argCount, va_list struVaList)
{
	int intSum = 0;
	while (argCount > 0) {
		stru_Int struArg = va_arg(struVaList, stru_Int);
		intSum += struArg.elem1;
		argCount--;
	}
	return intSum;
}

/**
 * Add ints of structs with two elements from the va_list
 *
 * @param argCount the count of structs in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @return the sum of ints
 */
int
add2IntsOfStructsFromVaList(int argCount, va_list struVaList)
{
	int intSum = 0;
	while (argCount > 0) {
		stru_2_Ints struArg = va_arg(struVaList, stru_2_Ints);
		intSum += struArg.elem1 + struArg.elem2;
		argCount--;
	}
	return intSum;
}

/**
 * Add ints of structs with three elements from the va_list
 *
 * @param argCount the count of structs in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @return the sum of ints
 */
int
add3IntsOfStructsFromVaList(int argCount, va_list struVaList)
{
	int intSum = 0;
	while (argCount > 0) {
		stru_3_Ints struArg = va_arg(struVaList, stru_3_Ints);
		intSum += struArg.elem1 + struArg.elem2 + struArg.elem3;
		argCount--;
	}
	return intSum;
}

/**
 * Add longs of structs with two elements from the va_list
 *
 * @param argCount the count of structs in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @return the sum of longs
 */
LONG
add2LongsOfStructsFromVaList(int argCount, va_list struVaList)
{
	LONG longSum = 0;
	while (argCount > 0) {
		stru_2_Longs struArg = va_arg(struVaList, stru_2_Longs);
		longSum += struArg.elem1 + struArg.elem2;
		argCount--;
	}
	return longSum;
}

/**
 * Add the only one element(float) of structs from the va_list
 *
 * @param argCount the count of structs in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @return the sum
 */
float
add1FloatOfStructsFromVaList(int argCount, va_list struVaList)
{
	float floatSum = 0;
	while (argCount > 0) {
		stru_Float struArg = va_arg(struVaList, stru_Float);
		floatSum += struArg.elem1;
		argCount--;
	}
	return floatSum;
}

/**
 * Add floats of structs with two elements from the va_list
 *
 * @param argCount the count of structs in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @return the sum of floats
 */
float
add2FloatsOfStructsFromVaList(int argCount, va_list struVaList)
{
	float floatSum = 0;
	while (argCount > 0) {
		stru_2_Floats struArg = va_arg(struVaList, stru_2_Floats);
		floatSum += struArg.elem1 + struArg.elem2;
		argCount--;
	}
	return floatSum;
}

/**
 * Add floats of structs with three elements from the va_list
 *
 * @param argCount the count of structs in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @return the sum of floats
 */
float
add3FloatsOfStructsFromVaList(int argCount, va_list struVaList)
{
	float floatSum = 0;
	while (argCount > 0) {
		stru_3_Floats struArg = va_arg(struVaList, stru_3_Floats);
		floatSum += struArg.elem1 + struArg.elem2 + struArg.elem3;
		argCount--;
	}
	return floatSum;
}

/**
 * Add the only one element(double) of structs from the va_list
 *
 * @param argCount the count of structs in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @return the sum
 */
double
add1DoubleOfStructsFromVaList(int argCount, va_list struVaList)
{
	double doubleSum = 0;
	while (argCount > 0) {
		stru_Double struArg = va_arg(struVaList, stru_Double);
		doubleSum += struArg.elem1;
		argCount--;
	}
	return doubleSum;
}

/**
 * Add doubles of structs with two elements from the va_list
 *
 * @param argCount the count of structs in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @return the sum of doubles
 */
double
add2DoublesOfStructsFromVaList(int argCount, va_list struVaList)
{
	double doubleSum = 0;
	while (argCount > 0) {
		stru_2_Doubles struArg = va_arg(struVaList, stru_2_Doubles);
		doubleSum += struArg.elem1 + struArg.elem2;
		argCount--;
	}
	return doubleSum;
}

/**
 * Add the elements(int & short) of structs from the va_list
 *
 * @param argCount the count of structs in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @return the sum
 */
int
addIntShortOfStructsFromVaList(int argCount, va_list struVaList)
{
	int intSum = 0;
	while (argCount > 0) {
		stru_Int_Short struArg = va_arg(struVaList, stru_Int_Short);
		intSum += struArg.elem1 + struArg.elem2;
		argCount--;
	}
	return intSum;
}

/**
 * Add the elements(short & int) of structs from the va_list
 *
 * @param argCount the count of structs in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @return the sum
 */
int
addShortIntOfStructsFromVaList(int argCount, va_list struVaList)
{
	int intSum = 0;
	while (argCount > 0) {
		stru_Short_Int struArg = va_arg(struVaList, stru_Short_Int);
		intSum += struArg.elem1 + struArg.elem2;
		argCount--;
	}
	return intSum;
}

/**
 * Add the elements(int & long) of structs from the va_list
 *
 * @param argCount the count of structs in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @return the sum
 */
LONG
addIntLongOfStructsFromVaList(int argCount, va_list struVaList)
{
	LONG longSum = 0;
	while (argCount > 0) {
		stru_Int_Long struArg = va_arg(struVaList, stru_Int_Long);
		longSum += struArg.elem1 + struArg.elem2;
		argCount--;
	}
	return longSum;
}

/**
 * Add the elements(long & int) of structs from the va_list
 *
 * @param argCount the count of structs in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @return the sum
 */
LONG
addLongIntOfStructsFromVaList(int argCount, va_list struVaList)
{
	LONG longSum = 0;
	while (argCount > 0) {
		stru_Long_Int struArg = va_arg(struVaList, stru_Long_Int);
		longSum += struArg.elem1 + struArg.elem2;
		argCount--;
	}
	return longSum;
}

/**
 * Add the elements(float & double) of structs from the va_list
 *
 * @param argCount the count of structs in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @return the sum
 */
double
addFloatDoubleOfStructsFromVaList(int argCount, va_list struVaList)
{
	double doubleSum = 0;
	while (argCount > 0) {
		stru_Float_Double struArg = va_arg(struVaList, stru_Float_Double);
		doubleSum += struArg.elem1 + struArg.elem2;
		argCount--;
	}
	return doubleSum;
}

/**
 * Add the elements(double & float) of structs from the va_list
 *
 * @param argCount the count of structs in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @return the sum
 */
double
addDoubleFloatOfStructsFromVaList(int argCount, va_list struVaList)
{
	double doubleSum = 0;
	while (argCount > 0) {
		stru_Double_Float struArg = va_arg(struVaList, stru_Double_Float);
		doubleSum += struArg.elem1 + struArg.elem2;
		argCount--;
	}
	return doubleSum;
}

/**
 * Add ints from the va_list with the specified count
 * by invoking an upcall method.
 *
 * @param argCount the count of the ints
 * @param intArgList the int va_list
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
int
addIntsFromVaListByUpcallMH(int argCount, va_list intVaList, int (*upcallMH)(int, va_list))
{
	int arg1 = va_arg(intVaList, int);
	int intSum = arg1 + (*upcallMH)(argCount - 1, intVaList);
	return intSum;
}

/**
 * Add longs from the va_list with the specified count
 * by invoking an upcall method.
 *
 * @param argCount the count of the longs
 * @param longArgList the long va_list
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
LONG
addLongsFromVaListByUpcallMH(int argCount, va_list longVaList, LONG (*upcallMH)(int, va_list))
{
	LONG longSum = (*upcallMH)(argCount, longVaList);
	return longSum;
}

/**
 * Add doubles from the va_list with the specified count
 * by invoking an upcall method.
 *
 * @param argCount the count of the double arguments
 * @param doubleArgList the double va_list
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoublesFromVaListByUpcallMH(int argCount, va_list doubleVaList, double (*upcallMH)(int, va_list))
{
	double arg1 = va_arg(doubleVaList, double);
	double doubleSum = arg1 + (*upcallMH)(argCount - 1, doubleVaList);
	return doubleSum;
}

/**
 * Add arguments with different types from the va_list
 * by invoking an upcall method.
 *
 * @param argVaList the va_list with mixed arguments
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addMixedArgsFromVaListByUpcallMH(va_list argVaList, double (*upcallMH)(va_list))
{
	double argSum = (*upcallMH)(argVaList);
	return argSum;
}

/**
 * Add more arguments with different types from the va_list
 * by invoking an upcall method.
 *
 * @param argVaList the va_list with mixed arguments
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addMoreMixedArgsFromVaListByUpcallMH(va_list argVaList, double (*upcallMH)(va_list))
{
	double argSum = (*upcallMH)(argVaList);
	return argSum;
}

/**
 * Add ints (accessed via MemoryAddress in java) from the va_list
 * with the specified count by invoking an upcall method.
 *
 * @param argCount the count of integer pointers in the va_list
 * @param ptrVaList the passed-in va_list containing the int pointers
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
int
addIntsByPtrFromVaListByUpcallMH(int argCount, va_list ptrVaList, int (*upcallMH)(int, va_list))
{
	int intSum = (*upcallMH)(argCount, ptrVaList);
	return intSum;
}

/**
 * Add longs (accessed via MemoryAddress in java) from the va_list
 * with the specified count by invoking an upcall method.
 *
 * @param argCount the count of integer pointers in the va_list
 * @param ptrVaList the passed-in va_list containing the long pointers
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
LONG
addLongsByPtrFromVaListByUpcallMH(int argCount, va_list ptrVaList, LONG (*upcallMH)(int, va_list))
{
	LONG longSum = (*upcallMH)(argCount, ptrVaList);
	return longSum;
}

/**
 * Add doubles (accessed via MemoryAddress in java) from the va_list
 * with the specified count by invoking an upcall method.
 *
 * @param argCount the count of integer pointers in the va_list
 * @param ptrVaList the passed-in va_list containing the double pointers
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
addDoublesByPtrFromVaListByUpcallMH(int argCount, va_list ptrVaList, double (*upcallMH)(int, va_list))
{
	double doubleSum = (*upcallMH)(argCount, ptrVaList);
	return doubleSum;
}

/**
 * Add the only one element(byte) of structs with the specified count
 * by invoking an upcall method.
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
char
add1ByteOfStructsFromVaListByUpcallMH(int argCount, va_list struVaList, char (*upcallMH)(int, va_list))
{
	char byteSum = (*upcallMH)(argCount, struVaList);
	return byteSum;
}

/**
 * Add bytes of structs with two elements from the va_list
 * with the specified count by invoking an upcall method.
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
char
add2BytesOfStructsFromVaListByUpcallMH(int argCount, va_list struVaList, char (*upcallMH)(int, va_list))
{
	char byteSum = (*upcallMH)(argCount, struVaList);
	return byteSum;
}

/**
 * Add bytes of structs with three elements from the va_list
 * with the specified count by invoking an upcall method.
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
char
add3BytesOfStructsFromVaListByUpcallMH(int argCount, va_list struVaList, char (*upcallMH)(int, va_list))
{
	char byteSum = (*upcallMH)(argCount, struVaList);
	return byteSum;
}

/**
 * Add bytes of structs with five elements from the va_list
 * with the specified count by invoking an upcall method.
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
char
add5BytesOfStructsFromVaListByUpcallMH(int argCount, va_list struVaList, char (*upcallMH)(int, va_list))
{
	char byteSum = (*upcallMH)(argCount, struVaList);
	return byteSum;
}

/**
 * Add bytes of structs with seven elements from the va_list
 * with the specified count by invoking an upcall method.
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
char
add7BytesOfStructsFromVaListByUpcallMH(int argCount, va_list struVaList, char (*upcallMH)(int, va_list))
{
	char byteSum = (*upcallMH)(argCount, struVaList);
	return byteSum;
}

/**
 * Add the only one element(short) of structs with the specified count
 * by invoking an upcall method.
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
short
add1ShortOfStructsFromVaListByUpcallMH(int argCount, va_list struVaList, short (*upcallMH)(int, va_list))
{
	short shortSum = (*upcallMH)(argCount, struVaList);
	return shortSum;
}

/**
 * Add shorts of structs with two elements from the va_list
 * with the specified count by invoking an upcall method.
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
short
add2ShortsOfStructsFromVaListByUpcallMH(int argCount, va_list struVaList, short (*upcallMH)(int, va_list))
{
	short shortSum = (*upcallMH)(argCount, struVaList);
	return shortSum;
}

/**
 * Add shorts of structs with three elements from the va_list
 * with the specified count by invoking an upcall method.
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
short
add3ShortsOfStructsFromVaListByUpcallMH(int argCount, va_list struVaList, short (*upcallMH)(int, va_list))
{
	short shortSum = (*upcallMH)(argCount, struVaList);
	return shortSum;
}

/**
 * Add the only one element(int) of structs with the specified count
 * by invoking an upcall method.
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
int
add1IntOfStructsFromVaListByUpcallMH(int argCount, va_list struVaList, int (*upcallMH)(int, va_list))
{
	int intSum = (*upcallMH)(argCount, struVaList);
	return intSum;
}

/**
 * Add ints of structs with two elements from the va_list
 * with the specified count by invoking an upcall method.
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
int
add2IntsOfStructsFromVaListByUpcallMH(int argCount, va_list struVaList, int (*upcallMH)(int, va_list))
{
	int intSum = (*upcallMH)(argCount, struVaList);
	return intSum;
}

/**
 * Add ints of structs with three elements from the va_list
 * with the specified count by invoking an upcall method.
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
int
add3IntsOfStructsFromVaListByUpcallMH(int argCount, va_list struVaList, int (*upcallMH)(int, va_list))
{
	int intSum = (*upcallMH)(argCount, struVaList);
	return intSum;
}

/**
 * Add longs of structs with two elements from the va_list
 * with the specified count by invoking an upcall method.
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
LONG
add2LongsOfStructsFromVaListByUpcallMH(int argCount, va_list struVaList, LONG (*upcallMH)(int, va_list))
{
	LONG longSum = (*upcallMH)(argCount, struVaList);
	return longSum;
}

/**
 * Add the only one element(float) of structs with the specified count
 * by invoking an upcall method.
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
float
add1FloatOfStructsFromVaListByUpcallMH(int argCount, va_list struVaList, float (*upcallMH)(int, va_list))
{
	float floatSum = (*upcallMH)(argCount, struVaList);
	return floatSum;
}

/**
 * Add floats of structs with two elements from the va_list
 * with the specified count by invoking an upcall method.
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
float
add2FloatsOfStructsFromVaListByUpcallMH(int argCount, va_list struVaList, float (*upcallMH)(int, va_list))
{
	float floatSum = (*upcallMH)(argCount, struVaList);
	return floatSum;
}

/**
 * Add floats of structs with three elements from the va_list
 * with the specified count by invoking an upcall method.
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
float
add3FloatsOfStructsFromVaListByUpcallMH(int argCount, va_list struVaList, float (*upcallMH)(int, va_list))
{
	float floatSum = (*upcallMH)(argCount, struVaList);
	return floatSum;
}

/**
 * Add the only one element(double) of structs with the specified count
 * by invoking an upcall method.
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
add1DoubleOfStructsFromVaListByUpcallMH(int argCount, va_list struVaList, double (*upcallMH)(int, va_list))
{
	double doubleSum = (*upcallMH)(argCount, struVaList);
	return doubleSum;
}

/**
 * Add doubles of structs with two elements from the va_list
 * with the specified count by invoking an upcall method.
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
double
add2DoublesOfStructsFromVaListByUpcallMH(int argCount, va_list struVaList, double (*upcallMH)(int, va_list))
{
	double doubleSum = (*upcallMH)(argCount, struVaList);
	return doubleSum;
}

/**
 * Add the elements(int & short) of structs from the va_list
 * with the specified count by invoking an upcall method.
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
int
addIntShortOfStructsFromVaListByUpcallMH(int argCount, va_list struVaList, int (*upcallMH)(int, va_list))
{
	int intSum = (*upcallMH)(argCount, struVaList);
	return intSum;
}

/**
 * Add the elements(short & int) of structs from the va_list
 * with the specified count by invoking an upcall method.
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
int
addShortIntOfStructsFromVaListByUpcallMH(int argCount, va_list struVaList, int (*upcallMH)(int, va_list))
{
	int intSum = (*upcallMH)(argCount, struVaList);
	return intSum;
}

/**
 * Add the elements(int & long) of structs from the va_list
 * with the specified count by invoking an upcall method.
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
LONG
addIntLongOfStructsFromVaListByUpcallMH(int argCount, va_list struVaList, LONG (*upcallMH)(int, va_list))
{
	LONG longSum = (*upcallMH)(argCount, struVaList);
	return longSum;
}

/**
 * Add the elements(long & int) of structs from the va_list
 * with the specified count by invoking an upcall method.
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @param upcallMH the function pointer to the upcall method
 * @return the sum
 */
LONG
addLongIntOfStructsFromVaListByUpcallMH(int argCount, va_list struVaList, LONG (*upcallMH)(int, va_list))
{
	LONG longSum = (*upcallMH)(argCount, struVaList);
	return longSum;
}

/**
 * Add the elements(float & double) of structs from the va_list
 * with the specified count by invoking an upcall method.
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @param upcallMH the function pofloater to the upcall method
 * @return the sum
 */
double
addFloatDoubleOfStructsFromVaListByUpcallMH(int argCount, va_list struVaList, double (*upcallMH)(int, va_list))
{
	double doubleSum = (*upcallMH)(argCount, struVaList);
	return doubleSum;
}

/**
 * Add the elements(double & float) of structs from the va_list
 * with the specified count by invoking an upcall method.
 *
 * @param argCount the count of struct in the va_list
 * @param struVaList the passed-in va_list containing structs
 * @param upcallMH the function pofloater to the upcall method
 * @return the sum
 */
double
addDoubleFloatOfStructsFromVaListByUpcallMH(int argCount, va_list struVaList, double (*upcallMH)(int, va_list))
{
	double doubleSum = (*upcallMH)(argCount, struVaList);
	return doubleSum;
}
