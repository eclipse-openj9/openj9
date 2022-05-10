/*******************************************************************************
 * Copyright (c) 2000, 2022 IBM Corp. and others
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

#ifdef TR_TARGET_64BIT

#include "codegen/AMD64PrivateLinkage.hpp"
#include "codegen/AMD64JNILinkage.hpp"

#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "compile/Method.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/CHTable.hpp"
#include "env/IO.hpp"
#include "env/J2IThunk.hpp"
#include "env/VMJ9.h"
#include "env/VerboseLog.hpp"
#include "env/jittypes.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "x/amd64/codegen/AMD64GuardedDevirtualSnippet.hpp"
#include "x/codegen/CallSnippet.hpp"
#include "x/codegen/CheckFailureSnippet.hpp"
#include "x/codegen/HelperCallSnippet.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "codegen/InstOpCode.hpp"

////////////////////////////////////////////////
//
// Helpful definitions
//
// These are only here to make the rest of the code below somewhat
// self-documenting.
//

enum
   {
   RETURN_ADDRESS_SIZE=8,
   NUM_INTEGER_LINKAGE_REGS=4,
   NUM_FLOAT_LINKAGE_REGS=8,
   };

////////////////////////////////////////////////
//
// Hack markers
//
// These things may become unnecessary as the implementation matures
//

// Hacks related to 8 byte slots
#define DOUBLE_SIZED_ARGS (1)

// Misc
#define INTERPRETER_CLOBBERS_XMMS          (!debug("interpreterClobbersXmms"))
// Note AOT relocations assumes that this value is 0.  Shall this value change, the AOT relocations also need to reflect this change.
#define VM_NEEDS_8_BYTE_CPINDEX            (0)


////////////////////////////////////////////////
//
// Initialization
//

J9::X86::AMD64::PrivateLinkage::PrivateLinkage(TR::CodeGenerator *cg)
   : J9::X86::PrivateLinkage(cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   const TR::RealRegister::RegNum noReg = TR::RealRegister::NoReg;
   uint8_t r, p;

   TR::RealRegister::RegNum metaReg = TR::RealRegister::ebp;

   _properties._properties =
        EightBytePointers | EightByteParmSlots
      | IntegersInRegisters | LongsInRegisters | FloatsInRegisters
      | NeedsThunksForIndirectCalls
      | UsesRegsForHelperArgs
      ;

   if (!fej9->pushesOutgoingArgsAtCallSite(cg->comp()))
      _properties._properties |= CallerCleanup | ReservesOutgoingArgsInPrologue;

   // Integer arguments
   p=0;
   _properties._firstIntegerArgumentRegister = p;
   _properties._argumentRegisters[p++]    = TR::RealRegister::eax;
   _properties._argumentRegisters[p++]    = TR::RealRegister::esi;
   _properties._argumentRegisters[p++]    = TR::RealRegister::edx;
   _properties._argumentRegisters[p++]    = TR::RealRegister::ecx;
   TR_ASSERT(p == NUM_INTEGER_LINKAGE_REGS, "assertion failure");
   _properties._numIntegerArgumentRegisters = NUM_INTEGER_LINKAGE_REGS;

   // Float arguments
   _properties._firstFloatArgumentRegister = p;
   for(r=0; r<=7; r++)
      _properties._argumentRegisters[p++] = TR::RealRegister::xmmIndex(r);
   TR_ASSERT(p == NUM_INTEGER_LINKAGE_REGS + NUM_FLOAT_LINKAGE_REGS, "assertion failure");
   _properties._numFloatArgumentRegisters = NUM_FLOAT_LINKAGE_REGS;

   // Preserved
   p=0;
   _properties._preservedRegisters[p++]    = TR::RealRegister::ebx;
   _properties._preservedRegisterMapForGC  = TR::RealRegister::ebxMask;

   int32_t lastPreservedRegister = 9; // changed to 9 for liberty, it used to be 15

   for (r=9; r<=lastPreservedRegister; r++)
      {
      _properties._preservedRegisters[p++] = TR::RealRegister::rIndex(r);
      _properties._preservedRegisterMapForGC |= TR::RealRegister::gprMask(TR::RealRegister::rIndex(r));
      }
   _properties._numberOfPreservedGPRegisters = p;

   if (!INTERPRETER_CLOBBERS_XMMS)
      for (r=8; r<=15; r++)
         {
         _properties._preservedRegisters[p++] = TR::RealRegister::xmmIndex(r);
         _properties._preservedRegisterMapForGC |= TR::RealRegister::xmmrMask(TR::RealRegister::xmmIndex(r));
         }

   _properties._numberOfPreservedXMMRegisters = p - _properties._numberOfPreservedGPRegisters;
   _properties._maxRegistersPreservedInPrologue = p;
   _properties._preservedRegisters[p++]    = metaReg;
   _properties._preservedRegisters[p++]    = TR::RealRegister::esp;
   _properties._numPreservedRegisters = p;

   // Other
   _properties._returnRegisters[0] = TR::RealRegister::eax;
   _properties._returnRegisters[1] = TR::RealRegister::xmm0;
   _properties._returnRegisters[2] = noReg;

   _properties._scratchRegisters[0] = TR::RealRegister::edi;
   _properties._scratchRegisters[1] = TR::RealRegister::r8;
   _properties._numScratchRegisters = 2;

   _properties._vtableIndexArgumentRegister = TR::RealRegister::r8;
   _properties._j9methodArgumentRegister = TR::RealRegister::edi;
   _properties._framePointerRegister = TR::RealRegister::esp;
   _properties._methodMetaDataRegister = metaReg;

   _properties._numberOfVolatileGPRegisters  = 6; // rax, rsi, rdx, rcx, rdi, r8
   _properties._numberOfVolatileXMMRegisters = INTERPRETER_CLOBBERS_XMMS? 16 : 8; // xmm0-xmm7

   // Offsets relative to where the frame pointer *would* point if we had one;
   // namely, the local with the highest address (ie. the "first" local)
   setOffsetToFirstParm(RETURN_ADDRESS_SIZE);
   _properties._offsetToFirstLocal = 0;

   // TODO: Need a better way to build the flags so they match the info above
   //
   memset(_properties._registerFlags, 0, sizeof(_properties._registerFlags));

   // Integer arguments/return
   _properties._registerFlags[TR::RealRegister::eax]      = IntegerArgument | IntegerReturn;
   _properties._registerFlags[TR::RealRegister::esi]      = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::edx]      = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::ecx]      = IntegerArgument;

   // Float arguments/return
   _properties._registerFlags[TR::RealRegister::xmm0]     = FloatArgument | FloatReturn;
   for(r=1; r <= 7; r++)
      _properties._registerFlags[TR::RealRegister::xmmIndex(r)]  = FloatArgument;

   // Preserved
   _properties._registerFlags[TR::RealRegister::ebx]      = Preserved;
   _properties._registerFlags[TR::RealRegister::esp]      = Preserved;
   _properties._registerFlags[metaReg]                  = Preserved;
   for(r=9; r <= lastPreservedRegister; r++)
      _properties._registerFlags[TR::RealRegister::rIndex(r)]    = Preserved;
   if(!INTERPRETER_CLOBBERS_XMMS)
      for(r=8; r <= 15; r++)
         _properties._registerFlags[TR::RealRegister::xmmIndex(r)]  = Preserved;


   p = 0;
   if (TR::Machine::enableNewPickRegister())
      {
      // Volatiles that aren't linkage regs
      if (TR::Machine::numGPRRegsWithheld(cg) == 0)
         {
         _properties._allocationOrder[p++] = TR::RealRegister::edi;
         _properties._allocationOrder[p++] = TR::RealRegister::r8;
         }
      else
         {
         TR_ASSERT(TR::Machine::numGPRRegsWithheld(cg) == 2, "numRegsWithheld: only 0 and 2 currently supported");
         }

      // Linkage regs in reverse order
      _properties._allocationOrder[p++] = TR::RealRegister::ecx;
      _properties._allocationOrder[p++] = TR::RealRegister::edx;
      _properties._allocationOrder[p++] = TR::RealRegister::esi;
      _properties._allocationOrder[p++] = TR::RealRegister::eax;
      }
   // Preserved regs
   _properties._allocationOrder[p++] = TR::RealRegister::ebx;
   _properties._allocationOrder[p++] = TR::RealRegister::r9;
   _properties._allocationOrder[p++] = TR::RealRegister::r10;
   _properties._allocationOrder[p++] = TR::RealRegister::r11;
   _properties._allocationOrder[p++] = TR::RealRegister::r12;
   _properties._allocationOrder[p++] = TR::RealRegister::r13;
   _properties._allocationOrder[p++] = TR::RealRegister::r14;
   _properties._allocationOrder[p++] = TR::RealRegister::r15;

   TR_ASSERT(p == machine()->getNumGlobalGPRs(), "assertion failure");

   if (TR::Machine::enableNewPickRegister())
      {
      // Linkage regs in reverse order
      if (TR::Machine::numRegsWithheld(cg) == 0)
         {
         _properties._allocationOrder[p++] = TR::RealRegister::xmm7;
         _properties._allocationOrder[p++] = TR::RealRegister::xmm6;
         }
      else
         {
         TR_ASSERT(TR::Machine::numRegsWithheld(cg) == 2, "numRegsWithheld: only 0 and 2 currently supported");
         }
      _properties._allocationOrder[p++] = TR::RealRegister::xmm5;
      _properties._allocationOrder[p++] = TR::RealRegister::xmm4;
      _properties._allocationOrder[p++] = TR::RealRegister::xmm3;
      _properties._allocationOrder[p++] = TR::RealRegister::xmm2;
      _properties._allocationOrder[p++] = TR::RealRegister::xmm1;
      _properties._allocationOrder[p++] = TR::RealRegister::xmm0;
      }
   // Preserved regs
   _properties._allocationOrder[p++] = TR::RealRegister::xmm8;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm9;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm10;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm11;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm12;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm13;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm14;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm15;

   TR_ASSERT(p == (machine()->getNumGlobalGPRs() + machine()->_numGlobalFPRs), "assertion failure");
   }


////////////////////////////////////////////////
//
// Argument manipulation
//

static uint8_t *flushArgument(
      uint8_t*cursor,
      TR::InstOpCode::Mnemonic op,
      TR::RealRegister::RegNum regIndex,
      int32_t offset,
      TR::CodeGenerator *cg)
   {
   /* TODO:AMD64: Share code with the other flushArgument */
   cursor = TR::InstOpCode(op).binary(cursor, OMR::X86::Default);

   // Mod | Reg | R/M
   //
   // 01     r    100  (8 bit displacement)
   // 10     r    100  (32 bit displacement)
   //
   uint8_t ModRM = 0x04;
   ModRM |= (offset >= -128 && offset <= 127) ? 0x40 : 0x80;

   *(cursor - 1) = ModRM;
   cg->machine()->getRealRegister(regIndex)->setRegisterFieldInModRM(cursor - 1);

   // Scale = 0x00, Index = none, Base = rsp
   //
   *cursor++ = 0x24;

   if (offset >= -128 && offset <= 127)
      {
      *cursor++ = (uint8_t)offset;
      }
   else
      {
      *(uint32_t *)cursor = offset;
      cursor += 4;
      }

   return cursor;
   }

