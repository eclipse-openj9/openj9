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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "cfreader.h"
#include "j9.h"

#include "jbcmap.h"
#include "pcstack.h" 
#include "bcnames.h" 
#include "j9port.h"
#include "j9protos.h"
#include "j9consts.h"
#include "j9cp.h"
#include "rommeth.h"
#include "argcount.h"
#include "stackmap_internal.h"
#include "ut_map.h"


#define DEFAULT_SCRATCH_SIZE 8192

#define INT			0x0	
#define OBJ			0x1
#define	NOT_FOUND	-1

#define	WALKED					0x1
#define	STACK_REQUEST		0x2

typedef struct J9MappingStack {
    UDATA pc;
		UDATA *stackTop;
		UDATA stackElements[1];
} J9MappingStack;

#define POP() \
	(*(--stackTop))

#define PUSH( t ) \
	((*stackTop++ = t))

#define PARAM_8(index, offset) ((index) [offset])

#ifdef J9VM_ENV_LITTLE_ENDIAN
#define PARAM_16(index, offset)	\
	( ( ((U_16) (index)[offset])			)	\
	| ( ((U_16) (index)[offset + 1]) << 8)	\
	)
#else
#define PARAM_16(index, offset)	\
	( ( ((U_16) (index)[offset]) << 8)	\
	| ( ((U_16) (index)[offset + 1])			)	\
	)
#endif

#ifdef J9VM_ENV_LITTLE_ENDIAN
#define PARAM_32(index, offset)						\
	( ( ((U_32) (index)[offset])					)	\
	| ( ((U_32) (index)[offset + 1]) << 8 )	\
	| ( ((U_32) (index)[offset + 2]) << 16)	\
	| ( ((U_32) (index)[offset + 3]) << 24)	\
	)
#else
#define PARAM_32(index, offset)						\
	( ( ((U_32) (index)[offset])		 << 24)	\
	| ( ((U_32) (index)[offset + 1]) << 16)	\
	| ( ((U_32) (index)[offset + 2]) << 8 )	\
	| ( ((U_32) (index)[offset + 3])			)	\
	)
#endif

static J9MappingStack* pushStack (J9MappingStack* liveStack, UDATA totalStack, UDATA** stackTop);
static IDATA outputStackMap (J9MappingStack * liveStack, U_32 * newStackDescription, UDATA bits);
static IDATA mapStack (UDATA *scratch, UDATA totalStack, U_8 * map, J9ROMClass * romClass, J9ROMMethod * romMethod, J9MappingStack ** resultStack);

/*
	Called by the GC to map frames - returns the stack depth at the map point.
	Also called by jar2jxe and the shared class loader to fix returns.
*/

