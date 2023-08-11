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

// constants to map specific register instances.
#define R1 1
#define R2 2
#define R3 3
#define R4 4
#define R5 5
#define R14 14
#define R15 15 // holds the stack pointer to the current or previous caller frame
#define MAX_MVC_COPY_LENGTH 256 // max number of bytes copyable by the MVC instruction
#define MAX_MVC_DISPLACEMENT 4096

/*
 * Generates an instruction to store a double precision floating point value to memory.
 * Format: STDY R1,D2(X2,B2)
 * R1 - valueRegister
 * D2 - displacementInteger
 * X2 - offsetRegister
 * B2 - baseRegister
 */
static I_32
STDY(I_8 *instructionPtr, I_32 valueRegister, I_32 offsetRegister, I_32 baseRegister, I_32 displacementInteger)
{
	I_32 instructionSize = 6;
	I_32 opcode = 0xED000000;
	*((I_32 *)instructionPtr) = opcode | ((valueRegister & 0xF) << 20) | ((offsetRegister & 0xF) << 16) | ((baseRegister & 0xF) << 12) | (displacementInteger & 0xFFF);
	instructionPtr[4] = ((displacementInteger & 0xFF000) >> 12);
	instructionPtr[5] = 0x67;
	return instructionSize;
}

/*
 * Generates an branch instruction.
 * Format: BCR M1, R1
 * M1 - conditionMask
 * R1 - destinationRegister
 */
static I_32
BCR(I_8 *instructionPtr, I_16 conditionMask, I_16 destinationRegister)
{
	I_32 instructionSize = 2;
	*((I_16 *)instructionPtr) = 0x0700 | ((conditionMask & 0x000F) << 4) | (destinationRegister & 0x000F);
	return instructionSize;
}

/*
 * Generates an instruction to store a single precision floating point value to memory.
 * Format: STEY R1,D2(X2,B2)
 * R1 - valueRegister
 * D2 - displacementInteger
 * X2 - offsetRegister
 * B2 - baseRegister
 */
static I_32
STEY(I_8 *instructionPtr, I_32 valueRegister, I_32 offsetRegister, I_32 baseRegister, I_32 displacementInteger)
{
	I_32 instructionSize = 6;
	*((I_32 *)instructionPtr) = 0xED000000 | ((valueRegister & 0xF) << 20) | ((offsetRegister & 0xF) << 16) | ((baseRegister & 0xF) << 12) | (displacementInteger & 0xFFF);
	instructionPtr[4] = ((displacementInteger & 0xFF000) >> 12);
	instructionPtr[5] = 0x66;
	return instructionSize;
}

/*
 * Generates an instruction that loads an address specified by D2, X2, and B2.
 * Format: LAY R1,D2(X2,B2)
 * R1 - destinationRegister
 * D2 - displacementInteger
 * X2 - offsetRegister
 * B2 - baseRegister
 */
static I_32
LAY(I_8 *instructionPtr, I_32 destinationRegister, I_32 offsetRegister, I_32 baseRegister, I_32 displacementInteger)
{
	I_32 instructionSize = 6;
	*((I_32 *)instructionPtr) = 0xE3000000 | ((destinationRegister & 0xF) << 20) | ((offsetRegister & 0xF) << 16) | ((baseRegister & 0xF) << 12) | (displacementInteger & 0xFFF);
	instructionPtr[4] = ((displacementInteger & 0xFF000) >> 12);
	instructionPtr[5] = 0x71;
	return instructionSize;
}

/*
 * Generates an instruction that branches to the address specified by R2. R1 is loaded with current PC information before branching.
 * Format: BASR R1,R2
 * R1 - returnAddressRegister
 * R2 - destinationRegister
 */
static I_32
BASR(I_8 *instructionPtr, I_32 returnAddressRegister, I_32 destinationRegister)
{
	I_32 instructionSize = 2;
	I_16 opcode = 0x0D00;
	*((I_16 *)instructionPtr) = (opcode | (((I_16)returnAddressRegister) << 4) | (((I_16)destinationRegister)));
	return instructionSize;
}

/*
 * Generates an instruction to materialize a 32-bit integer constant into the upper-half of R1
 * Format: IIHF R1,I2
 * R1 - destinationRegister
 * I2 - integerConstant
 */
static I_32
IIHF(I_8 *instructionPtr, I_32 destinationRegister, I_32 integerConstant)
{
	I_32 instructionSize = 6;
	I_32 opcode = 0xC008;
	*((I_16 *)instructionPtr) = (opcode | (I_16)(destinationRegister << 4));
	instructionPtr += 2;
	*((I_32 *)instructionPtr) = integerConstant;
	return instructionSize;
}

/*
 * Generates an instruction to materialize a 32-bit integer constant into the lower-half R1
 * Format: IILF R1, I2
 * R1 - destinationRegister
 * I2 - integerConstant
 */
