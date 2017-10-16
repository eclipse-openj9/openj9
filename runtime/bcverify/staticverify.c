/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include <string.h>

/* #include "bcvcfr.h" */

#include "cfreader.h"
#include "bcsizes.h"
#include "j9protos.h"
#include "vrfyconvert.h"

#include "ut_j9bcverify.h"

#define BYTECODE_START 1
#define	STACKMAP_START 2

#define FATAL_CLASS_FORMAT_ERROR -1
#define FALLBACK_CLASS_FORMAT_ERROR -2
#define FALLBACK_VERIFY_ERROR -3

typedef struct StackmapExceptionDetails {
	I_32 stackmapFrameIndex;
	U_32 stackmapFrameBCI;
} StackmapExceptionDetails;

static IDATA checkMethodStructure (J9PortLibrary * portLib, J9CfrClassFile * classfile, UDATA methodIndex, U_8 * instructionMap, J9CfrError * error, U_32 flags, I_32 *hasRET);
static IDATA buildInstructionMap (J9CfrClassFile * classfile, J9CfrAttributeCode * code, U_8 * map, UDATA methodIndex, J9CfrError * error);
static IDATA checkBytecodeStructure (J9CfrClassFile * classfile, UDATA methodIndex, UDATA length, U_8 * map, J9CfrError * error, U_32 flags, I_32 *hasRET);
static IDATA checkStackMap (J9CfrClassFile* classfile, J9CfrMethod * method, J9CfrAttributeCode * code, U_8 * map, UDATA flags, StackmapExceptionDetails* exceptionDetails);
static I_32 checkStackMapEntries (J9CfrClassFile* classfile, J9CfrAttributeCode * code, U_8 * map, U_8 ** entries, UDATA slotCount, U_8 * end);

static IDATA 
buildInstructionMap (J9CfrClassFile * classfile, J9CfrAttributeCode * code, U_8 * map, UDATA methodIndex, J9CfrError * error)
{
	U_8 *bcStart;
	U_8 *bcIndex;
	U_8 *bcEnd;
	UDATA bc, npairs, temp;
	IDATA pc, high, low, tableSize;
	U_16 errorType;

	bcStart = code->code;
	bcIndex = bcStart;
	bcEnd = bcIndex + (code->codeLength * sizeof(U_8));
	pc = 0;
	while (bcIndex < bcEnd) {
		bc = (UDATA) *bcIndex;
		pc = bcIndex - bcStart;
		map[pc] = BYTECODE_START;
		if (bc < CFR_BC_tableswitch) {
			/* short circuit the switch for always clean ones */
			bcIndex += (sunJavaByteCodeRelocation[bc] & 7);
			continue;
		}
		switch (bc) {
		case CFR_BC_wide:
			if (*(bcIndex + 1) == CFR_BC_iinc) {
				bcIndex += 6;
			} else {
				bcIndex += 4;
			}
			break;

		case CFR_BC_tableswitch:
			bcIndex += (4 - (pc & 3));
			bcIndex += 4;
			low = (IDATA) NEXT_U32(temp, bcIndex);
			high = (IDATA) NEXT_U32(temp, bcIndex);
			tableSize = ((I_32)high - (I_32)low + 1);
			if ((I_32)low > (I_32)high || tableSize <=0 || tableSize >= 16384){
				errorType = J9NLS_CFR_ERR_BC_SWITCH_EMPTY__ID;
				goto _leaveProc;
			}
			bcIndex += tableSize * 4;
			/*
			 * If overflow occurred, bcIndex will be less than or equal to the bcIndex of the start of this bytecode.
			 * We need to check for this explicitly, as the (bcIndex < bcEnd) loop condition will not catch this case.
			 */
			if (bcIndex <= (bcStart + pc)) {
				errorType = J9NLS_CFR_ERR_BC_INCOMPLETE__ID;
				goto _leaveProc;
			}
			break;

		case CFR_BC_lookupswitch:
			bcIndex += (4 - (pc & 3));
			bcIndex += 4;
			low = (IDATA) NEXT_U32(npairs, bcIndex);
			if ((I_32)low < 0) {
				errorType = J9NLS_CFR_ERR_BC_SWITCH_NEGATIVE_COUNT__ID;
				goto _leaveProc;
			}
			bcIndex += npairs * 8;
			/*
			 * If overflow occurred, bcIndex will be less than or equal to the bcIndex of the start of this bytecode.
			 * We need to check for this explicitly, as the (bcIndex < bcEnd) loop condition will not catch this case.
			 */
			if (bcIndex <= (bcStart + pc)) {
				errorType = J9NLS_CFR_ERR_BC_INCOMPLETE__ID;
				goto _leaveProc;
			}
			break;

		default:
			if (bc > CFR_BC_jsr_w) {
				errorType = J9NLS_CFR_ERR_BC_UNKNOWN__ID;
				goto _leaveProc;
			}
			bcIndex += (sunJavaByteCodeRelocation[bc] & 7);
		}
	}


	return 0;

  _leaveProc:
	Trc_STV_buildInstructionMap_VerifyError(errorType, methodIndex, pc);
	buildMethodError(error, errorType, CFR_ThrowVerifyError, (I_32)methodIndex, (U_32)pc, &classfile->methods[methodIndex], classfile->constantPool);
	return -1;
}



/*
	##class format checking

	Walk the sun bytecodes checking for illegal branch targets and doing some constant pool verification

	Invalid returns will cause us to modify the classfile in place such that the second pass of the verification can correctly
	throw the error (unless it turns out to be dead code).

*/

