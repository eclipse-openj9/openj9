/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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
#ifdef J9VM_TASUKI_LOCKS_SINGLE_SLOT

#include "arm/codegen/J9ARMSnippet.hpp"

#include <stdint.h>
#include "arm/codegen/ARMInstruction.hpp"
#include "codegen/Machine.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "runtime/J9CodeCache.hpp"

TR::ARMMonitorEnterSnippet::ARMMonitorEnterSnippet(
   TR::CodeGenerator   *codeGen,
   TR::Node            *monitorNode,
   int32_t             lwOffset,
   //bool                isPreserving,
   TR::LabelSymbol      *incLabel,
   TR::LabelSymbol      *callLabel,
   TR::LabelSymbol      *restartLabel)
   : _incLabel(incLabel),
     _lwOffset(lwOffset),
 //    _isReservationPreserving(isPreserving),
     TR::ARMHelperCallSnippet(codeGen, monitorNode, callLabel, monitorNode->getSymbolReference(), restartLabel)
   {
   // Helper call, preserves all registers
   //
   incLabel->setSnippet(this);
   gcMap().setGCRegisterMask(0xFFFFFFFF);
   }

uint8_t *TR::ARMMonitorEnterSnippet::emitSnippetBody()
   {

   // The 32-bit code for the snippet looks like:
   // incLabel:
   //    mvn     tempReg, (OBJECT_HEADER_LOCK_BITS_MASK - OBJECT_HEADER_LOCK_LAST_RECURSION_BIT) -> 0x7f
   //    and     tempReg, dataReg, tempReg
   //    cmp     metaReg, tempReg
   //    addeq   dataReg, dataReg, LOCK_INC_DEC_VALUE
   //    streq   dataReg, [addrReg]
   //    beq     restartLabel
   // callLabel:
   //    bl      jitMonitorEntry  <- HelperSnippet
   //    b       restartLabel;

   TR::RegisterDependencyConditions *deps = getRestartLabel()->getInstruction()->getDependencyConditions();

   TR::RealRegister *metaReg  = cg()->getMethodMetaDataRegister();
   TR::RealRegister *dataReg  = cg()->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(1)->getRealRegister());
   TR::RealRegister *addrReg  = cg()->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(2)->getRealRegister());
   TR::RealRegister *tempReg  = cg()->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(3)->getRealRegister());

   TR_ARMOpCode opcode;
   TR_ARMOpCodes opCodeValue;

   uint8_t *buffer = cg()->getBinaryBufferCursor();

   _incLabel->setCodeLocation(buffer);

   opcode.setOpCodeValue(ARMOp_mvn);
   buffer = opcode.copyBinaryToBuffer(buffer);
   tempReg->setRegisterFieldRD((uint32_t *)buffer);
   // OBJECT_HEADER_LOCK_BITS_MASK          is 0xFF
   // OBJECT_HEADER_LOCK_LAST_RECURSION_BIT is 0x80
   // Taken out before complement              0x7F
   // rotate_imm is 0 and immed_8 is 0x7F, I bit set to 1, result should be 0xFFFFFF80
   *(int32_t *)buffer |= ((uint32_t)(ARMConditionCodeAL) << 28 | 0x1 << 25 | (OBJECT_HEADER_LOCK_BITS_MASK - OBJECT_HEADER_LOCK_LAST_RECURSION_BIT));
   buffer += ARM_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(ARMOp_and);
   buffer = opcode.copyBinaryToBuffer(buffer);
   tempReg->setRegisterFieldRD((uint32_t *)buffer);
   dataReg->setRegisterFieldRN((uint32_t *)buffer);
   tempReg->setRegisterFieldRM((uint32_t *)buffer);
   *(int32_t *)buffer |= ((uint32_t)(ARMConditionCodeAL) << 28);
   buffer += ARM_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(ARMOp_cmp);
   buffer = opcode.copyBinaryToBuffer(buffer);
   metaReg->setRegisterFieldRN((uint32_t *)buffer);
   tempReg->setRegisterFieldRM((uint32_t *)buffer);
   *(int32_t *)buffer |= ((uint32_t)(ARMConditionCodeAL) << 28);
   buffer += ARM_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(ARMOp_add);
   buffer = opcode.copyBinaryToBuffer(buffer);
   dataReg->setRegisterFieldRD((uint32_t *)buffer);
   dataReg->setRegisterFieldRN((uint32_t *)buffer);
   *(int32_t *)buffer |= ((uint32_t)(ARMConditionCodeEQ) << 28 | 0x1 << 25 | LOCK_INC_DEC_VALUE);
   buffer += ARM_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(ARMOp_str);
   buffer = opcode.copyBinaryToBuffer(buffer);
   dataReg->setRegisterFieldRD((uint32_t *)buffer);
   addrReg->setRegisterFieldRN((uint32_t *)buffer); // offset_12 = 0, U = X, B = 0, L = 0
   *(int32_t *)buffer |= ((uint32_t)(ARMConditionCodeEQ) << 28);
   // No modification needed
   buffer += ARM_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(ARMOp_b);
   buffer = opcode.copyBinaryToBuffer(buffer);
   *(int32_t *)buffer |= ((uint32_t)(ARMConditionCodeEQ) << 28 | ((getRestartLabel()->getCodeLocation() - buffer - 8) >> 2) & 0x00FFFFFF);
   buffer += ARM_INSTRUCTION_LENGTH;

   cg()->setBinaryBufferCursor(buffer);
   buffer = TR::ARMHelperCallSnippet::emitSnippetBody();

   return buffer;
   }


