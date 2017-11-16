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

#include "p/codegen/PPCJNILinkage.hpp"

#include "codegen/CodeGenerator.hpp"
#include "codegen/Machine.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/CHTable.hpp"
#include "env/CompilerEnv.hpp"
#include "env/VMJ9.h"
#include "env/jittypes.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "infra/SimpleRegex.hpp"
#include "p/codegen/CallSnippet.hpp"
#include "p/codegen/GenerateInstructions.hpp"
#include "p/codegen/PPCEvaluator.hpp"
#include "p/codegen/PPCHelperCallSnippet.hpp"
#include "p/codegen/PPCInstruction.hpp"
#include "p/codegen/PPCTableOfConstants.hpp"
#include "p/codegen/StackCheckFailureSnippet.hpp"

TR::PPCJNILinkage::PPCJNILinkage(TR::CodeGenerator *cg)
   :TR::PPCPrivateLinkage(cg)
   {
   //Copy out SystemLinkage properties. Assumes no objects in TR::PPCLinkageProperties.
   TR::Linkage *sysLinkage = cg->getLinkage(TR_System);
   const TR::PPCLinkageProperties& sysLinkageProperties = sysLinkage->getProperties();

   _properties = sysLinkageProperties;

   //Set preservedRegisterMapForGC to PrivateLinkage properties.
   TR::Linkage *privateLinkage = cg->getLinkage(TR_Private);
   const TR::PPCLinkageProperties& privateLinkageProperties = privateLinkage->getProperties();

   _properties._preservedRegisterMapForGC = privateLinkageProperties.getPreservedRegisterMapForGC();

   }

int32_t TR::PPCJNILinkage::buildArgs(TR::Node *callNode,
                             TR::RegisterDependencyConditions *dependencies,
                             const TR::PPCLinkageProperties &properties)
   {
   TR_ASSERT(0, "Should call TR::PPCJNILinkage::buildJNIArgs instead.");
   return 0;
   }

TR::Register *TR::PPCJNILinkage::buildIndirectDispatch(TR::Node  *callNode)
   {
   TR_ASSERT(0, "Calling TR::PPCJNILinkage::buildIndirectDispatch does not make sense.");
   return NULL;
   }

void         TR::PPCJNILinkage::buildVirtualDispatch(TR::Node   *callNode,
                                                    TR::RegisterDependencyConditions *dependencies,
                                                    uint32_t                           sizeOfArguments)
   {
   TR_ASSERT(0, "Calling TR::PPCJNILinkage::buildVirtualDispatch does not make sense.");
   }

const TR::PPCLinkageProperties& TR::PPCJNILinkage::getProperties()
   {
   return _properties;
   }

