/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#include "stackmap_internal.h"
#include "rommeth.h"
#include "bcnames.h"
#include "pcstack.h"
#include "argcount.h"
#include "cfreader.h"
#include "ut_map.h"

#define	WALKED	1

#if 0 /* StackMapTable based fixreturns removed - CMVC 145180 - bogus stack maps cause shareclasses crash*/
/* Return code only used locally */
#define	MISSING_MAPS	1
#endif

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

static IDATA fixReturnBytecodesInMethod (J9PortLibrary * portLib, J9ROMClass * romClass, J9ROMMethod * romMethod);
static void fixReturns (UDATA *scratch, U_8 * map, J9ROMClass * romClass, J9ROMMethod * romMethod);

#if 0 /* StackMapTable based fixreturns removed */
static IDATA fixReturnsWithStackMaps(J9ROMClass * romClass, J9ROMMethod * romMethod, U_32 * stackMapMethods, UDATA i);
static UDATA getNextStackIndex (U_8 * stackMapData, UDATA mapPC, UDATA stackMapCount);
static UDATA getStackDepth (U_8 ** stackMapData, UDATA * stackMapCount);
static void parseLocals (U_8** stackMapData, IDATA localDelta);
static UDATA parseStack (U_8** stackMapData, UDATA stackCount);
#endif

/*
	Called by jar2jxe and the shared class loader to fix returns on unverified ROM classes.
*/

IDATA
fixReturnBytecodes(J9PortLibrary * portLib, struct J9ROMClass* romClass)
{
	UDATA i;
	J9ROMMethod * romMethod;
	IDATA rc = 0;
	BOOLEAN isJavaLangObject = (J9ROMCLASS_SUPERCLASSNAME(romClass) == NULL);

	Trc_Map_fixReturnBytecodes_Class((UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(romClass)));

	romMethod = J9ROMCLASS_ROMMETHODS(romClass);

	for (i = 0; i < romClass->romMethodCount; i++, romMethod = nextROMMethod(romMethod)) {
		if ((romMethod->modifiers & (CFR_ACC_NATIVE | CFR_ACC_ABSTRACT)) == 0) {
			if (isJavaLangObject) {
				/* Avoid rewriting genericReturn for Object.<init>() */
				U_8 * nameData = J9UTF8_DATA(J9ROMMETHOD_GET_NAME(romClass, romMethod));
				if (('<' == nameData[0]) && ('i' == nameData[1]) && (1 == romMethod->argCount)) {
					continue;
				}
			}
			rc = fixReturnBytecodesInMethod (portLib, romClass, romMethod);
			if (rc != BCT_ERR_NO_ERROR) {
				return rc;
			}
		}
	}

	return 0;
}



/* 
	Returns the bytecode for cleaning a method return - also returns the number of slots in the parameter.
*/

U_8
getReturnBytecode(J9ROMClass * romClass, J9ROMMethod * romMethod, UDATA * returnSlots)
{

	U_8 sigChar, returnBytecode;

	J9UTF8 * name = J9ROMMETHOD_GET_NAME(romClass, romMethod);
	J9UTF8 * signature = J9ROMMETHOD_GET_SIGNATURE(romClass, romMethod);

	U_8 * sigData = J9UTF8_DATA(signature);
	UDATA sigLength = J9UTF8_LENGTH(signature);

	/* Determine correct number of return slots for this method based on signature */

	*returnSlots = 0;
	sigChar = sigData[sigLength - 1];

	/* Update the sig char in the case we are dealing with an array */
	if ('[' == sigData[sigLength - 2]) {
		sigChar = '[';
	}

	if (sigChar != 'V') {
		*returnSlots = 1;
		if (sigChar == 'J' || sigChar == 'D') {
			*returnSlots = 2;
		}
	}

	/* Determine the correct return bytecode to insert */
	if ((J9UTF8_DATA(name)[0] == '<') && (J9UTF8_DATA(name)[1] == 'i')) {
		returnBytecode = JBreturnFromConstructor;
	} else {
		/* bool, byte, char, and short need special treatment since they need to be truncated before return */
		if (romMethod->modifiers & J9AccSynchronized) {
			switch(sigChar){
			case 'Z':
			case 'B':
			case 'C':
			case 'S':
				returnBytecode = JBgenericReturn;
				break;
			default:
				returnBytecode = JBsyncReturn0 + (U_8) *returnSlots;
				break;
			}
		} else {
			switch(sigChar){
			case 'Z':
				returnBytecode = JBreturnZ;
				break;
			case 'B':
				returnBytecode = JBreturnB;
				break;
			case 'C':
				returnBytecode = JBreturnC;
				break;
			case 'S':
				returnBytecode = JBreturnS;
				break;
			default:
				returnBytecode = JBreturn0 + (U_8) *returnSlots;
				break;
			}
		}
	}


	return returnBytecode;
}



