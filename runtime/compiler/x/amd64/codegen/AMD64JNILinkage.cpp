/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifdef TR_TARGET_64BIT

#include "codegen/AMD64JNILinkage.hpp"

#include <algorithm>
#include "codegen/CodeGenerator.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/Snippet.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/CompilerEnv.hpp"
#include "env/jittypes.h"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "env/VMJ9.h"
#include "runtime/Runtime.hpp"
#include "x/codegen/CheckFailureSnippet.hpp"
#include "x/codegen/HelperCallSnippet.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "x/codegen/J9LinkageUtils.hpp"

TR::Register *J9::X86::AMD64::JNILinkage::processJNIReferenceArg(TR::Node *child)
   {
   TR::Register *refReg;

   if (child->getOpCodeValue() == TR::loadaddr)
      {
      TR::SymbolReference *symRef = child->getSymbolReference();
      TR::StaticSymbol *staticSym = symRef->getSymbol()->getStaticSymbol();
      bool needsNullParameterCheck = false;

      if (staticSym)
         {
         // Address taken of static.
         //
         refReg = cg()->evaluate(child);
         if (!staticSym->isAddressOfClassObject())
            needsNullParameterCheck = true;
         }
      else
         {
         // Address taken of a parm or local.
         //
         if (child->pointsToNull())
            {
            refReg = cg()->allocateRegister();
            generateRegRegInstruction(TR::InstOpCode::XORRegReg(), child, refReg, refReg, cg());
            // TODO (81564): We need to kill the scratch register to prevent an
            // assertion error, but is this the right place to do so?
            cg()->stopUsingRegister(refReg);
            }
         else
            {
            refReg = cg()->evaluate(child);
            if (!child->pointsToNonNull())
               needsNullParameterCheck = true;
            }
         }

      if (needsNullParameterCheck)
         {
         generateMemImmInstruction(TR::InstOpCode::CMPMemImms(), child, generateX86MemoryReference(refReg, 0, cg()), 0, cg());
         generateRegMemInstruction(TR::InstOpCode::CMOVERegMem(), child, refReg, generateX86MemoryReference(cg()->findOrCreateConstantDataSnippet<intptr_t>(child, 0), cg()), cg());
         }
      }
   else
      {
      refReg = cg()->evaluate(child);
      }

   return refReg;
   }


int32_t J9::X86::AMD64::JNILinkage::computeMemoryArgSize(
   TR::Node *callNode,
   int32_t first,
   int32_t last,
   bool passThread)
   {
   int32_t size= 0;
   int32_t numFloatArgs = 0;
   int32_t numIntArgs = 0;

   int32_t slotSize = TR::Compiler->om.sizeofReferenceAddress();
   int32_t slotSizeMinus1 = slotSize-1;

   // For JNI dispatch, count the JNI env pointer first.  It is
   // assumed that for the 64-bit register passing linkages for JNI
   // that the evaluation order is left-to-right and that the JNI env
   // pointer will be in a register as the first argument.
   //
   if (passThread)
      {
      TR_ASSERT(!_systemLinkage->getProperties().passArgsRightToLeft(), "JNI argument evaluation expected to be left-to-right");
      TR_ASSERT(_systemLinkage->getProperties().getNumIntegerArgumentRegisters() > 0, "JNI env pointer expected to be passed in a register.");
      numIntArgs++;

      if (_systemLinkage->getProperties().getLinkageRegistersAssignedByCardinalPosition())
         numFloatArgs++;
      }

   bool allocateOnCallerFrame = false;

   for (int32_t i = first; i != last; i++)
      {
      TR::Node *child = callNode->getChild(i);
      TR::DataType type = child->getDataType();

      switch (type)
         {
         case TR::Float:
         case TR::Double:
            if (numFloatArgs >= _systemLinkage->getProperties().getNumFloatArgumentRegisters())
               allocateOnCallerFrame = true;
            numFloatArgs++;

            if (_systemLinkage->getProperties().getLinkageRegistersAssignedByCardinalPosition())
               numIntArgs++;

            break;

         case TR::Address:
         default:
            if (numIntArgs >= _systemLinkage->getProperties().getNumIntegerArgumentRegisters())
               allocateOnCallerFrame = true;
            numIntArgs++;

            if (_systemLinkage->getProperties().getLinkageRegistersAssignedByCardinalPosition())
               numFloatArgs++;

            break;
         }

      if (allocateOnCallerFrame)
         {
         int32_t roundedSize = (child->getSize()+slotSizeMinus1)&(~slotSizeMinus1);
         size += roundedSize ? roundedSize : slotSize;
         allocateOnCallerFrame = false;
         }
      }

   // Always reserve space on the caller's frame for the maximum number of linkage registers.
   //
   if (_systemLinkage->getProperties().getCallerFrameAllocatesSpaceForLinkageRegisters())
      {
      // TODO: this is Win64 Fastcall-specific at the moment.  It could be generalized if need be.
      //
      size += std::max(_systemLinkage->getProperties().getNumIntegerArgumentRegisters(),
                  _systemLinkage->getProperties().getNumFloatArgumentRegisters()) * TR::Compiler->om.sizeofReferenceAddress();
      }

   return size;
   }


