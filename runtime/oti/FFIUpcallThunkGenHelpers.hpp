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

#if !defined(FFIUPCALLTHUNKGENHELPERS_HPP_)
#define FFIUPCALLTHUNKGENHELPERS_HPP_

#define R0 0
#define R1 1
#define R2 2
#define R3 3
#define R4 4
#define R5 5
#define R6 6
#define R7 7
#define R14 14
#define R15 15
#define FPR0 0
#define FPR2 2
#define MAX_MVC_COPY_LENGTH 256
#define MAX_MVC_DISPLACEMENT 4096

I_32
XGR(I_8 *instructionPtr, I_32 register1, I_32 register2)
{
	I_32 opcode = 0xB9820000;
	*((I_32 *)instructionPtr) = opcode | ((register1 & 0xF) << 4) | ((register2 & 0xF));
	return 4;
}

/**
 * Generates an instruction to load a double precision floating point value into an FPR.
 * Format: "LD R1,D2(X2,B2)"
 * R1 - destinationRegister
 * D2 - displacementInteger
 * X2 - offsetRegister
 * B2 - baseRegister
 *
 * Returns: instruction size in bytes
 */
I_32
LD(I_8 *instructionPtr, I_32 destinationRegister, I_32 offsetRegister, I_32 baseRegister, I_32 displacementInteger)
{
	I_32 opcode = 0x68000000;
	*((I_32 *)instructionPtr) = opcode | ((destinationRegister & 0xF) << 20) | ((offsetRegister & 0xF) << 16) | ((baseRegister & 0xF) << 12) | ((displacementInteger & 0xFFF));
	return 4;
}

/**
 * Generates an instruction to load a single precision floating point value into an FPR.
 * Format: "LE R1,D2(X2,B2)"
 * R1 - destinationRegister
 * D2 - displacementInteger
 * X2 - offsetRegister
 * B2 - baseRegister
 *
 * Returns: instruction size in bytes
 */
I_32
LE(I_8 *instructionPtr, I_32 destinationRegister, I_32 offsetRegister, I_32 baseRegister, I_32 displacementInteger)
{
	I_32 opcode = 0x78000000;
	*((I_32 *)instructionPtr) = opcode | ((destinationRegister & 0xF) << 20) | ((offsetRegister & 0xF) << 16) | ((baseRegister & 0xF) << 12) | ((displacementInteger & 0xFFF));
	return 4;
}

/**
 * Generates an instruction to store a double precision floating point value to memory.
 * Format: "STDY R1,D2(X2,B2)"
 * R1 - valueRegister
 * D2 - displacementInteger
 * X2 - offsetRegister
 * B2 - baseRegister
 *
 * Returns: instruction size in bytes
 */
I_32
STDY(I_8 *instructionPtr, I_32 valueRegister, I_32 offsetRegister, I_32 baseRegister, I_32 displacementInteger)
{
	I_32 opcode = 0xED000000;
	*((I_32 *)instructionPtr) = opcode | ((valueRegister & 0xF) << 20) | ((offsetRegister & 0xF) << 16) | ((baseRegister & 0xF) << 12) | (displacementInteger & 0xFFF);
	instructionPtr[4] = ((displacementInteger & 0xFF000) >> 12);
	instructionPtr[5] = 0x67;
	return 6;
}

/**
 * Generates an branch instruction.
 * Format: "BCR M1, R1"
 * M1 - conditionMask
 * R1 - destinationRegister
 *
 * Returns: instruction size in bytes
 */
I_32
BCR(I_8 *instructionPtr, I_16 conditionMask, I_16 destinationRegister)
{
	*((I_16 *)instructionPtr) = 0x0700 | ((conditionMask & 0x000F) << 4) | (destinationRegister & 0x000F);
	return 2;
}

/**
 * Generates an instruction to store a single precision floating point value to memory.
 * Format: "STEY R1,D2(X2,B2)"
 * R1 - valueRegister
 * D2 - displacementInteger
 * X2 - offsetRegister
 * B2 - baseRegister
 *
 * Returns: instruction size in bytes
 */
I_32
STEY(I_8 *instructionPtr, I_32 valueRegister, I_32 offsetRegister, I_32 baseRegister, I_32 displacementInteger)
{
	*((I_32 *)instructionPtr) = 0xED000000 | ((valueRegister & 0xF) << 20) | ((offsetRegister & 0xF) << 16) | ((baseRegister & 0xF) << 12) | (displacementInteger & 0xFFF);
	instructionPtr[4] = ((displacementInteger & 0xFF000) >> 12);
	instructionPtr[5] = 0x66;
	return 6;
}

