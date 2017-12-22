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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "j9.h"

#include "pcstack.h" 
#include "bcnames.h" 
#include "j9port.h"
#include "j9protos.h"
#include "j9consts.h"
#include "rommeth.h"
#include "stackmap_internal.h"
#include "vrfytbl.h"
#include "bytecodewalk.h"
#include "ut_map.h"

#define	BRANCH_TARGET			0x01
#define	BRANCH_EXCEPTION_START	0x02
#define	BRANCH_TARGET_IN_USE	0x04
#define	BRANCH_TARGET_TO_WALK	0x08

/* PARALLEL_TYPE max size is U_32 without changing the unwalked branch stack size and logic (it holds seenLocals and pc) */
#define PARALLEL_TYPE		U_32
#define	PARALLEL_BYTES		(sizeof(PARALLEL_TYPE))
#define	PARALLEL_WRITES		((UDATA) (sizeof (U_32) / PARALLEL_BYTES))
#define	PARALLEL_BITS		(PARALLEL_BYTES * 8)

#ifdef J9VM_ENV_LITTLE_ENDIAN
#define PARALLEL_START_INDEX	(0)
#define	PARALLEL_INCREMENT		(1)
#else
#define PARALLEL_START_INDEX	(PARALLEL_WRITES - 1)
#define	PARALLEL_INCREMENT		(-1)
#endif

#define PARAM_8(index, offset) ((index) [offset])

#ifdef J9VM_ENV_LITTLE_ENDIAN
#define PARAM_16(index, offset)				\
	( ( ((U_16) (index)[offset])		)	\
	| ( ((U_16) (index)[offset + 1]) << 8)	\
	)
#else
#define PARAM_16(index, offset)			\
	( ( ((U_16) (index)[offset]) << 8)	\
	| ( ((U_16) (index)[offset + 1]) )	\
	)
#endif

#ifdef J9VM_ENV_LITTLE_ENDIAN
#define PARAM_32(index, offset)				\
	( ( ((U_32) (index)[offset])		  )	\
	| ( ((U_32) (index)[offset + 1]) << 8 )	\
	| ( ((U_32) (index)[offset + 2]) << 16)	\
	| ( ((U_32) (index)[offset + 3]) << 24)	\
	)
#else
#define PARAM_32(index, offset)				\
	( ( ((U_32) (index)[offset])	 << 24)	\
	| ( ((U_32) (index)[offset + 1]) << 16)	\
	| ( ((U_32) (index)[offset + 2]) << 8 )	\
	| ( ((U_32) (index)[offset + 3])	  ) \
	)
#endif

typedef struct DebugLocalMap {
	U_8 *bytecodeMap;
	PARALLEL_TYPE *mapArray;
	U_32 *rootStack;
	U_32 *rootStackTop;
	U_32 *resultArrayBase;
	J9ROMMethod *romMethod;
	UDATA targetPC;
	PARALLEL_TYPE currentLocals;
	J9ROMClass *romClass;
} DebugLocalMap;

static void debugBuildBranchMap (DebugLocalMap * mapData);
static void debugMapAllLocals (DebugLocalMap * mapData);
static IDATA debugMapLocalSet (DebugLocalMap * mapData, UDATA localIndexBase);
static void debugMergeStacks (DebugLocalMap * mapData, UDATA target);


