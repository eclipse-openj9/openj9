/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#include "p/codegen/InterfaceCastSnippet.hpp"

#include "codegen/Machine.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "p/codegen/PPCTableOfConstants.hpp"

TR::PPCInterfaceCastSnippet::PPCInterfaceCastSnippet(TR::CodeGenerator * cg, TR::Node * n, TR::LabelSymbol *restartLabel, TR::LabelSymbol *snippetLabel, TR::LabelSymbol *trueLabel, TR::LabelSymbol *falseLabel, TR::LabelSymbol *doneLabel, TR::LabelSymbol *callLabel, bool testCastClassIsSuper, bool checkCast, int32_t offsetClazz, int32_t offsetCastClassCache, bool needsResult)
   : TR::Snippet(cg, n, snippetLabel, false), _restartLabel(restartLabel), _trueLabel(trueLabel), _falseLabel(falseLabel), _doneLabel(doneLabel), _callLabel(callLabel), _testCastClassIsSuper(testCastClassIsSuper), _checkCast(checkCast), _offsetClazz(offsetClazz), _offsetCastClassCache(offsetCastClassCache), _needsResult(needsResult)
   {
   }

uint8_t *
TR::PPCInterfaceCastSnippet::emitSnippetBody()
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());

   uint8_t * cursor = cg()->getBinaryBufferCursor();

   TR::InstOpCode opcode;
   int32_t branchDistance;

   getSnippetLabel()->setCodeLocation(cursor);

   if (_checkCast)
      {
      TR::RegisterDependencyConditions *deps = _doneLabel->getInstruction()->getDependencyConditions();
      TR::RealRegister *objReg       = cg()->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(0)->getRealRegister());
      TR::RealRegister *castClassReg = cg()->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(1)->getRealRegister());
      TR::RealRegister *objClassReg  = cg()->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(2)->getRealRegister());
      TR::RealRegister *cndReg       = cg()->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(3)->getRealRegister());
      TR::RealRegister *scratch1Reg  = cg()->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(4)->getRealRegister());

      // checkcast

      // TODO: at present _testCastClassIsSuper is always false

      // lwz/ld  scratch1Reg, objReg(_offsetClazz)
      // lwz/ld  scratch1Reg, scratch1Reg(_offsetCastClassCache)
      // cmpl4/cmpl8 cr0, castClassReg, scratch1Reg
      // if (_testCastClassIsSuper)
      //    beq   cr0, 8 (_doneLabel)
      //    b restartLabel
      // 8: b _doneLabel
      // else
      //    bne   cr0, 8 (_callLabel)
      //    b doneLabel
      // 8: b _callLabel

      if (TR::Compiler->target.is64Bit() && !TR::Compiler->om.generateCompressedObjectHeaders())
         opcode.setOpCodeValue(TR::InstOpCode::ld);
      else
         opcode.setOpCodeValue(TR::InstOpCode::lwz);
      cursor = opcode.copyBinaryToBuffer(cursor);
      scratch1Reg->setRegisterFieldRT((uint32_t *)cursor);
      objReg->setRegisterFieldRA((uint32_t *)cursor);
      *(int32_t *)cursor |= _offsetClazz & 0xffff;
      cursor += PPC_INSTRUCTION_LENGTH;

      if (TR::Compiler->target.is64Bit())
         opcode.setOpCodeValue(TR::InstOpCode::ld);
      else
         opcode.setOpCodeValue(TR::InstOpCode::lwz);
      cursor = opcode.copyBinaryToBuffer(cursor);
      scratch1Reg->setRegisterFieldRT((uint32_t *)cursor);
      scratch1Reg->setRegisterFieldRA((uint32_t *)cursor);
      *(int32_t *)cursor |= _offsetCastClassCache & 0xffff;
      cursor += PPC_INSTRUCTION_LENGTH;

      if (TR::Compiler->target.is64Bit())
         opcode.setOpCodeValue(TR::InstOpCode::cmpl8);
      else
         opcode.setOpCodeValue(TR::InstOpCode::cmpl4);
      cursor = opcode.copyBinaryToBuffer(cursor);
      cndReg->setRegisterFieldRT((uint32_t *)cursor);
      castClassReg->setRegisterFieldRA((uint32_t *)cursor);
      scratch1Reg->setRegisterFieldRB((uint32_t *)cursor);
      cursor += PPC_INSTRUCTION_LENGTH;

      if (_testCastClassIsSuper)
         {
         opcode.setOpCodeValue(TR::InstOpCode::beq);
         cursor = opcode.copyBinaryToBuffer(cursor);
         cndReg->setRegisterFieldBI((uint32_t *)cursor);
         branchDistance = (intptrj_t) 8; // Jump two instructions below
         *(int32_t *)cursor |= (branchDistance & 0x0fffc);
         cursor += PPC_INSTRUCTION_LENGTH;

         opcode.setOpCodeValue(TR::InstOpCode::b);
         cursor = opcode.copyBinaryToBuffer(cursor);
         branchDistance = (intptrj_t)getReStartLabel()->getCodeLocation() - (intptrj_t)cursor;
         TR_ASSERT(TR::Compiler->target.cpu.isTargetWithinIFormBranchRange((intptrj_t)getReStartLabel()->getCodeLocation(), (intptrj_t)cursor),
                   "backward jump in Interface Cache is too long\n");
         *(int32_t *)cursor |= (branchDistance & 0x03fffffc);
         cursor += PPC_INSTRUCTION_LENGTH;

         opcode.setOpCodeValue(TR::InstOpCode::b);
         cursor = opcode.copyBinaryToBuffer(cursor);
         branchDistance = (intptrj_t)_doneLabel->getCodeLocation() - (intptrj_t)cursor;
         TR_ASSERT(TR::Compiler->target.cpu.isTargetWithinIFormBranchRange((intptrj_t)_doneLabel->getCodeLocation(), (intptrj_t)cursor),
                   "backward jump in Interface Cache is too long\n");
         *(int32_t *)cursor |= (branchDistance & 0x03fffffc);
         cursor += PPC_INSTRUCTION_LENGTH;
         }
      else
         {
         opcode.setOpCodeValue(TR::InstOpCode::bne);
         cursor = opcode.copyBinaryToBuffer(cursor);
         cndReg->setRegisterFieldBI((uint32_t *)cursor);
         branchDistance = (intptrj_t) 8; // Jump two instructions below
         *(int32_t *)cursor |= (branchDistance & 0x0fffc);
         cursor += PPC_INSTRUCTION_LENGTH;

         opcode.setOpCodeValue(TR::InstOpCode::b);
         cursor = opcode.copyBinaryToBuffer(cursor);
         branchDistance = (intptrj_t)getDoneLabel()->getCodeLocation() - (intptrj_t)cursor;
         TR_ASSERT(TR::Compiler->target.cpu.isTargetWithinIFormBranchRange((intptrj_t)getDoneLabel()->getCodeLocation(), (intptrj_t)cursor),
                   "backward jump in Interface Cache is too long\n");
         *(int32_t *)cursor |= (branchDistance & 0x03fffffc);
         cursor += PPC_INSTRUCTION_LENGTH;

         opcode.setOpCodeValue(TR::InstOpCode::b);
         cursor = opcode.copyBinaryToBuffer(cursor);
         branchDistance = (intptrj_t)_callLabel->getCodeLocation() - (intptrj_t)cursor;
         TR_ASSERT(TR::Compiler->target.cpu.isTargetWithinIFormBranchRange((intptrj_t)_callLabel->getCodeLocation(), (intptrj_t)cursor),
                   "backward jump in Interface Cache is too long\n");
         *(int32_t *)cursor |= (branchDistance & 0x03fffffc);
         cursor += PPC_INSTRUCTION_LENGTH;
         }
      }
   else // if not _checkcast, then it is instanceof
      {
      auto firstArgReg = getNode()->getSymbol()->castToMethodSymbol()->getLinkageConvention() == TR_CHelper ? TR::RealRegister::gr4 : TR::RealRegister::gr3;
      TR::RegisterDependencyConditions *deps = _doneLabel->getInstruction()->getDependencyConditions();
      TR::RealRegister *castClassReg = cg()->machine()->getRealRegister(static_cast<TR::RealRegister::RegNum>(firstArgReg));
      TR::RealRegister *objClassReg = cg()->machine()->getRealRegister(static_cast<TR::RealRegister::RegNum>(firstArgReg + 3));
      TR::RealRegister *resultReg = cg()->machine()->getRealRegister(static_cast<TR::RealRegister::RegNum>(firstArgReg + 5));
      TR::RealRegister *cndReg = cg()->machine()->getRealRegister(TR::RealRegister::cr0);
      TR::RealRegister *scratch1Reg = cg()->machine()->getRealRegister(static_cast<TR::RealRegister::RegNum>(firstArgReg + 2));
      TR::RealRegister *scratch2Reg = cg()->machine()->getRealRegister(static_cast<TR::RealRegister::RegNum>(firstArgReg + 4));

      // instanceof

      // lwz/ld   scratch2Reg, objClassReg(_offsetCastClassCache)
      // if (TR::Compiler->target.is64Bit())
      //    sh=0
      //    me=62
      //    rldicr scratch1Reg, scratch2Reg, 0, 0x3d
      // else
      //    rlwinm scratch1Reg, scratch2Reg, 0, 0xFFFF FFFE
      // cmpl  cr0, scratch1Reg, castClassReg
      // if (_testCastClassIsSuper)
      //    beq   cr0, 8 (continue)
      //    b     restartLabel
      // else
      //    beq   cr0, 8 (continue)
      //    b     callLabel
      // 8:
      // if (_needsResult)
      //    li   scratch1Reg, 0x1
      //    or   scratch1Reg, scratch1Reg, scratch2Reg
      //    xor. resultReg, scratch1Reg, scratch2Reg
      //    if (_falseLabel != _trueLabel)
      //       bne cr0, 8 (_falseLabel)
      //       b   _trueLabel
      //    8: b   _falseLabel      // branch if xor. does not set 0 on resultReg
      //    else
      //       b   _doneLabel
      // else
      //    if (_falseLabel != _trueLabel)
      //       li   scratch1Reg, 0x1
      //       and. scratch1Reg, scratch1Reg, scratch2Reg
      //       bne  cr0, 8 (_falseLabel)
      //       b   _trueLabel
      //    8: b   _falseLabel      // branch if and. does not set 0 on scratch1Reg
      //    else
      //       b   _doneLabel

      // TODO: this code is set on 390.
      // int32_t offset = _offsetCastClassCache;
      // if (TR::Compiler->target.is64Bit())
      //    {
      //    offset += 4;
      //    }

      if (TR::Compiler->target.is64Bit() && !TR::Compiler->om.generateCompressedObjectHeaders())
         opcode.setOpCodeValue(TR::InstOpCode::ld);
      else
         opcode.setOpCodeValue(TR::InstOpCode::lwz);
      cursor = opcode.copyBinaryToBuffer(cursor);
      scratch2Reg->setRegisterFieldRT((uint32_t *)cursor);
      objClassReg->setRegisterFieldRA((uint32_t *)cursor);
      *(int32_t *)cursor |= _offsetCastClassCache & 0xffff;
      cursor += PPC_INSTRUCTION_LENGTH;

      if (TR::Compiler->target.is64Bit())
         {
         opcode.setOpCodeValue(TR::InstOpCode::rldicr);
         cursor = opcode.copyBinaryToBuffer(cursor);
         scratch1Reg->setRegisterFieldRA((uint32_t *)cursor);
         scratch2Reg->setRegisterFieldRS((uint32_t *)cursor);
         *(int32_t *)cursor |= (0x3d << 5);
         }
      else
         {
         opcode.setOpCodeValue(TR::InstOpCode::rlwinm);
         cursor = opcode.copyBinaryToBuffer(cursor);
         scratch1Reg->setRegisterFieldRA((uint32_t *)cursor);
         scratch2Reg->setRegisterFieldRS((uint32_t *)cursor);
         *(int32_t *)cursor |= (30 << 1);
         }
      cursor += PPC_INSTRUCTION_LENGTH;

      if (TR::Compiler->target.is64Bit())
         opcode.setOpCodeValue(TR::InstOpCode::cmpl8);
      else
         opcode.setOpCodeValue(TR::InstOpCode::cmpl4);
      cursor = opcode.copyBinaryToBuffer(cursor);
      cndReg->setRegisterFieldRT((uint32_t *)cursor);
      scratch1Reg->setRegisterFieldRA((uint32_t *)cursor);
      castClassReg->setRegisterFieldRB((uint32_t *)cursor);
      cursor += PPC_INSTRUCTION_LENGTH;

      if (_testCastClassIsSuper)
         {
         opcode.setOpCodeValue(TR::InstOpCode::beq);
         cursor = opcode.copyBinaryToBuffer(cursor);
         cndReg->setRegisterFieldBI((uint32_t *)cursor);
         branchDistance = (intptrj_t) 8; // Jump two instructions below
         *(int32_t *)cursor |= (branchDistance & 0x0fffc);
         cursor += PPC_INSTRUCTION_LENGTH;

         opcode.setOpCodeValue(TR::InstOpCode::b);
         cursor = opcode.copyBinaryToBuffer(cursor);
         branchDistance = (intptrj_t)getReStartLabel()->getCodeLocation() - (intptrj_t)cursor;
         TR_ASSERT(TR::Compiler->target.cpu.isTargetWithinIFormBranchRange((intptrj_t)getReStartLabel()->getCodeLocation(), (intptrj_t)cursor),
                   "backward jump in Interface Cache is too long\n");
         *(int32_t *)cursor |= (branchDistance & 0x03fffffc);
         cursor += PPC_INSTRUCTION_LENGTH;
      }
      else
      {
         opcode.setOpCodeValue(TR::InstOpCode::beq);
         cursor = opcode.copyBinaryToBuffer(cursor);
         cndReg->setRegisterFieldBI((uint32_t *)cursor);
         branchDistance = (intptrj_t) 8; // Jump two instructions below
         *(int32_t *)cursor |= (branchDistance & 0x0fffc);
         cursor += PPC_INSTRUCTION_LENGTH;

         opcode.setOpCodeValue(TR::InstOpCode::b);
         cursor = opcode.copyBinaryToBuffer(cursor);
         branchDistance = (intptrj_t)_callLabel->getCodeLocation() - (intptrj_t)cursor;
         TR_ASSERT(TR::Compiler->target.cpu.isTargetWithinIFormBranchRange((intptrj_t)_callLabel->getCodeLocation(), (intptrj_t)cursor),
                   "backward jump in Interface Cache is too long\n");
         *(int32_t *)cursor |= (branchDistance & 0x03fffffc);
         cursor += PPC_INSTRUCTION_LENGTH;
      }

      if (_needsResult)
         {
         opcode.setOpCodeValue(TR::InstOpCode::addi);
         cursor = opcode.copyBinaryToBuffer(cursor);
         scratch1Reg->setRegisterFieldRT((uint32_t *)cursor);
         *(int32_t *)cursor |= 0x1;
         cursor += PPC_INSTRUCTION_LENGTH;

         opcode.setOpCodeValue(TR::InstOpCode::OR);
         cursor = opcode.copyBinaryToBuffer(cursor);
         scratch1Reg->setRegisterFieldRA((uint32_t *)cursor);
         scratch1Reg->setRegisterFieldRS((uint32_t *)cursor);
         scratch2Reg->setRegisterFieldRB((uint32_t *)cursor);
         cursor += PPC_INSTRUCTION_LENGTH;

         opcode.setOpCodeValue(TR::InstOpCode::XOR);
         cursor = opcode.copyBinaryToBuffer(cursor);
         resultReg->setRegisterFieldRA((uint32_t *)cursor);
         scratch1Reg->setRegisterFieldRS((uint32_t *)cursor);
         scratch2Reg->setRegisterFieldRB((uint32_t *)cursor);
         *(int32_t *)cursor |= 0x1; // Rc = 1
         cursor += PPC_INSTRUCTION_LENGTH;

         if (_falseLabel != _trueLabel)
            {
            opcode.setOpCodeValue(TR::InstOpCode::bne);
            cursor = opcode.copyBinaryToBuffer(cursor);
            cndReg->setRegisterFieldBI((uint32_t *)cursor);
            branchDistance = (intptrj_t) 8; // Jump two instructions below
            *(int32_t *)cursor |= (branchDistance & 0x0fffc);
            cursor += PPC_INSTRUCTION_LENGTH;

            opcode.setOpCodeValue(TR::InstOpCode::b);
            cursor = opcode.copyBinaryToBuffer(cursor);
            branchDistance = (intptrj_t)_trueLabel->getCodeLocation() - (intptrj_t)cursor;
            TR_ASSERT(TR::Compiler->target.cpu.isTargetWithinIFormBranchRange((intptrj_t)_trueLabel->getCodeLocation(), (intptrj_t)cursor),
                      "backward jump in Interface Cache is too long\n");
            *(int32_t *)cursor |= (branchDistance & 0x03fffffc);
            cursor += PPC_INSTRUCTION_LENGTH;

            opcode.setOpCodeValue(TR::InstOpCode::b);
            cursor = opcode.copyBinaryToBuffer(cursor);
            branchDistance = (intptrj_t)_falseLabel->getCodeLocation() - (intptrj_t)cursor;
            TR_ASSERT(TR::Compiler->target.cpu.isTargetWithinIFormBranchRange((intptrj_t)_falseLabel->getCodeLocation(), (intptrj_t)cursor),
                      "backward jump in Interface Cache is too long\n");
            *(int32_t *)cursor |= (branchDistance & 0x03fffffc);
            cursor += PPC_INSTRUCTION_LENGTH;
            }
         else
            {
            opcode.setOpCodeValue(TR::InstOpCode::b);
            cursor = opcode.copyBinaryToBuffer(cursor);
            branchDistance = (intptrj_t)_doneLabel->getCodeLocation() - (intptrj_t)cursor;
            TR_ASSERT(TR::Compiler->target.cpu.isTargetWithinIFormBranchRange((intptrj_t)_doneLabel->getCodeLocation(), (intptrj_t)cursor),
                      "backward jump in Interface Cache is too long\n");
            *(int32_t *)cursor |= (branchDistance & 0x03fffffc);
            cursor += PPC_INSTRUCTION_LENGTH;
            }
         }
      else
         {
         if (_falseLabel != _trueLabel)
            {
            opcode.setOpCodeValue(TR::InstOpCode::addi);
            cursor = opcode.copyBinaryToBuffer(cursor);
            scratch1Reg->setRegisterFieldRT((uint32_t *)cursor);
            *(int32_t *)cursor |= 0x1;
            cursor += PPC_INSTRUCTION_LENGTH;

            opcode.setOpCodeValue(TR::InstOpCode::AND);
            cursor = opcode.copyBinaryToBuffer(cursor);
            scratch1Reg->setRegisterFieldRA((uint32_t *)cursor);
            scratch1Reg->setRegisterFieldRS((uint32_t *)cursor);
            scratch2Reg->setRegisterFieldRB((uint32_t *)cursor);
            *(int32_t *)cursor |= 0x1; // Rc = 1
            cursor += PPC_INSTRUCTION_LENGTH;

            opcode.setOpCodeValue(TR::InstOpCode::bne);
            cursor = opcode.copyBinaryToBuffer(cursor);
            cndReg->setRegisterFieldBI((uint32_t *)cursor);
            branchDistance = (intptrj_t) 8; // Jump two instructions below
            *(int32_t *)cursor |= (branchDistance & 0x0fffc);
            cursor += PPC_INSTRUCTION_LENGTH;

            opcode.setOpCodeValue(TR::InstOpCode::b);
            cursor = opcode.copyBinaryToBuffer(cursor);
            branchDistance = (intptrj_t)_trueLabel->getCodeLocation() - (intptrj_t)cursor;
            TR_ASSERT(TR::Compiler->target.cpu.isTargetWithinIFormBranchRange((intptrj_t)_trueLabel->getCodeLocation(), (intptrj_t)cursor),
                      "backward jump in Interface Cache is too long\n");
            *(int32_t *)cursor |= (branchDistance & 0x03fffffc);
            cursor += PPC_INSTRUCTION_LENGTH;

            opcode.setOpCodeValue(TR::InstOpCode::b);
            cursor = opcode.copyBinaryToBuffer(cursor);
            branchDistance = (intptrj_t)_falseLabel->getCodeLocation() - (intptrj_t)cursor;
            TR_ASSERT(TR::Compiler->target.cpu.isTargetWithinIFormBranchRange((intptrj_t)_falseLabel->getCodeLocation(), (intptrj_t)cursor),
                      "backward jump in Interface Cache is too long\n");
            *(int32_t *)cursor |= (branchDistance & 0x03fffffc);
            cursor += PPC_INSTRUCTION_LENGTH;
            }
         else
            {
            opcode.setOpCodeValue(TR::InstOpCode::b);
            cursor = opcode.copyBinaryToBuffer(cursor);
            branchDistance = (intptrj_t)_doneLabel->getCodeLocation() - (intptrj_t)cursor;
            TR_ASSERT(TR::Compiler->target.cpu.isTargetWithinIFormBranchRange((intptrj_t)_doneLabel->getCodeLocation(), (intptrj_t)cursor),
                      "backward jump in Interface Cache is too long\n");
            *(int32_t *)cursor |= (branchDistance & 0x03fffffc);
            cursor += PPC_INSTRUCTION_LENGTH;
            }
         }
      }
   return cursor;
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCInterfaceCastSnippet * snippet)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(_cg->fe());
   uint8_t *cursor = snippet->getSnippetLabel()->getCodeLocation();
   bool     checkcast = snippet->isCheckCast();

   if (checkcast)
      {
      printSnippetLabel(pOutFile, snippet->getSnippetLabel(), cursor, "Interface Cast Snippet for Checkcast");

      TR::RegisterDependencyConditions *deps = snippet->getDoneLabel()->getInstruction()->getDependencyConditions();
      TR::RealRegister *objReg       = _cg->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(0)->getRealRegister());
      TR::RealRegister *castClassReg = _cg->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(1)->getRealRegister());
      TR::RealRegister *objClassReg  = _cg->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(2)->getRealRegister());
      TR::RealRegister *cndReg       = _cg->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(3)->getRealRegister());
      TR::RealRegister *scratch1Reg  = _cg->machine()->getRealRegister(deps->getPostConditions()->getRegisterDependency(4)->getRealRegister());

      int32_t value;

      printPrefix(pOutFile, NULL, cursor, 4);
      value = *((int32_t *) cursor) & 0x0ffff;
      if (TR::Compiler->target.is64Bit() && !TR::Compiler->om.generateCompressedObjectHeaders())
         trfprintf(pOutFile, "ld \t%s, [%s, %d]\t; Load object class", getName(scratch1Reg), getName(objReg), value );
      else
         trfprintf(pOutFile, "lwz \t%s, [%s, %d]\t; Load object class", getName(scratch1Reg), getName(objReg), value );
      cursor+=4;

      printPrefix(pOutFile, NULL, cursor, 4);
      value = *((int32_t *) cursor) & 0x0ffff;
      if (TR::Compiler->target.is64Bit())
         trfprintf(pOutFile, "ld \t%s, [%s, %d]\t; Load castClassCache", getName(scratch1Reg), getName(scratch1Reg), value );
      else
         trfprintf(pOutFile, "lwz \t%s, [%s, %d]\t; Load castClassCache", getName(scratch1Reg), getName(scratch1Reg), value );
      cursor+=4;

      printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "cmpl \t%s, %s, %s\t; Compare with type to cast", getName(cndReg), getName(castClassReg), getName(scratch1Reg) );
      cursor+=4;

      if (snippet->getTestCastClassIsSuper())
         {
         printPrefix(pOutFile, NULL, cursor, 4);
         value = *((int32_t *) cursor) & 0xfffc;
         value = (value << 16) >> 16;   // sign extend
         trfprintf(pOutFile, "beq \t%s, 0x%p\t;", getName(cndReg), (intptrj_t)cursor + value);
         cursor += 4;

         printPrefix(pOutFile, NULL, cursor, 4);
         value = *((int32_t *) cursor) & 0x03fffffc;
         value = (value << 6) >> 6;   // sign extend
         trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t;", (intptrj_t)cursor + value);
         cursor += 4;

         printPrefix(pOutFile, NULL, cursor, 4);
         value = *((int32_t *) cursor) & 0x03fffffc;
         value = (value << 6) >> 6;   // sign extend
         trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t;", (intptrj_t)cursor + value);
         cursor += 4;
         }
      else
         {
         printPrefix(pOutFile, NULL, cursor, 4);
         value = *((int32_t *) cursor) & 0xfffc;
         value = (value << 16) >> 16;   // sign extend
         trfprintf(pOutFile, "bne \t%s, 0x%p\t;", getName(cndReg), (intptrj_t)cursor + value);
         cursor += 4;

         printPrefix(pOutFile, NULL, cursor, 4);
         value = *((int32_t *) cursor) & 0x03fffffc;
         value = (value << 6) >> 6;   // sign extend
         trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t;", (intptrj_t)cursor + value);
         cursor += 4;

         printPrefix(pOutFile, NULL, cursor, 4);
         value = *((int32_t *) cursor) & 0x03fffffc;
         value = (value << 6) >> 6;   // sign extend
         trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t;", (intptrj_t)cursor + value);
         cursor += 4;
         }
      }
   else
      {
      // InstanceOf has trueLabel = falseLabel
      // in ifInstanceOf, trueLabel != falseLabel

      if (snippet->getTrueLabel() == snippet->getFalseLabel())
         printSnippetLabel(pOutFile, snippet->getSnippetLabel(), cursor, "Interface Cast Snippet for instanceOf");
      else
         printSnippetLabel(pOutFile, snippet->getSnippetLabel(), cursor, "Interface Cast Snippet for ifInstanceOf");

      TR::RegisterDependencyConditions *deps = snippet->getDoneLabel()->getInstruction()->getDependencyConditions();
      TR::RealRegister *castClassReg = _cg->machine()->getRealRegister(TR::RealRegister::gr3);
      TR::RealRegister *objClassReg = _cg->machine()->getRealRegister(TR::RealRegister::gr6);
      TR::RealRegister *resultReg = _cg->machine()->getRealRegister(TR::RealRegister::gr8);
      TR::RealRegister *cndReg = _cg->machine()->getRealRegister(TR::RealRegister::cr0);
      TR::RealRegister *scratch1Reg = _cg->machine()->getRealRegister(TR::RealRegister::gr5);
      TR::RealRegister *scratch2Reg = _cg->machine()->getRealRegister(TR::RealRegister::gr7);

      int32_t value;

      printPrefix(pOutFile, NULL, cursor, 4);
      value = *((int32_t *) cursor) & 0x0ffff;
      if (TR::Compiler->target.is64Bit() && !TR::Compiler->om.generateCompressedObjectHeaders())
         trfprintf(pOutFile, "ld \t%s, [%s, %d]\t; Load castClassCache", getName(scratch2Reg), getName(objClassReg), value );
      else
         trfprintf(pOutFile, "lwz \t%s, [%s, %d]\t; Load castClassCache", getName(scratch2Reg), getName(objClassReg), value );
      cursor+=4;

      printPrefix(pOutFile, NULL, cursor, 4);
      if (TR::Compiler->target.is64Bit())
         trfprintf(pOutFile, "rldicr \t%s, %s, 0, 0x3D; Clean last bit (cached result)", getName(scratch1Reg), getName(scratch2Reg));
      else
         trfprintf(pOutFile, "rlwinm \t%s, %s, 0, 0xFFFFFFFE; Clean last bit (cached result)", getName(scratch1Reg), getName(scratch2Reg));
      cursor+= 4;

      printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "cmpl \t%s, %s, %s\t; Compare with type to cast", getName(cndReg), getName(scratch1Reg), getName(castClassReg) );
      cursor+=4;

      if (snippet->getTestCastClassIsSuper())
         {
         printPrefix(pOutFile, NULL, cursor, 4);
         value = *((int32_t *) cursor) & 0xfffc;
         value = (value << 16) >> 16;   // sign extend
         trfprintf(pOutFile, "beq \t%s, 0x%p\t;", getName(cndReg), (intptrj_t)cursor + value);
         cursor += 4;

         printPrefix(pOutFile, NULL, cursor, 4);
         value = *((int32_t *) cursor) & 0x03fffffc;
         value = (value << 6) >> 6;   // sign extend
         trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t;", (intptrj_t)cursor + value);
         cursor += 4;
         }
      else
         {
         printPrefix(pOutFile, NULL, cursor, 4);
         value = *((int32_t *) cursor) & 0xfffc;
         value = (value << 16) >> 16;   // sign extend
         trfprintf(pOutFile, "beq \t%s, 0x%p\t;", getName(cndReg), (intptrj_t)cursor + value);
         cursor += 4;

         printPrefix(pOutFile, NULL, cursor, 4);
         value = *((int32_t *) cursor) & 0x03fffffc;
         value = (value << 6) >> 6;   // sign extend
         trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t;", (intptrj_t)cursor + value);
         cursor += 4;
         }

      if (snippet->getNeedsResult())
         {
         printPrefix(pOutFile, NULL, cursor, 4);
         trfprintf(pOutFile, "li \t%s, %d", getName(scratch1Reg), *((int32_t *) cursor) & 0x0000ffff);
         cursor += 4;

         printPrefix(pOutFile, NULL, cursor, 4);
         trfprintf(pOutFile, "or \t%s, %s, %s; Set the last bit", getName(scratch1Reg), getName(scratch1Reg), getName(scratch2Reg));
         cursor+= 4;

         printPrefix(pOutFile, NULL, cursor, 4);
         trfprintf(pOutFile, "xor. \t%s, %s, %s; Check if last bit is set in the cache", getName(resultReg), getName(scratch1Reg), getName(scratch2Reg));
         cursor+= 4;

         if (snippet->getFalseLabel() != snippet->getTrueLabel())
            {
            printPrefix(pOutFile, NULL, cursor, 4);
            value = *((int32_t *) cursor) & 0xfffc;
            value = (value << 16) >> 16;   // sign extend
            trfprintf(pOutFile, "bne \t%s, 0x%p\t;", getName(cndReg), (intptrj_t)cursor + value);
            cursor += 4;

            printPrefix(pOutFile, NULL, cursor, 4);
            value = *((int32_t *) cursor) & 0x03fffffc;
            value = (value << 6) >> 6;   // sign extend
            trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t;", (intptrj_t)cursor + value);
            cursor += 4;

            printPrefix(pOutFile, NULL, cursor, 4);
            value = *((int32_t *) cursor) & 0x03fffffc;
            value = (value << 6) >> 6;   // sign extend
            trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t;", (intptrj_t)cursor + value);
            cursor += 4;
            }
         else
            {
            printPrefix(pOutFile, NULL, cursor, 4);
            value = *((int32_t *) cursor) & 0x03fffffc;
            value = (value << 6) >> 6;   // sign extend
            trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t;", (intptrj_t)cursor + value);
            cursor += 4;
            }
         }
      else
         {
         if (snippet->getFalseLabel() != snippet->getTrueLabel())
            {
            printPrefix(pOutFile, NULL, cursor, 4);
            trfprintf(pOutFile, "li \t%s, %d", getName(scratch1Reg), *((int32_t *) cursor) & 0x0000ffff);
            cursor += 4;

            printPrefix(pOutFile, NULL, cursor, 4);
            trfprintf(pOutFile, "and. \t%s, %s, %s; Check if last bit is set in the cache", getName(scratch1Reg), getName(scratch1Reg), getName(scratch2Reg));
            cursor+= 4;

            printPrefix(pOutFile, NULL, cursor, 4);
            value = *((int32_t *) cursor) & 0xfffc;
            value = (value << 16) >> 16;   // sign extend
            trfprintf(pOutFile, "bne \t%s, 0x%p\t;", getName(cndReg), (intptrj_t)cursor + value);
            cursor += 4;

            printPrefix(pOutFile, NULL, cursor, 4);
            value = *((int32_t *) cursor) & 0x03fffffc;
            value = (value << 6) >> 6;   // sign extend
            trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t;", (intptrj_t)cursor + value);
            cursor += 4;

            printPrefix(pOutFile, NULL, cursor, 4);
            value = *((int32_t *) cursor) & 0x03fffffc;
            value = (value << 6) >> 6;   // sign extend
            trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t;", (intptrj_t)cursor + value);
            cursor += 4;
            }
         else
            {
            printPrefix(pOutFile, NULL, cursor, 4);
            value = *((int32_t *) cursor) & 0x03fffffc;
            value = (value << 6) >> 6;   // sign extend
            trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t;", (intptrj_t)cursor + value);
            cursor += 4;
            }
         }
      }
   }


uint32_t
TR::PPCInterfaceCastSnippet::getLength(int32_t estimatedSnippetStart)
   {
   uint32_t size;

   if (_checkCast)
      {
      size = 6;
      }
   else // interfaceof
      {
      size = 5;

      if (_needsResult)
         {
         size += 4;
         if (_falseLabel != _trueLabel)
            {
            size += 3;
            }
         else
            {
            size += 1;
            }
         }
      else
         {
         if (_falseLabel != _trueLabel)
            {
            size += 5;
            }
         else
            {
            size += 1;
            }
         }
      }

   return size * PPC_INSTRUCTION_LENGTH;
   }

