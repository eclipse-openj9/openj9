/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#include "j9cfg.h"
#include "codegen/AheadOfTimeCompile.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/PPCJNILinkage.hpp"
#include "codegen/PPCPrivateLinkage.hpp"
#include "env/CompilerEnv.hpp"
#include "env/OMRMemory.hpp"
#include "env/VMJ9.h"
#include "env/jittypes.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "p/codegen/PPCInstruction.hpp"
#include "p/codegen/PPCRecompilation.hpp"
#include "p/codegen/PPCSystemLinkage.hpp"

extern void TEMPORARY_initJ9PPCTreeEvaluatorTable(TR::CodeGenerator *cg);

J9::Power::CodeGenerator::CodeGenerator(TR::Compilation *comp) :
      J9::CodeGenerator(comp)
   {
   /**
    * Do not add CodeGenerator initialization logic here.
    * Use the \c initialize() method instead.
    */
   }

void
J9::Power::CodeGenerator::initialize()
   {
   self()->J9::CodeGenerator::initialize();

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

   if (cg->getSupportsTM())
      {
      cg->setSupportsInlineConcurrentLinkedQueue();
      }

   cg->setSupportsNewInstanceImplOpt();

   static char *disableMonitorCacheLookup = feGetEnv("TR_disableMonitorCacheLookup");
   if (!disableMonitorCacheLookup)
      comp->setOption(TR_EnableMonitorCacheLookup);

   cg->setSupportsPartialInlineOfMethodHooks();
   cg->setSupportsInliningOfTypeCoersionMethods();

   if (comp->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P8) && comp->target().cpu.supportsFeature(OMR_FEATURE_PPC_HAS_VSX) &&
      comp->target().is64Bit() && !comp->getOption(TR_DisableFastStringIndexOf) &&
      !TR::Compiler->om.canGenerateArraylets())
      cg->setSupportsInlineStringIndexOf();

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
      cg->setHasFixedFrameC_CallingConvention();

   }

