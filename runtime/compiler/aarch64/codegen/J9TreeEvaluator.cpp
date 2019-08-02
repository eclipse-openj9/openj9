/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#include "codegen/ARM64Instruction.hpp"
#include "codegen/ARM64JNILinkage.hpp"
#include "codegen/ARM64PrivateLinkage.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/ARM64Instruction.hpp"
#include "codegen/J9ARM64Snippet.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/OMRDataTypes_inlines.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "arm\codegen\OMRTreeEvaluator.hpp"

/*
 * J9 ARM64 specific tree evaluator table overrides
 */
extern void TEMPORARY_initJ9ARM64TreeEvaluatorTable(TR::CodeGenerator *cg)
   {
   TR_TreeEvaluatorFunctionPointer *tet = cg->getTreeEvaluatorTable();

   tet[TR::awrtbar] = TR::TreeEvaluator::awrtbarEvaluator;
   tet[TR::awrtbari] = TR::TreeEvaluator::awrtbariEvaluator;
   tet[TR::monexit] = TR::TreeEvaluator::monexitEvaluator;
   tet[TR::monent] = TR::TreeEvaluator::monentEvaluator;
   tet[TR::monexitfence] = TR::TreeEvaluator::monexitfenceEvaluator;
   tet[TR::asynccheck] = TR::TreeEvaluator::asynccheckEvaluator;
   tet[TR::instanceof] = TR::TreeEvaluator::instanceofEvaluator;
   tet[TR::checkcast] = TR::TreeEvaluator::checkcastEvaluator;
   tet[TR::checkcastAndNULLCHK] = TR::TreeEvaluator::checkcastAndNULLCHKEvaluator;
   tet[TR::New] = TR::TreeEvaluator::newObjectEvaluator;
   tet[TR::variableNew] = TR::TreeEvaluator::newObjectEvaluator;
   tet[TR::newarray] = TR::TreeEvaluator::newArrayEvaluator;
   tet[TR::anewarray] = TR::TreeEvaluator::anewArrayEvaluator;
   tet[TR::variableNewArray] = TR::TreeEvaluator::anewArrayEvaluator;
   tet[TR::multianewarray] = TR::TreeEvaluator::multianewArrayEvaluator;
   tet[TR::arraylength] = TR::TreeEvaluator::arraylengthEvaluator;
   tet[TR::ResolveCHK] = TR::TreeEvaluator::resolveCHKEvaluator;
   tet[TR::DIVCHK] = TR::TreeEvaluator::DIVCHKEvaluator;
   tet[TR::BNDCHK] = TR::TreeEvaluator::BNDCHKEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::ArrayCopyBNDCHK] = TR::TreeEvaluator::ArrayCopyBNDCHKEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::BNDCHKwithSpineCHK] = TR::TreeEvaluator::BNDCHKwithSpineCHKEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::SpineCHK] = TR::TreeEvaluator::BNDCHKwithSpineCHKEvaluator;
   tet[TR::ArrayStoreCHK] = TR::TreeEvaluator::ArrayStoreCHKEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::ArrayCHK] = TR::TreeEvaluator::ArrayCHKEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::ArrayStoreCHK] = TR::TreeEvaluator::ArrayStoreCHKEvaluator;
   tet[TR::ArrayCHK] = TR::TreeEvaluator::ArrayCHKEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::MethodEnterHook] = TR::TreeEvaluator::conditionalHelperEvaluator;
   // TODO:ARM64: Enable when Implemented: tet[TR::MethodExitHook] = TR::TreeEvaluator::conditionalHelperEvaluator;
   tet[TR::allocationFence] = TR::TreeEvaluator::flushEvaluator;
   tet[TR::loadFence] = TR::TreeEvaluator::flushEvaluator;
   tet[TR::storeFence] = TR::TreeEvaluator::flushEvaluator;
   tet[TR::fullFence] = TR::TreeEvaluator::flushEvaluator;

   }

