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

#ifdef TR_TARGET_ARM
#include <stdint.h>
#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "thrdsup.h"
#include "thrtypes.h"
#include "arm/codegen/ARMInstruction.hpp"
#include "arm/codegen/ARMOperand2.hpp"
#include "arm/codegen/J9ARMSnippet.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Machine.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/jittypes.h"
#include "env/CompilerEnv.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Bit.hpp"
#include "env/VMJ9.h"

#ifdef J9VM_GC_ALIGN_OBJECTS
#define OBJECT_ALIGNMENT 8
#else
#define OBJECT_ALIGNMENT (TR::Compiler->om.sizeofReferenceAddress())
#endif

static inline TR::Instruction *genNullTest(TR::CodeGenerator *cg,
                                          TR::Node          *node,
                                          TR::Register      *objectReg,
                                          TR::Register      *scratchReg,
                                          TR::Instruction   *cursor)
   {
   TR::Instruction *instr;
   uint32_t base, rotate;
   if (constantIsImmed8r(NULLVALUE, &base, &rotate))
      {
      instr = generateSrc1ImmInstruction(cg, ARMOp_cmp, node, objectReg, base, rotate, cursor);
      }
   else
      {
      instr = armLoadConstant(node, NULLVALUE, scratchReg, cg, cursor);
      instr = generateSrc2Instruction(cg, ARMOp_cmp, node, objectReg, scratchReg, instr);
      }

   return instr;
   }


static TR::Register *nonFixedDependency(TR::CodeGenerator                   *cg,
                                       TR::RegisterDependencyConditions *conditions,
                                       TR::Register                        *nonFixedReg,
                                       TR_RegisterKinds                    kind)
   {
   if (nonFixedReg == NULL)
      nonFixedReg = cg->allocateRegister(kind);

   TR::addDependency(conditions, nonFixedReg, TR::RealRegister::NoReg, kind, cg);
   return nonFixedReg;
   }

static int32_t numberOfRegisterCandidate(TR::Node *depNode, TR_RegisterKinds kind)
   {
   int32_t idx, result = 0;

   for (idx = 0; idx < depNode->getNumChildren(); idx++)
      {
      TR::Node *child = depNode->getChild(idx);
      TR::Register *reg;
      /*
       * The direct child node has HighGlobalRegisterNumber even if it is PassThrough
       * The child of PassThrough node has garbage data for HighGlobalRegisterNumber.
       */
      TR_GlobalRegisterNumber highNumber = child->getHighGlobalRegisterNumber();

      if (child->getOpCodeValue() == TR::PassThrough)
         child = child->getFirstChild();
      reg = child->getRegister();
      if (reg != NULL && reg->getKind() == kind)
         {
         result += 1;
         if (kind == TR_GPR && highNumber > -1)
            result += 1;
         }
      }
   return (result);
   }

TR::Instruction *OMR::ARM::TreeEvaluator::generateVFTMaskInstruction(TR::CodeGenerator *cg, TR::Node *node, TR::Register *dstReg, TR::Register *srcReg, TR::Instruction *prev)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   uintptrj_t mask = TR::Compiler->om.maskOfObjectVftField();
#ifdef OMR_GC_COMPRESSED_POINTERS
   bool isCompressed = true;
#else
   bool isCompressed = false;
#endif
   if (~mask == 0)
      {
      // no mask instruction required
      return prev;
      }
   else
      {
      return generateTrg1Src1ImmInstruction(cg, ARMOp_bic, node, dstReg, srcReg, ~mask, 0, prev);
      }
   }

TR::Instruction *OMR::ARM::TreeEvaluator::generateVFTMaskInstruction(TR::CodeGenerator *cg, TR::Node *node, TR::Register *reg, TR::Instruction *prev)
   {
   return generateVFTMaskInstruction(cg, node, reg, reg, prev);
   }

static TR::Instruction *genTestIsSuper(TR::CodeGenerator *cg,
                                      TR::Node          *node,
                                      TR::Register      *objClassReg,
                                      TR::Register      *castClassReg,
                                      TR::Register      *scratch1Reg,
                                      TR::Register      *scratch2Reg,
                                      int32_t           castClassDepth,
                                      TR::LabelSymbol    *failLabel,
                                      TR::Instruction   *cursor,
                                      TR::LabelSymbol    *doneLabel)
   {
   uint32_t base, rotate;
   uint32_t shiftAmount      = leadingZeroes(J9AccClassDepthMask);
   int32_t  superClassOffset = castClassDepth << 2;
   bool     outOfBound       = (superClassOffset > UPPER_IMMED12 || superClassOffset < LOWER_IMMED12);
   bool     regDepth         = !constantIsImmed8r(castClassDepth, &base, &rotate);

   TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(objClassReg, offsetof(J9Class, classDepthAndFlags), cg);
   cursor = generateTrg1MemInstruction(cg, ARMOp_ldr, node, scratch1Reg, tempMR, cursor);

   cursor = generateTrg1ImmInstruction(cg, ARMOp_mvn, node, scratch2Reg, 0, 0, cursor);

   TR_ARMOperand2 *operand2 = new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegLSRImmed, scratch2Reg, shiftAmount);
   cursor = generateTrg1Src2Instruction(cg, ARMOp_and, node, scratch1Reg, scratch1Reg, operand2, cursor);

   if (outOfBound || regDepth)
      {
      cursor = armLoadConstant(node, castClassDepth, scratch2Reg, cg, cursor);
      cursor = generateSrc2Instruction(cg, ARMOp_cmp, node, scratch1Reg, scratch2Reg, cursor);
      }
   else
      {
      cursor = generateSrc1ImmInstruction(cg, ARMOp_cmp, node, scratch1Reg, base, rotate, cursor);
      }

   if (failLabel)
      {
      cursor = generateConditionalBranchInstruction(cg, node, ARMConditionCodeLE, failLabel, cursor);
      }
   else
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      cursor = generateImmSymInstruction(cg, ARMOp_bl, node, (uintptr_t)symRef->getMethodAddress(), NULL, symRef, NULL, NULL, ARMConditionCodeLE);
      cursor->ARMNeedsGCMap(0xFFFFFFFF);
      cg->machine()->setLinkRegisterKilled(true);
      }


   tempMR = new (cg->trHeapMemory()) TR::MemoryReference(objClassReg, offsetof(J9Class,superclasses), cg);
   cursor = generateTrg1MemInstruction(cg, ARMOp_ldr, node, scratch1Reg, tempMR, cursor);

   if (outOfBound || regDepth)
      {
      tempMR = new (cg->trHeapMemory()) TR::MemoryReference(scratch1Reg, scratch2Reg, 4, cg);
      cursor = generateTrg1MemInstruction(cg, ARMOp_ldr, node, scratch1Reg, tempMR, cursor);
      }
   else
      {
      tempMR = new (cg->trHeapMemory()) TR::MemoryReference(scratch1Reg, superClassOffset, cg);
      cursor = generateTrg1MemInstruction(cg, ARMOp_ldr, node, scratch1Reg, tempMR, cursor);
      }

   cursor = generateSrc2Instruction(cg, ARMOp_cmp, node, scratch1Reg, castClassReg, cursor);
   return cursor;
   }


TR::Register *OMR::ARM::TreeEvaluator::VMcheckcastEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node            *objNode         = node->getFirstChild();
   TR::Node            *castClassNode   = node->getSecondChild();
   TR::SymbolReference *castClassSymRef = castClassNode->getSymbolReference();
   TR::Compilation *comp = TR::comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(6, 6, cg->trMemory());

   int32_t castClassDepth = -1;
   bool testEqualClass = instanceOfOrCheckCastNeedEqualityTest(node, cg);
   bool testCastClassIsSuper = instanceOfOrCheckCastNeedSuperTest(node, cg);
   TR::StaticSymbol *castClassSym;
   TR_OpaqueClassBlock *clazz;

   bool isCheckCastOrNullCheck    = (node->getOpCodeValue() == TR::checkcastAndNULLCHK);

   if (testCastClassIsSuper)
      {
      castClassSym = castClassSymRef->getSymbol()->getStaticSymbol();
      clazz = (TR_OpaqueClassBlock *) castClassSym->getStaticAddress();
      if (fej9->classHasBeenExtended(clazz) || comp->getMethodHotness() < warm || !comp->isRecompilationEnabled() )
         {
         castClassDepth = TR::Compiler->cls.classDepthOf(clazz);
         }
      else
         {
         testCastClassIsSuper = false;
         }
      }

   TR::Register *objReg       = cg->evaluate(objNode);
   TR::Register *castClassReg = cg->evaluate(castClassNode);
   TR::Register *objClassReg  = cg->allocateRegister();

   TR::addDependency(deps, objReg, TR::RealRegister::gr1, TR_GPR, cg);
   TR::addDependency(deps, castClassReg, TR::RealRegister::gr0, TR_GPR, cg);

   cg->machine()->setLinkRegisterKilled(true);

   if (!testEqualClass && !testCastClassIsSuper)
      {
      TR::SymbolReference *symRef  = node->getSymbolReference();
      TR::Instruction     *gcPoint = generateImmSymInstruction(cg, ARMOp_bl, node, (uint32_t)symRef->getMethodAddress(), deps, symRef);
      gcPoint->ARMNeedsGCMap(0xFFFFFFFF);
      deps->stopAddingConditions();
      cg->decReferenceCount(objNode);
      cg->decReferenceCount(castClassNode);
      return NULL;
      }

   TR::LabelSymbol *doneLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   TR::Instruction *gcPoint;

   TR::addDependency(deps, objClassReg, TR::RealRegister::NoReg, TR_GPR, cg);

   TR::Instruction *implicitExceptionPointInstr = NULL;
   if (!objNode->isNonNull())
      {
      if (!isCheckCastOrNullCheck)
         {
         genNullTest(cg, node, objReg, objClassReg, NULL);
         generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, doneLabel);
         }
      }

   implicitExceptionPointInstr = generateTrg1MemInstruction(cg, ARMOp_ldr, node, objClassReg,
                                 new (cg->trHeapMemory()) TR::MemoryReference(objReg, (int32_t)TR::Compiler->om.offsetOfObjectVftField(), cg));
   generateVFTMaskInstruction(cg, node, objClassReg);

   TR::SymbolReference *symRef = node->getSymbolReference();

   if (testEqualClass)
      {
      generateSrc2Instruction(cg, ARMOp_cmp, node, objClassReg, castClassReg);
      if (testCastClassIsSuper)
         generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, doneLabel);
      else
         {
         gcPoint = generateImmSymInstruction(cg, ARMOp_bl, node, (uintptr_t)symRef->getMethodAddress(), NULL, symRef, NULL, NULL, ARMConditionCodeNE);
         gcPoint->ARMNeedsGCMap(0xFFFFFFFF);
         }
      }

   if (testCastClassIsSuper)
      {
      TR::Register *scratch1Reg = cg->allocateRegister();
      TR::Register *scratch2Reg = cg->allocateRegister();
      TR::addDependency(deps, scratch1Reg, TR::RealRegister::NoReg, TR_GPR, cg);
      TR::addDependency(deps, scratch2Reg, TR::RealRegister::NoReg, TR_GPR, cg);

      genTestIsSuper(cg, node, objClassReg, castClassReg, scratch1Reg,
                     scratch2Reg, castClassDepth, NULL, NULL, doneLabel);
      gcPoint = generateImmSymInstruction(cg, ARMOp_bl, node, (uintptr_t)symRef->getMethodAddress(), NULL, symRef, NULL, NULL, ARMConditionCodeNE);
      gcPoint->ARMNeedsGCMap(0xFFFFFFFF);
      }

   if (isCheckCastOrNullCheck &&
         implicitExceptionPointInstr &&
         !objNode->isNonNull())
      {
      cg->setImplicitExceptionPoint(implicitExceptionPointInstr);
      implicitExceptionPointInstr->setNeedsGCMap();
      // find the bytecodeinfo of the
      // NULLCHK that was compacted
      TR::Node *implicitExceptionPointNode = comp->findNullChkInfo(node);
      implicitExceptionPointInstr->setNode(implicitExceptionPointNode);
      }

   generateLabelInstruction(cg, ARMOp_label, node, doneLabel, deps);
   deps->stopAddingConditions();

   cg->decReferenceCount(objNode);
   cg->decReferenceCount(castClassNode);
   return NULL;
   }


