/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#include "codegen/CodeGenerator.hpp"
#include "il/Symbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "x/codegen/X86Register.hpp"
#include "x/codegen/WriteBarrierSnippet.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Machine.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "env/IO.hpp"
#include "env/CompilerEnv.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"
#include "x/codegen/X86Ops_inlines.hpp"

uint8_t *TR::X86WriteBarrierSnippet::emitSnippetBody()
   {
   uint8_t *buffer = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(buffer);

   // get the helper's arguments into the right registers
   //
   buffer = buildArgs(buffer, false);

   // call the helper
   //
   buffer = genHelperCall(buffer);

   // restore regs
   //
   buffer = buildArgs(buffer, true);

   return genRestartJump(buffer);
   }


// ----------------------------------------------------------------------------
//                                  32-BIT
// ----------------------------------------------------------------------------


uint8_t *TR::IA32WriteBarrierSnippet::buildArgs(
   uint8_t *buffer,
   bool     restoreRegs)
   {

   if (restoreRegs)
      return buffer;

   TR_ASSERT(_deps->getNumPostConditions() >= getHelperArgCount(), "Not enough RegDeps");
   bool needDestAddressReg      = getHelperArgCount() == 3;
   bool needSourceReg           = getHelperArgCount() >= 2;
   bool needDestOwningObjectReg = getHelperArgCount() >= 1;

   if (TR::comp()->getOption(BreakOnWriteBarrierSnippet))
      {
      buffer = TR_X86OpCode(BADIA32Op).binary(buffer);
      }

   if (needSourceReg)
      {
      *buffer = 0x50;  // PushReg
      // now set the register field
      TR::RealRegister *realNewSpaceReg = toRealRegister(getSourceRegister());
      realNewSpaceReg->setRegisterFieldInOpcode(buffer++);
      }

   if (needDestAddressReg)
      {
      TR_ASSERT(_wrtBarrierKind == TR_WrtbarRealTime, "Unexpected helper argument count");
      *buffer = 0x50;  // PushReg
      // now set the register field
      TR::RealRegister *destAddressReg = toRealRegister(getDestAddressRegister());
      destAddressReg->setRegisterFieldInOpcode(buffer++);
      }

   if (needDestOwningObjectReg)
      {
      *buffer = 0x50;  // PushReg
      // now set the register field
      TR::RealRegister *realOldSpaceReg = toRealRegister(getDestOwningObjectRegister());
      realOldSpaceReg->setRegisterFieldInOpcode(buffer++);
      }

   return buffer;
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::IA32WriteBarrierSnippet * snippet)
   {
   if (pOutFile == NULL)
      return;

   bool needDestinationAddressReg = snippet->getHelperArgCount() == 3;
   bool needNewSpaceReg           = snippet->getHelperArgCount() >= 2;
   bool needOldSpaceReg           = snippet->getHelperArgCount() >= 1;
   bool isRealTimeGCBarrier = (snippet->getWriteBarrierKind() == TR_WrtbarRealTime);
   uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();
   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet));

   if (needNewSpaceReg)
      {
      printPrefix(pOutFile, NULL, bufferPos, 1);
      trfprintf(pOutFile, "push\t");
      print(pOutFile, snippet->getSourceRegister(), TR_WordReg);
      if (isRealTimeGCBarrier)
         trfprintf(pOutFile, "\t\t%s Object to be Stored Register",
                       commentString());
      else
         trfprintf(pOutFile, "\t\t%s Source (new space) Register",
                       commentString());
      bufferPos += 1;
      }

   if (needDestinationAddressReg)
      {
      printPrefix(pOutFile, NULL, bufferPos, 1);
      trfprintf(pOutFile, "push\t");
      print(pOutFile, snippet->getDestAddressRegister(), TR_WordReg);
      trfprintf(pOutFile, "\t\t%s Destination Address Register",
                    commentString());
      bufferPos += 1;
      }

   if (needOldSpaceReg)
      {
      printPrefix(pOutFile, NULL, bufferPos, 1);
      trfprintf(pOutFile, "push\t");
      print(pOutFile, snippet->getDestOwningObjectRegister(), TR_WordReg);
      if (isRealTimeGCBarrier)
         trfprintf(pOutFile, "\t\t%s Destination Object Register",
                       commentString());
      else
         trfprintf(pOutFile, "\t\t%s Destination Owning Object (Old Space) Register",
                       commentString());
      bufferPos += 1;
      }

   printPrefix(pOutFile, NULL, bufferPos, 5);
   trfprintf(pOutFile, "call\t%s", getName(snippet->getDestination()));
   bufferPos += 5;

   printRestartJump(pOutFile, snippet, bufferPos);
   }