void VMgenerateCatchBlockBBStartPrologue(TR::Node *node, TR::Instruction *fenceInstruction, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());

   TR::Block *block = node->getBlock();

   if (fej9->shouldPerformEDO(block, comp))
      {
      TR_UNIMPLEMENTED();
      }
   }

void
J9::ARM64::TreeEvaluator::generateTestAndReportFieldWatchInstructions(TR::CodeGenerator *cg, TR::Node *node, TR::Snippet *dataSnippet, bool isWrite, TR::Register *sideEffectRegister, TR::Register *valueReg, TR::Register *dataSnippetRegister)
   {
   TR_ASSERT_FATAL(false, "This helper implements platform specific code for Fieldwatch, which is currently not supported on ARM64 platforms.\n");
   }

void
J9::ARM64::TreeEvaluator::generateFillInDataBlockSequenceForUnresolvedField(TR::CodeGenerator *cg, TR::Node *node, TR::Snippet *dataSnippet, bool isWrite, TR::Register *sideEffectRegister, TR::Register *dataSnippetRegister)
   {
   TR_ASSERT_FATAL(false, "This helper implements platform specific code for Fieldwatch, which is currently not supported on ARM64 platforms.\n");
   }

TR::Register *
J9::ARM64::TreeEvaluator::awrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

TR::Register *
J9::ARM64::TreeEvaluator::awrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

TR::Register *
J9::ARM64::TreeEvaluator::DIVCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *divisor = node->getFirstChild()->getSecondChild();
   bool is64Bit = node->getFirstChild()->getType().isInt64();
   bool isConstDivisor = divisor->getOpCode().isLoadConst();

   if (!isConstDivisor || (!is64Bit && divisor->getInt() == 0) || (is64Bit && divisor->getLongInt() == 0))
      {
      TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);
      cg->addSnippet(new (cg->trHeapMemory()) TR::ARM64HelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference()));

      if (isConstDivisor)
         {
         // No explicit check required
         generateLabelInstruction(cg, TR::InstOpCode::b, node, snippetLabel);
         }
      else
         {
         TR::Register *divisorReg = cg->evaluate(divisor);
         TR::InstOpCode::Mnemonic compareOp = is64Bit ? TR::InstOpCode::cbzx : TR::InstOpCode::cbzw;
         generateCompareBranchInstruction(cg, compareOp, node, divisorReg, snippetLabel, NULL);
         }
      }
	
TR::Register *
J9::ARM64::TreeEvaluator::ArrayCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::VMarrayCheckEvaluator(node, cg);
   }

