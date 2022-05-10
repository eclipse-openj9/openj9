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

#include "codegen/UnresolvedDataSnippet.hpp"
#include "codegen/UnresolvedDataSnippet_inlines.hpp"

#include <algorithm>
#include "codegen/CodeGenerator.hpp"
#include "codegen/Instruction.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/StaticSymbol.hpp"
#include "il/StaticSymbol_inlines.hpp"
#include "il/Symbol.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/J9Runtime.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "codegen/InstOpCode.hpp"

J9::X86::UnresolvedDataSnippet::UnresolvedDataSnippet(
      TR::CodeGenerator *cg,
      TR::Node *node,
      TR::SymbolReference *symRef,
      bool isStore,
      bool isGCSafePoint) :
   J9::UnresolvedDataSnippet(cg, node, symRef, isStore, isGCSafePoint),
      _numLiveX87Registers(0),
      _glueSymRef(NULL)
      {
      TR_ASSERT(symRef->getSymbol(), "symbol should not be NULL");

      if (symRef->getSymbol()->getDataType()==TR::Float ||
          symRef->getSymbol()->getDataType()==TR::Double)
         {
         setIsFloatData();
         }
      }


TR_RuntimeHelper
J9::X86::UnresolvedDataSnippet::getHelper()
   {
   if (getDataSymbol()->isShadow())
      return isUnresolvedStore() ? TR_X86interpreterUnresolvedFieldSetterGlue :
         TR_X86interpreterUnresolvedFieldGlue;

   if (getDataSymbol()->isClassObject())
      return getDataSymbol()->addressIsCPIndexOfStatic() ? TR_X86interpreterUnresolvedClassFromStaticFieldGlue :
         TR_X86interpreterUnresolvedClassGlue;

   if (getDataSymbol()->isConstString())
      return TR_X86interpreterUnresolvedStringGlue;

   if (getDataSymbol()->isConstMethodType())
      return TR_interpreterUnresolvedMethodTypeGlue;

   if (getDataSymbol()->isConstMethodHandle())
      return TR_interpreterUnresolvedMethodHandleGlue;

   if (getDataSymbol()->isCallSiteTableEntry())
      return TR_interpreterUnresolvedCallSiteTableEntryGlue;

   if (getDataSymbol()->isMethodTypeTableEntry())
      return TR_interpreterUnresolvedMethodTypeTableEntryGlue;

   if (getDataSymbol()->isConstantDynamic())
      return TR_X86interpreterUnresolvedConstantDynamicGlue;

   // Must be a static data reference

   return isUnresolvedStore() ? TR_X86interpreterUnresolvedStaticFieldSetterGlue :
                         TR_X86interpreterUnresolvedStaticFieldGlue;
   }


uint8_t *
J9::X86::UnresolvedDataSnippet::emitSnippetBody()
   {
   uint8_t *cursor = cg()->getBinaryBufferCursor();

   uint8_t *snippetStart  = cursor;
   getSnippetLabel()->setCodeLocation(snippetStart);
   TR::Instruction *stackMapInstr = getDataReferenceInstruction();

   if (!stackMapInstr)
      {
      return TR::InstOpCode(TR::InstOpCode::bad).binary(cursor, OMR::X86::Default);
      }

   _glueSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(getHelper());

   if (cg()->comp()->target().is64Bit())
      {
      cursor = emitResolveHelperCall(cursor);
      cursor = emitConstantPoolAddress(cursor);
      cursor = emitConstantPoolIndex(cursor);
      cursor = emitUnresolvedInstructionDescriptor(cursor);
      }
   else
      {
      cursor = emitConstantPoolIndex(cursor);
      cursor = emitConstantPoolAddress(cursor);
      cursor = emitResolveHelperCall(cursor);
      cursor = emitUnresolvedInstructionDescriptor(cursor);
      }

   cursor = fixupDataReferenceInstruction(cursor);

   // If there is a GC map for this snippet, register it at the point that this
   // snippet is called.
   //
   gcMap().registerStackMap(stackMapInstr, cg());

   return cursor;
   }



