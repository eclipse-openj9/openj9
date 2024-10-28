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
 * This file contains the structs/unions required in the native code via a Clinker FFI DownCall & Upcall in java,
 * which come from:
 * org.openj9.test.jep389.downcall/upcall (JDK16/17)
 * org.openj9.test.jep419.downcall/upcall (JDK18)
 * org.openj9.test.jep424.downcall/upcall (JDK19)
 * org.openj9.test.jep434.downcall/upcall (JDK20)
 * org.openj9.test.jep442.downcall/upcall (JDK21+)
 *
 * Created by jincheng@ca.ibm.com
 */

#ifndef DOWNCALL_H
#define DOWNCALL_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

typedef int64_t LONG;

typedef struct stru_2_Bools {
	bool elem1;
	bool elem2;
} stru_2_Bools;

typedef struct stru_3_Bools {
	bool elem1;
	bool elem2;
	bool elem3;
} stru_3_Bools;

typedef struct stru_4_Bools {
	bool elem1;
	bool elem2;
	bool elem3;
	bool elem4;
} stru_4_Bools;

typedef struct stru_20_Bools {
	bool elem1;
	bool elem2;
	bool elem3;
	bool elem4;
	bool elem5;
	bool elem6;
	bool elem7;
	bool elem8;
	bool elem9;
	bool elem10;
	bool elem11;
	bool elem12;
	bool elem13;
	bool elem14;
	bool elem15;
	bool elem16;
	bool elem17;
	bool elem18;
	bool elem19;
	bool elem20;
} stru_20_Bools;

typedef struct stru_NestedStruct_Bool {
	stru_2_Bools elem1;
	bool elem2;
} stru_NestedStruct_Bool;

typedef struct stru_Bool_NestedStruct {
	bool elem1;
	stru_2_Bools elem2;
} stru_Bool_NestedStruct;

typedef struct stru_NestedBoolArray_Bool {
	bool elem1[2];
	bool elem2;
} stru_NestedBoolArray_Bool;

typedef struct stru_Bool_NestedBoolArray {
	bool elem1;
	bool elem2[2];
} stru_Bool_NestedBoolArray;

typedef struct stru_NestedStruArray_Bool {
	stru_2_Bools elem1[2];
	bool elem2;
} stru_NestedStruArray_Bool;

typedef struct stru_Bool_NestedStruArray {
	bool elem1;
	stru_2_Bools elem2[2];
} stru_Bool_NestedStruArray;

typedef struct stru_Byte {
	char elem1;
} stru_Byte;

typedef struct stru_2_Bytes {
	char elem1;
	char elem2;
} stru_2_Bytes;

typedef struct stru_4_Bytes {
	char elem1;
	char elem2;
	char elem3;
	char elem4;
} stru_4_Bytes;

typedef struct stru_3_Bytes {
	char elem1;
	char elem2;
	char elem3;
} stru_3_Bytes;

typedef struct stru_5_Bytes {
	char elem1;
	char elem2;
	char elem3;
	char elem4;
	char elem5;
} stru_5_Bytes;

typedef struct stru_7_Bytes {
	char elem1;
	char elem2;
	char elem3;
	char elem4;
	char elem5;
	char elem6;
	char elem7;
} stru_7_Bytes;

typedef struct stru_20_Bytes {
	char elem1;
	char elem2;
	char elem3;
	char elem4;
	char elem5;
	char elem6;
	char elem7;
	char elem8;
	char elem9;
	char elem10;
	char elem11;
	char elem12;
	char elem13;
	char elem14;
	char elem15;
	char elem16;
	char elem17;
	char elem18;
	char elem19;
	char elem20;
} stru_20_Bytes;

typedef struct stru_254_Bytes {
	char elem[254];
} stru_254_Bytes;

typedef struct stru_4K_Bytes {
	char elem[4096];
} stru_4K_Bytes;

typedef struct stru_NestedStruct_Byte {
	stru_2_Bytes elem1;
	char elem2;
} stru_NestedStruct_Byte;

typedef struct stru_Byte_NestedStruct {
	char elem1;
	stru_2_Bytes elem2;
} stru_Byte_NestedStruct;

typedef struct stru_NestedByteArray_Byte {
	char elem1[2];
	char elem2;
} stru_NestedByteArray_Byte;

typedef struct stru_Byte_NestedByteArray {
	char elem1;
	char elem2[2];
} stru_Byte_NestedByteArray;

