/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

#include "j9.h"

#include "codegen/ARM64Instruction.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/J9ARM64Snippet.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/SnippetGCMap.hpp"
#include "runtime/CodeCacheManager.hpp"

TR::ARM64MonitorEnterSnippet::ARM64MonitorEnterSnippet(
   TR::CodeGenerator *codeGen,
   TR::Node *monitorNode,
   TR::LabelSymbol *incLabel,
   TR::LabelSymbol *callLabel,
   TR::LabelSymbol *restartLabel)
   : _incLabel(incLabel),
     TR::ARM64HelperCallSnippet(codeGen, monitorNode, callLabel, monitorNode->getSymbolReference(), restartLabel)
   {
   // Helper call, preserves all registers
   incLabel->setSnippet(this);
   gcMap().setGCRegisterMask(0xFFFFFFFF);
   }

uint8_t *
TR::ARM64MonitorEnterSnippet::emitSnippetBody()
   {
   // The AArch64 code for the snippet looks like:
   //
   // incLabel:
   //    and     tempReg, dataReg, ~(OBJECT_HEADER_LOCK_BITS_MASK - OBJECT_HEADER_LOCK_LAST_RECURSION_BIT)
   //    cmp     metaReg, tempReg
   //    bne     callLabel
   //    add     dataReg, dataReg, LOCK_INC_DEC_VALUE
   //    str     dataReg, [addrReg]
   //    b       restartLabel
   // callLabel:
   //    bl      jitMonitorEntry
   //    b       restartLabel

   TR::RegisterDependencyConditions *deps = getRestartLabel()->getInstruction()->getDependencyConditions();

   TR::RealRegister *metaReg = cg()->getMethodMetaDataRegister();
   TR::RealRegister *dataReg = cg()->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(1)->getRealRegister());
   TR::RealRegister *addrReg = cg()->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(2)->getRealRegister());
   TR::RealRegister *tempReg = cg()->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(3)->getRealRegister());
   TR::RealRegister *zeroReg = cg()->machine()->getRealRegister(TR::RealRegister::xzr);

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());
   TR::InstOpCode::Mnemonic op;

   uint8_t *buffer = cg()->getBinaryBufferCursor();

   _incLabel->setCodeLocation(buffer);

   *(int32_t *)buffer = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::andimmx);
   tempReg->setRegisterFieldRD((uint32_t *)buffer);
   dataReg->setRegisterFieldRN((uint32_t *)buffer);
   // OBJECT_HEADER_LOCK_BITS_MASK is 0xFF
   // OBJECT_HEADER_LOCK_RECURSION_BIT is 0x80
   // (OBJECT_HEADER_LOCK_BITS_MASK - OBJECT_HEADER_LOCK_LAST_RECURSION_BIT) is 0x7F
   *(int32_t *)buffer |= 0x79E000; // immr=57, imms=56 for 0xFFFFFFFFFFFFFF80
   buffer += ARM64_INSTRUCTION_LENGTH;

   *(int32_t *)buffer = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::subsx); // for cmp
   metaReg->setRegisterFieldRN((uint32_t *)buffer);
   tempReg->setRegisterFieldRM((uint32_t *)buffer);
   zeroReg->setRegisterFieldRD((uint32_t *)buffer);
   buffer += ARM64_INSTRUCTION_LENGTH;

   *(int32_t *)buffer = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::b_cond);
   *(int32_t *)buffer |= ((4 << 5) | TR::CC_NE); // 4 instructions forward, bne
   buffer += ARM64_INSTRUCTION_LENGTH;

   *(int32_t *)buffer = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::addimmx);
   dataReg->setRegisterFieldRD((uint32_t *)buffer);
   dataReg->setRegisterFieldRN((uint32_t *)buffer);
   *(int32_t *)buffer |= ((LOCK_INC_DEC_VALUE & 0xFFF) << 10); // imm12
   buffer += ARM64_INSTRUCTION_LENGTH;

   op = fej9->generateCompressedLockWord() ? TR::InstOpCode::strimmw : TR::InstOpCode::strimmx;
   *(int32_t *)buffer = TR::InstOpCode::getOpCodeBinaryEncoding(op);
   dataReg->setRegisterFieldRT((uint32_t *)buffer);
   addrReg->setRegisterFieldRN((uint32_t *)buffer);
   // offset 0 -- no need to encode
   buffer += ARM64_INSTRUCTION_LENGTH;

   *(int32_t *)buffer = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::b);
   intptr_t destination = (intptr_t)getRestartLabel()->getCodeLocation();
   TR_ASSERT(!cg()->directCallRequiresTrampoline(destination, (intptr_t)buffer), "Jump target too far away.");
   intptr_t distance = (intptr_t)destination - (intptr_t)buffer;
   *(int32_t *)buffer |= ((distance >> 2) & 0x3FFFFFF); // imm26
   buffer += ARM64_INSTRUCTION_LENGTH;

   cg()->setBinaryBufferCursor(buffer);
   buffer = TR::ARM64HelperCallSnippet::emitSnippetBody(); // x0 holds the object

   return buffer;
   }