TR::Register *OMR::ARM::TreeEvaluator::VMifInstanceOfEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = TR::comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   bool branchOn1            = false;

   TR::Register    *resultReg = NULL;
   TR::LabelSymbol *doneLabel = NULL;
   TR::LabelSymbol *trueLabel = NULL;
   TR::LabelSymbol *res0Label = NULL;
   TR::LabelSymbol *res1Label = NULL;

   TR::RegisterDependencyConditions *conditions = NULL;
   TR::RegisterDependencyConditions *deps       = NULL;

   TR::ILOpCodes        opCode          = node->getOpCodeValue();
   TR::Node            *instanceOfNode  = node->getFirstChild();
   TR::Node            *valueNode       = node->getSecondChild();
   TR::Node            *depNode         = node->getNumChildren() == 3 ? node->getChild(2) : NULL;
   TR::Node            *objectNode      = instanceOfNode->getFirstChild();
   TR::Node            *castClassNode   = instanceOfNode->getSecondChild();
   int32_t             value           = valueNode->getInt();
   int32_t             numDeps         = depNode ? depNode->getNumChildren() : 0;
   int32_t             castClassDepth  = -1;
   TR::SymbolReference *castClassSymRef = castClassNode->getSymbolReference();
   TR::LabelSymbol      *branchLabel     = node->getBranchDestination()->getNode()->getLabel();
   bool testEqualClass = instanceOfOrCheckCastNeedEqualityTest(instanceOfNode, cg);
   bool testCastClassIsSuper = instanceOfOrCheckCastNeedSuperTest(instanceOfNode, cg);
   bool isFinalClass = (castClassSymRef == NULL) ? false : castClassSymRef->isNonArrayFinal(comp);
   bool needsHelperCall = TR::TreeEvaluator::instanceOfNeedHelperCall(testCastClassIsSuper, isFinalClass);

   // If the result itself is assigned to a global register, we still have to do
   // something special...
   bool needResult = (instanceOfNode->getReferenceCount() > 1);

   int32_t i;
   if (depNode != NULL)
      {
      if (!needsHelperCall && numberOfRegisterCandidate(depNode, TR_GPR) + 6 > cg->getMaximumNumberOfGPRsAllowedAcrossEdge(node))
         return (TR::Register *)1;

      TR::ARMLinkageProperties properties = cg->getLinkage()->getProperties();
      for (i = 0; i < depNode->getNumChildren(); i++)
          {
    	  TR::Node *childNode = depNode->getChild(i);
          int32_t  regIndex = cg->getGlobalRegister(depNode->getChild(i)->getGlobalRegisterNumber());
          int32_t highIndex = childNode->getHighGlobalRegisterNumber();

          if (needsHelperCall)
        	 {
             if (highIndex > -1)
                highIndex = cg->getGlobalRegister(highIndex);
             if (!properties.getPreserved((TR::RealRegister::RegNum) regIndex) || (highIndex > -1 && !properties.getPreserved((TR::RealRegister::RegNum) highIndex)))
                return (TR::Register *)1;
        	 }
          else
             {
             // Below check code taken from PPC.
             if (childNode->getOpCodeValue() == TR::PassThrough)
                childNode = childNode->getFirstChild();
             if ((childNode == instanceOfNode || childNode == castClassNode) && regIndex == TR::RealRegister::gr0)
                return ((TR::Register *) 1);
             }
          }
      }

   if (testEqualClass || testCastClassIsSuper)
      {
      doneLabel = generateLabelSymbol(cg);
      }

   if ((opCode == TR::ificmpeq && value == 1) ||
       (opCode != TR::ificmpeq && value == 0))
      {
      res0Label = doneLabel;
      res1Label = branchLabel;
      branchOn1 = true;
      }
   else
      {
      res0Label = branchLabel;
      res1Label = doneLabel;
      branchOn1 = false;
      }

   TR::Register    *objectReg;
   TR::Register    *objClassReg;
   TR::Register    *castClassReg;
   TR::Register    *scratch1Reg;
   TR::Register    *scratch2Reg;
   TR::Instruction *iCursor;

   if (needsHelperCall)
      {
      TR::ILOpCodes childOpCode = instanceOfNode->getOpCodeValue();

      TR::Node::recreate(instanceOfNode, TR::icall);
      directCallEvaluator(instanceOfNode, cg);
      TR::Node::recreate(instanceOfNode, childOpCode);

      iCursor = cg->getAppendInstruction();
      while (iCursor->getOpCodeValue() != ARMOp_bl)
         iCursor = iCursor->getPrev();
      conditions = iCursor->getDependencyConditions();
      iCursor = iCursor->getPrev();
      objectReg= conditions->searchPreConditionRegister(TR::RealRegister::gr1);
      castClassReg = conditions->searchPreConditionRegister(TR::RealRegister::gr0);
      scratch1Reg = conditions->searchPreConditionRegister(TR::RealRegister::gr2);
      scratch2Reg = conditions->searchPreConditionRegister(TR::RealRegister::gr3);
      objClassReg = conditions->searchPreConditionRegister(TR::RealRegister::gr4);

      if (depNode !=NULL)
         {
         cg->evaluate(depNode);
         deps = generateRegisterDependencyConditions(cg, depNode, 0, &iCursor);
         }

      if (testEqualClass || testCastClassIsSuper)
         {
         if (needResult)
            {
            resultReg = conditions->searchPreConditionRegister(TR::RealRegister::gr5);
            iCursor = generateTrg1ImmInstruction(cg, ARMOp_mov, node, resultReg, 0, 0, iCursor);
            }
         if (!objectNode->isNonNull())
            {
            iCursor = genNullTest(cg, objectNode, objectReg, scratch1Reg, iCursor);
            iCursor = generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, res0Label, iCursor);
            }
         }
      }
   else
      {
      bool   earlyEval = (castClassNode->getOpCodeValue() != TR::loadaddr ||
                          castClassNode->getReferenceCount() > 1)?true:false;
      bool   objInDep = false, castClassInDep = false;
      if (depNode != NULL)
         {
         cg->evaluate(depNode);
         deps = generateRegisterDependencyConditions(cg, depNode, 6);
         for (i = 0; i < numDeps; i++)
            {
            TR::Node *grNode = depNode->getChild(i);
            if (grNode->getOpCodeValue() == TR::PassThrough)
               grNode = grNode->getFirstChild();
            if (grNode == objectNode)
               objInDep = true;
            if (grNode == castClassNode)
               castClassInDep = true;
            }
         }
      if (deps == NULL)
         {
         conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(6, 6, cg->trMemory());
         }
      else
         {
         conditions = deps;
         }

      if (needResult)
         {
         resultReg = nonFixedDependency(cg, conditions, NULL, TR_GPR);
         }

      objClassReg = nonFixedDependency(cg, conditions, NULL, TR_GPR);

      objectReg = cg->evaluate(objectNode);
      if (!objInDep)
         objectReg = nonFixedDependency(cg, conditions, objectReg, TR_GPR);

      if (earlyEval)
         {
         castClassReg = cg->evaluate(castClassNode);
         if (!castClassInDep)
            castClassReg = nonFixedDependency(cg, conditions, castClassReg, TR_GPR);
         }

      if (needResult)
         {
         generateTrg1ImmInstruction(cg, ARMOp_mov, node, resultReg, 0, 0);
         }

      if (!objectNode->isNonNull())
         {
         genNullTest(cg, objectNode, objectReg, objClassReg, NULL);
         generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, res0Label);
         }

      if (!earlyEval)
         {
         castClassReg = cg->evaluate(castClassNode);
         if (!castClassInDep)
            castClassReg = nonFixedDependency(cg, conditions, castClassReg, TR_GPR);
         }

      if (testCastClassIsSuper)
         {
         scratch1Reg = nonFixedDependency(cg, conditions, NULL, TR_GPR);
         scratch2Reg = nonFixedDependency(cg, conditions, NULL, TR_GPR);
         cg->stopUsingRegister(scratch1Reg);
         cg->stopUsingRegister(scratch2Reg);
         }

      iCursor = cg->getAppendInstruction();
      generateLabelInstruction(cg, ARMOp_label, node, doneLabel, conditions);
      conditions->stopAddingConditions();
      cg->decReferenceCount(objectNode);
      cg->decReferenceCount(castClassNode);
      }

   if (testEqualClass || testCastClassIsSuper)
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(objectReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg);
      iCursor = generateTrg1MemInstruction(cg, ARMOp_ldr, node, objClassReg, tempMR, iCursor);
      iCursor = generateVFTMaskInstruction(cg, node, objClassReg, iCursor);
      }

   if (testEqualClass)
      {
      iCursor = generateSrc2Instruction(cg, ARMOp_cmp, node, objClassReg, castClassReg, iCursor);

      if (testCastClassIsSuper)
         {
         if (needResult)
            {
            trueLabel = generateLabelSymbol(cg);
            iCursor = generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, trueLabel, iCursor);
            }
         else
            {
            iCursor = generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, res1Label, iCursor);
            }
         }
      else if (needsHelperCall)
         {
         if (needResult)
            {
            iCursor = generateTrg1ImmInstruction(cg, ARMOp_mov, node, resultReg, 1, 0, iCursor);
            }
         iCursor = generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, res1Label, iCursor);
         }
      else
         {
         if (needResult)
            {
            iCursor = generateTrg1ImmInstruction(cg, ARMOp_mov, node, resultReg, 1, 0, iCursor);
            iCursor->setConditionCode(ARMConditionCodeEQ);
            }

         if (branchOn1)
            {
            iCursor = generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, res1Label, iCursor);
            }
         else
            {
            iCursor = generateConditionalBranchInstruction(cg, node, ARMConditionCodeNE, res0Label, iCursor);
            }
         }
      }

   if (testCastClassIsSuper)
      {
      TR::StaticSymbol *castClassSym;

      castClassSym = castClassSymRef->getSymbol()->getStaticSymbol();
      void * clazz = castClassSym->getStaticAddress();
      castClassDepth = TR::Compiler->cls.classDepthOf((TR_OpaqueClassBlock *) clazz);

      iCursor = genTestIsSuper(cg, node, objClassReg, castClassReg, scratch1Reg, scratch2Reg, castClassDepth, res0Label, iCursor, NULL);
      iCursor = generateConditionalBranchInstruction(cg, node, ARMConditionCodeNE, res0Label, iCursor);
      if (trueLabel == NULL)
         {
         if (needResult)
            {
            iCursor = generateTrg1ImmInstruction(cg, ARMOp_mov, node, resultReg, 1, 0, iCursor);
            }
         }
      else
         {
         iCursor = generateLabelInstruction(cg, ARMOp_label, node, trueLabel, iCursor);
         iCursor = generateTrg1ImmInstruction(cg, ARMOp_mov, node, resultReg, 1, 0, iCursor);
         }

      if (branchOn1)
         {
         iCursor = generateLabelInstruction(cg, ARMOp_b, node, res1Label, iCursor);
         }
      }

   if (needsHelperCall)
      {
      TR::Register *callResult = instanceOfNode->getRegister();
      TR::RegisterDependencyConditions *newDeps;

      if (resultReg == NULL)
         {
         resultReg = callResult;
         }
      else
         {
         generateTrg1Src1Instruction(cg, ARMOp_mov, node, resultReg, callResult);
         }

      generateSrc1ImmInstruction(cg, ARMOp_cmp, node, resultReg, 0, 0);

      if (depNode != NULL)
         {
         newDeps = conditions->cloneAndFix(cg, deps);
         }
      else
         {
         newDeps = conditions->cloneAndFix(cg);
         }

      if (branchOn1)
         {
         if (doneLabel == NULL)
            generateConditionalBranchInstruction(cg, node, ARMConditionCodeNE, res1Label, newDeps);
         else
            generateConditionalBranchInstruction(cg, node, ARMConditionCodeNE, res1Label);
         }
      else
         {
         if (doneLabel == NULL)
            generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, res0Label, newDeps);
         else
            generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, res0Label);
         }

      if (doneLabel != NULL)
         {
         generateLabelInstruction(cg, ARMOp_label, node, doneLabel, newDeps);
         newDeps->stopAddingConditions();
         }