typedef struct stru_NestedStruArray_Byte {
	stru_2_Bytes elem1[2];
	char elem2;
} stru_NestedStruArray_Byte;

typedef struct stru_Byte_NestedStruArray {
	char elem1;
	stru_2_Bytes elem2[2];
} stru_Byte_NestedStruArray;

typedef struct stru_2_Chars {
	short elem1;
	short elem2;
} stru_2_Chars;

typedef struct stru_3_Chars {
	short elem1;
	short elem2;
	short elem3;
} stru_3_Chars;

typedef struct stru_4_Chars {
	short elem1;
	short elem2;
	short elem3;
	short elem4;
} stru_4_Chars;

typedef struct stru_10_Chars {
	short elem1;
	short elem2;
	short elem3;
	short elem4;
	short elem5;
	short elem6;
	short elem7;
	short elem8;
	short elem9;
	short elem10;
} stru_10_Chars;

typedef struct stru_NestedStruct_Char {
	stru_2_Chars elem1;
	short elem2;
} stru_NestedStruct_Char;

typedef struct stru_Char_NestedStruct {
	short elem1;
	stru_2_Chars elem2;
} stru_Char_NestedStruct;

typedef struct stru_NestedCharArray_Char {
	short elem1[2];
	short elem2;
} stru_NestedCharArray_Char;

typedef struct stru_Char_NestedCharArray {
	short elem1;
	short elem2[2];
} stru_Char_NestedCharArray;

typedef struct stru_NestedStruArray_Char {
	stru_2_Chars elem1[2];
	short elem2;
} stru_NestedStruArray_Char;

typedef struct stru_Char_NestedStruArray {
	short elem1;
	stru_2_Chars elem2[2];
} stru_Char_NestedStruArray;

typedef struct stru_Short {
	short elem1;
} stru_Short;

typedef struct stru_2_Shorts {
	short elem1;
	short elem2;
} stru_2_Shorts;

typedef struct stru_3_Shorts {
	short elem1;
	short elem2;
	short elem3;
} stru_3_Shorts;

typedef struct stru_4_Shorts {
	short elem1;
	short elem2;
	short elem3;
	short elem4;
} stru_4_Shorts;

typedef struct stru_10_Shorts {
	short elem1;
	short elem2;
	short elem3;
	short elem4;
	short elem5;
	short elem6;
	short elem7;
	short elem8;
	short elem9;
	short elem10;
} stru_10_Shorts;

typedef struct stru_NestedStruct_Short {
	stru_2_Shorts elem1;
	short elem2;
} stru_NestedStruct_Short;

typedef struct stru_Short_NestedStruct {
	short elem1;
	stru_2_Shorts elem2;
} stru_Short_NestedStruct;

typedef struct stru_NestedShortArray_Short {
	short elem1[2];
	short elem2;
} stru_NestedShortArray_Short;

typedef struct stru_Short_NestedShortArray {
	short elem1;
	short elem2[2];
} stru_Short_NestedShortArray;

typedef struct stru_NestedStruArray_Short {
	stru_2_Shorts elem1[2];
	short elem2;
} stru_NestedStruArray_Short;

typedef struct stru_Short_NestedStruArray {
	short elem1;
	stru_2_Shorts elem2[2];
} stru_Short_NestedStruArray;

typedef struct stru_Int {
	int elem1;
} stru_Int;

typedef struct stru_2_Ints {
	int elem1;
	int elem2;
} stru_2_Ints;

typedef struct stru_3_Ints {
	int elem1;
	int elem2;
	int elem3;
} stru_3_Ints;

typedef struct stru_4_Ints {
	int elem1;
	int elem2;
	int elem3;
	int elem4;
} stru_4_Ints;

typedef struct stru_5_Ints {
	int elem1;
	int elem2;
	int elem3;
	int elem4;
	int elem5;
} stru_5_Ints;

typedef struct stru_NestedStruct_Int {
	stru_2_Ints elem1;
	int elem2;
} stru_NestedStruct_Int;

typedef struct stru_Int_NestedStruct {
	int elem1;
	stru_2_Ints elem2;
} stru_Int_NestedStruct;

typedef struct stru_NestedIntArray_Int {
	int elem1[2];
	int elem2;
} stru_NestedIntArray_Int;