// Build arguments for system linkage dispatch.
//
int32_t J9::X86::AMD64::JNILinkage::buildArgs(
      TR::Node *callNode,
      TR::RegisterDependencyConditions *deps,
      bool passThread,
      bool passReceiver)
   {
   TR::SymbolReference     *methodSymRef      = callNode->getSymbolReference();
   TR::MethodSymbol       *methodSymbol      = methodSymRef->getSymbol()->castToMethodSymbol();
   TR::Machine             *machine           = cg()->machine();
   TR::RealRegister::RegNum noReg             = TR::RealRegister::NoReg;
   TR::RealRegister        *espReal           = machine->getRealRegister(TR::RealRegister::esp);
   int32_t                  firstNodeArgument = callNode->getFirstArgumentIndex();
   int32_t                  lastNodeArgument  = callNode->getNumChildren() - 1;
   int32_t                  offset            = 0;
   uint16_t                 numIntArgs        = 0,
                            numFloatArgs      = 0;
   int32_t                  first, last;

   int32_t                  numCopiedRegs = 0;
   TR::Register             *copiedRegs[TR::X86LinkageProperties::MaxArgumentRegisters];

   TR_ASSERT(!_systemLinkage->getProperties().passArgsRightToLeft(), "JNI dispatch expected to be left-to-right.");

   if (!passReceiver)
      first = firstNodeArgument + 1;
   else first = firstNodeArgument;
   last = lastNodeArgument + 1;

   // We need to know how many arguments will be passed on the stack in order to add the right
   // amount of slush first for proper alignment.
   //
   int32_t memoryArgSize = computeMemoryArgSize(callNode, first, last, passThread);

   // For JNI callout there may be some extra information (such as the VMThread pointer)
   // that has been preserved on the stack that must be accounted for in this calculation.
   //
   int32_t adjustedMemoryArgSize = memoryArgSize + _JNIDispatchInfo.argSize;

   if (adjustedMemoryArgSize > 0)
      {
      // The C stack pointer (RSP) is 16-aligned before building any memory arguments (guaranteed by the VM).
      // Following the last memory argument the stack alignment must still be 16.
      //
      if ((adjustedMemoryArgSize % 16) != 0)
         memoryArgSize += 8;

      TR::InstOpCode::Mnemonic op = (memoryArgSize >= -128 && memoryArgSize <= 127) ? TR::InstOpCode::SUBRegImms() : TR::InstOpCode::SUBRegImm4();
      generateRegImmInstruction(op, callNode, espReal, memoryArgSize, cg());
      }

   // Evaluate the JNI env pointer first.
   //
   if (passThread)
      {
      TR::RealRegister::RegNum rregIndex = _systemLinkage->getProperties().getIntegerArgumentRegister(0);
      TR::Register *argReg = cg()->allocateRegister();
      generateRegRegInstruction(TR::Linkage::movOpcodes(RegReg, movType(TR::Address)), callNode, argReg, cg()->getMethodMetaDataRegister(), cg());
      deps->addPreCondition(argReg, rregIndex, cg());
      cg()->stopUsingRegister(argReg);
      numIntArgs++;

      if (_systemLinkage->getProperties().getLinkageRegistersAssignedByCardinalPosition())
         numFloatArgs++;

      if (_systemLinkage->getProperties().getCallerFrameAllocatesSpaceForLinkageRegisters())
         {
         int32_t slotSize = TR::Compiler->om.sizeofReferenceAddress();
         int32_t slotSizeMinus1 = slotSize-1;

         int32_t roundedSize = (slotSize+slotSizeMinus1)&(~slotSizeMinus1);
         offset += roundedSize ? roundedSize : slotSize;
         }
      }

   // if fastJNI do not pass the receiver just evaluate the first child
   if (!passReceiver)
      {
      TR::Node *child = callNode->getChild(first-1);
      cg()->evaluate(child);
      cg()->decReferenceCount(child);
      }

   int32_t i;
   for (i = first; i != last; i++)
      {
      TR::Node *child = callNode->getChild(i);
      TR::DataType type = child->getDataType();
      TR::RealRegister::RegNum rregIndex = noReg;

      switch (type)
         {
         case TR::Float:
         case TR::Double:
            rregIndex =
               (numFloatArgs < _systemLinkage->getProperties().getNumFloatArgumentRegisters()) ? _systemLinkage->getProperties().getFloatArgumentRegister(numFloatArgs) : noReg;
            numFloatArgs++;

            if (_systemLinkage->getProperties().getLinkageRegistersAssignedByCardinalPosition())
               numIntArgs++;

            break;

         case TR::Address:
            // Deliberate fall through for TR::Address types.
            //
         default:
            rregIndex =
               (numIntArgs < _systemLinkage->getProperties().getNumIntegerArgumentRegisters()) ? _systemLinkage->getProperties().getIntegerArgumentRegister(numIntArgs) : noReg;
            numIntArgs++;

            if (_systemLinkage->getProperties().getLinkageRegistersAssignedByCardinalPosition())
               numFloatArgs++;

            break;
         }

      TR::Register *vreg;
      if (type == TR::Address)
         vreg = processJNIReferenceArg(child);
      else
         vreg = cg()->evaluate(child);

      bool needsStackOffsetUpdate = false;
      if (rregIndex != noReg)
         {
         // For NULL JNI reference parameters, it is possible that the NULL value will be evaluated into
         // a different register than the child.  In that case it is not necessary to copy the temporary scratch
         // register across the call.
         //
         if ((child->getReferenceCount() > 1) &&
             (vreg == child->getRegister()))
            {
            TR::Register *argReg = cg()->allocateRegister();
            if (vreg->containsCollectedReference())
               argReg->setContainsCollectedReference();
            generateRegRegInstruction(TR::Linkage::movOpcodes(RegReg, movType(child->getDataType())), child, argReg, vreg, cg());
            vreg = argReg;
            copiedRegs[numCopiedRegs++] = vreg;
            }

         deps->addPreCondition(vreg, rregIndex, cg());

         if (_systemLinkage->getProperties().getCallerFrameAllocatesSpaceForLinkageRegisters())
            needsStackOffsetUpdate = true;
         }
      else
         {
         // Ideally, we would like to push rather than move
         generateMemRegInstruction(TR::Linkage::movOpcodes(MemReg, fullRegisterMovType(vreg)),
                                   child,
                                   generateX86MemoryReference(espReal, offset, cg()),
                                   vreg,
                                   cg());

         needsStackOffsetUpdate = true;
         }

      if (needsStackOffsetUpdate)
         {
         int32_t slotSize = TR::Compiler->om.sizeofReferenceAddress();
         int32_t slotSizeMinus1 = slotSize-1;

         int32_t roundedSize = (child->getSize()+slotSizeMinus1)&(~slotSizeMinus1);
         offset += roundedSize ? roundedSize : slotSize;
         }

      cg()->decReferenceCount(child);
      }

   // Now that we're finished making the preconditions, all the interferences
   // are established and we can kill these regs.
   //
   for (i = 0; i < numCopiedRegs; i++)
      cg()->stopUsingRegister(copiedRegs[i]);

   deps->stopAddingPreConditions();

   return memoryArgSize;
   }


TR::Register *
J9::X86::AMD64::JNILinkage::buildVolatileAndReturnDependencies(
      TR::Node *callNode,
      TR::RegisterDependencyConditions *deps,
      bool omitDedicatedFrameRegister)
   {
   TR_ASSERT(deps != NULL, "expected register dependencies");

   // Figure out which is the return register.
   //
   TR::RealRegister::RegNum returnRegIndex;
   TR_RegisterKinds         returnKind;

   switch (callNode->getDataType())
      {
      default:
         TR_ASSERT(0, "Unrecognized call node data type: #%d", (int)callNode->getDataType());
         // deliberate fall-through
      case TR::NoType:
         returnRegIndex = TR::RealRegister::NoReg;
         returnKind     = TR_NoRegister;
         break;
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
      case TR::Int64:
      case TR::Address:
         returnRegIndex = _systemLinkage->getProperties().getIntegerReturnRegister();
         returnKind     = TR_GPR;
         break;
      case TR::Float:
      case TR::Double:
         returnRegIndex = _systemLinkage->getProperties().getFloatReturnRegister();
         returnKind     = TR_FPR;
         break;
      }

   // Kill all non-preserved int and float regs besides the return register.
   //
   int32_t i;
   TR::RealRegister::RegNum scratchIndex = _systemLinkage->getProperties().getIntegerScratchRegister(1);
   for (i=0; i<_systemLinkage->getProperties().getNumVolatileRegisters(); i++)
      {
      TR::RealRegister::RegNum regIndex = _systemLinkage->getProperties()._volatileRegisters[i];

      if (regIndex != returnRegIndex)
         {
         if (!omitDedicatedFrameRegister || (regIndex != _JNIDispatchInfo.dedicatedFrameRegisterIndex))
            {
            TR_RegisterKinds rk = (i < _systemLinkage->getProperties()._numberOfVolatileGPRegisters) ? TR_GPR : TR_FPR;
            TR::Register *dummy = cg()->allocateRegister(rk);
            deps->addPostCondition(dummy, regIndex, cg());

            // Note that we don't setPlaceholderReg here.  If this volatile reg is also volatile
            // in the caller's linkage, then that flag doesn't matter much anyway.  If it's preserved
            // in the caller's linkage, then we don't want to set that flag because we want this
            // use of the register to count as a "real" use, thereby motivating the prologue to
            // preserve the regsiter.

            // A scratch register is necessary to call the native without a trampoline.
            //
            if (regIndex != scratchIndex)
               cg()->stopUsingRegister(dummy);
            }
         }
      }

   // Add a VMThread register dependence.
   //
   deps->addPostCondition(cg()->getVMThreadRegister(), TR::RealRegister::ebp, cg());

   // Now that everything is dead, we can allocate the return register without
   // interference
   //
   TR::Register *returnRegister;
   if (returnRegIndex)
      {
      TR_ASSERT(returnKind != TR_NoRegister, "assertion failure");

      if (callNode->getDataType() == TR::Address)
         returnRegister = cg()->allocateCollectedReferenceRegister();
      else
         {
         returnRegister = cg()->allocateRegister(returnKind);
         if (callNode->getDataType() == TR::Float)
            returnRegister->setIsSinglePrecision();
         }

      deps->addPostCondition(returnRegister, returnRegIndex, cg());
      }
   else
      returnRegister = NULL;

   deps->stopAddingPostConditions();

   return returnRegister;
   }