static I_32
IILF(I_8 *instructionPtr, I_32 destinationRegister, I_32 integerConstant)
{
	I_32 instructionSize = 6;
	I_32 opcode = 0xC009;
	*((I_16 *)instructionPtr) = (opcode | (I_16)(destinationRegister << 4));
	instructionPtr += 2;
	*((I_32 *)instructionPtr) = integerConstant;
	return instructionSize;
}

/*
 * Generates an instruction to load a 64-bit value into R1, from the memory destination specified by X2, B2, and D2.
 * Format: "LG R1,D2(X2,B2)"
 * R1 - destinationRegister
 * D2 - displacementInteger
 * X2 - offsetRegister
 * B2 - baseRegister
 */
static I_32
LG(I_8 *instructionPtr, I_32 destinationRegister, I_32 offsetRegister, I_32 baseRegister, I_32 displacementInteger)
{
	I_32 instructionSize = 6;
	I_32 opcode = 0xE3000000;
	*((I_32 *)instructionPtr) = (opcode | (destinationRegister << 20) | (offsetRegister << 16) | (baseRegister << 12) | (displacementInteger & 0x00000FFF));
	instructionPtr[4] = (I_8)((displacementInteger & 0x000FF000) >> 12);
	instructionPtr[5] = 0x04;
	return instructionSize;
}

/*
 * Generates an instruction to store a 64-bit value from R1, into the memory destination specified by X2, B2, and D2.
 * Format: "STG R1,D2(X2,B2)"
 * R1 - sourceRegister
 * D2 - displacementInteger
 * X2 - offsetRegister
 * B2 - baseRegister
 */
static I_32
STG(I_8 *instructionPtr, I_32 sourceRegister, I_32 offsetRegister, I_32 baseRegister, I_32 displacementInteger)
{
	I_32 instructionSize = 6;
	I_32 opcode = 0xE3000000;
	*((I_32 *)instructionPtr) = (opcode | (sourceRegister << 20) | (offsetRegister << 16) | (baseRegister << 12) | (displacementInteger & 0x00000FFF));
	instructionPtr[4] = (I_8)((displacementInteger & 0x000FF000) >> 12);
	instructionPtr[5] = 0x24;
	return instructionSize;
}

/*
 * Generates an instruction that moves 'L' bytes of data from the second operand location(specified by D2 and B2) to the first operand location (specified by D1,B1).
 * Format: D1(L,B1),D2(B2)
 * D1 - destinationDisplacementInt
 * B1 - destinationRegister
 * D2 - sourceDisplacementInt
 * B2 - sourceRegister
 * L -  length
 */
static I_32
MVC(I_8 *instructionPtr, I_32 destinationDisplacementInt, I_32 length, I_32 destinationRegister, I_32 sourceDisplacementInt, I_32 sourceRegister)
{
	I_32 instructionSize = 6;
	I_32 opcode = 0xD2000000;
	*((I_32 *)instructionPtr) = (opcode | ((length & 0xFF) << 16) | ((destinationRegister & 0xF) << 12) | (destinationDisplacementInt & 0xFFF));
	instructionPtr[4] = ((0x0F & sourceRegister) << 4) | ((sourceDisplacementInt & 0xF00) >> 8);
	instructionPtr[5] = sourceDisplacementInt & 0xFF;
	return instructionSize;
}

/*
 * Generates an instruction that creates a PC relative jump/branch if the CC is set to a specified value.
 * A 32 bit integer value specifies the number of halfword to jump from current PC to the destination address.
 * Format: BRCL M1, RI2
 * M1 - conditionMask
 * RI2 - immediateInteger
 */
static I_32
BRCL(I_8 *instructionPtr, I_32 conditionMask, I_32 immediateInteger)
{
	I_32 instructionSize = 6;
	I_32 opcode = 0xC0040000;
	*((I_32 *)instructionPtr) = (opcode | ((conditionMask & 0xF) << 20) | ((immediateInteger & 0xFFFF0000) >> 16));
	instructionPtr[4] = (immediateInteger & 0xFF00) >> 8;
	instructionPtr[5] = immediateInteger & 0xFF;
	return instructionSize;
}

/*
 * Generates an instruction to add a 32-bit integer constant to the 32-bit value in R1
 * Format: "AFI R1,I2"
 * R1 - sourceRegister
 * I2 - integerConstant
 */
static I_32
AFI(I_8 *instructionPtr, I_32 sourceRegister, I_32 immediateInteger)
{
	I_32 instructionSize = 6;
	I_32 opcode = 0xC2090000;
	*((I_32 *)instructionPtr) = (opcode | ((sourceRegister & 0x0000000F) << 20) | ((immediateInteger & 0xFFFF0000) >> 16));
	instructionPtr[4] = (I_8)((immediateInteger & 0xFF00) >> 8);
	instructionPtr[5] = (I_8)(immediateInteger & 0xFF);
	return instructionSize;

}