typedef struct stru_Int_NestedIntArray {
	int elem1;
	int elem2[2];
} stru_Int_NestedIntArray;

typedef struct stru_NestedStruArray_Int {
	stru_2_Ints elem1[2];
	int elem2;
} stru_NestedStruArray_Int;

typedef struct stru_Int_NestedStruArray {
	int elem1;
	stru_2_Ints elem2[2];
} stru_Int_NestedStruArray;

typedef struct stru_2_Longs {
	LONG elem1;
	LONG elem2;
} stru_2_Longs;

typedef struct stru_4_Longs {
	LONG elem1;
	LONG elem2;
	LONG elem3;
	LONG elem4;
} stru_4_Longs;

typedef struct stru_3_Longs {
	LONG elem1;
	LONG elem2;
	LONG elem3;
} stru_3_Longs;

typedef struct stru_NestedStruct_Long {
	stru_2_Longs elem1;
	LONG elem2;
} stru_NestedStruct_Long;

typedef struct stru_Long_NestedStruct {
	LONG elem1;
	stru_2_Longs elem2;
} stru_Long_NestedStruct;

typedef struct stru_NestedLongArray_Long {
	LONG elem1[2];
	LONG elem2;
} stru_NestedLongArray_Long;

typedef struct stru_Long_NestedLongArray {
	LONG elem1;
	LONG elem2[2];
} stru_Long_NestedLongArray;

typedef struct stru_NestedStruArray_Long {
	stru_2_Longs elem1[2];
	LONG elem2;
} stru_NestedStruArray_Long;

typedef struct stru_Long_NestedStruArray {
	LONG elem1;
	stru_2_Longs elem2[2];
} stru_Long_NestedStruArray;

typedef struct stru_Float {
	float elem1;
} stru_Float;

typedef struct stru_2_Floats {
	float elem1;
	float elem2;
} stru_2_Floats;

typedef struct stru_3_Floats {
	float elem1;
	float elem2;
	float elem3;
} stru_3_Floats;

typedef struct stru_4_Floats {
	float elem1;
	float elem2;
	float elem3;
	float elem4;
} stru_4_Floats;

typedef struct stru_5_Floats {
	float elem1;
	float elem2;
	float elem3;
	float elem4;
	float elem5;
} stru_5_Floats;

typedef struct stru_NestedStruct_Float {
	stru_2_Floats elem1;
	float elem2;
} stru_NestedStruct_Float;

typedef struct stru_Float_NestedStruct {
	float elem1;
	stru_2_Floats elem2;
} stru_Float_NestedStruct;

typedef struct stru_NestedFloatArray_Float {
	float elem1[2];
	float elem2;
} stru_NestedFloatArray_Float;

typedef struct stru_Float_NestedFloatArray {
	float elem1;
	float elem2[2];
} stru_Float_NestedFloatArray;

typedef struct stru_NestedStruArray_Float {
	stru_2_Floats elem1[2];
	float elem2;
} stru_NestedStruArray_Float;

typedef struct stru_Float_NestedStruArray {
	float elem1;
	stru_2_Floats elem2[2];
} stru_Float_NestedStruArray;

typedef struct stru_Double {
	double elem1;
} stru_Double;

typedef struct stru_2_Doubles {
	double elem1;
	double elem2;
} stru_2_Doubles;

typedef struct stru_3_Doubles {
	double elem1;
	double elem2;
	double elem3;
} stru_3_Doubles;

typedef struct stru_4_Doubles {
	double elem1;
	double elem2;
	double elem3;
	double elem4;
} stru_4_Doubles;

typedef struct stru_NestedStruct_Double {
	stru_2_Doubles elem1;
	double elem2;
} stru_NestedStruct_Double;

typedef struct stru_Double_NestedStruct {
	double elem1;
	stru_2_Doubles elem2;
} stru_Double_NestedStruct;

typedef struct stru_NestedDoubleArray_Double {
	double elem1[2];
	double elem2;
} stru_NestedDoubleArray_Double;

typedef struct stru_Double_NestedDoubleArray {
	double elem1;
	double elem2[2];
} stru_Double_NestedDoubleArray;

typedef struct stru_NestedStruArray_Double {
	stru_2_Doubles elem1[2];
	double elem2;
} stru_NestedStruArray_Double;

typedef struct stru_Double_NestedStruArray {
	double elem1;
	stru_2_Doubles elem2[2];
} stru_Double_NestedStruArray;