/**
 * Generates an instruction that loads an address specified by D2, X2, and B2.
 * Format: "LAY R1,D2(X2,B2)"
 * R1 - destinationRegister
 * D2 - displacementInteger
 * X2 - offsetRegister
 * B2 - baseRegister
 *
 * Returns: instruction size in bytes
 */
I_32
LAY(I_8 *instructionPtr, I_32 destinationRegister, I_32 offsetRegister, I_32 baseRegister, I_32 displacementInteger)
{
	*((I_32 *)instructionPtr) = 0xE3000000 | ((destinationRegister & 0xF) << 20) | ((offsetRegister & 0xF) << 16) | ((baseRegister & 0xF) << 12) | (displacementInteger & 0xFFF);
	instructionPtr[4] = ((displacementInteger & 0xFF000) >> 12);
	instructionPtr[5] = 0x71;
	return 6;
}

/**
 * Generates an instruction that branches to the address specified by R2. R1 is loaded with current PC information before branching.
 * Format: "BASR R1,R2"
 * R1 - returnAddressRegister
 * R2 - destinationRegister
 *
 * Returns: instruction size in bytes
 */
I_32
BASR(I_8 *instructionPtr, I_32 returnAddressRegister, I_32 destinationRegister)
{
	I_16 opcode = 0x0D00;
	*((I_16 *)instructionPtr) = (opcode | (((I_16)returnAddressRegister) << 4) | (((I_16)destinationRegister)));
	return 2;
}

/**
 * Generates an instruction to materialize a 32-bit integer constant into the upper-half of R1
 * Format: "IIHF R1,I2"
 * R1 - destinationRegister
 * I2 - integerConstant
 *
 * Returns: instruction size in bytes
 */
I_32
IIHF(I_8 *instructionPtr, I_32 destinationRegister, I_32 integerConstant)
{
	I_32 opcode = 0xC008;
	*((I_16 *)instructionPtr) = (opcode | (I_16)(destinationRegister << 4));
	instructionPtr += 2;
	*((I_32 *)instructionPtr) = integerConstant;
	return 6;
}

/**
 * Generates an instruction to materialize a 32-bit integer constant into the lower-half R1
 * Format: "IILF R1, I2"
 * R1 - destinationRegister
 * I2 - integerConstant
 *
 * Returns: instruction size in bytes
 */
I_32
IILF(I_8 *instructionPtr, I_32 destinationRegister, I_32 integerConstant)
{
	I_32 opcode = 0xC009;
	*((I_16 *)instructionPtr) = (opcode | (I_16)(destinationRegister << 4));
	instructionPtr += 2;
	*((I_32 *)instructionPtr) = integerConstant;
	return 6;
}


/**
 * Generates an instruction to store a 64-bit value from R1, into the memory destination specified by X2, B2, and D2.
 * Format: "STG R1,D2(X2,B2)"
 * R1 - sourceRegister
 * D2 - displacementInteger
 * X2 - offsetRegister
 * B2 - baseRegister
 *
 * Returns: instruction size in bytes
 */
I_32
STG(I_8 *instructionPtr, I_32 sourceRegister, I_32 offsetRegister, I_32 baseRegister, I_32 displacementInteger)
{
	I_32 opcode = 0xE3000000;
	*((I_32 *)instructionPtr) = (opcode | (sourceRegister << 20) | (offsetRegister << 16) | (baseRegister << 12) | (displacementInteger & 0x00000FFF));
	instructionPtr[4] = (I_8)((displacementInteger & 0x000FF000) >> 12);
	instructionPtr[5] = 0x24;
	return 6;
}

/**
 * Generates an instruction that conditionally branches to the address specified by
 * the second operand. The branch is executed if the condition code is set to the value
 * specified by mask1.
 * Format: "BC M1,D2(X2,B2)"
 * M1 - mask1
 * D2 - displacementInteger
 * X2 - offsetRegister
 * B2 - baseRegister
 *
 * Returns: instruction size in bytes
 *
 */
I_32
BC(I_8 *instructionPtr, I_32 mask1, I_32 offsetRegister, I_32 destinationRegister, I_32 displacementInteger) // zos
{
   I_32 opcode = 0x47000000;
   *((I_32 *)instructionPtr) = (opcode | ((mask1 & 0xF) << 20) | ((offsetRegister & 0xF) << 16) | ((destinationRegister & 0xF) << 12) | (displacementInteger & 0xFFF));
   return 4;
}

