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

#include "codegen/ARM64JNILinkage.hpp"

#include <algorithm>
#include "codegen/ARM64HelperCallSnippet.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/Relocation.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/VMJ9.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/StaticSymbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"

J9::ARM64::JNILinkage::JNILinkage(TR::CodeGenerator *cg)
   : J9::ARM64::PrivateLinkage(cg)
   {
   _systemLinkage = cg->getLinkage(TR_System);
   }

int32_t J9::ARM64::JNILinkage::buildArgs(TR::Node *callNode,
   TR::RegisterDependencyConditions *dependencies)
   {
   TR_ASSERT(0, "Should call J9::ARM64::JNILinkage::buildJNIArgs instead.");
   return 0;
   }

TR::Register *J9::ARM64::JNILinkage::buildIndirectDispatch(TR::Node *callNode)
   {
   TR_ASSERT(0, "Calling J9::ARM64::JNILinkage::buildIndirectDispatch does not make sense.");
   return NULL;
   }

void J9::ARM64::JNILinkage::buildVirtualDispatch(
      TR::Node *callNode,
      TR::RegisterDependencyConditions *dependencies,
      uint32_t argSize)
   {
   TR_ASSERT(0, "Calling J9::ARM64::JNILinkage::buildVirtualDispatch does not make sense.");
   }

void J9::ARM64::JNILinkage::releaseVMAccess(TR::Node *callNode, TR::Register *vmThreadReg, TR::Register *scratchReg0, TR::Register *scratchReg1, TR::Register *scratchReg2, TR::Register *scratchReg3)
   {
   TR_J9VMBase *fej9 = reinterpret_cast<TR_J9VMBase *>(fe());

   // Release VM access (spin lock)
   //
   //    addimmx scratch0, vmThreadReg, #publicFlagsOffset
   //    movzx   scratch1, constReleaseVMAccessOutOfLineMask
   //
   //    dmb ishst
   // loopHead:
   //    ldxrx   scratch2, [scratch0]
   //    tst     scratch2, scratch1
   //    b.ne    releaseVMAccessSnippet
   //    andimmx scratch2, scratch2, constReleaseVMAccessMask
   //    stxrx   scratch2, scratch2, [scratch0]
   //    cbnz    scratch2, loopHead
   // releaseVMRestart:
   //

   const int releaseVMAccessMask = fej9->constReleaseVMAccessMask();

   generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::addimmx, callNode, scratchReg0, vmThreadReg, fej9->thisThreadGetPublicFlagsOffset());
   loadConstant64(cg(), callNode, fej9->constReleaseVMAccessOutOfLineMask(), scratchReg1);
   // dmb ishst (Inner Shareable store barrier)
   // Arm Architecture Reference Manual states:
   // "This architecture assumes that all PEs that use the same operating system or hypervisor are in the same Inner Shareable shareability domain"
   // thus, inner shareable dmb suffices
   generateSynchronizationInstruction(cg(), TR::InstOpCode::dmb, callNode, 0xb);

   TR::LabelSymbol *loopHead = generateLabelSymbol(cg());
   generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, loopHead);
   generateTrg1MemInstruction(cg(), TR::InstOpCode::ldxrx, callNode, scratchReg2, new (trHeapMemory()) TR::MemoryReference(scratchReg0, 0, cg()));
   generateTestInstruction(cg(), callNode, scratchReg2, scratchReg1, true);

   TR::LabelSymbol *releaseVMAccessSnippetLabel = generateLabelSymbol(cg());
   TR::LabelSymbol *releaseVMAccessRestartLabel = generateLabelSymbol(cg());
   TR::SymbolReference *relVMSymRef = comp()->getSymRefTab()->findOrCreateReleaseVMAccessSymbolRef(comp()->getJittedMethodSymbol());

   TR::Snippet *releaseVMAccessSnippet = new (trHeapMemory()) TR::ARM64HelperCallSnippet(cg(), callNode, releaseVMAccessSnippetLabel, relVMSymRef, releaseVMAccessRestartLabel);
   cg()->addSnippet(releaseVMAccessSnippet);
   generateConditionalBranchInstruction(cg(), TR::InstOpCode::b_cond, callNode, releaseVMAccessSnippetLabel, TR::CC_NE);
   releaseVMAccessSnippet->gcMap().setGCRegisterMask(0);

   uint64_t mask = fej9->constReleaseVMAccessMask();
   bool n;
   uint32_t imm;
   if (logicImmediateHelper(mask, true, n, imm))
      {
      generateLogicalImmInstruction(cg(), TR::InstOpCode::andimmx, callNode, scratchReg2, scratchReg2, n, imm);
      }
   else
      {
      loadConstant64(cg(), callNode, mask, scratchReg3);
      generateTrg1Src2Instruction(cg(), TR::InstOpCode::andx, callNode, scratchReg2, scratchReg2, scratchReg3);
      }
   generateTrg1MemSrc1Instruction(cg(), TR::InstOpCode::stxrx, callNode, scratchReg2, new (trHeapMemory()) TR::MemoryReference(scratchReg0, 0, cg()), scratchReg2);
   generateCompareBranchInstruction(cg(), TR::InstOpCode::cbnzx, callNode, scratchReg2, loopHead);
   generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, releaseVMAccessRestartLabel);
   }