/* Mapping the verification encoding in J9JavaBytecodeVerificationTable */
static const U_32 decodeTable[] = {
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

/*
 * 	Merge the maps
 */

static void debugMergeStacks (DebugLocalMap * mapData, UDATA target)
{
	if (mapData->bytecodeMap[target] & BRANCH_TARGET_IN_USE) {
		PARALLEL_TYPE mergeResult = mapData->mapArray[target] & mapData->currentLocals;
		if (mergeResult == mapData->mapArray[target]) {
			return;
		} else {
			/* map changed - merge and rewalk */
			mapData->mapArray[target] = mergeResult;
			if (mapData->bytecodeMap[target] & BRANCH_TARGET_TO_WALK) {
				/* already on rootStack */
				return;
			}
		}
	} else {
		/* new map - save locals */
		mapData->mapArray[target] = mapData->currentLocals;
		mapData->bytecodeMap[target] |= BRANCH_TARGET_IN_USE;
	}
	*(mapData->rootStackTop) = (U_32) (target);
	(mapData->rootStackTop)++; 
	mapData->bytecodeMap[target] |= BRANCH_TARGET_TO_WALK;
}


/* 
	Build up the Locals map size_of(PARALLEL_TYPE) * 8 slots at a time from 0 to remainingLocals-1.
*/

static void
debugMapAllLocals(DebugLocalMap * mapData) 
{
	J9ROMMethod * romMethod = mapData->romMethod;
	UDATA localIndexBase = 0;
	UDATA writeIndex = PARALLEL_START_INDEX;
	UDATA remainingLocals = J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod) + J9_ARG_COUNT_FROM_ROM_METHOD(romMethod);
	PARALLEL_TYPE * parallelResultArrayBase = (PARALLEL_TYPE *) mapData->resultArrayBase;
	UDATA length = J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);

	/* Start with the arguments configured from the signature */
	/* A side effect is zeroing out the result array */
	argBitsFromSignature(
			J9UTF8_DATA(J9ROMMETHOD_GET_SIGNATURE(mapData->romClass, romMethod)),
			mapData->resultArrayBase,
			(remainingLocals + 31) >> 5,
			(romMethod->modifiers & J9AccStatic) != 0);

	while (remainingLocals) {
		if (remainingLocals > PARALLEL_BITS) {
			remainingLocals -= PARALLEL_BITS;
		} else {
			remainingLocals = 0;
		}
		
		/* set currentLocals to the appropriate part of resultArrayBase contents */
		mapData->currentLocals = parallelResultArrayBase[writeIndex];

		/* clear the mapArray for each local set */
		memset(mapData->mapArray, 0, length * PARALLEL_BYTES);

		/* Updated result is returned in currentLocals */
		debugMapLocalSet(mapData, localIndexBase);

		/* reset the bytecodeMap for next local set - leave only the original branch targets */
		if (remainingLocals) {
			UDATA i;
			for (i = 0; i < length; i++) {
				mapData->bytecodeMap[i] &= (BRANCH_TARGET | BRANCH_EXCEPTION_START);
			}
		}

		/* save the local set result */
		parallelResultArrayBase[writeIndex] = mapData->mapArray[mapData->targetPC];
		writeIndex += PARALLEL_INCREMENT;
		/* relies on wrapping of unsigned subtract for big endian */
		if (writeIndex >= PARALLEL_WRITES) {
			writeIndex = PARALLEL_START_INDEX;
			parallelResultArrayBase += PARALLEL_WRITES;
		}
		localIndexBase += PARALLEL_BITS;
	}
	return;
}



/* 
	Walk the bytecodes from the current pc to determine up to 32 at a time local variable visibilities.
*/

