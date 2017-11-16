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

#include "codegen/IA32PrivateLinkage.hpp"

#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/Snippet.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/CHTable.hpp"
#include "env/jittypes.h"
#include "env/CompilerEnv.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "trj9/env/VMJ9.h"
#include "x/codegen/CallSnippet.hpp"
#include "x/codegen/CheckFailureSnippet.hpp"
#include "x/codegen/FPTreeEvaluator.hpp"
#include "x/codegen/HelperCallSnippet.hpp"
#include "x/codegen/IA32LinkageUtils.hpp"
#include "x/codegen/JNIPauseSnippet.hpp"
#include "x/codegen/PassJNINullSnippet.hpp"
#include "x/codegen/X86Instruction.hpp"

TR::IA32PrivateLinkage::IA32PrivateLinkage(TR::CodeGenerator *cg)
   : TR::X86PrivateLinkage(cg)
   {
   // for shrinkwrapping, we cannot use pushes for the
   // preserved regs. pushes/pops need to be in sequence
   // and this is not compatible with shrinkwrapping as
   // registers need not be saved/restored in sequence
   //
   if (cg->comp()->getOption(TR_DisableShrinkWrapping))
      _properties._properties = UsesPushesForPreservedRegs;
   else
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
      if (cg->useSSEForSinglePrecision())
         _properties._registerFlags[TR::RealRegister::xmmIndex(i)] = 0;
      else
         _properties._registerFlags[TR::RealRegister::xmmIndex(i)] = Preserved; // TODO: Safe assumption?
      }

   if (cg->useSSEForSinglePrecision() || cg->useSSEForDoublePrecision())
      _properties._registerFlags[TR::RealRegister::xmm0] = FloatReturn;
   else
      _properties._registerFlags[TR::RealRegister::st0]  = FloatReturn;

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

   if (cg->useSSEForDoublePrecision())
      _properties._returnRegisters[1] = TR::RealRegister::xmm0;
   else
      _properties._returnRegisters[1] = TR::RealRegister::st0;

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
   _properties._offsetToFirstParm = 4;
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

// routines for shrink wrapping
//
TR::Instruction *TR::IA32PrivateLinkage::savePreservedRegister(TR::Instruction *cursor, int32_t regIndex, int32_t offset)
   {
   TR::RealRegister *reg = machine()->getX86RealRegister((TR::RealRegister::RegNum)regIndex);

   if (offset != -1)
      {
      cursor = generateMemRegInstruction(
                  cursor,
                  S4MemReg,
                  generateX86MemoryReference(machine()->getX86RealRegister(TR::RealRegister::vfp), offset, cg()),
                  reg,
                  cg()
                  );
      }
   else
      {
      cursor = new (trHeapMemory()) TR::X86RegInstruction(cursor, PUSHReg, reg, cg());
      }
   return cursor;
   }

TR::Instruction *TR::IA32PrivateLinkage::restorePreservedRegister(TR::Instruction *cursor, int32_t regIndex, int32_t offset)
   {
   TR::RealRegister *reg = machine()->getX86RealRegister((TR::RealRegister::RegNum)regIndex);

   if (offset != -1)
      {
      cursor = generateRegMemInstruction(
                  cursor,
                  L4RegMem,
                  reg,
                  generateX86MemoryReference(machine()->getX86RealRegister(TR::RealRegister::vfp), offset, cg()),
                  cg()
                  );
      }
   else
      {
      cursor = new (trHeapMemory()) TR::X86RegInstruction(cursor, POPReg, reg, cg());
      }
   return cursor;
   }

