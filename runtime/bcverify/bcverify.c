/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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

#include <string.h>

#include "bcvcfr.h"
#include "bcverify.h"
#include "j9bcvnls.h"

#include "cfreader.h"
#include "bcnames.h"
#include "pcstack.h"
#include "j9cp.h"
#include "j9protos.h"
#include "j9consts.h"
#include "omrthread.h"
#include "jvminit.h"
#include "vrfyconvert.h"
#include "bcverify_internal.h"
#include "vrfytbl.h"
#include "vmhook_internal.h"
#include "SCQueryFunctions.h"

#define _UTE_STATIC_
#include "ut_j9bcverify.h"

#include "bcverify.h"

/* Define for debug
#define DEBUG_BCV
*/

#define	BYTECODE_MAP_DEFAULT_SIZE		(2 * 1024)
#define	STACK_MAPS_DEFAULT_SIZE			(2 * 1024)
#define	LIVE_STACK_DEFAULT_SIZE			256
#define	ROOT_QUEUE_DEFAULT_SIZE 		256
#define	CLASSNAMELIST_DEFAULT_SIZE		(128 * sizeof(UDATA *))	/* 128 pointers */
#define	CLASSNAMESEGMENT_DEFAULT_SIZE	1024	/* 1k bytes - minimum of 8 bytes per classNameList entry */

#define BCV_INTERNAL_DEFAULT_SIZE (32*1024)

#define THIS_DLL_NAME J9_VERIFY_DLL_NAME
#define OPT_XVERIFY "-Xverify"
#define OPT_XVERIFY_COLON "-Xverify:"
#define OPT_ALL "all"
#define OPT_OPT "opt"
#define OPT_NO_OPT "noopt"
#define OPT_NO_FALLBACK "nofallback"
#define OPT_IGNORE_STACK_MAPS "ignorestackmaps"
#define OPT_EXCLUDEATTRIBUTE_EQUAL "excludeattribute="
#define OPT_BOOTCLASSPATH_STATIC "bootclasspathstatic"
#define OPT_DO_PROTECTED_ACCESS_CHECK "doProtectedAccessCheck"

static IDATA buildBranchMap (J9BytecodeVerificationData * verifyData);
static IDATA decompressStackMaps (J9BytecodeVerificationData * verifyData, IDATA localsCount, U_8 * stackMapData);
static VMINLINE IDATA parseLocals (J9BytecodeVerificationData * verifyData, U_8** stackMapData, J9BranchTargetStack * liveStack, IDATA localDelta, IDATA localsCount, IDATA maxLocals);
static VMINLINE IDATA parseStack (J9BytecodeVerificationData * verifyData, U_8** stackMapData, J9BranchTargetStack * liveStack, UDATA stackCount, UDATA maxStack);
static UDATA parseElement (J9BytecodeVerificationData * verifyData, U_8 ** stackMapData);
static VMINLINE void copyStack (J9BranchTargetStack *source, J9BranchTargetStack *destination);
static IDATA mergeObjectTypes (J9BytecodeVerificationData *verifyData, UDATA sourceType, UDATA * targetTypePointer);
static IDATA mergeStacks (J9BytecodeVerificationData * verifyData, UDATA target);
static J9UTF8 * mergeClasses(J9BytecodeVerificationData *verifyData, U_8* firstClass, UDATA firstLength, U_8* secondClass, UDATA secondLength, IDATA *reasonCode);
static void bcvHookClassesUnload (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void printMethod (J9BytecodeVerificationData * verifyData);
static IDATA simulateStack (J9BytecodeVerificationData * verifyData);

static IDATA parseOptions (J9JavaVM *vm, char *optionValues, char **errorString);
static IDATA setVerifyState ( J9JavaVM *vm, char *option, char **errorString );


/**
 * Walk the J9-format stack maps and set the uninitialized_this flag appropriately
 * for each map.  It is set to TRUE for the map, if the map's stack contains an
 * uninitialized_this object.
 * NOTE: This is only necessary for <init> methods.
 */
static void 
setInitializedThisStatus(J9BytecodeVerificationData *verifyData)
{
	J9BranchTargetStack * currentStack = NULL;
	IDATA nextMapIndex = 0;

	while (nextMapIndex < verifyData->stackMapsCount) {
		currentStack = BCV_INDEX_STACK (nextMapIndex);
		nextMapIndex++;

		/* Ensure we're not a stack map for dead code */
		if (currentStack->stackBaseIndex != -1) {
			BOOLEAN flag_uninitialized = FALSE;
			IDATA i = 0;
			for (; i < currentStack->stackTopIndex; i++) {
				if ((currentStack->stackElements[i] & BCV_SPECIAL_INIT) == BCV_SPECIAL_INIT) {
					flag_uninitialized = TRUE;
					break;
				}
			}
			currentStack->uninitializedThis = flag_uninitialized;
		}
	}
}



/*
	API
		@verifyData		-	internal data structure
		@firstClass		-	U_8 pointer to class name
		@firstLength	- UDATA length of class name
		@secondClass	-	U_8 pointer to class name
		@secondLength	- UDATA length of class name

	Answer the first common class shared by the two classes.
		If one of the classes is a parent of the other, answer that class.
	Return NULL on error.
		sets reasonCode to BCV_FAIL on verification error
		sets reasonCode to BCV_ERR_INSUFFICIENT_MEMORY on OOM
		sets reasonCode to BCV_ERR_INTERNAL_ERROR otherwise
*/
#define SUPERCLASS(clazz) ((clazz)->superclasses[ J9CLASS_DEPTH(clazz) - 1 ])
static J9UTF8 *
mergeClasses(J9BytecodeVerificationData *verifyData, U_8* firstClass, UDATA firstLength, U_8* secondClass, UDATA secondLength, IDATA *reasonCode)
{
	J9Class *sourceRAM, *targetRAM;
	UDATA sourceDepth, targetDepth;

	/* Go get the ROM class for the source and target.  Check if it returns null immediately to prevent
	 * having to load the second class in an error case */
	sourceRAM = j9rtv_verifierGetRAMClass( verifyData, verifyData->classLoader, firstClass, firstLength, reasonCode);
	if (NULL == sourceRAM) {
		return NULL;
	}

	targetRAM = j9rtv_verifierGetRAMClass( verifyData, verifyData->classLoader, secondClass, secondLength, reasonCode );
	if (NULL == targetRAM) {
		return NULL;
	}
	sourceRAM = J9_CURRENT_CLASS(sourceRAM);
	sourceDepth = J9CLASS_DEPTH(sourceRAM);
	targetDepth = J9CLASS_DEPTH(targetRAM);

	/* walk up the chain until sourceROM == targetROM */
	while( sourceRAM != targetRAM ) {
		if( sourceDepth >= targetDepth ) {
			sourceRAM = SUPERCLASS(sourceRAM);
			if ( sourceRAM ) {
				sourceDepth = J9CLASS_DEPTH(sourceRAM);
			}
		}
		if (sourceRAM == targetRAM )
			break;
		if( sourceDepth <= targetDepth ) {
			targetRAM = SUPERCLASS(targetRAM);
			if ( targetRAM ) {
				targetDepth = J9CLASS_DEPTH(targetRAM);
			}
		}
		if( (sourceRAM == NULL) || (targetRAM == NULL) ) {
			*reasonCode = BCV_FAIL;
			return NULL;
		}
	}

	/* good, both sourceROM and targetROM are the same class -- this is the new target class */
	return( J9ROMCLASS_CLASSNAME( targetRAM->romClass ) );
}
#undef SUPERCLASS


/*
	Determine the number of branch targets in this method.

	Returns
		count of unique branch targets and exception handler starts.
		BCV_ERR_INTERNAL_ERROR for any unexpected error
*/

static IDATA 
buildBranchMap (J9BytecodeVerificationData * verifyData)
{
	J9ROMMethod *romMethod = verifyData->romMethod;
	U_32 *bytecodeMap = verifyData->bytecodeMap;
	U_8 *bcStart;
	U_8 *bcIndex;
	U_8 *bcEnd;
	UDATA npairs, temp;
	IDATA pc, start, high, low, pcs;
	I_16 shortBranch;
	I_32 longBranch;
	UDATA bc, size;
	UDATA count = 0;

	bcStart = J9_BYTECODE_START_FROM_ROM_METHOD(romMethod);
	bcIndex = bcStart;
	bcEnd = bcStart + J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);

	while (bcIndex < bcEnd) {
		bc = *bcIndex;
		size = J9JavaInstructionSizeAndBranchActionTable[bc];
		if (size == 0) {
			verifyData->errorPC = bcIndex - bcStart;
			Trc_BCV_buildBranchMap_UnknownInstruction(verifyData->vmStruct,
					(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
					J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
					(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(romMethod)),
					J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)),
					(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(romMethod)),
					J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(romMethod)),
					bc, verifyData->errorPC, verifyData->errorPC);
			return BCV_ERR_INTERNAL_ERROR;
		}

		switch (size >> 4) {

		case 5: /* switches */
			start = bcIndex - bcStart;
			pc = (start + 4) & ~3;
			bcIndex = bcStart + pc;
			longBranch = (I_32) PARAM_32(bcIndex, 0);
			bcIndex += 4;
			if (bytecodeMap[start + longBranch] == 0) {
				bytecodeMap[start + longBranch] = BRANCH_TARGET;
				count++;
			}
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
				if (bytecodeMap[start + longBranch] == 0) {
					bytecodeMap[start + longBranch] = BRANCH_TARGET;
					count++;
				}
			}
			continue;

		case 2: /* gotos */
			if (bc == JBgotow) {
				start = bcIndex - bcStart;
				longBranch = (I_32) PARAM_32(bcIndex, 1);
				if (bytecodeMap[start + longBranch] == 0) {
					bytecodeMap[start + longBranch] = BRANCH_TARGET;
					count++;
				}
				break;
			} /* fall through for JBgoto */

		case 1: /* ifs */
			shortBranch = (I_16) PARAM_16(bcIndex, 1);
			start = bcIndex - bcStart;
			if (bytecodeMap[start + shortBranch] == 0) {
				bytecodeMap[start + shortBranch] = BRANCH_TARGET;
				count++;
			}
			break;

		}
		bcIndex += size & 7;
	}

	/* need to walk exceptions as well, since they are branch targets */
	if (romMethod->modifiers & J9AccMethodHasExceptionInfo) {
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
				if ((bytecodeMap[pcs] & BRANCH_TARGET) == 0) {
					bytecodeMap[pcs] |= BRANCH_TARGET;
					count++;
				}
				handler++;
			}
		}
	}
	Trc_BCV_buildBranchMap_branchCount(verifyData->vmStruct,
			(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
			J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
			(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(romMethod)),
			J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)),
			(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(romMethod)),
			J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(romMethod)),
			count);

	return count;
}



/*
	Convert the StackMap Attribute maps to internal uncompressed stackmaps.

	Returns
		BCV_SUCCESS on success,
		BCV_FAIL on verification error
*/
static IDATA 
decompressStackMaps (J9BytecodeVerificationData * verifyData, IDATA localsCount, U_8 * stackMapData)
{
	J9ROMMethod *romMethod = verifyData->romMethod;
	UDATA maxStack = J9_MAX_STACK_FROM_ROM_METHOD(romMethod);
	IDATA maxLocals = J9_ARG_COUNT_FROM_ROM_METHOD(romMethod) + J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod);
	UDATA length = (UDATA) J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);
	UDATA i;
	IDATA rc = BCV_SUCCESS;
	J9BranchTargetStack *liveStack = (J9BranchTargetStack *)verifyData->liveStack;
	J9BranchTargetStack *branchTargetStack = BCV_FIRST_STACK();
	U_8 mapType;
	UDATA mapPC = -1;
	UDATA temp;
	UDATA start = 0; /* Used in BUILD_VERIFY_ERROR */
	UDATA mapIndex = 0;
	UDATA errorModule = J9NLS_BCV_ERR_NO_ERROR__MODULE; /* default to BCV NLS catalog */

	Trc_BCV_decompressStackMaps_Entry(verifyData->vmStruct, localsCount);
	/* localsCount records the current locals depth as all stack maps (except full frame) are relative to the previous frame */
	for (i = 0; i < (UDATA) verifyData->stackMapsCount; i++) {
		IDATA localDelta = 0;
		UDATA stackCount = 0;
		
		NEXT_U8(mapType, stackMapData);
		mapPC++;

		if (mapType < CFR_STACKMAP_SAME_LOCALS_1_STACK) {
			/* Same frame 0-63 */
			mapPC += (UDATA) mapType;
			/* done */

		} else if (mapType < CFR_STACKMAP_SAME_LOCALS_1_STACK_END) {
			/* Same with one stack entry frame 64-127 */
			mapPC += (UDATA) ((UDATA) mapType - CFR_STACKMAP_SAME_LOCALS_1_STACK);
			stackCount = 1;

		} else {
			mapPC += NEXT_U16(temp, stackMapData);

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
				localDelta = NEXT_U16(temp, stackMapData);
				localsCount = 0;
			}
		}

		localsCount = parseLocals (verifyData, &stackMapData, liveStack, localDelta, localsCount, maxLocals);
		if (localsCount < 0) {
			BUILD_VERIFY_ERROR (errorModule, J9NLS_BCV_ERR_INCONSISTENT_STACK__ID);
			/* Jazz 82615: Set the pc value of the current stackmap frame to show up in the error message frame work */
			liveStack->pc = mapPC;
			verifyData->errorPC = mapPC;

			Trc_BCV_decompressStackMaps_LocalsArrayOverFlowUnderFlow(verifyData->vmStruct,
					(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
					J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
					(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(romMethod)),
					J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)),
					(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(romMethod)),
					J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(romMethod)),
					i, mapPC);
			rc = BCV_FAIL;
			break;
		}

		if (mapType == CFR_STACKMAP_FULL) {
			stackCount = NEXT_U16(temp, stackMapData);
		}

		if (BCV_SUCCESS != parseStack (verifyData, &stackMapData, liveStack, stackCount, maxStack)) {
			BUILD_VERIFY_ERROR (errorModule, J9NLS_BCV_ERR_INCONSISTENT_STACK__ID);
			/* Jazz 82615: Set the pc value of the current stackmap frame to show up in the error message frame work */
			liveStack->pc = mapPC;
			verifyData->errorPC = mapPC;

			Trc_BCV_decompressStackMaps_StackArrayOverFlow(verifyData->vmStruct,
					(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
					J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
					(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(romMethod)),
					J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)),
					(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(romMethod)),
					J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(romMethod)),
					i, mapPC);
			rc = BCV_FAIL;
			break;
		}

		if (mapPC >= length) {
			/* should never get here - caught in staticverify.c checkStackMap */
			BUILD_VERIFY_ERROR (errorModule, J9NLS_BCV_ERR_INCONSISTENT_STACK__ID);
			Trc_BCV_decompressStackMaps_MapOutOfRange(verifyData->vmStruct,
					(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
					J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
					(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(romMethod)),
					J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)),
					(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(romMethod)),
					J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(romMethod)),
					i, mapPC, length);
			rc = BCV_FAIL;
			break;
		}

		(verifyData->bytecodeMap)[mapPC] |= (mapIndex << BRANCH_INDEX_SHIFT) | BRANCH_TARGET;
		mapIndex++;

		copyStack (liveStack, branchTargetStack);
		branchTargetStack->pc = mapPC;
		/* Point to the next stack */
		branchTargetStack = BCV_NEXT_STACK (branchTargetStack);
	}
	
	Trc_BCV_decompressStackMaps_Exit(verifyData->vmStruct, rc);
	return rc;
}



