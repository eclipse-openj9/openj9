/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
#ifndef j9ddr_h
#define j9ddr_h

#include "j9comp.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef J9_STR
#define J9_STR_(x) #x
#define J9_STR(x) J9_STR_(x)
#endif

struct OMRPortLibrary;
typedef struct J9DDRCmdlineOptions {
	int argc;
	char** argv;
	char** envp;
	struct OMRPortLibrary* portLibrary;
	BOOLEAN shutdownPortLib;
} J9DDRCmdlineOptions;

typedef struct J9DDRFieldDeclaration {
	const char* name;
	const char* type;
	U_32 offset;
} J9DDRFieldDeclaration;

typedef struct J9DDRConstantDeclaration {
	U_64 value;
	const char* name;
} J9DDRConstantDeclaration;

typedef struct J9DDRStructDefinition {
	const char* name;
	const char* superName;
	const struct J9DDRFieldDeclaration* fields;
	const struct J9DDRConstantDeclaration* constants;
	U_32 size;
} J9DDRStructDefinition;

#define J9DDRFieldTableBegin(name)	static const J9DDRFieldDeclaration J9DDR_##name##_fields [] = {
/*((U_32)(UDATA)&(((structName*)1)->fieldName)) - 1 pattern avoids compile warnings on Linux */
#define J9DDRFieldTableEntry(structName, fieldName, fieldType)	{ J9_STR(fieldName), J9_STR(fieldType), ((U_32)(UDATA)&(((structName*)1)->fieldName)) - 1 },
#define J9DDRBitFieldTableEntry(fieldName, fieldType) { (fieldName), J9_STR(fieldType), 0 },
#define J9DDRFieldTableEnd            { 0, 0, 0 } };

#define J9DDRConstantTableBegin(name)	static const J9DDRConstantDeclaration J9DDR_##name##_constants [] = {
#define J9DDRConstantTableEntry(constantName)	{ (U_64) (constantName), J9_STR(constantName) },
#define J9DDRConstantTableEntryWithValue(constantName, constantValue)	{ (U_64) (constantValue), (constantName) },
#define J9DDRConstantTableEnd             { 0, 0 } };

#define J9DDRStructTableBegin(name)	const J9DDRStructDefinition J9DDR_##name##_structs [] = {
#define J9DDRStruct(structName, superStruct)	{ #structName, (superStruct), J9DDR_##structName##_fields, J9DDR_##structName##_constants, (U_32)sizeof(structName) },
#define J9DDRStructWithName(structName, name, superStruct)	{ #name, (superStruct), J9DDR_##name##_fields, J9DDR_##name##_constants, (U_32)sizeof(structName) },
#define J9DDREmptyStruct(structName, superStruct)	{ #structName, (superStruct), NULL, J9DDR_##structName##_constants, (U_32)0 },
#define J9DDRStructTableEnd            { 0, 0, 0, 0, 0 } };

#ifdef __cplusplus
}
#endif

#endif     /* j9ddr_h */

