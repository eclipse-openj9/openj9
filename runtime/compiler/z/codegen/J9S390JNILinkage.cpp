/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#include "z/codegen/J9S390JNILinkage.hpp"

#include "codegen/CodeGenerator.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/CompilerEnv.hpp"
#include "env/VMJ9.h"
#include "env/jittypes.h"
#include "env/j9method.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390HelperCallSnippet.hpp"
#include "z/codegen/S390J9CallSnippet.hpp"
#include "z/codegen/TRSystemLinkage.hpp"
#include "z/codegen/J9S390CHelperLinkage.hpp"

#define JNI_CALLOUT_FRAME_SIZE 5*sizeof(intptrj_t)

TR::J9S390JNILinkage::J9S390JNILinkage(TR::CodeGenerator * cg, TR_S390LinkageConventions elc, TR_LinkageConventions lc)
   :TR::S390PrivateLinkage(cg, elc, lc)
   {
   }

void TR::J9S390JNILinkage::checkException(TR::Node * callNode,
                                          TR::Register* javaLitOffsetReg,
                                          TR::Register* methodMetaDataVirtualRegister)
   {
   TR_J9VMBase *fej9 = static_cast<TR_J9VMBase *>(fe());

   generateRXInstruction(cg(), TR::InstOpCode::getLoadOpCode(), callNode,
                         javaLitOffsetReg,
                         generateS390MemoryReference(methodMetaDataVirtualRegister,
                                                     fej9->thisThreadGetCurrentExceptionOffset(),
                                                     cg()));

   // check exception
   TR::LabelSymbol * exceptionRestartLabel = generateLabelSymbol(self()->cg());
   TR::LabelSymbol * exceptionSnippetLabel = generateLabelSymbol(self()->cg());

   TR::Instruction *gcPoint = generateS390CompareAndBranchInstruction(self()->cg(),
                                                                      TR::InstOpCode::getCmpOpCode(), callNode,
                                                                      javaLitOffsetReg,
                                                                      (signed char)0,
                                                                      TR::InstOpCode::COND_BNE,
                                                                      exceptionSnippetLabel, false, true);
   gcPoint->setNeedsGCMap(0);

   self()->cg()->addSnippet(new (self()->trHeapMemory()) TR::S390HelperCallSnippet(self()->cg(), callNode, exceptionSnippetLabel,
      self()->comp()->getSymRefTab()->findOrCreateThrowCurrentExceptionSymbolRef(self()->comp()->getJittedMethodSymbol()), exceptionRestartLabel));
   generateS390LabelInstruction(self()->cg(), TR::InstOpCode::LABEL, callNode, exceptionRestartLabel);
   }

void
TR::J9S390JNILinkage::collapseJNIReferenceFrame(TR::Node * callNode,
                                                TR::Register* javaLitPoolRealRegister,
                                                TR::Register* methodAddressReg,
                                                TR::Register* javaStackPointerRealRegister)
   {
   TR_J9VMBase *fej9 = static_cast<TR_J9VMBase *>(fe());

   intptr_t flagValue = fej9->constJNIReferenceFrameAllocatedFlags();
   genLoadAddressConstant(cg(), callNode, flagValue, methodAddressReg, NULL, NULL, javaLitPoolRealRegister);
   generateRXInstruction(cg(), TR::InstOpCode::getAndOpCode(), callNode,
                         methodAddressReg,
                         generateS390MemoryReference(javaStackPointerRealRegister,
                                                     fej9->constJNICallOutFrameFlagsOffset() - JNI_CALLOUT_FRAME_SIZE,
                                                     cg()));

   // must check to see if the ref pool was used and clean them up if so--or we
   // leave a bunch of pinned garbage behind that screws up the gc quality forever
   TR::LabelSymbol * refPoolRestartLabel = generateLabelSymbol(cg());
   TR::LabelSymbol * refPoolSnippetLabel = generateLabelSymbol(cg());

   TR::Instruction *gcPoint = generateS390CompareAndBranchInstruction(self()->cg(),
                                                                      TR::InstOpCode::getCmpOpCode(),
                                                                      callNode, methodAddressReg,
                                                                      (signed char)0,
                                                                      TR::InstOpCode::COND_BNE,
                                                                      refPoolSnippetLabel, false, true);
   gcPoint->setNeedsGCMap(0);

   TR::SymbolReference * collapseSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_S390collapseJNIReferenceFrame,
                                                                                       false, false, false);

   cg()->addSnippet(new (trHeapMemory()) TR::S390HelperCallSnippet(cg(), callNode,
      refPoolSnippetLabel, collapseSymRef, refPoolRestartLabel));
   generateS390LabelInstruction(cg(), TR::InstOpCode::LABEL, callNode, refPoolRestartLabel);
   }

void
TR::J9S390JNILinkage::unwrapReturnValue(TR::Node* callNode,
                                        TR::Register* javaReturnRegister,
                                        TR::RegisterDependencyConditions* deps)
   {
   TR::LabelSymbol * tempLabel = generateLabelSymbol(cg());
   TR::Instruction* start = generateS390CompareAndBranchInstruction(cg(), TR::InstOpCode::getCmpOpCode(),
                                                                    callNode, javaReturnRegister,
                                                                    (signed char)0, TR::InstOpCode::COND_BE, tempLabel);

   start->setStartInternalControlFlow();
   generateRXInstruction(cg(), TR::InstOpCode::getLoadOpCode(), callNode, javaReturnRegister,
              generateS390MemoryReference(javaReturnRegister, 0, cg()));

   int32_t regPos = deps->searchPostConditionRegisterPos(javaReturnRegister);
   TR::RealRegister::RegNum realReg = deps->getPostConditions()->getRegisterDependency(regPos)->getRealRegister();
   TR::RegisterDependencyConditions * postDeps = new (trHeapMemory()) TR::RegisterDependencyConditions(0, 1, cg());
   postDeps->addPostCondition(javaReturnRegister, realReg);

   TR::Instruction* end =generateS390LabelInstruction(cg(), TR::InstOpCode::LABEL, callNode, tempLabel);
   end->setEndInternalControlFlow();
   end->setDependencyConditions(postDeps);
   }

void
TR::J9S390JNILinkage::callIFAOffloadCheck(TR::Node* callNode, TR_RuntimeHelper helperIndex)
   {
   TR::SymbolReference * offloadSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(helperIndex, false, false, false);
   TR::Node* helperCallNode = TR::Node::createWithSymRef(TR::call, 1, offloadSymRef);
   TR::Node* ramMethodParam = ramMethodParam = TR::Node::create(TR::aconst, 0);

   void* j9methodAsParam = callNode->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod()->resolvedMethodAddress();
   ramMethodParam->setAddress(reinterpret_cast<intptrj_t>(j9methodAsParam));
   helperCallNode->setAndIncChild(0, ramMethodParam);

   TR::S390CHelperLinkage* cHelperLinkage = static_cast<TR::S390CHelperLinkage*>(cg()->getLinkage(TR_CHelper));
   cHelperLinkage->buildDirectDispatch(helperCallNode, NULL, NULL, true);
   }