void
TR::ARMMonitorEnterSnippet::print(TR::FILE *pOutFile, TR_Debug *debug)
   {
   uint8_t *bufferPos = getIncLabel()->getCodeLocation();
   // debug->printSnippetLabel(pOutFile, getIncLabel(), bufferPos, debug->getName(snippet));
   debug->printSnippetLabel(pOutFile, getIncLabel(), bufferPos, "Inc Monitor Counter");

   TR::Machine      *machine    = cg()->machine();
   TR::RegisterDependencyConditions *deps = getRestartLabel()->getInstruction()->getDependencyConditions();
   TR::LabelSymbol *restartLabel = getRestartLabel();
   TR::RealRegister *metaReg  = cg()->getMethodMetaDataRegister();
   TR::RealRegister *dataReg  = machine->getRealRegister(deps->getPostConditions()->getRegisterDependency(1)->getRealRegister());
   TR::RealRegister *addrReg  = machine->getRealRegister(deps->getPostConditions()->getRegisterDependency(2)->getRealRegister());
   TR::RealRegister *tempReg  = machine->getRealRegister(deps->getPostConditions()->getRegisterDependency(3)->getRealRegister());

   debug->printPrefix(pOutFile, NULL, bufferPos, 4);
   //    mvn     tempReg, (OBJECT_HEADER_LOCK_BITS_MASK - OBJECT_HEADER_LOCK_LAST_RECURSION_BIT) -> 0x7f
   trfprintf(pOutFile, "mvn \t%s, #0x%08x; (OBJECT_HEADER_LOCK_BITS_MASK - OBJECT_HEADER_LOCK_LAST_RECURSION_BIT)", debug->getName(tempReg), (OBJECT_HEADER_LOCK_BITS_MASK - OBJECT_HEADER_LOCK_LAST_RECURSION_BIT));
   bufferPos += 4;

   debug->printPrefix(pOutFile, NULL, bufferPos, 4);
   //    and     tempReg, dataReg, tempReg
   trfprintf(pOutFile, "and \t%s, %s, %s;", debug->getName(tempReg), debug->getName(dataReg), debug->getName(tempReg));
   bufferPos += 4;

   debug->printPrefix(pOutFile, NULL, bufferPos, 4);
   //    cmp     metaReg, tempReg
   trfprintf(pOutFile, "cmp \t%s, %s; ", debug->getName(metaReg), debug->getName(tempReg));
   bufferPos += 4;

   debug->printPrefix(pOutFile, NULL, bufferPos, 4);
   //    addeq   dataReg, dataReg, LOCK_INC_DEC_VALUE
   trfprintf(pOutFile, "addeq \t%s, %s, #0x%08x; Increment the count", debug->getName(dataReg), debug->getName(dataReg), LOCK_INC_DEC_VALUE);
   bufferPos += 4;

   debug->printPrefix(pOutFile, NULL, bufferPos, 4);
   //    streq   dataReg, [addrReg]
   trfprintf(pOutFile, "streq \t%s, [%s]; Increment the count", debug->getName(dataReg), debug->getName(addrReg));
   bufferPos += 4;

   debug->printPrefix(pOutFile, NULL, bufferPos, 4);
   //    beq     restartLabel
   trfprintf(pOutFile, "beq\t" POINTER_PRINTF_FORMAT "\t\t; Return to %s", (intptrj_t)(restartLabel->getCodeLocation()), debug->getName(restartLabel));
   debug->print(pOutFile, (TR::ARMHelperCallSnippet *)this);
   }