uint8_t *
J9::X86::UnresolvedDataSnippet::emitResolveHelperCall(uint8_t *cursor)
   {
   // Get the address for the glue routine (or its trampoline)
   //
   intptr_t glueAddress = (intptr_t)_glueSymRef->getMethodAddress();

   cg()->addProjectSpecializedRelocation(cursor+1, (uint8_t *)_glueSymRef, NULL, TR_HelperAddress,
   __FILE__, __LINE__, getNode());

   // Call to the glue routine
   //
   const intptr_t rip = (intptr_t)(cursor+5);
   if ((cg()->needRelocationsForHelpers() && cg()->comp()->target().is64Bit()) ||
       NEEDS_TRAMPOLINE(glueAddress, rip, cg()))
      {
      TR_ASSERT(cg()->comp()->target().is64Bit(), "should not require a trampoline on 32-bit");
      glueAddress = TR::CodeCacheManager::instance()->findHelperTrampoline(_glueSymRef->getReferenceNumber(), (void *)cursor);
      TR_ASSERT(IS_32BIT_RIP(glueAddress, rip), "Local helper trampoline should be reachable directly.\n");
      }

   *cursor++ = 0xe8;    // TR::InstOpCode::CALLImm4

   int32_t offset = (int32_t)((intptr_t)glueAddress - rip);
   *(int32_t *)cursor = offset;
   cursor += 4;

   TR_ASSERT((intptr_t)cursor == rip, "assertion failure");

   return cursor;
   }


uint32_t
J9::X86::UnresolvedDataSnippet::getUnresolvedStaticStoreDeltaWithMemBarrier()
   {
   // If the this is not a store operation on an unresolved static field, return.
   //
   if (getDataSymbol()->isClassObject() || getDataSymbol()->isConstObjectRef())
      {
      return 0;
      }

   // Find the delta between the start of the memory barrier that should be guarding the store to the static field, and the start of the store instruction
   //
   TR::Instruction *instIter = getDataReferenceInstruction();
   uint8_t *dataRefInstOffset = getDataReferenceInstruction()->getBinaryEncoding();

   instIter = instIter->getNext();
   uint8_t delta = instIter->getBinaryEncoding() - dataRefInstOffset;

   if (cg()->comp()->getOption(TR_X86UseMFENCE))
      {
      while ( instIter->getOpCode().getOpCodeValue() != TR::InstOpCode::MFENCE && delta <= 20)
         {
         instIter = instIter->getNext();
         delta = instIter->getBinaryEncoding() - dataRefInstOffset;
         }

      // Return the appropriate offset flag.
      //
      if (delta == 20 && instIter->getOpCode().getOpCodeValue() == TR::InstOpCode::MFENCE)
         {
         return cpIndex_extremeStaticMemBarPos;
         }
      else if (delta == 16 && instIter->getOpCode().getOpCodeValue() == TR::InstOpCode::MFENCE)
         {
         return cpIndex_genStaticMemBarPos;
         }
      else
         {
         TR_ASSERT(0, "Invalid computed offset between start of store to static field and start of memory barrier: %d. Should be 16 or 20.", delta);
         return 0;
         }
      }
   else
      {
      while ( instIter->getOpCode().getOpCodeValue() != TR::InstOpCode::LOR4MemImms && delta <= 24)
         {
         instIter = instIter->getNext();
         delta = instIter->getBinaryEncoding() - dataRefInstOffset;
         }

      // Return the appropriate offset flag.
      //
      if (delta == 24 && instIter->getOpCode().getOpCodeValue() == TR::InstOpCode::LOR4MemImms)
         {
         return cpIndex_extremeStaticMemBarPos;
         }
      else if (delta == 16 && instIter->getOpCode().getOpCodeValue() == TR::InstOpCode::LOR4MemImms)
         {
         return cpIndex_genStaticMemBarPos;
         }
      else
         {
         TR_ASSERT(0, "Invalid computed offset between start of store to static field and start of memory barrier: %d. Should be 16 or 24.", delta);
         return 0;
         }
      }
   }