/*
	Set up a data flow walk to identify and clean any return bytecodes.
	The data consists of a "walked" map and a stack with unwalked branch pointers and matching depth counts.
*/

static IDATA
fixReturnBytecodesInMethod(J9PortLibrary * portLib, J9ROMClass * romClass, J9ROMMethod * romMethod)
{

	PORT_ACCESS_FROM_PORT(portLib);
	UDATA length;
	UDATA *scratch;
	UDATA scratchSize;
	UDATA accurateGuess = FALSE;
	U_8 *map;
#define LOCAL_SCRATCH 2048
	UDATA localScratch[LOCAL_SCRATCH / sizeof(UDATA)];
	UDATA *allocScratch = NULL;

	length = (UDATA) J9_ROUNDED_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);
	scratchSize = (romClass->maxBranchCount * sizeof(UDATA) * 2) + length;

	/* Allocate sufficient buffer */
	if(scratchSize < LOCAL_SCRATCH) {
		scratch = localScratch;

	} else {
		allocScratch = j9mem_allocate_memory(scratchSize, OMRMEM_CATEGORY_VM);

		if (allocScratch) {
			scratch = allocScratch;
		} else {
			Trc_Map_fixReturnBytecodesInMethod_AllocationFailure(scratchSize);
			return BCT_ERR_OUT_OF_MEMORY;
		}
	}

	/* Allocate the instruction map at end of scratch space */
	map = (U_8 *) (scratch + (romClass->maxBranchCount * 2));
	memset(map, 0, length);

	fixReturns(scratch, map, romClass, romMethod);

	j9mem_free_memory(allocScratch);

	return BCT_ERR_NO_ERROR;
}


/* 
	Data flow walk to identify the stack depth at return points and replace generic returns with the correct return type. 
*/

