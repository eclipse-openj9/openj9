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

#define STACK_SLOT_SIZE 8

#define ROUND_UP_TO_SLOT_MULTIPLE(s) ( ((s) + (STACK_SLOT_SIZE-1)) & (~(STACK_SLOT_SIZE-1)) )
#define ROUND_UP_SLOT(si) (((si) + 7) / 8)

#define MAX_GPRS 16
#define MAX_GPRS_PASSED_IN_REGS 6
#define MAX_FPRS_PASSED_IN_REGS 8

typedef enum StructPassingMechanismEnum {
	PASS_STRUCT_IN_MEMORY,
	PASS_STRUCT_IN_ONE_FPR,
	PASS_STRUCT_IN_TWO_FPR,
	PASS_STRUCT_IN_ONE_GPR_ONE_FPR,
	PASS_STRUCT_IN_ONE_FPR_ONE_GPR,
	PASS_STRUCT_IN_ONE_GPR,
	PASS_STRUCT_IN_TWO_GPR
} X64StructPassingMechanism;

#define REX	0x40
#define REX_W	0x08
#define REX_R	0x04
#define REX_X	0x02
#define REX_B	0x01

enum X64_GPR {
	rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi,
	r8, r9, r10, r11, r12, r13, r14, r15
};

enum X64_FPR {
	xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7
};

struct modRM_encoding {
	uint8_t rexr;
	uint8_t reg;
	uint8_t rexb;
	uint8_t rm;
};

// Also used for xmm0 - xmm7
const struct modRM_encoding modRM[MAX_GPRS] = {

	//  rexr   reg       rexb   rm
	//  ------ --------- ------ ---
	  { 0,     0x0 << 3, 0,     0x0 }, // rax / xmm0
	  { 0,     0x1 << 3, 0,     0x1 }, // rcx / xmm1
	  { 0,     0x2 << 3, 0,     0x2 }, // rdx / xmm2
	  { 0,     0x3 << 3, 0,     0x3 }, // rbx / xmm3
	  { 0,     0x4 << 3, 0,     0x4 }, // rsp / xmm4
	  { 0,     0x5 << 3, 0,     0x5 }, // rbp / xmm5
	  { 0,     0x6 << 3, 0,     0x6 }, // rsi / xmm6
	  { 0,     0x7 << 3, 0,     0x7 }, // rdi / xmm7
	  { REX_R, 0x0 << 3, REX_B, 0x0 }, // r8
	  { REX_R, 0x1 << 3, REX_B, 0x1 }, // r9
	  { REX_R, 0x2 << 3, REX_B, 0x2 }, // r10
	  { REX_R, 0x3 << 3, REX_B, 0x3 }, // r11
	  { 0x0,   0x0,      0x0,   0x0 }, // r12 (requires SIB to encode)
	  { REX_R, 0x5 << 3, REX_B, 0x5 }, // r13
	  { REX_R, 0x6 << 3, REX_B, 0x6 }, // r14
	  { REX_R, 0x7 << 3, REX_B, 0x7 }  // r15
};

const I_8 registerValues[MAX_GPRS] = {
	0,  // rax
	1,  // rcx
	2,  // rdx
	3,  // rbx
	4,  // rsp
	5,  // rbp
	6,  // rsi
	7,  // rdi
	0,  // r8
	1,  // r9
	2,  // r10
	3,  // r11
	4,  // r12
	5,  // r13
	6,  // r14
	7   // r15
};

const X64_GPR gprParmRegs[MAX_GPRS_PASSED_IN_REGS] = {
	rdi, rsi, rdx, rcx, r8, r9
};

const X64_FPR fprParmRegs[MAX_FPRS_PASSED_IN_REGS] = {
	xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7
};

typedef struct structParmInMemoryMetaData {

	// Stack offset to struct parm passed in memory
	I_32 memParmCursor;

	// Stack offset to argList entry to copy struct parm
	I_32 frameOffsetCursor;

	// Size of the struct parm in bytes
	U_32 sizeofStruct;

} structParmInMemoryMetaDataStruct;

// -----------------------------------------------------------------------------
// MOV treg, [rsp + disp32]
//
#define L8_TREG_mRSP_DISP32m(cursor, treg, disp32) \
	{ \
	*cursor = REX | REX_W; \
	uint8_t *rex = cursor++; \
	*cursor++ = 0x8b; \
	*cursor++ = 0x84 | modRM[treg].reg; \
	*rex |= modRM[treg].rexr; \
	*cursor++ = 0x24; \
	*(int32_t *)cursor = disp32; \
	cursor += 4; \
	}

// REX + op + modRM + SIB + disp32
#define L8_TREG_mRSP_DISP32m_LENGTH (1+3+4)

// -----------------------------------------------------------------------------
// MOV treg, [sreg + disp8]
//
// Warning: this does not encode sreg=r12 (requires a SIB byte)
//
#define L8_TREG_mSREG_DISP8m(cursor, treg, sreg, disp8) \
	{ \
	*cursor = REX | REX_W; \
	uint8_t *rex = cursor++; \
	*cursor++ = 0x8b; \
	*cursor++ = 0x40 | modRM[treg].reg | modRM[sreg].rm; \
	*rex |= (modRM[treg].rexr | modRM[sreg].rexb); \
	*(int8_t *)cursor = disp8; \
	cursor += 1; \
	}

// REX + op + modRM + disp8
#define L8_TREG_mSREG_DISP8m_LENGTH (1+2+1)

// -----------------------------------------------------------------------------
// MOV treg, [sreg]
//
// Warning: this does not encode sreg=r12 (requires a SIB byte)
//
#define L8_TREG_mSREGm(cursor, treg, sreg) \
	{ \
	*cursor = REX | REX_W; \
	uint8_t *rex = cursor++; \
	*cursor++ = 0x8b; \
	*cursor++ = 0x00 | modRM[treg].reg | modRM[sreg].rm; \
	*rex |= (modRM[treg].rexr | modRM[sreg].rexb); \
	}