TR::Register *
J9::ARM64::TreeEvaluator::VMarrayCheckEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   TR::Register *obj1Reg = cg->evaluate(node->getFirstChild());
   TR::Register *obj2Reg = cg->evaluate(node->getSecondChild());
   TR::Register *tmp1Reg = cg->allocateRegister();
   TR::Register *tmp2Reg = cg->allocateRegister();
   TR::LabelSymbol doneLabel = generateLabelSymbol(cg);
   TR::RegisterDependencyConditions deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(2, 2, cg->trMemory());
   TR::Compilation* comp = cg->comp();
   TR::LabelSymbol *snippetLabel;
   TR::Instruction *gcPoint;
   TR::Snippet *snippet;

   TR::addDependency(deps, obj1Reg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(deps, obj2Reg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(deps, tmp1Reg, TR::RealRegister::NoReg, TR_GPR, cg);
   TR::addDependency(deps, tmp2Reg, TR::RealRegister::NoReg, TR_GPR, cg);

   // Same array, we are done.
   generateCompareInstruction(cg, node, obj1Reg, obj2Reg);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, doneLabel, TR::CC_EQ);

   // If we know nothing about either object, test object1 first. It has to be an array.
   if ((!node->isArrayChkPrimitiveArray1())
         && (!node->isArrayChkReferenceArray1())
         && (!node->isArrayChkPrimitiveArray2())
         && (!node->isArrayChkReferenceArray2()))
      {
#ifdef OMR_GC_COMPRESSED_POINTERS
      generateTrg1MemInstruction(cg, TR::InstOpCode::ldrw, node, tmp1Reg,
            new (cg->trHeapMemory()) TR::MemoryReference(obj1Reg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg));
#else
      generateTrg1MemInstruction(cg, TR::InstOpCode::ldrw, node, tmp1Reg,
            new (cg->trHeapMemory()) TR::MemoryReference(obj1Reg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg));
#endif

      TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, tmp1Reg);
      generateTrg1MemInstruction(cg, TR::InstOpCode::ldrw, node, tmp1Reg,
            new (cg->trHeapMemory()) TR::MemoryReference(tmp1Reg, (int32_t) offsetof(J9Class, classDepthAndFlags), cg));

      loadConstant32(cg, node, (int32_t) J9AccClassRAMArray, tmp2Reg);
      generateCompareInstruction(cg, node, tmp2Reg, tmp1Reg);

      generateCompareImmInstruction(cg, node, tmp2Reg, NULLVALUE);

      snippetLabel = generateLabelSymbol(cg);
      gcPoint = generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, snippetLabel, TR::CC_EQ);
      snippet = new (cg->trHeapMemory()) TR::ARM64HelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference(), doneLabel);
      cg->addSnippet(snippet);
      }

   // One of the object is array. Test equality of two objects' classes.
#ifdef OMR_GC_COMPRESSED_POINTERS
   // must read only 32 bits
   generateTrg1MemInstruction(cg, TR::InstOpCode::ldrw, node, tmp2Reg,
         new (cg->trHeapMemory()) TR::MemoryReference(obj2Reg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg));
   generateTrg1MemInstruction(cg, TR::InstOpCode::ldrw, node, tmp1Reg,
         new (cg->trHeapMemory()) TR::MemoryReference(obj1Reg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg));
#else
   generateTrg1MemInstruction(cg, TR::InstOpCode::ldrw, node, tmp2Reg,
         new (cg->trHeapMemory()) TR::MemoryReference(obj2Reg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceAddress(), cg));
   generateTrg1MemInstruction(cg, TR::InstOpCode::ldrw, node, tmp1Reg,
         new (cg->trHeapMemory()) TR::MemoryReference(obj1Reg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceAddress(), cg));