// please reflect any changes to these routines in mapPreservedRegistersToStackOffsets
//
TR::Instruction *TR::IA32PrivateLinkage::savePreservedRegisters(TR::Instruction *cursor)
   {
   if (_properties.getUsesPushesForPreservedRegs())
      {
      for (int32_t pindex = _properties.getMaxRegistersPreservedInPrologue()-1;
           pindex >= 0;
           pindex--)
         {
         TR::RealRegister::RegNum idx = _properties.getPreservedRegister((uint32_t)pindex);
         TR::RealRegister *reg = machine()->getX86RealRegister(idx);
         if (reg->getHasBeenAssignedInMethod() && reg->getState() != TR::RealRegister::Locked)
            {
            cursor = new (trHeapMemory()) TR::X86RegInstruction(cursor, PUSHReg, reg, cg());
            }
         }
      }
   else
      {
      // for shrinkwrapping
      TR::ResolvedMethodSymbol *bodySymbol  = comp()->getJittedMethodSymbol();
      const int32_t          localSize   = _properties.getOffsetToFirstLocal() - bodySymbol->getLocalMappingCursor();
      const int32_t          pointerSize = _properties.getPointerSize();

      int32_t offsetCursor = -localSize - _properties.getPointerSize();
      int32_t numPreserved = getProperties().getMaxRegistersPreservedInPrologue();
      TR_BitVector *p              = cg()->getPreservedRegsInPrologue();

      for (int32_t pindex = numPreserved-1;
            pindex >= 0;
            pindex--)
         {
         TR::RealRegister::RegNum idx = _properties.getPreservedRegister((uint32_t)pindex);
         TR::RealRegister *reg = machine()->getX86RealRegister(idx);
         if (reg->getHasBeenAssignedInMethod() && reg->getState() != TR::RealRegister::Locked)
            {
            if (!p || p->get(idx))
               {
               cursor = generateMemRegInstruction(
                           cursor,
                           S4MemReg,
                           generateX86MemoryReference(machine()->getX86RealRegister(TR::RealRegister::vfp), offsetCursor, cg()),
                           reg,
                           cg()
                           );
               }
            offsetCursor -= pointerSize;
            }
         }
      }
   return cursor;
   }

TR::Instruction *TR::IA32PrivateLinkage::restorePreservedRegisters(TR::Instruction *cursor)
   {
   if (_properties.getUsesPushesForPreservedRegs())
      {
      for (int32_t pindex = 0;
           pindex < _properties.getMaxRegistersPreservedInPrologue();
           pindex++)
         {
         TR::RealRegister::RegNum idx = _properties.getPreservedRegister((uint32_t)pindex);
         TR::RealRegister *reg = machine()->getX86RealRegister(idx);
         if (reg->getHasBeenAssignedInMethod())
            {
            cursor = new (trHeapMemory()) TR::X86RegInstruction(cursor, POPReg, reg, cg());
            }
         }
      }
   else
      {
      // for shrinkwrapping
      TR::ResolvedMethodSymbol *bodySymbol  = comp()->getJittedMethodSymbol();
      const int32_t          localSize   = _properties.getOffsetToFirstLocal() - bodySymbol->getLocalMappingCursor();
      const int32_t          pointerSize = _properties.getPointerSize();


      int32_t offsetCursor = -localSize - _properties.getPointerSize();
      int32_t numPreserved = getProperties().getMaxRegistersPreservedInPrologue();
      TR_BitVector *p              = cg()->getPreservedRegsInPrologue();
      for (int32_t pindex = numPreserved-1;
            pindex >= 0;
            pindex--)
         {
         TR::RealRegister::RegNum idx = _properties.getPreservedRegister((uint32_t)pindex);
         TR::RealRegister *reg = machine()->getX86RealRegister(idx);
         if (reg->getHasBeenAssignedInMethod())
            {
            if (!p || p->get(idx))
               {
               cursor = generateRegMemInstruction(
                           cursor,
                           L4RegMem,
                           reg,
                           generateX86MemoryReference(machine()->getX86RealRegister(TR::RealRegister::vfp), offsetCursor, cg()),
                           cg()
                           );
               }
            offsetCursor -= pointerSize;
            }
         }
      }
   return cursor;
   }