// REX + op + modRM
#define L8_TREG_mSREGm_LENGTH (1+2)

// -----------------------------------------------------------------------------
// MOV [rsp + disp32], sreg
//
#define S8_mRSP_DISP32m_SREG(cursor, disp32, sreg) \
	{ \
	*cursor = REX | REX_W; \
	uint8_t *rex = cursor++; \
	*cursor++ = 0x89; \
	*cursor++ = 0x84 | modRM[sreg].reg; \
	*rex |= modRM[sreg].rexr; \
	*cursor++ = 0x24; \
	*(int32_t *)cursor = disp32; \
	cursor += 4; \
	}

// REX + op + modRM + SIB + disp32
#define S8_mRSP_DISP32m_SREG_LENGTH (1+3+4)

// -----------------------------------------------------------------------------
// MOVSS treg, [rsp + disp32]
//
#define MOVSS_TREG_mRSP_DISP32m(cursor, treg, disp32) \
	{ \
	*cursor++ = 0xf3; \
	*cursor++ = 0x0f; \
	*cursor++ = 0x10; \
	*cursor++ = 0x84 | modRM[treg].reg; \
	*cursor++ = 0x24; \
	*(int32_t *)cursor = disp32; \
	cursor += 4; \
	}

// 3*op + modRM + SIB + disp32
#define MOVSS_TREG_mRSP_DISP32m_LENGTH (3+2+4)

// -----------------------------------------------------------------------------
// MOVSS treg, [sreg]
//
#define MOVSS_TREG_mSREGm(cursor, treg, sreg) \
	{ \
	*cursor++ = 0xf3; \
	if (modRM[sreg].rexb) { \
		*cursor++ = REX | modRM[sreg].rexb; \
	} \
	*cursor++ = 0x0f; \
	*cursor++ = 0x10; \
	*cursor++ = 0x00 | modRM[treg].reg | modRM[sreg].rm; \
	}

// REX + 3*op + modRM
// Note: REX is always conservatively included in this calculation
#define MOVSS_TREG_mSREGm_LENGTH (1+3+1)

// -----------------------------------------------------------------------------
// MOVSS treg, [sreg + disp8]
//
#define MOVSS_TREG_mSREG_DISP8m(cursor, treg, sreg, disp8) \
	{ \
	*cursor++ = 0xf3; \
	if (modRM[sreg].rexb) { \
		*cursor++ = REX | modRM[sreg].rexb; \
	} \
	*cursor++ = 0x0f; \
	*cursor++ = 0x10; \
	*cursor++ = 0x40 | modRM[treg].reg | modRM[sreg].rm; \
	*(int8_t *)cursor = disp8; \
	cursor += 1; \
	}

// REX + 3*op + modRM + disp8
// Note: REX is always conservatively included in this calculation
#define MOVSS_TREG_mSREG_DISP8m_LENGTH (1+3+1+1)

// -----------------------------------------------------------------------------
// MOVSS [rsp + disp32], sreg
//
#define MOVSS_mRSP_DISP32m_SREG(cursor, disp32, sreg) \
	{ \
	*cursor++ = 0xf3; \
	*cursor++ = 0x0f; \
	*cursor++ = 0x11; \
	*cursor++ = 0x84 | modRM[sreg].reg; \
	*cursor++ = 0x24; \
	*(int32_t *)cursor = disp32; \
	cursor += 4; \
	}

// 3*op + modRM + SIB + disp32
#define MOVSS_mRSP_DISP32m_SREG_LENGTH (3+2+4)

// -----------------------------------------------------------------------------
// MOVSD treg, [rsp + disp32]
//
#define MOVSD_TREG_mRSP_DISP32m(cursor, treg, disp32) \
	{ \
	*cursor++ = 0xf2; \
	*cursor++ = 0x0f; \
	*cursor++ = 0x10; \
	*cursor++ = 0x84 | modRM[treg].reg; \
	*cursor++ = 0x24; \
	*(int32_t *)cursor = disp32; \
	cursor += 4; \
	}

// 3*op + modRM + SIB + disp32
#define MOVSD_TREG_mRSP_DISP32m_LENGTH (3+2+4)

// -----------------------------------------------------------------------------
// MOVSD treg, [sreg]
//
#define MOVSD_TREG_mSREGm(cursor, treg, sreg) \
	{ \
	*cursor++ = 0xf2; \
	if (modRM[sreg].rexb) { \
		*cursor++ = REX | modRM[sreg].rexb; \
	} \
	*cursor++ = 0x0f; \
	*cursor++ = 0x10; \
	*cursor++ = 0x00 | modRM[treg].reg | modRM[sreg].rm; \
	}

// REX + 3*op + modRM
// Note: REX is always conservatively included in this calculation
#define MOVSD_TREG_mSREGm_LENGTH (1+3+1)

// -----------------------------------------------------------------------------
// MOVSD treg, [sreg + disp8]
//
#define MOVSD_TREG_mSREG_DISP8m(cursor, treg, sreg, disp8) \
	{ \
	*cursor++ = 0xf2; \
	if (modRM[sreg].rexb) { \
		*cursor++ = REX | modRM[sreg].rexb; \
	} \
	*cursor++ = 0x0f; \
	*cursor++ = 0x10; \
	*cursor++ = 0x40 | modRM[treg].reg | modRM[sreg].rm; \
	*(int8_t *)cursor = disp8; \
	cursor += 1; \
	}

// REX + 3*op + modRM + disp8
// Note: REX is always conservatively included in this calculation
#define MOVSD_TREG_mSREG_DISP8m_LENGTH (1+3+1+1)

// -----------------------------------------------------------------------------
// MOVSD [rsp + disp32], sreg
//
#define MOVSD_mRSP_DISP32m_SREG(cursor, disp32, sreg) \
	{ \
	*cursor++ = 0xf2; \
	*cursor++ = 0x0f; \
	*cursor++ = 0x11; \
	*cursor++ = 0x84 | modRM[sreg].reg; \
	*cursor++ = 0x24; \
	*(int32_t *)cursor = disp32; \
	cursor += 4; \
	}