void J9::X86::AMD64::JNILinkage::buildJNICallOutFrame(
      TR::Node *callNode,
      TR::LabelSymbol *returnAddrLabel)
   {
   TR::ResolvedMethodSymbol *callSymbol = callNode->getSymbol()->castToResolvedMethodSymbol();
   TR_ResolvedMethod *resolvedMethod = callSymbol->getResolvedMethod();
   TR::Register *vmThreadReg = cg()->getMethodMetaDataRegister();
   TR::Register *scratchReg = NULL;
   TR::RealRegister *espReal = machine()->getRealRegister(TR::RealRegister::esp);

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());

   // Mask out the magic bit that indicates JIT frames below.
   //
   generateMemImmInstruction(
      TR::InstOpCode::SMemImm4(),
      callNode,
      generateX86MemoryReference(vmThreadReg, fej9->thisThreadGetJavaFrameFlagsOffset(), cg()),
      0,
      cg());

   // Grab 5 slots in the frame.
   //
   //    4:   tag bits (savedA0)
   //    3:   empty (savedPC)
   //    2:   return address in this frame (savedCP)
   //    1:   frame flags
   //    0:   RAM method
   //

   // Build stack frame in Java stack.  Tag current bp.
   //
   uintptr_t tagBits = 0;

   // If the current method is simply a wrapper for the JNI call, hide the call-out stack frame.
   //
   static_assert(IS_32BIT_SIGNED(J9SF_A0_INVISIBLE_TAG), "J9SF_A0_INVISIBLE_TAG must fit in immediate");
   if (resolvedMethod == comp()->getCurrentMethod())
      tagBits |= J9SF_A0_INVISIBLE_TAG;

   // Push tag bits (savedA0 slot).
   //
   generateImmInstruction(TR::InstOpCode::PUSHImm4, callNode, tagBits, cg());

   // (skip savedPC slot).
   //
   generateImmInstruction(TR::InstOpCode::PUSHImm4, callNode, 0, cg());

   // Push return address in this frame (savedCP slot).
   //
   if (!scratchReg)
      scratchReg = cg()->allocateRegister();

   TR::AMD64RegImm64SymInstruction* returnAddressInstr =
   generateRegImm64SymInstruction(
      TR::InstOpCode::MOV8RegImm64,
      callNode,
      scratchReg,
      0,
      new (trHeapMemory()) TR::SymbolReference(comp()->getSymRefTab(), returnAddrLabel),
      cg());

   returnAddressInstr->setReloKind(TR_AbsoluteMethodAddress);

   generateRegInstruction(TR::InstOpCode::PUSHReg, callNode, scratchReg, cg());

   // Push frame flags.
   //
   static_assert(IS_32BIT_SIGNED(J9_SSF_JIT_JNI_CALLOUT), "J9_SSF_JIT_JNI_CALLOUT must fit in immediate");
   generateImmInstruction(TR::InstOpCode::PUSHImm4, callNode, J9_SSF_JIT_JNI_CALLOUT, cg());

   // Push the RAM method for the native.
   //
   auto tempMR = generateX86MemoryReference(espReal, 0, cg());
   uintptr_t methodAddr = (uintptr_t) resolvedMethod->resolvedMethodAddress();
   if (IS_32BIT_SIGNED(methodAddr) && !TR::Compiler->om.nativeAddressesCanChangeSize())
      {
      generateImmInstruction(TR::InstOpCode::PUSHImm4, callNode, methodAddr, cg());
      }
   else
      {
      if (!scratchReg)
         scratchReg = cg()->allocateRegister();

      static const TR_ExternalRelocationTargetKind reloTypes[] = { TR_VirtualRamMethodConst, TR_NoRelocation /*Interfaces*/, TR_StaticRamMethodConst, TR_SpecialRamMethodConst };
      int reloType = callSymbol->getMethodKind() - 1; //method kinds are 1-based!!
      TR_ASSERT(reloTypes[reloType] != TR_NoRelocation, "There shouldn't be direct JNI interface calls!");
      generateRegImm64Instruction(TR::InstOpCode::MOV8RegImm64, callNode, scratchReg, methodAddr, cg(), reloTypes[reloType]);
      generateRegInstruction(TR::InstOpCode::PUSHReg, callNode, scratchReg, cg());
      }

   // Store out pc and literals values indicating the callout frame.
   //
   static_assert(IS_32BIT_SIGNED(J9SF_FRAME_TYPE_JIT_JNI_CALLOUT), "J9SF_FRAME_TYPE_JIT_JNI_CALLOUT must fit in immediate");
   generateMemImmInstruction(TR::InstOpCode::SMemImm4(), callNode, generateX86MemoryReference(vmThreadReg, fej9->thisThreadGetJavaPCOffset(), cg()), J9SF_FRAME_TYPE_JIT_JNI_CALLOUT, cg());

   if (scratchReg)
      cg()->stopUsingRegister(scratchReg);

   generateMemImmInstruction(TR::InstOpCode::SMemImm4(),
                             callNode,
                             generateX86MemoryReference(vmThreadReg, fej9->thisThreadGetJavaLiteralsOffset(), cg()),
                             0,
                             cg());
   }

void
J9::X86::AMD64::JNILinkage::buildJNIMergeLabelDependencies(TR::Node *callNode, bool killNonVolatileGPRs)
   {
   TR::RegisterDependencyConditions *deps = _JNIDispatchInfo.mergeLabelPostDeps;

   // Allocate the actual register that the JNI result will be returned in.  This may be different
   // than the linkage return register on the call because of register usage constraints after the
   // call (e.g., re-acquiring the VM).
   //
   TR::RealRegister::RegNum returnRegIndex;
   TR::Register *linkageReturnRegister = _JNIDispatchInfo.linkageReturnRegister;
   TR::Register *JNIReturnRegister;

   if (linkageReturnRegister)
      {
      JNIReturnRegister = cg()->allocateRegister(linkageReturnRegister->getKind());
      if (linkageReturnRegister->containsCollectedReference())
         JNIReturnRegister->setContainsCollectedReference();
      else if (linkageReturnRegister->isSinglePrecision())
         JNIReturnRegister->setIsSinglePrecision();

      if (JNIReturnRegister->getKind() == TR_GPR)
         returnRegIndex = TR::RealRegister::ecx;
      else
         returnRegIndex = _systemLinkage->getProperties().getFloatReturnRegister();

      deps->addPostCondition(JNIReturnRegister, returnRegIndex, cg());
      }
   else
      {
      JNIReturnRegister = NULL;
      returnRegIndex = TR::RealRegister::NoReg;
      }

   _JNIDispatchInfo.JNIReturnRegister = JNIReturnRegister;

   // Kill all registers across the JNI callout sequence.
   //
   int32_t i;
   for (i=0; i<_systemLinkage->getProperties().getNumVolatileRegisters(); i++)
      {
      TR::RealRegister::RegNum regIndex = _systemLinkage->getProperties()._volatileRegisters[i];

      if (regIndex != returnRegIndex)
         {
         TR_RegisterKinds rk = (i < _systemLinkage->getProperties()._numberOfVolatileGPRegisters) ? TR_GPR : TR_FPR;
         TR::Register *dummy = cg()->allocateRegister(rk);

         // Don't setPlaceholderReg here in case the caller's linkage lists this one as a preserved reg
         //
         deps->addPostCondition(dummy, regIndex, cg());
         cg()->stopUsingRegister(dummy);
         }
      }

   if (killNonVolatileGPRs)
      {
      for (i=0; i<_systemLinkage->getProperties().getNumPreservedRegisters(); i++)
         {
         TR::RealRegister::RegNum regIndex = _systemLinkage->getProperties()._preservedRegisters[i];

         if (regIndex != returnRegIndex)
            {
            TR_RegisterKinds rk = (i < _systemLinkage->getProperties()._numberOfPreservedGPRegisters) ? TR_GPR : TR_FPR;
            TR::Register *dummy = cg()->allocateRegister(rk);

            // Don't setPlaceholderReg here.  We release VM access around the
            // native call, so even though the callee's linkage won't alter a
            // given register, we still have a problem if a stack walk needs to
            // inspect/modify that register because once we're in C-land, we have
            // no idea where that regsiter's value is located.  Therefore, we need
            // to spill even the callee-saved registers around the call.
            //
            // In principle, we only need to do this for registers that contain
            // references.  However, at this location in the code, we don't yet
            // know which real registers those would be.  Tragically, this causes
            // us to save/restore ALL preserved registers in any method containing
            // a JNI call.

            deps->addPostCondition(dummy, regIndex, cg());
            cg()->stopUsingRegister(dummy);
            }
         }
      }

   // Add a VMThread register dependence.
   //
   deps->addPostCondition(cg()->getVMThreadRegister(), TR::RealRegister::ebp, cg());
   deps->stopAddingPostConditions();
   }

