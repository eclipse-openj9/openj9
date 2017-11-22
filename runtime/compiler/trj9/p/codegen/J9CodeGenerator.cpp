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

#include "j9cfg.h"
#include "codegen/AheadOfTimeCompile.hpp"           // for AheadOfTimeCompile
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Linkage.hpp"
#include "env/CompilerEnv.hpp"
#include "env/OMRMemory.hpp"
#include "env/VMJ9.h"
#include "env/jittypes.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "p/codegen/PPCInstruction.hpp"
#include "p/codegen/PPCJNILinkage.hpp"
#include "p/codegen/PPCPrivateLinkage.hpp"
#include "p/codegen/PPCRecompilation.hpp"
#include "p/codegen/PPCSystemLinkage.hpp"

extern void TEMPORARY_initJ9PPCTreeEvaluatorTable(TR::CodeGenerator *cg);

J9::Power::CodeGenerator::CodeGenerator() :
      J9::CodeGenerator()
   {
   TR::CodeGenerator *cg = self();
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (comp->fe());

   cg->setAheadOfTimeCompile(new (cg->trHeapMemory()) TR::AheadOfTimeCompile(cg));

   if (!comp->getOption(TR_FullSpeedDebug))
      cg->setSupportsDirectJNICalls();

   if (!comp->getOption(TR_DisableBDLLVersioning))
      {
      cg->setSupportsBigDecimalLongLookasideVersioning();
      }

   cg->setSupportsNewInstanceImplOpt();

   static char *disableMonitorCacheLookup = feGetEnv("TR_disableMonitorCacheLookup");
   if (!disableMonitorCacheLookup)
      comp->setOption(TR_EnableMonitorCacheLookup);

   cg->setSupportsPartialInlineOfMethodHooks();
   cg->setSupportsInliningOfTypeCoersionMethods();

   if (!comp->getOption(TR_DisableReadMonitors))
      cg->setSupportsReadOnlyLocks();

   static bool disableTLHPrefetch = (feGetEnv("TR_DisableTLHPrefetch") != NULL);

   // Enable software prefetch of the TLH and configure the TLH prefetching
   // geometry.
   //
   if (!disableTLHPrefetch && comp->getOption(TR_TLHPrefetch) && !comp->compileRelocatableCode())
      {
      cg->setEnableTLHPrefetching();
      }

   //This env-var does 3 things:
   // 1. Prevents batch clear in frontend/j9/rossa.cpp
   // 2. Prevents all allocations to nonZeroTLH
   // 3. Maintains the old semantics zero-init and prefetch.
   // The use of this env-var is more complete than the JIT Option then.
   static bool disableDualTLH = (feGetEnv("TR_DisableDualTLH") != NULL);
   // Enable use of non-zero initialized TLH for object allocations where
   // zero-initialization is not required as detected by the optimizer.
   //
   if (!disableDualTLH && !comp->getOption(TR_DisableDualTLH) && !comp->compileRelocatableCode() && !comp->getOptions()->realTimeGC())
      {
      cg->setIsDualTLH();
      }

   /*
    * "Statically" initialize the FE-specific tree evaluator functions.
    * This code only needs to execute once per JIT lifetime.
    */
   static bool initTreeEvaluatorTable = false;
   if (!initTreeEvaluatorTable)
      {
      TEMPORARY_initJ9PPCTreeEvaluatorTable(cg);
      initTreeEvaluatorTable = true;
      }

   if (comp->fej9()->hasFixedFrameC_CallingConvention())
      self()->setHasFixedFrameC_CallingConvention();

   }



// Get or create the TR::Linkage object that corresponds to the given linkage
// convention.
// Even though this method is common, its implementation is machine-specific.
//
TR::Linkage *
J9::Power::CodeGenerator::createLinkage(TR_LinkageConventions lc)
   {
   TR::Linkage *linkage;
   switch (lc)
      {
      case TR_Private:
         linkage = new (self()->trHeapMemory()) TR::PPCPrivateLinkage(self());
         break;
      case TR_System:
         linkage = new (self()->trHeapMemory()) TR::PPCSystemLinkage(self());
         break;
      case TR_CHelper:
      case TR_Helper:
         linkage = new (self()->trHeapMemory()) TR::PPCHelperLinkage(self(), lc);
         break;
      case TR_J9JNILinkage:
         linkage = new (self()->trHeapMemory()) TR::PPCJNILinkage(self());
         break;
      default :
         linkage = new (self()->trHeapMemory()) TR::PPCSystemLinkage(self());
         TR_ASSERT(false, "Unexpected linkage convention");
      }

   self()->setLinkage(lc, linkage);
   return linkage;
   }

TR::Recompilation *
J9::Power::CodeGenerator::allocateRecompilationInfo()
   {
   return TR_PPCRecompilation::allocate(self()->comp());
   }