#if 0 // TODO:PPC has this fixup.  It maybe a fix to something so keeping here as note.  See J9PPCEvaluator.cpp for reviveResultRegister() imple.
      // We have to revive the resultReg here. Should revisit to see how to use registers better
      // (e.g., reducing the chance of moving around).
      if (resultReg != callResult)
         reviveResultRegister(resultReg, callResult, cg);
#endif
      }

   if (resultReg != instanceOfNode->getRegister())
      instanceOfNode->setRegister(resultReg);
   cg->decReferenceCount(instanceOfNode);
   cg->decReferenceCount(valueNode);
   if (depNode != NULL)
      cg->decReferenceCount(depNode);
   node->setRegister(NULL);
   return NULL;
   }


TR::Register *OMR::ARM::TreeEvaluator::VMinstanceOfEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = TR::comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   TR::Register   *objectReg, *castClassReg, *objClassReg;
   TR::Register   *resultReg, *scratch1Reg, *scratch2Reg;
   TR::Instruction                     *iCursor;
   TR::RegisterDependencyConditions *conditions;
   TR::LabelSymbol                      *doneLabel, *trueLabel;
   TR::Node                            *objectNode, *castClassNode;
   int32_t        castClassDepth = -1;

   trueLabel = doneLabel = NULL;
   resultReg = NULL;
   objectNode = node->getFirstChild();
   castClassNode = node->getSecondChild();
   TR::SymbolReference * castClassSymRef = castClassNode->getSymbolReference();
   bool testEqualClass = instanceOfOrCheckCastNeedEqualityTest(node, cg);
   bool testCastClassIsSuper = instanceOfOrCheckCastNeedSuperTest(node, cg);
   bool isFinalClass = (castClassSymRef == NULL) ? false : castClassSymRef->isNonArrayFinal(comp);
   bool needsHelperCall = TR::TreeEvaluator::instanceOfNeedHelperCall(testCastClassIsSuper, isFinalClass);

   if (needsHelperCall)
      {
      TR::ILOpCodes   opCode = node->getOpCodeValue();

      TR::Node::recreate(node, TR::icall);
      directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);

      iCursor = cg->getAppendInstruction();
      while (iCursor->getOpCodeValue() != ARMOp_bl)
         iCursor = iCursor->getPrev();
      conditions = iCursor->getDependencyConditions();
      iCursor = iCursor->getPrev();
      objectReg= conditions->searchPreConditionRegister(
                    TR::RealRegister::gr1);
      castClassReg = conditions->searchPreConditionRegister(
                        TR::RealRegister::gr0);
      scratch1Reg = conditions->searchPreConditionRegister(
                       TR::RealRegister::gr2);
      scratch2Reg = conditions->searchPreConditionRegister(
                       TR::RealRegister::gr3);
      objClassReg = conditions->searchPreConditionRegister(
                       TR::RealRegister::gr4);

      if (testEqualClass || testCastClassIsSuper)
         {
         doneLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

         resultReg = conditions->searchPreConditionRegister(
                       TR::RealRegister::gr5);

         iCursor = generateTrg1ImmInstruction(cg, ARMOp_mov, node, resultReg, 0, 0, iCursor);
         if (!objectNode->isNonNull())
            {
            iCursor = genNullTest(cg, objectNode, objectReg, scratch1Reg, iCursor);
            iCursor = generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, doneLabel, iCursor);
            }
         }
      }
   else
      {
      bool   earlyEval = (castClassNode->getOpCodeValue() != TR::loadaddr ||
                          castClassNode->getReferenceCount() > 1)?true:false;

      conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(6, 6, cg->trMemory());

      resultReg = nonFixedDependency(cg, conditions, NULL, TR_GPR);
      objClassReg = nonFixedDependency(cg, conditions, NULL, TR_GPR);

      objectReg = cg->evaluate(objectNode);
      objectReg = nonFixedDependency(cg, conditions, objectReg, TR_GPR);
      if (earlyEval)
         {
         castClassReg = cg->evaluate(castClassNode);
         castClassReg = nonFixedDependency(cg, conditions, castClassReg, TR_GPR);
         }

      doneLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      generateTrg1ImmInstruction(cg, ARMOp_mov, node, resultReg, 0, 0);
      if (!objectNode->isNonNull())
         {
         genNullTest(cg, objectNode, objectReg, objClassReg, NULL);
         generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, doneLabel);
         }

      if (!earlyEval)
         {
         castClassReg = cg->evaluate(castClassNode);
         castClassReg = nonFixedDependency(cg, conditions, castClassReg, TR_GPR);
         }

      if (testCastClassIsSuper)
         {
         scratch1Reg = nonFixedDependency(cg, conditions, NULL, TR_GPR);
         scratch2Reg = nonFixedDependency(cg, conditions, NULL, TR_GPR);
         }

      iCursor = cg->getAppendInstruction();
      generateLabelInstruction(cg, ARMOp_label, node, doneLabel, conditions);
      cg->decReferenceCount(objectNode);
      cg->decReferenceCount(castClassNode);
      }

   if (testEqualClass || testCastClassIsSuper)
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(objectReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg);
      iCursor = generateTrg1MemInstruction(cg, ARMOp_ldr, node, objClassReg, tempMR, iCursor);
      iCursor = generateVFTMaskInstruction(cg, node, objClassReg, iCursor);
      }

   if (testEqualClass)
      {
      iCursor = generateSrc2Instruction(cg, ARMOp_cmp, node, objClassReg, castClassReg, iCursor);

      if (testCastClassIsSuper)
         {
         trueLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         iCursor = generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, trueLabel, iCursor);
         }
      else if (needsHelperCall)
         {
         iCursor = generateTrg1ImmInstruction(cg, ARMOp_mov, node, resultReg, 1, 0, iCursor);
         iCursor = generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, doneLabel, iCursor);
         }
      else
         {
         iCursor = generateTrg1ImmInstruction(cg, ARMOp_mov, node, resultReg, 1, 0, iCursor);
         iCursor->setConditionCode(ARMConditionCodeEQ);
         }
      }

   if (testCastClassIsSuper)
      {
      TR::StaticSymbol *castClassSym;

      castClassSym = castClassSymRef->getSymbol()->getStaticSymbol();
      void * clazz = castClassSym->getStaticAddress();
      castClassDepth = TR::Compiler->cls.classDepthOf((TR_OpaqueClassBlock *) clazz);

      iCursor = genTestIsSuper(cg, node, objClassReg, castClassReg, scratch1Reg, scratch2Reg, castClassDepth, doneLabel, iCursor, NULL);

      // needHelperCall must be false, and the only consumer of "trueLabel" is a branch instruction
      // with the exact same condition as required below. We can take advantage of this situation.

      if (trueLabel != NULL)
         {
         iCursor = generateLabelInstruction(cg, ARMOp_label, node, trueLabel, iCursor);
         }
      iCursor = generateTrg1ImmInstruction(cg, ARMOp_mov, node, resultReg, 1, 0, iCursor);
      iCursor->setConditionCode(ARMConditionCodeEQ);
      }

   if (needsHelperCall)
      {
      TR::Register *callResult = node->getRegister();
      if (resultReg == NULL)
         {
         resultReg = callResult;
         }
      else
         {
         generateTrg1Src1Instruction(cg, ARMOp_mov, node, resultReg, callResult);
         generateLabelInstruction(cg, ARMOp_label, node, doneLabel, conditions->cloneAndFix(cg));
         }
      }
   if (resultReg != node->getRegister())
      node->setRegister(resultReg);
   return resultReg;
   }