// 3*op + modRM + SIB + disp32
#define MOVSD_mRSP_DISP32m_SREG_LENGTH (3+2+4)


// -----------------------------------------------------------------------------
// MOV treg, imm32
//
#define MOV_TREG_IMM32(cursor, treg, imm32) \
	{ \
	*cursor = REX | REX_W; \
	uint8_t *rex = cursor++; \
	*cursor++ = 0xc7; \
	*cursor++ = 0xc0 | modRM[treg].rm; \
	*rex |= modRM[treg].rexb; \
	*(int32_t *)cursor = imm32; \
	cursor += 4; \
	}

// REX + op + modRM + imm32
#define MOV_TREG_IMM32_LENGTH (1+2+4)

// -----------------------------------------------------------------------------
// MOV treg, imm64
//
#define MOV_TREG_IMM64(cursor, treg, imm64) \
	{ \
	*cursor = REX | REX_W; \
	uint8_t *rex = cursor++; \
	*cursor++ = 0xb8 | modRM[treg].rm; \
	*rex |= modRM[treg].rexb; \
	*(int64_t *)cursor = imm64; \
	cursor += 8; \
	}

// REX + op + imm64
#define MOV_TREG_IMM64_LENGTH (1+1+8)

// -----------------------------------------------------------------------------
// MOV treg, sreg
//
#define MOV_TREG_SREG(cursor, treg, sreg) \
	{ \
	*cursor = REX | REX_W; \
	uint8_t *rex = cursor++; \
	*cursor++ = 0x89; \
	*cursor++ = 0xc0 | modRM[treg].rm | modRM[sreg].reg; \
	*rex |= modRM[treg].rexb | modRM[sreg].rexr; \
	}

// REX + op + modRM
#define MOV_TREG_SREG_LENGTH (3)

// -----------------------------------------------------------------------------
// LEA treg, [rsp + disp32]
//
#define LEA_TREG_mRSP_DISP32m(cursor, treg, disp32) \
	{ \
	*cursor = REX | REX_W; \
	uint8_t *rex = cursor++; \
	*cursor++ = 0x8d; \
	*cursor++ = 0x84 | modRM[treg].reg; \
	*rex |= modRM[treg].rexr; \
	*cursor++ = 0x24; \
	*(int32_t *)cursor = disp32; \
	cursor += 4; \
	}

// REX + op + modRM + SIB + disp32
#define LEA_TREG_mRSP_DISP32m_LENGTH (1+3+4)

// -----------------------------------------------------------------------------
// SUB rsp, imm32
//
#define SUB_RSP_IMM32(cursor, imm32) \
	*cursor++ = REX | REX_W; \
	*cursor++ = 0x81; \
	*cursor++ = 0xec; \
	*(int32_t *)cursor = imm32; \
	cursor += 4;

// REX + op + modRM + imm32
#define SUB_RSP_IMM32_LENGTH (1+2+4)

// -----------------------------------------------------------------------------
// SUB rsp, imm8
//
#define SUB_RSP_IMM8(cursor, imm8) \
	*cursor++ = REX | REX_W; \
	*cursor++ = 0x83; \
	*cursor++ = 0xec; \
	*cursor++ = imm8;

// REX + op + modRM + imm8
#define SUB_RSP_IMM8_LENGTH (1+2+1)

// -----------------------------------------------------------------------------
// ADD rsp, imm32
//
#define ADD_RSP_IMM32(cursor, imm32) \
	*cursor++ = REX | REX_W; \
	*cursor++ = 0x81; \
	*cursor++ = 0xc4; \
	*(int32_t *)cursor = imm32; \
	cursor += 4;

// REX + op + modRM + disp32
#define ADD_RSP_IMM32_LENGTH (1+2+4)

// -----------------------------------------------------------------------------
// ADD rsp, imm8
//
#define ADD_RSP_IMM8(cursor, imm8) \
	*cursor++ = REX | REX_W; \
	*cursor++ = 0x83; \
	*cursor++ = 0xc4; \
	*cursor++ = imm8;

// REX + op + modRM + imm8
#define ADD_RSP_IMM8_LENGTH (1+2+1)

// -----------------------------------------------------------------------------
// PUSH sreg
//
#define PUSH_SREG(cursor, sreg) \
	{ \
	if (modRM[sreg].rexb) { \
		*cursor++ = REX | modRM[sreg].rexb; \
	} \
	*cursor++ = 0x50 + registerValues[sreg]; \
	}

// REX + op
#define PUSH_SREG_LENGTH(sreg) ((modRM[sreg].rexb ? 1 : 0) + 1)

// -----------------------------------------------------------------------------
// POP sreg
//
#define POP_SREG(cursor, treg) \
	{ \
	if (modRM[treg].rexb) { \
		*cursor++ = REX | modRM[treg].rexb; \
	} \
	*cursor++ = 0x58 + registerValues[treg]; \
	}

// REX + op
#define POP_SREG_LENGTH(treg) ((modRM[treg].rexb ? 1 : 0) + 1)

// -----------------------------------------------------------------------------
// CALL [sreg + disp32]
//
#define CALL_mSREG_DISP32m(cursor, sreg, disp32) \
	{ \
	if (modRM[sreg].rexb) { \
		*cursor++ = (REX | modRM[sreg].rexb); \
	} \
	*cursor++ = 0xff; \
	*cursor++ = 0x90 | modRM[sreg].rm; \
	*(int32_t *)cursor = disp32; \
	cursor += 4; \
	}

// REX + op + modRM + disp32
#define CALL_mSREG_DISP32m_LENGTH(sreg) ((modRM[sreg].rexb ? 1 : 0) + 2 + 4)