static int32_t flushArgumentSize(
      TR::InstOpCode::Mnemonic op,
      int32_t offset)
   {
   int32_t size = TR::InstOpCode(op).length(OMR::X86::Default) + 1;   // length including ModRM + 1 SIB
   return size + (((offset >= -128 && offset <= 127)) ? 1 : 4);
   }

uint8_t *J9::X86::AMD64::PrivateLinkage::flushArguments(
      TR::Node *callNode,
      uint8_t *cursor,
      bool calculateSizeOnly,
      int32_t *sizeOfFlushArea,
      bool isReturnAddressOnStack,
      bool isLoad)
   {
   // TODO:AMD64: Share code with the other flushArguments
   TR::MethodSymbol     *methodSymbol = callNode->getSymbol()->castToMethodSymbol();

   int32_t numGPArgs = 0;
   int32_t numFPArgs = 0;
   int32_t argSize   = argAreaSize(callNode);
   int32_t offset    = argSize;
   bool    needFlush = false;

   // account for the return address in thunks and snippets.
   if (isReturnAddressOnStack)
      offset += sizeof(intptr_t);

   TR::RealRegister::RegNum reg = TR::RealRegister::NoReg;
   TR::InstOpCode::Mnemonic            op  = TR::InstOpCode::bad;
   TR::DataType            dt  = TR::NoType;

   if (calculateSizeOnly)
      *sizeOfFlushArea = 0;

   const uint8_t slotSize = DOUBLE_SIZED_ARGS? 8 : 4;

   for (int i = callNode->getFirstArgumentIndex(); i < callNode->getNumChildren(); i++)
      {
      TR::DataType type = callNode->getChild(i)->getType();
      dt = type.getDataType();
      op = TR::Linkage::movOpcodes(isLoad? RegMem : MemReg, movType(dt));

      switch (dt)
         {
         case TR::Int64:
            offset -= slotSize;

            // Longs take two slots.  Deliberate fall through.

         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Address:
            offset -= slotSize;
            if (numGPArgs < getProperties().getNumIntegerArgumentRegisters())
               {
               needFlush = true;
               if (!calculateSizeOnly)
                  reg = getProperties().getIntegerArgumentRegister(numGPArgs);
               }
            numGPArgs++;
            break;

         case TR::Float:
         case TR::Double:
            offset -= ((dt == TR::Double) ? 2 : 1) * slotSize;

            if (numFPArgs < getProperties().getNumFloatArgumentRegisters())
               {
               needFlush = true;
               if (!calculateSizeOnly)
                  reg = getProperties().getFloatArgumentRegister(numFPArgs);
               }

            numFPArgs++;
            break;
         default:
         	break;
         }

      if (needFlush)
         {
         if (calculateSizeOnly)
            *sizeOfFlushArea += flushArgumentSize(op, offset);
         else
            cursor = flushArgument(cursor, op, reg, offset, cg());

         needFlush = false;
         }
      }

   return cursor;
   }