TR::Register *TR::PPCJNILinkage::buildDirectDispatch(TR::Node *callNode)
   {
   bool aix_style_linkage = (TR::Compiler->target.isAIX() || (TR::Compiler->target.is64Bit() && TR::Compiler->target.isLinux()));
   TR::LabelSymbol           *returnLabel = generateLabelSymbol(cg());
   TR::SymbolReference      *callSymRef = callNode->getSymbolReference();
   TR::MethodSymbol         *callSymbol = callSymRef->getSymbol()->castToMethodSymbol();
   TR::SymbolReference      *gpuHelperSymRef;

   TR::ResolvedMethodSymbol *resolvedMethodSymbol;
   TR_ResolvedMethod       *resolvedMethod;
   bool dropVMAccess;
   bool isJNIGCPoint;
   bool killNonVolatileGPRs;
   bool checkExceptions;
   bool createJNIFrame;
   bool tearDownJNIFrame;
   bool wrapRefs;
   bool passReceiver;
   bool passThread;
   bool jniAccelerated;
   uintptrj_t targetAddress;

   bool crc32m1 = (callSymbol->getRecognizedMethod() == TR::java_util_zip_CRC32_update);
   bool crc32m2 = (callSymbol->getRecognizedMethod() == TR::java_util_zip_CRC32_updateBytes);
   bool crc32m3 = (callSymbol->getRecognizedMethod() == TR::java_util_zip_CRC32_updateByteBuffer);

   // TODO: How to handle discontiguous array?
   bool specialCaseJNI = (crc32m1 || crc32m2 || crc32m3) && !comp()->requiresSpineChecks();

   bool isGPUHelper = callSymbol->isHelper() && (callSymRef->getReferenceNumber() == TR_estimateGPU ||
                                                 callSymRef->getReferenceNumber() == TR_getStateGPU ||
                                                 callSymRef->getReferenceNumber() == TR_regionEntryGPU ||
                                                 callSymRef->getReferenceNumber() == TR_allocateGPUKernelParms ||
                                                 callSymRef->getReferenceNumber() == TR_copyToGPU ||
                                                 callSymRef->getReferenceNumber() == TR_launchGPUKernel ||
                                                 callSymRef->getReferenceNumber() == TR_copyFromGPU ||
                                                 callSymRef->getReferenceNumber() == TR_invalidateGPU ||
                                                 callSymRef->getReferenceNumber() == TR_regionExitGPU ||
                                                 callSymRef->getReferenceNumber() == TR_flushGPU);


   static bool keepVMDuringGPUHelper = feGetEnv("TR_KeepVMDuringGPUHelper") ? true : false;

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   if (!isGPUHelper)
      {
      resolvedMethodSymbol = callNode->getSymbol()->castToResolvedMethodSymbol();
      resolvedMethod = resolvedMethodSymbol->getResolvedMethod();
      dropVMAccess = !fej9->jniRetainVMAccess(resolvedMethod);
      isJNIGCPoint = !fej9->jniNoGCPoint(resolvedMethod);
      killNonVolatileGPRs = isJNIGCPoint;
      checkExceptions = !fej9->jniNoExceptionsThrown(resolvedMethod);
      createJNIFrame = !fej9->jniNoNativeMethodFrame(resolvedMethod);
      tearDownJNIFrame = !fej9->jniNoSpecialTeardown(resolvedMethod);
      wrapRefs = !fej9->jniDoNotWrapObjects(resolvedMethod);
      passReceiver = !fej9->jniDoNotPassReceiver(resolvedMethod);
      passThread = !fej9->jniDoNotPassThread(resolvedMethod);
      jniAccelerated = false;
      targetAddress = (uintptrj_t)resolvedMethod->startAddressForJNIMethod(comp());
      }
   else
      {
      gpuHelperSymRef = comp()->getSymRefTab()->methodSymRefFromName(comp()->getMethodSymbol(), "com/ibm/jit/JITHelpers", "GPUHelper", "()V", TR::MethodSymbol::Static);
      resolvedMethodSymbol = gpuHelperSymRef->getSymbol()->castToResolvedMethodSymbol();
      resolvedMethod = resolvedMethodSymbol->getResolvedMethod();

      if (keepVMDuringGPUHelper || (callSymRef->getReferenceNumber() == TR_copyToGPU || callSymRef->getReferenceNumber() == TR_copyFromGPU) || callSymRef->getReferenceNumber() == TR_regionExitGPU || callSymRef->getReferenceNumber() == TR_flushGPU)
         dropVMAccess = false; //TR_copyToGPU, TR_copyFromGPU, TR_regionExitGPU, TR_flushGPU (and all others if keepVMDuringGPUHelper is true)
      else
         dropVMAccess = true; //TR_regionEntryGPU, TR_launchGPUKernel, TR_estimateGPU, TR_allocateGPUKernelParms, (only if keepVMDuringGPUHelper is false)

      isJNIGCPoint = true;
      killNonVolatileGPRs = isJNIGCPoint;
      checkExceptions = false;
      createJNIFrame = true;
      tearDownJNIFrame = true;
      wrapRefs = false; //unused for this code path
      passReceiver = true;
      passThread = false;
      jniAccelerated = false; //unused for this code path
      targetAddress = (uintptrj_t)callSymbol->getMethodAddress();
      }

   if (!isGPUHelper && (TR::Options::getJniAccelerator() != NULL))
      {
      jniAccelerated = TR::SimpleRegex::match(TR::Options::getJniAccelerator(), resolvedMethod->signature(comp()->trMemory()));
      }

   if (!isGPUHelper && (callSymbol->isPureFunction() || resolvedMethodSymbol->canDirectNativeCall() || jniAccelerated || specialCaseJNI))
      {
      dropVMAccess = false;
      killNonVolatileGPRs = false;
      isJNIGCPoint = false;
      checkExceptions = false;
      createJNIFrame = false;
      tearDownJNIFrame = false;
      if (specialCaseJNI)
         {
         wrapRefs = false;
         passReceiver = false;
         passThread = false;
         }
      }

   TR::Instruction  *gcPoint;
   TR::Register        *returnRegister = NULL;
   TR::RealRegister *stackPtr = cg()->getStackPointerRegister();
   TR::RealRegister *metaReg = cg()->getMethodMetaDataRegister();
   TR::Register        *gr2Reg, *gr30Reg, *gr31Reg;
   int32_t             argSize;
   intptrj_t           aValue;

   TR::RegisterDependencyConditions *deps = new (trHeapMemory()) TR::RegisterDependencyConditions(104,104, trMemory());
   const TR::PPCLinkageProperties& jniLinkageProperties = getProperties();

   if (killNonVolatileGPRs || dropVMAccess || checkExceptions || tearDownJNIFrame)
      {
      gr30Reg = cg()->allocateRegister();
      gr31Reg = cg()->allocateRegister();
      addDependency(deps, gr30Reg, TR::RealRegister::gr30, TR_GPR, cg());
      addDependency(deps, gr31Reg, TR::RealRegister::gr31, TR_GPR, cg());
      }

   if (killNonVolatileGPRs)
      {
      // We need to kill all the non-volatiles so that they'll be in a stack frame in case
      // gc needs to find them.
      if (TR::Compiler->target.is32Bit())
         {
         // gr15 and gr16 are reserved in 64-bit, normal non-volatile in 32-bit
         addDependency(deps, NULL, TR::RealRegister::gr15, TR_GPR, cg());
         addDependency(deps, NULL, TR::RealRegister::gr16, TR_GPR, cg());
         }
      addDependency(deps, NULL, TR::RealRegister::gr17, TR_GPR, cg());
      addDependency(deps, NULL, TR::RealRegister::gr18, TR_GPR, cg());
      addDependency(deps, NULL, TR::RealRegister::gr19, TR_GPR, cg());
      addDependency(deps, NULL, TR::RealRegister::gr20, TR_GPR, cg());
      addDependency(deps, NULL, TR::RealRegister::gr21, TR_GPR, cg());
      addDependency(deps, NULL, TR::RealRegister::gr22, TR_GPR, cg());
      addDependency(deps, NULL, TR::RealRegister::gr23, TR_GPR, cg());
      addDependency(deps, NULL, TR::RealRegister::gr24, TR_GPR, cg());
      addDependency(deps, NULL, TR::RealRegister::gr25, TR_GPR, cg());
      addDependency(deps, NULL, TR::RealRegister::gr26, TR_GPR, cg());
      addDependency(deps, NULL, TR::RealRegister::gr27, TR_GPR, cg());
#ifndef J9VM_INTERP_ATOMIC_FREE_JNI
      if (!dropVMAccess)
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
         {
         addDependency(deps, NULL, TR::RealRegister::gr28, TR_GPR, cg());
         addDependency(deps, NULL, TR::RealRegister::gr29, TR_GPR, cg());
         }
      }

   cg()->machine()->setLinkRegisterKilled(true);
   cg()->setHasCall();

   argSize = buildJNIArgs(callNode, deps, jniLinkageProperties, specialCaseJNI?false:true, passReceiver, passThread);

   if (aix_style_linkage)
      {
      if (specialCaseJNI)
         gr2Reg = deps->searchPreConditionRegister(TR::RealRegister::gr2);
      else
         {
         gr2Reg = cg()->allocateRegister();
         addDependency(deps, gr2Reg, TR::RealRegister::gr2, TR_GPR, cg());
         }
      }

   if (specialCaseJNI)
      {
      // No argument change is needed
      if (crc32m1)
         {
         targetAddress = (uintptrj_t)crc32_oneByte;
         }

      // Argument changes are needed
      if (crc32m2 || crc32m3)
         {
         targetAddress = (uintptrj_t)((TR::Compiler->target.cpu.id() >= TR_PPCp8 && TR::Compiler->target.cpu.getPPCSupportsVSX())?crc32_vpmsum:crc32_no_vpmsum);

         // Assuming pre/postCondition have the same index, we use preCondition to map
         TR_PPCRegisterDependencyMap map(deps->getPreConditions()->getRegisterDependency(0), deps->getAddCursorForPre());
         for (int32_t cnt=0; cnt < deps->getAddCursorForPre(); cnt++)
            map.addDependency(deps->getPreConditions()->getRegisterDependency(cnt), cnt);
         
         TR::Register *addrArg, *posArg, *lenArg, *wasteArg;
         if (crc32m2)
            {
            addrArg = map.getVirtualWithTarget(TR::RealRegister::gr4);
            posArg = map.getVirtualWithTarget(TR::RealRegister::gr5);
            lenArg = map.getVirtualWithTarget(TR::RealRegister::gr6);

            generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::addi2, callNode, addrArg, addrArg, TR::Compiler->om.contiguousArrayHeaderSizeInBytes());
            }
         
         if (crc32m3)
            {
            addrArg = map.getVirtualWithTarget(TR::Compiler->target.is64Bit()?(TR::RealRegister::gr4):(TR::RealRegister::gr5));
            posArg = map.getVirtualWithTarget(TR::Compiler->target.is64Bit()?(TR::RealRegister::gr5):(TR::RealRegister::gr6));
            lenArg = map.getVirtualWithTarget(TR::Compiler->target.is64Bit()?(TR::RealRegister::gr6):(TR::RealRegister::gr7));
            if (!TR::Compiler->target.is64Bit())
               wasteArg = map.getVirtualWithTarget(TR::RealRegister::gr4);
            }
         generateTrg1Src2Instruction(cg(), TR::InstOpCode::add, callNode, addrArg, addrArg, posArg);

         deps->getPreConditions()->setDependencyInfo(map.getTargetIndex(TR::RealRegister::gr4), addrArg, TR::RealRegister::gr4, UsesDependentRegister);
         deps->getPostConditions()->setDependencyInfo(map.getTargetIndex(TR::RealRegister::gr4), addrArg, TR::RealRegister::gr4, UsesDependentRegister);

         deps->getPreConditions()->setDependencyInfo(map.getTargetIndex(TR::RealRegister::gr5), lenArg, TR::RealRegister::gr5, UsesDependentRegister);
         deps->getPostConditions()->setDependencyInfo(map.getTargetIndex(TR::RealRegister::gr5), lenArg, TR::RealRegister::gr5, UsesDependentRegister);

         deps->getPreConditions()->setDependencyInfo(map.getTargetIndex(TR::RealRegister::gr6), posArg, TR::RealRegister::gr6, UsesDependentRegister);
         deps->getPostConditions()->setDependencyInfo(map.getTargetIndex(TR::RealRegister::gr6), posArg, TR::RealRegister::gr6, UsesDependentRegister);

         if (crc32m3 && !TR::Compiler->target.is64Bit())
            {
            deps->getPreConditions()->setDependencyInfo(map.getTargetIndex(TR::RealRegister::gr7), wasteArg, TR::RealRegister::gr7, UsesDependentRegister);
            deps->getPostConditions()->setDependencyInfo(map.getTargetIndex(TR::RealRegister::gr7), wasteArg, TR::RealRegister::gr7, UsesDependentRegister);
            }
         }
      }

   TR::Register *gr0Reg = deps->searchPreConditionRegister(TR::RealRegister::gr0);
   TR::Register *gr3Reg = deps->searchPreConditionRegister(TR::RealRegister::gr3);
   TR::Register *gr11Reg = deps->searchPreConditionRegister(TR::RealRegister::gr11);
   TR::Register *gr12Reg = deps->searchPreConditionRegister(TR::RealRegister::gr12);
   TR::Register *cr0Reg = deps->searchPreConditionRegister(TR::RealRegister::cr0);
   TR::Register *lowReg = NULL, *highReg;

   switch (callNode->getOpCodeValue())
      {
      case TR::icall:
      case TR::acall:
         if (callNode->getDataType() == TR::Address)
            {
            if (!gr3Reg)
               {
               gr3Reg = cg()->allocateRegister();
               returnRegister = cg()->allocateCollectedReferenceRegister();
               deps->addPreCondition(gr3Reg, TR::RealRegister::gr3);
               deps->addPostCondition(returnRegister, TR::RealRegister::gr3);
               }
            else
               {
               returnRegister = deps->searchPostConditionRegister(TR::RealRegister::gr3);
               }
            }
         else
            {
            if (!gr3Reg)
               {
               gr3Reg = cg()->allocateRegister();
               returnRegister = gr3Reg;
               addDependency(deps, gr3Reg, TR::RealRegister::gr3, TR_GPR, cg());
               }
            else
               {
               returnRegister = deps->searchPostConditionRegister(TR::RealRegister::gr3);
               }
            }
         break;
      case TR::lcall:
         if (TR::Compiler->target.is64Bit())
            {
            if (!gr3Reg)
               {
               gr3Reg = cg()->allocateRegister();
               returnRegister = gr3Reg;
               addDependency(deps, gr3Reg, TR::RealRegister::gr3, TR_GPR, cg());
               }
            else
               {
               returnRegister = deps->searchPostConditionRegister(TR::RealRegister::gr3);
               }
            }
         else
            {
            if (!gr3Reg)
               {
               gr3Reg = cg()->allocateRegister();
               addDependency(deps, gr3Reg, TR::RealRegister::gr3, TR_GPR, cg());
               highReg = gr3Reg;
               }
            else
               {
               highReg = deps->searchPostConditionRegister(TR::RealRegister::gr3);
               }
            lowReg = deps->searchPostConditionRegister(TR::RealRegister::gr4);
            returnRegister = cg()->allocateRegisterPair(lowReg, highReg);
            }
         break;
      case TR::fcall:
      case TR::dcall:
         returnRegister = deps->searchPostConditionRegister(jniLinkageProperties.getFloatReturnRegister());
         if (!gr3Reg)
            {
            gr3Reg = cg()->allocateRegister();
            addDependency(deps, gr3Reg, TR::RealRegister::gr3, TR_GPR, cg());
            }
         break;
      case TR::call:
         if (!gr3Reg)
            {
            gr3Reg = cg()->allocateRegister();
            addDependency(deps, gr3Reg, TR::RealRegister::gr3, TR_GPR, cg());
            }
         returnRegister = NULL;
         break;
      default:
         if (!gr3Reg)
            {
            gr3Reg = cg()->allocateRegister();
            addDependency(deps, gr3Reg, TR::RealRegister::gr3, TR_GPR, cg());
            }
         returnRegister = NULL;
         TR_ASSERT( false, "Unknown direct call Opcode.");
      }

   if (createJNIFrame)
      {
      // push tag bits (savedA0)
      int32_t tagBits = fej9->constJNICallOutFrameSpecialTag();
      // if the current method is simply a wrapper for the JNI call, hide the call-out stack frame
      if (resolvedMethod == comp()->getCurrentMethod())
         tagBits |= fej9->constJNICallOutFrameInvisibleTag();
      loadConstant(cg(), callNode, tagBits, gr11Reg);
      loadConstant(cg(), callNode, 0, gr12Reg);

      generateMemSrc1Instruction(cg(),TR::InstOpCode::Op_stu, callNode,  new (trHeapMemory()) TR::MemoryReference(stackPtr, -TR::Compiler->om.sizeofReferenceAddress(), TR::Compiler->om.sizeofReferenceAddress(), cg()), gr11Reg);

      // skip savedPC slot (unused) and push return address (savedCP)
      cg()->fixedLoadLabelAddressIntoReg(callNode, gr11Reg, returnLabel);
      generateMemSrc1Instruction(cg(),TR::InstOpCode::Op_stu, callNode, new (trHeapMemory()) TR::MemoryReference(stackPtr, -2*TR::Compiler->om.sizeofReferenceAddress(), TR::Compiler->om.sizeofReferenceAddress(), cg()), gr11Reg);

      // begin: mask out the magic bit that indicates JIT frames below
      generateMemSrc1Instruction(cg(),TR::InstOpCode::Op_st, callNode, new (trHeapMemory()) TR::MemoryReference(metaReg, fej9->thisThreadGetJavaFrameFlagsOffset(), TR::Compiler->om.sizeofReferenceAddress(), cg()), gr12Reg);

      // push flags: use lis instead of lis/ori pair since this is a constant. Save one instr
      aValue = fej9->constJNICallOutFrameFlags();
      TR_ASSERT((aValue & ~0x7FFF0000) == 0, "Length assumption broken.");
      generateTrg1ImmInstruction(cg(), TR::InstOpCode::lis, callNode, gr11Reg, (aValue>>16)&0x0000FFFF);
      generateMemSrc1Instruction(cg(),TR::InstOpCode::Op_stu, callNode,  new (trHeapMemory()) TR::MemoryReference(stackPtr, -TR::Compiler->om.sizeofReferenceAddress(), TR::Compiler->om.sizeofReferenceAddress(), cg()),gr11Reg);

      // push the RAM method for the native
      aValue = (uintptrj_t)resolvedMethod->resolvedMethodAddress();
      // use loadAddressConstantFixed - fixed instruction count 2 32-bit, or 5 64-bit
      // loadAddressRAM needs a resolved method symbol so the gpuHelper SumRef is passed in instead of
      // the callSymRef which does not have a resolved method symbol
      if (isGPUHelper)
         callNode->setSymbolReference(gpuHelperSymRef);
      loadAddressRAM(cg(), callNode, aValue, gr11Reg);
      if (isGPUHelper)
         callNode->setSymbolReference(callSymRef); //change back to callSymRef afterwards
      generateMemSrc1Instruction(cg(),TR::InstOpCode::Op_stu, callNode,  new (trHeapMemory()) TR::MemoryReference(stackPtr, -TR::Compiler->om.sizeofReferenceAddress(), TR::Compiler->om.sizeofReferenceAddress(), cg()),gr11Reg);

      // store out jsp
      generateMemSrc1Instruction(cg(),TR::InstOpCode::Op_st, callNode, new (trHeapMemory()) TR::MemoryReference(metaReg,fej9->thisThreadGetJavaSPOffset(), TR::Compiler->om.sizeofReferenceAddress(), cg()), stackPtr);

      // store out pc and literals values indicating the callout frame
      aValue = fej9->constJNICallOutFrameType();
      TR_ASSERT(aValue>=LOWER_IMMED && aValue<=UPPER_IMMED, "Length assumption broken.");
      loadConstant(cg(), callNode, (int32_t)aValue, gr11Reg);
      generateMemSrc1Instruction(cg(),TR::InstOpCode::Op_st, callNode, new (trHeapMemory()) TR::MemoryReference(metaReg,fej9->thisThreadGetJavaPCOffset(), TR::Compiler->om.sizeofReferenceAddress(), cg()), gr11Reg);

      generateMemSrc1Instruction(cg(),TR::InstOpCode::Op_st, callNode, new (trHeapMemory()) TR::MemoryReference(metaReg,fej9->thisThreadGetJavaLiteralsOffset(), TR::Compiler->om.sizeofReferenceAddress(), cg()), gr12Reg);
      }

   if (passThread)
      generateTrg1Src1Instruction(cg(), TR::InstOpCode::mr, callNode, gr3Reg, metaReg);

   // Change if VMAccessLength if this code changes
   if (dropVMAccess)
      {
      // At this point: arguments for the native routine are all in place already, i.e., if there are
      //                more than 32Byte worth of arguments, some of them are on the stack. However,
      //                we potentially go out to call a helper before jumping to the native. There is
      //                no definite guarantee that the helper call will not trash the stack area concerned.
      //                But GAC did make sure it is safe with the current helper implementation. If this
      //                condition changes in the future, we will need some heroic measure to fix it.
      //                Furthermore, this potential call should not touch FP argument registers. ***
#ifdef J9VM_INTERP_ATOMIC_FREE_JNI
      releaseVMAccessAtomicFree(callNode, deps, metaReg, cr0Reg, gr30Reg, gr31Reg);
#else
      releaseVMAccess(callNode, deps, metaReg, gr12Reg, gr30Reg, gr31Reg);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
      }

   // get the address of the function descriptor
   // use loadAddressConstantFixed - fixed instruction count 2 32-bit, or 5 64-bit
   TR::Instruction *current = cg()->getAppendInstruction();
   if (isGPUHelper)
      loadConstant(cg(), callNode, (int64_t)targetAddress, gr12Reg);
   else
      loadAddressJNI(cg(), callNode, targetAddress, gr12Reg);
   cg()->getJNICallSites().push_front(new (trHeapMemory()) TR_Pair<TR_ResolvedMethod, TR::Instruction>(resolvedMethod, current->getNext())); // the first instruction generated by loadAddressC...

   if (aix_style_linkage &&
      !(TR::Compiler->target.is64Bit() && TR::Compiler->target.isLinux() && TR::Compiler->target.cpu.isLittleEndian()))
      {
      // get the target address
      generateTrg1MemInstruction(cg(),TR::InstOpCode::Op_load, callNode, gr0Reg, new (trHeapMemory()) TR::MemoryReference(gr12Reg, 0, TR::Compiler->om.sizeofReferenceAddress(), cg()));
      // put the target address into the count register
      generateSrc1Instruction(cg(), TR::InstOpCode::mtctr, callNode, gr0Reg);
      // load the toc register
      generateTrg1MemInstruction(cg(),TR::InstOpCode::Op_load, callNode, gr2Reg, new (trHeapMemory()) TR::MemoryReference(gr12Reg, TR::Compiler->om.sizeofReferenceAddress(), TR::Compiler->om.sizeofReferenceAddress(), cg()));
      // load the environment register
      generateTrg1MemInstruction(cg(),TR::InstOpCode::Op_load, callNode, gr11Reg, new (trHeapMemory()) TR::MemoryReference(gr12Reg, 2*TR::Compiler->om.sizeofReferenceAddress(), TR::Compiler->om.sizeofReferenceAddress(), cg()));
      }
   else
      {
      // put the target address into the count register
      generateSrc1Instruction(cg(), TR::InstOpCode::mtctr, callNode, gr12Reg);
      }

   // call the JNI function
   if (isJNIGCPoint)
      {
      gcPoint = generateInstruction(cg(), TR::InstOpCode::bctrl, callNode);
      gcPoint->PPCNeedsGCMap(jniLinkageProperties.getPreservedRegisterMapForGC());
      }
   else
      {
      generateInstruction(cg(), TR::InstOpCode::bctrl, callNode);
      }
   generateDepLabelInstruction(cg(), TR::InstOpCode::label, callNode, returnLabel, deps);

   if (dropVMAccess)
      {
      // Again, we have dependency on the fact that:
      //    1) this potential call will not trash the in-register return values
      //    2) we know GPRs are ok, since OTI saves GPRs before use
      //    3) FPRs are not so sure, pending GAC's verification ***
#ifdef J9VM_INTERP_ATOMIC_FREE_JNI
      acquireVMAccessAtomicFree(callNode, deps, metaReg, cr0Reg, gr30Reg, gr31Reg);
#else
      acquireVMAccess(callNode, deps, metaReg, gr12Reg, gr30Reg, gr31Reg);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
      }

   // jni methods may not return a full register in some cases so need to get the declared
   // type so that we sign and zero extend the narrower integer return types properly
   TR::LabelSymbol *tempLabel = generateLabelSymbol(cg());
   if (!isGPUHelper)
      {
      switch (resolvedMethod->returnType())
         {
         case TR::Address:
            if (wrapRefs)
               {
               // Unwrap the returned object if non-null
               generateTrg1Src1ImmInstruction(cg(),TR::InstOpCode::Op_cmpi, callNode, cr0Reg, returnRegister, 0);
               generateConditionalBranchInstruction(cg(), TR::InstOpCode::beq, callNode, tempLabel, cr0Reg);
               generateTrg1MemInstruction(cg(),TR::InstOpCode::Op_load, callNode, returnRegister, new (trHeapMemory()) TR::MemoryReference(returnRegister, 0, TR::Compiler->om.sizeofReferenceAddress(), cg()));
               generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, tempLabel);
               }
            break;
         case TR::Int8:
            if (resolvedMethod->returnTypeIsUnsigned())
               generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::andi_r, callNode, returnRegister, returnRegister, 0xff);
            else
               generateTrg1Src1Instruction(cg(), TR::InstOpCode::extsb, callNode, returnRegister, returnRegister);
            break;
         case TR::Int16:
            if (resolvedMethod->returnTypeIsUnsigned())
               generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::andi_r, callNode, returnRegister, returnRegister, 0xffff);
            else
               generateTrg1Src1Instruction(cg(), TR::InstOpCode::extsh, callNode, returnRegister, returnRegister);
            break;
         }
      }

   if (createJNIFrame)
      {
      // restore stack pointer: need to deal with growable stack -- stack may already be moved.
      generateTrg1MemInstruction(cg(),TR::InstOpCode::Op_load, callNode, gr12Reg, new (trHeapMemory()) TR::MemoryReference(metaReg,fej9->thisThreadGetJavaLiteralsOffset(), TR::Compiler->om.sizeofReferenceAddress(), cg()));
      generateTrg1MemInstruction(cg(),TR::InstOpCode::Op_load, callNode, stackPtr, new (trHeapMemory()) TR::MemoryReference(metaReg,fej9->thisThreadGetJavaSPOffset(), TR::Compiler->om.sizeofReferenceAddress(), cg()));
      generateTrg1Src2Instruction(cg(), TR::InstOpCode::add, callNode, stackPtr, gr12Reg, stackPtr);

      if (tearDownJNIFrame)
         {
         // must check to see if the ref pool was used and clean them up if so--or we
         // leave a bunch of pinned garbage behind that screws up the gc quality forever
         // Again, depend on that this call will not trash return register values, especially FPRs.
         //        Pending GAC's verification. ***
         uint32_t            flagValue = fej9->constJNIReferenceFrameAllocatedFlags();
         TR::LabelSymbol      *refPoolRestartLabel = generateLabelSymbol(cg());
         TR::SymbolReference *collapseSymRef = cg()->getSymRefTab()->findOrCreateRuntimeHelper(TR_PPCcollapseJNIReferenceFrame, false, false, false);

         generateTrg1MemInstruction(cg(),TR::InstOpCode::Op_load, callNode, gr30Reg, new (trHeapMemory()) TR::MemoryReference(stackPtr, fej9->constJNICallOutFrameFlagsOffset(), TR::Compiler->om.sizeofReferenceAddress(), cg()));
         simplifyANDRegImm(callNode, gr31Reg, gr30Reg, flagValue, cg());
         generateTrg1Src1ImmInstruction(cg(),TR::InstOpCode::Op_cmpi, callNode, cr0Reg, gr31Reg, 0);
         generateConditionalBranchInstruction(cg(), TR::InstOpCode::beq, callNode, refPoolRestartLabel, cr0Reg);
         generateDepImmSymInstruction(cg(), TR::InstOpCode::bl, callNode, (uintptrj_t)collapseSymRef->getMethodAddress(), new (trHeapMemory()) TR::RegisterDependencyConditions(0,0, trMemory()), collapseSymRef, NULL);
         generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, refPoolRestartLabel);
         }

      // Restore the JIT frame
      generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::addi2, callNode, stackPtr, stackPtr, 5*TR::Compiler->om.sizeofReferenceAddress());
      }

   if (checkExceptions)
      {
      generateTrg1MemInstruction(cg(),TR::InstOpCode::Op_load, callNode, gr31Reg, new (trHeapMemory()) TR::MemoryReference(metaReg, fej9->thisThreadGetCurrentExceptionOffset(), TR::Compiler->om.sizeofReferenceAddress(), cg()));
      generateTrg1Src1ImmInstruction(cg(),TR::InstOpCode::Op_cmpi, callNode, cr0Reg, gr31Reg, 0);

      TR::SymbolReference *throwSymRef = comp()->getSymRefTab()->findOrCreateThrowCurrentExceptionSymbolRef(comp()->getJittedMethodSymbol());
      TR::LabelSymbol *exceptionSnippetLabel = cg()->lookUpSnippet(TR::Snippet::IsHelperCall, throwSymRef);
      if (exceptionSnippetLabel == NULL)
         {
         exceptionSnippetLabel = generateLabelSymbol(cg());
         cg()->addSnippet(new (trHeapMemory()) TR::PPCHelperCallSnippet(cg(), callNode, exceptionSnippetLabel, throwSymRef));
         }
      gcPoint = generateConditionalBranchInstruction(cg(), TR::InstOpCode::bnel, callNode, exceptionSnippetLabel, cr0Reg);
      gcPoint->PPCNeedsGCMap(0x10000000);
      }

   TR::LabelSymbol *depLabel = generateLabelSymbol(cg());
   generateDepLabelInstruction(cg(), TR::InstOpCode::label, callNode, depLabel, deps->cloneAndFix(cg()));

   callNode->setRegister(returnRegister);

   cg()->freeAndResetTransientLongs();
   deps->stopUsingDepRegs(cg(), lowReg == NULL ? returnRegister : highReg, lowReg);

   return returnRegister;
   }