/**
  * \details
  *
  *  The 5-slot callout frame is structures as the following:
  *
  *  \verbatim
  *
  *        |-----|
  *        |     |      <-- constJNICallOutFrameSpecialTag() (For jni thunk, constJNICallOutFrameInvisibleTag())
  *  16/32 |-----|
  *        |     |      <-- savedPC. Empty.
  *  12/24 |-----|
  *        |     |      <-- return address for JNI call
  *   8/16 |-----|
  *        |     |      <-- constJNICallOutFrameFlags()
  *   4/8   -----
  *        |     |      <-- ramMethod for the native method
  *         -----       <-- stack pointer
  *
  * \endverbatim
  *
  * If there's IFA offload check, do no store java stack pointer to vmThread because C helper stores JSP to vmThread, overwritting the JNI's JSP.
  * Delay the JSP storing after the IFA call (before the JNI call).
  */
void
TR::J9S390JNILinkage::setupJNICallOutFrameIfNeeded(TR::Node * callNode,
                                                   bool isJNICallOutFrame,
                                                   bool isJavaOffLoadCheck,
                                                   TR::RealRegister * javaStackPointerRealRegister,
                                                   TR::Register * methodMetaDataVirtualRegister,
                                                   TR::Register* javaLitOffsetReg,
                                                   TR::S390JNICallDataSnippet *jniCallDataSnippet,
                                                   TR::Register* tempReg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   TR::Instruction * cursor = NULL;

   if (isJNICallOutFrame)
      {
      int32_t stackAdjust = (-5 * static_cast<int32_t>(sizeof(intptrj_t)));

      cursor = generateRXYInstruction(cg(), TR::InstOpCode::LAY, callNode, tempReg,
                                      generateS390MemoryReference(javaStackPointerRealRegister, stackAdjust, cg()), cursor);

      // Check VMThread field order
      TR_ASSERT_FATAL((fej9->thisThreadGetJavaFrameFlagsOffset() == fej9->thisThreadGetJavaLiteralsOffset() + TR::Compiler->om.sizeofReferenceAddress())
                      && fej9->thisThreadGetJavaLiteralsOffset() == fej9->thisThreadGetJavaPCOffset() + TR::Compiler->om.sizeofReferenceAddress(),
                      "The vmthread field order should be pc,literals,jitStackFrameFlags\n");

      // Fill up vmthread structure PC, Literals, and jitStackFrameFlags fields
      generateSS1Instruction(cg(), TR::InstOpCode::MVC, callNode, 3*(TR::Compiler->om.sizeofReferenceAddress()) - 1,
                             generateS390MemoryReference(methodMetaDataVirtualRegister, fej9->thisThreadGetJavaPCOffset(), cg()),
                             generateS390MemoryReference(jniCallDataSnippet->getBaseRegister(), jniCallDataSnippet->getPCOffset(), cg()));

      // store the JNI's adjusted jsp.
      if (!isJavaOffLoadCheck)
         {
         generateRXInstruction(cg(), TR::InstOpCode::getStoreOpCode(), callNode, tempReg,
                               generateS390MemoryReference(methodMetaDataVirtualRegister,
                                                           fej9->thisThreadGetJavaSPOffset(), cg()));
         }

      // Copy 5 slots from the JNI call data snippet to the stack as the callout frame
      generateSS1Instruction(cg(), TR::InstOpCode::MVC, callNode, -stackAdjust - 1,
                             generateS390MemoryReference(tempReg, 0, cg()),
                             generateS390MemoryReference(jniCallDataSnippet->getBaseRegister(),
                                                         jniCallDataSnippet->getJNICallOutFrameDataOffset(), cg()));
      }
   else
      {
      // store java stack pointer
      if (!isJavaOffLoadCheck)
         {
         generateRXInstruction(cg(), TR::InstOpCode::getStoreOpCode(), callNode,
                               javaStackPointerRealRegister,
                               generateS390MemoryReference(methodMetaDataVirtualRegister,
                                                           fej9->thisThreadGetJavaSPOffset(), cg()));
         }

      auto* literalOffsetMemoryReference = generateS390MemoryReference(methodMetaDataVirtualRegister,
                                                                       fej9->thisThreadGetJavaLiteralsOffset(), cg());

      // Set up literal offset slot to zero
      if (cg()->getS390ProcessorInfo()->supportsArch(TR_S390ProcessorInfo::TR_z10))
         {
         generateSILInstruction(cg(), TR::InstOpCode::getMoveHalfWordImmOpCode(), callNode, literalOffsetMemoryReference, 0);
         }
      else
         {
         generateRIInstruction(cg(), TR::InstOpCode::getLoadHalfWordImmOpCode(), callNode, javaLitOffsetReg, 0);

         generateRXInstruction(cg(), TR::InstOpCode::getStoreOpCode(), callNode, javaLitOffsetReg, literalOffsetMemoryReference);
         }
      }
   }

