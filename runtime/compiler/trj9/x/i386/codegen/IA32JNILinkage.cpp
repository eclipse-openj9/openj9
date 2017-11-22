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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#if defined(TR_TARGET_32BIT)

#include "codegen/IA32JNILinkage.hpp"

#include "codegen/CodeGenerator.hpp"
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
#include "trj9/env/VMJ9.h"
#include "x/codegen/CheckFailureSnippet.hpp"
#include "x/codegen/HelperCallSnippet.hpp"
#include "x/codegen/IA32LinkageUtils.hpp"
#include "x/codegen/JNIPauseSnippet.hpp"
#include "x/codegen/PassJNINullSnippet.hpp"
#include "x/codegen/X86Instruction.hpp"


TR::Register *TR::IA32JNILinkage::buildDirectDispatch(TR::Node *callNode, bool spillFPRegs)
   {
   TR::SymbolReference  *methodSymRef             = callNode->getSymbolReference();
   TR::MethodSymbol     *methodSymbol             = methodSymRef->getSymbol()->castToMethodSymbol();
   if (methodSymbol->isJNI())
      return buildJNIDispatch(callNode);
   else if (methodSymbol->isSystemLinkageDispatch())
      TR_ASSERT(false, "call through TR::IA32SystemLinkage::buildDirectDispatch instead.\n");
   else
      {
      TR_ASSERT(false, "TR::IA32JNILinkage::buildDirectDispatch can't hanlde this case.\n");
      return NULL;
      }
   }

int32_t static computeMemoryArgSize(TR::Node *callNode, int lastChildMinus1, bool passThread)
   {
   int argSize = passThread ? 4: 0;
   for (int i = callNode->getNumChildren(); i > lastChildMinus1; i--)
      {
      TR::Node *child = callNode->getChild(i-1);
      switch (child->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
            argSize += 4;
            break;
         case TR::Address:
            argSize += 4;
            break;
         case TR::Int64:
            argSize += 8;
            break;
         case TR::Float:
            argSize += 4;
            break;
         case TR::Double:
            argSize += 8;
            break;
         }
      }
   return argSize;
   }

