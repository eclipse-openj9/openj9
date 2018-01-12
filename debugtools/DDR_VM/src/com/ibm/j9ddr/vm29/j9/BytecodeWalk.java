/*******************************************************************************
 * Copyright (c) 2009, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9;

/**
 * Constants from oti/bytecodewalk.h
 * 
 * @author andhall
 *
 */
public class BytecodeWalk
{

	/* Bytecode groupings for verification */ 
	public static final int RTV_NOP = 0x01;
	public static final int RTV_PUSH_CONSTANT = 0x02;
	public static final int RTV_PUSH_CONSTANT_POOL_ITEM = 0x03;
	public static final int RTV_LOAD_TEMP_PUSH = 0x04;
	public static final int RTV_ARRAY_FETCH_PUSH = 0x05;
	public static final int RTV_POP_STORE_TEMP = 0x06;
	public static final int RTV_ARRAY_STORE = 0x07;
	public static final int RTV_INCREMENT = 0x08;
	public static final int RTV_POP_2_PUSH = 0x09;
	public static final int RTV_POP_X_PUSH_X = 0x0A;
	public static final int RTV_POP_X_PUSH_Y = 0x0B;
										/* gap */
	public static final int RTV_BRANCH = 0x0E;
	public static final int RTV_RETURN = 0x0F;
	public static final int RTV_STATIC_FIELD_ACCESS = 0x10;
	public static final int RTV_SEND = 0x11;
	public static final int RTV_PUSH_NEW = 0x12;
	public static final int RTV_MISC = 0x13;
	public static final int RTV_WIDE_LOAD_TEMP_PUSH = 0x14;
	public static final int RTV_WIDE_POP_STORE_TEMP = 0x15;
	public static final int RTV_POP_2_PUSH_INT = 0x16;
	public static final int	RTV_UNIMPLEMENTED = 0x17;
	public static final int RTV_BYTECODE_POP = 0x18;
	public static final int RTV_BYTECODE_POP2 = 0x19;
	public static final int RTV_BYTECODE_DUP = 0x1A;
	public static final int RTV_BYTECODE_DUPX1 = 0x1B;
	public static final int RTV_BYTECODE_DUPX2 = 0x1C;
	public static final int RTV_BYTECODE_DUP2 = 0x1D;
	public static final int RTV_BYTECODE_DUP2X1 = 0x1E;
	public static final int RTV_BYTECODE_DUP2X2 = 0x1f;
	public static final int RTV_BYTECODE_SWAP = 0x20;

	/* 

 = 32bit; type => [8 bits arity] [ 20 bits class index] [4 tag bits]

	tag bits:
		special (new / init / ret)
		base / object
		base type array / regular object, array
		null

	base types: (in the 20bit class index field)
		int
		float
		long
		double
		char
		short
		byte

	*/

	public static final int BCV_ARITY_MASK = 0xFF000000;
	public static final int BCV_ARITY_SHIFT = 24;
	public static final int BCV_CLASS_INDEX_MASK = 0x00FFFFF0;
	public static final int BCV_CLASS_INDEX_SHIFT = 4;

	public static final int BCV_GENERIC_OBJECT = 0;

	public static final int BCV_JAVA_LANG_OBJECT_INDEX = 0;
	public static final int BCV_JAVA_LANG_STRING_INDEX = 1;
	public static final int BCV_JAVA_LANG_THROWABLE_INDEX = 2; 
	public static final int BCV_JAVA_LANG_CLASS_INDEX = 3; 

	public static final int BCV_KNOWN_CLASSES = 4;

	public static final int BCV_OBJECT_OR_ARRAY = 0;		/* added for completeness */
	public static final int BCV_TAG_BASE_TYPE_OR_TOP = 1;		/* clear bit means object or array */
	public static final int BCV_TAG_BASE_ARRAY_OR_NULL = 2;		/* set bit means base type array */
	public static final int BCV_SPECIAL_INIT = 4;		/* set bit means special init object ("this" for <init>) */
	public static final int BCV_SPECIAL_NEW = 8;		/* set bit means special new object (PC offset in upper 28 bits) */