TR::S390JNICallDataSnippet*
TR::J9S390JNILinkage::buildJNICallDataSnippet(TR::Node* callNode,
                                              TR::RegisterDependencyConditions* deps,
                                              intptrj_t targetAddress,
                                              bool isJNICallOutFrame,
                                              bool isReleaseVMAccess,
                                              TR::LabelSymbol * returnFromJNICallLabel,
                                              int64_t killMask)
   {
   if (!(isJNICallOutFrame || isReleaseVMAccess))
      return NULL;

   TR::S390JNICallDataSnippet * jniCallDataSnippet = NULL;
   TR_ResolvedMethod * resolvedMethod = callNode->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod();
   TR_J9VMBase *fej9 = static_cast<TR_J9VMBase *>(fe());

   TR::Register * JNISnippetBaseReg = NULL;
   killMask = killAndAssignRegister(killMask, deps, &JNISnippetBaseReg, TR::RealRegister::GPR10, cg(), true);
   jniCallDataSnippet = new (trHeapMemory()) TR::S390JNICallDataSnippet(cg(), callNode);
   cg()->addSnippet(jniCallDataSnippet);
   jniCallDataSnippet->setBaseRegister(JNISnippetBaseReg);
   new (trHeapMemory()) TR::S390RILInstruction(TR::InstOpCode::LARL, callNode,
                                               jniCallDataSnippet->getBaseRegister(),
                                               jniCallDataSnippet, cg());

   jniCallDataSnippet->setTargetAddress(targetAddress);

   if (isReleaseVMAccess)
      {
      intptrj_t aValue = fej9->constReleaseVMAccessMask(); //0xfffffffffffdffdf
      jniCallDataSnippet->setConstReleaseVMAccessMask(aValue);

      aValue = fej9->constReleaseVMAccessOutOfLineMask(); //0x340001
      jniCallDataSnippet->setConstReleaseVMAccessOutOfLineMask(aValue);
      }

   if (isJNICallOutFrame)
      {
      jniCallDataSnippet->setPC(fej9->constJNICallOutFrameType());
      jniCallDataSnippet->setLiterals(0);
      jniCallDataSnippet->setJitStackFrameFlags(0);

      // JNI Callout Frame setup
      // 0(sp) : RAM method for the native
      jniCallDataSnippet->setRAMMethod(reinterpret_cast<uintptrj_t>(resolvedMethod->resolvedMethodAddress()));

      // 4[8](sp) : flags
      jniCallDataSnippet->setJNICallOutFrameFlags(fej9->constJNICallOutFrameFlags());

      // 8[16](sp) : return address (savedCP)
      jniCallDataSnippet->setReturnFromJNICall(returnFromJNICallLabel);

      // 12[24](sp) : savedPC
      jniCallDataSnippet->setSavedPC(0);

      // 16[32](sp) : tag bits (savedA0)
      intptr_t tagBits = fej9->constJNICallOutFrameSpecialTag();
      // if the current method is simply a wrapper for the JNI call, hide the call-out stack frame
      if (resolvedMethod == comp()->getCurrentMethod())
         {
         tagBits |= fej9->constJNICallOutFrameInvisibleTag();
         }

      jniCallDataSnippet->setTagBits(tagBits);
      }

   return jniCallDataSnippet;
   }

/**
 *
*/
void
TR::J9S390JNILinkage::addEndOfAllJNILabel(TR::Node* callNode,
                                          TR::RegisterDependencyConditions* deps,
                                          TR::Register* javaReturnRegister)
   {
   uint32_t numPostDeps = deps->getAddCursorForPost();
   int32_t returnRegPos = 0;

   TR::RegisterDependencyConditions * endOfAllJNIDeps = generateRegisterDependencyConditions(0, numPostDeps, cg());
   TR::RealRegister::RegNum returnRealReg = TR::RealRegister::NoReg;

   if (javaReturnRegister != NULL)
      {
      returnRegPos = deps->searchPostConditionRegisterPos(javaReturnRegister);
      returnRealReg = deps->getPostConditions()->getRegisterDependency(returnRegPos)->getRealRegister();
      }

   for(int i = 0; i < numPostDeps; ++i)
      {
      TR::RegisterDependency* dependency = deps->getPostConditions()->getRegisterDependency(i);
      TR::Register* tmpReg = dependency->getRegister();
      TR::RealRegister::RegNum realNum = dependency->getRealRegister();
      TR_RegisterKinds tmpRegKind = tmpReg->getKind();

      bool isOldTmpReg64bit = tmpReg->is64BitReg();
      uint32_t depFlag = dependency->getFlags();

      if ((javaReturnRegister != NULL) && (realNum == returnRealReg))
         {
         endOfAllJNIDeps->addPostCondition(tmpReg, realNum);
         }
      else
         {
         tmpReg = cg()->allocateRegister(tmpRegKind);
         tmpReg->setPlaceholderReg();

         if (isOldTmpReg64bit)
            tmpReg->setIs64BitReg();

         endOfAllJNIDeps->addPostCondition(tmpReg, realNum);
         cg()->stopUsingRegister(tmpReg);
         }

      endOfAllJNIDeps->getPostConditions()->getRegisterDependency(i)->assignFlags(depFlag);
      }

   TR::Instruction* endOfAllJNILabel = generateS390LabelInstruction(cg(), TR::InstOpCode::LABEL,
                                                                    callNode, generateLabelSymbol(cg()));
   endOfAllJNILabel->setDependencyConditions(endOfAllJNIDeps);
   }

/**
 * \brief
 * A utility function to print the dependency information of JNI VM helper calls.
*/
void
TR::J9S390JNILinkage::printDepsDebugInfo(TR::Node* callNode, TR::RegisterDependencyConditions* deps)
   {
   traceMsg(comp(), "CHelper linkage for n%dn %s in %s\n",
            callNode->getGlobalIndex(),
            callNode->getSymbolReference()->getSymbol()->getName(),
            comp()->signature());

   if (deps != NULL)
      {
      traceMsg(comp(), "pre helper call registers:: %d %d\n",
               (deps)->getAddCursorForPost(), (deps)->getNumPostConditions());

      for(int i = 0; i< (deps)->getAddCursorForPost(); ++i)
         {
         TR::Register* tmpReg = (deps)->getPostConditions()->getRegisterDependency(i)->getRegister();
         TR::RealRegister::RegNum realNum =(deps)->getPostConditions()->getRegisterDependency(i)->getRealRegister();

         traceMsg(comp(), "%s- GPR %d %s, []\n",
                  tmpReg->getRegisterName(comp()),
                  realNum-1,
                  tmpReg->isPlaceholderReg() ? "dummy" : "no-dummy");
         }
      }
   }