#endif
   TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, tmp2Reg);
   TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, tmp1Reg);
   generateCompareInstruction(cg, node, tmp1Reg, tmp2Reg);

   // If either object is known to be of primitive component type,
   // we are done: since both of them have to be of equal class.
   if (node->isArrayChkPrimitiveArray1() || node->isArrayChkPrimitiveArray2())
      {
      if (NULL == snippetLabel)
         {
         snippetLabel = generateLabelSymbol(cg);
         gcPoint = generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, snippetLabel, TR::CC_NE);
         snippet = new (cg->trHeapMemory()) TR::ARM64HelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference(), doneLabel);
         cg->addSnippet(snippet);
         }
      else
         generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, snippetLabel, TR::CC_NE);
      }
   // We have to take care of the un-equal class situation: both of them must be of reference array
   else
      {
      generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, doneLabel, TR::CC_EQ);

      // Object1 must be of reference component type, otherwise throw exception
      if (!node->isArrayChkReferenceArray1())
         {

         // Loading the Class Pointer -> classDepthAndFlags
#ifdef OMR_GC_COMPRESSED_POINTERS
         generateTrg1MemInstruction(cg, TR::InstOpCode::ldrw, node, tmp1Reg,
               new (cg->trHeapMemory()) TR::MemoryReference(obj1Reg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg));
#else
         generateTrg1MemInstruction(cg, TR::InstOpCode::ldrw, node, tmp1Reg,
               new (cg->trHeapMemory()) TR::MemoryReference(obj1Reg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg));
#endif

         TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, tmp1Reg);
         generateTrg1MemInstruction(cg, TR::InstOpCode::ldrw, node, tmp1Reg,
               new (cg->trHeapMemory()) TR::MemoryReference(tmp1Reg, (int32_t) offsetof(J9Class, classDepthAndFlags), cg));

         // We already have classDepth&Flags in tmp1Reg.  X = (ramclass->ClassDepthAndFlags)>>J9AccClassRAMShapeShift
         generateLogicalShiftRightImmInstruction(cg, node, tmp1Reg, tmp1Reg, J9AccClassRAMShapeShift);

         // We need to perform a X & OBJECT_HEADER_SHAPE_MASK
         generateNegInstruction(cg, node, obj2Reg, obj2Reg); // change ROL to ROR
         generateTrg1Src2Instruction(cg, TR::InstOpCode::rorvw, node, tmp1Reg, tmp1Reg, obj2Reg);
         generateCompareImmInstruction(cg, node, tmp2Reg, OBJECT_HEADER_SHAPE_POINTERS);

         if (NULL == snippetLabel)
            {
            snippetLabel = generateLabelSymbol(cg);
            gcPoint = generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, snippetLabel, TR::CC_NE);
            snippet = new (cg->trHeapMemory()) TR::ARM64HelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference(), doneLabel);
            cg->addSnippet(snippet);
            }
         else
            generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, snippetLabel, TR::CC_NE);
         }

      // Object2 must be of reference component type array, otherwise throw exception
      if (!node->isArrayChkReferenceArray2())
         {
#ifdef OMR_GC_COMPRESSED_POINTERS
         generateTrg1MemInstruction(cg, TR::InstOpCode::ldrw, node, tmp1Reg,
               new (cg->trHeapMemory()) TR::MemoryReference(obj2Reg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg));
#else
         generateTrg1MemInstruction(cg, TR::InstOpCode::ldrw, node, tmp1Reg,
               new (cg->trHeapMemory()) TR::MemoryReference(obj2Reg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg));
#endif

         TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, tmp1Reg);
         generateTrg1MemInstruction(cg, TR::InstOpCode::ldrw, node, tmp1Reg,
               new (cg->trHeapMemory()) TR::MemoryReference(tmp1Reg, (int32_t) offsetof(J9Class, classDepthAndFlags), cg));

         loadConstant32(cg, node, (int32_t) J9AccClassRAMArray, tmp2Reg);
         generateCompareInstruction(cg, node, tmp2Reg, tmp1Reg);

         TR::Instruction * i = generateCompareImmInstruction(cg, node, tmp2Reg, NULLVALUE);

         if (NULL == snippetLabel)
            {
            snippetLabel = generateLabelSymbol(cg);
            gcPoint = generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, snippetLabel, TR::CC_EQ);
            snippet = new (cg->trHeapMemory()) TR::ARM64HelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference(), doneLabel);
            cg->addSnippet(snippet);
            }
         else
            generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, snippetLabel, TR::CC_EQ);

         // We already have classDepth&Flags in tmp1Reg.  X = (ramclass->ClassDepthAndFlags)>>J9AccClassRAMShapeShift
         generateLogicalShiftRightImmInstruction(cg, node, tmp1Reg, tmp1Reg, J9AccClassRAMShapeShift);

         // We need to perform a X & OBJECT_HEADER_SHAPE_MASK
         generateNegInstruction(cg, node, obj2Reg, obj2Reg); // change ROL to ROR
         generateTrg1Src2Instruction(cg, TR::InstOpCode::rorvw, node, tmp1Reg, tmp1Reg, obj2Reg);
         generateCompareImmInstruction(cg, node, tmp2Reg, OBJECT_HEADER_SHAPE_POINTERS);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, snippetLabel, TR::CC_NE);
         }
      }

   generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, deps);
   if (NULL != snippetLabel)
      {
      gcPoint->ARM64NeedsGCMap(cg, 0x0);
      snippet->gcMap().setGCRegisterMask(0x0);
      }

   cg->stopUsingRegister(tmp1Reg);
   cg->stopUsingRegister(tmp2Reg);
   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   return NULL;
   }