static IDATA
debugMapLocalSet(DebugLocalMap * mapData, UDATA localIndexBase) 
{
	J9ROMMethod * romMethod = mapData->romMethod;
	PARALLEL_TYPE * mapArray = mapData->mapArray;
	U_8 * bytecodeMap = mapData->bytecodeMap;
	UDATA pc = 0;
	IDATA start;
	I_32 offset, low, high;
	U_32 npairs, setBit;
	UDATA index;
	I_16 offset16;
	I_32 offset32;
	U_8 *code, *bcIndex;
	UDATA popCount, type1, type2, action;
	UDATA justLoadedStack = FALSE;
	UDATA wideIndex = FALSE;
	UDATA bc, temp1, target, length;
	UDATA checkIfInsideException = J9ROMMETHOD_HAS_EXCEPTION_INFO(romMethod);
	J9ExceptionInfo *exceptionData = J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod);
	J9ExceptionHandler *handler;
	UDATA exception;

	code = J9_BYTECODE_START_FROM_ROM_METHOD(romMethod);
	length = J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);

	bcIndex = code;

	while (pc < length) {
		BOOLEAN lookAtPreviousTwoSlotConstant = FALSE;
		start = (IDATA) pc;
		
		if (bytecodeMap[start] & BRANCH_EXCEPTION_START) {
			handler = J9EXCEPTIONINFO_HANDLERS(exceptionData);

			for (exception = 0; exception < (UDATA) exceptionData->catchCount; exception++) {
				/* find the matching branch target, and copy/merge the locals to the exception target */
				if ((U_32) start == handler->startPC) {
					debugMergeStacks (mapData, handler->handlerPC);
				}
				handler++;
			}
		}

		/* Merge all branchTargets encountered */	
		if (bytecodeMap[start] & BRANCH_TARGET) {
			/* Don't try to merge a stack we just loaded */
			if (!justLoadedStack) {
				/* TBD merge map to this branch target in mapArray */
				debugMergeStacks (mapData, start);
				goto _checkFinished;
			}
		}
		justLoadedStack = FALSE;
			
		bcIndex = code + start;
		bc = *bcIndex;
		pc += (J9JavaInstructionSizeAndBranchActionTable[bc] & 7);

		type1 = (UDATA) J9JavaBytecodeVerificationTable[bc];
		action = type1 >> 8;
		type2 = (type1 >> 4) & 0xF;
		type1 = (UDATA) decodeTable[type1 & 0xF];
		type2 = (UDATA) decodeTable[type2];

		switch (action) {

		case RTV_WIDE_POP_STORE_TEMP:
			wideIndex = TRUE;	/* Fall through case !!! */

		case RTV_POP_STORE_TEMP:
			index = type2 & 0x7;
			if (type2 == 0) {
				index = PARAM_8(bcIndex, 1);
				if (wideIndex) {
					index = PARAM_16(bcIndex, 1);
					wideIndex = FALSE;
				}
			}
			
			/* Trace only those locals in the range of interest */
			lookAtPreviousTwoSlotConstant = (index + 1) == localIndexBase;
			index -= localIndexBase;

			if (index < PARALLEL_BITS) {
				setBit = 1 << index;
				
				if (type1 == BCV_GENERIC_OBJECT) {
					mapData->currentLocals |= setBit;
				} else {
					mapData->currentLocals &= ~setBit;					
					if (type1 & BCV_WIDE_TYPE_MASK) {
						setBit <<= 1;
						mapData->currentLocals &= ~setBit;					
					}
				}
			} else if (lookAtPreviousTwoSlotConstant) {
				/* Cover the case of a long/double at index 31 */
				if (type1 & BCV_WIDE_TYPE_MASK) {
					mapData->currentLocals &= ~(U_32)1;
				}
			}

			/* should inside exception be in bit array?? */
			if (checkIfInsideException) {
				/* For all exception handlers covering this instruction */
				handler = J9EXCEPTIONINFO_HANDLERS(exceptionData);

				for (exception = 0; exception < (UDATA) exceptionData->catchCount; exception++) {
					/* find the matching branch target, and copy/merge the locals to the exception target */
					if (((U_32) start >= handler->startPC) && ((U_32) start < handler->endPC)) {
						debugMergeStacks (mapData, handler->handlerPC);
					}
					handler++;
				}
			}
			break;

		case RTV_BRANCH:
			popCount = type2 & 0x07;

			if (bc == JBgotow) {
				offset32 = (I_32) PARAM_32(bcIndex, 1);
				target = start + offset32;
			} else {
				offset16 = (I_16) PARAM_16(bcIndex, 1);
				target = start + offset16;
			}

			debugMergeStacks (mapData, target);

			/* Unconditional branch (goto family) */
			if (popCount == 0) {
				goto _checkFinished;
			}
			break;

		case RTV_RETURN:
			goto _checkFinished;
			break;

		case RTV_MISC:
			switch (bc) {
			case JBathrow:
				goto _checkFinished;
				break;

			case JBtableswitch:
			case JBlookupswitch:
				bcIndex = (U_8 *) (((UDATA)bcIndex + 4) & ~3);
				offset = (I_32) PARAM_32(bcIndex, 0);
				bcIndex += 4;
				target = (UDATA) (start + offset);
				debugMergeStacks (mapData, target);
				low = (I_32) PARAM_32(bcIndex, 0);
				bcIndex += 4;

				if (bc == JBtableswitch) {
					high = (I_32) PARAM_32(bcIndex, 0);
					bcIndex += 4;
					npairs = (U_32) (high - low + 1);
					temp1 = 0;
				} else {
					npairs = (U_32) low;
					/* used to skip over the tableswitch key entry */
					temp1 = 4;
				}

				for (; npairs > 0; npairs--) {
					bcIndex += temp1;
					offset = (I_32) PARAM_32(bcIndex, 0);
					bcIndex += 4;
					target = (UDATA) (start + offset);
					debugMergeStacks (mapData, target);
				}

				goto _checkFinished;
				break;
			}
			break;
		}
		continue;

		_checkFinished:

		if (mapData->rootStackTop != mapData->rootStack) {
			(mapData->rootStackTop)--;
			pc = *(mapData->rootStackTop);
			mapData->currentLocals = mapData->mapArray[pc];
			bytecodeMap[pc] &= ~BRANCH_TARGET_TO_WALK;
			justLoadedStack = TRUE;
		} else {
			/* else we are done the rootStack -- return */
			return 0;
		}
	}
	Trc_Map_debugMapLocalSet_WalkOffEndOfBytecodeArray();
	return -1;					/* Should never get here */
}