static IDATA 
checkBytecodeStructure (J9CfrClassFile * classfile, UDATA methodIndex, UDATA length, U_8 * map, J9CfrError * error, U_32 flags, I_32 *hasRET)
{
#define CHECK_END \
	if(bcIndex > bcEnd) { \
		errorType = J9NLS_CFR_ERR_BC_INCOMPLETE__ID; \
		goto _verifyError; \
	}

	J9CfrAttributeCode *code;
	J9CfrConstantPoolInfo *info;
	J9CfrMethod *method;
	IDATA target, start, i1, i2, result, tableSize;
	U_8 *bcStart, *bcInstructionStart, *bcIndex, *bcEnd;
	UDATA bc, index, u1, u2, i, maxLocals, cpCount, tag, firstKey;
	UDATA sigChar;
	UDATA errorType;
	UDATA errorDataIndex = 0;


	method = &(classfile->methods[methodIndex]);
	sigChar = (UDATA) getReturnTypeFromSignature(classfile->constantPool[method->descriptorIndex].bytes, classfile->constantPool[method->descriptorIndex].slot1, NULL);
	code = method->codeAttribute;

	maxLocals = code->maxLocals;
	cpCount = classfile->constantPoolCount;

	bcStart = code->code;
	bcEnd = bcStart + length;
	bcIndex = bcStart;
	bcInstructionStart = bcIndex;
	while (bcIndex < bcEnd) {
		bcInstructionStart = bcIndex;
		/* Don't need to check that an instruction is in the code range here as method entries
			have 4 extra bytes at the end - exception and attribute counts so we won't exceed
			bcEnd by more than 4 (switches and wide iinc handled separately) and just test after the loop */
		NEXT_U8(bc, bcIndex);
		switch (bc) {
		case CFR_BC_nop:
		case CFR_BC_aconst_null:
		case CFR_BC_iconst_m1:
		case CFR_BC_iconst_0:
		case CFR_BC_iconst_1:
		case CFR_BC_iconst_2:
		case CFR_BC_iconst_3:
		case CFR_BC_iconst_4:
		case CFR_BC_iconst_5:
		case CFR_BC_lconst_0:
		case CFR_BC_lconst_1:
		case CFR_BC_fconst_0:
		case CFR_BC_fconst_1:
		case CFR_BC_fconst_2:
		case CFR_BC_dconst_0:
		case CFR_BC_dconst_1:
			break;

		case CFR_BC_bipush:
			bcIndex++;
			break;

		case CFR_BC_sipush:
			bcIndex+=2;
			break;

		case CFR_BC_ldc:
		case CFR_BC_ldc_w:
			if (bc == CFR_BC_ldc) {
				NEXT_U8(index, bcIndex);
			} else {
				NEXT_U16(index, bcIndex);
			}
			if ((!index) || (index >= cpCount)) {
				errorType = J9NLS_CFR_ERR_BAD_INDEX__ID;
				/* Jazz 82615: Set the constant pool index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			info = &(classfile->constantPool[index]);
			tag = (UDATA) info->tag;

			if (!((tag == CFR_CONSTANT_Integer)
				  || (tag == CFR_CONSTANT_Float)
				  || (tag == CFR_CONSTANT_String)
				  || ((tag == CFR_CONSTANT_Class)
							&& (((flags & BCT_MajorClassFileVersionMask) >= BCT_Java5MajorVersionShifted)
									|| ((flags & BCT_MajorClassFileVersionMask) == 0)))
				  || (((tag == CFR_CONSTANT_MethodType) || (tag == CFR_CONSTANT_MethodHandle))
						  && (((flags & BCT_MajorClassFileVersionMask) >= BCT_Java7MajorVersionShifted)
									|| ((flags & BCT_MajorClassFileVersionMask) == 0)))	)) {
				errorType = J9NLS_CFR_ERR_BC_LDC_NOT_CONSTANT__ID;
				/* Jazz 82615: Set the constant pool index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			break;

		case CFR_BC_ldc2_w:
			NEXT_U16(index, bcIndex);
			if ((!index) || (index >= cpCount - 1)) {
				errorType = J9NLS_CFR_ERR_BAD_INDEX__ID;
				/* Jazz 82615: Set the constant pool index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			info = &(classfile->constantPool[index]);
			tag = (UDATA) info->tag;
			if (!((tag == CFR_CONSTANT_Double)
				  || (tag == CFR_CONSTANT_Long))) {
				errorType = J9NLS_CFR_ERR_BC_LDC_NOT_CONSTANT__ID;
				/* Jazz 82615: Set the constant pool index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			break;

		case CFR_BC_iload:
		case CFR_BC_fload:
		case CFR_BC_aload:
			NEXT_U8(index, bcIndex);

			if (index >= maxLocals) {
				errorType = J9NLS_CFR_ERR_BC_LOAD_INDEX__ID;
				/* Jazz 82615: Set the local variable index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			break;

		case CFR_BC_lload:
		case CFR_BC_dload:
			NEXT_U8(index, bcIndex);

			if ((index + 1) >= maxLocals) {
				errorType = J9NLS_CFR_ERR_BC_LOAD_INDEX__ID;
				/* Jazz 82615: Set the local variable index to show up in the error message framework */
				errorDataIndex = index + 1;
				goto _verifyError;
			}
			break;

		case CFR_BC_iload_0:
		case CFR_BC_iload_1:
		case CFR_BC_iload_2:
		case CFR_BC_iload_3:
			index = bc - CFR_BC_iload_0;
			if (index >= maxLocals) {
				errorType = J9NLS_CFR_ERR_BC_LOAD_INDEX__ID;
				/* Jazz 82615: Set the local variable index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			break;

		case CFR_BC_lload_0:
		case CFR_BC_lload_1:
		case CFR_BC_lload_2:
		case CFR_BC_lload_3:
			index = bc - CFR_BC_lload_0;
			if ((index + 1) >= maxLocals) {
				errorType = J9NLS_CFR_ERR_BC_LOAD_INDEX__ID;
				/* Jazz 82615: Set the local variable index to show up in the error message framework */
				errorDataIndex = index + 1;
				goto _verifyError;
			}
			break;

		case CFR_BC_fload_0:
		case CFR_BC_fload_1:
		case CFR_BC_fload_2:
		case CFR_BC_fload_3:
			index = bc - CFR_BC_fload_0;
			if (index >= maxLocals) {
				errorType = J9NLS_CFR_ERR_BC_LOAD_INDEX__ID;
				/* Jazz 82615: Set the local variable index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			break;

		case CFR_BC_dload_0:
		case CFR_BC_dload_1:
		case CFR_BC_dload_2:
		case CFR_BC_dload_3:
			index = bc - CFR_BC_dload_0;
			if ((index + 1) >= maxLocals) {
				errorType = J9NLS_CFR_ERR_BC_LOAD_INDEX__ID;
				/* Jazz 82615: Set the local variable index to show up in the error message framework */
				errorDataIndex = index + 1;
				goto _verifyError;
			}
			break;

		case CFR_BC_aload_0:
			if (!maxLocals) {
				errorType = J9NLS_CFR_ERR_BC_LOAD_INDEX__ID;
				/* Jazz 82615: Set the local variable index to show up in the error message framework */
				errorDataIndex = 0;
				goto _verifyError;
			}
			break;

		case CFR_BC_aload_1:
		case CFR_BC_aload_2:
		case CFR_BC_aload_3:
			index = bc - CFR_BC_aload_0;
			if (index >= maxLocals) {
				errorType = J9NLS_CFR_ERR_BC_LOAD_INDEX__ID;
				/* Jazz 82615: Set the local variable index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			break;

		case CFR_BC_iaload:
		case CFR_BC_laload:
		case CFR_BC_faload:
		case CFR_BC_daload:
		case CFR_BC_aaload:
		case CFR_BC_baload:
		case CFR_BC_caload:
		case CFR_BC_saload:
			break;

		case CFR_BC_istore:
		case CFR_BC_fstore:
		case CFR_BC_astore:
			NEXT_U8(index, bcIndex);

			if (index >= maxLocals) {
				errorType = J9NLS_CFR_ERR_BC_STORE_INDEX__ID;
				/* Jazz 82615: Set the local variable index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			break;

		case CFR_BC_lstore:
		case CFR_BC_dstore:
			NEXT_U8(index, bcIndex);

			if ((index + 1) >= maxLocals) {
				errorType = J9NLS_CFR_ERR_BC_STORE_INDEX__ID;
				/* Jazz 82615: Set the local variable index to show up in the error message framework */
				errorDataIndex = index + 1;
				goto _verifyError;
			}
			break;


		case CFR_BC_istore_0:
		case CFR_BC_istore_1:
		case CFR_BC_istore_2:
		case CFR_BC_istore_3:
			index = bc - CFR_BC_istore_0;
			if (index >= maxLocals) {
				errorType = J9NLS_CFR_ERR_BC_STORE_INDEX__ID;
				/* Jazz 82615: Set the local variable index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			break;

		case CFR_BC_lstore_0:
		case CFR_BC_lstore_1:
		case CFR_BC_lstore_2:
		case CFR_BC_lstore_3:
			index = bc - CFR_BC_lstore_0;
			if ((index + 1) >= maxLocals) {
				errorType = J9NLS_CFR_ERR_BC_STORE_INDEX__ID;
				/* Jazz 82615: Set the local variable index to show up in the error message framework */
				errorDataIndex = index + 1;
				goto _verifyError;
			}
			break;

		case CFR_BC_fstore_0:
		case CFR_BC_fstore_1:
		case CFR_BC_fstore_2:
		case CFR_BC_fstore_3:
			index = bc - CFR_BC_fstore_0;
			if (index >= maxLocals) {
				errorType = J9NLS_CFR_ERR_BC_STORE_INDEX__ID;
				/* Jazz 82615: Set the local variable index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			break;

		case CFR_BC_dstore_0:
		case CFR_BC_dstore_1:
		case CFR_BC_dstore_2:
		case CFR_BC_dstore_3:
			index = bc - CFR_BC_dstore_0;
			if ((index + 1) >= maxLocals) {
				errorType = J9NLS_CFR_ERR_BC_STORE_INDEX__ID;
				/* Jazz 82615: Set the local variable index to show up in the error message framework */
				errorDataIndex = index + 1;
				goto _verifyError;
			}
			break;

		case CFR_BC_astore_0:
		case CFR_BC_astore_1:
		case CFR_BC_astore_2:
		case CFR_BC_astore_3:
			index = bc - CFR_BC_astore_0;
			if (index >= maxLocals) {
				errorType = J9NLS_CFR_ERR_BC_STORE_INDEX__ID;
				/* Jazz 82615: Set the local variable index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			break;

		case CFR_BC_iastore:
		case CFR_BC_lastore:
		case CFR_BC_fastore:
		case CFR_BC_dastore:
		case CFR_BC_aastore:
		case CFR_BC_bastore:
		case CFR_BC_castore:
		case CFR_BC_sastore:
			break;

		case CFR_BC_pop:
		case CFR_BC_pop2:
		case CFR_BC_dup:
		case CFR_BC_dup_x1:
		case CFR_BC_dup_x2:
		case CFR_BC_dup2:
		case CFR_BC_dup2_x1:
		case CFR_BC_dup2_x2:
		case CFR_BC_swap:
			break;

		case CFR_BC_iadd:
		case CFR_BC_ladd:
		case CFR_BC_fadd:
		case CFR_BC_dadd:
		case CFR_BC_isub:
		case CFR_BC_lsub:
		case CFR_BC_fsub:
		case CFR_BC_dsub:
		case CFR_BC_imul:
		case CFR_BC_lmul:
		case CFR_BC_fmul:
		case CFR_BC_dmul:
		case CFR_BC_idiv:
		case CFR_BC_ldiv:
		case CFR_BC_fdiv:
		case CFR_BC_ddiv:
		case CFR_BC_irem:
		case CFR_BC_lrem:
		case CFR_BC_frem:
		case CFR_BC_drem:
		case CFR_BC_ineg:
		case CFR_BC_lneg:
		case CFR_BC_fneg:
		case CFR_BC_dneg:
		case CFR_BC_ishl:
		case CFR_BC_lshl:
		case CFR_BC_ishr:
		case CFR_BC_lshr:
		case CFR_BC_iushr:
		case CFR_BC_lushr:
		case CFR_BC_iand:
		case CFR_BC_land:
		case CFR_BC_ior:
		case CFR_BC_lor:
		case CFR_BC_ixor:
		case CFR_BC_lxor:
			break;

		case CFR_BC_iinc:
			NEXT_U8(u1, bcIndex);
			bcIndex++;
			if (u1 >= maxLocals) {
				errorType = J9NLS_CFR_ERR_BC_IINC_INDEX__ID;
				/* Jazz 82615: Set the local variable index to show up in the error message framework */
				errorDataIndex = u1;
				goto _verifyError;
			}
			break;

		case CFR_BC_i2l:
		case CFR_BC_i2f:
		case CFR_BC_i2d:
		case CFR_BC_l2i:
		case CFR_BC_l2f:
		case CFR_BC_l2d:
		case CFR_BC_f2i:
		case CFR_BC_f2l:
		case CFR_BC_f2d:
		case CFR_BC_d2i:
		case CFR_BC_d2l:
		case CFR_BC_d2f:
		case CFR_BC_i2b:
		case CFR_BC_i2c:
		case CFR_BC_i2s:
			break;

		case CFR_BC_lcmp:
		case CFR_BC_fcmpl:
		case CFR_BC_fcmpg:
		case CFR_BC_dcmpl:
		case CFR_BC_dcmpg:
			break;

		case CFR_BC_jsr:
			method->j9Flags |= CFR_J9FLAG_HAS_JSR;
			classfile->j9Flags |= CFR_J9FLAG_HAS_JSR;
			/* fall through */

		case CFR_BC_goto:
		case CFR_BC_ifeq:
		case CFR_BC_ifne:
		case CFR_BC_iflt:
		case CFR_BC_ifge:
		case CFR_BC_ifgt:
		case CFR_BC_ifle:
		case CFR_BC_if_icmpeq:
		case CFR_BC_if_icmpne:
		case CFR_BC_if_icmplt:
		case CFR_BC_if_icmpge:
		case CFR_BC_if_icmpgt:
		case CFR_BC_if_icmple:
		case CFR_BC_if_acmpeq:
		case CFR_BC_if_acmpne:
		case CFR_BC_ifnull:
		case CFR_BC_ifnonnull:
			start = bcIndex - bcStart - 1;
			NEXT_U16(index, bcIndex);
			target = (I_16) index + start;
			if ((U_32) target >= (U_32) length) {
				errorType = J9NLS_CFR_ERR_BC_JUMP_OFFSET__ID;
				goto _verifyError;
			}
			if (!map[target]) {
				errorType = J9NLS_CFR_ERR_BC_JUMP_TARGET__ID;
				goto _verifyError;
			}
			break;

		case CFR_BC_ret:
			*hasRET = 1;
			method->j9Flags |= CFR_J9FLAG_HAS_JSR;
			classfile->j9Flags |= CFR_J9FLAG_HAS_JSR;
			NEXT_U8(index, bcIndex);
			if (index >= maxLocals) {
				errorType = J9NLS_CFR_ERR_BC_RET_INDEX__ID;
				goto _verifyError;
			}
			break;

		case CFR_BC_tableswitch:
			start = bcIndex - bcStart - 1;
			i1 = 3 - (start & 3);
			bcIndex += i1;
			CHECK_END;
			bcIndex -= i1;
			for (i2 = 0; i2 < i1; i2++) {
				NEXT_U8(index, bcIndex);
				if ((0 != index)
					&& (flags & CFR_Xfuture)
					&& (classfile->majorVersion < CFR_MAJOR_VERSION_REQUIRING_STACKMAPS)
				) {
					errorType = J9NLS_CFR_ERR_BC_SWITCH_PAD__ID;
					goto _verifyError;
				}
			}

			bcIndex += 12;
			CHECK_END;
			bcIndex -= 12;
			NEXT_U32(u1, bcIndex);
			target = start + (I_32) u1;
			if ((UDATA) target >= length) {
				errorType = J9NLS_CFR_ERR_BC_SWITCH_OFFSET__ID;
				goto _verifyError;
			}
			if (!map[target]) {
				errorType = J9NLS_CFR_ERR_BC_SWITCH_TARGET__ID;
				goto _verifyError;
			}
			i1 = (I_32) NEXT_U32(u1, bcIndex);
			i2 = (I_32) NEXT_U32(u2, bcIndex);
			tableSize = i2 - i1 + 1;
			if (i1 > i2 || tableSize <= 0 || tableSize >= 16384) {
				errorType = J9NLS_CFR_ERR_BC_SWITCH_EMPTY__ID;
				goto _verifyError;
			}

			bcIndex += tableSize * 4;
			CHECK_END;
			bcIndex -= tableSize * 4;

			/* Count the entries */
			i2 = tableSize;
			for (i1 = 0; i1 < i2; i1++) {
				NEXT_U32(u1, bcIndex);
				target = start + (I_32) u1;
				if ((UDATA) target >= length) {
					errorType = J9NLS_CFR_ERR_BC_SWITCH_OFFSET__ID;
					goto _verifyError;
				}
				if (!map[target]) {
					errorType = J9NLS_CFR_ERR_BC_SWITCH_TARGET__ID;
					goto _verifyError;
				}
			}
			break;

		case CFR_BC_lookupswitch:
			start = bcIndex - bcStart - 1;
			i1 = 3 - (start & 3);
			bcIndex += i1;
			CHECK_END;
			bcIndex -= i1;
			for (i2 = 0; i2 < i1; i2++) {
				NEXT_U8(index, bcIndex);
				if ((0 != index)
					&& (flags & CFR_Xfuture)
					&& (classfile->majorVersion < CFR_MAJOR_VERSION_REQUIRING_STACKMAPS)
				) {
					errorType = J9NLS_CFR_ERR_BC_SWITCH_PAD__ID;
					goto _verifyError;
				}
			}

			bcIndex += 8;
			CHECK_END;
			bcIndex -= 8;
			NEXT_U32(u1, bcIndex);
			target = start + (I_32) u1;
			if ((UDATA) target >= length) {
				errorType = J9NLS_CFR_ERR_BC_SWITCH_OFFSET__ID;
				goto _verifyError;
			}
			if (!map[target]) {
				errorType = J9NLS_CFR_ERR_BC_SWITCH_TARGET__ID;
				goto _verifyError;
			}
			NEXT_U32(u1, bcIndex);

			bcIndex += (I_32) u1 * 8;
			CHECK_END;
			bcIndex -= (I_32) u1 * 8;

			firstKey = TRUE;
			for (i = 0; i < u1; i++) {
				i1 = (IDATA) NEXT_U32(u2, bcIndex);
				if (!firstKey) {
					if ((I_32)i1 <= (I_32)i2) {
						errorType = J9NLS_CFR_ERR_BC_SWITCH_NOT_SORTED__ID;
						goto _verifyError;
					}
				}
				firstKey = FALSE;
				i2 = i1;
				target = start + (I_32) NEXT_U32(u2, bcIndex);
				if ((UDATA) target >= length) {
					errorType = J9NLS_CFR_ERR_BC_SWITCH_OFFSET__ID;
					goto _verifyError;
				}
				if (!map[target]) {
					errorType = J9NLS_CFR_ERR_BC_SWITCH_TARGET__ID;
					goto _verifyError;
				}
			}
			break;

		case CFR_BC_ireturn:
			switch (sigChar) {
			case 'B':
			case 'C':
			case 'I':
			case 'S':
			case 'Z':
				break;
			default:
				/* fail, modify the bytecode to be incompatible in the second pass of verification */
				if (sigChar != 'V') {
					*(bcIndex - 1) = CFR_BC_return;
				}
			}
			break;

		case CFR_BC_lreturn:
			if (sigChar != 'J') {
				/* fail, modify the bytecode to be incompatible in the second pass of verification */
				if (sigChar != 'V') {
					*(bcIndex - 1) = CFR_BC_return;
				}
			}
			break;

		case CFR_BC_freturn:
			if (sigChar != 'F') {
				/* fail, modify the bytecode to be incompatible in the second pass of verification */
				if (sigChar != 'V') {
					*(bcIndex - 1) = CFR_BC_return;
				}
			}
			break;

		case CFR_BC_dreturn:
			if (sigChar != 'D') {
				/* fail, modify the bytecode to be incompatible in the second pass of verification */
				if (sigChar != 'V') {
					*(bcIndex - 1) = CFR_BC_return;
				}
			}
			break;

		case CFR_BC_areturn:
			if ((sigChar != 'L') && (sigChar != '[')) {
				/* fail, modify the bytecode to be incompatible in the second pass of verification */
				if (sigChar != 'V') {
					*(bcIndex - 1) = CFR_BC_return;
				}
			}
			break;

		case CFR_BC_return:
/* do not modify the bytecode at all, if the return matches we're good, if it does not match it will fail as expected
			if (sigChar != 'V') {
				 fail, modify the bytecode to be incompatible in the second pass of verification
					 *(bcIndex - 1) = CFR_BC_return;	 leaving it alone will cause this to fail
			}
*/
			break;

		case CFR_BC_getstatic:
		case CFR_BC_putstatic:
		case CFR_BC_getfield:
		case CFR_BC_putfield:
			NEXT_U16(index, bcIndex);
			if ((!index) || (index >= cpCount)) {
				errorType = J9NLS_CFR_ERR_BAD_INDEX__ID;
				/* Jazz 82615: Set the constant pool index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			info = &(classfile->constantPool[index]);
			if (info->tag != CFR_CONSTANT_Fieldref) {
				errorType = J9NLS_CFR_ERR_BC_NOT_FIELDREF__ID;
				/* Jazz 82615: Set the constant pool index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			/* field signature has been already been checked by j9bcv_verifyClassStructure() */
			break;

		case CFR_BC_invokespecial:
		case CFR_BC_invokevirtual:
		case CFR_BC_invokestatic:
			/* Implicitly includes invokehandle & invokehandlegeneric bytecodes
			 * as they haven't been split from invokevirtual yet */
			NEXT_U16(index, bcIndex);
			if ((!index) || (index >= cpCount)) {
				errorType = J9NLS_CFR_ERR_BAD_INDEX__ID;
				/* Jazz 82615: Set the constant pool index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			info = &(classfile->constantPool[index]);
			if (info->tag != CFR_CONSTANT_Methodref) {
				if (((flags & BCT_MajorClassFileVersionMask) >= BCT_Java8MajorVersionShifted)
				&& (bc != CFR_BC_invokevirtual) && (info->tag == CFR_CONSTANT_InterfaceMethodref)
				) {
					/* JVMS 4.9.1 Static Contraints:
					 * The indexbyte operands of each invokespecial and invokestatic instruction must represent
					 * a valid index into the constant_pool table. The constant pool entry referenced by that
					 * index must be either of type CONSTANT_Methodref or of type CONSTANT_InterfaceMethodref.
					 */
					/* Valid - take no action */
				} else {
					errorType = J9NLS_CFR_ERR_BC_NOT_METHODREF__ID;
					/* Jazz 82615: Set the constant pool index to show up in the error message framework */
					errorDataIndex = index;
					goto _verifyError;
				}
			}
			index = info->slot2;
			info = &(classfile->constantPool[classfile->constantPool[index].slot1]);
			if (info->bytes[0] == '<') {
				if ((bc != CFR_BC_invokespecial)
						||(info->tag != CFR_CONSTANT_Utf8)
						|| (strncmp((char *) info->bytes, "<init>", 6))) {
					errorType = J9NLS_CFR_ERR_BC_METHOD_INVALID__ID;
					goto _verifyError;
				}
			}
			/* Check for '<init> returns V' already done in j9bcv_verifyClassStructure */
			/* Check for 'argCount <= 255' already done in j9bcv_verifyClassStructure */
			break;

		case CFR_BC_invokeinterface:
		{
			IDATA argCount = 0;

			NEXT_U16(index, bcIndex);
			if ((!index) || (index >= cpCount)) {
				errorType = J9NLS_CFR_ERR_BAD_INDEX__ID;
				/* Jazz 82615: Set the constant pool index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			info = &(classfile->constantPool[index]);
			if (info->tag != CFR_CONSTANT_InterfaceMethodref) {
				errorType = J9NLS_CFR_ERR_BC_NOT_INTERFACEMETHODREF__ID;
				/* Jazz 82615: Set the constant pool index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			info = &(classfile->constantPool[info->slot2]);
			if (classfile->constantPool[info->slot1].bytes[0] == '<') {
				errorType = J9NLS_CFR_ERR_BC_METHOD_INVALID__ID;
				goto _verifyError;
			}
			NEXT_U8(index, bcIndex);
			if ((argCount = j9bcv_checkMethodSignature(&(classfile->constantPool[info->slot2]), TRUE)) == -1) {
				errorType = J9NLS_CFR_ERR_BC_METHOD_INVALID_SIG__ID;
				goto _verifyError;
			}
			/* Check for 'argCount <= 255' when checking CFR_CONSTANT_*Methodref */
			if (argCount != ((IDATA) index - 1)) {
				errorType = J9NLS_CFR_ERR_BC_METHOD_NARGS__ID;
				goto _verifyError;
			}
			NEXT_U8(index, bcIndex);
			if (index) {
				errorType = J9NLS_CFR_ERR_BC_METHOD_RESERVED__ID;
				goto _verifyError;
			}
			break;
		}

		case CFR_BC_invokedynamic:
			NEXT_U16(index, bcIndex);
			if ((0 == index) || (index >= cpCount)) {
				errorType = J9NLS_CFR_ERR_BAD_INDEX__ID;
				/* Jazz 82615: Set the constant pool index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			info = &(classfile->constantPool[index]);
			if (info->tag != CFR_CONSTANT_InvokeDynamic) {
				errorType = J9NLS_CFR_ERR_BC_NOT_INVOKEDYNAMIC__ID;
				/* Jazz 82615: Set the constant pool index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			info = &(classfile->constantPool[info->slot2]);
			if (classfile->constantPool[info->slot1].bytes[0] == '<') {
				errorType = J9NLS_CFR_ERR_BC_METHOD_INVALID__ID;
				goto _verifyError;
			}
			/* TODO what other verification is required? */
			NEXT_U16(index, bcIndex);
			if (0 != index) {
				errorType = J9NLS_CFR_ERR_BC_INVOKEDYNAMIC_RESERVED__ID;
				goto _verifyError;
			}
			break;

		case CFR_BC_new:
			NEXT_U16(index, bcIndex);
			if ((!index) || (index >= cpCount)) {
				errorType = J9NLS_CFR_ERR_BAD_INDEX__ID;
				/* Jazz 82615: Set the constant pool index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			info = &(classfile->constantPool[index]);
			if (info->tag != CFR_CONSTANT_Class) {
				errorType = J9NLS_CFR_ERR_BC_NEW_NOT_CLASS__ID;
				/* Jazz 82615: Set the constant pool index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			info = &(classfile->constantPool[info->slot1]);
			if (info->bytes[0] == '[') {
				errorType = J9NLS_CFR_ERR_BC_NEW_ARRAY__ID;
				goto _verifyError;
			}
			break;

		case CFR_BC_newarray:
			NEXT_U8(index, bcIndex);
			if ((index < 4) || (index > 11)) {
				errorType = J9NLS_CFR_ERR_BC_NEWARRAY_TYPE__ID;
				goto _verifyError;
			}
			break;

		case CFR_BC_anewarray:
			NEXT_U16(index, bcIndex);
			if ((!index) || (index >= cpCount)) {
				errorType = J9NLS_CFR_ERR_BAD_INDEX__ID;
				/* Jazz 82615: Set the constant pool index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			info = &(classfile->constantPool[index]);
			if (info->tag != CFR_CONSTANT_Class) {
				errorType = J9NLS_CFR_ERR_BC_ANEWARRAY_NOT_CLASS__ID;
				/* Jazz 82615: Set the constant pool index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			break;

		case CFR_BC_arraylength:
			break;

		case CFR_BC_athrow:
			break;

		case CFR_BC_checkcast:
			NEXT_U16(index, bcIndex);
			if ((!index) || (index >= cpCount)) {
				errorType = J9NLS_CFR_ERR_BAD_INDEX__ID;
				/* Jazz 82615: Set the constant pool index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			info = &(classfile->constantPool[index]);
			if (info->tag != CFR_CONSTANT_Class) {
				errorType = J9NLS_CFR_ERR_BC_CHECKCAST_NOT_CLASS__ID;
				/* Jazz 82615: Set the constant pool index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			break;

		case CFR_BC_instanceof:
			NEXT_U16(index, bcIndex);
			if ((!index) || (index >= cpCount)) {
				errorType = J9NLS_CFR_ERR_BAD_INDEX__ID;
				/* Jazz 82615: Set the constant pool index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			info = &(classfile->constantPool[index]);
			if (info->tag != CFR_CONSTANT_Class) {
				errorType = J9NLS_CFR_ERR_BC_INSTANCEOF_NOT_CLASS__ID;
				/* Jazz 82615: Set the constant pool index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			break;

		case CFR_BC_monitorenter:
		case CFR_BC_monitorexit:
			break;

		case CFR_BC_wide:
			NEXT_U8(bc, bcIndex);
			switch (bc) {

			case CFR_BC_iload:
			case CFR_BC_fload:
			case CFR_BC_aload:
				NEXT_U16(index, bcIndex);

				if (index >= maxLocals) {
					errorType = J9NLS_CFR_ERR_BC_LOAD_INDEX__ID;
					/* Jazz 82615: Set the local variable index to show up in the error message framework */
					errorDataIndex = index;
					goto _verifyError;
				}
				break;

			case CFR_BC_lload:
			case CFR_BC_dload:
				NEXT_U16(index, bcIndex);

				if ((index + 1) >= maxLocals) {
					errorType = J9NLS_CFR_ERR_BC_LOAD_INDEX__ID;
					/* Jazz 82615: Set the local variable index to show up in the error message framework */
					errorDataIndex = index + 1;
					goto _verifyError;
				}
				break;

			case CFR_BC_istore:
			case CFR_BC_fstore:
			case CFR_BC_astore:
				NEXT_U16(index, bcIndex);

				if (index >= maxLocals) {
					errorType = J9NLS_CFR_ERR_BC_STORE_INDEX__ID;
					/* Jazz 82615: Set the local variable index to show up in the error message framework */
					errorDataIndex = index;
					goto _verifyError;
				}
				break;

			case CFR_BC_lstore:
			case CFR_BC_dstore:
				NEXT_U16(index, bcIndex);

				if ((index + 1) >= maxLocals) {
					errorType = J9NLS_CFR_ERR_BC_STORE_INDEX__ID;
					/* Jazz 82615: Set the local variable index to show up in the error message framework */
					errorDataIndex = index + 1;
					goto _verifyError;
				}
				break;

			case CFR_BC_iinc:
				CHECK_END;
				NEXT_U16(u1, bcIndex);
				bcIndex+=2;
				if (u1 >= maxLocals) {
					errorType = J9NLS_CFR_ERR_BC_IINC_INDEX__ID;
					goto _verifyError;
				}
				break;

			case CFR_BC_ret:
				*hasRET = 1;
				method->j9Flags |= CFR_J9FLAG_HAS_JSR;
				classfile->j9Flags |= CFR_J9FLAG_HAS_JSR;
				NEXT_U16(index, bcIndex);
				if (index >= maxLocals) {
					errorType = J9NLS_CFR_ERR_BC_RET_INDEX__ID;
					goto _verifyError;
				}
				break;

			default:
				errorType = J9NLS_CFR_ERR_BC_NOT_WIDE__ID;
				goto _verifyError;
			}
			break;

		case CFR_BC_multianewarray:
			NEXT_U16(index, bcIndex);
			NEXT_U8(u1, bcIndex);
			if ((!index) || (index >= cpCount)) {
				errorType = J9NLS_CFR_ERR_BAD_INDEX__ID;
				/* Jazz 82615: Set the constant pool index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			info = &(classfile->constantPool[index]);
			if (info->tag != CFR_CONSTANT_Class) {
				errorType = J9NLS_CFR_ERR_BC_MULTI_NOT_CLASS__ID;
				/* Jazz 82615: Set the constant pool index to show up in the error message framework */
				errorDataIndex = index;
				goto _verifyError;
			}
			info = &(classfile->constantPool[info->slot1]);
			u2 = 0;
			while (info->bytes[u2] == '[')
				u2++;
			if (!u2) {
				errorType = J9NLS_CFR_ERR_BC_MULTI_NOT_ARRAY__ID;
				goto _verifyError;
			}
			if ((!u1) || (u1 > u2)) {
				errorType = J9NLS_CFR_ERR_BC_MULTI_DIMS__ID;
				goto _verifyError;
			}
			break;

		case CFR_BC_jsr_w:
			method->j9Flags |= CFR_J9FLAG_HAS_JSR;
			classfile->j9Flags |= CFR_J9FLAG_HAS_JSR;
			/* fall through */

		case CFR_BC_goto_w:
			start = bcIndex - bcStart - 1;
			NEXT_U32(index, bcIndex);
			target = (I_32) index + start;
			if ((UDATA) target >= length) {
				errorType = J9NLS_CFR_ERR_BC_JUMP_OFFSET__ID;
				goto _verifyError;
			}
			if (!map[target]) {
				errorType = J9NLS_CFR_ERR_BC_JUMP_TARGET__ID;
				goto _verifyError;
			}
			break;

		default:
			errorType = J9NLS_CFR_ERR_BC_UNKNOWN__ID;
			goto _verifyError;
		}
	}

	CHECK_END;

	result = 0;
	goto _leaveProc;			/* All is well */

  _verifyError:
	start = bcInstructionStart - bcStart;
	Trc_STV_checkBytecodeStructure_VerifyError(errorType, methodIndex, start);
	buildMethodErrorWithExceptionDetails(error,
		errorType,
		0,
		CFR_ThrowVerifyError,
		(I_32)methodIndex,
		(U_32)start,
		method,
		classfile->constantPool,
		(U_32)errorDataIndex,
		-1,
		0);
	result = -1;

  _leaveProc:
	return result;

#undef CHECK_END
}



/**
 * Check whether any verification error occurs when walking through the stack map
 * @param classfile - pointer to J9CfrClassFile
 * @param method - pointer to J9CfrMethod
 * @param code - pointer to J9CfrAttributeCode
 * @param map - pointer to the stack map of the specified method
 * @param flags - settings in verification
 * @param exceptionDetails - pointer to StackmapExceptionDetails
 * @return 0 on success; otherwise, return a specific error code.
 */
static IDATA
checkStackMap (J9CfrClassFile* classfile, J9CfrMethod * method, J9CfrAttributeCode * code, U_8 * map, UDATA flags, StackmapExceptionDetails* exceptionDetails)
{
	UDATA i;
	IDATA errorCode = 0;
	/* delayedErrorCode only used to hold the "stack map not at a bytecode boundary" error while completing other checks */
	/* It is a verification retry error */
	IDATA delayedErrorCode = 0;
	J9CfrAttribute* attribute;

	for (i = 0; i < code->attributesCount; i++) {
		attribute = code->attributes[i];

		if (attribute->tag == CFR_ATTRIBUTE_StackMapTable){
			J9CfrAttributeStackMap * stackMap = (J9CfrAttributeStackMap *) attribute;
			U_8* entries = stackMap->entries;
			UDATA length = (UDATA) stackMap->mapLength;
			U_8* end = entries + length;
			UDATA j = 0;
			UDATA offset = (UDATA) -1;
			IDATA localSlots;
			J9CfrConstantPoolInfo* info = &classfile->constantPool[method->descriptorIndex];

			localSlots = j9bcv_checkMethodSignature(info, TRUE);
			if ((method->accessFlags & CFR_ACC_STATIC) == 0) {
				localSlots++;
			}

			for (j = 0; j < stackMap->numberOfEntries; j++) {
				U_8 frameType;
				UDATA delta;
				IDATA slotCount;

				if ((entries + 1) > end) {
					errorCode = FATAL_CLASS_FORMAT_ERROR;
					goto _failedCheck;
				}
				frameType = *entries++;

				if (frameType < CFR_STACKMAP_SAME_LOCALS_1_STACK) {
					offset += (UDATA) frameType;
				} else if (frameType < CFR_STACKMAP_SAME_LOCALS_1_STACK_END) {
					offset += (UDATA) frameType - (UDATA) CFR_STACKMAP_SAME_LOCALS_1_STACK;
				} else if (frameType >= CFR_STACKMAP_SAME_LOCALS_1_STACK_EXTENDED) {
					if ((entries + 2) > end) {
						errorCode = FATAL_CLASS_FORMAT_ERROR;
						goto _failedCheck;
					}
					offset += NEXT_U16(delta, entries);
				} else {
					/* illegal frame type */
					errorCode = FATAL_CLASS_FORMAT_ERROR;
					goto _failedCheck;
				}
				offset++;

				if (offset < code->codeLength) {
					/* Check only possibly valid offsets */
					if (map[offset] == 0) {
						/* invalid bytecode index */
						/* Don't go to _failedCheck - delay reporting the VerifyError */
						/* continue parsing the attribute to check for other failures first */
						/* particularly extra or missing bytes */
						delayedErrorCode = FALLBACK_VERIFY_ERROR;
						exceptionDetails->stackmapFrameIndex = (I_32)j;
						exceptionDetails->stackmapFrameBCI = (U_32)offset;
					}
				} else {
					errorCode = FALLBACK_VERIFY_ERROR;
					exceptionDetails->stackmapFrameIndex = (I_32)j;
					exceptionDetails->stackmapFrameBCI = (U_32)offset;
					goto _failedCheck;
				}

				if (entries > end) {
					errorCode = FATAL_CLASS_FORMAT_ERROR;
					goto _failedCheck;
				}

				if (frameType != CFR_STACKMAP_FULL) {
					/* Only executed with StackMapTable */
					slotCount = 0;
					if ((frameType >= CFR_STACKMAP_SAME_LOCALS_1_STACK) && (frameType <= CFR_STACKMAP_SAME_LOCALS_1_STACK_EXTENDED)) {
						slotCount = 1;
					}
					if (frameType >= CFR_STACKMAP_CHOP_3) {
						slotCount = (IDATA) frameType - CFR_STACKMAP_APPEND_BASE;
						localSlots += slotCount;
						if (slotCount < 0) {
							slotCount = 0;
						}
					}
			
				} else {
					/* full frame */
					/* Executed with StackMap or StackMapTable */
					NEXT_U16(slotCount, entries);
					localSlots = slotCount;
					if (errorCode = checkStackMapEntries (classfile, code, map, &entries, slotCount, end)) {
						goto _failedCheck;
					}
					NEXT_U16(slotCount, entries);
				}

				if (errorCode = checkStackMapEntries (classfile, code, map, &entries, slotCount, end)) {
					goto _failedCheck;
				}
			}

			if (entries != end) {
				/* extra bytes in the attribute */
				errorCode = FATAL_CLASS_FORMAT_ERROR;
				goto _failedCheck;
			}
		}
	}
_failedCheck:
	if (delayedErrorCode || (errorCode == FALLBACK_CLASS_FORMAT_ERROR) || (errorCode == FALLBACK_VERIFY_ERROR)) {
		if ((classfile->majorVersion < CFR_MAJOR_VERSION_REQUIRING_STACKMAPS) && ((flags & J9_VERIFY_NO_FALLBACK) == 0)) {
			/* Hide the bad StackMap/StackMapTable attribute and error.  Major version 51 and greater do not allow fallback to type inference */
			attribute->tag = CFR_ATTRIBUTE_Unknown;
			errorCode = 0; 	
			delayedErrorCode = 0;
		}
	}

	if (errorCode == 0) {
		errorCode = delayedErrorCode;
	}
	
	return errorCode;
}



static I_32
checkStackMapEntries (J9CfrClassFile* classfile, J9CfrAttributeCode * code, U_8 * map, U_8 ** entries, UDATA slotCount, U_8 * end)
{
	U_8* entry = *entries;
	U_8 entryType;
	U_16 offset;
	U_16 cpIndex;
	J9CfrConstantPoolInfo* cpBase = classfile->constantPool;
	U_32 cpCount = (U_32) classfile->constantPoolCount;

	for (; slotCount; slotCount--) {
		if ((entry + 1) > end) {
			return FATAL_CLASS_FORMAT_ERROR;
		}
		entryType = *entry++;

		if (entry > end) {
			return FATAL_CLASS_FORMAT_ERROR;
		}

		if (entryType > CFR_STACKMAP_TYPE_NEW_OBJECT) {
			/* Unknown entry */
			return FATAL_CLASS_FORMAT_ERROR;
		}

		if (entryType == CFR_STACKMAP_TYPE_NEW_OBJECT) {
			if ((entry + 2) > end) {
				return FATAL_CLASS_FORMAT_ERROR;
			}
			NEXT_U16(offset, entry);

			if (offset >= code->codeLength) {
				return FALLBACK_CLASS_FORMAT_ERROR;
			}

			if (map[offset] == 0) {
				/* invalid bytecode index */
				return FALLBACK_CLASS_FORMAT_ERROR;
			}

			if (code->code[offset] != CFR_BC_new) {
				return FALLBACK_CLASS_FORMAT_ERROR;
			}

		} else if (entryType == CFR_STACKMAP_TYPE_OBJECT) {
			if ((entry + 2) > end) {
				return FATAL_CLASS_FORMAT_ERROR;
			}
			NEXT_U16(cpIndex, entry);
			/* Check index is in range */
			if ((!cpIndex) || (cpIndex > cpCount)) {
				return FATAL_CLASS_FORMAT_ERROR;
			}
			/* Check index points to the right type of thing */
			if(cpBase[cpIndex].tag != CFR_CONSTANT_Class) {
				return FATAL_CLASS_FORMAT_ERROR;
			}
		}
	}

	*entries = entry;
	return 0;
}


/*
	##class format checking

	Check the Method provided and determine the 
	referenced constant pool entries. 

	@results is an array of size classfile->constantPoolCount.
	@instructionMap is a handle to a pointer. If non-null, an instruction
	map will be created at this location.

	Returns 0 on success, -1 on structure problem (error set), -2 on out of memory.
	
*/

static IDATA 
checkMethodStructure (J9PortLibrary * portLib, J9CfrClassFile * classfile, UDATA methodIndex, U_8 * instructionMap, J9CfrError * error, U_32 flags, I_32 *hasRET)
{
	J9CfrAttributeCode *code;
	J9CfrMethod *method;
	J9CfrConstantPoolInfo * info;
	U_8 *map = NULL;
	UDATA i;
	IDATA result;
	UDATA length, pc = 0;
	U_16 errorType;

	PORT_ACCESS_FROM_PORT (portLib);

	method = &(classfile->methods[methodIndex]);
	code = method->codeAttribute;
	if (!code) {
		if ((method->accessFlags & CFR_ACC_NATIVE) || (method->accessFlags & CFR_ACC_ABSTRACT)) {
			result = 0;
			goto _leaveProc;
		} else {
			errorType = J9NLS_CFR_ERR_CODE_MISSING__ID;
			goto _formatError;
		}
	}

	length = code->codeLength;
	if (length == 0) {
		errorType = J9NLS_CFR_ERR_CODE_ARRAY_EMPTY__ID;
		goto _formatError;
	}

	if (instructionMap) {
		map = instructionMap;
	} else {
		map = j9mem_allocate_memory(length, J9MEM_CATEGORY_CLASSES);
		if (map == NULL) {
			result = -2;
			goto _leaveProc;
		}
	}
	memset (map, 0, length);

	if (buildInstructionMap (classfile, code, map, methodIndex, error)) {
		result = -1;
		goto _leaveProc;
	}

	for (i = 0; i < code->exceptionTableLength; i++) {
		pc = code->exceptionTable[i].startPC;
		if (pc >= length) {
			errorType = J9NLS_CFR_ERR_HANDLER_START_PC__ID;
			goto _formatError;
		}
		if (!map[pc]) {
			errorType = J9NLS_CFR_ERR_HANDLER_START_TARGET__ID;
			goto _formatError;
		}
		pc = code->exceptionTable[i].endPC;
		if (pc > length) {
			errorType = J9NLS_CFR_ERR_HANDLER_END_PC__ID;
			goto _formatError;
		}
		if ((!map[pc]) && (pc != length)) {
			errorType = J9NLS_CFR_ERR_HANDLER_END_TARGET__ID;
			goto _formatError;
		}
		if (code->exceptionTable[i].startPC >= code->exceptionTable[i].endPC) {
			errorType = J9NLS_CFR_ERR_HANDLER_RANGE_EMPTY__ID;
			goto _formatError;
		}
		pc = code->exceptionTable[i].handlerPC;
		if (pc >= length) {
			errorType = J9NLS_CFR_ERR_HANDLER_PC__ID;
			goto _formatError;
		}
		if (!map[pc]) {
			errorType = J9NLS_CFR_ERR_HANDLER_TARGET__ID;
			goto _formatError;
		}
	}

	/* Throw a class format error if we are given a static <init> method (otherwise later we will throw a verify error due to back stack shape) */
	info = &(classfile->constantPool[method->nameIndex]);
	if (info->slot1 == 6) {		/* size of '<init>' */
		if ((info->tag == CFR_CONSTANT_Utf8)
				&& (!strncmp ((char *)info->bytes, "<init>", 6))) {
			if (method->accessFlags & CFR_ACC_STATIC) {
				errorType = J9NLS_CFR_ERR_ILLEGAL_METHOD_MODIFIERS__ID;
				goto _formatError;
			}
		}
	}

	result = checkBytecodeStructure (classfile, methodIndex, length, map, error, flags, hasRET);

	if (result) {
		goto _leaveProc;
	}

	if ((flags & J9_VERIFY_IGNORE_STACK_MAPS) == 0) {
		StackmapExceptionDetails exceptionDetails;
		memset(&exceptionDetails, 0, sizeof(exceptionDetails));
		result = checkStackMap(classfile, method, code, map, flags, &exceptionDetails);
		if (result) {
			if (result == FALLBACK_VERIFY_ERROR) {
				errorType = J9NLS_CFR_ERR_INVALID_STACK_MAP_ATTRIBUTE__ID;
				Trc_STV_checkMethodStructure_VerifyError(errorType, methodIndex, pc);
				/* Jazz 82615: Store error data for stackmap frame when verification error occurs */
				buildMethodErrorWithExceptionDetails(error,
					errorType,
					0,
					CFR_ThrowVerifyError,
					(I_32)methodIndex,
					(U_32)pc,
					method,
					classfile->constantPool,
					0,
					exceptionDetails.stackmapFrameIndex,
					exceptionDetails.stackmapFrameBCI);
				result = -1;
				goto _leaveProc;
			} else {
				errorType = J9NLS_CFR_ERR_INVALID_STACK_MAP_ATTRIBUTE__ID;
				goto _formatError;
			}
		}
	}

	/* Should check thrown exceptions.  */

	goto _leaveProc;			/* All is well */

  _formatError:
	Trc_STV_checkMethodStructure_FormatError(errorType, methodIndex);
	buildMethodError(error, errorType, CFR_ThrowClassFormatError, (I_32)methodIndex, (U_32)pc, method, classfile->constantPool);
	result = -1;
	goto _leaveProc;


  _leaveProc:
	if (map && (map != instructionMap)) {
		j9mem_free_memory (map);
	}

	return result;
}



/*
	##class format checking

	Check the Method provided and determine the 
	referenced constant pool entries. 

	Returns 0 on success, -1 on structure problem (error set), -2 on out of memory.
	
*/

IDATA 
j9bcv_verifyClassStructure (J9PortLibrary * portLib, J9CfrClassFile * classfile, U_8 * segment,
										U_8 * segmentLength, U_8 * freePointer, U_32 vmVersionShifted, U_32 flags, I_32 *hasRET)
{
	UDATA i;
	IDATA isInit;
	IDATA result = 0;
	UDATA length = 0;
	U_16 errorType = 0;
	J9CfrConstantPoolInfo *info;
	J9CfrConstantPoolInfo *utf8;
	J9CfrMethod *method;

	Trc_STV_j9bcv_verifyClassStructure_Entry(classfile->constantPool[classfile->constantPool[classfile->thisClass].slot1].slot1,
																						classfile->constantPool[classfile->constantPool[classfile->thisClass].slot1].bytes);

	for (i = 1; i < classfile->constantPoolCount; i++) {
		J9CfrConstantPoolInfo *nameAndSig;
		IDATA arity;
		UDATA end;
		IDATA argCount;

		info = &classfile->constantPool[i];
		switch (info->tag) {
		case CFR_CONSTANT_Class:
			/* Must be a UTF8. */
			utf8 = &classfile->constantPool[info->slot1];
			arity = bcvCheckClassName(utf8);
			if (arity < 0) {
				errorType = J9NLS_CFR_ERR_BAD_CLASS_NAME__ID;
				goto _formatError;
			}

			if (arity > 0) {	/* we have some sort of array */
				if (arity > 255) {
					errorType = J9NLS_CFR_ERR_TOO_MANY_DIMENSIONS__ID;
					goto _formatError;
				}
				end = utf8->slot1;

				switch (utf8->bytes[arity]) {
				case 'L':		/* object array */
					if (utf8->bytes[--end] != ';') {
						errorType = J9NLS_CFR_ERR_BAD_CLASS_NAME__ID;
						goto _formatError;
					}
					break;
				case 'B':		/* base type array */
				case 'C':
				case 'D':
				case 'F':
				case 'I':
				case 'J':
				case 'S':
				case 'Z':
					if (--end != (UDATA) arity) {
						errorType = J9NLS_CFR_ERR_BAD_CLASS_NAME__ID;
						goto _formatError;
					}
					break;
				default:
					errorType = J9NLS_CFR_ERR_BAD_CLASS_NAME__ID;
					goto _formatError;
				}
			}

			break;

		case CFR_CONSTANT_NameAndType:
			if (0 != (flags & CFR_Xfuture)) {
				/* TODO: use the flags field to determine if this entry has been verified already */
				utf8 = &classfile->constantPool[info->slot2]; /* get the descriptor */
				if ((U_8) '(' == (utf8->bytes)[0]) { /* method descriptor */
					if (bcvCheckMethodName(&classfile->constantPool[info->slot1]) < 0) {
						errorType = J9NLS_CFR_ERR_BAD_METHOD_NAME__ID;
						goto _formatError;
					}
					if (j9bcv_checkMethodSignature(&classfile->constantPool[info->slot2], FALSE) < 0) {
						errorType = J9NLS_CFR_ERR_BC_METHOD_INVALID_SIG__ID;
						goto _formatError;
					}
				} else {
					result = j9bcv_checkFieldSignature(utf8, 0);
					if (result < 0) {
						if (result == -1) {
							errorType = J9NLS_CFR_ERR_BC_FIELD_INVALID_SIG__ID;
							goto _formatError;
						} else {
							errorType = J9NLS_CFR_ERR_TOO_MANY_DIMENSIONS__ID;
							goto _formatError;
						}
					} else {
						result = 0;
					}
				}
			}
			break;
		case CFR_CONSTANT_Fieldref:
			nameAndSig = &classfile->constantPool[info->slot2];
			utf8 = &classfile->constantPool[nameAndSig->slot1];
			if (bcvCheckName(utf8)) {
				errorType = J9NLS_CFR_ERR_BAD_FIELD_NAME__ID;
				goto _formatError;
			}
			utf8 = &classfile->constantPool[nameAndSig->slot2];
			if ((result = j9bcv_checkFieldSignature(utf8, 0)) < 0) {
				if (result == -1) {
					errorType = J9NLS_CFR_ERR_BC_FIELD_INVALID_SIG__ID;
					goto _formatError;
				} else {
					errorType = J9NLS_CFR_ERR_TOO_MANY_DIMENSIONS__ID;
					goto _formatError;
				}
			} else {
				result = 0;
			}
			break;

		case CFR_CONSTANT_InvokeDynamic:
			/* No static constraints defined (so far) on slot1 */
		case CFR_CONSTANT_Methodref:
		case CFR_CONSTANT_InterfaceMethodref:
			nameAndSig = &classfile->constantPool[info->slot2];
			utf8 = &classfile->constantPool[nameAndSig->slot1];
			isInit = bcvCheckMethodName(utf8);
			if ((isInit < 0) || ((CFR_METHOD_NAME_CLINIT == isInit) && (CFR_CONSTANT_Methodref == info->tag))) {
				errorType = J9NLS_CFR_ERR_BAD_METHOD_NAME__ID;
				goto _formatError;
			}
			info = &classfile->constantPool[nameAndSig->slot2];
			if ((argCount = j9bcv_checkMethodSignature(info, TRUE)) < 0) {
				errorType = J9NLS_CFR_ERR_BC_METHOD_INVALID_SIG__ID;
				goto _formatError;
			}
			if (isInit) {
				if (info->bytes[info->slot1 - 1] != 'V') {
					errorType = J9NLS_CFR_ERR_BC_METHOD_INVALID_SIG__ID;
					goto _formatError;
				}
			}
			if (argCount > 255) {
				errorType = J9NLS_CFR_ERR_TOO_MANY_ARGS__ID;
				/* TODO: determine if this should be verifyError */
				goto _formatError;
			}
			break;

		case CFR_CONSTANT_MethodType:
			if (j9bcv_checkMethodSignature(&classfile->constantPool[info->slot1], FALSE) < 0) {
				errorType = J9NLS_CFR_ERR_METHODTYPE_INVALID_SIG__ID;
				goto _formatError;
			}
			break;

		case CFR_CONSTANT_MethodHandle:
			/* method kinds: confirm <init> only when REF_newInvokeSpecial  */
			if ((info->slot1 >= MH_REF_INVOKEVIRTUAL) && (info->slot1 <= MH_REF_INVOKEINTERFACE)) {
				J9CfrConstantPoolInfo *methodref = &classfile->constantPool[info->slot2];
				nameAndSig = &classfile->constantPool[methodref->slot2];
				utf8 = &classfile->constantPool[nameAndSig->slot1];
				isInit = bcvIsInitOrClinit(utf8);
				if (CFR_METHOD_NAME_CLINIT == isInit) {
					errorType = J9NLS_CFR_ERR_BAD_METHOD_NAME__ID;
					goto _formatError;
				}
				if (CFR_METHOD_NAME_INIT == isInit) {
					if (info->slot1 != MH_REF_NEWINVOKESPECIAL) {
						errorType = J9NLS_CFR_ERR_BAD_METHOD_NAME__ID;
						goto _formatError;
					}
				}
			}
			break;

		}
	}

	for (i = 0; i < classfile->fieldsCount; i++) {
		J9CfrField *field = &classfile->fields[i];
		info = &classfile->constantPool[field->descriptorIndex];
		utf8 = &classfile->constantPool[field->nameIndex];
		if (j9bcv_checkFieldSignature(info, 0) < 0) {
			errorType = J9NLS_CFR_ERR_BC_FIELD_INVALID_SIG__ID;
			goto _formatError;
		}
		if (bcvCheckName(utf8) < 0) {
			errorType = J9NLS_CFR_ERR_BAD_FIELD_NAME__ID;
			goto _formatError;
		}
	}

	/* walk the classfile methods list */
	for (i = 0; i < classfile->methodsCount; i++) {
		IDATA argCount = 0;

		method = &(classfile->methods[i]);
		utf8 = &classfile->constantPool[method->nameIndex];
		if ((isInit = bcvCheckMethodName(utf8)) < 0) {
			info = utf8;
			errorType = J9NLS_CFR_ERR_BAD_METHOD_NAME__ID;
			goto _formatError;
		}

		info = &classfile->constantPool[method->descriptorIndex];
		argCount = j9bcv_checkMethodSignature(info, TRUE);
		if (argCount < 0) {
			errorType = J9NLS_CFR_ERR_BC_METHOD_INVALID_SIG__ID;
			goto _formatError;
		}

		/* The requirement for taking no arguments was introduced in Java SE 9.
		 * In a class file whose version number is 51.0 or above, the method
		 * has its ACC_STATIC flag set and takes no arguments.
		 */
                 /* Leave this here to find usages of the following check:
                  * if (J2SE_VERSION(vm) >= J2SE_19) {
                  */
		if (vmVersionShifted >= BCT_Java9MajorVersionShifted) {
			if (classfile->majorVersion >= 51) {
				if ((CFR_METHOD_NAME_CLINIT == isInit) 
					&& (0 != argCount) 
				) {
					errorType = J9NLS_CFR_ERR_CLINIT_ILLEGAL_SIGNATURE__ID;
					goto _formatError;
				}
			}
		}

		if (0 == (method->accessFlags & CFR_ACC_STATIC)) {
			argCount++;
		}
		if (argCount > 255) {
			errorType = J9NLS_CFR_ERR_TOO_MANY_ARGS__ID;
			goto _formatError;
		}
		if (method->codeAttribute) {
			if (argCount > (I_32) method->codeAttribute->maxLocals) {
				errorType = J9NLS_CFR_ERR_MAX_LOCALS__ID;
				goto _formatError;
			}
		}
		if (isInit) {
			if (info->bytes[info->slot1 - 1] != 'V') {
				Trc_STV_j9bcv_verifyClassStructure_MethodError(J9NLS_CFR_ERR_BC_METHOD_INVALID_SIG__ID, i);
				buildMethodError((J9CfrError *)segment, errorType, CFR_ThrowClassFormatError, (I_32) i, 0, method, classfile->constantPool);
				result = -1;
				goto _leaveProc;
			}
		}

		if (method->codeAttribute) {
			length = method->codeAttribute->codeLength;
		}

		if ((freePointer + length) < segmentLength) {
			result = checkMethodStructure(portLib, classfile, i, freePointer, (J9CfrError *) segment, flags, hasRET);
		} else {				
			/* We should probably skip this -- since if we don't have enough memory, we should just bail */
			result = checkMethodStructure(portLib, classfile, i, NULL, (J9CfrError *) segment, flags, hasRET);
		}

		if (result) {
			goto _leaveProc;	/* Fail */
		}
	}

	goto _leaveProc;			/* All is well */

  _formatError:
	Trc_STV_j9bcv_verifyClassStructure_ClassError(errorType, info->romAddress);
	buildError((J9CfrError *)segment, errorType, CFR_ThrowClassFormatError, info->romAddress);
	result = -1;

  _leaveProc:
	Trc_STV_j9bcv_verifyClassStructure_Exit();
	return result;
}