static void 
fixReturns(UDATA *scratch, U_8 * map, J9ROMClass * romClass, J9ROMMethod * romMethod)
{
	IDATA pc;
	I_32 low, high, offset;
	U_32 index;
	U_8 *bcStart, *bcIndex, *bcEnd;
	U_8 sigChar, returnBytecode;
	UDATA bc, action, size, temp1, temp2, target, npairs;
	UDATA returnSlots;
	UDATA maxStack = (UDATA) J9_MAX_STACK_FROM_ROM_METHOD(romMethod);
	UDATA depth = 0;
	UDATA length;
	J9UTF8 *utf8Signature;
	J9SRP *callSiteData = (J9SRP *) J9ROMCLASS_CALLSITEDATA(romClass);
	UDATA *pendingStacks = scratch;
	J9ROMConstantPoolItem *pool = (J9ROMConstantPoolItem *) & (romClass[1]);
	J9ExceptionInfo *exceptionData = J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod);

	/* Determine the correct return bytecode to insert and the expected depth for a fixable return */

	returnBytecode = getReturnBytecode(romClass, romMethod, &returnSlots);

	/* Add exceptions to the pending walks list */
	if (J9ROMMETHOD_HAS_EXCEPTION_INFO(romMethod)) {
		UDATA exceptionsToWalk = (UDATA) exceptionData->catchCount;
		J9ExceptionHandler *handler = J9EXCEPTIONINFO_HANDLERS(exceptionData);

		while (exceptionsToWalk) {
			*pendingStacks++ = handler[--exceptionsToWalk].handlerPC;
			*pendingStacks++ = 1;
		}
	}

	bcStart = J9_BYTECODE_START_FROM_ROM_METHOD(romMethod);
	bcIndex = bcStart;
	length = J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);
	bcEnd = bcStart + length;

	while (bcIndex < bcEnd) {
		pc = bcIndex - bcStart;
		bc = (UDATA) *bcIndex;

		if (map[pc]) {
			/* We have been here before, stop this scan */
			goto _nextRoot;
		}
		map[pc] = WALKED;

		size = (UDATA) J9JavaInstructionSizeAndBranchActionTable[bc];
		action = (UDATA) JavaStackActionTable[bc];

		/* action encodes pushes and pops by the bytecode - 0x80 means something special */
		if (action != 0x80) {
			depth -= (action & 7);
			depth += ((action >> 4) & 3);

			if (!(size >> 4)) {
				bcIndex += size;
				if (size == 0) {
					/* Unknown bytecode */
					Trc_Map_fixReturns_UnknownBytecode(bc, pc);
					return;
				}
				continue;
			}

			switch (size >> 4) {

			case 1:			/* ifs */
				offset = (I_16) PARAM_16(bcIndex, 1);
				target = (UDATA) (pc + (I_16) offset);
				if (!map[target]) {
					*pendingStacks++ = target;
					*pendingStacks++ = depth;
				}
				bcIndex += (size & 7);
				continue;

			case 2:			/* gotos */
				offset = (I_16) PARAM_16(bcIndex, 1);
				target = (UDATA) (pc + (I_16) offset);
				if (bc == JBgotow) {
					offset = (I_32) PARAM_32(bcIndex, 1);
					target = (UDATA) (pc + offset);
				}
				bcIndex = bcStart + target;
				continue;

			case 4:			/* returns/athrow */
				/* fix the generic returns only - already changed returns */
				/* may have been changed to forward a verification error */
				if (bc == JBgenericReturn) {
					if (depth == returnSlots) {
						*bcIndex = returnBytecode;
					}
				}
				goto _nextRoot;

			case 5:			/* switches */
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
					if (!map[target]) {
						*pendingStacks++ = target;
						*pendingStacks++ = depth;
					}
				}

				/* finally continue at the default switch case */
				bcIndex = bcStart + temp2;
				continue;

			case 7:			/* breakpoint */
				/* Unexpected bytecode - unknown */
				Trc_Map_fixReturns_UnknownBytecode(bc, pc);
				return;

			default:
				bcIndex += (size & 7);
				continue;

			}
		} else {
			switch (bc) {

			case JBdup2:
			case JBdup2x1:
			case JBdup2x2:
				depth++;	/* fall through case */

			case JBldc:
			case JBldcw:
			case JBdup:
			case JBdupx1:
			case JBdupx2:
				depth++;	/* fall through case */

			case JBswap:
				break;

			case JBgetfield:
				depth--;	/* fall through case */

			case JBgetstatic:
				index = PARAM_16(bcIndex, 1);
				utf8Signature =
					J9ROMNAMEANDSIGNATURE_SIGNATURE(J9ROMFIELDREF_NAMEANDSIGNATURE
													((J9ROMFieldRef *) (&(pool[index]))));
				sigChar = (U_8) J9UTF8_DATA(utf8Signature)[0];
				depth += (UDATA) argCountCharConversion[sigChar - 'A'];
				break;

			case JBputfield:
				depth--;	/* fall through case !!! */

			case JBputstatic:
				index = PARAM_16(bcIndex, 1);
				utf8Signature =
					J9ROMNAMEANDSIGNATURE_SIGNATURE(J9ROMFIELDREF_NAMEANDSIGNATURE
													((J9ROMFieldRef *) (&(pool[index]))));
				sigChar = (U_8) J9UTF8_DATA(utf8Signature)[0];
				depth -= (UDATA) argCountCharConversion[sigChar - 'A'];
				break;

			case JBinvokeinterface2:
				bcIndex += 2;
				continue;

			case JBinvokehandle:
			case JBinvokehandlegeneric:
			case JBinvokevirtual:
			case JBinvokespecial:
			case JBinvokespecialsplit:
			case JBinvokeinterface:
				depth--;
			case JBinvokedynamic:
			case JBinvokestaticsplit:
			case JBinvokestatic: {
				index = PARAM_16(bcIndex, 1);

				if (JBinvokestaticsplit == bc) {
					index = *(U_16 *)(J9ROMCLASS_STATICSPLITMETHODREFINDEXES(romClass) + index);
				} else if (JBinvokespecialsplit == bc) {
					index = *(U_16 *)(J9ROMCLASS_SPECIALSPLITMETHODREFINDEXES(romClass) + index);
				}
				if (bc == JBinvokedynamic) {
					/* TODO 3 byte index */
					utf8Signature = (J9UTF8 *) (J9ROMNAMEANDSIGNATURE_SIGNATURE(SRP_PTR_GET(callSiteData + index, J9ROMNameAndSignature*)));
				} else {
					utf8Signature =
							J9ROMNAMEANDSIGNATURE_SIGNATURE(J9ROMMETHODREF_NAMEANDSIGNATURE
													((J9ROMMethodRef *) (&(pool[index]))));
				}

				depth -= (UDATA) getSendSlotsFromSignature(J9UTF8_DATA(utf8Signature));

				sigChar = J9UTF8_DATA(utf8Signature)[J9UTF8_LENGTH(utf8Signature) - 1];
				if (sigChar != 'V') {
					depth++;
					if (((sigChar == 'J') || (sigChar == 'D')) &&
							(J9UTF8_DATA(utf8Signature)[J9UTF8_LENGTH(utf8Signature) - 2] != '['))
					{
						depth++;
					}
				}

				if (bc == JBinvokeinterface2) {
					bcIndex -= 2;
				}
				break;
			}
			case JBmultianewarray:
				index = PARAM_8(bcIndex, 3);
				depth -= index;
				depth++;
				break;

			}
			bcIndex += (size & 7);
			continue;
		}

	  _nextRoot:

		if (pendingStacks == scratch) {
			return;
		}

		/* pop the next pending stack for walking */
		depth = *(--pendingStacks);
		bcIndex = bcStart + *(--pendingStacks);
	}
	Trc_Map_fixReturns_WalkOffEndOfBytecodeArray();
	/* Fell off end of bytecode array - should never get here */
}