/*
	Tag the branch and exception targets as well as the mapping target.
*/

static void 
debugBuildBranchMap (DebugLocalMap * mapData)
{
	J9ROMMethod *romMethod = mapData->romMethod;
	U_8 *bytecodeMap = mapData->bytecodeMap;
	U_8 *bcStart;
	U_8 *bcIndex;
	U_8 *bcEnd;
	UDATA npairs, temp;
	IDATA pc, start, high, low, pcs;
	I_16 shortBranch;
	I_32 longBranch;
	UDATA bc, size;

	bcStart = J9_BYTECODE_START_FROM_ROM_METHOD(romMethod);
	bcIndex = bcStart;
	bcEnd = bcStart + J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);

	memset(bytecodeMap, 0, J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod));

	/* Adjust JBinvokeinterface to point to the JBinvokeinterface2 */
	if (bcIndex[mapData->targetPC] == JBinvokeinterface) {
		mapData->targetPC -= 2;
	}
	
	/* Flag the mapping target */
	bytecodeMap[mapData->targetPC] = BRANCH_TARGET;

	while (bcIndex < bcEnd) {
		bc = *bcIndex;
		size = J9JavaInstructionSizeAndBranchActionTable[bc];

		switch (size >> 4) {

		case 5: /* switches */
			start = bcIndex - bcStart;
			pc = (start + 4) & ~3;
			bcIndex = bcStart + pc;
			longBranch = (I_32) PARAM_32(bcIndex, 0);
			bcIndex += 4;
			bytecodeMap[start + longBranch] = BRANCH_TARGET;
			low = (I_32) PARAM_32(bcIndex, 0);
			bcIndex += 4;
			low = (I_32) low;
			if (bc == JBtableswitch) {
				high = (I_32) PARAM_32(bcIndex, 0);
				bcIndex += 4;
				high = (I_32) high;
				npairs = (UDATA) (high - low + 1);
				pcs = 0;
			} else {
				npairs = (UDATA) low;
				pcs = 4;
			}

			for (temp = 0; temp < npairs; temp++) {
				bcIndex += pcs;
				longBranch = (I_32) PARAM_32(bcIndex, 0);
				bcIndex += 4;
				bytecodeMap[start + longBranch] = BRANCH_TARGET;
			}
			continue;

		case 2: /* gotos */
			if (bc == JBgotow) {
				start = bcIndex - bcStart;
				longBranch = (I_32) PARAM_32(bcIndex, 1);
				bytecodeMap[start + longBranch] = BRANCH_TARGET;
				break;
			} /* fall through for JBgoto */

		case 1: /* ifs */
			shortBranch = (I_16) PARAM_16(bcIndex, 1);
			start = bcIndex - bcStart;
			bytecodeMap[start + shortBranch] = BRANCH_TARGET;
			break;

		}
		bcIndex += size & 7;
	}

	/* need to walk exceptions as well, since they are branch targets */
	if (J9ROMMETHOD_HAS_EXCEPTION_INFO(romMethod)) {
		J9ExceptionInfo * exceptionData = J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod);
		J9ExceptionHandler *handler;

		if (exceptionData->catchCount) {
			handler = J9EXCEPTIONINFO_HANDLERS(exceptionData);
			for (temp=0; temp < (U_32) exceptionData->catchCount; temp++) {
				pc = (IDATA) handler->startPC;
				pcs = (IDATA) handler->handlerPC;
				/* Avoid re-walking a handler that handles itself */
				if (pc != pcs) {
					bytecodeMap[pc] |= BRANCH_EXCEPTION_START;
				}
				bytecodeMap[pcs] |= BRANCH_TARGET;
				handler++;
			}
		}
	}
}