TR::Register *
J9::ARM64::TreeEvaluator::monexitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR::Node::recreate(node, TR::call);
   TR::Register *targetRegister = directCallEvaluator(node, cg);
   TR::Node::recreate(node, opCode);
   return targetRegister;
   }

TR::Register *
J9::ARM64::TreeEvaluator::asynccheckEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The child contains an inline test. If it succeeds, the helper is called.
   // The address of the helper is contained as a long in this node.
   //
   TR::Node *testNode = node->getFirstChild();
   TR::Node *firstChild = testNode->getFirstChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Node *secondChild = testNode->getSecondChild();
   TR::Register *src2Reg = cg->evaluate(secondChild);

   TR_ASSERT(testNode->getOpCodeValue() == TR::lcmpeq, "asynccheck bad format");

   TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);
   cg->addSnippet(new (cg->trHeapMemory()) TR::ARM64HelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference()));

   // ToDo:
   // Optimize this using "cmp (immediate)" instead of "cmp (register)" when possible
   generateCompareInstruction(cg, node, src1Reg, src2Reg, true); // 64-bit compare

   TR::Instruction *gcPoint = generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, snippetLabel, TR::CC_EQ);
   gcPoint->ARM64NeedsGCMap(cg, 0xFFFFFFFF);

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->decReferenceCount(testNode);
   return NULL;
   }

TR::Register *
J9::ARM64::TreeEvaluator::instanceofEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // Call helper
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR::Node::recreate(node, TR::icall);
   TR::Register *targetRegister = directCallEvaluator(node, cg);
   TR::Node::recreate(node, opCode);
   return targetRegister;
   }

TR::Register *
J9::ARM64::TreeEvaluator::checkcastAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return checkcastEvaluator(node, cg);
   }

TR::Register *
J9::ARM64::TreeEvaluator::checkcastEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR::Node::recreate(node, TR::call);
   TR::Register *targetRegister = directCallEvaluator(node, cg);
   TR::Node::recreate(node, opCode);

   return targetRegister;
   }

TR::Register *
J9::ARM64::TreeEvaluator::flushEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes op = node->getOpCodeValue();

   if (op == TR::allocationFence)
      {
      if (!node->canOmitSync())
         {
         // Data synchronization barrier -- 0xF to turn on option for both reads and writes
         generateSynchronizationInstruction(cg, TR::InstOpCode::dsb, node, 0xF);
         }
      }
   else
      {
      uint32_t imm;
      if (op == TR::loadFence)
         {
         // 0xD to turn on option for reads
         imm = 0xD;
         }
      else if (op == TR::storeFence)
         {
         // 0xE to turn on option for writes
         imm = 0xE;
         }
      else
         {
         // 0xF to turn on option for both reads and writes
         imm = 0xF;
         }
      generateSynchronizationInstruction(cg, TR::InstOpCode::dsb, node, imm);
      }

   return NULL;
   }

TR::Register *
J9::ARM64::TreeEvaluator::multianewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR::Node::recreate(node, TR::acall);
   TR::Register *targetRegister = directCallEvaluator(node, cg);
   TR::Node::recreate(node, opCode);
   return targetRegister;
   }

TR::Register *
J9::ARM64::TreeEvaluator::newObjectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR::Node::recreate(node, TR::acall);
   TR::Register *targetRegister = directCallEvaluator(node, cg);
   TR::Node::recreate(node, opCode);
   return targetRegister;
   }