#if 0 /* StackMapTable based fixreturns removed */
/* 
	Linear walk utilizing stackmaps to identify the stack depth at return points 
	and replace generic returns with the correct return type. 
*/

static IDATA 
fixReturnsWithStackMaps(J9ROMClass * romClass, J9ROMMethod * romMethod, U_32 * stackMapMethods, UDATA i)
{
	I_32 low, high;
	U_32 index;
	U_8 *bcStart, *bcIndex, *bcEnd;
	UDATA nextStackIndex = (UDATA) -1;
	U_8 *stackMapData = NULL;
	U_8 sigChar, returnBytecode;
	UDATA bc, action, size, slots;
	UDATA returnSlots;
	UDATA depth = 0;
	UDATA stackMapCount = 0;
	J9UTF8 *utf8Signature;
	J9SRP *callSiteData = (J9SRP *) J9ROMCLASS_CALLSITEDATA(romClass);
	UDATA needStack = FALSE;
	J9ROMConstantPoolItem *pool = (J9ROMConstantPoolItem *) & (romClass[1]);

	/*Access the stored stack map data for this method, get pointer and map count */
	if (stackMapMethods) {
		if (stackMapMethods[i]) {
			stackMapData = SRP_GET(stackMapMethods[i], U_8 *);
			NEXT_U16(stackMapCount, stackMapData);
			nextStackIndex = getNextStackIndex (stackMapData, nextStackIndex, stackMapCount);
		}
	}
	
	/* Determine the correct return bytecode to insert and the expected depth for a fixable return */
	returnBytecode = getReturnBytecode(romClass, romMethod, &returnSlots);

	bcStart = J9_BYTECODE_START_FROM_ROM_METHOD(romMethod);
	bcIndex = bcStart;
	bcEnd = bcStart + J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);

	while (bcIndex < bcEnd) {
		bc = (UDATA) *bcIndex;
		if (((UDATA) (bcIndex - bcStart)) == nextStackIndex) {
			depth = getStackDepth (&stackMapData, &stackMapCount);
			nextStackIndex = getNextStackIndex (stackMapData, (bcIndex - bcStart), stackMapCount);
			needStack = FALSE;
		}

		/* Should always have a stack at least after any goto, athrow, return or switch type bytecode */
		/* if not found, default to the flow mapper for this method */
		if (needStack) {
			return MISSING_MAPS;
		}

		size = (UDATA) J9JavaInstructionSizeAndBranchActionTable[bc];
		action = (UDATA) JavaStackActionTable[bc];

		/* action encodes pushes and pops by the bytecode - 0x80 means something special */
		if (action != 0x80) {
			depth -= (action & 7);
			depth += ((action >> 4) & 3);

			/* Check upper nibble of size is less than 2 */
			if ((size & 0xE0) == 0) {
				bcIndex += (size & 7);
				if (size == 0) {
					/* Must check for zero else it will infinitely loop */
					/* Unknown bytecode - will fail verification later */
					Trc_Map_fixReturnsWithStackMaps_UnknownBytecode(bc, (bcIndex - bcStart));
					return BCT_ERR_NO_ERROR;
				}
				continue;
			}

			switch (size >> 4) {

			case 2:			/* goto/gotow */
			case 4:			/* returns/athrow */
				/* fix the generic returns only - already changed returns */
				/* may have been changed to forward a verification error */
				if (bc == JBgenericReturn) {
					if (depth == returnSlots) {
						*bcIndex = returnBytecode;
					} else {
						/* Fail if a return cannot be fixed - use the flow algorithm */
						return MISSING_MAPS;
					}
				}
				bcIndex += (size & 7);
				needStack = TRUE;
				continue;

			case 5:			/* switches */
				bcIndex = bcIndex + (4 - ((bcIndex - bcStart) & 3));
				low = (I_32) PARAM_32(bcIndex, 4);
				bcIndex += 8;

				if (bc == CFR_BC_tableswitch) {
					high = (I_32) PARAM_32(bcIndex, 0);
					bcIndex += 4;
					slots = high - low + 1;
				} else {
					slots = low * 2;
				}
				bcIndex += (slots * 4);
				needStack = TRUE;
				continue;

			default:
				bcIndex += (size & 7);
				continue;

			}
		} else {
			switch (bc) {

			case JBdup2:
			case JBdup2x1:
			case JBdup2x2:
				depth++;	/* fall through case */

			case JBldc:
			case JBldcw:
			case JBdup:
			case JBdupx1:
			case JBdupx2:
				depth++;	/* fall through case */

			case JBswap:
				break;

			case JBgetfield:
				depth--;	/* fall through case */

			case JBgetstatic:
				index = PARAM_16(bcIndex, 1);
				utf8Signature =
					J9ROMNAMEANDSIGNATURE_SIGNATURE(J9ROMFIELDREF_NAMEANDSIGNATURE
													((J9ROMFieldRef *) (&(pool[index]))));
				sigChar = (U_8) J9UTF8_DATA(utf8Signature)[0];
				depth += (UDATA) argCountCharConversion[sigChar - 'A'];
				break;

			case JBputfield:
				depth--;	/* fall through case !!! */

			case JBputstatic:
				index = PARAM_16(bcIndex, 1);
				utf8Signature =
					J9ROMNAMEANDSIGNATURE_SIGNATURE(J9ROMFIELDREF_NAMEANDSIGNATURE
													((J9ROMFieldRef *) (&(pool[index]))));
				sigChar = (U_8) J9UTF8_DATA(utf8Signature)[0];
				depth -= (UDATA) argCountCharConversion[sigChar - 'A'];
				break;

			case JBinvokeinterface2:
				bcIndex += 2;
				/* Point to JBinterface and adjust remaining size */
				size = 3;
				/* Fall through */

			case JBinvokehandle:
			case JBinvokehandlegeneric:
			case JBinvokevirtual:
			case JBinvokespecial:
			case JBinvokeinterface:
				depth--;
			case JBinvokedynamic:
			case JBinvokestatic:
				index = PARAM_16(bcIndex, 1);

				if (bc == JBinvokedynamic) {
					 /* TODO 3 byte index */
					utf8Signature = (J9UTF8 *) (J9ROMNAMEANDSIGNATURE_SIGNATURE(SRP_PTR_GET(callSiteData + index, J9ROMNameAndSignature*)));
				} else {
					utf8Signature =
							J9ROMNAMEANDSIGNATURE_SIGNATURE(J9ROMMETHODREF_NAMEANDSIGNATURE
													((J9ROMMethodRef *) (&(pool[index]))));
				}

				depth -= (UDATA) getSendSlotsFromSignature(J9UTF8_DATA(utf8Signature));

				sigChar = J9UTF8_DATA(utf8Signature)[J9UTF8_LENGTH(utf8Signature) - 1];
				if (sigChar != 'V') {
					depth++;
					
					if (((sigChar == 'J') || (sigChar == 'D')) &&
							(J9UTF8_DATA(utf8Signature)[J9UTF8_LENGTH(utf8Signature) - 2] != '[')) {
						depth++;
					}
				}
				break;

			case JBmultianewarray:
				index = PARAM_8(bcIndex, 3);
				depth -= index;
				depth++;
				break;

			}
			bcIndex += (size & 7);
		}
	}
	return BCT_ERR_NO_ERROR;
}