// -----------------------------------------------------------------------------
// CALL [sreg + disp8]
//
#define CALL_mSREG_DISP8m(cursor, sreg, disp8) \
	{ \
	if (modRM[sreg].rexb) { \
		*cursor++ = (REX | modRM[sreg].rexb); \
	} \
	*cursor++ = 0xff; \
	*cursor++ = 0x50 | modRM[sreg].rm; \
	*cursor++ = disp8; \
	}

// REX + op + modRM + disp32
#define CALL_mSREG_DISP8m_LENGTH(sreg) ((modRM[sreg].rexb ? 1 : 0) + 2 + 1)

// -----------------------------------------------------------------------------
// RET
//
#define RET(cursor) \
	{ \
	*cursor++ = 0xc3; \
	}

#define RET_LENGTH (1)

// -----------------------------------------------------------------------------
// INT3
//
#define INT3(cursor) \
	{ \
	*cursor++ = 0xcc; \
	}

#define INT3_LENGTH (1)

// -----------------------------------------------------------------------------
// REP MOVSB
//
#define REP_MOVSB(cursor) \
	{ \
	*cursor++ = 0xf3; \
	*cursor++ = 0xa4; \
	}

#define REP_MOVSB_LENGTH (2)


/**
 * @brief Analyzes a struct passed as a parameter to determine how it should be
 *        handled by the linkage.  This also handles struct return values.
 *
 * @param[in] gprRegParmCount : current count of linkage GPRs used.  This should
 *               be 0 when processing struct return values.
 * @param[in] fprRegParmCount : current count of linkage FPRs used.  This should
 *               be 0 when processing struct return values.
 * @param[in] structParm : the \c J9UpcallSigType info for the parameter
 *
 * @return A \c X64StructPassingMechanism value describing how this parameter
 *         should be handled.
 */
static X64StructPassingMechanism
analyzeStructParm(I_32 gprRegParmCount, I_32 fprRegParmCount, J9UpcallSigType structParm) {

	I_32 structSize = structParm.sizeInByte;

	if (structSize > 16) {
		return PASS_STRUCT_IN_MEMORY;
	}

	switch (structParm.type) {
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP:  // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP:
		{
			I_32 numParmFPRsRequired = (structSize <= 8) ? 1 : 2;
			if (fprRegParmCount + numParmFPRsRequired > MAX_FPRS_PASSED_IN_REGS) {
				return PASS_STRUCT_IN_MEMORY;
			}

			return (numParmFPRsRequired == 1) ? PASS_STRUCT_IN_ONE_FPR : PASS_STRUCT_IN_TWO_FPR;
		}
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_DP:     // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_SP_DP:  // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP:     // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP_SP:
		{
			if (fprRegParmCount + 2 > MAX_FPRS_PASSED_IN_REGS) {
				return PASS_STRUCT_IN_MEMORY;
			}

			return PASS_STRUCT_IN_TWO_FPR;
		}
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_SP:  // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_DP:
		{
			if ((gprRegParmCount + 1 > MAX_GPRS_PASSED_IN_REGS) ||
			    (fprRegParmCount + 1 > MAX_FPRS_PASSED_IN_REGS)) {
				return PASS_STRUCT_IN_MEMORY;
			}

			return PASS_STRUCT_IN_ONE_GPR_ONE_FPR;
		}
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_MISC:  // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_MISC:
		{
			if ((gprRegParmCount + 1 > MAX_GPRS_PASSED_IN_REGS) ||
			    (fprRegParmCount + 1 > MAX_FPRS_PASSED_IN_REGS)) {
				return PASS_STRUCT_IN_MEMORY;
			}

			return PASS_STRUCT_IN_ONE_FPR_ONE_GPR;
		}
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC:
		{
			I_32 numParmGPRsRequired = (structSize <= 8) ? 1 : 2;
			if (gprRegParmCount + numParmGPRsRequired > MAX_GPRS_PASSED_IN_REGS) {
				return PASS_STRUCT_IN_MEMORY;
			}

			return (numParmGPRsRequired == 1) ? PASS_STRUCT_IN_ONE_GPR : PASS_STRUCT_IN_TWO_GPR;
		}
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_OTHER:
		{
			return PASS_STRUCT_IN_MEMORY;
			break;
		}
		default:
		{
			Assert_VM_unreachable();
			return PASS_STRUCT_IN_MEMORY;
		}
	}
}


/**
 * @brief Generate the appropriate thunk/adaptor for a given J9UpcallMetaData
 *
 * @param metaData[in/out] a pointer to the given J9UpcallMetaData
 * @return the address for this future upcall function handle, either the thunk or the thunk-descriptor
 *
 * Details:
 *
 * The callin thunk essentially marshalls incoming C parameters into a sequential array
 * that is passed to a callin helper.  It also marshalls the return value from the callin
 * helper to the calling C function.
 *
 * Thunk generation proceeds in three stages:
 * 1) Determine the instructions and their sizes necessary to build the callin thunk.  This involves
 *    analyzing each parameter and the return value for the appropriate passing mechanism.
 *    Storage for the array passed to the helper is always allocated on the stack.  The incoming arguments
 *    are always copied to the outgoing array (i.e., no frame sharing with caller is performed).
 *    The stack addressability displacement is always assumed to be 32-bits for simplicity.
 * 2) Allocation of thunk memory
 * 3) Generating thunk instructions to the allocated buffer
 *
 */