/* Specifically returns BCV_ERR_INTERNAL_ERROR for failure */

static IDATA
parseLocals (J9BytecodeVerificationData * verifyData, U_8** stackMapData, J9BranchTargetStack * liveStack, IDATA localDelta, IDATA localsCount, IDATA maxLocals)
{
	UDATA i;
	UDATA stackEntry;
	UDATA unusedLocals;

	if (localDelta < 0) {
		/* Clear the chopped elements */
		for (;localDelta; localDelta++) {
			localsCount--;
			if (localsCount < 0) {
				goto _underflow;
			}
			liveStack->stackElements[localsCount] = BCV_BASE_TYPE_TOP;

			/* Check long/double type as long as there still remains local variables
			 * in the stackmap frame.
			 */
			if (localsCount > 0) {
				/* Possibly remove a double or long (counts as 1 local, but two slots).
				 * A double or a long is pushed as <top, double|long>
				 */
				stackEntry = liveStack->stackElements[localsCount - 1];
				if ((BCV_BASE_TYPE_DOUBLE == stackEntry) || (BCV_BASE_TYPE_LONG == stackEntry)) {
					localsCount--;
					if (localsCount < 0) {
						goto _underflow;
					}
					liveStack->stackElements[localsCount] = BCV_BASE_TYPE_TOP;
				}
			}
		}

	} else {
		for (;localDelta; localDelta--) {
			stackEntry = parseElement (verifyData, stackMapData);
			if (localsCount >= maxLocals) {
				/* Overflow */
				goto _overflow;
			}
			liveStack->stackElements[localsCount++] = stackEntry;
			if ((BCV_BASE_TYPE_DOUBLE == stackEntry) || (BCV_BASE_TYPE_LONG == stackEntry)) {
				if (localsCount >= maxLocals) {
					/* Overflow */
					goto _overflow;
				}
				liveStack->stackElements[localsCount++] = BCV_BASE_TYPE_TOP;
			}
		}

		/* Clear the remaining locals */
		unusedLocals = liveStack->stackBaseIndex - localsCount;
		for (i = localsCount; i < (unusedLocals + localsCount); i++) {
			liveStack->stackElements[i] = BCV_BASE_TYPE_TOP;
		}
	}

	return localsCount;

_underflow:
	/* Jazz 82615: Set the error code in the case of underflow on 'locals' in the current stackmap frame */
	verifyData->errorDetailCode = BCV_ERR_STACKMAP_FRAME_LOCALS_UNDERFLOW;

	Trc_BCV_parseLocals_LocalsArrayUnderFlow(verifyData->vmStruct,
		(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
		J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
		(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
		J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
		(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
		J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)));
	return BCV_ERR_INTERNAL_ERROR;

_overflow:
	/* Jazz 82615: Set the error code, the location of the last local variable allowed on 'locals'
	 * and the maximum local size in the case of overflow on 'locals' in the currrent stackmap frame.
	 */
	verifyData->errorDetailCode = BCV_ERR_STACKMAP_FRAME_LOCALS_OVERFLOW;
	verifyData->errorCurrentFramePosition = (maxLocals > 0) ? (U_32)(maxLocals - 1) : 0;
	verifyData->errorTempData = (UDATA)maxLocals;

	Trc_BCV_parseLocals_LocalsArrayOverFlow(verifyData->vmStruct,
		(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
		J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
		(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
		J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
		(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
		J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)));
	return BCV_ERR_INTERNAL_ERROR;
}


/*
 * returns BCV_SUCCESS on success
 * returns BCV_ERR_INTERNAL_ERROR on failure
 */

static IDATA
parseStack (J9BytecodeVerificationData * verifyData, U_8** stackMapData, J9BranchTargetStack * liveStack, UDATA stackCount, UDATA maxStack)
{
	UDATA stackEntry;
	UDATA* stackTop = RELOAD_STACKBASE(liveStack); /* Clears the stack */
	UDATA* stackBase = stackTop;

	for (;stackCount; stackCount--) {
		stackEntry = parseElement (verifyData, stackMapData);
		if ((UDATA) (stackTop - stackBase) >= maxStack) {
			/* Jazz 82615: Set the error code and the location of wrong data type on 'stack' (only keep the maximum size for stack) */
			verifyData->errorDetailCode = BCV_ERR_STACKMAP_FRAME_STACK_OVERFLOW;
			verifyData->errorCurrentFramePosition = (U_32)(stackBase - liveStack->stackElements);
			if (maxStack > 0) {
				verifyData->errorCurrentFramePosition += (U_32)(maxStack - 1);
			}
			verifyData->errorTempData = maxStack;
			return BCV_ERR_INTERNAL_ERROR;
		}
		PUSH(stackEntry);
		if ((stackEntry == BCV_BASE_TYPE_DOUBLE) || (stackEntry == BCV_BASE_TYPE_LONG)) {
			if ((UDATA) (stackTop - stackBase) >= maxStack) {
				/* Jazz 82615: Set the error code and the location of wrong data type on 'stack' (only keep the maximum size for stack) */
				verifyData->errorDetailCode = BCV_ERR_STACKMAP_FRAME_STACK_OVERFLOW;
				verifyData->errorCurrentFramePosition = (U_32)(stackBase - liveStack->stackElements);
				if (maxStack > 0) {
					verifyData->errorCurrentFramePosition += (U_32)(maxStack - 1);
				}
				verifyData->errorTempData = maxStack;
				return BCV_ERR_INTERNAL_ERROR;
			}
			PUSH(BCV_BASE_TYPE_TOP);
		}
	}

	SAVE_STACKTOP(liveStack, stackTop);
	return BCV_SUCCESS;
}


/*
 * returns stackEntry
 * No error path in this function
 */

static UDATA
parseElement (J9BytecodeVerificationData * verifyData, U_8** stackMapData)
{
	J9ROMClass * romClass = verifyData->romClass; 
	U_8 entryType;
	U_8 *mapData = *stackMapData;
	U_16 cpIndex;
	UDATA stackEntry;

	NEXT_U8(entryType, mapData);

	if (entryType < CFR_STACKMAP_TYPE_INIT_OBJECT) {
		/* return primitive type */
		stackEntry = verificationTokenDecode[entryType];

	} else if (entryType == CFR_STACKMAP_TYPE_INIT_OBJECT) {
		J9ROMMethod *romMethod = verifyData->romMethod;
		J9UTF8* className = J9ROMCLASS_CLASSNAME(romClass);
		stackEntry = convertClassNameToStackMapType(verifyData, J9UTF8_DATA(className), J9UTF8_LENGTH(className), BCV_SPECIAL_INIT, 0);

	} else if (entryType == CFR_STACKMAP_TYPE_OBJECT) {
		J9UTF8 *utf8string;
		J9ROMConstantPoolItem *constantPool = J9_ROM_CP_FROM_ROM_CLASS(romClass);

		NEXT_U16(cpIndex, mapData);
		utf8string = J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) (&constantPool[cpIndex]));
		pushClassType(verifyData, utf8string, &stackEntry);
		
	} else if (entryType == CFR_STACKMAP_TYPE_NEW_OBJECT) {
		NEXT_U16(cpIndex, mapData);
		stackEntry = BCV_SPECIAL_NEW | (((UDATA) cpIndex) << BCV_CLASS_INDEX_SHIFT);

	} else {
		/* Primitive arrays */
		U_16 arity;

		stackEntry = (UDATA) verificationTokenDecode[entryType];
		NEXT_U16(arity, mapData);
		stackEntry |= (((UDATA) arity) << BCV_ARITY_SHIFT);
	}
	
	*stackMapData = mapData;
	return stackEntry;
}
 


static void 
copyStack (J9BranchTargetStack *source, J9BranchTargetStack *destination) 
{
	UDATA pc = destination->pc;

	memcpy((UDATA *) destination, (UDATA *) source, (source->stackTopIndex + BCV_TARGET_STACK_HEADER_UDATA_SIZE) * sizeof(UDATA));

	destination->pc = pc;
}


/* returns
 *  BCV_SUCCESS : no merge necessary
 * 	BCV_FAIL  : cause a rewalk
 * 	BCV_ERR_INSUFFICIENT_MEMORY    : OOM - no rewalk
 */