uint8_t *
J9::X86::UnresolvedDataSnippet::emitUnresolvedInstructionDescriptor(uint8_t *cursor)
   {
   // On 64-bit a descriptor byte is only required for unresolved fields because
   // the 4-byte disp32 field
   //
   if (cg()->comp()->target().is64Bit() && !getDataSymbol()->isShadow())
      return cursor;

   // Descriptor
   // Bits:
   //    7-4: template instruction length
   //    3-0: offset of data reference from start of instruction
   //
   uint8_t       *instructionStart    = getDataReferenceInstruction()->getBinaryEncoding();
   const uint8_t  instructionLength   = getDataReferenceInstruction()->getBinaryLength();
   const size_t   dataReferenceOffset = getAddressOfDataReference() - instructionStart;

   TR_ASSERT(instructionLength     != 0,    "getDataReferenceInstruction()->getBinaryLength() should not be zero");

   const int32_t maxLen =  15;
   TR_ASSERT(instructionLength     <= maxLen,   "Instruction length (%d) must fit in descriptor", instructionLength);
   TR_ASSERT(dataReferenceOffset   <= maxLen,   "Data reference offset (%d) must fit in descriptor", dataReferenceOffset);

   *cursor++ = (uint8_t)((instructionLength << 4) | dataReferenceOffset);

   return cursor;
   }


uint8_t *
J9::X86::UnresolvedDataSnippet::emitConstantPoolAddress(uint8_t *cursor)
   {
   TR::Compilation *comp = cg()->comp();
   if (comp->target().is32Bit())
      {
      *cursor++ = 0x68;  // push imm4
      }

   *(intptr_t *)cursor = (intptr_t)getDataSymbolReference()->getOwningMethod(comp)->constantPool();
   // in MT JRE, it might have no data reference instruction
   if (getDataReferenceInstruction())
      {
      TR::Node *node = getDataReferenceInstruction()->getNode();
      if (node && ((node->getOpCodeValue() == TR::checkcastAndNULLCHK) || (node->getOpCodeValue() == TR::NULLCHK)))
         {
         node = node->getChild(0);
         }

      cg()->addProjectSpecializedRelocation(cursor,(uint8_t *)getDataSymbolReference()->getOwningMethod(comp)->constantPool(),
            node ? (uint8_t *)(uintptr_t)node->getInlinedSiteIndex() : (uint8_t *)-1, TR_ConstantPool,
                             __FILE__, __LINE__, node);
      }

   cursor += TR::Compiler->om.sizeofReferenceAddress();
   return cursor;
   }