TR::Instruction *
J9::X86::AMD64::PrivateLinkage::generateFlushInstruction(
      TR::Instruction *prev,
      TR_MovOperandTypes operandType,
      TR::DataType dataType,
      TR::RealRegister::RegNum regIndex,
      TR::Register *espReg,
      int32_t offset,
      TR::CodeGenerator *cg
   )
   {
   TR::Instruction *result;

   // Opcode
   //
   TR::InstOpCode::Mnemonic opCode = TR::Linkage::movOpcodes(operandType, movType(dataType));

   // Registers
   //
   TR::Register   *argReg  = cg->allocateRegister(movRegisterKind(movType(dataType)));

   TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)2, 2, cg);
   deps->addPreCondition (argReg, regIndex,            cg);
   deps->addPostCondition(argReg, regIndex,            cg);
   deps->addPreCondition (espReg, TR::RealRegister::esp, cg);
   deps->addPostCondition(espReg, TR::RealRegister::esp, cg);

   // Generate the instruction
   //
   TR::MemoryReference *memRef = generateX86MemoryReference(espReg, offset, cg);
   switch (operandType)
      {
      case RegMem:
         result = new (cg->trHeapMemory()) TR::X86RegMemInstruction(prev, opCode, argReg, memRef, deps, cg);
         break;
      case MemReg:
         result = new (cg->trHeapMemory()) TR::X86MemRegInstruction(prev, opCode, memRef, argReg, deps, cg);
         break;
      default:
         TR_ASSERT(0, "Flush instruction must be RegMem or MemReg");
         result = NULL;
         break;
      }

   // Cleanup
   //
   cg->stopUsingRegister(argReg);

   return result;
   }

TR::Instruction *
J9::X86::AMD64::PrivateLinkage::flushArguments(
      TR::Instruction *prev,
      TR::ResolvedMethodSymbol *methodSymbol,
      bool isReturnAddressOnStack,
      bool isLoad)
   {
   // TODO:AMD64: Share code with the other flushArguments

   int32_t numGPArgs = 0;
   int32_t numFPArgs = 0;
   int32_t offset    = argAreaSize(methodSymbol);
   bool    needFlush = false;

   const uint8_t slotSize = DOUBLE_SIZED_ARGS? 8 : 4;

   // account for the return address in SwitchToInterpreterPrePrologue
   if (isReturnAddressOnStack)
      offset += sizeof(intptr_t);

   TR::RealRegister::RegNum reg;
   TR::Register *espReg = cg()->allocateRegister();

   ListIterator<TR::ParameterSymbol> parameterIterator(&methodSymbol->getParameterList());
   TR::ParameterSymbol              *parmCursor;
   for (parmCursor = parameterIterator.getFirst(); parmCursor; parmCursor = parameterIterator.getNext())
      {
      TR::DataType       type = parmCursor->getType();
      TR::DataType      dt = type.getDataType();
      TR_MovOperandTypes ot = isLoad? RegMem : MemReg;

      switch (dt)
         {
         case TR::Int64:
            offset -= slotSize;

            // Deliberate fall through

         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Address:
            offset -= slotSize;
            if (numGPArgs < getProperties().getNumIntegerArgumentRegisters())
               {
               prev = generateFlushInstruction(prev, ot, dt, getProperties().getIntegerArgumentRegister(numGPArgs), espReg, offset, cg());
               }

            numGPArgs++;
            break;

         case TR::Double:
            offset -= slotSize;

            // Deliberate fall through

         case TR::Float:
            offset -= slotSize;

            if (numFPArgs < getProperties().getNumFloatArgumentRegisters())
               {
               prev = generateFlushInstruction(prev, ot, dt, getProperties().getFloatArgumentRegister(numFPArgs), espReg, offset, cg());
               }

            numFPArgs++;
            break;
         default:
         	break;
         }

      }

   cg()->stopUsingRegister(espReg);

   return prev;
   }

uint8_t *J9::X86::AMD64::PrivateLinkage::generateVirtualIndirectThunk(TR::Node *callNode)
   {
   int32_t              codeSize;
   TR::SymbolReference  *glueSymRef;
   uint8_t             *thunk;
   uint8_t             *thunkEntry;
   uint8_t             *cursor;
   TR::Compilation *comp = cg()->comp();

   (void)storeArguments(callNode, NULL, true, &codeSize);
   codeSize += 12;  // +10 TR::InstOpCode::MOV8RegImm64 +2 TR::InstOpCode::JMPReg

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());

   // Allocate the thunk in the cold code section
   //
   if (fej9->storeOffsetToArgumentsInVirtualIndirectThunks())
      {
      codeSize += 8;
      thunk = (uint8_t *)comp->trMemory()->allocateMemory(codeSize, heapAlloc);
      cursor = thunkEntry = thunk + 8; // 4 bytes holding the size of storeArguments
      }
   else
      {
      thunk = (uint8_t *)cg()->allocateCodeMemory(codeSize, true);
      cursor = thunkEntry = thunk;
      }

   switch (callNode->getDataType())
      {
      case TR::NoType:
         glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_AMD64icallVMprJavaSendVirtual0);
         break;
      case TR::Int64:
         glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_AMD64icallVMprJavaSendVirtualJ);
         break;
      case TR::Address:
         glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_AMD64icallVMprJavaSendVirtualL);
         break;
      case TR::Int32:
         glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_AMD64icallVMprJavaSendVirtual1);
         break;
      case TR::Float:
         glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_AMD64icallVMprJavaSendVirtualF);
         break;
      case TR::Double:
         glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_AMD64icallVMprJavaSendVirtualD);
         break;
      default:
         TR_ASSERT(0, "Bad return data type '%s' for call node [" POINTER_PRINTF_FORMAT "]\n",
                 comp->getDebug()->getName(callNode->getDataType()),
                 callNode);
      }

   cursor = storeArguments(callNode, cursor, false, NULL);

   if (fej9->storeOffsetToArgumentsInVirtualIndirectThunks())
      {
      *((int32_t *)thunk + 1) = cursor - thunkEntry;
      }

   // TR::InstOpCode::MOV8RegImm64 rdi, glueAddress
   //
   *(uint16_t *)cursor = 0xbf48;
   cursor += 2;
   *(uint64_t *)cursor = (uintptr_t)glueSymRef->getMethodAddress();
   cursor += 8;

   // TR::InstOpCode::JMPReg rdi
   //
   *(uint8_t *)cursor++ = 0xff;
   *(uint8_t *)cursor++ = 0xe7;

   if (fej9->storeOffsetToArgumentsInVirtualIndirectThunks())
      *(int32_t *)thunk = cursor - thunkEntry;

   diagnostic("\n-- ( Created J2I thunk " POINTER_PRINTF_FORMAT " for node " POINTER_PRINTF_FORMAT " )\n", thunkEntry, callNode);

   return thunkEntry;
   }

