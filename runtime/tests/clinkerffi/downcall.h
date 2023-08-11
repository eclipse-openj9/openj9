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
 * This file contains the structs required in the native code via a Clinker FFI DownCall & Upcall in java,
 * which come from:
 * org.openj9.test.jep389.downcall/upcall (JDK16/17)
 * org.openj9.test.jep419.downcall/upcall (JDK18)
 * org.openj9.test.jep424.downcall/upcall (JDK19+)
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

typedef struct stru_Bool_Bool {
	bool elem1;
	bool elem2;
} stru_Bool_Bool;

typedef struct stru_Bool_Bool_Bool {
	bool elem1;
	bool elem2;
	bool elem3;
} stru_Bool_Bool_Bool;

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
	stru_Bool_Bool elem1;
	bool elem2;
} stru_NestedStruct_Bool;

typedef struct stru_Bool_NestedStruct {
	bool elem1;
	stru_Bool_Bool elem2;
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
	stru_Bool_Bool elem1[2];
	bool elem2;
} stru_NestedStruArray_Bool;

typedef struct stru_Bool_NestedStruArray {
	bool elem1;
	stru_Bool_Bool elem2[2];
} stru_Bool_NestedStruArray;

typedef struct stru_Byte {
	char elem1;
} stru_Byte;

typedef struct stru_Byte_Byte {
	char elem1;
	char elem2;
} stru_Byte_Byte;

typedef struct stru_Byte_Byte_Byte {
	char elem1;
	char elem2;
	char elem3;
} stru_Byte_Byte_Byte;

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
	stru_Byte_Byte elem1;
	char elem2;
} stru_NestedStruct_Byte;

typedef struct stru_Byte_NestedStruct {
	char elem1;
	stru_Byte_Byte elem2;
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
	stru_Byte_Byte elem1[2];
	char elem2;
} stru_NestedStruArray_Byte;

typedef struct stru_Byte_NestedStruArray {
	char elem1;
	stru_Byte_Byte elem2[2];
} stru_Byte_NestedStruArray;

typedef struct stru_Char_Char {
	short elem1;
	short elem2;
} stru_Char_Char;

typedef struct stru_Char_Char_Char {
	short elem1;
	short elem2;
	short elem3;
} stru_Char_Char_Char;

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
	stru_Char_Char elem1;
	short elem2;
} stru_NestedStruct_Char;

typedef struct stru_Char_NestedStruct {
	short elem1;
	stru_Char_Char elem2;
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
	stru_Char_Char elem1[2];
	short elem2;
} stru_NestedStruArray_Char;

typedef struct stru_Char_NestedStruArray {
	short elem1;
	stru_Char_Char elem2[2];
} stru_Char_NestedStruArray;

typedef struct stru_Short {
	short elem1;
} stru_Short;

typedef struct stru_Short_Short {
	short elem1;
	short elem2;
} stru_Short_Short;

typedef struct stru_Short_Short_Short {
	short elem1;
	short elem2;
	short elem3;
} stru_Short_Short_Short;

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
	stru_Short_Short elem1;
	short elem2;
} stru_NestedStruct_Short;

typedef struct stru_Short_NestedStruct {
	short elem1;
	stru_Short_Short elem2;
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
	stru_Short_Short elem1[2];
	short elem2;
} stru_NestedStruArray_Short;

typedef struct stru_Short_NestedStruArray {
	short elem1;
	stru_Short_Short elem2[2];
} stru_Short_NestedStruArray;

typedef struct stru_Int {
	int elem1;
} stru_Int;

typedef struct stru_Int_Int {
	int elem1;
	int elem2;
} stru_Int_Int;

typedef struct stru_Int_Int_Int {
	int elem1;
	int elem2;
	int elem3;
} stru_Int_Int_Int;

typedef struct stru_5_Ints {
	int elem1;
	int elem2;
	int elem3;
	int elem4;
	int elem5;
} stru_5_Ints;

typedef struct stru_NestedStruct_Int {
	stru_Int_Int elem1;
	int elem2;
} stru_NestedStruct_Int;

typedef struct stru_Int_NestedStruct {
	int elem1;
	stru_Int_Int elem2;
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
	stru_Int_Int elem1[2];
	int elem2;
} stru_NestedStruArray_Int;

typedef struct stru_Int_NestedStruArray {
	int elem1;
	stru_Int_Int elem2[2];
} stru_Int_NestedStruArray;

typedef struct stru_Long_Long {
	LONG elem1;
	LONG elem2;
} stru_Long_Long;

typedef struct stru_Long_Long_Long {
	LONG elem1;
	LONG elem2;
	LONG elem3;
} stru_Long_Long_Long;

typedef struct stru_NestedStruct_Long {
	stru_Long_Long elem1;
	LONG elem2;
} stru_NestedStruct_Long;

typedef struct stru_Long_NestedStruct {
	LONG elem1;
	stru_Long_Long elem2;
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
	stru_Long_Long elem1[2];
	LONG elem2;
} stru_NestedStruArray_Long;

typedef struct stru_Long_NestedStruArray {
	LONG elem1;
	stru_Long_Long elem2[2];
} stru_Long_NestedStruArray;

typedef struct stru_Float {
	float elem1;
} stru_Float;

typedef struct stru_Float_Float {
	float elem1;
	float elem2;
} stru_Float_Float;

typedef struct stru_Float_Float_Float {
	float elem1;
	float elem2;
	float elem3;
} stru_Float_Float_Float;

typedef struct stru_5_Floats {
	float elem1;
	float elem2;
	float elem3;
	float elem4;
	float elem5;
} stru_5_Floats;

typedef struct stru_NestedStruct_Float {
	stru_Float_Float elem1;
	float elem2;
} stru_NestedStruct_Float;

typedef struct stru_Float_NestedStruct {
	float elem1;
	stru_Float_Float elem2;
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
	stru_Float_Float elem1[2];
	float elem2;
} stru_NestedStruArray_Float;

typedef struct stru_Float_NestedStruArray {
	float elem1;
	stru_Float_Float elem2[2];
} stru_Float_NestedStruArray;

typedef struct stru_Double {
	double elem1;
} stru_Double;

typedef struct stru_Double_Double {
	double elem1;
	double elem2;
} stru_Double_Double;

typedef struct stru_Double_Double_Double {
	double elem1;
	double elem2;
	double elem3;
} stru_Double_Double_Double;

typedef struct stru_NestedStruct_Double {
	stru_Double_Double elem1;
	double elem2;
} stru_NestedStruct_Double;

typedef struct stru_Double_NestedStruct {
	double elem1;
	stru_Double_Double elem2;
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
	stru_Double_Double elem1[2];
	double elem2;
} stru_NestedStruArray_Double;

typedef struct stru_Double_NestedStruArray {
	double elem1;
	stru_Double_Double elem2[2];
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

typedef struct stru_Float_Float_Double {
	float elem1;
	float elem2;
	double elem3;
} stru_Float_Float_Double;

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

typedef struct stru_Float_Float_Long {
	float elem1;
	float elem2;
	LONG elem3;
} stru_Float_Float_Long;

typedef struct stru_3_Shorts_Char {
	short elem1[3];
	short elem2; // intended for the char type in java
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

#endif /* DOWNCALL_H */
