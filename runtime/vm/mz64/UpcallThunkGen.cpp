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
#define R4 4 // R4 = CallerSP
#define R6 6 // entry point
#define R7 7 // return address
#define R15 15

static I_32
BC(I_8 *instructionPtr, I_32 mask1, I_32 offsetRegister, I_32 destinationRegister, I_32 displacementInteger)
{
   I_32 instructionSize = 4;
   I_32 opcode = 0x47000000;
   *((I_32 *)instructionPtr) = (opcode | ((mask1 & 0xF) << 20) | ((offsetRegister & 0xF) << 16) | ((destinationRegister & 0xF) << 12) | (displacementInteger & 0xFFF));
   return instructionSize;
}

static I_32
STMG(I_8 *instructionPtr, I_32 startRegister, I_32 endRegister, I_32 baseRegister, I_32 displacementInteger)
{
    I_32 instructionSize = 6;
    I_32 opcode = 0xEB000000;
    *((I_32 *)instructionPtr) = opcode | ((startRegister & 0xF) << 20) | ((endRegister & 0xF) << 16) | ((baseRegister & 0xF) << 12) | (displacementInteger & 0xFFF);
    instructionPtr[4] = ((displacementInteger & 0xFF000) >> 12);
    instructionPtr[5] = 0x24;
    return instructionSize;
}

