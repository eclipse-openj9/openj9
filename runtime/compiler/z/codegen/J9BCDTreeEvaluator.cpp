/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

//On zOS XLC linker can't handle files with same name at link time
//This workaround with pragma is needed. What this does is essentially
//give a different name to the codesection (csect) for this file. So it
//doesn't conflict with another file with same name.
#pragma csect(CODE,"TRJ9ZBCDTreeEvalBase#C")
#pragma csect(STATIC,"TRJ9ZBCDTreeEvalBase#S")
#pragma csect(TEST,"TRJ9ZBCDTreeEvalBase#T")

#include <algorithm>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "j9modron.h"
#include "thrdsup.h"
#include "thrtypes.h"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/Machine.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/CompilerEnv.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "ras/DebugCounter.hpp"
#include "env/VMJ9.h"
#include "z/codegen/J9S390Snippet.hpp"
#include "z/codegen/S390J9CallSnippet.hpp"
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390HelperCallSnippet.hpp"
#include "z/codegen/S390Instruction.hpp"
#include "z/codegen/S390Register.hpp"
#include "z/codegen/SystemLinkage.hpp"

#define TR_PACKED_TO_DFP_NOABS   (0x8)
#define TR_PACKED_TO_DFP_ABS     (0x9)

// mask at arch(10) is ---P
//                     0123
//
// mask at arch(11) is S-PF
//                     0123

#define TR_DFP_TO_PACKED_DEFAULT_MASK_ARCH10       (0x0) ///< Encode plus sign as 0xc
// RTC 93015: Using TypeReduction to treat all pdnegs as arith ops at ARCH(11) means that a tree with ipdstore -> pdneg -> pdX would get
// converted to ipdstore -> dd2pd -> ddneg -> ddX. Since the dd2pd could generate a CPDT that stores in place (rather than using a ZAP),
// this could mean that a ddneg producing a negative zero won't get cleaned; we'll use an LCDFR to flip the sign bit in DFP, then use a CPDT
// to write the final value. If we've decided to clean on the store, the clean isn't being generated, so we end up with a negative zero.
// The simplest solution is to *ALWAYS* clean the sign on the CPDT at ARCH(11), as there's no performance penalty in doing so.
#define TR_DFP_TO_PACKED_DEFAULT_MASK_ARCH11       (0x9) ///< Result has a sign field (S=1), encode plus sign as 0xc (P=0) and clean (F=1)
#define TR_DFP_TO_PACKED_UNSIGNED_ARCH10           (0x1) ///< P: encode plus sign as 0xc (P=0) or as 0xf (P=1).
#define TR_DFP_TO_PACKED_UNSIGNED_ARCH11           (0x2) ///< P: encode plus sign as 0xc (P=0) or as 0xf (P=1).
#define TR_DFP_TO_PACKED_CLEAN_ARCH11              (0x1) ///< F: force positive zero (F=1)

#define TR_MAX_DFP_PACKED_FAST_SIZE  (32) ///< these conversion instructions slow down when the actual length is > 32
#define TR_MAX_DFP_PACKED_SIZE       (34) ///< max actual length (<=34) these conversion instructions can handle

TR::MemoryReference *
J9::Z::TreeEvaluator::asciiAndUnicodeToPackedHelper(TR::Node *node,
                                                    TR_PseudoRegister *targetReg,
                                                    TR::MemoryReference *sourceMR,
                                                    TR_PseudoRegister *childReg,
                                                    TR::CodeGenerator * cg)
   {
   TR::Node *child = node->getFirstChild();
   bool isUnicode = child->getType().isAnyUnicode();
   bool isZoned = child->getType().isAnyZoned();

   TR::DataType sourceType = TR::NoType;
   TR::Compilation *comp = cg->comp();
   if (isUnicode)
      sourceType = TR::UnicodeDecimal;
   else if (isZoned)
      sourceType = TR::ZonedDecimal;
   else
      TR_ASSERT(false,"unexpected type on node %s (%p)\n",child->getOpCode().getName(),child);

   TR_StorageReference *hint = node->getStorageReferenceHint();
   TR_StorageReference *targetStorageReference = NULL;
   int32_t destSize = isUnicode ? cg->getUnicodeToPackedFixedResultSize() : cg->getAsciiToPackedFixedResultSize();
   TR_ASSERT(TR::DataType::getBCDPrecisionFromSize(node->getDataType(), destSize) >= childReg->getDecimalPrecision(),
      "%s source precision of %d should not exceed the fixed precision of %d\n",
         node->getOpCode().getName(), childReg->getDecimalPrecision(), TR::DataType::getBCDPrecisionFromSize(node->getDataType(), destSize));

   if (hint)
      {
      if (childReg->isInitialized() && hint == childReg->getStorageReference())
         {
         TR_ASSERT( false,"ad2pd/ud2pd operands will overlap because child storageReference of ud2pd is initialized hint\n");
         }
      else
         {
         TR_ASSERT(hint->getSymbolSize() >= destSize, "ad2pd/ud2pd hint size of %d should be >= the fixed size of %d\n",hint->getSymbolSize(),destSize);
         targetStorageReference = hint;
         }
      }

   if (targetStorageReference == NULL)
      targetStorageReference = TR_StorageReference::createTemporaryBasedStorageReference(destSize, comp);

   targetReg->setStorageReference(targetStorageReference, node);

   int32_t sourcePrecision = childReg->getDecimalPrecision();
   bool isTruncation = sourcePrecision > node->getDecimalPrecision();
   int32_t pkxSourcePrecision = isTruncation ? node->getDecimalPrecision() : sourcePrecision;
   int32_t pkxSourceSize = TR::DataType::getSizeFromBCDPrecision(sourceType, pkxSourcePrecision);
   int32_t targetPrecision = pkxSourcePrecision;
   int32_t sourceEndByte = TR::DataType::getLeftMostByte(child->getDataType(), pkxSourceSize);

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tasciiAndUnicodeToPackedHelper %p : op %s, isTruncation=%s, fixedDestSize %d, targetRegPrec %d, sourcePrecision %d, sourceEndByte %d, sourceSize %d, pkuSourceSize %d\n",
         node,node->getOpCode().getName(),isTruncation?"yes":"no",destSize,targetPrecision,sourcePrecision,sourceEndByte,childReg->getSize(),pkxSourceSize);

   // For PKA/PKU the 1st operand (target) size is fixed at 16 bytes and the 2nd operand (source) is variable.
   // For this reason use left, instead of right, aligned memory references so the correct alignment is done for both operands
   // (using right aligned references with SS1 would apply the same bump to both operands)
   TR::MemoryReference *destMR = generateS390LeftAlignedMemoryReference(node, targetReg->getStorageReference(), cg, destSize);
   sourceMR = reuseS390LeftAlignedMemoryReference(sourceMR, child, childReg->getStorageReference(), cg, sourceEndByte);

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tgen %s with fixed dest size of %d and source size %d. Set targetRegPrec to sourcePrec (%d)\n",isUnicode?"PKU":"PKA",destSize,pkxSourceSize,sourcePrecision);

   generateSS1Instruction(cg, isUnicode ? TR::InstOpCode::PKU : TR::InstOpCode::PKA, node, pkxSourceSize-1, destMR, sourceMR);

   int32_t destSizeAsCeilingPrecision = TR::DataType::byteLengthToPackedDecimalPrecisionCeiling(destSize);
   if (destSizeAsCeilingPrecision > pkxSourcePrecision)
      targetReg->addRangeOfZeroDigits(pkxSourcePrecision, destSizeAsCeilingPrecision);

   if (node->getOpCode().isSetSign())
      {
      TR::Node *setSignNode = node->getSetSignValueNode();
      TR_ASSERT(setSignNode->getOpCode().isLoadConst() && setSignNode->getOpCode().getSize() <= 4,"expecting a <= 4 size integral constant set sign amount on node %p\n",setSignNode);
      int32_t sign = setSignNode->get32bitIntegralValue();
      if (sign == TR::DataType::getPreferredPlusCode())
         targetReg->setKnownSignCode(TR::DataType::getPreferredPlusCode());
      else
         cg->genSignCodeSetting(node, targetReg, targetReg->getSize(), destMR, sign, targetReg, 0, false); // numericNibbleIsZero=false
      cg->decReferenceCount(setSignNode);
      }
   else
      {
      // PKA/PKU always sets the preferred positive code and therefore a known clean sign is generated.
      targetReg->setKnownSignCode(TR::DataType::getPreferredPlusCode());
      }

   targetReg->setDecimalPrecision(targetPrecision);
   targetReg->transferDataState(childReg);
   targetReg->setIsInitialized();
   node->setRegister(targetReg);
   return destMR;
   }

TR::Register *
J9::Z::TreeEvaluator::ud2pdVectorEvaluatorHelper(TR::Node * node, TR::CodeGenerator * cg)
   {
   // 1. use ud2pd helper to put ud->pd in some storage reference
   TR_PseudoRegister *packedPseudoReg = cg->allocatePseudoRegister(node->getDataType());
   TR::Node *child = node->getFirstChild();
   TR_PseudoRegister *childReg = cg->evaluateBCDNode(child);
   childReg = cg->privatizeBCDRegisterIfNeeded(node, child, childReg);
   TR::MemoryReference *sourceMR = generateS390RightAlignedMemoryReference(child, childReg->getStorageReference(), cg);
   asciiAndUnicodeToPackedHelper(node, packedPseudoReg, sourceMR, childReg, cg);

   // 2. load packed decimal from storage reference to register.
   TR::Register * targetReg = cg->allocateRegister(TR_VRF);
   TR::MemoryReference * pdSourceMR = generateS390RightAlignedMemoryReference(node,
                                                                              packedPseudoReg->getStorageReference(),
                                                                              cg);

   // PKU always puts the result into 16 bytes space
   uint8_t lengthToLoad = TR_VECTOR_REGISTER_SIZE - 1;
   generateVSIInstruction(cg, TR::InstOpCode::VLRL, node, targetReg, pdSourceMR, lengthToLoad);

   cg->decReferenceCount(child);
   node->setRegister(targetReg);
   return targetReg;
   }

/**
 * Handles TR::ud2pd
 */
TR::Register *
J9::Z::TreeEvaluator::ud2pdEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   cg->traceBCDEntry("ud2pd",node);
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(cg->comp(), "PD-Op/%s", node->getOpCode().getName()),
                            1, TR::DebugCounter::Cheap);
   TR::Register* targetReg = NULL;

   static char* isVectorBCDEnv = feGetEnv("TR_enableVectorBCD");
   if(TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() &&
           !cg->comp()->getOption(TR_DisableVectorBCD) ||
           isVectorBCDEnv)
      {
      targetReg = ud2pdVectorEvaluatorHelper(node, cg);
      }
   else
      {
      targetReg = cg->allocatePseudoRegister(node->getDataType());
      TR::Node *child = node->getFirstChild();
      TR_PseudoRegister *childReg = cg->evaluateBCDNode(child);
      childReg = cg->privatizeBCDRegisterIfNeeded(node, child, childReg);
      TR::MemoryReference *sourceMR = generateS390RightAlignedMemoryReference(child, childReg->getStorageReference(), cg);
      asciiAndUnicodeToPackedHelper(node, static_cast<TR_PseudoRegister*>(targetReg), sourceMR, childReg, cg);
      cg->decReferenceCount(child);
      node->setRegister(targetReg);
      }

   cg->traceBCDExit("ud2pd",node);
   return targetReg;
   }

/**
 * Handles TR::udsl2pd, TR::udst2pd
 */
TR::Register *
J9::Z::TreeEvaluator::udsl2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   cg->traceBCDEntry("udsl2pd",node);
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(cg->comp(), "PD-Op/%s", node->getOpCode().getName()),
                            1, TR::DebugCounter::Cheap);
   TR_PseudoRegister *targetReg = cg->allocatePseudoRegister(node->getDataType());
   TR::Node *child = node->getFirstChild();
   TR_PseudoRegister *childReg = cg->evaluateBCDNode(child);
   childReg = cg->privatizeBCDRegisterIfNeeded(node, child, childReg);

   bool isSrcTrailingSign = (child->getDataType() == TR::UnicodeDecimalSignTrailing);
   int32_t sourceSignEndByte = isSrcTrailingSign ? TR::DataType::getUnicodeSignSize() : childReg->getSize();
   TR::MemoryReference *sourceMR = generateS390LeftAlignedMemoryReference(child, childReg->getStorageReference(), cg, sourceSignEndByte);
   TR::MemoryReference *destMR = asciiAndUnicodeToPackedHelper(node, targetReg, sourceMR, childReg, cg);

   if (!node->getOpCode().isSetSign())
      {
      TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
      TR::LabelSymbol * cFlowRegionEnd = generateLabelSymbol(cg);

      bool isImplicitValue = node->getNumChildren() < 2;

      TR::RegisterDependencyConditions * deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, isImplicitValue ? 4 : 2, cg);

      if (destMR->getIndexRegister())
         deps->addPostConditionIfNotAlreadyInserted(destMR->getIndexRegister(), TR::RealRegister::AssignAny);
      if (destMR->getBaseRegister())
         deps->addPostConditionIfNotAlreadyInserted(destMR->getBaseRegister(), TR::RealRegister::AssignAny);

      bool isTruncation = childReg->getDecimalPrecision() > node->getDecimalPrecision();

      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tudsl2pdEvaluator %p : op %s, isTruncation=%s, targetReg->isInit=%s, targetRegSize=%d, targetRegPrec=%d, srcRegSize=%d, srcRegPrec=%d, sourceSignEndByte=%d\n",
            node,node->getOpCode().getName(),isTruncation?"yes":"no",targetReg->isInitialized()?"yes":"no",targetReg->getSize(),targetReg->getDecimalPrecision(),childReg->getSize(),childReg->getDecimalPrecision(),sourceSignEndByte);

      if (isImplicitValue)
         {
         if (sourceMR->getIndexRegister())
            deps->addPostConditionIfNotAlreadyInserted(sourceMR->getIndexRegister(), TR::RealRegister::AssignAny);
         if (sourceMR->getBaseRegister())
            deps->addPostConditionIfNotAlreadyInserted(sourceMR->getBaseRegister(), TR::RealRegister::AssignAny);

         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart, deps);
         cFlowRegionStart->setStartInternalControlFlow();

         // The primary (and currently the only) consumer of BCD evaluators in Java is the DAA intrinsics
         // library. The DAA library assumes all BCD types are positive, unless an explicit negative sign
         // code is present. Because of this deviation from the COBOL treatment of sign codes we must
         // take a specialized control path when generating instructions for Java.

         if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z10))
            {
            generateSILInstruction(cg, TR::InstOpCode::CLHHSI, node, generateS390LeftAlignedMemoryReference(*sourceMR, node, 0, cg, sourceSignEndByte), 0x002D);
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, cFlowRegionEnd);
            }
         else
            {
            generateSIInstruction(cg, TR::InstOpCode::CLI, node, generateS390LeftAlignedMemoryReference(*sourceMR, node, 0, cg, sourceSignEndByte), 0x00);
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, cFlowRegionEnd);

            generateSIInstruction(cg, TR::InstOpCode::CLI, node, generateS390LeftAlignedMemoryReference(*sourceMR, node, 1, cg, sourceSignEndByte), 0x2D);
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, cFlowRegionEnd);
            }
         }
      else
         {
         TR::Node *minusSign = node->getSecondChild();

         TR::MemoryReference *minusSignMR = generateS390ConstantAreaMemoryReference(cg, minusSign, true); // forSS=true

         generateSS1Instruction(cg, TR::InstOpCode::CLC, node,
                                TR::DataType::getUnicodeSignSize()-1,
                                generateS390LeftAlignedMemoryReference(*sourceMR, node, 0, cg, sourceSignEndByte),
                                minusSignMR);

         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart, deps);
         cFlowRegionStart->setStartInternalControlFlow();

         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK7, node, cFlowRegionEnd);
         }

      cg->genSignCodeSetting(node, NULL, targetReg->getSize(),
                                     generateS390RightAlignedMemoryReference(*destMR, node, 0, cg),
                                     TR::DataType::getPreferredMinusCode(), targetReg, 0, false); // numericNibbleIsZero=false

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, deps);
      cFlowRegionEnd->setEndInternalControlFlow();

      targetReg->resetSignState();
      targetReg->setHasKnownPreferredSign();

      if (!isTruncation)
         targetReg->transferCleanSign(childReg);
      else
         traceMsg(comp,"\tudsx2p is a truncation (srcRegPrec %d > nodePrec %d) so do not transfer any clean sign flags\n",childReg->getDecimalPrecision(),node->getDecimalPrecision());
      }

   //at this point targetReg is PseudoRegister that has converted Packed decimal value.
   static char* isVectorBCDEnv = feGetEnv("TR_enableVectorBCD");
   if (TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() &&
           !cg->comp()->getOption(TR_DisableVectorBCD) ||
           isVectorBCDEnv)
      {
      TR::Register * pdVectorTargetReg = cg->allocateRegister(TR_VRF);
      TR::MemoryReference * pdSourceMR = generateS390RightAlignedMemoryReference(node,
                                                                                 targetReg->getStorageReference(),
                                                                                 cg);
      //PKU always puts the result into 16 bytes space
      uint8_t lengthToLoad = TR_VECTOR_REGISTER_SIZE - 1;
      generateVSIInstruction(cg, TR::InstOpCode::VLRL, node, pdVectorTargetReg, pdSourceMR, lengthToLoad);

      cg->decReferenceCount(child);
      node->setRegister(pdVectorTargetReg);
      cg->traceBCDExit("udsl2pd",node);
      return pdVectorTargetReg;
      }
   else
      {
      cg->decReferenceCount(child);
      node->setRegister(targetReg);
      cg->traceBCDExit("udsl2pd",node);
      return targetReg;
      }
   }

/**
 * Handles pd2udsl,pd2udst, where the Unicode decimal signs are separate.
 */
TR::Register *
J9::Z::TreeEvaluator::pd2udslEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   cg->traceBCDEntry("pd2udsl",node);
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(cg->comp(), "PD-Op/%s", node->getOpCode().getName()),
                            1, TR::DebugCounter::Cheap);

   TR::Node* childNode = node->getFirstChild();
   TR::Compilation *comp = cg->comp();
   TR_PseudoRegister *childReg = NULL;
   TR::MemoryReference *sourceMR = NULL;
   TR_StorageReference* pdStorageRef = NULL;

   static char* isVectorBCDEnv = feGetEnv("TR_enableVectorBCD");
   if(TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() && !comp->getOption(TR_DisableVectorBCD) || isVectorBCDEnv)
      {
      // Perform an intermediate vector store. See pd2udVectorEvaluateHelper().
      TR::Register* pdValueReg = cg->evaluate(childNode);
      pdStorageRef = TR_StorageReference::createTemporaryBasedStorageReference(TR_VECTOR_REGISTER_SIZE, comp);
      pdStorageRef->setIsSingleUseTemporary();

      TR::MemoryReference* pdMR = generateS390RightAlignedMemoryReference(node, pdStorageRef, cg);
      sourceMR = pdMR;

      childReg = cg->allocatePseudoRegister(childNode->getDataType());
      childReg->setIsInitialized();
      childReg->setSize(childNode->getSize());
      childReg->setHasKnownValidData();
      childReg->setDecimalPrecision(childNode->getDecimalPrecision());

      generateVSIInstruction(cg, TR::InstOpCode::VSTRL, node, pdValueReg, pdMR, TR_VECTOR_REGISTER_SIZE - 1);

      }
   else
      {
      int32_t byteLength = TR::DataType::packedDecimalPrecisionToByteLength(node->getDecimalPrecision());
      childReg = cg->evaluateBCDNode(childNode);
      childReg = cg->privatizeBCDRegisterIfNeeded(node, childNode, childReg);
      sourceMR = cg->materializeFullBCDValue(childNode, childReg,
                                             cg->getPackedToUnicodeFixedSourceSize(),
                                             byteLength);
      }

   // One of two sequences generated by the reset of this evaluator:
   // for non-setSign ops when the knownSign=negative (known positive signs are more common so '+' is the initial/default setting)
   //
   //    MVC      [destSign],[minusSign]  // [sign] <- 002B '+'
   //    UNPKU    [destData],[src]
   //    MVI      [destSign+1],0x2D       // '-'
   //
   // for non-setSign ops (pd2udsl/pd2udst)
   //
   //    MVC      [destSign],[minusSign]  // [sign] <- 002B '+'
   //    UNPKU    [destData],[src]
   //    BRC      0x8,done                // if src sign is + (cc=0) we are done, otherwise in '-' (cc=1) and invalid (cc=3) case fall through and set '-' sign
   //    MVI      [destSign+1],0x2D       // '-'
   // done:
   //
   // The MVC/UNPKU are generated by the shared routine packedToUnicodeHelper and the BRC/MVI by this routine


   TR_PseudoRegister *targetReg = cg->allocatePseudoRegister(node->getDataType());
   TR::MemoryReference *destMR = packedToUnicodeHelper(node, targetReg, sourceMR, childReg, true, cg, pdStorageRef); // isSeparateSign=true

   int32_t destSignEndByte = (node->getDataType() == TR::UnicodeDecimalSignTrailing) ? TR::DataType::getUnicodeSignSize() : targetReg->getSize();

   if (childReg->hasKnownSignCode())
      {
      int32_t convertedSign = TR::DataType::convertSignEncoding(childNode->getDataType(), node->getDataType(), childReg->getKnownSignCode());
      if (convertedSign == TR::DataType::getNationalSeparateMinus())
         {
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tchildReg has negative knownSignCode 0x%x so generate an MVI of the converted sign 0x%x\n",childReg->getKnownSignCode(),convertedSign);
         generateSIInstruction(cg, TR::InstOpCode::MVI, node, generateS390LeftAlignedMemoryReference(*destMR, node, 1, cg, destSignEndByte), convertedSign);
         }
      else
         {
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tchildReg has positive knownSignCode 0x%x so no more codegen is needed (an MVC of 002B was already done)\n", childReg->getKnownSignCode());
         TR_ASSERT(convertedSign == TR::DataType::getNationalSeparatePlus(), "converted sign should be nationalSeparatePlusSign of 0x%x and not 0x%x\n", TR::DataType::getNationalSeparatePlus(), convertedSign);
         }
      targetReg->setKnownSignCode(convertedSign);
      }
   else
      {
      TR_ASSERT(cg->getAppendInstruction()->getOpCodeValue() == TR::InstOpCode::UNPKU,
                "the previous instruction should be an UNPKU\n");

      TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
      TR::LabelSymbol * cFlowRegionEnd = generateLabelSymbol(cg);

      TR::RegisterDependencyConditions * targetMRDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg);

      if (destMR->getIndexRegister())
         targetMRDeps->addPostConditionIfNotAlreadyInserted(destMR->getIndexRegister(), TR::RealRegister::AssignAny);
      if (destMR->getBaseRegister())
         targetMRDeps->addPostConditionIfNotAlreadyInserted(destMR->getBaseRegister(), TR::RealRegister::AssignAny);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart, targetMRDeps);
      cFlowRegionStart->setStartInternalControlFlow();

      // DAA library assumes all BCD types are positive, unless an explicit negative sign code is present
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK9, node, cFlowRegionEnd);

      TR_ASSERT(TR::DataType::getNationalSeparateMinus() <= 0xFF, "expecting nationalSeparateMinusSign to be <= 0xFF and not 0x%x\n", TR::DataType::getNationalSeparateMinus());
      generateSIInstruction(cg, TR::InstOpCode::MVI, node, generateS390LeftAlignedMemoryReference(*destMR, node, 1, cg, destSignEndByte), TR::DataType::getNationalSeparateMinus());

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, targetMRDeps);
      cFlowRegionEnd->setEndInternalControlFlow();

      targetReg->setHasKnownPreferredSign();
      }

   cg->decReferenceCount(childNode);
   node->setRegister(targetReg);
   cg->traceBCDExit("pd2udsl",node);
   return targetReg;
   }

TR::Register *
J9::Z::TreeEvaluator::pd2udEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(cg->comp(), "PD-Op/%s", node->getOpCode().getName()),
                            1, TR::DebugCounter::Cheap);
   TR_PseudoRegister *childReg = cg->evaluateBCDNode(child);
   TR_PseudoRegister* targetReg = cg->allocatePseudoRegister(node->getDataType());
   childReg = cg->privatizeBCDRegisterIfNeeded(node, child, childReg);
   int32_t byteLength = TR::DataType::packedDecimalPrecisionToByteLength(node->getDecimalPrecision());
   TR::MemoryReference *sourceMR = cg->materializeFullBCDValue(child,
                                                               childReg,
                                                               cg->getPackedToUnicodeFixedSourceSize(),
                                                               byteLength);

   packedToUnicodeHelper(node, targetReg, sourceMR, childReg, false, cg, NULL); // isSeparateSign=false

   cg->decReferenceCount(child);
   return targetReg;
   }

TR::Register *
J9::Z::TreeEvaluator::pd2udVectorEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   // 1. Evaluate child node and get a packed decimal in vector register
   TR::Node* childNode = node->getFirstChild();
   TR::Register* pdValueReg = cg->evaluate(childNode);

   // 2. Create a temp storage reference of size 16 bytes and dump all vector register contents there, to be picked up by UNPKU later
   //    This intermediate vector store is needed because vectorized pdloadi puts packed decimal in registers;
   //    but UNPKU is an SS instruction that takes inputs from memory.
   TR_StorageReference* pdStorageRef = TR_StorageReference::createTemporaryBasedStorageReference(TR_VECTOR_REGISTER_SIZE, cg->comp());
   pdStorageRef->setIsSingleUseTemporary();

   TR::MemoryReference* pdMR = generateS390RightAlignedMemoryReference(node, pdStorageRef, cg, true, true);
   generateVSIInstruction(cg, TR::InstOpCode::VSTRL, node, pdValueReg, pdMR, TR_VECTOR_REGISTER_SIZE - 1);

   // 3. Allocate and setup childReg PseudoRegister
   TR_PseudoRegister* childReg = cg->allocatePseudoRegister(childNode->getDataType());
   childReg->setIsInitialized();
   childReg->setSize(childNode->getSize());
   childReg->setDecimalPrecision(childNode->getDecimalPrecision());
   childReg->setHasKnownValidData();

   // 4. Generate UNPKU to unpack pdMR content to targetReg PseudoRegister
   TR_PseudoRegister* targetReg = cg->allocatePseudoRegister(node->getDataType());
   packedToUnicodeHelper(node, targetReg, pdMR, childReg, false, cg, pdStorageRef);  // isSeparateSign=false

   cg->decReferenceCount(childNode);
   return targetReg;
   }

TR::Register *
J9::Z::TreeEvaluator::pd2udEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   cg->traceBCDEntry("pd2ud",node);
   TR::Register* targetReg = NULL;
   static char* isVectorBCDEnv = feGetEnv("TR_enableVectorBCD");
   if(TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() && !cg->comp()->getOption(TR_DisableVectorBCD) || isVectorBCDEnv)
      {
      targetReg = pd2udVectorEvaluatorHelper(node, cg);
      }
   else
      {
      targetReg = pd2udEvaluatorHelper(node, cg);
      }

   node->setRegister(targetReg);
   cg->traceBCDExit("pd2ud",node);
   return targetReg;
   }

/**
 * \brief This evaluator helper is invoked by pd2ud Evaluator and pd2udsl Evaluator to generate unpack unicode
 * instruction (UNPKU).
 *
 * \param node              Parent node object.
 * \param targetReg         PseudoRegister object for the parent node (the node)
 * \param sourceMR          MemoryRefernece object pointer
 * \param childReg          PseudoRegister object for the child node (e.g. pdloadi node)
 * \param isSeparateSign    True if the operation is pd2udsl or pd2udst, which all have separate sign code. False
 *                          if it's pd2ud.
 * \param cg                The codegen object
 * \param srcStorageReference If not null, this replaces the childReg's StorageReference for unpack to unicode
*/
TR::MemoryReference *
J9::Z::TreeEvaluator::packedToUnicodeHelper(TR::Node *node,
                                            TR_PseudoRegister *targetReg,
                                            TR::MemoryReference *sourceMR,
                                            TR_PseudoRegister *childReg,
                                            bool isSeparateSign,
                                            TR::CodeGenerator * cg,
                                            TR_StorageReference* srcStorageReference)
   {
   TR::Node *child = node->getFirstChild();
   TR_StorageReference *hint = node->getStorageReferenceHint();
   TR_StorageReference *targetStorageReference = NULL;
   TR::Compilation *comp = cg->comp();

   int32_t destSize = node->getStorageReferenceSize();

   if (hint)
      {
      if (childReg->isInitialized() && hint == childReg->getStorageReference())
         {
         TR_ASSERT( false,"pd2ud operands will overlap because child storageReference of pd2ud is initialized hint\n");
         }
      else
         {
         if (destSize <= hint->getSymbolSize())
            targetStorageReference = hint;
         else
            TR_ASSERT(false,"pd2ud destSize (%d) should be <= hint size (%d)\n",destSize,hint->getSymbolSize());
         }
      }

   if (targetStorageReference == NULL)
      targetStorageReference = TR_StorageReference::createTemporaryBasedStorageReference(destSize, comp);

   targetReg->setStorageReference(targetStorageReference, node);

   int32_t unpkuDestPrecision = node->getDecimalPrecision();
   targetReg->setDecimalPrecision(unpkuDestPrecision);
   int32_t unpkuDestSize = TR::DataType::getSizeFromBCDPrecision(TR::UnicodeDecimal, unpkuDestPrecision);
   int32_t unpkuDestEndByte = TR::DataType::getLeftMostByte(node->getDataType(), unpkuDestSize);

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tpackedToUnicodeHelper %p : op %s, targetRegSize %d, targetRegPrec %d, srcRegSize %d, srcRegPrec %d\n",
         node,node->getOpCode().getName(),targetReg->getSize(),targetReg->getDecimalPrecision(),childReg->getSize(),childReg->getDecimalPrecision());

   // For UNPKU the 1st operand (target-unicode) size is variable and the 2nd operand (source-packed) is fixed at 16 bytes.
   // For this reason use left, instead of right, aligned memory references so the correct alignment is done for both operands
   // (using right aligned references with SS1 would apply the same bump to both operands)
   TR::MemoryReference *destMR = generateS390LeftAlignedMemoryReference(node, targetReg->getStorageReference(), cg, unpkuDestEndByte);
   // The sourceMR should have been created by calling materializeFullBCDValue to ensure it is large enough to be used in the UNPKU
   int32_t fixedSourceSize = cg->getPackedToUnicodeFixedSourceSize();

   TR_ASSERT(sourceMR->getStorageReference()->getSymbolSize() >= fixedSourceSize,
      "source memRef %d is not large enough to be used in the UNPKU (%d)\n",sourceMR->getStorageReference()->getSymbolSize(),fixedSourceSize);

   sourceMR = reuseS390LeftAlignedMemoryReference(sourceMR, child,
                                                  (srcStorageReference == NULL) ? childReg->getStorageReference() : srcStorageReference,
                                                  cg, fixedSourceSize);

   if (isSeparateSign)
      {
      //TR_ASSERT((node->getOpCode().isSetSign() && node->getNumChildren() == 3) || (node->getNumChildren() == 2),
      //   "expected two (or three if setSign) children on %s and not %d child(ren)\n",node->getOpCode().getName(),node->getNumChildren());
      int32_t destSignEndByte = (node->getDataType() == TR::UnicodeDecimalSignTrailing) ? TR::DataType::getUnicodeSignSize() : unpkuDestEndByte + TR::DataType::getUnicodeSignSize();

      bool isImplicitValue = node->getNumChildren() < 2;

      if (isImplicitValue)
         {
         if (cg->traceBCDCodeGen())
            traceMsg(comp, "\tgen 2 MVIs of unicode sign with size of %d and destSignEndByte of %d\n", TR::DataType::getUnicodeSignSize(),destSignEndByte);
         generateSIInstruction(cg, TR::InstOpCode::MVI, node,
                               generateS390LeftAlignedMemoryReference(*destMR, node, 0, cg, destSignEndByte), 0x00);
         generateSIInstruction(cg, TR::InstOpCode::MVI, node,
                               generateS390LeftAlignedMemoryReference(*destMR, node, 1, cg, destSignEndByte), 0x2B);
         }
      else
         {
         TR::Node *signNode = node->getSecondChild();
         TR::MemoryReference *signMR = generateS390ConstantAreaMemoryReference(cg, signNode, true); // forSS=true
         if (cg->traceBCDCodeGen())
            traceMsg(comp, "\tgen MVC of unicode sign with size of %d and destSignEndByte of %d\n", TR::DataType::getUnicodeSignSize(),destSignEndByte);
         generateSS1Instruction(cg, TR::InstOpCode::MVC, node,
                                TR::DataType::getUnicodeSignSize()-1,
                                generateS390LeftAlignedMemoryReference(*destMR, node, 0, cg, destSignEndByte),
                                signMR);
         }
      if (node->getOpCode().isSetSign())
         {
         TR::Node *setSignValue = node->getSetSignValueNode();
         if (setSignValue->getOpCode().isLoadConst() && setSignValue->getOpCode().getSize() <= 4)
            {
            targetReg->setKnownSignCode(setSignValue->get32bitIntegralValue());
            }
         }
      }

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tgen UNPKU: unpkuDestSize %d, destEndByte %d and fixed source size %d\n",unpkuDestSize,unpkuDestEndByte,fixedSourceSize);

   generateSS1Instruction(cg, TR::InstOpCode::UNPKU, node,
                          unpkuDestSize-1,
                          destMR,
                          sourceMR);

   targetReg->transferDataState(childReg);
   targetReg->setIsInitialized();
   node->setRegister(targetReg);
   return destMR;
   }

void
J9::Z::TreeEvaluator::zonedToZonedSeparateSignHelper(TR::Node *node, TR_PseudoRegister *srcReg, TR_PseudoRegister *targetReg, TR::MemoryReference *sourceMR, TR::MemoryReference *destMR, TR::CodeGenerator * cg)
   {
   TR_ASSERT(targetReg->isInitialized(),"targetRegister must be initialized before calling zonedToZonedSeparateSignHelper\n");
   targetReg->resetSignState(); // reset any incoming sign state now as sign is being moved from embedded to separate by this routine (so embedded setting is no longer valid)
   bool isSetSign = node->getOpCode().isSetSign();
   int32_t sign = 0;
   TR::Node *signCodeNode = NULL;
   TR::Compilation *comp = cg->comp();

   if (isSetSign)
      {
      signCodeNode = node->getSecondChild();
      TR_ASSERT(signCodeNode->getOpCode().isLoadConst(),"excepting zdsle2zdSetSign sign code to be a const\n");
      sign = signCodeNode->get32bitIntegralValue();
      }
   bool isDestTrailingSign = (node->getDataType() == TR::ZonedDecimalSignTrailingSeparate);
   bool isTruncation = false;
   int32_t digitsToClear = 0;
   if (node->getDecimalPrecision() < targetReg->getDecimalPrecision())
      isTruncation = true;
   else if (node->getDecimalPrecision() > targetReg->getDecimalPrecision())
      digitsToClear = node->getDecimalPrecision()-targetReg->getDecimalPrecision();

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tzonedToZonedSeparateSignHelper %p : op %s, isTruncation=%s, targetReg->knownSign=0x%x, trgSignIsZone=%s, targetReg->size=%d, targetRegPrec=%d, , digitsToClear=%d, (isSetSign=%s, sign 0x%x)\n",
         node,node->getOpCode().getName(),isTruncation?"yes":"no",targetReg->hasKnownOrAssumedSignCode() ? targetReg->getKnownOrAssumedSignCode() : 0,targetReg->knownOrAssumedSignIsZone()?"yes":"no",
         targetReg->getSize(),targetReg->getDecimalPrecision(),digitsToClear,isSetSign?"yes":"no",sign);

   TR_ASSERT(!isTruncation,"a zd2zdsxs operation should not truncate\n");
   if (digitsToClear > 0)
      {
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tdigitsToClear > 0 (%d) so set upper bytes to 0x%x and set targetRegPrec to nodePrec %d\n",digitsToClear,TR::DataType::getZonedZeroCode(),node->getDecimalPrecision());
      int32_t endByte = isDestTrailingSign ? node->getSize() : node->getSize() - TR::DataType::getZonedSignSize();
      cg->genZeroLeftMostZonedBytes(node, targetReg, endByte, digitsToClear, generateS390LeftAlignedMemoryReference(*destMR, node, 0, cg, endByte));
      targetReg->setDecimalPrecision(node->getDecimalPrecision());
      }

   int32_t endByteForDestSign = isDestTrailingSign ? TR::DataType::getZonedSignSize() : targetReg->getSize();
   TR::MemoryReference *destSignCodeMR = generateS390LeftAlignedMemoryReference(*destMR, node, 0, cg, endByteForDestSign);

   int32_t endByteForSourceSign = isDestTrailingSign ? (TR::DataType::getZonedSignSize() + TR::DataType::getZonedSignSize()) : TR::DataType::getZonedSignSize();
   TR::MemoryReference *srcSignCodeMR = generateS390LeftAlignedMemoryReference(*destMR, node, 0, cg, endByteForSourceSign);

   // no 'invalid sign' message is ever required for a setSign operation or when a known (but *not* assumed) sign is 0xc,0xd or 0xf
   intptr_t litPoolOffset = 0;
   if (isSetSign || (srcReg->hasKnownSignCode() && srcReg->knownSignIsEmbeddedPreferredOrUnsigned()))
      {
      int32_t signToSet = isSetSign ? sign :
                                      TR::DataType::convertSignEncoding(TR::ZonedDecimal, node->getDataType(), srcReg->getKnownSignCode());
      bool srcSignAlreadyZone = srcReg->knownOrAssumedSignIsZone(); // || targetReg->temporaryKnownSignCodeIs(TR::DataType::getZonedValue());
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\t%s case so gen MVI to set target sign to 0x%x (from source sign 0x%x) and do %sgen OI because srcReg->knownOrAssumedSignIsZone() = %s\n",
            isSetSign?"isSetSign=true":"srcReg->hasKnownSignCode",
            signToSet,
            isSetSign?sign:srcReg->getKnownSignCode(),
            srcSignAlreadyZone?"not ":"",
            srcSignAlreadyZone?"true":"false");

      TR_ASSERT(signToSet == TR::DataType::getZonedSeparatePlus() || signToSet == TR::DataType::getZonedSeparateMinus(),
         "signToSet value should be 0x%x ('+') or 0x%x ('-') and not 0x%x\n", TR::DataType::getZonedSeparatePlus(), TR::DataType::getZonedSeparateMinus(), sign);
      if (!srcSignAlreadyZone)
         {
         generateSIInstruction(cg, TR::InstOpCode::OI, node, srcSignCodeMR, TR::DataType::getZonedCode());
         }
      generateSIInstruction(cg, TR::InstOpCode::MVI, node, destSignCodeMR, (signToSet & 0xFF));
      targetReg->setKnownSignCode(signToSet);
      }
   else if (srcReg->hasKnownCleanSign())
      {
      TR_ASSERT(TR::DataType::getZonedSeparatePlus() == 0x4E && TR::DataType::getZonedSeparateMinus() == 0x60, "zd2zdsxs sequence only works when plus sign is 0x4E and minus sign is 0x60\n");
      TR::Register *tempReg1 = cg->allocateRegister(TR_GPR);
      TR::Register *tempReg2 = cg->allocateRegister(TR_GPR);

      generateRXInstruction(cg, TR::InstOpCode::IC, node, tempReg1, generateS390LeftAlignedMemoryReference(*srcSignCodeMR, node, 0, cg, srcSignCodeMR->getLeftMostByte()));

      generateRIInstruction(cg, TR::InstOpCode::NILL, node, tempReg1, 0x10);
      generateRSInstruction(cg, TR::InstOpCode::RLL, node, tempReg2, tempReg1, 29);  // rotate right by 3 (32-3=29)
      if (!targetReg->knownSignIsZone())
         {
         generateSIInstruction(cg, TR::InstOpCode::OI, node, generateS390LeftAlignedMemoryReference(*srcSignCodeMR, node, 0, cg, srcSignCodeMR->getLeftMostByte()), TR::DataType::getZonedCode());
         }
      generateRRInstruction(cg, TR::InstOpCode::OR, node, tempReg2, tempReg1);
      generateRIInstruction(cg, TR::InstOpCode::AHI, node, tempReg2, 0x4E);
      generateRXInstruction(cg, TR::InstOpCode::STC, node, tempReg2, destSignCodeMR);
      cg->stopUsingRegister(tempReg1);
      cg->stopUsingRegister(tempReg2);
      targetReg->setHasKnownPreferredSign();
      if (!isTruncation)
         targetReg->setHasKnownCleanSign();
      }
   else
      {
      // DAA library assumes all BCD types are positive, unless an explicit negative sign code is present
      TR::LabelSymbol * processSign     = generateLabelSymbol(cg);
      TR::LabelSymbol * processPositive = generateLabelSymbol(cg);
      TR::LabelSymbol * processNegative = generateLabelSymbol(cg);
      TR::LabelSymbol * cFlowRegionEnd  = generateLabelSymbol(cg);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processSign);
      processSign->setStartInternalControlFlow();

      // A negative sign code is represented by 0xB and 0xD (1011 and 1101 in binary). Due to the
      // symmetry in the binary encoding of the negative sign codes we can get away with two bit
      // mask tests to check if a sign code is negative:
      //
      // Step 1 : Test if bit 0 and bit 3 are set
      // Step 2 : Test if there is exactly one bit set from bit 1 and bit 2

      // Step 1
      generateSIInstruction(cg, TR::InstOpCode::TM, node, generateS390LeftAlignedMemoryReference(*srcSignCodeMR, node, 0, cg, srcSignCodeMR->getLeftMostByte()), 0x90);

      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK12, node, processPositive);

      // Step 2
      generateSIInstruction(cg, TR::InstOpCode::TM, node, generateS390LeftAlignedMemoryReference(*srcSignCodeMR, node, 0, cg, srcSignCodeMR->getLeftMostByte()), 0x60);

      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK9, node, processPositive);

      // ----------------- Incoming branch -----------------

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processNegative);

      // Patch in the preferred negative sign code
      generateSIInstruction(cg, TR::InstOpCode::MVI, node, generateS390LeftAlignedMemoryReference(*destSignCodeMR, node, 0, cg, destSignCodeMR->getLeftMostByte()), TR::DataType::getZonedSeparateMinus());

      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, node, cFlowRegionEnd);

      // ----------------- Incoming branch -----------------

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processPositive);

      // Patch in the preferred positive sign code
      generateSIInstruction(cg, TR::InstOpCode::MVI, node, generateS390LeftAlignedMemoryReference(*destSignCodeMR, node, 0, cg, destSignCodeMR->getLeftMostByte()), TR::DataType::getZonedSeparatePlus());

      // ----------------- Incoming branch -----------------

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd);
      cFlowRegionEnd->setEndInternalControlFlow();

      // Clear the embedded sign code of the source
      TR::Instruction* cursor = generateSIInstruction(cg, TR::InstOpCode::OI, node, generateS390LeftAlignedMemoryReference(*srcSignCodeMR, node, 0, cg, srcSignCodeMR->getLeftMostByte()), TR::DataType::getZonedCode());

      // Set up the proper register dependencies
      TR::RegisterDependencyConditions* dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg);

      if (srcSignCodeMR->getIndexRegister())
         dependencies->addPostCondition(srcSignCodeMR->getIndexRegister(), TR::RealRegister::AssignAny);

      if (srcSignCodeMR->getBaseRegister())
         dependencies->addPostCondition(srcSignCodeMR->getBaseRegister(), TR::RealRegister::AssignAny);

      if (destSignCodeMR->getIndexRegister())
         dependencies->addPostConditionIfNotAlreadyInserted(destSignCodeMR->getIndexRegister(), TR::RealRegister::AssignAny);

      if (destSignCodeMR->getBaseRegister())
         dependencies->addPostConditionIfNotAlreadyInserted(destSignCodeMR->getBaseRegister(), TR::RealRegister::AssignAny);

      cursor->setDependencyConditions(dependencies);

      targetReg->setHasKnownPreferredSign();
      }
   }

/**
 * Handles pd2zdsls,pd2zdsts,pd2zdslsSetSign,pd2zdstsSetSign
 */
TR::Register *
J9::Z::TreeEvaluator::pd2zdslsEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   cg->traceBCDEntry("pd2zdsls",node);
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(cg->comp(), "PD-Op/%s", node->getOpCode().getName()),
                            1, TR::DebugCounter::Cheap);
   TR_PseudoRegister *targetReg = cg->allocatePseudoRegister(node->getDataType());
   TR::Node *child = node->getFirstChild();
   TR_PseudoRegister *childReg = cg->evaluateBCDNode(child);
   childReg = cg->privatizeBCDRegisterIfNeeded(node, child, childReg);
   TR::MemoryReference *sourceMR = generateS390RightAlignedMemoryReference(child, childReg->getStorageReference(), cg);
   TR::MemoryReference *destMR = packedToZonedHelper(node, targetReg, sourceMR, childReg, cg);
   zonedToZonedSeparateSignHelper(node, childReg, targetReg, sourceMR, destMR, cg);
   cg->decReferenceCount(child);
   if (node->getOpCode().isSetSign())
      cg->decReferenceCount(node->getSecondChild());
   node->setRegister(targetReg);
   cg->traceBCDExit("pd2zdsls",node);
   return targetReg;
   }

void
J9::Z::TreeEvaluator::zonedSeparateSignToPackedOrZonedHelper(TR::Node *node, TR_PseudoRegister *targetReg, TR::MemoryReference *sourceMR, TR::MemoryReference *destMR, TR::CodeGenerator * cg)
   {
   TR_ASSERT( targetReg->isInitialized(),"targetRegister must be initialized before calling zonedSeparateSignToPackedOrZonedHelper\n");
   TR::Node *srcNode = node->getFirstChild();
   TR_PseudoRegister *srcReg = cg->evaluateBCDNode(srcNode);
   bool isTruncation = srcReg->getDecimalPrecision() > node->getDecimalPrecision();
   bool isSrcTrailingSign = (srcNode->getDataType() == TR::ZonedDecimalSignTrailingSeparate);
   int32_t sourceSignEndByte = isSrcTrailingSign ? TR::DataType::getZonedSignSize() : srcReg->getSize();
   TR::Compilation *comp = cg->comp();
   if (node->getOpCode().isSetSign())
      {
      TR::Node *signCodeNode = node->getSetSignValueNode();
      TR_ASSERT( signCodeNode->getOpCode().isLoadConst(),"excepting zonedSeparateSignToPackedOrZonedHelper sign code to be a const\n");
      int32_t sign = signCodeNode->get32bitIntegralValue();
      if (sign == TR::DataType::getIgnoredSignCode())
         {
         // just check for an invalid sign but do not set anything in this case
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tzonedSeparateSignToPackedOrZonedHelper %p : op %s, ignoredSetSign=true case, sign 0x%x\n",node,node->getOpCode().getName(),sign);

         TR::LabelSymbol * returnLabel     = generateLabelSymbol(cg);
         TR::LabelSymbol * callLabel       = generateLabelSymbol(cg);

         TR::LabelSymbol * cFlowRegionStart   = generateLabelSymbol(cg);
         TR::LabelSymbol * cflowRegionEnd     = generateLabelSymbol(cg);

         TR::RegisterDependencyConditions * deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg);

         if (sourceMR->getIndexRegister())
            deps->addPostConditionIfNotAlreadyInserted(sourceMR->getIndexRegister(), TR::RealRegister::AssignAny);
         if (sourceMR->getBaseRegister())
            deps->addPostConditionIfNotAlreadyInserted(sourceMR->getBaseRegister(), TR::RealRegister::AssignAny);

         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart, deps);
         cFlowRegionStart->setStartInternalControlFlow();

         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\t\ttargetReg->isInit=%s, targetRegSize=%d, targetRegPrec=%d, srcRegSize=%d, srcRegPrec=%d, sourceSignEndByte=%d\n",
               targetReg->isInitialized()?"yes":"no",targetReg->getSize(),targetReg->getDecimalPrecision(),srcReg->getSize(),srcReg->getDecimalPrecision(),sourceSignEndByte);

         generateSIInstruction(cg, TR::InstOpCode::CLI, node, generateS390LeftAlignedMemoryReference(*sourceMR, node, 0, cg, sourceSignEndByte), TR::DataType::getZonedSeparatePlus());
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK8, node, cflowRegionEnd);


         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cflowRegionEnd, deps);
         cflowRegionEnd->setEndInternalControlFlow();

         targetReg->transferSignState(srcReg, isTruncation);
         }
      else
         {
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tzonedSeparateSignToPackedOrZonedHelper %p : op %s, setSign=true case, sign 0x%x\n",node,node->getOpCode().getName(),sign);
         cg->genSignCodeSetting(node, targetReg, targetReg->getSize(), generateS390RightAlignedMemoryReference(*destMR, node, 0, cg), sign, targetReg, 0, false /* !numericNibbleIsZero */);
         }
      }
   else
      {
      TR::LabelSymbol * checkMinusLabel = generateLabelSymbol(cg);
      TR::LabelSymbol * returnLabel       = generateLabelSymbol(cg);
      TR::LabelSymbol * callLabel       = generateLabelSymbol(cg);

      TR::LabelSymbol * cFlowRegionStart   = generateLabelSymbol(cg);
      TR::LabelSymbol * cflowRegionEnd     = generateLabelSymbol(cg);

      TR::RegisterDependencyConditions * deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg);

      if (sourceMR->getIndexRegister())
         deps->addPostConditionIfNotAlreadyInserted(sourceMR->getIndexRegister(), TR::RealRegister::AssignAny);
      if (sourceMR->getBaseRegister())
         deps->addPostConditionIfNotAlreadyInserted(sourceMR->getBaseRegister(), TR::RealRegister::AssignAny);

      if (destMR->getIndexRegister())
         deps->addPostConditionIfNotAlreadyInserted(destMR->getIndexRegister(), TR::RealRegister::AssignAny);
      if (destMR->getBaseRegister())
         deps->addPostConditionIfNotAlreadyInserted(destMR->getBaseRegister(), TR::RealRegister::AssignAny);


      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart, deps);
      cFlowRegionStart->setStartInternalControlFlow();

      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tzonedSeparateSignToPackedOrZonedHelper %p : op %s, targetReg->isInit=%s, targetRegSize=%d, targetRegPrec=%d, srcRegSize=%d, srcRegPrec=%d, sourceSignEndByte=%d\n",
            node,node->getOpCode().getName(),targetReg->isInitialized()?"yes":"no",targetReg->getSize(),targetReg->getDecimalPrecision(),srcReg->getSize(),srcReg->getDecimalPrecision(),sourceSignEndByte);

        // DAA library assumes all BCD types are positive, unless an explicit negative sign code is present
      generateSIInstruction(cg, TR::InstOpCode::CLI, node, generateS390LeftAlignedMemoryReference(*sourceMR, node, 0, cg, sourceSignEndByte), TR::DataType::getZonedSeparateMinus());
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC0, node, checkMinusLabel);

      cg->genSignCodeSetting(node, NULL, targetReg->getSize(), generateS390RightAlignedMemoryReference(*destMR, node, 0, cg), TR::DataType::getPreferredPlusCode(), targetReg, 0, false /* !numericNibbleIsZero */);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cflowRegionEnd);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, checkMinusLabel);


      cg->genSignCodeSetting(node, NULL, targetReg->getSize(), generateS390RightAlignedMemoryReference(*destMR, node, 0, cg), TR::DataType::getPreferredMinusCode(), targetReg, 0, false /* !numericNibbleIsZero */);



      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cflowRegionEnd, deps);
      cflowRegionEnd->setEndInternalControlFlow();

      targetReg->setHasKnownPreferredSign();
      }
   }

/**
 * Handles zdsls2pd,zdsts2pd
 */
TR::Register *
J9::Z::TreeEvaluator::zdsls2pdEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   cg->traceBCDEntry("zdsls2pd",node);
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(cg->comp(), "PD-Op/%s", node->getOpCode().getName()),
                            1, TR::DebugCounter::Cheap);
   TR_PseudoRegister *targetReg = cg->allocatePseudoRegister(node->getDataType());
   TR::Node *child = node->getFirstChild();
   TR_PseudoRegister *childReg = cg->evaluateBCDNode(child);
   childReg = cg->privatizeBCDRegisterIfNeeded(node, child, childReg);
   TR::MemoryReference *sourceMR = generateS390RightAlignedMemoryReference(child, childReg->getStorageReference(), cg);
   TR::MemoryReference *destMR = zonedToPackedHelper(node, targetReg, sourceMR, childReg, cg);
   targetReg->resetSignState();  // the conversion operation is not complete yet so reset any sign state transferred in the zonedToPackedHelper
   // zonedToPackedHelper with a separate sign source will pack a zone code into the packed sign code position so set the zone value on the
   // targetReg to improve the zonedSeparateSignToPackedOrZonedHelper code generation
   targetReg->setTemporaryKnownSignCode(TR::DataType::getZonedValue());
   zonedSeparateSignToPackedOrZonedHelper(node, targetReg, sourceMR, destMR, cg);
   cg->decReferenceCount(child);
   if (node->getOpCode().isSetSign())
      cg->decReferenceCount(node->getSecondChild());
   node->setRegister(targetReg);
   cg->traceBCDExit("zdsls2pd",node);
   return targetReg;
   }

/**
 * Handles zdsls2zd,zdsts2zd
 */
TR::Register *
J9::Z::TreeEvaluator::zdsls2zdEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   cg->traceBCDEntry("zdsls2zd",node);
   TR::Node *srcNode = node->getFirstChild();
   TR_PseudoRegister *srcReg = cg->evaluateBCDNode(srcNode);

   bool isSetSign = node->getOpCode().isSetSign();
   int32_t sign = 0;
   TR::Node *signCodeNode = NULL;
   TR::Compilation *comp = cg->comp();
   if (isSetSign)
      {
      signCodeNode = node->getSecondChild();
      TR_ASSERT( signCodeNode->getOpCode().isLoadConst(),"excepting zdsle2zdSetSign sign code to be a const\n");
      sign = signCodeNode->get32bitIntegralValue();
      }

   bool isSrcTrailingSign = (srcNode->getDataType() == TR::ZonedDecimalSignTrailingSeparate);
   int32_t sourceOffset = 0;
   bool isTruncation = false;
   int32_t targetPrecision = srcReg->getDecimalPrecision();
   if (srcReg->getDecimalPrecision() > node->getDecimalPrecision())  // a truncation
      {
      isTruncation = true;
      sourceOffset = srcReg->getDecimalPrecision() - node->getDecimalPrecision();   // reach into the source by sourceOffset bytes to get the correct digits
      targetPrecision = node->getDecimalPrecision();
      }

   bool isEffectiveNop = isZonedOperationAnEffectiveNop(node, 0, isTruncation, srcReg, isSetSign, sign, cg);
   TR_PseudoRegister *targetReg = NULL;
   TR::MemoryReference *sourceMR = NULL;
   TR::MemoryReference *destMR = NULL;
   if (isEffectiveNop)
      {
      targetReg = evaluateBCDSignModifyingOperand(node, isEffectiveNop, false, false, sourceMR, cg); // isNondestructiveNop=false,initTarget=false
      }
   else
      {
      targetReg = evaluateBCDValueModifyingOperand(node, false, sourceMR, cg); // initTarget=false
      sourceMR = generateS390RightAlignedMemoryReference(srcNode, srcReg->getStorageReference(), cg);
      destMR = generateS390RightAlignedMemoryReference(node, targetReg->getStorageReference(), cg);
      }

   targetReg->setDecimalPrecision(targetPrecision);
   bool isInitialized = targetReg->isInitialized();
   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tzdsls2zdEvaluator %p : op %s, isInitialized=%s, targetRegSize=%d, targetRegPrec=%d, srcRegSize=%d, srcRegPrec=%d, isEffectiveNop=%s (isSetSign %s, sign 0x%x)\n",
         node,node->getOpCode().getName(),isInitialized?"yes":"no",
            targetReg->getSize(),targetReg->getDecimalPrecision(),srcReg->getSize(),srcReg->getDecimalPrecision(),isEffectiveNop?"yes":"no",isSetSign?"yes":"no",sign);

   if (!isEffectiveNop)
      {
      if (!isInitialized)
         {
         int32_t mvcSize = targetReg->getDecimalPrecision();
         int32_t srcEndByte = isSrcTrailingSign ? srcReg->getSize() : srcReg->getSize() - TR::DataType::getZonedSignSize();
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tisInit=false so gen MVC to init with size=%d and sourceOffset=%d, srcEndByte=%d\n",mvcSize,sourceOffset,srcEndByte);
         generateSS1Instruction(cg, TR::InstOpCode::MVC, node,
                                mvcSize-1,
                                generateS390RightAlignedMemoryReference(*destMR, node, 0, cg),
                                generateS390LeftAlignedMemoryReference(*sourceMR, node, sourceOffset, cg, srcEndByte));
         targetReg->transferDataState(srcReg);
         targetReg->setIsInitialized();
         }
      targetReg->setTemporaryKnownSignCode(TR::DataType::getZonedValue());
      if (isInitialized && isSrcTrailingSign)
         {
         destMR->addToTemporaryNegativeOffset(node, -TR::DataType::getZonedSignSize(), cg);
         }
      zonedSeparateSignToPackedOrZonedHelper(node, targetReg, sourceMR, destMR, cg);
      }

   if (isSrcTrailingSign)
      {
      if (isEffectiveNop)
         {
         targetReg->addToRightAlignedIgnoredBytes(TR::DataType::getZonedSignSize());
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tisSrcTrailingSign=true and isEffectiveNop=true (zdsls2zd) : increment targetReg %s ignoredBytes %d -> %d (by the TR::DataType::getZonedSignSize())\n",
               cg->getDebug()->getName(targetReg),targetReg->getRightAlignedIgnoredBytes() - TR::DataType::getZonedSignSize(),targetReg->getRightAlignedIgnoredBytes());
         }
      else if (isInitialized)
         {
         targetReg->addToRightAlignedDeadBytes(TR::DataType::getZonedSignSize());
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tisSrcTrailingSign=true and isInitialized=true (zdsls2zd) : increment targetReg %s deadBytes %d -> %d (by the TR::DataType::getZonedSignSize())\n",
               cg->getDebug()->getName(targetReg),targetReg->getRightAlignedDeadBytes() - TR::DataType::getZonedSignSize(),targetReg->getRightAlignedDeadBytes());
         }
      }

   cg->decReferenceCount(srcNode);
   if (node->getOpCode().isSetSign())
      cg->decReferenceCount(node->getSecondChild());
   node->setRegister(targetReg);
   cg->traceBCDExit("zdsls2zd",node);
   return targetReg;
   }

/**
 * Handles zd2zdsls,zd2zdsts
 */
TR::Register *
J9::Z::TreeEvaluator::zd2zdslsEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   cg->traceBCDEntry("zd2zdsls",node);
   TR::Compilation *comp = cg->comp();
   TR::Node *srcNode = node->getFirstChild();
   TR_PseudoRegister *srcReg = cg->evaluateBCDNode(srcNode);

   TR_StorageReference *srcStorageReference = srcReg->getStorageReference();
   TR::MemoryReference *sourceMR = generateS390RightAlignedMemoryReference(srcNode, srcStorageReference, cg);

   TR_PseudoRegister *targetReg = evaluateBCDValueModifyingOperand(node, false, sourceMR, cg); // initTarget=false
   TR::MemoryReference *destMR = generateS390RightAlignedMemoryReference(node, targetReg->getStorageReference(), cg);

   bool isTrailingSign = (node->getDataType() == TR::ZonedDecimalSignTrailingSeparate);

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tzd2zdslsEvaluator %p : op %s, targetReg->isInit=%s, targetRegSize=%d, targetRegPrec=%d\n",
         node,node->getOpCode().getName(),targetReg->isInitialized()?"yes":"no",targetReg->getSize(),targetReg->getDecimalPrecision());

   bool isTruncation = node->getDecimalPrecision() < srcReg->getDecimalPrecision();
   TR_ASSERT( !isTruncation,"a zd2zdsxs operation should not truncate\n");

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tset targetReg->prec to srcReg->prec %d\n",srcReg->getDecimalPrecision());
   targetReg->setDecimalPrecision(srcReg->getDecimalPrecision());

   // the (targetReg->isInitialized() && isTrailingSign) case below is needed to move the initialized data left by 1 byte to make room for the trailing separate sign code
   if (!targetReg->isInitialized() || (targetReg->isInitialized() && isTrailingSign))
      {
      int32_t mvcSize = srcReg->getSize();
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\t%s so gen MVC to init with size %d\n",!targetReg->isInitialized()?"isInit=false":"isInit=true and isTrailingSign=true", mvcSize);
      generateSS1Instruction(cg, TR::InstOpCode::MVC, node,
                             mvcSize-1,
                             generateS390LeftAlignedMemoryReference(*destMR, node, 0, cg, isTrailingSign ? srcReg->getSize() + TR::DataType::getZonedSignSize() : srcReg->getSize()),
                             generateS390RightAlignedMemoryReference(*sourceMR, node, 0, cg));
      targetReg->setIsInitialized();
      }

   zonedToZonedSeparateSignHelper(node, srcReg, targetReg, sourceMR, destMR, cg);

   cg->decReferenceCount(srcNode);
   if (node->getOpCode().isSetSign())
      cg->decReferenceCount(node->getSecondChild());
   node->setRegister(targetReg);
   cg->traceBCDExit("zd2zdsls",node);
   return targetReg;
   }

/**
 * Handles zdsle2zd,zd2zdsle
 */
TR::Register *
J9::Z::TreeEvaluator::zdsle2zdEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   cg->traceBCDEntry("zdsle2zd",node);
   TR::Node *srcNode = node->getFirstChild();
   TR_PseudoRegister *srcReg = cg->evaluateBCDNode(srcNode);

   bool isSetSign = node->getOpCode().isSetSign();
   int32_t sign = 0;
   TR::Node *signCodeNode = NULL;
   TR::Compilation *comp = cg->comp();
   if (isSetSign)
      {
      signCodeNode = node->getSecondChild();
      TR_ASSERT(signCodeNode->getOpCode().isLoadConst(),"excepting zdsle2zdSetSign sign code to be a const\n");
      sign = signCodeNode->get32bitIntegralValue();
      }
   bool isTrailingDst = node->getDataType() == TR::ZonedDecimal;
   bool isLeadingDst = !isTrailingDst;
   bool isTrailingSrc = srcNode->getDataType() == TR::ZonedDecimal;
   bool isLeadingSrc = !isTrailingSrc;

   bool isTruncation = false;
   int32_t digitsToClear = 0;
   if (node->getDecimalPrecision() < srcReg->getDecimalPrecision())
      isTruncation = true;
   else if (node->getDecimalPrecision() > srcReg->getDecimalPrecision())
      digitsToClear = node->getDecimalPrecision()-srcReg->getDecimalPrecision();

   bool isEffectiveNop = isZonedOperationAnEffectiveNop(node, 0, isTruncation, srcReg, isSetSign, sign, cg);
   bool isNondestructiveNop = isEffectiveNop && !isTruncation;
   bool doWidening = true;

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tzdsle2zdEvaluator %p : op %s, isEffectiveNop=%s, isTruncation=%s, srcSignIsZone=%s, srcReg->getSize()=%d, (isSetSign=%s, sign 0x%x)\n",
         node,node->getOpCode().getName(),isEffectiveNop?"yes":"no",isTruncation?"yes":"no",srcReg->knownOrAssumedSignIsZone()?"yes":"no",srcReg->getSize(),isSetSign?"yes":"no",sign);

   TR::MemoryReference *sourceMR = NULL;
   TR_PseudoRegister *targetReg = NULL;
   if (!isEffectiveNop &&
       isLeadingDst &&     // only do for leading sign so the sign code doesn't have to be moved again later
       doWidening &&
       digitsToClear > 0)
      {
      sourceMR = generateS390RightAlignedMemoryReference(srcNode, srcReg->getStorageReference(), cg);
      targetReg = evaluateBCDValueModifyingOperand(node, true, sourceMR, cg); // initTarget=true
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tperform an explicit widening (digitsToClear=%d, doWidening=yes, isEffectiveNop=no) set targetReg->prec to node->prec %d\n",digitsToClear,node->getDecimalPrecision());
      targetReg->setDecimalPrecision(node->getDecimalPrecision());
      }
   else
      {
      if (!isEffectiveNop)
         sourceMR = generateS390RightAlignedMemoryReference(srcNode, srcReg->getStorageReference(), cg);
      targetReg = evaluateBCDSignModifyingOperand(node, isEffectiveNop, isNondestructiveNop, true /*initTarget*/, sourceMR, cg);
      int32_t targetPrecision = isTruncation ? node->getDecimalPrecision() : srcReg->getDecimalPrecision();
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tdo not perform an explicit widening (set digitsToClear=%d->0, doWidening=%s, isEffectiveNop=%s) set targetReg->prec to %d\n",
            digitsToClear,doWidening?"yes":"no",isEffectiveNop ?"yes":"no",targetPrecision);
      digitsToClear = 0;
      targetReg->setDecimalPrecision(targetPrecision);
      }

   if (!isEffectiveNop)
      {
      TR::MemoryReference *destMR = isTrailingDst ? generateS390RightAlignedMemoryReference(node, targetReg->getStorageReference(), cg) :
                                                       generateS390LeftAlignedMemoryReference(node, targetReg->getStorageReference(), cg, targetReg->getSize());
      int32_t clearLeftMostByte = targetReg->getSize();
      if (isSetSign)
         {
         if (sign == TR::DataType::getIgnoredSignCode())
            {
            if (cg->traceBCDCodeGen()) traceMsg(comp,"\tisSetSign=true with ignored sign=0x%x\n",sign);
            if (isTrailingDst) // zdsle2zd
               {
               if (srcReg->getSize() == 1)
                  targetReg->transferSignState(srcReg, isTruncation);
               else
                  targetReg->setKnownSignCode(TR::DataType::getZonedValue());
               }
            else // zd2zdsle
               {
               if (targetReg->getSize() == 1)
                  targetReg->transferSignState(srcReg, isTruncation);
               else if (targetReg->getSize() > srcReg->getSize())  // a widening in the leadingDst and ignored case leaves a bad sign code
                  targetReg->setHasKnownBadSignCode();
               else
                  targetReg->setKnownSignCode(TR::DataType::getZonedValue());
               }
            }
         else
            {
            if (cg->traceBCDCodeGen()) traceMsg(comp,"\tisSetSign=true : call genSignCodeSetting with sign=0x%x\n",sign);
            bool numericNibbleIsZero = false;
            if (isTrailingDst)  // zdsle2zd
               {
               // bytes above the leftmost one have a top nibble of 0xf so use this knowledge to improve the sign code setting
               if (srcReg->getSize() == 1)
                  targetReg->transferSignState(srcReg, isTruncation);
               else
                  targetReg->setTemporaryKnownSignCode(TR::DataType::getZonedValue());
               }
            else // zd2zdsle
               {
               // when not performing an explicit widening then the bytes above the first one have a top nibble of 0xf so use this knowledge to improve the sign code setting
               if (targetReg->getSize() == 1)
                  targetReg->transferSignState(srcReg, isTruncation);
               else if (targetReg->getSize() <= srcReg->getSize())
                  targetReg->setTemporaryKnownSignCode(TR::DataType::getZonedValue());

               if (digitsToClear > 0)
                  {
                  numericNibbleIsZero = true;
                  digitsToClear--;
                  clearLeftMostByte--;
                  }
               }
            int32_t digitsCleared = cg->genSignCodeSetting(node, targetReg, targetReg->getSize(), destMR, sign, targetReg, 0, numericNibbleIsZero);
            TR_ASSERT(!numericNibbleIsZero || digitsCleared == 1,"the sign code setting should have also cleared 1 digit (digitsCleared = %d)\n",digitsCleared);
            }
         }

      if (digitsToClear > 0)
         {
         cg->genZeroLeftMostZonedBytes(node, targetReg, clearLeftMostByte, digitsToClear, destMR);
         }

      if (!isSetSign)
         {
         if (cg->traceBCDCodeGen()) traceMsg(comp,"\tisSetSign=false : generate MVZ of size 1 to transfer left aligned zdsle sign to right aligned zd sign position\n");

         sourceMR = isTrailingSrc ? reuseS390RightAlignedMemoryReference(sourceMR, srcNode, srcReg->getStorageReference(), cg) :
                                    reuseS390LeftAlignedMemoryReference(sourceMR, srcNode, srcReg->getStorageReference(), cg, srcReg->getSize());
         destMR = isTrailingDst ? reuseS390RightAlignedMemoryReference(destMR, node, targetReg->getStorageReference(), cg) :
                                  reuseS390LeftAlignedMemoryReference(destMR, node, targetReg->getStorageReference(), cg, targetReg->getSize());
         int32_t mvzSize = 1;
         generateSS1Instruction(cg, TR::InstOpCode::MVZ, node,
                                mvzSize-1,
                                destMR,
                                sourceMR);
         targetReg->transferSignState(srcReg, isTruncation);
         }

      bool srcSignWillBeIgnored = false;
      bool srcSignResetRedundant = srcReg->knownOrAssumedSignIsZone() || (isLeadingSrc && isTruncation);
      bool srcSignResetIllegal = targetReg->getSize() == 1;

      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tcheck before resetting srcSignCode: srcSignWillBeIgnored %s, srcSignResetRedundant %s, srcSignResetIllegal %s\n",
            srcSignWillBeIgnored?"yes":"no",srcSignResetRedundant?"yes":"no",srcSignResetIllegal?"yes":"no");
      if (!(srcSignWillBeIgnored || srcSignResetRedundant || srcSignResetIllegal))
         {
            {
            if (cg->traceBCDCodeGen()) traceMsg(comp,"\tgenerate OI 0xF0 to force %s-aligned high nibble to 0xF\n",isTrailingSrc?"right":"left");
            generateSIInstruction(cg, TR::InstOpCode::OI, node, generateS390LeftAlignedMemoryReference(*destMR, node, 0, cg, isTrailingSrc ? 1 : targetReg->getSize()), TR::DataType::getZonedCode());
            }
         }
      targetReg->setIsInitialized();
      }

   cg->decReferenceCount(srcNode);
   if (isSetSign)
      cg->decReferenceCount(signCodeNode);
   node->setRegister(targetReg);
   cg->traceBCDExit("zdsle2zd",node);
   return targetReg;
   }

TR::MemoryReference *
J9::Z::TreeEvaluator::zonedToPackedHelper(TR::Node *node, TR_PseudoRegister *targetReg, TR::MemoryReference *sourceMR, TR_PseudoRegister *childReg, TR::CodeGenerator * cg)
   {
   TR::Node *child = node->getFirstChild();
   TR_StorageReference *hint = node->getStorageReferenceHint();
   TR_StorageReference *targetStorageReference = NULL;
   int32_t destPrecision = 0;
   int32_t destSize = 0;
   TR::Compilation *comp = cg->comp();
   if (hint)
      {
      TR_ASSERT( !childReg->isInitialized() || hint != childReg->getStorageReference(),"bcd conversion operands will overlap\n");
      destSize = hint->getSymbolSize(); // may be be larger than the node->getSize() so take this opportunity to widen as part of the PACK
      destPrecision = TR::DataType::getBCDPrecisionFromSize(node->getDataType(), destSize); // may be be larger than the node->getSize() so take this opportunity to widen as part of the PACK
      targetStorageReference = hint;
      }
   else
      {
      destSize = node->getSize();
      destPrecision = node->getDecimalPrecision();
      targetStorageReference = TR_StorageReference::createTemporaryBasedStorageReference(destSize, comp);
      }

   targetReg->setStorageReference(targetStorageReference, node);

   int32_t sourcePrecision = childReg->getDecimalPrecision();
   bool isTruncation = false;
   int32_t sourceOffsetForLeftAlignment = 0;

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tzonedToPackedHelper %p : op %s, destPrecision %d, destSize %d, sourcePrecision %d, sourceSize %d\n",
         node,node->getOpCode().getName(),destPrecision,destSize,sourcePrecision,childReg->getSize());

   if (node->getDecimalPrecision() < sourcePrecision)
      {
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tnodePrec <= sourcePrecision (%d <= %d) so set sourcePrecision=nodePrec=%d,isTruncation=true,sourceOffsetForLeftAlignment=%d\n",
                     node->getDecimalPrecision(),sourcePrecision,node->getDecimalPrecision(),sourcePrecision - node->getDecimalPrecision());
      sourceOffsetForLeftAlignment = sourcePrecision - node->getDecimalPrecision();
      sourcePrecision = node->getDecimalPrecision();
      isTruncation = true;
      }

   TR::MemoryReference *destMR = NULL;
   if (destSize > 16)
      {
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tdestSize %d > 16 so reduce destSize to 16 and destPrecision to 31 for PACK encoding and clear top %d byte(s)\n",destSize,(destSize-16));
      destMR = generateS390RightAlignedMemoryReference(node, targetStorageReference, cg);
      cg->genZeroLeftMostPackedDigits(node, targetReg, destSize, (destSize-16)*2, destMR);
      destSize = 16;
      destPrecision = 31;
      }

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tsetting targetReg->prec to sourcePrecision %d\n",sourcePrecision);
   targetReg->setDecimalPrecision(sourcePrecision);

   // skip over trailing sign for the unpack
   bool isSrcTrailingSign = (child->getDataType() == TR::ZonedDecimalSignTrailingSeparate);
   int32_t sourceEndByte = isSrcTrailingSign ? sourcePrecision + TR::DataType::getZonedSignSize() :
                                               sourcePrecision;

   if (sourcePrecision <= 16)
      {
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tsourcePrecision %d <= 16 so generate a single PACK destSize %d, sourcePrecision %d, sourceEndByte %d\n",sourcePrecision,destSize,sourcePrecision,sourceEndByte);
      destMR = reuseS390RightAlignedMemoryReference(destMR, node, targetStorageReference, cg);
      generateSS2Instruction(cg, TR::InstOpCode::PACK, node,
                             destSize-1,
                             destMR,
                             sourcePrecision-1,
                             generateS390LeftAlignedMemoryReference(*sourceMR, node, 0, cg, sourceEndByte));
      int32_t destSizeAsCeilingPrecision = TR::DataType::byteLengthToPackedDecimalPrecisionCeiling(destSize);
      if (destSizeAsCeilingPrecision > sourcePrecision)
         targetReg->addRangeOfZeroDigits(sourcePrecision, destSizeAsCeilingPrecision);
      }
   else if (sourcePrecision >= 17 && sourcePrecision <= 31)
      {
      if (cg->traceBCDCodeGen())
         {
         if (sourcePrecision >= 17 && sourcePrecision <= 30)
            traceMsg(comp,"\tsourcePrecision 17 <= %d <= 30 so generate two PACKs with sourceEndByte %d\n",sourcePrecision,sourceEndByte);
         else
            traceMsg(comp,"\tsourcePrecision == 31  so generate three PACKs with sourceEndByte %d\n",sourceEndByte);
         }
      bool needsThirdPack = false;
      if (sourcePrecision == 31)
         {
         sourcePrecision = 29;   // The first two PACKs for the sourcePrecision=31 case are the same as for the sourcePrecision=29 case
         destPrecision = 29;
         needsThirdPack = true;
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tsourcePrecision == 31 so reduce sourcePrecision and destPrecision to 29 and update sourceEndByte to %d\n",sourceEndByte);
         }

      if (cg->traceBCDCodeGen())
         traceMsg(comp,"x^x : found large packed/zoned conv -- node %s (%p) prec %d, child %s (%p) prec %d (three=%s)\n",
            node->getOpCode().getName(),node,destPrecision,
            child->getOpCode().getName(),child,sourcePrecision,needsThirdPack?"yes":"no");

      destMR = reuseS390LeftAlignedMemoryReference(destMR, node, targetStorageReference, cg, destSize);
      sourceMR = generateS390LeftAlignedMemoryReference(*sourceMR, node, 0, cg, sourceEndByte);
      int32_t pack1SourceSize = sourcePrecision-14;
      int32_t pack1DestSize = TR::DataType::getSizeFromBCDPrecision(node->getDataType(), destPrecision-14);
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\t\t1st PACK destSize=%d,srcSize=%d\n",pack1DestSize,pack1SourceSize);
      generateSS2Instruction(cg, TR::InstOpCode::PACK, node,
                             pack1DestSize-1,
                             destMR,
                             pack1SourceSize-1,
                             sourceMR);
      int32_t pack1DestSizeAsPrecision = TR::DataType::byteLengthToPackedDecimalPrecisionCeiling(pack1DestSize);
      if (pack1DestSizeAsPrecision > pack1SourceSize)
         {
         int32_t rightMostDigits = (destSize-pack1DestSize)*2;
         targetReg->addRangeOfZeroDigits(pack1SourceSize+rightMostDigits, pack1DestSizeAsPrecision+rightMostDigits);
         }
      int32_t pack2SourceSize = 15;
      int32_t pack2SourceOffset = pack1SourceSize-1;
      int32_t pack2DestSize = TR::DataType::getSizeFromBCDPrecision(node->getDataType(), pack2SourceSize);
      int32_t pack2DestOffset = pack1DestSize-1;
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\t\t2nd PACK destSize=%d,destOffset=%d, srcSize=%d,srcOffset=%d\n",pack2DestSize,pack2DestOffset,pack2SourceSize,pack2SourceOffset);
      generateSS2Instruction(cg, TR::InstOpCode::PACK, node,
                             pack2DestSize-1,
                             generateS390LeftAlignedMemoryReference(*destMR, node, pack2DestOffset, cg, destMR->getLeftMostByte()),
                             pack2SourceSize-1,
                             generateS390LeftAlignedMemoryReference(*sourceMR, node, pack2SourceOffset, cg, sourceMR->getLeftMostByte()));
      if (needsThirdPack)
         {
         int32_t pack3SourceSize = 3;
         int32_t pack3SourceOffset = pack2SourceOffset+(pack2SourceSize-1);
         int32_t pack3DestSize = TR::DataType::getSizeFromBCDPrecision(node->getDataType(), pack3SourceSize);
         int32_t pack3DestOffset = pack2DestOffset+(pack2DestSize-1);
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\t\t3rd PACK destSize=%d,destOffset=%d, srcSize=%d,srcOffset=%d\n",pack3DestSize,pack3DestOffset,pack3SourceSize,pack3SourceOffset);
         generateSS2Instruction(cg, TR::InstOpCode::PACK, node,
                                pack3DestSize-1,
                                generateS390LeftAlignedMemoryReference(*destMR, node, pack3DestOffset, cg, destMR->getLeftMostByte()),
                                pack3SourceSize-1,
                                generateS390LeftAlignedMemoryReference(*sourceMR, node, pack3SourceOffset, cg, sourceMR->getLeftMostByte()));
         }
      }
   else
      {
      TR_ASSERT(false,"zd2pd unexpected sourcePrecision %d\n",sourcePrecision);
      }

   TR::Register* signCode     = cg->allocateRegister();
   TR::Register* signCode4Bit = cg->allocateRegister();

   TR::LabelSymbol * processSign     = generateLabelSymbol(cg);
   TR::LabelSymbol * processSignEnd  = generateLabelSymbol(cg);
   TR::LabelSymbol * processNegative = generateLabelSymbol(cg);
   TR::LabelSymbol * cFlowRegionEnd  = generateLabelSymbol(cg);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processSign);
   processSign->setStartInternalControlFlow();

   // Load the sign byte of the Packed Decimal from memory
   generateRXInstruction(cg, TR::InstOpCode::LLC, node, signCode, generateS390LeftAlignedMemoryReference(*destMR, node, 0, cg, 1));

   generateRRInstruction(cg, TR::InstOpCode::LR, node, signCode4Bit, signCode);

   // Clear most significant 4 bits
   generateRIInstruction(cg, TR::InstOpCode::NILL, node, signCode4Bit, 0x000F);

   // Compare the sign byte against the preferred negative sign code
   generateRIInstruction(cg, TR::InstOpCode::CHI, node, signCode4Bit, TR::DataType::getPreferredMinusCode());

   // Branch if equal
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK8, node, cFlowRegionEnd);

   // Clear least significant 4 bits
   generateRIInstruction(cg, TR::InstOpCode::NILL, node, signCode, 0x00F0);

   // Compare the sign byte against the alternative negative sign code
   generateRIInstruction(cg, TR::InstOpCode::CHI, node, signCode4Bit, TR::DataType::getAlternateMinusCode());

   // Branch if equal
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK8, node, processNegative);

   // Patch in the preferred positive sign code
   generateRIInstruction(cg, TR::InstOpCode::OILL, node, signCode, TR::DataType::getPreferredPlusCode());

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, node, processSignEnd);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processNegative);

   // Patch in the preferred negative sign code
   generateRIInstruction(cg, TR::InstOpCode::OILL, node, signCode, TR::DataType::getPreferredMinusCode());

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processSignEnd);

   generateRXInstruction(cg, TR::InstOpCode::STC, node, signCode, generateS390LeftAlignedMemoryReference(*destMR, node, 0, cg, 1));

   // Set up the proper register dependencies
   TR::RegisterDependencyConditions* dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg);

   dependencies->addPostCondition(signCode,     TR::RealRegister::AssignAny);
   dependencies->addPostCondition(signCode4Bit, TR::RealRegister::AssignAny);

   if (destMR->getIndexRegister())
      dependencies->addPostCondition(destMR->getIndexRegister(), TR::RealRegister::AssignAny);

   if (destMR->getBaseRegister())
      dependencies->addPostCondition(destMR->getBaseRegister(), TR::RealRegister::AssignAny);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, dependencies);
   cFlowRegionEnd->setEndInternalControlFlow();

   // Cleanup registers before returning
   cg->stopUsingRegister(signCode);
   cg->stopUsingRegister(signCode4Bit);

   targetReg->transferSignState(childReg, isTruncation);
   targetReg->transferDataState(childReg);
   targetReg->setIsInitialized();
   node->setRegister(targetReg);
   return destMR;
   }

TR::Register *
J9::Z::TreeEvaluator::zd2pdEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   cg->traceBCDEntry("zd2pd",node);
   TR::Register* targetReg = NULL;

   static char* isVectorBCDEnv = feGetEnv("TR_enableVectorBCD");
   if(TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() && !cg->comp()->getOption(TR_DisableVectorBCD) || isVectorBCDEnv)
      {
      targetReg = zd2pdVectorEvaluatorHelper(node, cg);
      }
   else
      {
      targetReg = cg->allocatePseudoRegister(node->getDataType());
      TR::Node *child = node->getFirstChild();
      TR_PseudoRegister *childReg = cg->evaluateBCDNode(child);
      childReg = cg->privatizeBCDRegisterIfNeeded(node, child, childReg);
      TR::MemoryReference *sourceMR = generateS390RightAlignedMemoryReference(child, childReg->getStorageReference(), cg);
      zonedToPackedHelper(node, static_cast<TR_PseudoRegister*>(targetReg), sourceMR, childReg, cg);
      cg->decReferenceCount(child);
      node->setRegister(targetReg);
      }

   cg->traceBCDExit("zd2pd",node);
   return targetReg;
   }

/**
 * 1. Get zd value by evaluating child node. It's in zdNode's PseudoRegister
 * 2. Get the memory reference from the pseudo register.
 * 3. Allocate Vector register to return
 * 4. get size of the node( node->getsize)
 * 5. generateVSI instruction using the information above.
 * 6. attach Vector register to the node.
 * 7. decReference BCD node for the child/
 * 8. return targetRegister.
*/
TR::Register *
J9::Z::TreeEvaluator::zd2pdVectorEvaluatorHelper(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register *targetReg = NULL;

   TR::Node *child = node->getFirstChild();
   TR_PseudoRegister *sourceReg = cg->evaluateBCDNode(child);
   sourceReg = cg->privatizeBCDRegisterIfNeeded(node, child, sourceReg);
   TR::MemoryReference *sourceMR = generateS390LeftAlignedMemoryReference(child, sourceReg->getStorageReference(), cg, child->getDecimalPrecision());
   targetReg = cg->allocateRegister(TR_VRF);
   int32_t destPrecision = std::min(node->getDecimalPrecision(), child->getDecimalPrecision());
   generateVSIInstruction(cg, TR::InstOpCode::VPKZ, node, targetReg, sourceMR, destPrecision - 1);

   node->setRegister(targetReg);
   cg->decReferenceCount(child);
   return targetReg;
   }

/**
 * \brief Check the sign of zd after pd2zd conversion.
 *
 * The UNPK instruction does not validate the digits nor the sign of the packed decimal.
 * We need to check the sign of PD and set ZD signs properly: use 0xc for positive, and 0xd for negative numbers.
 *
*/
void
J9::Z::TreeEvaluator::pd2zdSignFixup(TR::Node *node,
                                             TR::MemoryReference *destMR,
                                             TR::CodeGenerator * cg)
   {
   TR::Register* signCode     = cg->allocateRegister();
   TR::Register* signCode4Bit = cg->allocateRegister();

   TR::LabelSymbol * processSign     = generateLabelSymbol(cg);
   TR::LabelSymbol * processSignEnd  = generateLabelSymbol(cg);
   TR::LabelSymbol * processNegative = generateLabelSymbol(cg);
   TR::LabelSymbol * cFlowRegionEnd  = generateLabelSymbol(cg);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processSign);
   processSign->setStartInternalControlFlow();

   // Load the sign byte of the Zoned Decimal from memory
   generateRXInstruction(cg, TR::InstOpCode::LLC, node, signCode, generateS390LeftAlignedMemoryReference(*destMR, node, 0, cg, 1));

   generateRRInstruction(cg, TR::InstOpCode::LR, node, signCode4Bit, signCode);

   // Clear least significant 4 bits
   generateRIInstruction(cg, TR::InstOpCode::NILL, node, signCode4Bit, 0x00F0);

   // Compare the sign byte against the preferred negative sign code
   generateRIInstruction(cg, TR::InstOpCode::CHI, node, signCode4Bit, TR::DataType::getPreferredMinusCode() << 4);

   // Branch if equal
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK8, node, cFlowRegionEnd);

   // Clear most significant 4 bits
   generateRIInstruction(cg, TR::InstOpCode::NILL, node, signCode, 0x000F);

   // Compare the sign byte against the alternative negative sign code
   generateRIInstruction(cg, TR::InstOpCode::CHI, node, signCode4Bit, TR::DataType::getAlternateMinusCode() << 4);

   // Branch if equal
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK8, node, processNegative);

   // Patch in the preferred positive sign code
   generateRIInstruction(cg, TR::InstOpCode::OILL, node, signCode, TR::DataType::getPreferredPlusCode() << 4);

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, node, processSignEnd);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processNegative);

   // Patch in the preferred negative sign code
   generateRIInstruction(cg, TR::InstOpCode::OILL, node, signCode, TR::DataType::getPreferredMinusCode() << 4);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processSignEnd);

   generateRXInstruction(cg, TR::InstOpCode::STC, node, signCode, generateS390LeftAlignedMemoryReference(*destMR, node, 0, cg, 1));

   // Set up the proper register dependencies
   TR::RegisterDependencyConditions* dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg);

   dependencies->addPostCondition(signCode,     TR::RealRegister::AssignAny);
   dependencies->addPostCondition(signCode4Bit, TR::RealRegister::AssignAny);

   if (destMR->getIndexRegister())
      dependencies->addPostCondition(destMR->getIndexRegister(), TR::RealRegister::AssignAny);

   if (destMR->getBaseRegister())
      dependencies->addPostCondition(destMR->getBaseRegister(), TR::RealRegister::AssignAny);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, dependencies);
   cFlowRegionEnd->setEndInternalControlFlow();

   // Cleanup registers before returning
   cg->stopUsingRegister(signCode);
   cg->stopUsingRegister(signCode4Bit);
   }

TR::MemoryReference *
J9::Z::TreeEvaluator::packedToZonedHelper(TR::Node *node, TR_PseudoRegister *targetReg, TR::MemoryReference *sourceMR, TR_PseudoRegister *childReg, TR::CodeGenerator * cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Compilation *comp = cg->comp();

   TR_StorageReference *hint = node->getStorageReferenceHint();
   TR_StorageReference *targetStorageReference = NULL;
   int32_t destSize = 0;
   if (hint)
      {
      TR_ASSERT( !childReg->isInitialized() || hint != childReg->getStorageReference(),"bcd conversion operands will overlap\n");
      destSize = hint->getSymbolSize(); // may be be larger than the node->getSize() so take this opportunity to widen as part of the UNPK
      targetStorageReference = hint;
      }
   else
      {
      destSize = node->getSize();
      targetStorageReference = TR_StorageReference::createTemporaryBasedStorageReference(destSize, comp);
      }

   targetReg->setStorageReference(targetStorageReference, node);

   int32_t destPrecision = TR::DataType::getBCDPrecisionFromSize(node->getDataType(), destSize);
//   int32_t destPrecision = destSize;
   targetReg->setDecimalPrecision(destPrecision);
   int32_t sourcePrecision = childReg->getDecimalPrecision();
   int32_t sourceSize = childReg->getSize();

   // skip over trailing sign for the unpack
   bool isDestTrailingSign = (node->getDataType() == TR::ZonedDecimalSignTrailingSeparate);
   int32_t destEndByte = isDestTrailingSign ? destPrecision + TR::DataType::getZonedSignSize() :
                                              destPrecision;

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tpackedToZonedHelper %p : op %s, destPrecision %d, destSize %d, destEndByte %d, sourcePrecision %d, sourceSize %d\n",
         node,node->getOpCode().getName(),destPrecision,destSize,destEndByte,sourcePrecision,childReg->getSize());

   bool isTruncation = false;
   if (destPrecision < childReg->getDecimalPrecision())
      {
      isTruncation = true;
      sourcePrecision = destPrecision;
      sourceSize = TR::DataType::getSizeFromBCDPrecision(child->getDataType(), sourcePrecision);

      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tisTruncation=true (dstPrec %d < srcPrec %d) reduce srcPrec %d->%d, srcSize %d->%d\n",
            destPrecision,childReg->getDecimalPrecision(),childReg->getDecimalPrecision(),sourcePrecision,childReg->getSize(),sourceSize);
      }

   TR::Node *paddingAnchor = NULL;
   bool evaluatedPaddingAnchor = false;
   TR::MemoryReference *destMR = NULL;
   if (destPrecision <= 16 || sourcePrecision <= 16)
      {
      int32_t unpkDestOffset = 0;
      int32_t unpkDestSize = destPrecision;
      int32_t unpkSourceSize  = sourceSize;
      destMR = generateS390LeftAlignedMemoryReference(node, targetStorageReference, cg, destEndByte);

      if (destPrecision > 16)
         {
         int32_t bytesToSet = destPrecision-sourcePrecision;
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tdestPrecision %d > 16, sourcePrecision %d <= 16 gen %d leftmost bytes of 0xF0\n",destPrecision,sourcePrecision,bytesToSet);
         TR_ASSERT(bytesToSet > 0,"destPrecision (%d) should be > sourcePrecision (%d)\n",destPrecision,sourcePrecision);
         cg->genZeroLeftMostZonedBytes(node, targetReg, destEndByte, bytesToSet, destMR);
         evaluatedPaddingAnchor = true;
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\treduce unpkDestOffset %d->%d and unpkDestSize %d->%d\n",unpkDestOffset,bytesToSet,unpkDestSize,sourcePrecision);
         unpkDestOffset = bytesToSet;
         unpkDestSize = sourcePrecision;
         }

      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tdestPrecision %d <= 16 or sourcePrecision %d <= 16 so generate a single UNPK destPrecision %d, destOffset %d, unpkSourceSize %d\n",
                      destPrecision,sourcePrecision,unpkDestSize,unpkDestOffset,unpkSourceSize);
      generateSS2Instruction(cg, TR::InstOpCode::UNPK, node,
                             unpkDestSize-1,
                             generateS390LeftAlignedMemoryReference(*destMR, node, unpkDestOffset, cg, destMR->getLeftMostByte()),
                             unpkSourceSize-1,
                             generateS390RightAlignedMemoryReference(*sourceMR, node, 0, cg));
      if (unpkDestSize > sourcePrecision)
         {
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tunpkDestSize %d > sourcePrecision %d adding range of zero digits for pd2zd op\n",unpkDestSize,sourcePrecision);
         targetReg->addRangeOfZeroDigits(sourcePrecision, unpkDestSize);
         }
      }
   else
      {
      TR_ASSERT(destPrecision <= 31,"pd2zd destPrecision should be <= 31 and not %d\n",destPrecision);
      TR_ASSERT(sourcePrecision <= 31,"pd2zd sourcePrecision should be <= 31 and not %d\n",sourcePrecision);
      if (cg->traceBCDCodeGen())
         {
         if (sourcePrecision >= 17 && sourcePrecision <= 30)
            traceMsg(comp,"\tsourcePrecision 17 <= %d <= 30 so generate two UNPKs\n",sourcePrecision);
         else
            traceMsg(comp,"\tsourcePrecision == 31 so generate three UNPKs\n");
         }
      bool needsThirdUnpk = false;
      int32_t precisionAdjustment = 14;
      if (sourcePrecision == 31)
         {
         precisionAdjustment=16;
         needsThirdUnpk = true;
         }
      else
         {
         // in this case can do the conversion in 2 UNPKs instead of 3. Keep the target precision up to 30 bytes to widen extra bytes.
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tsourcePrecision < 31 (%d) so reduce destPrecision to min(destPrecision,30) = min(%d,30) = %d ",
               sourcePrecision,destPrecision,std::min(destPrecision,30));
         destPrecision = std::min(destPrecision, 30);
         destEndByte = isDestTrailingSign ? destPrecision + TR::DataType::getZonedSignSize() :
                                            destPrecision;
         targetReg->setDecimalPrecision(destPrecision);
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"and update targetReg->prec to new destPrecision %d and update destEndByte to %d\n",destPrecision,destEndByte);
         }

      if (cg->traceBCDCodeGen())
         traceMsg(comp,"x^x : found large packed/zoned conv -- node %s (%p) prec %d, child %s (%p) prec %d (three=%s)\n",
            node->getOpCode().getName(),node,destPrecision,
            child->getOpCode().getName(),child,sourcePrecision,needsThirdUnpk?"yes":"no");

      destMR = generateS390LeftAlignedMemoryReference(node, targetStorageReference, cg, destEndByte);
      sourceMR = generateS390LeftAlignedMemoryReference(*sourceMR, node, 0, cg, sourceSize);
      int32_t unpk1DestSize   = destPrecision-precisionAdjustment;
      int32_t unpk1SourceSize = TR::DataType::getSizeFromBCDPrecision(child->getDataType(), sourcePrecision-precisionAdjustment);
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\t\t1st UNPK destSize=%d,srcSize=%d\n",unpk1DestSize,unpk1SourceSize);
      generateSS2Instruction(cg, TR::InstOpCode::UNPK, node,
                             unpk1DestSize-1,
                             destMR,
                             unpk1SourceSize-1,
                             sourceMR);
      int32_t unpk2DestSize = 15;
      int32_t unpk2DestOffset = unpk1DestSize-1;
      int32_t unpk2SourceSize = TR::DataType::getSizeFromBCDPrecision(child->getDataType(), 15);
      int32_t unpk2SourceOffset = unpk1SourceSize-1;
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\t\t2nd UNPK destSize=%d,destOffset=%d, srcSize=%d,srcOffset=%d\n",unpk2DestSize,unpk2DestOffset,unpk2SourceSize,unpk2SourceOffset);
      generateSS2Instruction(cg, TR::InstOpCode::UNPK, node,
                             unpk2DestSize-1,
                             generateS390LeftAlignedMemoryReference(*destMR, node, unpk2DestOffset, cg, destMR->getLeftMostByte()),
                             unpk2SourceSize-1,
                             generateS390LeftAlignedMemoryReference(*sourceMR, node, unpk2SourceOffset, cg, sourceMR->getLeftMostByte()));
      if (needsThirdUnpk)
         {
         int32_t unpk3DestSize = 3;
         int32_t unpk3DestOffset = unpk2DestOffset+(unpk2DestSize-1);
         int32_t unpk3SourceSize = TR::DataType::getSizeFromBCDPrecision(child->getDataType(), 3);
         int32_t unpk3SourceOffset = unpk2SourceOffset+(unpk2SourceSize-1);
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\t\t3rd UNPK destSize=%d,destOffset=%d, srcSize=%d,srcOffset=%d\n",unpk3DestSize,unpk3DestOffset,unpk3SourceSize,unpk3SourceOffset);
         generateSS2Instruction(cg, TR::InstOpCode::UNPK, node,
                                unpk3DestSize-1,
                                generateS390LeftAlignedMemoryReference(*destMR, node, unpk3DestOffset, cg, destMR->getLeftMostByte()),
                                unpk3SourceSize-1,
                                generateS390LeftAlignedMemoryReference(*sourceMR, node, unpk3SourceOffset, cg, sourceMR->getLeftMostByte()));
         }
      }

   if (!evaluatedPaddingAnchor)
      cg->processUnusedNodeDuringEvaluation(paddingAnchor);

   pd2zdSignFixup(node, destMR, cg);

   targetReg->transferSignState(childReg, isTruncation);
   targetReg->transferDataState(childReg);
   targetReg->setIsInitialized();
   node->setRegister(targetReg);
   return destMR;
   }

TR::Register *
J9::Z::TreeEvaluator::pd2zdVectorEvaluatorHelper(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation* comp = cg->comp();
   traceMsg(comp, "DAA: Enter pd2zdVectorEvaluatorHelper\n");
   TR_PseudoRegister *targetReg = cg->allocatePseudoRegister(node->getDataType());

   // pd2zd we need to create storagerefence and save this value to the memoryreference
   // associated to that storagereference.
   // To do this, we need to
   //
   // 1. create NodeBasedStorageReference,
   // 2. creatememoryreference from the StorageREference,
   // 3. Use the memory reference to create VUPKZ instruction
   //
   // return the allocate PseudoRegister associate the storage reference to the Pseudo register
   // return this pseudoregister/
   //
   TR_StorageReference *hint = node->getStorageReferenceHint();
   int32_t sizeOfZonedValue = node->getSize(); //for zoned node, precision and the size must be the same.
   int32_t precision = node->getDecimalPrecision();
   TR_StorageReference* targetStorageReference = hint ? hint : TR_StorageReference::createTemporaryBasedStorageReference(sizeOfZonedValue, comp);

   targetReg->setStorageReference(targetStorageReference, node);
   TR::Node *child = node->getFirstChild(); //This child will evaluate to Vector Register
   TR::Register *valueRegister = cg->evaluate(child);
   TR_ASSERT((valueRegister->getKind() == TR_VRF || valueRegister->getKind() == TR_FPR),
             "valueChild should evaluate to Vector register.");

   TR::MemoryReference *targetMR = generateS390LeftAlignedMemoryReference(node, targetStorageReference, cg, sizeOfZonedValue, false);

   if (!targetStorageReference->isTemporaryBased())
      {
      TR::SymbolReference *memSymRef = targetStorageReference->getNode()->getSymbolReference();
      if (memSymRef)
         {
         targetMR->setListingSymbolReference(memSymRef);
         }
      }

   if(cg->traceBCDCodeGen())
      {
      traceMsg(comp, "gen VUKPZ, sizeOfZonedValue=%d, precision=%d\n", sizeOfZonedValue, precision);
      }

   generateVSIInstruction(cg, TR::InstOpCode::VUPKZ, node, valueRegister, targetMR, sizeOfZonedValue - 1);

   // Fix pd2zd signs. VUPKZ and its non-vector counterpart don't validate digits nor signs.
   pd2zdSignFixup(node, targetMR, cg);

   node->setRegister(targetReg);
   cg->decReferenceCount(child);
   targetReg->setIsInitialized();
   traceMsg(comp, "DAA: Leave pd2zdVectorEvaluatorHelper\n");
   return targetReg;
   }

TR::Register *
J9::Z::TreeEvaluator::pd2zdEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   cg->traceBCDEntry("pd2zd",node);
   TR::Register* targetReg = NULL;
   cg->generateDebugCounter("PD-Op/pd2zd", 1, TR::DebugCounter::Cheap);

   static char* isVectorBCDEnv = feGetEnv("TR_enableVectorBCD");
   if(TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() &&
           !cg->comp()->getOption(TR_DisableVectorBCD) ||
           isVectorBCDEnv)
      {
      targetReg = pd2zdVectorEvaluatorHelper(node, cg);
      }
   else
      {
      targetReg = cg->allocatePseudoRegister(node->getDataType());
      TR::Node *child = node->getFirstChild();
      TR_PseudoRegister *childReg = cg->evaluateBCDNode(child);
      childReg = cg->privatizeBCDRegisterIfNeeded(node, child, childReg);
      TR::MemoryReference *sourceMR = generateS390RightAlignedMemoryReference(child, childReg->getStorageReference(), cg);
      packedToZonedHelper(node, static_cast<TR_PseudoRegister*>(targetReg), sourceMR, childReg, cg);
      cg->decReferenceCount(child);
      node->setRegister(targetReg);
      }

   cg->traceBCDExit("pd2zd",node);
   return targetReg;
   }

// mask is SZPF
//         0123

#define TR_DFP_TO_ZONED_SIGN           (0x8) ///< S: does target zoned operand have a sign field (S=0 no, S=1 yes)
#define TR_DFP_TO_ZONED_ZONE           (0x4) ///< Z: encode zone field as 0xf (Z=0) or as 0x3 (Z=1)
#define TR_DFP_TO_ZONED_PLUS           (0x2) ///< P: encode plus sign as 0xc (P=0) or as 0xf (P=1). Ignored when S=0 and sign is then determined by Z.
#define TR_DFP_TO_ZONED_FORCE_CLEAN    (0x1) ///< F: force negative zero to positive zero when F=1. The plus sign 0xc vs 0xf is determined by P.  Ignored when S=0.

#define TR_MAX_DFP_ZONED_FAST_SIZE  (32) ///< these conversion instructions slow down when the actual length is > 32
#define TR_MAX_DFP_ZONED_SIZE       (34) ///< max actual length (<=34) these conversion instructions can handle

/**
 * Handles TR::df2zd,TR::dd2zd,TR::de2zd,TR::df2zdSetSign,TR::dd2zdSetSign,TR::de2zdSetSign,TR::df2zdClean,TR::dd2zdClean,TR::de2zdClean
 */
TR::Register *
J9::Z::TreeEvaluator::df2zdEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   cg->traceBCDExit("df2zd",node);

   TR_ASSERT( TR::Compiler->target.cpu.getSupportsArch(TR::CPU::zEC12),"CDZT/CXZT only valid on >= arch(10)\n");
   TR_ASSERT(node->getDecimalFraction() == 0,"frac should be 0 and not %d\n",node->getDecimalFraction());

   TR::Node *srcNode = node->getFirstChild();
   TR::Register *srcFPReg = cg->evaluate(srcNode);
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   bool isFloat      = srcNode->getDataType() == TR::DecimalFloat;
   bool isDouble     = srcNode->getDataType() == TR::DecimalDouble;
   bool isLongDouble = srcNode->getDataType() == TR::DecimalLongDouble;

   if (isFloat)
      {
      TR::Register *srcFPRegTemp = cg->allocateRegister(TR_FPR);
      generateRRFInstruction(cg, TR::InstOpCode::LDETR, node, srcFPRegTemp, srcFPReg, 0, false); // m4=0
      srcFPReg = srcFPRegTemp;
      cg->stopUsingRegister(srcFPRegTemp);
      isFloat = false;
      isDouble = true;
      }

   bool isSetSign = node->isSetSignValueOnNode();
   bool isClean = node->getOpCodeValue() == TR::df2zdClean || node->getOpCodeValue() == TR::dd2zdClean || node->getOpCodeValue() == TR::de2zdClean;

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\t%s : isSetSign %s, isClean %s\n",node->getOpCode().getName(),isSetSign?"yes":"no",isClean?"yes":"no");

   uint8_t mask = 0; // 0 is unsigned with ebcdic zone fields

   if (cg->traceBCDCodeGen()) traceMsg(comp,"\tinit mask to 0x%x\n",mask);

   bool needsSetSign = isSetSign;
   int32_t setSignValue = TR::DataType::getInvalidSignCode();
   bool signIsPreferred = true;
   bool signIsClean = false;
   bool resultSignIsUnsigned = false;
   TR::InstOpCode::Mnemonic setSignOpCode  = TR::InstOpCode::BAD;
   if (isSetSign)
      {
      // for all setSign cases do not set TR_DFP_TO_ZONED_SIGN in the mask -- for 0xf there is then nothing to do and for 0xc/0xd the sign is easier to set on top of 0xf
      resultSignIsUnsigned = true;
      TR_ASSERT(!isClean,"setsign and clean must not be set together on node %s (%p)\n",node->getOpCode().getName(),node);
      TR_ASSERT(TR::DataType::getUnsignedCode() == 0xf,"expecting unsigned code to be 0xf and not 0x%x for node %s (%p)\n",
              TR::DataType::getUnsignedCode(),node->getOpCode().getName(),node);
      TR_RawBCDSignCode sign = node->getSetSign();
      if (sign == raw_bcd_sign_0xf)
         {
         needsSetSign = false;
         signIsPreferred = false;
         if (cg->traceBCDCodeGen()) traceMsg(comp,"\t\tsetSign 0xf: leave mask at 0x%x for setSign 0xf\n",mask);
         }
      else if (sign == raw_bcd_sign_0xc)
         {
         setSignValue = TR::DataType::getPreferredPlusCode();
         if (cg->traceBCDCodeGen()) traceMsg(comp,"\t\tsetSign 0xc: leave mask at 0x%x and encode setSign 0xc afterwards on top of 0xf\n",mask);
         TR_ASSERT(TR::DataType::getPreferredPlusCode() == 0xc,"expecting preferred plus code to be 0xc and not 0x%x for node %s (%p)\n",
                 TR::DataType::getPreferredPlusCode(),node->getOpCode().getName(),node);

         setSignOpCode = TR::InstOpCode::LPDFR;
         }
      else if (sign == raw_bcd_sign_0xd)
         {
         setSignValue = TR::DataType::getPreferredMinusCode();
         if (cg->traceBCDCodeGen()) traceMsg(comp,"\t\tsetSign 0xd: leave mask at 0x%x and encode setSign 0xd afterwards on top of 0xf\n",mask);
         TR_ASSERT(TR::DataType::getPreferredMinusCode() == 0xd,"expecting preferred minus code to be 0xd and not 0x%x for node %s (%p)\n",
                 TR::DataType::getPreferredMinusCode(),node->getOpCode().getName(),node);

         setSignOpCode = TR::InstOpCode::LNDFR;
         }
      else
         {
         TR_ASSERT(false,"unsupported sign %d for %s (%p)\n",sign,node->getOpCode().getName(),node);
         }
      }
   else
      {
      mask |= TR_DFP_TO_ZONED_SIGN; // encode result as signed
      if (cg->traceBCDCodeGen()) traceMsg(comp,"\t\tsigned: update mask to 0x%x (+Sign 0x%x)\n",mask,TR_DFP_TO_ZONED_SIGN);
      if (isClean)
         {
         // TODO : signIsClean can also be set in non-setSign 0xf (TR_DFP_TO_ZONED_PLUS) cases where there is no truncation
         //      : or could always set TR_DFP_TO_ZONED_FORCE_CLEAN (no harm unless this somehow makes the instruction more expensive)
         mask |= TR_DFP_TO_ZONED_FORCE_CLEAN;
         signIsClean = true;
         if (cg->traceBCDCodeGen()) traceMsg(comp,"\t\tclean: update mask to 0x%x (+ForcePlus 0x%x to clean the sign)\n",mask,TR_DFP_TO_ZONED_FORCE_CLEAN);
         }
      }

   // DFP abs or set negative; the CZDT/CZXT will set the sign to 0xc or 0xd
   if (setSignOpCode != TR::InstOpCode::BAD)
      {
      TR::Register *setSignTempReg;
      if (isLongDouble)
         {
         setSignTempReg = cg->allocateFPRegisterPair();
         generateRRInstruction(cg, setSignOpCode, node, setSignTempReg->getHighOrder(), srcFPReg->getHighOrder());
         }
      else
         {
         setSignTempReg = cg->allocateRegister(TR_FPR);
         generateRRInstruction(cg, setSignOpCode, node, setSignTempReg, srcFPReg);
         }

      srcFPReg = setSignTempReg;
      cg->stopUsingRegister(setSignTempReg);
      }


   TR_PseudoRegister *targetReg = cg->allocatePseudoRegister(node->getDataType());
   TR_StorageReference *hint = node->getStorageReferenceHint();
   TR_StorageReference *targetStorageReference = NULL;
   int32_t memSize = 0; // Amount of memory allocated for the destination value
   int32_t regSize = node->getSize(); // Size used to determine the precision for the target register

   if (node->hasSourcePrecision() && node->getSourcePrecision() <= TR::DataType::getMaxExtendedDFPPrecision())
      {
      memSize = std::max<int32_t>(node->getDecimalPrecision(), node->getSourcePrecision());
      }
   else
      {
      // If source precision is unknown, assume the maximum possible value to avoid overflow errors
      if (isFloat || isDouble)
         memSize = TR::DataType::getMaxLongDFPPrecision();
      else
         memSize = TR::DataType::getMaxExtendedDFPPrecision();
      }

   if (hint)
      {
      // If there is no truncation, use the hint size to possibly widen here for free, rather than on a store.
      // If there is a truncation, we can't use the hint size without producing an incorrect result.
      // zdstore p=9      zdstore p = 6   zdstore p=3
      //   dd2zd p=8        dd2zd p = 4     dd2zd p=4
      //     ddX p=8          ddX p = 7       ddX p=7
      // For case 1, using the hint size is good. For case 2, it's incorrect, as we'd lose the truncation to 4 digits.
      // In case 3, using the hint size is fine, but there shouldn't be a hint generated since the p=4 dd2zd result can't fit
      // in the p=3 zdstore field
      if (node->hasSourcePrecision() && !node->isTruncating())
         {
         // Don't widen if doing so would require a conversion from double to long double
         if (!isDouble || (hint->getSymbolSize() <= TR::DataType::getMaxLongDFPPrecision()))
             {
            // Widen up to 32 digits (33 and 34 are slower)
            if (regSize <= TR_MAX_DFP_ZONED_FAST_SIZE && hint->getSymbolSize() > TR_MAX_DFP_ZONED_FAST_SIZE)
               regSize = TR_MAX_DFP_ZONED_FAST_SIZE;
            else
               regSize = hint->getSymbolSize(); // Widen up to the symbol size

            // Enlarge the memory required if it's now smaller than the precision we'll set on the target register
            if (regSize > memSize)
               memSize = regSize;
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\tset memSize = %d and regSize = %d based on hint #%d\n",memSize,regSize,hint->getReferenceNumber());
             }
         }
      targetStorageReference = hint;

      }
   else
      {
      targetStorageReference = TR_StorageReference::createTemporaryBasedStorageReference(memSize, comp);
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tset memSize = %d based on node size\n",memSize);
      }

   // regardless of how the memSize and regSize were set up earlier ensure they do not exceed the limits for the CZDT or CZXT instructions
   // and assert below if these max instruction settings are >= the max # of significant digits that can be represented for each DFP type
   if (isFloat)
      {
      TR_ASSERT(TR_DECIMAL_FLOAT_TO_ZONED_MAX_PRECISION >= TR::DataType::getMaxShortDFPPrecision(),"CZDT max %d must be >= DFP short max %d\n",
         TR_DECIMAL_FLOAT_TO_ZONED_MAX_PRECISION, TR::DataType::getMaxShortDFPPrecision());
      memSize = std::min(memSize, TR_DECIMAL_FLOAT_TO_ZONED_MAX_PRECISION);   // cannot get more than 16 digits from the DFP source (and larger encodings are illegal)
      regSize = std::min(regSize, TR_DECIMAL_FLOAT_TO_ZONED_MAX_PRECISION);
      }
   else if (isDouble)
      {
      TR_ASSERT(TR_DECIMAL_DOUBLE_TO_ZONED_MAX_PRECISION >= TR::DataType::getMaxLongDFPPrecision(),"CZDT max %d must be >= DFP long max %d\n",
         TR_DECIMAL_DOUBLE_TO_ZONED_MAX_PRECISION, TR::DataType::getMaxLongDFPPrecision());
      memSize = std::min(memSize, TR_DECIMAL_DOUBLE_TO_ZONED_MAX_PRECISION);   // cannot get more than 16 digits from the DFP source (and larger encodings are illegal)
      regSize = std::min(regSize, TR_DECIMAL_DOUBLE_TO_ZONED_MAX_PRECISION);
      }
   else
      {
      TR_ASSERT(TR_DECIMAL_LONG_DOUBLE_TO_ZONED_MAX_PRECISION >= TR::DataType::getMaxExtendedDFPPrecision(),"CZXT max %d must be >= DFP long double max %d\n",
         TR_DECIMAL_LONG_DOUBLE_TO_ZONED_MAX_PRECISION, TR::DataType::getMaxExtendedDFPPrecision());
      memSize = std::min(memSize, TR_DECIMAL_LONG_DOUBLE_TO_ZONED_MAX_PRECISION);   // cannot get more than 34 digits from the DFP source (and larger encodings are illegal)
      regSize = std::min(regSize, TR_DECIMAL_LONG_DOUBLE_TO_ZONED_MAX_PRECISION);
      }

   if (regSize > memSize)
      {
      // since the memSize is the max # of digits possibly converted then cannot set a higher regSize as these upper digits/bytes would be uninitialized
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tregSize %d > memSize %d : reduce regSize to %d\n",regSize,memSize,memSize);
      regSize = memSize;
      }

   int32_t destPrecision = TR::DataType::getBCDPrecisionFromSize(node->getDataType(), regSize);
   targetReg->setDecimalPrecision(destPrecision);

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tset destPrecision = %d\n",destPrecision);

   targetReg->setStorageReference(targetStorageReference, node);

   bool enforceSSLimits = true;  // RSL has same limits as SS (no index and 12 bit displacement)
   TR::MemoryReference *destMR = generateS390LeftAlignedMemoryReference(node, targetStorageReference, cg, memSize, enforceSSLimits);

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tgenerate DFP to zoned %s (%p) with op=%s and target zoned size %d, mask 0x%x (signIsClean %s, signIsPreferred %s)\n",
              node->getOpCode().getName(),node,isLongDouble ? "TR::InstOpCode::CZXT" : "TR::InstOpCode::CZDT",memSize,mask,signIsClean?"yes":"no",signIsPreferred?"yes":"no");

   if (isFloat)
      {
      TR_ASSERT(memSize > 0 && memSize <= TR_DECIMAL_FLOAT_TO_ZONED_MAX_PRECISION,"memSize %d out of allowed range 1 -> %d for convert DFP to zoned (CZDT)\n",
         memSize,TR_DECIMAL_FLOAT_TO_ZONED_MAX_PRECISION);
      }
   else if (isDouble)
      {
      TR_ASSERT(memSize > 0 && memSize <= TR_DECIMAL_DOUBLE_TO_ZONED_MAX_PRECISION,"memSize %d out of allowed range 1 -> %d for convert DFP to zoned (CZDT)\n",
         memSize,TR_DECIMAL_DOUBLE_TO_ZONED_MAX_PRECISION);
      }
   else
      {
      TR_ASSERT(isLongDouble,"type should be isLongDouble for node %s (%p)\n",node->getOpCode().getName(),node);
      TR_ASSERT(memSize > 0 && memSize <= TR_DECIMAL_LONG_DOUBLE_TO_ZONED_MAX_PRECISION,"memSize %d out of allowed range 1 -> %d for convert DFP to zoned (CZXT)\n",
         memSize,TR_DECIMAL_LONG_DOUBLE_TO_ZONED_MAX_PRECISION);
      }

   generateRSLbInstruction(cg, isLongDouble ? TR::InstOpCode::CZXT : TR::InstOpCode::CZDT, node, srcFPReg, memSize-1, destMR, mask);

   if (needsSetSign)
      {
      targetReg->setKnownSignCode(setSignValue);
      }
   else
      {
      if (signIsClean)
         targetReg->setHasKnownCleanSign();
      else if (signIsPreferred)
         targetReg->setHasKnownPreferredSign();
      }

   if (cg->traceBCDCodeGen())
      {
      traceMsg(comp,"\ttargetReg %s signState : cleanSign=%s, preferredSign=%s, knownSign=%s (0x%x)\n",
              cg->getDebug()->getName(targetReg),
              targetReg->hasKnownCleanSign() ? "yes" : "no", targetReg->hasKnownPreferredSign() ? "yes" : "no",
              targetReg->hasKnownSignCode() ? "yes" : "no", targetReg->hasKnownSignCode() ? targetReg->getKnownSignCode() : TR::DataType::getInvalidSignCode());
      }

   targetReg->setIsInitialized();
   targetReg->setHasKnownValidSignAndData();

   node->setRegister(targetReg);
   cg->decReferenceCount(srcNode);
   cg->traceBCDExit("df2zd",node);

   return targetReg;
   }

#define TR_ZONED_TO_DFP_SIGNED   (0x8)
#define TR_ZONED_TO_DFP_UNSIGNED (0x0)

/**
 * Handles TR::zd2df,TR::zd2dd,TR::zd2de,TR::zd2dfAbs,TR::zd2ddAbs,TR::zd2deAbs
 */
TR::Register *
J9::Z::TreeEvaluator::zd2ddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   cg->traceBCDEntry("zd2dd",node);

   TR_ASSERT( TR::Compiler->target.cpu.getSupportsArch(TR::CPU::zEC12),"CDZT/CXZT only valid on >= arch(10)\n");
   TR_ASSERT(node->getDataType() == TR::DecimalDouble || node->getDataType() == TR::DecimalLongDouble,"expecting op to be zd2dd or zd2de and not %d\n",node->getOpCodeValue());
   TR_ASSERT(node->getDecimalFraction() == 0,"frac should be 0 and not %d\n",node->getDecimalFraction());

   bool isAbs = node->getOpCodeValue() == TR::zd2dfAbs || node->getOpCodeValue() == TR::zd2ddAbs || node->getOpCodeValue() == TR::zd2deAbs;
   bool isLongDouble = node->getDataType() == TR::DecimalLongDouble;
   uint8_t mask = isAbs ? TR_ZONED_TO_DFP_UNSIGNED : TR_ZONED_TO_DFP_SIGNED;

   TR::Compilation *comp = cg->comp();
   TR::Node *srcNode = node->getFirstChild();
   TR_PseudoRegister *srcReg = cg->evaluateBCDNode(srcNode);
   TR_StorageReference *srcStorageReference = srcReg->getStorageReference();
   int32_t srcRegSize = srcReg->getSize();

   bool enforceSSLimits = true;  // RSL has same limits as SS (no index and 12 bit displacement)
   TR::MemoryReference *sourceMR = generateS390LeftAlignedMemoryReference(srcNode, srcStorageReference, cg, srcRegSize, enforceSSLimits);

   TR::Register *fprTargetReg = isLongDouble ? cg->allocateFPRegisterPair() : cg->allocateRegister(TR_FPR);

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tgenerate zoned to DFP %s (%p) with op=%s and src zoned size %d, mask 0x%x (%ssigned)\n",
         node->getOpCode().getName(),node,isLongDouble ? "CXZT" : "CDZT",srcRegSize,mask,isAbs?"un":"");

   if (isLongDouble)
      {
      TR_ASSERT(srcRegSize > 0 && srcRegSize <= TR_ZONED_TO_DECIMAL_LONG_DOUBLE_MAX_SIZE,"srcRegSize %d not in valid CXZT range %d -> %d\n",
         srcRegSize,0,TR_ZONED_TO_DECIMAL_LONG_DOUBLE_MAX_SIZE);
      }
   else
      {
      TR_ASSERT(srcRegSize > 0 && srcRegSize <= TR_ZONED_TO_DECIMAL_DOUBLE_MAX_SIZE,"srcRegSize %d not in valid CDZT range %d -> %d\n",
         srcRegSize,0,TR_ZONED_TO_DECIMAL_DOUBLE_MAX_SIZE);
      }

   generateRSLbInstruction(cg, isLongDouble ? TR::InstOpCode::CXZT : TR::InstOpCode::CDZT, node, fprTargetReg, srcRegSize-1, sourceMR, mask);

   node->setRegister(fprTargetReg);
   cg->decReferenceCount(srcNode);
   cg->traceBCDExit("zd2dd",node);

   return fprTargetReg;
   }

bool
J9::Z::TreeEvaluator::isZonedOperationAnEffectiveNop(TR::Node * node, int32_t shiftAmount, bool isTruncation, TR_PseudoRegister *srcReg, bool isSetSign, int32_t signToSet, TR::CodeGenerator * cg)
   {
   bool isEffectiveNop = false;
   int32_t zone = TR::DataType::getZonedValue();
   // For skipLeadingSignReset to be correct the node refCount must be 1 otherwise a commoned reference may be exposed to an incorrect
   // zone nibble (it will be the source's sign code and not the correct zone value)
   bool skipLeadingSignReset = false;
   bool srcSignIsZone = srcReg->knownOrAssumedSignIsZone();
   bool signIsAlreadySet = srcReg->hasKnownOrAssumedSignCode() && (srcReg->getKnownOrAssumedSignCode()==signToSet);
   bool signToSetIsZone          = signToSet == zone;
   bool signToSetIsIgnored       = signToSet == TR::DataType::getIgnoredSignCode();
   bool signToSetIsZoneOrIgnored = signToSetIsZone || signToSetIsIgnored;
   TR::Compilation *comp = cg->comp();

   TR_ASSERT(!node->getOpCode().isRightShift() || shiftAmount > 0,"shiftAmount should be > 0 for zoned right shifts and not a %d\n",shiftAmount);
   switch (node->getOpCodeValue())
      {
      case TR::zd2zdsle:
         isEffectiveNop = srcSignIsZone || (node->getDecimalPrecision() == 1);
         break;
      case TR::zdsle2zd:
         isEffectiveNop = srcSignIsZone || (srcReg->getDecimalPrecision() == 1);
         break;
      case TR::zdsts2zd:
      case TR::zdsls2zd:
         break;
      default:
         TR_ASSERT(false,"unexpected zoned opcode %d\n",node->getOpCodeValue());
         break;
      }
   return isEffectiveNop;
   }

/**
 * \brief This evaluator helper function evaluates BCDCHK nodes by emitting mainline and out-of-line instructions for
 * the underlying packed decimal operations. The mainline instructions perform the actual operations, and the OOL
 * instructions are for hardware exception handling.
 *
 * The canonical BCDCHK IL structure is the following:
 *
 * BCDCHK
 *      pdOpNode        // the operation node
 *      aladd           // optional address node. Exists only if the result of the operation is packed decimal
 *      callParam1      // call parameter nodes of the original DAA API call
 *      callParam2
 *      .
 *      .
 *      callParamN
 *
 * With the new DAA BCDCHK node tree structure, the first child of a BCDCHK node is
 * always the PD opNode. The first child and its sub-tree could throw packed decimal related hardware exceptions, which is
 * to be handled by the designated OOL instruction sequence.
 *
 * As for the second child of BCDCHK, it will be an address node if the result of the PD operation is a packed decimal. This address
 * node is to be used by the OOL for result copy back.
 *
 * The steps to evaluate the new BCDCHK node is the following:
 *
 * -# Create a callNode and attached BCDCHK's call parameter children to it. This callNode is to be evaluated
 *    later in the OOL section
 *
 * -# If applicable, evaluate address node's children (e.g. this is applicable to i2pd but not to PD comparisons)
 *
 * -# Create a handlerLabel that points to the start of the OOL section
 *
 * -# Evaluate the pdopNode (first child) and decrement its refCount.
 *
 * -# Emit a NOP BRC bearing the handlerLabel right after evaluating the pdopNode. This is for SignalHandler.c
 *
 * -# Switch to OOL code generation and evaluate the callNode
 *
 * -# Evaluate the addressNode (second child of BCDCHK node) to yield a correct address into the byte[]
 *
 * -# Copy the results produced by the call from byte[] back to mainline storage reference
 *
 * -# Finish up by decRefCount on callNode and addressNode
 *
 * \param node                  the BCDCHK node
 * \param cg                    codegen object
 * \param numCallParam          number of callNode children
 * \param callChildStartIndex   the index of the first callChild under the BCDCHK node
 * \param isResultPD            True if the result of the pdOpNode a PD; false if the result is a binary integer/long
 *                              This also implies that the second node of the BCDCHK node is an address node.
 * \param isUseVector           If true, emit vector packed decimal instructions
 * \param isVariableParam       true if the PD operation's precision is not a constant.
*/
TR::Register *
J9::Z::TreeEvaluator::BCDCHKEvaluatorImpl(TR::Node * node,
                                          TR::CodeGenerator * cg,
                                          uint32_t numCallParam,
                                          uint32_t callChildStartIndex,
                                          bool isResultPD,
                                          bool isUseVector,
                                          bool isVariableParam)
   {
   TR_Debug* debugObj = cg->getDebug();
   TR::Node* pdopNode = node->getFirstChild();
   TR::Node* secondChild = node->getSecondChild();

   bool isResultLong = pdopNode->getOpCodeValue() == TR::pd2l ||
                       pdopNode->getOpCodeValue() == TR::pd2lOverflow ||
                       pdopNode->getOpCodeValue() == TR::lcall;

   TR::LabelSymbol* handlerLabel     = generateLabelSymbol(cg);
   TR::LabelSymbol* passThroughLabel = generateLabelSymbol(cg);
   cg->setCurrentBCDCHKHandlerLabel(handlerLabel);

   // This is where the call children node come from and the node that has the call symRef
   TR::Node* childRootNode = isVariableParam ? pdopNode : node;

   // Create a call
   TR::ILOpCodes callType = isResultPD ? TR::call : (isResultLong ? TR::lcall : TR::icall);

   TR::Node * callNode = TR::Node::createWithSymRef(node, callType, numCallParam,
                                                    childRootNode->getSymbolReference());
   cg->incReferenceCount(callNode);
   callNode->setNumChildren(numCallParam);

   // Setup callNode children
   for (uint32_t i = 0; i < numCallParam; ++i)
      callNode->setAndIncChild(i, childRootNode->getChild(i + callChildStartIndex));

   // Evaluate secondChild's children, if the secondChild is an address node into a byte[]
   if(isResultPD && secondChild->getNumChildren() == 2)
      {
      cg->evaluate(secondChild->getFirstChild());
      cg->evaluate(secondChild->getSecondChild());
      }

   // Evaluate intrinsics node
   TR::Register* bcdOpResultReg = NULL;
   if(isVariableParam)
      {
      bcdOpResultReg = pd2lVariableEvaluator(node, cg, isUseVector);
      }
   else if(isResultPD && !isUseVector)
      {
      bcdOpResultReg = cg->evaluateBCDNode(pdopNode);
      }
   else
      {
      bcdOpResultReg = cg->evaluate(pdopNode);
      }

   // start of OOL section
   traceMsg(cg->comp(), "starting OOL section generation.\n");
   TR_S390OutOfLineCodeSection* outlinedHelperCall = new (INSN_HEAP) TR_S390OutOfLineCodeSection(handlerLabel, passThroughLabel, cg);
   cg->getS390OutOfLineCodeSectionList().push_front(outlinedHelperCall);
   outlinedHelperCall->swapInstructionListsWithCompilation();
   // snippetLabel : OOL Start label
   TR::Instruction* cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, handlerLabel);

   if(debugObj)
      {
      debugObj->addInstructionComment(cursor, "Start of BCDCHK OOL sequence");
      }

   // Debug counter for tracking how often we fall back to the OOL path of the DAA intrinsic
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(cg->comp(), "DAA/OOL/(%s)/%p", cg->comp()->signature(), node), 1, TR::DebugCounter::Undetermined);

   // Evaluate the callNode, duplicate and evaluate the address node, and then copy the
   // correct results back to the mainline storage ref or register
   TR::Register* callResultReg = cg->evaluate(callNode);

   if(isResultPD)
      {
      TR::Register* srcBaseReg = cg->evaluate(secondChild);
      TR::MemoryReference* srcMR = generateS390MemoryReference(srcBaseReg, 0, cg);
      int32_t resultSize = TR::DataType::packedDecimalPrecisionToByteLength(pdopNode->getDecimalPrecision());

      if(isUseVector)
         {
         TR_ASSERT(bcdOpResultReg && (bcdOpResultReg->getKind() == TR_VRF || bcdOpResultReg->getKind() == TR_FPR),
                   "Vector register expected\n");

         generateVSIInstruction(cg, TR::InstOpCode::VLRL, node, bcdOpResultReg, srcMR, resultSize - 1);
         }
      else
         {
         TR::MemoryReference* targetMR = generateS390RightAlignedMemoryReference(pdopNode, static_cast<TR_PseudoRegister*>(bcdOpResultReg)->getStorageReference(), cg);
         generateSS1Instruction(cg, TR::InstOpCode::MVC, node, resultSize - 1, targetMR, srcMR);
         }

      cg->decReferenceCount(secondChild);
      cg->stopUsingRegister(callResultReg);
      }
   else
      {
      if(isResultLong)
         {
         generateRREInstruction(cg, TR::InstOpCode::LGR, node, bcdOpResultReg, callResultReg);
         }
      else
         {
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, bcdOpResultReg, callResultReg);
         }
      }

   cg->stopUsingRegister(callResultReg);
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, passThroughLabel);

   // Decrement reference counts
   cg->recursivelyDecReferenceCount(callNode);
   if(isVariableParam)
      {
      // variable parameter l2pd is a call node
      cg->recursivelyDecReferenceCount(pdopNode);
      }
   else
      {
      cg->decReferenceCount(pdopNode);
      }

   if(debugObj)
      {
      debugObj->addInstructionComment(cursor, "End of BCDCHK OOL sequence: return to mainline");
      }

   traceMsg(cg->comp(), "Finished OOL section generation.\n");

   // ***Done using OOL with manual code generation *** //
   outlinedHelperCall->swapInstructionListsWithCompilation();
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, passThroughLabel, cg->getCurrentCheckNodeRegDeps());

   cg->setCurrentBCDCHKHandlerLabel(NULL);
   return bcdOpResultReg;
   }

/**
 * BCDCHKEvaluator -
 */
TR::Register *
J9::Z::TreeEvaluator::BCDCHKEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node* pdopNode = node->getFirstChild();
   TR::Register* resultReg = pdopNode->getRegister();
   bool isResultPD = pdopNode->getDataType() == TR::PackedDecimal;
   bool isVariableParam = false;
   uint32_t firstCallParamIndex = 0;

   static char* isVectorBCDEnv = feGetEnv("TR_enableVectorBCD");
   bool isEnableVectorBCD = TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility()
                               && !cg->comp()->getOption(TR_DisableVectorBCD)
                               || isVectorBCDEnv;

   // Validate PD operations under BCDCHK node
   switch (pdopNode->getOpCodeValue())
      {
      case TR::pdcmpgt:
      case TR::pdcmplt:
      case TR::pdcmpge:
      case TR::pdcmple:
      case TR::pdcmpeq:
      case TR::pdcmpne:
         break;
      case TR::i2pd:
      case TR::l2pd:
      case TR::pd2l:
      case TR::pd2i:
      case TR::pd2iOverflow:
      case TR::pd2lOverflow:
      case TR::pdadd:
      case TR::pdsub:
      case TR::pdmul:
      case TR::pddiv:
      case TR::pdrem:
      case TR::pdshlOverflow:
      case TR::pdshr:
         {
         cg->setIgnoreDecimalOverflowException(node->getLastChild()->getInt() == 0);
         break;
         }
      case TR::lcall:
      case TR::icall:
         {
         switch (pdopNode->getSymbol()->getMethodSymbol()->getMethod()->getRecognizedMethod())
            {
            case TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger_:
            case TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger_ByteBuffer_:
            case TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong_:
            case TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong_ByteBuffer_:
               {
               isVariableParam = true;

               // Need a parameter check because variable PD2L and PD2I could have non-constant 'checkOverflow' (see IS_VARIABLE_PD2I macro).
               TR::Node* checkOverflowNode = pdopNode->getLastChild();
               cg->setIgnoreDecimalOverflowException(checkOverflowNode->getOpCode().isLoadConst() && (checkOverflowNode->getInt() == 0));
               break;
               }

            default:
               {
               /**
                * BCDCHK can have a call node if the PD operation can be simplified to a No-Op.
                * For example, one can get an integer via a call
                * perform a i2pd followed by a pd2i. The pd2i (under BCDCHK) can be simplified to the icall.
                * If this is the case, the lcall/icall must have been evaluated.
                * We can skip the BCDCHK evaluation and return the call result.
                */
               TR_ASSERT_FATAL(resultReg != NULL,
                               "BCDCHKEvaluator: variable precision path encounters an unrecognized and unevaluated long/int call\n");
               }
            }
         break;
         }

      default:
         {
         /**
          * Unrecognized opCodes under BCDCHK should come from optimizations such as local CSE and tree simplifications.
          * They should be commoned nodes that's evaluated previously. Skip these nodes.
          */
         TR_ASSERT_FATAL(resultReg != NULL, "BCDCHKEvaluator: BCDCHK has an unevaluated non-PD node %p (non-PD op code %s) \n",
                         pdopNode,
                         pdopNode->getOpCode().getName());

         traceMsg(cg->comp(), "BCDCHK node n%dn has non-PD operation %s\n",
                  node->getGlobalIndex(), pdopNode->getOpCode().getName());
         }
      }

   if (!isVariableParam)
      {
      firstCallParamIndex = isResultPD ? 2 : 1;
      }

   // Evaluate call parameters
   TR::Node* callParamRoot = isVariableParam ? pdopNode : node;
   for (uint32_t i = firstCallParamIndex; i < callParamRoot->getNumChildren(); ++i)
      {
      if (!callParamRoot->getChild(i)->isSingleRefUnevaluated())
         cg->evaluate(callParamRoot->getChild(i));
      }

   /*
    * Avoid evaluating an evaluated pdOpNode (first child of BCDCHK) under a BCDCHK node if
    * it is already evaluated.
    *
    * This is to avoid generating OOL paths without mainline sequences. OOL without mainline can
    * cause RA to produce incorrect register use counts, and eventually produce incorrect GC maps that
    * make GC fail during runtime.
    */
   if (resultReg != NULL)
      {
      if (isVariableParam)
         cg->recursivelyDecReferenceCount(pdopNode);          // variable parameter l2pd is a call node
      else
         {
         // first child
         cg->decReferenceCount(pdopNode);

         // second child
         if (isResultPD)
            cg->recursivelyDecReferenceCount(node->getSecondChild());

         // call parameters: 2nd/3rd and above
         for (uint32_t i = firstCallParamIndex; i < node->getNumChildren(); ++i)
            cg->decReferenceCount(node->getChild(i));
         }

      traceMsg(cg->comp(), "Skipped BCDCHK node n%dn\n", node->getGlobalIndex());
      }
   else
      {
      uint32_t numCallChildren = isVariableParam ? pdopNode->getNumChildren() : (node->getNumChildren() - firstCallParamIndex);

      TR::RegisterDependencyConditions * daaDeps = new (INSN_HEAP) TR::RegisterDependencyConditions(0, 13, cg);

      cg->setCurrentCheckNodeRegDeps(daaDeps);
      cg->setCurrentCheckNodeBeingEvaluated(node);

      resultReg = BCDCHKEvaluatorImpl(node, cg, numCallChildren, firstCallParamIndex,
                                      isResultPD, isEnableVectorBCD, isVariableParam);

      cg->setCurrentCheckNodeRegDeps(NULL);
      cg->setCurrentCheckNodeBeingEvaluated(NULL);
      }

   cg->setIgnoreDecimalOverflowException(false);
   return resultReg;
   }

TR::Register*
J9::Z::TreeEvaluator::pdcmpVectorEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register* resultReg = cg->allocateRegister(TR_GPR);
   generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, resultReg, resultReg);
   generateLoad32BitConstant(cg, node, 1, resultReg, true);

   TR::RegisterDependencyConditions* deps = new(cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg);
   deps->addPostConditionIfNotAlreadyInserted(resultReg, TR::RealRegister::AssignAny);

   TR::Node* pd1Node = node->getFirstChild();
   TR::Node* pd2Node = node->getSecondChild();

   TR::Register* pd1Value = cg->evaluate(pd1Node);
   TR::Register* pd2Value = cg->evaluate(pd2Node);

   // TODO: should we correct bad sign before comparing them
   TR::Instruction* cursor = generateVRRhInstruction(cg, TR::InstOpCode::VCP, node, pd1Value, pd2Value, 0);

   TR::LabelSymbol* cFlowRegionStart = generateLabelSymbol(cg);
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
   cFlowRegionStart->setStartInternalControlFlow();

   TR::LabelSymbol* cFlowRegionEnd = generateLabelSymbol(cg);

   // Generate Branch Instructions
   switch(node->getOpCodeValue())
      {
      case TR::pdcmpeq:
         cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC0, node, cFlowRegionEnd);
         break;
      case TR::pdcmpne:
         cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, node, cFlowRegionEnd);
         cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC2, node, cFlowRegionEnd);
         break;
      case TR::pdcmplt:
         cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, node, cFlowRegionEnd);
         break;
      case TR::pdcmple:
         cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC0, node, cFlowRegionEnd);
         cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, node, cFlowRegionEnd);
         break;
      case TR::pdcmpgt:
         cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC2, node, cFlowRegionEnd);
         break;
      case TR::pdcmpge:
         cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC0, node, cFlowRegionEnd);
         cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC2, node, cFlowRegionEnd);
         break;
      default:
         TR_ASSERT(0, "Unrecognized op code in pd cmp vector evaluator helper.");
      }

   // TODO: The only reason we keep track of the cursor here is because `deps` has to be passed in after `cursor`. We
   // don't really need this restriction however if we rearrange the parameters.
   cursor = generateLoad32BitConstant(cg, node, 0, resultReg, true, cursor, deps);

   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, deps);
   cFlowRegionEnd->setEndInternalControlFlow();

   node->setRegister(resultReg);

   cg->decReferenceCount(pd1Node);
   cg->decReferenceCount(pd2Node);

   return resultReg;
   }

TR::Register*
J9::Z::TreeEvaluator::pdcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   cg->traceBCDEntry("pdcmpeq",node);
   cg->generateDebugCounter("PD-Op/pdcmpeq", 1, TR::DebugCounter::Cheap);

   // to support castedToBCD have to ensure generateS390CompareBool generates logical comparison only and not CP
   TR_ASSERT(node->castedToBCD() == false,"castedToBCD=true not supported for %s (%p)\n",node->getOpCode().getName(),node);
   TR::Register *targetReg = NULL;

   static char* isVectorBCDEnv = feGetEnv("TR_enableVectorBCD");
   if(TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() && !cg->comp()->getOption(TR_DisableVectorBCD) || isVectorBCDEnv)
      {
      targetReg = pdcmpVectorEvaluatorHelper(node, cg);
      }
   else
      {
      targetReg = generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, TR::InstOpCode::COND_BE, false);
      }

   cg->traceBCDExit("pdcmpeq",node);
   return targetReg;
   }

TR::Register *
J9::Z::TreeEvaluator::pdcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   cg->traceBCDEntry("pdcmpne",node);
   cg->generateDebugCounter("PD-Op/pdcmpne", 1, TR::DebugCounter::Cheap);

   TR_ASSERT(node->castedToBCD() == false,"castedToBCD=true not supported for %s (%p)\n",node->getOpCode().getName(),node);
   TR::Register *targetReg = NULL;

   static char* isVectorBCDEnv = feGetEnv("TR_enableVectorBCD");
   if(TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() && !cg->comp()->getOption(TR_DisableVectorBCD) || isVectorBCDEnv)
      {
      targetReg = pdcmpVectorEvaluatorHelper(node, cg);
      }
   else
      {
      targetReg = generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, TR::InstOpCode::COND_BNE, false);
      }

   cg->traceBCDExit("pdcmpne",node);
   return targetReg;
   }

TR::Register *
J9::Z::TreeEvaluator::pdcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   cg->traceBCDEntry("pdcmplt",node);
   cg->generateDebugCounter("PD-Op/pdcmplt", 1, TR::DebugCounter::Cheap);

   TR_ASSERT(node->castedToBCD() == false,"castedToBCD=true not supported for %s (%p)\n",node->getOpCode().getName(),node);
   TR::Register *targetReg = NULL;

   static char* isVectorBCDEnv = feGetEnv("TR_enableVectorBCD");
   if(TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() && !cg->comp()->getOption(TR_DisableVectorBCD) || isVectorBCDEnv)
      {
      targetReg = pdcmpVectorEvaluatorHelper(node, cg);
      }
   else
      {
      targetReg = generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, TR::InstOpCode::COND_BH, false);
      }

   cg->traceBCDExit("pdcmplt",node);
   return targetReg;
   }

TR::Register *J9::Z::TreeEvaluator::pdcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   cg->traceBCDEntry("pdcmpge",node);
   cg->generateDebugCounter("PD-Op/pdcmpge", 1, TR::DebugCounter::Cheap);

   TR_ASSERT(node->castedToBCD() == false,"castedToBCD=true not supported for %s (%p)\n",node->getOpCode().getName(),node);
   TR::Register *targetReg = NULL;

   static char* isVectorBCDEnv = feGetEnv("TR_enableVectorBCD");
   if(TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() && !cg->comp()->getOption(TR_DisableVectorBCD) || isVectorBCDEnv)
      {
      targetReg = pdcmpVectorEvaluatorHelper(node, cg);
      }
   else
      {
      targetReg = generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNL, TR::InstOpCode::COND_BNH, false);
      }

   cg->traceBCDExit("pdcmpge",node);
   return targetReg;
   }

TR::Register *J9::Z::TreeEvaluator::pdcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   cg->traceBCDEntry("pdcmpgt",node);
   cg->generateDebugCounter("PD-Op/pdcmpgt", 1, TR::DebugCounter::Cheap);

   TR_ASSERT(node->castedToBCD() == false,"castedToBCD=true not supported for %s (%p)\n",node->getOpCode().getName(),node);
   TR::Register *targetReg = NULL;

   static char* isVectorBCDEnv = feGetEnv("TR_enableVectorBCD");
   if(TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() && !cg->comp()->getOption(TR_DisableVectorBCD) || isVectorBCDEnv)
      {
      targetReg = pdcmpVectorEvaluatorHelper(node, cg);
      }
   else
      {
      targetReg = generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BH, TR::InstOpCode::COND_BL, false);
      }

   cg->traceBCDExit("pdcmpgt",node);
   return targetReg;
   }

TR::Register *J9::Z::TreeEvaluator::pdcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   cg->traceBCDEntry("pdcmple",node);
   cg->generateDebugCounter("PD-Op/pdcmple", 1, TR::DebugCounter::Cheap);

   TR_ASSERT(node->castedToBCD() == false,"castedToBCD=true not supported for %s (%p)\n",node->getOpCode().getName(),node);
   TR::Register *targetReg = NULL;

   static char* isVectorBCDEnv = feGetEnv("TR_enableVectorBCD");
   if(TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() && !cg->comp()->getOption(TR_DisableVectorBCD) || isVectorBCDEnv)
      {
      targetReg = pdcmpVectorEvaluatorHelper(node, cg);
      }
   else
      {

      targetReg = generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, TR::InstOpCode::COND_BNL, false);
      }

   cg->traceBCDExit("pdcmple",node);
   return targetReg;
   }

TR::RegisterDependencyConditions *
setupPackedDFPConversionGPRs(TR::Register *&gpr64, TR::Register *&gpr64Hi,
                             TR::Register *&gpr64Lo, bool isLongDouble, TR::CodeGenerator *cg)
   {
   TR::RegisterDependencyConditions *deps = NULL;
   if (isLongDouble)
      {
      gpr64 = cg->allocateConsecutiveRegisterPair(cg->allocateRegister(), cg->allocateRegister());
      gpr64Hi = gpr64->getHighOrder(); // 0-63
      gpr64Lo = gpr64->getLowOrder();  // 64-127
      }
   else
      {
      gpr64Hi = cg->allocateRegister();
      gpr64 = gpr64Hi;
      }

   if (isLongDouble)
      {
      deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg);
      deps->addPostCondition(gpr64, TR::RealRegister::EvenOddPair);
      deps->addPostCondition(gpr64Hi, TR::RealRegister::LegalEvenOfPair);
      deps->addPostCondition(gpr64Lo, TR::RealRegister::LegalOddOfPair);
      }
   return deps;
   }

/**
 * Handles TR::pd2dd,TR::pd2de
 */
TR::Register *
J9::Z::TreeEvaluator::pd2ddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   cg->traceBCDEntry("pd2dd",node);

   TR_ASSERT(node->getDataType() == TR::DecimalDouble || node->getDataType() == TR::DecimalLongDouble,"expecting op to be pd2dd or pd2de and not %d\n",node->getOpCodeValue());
   TR_ASSERT(node->getDecimalFraction() == 0,"frac should be 0 and not %d\n",node->getDecimalFraction());

   bool isLongDouble = node->getDataType() == TR::DecimalLongDouble;
   bool isAbs = node->getOpCodeValue() == TR::pd2dfAbs || node->getOpCodeValue() == TR::pd2ddAbs || node->getOpCodeValue() == TR::pd2deAbs;
   TR::Node *srcNode = node->getFirstChild();
   TR_PseudoRegister *srcReg = cg->evaluateBCDNode(srcNode);
   TR_StorageReference *srcStorageReference = srcReg->getStorageReference();
   int32_t srcRegSize = srcReg->getSize();
   int32_t srcFixedSize = cg->getPackedToDecimalLongDoubleFixedSize(); // max that can be handled by TR::InstOpCode::CXSTR
   TR_ASSERT(srcRegSize <= srcFixedSize,"srcRegSize %d must be <= %d in pd2ddEvaluator\n",srcRegSize,srcFixedSize);
   TR::Register *fprTargetReg = NULL;
   TR::Compilation *comp = cg->comp();

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"%s %p : srcRegPrec %d, srcRegSize %d  (srcFixedSize = %d, isInit = %s)\n",
         node->getOpCode().getName(),node,srcReg->getDecimalPrecision(),srcRegSize,srcFixedSize,srcReg->isInitialized()?"yes":"no");

   // At arch(11), we have an efficient instruction that converts the packed value, in memory, to the DFP value, in an FP register
   if (cg->supportsFastPackedDFPConversions())
      {
      uint8_t mask = isAbs ? TR_PACKED_TO_DFP_ABS : TR_PACKED_TO_DFP_NOABS;

      bool enforceSSLimits = true;  // RSL has same limits as SS (no index and 12 bit displacement)
      TR::MemoryReference *sourceMR = generateS390LeftAlignedMemoryReference(srcNode, srcStorageReference, cg, srcRegSize, enforceSSLimits);

      fprTargetReg = isLongDouble ? cg->allocateFPRegisterPair() : cg->allocateRegister(TR_FPR);

      int32_t srcPrec = srcReg->getDecimalPrecision();
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tgenerate packed to DFP %s (%p) with op=%s and src packed size %d (prec=%d), mask 0x%x%s\n",
            node->getOpCode().getName(),node,isLongDouble ? "CXPT" : "CDPT",srcRegSize,srcPrec,mask,isAbs?" (abs)":"");

      if (isLongDouble)
         {
         // 1 -> 18 bytes but for 18 bytes only 34 and not 35 digits are allowed (data exception otherwise)
         TR_ASSERT(srcPrec > 0 && srcPrec <= TR_PACKED_TO_DECIMAL_LONG_DOUBLE_PRECISION_ARCH11,"srcPrec %d not in valid CXPT range %d -> %d\n",
            srcPrec,0,TR_PACKED_TO_DECIMAL_LONG_DOUBLE_PRECISION_ARCH11);
         }
      else
         {
         // 1 -> 9 bytes but for 9 bytes only 16 and not 17 digits are allowed (data exception otherwise)
         TR_ASSERT(srcPrec > 0 && srcPrec <= TR_PACKED_TO_DECIMAL_DOUBLE_PRECISION_ARCH11,"srcPrec %d not in valid CDPT range %d -> %d\n",
            srcPrec,0,TR_PACKED_TO_DECIMAL_DOUBLE_PRECISION_ARCH11);
         }

      generateRSLbInstruction(cg, isLongDouble ? TR::InstOpCode::CXPT : TR::InstOpCode::CDPT, node, fprTargetReg, srcRegSize-1, sourceMR, mask);
      }
   else
      {
      // Pre-arch(11): Need to load from memory into a GPR and then convert to DFP in an FPR

      TR::Register *gpr64 = NULL;
      TR::Register *gpr64Hi = NULL;
      TR::Register *gpr64Lo = NULL;

      bool isForcedLongDouble = false;
      if (!isLongDouble && srcRegSize > cg->getPackedToDecimalDoubleFixedSize())
         {
         // There is a mismatch between the max prec supported by CDSTR (15 digits + sign) and the max prec supported in long (8-byte) DFP format (16 digits)
         // For this case use the larger convert instruction CXSTR and round down afterwards
         // CXSTR can convert up to 34 digits but the subsequent round down will lose precision due to rounding for any value with > 16 digits so only 9 bytes (16 or 17 digits)
         // of packed digits will be loaded and if the srcReg precision is > 16 then it will corrected back to a max of 16 digits with the NILL instruction below
         isForcedLongDouble = true;
         isLongDouble = true;
         srcRegSize = 9;
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tsrcRegSize %d  > max of %d for CDSTR : set isLongDouble=true and reduce srcRegSize %d -> %d\n",
               srcRegSize,cg->getPackedToDecimalDoubleFixedSize(),srcRegSize+9,srcRegSize);
         }

      // TODO: examine the leftAlignedZeroDigits on the srcReg to see if  srcRegSize can be widened to the next 2,4,8 byte boundary so the load code below can be simplified
      bool enforceSSLimits = false;
      TR::MemoryReference *sourceMR = generateS390LeftAlignedMemoryReference(srcNode, srcStorageReference, cg, srcRegSize, enforceSSLimits);

      TR::RegisterDependencyConditions *deps = setupPackedDFPConversionGPRs(gpr64, gpr64Hi, gpr64Lo, isLongDouble, cg);
      if (isLongDouble)
         fprTargetReg = cg->allocateFPRegisterPair();
      else
         fprTargetReg = cg->allocateRegister(TR_FPR);

      int32_t partialSize = srcRegSize;
      TR::Register *gpr64Partial = gpr64Hi;
      if (isLongDouble)
         {
         if (srcRegSize > 8)
            partialSize -= 8;
         else
            gpr64Partial = gpr64Lo;
         }

      switch (partialSize)
         {
         case 1:
            generateRXInstruction(cg, TR::InstOpCode::LLGC, srcNode, gpr64Partial, sourceMR);
            break;
         case 2:
            generateRXInstruction(cg, TR::InstOpCode::LLGH, srcNode, gpr64Partial, sourceMR);
            break;
         case 3:
            generateRRInstruction(cg, TR::InstOpCode::XGR, srcNode, gpr64Partial, gpr64Partial);
            generateRSInstruction(cg, TR::InstOpCode::ICM, srcNode, gpr64Partial, (uint32_t) 0x7, sourceMR);
            break;
         case 4:
            generateRXInstruction(cg, TR::InstOpCode::LLGF, srcNode, gpr64Partial, sourceMR);
            break;
         case 5:
         case 6:
         case 7:
            generateRRInstruction(cg, TR::InstOpCode::XGR, srcNode, gpr64Partial, gpr64Partial);
            generateRSInstruction(cg, TR::InstOpCode::ICMH, node, gpr64Partial, (uint32_t)((1 << (partialSize-4)) - 1), sourceMR);  // 5->mask=1, 6->mask=3, 7->mask=7
            generateRXInstruction(cg, TR::InstOpCode::L, srcNode, gpr64Partial, generateS390LeftAlignedMemoryReference(*sourceMR, srcNode, partialSize-4, cg, sourceMR->getLeftMostByte(), enforceSSLimits));
            break;
         case 8:
            generateRXInstruction(cg, TR::InstOpCode::LG, srcNode, gpr64Partial, sourceMR);
            break;
         default:
            TR_ASSERT(false,"unexpected partialSize %d\n",partialSize);
         }

      if (isLongDouble)
         {
         if (srcRegSize > 8)
            generateRXInstruction(cg, TR::InstOpCode::LG, srcNode, gpr64Lo, generateS390LeftAlignedMemoryReference(*sourceMR, srcNode, partialSize, cg, sourceMR->getLeftMostByte(), enforceSSLimits));
         else
            generateRRInstruction(cg, TR::InstOpCode::XGR, srcNode, gpr64Hi, gpr64Hi);
         }

      if (isForcedLongDouble &&
          srcReg->getDecimalPrecision() > TR::DataType::getMaxLongDFPPrecision())
         {
         // 9 bytes have been loaded from source and this many bytes can hold 16 or 17 digits
         // The NILL is to clear the 17th digit so the LDXTR generated later in de2dxHelperAndSetRegister
         // doesn't round incorrectly (there is no 'leftmost' truncation rounding mode for this instructions)
         // if the srcReg->getDecimalPrecision() == TR::DataType::getMaxLongDFPPrecision() (16) then the NILL is not needed.
         TR_ASSERT(srcRegSize == 9,"expecting srcRegSize == 9 and not %d on %s (%p) for the isForcedLongDouble case\n",
            srcRegSize,node->getOpCode().getName(),node);
         generateRIInstruction(cg, TR::InstOpCode::NILL, node, gpr64Hi, 0x000F);
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tgen NILL %s, 0x000F to clear out possible 17th digit from CXSTR in forced longDouble case\n",cg->getDebug()->getName(gpr64Hi));
         }

      generateRRInstruction(cg, isLongDouble ? TR::InstOpCode::CXSTR : TR::InstOpCode::CDSTR, node, fprTargetReg, gpr64, deps);

      // For a pre-arch(11) pd2ddAbs, need to force the positive sign separately from the conversion
      if (isAbs)
         generateRRInstruction(cg, TR::InstOpCode::LPDFR, node, isLongDouble ? fprTargetReg->getHighOrder() : fprTargetReg, isLongDouble ? fprTargetReg->getHighOrder() : fprTargetReg);

      if (isForcedLongDouble)
         fprTargetReg = J9::Z::TreeEvaluator::de2dxHelperAndSetRegister(fprTargetReg, fprTargetReg, node, cg);

      cg->stopUsingRegister(gpr64);
      }

   if (node->getRegister() == NULL) // may already be set by de2ddHelperAndSetRegister
      node->setRegister(fprTargetReg);
   cg->decReferenceCount(srcNode);
   cg->traceBCDExit("pd2dd",node);

   return fprTargetReg;
   }


/**
 * Handles TR::df2pd,TR::dd2pd,TR::de2pd
 */
TR::Register *
J9::Z::TreeEvaluator::df2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   cg->traceBCDEntry("df2pd",node);

   TR_ASSERT(node->getDecimalFraction() == 0,"frac should be 0 and not %d\n",node->getDecimalFraction());

   TR::Compilation *comp = cg->comp();

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"%s %p : resultPrec %d, resultSize %d\n",
         node->getOpCode().getName(),node,node->getDecimalPrecision(),node->getSize());

   bool isFloat      = node->getFirstChild()->getDataType() == TR::DecimalFloat;
   bool isDouble     = node->getFirstChild()->getDataType() == TR::DecimalDouble;
   bool isLongDouble = node->getFirstChild()->getDataType() == TR::DecimalLongDouble;

   TR_ASSERT(isFloat || isDouble || isLongDouble,  "conversion should be short, long or extended\n");

   bool isSetSign = node->isSetSignValueOnNode();
   bool signIsPreferred = true;
   bool isSignNegative = false;


   // Set up the masks for arch(10) and arch(11)
   uint8_t mask = TR_DFP_TO_PACKED_DEFAULT_MASK_ARCH10;
   uint8_t setUnsigned = TR_DFP_TO_PACKED_UNSIGNED_ARCH10;
   int32_t setSignValue = TR::DataType::getInvalidSignCode();

   if (cg->supportsFastPackedDFPConversions())
      {
      mask = TR_DFP_TO_PACKED_DEFAULT_MASK_ARCH11;
      setUnsigned = TR_DFP_TO_PACKED_UNSIGNED_ARCH11;
      }

   if (isSetSign)
      {
      TR_RawBCDSignCode sign = node->getSetSign();
      if (sign == raw_bcd_sign_0xf)
         {
         mask |= setUnsigned;
         setSignValue = TR::DataType::getUnsignedCode();
         signIsPreferred = false;
         if (cg->traceBCDCodeGen()) traceMsg(comp,"\t\tsetSign 0xf: set mask to 0x%x\n",mask);
         TR_ASSERT(TR::DataType::getUnsignedCode() == 0xf,"expecting unsigned code to be 0xf and not 0x%x for node %s (%p)\n",
                   TR::DataType::getUnsignedCode(),node->getOpCode().getName(),node);
         }
      else if (sign == raw_bcd_sign_0xc)
         {
         setSignValue = TR::DataType::getPreferredPlusCode();
         if (cg->traceBCDCodeGen()) traceMsg(comp,"\t\tsetSign 0xc: leave mask at 0x%x\n",mask);
         TR_ASSERT(TR::DataType::getPreferredPlusCode() == 0xc,"expecting preferred plus code to be 0xc and not 0x%x for node %s (%p)\n",
                 TR::DataType::getPreferredPlusCode(),node->getOpCode().getName(),node);
         }
      else if (sign == raw_bcd_sign_0xd)
         {
         setSignValue = TR::DataType::getPreferredMinusCode();
         isSignNegative = true;
         if (cg->traceBCDCodeGen()) traceMsg(comp,"\t\tsetSign 0xd: leave mask at 0x%x\n",mask);
         TR_ASSERT(TR::DataType::getPreferredMinusCode() == 0xd,"expecting preferred minus code to be 0xd and not 0x%x for node %s (%p)\n",
                 TR::DataType::getPreferredMinusCode(),node->getOpCode().getName(),node);
         }
      else
         {
         TR_ASSERT(false,"unsupported sign %d for %s (%p)\n",sign,node->getOpCode().getName(),node);
         }
      }

   int32_t maxBytesFromConv = 0;    // how many digits CSDTR/CSXTR can at most produce
   int32_t maxDigitsInSrc = 0;   // how many significant digits are possible from source dfp
   if (isFloat)
      {
      maxBytesFromConv = cg->getDecimalFloatToPackedFixedSize();  // CSDTR produces 15 digits == 8 bytes; CPDT produces 16 digits == 9 bytes
      maxDigitsInSrc = std::min(node->getSourcePrecision(), TR::DataType::getMaxShortDFPPrecision());    // TR::DataType::getMaxShortDFPPrecision() == 7 digits == 4 bytes
      }
   else if (isDouble)
      {
      maxBytesFromConv = cg->getDecimalDoubleToPackedFixedSize(); // CSDTR produces 15 digits == 8 bytes; CPDT produces 16 digits == 9 bytes
      maxDigitsInSrc = std::min(node->getSourcePrecision(), TR::DataType::getMaxLongDFPPrecision());     // TR::DataType::getMaxLongDFPPrecision() == 16 digits == 9 bytes
      }
   else if (isLongDouble)
      {
      maxBytesFromConv = cg->getDecimalLongDoubleToPackedFixedSize();  // CSXTR produces 31 digits == 16 bytes; CPXT produces 34 digits == 18 bytes
      maxDigitsInSrc = std::min(node->getSourcePrecision(), TR::DataType::getMaxExtendedDFPPrecision()); // TR::DataType::getMaxExtendedDFPPrecision() == 34 digits == 18 bytes
      }
   else
      {
      TR_ASSERT(false, "expecting conversion to be short, long or extended\n");
      }

   TR::Node *srcNode = node->getFirstChild();
   TR::Register *srcFPReg = cg->evaluate(srcNode);
   TR::Register *srcFPRegTemp = NULL;

   bool usesFPRegPair = false;
   bool releaseTempReg = false;

   // Allocate temporary registers for load positive/load negative and/or loading a larger size
   bool canClobber = cg->canClobberNodesRegister(srcNode);
   if (isLongDouble && isSetSign)
      {
      usesFPRegPair = true;
      if (canClobber)
         {
         srcFPRegTemp = srcFPReg;
         }
      else
         {
         srcFPRegTemp = cg->allocateFPRegisterPair();
         releaseTempReg = true;
         }
      }
   else if (isDouble && node->getSize() > maxBytesFromConv)
      {
      // Lengthening; will always need to allocate a register pair
      usesFPRegPair = true;
      releaseTempReg = true;
      srcFPRegTemp = cg->allocateFPRegisterPair();
      }
   else if (isDouble && isSetSign)
      {
      if (canClobber)
         {
         srcFPRegTemp = srcFPReg;
         releaseTempReg = true;
         }
      else
         {
         srcFPRegTemp = cg->allocateRegister(TR_FPR);
         releaseTempReg = true;
         }
      }
   else if (isFloat)
      {
      // Lengthening; will always need to allocate a register
      srcFPRegTemp = cg->allocateRegister(TR_FPR);
      releaseTempReg = true;
      }

   // Lengthen float values before load positive/load negative; those need double-size values
   if (isFloat)
      {
      generateRRFInstruction(cg, TR::InstOpCode::LDETR, node, srcFPRegTemp, srcFPReg, 0, false); // m4=0
      srcFPReg = srcFPRegTemp;
      isFloat = false;
      isDouble = true;
      maxBytesFromConv = cg->getDecimalDoubleToPackedFixedSize();
      }

   if (isSetSign)
      {
      // Generate a load positive or load negative
      TR::InstOpCode::Mnemonic opCode  = TR::InstOpCode::LPDFR;
      if (isSignNegative)
         opCode = TR::InstOpCode::LNDFR;

      if (isLongDouble) // Existing node was long double; lengthening from double to long double is performed later
         {
         generateRRInstruction(cg, opCode, node, srcFPRegTemp->getHighOrder(), srcFPReg->getHighOrder());

         // Only load the lower half if we're not clobbering
         if (!canClobber)
            generateRRInstruction(cg, TR::InstOpCode::LDR, node, srcFPRegTemp->getLowOrder(), srcFPReg->getLowOrder());
         }
      else if (isDouble && usesFPRegPair)
         {
         // Double that will be lengthened; load into the lower half, and lengthen from lower half to full pair later
         generateRRInstruction(cg, opCode, node, srcFPRegTemp->getLowOrder(), srcFPReg);
         }
      else
         {
         generateRRInstruction(cg, opCode, node, srcFPRegTemp, srcFPReg);
         }

      srcFPReg = srcFPRegTemp;
      }

   // If we need to lengthen double values, do so after the load positive/load negative, to avoid
   // generating the LDR unnecessarily
   if (isDouble && node->getSize() > maxBytesFromConv)
      {
      if (isSetSign)
         generateRRFInstruction(cg, TR::InstOpCode::LXDTR, node, srcFPRegTemp, srcFPReg->getLowOrder(), 0, false); // m4=0
      else
         generateRRFInstruction(cg, TR::InstOpCode::LXDTR, node, srcFPRegTemp, srcFPReg, 0, false); // m4=0
      srcFPReg = srcFPRegTemp;
      isDouble = false;
      isLongDouble = true;
      maxBytesFromConv = cg->getDecimalLongDoubleToPackedFixedSize();
      }

   if (releaseTempReg)
      cg->stopUsingRegister(srcFPRegTemp);

   int32_t maxDigitsFromConv = TR::DataType::byteLengthToPackedDecimalPrecisionCeiling(maxBytesFromConv);

   TR_ASSERT(isDouble || isLongDouble,"only long and extended types should now remain\n");


   if (cg->supportsFastPackedDFPConversions())
      {
      TR_PseudoRegister *targetReg = cg->allocatePseudoRegister(node->getDataType());
      TR_StorageReference *hint = node->getStorageReferenceHint();
      TR_StorageReference *targetStorageReference = NULL;
      int32_t memSize = 0; // Amount of memory allocated for the destination value
      int32_t regSize = maxBytesFromConv; // Minimal amount of memory needed to hold the converted value
      targetReg->setDecimalPrecision(node->getDecimalPrecision());

      if (node->hasSourcePrecision() && node->getSourcePrecision() <= TR::DataType::getMaxExtendedDFPPrecision())
         {
         // CPDT can be used to truncate, but it will cause an overflow error if it does. If overflow
         // is suppressed, as it is on COBOL, we can use the lower precision anyway; otherwise we need
         // to store the zoned value in a temp and use MVC to truncate.

         memSize = std::max(targetReg->getSize(), TR::DataType::packedDecimalPrecisionToByteLength(node->getSourcePrecision()));
         }
      else
         {
         // If source precision is unknown, assume the maximum possible value to avoid overflow errors
         if (isFloat || isDouble)
            memSize = TR::DataType::packedDecimalPrecisionToByteLength(TR::DataType::getMaxLongDFPPrecision());
         else
            memSize = TR::DataType::packedDecimalPrecisionToByteLength(TR::DataType::getMaxExtendedDFPPrecision());
         }

      if (hint)
         {
         // If there is no truncation, use the hint size to possibly widen here for free, rather than on a store.
         // If there is a truncation, we can't use the hint size without producing an incorrect result.
         // pdstore p=8      pdstore p = 8   pdstore p=4
         //   dd2pd p=7        dd2pd p = 4     dd2pd p=5
         //     ddX p=7          ddX p = 7       ddX p=7
         // For case 1, using the hint size is good. For case 2, it's incorrect, as we'd lose the truncation to 4 digits.
         // In case 3, using the hint size is fine, but there shouldn't be a hint generated since the p=5 dd2pd result can't fit
         // in the p=4 pdstore field
         if (node->hasSourcePrecision() && !node->isTruncating())
            {
            // Don't widen if doing so would require a conversion from double to long double
            if (!isDouble || (hint->getSymbolSize() <= TR::DataType::packedDecimalPrecisionToByteLength(TR::DataType::getMaxLongDFPPrecision())))
                {
               // Widen up to 32 digits (33 and 34 are slower)
               if (regSize <= TR::DataType::packedDecimalPrecisionToByteLength(TR_MAX_DFP_PACKED_FAST_SIZE) && hint->getSymbolSize() >= TR::DataType::packedDecimalPrecisionToByteLength(TR_MAX_DFP_PACKED_FAST_SIZE))
                  regSize = TR::DataType::packedDecimalPrecisionToByteLength(TR_MAX_DFP_PACKED_FAST_SIZE);
               else
                  regSize = hint->getSymbolSize(); // Widen up to the symbol size

               // Enlarge the memory required if it's now smaller than the precision we'll set on the target register
               if (regSize > memSize)
                  memSize = regSize;
               if (cg->traceBCDCodeGen())
                  traceMsg(comp,"\tset memSize = %d and regSize = %d based on hint #%d\n",memSize,regSize,hint->getReferenceNumber());
                }
            }
         targetStorageReference = hint;
         }
      else
         {
         targetStorageReference = TR_StorageReference::createTemporaryBasedStorageReference(memSize, comp);
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tset memSize = %d based on node size\n",memSize);
         }

      // regardless of how the memSize and targetPrec were set up earlier ensure they do not exceed the limits for the CPDT or CPXT instructions
      // and assert below if these max instruction settings are >= the max # of significant digits that can be represented for each DFP type
      int32_t maxConvertedPrecision = 0;
      if (isFloat)
         {
         TR_ASSERT(TR_DECIMAL_FLOAT_TO_PACKED_PRECISION_ARCH11 >= TR::DataType::getMaxShortDFPPrecision(),"CPDT max %d must be >= DFP short max %d\n",
            TR_DECIMAL_FLOAT_TO_PACKED_PRECISION_ARCH11, TR::DataType::getMaxShortDFPPrecision());
         memSize = std::min(memSize, TR_DECIMAL_FLOAT_TO_PACKED_SIZE_ARCH11);   // cannot get more than 9 bytes (16 digits) from the DFP source (and larger encodings are illegal)
         maxConvertedPrecision = TR_DECIMAL_FLOAT_TO_PACKED_PRECISION_ARCH11;
         }
      else if (isDouble)
         {
         TR_ASSERT(TR_DECIMAL_DOUBLE_TO_PACKED_PRECISION_ARCH11 >= TR::DataType::getMaxLongDFPPrecision(),"CPDT max %d must be >= DFP long max %d\n",
            TR_DECIMAL_DOUBLE_TO_PACKED_PRECISION_ARCH11, TR::DataType::getMaxLongDFPPrecision());
         memSize = std::min(memSize, TR_DECIMAL_DOUBLE_TO_PACKED_SIZE_ARCH11);   // cannot get more than 9 bytes (16 digits) from the DFP source (and larger encodings are illegal)
         maxConvertedPrecision = TR_DECIMAL_DOUBLE_TO_PACKED_PRECISION_ARCH11;
         }
      else
         {
         TR_ASSERT(TR_DECIMAL_LONG_DOUBLE_TO_PACKED_PRECISION_ARCH11 >= TR::DataType::getMaxExtendedDFPPrecision(),"CPXT max %d must be >= DFP long double max %d\n",
            TR_DECIMAL_LONG_DOUBLE_TO_PACKED_PRECISION_ARCH11, TR::DataType::getMaxExtendedDFPPrecision());
         memSize = std::min(memSize, TR_DECIMAL_LONG_DOUBLE_TO_PACKED_SIZE_ARCH11);   // cannot get more than 18 bytes (34 digits) from the DFP source (and larger encodings are illegal)
         maxConvertedPrecision = TR_DECIMAL_LONG_DOUBLE_TO_PACKED_PRECISION_ARCH11;
         }

      if (targetReg->getDecimalPrecision() > maxConvertedPrecision)
         {
         // since the finalPrecision is the max # of digits possibly converted then cannot set a higher targetPrec as these upper digits would be uninitialized
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\ttargetRegPrec %d > maxConvertedPrecision %d : reduce targetPrec to %d\n",targetReg->getDecimalPrecision(),maxConvertedPrecision,maxConvertedPrecision);
         targetReg->setDecimalPrecision(maxConvertedPrecision);
         }

      targetReg->setStorageReference(targetStorageReference, node);

      bool enforceSSLimits = true;  // RSL has same limits as SS (no index and 12 bit displacement)
      TR::MemoryReference *destMR = generateS390LeftAlignedMemoryReference(node, targetStorageReference, cg, memSize, enforceSSLimits);

      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tgenerate DFP to packed %s (%p) with op=%s and target packed size %d, mask 0x%x (signIsPreferred %s)\n",
                 node->getOpCode().getName(),node,isLongDouble ? "TR::InstOpCode::CPXT" : "TR::InstOpCode::CPDT",memSize,mask,signIsPreferred?"yes":"no");

      generateRSLbInstruction(cg, isLongDouble ? TR::InstOpCode::CPXT : TR::InstOpCode::CPDT, node, srcFPReg, memSize-1, destMR, mask);

      bool evenDigitNeedsClearing = !node->canSkipPadByteClearing() && isEven(node->getDecimalPrecision()) && node->isTruncating();
      // Clear the top nibble if we're truncating and have an even precision
      if (evenDigitNeedsClearing)
         generateSIInstruction(cg, TR::InstOpCode::NI, node, destMR = generateS390LeftAlignedMemoryReference(*destMR, node, 0, cg, memSize, enforceSSLimits), 0x0F);

      if (isSetSign)
         {
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tisSetSign = true : set setKnownSignCode to 0x%x on %s\n",setSignValue,cg->getDebug()->getName(targetReg));
         targetReg->setKnownSignCode(setSignValue);
         }
      else
         {
         if (srcNode->isNonNegative())
            {
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\tsrcNode %s (%p) isNonNegative = true : set setKnownSignCode to 0x%x on %s\n",
                  srcNode->getOpCode().getName(), srcNode, TR::DataType::getPreferredPlusCode(), cg->getDebug()->getName(targetReg));
            targetReg->setKnownSignCode(TR::DataType::getPreferredPlusCode());
            }
         else if (!evenDigitNeedsClearing)
            {
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\tisClean op = true (always clean when using %s) : setHasKnownCleanSign on %s\n",isLongDouble ? "TR::InstOpCode::CPXT" : "TR::InstOpCode::CPDT",cg->getDebug()->getName(targetReg));
            targetReg->setHasKnownCleanSign();
            }
         else if (signIsPreferred)
            {
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\tsignIsPreferred = true : setHasKnownPreferredSign on %s\n",cg->getDebug()->getName(targetReg));
            targetReg->setHasKnownPreferredSign();
            }
         }

      if (cg->traceBCDCodeGen())
         {
         traceMsg(comp,"\ttargetReg %s signState : cleanSign=%s, preferredSign=%s, knownSign=%s (0x%x)\n",
                 cg->getDebug()->getName(targetReg),
                 targetReg->hasKnownCleanSign() ? "yes" : "no", targetReg->hasKnownPreferredSign() ? "yes" : "no",
                 targetReg->hasKnownSignCode() ? "yes" : "no", targetReg->hasKnownSignCode() ? targetReg->getKnownSignCode() : TR::DataType::getInvalidSignCode());
         }

      targetReg->setIsInitialized();
      targetReg->setHasKnownValidSignAndData();

      node->setRegister(targetReg);
      cg->decReferenceCount(srcNode);
      cg->traceBCDExit("df2pd",node);

      return targetReg;
      }
   else
      {
      TR::Register *gpr64 = NULL;
      TR::Register *gpr64Hi = NULL;
      TR::Register *gpr64Lo = NULL;

      TR::RegisterDependencyConditions *deps = setupPackedDFPConversionGPRs(gpr64, gpr64Hi, gpr64Lo, isLongDouble, cg);

      TR_PseudoRegister *targetReg = cg->allocatePseudoRegister(node->getDataType());

      bool isTruncation = maxDigitsInSrc > node->getDecimalPrecision();
      int32_t targetPrec = std::min<int32_t>(maxDigitsInSrc, node->getDecimalPrecision());
      targetReg->setDecimalPrecision(targetPrec);

      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tset targetRegPrec to min(maxDigitsInSrc,nodePrec) = min(%d,%d) = %d (isTrunc = %s)\n",maxDigitsInSrc,node->getDecimalPrecision(),targetPrec,isTruncation?"yes":"no");

      TR_StorageReference *hint = node->getStorageReferenceHint();
      TR_StorageReference *targetStorageReference = NULL;
      if (hint)
         targetStorageReference = hint;
      else
         targetStorageReference = TR_StorageReference::createTemporaryBasedStorageReference(targetReg->getSize(), comp);
      targetReg->setStorageReference(targetStorageReference, node);

      bool enforceSSLimits = false; // RX/RS form inst are being used so Y-form is available for these
      TR::MemoryReference *destMR = generateS390LeftAlignedMemoryReference(node, targetStorageReference, cg, targetReg->getSize(), enforceSSLimits);

      int32_t remainingTargetDigits = targetPrec;
      if (targetPrec > maxDigitsFromConv)
         {
         int32_t highDigits = targetPrec - maxDigitsFromConv;
         remainingTargetDigits -= highDigits;
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\ttargetPrec %d > maxDigitsFromConv %d : first convert high %d digit(s)\n",targetPrec,maxDigitsFromConv,targetPrec-maxDigitsFromConv);
         // float cases have max targetPrec of 7 and a min maxDigitsFromConv of 15 so can never reach here
         // double case have max targetPrec of 16 and a maxDigitsFromConv of 15 (but only when targetPrec is <=15) or a maxDigitsFromConv of 31 when up-converted to longDouble
         TR_ASSERT(isLongDouble, "only longDouble cases require extra conversion on node %s (%p)\n",node->getOpCode().getName(),node);
         TR::Register *srcFPRegTemp = cg->allocateFPRegisterPair();
         TR::MemoryReference *shiftRef = generateS390MemoryReference(maxDigitsFromConv, cg);
         generateRXFInstruction(cg, TR::InstOpCode::SRXT, node, srcFPRegTemp, srcFPReg, shiftRef);  // shift away low digits
         generateRRInstruction(cg, TR::InstOpCode::CSXTR, node, gpr64, srcFPRegTemp, deps);         // convert high digits to packed
         generateRSInstruction(cg, TR::InstOpCode::SRL, node, gpr64Lo, gpr64Lo, 4);                 // get rid of packed sign code
         cg->stopUsingRegister(srcFPRegTemp);
         int32_t offsetBump = 0;
         switch (highDigits)
            {
            case 1:
            case 2:
               generateRXInstruction(cg, TR::InstOpCode::STC, node, gpr64Lo, destMR);
               offsetBump = 1;
               break;
            case 3:
               generateRXInstruction(cg, TR::InstOpCode::STH, node, gpr64Lo, destMR);
               offsetBump = 2;
               break;
            default:
               TR_ASSERT(false, "unexpected highDigits %d on node %s (%p)\n",highDigits,node->getOpCode().getName(),node);
            }
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tadd offsetBump = %d to destMR\n",offsetBump);
         destMR = generateS390LeftAlignedMemoryReference(*destMR, node, offsetBump, cg, targetReg->getSize());
         }

      TR::InstOpCode::Mnemonic convOp = isDouble ? TR::InstOpCode::CSDTR : TR::InstOpCode::CSXTR;
      generateRRFInstruction(cg, convOp, node, gpr64, srcFPReg, mask, false, deps);

      int32_t remainingTargetBytes = TR::DataType::packedDecimalPrecisionToByteLength(remainingTargetDigits);
      int32_t partialSize = std::min(maxBytesFromConv, remainingTargetBytes);
      TR::Register *gpr64Partial = gpr64Hi;
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tpartialSize = min(maxBytesFromConv, remainingTargetBytes) = min(%d, %d) = %d\n",maxBytesFromConv,remainingTargetBytes,partialSize);
      if (isLongDouble)
         {
         if (partialSize > 8)
            {
            partialSize -= 8;
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\tisExt=true and partialSize %d > 8 so reduce partialSize by 8 to %d\n",partialSize+8,partialSize);
            }
         else
            {
            gpr64Partial = gpr64Lo;
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\tisExt=true and partialSize %d <= 8 so store from the low gpr64 only\n",partialSize);
            }
         }

      bool needsClear = false;
      if (!node->canSkipPadByteClearing() && isEven(remainingTargetDigits) && isTruncation)
         {
         needsClear = true;
         TR::InstOpCode::Mnemonic clearOp = TR::InstOpCode::BAD;
         int32_t mask = 0;
         switch (partialSize)
            {
            case 1:
               TR_ASSERT(remainingTargetBytes > 1,"an even precision %d size 1 register is not valid\n",remainingTargetDigits);
               clearOp = TR::InstOpCode::NILL;
               mask = 0x000F;
               break;
            case 2:  // prec=2
               clearOp = TR::InstOpCode::NILL;
               mask = 0x0FFF;
               break;
            case 3:  // prec=4
               clearOp = TR::InstOpCode::NILH;
               mask = 0x000F;
               break;
            case 4:  // prec=6
               clearOp = TR::InstOpCode::NILH;
               mask = 0x0FFF;
               break;
            case 5:  // prec=8
               clearOp = TR::InstOpCode::NIHL;
               mask = 0x000F;
               break;
            case 6:  // prec=10
               clearOp = TR::InstOpCode::NIHL;
               mask = 0x0FFF;
               break;
            case 7:  // prec=12
               clearOp = TR::InstOpCode::NIHH;
               mask = 0x000F;
               break;
            case 8:  // prec=14
               clearOp = TR::InstOpCode::NIHH;
               mask = 0x0FFF;
               break;
            default:
               break;
               // will just use an NI clear in packed in this case
            }
         if (clearOp != TR::InstOpCode::BAD)
            {
            needsClear = false;
            generateRIInstruction(cg, clearOp, node, gpr64Partial, mask);
            if (cg->traceBCDCodeGen()) traceMsg(comp,"\tisTruncation=true and remainingTargetDigits isEven (%d) so perform regBased clear of top nibble with NIxx\n",remainingTargetDigits);
            }
         }

      switch (partialSize)
         {
         case 1:
            generateRXInstruction(cg, TR::InstOpCode::STC, node, gpr64Partial, destMR);
            break;
         case 2:
            generateRXInstruction(cg, TR::InstOpCode::STH, node, gpr64Partial, destMR);
            break;
         case 3:
            generateRSInstruction(cg, TR::InstOpCode::STCM, node, gpr64Partial, (uint32_t)0x7, destMR);
            break;
         case 4:
            generateRXInstruction(cg, TR::InstOpCode::ST, node, gpr64Partial, destMR);
            break;
         case 5:
         case 6:
         case 7:
            generateRSInstruction(cg, TR::InstOpCode::STCMH, node, gpr64Partial, (uint32_t)((1 << (partialSize-4)) - 1), destMR);
            generateRXInstruction(cg, TR::InstOpCode::ST, node, gpr64Partial, generateS390LeftAlignedMemoryReference(*destMR, node, partialSize-4, cg, destMR->getLeftMostByte(), enforceSSLimits));
            break;
         case 8:
            generateRXInstruction(cg, TR::InstOpCode::STG, node, gpr64Partial, destMR);
            break;
         default:
            TR_ASSERT(false,"unexpected partialSize %d\n", partialSize);
         }

      if (isLongDouble && remainingTargetBytes > 8)
         generateRXInstruction(cg, TR::InstOpCode::STG, node, gpr64Lo, generateS390LeftAlignedMemoryReference(*destMR, node, partialSize, cg, destMR->getLeftMostByte(), enforceSSLimits));

      if (deps)
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, generateLabelSymbol(cg), deps);

      if (needsClear)
         {
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tisTruncation=true and targetReg->isEvenPrec (%d) so perform memBased clear of top nibble with NI\n",targetReg->getDecimalPrecision());
         cg->genZeroLeftMostPackedDigits(node, targetReg, remainingTargetBytes, 1, destMR);
         }

      targetReg->setIsInitialized();
      targetReg->setHasKnownValidSignAndData();

      if (isSetSign)
         {
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tisSetSign = true : set setKnownSignCode to 0x%x on %s\n",setSignValue,cg->getDebug()->getName(targetReg));
         targetReg->setKnownSignCode(setSignValue);
         }
      else
         {
         if (srcNode->isNonNegative())
            {
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\tsrcNode %s (%p) isNonNegative = true : set setKnownSignCode to 0x%x on %s\n",
                  srcNode->getOpCode().getName(), srcNode, TR::DataType::getPreferredPlusCode(), cg->getDebug()->getName(targetReg));
            targetReg->setKnownSignCode(TR::DataType::getPreferredPlusCode());
            }
         else if (node->hasKnownCleanSign()) // perhaps a ddclean/declean has been inserted below in IL
            {
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\t%s (%p) hasKnownCleanSign() = true : setHasKnownCleanSign on %s\n",node->getOpCode().getName(),node,cg->getDebug()->getName(targetReg));
            targetReg->setHasKnownCleanSign();
            }
         else if (!isTruncation &&
                  srcNode->getOpCode().isSub() &&
                  srcNode->getFirstChild()->isNonNegative() &&
                  srcNode->getSecondChild()->isNonNegative())
            {
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\t%s (%p) srcNode %s (%p) has two nonNegative children : setHasKnownCleanSign on %s\n",
                  node->getOpCode().getName(),node,
                  srcNode->getOpCode().getName(),srcNode,
                  cg->getDebug()->getName(targetReg));
            targetReg->setHasKnownCleanSign();
            }
         else if (signIsPreferred)
            {
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\tsignIsPreferred = true : setHasKnownPreferredSign to 0x%x on %s\n",cg->getDebug()->getName(targetReg));
            targetReg->setHasKnownPreferredSign();
            }
         }

      if (cg->traceBCDCodeGen())
         {
         traceMsg(comp,"\ttargetReg %s signState : cleanSign=%s, preferredSign=%s, knownSign=%s (0x%x)\n",
                 cg->getDebug()->getName(targetReg),
                 targetReg->hasKnownCleanSign() ? "yes" : "no", targetReg->hasKnownPreferredSign() ? "yes" : "no",
                 targetReg->hasKnownSignCode() ? "yes" : "no", targetReg->hasKnownSignCode() ? targetReg->getKnownSignCode() : TR::DataType::getInvalidSignCode());
         }

      cg->stopUsingRegister(gpr64);
      node->setRegister(targetReg);
      cg->decReferenceCount(srcNode);
      cg->traceBCDExit("df2pd",node);
      return targetReg;
      }
   }




















TR::Register *
J9::Z::TreeEvaluator::pd2iEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   cg->traceBCDEntry("pd2i",node);
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(cg->comp(), "PD-Op/%s", node->getOpCode().getName()),
                            1, TR::DebugCounter::Cheap);
   TR::Register * reg = NULL;

   static char* isVectorBCDEnv = feGetEnv("TR_enableVectorBCD");
   if(TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() && !cg->comp()->getOption(TR_DisableVectorBCD) || isVectorBCDEnv)
      {
      reg = generateVectorPackedToBinaryConversion(node, TR::InstOpCode::VCVB, cg);
      }
   else
      {
      reg = generatePackedToBinaryConversion(node, TR::InstOpCode::CVB, cg);
      }

   cg->traceBCDExit("pd2i",node);
   return reg;
   }

TR::Register *
J9::Z::TreeEvaluator::pd2lEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   cg->traceBCDEntry("pd2l",node);
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(cg->comp(), "PD-Op/%s", node->getOpCode().getName()),
                            1, TR::DebugCounter::Cheap);
   TR::Register * reg = NULL;

   static char* isVectorBCDEnv = feGetEnv("TR_enableVectorBCD");
   if(TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() && !cg->comp()->getOption(TR_DisableVectorBCD) || isVectorBCDEnv)
      {
      reg = generateVectorPackedToBinaryConversion(node, TR::InstOpCode::VCVBG, cg);
      }
   else
      {
      reg = generatePackedToBinaryConversion(node, TR::InstOpCode::CVBG, cg);
      }

   cg->traceBCDExit("pd2l",node);
   return reg;
   }

TR::Register*
J9::Z::TreeEvaluator::pd2lVariableEvaluator(TR::Node* node, TR::CodeGenerator* cg, bool isUseVectorBCD)
   {
   cg->traceBCDEntry("pd2lVariableEvaluator",node);
   cg->generateDebugCounter("PD-Op/pd2l-var", 1, TR::DebugCounter::Cheap);

   TR::Node* pdOpNode      = node->getChild(0);
   TR::Node* pdAddressNode = node->getChild(1);

   TR::Compilation *comp = cg->comp();

   // This function handles PD2I and PD2L
   bool PD2I = pdOpNode->getOpCode().getOpCodeValue() == TR::icall;

   TR::Register* returnReg = cg->allocateRegister();

   TR::InstOpCode::Mnemonic conversionOp = PD2I ? TR::InstOpCode::VCVB : TR::InstOpCode::VCVBG;

   TR::Register* callAddrReg = cg->evaluate(pdAddressNode);
   TR::Register* precisionReg = cg->evaluate(pdOpNode->getChild(2));
   TR::Register* lengthReg = cg->allocateRegister();
   TR_ASSERT(precisionReg && (precisionReg->getKind() == TR_GPR), "precision should be a 32bit GPR");

   // byteLength = precision/2 + 1. Note that the length codes of all instructions are (byteLength-1).
   // Thus, lengthCode = precision/2
   if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z196))
      {
      generateRSInstruction(cg, TR::InstOpCode::SRAK, pdOpNode, lengthReg, precisionReg, 0x1, NULL);
      }
   else
      {
      generateRRInstruction(cg, TR::InstOpCode::LR, pdOpNode, lengthReg, precisionReg);
      generateRSInstruction(cg, TR::InstOpCode::SRA, pdOpNode, lengthReg, 0x1);
      }

   TR::MemoryReference* sourceMR = generateS390MemoryReference(callAddrReg, 0, cg);
   static bool disableTPBeforePD2I = feGetEnv("TR_DisableTPBeforePD2I") != NULL;

   if (isUseVectorBCD)
      {
      // variable length load + vector convert to binary
      TR::Register* vPDReg = cg->allocateRegister(TR_VRF);
      generateVRSdInstruction(cg, TR::InstOpCode::VLRLR, node, lengthReg, vPDReg, sourceMR);

      if (!disableTPBeforePD2I)
         {
         generateVRRgInstruction(cg, TR::InstOpCode::VTP, node, vPDReg);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC,
                                       TR::InstOpCode::COND_MASK7,
                                       node, cg->getCurrentBCDCHKHandlerLabel());
         }

      uint8_t ignoreOverflowMask = 0;

      if (TR::Compiler->target.cpu.getSupportsVectorPackedDecimalEnhancementFacility() && cg->getIgnoreDecimalOverflowException())
         {
         ignoreOverflowMask = 0x8;
         }

      generateVRRiInstruction(cg, conversionOp, node, returnReg, vPDReg, 1, ignoreOverflowMask);
      cg->stopUsingRegister(vPDReg);
      }
   else
      {
      const uint32_t tempSRSize = PD2I ? cg->getPackedToIntegerFixedSize()
                                       : cg->getPackedToLongFixedSize();

      // Allocate space on the stack for the PD to be copied to
      TR_StorageReference* tempSR = TR_StorageReference::createTemporaryBasedStorageReference(tempSRSize, comp);

      tempSR->setTemporaryReferenceCount(1);

      TR::MemoryReference* ZAPtargetMR = generateS390MemRefFromStorageRef(node, tempSR, cg, false, true);
      TR::Register* zapTargetBaseReg = cg->allocateRegister();
      /*
       * Insert an intermediate LA instruction before the ZAP+EX sequence to hold the ZAP target base address
       * value. Intermediate LA instructions are needed for all instructions targeted by EX (or EXRL) and have
       * memory references with unmaterialized base/index registers. This is done so that we are immune to
       * large displacement instruction adjustments.
       *
       * In this particular case, the instruction selection phase emits ZAP+EX. The peephole optimization later
       * replaces the EX with an EXRL and expands to three instructions:
       *
       * BRC [to EXRl]
       * ZAP
       * EXRL [of ZAP]
       *
       * These three instructions work fine if they are all together. If the ZAP is targeting a memory location that's
       * far away down the stack, large displacement instructions will be added in the memory reference binary encoding phase
       * to create the following functionally incorrect instruction sequence:
       *
       * BRC [to EXRL]
       * STG
       * LGHI
       * LA
       * ZAP
       * LG
       * EXRL
       *
       *
       * Having an intermediate LA instruction here prevents the large displacement adjustments on the ZAP instruction and holds
       * the BRC+ZAP+EXRL instructions together.
      */
      generateRXInstruction(cg, TR::InstOpCode::LA, node, zapTargetBaseReg, ZAPtargetMR);

      if (!disableTPBeforePD2I)
         {
         TR::Register* tempLengthForTP = cg->allocateRegister();

         if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z196))
            {
            generateRSInstruction(cg, TR::InstOpCode::SLAK, node, tempLengthForTP, lengthReg, 4);
            }
         else
            {
            generateRRInstruction(cg, TR::InstOpCode::LR, node, tempLengthForTP, lengthReg);
            generateRSInstruction(cg, TR::InstOpCode::SLA, node, tempLengthForTP, 4);
            }

         auto* testPackedInstruction = generateRSLInstruction(cg, TR::InstOpCode::TP, node, 0, generateS390MemoryReference(*sourceMR, 0, cg));

         generateEXDispatch(node, cg, tempLengthForTP, testPackedInstruction);

         // Fallback to the OOL path if anything is wrong with the input packed decimal
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK7, node, cg->getCurrentBCDCHKHandlerLabel());

         cg->stopUsingRegister(tempLengthForTP);
         }

      TR::Instruction* instrZAP = generateSS2Instruction(cg, TR::InstOpCode::ZAP, node,
                                                         tempSRSize - 1,
                                                         generateS390MemoryReference(zapTargetBaseReg, 0, cg),
                                                         0, sourceMR);

      generateEXDispatch(node, cg, lengthReg, instrZAP);

      if (PD2I)
         {
         generateRXInstruction (cg, TR::InstOpCode::CVB, node, returnReg, generateS390MemoryReference(*ZAPtargetMR, 0, cg));
         }
      else
         {
         generateRXInstruction(cg, TR::InstOpCode::CVBG, node, returnReg, generateS390MemoryReference(*ZAPtargetMR, 0, cg));
         }

      tempSR->setTemporaryReferenceCount(0);
      cg->stopUsingRegister(zapTargetBaseReg);
      }

   cg->decReferenceCount(pdAddressNode);
   cg->stopUsingRegister(lengthReg);
   pdOpNode->setRegister(returnReg);

   // Create a debug counter to track how often we execute the inline path for variable operations
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(cg->comp(),
                                                               "DAA/variable/inline/(%s)/%p",
                                                               cg->comp()->signature(), node),
                            1, TR::DebugCounter::Undetermined);

   cg->traceBCDExit("pd2lVariableEvaluator",node);

   return returnReg;
   }

TR::Register *
J9::Z::TreeEvaluator::generateVectorPackedToBinaryConversion(TR::Node * node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator * cg)
   {
   TR_ASSERT( op == TR::InstOpCode::VCVB || op == TR::InstOpCode::VCVBG,"unexpected opcode in gen vector pd2i\n");
   bool isPDToLong = (op == TR::InstOpCode::VCVBG);

   TR::Register *rResultReg = (isPDToLong) ? cg->allocateRegister() : cg->allocateRegister();

   // evaluate pdload
   TR::Node *pdValueNode = node->getFirstChild();
   TR::Register *vPdValueReg = cg->evaluate(pdValueNode);
   TR_ASSERT(vPdValueReg->getKind() == TR_VRF || vPdValueReg->getKind() == TR_FPR, "Vector register expected.");

   static bool disableTPBeforePD2I = feGetEnv("TR_DisableTPBeforePD2I") != NULL;
   if (!disableTPBeforePD2I)
      {
      generateVRRgInstruction(cg, TR::InstOpCode::VTP, node, vPdValueReg);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC,
                                    TR::InstOpCode::COND_MASK7, node,
                                    cg->getCurrentBCDCHKHandlerLabel());
      }

   uint8_t ignoreOverflowMask = 0;

   if (TR::Compiler->target.cpu.getSupportsVectorPackedDecimalEnhancementFacility() && cg->getIgnoreDecimalOverflowException())
      {
      ignoreOverflowMask = 0x8;
      }

   // Convert to signed binary of either 32-bit or 64-bit long
   generateVRRiInstruction(cg, op, node, rResultReg, vPdValueReg, 0x1, ignoreOverflowMask);

   cg->decReferenceCount(pdValueNode);
   node->setRegister(rResultReg);
   return rResultReg;
   }

TR::Register *
J9::Z::TreeEvaluator::generatePackedToBinaryConversion(TR::Node * node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator * cg)
   {
   TR_ASSERT( op == TR::InstOpCode::CVB || op == TR::InstOpCode::CVBG,"unexpected opcode in generatePackedToBinaryFixedConversion\n");
   TR::Register *targetReg = cg->allocateRegister();

   TR::Node *firstChild = node->getFirstChild();
   TR_PseudoRegister *firstReg = cg->evaluateBCDNode(firstChild);
   int32_t requiredSourceSize = op == TR::InstOpCode::CVB ? cg->getPackedToIntegerFixedSize() : cg->getPackedToLongFixedSize();
   TR::MemoryReference *sourceMR = cg->materializeFullBCDValue(firstChild,
                                                                          firstReg,
                                                                          requiredSourceSize,
                                                                          requiredSourceSize,
                                                                          false, // updateStorageReference
                                                                          false); // alwaysEnforceSSLimits -- to be used in CVB

   TR_StorageReference *firstStorageReference = firstReg->getStorageReference();
   sourceMR = reuseS390LeftAlignedMemoryReference(sourceMR, firstChild, firstStorageReference, cg, requiredSourceSize, false); // enforceSSLimits=false for CVB

   static bool disableTPBeforePD2I = feGetEnv("TR_DisableTPBeforePD2I") != NULL;

   if (!disableTPBeforePD2I)
      {
      generateRSLInstruction(cg, TR::InstOpCode::TP, node, firstReg->getSize() - 1, generateS390RightAlignedMemoryReference(*sourceMR, firstChild, 0, cg, false));
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK7, node, cg->getCurrentBCDCHKHandlerLabel());
      }

   TR::Instruction *inst = NULL;
   if (op == TR::InstOpCode::CVB)
      inst = generateRXInstruction(cg, op, node, targetReg, sourceMR);
   else
      inst = generateRXInstruction(cg, op, node, targetReg, sourceMR);

   if (sourceMR->getStorageReference() == firstStorageReference)
      firstReg->setHasKnownValidSignAndData();

   // Create a debug counter to track how often we execute the inline path
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(cg->comp(), "DAA/inline/(%s)/%p", cg->comp()->signature(), node), 1, TR::DebugCounter::Undetermined);

   cg->decReferenceCount(firstChild);
   node->setRegister(targetReg);
   return targetReg;
   }

TR::Register *
J9::Z::TreeEvaluator::i2pdEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   cg->traceBCDEntry("i2pd",node);
   cg->generateDebugCounter("PD-Op/i2pd", 1, TR::DebugCounter::Cheap);
   TR::Register * reg = NULL;

   static char* isVectorBCDEnv = feGetEnv("TR_enableVectorBCD");
   if(TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() && !cg->comp()->getOption(TR_DisableVectorBCD) || isVectorBCDEnv)
      {
      reg = generateVectorBinaryToPackedConversion(node, TR::InstOpCode::VCVD, cg);
      }
   else
      {
      reg = generateBinaryToPackedConversion(node, TR::InstOpCode::CVD, cg);
      }

   cg->traceBCDExit("i2pd",node);
   return reg;
   }

TR::Register *
J9::Z::TreeEvaluator::l2pdEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   cg->traceBCDEntry("l2pd",node);
   cg->generateDebugCounter("PD-Op/l2pd", 1, TR::DebugCounter::Cheap);
   TR::Register * reg = NULL;

   static char* isVectorBCDEnv = feGetEnv("TR_enableVectorBCD");
   if(TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() && !cg->comp()->getOption(TR_DisableVectorBCD) || isVectorBCDEnv)
      {
      reg = generateVectorBinaryToPackedConversion(node, TR::InstOpCode::VCVDG, cg);
      }
   else
      {
      reg = generateBinaryToPackedConversion(node, TR::InstOpCode::CVDG, cg);
      }

   cg->traceBCDExit("l2pd",node);
   return reg;
   }

/**
 * \brief This evaluator helper function evaluates i2pd and l2pd conversion nodes
 * using CVD or CVDG instructions.
 *
*/
TR::Register *
J9::Z::TreeEvaluator::generateBinaryToPackedConversion(TR::Node * node,
                                                       TR::InstOpCode::Mnemonic op,
                                                       TR::CodeGenerator * cg)
   {
   TR_ASSERT( op == TR::InstOpCode::CVD || op == TR::InstOpCode::CVDG,
              "unexpected opcode in generateBinaryToPackedConversion\n");

   TR_PseudoRegister *targetReg = cg->allocatePseudoRegister(node->getDataType());
   TR::Compilation *comp = cg->comp();
   bool isI2PD = op == TR::InstOpCode::CVD;
   TR_StorageReference *hint = node->getStorageReferenceHint();
   int32_t cvdSize = isI2PD ? cg->getIntegerToPackedFixedSize() : cg->getLongToPackedFixedSize();
   TR_StorageReference *targetStorageReference = hint ? hint : TR_StorageReference::createTemporaryBasedStorageReference(cvdSize, comp);
   targetReg->setStorageReference(targetStorageReference, node);

   TR::Node *firstChild = node->getFirstChild();
   TR::Register *firstReg = cg->evaluate(firstChild);
   TR::MemoryReference *targetMR = generateS390LeftAlignedMemoryReference(node,
                                                                          targetStorageReference,
                                                                          cg,
                                                                          cvdSize,
                                                                          false); // enforceSSLimits=false for CVD

   generateRXInstruction(cg, op, node, firstReg, targetMR);

   targetReg->setIsInitialized();

   cg->stopUsingRegister(firstReg);
   cg->decReferenceCount(firstChild);
   node->setRegister(targetReg);
   return targetReg;
   }


TR::Register *
J9::Z::TreeEvaluator::pdnegEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   cg->traceBCDEntry("pdneg",node);
   cg->generateDebugCounter("PD-Op/pdneg", 1, TR::DebugCounter::Cheap);

   TR_ASSERT(node->getNumChildren() == 1, "pdneg should only have 1 child");

   TR::Node *srcNode = node->getFirstChild();
   TR_PseudoRegister *srcReg = cg->evaluateBCDNode(srcNode);
   TR::Compilation *comp = cg->comp();

   TR::MemoryReference *sourceMR = generateS390RightAlignedMemoryReference(srcNode, srcReg->getStorageReference(), cg);

   // also do for assumed (PFD) preferred and clean signs?
   int32_t srcSign = srcReg->hasKnownOrAssumedSignCode() ? srcReg->getKnownOrAssumedSignCode() : TR::DataType::getInvalidSignCode();
   bool useRegBasedSequence = TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z10) && srcReg->hasKnownValidSign();
   bool isSrcSign0xF     = srcSign == 0xf;
   bool isSimpleSignFlip = srcSign == TR::DataType::getPreferredPlusCode() ||
                           srcSign == TR::DataType::getPreferredMinusCode() ||
                           srcReg->hasKnownOrAssumedPreferredSign() ||
                           srcReg->hasKnownOrAssumedCleanSign();
   bool isSimpleSignSet = isSrcSign0xF || isSimpleSignFlip;
   bool needsFullInitialization = !useRegBasedSequence || isSimpleSignSet;
   bool isTruncation = node->getDecimalPrecision() < srcReg->getDecimalPrecision();
   bool isWiden = node->getDecimalPrecision() > srcReg->getDecimalPrecision();

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tpdnegEvaluator: isTruncation=%s, isWiden=%s, srcSign = 0x%x, srcSignIsValid=%s, isSimpleSignSet=%s, useRegBasedSequence=%s, needsFullInitialization=%s (== !useRegBasedSequence || isSimpleSignSet)\n",
         isTruncation ? "yes":"no",
         isWiden ? "yes":"no",
         srcSign,
         srcReg->hasKnownValidSign() ? "yes":"no",
         isSimpleSignSet ? "yes":"no",
         useRegBasedSequence?"yes":"no",
         needsFullInitialization? "yes":"no");


   TR_PseudoRegister *targetReg = evaluateBCDSignModifyingOperand(node,
                                                                  false,      // isEffectiveNop=false
                                                                  false,      // isNondestructiveNop=false
                                                                  needsFullInitialization,
                                                                  sourceMR,
                                                                  cg);
   targetReg->setDecimalPrecision(std::min<int32_t>(node->getDecimalPrecision(), srcReg->getDecimalPrecision()));

   TR::MemoryReference *destMR = generateS390LeftAlignedMemoryReference(node, targetReg->getStorageReference(), cg, targetReg->getSize());

   if (srcReg->hasKnownValidData())
      targetReg->setHasKnownValidData();

   if (!needsFullInitialization && !targetReg->isInitialized() && targetReg->getSize() > 1)
      {
      int32_t mvcSize = targetReg->getSize() - 1;  // do not include the least significant byte as this is done as part of the sign setting below
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\ttargetReg is not init and size %d > 1 so gen MVC with size targetRegSize-1 = %d and leftMostByte %d\n",
            targetReg->getSize(),mvcSize,targetReg->getSize());
      generateSS1Instruction(cg, TR::InstOpCode::MVC, node,
                             mvcSize-1,
                             reuseS390LeftAlignedMemoryReference(destMR, node, targetReg->getStorageReference(), cg, targetReg->getSize()),
                             reuseS390LeftAlignedMemoryReference(sourceMR, srcNode, srcReg->getStorageReference(), cg, targetReg->getSize()));
      }

   bool isSignManipulation = false;
   if (isSrcSign0xF)
      {
      cg->genSignCodeSetting(node, targetReg, targetReg->getSize(), destMR, TR::DataType::getPreferredMinusCode(), srcReg, 0, false); // digitsToClear=0, numericNibbleIsZero=false
      if (targetReg->getDataType() == TR::PackedDecimal && targetReg->isEvenPrecision())
         cg->genZeroLeftMostDigitsIfNeeded(node, targetReg, targetReg->getSize(), 1, destMR);
      }
   else if (isSimpleSignFlip)
      {
      isSignManipulation = true;
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tsrcReg has known preferred (%s) or known clean (%s) sign so gen XI 0x1 of sign byte to flip it\n",
            srcReg->hasKnownPreferredSign()?"yes":"no",srcReg->hasKnownCleanSign()?"yes":"no");
      generateSIInstruction(cg, TR::InstOpCode::XI, node, reuseS390LeftAlignedMemoryReference(destMR, node, targetReg->getStorageReference(), cg, 1), 0x01);
      if (targetReg->getDataType() == TR::PackedDecimal && targetReg->isEvenPrecision())
         cg->genZeroLeftMostDigitsIfNeeded(node, targetReg, targetReg->getSize(), 1, destMR);

      }
   else if (useRegBasedSequence)
      {
      isSignManipulation = true;

      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\ttargetReg has unknown but valid sign so generate register based decode sequence\n");

      TR::Register *tempSign = cg->allocateRegister();
      TR::Register *targetSign = cg->allocateRegister();
      TR::Register *targetData = cg->allocateRegister();

      generateRXInstruction(cg, TR::InstOpCode::LB, node, tempSign, reuseS390LeftAlignedMemoryReference(sourceMR, srcNode, srcReg->getStorageReference(), cg, 1));

      generateRRInstruction(cg, TR::InstOpCode::LR, node, targetSign, tempSign);
      generateRRInstruction(cg, TR::InstOpCode::LR, node, targetData, tempSign);

      generateRIInstruction(cg, TR::InstOpCode::AHI, node, tempSign, 1);
      generateRIInstruction(cg, TR::InstOpCode::NILL, node, targetData, 0xF0);

      if (targetReg->getDataType() == TR::PackedDecimal && targetReg->isEvenPrecision())
         cg->genZeroLeftMostDigitsIfNeeded(node, targetReg, targetReg->getSize(), 1, destMR);

      if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::zEC12))
         generateRIEInstruction(cg, TR::InstOpCode::RISBGN, node, targetData, tempSign, 63, 63, 64-3);
      else
         generateRIEInstruction(cg, TR::InstOpCode::RISBG, node, targetData, tempSign, 63, 63, 64-3);

      generateRRInstruction(cg, TR::Compiler->target.is64Bit() ? TR::InstOpCode::NGR : TR::InstOpCode::NR, node, targetSign, targetData);
      generateRILInstruction(cg, TR::InstOpCode::XILF, node, targetSign, 13);

      generateRXInstruction(cg, TR::InstOpCode::STC, node, targetSign, reuseS390LeftAlignedMemoryReference(destMR, node, targetReg->getStorageReference(), cg, 1));

      cg->stopUsingRegister(tempSign);
      cg->stopUsingRegister(targetSign);
      cg->stopUsingRegister(targetData);
      }
   else
      {
      isSignManipulation = true;
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\ttargetReg has possibly invalid sign so generate memory based decode sequence\n");

      TR::Register *sourceSign = cg->allocateRegister();
      TR::Register *regMask = NULL;

      generateRXInstruction(cg, TR::Compiler->target.is64Bit() ? TR::InstOpCode::LLGC : TR::InstOpCode::LLC,
           node, sourceSign, reuseS390LeftAlignedMemoryReference(sourceMR, srcNode, srcReg->getStorageReference(), cg, 1));

      uint8_t signCodes[TR_NUM_DECIMAL_CODES];
      for (int32_t i = 0; i < TR_NUM_DECIMAL_CODES; i++)
         {
         TR_BCDSignCode normalizedSign = decimalSignCodeMap[i];
         if (normalizedSign == bcd_invalid_sign)
            signCodes[i] = i;
         else if (normalizedSign == bcd_plus || normalizedSign == bcd_unsigned)
            signCodes[i] = TR::DataType::getPreferredMinusCode();
         else if (normalizedSign == bcd_minus)
            signCodes[i] = TR::DataType::getPreferredPlusCode();
         else
            TR_ASSERT(false,"unexpected normalized sign for raw sign 0x%x\n",i);
         }
      size_t paddingLitPoolOffset = cg->findOrCreateLiteral(signCodes, TR_NUM_DECIMAL_CODES);
      TR::Register *litPoolBaseReg = NULL;
      if(cg->isLiteralPoolOnDemandOn())
         {
         litPoolBaseReg = cg->allocateRegister();
         generateLoadLiteralPoolAddress(cg, node, litPoolBaseReg);
         }
      else
         {
         litPoolBaseReg = cg->allocateRegister();
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, litPoolBaseReg, cg->getLitPoolRealRegister());
         }

      generateRIInstruction(cg, TR::InstOpCode::NILL, node, sourceSign, 0x0F);

      generateRRInstruction(cg, TR::Compiler->target.is64Bit() ? TR::InstOpCode::ALGR : TR::InstOpCode::ALR, node, litPoolBaseReg, sourceSign);

      if (targetReg->getDataType() == TR::PackedDecimal && targetReg->isEvenPrecision())
         cg->genZeroLeftMostDigitsIfNeeded(node, targetReg, targetReg->getSize(), 1, destMR);

      int32_t mvnSize = 1;
      generateSS1Instruction(cg, TR::InstOpCode::MVN, node,
                             mvnSize-1,
                             reuseS390LeftAlignedMemoryReference(destMR, node, targetReg->getStorageReference(), cg, 1),
                             generateS390MemoryReference(litPoolBaseReg, paddingLitPoolOffset, cg));

      cg->stopUsingRegister(sourceSign);
      if (regMask)
         cg->stopUsingRegister(regMask);
      if (litPoolBaseReg)
         cg->stopUsingRegister(litPoolBaseReg);
      }

   if (isSignManipulation)
      {
      if (srcReg->hasKnownPreferredSign())
         targetReg->setHasKnownPreferredSign();
      else if (srcReg->hasAssumedPreferredSign())
         targetReg->setHasAssumedPreferredSign();
      else
         targetReg->setSignStateInitialized();

      if (srcReg->hasKnownValidSign())
         targetReg->setHasKnownValidSign();
      }

   targetReg->transferDataState(srcReg);
   targetReg->setIsInitialized();

   node->setRegister(targetReg);
   cg->decReferenceCount(srcNode);
   cg->traceBCDExit("pdneg",node);
   return targetReg;
   }

TR_PseudoRegister *
J9::Z::TreeEvaluator::evaluateBCDValueModifyingOperand(TR::Node * node,
                                                       bool initTarget,
                                                       TR::MemoryReference *sourceMR,
                                                       TR::CodeGenerator * cg,
                                                       bool trackSignState,
                                                       int32_t sourceSize,
                                                       bool alwaysLegalToCleanSign) // alwaysLegalToCleanSign=true then a ZAP can be used to init/widen if another signMod inst is coming (e.g. AP)
   {
   TR_ASSERT(node->getType().isBCD(),"node %p type %s must be BCD\n",node,node->getDataType().toString());
   TR_OpaquePseudoRegister *reg = evaluateValueModifyingOperand(node, initTarget, sourceMR, cg, trackSignState, sourceSize, alwaysLegalToCleanSign);
   TR_PseudoRegister *pseudoReg = reg->getPseudoRegister();
   TR_ASSERT(pseudoReg,"pseudoReg should be non-NULL for node %p\n",node);
   return pseudoReg;
   }


TR_OpaquePseudoRegister *
J9::Z::TreeEvaluator::evaluateValueModifyingOperand(TR::Node * node,
                                                    bool initTarget,
                                                    TR::MemoryReference *sourceMR,
                                                    TR::CodeGenerator * cg,
                                                    bool trackSignState,
                                                    int32_t sourceSize,
                                                    bool alwaysLegalToCleanSign) // alwaysLegalToCleanSign=true then a ZAP can be used to init/widen if another signMod inst is coming (e.g. AP)
   {
   bool isBCD = node->getType().isBCD();
   bool isAggr = node->getType().isAggregate();
   TR_ASSERT(isBCD || isAggr,"node %p type %s must be BCD or aggregate\n",node,node->getDataType().toString());

   TR_OpaquePseudoRegister *targetReg = isBCD ? cg->allocatePseudoRegister(node->getDataType()) : cg->allocateOpaquePseudoRegister(node->getDataType());
   TR_PseudoRegister *targetBCDReg = targetReg->getPseudoRegister();

   TR::Node *firstChild = node->getFirstChild();
   TR_OpaquePseudoRegister *firstReg = cg->evaluateOPRNode(firstChild);
   TR_PseudoRegister *firstBCDReg = firstReg->getPseudoRegister();
   TR_StorageReference *firstStorageReference = firstReg->getStorageReference();
   TR::Compilation *comp = cg->comp();

   bool isInitialized = firstReg->isInitialized();
   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tevaluateValueModifyingOperand for %s (%p) with targetReg %s and firstReg %s (#%d isInit %s), sourceSize=%d : initTarget=%s, alwaysLegalToCleanSign=%s\n",
         node->getOpCode().getName(),node,cg->getDebug()->getName(targetReg),cg->getDebug()->getName(firstReg),
         firstStorageReference->getReferenceNumber(),isInitialized ? "yes":"no",sourceSize,initTarget ? "yes":"no",alwaysLegalToCleanSign ? "yes":"no");

   if (sourceSize == 0)
      sourceSize = firstReg->getSize();

   bool useZAP = false;

   // to avoid a clobber evaluate in the isInitialized case favour initializing to an available store hint and leave the isInitialized child untouched
   // also force to a new hint even if refCount==1 if there is ZAP widening to be done (and save a later clear)
   bool useNewStoreHint = !comp->getOption(TR_DisableNewStoreHint) &&
                          node->getOpCode().canHaveStorageReferenceHint() &&
                          initTarget && // have to also be initializing here otherwise in caller
                          node->getStorageReferenceHint() &&
                          node->getStorageReferenceHint()->isNodeBasedHint() &&
                          (firstChild->getReferenceCount() > 1 || node->getStorageReferenceSize() > sourceSize) &&
                          node->getStorageReferenceHint() != firstStorageReference;

   if (useNewStoreHint && node->getStorageReferenceHint()->getSymbolSize() < node->getStorageReferenceSize())
      {
      useNewStoreHint = false;
      TR_ASSERT(false,"a storageRef hint should be big enough for the node result (%d is not >= %d)\n",
         node->getStorageReferenceHint()->getSymbolSize(),node->getStorageReferenceSize());
      }

   if (isInitialized && !useNewStoreHint)
      {
      // Save the storage reference dependent state leftAlignedZeroDigits, rightAlignedDeadBytes and the derived liveSymbolSize before
      // the possible call to ssrClobberEvaluate below.
      // If a clobber evaluate is done then the above mentioned state will be reset on firstReg (so subsequent commoned uses of firstReg that now
      // use the newly created temporary storage reference are correct). Cache the values here as this state *will* persist up this tree on the targetReg.
      int32_t savedLiveSymbolSize = firstReg->getLiveSymbolSize();
      int32_t savedLeftAlignedZeroDigits = firstReg->getLeftAlignedZeroDigits();
      int32_t savedRightAlignedDeadBytes = firstReg->getRightAlignedDeadBytes();
      int32_t savedRightAlignedIgnoredBytes = firstReg->getRightAlignedIgnoredBytes();
      bool skipClobberEvaluate = false;
      if (node->getOpCode().isBasicOrSpecialPackedArithmetic())
         {
         // The special case of mul/add/sub/div = op1*op1 does not need a clobber evaluate as there are no uses beyond the current node's operation
         if (node->getNumChildren() > 1 &&
             node->getFirstChild() == node->getSecondChild() &&
             node->getFirstChild()->getReferenceCount() == 2 &&
             firstStorageReference->getOwningRegisterCount() == 1)
            {
            skipClobberEvaluate = true;
            }
         }
      if (!skipClobberEvaluate)
         cg->ssrClobberEvaluate(firstChild, sourceMR);
      int32_t resultSize = node->getStorageReferenceSize();
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tisInitialized==true: liveSymSize %d (symSize %d - firstReg->deadAndIgnoredBytes %d), resultSize = %d (nodeSize %d)\n",
            savedLiveSymbolSize,firstStorageReference->getSymbolSize(),firstReg->getRightAlignedDeadAndIgnoredBytes(),resultSize,node->getSize());
      if (savedLiveSymbolSize < resultSize)
         {
         // In this case the source memory slot has been initialized but it is no longer larger enough to contain the result for the current node.
         // Therefore either the size of the symbol must be increased (for autos) or a new larger, memory slot must be created and initialized (for non-autos)
         if (firstStorageReference->isTemporaryBased())
            {
            if (cg->traceBCDCodeGen())
               {
               traceMsg(comp,"\treg->getLiveSymbolSize() < resultSize (%d < %d) so call increaseTemporarySymbolSize\n",savedLiveSymbolSize,resultSize);
               traceMsg(comp,"\t\t * setting rightAlignedDeadBytes %d from firstReg %s to targetReg %s (valueMod incSize)\n",
                  savedRightAlignedDeadBytes,cg->getDebug()->getName(firstReg),cg->getDebug()->getName(targetReg));
               traceMsg(comp,"\t\t * setting rightAlignedIgnoredBytes %d from firstReg %s to targetReg %s (valueMod incSize)\n",
                  savedRightAlignedIgnoredBytes,cg->getDebug()->getName(firstReg),cg->getDebug()->getName(targetReg));
               }
            targetReg->setStorageReference(firstStorageReference, node);
            targetReg->increaseTemporarySymbolSize(resultSize - savedLiveSymbolSize);
            targetReg->setRightAlignedDeadBytes(savedRightAlignedDeadBytes);
            targetReg->setRightAlignedIgnoredBytes(savedRightAlignedIgnoredBytes);
            }
         else
            {
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\t\tfirstStorageReference is not temporary based and liveSymSize < resultSize (%d < %d) so alloc and init a new temp slot and clear left most bytes\n",
                  savedLiveSymbolSize,resultSize);
            int32_t destLength = resultSize;
            int32_t srcLength = sourceSize;
            // If the firstStorageReference is not a temp or a hint then the recursive dec in setStorageReference() will be wrong.
            // This should always be true because this is the initialized case and it is not legal to initialize a non-temp or non-hint.
            TR_ASSERT( firstStorageReference->isNodeBasedHint(), "expecting the srcStorargeReference to be a node based hint\n");
            bool performExplicitWidening = false;
            cg->initializeNewTemporaryStorageReference(node, targetReg, destLength, firstChild, firstReg, srcLength, sourceMR, performExplicitWidening, alwaysLegalToCleanSign, trackSignState);
            if (targetBCDReg)
               {
               TR_ASSERT(firstBCDReg,"firstBCDReg should be non-NULL when targetBCDReg is non-NULL for node %p\n",firstChild);
               if (performExplicitWidening)
                  targetBCDReg->setDecimalPrecision(node->getDecimalPrecision());
               else
                  targetBCDReg->setDecimalPrecision(firstBCDReg->getDecimalPrecision());
               }
            else
               {
               if (performExplicitWidening)
                  targetReg->setSize(node->getSize());
               else
                  targetReg->setSize(firstReg->getSize());
               }
            }
         }
      else
         {
         if (cg->traceBCDCodeGen())
            {
            traceMsg(comp,"\tliveSymSize >= resultSize (%d >= %d) so can reuse the firstStorageReference #%d for the targetStorageReference\n",
               savedLiveSymbolSize,resultSize,firstStorageReference->getReferenceNumber());
            traceMsg(comp,"\t\t * setting rightAlignedDeadBytes %d from firstReg %s to targetReg %s (valueMod reuse)\n",
               savedRightAlignedDeadBytes,cg->getDebug()->getName(firstReg),cg->getDebug()->getName(targetReg));
            traceMsg(comp,"\t\t * setting rightAlignedIgnoredBytes %d from firstReg %s to targetReg %s (valueMod reuse)\n",
               savedRightAlignedIgnoredBytes,cg->getDebug()->getName(firstReg),cg->getDebug()->getName(targetReg));
            traceMsg(comp,"\t\t * setting savedLeftAlignedZeroDigits %d from firstReg %s to targetReg %s (valueMod reuse)\n",
               savedLeftAlignedZeroDigits,cg->getDebug()->getName(firstReg),cg->getDebug()->getName(targetReg));
            }
         targetReg->setStorageReference(firstStorageReference, node);
         targetReg->setLeftAlignedZeroDigits(savedLeftAlignedZeroDigits);
         targetReg->setRightAlignedDeadBytes(savedRightAlignedDeadBytes);
         targetReg->setRightAlignedIgnoredBytes(savedRightAlignedIgnoredBytes);
         }
      targetReg->setIsInitialized();
      cg->freeUnusedTemporaryBasedHint(node);
      }
   else
      {
      // when initializing the hint storage reference use the symbol size and not the current node size so the same storage reference may be used
      // without further zero initialization for larger node sizes
      TR_StorageReference *targetStorageReference = NULL;
      int32_t destLength = 0;
      if (node->getOpCode().canHaveStorageReferenceHint() && node->getStorageReferenceHint())
         {
         int32_t resultSize = node->getStorageReferenceSize();
         targetStorageReference = node->getStorageReferenceHint();
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tusing storageRefHint #%d on node %p (useNewStoreHintOnInit=%d)\n",targetStorageReference->getReferenceNumber(),node,useNewStoreHint && isInitialized);
         if (targetStorageReference->isTemporaryBased())
            {
            // Consider this scenario (common when a sub-expression is rooted in a load of a large value returned from a runtime routine)
            //
            // store
            //   x      <- size < 10
            //     y    <- current node size=10
            //       z  <- size > 10 and a passThrough operation
            //         load <- size > 10
            //
            // The temporary hint is the size of z but if performExplicitWidening is also set to true below then code will be generated to initialize up
            // to the size of z even though this extra initialized space will be unused for the rest of the operation.
            // Nodes (x,y,z) that share the same hint are tracked and removed when the node is evaluated. At the current node's (y) initialization point
            // only x,y will be in this list and only up to size=10 will be initialized.
            destLength = targetStorageReference->getMaxSharedNodeSize();
            }
         }
      else
         {
         targetStorageReference = TR_StorageReference::createTemporaryBasedStorageReference(node->getStorageReferenceSize(), comp);
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tcreated new targetStorageReference #%d on node %p\n",targetStorageReference->getReferenceNumber(),node);
         }

      if (destLength > 0)
         {
         // update the symSize so in the initTarget=false case a consumer will not do a needlessly large initialization
         targetStorageReference->getTemporarySymbol()->setActiveSize(destLength);
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tsetting destLength and activeSize for initialization based on the smallest remaining node left on the temp based hint #%d : %d\n",
               targetStorageReference->getReferenceNumber(),destLength);
         }
      else if (destLength == 0)
         {
         destLength = targetStorageReference->getSymbolSize();
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tsetting destLength for initialization based on the current storageRef #%d size : %d\n",targetStorageReference->getReferenceNumber(),destLength);
         }
      else
         {
         TR_ASSERT(false,"unexpected negative destLength of %d for node %p\n",destLength,node);
         }

      targetReg->setStorageReference(targetStorageReference, node);
      if (initTarget)
         {
         int32_t srcLength  = sourceSize;
         TR::MemoryReference *destMR = isBCD ?
            generateS390RightAlignedMemoryReference(node, targetStorageReference, cg) :
            generateS390MemRefFromStorageRef(node, targetStorageReference, cg);
         // for packed to packed operations this is likely the start of some (possibly large) computation so *do* perform the explicit widening all at once at
         // the start so later operations do not have to clear.
         bool performExplicitWidening = targetReg->getDataType() == TR::PackedDecimal && firstReg->getDataType() == TR::PackedDecimal;

         int32_t zeroDigits = firstReg->getLeftAlignedZeroDigits();
         if (isBCD &&
             zeroDigits > 0 &&
             zeroDigits > targetReg->getLeftAlignedZeroDigits() &&
             firstReg->getLiveSymbolSize() == targetReg->getLiveSymbolSize() &&
             cg->storageReferencesMatch(targetStorageReference, firstStorageReference))
            {
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\ty^y : transfer leftAlignedZeroDigits %d from firstReg %s to targetReg %s (node %s %p)\n",
                  zeroDigits,cg->getDebug()->getName(firstReg),cg->getDebug()->getName(targetReg),node->getOpCode().getName(),node);
            targetReg->setLeftAlignedZeroDigits(zeroDigits);
            }

         cg->initializeStorageReference(node, targetReg, destMR, destLength, firstChild, firstReg, sourceMR, srcLength, performExplicitWidening, alwaysLegalToCleanSign, trackSignState);
         if (targetBCDReg)
            {
            TR_ASSERT(firstBCDReg,"firstBCDReg should be non-NULL when targetBCDReg is non-NULL for node %p\n",firstChild);
            if (performExplicitWidening)
               targetBCDReg->setDecimalPrecision(node->getDecimalPrecision());
            else
               targetBCDReg->setDecimalPrecision(firstBCDReg->getDecimalPrecision());
            targetBCDReg->transferDataState(firstBCDReg);
            }
         else
            {
            if (performExplicitWidening)
               targetReg->setSize(node->getSize());
            else
               targetReg->setSize(firstReg->getSize());
            }
         targetReg->setIsInitialized();
         }
      }
   if (cg->traceBCDCodeGen() && targetReg->getStorageReference()->isReadOnlyTemporary())
      traceMsg(comp,"reset readOnlyTemp flag on storageRef #%d (%s) (valueMod case)\n",
         targetReg->getStorageReference()->getReferenceNumber(),cg->getDebug()->getName(targetReg->getStorageReference()->getSymbol()));
   targetReg->getStorageReference()->setIsReadOnlyTemporary(false, NULL);
   node->setRegister(targetReg);
   return targetReg;
   }

/**
 * Handles all BCD and aggregate load and const types direct and indirect
 *
 * pdload
 * pdloadi
 *
 * zdload
 * zdloadi
 *
 * zdsleLoad
 * zdsleLoadi
 *
 * zdslsLoad
 * zdslsLoadi
 *
 * zdstsLoad
 * zdstsLoadi
 *
 * udLoad
 * udLoadi
 *
 * udstLoad
 * udstLoadi
 *
 * udslLoad
 * udslLoadi
 */
TR::Register *J9::Z::TreeEvaluator::pdloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   cg->traceBCDEntry("pdload",node);
   TR::Register* reg = NULL;

   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(cg->comp(), "PD-Op/%s", node->getOpCode().getName()),
                            1, TR::DebugCounter::Cheap);
   static char* isVectorBCDEnv = feGetEnv("TR_enableVectorBCD");
   if((TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() && !cg->comp()->getOption(TR_DisableVectorBCD) || isVectorBCDEnv) &&
           (node->getOpCodeValue() == TR::pdload || node->getOpCodeValue() == TR::pdloadi))
      {
      reg = pdloadVectorEvaluatorHelper(node, cg);
      }
   else
      {
      reg = pdloadEvaluatorHelper(node, cg);
      }

   cg->traceBCDExit("pdload",node);
   return reg;
   }


TR::Register *J9::Z::TreeEvaluator::pdloadEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();

   bool isBCD = node->getType().isBCD();

   TR_ASSERT(node->getOpCode().isLoadConst() ||
          (node->getOpCode().hasSymbolReference() && node->getSymbolReference() && !node->getSymbolReference()->isTempVariableSizeSymRef()),
      "load node %p must not be of a tempVariableSizeSymRef\n",node);

   TR_StorageReference *storageRef = TR_StorageReference::createNodeBasedStorageReference(node, node->getReferenceCount(), comp);

   TR_ASSERT(!node->getOpCode().isLoadConst() || node->getNumChildren() == 1,"BCD constant type (%s) should have 1 child and not %d children\n",
      node->getDataType().toString(),node->getNumChildren());
   bool isConstant = node->getOpCode().isLoadConst();
   bool isReadOnlyConstant = false;

   TR_OpaquePseudoRegister *targetReg = NULL;
   if (isBCD)
      {
      targetReg = cg->allocatePseudoRegister(node->getDataType());
      TR_PseudoRegister *targetPseudoReg = targetReg->getPseudoRegister();
      TR_ASSERT(targetPseudoReg,"targetPseudoReg should be non-NULL for node %p\n",node);
      targetPseudoReg->setStorageReference(storageRef, node);
      if (isConstant)
         {
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\t%s (%p) is a constant load so set hasKnownValidSignAndData = true%s\n",
               node->getOpCode().getName(),node,isReadOnlyConstant?" and skip privatizeStorageReference":"");
         targetPseudoReg->setHasKnownValidSignAndData();
         }

      if (node->hasKnownOrAssumedSignCode())
         {
         switch (node->getKnownOrAssumedSignCode())
            {
            case raw_bcd_sign_0xc:
               node->hasKnownSignCode() ? targetPseudoReg->setKnownSignCode(0xc) : targetPseudoReg->setAssumedSignCode(0xc);
               break;
            case raw_bcd_sign_0xd:
               node->hasKnownSignCode() ? targetPseudoReg->setKnownSignCode(0xd) : targetPseudoReg->setAssumedSignCode(0xd);
               break;
            case raw_bcd_sign_0xf:
               if (node->hasKnownOrAssumedCleanSign())
                  {
                  // Something has gone wrong and we've ended up with conflicting sign code properties on the node
                  // This is a bug and should be fixed but in a prod build conservatively reset the clean sign flag and
                  // do transfer the sign to the targetPseudoReg
                  TR_ASSERT(false,"conflicting sign code: sign code 0xf is not clean\n");
                  node->setHasKnownAndAssumedCleanSign(false);
                  }
               else
                  {
                  node->hasKnownSignCode() ? targetPseudoReg->setKnownSignCode(0xf) : targetPseudoReg->setAssumedSignCode(0xf);
                  }
               break;
            case raw_bcd_sign_unknown:
               break;
            default: TR_ASSERT(false,"unexpected node->getKnownOrAssumedSignCode() of %d\n",node->getKnownOrAssumedSignCode());
            }
         }

      if (!node->getOpCode().isSignlessBCDType() && node->hasKnownOrAssumedCleanSign())
         {
         uint32_t preferredPlusSign = TR::DataType::getPreferredPlusSignCode(node->getDataType());
         uint32_t preferredMinusSign = TR::DataType::getPreferredMinusSignCode(node->getDataType());
         if (node->isNonNegative()) // >= 0
            node->hasKnownCleanSign() ? targetPseudoReg->setKnownSignCode(preferredPlusSign) : targetPseudoReg->setAssumedSignCode(preferredPlusSign);
         else if (node->isNonZero() && node->isNonPositive())  // < 0
            node->hasKnownCleanSign() ? targetPseudoReg->setKnownSignCode(preferredMinusSign) : targetPseudoReg->setAssumedSignCode(preferredMinusSign);
         if (cg->traceBCDCodeGen() && targetPseudoReg->hasKnownOrAssumedSignCode())
            traceMsg(comp,"\ttargetPseudoReg has%sSignCode = true and it is 0x%x\n",targetPseudoReg->hasAssumedSignCode()?"Assumed":"Known",targetPseudoReg->getKnownOrAssumedSignCode());
         // call setHasCleanSign() after the set*SignCode() calls so the TR::DataType::getPreferredMinusCode() does not unset
         // the clean flag (as it must conservatively do to account for the unclean case of -0)
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tsetting Has%sCleanSign (due to node flag) on targetPseudoReg %s on %s (%p)\n",
               node->hasKnownCleanSign()?"Known":"Assumed",cg->getDebug()->getName(targetPseudoReg),node->getOpCode().getName(),node);
         node->hasKnownCleanSign() ? targetPseudoReg->setHasKnownCleanSign() : targetPseudoReg->setHasAssumedCleanSign();
         }

      // set decimal precision here so any copy made in privatizeStorageReference is marked with the correct precision
      targetPseudoReg->setDecimalPrecision(node->getDecimalPrecision());

      if (comp->fej9()->assumeLeftMostNibbleIsZero() && targetPseudoReg->isEvenPrecision() && TR::DataType::getDigitSize(node->getDataType()) == HalfByteDigit)
         targetPseudoReg->setLeftMostNibbleClear();

      if (storageRef->isTemporaryBased())
         {
         TR_ASSERT(false,"storageRef for load node %p should not be temp based\n");
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tstorageRef is tempBased so set targetReg %s to isInitialized=true\n",cg->getDebug()->getName(targetPseudoReg));
         targetPseudoReg->setIsInitialized();
         }

      if (cg->traceBCDCodeGen())
         {
         traceMsg(comp,"\tsignState on targetReg %s for %s (%p) :\n",cg->getDebug()->getName(targetPseudoReg),node->getOpCode().getName(),node);
         traceMsg(comp,"\t\tknownCleanSign=%d, knownPrefSign=%d, knownSign=0x%x, assumedCleanSign=%d, assumedPrefSign=%d, assumedSign=0x%x (signStateKnown %d, signStateAssumed %d)\n",
            targetPseudoReg->hasKnownCleanSign(),targetPseudoReg->hasKnownPreferredSign(),targetPseudoReg->hasKnownSignCode()?targetPseudoReg->getKnownSignCode():0,
            targetPseudoReg->hasAssumedCleanSign(),targetPseudoReg->hasAssumedPreferredSign(),targetPseudoReg->hasAssumedSignCode()?targetPseudoReg->getAssumedSignCode():0,
            targetPseudoReg->signStateKnown(),
            targetPseudoReg->signStateAssumed());
         traceMsg(comp,"\t%s (%p) has hasSignStateOnLoad=%d\n",node->getOpCode().getName(),node,node->hasSignStateOnLoad());
         }

      if (!node->hasSignStateOnLoad())
         {
         // even if a particular sign state is not known (i.e. clean,preferred, a particular value) knowing that a load does not have
         // any incoming sign state can help in generating better code (e.g. a ZAP can be used for widening as the side effect of cleaning
         // the sign will not matter vs using a ZAP to widen and illegally modifying a loaded value with an unsigned sign code 0xf->0xc)
         targetPseudoReg->setSignStateInitialized();
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tsetting SignStateInitialized due to hasSignStateOnLoad=false flag on %s (%p)\n",node->getOpCode().getName(),node);
         }
      }
   else
      {
      targetReg = cg->allocateOpaquePseudoRegister(node->getDataType());
      targetReg->setStorageReference(storageRef, node);
      }
   node->setRegister(targetReg);
   if (comp->getOption(TR_ForceBCDInit) || !isReadOnlyConstant)
      cg->privatizeStorageReference(node, targetReg, NULL);
   return targetReg;
   }

/**
 * \brief This helper uses vector instructions to evaluate pdload and pdloadi.
 *
 * Other types of load (zd, ud, etc) can't use vector registers/instructions.
 */
TR::Register*
J9::Z::TreeEvaluator::pdloadVectorEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getOpCodeValue() == TR::pdload || node->getOpCodeValue() == TR::pdloadi, "vector instructions only support PD load.");
   traceMsg(cg->comp(), "pdload Vector Evaluator, node=%p %d\n", node, __LINE__);

   TR::Register* vTargetReg = vTargetReg = cg->allocateRegister(TR_VRF);
   TR::Node* addressNode = node->getFirstChild();

   // No need to evaluate the address node of the pdloadi.
   // generateVSIInstruction() API will call separateIndexRegister() to separate the index
   // register by emitting an LA instruction. If there's a need for large displacement adjustment,
   // LAY will be emitted instead.
   TR::MemoryReference* sourceMR = generateS390MemoryReference(node, cg, false);

   // Index of the first byte to load, counting from the right ranging from 0-15.
   uint8_t indexFromTheRight = TR_VECTOR_REGISTER_SIZE - 1;
   if (node->getDecimalPrecision() > TR_MAX_INPUT_PACKED_DECIMAL_PRECISION)
      {
      // we are loading as many digits as we can starting from the right most digit of the PD in memory
      // Need to calculate offset in order to load this way
      sourceMR->addToOffset(node->getSize() - TR_VECTOR_REGISTER_SIZE);
      }
   else
      {
      indexFromTheRight = node->getSize() - 1;
      }

   TR_ASSERT(indexFromTheRight >= 0 && indexFromTheRight <= 15, "Load length too large for VLRL instruction");
   if(cg->traceBCDCodeGen())
      {
      traceMsg(cg->comp(),"\tGen VLRL for %s node->size=%d\n",
               node->getOpCode().getName(),
               node->getSize());
      }
   generateVSIInstruction(cg, TR::InstOpCode::VLRL, node, vTargetReg, sourceMR, indexFromTheRight);

   node->setRegister(vTargetReg);
   cg->decReferenceCount(addressNode);
   return vTargetReg;
   }

/**
 * A ZAP with an overlapping dest (1st operand) and source (2nd operand) are allowed if the rightmost byte
 * of the 1st operand is coincident with or to the right of the rightmost byte of the second operand
 * Check for this special case here to allow it.
 *
 * pdstorei <mustClean> s=8 bytes
 *    aiadd
 *       aload
 *       iconst 386
 *    pdloadi s=5 bytes
 *       aiadd
 *          aload
 *          iconst 388
 *
 * In this example the store is from 386->394 and the load from 388->393 so the rightmost byte (393->394) of the 1st operand (store) of the ZAP
 * is to the right of the rightmost byte of the 2nd operand (load) at 392->393
 */
bool
isLegalOverlappingZAP(TR::Node *store, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tisLegalOverlappingZAP check : store %s (%p), valueChild %s (%p)\n",
         store->getOpCode().getName(),store,store->getValueChild()->getOpCode().getName(),store->getValueChild());

   if (!store->getOpCode().isStoreIndirect())
      return false;

   TR::Node *load = store->getValueChild();
   if (!load->getOpCode().isLoadIndirect())
      return false;

   if (load->getRegister())
      return false;

   if (load->hasKnownOrAssumedCleanSign()) // won't need a ZAP anyway so don't bother going further
      return false;

   TR::Node *storeAddr = store->getFirstChild();
   TR::Node *loadVarAddr = load->getFirstChild();

   if (!cg->isSupportedAdd(storeAddr))
      return false;

   if (!cg->isSupportedAdd(loadVarAddr))
      return false;

   if (!cg->nodeMatches(storeAddr->getFirstChild(), loadVarAddr->getFirstChild()))
      return false;

   if (!storeAddr->getSecondChild()->getOpCode().isIntegralConst())
      return false;

   if (!loadVarAddr->getSecondChild()->getOpCode().isIntegralConst())
      return false;

   int64_t storeSize = store->getSize();
   int64_t loadSize = load->getSize();

   int64_t storeAddrOffset = storeAddr->getSecondChild()->get64bitIntegralValue() + store->getSymbolReference()->getOffset();
   int64_t loadAddrOffset = loadVarAddr->getSecondChild()->get64bitIntegralValue() + load->getSymbolReference()->getOffset();

   int64_t storeStart = storeAddrOffset;
   int64_t storeEnd   = storeStart + storeSize;

   int64_t loadStart = loadAddrOffset;
   int64_t loadEnd   = loadStart + loadSize;

   if (cg->traceBCDCodeGen())
      {
      int64_t overlapStart = std::max(storeStart, loadStart);
      int64_t overlapEnd = std::min(storeEnd, loadEnd);
      traceMsg(comp,"\tstoreRange %lld->%lld vs loadRange %lld->%lld --> overlap range %lld -> %lld\n",
         storeStart,storeEnd,loadStart,loadEnd,overlapStart,overlapEnd);
      }

   if (storeEnd >= loadEnd)
      {
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\t\tstoreEnd %lld >= loadEnd %lld : overlap ZAP is legal\n",storeEnd, loadEnd);
      return true;
      }
   else
      {
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\t\tstoreEnd %lld < loadEnd %lld : overlap ZAP is NOT legal\n",storeEnd, loadEnd);
      return false;
      }
   }

/**
  * This evaluator handles the following packed (pd) and unpacked (zd, ud)
  * direct/indirect store operations
  *
  * pdstore
  * pdstorei
  *
  * zdstore
  * zdstorei
  *
  * zdsleStore
  * zdsleStorei
  *
  * zdslsStore
  * zdslsStorei
  *
  * zdstsStore
  * zdstsStorei
  *
  * udStore
  * udStorei
  *
  * udstStore
  * udstStorei
  *
  * udslStore
  * udslStorei
  */
TR::Register*
J9::Z::TreeEvaluator::pdstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   cg->traceBCDEntry("pdstore",node);
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(cg->comp(), "PD-Op/%s", node->getOpCode().getName()),
                            1, TR::DebugCounter::Cheap);
   static char* isVectorBCDEnv = feGetEnv("TR_enableVectorBCD");
   if((TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() &&
       !cg->comp()->getOption(TR_DisableVectorBCD) ||
       isVectorBCDEnv) &&
           (node->getOpCodeValue() == TR::pdstore || node->getOpCodeValue() == TR::pdstorei))
      {
      pdstoreVectorEvaluatorHelper(node, cg);
      }
   else
      {
      pdstoreEvaluatorHelper(node, cg);
      }

   cg->traceBCDExit("pdstore",node);
   return NULL;
   }

TR::Register* J9::Z::TreeEvaluator::pdstoreEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool isBCD = node->getType().isBCD();
   bool isAggr = node->getType().isAggregate();

   TR::Node * valueChild = node->getValueChild();
   bool isPacked = node->getType().isAnyPacked();
   bool isIndirect = node->getOpCode().isIndirect();
   TR::Compilation *comp = cg->comp();

   bool evaluatedPaddingAnchor = false; // store nodes may contain an extra node giving an address of padding bytes (e.g. 0xF0F0..F0 for zoned)
   bool useZAP = isPacked && node->mustCleanSignInPDStoreEvaluator();

   TR_ASSERT(isBCD || (node->getSize() == valueChild->getSize()),"nodeSize %d != srcSize %d for node %p\n",node->getSize(),valueChild->getSize(),node);

   // If a temp copy may be needed for a child load or passthrough operations (such as a redundant pdclean) but the pdstore location
   // will live on (skipCopyOnStore=true) then force the use of the pdstore result location for the child value (and do not generate a temp copy)
   // Note: that size check below isn't quite the same as the isByteTruncation one below (when setting isLegalToChangeCommonedChildAddress)
   // as this first one uses valueChild nodeSize instead of the valueChild regSize.
   // However in cases where the flag will be checked then the valueReg will be uninitialized so the valueChild->getSize() will equal the valueReg->getSize().
   //
   // useStoreAsAnAccumulator check is needed below as it indicates no overlap between the store and any ancestor. If there is possible overlap then setting the skipCopyOnLoad
   // flag is incorrect as commoned references will use the updated value (updated by this store) instead of the correct value from the first reference point
   // pdstore "a1" // a1 and a2 overlap in some way
   //    pdload "a2"
   //...
   //   =>pdload "a2"  // this commoned node needs the value at first reference and not the updated value after the pdstore to "a1"
   //                  // if skipCopyOnLoad is set then "a2" will be loaded again at the commoned point and get the wrong value.
   bool uninitializedSourceLocationMayBeKilled = false;
   bool mustUseZAP = false;
   bool overlapZAPIsAllowed = false;
   if (valueChild->getSize() <= node->getSize() &&
       !valueChild->skipCopyOnLoad() &&
       valueChild->getReferenceCount() > 1 &&
       node->skipCopyOnStore())
      {
      bool canForceSkipCopyOnLoad = false;
      if (node->useStoreAsAnAccumulator()) // see comment above
         {
         canForceSkipCopyOnLoad = true;
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tsetting valueChild (%s) %p setSkipCopyOnLoad=true due to store with skipCopyOnStore=true (storeAccumCase)\n",valueChild->getOpCode().getName(),valueChild);
         }
      else if (useZAP && isLegalOverlappingZAP(node, cg))
         {
         canForceSkipCopyOnLoad = true;
         mustUseZAP = true; // the overlap check and forcing of skipCopyOnLoad is only valid if we do actually end up generating a ZAP (vs an MVC for example) so make sure this happens
         overlapZAPIsAllowed = true;
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tsetting valueChild %s (%p) setSkipCopyOnLoad=true due to store with skipCopyOnStore=true (legalOverlappingZAPCase)\n",valueChild->getOpCode().getName(),valueChild);
         }
      if (canForceSkipCopyOnLoad)
         {
         valueChild->setSkipCopyOnLoad(true);
         uninitializedSourceLocationMayBeKilled = true;
         }
      }

   if (useZAP && valueChild->getOpCode().isPackedLeftShift())
      {
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tsetting valueChild %p cleanSignDuringPackedLeftShift=true due to store that needs a ZAP\n",valueChild);
      valueChild->setCleanSignDuringPackedLeftShift(true);
      }

   TR_OpaquePseudoRegister *valueReg = cg->evaluateOPRNode(valueChild);

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\t%s (%p) : isInMemoryCopyProp=%s\n",node->getOpCode().getName(),node,node->isInMemoryCopyProp()?"yes":"no");
   // NOTE: if a temp copy is generated below then valueStorageReference and valueReg are reset to point to the temp copies
   TR_StorageReference *valueStorageReference = valueReg->getStorageReference();
   TR::MemoryReference *sourceMR = NULL;
   TR_StorageReference *tempStorageReference = NULL;
   bool nodeAndValueRegSizeMatch = node->getSize() == valueReg->getSize();
   bool allSizesMatch = false;
   if (valueStorageReference->isNonConstantNodeBased())
      {
      allSizesMatch = nodeAndValueRegSizeMatch &&
                      valueReg->getSize() == valueStorageReference->getNode()->getSize();
      }
   else
      {
      allSizesMatch = nodeAndValueRegSizeMatch;
      }

   if (valueStorageReference->isNonConstantNodeBased() &&
       comp->getOption(TR_PrivatizeOverlaps) &&
       !overlapZAPIsAllowed &&
       !(node->useStoreAsAnAccumulator() || valueReg->isInitialized()))
      {
      // In addition to when the isInMemoryCopyProp flag is set on the store there are two other cases when an temp copy is needed for overlap
      // 1) isUsingStorageRefFromAnotherStore : even with CSE commoning (so not subject to isInMemoryCopyProp flag as the IL itself is safe)
      //    can result in an overlap if 'b' is updated to point to 'c' storageRef and 'd' overlap
      //    This is a lazy fixup -- could also pro-actively not set skipCopyOnStore for 'c' in the first place if the stores for any of the commoned 'b' nodes
      //    are in memory types (BCD/Aggr) that also overlap with 'c' (e.g. 'd' in this case)
      //
      // c
      //   b
      //
      // d
      //   =>b   (was just 'b' before CSE) but could  be 'c' after 'c' is evaluated
      //
      TR::Node *storageRefNode = valueStorageReference->getNode();
      bool isUsingStorageRefFromAnotherStore = storageRefNode->getOpCode().isStore() && storageRefNode != node;

      // 2) The valueRegHasDeadOrIgnoredBytes check is for when a ZAP could be generated for an overlapping copy where the rightmost
      // bytes are not coincident (due to the deadOrIgnoredBytes) so go through a temp in this case too
      //
      // This also handles the case like the below (so do not bother checking useZAP along with valueRegHasDeadOrIgnoredBytes)
      // The copy is not redundant when the valueReg has some dead or ignored bytes as the right most bytes of the source
      // and target will not be coincident in this case even if the addresses exactly match
      // izdstore p = 6 "A"
      //    addr1
      //    zdshrSetSign p = 1  --> valueReg has 5 ignored bytes
      //       izdload "A" p = 6
      //          =>addr1
      //       iconst 5    // shift
      ///      iconst 15   // setSign
      //
      // In this case have to move from offset +0 to offset +5 and then clear the top 5 bytes (starting at offset +0)
      // If copyIsRedundant is incorrectly set to true then only the clear of the top 5 bytes happens and the one surviving
      // digit from the zdshrSetSign is clobbered
      // MVC +0(base,L=1),+5(base)   move surviving digit first
      // MVC +0(base,L-5),(constant) complete widening by setting top 5 bytes to 0xF0
      bool valueRegHasDeadOrIgnoredBytes = valueReg->getRightAlignedIgnoredBytes() > 0;

      // 3) if there is any size mismatch between the sizes of node, valueReg and storageRefNode
      //
      // if nodeSize != storageRefNodeSize then this could be a truncating copy where the data needs to be moved back a number of bytes
      // "a" and "a_alias" start at the same address (so loadOrStoreAddressesMatch will return true) but "a" is 10 bytes and "a_alias" is 13 bytes
      // The meaning of the IL below is to move the low (addr+3) 10 bytes of "a_alias" back (to the left) 3 bytes.
      // This is actual needed data movement so a copy must be done (TODO : going through a temp here but this particular size mismatch case could
      // be done with an MVC as this direction of copy is non-destructive.
      // ipdstore "a" s=10
      //    addr
      //    ipdload "a_alias" s=13     // valueChild may not be a simple load but some commoned pdX operation that has the ipdload as its storageRefNode
      //       =>addr

      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tisInMemoryCopyProp=%s, isUsingStorageRefFromAnotherStore=%s, valueRegHasDeadOrIgnoredBytes=%s : node %s (%p), valueReg %s, storageRefNode %s (%p)\n",
            node->isInMemoryCopyProp() ? "yes":"no",
            isUsingStorageRefFromAnotherStore ? "yes":"no",
            valueRegHasDeadOrIgnoredBytes ? "yes":"no",
            node->getOpCode().getName(),node,
            cg->getDebug()->getName(valueReg),
            storageRefNode->getOpCode().getName(),storageRefNode);

      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tallSizesMatch=%s (nodeSize=%d, valueRegSize=%d, storageRefNodeSize=%d)\n",
            allSizesMatch ? "yes":"no",node->getSize(),valueReg->getSize(),storageRefNode->getSize());

      if (node->isInMemoryCopyProp() || isUsingStorageRefFromAnotherStore || valueRegHasDeadOrIgnoredBytes || !allSizesMatch)
         {
         // a redundant copy is an MVC with exact matching target and source. This is a nop but a very expensive nop as the hardware treats it
         // as any other overlap copy (i.e. very slowly)
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tnode %s (%p) and source %s (%p) may overlap but first check if copy would be redundant\n",
               node->getOpCode().getName(),node,valueChild->getOpCode().getName(),valueChild);

         bool copyIsRedundant = !valueRegHasDeadOrIgnoredBytes && allSizesMatch && cg->loadOrStoreAddressesMatch(node, valueStorageReference->getNode());

         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tgot copyIsRedundant=%s from first test\n",copyIsRedundant?"yes":"no");

         //Further check if there is potential destructive overlap based on storage info
         if (isAggr && !copyIsRedundant && !valueRegHasDeadOrIgnoredBytes && allSizesMatch)
            {
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\tperform test for definitelyNoDestructive overlap\n");

            if (cg->getStorageDestructiveOverlapInfo(valueStorageReference->getNode(), valueReg->getSize(), node, node->getSize()) == TR_DefinitelyNoDestructiveOverlap)
               {
               copyIsRedundant = true;
               if (cg->traceBCDCodeGen())
                  traceMsg(comp,"\t\tset copyIsRedundant=true : overlap check between node %s (%p) size=%d and valueStorageRefNode %s (%p) valueRegSize %d returns TR_DefinitelyNoDestructiveOverlap\n",
                     node->getOpCode().getName(),node,node->getSize(),
                     valueStorageReference->getNode()->getOpCode().getName(),valueStorageReference->getNode(),valueReg->getSize());
               }
            }

         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\t\tcopyIsRedundant=%s\n",copyIsRedundant?"yes":"no");

         if (!copyIsRedundant)
            {
            // i.e. a simple load/store BUT load and store memory may overlap so must use a temp so MVC doesn't destructively overlap and lose some source bytes
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\tnode %s (%p) and source %s (%p) (uninitialized valueReg %s) may overlap -- must privatize valueReg\n",
                  node->getOpCode().getName(),node,valueChild->getOpCode().getName(),valueChild,cg->getDebug()->getName(valueReg));

            int32_t privatizedSize = valueReg->getSize();
            int32_t storageRefNodeSize = storageRefNode->getSize();
            if (!valueReg->isInitialized() &&
                storageRefNodeSize != privatizedSize)
               {
               // may need to increase the size of the memcpy so it captures all of the source value -- this is important for the example above of moving 10 bytes starting at addr_1+3
               // back 3 bytes to addr_1
               // This 13 byte copy will copy the entire original field and then the store generated by the usual pdstoreEvaluator will be MVC addr_1(10,br),addr_1+3(10,br)
               privatizedSize = storageRefNodeSize;
               if (cg->traceBCDCodeGen())
                  traceMsg(comp,"\tset privatizedSize to storageRefNodeSize %d for uninit valueReg %s with mismatched storageRefNodeSize %d and valueRegSize %d\n",
                    privatizedSize,cg->getDebug()->getName(valueReg),storageRefNodeSize,valueReg->getSize());

               if (valueRegHasDeadOrIgnoredBytes)
                  {
                  // below IL comes from statements like : DIVIDE powerOfTenLit into var  where var is an unsigned zoned type
                  // zdstore           s=15
                  //    addr
                  //    zdshrSetSign   s=12  <- passThrough with 3 rightAligned deadBytes
                  //       izdload      s=15
                  //          =>addr
                  //       iconst 3       // shift
                  //       iconst 0xf     // sign
                  //
                  // in this case using an overridden size of 15 from the zdload is incorrect as there are only 12 valid bytes after the passThru zdshrSetSign
                  // If the offset on the addr is less then the shift then the final offset will be < 0 and the binary encoding time assume will be hit
                  // For larger offsets no compile time problem is hit but the temp copy reaches back to read bytes from before it's field (but the these bytes
                  // are not actually examined so everything ends up 'working' (delta any access exceptions if this were the first field in storage)
                  if (cg->traceBCDCodeGen())
                     traceMsg(comp,"\t\tgetRightAlignedIgnoredBytes %d > 0 so reduce privatizedSize %d -> %d\n",
                        valueReg->getRightAlignedIgnoredBytes(), privatizedSize,  privatizedSize - valueReg->getRightAlignedIgnoredBytes());
                  privatizedSize = privatizedSize - valueReg->getRightAlignedIgnoredBytes();
                  }
               }
            TR_OpaquePseudoRegister *tempRegister = cg->privatizePseudoRegister(valueChild, valueReg, valueStorageReference, privatizedSize);
            tempStorageReference = tempRegister->getStorageReference();

            if (cg->traceBCDCodeGen())
               {
               if (node->isInMemoryCopyProp())
                  traceMsg(comp,"\ta^a : privatize needed due to isInMemoryCopyProp node %s (%p) on line_no=%d (storeCase)\n",
                     node->getOpCode().getName(),node,comp->getLineNumber(node));
               if (isUsingStorageRefFromAnotherStore)
                  traceMsg(comp,"\ta^a : privatize needed due to isUsingStorageRefFromAnotherStore storageRefNode %s (%p) on line_no=%d (storeCase)\n",
                     storageRefNode->getOpCode().getName(),storageRefNode,comp->getLineNumber(node));
               if (valueRegHasDeadOrIgnoredBytes)
                  traceMsg(comp,"\ta^a : privatize needed due to valueRegHasDeadOrIgnoredBytes valueReg %s valueChild %s (%p) on line_no=%d (storeCase)\n",
                     cg->getDebug()->getName(valueReg),valueChild->getOpCode().getName(),valueChild,comp->getLineNumber(node));
               }

            TR_ASSERT(!comp->getOption(TR_EnablePerfAsserts),"gen overlap copy on node %s (%p) on line_no=%d (storeCase)\n",
               node->getOpCode().getName(),node,comp->getLineNumber(node));

            if (isBCD)
               sourceMR = generateS390RightAlignedMemoryReference(valueChild, tempStorageReference, cg);
            else
               sourceMR = generateS390MemRefFromStorageRef(valueChild, tempStorageReference, cg);

            valueReg = tempRegister;
            valueStorageReference = tempStorageReference;

            TR_ASSERT(!isBCD || valueReg->getPseudoRegister(),"valueReg must be a pseudoRegister on node %s (%p)\n",valueChild->getOpCode().getName(),valueChild);
            }
         }
      else
         {
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"y^y : temp copy saved isInMemoryCopyProp = false on %s (%p) (storeCase)\n",node->getOpCode().getName(),node);
         }
      }

   TR_PseudoRegister *bcdValueReg = NULL;
   if (valueReg->getPseudoRegister())
      {
      bcdValueReg = valueReg->getPseudoRegister();
      }

   int32_t destSize = node->getSize();
   int32_t sourceSize = valueReg->getSize();

   TR_ASSERT(isBCD || (destSize == sourceSize),"destSize %d != sourceSize %d for node %p\n",destSize,sourceSize,node);

   bool isByteTruncation = sourceSize > destSize;
   bool isByteWidening = destSize > sourceSize;

   bool isLeadingSignByteWidening = isByteWidening && node->getType().isLeadingSign();

   useZAP =  useZAP && bcdValueReg && (!bcdValueReg->hasKnownOrAssumedCleanSign() || mustUseZAP);
   //useZAP = useZAP || (isPacked && isByteTruncation); // truncating packed stores that need overflow exception should be using pdshlOverflow

   bool preserveSrcSign = bcdValueReg && !bcdValueReg->isLegalToCleanSign();

   bool savePreZappedValue = false;
   if (useZAP &&
       valueChild->getReferenceCount() > 1 &&
       preserveSrcSign)
      {
      savePreZappedValue = true;
      if (cg->traceBCDCodeGen())
         {
         traceMsg(comp,"\tsetting savePreZappedValue=true because valueReg (from valueChild %p with refCount %d > 1) ",valueChild,valueChild->getReferenceCount());
         if (!bcdValueReg->signStateInitialized())
            traceMsg(comp,"has an uninitialized sign state and a ZAP is to be used for the store\n");
         else
            traceMsg(comp,"has signCode 0x%x and a ZAP is to be used for the store\n", bcdValueReg->getKnownOrAssumedSignCode());
         }
      }

   bool childContainsAccumulatedResult = valueStorageReference->isNodeBased() &&
                                         valueStorageReference->isNodeBasedHint() &&
                                         (valueStorageReference->getNode() == node);

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tisPacked=%s, useZAP=%s, valueReg->signStateInit()=%s, valueReg->hasKnownOrAssumedCleanSign()=%s, isByteTruncation=%s, isByteWidening=%s, destSize=%d, sourceSize=%d\n",
         isPacked?"true":"false",
         useZAP?"true":"false",
         bcdValueReg && bcdValueReg->signStateInitialized()?"true":"false",
         bcdValueReg && bcdValueReg->hasKnownOrAssumedCleanSign()?"true":"false",
         isByteTruncation?"true":"false",
         isByteWidening?"true":"false",
         destSize,
         sourceSize);

   TR::Node *sourceNode = NULL;
   bool changeCommonedChildAddress = false;
   bool isLegalToChangeCommonedChildAddress = false;

   TR_ASSERT( !childContainsAccumulatedResult || valueReg->isInitialized(),"an accumulated result should also be initialized\n");

   if (!isByteTruncation &&
       !isLeadingSignByteWidening &&
       !savePreZappedValue &&
       tempStorageReference == NULL && // valueReg->setStorageReference() will not work in this case as the valueReg is pointing to the copy (tempRef count underflow)
       valueChild->getReferenceCount() > 1 &&
       node->skipCopyOnStore())
      {
      isLegalToChangeCommonedChildAddress = true;
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tsetting isLegalToChangeCommonedChildAddress=true for valueChild %s (%p) because isByteTruncation=false, isLeadingSignByteWidening=false, refCount %d > 1, skipCopyOnStore=true and savePreZappedValue=false\n",
            valueChild->getOpCode().getName(),
            valueChild,
            valueChild->getReferenceCount());
      }

   if (!valueStorageReference->isTemporaryBased() &&
       valueStorageReference->getNode() != node)
      {
      TR_ASSERT(!valueReg->isInitialized(),"expecting valueReg to not be initialized for valueChild %p\n",valueChild);
      TR_ASSERT(valueReg->getStorageReference()->isNodeBased(),"expecting valueReg storageRef to be nodeBased on valueChild %p\n",valueChild);
      if (valueStorageReference->getNode()->getOpCode().isStore())
         {
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"found uninit storageRef node based STORE case valueChild %s (%p) and storageRefNode %s (%p)\n",
               valueChild->getOpCode().getName(),
               valueChild,
               valueStorageReference->getNode()->getOpCode().getName(),
               valueStorageReference->getNode());
         }
      else if (valueStorageReference->getNode()->getOpCode().isLoad())
         {
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"found uninit storageRef node based LOAD case valueChild %s (%p) and storageRefNode %s (%p), skipCopyOnLoad storageRefNode is %s\n",
               valueChild->getOpCode().getName(),
               valueChild,
               valueStorageReference->getNode()->getOpCode().getName(),
               valueStorageReference->getNode(),
               valueStorageReference->getNode()->skipCopyOnLoad()?"yes":"no");
         }
      else
         {
         TR_ASSERT(false,"storageRefNode %p should be a load or a store node %p (%s)\n",valueStorageReference->getNode(),cg->getDebug()->getName(valueStorageReference->getNode()));
         }
      }

   if (valueStorageReference->isTemporaryBased() || (valueStorageReference->getNode() != node))
      {
      if (cg->traceBCDCodeGen() && valueStorageReference->isTemporaryBased())
         traceMsg(comp,"\tvalueStorageReference->isTemporaryBased() case so see if changeCommonedChildAddress should be set to true\n");
      else if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tvalueStorageReference->getNode() != node (%p != %p) case so see if changeCommonedChildAddress should be set to true\n",
            valueStorageReference->getNode(),node);

      sourceNode = valueChild;
      if (isLegalToChangeCommonedChildAddress)
         {
         if (useZAP)
            {
            changeCommonedChildAddress = true;
            if (cg->traceBCDCodeGen()) traceMsg(comp,"\t\tset changeCommonedChildAddress = true due to ZAP\n");
            }
         else if (isByteWidening)
            {
            changeCommonedChildAddress = true;
            if (cg->traceBCDCodeGen()) traceMsg(comp,"\t\tset changeCommonedChildAddress = true due to byteWidening\n");
            }
/* // disable this case, not a good enough reason for potential operand store compare
         else if (!isIndirect && valueChild->getOpCode().isIndirect())    // addressability is cheaper
            {
            changeCommonedChildAddress = true;
            if (cg->traceBCDCodeGen()) traceMsg(comp,"\t\tset changeCommonedChildAddress = true due to cheaper addressability\n");
            }
*/
         else if (uninitializedSourceLocationMayBeKilled &&
                  !valueStorageReference->isTemporaryBased() &&            // last two conditions are true when source location is uninitialized (passThrough operations or just a load child)
                  (valueStorageReference->getNode()->getOpCode().isLoadVar() || valueStorageReference->getNode()->getOpCode().isStore()))
            {
            changeCommonedChildAddress = true;
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\t\tset changeCommonedChildAddress = true due to uninitialized storageRefNode %p with skipCopyOnLoad that was forced to true\n",valueStorageReference->getNode());
            }
         else
            {
            if (cg->traceBCDCodeGen()) traceMsg(comp,"\t\tleave changeCommonedChildAddress = false\n");
            }
         }
      else
         {
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\t\tisLegalToChangeCommonedChildAddress = false so do not attempt to look for cases to set changeCommonedChildAddress to true\n");
         }
      }
   else
      {
      TR_ASSERT( childContainsAccumulatedResult, "expecting the pdstore child node to contain the accumulated result\n");
      // If there is any byte truncation and we are in the accumulator case then this means some leftmost child of the store
      // may have written data outside the bounds of the current store and this would be (horribly) incorrect.
      // This case should never occur as hints should only be assigned when the pdstore memory location is large enough
      // to contain any leftmost result value.
      TR_ASSERT( !isByteTruncation,"byte truncation should not occur when using the pdstore as an accumulator\n");
      changeCommonedChildAddress = true;
      if (cg->traceBCDCodeGen()) traceMsg(comp,"\taccumulated hint case so unconditionally set changeCommonedChildAddress = true\n");
      }

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tbef legality check: changeCommonedChildAddress = %s and isLegalToChangeCommonedChildAddress=%s so final changeCommonedChildAddress=%s\n",
         changeCommonedChildAddress?"true":"false",
         isLegalToChangeCommonedChildAddress?"true":"false",
         (changeCommonedChildAddress && isLegalToChangeCommonedChildAddress)?"true":"false");

   changeCommonedChildAddress = changeCommonedChildAddress && isLegalToChangeCommonedChildAddress;

   // well this is unfortunate -- the valueChild has skipCopyOnLoad set on it but for some reason (likely some corner case savePreZappedValue)
   // isLegalToChangeCommonedChildAddress is false.
   // This means that it is not safe to keep using the storageRef on the valueChild past this store point so must force it to a temp
   bool mustPrivatizeValueChild = tempStorageReference == NULL && !valueReg->isInitialized() && uninitializedSourceLocationMayBeKilled && !changeCommonedChildAddress;
   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tmustPrivatizeValueChild=%s\n",mustPrivatizeValueChild?"yes":"no");

   TR_StorageReference *targetStorageReference =
         TR_StorageReference::createNodeBasedStorageReference(node,
                                                              changeCommonedChildAddress ? valueChild->getReferenceCount() : 1,
                                                              comp);

   rcount_t origValueChildRefCount = valueChild->getReferenceCount();

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tcreate node based targetStorageReference #%d from %s (%p) and nodeRefCount %d (%s)\n",
         targetStorageReference->getReferenceNumber(),
         node->getOpCode().getName(),
         node,
         targetStorageReference->getNodeReferenceCount(),
         changeCommonedChildAddress?"from valueChild":"fixed at 1");

   TR::MemoryReference *targetMR = NULL;
   if (useZAP)
      {
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tuseZAP=true so gen ZAP but first determine the zapDestSize, initial size is destSize=%d\n",destSize);
      int32_t zapDestSize = destSize;
      targetMR = generateS390RightAlignedMemoryReference(node, targetStorageReference, cg);
      TR::Node *sourceNodeForZAP = sourceNode;
      if (sourceNode)
         {
         if (sourceMR == NULL)
            sourceMR = generateS390RightAlignedMemoryReference(sourceNode, valueStorageReference, cg);
         cg->correctBadSign(sourceNode, bcdValueReg, sourceSize, sourceMR);
         }
      else
         {
         // when zapping a field against itself then we may be able to reduce the destSize if some of the upper bytes are already clear
         if (isByteWidening)
            {
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\t\tdestSize > sourceSize (%d > %d) so check valueReg->getLiveSymbolSize() %d against destSize %d before checking if the upper bytes are clear\n",
                  destSize,sourceSize,valueReg->getLiveSymbolSize(),destSize);
            if (valueReg->getBytesToClear(sourceSize, destSize) == 0)
               {
               zapDestSize=sourceSize;
               if (cg->traceBCDCodeGen())
                  traceMsg(comp,"\t\tvalueReg bytes sourceSize->destSize (%d->%d) are already clear so set zapDestSize=sourceSize=%d\n",sourceSize,destSize,sourceSize);
               }
            }
         cg->correctBadSign(node, bcdValueReg, zapDestSize, targetMR);
         // save the dead/ignored bytes here as it will be reset to 0 if savePreZappedValue is true as part of the setStorageReference call below
         int32_t savedRightAlignedDeadAndIgnoredBytes = valueReg->getRightAlignedDeadAndIgnoredBytes();
         if (savePreZappedValue)
            {
            TR_StorageReference *valueStorageReferenceCopy = TR_StorageReference::createTemporaryBasedStorageReference(sourceSize, comp);
            // when tempStorageReference != NULL then the valueReg->setStorageReference call below will not work as the temp ref count will underflow
            // valueReg in this case is actually pointing to the tempRegister created when copyMR was initialized
            // shouldn't reach here in this case as tempStorageReference is only used for the uninit and non-hint cases and this is an init path
            TR_ASSERT(tempStorageReference == NULL,"tempStorageReference == NULL should be null for node %p\n",node);
            valueReg->setStorageReference(valueStorageReferenceCopy, valueChild);
            valueReg->setIsInitialized();
            valueStorageReference = valueStorageReferenceCopy;
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\tsavePreZappedValue=true so gen MVC with sourceSize %d to copy #%d on pdstore for valueChild %p with refCnt %d\n",
                  sourceSize,valueStorageReferenceCopy->getReferenceNumber(),valueChild,valueChild->getReferenceCount());
            TR::MemoryReference *targetCopyMR = generateS390RightAlignedMemoryReference(*targetMR, node, 0, cg);
            if (savedRightAlignedDeadAndIgnoredBytes > 0)
               {
               if (cg->traceBCDCodeGen())
                  traceMsg(comp,"\tadd -savedRightAlignedDeadAndIgnoredBytes = -%d to sourceMR for savePreZappedValue copy\n",savedRightAlignedDeadAndIgnoredBytes);
               targetCopyMR->addToTemporaryNegativeOffset(node, -savedRightAlignedDeadAndIgnoredBytes, cg);
               }
            generateSS1Instruction(cg, TR::InstOpCode::MVC, node,
                                   sourceSize-1,
                                   generateS390RightAlignedMemoryReference(valueChild, valueStorageReferenceCopy, cg),
                                   targetCopyMR);

            }
         sourceMR = generateS390RightAlignedMemoryReference(*targetMR, node, 0, cg);   // ensure sourceMR and targetMR are the same when used for the ZAP below

         if (savedRightAlignedDeadAndIgnoredBytes > 0)
            {
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\tadd -savedRightAlignedDeadAndIgnoredBytes = -%d to sourceMR for final ZAP\n",savedRightAlignedDeadAndIgnoredBytes);
            sourceMR->addToTemporaryNegativeOffset(node, -savedRightAlignedDeadAndIgnoredBytes, cg);
            }

         sourceNodeForZAP = node; // so a NULL sourceNode is not passed in for the ZAP sourceMR reuse below
         }

      if (isByteTruncation)
         {
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tisByteTruncating ZAP so reduce sourceSize %d->%d\n",sourceSize,zapDestSize);
         sourceSize = zapDestSize;
         }

      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tgen ZAP with zapDestSize=%d,sourceSize=%d\n",zapDestSize,sourceSize);
      generateSS2Instruction(cg, TR::InstOpCode::ZAP, node,
                             zapDestSize-1,
                             reuseS390RightAlignedMemoryReference(targetMR, node, targetStorageReference, cg),
                             sourceSize-1,
                             reuseS390RightAlignedMemoryReference(sourceMR, sourceNodeForZAP, valueStorageReference, cg));
      }
   else
      {
      if (sourceNode)
         {
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tuseZAP=false and sourceNode %s (%p) is non-NULL so gen MVC but first determine the mvcSize\n",
               sourceNode->getOpCode().getName(),sourceNode);
         int32_t mvcSize = sourceSize;
         if (isByteTruncation)
            {
            mvcSize = destSize;
            }
         bool needsClear = false;
         if (isByteWidening)
            {
            needsClear = true;
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\t\tdestSize > sourceSize (%d > %d) so try to reduce mvcSize by checking if the upper bytes are clear\n",
                  destSize,sourceSize,valueReg->getLiveSymbolSize(),destSize);
            if (valueReg->getBytesToClear(sourceSize, destSize) == 0)
               {
               needsClear=false;
               mvcSize=destSize;
               if (cg->traceBCDCodeGen())
                  traceMsg(comp,"\t\tvalueReg bytes sourceSize->destSize (%d->%d) are already clear so set mvcSize=destSize=%d\n",sourceSize,destSize,mvcSize);
               }
            }

         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tsourceNode %s (%p) is non-NULL so gen MVC/memcpy with size %d to store (isByteTruncation=%s)\n",
               sourceNode->getOpCode().getName(),sourceNode,mvcSize,isByteTruncation?"yes":"no");

         if (isBCD)
            {
            targetMR = generateS390RightAlignedMemoryReference(node, targetStorageReference, cg);
            if (sourceMR == NULL)
               sourceMR = generateS390RightAlignedMemoryReference(sourceNode, valueStorageReference, cg);
            }
         else
            {
            targetMR = generateS390MemRefFromStorageRef(node, targetStorageReference, cg);
            if (sourceMR == NULL)
               sourceMR = generateS390MemRefFromStorageRef(sourceNode, valueStorageReference, cg);
            }

         // if getRightAlignedIgnoredBytes > - then the rightmost bytes will not be coincident so the addressesMatch check is not sufficient
         // to detect if the copyIsRedundant
         //
         // Similarly if the node and storageRefNode sizes do not match (!allSizesMatch) then different offset bumps will be applied even if their starting addresses
         // are coincident (i.e. loadOrStoreAddressesMatch would return true)
         bool copyIsRedundant = valueReg->getRightAlignedIgnoredBytes() == 0 &&
                                allSizesMatch &&
                                valueStorageReference->isNonConstantNodeBased() &&
                                cg->loadOrStoreAddressesMatch(node, valueStorageReference->getNode());
         if (cg->traceBCDCodeGen() && copyIsRedundant)
            traceMsg(comp,"\t\tcopyIsRedundant=yes so skip memcpy\n");
         if (!copyIsRedundant)
            cg->genMemCpy(targetMR, node, sourceMR, sourceNode, mvcSize);

         if (needsClear)
            {
            cg->widenBCDValue(node, NULL, valueReg->getSize(), node->getSize(), targetMR);
            evaluatedPaddingAnchor = true;
            }
         }
      else if (isByteWidening)
         {
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tuseZAP=false and sourceNode is NULL so just check if upper bytes need to be cleared\n");
         targetMR = generateS390RightAlignedMemoryReference(node, targetStorageReference, cg);
         cg->widenBCDValueIfNeeded(node, bcdValueReg, sourceSize, node->getSize(), targetMR);
         evaluatedPaddingAnchor = true;
         }
      }

   if (valueChild->getReferenceCount() > 1)
      {
      if (changeCommonedChildAddress)
         {
         int32_t savedLeftAlignedZeroDigits = valueReg->getLeftAlignedZeroDigits();
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tchangeCommonedChildAddress=true so update storage reference on valueReg %s (leftAlignedZeroDigits=%d) and reset isInit to false\n",
               cg->getDebug()->getName(valueReg),savedLeftAlignedZeroDigits);

         valueReg->setStorageReference(targetStorageReference, valueChild); // also resets leftAlignedZeroDigits

         // Reset isInit to false for correctness so the commoned reference does not clobber a user variable location
         // This reset is also done during addStorageReferenceHints but there is no guarantee this pass will be done for every
         // IL pattern
         if (!targetStorageReference->isTemporaryBased())
            valueReg->setIsInitialized(false);

         if (isByteWidening)
            {
            bcdValueReg->addRangeOfZeroBytes(sourceSize, destSize);
            }
         else if (savedLeftAlignedZeroDigits > 0)
            {
            // TODO: is the size check below needed? -- isByteWidening is checked in the if above and isByteTruncation would never happen for an accum case
            if (childContainsAccumulatedResult &&
                valueReg->getSize() == node->getSize())
               {
               if (cg->traceBCDCodeGen())
                  traceMsg(comp,"\tset leftAlignedZeroDigits to %d on %s after setStorageReference\n",savedLeftAlignedZeroDigits,cg->getDebug()->getName(valueReg));
               valueReg->setLeftAlignedZeroDigits(savedLeftAlignedZeroDigits);
               }
            else
               {
               // could also probably transfer savedLeftAlignedZeroDigits in some non-accum cases too but need to see a motivating case first
               if (cg->traceBCDCodeGen())
                  traceMsg(comp,"z^z : missed transferring zeroDigits %d to valueChild %s (%p) (accum=%s, valueRegSize %d, nodeSize %d\n",
                     savedLeftAlignedZeroDigits,valueChild->getOpCode().getName(),valueChild,childContainsAccumulatedResult?"yes":"no",valueReg->getSize(),node->getSize());
               }
            }

         if (useZAP)
            {
            bcdValueReg->setHasKnownValidSignAndData();
            bcdValueReg->setHasKnownCleanSign();
            TR_ASSERT(!bcdValueReg->hasKnownOrAssumedSignCode() || bcdValueReg->getKnownOrAssumedSignCode() != 0xf,"inconsistent sign code of 0xf found for node %p\n",valueChild);
            if (cg->traceBCDCodeGen()) traceMsg(comp,"\t\tsetting HasKnownCleanSign (due to ZAP) on valueReg %s on valueChild %p\n",cg->getDebug()->getName(bcdValueReg),valueChild);
            }
         }
      else if (mustPrivatizeValueChild ||
               (!valueStorageReference->isTemporaryBased() &&                                                 // comment1 below
                childContainsAccumulatedResult &&                                                            // comment2 below
                (!node->skipCopyOnStore() || isLeadingSignByteWidening)))                                    // comments 2 and 3 below
         {
         // comment1 (explains the first case where a temp copy is *not* needed)
         // do not generate another temp copy if storing a temp that is already attached to a commoned load or pass thru node
         //    pdstore
         //       =>ipdload (in temp1), skipSSCopy=false  <- temp1 will have the correct ref count for all its commoned uses
         //
         // comment2 (explains the second case where a temp copy is *not* needed)
         // pdstore
         //    =>pdshr
         // here the pdshr storageReference is store based as the result of the initial (an earlier) store of the same pdshr node being marked with skipCopyOnStore.
         // In this case all commoned references of pdshr can use the store based storageReference as this flag guarantees the store symbol is
         // not killed before the last reference to the pdshr is seen.
         // comment3
         // skipCopyOnStore does not consider kills of the value that happen during the store itself. When storing a value
         // with a leading sign, if we have to widen that value, we move the sign code. This causes later uses of the value
         // child to see the wrong result unless we make a copy, so we ignore skipCopyOnStore if isLeadingSignByteWidening.

         TR_StorageReference *valueStorageReferenceCopy = TR_StorageReference::createTemporaryBasedStorageReference(sourceSize, comp);
         // when tempStorageReference != NULL then the valueReg->setStorageReference call below will not work as the temp ref count will underflow
         // valueReg in this case is actually pointing to the tempRegister created when copyMR was initialized
         // shouldn't reach here in this case as tempStorageReference is only used for the uninit and non-hint cases and this is hint path
         TR_ASSERT(tempStorageReference == NULL,"tempStorageReference == NULL should be null for node %p\n",node);
         valueReg->setIsInitialized();

         // do not clean sign for the BCD copy as the commoned use may not be a final use (so the sign cleaning may be premature)
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tlate pdstore privatization of valueChild : so gen MVC/memcpy with sourceSize %d to copy #%d (%s) on %s for child %s (%p) with refCnt %d (mustPrivatizeValueChild %s)\n",
               sourceSize,valueStorageReferenceCopy->getReferenceNumber(),cg->getDebug()->getName(valueStorageReferenceCopy->getSymbol()),
               node->getOpCode().getName(),valueChild->getOpCode().getName(),valueChild,valueChild->getReferenceCount(),
               mustPrivatizeValueChild?"yes":"no");

         bool useSourceMR = sourceMR && !overlapZAPIsAllowed;

         TR::Node *copySourceNode = useSourceMR ? valueChild : node;
         TR::MemoryReference *copySourceMR = useSourceMR ? sourceMR : targetMR;
         TR_StorageReference *copySourceStorageRef = useSourceMR ? valueStorageReference : targetStorageReference;

         TR::MemoryReference *copyTargetMR = NULL;
         if (isBCD)
            {
            copySourceMR = reuseS390RightAlignedMemoryReference(copySourceMR, copySourceNode, copySourceStorageRef, cg);
            valueReg->setStorageReference(valueStorageReferenceCopy, valueChild);
            copyTargetMR = generateS390RightAlignedMemoryReference(valueChild, valueStorageReferenceCopy, cg);
            }
         else
            {
            copySourceMR = reuseS390MemRefFromStorageRef(copySourceMR, 0, copySourceNode, copySourceStorageRef, cg);
            valueReg->setStorageReference(valueStorageReferenceCopy, valueChild);
            copyTargetMR = generateS390MemRefFromStorageRef(valueChild, valueStorageReferenceCopy, cg);
            }

         cg->genMemCpy(copyTargetMR, node, copySourceMR, copySourceNode, sourceSize);

         if (useSourceMR)
            sourceMR = copySourceMR;
         else
            targetMR = copySourceMR;

         // If we are accumulating a leading sign type, then the above copy will include the
         // byte widening that we did before storing. The long-term fix is to rewrite this evaluator
         // to make the copy before we do any modification of the stored value.
         // The short term fix is to copy the widened sign back into this copy.
         if (childContainsAccumulatedResult && isLeadingSignByteWidening)
            {
            uint16_t signSize = 0;
            TR::InstOpCode::Mnemonic signCopyOp = TR::InstOpCode::BAD;

            switch (node->getType().getDataType())
               {
               case TR::ZonedDecimalSignLeadingEmbedded:
                  signSize = 1;
                  signCopyOp = TR::InstOpCode::MVZ;
                  break;
               case TR::ZonedDecimalSignLeadingSeparate:
                  signSize = 1;
                  signCopyOp = TR::InstOpCode::MVC;
                  break;
               case TR::UnicodeDecimalSignLeading:
                  signSize = 2;
                  signCopyOp = TR::InstOpCode::MVC;
                  break;
               default:
                  TR_ASSERT(0, "unknown leading sign type in pdStoreEvaluator");
               }

            TR::MemoryReference *originalSignCodeMR =
               reuseS390LeftAlignedMemoryReference(targetMR, node, targetStorageReference, cg, node->getSize());

            TR::MemoryReference *copyMR =
               reuseS390LeftAlignedMemoryReference(copyTargetMR, valueChild, valueStorageReferenceCopy, cg, sourceSize);

            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\tAccumulating a leading sign type: have to restore the sign code for the copy: signSize %d\n",
                        signSize);


            generateSS1Instruction(cg, signCopyOp, node,
                                   signSize-1,
                                   copyMR,
                                   originalSignCodeMR);

            }
         }
      }

   rcount_t finalValueChildRefCount = valueChild->getReferenceCount();
   if (changeCommonedChildAddress &&
       finalValueChildRefCount != origValueChildRefCount)
      {
      // In this case the addressChild and the valueChild share a commoned node.
      // This will cause the addressChild evaluation (done as part of getting targetMR) to be an impliedMemoryReference and
      // the aiadd will be incremented by one (in anticipation of the valueChild using the targetStorageRef going forward)
      // In the trivial case where this future use is only under the current store ( == 1 check below) then have to take care to do the final
      // recDec of the addressChild to remove the extra increment done when forming the targetMR.
      //
      // izdstore
      //    aiadd
      //       ...
      //          zdload
      //    =>zdload
      //
      TR_ASSERT(finalValueChildRefCount > 0 && finalValueChildRefCount < origValueChildRefCount,
         "finalValueChildRefCount %d must be > 0 and less than origValueChildRefCount %d on store %p\n",finalValueChildRefCount,origValueChildRefCount,node);
      // the only way the refCounts can be not equal is if we evaluated a targetMR
      TR_ASSERT(targetMR,"finalValueChildRefCount %d must be equal to origValueChildRefCount %d if targetMR is non-NULL on store %p\n",finalValueChildRefCount,origValueChildRefCount,node);
      if (isIndirect && finalValueChildRefCount == 1)
         {
         // only remaining use is as the valueChild of this very store so must do the final recDec of the addressChild
         // a recDec is safe here as the targetMR would have already privatized any loads in the address child to registers
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tfinalValueChildRefCount < origValueChildRefCount (%d < %d) and is 1 so recursively dec addrChild %s (%p) %d->%d\n",
               finalValueChildRefCount,origValueChildRefCount,
               node->getFirstChild()->getOpCode().getName(),
               node->getFirstChild(),
               node->getFirstChild()->getReferenceCount(),node->getFirstChild()->getReferenceCount()-1);
         cg->recursivelyDecReferenceCount(node->getFirstChild());
         }
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tfinalValueChildRefCount < origValueChildRefCount (%d < %d) decrement the targetStorageReference nodeRefCount by the difference %d->%d\n",
            finalValueChildRefCount,origValueChildRefCount,
            targetStorageReference->getNodeReferenceCount(),targetStorageReference->getNodeReferenceCount()-(origValueChildRefCount-finalValueChildRefCount));
      // the valueChild may be commoned more than once under the addressChild of the store so dec by the difference of the before and after refCounts
      targetStorageReference->decrementNodeReferenceCount(origValueChildRefCount-finalValueChildRefCount);
      }

   if (targetMR == NULL)
      {
      if (isIndirect)
         {
         // if changeCommonedChildAddress=true then we must not decrement the addressChild as it will be needed for future commoned references
         // to the valueChild
         // a recDec is safe here as the only way no store can be done (targetMR==NULL case) is when valueChildren have already privatized
         // any loads in the address child to registers when accumulating to the final store location
         if (!changeCommonedChildAddress)
            {
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\tno explicit store inst and changeCommonedChildAddress=false so recursively dec addrChild %p %d->%d\n",
                  node->getFirstChild(),node->getFirstChild()->getReferenceCount(),node->getFirstChild()->getReferenceCount()-1);
            cg->recursivelyDecReferenceCount(node->getFirstChild());
            }
         else
            {
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\tno explicit store inst and changeCommonedChildAddress=true so do NOT recursively dec addrChild %p (refCount stays at %d)\n",
                  node->getFirstChild(),node->getFirstChild()->getReferenceCount());
            }
         }
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tno explicit store inst so decrement the targetStorageReference nodeRefCount %d->%d\n",
            targetStorageReference->getNodeReferenceCount(),targetStorageReference->getNodeReferenceCount()-1);
      targetStorageReference->decrementNodeReferenceCount();
      }

   if (!evaluatedPaddingAnchor)
      cg->processUnusedNodeDuringEvaluation(NULL);

   cg->decReferenceCount(valueChild);
   return NULL;
   }

/**
 * This only handles pdstore and pdstorei.
 * Other types of stores (zd, ud) can't use vector instructions.
*/
TR::Register*
J9::Z::TreeEvaluator::pdstoreVectorEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   traceMsg(cg->comp(), "DAA: Entering pdstoreVectorEvaluator %d\n", __LINE__);
   TR::Compilation *comp = cg->comp();
   TR::Node * valueChild = node->getValueChild();
   TR::Node* addressNode = node->getChild(0);
   // evaluate valueChild (which is assumed by the OMR layer to be the second child) to Vector register.
   // for this "pdStore" we assume if we evaluate value node we get Vector Register
   TR::Register* pdValueReg = cg->evaluate(valueChild);

   TR_ASSERT((pdValueReg->getKind() == TR_FPR || pdValueReg->getKind() == TR_VRF),
             "vectorized pdstore is expecting its value in a vector register.");

   if (cg->traceBCDCodeGen())
      {
      traceMsg(comp,"generating VSTRL for pdstore node->size = %d.\n", node->getSize());
      }

   // No need to evaluate the address node of the pdstorei.
   // generateVSIInstruction() API will call separateIndexRegister() to separate the index
   // register by emitting an LA instruction. If there's a need for large displacement adjustment,
   // LAY will be emitted instead.
   TR::MemoryReference * targetMR = generateS390MemoryReference(node, cg, false);

   // 0 we store 1 byte, 15 we store 16 bytes
   uint8_t lengthToStore = TR_VECTOR_REGISTER_SIZE - 1;
   if (node->getDecimalPrecision() > TR_MAX_INPUT_PACKED_DECIMAL_PRECISION )
      {
      targetMR->addToOffset(node->getSize() - TR_VECTOR_REGISTER_SIZE);
      }
   else
      {
      lengthToStore = node->getSize() - 1;
      }

   generateVSIInstruction(cg, TR::InstOpCode::VSTRL, node, pdValueReg, targetMR, lengthToStore);
   cg->decReferenceCount(valueChild);
   cg->decReferenceCount(addressNode);

   traceMsg(cg->comp(), "DAA: Exiting pdstoreVectorEvaluator %d\n", __LINE__);
   return NULL;
   }

TR_PseudoRegister * J9::Z::TreeEvaluator::evaluateBCDSignModifyingOperand(TR::Node *node,
                                                                              bool isEffectiveNop,
                                                                              bool isNondestructiveNop,
                                                                              bool initTarget,
                                                                              TR::MemoryReference *sourceMR,
                                                                              TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getType().isBCD(),"node %p type %s must be BCD\n",node,node->getDataType().toString());
   TR_OpaquePseudoRegister *reg = evaluateSignModifyingOperand(node, isEffectiveNop, isNondestructiveNop, initTarget, sourceMR, cg);
   TR_PseudoRegister *pseudoReg = reg->getPseudoRegister();
   TR_ASSERT(pseudoReg,"pseudoReg should be non-NULL for node %p\n",node);
   return pseudoReg;
   }


TR_OpaquePseudoRegister * J9::Z::TreeEvaluator::evaluateSignModifyingOperand(TR::Node *node,
                                                                                 bool isEffectiveNop,
                                                                                 bool isNondestructiveNop,
                                                                                 bool initTarget,
                                                                                 TR::MemoryReference *sourceMR,
                                                                                 TR::CodeGenerator *cg)
   {
   bool isBCD = node->getType().isBCD();
   TR::Node *child = node->getFirstChild();
   TR_OpaquePseudoRegister *firstReg = cg->evaluateOPRNode(child);
   TR::Compilation *comp = cg->comp();

   if (isBCD)
      TR_ASSERT(firstReg->getPseudoRegister(),"firstReg->getPseudoRegister() is null in evaluateSignModifyingOperand for BCD node %p\n",child);

   if (cg->traceBCDCodeGen())
      {
      if (isBCD)
         traceMsg(comp,"\tevaluateSignModOperand %s (%p) : firstReg %s firstReg->getPseudoRegister()->prec %d (isInit %s, isLegalToCleanSign %s, isEffectiveNop %s, initTarget %s)\n",
            node->getOpCode().getName(),node,cg->getDebug()->getName(firstReg),firstReg->getPseudoRegister()->getDecimalPrecision(),
            firstReg->isInitialized() ? "yes":"no",firstReg->getPseudoRegister()->isLegalToCleanSign()? "yes":"no",isEffectiveNop ? "yes":"no",initTarget ? "yes":"no");
      else
         traceMsg(comp,"\tevaluateSignModOperand for aggr type %s (%p) : firstReg %s (isInit %s, isEffectiveNop %s, initTarget %s)\n",
            node->getOpCode().getName(),node,cg->getDebug()->getName(firstReg),
            firstReg->isInitialized() ? "yes":"no",isEffectiveNop ? "yes":"no",initTarget ? "yes":"no");
      }

   TR_OpaquePseudoRegister *targetReg = NULL;

   // Note that a clobber evaluate must be done for any initialized firstReg -- even in the effectiveNop case:
   // 2  pdclean  <- (isEffectiveNop=true) (temp1)
   // 1     pdremSelect <- node (isEffectiveNop=true) (temp1)
   // 2        pddivrem <- child (temp1)
   // ...
   //    pdshr (clobbers temp1)
   //       =>pddivrem (temp1)
   // ...
   //    =>pdclean (uses invalid clobbered temp1 - wrong)
   // if a clobber evaluate is *not* done and temp1 is used for the pdremSelect and the pdclean then the parent of the second reference to the pddivrem node
   // will clobber temp1 and subsequent references to pdclean (and pdremSelect if any) will use the incorrectly clobbered temp1.
   // The clobber evaluate will copy the pddivrem result in temp1 to temp2 and the commoned pdclean will use the (now unclobbered) temp1
   // TODO: an alternative fix would be to *not* clobber evaluate for the isEffectiveNop=true case but to instead allocate and mark a new register as read-only
   // for the commoned pddivrem but clobberable for the pdremSelect and pdclean (basically do a clobber evaluate but don't generate an MVC to copy the value).
   // Doing the MVC copy lazily by any later consumer (the pdshr) would likely be better in some cases.
   // UPDATE: the above TODO is complete as part of ReadOnlyTemporary sets done below
   bool resetReadOnly = true;
   if (isEffectiveNop)
      {
      resetReadOnly = false;
      targetReg = isBCD? cg->allocatePseudoRegister(firstReg->getPseudoRegister()) : cg->allocateOpaquePseudoRegister(firstReg);

      if (isBCD && (node->getDecimalPrecision() < firstReg->getPseudoRegister()->getDecimalPrecision()) &&
          (!firstReg->getPseudoRegister()->hasKnownOrAssumedSignCode() || (firstReg->getPseudoRegister()->getKnownOrAssumedSignCode() != TR::DataType::getPreferredPlusCode())))
         {
         // on a truncation of a value with an unknown or negative sign code then conservatively set clean to false as negative zero (unclean) may be produced
         targetReg->getPseudoRegister()->resetCleanSign();
         }
      TR_StorageReference *firstStorageReference = firstReg->getStorageReference();
      // transfer the zeroDigits/deadBytes and cache the firstReg->getStorageReference() *before* calling ssrClobberEvaluate in case
      // a new storage reference set on firstReg causes these values to be reset
      targetReg->setLeftAlignedZeroDigits(firstReg->getLeftAlignedZeroDigits());
      targetReg->setRightAlignedDeadBytes(firstReg->getRightAlignedDeadBytes());
      targetReg->setRightAlignedIgnoredBytes(firstReg->getRightAlignedIgnoredBytes());
      if (cg->traceBCDCodeGen())
         {
         traceMsg(comp,"\t * setting rightAlignedDeadBytes %d from firstReg %s to targetReg %s (signMod nop)\n",
            firstReg->getRightAlignedDeadBytes(),cg->getDebug()->getName(firstReg),cg->getDebug()->getName(targetReg));
         traceMsg(comp,"\t * setting rightAlignedIgnoredBytes %d from firstReg %s to targetReg %s (signMod nop)\n",
            firstReg->getRightAlignedIgnoredBytes(),cg->getDebug()->getName(firstReg),cg->getDebug()->getName(targetReg));
         if (isBCD)
            traceMsg(comp,"\t * setting savedLeftAlignedZeroDigits %d from firstReg %s to targetReg %s (signMod nop)\n",
               firstReg->getLeftAlignedZeroDigits(),cg->getDebug()->getName(firstReg),cg->getDebug()->getName(targetReg));
         }

      if (firstReg->isInitialized())
         {
         // The extra work to allow this for non-temp based is to expand the skipCopyOnStore check to all nodes (i.e. do not restrict this flag to those directly under a store node).
         // This skipCopyOnStore analysis will then guarantee that the underlying non-temp variable is not killed before its next use(s).
         if (!comp->getOption(TR_DisableRefinedBCDClobberEval) && firstStorageReference->isTemporaryBased() && isNondestructiveNop)
            {
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"%sskipping ssrClobberEvaluate for %s (%p) with child %s (%p) refCount %d %s 1 owningRegisterCount %d %s 1-- %s mark #%d (%s) as readOnlyTemp (nondestructive nop case)\n",
                        child->getReferenceCount() > 1 ? "y^y : ":"",
                        node->getOpCode().getName(),node,child->getOpCode().getName(),child,
                        child->getReferenceCount(),child->getReferenceCount() > 1 ? ">":"<=",
                        firstStorageReference->getOwningRegisterCount(), firstStorageReference->getOwningRegisterCount() > 1 ? ">" : "<=",
                        child->getReferenceCount() > 1 ? "do":"do not",firstStorageReference->getReferenceNumber(),
                  cg->getDebug()->getName(firstStorageReference->getSymbol()));

            if (child->getReferenceCount() > 1 || firstStorageReference->getOwningRegisterCount() > 1)
               {
               firstStorageReference->setIsReadOnlyTemporary(true, child);
               }
            resetReadOnly = false;
            }
         else
            {
            cg->ssrClobberEvaluate(child, sourceMR);
            }
         }

      // transfer the storageRef *after* calling ssrClobberEvaluate so the referenceCounts of the temporaries are set correctly
      TR_StorageReference *targetStorageReference = firstStorageReference;
      targetReg->setStorageReference(targetStorageReference, node);
      if (!firstReg->isInitialized() && targetStorageReference->isNodeBased())
         {
         // NodeReferenceCounts are not used for node based hints and this path should never be reached for these hints
         // as this type of storage reference is only used when it has been initialized
         TR_ASSERT( !targetStorageReference->isNodeBasedHint(),"a node based hint should have been initialized\n");
         // This is the case where the firstChild is likely an ipdload (or a pdclean of ipdload etc)
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tisEffectiveNop=yes and firstReg->isInit=false case so increment the targetStorageReference nodeRefCount by (node->refCount() - 1) = %d : %d->%d\n",
               node->getReferenceCount()-1,
               targetStorageReference->getNodeReferenceCount(),
               targetStorageReference->getNodeReferenceCount()+(node->getReferenceCount()-1));
         targetStorageReference->incrementNodeReferenceCount(node->getReferenceCount()-1);
         cg->privatizeStorageReference(node, targetReg, NULL);
         }
      }
   else if (firstReg->isInitialized())
      {
      TR_ASSERT( isBCD, "this path should only be taken for BCD nodes (unless we extend support for aggr types)\n");
      TR_StorageReference *firstStorageReference = firstReg->getStorageReference();
      // An initialized reg cannot have a non-hint node based storage reference as these would come from an ipdload node and pdload's never initialize a register
      TR_ASSERT( firstStorageReference->isTemporaryBased() || firstStorageReference->isNodeBasedHint(),"expecting the initalized firstReg to be either a temp or a node based hint\n");
      targetReg = cg->allocatePseudoRegister(node->getDataType());
      // transfer the zeroDigits/deadBytes and cache the firstReg->getStorageReference() *before* calling ssrClobberEvaluate in case
      // a new storage reference set on firstReg causes these values to be reset
      targetReg->setLeftAlignedZeroDigits(firstReg->getLeftAlignedZeroDigits());
      targetReg->setRightAlignedDeadBytes(firstReg->getRightAlignedDeadBytes());
      targetReg->setRightAlignedIgnoredBytes(firstReg->getRightAlignedIgnoredBytes());
      targetReg->getPseudoRegister()->transferDataState(firstReg->getPseudoRegister());
      if (cg->traceBCDCodeGen())
         {
         traceMsg(comp,"\t * setting rightAlignedDeadBytes %d from firstReg %s to targetReg %s (signMod isInit)\n",
            firstReg->getRightAlignedDeadBytes(),cg->getDebug()->getName(firstReg),cg->getDebug()->getName(targetReg));
         traceMsg(comp,"\t * setting rightAlignedIgnoredBytes %d from firstReg %s to targetReg %s (signMod isInit)\n",
            firstReg->getRightAlignedIgnoredBytes(),cg->getDebug()->getName(firstReg),cg->getDebug()->getName(targetReg));
         traceMsg(comp,"\t * setting savedLeftAlignedZeroDigits %d from firstReg %s to targetReg %s (signMod isInit)\n",
            firstReg->getLeftAlignedZeroDigits(),cg->getDebug()->getName(firstReg),cg->getDebug()->getName(targetReg));

         }

      if (!comp->getOption(TR_DisableRefinedBCDClobberEval) && firstReg->canBeConservativelyClobberedBy(node))
         {
         //    pdclean
         // 3    pdadd
         //
         //    AP    t1,t2
         //    ZAP   t1,t1 // this ZAP is a conservative clobber as it will not modify the value in pdadd and there are no special sign codes to be preserved
         //
         //    the t1 storageReference will be marked as readOnly and pdadd added to nodeToUpdateOnClobber list so if/when t1 is actually clobbered the commoned
         //    register/node can have its storageRef updated to point to the saved value.
         //
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"%sskipping ssrClobberEvaluate for %s (%p) with child %s (%p) refCount %d %s 1 owningRegisterCount %d %s 1-- %s mark #%d (%s) as readOnlyTemp (isInit case)\n",
                     child->getReferenceCount() > 1 ? "y^y : ":"",
                     node->getOpCode().getName(),node,child->getOpCode().getName(),child,
                     child->getReferenceCount(),child->getReferenceCount() > 1 ? ">":"<=",
                     firstStorageReference->getOwningRegisterCount(), firstStorageReference->getOwningRegisterCount() > 1 ? ">" : "<=",
                     child->getReferenceCount() > 1 ? "do":"do not",firstStorageReference->getReferenceNumber(),
                     cg->getDebug()->getName(firstStorageReference->getSymbol()));

         if (child->getReferenceCount() > 1 || firstStorageReference->getOwningRegisterCount() > 1)
            {
            firstStorageReference->setIsReadOnlyTemporary(true, child);
            }
         resetReadOnly = false;
         }
      else
         {
         cg->ssrClobberEvaluate(child, sourceMR);
         }

      // transfer the storageRef *after* calling ssrClobberEvaluate so the referenceCounts of the temporaries are set correctly
      targetReg->setStorageReference(firstStorageReference, node);
      targetReg->setIsInitialized();
      }
   else
      {
      TR_ASSERT( isBCD, "this path should only be taken for BCD nodes (unless we extend support for aggr types)\n");
      targetReg = cg->allocatePseudoRegister(node->getDataType());
      TR_StorageReference *targetStorageReference = NULL;
      if (node->getOpCode().canHaveStorageReferenceHint() && node->getStorageReferenceHint())
         targetStorageReference = node->getStorageReferenceHint();
      else
         targetStorageReference = TR_StorageReference::createTemporaryBasedStorageReference(node->getStorageReferenceSize(), comp);
      targetReg->setStorageReference(targetStorageReference, node);
      if (initTarget)
         {
         int32_t srcLiveSymbolSize = firstReg->getLiveSymbolSize();
         int32_t targetLiveSymbolSize = targetReg->getLiveSymbolSize();
         int32_t mvcSize = node->getSize();
         bool isTruncation = node->getSize() < firstReg->getSize();
         // if there are some left aligned zero digits in the source then increase the mvcSize to capture these in the initializing MVC
         if (firstReg->trackZeroDigits() &&
             (targetLiveSymbolSize == srcLiveSymbolSize) &&
             (srcLiveSymbolSize > mvcSize) &&
             (firstReg->getBytesToClear(mvcSize, srcLiveSymbolSize) == 0))
            {
            // increasing the mvcSize to include already zero'd bytes is illegal if targetLiveSymbolSize < srcLiveSymbolSize and
            // legal if targetLiveSymbolSize>=srcLiveSymbolSize but pointless if targetLiveSymbolSize > srcLiveSymbolSize as the extra
            // zero bytes will not be tracked on the targetReg so only do this when targetLiveSymbolSize == srcLiveSymbolSize
            //
            // In this case the source register has some zero bytes above its register size so increase the MVC size to include these zero bytes
            // e.g. if targetReg->getSize()=6 but the childLiveSymbolSize=9 then increase the mvcSize by 3 to 9
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\tupper %d bytes on srcReg %s are already clear so set mvcSize=%d\n", srcLiveSymbolSize-mvcSize,cg->getDebug()->getName(firstReg),srcLiveSymbolSize);
            targetReg->addRangeOfZeroBytes(mvcSize,srcLiveSymbolSize);
            mvcSize = srcLiveSymbolSize;
            }
         else if (!isTruncation)   // on a widening only initialize up to the source size
            {
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\tfirstReg->getSize() <= node->getSize() (%d <= %d) so reduce mvcSize\n",firstReg->getSize(),node->getSize());
            mvcSize = firstReg->getSize();
            }

         if (isTruncation && node->getType().isSeparateSign())
            {
            mvcSize -= node->getDataType().separateSignSize();
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\tnode %s is a truncating separateSign type so reduce mvcSize by sign size (%d->%d)\n",
                  node->getOpCode().getName(),mvcSize+node->getDataType().separateSignSize(),mvcSize);
            }

         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tfirstReg->isInitialized()==false so gen MVC to init with mvcSize %d\n", mvcSize);
         TR_ASSERT( sourceMR,"source memory reference should have been created by caller\n");
         generateSS1Instruction(cg, TR::InstOpCode::MVC, node,
                                mvcSize-1,
                                generateS390RightAlignedMemoryReference(node, targetStorageReference, cg),
                                generateS390RightAlignedMemoryReference(*sourceMR, node, 0, cg));
         targetReg->getPseudoRegister()->transferDataState(firstReg->getPseudoRegister());
         targetReg->setIsInitialized();
         }
      }

   if (isEffectiveNop || firstReg->isInitialized())
      cg->freeUnusedTemporaryBasedHint(node);

   if (firstReg->getSize() < node->getSize())
      {
      TR_ASSERT( isBCD, "this path should only be taken for BCD nodes (unless we extend support for aggr types)\n");
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\twidening: firstRegSize < nodeSize (%d < %d) so set targetReg->getPseudoRegister()->prec to firstReg->prec (%d)\n",firstReg->getSize(), node->getSize(),firstReg->getPseudoRegister()->getDecimalPrecision());
      targetReg->getPseudoRegister()->setDecimalPrecision(firstReg->getPseudoRegister()->getDecimalPrecision());
      }

   if (cg->traceBCDCodeGen() && targetReg->getStorageReference()->isReadOnlyTemporary())
      traceMsg(comp,"%sreset readOnlyTemp flag on storageRef #%d (%s) (signMod case)\n",
         resetReadOnly?"":"do not ",targetReg->getStorageReference()->getReferenceNumber(),cg->getDebug()->getName(targetReg->getStorageReference()->getSymbol()));

   if (resetReadOnly)
      targetReg->getStorageReference()->setIsReadOnlyTemporary(false, NULL);

   node->setRegister(targetReg);
   return targetReg;
   }

TR::Register *J9::Z::TreeEvaluator::pdSetSignHelper(TR::Node *node, int32_t sign, TR::CodeGenerator *cg)
   {
   TR::Node *srcNode = node->getFirstChild();
   TR_PseudoRegister *srcReg = cg->evaluateBCDNode(srcNode);
   TR_PseudoRegister *targetReg = NULL;

   if (node->getType().isAnyPacked())
      {
      targetReg = simpleWideningOrTruncation(node, srcReg, true, sign, cg);  // setSign=true
      }
   else if (node->getDataType() == TR::ZonedDecimal)
      {
      bool isEffectiveNop = (sign == TR::DataType::getIgnoredSignCode()) || srcReg->knownOrAssumedSignCodeIs(sign);
      TR::MemoryReference *sourceMR = NULL;
      if (!srcReg->isInitialized() && !isEffectiveNop)
         sourceMR = generateS390RightAlignedMemoryReference(srcNode, srcReg->getStorageReference(), cg);
      targetReg = evaluateBCDSignModifyingOperand(node, isEffectiveNop, isEffectiveNop, true, sourceMR, cg); // initTarget=true
      bool isTruncation = srcReg->getDecimalPrecision() > node->getDecimalPrecision();
      if (isTruncation)
         targetReg->setDecimalPrecision(node->getDecimalPrecision());
      else
         targetReg->setDecimalPrecision(srcReg->getDecimalPrecision());
      if (!isEffectiveNop)
         {
         TR_StorageReference *targetStorageReference = targetReg->getStorageReference();
         TR_StorageReference *firstStorageReference = srcReg->getStorageReference();
         TR::MemoryReference *destMR = generateS390RightAlignedMemoryReference(node, targetStorageReference, cg);
         int32_t destLength = targetReg->getSize();
         cg->genSignCodeSetting(node, targetReg, destLength, destMR, sign, srcReg, 0, false); // digitsToClear=0, numericNibbleIsZero=false
         }
      }
   else
      {
      TR_ASSERT(false,"unexpected datatype %s in pdSetSignHelper\n",node->getDataType().toString());
      }

   node->setRegister(targetReg);
   cg->decReferenceCount(srcNode);
   return targetReg;
   }

/**
 * \brief Evaluator function to evaluate pdSetSign opCode
*/
TR::Register*
J9::Z::TreeEvaluator::pdSetSignEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   cg->traceBCDEntry("pdSetSign",node);
   cg->generateDebugCounter("PD-Op/pdsetsign", 1, TR::DebugCounter::Cheap);

   TR::Register *targetReg = NULL;
   TR::Node *signNode = node->getSecondChild();

   TR_ASSERT(signNode->getOpCode().isLoadConst() && signNode->getOpCode().getSize() <= 4,
             "expecting a <= 4 size integral constant set sign amount\n");
   TR_ASSERT(node->getFirstChild()->getType().isAnyPacked(), "expecting setSign's first child of PD data type");

   int32_t sign = (int32_t)signNode->get64bitIntegralValue();
   cg->decReferenceCount(signNode);

   static char* isVectorBCDEnv = feGetEnv("TR_enableVectorBCD");
   if(TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() && !cg->comp()->getOption(TR_DisableVectorBCD) || isVectorBCDEnv)
      {
      targetReg = vectorPerformSignOperationHelper(node, cg, false, 0, node->hasKnownOrAssumedCleanSign(), SignOperationType::setSign, false, true, sign);
      }
   else
      {
      targetReg = pdSetSignHelper(node, sign, cg);
      }

   cg->traceBCDExit("pdSetSign",node);
   return targetReg;
   }

/**
 * TR::pdclear
 * TR::pdclearSetSign
 * current limitation for this is that leftMostDigit must equal digitsToClear (i.e. clearing right most digits)
 */
TR::Register *
J9::Z::TreeEvaluator::pdclearEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   cg->traceBCDEntry("pdclear",node);
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(cg->comp(), "PD-Op/%s", node->getOpCode().getName()),
                            1, TR::DebugCounter::Cheap);
   TR_ASSERT(!node->getOpCode().isSetSign(),"isSetSign on child not supported for node %s (%p)\n",node->getOpCode().getName(),node);
   bool isSetSign = node->getOpCode().isSetSignOnNode();
   TR_RawBCDSignCode setSignValue = isSetSign ? node->getSetSign() : raw_bcd_sign_unknown;
   int32_t sign = TR::DataType::getValue(setSignValue);
   TR::Compilation *comp = cg->comp();

   TR_ASSERT(!isSetSign || setSignValue != raw_bcd_sign_unknown,"setSignValue must be on the node for %p\n",node);

   TR::Node *srcNode = node->getChild(0);
   TR::Node *leftMostDigitNode = node->getChild(1);
   TR::Node *digitsToClearNode = node->getChild(2);
   TR::Node *literalAddrNode = (isSetSign && node->getNumChildren() > 3) ? node->getChild(3) : NULL;

   TR_ASSERT(leftMostDigitNode->getOpCode().isLoadConst() && leftMostDigitNode->getSize() <= 4,
      "leftMostDigitNode %p must be a <= 4 size const\n",leftMostDigitNode);
   TR_ASSERT(digitsToClearNode->getOpCode().isLoadConst() && digitsToClearNode->getSize() <= 4,
      "digitsToClearNode %p must be a <= 4 size const\n",digitsToClearNode);

   int32_t leftMostDigit = leftMostDigitNode->get32bitIntegralValue();
   int32_t leftMostByte = TR::DataType::packedDecimalPrecisionToByteLength(leftMostDigit);
   int32_t digitsToClear = digitsToClearNode->get32bitIntegralValue();
   int32_t rightMostDigit = leftMostDigit - digitsToClear;

   TR_ASSERT(leftMostDigit == digitsToClear,"leftMostDigit %d must equal digitsToClear for node %p\n",leftMostDigit,digitsToClear,node);

   TR_PseudoRegister *srcReg = cg->evaluateBCDNode(srcNode);
   bool isInitialized = srcReg->isInitialized();
   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\t%s (%p) : srcNode %s (%p) isInit=%s, digitClearRange %d->%d (leftMostByte=%d), digitsToClear = %d (isSetSign %s, sign 0x%x)\n",
         node->getOpCode().getName(),node,
         srcNode->getOpCode().getName(),srcNode,
         isInitialized ? "yes":"no",
         leftMostDigit,rightMostDigit,leftMostByte,digitsToClear,isSetSign?"yes":"no",sign);
   TR_StorageReference *srcStorageReference = srcReg->getStorageReference();
   TR::MemoryReference *sourceMR = generateS390RightAlignedMemoryReference(srcNode, srcStorageReference, cg);

   TR_PseudoRegister *targetReg = evaluateBCDValueModifyingOperand(node, true, sourceMR, cg); // initTarget=true
   TR::MemoryReference *destMR = generateS390RightAlignedMemoryReference(node, targetReg->getStorageReference(), cg);

   bool isTruncation = srcReg->getDecimalPrecision() > node->getDecimalPrecision();
   if (isTruncation)
      targetReg->setDecimalPrecision(node->getDecimalPrecision());
   else
      targetReg->setDecimalPrecision(srcReg->getDecimalPrecision());

   int32_t targetRegPrec = targetReg->getDecimalPrecision();

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tset targetReg prec to %d (isTrucation %s)\n",targetRegPrec,isTruncation?"yes":"no");

   bool truncatedIntoClearedDigits = false;
   if (targetRegPrec < leftMostDigit)
      {
      truncatedIntoClearedDigits = true;
      int32_t precDelta = leftMostDigit - targetRegPrec;
      leftMostDigit -= precDelta;
      leftMostByte = TR::DataType::packedDecimalPrecisionToByteLength(leftMostDigit);
      digitsToClear -= precDelta;
      rightMostDigit = leftMostDigit - digitsToClear;
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\ttargetRegPrec %d < leftMostDigit %d : update leftMostDigit %d->%d, leftMostByte = %d, digitsToClear %d->%d, rightMostDigit = %d\n",
            targetRegPrec,leftMostDigit+precDelta,leftMostDigit+precDelta,leftMostDigit,leftMostByte,digitsToClear+precDelta,digitsToClear,rightMostDigit);
      }

   // do not bother checking !node->canSkipPadByteClearing() below because being able to clear the full byte generally results in better codegen
   // coincidentEvenDigitCorrection is true when leftMostNibble == targetRegPrec so instead of generating separate NI 0xF0 and then NI 0x0F on the same byte
   // just inc digitsToClear below so this full byte clearing can be done in one instruction
   // e.g. p4v0 = (p15v0 / 10000) * 10000
   int32_t leftMostByteForClear = leftMostByte;
   bool needsEvenDigitCorrection = !truncatedIntoClearedDigits && isTruncation && targetReg->isEvenPrecision();
   bool coincidentEvenDigitCorrection = needsEvenDigitCorrection && (leftMostByteForClear == targetReg->getSize());
   if (isEven(leftMostDigit))
      {
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tleftMostDigit %d isEven : isInit=%s, truncatedIntoClearedDigits=%s, coincidentEvenDigitCorrection=%s -- adjust the leftMostNibble to preserve or clear the leftMostByte\n",
            leftMostDigit,isInitialized?"yes":"no",truncatedIntoClearedDigits?"yes":"no",needsEvenDigitCorrection?"yes":"no");

      if (isInitialized && !truncatedIntoClearedDigits && !coincidentEvenDigitCorrection) // full byte will be cleared if truncatedIntoClearedDigits or coincidentEvenDigitCorrection are true
         {
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\t\tisInit=yes,truncatedIntoClearedDigits=no,coincidentEvenDigitCorrection=no so dec %d->%d to preserve initialized leftMostNibble\n",digitsToClear,digitsToClear-1);
         digitsToClear--; // must preserve the top byte and then clear just the top digit after the clearAndSetSign
         leftMostByteForClear--;
         }
      else
         {
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\t\tisInit=no or truncatedIntoClearedDigits=yes or coincidentEvenDigitCorrection=yes so inc %d->%d to clear initialized leftMostNibble\n",digitsToClear,digitsToClear+1);
         digitsToClear++; // clear a larger even # of digits and put back
         }
      }

   if (!isTruncation && srcReg->isEvenPrecision() && srcReg->isLeftMostNibbleClear())
      {
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\twidening with even srcRegPrec %d update targetReg with zero range for leftMostNibble %d->%d\n",
            srcReg->getDecimalPrecision(),srcReg->getDecimalPrecision(),srcReg->getDecimalPrecision()+1);
      targetReg->addRangeOfZeroDigits(srcReg->getDecimalPrecision(),srcReg->getDecimalPrecision()+1);
      }

   // clearAndSetSign will be clearing full bytes so half byte values or signs will be put back afterwards
   clearAndSetSign(node, targetReg, leftMostByteForClear, digitsToClear, destMR, srcReg, sourceMR, isSetSign, sign, isInitialized, cg); // isSignInitialized=isInitialized

   if (!(truncatedIntoClearedDigits || coincidentEvenDigitCorrection))
      {
      if (isEven(leftMostDigit))
         {
         if (isInitialized)
            {
               {
               if (cg->traceBCDCodeGen())
                  traceMsg(comp,"\tisInit=yes : gen NI to clear right most nibble at byte %d\n",leftMostByte);
               generateSIInstruction(cg, TR::InstOpCode::NI, node,
                                     reuseS390LeftAlignedMemoryReference(destMR, node, targetReg->getStorageReference(), cg, leftMostByte),
                                     0xF0);
               }
            }
         else
            {
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\tisInit=no : gen MVZ to restore left most nibble at byte %d\n",leftMostByte);
            int32_t mvzSize = 1;
            generateSS1Instruction(cg, TR::InstOpCode::MVZ, node,
                                   mvzSize-1,
                                   reuseS390LeftAlignedMemoryReference(destMR, node, targetReg->getStorageReference(), cg, leftMostByte),
                                   reuseS390LeftAlignedMemoryReference(sourceMR, srcNode, srcStorageReference, cg, leftMostByte));
            }
         }

      if (needsEvenDigitCorrection && !node->canSkipPadByteClearing())
         cg->genZeroLeftMostPackedDigits(node, targetReg, targetReg->getSize(), 1, destMR);
      }

   cg->decReferenceCount(srcNode);
   cg->decReferenceCount(leftMostDigitNode);
   cg->decReferenceCount(digitsToClearNode);
   cg->processUnusedNodeDuringEvaluation(literalAddrNode);
   cg->traceBCDExit("pdclear",node);
   return targetReg;
   }

TR::Register *
J9::Z::TreeEvaluator::pdchkEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *chkResultReg  = cg->allocateRegister(TR_GPR);
   generateRRInstruction(cg, TR::Compiler->target.is64Bit() ? TR::InstOpCode::XGR : TR::InstOpCode::XR, node, chkResultReg, chkResultReg);

   TR::Node * pdloadNode = node->getFirstChild();
   TR::Register* pdReg = NULL;

   static char* isVectorBCDEnv = feGetEnv("TR_enableVectorBCD");
   if(TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() &&
           !cg->comp()->getOption(TR_DisableVectorBCD) ||
           isVectorBCDEnv)
      {
      pdReg = cg->evaluate(pdloadNode);
      generateVRRgInstruction(cg, TR::InstOpCode::VTP, node, pdReg);
      }
   else
      {
      pdReg = cg->evaluateBCDNode(pdloadNode);
      TR_StorageReference *pdStorageReference = static_cast<TR_PseudoRegister*>(pdReg)->getStorageReference();
      TR::MemoryReference *tempMR = generateS390RightAlignedMemoryReference(pdloadNode, pdStorageReference, cg);
      generateRSLInstruction(cg, TR::InstOpCode::TP, pdloadNode, static_cast<TR_PseudoRegister*>(pdReg)->getSize()-1, tempMR);
      }

   generateRRInstruction(cg, TR::InstOpCode::IPM, node, chkResultReg, chkResultReg);

   if(TR::Compiler->target.is64Bit())
      {
      generateRRInstruction(cg, TR::InstOpCode::LLGTR, node, chkResultReg, chkResultReg);
      generateRSInstruction(cg, TR::InstOpCode::SRLG, node, chkResultReg, chkResultReg, 28);
      }
   else
      {
      generateRSInstruction(cg, TR::InstOpCode::SRL, node, chkResultReg, 28);
      }

   node->setRegister(chkResultReg);
   cg->decReferenceCount(pdloadNode);
   return chkResultReg;
   }

/**
 * pd<op>Evaluator - various binary packed decimal evaluators
 */
void
J9::Z::TreeEvaluator::correctPackedArithmeticPrecision(TR::Node *node, int32_t op1EncodingSize, TR_PseudoRegister *targetReg, int32_t computedResultPrecision, TR::CodeGenerator * cg)
   {
   int32_t computedResultSize = TR::DataType::packedDecimalPrecisionToByteLength(computedResultPrecision);
   if (op1EncodingSize >= computedResultSize)
      targetReg->removeRangeOfZeroDigits(0, computedResultPrecision);
   else
      targetReg->removeRangeOfZeroBytes(0, op1EncodingSize);

   int32_t resultPrecision = std::min<int32_t>(computedResultPrecision, node->getDecimalPrecision());
   targetReg->setDecimalPrecision(resultPrecision);
   if (cg->traceBCDCodeGen())
      traceMsg(cg->comp(),"\tset targetRegPrec to min(computedResultPrecision, nodePrec) = min(%d, %d) = %d (targetRegSize = %d)\n",
         computedResultPrecision,node->getDecimalPrecision(),resultPrecision,targetReg->getSize());
   }

TR::Register *
J9::Z::TreeEvaluator::pdaddEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   cg->traceBCDEntry("pdadd",node);
   cg->generateDebugCounter("PD-Op/pdadd", 1, TR::DebugCounter::Cheap);

   TR::Register * reg = NULL;

   static char* isVectorBCDEnv = feGetEnv("TR_enableVectorBCD");
   if(TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() && !cg->comp()->getOption(TR_DisableVectorBCD) || isVectorBCDEnv)
      {
      reg = pdArithmeticVectorEvaluatorHelper(node, TR::InstOpCode::VAP, cg);
      }
   else
      {
      reg = pdaddsubEvaluatorHelper(node, TR::InstOpCode::AP, cg);
      }

   cg->traceBCDExit("pdadd",node);
   return reg;
   }

TR::Register *
J9::Z::TreeEvaluator::pdsubEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   cg->traceBCDEntry("pdsub",node);
   cg->generateDebugCounter("PD-Op/pdsub", 1, TR::DebugCounter::Cheap);

   TR::Register * reg = NULL;

   static char* isVectorBCDEnv = feGetEnv("TR_enableVectorBCD");
   if(TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() && !cg->comp()->getOption(TR_DisableVectorBCD) || isVectorBCDEnv)
      {
      reg = pdArithmeticVectorEvaluatorHelper(node, TR::InstOpCode::VSP, cg);
      }
   else
      {
      reg = pdaddsubEvaluatorHelper(node, TR::InstOpCode::SP, cg);
      }

   cg->traceBCDExit("pdsub",node);
   return reg;
   }

int32_t getAddSubComputedResultPrecision(TR::Node *node, TR::CodeGenerator * cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();

   TR_PseudoRegister *firstReg = firstChild->getPseudoRegister();
   if (firstReg == NULL)
      firstReg = cg->evaluateBCDNode(firstChild);

   TR_PseudoRegister *secondReg = secondChild->getPseudoRegister();
   if (secondReg == NULL)
      secondReg = cg->evaluateBCDNode(secondChild);

   int32_t precBump = (firstChild->isZero() || secondChild->isZero()) ? 0 : 1;
   int32_t computedResultPrecision = std::max(firstReg->getDecimalPrecision(), secondReg->getDecimalPrecision())+precBump;

   return computedResultPrecision;
   }

/**
 * This evaluator helper function uses BCD vector instructions for PD arithmetic operations:
 *
 * -- pdadd
 * -- pdsub
 * -- pdmul
 * -- pddiv
 *
 * whose corresponding BCD vector instructions are of VRI-f format.
 */
TR::Register *
J9::Z::TreeEvaluator::pdArithmeticVectorEvaluatorHelper(TR::Node * node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator * cg)
   {
   int32_t immediateValue = node->getDecimalPrecision();
   TR_ASSERT_FATAL((immediateValue >> 8) == 0, "Decimal precision (%d) exceeds 1 byte", immediateValue);

   if (TR::Compiler->target.cpu.getSupportsVectorPackedDecimalEnhancementFacility() && cg->getIgnoreDecimalOverflowException())
      {
      immediateValue |= 0x80;
      }
   TR::Node* firstChild = node->getFirstChild();
   TR::Node* secondChild = node->getSecondChild();

   TR::Register* firstChildReg = cg->evaluate(firstChild);
   TR::Register* secondChildReg = cg->evaluate(secondChild);

   // For simple PD Decimal Operations, let's set the mask to 0: no force positive nor set CC
   TR::Register* targetReg = cg->allocateRegister(TR_VRF);
   generateVRIfInstruction(cg, op, node, targetReg, firstChildReg, secondChildReg, immediateValue, 0x1);
   node->setRegister(targetReg);

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   return targetReg;
   }

/**
 * Handles pdadd,pdsub
 */
TR::Register *
J9::Z::TreeEvaluator::pdaddsubEvaluatorHelper(TR::Node * node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator * cg)
   {
   bool produceOverflowMessage = node->getOpCode().isPackedArithmeticOverflowMessage();
   bool isAdd = (op == TR::InstOpCode::AP);
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Compilation *comp = cg->comp();

   TR_PseudoRegister *firstReg = cg->evaluateBCDNode(firstChild);
   bool trackSignState=false;
   bool alwaysLegalToCleanSign=true; // ok to use ZAP (and clobber srcSign) to init as there is an AP/SP coming
   TR_PseudoRegister *targetReg = evaluateBCDValueModifyingOperand(node, true, NULL, cg, trackSignState, 0, alwaysLegalToCleanSign); // initTarget=true, sourceMR=NULL, srcSize=0
   cg->decReferenceCount(firstChild); // dec bef evaluating the second child to avoid an unneeded clobber evaluate
   TR_PseudoRegister *secondReg = cg->evaluateBCDNode(secondChild);
   TR_StorageReference *targetStorageReference = targetReg->getStorageReference();
   TR::MemoryReference *destMR = generateS390RightAlignedMemoryReference(node, targetStorageReference, cg);
   TR::MemoryReference *secondMR = generateS390RightAlignedMemoryReference(secondChild, secondReg->getStorageReference(), cg);


   int32_t op1EncodingPrecision = cg->getPDAddSubEncodedPrecision(node, firstReg);
   int32_t op1EncodingSize = cg->getPDAddSubEncodedSize(node, firstReg);
   // The preparatory clearing operations need a length set so base it on the op1EncodingSize but the final returned precision will be set after the AP/SP instruction has been generated
   targetReg->setDecimalPrecision(op1EncodingPrecision);

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\t%s: produceOverflowMessage=%s, node->getSize()=%d, firstReg->getSize()=%d, secondReg->getSize()=%d, op1EncodingPrec=%d, op1EncodingSize=%d\n",
         node->getOpCode().getName(),produceOverflowMessage?"yes":"no", node->getSize(), firstReg->getSize(), secondReg->getSize(),op1EncodingPrecision, targetReg->getSize());

   if (op1EncodingSize > firstReg->getSize())
      cg->clearByteRangeIfNeeded(node, targetReg, generateS390RightAlignedMemoryReference(*destMR, node, 0, cg), firstReg->getSize(), op1EncodingSize, true); // widenOnLeft=true

   // endByte=firstReg->getSize but for types like packed where the sign is right aligned this endByte setting does not matter
   // as the leftMostByte for the sign is always known (== 1)
   cg->correctBadSign(firstChild, firstReg, firstReg->getSize(), destMR);
   cg->correctBadSign(secondChild, secondReg, secondReg->getSize(), secondMR);

   int32_t computedResultPrecision = getAddSubComputedResultPrecision(node, cg);
   bool mayOverflow = computedResultPrecision > node->getDecimalPrecision();
   correctPackedArithmeticPrecision(node, op1EncodingSize, targetReg, computedResultPrecision, cg);

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tcomputedResultPrecision %s nodePrec (%d %s %d) -- mayOverflow = %s\n",
         mayOverflow?">":"<=",computedResultPrecision,mayOverflow?">":"<=",node->getDecimalPrecision(),mayOverflow?"yes":"no");

   TR::LabelSymbol * cFlowRegionStart = NULL;
   TR::LabelSymbol * cflowRegionEnd = NULL;
   TR::RegisterDependencyConditions * deps = NULL;
   if (mayOverflow && produceOverflowMessage)
      {
      cFlowRegionStart   = generateLabelSymbol(cg);
      cflowRegionEnd     = generateLabelSymbol(cg);
      deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg);

      if (destMR->getIndexRegister())
         deps->addPostConditionIfNotAlreadyInserted(destMR->getIndexRegister(), TR::RealRegister::AssignAny);
      if (destMR->getBaseRegister())
         deps->addPostConditionIfNotAlreadyInserted(destMR->getBaseRegister(), TR::RealRegister::AssignAny);
      if (secondMR->getIndexRegister())
         deps->addPostConditionIfNotAlreadyInserted(secondMR->getIndexRegister(), TR::RealRegister::AssignAny);
      if (secondMR->getBaseRegister())
         deps->addPostConditionIfNotAlreadyInserted(secondMR->getBaseRegister(), TR::RealRegister::AssignAny);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart, deps);
      cFlowRegionStart->setStartInternalControlFlow();
      }

   generateSS2Instruction(cg, op, node,
                             op1EncodingSize-1,
                             generateS390RightAlignedMemoryReference(*destMR, node, 0, cg),
                             secondReg->getSize()-1,
                             generateS390RightAlignedMemoryReference(*secondMR, node, 0, cg));

   targetReg->setHasKnownValidSignAndData();

   if (mayOverflow)
      {
      if (targetReg->isEvenPrecision() && !node->canSkipPadByteClearing())
         {
         cg->genZeroLeftMostPackedDigits(node, targetReg, targetReg->getSize(), 1, generateS390RightAlignedMemoryReference(*destMR, node, 0, cg));
         }
      targetReg->setHasKnownPreferredSign();
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\toverflow may occur so set HasKnownPreferredSign = true on reg %s\n",cg->getDebug()->getName(targetReg));
      if (produceOverflowMessage)
         {
         // The only overflow message handled is overflow into the next byte (i.e. not 'even' to 'odd' precision 'overflow').
         // This is also an important restriction as no NI for the top nibble is done here and if it were to be done then this
         // would also overwrite the condition code in the isFoldedIf=true case
         TR_ASSERT(targetReg->isOddPrecision(),"expecting targetPrecision to be odd and not %d for addsubOverflowMessage\n",targetReg->getDecimalPrecision());

         TR::LabelSymbol *oolEntryPoint = generateLabelSymbol(cg);
         TR::LabelSymbol *oolReturnPoint = generateLabelSymbol(cg);

         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BO, node, oolEntryPoint);

         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cflowRegionEnd, deps);
         cflowRegionEnd->setEndInternalControlFlow();
         }
      }
   else
      {
      targetReg->setHasKnownCleanSign();
      if (cg->traceBCDCodeGen())
         {
         if (firstChild->isZero() || secondChild->isZero())
            traceMsg(comp,"\t%s firstChild %p isZero=%s or secondChild %p isZero=%s so nibble clearing is NOT required and set HasKnownCleanSign = true on reg %s\n",
                        isAdd?"add":"sub",firstChild,firstChild->isZero()?"yes":"no",secondChild,secondChild->isZero()?"yes":"no",cg->getDebug()->getName(targetReg));
         else
            traceMsg(comp,"\t%s result prec %d is > both reg1 prec %d and reg2 prec %d so nibble clearing is NOT required and set HasKnownCleanSign = true on reg %s\n",
                        isAdd?"add":"sub",node->getDecimalPrecision(),firstReg->getDecimalPrecision(),secondReg->getDecimalPrecision(),cg->getDebug()->getName(targetReg));
         }
      // An NI to clear the top nibble is never required in this case:
      //    If the largest source is even (eg prec 4) then biggest the result can be is odd (i.e. +1 largest source -- prec 5)
      //    and on an odd result no clearing is needed
      //    If the largest source is odd (eg prec 5) then the biggest the result can be is even (i.e. +1 largest source -- prec 6)
      //    and the top nibble must already be clear as the whole byte must be clear before the operation
      }


   if (isAdd &&
       firstReg->hasKnownOrAssumedPositiveSignCode() &&
       secondReg->hasKnownOrAssumedPositiveSignCode())
      {
      if (cg->traceBCDCodeGen())
         traceMsg(comp, "\tfirstReg and secondReg have positive sign codes so set targetReg sign code to the preferred positive sign 0x%x\n", TR::DataType::getPreferredPlusCode());
      // positive+positive=positive and then AP will clean the positive sign to 0xc
      targetReg->setKnownSignCode(TR::DataType::getPreferredPlusCode());
      }

   node->setRegister(targetReg);
   cg->decReferenceCount(secondChild);
   return targetReg;
   }

TR::Register *
J9::Z::TreeEvaluator::pdmulEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   cg->traceBCDEntry("pdmul",node);
   cg->generateDebugCounter("PD-Op/pdmul", 1, TR::DebugCounter::Cheap);

   TR::Register * reg = NULL;

   static char* isVectorBCDEnv = feGetEnv("TR_enableVectorBCD");
   if(TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() &&
           !cg->comp()->getOption(TR_DisableVectorBCD) ||
           isVectorBCDEnv)
      {
      reg = pdArithmeticVectorEvaluatorHelper(node, TR::InstOpCode::VMP, cg);
      }
   else
      {
      reg = pdmulEvaluatorHelper(node, cg);
      }

   cg->traceBCDExit("pdmul",node);
   return reg;
   }

TR::Register *
J9::Z::TreeEvaluator::pdmulEvaluatorHelper(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Compilation *comp = cg->comp();

   TR_PseudoRegister *firstReg = cg->evaluateBCDNode(firstChild);
   bool trackSignState=false;
   bool alwaysLegalToCleanSign=true; // ok to use ZAP (and clobber srcSign) to init as there is an MP coming
   TR_PseudoRegister *targetReg = evaluateBCDValueModifyingOperand(node, true, NULL, cg, trackSignState, 0, alwaysLegalToCleanSign); // initTarget=true, sourceMR=NULL, srcSize=0
   cg->decReferenceCount(firstChild);
   TR_PseudoRegister *secondReg = cg->evaluateBCDNode(secondChild);
   TR_StorageReference *targetStorageReference = targetReg->getStorageReference();
   TR::MemoryReference *destMR = generateS390RightAlignedMemoryReference(node, targetStorageReference, cg);
   TR::MemoryReference *secondMR = generateS390RightAlignedMemoryReference(secondChild, secondReg->getStorageReference(), cg);

   int32_t op1EncodingPrecision = cg->getPDMulEncodedPrecision(node, firstReg, secondReg);
   int32_t op1EncodingSize = cg->getPDMulEncodedSize(node, firstReg, secondReg);
   // The preparatory clearing operations need a length set so base it on the op1EncodingSize but the final precision will be set after the MP instruction has been generated
   targetReg->setDecimalPrecision(op1EncodingPrecision);

   TR_ASSERT( targetReg->getSize() >= firstReg->getSize() + secondReg->getSize(),"MP may result in a data exception\n");
   TR_ASSERT( secondReg->getSize() <= 8, "MP will result in a spec exception\n");

   cg->clearByteRangeIfNeeded(node, targetReg, generateS390RightAlignedMemoryReference(*destMR, node, 0, cg), firstReg->getSize(), op1EncodingSize, true); // widenOnLeft=true

   cg->correctBadSign(firstChild, firstReg, firstReg->getSize(), destMR);
   cg->correctBadSign(secondChild, secondReg, secondReg->getSize(), secondMR);

   TR::Instruction * cursor =
      generateSS2Instruction(cg, TR::InstOpCode::MP, node,
                             op1EncodingSize-1,
                             generateS390RightAlignedMemoryReference(*destMR, node, 0, cg),
                             secondReg->getSize()-1,
                             generateS390RightAlignedMemoryReference(*secondMR, node, 0, cg));

   targetReg->setHasKnownValidSignAndData();

   int32_t computedResultPrecision = firstReg->getDecimalPrecision() + secondReg->getDecimalPrecision();
   correctPackedArithmeticPrecision(node, op1EncodingSize, targetReg, computedResultPrecision, cg);

   if (targetReg->getDecimalPrecision() < computedResultPrecision)
      {
      if (!node->canSkipPadByteClearing() && targetReg->isEvenPrecision())
         cg->genZeroLeftMostPackedDigits(node, targetReg, targetReg->getSize(), 1, generateS390RightAlignedMemoryReference(*destMR, node, 0, cg));
      }
   else if (cg->traceBCDCodeGen())
      {
      traceMsg(comp,"TR::InstOpCode::MP node %p targetRegPrec %d >= computedResultPrecision %d (firstRegPrec %d + secondRegPrec %d) so skip nibble clearing\n",
         node,targetReg->getDecimalPrecision(),computedResultPrecision,firstReg->getDecimalPrecision(),secondReg->getDecimalPrecision());
      }

   // Even with no overflow MP can produce a negative zero as the sign of the result is determined from the rules
   // of algebra *even when one or both of the operands are zero*. So 0 * -1 = -0 (0x0c * 0x1d = 0x0d -- not clean result)
   // MP will always produce a result with a preferred sign however.
   if (firstReg->hasKnownOrAssumedPositiveSignCode() &&
       secondReg->hasKnownOrAssumedPositiveSignCode())
      {
      if (cg->traceBCDCodeGen())
         traceMsg(comp, "\tfirstReg and secondReg have positive sign codes so set targetReg sign code to the preferred positive sign 0x%x\n", TR::DataType::getPreferredPlusCode());
      // positive*positive=positive and then MP will clean the positive sign to 0xc
      targetReg->setKnownSignCode(TR::DataType::getPreferredPlusCode());
      }
   else
      {
      targetReg->setHasKnownPreferredSign();
      }

   cg->decReferenceCount(secondChild);
   return targetReg;
   }

/**
 * Handles pddiv, and pdrem.
 */
TR::Register *
J9::Z::TreeEvaluator::pddivremEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(cg->comp(), "PD-Op/%s", node->getOpCode().getName()),
                            1, TR::DebugCounter::Cheap);
   TR::Register * reg = NULL;

   static char* isVectorBCDEnv = feGetEnv("TR_enableVectorBCD");
   if(TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() && !cg->comp()->getOption(TR_DisableVectorBCD) || isVectorBCDEnv)
      {
      reg = pddivremVectorEvaluatorHelper(node, cg);
      }
   else
      {
      reg = pddivremEvaluatorHelper(node, cg);
      }

   return reg;
   }

/**
 * Handles pddiv, and pdrem. This is the vector evaluator helper function.
 */
TR::Register *
J9::Z::TreeEvaluator::pddivremVectorEvaluatorHelper(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register* vTargetReg = NULL;
   TR::InstOpCode::Mnemonic opCode;
   switch(node->getOpCodeValue())
      {
      case TR::pddiv:
         opCode = TR::InstOpCode::VDP;
         break;
      case TR::pdrem:
         opCode = TR::InstOpCode::VRP;
         break;
      default:
         TR_ASSERT(0, "Unexpected opcode in pddiv/remVectorEvaluatorHelper");
         break;
      }

   vTargetReg = pdArithmeticVectorEvaluatorHelper(node, opCode, cg);
   return vTargetReg;
   }

/**
 * Handles pddiv, and pdrem. This is the non-vector evaluator helper function.
 */
TR::Register *
J9::Z::TreeEvaluator::pddivremEvaluatorHelper(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR_ASSERT( node->getOpCodeValue() == TR::pddiv || node->getOpCodeValue() == TR::pdrem,
              "pddivEvaluator only valid for pddiv/pdrem\n");

   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Compilation *comp = cg->comp();

   TR_PseudoRegister *firstReg = cg->evaluateBCDNode(firstChild);
   bool trackSignState=false;
   bool alwaysLegalToCleanSign=true; // ok to use ZAP (and clobber srcSign) to init as there is a DP coming
   TR_PseudoRegister *targetReg = evaluateBCDValueModifyingOperand(node, true, NULL, cg, trackSignState, 0, alwaysLegalToCleanSign); // initTarget=true, sourceMR=NULL, srcSize=0
   cg->decReferenceCount(firstChild);
   TR_PseudoRegister *secondReg = cg->evaluateBCDNode(secondChild);
   TR_StorageReference *targetStorageReference = targetReg->getStorageReference();
   TR::MemoryReference *destMR = generateS390RightAlignedMemoryReference(node, targetStorageReference, cg);

   if (secondReg->getDecimalPrecision() > secondChild->getDecimalPrecision())
      {
      TR_ASSERT( false,"the secondRegPrec has grown so using an inline DP may not be legal\n"); // TODO: for now disallow this completely but the below fix is also correct.
      TR_ASSERT(secondReg->getSize() == secondChild->getSize(),
         "the secondRegSize (regSize %d != nodeSize %d) has grown so using an inline DP may not be legal\n",secondReg->getSize(),secondChild->getSize());
      // The register precision may have been conservatively adjusted from an even precision to the next odd precision so in these
      // cases set it back to the even precision so the inline divide will still be legal. This extra nibble of precision will be zero so this is safe.
      secondReg->setDecimalPrecision(secondReg->getDecimalPrecision()-1);
      }

   int32_t dividendPrecision = 0;
   int32_t divisorSize = 0;
   int32_t dividendSizeBumpForClear = 0;
   TR::MemoryReference *divisorMR = NULL;

   divisorMR = generateS390RightAlignedMemoryReference(secondChild, secondReg->getStorageReference(), cg);
   dividendPrecision = cg->getPDDivEncodedPrecision(node, firstReg, secondReg);
   divisorSize = secondReg->getSize();

   targetReg->setDecimalPrecision(dividendPrecision);
   int32_t dividendSize = targetReg->getSize();
   TR_ASSERT( dividendSize <= node->getStorageReferenceSize(),"allocated symbol for pddiv/pdrem is too small\n");
   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\t%s: gen DP dividendSize = %d, secondOpSize = secondRegSize = %d, targetRegSize = %d (firstRegPrec %d, secondRegPrec %d)\n",
         node->getOpCode().getName(),dividendSize,secondReg->getSize(),targetReg->getSize(),firstReg->getDecimalPrecision(),secondReg->getDecimalPrecision());

   cg->clearByteRangeIfNeeded(node, targetReg, generateS390RightAlignedMemoryReference(*destMR, node, 0, cg), dividendSize-divisorSize-dividendSizeBumpForClear, dividendSize, true); // widenOnLeft=true

   cg->correctBadSign(firstChild, firstReg, targetReg->getSize(), destMR);
   cg->correctBadSign(secondChild, secondReg, secondReg->getSize(), divisorMR);

   generateSS2Instruction(cg, TR::InstOpCode::DP, node,
                          dividendSize-1,
                          generateS390RightAlignedMemoryReference(*destMR, node, 0, cg),
                          divisorSize-1,
                          generateS390RightAlignedMemoryReference(*divisorMR, node, 0, cg));

   targetReg->setHasKnownValidSignAndData();

   bool isRem = node->getOpCodeValue() == TR::pdrem;
   int32_t deadBytes = 0;
   bool isTruncation = false;
   if (isRem)
      {
      targetReg->setDecimalPrecision(secondReg->getDecimalPrecision());
      isTruncation = node->getDecimalPrecision() < targetReg->getDecimalPrecision();
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tpdrem: setting targetReg prec to divisor prec %d (node prec is %d), isTruncation=%s\n",
            secondReg->getDecimalPrecision(),node->getDecimalPrecision(),isTruncation?"yes":"no");
      targetReg->removeRangeOfZeroDigits(0, TR::DataType::byteLengthToPackedDecimalPrecisionCeiling(dividendSize));
      }
   else
      {
      deadBytes = divisorSize;
      // computedQuotientPrecision is the size of the quotient as computed by the DP instruction.
      // The actual returned node precision may be less.
      int32_t computedQuotientPrecision = TR::DataType::byteLengthToPackedDecimalPrecisionCeiling(dividendSize - deadBytes);
      if (firstReg->isEvenPrecision())
         {
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tfirstRegPrec (%d) isEven=true so reduce computedQuotientPrecision %d->%d\n",firstReg->getDecimalPrecision(),computedQuotientPrecision,computedQuotientPrecision-1);
         computedQuotientPrecision--;
         }
      isTruncation = node->getDecimalPrecision() < computedQuotientPrecision;
      int32_t resultQuotientPrecision = std::min<int32_t>(computedQuotientPrecision, node->getDecimalPrecision());
      targetReg->setDecimalPrecision(resultQuotientPrecision);
      targetReg->addToRightAlignedDeadBytes(deadBytes);
      if (cg->traceBCDCodeGen())
         {
         traceMsg(comp,"\tisDiv=true (pddivrem) : increment targetReg %s deadBytes %d -> %d (by the divisorSize)\n",
            cg->getDebug()->getName(targetReg),targetReg->getRightAlignedDeadBytes()-deadBytes,targetReg->getRightAlignedDeadBytes());
         traceMsg(comp,"\tsetting targetReg prec to min(computedQuotPrec, nodePrec) = min(%d, %d) = %d (size %d), isTruncation=%s\n",
            computedQuotientPrecision,node->getDecimalPrecision(),resultQuotientPrecision,targetReg->getSize(),isTruncation?"yes":"no");
         }
      targetReg->removeRangeOfZeroDigits(0, computedQuotientPrecision);
      }

   if (!node->canSkipPadByteClearing() && targetReg->isEvenPrecision() && isTruncation)
      {
      TR_ASSERT( node->getStorageReferenceSize() >= dividendSize,"operand size should only shrink from original size\n");
      int32_t leftMostByte = targetReg->getSize();
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\t%s: generating NI to clear top nibble with leftMostByte = targetReg->getSize() = %d\n",isRem ? "pdrem":"pddiv",targetReg->getSize());
      cg->genZeroLeftMostPackedDigits(node, targetReg, leftMostByte, 1, generateS390RightAlignedMemoryReference(*destMR, node, -deadBytes, cg));
      }

   targetReg->setHasKnownPreferredSign();
   if (isRem)
      {
      // sign of the remainder is the same as the sign of dividend (and then set to the preferred sign by the DP instruction)
      if (firstReg->hasKnownOrAssumedSignCode())
         {
         targetReg->setKnownSignCode(firstReg->hasKnownOrAssumedPositiveSignCode() ? TR::DataType::getPreferredPlusCode() : TR::DataType::getPreferredMinusCode());
         if (cg->traceBCDCodeGen())
            traceMsg(comp,"\tpdrem: firstReg has the knownSignCode 0x%x so set targetReg sign code to the preferred sign 0x%x\n",
               firstReg->getKnownOrAssumedSignCode(),targetReg->getKnownOrAssumedSignCode());
         }
      }
   else
      {
      // when the sign of the divisor and divident are different then the quotient sign is negative otherwise if the signs are the same then the
      // quotient sign is positive
      if (firstReg->hasKnownOrAssumedSignCode() && secondReg->hasKnownOrAssumedSignCode())
         {
         bool dividendSignIsPositive = firstReg->hasKnownOrAssumedPositiveSignCode();
         bool dividendSignIsNegative = !dividendSignIsPositive;
         bool divisorSignIsPositive = secondReg->hasKnownOrAssumedPositiveSignCode();
         bool divisorSignIsNegative = !divisorSignIsPositive;

         if ((dividendSignIsPositive && divisorSignIsPositive) ||
             (dividendSignIsNegative && divisorSignIsNegative))
            {
            targetReg->setKnownSignCode(TR::DataType::getPreferredPlusCode());
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\tpddiv: dividendSign matches the divisorSign so set targetReg sign code to the preferred sign 0x%x\n", TR::DataType::getPreferredPlusCode());
            }
         else
            {
            targetReg->setKnownSignCode(TR::DataType::getPreferredMinusCode());
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\tpddiv: dividendSign does not match the divisorSign so set targetReg sign code to the preferred sign 0x%x\n", TR::DataType::getPreferredMinusCode());
            }
         }
      }

   cg->decReferenceCount(secondChild);
   return targetReg;
   }

/**
 * Handles pdshr
 */
TR::Register *
J9::Z::TreeEvaluator::pdshrEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   cg->traceBCDEntry("pdshr",node);
   cg->generateDebugCounter("PD-Op/pdshr", 1, TR::DebugCounter::Cheap);

   TR::Register* targetReg = NULL;

   static char* isEnableVectorBCD = feGetEnv("TR_enableVectorBCD");
   if(TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() &&
           !cg->comp()->getOption(TR_DisableVectorBCD) ||
           isEnableVectorBCD)
      {
      targetReg = pdshrVectorEvaluatorHelper(node, cg);
      }
   else
      {
      targetReg = pdshiftEvaluatorHelper(node, cg, true);
      }

   cg->traceBCDExit("pdshr",node);
   return targetReg;
   }

void
J9::Z::TreeEvaluator::clearAndSetSign(TR::Node *node,
                                          TR_PseudoRegister *targetReg,
                                          int32_t leftMostByteForClear,
                                          int32_t digitsToClear,
                                          TR::MemoryReference *destMR,
                                          TR_PseudoRegister *srcReg,
                                          TR::MemoryReference *sourceMR,
                                          bool isSetSign,
                                          int32_t sign,
                                          bool signCodeIsInitialized,
                                          TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();

   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tclearAndSetSign: digitsToClear %d, leftMostByte %d (isSetSign=%s, sign 0x%x)\n",digitsToClear,leftMostByteForClear,isSetSign?"yes":"no",sign);
   bool clearingNeeded = digitsToClear > 0;
   if (isSetSign)
      {
      // a better sign code setting maybe possible if a current setting is known
      TR_PseudoRegister *signReg = signCodeIsInitialized ? targetReg : NULL;
      int32_t digitsCleared = cg->genSignCodeSetting(node, targetReg, node->getSize(), generateS390RightAlignedMemoryReference(*destMR, node, 0, cg), sign, signReg, digitsToClear, !clearingNeeded);
      if (clearingNeeded)
         {
         digitsToClear-=digitsCleared;
         if (digitsToClear > 0 && (digitsToClear&0x1) && sign == TR::DataType::getIgnoredSignCode())
            {
            digitsToClear++;  // when digitsToClear is odd for the ignore sign code case then bump up to the next even amount (and clear the sign too) as this is easier to clear
            targetReg->setHasKnownBadSignCode();
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\tignored setSign case so inc digitsToClear %d->%d and setHasKnownBadSignCode=true on targetReg %s\n",
                  digitsToClear-1,digitsToClear,cg->getDebug()->getName(targetReg));
            }
         }
      signCodeIsInitialized = true;
      if (cg->traceBCDCodeGen())
         {
         if (clearingNeeded)
            traceMsg(comp,"\t\tisSetSign case (clearingNeeded==true): sign setting cleared %d digits so adjust digitsToClear %d->%d\n",
               digitsCleared,digitsToClear+digitsCleared,digitsToClear);
          traceMsg(comp,"\t\tisSetSign case: set signCode of 0x%x on targetReg %s\n",sign,cg->getDebug()->getName(targetReg));
         }
      }
   else if (!signCodeIsInitialized)
      {
      /* if (digitsToClear == 1) // MVN done later is better then MVC/NI as the latter suffers from an OSC
         {
         int32_t mvcSize = 1;
         generateSS1Instruction(cg, TR::InstOpCode::MVC, node,
                                mvcSize-1,
                                generateS390RightAlignedMemoryReference(*destMR, node, 0, cg),
                                generateS390RightAlignedMemoryReference(*sourceMR, node, 0, cg));
         targetReg->transferSignState(srcReg, true); // digitsLost=true -- a clear always loses digits
         signCodeIsInitialized = true;    // no longer clear the sign code in the code below for if (needLateClear)
         if (cg->traceBCDCodeGen()) traceMsg(comp,"\t\tdigitsToClear==1 case: gen MVC to initialize sign code\n");
         }
      else */
      if (clearingNeeded)
         {
         digitsToClear++;         // clear the sign code too and then MVN in the new sign code
         if (cg->traceBCDCodeGen()) traceMsg(comp,"\t\t init=false && isSetSign=false case : bump digitsToClear %d->%d to clear entire field\n",digitsToClear,digitsToClear+1);
         }
      }
   TR_ASSERT(digitsToClear >= 0,"digitsToClear %d should be >= 0\n",digitsToClear);
   if (digitsToClear > 0)
      {
      if (cg->traceBCDCodeGen()) traceMsg(comp,"\t\tdigitsToClear %d > 0 so call genClearLeftMostDigitsIfNeeded\n",digitsToClear);
      cg->genZeroLeftMostDigitsIfNeeded(node, targetReg, leftMostByteForClear, digitsToClear, generateS390RightAlignedMemoryReference(*destMR, node, 0, cg));
      }

   if (!signCodeIsInitialized)
      {
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\t\tsignCodeIsInitialized=false after clearing of %d digits : init the sign now with an MVN of size 1\n",digitsToClear,isSetSign?"yes":"no");
      // Move the sign code over from the source location. The top nibble has already been cleared above.
      int32_t mvnSize = 1;
      generateSS1Instruction(cg, TR::InstOpCode::MVN, node,
                             mvnSize-1,
                             generateS390RightAlignedMemoryReference(*destMR, node, 0, cg),
                             generateS390RightAlignedMemoryReference(*sourceMR, node, 0, cg));
      targetReg->transferSignState(srcReg, true); // digitsLost=true -- a clear always loses digits
      }
   }

TR_PseudoRegister *
J9::Z::TreeEvaluator::simpleWideningOrTruncation(TR::Node *node,
                                                     TR_PseudoRegister *srcReg,
                                                     bool isSetSign,
                                                     int32_t sign,
                                                     TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tsimple widening or truncating shift: srcRegPrecision %d, isSetSign=%s, sign 0x%x\n",srcReg->getDecimalPrecision(),isSetSign?"yes":"no",sign);
   bool isDigitTruncation = false;
   bool needsTopNibbleClearing = false;
   int32_t srcPrecision = srcReg->getDecimalPrecision();
   if (srcReg->getDecimalPrecision() > node->getDecimalPrecision())
      {
      srcPrecision = node->getDecimalPrecision();
      isDigitTruncation = true;
      if (!node->canSkipPadByteClearing() && node->isEvenPrecision() && srcReg->getDigitsToClear(srcPrecision,srcPrecision+1) != 0)
         needsTopNibbleClearing = true;
      }

   int32_t targetPrecision = node->getDecimalPrecision();

   if (!isDigitTruncation && srcReg->isEvenPrecision() && !srcReg->isLeftMostNibbleClear())
      {
      if (targetPrecision != srcPrecision) // in case this routine starts doing explicit widenings at some point then !canSkipPadByteClearing alone is not valid
         {
         needsTopNibbleClearing = true;
         }
      else if (!node->canSkipPadByteClearing())
         {
         needsTopNibbleClearing = true;
         if (cg->traceBCDCodeGen()) traceMsg(comp,"z^z : new clear : simpleWide %p\n",node);
         }
      }

   bool isPassThrough = false;
   bool initTargetAndSign = (isSetSign && !isPassThrough); // try to get a ZAP generated here for a widening as this can simplify the coming setSign operation
   bool isNondestructiveNop = isPassThrough && !isDigitTruncation;
   TR_PseudoRegister *targetReg = NULL;
   TR::MemoryReference *sourceMR = NULL;
   if (cg->traceBCDCodeGen())
      traceMsg(comp,"\tisDigitTruncation=%s, srcPrecision=%d, isPassThrough=%s, needsTopNibbleClearing=%s, initTargetAndSign=%s\n",
         isDigitTruncation?"true":"false",srcPrecision,isPassThrough?"true":"false",needsTopNibbleClearing?"true":"false",initTargetAndSign?"yes":"no");
   if (!isPassThrough)
      sourceMR =  generateS390RightAlignedMemoryReference(node->getFirstChild(), srcReg->getStorageReference(), cg);
   if (initTargetAndSign || needsTopNibbleClearing)
      targetReg = evaluateBCDValueModifyingOperand(node, initTargetAndSign, sourceMR, cg, initTargetAndSign);
   else
      targetReg = evaluateBCDSignModifyingOperand(node, isPassThrough, isNondestructiveNop, false, sourceMR, cg); // initTarget=false

   bool isInitialized = targetReg->isInitialized();
   TR::MemoryReference *destMR = NULL;
   if (!isPassThrough)
      destMR = generateS390RightAlignedMemoryReference(node, targetReg->getStorageReference(), cg);
   if (!isInitialized && !isPassThrough)
      {
      int32_t srcSize = TR::DataType::packedDecimalPrecisionToByteLength(srcPrecision);
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"\tisInit=false and isPassThru=false so gen initializing MVC with size %d. Do not clear after MVC just set targetReg->prec to srcPrecision %d\n",srcSize,srcPrecision);
      generateSS1Instruction(cg, TR::InstOpCode::MVC, node,
                             srcSize-1,
                             generateS390RightAlignedMemoryReference(*destMR, node, 0, cg),
                             generateS390RightAlignedMemoryReference(*sourceMR, node, 0, cg));
      }
   else if (cg->traceBCDCodeGen())
      {
      traceMsg(comp,"\tisInit=true (%s) or isPassThru=true (%s): no move needed just set targetReg->prec to srcPrecision %d\n",isInitialized?"yes":"no",isPassThrough?"yes":"no",srcPrecision);
      }

   // a ZAP may have been generated when initializing targetReg so in this case do not transfer the srcReg sign
   if (!targetReg->signStateInitialized() || !initTargetAndSign)
      targetReg->transferSignState(srcReg, isDigitTruncation);

   targetReg->setDecimalPrecision(targetPrecision);

   if (isSetSign && !isPassThrough)
      cg->genSignCodeSetting(node, targetReg, targetReg->getSize(), generateS390RightAlignedMemoryReference(*destMR, node, 0, cg), sign, targetReg, 0, false /* !topNibbleIsZero */);
   else
      targetReg->transferSignState(srcReg, isDigitTruncation);

   targetReg->transferDataState(srcReg);

   if (needsTopNibbleClearing)
      {
      if (cg->traceBCDCodeGen()) traceMsg(comp,"\tisDigitTruncation=true and targetReg->isEvenPrecision() (%d) so clear top nibble\n",targetReg->isEvenPrecision());
      int32_t leftMostByteForClear = TR::DataType::packedDecimalPrecisionToByteLength(srcPrecision);
      cg->genZeroLeftMostPackedDigits(node, targetReg, leftMostByteForClear, 1, generateS390RightAlignedMemoryReference(*destMR, node, 0, cg));
      }

   if (!isPassThrough)
      targetReg->setIsInitialized();

   return targetReg;
   }

/*
 * \brief
 * Generate non-exception throwing instructions for pdModifyPrecision node to narrow or widen packed decimals.
 * The generated instruction sequence does not validate the source packed decimals. Any invalid packed
 * decimals will be loaded as is and modified as if their digits and signs were valid.
*/
TR::Register *
J9::Z::TreeEvaluator::pdModifyPrecisionEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   cg->traceBCDEntry("pdModifyPrecision",node);
   cg->generateDebugCounter("PD-Op/pdmodifyPrec", 1, TR::DebugCounter::Cheap);

   TR::Register* targetReg = NULL;

   static char* isEnableVectorBCD = feGetEnv("TR_enableVectorBCD");
   if(TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() &&
           !cg->comp()->getOption(TR_DisableVectorBCD)
           || isEnableVectorBCD)
      {
      int32_t targetPrec = node->getDecimalPrecision();
      targetReg = cg->allocateRegister(TR_VRF);

      if (TR::Compiler->target.cpu.getSupportsVectorPackedDecimalEnhancementFacility())
         {
         // Overflow exceptions can be ignored for z15 vector packed decimal VRI-i,f,g and VRR-i instructions. Given
         // this, VPSOP now becomes suitable for data truncations without incurring exceptions which eventually lead to
         // performance degradations. This is usually used to truncate high nibble of an even precision PD.
         targetReg = vectorPerformSignOperationHelper(node, cg, true, targetPrec, true, SignOperationType::maintain, false, false, 0, false, true);
         }
      else
         {
         int32_t imm = 0x0FFFF >> (TR_VECTOR_REGISTER_SIZE - TR::DataType::packedDecimalPrecisionToByteLength(targetPrec));
         TR::Register* pdReg = cg->evaluate(node->getFirstChild());
         TR::Register* maskReg = cg->allocateRegister(TR_VRF);
         generateVRIaInstruction(cg, TR::InstOpCode::VGBM, node, maskReg, imm, 0);

         if (targetPrec % 2 == 0)
            {
            TR::Register* shiftAmountReg = cg->allocateRegister(TR_VRF);
            generateVRIaInstruction(cg, TR::InstOpCode::VREPI, node, shiftAmountReg, 4, 0);
            generateVRRcInstruction(cg, TR::InstOpCode::VSRL, node, maskReg, maskReg, shiftAmountReg, 0, 0, 0);
            cg->stopUsingRegister(shiftAmountReg);
            }

         generateVRRcInstruction(cg, TR::InstOpCode::VN, node, targetReg, pdReg, maskReg, 0, 0, 0);

         cg->stopUsingRegister(maskReg);
         cg->decReferenceCount(node->getFirstChild());
         }
      }
   else
      {
      TR::Node *srcNode = node->getChild(0);
      TR_PseudoRegister *srcReg = cg->evaluateBCDNode(srcNode);
      targetReg = simpleWideningOrTruncation(node, srcReg, false, 0, cg);
      cg->decReferenceCount(srcNode);
      node->setRegister(targetReg);
      }

   cg->traceBCDExit("pdModifyPrecision",node);
   return targetReg;
   }

TR::Register *
J9::Z::TreeEvaluator::pdshlEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   cg->traceBCDEntry("pdshl",node);
   cg->generateDebugCounter("PD-Op/pdshl", 1, TR::DebugCounter::Cheap);

   TR::Register* targetReg = NULL;

   static char* isEnableVectorBCD = feGetEnv("TR_enableVectorBCD");
   if(TR::Compiler->target.cpu.getSupportsVectorPackedDecimalFacility() &&
           !cg->comp()->getOption(TR_DisableVectorBCD) ||
           isEnableVectorBCD)
      {
      targetReg = pdshlVectorEvaluatorHelper(node, cg);
      }
   else
      {
      targetReg = pdshiftEvaluatorHelper(node, cg, false);
      }

   cg->traceBCDExit("pdshl",node);
   return targetReg;
   }

/**
 * \brief This is a helper function that handles pdshl, pdshr, and pdshlOverflow nodes.
 *
 * pdshl is currently not used and replaced by pdshlOverflow.
*/
TR::Register *
J9::Z::TreeEvaluator::pdshiftEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg, bool isRightShift)
   {
   TR::Node* srcNode         = node->getChild(0);
   TR::Node* shiftAmountNode = node->getChild(1);
   TR::Compilation *comp     = cg->comp();
   int32_t roundAmount = 0;
   int32_t shiftAmount = 0;

   TR_ASSERT(shiftAmountNode, "expecting a shiftAmountNode\n");
   TR_ASSERT(shiftAmountNode->getOpCode().isLoadConst() &&
             shiftAmountNode->getOpCode().getSize() <= 4,
             "expecting a <= 4 size integral constant PD shift amount\n");
   shiftAmount = (int32_t)shiftAmountNode->get64bitIntegralValue();
   TR_ASSERT(shiftAmount >= 0, "unexpected PD shift amount of %d\n", shiftAmount);

   if(isRightShift)
      {
      shiftAmount *= -1;
      TR::Node* roundAmountNode = node->getChild(2);
      TR_ASSERT(roundAmountNode, "round amount node should not be null\n");
      roundAmount = roundAmountNode->get32bitIntegralValue();
      TR_ASSERT(roundAmount == 0 || roundAmount == 5, "unexpected round amount of %d\n", roundAmount);
      cg->decReferenceCount(roundAmountNode);
      }

   TR_PseudoRegister *srcReg = cg->evaluateBCDNode(srcNode);

   uint32_t srcPrecision = srcNode->getDecimalPrecision();
   uint32_t resultPrecision = node->getDecimalPrecision();
   uint32_t resultSize = TR::DataType::packedDecimalPrecisionToByteLength(resultPrecision);
   uint32_t sourceSize = TR::DataType::packedDecimalPrecisionToByteLength(srcPrecision);

   TR_StorageReference* targetStorageRef = TR_StorageReference::createTemporaryBasedStorageReference(resultSize, comp);
   TR_PseudoRegister* targetReg = cg->allocatePseudoRegister(node->getDataType());
   targetReg->setIsInitialized(true);
   targetReg->setSize(resultSize);
   targetReg->setStorageReference(targetStorageRef, node);

   TR::MemoryReference* targetMR = generateS390RightAlignedMemoryReference(node, targetReg->getStorageReference(), cg);
   TR::MemoryReference* sourceMR = generateS390RightAlignedMemoryReference(srcNode, srcReg->getStorageReference(), cg);
   TR_StorageReference* tmpStorageRef = NULL;
   TR::MemoryReference* tmpMR = NULL;

   if (cg->traceBCDCodeGen())
      {
      traceMsg(comp,"\tGen packed decimal shift: %s %p : shift by %d, roundAmount=%d, result Size=%d, precision %d, sourceSize=%d, precision %d\n",
               node->getOpCode().getName(),
               node,
               shiftAmount,
               roundAmount,
               resultSize,
               resultPrecision,
               sourceSize,
               srcNode->getDecimalPrecision());
      }

   if(shiftAmount == 0)
      {
      if (srcPrecision > resultPrecision)
         {
         /* Packed decimal narrowing with exception handling:
          *
          * If the narrowing operation truncates non-zero digits (e.g. shift "123C" by 0 digts and keep 2 digits yields "23C")
          * and the 'checkOverflow' parameter is true, the JIT'ed sequence should trigger HW exception and
          * yield control to the Java code (via OOL call) so that overflow exceptions can be thrown.
          * This is why PD arithmetic operations use 'pdshlOverflow' to perform data truncations
          * instead of 'modifyPrecision'.
          */

         tmpStorageRef = TR_StorageReference::createTemporaryBasedStorageReference(sourceSize, comp);
         tmpStorageRef->setTemporaryReferenceCount(1);
         tmpMR = generateS390RightAlignedMemoryReference(node, tmpStorageRef, cg);

         generateSS2Instruction(cg, TR::InstOpCode::ZAP, node,
                                sourceSize - 1,
                                generateS390RightAlignedMemoryReference(*tmpMR, node, 0, cg),
                                sourceSize - 1,
                                generateS390RightAlignedMemoryReference(*sourceMR, node, 0, cg));

         shiftAmount = srcPrecision - resultPrecision;
         if ((srcPrecision % 2) == 0)
            {
            // Source being even precision means we need an extra left shift to get right of the source's highest nibble.
            shiftAmount++;
            }

         generateSS3Instruction(cg, TR::InstOpCode::SRP, node,
                                sourceSize - 1,
                                generateS390RightAlignedMemoryReference(*tmpMR, node, 0, cg),
                                shiftAmount, roundAmount);

         generateSS3Instruction(cg, TR::InstOpCode::SRP, node,
                                sourceSize - 1,
                                generateS390RightAlignedMemoryReference(*tmpMR, node, 0, cg),
                                -1*shiftAmount, roundAmount);

         generateSS2Instruction(cg, TR::InstOpCode::ZAP, node,
                                resultSize - 1,
                                generateS390RightAlignedMemoryReference(*targetMR, node, 0, cg),
                                resultSize - 1,
                                generateS390RightAlignedMemoryReference(*tmpMR, node, 0, cg));
         }
      else  // zero shift, copy or widen result
         {
         generateSS2Instruction(cg, TR::InstOpCode::ZAP, node,
                                resultSize - 1,
                                generateS390RightAlignedMemoryReference(*targetMR, node, 0, cg),
                                sourceSize - 1,
                                generateS390RightAlignedMemoryReference(*sourceMR, node, 0, cg));

         // Top nibble cleaning if the PD widening or copying source precision is even
         if ((srcPrecision % 2) == 0)
            {
            cg->genZeroLeftMostPackedDigits(node, targetReg, sourceSize, 1,
                                            generateS390RightAlignedMemoryReference(*targetMR, node, 0, cg));
            }
         }
      }
   else // shiftAmount != 0
      {
      int32_t tmpResultByteSize = sourceSize;
      bool needExtraShift = false;

      if (!isRightShift)
         {
         if ((resultPrecision % 2) == 0)
            {
            /* An extra shift is needed when the left shift result's precision is even.
             * For example, let the input be 00 12 3C (precision=5), shiftAmount=2 and let the result precision be 4.
             * Shift this left by 2 should produce and expected result of 02 30 0C.
             *
             * To produce this expected result with HW exception, we need to
             *
             * 1. shift 00 12 3C by 3 (instead of 2) digits to produce an intermediate result 01 23 00 0C
             * 2. use ZAP to truncate this to 23 00 0C. The purpose of this ZAP is to truncate the leading digits,
             *    which may or may not be zero, and trigger HW exception in case they are non-zero so that the
             *    DAA Java implementation gets a chance to thrown Java exceptions. In our example, the leading
             *    '1' should not be silently discarded (using the NI instruction) because the API 'checkOverflow' parameter
             *    may be true.
             * 3. perform a right shift of 1 on the intermediate result to produce the expected result 02 30 0C.
             *
             */
            shiftAmount++;
            needExtraShift = true;
            }

         // Allocate enough temporary space to accommodate the amount of left shifts.
         tmpResultByteSize += (shiftAmount + 1)/2;
         }

      tmpStorageRef = TR_StorageReference::createTemporaryBasedStorageReference(tmpResultByteSize, comp);
      tmpStorageRef->setTemporaryReferenceCount(1);
      tmpMR = generateS390RightAlignedMemoryReference(node, tmpStorageRef, cg);

      // For this large tmp storage, we need to use XC+MVC to clear and move input into it.
      if (!isRightShift)
         {
         generateSS1Instruction(cg, TR::InstOpCode::XC, node,
                                tmpResultByteSize - 1,
                                generateS390RightAlignedMemoryReference(*tmpMR, node, 0, cg),
                                generateS390RightAlignedMemoryReference(*tmpMR, node, 0, cg));
         }

      generateSS1Instruction(cg, TR::InstOpCode::MVC, node,
                             sourceSize - 1,
                             generateS390RightAlignedMemoryReference(*tmpMR, node, 0, cg),
                             sourceMR);

      generateSS3Instruction(cg, TR::InstOpCode::SRP, node,
                             tmpResultByteSize - 1,
                             generateS390RightAlignedMemoryReference(*tmpMR, node, 0, cg),
                             shiftAmount, roundAmount);

      generateSS2Instruction(cg, TR::InstOpCode::ZAP, node,
                             resultSize - 1,
                             generateS390RightAlignedMemoryReference(*targetMR, node, 0, cg),
                             tmpResultByteSize - 1,
                             generateS390RightAlignedMemoryReference(*tmpMR, node, 0, cg));

      if (needExtraShift)
         {
         generateSS3Instruction(cg, TR::InstOpCode::SRP, node,
                                resultSize - 1,
                                generateS390RightAlignedMemoryReference(*targetMR, node, 0, cg),
                                -1, 0);
         }
      }

   cg->decReferenceCount(srcNode);
   cg->decReferenceCount(shiftAmountNode);
   node->setRegister(targetReg);
   return targetReg;
   }

TR::Register*
J9::Z::TreeEvaluator::vectorPerformSignOperationHelper(TR::Node *node,
                                                       TR::CodeGenerator *cg,
                                                       bool setPrecision,
                                                       uint32_t precision,
                                                       bool signedStatus,
                                                       SignOperationType signOpType,
                                                       bool signValidityCheck,
                                                       bool digitValidityCheck,
                                                       int32_t sign,
                                                       bool setConditionCode,
                                                       bool ignoreDecimalOverflow)
   {
   TR::Register *targetReg = cg->allocateRegister(TR_VRF);
   TR::Node *pdNode = node->getFirstChild();

   TR::Register *childReg = cg->evaluate(pdNode);

   int32_t numPrecisionDigits = setPrecision ? precision : TR_MAX_INPUT_PACKED_DECIMAL_PRECISION;
   if (numPrecisionDigits > TR_MAX_INPUT_PACKED_DECIMAL_PRECISION)
      {
      numPrecisionDigits = TR_MAX_INPUT_PACKED_DECIMAL_PRECISION;
      }

   uint8_t constImm3 = numPrecisionDigits;

   if (ignoreDecimalOverflow)
      {
      constImm3 |= 0x80;
      }

   // Bit 4-5 Sign Operation, 6 Positive Sign code, 7 Sign validation on V2
   uint8_t constImm4 = signOpType << 2; 

   if (signOpType == SignOperationType::setSign)
      {
      switch (sign)
         {
         case TR_PREFERRED_PLUS_CODE:
         case TR_ALTERNATE_PLUS_CODE:
         case TR_ZONED_PLUS:
            constImm4 |= 0x1;
            break;
         case TR_PREFERRED_MINUS_CODE:
         case TR_ALTERNATE_MINUS_CODE:
            break;
         default:
            TR_ASSERT_FATAL(false, "Packed Decimal sign code 0x%x is invalid", sign);
            break;
         }
      }

   // If signedStatus is true it means signed so use 0xC instead of 0xF
   constImm4 |= (signedStatus ? 0x0 : 0x2 );
   constImm4 |= (signValidityCheck ? 0x1 : 0x0);
   constImm4 |= (digitValidityCheck ? 0x0 : 0x80);

   // Current use of TR::pdclean does not want to modifyprecision or set condition code.
   // TODO: We can probably come up with more complex optimization that will collapse modify precision and TR::setsign
   // or TR::pdclean to one instruction.
   generateVRIgInstruction(cg, TR::InstOpCode::VPSOP, node, targetReg, childReg, constImm3, constImm4, setConditionCode);

   node->setRegister(targetReg);
   cg->decReferenceCount(pdNode);
   return targetReg;
   }

TR::Register *
J9::Z::TreeEvaluator::generateVectorBinaryToPackedConversion(TR::Node * node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator * cg)
   {
   TR_ASSERT(op == TR::InstOpCode::VCVD || op == TR::InstOpCode::VCVDG,
              "unexpected opcode in gen vector i2pd\n");

   TR::Register *vTargetReg = cg->allocateRegister(TR_VRF);
   TR::Node * firstChild = node->getFirstChild();
   TR::Register *sourceReg = cg->evaluate(firstChild);
   bool isUseRegPair = (op == TR::InstOpCode::VCVDG && sourceReg->getRegisterPair());

   if (isUseRegPair)
      {
      TR::Register *tempReg = cg->allocateRegister();
      generateRSInstruction(cg, TR::InstOpCode::SLLG, node, tempReg, sourceReg->getRegisterPair()->getHighOrder(), 32);
      generateRRInstruction(cg, TR::InstOpCode::LR, node, tempReg, sourceReg->getRegisterPair()->getLowOrder());
      sourceReg = tempReg;
      }

   uint8_t decimalPrecision = node->getDecimalPrecision();

   if (TR::Compiler->target.cpu.getSupportsVectorPackedDecimalEnhancementFacility() && cg->getIgnoreDecimalOverflowException())
      {
      decimalPrecision |= 0x80;
      }

   generateVRIiInstruction(cg, op, node, vTargetReg, sourceReg, decimalPrecision, 0x1);

   if (isUseRegPair)
      {
      cg->stopUsingRegister(sourceReg);
      }

   cg->decReferenceCount(firstChild);
   node->setRegister(vTargetReg);
   return vTargetReg;
   }

TR::Register *
J9::Z::TreeEvaluator::pdshlVectorEvaluatorHelper(TR::Node *node, TR::CodeGenerator * cg)
   {
   TR::Register * targetReg = NULL;
   TR::Node *firstChild = node->getChild(0);
   TR::Node *shiftAmountNode = node->getNumChildren() > 1 ? node->getSecondChild() : NULL;
   TR_ASSERT(shiftAmountNode, "shift amount node should not be null");
   TR_ASSERT(shiftAmountNode->getOpCode().isLoadConst() && shiftAmountNode->getOpCode().getSize() <= 4,
               "expecting a <= 4 size integral constant PD shift amount\n");

   // If this is a pdshlOverflow with i2pd and other pd-arithmetic operations under it, these vector instructions will
   // truncate the resulting PD by the amount specified by 'decimalPrecision'. Therefore, we can
   // skip the shift and just return i2pd results.
   bool isSkipShift = node->getOpCodeValue() == TR::pdshlOverflow &&
           (firstChild->getOpCodeValue() == TR::i2pd ||
            firstChild->getOpCodeValue() == TR::l2pd ||
            firstChild->getOpCodeValue() == TR::pdadd ||
            firstChild->getOpCodeValue() == TR::pdsub ||
            firstChild->getOpCodeValue() == TR::pdmul ||
            firstChild->getOpCodeValue() == TR::pddiv ||
            firstChild->getOpCodeValue() == TR::pdrem) &&
           firstChild->isSingleRefUnevaluated();

   int32_t shiftAmount = (int32_t)shiftAmountNode->get64bitIntegralValue();
   uint8_t decimalPrecision = node->getDecimalPrecision();

   if (isSkipShift)
      {
      firstChild->setDecimalPrecision(decimalPrecision);
      }

   TR::Register * sourceReg = cg->evaluate(firstChild);

   if (isSkipShift)
      {
      // Passthrough. Assign register to node before decrementing refCount of the firstChild
      // to avoid killing this live register
      targetReg = sourceReg;
      }
   else
      {
      TR_ASSERT_FATAL((shiftAmount >= -32 && shiftAmount <= 31), "TR::pdshl/r shift amount (%d )not in range [-32, 31]", shiftAmount);
      
      if (TR::Compiler->target.cpu.getSupportsVectorPackedDecimalEnhancementFacility() && cg->getIgnoreDecimalOverflowException())
         {
         decimalPrecision |= 0x80;
         }

      targetReg = cg->allocateRegister(TR_VRF);
      generateVRIgInstruction(cg, TR::InstOpCode::VSRP, node, targetReg, sourceReg, decimalPrecision, shiftAmount, 0x01);
      }

   node->setRegister(targetReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(shiftAmountNode);
   return targetReg;
   }

TR::Register *
J9::Z::TreeEvaluator::pdshrVectorEvaluatorHelper(TR::Node *node, TR::CodeGenerator * cg)
   {
   TR::Node *srcNode = node->getChild(0);
   TR::Node *shiftAmountNode = node->getNumChildren() > 1 ? node->getChild(1) : NULL;
   TR_ASSERT(shiftAmountNode != NULL, "pdshrVectorEvaluatorHelper is expecting a shiftAmountNode as child-1\n");
   TR_ASSERT(shiftAmountNode->getOpCode().isLoadConst() && shiftAmountNode->getOpCode().getSize() <= 4,
              "expecting a <= 4 size integral constant PD shift amount\n");

   int32_t shiftAmount = (int32_t)shiftAmountNode->get32bitIntegralValue();
   TR_ASSERT((shiftAmount >=0 || shiftAmount <= 31),"unexpected TR::pdshr shift amount of %d\n",shiftAmount);

   //set shift amount and round amount
   shiftAmount *= -1;                 // right shift is negative
   shiftAmount &= 0x0000007F;         // clear off top bits

   TR::Node *roundAmountNode = node->getChild(2);
   TR_ASSERT( roundAmountNode->getOpCode().isLoadConst(),"excepting pdshr round amount to be a const\n");
   int32_t roundAmount = roundAmountNode->get32bitIntegralValue();
   TR_ASSERT(roundAmount == 0 || roundAmount == 5, "round amount should be 0 or 5 and not %d\n",roundAmount);
   if (roundAmount)
      {
      shiftAmount |= 0x80;       //set the round bit in the shift amount. (immediate3 field in VRIg)
      }

   // Get PD value
   TR::Register * pdValueReg = cg->evaluate(srcNode);
   TR::Register* targetReg = cg->allocateRegister(TR_VRF);
   uint8_t decimalPrecision = node->getDecimalPrecision();

   if (TR::Compiler->target.cpu.getSupportsVectorPackedDecimalEnhancementFacility() && cg->getIgnoreDecimalOverflowException())
      {
      decimalPrecision |= 0x80;
      }

   // Perform shift and set condition code on overflows
   generateVRIgInstruction(cg, TR::InstOpCode::VSRP, node, targetReg, pdValueReg, decimalPrecision, shiftAmount, 0x1);

   node->setRegister(targetReg);

   cg->decReferenceCount(srcNode);
   cg->decReferenceCount(shiftAmountNode);
   cg->decReferenceCount(roundAmountNode);

   return targetReg;
   }