uint8_t *
J9::X86::UnresolvedDataSnippet::emitConstantPoolIndex(uint8_t *cursor)
   {

   // Anatomy of a cpIndex passed to the runtime:
   //
   //  byte 3   byte 2   byte 1   byte 0
   //
   // 3 222  2 222    1 1      0 0      0
   // 10987654 32109876 54321098 76543210
   //  ||||__| |||    |_________________|
   //  ||| |   |||             |
   //  ||| |   |||             +---------------- cpIndex (0-16)
   // |||| |   ||| ||+-------------------------- upper long dword resolution (17)
   // |||| |   ||| |+--------------------------- lower long dword resolution (18)
   // |||| |   ||| +---------------------------- check volatility (19)
   // |||| |   ||+------------------------------ resolve, but do not patch snippet template (21)
   // |||| |   |+------------------------------- resolve, but do not patch mainline code (22)
   // |||| |   +-------------------------------- long push instruction (23)
   // |||| +------------------------------------ number of live X87 registers across this resolution (24-27)
   // |||+-------------------------------------- has live XMM registers (28)
   // ||+--------------------------------------- static resolution (29)
   // |+---------------------------------------- 64-bit: resolved value is 8 bytes (30)
   // |                                          32-bit: resolved value is high word of long pair (30)
   // +----------------------------------------- 64 bit: extreme static memory barrier position (31)

   TR::Compilation *comp = cg()->comp();
   TR::Symbol *dataSymbol = getDataSymbolReference()->getSymbol();
   int32_t cpIndex;
   if (dataSymbol->isCallSiteTableEntry())
      {
      cpIndex = dataSymbol->castToCallSiteTableEntrySymbol()->getCallSiteIndex();
      }
   else if (dataSymbol->isMethodTypeTableEntry())
      {
      cpIndex = dataSymbol->castToMethodTypeTableEntrySymbol()->getMethodTypeIndex();
      }
   else // constant pool index
      {
      cpIndex = getDataSymbolReference()->getCPIndex();
      }

   if (getNumLiveX87Registers() > 0) cpIndex |= (getNumLiveX87Registers() << 24);
   if (hasLiveXMMRegisters() && comp->target().is32Bit()) cpIndex |= cpIndex_hasLiveXMMRegisters;

   // For unresolved object refs, the snippet need not be patched because its
   // data is already correct.
   //
   if (getDataSymbol()->isConstObjectRef()) cpIndex |= cpIndex_doNotPatchSnippet;

   // Identify static resolution.
   //
   if (!getDataSymbol()->isShadow() && !getDataSymbol()->isClassObject() && !getDataSymbol()->isConstObjectRef())
      {
      cpIndex |= cpIndex_isStaticResolution;
      }

   if (resolveMustPatch8Bytes())
      {
      TR_ASSERT(comp->target().is64Bit(), "resolveMustPatch8Bytes must only be set on 64-bit");
      cpIndex |= cpIndex_patch8ByteResolution;
      }

   if (comp->target().is32Bit())
      {
      if (getDataSymbolReference()->getOffset() == 4)
         {
         TR_ASSERT(((!getDataSymbol()->isShadow() && !getDataSymbol()->isClassObject() && !getDataSymbol()->isConstObjectRef()) || getDataSymbol()->isShadow()),
                 "unexpected symbol kind for offset 4, instruction=%x\n", getDataReferenceInstruction());

         // On 32-bit, an unresolved reference with a 4-byte offset is the high word
         // of an unresolved long.  The offset will be added at runtime.
         //
         cpIndex |= cpIndex_isHighWordOfLongPair;
         }
      }

   if (!comp->getOption(TR_DisableNewX86VolatileSupport) && getDataReferenceInstruction() != NULL)
      {
      TR::Instruction *dataRefInst = getDataReferenceInstruction();
      TR::InstOpCode::Mnemonic opCode = dataRefInst->getOpCode().getOpCodeValue();

      // If this is a store operation on an unresolved field, set the volatility check flag.
      //
      if (comp->target().isSMP())
         {
         if (!getDataSymbol()->isClassObject() && !getDataSymbol()->isConstObjectRef() &&  isUnresolvedStore() && opCode != TR::InstOpCode::CMPXCHG8BMem && getDataSymbol()->isVolatile())
            {
            cpIndex |= cpIndex_checkVolatility;

            if (comp->target().is64Bit() && ((TR::X86MemInstruction *)dataRefInst)->getMemoryReference())
               {
               if (((TR::X86MemInstruction *)dataRefInst)->getMemoryReference()->processAsFPVolatile())
                  {
                  cpIndex |= cpIndex_isFloatStore;
                  }
               }

            if (comp->target().is64Bit() && !getDataSymbol()->isShadow())
               {
               //If this is a store into a static field, we need to know the offset between the start of the store instruction and the memory barrier.
               //
               cpIndex |= getUnresolvedStaticStoreDeltaWithMemBarrier();
               }
            }

         if (comp->target().is32Bit())
            {
            // If this is a portion of a long store, flag which half of the long
            // is being stored
            //
            if (dataRefInst->getKind() == TR::Instruction::IsRegMem)
               {
               if (((TR::X86RegMemInstruction *)dataRefInst)->getMemoryReference()->processAsLongVolatileLow())
                  {
                  cpIndex |= cpIndex_procAsLongLow | cpIndex_checkVolatility;
                  }
               else if (((TR::X86RegMemInstruction *)dataRefInst)->getMemoryReference()->processAsLongVolatileHigh())
                  {
                  cpIndex |= cpIndex_procAsLongHigh | cpIndex_checkVolatility;
                  }
               }
            }
         }
      }

   if (comp->target().is32Bit())
      {
      if (cpIndex <= 127 && cpIndex >= -128)
         {
         *cursor++ = 0x6a; // push   imms
         *(int8_t *)cursor++ = (int8_t)cpIndex;
         }
      else
         {
         *cursor++ = 0x68; // push   imm4
         *(int32_t *)cursor = (cpIndex | cpIndex_longPushTag);
         cursor += 4;
         }
      }
   else
      {
      *(int32_t *)cursor = cpIndex;
      cursor += 4;
      }
   return cursor;
   }