// tempReg0 to tempReg2 are temporary registers
void TR::PPCJNILinkage::releaseVMAccess(TR::Node* callNode, TR::RegisterDependencyConditions* deps, TR::RealRegister* metaReg, TR::Register* tempReg0, TR::Register* tempReg1, TR::Register* tempReg2)
   {
   // Release vm access - use hardware registers because of the control flow
   TR::Instruction  *gcPoint;
   const TR::PPCLinkageProperties &properties = getProperties();

   TR::Register *gr28Reg = cg()->allocateRegister();
   TR::Register *gr29Reg = cg()->allocateRegister();
   TR::Register *cr0Reg = deps->searchPreConditionRegister(TR::RealRegister::cr0);

   addDependency(deps, gr28Reg, TR::RealRegister::gr28, TR_GPR, cg());
   addDependency(deps, gr29Reg, TR::RealRegister::gr29, TR_GPR, cg());

   intptrj_t aValue;

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   aValue = fej9->constReleaseVMAccessOutOfLineMask();
   TR_ASSERT(aValue<=0x7fffffff, "Value assumption broken.");
   // use loadIntConstantFixed - fixed instruction count 2 with int32_t argument
   cg()->loadIntConstantFixed(callNode, (int32_t) aValue, tempReg1);

   aValue = fej9->constReleaseVMAccessMask();
   // use loadAddressConstantFixed - fixed instruction count 2 32-bit, or 5 64-bit
   cg()->loadAddressConstantFixed(callNode, aValue, tempReg0, NULL, NULL, -1, false);
   generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::addi2, callNode, gr28Reg, metaReg,
                                              fej9->thisThreadGetPublicFlagsOffset());
   generateInstruction(cg(), TR::InstOpCode::lwsync, callNode); // This is necessary for the fast path but redundant for the slow path
   TR::LabelSymbol *loopHead = generateLabelSymbol(cg());
   generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, loopHead);
   generateTrg1MemInstruction(cg(),TR::InstOpCode::Op_larx, callNode, tempReg2, new (trHeapMemory()) TR::MemoryReference(NULL, gr28Reg, TR::Compiler->om.sizeofReferenceAddress(), cg()));
   generateTrg1Src2Instruction(cg(), TR::InstOpCode::and_r, callNode, gr29Reg, tempReg2, tempReg1);
   generateTrg1Src2Instruction(cg(), TR::InstOpCode::AND, callNode, tempReg2, tempReg2, tempReg0);

   TR::LabelSymbol *longReleaseLabel = generateLabelSymbol(cg());
   TR::LabelSymbol *longReleaseSnippetLabel;
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg());
   TR::SymbolReference *relVMSymRef = comp()->getSymRefTab()->findOrCreateReleaseVMAccessSymbolRef(comp()->getJittedMethodSymbol());
   longReleaseSnippetLabel = cg()->lookUpSnippet(TR::Snippet::IsHelperCall, relVMSymRef);
   if (longReleaseSnippetLabel == NULL)
      {
      longReleaseSnippetLabel = generateLabelSymbol(cg());
      cg()->addSnippet(new (trHeapMemory()) TR::PPCHelperCallSnippet(cg(), callNode, longReleaseSnippetLabel, relVMSymRef));
      }

   generateConditionalBranchInstruction(cg(), TR::InstOpCode::bne, callNode, longReleaseLabel, cr0Reg);
   generateMemSrc1Instruction(cg(),TR::InstOpCode::Op_stcx_r, callNode, new (trHeapMemory()) TR::MemoryReference(NULL, gr28Reg, TR::Compiler->om.sizeofReferenceAddress(), cg()), tempReg2);

   if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
      // use PPC AS branch hint
      generateConditionalBranchInstruction(cg(), TR::InstOpCode::bne, PPCOpProp_BranchUnlikely, callNode, loopHead, cr0Reg);
   else
      generateConditionalBranchInstruction(cg(), TR::InstOpCode::bne, callNode, loopHead, cr0Reg);

   generateLabelInstruction(cg(), TR::InstOpCode::b, callNode, doneLabel);
   generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, longReleaseLabel);
   gcPoint = generateLabelInstruction(cg(), TR::InstOpCode::bl, callNode, longReleaseSnippetLabel);
   gcPoint->PPCNeedsGCMap(~(properties.getPreservedRegisterMapForGC()));
   generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, doneLabel);
   // end of release vm access (spin lock)
   }