void J9::ARM64::JNILinkage::acquireVMAccess(TR::Node *callNode, TR::Register *vmThreadReg, TR::Register *scratchReg0, TR::Register *scratchReg1, TR::Register *scratchReg2)
   {
   TR_J9VMBase *fej9 = reinterpret_cast<TR_J9VMBase *>(fe());

   // Re-acquire VM access.
   //    addimmx scratch0, vmThreadReg, #publicFlagsOffset
   //    movzx   scratch1, constAcquireMAccessOutOfLineMask
   //
   // loopHead:
   //    ldxrx   scratch2, [scratch0]
   //    cbnzx   scratch2, reacquireVMAccessSnippet
   //    stxrx   scratch2, scratch1, [scratch0]
   //    cbnz    scratch2, loopHead
   //    dmb ishld
   // reacquireVMAccessRestartLabel:
   //
   generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::addimmx, callNode, scratchReg0, vmThreadReg, fej9->thisThreadGetPublicFlagsOffset());
   loadConstant64(cg(), callNode, fej9->constAcquireVMAccessOutOfLineMask(), scratchReg1);

   TR::LabelSymbol *loopHead = generateLabelSymbol(cg());
   generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, loopHead);
   generateTrg1MemInstruction(cg(), TR::InstOpCode::ldxrx, callNode, scratchReg2, new (trHeapMemory()) TR::MemoryReference(scratchReg0, 0, cg()));

   TR::LabelSymbol *reacquireVMAccessSnippetLabel = generateLabelSymbol(cg());
   TR::LabelSymbol *reacquireVMAccessRestartLabel = generateLabelSymbol(cg());
   TR::SymbolReference *acqVMSymRef = comp()->getSymRefTab()->findOrCreateAcquireVMAccessSymbolRef(comp()->getJittedMethodSymbol());

   TR::Snippet *reacquireVMAccessSnippet = new (trHeapMemory()) TR::ARM64HelperCallSnippet(cg(), callNode, reacquireVMAccessSnippetLabel, acqVMSymRef, reacquireVMAccessRestartLabel);
   cg()->addSnippet(reacquireVMAccessSnippet);
   generateCompareBranchInstruction(cg(), TR::InstOpCode::cbnzx, callNode, scratchReg2, reacquireVMAccessSnippetLabel);
   reacquireVMAccessSnippet->gcMap().setGCRegisterMask(0);

   generateTrg1MemSrc1Instruction(cg(), TR::InstOpCode::stxrx, callNode, scratchReg2, new (trHeapMemory()) TR::MemoryReference(scratchReg0, 0, cg()), scratchReg1);
   generateCompareBranchInstruction(cg(), TR::InstOpCode::cbnzx, callNode, scratchReg2, loopHead);
   // dmb ishld (Inner Shareable load barrier)
   generateSynchronizationInstruction(cg(), TR::InstOpCode::dmb, callNode, 0x9);

   generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, reacquireVMAccessRestartLabel);
   }

#ifdef J9VM_INTERP_ATOMIC_FREE_JNI
void J9::ARM64::JNILinkage::releaseVMAccessAtomicFree(TR::Node *callNode, TR::Register *vmThreadReg)
   {
   TR_UNIMPLEMENTED();
   }

void J9::ARM64::JNILinkage::acquireVMAccessAtomicFree(TR::Node *callNode, TR::Register *vmThreadReg)
   {
   TR_UNIMPLEMENTED();
   }
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */

void J9::ARM64::JNILinkage::buildJNICallOutFrame(TR::Node *callNode, bool isWrapperForJNI, TR::LabelSymbol *returnAddrLabel,
                                                 TR::Register *vmThreadReg, TR::Register *javaStackReg, TR::Register *scratchReg0, TR::Register *scratchReg1)
   {
   TR_J9VMBase *fej9 = reinterpret_cast<TR_J9VMBase *>(fe());

   // begin: mask out the magic bit that indicates JIT frames below
   loadConstant64(cg(), callNode, 0, scratchReg1);
   generateMemSrc1Instruction(cg(), TR::InstOpCode::strimmx, callNode, new (trHeapMemory()) TR::MemoryReference(vmThreadReg, fej9->thisThreadGetJavaFrameFlagsOffset(), cg()), scratchReg1);

   // Grab 5 slots in the frame.
   //
   //    4:   tag bits (savedA0)
   //    3:   empty (savedPC)
   //    2:   return address in this frame (savedCP)
   //    1:   frame flags
   //    0:   RAM method
   //
   // push tag bits (savedA0)
   int32_t tagBits = fej9->constJNICallOutFrameSpecialTag();
   // if the current method is simply a wrapper for the JNI call, hide the call-out stack frame
   if (isWrapperForJNI)
      tagBits |= fej9->constJNICallOutFrameInvisibleTag();
   loadConstant64(cg(), callNode, tagBits, scratchReg0);

   generateMemSrc1Instruction(cg(), TR::InstOpCode::strprex, callNode, new (trHeapMemory()) TR::MemoryReference(javaStackReg, -TR::Compiler->om.sizeofReferenceAddress(), cg()), scratchReg0);

   // empty saved pc slot
   generateMemSrc1Instruction(cg(), TR::InstOpCode::strprex, callNode, new (trHeapMemory()) TR::MemoryReference(javaStackReg, -TR::Compiler->om.sizeofReferenceAddress(), cg()), scratchReg1);

   // push return address (savedCP)
   generateTrg1ImmSymInstruction(cg(), TR::InstOpCode::adr, callNode, scratchReg0, 0, returnAddrLabel);
   generateMemSrc1Instruction(cg(), TR::InstOpCode::strprex, callNode, new (trHeapMemory()) TR::MemoryReference(javaStackReg, -TR::Compiler->om.sizeofReferenceAddress(), cg()), scratchReg0);

   // push flags
   loadConstant64(cg(), callNode, fej9->constJNICallOutFrameFlags(), scratchReg0);
   generateMemSrc1Instruction(cg(), TR::InstOpCode::strprex, callNode, new (trHeapMemory()) TR::MemoryReference(javaStackReg, -TR::Compiler->om.sizeofReferenceAddress(), cg()), scratchReg0);

   TR::ResolvedMethodSymbol *resolvedMethodSymbol = callNode->getSymbol()->castToResolvedMethodSymbol();
   TR_ResolvedMethod *resolvedMethod = resolvedMethodSymbol->getResolvedMethod();
   uintptr_t methodAddr = reinterpret_cast<uintptr_t>(resolvedMethod->resolvedMethodAddress());

   // push the RAM method for the native
   if (fej9->needClassAndMethodPointerRelocations())
      {
      // load a 64-bit constant into a register with a fixed 4 instruction sequence
      TR::Instruction *firstInstruction;

      TR::Instruction *cursor = firstInstruction = generateTrg1ImmInstruction(cg(), TR::InstOpCode::movzx, callNode, scratchReg0, methodAddr & 0x0000ffff, cursor);
      cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::movkx, callNode, scratchReg0, ((methodAddr >> 16) & 0x0000ffff) | TR::MOV_LSL16, cursor);
      cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::movkx, callNode, scratchReg0, ((methodAddr >> 32) & 0x0000ffff) | TR::MOV_LSL32 , cursor);
      cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::movkx, callNode, scratchReg0, (methodAddr >> 48) | TR::MOV_LSL48 , cursor);

      TR_ExternalRelocationTargetKind reloType;
      if (resolvedMethodSymbol->isSpecial())
         reloType = TR_SpecialRamMethodConst;
      else if (resolvedMethodSymbol->isStatic())
         reloType = TR_StaticRamMethodConst;
      else if (resolvedMethodSymbol->isVirtual())
         reloType = TR_VirtualRamMethodConst;
      else
         {
         reloType = TR_NoRelocation;
         TR_ASSERT(0, "JNI relocation not supported.");
         }
      cg()->addExternalRelocation(new (trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
                                                            firstInstruction,
                                                            reinterpret_cast<uint8_t *>(callNode->getSymbolReference()),
                                                            reinterpret_cast<uint8_t *>(callNode->getInlinedSiteIndex()),
                                                            reloType, cg()),
                                                          __FILE__,__LINE__, callNode);

      }
   else
      {
      loadConstant64(cg(), callNode, methodAddr, scratchReg0);
      }
   generateMemSrc1Instruction(cg(), TR::InstOpCode::strprex, callNode, new (trHeapMemory()) TR::MemoryReference(javaStackReg, -TR::Compiler->om.sizeofReferenceAddress(), cg()), scratchReg0);

   // store out java sp
   generateMemSrc1Instruction(cg(), TR::InstOpCode::strimmx, callNode, new (trHeapMemory()) TR::MemoryReference(vmThreadReg, fej9->thisThreadGetJavaSPOffset(), cg()), javaStackReg);

   // store out pc and literals values indicating the callout frame
   loadConstant64(cg(), callNode, fej9->constJNICallOutFrameType(), scratchReg0);
   generateMemSrc1Instruction(cg(), TR::InstOpCode::strimmx, callNode, new (trHeapMemory()) TR::MemoryReference(vmThreadReg, fej9->thisThreadGetJavaPCOffset(), cg()), scratchReg0);

   generateMemSrc1Instruction(cg(), TR::InstOpCode::strimmx, callNode, new (trHeapMemory()) TR::MemoryReference(vmThreadReg, fej9->thisThreadGetJavaLiteralsOffset(), cg()), scratchReg1);

   }

void J9::ARM64::JNILinkage::restoreJNICallOutFrame(TR::Node *callNode, bool tearDownJNIFrame, TR::Register *vmThreadReg, TR::Register *javaStackReg, TR::Register *scratchReg)
   {
   TR_J9VMBase *fej9 = reinterpret_cast<TR_J9VMBase *>(fe());

   // restore stack pointer: need to deal with growable stack -- stack may already be moved.
   generateTrg1MemInstruction(cg(), TR::InstOpCode::ldrimmx, callNode, scratchReg, new (trHeapMemory()) TR::MemoryReference(vmThreadReg, fej9->thisThreadGetJavaLiteralsOffset(), cg()));
   generateTrg1MemInstruction(cg(),TR::InstOpCode::ldrimmx, callNode, javaStackReg, new (trHeapMemory()) TR::MemoryReference(vmThreadReg, fej9->thisThreadGetJavaSPOffset(), cg()));
   generateTrg1Src2Instruction(cg(), TR::InstOpCode::addx, callNode, javaStackReg, scratchReg, javaStackReg);

   if (tearDownJNIFrame)
      {
      // must check to see if the ref pool was used and clean them up if so--or we
      // leave a bunch of pinned garbage behind that screws up the gc quality forever
      TR::LabelSymbol      *refPoolRestartLabel = generateLabelSymbol(cg());
      TR::LabelSymbol      *refPoolSnippetLabel = generateLabelSymbol(cg());
      TR::SymbolReference *collapseSymRef = cg()->getSymRefTab()->findOrCreateRuntimeHelper(TR_ARM64jitCollapseJNIReferenceFrame, false, false, false);
      TR::Snippet *snippet = new (trHeapMemory()) TR::ARM64HelperCallSnippet(cg(), callNode, refPoolSnippetLabel, collapseSymRef, refPoolRestartLabel);
      cg()->addSnippet(snippet);
      generateTrg1MemInstruction(cg(), TR::InstOpCode::ldrimmx, callNode, scratchReg, new (trHeapMemory()) TR::MemoryReference(javaStackReg, fej9->constJNICallOutFrameFlagsOffset(), cg()));
      generateTestImmInstruction(cg(), callNode, scratchReg, fej9->constJNIReferenceFrameAllocatedFlags(), true);
      generateConditionalBranchInstruction(cg(), TR::InstOpCode::b_cond, callNode, refPoolSnippetLabel, TR::CC_NE);
      generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, refPoolRestartLabel);
      }

   // Restore the JIT frame
   generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::addimmx, callNode, javaStackReg, javaStackReg, 5*TR::Compiler->om.sizeofReferenceAddress());
   }