void J9::X86::AMD64::JNILinkage::buildOutgoingJNIArgsAndDependencies(
      TR::Node *callNode,
      bool passThread,
      bool passReceiver,
      bool killNonVolatileGPRs)
   {
   // Allocate adequate register dependencies.  The pre and post conditions are distinguished because they will
   // appear on different instructions.
   //
   // callPre = number of argument registers
   // callPost = number of volatile + VMThread + return register
   // labelPost = number of volatile + preserved + VMThread + return register
   //
   uint32_t callPre = _systemLinkage->getProperties().getNumIntegerArgumentRegisters() + _systemLinkage->getProperties().getNumFloatArgumentRegisters();
   uint32_t callPost = _systemLinkage->getProperties().getNumVolatileRegisters() + 1 + (callNode->getDataType() == TR::NoType ? 0 : 1);

   // This is overly pessimistic.  In reality, only the linkage preserved registers that contain collected references
   // need to be evicted across the JNI call.  There currently isn't a convenient mechanism to specify this...
   //
   uint32_t labelPost = _systemLinkage->getProperties().getNumVolatileRegisters() +
                        _systemLinkage->getProperties().getNumPreservedRegisters() + 1 +
                        (callNode->getDataType() == TR::NoType ? 0 : 1);

   _JNIDispatchInfo.callPostDeps       = generateRegisterDependencyConditions(callPre, callPost, cg());
   _JNIDispatchInfo.mergeLabelPostDeps = generateRegisterDependencyConditions(0, labelPost, cg());

   // Evaluate outgoing arguments on the system stack and build pre-conditions.
   //
   _JNIDispatchInfo.argSize += buildArgs(callNode, _JNIDispatchInfo.callPostDeps, passThread, passReceiver);

   // Build post-conditions.
   //
   // Omit the dedicated frame register because it will be in Locked state by the time
   // the register assigner reaches the call instruction, and the assignment won't happen.
   //
   _JNIDispatchInfo.linkageReturnRegister = buildVolatileAndReturnDependencies(callNode, _JNIDispatchInfo.callPostDeps, true);

   for (int32_t i=0; i<callPost; i++)
      {
      TR::RegisterDependency *dep = _JNIDispatchInfo.callPostDeps->getPostConditions()->getRegisterDependency(i);
      if (dep->getRealRegister() == _systemLinkage->getProperties().getIntegerScratchRegister(1))
         {
         _JNIDispatchInfo.dispatchTrampolineRegister = dep->getRegister();
         break;
         }
      }

   buildJNIMergeLabelDependencies(callNode, killNonVolatileGPRs);
   }

TR::Instruction *
J9::X86::AMD64::JNILinkage::generateMethodDispatch(
      TR::Node *callNode,
      bool isJNIGCPoint,
      uintptr_t targetAddress)
   {
   TR::ResolvedMethodSymbol *callSymbol  = callNode->getSymbol()->castToResolvedMethodSymbol();
   TR::RealRegister *espReal     = machine()->getRealRegister(TR::RealRegister::esp);
   TR::Register *vmThreadReg = cg()->getMethodMetaDataRegister();
   intptr_t argSize     = _JNIDispatchInfo.argSize;
   TR::SymbolReference *methodSymRef= callNode->getSymbolReference();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp()->fe());

   if (methodSymRef->getReferenceNumber()>=TR_AMD64numRuntimeHelpers)
      {
      fej9->reserveTrampolineIfNecessary(comp(), methodSymRef, false);
      }

   // Load machine bp esp + offsetof(J9CInterpreterStackFrame, machineBP) + argSize
   //
   generateRegMemInstruction(TR::InstOpCode::LRegMem(),
                             callNode,
                             vmThreadReg,
                             generateX86MemoryReference(espReal, offsetof(J9CInterpreterStackFrame, machineBP) + argSize, cg()),
                             cg());

   // Dispatch JNI method directly.
   //
   TR_ASSERT(_JNIDispatchInfo.dispatchTrampolineRegister, "No trampoline scratch register available for directJNI dispatch.");

   // TODO: Need an AOT relocation here.
   //
   // The code below is disabled because of a problem with direct call to JNI methods
   // through trampoline. The problem is that at the time of generation of the trampoline
   // we don't know if we did directToJNI call or we are calling the native thunk. In case
   // we are calling the thunk the code below won't work and it will give the caller address
   // of the C native and we crash.
   //


   static const TR_ExternalRelocationTargetKind reloTypes[] = {TR_JNIVirtualTargetAddress, TR_NoRelocation /*Interfaces*/, TR_JNIStaticTargetAddress, TR_JNISpecialTargetAddress};
   int reloType = callSymbol->getMethodKind()-1;  //method kinds are 1-based!!

   TR_ASSERT(reloTypes[reloType] != TR_NoRelocation, "There shouldn't be direct JNI interface calls!");

   TR::X86RegInstruction *patchedInstr=
   generateRegImm64Instruction(
      TR::InstOpCode::MOV8RegImm64,
      callNode,
      _JNIDispatchInfo.dispatchTrampolineRegister,
      targetAddress,
      cg(), reloTypes[reloType]);

   TR::X86RegInstruction *instr = generateRegInstruction(
      TR::InstOpCode::CALLReg,
      callNode,
      _JNIDispatchInfo.dispatchTrampolineRegister,
      _JNIDispatchInfo.callPostDeps,
      cg());
   cg()->getJNICallSites().push_front(new (trHeapMemory()) TR_Pair<TR_ResolvedMethod, TR::Instruction>(callSymbol->getResolvedMethod(), patchedInstr));

   if (isJNIGCPoint)
      instr->setNeedsGCMap(_systemLinkage->getProperties().getPreservedRegisterMapForGC());

   if (_JNIDispatchInfo.dispatchTrampolineRegister)
      cg()->stopUsingRegister(_JNIDispatchInfo.dispatchTrampolineRegister);

   // Clean up argument area if not callee cleanup linkage.
   //
   // The total C stack argument size includes the push of the VMThread register plus
   // the memory arguments for the call.  Only clean up the memory argument portion.
   //
   if (!cg()->getJNILinkageCalleeCleanup())
      {
      intptr_t cleanUpSize = argSize - TR::Compiler->om.sizeofReferenceAddress();

      if (comp()->target().is64Bit())
         TR_ASSERT(cleanUpSize <= 0x7fffffff, "Caller cleanup argument size too large for one instruction on AMD64.");


      if (cleanUpSize != 0)
         {
         TR::InstOpCode::Mnemonic op = (cleanUpSize >= -128 && cleanUpSize <= 127) ? TR::InstOpCode::ADDRegImms() : TR::InstOpCode::ADDRegImm4();
         generateRegImmInstruction(op, callNode, espReal, cleanUpSize, cg());
         }
      }

   return instr;
   }


