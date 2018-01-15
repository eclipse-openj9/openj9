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

#ifndef cfreader_h
#define cfreader_h 

#include "cfr.h"
#include "cfrerrnls.h"
#include <string.h>
#include <stdlib.h>

/* Flags for j9bcutil_readClassFileBytes. */
#define CFR_StripDebugAttributes 0x100
#define CFR_StaticVerification 0x200
#define CFR_LeaveJSRs 0x400
#define CFR_Xfuture 0x800

/* flags to indicate what has been checked */
#define CFR_FLAGS1_ValidFieldSignature  4
#define CFR_FLAGS1_ValidMethodSignature  8

#define CFR_MAJOR_VERSION_REQUIRING_STACKMAPS 51

/* Flags for StackMapTable frame types */
#define	CFR_STACKMAP_SAME														(U_8) 0
#define	CFR_STACKMAP_SAME_LOCALS_1_STACK						(U_8) 64
#define	CFR_STACKMAP_SAME_LOCALS_1_STACK_END				(U_8) 128
#define	CFR_STACKMAP_SAME_LOCALS_1_STACK_EXTENDED		(U_8) 247
#define	CFR_STACKMAP_CHOP_3													(U_8) 248
#define	CFR_STACKMAP_CHOP_1													(U_8) 250
#define	CFR_STACKMAP_SAME_EXTENDED									(U_8) 251
#define	CFR_STACKMAP_CHOP_BASE											(U_8) 251
#define	CFR_STACKMAP_APPEND_BASE										(U_8) 251
#define	CFR_STACKMAP_APPEND_1												(U_8) 252
#define	CFR_STACKMAP_APPEND_3												(U_8) 254
#define	CFR_STACKMAP_FULL														(U_8) 255

#define CFR_STACKMAP_TYPE_TOP							0x00
#define CFR_STACKMAP_TYPE_INT							0x01
#define CFR_STACKMAP_TYPE_FLOAT 					0x02
#define CFR_STACKMAP_TYPE_DOUBLE					0x03
#define CFR_STACKMAP_TYPE_LONG 						0x04
#define CFR_STACKMAP_TYPE_NULL						0x05

#define CFR_STACKMAP_TYPE_INIT_OBJECT			0x06	/* "this" of an <init> method */
#define CFR_STACKMAP_TYPE_OBJECT					0x07
#define CFR_STACKMAP_TYPE_NEW_OBJECT			0x08	/* result of a new - includes a method bytecode offset */

#define	CFR_STACKMAP_TYPE_BASE_ARRAY_BIT	0x08

#define	CFR_STACKMAP_TYPE_INT_ARRAY				0x09
#define	CFR_STACKMAP_TYPE_FLOAT_ARRAY			0x0A
#define	CFR_STACKMAP_TYPE_DOUBLE_ARRAY			0x0B
#define	CFR_STACKMAP_TYPE_LONG_ARRAY			0x0C
#define	CFR_STACKMAP_TYPE_SHORT_ARRAY			0x0D
#define	CFR_STACKMAP_TYPE_BYTE_ARRAY			0x0E
#define	CFR_STACKMAP_TYPE_CHAR_ARRAY			0x0F
#define	CFR_STACKMAP_TYPE_BOOL_ARRAY			0x10

#define	CFR_METHOD_NAME_INIT	1
#define	CFR_METHOD_NAME_CLINIT	2
#define	CFR_METHOD_NAME_INVALID	-1

/* Macros. */
/* This heuristic is based actual experimentation.  It might have changed, though, as the ROM format changed. NOTE: There are 4 buckets. */
#define ESTIMATE_SIZE(dataSize) ((dataSize < 1000)?dataSize + 1000:(((dataSize < 10000)||(dataSize > 50000))?dataSize + 5000:dataSize + 20000))

#define CHECK_EOF(nextRead) \
	if((UDATA)(nextRead) > (UDATA)(dataEnd - index)) \
	{ \
		errorCode = (U_32)J9NLS_CFR_ERR_UNEXPECTED_EOF__ID; \
		offset = (U_32)(dataEnd - data); \
		goto _errorFound; \
	}

#define MIN_SIZEOF(typea, typeb) \
	(sizeof(typea) > sizeof(typeb) ? sizeof(typeb) : sizeof(typea))