/*
 * 
 * Perform a conservative map of active local variables for use during debug mode
 * 
 */

IDATA
j9localmap_DebugLocalBitsForPC(J9PortLibrary * portLib, J9ROMClass * romClass, J9ROMMethod * romMethod, UDATA pc, U_32 * resultArrayBase,
								void * userData, 
								UDATA * (* getBuffer) (void * userData), 
								void (* releaseBuffer) (void * userData)) 
{

	PORT_ACCESS_FROM_PORT(portLib);
	UDATA mapWords = (UDATA) ((J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod) + J9_ARG_COUNT_FROM_ROM_METHOD(romMethod) + 31) >> 5);
	UDATA length;
	UDATA *scratch;
	DebugLocalMap mapData;
	UDATA scratchSize;
#define LOCAL_SCRATCH 2048
	UDATA localScratch[LOCAL_SCRATCH / sizeof(UDATA)];
	UDATA *allocScratch = NULL;
	UDATA *globalScratch = NULL;

	Trc_Map_j9localmap_DebugLocalBitsForPC_Method(J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod) + J9_ARG_COUNT_FROM_ROM_METHOD(romMethod), pc, 
													(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(romClass)),
													(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_GET_NAME(romClass, romMethod)), J9UTF8_DATA(J9ROMMETHOD_GET_NAME(romClass, romMethod)),
													(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_GET_SIGNATURE(romClass, romMethod)), J9UTF8_DATA(J9ROMMETHOD_GET_SIGNATURE(romClass, romMethod)));
	
	memset(&mapData, 0, sizeof(DebugLocalMap));
	mapData.romMethod = romMethod;
	mapData.targetPC = pc;
	mapData.resultArrayBase = resultArrayBase;
	mapData.romClass = romClass;

	length = (UDATA) J9_ROUNDED_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);
	scratchSize = ((romClass->maxBranchCount + 2) * sizeof(U_32)) + (length * sizeof(U_32)) + length; 

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
				Trc_Map_j9localmap_DebugLocalBitsForPC_GlobalBufferFailure(scratchSize);
				return BCT_ERR_OUT_OF_MEMORY;
			}
		} else {
			Trc_Map_j9localmap_DebugLocalBitsForPC_AllocationFailure(scratchSize);
			return BCT_ERR_OUT_OF_MEMORY;
		}
	}

	mapData.bytecodeMap = (U_8 *) scratch;
	mapData.mapArray = (U_32*) (((UDATA) mapData.bytecodeMap) + length);
	mapData.rootStack = (U_32 *) (((UDATA) mapData.mapArray) + (length * sizeof(U_32)));
	debugBuildBranchMap(&mapData);
	mapData.rootStackTop = mapData.rootStack;

	memset(mapData.rootStack, 0, (romClass->maxBranchCount + 2) * sizeof(U_32));
	debugMapAllLocals(&mapData);

 	if (globalScratch) {
		(releaseBuffer) (userData);
 	}

	j9mem_free_memory(allocScratch);

	return 0;
}

#undef LOCAL_SCRATCH


UDATA
validateLocalSlot(J9VMThread *currentThread, J9Method *ramMethod, U_32 offsetPC, U_32 slot, char slotSignature, UDATA compressTypes)
{
	UDATA rc = J9_SLOT_VALIDATE_ERROR_NONE;
	J9ROMMethod * romMethod = getOriginalROMMethod(ramMethod);
	U_32 argTempCount = romMethod->argCount + romMethod->tempCount;
	U_32 smallMap;
	U_32 * localMap = &smallMap;
	J9ROMClass * romClass = J9_CLASS_FROM_METHOD(ramMethod)->romClass;
	J9JavaVM * vm = currentThread->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);