TR::Register *OMR::ARM::TreeEvaluator::VMmonexitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = TR::comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   int32_t lwOffset = fej9->getByteOffsetToLockword(cg->getMonClass(node));

   if (comp->getOption(TR_OptimizeForSpace) ||
       (comp->getOption(TR_FullSpeedDebug) /*&& !comp->getOption(TR_EnableLiveMonitorMetadata)*/) ||
       comp->getOption(TR_DisableInlineMonEnt) ||
       lwOffset <= 0)
      {
      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node::recreate(node, TR::call);
      TR::Register *targetRegister = directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);
      return targetRegister;
      }

   TR::Node     *objNode    = node->getFirstChild();
   TR::Register *objReg     = cg->evaluate(objNode);
   TR::Register *monitorReg = cg->allocateRegister();
   TR::Register *flagReg    = cg->allocateRegister();
   TR::Register *metaReg    = cg->getMethodMetaDataRegister();

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(3, 3, cg->trMemory());
   TR::addDependency(deps, objReg, TR::RealRegister::gr0, TR_GPR, cg);
   TR::addDependency(deps, monitorReg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(deps, flagReg, TR::RealRegister::NoReg, TR_GPR, cg);

   TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(objReg, lwOffset, cg);
   generateTrg1MemInstruction(cg, ARMOp_ldr, node, monitorReg, tempMR);

#if !defined(J9VM_TASUKI_LOCKS_SINGLE_SLOT)
   tempMR = new (cg->trHeapMemory()) TR::MemoryReference(objReg, TR::Compiler->om.offsetOfHeaderFlags(), cg);
   generateTrg1MemInstruction(cg, ARMOp_ldr, node, flagReg, tempMR);
#endif

   generateSrc2Instruction(cg, ARMOp_cmp, node, monitorReg, metaReg);

#if !defined(J9VM_TASUKI_LOCKS_SINGLE_SLOT)
   TR_ASSERT(OBJECT_HEADER_FLAT_LOCK_CONTENTION == 0x80000000, "Constant redefined.");
   TR::Instruction *instr = generateTrg1Src1ImmInstruction(cg, ARMOp_and_r, node, flagReg, flagReg, 2, 30);
   instr->setConditionCode(ARMConditionCodeEQ);
#else
   TR::Instruction *instr = generateTrg1ImmInstruction(cg, ARMOp_mov, node, flagReg, 0, 0);
   instr->setConditionCode(ARMConditionCodeEQ);

   if (TR::Compiler->target.isSMP() && TR::Compiler->target.cpu.id() != TR_DefaultARMProcessor)
      {
      //instr = generateInstruction(cg, (TR::Compiler->target.cpu.id() == TR_ARMv6) ? ARMOp_dmb_v6 : ARMOp_dmb, node);
      instr = generateInstruction(cg, ARMOp_dmb_v6 , node); // v7 version is unconditional
      instr->setConditionCode(ARMConditionCodeEQ);
      }
#endif

   tempMR = new (cg->trHeapMemory()) TR::MemoryReference(objReg, lwOffset, cg);
   instr  = generateMemSrc1Instruction(cg, ARMOp_str, node, tempMR, flagReg);
   instr->setConditionCode(ARMConditionCodeEQ);

#ifdef J9VM_TASUKI_LOCKS_SINGLE_SLOT
   // Branch to decLabel in snippet first
   TR::LabelSymbol *decLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *callLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *doneLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::Snippet *snippet = new (cg->trHeapMemory()) TR::ARMMonitorExitSnippet(cg, node, lwOffset, decLabel, callLabel, doneLabel);

   generateConditionalBranchInstruction(cg, node, ARMConditionCodeNE, decLabel);

   generateLabelInstruction(cg, ARMOp_label, node, doneLabel, deps);
   cg->addSnippet(snippet);
   doneLabel->setEndInternalControlFlow();
#else
   // Branch directly to jitMonitorExit
   TR::SymbolReference *symRef = node->getSymbolReference();
   instr = generateImmSymInstruction(cg, ARMOp_bl, node, (uint32_t)symRef->getMethodAddress(), deps, symRef);
   instr->setConditionCode(ARMConditionCodeNE);
   instr->ARMNeedsGCMap(0xFFFFFFFF);
   cg->machine()->setLinkRegisterKilled(true);
#endif
   cg->decReferenceCount(objNode);
   return NULL;
   }


static void genHeapAlloc(TR::CodeGenerator  *cg,
                         TR::Node           *node,
                         TR::Instruction *  &iCursor,
                         bool               isVariableLen,
                         TR::Register       *enumReg,
                         TR::Register       *resReg,
                         TR::Register       *zeroReg,
                         TR::Register       *dataSizeReg,
                         TR::Register       *heapTopReg,
                         TR::Register       *sizeReg,
                         TR::LabelSymbol     *callLabel,
                         int32_t            allocSize)
   {
   if (TR::Options::getCmdLineOptions()->realTimeGC())
      {
      TR_ASSERT(0, "genHeapAlloc() not supported for RT");
      return;
      }

   TR::Register     *metaReg = cg->getMethodMetaDataRegister();
   uint32_t         base, rotate;
   bool             sizeInReg = (isVariableLen || !constantIsImmed8r(allocSize, &base, &rotate));

   TR::ILOpCodes    opCode = node->getOpCodeValue();
   bool             isArrayAlloc = (opCode == TR::newarray || opCode == TR::anewarray);

   if (isVariableLen)
      {
      // This is coming from the cg setting: less than 0x10000000 will not wrap around
      iCursor = generateTrg1Src1ImmInstruction(cg, ARMOp_and_r, node, zeroReg, enumReg, 0x000000FF, 24, iCursor);
      iCursor = generateConditionalBranchInstruction(cg, node, ARMConditionCodeNE, callLabel, iCursor);

      if (isArrayAlloc)
         {
         // Zero length arrays are discontiguous
         iCursor = generateSrc2Instruction(cg, ARMOp_tst, node, enumReg, enumReg, iCursor);
         iCursor = generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, callLabel, iCursor);
         }
      }

   TR::MemoryReference *tempMR;
   tempMR = new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapAlloc), cg);
   iCursor = generateTrg1MemInstruction(cg, ARMOp_ldr, node, resReg, tempMR, iCursor);
   //tempMR = new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapTop), cg);
   //iCursor = generateTrg1MemInstruction(cg, ARMOp_ldr, node, heapTopReg, tempMR, iCursor);

   if (sizeInReg)
      {
      if (isVariableLen)
         {
         int32_t elementSize = TR::Compiler->om.getSizeOfArrayElement(node);
         int32_t round = (elementSize >= OBJECT_ALIGNMENT) ? 0 : OBJECT_ALIGNMENT; // zero indicates no rounding is necessary
         bool headerAligned = allocSize % OBJECT_ALIGNMENT ? 0 : 1;

         if (elementSize >= 4)
            {
            TR_ARMOperand2 *operand2 = new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegLSLImmed, enumReg, trailingZeroes(elementSize));
            iCursor = generateTrg1Src1Instruction(cg, ARMOp_mov, node, dataSizeReg, operand2, iCursor);
            }
         else
            {
            // Round the data area to a multiple of 4
            iCursor = generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, dataSizeReg, enumReg, 3, 0, iCursor);
            if (elementSize == 2)
               {
               iCursor = generateTrg1Src2Instruction(cg, ARMOp_add, node, dataSizeReg, dataSizeReg, enumReg, iCursor);
               }
            iCursor = generateTrg1Src1ImmInstruction(cg, ARMOp_bic, node, dataSizeReg, dataSizeReg, 3, 0, iCursor);
            }
         if ((round != 0 && round != 4) || !headerAligned)
            {
            iCursor = generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, sizeReg, dataSizeReg, allocSize+round-1, 0, iCursor);
            iCursor = generateTrg1Src1ImmInstruction(cg, ARMOp_bic, node, sizeReg, sizeReg, round-1, 0, iCursor);
            }
         else
            iCursor = generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, sizeReg, dataSizeReg, allocSize, 0, iCursor);
         }
      else
         iCursor = armLoadConstant(node, allocSize, sizeReg, cg, iCursor);
      }

   // Borrowing heapTopReg
   tempMR = new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapTop), cg);
   iCursor = generateTrg1MemInstruction(cg, ARMOp_ldr, node, heapTopReg, tempMR, iCursor);

   // Calculate the after-allocation heapAlloc: if the size is huge,
   //   we need to check address wrap-around also. This is unsigned
   //   integer arithmetic, checking carry bit is enough to detect it.
   //   For variable length array, we did an up-front check already.

   if (allocSize > cg->getMaxObjectSizeGuaranteedNotToOverflow())
      {
      if (sizeInReg)
         {
         iCursor = generateTrg1Src2Instruction(cg, ARMOp_add_r, node, sizeReg, resReg, sizeReg, iCursor);
         }
      else
         {
         iCursor = generateTrg1Src1ImmInstruction(cg, ARMOp_add_r, node, sizeReg, resReg, base, rotate, iCursor);
         }
      iCursor = generateConditionalBranchInstruction(cg, node, ARMConditionCodeCS, callLabel, iCursor);
      }
   else
      {
      if (sizeInReg)
         {
         iCursor = generateTrg1Src2Instruction(cg, ARMOp_add, node, sizeReg, resReg, sizeReg, iCursor);
         }
      else
         {
         iCursor = generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, sizeReg, resReg, base, rotate, iCursor);
         }
      }

   iCursor = generateSrc2Instruction(cg, ARMOp_cmp, node, sizeReg, heapTopReg, iCursor);
   iCursor = generateConditionalBranchInstruction(cg, node, ARMConditionCodeHI, callLabel, iCursor);
   tempMR = new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapAlloc), cg);
   iCursor = generateMemSrc1Instruction(cg, ARMOp_str, node, tempMR, sizeReg, iCursor);
   iCursor = generateTrg1ImmInstruction(cg, ARMOp_mov, node, zeroReg, 0, 0, iCursor);
   }