TR_J2IThunk *J9::X86::AMD64::PrivateLinkage::generateInvokeExactJ2IThunk(TR::Node *callNode, char *signature)
   {
   TR::Compilation *comp = cg()->comp();

   // Allocate thunk storage
   //
   int32_t codeSize;
   (void)storeArguments(callNode, NULL, true, &codeSize);
   codeSize += 10;  // TR::InstOpCode::MOV8RegImm64
   if (comp->getOption(TR_BreakOnJ2IThunk))
      codeSize += 1; // breakpoint opcode
   if (comp->getOptions()->getVerboseOption(TR_VerboseJ2IThunks))
      codeSize += 5; // JMPImm4
   else
      codeSize += 2; // TR::InstOpCode::JMPReg

   TR_J2IThunkTable *thunkTable = comp->getPersistentInfo()->getInvokeExactJ2IThunkTable();
   TR_J2IThunk      *thunk      = TR_J2IThunk::allocate(codeSize, signature, cg(), thunkTable);
   uint8_t          *cursor     = thunk->entryPoint();

   TR::SymbolReference  *glueSymRef;
   switch (callNode->getDataType())
      {
      case TR::NoType:
         glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExact0);
         break;
      case TR::Int64:
         glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExactJ);
         break;
      case TR::Address:
         glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExactL);
         break;
      case TR::Int32:
         glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExact1);
         break;
      case TR::Float:
         glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExactF);
         break;
      case TR::Double:
         glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExactD);
         break;
      default:
         TR_ASSERT(0, "Bad return data type '%s' for call node [" POINTER_PRINTF_FORMAT "]\n",
                 comp->getDebug()->getName(callNode->getDataType()),
                 callNode);
        }

   if (comp->getOption(TR_BreakOnJ2IThunk))
      *cursor++ = 0xcc;

   // TR::InstOpCode::MOV8RegImm64 rdi, glueAddress
   //
   *(uint16_t *)cursor = 0xbf48;
   cursor += 2;
   *(uint64_t *)cursor = (uintptr_t) cg()->fej9()->getInvokeExactThunkHelperAddress(comp, glueSymRef, callNode->getDataType());
   cursor += 8;

   // Arg stores
   //
   cursor = storeArguments(callNode, cursor, false, NULL);

   if (comp->getOptions()->getVerboseOption(TR_VerboseJ2IThunks))
      {
      // JMPImm4 helper
      //
      *(uint8_t *)cursor++ = 0xe9;
      TR::SymbolReference *helper = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_methodHandleJ2IGlue);
      int32_t disp32 = cg()->branchDisplacementToHelperOrTrampoline(cursor+4, helper);
      *(int32_t *)cursor = disp32;
      cursor += 4;
      }
   else
      {
      // TR::InstOpCode::JMPReg rdi
      //
      *(uint8_t *)cursor++ = 0xff;
      *(uint8_t *)cursor++ = 0xe7;
      }

   if (comp->getOption(TR_TraceCG))
      {
      traceMsg(comp, "\n-- ( Created invokeExact J2I thunk " POINTER_PRINTF_FORMAT " for node " POINTER_PRINTF_FORMAT " )", thunk, callNode);
      }

   return thunk;
   }

TR::Instruction *J9::X86::AMD64::PrivateLinkage::savePreservedRegisters(TR::Instruction *cursor)
   {
   TR::ResolvedMethodSymbol *bodySymbol  = comp()->getJittedMethodSymbol();
   const int32_t          localSize   = _properties.getOffsetToFirstLocal() - bodySymbol->getLocalMappingCursor();
   const int32_t          pointerSize = _properties.getPointerSize();


   int32_t offsetCursor = -localSize - pointerSize;

   int32_t preservedRegStoreBytesSaved = 0;
   if (_properties.getOffsetToFirstLocal() - bodySymbol->getLocalMappingCursor() > 0)
      preservedRegStoreBytesSaved -= 4; // There's an extra mov rsp

   for (int32_t pindex = _properties.getMaxRegistersPreservedInPrologue()-1;
        pindex >= 0;
        pindex--)
      {
      TR::RealRegister *scratch = machine()->getRealRegister(getProperties().getIntegerScratchRegister(0));
      TR::RealRegister::RegNum idx = _properties.getPreservedRegister((uint32_t)pindex);
      TR::RealRegister::RegNum r8  = TR::RealRegister::r8;
      TR::RealRegister *reg = machine()->getRealRegister(idx);
      if (reg->getHasBeenAssignedInMethod() &&
            (reg->getState() != TR::RealRegister::Locked))
         {
         cursor = generateMemRegInstruction(
            cursor,
            TR::Linkage::movOpcodes(MemReg, fullRegisterMovType(reg)),
            generateX86MemoryReference(machine()->getRealRegister(TR::RealRegister::vfp), offsetCursor, cg()),
            reg,
            cg()
            );
         offsetCursor -= pointerSize;
         }
      }

   cursor = cg()->generateDebugCounter(cursor, "cg.prologues:no-preservedRegStoreBytesSaved", 1, TR::DebugCounter::Expensive);

   return cursor;
   }

TR::Instruction *J9::X86::AMD64::PrivateLinkage::restorePreservedRegisters(TR::Instruction *cursor)
   {
   TR::ResolvedMethodSymbol *bodySymbol  = comp()->getJittedMethodSymbol();
   const int32_t          localSize   = _properties.getOffsetToFirstLocal() - bodySymbol->getLocalMappingCursor();
   const int32_t          pointerSize = _properties.getPointerSize();

   int32_t pindex;
   int32_t offsetCursor = -localSize - _properties.getPointerSize();
   for (pindex = _properties.getMaxRegistersPreservedInPrologue()-1;
        pindex >= 0;
        pindex--)
      {
      TR::RealRegister::RegNum idx = _properties.getPreservedRegister((uint32_t)pindex);
      TR::RealRegister *reg = machine()->getRealRegister(idx);
      if (reg->getHasBeenAssignedInMethod())
         {
         cursor = generateRegMemInstruction(
            cursor,
            TR::Linkage::movOpcodes(RegMem, fullRegisterMovType(reg)),
            reg,
            generateX86MemoryReference(machine()->getRealRegister(TR::RealRegister::vfp), offsetCursor, cg()),
            cg()
            );
         offsetCursor -= _properties.getPointerSize();
         }
      }
   cursor = cg()->generateDebugCounter(cursor, "cg.epilogues:no-preservedRegStoreBytesSaved", 1, TR::DebugCounter::Expensive);
   return cursor;
   }