/* 
	Extract and return the bytecode offset for the next stack map.  -1 indicates no further maps. 
*/

static UDATA
getNextStackIndex (U_8 * stackMapData, UDATA mapPC, UDATA stackMapCount)
{
	U_8 mapType;
	UDATA temp;

	if (stackMapCount == 0) {
		return (UDATA) -1;
	}
	
	mapPC++;
	NEXT_U8(mapType, stackMapData);

	if (mapType < CFR_STACKMAP_SAME_LOCALS_1_STACK) {
		/* offset delta encoded in the mapType */
		mapPC += (UDATA) mapType;

	} else if (mapType < CFR_STACKMAP_SAME_LOCALS_1_STACK_END) {
		/* offset delta encoded in the mapType */
		mapPC += ((UDATA) mapType - CFR_STACKMAP_SAME_LOCALS_1_STACK);

	} else {
		/* offset delta explicit after the mapType */
		mapPC += NEXT_U16(temp, stackMapData);
	}
	return mapPC;
}


/* 
	Calculate the stack depth from the current stack map.
	Most occurrences will be zero. 
*/

static UDATA
getStackDepth (U_8 ** stackMapData, UDATA * stackMapCount)
{
	U_8 mapType;
	UDATA temp;
	UDATA depth = 0;

	if (*stackMapCount) {
		IDATA localDelta = 0;
		UDATA stackCount = 0;
		
		(*stackMapCount)--;
		NEXT_U8(mapType, *stackMapData);

		if (mapType < CFR_STACKMAP_SAME_LOCALS_1_STACK) {
			/* done */
			return depth;

		} else if (mapType < CFR_STACKMAP_SAME_LOCALS_1_STACK_END) {
			/* Same with one stack entry frame 64-127 */
			stackCount = 1;

		} else {
			/* Skip the map offset_delta */
			NEXT_U16(temp, *stackMapData);

			if (mapType == CFR_STACKMAP_SAME_LOCALS_1_STACK_EXTENDED) {
				/* Same with one stack entry extended address frame 247 */
				stackCount = 1;

			} else if (mapType < CFR_STACKMAP_FULL) {
				/* Chop 3-1 locals frame 248-250 */
				/* Same with extended address frame 251 */
				/* Append 1-3 locals frame 252-254 */
				localDelta = ((IDATA) mapType) - CFR_STACKMAP_SAME_EXTENDED;

			} else if (mapType == CFR_STACKMAP_FULL) {
				/* Full frame 255 */
				NEXT_U16(localDelta, *stackMapData);
			}
		}

		parseLocals (stackMapData, localDelta);

		if (mapType == CFR_STACKMAP_FULL) {
			stackCount = NEXT_U16(temp, *stackMapData);
		}

		depth = parseStack (stackMapData, stackCount);
	}

	return depth;
}


