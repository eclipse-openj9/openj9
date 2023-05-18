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

static I_32 adjustAlignment(I_32 offset, I_32 alignment)
{
	return (offset + alignment - 1) & ~(alignment - 1);
}

/*
 * Macros for instructions expected to be used in thunk generation
 */
#define LDR(rt, rn, imm12)     (0xF9400000 | (rt) | ((rn) << 5) | (((imm12) & 0x00007ff8) << 7)) // imm12 must be >= 0, multiple of 8
#define STR(rt, rn, imm12)     (0xF9000000 | (rt) | ((rn) << 5) | (((imm12) & 0x00007ff8) << 7)) // imm12 must be >= 0, multiple of 8
#define STRW(rt, rn, imm12)    (0xB9000000 | (rt) | ((rn) << 5) | (((imm12) & 0x00003ffc) << 8)) // imm12 must be >= 0, multiple of 4
#define STRH(rt, rn, imm12)    (0x79000000 | (rt) | ((rn) << 5) | (((imm12) & 0x00001ffe) << 9)) // imm12 must be >= 0, multiple of 2
#define STRB(rt, rn, imm12)    (0x39000000 | (rt) | ((rn) << 5) | (((imm12) & 0x00000fff) << 10)) // imm12 must be >= 0
#define VLDRS(rt, rn, imm12)   (0xBD400000 | (rt) | ((rn) << 5) | (((imm12) & 0x00003ffc) << 8)) // imm12 must be >= 0, multiple of 4
#define VSTRS(rt, rn, imm12)   (0xBD000000 | (rt) | ((rn) << 5) | (((imm12) & 0x00003ffc) << 8)) // imm12 must be >= 0, multiple of 4
#define VLDRD(rt, rn, imm12)   (0xFD400000 | (rt) | ((rn) << 5) | (((imm12) & 0x00007ff8) << 7)) // imm12 must be >= 0, multiple of 8
#define VSTRD(rt, rn, imm12)   (0xFD000000 | (rt) | ((rn) << 5) | (((imm12) & 0x00007ff8) << 7)) // imm12 must be >= 0, multiple of 8
#define ADD(rd, rn, imm12)     (0x91000000 | (rd) | ((rn) << 5) | ((imm12) << 10)) // ADD (immediate) GPR, imm12 must be >= 0
#define SUB(rd, rn, imm12)     (0xD1000000 | (rd) | ((rn) << 5) | ((imm12) << 10)) // SUB (immediate) GPR, imm12 must be >= 0
#define MOV(rd, rm)            (0xaa0003e0 | (rd) | ((rm) << 16)) // MOV (register)
#define MOVZ(rd, imm16)        (0xd2800000 | (rd) | ((imm16) << 5)) // MOVZ
#define MOVK(rd, imm16, lsl)   (0xf2800000 | (rd) | ((imm16) << 5) | ((lsl) << 21) ) // MOVK
#define LSL16 1
#define LSL32 2
#define LSL48 3
#define CBNZ(rt, imm19)        (0xB5000000 | (rt) | (((imm19) & 0x1ffffc) << 3)) // imm19 must be multiple of 4
#define BR(rn)                 (0xD61F0000 | ((rn) << 5))
#define BLR(rn)                (0xD63F0000 | ((rn) << 5)) // branch and link
#define RET(rn)                (0xD65F0000 | ((rn) << 5))
#define C_SP 31 // x31
#define LR 30 // x30
#define HIDDEN_PARAM_REG 8 // x8

#define ROUND_UP_SLOT(si)      (((si) + 7) / 8)
#define ROUND_UP_SLOT_BYTES(si) ((ROUND_UP_SLOT(si)) * 8)

#define MAX_PARAM_GPR_NUM 8
#define MAX_PARAM_FPR_NUM 8

#define PARAM_REGS_SAVE_SIZE (MAX_PARAM_GPR_NUM * 8 + MAX_PARAM_FPR_NUM * 8)