uint8_t *
J9::X86::UnresolvedDataSnippet::fixupDataReferenceInstruction(uint8_t *cursor)
   {
   uint8_t *instructionStart = getDataReferenceInstruction()->getBinaryEncoding();
   uint8_t bytesToCopy;
   TR::Compilation *comp = cg()->comp();

   // Only for unresolved statics whose reference instruction may be executed in place
   // in the snippet should the entire reference instruction be copied.  All other
   // unresolved references should copy 8 bytes only (this may include bytes from
   // the following instruction).
   //
   if (!getDataSymbol()->isShadow() && !getDataSymbol()->isClassObject() && !getDataSymbol()->isConstObjectRef())
      {
      // Unresolved statics may result in the instruction being patched and executed in the snippet
      // rather than the mainline code.  A return instruction is necessary in this case.
      //
      uint8_t length = getDataReferenceInstruction()->getBinaryLength();

      bytesToCopy = std::max<uint8_t>(8, length);
      memcpy(cursor, instructionStart, bytesToCopy);

      // If the TR::InstOpCode::RET will overwrite one of the eight bytes that will eventually be patched
      // then the byte it overwrites needs to be preserved.
      //
      if (length < 8)
         {
         uint8_t b = *(cursor+length);
         *(cursor+length) = 0xc3;  // TR::InstOpCode::RET
         cursor += bytesToCopy;
         *cursor++ = b;
         }
      else
         {
         *(cursor+length) = 0xc3;  // TR::InstOpCode::RET
         cursor += (bytesToCopy + 1);
         }
      }
   else
      {
      if (comp->target().is64Bit())
         {
         // Unresolved instance fields require 4 byte patching at a variable instruction
         // offset so the 8 bytes following the start of the instruction need to be
         // cached.
         //
         // For unresolved strings and classes only 2 bytes (REX+Op) are necessary.
         //
         bytesToCopy = getDataSymbol()->isShadow() ? 8 : 2;
         }
      else
         {
         if (getDataSymbol()->isConstObjectRef())
            {
            bytesToCopy = std::max<uint8_t>(8, getDataReferenceInstruction()->getBinaryLength());
            }
         else
            {
            bytesToCopy = 8;
            }
         }

      memcpy(cursor, instructionStart, bytesToCopy);
      cursor += bytesToCopy;

      // For object ref constants, the original instruction already loaded a valid constant pool
      // address and since it was copied to the snippet it needs a relocation here.
      //
      // This assumes that the string reference instruction is a MOV Reg, Imm32 (32-bit)
      // or a MOV Reg, Imm64 (64-bit) instruction.
      //
      if (comp->target().is32Bit() && getDataSymbol()->isConstObjectRef())
         {
         uint8_t *stringConstantPtr = (cursor - bytesToCopy) + getDataReferenceInstruction()->getBinaryLength() - TR::Compiler->om.sizeofReferenceAddress();

         cg()->addProjectSpecializedRelocation(stringConstantPtr, (uint8_t *)getDataSymbolReference()->getOwningMethod(comp)->constantPool(),
               getDataReferenceInstruction()->getNode() ? (uint8_t *)(uintptr_t)getDataReferenceInstruction()->getNode()->getInlinedSiteIndex() : (uint8_t *)-1, TR_ConstantPool,
                    __FILE__, __LINE__, getDataReferenceInstruction()->getNode());
         }
      }

   // Write a call to this snippet over the original data reference instruction
   //
   uint8_t *originalInstruction = getDataReferenceInstruction()->getBinaryEncoding();
   *originalInstruction = 0xe8;     // CallImm32
   *(int32_t *)(originalInstruction+1) = (int32_t)(cg()->getBinaryBufferCursor() - 5 - originalInstruction);

   return cursor;
   }