uint32_t TR::ARMMonitorEnterSnippet::getLength(int32_t estimatedSnippetStart)
   {
   int32_t len = 28;
   len += TR::ARMHelperCallSnippet::getLength(estimatedSnippetStart+len);
   return len;
   }

int32_t TR::ARMMonitorEnterSnippet::setEstimatedCodeLocation(int32_t estimatedSnippetStart)
   {
   _incLabel->setEstimatedCodeLocation(estimatedSnippetStart);
   getSnippetLabel()->setEstimatedCodeLocation(estimatedSnippetStart+ 28);
   return(estimatedSnippetStart);
   }


TR::ARMMonitorExitSnippet::ARMMonitorExitSnippet(
   TR::CodeGenerator   *codeGen,
   TR::Node            *monitorNode,
   int32_t            lwOffset,
   //bool               flag,
   //bool               isPreserving,
   TR::LabelSymbol      *decLabel,
   //TR::LabelSymbol      *restoreAndCallLabel,
   TR::LabelSymbol      *callLabel,
   TR::LabelSymbol      *restartLabel)
   : _decLabel(decLabel),
     //_restoreAndCallLabel(restartLabel),
     _lwOffset(lwOffset),
     //_isReadOnly(flag),
     //_isReservationPreserving(isPreserving),
     TR::ARMHelperCallSnippet(codeGen, monitorNode, callLabel, monitorNode->getSymbolReference(), restartLabel)
   {
   // Helper call, preserves all registers
   //
   decLabel->setSnippet(this);
   gcMap().setGCRegisterMask(0xFFFFFFFF);
   }

uint8_t *TR::ARMMonitorExitSnippet::emitSnippetBody()
   {

//#if !defined(J9VM_TASUKI_LOCKS_SINGLE_SLOT)
//   TR_ASSERT(0, "Tasuki double slot lock exit snippet not implemented");
//#endif

   TR::RegisterDependencyConditions *deps = getRestartLabel()->getInstruction()->getDependencyConditions();

   TR::RealRegister *metaReg  = cg()->getMethodMetaDataRegister();
   TR::RealRegister *objReg = cg()->machine()->getRealRegister(TR::RealRegister::gr0);
   TR::RealRegister *monitorReg = cg()->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(1)->getRealRegister());
   TR::RealRegister *threadReg  = cg()->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(2)->getRealRegister());

   TR_ARMOpCode opcode;
   TR_ARMOpCodes opCodeValue;

   uint8_t *buffer = cg()->getBinaryBufferCursor();