/*
 * Generates a sequence of instructions to copy data from a Java MemorySegment Object to a C struct.
 * The struct is smaller than or equal to 4096 bytes.
 */
static I_32
generateSmallStructCopyInstructions(I_8 *instructionPtr, I_32 structSize, I_32 offsetToParameterArea, I_32 sourceRegister, I_32 destinationRegister)
{
	I_32 totalInstructionSize = 0;

	const I_32 numLargeCopies = structSize / MAX_MVC_COPY_LENGTH;
	const I_32 residue = structSize % MAX_MVC_COPY_LENGTH;

	// Generate upto 16 MVC instructions to copy up 4096 bytes. Each MVC instruction in this sequence will copy 256 bytes.
	for(int i = 0; i < numLargeCopies; i++) {
		// MVC D1(L,B1),D2(B2)
		// The 'L' field is 0-based
		totalInstructionSize += MVC(instructionPtr + totalInstructionSize, i * MAX_MVC_COPY_LENGTH, MAX_MVC_COPY_LENGTH - 1, destinationRegister, i * MAX_MVC_COPY_LENGTH, sourceRegister);
	}
	// Generate an instruction to copy the last set of bytes (less than 256).
	if (residue != 0) {
		// MVC D1(L,B1),D2(B2)
		totalInstructionSize += MVC(instructionPtr + totalInstructionSize, numLargeCopies * MAX_MVC_COPY_LENGTH, residue - 1, destinationRegister, numLargeCopies * MAX_MVC_COPY_LENGTH, sourceRegister);
	}
	return totalInstructionSize;
}

/*
 * Generates a sequence of instructions to copy data from a Java MemorySegment Object to a C struct.
 * The struct is larger than 4096 bytes.
 */
static I_32
generateLargeStructCopyInstructions(I_8 *instructionPtr, I_32 structSize, I_32 offsetToParameterArea, I_32 sourceRegister, I_32 destinationRegister)
{
	I_32 totalInstructionSize = 0;

	const I_32 numFullSlots = structSize / MAX_MVC_COPY_LENGTH;
	const I_32 residue = structSize % MAX_MVC_COPY_LENGTH;
	const I_32 maxLargeCopies = MAX_MVC_DISPLACEMENT / MAX_MVC_COPY_LENGTH;

	const I_32 loopResidue = numFullSlots % maxLargeCopies;

	// loopInstructionsLength = 16x MVC(6) + 2x LAY (6) + 1x AFI (6)
	const I_32 loopInstructionsLength = 114; // size in bytes
	I_32 destinationDisplacementInt = -1;
	I_32 sourceDisplacementInt = -1;
	I_32 currMVCLength = -1;

	// Load loop counter into R3
	totalInstructionSize += IILF(instructionPtr + totalInstructionSize, R3, numFullSlots - loopResidue);

	// Generate a loop that will copy 4096 bytes at a time (i.e. 16 MVC instructions). The loop concludes when there
	// are less than 4096 bytes left to copy.
	for (int i = 0; i < maxLargeCopies; i++) {
		// MVC D1(L,B1),D2(B2)
		// The 'L' field is 0-based
		destinationDisplacementInt = i * MAX_MVC_COPY_LENGTH;
		currMVCLength = MAX_MVC_COPY_LENGTH - 1;
		sourceDisplacementInt = i * MAX_MVC_COPY_LENGTH;
		totalInstructionSize += MVC(instructionPtr + totalInstructionSize, destinationDisplacementInt, currMVCLength, destinationRegister, sourceDisplacementInt, sourceRegister);
	}
	totalInstructionSize += LAY(instructionPtr + totalInstructionSize, destinationRegister, 0, destinationRegister, MAX_MVC_DISPLACEMENT);
	totalInstructionSize += LAY(instructionPtr + totalInstructionSize, sourceRegister, 0, sourceRegister, MAX_MVC_DISPLACEMENT);
	totalInstructionSize += AFI(instructionPtr + totalInstructionSize, R3, -maxLargeCopies);

	const I_32 conditionMask = 7;
	totalInstructionSize += BRCL(instructionPtr + totalInstructionSize, conditionMask, -(loopInstructionsLength/2));

	// If numberOfBytesLeftToCopy > 256 && numberOfBytesLeftToCopy < 4096, then we generate the appropriate number of MVC instructions to copy them sequentially below.
	if (loopResidue > 0) {
		for (I_32 i = 0; i < loopResidue; i++) {
			// MVC D1(L,B1),D2(B2)
			destinationDisplacementInt = i * MAX_MVC_COPY_LENGTH;
			currMVCLength = MAX_MVC_COPY_LENGTH - 1;
			sourceDisplacementInt = i * MAX_MVC_COPY_LENGTH;
			totalInstructionSize += MVC(instructionPtr + totalInstructionSize, destinationDisplacementInt, currMVCLength, destinationRegister, sourceDisplacementInt, sourceRegister);
		}
	}

	// If numberOfBytesLeftToCopy < 256 then we generate an instruction here to copy those bytes over.
	if (residue != 0) {
		// MVC D1(L,B1),D2(B2)
		destinationDisplacementInt = loopResidue > 0 ? loopResidue * MAX_MVC_COPY_LENGTH : (numFullSlots - maxLargeCopies) * MAX_MVC_COPY_LENGTH;
		currMVCLength = residue - 1;
		sourceDisplacementInt = loopResidue > 0 ? loopResidue * MAX_MVC_COPY_LENGTH : (numFullSlots - maxLargeCopies) * MAX_MVC_COPY_LENGTH;
		totalInstructionSize += MVC(instructionPtr + totalInstructionSize, destinationDisplacementInt, currMVCLength, destinationRegister, sourceDisplacementInt, sourceRegister);
	}
	return totalInstructionSize;
}

