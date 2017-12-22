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

#ifndef _LINEARSLOTWALKER_H
#define _LINEARSLOTWALKER_H

#include "j9.h"
#include "pool_api.h"
#include "hashtable_api.h"
 

/* Frame type description structures */

typedef struct J9SWFTJit {
	UDATA				*decompilationStack;
	UDATA				*jitInfo;
	UDATA				bytecodeIndex;
	UDATA				inlineDepth;
	UDATA				pcOffset;
/*
	UDATA				*stackMap;
	UDATA				stackMapSlots;
	UDATA				parmBaseOffset;
	UDATA				parmSlots;
	UDATA				localBaseOffset;
*/	
} J9SWFTJit;

typedef struct J9SWFTBytecode {
	UDATA				bytecodeIndex;
} J9SWFTBytecode;

typedef struct J9SWFTResolve {
	UDATA				type;
} J9SWFTResolve;

typedef struct J9SWFTJniCallin {
	UDATA				els;
} J9SWFTJniCallin;

typedef union J9SWFrameTypeDescriptor {
	J9SWFTJit			jit;
	J9SWFTBytecode		bytecodeIndex;
	J9SWFTResolve		resolve;
	J9SWFTJniCallin		jniCallIn;
} J9SWFrameTypeDescriptor;



/** J9SWFrame describes a frame and serves as a linked list link */

typedef struct J9SWFrame {
	UDATA		number;
	UDATA		type;							/* Frame type, e.g Bytecode, JIT, JIT Resolve .. */
	char		*name;							/* fully differentiated frame name (based on frame flags etc) */
	J9Method	*method;
	UDATA		*topSlot;						/* Top slot of the frame, ie SP for the first frame */
	UDATA		*bottomSlot;					/* Bottom slot of the frame */
	UDATA		argCount;						/* Number of method arguments */

	J9SWFrameTypeDescriptor		ftd;			/* Union describing bits and pieces that differ between various frame types */
	UDATA		*cp;
	UDATA		*pc;
	UDATA		frameFlags;
	UDATA		literals;

	struct J9SWFrame *linkNext;
	struct J9SWFrame *linkPrevious;
} J9SWFrame;


/** J9SWSlot describes the _contents_ of a single slot */

typedef struct J9SWSlot {
	UDATA		data;							/* Slot value */
	UDATA		type;							/* Slot type as annotated by the stack walker */
	char		*name;							/* Verbose slot description */
} J9SWSlot;


/** Free text slot annotation. A single slot can have more then one annotations. 
    E.g. it can be both arg0EA and BP. Note that it is not ment to describe the slot contents */

typedef struct J9SWSlotAnnotation {
	IDATA		slotIndex;
	char		*description;
} J9SWSlotAnnotation;


/** Describes the Linear Slot Walker, hangs off J9StackWalkState (walkState->linearSlotWalker) */

typedef struct J9SlotWalker {
	J9SWFrame		*frameList;
	J9HashTable		*slotAnnotations;
	J9Pool			*annotationPool;
	J9Pool			*stringPool;
	J9SWSlot		*slots;

	char			*stringSlab;			/* string slab allocated via stringPool */
	char			*stringFree;			/* pointer to the next free string in the stringSlab */

	J9SWFrame		*currentFrame;
	UDATA			frameCount;
	UDATA			*sp;					/* points at the top slot of the stack */
	UDATA			*stackBottom;			/* points at the bottom slot of the stack */
} J9SlotWalker;



#define SW_ERROR_NONE						 0
#define SW_ERROR_HASHTABLE_NOMEM			-1
#define SW_ERROR_ANNOTATION_POOL_NOMEM		-2
#define SW_ERROR_STRING_POOL_NOMEM			-3
#define SW_ERROR_NOMEM						-4

#define LSW_STRING_MAX						1024
#define LSW_STRING_POOL_MAX					4096


#define LSW_FRAME_TYPE_NATIVE_METHOD		J9SF_FRAME_TYPE_NATIVE_METHOD		
#define LSW_FRAME_TYPE_JNI_NATIVE_METHOD	J9SF_FRAME_TYPE_JNI_NATIVE_METHOD	
#define LSW_FRAME_TYPE_JIT_JNI_CALLOUT		J9SF_FRAME_TYPE_JIT_JNI_CALLOUT		
#define LSW_FRAME_TYPE_METHOD				J9SF_FRAME_TYPE_METHOD				
#define LSW_FRAME_TYPE_JIT_RESOLVE			J9SF_FRAME_TYPE_JIT_RESOLVE			
#define LSW_FRAME_TYPE_END_OF_STACK			J9SF_FRAME_TYPE_END_OF_STACK		
#define LSW_FRAME_TYPE_GENERIC_SPECIAL		J9SF_FRAME_TYPE_GENERIC_SPECIAL
#define LSW_FRAME_TYPE_METHODTYPE			J9SF_FRAME_TYPE_METHODTYPE
#define LSW_FRAME_TYPE_BYTECODE				10
#define LSW_FRAME_TYPE_JNI_CALL_IN			11
#define LSW_FRAME_TYPE_JIT					12
#define LSW_FRAME_TYPE_JIT_INLINE			13


#define LSW_TYPE_FRAME_TYPE					1
#define LSW_TYPE_BP							2
#define LSW_TYPE_FRAME_BOTTOM				3
#define LSW_TYPE_UNWIND_SP					4
#define LSW_TYPE_FRAME_NAME					5
#define LSW_TYPE_FIXED_SLOTS				6
#define LSW_TYPE_METHOD						7
#define LSW_TYPE_ARG_COUNT					8
#define LSW_TYPE_JIT_BP						9
#define LSW_TYPE_O_SLOT						10
#define LSW_TYPE_I_SLOT						11	
#define LSW_TYPE_JIT_REG_SLOT				12
#define LSW_TYPE_F_SLOT						13
#define LSW_TYPE_FLAGS						14
#define LSW_TYPE_ADDRESS					15
#define LSW_TYPE_RESOLVE_FRAME_TYPE			16
#define LSW_TYPE_JIT_FRAME_INFO				17
#define LSW_TYPE_FRAME_INFO					18
#define LSW_TYPE_ELS						19
#define LSW_TYPE_INDIRECT_O_SLOT			20
#define LSW_TYPE_METHODTYPE					21
#define LSW_TYPE_DESCRIPTION_INT_COUNT		22

#define LSW_TYPE_UNKNOWN					100

IDATA lswInitialize(J9JavaVM * vm, J9StackWalkState * walkState);
IDATA lswFrameNew(J9JavaVM * vm, J9StackWalkState * walkState, UDATA frameType);
IDATA lswRecord(J9StackWalkState * walkState, UDATA recordType, void * recordValue);
IDATA lswPrintFrames(J9VMThread * vmThread, J9StackWalkState * walkState);
IDATA lswCleanup(J9JavaVM * vm, J9StackWalkState * walkState);
IDATA lswRecordSlot(J9StackWalkState * walkState, const void * slotAddress, UDATA slotType, const char *format, ...);

#endif