TR::Register *TR::IA32JNILinkage::buildJNIDispatch(TR::Node *callNode)
   {

#ifdef DEBUG
   if (debug("reportJNI"))
      {
      printf("JNI Dispatch: %s calling %s\n", comp()->signature(), comp()->getDebug()->getName(callNode->getSymbolReference()));
      }
#endif

   static char *dontUseEBXasGPR = feGetEnv("dontUseEBXasGPR");

   TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, 20, cg());

   cg()->setVMThreadRequired(true);
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

   TR::Register *ebpReal = cg()->getMethodMetaDataRegister();
   TR::RealRegister *espReal = cg()->machine()->getX86RealRegister(TR::RealRegister::esp);
   TR::LabelSymbol *returnAddrLabel = NULL;
   TR::X86VFPDedicateInstruction  *vfpDedicateInstruction = NULL;

   // Use ebx as an anchored frame register because the arguments to the native call are
   // evaluated after the stacks have been switched.
   //
   vfpDedicateInstruction = generateVFPDedicateInstruction(
         cg()->machine()->getX86RealRegister(TR::RealRegister::ebx), callNode, cg());

   returnAddrLabel = generateLabelSymbol(cg());
   if (createJNIFrame)
      {
      // Anchored frame pointer register.
      //
      TR::RealRegister *ebxReal = cg()->machine()->getX86RealRegister(getProperties().getFramePointerRegister());


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

   // Squirrel away the vmthread on the C stack.
   //
   generateRegInstruction(PUSHReg, callNode, ebpReal, cg());

   // save esp to Register
   TR::Register *ediReal = cg()->allocateRegister();
   if (cg()->getJNILinkageCalleeCleanup())
      generateRegRegInstruction(MOV4RegReg, callNode, ediReal, espReal, cg());

   // Marshall outgoing args
   // stdcall goes in C order, which is reverse of java order
   //
   uint32_t argSize = 4; // hack: isn't really an arg, but we need argsize to
                         //       know how to reach back.
   int lastChildMinus1;
   if (!passReceiver)
      {
      lastChildMinus1 = 1;
      TR::Node *firstChild =  callNode->getChild(0);
      cg()->evaluate(firstChild);
      cg()->decReferenceCount(firstChild);
      }
   else lastChildMinus1 = 0;

   int32_t totalSize = argSize + computeMemoryArgSize(callNode, lastChildMinus1, passThread);
   if ( totalSize % 16 != 0 )
      {
      int32_t adjustedMemoryArgSize = 16 - (totalSize % 16);
      TR_X86OpCodes op = SUBRegImm4();
      generateRegImmInstruction(op, callNode, espReal, adjustedMemoryArgSize, cg());
      argSize = argSize + adjustedMemoryArgSize;
      if (comp()->getOption(TR_TraceCG))
         traceMsg(comp(), "adjust arguments size by %d to make arguments 16 byte aligned \n", adjustedMemoryArgSize);
      }

   for (int i = callNode->getNumChildren(); i > lastChildMinus1; i--)
      {
      TR::Node *child = callNode->getChild(i-1);
      switch (child->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
            pushIntegerWordArg(child);
            argSize += 4;
            break;
         case TR::Address:
            pushJNIReferenceArg(child);
            argSize += 4;
            break;
         case TR::Int64:
            TR::IA32LinkageUtils::pushLongArg(child, cg());
            argSize += 8;
            break;
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

   if (!dontUseEBXasGPR)
      {
      ebxReal = cg()->allocateRegister();
      deps->addPostCondition(ebxReal, TR::RealRegister::ebx, cg());
      cg()->stopUsingRegister(ebxReal);
      }

   deps->addPostCondition(ediReal, TR::RealRegister::edi, cg());
   if (!cg()->getJNILinkageCalleeCleanup())
      cg()->stopUsingRegister(ediReal);

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

   // Pass in the env to the jni method as the lexically first arg.
   //
   if (passThread)
      {
      generateRegInstruction(PUSHReg, callNode, ebpReal, cg());
      argSize += 4;
      }

   TR_X86OpCodes op;

   if (dropVMAccess)
      {
      // Release vm access (spin lock).
      //
      generateRegMemInstruction(
            L4RegMem,
            callNode,
            eaxReal,
            generateX86MemoryReference(ebpReal, fej9->thisThreadGetPublicFlagsOffset(), cg()),
            cg()
            );

      TR::LabelSymbol *loopHead = generateLabelSymbol(cg());
      loopHead->setStartInternalControlFlow();

      // Loop head
      //
      generateLabelInstruction(LABEL, callNode, loopHead, cg());
      generateRegRegInstruction(MOV4RegReg, callNode, esiReal, eaxReal, cg());

      TR::LabelSymbol *longReleaseSnippetLabel = generateLabelSymbol(cg());
      TR::LabelSymbol *longReleaseRestartLabel = generateLabelSymbol(cg());

      generateRegImmInstruction(TEST4RegImm4, callNode, eaxReal, fej9->constReleaseVMAccessOutOfLineMask(), cg());
      generateLabelInstruction(JNE4, callNode, longReleaseSnippetLabel, cg());

      cg()->addSnippet(
            new (trHeapMemory()) TR::X86HelperCallSnippet(
               cg(),
               callNode,
               longReleaseRestartLabel,
               longReleaseSnippetLabel,
               comp()->getSymRefTab()->findOrCreateReleaseVMAccessSymbolRef(comp()->getJittedMethodSymbol())
               )
            );

      generateRegImmInstruction(AND4RegImm4, callNode, esiReal, fej9->constReleaseVMAccessMask(), cg());

      op = TR::Compiler->target.isSMP() ? LCMPXCHG4MemReg : CMPXCHG4MemReg;

      generateMemRegInstruction(op, callNode, generateX86MemoryReference(ebpReal, fej9->thisThreadGetPublicFlagsOffset(), cg()), esiReal, cg());

      TR::LabelSymbol *pauseSnippetLabel = generateLabelSymbol(cg());
      cg()->addSnippet(new (trHeapMemory()) TR::X86JNIPauseSnippet(cg(), callNode, loopHead, pauseSnippetLabel));
      generateLabelInstruction(JNE4, callNode, pauseSnippetLabel, cg());

      generateLabelInstruction(LABEL, callNode, longReleaseRestartLabel, cg());
      }
   // Load machine bp esp + fej9->thisThreadGetMachineBPOffset() + argSize
   //
   generateRegMemInstruction(
      L4RegMem,
      callNode,
      ebpReal,
      generateX86MemoryReference(espReal, fej9->thisThreadGetMachineBPOffset(comp()) + argSize, cg()),
      cg()
      );

   if (!cg()->useSSEForDoublePrecision())
      {
      // Force the FP register stack to be spilled.
      //
      TR::RegisterDependencyConditions  *fpSpillDependency = generateRegisterDependencyConditions(1, 0, cg());
      fpSpillDependency->addPreCondition(NULL, TR::RealRegister::AllFPRegisters, cg());
      generateInstruction(FPREGSPILL, callNode, fpSpillDependency, cg());
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
      instr->setNeedsGCMap((argSize<<14) | getProperties().getPreservedRegisterMapForGC());

   // memoize the call instruction, in order that we can register an assumption for this later on
   cg()->getJNICallSites().push_front(new (trHeapMemory()) TR_Pair<TR_ResolvedMethod,TR::Instruction>(resolvedMethodSymbol->getResolvedMethod(), instr));

   // To Do:: will need an aot relocation for this one at some point.

   // Lay down a label for the frame push to reference.
   //
   generateLabelInstruction(LABEL, callNode, returnAddrLabel, cg());

   // If not callee cleanup, then need to clean up the args with explicit
   // add esp, argSize-4 (-4 because we added an extra push of ebp) after the call
   //
   if (!cg()->getJNILinkageCalleeCleanup())
      {
      int32_t cleanUpSize = argSize - 4;
      if (cleanUpSize >= -128 && cleanUpSize <= 127)
         {
         generateRegImmInstruction(ADD4RegImms, callNode, espReal, cleanUpSize, cg());
         }
      else
         {
         generateRegImmInstruction(ADD4RegImm4, callNode, espReal, cleanUpSize, cg());
         }
      }
   else
      {
      //Move esp back
      generateRegRegInstruction(MOV4RegReg, callNode, espReal, ediReal, cg());
      cg()->stopUsingRegister(ediReal);
      }


   // Need to squirrel away the return value for some data types to avoid register
   // conflicts with subsequent code to acquire vm access, etc.
   //
   // jni methods may not return a full register in some cases so need to get the declared
   // type so that we sign and zero extend the narrower integer return types properly.
   //
   bool isUnsigned = resolvedMethod->returnTypeIsUnsigned();
   switch (resolvedMethod->returnType())
      {
      case TR::Int8:
         generateRegRegInstruction(isUnsigned ? MOVZXReg4Reg1 : MOVSXReg4Reg1,
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

   // Restore the vmthread back from the C stack.
   //
   generateRegInstruction(POPReg, callNode, ebpReal, cg());

   if (dropVMAccess)
      {
      // Re-acquire vm access.
      //
      generateRegRegInstruction(XOR4RegReg, callNode, eaxReal, eaxReal, cg());
      generateRegImmInstruction(MOV4RegImm4, callNode, ediReal, fej9->constAcquireVMAccessOutOfLineMask(), cg());

      TR::LabelSymbol *longReacquireSnippetLabel = generateLabelSymbol(cg());
      TR::LabelSymbol *longReacquireRestartLabel = generateLabelSymbol(cg());

      generateMemRegInstruction(
            op,
            callNode,
            generateX86MemoryReference(ebpReal, fej9->thisThreadGetPublicFlagsOffset(), cg()),
            ediReal,
            cg()
            );
      generateLabelInstruction(JNE4, callNode, longReacquireSnippetLabel, cg());

      // to do:: ecx may hold a reference across this snippet
      // If the return type is address something needs to be represented in the
      // register map for the snippet.
      //
      cg()->addSnippet(
            new (trHeapMemory()) TR::X86HelperCallSnippet(
               cg(),
               callNode,
               longReacquireRestartLabel,
               longReacquireSnippetLabel,
               comp()->getSymRefTab()->findOrCreateAcquireVMAccessSymbolRef(comp()->getJittedMethodSymbol())
               )
            );
      generateLabelInstruction(LABEL, callNode, longReacquireRestartLabel, cg());
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
         if (flagValue <= 255)
            {
            op = TEST1MemImm1;
            }
         else
            {
            op = TEST4MemImm4;
            }
         TR::LabelSymbol *refPoolSnippetLabel = generateLabelSymbol(cg());
         TR::LabelSymbol *refPoolRestartLabel = generateLabelSymbol(cg());
         generateMemImmInstruction(op, callNode, generateX86MemoryReference(espReal, fej9->constJNICallOutFrameFlagsOffset(), cg()), flagValue, cg());
         generateLabelInstruction(JNE4, callNode, refPoolSnippetLabel, cg());

         cg()->addSnippet(
               new (trHeapMemory()) TR::X86HelperCallSnippet(
                  cg(),
                  callNode,
                  refPoolRestartLabel,
                  refPoolSnippetLabel,
                  cg()->symRefTab()->findOrCreateRuntimeHelper(TR_IA32jitCollapseJNIReferenceFrame, false, false, false)
                  )
               );

         generateLabelInstruction(LABEL, callNode, refPoolRestartLabel, cg());
         }

      // Now set esp back to its previous value.
      //
      generateRegImmInstruction(ADD4RegImms, callNode, espReal, 20, cg());
      }

   // Get return registers set up before preserved regs are restored.
   //
   TR::Register *returnRegister = NULL;
   bool requiresFPstackPop = false;
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
         requiresFPstackPop = true;
         break;
      case TR::Double:
         deps->addPostCondition(returnRegister = cg()->allocateRegister(TR_X87), TR::RealRegister::st0, cg());
         requiresFPstackPop = true;
         break;
      }

   deps->addPostCondition(cg()->getMethodMetaDataRegister(), getProperties().getMethodMetaDataRegister(), cg());

   deps->stopAddingConditions();

   if (checkExceptions)
      {
      // Check exceptions.
      //
      generateRegMemInstruction(
            L4RegMem,
            callNode,
            ediReal,
            generateX86MemoryReference(ebpReal, fej9->thisThreadGetCurrentExceptionOffset(), cg()),
            cg()
            );
      TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg());
      generateRegRegInstruction(TEST4RegReg, callNode, ediReal, ediReal, cg());

      instr = generateLabelInstruction(JNE4, callNode, snippetLabel, cg());
      instr->setNeedsGCMap((argSize<<14) | getProperties().getPreservedRegisterMapForGC());

      TR::Snippet *snippet = new (trHeapMemory()) TR::X86CheckFailureSnippet(
            cg(),
            cg()->symRefTab()->findOrCreateRuntimeHelper(TR_IA32jitThrowCurrentException, false, false, false),
            snippetLabel,
            instr,
            requiresFPstackPop
            );
      cg()->addSnippet(snippet);
      }

   generateVFPReleaseInstruction(vfpDedicateInstruction, callNode, cg());
   TR::LabelSymbol *restartLabel = generateLabelSymbol(cg());
   restartLabel->setEndInternalControlFlow();
   generateLabelInstruction(LABEL, callNode, restartLabel, deps, cg());

   // Stop using the killed registers that are not going to persist.
   //
   if (deps)
      stopUsingKilledRegisters(deps, returnRegister);

   // If the processor supports SSE, return floating-point values in XMM registers.
   //
   if (cg()->useSSEForSinglePrecision() && callNode->getOpCode().isFloat())
      {
   TR::MemoryReference  *tempMR = cg()->machine()->getDummyLocalMR(TR::Float);
      generateFPMemRegInstruction(FSTPMemReg, callNode, tempMR, returnRegister, cg());
      returnRegister = cg()->allocateSinglePrecisionRegister(TR_FPR);
      generateRegMemInstruction(MOVSSRegMem, callNode, returnRegister, generateX86MemoryReference(*tempMR, 0, cg()), cg());
      }
   else if (cg()->useSSEForDoublePrecision() && callNode->getOpCode().isDouble())
      {
   TR::MemoryReference  *tempMR = cg()->machine()->getDummyLocalMR(TR::Double);
      generateFPMemRegInstruction(DSTPMemReg, callNode, tempMR, returnRegister, cg());
      returnRegister = cg()->allocateRegister(TR_FPR);
      generateRegMemInstruction(cg()->getXMMDoubleLoadOpCode(), callNode, returnRegister, generateX86MemoryReference(*tempMR, 0, cg()), cg());
      }
   else
      {
      // If the method returns a floating point value that is not used, insert a dummy store to
      // eventually pop the value from the floating point stack.
      //
      if ((callNode->getDataType() == TR::Float ||
           callNode->getDataType() == TR::Double) &&
          callNode->getReferenceCount() == 1)
         {
         generateFPSTiST0RegRegInstruction(FSTRegReg, callNode, returnRegister, returnRegister, cg());
         }
      }

   bool useRegisterAssociations = cg()->enableRegisterAssociations() ? true : false;
   if (useRegisterAssociations)
      associatePreservedRegisters(deps, returnRegister);

   cg()->setVMThreadRequired(false);

   return returnRegister;
   }

#endif
