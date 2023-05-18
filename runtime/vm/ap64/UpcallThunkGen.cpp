/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "j9.h"
#include "ut_j9vm.h"

/**
 * @file: UpCallThunkGen.cpp
 * @brief: Service routines dealing with platform-ABI specifics for upcall
 *
 * Given an upcallMetaData, an upcall thunk/adaptor will be generated;
 * Given an upcallSignature, argListPtr, and argIndex, a pointer to that specific arg will be returned
 */

extern "C" {

#if JAVA_SPEC_VERSION >= 16

/**
 * Macros for instructions expected to be used in thunk generation
 */
#define LD(rt, ra, si)        (0xE8000000 | ((rt) << 21) | ((ra) << 16) | ((si) & 0x0000ffff))
#define STD(rs, ra, si)       (0xF8000000 | ((rs) << 21) | ((ra) << 16) | ((si) & 0x0000ffff))
#define LFS(frt, ra, si)      (0xC0000000 | ((frt) << 21) | ((ra) << 16) | ((si) & 0x0000ffff))
#define STFS(frs, ra, si)     (0xD0000000 | ((frs) << 21) | ((ra) << 16) | ((si) & 0x0000ffff))
#define LFD(frt, ra, si)      (0xC8000000 | ((frt) << 21) | ((ra) << 16) | ((si) & 0x0000ffff))
#define STFD(frs, ra, si)     (0xD8000000 | ((frs) << 21) | ((ra) << 16) | ((si) & 0x0000ffff))
#define ADDI(rt, ra, si)      (0x38000000 | ((rt) << 21) | ((ra) << 16) | ((si) & 0x0000ffff))
#define STDU(rs, ra, si)      (0xF8000001 | ((rs) << 21) | ((ra) << 16) | ((si) & 0x0000ffff))
#define MFLR(rt)              (0x7C0802A6 | ((rt) << 21))
#define MTLR(rs)              (0x7C0803A6 | ((rs) << 21))
#define MTCTR(rs)             (0x7C0903A6 | ((rs) << 21))
#define BCTR()                (0x4E800420)
#define BCTRL()               (0x4E800421)
#define BDNZ(si)              (0x42000000 | ((si) & 0x0000ffff))
#define BLR()                 (0x4E800020)

#define ROUND_UP_SLOT(si)     (((si) + 7) / 8)

/**
 * @brief Generate straight sequence of instructions to copy back result
 * @param instrArray[in/out] A pointer to the thunk memory
 * @param currIdx[in/out] A pointer to the current instruction index
 * @param resSize[in] The size in byte to copy, guarantee to be not more than 64
 * @param paramOffset[in] Offset to the parameter area
 * @return none
 *
 * Details:
 *   a static routine to generate instructions to copy back the upcall result
 *   fixed registers are used
 *     load the hidden parameter into register number 4
 *     load the result in sequence in no more than 8 registers starting from register 5
 *     store these registers back into the memory designated by the hidden parameter
 *
 *     short-cut convenience in handling the residue
 */
static void
copyBackStraight(I_32 *instrArray, I_32 *currIdx, I_32 resSize, I_32 paramOffset)
{
	I_32 localIdx = *currIdx;
	I_32 roundUpSlots = ROUND_UP_SLOT(resSize);

	instrArray[localIdx++] = LD(4, 1, paramOffset);
	for (I_32 gIdx = 0; gIdx < roundUpSlots; gIdx++) {
		instrArray[localIdx++] = LD(gIdx + 5, 3, gIdx * 8);
	}

	for (I_32 gIdx = 0; gIdx < roundUpSlots; gIdx++) {
		instrArray[localIdx++] = STD(gIdx + 5, 4, gIdx * 8);
	}

	*currIdx = localIdx;
}

/**
 * @brief Generate instruction loop to copy back result
 * @param instrArray[in/out] A pointer to the thunk memory
 * @param currIdx[in/out]    A pointer to the current instruction index
 * @param resSize[in] The size in byte to copy, guarantee to be more than 64
 * @param paramOffset[in] Offset to the parameter area
 * @return none
 *
 * Details:
 *   a static routine to generate instruction loop to copy back the upcall result
 *   fixed registers are used
 *     load the hidden parameter into register number 4
 *     set up the loop:  2 instructions
 *     loop itself: 11 instructions (4 load, 4 store, 2 addi, and branch)
 *     load the residue in sequence in no more than 4 registers starting from register 5
 *     store these registers back into the memory designated by the hidden parameter
 *
 *     short-cut convenience in handling the residue
 */
static void
copyBackLoop(I_32 *instrArray, I_32 *currIdx, I_32 resSize, I_32 paramOffset)
{
	I_32 localIdx = *currIdx;
	I_32 roundUpSlots = ROUND_UP_SLOT(resSize & 31);

	instrArray[localIdx++] = LD(4, 1, paramOffset);
	instrArray[localIdx++] = ADDI(0, 0, resSize >> 5);
	instrArray[localIdx++] = MTCTR(0);

	instrArray[localIdx++] = LD(5, 3, 0);
	instrArray[localIdx++] = LD(6, 3, 8);
	instrArray[localIdx++] = LD(7, 3, 16);
	instrArray[localIdx++] = LD(8, 3, 24);
	instrArray[localIdx++] = STD(5, 4, 0);
	instrArray[localIdx++] = STD(6, 4, 8);
	instrArray[localIdx++] = STD(7, 4, 16);
	instrArray[localIdx++] = STD(8, 4, 24);
	instrArray[localIdx++] = ADDI(3, 3, 32);
	instrArray[localIdx++] = ADDI(4, 4, 32);
	instrArray[localIdx++] = BDNZ(-40);

	for (I_32 gIdx = 0; gIdx < roundUpSlots; gIdx++) {
		instrArray[localIdx++] = LD(gIdx + 5, 3, gIdx * 8);
	}

	for (I_32 gIdx = 0; gIdx < roundUpSlots; gIdx++) {
		instrArray[localIdx++] = STD(gIdx + 5, 4, gIdx * 8);
	}

	*currIdx = localIdx;
}

/**
 * @brief Generate the appropriate thunk/adaptor for a given J9UpcallMetaData
 *
 * @param metaData[in/out] a pointer to the given J9UpcallMetaData
 * @return the address for this future upcall function handle, either the thunk or the thunk-descriptor
 *
 * Details:
 *   On AIX, the caller frame always has the parameter area. Unless thunk needs to distribute
 *   result back to the hidden parameter, there is no need to create a new frame. And, if a new
 *   frame is needed, the minimum frame size can be used (112 bytes).
 *
 *   A thunk or adaptor is mainly composed of 4 parts of instructions to be counted separately:
 *   1) the eventual call to the upcallCommonDispatcher (fixed number of instructions)
 *   2) if needed, instructions to build a stack frame
 *   3) pushing in-register arguments back to the stack, in caller frame
 *   4) if needed, instructions to distribute  the java result back to the native side appropriately
 *
 *     1) and 3) are mandatory, while 2) and 4) depend on the particular signature under consideration.
 *     mainly 4) implies needing 2), since this adaptor expects a return from java side before
 *     returning to the native caller.
 */
void *
createUpcallThunk(J9UpcallMetaData *metaData)
{
	J9JavaVM *vm = metaData->vm;
	const J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	J9UpcallSigType *sigArray = metaData->nativeFuncSignature->sigArray;
	/* The index of the return type in the signature array */
	I_32 lastSigIdx = (I_32)(metaData->nativeFuncSignature->numSigs - 1);
	I_32 stackSlotCount = 0;
	I_32 fprCovered = 0;
	I_32 tempInt = 0;
	I_32 instructionCount = 0;
	bool hiddenParameter = false;

	Assert_VM_true(lastSigIdx >= 0);

	/* To call the dispatcher: mv r11 to r3(metaData), load target-desc, load target-addr, set-up argListPtr, mtctr, bctr
	 * Assuming: coming in r11 with metaData (we will fill in the thunk descriptor), r2 already with the right TOC
	 */
	instructionCount = 6;

	/* Testing the return type */
	tempInt = sigArray[lastSigIdx].sizeInByte;
	switch (sigArray[lastSigIdx].type & J9_FFI_UPCALL_SIG_TYPE_MASK) {
		case J9_FFI_UPCALL_SIG_TYPE_VOID:
			metaData->upCallCommonDispatcher = (void *)vmFuncs->native2InterpJavaUpcall0;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_CHAR:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_SHORT: /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_INT32:
			metaData->upCallCommonDispatcher = (void *)vmFuncs->native2InterpJavaUpcall1;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_POINTER: /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_INT64:
			metaData->upCallCommonDispatcher = (void *)vmFuncs->native2InterpJavaUpcallJ;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_FLOAT:
			metaData->upCallCommonDispatcher = (void *)vmFuncs->native2InterpJavaUpcallF;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
			metaData->upCallCommonDispatcher = (void *)vmFuncs->native2InterpJavaUpcallD;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT:
		{
			metaData->upCallCommonDispatcher = (void *)vmFuncs->native2InterpJavaUpcallStruct;
			hiddenParameter = true;
			stackSlotCount += 1;
			if (tempInt <= 64) {
				/* Straight-forward copy: load hidden pointer, sequence of copy */
				instructionCount += 1 + (ROUND_UP_SLOT(tempInt) * 2);
			} else {
				/* Loop 32-byte per iteration: load hidden pointer, set-up CTR for loop-count
				 * 11-instruction loop body, residue copy
				 * Note: didn't optimize for loop-entry alignment
				 */
				instructionCount += 3 + 11 + (ROUND_UP_SLOT(tempInt & 31) * 2);
			}
			break;
		}
		default:
			Assert_VM_unreachable();
	}

	if (hiddenParameter) {
		instructionCount += 1;
	}

	/* Loop through the arguments */
	for (I_32 i = 0; i < lastSigIdx; i++) {
		/* Testing this argument */
		tempInt = sigArray[i].sizeInByte;
		switch (sigArray[i].type & J9_FFI_UPCALL_SIG_TYPE_MASK) {
			case J9_FFI_UPCALL_SIG_TYPE_CHAR:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_SHORT:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT32:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_POINTER: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT64:
			{
				stackSlotCount += 1;
				if (stackSlotCount <= 8) {
					instructionCount += 1;
				}
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_FLOAT: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
			{
				stackSlotCount += 1;
				fprCovered += 1;

				/* GPR portion definitely runs out before FPR does */
				if (stackSlotCount <= 8) {
					instructionCount += 1;
				}
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT:
			{
				stackSlotCount += ROUND_UP_SLOT(tempInt);
				if (stackSlotCount > 8) {
					if ((stackSlotCount - ROUND_UP_SLOT(tempInt)) < 8) {
						instructionCount += 8 + ROUND_UP_SLOT(tempInt) - stackSlotCount;
					}
				} else {
					instructionCount += ROUND_UP_SLOT(tempInt);
				}
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_VA_LIST: /* Unused */
				/* This must be the last argument */
				Assert_VM_true(i == (lastSigIdx - 1));
				break;
			default:
				Assert_VM_unreachable();
		}

		/* Saturate what we want to know: if there are any in-register arguments to be pushed back */
		if (stackSlotCount > 8) {
			break;
		}
	}

	/* 7 instructions to build frame: mflr, save-return-addr, stdu-frame,
	 * addi-tear-down-frame, load-return-addr, mtlr, blr
	 */
	I_32 frameSize = 0;
	I_32 offsetToParamArea = 0;
	I_32 roundedCodeSize = 0;
	I_32 *thunkMem = NULL;  /* always 4-byte instruction: convenient to use int-pointer */

	/* Always use the caller frame */
	if (hiddenParameter) {
		instructionCount += 7;
		frameSize = 112;
		offsetToParamArea = 160;
	} else {
		frameSize = 0;
		offsetToParamArea = 48;
	}

	/* Hopefully thunk memory is 8-byte aligned. We also make sure thunkSize is multiple of 8
	 * another 8-byte to store metaData pointer itself
	 */
	roundedCodeSize = ((instructionCount + 1) / 2) * 8;
	metaData->thunkSize = roundedCodeSize;
	thunkMem = (I_32 *)vmFuncs->allocateUpcallThunkMemory(metaData);
	if (NULL == thunkMem) {
		return NULL;
	}

	metaData->thunkAddress = (void *)thunkMem;

	/* Generate the instruction sequence according to the signature, looping over them again */
	I_32 gprIdx = 3;
	I_32 fprIdx = 1;
	I_32 slotIdx = 0;
	I_32 instrIdx = 0;
	I_32 C_SP = 1;

	if (hiddenParameter) {
		thunkMem[instrIdx++] = MFLR(0);
		thunkMem[instrIdx++] = STD(0, C_SP, 16);
		thunkMem[instrIdx++] = STDU(C_SP, C_SP, -frameSize);
		thunkMem[instrIdx++] = STD(gprIdx++, C_SP, offsetToParamArea);
		slotIdx += 1;
	}

	/* Loop through the arguments again */
	for (I_32 i = 0; i < lastSigIdx; i++) {
		/* Testing this argument */
		tempInt = sigArray[i].sizeInByte;
		switch (sigArray[i].type & J9_FFI_UPCALL_SIG_TYPE_MASK) {
			case J9_FFI_UPCALL_SIG_TYPE_CHAR:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_SHORT:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT32:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_POINTER: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT64:
			{
				if (slotIdx < 8) {
					thunkMem[instrIdx++] = STD(gprIdx, C_SP, offsetToParamArea + (slotIdx * 8));
				}
				gprIdx += 1;
				slotIdx += 1;
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_FLOAT:
			{
				if (slotIdx < 8) {
					thunkMem[instrIdx++] = STFS(fprIdx, C_SP, offsetToParamArea + (slotIdx * 8));
				}
				fprIdx += 1;
				gprIdx += 1;
				slotIdx += 1;
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
			{
				if (slotIdx < 8) {
					thunkMem[instrIdx++] = STFD(fprIdx, C_SP, offsetToParamArea + (slotIdx * 8));
				}
				fprIdx += 1;
				gprIdx += 1;
				slotIdx += 1;
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT:
			{
				if ((slotIdx + ROUND_UP_SLOT(tempInt)) > 8) {
					if (slotIdx < 8) {
						for (I_32 gIdx=0; gIdx < (8 - slotIdx); gIdx++) {
							thunkMem[instrIdx++] = STD(gprIdx + gIdx, C_SP,
								 offsetToParamArea + (slotIdx + gIdx) * 8);
						}
					}
				} else {
					for (I_32 gIdx=0; gIdx < ROUND_UP_SLOT(tempInt); gIdx++) {
						thunkMem[instrIdx++] = STD(gprIdx + gIdx, C_SP,
								offsetToParamArea + (slotIdx + gIdx) * 8);
					}
				}
				gprIdx += ROUND_UP_SLOT(tempInt);
				slotIdx += ROUND_UP_SLOT(tempInt);
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_VA_LIST: /* Unused */
				break;

			default:
				Assert_VM_unreachable();
		}

		/* No additional argument instructions are expected */
		if (slotIdx > 8) {
			break;
		}
	}

	/* Make the jump or call to the common dispatcher.
	 * gr11 is currently pointing at metaData (since we fill it in the thunk-descriptor)
	 * as the env pointer.Also, we assumed TOC register is set up as well during call to
	 * this thunk (we will fill in the thunk-descriptor too).
	 */
	thunkMem[instrIdx++] = LD(12, 11, offsetof(J9UpcallMetaData, upCallCommonDispatcher));
	thunkMem[instrIdx++] = ADDI(3, 11, 0);
	thunkMem[instrIdx++] = LD(0, 12, 0);
	thunkMem[instrIdx++] = ADDI(4, C_SP, offsetToParamArea);
	thunkMem[instrIdx++] = MTCTR(0);

	if (hiddenParameter) {
		thunkMem[instrIdx++] = BCTRL();

		/* Distribute result if needed, then tear down the frame and return */
		tempInt = sigArray[lastSigIdx].sizeInByte;
		switch (sigArray[lastSigIdx].type & J9_FFI_UPCALL_SIG_TYPE_MASK) {
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT:
			{
				if (tempInt <= 64) {
					copyBackStraight(thunkMem, &instrIdx, tempInt, offsetToParamArea);
				} else {
					/* Note: didn't optimize for loop-entry alignment */
					copyBackLoop(thunkMem, &instrIdx, tempInt, offsetToParamArea);
				}
				break;
			}
			default:
				Assert_VM_unreachable();
		}

		thunkMem[instrIdx++] = ADDI(C_SP, C_SP, frameSize);
		thunkMem[instrIdx++] = LD(0, C_SP, 16);
		thunkMem[instrIdx++] = MTLR(0);
		thunkMem[instrIdx++] = BLR();
	} else {
		thunkMem[instrIdx++] = BCTR();
	}

	Assert_VM_true(instrIdx == instructionCount);

	/* Set up the thunk descriptor */
	metaData->functionPtr[0] = (UDATA)thunkMem;
	metaData->functionPtr[1] = *(UDATA *)((char *)(metaData->upCallCommonDispatcher) + 8);
	metaData->functionPtr[2] = (UDATA)metaData;

	/* Finish up before returning */
	vmFuncs->doneUpcallThunkGeneration(metaData, (void *)thunkMem);

	/* Return the thunk descriptor */
	return (void *)(&(metaData->functionPtr));
}

/**
 * @brief Calculate the requested argument in-stack memory address to return
 *
 * @param nativeSig[in] a pointer to the J9UpcallNativeSignature
 * @param argListPtr[in] a pointer to the argument list prepared by the thunk
 * @param argIdx[in] the requested argument index
 * @return address in argument list for the requested argument
 *
 * Details:
 *   A quick walk-through of the argument list ahead of the requested one
 *   Calculating its address based on argListPtr
 */
void *
getArgPointer(J9UpcallNativeSignature *nativeSig, void *argListPtr, I_32 argIdx)
{
	J9UpcallSigType *sigArray = nativeSig->sigArray;
	/* The index for the return type in the signature array */
	I_32 lastSigIdx = (I_32)(nativeSig->numSigs - 1);
	I_32 stackSlotCount = 0;
	I_32 tempInt = 0;

	Assert_VM_true((argIdx >= 0) && (argIdx < lastSigIdx));

	/* Testing the return type */
	tempInt = sigArray[lastSigIdx].sizeInByte;
	if (J9_ARE_ALL_BITS_SET(sigArray[lastSigIdx].type, J9_FFI_UPCALL_SIG_TYPE_STRUCT)) {
		stackSlotCount += 1;
	}

	/* Loop through the arguments */
	for (I_32 i = 0; i < argIdx; i++) {
		/* Testing this argument */
		tempInt = sigArray[i].sizeInByte;
		switch (sigArray[i].type & J9_FFI_UPCALL_SIG_TYPE_MASK) {
			case J9_FFI_UPCALL_SIG_TYPE_CHAR:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_SHORT:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT32:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_POINTER: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT64:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_FLOAT:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
				stackSlotCount += 1;
				break;
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT:
				stackSlotCount += ROUND_UP_SLOT(tempInt);
				break;
			default:
				Assert_VM_unreachable();
		}
	}

	return (void *)((char *)argListPtr + (stackSlotCount * 8));
}

#endif /* JAVA_SPEC_VERSION >= 16 */

} /* extern "C" */