#define MAX_BYTES_COPY_BACK_STRAIGHT 64

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
 *     load the hidden parameter into register x1
 *     load the result in sequence in no more than 8 registers starting from register x2
 *     store these registers back into the memory designated by the hidden parameter
 */
static void
copyBackStraight(I_32 *instrArray, I_32 *currIdx, I_32 resSize, I_32 paramOffset)
{
	I_32 localIdx = *currIdx;
	I_32 roundUpSlots = ROUND_UP_SLOT(resSize);

	/* x0 points to the return value */
	instrArray[localIdx++] = LDR(1, C_SP, paramOffset); /* load hidden parameter */
	for (I_32 gIdx = 0; gIdx < roundUpSlots; gIdx++) {
		instrArray[localIdx++] = LDR(gIdx + 2, 0, gIdx * 8);
	}

	for (I_32 gIdx = 0; gIdx < roundUpSlots; gIdx++) {
		instrArray[localIdx++] = STR(gIdx + 2, 1, gIdx * 8);
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
 *     load the hidden parameter into register x1
 *     set up the loop: 2 instructions
 *     loop itself: 12 instructions (4 load, 4 store, 1 sub, 2 add, and 1 cbnz)
 *     load the residue in sequence in no more than 4 registers starting from register x2
 *     store these registers back into the memory designated by the hidden parameter
 *
 *     short-cut convenience in handling the residue
 */
static void
copyBackLoop(I_32 *instrArray, I_32 *currIdx, I_32 resSize, I_32 paramOffset)
{
	I_32 localIdx = *currIdx;
	I_32 roundUpSlots = ROUND_UP_SLOT(resSize & 31);

	/* x0 points to the return value */
	instrArray[localIdx++] = LDR(1, C_SP, paramOffset); /* load hidden parameter */
	instrArray[localIdx++] = MOVZ(6, resSize >> 5);
	instrArray[localIdx++] = LDR(2, 0, 0);
	instrArray[localIdx++] = LDR(3, 0, 8);
	instrArray[localIdx++] = LDR(4, 0, 16);
	instrArray[localIdx++] = LDR(5, 0, 24);
	instrArray[localIdx++] = STR(2, 1, 0);
	instrArray[localIdx++] = STR(3, 1, 8);
	instrArray[localIdx++] = STR(4, 1, 16);
	instrArray[localIdx++] = STR(5, 1, 24);
	instrArray[localIdx++] = SUB(6, 6, 1);
	instrArray[localIdx++] = ADD(0, 0, 32);
	instrArray[localIdx++] = ADD(1, 1, 32);
	instrArray[localIdx++] = CBNZ(6, -44); /* loop back to LDR(2, 0, 0) */

	for (I_32 gIdx = 0; gIdx < roundUpSlots; gIdx++) {
		instrArray[localIdx++] = LDR(gIdx + 2, 0, gIdx * 8);
	}

	for (I_32 gIdx = 0; gIdx < roundUpSlots; gIdx++) {
		instrArray[localIdx++] = STR(gIdx + 2, 1, gIdx * 8);
	}

	*currIdx = localIdx;
}

/**
 * @brief Generate the appropriate thunk/adaptor for a given J9UpcallMetaData
 *
 * @param metaData[in/out] a pointer to the given J9UpcallMetaData
 * @return the address for this future upcall function handle
 */
void *
createUpcallThunk(J9UpcallMetaData *metaData)
{
	J9JavaVM *vm = metaData->vm;
	const J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	J9UpcallSigType *sigArray = metaData->nativeFuncSignature->sigArray;
	I_32 lastSigIdx = (I_32)(metaData->nativeFuncSignature->numSigs - 1); /* The index of the return type in the signature array */
	I_32 argSize = 0;
	I_32 resultSize = 0;
	I_32 gprIdx = 0;
	I_32 fprIdx = 0;
	I_32 instrCount = 0;
	bool hiddenParameter = false;
	bool resultDistNeeded = false;

	Assert_VM_true(lastSigIdx >= 0);

	/*
	 * Instructions to call the dispatcher: 7
	 *   set metaData to x0 (4 instructions), load target-addr, set up argListPtr, blr
	 * Instructions to extend and restore stack frame: 5
	 *   set up and restore SP (2 instructions), save and restore LR (2 instructions), ret
	 */
	instrCount = 7 + 5;

	/* Testing the return type */
	resultSize = sigArray[lastSigIdx].sizeInByte;
	switch (sigArray[lastSigIdx].type) {
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
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP: /* all floats */
			Assert_VM_true(0 == (resultSize % sizeof(float)));
			metaData->upCallCommonDispatcher = (void *)vmFuncs->native2InterpJavaUpcallStruct;
			resultDistNeeded = true;
			if (resultSize <= (I_32)(4 * sizeof(float))) { /* max 4 regs (s0-s3) */
				/* Distribute result back into FPRs */
				instrCount += resultSize/sizeof(float);
			} else {
				/* Copy back to memory area designated by a hidden parameter */
				hiddenParameter = true;
				if (resultSize <= MAX_BYTES_COPY_BACK_STRAIGHT) {
					/* Straight-forward copy: load hidden pointer, sequence of copy */
					instrCount += 1 + (ROUND_UP_SLOT(resultSize) * 2);
				} else {
					/* Loop 32-byte per iteration: load hidden pointer, set up counter for loop-count */
					/* 12-instruction loop body, residue copy */
					instrCount += 2 + 12 + (ROUND_UP_SLOT(resultSize & 31) * 2);
				}
			}
			break;
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP: /* all doubles */
			Assert_VM_true(0 == (resultSize % sizeof(double)));
			metaData->upCallCommonDispatcher = (void *)vmFuncs->native2InterpJavaUpcallStruct;
			resultDistNeeded = true;
			if (resultSize <= (I_32)(4 * sizeof(double))) { /* max 4 regs (d0-d3) */
				/* Distribute back into FPRs */
				instrCount += resultSize/sizeof(double);
			} else {
				/* Copy back to memory area designated by a hidden parameter */
				hiddenParameter = true;
				if (resultSize <= MAX_BYTES_COPY_BACK_STRAIGHT) {
					/* Straight-forward copy: load hidden pointer, sequence of copy */
					instrCount += 1 + (ROUND_UP_SLOT(resultSize) * 2);
				} else {
					/* Loop 32-byte per iteration: load hidden pointer, set up counter for loop-count */
					/* 12-instruction loop body, residue copy */
					instrCount += 2 + 12 + (ROUND_UP_SLOT(resultSize & 31) * 2);
				}
			}
			break;
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_DP:    /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_SP_DP: /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP:    /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP_SP: /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_SP:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_DP:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_MISC:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_MISC:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_OTHER:    /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC:
			metaData->upCallCommonDispatcher = (void *)vmFuncs->native2InterpJavaUpcallStruct;
			resultDistNeeded = true;
			if (resultSize <= (I_32)(2 * sizeof(I_64))) { /* 16 bytes or smaller */
				/* Distribute result back into GPRs (x0-x1) */
				instrCount += ROUND_UP_SLOT(resultSize);
			} else {
				/* Copy back to memory area designated by a hidden parameter */
				hiddenParameter = true;
				if (resultSize <= MAX_BYTES_COPY_BACK_STRAIGHT) {
					/* Straight-forward copy: load hidden pointer, sequence of copy */
					instrCount += 1 + (ROUND_UP_SLOT(resultSize) * 2);
				} else {
					/* Loop 32-byte per iteration: load hidden pointer, set up counter for loop-count */
					/* 12-instruction loop body, residue copy */
					instrCount += 2 + 12 + (ROUND_UP_SLOT(resultSize & 31) * 2);
				}
			}
			break;
		default:
			Assert_VM_unreachable();
	}

	/* Loop through the arguments */
	for (I_32 i = 0; i < lastSigIdx; i++) {
		argSize = sigArray[i].sizeInByte;
		switch (sigArray[i].type) {
			case J9_FFI_UPCALL_SIG_TYPE_CHAR:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_SHORT:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT32:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_POINTER: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT64:
				if (gprIdx < MAX_PARAM_GPR_NUM) {
					gprIdx++;
					instrCount += 1; /* for saving arg in GPR to stack */
				}
				break;
			case J9_FFI_UPCALL_SIG_TYPE_FLOAT: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
				if (fprIdx < MAX_PARAM_FPR_NUM) {
					fprIdx++;
					instrCount += 1; /* for saving arg in FPR to stack */
				}
				break;
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP:
				if (argSize <= (I_32)(4 * sizeof(float))) {
					I_32 structRegs = argSize/sizeof(float);
					if (fprIdx + structRegs <= MAX_PARAM_FPR_NUM) {
						fprIdx += structRegs;
						instrCount += structRegs; /* for saving arg in FPR to stack */
					} else {
						/* Whole struct is passed in stack */
						fprIdx = MAX_PARAM_FPR_NUM; /* Use no more FPRs */
					}
				} else {
					/* Passed as pointer */
					if (gprIdx < MAX_PARAM_GPR_NUM) {
						gprIdx++;
						instrCount += 1;
					}
				}
				break;
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP:
				if (argSize <= (I_32)(4 * sizeof(double))) {
					I_32 structRegs = argSize/sizeof(double);
					if (fprIdx + structRegs <= MAX_PARAM_FPR_NUM) {
						fprIdx += structRegs;
						instrCount += structRegs; /* for saving arg in FPR to stack */
					} else {
						/* Whole struct is passed in stack */
						fprIdx = MAX_PARAM_FPR_NUM; /* Use no more FPRs */
					}
				} else {
					/* Passed as pointer */
					if (gprIdx < MAX_PARAM_GPR_NUM) {
						gprIdx++;
						instrCount += 1;
					}
				}
				break;
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_DP:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_SP_DP: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP_SP: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_SP:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_DP:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_MISC:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_MISC:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_OTHER:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC:
				if (argSize <= (I_32)(2 * sizeof(I_64))) {
					I_32 structRegs = ROUND_UP_SLOT(argSize);
					if (gprIdx + structRegs <= MAX_PARAM_GPR_NUM) {
						gprIdx += structRegs;
						instrCount += structRegs; /* for saving args in GPR to stack */
					} else {
						/* Whole struct is passed in stack */
						gprIdx = MAX_PARAM_GPR_NUM; /* Use no more GPRs */
					}
				} else {
					/* Passed as pointer */
					if (gprIdx < MAX_PARAM_GPR_NUM) {
						gprIdx++;
						instrCount += 1;
					}
				}
				break;
			case J9_FFI_UPCALL_SIG_TYPE_VA_LIST: /* Unused */
				/* This must be the last argument */
				Assert_VM_true(i == (lastSigIdx - 1));
				break;
			default:
				Assert_VM_unreachable();
		}
	}

	I_32 offsetToParamArea = 16; /* SP, LR */

	if (hiddenParameter) {
		/* for saving hiddenParam */
		instrCount += 1;
		offsetToParamArea += 8;
	}

	I_32 frameSize0 = PARAM_REGS_SAVE_SIZE + offsetToParamArea;
	I_32 frameSize = adjustAlignment(frameSize0, 16); /* SP must always align to 16-byte boundary on AArch64 */
	offsetToParamArea += (frameSize - frameSize0); /* Adjust the offset */

	/* Hopefully thunk memory is 8-byte aligned. We also make sure thunkSize is multiple of 8 */
	I_32 roundedCodeSize = ((instrCount + 1) / 2) * 8;
	metaData->thunkSize = roundedCodeSize;
	I_32 *thunkMem = (I_32 *)vmFuncs->allocateUpcallThunkMemory(metaData); /* always 4-byte instruction: convenient to use int-pointer */
	if (NULL == thunkMem) {
		return NULL;
	}

	metaData->thunkAddress = (void *)thunkMem;

	/* Generate the instruction sequence according to the signature, looping over them again */
	/* Thunk always saves the arguments at the 8-byte boundary in the extended stack */
	gprIdx = 0;
	fprIdx = 0;
	I_32 stackOffset = 0;
	I_32 instrIdx = 0;
	const I_32 LR_OFFSET = 8; /* offset for saving the return address */
	const I_32 HIDDEN_PARAM_OFFSET = 16; /* offset for saving the hidden param */

	omrthread_jit_write_protect_disable();

	thunkMem[instrIdx++] = SUB(C_SP, C_SP, frameSize);
	thunkMem[instrIdx++] = STR(LR, C_SP, LR_OFFSET);

	if (hiddenParameter) {
		/* save hidden param to stack */
		thunkMem[instrIdx++] = STR(HIDDEN_PARAM_REG, C_SP, HIDDEN_PARAM_OFFSET);
	}

	/* Loop through the arguments again */
	for (I_32 i = 0; i < lastSigIdx; i++) {
		argSize = sigArray[i].sizeInByte;
		switch (sigArray[i].type) {
			case J9_FFI_UPCALL_SIG_TYPE_CHAR:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_SHORT:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT32:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_POINTER: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT64:
				if (gprIdx < MAX_PARAM_GPR_NUM) {
					thunkMem[instrIdx++] = STR(gprIdx, C_SP, offsetToParamArea + stackOffset);
					gprIdx++;
					stackOffset += 8;
				}
				break;
			case J9_FFI_UPCALL_SIG_TYPE_FLOAT:
				if (fprIdx < MAX_PARAM_FPR_NUM) {
					thunkMem[instrIdx++] = VSTRS(fprIdx, C_SP, offsetToParamArea + stackOffset);
					fprIdx++;
					stackOffset += 8;
				}
				break;
			case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
				if (fprIdx < MAX_PARAM_FPR_NUM) {
					thunkMem[instrIdx++] = VSTRD(fprIdx, C_SP, offsetToParamArea + stackOffset);
					fprIdx++;
					stackOffset += 8;
				}
				break;
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP:
				if (argSize <= (I_32)(4 * sizeof(float))) {
					I_32 structRegs = argSize/sizeof(float);
					if (fprIdx + structRegs <= MAX_PARAM_FPR_NUM) {
						for (I_32 fIdx=0; fIdx < structRegs; fIdx++) {
							thunkMem[instrIdx++] = VSTRS(fprIdx + fIdx, C_SP,
									offsetToParamArea + stackOffset + fIdx * 4);
						}
						fprIdx += structRegs;
						stackOffset += adjustAlignment(argSize, 8);
					} else {
						/* Whole struct is passed in stack */
						fprIdx = MAX_PARAM_FPR_NUM; /* Use no more FPRs */
					}
				} else {
					/* Passed as pointer */
					if (gprIdx < MAX_PARAM_GPR_NUM) {
						thunkMem[instrIdx++] = STR(gprIdx, C_SP, offsetToParamArea + stackOffset);
						gprIdx++;
						stackOffset += 8;
					}
				}
				break;
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP:
				if (argSize <= (I_32)(4 * sizeof(double))) {
					I_32 structRegs = argSize/sizeof(double);
					if (fprIdx + structRegs <= MAX_PARAM_FPR_NUM) {
						for (I_32 fIdx=0; fIdx < structRegs; fIdx++) {
							thunkMem[instrIdx++] = VSTRD(fprIdx + fIdx, C_SP,
									offsetToParamArea + stackOffset + fIdx * 8);
						}
						fprIdx += structRegs;
						stackOffset += argSize;
					} else {
						/* Whole struct is passed in stack */
						fprIdx = MAX_PARAM_FPR_NUM; /* Use no more FPRs */
					}
				} else {
					/* Passed as pointer */
					if (gprIdx < MAX_PARAM_GPR_NUM) {
						thunkMem[instrIdx++] = STR(gprIdx, C_SP, offsetToParamArea + stackOffset);
						gprIdx++;
						stackOffset += 8;
					}
				}
				break;
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_DP:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_SP_DP: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP_SP: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_SP:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_DP:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_MISC:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_MISC:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_OTHER:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC:
				if (argSize <= (I_32)(2 * sizeof(I_64))) {
					I_32 structRegs = ROUND_UP_SLOT(argSize);
					if (gprIdx + structRegs <= MAX_PARAM_GPR_NUM) {
						for (I_32 gIdx=0; gIdx < structRegs; gIdx++) {
							thunkMem[instrIdx++] = STR(gprIdx + gIdx, C_SP,
									offsetToParamArea + stackOffset + gIdx * 8);
						}
						gprIdx += structRegs;
						stackOffset += structRegs * 8;
					} else {
						/* Whole struct is passed in stack */
						gprIdx = MAX_PARAM_GPR_NUM; /* Use no more GPRs */
					}
				} else {
					/* Passed as pointer */
					if (gprIdx < MAX_PARAM_GPR_NUM) {
						thunkMem[instrIdx++] = STR(gprIdx, C_SP, offsetToParamArea + stackOffset);
						gprIdx++;
						stackOffset += 8;
					}
				}
				break;
			case J9_FFI_UPCALL_SIG_TYPE_VA_LIST: /* Unused */
				break;
			default:
				Assert_VM_unreachable();
		}
	}

	/*
	 * Make the call to the common dispatcher
	 *   x0: metaData
	 *   x1: list of parameters in the stack
	 */
	thunkMem[instrIdx++] = MOVZ(0, (reinterpret_cast<uint64_t>(metaData) & 0xFFFF));
	thunkMem[instrIdx++] = MOVK(0, ((reinterpret_cast<uint64_t>(metaData) >> 16) & 0xFFFF), LSL16);
	thunkMem[instrIdx++] = MOVK(0, ((reinterpret_cast<uint64_t>(metaData) >> 32) & 0xFFFF), LSL32);
	thunkMem[instrIdx++] = MOVK(0, ((reinterpret_cast<uint64_t>(metaData) >> 48) & 0xFFFF), LSL48);
	thunkMem[instrIdx++] = LDR(2, 0, offsetof(J9UpcallMetaData, upCallCommonDispatcher));
	thunkMem[instrIdx++] = ADD(1, C_SP, offsetToParamArea);

	thunkMem[instrIdx++] = BLR(2); /* call the common dispatcher */

	if (resultDistNeeded) {
		resultSize = sigArray[lastSigIdx].sizeInByte;
		if (hiddenParameter) {
			/* Distribute result if needed, then tear down the frame and return */
			/* x0 is set by the call */
			switch (sigArray[lastSigIdx].type & J9_FFI_UPCALL_SIG_TYPE_MASK) {
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT:
				{
					if (resultSize <= MAX_BYTES_COPY_BACK_STRAIGHT) {
						copyBackStraight(thunkMem, &instrIdx, resultSize, HIDDEN_PARAM_OFFSET);
					} else {
						copyBackLoop(thunkMem, &instrIdx, resultSize, HIDDEN_PARAM_OFFSET);
					}
					break;
				}
				default:
					Assert_VM_unreachable();
			}
		} else {
			switch (sigArray[lastSigIdx].type) {
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP:
					for (I_32 fIdx = 0; (I_32)(fIdx * sizeof(float)) < resultSize; fIdx++) {
						thunkMem[instrIdx++] = VLDRS(fIdx, 0, fIdx * 4);
					}
					break;
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP:
					for (I_32 fIdx = 0; (I_32)(fIdx * sizeof(double)) < resultSize; fIdx++) {
						thunkMem[instrIdx++] = VLDRD(fIdx, 0, fIdx * 8);
					}
					break;
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_DP:    /* Fall through */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_SP_DP: /* Fall through */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP:    /* Fall through */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP_SP: /* Fall through */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_SP:  /* Fall through */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_DP:  /* Fall through */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_MISC:  /* Fall through */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_MISC:  /* Fall through */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_OTHER:    /* Fall through */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC:
					for (I_32 gIdx = 1; (I_32)(gIdx * sizeof(I_64)) < ROUND_UP_SLOT_BYTES(resultSize); gIdx++) {
						thunkMem[instrIdx++] = LDR(gIdx, 0, gIdx * 8);
					}
					thunkMem[instrIdx++] = LDR(0, 0, 0); /* Load x0 at last */
					break;
				default:
					Assert_VM_unreachable();
			}
		}
	}

	thunkMem[instrIdx++] = LDR(LR, C_SP, LR_OFFSET); /* load return address */
	thunkMem[instrIdx++] = ADD(C_SP, C_SP, frameSize); /* restore C_SP */
	thunkMem[instrIdx++] = RET(LR); /* return to the caller */

	Assert_VM_true(instrIdx == instrCount);

	omrthread_jit_write_protect_enable();

	/* Finish up before returning */
	vmFuncs->doneUpcallThunkGeneration(metaData, (void *)(metaData->thunkAddress));

	/* Return the thunk descriptor */
	return (void *)(metaData->thunkAddress);
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
	I_32 lastSigIdx = (I_32)(nativeSig->numSigs - 1); /* The index for the return type in the signature array */
	I_32 stackOffset = 0;
	I_32 stackOffset1 = 0; /* for arguments in the stack extended by the thunk */
	I_32 stackOffset2 = PARAM_REGS_SAVE_SIZE; /* for arguments in the original stack */
	I_32 gprIdx = 0;
	I_32 fprIdx = 0;
	bool isPointerToStruct = false;
	void *ret = NULL;

	Assert_VM_true((argIdx >= 0) && (argIdx < lastSigIdx));

	/* Loop through the arguments */
	for (I_32 i = 0; i <= argIdx; i++) {
		I_32 argSize = sigArray[i].sizeInByte;
		isPointerToStruct = false;
		switch (sigArray[i].type) {
			case J9_FFI_UPCALL_SIG_TYPE_CHAR:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_SHORT:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT32:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_POINTER: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT64:
				if (gprIdx < MAX_PARAM_GPR_NUM) {
					stackOffset = stackOffset1;
					stackOffset1 += 8;
					gprIdx++;
				} else {
#if defined(OSX)
					if (argSize > 1) {
						stackOffset2 = adjustAlignment(stackOffset2, argSize);
					}
#endif /* defined(OSX) */
					stackOffset = stackOffset2;
#if defined(OSX)
					stackOffset2 += argSize;
#elif defined(LINUX)
					stackOffset2 += 8;
#endif /* defined(OSX) */
				}
				break;
			case J9_FFI_UPCALL_SIG_TYPE_FLOAT:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
				if (fprIdx < MAX_PARAM_FPR_NUM) {
					stackOffset = stackOffset1;
					stackOffset1 += 8;
					fprIdx++;
				} else {
#if defined(OSX)
					stackOffset2 = adjustAlignment(stackOffset2, argSize);
#endif /* defined(OSX) */
					stackOffset = stackOffset2;
#if defined(OSX)
					stackOffset2 += argSize;
#elif defined(LINUX)
					stackOffset2 += 8;
#endif /* defined(OSX) */
				}
				break;
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP:
				if (argSize <= (I_32)(4 * sizeof(float))) {
					I_32 structRegs = argSize/sizeof(float);
					if (fprIdx + structRegs <= MAX_PARAM_FPR_NUM) {
						stackOffset = stackOffset1;
						stackOffset1 += adjustAlignment(argSize, 8);
						fprIdx += structRegs;
					} else {
						/* Whole struct is passed in stack */
#if defined(OSX)
						stackOffset2 = adjustAlignment(stackOffset2, 4);
#endif /* defined(OSX) */
						stackOffset = stackOffset2;
#if defined(OSX)
						stackOffset2 += argSize;
#elif defined(LINUX)
						stackOffset2 += adjustAlignment(argSize, 8);
#endif /* defined(OSX) */
						fprIdx = MAX_PARAM_FPR_NUM; /* Use no more FPRs */
					}
				} else {
					/* Passed as pointer */
					isPointerToStruct = true;
					if (gprIdx < MAX_PARAM_GPR_NUM) {
						stackOffset = stackOffset1;
						stackOffset1 += 8;
						gprIdx++;
					} else {
#if defined(OSX)
						stackOffset2 = adjustAlignment(stackOffset2, 8);
#endif /* defined(OSX) */
						stackOffset = stackOffset2;
						stackOffset2 += 8;
					}
				}
				break;
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP:
				if (argSize <= (I_32)(4 * sizeof(double))) {
					I_32 structRegs = argSize/sizeof(double);
					if (fprIdx + structRegs <= MAX_PARAM_FPR_NUM) {
						stackOffset = stackOffset1;
						stackOffset1 += argSize;
						fprIdx += structRegs;
					} else {
						/* Whole struct is passed in stack */
#if defined(OSX)
						stackOffset2 = adjustAlignment(stackOffset2, 8);
#endif /* defined(OSX) */
						stackOffset = stackOffset2;
						stackOffset2 += argSize;
						fprIdx = MAX_PARAM_FPR_NUM; /* Use no more FPRs */
					}
				} else {
					/* Passed as pointer */
					isPointerToStruct = true;
					if (gprIdx < MAX_PARAM_GPR_NUM) {
						stackOffset = stackOffset1;
						stackOffset1 += 8;
						gprIdx++;
					} else {
#if defined(OSX)
						stackOffset2 = adjustAlignment(stackOffset2, 8);
#endif /* defined(OSX) */
						stackOffset = stackOffset2;
						stackOffset2 += 8;
					}
				}
				break;
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_DP:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_SP_DP: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP_SP: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_SP:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_DP:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_MISC:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_MISC:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_OTHER:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC:
				if (argSize <= (I_32)(2 * sizeof(I_64))) {
					I_32 structRegs = ROUND_UP_SLOT(argSize);
					if (gprIdx + structRegs <= MAX_PARAM_GPR_NUM) {
						stackOffset = stackOffset1;
						stackOffset1 += structRegs * 8;
						gprIdx += structRegs;
					} else {
						/* Whole struct is passed in stack */
#if defined(OSX)
						stackOffset2 = adjustAlignment(stackOffset2, 8);
#endif /* defined(OSX) */
						stackOffset = stackOffset2;
						stackOffset2 += structRegs * 8;
						gprIdx = MAX_PARAM_GPR_NUM; /* Use no more GPRs */
					}
				} else {
					/* Passed as pointer */
					isPointerToStruct = true;
					if (gprIdx < MAX_PARAM_GPR_NUM) {
						stackOffset = stackOffset1;
						stackOffset1 += 8;
						gprIdx++;
					} else {
#if defined(OSX)
						stackOffset2 = adjustAlignment(stackOffset2, 8);
#endif /* defined(OSX) */
						stackOffset = stackOffset2;
						stackOffset2 += 8;
					}
				}
				break;
			default:
				Assert_VM_unreachable();
		}
	}

	ret = (void *)((char *)argListPtr + stackOffset);

	if (isPointerToStruct) {
		ret = *((void **)ret);
	}

	return ret;
}

#endif /* JAVA_SPEC_VERSION >= 16 */

} /* extern "C" */