static void genInitObjectHeader(TR::CodeGenerator  *cg,
                                TR::Node           *node,
                                TR::Instruction *  &iCursor,
                                J9Class           *clazz,
                                TR::Register       *classReg,
                                TR::Register       *resReg,
                                TR::Register       *zeroReg,
                                TR::Register       *temp1Reg,
                                TR::Register       *temp2Reg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   uint32_t               staticFlag = clazz->romClass->instanceShape;
   TR::Register           *metaReg    = cg->getMethodMetaDataRegister();
   TR::MemoryReference *tempMR;

   // Store the class
   if (classReg == NULL)
      {
      iCursor = armLoadConstant(node, (int32_t)clazz, temp1Reg, cg, iCursor);

      tempMR = new (cg->trHeapMemory()) TR::MemoryReference(resReg, (int32_t)TR::Compiler->om.offsetOfObjectVftField(), cg);
      iCursor = generateMemSrc1Instruction(cg, ARMOp_str, node, tempMR, temp1Reg, iCursor);
      }
   else
      {
      tempMR = new (cg->trHeapMemory()) TR::MemoryReference(resReg, (int32_t)TR::Compiler->om.offsetOfObjectVftField(), cg);
      iCursor = generateMemSrc1Instruction(cg, ARMOp_str, node, tempMR, classReg, iCursor);
      }

   // If the object flags cannot be determined at compile time, we have to add a load
   // for it. And then, OR it with temp1Reg.
   // todo: if MODRON_GC is used, staticFlag could be obtained dynamically, and using isStaticObjectFlags and
   // getStaticObjectFlags interfaces

#ifndef J9VM_INTERP_FLAGS_IN_CLASS_SLOT
   bool isStaticFlag = fej9->isStaticObjectFlags();

#if  defined(J9VM_OPT_NEW_OBJECT_HASH)
 // TODO: need to handle the !isStaticFlag case as mentioned above.
 // TODO: only one temp register is needed. Fix later
  if (isStaticFlag != 0)
      {
      staticFlag |= fej9->getStaticObjectFlags();
      iCursor = armLoadConstant(node, staticFlag, temp1Reg, cg, iCursor);
      }
#else

   // Calculate:  (resReg << 14) & 0x7FFF0000
   //   I can only think of 4 instructions sequence which takes advantage of the fact
   //   that resReg itself is at least dword-aligned. Need to revisit whether there is
   //   better sequence.
   iCursor = generateTrg1ImmInstruction(cg, ARMOp_mov, node, temp1Reg, 2, 16, iCursor);
   iCursor = generateTrg1Src1ImmInstruction(cg, ARMOp_sub, node, temp1Reg, temp1Reg, 1, 0, iCursor);
   iCursor = generateTrg1Src2Instruction(cg, ARMOp_and, node, temp1Reg, resReg, temp1Reg, iCursor);
   iCursor = generateTrg1Src1Instruction(cg, ARMOp_mov, node, temp1Reg,
                new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegLSLImmed, temp1Reg, 14), iCursor);

   if (isStaticFlag != 0)
      {
      staticFlag |= fej9->getStaticObjectFlags();
      uint32_t  val1, val2;
      if (constantIsImmed8r(staticFlag, &val1, &val2))
         {
         iCursor = generateTrg1Src1ImmInstruction(cg, ARMOp_orr, node, temp1Reg, temp1Reg, val1, val2, iCursor);
         }
      else
         {
         iCursor = armLoadConstant(node, staticFlag, temp2Reg, cg, iCursor);
         iCursor = generateTrg1Src2Instruction(cg, ARMOp_orr, node, temp1Reg, temp1Reg, temp2Reg, iCursor);
         }
      }
#endif /* J9VM_OPT_NEW_OBJECT_HASH */

   // Store the flags
   tempMR = new (cg->trHeapMemory()) TR::MemoryReference(resReg, (int32_t)TR::Compiler->om.offsetOfHeaderFlags(), cg);
   iCursor = generateMemSrc1Instruction(cg, ARMOp_str, node, tempMR, temp1Reg, iCursor);
#endif /* J9VM_INTERP_FLAGS_IN_CLASS_SLOT */

   // Initialize monitor if required
   int32_t monitorOffset;
   bool initMonitor = false;
#if !defined(J9VM_THR_LOCK_NURSERY)
   monitorOffset = (int32_t)TMP_OFFSETOF_J9OBJECT_MONITOR;
   initMonitor = true;
#elif defined(J9VM_THR_LOCK_NURSERY_FAT_ARRAYS)
   monitorOffset = (int32_t)TMP_OFFSETOF_J9INDEXABLEOBJECT_MONITOR;
   // Initialize the monitor
   // word for arrays that have a
   // lock word
   int32_t lwOffset = fej9->getByteOffsetToLockword((TR_OpaqueClassBlock *)clazz);
   if ((node->getOpCodeValue() != TR::New) &&
         (lwOffset > 0))
      initMonitor = true;
#endif
   if (initMonitor)
      {
      tempMR = new (cg->trHeapMemory()) TR::MemoryReference(resReg, monitorOffset, cg);
      iCursor = generateMemSrc1Instruction(cg, ARMOp_str, node, tempMR, zeroReg, iCursor);
      }

   }


static void genAlignDoubleArray(TR::CodeGenerator  *cg,
                                TR::Node           *node,
                                TR::Instruction   *&iCursor,
                                bool               isVariableLen,
                                TR::Register       *resReg,
                                int32_t            objectSize,
                                int32_t            dataBegin,
                                TR::Register       *dataSizeReg,
                                TR::Register       *temp1Reg,
                                TR::Register       *temp2Reg)
   {
   TR::LabelSymbol *slotAtStart = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *doneAlign = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   iCursor = generateTrg1Src1ImmInstruction(cg, ARMOp_and_r, node, temp1Reg, resReg, 7, 0, iCursor);
   iCursor = generateTrg1ImmInstruction(cg, ARMOp_mov, node, temp2Reg, 3, 0, iCursor);
   iCursor = generateConditionalBranchInstruction(cg, node, ARMConditionCodeNE, slotAtStart, iCursor);

   // Should be able to have branch-less sequence here ... but we need to change armLoadConstant for that.
   // TODO: consider this for the future.

   // The slop bytes are at the end of the allocated object.
   TR::MemoryReference *tempMR;
   if (isVariableLen)
      {
      tempMR = new (cg->trHeapMemory()) TR::MemoryReference(temp1Reg, dataBegin, cg);
      iCursor = generateTrg1Src2Instruction(cg, ARMOp_add, node, temp1Reg, resReg, dataSizeReg, iCursor);
      iCursor = generateMemSrc1Instruction(cg, ARMOp_str, node, tempMR, temp2Reg, iCursor);
      }
   else if (objectSize > UPPER_IMMED12)
      {
      tempMR = new (cg->trHeapMemory()) TR::MemoryReference(resReg, temp1Reg, cg);
      iCursor = armLoadConstant(node, objectSize, temp1Reg, cg, iCursor);
      iCursor = generateMemSrc1Instruction(cg, ARMOp_str, node, tempMR, temp2Reg, iCursor);
      }
   else
      {
      tempMR = new (cg->trHeapMemory()) TR::MemoryReference(resReg, objectSize, cg);
      iCursor = generateMemSrc1Instruction(cg, ARMOp_str, node, tempMR, temp2Reg, iCursor);
      }
   iCursor = generateLabelInstruction(cg, ARMOp_b, node, doneAlign, iCursor);

   // the slop bytes are at the start of the allocation
   tempMR = new (cg->trHeapMemory()) TR::MemoryReference(resReg, (int32_t)0, cg);
   iCursor = generateLabelInstruction(cg, ARMOp_label, node, slotAtStart, iCursor);
   iCursor = generateMemSrc1Instruction(cg, ARMOp_str, node, tempMR, temp2Reg, iCursor);
   iCursor = generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, resReg, resReg, 4, 0, iCursor);
   iCursor = generateLabelInstruction(cg, ARMOp_label, node, doneAlign, iCursor);
   }