size_t J9::ARM64::JNILinkage::buildJNIArgs(TR::Node *callNode, TR::RegisterDependencyConditions *dependencies, bool passThread, bool passReceiver, bool killNonVolatileGPRs)
   {
   const TR::ARM64LinkageProperties &properties = _systemLinkage->getProperties();
   TR::ARM64MemoryArgument *pushToMemory = NULL;
   TR::Register *argMemReg;
   TR::Register *tempReg;
   int32_t argIndex = 0;
   int32_t numMemArgs = 0;
   int32_t argSize = 0;
   int32_t numIntegerArgs = 0;
   int32_t numFloatArgs = 0;
   int32_t totalSize;
   int32_t i;

   TR::Node *child;
   TR::DataType childType;
   TR::DataType resType = callNode->getType();

   uint32_t firstArgumentChild = callNode->getFirstArgumentIndex();
   if (passThread)
      {
      numIntegerArgs += 1;
      }
   // if fastJNI do not pass the receiver just evaluate the first child
   if (!passReceiver)
      {
      TR::Node *firstArgChild = callNode->getChild(firstArgumentChild);
      cg()->evaluate(firstArgChild);
      cg()->decReferenceCount(firstArgChild);
      firstArgumentChild += 1;
      }
   /* Step 1 - figure out how many arguments are going to be spilled to memory i.e. not in registers */
   for (i = firstArgumentChild; i < callNode->getNumChildren(); i++)
      {
      child = callNode->getChild(i);
      childType = child->getDataType();

      switch (childType)
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Int64:
         case TR::Address:
            if (numIntegerArgs >= properties.getNumIntArgRegs())
               numMemArgs++;
            numIntegerArgs++;
            break;

         case TR::Float:
         case TR::Double:
            if (numFloatArgs >= properties.getNumFloatArgRegs())
                  numMemArgs++;
            numFloatArgs++;
            break;

         default:
            TR_ASSERT(false, "Argument type %s is not supported\n", childType.toString());
         }
      }

   // From here, down, any new stack allocations will expire / die when the function returns
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());
   /* End result of Step 1 - determined number of memory arguments! */
   if (numMemArgs > 0)
      {
      pushToMemory = new (trStackMemory()) TR::ARM64MemoryArgument[numMemArgs];

      argMemReg = cg()->allocateRegister();
      }

   totalSize = numMemArgs * 8;
   // align to 16-byte boundary
   totalSize = (totalSize + 15) & (~15);

   numIntegerArgs = 0;
   numFloatArgs = 0;

   if (passThread)
      {
      // first argument is JNIenv
      numIntegerArgs += 1;
      }

   for (i = firstArgumentChild; i < callNode->getNumChildren(); i++)
      {
      TR::MemoryReference *mref = NULL;
      TR::Register *argRegister;
      TR::InstOpCode::Mnemonic op;
      bool           checkSplit = true;

      child = callNode->getChild(i);
      childType = child->getDataType();

      switch (childType)
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Int64:
         case TR::Address:
            if (childType == TR::Address)
               {
               argRegister = pushJNIReferenceArg(child);
               checkSplit = false;
               }
            else if (childType == TR::Int64)
               argRegister = pushLongArg(child);
            else
               argRegister = pushIntegerWordArg(child);

            if (numIntegerArgs < properties.getNumIntArgRegs())
               {
               if (checkSplit && !cg()->canClobberNodesRegister(child, 0))
                  {
                  if (argRegister->containsCollectedReference())
                     tempReg = cg()->allocateCollectedReferenceRegister();
                  else
                     tempReg = cg()->allocateRegister();
                  generateMovInstruction(cg(), callNode, tempReg, argRegister);
                  argRegister = tempReg;
                  }
               if (numIntegerArgs == 0 &&
                  (resType.isAddress() || resType.isInt32() || resType.isInt64()))
                  {
                  TR::Register *resultReg;
                  if (resType.isAddress())
                     resultReg = cg()->allocateCollectedReferenceRegister();
                  else
                     resultReg = cg()->allocateRegister();

                  dependencies->addPreCondition(argRegister, TR::RealRegister::x0);
                  dependencies->addPostCondition(resultReg, TR::RealRegister::x0);
                  }
               else
                  {
                  TR::addDependency(dependencies, argRegister, properties.getIntegerArgumentRegister(numIntegerArgs), TR_GPR, cg());
                  }
               }
            else
               {
               // numIntegerArgs >= properties.getNumIntArgRegs()
               if (childType == TR::Address || childType == TR::Int64)
                  {
                  op = TR::InstOpCode::strpostx;
                  }
               else
                  {
                  op = TR::InstOpCode::strpostw;
                  }
               mref = getOutgoingArgumentMemRef(argMemReg, argRegister, op, pushToMemory[argIndex++]);
               argSize += 8; // always 8-byte aligned
               }
            numIntegerArgs++;
            break;

         case TR::Float:
         case TR::Double:
            if (childType == TR::Float)
               argRegister = pushFloatArg(child);
            else
               argRegister = pushDoubleArg(child);

            if (numFloatArgs < properties.getNumFloatArgRegs())
               {
               if (!cg()->canClobberNodesRegister(child, 0))
                  {
                  tempReg = cg()->allocateRegister(TR_FPR);
                  op = (childType == TR::Float) ? TR::InstOpCode::fmovs : TR::InstOpCode::fmovd;
                  generateTrg1Src1Instruction(cg(), op, callNode, tempReg, argRegister);
                  argRegister = tempReg;
                  }
               if ((numFloatArgs == 0 && resType.isFloatingPoint()))
                  {
                  TR::Register *resultReg;
                  if (resType.getDataType() == TR::Float)
                     resultReg = cg()->allocateSinglePrecisionRegister();
                  else
                     resultReg = cg()->allocateRegister(TR_FPR);

                  dependencies->addPreCondition(argRegister, TR::RealRegister::v0);
                  dependencies->addPostCondition(resultReg, TR::RealRegister::v0);
                  }
               else
                  {
                  TR::addDependency(dependencies, argRegister, properties.getFloatArgumentRegister(numFloatArgs), TR_FPR, cg());
                  }
               }
            else
               {
               // numFloatArgs >= properties.getNumFloatArgRegs()
               if (childType == TR::Double)
                  {
                  op = TR::InstOpCode::vstrpostd;
                  }
               else
                  {
                  op = TR::InstOpCode::vstrposts;
                  }
               mref = getOutgoingArgumentMemRef(argMemReg, argRegister, op, pushToMemory[argIndex++]);
               argSize += 8; // always 8-byte aligned
               }
            numFloatArgs++;
            break;
         } // end of switch
      } // end of for

   for (int32_t i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastGPR; ++i)
      {
      TR::RealRegister::RegNum realReg = (TR::RealRegister::RegNum)i;
      if (getProperties().getRegisterFlags(realReg) & ARM64_Reserved)
         {
         continue;
         }
      if (properties.getPreserved(realReg))
         {
         if (killNonVolatileGPRs)
            {
            // We release VM access around the native call,
            // so even though the callee's linkage won't alter a
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
            TR::addDependency(dependencies, NULL, realReg, TR_GPR, cg());
            }
         continue;
         }

      if (!dependencies->searchPreConditionRegister(realReg))
         {
         if (realReg == properties.getIntegerArgumentRegister(0) && callNode->getDataType() == TR::Address)
            {
            dependencies->addPreCondition(cg()->allocateRegister(), TR::RealRegister::x0);
            dependencies->addPostCondition(cg()->allocateCollectedReferenceRegister(), TR::RealRegister::x0);
            }
         else
            {
            TR::addDependency(dependencies, NULL, realReg, TR_GPR, cg());
            }
         }
      }

   int32_t floatRegsUsed = std::min<int32_t>(numFloatArgs, properties.getNumFloatArgRegs());
   for (i = (TR::RealRegister::RegNum)((uint32_t)TR::RealRegister::v0 + floatRegsUsed); i <= TR::RealRegister::LastFPR; i++)
      {
      if (!properties.getPreserved((TR::RealRegister::RegNum)i))
         {
         // NULL dependency for non-preserved regs
         TR::addDependency(dependencies, NULL, (TR::RealRegister::RegNum)i, TR_FPR, cg());
         }
      }

   if (numMemArgs > 0)
      {
      TR::RealRegister *sp = cg()->machine()->getRealRegister(properties.getStackPointerRegister());
      generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::subimmx, callNode, argMemReg, sp, totalSize);

      for (argIndex = 0; argIndex < numMemArgs; argIndex++)
         {
         TR::Register *aReg = pushToMemory[argIndex].argRegister;
         generateMemSrc1Instruction(cg(), pushToMemory[argIndex].opCode, callNode, pushToMemory[argIndex].argMemory, aReg);
         cg()->stopUsingRegister(aReg);
         }

      cg()->stopUsingRegister(argMemReg);
      }

   return totalSize;
   }