TR::Register *
J9::ARM64::TreeEvaluator::newArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR::Node::recreate(node, TR::acall);
   TR::Register *targetRegister = directCallEvaluator(node, cg);
   TR::Node::recreate(node, opCode);
   return targetRegister;
   }

TR::Register *
J9::ARM64::TreeEvaluator::anewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR::Node::recreate(node, TR::acall);
   TR::Register *targetRegister = directCallEvaluator(node, cg);
   TR::Node::recreate(node, opCode);
   return targetRegister;
   }

TR::Register *
J9::ARM64::TreeEvaluator::monentEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR::Node::recreate(node, TR::call);
   TR::Register *targetRegister = directCallEvaluator(node, cg);
   TR::Node::recreate(node, opCode);
   return targetRegister;
   }

TR::Register *
J9::ARM64::TreeEvaluator::arraylengthEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   // ldrw R, [B + contiguousSize]        
   // cmp R, 0                            
   // cselw R, [B + discontiguousSize]    
   //
   TR::Register *objectReg = cg->evaluate(node->getFirstChild());
   TR::Register *lengthReg = cg->allocateRegister();
   TR::Register *discontiguousLengthReg = cg->allocateRegister();

   TR::MemoryReference *contiguousArraySizeMR = new (cg->trHeapMemory()) TR::MemoryReference(objectReg, fej9->getOffsetOfContiguousArraySizeField(), cg);
   TR::MemoryReference *discontiguousArraySizeMR = new (cg->trHeapMemory()) TR::MemoryReference(objectReg, fej9->getOffsetOfDiscontiguousArraySizeField(), cg);

   generateTrg1MemInstruction(cg, TR::InstOpCode::ldrw, node, lengthReg, contiguousArraySizeMR);
   generateCompareImmInstruction(cg, node, lengthReg, 0);
   generateTrg1MemInstruction(cg, TR::InstOpCode::ldrw, node, discontiguousLengthReg, discontiguousArraySizeMR);
   generateCondTrg1Src2Instruction(cg, TR::InstOpCode::cselw, node, lengthReg, lengthReg, discontiguousLengthReg, TR::CC_NE);

   cg->stopUsingRegister(discontiguousLengthReg);
   cg->decReferenceCount(node->getFirstChild());
   node->setRegister(lengthReg);

   return lengthReg;
   }

TR::Register *
J9::ARM64::TreeEvaluator::BNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *secondChild = node->getSecondChild();
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg;
   TR::Register *src2Reg = NULL;
   uint64_t value;
   TR::LabelSymbol *snippetLabel;
   TR::Instruction *gcPoint;
   bool reversed = false;

   if ((firstChild->getOpCode().isLoadConst())
         && (constantIsUnsignedImm12(firstChild->get64bitIntegralValueAsUnsigned()))
         && (NULL == firstChild->getRegister()))
      {
      src2Reg = cg->evaluate(secondChild);
      reversed = true;
      }
   else
      {
      src1Reg = cg->evaluate(firstChild);

      if ((secondChild->getOpCode().isLoadConst())
            && (NULL == secondChild->getRegister()))
         {
         value = secondChild->get64bitIntegralValueAsUnsigned();
         if (!constantIsUnsignedImm12(value))
            {
            src2Reg = cg->evaluate(secondChild);
            }
         }
      else
         src2Reg = cg->evaluate(secondChild);
      }

   if (reversed)
      {
      generateCompareImmInstruction(cg, node, src2Reg, firstChild->get64bitIntegralValueAsUnsigned());
      }
   else
      {
      if (NULL == src2Reg)
         generateCompareImmInstruction(cg, node, src1Reg, value);
      else
         generateCompareInstruction(cg, node, src1Reg, src2Reg);
      }

   snippetLabel = generateLabelSymbol(cg);
   cg->addSnippet(new (cg->trHeapMemory()) TR::ARM64HelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference()));
   
   gcPoint = generateConditionalBranchInstruction(cg, TR::InstOpCode::b_cond, node, snippetLabel, TR::CC_CS);

   gcPoint->ARM64NeedsGCMap(cg, 0xFFFFFFFF);

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   secondChild->setIsNonNegative(true);
   return (NULL);
   }

