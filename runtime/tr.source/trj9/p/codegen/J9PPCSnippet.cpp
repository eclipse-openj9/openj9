/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#include "p/codegen/J9PPCSnippet.hpp"

#include <stdint.h>
#include "j9.h"
#include "thrdsup.h"
#include "thrtypes.h"
#include "codegen/CodeGenerator.hpp"
#include "codegen/Machine.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "codegen/SnippetGCMap.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "p/codegen/PPCEvaluator.hpp"
#include "p/codegen/PPCInstruction.hpp"
#include "p/codegen/GenerateInstructions.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheManager.hpp"

#if defined(TR_HOST_POWER)
extern uint32_t getPPCCacheLineSize();
#else
uint32_t getPPCCacheLineSize()
   {
   return 32;
   }
#endif

TR::PPCMonitorEnterSnippet::PPCMonitorEnterSnippet(
   TR::CodeGenerator   *codeGen,
   TR::Node            *monitorNode,
   int32_t             lwOffset,
   bool                isPreserving,
   TR::LabelSymbol      *incLabel,
   TR::LabelSymbol      *callLabel,
   TR::LabelSymbol      *restartLabel,
   TR::Register        *objectClassReg)
   : _incLabel(incLabel),
     _lwOffset(lwOffset),
     _isReservationPreserving(isPreserving),
     _objectClassReg(objectClassReg),
     TR::PPCHelperCallSnippet(codeGen, monitorNode, callLabel, monitorNode->getSymbolReference(), restartLabel)
   {
   // Helper call, preserves all registers
   //
   incLabel->setSnippet(this);
   gcMap().setGCRegisterMask(0xFFFFFFFF);
   }

uint8_t *TR::PPCMonitorEnterSnippet::emitSnippetBody()
   {

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());

#if !defined(J9VM_TASUKI_LOCKS_SINGLE_SLOT)
   TR_ASSERT(0, "Tasuki double slot lock enter snippet not implemented");
#endif
   // The 32-bit code for the snippet looks like:
   // incLabel:
   //    rlwinm  threadReg, monitorReg, 0, LOCK_THREAD_PTR_AND_UPPER_COUNT_BIT_MASK
   //    cmp     cr0, 0, metaReg, threadReg
   //    bne     callLabel
   //    addi    monitorReg, monitorReg, LOCK_INC_DEC_VALUE
   //    stw     [objReg, monitor offset], monitorReg
   //    b       restartLabel
   // callLabel:
   //    bl   jitMonitorEntry
   //    b    restartLabel;

   // for 64-bit the rlwinm is replaced with:
   //    rldicr  threadReg, monitorReg, 0, (long) LOCK_THREAD_PTR_AND_UPPER_COUNT_BIT_MASK



   // ReservationPreserving enter snippet looks like:
   // incLabel:
   //    li      tempReg, PRESERVE_ENTER_COMPLEMENT
   //    andc    tempReg, monitorReg, tempReg
   //    addi    threadReg, metaReg, RES
   //    cmpl    cndReg, tempReg, threadReg
   //    bne     callLabel
   //    addi    monitorReg, monitorReg, INC
   //    st      monitorReg, [objReg, lwOffset]
   //    b       restartLabel
   // callLabel:
   //    bl      jitMonitorEntry
   //    b       restartLabel

   TR::RegisterDependencyConditions *deps = getRestartLabel()->getInstruction()->getDependencyConditions();

   TR::RealRegister *metaReg  = cg()->getMethodMetaDataRegister();
   TR::RealRegister *objReg;
   if (_objectClassReg)
      objReg = cg()->machine()->getPPCRealRegister(deps->getPostConditions()->getRegisterDependency(4)->getRealRegister());
   else
      objReg = cg()->machine()->getPPCRealRegister(TR::RealRegister::gr3);
   TR::RealRegister *cndReg = cg()->machine()->getPPCRealRegister(TR::RealRegister::cr0);
   TR::RealRegister *monitorReg = cg()->machine()->getPPCRealRegister(TR::RealRegister::gr11);
   TR::RealRegister *threadReg  = cg()->machine()->getPPCRealRegister(deps->getPostConditions()->getRegisterDependency(2)->getRealRegister());

   TR::InstOpCode opcode;
   TR::InstOpCode::Mnemonic opCodeValue;

   uint8_t *buffer = cg()->getBinaryBufferCursor();

   _incLabel->setCodeLocation(buffer);

   if (isReservationPreserving())
      {
      TR::RealRegister *tempReg = cg()->machine()->getPPCRealRegister(deps->getPostConditions()->getRegisterDependency(4)->getRealRegister());

      opcode.setOpCodeValue(TR::InstOpCode::li);
      buffer = opcode.copyBinaryToBuffer(buffer);
      tempReg->setRegisterFieldRT((uint32_t *)buffer);
      *(int32_t *)buffer |= LOCK_RES_PRESERVE_ENTER_COMPLEMENT;
      buffer += PPC_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(TR::InstOpCode::andc);
      buffer = opcode.copyBinaryToBuffer(buffer);
      tempReg->setRegisterFieldRA((uint32_t *)buffer);
      monitorReg->setRegisterFieldRS((uint32_t *)buffer);
      tempReg->setRegisterFieldRB((uint32_t *)buffer);
      buffer += PPC_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(TR::InstOpCode::addi);
      buffer = opcode.copyBinaryToBuffer(buffer);
      threadReg->setRegisterFieldRT((uint32_t *)buffer);
      metaReg->setRegisterFieldRA((uint32_t *)buffer);
      *(int32_t *)buffer |= LOCK_RESERVATION_BIT;
      buffer += PPC_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(TR::InstOpCode::Op_cmpl);
      buffer = opcode.copyBinaryToBuffer(buffer);
      cndReg->setRegisterFieldRT((uint32_t *)buffer);
      tempReg->setRegisterFieldRA((uint32_t *)buffer);
      threadReg->setRegisterFieldRB((uint32_t *)buffer);
      buffer += PPC_INSTRUCTION_LENGTH;
      }
   else
      {
      if (TR::Compiler->target.is64Bit())
         {
         opcode.setOpCodeValue(TR::InstOpCode::rldicr);
         buffer = opcode.copyBinaryToBuffer(buffer);
         threadReg->setRegisterFieldRA((uint32_t *)buffer);
         monitorReg->setRegisterFieldRS((uint32_t *)buffer);
         // sh = 0
         // assumption here that thread pointer is in upper bits, so MB = 0
         // ME = 32 + LOCK_LAST_RECURSION_BIT_NUMBER
         int32_t ME = 32 + LOCK_LAST_RECURSION_BIT_NUMBER;
         int32_t me_field_encoding = (ME >> 5) | ((ME & 0x1F) << 1);
         *(int32_t *)buffer |= (me_field_encoding << 5);
         }
      else
         {
         opcode.setOpCodeValue(TR::InstOpCode::rlwinm);
         buffer = opcode.copyBinaryToBuffer(buffer);
         threadReg->setRegisterFieldRA((uint32_t *)buffer);
         monitorReg->setRegisterFieldRS((uint32_t *)buffer);
         // SH = 0
         // assumption here that thread pointer is in upper bits, so MB = 0
         // ME = LOCK_LAST_RECURSION_BIT_NUMBER
         *(int32_t *)buffer |= (LOCK_LAST_RECURSION_BIT_NUMBER << 1);
         }
      buffer += PPC_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(TR::InstOpCode::Op_cmp);
      buffer = opcode.copyBinaryToBuffer(buffer);
      cndReg->setRegisterFieldRT((uint32_t *)buffer);
      metaReg->setRegisterFieldRA((uint32_t *)buffer);
      threadReg->setRegisterFieldRB((uint32_t *)buffer);
      buffer += PPC_INSTRUCTION_LENGTH;
      }

   opcode.setOpCodeValue(TR::InstOpCode::bne);
   buffer = opcode.copyBinaryToBuffer(buffer);
   cndReg->setRegisterFieldBI((uint32_t *)buffer);
   *(int32_t *)buffer |= 16;
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(TR::InstOpCode::addi);
   buffer = opcode.copyBinaryToBuffer(buffer);
   monitorReg->setRegisterFieldRT((uint32_t *)buffer);
   monitorReg->setRegisterFieldRA((uint32_t *)buffer);
   *(int32_t *)buffer |= LOCK_INC_DEC_VALUE;
   buffer += PPC_INSTRUCTION_LENGTH;

   if (TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord())
      opCodeValue = TR::InstOpCode::std;
   else
      opCodeValue = TR::InstOpCode::stw;
   opcode.setOpCodeValue(opCodeValue);
   buffer = opcode.copyBinaryToBuffer(buffer);
   monitorReg->setRegisterFieldRS((uint32_t *)buffer);
   objReg->setRegisterFieldRA((uint32_t *)buffer);
   *(int32_t *)buffer |= _lwOffset & 0xFFFF;
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(TR::InstOpCode::b);
   buffer = opcode.copyBinaryToBuffer(buffer);
   *(int32_t *)buffer |= (getRestartLabel()->getCodeLocation()-buffer) & 0x03FFFFFC;
   buffer += PPC_INSTRUCTION_LENGTH;

   cg()->setBinaryBufferCursor(buffer);
   buffer = TR::PPCHelperCallSnippet::emitSnippetBody();

   return buffer;
   }


void
TR::PPCMonitorEnterSnippet::print(TR::FILE *pOutFile, TR_Debug *debug)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());
   uint8_t *cursor = getIncLabel()->getCodeLocation();

   debug->printSnippetLabel(pOutFile, getIncLabel(), cursor, "Inc Monitor Counter");

   TR::RegisterDependencyConditions *deps = getRestartLabel()->getInstruction()->getDependencyConditions();

   TR::Machine *machine = cg()->machine();
   TR::RealRegister *metaReg    = cg()->getMethodMetaDataRegister();
   TR::RealRegister *objReg = _objectClassReg ?
      machine->getPPCRealRegister(deps->getPostConditions()->getRegisterDependency(4)->getRealRegister()) :
      machine->getPPCRealRegister(TR::RealRegister::gr3);
   TR::RealRegister *condReg    = machine->getPPCRealRegister(TR::RealRegister::cr0);
   TR::RealRegister *monitorReg  = machine->getPPCRealRegister(TR::RealRegister::gr11);
   TR::RealRegister *threadReg = machine->getPPCRealRegister(deps->getPostConditions()->getRegisterDependency(2)->getRealRegister());

   if (isReservationPreserving())
      {
      TR::RealRegister *tempReg = machine->getPPCRealRegister(deps->getPostConditions()->getRegisterDependency(4)->getRealRegister());

      debug->printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "li \t%s, 0x%x\t;", debug->getName(tempReg), LOCK_RES_PRESERVE_ENTER_COMPLEMENT);
      cursor += 4;

      debug->printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "andc\t%s, %s, %s\t;", debug->getName(tempReg), debug->getName(monitorReg), debug->getName(tempReg));
      cursor += 4;

      debug->printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "addi\t%s, %s, %d\t;", debug->getName(threadReg), debug->getName(metaReg), LOCK_RESERVATION_BIT);
      cursor += 4;

      debug->printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "%s\t%s, %s, %s\t;", ppcOpCodeToNameMap[TR::InstOpCode::Op_cmpl][1], debug->getName(condReg), debug->getName(tempReg), debug->getName(threadReg));
      cursor += 4;
      }
   else
      {
      debug->printPrefix(pOutFile, NULL, cursor, 4);
      if (TR::Compiler->target.is64Bit())
         trfprintf(pOutFile, "rldicr \t%s, %s, 0, " INT64_PRINTF_FORMAT_HEX "; get thread value and high bit of count", debug->getName(threadReg), debug->getName(monitorReg), (int64_t) LOCK_THREAD_PTR_AND_UPPER_COUNT_BIT_MASK);
      else
         trfprintf(pOutFile, "rlwinm \t%s, %s, 0, 0x%x; get thread value and high bit of count", debug->getName(threadReg), debug->getName(monitorReg), LOCK_THREAD_PTR_AND_UPPER_COUNT_BIT_MASK);
      cursor+= 4;

      debug->printPrefix(pOutFile, NULL, cursor, 4);
      if (TR::Compiler->target.is64Bit())
         trfprintf(pOutFile, "cmp8 \t%s, %s, %s; Compare these two", debug->getName(condReg), debug->getName(metaReg), debug->getName(threadReg));
      else
         trfprintf(pOutFile, "cmp4 \t%s, %s, %s; Compare these two", debug->getName(condReg), debug->getName(metaReg), debug->getName(threadReg));
      cursor+= 4;
      }

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "bne \t");
   debug->print(pOutFile, getSnippetLabel());
   trfprintf(pOutFile, ", %s; Call Helper", debug->getName(condReg));
   cursor+= 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "addi \t%s, %s, %d; Increment the count", debug->getName(monitorReg), debug->getName(monitorReg), LOCK_INC_DEC_VALUE);
   cursor+= 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   if (TR::Compiler->target.is64Bit())
      trfprintf(pOutFile, "std \t[%s, %d], %s; Store the new count", debug->getName(objReg), getLockWordOffset(), debug->getName(monitorReg));
   else
      trfprintf(pOutFile, "stw \t[%s, %d], %s; Store the new count", debug->getName(objReg), getLockWordOffset(), debug->getName(monitorReg));
   cursor+= 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   int32_t distance = *((int32_t *) cursor) & 0x03fffffc;
   distance = (distance << 6) >> 6;   // sign extend
   trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t\t; ", (intptrj_t)cursor + distance);
   debug->print(pOutFile, getRestartLabel());

   debug->print(pOutFile, (TR::PPCHelperCallSnippet *)this);
   }


uint32_t TR::PPCMonitorEnterSnippet::getLength(int32_t estimatedSnippetStart)
   {
   int32_t len = isReservationPreserving()?32:24;
   len += TR::PPCHelperCallSnippet::getLength(estimatedSnippetStart+len);
   return len;
   }

int32_t TR::PPCMonitorEnterSnippet::setEstimatedCodeLocation(int32_t estimatedSnippetStart)
   {
   _incLabel->setEstimatedCodeLocation(estimatedSnippetStart);
   getSnippetLabel()->setEstimatedCodeLocation(estimatedSnippetStart+ (isReservationPreserving()?32:24));
   return(estimatedSnippetStart);
   }

TR::PPCMonitorExitSnippet::PPCMonitorExitSnippet(
   TR::CodeGenerator   *codeGen,
   TR::Node            *monitorNode,
   int32_t            lwOffset,
   bool               flag,
   bool               isPreserving,
   TR::LabelSymbol      *decLabel,
   TR::LabelSymbol      *restoreAndCallLabel,
   TR::LabelSymbol      *callLabel,
   TR::LabelSymbol      *restartLabel,
   TR::Register        *objectClassReg)
   : _decLabel(decLabel),
     _restoreAndCallLabel(restoreAndCallLabel),
     _lwOffset(lwOffset),
     _isReadOnly(flag),
     _isReservationPreserving(isPreserving),
     _objectClassReg(objectClassReg),
     TR::PPCHelperCallSnippet(codeGen, monitorNode, callLabel, monitorNode->getSymbolReference(), restartLabel)
   {
   // Helper call, preserves all registers
   //
   decLabel->setSnippet(this);
   gcMap().setGCRegisterMask(0xFFFFFFFF);
   }

uint8_t *TR::PPCMonitorExitSnippet::emitSnippetBody()
   {

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());

#if !defined(J9VM_TASUKI_LOCKS_SINGLE_SLOT)
   TR_ASSERT(0, "Tasuki double slot lock exit snippet not implemented");
