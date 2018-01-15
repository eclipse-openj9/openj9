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

#ifndef bytecodewalk_h
#define bytecodewalk_h

/* Bytecode groupings for verification */ 
#define RTV_NOP						0x01
#define RTV_PUSH_CONSTANT			0x02
#define RTV_PUSH_CONSTANT_POOL_ITEM	0x03
#define RTV_LOAD_TEMP_PUSH			0x04
#define RTV_ARRAY_FETCH_PUSH		0x05
#define RTV_POP_STORE_TEMP			0x06
#define RTV_ARRAY_STORE				0x07
#define RTV_INCREMENT				0x08
#define RTV_POP_2_PUSH				0x09
#define RTV_POP_X_PUSH_X			0x0A
#define RTV_POP_X_PUSH_Y			0x0B
									/* gap */
#define RTV_BRANCH					0x0E
#define RTV_RETURN					0x0F
#define RTV_STATIC_FIELD_ACCESS		0x10
#define RTV_SEND					0x11
#define RTV_PUSH_NEW				0x12
#define RTV_MISC					0x13
#define RTV_WIDE_LOAD_TEMP_PUSH		0x14
#define RTV_WIDE_POP_STORE_TEMP		0x15
#define RTV_POP_2_PUSH_INT			0x16
#define	RTV_UNIMPLEMENTED			0x17
#define RTV_BYTECODE_POP			0x18
#define RTV_BYTECODE_POP2			0x19
#define RTV_BYTECODE_DUP			0x1A
#define RTV_BYTECODE_DUPX1			0x1B
#define RTV_BYTECODE_DUPX2			0x1C
#define RTV_BYTECODE_DUP2			0x1D
#define RTV_BYTECODE_DUP2X1			0x1E
#define RTV_BYTECODE_DUP2X2			0x1f
#define RTV_BYTECODE_SWAP			0x20

/* 

32bit type => [8 bits arity] [ 19 bits class index] [5 tag bits]

tag bits:
	special (new / init / ret)
	base / object
	base type array / regular object, array
	null

base types: (in the 19bit class index field)
	int
	float
	long
	double
	char
	short
	byte
	bool

*/

#define BCV_ARITY_MASK						0xFF000000
#define BCV_ARITY_SHIFT						24
#define BCV_CLASS_INDEX_MASK				0x00FFFFE0
#define BCV_CLASS_INDEX_SHIFT				5

#define BCV_GENERIC_OBJECT					0

/* Defines the class index into the classNameList.  The order
 * must match vrfyhelp.c: initializeClassNameList()
 */
#define BCV_JAVA_LANG_OBJECT_INDEX					0
#define BCV_JAVA_LANG_STRING_INDEX					1
#define BCV_JAVA_LANG_THROWABLE_INDEX				2
#define BCV_JAVA_LANG_CLASS_INDEX					3
#define BCV_JAVA_LANG_INVOKE_METHOD_TYPE_INDEX		4
#define BCV_JAVA_LANG_INVOKE_METHODHANDLE_INDEX		5

#define BCV_KNOWN_CLASSES							7

#define BCV_OBJECT_OR_ARRAY					0		/* added for completeness */
#define BCV_TAG_BASE_TYPE_OR_TOP			1		/* clear bit means object or array */
#define BCV_TAG_BASE_ARRAY_OR_NULL			2		/* set bit means base type array */
#define BCV_SPECIAL_INIT					4		/* set bit means special init object ("this" for <init>) */
#define BCV_SPECIAL_NEW						8		/* set bit means special new object (PC offset in upper 28 bits) */

#define	BCV_TAG_MASK						(BCV_TAG_BASE_TYPE_OR_TOP | BCV_TAG_BASE_ARRAY_OR_NULL | BCV_SPECIAL_INIT | BCV_SPECIAL_NEW)

#define	BCV_SPECIAL							(BCV_SPECIAL_INIT | BCV_SPECIAL_NEW)
#define	BCV_BASE_OR_SPECIAL					(BCV_TAG_BASE_TYPE_OR_TOP | BCV_SPECIAL_INIT | BCV_SPECIAL_NEW)

#define BCV_BASE_TYPE_INT_BIT				0x020
#define BCV_BASE_TYPE_FLOAT_BIT				0x040
#define BCV_BASE_TYPE_LONG_BIT				0x080
#define BCV_BASE_TYPE_DOUBLE_BIT			0x100
#define BCV_BASE_TYPE_SHORT_BIT				0x200
#define BCV_BASE_TYPE_BYTE_BIT				0x400
#define BCV_BASE_TYPE_CHAR_BIT				0x800
#define BCV_BASE_TYPE_BOOL_BIT				0x1000