/**
 * Generates an instruction to store values in startRegister to endRegister in storage, starting from the address
 * specified by baseRegister + displacementInteger.
 * Format: "STMG R1,R3,D2(B2)"
 * R1 - startRegister
 * R3 - endRegister
 * D2 - displacementInteger
 * B2 - baseRegister
 *
 * Returns: instruction size in bytes
 */
I_32
STMG(I_8 *instructionPtr, I_32 startRegister, I_32 endRegister, I_32 baseRegister, I_32 displacementInteger) // zos
{
	I_32 opcode = 0xEB000000;
	*((I_32 *)instructionPtr) = opcode | ((startRegister & 0xF) << 20) | ((endRegister & 0xF) << 16) | ((baseRegister & 0xF) << 12) | (displacementInteger & 0xFFF);
	instructionPtr[4] = ((displacementInteger & 0xFF000) >> 12);
	instructionPtr[5] = 0x24;
	return 6;
}

/**
 * Generates an instruction to load startRegister to endRegister with values from storage, starting from the address
 * specified by baseRegister + displacementInteger.
 * Format: "LMG R1,R3,D2(B2)"
 * R1 - startRegister
 * R3 - endRegister
 * D2 - displacementInteger
 * B2 - baseRegister
 *
 * Returns: instruction size in bytes
 */
I_32
LMG(I_8 *instructionPtr, I_32 startRegister, I_32 endRegister, I_32 baseRegister, I_32 displacementInteger) // zos
{
	I_32 opcode = 0xEB000000;
	*((I_32 *)instructionPtr) = opcode | ((startRegister & 0xF) << 20) | ((endRegister & 0xF) << 16) | ((baseRegister & 0xF) << 12) | (displacementInteger & 0xFFF);
	instructionPtr[4] = ((displacementInteger & 0xFF000) >> 12);
	instructionPtr[5] = 0x04;
	return 6;
}

/**
 * Generates an instruction to load a 64-bit value into R1, from the memory destination specified by X2, B2, and D2.
 * Format: "LG R1,D2(X2,B2)"
 * R1 - destinationRegister
 * D2 - displacementInteger
 * X2 - offsetRegister
 * B2 - baseRegister
 *
 * Returns: instruction size in bytes
 */
I_32
LG(I_8 *instructionPtr, I_32 destinationRegister, I_32 offsetRegister, I_32 baseRegister, I_32 displacementInteger)
{
	I_32 opcode = 0xE3000000;
	*((I_32 *)instructionPtr) = (opcode | (destinationRegister << 20) | (offsetRegister << 16) | (baseRegister << 12) | (displacementInteger & 0x00000FFF));
	instructionPtr[4] = (I_8)((displacementInteger & 0x000FF000) >> 12);
	instructionPtr[5] = 0x04;
	return 6;
}

/*
 * Generates an instruction that moves 'L' bytes of data from the second operand location(specified by D2 and B2) to the first operand location (specified by D1,B1).
 * Format: D1(L,B1),D2(B2)
 * D1 - destinationDisplacementInt
 * B1 - destinationRegister
 * D2 - sourceDisplacementInt
 * B2 - sourceRegister
 * L -  length
 *
 * Returns: instruction size in bytes
 */
I_32
MVC(I_8 *instructionPtr, I_32 destinationDisplacementInt, I_32 length, I_32 destinationRegister, I_32 sourceDisplacementInt, I_32 sourceRegister)
{
	I_32 opcode = 0xD2000000;
	*((I_32 *)instructionPtr) = (opcode | ((length & 0xFF) << 16) | ((destinationRegister & 0xF) << 12) | (destinationDisplacementInt & 0xFFF));
	instructionPtr[4] = ((0x0F & sourceRegister) << 4) | ((sourceDisplacementInt & 0xF00) >> 8);
	instructionPtr[5] = sourceDisplacementInt & 0xFF;
	return 6;
}

/*
 * Generates an instruction that creates a PC relative jump/branch if the CC is set to a specified value.
 * A 32 bit integer value specifies the number of halfword to jump from current PC to the destination address.
 * Format: BRCL M1, RI2
 * M1 - conditionMask
 * RI2 - immediateInteger
 *
 * Returns: instruction size in bytes
 */
I_32
BRCL(I_8 *instructionPtr, I_32 conditionMask, I_32 immediateInteger)
{
	I_32 opcode = 0xC0040000;
	*((I_32 *)instructionPtr) = (opcode | ((conditionMask & 0xF) << 20) | ((immediateInteger & 0xFFFF0000) >> 16));
	instructionPtr[4] = (immediateInteger & 0xFF00) >> 8;
	instructionPtr[5] = immediateInteger & 0xFF;
	return 6;
}

