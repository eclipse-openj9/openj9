################################################################################
# Copyright (c) 2021, 2022 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
################################################################################

set(OMR_ENHANCED_WARNINGS OFF)
j9vm_add_library(clinkerffitests SHARED
	downcall.c
)
target_link_libraries(clinkerffitests
	PRIVATE
		j9vm_interface
		jvm
)

omr_add_exports(clinkerffitests
	add2Ints
	add3Ints
	addIntAndChar
	add2IntsReturnVoid
	add2BoolsWithOr
	createNewCharFrom2Chars
	add2Bytes
	add2Shorts
	add2Longs
	add2Floats
	add2Doubles
	addIntsFromVaList
	addLongsFromVaList
	addDoublesFromVaList
	addIntAndIntFromPointer
	addBoolAndBoolFromPointerWithOr
	createNewCharFromCharAndCharFromPointer
	addByteAndByteFromPointer
	addShortAndShortFromPointer
	addLongAndLongFromPointer
	addFloatAndFloatFromPointer
	addDoubleAndDoubleFromPointer
	addBoolAndBoolsFromStructWithXor
	addBoolFromPointerAndBoolsFromStructWithXor
	addBoolFromPointerAndBoolsFromStructWithXor_returnBoolPointer
	addBoolAndBoolsFromStructPointerWithXor
	addBoolAndBoolsFromNestedStructWithXor
	addBoolAndBoolsFromNestedStructWithXor_reverseOrder
	addBoolAndBoolsFromStructWithNestedBoolArray
	addBoolAndBoolsFromStructWithNestedBoolArray_reverseOrder
	addBoolAndBoolsFromStructWithNestedStructArray
	addBoolAndBoolsFromStructWithNestedStructArray_reverseOrder
	add2BoolStructsWithXor_returnStruct
	add2BoolStructsWithXor_returnStructPointer
	add3BoolStructsWithXor_returnStruct
	addByteAndBytesFromStruct
	addByteFromPointerAndBytesFromStruct
	addByteFromPointerAndBytesFromStruct_returnBytePointer
	addByteAndBytesFromStructPointer
	addByteAndBytesFromNestedStruct
	addByteAndBytesFromNestedStruct_reverseOrder
	addByteAndBytesFromStructWithNestedByteArray
	addByteAndBytesFromStructWithNestedByteArray_reverseOrder
	addByteAndBytesFromStructWithNestedStructArray
	addByteAndBytesFromStructWithNestedStructArray_reverseOrder
	add2ByteStructs_returnStruct
	add2ByteStructs_returnStructPointer
	add3ByteStructs_returnStruct
	addCharAndCharsFromStruct
	addCharFromPointerAndCharsFromStruct
	addCharFromPointerAndCharsFromStruct_returnCharPointer
	addCharAndCharsFromStructPointer
	addCharAndCharsFromNestedStruct
	addCharAndCharsFromNestedStruct_reverseOrder
	addCharAndCharsFromStructWithNestedCharArray
	addCharAndCharsFromStructWithNestedCharArray_reverseOrder
	addCharAndCharsFromStructWithNestedStructArray
	addCharAndCharsFromStructWithNestedStructArray_reverseOrder
	add2CharStructs_returnStruct
	add2CharStructs_returnStructPointer
	add3CharStructs_returnStruct
	addShortAndShortsFromStruct
	addShortFromPointerAndShortsFromStruct
	addShortFromPointerAndShortsFromStruct_returnShortPointer
	addShortAndShortsFromStructPointer
	addShortAndShortsFromNestedStruct
	addShortAndShortsFromNestedStruct_reverseOrder
	addShortAndShortsFromStructWithNestedShortArray
	addShortAndShortsFromStructWithNestedShortArray_reverseOrder
	addShortAndShortsFromStructWithNestedStructArray
	addShortAndShortsFromStructWithNestedStructArray_reverseOrder
	add2ShortStructs_returnStruct
	add2ShortStructs_returnStructPointer
	add3ShortStructs_returnStruct
	addIntAndIntsFromStruct
	addIntAndIntShortFromStruct
	addIntAndShortIntFromStruct
	addIntFromPointerAndIntsFromStruct
	addIntFromPointerAndIntsFromStruct_returnIntPointer
	addIntAndIntsFromStructPointer
	addIntAndIntsFromNestedStruct
	addIntAndIntsFromNestedStruct_reverseOrder
	addIntAndIntsFromStructWithNestedIntArray
	addIntAndIntsFromStructWithNestedIntArray_reverseOrder
	addIntAndIntsFromStructWithNestedStructArray
	addIntAndIntsFromStructWithNestedStructArray_reverseOrder
	add2IntStructs_returnStruct
	add2IntStructs_returnStructPointer
	add3IntStructs_returnStruct
	addLongAndLongsFromStruct
	addIntAndIntLongFromStruct
	addIntAndLongIntFromStruct
	addLongFromPointerAndLongsFromStruct
	addLongFromPointerAndLongsFromStruct_returnLongPointer
	addLongAndLongsFromStructPointer
	addLongAndLongsFromNestedStruct
	addLongAndLongsFromNestedStruct_reverseOrder
	addLongAndLongsFromStructWithNestedLongArray
	addLongAndLongsFromStructWithNestedLongArray_reverseOrder
	addLongAndLongsFromStructWithNestedStructArray
	addLongAndLongsFromStructWithNestedStructArray_reverseOrder
	add2LongStructs_returnStruct
	add2LongStructs_returnStructPointer
	add3LongStructs_returnStruct
	addFloatAndFloatsFromStruct
	addFloatFromPointerAndFloatsFromStruct
	addFloatFromPointerAndFloatsFromStruct_returnFloatPointer
	addFloatAndFloatsFromStructPointer
	addFloatAndFloatsFromNestedStruct
	addFloatAndFloatsFromNestedStruct_reverseOrder
	addFloatAndFloatsFromStructWithNestedFloatArray
	addFloatAndFloatsFromStructWithNestedFloatArray_reverseOrder
	addFloatAndFloatsFromStructWithNestedStructArray
	addFloatAndFloatsFromStructWithNestedStructArray_reverseOrder
	add2FloatStructs_returnStruct
	add2FloatStructs_returnStructPointer
	add3FloatStructs_returnStruct
	addDoubleAndDoublesFromStruct
	addDoubleAndFloatDoubleFromStruct
	addDoubleAndIntDoubleFromStruct
	addDoubleAndDoubleFloatFromStruct
	addDoubleAndDoubleIntFromStruct
	addDoubleFromPointerAndDoublesFromStruct
	addDoubleFromPointerAndDoublesFromStruct_returnDoublePointer
	addDoubleAndDoublesFromStructPointer
	addDoubleAndDoublesFromNestedStruct
	addDoubleAndDoublesFromNestedStruct_reverseOrder
	addDoubleAndDoublesFromStructWithNestedDoubleArray
	addDoubleAndDoublesFromStructWithNestedDoubleArray_reverseOrder
	addDoubleAndDoublesFromStructWithNestedStructArray
	addDoubleAndDoublesFromStructWithNestedStructArray_reverseOrder
	add2DoubleStructs_returnStruct
	add2DoubleStructs_returnStructPointer
	add3DoubleStructs_returnStruct
)

install(
	TARGETS clinkerffitests
	LIBRARY DESTINATION ${j9vm_SOURCE_DIR}
	RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
)