uint32_t TR::IA32WriteBarrierSnippet::getLength(int32_t estimatedSnippetStart)
   {
   int32_t size = 5 + getHelperArgCount();
   if (TR::comp()->getOption(BreakOnWriteBarrierSnippet))
      size ++;
   return size + estimateRestartJumpLength(estimatedSnippetStart + size);
   }


// ----------------------------------------------------------------------------
//                                  64-BIT
// ----------------------------------------------------------------------------

#define CALL_SIZE (5)


static uint8_t *xchgRAX(uint8_t            *buffer,
                        TR::RealRegister *reg)
   {
   buffer = TR_X86OpCode(XCHG8AccReg).binary(buffer,
                                             TR::RealRegister::REX |
                                             TR::RealRegister::REX_W |
                                             reg->rexBits(TR::RealRegister::REX_B, false));
   reg->setRegisterFieldInOpcode(buffer-1);

   return buffer;
   }


static uint8_t *xchg(uint8_t            *buffer,
                     TR::RealRegister *reg1,
                     TR::RealRegister *reg2)
   {
   buffer = TR_X86OpCode(XCHG8RegReg).binary(buffer,
                                             TR::RealRegister::REX |
                                             TR::RealRegister::REX_W |
                                             reg1->rexBits(TR::RealRegister::REX_R, false) |
                                             reg2->rexBits(TR::RealRegister::REX_B, false));
   reg1->setRegisterFieldInModRM(buffer-1);
   reg2->setRMRegisterFieldInModRM(buffer-1);

   return buffer;
   }


#define PUSH_RAX(buffer) *buffer++ = 0x50;
#define PUSH_RSI(buffer) *buffer++ = 0x56;

#define MOV_RAX_RSI(buffer) \
         *buffer++ = 0x48;   \
         *buffer++ = 0x8b;  \
         *buffer++ = 0xc6;

#define MOV_RSI_RAX(buffer) \
         *buffer++ = 0x48;   \
         *buffer++ = 0x8b;  \
         *buffer++ = 0xf0;

#define XCHG_RSI_RAX(buffer) \
         *buffer++ = 0x48;   \
         *buffer++ = 0x96;

#define POP_RAX(buffer) *buffer++ = 0x58;
#define POP_RSI(buffer) *buffer++ = 0x5e;