IDATA
j9stackmap_StackBitsForPC(J9PortLibrary * portLib, UDATA pc, J9ROMClass * romClass, J9ROMMethod * romMethod,
								U_32 * resultArrayBase, UDATA resultArraySize,
								void * userData, 
								UDATA * (* getBuffer) (void * userData), 
								void (* releaseBuffer) (void * userData))
{

	PORT_ACCESS_FROM_PORT(portLib);
	UDATA length, stackStructSize;
	UDATA scratchSize;
	UDATA *scratch;
	UDATA maxStackStructsSize;
	UDATA accurateGuess = FALSE;
	IDATA rc;
	U_8 *map;
	J9MappingStack *resultStack = NULL;
#define LOCAL_SCRATCH 2048
	UDATA localScratch[LOCAL_SCRATCH / sizeof(UDATA)];
	UDATA *allocScratch = NULL;
	UDATA *globalScratch = NULL;

	Trc_Map_j9stackmap_StackBitsForPC_Method(J9_MAX_STACK_FROM_ROM_METHOD(romMethod), pc, 
												(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(romClass)),
												(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_GET_NAME(romClass, romMethod)), J9UTF8_DATA(J9ROMMETHOD_GET_NAME(romClass, romMethod)),
												(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_GET_SIGNATURE(romClass, romMethod)), J9UTF8_DATA(J9ROMMETHOD_GET_SIGNATURE(romClass, romMethod)));
	
	/* Add 2 for the J9MappingStack size */
	stackStructSize = J9_MAX_STACK_FROM_ROM_METHOD(romMethod) + 2;
	length = (UDATA) J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);

	maxStackStructsSize = stackStructSize * romClass->maxBranchCount;
	scratchSize = (maxStackStructsSize * sizeof(UDATA)) + length;

	/* Allocate sufficient buffer */
	if(scratchSize < LOCAL_SCRATCH) {
		scratch = localScratch;

	} else {
		allocScratch = j9mem_allocate_memory(scratchSize, OMRMEM_CATEGORY_VM);

		if (allocScratch) {
			scratch = allocScratch;
		} else if (getBuffer != NULL) {
			globalScratch = (getBuffer) (userData);
			scratch = globalScratch;
			if (!scratch) {
				Trc_Map_j9stackmap_StackBitsForPC_GlobalBufferFailure(scratchSize);
				return BCT_ERR_OUT_OF_MEMORY;
			}
		} else {
			Trc_Map_j9stackmap_StackBitsForPC_AllocationFailure(scratchSize);
			return BCT_ERR_OUT_OF_MEMORY;
		}
	}

	/* Allocate the instruction map at end of scratch space */
	map = (U_8 *) (scratch + maxStackStructsSize);
	memset(map, 0, length);

	/* Flag the map target bytecode */
	map[pc] = STACK_REQUEST;

	if ((rc = mapStack(scratch, stackStructSize, map, romClass, romMethod, &resultStack)) == BCT_ERR_NO_ERROR) {
		rc = outputStackMap(resultStack, resultArrayBase, resultArraySize);
	}

 	if (globalScratch) {
		(releaseBuffer) (userData);
 	}

	j9mem_free_memory(allocScratch);

	return rc;
}

#undef LOCAL_SCRATCH



/* 

*/