	public static final int	BCV_TAG_MASK = (BCV_TAG_BASE_TYPE_OR_TOP | BCV_TAG_BASE_ARRAY_OR_NULL | BCV_SPECIAL_INIT | BCV_SPECIAL_NEW);

	public static final int	BCV_SPECIAL = (BCV_SPECIAL_INIT | BCV_SPECIAL_NEW);
	public static final int	BCV_BASE_OR_SPECIAL = (BCV_TAG_BASE_TYPE_OR_TOP | BCV_SPECIAL_INIT | BCV_SPECIAL_NEW);

	public static final int BCV_BASE_TYPE_INT_BIT = 0x010;
	public static final int BCV_BASE_TYPE_FLOAT_BIT = 0x020;
	public static final int BCV_BASE_TYPE_LONG_BIT = 0x040;
	public static final int BCV_BASE_TYPE_DOUBLE_BIT = 0x080;
	public static final int BCV_BASE_TYPE_SHORT_BIT = 0x100;
	public static final int BCV_BASE_TYPE_BYTE_BIT = 0x200;
	public static final int BCV_BASE_TYPE_CHAR_BIT = 0x400;

	public static final int	BCV_BASE_TYPE_TOP = (BCV_TAG_BASE_TYPE_OR_TOP);						/* used for uninitialized temps and half a long/double */
	public static final int	BCV_BASE_TYPE_NULL = (BCV_TAG_BASE_ARRAY_OR_NULL | BCV_ARITY_MASK);	/* NULL object - arity used to simplify tests */

	public static final int	BCV_WIDE_TYPE_MASK = (BCV_BASE_TYPE_LONG_BIT | BCV_BASE_TYPE_DOUBLE_BIT );

	public static final int BCV_BASE_TYPE_INT = (BCV_TAG_BASE_TYPE_OR_TOP | BCV_BASE_TYPE_INT_BIT);
	public static final int BCV_BASE_TYPE_FLOAT = (BCV_TAG_BASE_TYPE_OR_TOP | BCV_BASE_TYPE_FLOAT_BIT);
	public static final int BCV_BASE_TYPE_DOUBLE = (BCV_TAG_BASE_TYPE_OR_TOP | BCV_BASE_TYPE_DOUBLE_BIT);
	public static final int BCV_BASE_TYPE_LONG = (BCV_TAG_BASE_TYPE_OR_TOP | BCV_BASE_TYPE_LONG_BIT);
	public static final int BCV_BASE_TYPE_SHORT = (BCV_TAG_BASE_TYPE_OR_TOP | BCV_BASE_TYPE_SHORT_BIT);
	public static final int BCV_BASE_TYPE_BYTE = (BCV_TAG_BASE_TYPE_OR_TOP | BCV_BASE_TYPE_BYTE_BIT);
	public static final int BCV_BASE_TYPE_CHAR = (BCV_TAG_BASE_TYPE_OR_TOP | BCV_BASE_TYPE_CHAR_BIT);

	public static final int BCV_BASE_ARRAY_TYPE_INT = (BCV_TAG_BASE_ARRAY_OR_NULL | BCV_BASE_TYPE_INT_BIT);
	public static final int BCV_BASE_ARRAY_TYPE_FLOAT = (BCV_TAG_BASE_ARRAY_OR_NULL | BCV_BASE_TYPE_FLOAT_BIT);
	public static final int BCV_BASE_ARRAY_TYPE_DOUBLE = (BCV_TAG_BASE_ARRAY_OR_NULL | BCV_BASE_TYPE_DOUBLE_BIT);
	public static final int BCV_BASE_ARRAY_TYPE_LONG = (BCV_TAG_BASE_ARRAY_OR_NULL | BCV_BASE_TYPE_LONG_BIT);
	public static final int BCV_BASE_ARRAY_TYPE_SHORT = (BCV_TAG_BASE_ARRAY_OR_NULL | BCV_BASE_TYPE_SHORT_BIT);
	public static final int BCV_BASE_ARRAY_TYPE_BYTE = (BCV_TAG_BASE_ARRAY_OR_NULL | BCV_BASE_TYPE_BYTE_BIT);
	public static final int BCV_BASE_ARRAY_TYPE_CHAR = (BCV_TAG_BASE_ARRAY_OR_NULL | BCV_BASE_TYPE_CHAR_BIT);

}
