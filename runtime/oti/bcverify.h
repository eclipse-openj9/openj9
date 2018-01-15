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

#ifndef bcverify_h
#define bcverify_h

/* @ddr_namespace: default */
#include "bytecodewalk.h"
#include "j9comp.h"

#define BRANCH_INDEX_SHIFT			4		/* number of bits defined below */
#define	BRANCH_TARGET				1		/* at least one instruction branches here */
#define	BRANCH_EXCEPTION_START		2		/* start of exception range */
#define	BRANCH_ON_UNWALKED_QUEUE	4		/* queued for first walk */
#define	BRANCH_ON_REWALK_QUEUE		8		/* queued for repeat walk */

#define BCV_FIRST_STACK() \
	((J9BranchTargetStack *) (verifyData->stackMaps))

#define BCV_NEXT_STACK(thisStack) \
	((J9BranchTargetStack *) (((U_8 *) (thisStack)) + (verifyData->stackSize)))

#define BCV_INDEX_STACK(stackCount) \
	((J9BranchTargetStack *) (((U_8 *) BCV_FIRST_STACK()) + (verifyData->stackSize * (stackCount))))

#define BCV_STACK_INDEX(thisStack) \
	((((U_8 *) (thisStack)) - ((U_8 *) BCV_FIRST_STACK())) / (verifyData->stackSize))

typedef struct J9BranchTargetStack {
	UDATA pc;
	UDATA uninitializedThis;
    IDATA stackBaseIndex;
    IDATA stackTopIndex;
	UDATA stackElements[1];
} J9BranchTargetStack;

#define	BCV_TARGET_STACK_HEADER_UDATA_SIZE			4
#define	BCV_STACK_OVERFLOW_BUFFER_UDATA_SIZE		2

typedef struct J9BCVAlloc {
		struct J9BCVAlloc *prev;
		UDATA data[1];
} J9BCVAlloc;

#define BUILD_VERIFY_ERROR(module, code ) 	\
	verifyData->errorPC = (UDATA) start;	\
	verifyData->errorModule = module; \
	verifyData->errorCode = code; 

#define RESET_VERIFY_ERROR(verifyData)	\
		(verifyData)->errorPC = (UDATA)-1;	\
		(verifyData)->errorModule = (UDATA)-1; \
		(verifyData)->errorCode = (UDATA)-1; \
		(verifyData)->errorTargetType = (UDATA)-1; \
		(verifyData)->errorTempData = (UDATA)-1; \
		(verifyData)->errorDetailCode = 0; \
		(verifyData)->errorArgumentIndex = (U_16)-1; \
	    (verifyData)->errorTargetFrameIndex = -1; \
	    (verifyData)->errorCurrentFramePosition = 0; \
		(verifyData)->errorClassString = NULL; \
		(verifyData)->errorMethodString = NULL; \
		(verifyData)->errorSignatureString = NULL;

#define CHECK_STACK_UNDERFLOW \
	if( stackTop < stackBase ) { \
		errorType = J9NLS_BCV_ERR_STACK_UNDERFLOW__ID;	\
		goto _verifyError; \
	}

#define DROP( x )	\
	stackTop -= x;	\
	CHECK_STACK_UNDERFLOW

#define POP	\
	*(--stackTop); \
	CHECK_STACK_UNDERFLOW

#define PUSH( t ) \
	*stackTop = (UDATA) (t); \
	stackTop++;

#define PUSH_PAIR( tempType ) \
	PUSH(tempType); \
	PUSH(BCV_BASE_TYPE_TOP);

#define PUSH_INTEGER_CONSTANT	\
	PUSH(BCV_BASE_TYPE_INT);

#define PUSH_LONG_CONSTANT \
	PUSH_PAIR(BCV_BASE_TYPE_LONG);

#define PUSH_FLOAT_CONSTANT	\
	PUSH(BCV_BASE_TYPE_FLOAT);

#define PUSH_DOUBLE_CONSTANT \
	PUSH_PAIR(BCV_BASE_TYPE_DOUBLE);

#define PUSH_TEMP_OBJECT( tempIndex ) \
	inconsistentStack |= (temps[tempIndex] & BCV_TAG_BASE_TYPE_OR_TOP); \
	PUSH( temps[tempIndex] );

#define CHECK_TEMP( tempIndex , tempType ) \
	inconsistentStack |= ( temps[ ( tempIndex ) ] != tempType );

#define CHECK_TEMP_INTEGER( tempIndex ) \
	CHECK_TEMP( tempIndex , (UDATA) BCV_BASE_TYPE_INT );

#define CHECK_TEMP_FLOAT( tempIndex ) \
	CHECK_TEMP( tempIndex , (UDATA) BCV_BASE_TYPE_FLOAT );