/* 
	Parse through the local variable descriptors in a stack map - throw away. 
*/

static void
parseLocals (U_8** stackMapData, IDATA localDelta)
{
	/* Eat the local descriptors */
	if (localDelta > 0) {
		for (;localDelta; localDelta--) {
			U_8 entryType = **stackMapData;
			(*stackMapData)++;
			if (entryType >= CFR_STACKMAP_TYPE_OBJECT) {
				(*stackMapData) += 2;
			}
		}
	}
}


/* 
	Parse through the stack descriptors to calculate the depth. 
*/

static UDATA
parseStack (U_8** stackMapData, UDATA stackCount)
{
	/* Starting depth is one per map descriptor */ 
	UDATA depth = stackCount;

	for (;stackCount; stackCount--) {
		U_8 entryType = **stackMapData;
		/* Skip one descriptor byte per entry */
		(*stackMapData)++;

		if ((entryType == CFR_STACKMAP_TYPE_DOUBLE) || (entryType == CFR_STACKMAP_TYPE_LONG)) {
			/* Increase depth for each Double/Long - they take two slots */
			depth++;

		} else if (entryType >= CFR_STACKMAP_TYPE_OBJECT) {
			/* Skip two more descriptor bytes for other entries */
			/* It is either constant pool index or an array size */
			(*stackMapData) += 2;
		}
	}
	return depth;
}

#endif