typedef struct stru_Int_Short {
	int elem1;
	short elem2;
} stru_Int_Short;

typedef struct stru_Short_Int {
	short elem1;
	int elem2;
} stru_Short_Int;

typedef struct stru_Int_Long {
	int elem1;
	LONG elem2;
} stru_Int_Long;

typedef struct stru_Long_Int {
	LONG elem1;
	int elem2;
} stru_Long_Int;

typedef struct stru_Int_Double {
	int elem1;
	double elem2;
} stru_Int_Double;

typedef struct stru_Double_Int {
	double elem1;
	int elem2;
} stru_Double_Int;

typedef struct stru_Float_Double {
	float elem1;
	double elem2;
} stru_Float_Double;

typedef struct stru_Double_Float {
	double elem1;
	float elem2;
} stru_Double_Float;

typedef struct stru_2_Floats_Double {
	float elem1;
	float elem2;
	double elem3;
} stru_2_Floats_Double;

typedef struct stru_Double_Float_Float {
	double elem1;
	float elem2;
	float elem3;
} stru_Double_Float_Float;

typedef struct stru_Int_Float_Float {
	int elem1;
	float elem2;
	float elem3;
} stru_Int_Float_Float;

typedef struct stru_Float_Int_Float {
	float elem1;
	int elem2;
	float elem3;
} stru_Float_Int_Float;

typedef struct stru_Int_Float_Double {
	int elem1;
	float elem2;
	double elem3;
} stru_Int_Float_Double;

typedef struct stru_Float_Int_Double {
	float elem1;
	int elem2;
	double elem3;
} stru_Float_Int_Double;

typedef struct stru_Long_Double {
	LONG elem1;
	double elem2;
} stru_Long_Double;

typedef struct stru_Int_3_Floats {
	int elem1;
	float elem2;
	float elem3;
	float elem4;
} stru_Int_3_Floats;

typedef struct stru_Long_2_Floats {
	LONG elem1;
	float elem2;
	float elem3;
} stru_Long_2_Floats;

typedef struct stru_3_Floats_Int {
	float elem1;
	float elem2;
	float elem3;
	int elem4;
} stru_3_Floats_Int;

typedef struct stru_Float_Long {
	float elem1;
	LONG elem2;
} stru_Float_Long;

typedef struct stru_Double_Float_Int {
	double elem1;
	float elem2;
	int elem3;
} stru_Double_Float_Int;

typedef struct stru_Double_Long {
	double elem1;
	LONG elem2;
} stru_Double_Long;

typedef struct stru_2_Floats_Long {
	float elem1;
	float elem2;
	LONG elem3;
} stru_2_Floats_Long;

typedef struct stru_3_Shorts_Char {
	short elem1[3];
	short elem2; /* intended for the char type in java */
} stru_3_Shorts_Char;

typedef struct stru_Int_Float_Int_Float {
	int elem1;
	float elem2;
	int elem3;
	float elem4;
} stru_Int_Float_Int_Float;

typedef struct stru_Int_Double_Float {
	int elem1;
	double elem2;
	float elem3;
} stru_Int_Double_Float;

typedef struct stru_Float_Double_Int {
	float elem1;
	double elem2;
	int elem3;
} stru_Float_Double_Int;

typedef struct stru_Int_Double_Int {
	int elem1;
	double elem2;
	int elem3;
} stru_Int_Double_Int;

typedef struct stru_Float_Double_Float {
	float elem1;
	double elem2;
	float elem3;
} stru_Float_Double_Float;

typedef struct stru_Int_Double_Long {
	int elem1;
	double elem2;
	LONG elem3;
} stru_Int_Double_Long;

typedef union union_2_Bools {
	bool elem1;
	bool elem2;
} union_2_Bools;

typedef union union_3_Bools {
	bool elem1;
	bool elem2;
	bool elem3;
} union_3_Bools;

typedef union union_NestedUnion_Bool {
	union_2_Bools elem1;
	bool elem2;
} union_NestedUnion_Bool;

typedef union union_Bool_NestedUnion {
	bool elem1;
	union_2_Bools elem2;
} union_Bool_NestedUnion;

typedef union union_NestedBoolArray_Bool {
	bool elem1[2];
	bool elem2;
} union_NestedBoolArray_Bool;