#define CHECK_TEMP_PAIR( tempIndex, tempType ) \
	CHECK_TEMP( tempIndex , (UDATA) (tempType) ); \
	CHECK_TEMP( ( tempIndex + 1 ) , (UDATA) BCV_BASE_TYPE_TOP );

#define CHECK_TEMP_LONG( tempIndex ) \
	CHECK_TEMP_PAIR( tempIndex , (UDATA) BCV_BASE_TYPE_LONG );

#define CHECK_TEMP_DOUBLE( tempIndex ) \
	CHECK_TEMP_PAIR( tempIndex , (UDATA) BCV_BASE_TYPE_DOUBLE );

/* The macro below is used for baload and bastore to deal with the case of byte/boolean array type.
 * The reason for that is as follows:
 * 1) The existing implementation prevents us from inserting a boolean array type in J9JavaBytecodeArrayTypeTable
 *    (in which the expected type is determined by J9JavaBytecodeArrayTypeTable[bc - baload or bastore]).
 * 2) For baload and bastore, there is no way to determine whether the target type (popped out from the stack)
 *    is byte array (calculated via J9JavaBytecodeArrayTypeTable) or boolean array (not in J9JavaBytecodeArrayTypeTable).
 *
 * So we need to ensure the consistency check addresses the cases of boolean array and byte array for baload/bastore.
 * In such case, the macro can be expended to:
 * if (baload or bastore == bc),
 * then inconsistentStack |= (BCV_BASE_ARRAY_TYPE_BYTE != arrayType) && (BCV_BASE_ARRAY_TYPE_BOOL != arrayType));
 */
#define CHECK_BOOL_ARRAY(opCode, bc, operandType) \
	if (opCode == bc) { \
		inconsistentStack |= ((operandType != arrayType) && (BCV_BASE_ARRAY_TYPE_BOOL != arrayType)); \
	} else { \
		inconsistentStack |= (operandType != arrayType); \
	}

#define	POP_TOS_TYPE( foundType, expectedType ) \
	foundType = POP; \
	if (expectedType & BCV_WIDE_TYPE_MASK) { \
		inconsistentStack |= (foundType != BCV_BASE_TYPE_TOP); \
		foundType = POP; \
	}

#define	POP_TOS_TYPE_EQUAL( foundType, expectedType ) \
	POP_TOS_TYPE(foundType, expectedType); \
	inconsistentStack2 |= (foundType != expectedType);
	
#define POP_TOS( baseType )	\
	{	\
		UDATA popTemp = POP;	\
		inconsistentStack |= (popTemp != (UDATA) (baseType)); \
	}

#define POP_TOS_INTEGER	\
	POP_TOS( BCV_BASE_TYPE_INT );

#define POP_TOS_FLOAT		\
	POP_TOS( BCV_BASE_TYPE_FLOAT );

#define POP_TOS_2( baseType )		\
	POP_TOS( BCV_BASE_TYPE_TOP ); \
	POP_TOS( baseType );

#define POP_TOS_LONG		\
	POP_TOS_2( BCV_BASE_TYPE_LONG );

#define POP_TOS_DOUBLE		\
	POP_TOS_2( BCV_BASE_TYPE_DOUBLE );

#define POP_TOS_SINGLE( temp1 )	\
	temp1 = POP; \
	inconsistentStack |= (temp1 == BCV_BASE_TYPE_TOP);

#define IS_WIDE_TYPE(temp)	\
	(J9_ARE_ALL_BITS_SET((temp), BCV_BASE_TYPE_LONG) | J9_ARE_ALL_BITS_SET((temp), BCV_BASE_TYPE_DOUBLE))

#define TYPE_PAIR_CHECK(temp1, temp2)	\
	{ \
		UDATA isWideType = IS_WIDE_TYPE(temp2); \
		inconsistentStack |= IS_WIDE_TYPE(temp1); \
		inconsistentStack |= ((BCV_BASE_TYPE_TOP == (temp1)) ? (!isWideType) : isWideType); \
	}

/* To ensure these two types on the current stack are 2 singles or a correctly ordered wide pair(long/double),
 * there are 3 cases that need to be covered:
 * 1) the second type (temp2) under the first type (temp1) must not be 'top' in any case.
 * 2) the first type (temp1) must not be long/double in any case.
 * 3) the type underneath (temp2) is long/double only when the first type (temp1) is 'top';
 *    otherwise, the type underneath (temp2) must not be long/double.
 * Note: if the first type (temp1) is not 'top', these two types could be other primitives
 * (short, int, char, byte, boolean, float, etc) or object references.
 */
#define POP_TOS_PAIR(temp1, temp2)	\
	temp1 = POP; \
	POP_TOS_SINGLE(temp2); \
	TYPE_PAIR_CHECK(temp1, temp2);