// tempReg0 to tempReg2 are temporary registers
void TR::PPCJNILinkage::acquireVMAccess(TR::Node* callNode, TR::RegisterDependencyConditions* deps, TR::RealRegister* metaReg, TR::Register* tempReg0, TR::Register* tempReg1, TR::Register* tempReg2)
   {
   // Acquire VM Access
   TR::Instruction  *gcPoint;

   TR::Register *cr0Reg = deps->searchPreConditionRegister(TR::RealRegister::cr0);

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   loadConstant(cg(), callNode, (int32_t)fej9->constAcquireVMAccessOutOfLineMask(), tempReg1);
   generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::addi2, callNode, tempReg0, metaReg,
                                              fej9->thisThreadGetPublicFlagsOffset());
   TR::LabelSymbol *loopHead2 = generateLabelSymbol(cg());
   generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, loopHead2);
   generateTrg1MemInstruction(cg(),TR::InstOpCode::Op_larx, PPCOpProp_LoadReserveExclusiveAccess, callNode, tempReg2, new (trHeapMemory()) TR::MemoryReference(NULL, tempReg0, TR::Compiler->om.sizeofReferenceAddress(), cg()));
   generateTrg1Src1ImmInstruction(cg(),TR::InstOpCode::Op_cmpi, callNode, cr0Reg, tempReg2, 0);
   TR::LabelSymbol *longReacquireLabel = generateLabelSymbol(cg());
   TR::LabelSymbol *longReacquireSnippetLabel;
   TR::LabelSymbol *doneLabel2 = generateLabelSymbol(cg());
   TR::SymbolReference *acqVMSymRef = comp()->getSymRefTab()->findOrCreateAcquireVMAccessSymbolRef(comp()->getJittedMethodSymbol());
   longReacquireSnippetLabel = cg()->lookUpSnippet(TR::Snippet::IsHelperCall, acqVMSymRef);
   if (longReacquireSnippetLabel == NULL)
      {
      longReacquireSnippetLabel = generateLabelSymbol(cg());
      cg()->addSnippet(new (trHeapMemory()) TR::PPCHelperCallSnippet(cg(), callNode, longReacquireSnippetLabel, acqVMSymRef));
      }

   generateConditionalBranchInstruction(cg(), TR::InstOpCode::bne, callNode, longReacquireLabel, cr0Reg);
   generateMemSrc1Instruction(cg(),TR::InstOpCode::Op_stcx_r, callNode, new (trHeapMemory()) TR::MemoryReference(NULL, tempReg0, TR::Compiler->om.sizeofReferenceAddress(), cg()), tempReg1);

   if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
      // use PPC AS branch hint
      generateConditionalBranchInstruction(cg(), TR::InstOpCode::bne, PPCOpProp_BranchUnlikely, callNode, loopHead2, cr0Reg);
   else
      generateConditionalBranchInstruction(cg(), TR::InstOpCode::bne, callNode, loopHead2, cr0Reg);

   generateInstruction(cg(), TR::InstOpCode::isync, callNode);
   generateLabelInstruction(cg(), TR::InstOpCode::b, callNode, doneLabel2);
   generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, longReacquireLabel);
   gcPoint = generateLabelInstruction(cg(), TR::InstOpCode::bl, callNode, longReacquireSnippetLabel);
   gcPoint->PPCNeedsGCMap(0x00000000);
   generateLabelInstruction(cg(), TR::InstOpCode::label, callNode, doneLabel2);
   // end of reacquire VM Access
   }

#ifdef J9VM_INTERP_ATOMIC_FREE_JNI
void TR::PPCJNILinkage::releaseVMAccessAtomicFree(TR::Node* callNode, TR::RegisterDependencyConditions* deps, TR::RealRegister* metaReg, TR::Register* cr0Reg, TR::Register* tempReg1, TR::Register* tempReg2)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe();

   generateInstruction(cg(), TR::InstOpCode::lwsync, callNode);
   generateTrg1MemInstruction(cg(), TR::InstOpCode::Op_load, callNode, tempReg1, new (trHeapMemory()) TR::MemoryReference(metaReg, fej9->thisThreadGetPublicFlagsOffset(), TR::Compiler->om.sizeofReferenceAddress(), cg()));
   if (J9_PUBLIC_FLAGS_HALT_THREAD_ANY >= LOWER_IMMED && J9_PUBLIC_FLAGS_HALT_THREAD_ANY <= UPPER_IMMED)
      generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::andi_r, callNode, tempReg2, tempReg1, cr0Reg, J9_PUBLIC_FLAGS_HALT_THREAD_ANY);
   else
      {
      loadConstant(cg(), callNode, J9_PUBLIC_FLAGS_HALT_THREAD_ANY, tempReg2);
      // FIXME: Apparently I'm the first one to ever use a Trg1Src2 record-form instruction...
      // ctor for Trg1Src2 + condReg only has the preced form, if you don't pass a preceding instruction the following instruction will end up as the first instruction after the prologue
      generateTrg1Src2Instruction(cg(), TR::InstOpCode::and_r, callNode, tempReg2, tempReg1, tempReg2, cr0Reg, /* FIXME */ cg()->getAppendInstruction());
      }

   TR::SymbolReference *jitReleaseVMAccessSymRef = comp()->getSymRefTab()->findOrCreateReleaseVMAccessSymbolRef(comp()->getJittedMethodSymbol());
   OMR::LabelSymbol *releaseVMAcessSnippetLabel = cg()->lookUpSnippet(TR::Snippet::IsHelperCall, jitReleaseVMAccessSymRef);
   if (!releaseVMAcessSnippetLabel)
      {
      releaseVMAcessSnippetLabel = generateLabelSymbol(cg());
      cg()->addSnippet(new (trHeapMemory()) TR::PPCHelperCallSnippet(cg(), callNode, releaseVMAcessSnippetLabel, jitReleaseVMAccessSymRef));
      }

   if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
      generateConditionalBranchInstruction(cg(), TR::InstOpCode::bnel, PPCOpProp_BranchUnlikely, callNode, releaseVMAcessSnippetLabel, cr0Reg);
   else
      generateConditionalBranchInstruction(cg(), TR::InstOpCode::bnel, callNode, releaseVMAcessSnippetLabel, cr0Reg);

   generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, callNode, tempReg1, 1);
   generateMemSrc1Instruction(cg(), TR::InstOpCode::Op_st, callNode, new (trHeapMemory()) TR::MemoryReference(metaReg, (int32_t)offsetof(struct J9VMThread, inNative), TR::Compiler->om.sizeofReferenceAddress(), cg()), tempReg1);
   generateInstruction(cg(), TR::InstOpCode::lwsync, callNode); // Necessary?
   }

void TR::PPCJNILinkage::acquireVMAccessAtomicFree(TR::Node* callNode, TR::RegisterDependencyConditions* deps, TR::RealRegister* metaReg, TR::Register* cr0Reg, TR::Register* tempReg1, TR::Register* tempReg2)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe();

   generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, callNode, tempReg1, 0);
   generateMemSrc1Instruction(cg(), TR::InstOpCode::Op_st, callNode, new (trHeapMemory()) TR::MemoryReference(metaReg, (int32_t)offsetof(struct J9VMThread, inNative), TR::Compiler->om.sizeofReferenceAddress(), cg()), tempReg1);
   generateInstruction(cg(), TR::InstOpCode::sync, callNode); // Necessary?
   generateTrg1MemInstruction(cg(), TR::InstOpCode::Op_load, callNode, tempReg1, new (trHeapMemory()) TR::MemoryReference(metaReg, fej9->thisThreadGetPublicFlagsOffset(), TR::Compiler->om.sizeofReferenceAddress(), cg()));
   if (J9_PUBLIC_FLAGS_VM_ACCESS >= LOWER_IMMED && J9_PUBLIC_FLAGS_VM_ACCESS <= UPPER_IMMED)
      generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::Op_cmpli, callNode, cr0Reg, tempReg1, J9_PUBLIC_FLAGS_VM_ACCESS);
   else
      {
      loadConstant(cg(), callNode, J9_PUBLIC_FLAGS_VM_ACCESS, tempReg2);
      generateTrg1Src2Instruction(cg(), TR::InstOpCode::Op_cmpli, callNode, cr0Reg, tempReg1, tempReg2);
      }

   TR::SymbolReference *jitAcquireVMAccessSymRef = comp()->getSymRefTab()->findOrCreateAcquireVMAccessSymbolRef(comp()->getJittedMethodSymbol());
   OMR::LabelSymbol *acquireVMAcessSnippetLabel = cg()->lookUpSnippet(TR::Snippet::IsHelperCall, jitAcquireVMAccessSymRef);
   if (!acquireVMAcessSnippetLabel)
      {
      acquireVMAcessSnippetLabel = generateLabelSymbol(cg());
      cg()->addSnippet(new (trHeapMemory()) TR::PPCHelperCallSnippet(cg(), callNode, acquireVMAcessSnippetLabel, jitAcquireVMAccessSymRef));
      }

   if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
      generateConditionalBranchInstruction(cg(), TR::InstOpCode::bnel, PPCOpProp_BranchUnlikely, callNode, acquireVMAcessSnippetLabel, cr0Reg);
   else
      generateConditionalBranchInstruction(cg(), TR::InstOpCode::bnel, callNode, acquireVMAcessSnippetLabel, cr0Reg);
   }
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */

int32_t TR::PPCJNILinkage::buildJNIArgs(TR::Node *callNode,
                                       TR::RegisterDependencyConditions *dependencies,
                                       const TR::PPCLinkageProperties &properties,
                                       bool isFastJNI,
                                       bool passReceiver,
                                       bool implicitEnvArg)

   {
   //TODO: Temporary clone of PPCSystemLinkage::buildArgs. Both PPCSystemLinkage::buildArgs and
   //this buildJNIArgs will be refactored for commonality.

   TR::PPCMemoryArgument *pushToMemory = NULL;
   TR::Register       *tempRegister;
   int32_t   argIndex = 0, memArgs = 0, i;
   int32_t   argSize = 0;
   uint32_t  numIntegerArgs = 0;
   uint32_t  numFloatArgs = 0;

   TR::Node  *child;
   void     *smark;
   TR::DataType resType = callNode->getType();
   TR_Array<TR::Register *> &tempLongRegisters = cg()->getTransientLongRegisters();
   TR::Symbol * callSymbol = callNode->getSymbolReference()->getSymbol();

   uint32_t firstArgumentChild = callNode->getFirstArgumentIndex();

   bool aix_style_linkage = (TR::Compiler->target.isAIX() || (TR::Compiler->target.is64Bit() && TR::Compiler->target.isLinux()));

   if (isFastJNI)  // Account for extra parameters (env and obj)
      {
      if (implicitEnvArg)
         numIntegerArgs += 1;
      if (!passReceiver)
         {
         // Evaluate as usual if necessary, but don't actually pass it to the callee
         TR::Node *firstArgChild = callNode->getChild(firstArgumentChild);
         if (firstArgChild->getReferenceCount() > 1)
            {
            switch (firstArgChild->getDataType())
               {
               case TR::Int32:
                  pushIntegerWordArg(firstArgChild);
                  break;
               case TR::Int64:
                  pushLongArg(firstArgChild);
                  break;
               case TR::Address:
                  pushAddressArg(firstArgChild);
                  break;
               default:
                  TR_ASSERT( false, "Unexpected first child type");
               }
            }
         else
            cg()->recursivelyDecReferenceCount(firstArgChild);
         firstArgumentChild += 1;
         }
      }

   /* Step 1 - figure out how many arguments are going to be spilled to memory i.e. not in registers */
   for (i = firstArgumentChild; i < callNode->getNumChildren(); i++)
      {
      child = callNode->getChild(i);
      switch (child->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Address:
            if (numIntegerArgs >= properties.getNumIntArgRegs())
               memArgs++;
            numIntegerArgs++;
            break;
         case TR::Int64:
            if (TR::Compiler->target.is64Bit())
               {
               if (numIntegerArgs >= properties.getNumIntArgRegs())
                  memArgs++;
               numIntegerArgs++;
               }
            else
               {
               if (aix_style_linkage)
                  {
                  if (numIntegerArgs == properties.getNumIntArgRegs()-1)
                     memArgs++;
                  else if (numIntegerArgs > properties.getNumIntArgRegs()-1)
                     memArgs += 2;
                  }
               else
                  {
                  if (numIntegerArgs & 1)
                     numIntegerArgs++;
                  if (numIntegerArgs >= properties.getNumIntArgRegs())
                     memArgs += 2;
                  }
               numIntegerArgs += 2;
               }
            break;
         case TR::Float:
            if (aix_style_linkage)
               {
               if (numIntegerArgs >= properties.getNumIntArgRegs())
                  memArgs++;
               numIntegerArgs++;
               }
            else
               {
               if (numFloatArgs >= properties.getNumFloatArgRegs())
                  memArgs++;
               }
            numFloatArgs++;
            break;
         case TR::Double:
            if (aix_style_linkage)
               {
               if (TR::Compiler->target.is64Bit())
                  {
                  if (numIntegerArgs >= properties.getNumIntArgRegs())
                     memArgs++;
                  numIntegerArgs++;
                  }
               else
                  {
                  if (numIntegerArgs >= properties.getNumIntArgRegs()-1)
                     memArgs++;
                  numIntegerArgs += 2;
                  }
               }
            else
               {
               if (numFloatArgs >= properties.getNumFloatArgRegs())
                  memArgs++;
               }
            numFloatArgs++;
            break;
         case TR::VectorDouble:
            TR_ASSERT(false, "JNI dispatch: VectorDouble argument not expected");
            break;
         case TR::Aggregate:
            {
            size_t size = child->getSymbolReference()->getSymbol()->getSize();
            size = (size + sizeof(uintptr_t) - 1) & (~(sizeof(uintptr_t) - 1)); // round up the size
            size_t slots = size / sizeof(uintptr_t);

            if (numIntegerArgs >= properties.getNumIntArgRegs())
               memArgs += slots;
            else
               memArgs += (properties.getNumIntArgRegs() - numIntegerArgs) > slots ? 0: slots - (properties.getNumIntArgRegs() - numIntegerArgs);
            numIntegerArgs += slots;
            }
            break;
         default:
            TR_ASSERT(false, "Argument type %s is not supported\n", child->getDataType().toString());
         }
      }

   // From here, down, any new stack allocations will expire / die when the function returns
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   /* End result of Step 1 - determined number of memory arguments! */
   if (memArgs > 0)
      {
      pushToMemory = new (trStackMemory()) TR::PPCMemoryArgument[memArgs];
      }

   numIntegerArgs = 0;
   numFloatArgs = 0;

   if (isFastJNI && implicitEnvArg)  // Account for extra parameter (env)
      {
      // first argument is JNIenv
      numIntegerArgs += 1;
      if (aix_style_linkage)
         argSize += TR::Compiler->om.sizeofReferenceAddress();
      }

   for (i = firstArgumentChild; i < callNode->getNumChildren(); i++)
      {
      TR::MemoryReference *mref=NULL;
      TR::Register        *argRegister;
      bool                 checkSplit = true;

      child = callNode->getChild(i);
      TR::DataType childType = child->getDataType();

      switch (childType)
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Address: // have to do something for GC maps here
            if (isFastJNI && childType==TR::Address)
               {
               argRegister = pushJNIReferenceArg(child);
               checkSplit = false;
               }
            else
               if (childType == TR::Address && !isFastJNI)
                  argRegister = pushAddressArg(child);
               else
                  argRegister = pushIntegerWordArg(child);

            if (numIntegerArgs < properties.getNumIntArgRegs())
               {
               // Sign extend non-64bit Integers on LinuxPPC64 as required by the ABI
               // The AIX64 ABI also specifies this behaviour but we observed otherwise
               // We can do this blindly in Java since there is no unsigned types
               // WCode will have to do something better!
               if (isFastJNI &&
                   (TR::Compiler->target.isLinux() && TR::Compiler->target.is64Bit()) &&
                   childType != TR::Address)
                  generateTrg1Src1Instruction(cg(), TR::InstOpCode::extsw, callNode, argRegister, argRegister);

               if (checkSplit && !cg()->canClobberNodesRegister(child, 0))
                  {
                  if (argRegister->containsCollectedReference())
                     tempRegister = cg()->allocateCollectedReferenceRegister();
                  else
                     tempRegister = cg()->allocateRegister();
                  generateTrg1Src1Instruction(cg(), TR::InstOpCode::mr, callNode, tempRegister, argRegister);
                  argRegister = tempRegister;
                  }
               if (numIntegerArgs == 0 &&
                  (resType.isAddress() || resType.isInt32() || resType.isInt64()))
                  {
                  TR::Register *resultReg;
                  if (resType.isAddress())
                     resultReg = cg()->allocateCollectedReferenceRegister();
                  else
                     resultReg = cg()->allocateRegister();
                  dependencies->addPreCondition(argRegister, TR::RealRegister::gr3);
                  dependencies->addPostCondition(resultReg, TR::RealRegister::gr3);
                  }
               else if (TR::Compiler->target.is32Bit() && numIntegerArgs == 1 && resType.isInt64())
                  {
                  TR::Register *resultReg = cg()->allocateRegister();
                  dependencies->addPreCondition(argRegister, TR::RealRegister::gr4);
                  dependencies->addPostCondition(resultReg, TR::RealRegister::gr4);
                  }
               else
                  {
                  addDependency(dependencies, argRegister, properties.getIntegerArgumentRegister(numIntegerArgs), TR_GPR, cg());
                  }
               }
            else // numIntegerArgs >= properties.getNumIntArgRegs()
               {
               mref = getOutgoingArgumentMemRef(argSize, argRegister,TR::InstOpCode::Op_st, pushToMemory[argIndex++], TR::Compiler->om.sizeofReferenceAddress(), properties);
               //printf("integral or address memory arg, offset = %d\n", argSize);
               if (!aix_style_linkage)
                  argSize += TR::Compiler->om.sizeofReferenceAddress();
               }
            numIntegerArgs++;
            if (aix_style_linkage)
               argSize += TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR::Int64:
            argRegister = pushLongArg(child);
            if (!aix_style_linkage)
               {
               if (numIntegerArgs & 1)
                  {
                  if (numIntegerArgs < properties.getNumIntArgRegs())
                     addDependency(dependencies, NULL, properties.getIntegerArgumentRegister(numIntegerArgs), TR_GPR, cg());
                  numIntegerArgs++;
                  }
               }
            if (numIntegerArgs < properties.getNumIntArgRegs())
               {
               if (!cg()->canClobberNodesRegister(child, 0))
                  {
                  if (TR::Compiler->target.is64Bit())
                     {
                     tempRegister = cg()->allocateRegister();
                     generateTrg1Src1Instruction(cg(), TR::InstOpCode::mr, callNode, tempRegister, argRegister);
                     argRegister = tempRegister;
                     }
                  else
                     {
                     tempRegister = cg()->allocateRegister();
                     generateTrg1Src1Instruction(cg(), TR::InstOpCode::mr, callNode, tempRegister, argRegister->getRegisterPair()->getHighOrder());
                     argRegister = cg()->allocateRegisterPair(argRegister->getRegisterPair()->getLowOrder(), tempRegister);
                     tempLongRegisters.add(argRegister);
                     }
                  }
               if (numIntegerArgs == 0 &&
                   (resType.isAddress() || resType.isInt32() || resType.isInt64()))
                  {
                  TR::Register *resultReg;
                  if (resType.isAddress())
                     resultReg = cg()->allocateCollectedReferenceRegister();
                  else
                     resultReg = cg()->allocateRegister();
                  if (TR::Compiler->target.is64Bit())
                     dependencies->addPreCondition(argRegister, TR::RealRegister::gr3);
                  else
                     dependencies->addPreCondition(argRegister->getRegisterPair()->getHighOrder(), TR::RealRegister::gr3);
                  dependencies->addPostCondition(resultReg, TR::RealRegister::gr3);
                  }
               else if (TR::Compiler->target.is32Bit() && numIntegerArgs == 1 && resType.isInt64())
                  {
                  TR::Register *resultReg = cg()->allocateRegister();
                  dependencies->addPreCondition(argRegister, TR::RealRegister::gr4);
                  dependencies->addPostCondition(resultReg, TR::RealRegister::gr4);
                  }
               else
                  {
                  if (TR::Compiler->target.is64Bit())
                     addDependency(dependencies, argRegister, properties.getIntegerArgumentRegister(numIntegerArgs), TR_GPR, cg());
                  else
                     addDependency(dependencies, argRegister->getRegisterPair()->getHighOrder(), properties.getIntegerArgumentRegister(numIntegerArgs), TR_GPR, cg());
                  }
               if (TR::Compiler->target.is32Bit())
                  {
                  if (numIntegerArgs < properties.getNumIntArgRegs()-1)
                     {
                     if (!cg()->canClobberNodesRegister(child, 0))
                        {
                        TR::Register *over_lowReg = argRegister->getRegisterPair()->getLowOrder();
                        tempRegister = cg()->allocateRegister();
                        generateTrg1Src1Instruction(cg(), TR::InstOpCode::mr, callNode, tempRegister, over_lowReg);
                        argRegister->getRegisterPair()->setLowOrder(tempRegister, cg());
                        }
                     if (numIntegerArgs == 0 && resType.isInt64())
                        {
                        TR::Register *resultReg = cg()->allocateRegister();
                        dependencies->addPreCondition(argRegister->getRegisterPair()->getLowOrder(), TR::RealRegister::gr4);
                        dependencies->addPostCondition(resultReg, TR::RealRegister::gr4);
                        }
                     else
                        addDependency(dependencies, argRegister->getRegisterPair()->getLowOrder(), properties.getIntegerArgumentRegister(numIntegerArgs+1), TR_GPR, cg());
                     }
                  else // numIntegerArgs == properties.getNumIntArgRegs()-1
                     {
                     mref = getOutgoingArgumentMemRef(argSize+4, argRegister->getRegisterPair()->getLowOrder(), TR::InstOpCode::stw, pushToMemory[argIndex++], 4, properties);
                     }
                  numIntegerArgs ++;
                  }
               }
            else // numIntegerArgs >= properties.getNumIntArgRegs()
               {
               if (TR::Compiler->target.is64Bit())
                  {
                  mref = getOutgoingArgumentMemRef(argSize, argRegister, TR::InstOpCode::std, pushToMemory[argIndex++], TR::Compiler->om.sizeofReferenceAddress(), properties);
                  }
               else
                  {
                  if (!aix_style_linkage)
                     argSize = (argSize + 4) & (~7);
                  mref = getOutgoingArgumentMemRef(argSize, argRegister->getRegisterPair()->getHighOrder(), TR::InstOpCode::stw, pushToMemory[argIndex++], 4, properties);
                  mref = getOutgoingArgumentMemRef(argSize+4, argRegister->getRegisterPair()->getLowOrder(), TR::InstOpCode::stw, pushToMemory[argIndex++], 4, properties);
                  numIntegerArgs ++;
                  if (!aix_style_linkage)
                     argSize += 8;
                  }
               }
            numIntegerArgs ++;
            if (aix_style_linkage)
               argSize += 8;
            break;

         case TR::Float:
            argRegister = pushFloatArg(child);
            for (int r = 0; r < ((childType == TR::Float) ? 1: 2); r++)
            {
            TR::Register * argReg;
            if (childType == TR::Float)
               argReg = argRegister;
            else
               argReg = (r == 0) ? argRegister->getHighOrder() : argRegister->getLowOrder();

            if (numFloatArgs < properties.getNumFloatArgRegs())
               {
               if (!cg()->canClobberNodesRegister(child, 0))
                  {
                  tempRegister = cg()->allocateRegister(TR_FPR);
                  generateTrg1Src1Instruction(cg(), TR::InstOpCode::fmr, callNode, tempRegister, argReg);
                  argReg = tempRegister;
                  }
               if (numFloatArgs == 0 && resType.isFloatingPoint())
                  {
                  TR::Register *resultReg;
                  if (resType.getDataType() == TR::Float)
                     resultReg = cg()->allocateSinglePrecisionRegister();
                  else
                     resultReg = cg()->allocateRegister(TR_FPR);

                  dependencies->addPreCondition(argReg, (r == 0) ? TR::RealRegister::fp1 : TR::RealRegister::fp2);
                  dependencies->addPostCondition(resultReg, (r == 0) ? TR::RealRegister::fp1 : TR::RealRegister::fp2);
                  }
               else
                  addDependency(dependencies, argReg, properties.getFloatArgumentRegister(numFloatArgs), TR_FPR, cg());
               }
            else if (!aix_style_linkage)
               // numFloatArgs >= properties.getNumFloatArgRegs()
               {
               mref = getOutgoingArgumentMemRef(argSize, argReg, TR::InstOpCode::stfs, pushToMemory[argIndex++], 4, properties);
               argSize += 4;
               }

            if (aix_style_linkage)
               {
               if (numIntegerArgs < properties.getNumIntArgRegs())
                  {
                  if (numIntegerArgs==0 && resType.isAddress())
                     {
                     TR::Register *aReg = cg()->allocateRegister();
                     TR::Register *bReg = cg()->allocateCollectedReferenceRegister();
                     dependencies->addPreCondition(aReg, TR::RealRegister::gr3);
                     dependencies->addPostCondition(bReg, TR::RealRegister::gr3);
                     }
                  else
                     addDependency(dependencies, NULL, properties.getIntegerArgumentRegister(numIntegerArgs), TR_GPR, cg());
                  }
               else // numIntegerArgs >= properties.getNumIntArgRegs()
                  {
                  if (TR::Compiler->target.is64Bit() && TR::Compiler->target.isLinux())
                     {
                     mref = getOutgoingArgumentMemRef(argSize+4, argReg, TR::InstOpCode::stfs, pushToMemory[argIndex++], 4, properties);
                     }
                  else
                     {
                     mref = getOutgoingArgumentMemRef(argSize, argReg, TR::InstOpCode::stfs, pushToMemory[argIndex++], 4, properties);
                     }
                  }

               numIntegerArgs++;
               }
            numFloatArgs++;
            if (aix_style_linkage)
               argSize += TR::Compiler->om.sizeofReferenceAddress();

            }  // for loop
            break;

         case TR::Double:
            argRegister = pushDoubleArg(child);
            for (int r = 0; r < ((childType == TR::Double) ? 1: 2); r++)
            {
            TR::Register * argReg;
            if (childType == TR::Double)
               argReg = argRegister;
            else
               argReg = (r == 0) ? argRegister->getHighOrder() : argRegister->getLowOrder();

            if (numFloatArgs < properties.getNumFloatArgRegs())
               {
               if (!cg()->canClobberNodesRegister(child, 0))
                  {
                  tempRegister = cg()->allocateRegister(TR_FPR);
                  generateTrg1Src1Instruction(cg(), TR::InstOpCode::fmr, callNode, tempRegister, argReg);
                  argReg = tempRegister;
                  }
               if (numFloatArgs == 0 && resType.isFloatingPoint())
                  {
                  TR::Register *resultReg;
                  if (resType.getDataType() == TR::Float)
                     resultReg = cg()->allocateSinglePrecisionRegister();
                  else
                     resultReg = cg()->allocateRegister(TR_FPR);
                  dependencies->addPreCondition(argReg, (r==0) ? TR::RealRegister::fp1 : TR::RealRegister::fp2);
                  dependencies->addPostCondition(resultReg, (r==0) ? TR::RealRegister::fp1 : TR::RealRegister::fp2);
                  }
               else
                  addDependency(dependencies, argReg, properties.getFloatArgumentRegister(numFloatArgs), TR_FPR, cg());
               }
            else if (!aix_style_linkage)
               // numFloatArgs >= properties.getNumFloatArgRegs()
               {
               argSize = (argSize + 4) & (~7);
               mref = getOutgoingArgumentMemRef(argSize, argReg, TR::InstOpCode::stfd, pushToMemory[argIndex++], 8, properties);
               argSize += 8;
               }

            if (aix_style_linkage)
               {
               if (numIntegerArgs < properties.getNumIntArgRegs())
                  {
                  TR::MemoryReference *tempMR;

                  if (numIntegerArgs==0 && resType.isAddress())
                     {
                     TR::Register *aReg = cg()->allocateRegister();
                     TR::Register *bReg = cg()->allocateCollectedReferenceRegister();
                     dependencies->addPreCondition(aReg, TR::RealRegister::gr3);
                     dependencies->addPostCondition(bReg, TR::RealRegister::gr3);
                     }
                  else
                        addDependency(dependencies, NULL, properties.getIntegerArgumentRegister(numIntegerArgs), TR_GPR, cg());

                  if (TR::Compiler->target.is32Bit())
                     {
                     if ((numIntegerArgs+1) < properties.getNumIntArgRegs())
                           addDependency(dependencies, NULL, properties.getIntegerArgumentRegister(numIntegerArgs+1), TR_GPR, cg());
                     else
                        {
                        mref = getOutgoingArgumentMemRef(argSize, argReg, TR::InstOpCode::stfd, pushToMemory[argIndex++], 8, properties);
                        }
                     }
                  }
               else // numIntegerArgs >= properties.getNumIntArgRegs()
                  {
                  mref = getOutgoingArgumentMemRef(argSize, argReg, TR::InstOpCode::stfd, pushToMemory[argIndex++], 8, properties);
                  }

               numIntegerArgs += TR::Compiler->target.is64Bit()?1:2;
               }
            numFloatArgs++;
            if (aix_style_linkage)
               argSize += 8;

            }    // end of for loop
            break;
         case TR::VectorDouble:
            TR_ASSERT(false, "JNI dispatch: VectorDouble argument not expected");
            break;
         }
      }

   while (numIntegerArgs < properties.getNumIntArgRegs())
      {
      if (numIntegerArgs == 0 && resType.isAddress())
         {
         dependencies->addPreCondition(cg()->allocateRegister(), properties.getIntegerArgumentRegister(0));
         dependencies->addPostCondition(cg()->allocateCollectedReferenceRegister(), properties.getIntegerArgumentRegister(0));
         }
      else
         {
         addDependency(dependencies, NULL, properties.getIntegerArgumentRegister(numIntegerArgs), TR_GPR, cg());
         }
      numIntegerArgs++;
      }

   TR_LiveRegisters *liveRegs;
   bool liveVSXScalar, liveVSXVector, liveVMX;

   liveRegs = cg()->getLiveRegisters(TR_VSX_SCALAR);
   liveVSXScalar = (!liveRegs || liveRegs->getNumberOfLiveRegisters() > 0);
   liveRegs = cg()->getLiveRegisters(TR_VSX_VECTOR);
   liveVSXVector = (!liveRegs || liveRegs->getNumberOfLiveRegisters() > 0);
   liveRegs = cg()->getLiveRegisters(TR_VRF);
   liveVMX = (!liveRegs || liveRegs->getNumberOfLiveRegisters() > 0);

   addDependency(dependencies, NULL, TR::RealRegister::fp0, TR_FPR, cg());
   addDependency(dependencies, NULL, TR::RealRegister::gr0, TR_GPR, cg());
   addDependency(dependencies, NULL, TR::RealRegister::gr11, TR_GPR, cg());
   addDependency(dependencies, NULL, TR::RealRegister::gr12, TR_GPR, cg());
   addDependency(dependencies, NULL, TR::RealRegister::cr0, TR_CCR, cg());
   addDependency(dependencies, NULL, TR::RealRegister::cr1, TR_CCR, cg());
   addDependency(dependencies, NULL, TR::RealRegister::cr5, TR_CCR, cg());
   addDependency(dependencies, NULL, TR::RealRegister::cr6, TR_CCR, cg());
   addDependency(dependencies, NULL, TR::RealRegister::cr7, TR_CCR, cg());
   if (!isFastJNI && aix_style_linkage)
      addDependency(dependencies, NULL, TR::RealRegister::gr2, TR_GPR, cg());

   int32_t floatRegsUsed = (numFloatArgs>properties.getNumFloatArgRegs())?properties.getNumFloatArgRegs():numFloatArgs;

   bool isHelper = false;
   if (callNode->getSymbolReference()->getReferenceNumber() == TR_PPCVectorLogDouble)
      {
      isHelper = true;
      }

   if (liveVMX || liveVSXScalar || liveVSXVector)
      {
      for (i=(TR::RealRegister::RegNum)((uint32_t)TR::RealRegister::LastFPR+1); i<=TR::RealRegister::LastVSR; i++)
         {
         // isFastJNI implying: no call back into Java, such that preserved is preserved
         if (!properties.getPreserved((TR::RealRegister::RegNum)i) ||
             (!isFastJNI  && !isHelper))
            {
            addDependency(dependencies, NULL, (TR::RealRegister::RegNum)i, TR_VSX_SCALAR, cg());
            }

         }
      }

   for (i=(TR::RealRegister::RegNum)((uint32_t)TR::RealRegister::fp0+floatRegsUsed+1); i<=TR::RealRegister::LastFPR; i++)
      {
      // isFastJNI implying: no call back into Java, such that preserved is preserved
      // TODO: liveVSXVector is an overkill for assembler mode, really only vectors required
      if (!properties.getPreserved((TR::RealRegister::RegNum)i) || liveVSXVector ||
          (!isFastJNI))
         {
         addDependency(dependencies, NULL, (TR::RealRegister::RegNum)i, TR_FPR, cg());
         }
      }

   if (memArgs > 0)
      {
      for (argIndex=0; argIndex<memArgs; argIndex++)
         {
         TR::Register *aReg = pushToMemory[argIndex].argRegister;
         generateMemSrc1Instruction(cg(), pushToMemory[argIndex].opCode, callNode, pushToMemory[argIndex].argMemory, aReg);
         cg()->stopUsingRegister(aReg);
         }
      }

   return argSize;
   }

TR::Register *TR::PPCJNILinkage::pushJNIReferenceArg(TR::Node *child)
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
            TR::Register *condReg = cg()->allocateRegister(TR_CCR);
            TR::Register *addrReg = cg()->evaluate(child);
            TR::MemoryReference *tmpMemRef = new (trHeapMemory()) TR::MemoryReference(addrReg, (int32_t)0, TR::Compiler->om.sizeofReferenceAddress(), cg());
            TR::Register *whatReg = cg()->allocateCollectedReferenceRegister();
            TR::LabelSymbol *nonNullLabel = generateLabelSymbol(cg());

            checkSplit = false;
            generateTrg1MemInstruction(cg(),TR::InstOpCode::Op_load, child, whatReg, tmpMemRef);
            if (!cg()->canClobberNodesRegister(child))
               {
               // Since this is a static variable, it is non-collectable.
               TR::Register *tempRegister = cg()->allocateRegister();
               generateTrg1Src1Instruction(cg(), TR::InstOpCode::mr, child, tempRegister, addrReg);
               pushRegister = tempRegister;
               }
            else
               pushRegister = addrReg;
            generateTrg1Src1ImmInstruction(cg(),TR::InstOpCode::Op_cmpi, child, condReg, whatReg, 0);
            generateConditionalBranchInstruction(cg(), TR::InstOpCode::bne, child, nonNullLabel, condReg);
            generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, child, pushRegister, 0);

            TR::RegisterDependencyConditions *conditions = new (trHeapMemory()) TR::RegisterDependencyConditions(3, 3, trMemory());
            addDependency(conditions, pushRegister, TR::RealRegister::NoReg, TR_GPR, cg());
            addDependency(conditions, whatReg, TR::RealRegister::NoReg, TR_GPR, cg());
            addDependency(conditions, condReg, TR::RealRegister::NoReg, TR_CCR, cg());

            generateDepLabelInstruction(cg(), TR::InstOpCode::label, child, nonNullLabel, conditions);
            conditions->stopUsingDepRegs(cg(), pushRegister);
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
            loadConstant(cg(), child, 0, pushRegister);
            cg()->decReferenceCount(child);
            }
         else
            {
            TR::Register *addrReg = cg()->evaluate(child);
            TR::Register *condReg = cg()->allocateRegister(TR_CCR);
            TR::Register *whatReg = cg()->allocateCollectedReferenceRegister();
            TR::LabelSymbol *nonNullLabel = generateLabelSymbol(cg());

            checkSplit = false;
            generateTrg1MemInstruction(cg(),TR::InstOpCode::Op_load, child, whatReg, new (trHeapMemory()) TR::MemoryReference(addrReg, (int32_t)0, TR::Compiler->om.sizeofReferenceAddress(), cg()));
            if (!cg()->canClobberNodesRegister(child))
               {
               // Since this points at a parm or local location, it is non-collectable.
               TR::Register *tempReg = cg()->allocateRegister();
               generateTrg1Src1Instruction(cg(), TR::InstOpCode::mr, child, tempReg, addrReg);
               pushRegister = tempReg;
               }
            else
               pushRegister = addrReg;
            generateTrg1Src1ImmInstruction(cg(),TR::InstOpCode::Op_cmpi, child, condReg, whatReg, 0);
            generateConditionalBranchInstruction(cg(), TR::InstOpCode::bne, child, nonNullLabel, condReg);
            generateTrg1ImmInstruction(cg(), TR::InstOpCode::li, child, pushRegister, 0);

            TR::RegisterDependencyConditions *conditions = new (trHeapMemory()) TR::RegisterDependencyConditions(3, 3, trMemory());
            addDependency(conditions, pushRegister, TR::RealRegister::NoReg, TR_GPR, cg());
            addDependency(conditions, whatReg, TR::RealRegister::NoReg, TR_GPR, cg());
            addDependency(conditions, condReg, TR::RealRegister::NoReg, TR_CCR, cg());

            generateDepLabelInstruction(cg(), TR::InstOpCode::label, child, nonNullLabel, conditions);
            conditions->stopUsingDepRegs(cg(), pushRegister);
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
      generateTrg1Src1Instruction(cg(), TR::InstOpCode::mr, child, tempReg, pushRegister);
      pushRegister = tempReg;
      }
   return pushRegister;
   }