typedef union union_Bool_NestedBoolArray {
	bool elem1;
	bool elem2[2];
} union_Bool_NestedBoolArray;

typedef union union_NestedUnionArray_Bool {
	union_2_Bools elem1[2];
	bool elem2;
} union_NestedUnionArray_Bool;

typedef union union_Bool_NestedUnionArray {
	bool elem1;
	union_2_Bools elem2[2];
} union_Bool_NestedUnionArray;

typedef union union_Byte {
	char elem1;
} union_Byte;

typedef union union_2_Bytes {
	char elem1;
	char elem2;
} union_2_Bytes;

typedef union union_3_Bytes {
	char elem1;
	char elem2;
	char elem3;
} union_3_Bytes;

typedef union union_NestedUnion_Byte {
	union_2_Bytes elem1;
	char elem2;
} union_NestedUnion_Byte;

typedef union union_Byte_NestedUnion {
	char elem1;
	union_2_Bytes elem2;
} union_Byte_NestedUnion;

typedef union union_NestedByteArray_Byte {
	char elem1[2];
	char elem2;
} union_NestedByteArray_Byte;

typedef union union_Byte_NestedByteArray {
	char elem1;
	char elem2[2];
} union_Byte_NestedByteArray;

typedef union union_NestedUnionArray_Byte {
	union_2_Bytes elem1[2];
	char elem2;
} union_NestedUnionArray_Byte;

typedef union union_Byte_NestedUnionArray {
	char elem1;
	union_2_Bytes elem2[2];
} union_Byte_NestedUnionArray;

typedef union union_2_Chars {
	short elem1;
	short elem2;
} union_2_Chars;

typedef union union_3_Chars {
	short elem1;
	short elem2;
	short elem3;
} union_3_Chars;

typedef union union_NestedUnion_Char {
	union_2_Chars elem1;
	short elem2;
} union_NestedUnion_Char;

typedef union union_Char_NestedUnion {
	short elem1;
	union_2_Chars elem2;
} union_Char_NestedUnion;

typedef union union_NestedCharArray_Char {
	short elem1[2];
	short elem2;
} union_NestedCharArray_Char;

typedef union union_Char_NestedCharArray {
	short elem1;
	short elem2[2];
} union_Char_NestedCharArray;

typedef union union_NestedUnionArray_Char {
	union_2_Chars elem1[2];
	short elem2;
} union_NestedUnionArray_Char;

typedef union union_Char_NestedUnionArray {
	short elem1;
	union_2_Chars elem2[2];
} union_Char_NestedUnionArray;

typedef union union_Short {
	short elem1;
} union_Short;

typedef union union_2_Shorts {
	short elem1;
	short elem2;
} union_2_Shorts;

typedef union union_3_Shorts {
	short elem1;
	short elem2;
	short elem3;
} union_3_Shorts;

typedef union union_NestedUnion_Short {
	union_2_Shorts elem1;
	short elem2;
} union_NestedUnion_Short;

typedef union union_Short_NestedUnion {
	short elem1;
	union_2_Shorts elem2;
} union_Short_NestedUnion;

typedef union union_NestedShortArray_Short {
	short elem1[2];
	short elem2;
} union_NestedShortArray_Short;

typedef union union_Short_NestedShortArray {
	short elem1;
	short elem2[2];
} union_Short_NestedShortArray;

typedef union union_NestedUnionArray_Short {
	union_2_Shorts elem1[2];
	short elem2;
} union_NestedUnionArray_Short;

typedef union union_Short_NestedUnionArray {
	short elem1;
	union_2_Shorts elem2[2];
} union_Short_NestedUnionArray;

typedef union union_Int {
	int elem1;
} union_Int;

typedef union union_2_Ints {
	int elem1;
	int elem2;
} union_2_Ints;

typedef union union_3_Ints {
	int elem1;
	int elem2;
	int elem3;
} union_3_Ints;

typedef union union_NestedUnion_Int {
	union_2_Ints elem1;
	int elem2;
} union_NestedUnion_Int;

typedef union union_Int_NestedUnion {
	int elem1;
	union_2_Ints elem2;
} union_Int_NestedUnion;

typedef union union_NestedIntArray_Int {
	int elem1[2];
	int elem2;
} union_NestedIntArray_Int;

typedef union union_Int_NestedIntArray {
	int elem1;
	int elem2[2];
} union_Int_NestedIntArray;