void
TR::ARM64MonitorEnterSnippet::print(TR::FILE *pOutFile, TR_Debug *debug)
   {
   uint8_t *cursor = getIncLabel()->getCodeLocation();

   debug->printSnippetLabel(pOutFile, getIncLabel(), cursor, "Inc Monitor Counter");
   }

uint32_t TR::ARM64MonitorEnterSnippet::getLength(int32_t estimatedSnippetStart)
   {
   int32_t len = 6 * ARM64_INSTRUCTION_LENGTH;
   return len + TR::ARM64HelperCallSnippet::getLength(estimatedSnippetStart+len);
   }

int32_t TR::ARM64MonitorEnterSnippet::setEstimatedCodeLocation(int32_t estimatedSnippetStart)
   {
   _incLabel->setEstimatedCodeLocation(estimatedSnippetStart);
   getSnippetLabel()->setEstimatedCodeLocation(estimatedSnippetStart + 6 * ARM64_INSTRUCTION_LENGTH);
   return estimatedSnippetStart;
   }


TR::ARM64MonitorExitSnippet::ARM64MonitorExitSnippet(
   TR::CodeGenerator *codeGen,
   TR::Node *monitorNode,
   TR::LabelSymbol *decLabel,
   TR::LabelSymbol *callLabel,
   TR::LabelSymbol *restartLabel)
   : _decLabel(decLabel),
     TR::ARM64HelperCallSnippet(codeGen, monitorNode, callLabel, monitorNode->getSymbolReference(), restartLabel)
   {
   // Helper call, preserves all registers
   decLabel->setSnippet(this);
   gcMap().setGCRegisterMask(0xFFFFFFFF);
   }