bool
J9::Power::CodeGenerator::canEmitDataForExternallyRelocatableInstructions()
   {
   // On Power, data cannot be emitted inside instructions that will be associated with an
   // external relocation record (ex. AOT or Remote compiles in OpenJ9). This is because when the
   // relocation is applied when a method is loaded, the new data in the instruction is OR'ed in (The reason
   // for OR'ing is that sometimes usefule information such as flags and hints can be stored during compilation in these data fields).
   // Hence, for the relocation to be applied correctly, we must ensure that the data fields inside the instruction
   // initially are zero.
#ifdef J9VM_OPT_JITSERVER
   return !self()->comp()->compileRelocatableCode() && !self()->comp()->isOutOfProcessCompilation();
#endif
   return !self()->comp()->compileRelocatableCode();
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
         linkage = new (self()->trHeapMemory()) J9::Power::PrivateLinkage(self());
         break;
      case TR_System:
         linkage = new (self()->trHeapMemory()) TR::PPCSystemLinkage(self());
         break;
      case TR_CHelper:
      case TR_Helper:
         linkage = new (self()->trHeapMemory()) J9::Power::HelperLinkage(self(), lc);
         break;
      case TR_J9JNILinkage:
         linkage = new (self()->trHeapMemory()) J9::Power::JNILinkage(self());
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
            uintptr_t JNIMethodAddress = (uintptr_t)methodSymbol->getResolvedMethod()->startAddressForJNIMethod(comp);
            TR::Node *firstNode = comp->getStartTree()->getNode();
            if (comp->target().is64Bit())
               {
               int32_t highBits = (int32_t)(JNIMethodAddress>>32), lowBits = (int32_t)JNIMethodAddress;
               TR::Instruction *cursor = new (self()->trHeapMemory()) TR::PPCImmInstruction(TR::InstOpCode::dd, firstNode,
                  comp->target().cpu.isBigEndian()?highBits:lowBits, NULL, self());
               generateImmInstruction(self(), TR::InstOpCode::dd, firstNode,
                  comp->target().cpu.isBigEndian()?lowBits:highBits, cursor);
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

   if (self()->supportsJitMethodEntryAlignment())
      {
      self()->setPreJitMethodEntrySize(data->estimate);
      data->estimate += (self()->getJitMethodEntryAlignmentBoundary() - 1);
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
   TR::Compilation *comp = self()->comp();

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

   if (!comp->compileRelocatableCode() &&
       !comp->getOption(TR_DisableDFP) &&
       comp->target().cpu.supportsDecimalFloatingPoint())
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

   if (method == TR::java_lang_Math_fma_D ||
       method == TR::java_lang_Math_fma_F ||
       method == TR::java_lang_StrictMath_fma_D ||
       method == TR::java_lang_StrictMath_fma_F)
      {
      return true;
      }

   // Transactional Memory
   if (self()->getSupportsInlineConcurrentLinkedQueue())
      {
      if (method == TR::java_util_concurrent_ConcurrentLinkedQueue_tmOffer ||
          method == TR::java_util_concurrent_ConcurrentLinkedQueue_tmPoll ||
          method == TR::java_util_concurrent_ConcurrentLinkedQueue_tmEnabled)
          {
          return true;
          }
      }

   return false;
}


bool
J9::Power::CodeGenerator::enableAESInHardwareTransformations()
   {
   TR::Compilation *comp = self()->comp();
   if ( (comp->target().cpu.getPPCSupportsAES() || (comp->target().cpu.supportsFeature(OMR_FEATURE_PPC_HAS_ALTIVEC) && comp->target().cpu.supportsFeature(OMR_FEATURE_PPC_HAS_VSX))) &&
         !comp->getOption(TR_DisableAESInHardware))
      return true;
   else
      return false;
   }

void
J9::Power::CodeGenerator::insertPrefetchIfNecessary(TR::Node *node, TR::Register *targetRegister)
   {
   TR::Compilation *comp = self()->comp();
   static bool disableHotFieldPrefetch = (feGetEnv("TR_DisableHotFieldPrefetch") != NULL);
   static bool disableHotFieldNextElementPrefetch  = (feGetEnv("TR_DisableHotFieldNextElementPrefetch") != NULL);
   static bool disableIteratorPrefetch  = (feGetEnv("TR_DisableIteratorPrefetch") != NULL);
   static bool disableStringObjPrefetch = (feGetEnv("TR_DisableStringObjPrefetch") != NULL);
   bool optDisabled = false;

   if (node->getOpCodeValue() == TR::aloadi ||
        (comp->target().is64Bit() &&
         comp->useCompressedPointers() &&
         node->getOpCodeValue() == TR::l2a &&
         comp->getMethodHotness() >= scorching &&
         TR::Compiler->om.compressedReferenceShiftOffset() == 0 &&
         comp->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P6))
      )
      {
      int32_t prefetchOffset = comp->findPrefetchInfo(node);
      TR::Node *firstChild = node->getFirstChild();

      if (!disableHotFieldPrefetch && prefetchOffset >= 0) // Prefetch based on hot field information
         {
         bool canSkipNullChk = false;
         static bool disableDelayPrefetch = (feGetEnv("TR_DisableDelayPrefetch") != NULL);
         TR::LabelSymbol *endCtrlFlowLabel = generateLabelSymbol(self());
         TR::Node *topNode = self()->getCurrentEvaluationTreeTop()->getNode();
         // Search the current block for a check for NULL
         TR::TreeTop *tt = self()->getCurrentEvaluationTreeTop()->getNextTreeTop();
         TR::Node *checkNode = NULL;

         while (!disableDelayPrefetch && tt && (tt->getNode()->getOpCodeValue() != TR::BBEnd))
            {
            checkNode = tt->getNode();
            if (checkNode->getOpCodeValue() == TR::ifacmpeq &&
                checkNode->getFirstChild() == node &&
                checkNode->getSecondChild()->getDataType() == TR::Address &&
                checkNode->getSecondChild()->isZero())
               {
               canSkipNullChk = true;
               }
            tt = tt->getNextTreeTop();
            }

         if (!canSkipNullChk)
            {
            TR::Register *condReg = self()->allocateRegister(TR_CCR);
            TR::LabelSymbol *startCtrlFlowLabel = generateLabelSymbol(self());
            startCtrlFlowLabel->setStartInternalControlFlow();
            endCtrlFlowLabel->setEndInternalControlFlow();
            generateLabelInstruction(self(), TR::InstOpCode::label, node, startCtrlFlowLabel);

            // check for null
            generateTrg1Src1ImmInstruction(self(), TR::InstOpCode::cmpli4, node, condReg, targetRegister, NULLVALUE);
            generateConditionalBranchInstruction(self(), TR::InstOpCode::beql, node, endCtrlFlowLabel, condReg);

            TR::Register *tempReg = self()->allocateRegister();
            TR::RegisterDependencyConditions *deps = new (self()->trHeapMemory()) TR::RegisterDependencyConditions(1, 2, self()->trMemory());
            deps->addPostCondition(tempReg, TR::RealRegister::NoReg);
            TR::addDependency(deps, condReg, TR::RealRegister::NoReg, TR_CCR, self());

            if (comp->target().is64Bit() && !comp->useCompressedPointers())
               {
               TR::MemoryReference *tempMR = TR::MemoryReference::createWithDisplacement(self(), targetRegister, prefetchOffset, 8);
               generateTrg1MemInstruction(self(), TR::InstOpCode::ld, node, tempReg, tempMR);
               }
            else
               {
               TR::MemoryReference *tempMR = TR::MemoryReference::createWithDisplacement(self(), targetRegister, prefetchOffset, 4);
               generateTrg1MemInstruction(self(), TR::InstOpCode::lwz, node, tempReg, tempMR);
               }

            TR::MemoryReference *targetMR = TR::MemoryReference::createWithDisplacement(self(), tempReg, (int32_t)0, 4);
            targetMR->forceIndexedForm(node, self());
            generateMemInstruction(self(), TR::InstOpCode::dcbt, node, targetMR);

            self()->stopUsingRegister(tempReg);
            self()->stopUsingRegister(condReg);
            }
         else
            {
            // Delay the dcbt to after the null check and fall through to the next block's treetop.
            TR::TreeTop *useTree = tt->getNextTreeTop();
            TR_ASSERT(useTree->getNode()->getOpCodeValue() == TR::BBStart, "Expecting a BBStart on the fall through\n");
            TR::Node *useNode = useTree->getNode();
            TR::Block *bb = useNode->getBlock();
            if (bb->isExtensionOfPreviousBlock()) // Survived the null check
               {
               TR_PrefetchInfo *pf = new (self()->trHeapMemory())TR_PrefetchInfo(self()->getCurrentEvaluationTreeTop(), useTree, node, useNode, prefetchOffset, false);
               comp->removeExtraPrefetchInfo(useNode);
               comp->getExtraPrefetchInfo().push_front(pf);
               }
            }

         // Do a prefetch on the next element of the array
         // if the pointer came from an array.  Seems to give no gain at all, disabled until later
         TR::Register *pointerReg = NULL;
         bool fromRegLoad = false;
         if (!disableHotFieldNextElementPrefetch)
            {
            // 32bit
            if (comp->target().is32Bit())
               {
               if (!(firstChild->getOpCodeValue() == TR::aiadd &&
                     firstChild->getFirstChild() &&
                     firstChild->isInternalPointer()) &&
                   !(firstChild->getOpCodeValue() == TR::aRegLoad &&
                     firstChild->getSymbolReference()->getSymbol()->isInternalPointer()))
                  {
                  optDisabled = true;
                  }
               else
                  {
                  fromRegLoad = (firstChild->getOpCodeValue() == TR::aRegLoad);
                  pointerReg = fromRegLoad ? firstChild->getRegister() : self()->allocateRegister();
                  if (!fromRegLoad)
                     {
                     // Case for aiadd, there should be 2 children
                     TR::Node *baseObject = firstChild->getFirstChild();
                     TR::Register *baseReg = (baseObject) ? baseObject->getRegister() : NULL;
                     TR::Node *indexObject = firstChild->getSecondChild();
                     TR::Register *indexReg = (indexObject) ? indexObject->getRegister() : NULL;
                     // If the index is constant we just grab it
                     if (indexObject->getOpCode().isLoadConst())
                        {
                        int32_t len = indexObject->getInt();
                        if (len >= LOWER_IMMED && len <= UPPER_IMMED)
                           generateTrg1Src1ImmInstruction(self(), TR::InstOpCode::addi, node, pointerReg, baseReg, len);
                        else
                           {
                           indexReg = self()->allocateRegister();
                           loadConstant(self(), node, len, indexReg);
                           generateTrg1Src2Instruction(self(), TR::InstOpCode::add, node, pointerReg, baseReg, indexReg);
                           self()->stopUsingRegister(indexReg);
                           }
                        }
                     else
                        generateTrg1Src2Instruction(self(), TR::InstOpCode::add, node, pointerReg, baseReg, indexReg);
                     }
                  }
               }
            // 64bit CR
            else if (comp->target().is64Bit() && comp->useCompressedPointers())
               {
               if (!(firstChild->getOpCodeValue() == TR::iu2l &&
                     firstChild->getFirstChild() &&
                     firstChild->getFirstChild()->getOpCodeValue() == TR::iloadi &&
                     firstChild->getFirstChild()->getNumChildren() == 1))
                  {
                  optDisabled = true;
                  }
               else
                  {
                  fromRegLoad = true;
                  pointerReg = firstChild->getFirstChild()->getFirstChild()->getRegister();
                  }
               }
            // 64bit - TODO
            else
               optDisabled = true;
            }

         if (!optDisabled)
            {
            TR::Register *condReg = self()->allocateRegister(TR_CCR);
            TR::Register *tempReg = self()->allocateRegister();

            // 32 bit only.... For -Xgc:noconcurrentmark, heapBase will be 0 and heapTop will be ok
            // Otherwise, for a 2.25Gb or bigger heap, heapTop will be 0.  Relying on correct JIT initialization
            uintptr_t heapTop = comp->getOptions()->getHeapTop() ? comp->getOptions()->getHeapTop() : 0xFFFFFFFF;

            if (pointerReg && (heapTop > comp->getOptions()->getHeapBase()))  // Check for gencon
               {
               TR::Register *temp3Reg = self()->allocateRegister();
               static bool prefetch2Ahead = (feGetEnv("TR_Prefetch2Ahead") != NULL);
               if (comp->target().is64Bit() && !comp->useCompressedPointers())
                  {
                  if (!prefetch2Ahead)
                     generateTrg1MemInstruction(self(), TR::InstOpCode::ld, node, temp3Reg, TR::MemoryReference::createWithDisplacement(self(), pointerReg, (int32_t)TR::Compiler->om.sizeofReferenceField(), 8));
                  else
                     generateTrg1MemInstruction(self(), TR::InstOpCode::ld, node, temp3Reg, TR::MemoryReference::createWithDisplacement(self(), pointerReg, (int32_t)(TR::Compiler->om.sizeofReferenceField()*2), 8));
                  }
               else
                  {
                  if (!prefetch2Ahead)
                     generateTrg1MemInstruction(self(), TR::InstOpCode::lwz, node, temp3Reg, TR::MemoryReference::createWithDisplacement(self(), pointerReg, (int32_t)TR::Compiler->om.sizeofReferenceField(), 4));
                  else
                     generateTrg1MemInstruction(self(), TR::InstOpCode::lwz, node, temp3Reg, TR::MemoryReference::createWithDisplacement(self(), pointerReg, (int32_t)(TR::Compiler->om.sizeofReferenceField()*2), 4));
                  }

               if (comp->getOptions()->getHeapBase() != NULL)
                  {
                  loadAddressConstant(self(), comp->compileRelocatableCode(), node, (intptr_t)(comp->getOptions()->getHeapBase()), tempReg);
                  generateTrg1Src2Instruction(self(), TR::InstOpCode::cmpl4, node, condReg, temp3Reg, tempReg);
                  generateConditionalBranchInstruction(self(), TR::InstOpCode::blt, node, endCtrlFlowLabel, condReg);
                  }
               if (heapTop != 0xFFFFFFFF)
                  {
                  loadAddressConstant(self(), comp->compileRelocatableCode(), node, (intptr_t)(heapTop-prefetchOffset), tempReg);
                  generateTrg1Src2Instruction(self(), TR::InstOpCode::cmpl4, node, condReg, temp3Reg, tempReg);
                  generateConditionalBranchInstruction(self(), TR::InstOpCode::bgt, node, endCtrlFlowLabel, condReg);
                  }
               TR::MemoryReference *targetMR = TR::MemoryReference::createWithDisplacement(self(), temp3Reg, (int32_t)0, 4);
               targetMR->forceIndexedForm(node, self());
               generateMemInstruction(self(), TR::InstOpCode::dcbt, node, targetMR); // use dcbt for prefetch next element

               self()->stopUsingRegister(temp3Reg);
               }

            if (!fromRegLoad)
               self()->stopUsingRegister(pointerReg);
            self()->stopUsingRegister(tempReg);
            self()->stopUsingRegister(condReg);
            }
            generateLabelInstruction(self(), TR::InstOpCode::label, node, endCtrlFlowLabel);
         }

      // Try prefetch all string objects, no apparent gain.  Disabled for now.
      if (!disableStringObjPrefetch && 0 &&
         node->getSymbolReference() &&
         !node->getSymbolReference()->isUnresolved() &&
         (node->getSymbolReference()->getSymbol()->getKind() == TR::Symbol::IsShadow) &&
         (node->getSymbolReference()->getCPIndex() >= 0))
         {
         int32_t len;
         const char *fieldName = node->getSymbolReference()->getOwningMethod(comp)->fieldSignatureChars(
            node->getSymbolReference()->getCPIndex(), len);

         if (fieldName && strstr(fieldName, "Ljava/lang/String;"))
            {
            TR::MemoryReference *targetMR = TR::MemoryReference::createWithDisplacement(self(), targetRegister, (int32_t)0, 4);
            targetMR->forceIndexedForm(node, self());
            generateMemInstruction(self(), TR::InstOpCode::dcbt, node, targetMR);
            }
         }
      }

   if (node->getOpCodeValue() == TR::aloadi ||
         (comp->target().is64Bit() &&
          comp->useCompressedPointers() &&
          (node->getOpCodeValue() == TR::iloadi || node->getOpCodeValue() == TR::irdbari) &&
          comp->getMethodHotness() >= hot))
      {
      TR::Node *firstChild = node->getFirstChild();
      optDisabled = disableIteratorPrefetch;
      if (!disableIteratorPrefetch)
         {
         // 32bit
         if (comp->target().is32Bit())
            {
            if (!(firstChild &&
                firstChild->getOpCodeValue() == TR::aiadd &&
                firstChild->isInternalPointer() &&
                (strstr(comp->fe()->sampleSignature(node->getOwningMethod(), 0, 0, self()->trMemory()),"java/util/TreeMap$UnboundedValueIterator.next()")
                || strstr(comp->fe()->sampleSignature(node->getOwningMethod(), 0, 0, self()->trMemory()),"java/util/ArrayList$Itr.next()"))
               ))
               {
               optDisabled = true;
               }
            }
         // 64bit cr
         else if (comp->target().is64Bit() && comp->useCompressedPointers())
            {
            if (!(firstChild &&
                firstChild->getOpCodeValue() == TR::aladd &&
                firstChild->isInternalPointer() &&
                (strstr(comp->fe()->sampleSignature(node->getOwningMethod(), 0, 0, self()->trMemory()),"java/util/TreeMap$UnboundedValueIterator.next()")
                || strstr(comp->fe()->sampleSignature(node->getOwningMethod(), 0, 0, self()->trMemory()),"java/util/ArrayList$Itr.next()"))
               ))
               {
               optDisabled = true;
               }
            }
         }

      // The use of this prefetching can cause a SEGV when the object array is allocated at the every end of the heap.
      // The GC will protected against the SEGV by adding a "tail-padding" page, but only when -XAggressive is enabled!
      if (!optDisabled && comp->getOption(TR_AggressiveOpts))
         {
         int32_t loopSize = 0;
         int32_t prefetchElementStride = 1;
         TR::Block *b = self()->getCurrentEvaluationBlock();
         TR_BlockStructure *blockStructure = b->getStructureOf();
         if (blockStructure)
            {
            TR_Structure *containingLoop = blockStructure->getContainingLoop();
            if (containingLoop)
               {
               TR_ScratchList<TR::Block> blocksInLoop(comp->trMemory());

               containingLoop->getBlocks(&blocksInLoop);
               ListIterator<TR::Block> blocksIt(&blocksInLoop);
               TR::Block *nextBlock;
               for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
                  {
                  loopSize += nextBlock->getNumberOfRealTreeTops();
                  }
               }
            }

         if (comp->useCompressedPointers())
            {
            prefetchElementStride = 2;
            }
         else
            {
            if (loopSize < 200) //comp->useCompressedPointers() is false && loopSize < 200.
               {
               prefetchElementStride = 4;
               }
            else if (loopSize < 300) //comp->useCompressedPointers() is false && loopsize >=200 && loopsize < 300.
               {
               prefetchElementStride = 2;
               }
            //If comp->useCompressedPointers() is false and loopsize >= 300, prefetchElementStride does not get changed.
            }

         // Look at the aiadd's children
         TR::Node *baseObject = firstChild->getFirstChild();
         TR::Register *baseReg = (baseObject) ? baseObject->getRegister() : NULL;
         TR::Node *indexObject = firstChild->getSecondChild();
         TR::Register *indexReg = (indexObject) ? indexObject->getRegister() : NULL;
         if (baseReg && indexReg && loopSize > 0)
            {
            TR::Register *tempReg = self()->allocateRegister();
            generateTrg1Src1ImmInstruction(self(), TR::InstOpCode::addi, node, tempReg, indexReg, prefetchElementStride * TR::Compiler->om.sizeofReferenceField());
            if (comp->target().is64Bit() && !comp->useCompressedPointers())
               {
               TR::MemoryReference *targetMR = TR::MemoryReference::createWithIndexReg(self(), baseReg, tempReg, 8);
               generateTrg1MemInstruction(self(), TR::InstOpCode::ld, node, tempReg, targetMR);
               }
            else
               {
               TR::MemoryReference *targetMR = TR::MemoryReference::createWithIndexReg(self(), baseReg, tempReg, 4);
               generateTrg1MemInstruction(self(), TR::InstOpCode::lwz, node, tempReg, targetMR);
               }

            if (comp->useCompressedPointers())
               {
               generateShiftLeftImmediateLong(self(), node, tempReg, tempReg, TR::Compiler->om.compressedReferenceShiftOffset());
               }
            TR::MemoryReference *target2MR =  TR::MemoryReference::createWithDisplacement(self(), tempReg, 0, 4);
            target2MR->forceIndexedForm(node, self());
            generateMemInstruction(self(), TR::InstOpCode::dcbt, node, target2MR);
            self()->stopUsingRegister(tempReg);
            }
         }
      }
   else if (node->getOpCodeValue() == TR::awrtbari &&
            comp->getMethodHotness() >= scorching &&
            comp->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P6) &&
              (comp->target().is32Bit() ||
                (comp->target().is64Bit() &&
                 comp->useCompressedPointers() &&
                 TR::Compiler->om.compressedReferenceShiftOffset() == 0
                )
              )
           )
      {
      // Take the source register of the store and apply on the prefetchOffset right away
      int32_t prefetchOffset = comp->findPrefetchInfo(node);
      if (prefetchOffset >= 0)
         {
         TR::MemoryReference *targetMR = TR::MemoryReference::createWithDisplacement(self(), targetRegister, prefetchOffset, TR::Compiler->om.sizeofReferenceAddress());
         targetMR->forceIndexedForm(node, self());
         generateMemInstruction(self(), TR::InstOpCode::dcbt, node, targetMR);
         }
      }
   }

TR::Linkage *
J9::Power::CodeGenerator::deriveCallingLinkage(TR::Node *node, bool isIndirect)
   {
   TR::SymbolReference *symRef    = node->getSymbolReference();
   TR::MethodSymbol    *callee    = symRef->getSymbol()->castToMethodSymbol();
   TR_J9VMBase         *fej9      = (TR_J9VMBase *)(self()->fe());

   static char * disableDirectNativeCall = feGetEnv("TR_DisableDirectNativeCall");

   // Clean-up: the fej9 checking seemed unnecessary
   if (!isIndirect && callee->isJNI() && fej9->canRelocateDirectNativeCalls() &&
       (node->isPreparedForDirectJNI() ||
        (disableDirectNativeCall == NULL && callee->getResolvedMethodSymbol()->canDirectNativeCall())))
      return self()->getLinkage(TR_J9JNILinkage);

   return self()->getLinkage(callee->getLinkageConvention());
   }