void *
createUpcallThunk(J9UpcallMetaData *metaData)
{
	J9JavaVM *vm = metaData->vm;
	PORT_ACCESS_FROM_JAVAVM(vm);
	const J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	J9UpcallSigType *sigArray = metaData->nativeFuncSignature->sigArray;
	I_32 lastSigIdx = (I_32)(metaData->nativeFuncSignature->numSigs - 1); // The index of the return type in the signature array
	I_32 gprRegSpillInstructionCount = 0;
	I_32 gprRegFillInstructionCount = 0;
	I_32 fprRegSpillInstructionCount = 0;
	I_32 fprRegFillInstructionCount = 0;
	I_32 gprRegParmCount = 0;
	I_32 fprRegParmCount = 0;
	I_32 copyStructInstructionsByteCount = 0;
	I_32 prepareStructReturnInstructionsLength = 0;
	I_32 numStructsPassedInMemory = 0;
	I_32 stackSlotCount = 0;
	I_32 preservedRegisterAreaSize = 0;
	bool hiddenParameter = false;

	Assert_VM_true(lastSigIdx >= 0);

	// -------------------------------------------------------------------------------
	// Set up the appropriate VM upcall dispatch function based the return type
	// -------------------------------------------------------------------------------

	switch (sigArray[lastSigIdx].type) {
		case J9_FFI_UPCALL_SIG_TYPE_VOID:
			metaData->upCallCommonDispatcher = (void *)vmFuncs->native2InterpJavaUpcall0;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_CHAR:  // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_SHORT: // Fall through
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
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP:   // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP:   // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_DP:    // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_SP_DP: // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP:    // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP_SP: // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_SP:  // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_DP:  // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_MISC:  // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_MISC:  // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC:     // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_OTHER:
		{
			metaData->upCallCommonDispatcher = (void *)vmFuncs->native2InterpJavaUpcallStruct;
			X64StructPassingMechanism mechanism = analyzeStructParm(0, 0, sigArray[lastSigIdx]);
			switch (mechanism) {
				case PASS_STRUCT_IN_MEMORY:
					hiddenParameter = true;
					gprRegParmCount++;

					// A preserved register will hold the hidden parameter across the function call.
					// The preserved register must therefore be saved/restored.
					preservedRegisterAreaSize = 8;

					prepareStructReturnInstructionsLength +=
						 (MOV_TREG_SREG_LENGTH   // mov rbx, rax (preserve hidden parameter)
						+ MOV_TREG_SREG_LENGTH   // mov rsi, rax (return value from call)
						+ MOV_TREG_SREG_LENGTH   // mov rdi, rbx (address from preserved hidden parameter)
						+ MOV_TREG_IMM32_LENGTH
						+ REP_MOVSB_LENGTH
						+ MOV_TREG_SREG_LENGTH); // mov rax, rbx (return caller-supplied  buffer in rax)
					break;
				case PASS_STRUCT_IN_ONE_FPR:
					prepareStructReturnInstructionsLength += MOVSD_TREG_mSREGm_LENGTH;
					break;
				case PASS_STRUCT_IN_TWO_FPR:
					prepareStructReturnInstructionsLength +=
						(MOVSD_TREG_mSREGm_LENGTH + MOVSD_TREG_mSREG_DISP8m_LENGTH);
					break;
				case PASS_STRUCT_IN_ONE_GPR_ONE_FPR:
					prepareStructReturnInstructionsLength +=
						(MOVSD_TREG_mSREG_DISP8m_LENGTH + L8_TREG_mSREGm_LENGTH);
					break;
				case PASS_STRUCT_IN_ONE_FPR_ONE_GPR:
					prepareStructReturnInstructionsLength +=
						(MOVSD_TREG_mSREGm_LENGTH + L8_TREG_mSREG_DISP8m_LENGTH);
					break;
				case PASS_STRUCT_IN_ONE_GPR:
					prepareStructReturnInstructionsLength += L8_TREG_mSREGm_LENGTH;
					break;
				case PASS_STRUCT_IN_TWO_GPR:
					prepareStructReturnInstructionsLength +=
						(L8_TREG_mSREG_DISP8m_LENGTH + L8_TREG_mSREGm_LENGTH);
					break;
				default:
					Assert_VM_unreachable();
			}

			break;
		}
		default:
			Assert_VM_unreachable();
	}

	// -------------------------------------------------------------------------------
	// Determine instruction and buffer requirements from each parameter
	// -------------------------------------------------------------------------------

	for (I_32 i = 0; i < lastSigIdx; i++) {
		switch (sigArray[i].type) {
			case J9_FFI_UPCALL_SIG_TYPE_CHAR:    // Fall through
			case J9_FFI_UPCALL_SIG_TYPE_SHORT:   // Fall through
			case J9_FFI_UPCALL_SIG_TYPE_INT32:   // Fall through
			case J9_FFI_UPCALL_SIG_TYPE_POINTER: // Fall through
			case J9_FFI_UPCALL_SIG_TYPE_INT64:
			{
				stackSlotCount++;

				if (gprRegParmCount < MAX_GPRS_PASSED_IN_REGS) {
					// Parm must be spilled from parm register to argList
					gprRegParmCount++;
					gprRegSpillInstructionCount++;
				} else {
					// Parm must be filled from frame and spilled to argList
					gprRegFillInstructionCount++;
					gprRegSpillInstructionCount++;
				}
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_FLOAT:  // Fall through
			case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
			{
				stackSlotCount += 1;

				if (fprRegParmCount < MAX_FPRS_PASSED_IN_REGS) {
					// Parm must be spilled from parm register to argList
					fprRegParmCount += 1;
					fprRegSpillInstructionCount += 1;
				} else {
					// Parm must be filled from frame and spilled to argList
					fprRegFillInstructionCount += 1;
					fprRegSpillInstructionCount += 1;
				}

				break;
			}
			default:
			{
				stackSlotCount += ROUND_UP_TO_SLOT_MULTIPLE(sigArray[i].sizeInByte) / STACK_SLOT_SIZE;

				X64StructPassingMechanism mechanism = analyzeStructParm(gprRegParmCount, fprRegParmCount, sigArray[i]);
				switch (mechanism) {
					case PASS_STRUCT_IN_MEMORY:
						copyStructInstructionsByteCount +=
							  LEA_TREG_mRSP_DISP32m_LENGTH
							+ LEA_TREG_mRSP_DISP32m_LENGTH
							+ MOV_TREG_IMM32_LENGTH
							+ REP_MOVSB_LENGTH;
						numStructsPassedInMemory += 1;
						break;

					case PASS_STRUCT_IN_ONE_FPR:
						// Parm must be spilled from parm register to argList
						fprRegParmCount += 1;
						fprRegSpillInstructionCount += 1;
						break;

					case PASS_STRUCT_IN_TWO_FPR:
						// Parm must be spilled from two parm registers to argList
						fprRegParmCount += 2;
						fprRegSpillInstructionCount += 2;
						break;

					case PASS_STRUCT_IN_ONE_GPR_ONE_FPR:  // Fall through
					case PASS_STRUCT_IN_ONE_FPR_ONE_GPR:
						// Parm must be spilled from two parm registers to argList
						gprRegParmCount += 1;
						gprRegSpillInstructionCount += 1;
						fprRegParmCount += 1;
						fprRegSpillInstructionCount += 1;
						break;

					case PASS_STRUCT_IN_ONE_GPR:
						// Parm must be spilled from parm register to argList
						gprRegParmCount += 1;
						gprRegSpillInstructionCount += 1;
						break;

					case PASS_STRUCT_IN_TWO_GPR:
						// Parm must be spilled from two parm registers to argList
						gprRegParmCount += 2;
						gprRegSpillInstructionCount += 2;
						break;

					default:
						Assert_VM_unreachable();
				}
			}
		}
	}

	// -------------------------------------------------------------------------------
	// Calculate size of VM parameter buffer
	// -------------------------------------------------------------------------------

	I_32 frameSize =
		// Storage required to pass argList
		stackSlotCount * STACK_SLOT_SIZE;

	// Adjust frame size such that the end of the input argument area is a multiple of 16.
	if ((frameSize + preservedRegisterAreaSize) % 16 == 0) {
		frameSize += STACK_SLOT_SIZE;
	}

	// -------------------------------------------------------------------------------
	// Allocate thunk memory
	// -------------------------------------------------------------------------------

	I_32 thunkSize = 0;
	I_32 roundedCodeSize = 0;

	// Determines whether a debugger breakpoint is inserted at the start of each thunk.
	// This is only useful for debugging.
	I_32 breakOnEntry = 0;

	if (breakOnEntry) {
		thunkSize += INT3_LENGTH;
	}

	if (hiddenParameter) {
		// Save and restore preserved register
		thunkSize += (PUSH_SREG_LENGTH(rbx) + POP_SREG_LENGTH(rbx));
	}

	if (frameSize >= -128 && frameSize <= 127) {
		thunkSize += SUB_RSP_IMM8_LENGTH;
		thunkSize += ADD_RSP_IMM8_LENGTH;
	} else {
		thunkSize += SUB_RSP_IMM32_LENGTH;
		thunkSize += ADD_RSP_IMM32_LENGTH;
	}

	thunkSize += gprRegFillInstructionCount * L8_TREG_mRSP_DISP32m_LENGTH
	           + gprRegSpillInstructionCount * S8_mRSP_DISP32m_SREG_LENGTH;

	// MOVSS and MOVSD instructions are the same size
	thunkSize += fprRegFillInstructionCount * MOVSS_TREG_mRSP_DISP32m_LENGTH
	           + fprRegSpillInstructionCount * MOVSS_mRSP_DISP32m_SREG_LENGTH;

	thunkSize += copyStructInstructionsByteCount;

	Assert_VM_true(offsetof(J9UpcallMetaData, upCallCommonDispatcher) <= 127);

	thunkSize += MOV_TREG_IMM64_LENGTH
	           + MOV_TREG_SREG_LENGTH
	           + CALL_mSREG_DISP8m_LENGTH(rdi)
	           + RET_LENGTH;

	thunkSize += prepareStructReturnInstructionsLength;

	roundedCodeSize = ROUND_UP_TO_SLOT_MULTIPLE(thunkSize);

	metaData->thunkSize = roundedCodeSize;

	U_8 *thunkMem = (U_8 *)vmFuncs->allocateUpcallThunkMemory(metaData);
	if (NULL == thunkMem) {
		return NULL;
	}
	metaData->thunkAddress = (void *)thunkMem;

	// -------------------------------------------------------------------------------
	// Emit thunk instructions
	// -------------------------------------------------------------------------------

	I_32 frameOffsetCursor = 0;
	I_32 memParmCursor = 0;
	I_32 numStructsPassedInMemoryCursor = 0;
	gprRegParmCount = 0;
	fprRegParmCount = 0;
	structParmInMemoryMetaDataStruct *structParmInMemory = NULL;

	if (numStructsPassedInMemory > 0) {
		/**
		 * Allocate a bookkeeping structure for struct parms passed in memory to avoid
		 * a complete traversal of the parameters again.  The memory for this structure
		 * will be freed in this function once the information is consumed.
		 */
		structParmInMemory = (structParmInMemoryMetaDataStruct *)
			j9mem_allocate_memory(numStructsPassedInMemory * sizeof(structParmInMemoryMetaDataStruct), OMRMEM_CATEGORY_VM);

		if (NULL == structParmInMemory) {
			return NULL;
		}
	}

	U_8 *thunkCursor = thunkMem;

	if (breakOnEntry) {
		INT3(thunkCursor)
	}

	if (hiddenParameter) {
		PUSH_SREG(thunkCursor, rbx)
	}

	if (frameSize > 0) {
		if (frameSize >= -128 && frameSize <= 127) {
			SUB_RSP_IMM8(thunkCursor, frameSize)
		} else {
			SUB_RSP_IMM32(thunkCursor, frameSize)
		}
	}

	if (hiddenParameter) {
		// Copy the hidden parameter to a register preserved across the call (rbx)
		MOV_TREG_SREG(thunkCursor, rbx, gprParmRegs[0])
		gprRegParmCount++;
	}

	for (I_32 i = 0; i < lastSigIdx; i++) {
		switch (sigArray[i].type) {
			case J9_FFI_UPCALL_SIG_TYPE_CHAR:    // Fall through
			case J9_FFI_UPCALL_SIG_TYPE_SHORT:   // Fall through
			case J9_FFI_UPCALL_SIG_TYPE_INT32:   // Fall through
			case J9_FFI_UPCALL_SIG_TYPE_POINTER: // Fall through
			case J9_FFI_UPCALL_SIG_TYPE_INT64:
			{
				if (gprRegParmCount < MAX_GPRS_PASSED_IN_REGS) {
					// Parm must be spilled from parm register to argList
					S8_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, gprParmRegs[gprRegParmCount])
					gprRegParmCount++;
				} else {
					// Parm must be filled from frame and spilled to argList.
					// Use rax as the intermediary register since it is volatile
					L8_TREG_mRSP_DISP32m(thunkCursor, rax, frameSize + preservedRegisterAreaSize + 8 + memParmCursor)
					S8_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, rax)
					memParmCursor += STACK_SLOT_SIZE;
				}

				frameOffsetCursor += STACK_SLOT_SIZE;
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_FLOAT:  // Fall through
			case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
			{
				bool isFloat = (sigArray[i].type == J9_FFI_UPCALL_SIG_TYPE_FLOAT);
				if (fprRegParmCount < MAX_FPRS_PASSED_IN_REGS) {
					// Parm must be spilled from parm register to argList
					if (isFloat) {
						MOVSS_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, fprParmRegs[fprRegParmCount])
					} else {
						MOVSD_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, fprParmRegs[fprRegParmCount])
					}

					fprRegParmCount++;
				} else {
					// Parm must be filled from frame and spilled to argList.
					// Use xmm0 as the intermediary register since it is volatile and it
					// must have been processed as the first parameter already.
					if (isFloat) {
						MOVSS_TREG_mRSP_DISP32m(thunkCursor, xmm0, frameSize + preservedRegisterAreaSize + 8 + memParmCursor)
						MOVSS_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, xmm0)
					} else {
						MOVSD_TREG_mRSP_DISP32m(thunkCursor, xmm0, frameSize + preservedRegisterAreaSize + 8 + memParmCursor)
						MOVSD_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, xmm0)
					}

					memParmCursor += STACK_SLOT_SIZE;
				}

				frameOffsetCursor += STACK_SLOT_SIZE;
				break;
			}
			default:
			{
				// Handle structs passed in registers.  Structs passed in memory will be handled
				// after all other parameters are processed to avoid the complication of preserving
				// registers implicitly required for REP MOVSB.
				X64StructPassingMechanism mechanism = analyzeStructParm(gprRegParmCount, fprRegParmCount, sigArray[i]);
				switch (mechanism) {
					case PASS_STRUCT_IN_MEMORY:
					{
						// Record the source and destination offsets and the number of bytes to copy
						structParmInMemory[numStructsPassedInMemoryCursor].memParmCursor = memParmCursor;
						structParmInMemory[numStructsPassedInMemoryCursor].frameOffsetCursor = frameOffsetCursor;
						structParmInMemory[numStructsPassedInMemoryCursor].sizeofStruct = sigArray[i].sizeInByte;

						I_32 roundedStructSize = ROUND_UP_TO_SLOT_MULTIPLE(sigArray[i].sizeInByte);
						memParmCursor += roundedStructSize;
						frameOffsetCursor += roundedStructSize;
						numStructsPassedInMemoryCursor += 1;
						break;
					}
					case PASS_STRUCT_IN_ONE_FPR:
					{
						MOVSD_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, fprParmRegs[fprRegParmCount])
						fprRegParmCount += 1;
						frameOffsetCursor += STACK_SLOT_SIZE;
						break;
					}
					case PASS_STRUCT_IN_TWO_FPR:
					{
						MOVSD_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, fprParmRegs[fprRegParmCount])
						fprRegParmCount += 1;
						frameOffsetCursor += STACK_SLOT_SIZE;
						MOVSD_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, fprParmRegs[fprRegParmCount])
						fprRegParmCount += 1;
						frameOffsetCursor += STACK_SLOT_SIZE;
						break;
					}
					case PASS_STRUCT_IN_ONE_GPR_ONE_FPR:
					{
						S8_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, gprParmRegs[gprRegParmCount])
						gprRegParmCount++;
						frameOffsetCursor += STACK_SLOT_SIZE;
						MOVSD_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, fprParmRegs[fprRegParmCount])
						fprRegParmCount += 1;
						frameOffsetCursor += STACK_SLOT_SIZE;
						break;
					}
					case PASS_STRUCT_IN_ONE_FPR_ONE_GPR:
					{
						MOVSD_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, fprParmRegs[fprRegParmCount])
						fprRegParmCount += 1;
						frameOffsetCursor += STACK_SLOT_SIZE;
						S8_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, gprParmRegs[gprRegParmCount])
						gprRegParmCount++;
						frameOffsetCursor += STACK_SLOT_SIZE;
						break;
					}
					case PASS_STRUCT_IN_ONE_GPR:
					{
						S8_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, gprParmRegs[gprRegParmCount])
						gprRegParmCount++;
						frameOffsetCursor += STACK_SLOT_SIZE;
						break;
					}
					case PASS_STRUCT_IN_TWO_GPR:
					{
						S8_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, gprParmRegs[gprRegParmCount])
						gprRegParmCount++;
						frameOffsetCursor += STACK_SLOT_SIZE;
						S8_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, gprParmRegs[gprRegParmCount])
						gprRegParmCount++;
						frameOffsetCursor += STACK_SLOT_SIZE;
						break;
					}
					default:
						Assert_VM_unreachable();
				}
			}
		}
	}

	// Handle structs passed in memory.  No need to preserve rsi, rdi, rcx.
	if (numStructsPassedInMemory > 0) {
		for (I_32 i = 0; i < numStructsPassedInMemory; i++) {
			LEA_TREG_mRSP_DISP32m(thunkCursor, rsi, frameSize + preservedRegisterAreaSize + 8 + structParmInMemory[i].memParmCursor)
			LEA_TREG_mRSP_DISP32m(thunkCursor, rdi, structParmInMemory[i].frameOffsetCursor)
			MOV_TREG_IMM32(thunkCursor, rcx, structParmInMemory[i].sizeofStruct)
			REP_MOVSB(thunkCursor)
		}

		j9mem_free_memory(structParmInMemory);
	}

	// -------------------------------------------------------------------------------
	// Call or jump to the common upcall dispatcher
	// -------------------------------------------------------------------------------

	// Parm 1 : J9UpcallMetaData *data
	MOV_TREG_IMM64(thunkCursor, rdi, reinterpret_cast<int64_t>(metaData))

	// Parm 2 : void *argsListPointer
	MOV_TREG_SREG(thunkCursor, rsi, rsp)

	CALL_mSREG_DISP8m(thunkCursor, rdi, offsetof(J9UpcallMetaData, upCallCommonDispatcher))

	// -------------------------------------------------------------------------------
	// Process return value for ABI representation
	// -------------------------------------------------------------------------------

	switch (sigArray[lastSigIdx].type) {
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP:   // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP:   // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_DP:    // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_SP_DP: // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP:    // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP_SP: // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_SP:  // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_DP:  // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_MISC:  // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_MISC:  // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC:     // Fall through
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_OTHER:
		{
			X64StructPassingMechanism mechanism = analyzeStructParm(0, 0, sigArray[lastSigIdx]);
			switch (mechanism) {
				case PASS_STRUCT_IN_MEMORY:
					// rax == buffer address from return value
					MOV_TREG_SREG(thunkCursor, rsi, rax)

					// rbx = caller-supplied buffer address (preserved in rbx)
					MOV_TREG_SREG(thunkCursor, rdi, rbx)
					MOV_TREG_IMM32(thunkCursor, rcx, sigArray[lastSigIdx].sizeInByte)
					REP_MOVSB(thunkCursor)

					// rax must contain the address of the caller-supplied buffer
					MOV_TREG_SREG(thunkCursor, rax, rbx)
					break;
				case PASS_STRUCT_IN_ONE_FPR:
					MOVSD_TREG_mSREGm(thunkCursor, xmm0, rax)
					break;
				case PASS_STRUCT_IN_TWO_FPR:
					MOVSD_TREG_mSREGm(thunkCursor, xmm0, rax)
					MOVSD_TREG_mSREG_DISP8m(thunkCursor, xmm1, rax, 8)
					break;
				case PASS_STRUCT_IN_ONE_GPR_ONE_FPR:
					MOVSD_TREG_mSREG_DISP8m(thunkCursor, xmm0, rax, 8)
					L8_TREG_mSREGm(thunkCursor, rax, rax)
					break;
				case PASS_STRUCT_IN_ONE_FPR_ONE_GPR:
					MOVSD_TREG_mSREGm(thunkCursor, xmm0, rax)
					L8_TREG_mSREG_DISP8m(thunkCursor, rax, rax, 8)
					break;
				case PASS_STRUCT_IN_ONE_GPR:
					L8_TREG_mSREGm(thunkCursor, rax, rax)
					break;
				case PASS_STRUCT_IN_TWO_GPR:
					L8_TREG_mSREG_DISP8m(thunkCursor, rdx, rax, 8)
					L8_TREG_mSREGm(thunkCursor, rax, rax)
					break;
				default:
					Assert_VM_unreachable();
			}
		}
		default:
			// For all other return types the VM helper will have placed the
			// value in the correct register per the ABI.
			break;

	}

	// -------------------------------------------------------------------------------
	// Cleanup frame and return
	// -------------------------------------------------------------------------------

	if (frameSize > 0) {
		if (frameSize >= -128 && frameSize <= 127) {
			ADD_RSP_IMM8(thunkCursor, frameSize)
		} else {
			ADD_RSP_IMM32(thunkCursor, frameSize)
		}
	}

	if (hiddenParameter) {
		POP_SREG(thunkCursor, rbx)
	}

	RET(thunkCursor)

	// Check for thunk memory overflow
	Assert_VM_true( (thunkCursor - thunkMem) <= roundedCodeSize );

	// Finish up before returning
	vmFuncs->doneUpcallThunkGeneration(metaData, (void *)thunkMem);

	return (void *)thunkMem;
}