#endif

   TR::RegisterDependencyConditions *deps = getRestartLabel()->getInstruction()->getDependencyConditions();

   TR::RealRegister *metaReg  = cg()->getMethodMetaDataRegister();
   TR::RealRegister *objReg;
   if (_objectClassReg)
      objReg = cg()->machine()->getPPCRealRegister(deps->getPostConditions()->getRegisterDependency(4)->getRealRegister());
   else
      objReg = cg()->machine()->getPPCRealRegister(TR::RealRegister::gr3);
   TR::RealRegister *cndReg = cg()->machine()->getPPCRealRegister(TR::RealRegister::cr0);
   TR::RealRegister *monitorReg = cg()->machine()->getPPCRealRegister(TR::RealRegister::gr11);
   TR::RealRegister *threadReg  = cg()->machine()->getPPCRealRegister(deps->getPostConditions()->getRegisterDependency(2)->getRealRegister());

   TR::InstOpCode opcode;
   TR::InstOpCode::Mnemonic opCodeValue;

   uint8_t *buffer = cg()->getBinaryBufferCursor();

   if (_isReadOnly)
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

      TR::RealRegister *offsetReg = cg()->machine()->getPPCRealRegister(TR::RealRegister::gr4);
      _decLabel->setCodeLocation(buffer);

      opcode.setOpCodeValue(TR::InstOpCode::andi_r);
      buffer = opcode.copyBinaryToBuffer(buffer);
      monitorReg->setRegisterFieldRS((uint32_t *)buffer);
      threadReg->setRegisterFieldRA((uint32_t *)buffer);
      *(int32_t *)buffer |= (0x3);
      buffer += PPC_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(TR::InstOpCode::OR);
      buffer = opcode.copyBinaryToBuffer(buffer);
      threadReg->setRegisterFieldRA((uint32_t *)buffer);
      threadReg->setRegisterFieldRS((uint32_t *)buffer);
      metaReg->setRegisterFieldRB((uint32_t *)buffer);
      buffer += PPC_INSTRUCTION_LENGTH;

      if (TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord())
         opCodeValue = TR::InstOpCode::stdcx_r;
      else
         opCodeValue = TR::InstOpCode::stwcx_r;
      opcode.setOpCodeValue(opCodeValue);
      buffer = opcode.copyBinaryToBuffer(buffer);
      threadReg->setRegisterFieldRS((uint32_t *)buffer);
      objReg->setRegisterFieldRA((uint32_t *)buffer);
      offsetReg->setRegisterFieldRB((uint32_t *)buffer);
      buffer += PPC_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(TR::InstOpCode::beq);
      buffer = opcode.copyBinaryToBuffer(buffer);
      cndReg->setRegisterFieldBI((uint32_t *)buffer);
      *(int32_t *)buffer |= 8;
      buffer += PPC_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(TR::InstOpCode::b);
      buffer = opcode.copyBinaryToBuffer(buffer);
      *(int32_t *)buffer |= (getRestoreAndCallLabel()->getCodeLocation()-buffer) & 0x03FFFFFC;
      buffer += PPC_INSTRUCTION_LENGTH;

      cg()->setBinaryBufferCursor(buffer);
      buffer = TR::PPCHelperCallSnippet::emitSnippetBody();

      return buffer;

      }

   // The 32-bit code for the snippet looks like:
   // decLabel:
   //    rlwinm  threadReg, monitorReg, 0, ~OBJECT_HEADER_LOCK_RECURSION_MASK  // mask out count field
   //    cmp     cr0, 0, metaReg, threadReg
   //    bne     callLabel                      // inflated or flc bit set
   //    addi    monitorReg, monitorReg, -LOCK_INC_DEC_VALUE
   //    stw     [objReg, monitor offset], monitorReg
   //    b       restartLabel
   // callLabel:
   //    bl    jitMonitorExit
   //    b     restartLabel

   // mask out count field for 64-bit is:
   //    li     threadReg, OBJECT_HEADER_LOCK_RECURSION_MASK
   //    andc   threadReg, monitorReg, threadReg


   // reservationPreserving exit snippet looks like:
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

   if (isReservationPreserving())
      {
      opcode.setOpCodeValue(TR::InstOpCode::li);
      buffer = opcode.copyBinaryToBuffer(buffer);
      threadReg->setRegisterFieldRT((uint32_t *)buffer);
      *(int32_t *)buffer |= LOCK_OWNING_NON_INFLATED_COMPLEMENT;
      buffer += PPC_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(TR::InstOpCode::andc);
      buffer = opcode.copyBinaryToBuffer(buffer);
      threadReg->setRegisterFieldRA((uint32_t *)buffer);
      monitorReg->setRegisterFieldRS((uint32_t *)buffer);
      threadReg->setRegisterFieldRB((uint32_t *)buffer);
      buffer += PPC_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(TR::InstOpCode::Op_cmpl);
      buffer = opcode.copyBinaryToBuffer(buffer);
      cndReg->setRegisterFieldRT((uint32_t *)buffer);
      threadReg->setRegisterFieldRA((uint32_t *)buffer);
      metaReg->setRegisterFieldRB((uint32_t *)buffer);
      buffer += PPC_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(TR::InstOpCode::bne);
      buffer = opcode.copyBinaryToBuffer(buffer);
      cndReg->setRegisterFieldBI((uint32_t *)buffer);
      *(int32_t *)buffer |= 36;
      buffer += PPC_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(TR::InstOpCode::andi_r);
      buffer = opcode.copyBinaryToBuffer(buffer);
      threadReg->setRegisterFieldRA((uint32_t *)buffer);
      monitorReg->setRegisterFieldRS((uint32_t *)buffer);
      *(int32_t *)buffer |= OBJECT_HEADER_LOCK_RECURSION_MASK;
      buffer += PPC_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(TR::InstOpCode::beq);
      buffer = opcode.copyBinaryToBuffer(buffer);
      cndReg->setRegisterFieldBI((uint32_t *)buffer);
      *(int32_t *)buffer |= 28;
      buffer += PPC_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(TR::InstOpCode::andi_r);
      buffer = opcode.copyBinaryToBuffer(buffer);
      threadReg->setRegisterFieldRA((uint32_t *)buffer);
      monitorReg->setRegisterFieldRS((uint32_t *)buffer);
      *(int32_t *)buffer |= LOCK_OWNING_NON_INFLATED_COMPLEMENT;
      buffer += PPC_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(TR::InstOpCode::Op_cmpli);
      buffer = opcode.copyBinaryToBuffer(buffer);
      cndReg->setRegisterFieldRT((uint32_t *)buffer);
      threadReg->setRegisterFieldRA((uint32_t *)buffer);
      *(int32_t *)buffer |= LOCK_RES_CONTENDED_VALUE;
      buffer += PPC_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(TR::InstOpCode::beq);
      buffer = opcode.copyBinaryToBuffer(buffer);
      cndReg->setRegisterFieldBI((uint32_t *)buffer);
      *(int32_t *)buffer |= 16;
      buffer += PPC_INSTRUCTION_LENGTH;
      }
   else
      {
      if (TR::Compiler->target.is64Bit())
         {
         opcode.setOpCodeValue(TR::InstOpCode::addi);
         buffer = opcode.copyBinaryToBuffer(buffer);
         threadReg->setRegisterFieldRT((uint32_t *)buffer);
         // RA = 0
         *(int32_t *)buffer |= OBJECT_HEADER_LOCK_RECURSION_MASK;
         buffer += PPC_INSTRUCTION_LENGTH;

         opcode.setOpCodeValue(TR::InstOpCode::andc);
         buffer = opcode.copyBinaryToBuffer(buffer);
         threadReg->setRegisterFieldRA((uint32_t *)buffer);
         monitorReg->setRegisterFieldRS((uint32_t *)buffer);
         threadReg->setRegisterFieldRB((uint32_t *)buffer);
         }
      else
         {
         opcode.setOpCodeValue(TR::InstOpCode::rlwinm);
         buffer = opcode.copyBinaryToBuffer(buffer);
         threadReg->setRegisterFieldRA((uint32_t *)buffer);
         monitorReg->setRegisterFieldRS((uint32_t *)buffer);
         // SH = 0
         // assumption here that thread pointer is in upper bits, so MB = 0
         // MB = LOCK_FIRST_RECURSION_BIT_NUMBER + 1
         // ME = LOCK_LAST_RECURSION_BIT_NUMBER - 1
         // this is a mask that wraps from the low order bits into the high bits
         *(int32_t *)buffer |= ((LOCK_FIRST_RECURSION_BIT_NUMBER + 1) << 6);
         *(int32_t *)buffer |= ((LOCK_LAST_RECURSION_BIT_NUMBER - 1) << 1);
         }
      buffer += PPC_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(TR::InstOpCode::Op_cmp);
      buffer = opcode.copyBinaryToBuffer(buffer);
      cndReg->setRegisterFieldRT((uint32_t *)buffer);
      metaReg->setRegisterFieldRA((uint32_t *)buffer);
      threadReg->setRegisterFieldRB((uint32_t *)buffer);
      buffer += PPC_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(TR::InstOpCode::bne);
      buffer = opcode.copyBinaryToBuffer(buffer);
      cndReg->setRegisterFieldBI((uint32_t *)buffer);
      *(int32_t *)buffer |= 16;
      buffer += PPC_INSTRUCTION_LENGTH;
      }

   opcode.setOpCodeValue(TR::InstOpCode::addi);
   buffer = opcode.copyBinaryToBuffer(buffer);
   monitorReg->setRegisterFieldRT((uint32_t *)buffer);
   monitorReg->setRegisterFieldRA((uint32_t *)buffer);
   *(int32_t *)buffer |= (-LOCK_INC_DEC_VALUE) & 0xFFFF;
   buffer += PPC_INSTRUCTION_LENGTH;

   if (TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord())
      opCodeValue = TR::InstOpCode::std;
   else
      opCodeValue = TR::InstOpCode::stw;
   opcode.setOpCodeValue(opCodeValue);
   buffer = opcode.copyBinaryToBuffer(buffer);
   monitorReg->setRegisterFieldRS((uint32_t *)buffer);
   objReg->setRegisterFieldRA((uint32_t *)buffer);
   *(int32_t *)buffer |= _lwOffset & 0xFFFF;
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(TR::InstOpCode::b);
   buffer = opcode.copyBinaryToBuffer(buffer);
   *(int32_t *)buffer |= (getRestartLabel()->getCodeLocation()-buffer) & 0x03FFFFFC;
   buffer += PPC_INSTRUCTION_LENGTH;

   cg()->setBinaryBufferCursor(buffer);
   buffer = TR::PPCHelperCallSnippet::emitSnippetBody();

   return buffer;
   }


void
TR::PPCMonitorExitSnippet::print(TR::FILE *pOutFile, TR_Debug *debug)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());
   uint8_t *cursor = getDecLabel()->getCodeLocation();

   debug->printSnippetLabel(pOutFile, getDecLabel(), cursor, "Dec Monitor Counter");

   TR::RegisterDependencyConditions *conditions = getRestartLabel()->getInstruction()->getDependencyConditions();
   TR::RealRegister *objReg     = _objectClassReg ?
      cg()->machine()->getPPCRealRegister(conditions->getPostConditions()->getRegisterDependency(4)->getRealRegister()) :
      cg()->machine()->getPPCRealRegister(TR::RealRegister::gr3);
   TR::RealRegister *metaReg    = cg()->getMethodMetaDataRegister();
   TR::RealRegister *condReg    = cg()->machine()->getPPCRealRegister(TR::RealRegister::cr0);
   TR::RealRegister *monitorReg  = cg()->machine()->getPPCRealRegister(TR::RealRegister::gr11);
   TR::RealRegister *threadReg = cg()->machine()->getPPCRealRegister(conditions->getPostConditions()->getRegisterDependency(2)->getRealRegister());

   if (isReservationPreserving())
      {
      debug->printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "li \t%s, 0x%x\t;", debug->getName(threadReg), LOCK_OWNING_NON_INFLATED_COMPLEMENT);
      cursor += 4;

      debug->printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "andc\t%s, %s, %s\t;", debug->getName(threadReg), debug->getName(monitorReg), debug->getName(threadReg));
      cursor += 4;

      debug->printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "%s\t%s, %s, %s\t;", ppcOpCodeToNameMap[TR::InstOpCode::Op_cmpl][1], debug->getName(condReg), debug->getName(threadReg), debug->getName(metaReg));
      cursor += 4;

      debug->printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "bne \t%s, " POINTER_PRINTF_FORMAT "\t;", debug->getName(condReg), (intptrj_t)cursor + 36);
      cursor += 4;

      debug->printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "andi.\t%s, %s, 0x%x\t;", debug->getName(threadReg), debug->getName(monitorReg), OBJECT_HEADER_LOCK_RECURSION_MASK);
      cursor += 4;

      debug->printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "beq \t%s, " POINTER_PRINTF_FORMAT "\t;", debug->getName(condReg), (intptrj_t)cursor + 28);
      cursor += 4;

      debug->printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "andi.\t%s, %s, 0x%x\t;", debug->getName(threadReg), debug->getName(monitorReg), LOCK_OWNING_NON_INFLATED_COMPLEMENT);
      cursor += 4;

      debug->printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "cmplwi %s, %s, 0x%x\t;", debug->getName(condReg), debug->getName(threadReg), LOCK_RES_CONTENDED_VALUE);
      cursor += 4;

      debug->printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "beq \t%s, " POINTER_PRINTF_FORMAT "\t;", debug->getName(condReg), (intptrj_t)cursor + 16);
      cursor += 4;
      }
   else
      {
      debug->printPrefix(pOutFile, NULL, cursor, 4);
      if (TR::Compiler->target.is64Bit())
         {
         trfprintf(pOutFile, "li \t%s, 0x%x", debug->getName(threadReg), *((int32_t *) cursor) & 0x0000ffff);
         cursor += 4;
         debug->printPrefix(pOutFile, NULL, cursor, 4);
         trfprintf(pOutFile, "andc \t%s, %s, %s; Mask out count field", debug->getName(threadReg), debug->getName(monitorReg), debug->getName(threadReg));
         }
      else
         trfprintf(pOutFile, "rlwinm \t%s, %s, 0, 0x%x; Mask out count field", debug->getName(threadReg), debug->getName(monitorReg), ~OBJECT_HEADER_LOCK_RECURSION_MASK);
      cursor+= 4;

      debug->printPrefix(pOutFile, NULL, cursor, 4);
      if (TR::Compiler->target.is64Bit())
         trfprintf(pOutFile, "cmp8 \t%s, %s, %s; Test for inflated or flc bit set", debug->getName(condReg), debug->getName(metaReg), debug->getName(threadReg));
      else
         trfprintf(pOutFile, "cmp4 \t%s, %s, %s; Test for inflated or flc bit set", debug->getName(condReg), debug->getName(metaReg), debug->getName(threadReg));
      cursor+= 4;

      debug->printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "bne \t");
      debug->print(pOutFile, getSnippetLabel());
      trfprintf(pOutFile, ", %s; Call Helper", debug->getName(condReg));
      cursor+= 4;
      }

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "addi \t%s, %s, %d; Decrement the count", debug->getName(monitorReg), debug->getName(monitorReg), -LOCK_INC_DEC_VALUE);
   cursor+= 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   if (TR::Compiler->target.is64Bit())
      trfprintf(pOutFile, "std \t[%s, %d], %s; Store the new count", debug->getName(objReg), getLockWordOffset(), debug->getName(monitorReg));
   else
      trfprintf(pOutFile, "stw \t[%s, %d], %s; Store the new count", debug->getName(objReg), getLockWordOffset(), debug->getName(monitorReg));
   cursor+= 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   int32_t distance = *((int32_t *) cursor) & 0x03fffffc;
   distance = (distance << 6) >> 6;   // sign extend
   trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t\t; ", (intptrj_t)cursor + distance);
   debug->print(pOutFile, getRestartLabel());

   debug->print(pOutFile, (TR::PPCHelperCallSnippet *)this);
   }


uint32_t TR::PPCMonitorExitSnippet::getLength(int32_t estimatedSnippetStart)
   {

   int32_t len;
   if (_isReadOnly)
      len = PPC_INSTRUCTION_LENGTH*5;
   else if (isReservationPreserving())
      len = PPC_INSTRUCTION_LENGTH*12;
   else
      {
      if (TR::Compiler->target.is64Bit())
         len = PPC_INSTRUCTION_LENGTH*7;
      else
         len = PPC_INSTRUCTION_LENGTH*6;
      }
   len += TR::PPCHelperCallSnippet::getLength(estimatedSnippetStart + len);
   return len;
   }

int32_t TR::PPCMonitorExitSnippet::setEstimatedCodeLocation(int32_t estimatedSnippetStart)
   {
   int32_t len;
   _decLabel->setEstimatedCodeLocation(estimatedSnippetStart);

   if (_isReadOnly)
      len = PPC_INSTRUCTION_LENGTH*5;
   else if (isReservationPreserving())
      len = PPC_INSTRUCTION_LENGTH*12;
   else
      {
      if (TR::Compiler->target.is64Bit())
         len = PPC_INSTRUCTION_LENGTH*7;
      else
         len = PPC_INSTRUCTION_LENGTH*6;
      }

   getSnippetLabel()->setEstimatedCodeLocation(estimatedSnippetStart+len);
   return(estimatedSnippetStart);
   }

TR::PPCLockReservationEnterSnippet::PPCLockReservationEnterSnippet(
   TR::CodeGenerator   *cg,
   TR::Node            *monitorNode,
   int32_t            lwOffset,
   TR::LabelSymbol      *startLabel,
   TR::LabelSymbol      *enterCallLabel,
   TR::LabelSymbol      *restartLabel,
   TR::Register        *objectClassReg)
   : _lwOffset(lwOffset), _startLabel(startLabel),
     _objectClassReg(objectClassReg),
     TR::PPCHelperCallSnippet(cg, monitorNode, enterCallLabel, monitorNode->getSymbolReference(), restartLabel)
   {
   startLabel->setSnippet(this);
   // Default registerMask is right already.
   }