////////////////////////////////////////////////
//
// Call node evaluation
//

int32_t J9::X86::AMD64::PrivateLinkage::argAreaSize(TR::ResolvedMethodSymbol *methodSymbol)
   {
   int32_t result = 0;
   ListIterator<TR::ParameterSymbol>   paramIterator(&(methodSymbol->getParameterList()));
   TR::ParameterSymbol                *paramCursor;
   for (paramCursor = paramIterator.getFirst(); paramCursor; paramCursor = paramIterator.getNext())
      {
      result += paramCursor->getRoundedSize() * ((DOUBLE_SIZED_ARGS && paramCursor->getDataType() != TR::Address) ? 2 : 1);
      }
   return result;
   }

int32_t J9::X86::AMD64::PrivateLinkage::argAreaSize(TR::Node *callNode)
   {
   // TODO: We only need this function because unresolved calls don't have a
   // TR::ResolvedMethodSymbol, and only TR::ResolvedMethodSymbol has
   // getParameterList().  If getParameterList() ever moves to TR::MethodSymbol,
   // then this function becomes unnecessary.
   //
   TR::Node *child;
   int32_t  i;
   int32_t  result  = 0;
   int32_t  firstArgument   = callNode->getFirstArgumentIndex();
   int32_t  lastArgument    = callNode->getNumChildren() - 1;
   for (i=firstArgument; i <= lastArgument; i++)
      {
      child = callNode->getChild(i);
      result += child->getRoundedSize() * ((DOUBLE_SIZED_ARGS && child->getDataType() != TR::Address) ? 2 : 1);
      }
   return result;
   }

int32_t J9::X86::AMD64::PrivateLinkage::buildArgs(TR::Node                             *callNode,
                                          TR::RegisterDependencyConditions  *dependencies)
   {
   TR::MethodSymbol *methodSymbol = callNode->getSymbol()->getMethodSymbol();
   TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
   bool passArgsOnStack;
   bool rightToLeft = methodSymbol && methodSymbol->isHelper()
      && !methodSymRef->isOSRInductionHelper(); //we want the arguments for induceOSR to be passed from left to right as in any other non-helper call
   if (callNode->getOpCode().isIndirect())
      {
      if (methodSymbol->isVirtual() &&
         !methodSymRef->isUnresolved() &&
         !comp()->getOption(TR_FullSpeedDebug) && // On FSD we need to call these through the vtable (because the methodIsOverridden flag is unreliable) so we need args to be in regs
          methodSymbol->isVMInternalNative())
         {
         TR_ResolvedMethod *resolvedMethod = methodSymbol->castToResolvedMethodSymbol()->getResolvedMethod();
         passArgsOnStack = (
            !resolvedMethod->virtualMethodIsOverridden() &&
            !resolvedMethod->isAbstract());
         }
      else
         {
         passArgsOnStack = false;
         }
      }
   else
      {
      passArgsOnStack = methodSymbol->isVMInternalNative() && cg()->supportVMInternalNatives();
      }

   switch (callNode->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod())
      {
      case TR::java_lang_invoke_ComputedCalls_dispatchJ9Method:
         passArgsOnStack = true;
         break;
      default:
      	break;
      }

#ifdef J9VM_OPT_JAVA_CRYPTO_ACCELERATION
   // Violate the right-to-left linkage requirement for JIT helpers for the AES helpers.
   // Call them with Java method private linkage.
   //
   if (cg()->enableAESInHardwareTransformations())
      {
      if (methodSymbol && methodSymbol->isHelper())
         {
         switch (methodSymRef->getReferenceNumber())
            {
            case TR_doAESInHardwareInner:
            case TR_expandAESKeyInHardwareInner:
               rightToLeft = false;
               break;

            default:
               break;
            }
         }
      }
#endif

   return buildPrivateLinkageArgs(callNode, dependencies, rightToLeft, passArgsOnStack);
   }