void
J9::Power::CodeGenerator::generateBinaryEncodingPrologue(
      TR_PPCBinaryEncodingData *data)
   {
   TR::Compilation *comp = self()->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   TR::Instruction *tempInstruction;

   data->recomp = comp->getRecompilationInfo();
   data->cursorInstruction = self()->getFirstInstruction();
   data->preProcInstruction = data->cursorInstruction;
   data->jitTojitStart = data->cursorInstruction->getNext();

   self()->getLinkage()->loadUpArguments(data->cursorInstruction);

   if (data->recomp != NULL)
      {
      data->recomp->generatePrePrologue();
      }
   else
      {
      if (comp->getOption(TR_FullSpeedDebug) || comp->getOption(TR_SupportSwitchToInterpreter))
         {
         self()->generateSwitchToInterpreterPrePrologue(NULL, comp->getStartTree()->getNode());
         }
      else
         {
         TR::ResolvedMethodSymbol *methodSymbol = comp->getMethodSymbol();
         /* save the original JNI native address if a JNI thunk is generated */
         /* thunk is not recompilable, nor does it support FSD */
         if (methodSymbol->isJNI())
            {
            uintptrj_t JNIMethodAddress = (uintptrj_t)methodSymbol->getResolvedMethod()->startAddressForJNIMethod(comp);
            TR::Node *firstNode = comp->getStartTree()->getNode();
            if (TR::Compiler->target.is64Bit())
               {
               int32_t highBits = (int32_t)(JNIMethodAddress>>32), lowBits = (int32_t)JNIMethodAddress;
               TR::Instruction *cursor = new (self()->trHeapMemory()) TR::PPCImmInstruction(TR::InstOpCode::dd, firstNode,
                  TR::Compiler->target.cpu.isBigEndian()?highBits:lowBits, NULL, self());
               generateImmInstruction(self(), TR::InstOpCode::dd, firstNode,
                  TR::Compiler->target.cpu.isBigEndian()?lowBits:highBits, cursor);
               }
            else
               {
               new (self()->trHeapMemory()) TR::PPCImmInstruction(TR::InstOpCode::dd, firstNode, (int32_t)JNIMethodAddress, NULL, self());
               }
            }
         }
      }

   data->cursorInstruction = self()->getFirstInstruction();

   while (data->cursorInstruction && data->cursorInstruction->getOpCodeValue() != TR::InstOpCode::proc)
      {
      data->estimate          = data->cursorInstruction->estimateBinaryLength(data->estimate);
      data->cursorInstruction = data->cursorInstruction->getNext();
      }

   int32_t boundary = comp->getOptions()->getJitMethodEntryAlignmentBoundary(self());
   if (boundary && (boundary > 4) && ((boundary & (boundary - 1)) == 0))
      {
      comp->getOptions()->setJitMethodEntryAlignmentBoundary(boundary);
      self()->setPreJitMethodEntrySize(data->estimate);
      data->estimate += (boundary - 4);
      }

   tempInstruction = data->cursorInstruction;
   if ((data->recomp!=NULL) && (!data->recomp->useSampling()))
      {
      tempInstruction = data->recomp->generatePrologue(tempInstruction);
      }

   self()->getLinkage()->createPrologue(tempInstruction);

   }


void
J9::Power::CodeGenerator::lowerTreeIfNeeded(
      TR::Node *node,
      int32_t childNumberOfNode,
      TR::Node *parent,
      TR::TreeTop *tt)
   {
   J9::CodeGenerator::lowerTreeIfNeeded(node, childNumberOfNode, parent, tt);

   if ((node->getOpCode().isLeftShift() ||
        node->getOpCode().isRightShift() || node->getOpCode().isRotate()) &&
       self()->needsNormalizationBeforeShifts() &&
       !node->isNormalizedShift())
      {
      TR::Node *second = node->getSecondChild();
      int32_t normalizationAmount;
      if (node->getType().isInt64())
         normalizationAmount = 63;
      else
         normalizationAmount = 31;

      // Some platforms like IA32 obey Java semantics for shifts even if the
      // shift amount is greater than 31 or 63. However other platforms like PPC need
      // to normalize the shift amount to range (0, 31) or (0, 63) before shifting in order
      // to obey Java semantics. This can be captured in the IL and commoning/hoisting
      // can be done (look at Compressor.compress).
      //
      if ( (second->getOpCodeValue() != TR::iconst) &&
          ((second->getOpCodeValue() != TR::iand) ||
           (second->getSecondChild()->getOpCodeValue() != TR::iconst) ||
           (second->getSecondChild()->getInt() != normalizationAmount)))
         {
         // Not normalized yet
         //
         TR::Node * normalizedNode = TR::Node::create(TR::iand, 2, second, TR::Node::create(second, TR::iconst, 0, normalizationAmount));
         second->recursivelyDecReferenceCount();
         node->setAndIncChild(1, normalizedNode);
         node->setNormalizedShift(true);
         }
      }

   }