typedef union union_NestedUnionArray_Int {
	union_2_Ints elem1[2];
	int elem2;
} union_NestedUnionArray_Int;

typedef union union_Int_NestedUnionArray {
	int elem1;
	union_2_Ints elem2[2];
} union_Int_NestedUnionArray;

typedef union union_2_Longs {
	LONG elem1;
	LONG elem2;
} union_2_Longs;

typedef union union_3_Longs {
	LONG elem1;
	LONG elem2;
	LONG elem3;
} union_3_Longs;

typedef union union_NestedUnion_Long {
	union_2_Longs elem1;
	LONG elem2;
} union_NestedUnion_Long;

typedef union union_Long_NestedUnion {
	LONG elem1;
	union_2_Longs elem2;
} union_Long_NestedUnion;

typedef union union_NestedLongArray_Long {
	LONG elem1[2];
	LONG elem2;
} union_NestedLongArray_Long;

typedef union union_Long_NestedLongArray {
	LONG elem1;
	LONG elem2[2];
} union_Long_NestedLongArray;

typedef union union_NestedUnionArray_Long {
	union_2_Longs elem1[2];
	LONG elem2;
} union_NestedUnionArray_Long;

typedef union union_Long_NestedUnionArray {
	LONG elem1;
	union_2_Longs elem2[2];
} union_Long_NestedUnionArray;

typedef union union_Float {
	float elem1;
} union_Float;

typedef union union_2_Floats {
	float elem1;
	float elem2;
} union_2_Floats;

typedef union union_3_Floats {
	float elem1;
	float elem2;
	float elem3;
} union_3_Floats;

typedef union union_NestedUnion_Float {
	union_2_Floats elem1;
	float elem2;
} union_NestedUnion_Float;

typedef union union_Float_NestedUnion {
	float elem1;
	union_2_Floats elem2;
} union_Float_NestedUnion;

typedef union union_NestedFloatArray_Float {
	float elem1[2];
	float elem2;
} union_NestedFloatArray_Float;

typedef union union_Float_NestedFloatArray {
	float elem1;
	float elem2[2];
} union_Float_NestedFloatArray;

typedef union union_NestedUnionArray_Float {
	union_2_Floats elem1[2];
	float elem2;
} union_NestedUnionArray_Float;

typedef union union_Float_NestedUnionArray {
	float elem1;
	union_2_Floats elem2[2];
} union_Float_NestedUnionArray;

typedef union union_Double {
	double elem1;
} union_Double;

typedef union union_2_Doubles {
	double elem1;
	double elem2;
} union_2_Doubles;

typedef union union_3_Doubles {
	double elem1;
	double elem2;
	double elem3;
} union_3_Doubles;

typedef union union_NestedUnion_Double {
	union_2_Doubles elem1;
	double elem2;
} union_NestedUnion_Double;

typedef union union_Double_NestedUnion {
	double elem1;
	union_2_Doubles elem2;
} union_Double_NestedUnion;

typedef union union_NestedDoubleArray_Double {
	double elem1[2];
	double elem2;
} union_NestedDoubleArray_Double;

typedef union union_Double_NestedDoubleArray {
	double elem1;
	double elem2[2];
} union_Double_NestedDoubleArray;

typedef union union_NestedUnionArray_Double {
	union_2_Doubles elem1[2];
	double elem2;
} union_NestedUnionArray_Double;

typedef union union_Double_NestedUnionArray {
	double elem1;
	union_2_Doubles elem2[2];
} union_Double_NestedUnionArray;

typedef union union_Byte_Short {
	char elem1;
	short elem2;
} union_Byte_Short;

typedef union union_Short_Byte {
	short elem1;
	char elem2;
} union_Short_Byte;

typedef union union_Int_Byte {
	int elem1;
	char elem2;
} union_Int_Byte;

typedef union union_Byte_Int {
	char elem1;
	int elem2;
} union_Byte_Int;

typedef union union_Int_Short {
	int elem1;
	short elem2;
} union_Int_Short;

typedef union union_Short_Int {
	short elem1;
	int elem2;
} union_Short_Int;

typedef union union_Short_Float{
	short elem1;
	float elem2;
} union_Short_Float;

typedef union union_Float_Short {
	float elem1;
	short elem2;
} union_Float_Short;

typedef union union_Int_Float{
	int elem1;
	float elem2;
} union_Int_Float;