#define POP_TOS_BASE_ARRAY( basetype ) \
	temp1 = POP; \
	inconsistentStack |= ( (temp1 != (basetype | BCV_TAG_BASE_ARRAY_OR_NULL) ) && (temp1 !=  BCV_BASE_TYPE_NULL) );

#define POP_TOS_INTEGER_ARRAY		\
	POP_TOS_BASE_ARRAY( BCV_BASE_TYPE_INT_BIT );

#define POP_TOS_LONG_ARRAY		\
	POP_TOS_BASE_ARRAY( BCV_BASE_TYPE_LONG_BIT );

#define POP_TOS_FLOAT_ARRAY		\
	POP_TOS_BASE_ARRAY( BCV_BASE_TYPE_FLOAT_BIT );

#define POP_TOS_DOUBLE_ARRAY		\
	POP_TOS_BASE_ARRAY( BCV_BASE_TYPE_DOUBLE_BIT );

#define POP_TOS_BYTE_ARRAY		\
	POP_TOS_BASE_ARRAY( BCV_BASE_TYPE_BYTE_BIT );

#define POP_TOS_CHAR_ARRAY		\
	POP_TOS_BASE_ARRAY( BCV_BASE_TYPE_CHAR_BIT );

#define POP_TOS_SHORT_ARRAY		\
	POP_TOS_BASE_ARRAY( BCV_BASE_TYPE_SHORT_BIT );

#define POP_TOS_OBJECT_ARRAY( t )		\
	t = POP; \
	inconsistentStack |= ( (t & BCV_BASE_OR_SPECIAL) || ((t & BCV_ARITY_MASK) == 0) );

#define POP_TOS_ARRAY( t )		\
	t = POP; \
	if ((t & BCV_TAG_BASE_ARRAY_OR_NULL) == 0) { \
		inconsistentStack |= ( (t & BCV_BASE_OR_SPECIAL) || ((t & BCV_ARITY_MASK) == 0) ); \
	}

#define POP_TOS_OBJECT_IN_MONITOR( t )			\
	t = POP; \
	inconsistentStack |= t & BCV_TAG_BASE_TYPE_OR_TOP;

#define POP_TOS_OBJECT( t )			\
	t = POP; \
	inconsistentStack |= t & BCV_BASE_OR_SPECIAL;

#define POP_TOS_OBJECT_OR_NEW( t )			\
	t = POP; \
	inconsistentStack |= t & (BCV_TAG_BASE_TYPE_OR_TOP | BCV_SPECIAL_INIT); 

/* pre-index clearing for doubles and longs being partially overwritten */
/* performs zero check that could be optimized away by adding an extra -1 local */

#define STORE_TEMP( tempIndex, tempType )	\
	temps[ ( tempIndex ) ] = ( tempType ); \
	if (tempIndex && (temps[ ( tempIndex - 1 ) ] & BCV_WIDE_TYPE_MASK) && (temps[ ( tempIndex - 1 ) ] & BCV_TAG_BASE_TYPE_OR_TOP)) { \
		temps[ ( tempIndex - 1 ) ] = BCV_BASE_TYPE_TOP; \
	}

#define STORE_TEMP_OBJECT( tempIndex, tempType )	\
	STORE_TEMP( tempIndex, tempType );

#define STORE_TEMP_INTEGER( tempIndex ) \
	STORE_TEMP( tempIndex, (UDATA) (BCV_BASE_TYPE_INT) );

#define STORE_TEMP_FLOAT( tempIndex ) \
	STORE_TEMP( tempIndex, (UDATA) (BCV_BASE_TYPE_FLOAT) );

/* direct write of the TOP avoids the pre-index check */

#define STORE_TEMP_PAIR( tempIndex, tempType )  \
	STORE_TEMP( tempIndex, tempType ); \
	temps[ ( tempIndex + 1 ) ] = (BCV_BASE_TYPE_TOP);

#define STORE_TEMP_LONG( tempIndex )  \
	STORE_TEMP_PAIR( tempIndex, (UDATA) (BCV_BASE_TYPE_LONG) );

#define STORE_TEMP_DOUBLE( tempIndex ) \
	STORE_TEMP_PAIR( tempIndex, (UDATA) (BCV_BASE_TYPE_DOUBLE) );

#define RELOAD_LIVESTACK \
	stackBase =  &(liveStack->stackElements[liveStack->stackBaseIndex]); \
	stackTop = &(liveStack->stackElements[liveStack->stackTopIndex]); \
	temps = liveStack->stackElements;

#define RELOAD_STACKTOP(liveStack) \
	&(liveStack->stackElements[liveStack->stackTopIndex])

#define RELOAD_STACKBASE(liveStack) \
	&(liveStack->stackElements[liveStack->stackBaseIndex])

#define SAVE_STACKTOP(liveStack, stackTop) \
	liveStack->stackTopIndex = (IDATA) (stackTop - liveStack->stackElements)

#endif     /* bcverify_h */


