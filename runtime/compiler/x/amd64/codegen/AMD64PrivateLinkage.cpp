/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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
#include "codegen/AMD64CallSnippet.hpp"
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
#include "env/jittypes.h"
#include "objectfmt/GlobalFunctionCallData.hpp"
#include "objectfmt/ObjectFormat.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "x/amd64/codegen/AMD64GuardedDevirtualSnippet.hpp"
#include "x/codegen/CallSnippet.hpp"
#include "x/codegen/CheckFailureSnippet.hpp"
#include "x/codegen/HelperCallSnippet.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"
#include "x/codegen/X86Ops_inlines.hpp"

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
      TR_X86OpCodes op,
      TR::RealRegister::RegNum regIndex,
      int32_t offset,
      TR::CodeGenerator *cg)
   {
   /* TODO:AMD64: Share code with the other flushArgument */
   cursor = TR_X86OpCode(op).binary(cursor);

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
      TR_X86OpCodes op,
      int32_t offset)
   {
   int32_t size = TR_X86OpCode(op).length() + 1;   // length including ModRM + 1 SIB
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
   TR_X86OpCodes            op  = BADIA32Op;
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
   TR_X86OpCodes opCode = TR::Linkage::movOpcodes(operandType, movType(dataType));

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
   codeSize += 12;  // +10 MOV8RegImm64 +2 JMPReg

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
         glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_AMD64icallVMprJavaSendVirtual0, false, false, false);
         break;
      case TR::Int64:
         glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_AMD64icallVMprJavaSendVirtualJ, false, false, false);
         break;
      case TR::Address:
         glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_AMD64icallVMprJavaSendVirtualL, false, false, false);
         break;
      case TR::Int32:
         glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_AMD64icallVMprJavaSendVirtual1, false, false, false);
         break;
      case TR::Float:
         glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_AMD64icallVMprJavaSendVirtualF, false, false, false);
         break;
      case TR::Double:
         glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_AMD64icallVMprJavaSendVirtualD, false, false, false);
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

   // MOV8RegImm64 rdi, glueAddress
   //
   *(uint16_t *)cursor = 0xbf48;
   cursor += 2;
   *(uint64_t *)cursor = (uintptr_t)glueSymRef->getMethodAddress();
   cursor += 8;

   // JMPReg rdi
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
   codeSize += 10;  // MOV8RegImm64
   if (comp->getOption(TR_BreakOnJ2IThunk))
      codeSize += 1; // breakpoint opcode
   if (comp->getOptions()->getVerboseOption(TR_VerboseJ2IThunks))
      codeSize += 5; // JMPImm4
   else
      codeSize += 2; // JMPReg

   TR_J2IThunkTable *thunkTable = comp->getPersistentInfo()->getInvokeExactJ2IThunkTable();
   TR_J2IThunk      *thunk      = TR_J2IThunk::allocate(codeSize, signature, cg(), thunkTable);
   uint8_t          *cursor     = thunk->entryPoint();

   TR::SymbolReference  *glueSymRef;
   switch (callNode->getDataType())
      {
      case TR::NoType:
         glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExact0, false, false, false);
         break;
      case TR::Int64:
         glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExactJ, false, false, false);
         break;
      case TR::Address:
         glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExactL, false, false, false);
         break;
      case TR::Int32:
         glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExact1, false, false, false);
         break;
      case TR::Float:
         glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExactF, false, false, false);
         break;
      case TR::Double:
         glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExactD, false, false, false);
         break;
      default:
         TR_ASSERT(0, "Bad return data type '%s' for call node [" POINTER_PRINTF_FORMAT "]\n",
                 comp->getDebug()->getName(callNode->getDataType()),
                 callNode);
        }

   if (comp->getOption(TR_BreakOnJ2IThunk))
      *cursor++ = 0xcc;

   // MOV8RegImm64 rdi, glueAddress
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
      TR::SymbolReference *helper = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_methodHandleJ2IGlue, false, false, false);
      int32_t disp32 = cg()->branchDisplacementToHelperOrTrampoline(cursor+4, helper);
      *(int32_t *)cursor = disp32;
      cursor += 4;
      }
   else
      {
      // JMPReg rdi
      //
      *(uint8_t *)cursor++ = 0xff;
      *(uint8_t *)cursor++ = 0xe7;
      }

   traceMsg(comp, "\n-- ( Created invokeExact J2I thunk " POINTER_PRINTF_FORMAT " for node " POINTER_PRINTF_FORMAT " )", thunk, callNode);

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
      case TR::java_lang_invoke_MethodHandle_invokeWithArgumentsHelper:
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
         traceMsg(comp(), "parm area size was %d, and is aligned to %d\n", parmAreaSize, alignedParmAreaSize);
         }
      if (alignedParmAreaSize > 0)
         generateRegImmInstruction((alignedParmAreaSize <= 127 ? SUBRegImms() : SUBRegImm4()), callNode, stackPointer, alignedParmAreaSize, cg());
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

            generateRegRegInstruction(needsZeroExtension? MOVZXReg8Reg4 : TR::Linkage::movOpcodes(RegReg, movType(child->getDataType())), child, preCondReg, vreg, cg());
            vreg = preCondReg;

            TR_ASSERT(numDupedArgRegs < NUM_INTEGER_LINKAGE_REGS + NUM_FLOAT_LINKAGE_REGS, "assertion failure");
            dupedArgRegs[numDupedArgRegs++] = preCondReg;
            }
         else
            {
            preCondReg = vreg;
            if (needsZeroExtension)
               generateRegRegInstruction(MOVZXReg8Reg4, child, preCondReg, vreg, cg());
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
               S8MemImm4,
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
   bool readOnlyCode = cg()->comp()->getGenerateReadOnlyCode();
   TR::Register *cachedAddressRegister = cg()->allocateRegister();

   TR::Node *node = site.getCallNode();
   uint64_t addrToBeCompared = (uint64_t) picSlot.getClassAddress();
   TR::Instruction *firstInstruction;
   if (picSlot.getMethodAddress())
      {
      addrToBeCompared = (uint64_t) picSlot.getMethodAddress();

      if (readOnlyCode)
         {
         TR::MemoryReference *constMR = TR::TreeEvaluator::generateLoadConstantMemoryReference(node, static_cast<intptr_t>(addrToBeCompared), cg(), TR_ClassAddress);
         firstInstruction = generateRegMemInstruction(LRegMem(), node, cachedAddressRegister, constMR, cg());
         }
      else
         {
         firstInstruction = generateRegImm64Instruction(MOV8RegImm64, node, cachedAddressRegister, addrToBeCompared, cg());
         }
      }
   else
      {
      if (readOnlyCode)
         {
         TR::MemoryReference *constMR = TR::TreeEvaluator::generateLoadConstantMemoryReference(node, static_cast<intptr_t>(picSlot.getClassAddress()), cg(), TR_ClassAddress);
         firstInstruction = generateRegMemInstruction(LRegMem(), node, cachedAddressRegister, constMR, cg());
         }
      else
         {
         firstInstruction = generateRegImm64Instruction(MOV8RegImm64, node, cachedAddressRegister, (uint64_t)picSlot.getClassAddress(), cg());
         }
      }

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
      generateMemRegInstruction(CMP8MemReg, node,
                                generateX86MemoryReference(vftReg, picSlot.getSlot(), cg()), cachedAddressRegister, cg());
   else
      generateRegRegInstruction(CMP8RegReg, node, cachedAddressRegister, vftReg, cg());

   cg()->stopUsingRegister(cachedAddressRegister);

   if (picSlot.needsJumpOnNotEqual())
      {
      if (picSlot.needsLongConditionalBranch())
         {
         generateLongLabelInstruction(JNE4, node, mismatchLabel, cg());
         }
      else
         {
         TR_X86OpCodes op = picSlot.needsShortConditionalBranch() ? JNE1 : JNE4;
         generateLabelInstruction(op, node, mismatchLabel, cg());
         }
      }
   else if (picSlot.needsJumpOnEqual())
      {
      if (picSlot.needsLongConditionalBranch())
         {
         generateLongLabelInstruction(JE4, node, mismatchLabel, cg());
         }
      else
         {
         TR_X86OpCodes op = picSlot.needsShortConditionalBranch() ? JE1 : JE4;
         generateLabelInstruction(op, node, mismatchLabel, cg());
         }
      }

   TR::Instruction *instr;
   if (picSlot.getMethod())
      {
      TR::SymbolReference * callSymRef = comp()->getSymRefTab()->findOrCreateMethodSymbol(
            node->getSymbolReference()->getOwningMethodIndex(), -1, picSlot.getMethod(), TR::MethodSymbol::Virtual);

      if (readOnlyCode)
         {
         // Not a global function, but a "local function" ??
         TR::GlobalFunctionCallData data(callSymRef, node, 0, (uintptr_t)picSlot.getMethod()->startAddressForJittedMethod(), 0, TR_NoRelocation, true, cg());
         instr = cg()->getObjFmt()->emitGlobalFunctionCall(data);
         }
      else
         {
         instr = generateImmSymInstruction(CALLImm4, node, (intptr_t)picSlot.getMethod()->startAddressForJittedMethod(), callSymRef, cg());
         }
      }
   else if (picSlot.getHelperMethodSymbolRef())
      {
      TR::MethodSymbol *helperMethod = picSlot.getHelperMethodSymbolRef()->getSymbol()->castToMethodSymbol();
      instr = generateImmSymInstruction(CALLImm4, node, (uint32_t)(uintptr_t)helperMethod->getMethodAddress(), picSlot.getHelperMethodSymbolRef(), cg());
      }
   else
      {
      TR_ASSERT_FATAL(!readOnlyCode, "target address required for read only");
      instr = generateImmInstruction(CALLImm4, node, 0, cg());
      }

   instr->setNeedsGCMap(site.getPreservedRegisterMask());

   // Put a GC map on this label, since the instruction after it may provide
   // the return address in this stack frame while PicBuilder is active
   //
   // TODO: Can we get rid of some of these?  Maybe they don't cost anything.
   //
   if (picSlot.needsJumpToDone())
      {
      instr = generateLabelInstruction(JMP4, node, doneLabel, cg());
      instr->setNeedsGCMap(site.getPreservedRegisterMask());
      }

   if (picSlot.generateNextSlotLabelInstruction())
      {
      generateLabelInstruction(LABEL, node, mismatchLabel, cg());
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
   if (comp()->getGenerateReadOnlyCode())
      {
      buildNoPatchingIPIC(site, entryLabel, doneLabel, thunk);
      return;
      }

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());
   int32_t numIPICs = 0;

   TR_ASSERT(doneLabel, "a doneLabel is required for PIC dispatches");

   if (entryLabel)
      generateLabelInstruction(LABEL, site.getCallNode(), entryLabel, cg());

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
         generateInstruction(BADIA32Op, site.getCallNode(), cg());
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


void J9::X86::AMD64::PrivateLinkage::buildNoPatchingIPIC(TR::X86CallSite &site, TR::LabelSymbol *entryLabel, TR::LabelSymbol *doneLabel, uint8_t *thunk)
   {

   TR_ASSERT(doneLabel, "a doneLabel is required for PIC dispatches");

   if (entryLabel)
      generateLabelInstruction(LABEL, site.getCallNode(), entryLabel, cg());

   ccInterfaceData *ccInterfaceDataAddress =
      reinterpret_cast<ccInterfaceData *>(cg()->allocateCodeMemory(sizeof(ccInterfaceData), false));

   if (!ccInterfaceDataAddress)
      {
      comp()->failCompilation<TR::CompilationException>("Could not allocate interface dispatch metadata");
      }

   ccInterfaceDataAddress->slot1Class = static_cast<intptr_t>(-1);
   TR::SymbolReference *dispatchIPicSlot1MethodReadOnlySymRef =
      cg()->symRefTab()->findOrCreateRuntimeHelper(TR_AMD64dispatchIPicSlot1MethodReadOnly, false, false, false);
   ccInterfaceDataAddress->slot1Method = reinterpret_cast<intptr_t>(dispatchIPicSlot1MethodReadOnlySymRef->getMethodAddress());

   ccInterfaceDataAddress->slot2Class = static_cast<intptr_t>(-1);
   TR::SymbolReference *dispatchIPicSlot2MethodReadOnlySymRef =
      cg()->symRefTab()->findOrCreateRuntimeHelper(TR_AMD64dispatchIPicSlot2MethodReadOnly, false, false, false);
   ccInterfaceDataAddress->slot2Method = reinterpret_cast<intptr_t>(dispatchIPicSlot2MethodReadOnlySymRef->getMethodAddress());

   TR::SymbolReference *IPicResolveReadOnlySymRef =
      cg()->symRefTab()->findOrCreateRuntimeHelper(TR_AMD64IPicResolveReadOnly, false, false, false);
   ccInterfaceDataAddress->slowInterfaceDispatchMethod = reinterpret_cast<intptr_t>(IPicResolveReadOnlySymRef->getMethodAddress());

   ccInterfaceDataAddress->cpAddress = reinterpret_cast<intptr_t>(site.getSymbolReference()->getOwningMethod(comp())->constantPool());
   ccInterfaceDataAddress->cpIndex = static_cast<intptr_t>(site.getSymbolReference()->getCPIndexForVM());
   ccInterfaceDataAddress->interfaceClassAddress = 0;
   ccInterfaceDataAddress->itableOffset = 0;

   intptr_t interfaceDataAddress = reinterpret_cast<intptr_t>(ccInterfaceDataAddress);

   TR::StaticSymbol *interfaceDataSlot1ClassSymbol  =
      TR::StaticSymbol::createWithAddress(comp()->trHeapMemory(), TR::Address, reinterpret_cast<void *>(interfaceDataAddress + offsetof(ccInterfaceData, slot1Class)));
   interfaceDataSlot1ClassSymbol->setNotDataAddress();
   TR::SymbolReference *interfaceDataSlot1ClassSymRef = new (comp()->trHeapMemory()) TR::SymbolReference(comp()->getSymRefTab(), interfaceDataSlot1ClassSymbol, 0);

   TR::Register *vftRegister = site.evaluateVFT();

   // startSlot1:
   //    cmp Rclass, [RIP + slot1Class]
   //    jne short startSlot2
   //    call [RIP + slot1Method]
   //    jmp short done
   //
   TR::LabelSymbol *startSlot1Label = generateLabelSymbol(cg());
   generateLabelInstruction(LABEL, site.getCallNode(), startSlot1Label, cg());
   generateRegMemInstruction(CMP8RegMem, site.getCallNode(),
                             vftRegister,
                             new (comp()->trHeapMemory()) TR::MemoryReference(interfaceDataSlot1ClassSymRef, cg(), true),
                             cg());

   TR::LabelSymbol *startSlot2Label = generateLabelSymbol(cg());
   generateLabelInstruction(JNE4, site.getCallNode(), startSlot2Label, cg());

   TR::StaticSymbol *interfaceDataSlot1MethodSymbol  =
      TR::StaticSymbol::createWithAddress(comp()->trHeapMemory(), TR::Address, reinterpret_cast<void *>(interfaceDataAddress + offsetof(ccInterfaceData, slot1Method)));
   interfaceDataSlot1MethodSymbol->setNotDataAddress();
   TR::SymbolReference *interfaceDataSlot1MethodSymRef = new (comp()->trHeapMemory()) TR::SymbolReference(comp()->getSymRefTab(), interfaceDataSlot1MethodSymbol, 0);
   TR::Instruction *slotCallInstruction = generateMemInstruction(CALLMem, site.getCallNode(),
      new (comp()->trHeapMemory()) TR::MemoryReference(interfaceDataSlot1MethodSymRef, cg(), true),
      cg());
   slotCallInstruction->setNeedsGCMap(site.getPreservedRegisterMask());

   generateLabelInstruction(JMP4, site.getCallNode(), doneLabel, cg());

   // startSlot2:
   //    cmp Rclass, [RIP + slot2Class]
   //    jne short interfaceDispatchReadOnlySnippet
   //    call [RIP + slot2Method]
   // done:
   //
   generateLabelInstruction(LABEL, site.getCallNode(), startSlot2Label, cg());

   TR::StaticSymbol *interfaceDataSlot2ClassSymbol  =
      TR::StaticSymbol::createWithAddress(comp()->trHeapMemory(), TR::Address, reinterpret_cast<void *>(interfaceDataAddress + offsetof(ccInterfaceData, slot2Class)));
   interfaceDataSlot2ClassSymbol->setNotDataAddress();
   TR::SymbolReference *interfaceDataSlot2ClassSymRef = new (comp()->trHeapMemory()) TR::SymbolReference(comp()->getSymRefTab(), interfaceDataSlot2ClassSymbol, 0);
   generateRegMemInstruction(CMP8RegMem, site.getCallNode(),
                             vftRegister,
                             new (comp()->trHeapMemory()) TR::MemoryReference(interfaceDataSlot2ClassSymRef, cg(), true),
                             cg());

   TR::LabelSymbol *interfaceDispatchReadOnlySnippetLabel = generateLabelSymbol(cg());

   generateLabelInstruction(JNE4, site.getCallNode(), interfaceDispatchReadOnlySnippetLabel, cg());

   TR::StaticSymbol *interfaceDataSlot2MethodSymbol  =
      TR::StaticSymbol::createWithAddress(comp()->trHeapMemory(), TR::Address, reinterpret_cast<void *>(interfaceDataAddress + offsetof(ccInterfaceData, slot2Method)));
   interfaceDataSlot2MethodSymbol->setNotDataAddress();
   TR::SymbolReference *interfaceDataSlot2MethodSymRef = new (comp()->trHeapMemory()) TR::SymbolReference(comp()->getSymRefTab(), interfaceDataSlot2MethodSymbol, 0);

   slotCallInstruction = generateMemInstruction(CALLMem, site.getCallNode(),
      new (comp()->trHeapMemory()) TR::MemoryReference(interfaceDataSlot2MethodSymRef, cg(), true),
      cg());
   slotCallInstruction->setNeedsGCMap(site.getPreservedRegisterMask());

   generateLabelInstruction(LABEL, site.getCallNode(), doneLabel, cg());

   TR::X86InterfaceDispatchReadOnlySnippet *snippet = new (trHeapMemory()) TR::X86InterfaceDispatchReadOnlySnippet(
      interfaceDispatchReadOnlySnippetLabel,
      site.getCallNode(),
      interfaceDataAddress,
      startSlot1Label,
      doneLabel,
      cg());

   snippet->gcMap().setGCRegisterMask(site.getPreservedRegisterMask());
   cg()->addSnippet(snippet);
   }


void
J9::X86::AMD64::PrivateLinkage::buildNoPatchingVirtualDispatchWithResolve(
      TR::X86CallSite &site,
      TR::Register *vftRegister,
      TR::LabelSymbol *entryLabel,
      TR::LabelSymbol *doneLabel)
   {
   if (entryLabel)
      generateLabelInstruction(LABEL, site.getCallNode(), entryLabel, cg());

   /**
    * Define the shape of the structure that will be allocated in a data area
    * to guide the resolution of this virtual dispatch.
    *
    * BEWARE: Offsets of fields in this structure are described in
    * x/runtime/X86PicBuilder.inc so any changes to the size or ordering of
    * these fields will require a corresponding change there as well.
    */
   struct ccResolveVirtualData
      {
      intptr_t cpAddress;
      intptr_t cpIndex;
      intptr_t directMethod;
      intptr_t j2iThunk;
      int32_t vtableOffset;
      };

   ccResolveVirtualData *ccResolveVirtualDataAddress =
      reinterpret_cast<ccResolveVirtualData *>(cg()->allocateCodeMemory(sizeof(ccResolveVirtualData), false));

   if (!ccResolveVirtualDataAddress)
      {
      comp()->failCompilation<TR::CompilationException>("Could not allocate resolve virtual dispatch metadata");
      }

   ccResolveVirtualDataAddress->cpAddress = reinterpret_cast<intptr_t>(site.getSymbolReference()->getOwningMethod(comp())->constantPool());
   ccResolveVirtualDataAddress->cpIndex = static_cast<intptr_t>(site.getSymbolReference()->getCPIndexForVM());
   ccResolveVirtualDataAddress->directMethod = 0;
   ccResolveVirtualDataAddress->j2iThunk = reinterpret_cast<intptr_t>(site.getThunkAddress());
   ccResolveVirtualDataAddress->vtableOffset = 0;

   intptr_t resolveVirtualDataAddress = reinterpret_cast<intptr_t>(ccResolveVirtualDataAddress);

   TR::StaticSymbol *resolveVirtualDataSymbol =
      TR::StaticSymbol::createWithAddress(comp()->trHeapMemory(), TR::Int32, reinterpret_cast<void *>(resolveVirtualDataAddress + offsetof(ccResolveVirtualData, vtableOffset)));
   resolveVirtualDataSymbol->setNotDataAddress();
   TR::SymbolReference *resolveVirtualDataSymRef = new (comp()->trHeapMemory()) TR::SymbolReference(comp()->getSymRefTab(), resolveVirtualDataSymbol, 0);

   // loadResolvedVtableOffset:
   //    movsx RvtableOffset, dword [RIP + resolvedVtableOffset]
   //
   TR::LabelSymbol *loadResolvedVtableOffsetLabel = generateLabelSymbol(cg());
   generateLabelInstruction(LABEL, site.getCallNode(), loadResolvedVtableOffsetLabel, cg());

   TR::Register *vtableOffsetReg = cg()->allocateRegister();
   generateRegMemInstruction(MOVSXReg8Mem4, site.getCallNode(),
                             vtableOffsetReg,
                             new (comp()->trHeapMemory()) TR::MemoryReference(resolveVirtualDataSymRef, cg(), true),
                             cg());

   // The following test will only succeed once.  Once resolved, the vtable offset
   // will be known, the test will fail, and the dispatch will be done through the vtable.
   //
   //    test RvtableOffset, RvtableOffset
   //    jz resolveVirtualDispatchReadOnlyDataSnippet
   //
   TR::LabelSymbol *resolveVirtualDispatchReadOnlyDataSnippetLabel = generateLabelSymbol(cg());

   TR::X86ResolveVirtualDispatchReadOnlyDataSnippet *snippet = new (trHeapMemory()) TR::X86ResolveVirtualDispatchReadOnlyDataSnippet(
      resolveVirtualDispatchReadOnlyDataSnippetLabel,
      site.getCallNode(),
      resolveVirtualDataAddress,
      loadResolvedVtableOffsetLabel,
      doneLabel,
      cg());

   snippet->gcMap().setGCRegisterMask(site.getPreservedRegisterMask());
   cg()->addSnippet(snippet);

   generateRegRegInstruction(TEST8RegReg, site.getCallNode(), vtableOffsetReg, vtableOffsetReg, cg());
   generateLabelInstruction(JE4, site.getCallNode(), resolveVirtualDispatchReadOnlyDataSnippetLabel, cg());

   //    call [Rclass + RvtableOffset + 0x00000000]
   //
   TR::MemoryReference *dispatchMR = generateX86MemoryReference(vftRegister, vtableOffsetReg, 0, 0, cg());
   dispatchMR->setForceWideDisplacement();
   TR::Instruction *callInstr = generateMemInstruction(CALLMem, site.getCallNode(), dispatchMR, cg());
   callInstr->setNeedsGCMap(site.getPreservedRegisterMask());

   site.getPostConditionsUnderConstruction()->addPostCondition(vtableOffsetReg, TR::RealRegister::r8, cg());

   generateLabelInstruction(LABEL, site.getCallNode(), doneLabel, cg());
   }


void J9::X86::AMD64::PrivateLinkage::buildVirtualOrComputedCall(TR::X86CallSite &site, TR::LabelSymbol *entryLabel, TR::LabelSymbol *doneLabel, uint8_t *thunk)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp()->fe());
   if (entryLabel)
      generateLabelInstruction(LABEL, site.getCallNode(), entryLabel, cg());

   TR::SymbolReference *methodSymRef = site.getSymbolReference();
   if (comp()->getOption(TR_TraceCG))
      {
      traceMsg(comp(), "buildVirtualOrComputedCall(%p), isComputed=%d\n", site.getCallNode(), methodSymRef->getSymbol()->castToMethodSymbol()->isComputed());
      }
   bool evaluateVftEarly = methodSymRef->isUnresolved() || fej9->forceUnresolvedDispatch();

   if (methodSymRef->getSymbol()->castToMethodSymbol()->isComputed())
      {
      buildVFTCall(site, CALLReg, site.evaluateVFT(), NULL);
      }
   else if (!evaluateVftEarly && site.resolvedVirtualShouldUseVFTCall())
      {
      // Call through VFT
      //
      TR::MemoryReference *dispatchMR;
      TR::Register *vftRegister = site.evaluateVFT();

      if (comp()->getGenerateReadOnlyCode())
         {
         /**
          * The particular addressing mode of the VPIC dispatch instruction is required by runtime
          * glue logic to distinguish a virtual dispatch produced in a read-only VPIC from other
          * cases.  In particular, see VM_JITInterface::jitVTableIndex() in runtime/oti/JITInterface.hpp
          */

         TR::Register *vtableOffsetReg = cg()->allocateRegister();
         generateRegImmInstruction(MOV8RegImm4, site.getCallNode(), vtableOffsetReg, methodSymRef->getOffset(), cg());

         // [ Rclass + RvtableOffset + 0x00000000 ]
         //
         dispatchMR = generateX86MemoryReference(vftRegister, vtableOffsetReg, 0, 0, cg());
         dispatchMR->setForceWideDisplacement();

         site.getPostConditionsUnderConstruction()->addPostCondition(vtableOffsetReg, TR::RealRegister::r8, cg());
         }
      else
         {
         // [ Rclass + disp32 ]
         //
         dispatchMR = generateX86MemoryReference(vftRegister, methodSymRef->getOffset(), cg());
         }

      buildVFTCall(site, CALLMem, NULL, dispatchMR);
      }
   else
      {
      TR::Register *vftRegister = site.evaluateVFT(); // We must evaluate the VFT here to avoid a later evaluation that pollutes the VPic shape.

      if (!comp()->getGenerateReadOnlyCode())
         {
         buildVPIC(site, entryLabel, doneLabel);
         }
      else
         {
         buildNoPatchingVirtualDispatchWithResolve(site, vftRegister, entryLabel, doneLabel);
         }
      }
   }