uint8_t *
TR::PPCLockReservationEnterSnippet::emitSnippetBody()
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());

   // PRIMITIVE lockReservation enter snippet looks like:
   // startLabel:
   //    cmpli cr0, monitorReg, 0
   //    bne   reserved_checkLabel
   //    li    tempReg, lwOffset
   // loopLabel:
   //    larx  monitorReg, [objReg, tempReg]
   //    cmpli cr0, monitorReg, 0
   //    bne   enterCallLabel
   //    stcx. valReg, [objReg, tempReg]
   //    bne   loopLabel
   //    isync
   //    b     restartLabel
   // reserved_checkLabel:
   //    li    tempReg, PRIMITIVE_ENTER_MASK
   //    andc  tempReg, monitorReg, tempReg
   //    cmpl  cr0, tempReg, valReg
   //    beq   restartLabel (NOTE: possible far branch.)
   // enterCallLabel:
   //    bl    jitMonitorEntry
   //    b     restartLabel

   // lockReservation enter snippet looks like:
   // startLabel:
   //    cmpli cr0, monitorReg, 0
   //    bne   reserved_checkLabel
   //    li    tempReg, lwOffset
   //    addi  valReg, metaReg, RES+INC           <--- Diff from PRIMITIVE
   // loopLabel:
   //    larx  monitorReg, [objReg, tempReg]
   //    cmpli cr0, monitorReg, 0
   //    bne   enterCallLabel
   //    stcx. valReg, [objReg, tempReg]
   //    bne   loopLabel
   //    isync
   //    b     restartLabel
   // reserved_checkLabel:
   //    li    tempReg, NON_PRIMITIVE_ENTER_MASK
   //    andc  tempReg, monitorReg, tempReg
   //    addi  valReg, metaReg, RES               <--- Diff
   //    cmpl  cr0, tempReg, valReg
   //    bne   enterCallLabel                     <--- Diff
   //    addi  monitorReg, monitorReg, INC        <--- Diff
   //    st    monitorReg, [objReg, lwOffset]     <--- Diff
   //    b     restartLabel                       <--- Diff
   // enterCallLabel:
   //    bl    jitMonitorEntry
   //    b     restartLabel

   // NOTE: (potential improvements)
   //   a) Didn't try to save 1 instruction in 32bit mode for reservation checking: maintainability;
   //   b) Didn't make a case for non-SMP machine: maintainability too;
   //   c) Recursive-reserved but contended, we call helper (but unnecessary);

   TR::RegisterDependencyConditions *deps = getRestartLabel()->getInstruction()->getDependencyConditions();
   TR::RealRegister *metaReg  = cg()->getMethodMetaDataRegister();
   TR::RealRegister *objReg;
   if (_objectClassReg)
      objReg = ( TR::RealRegister *) _objectClassReg->getRealRegister();
   else
      objReg = cg()->machine()->getPPCRealRegister(TR::RealRegister::gr3);
   TR::RealRegister *cndReg = cg()->machine()->getPPCRealRegister(TR::RealRegister::cr0);
   TR::RealRegister *monitorReg = cg()->machine()->getPPCRealRegister(TR::RealRegister::gr11);

   int32_t regNum = 3;

   TR::RealRegister *valReg  = cg()->machine()->getPPCRealRegister(deps->getPostConditions()->getRegisterDependency(regNum)->getRealRegister());
   TR::RealRegister *tempReg  = cg()->machine()->getPPCRealRegister(deps->getPostConditions()->getRegisterDependency(regNum + 1)->getRealRegister());

   TR::InstOpCode  opcode;
   TR::InstOpCode::Mnemonic opCodeValue;
   uint8_t      *buffer = cg()->getBinaryBufferCursor();
   bool          isPrimitive = getNode()->isPrimitiveLockedRegion();

   getStartLabel()->setCodeLocation(buffer);

   opcode.setOpCodeValue(TR::InstOpCode::Op_cmpli);
   buffer = opcode.copyBinaryToBuffer(buffer);
   cndReg->setRegisterFieldRT((uint32_t *)buffer);
   monitorReg->setRegisterFieldRA((uint32_t *)buffer);
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(TR::InstOpCode::bne);
   buffer = opcode.copyBinaryToBuffer(buffer);
   cndReg->setRegisterFieldBI((uint32_t *)buffer);
   *(int32_t *)buffer |= isPrimitive?36:40;
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(TR::InstOpCode::li);
   buffer = opcode.copyBinaryToBuffer(buffer);
   tempReg->setRegisterFieldRT((uint32_t *)buffer);
   *(int32_t *)buffer |= getLockWordOffset() & 0x0000FFFF;
   buffer += PPC_INSTRUCTION_LENGTH;

   if (!isPrimitive)
      {
      opcode.setOpCodeValue(TR::InstOpCode::addi);
      buffer = opcode.copyBinaryToBuffer(buffer);
      valReg->setRegisterFieldRT((uint32_t *)buffer);
      metaReg->setRegisterFieldRA((uint32_t *)buffer);
      *(int32_t *)buffer |= LOCK_RESERVATION_BIT | LOCK_INC_DEC_VALUE;
      buffer += PPC_INSTRUCTION_LENGTH;
      }

   // LoopLabel is here:
   if (TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord())
      opCodeValue = TR::InstOpCode::ldarx;
   else
      opCodeValue = TR::InstOpCode::lwarx;
   opcode.setOpCodeValue(opCodeValue);
   buffer = opcode.copyBinaryToBuffer(buffer);
   monitorReg->setRegisterFieldRT((uint32_t *)buffer);
   objReg->setRegisterFieldRA((uint32_t *)buffer);
   tempReg->setRegisterFieldRB((uint32_t *)buffer);
   if (TR::Compiler->target.cpu.id() >= TR_PPCp6)
      *(int32_t *)buffer |=  PPCOpProp_LoadReserveExclusiveAccess;
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(TR::InstOpCode::Op_cmpli);
   buffer = opcode.copyBinaryToBuffer(buffer);
   cndReg->setRegisterFieldRT((uint32_t *)buffer);
   monitorReg->setRegisterFieldRA((uint32_t *)buffer);
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(TR::InstOpCode::bne);
   buffer = opcode.copyBinaryToBuffer(buffer);
   cndReg->setRegisterFieldBI((uint32_t *)buffer);
   *(int32_t *)buffer |= isPrimitive?36:52;
   buffer += PPC_INSTRUCTION_LENGTH;

   if (TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord())
      opCodeValue = TR::InstOpCode::stdcx_r;
   else
      opCodeValue = TR::InstOpCode::stwcx_r;
   opcode.setOpCodeValue(opCodeValue);
   buffer = opcode.copyBinaryToBuffer(buffer);
   valReg->setRegisterFieldRS((uint32_t *)buffer);
   objReg->setRegisterFieldRA((uint32_t *)buffer);
   tempReg->setRegisterFieldRB((uint32_t *)buffer);
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(TR::InstOpCode::bne);
   buffer = opcode.copyBinaryToBuffer(buffer);
   cndReg->setRegisterFieldBI((uint32_t *)buffer);
   *(int32_t *)buffer |= (-16) & 0x0000FFFF;
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(TR::InstOpCode::isync);
   buffer = opcode.copyBinaryToBuffer(buffer);
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(TR::InstOpCode::b);
   buffer = opcode.copyBinaryToBuffer(buffer);
   *(int32_t *)buffer |= (getRestartLabel()->getCodeLocation() - buffer) & 0x03FFFFFC;
   buffer += PPC_INSTRUCTION_LENGTH;

   // reserved_checkLabel is here:
   opcode.setOpCodeValue(TR::InstOpCode::li);
   buffer = opcode.copyBinaryToBuffer(buffer);
   tempReg->setRegisterFieldRT((uint32_t *)buffer);
   *(int32_t *)buffer |= isPrimitive?LOCK_RES_PRIMITIVE_ENTER_MASK:LOCK_RES_NON_PRIMITIVE_ENTER_MASK;
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(TR::InstOpCode::andc);
   buffer = opcode.copyBinaryToBuffer(buffer);
   tempReg->setRegisterFieldRA((uint32_t *)buffer);
   monitorReg->setRegisterFieldRS((uint32_t *)buffer);
   tempReg->setRegisterFieldRB((uint32_t *)buffer);
   buffer += PPC_INSTRUCTION_LENGTH;

   if (!isPrimitive)
      {
      opcode.setOpCodeValue(TR::InstOpCode::addi);
      buffer = opcode.copyBinaryToBuffer(buffer);
      valReg->setRegisterFieldRT((uint32_t *)buffer);
      metaReg->setRegisterFieldRA((uint32_t *)buffer);
      *(int32_t *)buffer |= LOCK_RESERVATION_BIT;
      buffer += PPC_INSTRUCTION_LENGTH;
      }

   opcode.setOpCodeValue(TR::InstOpCode::Op_cmpl);
   buffer = opcode.copyBinaryToBuffer(buffer);
   cndReg->setRegisterFieldRT((uint32_t *)buffer);
   tempReg->setRegisterFieldRA((uint32_t *)buffer);
   valReg->setRegisterFieldRB((uint32_t *)buffer);
   buffer += PPC_INSTRUCTION_LENGTH;

   if (isPrimitive)
      {
      int32_t bOffset = getRestartLabel()->getCodeLocation()-buffer;
      if (bOffset<LOWER_IMMED || bOffset>UPPER_IMMED)
         bOffset = 8;
      opcode.setOpCodeValue(TR::InstOpCode::beq);
      buffer = opcode.copyBinaryToBuffer(buffer);
      cndReg->setRegisterFieldBI((uint32_t *)buffer);
      *(int32_t *)buffer |= bOffset & 0x0000FFFF;
      buffer += PPC_INSTRUCTION_LENGTH;
      }
   else
      {
      opcode.setOpCodeValue(TR::InstOpCode::bne);
      buffer = opcode.copyBinaryToBuffer(buffer);
      cndReg->setRegisterFieldBI((uint32_t *)buffer);
      *(int32_t *)buffer |= 16;
      buffer += PPC_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(TR::InstOpCode::addi);
      buffer = opcode.copyBinaryToBuffer(buffer);
      monitorReg->setRegisterFieldRT((uint32_t *)buffer);
      monitorReg->setRegisterFieldRA((uint32_t *)buffer);
      *(int32_t *)buffer |= LOCK_INC_DEC_VALUE;
      buffer += PPC_INSTRUCTION_LENGTH;

      if (TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord())
         opCodeValue = TR::InstOpCode::std;
      else
         opCodeValue = TR::InstOpCode::stw;
      opcode.setOpCodeValue(opCodeValue);
      buffer = opcode.copyBinaryToBuffer(buffer);
      monitorReg->setRegisterFieldRS((uint32_t *)buffer);
      objReg->setRegisterFieldRA((uint32_t *)buffer);
      *(int32_t *)buffer |= getLockWordOffset() & 0x0000FFFF;
      buffer += PPC_INSTRUCTION_LENGTH;

      opcode.setOpCodeValue(TR::InstOpCode::b);
      buffer = opcode.copyBinaryToBuffer(buffer);
      *(int32_t *)buffer |= (getRestartLabel()->getCodeLocation()-buffer) & 0x03FFFFFC;
      buffer += PPC_INSTRUCTION_LENGTH;
      }

   cg()->setBinaryBufferCursor(buffer);
   buffer = TR::PPCHelperCallSnippet::emitSnippetBody();

   return buffer;
   }


void
TR::PPCLockReservationEnterSnippet::print(TR::FILE *pOutFile, TR_Debug *debug)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());
   uint8_t *cursor = getStartLabel()->getCodeLocation();
   int32_t distance;
   bool     isPrimitive = getNode()->isPrimitiveLockedRegion();

   debug->printSnippetLabel(pOutFile, getStartLabel(), cursor, isPrimitive?"Primitive Reservation Enter":"Reservation Enter");

   TR::RegisterDependencyConditions *conditions = getRestartLabel()->getInstruction()->getDependencyConditions();
   TR::RealRegister *objReg     = cg()->machine()->getPPCRealRegister(TR::RealRegister::gr3);
   TR::RealRegister *metaReg    = cg()->getMethodMetaDataRegister();
   TR::RealRegister *condReg    = cg()->machine()->getPPCRealRegister(TR::RealRegister::cr0);
   TR::RealRegister *monitorReg = cg()->machine()->getPPCRealRegister(TR::RealRegister::gr11);
   TR::RealRegister *valReg     = cg()->machine()->getPPCRealRegister(conditions->getPostConditions()->getRegisterDependency(3)->getRealRegister());
   TR::RealRegister *tempReg    = cg()->machine()->getPPCRealRegister(conditions->getPostConditions()->getRegisterDependency(4)->getRealRegister());

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   if (TR::Compiler->target.is64Bit())
      trfprintf(pOutFile, "cmpldi ");
   else
      trfprintf(pOutFile, "cmplwi ");
   trfprintf(pOutFile, "%s, %s, 0\t;", debug->getName(condReg), debug->getName(monitorReg));
   cursor+= 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "bne \t%s, " POINTER_PRINTF_FORMAT "\t;", debug->getName(condReg), (intptrj_t)cursor + (isPrimitive?36:40));
   cursor += 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "li \t%s, %d\t;", debug->getName(tempReg), getLockWordOffset());
   cursor += 4;

   if (!isPrimitive)
      {
      debug->printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "addi\t%s, %s, %d\t;", debug->getName(valReg), debug->getName(metaReg), LOCK_RESERVATION_BIT | LOCK_INC_DEC_VALUE);
      cursor += 4;
      }

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "%s\t%s, [%s, %s]\t;", ppcOpCodeToNameMap[TR::InstOpCode::Op_larx][1], debug->getName(monitorReg), debug->getName(objReg), debug->getName(tempReg));
   cursor += 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "%s %s, %s, 0\t;", ppcOpCodeToNameMap[TR::InstOpCode::Op_cmpli][1], debug->getName(condReg), debug->getName(monitorReg));
   cursor += 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "bne \t%s, " POINTER_PRINTF_FORMAT "\t;", debug->getName(condReg), (intptrj_t)cursor + (isPrimitive?36:52));
   cursor += 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "%s\t[%s,%s], %s\t;", ppcOpCodeToNameMap[TR::InstOpCode::Op_stcx_r][1], debug->getName(objReg), debug->getName(tempReg), debug->getName(valReg));
   cursor += 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "bne \t%s, " POINTER_PRINTF_FORMAT "\t; Loop back", debug->getName(condReg), (intptrj_t)cursor - 16);
   cursor += 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "isync \t\t\t;");
   cursor += 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   distance = *((int32_t *) cursor) & 0x03fffffc;
   distance = (distance << 6) >> 6;   // sign extend
   trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t\t; ", (intptrj_t)cursor + distance);
   debug->print(pOutFile, getRestartLabel());
   cursor += 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "li \t%s, 0x%x\t; reserved_check begins", debug->getName(tempReg), isPrimitive?LOCK_RES_PRIMITIVE_ENTER_MASK:LOCK_RES_NON_PRIMITIVE_ENTER_MASK);
   cursor += 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "andc\t%s, %s, %s\t;", debug->getName(tempReg), debug->getName(monitorReg), debug->getName(tempReg));
   cursor += 4;

   if (!isPrimitive)
      {
      debug->printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "addi\t%s, %s, %d\t;", debug->getName(valReg), debug->getName(metaReg), LOCK_RESERVATION_BIT);
      cursor += 4;
      }

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "%s\t%s, %s, %s\t;", ppcOpCodeToNameMap[TR::InstOpCode::Op_cmpl][1], debug->getName(condReg), debug->getName(tempReg), debug->getName(valReg));
   cursor += 4;

   if (isPrimitive)
      {
      distance = *((int32_t *) cursor) & 0x0000ffff;
      distance = (distance << 16) >> 16;   // sign extend
      debug->printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "beq \t%s, " POINTER_PRINTF_FORMAT "\t;", debug->getName(condReg), (intptrj_t)cursor + distance);
      cursor += 4;
      }
   else
      {
      debug->printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "bne \t%s, " POINTER_PRINTF_FORMAT "\t;", debug->getName(condReg), (intptrj_t)cursor + 16);
      cursor += 4;

      debug->printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "addi\t%s, %s, %d\t;", debug->getName(monitorReg), debug->getName(monitorReg), LOCK_INC_DEC_VALUE);
      cursor += 4;

      debug->printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "%s\t[%s,%d], %s\t;", ppcOpCodeToNameMap[TR::InstOpCode::Op_st][1], debug->getName(objReg), getLockWordOffset(), debug->getName(monitorReg));
      cursor += 4;

      debug->printPrefix(pOutFile, NULL, cursor, 4);
      distance = *((int32_t *) cursor) & 0x03fffffc;
      distance = (distance << 6) >> 6;   // sign extend
      trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t\t; ", (intptrj_t)cursor + distance);
      debug->print(pOutFile, getRestartLabel());
      cursor += 4;
      }

   debug->print(pOutFile, (TR::PPCHelperCallSnippet *)this);
   }


uint32_t
TR::PPCLockReservationEnterSnippet::getLength(int32_t estimatedSnippetStart)
   {
   int32_t len = getNode()->isPrimitiveLockedRegion()?56:76;
   len += TR::PPCHelperCallSnippet::getLength(estimatedSnippetStart+len);
   return len;
   }

int32_t
TR::PPCLockReservationEnterSnippet::setEstimatedCodeLocation(int32_t p)
   {
   getStartLabel()->setEstimatedCodeLocation(p);
   getSnippetLabel()->setEstimatedCodeLocation(p+ (getNode()->isPrimitiveLockedRegion()?56:76));
   return p;
   }

TR::PPCLockReservationExitSnippet::PPCLockReservationExitSnippet(
   TR::CodeGenerator   *cg,
   TR::Node            *monitorNode,
   int32_t            lwOffset,
   TR::LabelSymbol      *startLabel,
   TR::LabelSymbol      *exitCallLabel,
   TR::LabelSymbol      *restartLabel,
   TR::Register        *objectClassReg)
   : _startLabel(startLabel), _lwOffset(lwOffset),
     _objectClassReg(objectClassReg),
     TR::PPCHelperCallSnippet(cg, monitorNode, exitCallLabel, monitorNode->getSymbolReference(), restartLabel)
   {
   startLabel->setSnippet(this);
   // Default registerMask is right already.
   }

uint8_t *
TR::PPCLockReservationExitSnippet::emitSnippetBody()
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());

   // PRIMITIVE lockReservation exit snippet looks like:
   // startLabel:
   //    li     tempReg, RES_OWNING_COMPLEMENT
   //    andc   tempReg, monitorReg, tempReg
   //    addi   valReg, metaReg, RES_BIT
   //    cmpl   cndReg, tempReg, valReg               <--- RES not owned by me: impossible case -- should not INC
   //    bne    exitCallLabel
   //    andi_r tempReg, monitorReg, RECURSION_MASK
   //    bne    restartLabel (NOTE: potential far)
   //    addi   monitorReg, monitorReg, INC
   //    st     monitorReg, [objReg, lwOffset]
   // exitCallLabel:
   //    bl     jitMonitorExit
   //    b      restartLabel

   // Non-PRIMITIVE reservationLock exit snippet looks like:
   // startLabel:
   //    li     tempReg, RES_OWNING_COMPLEMENT
   //    andc   tempReg, monitorReg, tempReg
   //    addi   valReg, metaReg, RES
   //    cmpl   cndReg, tempReg, valReg
   //    bne    exitCallLabel
   //    andi_r tempReg, monitorReg, NON_PRIMITIVE_EXIT_MASK         <--- Diff from PRIMITIVE
   //    beq    exitCallLabel                                        <--- Diff
   //    addi   monitorReg, monitorReg, -INC                         <--- Diff
   //    st     monitorReg, [objReg, lwOffset]
   //    b      restartLabel                                         <--- Diff
   // exitCallLabel:
   //    bl     jitMonitorExit
   //    b      restartLabel

   TR::RegisterDependencyConditions *deps = getRestartLabel()->getInstruction()->getDependencyConditions();
   TR::RealRegister *metaReg  = cg()->getMethodMetaDataRegister();
   TR::RealRegister *objReg;
   if (_objectClassReg)
     objReg = (TR::RealRegister *) _objectClassReg->getRealRegister();
  else
     objReg = cg()->machine()->getPPCRealRegister(TR::RealRegister::gr3);

   TR::RealRegister *cndReg = cg()->machine()->getPPCRealRegister(TR::RealRegister::cr0);
   TR::RealRegister *monitorReg = cg()->machine()->getPPCRealRegister(TR::RealRegister::gr11);

   int32_t regNum = 3;

   TR::RealRegister *valReg  = cg()->machine()->getPPCRealRegister(deps->getPostConditions()->getRegisterDependency(regNum)->getRealRegister());
   TR::RealRegister *tempReg  = cg()->machine()->getPPCRealRegister(deps->getPostConditions()->getRegisterDependency(regNum + 1)->getRealRegister());

   TR::InstOpCode  opcode;
   TR::InstOpCode::Mnemonic opCodeValue;
   uint8_t      *buffer = cg()->getBinaryBufferCursor();
   bool          isPrimitive = getNode()->isPrimitiveLockedRegion();

   getStartLabel()->setCodeLocation(buffer);

   opcode.setOpCodeValue(TR::InstOpCode::li);
   buffer = opcode.copyBinaryToBuffer(buffer);
   tempReg->setRegisterFieldRT((uint32_t *)buffer);
   *(int32_t *)buffer |= LOCK_RES_OWNING_COMPLEMENT;
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(TR::InstOpCode::andc);
   buffer = opcode.copyBinaryToBuffer(buffer);
   tempReg->setRegisterFieldRA((uint32_t *)buffer);
   monitorReg->setRegisterFieldRS((uint32_t *)buffer);
   tempReg->setRegisterFieldRB((uint32_t *)buffer);
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(TR::InstOpCode::addi);
   buffer = opcode.copyBinaryToBuffer(buffer);
   valReg->setRegisterFieldRT((uint32_t *)buffer);
   metaReg->setRegisterFieldRA((uint32_t *)buffer);
   *(int32_t *)buffer |= LOCK_RESERVATION_BIT;
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(TR::InstOpCode::Op_cmpl);
   buffer = opcode.copyBinaryToBuffer(buffer);
   cndReg->setRegisterFieldRT((uint32_t *)buffer);
   tempReg->setRegisterFieldRA((uint32_t *)buffer);
   valReg->setRegisterFieldRB((uint32_t *)buffer);
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(TR::InstOpCode::bne);
   buffer = opcode.copyBinaryToBuffer(buffer);
   cndReg->setRegisterFieldBI((uint32_t *)buffer);
   *(int32_t *)buffer |= isPrimitive?20:24;
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(TR::InstOpCode::andi_r);
   buffer = opcode.copyBinaryToBuffer(buffer);
   tempReg->setRegisterFieldRA((uint32_t *)buffer);
   monitorReg->setRegisterFieldRS((uint32_t *)buffer);
   *(int32_t *)buffer |= isPrimitive?OBJECT_HEADER_LOCK_RECURSION_MASK:LOCK_RES_NON_PRIMITIVE_EXIT_MASK;
   buffer += PPC_INSTRUCTION_LENGTH;

   if (isPrimitive)
      {
      opcode.setOpCodeValue(TR::InstOpCode::bne);
      buffer = opcode.copyBinaryToBuffer(buffer);
      cndReg->setRegisterFieldBI((uint32_t *)buffer);
      int32_t bOffset = getRestartLabel()->getCodeLocation()-buffer;
      if (bOffset<LOWER_IMMED || bOffset>UPPER_IMMED)
         bOffset = 16;
      *(int32_t *)buffer |= bOffset & 0x0000FFFF;
      }
   else
      {
      opcode.setOpCodeValue(TR::InstOpCode::beq);
      buffer = opcode.copyBinaryToBuffer(buffer);
      cndReg->setRegisterFieldBI((uint32_t *)buffer);
      *(int32_t *)buffer |= 16;
      }
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(TR::InstOpCode::addi);
   buffer = opcode.copyBinaryToBuffer(buffer);
   monitorReg->setRegisterFieldRT((uint32_t *)buffer);
   monitorReg->setRegisterFieldRA((uint32_t *)buffer);
   *(int32_t *)buffer |= (isPrimitive?LOCK_INC_DEC_VALUE:-LOCK_INC_DEC_VALUE) & 0x0000FFFF;
   buffer += PPC_INSTRUCTION_LENGTH;

   if (TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord())
      opCodeValue = TR::InstOpCode::std;
   else
      opCodeValue = TR::InstOpCode::stw;
   opcode.setOpCodeValue(opCodeValue);
   buffer = opcode.copyBinaryToBuffer(buffer);
   monitorReg->setRegisterFieldRS((uint32_t *)buffer);
   objReg->setRegisterFieldRA((uint32_t *)buffer);
   *(int32_t *)buffer |= getLockWordOffset() & 0x0000FFFF;
   buffer += PPC_INSTRUCTION_LENGTH;

   if (!isPrimitive)
      {
      opcode.setOpCodeValue(TR::InstOpCode::b);
      buffer = opcode.copyBinaryToBuffer(buffer);
      *(int32_t *)buffer |= (getRestartLabel()->getCodeLocation()-buffer) & 0x03FFFFFC;
      buffer += PPC_INSTRUCTION_LENGTH;
      }

   // exitCallLabel is here
   cg()->setBinaryBufferCursor(buffer);
   buffer = TR::PPCHelperCallSnippet::emitSnippetBody();

   return buffer;
   }


