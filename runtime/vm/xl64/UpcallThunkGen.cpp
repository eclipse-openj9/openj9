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
 *   A thunk or adaptor is mainly composed of 4 parts of instructions to be counted separately:
 *   1) the eventual call to the upcallCommonDispatcher (fixed number of instructions)
 *   2) if needed, instructions to build a stack frame
 *   3) pushing in-register arguments back to the stack, either in newly-built frame or caller frame
 *   4) if needed, instructions to distribute  the java result back to the native side appropriately
 *
 *     1) and 3) are mandatory, while 2) and 4) depend on the particular signature under consideration.
 *     2) is most likely needed, since the caller frame might not contain the parameter area in most cases;
 *     4) implies needing 2), since this adaptor expects a return from java side before returning
 *        to the native caller.
 *
 *   there are two different scenarios under 4):
 *      a) distribute  result back into register-containable aggregates (either homogeneous FP or not)
 *      b) copy the result back to the area designated by the hidden-parameter
 *
 *   most of the complexities are due to handling ALL_SP homogeneous struct: the register image and
 *   memory image are different, such that it can lead to the situation in which FPR parameter registers
 *   run out before GPR parameter registers. In that case, floating point arguments need to be passed
 *   in GPRs (and in the right position if it is an SP).
 *
 *   when a new frame is needed, it is at least 32bytes in size and 16-byte-aligned. this implementation
 *   going as follows: if caller-frame parameter area can be used, the new frame will be of fixed 48-byte
 *   size; otherwise, it will be of [64 + round-up-16(parameterArea)].
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
	bool resultDistNeeded = false;
	bool paramAreaNeeded = true;

	Assert_VM_true(lastSigIdx >= 0);

	/* To call the dispatcher: load metaData, load targetAddress, set-up argListPtr, mtctr, bctr */
	instructionCount = 5;

	/* Testing the return type */
	tempInt = sigArray[lastSigIdx].sizeInByte;
	switch (sigArray[lastSigIdx].type) {
		case J9_FFI_UPCALL_SIG_TYPE_VOID:
			metaData->upCallCommonDispatcher = (void *)vmFuncs->native2InterpJavaUpcall0;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_CHAR:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_SHORT: /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_INT32:
			metaData->upCallCommonDispatcher = (void *)vmFuncs->native2InterpJavaUpcall1;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_POINTER:
		case J9_FFI_UPCALL_SIG_TYPE_INT64:
			metaData->upCallCommonDispatcher = (void *)vmFuncs->native2InterpJavaUpcallJ;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_FLOAT:
			metaData->upCallCommonDispatcher = (void *)vmFuncs->native2InterpJavaUpcallF;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
			metaData->upCallCommonDispatcher = (void *)vmFuncs->native2InterpJavaUpcallD;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP:
		{
			Assert_VM_true(0 == (tempInt % sizeof(float)));
			metaData->upCallCommonDispatcher = (void *)vmFuncs->native2InterpJavaUpcallStruct;
			resultDistNeeded = true;
			if (tempInt <= (I_32)(8 * sizeof(float))) {
				/* Distribute result back into FPRs */
				instructionCount += tempInt/sizeof(float);
			} else {
				/* Copy back to memory area designated by a hidden parameter
				 * Temporarily take a convenient short-cut of always 8-byte
				 */
				stackSlotCount += 1;
				hiddenParameter = true;
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
			}
			break;
		}
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP:
		{
			Assert_VM_true(0 == (tempInt % sizeof(double)));
			metaData->upCallCommonDispatcher = (void *)vmFuncs->native2InterpJavaUpcallStruct;
			resultDistNeeded = true;
			if (tempInt <= (I_32)(8 * sizeof(double))) {
				/* Distribute back into FPRs */
				instructionCount += tempInt/sizeof(double);
			} else {
				/* Loop 32-byte per iteration: load hidden pointer, set-up CTR for loop-count
				 * 11-instruction loop body, residue copy
				 * Note: didn't optimize for loop-entry alignment
				 */
				stackSlotCount += 1;
				hiddenParameter = true;
				instructionCount += 3 + 11 + (ROUND_UP_SLOT(tempInt & 31) * 2);
			}
			break;
		}
		/* Definitely <= 16-byte */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_DP:    /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_SP_DP: /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP:    /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP_SP: /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_SP:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_DP:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_MISC:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_MISC:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC:
		{
			Assert_VM_true(tempInt <= 16);
			metaData->upCallCommonDispatcher = (void *)vmFuncs->native2InterpJavaUpcallStruct;
			resultDistNeeded = true;
			instructionCount += ROUND_UP_SLOT(tempInt);
			break;
		}
		/* Definitely > 16-byte */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_OTHER:
		{
			Assert_VM_true(tempInt > 16);
			metaData->upCallCommonDispatcher = (void *)vmFuncs->native2InterpJavaUpcallStruct;
			resultDistNeeded = true;
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
		switch (sigArray[i].type) {
			case J9_FFI_UPCALL_SIG_TYPE_CHAR:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_SHORT:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT32:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_POINTER: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT64:
			{
				stackSlotCount += 1;
				if (stackSlotCount > 8) {
					paramAreaNeeded = false;
				} else {
					instructionCount += 1;
				}
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_FLOAT:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
			{
				stackSlotCount += 1;
				fprCovered += 1;
				if (fprCovered > 13) {
					if (stackSlotCount > 8) {
						paramAreaNeeded = false;
					} else {
						instructionCount += 1;
					}
				} else {
					instructionCount += 1;
				}
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP:
			{
				Assert_VM_true(0 == (tempInt % sizeof(float)));
				stackSlotCount += ROUND_UP_SLOT(tempInt);
				if (tempInt <= (I_32)(8 * sizeof(float))) {
					if ((fprCovered + (tempInt / sizeof(float))) > 13) {
						/* It is really tricky here. some remaining SPs are passed in
						 * GPRs if there are free GPR parameter registers.
						 */
						I_32 restSlots = 0;
						if (fprCovered < 13) {
							instructionCount += 13 - fprCovered;
							/* Round-down the already passed in FPRs */
							restSlots = ROUND_UP_SLOT(tempInt) - ((13 - fprCovered) / 2);
						} else {
							restSlots = ROUND_UP_SLOT(tempInt);
						}

						Assert_VM_true(restSlots > 0);
						if ((stackSlotCount - restSlots) < 8) {
							if (stackSlotCount > 8) {
								paramAreaNeeded = false;
								instructionCount += 8 - (stackSlotCount - restSlots);
							} else {
								instructionCount += restSlots;
							}
						} else {
							paramAreaNeeded = false;
						}
					} else {
						instructionCount += tempInt / sizeof(float);
					}
					fprCovered += tempInt / sizeof(float);
				} else {
					if (stackSlotCount > 8) {
						paramAreaNeeded = false;
						if ((stackSlotCount - ROUND_UP_SLOT(tempInt)) < 8) {
							instructionCount += 8 + ROUND_UP_SLOT(tempInt) - stackSlotCount;
						}
					} else {
						instructionCount += ROUND_UP_SLOT(tempInt);
					}
				}
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP:
			{
				Assert_VM_true(0 == (tempInt % sizeof(double)));
				stackSlotCount += ROUND_UP_SLOT(tempInt);
				if (tempInt <= (I_32)(8 * sizeof(double))) {
					if ((fprCovered + (tempInt / sizeof(double))) > 13) {
						I_32 restSlots = 0;
						if (fprCovered < 13) {
							instructionCount += 13 - fprCovered;
							restSlots = ROUND_UP_SLOT(tempInt) - (13 - fprCovered);
						} else {
							restSlots = ROUND_UP_SLOT(tempInt);
						}

						Assert_VM_true(restSlots > 0);
						if ((stackSlotCount - restSlots) < 8) {
							if (stackSlotCount > 8) {
								paramAreaNeeded = false;
								instructionCount += 8 - (stackSlotCount - restSlots);
							} else {
								instructionCount += restSlots;
							}
						} else {
							paramAreaNeeded = false;
						}
					} else {
						instructionCount += tempInt / sizeof(double);
					}
					fprCovered += tempInt / sizeof(double);
				} else {
					if (stackSlotCount > 8) {
						paramAreaNeeded = false;
						if ((stackSlotCount - ROUND_UP_SLOT(tempInt)) < 8) {
							instructionCount += 8 + ROUND_UP_SLOT(tempInt) - stackSlotCount;
						}
					} else {
						instructionCount += ROUND_UP_SLOT(tempInt);
					}
				}
				break;
			}
			/* Definitely <= 16-byte */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_DP:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_SP_DP: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP_SP: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_SP:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_DP:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_MISC:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_MISC:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC:
			{
				Assert_VM_true(tempInt <= 16);
				stackSlotCount += ROUND_UP_SLOT(tempInt);
				if (stackSlotCount > 8) {
					paramAreaNeeded = false;
					if ((stackSlotCount - ROUND_UP_SLOT(tempInt)) < 8) {
						instructionCount += 8 + ROUND_UP_SLOT(tempInt) - stackSlotCount;
					}
				} else {
					instructionCount += ROUND_UP_SLOT(tempInt);
				}
				break;
			}
			/* Definitely > 16-byte */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_OTHER:
			{
				Assert_VM_true(tempInt > 16);
				stackSlotCount += ROUND_UP_SLOT(tempInt);
				if (stackSlotCount > 8) {
					paramAreaNeeded = false;
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
				paramAreaNeeded = false;
				break;
			default:
				Assert_VM_unreachable();
		}

		/* Saturate what we want to know: if there are any in-register args to be pushed back */
		if ((stackSlotCount > 8) && (fprCovered > 13)) {
			Assert_VM_true(paramAreaNeeded == false);
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

	if (resultDistNeeded || paramAreaNeeded) {
		instructionCount += 7;
		if (paramAreaNeeded) {
			frameSize = 64 + ((stackSlotCount + 1) / 2) * 16;
			offsetToParamArea = 48;
		} else {
			frameSize = 48;
			/* Using the caller frame paramArea */
			offsetToParamArea = 80;
		}
	} else {
		frameSize = 0;
		/* Using the caller frame paramArea */
		offsetToParamArea = 32;
	}

	/* If a frame is needed, less than 22 slots are expected (8 GPR + 13 FPR) */
	Assert_VM_true(frameSize <= 240);

	/* Hopefully a thunk memory is 8-byte aligned. We also make sure thunkSize is multiple of 8
	 * another 8-byte to store metaData pointer itself
	 */
	roundedCodeSize = ((instructionCount + 1) / 2) * 8;
	metaData->thunkSize = roundedCodeSize + 8;
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

	if (resultDistNeeded || paramAreaNeeded) {
		thunkMem[instrIdx++] = MFLR(0);
		thunkMem[instrIdx++] = STD(0, C_SP, 16);
		thunkMem[instrIdx++] = STDU(C_SP, C_SP, -frameSize);
	}

	if (hiddenParameter) {
		thunkMem[instrIdx++] = STD(gprIdx++, C_SP, offsetToParamArea);
		slotIdx += 1;
	}

	/* Loop through the arguments again */
	for (I_32 i = 0; i < lastSigIdx; i++) {
		/* Testing this argument */
		tempInt = sigArray[i].sizeInByte;
		switch (sigArray[i].type) {
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
				if (fprIdx <= 13) {
					thunkMem[instrIdx++] = STFS(fprIdx, C_SP, offsetToParamArea + (slotIdx * 8));
				} else {
					if (slotIdx < 8) {
						thunkMem[instrIdx++] = STD(gprIdx, C_SP, offsetToParamArea + (slotIdx * 8));
					}
				}
				fprIdx += 1;
				gprIdx += 1;
				slotIdx += 1;
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
			{
				if (fprIdx <= 13) {
					thunkMem[instrIdx++] = STFD(fprIdx, C_SP, offsetToParamArea + (slotIdx * 8));
				} else {
					if (slotIdx < 8) {
						thunkMem[instrIdx++] = STD(gprIdx, C_SP, offsetToParamArea + (slotIdx * 8));
					}
				}
				fprIdx += 1;
				gprIdx += 1;
				slotIdx += 1;
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP:
			{
				if (tempInt <= (I_32)(8 * sizeof(float))) {
					if ((fprIdx + (tempInt / sizeof(float))) > 14) {
						I_32 restSlots = 0;
						I_32 gprStartSlot = 0;

						if (fprIdx <= 13) {
							for (I_32 eIdx = 0; eIdx < (14 - fprIdx); eIdx++) {
								thunkMem[instrIdx++] = STFS(fprIdx + eIdx, C_SP,
									offsetToParamArea + (slotIdx * 8) + (eIdx * sizeof(float)));
							}
							/* Round-down the already passed in FPRs */
							restSlots = ROUND_UP_SLOT(tempInt) - ((14 - fprIdx) / 2);
						} else {
							restSlots = ROUND_UP_SLOT(tempInt);
						}

						if ((gprStartSlot = (slotIdx + ROUND_UP_SLOT(tempInt) - restSlots)) < 8) {
							if ((slotIdx + ROUND_UP_SLOT(tempInt)) > 8) {
								for (I_32 gIdx = 0; gIdx < (8 - gprStartSlot); gIdx++) {
									thunkMem[instrIdx++] = STD(3 + gprStartSlot + gIdx, C_SP,
										offsetToParamArea + ((gprStartSlot + gIdx) * 8));
								}
							} else {
								for (I_32 gIdx = 0; gIdx < restSlots; gIdx++) {
									thunkMem[instrIdx++] = STD(3 + gprStartSlot + gIdx, C_SP,
										offsetToParamArea + ((gprStartSlot + gIdx) * 8));
								}
							}
						}
					} else {
						for (I_32 eIdx = 0; eIdx < (I_32)(tempInt / sizeof(float)); eIdx++) {
							thunkMem[instrIdx++] = STFS(fprIdx + eIdx, C_SP,
								 offsetToParamArea + (slotIdx * 8) + (eIdx * sizeof(float)));
						}
					}

					fprIdx += tempInt / sizeof(float);
				} else {
					if ((slotIdx + ROUND_UP_SLOT(tempInt)) > 8) {
						if (slotIdx < 8) {
							for (I_32 gIdx = 0; gIdx < (8 - slotIdx); gIdx++) {
								thunkMem[instrIdx++] = STD(gprIdx + gIdx, C_SP,
									 offsetToParamArea + (slotIdx + gIdx) * 8);
							}
						}
					} else {
						for (I_32 gIdx = 0; gIdx < ROUND_UP_SLOT(tempInt); gIdx++) {
							thunkMem[instrIdx++] = STD(gprIdx + gIdx, C_SP, offsetToParamArea + (slotIdx + gIdx) * 8);
						}
					}
				}
				gprIdx += ROUND_UP_SLOT(tempInt);
				slotIdx += ROUND_UP_SLOT(tempInt);
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP:
			{
				if (tempInt <= (I_32)(8 * sizeof(double))) {
					if ((fprIdx + (tempInt / sizeof(double))) > 14) {
						I_32 restSlots = 0;
						I_32 gprStartSlot = 0;

						if (fprIdx <= 13) {
							for (I_32 eIdx = 0; eIdx < (14 - fprIdx); eIdx++) {
								thunkMem[instrIdx++] = STFD(fprIdx + eIdx, C_SP,
									offsetToParamArea + (slotIdx + eIdx) * 8);
							}
							restSlots = ROUND_UP_SLOT(tempInt) - (14 - fprIdx);
						} else {
							restSlots = ROUND_UP_SLOT(tempInt);
						}

						if ((gprStartSlot = (slotIdx + ROUND_UP_SLOT(tempInt) - restSlots)) < 8) {
							if ((slotIdx + ROUND_UP_SLOT(tempInt)) > 8) {
								for (I_32 gIdx = 0; gIdx < (8 - gprStartSlot); gIdx++) {
									thunkMem[instrIdx++] = STD(3 + gprStartSlot + gIdx, C_SP,
										offsetToParamArea + ((gprStartSlot + gIdx) * 8));
								}
							} else {
								for (I_32 gIdx = 0; gIdx < restSlots; gIdx++) {
									thunkMem[instrIdx++] = STD(3 + gprStartSlot + gIdx, C_SP,
										offsetToParamArea + ((gprStartSlot + gIdx) * 8));
								}
							}
						}
					} else {
						for (I_32 eIdx = 0; eIdx < (I_32)(tempInt / sizeof(double)); eIdx++) {
							thunkMem[instrIdx++] = STFD(fprIdx + eIdx, C_SP,
									offsetToParamArea + (slotIdx + eIdx) * 8);
						}
					}

					fprIdx += tempInt / sizeof(double);
				} else {
					if ((slotIdx + ROUND_UP_SLOT(tempInt)) > 8) {
						if (slotIdx < 8) {
							for (I_32 gIdx = 0; gIdx < (8 - slotIdx); gIdx++) {
								thunkMem[instrIdx++] = STD(gprIdx + gIdx, C_SP,
									 offsetToParamArea + (slotIdx + gIdx) * 8);
							}
						}
					} else {
						for (I_32 gIdx = 0; gIdx < ROUND_UP_SLOT(tempInt); gIdx++) {
							thunkMem[instrIdx++] = STD(gprIdx + gIdx, C_SP,
									offsetToParamArea + (slotIdx + gIdx) * 8);
						}
					}
				}
				gprIdx += ROUND_UP_SLOT(tempInt);
				slotIdx += ROUND_UP_SLOT(tempInt);
				break;
			}
			/* Definitely <= 16-byte */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_DP:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_SP_DP: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP_SP: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_SP:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_DP:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_MISC:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_MISC:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC:
			{
				if ((slotIdx + ROUND_UP_SLOT(tempInt)) > 8) {
					if (slotIdx < 8) {
						for (I_32 gIdx = 0; gIdx < (8 - slotIdx); gIdx++) {
							thunkMem[instrIdx++] = STD(gprIdx + gIdx, C_SP,
									offsetToParamArea + (slotIdx + gIdx) * 8);
						}
					}
				} else {
					for (I_32 gIdx = 0; gIdx < ROUND_UP_SLOT(tempInt); gIdx++) {
						thunkMem[instrIdx++] = STD(gprIdx + gIdx, C_SP,
								offsetToParamArea + (slotIdx + gIdx) * 8);
					}
				}
				slotIdx += ROUND_UP_SLOT(tempInt);
				gprIdx += ROUND_UP_SLOT(tempInt);
				break;
			}
			/* Definitely > 16-byte */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_OTHER:
			{
				if ((slotIdx + ROUND_UP_SLOT(tempInt)) > 8) {
					if (slotIdx < 8) {
						for (I_32 gIdx = 0; gIdx < (8 - slotIdx); gIdx++) {
							thunkMem[instrIdx++] = STD(gprIdx + gIdx, C_SP,
									offsetToParamArea + (slotIdx + gIdx) * 8);
						}
					}
				} else {
					for (I_32 gIdx = 0; gIdx < ROUND_UP_SLOT(tempInt); gIdx++) {
						thunkMem[instrIdx++] = STD(gprIdx + gIdx, C_SP,
								offsetToParamArea + (slotIdx + gIdx) * 8);
					}
				}
				slotIdx += ROUND_UP_SLOT(tempInt);
				gprIdx += ROUND_UP_SLOT(tempInt);
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_VA_LIST: /* Unused */
				break;
			default:
				Assert_VM_unreachable();
		}

		/* No additional arg instructions are expected */
		if ((slotIdx > 8) && (fprIdx > 13)) {
			break;
		}
	}

	/* Make the jump or call to the common dispatcher.
	 * gr12 is currently pointing at thunkMem (by ABI requirement),
	 * in which case we can load the metaData by a fixed offset
	 */
	thunkMem[instrIdx++] = LD(3, 12, roundedCodeSize);
	thunkMem[instrIdx++] = LD(12, 3, offsetof(J9UpcallMetaData, upCallCommonDispatcher));
	thunkMem[instrIdx++] = ADDI(4, C_SP, offsetToParamArea);
	thunkMem[instrIdx++] = MTCTR(12);

	if (resultDistNeeded || paramAreaNeeded) {
		thunkMem[instrIdx++] = BCTRL();

		/* Distribute result if needed, then tear down the frame and return */
		if (resultDistNeeded) {
			tempInt = sigArray[lastSigIdx].sizeInByte;
			switch (sigArray[lastSigIdx].type) {
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP:
				{
					if (tempInt <= (I_32)(8 * sizeof(float))) {
						for (I_32 fIdx = 0; fIdx < (I_32)(tempInt/sizeof(float)); fIdx++) {
							thunkMem[instrIdx++] = LFS(1+fIdx, 3, fIdx * sizeof(float));
						}
					} else {
						if (tempInt <= 64) {
							copyBackStraight(thunkMem, &instrIdx, tempInt, offsetToParamArea);
						} else {
							/* Note: didn't optimize for loop-entry alignment */
							copyBackLoop(thunkMem, &instrIdx, tempInt, offsetToParamArea);
						}
					}
					break;
				}
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP:
				{
					if (tempInt <= (I_32)(8 * sizeof(double))) {
						for (I_32 fIdx = 0; fIdx < (I_32)(tempInt/sizeof(double)); fIdx++) {
							thunkMem[instrIdx++] = LFD(1+fIdx, 3, fIdx * sizeof(double));
						}
					} else {
						/* Note: didn't optimize for loop-entry alignment */
						copyBackLoop(thunkMem, &instrIdx, tempInt, offsetToParamArea);
					}
					break;
				}
				/* Definitely <= 16-byte */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_DP:    /* Fall through */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_SP_DP: /* Fall through */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP:    /* Fall through */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP_SP: /* Fall through */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_SP:  /* Fall through */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_DP:  /* Fall through */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_MISC:  /* Fall through */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_MISC:  /* Fall through */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC:
				{
					if (tempInt > 8) {
						thunkMem[instrIdx++] = LD(4, 3, 8);
					}
					thunkMem[instrIdx++] = LD(3, 3, 0);
					break;
				}
				/* Definitely > 16-byte */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_OTHER:
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
		}

		thunkMem[instrIdx++] = ADDI(C_SP, C_SP, frameSize);
		thunkMem[instrIdx++] = LD(0, C_SP, 16);
		thunkMem[instrIdx++] = MTLR(0);
		thunkMem[instrIdx++] = BLR();
	} else {
		thunkMem[instrIdx++] = BCTR();
	}

	Assert_VM_true(instrIdx == instructionCount);

	/* Store the metaData pointer */
	*(J9UpcallMetaData **)((char *)thunkMem + roundedCodeSize) = metaData;

	/* Finish up before returning */
	vmFuncs->doneUpcallThunkGeneration(metaData, (void *)thunkMem);

	return (void *)thunkMem;
}

/**
 * @brief Calculate the requested argument in-stack memory address to return
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
	switch (sigArray[lastSigIdx].type) {
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP:
			if (tempInt > (I_32)(8 * sizeof(float))) {
				stackSlotCount += 1;
			}
			break;
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP:
			if (tempInt > (I_32)(8 * sizeof(double))) {
				stackSlotCount += 1;
			}
			break;
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_OTHER:
			stackSlotCount += 1;
			break;
		default:
			break;
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