int32_t J9::X86::AMD64::PrivateLinkage::buildPrivateLinkageArgs(TR::Node                             *callNode,
                                                        TR::RegisterDependencyConditions  *dependencies,
                                                        bool                                 rightToLeft,
                                                        bool                                 passArgsOnStack)
   {
   TR::RealRegister::RegNum   noReg         = TR::RealRegister::NoReg;
   TR::RealRegister          *stackPointer  = machine()->getRealRegister(TR::RealRegister::esp);
   int32_t                    firstArgument = callNode->getFirstArgumentIndex();
   int32_t                    lastArgument  = callNode->getNumChildren() - 1;
   int32_t                    offset        = 0;

   uint16_t                   numIntArgs     = 0,
                              numFloatArgs   = 0,
                              numSpecialArgs = 0;

   int                        numDupedArgRegs = 0;
   TR::Register               *dupedArgRegs[NUM_INTEGER_LINKAGE_REGS + NUM_FLOAT_LINKAGE_REGS];

   // Even though the parameters will be passed on the stack, create dummy linkage registers
   // to ensure that if the call were to be made using the linkage registers (e.g., in a guarded
   // devirtual snippet if it was overridden) then the registers would be available at this
   // point.
   //
   bool createDummyLinkageRegisters = (callNode->getOpCode().isIndirect() && passArgsOnStack) ? true: false;

   int32_t parmAreaSize = argAreaSize(callNode);

   int32_t start, stop, step;
   if (rightToLeft || getProperties().passArgsRightToLeft())
      {
      start = lastArgument;
      stop  = firstArgument - 1;
      step  = -1;
      TR_ASSERT(stop <= start, "Loop must terminate");
      }
   else
      {
      start = firstArgument;
      stop  = lastArgument + 1;
      step  = 1;
      TR_ASSERT(stop >= start, "Loop must terminate");
      }

   // we're going to align the stack depend on the alignment property
   // adjust = something
   // allocateSize = parmAreaSize + adjust;
   // then subtract stackpointer with allocateSize
   uint32_t alignedParmAreaSize = parmAreaSize;

   if (!getProperties().getReservesOutgoingArgsInPrologue() && !callNode->getSymbol()->castToMethodSymbol()->isHelper())
      {
      // Align the stack for alignment larger than 16 bytes(stack is aligned to the multiple of sizeOfJavaPointer by default, which is 4 bytes on 32-bit, and 8 bytes on 64-bit)
      // Basically, we're aligning the start of next frame (after the return address) to a multiple of staceFrameAlignmentInBytes
      // Stack frame alignment is needed for stack allocated object alignment
      uint32_t stackFrameAlignment = cg()->fej9()->getLocalObjectAlignmentInBytes();
      if (stackFrameAlignment > TR::Compiler->om.sizeofReferenceAddress())
         {
         uint32_t frameSize = parmAreaSize + cg()->getFrameSizeInBytes() + getProperties().getRetAddressWidth();
         frameSize += stackFrameAlignment - (frameSize % stackFrameAlignment);
         alignedParmAreaSize = frameSize - cg()->getFrameSizeInBytes() - getProperties().getRetAddressWidth();

         if (comp()->getOption(TR_TraceCG))
            {
            traceMsg(comp(), "parm area size was %d, and is aligned to %d\n", parmAreaSize, alignedParmAreaSize);
            }
         }
      if (alignedParmAreaSize > 0)
         generateRegImmInstruction((alignedParmAreaSize <= 127 ? TR::InstOpCode::SUBRegImms() : TR::InstOpCode::SUBRegImm4()), callNode, stackPointer, alignedParmAreaSize, cg());
      }

   int32_t i;
   for (i = start; i != stop; i += step)
      {
      TR::Node                *child     = callNode->getChild(i);
      TR::DataType             type      = child->getType();
      TR::RealRegister::RegNum  rregIndex = noReg;
      TR::DataType            dt        = type.getDataType();

      switch (dt)
         {
         case TR::Float:
         case TR::Double:
            rregIndex =
               (numFloatArgs < NUM_FLOAT_LINKAGE_REGS) && (!passArgsOnStack || createDummyLinkageRegisters)
               ? getProperties().getFloatArgumentRegister(numFloatArgs)
               : noReg
               ;
            numFloatArgs++;
            break;
         case TR::Address:
         default:
            {
            if (i == firstArgument && !callNode->getSymbolReference()->isUnresolved())
               {
               // TODO:JSR292: This should really be in the front-end
               TR::MethodSymbol *sym = callNode->getSymbol()->castToMethodSymbol();
               switch (sym->getMandatoryRecognizedMethod())
                  {
                  case TR::java_lang_invoke_ComputedCalls_dispatchJ9Method:
                     rregIndex = getProperties().getJ9MethodArgumentRegister();
                     numSpecialArgs++;
                     break;
                  case TR::java_lang_invoke_ComputedCalls_dispatchVirtual:
                  case TR::com_ibm_jit_JITHelpers_dispatchVirtual:
                     rregIndex = getProperties().getVTableIndexArgumentRegister();
                     numSpecialArgs++;
                     break;
                  default:
                  	break;
                  }
               }
            if (rregIndex == noReg)
               {
               rregIndex =
                  (numIntArgs < NUM_INTEGER_LINKAGE_REGS) && (!passArgsOnStack || createDummyLinkageRegisters)
                  ? getProperties().getIntegerArgumentRegister(numIntArgs)
                  : noReg
                  ;
               numIntArgs++;
               }
            break;
            }
         }

      bool willKeepConstRegLiveAcrossCall = true;
      rcount_t oldRefCount = child->getReferenceCount();
      TR::Register *childReg = child->getRegister();
      if (child->getOpCode().isLoadConst() &&
            !childReg &&
            (callNode->getSymbol()->castToMethodSymbol()->isStatic() ||
             callNode->getChild(callNode->getFirstArgumentIndex()) != child))
         {
         child->setReferenceCount(1);
         willKeepConstRegLiveAcrossCall = false;
         }

      TR::Register *vreg = NULL;
      if ((rregIndex != noReg) ||
          (child->getOpCodeValue() != TR::iconst) ||
          (child->getOpCodeValue() == TR::iconst && childReg))
         vreg = cg()->evaluate(child);

      if (rregIndex != noReg)
         {
         bool needsZeroExtension = (child->getDataType() == TR::Int32) && !vreg->areUpperBitsZero();
         TR::Register *preCondReg;

         if (/*!child->getOpCode().isLoadConst() &&*/ (child->getReferenceCount() > 1))
            {
            preCondReg = cg()->allocateRegister(vreg->getKind());
            if (vreg->containsCollectedReference())
               preCondReg->setContainsCollectedReference();

            generateRegRegInstruction(needsZeroExtension? TR::InstOpCode::MOVZXReg8Reg4 : TR::Linkage::movOpcodes(RegReg, movType(child->getDataType())), child, preCondReg, vreg, cg());
            vreg = preCondReg;

            TR_ASSERT(numDupedArgRegs < NUM_INTEGER_LINKAGE_REGS + NUM_FLOAT_LINKAGE_REGS, "assertion failure");
            dupedArgRegs[numDupedArgRegs++] = preCondReg;
            }
         else
            {
            preCondReg = vreg;
            if (needsZeroExtension)
               generateRegRegInstruction(TR::InstOpCode::MOVZXReg8Reg4, child, preCondReg, vreg, cg());
            }

         dependencies->addPreCondition(preCondReg, rregIndex, cg());
         }

      offset += child->getRoundedSize() * ((DOUBLE_SIZED_ARGS && dt != TR::Address) ? 2 : 1);

      if ((rregIndex == noReg) || (rregIndex != noReg && createDummyLinkageRegisters))
         {
         if (vreg)
            generateMemRegInstruction(
               TR::Linkage::movOpcodes(MemReg, fullRegisterMovType(vreg)),
               child,
               generateX86MemoryReference(stackPointer, parmAreaSize-offset, cg()),
               vreg,
               cg()
               );
         else
            {
            int32_t konst = child->getInt();
            generateMemImmInstruction(
               TR::InstOpCode::S8MemImm4,
               child,
               generateX86MemoryReference(stackPointer, parmAreaSize-offset, cg()),
               konst,
               cg()
               );
            }
         }

      if (vreg)
         {
         cg()->decReferenceCount(child);
         ////if (child->getOpCode().isLoadConst() && !childReg)
         if (!willKeepConstRegLiveAcrossCall)
            {
            child->setReferenceCount(oldRefCount-1);
            child->setRegister(NULL);
            }
         }
      else
         child->setReferenceCount(oldRefCount-1);
      }


   // Now that we're finished making the precondition, all the interferences
   // are established, and we can stopUsing these regs.
   //
   for (i = 0; i < numDupedArgRegs; i++)
      {
      cg()->stopUsingRegister(dupedArgRegs[i]);
      }

   // Sanity check
   //
   TR_ASSERT(numIntArgs + numFloatArgs + numSpecialArgs == callNode->getNumChildren() - callNode->getFirstArgumentIndex(), "assertion failure");
   TR_ASSERT(offset == parmAreaSize, "assertion failure");

   return alignedParmAreaSize;
   }