#if 0
   if (0 /*_isReadOnly*/)
      {
      // THIS CODE IS CURRENTLY DISABLED
      // BEFORE THIS CODE CAN BE ENABLED IT MUST BE UPDATED FOR THE NEW LOCK FIELDS
      // decLabel (actual restoreAndCallLabel):
      //    andi    threadReg, monitorReg, 0x3
      //    or      threadReg, metaReg, threadReg
      //    stwcx.  [objReg, offsetReg], threadReg
      //    beq     callLabel
      //    b       loopLabel
      // callLabel:
      //    bl   jitMonitorExit
      //    b    doneLabel;

      TR::RealRegister *offsetReg = cg()->machine()->getRealRegister(TR::RealRegister::gr4);
      _decLabel->setCodeLocation(buffer);

      opcode.setOpCodeValue(ARMOp_andi_r);
      buffer = opcode.copyBinaryToBuffer(buffer);
      monitorReg->setRegisterFieldRS((uint32_t *)buffer);
      threadReg->setRegisterFieldRA((uint32_t *)buffer);
      *(int32_t *)buffer |= (0x3);
      buffer += ARM_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(ARMOp_or);
      buffer = opcode.copyBinaryToBuffer(buffer);
      threadReg->setRegisterFieldRA((uint32_t *)buffer);
      threadReg->setRegisterFieldRS((uint32_t *)buffer);
      metaReg->setRegisterFieldRB((uint32_t *)buffer);
      buffer += ARM_INSTRUCTION_LENGTH;

      if (TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord())
         opCodeValue = ARMOp_stdcx_r;
      else
         opCodeValue = ARMOp_stwcx_r;
      opcode.setOpCodeValue(opCodeValue);
      buffer = opcode.copyBinaryToBuffer(buffer);
      threadReg->setRegisterFieldRS((uint32_t *)buffer);
      objReg->setRegisterFieldRA((uint32_t *)buffer);
      offsetReg->setRegisterFieldRB((uint32_t *)buffer);
      buffer += ARM_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(ARMOp_beq);
      buffer = opcode.copyBinaryToBuffer(buffer);
      cndReg->setRegisterFieldBI((uint32_t *)buffer);
      *(int32_t *)buffer |= 8;
      buffer += ARM_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(ARMOp_b);
      buffer = opcode.copyBinaryToBuffer(buffer);
      *(int32_t *)buffer |= ((uint32_t)(ARMConditionCodeAL) << 28 | ((getRestoreAndCallLabel()->getCodeLocation()-buffer) >> 2) & 0x00FFFFFF);
      buffer += ARM_INSTRUCTION_LENGTH;

      cg()->setBinaryBufferCursor(buffer);
      buffer = TR::ARMHelperCallSnippet::emitSnippetBody();

      return buffer;

      }
#endif
   // The 32-bit code for the snippet looks like:
   // decLabel:
   //    mvn     threadReg, OBJECT_HEADER_LOCK_RECURSION_MASK  // mask out count field
   //    and     threadReg, monitorReg, threadReg
   //    cmp     metaReg, threadReg
   //    subeq   monitorReg, monitorReg, LOCK_INC_DEC_VALUE
   //    streq   [objReg, monitor offset], monitorReg
   //    beq     restartLabel
   // callLabel:
   //    bl      jitMonitorExit                      // inflated or flc bit set
   //    b       restartLabel

   // reservationPreserving exit snippet looks like: (Not for ARM)
   // decLabel:
   //    li     threadReg, OWING_NON_INFLATED_COMPLEMENT
   //    andc   threadReg, monitorReg, threadReg
   //    cmpl   cndReg, threadReg, metaReg
   //    bne    callLabel
   //    andi_r threadReg, monitorReg, OBJECT_HEADER_LOCK_RECURSION_MASK
   //    beq    callLabel
   //    andi_r threadReg, monitorReg, OWING_NON_INFLATED_COMPLEMENT
   //    cmpli  cndReg, threadReg, LOCK_RES_CONTENDED_VALUE
   //    beq    callLabel
   //    addi   monitorReg, monitorReg, -INC
   //    st     monitorReg, [objReg, lwOffset]
   //    b      restartLabel
   // callLabel:
   //    bl     jitMonitorExit
   //    b      restartLabel

   _decLabel->setCodeLocation(buffer);
