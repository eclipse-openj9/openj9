/*******************************************************************************
 * Copyright IBM Corp. and others 2024
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
#include "FFIUpcallThunkGenHelpers.hpp"
#include <algorithm>

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
 * @brief Generate the appropriate thunk/adaptor for a given J9UpcallMetaData
 *
 * @param metaData[in/out] a pointer to the given J9UpcallMetaData
 * @return the address for this future upcall function handle, either the thunk or the thunk-descriptor
 *
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
	const I_32 lastSigIdx = (I_32)(metaData->nativeFuncSignature->numSigs - 1);
	U_8 returnType = sigArray[lastSigIdx].type;
	U_32 returnValueSize = sigArray[lastSigIdx].sizeInByte;

	Assert_VM_true(lastSigIdx >= 0);

	I_32 numInstructions = 10;
	I_32 numStackSlots = 0;
	bool hiddenParameter = false;

	// Test the return type
	switch (returnType & J9_FFI_UPCALL_SIG_TYPE_MASK) {
	case J9_FFI_UPCALL_SIG_TYPE_VOID:
		metaData->upCallCommonDispatcher = (void *)vmFuncs->native2InterpJavaUpcall0;
		break;
	case J9_FFI_UPCALL_SIG_TYPE_CHAR:
	case J9_FFI_UPCALL_SIG_TYPE_SHORT:
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
	case J9_FFI_UPCALL_SIG_TYPE_STRUCT:
		metaData->upCallCommonDispatcher = (void *)vmFuncs->native2InterpJavaUpcallStruct;
		if ((returnType == J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP && returnValueSize == (2 * sizeof(float)))
			|| (returnType == J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP && returnValueSize == (2 * sizeof(double)))) {
			numInstructions += 2;
		}
		else if (returnValueSize <= 8) {
			// A single instruction, LG, is used to load data from [R3] into R1
			numInstructions++;
		}
		else if (returnValueSize <= 16) {
			numInstructions += 2;
		}
		else if (returnValueSize <= 24) {
			numInstructions += 3;
		}
		else {
			// all other structs are stored in memory
			hiddenParameter = true;
			if (returnValueSize <= MAX_MVC_DISPLACEMENT) {
				numInstructions += returnValueSize / MAX_MVC_COPY_LENGTH;
				numInstructions += returnValueSize % MAX_MVC_COPY_LENGTH == 0 ? 2 : 3;
			}
			else {
				const I_32 numFullSlots = returnValueSize / MAX_MVC_COPY_LENGTH;
				const I_32 residue = returnValueSize % MAX_MVC_COPY_LENGTH;
				const I_32 maxLargeCopies = MAX_MVC_DISPLACEMENT / MAX_MVC_COPY_LENGTH;
				const I_32 loopResidue = numFullSlots % maxLargeCopies;
				// the MVC sequence to copy large structs always generates 7 extra instructions on top of the MVC instructions.
				numInstructions += maxLargeCopies + loopResidue + residue + 7;
			}
		}
		break;
	default:
		Assert_VM_unreachable();
	}

	I_32 gprArgCount = 0;
	I_32 fprArgCount = 0;
	// We have 3 GPRs (R1 to R3) and 4 FPRs (F0, F2, F4, F6) available for parameter passing.
	const I_32 maxGPRArgCount = 3;
	const I_32 maxFPRArgCount = 4;

	// Loop through the arguments and count how many instructions you'll need to copy data from registers to memory, and how many
	// bytes of space you'll need to allocate in thunk and new stack frame to store the instructions + parameter info.
	for (I_32 i = 0; i < lastSigIdx; i++) {
		U_32 argSizeInBytes = sigArray[i].sizeInByte;
		switch (sigArray[i].type) {
		case J9_FFI_UPCALL_SIG_TYPE_CHAR:    /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_SHORT:   /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_INT32:   /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_POINTER: /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_INT64:
			if (gprArgCount < maxGPRArgCount) {
				numInstructions++;
				gprArgCount++;
			}
			break;
		case J9_FFI_UPCALL_SIG_TYPE_FLOAT:
		case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
			if (fprArgCount < maxFPRArgCount) {
				numInstructions++;
				fprArgCount++;
			}
			if (i < 3) {
				gprArgCount++; // If one of the first three doublewords of the argument area is float/double, then the corresponding GPR is undefined. So we must ignore any value in there.
			}
			break;
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP:
			if (argSizeInBytes == 2 * sizeof(float)) {
				// a pure float struct with only 2 floats is passed in 2 FPRs.
				// If only one FPR is free, then the first half of the struct is
				// in FPR6, and the second half is in the corresponding slot
				// in the argument area, right adjusted.
				if (fprArgCount <= maxFPRArgCount - 2) {
					numInstructions += 2;
					fprArgCount += 2;
					if (i < 3) {
						gprArgCount += 2;
					}
				}
				else if (fprArgCount < maxFPRArgCount) {
					numInstructions++;
					fprArgCount++;
				}
			}
			else { // All other pure float structs are passed in GPR (if there's space)
				if (gprArgCount < maxGPRArgCount) {
					// Calculate + round up the size in terms of the number of doublewords. This field, gprsNeededForEntireStruct, represents the
					// number of GPRs we would hypothetically need to load the entire struct in GPRs.
					int gprsNeededForEntireStruct = ((argSizeInBytes - 1) / 8) + 1;

					// We may not have all the GPRs available to store the entire struct. In that case, we store only what's in GPR
					// (the rest of the struct will already be in memory.)
					int numGPRsAvailable = maxGPRArgCount - gprArgCount;
					numInstructions += std::min(gprsNeededForEntireStruct, numGPRsAvailable);
					gprArgCount += std::min(gprsNeededForEntireStruct, numGPRsAvailable);
				}
			}
			break;
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP:
			if (argSizeInBytes == 2 * sizeof(double)) {
				// a pure double struct with only 2 floats is passed in 2 FPRs.
				// If only one FPR is free, then the first half of the struct is
				// in FPR6, and the second half is in the corresponding memory slot
				// in the stack's argument area.
				if (fprArgCount <= maxFPRArgCount - 2) {
					numInstructions += 2;
					fprArgCount += 2;
					if (i < 3) {
						gprArgCount += 2;
					}
				}
				else if (fprArgCount < maxFPRArgCount) {
					numInstructions++;
					fprArgCount++;
				}
			}
			else { // All other pure float structs are passed in GPR (if there's space)
				if (gprArgCount < maxGPRArgCount) {
					// Calculate the size in terms of the number of doublewords. This field, gprsNeededForEntireStruct, represents the
					// number of GPRs we would hypothetically need to load the entire struct in GPRs.
					int gprsNeededForEntireStruct = argSizeInBytes / 8;
					int numGPRsAvailable = maxGPRArgCount - gprArgCount;
					numInstructions += std::min(gprsNeededForEntireStruct, numGPRsAvailable);
					gprArgCount += std::min(gprsNeededForEntireStruct, numGPRsAvailable);
				}
			}
			break;
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_DP:
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_SP_DP:
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP:
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP_SP:
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_SP:
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_DP:
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_MISC:
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_MISC:
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_OTHER:
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC:
		{
			// Calculate + round up the size in terms of the number of doublewords. This field, gprsNeededForEntireStruct, represents the
			// number of GPRs we would hypothetically need to load the entire struct in GPRs.
			int gprsNeededForEntireStruct = ((argSizeInBytes - 1) / 8) + 1;
			int numGPRsAvailable = maxGPRArgCount - gprArgCount;
			numInstructions += std::min(gprsNeededForEntireStruct, numGPRsAvailable);
			gprArgCount += std::min(gprsNeededForEntireStruct, numGPRsAvailable);
		}
			break;
		default:
			Assert_VM_unreachable();
		}
		// On z/OS the first four double/float type arguments are passed in FPRs, and the first three arguments of any other type
		// are passed in GPRs 1-3. If we have passed those limits while iterating through the argument list, then we can stop
		// counting the number of instructions. We won't need to store the remaining parameters because they are already
		// in memory.
		if ((fprArgCount >= 4) && (gprArgCount >= 3)) {
			break;
		}
	}

	// The thunk will frame will be 256 bytes in size. Since parameters will be stored in the caller frame, offsetFromR4
	// must account for the frame size of the thunk.
	const I_32 defaultStackPointerBias = 2048;
	const I_32 offsetToParamArea = 128;
	const I_32 thunkFrameSize = 256;
	const I_32 argAreaOffsetFromSP = defaultStackPointerBias + offsetToParamArea + thunkFrameSize;
	const I_32 registerSaveAreaSize = 96;
	I_8 *thunkMem = NULL;

	// Size of thunk including the numInstructions generated to store all arguments in memory, 8-byte aligned.
	metaData->thunkSize = (((numInstructions * 6) / 8) + 1) * 8;
	thunkMem = (I_8 *)vmFuncs->allocateUpcallThunkMemory(metaData);
	I_8 *thunkMemStart = thunkMem;
	if (NULL == thunkMem) {
		return NULL;
	}
	metaData->thunkAddress = (void *)thunkMem;

	U_32 gprIndex = 1;
	const U_32 maxGPRIndex = 3;
	U_32 fprIndex = 0;
	const U_32 maxFPRIndex = 6;
	U_32 currStackSlotIndex = 0;

	// Now we loop through the arguments and generate the actual instructions to store any register arguments in the caller frame.
	const I_32 numRegsToPreserve = 10;
	const I_32 offsetToRegSaveArea = defaultStackPointerBias - thunkFrameSize + registerSaveAreaSize - (numRegsToPreserve * 8); // This should evaluate to 1808.
	thunkMem += STMG(thunkMem, R6, R15, R4, offsetToRegSaveArea);
	thunkMem += LAY(thunkMem, R4, 0, R4, -thunkFrameSize);
	if (hiddenParameter) {
		thunkMem += STG(thunkMem, gprIndex, 0, R4, argAreaOffsetFromSP + (currStackSlotIndex * 8));
		gprIndex++;
		currStackSlotIndex++;
	}
	for (I_32 i = 0; i < lastSigIdx; i++) {
		U_32 argSizeInBytes = sigArray[i].sizeInByte;
		const U_32 totalStackSlotsReq = (argSizeInBytes + 7) / 8;
		switch (sigArray[i].type) {
		case J9_FFI_UPCALL_SIG_TYPE_CHAR:    /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_SHORT:   /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_INT32:   /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_POINTER: /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_INT64:
			if (gprIndex <= maxGPRIndex) {
				thunkMem += STG(thunkMem, gprIndex, 0, R4, argAreaOffsetFromSP + (currStackSlotIndex * 8));
				gprIndex += 1;
			}
			currStackSlotIndex += 1;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_FLOAT:
			if (fprIndex <= maxFPRIndex) {
				thunkMem += STEY(thunkMem, fprIndex, 0, R4, argAreaOffsetFromSP + (currStackSlotIndex * 8));
				fprIndex += 2;
			}
			if (i < 3) {
				gprIndex++; // If one of the first three doublewords of the argument area is float/double, then the corresponding GPR is undefined. So we must ignore any value in there.
			}
			currStackSlotIndex += 1;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
			if (fprIndex <= maxFPRIndex) {
				thunkMem += STDY(thunkMem, fprIndex, 0, R4, argAreaOffsetFromSP + (currStackSlotIndex * 8));
				fprIndex += 2;
			}
			if (i < 3) {
				gprIndex++; // If one of the first three doublewords of the argument area is float/double, then the corresponding GPR is undefined. So we must ignore any value in there.
			}
			currStackSlotIndex += 1;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP:
			if (argSizeInBytes == 2 * sizeof(float)) {
				// a pure float struct with only 2 floats is passed in 2 FPRs.
				// If only one FPR is free (i.e. FPR6), then the first half of the struct is
				// in FPR6, and the second half is in the corresponding slot
				// in the argument area, right adjusted.
				if (fprIndex <= maxFPRIndex - 2) {
					thunkMem += STEY(thunkMem, fprIndex, 0, R4, argAreaOffsetFromSP + (currStackSlotIndex * 8));
					thunkMem += STEY(thunkMem, fprIndex + 2, 0, R4, argAreaOffsetFromSP + (currStackSlotIndex * 8) + 4);
					fprIndex += 4;
					if (i < 3) {
						gprIndex += 2;
					}
				}
				else if (fprIndex <= maxFPRIndex) {
					thunkMem += STEY(thunkMem, fprIndex, 0, R4, argAreaOffsetFromSP + (currStackSlotIndex * 8));
					fprIndex += 2;
				}
			}
			else { // All other pure float structs are passed in GPR (if there's space)
				if (gprIndex <= maxGPRIndex) {
					U_32 numGPRsAvailable = maxGPRIndex - gprIndex + 1;
					U_32 tempArgIndex = currStackSlotIndex;
						for (U_32 i = 1; i <= std::min(numGPRsAvailable, totalStackSlotsReq); i++) {
							thunkMem += STG(thunkMem, gprIndex, 0, R4, argAreaOffsetFromSP + (tempArgIndex * 8));
							tempArgIndex += 1;
							gprIndex++;
						}
				}
			}
			currStackSlotIndex += totalStackSlotsReq;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP:
			if (argSizeInBytes == 2 * sizeof(double)) {
				// a pure double struct with only 2 doubles is passed in 2 FPRs.
				// If only one FPR is free (i.e. FPR6), then the first half of the struct is
				// in FPR6, and the second half is in the corresponding slot in memory in the
				// argument area.
				if (fprIndex <= maxFPRIndex - 2) {
					thunkMem += STDY(thunkMem, fprIndex, 0, R4, argAreaOffsetFromSP + (currStackSlotIndex * 8));
					thunkMem += STDY(thunkMem, fprIndex + 2, 0, R4, argAreaOffsetFromSP + ((currStackSlotIndex + 1) * 8));
					fprIndex += 4;
					if (i < 3) {
						gprIndex += 2;
					}
				}
				else if (fprIndex <= maxFPRIndex) {
					thunkMem += STDY(thunkMem, fprIndex, 0, R4, argAreaOffsetFromSP + (currStackSlotIndex * 8));
					fprIndex += 2;
				}
			}
			else { // This is for all other pure double structs (they will be passed in GPRs if there's space)
				if (gprIndex <= maxGPRIndex) {
					U_32 numGPRsAvailable = maxGPRIndex - gprIndex + 1;
					U_32 tempArgIndex = currStackSlotIndex;
					for (U_32 i = 1; i <= std::min(numGPRsAvailable, totalStackSlotsReq); i++) {
						thunkMem += STG(thunkMem, gprIndex, 0, R4, argAreaOffsetFromSP + (tempArgIndex * 8));
						tempArgIndex += 1;
						gprIndex++;
					}
					// increase currStackSlotIndex by the number of slots this struct takes in memory
				}
			}
			currStackSlotIndex += totalStackSlotsReq;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_DP:
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_SP_DP:
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP:
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP_SP:
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_SP:
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_DP:
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_MISC:
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_MISC:
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_OTHER:
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC:
			if (gprIndex <= maxGPRIndex) {
				U_32 numGPRsAvailable = maxGPRIndex - gprIndex + 1;
				U_32 tempArgIndex = currStackSlotIndex;
				for (U_32 i = 1; i <= std::min(numGPRsAvailable, totalStackSlotsReq); i++) {
					thunkMem += STG(thunkMem, gprIndex, 0, R4, argAreaOffsetFromSP + (tempArgIndex * 8));
					tempArgIndex += 1;
					gprIndex++;
				}
			}
			currStackSlotIndex += totalStackSlotsReq;
			break;
		default:
			Assert_VM_unreachable();
		}
	}

	// Now jump to the common dispatcher helper routine.
	thunkMem += IIHF(thunkMem, R1, ((reinterpret_cast<uint64_t>(metaData)) >> 32));
	thunkMem += IILF(thunkMem, R1, reinterpret_cast<uint64_t>(metaData));
	thunkMem += LAY(thunkMem, R2, 0, R4, argAreaOffsetFromSP); // argument pointer

	// The upcall dispatch routine's function descriptor is written into the first doubleword of this thunk's function descriptor (see the assignment of "metaData->functionPtr[0]" at the end of this function).
	// We use this descriptor to load the env pointer into R5 and the routine's entry point into R6.
	thunkMem += LMG(thunkMem, R5, R6, R5, 0);

	// Now generate the branch to the helper
	thunkMem += BASR(thunkMem, R7, R6);

	// Mandatory NOP instruction
	// With XPLINK, the function entry point address is not always passed to the called function. To allow Language Environment and other tools to find the entry point of the currently executing routine, every
	// call site, which is located by the "return address" field of the current stack frame, contains information necessary to locate the entry points of both the calling and called functions and, if required,
	// information about floating-point parameters passed. This is done by encoding information in a no-op instruction at the return point.
	// This is encoded using a 2 byte NOPR instruction, like the BCR below.
	thunkMem += BCR(thunkMem, 0, 0);

	// Handle struct return type
	if ((returnType & J9_FFI_UPCALL_SIG_TYPE_MASK) == J9_FFI_UPCALL_SIG_TYPE_STRUCT) {
		// After returning we load the return struct value into registers or memory (if applicable)
		if (returnType == J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP && returnValueSize == 2 * sizeof(float)) {
			// In this case, GPR3 has an address. We need to load the first 4 bytes at this address into FPR0 and the next 4 bytes into FPR2
			thunkMem += LE(thunkMem, FPR0, 0, R3, 0);
			thunkMem += LE(thunkMem, FPR2, 0, R3, 4);
		}
		else if (returnType == J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP && returnValueSize == 2 * sizeof(double)) {
			thunkMem += LD(thunkMem, FPR0, 0, R3, 0);
			thunkMem += LD(thunkMem, FPR2, 0, R3, 8);
		}
		else if (returnValueSize <= 8) {
			thunkMem += LG(thunkMem, R1, 0, R3, 0);
		}
		else if (returnValueSize <= 16) {
			thunkMem += LG(thunkMem, R1, 0, R3, 0);
			thunkMem += LG(thunkMem, R2, 0, R3, 8);
		}
		else if (returnValueSize <= 24) {
			thunkMem += LG(thunkMem, R1, 0, R3, 0);
			thunkMem += LG(thunkMem, R2, 0, R3, 8);
			thunkMem += LG(thunkMem, R3, 0, R3, 16);
		}
		else if (hiddenParameter) {
			thunkMem += LG(thunkMem, R1, 0, R4, argAreaOffsetFromSP);
			if (returnValueSize <= MAX_MVC_DISPLACEMENT) {
				thunkMem += generateSmallStructCopyInstructions(thunkMem, (I_64)returnValueSize, argAreaOffsetFromSP, R3, R1);
			}
			else {
				thunkMem += generateLargeStructCopyInstructions(thunkMem, (I_64)returnValueSize, argAreaOffsetFromSP, R3, R1);
			}
		}
	}

	// Upon returning copy back the structure, then restore the stack pointer.
	thunkMem += LMG(thunkMem, R6, R15, R4, offsetToRegSaveArea + thunkFrameSize); // 1808+256
	thunkMem += LAY(thunkMem, R4, 0, R4, thunkFrameSize);
	// Then branch back
	thunkMem += BC(thunkMem, 0xF, 0, R7, 2);
	// Ensure that the space allocated for instructions >= space used by generated instructions. This can be a source of very difficult bugs, so it's better to catch it early.
	Assert_VM_true(((uint64_t)thunkMem - (uint64_t)thunkMemStart) <= metaData->thunkSize);

	// Finish up before returning
	vmFuncs->doneUpcallThunkGeneration(metaData, metaData->thunkAddress);
	metaData->functionPtr[0] = reinterpret_cast<UDATA>((reinterpret_cast<uint64_t *>(metaData->upCallCommonDispatcher)));
	metaData->functionPtr[1] = reinterpret_cast<UDATA>(metaData->thunkAddress);

	return (void *)(&(metaData->functionPtr));
}

/**
 * @brief Calculate the requested argument in-stack memory address to return
 * @param nativeSig[in] A pointer to the J9UpcallNativeSignature
 * @param argListPtr[in] A pointer to the argument list prepared by the thunk
 * @param argIdx[in] The requested argument index
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
	const I_32 lastSigIdx = (I_32)(nativeSig->numSigs - 1);
	U_8 returnType = sigArray[lastSigIdx].type;
	U_32 returnValueSize = sigArray[lastSigIdx].sizeInByte;

	Assert_VM_true((argIdx >= 0) && (argIdx < lastSigIdx));

	I_32 stackSlotCount = 0;
	if (((returnType & J9_FFI_UPCALL_SIG_TYPE_MASK) == J9_FFI_UPCALL_SIG_TYPE_STRUCT) && returnValueSize > 24) {
		// In the case where the return value's type is a struct, we must account for the hidden argument.
		// All structs <= 24 will be stored in GPRs or FPRs. The rest are stored in a buffer in memory. The buffer's address is the first double
		// word in the argument area on the stack. Since this is considered a "hidden" argument, we must skip over this slot if it is present.
		stackSlotCount++;
	}

	/* Loop through the arguments. All arguments are in the caller frame, so this is simple. */
	for (I_32 i = 0; i < argIdx; i++) {
		U_32 argSizeInBytes = sigArray[i].sizeInByte;
		/* Testing this argument */
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
				stackSlotCount += ((argSizeInBytes - 1) / 8) + 1;
				break;
			default:
				Assert_VM_unreachable();
		}
	}
	return (void *)((U_8 *)argListPtr + (stackSlotCount * 8));
}

#endif /* JAVA_SPEC_VERSION >= 16 */

} /* extern "C" */