void J9::X86::AMD64::JNILinkage::releaseVMAccess(TR::Node *callNode)
   {
   // Release VM access (spin lock).
   //
   //    mov        scratch1, [rbp+publicFlags]
   // loopHead:
   //    mov        scratch2, scratch1
   //    test       scratch1, constReleaseVMAccessOutOfLineMask
   //    jne        longReleaseSnippet
   //    and        scratch2, constReleaseVMAccessMask
   //    [l]cmpxchg [rbp+publicFlags], scratch2
   //    jne        pauseSnippet OR loopHead
   // longReleaseRestart:
   //    scratch1 <-> RAX
   //    scratch2 <-> NoReg
   //    scratch3 <-> NoReg
   //
   TR::InstOpCode::Mnemonic op;

   TR::Register *vmThreadReg = cg()->getMethodMetaDataRegister();
   TR::Register *scratchReg1 = cg()->allocateRegister();
   TR::Register *scratchReg2 = cg()->allocateRegister();
   TR::Register *scratchReg3 = NULL;
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());

   generateRegMemInstruction(TR::InstOpCode::LRegMem(),
                             callNode,
                             scratchReg1,
                             generateX86MemoryReference(vmThreadReg, fej9->thisThreadGetPublicFlagsOffset(), cg()),
                             cg());

   TR::LabelSymbol *loopHeadLabel = generateLabelSymbol(cg());

   // Loop head
   //
   generateLabelInstruction(TR::InstOpCode::label, callNode, loopHeadLabel, cg());
   generateRegRegInstruction(TR::InstOpCode::MOVRegReg(), callNode, scratchReg2, scratchReg1, cg());

   TR::LabelSymbol *longReleaseSnippetLabel = generateLabelSymbol(cg());
   TR::LabelSymbol *longReleaseRestartLabel = generateLabelSymbol(cg());

   uintptr_t mask = fej9->constReleaseVMAccessOutOfLineMask();

   if (comp()->target().is64Bit() && (mask > 0x7fffffff))
      {
      if (!scratchReg3)
         scratchReg3 = cg()->allocateRegister();

      generateRegImm64Instruction(TR::InstOpCode::MOV8RegImm64, callNode, scratchReg3, mask, cg());
      generateRegRegInstruction(TR::InstOpCode::TEST8RegReg, callNode, scratchReg1, scratchReg3, cg());
      }
   else
      {
      op = (mask <= 255) ? TR::InstOpCode::TEST1RegImm1 : TR::InstOpCode::TEST4RegImm4;
      generateRegImmInstruction(op, callNode, scratchReg1, mask, cg());
      }
   generateLabelInstruction(TR::InstOpCode::JNE4, callNode, longReleaseSnippetLabel, cg());

   {
   TR_OutlinedInstructionsGenerator og(longReleaseSnippetLabel, callNode, cg());
   auto helper = comp()->getSymRefTab()->findOrCreateReleaseVMAccessSymbolRef(comp()->getMethodSymbol());
   generateImmSymInstruction(TR::InstOpCode::CALLImm4, callNode, (uintptr_t)helper->getMethodAddress(), helper, cg());
   generateLabelInstruction(TR::InstOpCode::JMP4, callNode, longReleaseRestartLabel, cg());
   og.endOutlinedInstructionSequence();
   }

   mask = fej9->constReleaseVMAccessMask();

   if (comp()->target().is64Bit() && (mask > 0x7fffffff))
      {
      if (!scratchReg3)
         scratchReg3 = cg()->allocateRegister();

      generateRegImm64Instruction(TR::InstOpCode::MOV8RegImm64, callNode, scratchReg3, mask, cg());
      generateRegRegInstruction(TR::InstOpCode::AND8RegReg, callNode, scratchReg2, scratchReg3, cg());
      }
   else
      {
      op = (mask <= 255) ? TR::InstOpCode::AND1RegImm1 : TR::InstOpCode::AND4RegImm4;
      generateRegImmInstruction(op, callNode, scratchReg2, mask, cg());
      }

   op = comp()->target().isSMP() ? TR::InstOpCode::LCMPXCHGMemReg() : TR::InstOpCode::CMPXCHGMemReg(cg());
   generateMemRegInstruction(
      op,
      callNode,
      generateX86MemoryReference(vmThreadReg, fej9->thisThreadGetPublicFlagsOffset(), cg()),
      scratchReg2,
      cg());

   generateLabelInstruction(TR::InstOpCode::JNE4, callNode, loopHeadLabel, cg());

   int8_t numDeps = scratchReg3 ? 3 : 2;
   TR::RegisterDependencyConditions *deps = generateRegisterDependencyConditions(numDeps, numDeps, cg());
   deps->addPreCondition(scratchReg1, TR::RealRegister::eax, cg());
   deps->addPostCondition(scratchReg1, TR::RealRegister::eax, cg());
   cg()->stopUsingRegister(scratchReg1);

   deps->addPreCondition(scratchReg2, TR::RealRegister::NoReg, cg());
   deps->addPostCondition(scratchReg2, TR::RealRegister::NoReg, cg());
   cg()->stopUsingRegister(scratchReg2);

   if (scratchReg3)
      {
      deps->addPreCondition(scratchReg3, TR::RealRegister::NoReg, cg());
      deps->addPostCondition(scratchReg3, TR::RealRegister::NoReg, cg());
      cg()->stopUsingRegister(scratchReg3);
      }

   deps->stopAddingConditions();

   generateLabelInstruction(TR::InstOpCode::label, callNode, longReleaseRestartLabel, deps, cg());
   }


void J9::X86::AMD64::JNILinkage::acquireVMAccess(TR::Node *callNode)
   {
   // Re-acquire VM access.
   //
   //    xor        scratch1, scratch1
   //    mov        rdi, constAcquireVMAccessOutOfLineMask
   //    [l]cmpxchg [rbp + publicFlags], rdi
   //    jne        longReacquireSnippetLabel
   // longReacquireRestartLabel:
   //

   TR::Register *vmThreadReg = cg()->getMethodMetaDataRegister();
   TR::Register *scratchReg1 = cg()->allocateRegister();
   TR::Register *scratchReg2 = cg()->allocateRegister();

   generateRegRegInstruction(TR::InstOpCode::XORRegReg(), callNode, scratchReg1, scratchReg1, cg());

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   uintptr_t mask = fej9->constAcquireVMAccessOutOfLineMask();

   if (comp()->target().is64Bit() && (mask > 0x7fffffff))
      generateRegImm64Instruction(TR::InstOpCode::MOV8RegImm64, callNode, scratchReg2, mask, cg());
   else
      generateRegImmInstruction(TR::InstOpCode::MOV4RegImm4, callNode, scratchReg2, mask, cg());

   TR::LabelSymbol *longReacquireSnippetLabel = generateLabelSymbol(cg());
   TR::LabelSymbol *longReacquireRestartLabel = generateLabelSymbol(cg());

   TR::InstOpCode::Mnemonic op = comp()->target().isSMP() ? TR::InstOpCode::LCMPXCHGMemReg() : TR::InstOpCode::CMPXCHGMemReg(cg());
   generateMemRegInstruction(
      op,
      callNode,
      generateX86MemoryReference(vmThreadReg, fej9->thisThreadGetPublicFlagsOffset(), cg()),
      scratchReg2,
      cg());
   generateLabelInstruction(TR::InstOpCode::JNE4, callNode, longReacquireSnippetLabel, cg());

   // TODO: ecx may hold a reference across this snippet
   // If the return type is address something needs to be represented in the
   // register map for the snippet.
   //
   {
   TR_OutlinedInstructionsGenerator og(longReacquireSnippetLabel, callNode, cg());
   auto helper = comp()->getSymRefTab()->findOrCreateAcquireVMAccessSymbolRef(comp()->getMethodSymbol());
   generateImmSymInstruction(TR::InstOpCode::CALLImm4, callNode, (uintptr_t)helper->getMethodAddress(), helper, cg());
   generateLabelInstruction(TR::InstOpCode::JMP4, callNode, longReacquireRestartLabel, cg());
   og.endOutlinedInstructionSequence();
   }
   TR::RegisterDependencyConditions *deps = generateRegisterDependencyConditions(2, 2, cg());
   deps->addPreCondition(scratchReg1, TR::RealRegister::eax, cg());
   deps->addPostCondition(scratchReg1, TR::RealRegister::eax, cg());
   cg()->stopUsingRegister(scratchReg1);

   deps->addPreCondition(scratchReg2, TR::RealRegister::NoReg, cg());
   deps->addPostCondition(scratchReg2, TR::RealRegister::NoReg, cg());
   cg()->stopUsingRegister(scratchReg2);

   deps->stopAddingConditions();

   generateLabelInstruction(TR::InstOpCode::label, callNode, longReacquireRestartLabel, deps, cg());
   }


#ifdef J9VM_INTERP_ATOMIC_FREE_JNI
void J9::X86::AMD64::JNILinkage::releaseVMAccessAtomicFree(TR::Node *callNode)
   {
   TR::Register *vmThreadReg = cg()->getMethodMetaDataRegister();

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());

   generateMemImmInstruction(TR::InstOpCode::S8MemImm4,
                             callNode,
                             generateX86MemoryReference(vmThreadReg, offsetof(struct J9VMThread, inNative), cg()),
                             1,
                             cg());

#if !defined(J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH)
   TR::MemoryReference *mr = generateX86MemoryReference(cg()->machine()->getRealRegister(TR::RealRegister::esp), intptr_t(0), cg());
   mr->setRequiresLockPrefix();
   generateMemImmInstruction(TR::InstOpCode::OR4MemImms, callNode, mr, 0, cg());