uint8_t *
TR::ARM64MonitorExitSnippet::emitSnippetBody()
   {
   // The AArch64 code for the snippet looks like:
   //
   // decLabel:
   //    and     tempReg, dataReg, ~OBJECT_HEADER_LOCK_RECURSION_MASK
   //    cmp     metaReg, tempReg
   //    bne     callLabel
   //    sub     dataReg, dataReg, LOCK_INC_DEC_VALUE
   //    str     dataReg, [addrReg]
   //    b       restartLabel
   // callLabel:
   //    bl      jitMonitorExit
   //    b       restartLabel

   TR::RegisterDependencyConditions *deps = getRestartLabel()->getInstruction()->getDependencyConditions();

   TR::RealRegister *metaReg = cg()->getMethodMetaDataRegister();
   TR::RealRegister *dataReg = cg()->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(1)->getRealRegister());
   TR::RealRegister *addrReg = cg()->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(2)->getRealRegister());
   TR::RealRegister *tempReg = cg()->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(3)->getRealRegister());
   TR::RealRegister *zeroReg = cg()->machine()->getRealRegister(TR::RealRegister::xzr);

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());
   TR::InstOpCode::Mnemonic op;

   uint8_t *buffer = cg()->getBinaryBufferCursor();

   _decLabel->setCodeLocation(buffer);

   *(int32_t *)buffer = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::andimmx);
   tempReg->setRegisterFieldRD((uint32_t *)buffer);
   dataReg->setRegisterFieldRN((uint32_t *)buffer);
   // OBJECT_HEADER_LOCK_RECURSION_MASK is 0xF8
   *(int32_t *)buffer |= 0x78E800; // immr=56, imms=58 for 0xFFFFFFFFFFFFFF07
   buffer += ARM64_INSTRUCTION_LENGTH;

   *(int32_t *)buffer = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::subsx); // for cmp
   metaReg->setRegisterFieldRN((uint32_t *)buffer);
   tempReg->setRegisterFieldRM((uint32_t *)buffer);
   zeroReg->setRegisterFieldRD((uint32_t *)buffer);
   buffer += ARM64_INSTRUCTION_LENGTH;

   *(int32_t *)buffer = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::b_cond);
   *(int32_t *)buffer |= ((4 << 5) | TR::CC_NE); // 4 instructions forward, bne
   buffer += ARM64_INSTRUCTION_LENGTH;

   *(int32_t *)buffer = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::subimmx);
   dataReg->setRegisterFieldRD((uint32_t *)buffer);
   dataReg->setRegisterFieldRN((uint32_t *)buffer);
   *(int32_t *)buffer |= ((LOCK_INC_DEC_VALUE & 0xFFF) << 10); // imm12
   buffer += ARM64_INSTRUCTION_LENGTH;

   op = fej9->generateCompressedLockWord() ? TR::InstOpCode::strimmw : TR::InstOpCode::strimmx;
   *(int32_t *)buffer = TR::InstOpCode::getOpCodeBinaryEncoding(op);
   dataReg->setRegisterFieldRT((uint32_t *)buffer);
   addrReg->setRegisterFieldRN((uint32_t *)buffer);
   // offset 0 -- no need to encode
   buffer += ARM64_INSTRUCTION_LENGTH;

   *(int32_t *)buffer = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::b);
   intptr_t destination = (intptr_t)getRestartLabel()->getCodeLocation();
   TR_ASSERT(!cg()->directCallRequiresTrampoline(destination, (intptr_t)buffer), "Jump target too far away.");
   intptr_t distance = (intptr_t)destination - (intptr_t)buffer;
   *(int32_t *)buffer |= ((distance >> 2) & 0x3FFFFFF); // imm26
   buffer += ARM64_INSTRUCTION_LENGTH;

   cg()->setBinaryBufferCursor(buffer);
   buffer = TR::ARM64HelperCallSnippet::emitSnippetBody(); // x0 holds the object

   return buffer;
   }

void
TR::ARM64MonitorExitSnippet::print(TR::FILE *pOutFile, TR_Debug *debug)
   {
   uint8_t *cursor = getDecLabel()->getCodeLocation();
   debug->printSnippetLabel(pOutFile, getDecLabel(), cursor, "Dec Monitor Counter");
   }

uint32_t
TR::ARM64MonitorExitSnippet::getLength(int32_t estimatedSnippetStart)
   {
   int32_t len = 6 * ARM64_INSTRUCTION_LENGTH;
   return len + TR::ARM64HelperCallSnippet::getLength(estimatedSnippetStart+len);
   }

int32_t TR::ARM64MonitorExitSnippet::setEstimatedCodeLocation(int32_t estimatedSnippetStart)
   {
   _decLabel->setEstimatedCodeLocation(estimatedSnippetStart);
   getSnippetLabel()->setEstimatedCodeLocation(estimatedSnippetStart + 6 * ARM64_INSTRUCTION_LENGTH);
   return estimatedSnippetStart;
   }