static void genInitArrayHeader(TR::CodeGenerator  *cg,
                               TR::Node           *node,
                               TR::Instruction   *&iCursor,
                               bool               isVariableLen,
                               J9Class           *clazz,
                               TR::Register       *resReg,
                               TR::Register       *zeroReg,
                               TR::Register       *eNumReg,
                               TR::Register       *dataSizeReg,
                               TR::Register       *temp1Reg,
                               TR::Register       *temp2Reg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   int32_t      elementSize = TR::Compiler->om.getSizeOfArrayElement(node);
   TR::Register *instanceSizeReg;

   genInitObjectHeader(cg, node, iCursor, clazz, NULL, resReg, zeroReg, temp1Reg, temp2Reg);

   instanceSizeReg = eNumReg;

   // Store the array size
   TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(resReg, (int32_t)(fej9->getOffsetOfContiguousArraySizeField()), cg);
   iCursor = generateMemSrc1Instruction(cg, ARMOp_str, node, tempMR, instanceSizeReg, iCursor);
   }


TR::Register *OMR::ARM::TreeEvaluator::VMnewEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   int32_t                    dataBegin, idx;
   J9Class                   *clazz;
   TR::Register               *callResult;
   TR::LabelSymbol             *callLabel, *doneLabel;
   TR::RegisterDependencyConditions *deps;
   TR::Instruction            *iCursor = NULL;
   bool                       isArray = false, isDoubleArray = false;
   TR::Compilation *comp = TR::comp();

   // AOT does not support inline allocates - cannot currently guarantee totalInstanceSize

   TR::ILOpCodes opCode        = node->getOpCodeValue();
   int32_t      objectSize    = cg->comp()->canAllocateInlineOnStack(node, (TR_OpaqueClassBlock *&) clazz);
   bool         isVariableLen = (objectSize == 0);
   if (objectSize < 0 || comp->compileRelocatableCode() || comp->suppressAllocationInlining())
      {
      TR::Node::recreate(node, TR::acall);
      callResult = directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);
      return callResult;
      }

   TR::Register *enumReg;
   TR::Register *classReg;
   int32_t      allocateSize = objectSize;
   callLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   doneLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(10, 10, cg->trMemory());

   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = NULL;

   if (opCode == TR::New)
      {
      classReg = cg->evaluate(firstChild);
      if (firstChild->getReferenceCount() > 1)
         {
         TR::Register *copyReg = cg->allocateRegister();
         iCursor = generateTrg1Src1Instruction(cg, ARMOp_mov, node, copyReg, classReg, iCursor);
         classReg = copyReg;
         }
      dataBegin = sizeof(J9Object);
      }
   else
      {
      isArray = true;
      dataBegin = sizeof(J9IndexableObjectContiguous);
      secondChild = node->getSecondChild();
#ifndef J9VM_GC_ALIGN_OBJECTS
      if (opCode == TR::newarray)
         {
         if (secondChild->getInt() == 7 || secondChild->getInt() == 11)
            {
            // To ensure 8-byte alignment on the array, we need 4-byte slop.
            allocateSize += 4;
            isDoubleArray = true;
            }
         }
      else
         {
         TR_ASSERT(opCode == TR::anewarray, "Bad opCode for VMnewEvaluator");
         }
#endif
      // Potential helper call requires us to evaluate the arguments always.
      // For TR::newarray, classReg refers to the primitive type, not a class.
      enumReg  = cg->evaluate(firstChild);
      classReg = cg->evaluate(secondChild);
      if (secondChild->getReferenceCount() > 1)
         {
         TR::Register *copyReg = cg->allocateRegister();
         iCursor = generateTrg1Src1Instruction(cg, ARMOp_mov, node, copyReg, classReg, iCursor);
         classReg = copyReg;
         }
      }

   TR::addDependency(deps, classReg, TR::RealRegister::NoReg, TR_GPR, cg);
   if (secondChild == NULL)
      enumReg = cg->allocateRegister();
   TR::addDependency(deps, enumReg, TR::RealRegister::NoReg, TR_GPR, cg);

   // We have two paths through this code, each of which calls a different helper:
   // one calls jitNewObject, the other performs a write barrier (when multimidlets are enabled).
   // Each helper has different register allocation requirements so we'll set the deps here for
   // the write barrier (which should be the common case), and fix up the registers in the call
   // to jitNewObject.

   TR::Register *resReg      = cg->allocateRegister();
   TR::Register *zeroReg     = cg->allocateRegister();
   TR::Register *dataSizeReg = cg->allocateRegister();
   TR::Register *temp1Reg    = cg->allocateRegister();
   TR::Register *temp2Reg    = cg->allocateRegister();
   TR::addDependency(deps, resReg, TR::RealRegister::gr0, TR_GPR, cg);
   TR::addDependency(deps, zeroReg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(deps, dataSizeReg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(deps, temp1Reg, TR::RealRegister::gr2, TR_GPR, cg);
   TR::addDependency(deps, temp2Reg, TR::RealRegister::gr1, TR_GPR, cg);


   if (isVariableLen)
      allocateSize += dataBegin;
   else
      allocateSize = (allocateSize + OBJECT_ALIGNMENT - 1) & (-OBJECT_ALIGNMENT);

   // classReg and enumReg have to be intact still, in case we have to call the helper.
   // On return, zeroReg is set to 0, and dataSizeReg is set to the size of data area if
   // isVariableLen is true.
   genHeapAlloc(cg, node, iCursor, isVariableLen, enumReg, resReg, zeroReg, dataSizeReg, temp1Reg, temp2Reg, callLabel, allocateSize);

   if (isArray)
      {
      // Align the array if necessary.
      if (isDoubleArray)
         genAlignDoubleArray(cg, node, iCursor, isVariableLen, resReg, objectSize, dataBegin, dataSizeReg, temp1Reg, temp2Reg);
      genInitArrayHeader(cg, node, iCursor, isVariableLen, clazz, resReg, zeroReg, enumReg, dataSizeReg, temp1Reg, temp2Reg);
      }
   else
      {
      genInitObjectHeader(cg, node, iCursor, clazz, classReg, resReg, zeroReg, temp1Reg, temp2Reg);
      }

   // Perform initialization if it is needed:
   //   1) Initialize certain array elements individually. This depends on the optimizer
   //      providing a "short" list of individual indices;
   //   2) Initialize the whole array:
   //      a) If the object size is constant, we can choose strategy depending on the
   //         size of the array. Using straight line of code, or unrolled loop;
   //      b) For variable length of array, do a counted loop;

   TR_ExtraInfoForNew *initInfo = node->getSymbolReference()->getExtraInfo();
   TR::MemoryReference *tempMR;

   if (!node->canSkipZeroInitialization() && (initInfo == NULL || initInfo->numZeroInitSlots > 0))
      {
      if (!isVariableLen)
         {
         if (initInfo!=NULL && initInfo->zeroInitSlots!=NULL &&
             initInfo->numZeroInitSlots<=9 && objectSize<=UPPER_IMMED12)
            {
            TR_BitVectorIterator bvi(*initInfo->zeroInitSlots);

            while (bvi.hasMoreElements())
               {
               tempMR  = new (cg->trHeapMemory()) TR::MemoryReference(resReg, bvi.getNextElement() * 4 + dataBegin, cg);
               iCursor = generateMemSrc1Instruction(cg, ARMOp_str, node, tempMR, zeroReg, iCursor);
               }
            }
         else if (objectSize > 48)
            {
            // maybe worth of exploring the possibility of using dcbz, but the alignment
            // would require a little more cost of setup.

            int32_t loopCnt = (objectSize - dataBegin) >> 4;
            int32_t residue = ((objectSize - dataBegin) >> 2) & 0x03;
            TR::LabelSymbol *initLoop = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

            iCursor = armLoadConstant(node, loopCnt, temp1Reg, cg, iCursor);
            iCursor = generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, temp2Reg, resReg, dataBegin, 0, iCursor);
            iCursor = generateLabelInstruction(cg, ARMOp_label, node, initLoop, iCursor);
            tempMR  = new (cg->trHeapMemory()) TR::MemoryReference(temp2Reg, 0, cg);
            iCursor = generateMemSrc1Instruction(cg, ARMOp_str, node, tempMR, zeroReg, iCursor);
            tempMR  = new (cg->trHeapMemory()) TR::MemoryReference(temp2Reg, 4, cg);
            iCursor = generateMemSrc1Instruction(cg, ARMOp_str, node, tempMR, zeroReg, iCursor);
            tempMR  = new (cg->trHeapMemory()) TR::MemoryReference(temp2Reg, 8, cg);
            iCursor = generateMemSrc1Instruction(cg, ARMOp_str, node, tempMR, zeroReg, iCursor);
            tempMR  = new (cg->trHeapMemory()) TR::MemoryReference(temp2Reg, 12, cg);
            iCursor = generateMemSrc1Instruction(cg, ARMOp_str, node, tempMR, zeroReg, iCursor);

            // TODO: add the update form instruction, we will not need the following instruction
            iCursor = generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, temp2Reg, temp2Reg, 16, 0, iCursor);

            iCursor = generateTrg1Src1ImmInstruction(cg, ARMOp_sub_r, node, temp1Reg, temp1Reg, 1, 0, iCursor);
            iCursor = generateConditionalBranchInstruction(cg, node, ARMConditionCodeNE, initLoop, iCursor);
            for (idx = 0; idx < residue; idx++)
               {
               tempMR  = new (cg->trHeapMemory()) TR::MemoryReference(temp2Reg, idx << 2, cg);
               iCursor = generateMemSrc1Instruction(cg, ARMOp_str, node, tempMR, zeroReg, iCursor);
               }
            }
         else
            {
            for (idx = dataBegin; idx < objectSize; idx += 4)
               {
               tempMR  = new (cg->trHeapMemory()) TR::MemoryReference(resReg, idx, cg);
               iCursor = generateMemSrc1Instruction(cg, ARMOp_str, node, tempMR, zeroReg, iCursor);
               }
            }
         }
      else   // Init variable length array
         {
         // Test for zero length array: also set up loop count

         iCursor = generateTrg1Src1Instruction(cg, ARMOp_mov_r, node, temp1Reg, new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegLSRImmed, dataSizeReg, 2), iCursor);
         iCursor = generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, doneLabel, iCursor);
         iCursor = generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, temp2Reg, resReg, dataBegin - 4, 0, iCursor);

         TR::LabelSymbol *initLoop = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         iCursor = generateLabelInstruction(cg, ARMOp_label, node, initLoop, iCursor);
         tempMR  = new (cg->trHeapMemory()) TR::MemoryReference(temp2Reg, temp1Reg, 4, cg);
         iCursor = generateMemSrc1Instruction(cg, ARMOp_str, node, tempMR, zeroReg, iCursor);
         iCursor = generateTrg1Src1ImmInstruction(cg, ARMOp_sub_r, node, temp1Reg, temp1Reg, 1, 0, iCursor);
         iCursor = generateConditionalBranchInstruction(cg, node, ARMConditionCodeNE, initLoop, iCursor);
         }
      }
   generateConditionalBranchInstruction(cg, node, ARMConditionCodeAL, doneLabel);

   // TODO:  using snippet to better use of branch prediction facility
   generateLabelInstruction(cg, ARMOp_label, node, callLabel);
   generateTrg1Src1Instruction(cg, ARMOp_mov, node, resReg, classReg);
   if (secondChild != NULL)
      generateTrg1Src1Instruction(cg, ARMOp_mov, node, temp2Reg, enumReg);
   iCursor = generateImmSymInstruction(cg, ARMOp_bl, node, (uint32_t)node->getSymbolReference()->getMethodAddress(), deps, node->getSymbolReference());
   iCursor->ARMNeedsGCMap(0xFFFFFFFE);  // mask off gr0
   cg->machine()->setLinkRegisterKilled(true);
   generateLabelInstruction(cg, ARMOp_label, node, doneLabel, deps);
   deps->stopAddingConditions();

   cg->decReferenceCount(firstChild);
   if (secondChild != NULL)
      cg->decReferenceCount(secondChild);

   node->setRegister(resReg);
   resReg->setContainsCollectedReference();
   return resReg;
   }