static IDATA 
mapStack(UDATA *scratch, UDATA totalStack, U_8 * map, J9ROMClass * romClass, J9ROMMethod * romMethod, J9MappingStack ** resultStack)
{

	I_32 low, high, offset;
	U_32 index;
	U_8 *bcStart, *bcIndex, *bcEnd;
	U_8 signature;
	UDATA bc, action, size, type, temp1, temp2, temp3, target, npairs;
	UDATA *stackTop, *stackLow;
	UDATA length;
	UDATA exceptionsToWalk = 0;
	J9UTF8 *utf8Signature;
	J9ROMNameAndSignature *nameAndSig;
	J9SRP *callSiteData;
	J9MappingStack *liveStack;
	J9MappingStack *startStack = (J9MappingStack *) scratch;
	J9ROMConstantPoolItem *pool = (J9ROMConstantPoolItem *) & (romClass[1]);
	J9ExceptionInfo *exceptionData = J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod);

	if (J9ROMMETHOD_HAS_EXCEPTION_INFO(romMethod)) {
		exceptionsToWalk = (UDATA) exceptionData->catchCount;
	}

	/* initialize the first stack */
	liveStack = startStack;
	stackTop = &(liveStack->stackElements[0]);
	liveStack->stackTop = stackTop;

	stackLow = (UDATA *) &(liveStack->stackTop);

	bcStart = J9_BYTECODE_START_FROM_ROM_METHOD(romMethod);
	bcIndex = bcStart;
	length = J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);
	bcEnd = bcStart + length;

	while (bcIndex < bcEnd) {
		UDATA pc = bcIndex - bcStart;
		bc = (UDATA) *bcIndex;

		if (map[pc]) {
			/* See if this is the target pc */
			if (map[pc] & STACK_REQUEST) {
				liveStack->stackTop = stackTop;
				*resultStack = liveStack;
				return BCT_ERR_NO_ERROR;
			}
			/* We have been here before, stop this scan */
			goto _nextRoot;
		}
		map[pc] = WALKED;

		size = (UDATA) J9JavaInstructionSizeAndBranchActionTable[bc];
		action = (UDATA) JavaStackActionTable[bc];

		/* action encodes pushes and pops by the bytecode - 0x80 means something special */
		if (action != 0x80) {
			stackTop -= (action & 7);

			if (action >= 0x10) {
				if (action >= 0x50) {
					PUSH(OBJ);
				} else {
					PUSH(INT);
					if (action >= 0x20) {
						PUSH(INT);
					}
				}
			}

			if (!(size >> 4)) {
				bcIndex += size;
				if (size == 0) {
					/* Unknown bytecode */
					Trc_Map_mapstack_UnknownBytecode(bc, pc);
					return BCT_ERR_STACK_MAP_FAILED;
				}
				continue;
			}

			switch (size >> 4) {

			case 1:			/* ifs */
				offset = (I_16) PARAM_16(bcIndex, 1);
				target = (UDATA) (pc + (I_16) offset);
				if (!(map[target] & WALKED)) {
					liveStack->pc = target;
					liveStack = pushStack(liveStack, totalStack, &stackTop);
					stackLow = (UDATA *) &(liveStack->stackTop);
				}
				bcIndex += (size & 7);
				continue;

			case 2:			/* gotos */
				if (bc == JBgoto) {
					offset = (I_16) PARAM_16(bcIndex, 1);
					target = (UDATA) (pc + (I_16) offset);
				} else {
					offset = (I_32) PARAM_32(bcIndex, 1);
					target = (UDATA) (pc + offset);
				}
				bcIndex = bcStart + target;
				continue;

			case 4:			/* returns/athrow */
				goto _nextRoot;

			case 5:			/* switches */
				liveStack->stackTop = stackTop;

				bcIndex = bcIndex + (4 - (pc & 3));
				offset = (I_32) PARAM_32(bcIndex, 0);
				bcIndex += 4;
				temp2 = (UDATA) (pc + offset);
				low = (I_32) PARAM_32(bcIndex, 0);
				bcIndex += 4;

				if (bc == JBtableswitch) {
					high = (I_32) PARAM_32(bcIndex, 0);
					bcIndex += 4;
					npairs = (UDATA) (high - low + 1);
					temp1 = 0;
				} else {
					npairs = (UDATA) low;
					/* used to skip over the tableswitch key entry */
					temp1 = 4;
				}

				for (; npairs > 0; npairs--) {
					bcIndex += temp1;
					offset = (I_32) PARAM_32(bcIndex, 0);
					bcIndex += 4;
					target = (UDATA) (pc + (I_32) offset);
					if (!(map[target] & WALKED)) {
						liveStack->pc = target;
						liveStack = pushStack(liveStack, totalStack, &stackTop);
					}
				}

				stackLow = (UDATA *) &(liveStack->stackTop);

				/* finally continue at the default switch case */
				bcIndex = bcStart + temp2;
				continue;

			case 7:			/* breakpoint */
				/* Unexpected bytecode - unknown */
				Trc_Map_mapstack_UnknownBytecode(bc, pc);
				return BCT_ERR_STACK_MAP_FAILED;

			default:
				bcIndex += (size & 7);
				continue;

			}
		} else {
			switch (bc) {

			case JBldc:
			case JBldcw:
				if (bc == JBldc) {
					index = PARAM_8(bcIndex, 1);
				} else {
					index = PARAM_16(bcIndex, 1);
				}

				if (pool[index].slot2) {
					PUSH(OBJ);
				} else {
					PUSH(INT);
				}
				
				break;

			case JBdup:
				type = POP();
				PUSH(type);
				PUSH(type);
				
				break;

			case JBdupx1:
				type = POP();
				temp1 = POP();
				PUSH(type);
				PUSH(temp1);
				PUSH(type);
				
				break;

			case JBdupx2:
				type = POP();
				temp1 = POP();
				temp2 = POP();
				PUSH(type);
				PUSH(temp2);
				PUSH(temp1);
				PUSH(type);
				
				break;

			case JBdup2:
				temp1 = POP();
				temp2 = POP();
				PUSH(temp2);
				PUSH(temp1);
				PUSH(temp2);
				PUSH(temp1);
				
				break;

			case JBdup2x1:
				type = POP();
				temp1 = POP();
				temp2 = POP();
				PUSH(temp1);
				PUSH(type);
				PUSH(temp2);
				PUSH(temp1);
				PUSH(type);
				
				break;

			case JBdup2x2:
				type = POP();
				temp1 = POP();
				temp2 = POP();
				temp3 = POP();
				PUSH(temp1);
				PUSH(type);
				PUSH(temp3);
				PUSH(temp2);
				PUSH(temp1);
				PUSH(type);
				
				break;

			case JBswap:
				type = POP();
				temp1 = POP();
				PUSH(type);
				PUSH(temp1);
				break;

			case JBgetfield:
				POP();			/* fall through case !!!! */

			case JBgetstatic:
				index = PARAM_16(bcIndex, 1);
				utf8Signature =
					J9ROMNAMEANDSIGNATURE_SIGNATURE(J9ROMFIELDREF_NAMEANDSIGNATURE
													((J9ROMFieldRef *) (&(pool[index]))));
				signature = (U_8) J9UTF8_DATA(utf8Signature)[0];
				if ((signature == 'L') || (signature == '[')) {
					PUSH(OBJ);
				} else {
					PUSH(INT);
					if ((signature == 'J') || (signature == 'D')) {
						PUSH(INT);
					}
				}
				
				break;

			case JBputfield:
				POP();			/* fall through case !!! */

			case JBputstatic:
				index = PARAM_16(bcIndex, 1);
				utf8Signature =
					J9ROMNAMEANDSIGNATURE_SIGNATURE(J9ROMFIELDREF_NAMEANDSIGNATURE
													((J9ROMFieldRef *) (&(pool[index]))));
				signature = (U_8) J9UTF8_DATA(utf8Signature)[0];
				stackTop -= (UDATA) argCountCharConversion[signature - 'A'];
				break;

			case JBinvokeinterface2:
				bcIndex += 2;
				continue;

			case JBinvokehandle:
			case JBinvokehandlegeneric:
			case JBinvokevirtual:
			case JBinvokespecial:
			case JBinvokeinterface:
			case JBinvokespecialsplit:
				POP();
			case JBinvokestaticsplit:
			case JBinvokestatic: {
				index = PARAM_16(bcIndex, 1);

				if (JBinvokestaticsplit == bc) {
					index = *(U_16 *)(J9ROMCLASS_STATICSPLITMETHODREFINDEXES(romClass) + index);
				} else if (JBinvokespecialsplit == bc) {
					index = *(U_16 *)(J9ROMCLASS_SPECIALSPLITMETHODREFINDEXES(romClass) + index);
				}
				utf8Signature =
						J9ROMNAMEANDSIGNATURE_SIGNATURE(
								J9ROMMETHODREF_NAMEANDSIGNATURE((J9ROMMethodRef *) (&(pool[index]))));
				stackTop -= (UDATA) getSendSlotsFromSignature(J9UTF8_DATA(utf8Signature));

				signature = J9UTF8_DATA(utf8Signature)[J9UTF8_LENGTH(utf8Signature) - 1];
				if (signature != 'V') {
					if ((signature == ';') || (J9UTF8_DATA(utf8Signature)[J9UTF8_LENGTH(utf8Signature) - 2] == '[')) {
						PUSH(OBJ);
					} else {
						PUSH(INT);
						if ((signature == 'J') || (signature == 'D')) {
							PUSH(INT);
						}
					}
				}

				if (bc == JBinvokeinterface2) {
					bcIndex -= 2;
				}
				break;
			}
			case JBinvokedynamic:
				index = PARAM_16(bcIndex, 1);
				/* TODO 3 byte index */
				callSiteData = (J9SRP *) J9ROMCLASS_CALLSITEDATA(romClass);
				nameAndSig = SRP_PTR_GET(callSiteData + index, J9ROMNameAndSignature*);
				utf8Signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);

				stackTop -= (UDATA) getSendSlotsFromSignature(J9UTF8_DATA(utf8Signature));
				signature = J9UTF8_DATA(utf8Signature)[J9UTF8_LENGTH(utf8Signature) - 1];
				if (signature != 'V') {
					if ((signature == ';') || (J9UTF8_DATA(utf8Signature)[J9UTF8_LENGTH(utf8Signature) - 2] == '[')) {
						PUSH(OBJ);
					} else {
						PUSH(INT);
						if ((signature == 'J') || (signature == 'D')) {
							PUSH(INT);
						}
					}
				}
				break;

			case JBmultianewarray:
				index = PARAM_8(bcIndex, 3);
				stackTop -= index;
				PUSH(OBJ);
				break;

			}
			bcIndex += (size & 7);
			continue;
		}

	  _nextRoot:

		if (liveStack == startStack) {
			if (exceptionsToWalk != 0) {
				/* target PC not found in regular code - try exceptions */
				J9ExceptionHandler *handler = J9EXCEPTIONINFO_HANDLERS(exceptionData);

				/* exceptions are initialized with stacks containing only an object (the exception object) */
				stackTop = &(liveStack->stackElements[0]);
				PUSH(OBJ);
				liveStack->stackTop = stackTop;

				while (exceptionsToWalk) {
					/* branch all exceptions */
					liveStack->pc = handler[--exceptionsToWalk].handlerPC;
					liveStack = pushStack(liveStack, totalStack, &stackTop);
				}

			} else {
				/* PC not found - possible if asked to map dead code return by shared class loading */
				Trc_Map_mapstack_MapPCNotFound();
				return BCT_ERR_STACK_MAP_FAILED;
			}
		}
		liveStack = (J9MappingStack *) (((UDATA *) liveStack) - totalStack);
		stackTop = liveStack->stackTop;
		stackLow = (UDATA *) &(liveStack->stackTop);
		bcIndex = bcStart + liveStack->pc;
	}
	Trc_Map_mapstack_WalkOffEndOfBytecodeArray();
	return BCT_ERR_STACK_MAP_FAILED;					/* Fell off end of bytecode array - should never get here */
}