static IDATA 
mergeObjectTypes (J9BytecodeVerificationData *verifyData, UDATA sourceType, UDATA * targetTypePointer)
{ 
	J9ROMClass * romClass = verifyData->romClass;
	UDATA targetType = *targetTypePointer;
	UDATA sourceIndex, targetIndex;  
	J9UTF8 *name;
	UDATA classArity, targetArity, classIndex;
	IDATA rc = BCV_SUCCESS;
	U_8 *sourceName, *targetName;
	UDATA sourceLength, targetLength;
	U_32 *offset;
	IDATA reasonCode = 0;

	/* assume that sourceType and targetType are not equal */

	/* if target is more general than source, then its fine */
	rc =  isClassCompatible( verifyData, sourceType, targetType, &reasonCode ) ;

	if (TRUE == rc) {
		return BCV_SUCCESS;	/* no merge required */
	} else { /* FALSE == rc */
		/* VM error, no need to continue, return appropriate rc */
		if (BCV_ERR_INTERNAL_ERROR == reasonCode) {
			*targetTypePointer = (U_32)BCV_JAVA_LANG_OBJECT_INDEX << BCV_CLASS_INDEX_SHIFT;
			Trc_BCV_mergeObjectTypes_UnableToLoadClass(verifyData->vmStruct,
				(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
				sourceType, targetType);
			return (IDATA) BCV_FAIL;
		} else if (BCV_ERR_INSUFFICIENT_MEMORY == reasonCode) {
			Trc_BCV_mergeObjectTypes_MergeClasses_OutOfMemoryException(verifyData->vmStruct,
				(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)));
			return BCV_ERR_INSUFFICIENT_MEMORY;
		}
	}

	/* Types were not compatible, thus target is not equal or more general than source */

	/* NULL always loses to objects */
	if (targetType == BCV_BASE_TYPE_NULL) {
		Trc_BCV_mergeObjectTypes_NullTargetOverwritten(verifyData->vmStruct,
				(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
				sourceType);
		*targetTypePointer = sourceType;
		/* cause a re-walk */
		return (IDATA) BCV_FAIL;
	}

	/* if the source or target are base type arrays, decay them to object arrays of arity n-1 (or just Object) */
	/* Base arrays already have an implicit arity of 1, so just keep the arity for object */
	if (sourceType & BCV_TAG_BASE_ARRAY_OR_NULL) {
		Trc_BCV_mergeObjectTypes_DecaySourceArray(verifyData->vmStruct,
				(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
				sourceType);
		sourceType = (sourceType & BCV_ARITY_MASK) | ((U_32)BCV_JAVA_LANG_OBJECT_INDEX << BCV_CLASS_INDEX_SHIFT);
	}

	if (targetType & BCV_TAG_BASE_ARRAY_OR_NULL) {
		Trc_BCV_mergeObjectTypes_DecayTargetArray(verifyData->vmStruct,
				(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
				targetType);
		targetType = (targetType & BCV_ARITY_MASK) | ((U_32)BCV_JAVA_LANG_OBJECT_INDEX << BCV_CLASS_INDEX_SHIFT);
	}

	classArity = sourceType & BCV_ARITY_MASK;
	targetArity = targetType & BCV_ARITY_MASK;

	if (classArity == targetArity) {
		/* Find the common parent class if the same arity */

		sourceIndex = (sourceType & BCV_CLASS_INDEX_MASK) >> BCV_CLASS_INDEX_SHIFT;
		targetIndex = (targetType & BCV_CLASS_INDEX_MASK) >> BCV_CLASS_INDEX_SHIFT;

		offset = (U_32 *) verifyData->classNameList[sourceIndex];
		sourceLength = (UDATA) J9UTF8_LENGTH(offset + 1);

		if (offset[0] == 0) {
			sourceName = J9UTF8_DATA(offset + 1);

		} else {
			sourceName = (U_8 *) ((UDATA) offset[0] + (UDATA) romClass);
		}

		offset = (U_32 *) verifyData->classNameList[targetIndex];
		targetLength = (UDATA) J9UTF8_LENGTH(offset + 1);

		if (offset[0] == 0) {
			targetName = J9UTF8_DATA(offset + 1);

		} else {
			targetName = (U_8 *) ((UDATA) offset[0] + (UDATA) romClass);
		}

 		name = mergeClasses(verifyData, sourceName, sourceLength, targetName, targetLength, &reasonCode);

 		if (NULL == name) {
 			if (BCV_ERR_INSUFFICIENT_MEMORY == reasonCode) {
 				Trc_BCV_mergeObjectTypes_MergeClasses_OutOfMemoryException(verifyData->vmStruct,
					(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
					J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
					(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
					J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
					(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
					J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)));
 				return BCV_ERR_INSUFFICIENT_MEMORY;
 			} else {
 				Trc_BCV_mergeObjectTypes_MergeClassesFail(verifyData->vmStruct,
					(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
					J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
					(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
					J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
					(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
					J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
					sourceLength, sourceName, targetLength, targetName);
 				*targetTypePointer = sourceType;
 				/* cause a re-walk */
 				return (IDATA) BCV_FAIL;
 			}
 		}

		classIndex = findClassName( verifyData, J9UTF8_DATA(name), J9UTF8_LENGTH(name) );
		Trc_BCV_mergeObjectTypes_MergeClassesSucceed(verifyData->vmStruct,
				(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
				sourceLength, sourceName, targetLength, targetName, J9UTF8_LENGTH(name), J9UTF8_DATA(name), classIndex);

	} else {

		/* Different arity means common parent class is the minimum arity of class Object */
		classIndex = BCV_JAVA_LANG_OBJECT_INDEX;

		Trc_BCV_mergeObjectTypes_MergeClassesMinimumArity(verifyData->vmStruct,
				(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
				classArity, targetArity);
		/* Find minimum common arity of arrays */ 
		if( targetArity < classArity ) {
			classArity = targetArity;
		}
	}

	/* slam new type into targetTypePointer */
	*targetTypePointer = classArity | ( classIndex << BCV_CLASS_INDEX_SHIFT );
	Trc_BCV_mergeObjectTypes_MergedClass(verifyData->vmStruct,
			(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
			J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
			(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
			J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
			(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
			J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
			*targetTypePointer);
	/* cause a re-walk */
	return (IDATA) BCV_FAIL;
}

/*
 * returns BCV_SUCCESS on success
 * returns BCV_FAIL on failure
 * returns BCV_ERR_INSUFFICIENT_MEMORY on OOM */
static IDATA 
mergeStacks (J9BytecodeVerificationData * verifyData, UDATA target)
{
	J9ROMClass *romClass = verifyData->romClass;
	J9ROMMethod *romMethod = verifyData->romMethod;
	UDATA maxIndex = J9_ARG_COUNT_FROM_ROM_METHOD(romMethod) + J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod);
	U_32 *bytecodeMap = verifyData->bytecodeMap;
	UDATA i = 0;
	UDATA stackIndex;
	IDATA rewalk = FALSE;
	IDATA rc = BCV_SUCCESS;
	UDATA *targetStackPtr, *targetStackTop, *sourceStackPtr, *sourceStackTop, *sourceStackTemps;
	J9BranchTargetStack *liveStack = (J9BranchTargetStack *) verifyData->liveStack;
	J9BranchTargetStack *targetStack;


	stackIndex = bytecodeMap[target] >> BRANCH_INDEX_SHIFT;
	targetStack = BCV_INDEX_STACK (stackIndex);

	Trc_BCV_mergeStacks_Entry(verifyData->vmStruct,
			(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
			J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
			(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
			J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
			(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
			J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
			target, target);

	if (targetStack->stackBaseIndex == -1) {

		/* Target location does not have a stack, so give the target our current stack */
		copyStack(liveStack, targetStack);
		verifyData->unwalkedQueue[verifyData->unwalkedQueueTail++] = target;
		verifyData->unwalkedQueueTail %= (verifyData->rootQueueSize / sizeof(UDATA));
		bytecodeMap[target] |= BRANCH_ON_UNWALKED_QUEUE;
		Trc_BCV_mergeStacks_CopyStack(verifyData->vmStruct,
				(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
				stackIndex, target, target);
		goto _finished;

	} else {
		UDATA mergePC = (UDATA) -1;
		U_32 resultArrayBase;

		/* Check stack size equality */
		if (targetStack->stackTopIndex != liveStack->stackTopIndex) {
			rc = BCV_FAIL;
			Trc_BCV_mergeStacks_DepthMismatch(verifyData->vmStruct,
					(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
					J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
					(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
					J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
					(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
					J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
					stackIndex, target, target,
					liveStack->stackTopIndex, targetStack->stackTopIndex);
			goto _finished;
		}

		/* Now we have to merge stacks */
		targetStackPtr = targetStack->stackElements;
		targetStackTop =  RELOAD_STACKTOP(targetStack);
		sourceStackPtr = liveStack->stackElements;
		sourceStackTop = RELOAD_STACKTOP(liveStack);

		/* remember where the temps end */
		sourceStackTemps = RELOAD_STACKBASE(liveStack);

		Trc_BCV_mergeStacks_MergeStacks(verifyData->vmStruct,
				(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
				stackIndex, target, target);

		while (sourceStackPtr != sourceStackTop) {

			/* Merge if the source and target slots are not identical */
			if (*sourceStackPtr != *targetStackPtr) {
				UDATA sourceItem = *sourceStackPtr;
				UDATA targetItem = *targetStackPtr;

				/* Merge in the locals */
				if (sourceStackPtr < sourceStackTemps ) {

					/* Merge when either the source or target not an object */
					if ((sourceItem | targetItem) & (BCV_BASE_OR_SPECIAL)) {

						/* Mismatch results in undefined local - rewalk if modified stack */
						if (*targetStackPtr != (UDATA) (BCV_BASE_TYPE_TOP)) {
							*targetStackPtr = (UDATA) (BCV_BASE_TYPE_TOP);
							rewalk = TRUE;
						}

					/* Merge two objects */
					} else {

						/* extra checks here to avoid calling local mapper unnecessarily */
						/* Null source or java/lang/Object targets always work trivially */
						if ((*sourceStackPtr != BCV_BASE_TYPE_NULL) && (*targetStackPtr != (BCV_JAVA_LANG_OBJECT_INDEX << BCV_CLASS_INDEX_SHIFT))) {

							/* Null target always causes a re-walk - source is never null here */
							if (*targetStackPtr == BCV_BASE_TYPE_NULL) {
								*targetStackPtr = *sourceStackPtr;
								rewalk = TRUE;
							} else {

								/* Use local mapper to check merge necessity in locals */
								if ((verifyData->verificationFlags & J9_VERIFY_OPTIMIZE) && (maxIndex <= 32)) {
									/* Only handle 32 locals or less */
									UDATA index = (UDATA) (sourceStackPtr - liveStack->stackElements);

									/* Reuse map in this merge if needed for multiple merges at same map */
									if (mergePC == ((UDATA) -1)) {
										mergePC = target;
										if (j9localmap_LocalBitsForPC(verifyData->portLib, romClass, romMethod, mergePC, &resultArrayBase, NULL, NULL, NULL) != 0) {
											/* local map error - force a full merge */
											resultArrayBase = (U_32) -1;
										} 
									}

									if (resultArrayBase & (1 << index)) {
										UDATA origSource = *sourceStackPtr;
										UDATA origTarget = *targetStackPtr;
										IDATA tempRC = mergeObjectTypes(verifyData, *sourceStackPtr, targetStackPtr);

										/* Merge the objects - the result is live */
										if (BCV_FAIL == tempRC) {
											rewalk = TRUE;
										} else if (BCV_ERR_INSUFFICIENT_MEMORY == tempRC) {
											rc = BCV_ERR_INSUFFICIENT_MEMORY;
											goto _finished;
										}

										Trc_BCV_mergeStacks_OptMergeRequired(verifyData->vmStruct,
												(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
												J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
												(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
												J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
												(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
												J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
												origSource, origTarget, *targetStackPtr);

									} else {
										Trc_BCV_mergeStacks_OptMergeNotRequired(verifyData->vmStruct,
												(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
												J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
												(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
												J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
												(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
												J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
												*sourceStackPtr, *targetStackPtr);
										/* Tag undefined - local variable is dead */
										*targetStackPtr = (UDATA) (BCV_BASE_TYPE_TOP);
										rewalk = TRUE;
									}

								} else {
									IDATA tempRC =  mergeObjectTypes(verifyData, *sourceStackPtr, targetStackPtr);
									if (BCV_FAIL == tempRC) {
										rewalk = TRUE;
									} else if (BCV_ERR_INSUFFICIENT_MEMORY == tempRC) {
										rc = BCV_ERR_INSUFFICIENT_MEMORY;
										goto _finished;
									}
								}
							}
						}
					}

				/* Merge is on the stack */
				} else {

					if (!((sourceItem | targetItem) & BCV_BASE_OR_SPECIAL)) {
						/* Merge two objects */
						IDATA tempRC = mergeObjectTypes(verifyData, *sourceStackPtr, targetStackPtr);
						if (BCV_FAIL == tempRC) {
							rewalk = TRUE;
						} else if (BCV_ERR_INSUFFICIENT_MEMORY == tempRC) {
							rc = BCV_ERR_INSUFFICIENT_MEMORY;
							goto _finished;
						}
					}
				}
			}
			sourceStackPtr++;
			targetStackPtr++;
		}
	}

	/* add to the root set if we changed the target stack */
	if (rewalk) {
		if (!(bytecodeMap[target] & BRANCH_ON_REWALK_QUEUE)) {
			Trc_BCV_mergeStacks_QueueForRewalk(verifyData->vmStruct,
					(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
					J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
					(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
					J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
					(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
					J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
					target, target);
			verifyData->rewalkQueue[verifyData->rewalkQueueTail++] = target;
			verifyData->rewalkQueueTail %= (verifyData->rootQueueSize / sizeof(UDATA));
			bytecodeMap[target] |= BRANCH_ON_REWALK_QUEUE;
			bytecodeMap[target] &= ~BRANCH_ON_UNWALKED_QUEUE;
		}
	}

_finished:

	Trc_BCV_mergeStacks_Exit(verifyData->vmStruct,
			(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
			J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
			(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
			J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
			(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
			J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
			rc);

	return rc;
}





#ifdef DEBUG_BCV
static void 
printMethod (J9BytecodeVerificationData * verifyData)
{
	J9ROMClass *romClass = verifyData->romCLass;
	J9ROMMethod *method = verifyData->romMethod;
	U_8* string;
#if 0
	J9CfrAttributeExceptions* exceptions;
#endif
	IDATA arity, i, j; 
 
	string = J9UTF8_DATA(J9ROMCLASS_CLASSNAME(romClass));
	printf("<");
	for( i=0; i< J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass)); i++ )
	{
		printf( "%c", (string[i] == '/')?'.':string[i]);
	}
	printf(">");

	if( strncmp( string, "java/util/Arrays", i ) == 0 ) {
		printf("stop");
	}
  
	/* Return type. */
	string = J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(method));
	i = 0;
	while(string[i++] != ')');
	arity = 0;
	while(string[i] == '[') arity++, i++;
	switch(string[i])
	{
		case 'B':
			printf( "byte");
			break;

		case 'C':
			printf( "char");
			break;

		case 'D':
			printf( "double");
			break;

		case 'F':
			printf( "float");
			break;

		case 'I':
			printf( "int");
			break;

		case 'J':
			printf( "long");
			break;

		case 'L':
			i++;
			while(string[i] != ';')
			{
				printf( "%c", (string[i] == '/')?'.':string[i]);
				i++;
			}
			break;

		case 'S':
			printf( "short");
			break;

		case 'V':
			printf( "void");
			break;

		case 'Z':
			printf( "boolean");
			break;
	}
	for(i = 0; i < arity; i++)
		printf( "[]");

	printf( " %.*s(", J9UTF8_LENGTH(J9ROMMETHOD_NAME(method)), J9UTF8_DATA(J9ROMMETHOD_NAME(method)));

	for(i = 1; string[i] != ')'; i++)
	{
		arity = 0;
		while(string[i] == '[') arity++, i++;
		switch(string[i])
		{
			case 'B':
				printf( "byte");
				break;

			case 'C':
				printf( "char");
				break;

			case 'D':
				printf( "double");
				break;

			case 'F':
				printf( "float");
				break;

			case 'I':
				printf( "int");
				break;

			case 'J':
				printf( "long");
				break;
		
			case 'L':
				i++;
				while(string[i] != ';')
				{
					printf( "%c", (string[i] == '/')?'.':string[i]);
					i++;
				}
				break;

			case 'S':
				printf( "short");
				break;

			case 'V':
				printf( "void");
				break;

			case 'Z':
				printf( "boolean");
				break;
		}
		for(j = 0; j < arity; j++)
			printf( "[]");

		if(string[i + 1] != ')')
			printf( ", ");
	}

	printf( ")");
#if 0	/* need to fix this code to work with J9ROMMethods.. */
	for(i = 0; i < method->attributesCount; i++)
	{
		if(method->attributes[i]->tag == CFR_ATTRIBUTE_Exceptions)
		{
			exceptions = (J9CfrAttributeExceptions*)method->attributes[i];

			printf( " throws ");
			for(j = 0; j < exceptions->numberOfExceptions - 1; j++)
			{
				if(exceptions->exceptionIndexTable[j] != 0)
				{
					index = classfile->constantPool[exceptions->exceptionIndexTable[j]].slot1;
					string = classfile->constantPool[index].bytes;
					while(*string)
					{
						printf( "%c", (*string == '/')?'.':*string);
						string++;
					}
					printf( ", ");
				}
			}
			index = classfile->constantPool[exceptions->exceptionIndexTable[j]].slot1;
			string = classfile->constantPool[index].bytes;
			while(*string)
			{
				printf( "%c", (*string == '/')?'.':*string);
				string++;
			}

			i = method->attributesCount;
		}
	}	
#endif
	printf( ";\n");
	return;
}


#endif

/*
 * return BCV_SUCCESS on success
 * returns BCV_ERR_INTERNAL_ERROR on any errors
 * returns BCV_ERR_INSUFFICIENT_MEMORY on OOM

*/

static IDATA 
simulateStack (J9BytecodeVerificationData * verifyData)
{

#define CHECK_END \
	if(pc > length) { \
		errorType = J9NLS_BCV_ERR_UNEXPECTED_EOF__ID;	\
		verboseErrorCode = BCV_ERR_UNEXPECTED_EOF;	\
		goto _verifyError; \
	}

	J9ROMClass * romClass = verifyData->romClass;
	J9ROMMethod * romMethod = verifyData->romMethod;
	J9BranchTargetStack * liveStack = (J9BranchTargetStack *) verifyData->liveStack;
	U_32 * bytecodeMap = verifyData->bytecodeMap;
	IDATA start = 0;
	UDATA pc = 0;
	UDATA length, index, target;
	J9ROMConstantPoolItem *constantPool;
	J9ROMConstantPoolItem *info;
	J9UTF8 *utf8string;
	U_8 *code;
	U_8 *bcIndex;
	UDATA bc = 0;
	UDATA popCount, type1, type2, action;
	UDATA type, temp1, temp2, temp3, maxStack;
	UDATA justLoadedStack = FALSE;
	UDATA *stackBase;
	UDATA *stackTop;
	UDATA *temps;
	UDATA stackIndex;
	UDATA wideIndex = FALSE;
	UDATA *ptr;
	IDATA i1, i2;
	UDATA classIndex;
	UDATA errorModule = J9NLS_BCV_ERR_NO_ERROR__MODULE; /* default to BCV NLS catalog */
	U_16 errorType;
	I_16 offset16;
	I_32 offset32;
	J9BranchTargetStack * branch;
	UDATA checkIfInsideException = romMethod->modifiers & J9AccMethodHasExceptionInfo;
	UDATA tempStoreChange;
	J9ExceptionInfo *exceptionData = J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod);
	J9ExceptionHandler *handler;
	UDATA exception;
	J9SRP *callSiteData = (J9SRP *) J9ROMCLASS_CALLSITEDATA(romClass);
	UDATA* originalStackTop;
	UDATA originalStackZeroEntry;
	/* Jazz 104084: Initialize verification error codes by default */
	IDATA verboseErrorCode = 0;
	UDATA errorTargetType = (UDATA)-1;
	UDATA errorStackIndex = (UDATA)-1;
	UDATA errorTempData = (UDATA)-1;

	Trc_BCV_simulateStack_Entry(verifyData->vmStruct);

#ifdef DEBUG_BCV
	printMethod(verifyData);
#endif

	verifyData->unwalkedQueueHead = 0;
	verifyData->unwalkedQueueTail = 0;
	verifyData->rewalkQueueHead = 0;
	verifyData->rewalkQueueTail = 0;

	code = J9_BYTECODE_START_FROM_ROM_METHOD(romMethod);
	length = (UDATA) J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);
	maxStack = J9_MAX_STACK_FROM_ROM_METHOD(romMethod);

	/* Jazz 105041: Initialize the 1st data slot on 'stack' with 'top' (placeholdler)
	 * to avoid storing garbage data type in the error message buffer
	 * when stack underflow occurs.
	 */
	liveStack->stackElements[liveStack->stackBaseIndex] = BCV_BASE_TYPE_TOP;

	RELOAD_LIVESTACK;

	bcIndex = code;

	constantPool = J9_ROM_CP_FROM_ROM_CLASS(romClass);

	while (pc < length) {
		if ((UDATA) (stackTop - stackBase) > maxStack) {
			errorType = J9NLS_BCV_ERR_STACK_OVERFLOW__ID;
			verboseErrorCode = BCV_ERR_STACK_OVERFLOW;
			SAVE_STACKTOP(liveStack, stackTop);
			goto _verifyError;
		}

		/* If exception start PC, or possible branch to inside an exception range, */
		/*  copy the existing stack shape into the exception stack */
		if ((bytecodeMap[pc] & BRANCH_EXCEPTION_START) || (justLoadedStack && checkIfInsideException)) {
			handler = J9EXCEPTIONINFO_HANDLERS(exceptionData);
			SAVE_STACKTOP(liveStack, stackTop);

			/* Save the current liveStack element zero */
			/* Reset the stack pointer to push the exception on the empty stack */
			originalStackTop = stackTop;
			originalStackZeroEntry = liveStack->stackElements[liveStack->stackBaseIndex];

			for (exception = 0; exception < (UDATA) exceptionData->catchCount; exception++) {

				/* find the matching branch target, and copy/merge the stack with the exception object */
				if ((pc >= handler->startPC) && (pc < handler->endPC)) {
#ifdef DEBUG_BCV
					printf("exception startPC: %d\n", handler->startPC);
#endif
					stackIndex = bytecodeMap[handler->handlerPC] >> BRANCH_INDEX_SHIFT;
					branch = BCV_INDEX_STACK (stackIndex);

					/* "push" the exception object */
					classIndex = BCV_JAVA_LANG_THROWABLE_INDEX;
					if (handler->exceptionClassIndex) {
						/* look up the class in the constant pool */
						utf8string = J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *)(&constantPool [handler->exceptionClassIndex]));
						classIndex = findClassName(verifyData, J9UTF8_DATA(utf8string), J9UTF8_LENGTH(utf8string));
					}

					/* Empty the stack */
					stackTop = &(liveStack->stackElements[liveStack->stackBaseIndex]);
					PUSH(classIndex << BCV_CLASS_INDEX_SHIFT);
					SAVE_STACKTOP(liveStack, stackTop);

					if (BCV_ERR_INSUFFICIENT_MEMORY == mergeStacks (verifyData, handler->handlerPC)) {
						errorType = J9NLS_BCV_ERR_VERIFY_OUT_OF_MEMORY__ID;
						goto _outOfMemoryError;
					}
				}
				handler++;
			}

			/* Restore liveStack */
			liveStack->stackElements[liveStack->stackBaseIndex] = originalStackZeroEntry;
			stackTop = originalStackTop;
		}

		start = (IDATA) pc;

		/* Merge all branchTargets encountered */	
		if (bytecodeMap[pc] & BRANCH_TARGET) {
			/* Don't try to merge a stack we just loaded */
			if (!justLoadedStack) {
				SAVE_STACKTOP(liveStack, stackTop);
				if (BCV_ERR_INSUFFICIENT_MEMORY == mergeStacks (verifyData, start)) {
					errorType = J9NLS_BCV_ERR_VERIFY_OUT_OF_MEMORY__ID;
					goto _outOfMemoryError;
				}
				goto _checkFinished;
			}
		}
		justLoadedStack = FALSE;
		
		bcIndex = code + pc;
		bc = *bcIndex;
#ifdef DEBUG_BCV
#if 1							/* for really verbose logging */
		printf("pc: %d bc: %d\n", pc, bc);
#endif
#endif
		pc += (J9JavaInstructionSizeAndBranchActionTable[bc] & 7);
		CHECK_END;

		popCount = JavaStackActionTable[bc] & 0x07;
		if ((stackTop - popCount) < stackBase) {
			errorType = J9NLS_BCV_ERR_STACK_UNDERFLOW__ID;
			verboseErrorCode = BCV_ERR_STACK_UNDERFLOW;
			/* Given that the actual data type involved has not yet been located through pop operation 
			 * when stack underflow occurs, it needs to step back by 1 slot to the actual data type 
			 * to be manipulated by the opcode.
			 */
			errorStackIndex = (U_32)(stackTop - liveStack->stackElements - 1);
			/* Always set to the location of the 1st data type on 'stack' to show up if stackTop <= stackBase */
			if (stackTop <= stackBase) {
				errorStackIndex = (U_32)(stackBase - liveStack->stackElements);
			}
			goto _verifyError;
		}

		type1 = (UDATA) J9JavaBytecodeVerificationTable[bc];
		action = type1 >> 8;
		type2 = (type1 >> 4) & 0xF;
		type1 = (UDATA) decodeTable[type1 & 0xF];
		type2 = (UDATA) decodeTable[type2];

		switch (action) {
		case RTV_NOP:
		case RTV_INCREMENT:
			break;

		case RTV_WIDE_LOAD_TEMP_PUSH:
			if (type1 == BCV_GENERIC_OBJECT) {
				/* Only set for wide Objects - primitives don't read temps */
				wideIndex = TRUE;
			}	/* Fall through case !!! */
			
		case RTV_LOAD_TEMP_PUSH:
			if (type1 == BCV_GENERIC_OBJECT) {
				/* aload family */
				index = type2 & 0x7;
				if (type2 == 0) {
					index = PARAM_8(bcIndex, 1);
					if (wideIndex) {
						index = PARAM_16(bcIndex, 1);
						wideIndex = FALSE;
					}
				}
				type1 = temps[index];
				PUSH(type1);
				break;
			}	/* Fall through case !!! */

		case RTV_PUSH_CONSTANT:
		
		_pushConstant:
			PUSH(type1);
			if (type1 & BCV_WIDE_TYPE_MASK) {
				PUSH(BCV_BASE_TYPE_TOP);
			}
			break;

		case RTV_PUSH_CONSTANT_POOL_ITEM:
			switch (bc) {
			case JBldc:
			case JBldcw:
				if (bc == JBldc) {
					index = PARAM_8(bcIndex, 1);
				} else {
					index = PARAM_16(bcIndex, 1);
				}
				stackTop = pushLdcType(verifyData, romClass, index, stackTop);
				break;

			/* Change lookup table to generate constant of correct type */
			case JBldc2lw:
				PUSH_LONG_CONSTANT;
				break;

			case JBldc2dw:
				PUSH_DOUBLE_CONSTANT;
				break;
			}
			break;

		case RTV_ARRAY_FETCH_PUSH:
			DROP(1);
			type = POP;
			if (type != BCV_BASE_TYPE_NULL) {
				if (bc == JBaaload) {
					type1 = type - 0x01000000;	/* reduce types arity by one */
					PUSH(type1);
					break;
				}
			}
			goto _pushConstant;
			break;

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
			
			tempStoreChange = FALSE;
			
			if (type1 == BCV_GENERIC_OBJECT) {
				/* astore family */
				type = POP;
				tempStoreChange = (type != temps[index]);
				STORE_TEMP(index, type);
			} else {
				DROP(popCount);
				/* because of pre-index local clearing - the order here matters */
				if (type1 & BCV_WIDE_TYPE_MASK) {
					tempStoreChange = (temps[index + 1] != BCV_BASE_TYPE_TOP);
					STORE_TEMP((index + 1), BCV_BASE_TYPE_TOP);
				}
				tempStoreChange |= (type1 != temps[index]);
				STORE_TEMP(index, type1);
			}
			
			if (checkIfInsideException && tempStoreChange) {
				/* For all exception handlers covering this instruction */
				handler = J9EXCEPTIONINFO_HANDLERS(exceptionData);
				SAVE_STACKTOP(liveStack, stackTop);

				/* Save the current liveStack element zero */
				/* Reset the stack pointer to push the exception on the empty stack */
				originalStackTop = stackTop;
				originalStackZeroEntry = liveStack->stackElements[liveStack->stackBaseIndex];

				for (exception = 0; exception < (UDATA) exceptionData->catchCount; exception++) {

					/* Find all matching exception ranges and copy/merge the stack with the exception object */
					if (((UDATA) start >= handler->startPC) && ((UDATA) start < handler->endPC)) {
#ifdef DEBUG_BCV
						printf("exception map change at startPC: %d\n", handler->startPC);
#endif
						stackIndex = bytecodeMap[handler->handlerPC] >> BRANCH_INDEX_SHIFT;
						branch = BCV_INDEX_STACK (stackIndex);
							
						/* "push" the exception object */
						classIndex = BCV_JAVA_LANG_THROWABLE_INDEX;
						if (handler->exceptionClassIndex) {
							/* look up the class in the constant pool */
							utf8string = J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) (&constantPool [handler->exceptionClassIndex]));
							classIndex = findClassName(verifyData, J9UTF8_DATA(utf8string), J9UTF8_LENGTH(utf8string));
						}

						/* Empty the stack */
						stackTop = &(liveStack->stackElements[liveStack->stackBaseIndex]);
						PUSH(classIndex << BCV_CLASS_INDEX_SHIFT);
						SAVE_STACKTOP(liveStack, stackTop);

						if (BCV_ERR_INSUFFICIENT_MEMORY == mergeStacks (verifyData, branch->pc)) {
							errorType = J9NLS_BCV_ERR_VERIFY_OUT_OF_MEMORY__ID;
							goto _outOfMemoryError;
						}
					}
					handler++;
				}

				/* Restore liveStack */
				liveStack->stackElements[liveStack->stackBaseIndex] = originalStackZeroEntry;
				stackTop = originalStackTop;
			}
			break;

		case RTV_POP_X_PUSH_X:
			popCount = 0;
			if (type2) {
				/* shift family */
				popCount = 1;
			}	/* fall through */
			
		case RTV_ARRAY_STORE:
			DROP(popCount);
			break;

		case RTV_POP_X_PUSH_Y:
			/* Cause push of output type */
			type1 = type2;	/* fall through */

		case RTV_POP_2_PUSH:
			DROP(popCount);
			goto _pushConstant;
			break;

		case RTV_BRANCH:
			popCount = type2 & 0x07;
			stackTop -= popCount;

			if (bc == JBgotow) {
				offset32 = (I_32) PARAM_32(bcIndex, 1);
				target = start + offset32;
			} else {
				offset16 = (I_16) PARAM_16(bcIndex, 1);
				target = start + offset16;
			}

			SAVE_STACKTOP(liveStack, stackTop);
			/* Merge our stack to the target */
			if (BCV_ERR_INSUFFICIENT_MEMORY == mergeStacks (verifyData, target)) {
				errorType = J9NLS_BCV_ERR_VERIFY_OUT_OF_MEMORY__ID;
				goto _outOfMemoryError;
			}

			/* Unconditional branch (goto family) */
			if (popCount == 0) {
				goto _checkFinished;
			}
			break;

		case RTV_RETURN:
			goto _checkFinished;
			break;

		case RTV_STATIC_FIELD_ACCESS:
			index = PARAM_16(bcIndex, 1);
			info = &constantPool[index];
			utf8string = ((J9UTF8 *) (J9ROMNAMEANDSIGNATURE_SIGNATURE(J9ROMFIELDREF_NAMEANDSIGNATURE((J9ROMFieldRef *) info))));

			if (bc >= JBgetfield) {
				/* field bytecode receiver */
				DROP(1);
			}
			
			if (bc & 1) {
				/* JBputfield/JBpustatic - odd bc's */
				DROP(1);
				if ((*J9UTF8_DATA(utf8string) == 'D') || (*J9UTF8_DATA(utf8string) == 'J')) {
					DROP(1);
				}

			} else {
				/* JBgetfield/JBgetstatic - even bc's */
				stackTop = pushFieldType(verifyData, utf8string, stackTop);
			}
			break;

		case RTV_SEND:
			if (bc == JBinvokeinterface2) {
				/* Set to point to JBinvokeinterface */
				bcIndex += 2;
			}
			index = PARAM_16(bcIndex, 1);
			if (JBinvokestaticsplit == bc) {
				index = *(U_16 *)(J9ROMCLASS_STATICSPLITMETHODREFINDEXES(romClass) + index);
			} else if (JBinvokespecialsplit == bc) {
				index = *(U_16 *)(J9ROMCLASS_SPECIALSPLITMETHODREFINDEXES(romClass) + index);
			}
			if (bc == JBinvokedynamic) {
				/* TODO invokedynamic should allow for a 3 byte index.  Adjust 'index' to include the other byte */
				utf8string = ((J9UTF8 *) (J9ROMNAMEANDSIGNATURE_SIGNATURE(SRP_PTR_GET(callSiteData + index, J9ROMNameAndSignature*))));
			} else {
				info = &constantPool[index];
				utf8string = ((J9UTF8 *) (J9ROMNAMEANDSIGNATURE_SIGNATURE(J9ROMMETHODREF_NAMEANDSIGNATURE((J9ROMMethodRef *) info))));
			}
			stackTop -= getSendSlotsFromSignature(J9UTF8_DATA(utf8string));

			if ((JBinvokestatic != bc) 
			&& (JBinvokedynamic != bc)
			&& (JBinvokestaticsplit != bc)
			) {
				if ((JBinvokespecial == bc) 
				|| (JBinvokespecialsplit == bc)
				) {

					type = POP;
					if (J9UTF8_DATA(J9ROMNAMEANDSIGNATURE_NAME(J9ROMMETHODREF_NAMEANDSIGNATURE((J9ROMMethodRef *) info)))[0] == '<') {

						/* This is <init>, verify that this is a NEW or INIT object */
						if (type & BCV_SPECIAL) {
							temp1 = getSpecialType(verifyData, type, code);
							/* This invoke special will make all copies of this "type" on the stack a real
							   object, find all copies of this object and initialize them */
							ptr = temps;	/* assumption that stack follows temps */
							/* we don't strictly need to walk temps here, the pending stack would be enough */
							while (ptr != stackTop) {
								if (*ptr == type) {
									*ptr = temp1;
								}
								ptr++;
							}
							type = temp1;
							break;
						}
					}
				} else { /* virtual or interface */
					DROP(1);
				} 
			}

			stackTop = pushReturnType(verifyData, utf8string, stackTop);
			break;

		case RTV_PUSH_NEW:
			switch (bc) {
			case JBnew:
			case JBnewdup:
				/* put a uninitialized object of the correct type on the stack */
				PUSH(BCV_SPECIAL_NEW | (start << BCV_CLASS_INDEX_SHIFT));
				break;

			case JBnewarray:
				index = PARAM_8(bcIndex, 1);
				type = (UDATA) newArrayParamConversion[index];
				DROP(1);	/* pop the size of the array */
				PUSH(type);	/* arity of one implicit */
				break;

			case JBanewarray:
				index = PARAM_16(bcIndex, 1);
				DROP(1);	/* pop the size of the array */
				info = &constantPool[index];
				utf8string = J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) info);

				stackTop = pushClassType(verifyData, utf8string, stackTop);
				/* arity is one greater than signature */
				type = POP;
				PUSH(( (UDATA)1 << BCV_ARITY_SHIFT) + type);
				break;

			case JBmultianewarray:
				/* points to cp entry for class of object to create */
				index = PARAM_16(bcIndex, 1);
				i1 = PARAM_8(bcIndex, 3);
				DROP(i1);
				if (stackTop < stackBase) {
					errorType = J9NLS_BCV_ERR_STACK_UNDERFLOW__ID;
					verboseErrorCode = BCV_ERR_STACK_UNDERFLOW;
					/* Always set to the location of the 1st data type on 'stack' to show up if stackTop <= stackBase */
					errorStackIndex = (U_32)(stackBase - liveStack->stackElements);
					goto _verifyError;
				}

				info = &constantPool[index];
				utf8string = J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) info);

				stackTop = pushClassType(verifyData, utf8string, stackTop);
				break;
			}
			break;

		case RTV_MISC:
			switch (bc) {
			case JBathrow:
				goto _checkFinished;
				break;

			case JBarraylength:
			case JBinstanceof:
				DROP(1);
				PUSH_INTEGER_CONSTANT;
				break;

			case JBtableswitch:
			case JBlookupswitch:
				DROP(1);
				index = (UDATA) ((4 - (pc & 3)) & 3);	/* consume padding */
				pc += index;
				bcIndex += index;
				pc += 8;
				CHECK_END;
				offset32 = (I_32) PARAM_32(bcIndex, 1);
				bcIndex += 4;
				target = offset32 + start;
				SAVE_STACKTOP(liveStack, stackTop);
				if (BCV_ERR_INSUFFICIENT_MEMORY == mergeStacks (verifyData, target)) {
					errorType = J9NLS_BCV_ERR_VERIFY_OUT_OF_MEMORY__ID;
					goto _outOfMemoryError;
				}

				if (bc == JBtableswitch) {
					i1 = (I_32) PARAM_32(bcIndex, 1);
					bcIndex += 4;
					pc += 4;
					i2 = (I_32) PARAM_32(bcIndex, 1);
					bcIndex += 4;

					pc += ((I_32)i2 - (I_32)i1 + 1) * 4;
					CHECK_END;

					/* Add the table switch destinations in reverse order to more closely mimic the order that people
					   (ie: the TCKs) expect you to load classes */
					bcIndex += (((I_32)i2 - (I_32)i1) * 4);	/* point at the last table switch entry */

					/* Count the entries */
					i2 = (I_32)i2 - (I_32)i1 + 1;
					for (i1 = 0; (I_32)i1 < (I_32)i2; i1++) {
						offset32 = (I_32) PARAM_32(bcIndex, 1);
						bcIndex -= 4;	/* back up to point at the previous table switch entry */
						target = offset32 + start;
						if (BCV_ERR_INSUFFICIENT_MEMORY == mergeStacks (verifyData, target)) {
							errorType = J9NLS_BCV_ERR_VERIFY_OUT_OF_MEMORY__ID;
							goto _outOfMemoryError;
						}
					}
				} else {
					i2 = (I_32) PARAM_32(bcIndex, 1);
					bcIndex += 4;

					pc += (I_32)i2 * 8;
					CHECK_END;
					for (i1 = 0; (I_32)i1 < (I_32)i2; i1++) {
						bcIndex += 4;
						offset32 = (I_32) PARAM_32(bcIndex, 1);
						bcIndex += 4;
						target = offset32 + start;
						if (BCV_ERR_INSUFFICIENT_MEMORY == mergeStacks (verifyData, target)) {
							errorType = J9NLS_BCV_ERR_VERIFY_OUT_OF_MEMORY__ID;
							goto _outOfMemoryError;
						}
					}
				}
				goto _checkFinished;
				break;

			case JBmonitorenter:
			case JBmonitorexit:
				DROP(1);
				break;

			case JBcheckcast:
				index = PARAM_16(bcIndex, 1);
				DROP(1);
				info = &constantPool[index];
				utf8string = J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) info);
				stackTop = pushClassType(verifyData, utf8string, stackTop);
				break;
			}
			break;

		case RTV_POP_2_PUSH_INT:
			DROP(popCount);
			PUSH_INTEGER_CONSTANT;
			break;

		case RTV_BYTECODE_POP:
		case RTV_BYTECODE_POP2:
			DROP(popCount);
			break;

		case RTV_BYTECODE_DUP:
			type = POP;
			PUSH(type);
			PUSH(type);
			break;

		case RTV_BYTECODE_DUPX1:
			type = POP;
			temp1 = POP;
			PUSH(type);
			PUSH(temp1);
			PUSH(type);
			break;

		case RTV_BYTECODE_DUPX2:
			type = POP;
			temp1 = POP;
			temp2 = POP;
			PUSH(type);
			PUSH(temp2);
			PUSH(temp1);
			PUSH(type);
			break;

		case RTV_BYTECODE_DUP2:
			temp1 = POP;
			temp2 = POP;
			PUSH(temp2);
			PUSH(temp1);
			PUSH(temp2);
			PUSH(temp1);
			break;

		case RTV_BYTECODE_DUP2X1:
			type = POP;
			temp1 = POP;
			temp2 = POP;
			PUSH(temp1);
			PUSH(type);
			PUSH(temp2);
			PUSH(temp1);
			PUSH(type);
			break;

		case RTV_BYTECODE_DUP2X2:
			type = POP;
			temp1 = POP;
			temp2 = POP;
			temp3 = POP;
			PUSH(temp1);
			PUSH(type);
			PUSH(temp3);
			PUSH(temp2);
			PUSH(temp1);
			PUSH(type);
			break;

		case RTV_BYTECODE_SWAP:
			type = POP;
			temp1 = POP;
			PUSH(type);
			PUSH(temp1);
			break;

		case RTV_UNIMPLEMENTED:
			errorType = J9NLS_BCV_ERR_BC_UNKNOWN__ID;
			/* Jazz 104084: Set the error code in the case of unrecognized opcode. */
			verboseErrorCode = BCV_ERR_BAD_BYTECODE;
			goto _verifyError;
			break;
		}
		continue;

_checkFinished:

		if (verifyData->unwalkedQueueHead != verifyData->unwalkedQueueTail) {
			pc = verifyData->unwalkedQueue[verifyData->unwalkedQueueHead++];
			verifyData->unwalkedQueueHead %= (verifyData->rootQueueSize / sizeof(UDATA));
			if ((bytecodeMap[pc] & BRANCH_ON_UNWALKED_QUEUE) == 0) {
				goto _checkFinished;
			}
			bytecodeMap[pc] &= ~BRANCH_ON_UNWALKED_QUEUE;
			bcIndex = code + pc;
			stackIndex = bytecodeMap[pc] >> BRANCH_INDEX_SHIFT;
			branch = BCV_INDEX_STACK (stackIndex);
			copyStack(branch, liveStack);
			RELOAD_LIVESTACK;
			justLoadedStack = TRUE;
			Trc_BCV_simulateStack_NewWalkFrom(verifyData->vmStruct, 
					(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
					J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
					(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(romMethod)),
					J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)),
					(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(romMethod)),
					J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(romMethod)),
					start, start, pc, pc);
		} else if (verifyData->rewalkQueueHead != verifyData->rewalkQueueTail) {
			pc = verifyData->rewalkQueue[verifyData->rewalkQueueHead++];
			verifyData->rewalkQueueHead %= (verifyData->rootQueueSize / sizeof(UDATA));
			if ((bytecodeMap[pc] & BRANCH_ON_REWALK_QUEUE) == 0) {
				goto _checkFinished;
			}
			bytecodeMap[pc] &= ~BRANCH_ON_REWALK_QUEUE;
			bcIndex = code + pc;
			stackIndex = bytecodeMap[pc] >> BRANCH_INDEX_SHIFT;
			branch = BCV_INDEX_STACK (stackIndex);
			copyStack(branch, liveStack);
			RELOAD_LIVESTACK;
			justLoadedStack = TRUE;
			Trc_BCV_simulateStack_RewalkFrom(verifyData->vmStruct, 
					(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
					J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
					(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(romMethod)),
					J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)),
					(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(romMethod)),
					J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(romMethod)),
					start, start, pc, pc);
		} else {
			Trc_BCV_simulateStack_Exit(verifyData->vmStruct);
			/* else we are done the rootSet -- return */
			return BCV_SUCCESS;
		}
	}
#undef CHECK_END

	errorType = J9NLS_BCV_ERR_UNEXPECTED_EOF__ID;	/* should never reach here */

_verifyError:
	/* Jazz 104084: Store the verification error data here when error occurs */
	storeVerifyErrorData(verifyData, (I_16)verboseErrorCode, (U_32)errorStackIndex, errorTargetType, errorTempData, start);

	BUILD_VERIFY_ERROR(errorModule, errorType);
	Trc_BCV_simulateStack_verifyError(verifyData->vmStruct,
										verifyData->errorPC,
										verifyData->errorCode);
	Trc_BCV_simulateStack_verifyErrorBytecode(verifyData->vmStruct,
			(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
			J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
			(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(romMethod)),
			J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)),
			(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(romMethod)),
			J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(romMethod)),
			verifyData->errorCode, verifyData->errorPC, verifyData->errorPC, bc);
	Trc_BCV_simulateStack_Exit(verifyData->vmStruct);

	return BCV_ERR_INTERNAL_ERROR;

_outOfMemoryError:
	BUILD_VERIFY_ERROR(errorModule, errorType);
	Trc_BCV_simulateStack_verifyError(verifyData->vmStruct,
										verifyData->errorPC,
										verifyData->errorCode);

	Trc_BCV_simulateStack_verifyErrorBytecode_OutOfMemoryException(verifyData->vmStruct,
		(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
		J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
		(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(romMethod)),
		J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)),
		(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(romMethod)),
		J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(romMethod)),
		verifyData->errorCode, verifyData->errorPC, verifyData->errorPC, bc);
	Trc_BCV_simulateStack_Exit(verifyData->vmStruct);

	return BCV_ERR_INSUFFICIENT_MEMORY;

}

/*
 * return BCV_SUCCESS on success
 * returns BCV_ERR_INSUFFICIENT_MEMORY on OOM
 */

IDATA
allocateVerifyBuffers (J9PortLibrary * portLib, J9BytecodeVerificationData *verifyData)
{
	Trc_BCV_allocateVerifyBuffers_Event1(verifyData->vmStruct);

	verifyData->classNameList = 0;
	verifyData->classNameListEnd = 0;
	verifyData->classNameSegment = 0;
	verifyData->classNameSegmentFree = 0;
	verifyData->classNameSegmentEnd = 0;
	verifyData->bytecodeMap = 0;
	verifyData->stackMaps = 0;
	verifyData->liveStack = 0;
	verifyData->unwalkedQueue = 0;
	verifyData->rewalkQueue = 0;

	verifyData->classNameList = (J9UTF8 **) bcvalloc (verifyData, (UDATA) CLASSNAMELIST_DEFAULT_SIZE);
	verifyData->classNameListEnd = (J9UTF8 **)((UDATA)verifyData->classNameList + CLASSNAMELIST_DEFAULT_SIZE);

	verifyData->classNameSegment = bcvalloc (verifyData, (UDATA) CLASSNAMESEGMENT_DEFAULT_SIZE);
	verifyData->classNameSegmentEnd = verifyData->classNameSegment + CLASSNAMESEGMENT_DEFAULT_SIZE;
	verifyData->classNameSegmentFree = verifyData->classNameSegment;

	verifyData->bytecodeMap = bcvalloc (verifyData, (UDATA) BYTECODE_MAP_DEFAULT_SIZE);
	verifyData->bytecodeMapSize = BYTECODE_MAP_DEFAULT_SIZE;

	verifyData->stackMaps = bcvalloc (verifyData, (UDATA) STACK_MAPS_DEFAULT_SIZE);
	verifyData->stackMapsSize = STACK_MAPS_DEFAULT_SIZE;
	verifyData->stackMapsCount = 0;

	verifyData->unwalkedQueue = bcvalloc (verifyData, (UDATA) ROOT_QUEUE_DEFAULT_SIZE);
	verifyData->unwalkedQueueHead = 0;
	verifyData->unwalkedQueueTail = 0;
	verifyData->rewalkQueue = bcvalloc (verifyData, (UDATA) ROOT_QUEUE_DEFAULT_SIZE);
	verifyData->rewalkQueueHead = 0;
	verifyData->rewalkQueueTail = 0;
	verifyData->rootQueueSize = ROOT_QUEUE_DEFAULT_SIZE;

	verifyData->liveStack = bcvalloc (verifyData, (UDATA) LIVE_STACK_DEFAULT_SIZE);
	verifyData->liveStackSize = LIVE_STACK_DEFAULT_SIZE;
	verifyData->stackSize = 0;

	RESET_VERIFY_ERROR(verifyData);

	verifyData->portLib = portLib;

	if (!(verifyData->classNameList && verifyData->classNameSegment && verifyData->bytecodeMap 
			&& verifyData->stackMaps && verifyData->unwalkedQueue && verifyData->rewalkQueue && verifyData->liveStack)) {
		freeVerifyBuffers (portLib, verifyData);
		Trc_BCV_allocateVerifyBuffers_allocFailure(verifyData->vmStruct);
		return BCV_ERR_INSUFFICIENT_MEMORY;
	}
	/* Now we know the allocates were successful, initialize the required data */
	verifyData->classNameList[0] = NULL;
	return BCV_SUCCESS;
}


/*
	BCV interface to j9mem_allocate_memory.
	@return pointer to allocated memory, or 0 if allocation fails.
	@note Does not deallocate the internal buffer
*/

void*  
bcvalloc (J9BytecodeVerificationData * verifyData, UDATA byteCount)
{
	UDATA *returnVal = 0;
	J9BCVAlloc *temp1, *temp2;

	PORT_ACCESS_FROM_PORT(verifyData->portLib);

	/* Round to UDATA multiple */
	byteCount = (UDATA) ((byteCount + (sizeof(UDATA) - 1)) & ~(sizeof(UDATA) - 1));
	/* Allow room for the linking header */
	byteCount += sizeof(UDATA);

	if (verifyData->internalBufferStart == 0) {
		verifyData->internalBufferStart = j9mem_allocate_memory(BCV_INTERNAL_DEFAULT_SIZE, J9MEM_CATEGORY_CLASSES);
		if (verifyData->internalBufferStart == 0) {
			return 0;
		}
		verifyData->internalBufferEnd = (UDATA *) ((UDATA)verifyData->internalBufferStart + BCV_INTERNAL_DEFAULT_SIZE);
		verifyData->currentAlloc = verifyData->internalBufferStart;
		*verifyData->currentAlloc = (UDATA) verifyData->currentAlloc;
	}

	temp1 = (J9BCVAlloc *) verifyData->currentAlloc;
	temp2 = (J9BCVAlloc *) ((UDATA) temp1 + byteCount);

	if ((UDATA *) temp2 >= verifyData->internalBufferEnd) {
		returnVal = j9mem_allocate_memory(byteCount, J9MEM_CATEGORY_CLASSES);
		Trc_BCV_bcvalloc_ExternalAlloc(verifyData->vmStruct, 
				(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				byteCount, returnVal);
		if (returnVal == 0) {
			return 0;
		}
	} else {
		/* tag the back pointer as the block following this pointer is in use */
		temp1->prev = (J9BCVAlloc *) ((UDATA) temp1->prev | 1);
		temp2->prev = temp1;
		verifyData->currentAlloc = (UDATA *) temp2;
		returnVal = temp1->data;
	}
	return returnVal;
}



/*
	BCV interface to j9mem_allocate_memory.
*/

void  
bcvfree (J9BytecodeVerificationData * verifyData, void* address)
{
	J9BCVAlloc *temp1, *temp2;

	PORT_ACCESS_FROM_PORT(verifyData->portLib);

	if (((UDATA *) address >= verifyData->internalBufferEnd) || ((UDATA *) address < verifyData->internalBufferStart)) {
		Trc_BCV_bcvalloc_ExternalFreeAddress(verifyData->vmStruct, address);
		j9mem_free_memory(address);
		return;
	}

	temp1 = (J9BCVAlloc *) ((UDATA *) address - 1);
	/* flag block following the pointer as free */
	temp1->prev = (J9BCVAlloc *) ((UDATA) temp1->prev & ~1);
	temp2 = (J9BCVAlloc *) verifyData->currentAlloc;

	while (temp1 == temp2->prev) {
		/* Release most recent alloc and any preceding contiguous already freed allocs */
		temp2 = temp2->prev;
		temp1 = temp2->prev;
		if ((UDATA) temp1->prev & 1) {
			/* stop if an in-use block is found */
			verifyData->currentAlloc = (UDATA *) temp2;
			break;
		}
		if (temp1 == temp2) {
			/* all blocks unused - release the buffer */
			j9mem_free_memory(verifyData->internalBufferStart);
			verifyData->internalBufferStart = 0;	/* Set the internal buffer start to zero so it will be re-allocated next time */
			verifyData->internalBufferEnd = 0;
			break;
		}
	}
}


void  
freeVerifyBuffers (J9PortLibrary * portLib, J9BytecodeVerificationData *verifyData)
{
	Trc_BCV_freeVerifyBuffers_Event1(verifyData->vmStruct);

	if (verifyData->classNameList ) {
		bcvfree (verifyData, verifyData->classNameList);
	}

	if (verifyData->classNameSegment ) {
		bcvfree (verifyData, verifyData->classNameSegment);
	}

	if (verifyData->bytecodeMap ) {
		bcvfree (verifyData, verifyData->bytecodeMap);
	}

	if (verifyData->stackMaps ) {
		bcvfree (verifyData, verifyData->stackMaps);
	}

	if (verifyData->unwalkedQueue ) {
		bcvfree (verifyData, verifyData->unwalkedQueue);
	}

	if (verifyData->rewalkQueue ) {
		bcvfree (verifyData, verifyData->rewalkQueue);
	}

	if (verifyData->liveStack ) {
		bcvfree (verifyData, verifyData->liveStack);
	}

	verifyData->classNameList = 0;
	verifyData->classNameListEnd = 0;
	verifyData->classNameSegment = 0;
	verifyData->classNameSegmentFree = 0;
	verifyData->classNameSegmentEnd = 0;
	verifyData->bytecodeMap = 0;
	verifyData->stackMaps = 0;
	verifyData->liveStack = 0;
	verifyData->unwalkedQueue = 0;
	verifyData->rewalkQueue = 0;
}



void  
j9bcv_freeVerificationData (J9PortLibrary * portLib, J9BytecodeVerificationData * verifyData)
{
	PORT_ACCESS_FROM_PORT(portLib);
	if (verifyData) {
#ifdef J9VM_THR_PREEMPTIVE
		JavaVM* jniVM = (JavaVM*)verifyData->javaVM;
	    J9ThreadEnv* threadEnv;
		(*jniVM)->GetEnv(jniVM, (void**)&threadEnv, J9THREAD_VERSION_1_1);

		threadEnv->monitor_destroy( verifyData->verifierMutex );
#endif
		freeVerifyBuffers( PORTLIB, verifyData );
		j9mem_free_memory( verifyData->excludeAttribute );
		j9mem_free_memory( verifyData );
	}
}

/*
 * returns J9BytecodeVerificationData* on success
 * returns NULL on OOM
 */
J9BytecodeVerificationData *  
j9bcv_initializeVerificationData(J9JavaVM* javaVM)
{
	J9BytecodeVerificationData * verifyData;
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	JavaVM* jniVM = (JavaVM*)javaVM;
	J9ThreadEnv* threadEnv;

	(*jniVM)->GetEnv(jniVM, (void**)&threadEnv, J9THREAD_VERSION_1_1);

	verifyData = j9mem_allocate_memory((UDATA) sizeof(*verifyData), J9MEM_CATEGORY_CLASSES);
	if( !verifyData ) {
		goto error_no_memory;
	}

	/* blank the vmStruct field */
	verifyData->vmStruct = NULL;
	verifyData->javaVM = javaVM;

#ifdef J9VM_THR_PREEMPTIVE
	threadEnv->monitor_init_with_name(&verifyData->verifierMutex, 0, "BCVD verifier");
	if (!verifyData->verifierMutex) {
		goto error_no_memory;
	}
#endif

	verifyData->verifyBytecodesFunction = j9bcv_verifyBytecodes;
	verifyData->checkClassLoadingConstraintForNameFunction = j9bcv_checkClassLoadingConstraintForName;
	verifyData->internalBufferStart = 0;
	verifyData->internalBufferEnd = 0;
	verifyData->portLib = PORTLIB;
	verifyData->ignoreStackMaps = 0;
	verifyData->excludeAttribute = NULL;
	verifyData->redefinedClassesCount = 0;

	if (BCV_ERR_INSUFFICIENT_MEMORY == allocateVerifyBuffers (PORTLIB, verifyData)) {
		goto error_no_memory;
	}

	/* default verification options */
	verifyData->verificationFlags = J9_VERIFY_SKIP_BOOTSTRAP_CLASSES | J9_VERIFY_OPTIMIZE; 	

	return verifyData;

error_no_memory:
	if (verifyData) {
#ifdef J9VM_THR_PREEMPTIVE
		threadEnv->monitor_destroy (verifyData->verifierMutex);
#endif
		j9mem_free_memory(verifyData);
	}
	return NULL;
}



#define ALLOC_BUFFER(name, needed) \
	if (needed > name##Size) { \
		bcvfree(verifyData, name); \
		name = bcvalloc(verifyData, needed); \
		if (NULL == name) { \
			name##Size = 0; \
			result = BCV_ERR_INSUFFICIENT_MEMORY; \
			break; \
		} \
		name##Size = needed; \
	}

/* 
 * Sequence the 2 verification passes - flow based type inference stack map generation
 * and linear stack map verification
 *
 * returns BCV_SUCCESS on success
 * returns BCV_ERR_INSUFFICIENT_MEMORY on OOM
 * returns BCV_ERR_INTERNAL_ERROR on verification error
 */

IDATA 
j9bcv_verifyBytecodes (J9PortLibrary * portLib, J9Class * clazz, J9ROMClass * romClass,
		   J9BytecodeVerificationData * verifyData)
{
	UDATA i, j;
	J9ROMMethod *romMethod;
	UDATA argCount;
	UDATA length;
	UDATA mapLength;
	BOOLEAN hasStackMaps = (J9ROMCLASS_HAS_VERIFY_DATA(romClass) != 0);
	UDATA oldState;
	IDATA result = 0;
	UDATA rootQueueSize;
	U_32 *bytecodeMap;
	UDATA start = 0;
	J9BranchTargetStack *liveStack;
	U_8 *stackMapData;
	UDATA stackMapsSize;
	UDATA *stackTop;
	BOOLEAN classVersionRequiresStackmaps = romClass->majorVersion >= CFR_MAJOR_VERSION_REQUIRING_STACKMAPS;
	BOOLEAN newFormat = (classVersionRequiresStackmaps || hasStackMaps);
	BOOLEAN verboseVerification = (J9_VERIFY_VERBOSE_VERIFICATION == (verifyData->verificationFlags & J9_VERIFY_VERBOSE_VERIFICATION));
	BOOLEAN classRelationshipVerifierEnabled = (J9_ARE_ANY_BITS_SET(verifyData->javaVM->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_ENABLE_CLASS_RELATIONSHIP_VERIFIER) && classVersionRequiresStackmaps);
	BOOLEAN classRelationshipVerifierIgnoreSCCEnabled = (J9_ARE_ANY_BITS_SET(verifyData->javaVM->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_ENABLE_CLASS_RELATIONSHIP_VERIFIER_IGNORE_SCC));
	BOOLEAN sharedCacheEnabled = (FALSE == classRelationshipVerifierIgnoreSCCEnabled) && (NULL != verifyData->javaVM->sharedClassConfig);
	BOOLEAN cacheRelationshipSnippetsEnabled = FALSE;
	BOOLEAN foundSnippetsInCache = FALSE;
	J9SharedDataDescriptor classRelationshipSnippetsDataDescriptor = {0};
	PORT_ACCESS_FROM_PORT(portLib);
	
	Trc_BCV_j9bcv_verifyBytecodes_Entry(verifyData->vmStruct,
									(UDATA) J9UTF8_LENGTH((J9ROMCLASS_CLASSNAME(romClass))),
									J9UTF8_DATA(J9ROMCLASS_CLASSNAME(romClass)));

	/* save current and set vmState */
	oldState = verifyData->vmStruct->omrVMThread->vmState;
	verifyData->vmStruct->omrVMThread->vmState = J9VMSTATE_BCVERIFY;

	verifyData->romClass = romClass;
	verifyData->errorPC = 0;
	
	verifyData->romClassInSharedClasses = j9shr_Query_IsAddressInCache(verifyData->javaVM, romClass, romClass->romSize);

	/* List is used for the whole class */
	initializeClassNameList(verifyData);


	romMethod = (J9ROMMethod *) J9ROMCLASS_ROMMETHODS(romClass);

	if (verboseVerification) {
		ALWAYS_TRIGGER_J9HOOK_VM_CLASS_VERIFICATION_START(verifyData->javaVM->hookInterface, verifyData, newFormat);
	}

	/* Enable snippets if -XX:+ClassRelationshipVerifier is used, the shared cache is enabled and the class is in the cache */
	cacheRelationshipSnippetsEnabled = classRelationshipVerifierEnabled && sharedCacheEnabled && verifyData->romClassInSharedClasses;

	if (cacheRelationshipSnippetsEnabled) {
		foundSnippetsInCache = j9bcv_fetchClassRelationshipSnippetsFromSharedCache(verifyData, &classRelationshipSnippetsDataDescriptor, &result);

		if (foundSnippetsInCache) {
			/**
			 * Skip the linear bytecode walk. Snippets already exist in the shared cache since this class
			 * was already verified and processed during the initial run.
			 */
			goto _done;
		} else {
			/**
			 * Otherwise, snippets do not already exist in the shared cache (initial run).
			 * Continue to the linear bytecode walk, storing snippets as isClassCompatible() is called.
			 */
			if (BCV_ERR_INSUFFICIENT_MEMORY == result) {
				/* Failed to allocate snippet table */
				goto _done;
			}
		}

	}

	/* For each method in the class */
	for (i = 0; i < (UDATA) romClass->romMethodCount; i++) {

		UDATA createStackMaps;
		
		verifyData->ignoreStackMaps = (verifyData->verificationFlags & J9_VERIFY_IGNORE_STACK_MAPS) != 0;
		verifyData->createdStackMap = FALSE;
		verifyData->romMethod = romMethod;

		Trc_BCV_j9bcv_verifyBytecodes_VerifyMethod(verifyData->vmStruct,
				(UDATA) J9UTF8_LENGTH((J9ROMCLASS_CLASSNAME(romClass))),
				J9UTF8_DATA(J9ROMCLASS_CLASSNAME(romClass)),
				(UDATA) J9UTF8_LENGTH((J9ROMMETHOD_NAME(romMethod))),
				J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)),
				(UDATA) J9UTF8_LENGTH((J9ROMMETHOD_SIGNATURE(romMethod))),
				J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(romMethod)),
				romMethod->modifiers);

		/* If native or abstract method, do nothing */
		if (!((romMethod->modifiers & J9AccNative) || (romMethod->modifiers & J9AccAbstract))) {
			BOOLEAN isInitMethod = FALSE;

			/* BCV_TARGET_STACK_HEADER_UDATA_SIZE for pc/stackBase/stackEnd in J9BranchTargetStack and
			 * BCV_STACK_OVERFLOW_BUFFER_UDATA_SIZE for late overflow detection of longs/doubles
			 */
			verifyData->stackSize = (J9_MAX_STACK_FROM_ROM_METHOD(romMethod)
									+ J9_ARG_COUNT_FROM_ROM_METHOD(romMethod)
									+ J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod)
									+ BCV_TARGET_STACK_HEADER_UDATA_SIZE
									+ BCV_STACK_OVERFLOW_BUFFER_UDATA_SIZE) * sizeof(UDATA);
						
			ALLOC_BUFFER(verifyData->liveStack, verifyData->stackSize);

			length = (UDATA) (J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod));
			mapLength = length * sizeof(U_32);
			
			ALLOC_BUFFER(verifyData->bytecodeMap, mapLength);
			bytecodeMap = verifyData->bytecodeMap;
			
_fallBack:
			memset(bytecodeMap, 0, mapLength);
							
			createStackMaps = !classVersionRequiresStackmaps && (verifyData->ignoreStackMaps || !hasStackMaps);

			if (createStackMaps) {
				verifyData->stackMapsCount = buildBranchMap(verifyData);
				
				if (verifyData->stackMapsCount == (UDATA)BCV_ERR_INTERNAL_ERROR) {
					BUILD_VERIFY_ERROR(J9NLS_BCV_ERR_BYTECODES_INVALID__MODULE, J9NLS_BCV_ERR_BYTECODES_INVALID__ID);
					result = BCV_ERR_INTERNAL_ERROR;
					break;
				}
			} else {
				
				U_32 *stackMapMethod = getStackMapInfoForROMMethod(romMethod);

				verifyData->stackMapsCount = 0;
				stackMapData = 0;

				/*Access the stored stack map data for this method, get pointer and map count */
				if (stackMapMethod) {
					stackMapData = (U_8 *)(stackMapMethod + 1);
					NEXT_U16(verifyData->stackMapsCount, stackMapData);
				}
			}
		
			stackMapsSize = (verifyData->stackSize) * (verifyData->stackMapsCount);
	
			ALLOC_BUFFER(verifyData->stackMaps, stackMapsSize);
		
			if (createStackMaps && verifyData->stackMapsCount) {
				UDATA mapIndex = 0;
				/* Non-empty internal stackMap created */
				verifyData->createdStackMap = TRUE;
	
				liveStack = BCV_FIRST_STACK ();
				/* Initialize stackMaps */
				for (j = 0; j < length; j++) {
					if ((bytecodeMap)[j] & BRANCH_TARGET) {
						liveStack->pc = j;		/* offset of the branch target */
						liveStack->stackBaseIndex = -1;
						liveStack->stackTopIndex = -1;
						liveStack = BCV_NEXT_STACK (liveStack);
						(bytecodeMap)[j] |= (mapIndex << BRANCH_INDEX_SHIFT);
						mapIndex++;
					}
				}
	
				rootQueueSize = (verifyData->stackMapsCount + 1) * sizeof(UDATA);

				if (rootQueueSize > verifyData->rootQueueSize) {
					bcvfree(verifyData, verifyData->unwalkedQueue);
					verifyData->unwalkedQueue = bcvalloc(verifyData, rootQueueSize);
					bcvfree(verifyData, verifyData->rewalkQueue);
					verifyData->rewalkQueue = bcvalloc(verifyData, rootQueueSize);
					verifyData->rootQueueSize = rootQueueSize;
					if (!(verifyData->unwalkedQueue && verifyData->rewalkQueue)) {
						result = BCV_ERR_INSUFFICIENT_MEMORY;
						break;
					}
				}
			}

			liveStack = (J9BranchTargetStack *) verifyData->liveStack;
			stackTop = &(liveStack->stackElements[0]);
	
			isInitMethod = buildStackFromMethodSignature(verifyData, &stackTop, &argCount);
	
			SAVE_STACKTOP(liveStack, stackTop);
			liveStack->stackBaseIndex = liveStack->stackTopIndex;
		
			result = 0;
			if (verifyData->stackMapsCount) {
				if (createStackMaps) {
					result = simulateStack (verifyData);
				} else {
					result = decompressStackMaps (verifyData, argCount, stackMapData);
				}
			}
			
			if (BCV_ERR_INSUFFICIENT_MEMORY == result) {
				goto _done;
			}

			/* If stack maps created */
			/* Only perform second verification pass with a valid J9Class */
			if ((result == BCV_SUCCESS) && clazz) {
				if (isInitMethod) {
					/* CMVC 199785: Jazz103 45899: Only run this when the stack has been built correctly */
					setInitializedThisStatus(verifyData);
				}

				if (newFormat && verboseVerification) {
					ALWAYS_TRIGGER_J9HOOK_VM_METHOD_VERIFICATION_START(verifyData->javaVM->hookInterface, verifyData);
				}

				result = j9rtv_verifyBytecodes (verifyData);

				if (BCV_ERR_INSUFFICIENT_MEMORY == result) {
					goto _done;
				}

				if (newFormat && verboseVerification) {
					BOOLEAN willFailOver = FALSE;
					/*
					 * If verification failed and will fail over to older verifier, we only output stack map frame details
					 * if the frame count is bigger than 0.
					 */
					if ((BCV_SUCCESS != result)
						&& !classVersionRequiresStackmaps
						&& !createStackMaps
						&& (J9_VERIFY_NO_FALLBACK != (verifyData->verificationFlags & J9_VERIFY_NO_FALLBACK))) {
						willFailOver = TRUE;
					}

					if (!willFailOver || (verifyData->stackMapsCount > 0)) {
						ALWAYS_TRIGGER_J9HOOK_VM_STACKMAPFRAME_VERIFICATION(verifyData->javaVM->hookInterface, verifyData);
					}
				}
			}
			/* If verify error */
			if (result) {
				/* Check verification fallback criteria */
				if (classVersionRequiresStackmaps || createStackMaps || (verifyData->verificationFlags & J9_VERIFY_NO_FALLBACK)) {
					/* no retry */
					result = BCV_ERR_INTERNAL_ERROR;
					break;
				} else {
					/* reset verification failure */
					RESET_VERIFY_ERROR(verifyData);
					verifyData->errorPC = (UDATA) 0;
					verifyData->errorModule = 0;
					verifyData->errorCode = 0;				

					Trc_BCV_j9bcv_verifyBytecodes_ReverifyMethod(verifyData->vmStruct,
							(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass)),
							J9UTF8_DATA(J9ROMCLASS_CLASSNAME(romClass)),
							(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(romMethod)),
							J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)),
							(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(romMethod)),
							J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(romMethod)));

					/* retry with ignoreStackMaps enabled */
					verifyData->ignoreStackMaps = TRUE;

					if (verboseVerification) {
						newFormat = FALSE;
						ALWAYS_TRIGGER_J9HOOK_VM_CLASS_VERIFICATION_FALLBACK(verifyData->javaVM->hookInterface, verifyData, newFormat);
					}

					goto _fallBack;
				}
			}
		}
		
		romMethod = J9_NEXT_ROM_METHOD(romMethod);
	}

	if (cacheRelationshipSnippetsEnabled) {
		result = j9bcv_storeClassRelationshipSnippetsToSharedCache(verifyData);
	}

_done:
	verifyData->vmStruct->omrVMThread->vmState = oldState;
	if (result == BCV_ERR_INSUFFICIENT_MEMORY) {
		Trc_BCV_j9bcv_verifyBytecodes_OutOfMemory(verifyData->vmStruct, 
				(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(romMethod)));
	}

	if (cacheRelationshipSnippetsEnabled) {
		result = j9bcv_processClassRelationshipSnippets(verifyData, &classRelationshipSnippetsDataDescriptor);
	}

	if (verboseVerification) {
		ALWAYS_TRIGGER_J9HOOK_VM_CLASS_VERIFICATION_END(verifyData->javaVM->hookInterface, verifyData, newFormat);
	}

	Trc_BCV_j9bcv_verifyBytecodes_Exit(verifyData->vmStruct, result);

	return result;
}
#undef ALLOC_BUFFER