TR::Register *
OMR::ARM::TreeEvaluator::VMmonentEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = TR::comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   int32_t lwOffset = fej9->getByteOffsetToLockword(cg->getMonClass(node));

   if (comp->getOption(TR_OptimizeForSpace) ||
       (comp->getOption(TR_FullSpeedDebug) /*&& !comp->getOption(TR_EnableLiveMonitorMetadata)*/) ||
       comp->getOption(TR_DisableInlineMonEnt) ||
       lwOffset <= 0)
      {
      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node::recreate(node, TR::call);
      TR::Register *targetRegister = directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);
      return targetRegister;
      }

   TR::Node *objNode = node->getFirstChild();
   TR::Register *objReg = cg->evaluate(objNode);
   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(4, 4, cg->trMemory());
   TR::Register *metaReg, *dataReg, *addrReg;
   TR::LabelSymbol   *callLabel;
   TR::Instruction *iCursor;

   metaReg = cg->getMethodMetaDataRegister();
   dataReg = cg->allocateRegister();
   addrReg = cg->allocateRegister();
   TR::addDependency(deps, objReg, TR::RealRegister::gr0, TR_GPR, cg);
   TR::addDependency(deps, dataReg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(deps, addrReg, TR::RealRegister::NoReg, TR_GPR, cg);

#ifdef J9VM_TASUKI_LOCKS_SINGLE_SLOT
   TR::Register *tempReg = cg->allocateRegister();
   TR::addDependency(deps, tempReg, TR::RealRegister::NoReg, TR_GPR, cg);
#endif
   callLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

#ifdef J9VM_TASUKI_LOCKS_SINGLE_SLOT // __ARM_ARCH_6__ and up
   // This is spinning on the object itself and not on the global objectFlagSpinLock
   TR::LabelSymbol *incLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *loopLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *doneLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, addrReg, objReg, lwOffset, 0);
   generateLabelInstruction(cg, ARMOp_label, node, loopLabel);
   generateTrg1MemInstruction(cg, ARMOp_ldrex, node, dataReg, new (cg->trHeapMemory()) TR::MemoryReference(addrReg, (int32_t)0, cg));
   generateSrc1ImmInstruction(cg, ARMOp_cmp, node, dataReg, 0, 0);
   TR::Instruction *cursor = generateConditionalBranchInstruction(cg, node, ARMConditionCodeNE, incLabel);

   generateTrg1MemSrc1Instruction(cg, ARMOp_strex, node, tempReg, new (cg->trHeapMemory()) TR::MemoryReference(addrReg, (int32_t)0, cg), metaReg);
   generateSrc1ImmInstruction(cg, ARMOp_cmp, node, tempReg, 0, 0);
   generateConditionalBranchInstruction(cg, node, ARMConditionCodeNE, loopLabel);

   if (TR::Compiler->target.isSMP() && TR::Compiler->target.cpu.id() != TR_DefaultARMProcessor)
      {
      generateInstruction(cg, (TR::Compiler->target.cpu.id() == TR_ARMv6) ? ARMOp_dmb_v6 : ARMOp_dmb, node);
      }

   generateLabelInstruction(cg, ARMOp_label, node, doneLabel, deps);

   TR::Snippet *snippet = new (cg->trHeapMemory()) TR::ARMMonitorEnterSnippet(cg, node, lwOffset, incLabel, callLabel, doneLabel);
   cg->addSnippet(snippet);
   doneLabel->setEndInternalControlFlow();
#else
   generateTrg1MemInstruction(cg, ARMOp_ldr, node, addrReg, new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, objectFlagSpinLockAddress), cg));

   generateTrg1Src2Instruction(cg, ARMOp_swp, node, dataReg, addrReg, addrReg);
   generateSrc1ImmInstruction(cg, ARMOp_cmp, node, dataReg, 0, 0);
   generateConditionalBranchInstruction(cg, node, ARMConditionCodeNE, callLabel);
   generateTrg1MemInstruction(cg, ARMOp_ldr, node, dataReg, new (cg->trHeapMemory()) TR::MemoryReference(objReg, lwOffset, cg));
   generateSrc1ImmInstruction(cg, ARMOp_cmp, node, dataReg, 0, 0);

   iCursor = generateMemSrc1Instruction(cg, ARMOp_str, node, new (cg->trHeapMemory()) TR::MemoryReference(objReg, lwOffset, cg), metaReg);
   iCursor->setConditionCode(ARMConditionCodeEQ);

   iCursor = generateTrg1ImmInstruction(cg, ARMOp_mov, node, dataReg, 0, 0);
   iCursor->setConditionCode(ARMConditionCodeNE);

   generateMemSrc1Instruction(cg, ARMOp_str, node, new (cg->trHeapMemory()) TR::MemoryReference(addrReg, (int32_t)0, cg), dataReg);
   generateLabelInstruction(cg, ARMOp_label, node, callLabel);

   iCursor = generateImmSymInstruction(cg, ARMOp_bl, node, (uint32_t)node->getSymbolReference()->getMethodAddress(), deps, node->getSymbolReference());
   iCursor->setConditionCode(ARMConditionCodeNE);
   iCursor->ARMNeedsGCMap(0xFFFFFFFF);
   cg->machine()->setLinkRegisterKilled(true);
#endif
   deps->stopAddingConditions();

   cg->decReferenceCount(objNode);
   return NULL;
   }