TR::Register *J9::ARM64::JNILinkage::getReturnRegisterFromDeps(TR::Node *callNode, TR::RegisterDependencyConditions *deps)
   {
   const TR::ARM64LinkageProperties &pp = _systemLinkage->getProperties();
   TR::Register *retReg;

   switch(callNode->getOpCodeValue())
      {
      case TR::icall:
         retReg = deps->searchPostConditionRegister(
                     pp.getIntegerReturnRegister());
         break;
      case TR::lcall:
      case TR::acall:
         retReg = deps->searchPostConditionRegister(
                     pp.getLongReturnRegister());
         break;
      case TR::fcall:
      case TR::dcall:
         retReg = deps->searchPostConditionRegister(
                     pp.getFloatReturnRegister());
         break;
      case TR::call:
         retReg = NULL;
         break;
      default:
         retReg = NULL;
         TR_ASSERT_FATAL(false, "Unsupported direct call Opcode.");
      }
   return retReg;
   }

TR::Register *J9::ARM64::JNILinkage::pushJNIReferenceArg(TR::Node *child)
   {
   TR::Register *pushRegister;
   bool         checkSplit = true;

   if (child->getOpCodeValue() == TR::loadaddr)
      {
      TR::SymbolReference * symRef = child->getSymbolReference();
      TR::StaticSymbol *sym = symRef->getSymbol()->getStaticSymbol();
      if (sym)
         {
         if (sym->isAddressOfClassObject())
            {
            pushRegister = pushAddressArg(child);
            }
         else
            {
            TR::Register *addrReg = cg()->evaluate(child);
            TR::MemoryReference *tmpMemRef = new (trHeapMemory()) TR::MemoryReference(addrReg, (int32_t)0, cg());
            TR::Register *whatReg = cg()->allocateCollectedReferenceRegister();

            checkSplit = false;
            generateTrg1MemInstruction(cg(), TR::InstOpCode::ldrimmx, child, whatReg, tmpMemRef);
            if (!cg()->canClobberNodesRegister(child))
               {
               // Since this is a static variable, it is non-collectable.
               TR::Register *tempRegister = cg()->allocateRegister();
               generateMovInstruction(cg(), child, tempRegister, addrReg);
               pushRegister = tempRegister;
               }
            else
               pushRegister = addrReg;
            generateCompareImmInstruction(cg(), child, whatReg, 0, true);
            generateCondTrg1Src2Instruction(cg(), TR::InstOpCode::cselx, child, pushRegister, pushRegister, whatReg, TR::CC_NE);

            cg()->stopUsingRegister(whatReg);
            cg()->decReferenceCount(child);
            }
         }
      else // must be loadaddr of parm or local
         {
         if (child->pointsToNonNull())
            {
            pushRegister = pushAddressArg(child);
            }
         else if (child->pointsToNull())
            {
            checkSplit = false;
            pushRegister = cg()->allocateRegister();
            loadConstant64(cg(), child, 0, pushRegister);
            cg()->decReferenceCount(child);
            }
         else
            {
            TR::Register *addrReg = cg()->evaluate(child);
            TR::MemoryReference *tmpMemRef = new (trHeapMemory()) TR::MemoryReference(addrReg, (int32_t)0, cg());
            TR::Register *whatReg = cg()->allocateCollectedReferenceRegister();

            checkSplit = false;
            generateTrg1MemInstruction(cg(), TR::InstOpCode::ldrimmx, child, whatReg, tmpMemRef);
            if (!cg()->canClobberNodesRegister(child))
               {
               // Since this points at a parm or local location, it is non-collectable.
               TR::Register *tempRegister = cg()->allocateRegister();
               generateMovInstruction(cg(), child, tempRegister, addrReg);
               pushRegister = tempRegister;
               }
            else
               pushRegister = addrReg;
            generateCompareImmInstruction(cg(), child, whatReg, 0, true);
            generateCondTrg1Src2Instruction(cg(), TR::InstOpCode::cselx, child, pushRegister, pushRegister, whatReg, TR::CC_NE);

            cg()->stopUsingRegister(whatReg);
            cg()->decReferenceCount(child);
            }
         }
      }
   else
      {
      pushRegister = pushAddressArg(child);
      }

   if (checkSplit && !cg()->canClobberNodesRegister(child, 0))
      {
      TR::Register *tempReg = pushRegister->containsCollectedReference()?
        cg()->allocateCollectedReferenceRegister():cg()->allocateRegister();
      generateMovInstruction(cg(), child, tempReg, pushRegister);
      pushRegister = tempReg;
      }
   return pushRegister;
   }