#endif /* !J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH */

   TR::LabelSymbol *longReleaseSnippetLabel = generateLabelSymbol(cg());
   TR::LabelSymbol *longReleaseRestartLabel = generateLabelSymbol(cg());

   static_assert(IS_32BIT_SIGNED(J9_PUBLIC_FLAGS_VM_ACCESS), "J9_PUBLIC_FLAGS_VM_ACCESS must fit in immediate");
   generateMemImmInstruction(J9_PUBLIC_FLAGS_VM_ACCESS < 128 ? TR::InstOpCode::CMP4MemImms : TR::InstOpCode::CMP4MemImm4,
                             callNode,
                             generateX86MemoryReference(vmThreadReg, fej9->thisThreadGetPublicFlagsOffset(), cg()),
                             J9_PUBLIC_FLAGS_VM_ACCESS,
                             cg());
   generateLabelInstruction(TR::InstOpCode::JNE4, callNode, longReleaseSnippetLabel, cg());
   generateLabelInstruction(TR::InstOpCode::label, callNode, longReleaseRestartLabel, cg());

   TR_OutlinedInstructionsGenerator og(longReleaseSnippetLabel, callNode, cg());
   auto helper = comp()->getSymRefTab()->findOrCreateReleaseVMAccessSymbolRef(comp()->getMethodSymbol());
   generateImmSymInstruction(TR::InstOpCode::CALLImm4, callNode, (uintptr_t)helper->getMethodAddress(), helper, cg());
   generateLabelInstruction(TR::InstOpCode::JMP4, callNode, longReleaseRestartLabel, cg());
   og.endOutlinedInstructionSequence();
   }


void J9::X86::AMD64::JNILinkage::acquireVMAccessAtomicFree(TR::Node *callNode)
   {
   TR::Register *vmThreadReg = cg()->getMethodMetaDataRegister();

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());

   generateMemImmInstruction(TR::InstOpCode::S8MemImm4,
                             callNode,
                             generateX86MemoryReference(vmThreadReg, offsetof(struct J9VMThread, inNative), cg()),
                             0,
                             cg());

#if !defined(J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH)
   TR::MemoryReference *mr = generateX86MemoryReference(cg()->machine()->getRealRegister(TR::RealRegister::esp), intptr_t(0), cg());
   mr->setRequiresLockPrefix();
   generateMemImmInstruction(TR::InstOpCode::OR4MemImms, callNode, mr, 0, cg());
#endif /* !J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH */

   TR::LabelSymbol *longAcquireSnippetLabel = generateLabelSymbol(cg());
   TR::LabelSymbol *longAcquireRestartLabel = generateLabelSymbol(cg());

   static_assert(IS_32BIT_SIGNED(J9_PUBLIC_FLAGS_VM_ACCESS), "J9_PUBLIC_FLAGS_VM_ACCESS must fit in immediate");
   generateMemImmInstruction(J9_PUBLIC_FLAGS_VM_ACCESS < 128 ? TR::InstOpCode::CMP4MemImms : TR::InstOpCode::CMP4MemImm4,
                             callNode,
                             generateX86MemoryReference(vmThreadReg, fej9->thisThreadGetPublicFlagsOffset(), cg()),
                             J9_PUBLIC_FLAGS_VM_ACCESS,
                             cg());
   generateLabelInstruction(TR::InstOpCode::JNE4, callNode, longAcquireSnippetLabel, cg());
   generateLabelInstruction(TR::InstOpCode::label, callNode, longAcquireRestartLabel, cg());

   TR_OutlinedInstructionsGenerator og(longAcquireSnippetLabel, callNode, cg());
   auto helper = comp()->getSymRefTab()->findOrCreateAcquireVMAccessSymbolRef(comp()->getMethodSymbol());
   generateImmSymInstruction(TR::InstOpCode::CALLImm4, callNode, (uintptr_t)helper->getMethodAddress(), helper, cg());
   generateLabelInstruction(TR::InstOpCode::JMP4, callNode, longAcquireRestartLabel, cg());
   og.endOutlinedInstructionSequence();
   }


#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */

void J9::X86::AMD64::JNILinkage::cleanupReturnValue(
      TR::Node *callNode,
      TR::Register *linkageReturnReg,
      TR::Register *targetReg)
   {
   if (!callNode->getOpCode().isFloatingPoint())
      {
      // Native and JNI methods may not return a full register in some cases so we need to get the declared
      // type so that we sign and zero extend the narrower integer return types properly.
      //
      TR::InstOpCode::Mnemonic op;
      TR::SymbolReference      *callSymRef = callNode->getSymbolReference();
      TR::ResolvedMethodSymbol *callSymbol = callNode->getSymbol()->castToResolvedMethodSymbol();
      TR_ResolvedMethod *resolvedMethod = callSymbol->getResolvedMethod();

      bool isUnsigned = resolvedMethod->returnTypeIsUnsigned();

      switch (resolvedMethod->returnType())
         {
         case TR::Int8:
            if (comp()->getSymRefTab()->isReturnTypeBool(callSymRef))
               {
               // For bool return type, must check whether value returned by
               // JNI is zero (false) or non-zero (true) to yield Java result
               generateRegRegInstruction(TR::InstOpCode::TEST1RegReg, callNode,
                     linkageReturnReg, linkageReturnReg, cg());
               generateRegInstruction(TR::InstOpCode::SETNE1Reg, callNode, linkageReturnReg, cg());
               op = comp()->target().is64Bit() ? TR::InstOpCode::MOVZXReg8Reg1 : TR::InstOpCode::MOVZXReg4Reg1;
               }
            else if (isUnsigned)
               {
               op = comp()->target().is64Bit() ? TR::InstOpCode::MOVZXReg8Reg1 : TR::InstOpCode::MOVZXReg4Reg1;
               }
            else
               {
               op = comp()->target().is64Bit() ? TR::InstOpCode::MOVSXReg8Reg1 : TR::InstOpCode::MOVSXReg4Reg1;
               }
            break;
         case TR::Int16:
            if (isUnsigned)
               {
               op = comp()->target().is64Bit() ? TR::InstOpCode::MOVZXReg8Reg2 : TR::InstOpCode::MOVZXReg4Reg2;
               }
            else
               {
               op = comp()->target().is64Bit() ? TR::InstOpCode::MOVSXReg8Reg2 : TR::InstOpCode::MOVSXReg4Reg2;
               }
            break;
         default:
            // TR::Address, TR_[US]Int64, TR_[US]Int32
            //
            op = (linkageReturnReg != targetReg) ? TR::InstOpCode::MOVRegReg() : TR::InstOpCode::bad;
            break;
         }

      if (op != TR::InstOpCode::bad)
         generateRegRegInstruction(op, callNode, targetReg, linkageReturnReg, cg());
      }
   }


void J9::X86::AMD64::JNILinkage::checkForJNIExceptions(TR::Node *callNode)
   {
   TR::Register *vmThreadReg = cg()->getMethodMetaDataRegister();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());

   // Check exceptions.
   //
   generateMemImmInstruction(TR::InstOpCode::CMPMemImms(), callNode, generateX86MemoryReference(vmThreadReg, fej9->thisThreadGetCurrentExceptionOffset(), cg()), 0, cg());

   TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg());
   TR::Instruction *instr = generateLabelInstruction(TR::InstOpCode::JNE4, callNode, snippetLabel, cg());

   uint32_t gcMap = _systemLinkage->getProperties().getPreservedRegisterMapForGC();
   if (comp()->target().is32Bit())
      {
      gcMap |= (_JNIDispatchInfo.argSize<<14);
      }
   instr->setNeedsGCMap(gcMap);

   TR::Snippet *snippet =
      new (trHeapMemory()) TR::X86CheckFailureSnippet(cg(),
                                     cg()->symRefTab()->findOrCreateRuntimeHelper(TR_throwCurrentException),
                                     snippetLabel,
                                     instr,
                                     _JNIDispatchInfo.requiresFPstackPop);
   cg()->addSnippet(snippet);
   }