/*
 * returns J9VMDLLMAIN_OK on success
 * returns J9VMDLLMAIN_FAILED on error
 */
IDATA  
j9bcv_J9VMDllMain (J9JavaVM* vm, IDATA stage, void* reserved)
{
	J9BytecodeVerificationData* verifyData = NULL;
	char optionValuesBuffer[128];					/* Needs to be big enough to hold -Xverify option values */
	char* optionValuesBufferPtr = optionValuesBuffer;
	J9VMDllLoadInfo* loadInfo = NULL;
	IDATA xVerifyIndex = -1;
	IDATA xVerifyColonIndex = -1;
	IDATA verboseVerificationIndex = -1;
	IDATA noVerboseVerificationIndex = -1;
	IDATA verifyErrorDetailsIndex = -1;
	IDATA noVerifyErrorDetailsIndex = -1;
	IDATA classRelationshipVerifierIndex = -1;
	IDATA noClassRelationshipVerifierIndex = -1;
	IDATA classRelationshipVerifierIgnoreSCCIndex = -1;
	IDATA noClassRelationshipVerifierIgnoreSCCIndex = -1;
	IDATA returnVal = J9VMDLLMAIN_OK;
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	J9HookInterface ** vmHooks = vm->internalVMFunctions->getVMHookInterface(vm);
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

	PORT_ACCESS_FROM_JAVAVM(vm);

	switch(stage) {

		case ALL_VM_ARGS_CONSUMED :
			FIND_AND_CONSUME_ARG( OPTIONAL_LIST_MATCH, OPT_XVERIFY, NULL);
			break;

		case BYTECODE_TABLE_SET :
			loadInfo = FIND_DLL_TABLE_ENTRY( THIS_DLL_NAME );
			verifyData = j9bcv_initializeVerificationData(vm);
			if( !verifyData ) {
				loadInfo->fatalErrorStr = "j9bcv_initializeVerificationData failed";
				returnVal = J9VMDLLMAIN_FAILED;
				break;
			}

			vm->bytecodeVerificationData = verifyData;
			vm->runtimeFlags |= J9_RUNTIME_VERIFY;

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) 
			if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_CLASSES_UNLOAD, bcvHookClassesUnload, OMR_GET_CALLSITE(), vm)) {
				returnVal = J9VMDLLMAIN_FAILED;
				break;
			}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

			/* Parse the -Xverify and -Xverify:<opt> commandline options.
			 * Rules:
			 * 1. -Xverify skips any previous -Xverify:<opt> arguments.  -Xverify is the default state.
			 * 2. Any -Xverify:<opt> prior to -Xverify is ignored.
			 * 3. All -Xverify:<opt> after the -Xverify are processed in left-to-right order.
			 * 4. -Xverify:<opt>,<opt> etc is also valid.
			 * 5. -Xverify: is an error.
			 * 6. -Xverify:<opt> processing occurs in the parseOptions function.
			 *
			 * This parsing is a duplicate of the parsing in the function VMInitStages of jvminit.c
			 */
			xVerifyIndex = FIND_ARG_IN_VMARGS( EXACT_MATCH, OPT_XVERIFY, NULL);
			xVerifyColonIndex = FIND_ARG_IN_VMARGS_FORWARD( STARTSWITH_MATCH, OPT_XVERIFY_COLON, NULL);
			while (xVerifyColonIndex >= 0) {
				/* Ignore -Xverify:<opt>'s prior to the the last -Xverify */
				if (xVerifyColonIndex > xVerifyIndex) {
					/* Deal with possible -Xverify:<opt>,<opt> case */
					GET_OPTION_VALUES( xVerifyColonIndex, ':', ',', &optionValuesBufferPtr, 128 );

					if(*optionValuesBuffer) {
						if (!parseOptions(vm, optionValuesBuffer, &loadInfo->fatalErrorStr)) {
							returnVal = J9VMDLLMAIN_FAILED;
						}
					} else {
						loadInfo->fatalErrorStr = "No options specified for -Xverify:<opt>";
						returnVal = J9VMDLLMAIN_FAILED;
					}
				}
				/* Advance to next argument */
				xVerifyColonIndex = FIND_NEXT_ARG_IN_VMARGS_FORWARD(STARTSWITH_MATCH, OPT_XVERIFY_COLON, NULL, xVerifyColonIndex);
			}

			verboseVerificationIndex = FIND_AND_CONSUME_ARG(EXACT_MATCH, VMOPT_XXVERBOSEVERIFICATION, NULL);
			noVerboseVerificationIndex = FIND_AND_CONSUME_ARG(EXACT_MATCH, VMOPT_XXNOVERBOSEVERIFICATION, NULL);
			if (verboseVerificationIndex > noVerboseVerificationIndex) {
				vm->bytecodeVerificationData->verificationFlags |= J9_VERIFY_VERBOSE_VERIFICATION;
			}

			verifyErrorDetailsIndex = FIND_AND_CONSUME_ARG(EXACT_MATCH, VMOPT_XXVERIFYERRORDETAILS, NULL);
			noVerifyErrorDetailsIndex = FIND_AND_CONSUME_ARG(EXACT_MATCH, VMOPT_XXNOVERIFYERRORDETAILS, NULL);
			if (verifyErrorDetailsIndex >= noVerifyErrorDetailsIndex) {
				vm->bytecodeVerificationData->verificationFlags |= J9_VERIFY_ERROR_DETAILS;
			}

			/* Set runtime flag for -XX:+ClassRelationshipVerifier */
			classRelationshipVerifierIndex = FIND_AND_CONSUME_ARG(EXACT_MATCH, VMOPT_XXCLASSRELATIONSHIPVERIFIER, NULL);
			noClassRelationshipVerifierIndex = FIND_AND_CONSUME_ARG(EXACT_MATCH, VMOPT_XXNOCLASSRELATIONSHIPVERIFIER, NULL);
			if (classRelationshipVerifierIndex > noClassRelationshipVerifierIndex) {
				if (J9_ARE_ANY_BITS_SET(vm->runtimeFlags, J9_RUNTIME_XFUTURE)) {
					loadInfo->fatalErrorStr = "-XX:+ClassRelationshipVerifier cannot be used if -Xfuture or if -Xverify:all is enabled";
					returnVal = J9VMDLLMAIN_FAILED;
				} else {
					vm->extendedRuntimeFlags2 |= J9_EXTENDED_RUNTIME2_ENABLE_CLASS_RELATIONSHIP_VERIFIER;
				}
			}

			/**
			 * Set runtime flag for -XX:+ClassRelationshipVerifierIgnoreSCC
			 *
			 * If both -XX:+ClassRelationshipVerifier and -XX:+ClassRelationshipVerifierIgnoreSCC are used,
			 * the right-most option will take precedence.
			 */
			classRelationshipVerifierIgnoreSCCIndex = FIND_AND_CONSUME_ARG(EXACT_MATCH, VMOPT_XXCLASSRELATIONSHIPVERIFIER_IGNORESCC, NULL);
			noClassRelationshipVerifierIgnoreSCCIndex = FIND_AND_CONSUME_ARG(EXACT_MATCH, VMOPT_XXNOCLASSRELATIONSHIPVERIFIER_IGNORESCC, NULL);
			if (classRelationshipVerifierIgnoreSCCIndex > noClassRelationshipVerifierIgnoreSCCIndex) {
				if ((classRelationshipVerifierIgnoreSCCIndex > classRelationshipVerifierIndex) &&
					(classRelationshipVerifierIgnoreSCCIndex > noClassRelationshipVerifierIndex)
				) {
					if (J9_ARE_ANY_BITS_SET(vm->runtimeFlags, J9_RUNTIME_XFUTURE)) {
						loadInfo->fatalErrorStr = "-XX:+ClassRelationshipVerifierIgnoreSCC cannot be used if -Xfuture or if -Xverify:all is enabled";
						returnVal = J9VMDLLMAIN_FAILED;
					} else {
						if (-1 == classRelationshipVerifierIndex) {
							vm->extendedRuntimeFlags2 |= J9_EXTENDED_RUNTIME2_ENABLE_CLASS_RELATIONSHIP_VERIFIER;
						}
						vm->extendedRuntimeFlags2 |= J9_EXTENDED_RUNTIME2_ENABLE_CLASS_RELATIONSHIP_VERIFIER_IGNORE_SCC;
					}
				}
			}

			break;

		case LIBRARIES_ONUNLOAD :
			if (vm->bytecodeVerificationData) {
				j9bcv_freeVerificationData(PORTLIB, vm->bytecodeVerificationData);
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) 
				(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_CLASSES_UNLOAD, bcvHookClassesUnload, vm);
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
			}
			break;
	}
	return returnVal;
}