TR::Register *TR::IA32PrivateLinkage::buildJNIDispatch(TR::Node *callNode)
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
   TR::RealRegister *espReal = machine()->getX86RealRegister(TR::RealRegister::esp);
   TR::LabelSymbol *returnAddrLabel = NULL;
   TR::X86VFPDedicateInstruction  *vfpDedicateInstruction = NULL;

   // Use ebx as an anchored frame register because the arguments to the native call are
   // evaluated after the stacks have been switched.
   //
   vfpDedicateInstruction = generateVFPDedicateInstruction(
         machine()->getX86RealRegister(TR::RealRegister::ebx), callNode, cg());

   returnAddrLabel = generateLabelSymbol(cg());
   if (createJNIFrame)
      {
      // Anchored frame pointer register.
      //
      TR::RealRegister *ebxReal = machine()->getX86RealRegister(getProperties().getFramePointerRegister());


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
      generateLabelInstruction(PUSHImm4, callNode, returnAddrLabel, cg());

      // Push frame flags.
      //
      generateImmInstruction(PUSHImm4, callNode, fej9->constJNICallOutFrameFlags(), cg());

      // Push the RAM method for the native.
      //
      generateImmInstruction(PUSHImm4, callNode, (uintptrj_t) resolvedMethod->resolvedMethodAddress(), cg());

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
      generateX86MemoryReference(espReal, ((TR_J9VMBase *) fej9)->thisThreadGetMachineBPOffset(comp()) + argSize, cg()),
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
      generateX86MemoryReference(ebpReal, ((TR_J9VMBase *) fej9)->thisThreadGetMachineSPOffset(), cg()),
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
   TR::MemoryReference  *tempMR = machine()->getDummyLocalMR(TR::Float);
      generateFPMemRegInstruction(FSTPMemReg, callNode, tempMR, returnRegister, cg());
      returnRegister = cg()->allocateSinglePrecisionRegister(TR_FPR);
      generateRegMemInstruction(MOVSSRegMem, callNode, returnRegister, generateX86MemoryReference(*tempMR, 0, cg()), cg());
      }
   else if (cg()->useSSEForDoublePrecision() && callNode->getOpCode().isDouble())
      {
   TR::MemoryReference  *tempMR = machine()->getDummyLocalMR(TR::Double);
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


int32_t TR::IA32PrivateLinkage::buildArgs(
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
         generateRegRegInstruction(MOV4RegReg, thisChild, eaxRegister, rcvrReg, cg());
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


TR::UnresolvedDataSnippet *TR::IA32PrivateLinkage::generateX86UnresolvedDataSnippetWithCPIndex(
      TR::Node *child,
      TR::SymbolReference *symRef,
      int32_t cpIndex)
   {
   // We know the symbol must have already been resolved, so no GC can
   // happen (unless the gcOnResolve option is being used).
   //
   TR::UnresolvedDataSnippet *snippet = TR::UnresolvedDataSnippet::create(cg(), child, symRef, false, (debug("gcOnResolve") != NULL));
   cg()->addSnippet(snippet);

   TR::Instruction *dataReferenceInstruction = generateImmSnippetInstruction(PUSHImm4, child, cpIndex, snippet, cg());
   snippet->setDataReferenceInstruction(dataReferenceInstruction);
   generateBoundaryAvoidanceInstruction(TR::X86BoundaryAvoidanceInstruction::unresolvedAtomicRegions, 8, 8, dataReferenceInstruction, cg());

   return snippet;
   }

TR::Register *TR::IA32PrivateLinkage::pushIntegerWordArg(TR::Node *child)
   {
   TR::Compilation *comp = cg()->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(fe());
   if (child->getRegister() == NULL)
      {
      if (child->getOpCode().isLoadConst())
         {
         int32_t value = child->getInt();
         TR_X86OpCodes pushOp;
         if (value >= -128 && value <= 127)
            {
            pushOp = PUSHImms;
            }
         else
            {
            pushOp = PUSHImm4;
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
               TR::Instruction *instr = generateImmSymInstruction(PUSHImm4, child, (uintptrj_t)sym->getStaticAddress(), symRef, cg());

               // HCR register the class passed as a parameter
               //
               if ((sym->isClassObject() || sym->isAddressOfClassObject())
                   && cg()->wantToPatchClassPointer((TR_OpaqueClassBlock*)sym->getStaticAddress(), child))
                  {
                  comp->getStaticHCRPICSites()->push_front(instr);
                  }
               }

            cg()->decReferenceCount(child);
            return NULL;
            }
         }
      }

   return TR::IA32LinkageUtils::pushIntegerWordArg(child, cg());
   }

TR::Register *TR::IA32PrivateLinkage::pushThis(TR::Node *child)
   {
   // Don't decrement the reference count on the "this" child until we've
   // had a chance to set up its dependency conditions
   //
   TR::Register *tempRegister = cg()->evaluate(child);
   generateRegInstruction(PUSHReg, child, tempRegister, cg());
   return tempRegister;
   }

TR::Register *TR::IA32PrivateLinkage::pushJNIReferenceArg(TR::Node *child)
   {
   if (child->getOpCodeValue() == TR::loadaddr)
      {
      TR::SymbolReference * symRef = child->getSymbolReference();
      TR::StaticSymbol *sym = symRef->getSymbol()->getStaticSymbol();
      if (sym)
         {
         if (sym->isAddressOfClassObject())
            {
            pushIntegerWordArg(child);
            }
         else
            {
         TR::MemoryReference  *tempMR;
            if (child->getRegister())
               tempMR = generateX86MemoryReference(child->getRegister(), 0, cg());
            else
               tempMR = generateX86MemoryReference(child, cg());
            TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg());
            TR::LabelSymbol *startLabel   = generateLabelSymbol(cg());
            TR::LabelSymbol *restartLabel = generateLabelSymbol(cg());
            startLabel->setStartInternalControlFlow();
            restartLabel->setEndInternalControlFlow();
            generateLabelInstruction(LABEL, child, startLabel, cg());

            generateMemImmInstruction(CMP4MemImms, child, tempMR, 0, cg());
            cg()->addSnippet(new (trHeapMemory()) TR::X86PassJNINullSnippet(cg(), child, restartLabel, snippetLabel));
            generateLabelInstruction(JE4, child, snippetLabel, cg());
            if (symRef->isUnresolved())
               {
               generateX86UnresolvedDataSnippetWithCPIndex(child, symRef, symRef->getCPIndex());
               }
            else if (child->getRegister())
               {
               generateRegInstruction(PUSHReg, child, child->getRegister(), cg());
               }
            else
               {
               // must pass symbol reference so that aot can put out a relocation for it
               generateImmSymInstruction(PUSHImm4, child, (uintptrj_t)sym->getStaticAddress(), symRef, cg());
               }

            generateLabelInstruction(LABEL, child, restartLabel, cg());
            tempMR->decNodeReferenceCounts(cg());
            cg()->decReferenceCount(child);
            }
         }
      else // must be loadaddr of parm or local
         {
         if (child->pointsToNonNull())
            {
            pushIntegerWordArg(child);
            }
         else if (child->pointsToNull())
            {
            generateImmInstruction(PUSHImms, child, 0, cg());
            cg()->decReferenceCount(child);
            }
         else
            {
            TR::Register *refReg = cg()->evaluate(child);
            TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg());
            TR::LabelSymbol *startLabel   = generateLabelSymbol(cg());
            TR::LabelSymbol *restartLabel = generateLabelSymbol(cg());
            startLabel->setStartInternalControlFlow();
            restartLabel->setEndInternalControlFlow();
            generateLabelInstruction(LABEL, child, startLabel, cg());

            generateMemImmInstruction(CMP4MemImms, child, generateX86MemoryReference(refReg, 0, cg()), 0, cg());
            cg()->addSnippet(new (trHeapMemory()) TR::X86PassJNINullSnippet(cg(), child, restartLabel, snippetLabel));
            generateLabelInstruction(JE4, child, snippetLabel, cg());

            generateRegInstruction(PUSHReg, child, refReg, cg());
            TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, 1, cg());
            deps->addPostCondition(refReg, TR::RealRegister::NoReg, cg());

            generateLabelInstruction(LABEL, child, restartLabel, deps, cg());
            cg()->decReferenceCount(child);
            }
         }
      }
   else
      {
      TR::IA32LinkageUtils::pushIntegerWordArg(child, cg());
      }

   return NULL;
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