static TR_AtomicRegion X86PicSlotAtomicRegion[] =
   {
   { 0x0, 8 }, // Maximum instruction size that can be patched atomically.
   { 0,0 }
   };


TR::Instruction *J9::X86::AMD64::PrivateLinkage::buildPICSlot(TR::X86PICSlot picSlot, TR::LabelSymbol *mismatchLabel, TR::LabelSymbol *doneLabel, TR::X86CallSite &site)
   {
   TR::Register *cachedAddressRegister = cg()->allocateRegister();

   TR::Node *node = site.getCallNode();
   uint64_t addrToBeCompared = (uint64_t) picSlot.getClassAddress();
   TR::Instruction *firstInstruction;
   if (picSlot.getMethodAddress())
      {
      addrToBeCompared = (uint64_t) picSlot.getMethodAddress();
      firstInstruction = generateRegImm64Instruction(TR::InstOpCode::MOV8RegImm64, node, cachedAddressRegister, addrToBeCompared, cg());
      }
   else
      firstInstruction = generateRegImm64Instruction(TR::InstOpCode::MOV8RegImm64, node, cachedAddressRegister, (uint64_t)picSlot.getClassAddress(), cg());


   firstInstruction->setNeedsGCMap(site.getPreservedRegisterMask());

   if (!site.getFirstPICSlotInstruction())
      site.setFirstPICSlotInstruction(firstInstruction);

   if (picSlot.needsPicSlotAlignment())
      {
      generateBoundaryAvoidanceInstruction(
         X86PicSlotAtomicRegion,
         8,
         8,
         firstInstruction,
         cg());
      }

   TR::Register *vftReg = site.evaluateVFT();

   // The ordering of these registers in this compare instruction is important in order to get the
   // VFT register into the ModRM r/m field of the generated instruction.
   //
   if (picSlot.getMethodAddress())
      generateMemRegInstruction(TR::InstOpCode::CMP8MemReg, node,
                                generateX86MemoryReference(vftReg, picSlot.getSlot(), cg()), cachedAddressRegister, cg());
   else
      generateRegRegInstruction(TR::InstOpCode::CMP8RegReg, node, cachedAddressRegister, vftReg, cg());

   cg()->stopUsingRegister(cachedAddressRegister);

   if (picSlot.needsJumpOnNotEqual())
      {
      if (picSlot.needsLongConditionalBranch())
         {
         generateLongLabelInstruction(TR::InstOpCode::JNE4, node, mismatchLabel, cg());
         }
      else
         {
         TR::InstOpCode::Mnemonic op = picSlot.needsShortConditionalBranch() ? TR::InstOpCode::JNE1 : TR::InstOpCode::JNE4;
         generateLabelInstruction(op, node, mismatchLabel, cg());
         }
      }
   else if (picSlot.needsJumpOnEqual())
      {
      if (picSlot.needsLongConditionalBranch())
         {
         generateLongLabelInstruction(TR::InstOpCode::JE4, node, mismatchLabel, cg());
         }
      else
         {
         TR::InstOpCode::Mnemonic op = picSlot.needsShortConditionalBranch() ? TR::InstOpCode::JE1 : TR::InstOpCode::JE4;
         generateLabelInstruction(op, node, mismatchLabel, cg());
         }
      }

   TR::Instruction *instr;
   if (picSlot.getMethod())
      {
      TR::SymbolReference * callSymRef = comp()->getSymRefTab()->findOrCreateMethodSymbol(
            node->getSymbolReference()->getOwningMethodIndex(), -1, picSlot.getMethod(), TR::MethodSymbol::Virtual);

      instr = generateImmSymInstruction(TR::InstOpCode::CALLImm4, node, (intptr_t)picSlot.getMethod()->startAddressForJittedMethod(), callSymRef, cg());
      }
   else if (picSlot.getHelperMethodSymbolRef())
      {
      TR::MethodSymbol *helperMethod = picSlot.getHelperMethodSymbolRef()->getSymbol()->castToMethodSymbol();
      instr = generateImmSymInstruction(TR::InstOpCode::CALLImm4, node, (uint32_t)(uintptr_t)helperMethod->getMethodAddress(), picSlot.getHelperMethodSymbolRef(), cg());
      }
   else
      {
      instr = generateImmInstruction(TR::InstOpCode::CALLImm4, node, 0, cg());
      }

   instr->setNeedsGCMap(site.getPreservedRegisterMask());

   // Put a GC map on this label, since the instruction after it may provide
   // the return address in this stack frame while PicBuilder is active
   //
   // TODO: Can we get rid of some of these?  Maybe they don't cost anything.
   //
   if (picSlot.needsJumpToDone())
      {
      instr = generateLabelInstruction(TR::InstOpCode::JMP4, node, doneLabel, cg());
      instr->setNeedsGCMap(site.getPreservedRegisterMask());
      }

   if (picSlot.generateNextSlotLabelInstruction())
      {
      generateLabelInstruction(TR::InstOpCode::label, node, mismatchLabel, cg());
      }

   return firstInstruction;
   }


static TR_AtomicRegion amd64IPicAtomicRegions[] =
   {
   { 0x02, 8 }, // Class test immediate 1
   { 0x1a, 8 }, // Class test immediate 2
   { 0x3b, 4 }, // IPICInit call displacement
   { 0,0 }      // (null terminator)

   // Some other important regions
   //{ 0x10, 4 }, // Call displacement 1  (no race here)
   //{ 0x28, 4 }, // Call displacement 2  (no race here)
   };