uint32_t
J9::X86::UnresolvedDataSnippet::getLength(int32_t estimatedSnippetStart)
   {
   uint32_t length;

   length = 5;  // TR::InstOpCode::CALLImm4 to resolve helper

   // cpAddr
   //
   length += sizeof(intptr_t);

   // cpIndex
   //
   // This is a conservative approximation.
   //
   length += 4;

   // 32-bit adjustments
   //
   if (cg()->comp()->target().is32Bit())
      {
      length += (
                  1  // TR::InstOpCode::PUSHImm4 for cpAddr
                 +1  // TR::InstOpCode::PUSHImm4 for cpIndex
                 +1  // descriptor byte
                );
      }

   if (!getDataSymbol()->isShadow() && !getDataSymbol()->isClassObject() && !getDataSymbol()->isConstObjectRef())
      {
      // Static reference.
      //
      uint32_t instructionLength = getDataReferenceInstruction()->getBinaryLength();

      // +1 is for either a TR::InstOpCode::RET instruction if instructionLength > 7 or for a copy of
      // the byte that the TR::InstOpCode::RET instruction overwrites if instructionLength < 8.
      //
      length += std::max<uint32_t>(8, instructionLength) + 1;
      }
   else
      {
      if (cg()->comp()->target().is32Bit())
         {
         if (getDataSymbol()->isConstObjectRef())
            {
            uint32_t instructionLength = getDataReferenceInstruction()->getBinaryLength();
            length += std::max<uint32_t>(8, instructionLength);
            }
          else
            {
            length += 8;
            }
         }
      else
         {
         if (getDataSymbol()->isShadow())
            {
            length += (
                        8  // cached instruction bytes
                       +1  // descriptor byte
                      );
            }
         else
            {
            length += 2;  // REX+opcode
            }
         }
      }

   return length;
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::UnresolvedDataSnippet * snippet)
   {
   if (pOutFile == NULL)
      return;

   uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();
   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet));
   trfprintf(pOutFile, " for instr [%s]", getName(snippet->getDataReferenceInstruction()));

   if (_comp->target().is64Bit())
      {
      printPrefix(pOutFile, NULL, bufferPos, 5);

      TR_RuntimeHelper helperIndex = snippet->getHelper();
      TR::SymbolReference *glueSymRef = _cg->getSymRef(helperIndex);
      trfprintf(pOutFile, "call\t%s", getName(glueSymRef));
      bufferPos += 5;

      printPrefix(pOutFile, NULL, bufferPos, 8);
      trfprintf(pOutFile,
                    "%s\t" POINTER_PRINTF_FORMAT "\t%s address of constant pool for this method",
                    dqString(),
                    getOwningMethod(snippet->getDataSymbolReference())->constantPool(),
                    commentString());
      bufferPos += 8;

      printPrefix(pOutFile, NULL, bufferPos, 4);
      trfprintf(pOutFile,
                   "%s\t0x%08x\t\t\t\t%s constant pool index",
                    ddString(),
                    snippet->getDataSymbolReference()->getCPIndex(),
                    commentString());
      bufferPos += 4;

      if (snippet->getDataSymbol()->isShadow())
         {
         printPrefix(pOutFile, NULL, bufferPos, 1);

         uint8_t descriptor = *bufferPos;
         uint8_t length = (descriptor >> 4) & 0xf;
         uint8_t offset = (descriptor & 0xf);

         trfprintf(
            pOutFile,
            "%s\t%02x\t\t\t\t\t\t\t%s instruction descriptor: length=%d, disp32 offset=%d",
            dbString(),
            descriptor,
            commentString(),
            length,
            offset
            );

         bufferPos++;
         }

      }
   else
      {
      if (!snippet->getDataReferenceInstruction())
         {
         // when do not patch mainline flag on, there is no data reference instruction copy in snippet
         printPrefix(pOutFile, NULL, bufferPos, 1);
         trfprintf(pOutFile, "int \t3\t\t\t%s (No data reference instruction; NEVER CALLED)",
                       commentString());
         return;
         }

      // 0x68 == PUSHImm32
      //
      int32_t size = (*bufferPos == 0x68) ? 5 : 2;
      printPrefix(pOutFile, NULL, bufferPos, size);
      trfprintf(
         pOutFile,
         "push\t" POINTER_PRINTF_FORMAT "\t\t%s constant pool index",
         snippet->getDataSymbolReference()->getCPIndex(),
         commentString());
      bufferPos += size;

      printPrefix(pOutFile, NULL, bufferPos, 5);
      trfprintf(
         pOutFile,
         "push\t" POINTER_PRINTF_FORMAT "\t\t%s address of constant pool for this method",
         getOwningMethod(snippet->getDataSymbolReference())->constantPool(),
         commentString());
      bufferPos += 5;

      printPrefix(pOutFile, NULL, bufferPos, 5);
      TR::SymbolReference *glueSymRef = _cg->getSymRef(snippet->getHelper());
      trfprintf(pOutFile, "call\t%s", getName(glueSymRef));
      bufferPos += 5;
      }

   if (!snippet->getDataSymbol()->isShadow() &&
       !snippet->getDataSymbol()->isClassObject() &&
       !snippet->getDataSymbol()->isConstObjectRef() &&
       !snippet->getDataSymbol()->isMethodTypeTableEntry())
      {
      int32_t length = snippet->getDataReferenceInstruction()->getBinaryLength();
      int32_t bytesToCopy = std::max<int32_t>(8, length);

      if (length < 8)
         {
         printPrefix(pOutFile, NULL, bufferPos, bytesToCopy);
         bufferPos += bytesToCopy;
         trfprintf(pOutFile, "%s\t(%d)\t\t\t%s patch instruction bytes + TR::InstOpCode::RET + residue",
                       dbString(),
                       bytesToCopy,
                       commentString());
         printPrefix(pOutFile, NULL, bufferPos, 1);
         trfprintf(pOutFile, "%s\t\t\t\t\t\t%s byte that TR::InstOpCode::RET overwrote",
                       dbString(),
                       commentString());
         bufferPos ++;
         }
      else
         {
         printPrefix(pOutFile, NULL, bufferPos, bytesToCopy+1);
         bufferPos += (bytesToCopy + 1);
         trfprintf(pOutFile, "%s\t(%d)\t\t\t\t%s patch instruction bytes + TR::InstOpCode::RET",
                       dbString(),
                       (bytesToCopy+1),
                       commentString());
         }
      }
   else
      {
      int32_t bytesToCopy;

      if (_comp->target().is64Bit())
         {
         if (snippet->getDataSymbol()->isShadow())
            {
            bytesToCopy = 8;
            printPrefix(pOutFile, NULL, bufferPos, bytesToCopy);
            bufferPos += bytesToCopy;
            trfprintf(pOutFile, "%s\t(%d)\t\t\t\t\t\t%s patch instruction bytes",
                          dbString(),
                          bytesToCopy,
                          commentString());
            }
         else
            {
            bytesToCopy = 2;
            printPrefix(pOutFile, NULL, bufferPos, bytesToCopy);
            bufferPos += bytesToCopy;
            trfprintf(pOutFile, "%s\t\t\t\t\t\t\t\t%s REX + op of TR::InstOpCode::MOV8RegImm64",
                          dwString(),
                          commentString());
            }
         }
      else
         {
         if (snippet->getDataSymbol()->isConstString())
            {
            // TODO:JSR292: isConstMethodType
            // TODO:JSR292: isConstMethodHandle
            bytesToCopy = std::max<int32_t>(8, snippet->getDataReferenceInstruction()->getBinaryLength());
            printPrefix(pOutFile, NULL, bufferPos, bytesToCopy);
            bufferPos += bytesToCopy;
            trfprintf(pOutFile, "%s\t(%d)\t\t\t\t\t\t%s patched string instruction bytes",
                          dbString(),
                          bytesToCopy,
                          commentString());
            }
         else
            {
            bytesToCopy = 8;
            printPrefix(pOutFile, NULL, bufferPos, bytesToCopy);
            bufferPos += bytesToCopy;
            trfprintf(pOutFile, "%s\t(%d)\t\t\t\t\t\t%s patch instruction bytes",
                          dbString(),
                          bytesToCopy,
                          commentString());
            }
         }
      }
   }