#ifdef J9ZOS390
void
TR::J9S390JNILinkage::saveRegistersForCEEHandler(TR::Node * callNode, TR::Register* vmThreadRegister)
   {
   // CSP save offset is  JIT_GPR_SAVE_OFFSET(CSP) defined as:
   // define({JIT_GPR_SAVE_OFFSET},{eval(STACK_BIAS+J9TR_cframe_jitGPRs+((CSP)*J9TR_pointerSize))})
   //
   // C stack pointer (CSP) is GPR15 on zLinux, and GPR4 on zOS
   uint32_t cspNum = 4;
   uint32_t stackBias  = 2048;
   uint32_t cframeJitGPRs = offsetof(J9CInterpreterStackFrame, jitGPRs);

   TR::RealRegister::RegNum sspRegNum = static_cast<TR::S390zOSSystemLinkage *>(cg()->getLinkage(TR_System))->getStackPointerRegister();
   TR::Register* sspReg = getS390RealRegister(sspRegNum);
   uint32_t cspSaveBias = stackBias + cframeJitGPRs + cspNum*sizeof(intptrj_t);

   // store GPR4 to vmThread->returnValue
   generateRXInstruction(cg(), TR::InstOpCode::getStoreOpCode(), callNode, sspReg,
                         generateS390MemoryReference(vmThreadRegister, offsetof(J9VMThread, returnValue), cg()));

   // load SSP
   generateRXInstruction(cg(), TR::InstOpCode::getLoadOpCode(), callNode, sspReg,
                         generateS390MemoryReference(vmThreadRegister, offsetof(J9VMThread, systemStackPointer), cg()));

   // store GPR4 to system stack SSP save slot
   // CEEHDLR will hang if the SSP is not in the returnValue field of J9VMThread
   generateSS1Instruction(cg(), TR::InstOpCode::MVC, callNode, sizeof(intptrj_t) - 1,
                          generateS390MemoryReference(sspReg, cspSaveBias, cg()),
                          generateS390MemoryReference(vmThreadRegister, offsetof(J9VMThread, returnValue), cg()));

   // The JNI definitly need GPR13 on the system stack for CEEHDLR.
   // Otherwise, the GPR13 will be 0 after JNI call returns. This leads to crashes in the JIT'ed code.
   uint32_t gpr13SaveOffset = stackBias + cframeJitGPRs + 13*sizeof(intptrj_t);
   generateRXInstruction(cg(), TR::InstOpCode::getStoreOpCode(), callNode, vmThreadRegister,
                         generateS390MemoryReference(sspReg, gpr13SaveOffset, cg()));
   }
#endif

/**
 *
 * \details
 *
 * The JNI dispatch sequence is:
 *
 * -# Setup register dependency for the JNI linkage and build native call arguments by
 *    passing arguments from the Java side to the native side according to the system
 *    linkage (e.g. XPLINK on z/OS)
 * -# If a callout frame is needed, build the 5-slot callout frame on top of the Java stack.
 * -# Call releaseVMAccess VM helper to prepare for JNI entry.
 * -# <b>[z/OS only]</b> Invoke IFA switching VM helper to turn off IFA if the native code is
 *    not zIIP-eligible. In doing so, native code execution is switched from zIIPs to GCPs.
 * -# Branch to native code.
 * -# Upon return from the native code, restore the Java stack pointer by
 *      --# loading the Java stack pointer from the VMThread structure.
 *      --# adjusting the newly loaded Java stack pointer by adding the Java literal offset it.
 *          The literal offset is the number of object references used by the native code
 * -# If a callout frame was built prior to the native call, collapse the 5-slot callout
 *    frame by simply shrinking the Java stack pointer by 5 * sizeof(intptrj_t).
 * -# Build an End-JNI-Post-dependencies.
 * -# <b>[z/OS only]</b> Invoke IFA switching VM helper to turn on IFA if the native
 *    code is not zIIP-eligible.
 * -# Call acquireVMAccessMask to get VM access and prevent GC from happening.
 * -# If the native code returns a wrapped object, unwrap the return value.
 * -# Collapse the JNI reference frame by calling the TR_S390collapseJNIReferenceFrame VM
 *    helper if needed
 * -# Check exceptions by calling the check exception VM helper if needed
 * -# Add an end-of-all-JNI post-dependency group.
 *
 * Special notes:
 * -# The last end-of-all-JNI post-dependency group is needed because JNI does not preserve any objects
 *    in any registers. Reference stored in any register is invalidated because GC can't
 *    see or update them when the native code is being executed. This kills-all
 *    dependency group is placed at the very end (after all helpers) because some
 *    helpers (IFA offload, for instance) are c-helper calls with dependencies. These CHelpers
 *    can cause spills and reverse spills around them. But the Java stack pointer is an adjusted pointer
 *    that can not be used for spills. Hence, placing a kills-all dependency group to force
 *    reverse-spills happen at the end of all JNI related operations.
 * -# There are two temporary registers used across the whole JNI dispatch: GPR11/GPR9 on z/OS
 *    and GPR1/GPR9 on zLinux. For example, GPR11 and GPR9 are used to for VM access release
 *    and acquire around the JNI call.
 * -# Certain native functions (e.g. java/lang/Math) are marked as `canDirectNativeCall` and are void
 *    of many VM helper calls (overhead). These include releasing/acquiring VM access, checking exceptions
 *    upon return, building and tearing down of the JNI callout frame, and collapsing of the JNI reference frame.
*/
TR::Register*
TR::J9S390JNILinkage::buildDirectDispatch(TR::Node * callNode)
   {
   if (comp()->getOption(TR_TraceCG))
      traceMsg(comp(), "\nBuild JNI Direct Dispatch\n");

   TR_J9VMBase *fej9 = static_cast<TR_J9VMBase *>(fe());

   TR::SystemLinkage * systemLinkage = static_cast<TR::SystemLinkage *>(cg()->getLinkage(TR_System));
   TR::LabelSymbol * returnFromJNICallLabel = generateLabelSymbol(cg());
   int32_t numDeps = systemLinkage->getNumberOfDependencyGPRegisters();
   if (cg()->supportsHighWordFacility() && !comp()->getOption(TR_DisableHighWordRA))
      numDeps += 16; //HPRs need to be spilled
   if (cg()->getSupportsVectorRegisters())
      numDeps += 32; //VRFs need to be spilled

   // 70896 Remove DEPEND instruction and merge glRegDeps to call deps
   // *Speculatively* increase numDeps for dependencies from glRegDeps
   // which is added right before callNativeFunction.
   // GlobalRegDeps should not add any more children after here.
   // TR::RegisterDependencyConditions *glRegDeps;
   TR::Node *GlobalRegDeps;

   bool hasGlRegDeps = (callNode->getNumChildren() >= 1) &&
            (callNode->getChild(callNode->getNumChildren()-1)->getOpCodeValue() == TR::GlRegDeps);
   if(hasGlRegDeps)
      {
      GlobalRegDeps = callNode->getChild(callNode->getNumChildren()-1);
      numDeps += GlobalRegDeps->getNumChildren();
      }

   TR::RegisterDependencyConditions * deps = generateRegisterDependencyConditions(numDeps, numDeps, cg());
   int64_t killMask = -1;
   TR::RealRegister * javaStackPointerRealRegister = getStackPointerRealRegister();
   TR::RealRegister * methodMetaDataRealRegister = getMethodMetaDataRealRegister();
   TR::RealRegister * javaLitPoolRealRegister = getLitPoolRealRegister();
   TR::Register * methodMetaDataVirtualRegister = methodMetaDataRealRegister;

   TR::Register * methodAddressReg = NULL;
   TR::Register * javaLitOffsetReg = NULL;
   comp()->setHasNativeCall();

   if (cg()->getSupportsRuntimeInstrumentation())
      TR::TreeEvaluator::generateRuntimeInstrumentationOnOffSequence(cg(), TR::InstOpCode::RIOFF, callNode);

   TR::ResolvedMethodSymbol * cs = callNode->getSymbol()->castToResolvedMethodSymbol();
   TR_ResolvedMethod * resolvedMethod = cs->getResolvedMethod();

   bool isPassJNIThread             = !fej9->jniDoNotPassThread(resolvedMethod);
   bool isPassReceiver              = !fej9->jniDoNotPassReceiver(resolvedMethod);
   bool isJNIGCPoint                = !fej9->jniNoGCPoint(resolvedMethod);
   bool isJNICallOutFrame           = !fej9->jniNoNativeMethodFrame(resolvedMethod);
   bool isReleaseVMAccess           = !fej9->jniRetainVMAccess(resolvedMethod);
   bool isAcquireVMAccess           = isReleaseVMAccess;
   bool isCollapseJNIReferenceFrame = !fej9->jniNoSpecialTeardown(resolvedMethod);
   bool isCheckException            = !fej9->jniNoExceptionsThrown(resolvedMethod);
   bool isUnwrapAddressReturnValue  = !fej9->jniDoNotWrapObjects(resolvedMethod);
   bool isJavaOffLoadCheck          = false;
   bool hasCEEHandlerAndFrame       = fej9->CEEHDLREnabled() && isJNICallOutFrame;

   killMask = killAndAssignRegister(killMask, deps, &methodAddressReg, (TR::Compiler->target.isLinux()) ?
                                        TR::RealRegister::GPR1 : TR::RealRegister::GPR9 , cg(), true);

   killMask = killAndAssignRegister(killMask, deps, &javaLitOffsetReg, TR::RealRegister::GPR11, cg(), true);

   intptrj_t targetAddress = reinterpret_cast<intptrj_t>(resolvedMethod->startAddressForJNIMethod(comp()));
   TR::DataType returnType = resolvedMethod->returnType();

   static char * disablePureFn = feGetEnv("TR_DISABLE_PURE_FUNC_RECOGNITION");
   if (cs->canDirectNativeCall())
      {
      isReleaseVMAccess             = false;
      isAcquireVMAccess             = false;
      isJNIGCPoint                  = false;
      isCheckException              = false;
      isJNICallOutFrame             = false;
      isCollapseJNIReferenceFrame   = false;
      }

   if (cs->isPureFunction() && (disablePureFn == NULL))
      {
      isReleaseVMAccess = false;
      isAcquireVMAccess = false;
      isCheckException  = false;
      }

   static bool forceJNIOffloadCheck = feGetEnv("TR_force_jni_offload_check") != NULL;
   bool methodIsNotzIIPable = static_cast<TR_ResolvedJ9Method *>(resolvedMethod)->methodIsNotzAAPEligible();
   if ((fej9->isJavaOffloadEnabled() && methodIsNotzIIPable)
           || (fej9->CEEHDLREnabled() && isJNICallOutFrame)
           || (forceJNIOffloadCheck && methodIsNotzIIPable && TR::Compiler->target.isZOS()))
      isJavaOffLoadCheck = true;

   if (comp()->getOption(TR_TraceCG))
      traceMsg(comp(), "isPassReceiver: %d, isOffloadCheck %d, isPassJNIThread: %d, isCEEHDLREnabled %d, isJNIGCPoint: %d, isJNICallOutFrame:%d, isReleaseVMAccess: %d, isCollapseJNIReferenceFrame: %d, methodNOTzIIPable: %d\n",
               isPassReceiver, isJavaOffLoadCheck, isPassJNIThread,
               (fej9->CEEHDLREnabled() && isJNICallOutFrame),
               isJNIGCPoint,
               isJNICallOutFrame, isReleaseVMAccess,
               isCollapseJNIReferenceFrame, methodIsNotzIIPable);

   if (isPassJNIThread)
      {
      //First param for JNI call in JNIEnv pointer
      TR::Register * jniEnvRegister = cg()->allocateRegister();
      deps->addPreCondition(jniEnvRegister, systemLinkage->getIntegerArgumentRegister(0));
      generateRRInstruction(cg(), TR::InstOpCode::getLoadRegOpCode(), callNode,
      jniEnvRegister, methodMetaDataVirtualRegister);
      }

   // JNI dispatch does not allow for any object references to survive in preserved registers
   // they are saved onto the system stack, which the stack walker has no way of accessing.
   // Hence, ensure we kill all preserved HPRs (6-12) as well
   if (cg()->supportsHighWordFacility() && !comp()->getOption(TR_DisableHighWordRA))
      {
      TR::Register *dummyReg = NULL;
      killAndAssignRegister(killMask, deps, &dummyReg, REGNUM(TR::RealRegister::HPR6), cg(), true, true );
      killAndAssignRegister(killMask, deps, &dummyReg, REGNUM(TR::RealRegister::HPR7), cg(), true, true );
      killAndAssignRegister(killMask, deps, &dummyReg, REGNUM(TR::RealRegister::HPR8), cg(), true, true );
      killAndAssignRegister(killMask, deps, &dummyReg, REGNUM(TR::RealRegister::HPR9), cg(), true, true );
      killAndAssignRegister(killMask, deps, &dummyReg, REGNUM(TR::RealRegister::HPR10), cg(), true, true );
      killAndAssignRegister(killMask, deps, &dummyReg, REGNUM(TR::RealRegister::HPR11), cg(), true, true );
      killAndAssignRegister(killMask, deps, &dummyReg, REGNUM(TR::RealRegister::HPR12), cg(), true, true );
      }

   // Setup native call arguments
   TR::S390PrivateLinkage* privateLinkage = static_cast<TR::S390PrivateLinkage*>(cg()->getLinkage(TR_Private));
   privateLinkage->setupRegisterDepForLinkage(callNode, TR_JNIDispatch, deps, killMask, systemLinkage, GlobalRegDeps, hasGlRegDeps, &methodAddressReg, javaLitOffsetReg);
   OMR::Z::Linkage::setupBuildArgForLinkage(callNode, TR_JNIDispatch, deps, true, isPassReceiver, killMask, GlobalRegDeps, hasGlRegDeps, systemLinkage);

   // Create the S390JNICallDataSnippet metadata
   // Information in this snippet is hardcoded in the JIT'ed function body as data.
   TR::S390JNICallDataSnippet * jniCallDataSnippet = buildJNICallDataSnippet(callNode, deps,
                                                                             targetAddress,
                                                                             isJNICallOutFrame, isReleaseVMAccess,
                                                                             returnFromJNICallLabel,
                                                                             killMask);
   // Sets up PC, Stack pointer and literals offset slots.
   setupJNICallOutFrameIfNeeded(callNode, isJNICallOutFrame, isJavaOffLoadCheck,
                                javaStackPointerRealRegister, methodMetaDataVirtualRegister,
                                javaLitOffsetReg,
                                jniCallDataSnippet, methodAddressReg);

   if (isReleaseVMAccess)
      {
#ifdef J9VM_INTERP_ATOMIC_FREE_JNI
      releaseVMAccessMaskAtomicFree(callNode, methodMetaDataVirtualRegister, methodAddressReg);
#else
      releaseVMAccessMask(callNode, methodMetaDataVirtualRegister, methodAddressReg, javaLitOffsetReg, jniCallDataSnippet, deps);
#endif
      }

#ifdef J9ZOS390
   if (fej9->CEEHDLREnabled())
      saveRegistersForCEEHandler(callNode, methodMetaDataVirtualRegister);
#endif

   TR::Instruction* cursor = NULL;

   // Turn off Java Offload if calling user native
   // Depends on calloutframe setup
   if (isJavaOffLoadCheck)
      {
      callIFAOffloadCheck(callNode, TR_S390jitPreJNICallOffloadCheck);

      // Fix up jsp in the VMThread.
      // The IFA offload C-Helper call requires an un-adjusted java stack so that spills and reverse
      // spills are done correctly.
      // At the end of the IFA offload call, the un-adjusted java stack will be loaded.
      // Therefore, we need to adjust it and store the updated java stack to vmthread.
      if (isJNICallOutFrame)
         {
         int32_t stackAdjust = (-5 * static_cast<int32_t>(sizeof(intptrj_t)));

         cursor = generateRXYInstruction(cg(), TR::InstOpCode::LAY, callNode, javaStackPointerRealRegister,
                                         generateS390MemoryReference(javaStackPointerRealRegister, stackAdjust, cg()), cursor);

         // store out jsp
         generateRXInstruction(cg(), TR::InstOpCode::getStoreOpCode(), callNode, javaStackPointerRealRegister,
                               generateS390MemoryReference(methodMetaDataVirtualRegister,
                                                           fej9->thisThreadGetJavaSPOffset(), cg()));
         }
      }

   // Generate a call to the native function
   TR::Register* javaReturnRegister = systemLinkage->callNativeFunction(callNode,
                                                                        deps, targetAddress, methodAddressReg,
                                                                        javaLitOffsetReg, returnFromJNICallLabel,
                                                                        jniCallDataSnippet, isJNIGCPoint);

   // restore java stack pointer
   cursor = generateRXInstruction(cg(), TR::InstOpCode::getLoadOpCode(), callNode,
                                  javaStackPointerRealRegister,
                                  generateS390MemoryReference(methodMetaDataVirtualRegister,
                                                              fej9->thisThreadGetJavaSPOffset(), cg()));

   cursor = generateRXInstruction(cg(), TR::InstOpCode::getAddOpCode(), callNode,
                                  javaStackPointerRealRegister,
                                  generateS390MemoryReference(methodMetaDataVirtualRegister,
                                                              fej9->thisThreadGetJavaLiteralsOffset(), cg()));

   // Pop the JNI callout frame
   if (isJNICallOutFrame)
     {
     cursor = generateRXInstruction(cg(), TR::InstOpCode::LA, callNode,
                                    javaStackPointerRealRegister,
                                    generateS390MemoryReference(javaStackPointerRealRegister, JNI_CALLOUT_FRAME_SIZE, cg()));
     }

   //OMR::Z::Linkage::generateDispatchReturnLable(callNode, cg(), deps, javaReturnRegister, hasGlRegDeps, GlobalRegDeps);
   TR::RegisterDependencyConditions * endJNIPostDeps = new (self()->trHeapMemory())
               TR::RegisterDependencyConditions(NULL, deps->getPostConditions(), 0, deps->getAddCursorForPost(), cg());

   cursor->setDependencyConditions(endJNIPostDeps);

   if (cg()->getSupportsRuntimeInstrumentation())
      TR::TreeEvaluator::generateRuntimeInstrumentationOnOffSequence(cg(), TR::InstOpCode::RION, callNode);
   // ========================================== END OF JNI. Helper calls below ========================================

   //Turn on Java Offload
   if (isJavaOffLoadCheck)
     callIFAOffloadCheck(callNode, TR_S390jitPostJNICallOffloadCheck);

   if (isAcquireVMAccess)
      {
#ifdef J9VM_INTERP_ATOMIC_FREE_JNI
      acquireVMAccessMaskAtomicFree(callNode, methodMetaDataVirtualRegister, methodAddressReg);
#else
      acquireVMAccessMask(callNode, javaLitPoolRealRegister, methodMetaDataVirtualRegister);
#endif
      }

   isUnwrapAddressReturnValue = (isUnwrapAddressReturnValue &&
                         (returnType == TR::Address) &&
                         (javaReturnRegister!= NULL) &&
                         (javaReturnRegister->getKind() == TR_GPR));

   if (isUnwrapAddressReturnValue)
     {
     unwrapReturnValue(callNode, javaReturnRegister, deps);
     }

   if (isCollapseJNIReferenceFrame)
     {
     collapseJNIReferenceFrame(callNode, javaLitPoolRealRegister, methodAddressReg, javaStackPointerRealRegister);
     }

   if (isCheckException)
     {
     checkException(callNode, javaLitOffsetReg, methodMetaDataVirtualRegister);
     }

   // ================================== Stop using JNI post deps registers =============================================

   callNode->setRegister(javaReturnRegister);

   if (javaReturnRegister)
      {
      deps->stopUsingDepRegs(cg(), javaReturnRegister->getRegisterPair() == NULL ? javaReturnRegister :
         javaReturnRegister->getHighOrder(), javaReturnRegister->getLowOrder());
      }
   else
      {
      deps->stopUsingDepRegs(cg());
      }


   if(hasGlRegDeps)
      {
      cg()->decReferenceCount(GlobalRegDeps);
      }

   // ================================== END of the entire JNI sequence. Kill all regs ========================================

   if (cg()->getDebug() && comp()->getOption(TR_TraceCG))
      printDepsDebugInfo(callNode, deps);

   addEndOfAllJNILabel(callNode, deps, javaReturnRegister);

   return javaReturnRegister;
   }


#ifdef J9VM_INTERP_ATOMIC_FREE_JNI

/**
 * \details
 * This is the atomic-free JNI design and works in conjunction with VMAccess.cpp
 * atomic-free JNI changes.
 *
 * In the JNI dispatch sequence, a release-vm-access action is performed before
 * the branch to native code; and an acquire-vm-access
 * is done after the thread execution returns from the native call. Both of the
 * actions require synchronization between the
 * application thread and the GC thread. This was previously implemented with the
 * atomic compare-and-swap (CS) instruction, which is slow in nature.
 *
 * To speed up the JNI acquire and release access actions (the fast path), a
 * store-load sequence is generated by this evaluator
 * to replace the CS instruction. Normally, the fast path ST-LD are not serialized
 * and can be done out-of-order for higher performance. Synchronization
 * burden is offloaded to the slow path.
 *
 * The slow path is where a thread tries to acquire exclusive vm access. The slow path
 * should be taken proportionally less often than the fast
 * path. Should the slow path be taken, that thread will be penalized by calling a slow
 * flushProcessWriteBuffer() routine so that all threads
 * can momentarily synchronize memory writes. Having fast and slow paths makes the
 * atomic-free JNI design asymmetric.
 *
 * <b>z/OS notes</b>
 * zOS currently does not support the asymmetric algorithm. Hence,
 * a serialization instruction (BCR 14,0) between the store and the load.
 */
void
TR::J9S390JNILinkage::releaseVMAccessMaskAtomicFree(TR::Node * callNode,
                                                    TR::Register * methodMetaDataVirtualRegister,
                                                    TR::Register * tempReg1)
   {
   TR_J9VMBase *fej9 = static_cast<TR_J9VMBase *>(fe());
   TR::CodeGenerator* cg = self()->cg();
   TR::Compilation* comp = self()->comp();

   // Store a 1 into vmthread->inNative
   generateSILInstruction(cg, TR::InstOpCode::getMoveHalfWordImmOpCode(), callNode,
                          generateS390MemoryReference(methodMetaDataVirtualRegister, offsetof(J9VMThread, inNative), cg),
                          1);

#if !defined(J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH)
   generateSerializationInstruction(cg, callNode, NULL);
#endif

   // Compare vmthread public flag with J9_PUBLIC_FLAGS_VM_ACCESS
   generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), callNode, tempReg1,
                         generateS390MemoryReference(methodMetaDataVirtualRegister, fej9->thisThreadGetPublicFlagsOffset(), cg));

   TR::LabelSymbol * longReleaseSnippetLabel = generateLabelSymbol(cg);
   TR::LabelSymbol * longReleaseRestartLabel = generateLabelSymbol(cg);

   TR_ASSERT_FATAL(J9_PUBLIC_FLAGS_VM_ACCESS >= MIN_IMMEDIATE_BYTE_VAL && J9_PUBLIC_FLAGS_VM_ACCESS <= MAX_IMMEDIATE_BYTE_VAL, "VM access bit must be immediate");
   generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), callNode, tempReg1, J9_PUBLIC_FLAGS_VM_ACCESS, longReleaseSnippetLabel, TR::InstOpCode::COND_BNE);
   cg->addSnippet(new (self()->trHeapMemory()) TR::S390HelperCallSnippet(cg,
                                                                         callNode, longReleaseSnippetLabel,
                                                                         comp->getSymRefTab()->findOrCreateAcquireVMAccessSymbolRef(comp->getJittedMethodSymbol()),
                                                                         longReleaseRestartLabel));

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, callNode, longReleaseRestartLabel);
   }

/**
 * \brief
 * Build the atomic-free acquire VM access sequence for JNI dispatch.
 * */
void
TR::J9S390JNILinkage::acquireVMAccessMaskAtomicFree(TR::Node * callNode,
                                                    TR::Register * methodMetaDataVirtualRegister,
                                                    TR::Register * tempReg1)
   {
   TR_J9VMBase *fej9 = static_cast<TR_J9VMBase *>(fe());
   TR::CodeGenerator* cg = self()->cg();
   TR::Compilation* comp = self()->comp();

   // Zero vmthread->inNative, which is a UDATA field
   generateSS1Instruction(cg, TR::InstOpCode::XC, callNode, TR::Compiler->om.sizeofReferenceAddress() - 1,
                          generateS390MemoryReference(methodMetaDataVirtualRegister, offsetof(J9VMThread, inNative), cg),
                          generateS390MemoryReference(methodMetaDataVirtualRegister, offsetof(J9VMThread, inNative), cg));

#if !defined(J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH)
   generateSerializationInstruction(cg, callNode, NULL);
#endif

   // Compare vmthread public flag with J9_PUBLIC_FLAGS_VM_ACCESS
   generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), callNode, tempReg1,
                         generateS390MemoryReference(methodMetaDataVirtualRegister, fej9->thisThreadGetPublicFlagsOffset(), cg));

   TR::LabelSymbol * longAcquireSnippetLabel = generateLabelSymbol(cg);
   TR::LabelSymbol * longAcquireRestartLabel = generateLabelSymbol(cg);

   TR_ASSERT_FATAL(J9_PUBLIC_FLAGS_VM_ACCESS >= MIN_IMMEDIATE_BYTE_VAL && J9_PUBLIC_FLAGS_VM_ACCESS <= MAX_IMMEDIATE_BYTE_VAL, "VM access bit must be immediate");
   generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), callNode, tempReg1, J9_PUBLIC_FLAGS_VM_ACCESS, longAcquireSnippetLabel, TR::InstOpCode::COND_BNE);

   cg->addSnippet(new (self()->trHeapMemory()) TR::S390HelperCallSnippet(cg,
                                                                         callNode, longAcquireSnippetLabel,
                                                                         comp->getSymRefTab()->findOrCreateAcquireVMAccessSymbolRef(comp->getJittedMethodSymbol()),
                                                                         longAcquireRestartLabel));

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, callNode, longAcquireRestartLabel);
   }
#else

/**
 * \brief
 * Release vm access. Invoked before transitioning from the JIT'ed code to native code.
 *
 * At this point: arguments for the native routine are all in place already, i.e., if there are
 *                more than 24 byte worth of arguments, some of them are on the stack. However,
 *                we potentially go out to call a helper before jumping to the native.
 *                but the helper call saves and restores all regs
 */