#if defined(J9VM_OPT_DEBUG_INFO_SERVER)
	J9MethodDebugInfo *methodDebugInfo;
#endif

	if (romMethod->modifiers & J9AccNative) {
		return J9_SLOT_VALIDATE_ERROR_NATIVE_METHOD;
	}

	/* Make sure the the slot number is in range of the args and temps */

	if ((slotSignature == 'D') || (slotSignature == 'J')) {
		if ((slot + 1) >= argTempCount) {
			return J9_SLOT_VALIDATE_ERROR_INVALID_SLOT;
		}
	} else {
		if (slot >= argTempCount) {
			return J9_SLOT_VALIDATE_ERROR_INVALID_SLOT;
		}
	}

#if defined(J9VM_OPT_DEBUG_INFO_SERVER)
	/* Check the local variable table for more stringent type checking */

	methodDebugInfo = getMethodDebugInfoForROMClass(vm, ramMethod);
	if (methodDebugInfo != NULL) {
		J9VariableInfoWalkState state;
		J9VariableInfoValues *values;
		
		values = variableInfoStartDo(methodDebugInfo, &state);
		while(values != NULL) {
			if ((values->slotNumber == slot) && (offsetPC >= values->startVisibility) && (offsetPC < (values->startVisibility + values->visibilityLength))) {
				J9UTF8 * signature = values->signature;
				char sigChar = (char) (J9UTF8_DATA(signature)[0]);

				if (compressTypes) {
					if ((sigChar == 'B') || (sigChar == 'C') || (sigChar == 'S') || (sigChar == 'Z')) {
						sigChar = 'I';
					} else if (sigChar == '[') {
						sigChar = 'L';
					}
				}
				if (sigChar != slotSignature) {
					rc = J9_SLOT_VALIDATE_ERROR_TYPE_MISMATCH;
				}
				break;
			}
			values = variableInfoNextDo(&state);
		}
		releaseOptInfoBuffer(vm, romClass);
	}
#endif

	/* If the type is valid, check the local map to ensure that the variable table isn't lying */

	if (rc == J9_SLOT_VALIDATE_ERROR_NONE) {
		IDATA mapRC;

		if (argTempCount > 32) {
			localMap = j9mem_allocate_memory(((argTempCount + 31) / 32) * sizeof(U_32), OMRMEM_CATEGORY_VM);
			if (localMap == NULL) {
				return J9_SLOT_VALIDATE_ERROR_OUT_OF_MEMORY;
			}
		}

		mapRC = j9localmap_DebugLocalBitsForPC(PORTLIB, romClass, romMethod, offsetPC, localMap, vm, j9mapmemory_GetBuffer, j9mapmemory_ReleaseBuffer);
		if (mapRC < 0) {
			switch (mapRC) {
				case BCT_ERR_OUT_OF_MEMORY:
					rc = J9_SLOT_VALIDATE_ERROR_OUT_OF_MEMORY;
					break;
				default:
					rc = J9_SLOT_VALIDATE_ERROR_LOCAL_MAP_ERROR;
					break;
			}
		} else {
			if ((slotSignature == 'L') || (slotSignature == '[')) {
				if ((localMap[slot / 32] & (1 << (slot % 32))) == 0) {
					rc = J9_SLOT_VALIDATE_ERROR_LOCAL_MAP_MISMATCH;
				}
			} else {
				if ((localMap[slot / 32] & (1 << (slot % 32))) == 0) {
					if ((slotSignature == 'J') || (slotSignature == 'D')) {
						if ((localMap[(slot + 1) / 32] & (1 << ((slot + 1) % 32))) != 0) {
							rc = J9_SLOT_VALIDATE_ERROR_LOCAL_MAP_MISMATCH;
						}
					}
				} else {
					rc = J9_SLOT_VALIDATE_ERROR_LOCAL_MAP_MISMATCH;
				}
			}
		}
	
		if (argTempCount > 32) {
			j9mem_free_memory(localMap);
		}
	}

	return rc;
}


void
installDebugLocalMapper(J9JavaVM * vm)
{
	vm->localMapFunction = j9localmap_DebugLocalBitsForPC;
}