void J9::ARM64::JNILinkage::adjustReturnValue(TR::Node *callNode, bool wrapRefs, TR::Register *returnRegister)
   {
   TR::SymbolReference      *callSymRef = callNode->getSymbolReference();
   TR::ResolvedMethodSymbol *callSymbol = callNode->getSymbol()->castToResolvedMethodSymbol();
   TR_ResolvedMethod *resolvedMethod = callSymbol->getResolvedMethod();

   // jni methods may not return a full register in some cases so need to get the declared
   // type so that we sign and zero extend the narrower integer return types properly
   TR::LabelSymbol *tempLabel = generateLabelSymbol(cg());

   switch (resolvedMethod->returnType())
      {
      case TR::Address:
         if (wrapRefs)
            {
            // unwrap when the returned object is non-null
            generateCompareBranchInstruction(cg(),TR::InstOpCode::cbzx, callNode, returnRegister, tempLabel);
            generateTrg1MemInstruction(cg(), TR::InstOpCode::ldrimmx, callNode, returnRegister, new (trHeapMemory()) TR::MemoryReference(returnRegister, 0, cg()));
            generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, tempLabel);
            }
         break;
      case TR::Int8:
         if (comp()->getSymRefTab()->isReturnTypeBool(callSymRef))
            {
            // For bool return type, must check whether value return by
            // JNI is zero (false) or non-zero (true) to yield Java result
            generateCompareImmInstruction(cg(), callNode, returnRegister, 0);
            generateCSetInstruction(cg(), callNode, returnRegister, TR::CC_NE);
            }
         else if (resolvedMethod->returnTypeIsUnsigned())
            {
            // 7 in immr:imms means 0xff
            generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::andimmw, callNode, returnRegister, returnRegister, 7);
            }
         else
            {
            // sxtb (sign extend byte)
            generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::sbfmw, callNode, returnRegister, returnRegister, 7);
            }
         break;
      case TR::Int16:
         if (resolvedMethod->returnTypeIsUnsigned())
            {
            // 0xf in immr:imms means 0xffff
            generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::andimmw, callNode, returnRegister, returnRegister, 0xf);
            }
         else
            {
            // sxth (sign extend halfword)
            generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::sbfmw, callNode, returnRegister, returnRegister, 0xf);
            }
         break;
      }
   }

