/*******************************************************************************
 * Copyright (c) 2000, 2021 IBM Corp. and others
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

#include "codegen/IA32PrivateLinkage.hpp"

#include "codegen/CodeGenerator.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/Snippet.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/CHTable.hpp"
#include "env/jittypes.h"
#include "env/CompilerEnv.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "env/VMJ9.h"
#include "x/codegen/CallSnippet.hpp"
#include "x/codegen/CheckFailureSnippet.hpp"
#include "x/codegen/FPTreeEvaluator.hpp"
#include "x/codegen/HelperCallSnippet.hpp"
#include "x/codegen/IA32LinkageUtils.hpp"
#include "x/codegen/X86Instruction.hpp"

J9::X86::I386::PrivateLinkage::PrivateLinkage(TR::CodeGenerator *cg)
   : J9::X86::PrivateLinkage(cg)
   {
   _properties._properties = 0;
   _properties._registerFlags[TR::RealRegister::NoReg] = 0;
   _properties._registerFlags[TR::RealRegister::eax]   = IntegerReturn;
   _properties._registerFlags[TR::RealRegister::ebx]   = Preserved;
   _properties._registerFlags[TR::RealRegister::ecx]   = Preserved;
   _properties._registerFlags[TR::RealRegister::edx]   = IntegerReturn;
   _properties._registerFlags[TR::RealRegister::edi]   = 0;
   _properties._registerFlags[TR::RealRegister::esi]   = Preserved;
   _properties._registerFlags[TR::RealRegister::ebp]   = Preserved;
   _properties._registerFlags[TR::RealRegister::esp]   = Preserved;

   for (int i = 0; i <= 7; i++)
      {
      _properties._registerFlags[TR::RealRegister::xmmIndex(i)] = 0;
      }

   _properties._registerFlags[TR::RealRegister::xmm0] = FloatReturn;

   _properties._preservedRegisters[0] = TR::RealRegister::ebx;
   _properties._preservedRegisters[1] = TR::RealRegister::ecx;
   _properties._preservedRegisters[2] = TR::RealRegister::esi;
   _properties._maxRegistersPreservedInPrologue = 3;
   _properties._preservedRegisters[3] = TR::RealRegister::ebp;
   _properties._preservedRegisters[4] = TR::RealRegister::esp;
   _properties._numPreservedRegisters = 5;

   _properties._argumentRegisters[0] = TR::RealRegister::NoReg;

   _properties._numIntegerArgumentRegisters  = 0;
   _properties._firstIntegerArgumentRegister = 0;
   _properties._numFloatArgumentRegisters    = 0;
   _properties._firstFloatArgumentRegister   = 0;

   _properties._returnRegisters[0] = TR::RealRegister::eax;
   _properties._returnRegisters[1] = TR::RealRegister::xmm0;
   _properties._returnRegisters[2] = TR::RealRegister::edx;

   _properties._scratchRegisters[0] = TR::RealRegister::edi;
   _properties._scratchRegisters[1] = TR::RealRegister::edx;
   _properties._numScratchRegisters = 2;

   _properties._preservedRegisterMapForGC = // TODO:AMD64: Use the proper mask value
      (  (1 << (TR::RealRegister::ebx-1)) |    // Preserved register map for GC
         (1 << (TR::RealRegister::ecx-1)) |
         (1 << (TR::RealRegister::esi-1)) |
         (1 << (TR::RealRegister::ebp-1)) |
         (1 << (TR::RealRegister::esp-1))),

   _properties._vtableIndexArgumentRegister = TR::RealRegister::edx;
   _properties._j9methodArgumentRegister = TR::RealRegister::edi;
   _properties._framePointerRegister = TR::RealRegister::ebx;
   _properties._methodMetaDataRegister = TR::RealRegister::ebp;

   _properties._numberOfVolatileGPRegisters  = 3;
   _properties._numberOfVolatileXMMRegisters = 8; // xmm0-xmm7
   setOffsetToFirstParm(4);
   _properties._offsetToFirstLocal = 0;


   // 173135: If we are too eager picking the byte regs, we won't have any
   // available for byte operations in the code, and we'll need register
   // shuffles.  Normally, shuffles are no big deal, but if they occur right
   // before byte operations, we can end up with partial register stalls which
   // are as bad as spills.
   //
   // Most byte operations in Java come from byte array element operations.
   // Array indexing expressions practically never need byte registers, so
   // while favouring non-byte regs for GRA could hurt byte candidates from
   // array elements, it should practically never hurt indexing expressions.
   // We expect array indexing expressions to need global regs far more often
   // than array elements, so favouring non-byte registers makes sense, all
   // else being equal.
   //
   // Ideally, pickRegister would detect the use of byte expressions and do
   // the right thing.

   // volatiles, non-byte-reg first
   _properties._allocationOrder[0] = TR::RealRegister::edi;
   _properties._allocationOrder[1] = TR::RealRegister::edx;
   _properties._allocationOrder[2] = TR::RealRegister::eax;
   // preserved, non-byte-reg first
   _properties._allocationOrder[3] = TR::RealRegister::esi;
   _properties._allocationOrder[4] = TR::RealRegister::ebx;
   _properties._allocationOrder[5] = TR::RealRegister::ecx;
   _properties._allocationOrder[6] = TR::RealRegister::ebp;
   // floating point
   _properties._allocationOrder[7] = TR::RealRegister::st0;
   _properties._allocationOrder[8] = TR::RealRegister::st1;
   _properties._allocationOrder[9] = TR::RealRegister::st2;
   _properties._allocationOrder[10] = TR::RealRegister::st3;
   _properties._allocationOrder[11] = TR::RealRegister::st4;
   _properties._allocationOrder[12] = TR::RealRegister::st5;
   _properties._allocationOrder[13] = TR::RealRegister::st6;
   _properties._allocationOrder[14] = TR::RealRegister::st7;
   }

TR::Instruction *J9::X86::I386::PrivateLinkage::savePreservedRegisters(TR::Instruction *cursor)
   {
   TR::ResolvedMethodSymbol *bodySymbol  = comp()->getJittedMethodSymbol();
   const int32_t          localSize   = _properties.getOffsetToFirstLocal() - bodySymbol->getLocalMappingCursor();
   const int32_t          pointerSize = _properties.getPointerSize();

   int32_t offsetCursor = -localSize - _properties.getPointerSize();
   int32_t numPreserved = getProperties().getMaxRegistersPreservedInPrologue();

   for (int32_t pindex = numPreserved-1;
         pindex >= 0;
         pindex--)
      {
      TR::RealRegister::RegNum idx = _properties.getPreservedRegister((uint32_t)pindex);
      TR::RealRegister *reg = machine()->getRealRegister(idx);
      if (reg->getHasBeenAssignedInMethod() && reg->getState() != TR::RealRegister::Locked)
         {
         cursor = generateMemRegInstruction(
                     cursor,
                     TR::InstOpCode::S4MemReg,
                     generateX86MemoryReference(machine()->getRealRegister(TR::RealRegister::vfp), offsetCursor, cg()),
                     reg,
                     cg()
                     );
         offsetCursor -= pointerSize;
         }
      }
   return cursor;
   }

TR::Instruction *J9::X86::I386::PrivateLinkage::restorePreservedRegisters(TR::Instruction *cursor)
   {
   TR::ResolvedMethodSymbol *bodySymbol  = comp()->getJittedMethodSymbol();
   const int32_t          localSize   = _properties.getOffsetToFirstLocal() - bodySymbol->getLocalMappingCursor();
   const int32_t          pointerSize = _properties.getPointerSize();

   int32_t offsetCursor = -localSize - _properties.getPointerSize();
   int32_t numPreserved = getProperties().getMaxRegistersPreservedInPrologue();
   for (int32_t pindex = numPreserved-1;
        pindex >= 0;
        pindex--)
      {
      TR::RealRegister::RegNum idx = _properties.getPreservedRegister((uint32_t)pindex);
      TR::RealRegister *reg = machine()->getRealRegister(idx);
      if (reg->getHasBeenAssignedInMethod())
         {
         cursor = generateRegMemInstruction(
                     cursor,
                     TR::InstOpCode::L4RegMem,
                     reg,
                     generateX86MemoryReference(machine()->getRealRegister(TR::RealRegister::vfp), offsetCursor, cg()),
                     cg()
                     );
         offsetCursor -= pointerSize;
         }
      }
   return cursor;
   }


int32_t J9::X86::I386::PrivateLinkage::buildArgs(
      TR::Node *callNode,
      TR::RegisterDependencyConditions *dependencies)
   {
   int32_t      argSize            = 0;
   TR::Register *eaxRegister        = NULL;
   TR::Node     *thisChild          = NULL;
   int32_t      firstArgumentChild = callNode->getFirstArgumentIndex();
   int32_t      linkageRegChildIndex = -1;

   int32_t receiverChildIndex = -1;
   if (callNode->getSymbol()->castToMethodSymbol()->firstArgumentIsReceiver() && callNode->getOpCode().isIndirect())
      receiverChildIndex = firstArgumentChild;

   if (!callNode->getSymbolReference()->isUnresolved())
      switch(callNode->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod())
         {
         case TR::java_lang_invoke_ComputedCalls_dispatchJ9Method:
         case TR::java_lang_invoke_ComputedCalls_dispatchVirtual:
         case TR::com_ibm_jit_JITHelpers_dispatchVirtual:
            linkageRegChildIndex = firstArgumentChild;
            receiverChildIndex = callNode->getOpCode().isIndirect()? firstArgumentChild+1 : -1;
         }

   for (int i = firstArgumentChild; i < callNode->getNumChildren(); i++)
      {
      TR::Node *child = callNode->getChild(i);
      switch (child->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Address:
            if (i == receiverChildIndex)
               {
               eaxRegister = pushThis(child);
               thisChild   = child;
               }
            else
               {
               pushIntegerWordArg(child);
               }
            argSize += 4;
            break;
         case TR::Int64:
            {
            TR::Register *reg = NULL;
            if (i == linkageRegChildIndex)
               {
               // TODO:JSR292: This should really be in the front-end
               TR::MethodSymbol *sym = callNode->getSymbol()->castToMethodSymbol();
               switch (sym->getMandatoryRecognizedMethod())
                  {
                  case TR::java_lang_invoke_ComputedCalls_dispatchJ9Method:
                     reg = cg()->evaluate(child);
                     if (reg->getRegisterPair())
                        reg = reg->getRegisterPair()->getLowOrder();
                     dependencies->addPreCondition(reg, getProperties().getJ9MethodArgumentRegister(), cg());
                     cg()->decReferenceCount(child);
                     break;
                  case TR::java_lang_invoke_ComputedCalls_dispatchVirtual:
                  case TR::com_ibm_jit_JITHelpers_dispatchVirtual:
                     reg = cg()->evaluate(child);
                     if (reg->getRegisterPair())
                        reg = reg->getRegisterPair()->getLowOrder();
                     dependencies->addPreCondition(reg, getProperties().getVTableIndexArgumentRegister(), cg());
                     cg()->decReferenceCount(child);
                     break;
                  }
               }
            if (!reg)
               {
               TR::IA32LinkageUtils::pushLongArg(child, cg());
               argSize += 8;
               }
            break;
            }
         case TR::Float:
            TR::IA32LinkageUtils::pushFloatArg(child, cg());
            argSize += 4;
            break;
         case TR::Double:
            TR::IA32LinkageUtils::pushDoubleArg(child, cg());
            argSize += 8;
            break;
         }
      }

   if (thisChild)
      {
      TR::Register *rcvrReg = cg()->evaluate(thisChild);

      if (thisChild->getReferenceCount() > 1)
         {
         eaxRegister = cg()->allocateCollectedReferenceRegister();
         generateRegRegInstruction(TR::InstOpCode::MOV4RegReg, thisChild, eaxRegister, rcvrReg, cg());
         }
      else
         {
         eaxRegister = rcvrReg;
         }

      dependencies->addPreCondition(eaxRegister, TR::RealRegister::eax, cg());
      cg()->stopUsingRegister(eaxRegister);
      cg()->decReferenceCount(thisChild);
      }

   return argSize;

   }


TR::UnresolvedDataSnippet *J9::X86::I386::PrivateLinkage::generateX86UnresolvedDataSnippetWithCPIndex(
      TR::Node *child,
      TR::SymbolReference *symRef,
      int32_t cpIndex)
   {
   // We know the symbol must have already been resolved, so no GC can
   // happen (unless the gcOnResolve option is being used).
   //
   TR::UnresolvedDataSnippet *snippet = TR::UnresolvedDataSnippet::create(cg(), child, symRef, false, (debug("gcOnResolve") != NULL));
   cg()->addSnippet(snippet);

   TR::Instruction *dataReferenceInstruction = generateImmSnippetInstruction(TR::InstOpCode::PUSHImm4, child, cpIndex, snippet, cg());
   snippet->setDataReferenceInstruction(dataReferenceInstruction);
   generateBoundaryAvoidanceInstruction(TR::X86BoundaryAvoidanceInstruction::unresolvedAtomicRegions, 8, 8, dataReferenceInstruction, cg());

   return snippet;
   }

TR::Register *J9::X86::I386::PrivateLinkage::pushIntegerWordArg(TR::Node *child)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   if (child->getRegister() == NULL)
      {
      if (child->getOpCode().isLoadConst())
         {
         int32_t value = child->getInt();
         TR::InstOpCode::Mnemonic pushOp;
         if (value >= -128 && value <= 127)
            {
            pushOp = TR::InstOpCode::PUSHImms;
            }
         else
            {
            pushOp = TR::InstOpCode::PUSHImm4;
            }

         if (child->getOpCodeValue() == TR::aconst && child->isMethodPointerConstant() &&
             cg()->needClassAndMethodPointerRelocations())
            {
            generateImmInstruction(pushOp, child, value, cg(), TR_MethodPointer);
            }
         else
            {
            generateImmInstruction(pushOp, child, value, cg());
            }
         cg()->decReferenceCount(child);
         return NULL;
         }
      else if (child->getOpCodeValue() == TR::loadaddr)
         {
         // Special casing for instanceof under an if relies on this push of unresolved being
         // implemented as a push imm, so don't change unless that code is changed to match.
         //
         TR::SymbolReference * symRef = child->getSymbolReference();
         TR::StaticSymbol *sym = symRef->getSymbol()->getStaticSymbol();
         if (sym)
            {
            if (symRef->isUnresolved())
               {
               generateX86UnresolvedDataSnippetWithCPIndex(child, symRef, 0);
               }
            else
               {
               // Must pass symbol reference so that aot can put out a relocation for it
               //
               TR::Instruction *instr = generateImmSymInstruction(TR::InstOpCode::PUSHImm4, child, (uintptr_t)sym->getStaticAddress(), symRef, cg());

               // HCR register the class passed as a parameter
               //
               if ((sym->isClassObject() || sym->isAddressOfClassObject())
                   && cg()->wantToPatchClassPointer((TR_OpaqueClassBlock*)sym->getStaticAddress(), child))
                  {
                  comp()->getStaticHCRPICSites()->push_front(instr);
                  }
               }

            cg()->decReferenceCount(child);
            return NULL;
            }
         }
      }

   return TR::IA32LinkageUtils::pushIntegerWordArg(child, cg());
   }

TR::Register *J9::X86::I386::PrivateLinkage::pushThis(TR::Node *child)
   {
   // Don't decrement the reference count on the "this" child until we've
   // had a chance to set up its dependency conditions
   //
   TR::Register *tempRegister = cg()->evaluate(child);
   generateRegInstruction(TR::InstOpCode::PUSHReg, child, tempRegister, cg());
   return tempRegister;
   }


static TR_AtomicRegion X86PicSlotAtomicRegion[] =
   {
   { 0x0, 8 }, // Maximum instruction size that can be patched atomically.
   { 0,0 }
   };

static TR_AtomicRegion X86PicCallAtomicRegion[] =
   {
   { 1, 4 },  // disp32 of a CALL instruction
   { 0,0 }
   };


TR::Instruction *J9::X86::I386::PrivateLinkage::buildPICSlot(
      TR::X86PICSlot picSlot,
      TR::LabelSymbol *mismatchLabel,
      TR::LabelSymbol *doneLabel,
      TR::X86CallSite &site)
   {
   TR::Node *node = site.getCallNode();
   TR::Register *vftReg = site.evaluateVFT();
   uintptr_t addrToBeCompared = picSlot.getClassAddress();
   TR::Instruction *firstInstruction;
   if (picSlot.getMethodAddress())
      {
      addrToBeCompared = (uintptr_t) picSlot.getMethodAddress();
      firstInstruction = generateMemImmInstruction(TR::InstOpCode::CMPMemImm4(false), node,
                                generateX86MemoryReference(vftReg, picSlot.getSlot(), cg()), (uint32_t) addrToBeCompared, cg());
      }
   else
      firstInstruction = generateRegImmInstruction(TR::InstOpCode::CMP4RegImm4, node, vftReg, addrToBeCompared, cg());

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
   else if (picSlot.needsNopAndJump())
      {
      generatePaddingInstruction(1, node, cg())->setNeedsGCMap((site.getArgSize()<<14) | site.getPreservedRegisterMask());
      generateLongLabelInstruction(TR::InstOpCode::JMP4, node, mismatchLabel, cg());
      }

   TR::Instruction *instr;
   if (picSlot.getMethod())
      {
      instr = generateImmInstruction(TR::InstOpCode::CALLImm4, node, (uint32_t)(uintptr_t)picSlot.getMethod()->startAddressForJittedMethod(), cg());
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

   if (picSlot.needsPicCallAlignment())
      {
      generateBoundaryAvoidanceInstruction(
         X86PicCallAtomicRegion,
         8,
         8,
         instr,
         cg());
      }

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

static TR_AtomicRegion ia32VPicAtomicRegions[] =
   {
   { 0x02, 4 }, // Class test immediate 1
   { 0x09, 4 }, // Call displacement 1
   { 0x17, 2 }, // VPICInit call instruction patched via self-loop
   { 0,0 }      // (null terminator)
   };

static TR_AtomicRegion ia32VPicAtomicRegionsRT[] =
   {
   { 0x02, 4 }, // Class test immediate 1
   { 0x06, 2 }, // nop+jmp opcodes
   { 0x08, 4 }, // Jne displacement 1
   { 0x0d, 2 }, // first two bytes of call instruction are atomically patched by virtualJitToInterpreted
   { 0,0 }      // (null terminator)
   };

static TR_AtomicRegion ia32IPicAtomicRegions[] =
   {
   { 0x02, 4 }, // Class test immediate 1
   { 0x09, 4 }, // Call displacement 1
   { 0x11, 4 }, // Class test immediate 2
   { 0x18, 4 }, // Call displacement 2
   { 0x27, 4 }, // IPICInit call displacement
   { 0,0 }      // (null terminator)
   };

static TR_AtomicRegion ia32IPicAtomicRegionsRT[] =
   {
   { 0x02, 4 }, // Class test immediate 1
   { 0x09, 4 }, // Call displacement 1
   { 0x11, 4 }, // Class test immediate 2
   { 0x18, 4 }, // Call displacement 2
   { 0x2f, 4 }, // IPICInit call displacement
   { 0,0 }      // (null terminator)
   };

void J9::X86::I386::PrivateLinkage::buildIPIC(
      TR::X86CallSite &site,
      TR::LabelSymbol *entryLabel,
      TR::LabelSymbol *doneLabel,
      uint8_t *thunk)
   {
   TR_ASSERT(doneLabel, "a doneLabel is required for IPIC dispatches");

   if (entryLabel)
      generateLabelInstruction(TR::InstOpCode::label, site.getCallNode(), entryLabel, cg());

   TR::Instruction *startOfPicInstruction = cg()->getAppendInstruction();

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
      if (breakBeforeIPICUsingLastITable)
         generateInstruction(TR::InstOpCode::bad, site.getCallNode(), cg());
      if (numIPicSlotsBeforeLastITable)
         numIPicSlots = atoi(numIPicSlotsBeforeLastITable);
   }

   if (numIPicSlots > 1)
      {
      TR::X86PICSlot emptyPicSlot = TR::X86PICSlot(-1, NULL);
      emptyPicSlot.setNeedsShortConditionalBranch();
      emptyPicSlot.setJumpOnNotEqual();
      emptyPicSlot.setNeedsPicSlotAlignment();
      emptyPicSlot.setNeedsPicCallAlignment();
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

   // Generate the last slot
   // (long branch to lookup snippet, fall through to doneLabel)
   //
   TR::X86PICSlot lastPicSlot = TR::X86PICSlot(-1, NULL, false);
   lastPicSlot.setJumpOnNotEqual();
   lastPicSlot.setNeedsPicSlotAlignment();
   lastPicSlot.setHelperMethodSymbolRef(callHelperSymRef);

   TR::LabelSymbol *lookupDispatchSnippetLabel = generateLabelSymbol(cg());

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
      lastPicSlot.setNeedsPicCallAlignment();
      lastPicSlot.setNeedsLongConditionalBranch();
      slotPatchInstruction = buildPICSlot(lastPicSlot, lookupDispatchSnippetLabel, NULL, site);
      }

   // Skip past any boundary avoidance padding to get to the real first instruction
   // of the PIC.
   //
   startOfPicInstruction = startOfPicInstruction->getNext();
   while (startOfPicInstruction->getOpCodeValue() == TR::InstOpCode::bad)
      {
      startOfPicInstruction = startOfPicInstruction->getNext();
      }

   TR::X86PicDataSnippet *snippet = new (trHeapMemory()) TR::X86PicDataSnippet(
      IPicParameters.defaultNumberOfSlots,
      startOfPicInstruction,
      lookupDispatchSnippetLabel,
      doneLabel,
      site.getSymbolReference(),
      slotPatchInstruction,
      site.getThunkAddress(),
      true,
      cg());

   snippet->gcMap().setGCRegisterMask((site.getArgSize()<<14) | site.getPreservedRegisterMask());
   cg()->addSnippet(snippet);
   }

void J9::X86::I386::PrivateLinkage::buildVirtualOrComputedCall(
      TR::X86CallSite &site,
      TR::LabelSymbol *entryLabel,
      TR::LabelSymbol *doneLabel,
      uint8_t *thunk)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   bool resolvedSite = !(site.getSymbolReference()->isUnresolved() || fej9->forceUnresolvedDispatch());
   if (site.getSymbolReference()->getSymbol()->castToMethodSymbol()->isComputed())
      {
      // TODO:JSR292: Try to avoid the explicit VFT load
      TR::Register *targetAddress = site.evaluateVFT();
      if (targetAddress->getRegisterPair())
         targetAddress = targetAddress->getRegisterPair()->getLowOrder();
      buildVFTCall(site, TR::InstOpCode::CALLReg, targetAddress, NULL);
      }
   else if (resolvedSite && site.resolvedVirtualShouldUseVFTCall())
      {
      if (entryLabel)
         generateLabelInstruction(TR::InstOpCode::label, site.getCallNode(), entryLabel, cg());

      intptr_t offset=site.getSymbolReference()->getOffset();
      if (!resolvedSite)
         offset = 0;

      buildVFTCall(site, TR::InstOpCode::CALLMem, NULL, generateX86MemoryReference(site.evaluateVFT(), offset, cg()));
      }
   else
      {
      J9::X86::PrivateLinkage::buildVPIC(site, entryLabel, doneLabel);
      }
   }

intptr_t
J9::X86::I386::PrivateLinkage::entryPointFromCompiledMethod()
   {
   return reinterpret_cast<intptr_t>(cg()->getCodeStart());
   }