/* Assume that bits will never be 0 on entry unless newStackDescription is NULL 
	 (as used for  cleaning in shared classes and jar2jxe - they only want the depth) 
*/

static IDATA 
outputStackMap(J9MappingStack * liveStack, U_32 * newStackDescription, UDATA bits)
{
	UDATA *stackTop;
	IDATA stackSize;

	stackSize = (IDATA) (liveStack->stackTop - (UDATA *) (&(liveStack->stackElements[0])));

	if (stackSize && newStackDescription) {

		stackTop = liveStack->stackTop;

		/* Get rid of unwanted stack entries. */
		/* Typically they are invoke parameters that are mapped as locals in the next frame */
		stackTop -= (stackSize - bits);

		newStackDescription += ((bits - 1) >> 5);
		*newStackDescription = 0;

		for (;;) {
			*newStackDescription <<= 1;
			*newStackDescription |= *(--stackTop);
			if (--bits == 0) {
				break;
			}
			if ((bits & 31) == 0) {
				*--newStackDescription = 0;
			}
		}
	}

	return stackSize;
}



static J9MappingStack* 
pushStack(J9MappingStack* liveStack, UDATA totalStack, UDATA** stackTop)
{

	J9MappingStack *newStack;

	/* save the stack pointer */
	liveStack->stackTop = *stackTop;
	/* point to next stack */
	newStack = (J9MappingStack *) (((UDATA *) liveStack) + totalStack);

	memcpy((UDATA *) newStack, (UDATA *) liveStack, totalStack * sizeof(UDATA));

	/* update the stack pointer */
	newStack->stackTop = liveStack->stackTop + totalStack;
	*stackTop = *stackTop + totalStack;

	return newStack;
}