void J9::ARM64::JNILinkage::checkForJNIExceptions(TR::Node *callNode, TR::Register *vmThreadReg, TR::Register *scratchReg)
   {
   TR_J9VMBase *fej9 = reinterpret_cast<TR_J9VMBase *>(fe());

   generateTrg1MemInstruction(cg(),TR::InstOpCode::ldrimmx, callNode, scratchReg, new (trHeapMemory()) TR::MemoryReference(vmThreadReg, fej9->thisThreadGetCurrentExceptionOffset(), cg()));

   TR::SymbolReference *throwSymRef = cg()->getSymRefTab()->findOrCreateThrowCurrentExceptionSymbolRef(comp()->getJittedMethodSymbol());
   TR::LabelSymbol *exceptionSnippetLabel = generateLabelSymbol(cg());
   TR::Snippet *snippet = new (trHeapMemory()) TR::ARM64HelperCallSnippet(cg(), callNode, exceptionSnippetLabel, throwSymRef);
   cg()->addSnippet(snippet);
   TR::Instruction *gcPoint = generateCompareBranchInstruction(cg(),TR::InstOpCode::cbnzx, callNode, scratchReg, exceptionSnippetLabel);
   // x0 may contain a reference returned by JNI method
   snippet->gcMap().setGCRegisterMask(1);
   }

TR::Instruction *J9::ARM64::JNILinkage::generateMethodDispatch(TR::Node *callNode, bool isJNIGCPoint,
                                                               TR::RegisterDependencyConditions *deps, uintptr_t targetAddress, TR::Register *scratchReg)
   {
   TR::ResolvedMethodSymbol *resolvedMethodSymbol = callNode->getSymbol()->castToResolvedMethodSymbol();
   TR_ResolvedMethod *resolvedMethod = resolvedMethodSymbol->getResolvedMethod();
   TR_J9VMBase *fej9 = reinterpret_cast<TR_J9VMBase *>(fe());

   TR::Instruction *cursor = cg()->getAppendInstruction();

   // load a 64-bit constant into a register with a fixed 4 instruction sequence
   TR::Instruction *firstInstruction;
   cursor = firstInstruction = generateTrg1ImmInstruction(cg(), TR::InstOpCode::movzx, callNode, scratchReg, targetAddress & 0x0000ffff, cursor);
   cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::movkx, callNode, scratchReg, ((targetAddress >> 16) & 0x0000ffff) | TR::MOV_LSL16, cursor);
   cursor = generateTrg1ImmInstruction(cg(), TR::InstOpCode::movkx, callNode, scratchReg, ((targetAddress >> 32) & 0x0000ffff) | TR::MOV_LSL32, cursor);
   generateTrg1ImmInstruction(cg(), TR::InstOpCode::movkx, callNode, scratchReg, (targetAddress >> 48) | TR::MOV_LSL48, cursor);

   if (fej9->needClassAndMethodPointerRelocations())
      {
      TR_ExternalRelocationTargetKind reloType;
      if (resolvedMethodSymbol->isSpecial())
         reloType = TR_JNISpecialTargetAddress;
      else if (resolvedMethodSymbol->isStatic())
         reloType = TR_JNIStaticTargetAddress;
      else if (resolvedMethodSymbol->isVirtual())
         reloType = TR_JNIVirtualTargetAddress;
      else
         {
         reloType = TR_NoRelocation;
         TR_ASSERT(0, "JNI relocation not supported.");
         }
      cg()->addExternalRelocation(new (trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
                                                            firstInstruction,
                                                            reinterpret_cast<uint8_t *>(callNode->getSymbolReference()),
                                                            reinterpret_cast<uint8_t *>(callNode->getInlinedSiteIndex()),
                                                            reloType, cg()),
                                                          __FILE__,__LINE__, callNode);

      }

   // Add the first instruction of address materialization sequence to JNI call sites
   cg()->getJNICallSites().push_front(new (trHeapMemory()) TR_Pair<TR_ResolvedMethod, TR::Instruction>(resolvedMethod, firstInstruction));

   TR::Instruction *gcPoint = generateRegBranchInstruction(cg(), TR::InstOpCode::blr, callNode, scratchReg, deps);
   if (isJNIGCPoint)
      {
      // preserved registers are killed
      gcPoint->ARM64NeedsGCMap(cg(), 0);
      }
   return gcPoint;
   }