typedef union union_Float_Int {
	float elem1;
	int elem2;
} union_Float_Int;

typedef union union_Int_Short_Byte {
	int elem1;
	short elem2;
	char elem3;
} union_Int_Short_Byte;

typedef union union_Byte_Short_Int {
	char elem1;
	short elem2;
	int elem3;
} union_Byte_Short_Int;

typedef union union_Int_Long {
	int elem1;
	LONG elem2;
} union_Int_Long;

typedef union union_Long_Int {
	LONG elem1;
	int elem2;
} union_Long_Int;

typedef union union_Float_Double {
	float elem1;
	double elem2;
} union_Float_Double;

typedef union union_Double_Float {
	double elem1;
	float elem2;
} union_Double_Float;

typedef union union_Int_Double {
	int elem1;
	double elem2;
} union_Int_Double;

typedef union union_Double_Int {
	double elem1;
	int elem2;
} union_Double_Int;

typedef union union_Float_Long {
	float elem1;
	LONG elem2;
} union_Float_Long;

typedef union union_Long_Float {
	LONG elem1;
	float elem2;
} union_Long_Float;

typedef union union_Long_Double {
	LONG elem1;
	double elem2;
} union_Long_Double;

typedef union union_Double_Long {
	double elem1;
	LONG elem2;
} union_Double_Long;

typedef union union_Int_Float_Double {
	int elem1;
	float elem2;
	double elem3;
} union_Int_Float_Double;

typedef union union_Int_Double_Float {
	int elem1;
	double elem2;
	float elem3;
} union_Int_Double_Float;

typedef union union_Int_Float_Long {
	int elem1;
	float elem2;
	LONG elem3;
} union_Int_Float_Long;

typedef union union_Int_Long_Float {
	int elem1;
	LONG elem2;
	float elem3;
} union_Int_Long_Float;

typedef union union_Float_Long_Double {
	float elem1;
	LONG elem2;
	double elem3;
} union_Float_Long_Double;

typedef union union_Int_Double_Long {
	int elem1;
	double elem2;
	LONG elem3;
} union_Int_Double_Long;

typedef union union_Int_Double_Ptr {
	int elem1;
	double elem2;
	LONG *elem3;
} union_Int_Double_Ptr;

typedef union union_Int_Float_Ptr {
	int elem1;
	float elem2;
	LONG *elem3;
} union_Int_Float_Ptr;

typedef union union_Int_Ptr_Float {
	int elem1;
	LONG *elem2;
	float elem3;
} union_Int_Ptr_Float;

typedef union union_Byte_Short_Int_Long {
	char elem1;
	short elem2;
	int elem3;
	LONG elem4;
} union_Byte_Short_Int_Long;

typedef union union_Long_Int_Short_Byte {
	LONG elem1;
	int elem2;
	short elem3;
	char elem4;
} union_Long_Int_Short_Byte;

typedef union union_Byte_Short_Float_Double {
	char elem1;
	short elem2;
	float elem3;
	double elem4;
} union_Byte_Short_Float_Double;

typedef union union_Double_Float_Short_Byte {
	double elem1;
	float elem2;
	short elem3;
	char elem4;
} union_Double_Float_Short_Byte;

typedef struct stru_Bool_NestedUnion_2_Bools {
	bool elem1;
	union_2_Bools elem2;
} stru_Bool_NestedUnion_2_Bools;

typedef union union_Bool_NestedStruct_2_Bools {
	bool elem1;
	stru_2_Bools elem2;
} union_Bool_NestedStruct_2_Bools;

typedef union union_Bool_NestedStruct_4_Bools {
	bool elem1;
	stru_4_Bools elem2;
} union_Bool_NestedStruct_4_Bools;

typedef struct stru_Byte_NestedUnion_2_Bytes {
	char elem1;
	union_2_Bytes elem2;
} stru_Byte_NestedUnion_2_Bytes;

typedef union union_Byte_NestedStruct_2_Bytes {
	char elem1;
	stru_2_Bytes elem2;
} union_Byte_NestedStruct_2_Bytes;

typedef union union_Byte_NestedStruct_4_Bytes {
	char elem1;
	stru_4_Bytes elem2;
} union_Byte_NestedStruct_4_Bytes;

typedef struct stru_Char_NestedUnion_2_Chars {
	short elem1;
	union_2_Chars elem2;
} stru_Char_NestedUnion_2_Chars;