TR::Register *J9::X86::AMD64::PrivateLinkage::buildJNIDispatch(TR::Node *callNode)
   {
   TR_ASSERT(0, "AMD64 implements JNI dispatch using a system linkage");
   return NULL;
   }


TR::Instruction *J9::X86::AMD64::PrivateLinkage::buildUnresolvedOrInterpretedDirectCallReadOnly(TR::SymbolReference *methodSymRef, TR::X86CallSite &site)
   {
   TR::MethodSymbol *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();

   bool isJitInduceOSRCall = false;
   if (comp()->target().is64Bit() &&
       methodSymbol->isHelper() &&
       methodSymRef->isOSRInductionHelper())
      {
      TR_ASSERT_FATAL(false, "induceOSR not supported for read only");
      isJitInduceOSRCall = true;
      }

   TR::LabelSymbol *label = generateLabelSymbol(cg());

   TR::X86CallReadOnlySnippet *snippet = reinterpret_cast<TR::X86CallReadOnlySnippet *>(new (trHeapMemory()) TR::X86CallReadOnlySnippet(cg(), site.getCallNode(), label, false));

   cg()->addSnippet(snippet);
   snippet->gcMap().setGCRegisterMask(site.getPreservedRegisterMask());

   intptr_t staticSpecialDataAddress;
   intptr_t snippetOrCompiledMethodOffset;

   if (methodSymRef->isUnresolved() || (comp()->compileRelocatableCode() && !methodSymbol->isHelper()))
      {
      ccUnresolvedStaticSpecialData *ccUnresolvedStaticSpecialAddress =
         reinterpret_cast<ccUnresolvedStaticSpecialData *>(cg()->allocateCodeMemory(sizeof(ccUnresolvedStaticSpecialData), false));

      if (!ccUnresolvedStaticSpecialAddress)
         {
         comp()->failCompilation<TR::CompilationException>("Could not allocate unresolved static/special ccdata");
         }

      // Requires TR::ExternalRelocation
      ccUnresolvedStaticSpecialAddress->cpAddress = (intptr_t)methodSymRef->getOwningMethod(comp())->constantPool();
      ccUnresolvedStaticSpecialAddress->ramMethod = 0;

      snippet->setUnresolvedStaticSpecialDataAddress(reinterpret_cast<intptr_t>(ccUnresolvedStaticSpecialAddress));
      snippet->setRAMMethodDataAddress(reinterpret_cast<intptr_t>(&(ccUnresolvedStaticSpecialAddress->ramMethod)));
      snippetOrCompiledMethodOffset = offsetof(ccUnresolvedStaticSpecialData, snippetOrCompiledMethod);
      staticSpecialDataAddress = reinterpret_cast<intptr_t>(ccUnresolvedStaticSpecialAddress);
      }
   else
      {
      // Interpreted only
      //
      ccInterpretedStaticSpecialData *ccInterpretedStaticSpecialAddress =
         reinterpret_cast<ccInterpretedStaticSpecialData *>(cg()->allocateCodeMemory(sizeof(ccInterpretedStaticSpecialData), false));

      if (!ccInterpretedStaticSpecialAddress)
         {
         comp()->failCompilation<TR::CompilationException>("Could not allocate unresolved static/special ccdata");
         }

      // For jitInduceOSR we don't need to set the RAM method (the method that the VM needs to start executing)
      // because VM is going to figure what method to execute by looking up the the jitPC in the GC map and finding
      // the desired invoke bytecode.
      //
      intptr_t ramMethod;
      if (!isJitInduceOSRCall)
         {
#if defined(J9VM_OPT_JITSERVER)
         ramMethod = comp()->isOutOfProcessCompilation() && !methodSymbol->isInterpreted() ?
                             (intptr_t)methodSymRef->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod()->getPersistentIdentifier() :
                             (intptr_t)methodSymbol->getMethodAddress();
#else
         ramMethod = (intptr_t)methodSymbol->getMethodAddress();
#endif
         }
      else
         {
         ramMethod = 0;
         }

      ccInterpretedStaticSpecialAddress->ramMethod = ramMethod;

      if (comp()->getOption(TR_UseSymbolValidationManager))
         {
         TR_ASSERT_FATAL(0, "SVM not supported yet for read only");
/*
         cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                                       (uint8_t *)ramMethod,
                                                                                       (uint8_t *)TR::SymbolType::typeMethod,
                                                                                       TR_SymbolFromManager,
                                                                                       cg()),
                                     __FILE__, __LINE__, getNode());
*/
         }

      if (comp()->getOption(TR_EnableHCR))
         {
         TR_ASSERT_FATAL(0, "HCR not supported yet for read only");
/*
         cg()->jitAddPicToPatchOnClassRedefinition((void *)ramMethod, (void *)cursor);
*/
         }

      snippet->setInterpretedStaticSpecialDataAddress(reinterpret_cast<intptr_t>(ccInterpretedStaticSpecialAddress));
      snippet->setRAMMethodDataAddress(reinterpret_cast<intptr_t>(&(ccInterpretedStaticSpecialAddress->ramMethod)));
      snippetOrCompiledMethodOffset = offsetof(ccInterpretedStaticSpecialData, snippetOrCompiledMethod);
      staticSpecialDataAddress = reinterpret_cast<intptr_t>(ccInterpretedStaticSpecialAddress);
      }

   TR::StaticSymbol *snippetOrCompiledSlotSymbol =
      TR::StaticSymbol::createWithAddress(comp()->trHeapMemory(), TR::Address, reinterpret_cast<void *>(staticSpecialDataAddress + snippetOrCompiledMethodOffset));
   TR::SymbolReference *snippetOrCompiledSlotSymRef = new (comp()->trHeapMemory()) TR::SymbolReference(comp()->getSymRefTab(), snippetOrCompiledSlotSymbol, 0);

   // Local function call
   //
   // The AMD64CallReadOnlySnippet will initialize the snippetOrCompiledMethod field with the address
   // of the snippet when it is emitted.  Alternatively, use an AbsoluteLabelRelocation
   //
   TR::Instruction *slotCallInstruction = generateMemInstruction(CALLMem, site.getCallNode(),
      new (comp()->trHeapMemory()) TR::MemoryReference(snippetOrCompiledSlotSymRef, cg(), true),
      cg());

   return slotCallInstruction;
   }
#endif