/*
 * Generates an instruction to add a 32-bit integer constant to the 32-bit value in R1
 * Format: "AFI R1,I2"
 * R1 - sourceRegister
 * I2 - integerConstant
 *
 * Returns: instruction size in bytes
 */
I_32
AFI(I_8 *instructionPtr, I_32 sourceRegister, I_32 immediateInteger)
{
	I_32 opcode = 0xC2090000;
	*((I_32 *)instructionPtr) = (opcode | ((sourceRegister & 0x0000000F) << 20) | ((immediateInteger & 0xFFFF0000) >> 16));
	instructionPtr[4] = (I_8)((immediateInteger & 0xFF00) >> 8);
	instructionPtr[5] = (I_8)(immediateInteger & 0xFF);
	return 6;

}

/*
 * Generates a sequence of instructions to copy data from a Java MemorySegment Object to a C struct.
 * The struct is smaller than or equal to 4096 bytes.
 */
I_32
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
I_32
generateLargeStructCopyInstructions(I_8 *instructionPtr, I_32 structSize, I_32 offsetToParameterArea, I_32 sourceRegister, I_32 destinationRegister)
{
	I_32 totalInstructionSize = 0;

	const I_32 numFullSlots = structSize / MAX_MVC_COPY_LENGTH;
	const I_32 residue = structSize % MAX_MVC_COPY_LENGTH;
	const I_32 maxMVCInstrPerIteration = MAX_MVC_DISPLACEMENT / MAX_MVC_COPY_LENGTH;

	const I_32 loopResidue = numFullSlots % maxMVCInstrPerIteration;

	I_32 destinationDisplacementInt = -1;
	I_32 sourceDisplacementInt = -1;
	I_32 currMVCLength = -1;

	// Load loop counter into R3
	totalInstructionSize += IILF(instructionPtr + totalInstructionSize, R0, numFullSlots - loopResidue);
	I_32 totalSizeAtLoopStart = totalInstructionSize;

	// Generate a loop that will copy 4096 bytes at a time (i.e. 16 MVC instructions). The loop concludes when there
	// are less than 4096 bytes left to copy.
	for (int i = 0; i < maxMVCInstrPerIteration; i++) {
		// MVC D1(L,B1),D2(B2)
		// The 'L' field is 0-based
		destinationDisplacementInt = i * MAX_MVC_COPY_LENGTH;
		currMVCLength = MAX_MVC_COPY_LENGTH - 1;
		sourceDisplacementInt = i * MAX_MVC_COPY_LENGTH;
		totalInstructionSize += MVC(instructionPtr + totalInstructionSize, destinationDisplacementInt, currMVCLength, destinationRegister, sourceDisplacementInt, sourceRegister);
	}
	totalInstructionSize += LAY(instructionPtr + totalInstructionSize, destinationRegister, 0, destinationRegister, MAX_MVC_DISPLACEMENT);
	totalInstructionSize += LAY(instructionPtr + totalInstructionSize, sourceRegister, 0, sourceRegister, MAX_MVC_DISPLACEMENT);
	totalInstructionSize += AFI(instructionPtr + totalInstructionSize, R0, -maxMVCInstrPerIteration);

	const I_32 conditionMask = 7;
	I_32 totalSizeAtLoopEnd = totalInstructionSize;
	totalInstructionSize += BRCL(instructionPtr + totalInstructionSize, conditionMask, -((totalSizeAtLoopEnd - totalSizeAtLoopStart)/2));

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
		destinationDisplacementInt = loopResidue > 0 ? loopResidue * MAX_MVC_COPY_LENGTH : 0; // (numFullSlots - maxMVCInstrPerIteration) * MAX_MVC_COPY_LENGTH;
		currMVCLength = residue - 1;
		sourceDisplacementInt = loopResidue > 0 ? loopResidue * MAX_MVC_COPY_LENGTH : 0;// (numFullSlots - maxMVCInstrPerIteration) * MAX_MVC_COPY_LENGTH;
		totalInstructionSize += MVC(instructionPtr + totalInstructionSize, destinationDisplacementInt, currMVCLength, destinationRegister, sourceDisplacementInt, sourceRegister);
	}
	return totalInstructionSize;
}

#endif /* FFIUPCALLTHUNKGENHELPERS_HPP_ */