#define J9_CFR_ALIGN(ptr, type) \
	(ptr = (void*)((char*)ptr + MIN_SIZEOF(UDATA, type) - 1 - \
		(((UDATA)ptr - 1) % MIN_SIZEOF(UDATA, type))))

#define ALLOC(result, type) ( \
	(result = (type*)J9_CFR_ALIGN(freePointer, type)), \
	(freePointer += sizeof(type)), \
	(freePointer < segmentEnd))

#define ALLOC_CAST(result, type, cast) ( \
	(result = (cast*)J9_CFR_ALIGN(freePointer, cast)), \
	(freePointer += sizeof(type)), \
	(freePointer < segmentEnd))

#define ALLOC_ARRAY(result, size, type) ( \
	(result = (type*)J9_CFR_ALIGN(freePointer, type)), \
	(freePointer += size * sizeof(type)), \
	(freePointer < segmentEnd))


#define NEXT_U8(value, index) (value = *((index)++))
#define NEXT_U16(value, index) ((value = ((U_16)(index)[0] << 8) | (U_16)(index)[1]), index += 2, value)
#define NEXT_I16(value, index) ((value = (I_16) (((U_16)(index)[0] << 8) | (U_16)(index)[1])), index += 2, value)
#define NEXT_U32(value, index) ((value = ((U_32)(index)[0] << 24) | ((U_32)(index)[1] << 16) | ((U_32)(index)[2] << 8) | (U_32)(index)[3]), index += 4, value)
#define NEXT_I32(value, index) ((value = (I_32) (((U_32)(index)[0] << 24) | ((U_32)(index)[1] << 16) | ((U_32)(index)[2] << 8) | (U_32)(index)[3])), index += 4, value)

#define NEXT_U8_ENDIAN(bigEndian, value, index) 	value = *(index++)

#define NEXT_U16_ENDIAN(bigEndian, value, index) ( \
	(value) = !(bigEndian)						\
		?	( ( ((U_16) (index)[0])			)	\
			| ( ((U_16) (index)[1]) << 8)	\
			)															\
		:	( ( ((U_16) (index)[0]) << 8)	\
			| ( ((U_16) (index)[1])			)	\
			), 														\
	((index) += 2)										\
	)

#define NEXT_I16_ENDIAN(bigEndian, value, index) ( \
	(value) = !(bigEndian)						\
		?	(I_16)												\
			( ( ((U_16) (index)[0])			)	\
			| ( ((U_16) (index)[1]) << 8)	\
			)															\
		:	(I_16)												\
			( ( ((U_16) (index)[0]) << 8)	\
			| ( ((U_16) (index)[1])			)	\
			), 														\
	((index) += 2)										\
	)

#define NEXT_U32_ENDIAN(bigEndian, value, index) ( \
	(value) = !(bigEndian)							\
		?	( ( ((U_32) (index)[0])			 )	\
			| ( ((U_32) (index)[1]) << 8 )	\
			| ( ((U_32) (index)[2]) << 16)	\
			| ( ((U_32) (index)[3]) << 24)	\
			) 															\
		:	( ( ((U_32) (index)[0]) << 24)	\
			| ( ((U_32) (index)[1]) << 16)	\
			| ( ((U_32) (index)[2]) << 8 )	\
			| ( ((U_32) (index)[3])			 )	\
			),															\
	((index) += 4)											\
	)

#define NEXT_I32_ENDIAN(bigEndian, value, index) ( \
	(value) = !(bigEndian)							\
		?	(I_32)													\
			( ( ((U_32) (index)[0])			 )	\
			| ( ((U_32) (index)[1]) << 8 )	\
			| ( ((U_32) (index)[2]) << 16)	\
			| ( ((U_32) (index)[3]) << 24)	\
			) 															\
		:	(I_32)													\
			( ( ((U_32) (index)[0]) << 24)	\
			| ( ((U_32) (index)[1]) << 16)	\
			| ( ((U_32) (index)[2]) << 8 )	\
			| ( ((U_32) (index)[3])			 )	\
			),															\
	((index) += 4)											\
	)

#ifdef J9VM_ENV_LITTLE_ENDIAN
#define SWAPPING_ENDIAN(flags) ((flags) & BCT_BigEndianOutput)
#else /* BIG_ENDIAN */
#define SWAPPING_ENDIAN(flags) ((flags) & BCT_LittleEndianOutput)
#endif	

#endif     /* cfreader_h */