#define BCV_BASE_TYPE_MASK 					(BCV_BASE_TYPE_INT_BIT | BCV_BASE_TYPE_FLOAT_BIT | BCV_BASE_TYPE_LONG_BIT | BCV_BASE_TYPE_DOUBLE_BIT | BCV_BASE_TYPE_SHORT_BIT | BCV_BASE_TYPE_BYTE_BIT | BCV_BASE_TYPE_CHAR_BIT | BCV_BASE_TYPE_BOOL_BIT)
#define	BCV_BASE_TYPE_TOP					(BCV_TAG_BASE_TYPE_OR_TOP)						/* used for uninitialized temps and half a long/double */
#define	BCV_BASE_TYPE_NULL					(BCV_TAG_BASE_ARRAY_OR_NULL | BCV_ARITY_MASK)	/* NULL object - arity used to simplify tests */

#define	BCV_WIDE_TYPE_MASK					(BCV_BASE_TYPE_LONG_BIT | BCV_BASE_TYPE_DOUBLE_BIT )

#define BCV_BASE_TYPE_INT					(BCV_TAG_BASE_TYPE_OR_TOP | BCV_BASE_TYPE_INT_BIT)
#define BCV_BASE_TYPE_FLOAT					(BCV_TAG_BASE_TYPE_OR_TOP | BCV_BASE_TYPE_FLOAT_BIT)
#define BCV_BASE_TYPE_DOUBLE				(BCV_TAG_BASE_TYPE_OR_TOP | BCV_BASE_TYPE_DOUBLE_BIT)
#define BCV_BASE_TYPE_LONG					(BCV_TAG_BASE_TYPE_OR_TOP | BCV_BASE_TYPE_LONG_BIT)
#define BCV_BASE_TYPE_SHORT					(BCV_TAG_BASE_TYPE_OR_TOP | BCV_BASE_TYPE_SHORT_BIT)
#define BCV_BASE_TYPE_BYTE					(BCV_TAG_BASE_TYPE_OR_TOP | BCV_BASE_TYPE_BYTE_BIT)
#define BCV_BASE_TYPE_CHAR					(BCV_TAG_BASE_TYPE_OR_TOP | BCV_BASE_TYPE_CHAR_BIT)
#define BCV_BASE_TYPE_BOOL					(BCV_TAG_BASE_TYPE_OR_TOP | BCV_BASE_TYPE_BOOL_BIT)

#define BCV_BASE_ARRAY_TYPE_INT				(BCV_TAG_BASE_ARRAY_OR_NULL | BCV_BASE_TYPE_INT_BIT)
#define BCV_BASE_ARRAY_TYPE_FLOAT			(BCV_TAG_BASE_ARRAY_OR_NULL | BCV_BASE_TYPE_FLOAT_BIT)
#define BCV_BASE_ARRAY_TYPE_DOUBLE			(BCV_TAG_BASE_ARRAY_OR_NULL | BCV_BASE_TYPE_DOUBLE_BIT)
#define BCV_BASE_ARRAY_TYPE_LONG			(BCV_TAG_BASE_ARRAY_OR_NULL | BCV_BASE_TYPE_LONG_BIT)
#define BCV_BASE_ARRAY_TYPE_SHORT			(BCV_TAG_BASE_ARRAY_OR_NULL | BCV_BASE_TYPE_SHORT_BIT)
#define BCV_BASE_ARRAY_TYPE_BYTE			(BCV_TAG_BASE_ARRAY_OR_NULL | BCV_BASE_TYPE_BYTE_BIT)
#define BCV_BASE_ARRAY_TYPE_CHAR			(BCV_TAG_BASE_ARRAY_OR_NULL | BCV_BASE_TYPE_CHAR_BIT)
#define BCV_BASE_ARRAY_TYPE_BOOL			(BCV_TAG_BASE_ARRAY_OR_NULL | BCV_BASE_TYPE_BOOL_BIT)

#define BCV_INDEX_FROM_TYPE(type) (((type) & BCV_CLASS_INDEX_MASK) >> BCV_CLASS_INDEX_SHIFT)
#define BCV_ARITY_FROM_TYPE(type) (((type) & BCV_ARITY_MASK) >> BCV_ARITY_SHIFT)

#endif /*bytecodewalk_h*/