#if 0
   if (0/*isReservationPreserving()*/)
      {
      opcode.setOpCodeValue(ARMOp_li);
      buffer = opcode.copyBinaryToBuffer(buffer);
      threadReg->setRegisterFieldRT((uint32_t *)buffer);
      *(int32_t *)buffer |= LOCK_OWNING_NON_INFLATED_COMPLEMENT;
      buffer += ARM_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(ARMOp_andc);
      buffer = opcode.copyBinaryToBuffer(buffer);
      threadReg->setRegisterFieldRA((uint32_t *)buffer);
      monitorReg->setRegisterFieldRS((uint32_t *)buffer);
      threadReg->setRegisterFieldRB((uint32_t *)buffer);
      buffer += ARM_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(Op_cmpl);
      buffer = opcode.copyBinaryToBuffer(buffer);
      cndReg->setRegisterFieldRT((uint32_t *)buffer);
      threadReg->setRegisterFieldRA((uint32_t *)buffer);
      metaReg->setRegisterFieldRB((uint32_t *)buffer);
      buffer += ARM_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(ARMOp_bne);
      buffer = opcode.copyBinaryToBuffer(buffer);
      cndReg->setRegisterFieldBI((uint32_t *)buffer);
      *(int32_t *)buffer |= 36;
      buffer += ARM_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(ARMOp_andi_r);
      buffer = opcode.copyBinaryToBuffer(buffer);
      threadReg->setRegisterFieldRA((uint32_t *)buffer);
      monitorReg->setRegisterFieldRS((uint32_t *)buffer);
      *(int32_t *)buffer |= OBJECT_HEADER_LOCK_RECURSION_MASK;
      buffer += ARM_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(ARMOp_beq);
      buffer = opcode.copyBinaryToBuffer(buffer);
      cndReg->setRegisterFieldBI((uint32_t *)buffer);
      *(int32_t *)buffer |= 28;
      buffer += ARM_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(ARMOp_andi_r);
      buffer = opcode.copyBinaryToBuffer(buffer);
      threadReg->setRegisterFieldRA((uint32_t *)buffer);
      monitorReg->setRegisterFieldRS((uint32_t *)buffer);
      *(int32_t *)buffer |= LOCK_OWNING_NON_INFLATED_COMPLEMENT;
      buffer += ARM_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(Op_cmpli);
      buffer = opcode.copyBinaryToBuffer(buffer);
      cndReg->setRegisterFieldRT((uint32_t *)buffer);
      threadReg->setRegisterFieldRA((uint32_t *)buffer);
      *(int32_t *)buffer |= LOCK_RES_CONTENDED_VALUE;
      buffer += ARM_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(ARMOp_beq);
      buffer = opcode.copyBinaryToBuffer(buffer);
      cndReg->setRegisterFieldBI((uint32_t *)buffer);
      *(int32_t *)buffer |= 16;
      buffer += ARM_INSTRUCTION_LENGTH;
      }
   else
#endif
      {
      opcode.setOpCodeValue(ARMOp_mvn);
      buffer = opcode.copyBinaryToBuffer(buffer);
      threadReg->setRegisterFieldRD((uint32_t *)buffer);
      // 32-bit immediate shifter_operand
      // rotate_imm = 0
      // immed_8 = OBJECT_HEADER_LOCK_RECURSION_MASK (0xF8)
      *(int32_t *)buffer |= ((uint32_t)(ARMConditionCodeAL) << 28 | 0x1 << 25 | OBJECT_HEADER_LOCK_RECURSION_MASK);
      buffer += ARM_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(ARMOp_and);
      buffer = opcode.copyBinaryToBuffer(buffer);
      threadReg->setRegisterFieldRD((uint32_t *)buffer);
      monitorReg->setRegisterFieldRN((uint32_t *)buffer);
      threadReg->setRegisterFieldRM((uint32_t *)buffer);
      *(int32_t *)buffer |= ((uint32_t)(ARMConditionCodeAL) << 28);
      buffer += ARM_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(ARMOp_cmp);
      buffer = opcode.copyBinaryToBuffer(buffer);
      metaReg->setRegisterFieldRN((uint32_t *)buffer);
      threadReg->setRegisterFieldRM((uint32_t *)buffer);
      *(int32_t *)buffer |= ((uint32_t)(ARMConditionCodeAL) << 28);
      buffer += ARM_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(ARMOp_sub);
      buffer = opcode.copyBinaryToBuffer(buffer);
      monitorReg->setRegisterFieldRD((uint32_t *)buffer);
      monitorReg->setRegisterFieldRN((uint32_t *)buffer);
      *(int32_t *)buffer |= ((uint32_t)(ARMConditionCodeEQ) << 28 | 0x1 << 25 | (LOCK_INC_DEC_VALUE & 0xFFFF));
      buffer += ARM_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(ARMOp_str);
      buffer = opcode.copyBinaryToBuffer(buffer);
      monitorReg->setRegisterFieldRD((uint32_t *)buffer);
      objReg->setRegisterFieldRN((uint32_t *)buffer);
      //  Store base register offset
      //  P(24)=1(offset addressing), U=1(23)(add offset), I=0(immed), B=0(word), L=0(store)
      *(int32_t *)buffer |= ((uint32_t)(ARMConditionCodeEQ) << 28 | 0x1 << 24 | 0x1 << 23 | _lwOffset & 0xFFF);
      buffer += ARM_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(ARMOp_b);
      buffer = opcode.copyBinaryToBuffer(buffer);
      // Back to restartLabel
      *(int32_t *)buffer |= ((uint32_t)(ARMConditionCodeEQ) << 28 |((getRestartLabel()->getCodeLocation() - buffer - 8) >> 2) & 0x00FFFFFF);
      buffer += ARM_INSTRUCTION_LENGTH;
      }

   cg()->setBinaryBufferCursor(buffer);
   buffer = TR::ARMHelperCallSnippet::emitSnippetBody();

   return buffer;
   }