TR::Instruction *TR::IA32PrivateLinkage::buildPICSlot(
      TR::X86PICSlot picSlot,
      TR::LabelSymbol *mismatchLabel,
      TR::LabelSymbol *doneLabel,
      TR::X86CallSite &site)
   {
   TR::Node *node = site.getCallNode();
   TR::Register *vftReg = site.evaluateVFT();
   uintptrj_t addrToBeCompared = picSlot.getClassAddress();
   TR::Instruction *firstInstruction;
   if (picSlot.getMethodAddress())
      {
      addrToBeCompared = (uintptrj_t) picSlot.getMethodAddress();
      firstInstruction = generateMemImmInstruction(CMPMemImm4(false), node,
                                generateX86MemoryReference(vftReg, picSlot.getSlot(), cg()), (uint32_t) addrToBeCompared, cg());
      }
   else
      firstInstruction = generateRegImmInstruction(CMP4RegImm4, node, vftReg, addrToBeCompared, cg());

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
   else if (picSlot.needsNopAndJump())
      {
      generatePaddingInstruction(1, node, cg())->setNeedsGCMap((site.getArgSize()<<14) | site.getPreservedRegisterMask());
      generateLongLabelInstruction(JMP4, node, mismatchLabel, cg());
      }

   TR::Instruction *instr;
   if (picSlot.getMethod())
      {
      instr = generateImmInstruction(CALLImm4, node, (uint32_t)(uintptrj_t)picSlot.getMethod()->startAddressForJittedMethod(), cg());
      }
   else if (picSlot.getHelperMethodSymbolRef())
      {
      TR::MethodSymbol *helperMethod = picSlot.getHelperMethodSymbolRef()->getSymbol()->castToMethodSymbol();
      instr = generateImmSymInstruction(CALLImm4, node, (uint32_t)(uintptrj_t)helperMethod->getMethodAddress(), picSlot.getHelperMethodSymbolRef(), cg());
      }
   else
      {
      instr = generateImmInstruction(CALLImm4, node, 0, cg());
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
      instr = generateLabelInstruction(JMP4, node, doneLabel, cg());
      instr->setNeedsGCMap(site.getPreservedRegisterMask());
      }

   if (picSlot.generateNextSlotLabelInstruction())
      {
      generateLabelInstruction(LABEL, node, mismatchLabel, cg());
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

void TR::IA32PrivateLinkage::buildIPIC(
      TR::X86CallSite &site,
      TR::LabelSymbol *entryLabel,
      TR::LabelSymbol *doneLabel,
      uint8_t *thunk)
   {
   TR_ASSERT(doneLabel, "a doneLabel is required for IPIC dispatches");

   if (entryLabel)
      generateLabelInstruction(LABEL, site.getCallNode(), entryLabel, cg());

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
         generateInstruction(BADIA32Op, site.getCallNode(), cg());
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

   TR_Method *method = site.getMethodSymbol()->getMethod();
   TR_OpaqueClassBlock *declaringClass = NULL;
   uintptrj_t itableIndex;
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
   while (startOfPicInstruction->getOpCodeValue() == BADIA32Op)
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

void TR::IA32PrivateLinkage::buildVirtualOrComputedCall(
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
      buildVFTCall(site, CALLReg, doneLabel, targetAddress, NULL);
      }
   else if (resolvedSite && site.resolvedVirtualShouldUseVFTCall())
      {
      if (entryLabel)
         generateLabelInstruction(LABEL, site.getCallNode(), entryLabel, cg());

      intptrj_t offset=site.getSymbolReference()->getOffset();
      if (!resolvedSite)
         offset = 0;

      buildVFTCall(site, CALLMem, doneLabel, NULL, generateX86MemoryReference(site.evaluateVFT(), offset, cg()));
      }
   else
      {
      TR::X86PrivateLinkage::buildVPIC(site, entryLabel, doneLabel);
      }
   }