static IDATA 
setVerifyState(J9JavaVM *vm, char *option, char **errorString)
{
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (0 == strcmp(option, OPT_ALL)) {
		/* JDK7 - CMVC 151154: Sun launcher converts -Xfuture to -Xverify:all */
		vm->runtimeFlags |= J9_RUNTIME_XFUTURE;
		vm->bytecodeVerificationData->verificationFlags &= ~J9_VERIFY_SKIP_BOOTSTRAP_CLASSES;
	} else if (0 == strcmp(option, OPT_OPT)) {
		/* on by default - will override a "noopt" before it */
		vm->bytecodeVerificationData->verificationFlags |= J9_VERIFY_OPTIMIZE;
	} else if (0 == strcmp(option, OPT_NO_OPT)) {
		vm->bytecodeVerificationData->verificationFlags &= ~J9_VERIFY_OPTIMIZE;
	} else if (0 == strcmp(option, OPT_NO_FALLBACK)) {
		vm->bytecodeVerificationData->verificationFlags |= J9_VERIFY_NO_FALLBACK;
	} else if (0 == strcmp(option, OPT_IGNORE_STACK_MAPS)) {
		vm->bytecodeVerificationData->verificationFlags |= J9_VERIFY_IGNORE_STACK_MAPS;
	} else if (0 == strncmp(option, OPT_EXCLUDEATTRIBUTE_EQUAL, sizeof(OPT_EXCLUDEATTRIBUTE_EQUAL) - 1)) {
		if (0 != option[sizeof(OPT_EXCLUDEATTRIBUTE_EQUAL)]) {
			UDATA length;
			vm->bytecodeVerificationData->verificationFlags |= J9_VERIFY_EXCLUDE_ATTRIBUTE;
			/* Save the parameter string, NULL terminated and the length excluding the NULL */
			length = strlen(option) - sizeof(OPT_EXCLUDEATTRIBUTE_EQUAL) + 1;
			vm->bytecodeVerificationData->excludeAttribute = j9mem_allocate_memory(length + 1, J9MEM_CATEGORY_CLASSES);
			if (NULL == vm->bytecodeVerificationData->excludeAttribute) {
				if (errorString) {
					*errorString = "Out of memory processing -Xverify:<opt>";
				}
				return FALSE;
			}
			memcpy(vm->bytecodeVerificationData->excludeAttribute, &(option[sizeof(OPT_EXCLUDEATTRIBUTE_EQUAL) - 1]), length + 1);
		}
	} else if (0 == strcmp(option, OPT_BOOTCLASSPATH_STATIC)) {
		vm->bytecodeVerificationData->verificationFlags |= J9_VERIFY_BOOTCLASSPATH_STATIC;
	} else if (0 == strcmp(option, OPT_DO_PROTECTED_ACCESS_CHECK)) {
		vm->bytecodeVerificationData->verificationFlags |= J9_VERIFY_DO_PROTECTED_ACCESS_CHECK;
	} else {
		if (errorString) {
			*errorString = "Unrecognised option(s) for -Xverify:<opt>";
		}
		return FALSE;
	}
	return TRUE;
}



static IDATA 
parseOptions(J9JavaVM *vm, char *optionValues, char **errorString) 
{
	char *optionValue = optionValues;			/* Values are separated by single NULL characters. */

	/* call setVerifyState on each individual option */
	while (*optionValue) {
		if( !setVerifyState( vm, optionValue, errorString ) ) {
			return FALSE;
		}
		optionValue = optionValue + strlen(optionValue) + 1;			/* Step past null separator to next element */
	}
	return TRUE;
}


#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) 
/**
 * Unlink any constraints related to dying classloaders
 */
static void 
bcvHookClassesUnload(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9JavaVM *javaVM = userData;

	if (0 != (javaVM->runtimeFlags & J9_RUNTIME_VERIFY)) {
		unlinkClassLoadingConstraints(javaVM);
	}
}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */


