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

#if defined(TR_TARGET_32BIT)

#include "codegen/IA32JNILinkage.hpp"

#include "codegen/CodeGenerator.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/Snippet.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/jittypes.h"
#include "env/CompilerEnv.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "env/VMJ9.h"
#include "x/codegen/CheckFailureSnippet.hpp"
#include "x/codegen/IA32LinkageUtils.hpp"
#include "x/codegen/X86Instruction.hpp"


TR::Register *TR::IA32JNILinkage::buildDirectDispatch(TR::Node *callNode, bool spillFPRegs)
   {
   TR::MethodSymbol* methodSymbol = callNode->getSymbolReference()->getSymbol()->castToMethodSymbol();
   TR_ASSERT(methodSymbol->isJNI(), "TR::IA32JNILinkage::buildDirectDispatch can't hanlde this case.\n");
   return buildJNIDispatch(callNode);
   }

TR::Register *TR::IA32JNILinkage::buildJNIDispatch(TR::Node *callNode)
   {
#ifdef DEBUG
   if (debug("reportJNI"))
      {
      printf("JNI Dispatch: %s calling %s\n", comp()->signature(), comp()->getDebug()->getName(callNode->getSymbolReference()));
      }
#endif

   TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, 20, cg());

   TR::SymbolReference      *callSymRef = callNode->getSymbolReference();
   TR::MethodSymbol         *callSymbol = callSymRef->getSymbol()->castToMethodSymbol();
   TR::ResolvedMethodSymbol *resolvedMethodSymbol = callSymbol->castToResolvedMethodSymbol();
   TR_ResolvedMethod *resolvedMethod = resolvedMethodSymbol->getResolvedMethod();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
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
   else if (callSymbol->isPureFunction())
      {
      dropVMAccess = false;
      isJNIGCPoint = false;
      checkExceptions = false;
      }

   TR::Register*     ebpReal = cg()->getVMThreadRegister();
   TR::RealRegister* espReal = cg()->machine()->getRealRegister(TR::RealRegister::esp);
   TR::LabelSymbol*           returnAddrLabel = NULL;

   // Build parameters
   int32_t argSize = buildParametersOnCStack(callNode, passReceiver ? 0 : 1, passThread, true);

   // JNI Call
   TR::LabelSymbol* begLabel = generateLabelSymbol(cg());
   TR::LabelSymbol* endLabel = generateLabelSymbol(cg());
   begLabel->setStartInternalControlFlow();
   endLabel->setEndInternalControlFlow();

   generateLabelInstruction(LABEL, callNode, begLabel, cg());

   // Save VFP
   TR::X86VFPSaveInstruction* vfpSave = generateVFPSaveInstruction(callNode, cg());

   returnAddrLabel = generateLabelSymbol(cg());
   if (createJNIFrame)
      {
      // Anchored frame pointer register.
      //
      TR::RealRegister *ebxReal = cg()->machine()->getRealRegister(TR::RealRegister::ebx);


      // Begin: mask out the magic bit that indicates JIT frames below
      //
      generateMemImmInstruction(
            S4MemImm4,
            callNode,
            generateX86MemoryReference(ebpReal, fej9->thisThreadGetJavaFrameFlagsOffset(), cg()),
            0,
            cg()
            );
      // End: mask out the magic bit that indicates JIT frames below

      ebxReal->setHasBeenAssignedInMethod(true);

      // Build stack frame in java stack.
      // Tag current bp.
      //
      uint32_t tagBits = fej9->constJNICallOutFrameSpecialTag();

      // If the current method is simply a wrapper for the JNI call, hide the call-out stack frame.
      //
      if (resolvedMethod == comp()->getCurrentMethod())
         {
         tagBits |= fej9->constJNICallOutFrameInvisibleTag();
         }

      // Push tag bits (savedA0 slot).
      //
      generateImmInstruction(PUSHImm4, callNode, tagBits, cg());

      // Allocate space to get to frame size for special frames (skip savedPC slot).
      //
      generateRegImmInstruction(SUB4RegImms, callNode, espReal, 4, cg());

      // Push return address in this frame (savedCP slot).
      //
      TR::X86LabelInstruction* returnAddrLabelInstruction = generateLabelInstruction(PUSHImm4, callNode, returnAddrLabel, cg());
      returnAddrLabelInstruction->setReloType(TR_AbsoluteMethodAddress);

      // Push frame flags.
      //
      generateImmInstruction(PUSHImm4, callNode, fej9->constJNICallOutFrameFlags(), cg());

      // Push the RAM method for the native.
      //
      static const int reloTypes[] = {TR_VirtualRamMethodConst, 0 /*Interfaces*/, TR_StaticRamMethodConst, TR_SpecialRamMethodConst};
      int rType = resolvedMethodSymbol->getMethodKind()-1; //method kinds are 1-based
      TR_ASSERT(reloTypes[rType], "There shouldn't be direct JNI interface calls!");
      generateImmInstruction(PUSHImm4, callNode, (uintptrj_t) resolvedMethod->resolvedMethodAddress(), cg(), reloTypes[rType]);

      // Store out pc and literals values indicating the callout frame.
      //
      generateMemImmInstruction(
            S4MemImm4,
            callNode,
            generateX86MemoryReference(ebpReal, fej9->thisThreadGetJavaPCOffset(), cg()),
            fej9->constJNICallOutFrameType(),
            cg()
            );
      generateMemImmInstruction(
            S4MemImm4,
            callNode,
            generateX86MemoryReference(ebpReal, fej9->thisThreadGetJavaLiteralsOffset(), cg()),
            0,
            cg()
            );
      }

   // Store out jsp.
   //
   generateMemRegInstruction(
      S4MemReg,
      callNode,
      generateX86MemoryReference(ebpReal, fej9->thisThreadGetJavaSPOffset(), cg()),
      espReal,
      cg()
      );

   // Switch stacks.
   // Load up machine SP.
   //
   generateRegMemInstruction(
      L4RegMem,
      callNode,
      espReal,
      generateX86MemoryReference(ebpReal, ((TR_J9VMBase *) fej9)->thisThreadGetMachineSPOffset(), cg()),
      cg()
      );

   // Save ESP to EDI, a callee preserved register
   // It is necessary because the JNI method may be either caller-cleanup or callee-cleanup
   TR::Register *ediReal = cg()->allocateRegister();
   generateRegRegInstruction(MOV4RegReg, callNode, ediReal, espReal, cg());
   generateRegImmInstruction(argSize >= -128 && argSize <= 127 ? SUB4RegImms : SUB4RegImm4, callNode, espReal, argSize, cg());

   // We have to be careful to allocate the return register after the
   // dependency conditions for the other killed registers have been set up,
   // otherwise it will be marked as interfering with them.
   // Start by creating a dummy post condition and then fill it in after the
   // others have been set up.
   //
   deps->addPostCondition(NULL, TR::RealRegister::ecx, cg());

   TR::Register *esiReal;
   TR::Register *eaxReal;
   TR::Register *edxReal;
   TR::Register *ebxReal;
   eaxReal = cg()->allocateRegister();
   deps->addPostCondition(eaxReal, TR::RealRegister::eax, cg());
   cg()->stopUsingRegister(eaxReal);

   edxReal = cg()->allocateRegister();
   deps->addPostCondition(NULL, TR::RealRegister::edx, cg());

   ebxReal = cg()->allocateRegister();
   deps->addPostCondition(ebxReal, TR::RealRegister::ebx, cg());
   cg()->stopUsingRegister(ebxReal);

   deps->addPostCondition(ediReal, TR::RealRegister::edi, cg());

   esiReal = cg()->allocateRegister();
   deps->addPostCondition(esiReal, TR::RealRegister::esi, cg());
   cg()->stopUsingRegister(esiReal);

   deps->getPostConditions()->setDependencyInfo(2, edxReal, TR::RealRegister::edx, cg());
   if (!callNode->getType().isInt64())
      cg()->stopUsingRegister(edxReal);

   TR::Register *ecxReal;
   if (callNode->getDataType() == TR::Address)
      ecxReal = cg()->allocateCollectedReferenceRegister();
   else
      ecxReal = cg()->allocateRegister();
   deps->getPostConditions()->setDependencyInfo(0, ecxReal, TR::RealRegister::ecx, cg());
   if (callNode->getDataType() == TR::NoType)
      cg()->stopUsingRegister(ecxReal);

   // Kill all the xmm registers across the JNI callout sequence.
   //
   for (int i = 0; i <= 7; i++)
      {
      TR::Register *xmm_i = cg()->allocateRegister(TR_FPR);
      deps->addPostCondition(xmm_i, TR::RealRegister::xmmIndex(i), cg());
      cg()->stopUsingRegister(xmm_i);
      }

   if (dropVMAccess)
      {
      generateMemImmInstruction(S4MemImm4,
                                callNode,
                                generateX86MemoryReference(ebpReal, offsetof(struct J9VMThread, inNative), cg()),
                                1,
                                cg());

#if !defined(J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH)
      TR::MemoryReference *mr = generateX86MemoryReference(espReal, intptrj_t(0), cg());
      mr->setRequiresLockPrefix();
      generateMemImmInstruction(OR4MemImms, callNode, mr, 0, cg());
#endif /* !J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH */

      TR::LabelSymbol *longReleaseSnippetLabel = generateLabelSymbol(cg());
      TR::LabelSymbol *longReleaseRestartLabel = generateLabelSymbol(cg());

      static_assert(J9_PUBLIC_FLAGS_VM_ACCESS <= 0x7fffffff, "VM access bit must be immediate");
      generateMemImmInstruction(CMP4MemImm4,
                                callNode,
                                generateX86MemoryReference(ebpReal, fej9->thisThreadGetPublicFlagsOffset(), cg()),
                                J9_PUBLIC_FLAGS_VM_ACCESS,
                                cg());
      generateLabelInstruction(JNE4, callNode, longReleaseSnippetLabel, cg());
      generateLabelInstruction(LABEL, callNode, longReleaseRestartLabel, cg());

      TR_OutlinedInstructionsGenerator og(longReleaseSnippetLabel, callNode, cg());
      auto helper = comp()->getSymRefTab()->findOrCreateReleaseVMAccessSymbolRef(comp()->getMethodSymbol());
      generateImmSymInstruction(CALLImm4, callNode, (uintptrj_t)helper->getMethodAddress(), helper, cg());
      generateLabelInstruction(JMP4, callNode, longReleaseRestartLabel, cg());
      }

   // Dispatch jni method directly.
   //
   TR::Instruction  *instr = generateImmSymInstruction(
      CALLImm4,
      callNode,
      (uintptrj_t)resolvedMethodSymbol->getResolvedMethod()->startAddressForJNIMethod(comp()),
      callNode->getSymbolReference(),
      cg()
      );

   if (isJNIGCPoint)
      instr->setNeedsGCMap((argSize<<14) | 0xFF00FFFF);

   // memoize the call instruction, in order that we can register an assumption for this later on
   cg()->getJNICallSites().push_front(new (trHeapMemory()) TR_Pair<TR_ResolvedMethod,TR::Instruction>(resolvedMethodSymbol->getResolvedMethod(), instr));

   // To Do:: will need an aot relocation for this one at some point.

   // Lay down a label for the frame push to reference.
   //
   generateLabelInstruction(LABEL, callNode, returnAddrLabel, cg());

   // Restore stack pointer
   generateRegRegInstruction(MOV4RegReg, callNode, espReal, ediReal, cg());
   cg()->stopUsingRegister(ediReal);

   // Need to squirrel away the return value for some data types to avoid register
   // conflicts with subsequent code to acquire vm access, etc.
   //
   // jni methods may not return a full register in some cases so need to get the declared
   // type so that we sign and zero extend the narrower integer return types properly.
   //
   bool isUnsigned = resolvedMethod->returnTypeIsUnsigned();
   bool isBoolean;
   switch (resolvedMethod->returnType())
      {
      case TR::Int8:
         isBoolean = comp()->getSymRefTab()->isReturnTypeBool(callSymRef);
         if (isBoolean)
            {
            // For bool return type, must check whether value returned by
            // JNI is zero (false) or non-zero (true) to yield Java result
            generateRegRegInstruction(TEST1RegReg, callNode, eaxReal, eaxReal, cg());
            generateRegInstruction(SETNE1Reg, callNode, eaxReal, cg());
            }
         generateRegRegInstruction((isUnsigned || isBoolean)
                                         ? MOVZXReg4Reg1 : MOVSXReg4Reg1,
                                   callNode, ecxReal, eaxReal, cg());
         break;
      case TR::Int16:
         generateRegRegInstruction(isUnsigned ? MOVZXReg4Reg2 : MOVSXReg4Reg2,
                                   callNode, ecxReal, eaxReal, cg());
         break;
      case TR::Address:
      case TR::Int64:
      case TR::Int32:
         generateRegRegInstruction(MOV4RegReg, callNode, ecxReal, eaxReal, cg());
         break;
      }

   if (dropVMAccess)
      {
      // Re-acquire vm access.
      //
      generateMemImmInstruction(S4MemImm4,
                                callNode,
                                generateX86MemoryReference(ebpReal, offsetof(struct J9VMThread, inNative), cg()),
                                0,
                                cg());

#if !defined(J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH)
      TR::MemoryReference *mr = generateX86MemoryReference(espReal, intptrj_t(0), cg());
      mr->setRequiresLockPrefix();
      generateMemImmInstruction(OR4MemImms, callNode, mr, 0, cg());
#endif /* !J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH */

      TR::LabelSymbol *longAcquireSnippetLabel = generateLabelSymbol(cg());
      TR::LabelSymbol *longAcquireRestartLabel = generateLabelSymbol(cg());

      static_assert(J9_PUBLIC_FLAGS_VM_ACCESS <= 0x7fffffff, "VM access bit must be immediate");
      generateMemImmInstruction(CMP4MemImm4,
                                callNode,
                                generateX86MemoryReference(ebpReal, fej9->thisThreadGetPublicFlagsOffset(), cg()),
                                J9_PUBLIC_FLAGS_VM_ACCESS,
                                cg());
      generateLabelInstruction(JNE4, callNode, longAcquireSnippetLabel, cg());
      generateLabelInstruction(LABEL, callNode, longAcquireRestartLabel, cg());

      TR_OutlinedInstructionsGenerator og(longAcquireSnippetLabel, callNode, cg());
      auto helper = comp()->getSymRefTab()->findOrCreateAcquireVMAccessSymbolRef(comp()->getMethodSymbol());
      generateImmSymInstruction(CALLImm4, callNode, (uintptrj_t)helper->getMethodAddress(), helper, cg());
      generateLabelInstruction(JMP4, callNode, longAcquireRestartLabel, cg());
      }

   if (TR::Address == resolvedMethod->returnType() && wrapRefs)
      {
      // Unless NULL, need to indirect once to get the real java reference
      //
      TR::LabelSymbol *tempLab = generateLabelSymbol(cg());
      generateRegRegInstruction(TEST4RegReg, callNode, ecxReal, ecxReal, cg());
      generateLabelInstruction(JE4, callNode, tempLab, cg());
      generateRegMemInstruction(L4RegMem, callNode, ecxReal, generateX86MemoryReference(ecxReal, 0, cg()), cg());
      generateLabelInstruction(LABEL, callNode, tempLab, cg());
      }

   // Switch stacks back.
   // First store out the machine sp into the vm thread.  It has to be done as sometimes
   // it gets tromped on by call backs.
   //
   generateMemRegInstruction(
      S4MemReg,
      callNode,
      generateX86MemoryReference(ebpReal, fej9->thisThreadGetMachineSPOffset(), cg()),
      espReal,
      cg()
      );

   // Next load up the java sp so we have the callout frame on top of the java stack.
   //
   generateRegMemInstruction(
      L4RegMem,
      callNode,
      espReal,
      generateX86MemoryReference(ebpReal, fej9->thisThreadGetJavaSPOffset(), cg()),
      cg()
      );

   if (createJNIFrame)
      {
      generateRegMemInstruction(
            ADD4RegMem,
            callNode,
            espReal,
            generateX86MemoryReference(ebpReal, fej9->thisThreadGetJavaLiteralsOffset(), cg()),
            cg()
            );

      if (tearDownJNIFrame)
         {
         // Must check to see if the ref pool was used and clean them up if so--or we
         // leave a bunch of pinned garbage behind that screws up the gc quality forever.
         //
         uint32_t flagValue = fej9->constJNIReferenceFrameAllocatedFlags();
         TR_X86OpCodes op = flagValue <= 255 ? TEST1MemImm1 : TEST4MemImm4;
         TR::LabelSymbol *refPoolSnippetLabel = generateLabelSymbol(cg());
         TR::LabelSymbol *refPoolRestartLabel = generateLabelSymbol(cg());
         generateMemImmInstruction(op, callNode, generateX86MemoryReference(espReal, fej9->constJNICallOutFrameFlagsOffset(), cg()), flagValue, cg());
         generateLabelInstruction(JNE4, callNode, refPoolSnippetLabel, cg());
         generateLabelInstruction(LABEL, callNode, refPoolRestartLabel, cg());

         TR_OutlinedInstructionsGenerator og(refPoolSnippetLabel, callNode, cg());
         generateHelperCallInstruction(callNode, TR_IA32jitCollapseJNIReferenceFrame, NULL, cg());
         generateLabelInstruction(JMP4, callNode, refPoolRestartLabel, cg());
         }

      // Now set esp back to its previous value.
      //
      generateRegImmInstruction(ADD4RegImms, callNode, espReal, 20, cg());
      }

   // Get return registers set up before preserved regs are restored.
   //
   TR::Register *returnRegister = NULL;
   switch (callNode->getDataType())
      {
      case TR::Int32:
      case TR::Address:
         returnRegister = ecxReal;
         break;
      case TR::Int64:
         returnRegister = cg()->allocateRegisterPair(ecxReal, edxReal);
         break;

      // Current system linkage does not use XMM0 for floating point return, even if SSE is supported on the processor.
      //
      case TR::Float:
         deps->addPostCondition(returnRegister = cg()->allocateSinglePrecisionRegister(TR_X87), TR::RealRegister::st0, cg());
         break;
      case TR::Double:
         deps->addPostCondition(returnRegister = cg()->allocateRegister(TR_X87), TR::RealRegister::st0, cg());
         break;
      }

   deps->stopAddingConditions();

   if (checkExceptions)
      {
      // Check exceptions.
      //
      generateMemImmInstruction(
            CMP4MemImms,
            callNode,
            generateX86MemoryReference(ebpReal, fej9->thisThreadGetCurrentExceptionOffset(), cg()),
            0,
            cg()
            );
      TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg());
      instr = generateLabelInstruction(JNE4, callNode, snippetLabel, cg());
      instr->setNeedsGCMap((argSize<<14) | 0xFF00FFFF);

      TR::Snippet *snippet = new (trHeapMemory()) TR::X86CheckFailureSnippet(
            cg(),
            cg()->symRefTab()->findOrCreateRuntimeHelper(TR_throwCurrentException, false, false, false),
            snippetLabel,
            instr,
            callNode->getDataType() == TR::Float || callNode->getDataType() == TR::Double);
      cg()->addSnippet(snippet);
      }

   // Restore VFP
   generateVFPRestoreInstruction(vfpSave, callNode, cg());
   generateLabelInstruction(LABEL, callNode, endLabel, deps, cg());

   // Stop using the killed registers that are not going to persist.
   //
   if (deps)
      stopUsingKilledRegisters(deps, returnRegister);

   // If the processor supports SSE, return floating-point values in XMM registers.
   //
   if (callNode->getOpCode().isFloat())
      {
      TR::MemoryReference  *tempMR = cg()->machine()->getDummyLocalMR(TR::Float);
      generateFPMemRegInstruction(FSTPMemReg, callNode, tempMR, returnRegister, cg());
      returnRegister = cg()->allocateSinglePrecisionRegister(TR_FPR);
      generateRegMemInstruction(MOVSSRegMem, callNode, returnRegister, generateX86MemoryReference(*tempMR, 0, cg()), cg());
      }
   else if (callNode->getOpCode().isDouble())
      {
      TR::MemoryReference  *tempMR = cg()->machine()->getDummyLocalMR(TR::Double);
      generateFPMemRegInstruction(DSTPMemReg, callNode, tempMR, returnRegister, cg());
      returnRegister = cg()->allocateRegister(TR_FPR);
      generateRegMemInstruction(cg()->getXMMDoubleLoadOpCode(), callNode, returnRegister, generateX86MemoryReference(*tempMR, 0, cg()), cg());
      }

   if (cg()->enableRegisterAssociations())
      associatePreservedRegisters(deps, returnRegister);

   return returnRegister;
   }

#endif