void
TR::PPCLockReservationExitSnippet::print(TR::FILE *pOutFile, TR_Debug *debug)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());
   uint8_t *cursor = getStartLabel()->getCodeLocation();
   int32_t distance;
   bool     isPrimitive = getNode()->isPrimitiveLockedRegion();

   debug->printSnippetLabel(pOutFile, getStartLabel(), cursor, isPrimitive?"Primitive Reservation Exit":"Reservation Exit");

   TR::RegisterDependencyConditions *conditions = getRestartLabel()->getInstruction()->getDependencyConditions();
   TR::RealRegister *objReg     = cg()->machine()->getPPCRealRegister(TR::RealRegister::gr3);
   TR::RealRegister *metaReg    = cg()->getMethodMetaDataRegister();
   TR::RealRegister *condReg    = cg()->machine()->getPPCRealRegister(TR::RealRegister::cr0);
   TR::RealRegister *monitorReg = cg()->machine()->getPPCRealRegister(TR::RealRegister::gr11);
   TR::RealRegister *valReg     = cg()->machine()->getPPCRealRegister(conditions->getPostConditions()->getRegisterDependency(3)->getRealRegister());
   TR::RealRegister *tempReg    = cg()->machine()->getPPCRealRegister(conditions->getPostConditions()->getRegisterDependency(4)->getRealRegister());

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "li \t%s, 0x%x\t;", debug->getName(tempReg), LOCK_RES_OWNING_COMPLEMENT);
   cursor += 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "andc\t%s, %s, %s\t;", debug->getName(tempReg), debug->getName(monitorReg), debug->getName(tempReg));
   cursor += 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "addi\t%s, %s, %d\t;", debug->getName(valReg), debug->getName(metaReg), LOCK_RESERVATION_BIT);
   cursor += 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "%s\t%s, %s, %s\t;", ppcOpCodeToNameMap[TR::InstOpCode::Op_cmpl][1], debug->getName(condReg), debug->getName(tempReg), debug->getName(valReg));
   cursor += 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "bne \t%s, " POINTER_PRINTF_FORMAT "\t;", debug->getName(condReg), (intptrj_t)cursor + (isPrimitive?20:24));
   cursor += 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "andi.\t%s, %s, 0x%x\t;", debug->getName(tempReg), debug->getName(monitorReg), isPrimitive?OBJECT_HEADER_LOCK_RECURSION_MASK:LOCK_RES_NON_PRIMITIVE_EXIT_MASK);
   cursor += 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   if (isPrimitive)
      {
      distance = *((int32_t *) cursor) & 0x0000ffff;
      distance = (distance << 16) >> 16;   // sign extend
      trfprintf(pOutFile, "bne \t%s, " POINTER_PRINTF_FORMAT "\t;", debug->getName(condReg), (intptrj_t)cursor + distance);
      }
   else
      {
      trfprintf(pOutFile, "beq \t%s, " POINTER_PRINTF_FORMAT "\t;", debug->getName(condReg), (intptrj_t)cursor + 16);
      }
   cursor += 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "addi\t%s, %s, %d\t;", debug->getName(monitorReg), debug->getName(monitorReg), isPrimitive?LOCK_INC_DEC_VALUE:-LOCK_INC_DEC_VALUE);
   cursor += 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "%s\t[%s,%d], %s\t;", ppcOpCodeToNameMap[TR::InstOpCode::Op_st][1], debug->getName(objReg), getLockWordOffset(), debug->getName(monitorReg));
   cursor += 4;

   if (!isPrimitive)
      {
      debug->printPrefix(pOutFile, NULL, cursor, 4);
      distance = *((int32_t *) cursor) & 0x03fffffc;
      distance = (distance << 6) >> 6;   // sign extend
      trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t\t; ", (intptrj_t)cursor + distance);
      debug->print(pOutFile, getRestartLabel());
      cursor += 4;
      }

   debug->print(pOutFile, (TR::PPCHelperCallSnippet *)this);
   }


uint32_t
TR::PPCLockReservationExitSnippet::getLength(int32_t estimatedSnippetStart)
   {
   int32_t len = getNode()->isPrimitiveLockedRegion()?36:40;
   len += TR::PPCHelperCallSnippet::getLength(estimatedSnippetStart+len);
   return len;
   }

int32_t
TR::PPCLockReservationExitSnippet::setEstimatedCodeLocation(int32_t p)
   {
   getStartLabel()->setEstimatedCodeLocation(p);
   getSnippetLabel()->setEstimatedCodeLocation(p+ (getNode()->isPrimitiveLockedRegion()?36:40));
   return p;
   }


TR::PPCReadMonitorSnippet::PPCReadMonitorSnippet(
   TR::CodeGenerator   *codeGen,
   TR::Node            *monitorEnterNode,
   TR::Node            *monitorExitNode,
   TR::LabelSymbol      *recurCheckLabel,
   TR::LabelSymbol      *monExitCallLabel,
   TR::LabelSymbol      *restartLabel,
   TR::InstOpCode::Mnemonic       loadOpCode,
   int32_t             loadOffset,
   TR::Register        *objectClassReg)
   : _monitorEnterHelper(monitorEnterNode->getSymbolReference()),
     _recurCheckLabel(recurCheckLabel),
     _loadOpCode(loadOpCode),
     _loadOffset(loadOffset),
     _objectClassReg(objectClassReg),
     TR::PPCHelperCallSnippet(codeGen, monitorExitNode, monExitCallLabel, monitorExitNode->getSymbolReference(), restartLabel)
   {
   recurCheckLabel->setSnippet(this);
   // Helper call, preserves all registers
   //
   gcMap().setGCRegisterMask(0xFFFFFFFF);
   }

uint8_t *TR::PPCReadMonitorSnippet::emitSnippetBody()
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());


   // The 32-bit code for the snippet looks like:
   // recurCheckLabel:
   //    rlwinm  monitorReg, monitorReg, 0, LOCK_THREAD_PTR_MASK
   //    cmp     cndReg, metaReg, monitorReg
   //    bne     cndReg, slowPath
   //    <load>
   //    b       restartLabel
   // slowPath:
   //    bl   monitorEnterHelper
   //    <load>
   //    bl   monitorExitHelper
   //    b    restartLabel;

   // for 64-bit the rlwinm is replaced with:
   //    rldicr  threadReg, monitorReg, 0, (long) LOCK_THREAD_PTR_MASK

   TR::RegisterDependencyConditions *deps = getRestartLabel()->getInstruction()->getDependencyConditions();

   TR::RealRegister *metaReg  = cg()->getMethodMetaDataRegister();
   TR::RealRegister *monitorReg  = cg()->machine()->getPPCRealRegister(deps->getPostConditions()->getRegisterDependency(0)->getRealRegister());
   TR::RealRegister *cndReg  = cg()->machine()->getPPCRealRegister(deps->getPostConditions()->getRegisterDependency(2)->getRealRegister());
   TR::RealRegister *loadResultReg  = cg()->machine()->getPPCRealRegister(deps->getPostConditions()->getRegisterDependency(3)->getRealRegister());
   bool                isResultCollectable = deps->getPostConditions()->getRegisterDependency(3)->getRegister()->containsCollectedReference();
   TR::RealRegister *loadBaseReg  = cg()->machine()->getPPCRealRegister(deps->getPostConditions()->getRegisterDependency(4)->getRealRegister());
   TR::Compilation *comp = cg()->comp();
   TR::InstOpCode opcode;

   uint8_t *buffer = cg()->getBinaryBufferCursor();

   _recurCheckLabel->setCodeLocation(buffer);

   if (TR::Compiler->target.is64Bit())
      {
      opcode.setOpCodeValue(TR::InstOpCode::rldicr);
      buffer = opcode.copyBinaryToBuffer(buffer);
      monitorReg->setRegisterFieldRA((uint32_t *)buffer);
      monitorReg->setRegisterFieldRS((uint32_t *)buffer);
      // sh = 0
      // assumption here that thread pointer is in upper bits, so MB = 0
      // ME = 32 + LOCK_LAST_RECURSION_BIT_NUMBER - 1
      int32_t ME = 32 + LOCK_LAST_RECURSION_BIT_NUMBER - 1;
      int32_t me_field_encoding = (ME >> 5) | ((ME & 0x1F) << 1);
      *(int32_t *)buffer |= (me_field_encoding << 5);
      }
   else
      {
      opcode.setOpCodeValue(TR::InstOpCode::rlwinm);
      buffer = opcode.copyBinaryToBuffer(buffer);
      monitorReg->setRegisterFieldRA((uint32_t *)buffer);
      monitorReg->setRegisterFieldRS((uint32_t *)buffer);
      // sh = 0
      // assumption here that thread pointer is in upper bits, so MB = 0
      // ME = LOCK_LAST_RECURSION_BIT_NUMBER - 1
      *(int32_t *)buffer |= ((LOCK_LAST_RECURSION_BIT_NUMBER - 1) << 1);
      }
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(TR::InstOpCode::Op_cmp);
   buffer = opcode.copyBinaryToBuffer(buffer);
   cndReg->setRegisterFieldRT((uint32_t *)buffer);
   metaReg->setRegisterFieldRA((uint32_t *)buffer);
   monitorReg->setRegisterFieldRB((uint32_t *)buffer);
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(TR::InstOpCode::bne);
   buffer = opcode.copyBinaryToBuffer(buffer);
   cndReg->setRegisterFieldBI((uint32_t *)buffer);
   *(int32_t *)buffer |= 12;
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(_loadOpCode);
   buffer = opcode.copyBinaryToBuffer(buffer);
   loadResultReg->setRegisterFieldRT((uint32_t *)buffer);
   loadBaseReg->setRegisterFieldRA((uint32_t *)buffer);
   *(int32_t *)buffer |= _loadOffset & 0xFFFF;
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(TR::InstOpCode::b);
   buffer = opcode.copyBinaryToBuffer(buffer);
   *(int32_t *)buffer |= (getRestartLabel()->getCodeLocation()-buffer) & 0x03FFFFFC;
   buffer += PPC_INSTRUCTION_LENGTH;

   intptrj_t distance = (intptrj_t)getMonitorEnterHelper()->getSymbol()->castToMethodSymbol()->getMethodAddress() - (intptrj_t)buffer;

   if (!(distance>=BRANCH_BACKWARD_LIMIT && distance<=BRANCH_FORWARD_LIMIT))
      {
      distance = fej9->indexedTrampolineLookup(getMonitorEnterHelper()->getReferenceNumber(), (void *)buffer) - (intptrj_t)buffer;
      TR_ASSERT(distance>=BRANCH_BACKWARD_LIMIT && distance<=BRANCH_FORWARD_LIMIT,
             "CodeCache is more than 32MB.\n");
      }

   opcode.setOpCodeValue(TR::InstOpCode::bl);
   buffer = opcode.copyBinaryToBuffer(buffer);

   if (comp->compileRelocatableCode())
      {
      cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(buffer,(uint8_t *)getMonitorEnterHelper(),TR_HelperAddress, cg()),
                                __FILE__, __LINE__, getNode());
      }

   *(int32_t *)buffer |= distance & 0x03FFFFFC;
   buffer += PPC_INSTRUCTION_LENGTH;

   gcMap().registerStackMap(buffer, cg());

   opcode.setOpCodeValue(_loadOpCode);
   buffer = opcode.copyBinaryToBuffer(buffer);
   loadResultReg->setRegisterFieldRT((uint32_t *)buffer);
   loadBaseReg->setRegisterFieldRA((uint32_t *)buffer);
   *(int32_t *)buffer |= _loadOffset & 0xFFFF;
   buffer += PPC_INSTRUCTION_LENGTH;

   // this will call jitMonitorExit and return to the restart label
   cg()->setBinaryBufferCursor(buffer);

   // Defect 101811
   TR_GCStackMap *exitMap = gcMap().getStackMap()->clone(cg()->trMemory());
   exitMap->setByteCodeInfo(getNode()->getByteCodeInfo());
   if (isResultCollectable)
      exitMap->setRegisterBits(cg()->registerBitMask((int)deps->getPostConditions()->getRegisterDependency(3)->getRealRegister()));

   // Throw away entry map
   gcMap().setStackMap(exitMap);
   buffer = TR::PPCHelperCallSnippet::emitSnippetBody();

   return buffer;
   }

void
TR::PPCReadMonitorSnippet::print(TR::FILE *pOutFile, TR_Debug *debug)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());
   uint8_t *cursor = getRecurCheckLabel()->getCodeLocation();

   debug->printSnippetLabel(pOutFile, getRecurCheckLabel(), cursor, "Read Monitor Snippet");

   TR::RegisterDependencyConditions *deps = getRestartLabel()->getInstruction()->getDependencyConditions();

   TR::Machine *machine = cg()->machine();
   TR::RealRegister *metaReg    = cg()->getMethodMetaDataRegister();
   TR::RealRegister *monitorReg  = machine->getPPCRealRegister(deps->getPostConditions()->getRegisterDependency(1)->getRealRegister());
   TR::RealRegister *condReg  = machine->getPPCRealRegister(deps->getPostConditions()->getRegisterDependency(2)->getRealRegister());
   TR::RealRegister *loadResultReg  = machine->getPPCRealRegister(deps->getPostConditions()->getRegisterDependency(3)->getRealRegister());
   TR::RealRegister *loadBaseReg  = machine->getPPCRealRegister(deps->getPostConditions()->getRegisterDependency(4)->getRealRegister());

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   if (TR::Compiler->target.is64Bit())
      trfprintf(pOutFile, "rldicr \t%s, %s, 0, " INT64_PRINTF_FORMAT_HEX "\t; Get owner thread value", debug->getName(monitorReg), debug->getName(monitorReg), (int64_t) LOCK_THREAD_PTR_MASK);
   else
      trfprintf(pOutFile, "rlwinm \t%s, %s, 0, 0x%x\t; Get owner thread value", debug->getName(monitorReg), debug->getName(monitorReg), LOCK_THREAD_PTR_MASK);
   cursor+= 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   if (TR::Compiler->target.is64Bit())
      trfprintf(pOutFile, "cmp8 \t%s, %s, %s\t; Compare VMThread to owner thread", debug->getName(condReg), debug->getName(metaReg), debug->getName(monitorReg));
   else
      trfprintf(pOutFile, "cmp4 \t%s, %s, %s\t; Compare VMThread to owner thread", debug->getName(condReg), debug->getName(metaReg), debug->getName(monitorReg));
   cursor+= 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   int32_t distance = *((int32_t *) cursor) & 0x0000fffc;
   distance = (distance << 16) >> 16;   // sign extend
   trfprintf(pOutFile, "bne %s, " POINTER_PRINTF_FORMAT "\t; Use Helpers", debug->getName(condReg), (intptrj_t)cursor + distance);
   cursor+= 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "%s \t%s, [%s, %d]\t; Load", ppcOpCodeToNameMap[getLoadOpCode()][1], debug->getName(loadResultReg), debug->getName(loadBaseReg), getLoadOffset());
   cursor+= 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   distance = *((int32_t *) cursor) & 0x03fffffc;
   distance = (distance << 6) >> 6;   // sign extend
   trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t\t; ", (intptrj_t)cursor + distance);
   debug->print(pOutFile, getRestartLabel());
   cursor+= 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   distance = *((int32_t *) cursor) & 0x03fffffc;
   distance = (distance << 6) >> 6;   // sign extend
   trfprintf(pOutFile, "bl \t" POINTER_PRINTF_FORMAT "\t\t; %s", (intptrj_t)cursor + distance, debug->getName(getMonitorEnterHelper()));
   if (debug->isBranchToTrampoline(getMonitorEnterHelper(), cursor, distance))
      trfprintf(pOutFile, " Through trampoline");
   cursor+= 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "%s \t%s, [%s, %d]\t; Load", ppcOpCodeToNameMap[getLoadOpCode()][1], debug->getName(loadResultReg), debug->getName(loadBaseReg), getLoadOffset());

   debug->print(pOutFile, (TR::PPCHelperCallSnippet *)this);
   }

uint32_t TR::PPCReadMonitorSnippet::getLength(int32_t estimatedSnippetStart)
   {
   int32_t len = 28;
   len += TR::PPCHelperCallSnippet::getLength(estimatedSnippetStart+len);
   return len;
   }

int32_t TR::PPCReadMonitorSnippet::setEstimatedCodeLocation(int32_t estimatedSnippetStart)
   {
   _recurCheckLabel->setEstimatedCodeLocation(estimatedSnippetStart);
   getSnippetLabel()->setEstimatedCodeLocation(estimatedSnippetStart+28);
   return(estimatedSnippetStart);
   }

TR::PPCHeapAllocSnippet::PPCHeapAllocSnippet(
   TR::CodeGenerator   *codeGen,
   TR::Node            *node,
   TR::LabelSymbol      *callLabel,
   TR::SymbolReference *destination,
   TR::LabelSymbol      *restartLabel,
   bool               insertType)
   : _restartLabel(restartLabel), _destination(destination), _insertType(insertType),
     TR::Snippet(codeGen, node, callLabel, destination->canCauseGC())
   {
   if (destination->canCauseGC())
      {
      // Helper call, preserves all registers
      //
      gcMap().setGCRegisterMask(0xFFFFFFFF);
      }
   }

uint8_t *TR::PPCHeapAllocSnippet::emitSnippetBody()
   {

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());

   // heap allocation snippet:
   //    callLabel:
   //      [li r3, array type]
   //      bl  jitNewXXXX
   //      mr  resReg, gr3
   //      b   restartLabel

   TR::RegisterDependencyConditions *deps = getRestartLabel()->getInstruction()->getDependencyConditions();

   uint8_t  *buffer = cg()->getBinaryBufferCursor();

   getSnippetLabel()->setCodeLocation(buffer);

   TR::InstOpCode opcode;

   TR::RealRegister *callRetReg  = cg()->machine()->getPPCRealRegister(TR::RealRegister::gr3);
   TR::RealRegister *resultReg  = cg()->machine()->getPPCRealRegister(deps->getPostConditions()->getRegisterDependency(6)->getRealRegister());   // Refer to VMnewEvaluator

   if (getInsertType())
      {
      TR::RealRegister *objectReg  = cg()->machine()->
         getPPCRealRegister(deps->getPostConditions()->getRegisterDependency(0)->
         getRealRegister());   // Refer to VMnewEvaluator

      opcode.setOpCodeValue(TR::InstOpCode::li);
      buffer = opcode.copyBinaryToBuffer(buffer);
      objectReg->setRegisterFieldRS((uint32_t *)buffer);
      *(int32_t *)buffer |= getNode()->getSecondChild()->getInt();
      buffer += PPC_INSTRUCTION_LENGTH;
      }
   TR::Compilation *comp = cg()->comp();
   intptrj_t   distance = (intptrj_t)getDestination()->getSymbol()->castToMethodSymbol()->getMethodAddress() - (intptrj_t)buffer;
   if (!(distance>=BRANCH_BACKWARD_LIMIT && distance<=BRANCH_FORWARD_LIMIT))
      {
      distance = fej9->indexedTrampolineLookup(getDestination()->getReferenceNumber(), (void *)buffer) - (intptrj_t)buffer;
      TR_ASSERT(distance>=BRANCH_BACKWARD_LIMIT && distance<=BRANCH_FORWARD_LIMIT, "CodeCache is more than 32MB.\n");
      }

   opcode.setOpCodeValue(TR::InstOpCode::bl);
   buffer = opcode.copyBinaryToBuffer(buffer);
   *(int32_t *)buffer |= distance & 0x03FFFFFC;
   if (cg()->comp()->compileRelocatableCode())
      {
      cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(buffer, (uint8_t*) getDestination(), TR_HelperAddress, cg()),
                             __FILE__,
                             __LINE__,
                             getNode());
      }
   buffer += PPC_INSTRUCTION_LENGTH;

   if (gcMap().getStackMap() != NULL)
      gcMap().getStackMap()->resetRegistersBits(cg()->registerBitMask(resultReg->getRegisterNumber()));
   gcMap().registerStackMap(buffer, cg());

   // AltFormatx
   opcode.setOpCodeValue(TR::InstOpCode::mr);
   buffer = opcode.copyBinaryToBuffer(buffer);
   callRetReg->setRegisterFieldRS((uint32_t *)buffer);
   resultReg->setRegisterFieldRA((uint32_t *)buffer);
   buffer += PPC_INSTRUCTION_LENGTH;

   opcode.setOpCodeValue(TR::InstOpCode::b);
   buffer = opcode.copyBinaryToBuffer(buffer);
   *(int32_t *)buffer |= (getRestartLabel()->getCodeLocation() - buffer) & 0x03FFFFFC;
   buffer += PPC_INSTRUCTION_LENGTH;

   return(buffer);
   }


void
TR::PPCHeapAllocSnippet::print(TR::FILE *pOutFile, TR_Debug *debug)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());
   uint8_t *cursor;
   int32_t  distance;

   TR::RegisterDependencyConditions *conditions = getRestartLabel()->getInstruction()->getDependencyConditions();

   TR::Machine *machine = cg()->machine();

   cursor = getSnippetLabel()->getCodeLocation();

   debug->printSnippetLabel(pOutFile, getSnippetLabel(), cursor, "Heap Allocation Snippet");

   TR::RealRegister *resultReg  = machine->getPPCRealRegister(conditions->getPostConditions()->getRegisterDependency(6)->getRealRegister());
   TR::RealRegister *callRetReg = machine->getPPCRealRegister(TR::RealRegister::gr3);

   char    *info = "";
   if (debug->isBranchToTrampoline(getDestination(), cursor, distance))
      info = " Through trampoline";

   if (getInsertType())
      {
      TR::RealRegister *arrayTypeReg  = machine->getPPCRealRegister(conditions->getPostConditions()->getRegisterDependency(0)->getRealRegister());
      debug->printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "li \t%s, 0x%x", debug->getName(arrayTypeReg), getNode()->getSecondChild()->getInt());
      cursor += 4;
      }

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   distance = *((int32_t *) cursor) & 0x03fffffc;
   distance = (distance << 6) >> 6;   // sign extend
   trfprintf(pOutFile, "bl \t" POINTER_PRINTF_FORMAT "\t\t;%s", (intptrj_t)cursor + distance, info);
   cursor += 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "mr \t%s, %s", debug->getName(resultReg), debug->getName(callRetReg));
   cursor += 4;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   distance = *((int32_t *) cursor) & 0x03fffffc;
   distance = (distance << 6) >> 6;   // sign extend
   TR_ASSERT( (intptrj_t)cursor + distance == (intptrj_t)getRestartLabel()->getCodeLocation(), "heap snippet: bad branch to restart label");
   trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t; Back to restart %s", (intptrj_t)cursor + distance, debug->getName(getRestartLabel()));
   }


uint32_t TR::PPCHeapAllocSnippet::getLength(int32_t estimatedCodeStart)
   {
   uint32_t len;
   if (getInsertType())
      {
      len = PPC_INSTRUCTION_LENGTH*4;
      }
   else
      {
      len = PPC_INSTRUCTION_LENGTH*3;
      }
   return len;
   }

TR::PPCAllocPrefetchSnippet::PPCAllocPrefetchSnippet(
   TR::CodeGenerator   *codeGen,
   TR::Node            *node,
   TR::LabelSymbol      *callLabel)
     : TR::Snippet(codeGen, node, callLabel, false)
   {
   }

uint32_t TR::getCCPreLoadedCodeSize()
   {
   uint32_t size = 0;

   // XXX: Can't check if processor supports transient at this point because processor type hasn't been determined
   //      so we have to allocate for the larger of the two scenarios
   if (false)
      {
      const uint32_t linesToPrefetch = TR::Options::_TLHPrefetchLineCount > 0 ? TR::Options::_TLHPrefetchLineCount : 8;
      size += (linesToPrefetch + 1) / 2 * 4;
      }
   else
      {
      static bool l3SkipLines = feGetEnv("TR_l3SkipLines") != NULL;
      const uint32_t linesToLdPrefetch = TR::Options::_TLHPrefetchLineCount > 0 ? TR::Options::_TLHPrefetchLineCount : 4;
      const uint32_t linesToStPrefetch = linesToLdPrefetch * 2;
      size += 4 + (linesToLdPrefetch + 1) / 2 * 4 + 1 + (l3SkipLines ? 2 : 0) + (linesToStPrefetch + 1) / 2 * 4;
      }
   size += 3;

   //if (!TR::CodeGenerator::supportsTransientPrefetch() && !doL1Pref)
   // XXX: Can't check if processor supports transient at this point because processor type hasn't been determined
   //      so we have to allocate for the larger of the two scenarios
   if (false)
      {
      const uint32_t linesToPrefetch = TR::Options::_TLHPrefetchLineCount > 0 ? TR::Options::_TLHPrefetchLineCount : 8;
      size += (linesToPrefetch + 1) / 2 * 4;
      }
   else
      {
      static bool l3SkipLines = feGetEnv("TR_l3SkipLines") != NULL;
      const uint32_t linesToLdPrefetch = TR::Options::_TLHPrefetchLineCount > 0 ? TR::Options::_TLHPrefetchLineCount : 4;
      const uint32_t linesToStPrefetch = linesToLdPrefetch * 2;
      size += 4 + (linesToLdPrefetch + 1) / 2 * 4 + 1 + (l3SkipLines ? 2 : 0) + (linesToStPrefetch + 1) / 2 * 4;
      }
   size += 3;

      //TR_ObjectAlloc/TR_ConstLenArrayAlloc/TR_VariableLenArrayAlloc/Hybrid arraylets
      size += (2 + (11 + 11) + 2) + (16 + 14) + 4 + 9 + 4;
      // If TLH prefetching is done
      size += 2*5 + 2*5;
   #if defined(DEBUG)
      //TR_ObjectAlloc/TR_ConstLenArrayAlloc
      size += 5 + 8;
   #endif

   //TR_writeBarrier/TR_writeBarrierAndCardMark/TR_cardMark
   size += 12;
   if (TR::Options::getCmdLineOptions()->getGcCardSize() > 0)
      size += 19 + (TR::Options::getCmdLineOptions()->getGcMode() != TR_WrtbarCardMarkIncremental ? 13 : 10);

#if defined(TR_TARGET_32BIT)
   // If heap base and/or size is constant we can materialize them with 1 or 2 instructions
   // Assume 2 instructions, which means we want space for 1 additional instruction
   // for both TR_writeBarrier and TR_writeBarrierAndCardMark
   if (!TR::Options::getCmdLineOptions()->isVariableHeapBaseForBarrierRange0())
      size += 2;
   if (!TR::Options::getCmdLineOptions()->isVariableHeapSizeForBarrierRange0())
      size += 2;
#endif

   //TR_arrayStoreCHK
   size += 25;

   // Add size for other helpers

   // Eyecatchers, one per helper
   size += TR_numCCPreLoadedCode;

   return TR::alignAllocationSize<8>(size * PPC_INSTRUCTION_LENGTH);
   }

#ifdef __LITTLE_ENDIAN__
#define CCEYECATCHER(a, b, c, d) (((a) << 0) | ((b) << 8) | ((c) << 16) | ((d) << 24))
#else
#define CCEYECATCHER(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | ((d) << 0))
#endif

static uint8_t* initializeCCPreLoadedPrefetch(uint8_t *buffer, void **CCPreLoadedCodeTable, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node *n = cg->getFirstInstruction()->getNode();

   // Prefetch helper; prefetches a number of lines and returns directly to JIT code
   // In:
   //   r8 = object ptr
   // Out:
   //   r8 = object ptr
   // Clobbers:
   //   r10, r11
   //   cr0

   cg->setFirstInstruction(NULL);
   cg->setAppendInstruction(NULL);

   TR::Instruction *eyecatcher = generateImmInstruction(cg, TR::InstOpCode::dd, n, CCEYECATCHER('C', 'C', 'H', 5));

   TR::LabelSymbol *entryLabel = generateLabelSymbol(cg);
   TR::Instruction *entry = generateLabelInstruction(cg, TR::InstOpCode::label, n, entryLabel);
   TR::Instruction *cursor = entry;

   TR::Register *metaReg = cg->getMethodMetaDataRegister();
   TR::Register *r8 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr8);
   TR::Register *r10 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr10);
   TR::Register *r11 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr11);
   TR::Register *cr0 = cg->machine()->getPPCRealRegister(TR::RealRegister::cr0);

   static bool    doL1Pref = feGetEnv("TR_doL1Prefetch") != NULL;
   const uint32_t ppcCacheLineSize = getPPCCacheLineSize();
   uint32_t       helperSize;

   if (!TR::CodeGenerator::supportsTransientPrefetch() && !doL1Pref)
      {
      const uint32_t linesToPrefetch = TR::Options::_TLHPrefetchLineCount > 0 ? TR::Options::_TLHPrefetchLineCount : 8;
      const uint32_t restartAfterLines = TR::Options::_TLHPrefetchBoundaryLineCount > 0 ?  TR::Options::_TLHPrefetchBoundaryLineCount : 8;
      const uint32_t skipLines = TR::Options::_TLHPrefetchStaggeredLineCount > 0  ? TR::Options::_TLHPrefetchStaggeredLineCount : 4;
      helperSize = (linesToPrefetch + 1) / 2 * 4;

      TR_ASSERT( skipLines * ppcCacheLineSize <= UPPER_IMMED, "Cache line size too big for immediate");
      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r10, r8, skipLines * ppcCacheLineSize, cursor);
      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r11, r8, (skipLines + 1) * ppcCacheLineSize, cursor);
      cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtst, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r10, 4, cg), cursor);
      cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtst, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r11, 4, cg), cursor);

      for (uint32_t i = 2; i < linesToPrefetch; i += 2)
         {
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r10, r10, ppcCacheLineSize * 2, cursor);
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r11, r11, ppcCacheLineSize * 2, cursor);
         cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtst, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r10, 4, cg), cursor);
         cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtst, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r11, 4, cg), cursor);
         }

      cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, n, r11, restartAfterLines * ppcCacheLineSize, cursor);
      cursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, tlhPrefetchFTA), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          r11, cursor);
      }
   else
      {
      // Transient version
      static const char *s = feGetEnv("TR_l3SkipLines");
      static uint32_t l3SkipLines = s ? atoi(s) : 0;
      const uint32_t linesToLdPrefetch = TR::Options::_TLHPrefetchLineCount > 0 ? TR::Options::_TLHPrefetchLineCount : 4;
      const uint32_t linesToStPrefetch = linesToLdPrefetch * 2;
      const uint32_t restartAfterLines = TR::Options::_TLHPrefetchBoundaryLineCount > 0 ?  TR::Options::_TLHPrefetchBoundaryLineCount : 4;
      const uint32_t skipLines = TR::Options::_TLHPrefetchStaggeredLineCount > 0  ? TR::Options::_TLHPrefetchStaggeredLineCount : 4;
      helperSize = 4 + (linesToLdPrefetch + 1) / 2 * 4 + 1 + (l3SkipLines ? 2 : 0) + (linesToStPrefetch + 1) / 2 * 4;

      cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r10,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, debugEventData3), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          cursor);
      cursor = generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, n, cr0, r10, 0, cursor);
      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xori, n, r10, r10, 1, cursor);
      cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, n, r11, restartAfterLines * ppcCacheLineSize, cursor);
      cursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, tlhPrefetchFTA), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          r11, cursor);
      cursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, debugEventData3), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          r10, cursor);

      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r10, r8, skipLines * ppcCacheLineSize, cursor);
      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r11, r8, (skipLines + 1) * ppcCacheLineSize, cursor);
      cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtt, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r10, 4, cg), cursor);
      cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtt, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r11, 4, cg), cursor);

      for (uint32_t i = 2; i < linesToLdPrefetch; i += 2)
         {
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r10, r10, ppcCacheLineSize * 2, cursor);
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r11, r11, ppcCacheLineSize * 2, cursor);
         cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtt, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r10, 4, cg), cursor);
         cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtt, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r11, 4, cg), cursor);
         }

      cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beqlr, n, NULL, cr0, cursor);

      if (l3SkipLines > 0)
         {
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r10, r10, ppcCacheLineSize * l3SkipLines, cursor);
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r11, r11, ppcCacheLineSize * l3SkipLines, cursor);
         }

      for (uint32_t i = 0; i < linesToStPrefetch; i += 2)
         {
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r10, r10, ppcCacheLineSize * 2, cursor);
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r11, r11, ppcCacheLineSize * 2, cursor);
         cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtstt, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r10, 4, cg), cursor);
         cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtstt, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r11, 4, cg), cursor);
         }
      }

   cursor = generateInstruction(cg, TR::InstOpCode::blr, n, cursor);

   cg->setBinaryBufferStart(buffer);
   cg->setBinaryBufferCursor(buffer);

   for (TR::Instruction *i = eyecatcher; i != NULL; i = i->getNext())
      cg->setBinaryBufferCursor(i->generateBinaryEncoding());

   helperSize += 3;
   TR_ASSERT(cg->getBinaryBufferCursor() - entryLabel->getCodeLocation() == helperSize * PPC_INSTRUCTION_LENGTH,
           "Per-codecache prefetch helper, unexpected size");

   CCPreLoadedCodeTable[TR_AllocPrefetch] = entryLabel->getCodeLocation();

   return cg->getBinaryBufferCursor() - PPC_INSTRUCTION_LENGTH;
   }

static uint8_t* initializeCCPreLoadedNonZeroPrefetch(uint8_t *buffer, void **CCPreLoadedCodeTable, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node *n = cg->getFirstInstruction()->getNode();

   // NonZero TLH Prefetch helper; prefetches a number of lines and returns directly to JIT code
   // In:
   //   r8 = object ptr
   // Out:
   //   r8 = object ptr
   // Clobbers:
   //   r10, r11
   //   cr0

   cg->setFirstInstruction(NULL);
   cg->setAppendInstruction(NULL);

   TR::Instruction *eyecatcher = generateImmInstruction(cg, TR::InstOpCode::dd, n, CCEYECATCHER('C', 'C', 'H', 6));

   TR::LabelSymbol *entryLabel = generateLabelSymbol(cg);
   TR::Instruction *entry = generateLabelInstruction(cg, TR::InstOpCode::label, n, entryLabel);
   TR::Instruction *cursor = entry;

   TR::Register *metaReg = cg->getMethodMetaDataRegister();
   TR::Register *r8 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr8);
   TR::Register *r10 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr10);
   TR::Register *r11 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr11);
   TR::Register *cr0 = cg->machine()->getPPCRealRegister(TR::RealRegister::cr0);

   static bool    doL1Pref = feGetEnv("TR_doL1Prefetch") != NULL;
   const uint32_t ppcCacheLineSize = getPPCCacheLineSize();
   uint32_t       helperSize;

   if (!TR::CodeGenerator::supportsTransientPrefetch() && !doL1Pref)
      {
      const uint32_t linesToPrefetch = TR::Options::_TLHPrefetchLineCount > 0 ? TR::Options::_TLHPrefetchLineCount : 8;
      const uint32_t restartAfterLines = TR::Options::_TLHPrefetchBoundaryLineCount > 0 ?  TR::Options::_TLHPrefetchBoundaryLineCount : 8;
      const uint32_t skipLines = TR::Options::_TLHPrefetchStaggeredLineCount > 0  ? TR::Options::_TLHPrefetchStaggeredLineCount : 4;
      helperSize = (linesToPrefetch + 1) / 2 * 4;

      TR_ASSERT( skipLines * ppcCacheLineSize <= UPPER_IMMED,"Cache line size too big for immediate");
      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r10, r8, skipLines * ppcCacheLineSize, cursor);
      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r11, r8, (skipLines + 1) * ppcCacheLineSize, cursor);
      cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtst, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r10, 4, cg), cursor);
      cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtst, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r11, 4, cg), cursor);

      for (uint32_t i = 2; i < linesToPrefetch; i += 2)
         {
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r10, r10, ppcCacheLineSize * 2, cursor);
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r11, r11, ppcCacheLineSize * 2, cursor);
         cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtst, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r10, 4, cg), cursor);
         cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtst, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r11, 4, cg), cursor);
         }

      cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, n, r11, restartAfterLines * ppcCacheLineSize, cursor);
      cursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, nonZeroTlhPrefetchFTA), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          r11, cursor);
      }
   else
      {
      // Transient version
      static const char *s = feGetEnv("TR_l3SkipLines");
      static uint32_t l3SkipLines = s ? atoi(s) : 0;
      const uint32_t linesToLdPrefetch = TR::Options::_TLHPrefetchLineCount > 0 ? TR::Options::_TLHPrefetchLineCount : 4;
      const uint32_t linesToStPrefetch = linesToLdPrefetch * 2;
      const uint32_t restartAfterLines = TR::Options::_TLHPrefetchBoundaryLineCount > 0 ?  TR::Options::_TLHPrefetchBoundaryLineCount : 4;
      const uint32_t skipLines = TR::Options::_TLHPrefetchStaggeredLineCount > 0  ? TR::Options::_TLHPrefetchStaggeredLineCount : 4;
      helperSize = 4 + (linesToLdPrefetch + 1) / 2 * 4 + 1 + (l3SkipLines ? 2 : 0) + (linesToStPrefetch + 1) / 2 * 4;

      cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r10,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, debugEventData3), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          cursor);
      cursor = generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, n, cr0, r10, 0, cursor);
      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xori, n, r10, r10, 1, cursor);
      cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, n, r11, restartAfterLines * ppcCacheLineSize, cursor);
      cursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, nonZeroTlhPrefetchFTA), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          r11, cursor);
      cursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, debugEventData3), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          r10, cursor);

      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r10, r8, skipLines * ppcCacheLineSize, cursor);
      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r11, r8, (skipLines + 1) * ppcCacheLineSize, cursor);
      cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtt, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r10, 4, cg), cursor);
      cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtt, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r11, 4, cg), cursor);

      for (uint32_t i = 2; i < linesToLdPrefetch; i += 2)
         {
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r10, r10, ppcCacheLineSize * 2, cursor);
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r11, r11, ppcCacheLineSize * 2, cursor);
         cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtt, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r10, 4, cg), cursor);
         cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtt, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r11, 4, cg), cursor);
         }

      cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beqlr, n, NULL, cr0, cursor);

      if (l3SkipLines > 0)
         {
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r10, r10, ppcCacheLineSize * l3SkipLines, cursor);
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r11, r11, ppcCacheLineSize * l3SkipLines, cursor);
         }

      for (uint32_t i = 0; i < linesToStPrefetch; i += 2)
         {
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r10, r10, ppcCacheLineSize * 2, cursor);
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r11, r11, ppcCacheLineSize * 2, cursor);
         cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtstt, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r10, 4, cg), cursor);
         cursor = generateMemInstruction(cg, TR::InstOpCode::dcbtstt, n, new (cg->trHeapMemory()) TR::MemoryReference(NULL, r11, 4, cg), cursor);
         }
      }

   cursor = generateInstruction(cg, TR::InstOpCode::blr, n, cursor);

   cg->setBinaryBufferStart(buffer);
   cg->setBinaryBufferCursor(buffer);

   for (TR::Instruction *i = eyecatcher; i != NULL; i = i->getNext())
      cg->setBinaryBufferCursor(i->generateBinaryEncoding());

   helperSize += 3;
   TR_ASSERT(cg->getBinaryBufferCursor() - entryLabel->getCodeLocation() == helperSize * PPC_INSTRUCTION_LENGTH,
           "Per-codecache prefetch helper, unexpected size");

   CCPreLoadedCodeTable[TR_NonZeroAllocPrefetch] = entryLabel->getCodeLocation();

   return cg->getBinaryBufferCursor() - PPC_INSTRUCTION_LENGTH;
   }

static TR::Instruction* genZeroInit(TR::CodeGenerator *cg, TR::Node *n, TR::Register *objStartReg, TR::Register *objEndReg, TR::Register *needsZeroInitCondReg,
                                   TR::Register *iterReg, TR::Register *zeroReg, TR::Register *condReg, uint32_t initOffset, TR::Instruction *cursor)
   {
   // Generates 24 instructions (+6 if DEBUG)
#if defined(DEBUG)
   // Fill the object with junk to make sure zero-init is working
      {
      TR::LabelSymbol *loopStartLabel = generateLabelSymbol(cg);

      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, iterReg, objStartReg, initOffset, cursor);
      cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, n, zeroReg, -1, cursor);

      cursor = generateLabelInstruction(cg, TR::InstOpCode::label, n, loopStartLabel, cursor);
      // XXX: This can be improved to use std on 64-bit, but we have to adjust for the size not being 8-byte aligned
      cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(iterReg, 0, 4, cg),
                                          zeroReg, cursor);
      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, iterReg, iterReg, 4, cursor);
      cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, condReg, iterReg, objEndReg, cursor);
      cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, n, loopStartLabel, condReg, cursor);
      }
#endif

   TR::LabelSymbol *unrolledLoopStartLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *residueLoopStartLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *doneZeroInitLabel = generateLabelSymbol(cg);

   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beqlr, n, NULL, needsZeroInitCondReg, cursor);

   cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, iterReg, objStartReg, initOffset, cursor);
   // Use the zero reg temporarily to calculate unrolled loop iterations, equal to (stop - start) >> 5
   cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, n, zeroReg, iterReg, objEndReg, cursor);
   cursor = generateShiftRightLogicalImmediate(cg, n, zeroReg, zeroReg, 5, cursor);
   cursor = generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, n, condReg, zeroReg, 0, cursor);
   cursor = generateSrc1Instruction(cg, TR::InstOpCode::mtctr, n, zeroReg);
   cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, n, zeroReg, 0, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, n, residueLoopStartLabel, condReg, cursor);

   cursor = generateLabelInstruction(cg, TR::InstOpCode::label, n, unrolledLoopStartLabel, cursor);
   for (int i = 0; i < 32; i += 4)
      cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(iterReg, i, 4, cg),
                                          zeroReg, cursor);
   cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, iterReg, iterReg, 32, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bdnz, n, unrolledLoopStartLabel, /* Not used */ condReg, cursor);

   cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, condReg, iterReg, objEndReg, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bge, n, doneZeroInitLabel, condReg, cursor);

   // Residue loop
   cursor = generateLabelInstruction(cg, TR::InstOpCode::label, n, residueLoopStartLabel, cursor);
   cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, n,
                                       new (cg->trHeapMemory()) TR::MemoryReference(iterReg, 0, 4, cg),
                                       zeroReg, cursor);
   cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, iterReg, iterReg, 4, cursor);
   cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, condReg, iterReg, objEndReg, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, n, residueLoopStartLabel, condReg, cursor);

   cursor = generateLabelInstruction(cg, TR::InstOpCode::label, n, doneZeroInitLabel, cursor);

   return cursor;
   }

static uint8_t* initializeCCPreLoadedObjectAlloc(uint8_t *buffer, void **CCPreLoadedCodeTable, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR::Node *n = cg->getFirstInstruction()->getNode();

   // Object allocation off TLH (zero or non-zero)
   // If it fails we branch to the regular helper (via it's trampoline)
   // which will return directly to JIT code
   // In:
   //   r3 = class ptr
   //   r4 = alloc size in bytes
   // If canSkipZeroInit:
   //   r11 = 1 if optimizer says we can skip zero init (allocate on nonZeroTLH)
   // Out:
   //   r3 = object ptr
   // Clobbers:
   //   r4, r8 if TLH prefetch, r10, r11
   //   cr0 if TLH prefetch, cr1, cr2

   cg->setFirstInstruction(NULL);
   cg->setAppendInstruction(NULL);

   TR::Instruction *eyecatcher = generateImmInstruction(cg, TR::InstOpCode::dd, n, CCEYECATCHER('C', 'C', 'H', 0));

   TR::LabelSymbol *entryLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *doNonZeroHeapTLHAlloc = generateLabelSymbol(cg);

   //Branch to VM Helpers for Zeroed TLH.
   TR::LabelSymbol *zeroed_branchToHelperLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *zeroed_helperTrampolineLabel = generateLabelSymbol(cg);
   zeroed_helperTrampolineLabel->setCodeLocation((uint8_t *)TR::CodeCacheManager::instance()->findHelperTrampoline(buffer, TR_newObject));

   //Branch to VM Helpers for nonZeroed TLH.
   TR::LabelSymbol *nonZeroed_branchToHelperLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *nonZeroed_helperTrampolineLabel = generateLabelSymbol(cg);
   nonZeroed_helperTrampolineLabel->setCodeLocation((uint8_t *)TR::CodeCacheManager::instance()->findHelperTrampoline(buffer, TR_newObjectNoZeroInit));

   TR::Instruction *entry = generateLabelInstruction(cg, TR::InstOpCode::label, n, entryLabel);
   TR::Instruction *cursor = entry;
#if defined(J9VM_INTERP_COMPRESSED_OBJECT_HEADER)
   const TR::InstOpCode::Mnemonic Op_stclass = TR::InstOpCode::stw;
#else
   const TR::InstOpCode::Mnemonic Op_stclass =TR::InstOpCode::Op_st;
#endif

   TR::Register *metaReg = cg->getMethodMetaDataRegister();
   TR::Register *r3 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr3);
   TR::Register *r4 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr4);
   TR::Register *r8 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr8);
   TR::Register *r10 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr10);
   TR::Register *r11 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr11);
   TR::Register *cr0 = cg->machine()->getPPCRealRegister(TR::RealRegister::cr0);
   TR::Register *cr1 = cg->machine()->getPPCRealRegister(TR::RealRegister::cr1);
   TR::Register *cr2 = cg->machine()->getPPCRealRegister(TR::RealRegister::cr2);

#if defined(DEBUG)
   // Trap if size not word aligned
   cursor = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, n, r10, r4, 0, 3, cursor);
   cursor = generateSrc1Instruction(cg, TR::InstOpCode::twneqi, n, r10, 0, cursor);
   // Trap if size does not preserve object alignment requirements
   cursor = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, n, r10, r4, 0, fej9->getObjectAlignmentInBytes() - 1, cursor);
   cursor = generateSrc1Instruction(cg, TR::InstOpCode::twneqi, n, r10, 0, cursor);
   // Trap if size less than minimum object size
   cursor = generateSrc1Instruction(cg, TR::InstOpCode::twlti, n, r4, J9_GC_MINIMUM_OBJECT_SIZE, cursor);
#endif

   //Allocate on nonZeroHeapAlloc or heapAlloc based on r11.
   cursor = generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, n, cr2, r11, 1, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, false, n, doNonZeroHeapTLHAlloc, cr2, cursor); //Set likeliness of branch to false.
   //2 instructions so far.
      {
      //Allocate on zeroed TLH here.

      if (cg->enableTLHPrefetching())
         {
         cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r10,
                                             new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, tlhPrefetchFTA), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                             cursor);
         cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::subf_r, n, r10, r4, r10, cr0, cursor);
         cursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, n,
                                             new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, tlhPrefetchFTA), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                             r10, cursor);
         }
      cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r10,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(struct J9VMThread, heapAlloc), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          cursor);
      cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r11,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(struct J9VMThread, heapTop), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          cursor);
      // New heapAlloc
      cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::add, n, r4, r10, r4, cursor);
      // Check if we exceeded the TLH
      cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr2, r4, r11, cursor);
      cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bgt, n, zeroed_branchToHelperLabel, cr2, cursor);
      // Check if we overflowed
      cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr2, r4, r10, cursor);
      cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::ble, n, zeroed_branchToHelperLabel, cr2, cursor);
      // Store new heapAlloc
      cursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(struct J9VMThread, heapAlloc), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          r4, cursor);
      // Store class ptr
      cursor = generateMemSrc1Instruction(cg,Op_stclass, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(r10, TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceField(), cg),
                                          r3, cursor);
      cursor = generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, n, r3, r10, cursor);

      if (cg->enableTLHPrefetching())
         {
         TR::LabelSymbol *prefetchHelperLabel = generateLabelSymbol(cg);
         TR_ASSERT( CCPreLoadedCodeTable[TR_AllocPrefetch], "Expecting prefetch helper to be initialized");
         prefetchHelperLabel->setCodeLocation((uint8_t *)CCPreLoadedCodeTable[TR_AllocPrefetch]);
         cursor = generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, n, r8, r3, cursor);
         cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::ble, n, prefetchHelperLabel, cr0, cursor);
         }

      //Done allocating on TLH, return to calling code.
      cursor = generateInstruction(cg, TR::InstOpCode::blr, n, cursor);

      //11 instructions excluding tlhPrefetch.
      }
   cursor = generateLabelInstruction(cg, TR::InstOpCode::label, n, doNonZeroHeapTLHAlloc, cursor);
      {
      //Allocate on nonZero TLH here.

      if (cg->enableTLHPrefetching())
         {
         cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r10,
                                             new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, nonZeroTlhPrefetchFTA), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                             cursor);
         cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::subf_r, n, r10, r4, r10, cr0, cursor);
         cursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, n,
                                             new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, nonZeroTlhPrefetchFTA), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                             r10, cursor);
         }
      cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r10,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(struct J9VMThread, nonZeroHeapAlloc), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          cursor);
      cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r11,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(struct J9VMThread, nonZeroHeapTop), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          cursor);
      // New heapAlloc
      cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::add, n, r4, r10, r4, cursor);
      // Check if we exceeded the TLH
      cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr2, r4, r11, cursor);
      cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bgt, n, nonZeroed_branchToHelperLabel, cr2, cursor);
      // Check if we overflowed
      cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr2, r4, r10, cursor);
      cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::ble, n, nonZeroed_branchToHelperLabel, cr2, cursor);
      // Store new heapAlloc
      cursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(struct J9VMThread, nonZeroHeapAlloc), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          r4, cursor);
      // Store class ptr
      cursor = generateMemSrc1Instruction(cg,Op_stclass, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(r10, TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceField(), cg),
                                          r3, cursor);
      cursor = generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, n, r3, r10, cursor);

      if (cg->enableTLHPrefetching())
         {
         TR::LabelSymbol *prefetchHelperLabel = generateLabelSymbol(cg);
         TR_ASSERT( CCPreLoadedCodeTable[TR_NonZeroAllocPrefetch], "Expecting non-zero tlh prefetch helper to be initialized");
         prefetchHelperLabel->setCodeLocation((uint8_t *)CCPreLoadedCodeTable[TR_NonZeroAllocPrefetch]);
         cursor = generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, n, r8, r3, cursor);
         cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::ble, n, prefetchHelperLabel, cr0, cursor);
         }

      //Done allocating on TLH, return to calling code.
      cursor = generateInstruction(cg, TR::InstOpCode::blr, n, cursor);

      //11 instructions excluding tlhPrefetch.
      }

   //Call out to VM helper for Zeroed TLH.
   cursor = generateLabelInstruction(cg, TR::InstOpCode::label, n, zeroed_branchToHelperLabel, cursor);
   cursor = generateLabelInstruction(cg, TR::InstOpCode::b, n, zeroed_helperTrampolineLabel, cursor);
   //1 instruction.

   //Call out to VM helper for nonZeroed TLH.
   cursor = generateLabelInstruction(cg, TR::InstOpCode::label, n, nonZeroed_branchToHelperLabel, cursor);
   cursor = generateLabelInstruction(cg, TR::InstOpCode::b, n, nonZeroed_helperTrampolineLabel, cursor);
   //1 instruction.

   cg->setBinaryBufferStart(buffer);
   cg->setBinaryBufferCursor(buffer);

   for (TR::Instruction *i = eyecatcher; i != NULL; i = i->getNext())
      cg->setBinaryBufferCursor(i->generateBinaryEncoding());

   uint32_t helperSize = 2 + (11 + 11) + 2 + (cg->enableTLHPrefetching() ? 2*5 : 0);

#ifdef DEBUG
   helperSize += 5;
#endif

   TR_ASSERT(cg->getBinaryBufferCursor() - entryLabel->getCodeLocation() == helperSize * PPC_INSTRUCTION_LENGTH,
             "Per-codecache object allocation helper, unexpected size");

   CCPreLoadedCodeTable[TR_ObjAlloc] = entryLabel->getCodeLocation();

   return cg->getBinaryBufferCursor() - PPC_INSTRUCTION_LENGTH;
   }

static uint8_t* initializeCCPreLoadedArrayAlloc(uint8_t *buffer, void **CCPreLoadedCodeTable, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR::Node *n = cg->getFirstInstruction()->getNode();

   // Array allocation off TLH (zero or non-zero)
   // If it fails we branch to the regular helper (via it's trampoline)
   // which will return directly to JIT code
   // In:
   //   r3 = class ptr (or identifier for arrays of primitive types)
   //   r4 = number of elements
   //   r5 = data size in bytes (r4 * element size) for variable length arrays
   //        total alloc size in bytes for constant length arrays
   //   r8 = array class ptr
   // If canSkipZeroInit:
   //   r11 = 1 if optimizer says we can skip zero init (allocate on nonZeroTLH)
   // Out:
   //   r3 = array ptr
   //   r4 = number of elements
   // Clobbers:
   //   r5, r8, r10, r11
   //   cr0, cr1, cr2

   cg->setFirstInstruction(NULL);
   cg->setAppendInstruction(NULL);

   TR::Instruction *eyecatcher = generateImmInstruction(cg, TR::InstOpCode::dd, n, CCEYECATCHER('C', 'C', 'H', 1));

   TR::LabelSymbol *entryLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *doneTLHAlloc = generateLabelSymbol(cg);
   TR::LabelSymbol *doNonZeroHeapTLHAlloc = generateLabelSymbol(cg);
   TR::LabelSymbol *constLenArrayAllocLabel = generateLabelSymbol(cg);

   //Branch to VM Helpers for Zeroed TLH.
   TR::LabelSymbol *zeroed_branchToHelperLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *zeroed_branchToPrimArrayHelperLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *zeroed_aNewArrayTrampolineLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *zeroed_newArrayTrampolineLabel = generateLabelSymbol(cg);
   zeroed_aNewArrayTrampolineLabel->setCodeLocation((uint8_t *)TR::CodeCacheManager::instance()->findHelperTrampoline(buffer, TR_aNewArray));
   zeroed_newArrayTrampolineLabel->setCodeLocation((uint8_t *)TR::CodeCacheManager::instance()->findHelperTrampoline(buffer, TR_newArray));

   //Branch to VM Helpers for non-Zeroed TLH.
   TR::LabelSymbol *nonZeroed_branchToHelperLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *nonZeroed_branchToPrimArrayHelperLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *nonZeroed_aNewArrayTrampolineLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *nonZeroed_newArrayTrampolineLabel = generateLabelSymbol(cg);
   nonZeroed_aNewArrayTrampolineLabel->setCodeLocation((uint8_t *)TR::CodeCacheManager::instance()->findHelperTrampoline(buffer, TR_aNewArrayNoZeroInit));
   nonZeroed_newArrayTrampolineLabel->setCodeLocation((uint8_t *)TR::CodeCacheManager::instance()->findHelperTrampoline(buffer, TR_newArrayNoZeroInit));

   TR::Instruction *entry = generateLabelInstruction(cg, TR::InstOpCode::label, n, entryLabel);
   TR::Instruction *cursor = entry;
#if defined(J9VM_INTERP_COMPRESSED_OBJECT_HEADER)
   const TR::InstOpCode::Mnemonic Op_stclass = TR::InstOpCode::stw;
#else
   const TR::InstOpCode::Mnemonic Op_stclass =TR::InstOpCode::Op_st;
#endif

   TR::Register *metaReg = cg->getMethodMetaDataRegister();
   TR::Register *r3 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr3);
   TR::Register *r4 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr4);
   TR::Register *r5 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr5);
   TR::Register *r8 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr8);
   TR::Register *r10 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr10);
   TR::Register *r11 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr11);
   TR::Register *cr0 = cg->machine()->getPPCRealRegister(TR::RealRegister::cr0);
   TR::Register *cr1 = cg->machine()->getPPCRealRegister(TR::RealRegister::cr1);
   TR::Register *cr2 = cg->machine()->getPPCRealRegister(TR::RealRegister::cr2);

   // This is the distance between the end of the heap and the end of the address space,
   // such that adding this value or less to any heap pointer is guaranteed not to overflow
   const uint32_t maxSafeSize = cg->getMaxObjectSizeGuaranteedNotToOverflow();

   // Detect large or negative number of elements in case addr wrap-around
   if (maxSafeSize < 0x00100000)
      {
      cursor = generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, n, cr0, r4, (maxSafeSize >> 6) << 2, cursor);
      cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bge, n, zeroed_branchToHelperLabel, cr0, cursor);
      }
   else
      {
      if (TR::Compiler->target.is64Bit())
         cursor = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicr_r, n, r10, r4, cr0, 0,
                                                  CONSTANT64(0xffffffffffffffff) << (27 - leadingZeroes(maxSafeSize)),
                                                  cursor);
      else
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andis_r, n, r10, r4, cr0,
                                                 (0xffffffff << (11 - leadingZeroes(maxSafeSize))) & 0x0000ffff,
                                                 cursor);
      cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, n, zeroed_branchToHelperLabel, cr0, cursor);
      }
   //2 instructions.

   if (TR::Compiler->om.useHybridArraylets())
      {
      // Check if this array needs to be discontiguous because it is too large or is a zero-length array
      // Zero length arrays are discontiguous (i.e. they also need the discontiguous length field to be 0) because
      // they are indistinguishable from non-zero length discontiguous arrays
      int32_t maxContiguousArraySize = TR::Compiler->om.maxContiguousArraySizeInBytes();
      TR_ASSERT( maxContiguousArraySize >= 0, "Unexpected negative size for max contiguous array size");
      if (maxContiguousArraySize > UPPER_IMMED)
         {
         cursor = loadConstant(cg, n, maxContiguousArraySize, r10, cursor);
         cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::cmpl4, n, cr0, r5, r10, cursor);
         }
      else
         cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli4, n, cr0, r5, maxContiguousArraySize, cursor);
      cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bgt, n, zeroed_branchToHelperLabel, cr0, cursor);
      }

   cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli4, n, cr0, r5, 0, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, n, zeroed_branchToHelperLabel, cr0, cursor);

   // Add header to size and align to OBJECT_ALIGNMENT
   cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r5, r5, sizeof(J9IndexableObjectContiguous) + fej9->getObjectAlignmentInBytes() - 1, cursor);
   cursor = generateTrg1Src1Imm2Instruction(cg, TR::Compiler->target.is64Bit() ? TR::InstOpCode::rldicr : TR::InstOpCode::rlwinm, n, r5, r5, 0, -fej9->getObjectAlignmentInBytes(), cursor);
   // Check if size < J9_GC_MINIMUM_OBJECT_SIZE and set size = J9_GC_MINIMUM_OBJECT_SIZE if so
   cursor = generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, n, cr0, r5, J9_GC_MINIMUM_OBJECT_SIZE, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bge, n, constLenArrayAllocLabel, cr0, cursor);
   cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, n, r5, J9_GC_MINIMUM_OBJECT_SIZE, cursor);
   //5 instructions.

   // At this point r5 should contain the total alloc size in bytes for this array
   cursor = generateLabelInstruction(cg, TR::InstOpCode::label, n, constLenArrayAllocLabel, cursor);

#if defined(DEBUG)
   // Trap if size not word aligned
   cursor = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, n, r10, r5, 0, 3, cursor);
   cursor = generateSrc1Instruction(cg, TR::InstOpCode::twneqi, n, r10, 0, cursor);
   // Trap if size does not preserve object alignment requirements
   cursor = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, n, r10, r5, 0, fej9->getObjectAlignmentInBytes() - 1, cursor);
   cursor = generateSrc1Instruction(cg, TR::InstOpCode::twneqi, n, r10, 0, cursor);
   // Trap if size less than minimum object size
   cursor = generateSrc1Instruction(cg, TR::InstOpCode::twlti, n, r5, J9_GC_MINIMUM_OBJECT_SIZE, cursor);
   // Trap if data size not word aligned
   cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, n, r10, r5, -sizeof(J9IndexableObjectContiguous), cursor);
   cursor = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, n, r10, r10, 0, 3, cursor);
   cursor = generateSrc1Instruction(cg, TR::InstOpCode::twneqi, n, r10, 0, cursor);
#endif

   //Allocate on nonZeroHeapAlloc or heapAlloc based on r11.
   cursor = generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, n, cr2, r11, 1, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, false, n, doNonZeroHeapTLHAlloc, cr2, cursor); //Set likeliness of branch to false.
   //2 instructions.
      {
      //Allocate on zeroed TLH here.

      if (cg->enableTLHPrefetching())
         {
         cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r10,
                                             new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, tlhPrefetchFTA), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                             cursor);
         cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::subf_r, n, r10, r5, r10, cr0, cursor);
         cursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, n,
                                             new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, tlhPrefetchFTA), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                             r10, cursor);
         }
      cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r10,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(struct J9VMThread, heapAlloc), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          cursor);
      cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r11,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(struct J9VMThread, heapTop), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          cursor);
      // New heapAlloc
      cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::add, n, r5, r10, r5, cursor);
      // Check if we exceeded the TLH
      cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr2, r5, r11, cursor);
      cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bgt, n, zeroed_branchToHelperLabel, cr2, cursor);
      // Check if we overflowed
      // For variable-length arrays we already know it won't overflow (see above) however since we're interested in
      // a small i-cache footprint here it's better to have the variable length path fall through to the const length path.
      cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr2, r5, r10, cursor);
      cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::ble, n, zeroed_branchToHelperLabel, cr2, cursor);
      // Store new heapAlloc
      cursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(struct J9VMThread, heapAlloc), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          r5, cursor);
      // Store class ptr
      cursor = generateMemSrc1Instruction(cg,Op_stclass, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(r10, TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceField(), cg),
                                          r8, cursor);
      cursor = generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, n, r3, r10, cursor);
      // Store array length
      cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(r10, fej9->getOffsetOfContiguousArraySizeField(), 4, cg),
                                          r4, cursor);

      if (cg->enableTLHPrefetching())
         {
         TR::LabelSymbol *prefetchHelperLabel = generateLabelSymbol(cg);
         TR_ASSERT( CCPreLoadedCodeTable[TR_AllocPrefetch], "Expecting prefetch helper to be initialized");
         prefetchHelperLabel->setCodeLocation((uint8_t *)CCPreLoadedCodeTable[TR_AllocPrefetch]);
         cursor = generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, n, r8, r3);
         cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::ble, n, prefetchHelperLabel, cr0, cursor);
         }

      //Done allocating on TLH, return to calling code.
      cursor = generateInstruction(cg, TR::InstOpCode::blr, n, cursor);

      //11 instructions (excluding prefetch).
      }
   cursor = generateLabelInstruction(cg, TR::InstOpCode::label, n, doNonZeroHeapTLHAlloc, cursor);
      {
      //Allocate on nonZero TLH here.

      if (cg->enableTLHPrefetching())
         {
         cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r10,
                                             new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, nonZeroTlhPrefetchFTA), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                             cursor);
         cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::subf_r, n, r10, r5, r10, cr0, cursor);
         cursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, n,
                                             new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, nonZeroTlhPrefetchFTA), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                             r10, cursor);
         }

      cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r10,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(struct J9VMThread, nonZeroHeapAlloc), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          cursor);
      cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r11,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(struct J9VMThread, nonZeroHeapTop), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          cursor);
      // New heapAlloc
      cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::add, n, r5, r10, r5, cursor);
      // Check if we exceeded the TLH
      cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr2, r5, r11, cursor);
      cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bgt, n, nonZeroed_branchToHelperLabel, cr2, cursor);
      // Check if we overflowed
      // For variable-length arrays we already know it won't overflow (see above) however since we're interested in
      // a small i-cache footprint here it's better to have the variable length path fall through to the const length path.
      cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr2, r5, r10, cursor);
      cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::ble, n, nonZeroed_branchToHelperLabel, cr2, cursor);
      // Store new heapAlloc
      cursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(struct J9VMThread, nonZeroHeapAlloc), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          r5, cursor);
      // Store class ptr
      cursor = generateMemSrc1Instruction(cg,Op_stclass, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(r10, TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceField(), cg),
                                          r8, cursor);
      cursor = generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, n, r3, r10, cursor);
      // Store array length
      cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, n,
                                          new (cg->trHeapMemory()) TR::MemoryReference(r10, fej9->getOffsetOfContiguousArraySizeField(), 4, cg),
                                          r4, cursor);

      if (cg->enableTLHPrefetching())
         {
         TR::LabelSymbol *prefetchHelperLabel = generateLabelSymbol(cg);
         TR_ASSERT( CCPreLoadedCodeTable[TR_NonZeroAllocPrefetch], "Expecting non-zero tlh prefetch helper to be initialized");
         prefetchHelperLabel->setCodeLocation((uint8_t *)CCPreLoadedCodeTable[TR_NonZeroAllocPrefetch]);
         cursor = generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, n, r8, r3);
         cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::ble, n, prefetchHelperLabel, cr0, cursor);
         }

      //Done allocating on TLH, return to calling code.
      cursor = generateInstruction(cg, TR::InstOpCode::blr, n, cursor);

      //12 instructions (excluding prefetch).
      }

   //Call out to VM helper for Zeroed TLH.
   cursor = generateLabelInstruction(cg, TR::InstOpCode::label, n, zeroed_branchToHelperLabel, cursor);
   cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli4, n, cr0, r3, 11, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::ble, n, zeroed_branchToPrimArrayHelperLabel, cr0, cursor);
   cursor = generateLabelInstruction(cg, TR::InstOpCode::b, n, zeroed_aNewArrayTrampolineLabel, cursor);
   cursor = generateLabelInstruction(cg, TR::InstOpCode::label, n, zeroed_branchToPrimArrayHelperLabel, cursor);
   cursor = generateLabelInstruction(cg, TR::InstOpCode::b, n, zeroed_newArrayTrampolineLabel, cursor);
   //4 instructions.

   //Call out to VM helper for nonZeroed TLH.
   cursor = generateLabelInstruction(cg, TR::InstOpCode::label, n, nonZeroed_branchToHelperLabel, cursor);
   cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli4, n, cr0, r3, 11, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::ble, n, nonZeroed_branchToPrimArrayHelperLabel, cr0, cursor);
   cursor = generateLabelInstruction(cg, TR::InstOpCode::b, n, nonZeroed_aNewArrayTrampolineLabel, cursor);
   cursor = generateLabelInstruction(cg, TR::InstOpCode::label, n, nonZeroed_branchToPrimArrayHelperLabel, cursor);
   cursor = generateLabelInstruction(cg, TR::InstOpCode::b, n, nonZeroed_newArrayTrampolineLabel, cursor);
   //4 instructions.

   cg->setBinaryBufferStart(buffer);
   cg->setBinaryBufferCursor(buffer);

   for (TR::Instruction *i = eyecatcher; i != NULL; i = i->getNext())
      cg->setBinaryBufferCursor(i->generateBinaryEncoding());

   uint32_t helperSize = 25 + 14 + 4 + (TR::Compiler->om.useHybridArraylets() ? (TR::Compiler->om.maxContiguousArraySizeInBytes() > UPPER_IMMED ? 4 : 2) : 0)
      + (cg->enableTLHPrefetching() ? (2*5) : 0);
#ifdef DEBUG
   helperSize += 8;
#endif

   TR_ASSERT(cg->getBinaryBufferCursor() - entryLabel->getCodeLocation() == helperSize * PPC_INSTRUCTION_LENGTH,
             "Per-codecache array allocation helpers, unexpected size");

   CCPreLoadedCodeTable[TR_VariableLenArrayAlloc] = entryLabel->getCodeLocation();
   CCPreLoadedCodeTable[TR_ConstLenArrayAlloc] = constLenArrayAllocLabel->getCodeLocation();

   return cg->getBinaryBufferCursor() - PPC_INSTRUCTION_LENGTH;
   }

static uint8_t* initializeCCPreLoadedWriteBarrier(uint8_t *buffer, void **CCPreLoadedCodeTable, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR::Node *n = cg->getFirstInstruction()->getNode();

   // Write barrier
   // In:
   //   r3 = dst object
   //   r4 = src object
   // Out:
   //   none
   // Clobbers:
   //   r3-r6, r11
   //   cr0, cr1

   cg->setFirstInstruction(NULL);
   cg->setAppendInstruction(NULL);

   TR::Instruction *eyecatcher = generateImmInstruction(cg, TR::InstOpCode::dd, n, CCEYECATCHER('C', 'C', 'H', 2));

   TR::LabelSymbol *entryLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *helperTrampolineLabel = generateLabelSymbol(cg);
   helperTrampolineLabel->setCodeLocation((uint8_t *)TR::CodeCacheManager::instance()->findHelperTrampoline(buffer, TR_writeBarrierStoreGenerational));
   TR::Instruction *entry = generateLabelInstruction(cg, TR::InstOpCode::label, n, entryLabel);
   TR::Instruction *cursor = entry;
#if defined(J9VM_INTERP_COMPRESSED_OBJECT_HEADER)
   const TR::InstOpCode::Mnemonic Op_lclass = TR::InstOpCode::lwz;
#else
   const TR::InstOpCode::Mnemonic Op_lclass =TR::InstOpCode::Op_load;
#endif
   const TR::InstOpCode::Mnemonic rememberedClassMaskOp = J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST > UPPER_IMMED ||
                                               J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST < LOWER_IMMED ? TR::InstOpCode::andis_r : TR::InstOpCode::andi_r;
   const uint32_t      rememberedClassMask = rememberedClassMaskOp == TR::InstOpCode::andis_r ?
                                             J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST >> 16 : J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST;

   TR::Register *metaReg = cg->getMethodMetaDataRegister();
   TR::Register *r3 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr3);
   TR::Register *r4 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr4);
   TR::Register *r5 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr5);
   TR::Register *r6 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr6);
   TR::Register *r11 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr11);
   TR::Register *cr0 = cg->machine()->getPPCRealRegister(TR::RealRegister::cr0);
   TR::Register *cr1 = cg->machine()->getPPCRealRegister(TR::RealRegister::cr1);

   TR_ASSERT((J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST <= UPPER_IMMED && J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST >= LOWER_IMMED) ||
           (J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST & 0xffff) == 0, "Expecting J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST to fit in immediate field");

   const bool constHeapBase = !comp->getOptions()->isVariableHeapBaseForBarrierRange0();
   const bool constHeapSize = !comp->getOptions()->isVariableHeapSizeForBarrierRange0();
   intptrj_t heapBase;
   intptrj_t heapSize;

   if (TR::Compiler->target.is32Bit() && !comp->compileRelocatableCode() && constHeapBase)
      {
      heapBase = comp->getOptions()->getHeapBaseForBarrierRange0();
      cursor = loadAddressConstant(cg, n, heapBase, r5, cursor);
      }
   else
      cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r5,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(struct J9VMThread, heapBaseForBarrierRange0), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          cursor);
   if (TR::Compiler->target.is32Bit() && !comp->compileRelocatableCode() && constHeapSize)
      {
      heapSize = comp->getOptions()->getHeapSizeForBarrierRange0();
      cursor = loadAddressConstant(cg, n, heapSize, r6, cursor);
      }
   else
      cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r6,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(struct J9VMThread, heapSizeForBarrierRange0), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          cursor);
   cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, n, r11, r5, r3, cursor);
   cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr0, r11, r6, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bgelr, n, NULL, cr0, cursor);
   cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, n, r11, r5, r4, cursor);
   cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr0, r11, r6, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bltlr, n, NULL, cr0, cursor);
   cursor = generateTrg1MemInstruction(cg,Op_lclass, n, r11,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r3, TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceField(), cg),
                                       cursor);
   cursor = generateTrg1Src1ImmInstruction(cg, rememberedClassMaskOp, n, r11, r11, cr0, rememberedClassMask, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bnelr, n, NULL, cr0, cursor);
   cursor = generateLabelInstruction(cg, TR::InstOpCode::b, n, helperTrampolineLabel, cursor);

   cg->setBinaryBufferStart(buffer);
   cg->setBinaryBufferCursor(buffer);

   for (TR::Instruction *i = eyecatcher; i != NULL; i = i->getNext())
      cg->setBinaryBufferCursor(i->generateBinaryEncoding());

   const uint32_t helperSize = 12 +
      (TR::Compiler->target.is32Bit() && !comp->compileRelocatableCode() && constHeapBase && heapBase > UPPER_IMMED && heapBase < LOWER_IMMED ? 1 : 0) +
      (TR::Compiler->target.is32Bit() && !comp->compileRelocatableCode() && constHeapSize && heapSize > UPPER_IMMED && heapSize < LOWER_IMMED ? 1 : 0);
   TR_ASSERT(cg->getBinaryBufferCursor() - entryLabel->getCodeLocation() == helperSize * PPC_INSTRUCTION_LENGTH,
           "Per-codecache write barrier, unexpected size");

   CCPreLoadedCodeTable[TR_writeBarrier] = entryLabel->getCodeLocation();

   return cg->getBinaryBufferCursor() - PPC_INSTRUCTION_LENGTH;
   }

static uint8_t* initializeCCPreLoadedWriteBarrierAndCardMark(uint8_t *buffer, void **CCPreLoadedCodeTable, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR::Node *n = cg->getFirstInstruction()->getNode();

   // Write barrier and card mark
   // In:
   //   r3 = dst object
   //   r4 = src object
   // Out:
   //   none
   // Clobbers:
   //   r3-r6, r11
   //   cr0, cr1

   cg->setFirstInstruction(NULL);
   cg->setAppendInstruction(NULL);

   TR::Instruction *eyecatcher = generateImmInstruction(cg, TR::InstOpCode::dd, n, CCEYECATCHER('C', 'C', 'H', 3));

   TR::LabelSymbol *entryLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *doneCardMarkLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *helperTrampolineLabel = generateLabelSymbol(cg);
   helperTrampolineLabel->setCodeLocation((uint8_t *)TR::CodeCacheManager::instance()->findHelperTrampoline(buffer, TR_writeBarrierStoreGenerationalAndConcurrentMark));
   TR::Instruction *entry = generateLabelInstruction(cg, TR::InstOpCode::label, n, entryLabel);
   TR::Instruction *cursor = entry;
#if defined(J9VM_INTERP_COMPRESSED_OBJECT_HEADER)
   const TR::InstOpCode::Mnemonic Op_lclass = TR::InstOpCode::lwz;
#else
   const TR::InstOpCode::Mnemonic Op_lclass =TR::InstOpCode::Op_load;
#endif
   const TR::InstOpCode::Mnemonic cmActiveMaskOp = J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE > UPPER_IMMED ||
                                        J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE < LOWER_IMMED ? TR::InstOpCode::andis_r : TR::InstOpCode::andi_r;
   const uint32_t      cmActiveMask = cmActiveMaskOp == TR::InstOpCode::andis_r ?
                                      J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE >> 16 : J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE;
   const TR::InstOpCode::Mnemonic rememberedClassMaskOp = J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST > UPPER_IMMED ||
                                               J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST < LOWER_IMMED ? TR::InstOpCode::andis_r : TR::InstOpCode::andi_r;
   const uint32_t      rememberedClassMask = rememberedClassMaskOp == TR::InstOpCode::andis_r ?
                                             J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST >> 16 : J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST;
   const uintptr_t     cardTableShift = TR::Compiler->target.is64Bit() ?
                                        trailingZeroes((uint64_t)comp->getOptions()->getGcCardSize()) :
                                        trailingZeroes((uint32_t)comp->getOptions()->getGcCardSize());

   TR::Register *metaReg = cg->getMethodMetaDataRegister();
   TR::Register *r3 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr3);
   TR::Register *r4 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr4);
   TR::Register *r5 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr5);
   TR::Register *r6 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr6);
   TR::Register *r11 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr11);
   TR::Register *cr0 = cg->machine()->getPPCRealRegister(TR::RealRegister::cr0);
   TR::Register *cr1 = cg->machine()->getPPCRealRegister(TR::RealRegister::cr1);

   TR_ASSERT((J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE <= UPPER_IMMED && J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE >= LOWER_IMMED) ||
           (J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE & 0xffff) == 0, "Expecting J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE to fit in immediate field");
   TR_ASSERT((J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST <= UPPER_IMMED && J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST >= LOWER_IMMED) ||
           (J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST & 0xffff) == 0, "Expecting J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST to fit in immediate field");

   const bool constHeapBase = !comp->getOptions()->isVariableHeapBaseForBarrierRange0();
   const bool constHeapSize = !comp->getOptions()->isVariableHeapSizeForBarrierRange0();
   intptrj_t heapBase;
   intptrj_t heapSize;

   if (TR::Compiler->target.is32Bit() && !comp->compileRelocatableCode() && constHeapBase)
      {
      heapBase = comp->getOptions()->getHeapBaseForBarrierRange0();
      cursor = loadAddressConstant(cg, n, heapBase, r5, cursor);
      }
   else
      cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r5,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(struct J9VMThread, heapBaseForBarrierRange0), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          cursor);
   if (TR::Compiler->target.is32Bit() && !comp->compileRelocatableCode() && constHeapSize)
      {
      heapSize = comp->getOptions()->getHeapSizeForBarrierRange0();
      cursor = loadAddressConstant(cg, n, heapSize, r6, cursor);
      }
   else
      cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r6,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(struct J9VMThread, heapSizeForBarrierRange0), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                          cursor);
   cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, n, r11, r5, r3, cursor);
   cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr0, r11, r6, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bgelr, n, NULL, cr0, cursor);
   cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, n, r5, r5, r4, cursor);
   cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr1, r5, r6, cursor);
   cursor = generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, n, r6,
                                       new (cg->trHeapMemory()) TR::MemoryReference(metaReg,  offsetof(struct J9VMThread, privateFlags), 4, cg),
                                       cursor);
   cursor = generateTrg1Src1ImmInstruction(cg, cmActiveMaskOp, n, r6, r6, cr0, cmActiveMask, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, n, doneCardMarkLabel, cr0, cursor);
   cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r6,
                                       new (cg->trHeapMemory()) TR::MemoryReference(metaReg,  offsetof(struct J9VMThread, activeCardTableBase), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                       cursor);
   if (TR::Compiler->target.is64Bit())
      cursor = generateShiftRightLogicalImmediateLong(cg, n, r11, r11, cardTableShift, cursor);
   else
      cursor = generateShiftRightLogicalImmediate(cg, n, r11, r11, cardTableShift, cursor);
   cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, n, r5, 1, cursor);
   cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stbx, n,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r6, r11, 1, cg),
                                       r5, cursor);
   cursor = generateLabelInstruction(cg, TR::InstOpCode::label, n, doneCardMarkLabel, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bltlr, n, NULL, cr1, cursor);
   cursor = generateTrg1MemInstruction(cg,Op_lclass, n, r11,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r3, TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceField(), cg),
                                       cursor);
   cursor = generateTrg1Src1ImmInstruction(cg, rememberedClassMaskOp, n, r11, r11, cr0, rememberedClassMask, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bnelr, n, NULL, cr0, cursor);
   cursor = generateLabelInstruction(cg, TR::InstOpCode::b, n, helperTrampolineLabel, cursor);

   cg->setBinaryBufferStart(buffer);
   cg->setBinaryBufferCursor(buffer);

   for (TR::Instruction *i = eyecatcher; i != NULL; i = i->getNext())
      cg->setBinaryBufferCursor(i->generateBinaryEncoding());

   const uint32_t helperSize = 19 +
      (TR::Compiler->target.is32Bit() && !comp->compileRelocatableCode() && constHeapBase && heapBase > UPPER_IMMED && heapBase < LOWER_IMMED ? 1 : 0) +
      (TR::Compiler->target.is32Bit() && !comp->compileRelocatableCode() && constHeapSize && heapSize > UPPER_IMMED && heapSize < LOWER_IMMED ? 1 : 0);
   TR_ASSERT(cg->getBinaryBufferCursor() - entryLabel->getCodeLocation() == helperSize * PPC_INSTRUCTION_LENGTH,
           "Per-codecache write barrier with card mark, unexpected size");

   CCPreLoadedCodeTable[TR_writeBarrierAndCardMark] = entryLabel->getCodeLocation();

   return cg->getBinaryBufferCursor() - PPC_INSTRUCTION_LENGTH;
   }

static uint8_t* initializeCCPreLoadedCardMark(uint8_t *buffer, void **CCPreLoadedCodeTable, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node *n = cg->getFirstInstruction()->getNode();

   // Card mark
   // In:
   //   r3 = dst object
   // Out:
   //   none
   // Clobbers:
   //   r4-r5, r11
   //   cr0

   cg->setFirstInstruction(NULL);
   cg->setAppendInstruction(NULL);

   TR::Instruction *eyecatcher = generateImmInstruction(cg, TR::InstOpCode::dd, n, CCEYECATCHER('C', 'C', 'H', 4));

   TR::LabelSymbol *entryLabel = generateLabelSymbol(cg);
   TR::Instruction *entry = generateLabelInstruction(cg, TR::InstOpCode::label, n, entryLabel);
   TR::Instruction *cursor = entry;
   const TR::InstOpCode::Mnemonic cmActiveMaskOp = J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE > UPPER_IMMED ||
                                        J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE < LOWER_IMMED ? TR::InstOpCode::andis_r : TR::InstOpCode::andi_r;
   const uint32_t      cmActiveMask = cmActiveMaskOp == TR::InstOpCode::andis_r ?
                                      J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE >> 16 : J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE;
   const uintptr_t     cardTableShift = TR::Compiler->target.is64Bit() ?
                                        trailingZeroes((uint64_t)comp->getOptions()->getGcCardSize()) :
                                        trailingZeroes((uint32_t)comp->getOptions()->getGcCardSize());

   TR::Register *metaReg = cg->getMethodMetaDataRegister();
   TR::Register *r3 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr3);
   TR::Register *r4 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr4);
   TR::Register *r5 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr5);
   TR::Register *r11 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr11);
   TR::Register *cr0 = cg->machine()->getPPCRealRegister(TR::RealRegister::cr0);

   TR_ASSERT((J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE <= UPPER_IMMED && J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE >= LOWER_IMMED) ||
                       (J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE & 0xffff) == 0, "Expecting J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE to fit in immediate field");

   cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r5,
                                       new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(struct J9VMThread, heapBaseForBarrierRange0), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                       cursor);
   cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r4,
                                       new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(struct J9VMThread, heapSizeForBarrierRange0), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                       cursor);
   cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, n, r5, r5, r3, cursor);
   cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr0, r5, r4, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bgelr, n, NULL, cr0, cursor);
   // Incremental (i.e. balanced) always dirties the card
   if (comp->getOptions()->getGcMode() != TR_WrtbarCardMarkIncremental)
      {
      cursor = generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, n, r4,
                                          new (cg->trHeapMemory()) TR::MemoryReference(metaReg,  offsetof(struct J9VMThread, privateFlags), 4, cg),
                                          cursor);
      cursor = generateTrg1Src1ImmInstruction(cg, cmActiveMaskOp, n, r4, r4, cr0, cmActiveMask, cursor);
      cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beqlr, n, NULL, cr0, cursor);
      }
   cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r4,
                                       new (cg->trHeapMemory()) TR::MemoryReference(metaReg,  offsetof(struct J9VMThread, activeCardTableBase), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                       cursor);
   if (TR::Compiler->target.is64Bit())
      cursor = generateShiftRightLogicalImmediateLong(cg, n, r5, r5, cardTableShift, cursor);
   else
      cursor = generateShiftRightLogicalImmediate(cg, n, r5, r5, cardTableShift, cursor);
   cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, n, r11, 1, cursor);
   cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stbx, n,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r4, r5, 1, cg),
                                       r11, cursor);
   cursor = generateInstruction(cg, TR::InstOpCode::blr, n, cursor);

   cg->setBinaryBufferStart(buffer);
   cg->setBinaryBufferCursor(buffer);

   for (TR::Instruction *i = eyecatcher; i != NULL; i = i->getNext())
      cg->setBinaryBufferCursor(i->generateBinaryEncoding());

   const uint32_t helperSize = comp->getOptions()->getGcMode() != TR_WrtbarCardMarkIncremental ? 13 : 10;
   TR_ASSERT(cg->getBinaryBufferCursor() - entryLabel->getCodeLocation() == helperSize * PPC_INSTRUCTION_LENGTH,
           "Per-codecache card mark, unexpected size");

   CCPreLoadedCodeTable[TR_cardMark] = entryLabel->getCodeLocation();

   return cg->getBinaryBufferCursor() - PPC_INSTRUCTION_LENGTH;
   }

static uint8_t* initializeCCPreLoadedArrayStoreCHK(uint8_t *buffer, void **CCPreLoadedCodeTable, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR::Node *n = cg->getFirstInstruction()->getNode();

   // Array store check
   // In:
   //   r3 = dst object
   //   r4 = src object
   //   r11 = root class (j/l/Object)
   // Out:
   //   none
   // Clobbers:
   //   r5-r7, r11
   //   cr0

   cg->setFirstInstruction(NULL);
   cg->setAppendInstruction(NULL);

   TR::Instruction *eyecatcher = generateImmInstruction(cg, TR::InstOpCode::dd, n, CCEYECATCHER('C', 'C', 'H', 7));

   TR::LabelSymbol *entryLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *skipSuperclassTestLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *helperTrampolineLabel = generateLabelSymbol(cg);
   helperTrampolineLabel->setCodeLocation((uint8_t *)TR::CodeCacheManager::instance()->findHelperTrampoline(buffer, TR_typeCheckArrayStore));
   TR::Instruction *entry = generateLabelInstruction(cg, TR::InstOpCode::label, n, entryLabel);
   TR::Instruction *cursor = entry;
#if defined(J9VM_INTERP_COMPRESSED_OBJECT_HEADER)
   const TR::InstOpCode::Mnemonic Op_lclass = TR::InstOpCode::lwz;
#else
   const TR::InstOpCode::Mnemonic Op_lclass =TR::InstOpCode::Op_load;
#endif

   TR::Register *r3 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr3);
   TR::Register *r4 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr4);
   TR::Register *r5 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr5);
   TR::Register *r6 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr6);
   TR::Register *r7 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr7);
   TR::Register *r11 = cg->machine()->getPPCRealRegister(TR::RealRegister::gr11);
   TR::Register *cr0 = cg->machine()->getPPCRealRegister(TR::RealRegister::cr0);

   cursor = generateTrg1MemInstruction(cg,Op_lclass, n, r5,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r3, TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceField(), cg),
                                       cursor);
   cursor = generateTrg1MemInstruction(cg,Op_lclass, n, r6,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r4, TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceField(), cg),
                                       cursor);
   cursor = TR::TreeEvaluator::generateVFTMaskInstruction(cg, n, r5, cursor);
   cursor = TR::TreeEvaluator::generateVFTMaskInstruction(cg, n, r6, cursor);
   cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r5,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r5, offsetof(J9ArrayClass, componentType), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                       cursor);

   cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr0, r5, r6, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beqlr, n, NULL, cr0, cursor);
   cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r7,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r6, offsetof(J9Class, castClassCache), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                       cursor);
   cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr0, r5, r7, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beqlr, n, NULL, cr0, cursor);
   cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr0, r5, r11, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beqlr, n, NULL, cr0, cursor);

   cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r7,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r5, offsetof(J9Class, romClass), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                       cursor);
   cursor = generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, n, r7,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r7, offsetof(J9ROMClass, modifiers), 4, cg),
                                       cursor);
   cursor = generateShiftRightLogicalImmediate(cg, n, r7, r7, 1, cursor);
   TR_ASSERT(!(((J9_JAVA_CLASS_ARRAY | J9_JAVA_INTERFACE) >> 1) & ~0xffff),
           "Expecting shifted ROM class modifiers to fit in immediate");
   cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, n, r7, r7, cr0, (J9_JAVA_CLASS_ARRAY | J9_JAVA_INTERFACE) >> 1, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, n, skipSuperclassTestLabel, cr0, cursor);

   cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r7,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r6, offsetof(J9Class, classDepthAndFlags), TR::Compiler->om.sizeofReferenceAddress(), cg),
                                       cursor);
   TR_ASSERT(!(J9_JAVA_CLASS_DEPTH_MASK & ~0xffff),
           "Expecting class depth mask to fit in immediate");
   cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, n, r7, r7, cr0, J9_JAVA_CLASS_DEPTH_MASK, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, n, skipSuperclassTestLabel, cr0, cursor);
   cursor = generateTrg1MemInstruction(cg,Op_lclass, n, r7,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r6, offsetof(J9Class, superclasses), TR::Compiler->om.sizeofReferenceField(), cg),
                                       cursor);
   cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, n, r7,
                                       new (cg->trHeapMemory()) TR::MemoryReference(r7, 0, TR::Compiler->om.sizeofReferenceAddress(), cg),
                                       cursor);
   cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, n, cr0, r5, r7, cursor);
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beqlr, n, NULL, cr0, cursor);

   cursor = generateLabelInstruction(cg, TR::InstOpCode::label, n, skipSuperclassTestLabel, cursor);
   cursor = generateLabelInstruction(cg, TR::InstOpCode::b, n, helperTrampolineLabel, cursor);

   cg->setBinaryBufferStart(buffer);
   cg->setBinaryBufferCursor(buffer);

   for (TR::Instruction *i = eyecatcher; i != NULL; i = i->getNext())
      cg->setBinaryBufferCursor(i->generateBinaryEncoding());

   const uint32_t helperSize = 25;
   TR_ASSERT(cg->getBinaryBufferCursor() - entryLabel->getCodeLocation() == helperSize * PPC_INSTRUCTION_LENGTH,
           "Per-codecache array store check, unexpected size");

   CCPreLoadedCodeTable[TR_arrayStoreCHK] = entryLabel->getCodeLocation();

   return cg->getBinaryBufferCursor() - PPC_INSTRUCTION_LENGTH;
   }

void TR::createCCPreLoadedCode(uint8_t *CCPreLoadedCodeBase, uint8_t *CCPreLoadedCodeTop, void **CCPreLoadedCodeTable, TR::CodeGenerator *cg)
   {
   /* If you modify this make sure you update CCPreLoadedCodeSize above as well */

   if (TR::Options::getCmdLineOptions()->realTimeGC())
      return;

   // We temporarily clobber the first and append instructions so we can use high level codegen to generate pre-loaded code
   // So save the original values here and restore them when done
   TR::Compilation *comp = cg->comp();
   TR::Instruction *curFirst = cg->getFirstInstruction();
   TR::Instruction *curAppend = cg->getAppendInstruction();
   uint8_t *curBinaryBufferStart = cg->getBinaryBufferStart();
   uint8_t *curBinaryBufferCursor = cg->getBinaryBufferCursor();

   uint8_t *buffer = (uint8_t *)CCPreLoadedCodeBase;

   buffer = initializeCCPreLoadedPrefetch(buffer, CCPreLoadedCodeTable, cg);
   buffer = initializeCCPreLoadedNonZeroPrefetch(buffer + PPC_INSTRUCTION_LENGTH, CCPreLoadedCodeTable, cg);
   buffer = initializeCCPreLoadedObjectAlloc(buffer + PPC_INSTRUCTION_LENGTH, CCPreLoadedCodeTable, cg);
   buffer = initializeCCPreLoadedArrayAlloc(buffer + PPC_INSTRUCTION_LENGTH, CCPreLoadedCodeTable, cg);
   buffer = initializeCCPreLoadedWriteBarrier(buffer + PPC_INSTRUCTION_LENGTH, CCPreLoadedCodeTable, cg);
   if (comp->getOptions()->getGcCardSize() > 0)
      {
      buffer = initializeCCPreLoadedWriteBarrierAndCardMark(buffer + PPC_INSTRUCTION_LENGTH, CCPreLoadedCodeTable, cg);
      buffer = initializeCCPreLoadedCardMark(buffer + PPC_INSTRUCTION_LENGTH, CCPreLoadedCodeTable, cg);
      }
   buffer = initializeCCPreLoadedArrayStoreCHK(buffer + PPC_INSTRUCTION_LENGTH, CCPreLoadedCodeTable, cg);

   // Other Code Cache Helper Initialization will go here

   TR_ASSERT(buffer <= (uint8_t*)CCPreLoadedCodeTop, "Exceeded CodeCache Helper Area");

   // Apply all of our relocations now before we sync
   TR::list<TR::Relocation*> &relocs = cg->getRelocationList();
   auto iterator = relocs.begin();
   while (iterator != relocs.end())
      {
      if ((*iterator)->getUpdateLocation() >= CCPreLoadedCodeBase &&
          (*iterator)->getUpdateLocation() <= CCPreLoadedCodeTop)
         {
         (*iterator)->apply(cg);
         iterator = relocs.erase(iterator);
         }
      else
    	  ++iterator;
      }

#if defined(TR_HOST_POWER)
   ppcCodeSync((uint8_t *)CCPreLoadedCodeBase, buffer - (uint8_t *)CCPreLoadedCodeBase + 1);
#endif

   cg->setFirstInstruction(curFirst);
   cg->setAppendInstruction(curAppend);
   cg->setBinaryBufferStart(curBinaryBufferStart);
   cg->setBinaryBufferCursor(curBinaryBufferCursor);

   }

uint8_t *TR::PPCAllocPrefetchSnippet::emitSnippetBody()
   {
   TR::Compilation *comp = cg()->comp();
   uint8_t *buffer = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(buffer);
   TR::InstOpCode opcode;

   if (TR::Options::getCmdLineOptions()->realTimeGC())
      return NULL;

   TR_ASSERT((uintptrj_t)((comp->getCurrentCodeCache())->getCCPreLoadedCodeAddress(TR_AllocPrefetch, cg())) != 0xDEADBEEF,
         "Invalid addr for code cache helper");
   intptrj_t distance = (intptrj_t)(comp->getCurrentCodeCache())->getCCPreLoadedCodeAddress(TR_AllocPrefetch, cg())
                           - (intptrj_t)buffer;
   opcode.setOpCodeValue(TR::InstOpCode::b);
   buffer = opcode.copyBinaryToBuffer(buffer);
   *(int32_t *)buffer |= distance & 0x03FFFFFC;
   return buffer+PPC_INSTRUCTION_LENGTH;
   }

void
TR::PPCAllocPrefetchSnippet::print(TR::FILE *pOutFile, TR_Debug * debug)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());
   uint8_t *cursor = getSnippetLabel()->getCodeLocation();

   debug->printSnippetLabel(pOutFile, getSnippetLabel(), cursor, "Allocation Prefetch Snippet");

   int32_t  distance;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   distance = *((int32_t *) cursor) & 0x03fffffc;
   distance = (distance << 6) >> 6;   // sign extend
   trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t\t", (intptrj_t)cursor + distance);
   }

uint32_t TR::PPCAllocPrefetchSnippet::getLength(int32_t estimatedCodeStart)
   {

   if (TR::Options::getCmdLineOptions()->realTimeGC())
      return 0;

   return PPC_INSTRUCTION_LENGTH;
   }

TR::PPCNonZeroAllocPrefetchSnippet::PPCNonZeroAllocPrefetchSnippet(
   TR::CodeGenerator   *codeGen,
   TR::Node            *node,
   TR::LabelSymbol      *callLabel)
     : TR::Snippet(codeGen, node, callLabel, false)
   {
   }

uint8_t *TR::PPCNonZeroAllocPrefetchSnippet::emitSnippetBody()
   {
   TR::Compilation *comp = cg()->comp();
   uint8_t *buffer = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(buffer);
   TR::InstOpCode opcode;

   if (TR::Options::getCmdLineOptions()->realTimeGC())
      return NULL;

   TR_ASSERT((uintptrj_t)((comp->getCurrentCodeCache())->getCCPreLoadedCodeAddress(TR_NonZeroAllocPrefetch, cg())) != 0xDEADBEEF,
         "Invalid addr for code cache helper");
   intptrj_t distance = (intptrj_t)(comp->getCurrentCodeCache())->getCCPreLoadedCodeAddress(TR_NonZeroAllocPrefetch, cg())
                           - (intptrj_t)buffer;
   opcode.setOpCodeValue(TR::InstOpCode::b);
   buffer = opcode.copyBinaryToBuffer(buffer);
   *(int32_t *)buffer |= distance & 0x03FFFFFC;
   return buffer+PPC_INSTRUCTION_LENGTH;
   }

void
TR::PPCNonZeroAllocPrefetchSnippet::print(TR::FILE *pOutFile, TR_Debug * debug)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());
   uint8_t *cursor = getSnippetLabel()->getCodeLocation();

   debug->printSnippetLabel(pOutFile, getSnippetLabel(), cursor, "Non Zero TLH Allocation Prefetch Snippet");

   int32_t  distance;

   debug->printPrefix(pOutFile, NULL, cursor, 4);
   distance = *((int32_t *) cursor) & 0x03fffffc;
   distance = (distance << 6) >> 6;   // sign extend
   trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t\t", (intptrj_t)cursor + distance);
   }

uint32_t TR::PPCNonZeroAllocPrefetchSnippet::getLength(int32_t estimatedCodeStart)
   {

   if (TR::Options::getCmdLineOptions()->realTimeGC())
      return 0;

   return PPC_INSTRUCTION_LENGTH;
   }