/**
 * @brief Calculate the requested argument in-stack memory address to return
 *
 * @param nativeSig[in] a pointer to the J9UpcallNativeSignature
 * @param argListPtr[in] a pointer to the argument list prepared by the thunk
 * @param argIdx[in] the requested argument index
 *
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
	I_32 lastSigIdx = (I_32)(nativeSig->numSigs - 1); // The index for the return type in the signature array
	I_32 stackSlotCount = 0;
	I_32 tempInt = 0;

	Assert_VM_true((argIdx >= 0) && (argIdx < lastSigIdx));

	// Loop through the arguments
	for (I_32 i = 0; i < argIdx; i++) {
		// Testing this argument
		tempInt = sigArray[i].sizeInByte;
		switch (sigArray[i].type & J9_FFI_UPCALL_SIG_TYPE_MASK) {
			case J9_FFI_UPCALL_SIG_TYPE_CHAR:    // Fall through
			case J9_FFI_UPCALL_SIG_TYPE_SHORT:   // Fall through
			case J9_FFI_UPCALL_SIG_TYPE_INT32:   // Fall through
			case J9_FFI_UPCALL_SIG_TYPE_POINTER: // Fall through
			case J9_FFI_UPCALL_SIG_TYPE_INT64:   // Fall through
			case J9_FFI_UPCALL_SIG_TYPE_FLOAT:   // Fall through
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

	return ((void *)((char *)argListPtr + (stackSlotCount * STACK_SLOT_SIZE)));
}

#endif /* JAVA_SPEC_VERSION >= 16 */

} /* extern "C" */