void J9::X86::AMD64::PrivateLinkage::buildIPIC(TR::X86CallSite &site, TR::LabelSymbol *entryLabel, TR::LabelSymbol *doneLabel, uint8_t *thunk)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());
   int32_t numIPICs = 0;

   TR_ASSERT(doneLabel, "a doneLabel is required for PIC dispatches");

   if (entryLabel)
      generateLabelInstruction(TR::InstOpCode::label, site.getCallNode(), entryLabel, cg());

   int32_t numIPicSlots = IPicParameters.defaultNumberOfSlots;

   TR::SymbolReference *callHelperSymRef =
      cg()->symRefTab()->findOrCreateRuntimeHelper(TR_X86populateIPicSlotCall, true, true, false);

   static char *interfaceDispatchUsingLastITableStr         = feGetEnv("TR_interfaceDispatchUsingLastITable");
   static char *numIPicSlotsStr                             = feGetEnv("TR_numIPicSlots");
   static char *numIPicSlotsBeforeLastITable                = feGetEnv("TR_numIPicSlotsBeforeLastITable");
   static char *breakBeforeIPICUsingLastITable              = feGetEnv("TR_breakBeforeIPICUsingLastITable");

   if (numIPicSlotsStr)
      numIPicSlots = atoi(numIPicSlotsStr);

   bool useLastITableCache = site.useLastITableCache() || interfaceDispatchUsingLastITableStr;
   if (useLastITableCache)
      {
      if (numIPicSlotsBeforeLastITable)
         {
         numIPicSlots = atoi(numIPicSlotsBeforeLastITable);
         }
      if (breakBeforeIPICUsingLastITable)
         {
         // This will occasionally put a break before an IPIC that does not use
         // the lastITable cache, because we might still change our mind
         // below.  However, this is where we want the break instruction, so
         // let's just lay it down.  It's likely not worth the effort to get
         // this exactly right in all cases.
         //
         generateInstruction(TR::InstOpCode::INT3, site.getCallNode(), cg());
         }
      }


   if (numIPicSlots > 1)
      {
      TR::X86PICSlot emptyPicSlot = TR::X86PICSlot(IPicParameters.defaultSlotAddress, NULL);
      emptyPicSlot.setNeedsShortConditionalBranch();
      emptyPicSlot.setJumpOnNotEqual();
      emptyPicSlot.setNeedsPicSlotAlignment();
      emptyPicSlot.setHelperMethodSymbolRef(callHelperSymRef);
      emptyPicSlot.setGenerateNextSlotLabelInstruction();

      // Generate all slots except the last
      // (short branch to next slot, jump to doneLabel)
      //
      for (int32_t i = 1; i < numIPicSlots; i++)
         {
         TR::LabelSymbol *nextSlotLabel = generateLabelSymbol(cg());
         buildPICSlot(emptyPicSlot, nextSlotLabel, doneLabel, site);
         }
      }

   // Configure the last slot
   //
   TR::LabelSymbol *lookupDispatchSnippetLabel = generateLabelSymbol(cg());
   TR::X86PICSlot lastPicSlot = TR::X86PICSlot(IPicParameters.defaultSlotAddress, NULL, false);
   lastPicSlot.setJumpOnNotEqual();
   lastPicSlot.setNeedsPicSlotAlignment();
   lastPicSlot.setHelperMethodSymbolRef(callHelperSymRef);
   TR::Instruction *slotPatchInstruction = NULL;

   TR::Method *method = site.getMethodSymbol()->getMethod();
   TR_OpaqueClassBlock *declaringClass = NULL;
   uintptr_t itableIndex;
   if (  useLastITableCache
      && (declaringClass = site.getSymbolReference()->getOwningMethod(comp())->getResolvedInterfaceMethod(site.getSymbolReference()->getCPIndex(), &itableIndex))
      && performTransformation(comp(), "O^O useLastITableCache for n%dn itableIndex=%d: %.*s.%.*s%.*s\n",
            site.getCallNode()->getGlobalIndex(), (int)itableIndex,
            method->classNameLength(), method->classNameChars(),
            method->nameLength(),      method->nameChars(),
            method->signatureLength(), method->signatureChars()))
      {
      buildInterfaceDispatchUsingLastITable (site, numIPicSlots, lastPicSlot, slotPatchInstruction, doneLabel, lookupDispatchSnippetLabel, declaringClass, itableIndex);
      }
   else
      {
      // Last PIC slot is long branch to lookup snippet, fall through to doneLabel
      lastPicSlot.setNeedsLongConditionalBranch();
      slotPatchInstruction = buildPICSlot(lastPicSlot, lookupDispatchSnippetLabel, NULL, site);
      }

   TR::Instruction *startOfPicInstruction = site.getFirstPICSlotInstruction();
   TR::X86PicDataSnippet *snippet = new (trHeapMemory()) TR::X86PicDataSnippet(
      numIPicSlots,
      startOfPicInstruction,
      lookupDispatchSnippetLabel,
      doneLabel,
      site.getSymbolReference(),
      slotPatchInstruction,
      site.getThunkAddress(),
      true,
      cg());

   snippet->gcMap().setGCRegisterMask(site.getPreservedRegisterMask());
   cg()->addSnippet(snippet);

   cg()->incPicSlotCountBy(IPicParameters.defaultNumberOfSlots);
   numIPICs = IPicParameters.defaultNumberOfSlots;

   cg()->reserveNTrampolines(numIPICs);
   }

void J9::X86::AMD64::PrivateLinkage::buildVirtualOrComputedCall(TR::X86CallSite &site, TR::LabelSymbol *entryLabel, TR::LabelSymbol *doneLabel, uint8_t *thunk)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp()->fe());
   if (entryLabel)
      generateLabelInstruction(TR::InstOpCode::label, site.getCallNode(), entryLabel, cg());

   TR::SymbolReference *methodSymRef = site.getSymbolReference();
   if (comp()->getOption(TR_TraceCG))
      {
      traceMsg(comp(), "buildVirtualOrComputedCall(%p), isComputed=%d\n", site.getCallNode(), methodSymRef->getSymbol()->castToMethodSymbol()->isComputed());
      }

   bool evaluateVftEarly = methodSymRef->isUnresolved()
      || !fej9->isResolvedVirtualDispatchGuaranteed(comp());

   if (methodSymRef->getSymbol()->castToMethodSymbol()->isComputed())
      {
      buildVFTCall(site, TR::InstOpCode::CALLReg, site.evaluateVFT(), NULL);
      }
   else if (evaluateVftEarly)
      {
      site.evaluateVFT(); // We must evaluate the VFT here to avoid a later evaluation that pollutes the VPic shape.
      buildVPIC(site, entryLabel, doneLabel);
      }
   else if (site.resolvedVirtualShouldUseVFTCall())
      {
      // Call through VFT
      //
      if (comp()->compileRelocatableCode())
         {
         // Non-SVM AOT still has to force unresolved virtual dispatch, which
         // works there because it won't transform other things into virtual
         // calls, e.g. invokeinterface of an Object method.
         TR_ASSERT_FATAL(
            comp()->getOption(TR_UseSymbolValidationManager),
            "resolved virtual dispatch in AOT requires SVM");

         void *thunk = site.getThunkAddress();
         TR_OpaqueMethodBlock *method = methodSymRef
            ->getSymbol()
            ->castToResolvedMethodSymbol()
            ->getResolvedMethod()
            ->getPersistentIdentifier();

         comp()->getSymbolValidationManager()
            ->addJ2IThunkFromMethodRecord(thunk, method);
         }

      buildVFTCall(site, TR::InstOpCode::CALLMem, NULL, generateX86MemoryReference(site.evaluateVFT(), methodSymRef->getOffset(), cg()));
      }
   else
      {
      site.evaluateVFT(); // We must evaluate the VFT here to avoid a later evaluation that pollutes the VPic shape.
      buildVPIC(site, entryLabel, doneLabel);
      }
   }

TR::Register *J9::X86::AMD64::PrivateLinkage::buildJNIDispatch(TR::Node *callNode)
   {
   TR_ASSERT(0, "AMD64 implements JNI dispatch using a system linkage");
   return NULL;
   }

#endif