void J9::X86::AMD64::JNILinkage::cleanupJNIRefPool(TR::Node *callNode)
   {
   // Must check to see if the ref pool was used and clean them up if so--or we
   // leave a bunch of pinned garbage behind that screws up the gc quality forever.
   //
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   static_assert(IS_32BIT_SIGNED(J9_SSF_JIT_JNI_FRAME_COLLAPSE_BITS), "J9_SSF_JIT_JNI_FRAME_COLLAPSE_BITS must fit in immediate");

   TR::RealRegister *espReal = machine()->getRealRegister(TR::RealRegister::esp);

   TR::LabelSymbol *refPoolSnippetLabel = generateLabelSymbol(cg());
   TR::LabelSymbol *refPoolRestartLabel = generateLabelSymbol(cg());

   generateMemImmInstruction(J9_SSF_JIT_JNI_FRAME_COLLAPSE_BITS <= 255 ? TR::InstOpCode::TEST1MemImm1 : TR::InstOpCode::TESTMemImm4(),
                             callNode,
                             generateX86MemoryReference(espReal, fej9->constJNICallOutFrameFlagsOffset(), cg()),
                             J9_SSF_JIT_JNI_FRAME_COLLAPSE_BITS,
                             cg());

   generateLabelInstruction(TR::InstOpCode::JNE4, callNode, refPoolSnippetLabel, cg());
   generateLabelInstruction(TR::InstOpCode::label, callNode, refPoolRestartLabel, cg());

   TR_OutlinedInstructionsGenerator og(refPoolSnippetLabel, callNode, cg());
   generateHelperCallInstruction(callNode, TR_AMD64jitCollapseJNIReferenceFrame, NULL, cg());
   generateLabelInstruction(TR::InstOpCode::JMP4, callNode, refPoolRestartLabel, cg());
   og.endOutlinedInstructionSequence();
   }


TR::Register *J9::X86::AMD64::JNILinkage::buildDirectDispatch(
      TR::Node *callNode,
      bool spillFPRegs)
   {
   TR::SymbolReference *callSymRef = callNode->getSymbolReference();
   TR::MethodSymbol *callSymbol = callSymRef->getSymbol()->castToMethodSymbol();

   bool isGPUHelper = callSymbol->isHelper() && (callSymRef->getReferenceNumber() == TR_estimateGPU ||
                                                 callSymRef->getReferenceNumber() == TR_getStateGPU ||
                                                 callSymRef->getReferenceNumber() == TR_regionEntryGPU ||
                                                 callSymRef->getReferenceNumber() == TR_allocateGPUKernelParms ||
                                                 callSymRef->getReferenceNumber() == TR_copyToGPU ||
                                                 callSymRef->getReferenceNumber() == TR_launchGPUKernel ||
                                                 callSymRef->getReferenceNumber() == TR_copyFromGPU ||
                                                 callSymRef->getReferenceNumber() == TR_invalidateGPU ||
                                                 callSymRef->getReferenceNumber() == TR_flushGPU ||
                                                 callSymRef->getReferenceNumber() == TR_regionExitGPU);
   if (callSymbol->isJNI() || isGPUHelper)
      {
      return buildDirectJNIDispatch(callNode);
      }

   TR_ASSERT(false, "call through TR::AMD64SystemLinkage::buildDirectDispatch instead.\n");
   return NULL;
   }


void
J9::X86::AMD64::JNILinkage::populateJNIDispatchInfo()
   {
   _JNIDispatchInfo.numJNIFrameSlotsPushed = 5;

   _JNIDispatchInfo.JNIReturnRegister = NULL;
   _JNIDispatchInfo.linkageReturnRegister = NULL;

   _JNIDispatchInfo.callPreDeps = NULL;
   _JNIDispatchInfo.callPostDeps = NULL;
   _JNIDispatchInfo.mergeLabelPostDeps = NULL;
   _JNIDispatchInfo.dispatchTrampolineRegister = NULL;

   _JNIDispatchInfo.argSize = 0;

   _JNIDispatchInfo.requiresFPstackPop = false;

   _JNIDispatchInfo.dedicatedFrameRegisterIndex = _systemLinkage->getProperties().getIntegerScratchRegister(0);
   }