bool J9::Power::CodeGenerator::suppressInliningOfRecognizedMethod(TR::RecognizedMethod method)
   {
   if (self()->isMethodInAtomicLongGroup(method))
      {
      return true;
      }

#ifdef J9VM_OPT_JAVA_CRYPTO_ACCELERATION
   if (self()->suppressInliningOfCryptoMethod(method))
      {
      return true;
      }
#endif

   if (!self()->comp()->compileRelocatableCode() &&
       !self()->comp()->getOption(TR_DisableDFP) &&
       TR::Compiler->target.cpu.supportsDecimalFloatingPoint())
      {
      if (method == TR::java_math_BigDecimal_DFPIntConstructor ||
          method == TR::java_math_BigDecimal_DFPLongConstructor ||
          method == TR::java_math_BigDecimal_DFPLongExpConstructor ||
          method == TR::java_math_BigDecimal_DFPAdd ||
          method == TR::java_math_BigDecimal_DFPSubtract ||
          method == TR::java_math_BigDecimal_DFPMultiply ||
          method == TR::java_math_BigDecimal_DFPDivide ||
          method == TR::java_math_BigDecimal_DFPScaledAdd ||
          method == TR::java_math_BigDecimal_DFPScaledSubtract ||
          method == TR::java_math_BigDecimal_DFPScaledMultiply ||
          method == TR::java_math_BigDecimal_DFPScaledDivide ||
          method == TR::java_math_BigDecimal_DFPRound ||
          method == TR::java_math_BigDecimal_DFPSetScale ||
          method == TR::java_math_BigDecimal_DFPCompareTo ||
          method == TR::java_math_BigDecimal_DFPSignificance ||
          method == TR::java_math_BigDecimal_DFPExponent ||
          method == TR::java_math_BigDecimal_DFPBCDDigits ||
          method == TR::java_math_BigDecimal_DFPUnscaledValue)
            return true;
      }

   if ((method==TR::java_util_concurrent_atomic_AtomicBoolean_getAndSet) ||
      (method==TR::java_util_concurrent_atomic_AtomicInteger_getAndAdd) ||
      (method==TR::java_util_concurrent_atomic_AtomicInteger_getAndIncrement) ||
      (method==TR::java_util_concurrent_atomic_AtomicInteger_getAndDecrement) ||
      (method==TR::java_util_concurrent_atomic_AtomicInteger_getAndSet) ||
      (method==TR::java_util_concurrent_atomic_AtomicInteger_addAndGet) ||
      (method==TR::java_util_concurrent_atomic_AtomicInteger_decrementAndGet) ||
      (method==TR::java_util_concurrent_atomic_AtomicInteger_incrementAndGet))
      {
      return true;
      }

   if (method == TR::java_lang_Math_abs_F ||
       method == TR::java_lang_Math_abs_D ||
       method == TR::java_lang_Math_abs_I ||
       method == TR::java_lang_Math_abs_L ||
       method == TR::java_lang_Integer_highestOneBit ||
       method == TR::java_lang_Integer_numberOfLeadingZeros ||
       method == TR::java_lang_Integer_numberOfTrailingZeros ||
       method == TR::java_lang_Integer_rotateLeft ||
       method == TR::java_lang_Integer_rotateRight ||
       method == TR::java_lang_Long_highestOneBit ||
       method == TR::java_lang_Long_numberOfLeadingZeros ||
       method == TR::java_lang_Long_numberOfTrailingZeros ||
       method == TR::java_lang_Short_reverseBytes ||
       method == TR::java_lang_Integer_reverseBytes ||
       method == TR::java_lang_Long_reverseBytes ||
       (TR::Compiler->target.is64Bit() &&
        (method == TR::java_lang_Long_rotateLeft ||
        method == TR::java_lang_Long_rotateRight)))
      {
      return true;
      }

   // Transactional Memory
   if (self()->getSupportsTM())
      {
      if (method == TR::java_util_concurrent_ConcurrentHashMap_tmEnabled ||
          method == TR::java_util_concurrent_ConcurrentHashMap_tmPut ||
          method == TR::java_util_concurrent_ConcurrentLinkedQueue_tmOffer ||
          method == TR::java_util_concurrent_ConcurrentLinkedQueue_tmPoll ||
          method == TR::java_util_concurrent_ConcurrentLinkedQueue_tmEnabled ||
          method == TR::java_util_concurrent_atomic_AtomicStampedReference_tmDoubleWordCAS ||
          method == TR::java_util_concurrent_atomic_AtomicStampedReference_tmDoubleWordSet ||
          method == TR::java_util_concurrent_atomic_AtomicStampedReference_tmEnabled ||
          method == TR::java_util_concurrent_atomic_AtomicMarkableReference_tmDoubleWordCAS ||
          method == TR::java_util_concurrent_atomic_AtomicMarkableReference_tmDoubleWordSet ||
          method == TR::java_util_concurrent_atomic_AtomicMarkableReference_tmEnabled)
          {
          return true;
          }
      }

   return false;
}


bool
J9::Power::CodeGenerator::enableAESInHardwareTransformations()
   {
   if ( (TR::Compiler->target.cpu.getPPCSupportsAES() || (TR::Compiler->target.cpu.getPPCSupportsVMX() && TR::Compiler->target.cpu.getPPCSupportsVSX())) &&
         !self()->comp()->getOptions()->getOption(TR_DisableAESInHardware))
      return true;
   else
      return false;
   }