void
TR::ARMMonitorExitSnippet::print(TR::FILE *pOutFile, TR_Debug *debug)
   {
   uint8_t *bufferPos = getDecLabel()->getCodeLocation();
   // debug->printSnippetLabel(pOutFile, getDecLabel(), bufferPos, debug->getName(snippet));
   debug->printSnippetLabel(pOutFile, getDecLabel(), bufferPos, "Dec Monitor Counter");

   TR::Machine      *machine    = cg()->machine();
   TR::RegisterDependencyConditions *deps = getRestartLabel()->getInstruction()->getDependencyConditions();
   TR::LabelSymbol *restartLabel = getRestartLabel();
   TR::RealRegister *metaReg  = cg()->getMethodMetaDataRegister();
   TR::RealRegister *objReg = machine->getRealRegister(TR::RealRegister::gr0);
   TR::RealRegister *monitorReg = machine->getRealRegister(deps->getPostConditions()->getRegisterDependency(1)->getRealRegister());
   TR::RealRegister *threadReg  = machine->getRealRegister(deps->getPostConditions()->getRegisterDependency(2)->getRealRegister());

   debug->printPrefix(pOutFile, NULL, bufferPos, 4);
   //    mvn     threadReg, OBJECT_HEADER_LOCK_RECURSION_MASK  // mask out count field
   trfprintf(pOutFile, "mvn\t%s, #0x%08x; OBJECT_HEADER_LOCK_RECURSION_MASK", debug->getName(threadReg), OBJECT_HEADER_LOCK_RECURSION_MASK);
   bufferPos += 4;

   debug->printPrefix(pOutFile, NULL, bufferPos, 4);
   //    and     threadReg, monitorReg, threadReg
   trfprintf(pOutFile, "and\t%s, %s, %s; ", debug->getName(threadReg), debug->getName(monitorReg), debug->getName(threadReg));
   bufferPos += 4;

   debug->printPrefix(pOutFile, NULL, bufferPos, 4);
   //    cmp     metaReg, threadReg
   trfprintf(pOutFile, "cmp\t%s, %s; ", debug->getName(metaReg), debug->getName(threadReg));
   bufferPos += 4;

   debug->printPrefix(pOutFile, NULL, bufferPos, 4);
   //    subeq   monitorReg, monitorReg, LOCK_INC_DEC_VALUE
   trfprintf(pOutFile, "subeq\t%s, %s, #0x%08x; Decrement the count", debug->getName(monitorReg), debug->getName(monitorReg), LOCK_INC_DEC_VALUE);
   bufferPos += 4;

   debug->printPrefix(pOutFile, NULL, bufferPos, 4);
   //    streq   monitorReg, [objReg, monitor offset]
   trfprintf(pOutFile, "streq\t%s, [%s, %d]; ", debug->getName(monitorReg), debug->getName(objReg), getLockWordOffset());
   bufferPos += 4;

   debug->printPrefix(pOutFile, NULL, bufferPos, 4);
   //    beq     restartLabel
   trfprintf(pOutFile, "beq\t" POINTER_PRINTF_FORMAT "\t\t; Return to %s", (intptrj_t)(restartLabel->getCodeLocation()), debug->getName(restartLabel));

   debug->print(pOutFile, (TR::ARMHelperCallSnippet *)this);
   }



uint32_t TR::ARMMonitorExitSnippet::getLength(int32_t estimatedSnippetStart)
   {

   int32_t len;
   len = ARM_INSTRUCTION_LENGTH*6;
   len += TR::ARMHelperCallSnippet::getLength(estimatedSnippetStart + len);
   return len;
   }

int32_t TR::ARMMonitorExitSnippet::setEstimatedCodeLocation(int32_t estimatedSnippetStart)
   {
   int32_t len;
   _decLabel->setEstimatedCodeLocation(estimatedSnippetStart);

   len = ARM_INSTRUCTION_LENGTH*6;

   getSnippetLabel()->setEstimatedCodeLocation(estimatedSnippetStart+len);
   return(estimatedSnippetStart);
   }

#endif