uint8_t *TR::AMD64WriteBarrierSnippet::buildArgs(
   uint8_t *buffer,
   bool     restoreRegs)
   {
   TR::Machine *machine = cg()->machine();
   TR::Linkage *linkage = cg()->getLinkage();

   //
   // NOTE: THIS CODE HAS BEEN CUSTOMIZED TO THE CURRENT AMD64 PRIVATE LINKAGE.  IT WILL
   // EVENTUALLY BE REPLACED BY THE HELPER CALL SNIPPET THAT CAN HANDLE TR_INSTRUCTIONS.
   //

   // Note that helper args are passed in reverse order as per the weird helper linkage spec
   //
   TR::RealRegister *r1Real = toRealRegister(getDestOwningObjectRegister());
   TR::RealRegister::RegNum r1 = r1Real->getRegisterNumber();
   TR::RealRegister::RegNum arg1Linkage = linkage->getProperties().getIntegerArgumentRegister(0);
   TR_ASSERT(arg1Linkage == TR::RealRegister::eax, "unsupported AMD64 linkage");
   TR::RealRegister *raxReal = machine->getX86RealRegister(arg1Linkage);

   TR::RealRegister *r2Real = NULL, *rsiReal = NULL;
   TR::RealRegister::RegNum r2 = TR::RealRegister::NoReg, arg2Linkage = TR::RealRegister::NoReg;

   if (_deps->getNumPostConditions() > 1)
      {
      r2Real = toRealRegister(getSourceRegister());
      r2 = r2Real->getRegisterNumber();
      arg2Linkage = linkage->getProperties().getIntegerArgumentRegister(1);
      TR_ASSERT(arg2Linkage == TR::RealRegister::esi, "unsupported AMD64 linkage");
      rsiReal = machine->getX86RealRegister(arg2Linkage);
      }

   // Fast path the single argument case.  Presently, only a batch arraycopy write barrier applies.
   //
   if (_deps->getNumPostConditions() == 1)
      {
      if (r1 != TR::RealRegister::eax)
         {
         // Register is not in RAX so exchange it.  This applies to both building and restoring args.
         //
         buffer = xchgRAX(buffer, r1Real);
         }

      return buffer;
      }

   int8_t action = 0;

   action = (restoreRegs ? 32 : 0) |
            (r1 == TR::RealRegister::eax ? 16 : 0) |
            (r1 == TR::RealRegister::esi ? 8 : 0) |
            (r2 == TR::RealRegister::esi ? 4 : 0) |
            (r2 == TR::RealRegister::eax ? 2 : 0) |
            (r1 == r2 ? 1 : 0);

   switch (action)
      {
      case 0:
      case 32:
         buffer = xchgRAX(buffer, r1Real);
         buffer = xchg(buffer, rsiReal, r2Real);
         break;

      case 1:
         PUSH_RSI(buffer)
         buffer = xchgRAX(buffer, r1Real);
         MOV_RSI_RAX(buffer)
         break;

      case 2:
         XCHG_RSI_RAX(buffer)
         buffer = xchgRAX(buffer, r1Real);
         break;

      case 4:
      case 36:
         buffer = xchgRAX(buffer, r1Real);
         break;

      case 8:
         XCHG_RSI_RAX(buffer)
         buffer = xchg(buffer, rsiReal, r2Real);
         break;

      case 10:
      case 42:
         XCHG_RSI_RAX(buffer)
         break;

      case 13:
         PUSH_RAX(buffer)
         MOV_RAX_RSI(buffer)
         break;

      case 16:
      case 48:
         buffer = xchg(buffer, rsiReal, r2Real);
         break;

      case 19:
         PUSH_RSI(buffer)
         MOV_RSI_RAX(buffer)
         break;

      case 33:
         buffer = xchgRAX(buffer, r1Real);
         POP_RSI(buffer)
         break;

      case 34:
         buffer = xchgRAX(buffer, r1Real);
         XCHG_RSI_RAX(buffer)
         break;

      case 40:
         buffer = xchg(buffer, rsiReal, r2Real);
         XCHG_RSI_RAX(buffer)
         break;

      case 45:
         POP_RAX(buffer)
         break;

      case 51:
         POP_RSI(buffer)
         break;
      }

   return buffer;
   }


uint32_t TR::AMD64WriteBarrierSnippet::getLength(int32_t estimatedSnippetStart)
   {
   // Generate linkage into a temporary buffer to calculate length.
   //
   uint8_t xchgBuffer[AMD64_MAX_XCHG_BUFFER_SIZE];
   uint8_t *cursor = xchgBuffer;
   cursor = buildArgs(cursor, false);
   cursor = buildArgs(cursor, true);
   TR_ASSERT(cursor-xchgBuffer <= sizeof(xchgBuffer), "Must not overflow xchgBuffer");

   // total size = call size + xchg size
   //
   uint8_t size = CALL_SIZE + (cursor-xchgBuffer);

   return size + estimateRestartJumpLength(estimatedSnippetStart + size);
   }


TR::X86WriteBarrierSnippet *
TR::generateX86WriteBarrierSnippet(
   TR::CodeGenerator   *cg,
   TR::Node            *node,
   TR::LabelSymbol      *restartLabel,
   TR::LabelSymbol      *snippetLabel,
   TR::SymbolReference *helperSymRef,
   int32_t             helperArgCount,
   TR_WriteBarrierKind gcMode,
   TR::RegisterDependencyConditions *conditions)
   {
   if (TR::Compiler->target.is64Bit())
      {
      return
         new (cg->trHeapMemory()) TR::AMD64WriteBarrierSnippet(
            cg,
            node,
            restartLabel,
            snippetLabel,
            helperSymRef,
            helperArgCount,
            conditions);
      }
   else
      {
      return
         new (cg->trHeapMemory()) TR::IA32WriteBarrierSnippet(
            cg,
            node,
            restartLabel,
            snippetLabel,
            helperSymRef,
            helperArgCount,
            gcMode,
            conditions);
      }
   }