static void VMoutlinedHelperArrayStoreCHKEvaluator(TR::Node *node, TR::Register *srcReg, TR::Register *dstReg, TR::CodeGenerator *cg)
   {
   TR::SymbolReference *arrayStoreChkHelper = node->getSymbolReference();

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(2, 2, cg->trMemory());
   TR::addDependency(deps, dstReg, TR::RealRegister::x0, TR_GPR, cg);
   TR::addDependency(deps, srcReg, TR::RealRegister::x1, TR_GPR, cg);

   TR::Instruction *gcPoint = generateImmSymInstruction(cg, TR::InstOpCode::bl, node,
                                                        (uintptr_t)arrayStoreChkHelper->getMethodAddress(),
                                                        deps, arrayStoreChkHelper, NULL);
   gcPoint->ARM64NeedsGCMap(cg, 0xFFFFFFFF);

   cg->machine()->setLinkRegisterKilled(true);
   }

static void VMoutlinedHelperWrtbarEvaluator(TR::Node *node, TR::Register *srcReg, TR::Register *dstReg, TR::CodeGenerator *cg)
   {
   auto gcMode = TR::Compiler->om.writeBarrierType();
   if (gcMode == gc_modron_wrtbar_none)
      return;

   TR::Compilation *comp = cg->comp();
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
   TR::SymbolReference *wbref = comp->getSymRefTab()->findOrCreateWriteBarrierStoreSymbolRef(comp->getMethodSymbol());

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(2, 2, cg->trMemory());
   TR::addDependency(deps, dstReg, TR::RealRegister::x0, TR_GPR, cg);
   TR::addDependency(deps, srcReg, TR::RealRegister::x1, TR_GPR, cg);

   generateCompareBranchInstruction(cg, TR::InstOpCode::cbzx, node, srcReg, doneLabel);

   TR::Instruction *gcPoint = generateImmSymInstruction(cg, TR::InstOpCode::bl, node,
                                                        (uintptr_t)wbref->getMethodAddress(),
                                                        new (cg->trHeapMemory()) TR::RegisterDependencyConditions((uint8_t)0, 0, cg->trMemory()),
                                                        wbref, NULL);
   gcPoint->ARM64NeedsGCMap(cg, 0xFFFFFFFF);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, deps);

   cg->machine()->setLinkRegisterKilled(true);
   }

TR::Register *
J9::ARM64::TreeEvaluator::ArrayStoreCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *wrtBarNode = node->getFirstChild();
   TR::Node *srcNode = wrtBarNode->getSecondChild();
   TR::Node *dstNode = wrtBarNode->getThirdChild();
   TR::Register *srcReg = cg->evaluate(srcNode);
   TR::Register *dstReg = cg->evaluate(dstNode);

   if (!srcNode->isNull())
      {
      VMoutlinedHelperArrayStoreCHKEvaluator(node, srcReg, dstReg, cg);
      }

   TR::MemoryReference *storeMR = new (cg->trHeapMemory()) TR::MemoryReference(wrtBarNode, TR::Compiler->om.sizeofReferenceAddress(), cg);
   generateMemSrc1Instruction(cg, TR::InstOpCode::strimmx, node, storeMR, srcReg);

   if (!srcNode->isNull())
      {
      VMoutlinedHelperWrtbarEvaluator(wrtBarNode, srcReg, dstReg, cg);
      }

   cg->decReferenceCount(srcNode);
   cg->decReferenceCount(dstNode);
   storeMR->decNodeReferenceCounts(cg);
   cg->decReferenceCount(wrtBarNode);

   return NULL;
   }
