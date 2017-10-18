/*******************************************************************************
 * Copyright (c) 2015, 2018 IBM Corp. and others
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

#include "errormessage_internal.h"

/* Data type string */
const char* dataTypeNames[] = {
	"top",											/* CFR_STACKMAP_TYPE_TOP							0x00 */
	"integer",										/* CFR_STACKMAP_TYPE_INT							0x01 */
	"float",										/* CFR_STACKMAP_TYPE_FLOAT 							0x02 */
	"double",										/* CFR_STACKMAP_TYPE_DOUBLE							0x03 */
	"long",											/* CFR_STACKMAP_TYPE_LONG 							0x04 */
	"null",											/* CFR_STACKMAP_TYPE_NULL							0x05 */
	"uninitializedThis",							/* CFR_STACKMAP_TYPE_INIT_OBJECT					0x06 */
	"reference",									/* CFR_STACKMAP_TYPE_OBJECT							0x07 */
	"uninitialized",								/* CFR_STACKMAP_TYPE_NEW_OBJECT						0x08 */
	"I",											/* CFR_STACKMAP_TYPE_INT_ARRAY						0x09 */
	"F",											/* CFR_STACKMAP_TYPE_FLOAT_ARRAY					0x0A */
	"D",											/* CFR_STACKMAP_TYPE_DOUBLE_ARRAY					0x0B */
	"J",											/* CFR_STACKMAP_TYPE_LONG_ARRAY						0x0C */
	"S",											/* CFR_STACKMAP_TYPE_SHORT_ARRAY					0x0D */
	"B",											/* CFR_STACKMAP_TYPE_BYTE_ARRAY						0x0E */
	"C",											/* CFR_STACKMAP_TYPE_CHAR_ARRAY						0x0F */
	"Z"												/* CFR_STACKMAP_TYPE_BOOL_ARRAY						0x10 */
};

/* Length of data type string */
const UDATA dataTypeLength[] = {
	sizeof("top") - 1,					/* top */
	sizeof("integer") - 1,				/* integer */
	sizeof("float") - 1,				/* float */
	sizeof("double") - 1,				/* double */
	sizeof("long") - 1,					/* long */
	sizeof("null") - 1,					/* null */
	sizeof("uninitializedThis") - 1,	/* uninitializedThis */
	sizeof("reference") - 1,			/* reference */
	sizeof("uninitialized") - 1,		/* uninitialized */
	sizeof("I") - 1,					/* I */
	sizeof("F") - 1,					/* F */
	sizeof("D") - 1,					/* D */
	sizeof("J") - 1,					/* J */
	sizeof("S") - 1,					/* S */
	sizeof("B") - 1,					/* B */
	sizeof("C") - 1,					/* C */
	sizeof("Z") - 1						/* Z */
};