/*
TR::Register *OMR::Power::TreeEvaluator::VMarrayCheckEvaluator(TR::Node *node)
   {
   TR::Register    *obj1Reg, *obj2Reg, *tmp1Reg, *tmp2Reg, *cndReg;
   TR::LabelSymbol *doneLabel, *snippetLabel;
   TR_Instruction *gcPoint;
   TR::Snippet     *snippet;
   TR::RegisterDependencyConditions *conditions;
   int32_t         depIndex;

   obj1Reg = ppcCodeGen->evaluate(node->getFirstChild());
   obj2Reg = ppcCodeGen->evaluate(node->getSecondChild());
   doneLabel = TR::LabelSymbol::create(cg->trHeapMemory(),ppcCodeGen);
   conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(5, 5, cg->trMemory());
   depIndex = 0;
   nonFixedDependency(conditions, obj1Reg, &depIndex, TR_GPR, true);
   nonFixedDependency(conditions, obj2Reg, &depIndex, TR_GPR, true);
   tmp1Reg = nonFixedDependency(conditions, NULL, &depIndex, TR_GPR, false);
   tmp2Reg = nonFixedDependency(conditions, NULL, &depIndex, TR_GPR, false);
   cndReg = ppcCodeGen->allocateRegister(TR_CCR);
   TR::addDependency(conditions, cndReg, TR::RealRegister::cr0, TR_CCR, cg);

   // We have a unique snippet sharing arrangement in this code sequence.
   // It is not generally applicable for other situations.
   snippetLabel = NULL;

   // Same array, we are done.
   //
   generateTrg1Src2Instruction(TR::InstOpCode::cmpl4, node, cndReg, obj1Reg, obj2Reg);
   generateConditionalBranchInstruction(TR::InstOpCode::beq, node, doneLabel, cndReg);

   // If we know nothing about either object, test object1 first. It has to be an array.
   //
   if (!node->isArrayChkPrimitiveArray1() &&
       !node->isArrayChkReferenceArray1() &&
       !node->isArrayChkPrimitiveArray2() &&
       !node->isArrayChkReferenceArray2())
      {
      generateTrg1MemInstruction(TR::InstOpCode::lwz, node, tmp1Reg,
         new (cg->trHeapMemory()) TR::MemoryReference(obj1Reg, (int32_t)TR::Compiler->om.offsetOfHeaderFlags(), 4));
      generateTrg1Src1ImmInstruction(TR::InstOpCode::andi_r, node, tmp1Reg, tmp1Reg, OBJECT_HEADER_INDEXABLE);
      snippetLabel = TR::LabelSymbol::create(cg->trHeapMemory(),ppcCodeGen);
      gcPoint = generateConditionalBranchInstruction(TR::InstOpCode::beql, node, snippetLabel, cndReg);
      snippet = new (cg->trHeapMemory()) TR::PPCHelperCallSnippet(snippetLabel, node->getSymbolReference());
      ppcCodeGen->addSnippet(snippet);
      }

   // One of the object is array. Test equality of two objects' classes.
   //
   generateTrg1MemInstruction(TR::InstOpCode::lwz, node, tmp2Reg,
      new (cg->trHeapMemory()) TR::MemoryReference(obj2Reg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), 4));
   generateTrg1MemInstruction(TR::InstOpCode::lwz, node, tmp1Reg,
      new (cg->trHeapMemory()) TR::MemoryReference(obj1Reg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), 4));
   generateTrg1Src2Instruction(TR::InstOpCode::cmpl4, node, cndReg, tmp1Reg, tmp2Reg);

   // If either object is known to be of primitive component type,
   // we are done: since both of them have to be of equal class.
   if (node->isArrayChkPrimitiveArray1() || node->isArrayChkPrimitiveArray2())
      {
      if (snippetLabel == NULL)
         {
         snippetLabel = TR::LabelSymbol::create(cg->trHeapMemory(),ppcCodeGen);
         gcPoint = generateConditionalBranchInstruction(TR::InstOpCode::bnel, node, snippetLabel, cndReg);
         snippet = new (cg->trHeapMemory()) TR::PPCHelperCallSnippet(snippetLabel, node->getSymbolReference());
         ppcCodeGen->addSnippet(snippet);
         }
      else
         generateConditionalBranchInstruction(TR::InstOpCode::bne, node, snippetLabel, cndReg);
      }
   // We have to take care of the un-equal class situation: both of them must be of reference array
   else
      {
      generateConditionalBranchInstruction(TR::InstOpCode::beq, node, doneLabel, cndReg);

      // Object1 must be of reference component type, otherwise throw exception
      if (!node->isArrayChkReferenceArray1())
         {
         generateTrg1MemInstruction(TR::InstOpCode::lwz, node, tmp1Reg,
            new (cg->trHeapMemory()) TR::MemoryReference(obj1Reg, (int32_t)TR::Compiler->om.offsetOfHeaderFlags(), 4));
         generateTrg1Src1ImmInstruction(TR::InstOpCode::andi_r, node, tmp1Reg, tmp1Reg, OBJECT_HEADER_SHAPE_MASK);
         if (snippetLabel == NULL)
            {
            snippetLabel = TR::LabelSymbol::create(cg->trHeapMemory(),ppcCodeGen);
            gcPoint = generateConditionalBranchInstruction(TR::InstOpCode::bnel, node, snippetLabel, cndReg);
            snippet = new (cg->trHeapMemory()) TR::PPCHelperCallSnippet(snippetLabel, node->getSymbolReference());
            ppcCodeGen->addSnippet(snippet);
            }
         else
            generateConditionalBranchInstruction(TR::InstOpCode::bne, node, snippetLabel, cndReg);
         }

      // Object2 must be of reference component type array, otherwise throw exception
      if (!node->isArrayChkReferenceArray2())
         {
         generateTrg1MemInstruction(TR::InstOpCode::lwz, node, tmp2Reg,
            new (cg->trHeapMemory()) TR::MemoryReference(obj2Reg, (int32_t)TR::Compiler->om.offsetOfHeaderFlags(), 4));
         generateTrg1Src1ImmInstruction(TR::InstOpCode::andi_r, node, tmp1Reg, tmp2Reg, OBJECT_HEADER_INDEXABLE);
         if (snippetLabel == NULL)
            {
            snippetLabel = TR::LabelSymbol::create(cg->trHeapMemory(),ppcCodeGen);
            gcPoint = generateConditionalBranchInstruction(TR::InstOpCode::beql, node, snippetLabel, cndReg);
            snippet = new (cg->trHeapMemory()) TR::PPCHelperCallSnippet(snippetLabel, node->getSymbolReference());
            ppcCodeGen->addSnippet(snippet);
            }
         else
            generateConditionalBranchInstruction(TR::InstOpCode::beq, node, snippetLabel, cndReg);

         generateTrg1Src1ImmInstruction(TR::InstOpCode::andi_r, node, tmp1Reg, tmp2Reg, OBJECT_HEADER_SHAPE_MASK);
         generateConditionalBranchInstruction(TR::InstOpCode::bne, node, snippetLabel, cndReg);
         }
      }

   generateDepLabelInstruction(TR::InstOpCode::label, node, doneLabel, conditions);

   conditions->stopUsingDepRegs(obj1Reg, obj2Reg);

   ppcCodeGen->decReferenceCount(node->getFirstChild());
   ppcCodeGen->decReferenceCount(node->getSecondChild());
   return NULL;
   }


void OMR::Power::TreeEvaluator::VMarrayStoreCHKEvaluator(TR::Node *node, TR::Register *src, TR::Register *dst, TR::Register *t1Reg, TR::Register *t2Reg, TR::Register *cndReg, TR::LabelSymbol *toWB, TR_Instruction *iPtr)
   {
   int32_t objectClass = (int32_t)getSystemClassFromClassName("java/lang/Object", 16);
   int32_t upper = ((objectClass>>16) + ((objectClass & 0x00008000)>>15)) & 0x0000FFFF;
   int32_t lower = objectClass & 0x0000FFFF;

   iPtr = generateTrg1MemInstruction(TR::InstOpCode::lwz, node, t1Reg,
             new (cg->trHeapMemory()) TR::MemoryReference(dst, (int32_t)TR::Compiler->om.offsetOfObjectVftField(), 4), iPtr);
   iPtr = generateTrg1MemInstruction(TR::InstOpCode::lwz, node, t2Reg,
             new (cg->trHeapMemory()) TR::MemoryReference(src, (int32_t)TR::Compiler->om.offsetOfObjectVftField(), 4), iPtr);
   iPtr = generateTrg1MemInstruction(TR::InstOpCode::lwz, node, t1Reg,
             new (cg->trHeapMemory()) TR::MemoryReference(t1Reg, (int32_t)offsetof(J9ArrayClass, componentType), 4),
             iPtr);
   iPtr = generateTrg1Src2Instruction(TR::InstOpCode::cmpl4, node, cndReg, t1Reg, t2Reg, iPtr);
   iPtr = generateConditionalBranchInstruction(TR::InstOpCode::beq, node, toWB, cndReg, iPtr);
   if (upper == 0)
      {
      iPtr = generateTrg1ImmInstruction(TR::InstOpCode::li, node, t2Reg, lower, iPtr);
      }
   else
      {
      iPtr = generateTrg1ImmInstruction(TR::InstOpCode::lis, node, t2Reg, upper, iPtr);
      if (lower != 0)
         {
         iPtr = generateTrg1Src1ImmInstruction(TR::InstOpCode::addi, node, t2Reg, t2Reg, lower, iPtr);
         }
      }
   iPtr = generateTrg1Src2Instruction(TR::InstOpCode::cmpl4, node, cndReg, t1Reg, t2Reg, iPtr);
   iPtr = generateConditionalBranchInstruction(TR::InstOpCode::beq, node, toWB, cndReg, iPtr);
   }
*/


void OMR::ARM::TreeEvaluator::VMwrtbarEvaluator(TR::Node *node, TR::Register *srcReg, TR::Register *dstReg, TR::Register *flagReg, bool needDeps, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = TR::comp();
   TR::LabelSymbol      *doneLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::SymbolReference *wbref = comp->getSymRefTab()->findOrCreateWriteBarrierStoreSymbolRef(comp->getMethodSymbol());
   auto gcMode = TR::Compiler->om.writeBarrierType();

   TR::Register *tempReg = NULL;
   TR::RegisterDependencyConditions *deps = NULL;

   if (flagReg == NULL)
      tempReg = cg->allocateRegister();
   else
      tempReg = flagReg;

   if (needDeps)
      {
      deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(3, 3, cg->trMemory());
      TR::addDependency(deps, srcReg,  TR::RealRegister::gr1, TR_GPR, cg);
      TR::addDependency(deps, dstReg,  TR::RealRegister::gr0, TR_GPR, cg);
      TR::addDependency(deps, tempReg, TR::RealRegister::gr2, TR_GPR, cg);
      }

   // todo check for _isSourceNonNull in PPC gen to see possible opt?
   // nullcheck store - skip write barrier if null being written in
   generateSrc2Instruction(cg, ARMOp_tst, node, srcReg, srcReg);
   generateConditionalBranchInstruction(cg, node, ARMConditionCodeEQ, doneLabel);

   if (gcMode != gc_modron_wrtbar_always)
      {
      // check to see if colour bits give hint (read FLAGS field of J9Object).
// @@ getWordOffsetToGCFlags() and getWriteBarrierGCFlagMaskAsByte() are missing in TR_FrontEnd
// @@      if (flagReg == NULL)
// @@         generateTrg1MemInstruction(cg, ARMOp_ldr, node, tempReg, new (cg->trHeapMemory()) TR::MemoryReference(dstReg, fej9->getWordOffsetToGCFlags(), cg));
// @@      generateTrg1Src2Instruction(cg, ARMOp_and_r, node, tempReg, tempReg, new (cg->trHeapMemory()) TR_ARMOperand2(fej9->getWriteBarrierGCFlagMaskAsByte(), 8));
      }

   TR::Instruction *cursor = generateImmSymInstruction(cg, ARMOp_bl,
                                        node, (uintptr_t)wbref->getMethodAddress(),
                                        new (cg->trHeapMemory()) TR::RegisterDependencyConditions((uint8_t)0, 0, cg->trMemory()),
                                        wbref, NULL);

   if (gcMode != gc_modron_wrtbar_always)
      {
      // conditional call to the write barrier
      cursor->setConditionCode(ARMConditionCodeNE);
      }

   // place label that marks work was done.
   generateLabelInstruction(cg, ARMOp_label, node, doneLabel, deps);

   if (flagReg == NULL)
      cg->stopUsingRegister(tempReg);
   }

void VMgenerateCatchBlockBBStartPrologue(TR::Node *node, TR::Instruction *fenceInstruction, TR::CodeGenerator *cg)
   {
   /* @@ not implemented @@ */
   }

#else /* TR_TARGET_ARM   */
// the following is to force an export to keep ilib happy
int J9ARMEvaluator=0;
#endif /* TR_TARGET_ARM   */



