/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#include "j9.h"
#include "bcverify.h"
#include "cfreader.h"

/* Mapping the verification encoding in J9JavaBytecodeVerificationTable */
const U_32 decodeTable[] = {
	0x0,						/* return index */
	BCV_BASE_TYPE_INT,			/* CFR_STACKMAP_TYPE_INT */
	BCV_BASE_TYPE_FLOAT,		/* CFR_STACKMAP_TYPE_FLOAT */
	BCV_BASE_TYPE_DOUBLE,		/* CFR_STACKMAP_TYPE_DOUBLE */
	BCV_BASE_TYPE_LONG,			/* CFR_STACKMAP_TYPE_LONG */
	BCV_BASE_TYPE_NULL,			/* CFR_STACKMAP_TYPE_NULL */
	0x6,						/* return index */
	BCV_GENERIC_OBJECT,			/* CFR_STACKMAP_TYPE_OBJECT */
	0x8,						/* return index */
	0x9,						/* return index */
	0xA,						/* return index */
	0xB,						/* return index */
	0xC,						/* return index */
	0xD,						/* return index */
	0xE,						/* return index */
	0xF							/* return index */
};

/* mapping JBnewarray parameter 0 to 11 */
const U_32 newArrayParamConversion[] = {
0,
0,
0,
0,
(BCV_BASE_TYPE_BOOL_BIT | BCV_TAG_BASE_ARRAY_OR_NULL),		/* 4 */
(BCV_BASE_TYPE_CHAR_BIT | BCV_TAG_BASE_ARRAY_OR_NULL),		/* 5 */
(BCV_BASE_TYPE_FLOAT_BIT | BCV_TAG_BASE_ARRAY_OR_NULL),		/* 6 */
(BCV_BASE_TYPE_DOUBLE_BIT | BCV_TAG_BASE_ARRAY_OR_NULL),	/* 7 */
(BCV_BASE_TYPE_BYTE_BIT | BCV_TAG_BASE_ARRAY_OR_NULL),		/* 8 */
(BCV_BASE_TYPE_SHORT_BIT | BCV_TAG_BASE_ARRAY_OR_NULL),		/* 9 */
(BCV_BASE_TYPE_INT_BIT | BCV_TAG_BASE_ARRAY_OR_NULL),		/* 10 */
(BCV_BASE_TYPE_LONG_BIT | BCV_TAG_BASE_ARRAY_OR_NULL)};		/* 11 */

/* mapping characters A..Z,[ */
const U_32 baseTypeCharConversion[] = {
0,						BCV_BASE_TYPE_BYTE_BIT,		BCV_BASE_TYPE_CHAR_BIT,		BCV_BASE_TYPE_DOUBLE_BIT,
0,						BCV_BASE_TYPE_FLOAT_BIT,	0,							0,
BCV_BASE_TYPE_INT_BIT,	BCV_BASE_TYPE_LONG_BIT,		0,							0,
0,						0,							0,							0,
0,						0,							BCV_BASE_TYPE_SHORT_BIT,	0, 
0,						0,							0,							0,
0,						BCV_BASE_TYPE_BOOL_BIT,		0};

/* mapping characters A..Z,[ */
const U_32 argTypeCharConversion[] = {
0,					BCV_BASE_TYPE_INT,		BCV_BASE_TYPE_INT,		BCV_BASE_TYPE_DOUBLE,
0,					BCV_BASE_TYPE_FLOAT,	0,						0,
BCV_BASE_TYPE_INT,	BCV_BASE_TYPE_LONG,		0,						0,
0,					0,						0,						0,
0,					0,						BCV_BASE_TYPE_INT,		0, 
0,					0,						0,						0,
0,					BCV_BASE_TYPE_INT,		0}; 

/* mapping characters A..Z,[ */
const U_32 oneArgTypeCharConversion[] = {
0,					BCV_BASE_TYPE_INT,		BCV_BASE_TYPE_INT,		0,
0,					BCV_BASE_TYPE_FLOAT,	0,						0,
BCV_BASE_TYPE_INT,	0,						0,						0,
0,					0,						0,						0,
0,					0,						BCV_BASE_TYPE_INT,		0, 
0,					0,						0,						0,
0,					BCV_BASE_TYPE_INT,		0};