TR::Register *J9::X86::AMD64::JNILinkage::buildDirectJNIDispatch(TR::Node *callNode)
   {
#ifdef DEBUG
   if (debug("reportJNI"))
      {
      printf("AMD64 JNI Dispatch: %s calling %s\n", comp()->signature(), comp()->getDebug()->getName(callNode->getSymbolReference()));
      }
#endif
   TR::SymbolReference      *callSymRef = callNode->getSymbolReference();
   TR::MethodSymbol         *callSymbol = callSymRef->getSymbol()->castToMethodSymbol();

   bool isGPUHelper = callSymbol->isHelper() && (callSymRef->getReferenceNumber() == TR_estimateGPU ||
                                                 callSymRef->getReferenceNumber() == TR_getStateGPU ||
                                                 callSymRef->getReferenceNumber() == TR_regionEntryGPU ||
                                                 callSymRef->getReferenceNumber() == TR_allocateGPUKernelParms ||
                                                 callSymRef->getReferenceNumber() == TR_copyToGPU ||
                                                 callSymRef->getReferenceNumber() == TR_launchGPUKernel ||
                                                 callSymRef->getReferenceNumber() == TR_copyFromGPU ||
                                                 callSymRef->getReferenceNumber() == TR_invalidateGPU ||
                                                 callSymRef->getReferenceNumber() == TR_flushGPU ||
                                                 callSymRef->getReferenceNumber() == TR_regionExitGPU);

   static bool keepVMDuringGPUHelper = feGetEnv("TR_KeepVMDuringGPUHelper") ? true : false;

   TR::Register *vmThreadReg = cg()->getMethodMetaDataRegister();
   TR::RealRegister *espReal = machine()->getRealRegister(TR::RealRegister::esp);

   TR::ResolvedMethodSymbol *resolvedMethodSymbol;
   TR_ResolvedMethod       *resolvedMethod;
   TR::SymbolReference      *gpuHelperSymRef;
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());

   bool dropVMAccess;
   bool isJNIGCPoint;
   bool killNonVolatileGPRs;
   bool checkExceptions;
   bool createJNIFrame;
   bool tearDownJNIFrame;
   bool wrapRefs;
   bool passReceiver;
   bool passThread;

   if (!isGPUHelper)
      {
      resolvedMethodSymbol = callNode->getSymbol()->castToResolvedMethodSymbol();
      resolvedMethod       = resolvedMethodSymbol->getResolvedMethod();
      dropVMAccess         = !fej9->jniRetainVMAccess(resolvedMethod);
      isJNIGCPoint         = !fej9->jniNoGCPoint(resolvedMethod);
      killNonVolatileGPRs  = isJNIGCPoint;
      checkExceptions      = !fej9->jniNoExceptionsThrown(resolvedMethod);
      createJNIFrame       = !fej9->jniNoNativeMethodFrame(resolvedMethod);
      tearDownJNIFrame     = !fej9->jniNoSpecialTeardown(resolvedMethod);
      wrapRefs             = !fej9->jniDoNotWrapObjects(resolvedMethod);
      passReceiver         = !fej9->jniDoNotPassReceiver(resolvedMethod);
      passThread           = !fej9->jniDoNotPassThread(resolvedMethod);
      }
   else
      {
      gpuHelperSymRef      = comp()->getSymRefTab()->methodSymRefFromName(comp()->getMethodSymbol(), "com/ibm/jit/JITHelpers", "GPUHelper", "()V", TR::MethodSymbol::Static);
      resolvedMethodSymbol = gpuHelperSymRef->getSymbol()->castToResolvedMethodSymbol();
      resolvedMethod       = resolvedMethodSymbol->getResolvedMethod();

      if (keepVMDuringGPUHelper || (callSymRef->getReferenceNumber() == TR_copyToGPU || callSymRef->getReferenceNumber() == TR_copyFromGPU || callSymRef->getReferenceNumber() == TR_flushGPU || callSymRef->getReferenceNumber() == TR_regionExitGPU || callSymRef->getReferenceNumber() == TR_estimateGPU))
         dropVMAccess = false; //TR_copyToGPU, TR_copyFromGPU, TR_regionExitGPU (and all others if keepVMDuringGPUHelper is true)
      else
         dropVMAccess = true; //TR_regionEntryGPU, TR_launchGPUKernel, TR_estimateGPU, TR_allocateGPUKernelParms, ... (only if keepVMDuringGPUHelper is false)

      isJNIGCPoint        = true;
      killNonVolatileGPRs = isJNIGCPoint;
      checkExceptions     = false;
      createJNIFrame      = true;
      tearDownJNIFrame    = true;
      wrapRefs            = false; //unused for this code path
      passReceiver        = true;
      passThread          = false;
      }

   populateJNIDispatchInfo();

   static char * disablePureFn = feGetEnv("TR_DISABLE_PURE_FUNC_RECOGNITION");
   if (!isGPUHelper)
      {
      if (resolvedMethodSymbol->canDirectNativeCall())
         {
         dropVMAccess = false;
         killNonVolatileGPRs = false;
         isJNIGCPoint = false;
         checkExceptions = false;
         createJNIFrame = false;
         tearDownJNIFrame = false;
         }
      else if (callNode->getSymbol()->castToResolvedMethodSymbol()->isPureFunction() && (disablePureFn == NULL))
         {
         dropVMAccess = false;
         isJNIGCPoint = false;
         checkExceptions = false;
         }
      }

   // Anchor the Java frame here to establish the top of the frame.  The reason this must occur here
   // is because the subsequent manual adjustments of the stack pointer confuse the vfp logic.
   // This should be fixed in a subsquent revision of that code.
   //
   TR::X86VFPDedicateInstruction *vfpDedicateInstruction =
      generateVFPDedicateInstruction(machine()->getRealRegister(_JNIDispatchInfo.dedicatedFrameRegisterIndex), callNode, cg());

   // First, build a JNI callout frame on the Java stack.
   //
   TR::LabelSymbol *returnAddrLabel = generateLabelSymbol(cg());
   if (createJNIFrame)
      {
      if (isGPUHelper)
         callNode->setSymbolReference(gpuHelperSymRef);

      buildJNICallOutFrame(callNode, returnAddrLabel);

      if (isGPUHelper)
         callNode->setSymbolReference(callSymRef); //change back to callSymRef afterwards

#if JAVA_SPEC_VERSION >= 19
      /**
       * For virtual threads, bump the callOutCounter.  It is safe and most efficient to
       * do this unconditionally.  No need to check for overflow.
       */
      generateMemInstruction(
         TR::InstOpCode::INC8Mem,
         callNode,
         generateX86MemoryReference(vmThreadReg, fej9->thisThreadGetCallOutCountOffset(), cg()),
         cg());
#endif
      }

   // Switch from the Java stack to the C stack:
   TR::J9LinkageUtils::switchToMachineCStack(callNode, cg());

   // Preserve the VMThread pointer on the C stack.
   // Adjust the argSize to include the just pushed VMThread pointer.
   //
   generateRegInstruction(TR::InstOpCode::PUSHReg, callNode, vmThreadReg, cg());
   if (passThread || isGPUHelper)
      {
      _JNIDispatchInfo.argSize = TR::Compiler->om.sizeofReferenceAddress();
      }

   TR::LabelSymbol *startJNISequence = generateLabelSymbol(cg());
   startJNISequence->setStartInternalControlFlow();
   generateLabelInstruction(TR::InstOpCode::label, callNode, startJNISequence, cg());

   if (isGPUHelper)
      callNode->setSymbolReference(gpuHelperSymRef);

   buildOutgoingJNIArgsAndDependencies(callNode, passThread, passReceiver, killNonVolatileGPRs);

   if (isGPUHelper)
      callNode->setSymbolReference(callSymRef); //change back to callSymRef afterwards

   if (dropVMAccess)
      {
#ifdef J9VM_INTERP_ATOMIC_FREE_JNI
      releaseVMAccessAtomicFree(callNode);
#else
      releaseVMAccess(callNode);
#endif
      }

   uintptr_t targetAddress;

   if (isGPUHelper)
      {
      callNode->setSymbolReference(gpuHelperSymRef);
      targetAddress = (uintptr_t)callSymbol->getMethodAddress();
      }
   else
      {
      TR::ResolvedMethodSymbol *callSymbol1  = callNode->getSymbol()->castToResolvedMethodSymbol();
      targetAddress = (uintptr_t)callSymbol1->getResolvedMethod()->startAddressForJNIMethod(comp());
      }

  TR::Instruction *callInstr = generateMethodDispatch(callNode, isJNIGCPoint, targetAddress);

  if (isGPUHelper)
      callNode->setSymbolReference(callSymRef); //change back to callSymRef afterwards

   // TODO: will need an AOT relocation for this one at some point.
   // Lay down a label for the frame push to reference.
   //
   generateLabelInstruction(callInstr, TR::InstOpCode::label, returnAddrLabel, cg());

   if (_JNIDispatchInfo.JNIReturnRegister)
      {
      if (isGPUHelper)
         callNode->setSymbolReference(gpuHelperSymRef);

      cleanupReturnValue(callNode, _JNIDispatchInfo.linkageReturnRegister, _JNIDispatchInfo.JNIReturnRegister);

      if (isGPUHelper)
         callNode->setSymbolReference(callSymRef); //change back to callSymRef afterwards

      if (_JNIDispatchInfo.linkageReturnRegister != _JNIDispatchInfo.JNIReturnRegister)
         cg()->stopUsingRegister(_JNIDispatchInfo.linkageReturnRegister);
      }

   // Restore the VMThread back from the C stack.
   //
   generateRegInstruction(TR::InstOpCode::POPReg, callNode, vmThreadReg, cg());

   if (dropVMAccess)
      {
#ifdef J9VM_INTERP_ATOMIC_FREE_JNI
      acquireVMAccessAtomicFree(callNode);
#else
      acquireVMAccess(callNode);
#endif
      }

   if (resolvedMethod->returnType() == TR::Address && wrapRefs)
      {
      // Unless NULL, need to indirect once to get the real Java reference.
      //
      // This MUST be done AFTER VM access has been re-acquired!
      //
      TR::Register *targetReg = _JNIDispatchInfo.JNIReturnRegister;
      TR::LabelSymbol *nullLabel = generateLabelSymbol(cg());
      generateRegRegInstruction(TR::InstOpCode::TESTRegReg(), callNode, targetReg, targetReg, cg());
      generateLabelInstruction(TR::InstOpCode::JE4, callNode, nullLabel, cg());

      generateRegMemInstruction(
         TR::InstOpCode::LRegMem(),
         callNode,
         targetReg,
         generateX86MemoryReference(targetReg, 0, cg()),
         cg());

      generateLabelInstruction(TR::InstOpCode::label, callNode, nullLabel, cg());
      }

   //    1) Store out the machine sp into the vm thread.  It has to be done as sometimes
   //       it gets tromped on by call backs.
   generateMemRegInstruction(
      TR::InstOpCode::SMemReg(),
      callNode,
      generateX86MemoryReference(vmThreadReg, fej9->thisThreadGetMachineSPOffset(), cg()),
      espReal,
      cg());

   TR::J9LinkageUtils::switchToJavaStack(callNode, cg());

   if (createJNIFrame)
      {
#if JAVA_SPEC_VERSION >= 19
      /**
       * For virtual threads, decrement the callOutCounter.  It is safe and most efficient to
       * do this unconditionally.  No need to check for underflow.
       */
      generateMemInstruction(
         TR::InstOpCode::DEC8Mem,
         callNode,
         generateX86MemoryReference(vmThreadReg, fej9->thisThreadGetCallOutCountOffset(), cg()),
         cg());
#endif

      generateRegMemInstruction(
            TR::InstOpCode::ADDRegMem(),
            callNode,
            espReal,
            generateX86MemoryReference(vmThreadReg, fej9->thisThreadGetJavaLiteralsOffset(), cg()),
            cg());
      }

   if (createJNIFrame && tearDownJNIFrame)
      {
      cleanupJNIRefPool(callNode);
      }

   // Clean up JNI callout frame.
   //
   if (createJNIFrame)
      {
      TR::X86RegImmInstruction *instr = generateRegImmInstruction(
            TR::InstOpCode::ADDRegImms(),
            callNode,
            espReal,
            _JNIDispatchInfo.numJNIFrameSlotsPushed * TR::Compiler->om.sizeofReferenceAddress(),
            cg());
      }

   if (checkExceptions)
      {
      checkForJNIExceptions(callNode);
      }

   generateVFPReleaseInstruction(vfpDedicateInstruction, callNode, cg());

   TR::LabelSymbol *restartLabel = generateLabelSymbol(cg());
   restartLabel->setEndInternalControlFlow();
   generateLabelInstruction(TR::InstOpCode::label, callNode, restartLabel, _JNIDispatchInfo.mergeLabelPostDeps, cg());

   return _JNIDispatchInfo.JNIReturnRegister;
   }


#endif