static I_32
LMG(I_8 *instructionPtr, I_32 startRegister, I_32 endRegister, I_32 baseRegister, I_32 displacementInteger)
{
    I_32 instructionSize = 6;
    I_32 opcode = 0xEB000000;
    *((I_32 *)instructionPtr) = opcode | ((startRegister & 0xF) << 20) | ((endRegister & 0xF) << 16) | ((baseRegister & 0xF) << 12) | (displacementInteger & 0xFFF);
    instructionPtr[4] = ((displacementInteger & 0xFF000) >> 12);
    instructionPtr[5] = 0x04;
    return instructionSize;
}

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
    I_32 lastSigIdx = (I_32)(metaData->nativeFuncSignature->numSigs - 1);

    Assert_VM_true(lastSigIdx >= 0);

    I_32 numInstructions = 0;
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
        I_32 argSizeInBytes = sigArray[i].sizeInByte;
        switch (sigArray[i].type) {
            case J9_FFI_UPCALL_SIG_TYPE_CHAR:    /* Fall through */
            case J9_FFI_UPCALL_SIG_TYPE_SHORT:   /* Fall through */
            case J9_FFI_UPCALL_SIG_TYPE_INT32:   /* Fall through */
            case J9_FFI_UPCALL_SIG_TYPE_POINTER: /* Fall through */
            case J9_FFI_UPCALL_SIG_TYPE_INT64:
                if (gprArgCount < maxGPRArgCount) {
                    numInstructions++;
                }
                gprArgCount++;
                break;
            case J9_FFI_UPCALL_SIG_TYPE_FLOAT:
            case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
                if (fprArgCount < maxFPRArgCount) {
                    numInstructions++;
                }
                fprArgCount++;
                break;
            default:
                Assert_VM_unreachable();
        }
        if (fprArgCount + gprArgCount >= 7)
            break;

    }

    // The thunk will frame will be 256 bytes in size. Since parameters will be stored in the caller frame, offsetFromR4
    // must account for the frame size of the thunk.
    const I_32 defaultStackPointerBias = 2048;
    const I_32 offsetToParamArea = 128;
    const I_32 calleeFrameSize = 256;
    const I_32 offsetFromR4 = defaultStackPointerBias + offsetToParamArea + calleeFrameSize;
    I_8 *thunkMem = NULL;

    // Size of the instructions that are always present in the thunk, 8 byte-aligned.
    const I_32 extraInstructionsSize = 80;
    // Size of thunk including the numInstructions generated to store all arguments in memory, 8-byte aligned.
    metaData->thunkSize = ((( numInstructions * 6 + extraInstructionsSize) / 8) + 1) * 8;
    thunkMem = (I_8 *)vmFuncs->allocateUpcallThunkMemory(metaData);
    if (NULL == thunkMem) {
        return NULL;
    }
    metaData->thunkAddress = (void *)thunkMem;

    // Generate the instruction sequence to copy data from C side to Java side.
    I_32 gprIndex = 1;
    I_32 maxGPRIndex = 3;
    I_32 fprIndex = 0;
    I_32 maxFPRIndex = 6;
    I_32 currArgIndex = 0;

    // Now we loop through the arguments and generate the actual instructions to store any register arguments in the caller frame.
    thunkMem += STMG(thunkMem, R6, R15, R4, 1808);
    thunkMem += LAY(thunkMem, R4, 0, R4, -256);
    for (I_32 i = 0; i < lastSigIdx; i++) {
        switch(sigArray[i].type) {
            case J9_FFI_UPCALL_SIG_TYPE_CHAR:    /* Fall through */
            case J9_FFI_UPCALL_SIG_TYPE_SHORT:   /* Fall through */
            case J9_FFI_UPCALL_SIG_TYPE_INT32:   /* Fall through */
            case J9_FFI_UPCALL_SIG_TYPE_POINTER: /* Fall through */
            case J9_FFI_UPCALL_SIG_TYPE_INT64:
                if (gprIndex <= maxGPRIndex) {
                    thunkMem += STG(thunkMem, gprIndex, 0, R4, offsetFromR4 + (currArgIndex * 8));
                    currArgIndex += 1;
                }
                gprIndex += 1;
                break;
            case J9_FFI_UPCALL_SIG_TYPE_FLOAT:
                if (fprIndex <= maxFPRIndex) {
                    thunkMem += STEY(thunkMem, fprIndex, 0, R4, offsetFromR4 + (currArgIndex * 8));
                    currArgIndex += 1;
                }
                fprIndex += 2;
		        if (i < 3) {
		            gprIndex++; // If one of the first three doublewords of the argument area is float/double, then the corresponding GPR is undefined. So we must ignore any value in there.
		        }
                break;
            case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
                if (fprIndex <= maxFPRIndex) {
                    thunkMem += STDY(thunkMem, fprIndex, 0, R4, offsetFromR4 + (currArgIndex * 8));
                    currArgIndex += 1;
                }
                fprIndex += 2;
		        if (i < 3) {
		            gprIndex++; // If one of the first three doublewords of the argument area is float/double, then the corresponding GPR is undefined. So we must ignore any value in there.
		        }
                break;
            default:
                Assert_VM_unreachable();
        }

    }

    // Now jump to the common dispatcher helper routine.
    thunkMem += IIHF(thunkMem, R1, ((reinterpret_cast<uint64_t>(metaData)) >> 32));
    thunkMem += IILF(thunkMem, R1, reinterpret_cast<uint64_t>(metaData));
    thunkMem += LAY(thunkMem, R2, 0, R4, offsetFromR4); // argument pointer
    uint64_t upcallAddress = *(reinterpret_cast<uint64_t *>(reinterpret_cast<uint64_t>(metaData->upCallCommonDispatcher) + 8));
    // Materialize the destination address in R6.
    thunkMem += IIHF(thunkMem, R6, upcallAddress >> 32);
    thunkMem += IILF(thunkMem, R6, upcallAddress);

    // Now generate the branch to the helper
    thunkMem += BASR(thunkMem, R7, R6);
    thunkMem += BCR(thunkMem, 0, 0); // Mandatory NOP instruction
    // Upon returning copy back the structure, then restore the stack pointer.
    thunkMem += LMG(thunkMem, R6, R15, R4, 2064); // 1808+256
    thunkMem += LAY(thunkMem, R4, 0, R4, 256);
    // Then branch back
    thunkMem += BC(thunkMem, 0xF, 0, R7, 2);

    // Finish up before returning
    vmFuncs->doneUpcallThunkGeneration(metaData, (void *)(metaData->thunkAddress));
    metaData->functionPtr[0] = (UDATA)(*(reinterpret_cast<uint64_t *>(metaData->upCallCommonDispatcher)));
    metaData->functionPtr[1] = (UDATA )(metaData->thunkAddress);

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
    I_32 lastSigIdx = (I_32)(nativeSig->numSigs - 1);

    Assert_VM_true((argIdx >= 0) && (argIdx < lastSigIdx));

    I_32 stackSlotCount = 0;

    /* Loop through the arguments. All arguments are in the caller frame, so this is simple. */
    for (I_32 i = 0; i < argIdx; i++) {
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
            default:
                Assert_VM_unreachable();
        }
    }
    return (void *)((char *)argListPtr + (stackSlotCount * 8));
}

#endif /* JAVA_SPEC_VERSION >= 16 */

} /* extern "C" */