/**
 * @brief Generate the appropriate thunk/adaptor for a given J9UpcallMetaData
 * @param metaData[in/out] A pointer to the given J9UpcallMetaData
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
 */
void *
createUpcallThunk(J9UpcallMetaData *metaData)
{
	J9JavaVM *vm = metaData->vm;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	J9UpcallSigType *sigArray = metaData->nativeFuncSignature->sigArray;
	I_32 lastSigIdx = metaData->nativeFuncSignature->numSigs - 1; // The index of the return type in the signature array

	Assert_VM_true(lastSigIdx >= 0);

	// Before allocating the thunk memory we need to:
	// - Calculate the amount of instructions (instructionCount) needed to generate a call to the common dispatcher for the Java method.
	// - Calculate the amount of instructions (instructionCount) we will need to transfer parameters from registers + C stack to our stack
	// - Calculate the amount of additional space (if any) we will need to store the parameters + instructions to store the parameter
	// - Calculate the amount of instructions we will need to unpack Java's MemorySegment object into a struct before returning to the C native code.
	// - Calculate the amount of instructions we will need to build a stack frame (if it's needed based on the above additional space requirements).
	//
	// - Once we know how much space is needed, we can allocate the thunk and generate the necessary instructions to save parameters. Then we can call
	// the dispatcher. If the return type is a struct, then we'll need to also generate instructions to unpack the Java return value into the format required
	// by the C side.

	I_32 numInstructions = 0;

	bool hiddenParameter = false;
	I_32 numStackSlots = 0;

	// Test the return type
	switch (sigArray[lastSigIdx].type & J9_FFI_UPCALL_SIG_TYPE_MASK) {
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
			// All struct returns are handled by populating a buffer provided by the caller.
			// This buffer is referenced by an address known as the "hidden parameter" that can
			// be found in the caller's frame
			hiddenParameter = true;
			// extra instruction used to store hidden parameter in callee frame
			numInstructions++;
			// add an extra slot to callee's frame to store the hidden parameter
			numStackSlots += 1;
			// We will use MVC to copy over the struct bytes. The structSize / MAX_MVC_COPY_LENGTH tells us how many MVC instructions we need
			// in a loop (copying 256 bytes with each MVC instruction). Then we add 1 to account for any residual bytes and 1 to materialize
			// source address.
			if (sigArray[lastSigIdx].sizeInByte <= MAX_MVC_DISPLACEMENT)
				numInstructions += 2 + sigArray[lastSigIdx].sizeInByte / MAX_MVC_COPY_LENGTH;
			else {
				// Similar to the above case but we need 6 instructions to maintain loop counters and handle residue.
				numInstructions += 6 + sigArray[lastSigIdx].sizeInByte / MAX_MVC_COPY_LENGTH + 1;
			}
			break;
		default:
			Assert_VM_unreachable();
	}

	I_32 gprArgCount = 0;
	I_32 fprArgCount = 0;
	// We have 5 GPRs (R2 to R6) and 4 FPRs (F0, F2, F4, F6) available for parameter passing.
	const I_32 maxGPRArgCount = 5;
	const I_32 maxFPRArgCount = 4;

	// Loop through the arguments and count how many instructions you'll need to copy data from registers to memory, and how many
	// bytes of space you'll need to allocate in thunk and new stack frame to store the instructions + parameter info.
	for (I_32 i = 0; i < lastSigIdx; i++) {
		I_32 argSizeInBytes = sigArray[i].sizeInByte;
		// Testing this argument
		switch (sigArray[i].type) {
			case J9_FFI_UPCALL_SIG_TYPE_CHAR:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_SHORT:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT32:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_POINTER: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT64:
				if (gprArgCount < maxGPRArgCount) {
					numStackSlots++;
					numInstructions++;
				}
				gprArgCount++;
				break;
			case J9_FFI_UPCALL_SIG_TYPE_FLOAT:
			case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
				if (fprArgCount < maxFPRArgCount) {
					numStackSlots++;
					numInstructions++;
				}
				fprArgCount++;
				break;
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP:
				// There are three possible scenarios we need to handle for pure float structures:
				// 1. The struct consists of either
				// 	a. A single float element. This struct is said to have "Type T".
				// 	b. A child struct of "Type T"
				// 2. The struct consists of exactly two float elements or elements of "Type T"
				// 3. The struct consists of more than two float elements of elements of "Type T"
				//
				// Below is how we will handle each of the above scenarios:
				// 1. Treat this case as any other primitive float argument --> data is either in fpr or in memory
				// 2. The struct will be in a GR (if available) or in memory. i.e. We treat this case like we are dealing with an 8-byte primitive.
				// 3. In this case the struct or a copy of it will be somewhere in memory, and a pointer to it will be found in GR (if available) or in memory.
				if (argSizeInBytes == sizeof(float)) { // struct consists of a single float element
					if (fprArgCount < maxFPRArgCount) {
						numStackSlots++;
						numInstructions++;
					}
					fprArgCount++;
				}
				else {
					// In all other cases floats are passed by value in GPR or by pointer in GPR.
					// So we can handle them identically.
					if (gprArgCount < maxGPRArgCount) {
						numStackSlots++;
						numInstructions++;
					}
					gprArgCount++;
				}
				break;
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP:
				// If we have a structure of all doubles, we must handle the following two scenarios:
				// 1. A structure with a single element of type double or a structure with a single element that is also a structure of type double (or another sub-child of the structure).
				// 2. A structure with more than one element of type double.
				//
				// For scenario 1, the value is passed via FPR like a normal double type would.
				// For scenario 2, the value is passed via pointer to the structure or a copy of the structure.
				if (argSizeInBytes == sizeof(double)) {
					if (fprArgCount < maxFPRArgCount) {
						numStackSlots++;
						numInstructions++;
					}
					fprArgCount++;
				}
				else {
					if (gprArgCount < maxGPRArgCount) {
						numStackSlots++;
						numInstructions++;
					}
					gprArgCount++;
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
				// We should be able to capture all the other structs here (i.e. any structs that are not pure float/doubles)..
				// Structs that are 1,2,4 or 8 bytes in size are passed as a single 8-byte value in a GR or in the caller frame parameter area.
				// All other structs are passed via pointer in a GR. So these cases can be handled in a common way (using an STG instruction).
				if (gprArgCount < maxGPRArgCount) {
					numStackSlots++;
					numInstructions++;
				}
				gprArgCount++;
				break;
			default:
				Assert_VM_unreachable();
		}
	}


	I_32 frameSize = 0;
	// The first 160 bytes of a calling function's stack frame, are referred to as the register save area. Following this
	// is an area called the Parameter Area. Any arguments that can't be passed by register to the callee will be placed in
	// this Parameter Area. So we use this offset to find those arguments if needed.
	const I_32 offsetToParamArea = 160;
	I_8 *thunkMem = NULL;

	// Stack space to store arguments or return values
	frameSize = offsetToParamArea + numStackSlots * 8;

	// 8 byte align the thunk memory.
	// Fixed amount of instructions needed for backing up and restoring data before/after we invoke the common dispatcher
	const I_32 extraInstructionsSize = 64;
	metaData->thunkSize = ((( numInstructions * 6 + extraInstructionsSize) / 8) + 1) * 8 + 8;
	thunkMem = (I_8 *)vmFuncs->allocateUpcallThunkMemory(metaData);
	if (NULL == thunkMem) {
		return NULL;
	}
	metaData->thunkAddress = (void *)thunkMem;

	// Generate the instruction sequence to copy data from C side to Java side.
	I_32 stackSlotIndex = 0;
	I_32 gprIndex = 2;
	I_32 maxGPRIndex = 6;
	I_32 fprIndex = 0;
	I_32 maxFPRIndex = 6;

	// The first 160 bytes of a function's stack frame, are referred to as the register save area. If we would like to preserve
	// certain registers across function calls, then we can use this area to save information. Offset of 114 from the start of the
	// stack is the save area for R14
	const I_32 offsetToR14Slot = 112;

	// First extend the stack frame
	// Save the original return address into memory
	thunkMem += STG(thunkMem, R14, 0, R15, offsetToR14Slot);
	// Store the current C stack pointer at the first byte of the new frame.
	thunkMem += STG(thunkMem, R15, 0, R15, -frameSize);
	// Then update R15 with the new C stack pointer.
	// Now do an add to update the stack pointer in R15.
	thunkMem += LAY(thunkMem, R15, 0, R15, -frameSize);

	if (hiddenParameter) {
		// if there's a hidden parameter, then store it.
		thunkMem += STG(thunkMem, gprIndex, 0, R15, offsetToParamArea + (stackSlotIndex * 8));
		gprIndex++;
		stackSlotIndex += 1;
	}

	// Now that the stack has expanded and the new stack pointer is in, we loop through the arguments and generate the actual instructions
	// We only need to store the argument in the calleeFrame if it's in register. If it's in the callerFrame, then we will find it when
	// getArgPointer is invoked.
	for (I_32 i = 0; i < lastSigIdx; i++) {
		I_32 argSizeInBytes = sigArray[i].sizeInByte;
		switch(sigArray[i].type) {
			case J9_FFI_UPCALL_SIG_TYPE_CHAR:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_SHORT:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT32:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_POINTER: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT64:
				if (gprIndex <= maxGPRIndex) {
					thunkMem += STG(thunkMem, gprIndex, 0, R15, offsetToParamArea + (stackSlotIndex * 8));
					stackSlotIndex += 1;
				}
				gprIndex += 1;
				break;
			case J9_FFI_UPCALL_SIG_TYPE_FLOAT:
				if (fprIndex <= maxFPRIndex) {
					thunkMem += STEY(thunkMem, fprIndex, 0, R15, offsetToParamArea + (stackSlotIndex * 8));
					stackSlotIndex += 1;
				}
				fprIndex += 2;
				break;
			case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
				if (fprIndex <= maxFPRIndex) {
					thunkMem += STDY(thunkMem, fprIndex, 0, R15, offsetToParamArea + (stackSlotIndex * 8));
					stackSlotIndex += 1;
				}
				fprIndex += 2;
				break;
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP:
				if (argSizeInBytes == sizeof(float)) {
					// Pure float structs with a single float element are passed in FPR like any other regular float argument.
					if (fprIndex <= maxFPRIndex) {
						thunkMem += STEY(thunkMem, fprIndex, 0, R15, offsetToParamArea + (stackSlotIndex * 8));
						stackSlotIndex += 1;
					}
					fprIndex += 2;
				}
				else {
					// For all other pure float structs, the struct is passed in GPR by value or pointer. So we handle them similarly.
					if (gprIndex <= maxGPRIndex) {
						thunkMem += STG(thunkMem, gprIndex, 0, R15, offsetToParamArea + (stackSlotIndex * 8));
						stackSlotIndex += 1;
					}
					gprIndex += 1;
				}
				break;
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP:
				if (argSizeInBytes == sizeof(double)) {
					// Similar to the float case, pure double structs with a single element are passed in FPR like any other
					// regular double argument.
					if (fprIndex <= maxFPRIndex) {
						thunkMem += STDY(thunkMem, fprIndex, 0, R15, offsetToParamArea + (stackSlotIndex * 8));
						stackSlotIndex += 1;
					}
					fprIndex += 2;
				}
				else {
					// For all other cases, the struct is passed via pointer in a GPR.
					if (gprIndex <= maxGPRIndex) {
						thunkMem += STG(thunkMem, gprIndex, 0, R15, offsetToParamArea + (stackSlotIndex * 8));
						stackSlotIndex += 1;
					}
					gprIndex += 1;
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
				// There are two possible scenarios here. Either the struct is passed by value in a GPR (if struct size is 1,2,4,8),
				// or by a pointer in GPR. In either case, we just need to store whatever is in the GPR into memory.
				// So we do that here.
				if (gprIndex <= maxGPRIndex) {
					thunkMem += STG(thunkMem, gprIndex, 0, R15, offsetToParamArea + (stackSlotIndex * 8));
					stackSlotIndex += 1;
				}
				gprIndex += 1;
				break;
			default:
				Assert_VM_unreachable();
		}
	}

	// Now jump to the common dispatcher helper routine.
	// For the first parameter materialize the metaData pointer into R2.
	// For the second parameter, load C_SP + 160 into R3 to point to where the current list of parameters start.
	thunkMem += IIHF(thunkMem, R2, (reinterpret_cast<uint64_t>(metaData) >> 32));
	thunkMem += IILF(thunkMem, R2, reinterpret_cast<uint64_t>(metaData));
	// Load the R3 with argPointer (stackFrame + 160)
	thunkMem += LAY(thunkMem, R3, 0, R15, offsetToParamArea);
	// Materialize the destination address in R1.
	thunkMem += IIHF(thunkMem, R1, reinterpret_cast<uint64_t>(metaData->upCallCommonDispatcher) >> 32);
	thunkMem += IILF(thunkMem, R1, reinterpret_cast<uint64_t>(metaData->upCallCommonDispatcher));
	// Now generate the branch to the helper
	thunkMem += BASR(thunkMem, R14, R1);

	// Upon returning copy back the structure, then restore the stack pointer.
	if (hiddenParameter) {
		// If there's a hidden parameter, then we need to copy the result back into the buffer provided by the native caller.
		// The max displacement value possible by an MVC instruction is 4096 bytes. So if the struct size is smaller than 4096,
		// we can copy all of it in a straight sequence of MVC instructions. If it's larger, we fall through into the `else` block
		// below and copy the data in a loop.
		thunkMem += LG(thunkMem, R4, 0, R15, offsetToParamArea);
		if (sigArray[lastSigIdx].sizeInByte <= MAX_MVC_DISPLACEMENT)
			thunkMem += generateSmallStructCopyInstructions(thunkMem, (I_64)sigArray[lastSigIdx].sizeInByte, offsetToParamArea, R2, R4);
		else
			thunkMem += generateLargeStructCopyInstructions(thunkMem, (I_64)sigArray[lastSigIdx].sizeInByte, offsetToParamArea, R2, R4);
	}
	thunkMem += LAY(thunkMem, R15, 0, R15, frameSize);
	// Now load R14 with the return address.
	thunkMem += LG(thunkMem, R14, 0, R15, offsetToR14Slot);
	// Then branch back
	thunkMem += BCR(thunkMem, 0xF, R14);

	// Finish up before returning
	vmFuncs->doneUpcallThunkGeneration(metaData, (void *)(metaData->thunkAddress));
	return (void *)(metaData->thunkAddress);
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
	I_32 lastSigIdx = nativeSig->numSigs - 1; // The index for the return type in the signature array

	// Make sure argIdx >= 0
	Assert_VM_true((argIdx >= 0) && (argIdx < lastSigIdx));

	// Test the return type
	I_32 argSize = sigArray[argIdx].sizeInByte;

	I_32 gprIndex = 1;
	I_32 fprIndex = -2;
	I_32 callerFrameIndex = -1; // values that weren't passed by register
	I_32 calleeFrameIndex = -1; // values that were passed through register
	bool argumentInCallerFrame = false; // At first we assume the argument hasn't overflown and is in the new frame allocated by the callee
	I_32 paramAreaOffset = 160; // offset from the start of a stack frame to start of parameter area holding overflow arguments

	if ((sigArray[lastSigIdx].type & J9_FFI_UPCALL_SIG_TYPE_MASK) == J9_FFI_UPCALL_SIG_TYPE_STRUCT) {
		// If the return type is a struct, then the first entry in the calleeFrame is an address to a buffer
		// where the struct must be stored when we return to native caller. So we must skip over that
		// "hiddenParameter" when trying to identify arguments in getArgPointer.
		calleeFrameIndex++;
		// When the return type is a struct, the buffer where the struct must be populated is initially
		// provided in GPR2 by the caller. So we must increment the GPR index as well in order to correctly
		// find the position of the current argument pointer.
		gprIndex++;
	}

	I_32 structOffset = 0;
	bool isPointerToStruct = false;
	// These booleans determine if the argument is a struct with a single float or double element.
	bool isPureFloatSingleElementStruct = (sigArray[argIdx].type == J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP) && argSize == sizeof(float);
	bool isPureDoubleSingleElementStruct = (sigArray[argIdx].type == J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP) && argSize == sizeof(double);
	// If the argument is a struct, set certain flags/offsets for later calculation.
	switch(sigArray[argIdx].type & J9_FFI_UPCALL_SIG_TYPE_MASK) {
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT:
			// structs not of size = 1,2,4,8 are passed by pointers. These must be dereferenced before
			// returning. So we set a flag here to do so.
			if (argSize != 1 && argSize != 2 && argSize != 4 && argSize != 8) {
				isPointerToStruct = true;
			}
			else if (!isPureFloatSingleElementStruct && !isPureDoubleSingleElementStruct) {
				// structs that are passed by value are expected to be stored as right aligned in the 8 byte
				// stack slot. However the caller of getArgPointer expects the return address to struct to point to the start of the struct.
				// So for structs smaller than 8 that are passed by value we must increment the offset by the required amount
				// to adjust the final return address.
				// Note: structs with a single double or single float element in them are an exception to the above rule.
				// These arguments are stored left aligned on the stack, so no additional calibration needs to be done on the stack slot.
				//
				structOffset = 8 - argSize;
			}
			break;
		default:
			break;
	}

	// We have R2 to R6 available for parameter passing by registers for GPR arguments. So the max index is 6.
	const I_32 maxGPRIndex = 6;
	// We have F0, F2, F6 available for floating point parameter passing.
	const I_32 maxFPRIndex = 6;
	// Loop through the arguments upto and including i = argIdx and set properties accordingly.
	for (I_32 i = 0; i <= argIdx; i++) {
		switch (sigArray[i].type) {
			case J9_FFI_UPCALL_SIG_TYPE_CHAR:
			case J9_FFI_UPCALL_SIG_TYPE_SHORT:
			case J9_FFI_UPCALL_SIG_TYPE_INT32:
			case J9_FFI_UPCALL_SIG_TYPE_POINTER:
			case J9_FFI_UPCALL_SIG_TYPE_INT64:
				// These arguments are in GPRs
				gprIndex++;
				if (gprIndex > maxGPRIndex) {
					// This argument should be in the caller frame (i.e. overflow argument) so increase the callerFrameIndex.
					callerFrameIndex++;
					argumentInCallerFrame = true;
				}
				else {
					// These arguments are in the callee frame (not overflow argument), so increase the calleeFrameIndex.
					calleeFrameIndex++;
					argumentInCallerFrame = false;
				}
				break;
			case J9_FFI_UPCALL_SIG_TYPE_FLOAT:
			case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
				// These arguments are either in FPRs or in the caller frame
				fprIndex += 2;
				// Overflow argument, so in the caller frame
				if (fprIndex > maxFPRIndex) {
					callerFrameIndex++;
					argumentInCallerFrame = true;
				}
				else {
					calleeFrameIndex++;
					argumentInCallerFrame = false;
				}
				break;
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP:
				if (sigArray[i].sizeInByte == sizeof(float)) {
					// If the size is 4 bytes, then it means this is a struct with a single
					// float element. These arguments are either passed via FPR or in the caller frame depending on if we have overflow arguments.
					fprIndex += 2;
					if (fprIndex > maxFPRIndex) { // overflow argument
						callerFrameIndex++;
						argumentInCallerFrame = true;
					}
					else {
						calleeFrameIndex++;
						argumentInCallerFrame = false;
					}
				}
				else {
					// The other case is if the size is 8 bytes (2 elements) or larger of a pure float struct. These are all passed via value or pointer in a GPR.
					gprIndex++;
					if (gprIndex > maxGPRIndex) { // overflow argument
						callerFrameIndex++;
						argumentInCallerFrame = true;
					}
					else {
						calleeFrameIndex++;
						argumentInCallerFrame = false;
					}
				}
				break;
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP:
				if (sigArray[i].sizeInByte == sizeof(double)) {
					// If the size is 8 bytes, then it means this is a struct with a single
					// double element. These arguments are either passed via FPR or in the caller frame depending on if we have overflow arguments.
					fprIndex += 2;
					if (fprIndex > maxFPRIndex) { // overflow argument
						callerFrameIndex++;
						argumentInCallerFrame = true;
					}
					else {
						calleeFrameIndex++;
						argumentInCallerFrame = false;
					}
				}
				else {
					// In all other cases the struct is of size > 8. These structs are passed via pointer
					// in a GPR
					gprIndex++;
					if (gprIndex > maxGPRIndex) { // overflow argument
						callerFrameIndex++;
						argumentInCallerFrame = true;
					}
					else {
						calleeFrameIndex++;
						argumentInCallerFrame = false;
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
				// For any other struct except for pure floats and doubles, they are passed via GPRs either by value or by pointer.
				gprIndex++;
				if (gprIndex > maxGPRIndex) { // overflow argument
					callerFrameIndex++;
					argumentInCallerFrame = true;
				}
				else {
					calleeFrameIndex++;
					argumentInCallerFrame = false;
				}
				break;
			default:
				Assert_VM_unreachable();
		}
	}

	void *returnAddress = NULL;
	if (argumentInCallerFrame) {
		// The native side right aligns float values in an 8-byte slot when passing them on the stack (i.e. overflow argument). However the caller of getArgPointer
		// expects floats to be left aligned in 8-byte slot on BE platforms. So we compensate for that discrepancy here.
		I_32 floatOffset = (((sigArray[argIdx].type & J9_FFI_UPCALL_SIG_TYPE_MASK) == J9_FFI_UPCALL_SIG_TYPE_FLOAT) || isPureFloatSingleElementStruct) ? 4 : 0;

		// (argListPtr - paramAreaOffset) corresponds to the start of the stack frame, which holds the stack frame address of the previous frame
		uintptr_t callerFramePointer = reinterpret_cast<uintptr_t>(argListPtr) - paramAreaOffset;
		uintptr_t callerFrame = reinterpret_cast<uintptr_t>(*(reinterpret_cast<uint64_t *>(callerFramePointer)));

		returnAddress = reinterpret_cast<void *>(callerFrame + callerFrameIndex * 8 + paramAreaOffset + floatOffset + structOffset);
	}
	else {
		returnAddress = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(argListPtr) + calleeFrameIndex * 8 + structOffset);
	}

	if (isPointerToStruct) {
		returnAddress = reinterpret_cast<void *>(*(uint64_t *)returnAddress);
	}

	return returnAddress;
}

#endif /* JAVA_SPEC_VERSION >= 16 */

} /* extern "C" */