void
TR::J9S390JNILinkage::releaseVMAccessMask(TR::Node * callNode,
                                          TR::Register * methodMetaDataVirtualRegister,
                                          TR::Register * methodAddressReg, TR::Register * javaLitOffsetReg,
                                          TR::S390JNICallDataSnippet * jniCallDataSnippet,
                                          TR::RegisterDependencyConditions * deps)
   {
   TR::LabelSymbol * loopHead = generateLabelSymbol(self()->cg());
   TR::LabelSymbol * longReleaseSnippetLabel = generateLabelSymbol(self()->cg());
   TR::LabelSymbol * doneLabel = generateLabelSymbol(self()->cg());
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(self()->fe());

   generateRXInstruction(self()->cg(), TR::InstOpCode::getLoadOpCode(), callNode, methodAddressReg,
     generateS390MemoryReference(methodMetaDataVirtualRegister,
       fej9->thisThreadGetPublicFlagsOffset(), self()->cg()));

   TR::Instruction * label = generateS390LabelInstruction(self()->cg(), TR::InstOpCode::LABEL, callNode, loopHead);
   label->setStartInternalControlFlow();

   generateRRInstruction(self()->cg(), TR::InstOpCode::getLoadRegOpCode(), callNode, javaLitOffsetReg, methodAddressReg);
   generateRXInstruction(self()->cg(), TR::InstOpCode::getAndOpCode(), callNode, javaLitOffsetReg,
        generateS390MemoryReference(jniCallDataSnippet->getBaseRegister(), jniCallDataSnippet->getConstReleaseVMAccessOutOfLineMaskOffset(), self()->cg()));

   TR::Instruction * gcPoint = (TR::Instruction *) generateS390BranchInstruction(
      self()->cg(), TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, callNode, longReleaseSnippetLabel);
   gcPoint->setNeedsGCMap(0);

   generateRRInstruction(self()->cg(), TR::InstOpCode::getLoadRegOpCode(), callNode, javaLitOffsetReg, methodAddressReg);
   generateRXInstruction(self()->cg(), TR::InstOpCode::getAndOpCode(), callNode, javaLitOffsetReg,
         generateS390MemoryReference(jniCallDataSnippet->getBaseRegister(), jniCallDataSnippet->getConstReleaseVMAccessMaskOffset(), self()->cg()));
   generateRSInstruction(self()->cg(), TR::InstOpCode::getCmpAndSwapOpCode(), callNode, methodAddressReg, javaLitOffsetReg,
     generateS390MemoryReference(methodMetaDataVirtualRegister,
       fej9->thisThreadGetPublicFlagsOffset(), self()->cg()));


   //get existing post conditions on the registers parameters and create a new post cond for the internal control flow
   TR::RegisterDependencyConditions * postDeps = new (self()->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, self()->cg());
   TR::RealRegister::RegNum realReg;
   int32_t regPos = deps->searchPostConditionRegisterPos(methodMetaDataVirtualRegister);
   if (regPos >= 0)
      {
      realReg = deps->getPostConditions()->getRegisterDependency(regPos)->getRealRegister();
      postDeps->addPostCondition(methodMetaDataVirtualRegister, realReg);
      }
   else
      postDeps->addPostCondition(methodMetaDataVirtualRegister, TR::RealRegister::AssignAny);

   regPos = deps->searchPostConditionRegisterPos(methodAddressReg);
   if (regPos >= 0)
      {
      realReg = deps->getPostConditions()->getRegisterDependency(regPos)->getRealRegister();
      postDeps->addPostCondition(methodAddressReg, realReg);
      }
   else
      postDeps->addPostCondition(methodAddressReg, TR::RealRegister::AssignAny);

   regPos = deps->searchPostConditionRegisterPos(javaLitOffsetReg);
   if (regPos >= 0)
      {
      realReg = deps->getPostConditions()->getRegisterDependency(regPos)->getRealRegister();
      postDeps->addPostCondition(javaLitOffsetReg, realReg);
      }
   else
      postDeps->addPostCondition(javaLitOffsetReg, TR::RealRegister::AssignAny);


   TR::Instruction * br = generateS390BranchInstruction(self()->cg(), TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, callNode, loopHead);
   br->setEndInternalControlFlow();
   br->setDependencyConditions(postDeps);

   generateS390LabelInstruction(self()->cg(), TR::InstOpCode::LABEL, callNode, doneLabel);


   self()->cg()->addSnippet(new (self()->trHeapMemory()) TR::S390HelperCallSnippet(self()->cg(), callNode, longReleaseSnippetLabel,
                              self()->comp()->getSymRefTab()->findOrCreateReleaseVMAccessSymbolRef(self()->comp()->getJittedMethodSymbol()), doneLabel));
   // end of release vm access (spin lock)
   }

/**
 *
*/
void TR::J9S390JNILinkage::acquireVMAccessMask(TR::Node * callNode,
                                               TR::Register * javaLitPoolVirtualRegister,
                                               TR::Register * methodMetaDataVirtualRegister)
   {
   // start of acquire vm access

   TR::Register * tempReg1 = cg()->allocateRegister();
   TR::Register * tempReg2 = cg()->allocateRegister();

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(self()->fe());
   intptrj_t aValue = fej9->constAcquireVMAccessOutOfLineMask();

   TR::Instruction * loadInstr = static_cast<TR::Instruction *>(genLoadAddressConstant(self()->cg(),
                                                                                       callNode, aValue, tempReg1,
                                                                                       NULL, NULL, javaLitPoolVirtualRegister));

   switch (loadInstr->getKind())
         {
         case TR::Instruction::IsRX:
         case TR::Instruction::IsRXE:
         case TR::Instruction::IsRXY:
         case TR::Instruction::IsRXYb:
              ((TR::S390RXInstruction *)loadInstr)->getMemoryReference()->setMemRefMustNotSpill();
              break;
         default:
              break;
         }

   generateRRInstruction(self()->cg(), TR::InstOpCode::getXORRegOpCode(), callNode, tempReg2, tempReg2);

   TR::LabelSymbol * longAcquireSnippetLabel = generateLabelSymbol(self()->cg());
   TR::LabelSymbol * acquireDoneLabel = generateLabelSymbol(self()->cg());
   generateRSInstruction(cg(), TR::InstOpCode::getCmpAndSwapOpCode(), callNode, tempReg2, tempReg1,
      generateS390MemoryReference(methodMetaDataVirtualRegister,
         (int32_t)fej9->thisThreadGetPublicFlagsOffset(), self()->cg()));
   TR::Instruction *gcPoint = (TR::Instruction *) generateS390BranchInstruction(self()->cg(), TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, callNode, longAcquireSnippetLabel);
   gcPoint->setNeedsGCMap(0);

   self()->cg()->addSnippet(new (self()->trHeapMemory()) TR::S390HelperCallSnippet(self()->cg(), callNode, longAcquireSnippetLabel,
                              self()->comp()->getSymRefTab()->findOrCreateAcquireVMAccessSymbolRef(self()->comp()->getJittedMethodSymbol()), acquireDoneLabel));
   generateS390LabelInstruction(self()->cg(), TR::InstOpCode::LABEL, callNode, acquireDoneLabel);

   cg()->stopUsingRegister(tempReg1);
   cg()->stopUsingRegister(tempReg2);
   }

#endif