typedef union union_Char_NestedStruct_2_Chars {
	short elem1;
	stru_2_Chars elem2;
} union_Char_NestedStruct_2_Chars;

typedef union union_Char_NestedStruct_4_Chars {
	short elem1;
	stru_4_Chars elem2;
} union_Char_NestedStruct_4_Chars;

typedef struct stru_Short_NestedUnion_2_Shorts {
	short elem1;
	union_2_Shorts elem2;
} stru_Short_NestedUnion_2_Shorts;

typedef union union_Short_NestedStruct_2_Shorts {
	short elem1;
	stru_2_Shorts elem2;
} union_Short_NestedStruct_2_Shorts;

typedef union union_Short_NestedStruct_4_Shorts {
	short elem1;
	stru_4_Shorts elem2;
} union_Short_NestedStruct_4_Shorts;

typedef struct stru_Int_NestedUnion_2_Ints {
	int elem1;
	union_2_Ints elem2;
} stru_Int_NestedUnion_2_Ints;

typedef union union_Int_NestedStruct_2_Ints {
	int elem1;
	stru_2_Ints elem2;
} union_Int_NestedStruct_2_Ints;

typedef union union_Int_NestedStruct_4_Ints {
	int elem1;
	stru_4_Ints elem2;
} union_Int_NestedStruct_4_Ints;

typedef struct stru_Long_NestedUnion_2_Longs {
	LONG elem1;
	union_2_Longs elem2;
} stru_Long_NestedUnion_2_Longs;

typedef union union_Long_NestedStruct_2_Longs {
	LONG elem1;
	stru_2_Longs elem2;
} union_Long_NestedStruct_2_Longs;

typedef union union_Long_NestedStruct_4_Longs {
	LONG elem1;
	stru_4_Longs elem2;
} union_Long_NestedStruct_4_Longs;

typedef struct stru_Float_NestedUnion_2_Floats {
	float elem1;
	union_2_Floats elem2;
} stru_Float_NestedUnion_2_Floats;

typedef union union_Float_NestedStruct_2_Floats {
	float elem1;
	stru_2_Floats elem2;
} union_Float_NestedStruct_2_Floats;

typedef union union_Float_NestedStruct_4_Floats {
	float elem1;
	stru_4_Floats elem2;
} union_Float_NestedStruct_4_Floats;

typedef struct stru_Double_NestedUnion_2_Doubles {
	double elem1;
	union_2_Doubles elem2;
} stru_Double_NestedUnion_2_Doubles;

typedef union union_Double_NestedStruct_2_Doubles {
	double elem1;
	stru_2_Doubles elem2;
} union_Double_NestedStruct_2_Doubles;

typedef union union_Double_NestedStruct_4_Doubles {
	double elem1;
	stru_4_Doubles elem2;
} union_Double_NestedStruct_4_Doubles;

typedef union union_Short_NestedStruct_2_Bytes {
	short elem1;
	stru_2_Bytes elem2;
} union_Short_NestedStruct_2_Bytes;

typedef union union_Short_NestedStruct_4_Bytes {
	short elem1;
	stru_4_Bytes elem2;
} union_Short_NestedStruct_4_Bytes;

typedef union union_Int_NestedStruct_2_Shorts {
	int elem1;
	stru_2_Shorts elem2;
} union_Int_NestedStruct_2_Shorts;

typedef union union_Int_NestedStruct_4_Shorts {
	int elem1;
	stru_4_Shorts elem2;
} union_Int_NestedStruct_4_Shorts;

typedef union union_Float_NestedStruct_2_Shorts {
	float elem1;
	stru_2_Shorts elem2;
} union_Float_NestedStruct_4_Shorts;

typedef union union_Long_NestedStruct_2_Ints {
	LONG elem1;
	stru_2_Ints elem2;
} union_Long_NestedStruct_2_Ints;

typedef union union_Long_NestedStruct_2_Floats {
	LONG elem1;
	stru_2_Floats elem2;
} union_Long_NestedStruct_2_Floats;

typedef union union_Double_NestedStruct_2_Ints {
	double elem1;
	stru_2_Ints elem2;
} union_Double_NestedStruct_2_Ints;

typedef union union_Double_NestedStruct_2_Floats {
	double elem1;
	stru_2_Floats elem2;
} union_Double_NestedStruct_2_Floats;

#endif /* DOWNCALL_H */