/* mapping verification base types */
const U_8 verificationBaseTokenEncode[] = {
CFR_STACKMAP_TYPE_TOP,			CFR_STACKMAP_TYPE_INT,	CFR_STACKMAP_TYPE_FLOAT,	0,
CFR_STACKMAP_TYPE_LONG,			0,						0,							0,
CFR_STACKMAP_TYPE_DOUBLE,		0,						0,							0
};

/* mapping verification base types */
const U_8 verificationBaseArrayTokenEncode[] = {
CFR_STACKMAP_TYPE_NULL,			CFR_STACKMAP_TYPE_INT_ARRAY,	CFR_STACKMAP_TYPE_FLOAT_ARRAY,	0,
CFR_STACKMAP_TYPE_LONG_ARRAY,	0,								0,								0,
CFR_STACKMAP_TYPE_DOUBLE_ARRAY,	0,								0,								0,
0,								0,								0,								0,
CFR_STACKMAP_TYPE_SHORT_ARRAY,	0,								0,								0,
0,								0,								0,								0,
0,								0,								0,								0,
0,								0,								0,								0,
CFR_STACKMAP_TYPE_BYTE_ARRAY,	0,								0,								0,
0,								0,								0,								0,
0,								0,								0,								0,
0,								0,								0,								0,
0,								0,								0,								0,
0,								0,								0,								0,
0,								0,								0,								0,
0,								0,								0,								0,
CFR_STACKMAP_TYPE_CHAR_ARRAY,	0,								0,								0	
};

/* unmapping verification types */
const U_32 verificationTokenDecode[] = {
BCV_BASE_TYPE_TOP,				/* 0 - CFR_STACKMAP_TYPE_TOP */
BCV_BASE_TYPE_INT,				/* 1 - CFR_STACKMAP_TYPE_INT */
BCV_BASE_TYPE_FLOAT,			/* 2 - CFR_STACKMAP_TYPE_FLOAT */
BCV_BASE_TYPE_DOUBLE,			/* 3 - CFR_STACKMAP_TYPE_DOUBLE */
BCV_BASE_TYPE_LONG,				/* 4 - CFR_STACKMAP_TYPE_LONG */
BCV_BASE_TYPE_NULL,				/* 5 - CFR_STACKMAP_TYPE_NULL */
BCV_SPECIAL_INIT,				/* 6 - CFR_STACKMAP_TYPE_INIT_OBJECT */
BCV_GENERIC_OBJECT,				/* 7 - CFR_STACKMAP_TYPE_OBJECT - special handling */
BCV_SPECIAL_NEW,				/* 8 - CFR_STACKMAP_TYPE_NEW_OBJECT - special handling */
BCV_BASE_ARRAY_TYPE_INT,		/* 9 - CFR_STACKMAP_TYPE_INT_ARRAY */
BCV_BASE_ARRAY_TYPE_FLOAT,		/* A - CFR_STACKMAP_TYPE_FLOAT_ARRAY */
BCV_BASE_ARRAY_TYPE_DOUBLE,		/* B - CFR_STACKMAP_TYPE_DOUBLE_ARRAY */
BCV_BASE_ARRAY_TYPE_LONG,		/* C - CFR_STACKMAP_TYPE_LONG_ARRAY */
BCV_BASE_ARRAY_TYPE_SHORT,		/* D - CFR_STACKMAP_TYPE_SHORT_ARRAY */
BCV_BASE_ARRAY_TYPE_BYTE,		/* E - CFR_STACKMAP_TYPE_BYTE_ARRAY */
BCV_BASE_ARRAY_TYPE_CHAR,		/* F - CFR_STACKMAP_TYPE_CHAR_ARRAY */
BCV_BASE_ARRAY_TYPE_BOOL		/* 10 - CFR_STACKMAP_TYPE_BOOL_ARRAY */
};