TR::Register *J9::ARM64::JNILinkage::buildDirectDispatch(TR::Node *callNode)
   {
   TR::LabelSymbol *returnLabel = generateLabelSymbol(cg());
   TR::SymbolReference *callSymRef = callNode->getSymbolReference();
   TR::MethodSymbol *callSymbol = callSymRef->getSymbol()->castToMethodSymbol();
   TR::ResolvedMethodSymbol *resolvedMethodSymbol = callNode->getSymbol()->castToResolvedMethodSymbol();
   TR_ResolvedMethod *resolvedMethod = resolvedMethodSymbol->getResolvedMethod();
   uintptr_t targetAddress = reinterpret_cast<uintptr_t>(resolvedMethod->startAddressForJNIMethod(comp()));
   TR_J9VMBase *fej9 = reinterpret_cast<TR_J9VMBase *>(fe());

   bool dropVMAccess = !fej9->jniRetainVMAccess(resolvedMethod);
   bool isJNIGCPoint = !fej9->jniNoGCPoint(resolvedMethod);
   bool killNonVolatileGPRs = isJNIGCPoint;
   bool checkExceptions = !fej9->jniNoExceptionsThrown(resolvedMethod);
   bool createJNIFrame = !fej9->jniNoNativeMethodFrame(resolvedMethod);
   bool tearDownJNIFrame = !fej9->jniNoSpecialTeardown(resolvedMethod);
   bool wrapRefs = !fej9->jniDoNotWrapObjects(resolvedMethod);
   bool passReceiver = !fej9->jniDoNotPassReceiver(resolvedMethod);
   bool passThread = !fej9->jniDoNotPassThread(resolvedMethod);

   if (resolvedMethodSymbol->canDirectNativeCall())
      {
      dropVMAccess = false;
      killNonVolatileGPRs = false;
      isJNIGCPoint = false;
      checkExceptions = false;
      createJNIFrame = false;
      tearDownJNIFrame = false;
      }
   else if (resolvedMethodSymbol->isPureFunction())
      {
      dropVMAccess = false;
      isJNIGCPoint = false;
      checkExceptions = false;
      }

   cg()->machine()->setLinkRegisterKilled(true);

   const int maxRegisters = getProperties()._numAllocatableIntegerRegisters + getProperties()._numAllocatableFloatRegisters;
   TR::RegisterDependencyConditions *deps = new (trHeapMemory()) TR::RegisterDependencyConditions(maxRegisters, maxRegisters, trMemory());

   size_t spSize = buildJNIArgs(callNode, deps, passThread, passReceiver, killNonVolatileGPRs);
   TR::RealRegister *sp = machine()->getRealRegister(_systemLinkage->getProperties().getStackPointerRegister());

   if (spSize > 0)
      {
      if (constantIsUnsignedImm12(spSize))
         {
         generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::subimmx, callNode, sp, sp, spSize);
         }
      else
         {
         TR_ASSERT_FATAL(false, "Too many arguments.");
         }
      }

   TR::Register *returnRegister = getReturnRegisterFromDeps(callNode, deps);
   auto postLabelDeps = deps->clonePost(cg());
   TR::RealRegister *vmThreadReg = machine()->getRealRegister(getProperties().getMethodMetaDataRegister());   // x19
   TR::RealRegister *javaStackReg = machine()->getRealRegister(getProperties().getStackPointerRegister());    // x20
   TR::Register *x9Reg = deps->searchPreConditionRegister(TR::RealRegister::x9);
   TR::Register *x10Reg = deps->searchPreConditionRegister(TR::RealRegister::x10);
   TR::Register *x11Reg = deps->searchPreConditionRegister(TR::RealRegister::x11);
   TR::Register *x12Reg = deps->searchPreConditionRegister(TR::RealRegister::x12);

   if (createJNIFrame)
      {
      buildJNICallOutFrame(callNode, (resolvedMethod == comp()->getCurrentMethod()), returnLabel, vmThreadReg, javaStackReg, x9Reg, x10Reg);
      }

   if (passThread)
      {
      TR::RealRegister *vmThread = machine()->getRealRegister(getProperties().getMethodMetaDataRegister());
      TR::Register *x0Reg = deps->searchPreConditionRegister(TR::RealRegister::x0);
      generateMovInstruction(cg(), callNode, x0Reg, vmThread);
      }

   if (dropVMAccess)
      {
#ifdef J9VM_INTERP_ATOMIC_FREE_JNI
      releaseVMAccessAtomicFree(callNode, vmThreadReg);
#else
      releaseVMAccess(callNode, vmThreadReg, x9Reg, x10Reg, x11Reg, x12Reg);
#endif
      }

   TR::Instruction *callInstr = generateMethodDispatch(callNode, isJNIGCPoint, deps, targetAddress, x9Reg);
   generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, returnLabel, callInstr);

   if (spSize > 0)
      {
      if (constantIsUnsignedImm12(spSize))
         {
         generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::addimmx, callNode, sp, sp, spSize);
         }
      else
         {
         TR_ASSERT_FATAL(false, "Too many arguments.");
         }
      }

   if (dropVMAccess)
      {
#ifdef J9VM_INTERP_ATOMIC_FREE_JNI
      acquireVMAccessAtomicFree(callNode, vmThreadReg);
#else
      acquireVMAccess(callNode, vmThreadReg, x9Reg, x10Reg, x11Reg);
#endif
      }

   if (returnRegister != NULL)
      {
      adjustReturnValue(callNode, wrapRefs, returnRegister);
      }

   if (createJNIFrame)
      {
      restoreJNICallOutFrame(callNode, tearDownJNIFrame, vmThreadReg, javaStackReg, x9Reg);
      }

   if (checkExceptions)
      {
      checkForJNIExceptions(callNode, vmThreadReg, x9Reg);
      }

   TR::LabelSymbol *depLabel = generateLabelSymbol(cg());
   generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, depLabel, postLabelDeps);

   callNode->setRegister(returnRegister);

   deps->stopUsingDepRegs(cg(), returnRegister);
   return returnRegister;
   }
