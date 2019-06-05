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

//On zOS XLC linker can't handle files with same name at link time
//This workaround with pragma is needed. What this does is essentially
//give a different name to the codesection (csect) for this file. So it
//doesn't conflict with another file with same name.
#pragma csect(CODE,"TRJ9ZTreeEvalBase#C")
#pragma csect(STATIC,"TRJ9ZTreeEvalBase#S")
#pragma csect(TEST,"TRJ9ZTreeEvalBase#T")

#include <algorithm>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "omrmodroncore.h"
#include "thrdsup.h"
#include "thrtypes.h"
#include "codegen/AheadOfTimeCompile.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/J9WatchedStaticFieldSnippet.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/VirtualGuard.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
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
#include "il/symbol/ParameterSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "infra/Bit.hpp"
#include "ras/Delimiter.hpp"
#include "ras/DebugCounter.hpp"
#include "env/VMJ9.h"
#include "z/codegen/J9S390PrivateLinkage.hpp"
#include "z/codegen/J9S390Snippet.hpp"
#include "z/codegen/J9S390CHelperLinkage.hpp"
#include "z/codegen/BinaryCommutativeAnalyser.hpp"
#include "z/codegen/S390J9CallSnippet.hpp"
#include "z/codegen/ForceRecompilationSnippet.hpp"
#include "z/codegen/ReduceSynchronizedFieldLoad.hpp"
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390HelperCallSnippet.hpp"
#include "z/codegen/S390Instruction.hpp"
#include "z/codegen/S390Recompilation.hpp"
#include "z/codegen/S390Register.hpp"
#include "z/codegen/SystemLinkage.hpp"
#include "runtime/J9Profiler.hpp"

/*
 * List of functions that is needed by J9 Specific Evaluators that were moved from codegen.
 * Since other evaluators in codegen still calls these, extern here in order to call them.
 */
extern void updateReferenceNode(TR::Node * node, TR::Register * reg);
extern void killRegisterIfNotLocked(TR::CodeGenerator * cg, TR::RealRegister::RegNum reg, TR::Instruction * instr , TR::RegisterDependencyConditions * deps = NULL);
extern TR::Register * iDivRemGenericEvaluator(TR::Node * node, TR::CodeGenerator * cg, bool isDivision, TR::MemoryReference * divchkDivisorMR);
extern TR::Instruction * generateS390CompareOps(TR::Node * node, TR::CodeGenerator * cg, TR::InstOpCode::S390BranchCondition fBranchOpCond, TR::InstOpCode::S390BranchCondition rBranchOpCond, TR::LabelSymbol * targetLabel);


/* Moved from Codegen to FE */
///////////////////////////////////////////////////////////////////////////////////
// Generate code to perform a comparison and branch to a snippet.
// This routine is used mostly by bndchk evaluator.
//
// The comparison type is determined by the choice of CMP operators:
//   - fBranchOp:  Operator used for forward operation ->  A fCmp B
//   - rBranchOp:  Operator user for reverse operation ->  B rCmp A <=> A fCmp B
//
// TODO - avoid code duplication, this routine may be able to merge with the one
//        above which has the similar logic.
///////////////////////////////////////////////////////////////////////////////////
TR::Instruction *
generateS390CompareBranchLabel(TR::Node * node, TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic branchOp, TR::InstOpCode::S390BranchCondition fBranchOpCond, TR::InstOpCode::S390BranchCondition rBranchOpCond,
   TR::LabelSymbol * label)
   {
   return generateS390CompareOps(node, cg, fBranchOpCond, rBranchOpCond, label);
   }

/* Moved from Codegen to FE since only awrtbarEvaluator calls this function */
static TR::Register *
allocateWriteBarrierInternalPointerRegister(TR::CodeGenerator * cg, TR::Node * sourceChild)
   {
   TR::Register * sourceRegister;

   if (sourceChild->getRegister() != NULL && !cg->canClobberNodesRegister(sourceChild))
      {
      if (!sourceChild->getRegister()->containsInternalPointer())
         {
         sourceRegister = cg->allocateCollectedReferenceRegister();
         }
      else
         {
         sourceRegister = cg->allocateRegister();
         sourceRegister->setPinningArrayPointer(sourceChild->getRegister()->getPinningArrayPointer());
         sourceRegister->setContainsInternalPointer();
         }
      generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), sourceChild, sourceRegister, sourceChild->getRegister());
      }
   else
      {
      sourceRegister = cg->evaluate(sourceChild);
      }

   return sourceRegister;
   }


extern TR::Register *
doubleMaxMinHelper(TR::Node *node, TR::CodeGenerator *cg, bool isMaxOp)
   {
   TR_ASSERT(node->getNumChildren() >= 1  || node->getNumChildren() <= 2, "node has incorrect number of children");

   /* ===================== Allocating Registers  ===================== */

   TR::Register      * v16           = cg->allocateRegister(TR_VRF);
   TR::Register      * v17           = cg->allocateRegister(TR_VRF);
   TR::Register      * v18           = cg->allocateRegister(TR_VRF);

   /* ===================== Generating instructions  ===================== */

   /* ====== LD FPR0,16(GPR5)       Load a ====== */
   TR::Register      * v0      = cg->fprClobberEvaluate(node->getFirstChild());

   /* ====== LD FPR2, 0(GPR5)       Load b ====== */
   TR::Register      * v2      = cg->evaluate(node->getSecondChild());

   /* ====== WFTCIDB V16,V0,X'F'     a == NaN ====== */
   generateVRIeInstruction(cg, TR::InstOpCode::VFTCI, node, v16, v0, 0xF, 8, 3);

   /* ====== For Max: WFCHE V17,V0,V2     Compare a >= b ====== */
   if(isMaxOp)
      {
      generateVRRcInstruction(cg, TR::InstOpCode::VFCH, node, v17, v0, v2, 0, 8, 3);
      }
   /* ====== For Min: WFCHE V17,V0,V2     Compare a <= b ====== */
   else
      {
      generateVRRcInstruction(cg, TR::InstOpCode::VFCH, node, v17, v2, v0, 0, 8, 3);
      }

   /* ====== VO V16,V16,V17     (a >= b) || (a == NaN) ====== */
   generateVRRcInstruction(cg, TR::InstOpCode::VO, node, v16, v16, v17, 0, 0, 0);

   /* ====== For Max: WFTCIDB V17,V0,X'800'     a == +0 ====== */
   if(isMaxOp)
    {
       generateVRIeInstruction(cg, TR::InstOpCode::VFTCI, node, v17, v0, 0x800, 8, 3);
    }
   /* ====== For Min: WFTCIDB V17,V0,X'400'     a == -0 ====== */
   else
    {
       generateVRIeInstruction(cg, TR::InstOpCode::VFTCI, node, v17, v0, 0x400, 8, 3);
    }
   /* ====== WFTCIDB V18,V2,X'C00'       b == 0 ====== */
   generateVRIeInstruction(cg, TR::InstOpCode::VFTCI, node, v18, v2, 0xC00, 8, 3);

   /* ====== VN V17,V17,V18     (a == -0) && (b == 0) ====== */
   generateVRRcInstruction(cg, TR::InstOpCode::VN, node, v17, v17, v18, 0, 0, 0);

   /* ====== VO V16,V16,V17     (a >= b) || (a == NaN) || ((a == -0) && (b == 0)) ====== */
   generateVRRcInstruction(cg, TR::InstOpCode::VO, node, v16, v16, v17, 0, 0, 0);

   /* ====== VSEL V0,V0,V2,V16 ====== */
   generateVRReInstruction(cg, TR::InstOpCode::VSEL, node, v0, v0, v2, v16);

   /* ===================== Deallocating Registers  ===================== */
   cg->stopUsingRegister(v2);
   cg->stopUsingRegister(v16);
   cg->stopUsingRegister(v17);
   cg->stopUsingRegister(v18);

   node->setRegister(v0);

   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());

   return node->getRegister();
   }

/**
 * \brief
 *
 * Use vector instructions to find the index of a sub-string inside
 * a string assuming both strings have the same element size. Each element
 * is 1-byte for compact strings and 2-bytes for non-compressed strings.
 *
 * \details
 *
 * The vector sequence searches for the first character of the sub-string
 * inside the source/master string. If the first character is located, it'll
 * perform iterative vector binary compares to match the rest of the sub-string
 * starting from the first character position.
 *
 * This evaluator inlines the following Java intrinsic methods:
 *
 * <verbatim>
 * For Java 9 and above:
 *
 * StringLatin1.indexOf(s1Value, s1Length, s2Value, s2Length, fromIndex);
 * StringUTF16.indexOf(s1Value, s1Length, s2Value, s2Length, fromIndex);
 *
 * For Java 8:
 * com.ibm.jit.JITHelpers.intrinsicIndexOfStringLatin1(char[] s1Value, int s1len, char[] s2Value, int s2len, int start);
 * com.ibm.jit.JITHelpers.intrinsicIndexOfStringUTF16(char[] s1Value, int s1len, char[] s2Value, int s2len, int start);
 *
 * Assumptions:
 *
 * -# 0 <= fromIndex < s1Length
 * -# s1Length could be anything: positive, negative or 0.
 * -# s2Length > 0
 * -# s1Value and s2Value are non-null arrays and are interpreted as byte arrays.
 * -# s1Length and s2Length are not related. i.e. s1Length could be smallers than s2Length.
 *
 * <\verbatim>
 *
 * \param node the intrinsic function call node
 * \param cg the code generator
 * \param isUTF16 true if the string is a decompressed string.
 *
 * \return a register for that contains the indexOf() result.
*/
TR::Register*
inlineVectorizedStringIndexOf(TR::Node* node, TR::CodeGenerator* cg, bool isUTF16)
   {
   const uint32_t elementSizeMask = isUTF16 ? 1 : 0;
   const int8_t vectorSize = cg->machine()->getVRFSize();
   const uintptrj_t headerSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
   const bool supportsVSTRS = TR::Compiler->target.cpu.getSupportsVectorFacilityEnhancement2();
   TR::Compilation* comp = cg->comp();

   if (comp->getOption(TR_TraceCG))
      traceMsg(comp, "inlineVectorizedStringIndexOf. Is isUTF16 %d\n", isUTF16);

   // This evaluator function handles different indexOf() intrinsics, some of which are static calls without a
   // receiver. Hence, the need for static call check.
   const bool isStaticCall = node->getSymbolReference()->getSymbol()->castToMethodSymbol()->isStatic();
   const uint8_t firstCallArgIdx = isStaticCall ? 0 : 1;

   // Get call parameters where s1Value and s2Value are byte arrays
   TR::Register* s1ValueReg = cg->evaluate(node->getChild(firstCallArgIdx));
   TR::Register* s1LenReg = cg->gprClobberEvaluate(node->getChild(firstCallArgIdx+1));
   TR::Register* s2ValueReg = cg->evaluate(node->getChild(firstCallArgIdx+2));
   TR::Register* s2LenReg = cg->gprClobberEvaluate(node->getChild(firstCallArgIdx+3));
   TR::Register* fromIndexReg = cg->gprClobberEvaluate(node->getChild(firstCallArgIdx+4));

   // Registers
   TR::Register* resultReg          = cg->allocateRegister();
   TR::Register* maxIndexReg        = cg->allocateRegister();
   TR::Register* s1VecStartIndexReg = fromIndexReg;
   TR::Register* s2VecStartIndexReg = cg->allocateRegister();
   TR::Register* loadLenReg         = cg->allocateRegister();
   TR::Register* char1IndexReg      = resultReg;

   TR::Register* s1PartialVReg      = cg->allocateRegister(TR_VRF);
   TR::Register* s2PartialVReg      = cg->allocateRegister(TR_VRF);
   TR::Register* s2Char1RepVReg     = cg->allocateRegister(TR_VRF);
   TR::Register* tmpVReg            = cg->allocateRegister(TR_VRF);

   // Register dependencies
   TR::RegisterDependencyConditions* regDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, supportsVSTRS ? 14 : 13, cg);

   regDeps->addPostCondition(s1ValueReg, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(s1LenReg, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(s2ValueReg, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(s2LenReg, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(fromIndexReg, TR::RealRegister::AssignAny);

   regDeps->addPostCondition(resultReg, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(maxIndexReg, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(s2VecStartIndexReg, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(loadLenReg, TR::RealRegister::AssignAny);

   regDeps->addPostCondition(s1PartialVReg, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(s2PartialVReg, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(s2Char1RepVReg, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(tmpVReg, TR::RealRegister::AssignAny);

   // Labels
   TR::LabelSymbol* labelStart = generateLabelSymbol(cg);
   TR::LabelSymbol* labelFindS2Head = generateLabelSymbol(cg);
   TR::LabelSymbol* labelLoadLen16 = generateLabelSymbol(cg);
   TR::LabelSymbol* labelLoadLenDone = generateLabelSymbol(cg);
   TR::LabelSymbol* labelMatchS2Loop = generateLabelSymbol(cg);
   TR::LabelSymbol* labelMatchS2Residue = generateLabelSymbol(cg);
   TR::LabelSymbol* labelMatchS2LoopSetup = generateLabelSymbol(cg);
   TR::LabelSymbol* labelS2PartialMatch  = generateLabelSymbol(cg);
   TR::LabelSymbol* labelLoadResult = generateLabelSymbol(cg);
   TR::LabelSymbol* labelResultDone = generateLabelSymbol(cg);
   TR::LabelSymbol* labelStringNotFound = generateLabelSymbol(cg);
   TR::LabelSymbol* labelDone = generateLabelSymbol(cg);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelStart);
   labelStart->setStartInternalControlFlow();

   // Decompressed strings have [byte_length = char_length * 2]
   if (isUTF16 && TR::Compiler->target.is64Bit())
      {
      generateShiftAndKeepSelected64Bit(node, cg, s1LenReg, s1LenReg, 31, 62, 1, true, false);
      generateShiftAndKeepSelected64Bit(node, cg, s2LenReg, s2LenReg, 31, 62, 1, true, false);
      generateShiftAndKeepSelected64Bit(node, cg, fromIndexReg, fromIndexReg, 31, 62, 1, true, false);
      }
   else
      {
      generateRRInstruction(cg, TR::InstOpCode::LGFR, node, s1LenReg, s1LenReg);
      generateRRInstruction(cg, TR::InstOpCode::LGFR, node, s2LenReg, s2LenReg);
      generateRRInstruction(cg, TR::InstOpCode::LGFR, node, fromIndexReg, fromIndexReg);

      if (isUTF16)
         {
         generateRSInstruction(cg, TR::InstOpCode::SLL, node, s1LenReg, 1);
         generateRSInstruction(cg, TR::InstOpCode::SLL, node, s2LenReg, 1);
         generateRSInstruction(cg, TR::InstOpCode::SLL, node, fromIndexReg, 1);
         }
      }

   generateRRRInstruction(cg, TR::InstOpCode::getSubtractThreeRegOpCode(), node, maxIndexReg, s1LenReg, s2LenReg);
   generateRIEInstruction(cg, TR::InstOpCode::getCmpRegAndBranchRelOpCode(), node, maxIndexReg, fromIndexReg, labelStringNotFound, TR::InstOpCode::COND_BLR);

   // s2Len debug counters
   static bool enableIndexOfDebugCounter = feGetEnv("TR_EnableIndexOfDebugCounter") != NULL;
   if (enableIndexOfDebugCounter)
      {
      TR::LabelSymbol* labelS2LenGT10 = generateLabelSymbol(cg);
      TR::LabelSymbol* labelS2LenGT30 = generateLabelSymbol(cg);
      TR::LabelSymbol* labelS2LenGT60 = generateLabelSymbol(cg);
      TR::LabelSymbol* labelS2LenGT100 = generateLabelSymbol(cg);
      TR::LabelSymbol* labelS2LenCheckDone  = generateLabelSymbol(cg);

      uint8_t boundary10Char = isUTF16 ? 20 : 10;
      uint8_t boundary30Char = isUTF16 ? 60 : 30;
      uint8_t boundary60Char = isUTF16 ? 120 : 60;
      uint8_t boundary100Char = isUTF16 ? 200 : 100;

      generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, s2LenReg, boundary10Char, labelS2LenGT10, TR::InstOpCode::COND_BH);
      cg->generateDebugCounter("indexOfString/s2Len/below-10", 1, TR::DebugCounter::Cheap);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelS2LenCheckDone);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelS2LenGT10);
      generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, s2LenReg, boundary30Char, labelS2LenGT30, TR::InstOpCode::COND_BH);
      cg->generateDebugCounter("indexOfString/s2Len/10-30", 1, TR::DebugCounter::Cheap);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelS2LenCheckDone);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelS2LenGT30);
      generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, s2LenReg, boundary60Char, labelS2LenGT60, TR::InstOpCode::COND_BH);
      cg->generateDebugCounter("indexOfString/s2Len/30-60", 1, TR::DebugCounter::Cheap);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelS2LenCheckDone);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelS2LenGT60);
      generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, s2LenReg, boundary100Char, labelS2LenGT100, TR::InstOpCode::COND_BH);
      cg->generateDebugCounter("indexOfString/s2Len/60-100", 1, TR::DebugCounter::Cheap);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelS2LenCheckDone);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelS2LenGT100);
      cg->generateDebugCounter("indexOfString/s2Len/above-100", 1, TR::DebugCounter::Cheap);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelS2LenCheckDone);
      }

   if (supportsVSTRS)
      {
      TR::Register* s2HeadVReg = s2Char1RepVReg;
      TR::Register* s2LenVReg = cg->allocateRegister(TR_VRF);

      regDeps->addPostCondition(s2LenVReg, TR::RealRegister::AssignAny);

      // Load the first piece of s2Value (s2 header) which is either 16 bytes or s2Len
      TR::LabelSymbol* labelLoad16Bytes = generateLabelSymbol(cg);
      TR::LabelSymbol* labelLoadS2Done = generateLabelSymbol(cg);

      generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, s2LenReg, (int8_t)vectorSize, labelLoad16Bytes, TR::InstOpCode::COND_BNL);
      generateRIEInstruction(cg, TR::InstOpCode::getAddHalfWordImmDistinctOperandOpCode(), node, loadLenReg, s2LenReg, -1);
      generateVRSbInstruction(cg, TR::InstOpCode::VLL, node, s2HeadVReg, loadLenReg, generateS390MemoryReference(s2ValueReg, headerSize, cg));
      generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, loadLenReg, s2LenReg);
      generateVRSbInstruction(cg, TR::InstOpCode::VLVG, node, s2LenVReg, s2LenReg, generateS390MemoryReference(7, cg), 0);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelLoadS2Done);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelLoad16Bytes);
      generateVRXInstruction(cg, TR::InstOpCode::VL, node, s2HeadVReg, generateS390MemoryReference(s2ValueReg, headerSize, cg));
      generateRIInstruction(cg, TR::InstOpCode::LHI, node, loadLenReg, vectorSize);
      generateVRSbInstruction(cg, TR::InstOpCode::VLVG, node, s2LenVReg, loadLenReg, generateS390MemoryReference(7, cg), 0);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelLoadS2Done);

      // Loop to search for s2 header in s1
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelFindS2Head);

      // Determine s1 load length and load a piece of s1
      TR::Register* tmpReg = s2VecStartIndexReg;
      generateRRRInstruction(cg, TR::InstOpCode::getSubtractThreeRegOpCode(), node, loadLenReg, s1LenReg, s1VecStartIndexReg);
      generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, loadLenReg, (int8_t)vectorSize, labelLoadLen16, TR::InstOpCode::COND_BNL);
      generateRRRInstruction(cg, TR::InstOpCode::getAddThreeRegOpCode(), node, tmpReg, s1ValueReg, s1VecStartIndexReg);
      generateVRSbInstruction(cg, TR::InstOpCode::VLL, node, s1PartialVReg, loadLenReg, generateS390MemoryReference(tmpReg, headerSize, cg));
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelLoadLenDone);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelLoadLen16);
      generateVRXInstruction(cg, TR::InstOpCode::VL, node, s1PartialVReg, generateS390MemoryReference(s1ValueReg, s1VecStartIndexReg, headerSize, cg));
      generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, loadLenReg, vectorSize);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelLoadLenDone);

      // VSTRS sets CC with the following values:
      // CC = 0, no match or partial match, AND (zs = 0 OR no zero byte in source VRF)
      // CC = 1, no match AND (zs = 1) AND (zero byte in source VRF)
      // CC = 2, full match
      // CC = 3, partial match but no full match.
      TR::LabelSymbol* labelS2HeadFullMatch = generateLabelSymbol(cg);
      TR::LabelSymbol* labelS2HeadPartMatch = generateLabelSymbol(cg);

      generateVRRdInstruction(cg, TR::InstOpCode::VSTRS, node, tmpVReg, s1PartialVReg, s2HeadVReg, s2LenVReg, 0, elementSizeMask);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC2, node, labelS2HeadFullMatch);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC3, node, labelS2HeadPartMatch);

      // s2 header not found in s1
      // Load the next 16 bytes of s1 and continue
      generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, s1VecStartIndexReg, loadLenReg);
      generateRIEInstruction(cg, TR::InstOpCode::getCmpRegAndBranchRelOpCode(), node, s1VecStartIndexReg, maxIndexReg, labelStringNotFound, TR::InstOpCode::COND_BHR);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelFindS2Head);

      // s2 header full match
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelS2HeadFullMatch);

      // If s2Len <= 16 then we are done, otherwise we continue to check the rest of s2. We first handle residue bytes
      // of s2, then handle the rest 16-byte chunks.
      generateVRScInstruction(cg, TR::InstOpCode::VLGV, node, char1IndexReg, tmpVReg, generateS390MemoryReference(7, cg), 0);
      generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, char1IndexReg, s1VecStartIndexReg);
      generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, s2LenReg, (int8_t)vectorSize, labelLoadResult, TR::InstOpCode::COND_BNH);
      generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, s1VecStartIndexReg, loadLenReg);
      generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, s2VecStartIndexReg, vectorSize);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelMatchS2Residue);

      // s2 header partial match
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelS2HeadPartMatch);

      // Starting from the beginning of the partial match, load the next 16 bytes from s1 and redo s2 header search.
      // This implies that the partial match will be re-matched by the next VSTRS. This can potentially benefit string
      // search cases where s2 is shorter than 16 bytes. For short s2 strings, string search can potentially be done in
      // the next VSTRS and can we avoid residue matching which requires several index adjustments that do not provide
      // performance benefits.
      generateVRScInstruction(cg, TR::InstOpCode::VLGV, node, char1IndexReg, tmpVReg, generateS390MemoryReference(7, cg), 0);
      generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, s1VecStartIndexReg, char1IndexReg);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelFindS2Head);

      cg->stopUsingRegister(s2LenVReg);
      }
   else
      {
      generateVRXInstruction(cg, TR::InstOpCode::VLREP, node, s2Char1RepVReg, generateS390MemoryReference(s2ValueReg, headerSize, cg), elementSizeMask);

      /************************************** 1st char of s2 ******************************************/
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelFindS2Head);

      // Determine s1 load length. loadLenReg is either vectorSize-1 (15) or the 1st_char_matching residue length.
      generateRIEInstruction(cg, TR::InstOpCode::getAddHalfWordImmDistinctOperandOpCode(), node, loadLenReg, s1VecStartIndexReg, vectorSize);
      generateRIEInstruction(cg, TR::InstOpCode::getCmpRegAndBranchRelOpCode(), node, loadLenReg, s1LenReg, labelLoadLen16, TR::InstOpCode::COND_BNHR);
      generateRRRInstruction(cg, TR::InstOpCode::getSubtractThreeRegOpCode(), node, loadLenReg, s1LenReg, s1VecStartIndexReg);
      generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, loadLenReg, -1);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelLoadLenDone);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelLoadLen16);
      generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, loadLenReg, vectorSize-1);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelLoadLenDone);

      TR::Register* tmpReg = char1IndexReg;
      TR::LabelSymbol* labelExtractFirstCharPos = generateLabelSymbol(cg);

      generateRRRInstruction(cg, TR::InstOpCode::getAddThreeRegOpCode(), node, tmpReg, s1ValueReg, s1VecStartIndexReg);
      generateVRSbInstruction(cg, TR::InstOpCode::VLL, node, s1PartialVReg, loadLenReg, generateS390MemoryReference(tmpReg, headerSize, cg));
      generateVRRbInstruction(cg, TR::InstOpCode::VFEE, node, tmpVReg, s1PartialVReg, s2Char1RepVReg, 0x1, elementSizeMask);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, node, labelExtractFirstCharPos);

      // 1st char not found. Loop back and retry from the next chunk
      generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, s1VecStartIndexReg, loadLenReg);
      generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, s1VecStartIndexReg, 1);
      generateRIEInstruction(cg, TR::InstOpCode::getCmpRegAndBranchRelOpCode(), node, s1VecStartIndexReg, maxIndexReg, labelStringNotFound, TR::InstOpCode::COND_BHR);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelFindS2Head);

      // Found 1st char. check it's byte index in tmpVReg byte 7.
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelExtractFirstCharPos);

      generateVRScInstruction(cg, TR::InstOpCode::VLGV, node, char1IndexReg, tmpVReg, generateS390MemoryReference(7, cg), 0);
      generateRIEInstruction(cg, TR::InstOpCode::getCmpRegAndBranchRelOpCode(), node, char1IndexReg, loadLenReg, labelStringNotFound, TR::InstOpCode::COND_BNLR);

      generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, char1IndexReg, s1VecStartIndexReg);
      generateRIEInstruction(cg, TR::InstOpCode::getCmpRegAndBranchRelOpCode(), node, char1IndexReg, maxIndexReg, labelStringNotFound, TR::InstOpCode::COND_BHR);

      /************************************** s2 Residue matching ******************************************/
      generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, s1VecStartIndexReg, char1IndexReg);
      }

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelMatchS2Residue);

   // s2 residue length  = s2LenReg mod 16
   generateRRInstruction(cg, TR::InstOpCode::LLGHR, node, loadLenReg, s2LenReg);
   generateRIInstruction(cg, TR::InstOpCode::NILL, node, loadLenReg, 0x000F);
   generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, loadLenReg, (int8_t)0, labelMatchS2LoopSetup, TR::InstOpCode::COND_BE);

   TR::Register* tmpReg = s2VecStartIndexReg;
   generateRRRInstruction(cg, TR::InstOpCode::getAddThreeRegOpCode(), node, tmpReg, s1ValueReg, s1VecStartIndexReg);

   // Vector loads use load index. And [load_index = load_len - 1]
   generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, loadLenReg, -1);
   generateVRSbInstruction(cg, TR::InstOpCode::VLL, node, s1PartialVReg, loadLenReg, generateS390MemoryReference(tmpReg, headerSize, cg));

   // If VSTRS is supported, the first VSTRS already handled the 1st 16 bytes at this point (full match in the 1st 16
   // bytes). Hence, residue offset starts at 16.
   uint32_t s2ResidueDisp = headerSize + (supportsVSTRS ? vectorSize : 0);

   generateVRSbInstruction(cg, TR::InstOpCode::VLL, node, s2PartialVReg, loadLenReg, generateS390MemoryReference(s2ValueReg, s2ResidueDisp, cg));
   generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, loadLenReg, 1);

   if (supportsVSTRS)
      {
      generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, s2VecStartIndexReg, vectorSize);
      }

   generateVRRbInstruction(cg, TR::InstOpCode::VCEQ, node, tmpVReg, s1PartialVReg, s2PartialVReg, 1, elementSizeMask);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC0, node, labelMatchS2LoopSetup);

   // The residue does not match. Continue to find the 1st char in s1, starting from the next element.
   generateRIEInstruction(cg, TR::InstOpCode::getAddHalfWordImmDistinctOperandOpCode(), node, s1VecStartIndexReg, char1IndexReg, isUTF16 ? 2 : 1);
   generateRIEInstruction(cg, TR::InstOpCode::getCmpRegAndBranchRelOpCode(), node, s1VecStartIndexReg, maxIndexReg, labelStringNotFound, TR::InstOpCode::COND_BHR);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelFindS2Head);

   /************************************** s2 matching loop ENTRY ******************************************/

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelMatchS2LoopSetup);
   generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, s1VecStartIndexReg, loadLenReg);

   if (supportsVSTRS)
      {
      generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, s2VecStartIndexReg, loadLenReg);
      }
   else
      {
      generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, s2VecStartIndexReg, loadLenReg);
      }

   TR::Register* loopCountReg = loadLenReg;
   generateRSInstruction(cg, TR::InstOpCode::SRLK, node, loopCountReg, s2LenReg, 4);

   if (supportsVSTRS)
      {
      generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, loopCountReg, -1);
      }

   generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, loopCountReg, static_cast<int8_t>(0), labelLoadResult, TR::InstOpCode::COND_BE);

   /************************************** s2 matching loop ******************************************/
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelMatchS2Loop);

   // Start to match the reset of s2
   generateVRXInstruction(cg, TR::InstOpCode::VL, node, s1PartialVReg, generateS390MemoryReference(s1ValueReg, s1VecStartIndexReg, headerSize, cg));
   generateVRXInstruction(cg, TR::InstOpCode::VL, node, s2PartialVReg, generateS390MemoryReference(s2ValueReg, s2VecStartIndexReg, headerSize, cg));

   generateVRRbInstruction(cg, TR::InstOpCode::VCEQ, node, tmpVReg, s1PartialVReg, s2PartialVReg, 1, elementSizeMask);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC0, node, labelS2PartialMatch);

   // s2 chunk does not match. Go back to search for 1st char again.
   generateRIEInstruction(cg, TR::InstOpCode::getAddHalfWordImmDistinctOperandOpCode(), node, s1VecStartIndexReg, char1IndexReg, isUTF16 ? 2 : 1);
   generateRIEInstruction(cg, TR::InstOpCode::getCmpRegAndBranchRelOpCode(), node, s1VecStartIndexReg, maxIndexReg, labelStringNotFound, TR::InstOpCode::COND_BHR);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelFindS2Head);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelS2PartialMatch);
   generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, s1VecStartIndexReg, vectorSize);
   generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, s2VecStartIndexReg, vectorSize);
   generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, loopCountReg, -1);
   generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, loopCountReg, (int8_t)0, labelMatchS2Loop, TR::InstOpCode::COND_BNE);

   // Load -1 if s2 is no found in s1 or load the character index of the 1st character of s2 in s1
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelLoadResult);

   if (isUTF16)
      {
      // Byte-index to char-index conversion
      generateRSInstruction(cg, TR::InstOpCode::SRA, node, resultReg, 1);
      }

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelResultDone);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelStringNotFound);
   generateRIInstruction(cg, TR::InstOpCode::LHI, node, resultReg, -1);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelResultDone);

   // Result debug counters
   if (enableIndexOfDebugCounter)
      {
      TR::LabelSymbol* labelResultGT10 = generateLabelSymbol(cg);
      TR::LabelSymbol* labelResultGT30 = generateLabelSymbol(cg);
      TR::LabelSymbol* labelResultGT60 = generateLabelSymbol(cg);
      TR::LabelSymbol* labelResultGT100 = generateLabelSymbol(cg);
      TR::LabelSymbol* labelResultCheckDone = generateLabelSymbol(cg);

      uint8_t boundary10Char = 10;
      uint8_t boundary30Char = 30;
      uint8_t boundary60Char = 60;
      uint8_t boundary100Char = 100;

      generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, s2LenReg, boundary10Char, labelResultGT10, TR::InstOpCode::COND_BH);
      cg->generateDebugCounter("indexOfString/result/below-10", 1, TR::DebugCounter::Cheap);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelResultCheckDone);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node,  labelResultGT10);
      generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, s2LenReg, boundary30Char, labelResultGT30, TR::InstOpCode::COND_BH);
      cg->generateDebugCounter("indexOfString/result/10-30", 1, TR::DebugCounter::Cheap);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelResultCheckDone);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node,  labelResultGT30);
      generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, s2LenReg, boundary60Char, labelResultGT60, TR::InstOpCode::COND_BH);
      cg->generateDebugCounter("indexOfString/result/30-60", 1, TR::DebugCounter::Cheap);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelResultCheckDone);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node,   labelResultGT60);
      generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, s2LenReg, boundary100Char, labelResultGT100, TR::InstOpCode::COND_BH);
      cg->generateDebugCounter("indexOfString/result/60-100", 1, TR::DebugCounter::Cheap);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelResultCheckDone);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelResultGT100);
      cg->generateDebugCounter("indexOfString/result/above-100", 1, TR::DebugCounter::Cheap);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelResultCheckDone);
      }

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelDone, regDeps);
   labelDone->setEndInternalControlFlow();

   node->setRegister(resultReg);

   for (int32_t i = 0; i < node->getNumChildren(); ++i)
      {
      cg->decReferenceCount(node->getChild(i));
      }

   cg->stopUsingRegister(maxIndexReg);
   cg->stopUsingRegister(s2VecStartIndexReg);
   cg->stopUsingRegister(loadLenReg);

   cg->stopUsingRegister(s1PartialVReg);
   cg->stopUsingRegister(s2PartialVReg);
   cg->stopUsingRegister(s2Char1RepVReg);
   cg->stopUsingRegister(tmpVReg);

   return resultReg;
   }


  /** \brief
   *     Attempts to use vector registers to perform SIMD conversion of characters from lowercase to uppercase.
   *
   *  \detail
   *     Uses vector registers to convert 16 bytes at a time.
   *
   *  \param node
   *     The node representing the HW optimized toUpper and toLower recognized calls.
   *
   *  \param cg
   *     The code generator used to generate the instructions.
   *
   *  \param isToUpper
   *     Boolean representing case conversion, either to upper or to lower.
   *
   *  \param isCompressedString
   *     Boolean representing the string's compression.
   *
   *  \return
   *     A register containing the return value of the Java call. The return value
   *     will be 1 if the entire contents of the input array was translated and 0 if
   *     we were unable to translate the entire contents of the array (up to the specified length).
   */
TR::Register * caseConversionHelper(TR::Node* node, TR::CodeGenerator* cg, bool isToUpper, bool isCompressedString)
   {
   TR::Register* sourceRegister = cg->evaluate(node->getChild(1));
   TR::Register* destRegister = cg->evaluate(node->getChild(2));
   TR::Register* lengthRegister = cg->gprClobberEvaluate(node->getChild(3));

   TR::Register* addressOffset = cg->allocateRegister();
   TR::Register* loadLength = cg->allocateRegister();

   // Loopcounter register for number of 16 byte conversions, when it is used, the length is not needed anymore
   TR::Register* loopCounter = lengthRegister;

   TR::Register* charBufferVector = cg->allocateRegister(TR_VRF);
   TR::Register* selectionVector = cg->allocateRegister(TR_VRF);
   TR::Register* modifiedCaseVector = cg->allocateRegister(TR_VRF);
   TR::Register* charOffsetVector = cg->allocateRegister(TR_VRF);
   TR::Register* alphaRangeVector = cg->allocateRegister(TR_VRF);
   TR::Register* alphaCondVector = cg->allocateRegister(TR_VRF);
   TR::Register* invalidRangeVector = cg->allocateRegister(TR_VRF);
   TR::Register* invalidCondVector = cg->allocateRegister(TR_VRF);

   TR::LabelSymbol* cFlowRegionStart = generateLabelSymbol( cg);
   TR::LabelSymbol* fullVectorConversion = generateLabelSymbol( cg);
   TR::LabelSymbol* cFlowRegionEnd = generateLabelSymbol( cg);
   TR::LabelSymbol* success = generateLabelSymbol( cg);
   TR::LabelSymbol* handleInvalidChars = generateLabelSymbol( cg);
   TR::LabelSymbol* loop = generateLabelSymbol( cg);

   TR::Instruction* cursor;

   const int elementSizeMask = (isCompressedString) ? 0x0 : 0x1;    // byte or halfword mask
   const int8_t sizeOfVector = cg->machine()->getVRFSize();
   const bool is64 = TR::Compiler->target.is64Bit();
   uintptrj_t headerSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();

   TR::RegisterDependencyConditions * regDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 13, cg);
   regDeps->addPostCondition(sourceRegister, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(destRegister, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(lengthRegister, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(addressOffset, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(loadLength, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(charBufferVector, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(selectionVector, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(modifiedCaseVector, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(charOffsetVector, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(alphaRangeVector, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(alphaCondVector, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(invalidRangeVector, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(invalidCondVector, TR::RealRegister::AssignAny);

   generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, addressOffset, addressOffset);

   generateVRIaInstruction(cg, TR::InstOpCode::VGBM, node, alphaRangeVector, 0, 0);
   generateVRIaInstruction(cg, TR::InstOpCode::VGBM, node, alphaCondVector, 0, 0);
   generateVRIaInstruction(cg, TR::InstOpCode::VGBM, node, invalidRangeVector, 0, 0);
   generateVRIaInstruction(cg, TR::InstOpCode::VGBM, node, invalidCondVector, 0, 0);

   // Characters a-z (0x61-0x7A) when to upper and A-Z (0x41-0x5A) when to lower
   generateVRIaInstruction (cg, TR::InstOpCode::VLEIH, node, alphaRangeVector, isToUpper ? 0x617A : 0x415A, 0x0);
   // Characters àáâãäåæçèéêëìíîïðñòóôõö (0xE0-0xF6) when to upper and ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ (0xC0-0xD6) when to lower
   generateVRIaInstruction (cg, TR::InstOpCode::VLEIH, node, alphaRangeVector, isToUpper ? 0xE0F6 : 0xC0D6, 0x1);
   // Characters øùúûüýþ (0xF8-0xFE) when to upper and ØÙÚÛÜÝÞ (0xD8-0xDE) when to lower
   generateVRIaInstruction (cg, TR::InstOpCode::VLEIH, node, alphaRangeVector, isToUpper ? 0xF8FE : 0xD8DE, 0X2);

   if (!isCompressedString)
      {
      generateVRRaInstruction(cg, TR::InstOpCode::VUPLH, node, alphaRangeVector, alphaRangeVector, 0, 0, 0, 0);
      }

   // Condition codes for >= (bits 0 and 2) and <= (bits 0 and 1)
   if (isCompressedString)
      {
      generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, alphaCondVector, 0XA0C0, 0X0);
      generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, alphaCondVector, 0XA0C0, 0X1);
      generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, alphaCondVector, 0XA0C0, 0X2);
      }
   else
      {
      generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, alphaCondVector, 0XA000, 0X0);
      generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, alphaCondVector, 0XC000, 0X1);
      generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, alphaCondVector, 0XA000, 0X2);
      generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, alphaCondVector, 0XC000, 0X3);
      generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, alphaCondVector, 0XA000, 0X4);
      generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, alphaCondVector, 0XC000, 0X5);
      }

   if (isToUpper)
      {
      // Can't uppercase \u00DF (capital sharp s) nor \u00B5 (mu) with a simple addition of 0x20 so we do an equality
      // comparison (bit 0) and greater than or equal comparison (bits 0 and 2) for codes larger than or equal to 0xFF
      generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, invalidRangeVector, 0xDFDF, 0x0);
      generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, invalidRangeVector, 0xB5B5, 0x1);
      generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, invalidRangeVector, 0xFFFF, 0x2);

      if (isCompressedString)
         {
         generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, invalidCondVector, 0x8080, 0x0);
         generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, invalidCondVector, 0x8080, 0x1);
         generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, invalidCondVector, 0xA0A0, 0x2);
         }
      else
         {
         generateVRRaInstruction(cg, TR::InstOpCode::VUPLH, node, invalidRangeVector, invalidRangeVector, 0, 0, 0, 0);

         generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, invalidCondVector, 0x8000, 0x0);
         generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, invalidCondVector, 0x8000, 0x1);
         generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, invalidCondVector, 0x8000, 0x2);
         generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, invalidCondVector, 0x8000, 0x3);
         generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, invalidCondVector, 0xA000, 0x4);
         generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, invalidCondVector, 0xA000, 0x5);
         }
      }
   else if (!isCompressedString)
      {
      // Can't lowercase codes larger than 0xFF but we only need to check this if our input is not compressed since
      // all compressed values will be <= 0xFF
      generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, invalidRangeVector, 0x00FF, 0x0);
      generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, invalidRangeVector, 0x00FF, 0x1);

      generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, invalidCondVector, 0x2000, 0x0);
      generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, invalidCondVector, 0x2000, 0x1);
      }

   // Constant value of 0x20, used to convert between upper and lower
   generateVRIaInstruction(cg, TR::InstOpCode::VREPI, node, charOffsetVector, 0x20, elementSizeMask);

   generateRRInstruction(cg, TR::InstOpCode::LR, node, loadLength, lengthRegister);
   generateRILInstruction(cg, TR::InstOpCode::NILF, node, loadLength, 0xF);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
   cFlowRegionStart->setStartInternalControlFlow();
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZ, node, fullVectorConversion);

   // VLL and VSTL take an index, not a count, so subtract the input length by 1
   generateRILInstruction(cg, TR::InstOpCode::SLFI, node, loadLength, 1);

   generateVRSbInstruction(cg, TR::InstOpCode::VLL, node, charBufferVector, loadLength, generateS390MemoryReference(sourceRegister, headerSize, cg));

   // Check for invalid characters, go to fallback individual character conversion implementation
   if (isToUpper || !isCompressedString)
      {
      generateVRRdInstruction(cg, TR::InstOpCode::VSTRC, node, selectionVector, charBufferVector, invalidRangeVector, invalidCondVector, 0x1 , elementSizeMask);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, node, handleInvalidChars);
      }

   generateVRRdInstruction(cg, TR::InstOpCode::VSTRC, node, selectionVector, charBufferVector, alphaRangeVector, alphaCondVector, 0x4, elementSizeMask);
   generateVRRcInstruction(cg, isToUpper ? TR::InstOpCode::VS : TR::InstOpCode::VA, node, modifiedCaseVector, charBufferVector, charOffsetVector, 0x0, 0x0, elementSizeMask);
   generateVRReInstruction(cg, TR::InstOpCode::VSEL, node, modifiedCaseVector, modifiedCaseVector, charBufferVector, selectionVector);

   generateVRSbInstruction(cg, TR::InstOpCode::VSTL, node, modifiedCaseVector, loadLength, generateS390MemoryReference(destRegister, headerSize, cg), 0);

   // Increment index by the remainder then add 1, since the loadLength contains the highest index, we must go one past that
   generateRIEInstruction(cg, TR::InstOpCode::AHIK, node, addressOffset, loadLength, 1);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, fullVectorConversion);

   generateRIEInstruction(cg, TR::InstOpCode::CIJ, node, lengthRegister, sizeOfVector, success, TR::InstOpCode::COND_BL);

   // Set the loopCounter to the amount of groups of 16 bytes left, ignoring already accounted for remainder
   generateRSInstruction(cg, TR::InstOpCode::SRL, node, loopCounter, loopCounter, 4);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, loop);

   generateVRXInstruction(cg, TR::InstOpCode::VL, node, charBufferVector, generateS390MemoryReference(sourceRegister, addressOffset, headerSize, cg));

   if (isToUpper || !isCompressedString)
      {
      generateVRRdInstruction(cg, TR::InstOpCode::VSTRC, node, selectionVector, charBufferVector, invalidRangeVector, invalidCondVector, 0x1 , elementSizeMask);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, node, handleInvalidChars);
      }

   generateVRRdInstruction(cg, TR::InstOpCode::VSTRC, node, selectionVector, charBufferVector, alphaRangeVector, alphaCondVector, 0x4, elementSizeMask);
   generateVRRcInstruction(cg, isToUpper ? TR::InstOpCode::VS : TR::InstOpCode::VA, node, modifiedCaseVector, charBufferVector, charOffsetVector, 0x0, 0x0, elementSizeMask);
   generateVRReInstruction(cg, TR::InstOpCode::VSEL, node, modifiedCaseVector, modifiedCaseVector, charBufferVector, selectionVector);

   generateVRXInstruction(cg, TR::InstOpCode::VST, node, modifiedCaseVector, generateS390MemoryReference(destRegister, addressOffset, headerSize, cg), 0);

   generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, addressOffset, generateS390MemoryReference(addressOffset, sizeOfVector, cg));
   generateS390BranchInstruction(cg, TR::InstOpCode::BRCT, node, loopCounter, loop);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, success);

   generateRIInstruction(cg, TR::InstOpCode::LHI, node, lengthRegister, 1);

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cFlowRegionEnd);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, handleInvalidChars);
   cg->generateDebugCounter(isToUpper? "z13/simd/toUpper/null"  : "z13/simd/toLower/null", 1, TR::DebugCounter::Cheap);
   generateRRInstruction(cg, TR::InstOpCode::XR, node, lengthRegister, lengthRegister);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, regDeps);
   cFlowRegionEnd->setEndInternalControlFlow();

   cg->stopUsingRegister(addressOffset);
   cg->stopUsingRegister(loadLength);

   cg->stopUsingRegister(charBufferVector);
   cg->stopUsingRegister(selectionVector);
   cg->stopUsingRegister(modifiedCaseVector);
   cg->stopUsingRegister(charOffsetVector);
   cg->stopUsingRegister(alphaRangeVector);
   cg->stopUsingRegister(alphaCondVector);
   cg->stopUsingRegister(invalidRangeVector);
   cg->stopUsingRegister(invalidCondVector);

   node->setRegister(lengthRegister);

   cg->decReferenceCount(node->getChild(0));
   cg->decReferenceCount(node->getChild(1));
   cg->decReferenceCount(node->getChild(2));
   cg->decReferenceCount(node->getChild(3));

   return node->getRegister();
   }

extern TR::Register *
intrinsicIndexOf(TR::Node * node, TR::CodeGenerator * cg, bool isCompressed)
   {
   cg->generateDebugCounter("z13/simd/indexOf", 1, TR::DebugCounter::Free);

   TR::Register* address = cg->evaluate(node->getChild(1));
   TR::Register* value = cg->evaluate(node->getChild(2));
   TR::Register* index = cg->evaluate(node->getChild(3));
   TR::Register* size = cg->gprClobberEvaluate(node->getChild(4));

   // load length isn't used after loop, size must is adjusted to become bytes left
   TR::Register* loopCounter = size;
   TR::Register* loadLength = cg->allocateRegister();
   TR::Register* indexRegister = cg->allocateRegister();
   TR::Register* offsetAddress = cg->allocateRegister();
   TR::Register* scratch = offsetAddress;

   TR::Register* charBufferVector = cg->allocateRegister(TR_VRF);
   TR::Register* resultVector = cg->allocateRegister(TR_VRF);
   TR::Register* valueVector = cg->allocateRegister(TR_VRF);

   TR::LabelSymbol* cFlowRegionStart = generateLabelSymbol( cg);
   TR::LabelSymbol* loopLabel = generateLabelSymbol( cg);
   TR::LabelSymbol* fullVectorLabel = generateLabelSymbol( cg);
   TR::LabelSymbol* notFoundInResidue = generateLabelSymbol( cg);
   TR::LabelSymbol* foundLabel = generateLabelSymbol( cg);
   TR::LabelSymbol* foundLabelExtractedScratch = generateLabelSymbol( cg);
   TR::LabelSymbol* failureLabel = generateLabelSymbol( cg);
   TR::LabelSymbol* cFlowRegionEnd = generateLabelSymbol( cg);

   const int elementSizeMask = isCompressed ? 0x0 : 0x1;   // byte or halfword mask
   uintptrj_t headerSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
   const int8_t sizeOfVector = cg->machine()->getVRFSize();
   const bool is64 = TR::Compiler->target.is64Bit();

   TR::RegisterDependencyConditions * regDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 8, cg);
   regDeps->addPostCondition(address, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(loopCounter, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(indexRegister, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(loadLength, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(offsetAddress, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(charBufferVector, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(resultVector, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(valueVector, TR::RealRegister::AssignAny);

   generateVRRfInstruction(cg, TR::InstOpCode::VLVGP, node, valueVector, index, value);
   generateVRIcInstruction(cg, TR::InstOpCode::VREP, node, valueVector, valueVector, (sizeOfVector / (1 << elementSizeMask)) - 1, elementSizeMask);
   generateVRIaInstruction(cg, TR::InstOpCode::VGBM, node, resultVector, 0, 0);
   generateVRIaInstruction(cg, TR::InstOpCode::VGBM, node, charBufferVector, 0, 0);

   if (is64)
      {
      generateRREInstruction(cg, TR::InstOpCode::LLGFR, node, indexRegister, index);
      }
   else
      {
      generateRRInstruction(cg, TR::InstOpCode::LR, node, indexRegister, index);
      }
   generateRRInstruction(cg, TR::InstOpCode::SR, node, size, index);

   if (!isCompressed)
      {
      generateRSInstruction(cg, TR::InstOpCode::SLL, node, size, 1);
      generateRSInstruction(cg, TR::InstOpCode::SLL, node, indexRegister, 1);
      }

   generateRRInstruction(cg, TR::InstOpCode::LR, node, loadLength, size);
   generateRILInstruction(cg, TR::InstOpCode::NILF, node, loadLength, 0xF);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
   cFlowRegionStart->setStartInternalControlFlow();
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZ, node, fullVectorLabel);

   // VLL takes an index, not a count, so subtract 1 from the count
   generateRILInstruction(cg, TR::InstOpCode::SLFI, node, loadLength, 1);

   generateRXInstruction(cg, TR::InstOpCode::LA, node, offsetAddress, generateS390MemoryReference(address, indexRegister, headerSize, cg));
   generateVRSbInstruction(cg, TR::InstOpCode::VLL, node, charBufferVector, loadLength, generateS390MemoryReference(offsetAddress, 0, cg));

   generateVRRbInstruction(cg, TR::InstOpCode::VFEE, node, resultVector, charBufferVector, valueVector, 0x1, elementSizeMask);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK1, node, notFoundInResidue);

   generateVRScInstruction(cg, TR::InstOpCode::VLGV, node, scratch, resultVector, generateS390MemoryReference(7, cg), 0);
   generateRIEInstruction(cg, TR::InstOpCode::CRJ, node, scratch, loadLength, foundLabelExtractedScratch, TR::InstOpCode::COND_BNH);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, notFoundInResidue);

   // Increment index by loaded length + 1, since we subtracted 1 earlier
   generateRIEInstruction(cg, TR::InstOpCode::AHIK, node, loadLength, loadLength, 1);
   generateRRInstruction(cg, TR::InstOpCode::AR, node, indexRegister, loadLength);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, fullVectorLabel);

   generateRIEInstruction(cg, TR::InstOpCode::CIJ, node, size, sizeOfVector, failureLabel, TR::InstOpCode::COND_BL);

   // Set loopcounter to 1/16 of the length, remainder has already been accounted for
   generateRSInstruction(cg, TR::InstOpCode::SRL, node, loopCounter, loopCounter, 4);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, loopLabel);

   generateVRXInstruction(cg, TR::InstOpCode::VL, node, charBufferVector, generateS390MemoryReference(address, indexRegister, headerSize, cg));

   generateVRRbInstruction(cg, TR::InstOpCode::VFEE, node, resultVector, charBufferVector, valueVector, 0x1, elementSizeMask);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK4, node, foundLabel);

   generateRILInstruction(cg, TR::InstOpCode::AFI, node, indexRegister, sizeOfVector);

   generateS390BranchInstruction(cg, TR::InstOpCode::BRCT, node, loopCounter, loopLabel);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, failureLabel);
   generateRIInstruction(cg, TR::InstOpCode::LHI, node, indexRegister, 0xFFFF);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_B, node, cFlowRegionEnd);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, foundLabel);
   generateVRScInstruction(cg, TR::InstOpCode::VLGV, node, scratch, resultVector, generateS390MemoryReference(7, cg), 0);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, foundLabelExtractedScratch);
   generateRRInstruction(cg, TR::InstOpCode::AR, node, indexRegister, scratch);

   if (!isCompressed)
      {
      generateRSInstruction(cg, TR::InstOpCode::SRL, node, indexRegister, indexRegister, 1);
      }

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, regDeps);
   cFlowRegionEnd->setEndInternalControlFlow();

   cg->stopUsingRegister(loopCounter);
   cg->stopUsingRegister(loadLength);
   cg->stopUsingRegister(offsetAddress);

   cg->stopUsingRegister(charBufferVector);
   cg->stopUsingRegister(resultVector);
   cg->stopUsingRegister(valueVector);

   node->setRegister(indexRegister);
   cg->recursivelyDecReferenceCount(node->getChild(0));
   cg->decReferenceCount(node->getChild(1));
   cg->decReferenceCount(node->getChild(2));
   cg->decReferenceCount(node->getChild(3));
   cg->decReferenceCount(node->getChild(4));
   return indexRegister;
   }

extern TR::Register *
toUpperIntrinsic(TR::Node *node, TR::CodeGenerator *cg, bool isCompressedString)
   {
   cg->generateDebugCounter("z13/simd/toUpper", 1, TR::DebugCounter::Free);
   return caseConversionHelper(node, cg, true, isCompressedString);
   }

extern TR::Register *
toLowerIntrinsic(TR::Node *node, TR::CodeGenerator *cg, bool isCompressedString)
   {
   cg->generateDebugCounter("z13/simd/toLower", 1, TR::DebugCounter::Free);
   return caseConversionHelper(node, cg, false, isCompressedString);
   }

extern TR::Register *
inlineDoubleMax(TR::Node *node, TR::CodeGenerator *cg)
   {
   cg->generateDebugCounter("z13/simd/doubleMax", 1, TR::DebugCounter::Free);
   return doubleMaxMinHelper(node, cg, true);
   }

extern TR::Register *
inlineDoubleMin(TR::Node *node, TR::CodeGenerator *cg)
   {
   cg->generateDebugCounter("z13/simd/doubleMin", 1, TR::DebugCounter::Free);
   return doubleMaxMinHelper(node, cg, false);
   }


/*
 * J9 S390 specific tree evaluator table overrides
 */
extern void TEMPORARY_initJ9S390TreeEvaluatorTable(TR::CodeGenerator *cg)
   {
   TR_TreeEvaluatorFunctionPointer *tet = cg->getTreeEvaluatorTable();

   tet[TR::monent] =                TR::TreeEvaluator::monentEvaluator;
   tet[TR::monexit] =               TR::TreeEvaluator::monexitEvaluator;
   tet[TR::monexitfence] =          TR::TreeEvaluator::monexitfenceEvaluator;
   tet[TR::asynccheck] =            TR::TreeEvaluator::asynccheckEvaluator;
   tet[TR::instanceof] =            TR::TreeEvaluator::instanceofEvaluator;
   tet[TR::checkcast] =             TR::TreeEvaluator::checkcastEvaluator;
   tet[TR::checkcastAndNULLCHK] =   TR::TreeEvaluator::checkcastAndNULLCHKEvaluator;
   tet[TR::New] =                   TR::TreeEvaluator::newObjectEvaluator;
   tet[TR::variableNew] =           TR::TreeEvaluator::newObjectEvaluator;
   tet[TR::newarray] =              TR::TreeEvaluator::newArrayEvaluator;
   tet[TR::anewarray] =             TR::TreeEvaluator::anewArrayEvaluator;
   tet[TR::variableNewArray] =      TR::TreeEvaluator::anewArrayEvaluator;
   tet[TR::multianewarray] =        TR::TreeEvaluator::multianewArrayEvaluator;
   tet[TR::arraylength] =           TR::TreeEvaluator::arraylengthEvaluator;
   tet[TR::ResolveCHK] =            TR::TreeEvaluator::resolveCHKEvaluator;
   tet[TR::DIVCHK] =                TR::TreeEvaluator::DIVCHKEvaluator;
   tet[TR::BNDCHK] =                TR::TreeEvaluator::BNDCHKEvaluator;
   tet[TR::ArrayCopyBNDCHK] =       TR::TreeEvaluator::ArrayCopyBNDCHKEvaluator;
   tet[TR::BNDCHKwithSpineCHK] =    TR::TreeEvaluator::BNDCHKwithSpineCHKEvaluator;
   tet[TR::SpineCHK] =              TR::TreeEvaluator::BNDCHKwithSpineCHKEvaluator;
   tet[TR::ArrayStoreCHK] =         TR::TreeEvaluator::ArrayStoreCHKEvaluator;
   tet[TR::ArrayCHK] =              TR::TreeEvaluator::ArrayCHKEvaluator;
   tet[TR::MethodEnterHook] =       TR::TreeEvaluator::conditionalHelperEvaluator;
   tet[TR::MethodExitHook] =        TR::TreeEvaluator::conditionalHelperEvaluator;

   tet[TR::tstart] = TR::TreeEvaluator::tstartEvaluator;
   tet[TR::tfinish] = TR::TreeEvaluator::tfinishEvaluator;
   tet[TR::tabort] = TR::TreeEvaluator::tabortEvaluator;

   }


TR::Instruction *
J9::Z::TreeEvaluator::genLoadForObjectHeaders(TR::CodeGenerator *cg, TR::Node *node, TR::Register *reg, TR::MemoryReference *tempMR, TR::Instruction *iCursor)
   {
#if defined(OMR_GC_COMPRESSED_POINTERS)
   return generateRXInstruction(cg, TR::InstOpCode::LLGF, node, reg, tempMR, iCursor);
#else
   return generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, reg, tempMR, iCursor);
#endif
   }

TR::Instruction *
J9::Z::TreeEvaluator::genLoadForObjectHeadersMasked(TR::CodeGenerator *cg, TR::Node *node, TR::Register *reg, TR::MemoryReference *tempMR, TR::Instruction *iCursor)
   {
   // Bit-mask for masking J9Object header to extract J9Class
   uint16_t mask = 0xFF00;
   TR::Compilation *comp = cg->comp();
   TR::Instruction *loadInstr;

#if defined(OMR_GC_COMPRESSED_POINTERS)
   if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z13) &&
         !(comp->getOption(TR_DisableZ13) || comp->getOption(TR_DisableZ13LoadAndMask)))
      {
      iCursor = generateRXInstruction(cg, TR::InstOpCode::LLZRGF, node, reg, tempMR, iCursor);
      loadInstr = iCursor;
      cg->generateDebugCounter("z13/LoadAndMask", 1, TR::DebugCounter::Free);
      }
   else
      {
      // Zero out top 32 bits and load the unmasked J9Class
      iCursor = generateRXInstruction(cg, TR::InstOpCode::LLGF, node, reg, tempMR, iCursor);
      loadInstr = iCursor;
      // Now mask it to get the actual pointer
      iCursor = generateRIInstruction(cg, TR::InstOpCode::NILL, node, reg, mask, iCursor);
      }
#else
   if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z13))
      {
      iCursor = generateRXInstruction(cg, TR::InstOpCode::getLoadAndMaskOpCode(), node, reg, tempMR, iCursor);
      loadInstr = iCursor;
      cg->generateDebugCounter("z13/LoadAndMask", 1, TR::DebugCounter::Free);
      }
   else
      {
      iCursor = generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, reg, tempMR, iCursor);
      loadInstr = iCursor;
      iCursor = generateRIInstruction(cg, TR::InstOpCode::NILL,           node, reg, mask,   iCursor);
      }
#endif

   // The intended functionality of rdbar/wrtbar IL nodes is to first report to the VM that a field is being watched
   // (i.e. being read or being written to), and then perform the actual load/store operation. To achieve this, evaluators
   // for rdbar/wrtbar opcodes first call helper routines to generate code that will report to the VM that a field is being
   // read or written to. Following this, they will perform the actual load/store operation on the field.
   // The helper routines can call this routine in order to determine if fieldwatch is enabled
   // on a particular Java class. In those cases we may end up loading the Java class before the actual indirect load occurs
   // on the field. In general, if the object we are trying to load is null, an exception is thrown during the load.
   // To handle this we need to set an exception point and the GC Map for the VM. We must do the same here for rdbar/wrtbar for
   // the above explained reason.
   if (node->getOpCode().isReadBar() || node->getOpCode().isWrtBar())
      {
      cg->setImplicitExceptionPoint(loadInstr);
      loadInstr->setNeedsGCMap(0x0000FFFF);
      if (node->getOpCodeValue() == TR::checkcastAndNULLCHK)
         {
         loadInstr->setNode(cg->comp()->findNullChkInfo(node));
         }
      }
   return iCursor;
   }

// max number of cache slots used by checkcat/instanceof
#define NUM_PICS 3

static inline TR::Instruction *
genNullTest(TR::CodeGenerator * cg, TR::Node * node, TR::Register * tgtReg, TR::Register * srcReg, TR::Instruction * cursor)
   {
   TR::Instruction * iRet;

   static_assert(NULLVALUE == 0, "NULLVALUE is assumed to be zero here");
   iRet = generateRRInstruction(cg, TR::InstOpCode::getLoadTestRegOpCode(), node, tgtReg, srcReg, cursor);

   return iRet;
   }

static TR::Instruction *
genTestIsSuper(TR::CodeGenerator * cg, TR::Node * node,
   TR::Register * objClassReg, TR::Register * castClassReg,
   TR::Register * scratch1Reg, TR::Register * scratch2Reg, TR::Register * resultReg,
   TR::Register * litPoolBaseReg, int32_t castClassDepth,
   TR::LabelSymbol * failLabel, TR::LabelSymbol * trueLabel, TR::LabelSymbol * callHelperLabel,
   TR::RegisterDependencyConditions * conditions, TR::Instruction * cursor,
   bool addDataSnippetAsSecondaryCache,
   TR::Register * classObjectClazzSnippetReg,
   TR::Register * instanceOfClazzSnippetReg
   )
   {
   TR::Compilation *comp = cg->comp();
   TR_Debug * debugObj = cg->getDebug();

   int32_t superClassOffset = castClassDepth * TR::Compiler->om.sizeofReferenceAddress();
   bool outOfBound = (superClassOffset > MAX_IMMEDIATE_VAL || superClassOffset < MIN_IMMEDIATE_VAL) ? true : false;
   // For the scenario where a call to Class.isInstance() is converted to instanceof,
   // we need to load the class depth at runtime because we don't have it at compile time
   bool dynamicCastClass = (castClassDepth == -1);
   bool eliminateSuperClassArraySizeCheck = (!dynamicCastClass && (castClassDepth < cg->comp()->getOptions()->_minimumSuperclassArraySize));


#ifdef OMR_GC_COMPRESSED_POINTERS
   // objClassReg contains the class offset, so we may need to
   // convert this offset to a real J9Class pointer
#endif
   if (dynamicCastClass)
      {
      TR::LabelSymbol * notInterfaceLabel = generateLabelSymbol(cg);
      TR_ASSERT((node->getOpCodeValue() == TR::instanceof &&
            node->getSecondChild()->getOpCodeValue() != TR::loadaddr), "genTestIsSuper: castClassDepth == -1 is only supported for transformed isInstance calls.");

      // check if cast class is an interface
      cursor = generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, scratch1Reg,
            generateS390MemoryReference(castClassReg, offsetof(J9Class, romClass), cg), cursor);

      cursor = generateRXInstruction(cg, TR::InstOpCode::L, node, scratch1Reg,
            generateS390MemoryReference(scratch1Reg, offsetof(J9ROMClass, modifiers), cg), cursor);


      TR_ASSERT(((J9AccInterface | J9AccClassArray) < UINT_MAX && (J9AccInterface | J9AccClassArray) > 0),
            "genTestIsSuper::(J9AccInterface | J9AccClassArray) is not a 32-bit number\n");

      cursor = generateRILInstruction(cg, TR::InstOpCode::NILF, node, scratch1Reg, static_cast<int32_t>((J9AccInterface | J9AccClassArray)), cursor);

      if (debugObj)
         debugObj->addInstructionComment(cursor, "Check if castClass is an interface or class array and jump to helper sequence");

      // insert snippet check
      if ( addDataSnippetAsSecondaryCache )
         {
        cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, notInterfaceLabel, cursor);
         // classObjectClazzSnippet and instanceOfClazzSnippet stores values of currentObject and cast Object when
        // the the helper call returns success.
         // test if class is interface of not.
         // if interface, we do the following.
         //
         // insert instanceof site snippet test
         // cmp objectClassReg, classObjectClazzSnippet
         // jne helper call
         // cmp castclassreg, instanceOfClazzSnippet
         // je true_label
         // jump to outlined label
         // test jitInstanceOf results
         // JE fail_label        // instanceof result is not true
         //
         // the following will be done at the end of instanceof evaluation when we do helperCall
         // cmp snippet1 with value -1
         // jne true_label         // snippet already updated
         // update classObjectClazzSnippet, instanceOfClazzSnippet with object class and instance of class
         // jmp true_label
         //NO need for cache test for z, if it is dynamic we will already have failed cache test if we got here.
         cursor = generateRXInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, objClassReg, generateS390MemoryReference(classObjectClazzSnippetReg,0,cg), cursor);
         cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, callHelperLabel, cursor);
         cursor = generateRXInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, castClassReg, generateS390MemoryReference(instanceOfClazzSnippetReg,0,cg), cursor);
         cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, trueLabel, cursor);
         cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, callHelperLabel, cursor);
         cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, notInterfaceLabel, cursor);
         }
      else
         {
        cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, callHelperLabel, cursor);
         }
      }


   TR::InstOpCode::Mnemonic loadOp;
   int32_t bytesOffset;

   if (TR::Compiler->target.is64Bit())
      {
      loadOp = TR::InstOpCode::LLGH;
      bytesOffset = 6;
      }
   else
      {
      loadOp = TR::InstOpCode::LLH;
      bytesOffset = 2;
      }

   if (dynamicCastClass)
      {
      cursor = generateRXInstruction(cg, loadOp, node, scratch2Reg,
            generateS390MemoryReference(castClassReg, offsetof(J9Class, classDepthAndFlags) + bytesOffset, cg), cursor);

      TR_ASSERT(sizeof(((J9Class*)0)->classDepthAndFlags) == sizeof(uintptr_t),
            "genTestIsSuper::J9Class->classDepthAndFlags is wrong size\n");
      }

   if (!eliminateSuperClassArraySizeCheck)
      {
      if (resultReg)
         {
         cursor = generateRIInstruction(cg, TR::InstOpCode::LHI, node, resultReg, 0, cursor);
         }

      cursor = generateRXInstruction(cg, loadOp, node, scratch1Reg,
            generateS390MemoryReference(objClassReg, offsetof(J9Class, classDepthAndFlags) + bytesOffset, cg) , cursor);
      TR_ASSERT(sizeof(((J9Class*)0)->classDepthAndFlags) == sizeof(uintptr_t),
                  "genTestIsSuper::J9Class->classDepthAndFlags is wrong size\n");

      bool generateCompareAndBranchIsPossible = false;

      if (dynamicCastClass)
         generateCompareAndBranchIsPossible = true;
      else if (outOfBound)
         {
         if (TR::Compiler->target.is64Bit())
            {
            cursor = genLoadLongConstant(cg, node, castClassDepth, scratch2Reg, cursor, conditions, litPoolBaseReg);
            }
         else
            {
            cursor = generateLoad32BitConstant(cg, node, castClassDepth, scratch2Reg, false, cursor, conditions, litPoolBaseReg);
            }
         generateCompareAndBranchIsPossible = true;
         }
      else
         {
         cursor = generateRIInstruction(cg, TR::InstOpCode::getCmpHalfWordImmOpCode(), node, scratch1Reg, castClassDepth, cursor);
         }

      if (generateCompareAndBranchIsPossible)
         cursor = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, scratch1Reg, scratch2Reg, TR::InstOpCode::COND_BNH, failLabel, false, false);
      else
         cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, failLabel, cursor);

      if (debugObj)
         debugObj->addInstructionComment(cursor, "Fail if depth(obj) > depth(castClass)");

      }

   if (resultReg)
      {
      cursor = generateRIInstruction(cg, TR::InstOpCode::LHI, node, resultReg, 1, cursor);
      }
#ifdef OMR_GC_COMPRESSED_POINTERS
   // objClassReg contains the class offset, so we may need to
   // convert this offset to a real J9Class pointer
#endif
   cursor = generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, scratch1Reg,
               generateS390MemoryReference(objClassReg, offsetof(J9Class, superclasses), cg), cursor);

   if (outOfBound || dynamicCastClass)
      {
      if (TR::Compiler->target.is64Bit())
         {
         cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, scratch2Reg, scratch2Reg, 3, cursor);
         }
      else
         {
         cursor = generateRSInstruction(cg, TR::InstOpCode::SLL, node, scratch2Reg, 2, cursor);
         }
#ifdef OMR_GC_COMPRESSED_POINTERS
      // castClassReg contains the class offset, but the memory reference below will
      // generate a J9Class pointer. We may need to convert this pointer to an offset
#endif
      cursor = generateRXInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, castClassReg,
                  generateS390MemoryReference(scratch1Reg, scratch2Reg, 0, cg), cursor);
      }
   else
      {
#ifdef OMR_GC_COMPRESSED_POINTERS
      // castClassReg contains the class offset, but the memory reference below will
      // generate a J9Class pointer. We may need to convert this pointer to an offset
#endif
      cursor = generateRXInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, castClassReg,
                  generateS390MemoryReference(scratch1Reg, superClassOffset, cg), cursor);
      }

   if (debugObj)
      debugObj->addInstructionComment(cursor, "Check if objClass is subclass of castClass");

   return cursor;
   }

// Checks for the scenario where a call to Class.isInstance() is converted to instanceof,
// and we need to load the j9class of the cast class at runtime because we don't have it at compile time
static bool isDynamicCastClassPointer(TR::Node * castOrInstanceOfNode)
   {
   if (castOrInstanceOfNode->getOpCodeValue() == TR::instanceof)
      {
      TR::Node * castClassNode = castOrInstanceOfNode->getSecondChild();
      TR_OpaqueClassBlock* castClassAddr = TR::TreeEvaluator::getCastClassAddress(castClassNode);

      bool isUnresolved = castOrInstanceOfNode->getOpCode().hasSymbolReference() && castOrInstanceOfNode->getSymbolReference()->isUnresolved();

      // came from transformed call isInstance to node instanceof, can't resolve at compile time
      return !castClassAddr && !isUnresolved;
      }
   return false;
   }

/*
 * generate test if object class is reference array
 * testerReg = load (objectClassReg+offset_romClass)
 * andImmediate with J9AccClassArray(0x10000)
 * MASK6 failLabel(If not Array we Fail)
 * testerReg = load (objectClassReg + leafcomponent_offset)
 * testerReg = load (objectClassReg + offset_romClass)
 * testerReg = load (objectClassReg + offset_modifiers)
 * andImmediate with J9AccClassInternalPrimitiveType(0x20000)
 * MASK6 trueLabel(if equal we fail, not equal we succeed)
 */
static void genIsReferenceArrayTest(TR::Node        *node,
                                    TR::Register    *objectClassReg,
                                    TR::Register    *scratchReg1,
                                    TR::Register    *scratchReg2,
                                    TR::Register    *resultReg,
                                    TR::LabelSymbol *failLabel,
                                    TR::LabelSymbol *trueLabel,
                                    bool needsResult,
                                    bool trueFallThrough,
                                    TR::CodeGenerator *cg)
   {
      if (needsResult)
         {
            generateRIInstruction(cg, TR::InstOpCode::LHI, node, resultReg, 0);
         }
      generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, scratchReg1,
                            generateS390MemoryReference(objectClassReg, offsetof(J9Class,romClass), cg));
      generateRXInstruction(cg, TR::InstOpCode::L, node, scratchReg1,
                            generateS390MemoryReference(scratchReg1, offsetof(J9ROMClass, modifiers), cg));
      generateRILInstruction(cg, TR::InstOpCode::NILF, node, scratchReg1, static_cast<int32_t>(J9AccClassArray));
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, failLabel);

      generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, scratchReg1,
                            generateS390MemoryReference(objectClassReg, offsetof(J9ArrayClass,componentType), cg));
      generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, scratchReg1,
                            generateS390MemoryReference(scratchReg1, offsetof(J9Class,romClass), cg));
      generateRXInstruction(cg, TR::InstOpCode::L, node, scratchReg1,
                            generateS390MemoryReference(scratchReg1, offsetof(J9ROMClass, modifiers), cg));
      generateRILInstruction(cg, TR::InstOpCode::NILF, node, scratchReg1, static_cast<int32_t>(J9AccClassInternalPrimitiveType));

      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, failLabel);
      if (needsResult)
         {
         generateRIInstruction(cg, TR::InstOpCode::LHI, node, resultReg, 1);
         }
      if (!trueFallThrough)
         {
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, trueLabel);
         }
   }
// only need a helper call if the class is not super and not final, otherwise
// it can be determined without a call-out
static bool needHelperCall(TR::Node * castOrInstanceOfNode, bool testCastClassIsSuper, bool isFinalClass)
   {
   return (!testCastClassIsSuper || isDynamicCastClassPointer(castOrInstanceOfNode)) && !isFinalClass;
   }

static bool needTestCache(bool cachingEnabled, bool needsHelperCall, bool superClassTest)
   {
   return cachingEnabled && needsHelperCall && !superClassTest;
   }

static TR::Register * establishLitPoolBaseReg(TR::Node * castOrInstanceOfNode, TR::CodeGenerator * cg)
   {
   if (castOrInstanceOfNode->getNumChildren() != 3)
      {
      return NULL;
      }
   else
      {
      TR::Node* litPoolBaseChild = castOrInstanceOfNode->getLastChild();
      TR_ASSERT((litPoolBaseChild->getOpCodeValue()==TR::aload) || (litPoolBaseChild->getOpCodeValue()==TR::aRegLoad),
         "Literal pool base child expected\n");
      return cg->evaluate(litPoolBaseChild);
      }
   }

// this is messy and a rough approximation - there can be no more than 10
// post dependencies in instance-of.
static int maxInstanceOfPostDependencies()
   {
   return 10;
   }

// similarly yucky... instanceof takes 2 parms and kills the return address
bool killedByInstanceOfHelper(int32_t regIndex, TR::Node * node, TR::CodeGenerator * cg)
   {
   if (regIndex == -1)
      {
      return false; // not mapped to a specific register
      }
   int realReg = cg->getGlobalRegister(regIndex);

#if defined(TR_TARGET_64BIT)
   bool needsHelperCall = false;
#if defined(J9ZOS390)
   if (cg->comp()->getOption(TR_EnableRMODE64))
#endif
      {
      TR::Node * castClassNode = node->getSecondChild();
      TR::SymbolReference * castClassSymRef = castClassNode->getSymbolReference();
      bool testCastClassIsSuper = TR::TreeEvaluator::instanceOfOrCheckCastNeedSuperTest(node, cg);
      bool isFinalClass = (castClassSymRef == NULL) ? false : castClassSymRef->isNonArrayFinal(cg->comp());
      needsHelperCall = needHelperCall(node, testCastClassIsSuper, isFinalClass);
      }

#endif

   if (realReg == TR::RealRegister::GPR1 ||
       realReg == TR::RealRegister::GPR2 ||
       realReg == cg->getReturnAddressRegister()
#if defined(TR_TARGET_64BIT)
       || (needsHelperCall &&
#if defined(J9ZOS390)
           cg->comp()->getOption(TR_EnableRMODE64) &&
#endif
           realReg == cg->getEntryPointRegister())
#endif
      )
      {
      return true;
      }
   else
      {
      return false;
      }
   }

static bool generateInlineTest(TR::CodeGenerator * cg, TR::Node * node, TR::Node * castClassNode,
                               TR::Register * objClassReg, TR::Register * resultReg,
                               TR::Register * scratchReg, TR::Register * litPoolReg,
                               bool needsResult, TR::LabelSymbol * falseLabel,
                               TR::LabelSymbol * trueLabel, TR::LabelSymbol * doneLabel, bool isCheckCast, int32_t maxNum_PICS = NUM_PICS)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   TR_OpaqueClassBlock* guessClassArray[NUM_PICS];
   TR_OpaqueClassBlock* castClassAddr = TR::TreeEvaluator::getCastClassAddress(castClassNode);
   uint8_t num_PICs = 0, i;

   if (!castClassAddr)
      {
      return false;
      }

   if (isCheckCast)
      {
      TR_OpaqueClassBlock *tempGuessClassArray[NUM_PICS];
      uint8_t numberOfGuessClasses = TR::TreeEvaluator::interpreterProfilingInstanceOfOrCheckCastInfo(cg, node, tempGuessClassArray);
      if (numberOfGuessClasses > 0)
         {
         for (i = 0; i < numberOfGuessClasses; i++)
            {
            if (instanceOfOrCheckCast((J9Class*)tempGuessClassArray[i], (J9Class*)castClassAddr))
               {
               guessClassArray[num_PICs++] = tempGuessClassArray[i];
               if (maxNum_PICS == num_PICs) break;
               }
            }
         }
      }
   else
      {
      num_PICs = TR::TreeEvaluator::interpreterProfilingInstanceOfOrCheckCastInfo(cg, node, guessClassArray);
      }

   // defect 92901
   // if test fails, in case of checkcast, there is no need to generate inline check for guess value
   if (num_PICs == 0)
      return false;

   bool result_bool;
   TR::LabelSymbol *result_label;
   TR::Instruction * unloadableConstInstr[NUM_PICS];
   num_PICs = ((num_PICs > maxNum_PICS) ? maxNum_PICS : num_PICs);
   for (i = 0; i < num_PICs; i++)
      {
      dumpOptDetails(comp, "inline test with guess class address of %p\n", guessClassArray[i]);
      if (cg->needClassAndMethodPointerRelocations())
         unloadableConstInstr[i] = generateRegLitRefInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, scratchReg,(uintptrj_t) guessClassArray[i], TR_ClassPointer, NULL, NULL, NULL);
      else
         unloadableConstInstr[i] = generateRILInstruction(cg, TR::InstOpCode::LARL, node, scratchReg, guessClassArray[i]);

      if (fej9->isUnloadAssumptionRequired((TR_OpaqueClassBlock *)(guessClassArray[i]), comp->getCurrentMethod()))
         comp->getStaticPICSites()->push_front(unloadableConstInstr[i]);

      if (cg->wantToPatchClassPointer(guessClassArray[i], node))
         comp->getStaticHCRPICSites()->push_front(unloadableConstInstr[i]);

      result_bool = instanceOfOrCheckCast((J9Class*)(guessClassArray[i]), (J9Class*)castClassAddr);
      result_label = (falseLabel != trueLabel ) ? (result_bool ? trueLabel : falseLabel) : doneLabel;

      if (needsResult)
         generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, resultReg, (int32_t)result_bool);
      generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalRegOpCode(), node, objClassReg, scratchReg, TR::InstOpCode::COND_BE, result_label);

      }
   return true;
   }
static void
generateTestBitFlag(
      TR::CodeGenerator *cg,
      TR::Node *node,
      TR::Register *mdReg,
      int32_t offset,
      int32_t size,
      uint64_t bitFlag)
   {
   TR::MemoryReference * tempMR;
   int shiftForFlag = TR::TreeEvaluator::checkNonNegativePowerOfTwo((int64_t) bitFlag);
   TR_ASSERT(shiftForFlag > 0, "generateTestBitFlag: flag is assumed to be power of 2\n");

   // point offset to the end of the word we point to, so we can make a byte comparison using tm
   offset += size - 1;

   // TM tests the bits for one byte, so we calculate several displacements for different flags
   //  Even though TM does not require the flag to be a power of two, the following code and the previous assumption require it
   if (shiftForFlag < 8)
      {
      tempMR = generateS390MemoryReference(mdReg, offset, cg);
      }
   else if (shiftForFlag < 16)
      {
      tempMR = generateS390MemoryReference(mdReg, offset - 1, cg);
      bitFlag = bitFlag >> 8;
      }
   else if (shiftForFlag < 24)
      {
      tempMR = generateS390MemoryReference(mdReg, offset - 2, cg);
      bitFlag = bitFlag >> 16;
      }
   else if (shiftForFlag < 32)
      {
      tempMR = generateS390MemoryReference(mdReg, offset - 3, cg);
      bitFlag = bitFlag >> 24;
      }
#if defined(TR_TARGET_64BIT)
   else if (shiftForFlag < 40)
      {
      tempMR = generateS390MemoryReference(mdReg, offset - 4, cg);
      bitFlag = bitFlag >> 32;
      }
   else if (shiftForFlag < 48)
      {
      tempMR = generateS390MemoryReference(mdReg, offset - 5, cg);
      bitFlag = bitFlag >> 40;
      }
   else if (shiftForFlag < 56)
      {
      tempMR = generateS390MemoryReference(mdReg, offset - 6, cg);
      bitFlag = bitFlag >> 48;
      }
   else if (shiftForFlag < 64)
      {
      tempMR = generateS390MemoryReference(mdReg, offset - 7, cg);
      bitFlag = bitFlag >> 56;
      }
#endif
   else
      {
      TR_ASSERT(0, "generateTestBitFlag: flag size assumption incorrect\n");
      }

   generateSIInstruction(cg, TR::InstOpCode::TM, node, tempMR, (uint32_t) bitFlag);
   }

static void
VMnonNullSrcWrtBarCardCheckEvaluator(
      TR::Node * node,
      TR::Register * owningObjectReg,
      TR::Register * srcReg,
      TR::Register *temp1Reg,
      TR::Register *temp2Reg,
      TR::LabelSymbol *doneLabel,
      TR::SymbolReference *wbRef ,
      TR::RegisterDependencyConditions *conditions,
      TR::CodeGenerator *cg,
      bool doCompileTimeCheckForHeapObj = true)
   {
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   auto gcMode = TR::Compiler->om.writeBarrierType();
   bool doWrtBar = (gcMode == gc_modron_wrtbar_oldcheck || gcMode == gc_modron_wrtbar_cardmark_and_oldcheck || gcMode == gc_modron_wrtbar_always);
   //We need to do a runtime check on cardmarking for gencon policy if our owningObjReg is in tenure
   bool doCrdMrk = (gcMode == gc_modron_wrtbar_cardmark_and_oldcheck);

   TR_ASSERT(srcReg != NULL, "VMnonNullSrcWrtBarCardCheckEvaluator: Cannot send in a null source object...look at the fcn name\n");
   TR_ASSERT(doWrtBar == true,"VMnonNullSrcWrtBarCardCheckEvaluator: Invalid call to VMnonNullSrcWrtBarCardCheckEvaluator\n");

   TR::Node * wrtbarNode = NULL;
   TR::LabelSymbol * helperSnippetLabel = generateLabelSymbol(cg);
   if (node->getOpCodeValue() == TR::awrtbari || node->getOpCodeValue() == TR::awrtbar)
      wrtbarNode = node;
   else if (node->getOpCodeValue() == TR::ArrayStoreCHK)
      wrtbarNode = node->getFirstChild();
   if (gcMode != gc_modron_wrtbar_always)
      {
      bool is64Bit = TR::Compiler->target.is64Bit();
      bool isConstantHeapBase = !comp->getOptions()->isVariableHeapBaseForBarrierRange0();
      bool isConstantHeapSize = !comp->getOptions()->isVariableHeapSizeForBarrierRange0();
      int32_t shiftAmount = TR::Compiler->om.compressedReferenceShift();
      TR::InstOpCode::Mnemonic opLoadReg = TR::InstOpCode::getLoadRegOpCode();
      TR::InstOpCode::Mnemonic opSubtractReg = TR::InstOpCode::getSubstractRegOpCode();
      TR::InstOpCode::Mnemonic opSubtract = TR::InstOpCode::getSubstractOpCode();
      TR::InstOpCode::Mnemonic opCmpLog = TR::InstOpCode::getCmpLogicalOpCode();
      bool disableSrcObjCheck = true; //cg->comp()->getOption(TR_DisableWrtBarSrcObjCheck);
      bool constantHeapCase = ((!comp->compileRelocatableCode()) && isConstantHeapBase && isConstantHeapSize && shiftAmount == 0 && (!is64Bit || TR::Compiler->om.generateCompressedObjectHeaders()));
      if (constantHeapCase)
         {
         // these return uintptrj_t but because of the if(constantHeapCase) they are guaranteed to be <= MAX(uint32_t). The uses of heapSize, heapBase, and heapSum need to be uint32_t.
         uint32_t heapSize = comp->getOptions()->getHeapSizeForBarrierRange0();
         uint32_t heapBase = comp->getOptions()->getHeapBaseForBarrierRange0();

         if (!doCrdMrk && !disableSrcObjCheck)
            {
            uint32_t heapSum = heapBase + heapSize;
            generateRRInstruction(cg, opLoadReg, node, temp1Reg, owningObjectReg);
            generateRILInstruction(cg, TR::InstOpCode::IILF, node, temp2Reg, heapSum);
            generateRRInstruction(cg, opSubtractReg, node, temp1Reg, temp2Reg);
            generateRRInstruction(cg, opSubtractReg, node, temp2Reg, srcReg);
            generateRRInstruction(cg, is64Bit ? TR::InstOpCode::NGR : TR::InstOpCode::NR, node, temp1Reg, temp2Reg);
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, node, doneLabel);
            }
         else
            {
            generateRRInstruction(cg, opLoadReg, node, temp1Reg, owningObjectReg); //copy owning into temp
            generateRILInstruction(cg, is64Bit ? TR::InstOpCode::SLGFI : TR::InstOpCode::SLFI, node, temp1Reg, heapBase); //temp = temp - heapbase
            generateS390CompareAndBranchInstruction(cg, is64Bit ? TR::InstOpCode::CLG : TR::InstOpCode::CL, node, temp1Reg, heapSize, TR::InstOpCode::COND_BH, doneLabel, false, false, NULL, conditions);
            }
         }
      else
         {
         TR::MemoryReference * offset = generateS390MemoryReference(cg->getMethodMetaDataRealRegister(), offsetof(J9VMThread, heapBaseForBarrierRange0), cg);
         TR::MemoryReference * size = generateS390MemoryReference(cg->getMethodMetaDataRealRegister(), offsetof(J9VMThread, heapSizeForBarrierRange0), cg);
         generateRRInstruction(cg, opLoadReg, node, temp1Reg, owningObjectReg);
         generateRXInstruction(cg, opSubtract, node, temp1Reg, offset);
         generateRXInstruction(cg, opCmpLog, node, temp1Reg, size);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BH, node, doneLabel);
         }

      TR::LabelSymbol *noChkLabel = generateLabelSymbol(cg);

      if (!TR::Options::getCmdLineOptions()->realTimeGC())
         {
         bool isDefinitelyNonHeapObj = false, isDefinitelyHeapObj = false;
         if (wrtbarNode != NULL && doCompileTimeCheckForHeapObj)
            {
            isDefinitelyNonHeapObj = wrtbarNode->isNonHeapObjectWrtBar();
            isDefinitelyHeapObj = wrtbarNode->isHeapObjectWrtBar();
            }
         if (doCrdMrk && !isDefinitelyNonHeapObj)
            {
            TR::LabelSymbol *srcObjChkLabel = generateLabelSymbol(cg);
            // CompileTime check for heap object
            // SRLG r2, rHeapAddr, cardSize
            // L    r1, cardTableVirtualStartOffset(metaData)
            // LHI  r3,0x1
            // STC  r3,0x0(r1,r2)
            uintptr_t cardSize = comp->getOptions()->getGcCardSize();
            int32_t shiftValue = TR::TreeEvaluator::checkNonNegativePowerOfTwo((int32_t) cardSize);
            TR::Register * cardOffReg = temp1Reg;
            TR::Register * mdReg = cg->getMethodMetaDataRealRegister();

            // If conditions are NULL, we handle early assignment here.
            // O.w. caller is responsible for handling early assignment and making sure GPR1, GPR2 and RAREG are
            // available in conditions
            TR_ASSERT(shiftValue > 0,"VMnonNullSrcWrtBarCardCheckEvaluator: Card size must be power of 2");
            static_assert(CARD_DIRTY <= MAX_IMMEDIATE_VAL, "VMCardCheckEvaluator: CARD_DIRTY flag is assumed to be small enough for an imm op");

            // If it is tarok balanced policy, we must generate card marking sequence.
            //
            auto gcMode = TR::Compiler->om.writeBarrierType();
            if (!(gcMode == gc_modron_wrtbar_cardmark_incremental || gcMode == gc_modron_wrtbar_satb))
               {
               generateTestBitFlag(cg, node, mdReg, offsetof(J9VMThread, privateFlags), sizeof(UDATA), J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE);
               // If the flag is not set, then we skip card marking
               generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, srcObjChkLabel);
               }
            // dirty(activeCardTableBase + temp3Reg >> card_size_shift)
            if (TR::Compiler->target.is64Bit())
               generateRSInstruction(cg, TR::InstOpCode::SRLG, node, cardOffReg, cardOffReg, shiftValue);
            else
               generateRSInstruction(cg, TR::InstOpCode::SRL, node, cardOffReg, shiftValue);

            generateRXInstruction(cg, TR::InstOpCode::getAddOpCode(), node, cardOffReg,
                                  generateS390MemoryReference(mdReg, offsetof(J9VMThread, activeCardTableBase), cg));
            // Store the flag to the card's byte.
            generateSIInstruction(cg, TR::InstOpCode::MVI, node, generateS390MemoryReference(cardOffReg,0x0,cg), CARD_DIRTY);

            if (!disableSrcObjCheck)
               generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, noChkLabel);
            // If condition is NULL, the early assignment is handled by caller.
            // If not, early assignment handled here
            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, srcObjChkLabel, conditions);
            }
         }
      else
         TR_ASSERT(0, "card marking not supported for RT");

      //Either if cardmarking is not on at compile time or runtime, we want to test srcobj because if its not in nursery, then
      //we don't have to do wrtbarrier
      if (!disableSrcObjCheck && !(!doCrdMrk && constantHeapCase))
         {
         generateRRInstruction(cg, opLoadReg, node, temp1Reg, srcReg);
         if (constantHeapCase)
            {
            // these return uintptrj_t but because of the if(constantHeapCase) they are guaranteed to be <= MAX(uint32_t). The uses of heapSize, heapBase, and heapSum need to be uint32_t.
            uint32_t heapBase = comp->getOptions()->getHeapBaseForBarrierRange0();
            uint32_t heapSize = comp->getOptions()->getHeapSizeForBarrierRange0();
            generateRILInstruction(cg, is64Bit ? TR::InstOpCode::SLGFI : TR::InstOpCode::SLFI, node, temp1Reg, heapBase);
            generateS390CompareAndBranchInstruction(cg, is64Bit ? TR::InstOpCode::CLG : TR::InstOpCode::CL, node, temp1Reg, heapSize, TR::InstOpCode::COND_BL, doneLabel, false);
            }
         else
            {
            TR::MemoryReference *offset = generateS390MemoryReference(cg->getMethodMetaDataRealRegister(),
                  offsetof(J9VMThread, heapBaseForBarrierRange0), cg);
            TR::MemoryReference *size = generateS390MemoryReference(cg->getMethodMetaDataRealRegister(),
                  offsetof(J9VMThread, heapSizeForBarrierRange0), cg);
            generateRXInstruction(cg, opSubtract, node, temp1Reg, offset);
            generateRXInstruction(cg, opCmpLog, node, temp1Reg, size);
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, node, doneLabel);
            }
         }
      //If cardmarking is on at compile time (mode=wrtbaroldcrdmrkcheck) then need a label for when cardmarking is done
      //in which case we need to skip the srcobj check
      if (doCrdMrk)
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, noChkLabel, conditions);

      // inline checking remembered bit for generational or (gencon+cardmarking is inlined).
      static_assert(J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST <= 0xFF, "The constant is too big");
      int32_t offsetToAgeBits =  TR::Compiler->om.offsetOfHeaderFlags() + 3;
#if defined(J9VM_INTERP_FLAGS_IN_CLASS_SLOT) && defined(TR_TARGET_64BIT) && defined(OMR_GC_FULL_POINTERS)
      offsetToAgeBits += 4;
#endif
      TR::MemoryReference * tempMR = generateS390MemoryReference(owningObjectReg, offsetToAgeBits, cg);
      generateSIInstruction(cg, TR::InstOpCode::TM, node, tempMR, J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST);
      //Need to do wrtbarrer, go to the snippet
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK8, node, helperSnippetLabel);
      }
   else
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, helperSnippetLabel);

   //Create a snipper to make the call so the fall through path is to doneLabel, we expect to call the helper less, this would remove a
   //branch
   cg->addSnippet(new (cg->trHeapMemory()) TR::S390HelperCallSnippet(cg, node, helperSnippetLabel, wbRef, doneLabel));
   }

static void
VMCardCheckEvaluator(
      TR::Node * node,
      TR::Register * owningObjectReg,
      TR::Register * tempReg,
      TR::RegisterDependencyConditions * conditions,
      TR::CodeGenerator * cg,
      bool clobberDstReg,
      TR::LabelSymbol *doneLabel = NULL,
      bool doCompileTimeCheckForHeapObj = true)
   {
   if (!TR::Options::getCmdLineOptions()->realTimeGC())
      {
      TR::Node * wrtbarNode = NULL;
      if (node->getOpCodeValue() == TR::awrtbari || node->getOpCodeValue() == TR::awrtbar)
         wrtbarNode = node;
      else if (node->getOpCodeValue() == TR::ArrayStoreCHK)
         wrtbarNode = node->getFirstChild();

      // CompileTime check for heap object
      bool isDefinitelyNonHeapObj = false, isDefinitelyHeapObj = false;

      if (wrtbarNode != NULL && doCompileTimeCheckForHeapObj)
         {
         isDefinitelyNonHeapObj = wrtbarNode->isNonHeapObjectWrtBar();
         isDefinitelyHeapObj = wrtbarNode->isHeapObjectWrtBar();
         }

      // Make sure we really should be here
      TR::Compilation * comp = cg->comp();

      // 83613: We used to do inline CM for Old&CM Objects.
      // However, since all Old objects will go through the wrtbar helper,
      // which will CM too, our inline CM would become redundant.
      TR_ASSERT( (TR::Compiler->om.writeBarrierType()==gc_modron_wrtbar_cardmark || TR::Compiler->om.writeBarrierType()==gc_modron_wrtbar_cardmark_incremental) && !isDefinitelyNonHeapObj,
         "VMCardCheckEvaluator: Invalid call to cardCheckEvaluator\n");
      TR_ASSERT(doneLabel, "VMCardCheckEvaluator: doneLabel must be defined\n");
      TR_ASSERT((conditions && tempReg || clobberDstReg), "VMCardCheckEvaluator: Either a tempReg must be sent in to be used, or we should be able to clobber the owningObjReg\n");
      TR_ASSERT(!(clobberDstReg && tempReg), "VMCardCheckEvaluator: If owningObjReg is clobberable, don't allocate a tempReg\n");

      // We do not card-mark non-heap objects.
      if (!isDefinitelyNonHeapObj)
         {
         // SRLG r2, rHeapAddr, cardSize
         // L    r1, cardTableVirtualStartOffset(metaData)
         // LHI  r3,0x1
         // STC  r3,0x0(r1,r2)

         uintptr_t cardSize = comp->getOptions()->getGcCardSize();
         int32_t shiftValue = TR::TreeEvaluator::checkNonNegativePowerOfTwo((int32_t) cardSize);

         TR::Register * cardOffReg;
         TR::Register * mdReg = cg->getMethodMetaDataRealRegister();

         if (!clobberDstReg)
            cardOffReg = tempReg;
         else if (clobberDstReg)
            cardOffReg = owningObjectReg;

         TR_ASSERT(shiftValue > 0,"VMCardCheckEvaluator: Card size must be power of 2");
         static_assert(CARD_DIRTY <= MAX_IMMEDIATE_VAL, "VMCardCheckEvaluator: CARD_DIRTY flag is assumed to be small enough for an imm op");

         // If it is tarok balanced policy, we must generate card marking sequence.
         auto gcMode = TR::Compiler->om.writeBarrierType();
         if (!(gcMode == gc_modron_wrtbar_cardmark_incremental || gcMode == gc_modron_wrtbar_satb))
            {
            generateTestBitFlag(cg, node, mdReg, offsetof(J9VMThread, privateFlags), sizeof(UDATA), J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE);
            // If the flag is not set, then we skip card marking
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, doneLabel);
            }

         // cardOffReg (Temp) = owningObjectReg - heapBaseForBarrierRange0
         // Defect 91242 - If we can clobber the destination reg, then use owningObjectReg instead of cardOffReg.
         if (!clobberDstReg)
            generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, cardOffReg, owningObjectReg);
         generateRXInstruction(cg, TR::InstOpCode::getSubstractOpCode(), node, cardOffReg,
                               generateS390MemoryReference(mdReg, offsetof(J9VMThread, heapBaseForBarrierRange0), cg));

         // Unless we know it's definitely a heap object, we need to check if offset
         // from base is less than heap size to determine if object resides in heap.
         if (!isDefinitelyHeapObj)
            {
            // if (cardOffReg(Temp) >= heapSizeForBarrierRage0), object not in the heap
            generateRXInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, cardOffReg,
                                      generateS390MemoryReference(mdReg, offsetof(J9VMThread, heapSizeForBarrierRange0), cg));
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNL, node, doneLabel);
            }

         // dirty(activeCardTableBase + temp3Reg >> card_size_shift)
         if (TR::Compiler->target.is64Bit())
            generateRSInstruction(cg, TR::InstOpCode::SRLG, node, cardOffReg, cardOffReg, shiftValue);
         else
            generateRSInstruction(cg, TR::InstOpCode::SRL, node, cardOffReg, shiftValue);

         //add the ActiveCardTableBase to the card offset
         generateRXInstruction(cg, TR::InstOpCode::getAddOpCode(), node, cardOffReg,
                generateS390MemoryReference(mdReg, offsetof(J9VMThread, activeCardTableBase), cg));
         // Store the flag to the card's byte.
         generateSIInstruction(cg, TR::InstOpCode::MVI, node, generateS390MemoryReference(cardOffReg, 0x0, cg), CARD_DIRTY);
         }
      }
   else
      TR_ASSERT(0, "VMCardCheckEvaluator not supported for RT");
   }

static void
VMwrtbarEvaluator(
      TR::Node * node,
      TR::Register * srcReg,
      TR::Register * owningObjectReg,
      bool srcNonNull,
      TR::CodeGenerator * cg)
   {
   TR::Instruction * cursor;
   TR::Compilation * comp = cg->comp();
   auto gcMode = TR::Compiler->om.writeBarrierType();
   bool doWrtBar = (gcMode == gc_modron_wrtbar_oldcheck || gcMode == gc_modron_wrtbar_cardmark_and_oldcheck || gcMode == gc_modron_wrtbar_always);
   bool doCrdMrk = ((gcMode == gc_modron_wrtbar_cardmark ||gcMode == gc_modron_wrtbar_cardmark_and_oldcheck || gcMode == gc_modron_wrtbar_cardmark_incremental)&& !node->isNonHeapObjectWrtBar());

   // See VM Design 2048 for when wrtbar can be skipped, as determined by VP.
   if ( (node->getOpCode().isWrtBar() && node->skipWrtBar()) ||
        ((node->getOpCodeValue() == TR::ArrayStoreCHK) && node->getFirstChild()->getOpCode().isWrtBar() && node->getFirstChild()->skipWrtBar() ) )
      return;
   TR::RegisterDependencyConditions * conditions;
   TR::LabelSymbol * doneLabel = generateLabelSymbol(cg);
   if (doWrtBar)
      conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg);
   else
      conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 1, cg);

   if (doWrtBar) // generational or gencon
      {
      TR::SymbolReference * wbRef = NULL;
      if (gcMode == gc_modron_wrtbar_always)
         wbRef = comp->getSymRefTab()->findOrCreateWriteBarrierStoreSymbolRef();
      else // use jitWriteBarrierStoreGenerational for both generational and gencon, because we inline card marking.
         {
         static char *disable = feGetEnv("TR_disableGenWrtBar");
         wbRef = disable ?
            comp->getSymRefTab()->findOrCreateWriteBarrierStoreSymbolRef() :
            comp->getSymRefTab()->findOrCreateWriteBarrierStoreGenerationalSymbolRef();
         }
      TR::Register *epReg, *raReg;
      epReg = cg->allocateRegister();
      raReg = cg->allocateRegister();
      conditions->addPostCondition(raReg, cg->getReturnAddressRegister());
      conditions->addPostCondition(owningObjectReg, TR::RealRegister::GPR1);
      conditions->addPostCondition(srcReg, TR::RealRegister::GPR2);
      conditions->addPostCondition(epReg, cg->getEntryPointRegister());
      if (srcNonNull == false)
         {
         // If object is NULL, done
         static_assert(NULLVALUE == 0, "NULLVALUE is assumed to be zero here");
         generateRRInstruction(cg, TR::InstOpCode::getLoadTestRegOpCode(), node, srcReg, srcReg);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, doneLabel);
         }
      // Inlines cardmarking and remembered bit check for gencon.
      VMnonNullSrcWrtBarCardCheckEvaluator(node, owningObjectReg, srcReg, epReg, raReg, doneLabel, wbRef, conditions, cg, false);
      cg->stopUsingRegister(epReg);
      cg->stopUsingRegister(raReg);
      }
   else if (doCrdMrk)  // -Xgc:optavgpause, concurrent marking only
      {
      conditions->addPostCondition(owningObjectReg, TR::RealRegister::AssignAny);
      VMCardCheckEvaluator(node, owningObjectReg, NULL, conditions, cg, true, doneLabel);
      }
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneLabel, conditions);
   }

///////////////////////////////////////////////////////////////////////////////////////
// monentEvaluator:  acquire lock for synchronising method
///////////////////////////////////////////////////////////////////////////////////////
TR::Register *
J9::Z::TreeEvaluator::monentEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return TR::TreeEvaluator::VMmonentEvaluator(node, cg);
   }

///////////////////////////////////////////////////////////////////////////////////////
// monexitEvaluator:  release lock for synchronising method
///////////////////////////////////////////////////////////////////////////////////////
TR::Register *
J9::Z::TreeEvaluator::monexitEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return TR::TreeEvaluator::VMmonexitEvaluator(node, cg);
   }

///////////////////////////////////////////////////////////////////////////////////////
// monexitfence -- do nothing, just a placeholder for live monitor meta data
///////////////////////////////////////////////////////////////////////////////////////
TR::Register *
J9::Z::TreeEvaluator::monexitfenceEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return NULL;
   }

///////////////////////////////////////////////////////////////////////////////////////
// asynccheckEvaluator: GC point
///////////////////////////////////////////////////////////////////////////////////////
TR::Register *
J9::Z::TreeEvaluator::asynccheckEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   // used by asynccheck
   // The child contains an inline test.
   //
   TR::Node * testNode = node->getFirstChild();
   TR::Node * firstChild = testNode->getFirstChild();
   TR::Node * secondChild = testNode->getSecondChild();
   TR::Compilation *comp = cg->comp();
   intptrj_t value = TR::Compiler->target.is64Bit() ? secondChild->getLongInt() : secondChild->getInt();

   TR_ASSERT( testNode->getOpCodeValue() == (TR::Compiler->target.is64Bit() ? TR::lcmpeq : TR::icmpeq), "asynccheck bad format");
   TR_ASSERT( secondChild->getOpCode().isLoadConst() && secondChild->getRegister() == NULL, "asynccheck bad format");

   TR::LabelSymbol * snippetLabel = generateLabelSymbol(cg);
   TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
   TR::Instruction * gcPoint;

   TR::LabelSymbol * reStartLabel = generateLabelSymbol(cg);

   // (0)  asynccheck #4[0x004d7a88]Method[jitCheckAsyncMessages]
   // (1)    icmpeq
   // (1)      iload #281[0x00543138] MethodMeta[stackOverflowMark]+28
   // (1)      iconst -1

   if (TR::Compiler->target.is32Bit() &&
       (firstChild->getOpCodeValue() == TR::iload) &&
       firstChild->getRegister() == NULL && value < 0)
      {
      // instead of comparing to the value itself, we can compare to 0
      // and, if the value is less than zero, we know it must be an async-check
      // since non-code addresses are always positive in 31-bit 390 code so the only
      // negative address we could have would be the 'bogus' -1 address to force
      // async-check.
      // (the VM ensures that all malloc'ed storage has the high-order-bit cleared)
      TR::Register * testRegister = cg->allocateRegister();
      TR::MemoryReference * tempMR = generateS390MemoryReference(firstChild, cg);

      TR_ASSERT( getIntegralValue(secondChild) == -1, "asynccheck bad format");
      TR_ASSERT(  TR::Compiler->target.is32Bit(), "ICM can be used for 32bit code-gen only!");

      static char * dontUseTM = feGetEnv("TR_DONTUSETMFORASYNC");
      if (comp->getOption(TR_DisableOOL))
         {
         reStartLabel->setEndInternalControlFlow();
         }
      if (firstChild->getReferenceCount()>1 || dontUseTM)
         {
         generateRSInstruction(cg, TR::InstOpCode::ICM, firstChild, testRegister, (uint32_t) 0xF, tempMR);
         if (comp->getOption(TR_DisableOOL))
            {
            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
            cFlowRegionStart->setStartInternalControlFlow();
            }
         gcPoint = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, node, snippetLabel);
         }
      else
         {
         generateSIInstruction(cg, TR::InstOpCode::TM, firstChild, tempMR, 0xFF);
         if (comp->getOption(TR_DisableOOL))
            {
            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
            cFlowRegionStart->setStartInternalControlFlow();
            }
         gcPoint = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BO, node, snippetLabel);
         }

      firstChild->setRegister(testRegister);
      tempMR->stopUsingMemRefRegister(cg);
      }
   else
      {

      if (comp->getOption(TR_DisableOOL))
         {
         reStartLabel->setEndInternalControlFlow();
         }
      if (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL)
         {
         if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z10))
            {
            TR::MemoryReference * tempMR = generateS390MemoryReference(firstChild, cg);

            if (tempMR->getIndexRegister() != NULL && tempMR->getBaseRegister() != NULL)
               {
               TR::SymbolReference * symRef = firstChild->getSymbolReference();
               TR::Symbol * symbol = symRef->getSymbol();
               TR::Register * src1Reg = NULL;
               if (firstChild->getDataType() == TR::Address &&
                   !symbol->isInternalPointer() &&
                   !symbol->isNotCollected()    &&
                   !symbol->isAddressOfClassObject())
                  {
                  src1Reg = cg->allocateCollectedReferenceRegister();
                  }
               else
                  {
                  src1Reg = cg->allocateRegister();
                  }
               generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), firstChild, src1Reg, tempMR);

               updateReferenceNode(firstChild, src1Reg);
               firstChild->setRegister(src1Reg);

               generateRIInstruction(cg, TR::InstOpCode::getCmpHalfWordImmOpCode(), node, src1Reg, value);
               }
            else
               {
               generateSILInstruction(cg, TR::InstOpCode::getCmpHalfWordImmToMemOpCode(), node, tempMR, value);
               }
            tempMR->stopUsingMemRefRegister(cg);
            }
         else
            {
            TR::Register * src1Reg = cg->evaluate(firstChild);

            generateRIInstruction(cg, TR::InstOpCode::getCmpHalfWordImmOpCode(), node, src1Reg, value);
            }
         }
      else
         {
         TR::Register * src1Reg = cg->evaluate(firstChild);
         TR::Register * tempReg = cg->evaluate(secondChild);
         generateRRInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, src1Reg, tempReg);
         }
      if (comp->getOption(TR_DisableOOL))
         {
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
         cFlowRegionStart->setStartInternalControlFlow();
         }
      gcPoint = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, snippetLabel);
      }

   TR::RegisterDependencyConditions * dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg);
   TR::Register * rRA = cg->allocateRegister();
   // only 64bit zLinux and zOS trampoline requires rEP
#if defined(TR_TARGET_64BIT)
   TR::Register * rEP = NULL;
#if defined(J9ZOS390)
   if (comp->getOption(TR_EnableRMODE64))
#endif
      {
      rEP = cg->allocateRegister();
      dependencies->addPostCondition(rEP, cg->getEntryPointRegister());
      }
#endif

   dependencies->addPostCondition(rRA, cg->getReturnAddressRegister());

   if (!comp->getOption(TR_DisableOOL))
      {
      TR_Debug * debugObj = cg->getDebug();
      if (debugObj)
         debugObj->addInstructionComment(gcPoint, "Branch to OOL asyncCheck sequence");

      // starts OOL sequence, replacing the helper call snippet
      TR_S390OutOfLineCodeSection *outlinedHelperCall = NULL;
      outlinedHelperCall = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(snippetLabel, reStartLabel, cg);
      cg->getS390OutOfLineCodeSectionList().push_front(outlinedHelperCall);
      outlinedHelperCall->swapInstructionListsWithCompilation();

      // snippetLabel : OOL Start label
      TR::Instruction * cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, snippetLabel);
      if (debugObj)
         debugObj->addInstructionComment(cursor, "Denotes start of OOL asyncCheck sequence");

      // BRASL R14, VMHelper, gc stack map on BRASL
      gcPoint = generateDirectCall(cg, node, false, node->getSymbolReference(), dependencies, cursor);
      gcPoint->setDependencyConditions(dependencies);

      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, reStartLabel);
      if (debugObj)
         debugObj->addInstructionComment(cursor, "Denotes end of OOL asyncCheck sequence: return to mainline");

      // Done using OOL with manual code generation
      outlinedHelperCall->swapInstructionListsWithCompilation();
      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, reStartLabel);
      if (debugObj)
         debugObj->addInstructionComment(cursor, "OOL asyncCheck return label");
      }
   else
      {
      TR::Snippet * snippet = new (cg->trHeapMemory()) TR::S390HelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference(), reStartLabel);
      cg->addSnippet(snippet);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, reStartLabel, dependencies);
      }

   gcPoint->setNeedsGCMap(0x0000FFFF);

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->decReferenceCount(testNode);
#if defined(TR_TARGET_64BIT)
#if defined(J9ZOS390)
   if (comp->getOption(TR_EnableRMODE64))
#endif
      {
      cg->stopUsingRegister(rEP);
      }
#endif
   cg->stopUsingRegister(rRA);

   return NULL;

   }


///////////////////////////////////////////////////////////////////////////////////////
// instanceofEvaluator: symref is the class object, cp index is in the "int" field,
//   child is the object reference
///////////////////////////////////////////////////////////////////////////////////////
TR::Register *
J9::Z::TreeEvaluator::instanceofEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   if (comp->getOption(TR_OptimizeForSpace) || comp->getOption(TR_DisableInlineInstanceOf))
      {
      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::S390CHelperLinkage *helperLink =  static_cast<TR::S390CHelperLinkage*>(cg->getLinkage(TR_CHelper));
      TR::Node::recreate(node, TR::icall);
      TR::Register * targetRegister = helperLink->buildDirectDispatch(node);
      for (auto i=0; i < node->getNumChildren(); i++)
         cg->decReferenceCount(node->getChild(i));
      TR::Node::recreate(node, opCode);
      return targetRegister;
      }
   else
      {
      return TR::TreeEvaluator::VMinstanceOfEvaluator(node, cg);
      }
   }

static void
generateNullChkSnippet(
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::LabelSymbol * snippetLabel = generateLabelSymbol(cg);
   TR::S390BranchInstruction * brInstr = (TR::S390BranchInstruction*) generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, snippetLabel);
   brInstr->setExceptBranchOp();

   TR::SymbolReference *symRef = comp->getSymRefTab()->findOrCreateNullCheckSymbolRef(comp->getMethodSymbol());
   cg->addSnippet(new (cg->trHeapMemory()) TR::S390HelperCallSnippet(cg, node, snippetLabel, symRef));
   }

///////////////////////////////////////////////////////////////////////////////////////
//  checkcastEvaluator - checkcast
///////////////////////////////////////////////////////////////////////////////////////
TR::Register *
J9::Z::TreeEvaluator::checkcastEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   if (comp->getOption(TR_OptimizeForSpace) || comp->getOption(TR_DisableInlineCheckCast))
      {
      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node * objNode = node->getFirstChild();
      bool needsNullTest         = !objNode->isNonNull() && !node->chkIsReferenceNonNull();
      bool isCheckcastAndNullChk = (opCode == TR::checkcastAndNULLCHK);

      // Do null check if needed
      if (needsNullTest && isCheckcastAndNullChk)
         {
         TR::Register * objReg = cg->evaluate(objNode);
         generateRRInstruction(cg, TR::InstOpCode::getLoadTestRegOpCode(), node, objReg, objReg);
         // find the bytecodeInfo of the compacted NULLCHK
         TR::Node *nullChkInfo = comp->findNullChkInfo(node);
         generateNullChkSnippet(nullChkInfo, cg);
         }

      // call helper to do checkcast
      TR::Node::recreate(node, TR::call);
      TR::Register * targetRegister = directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);
      return targetRegister;
      }
   else
      {
      return TR::TreeEvaluator::VMcheckcastEvaluator(node, cg);
      }
   }

///////////////////////////////////////////////////////////////////////////////////////
//  checkcastAndNULLCHKEvaluator - checkcastAndNULLCHK
///////////////////////////////////////////////////////////////////////////////////////
TR::Register *
J9::Z::TreeEvaluator::checkcastAndNULLCHKEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return checkcastEvaluator(node, cg);
   }

/**   \brief Generates helper call sequence for all VMNew nodes.
 *
 *    \param node
 *       A new allocation node for which helper call is going to be generated
 *
 *    \param cg
 *       The code generator used to generate the instructions.
 *
 *    \param doInlineAllocation
 *       A boolean to notify if we have generated inline allocation sequence or not
 *
 *    \param
 *       A register to store return value from helper
 *
 *    \return
 *       A register that contains return value from helper.
 *
 *    \details
 *       Generates a helper call sequence for all new allocation nodes. It also handles special cases where we need to generate 64-bit extended children of call node
 */
TR::Register *
J9::Z::TreeEvaluator::generateHelperCallForVMNewEvaluators(TR::Node *node, TR::CodeGenerator *cg, bool doInlineAllocation, TR::Register *resReg)
   {
   TR::S390CHelperLinkage *helperLink = static_cast<TR::S390CHelperLinkage*>(cg->getLinkage(TR_CHelper));
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR::Node *helperCallNode = TR::Node::createWithSymRef(node, TR::acall, (opCode == TR::New || opCode == TR::variableNew)  ? 1 : 2, node->getSymbolReference());
   TR::Node *firstChild = node->getFirstChild();
   if (!(opCode == TR::New || opCode == TR::variableNew))
      {
      // For 64 bit target we need to make sure we use whole 64 bit register even for loading integers as helper expects arguments like that
      // For these scenarios where children of original node is 32-bit we generate a following helper call node
      // acall
      //   #IF (firstChild ->iconst || iRegLoad ) && 64-bit platform
      //   -> i2l
      //       -> firstChild
      //   #ELSE
      //   ->firstChild
      //   #ENDIF
      //   #IF (secondChild -> iconst || iRegLoad) && 64-bit platform
      //   -> i2l
      //       -> secondChild
      //   #ELSE
      //   ->secondChild
      //   #ENDIF
      // If we generate i2l node, we need to artificially set reference count of node to 1.
      // After helper call is generated we decrease reference count of this node so that a register will be marked dead for RA.
      TR::Node *secondChild = node->getSecondChild();
      if (TR::Compiler->target.is64Bit())
         {
         if (firstChild->getOpCode().isLoadConst() || firstChild->getOpCodeValue() == TR::iRegLoad)
            {
            firstChild = TR::Node::create(TR::i2l, 1, firstChild);
            firstChild->setReferenceCount(1);
            }
         if (secondChild->getOpCode().isLoadConst() || secondChild->getOpCodeValue() == TR::iRegLoad)
            {
            secondChild = TR::Node::create(TR::i2l, 1, secondChild);
            secondChild->setReferenceCount(1);
            }
         }
      helperCallNode->setChild(1, secondChild);
      }
   helperCallNode->setChild(0, firstChild);
   resReg = helperLink->buildDirectDispatch(helperCallNode, resReg);
   for (auto i=0; i < helperCallNode->getNumChildren(); i++)
      {
      if (helperCallNode->getChild(i)->getOpCodeValue() == TR::i2l)
         cg->decReferenceCount(helperCallNode->getChild(i));
      }
   // For some cases, we can not generate inline allocation sequence such as variableNew*. In these cases only helper call is generated.
   // So for these cases we need to decrease reference count of node here.
   if (!doInlineAllocation)
      {
      node->setRegister(resReg);
      for (auto i=0; i<node->getNumChildren(); i++)
         cg->decReferenceCount(node->getChild(i));
      }
   return resReg;
   }

///////////////////////////////////////////////////////////////////////////////////////
// newObjectEvaluator: new symref is the class object
///////////////////////////////////////////////////////////////////////////////////////
TR::Register *
J9::Z::TreeEvaluator::newObjectEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   if (cg->comp()->suppressAllocationInlining())
      return generateHelperCallForVMNewEvaluators(node, cg);
   else
      return TR::TreeEvaluator::VMnewEvaluator(node, cg);
   }

///////////////////////////////////////////////////////////////////////////////////////
// newArrayEvaluator: new array of primitives
///////////////////////////////////////////////////////////////////////////////////////
TR::Register *
J9::Z::TreeEvaluator::newArrayEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   if (cg->comp()->suppressAllocationInlining())
      return generateHelperCallForVMNewEvaluators(node, cg);
   else
      return TR::TreeEvaluator::VMnewEvaluator(node, cg);
   }

///////////////////////////////////////////////////////////////////////////////////////
// newArrayEvaluator: new array of objects
///////////////////////////////////////////////////////////////////////////////////////
TR::Register *
J9::Z::TreeEvaluator::anewArrayEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   if (cg->comp()->suppressAllocationInlining())
      return generateHelperCallForVMNewEvaluators(node, cg);
   else
      return TR::TreeEvaluator::VMnewEvaluator(node, cg);
   }

///////////////////////////////////////////////////////////////////////////////////////
// multianewArrayEvaluator:  multi-dimensional new array of objects
///////////////////////////////////////////////////////////////////////////////////////
TR::Register *
J9::Z::TreeEvaluator::multianewArrayEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR::Node::recreate(node, TR::acall);
   TR::Register * targetRegister = directCallEvaluator(node, cg);
   TR::Node::recreate(node, opCode);
   return targetRegister;
   }


TR::Register *
J9::Z::TreeEvaluator::arraylengthEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR::Register *objectReg = cg->evaluate(node->getFirstChild());
   TR::Register *lengthReg = cg->allocateRegister();

   TR::MemoryReference *contiguousArraySizeMR = generateS390MemoryReference(objectReg, fej9->getOffsetOfContiguousArraySizeField(), cg);
   TR::MemoryReference *discontiguousArraySizeMR = generateS390MemoryReference(objectReg, fej9->getOffsetOfDiscontiguousArraySizeField(), cg);

   // Load the Contiguous Array Size and test if it's zero.
   generateRSInstruction(cg, TR::InstOpCode::ICM, node, lengthReg, (uint32_t) 0xF, contiguousArraySizeMR);

   if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z196))
      {
      // Conditionally load from discontiguousArraySize if contiguousArraySize is zero
      generateRSInstruction(cg, TR::InstOpCode::LOC, node, lengthReg, 0x8, discontiguousArraySizeMR);
      }
   else
      {
      TR::LabelSymbol * oolStartLabel = generateLabelSymbol(cg);
      TR::LabelSymbol * oolReturnLabel = generateLabelSymbol(cg);

      // Branch to OOL if contiguous array size is zero
      TR::Instruction * temp = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, oolStartLabel);

      TR_S390OutOfLineCodeSection *outlinedDiscontigPath = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(oolStartLabel,oolReturnLabel,cg);
      cg->getS390OutOfLineCodeSectionList().push_front(outlinedDiscontigPath);
      outlinedDiscontigPath->swapInstructionListsWithCompilation();

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, oolStartLabel);

      if (cg->getDebug())
         {
         cg->getDebug()->addInstructionComment(temp, "Start of OOL arraylength sequence");
         }

      // Load from discontiguousArraySize if contiguousArraySize is zero
      generateRXInstruction(cg, TR::InstOpCode::L, node, lengthReg, discontiguousArraySizeMR);

      TR::Instruction* returnInsturction = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, oolReturnLabel);

      if (cg->getDebug())
         {
         cg->getDebug()->addInstructionComment(returnInsturction, "End of OOL arraylength sequence");
         }

      outlinedDiscontigPath->swapInstructionListsWithCompilation();

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, oolReturnLabel);
      }

   cg->decReferenceCount(node->getFirstChild());
   node->setRegister(lengthReg);
   return lengthReg;
   }


///////////////////////////////////////////////////////////////////////////////////////
// resolveCHKEvaluator - Resolve check a static, field or method. child 1 is reference
//   to be resolved. Symbolref indicates failure action/destination
///////////////////////////////////////////////////////////////////////////////////////
   TR::Register *
J9::Z::TreeEvaluator::resolveCHKEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   // No code is generated for the resolve check. The child will reference an
   // unresolved symbol and all check handling is done via the corresponding
   // snippet.
   //
   TR::Node * firstChild = node->getFirstChild();
   bool fixRefCount = false;
   if (cg->comp()->useCompressedPointers())
      {
      // for stores under ResolveCHKs, artificially bump
      // down the reference count before evaluation (since stores
      // return null as registers)
      //
      if (node->getFirstChild()->getOpCode().isStoreIndirect() &&
            node->getFirstChild()->getReferenceCount() > 1)
         {
         node->getFirstChild()->decReferenceCount();
         fixRefCount = true;
         }
      }
   cg->evaluate(firstChild);
   if (fixRefCount)
      firstChild->incReferenceCount();

   cg->decReferenceCount(firstChild);
   return NULL;
   }


///////////////////////////////////////////////////////////////////////////////////////
// DIVCHKEvaluator - Divide by zero check. child 1 is the divide. Symbolref indicates
//    failure action/destination
///////////////////////////////////////////////////////////////////////////////////////
TR::Register *
J9::Z::TreeEvaluator::DIVCHKEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node * secondChild = node->getFirstChild()->getSecondChild();
   TR::DataType dtype = secondChild->getType();
   bool constDivisor = secondChild->getOpCode().isLoadConst();
   TR::Snippet * snippet;
   TR::LabelSymbol * snippetLabel = generateLabelSymbol(cg);
   TR::Instruction * cursor = NULL;   // Point to instruction that will assign targetReg
   TR::MemoryReference * divisorMr = NULL;

   bool divisorIsFieldAccess = false;
   bool willUseIndexAndBaseReg = false;
   if (secondChild->getNumChildren() != 0 &&
       secondChild->getOpCode().isMemoryReference() &&
       secondChild->getReferenceCount() == 1 &&
       secondChild->getRegister() == NULL)
      {
      divisorIsFieldAccess = (secondChild->getFirstChild()->getOpCodeValue() != TR::aladd &&
                              secondChild->getFirstChild()->getOpCodeValue() != TR::aiadd);
      // Defect 151061
      // The following comes from com/ibm/oti/vm/BootstrapClassLoader.addPackage
      // in hello world with a compressed pointers build
      //
      // [0x0000020007522994] (  0)  DIVCHK #11[0x000002000752293c]  Method[jitThrowArithmeticException]
      // [0x0000020007522904] (  2)    irem   <flags:"0x8000" (simpleDivCheck )/>
      // [0x000002000752262c] (  1)      iand   <flags:"0x1100" (X>=0 cannotOverflow )/>
      //                      (  3)        ==>icall at [0x00000200075223f8] (in GPR_0049)   <flags:"0x30" (arithmeticPreference invalid8BitGlobalRegister)/>
      // [0x00000200075225f4] (  1)        iconst 0x7fffffff   <flags:"0x104" (X!=0 X>=0 )/>
      // [0x00000200075228cc] (  1)      iiload #251[0x000002000745c940]+12  Shadow[<array-size>]   <flags:"0x1100" (X>=0 cannotOverflow )/>
      // [0x00000200074665d0] (  1)        l2a
      // [0x000002000745c908] (  1)          lshl   <flags:"0x800" (compressionSequence )/>
      //                      (  2)            ==>iu2l at [0x000002000745c8d0] (in GPR_0072)   <flags:"0x4" (X!=0 )/>
      // [0x000002000745c860] (  2)            iconst 1
      //
      // When generating a memref, because of the shift=1, the memref will use the same register
      // for the base and index register in order to avoid generating a shift instruction
      // But CLGHSI cannot take a memref which uses the index reg

      willUseIndexAndBaseReg = secondChild->getFirstChild() != NULL &&
         secondChild->getFirstChild()->getOpCodeValue() == TR::l2a &&
         secondChild->getFirstChild()->getFirstChild() != NULL &&
         secondChild->getFirstChild()->getFirstChild()->chkCompressionSequence() &&
         TR::Compiler->om.compressedReferenceShiftOffset() == 1;
      }

   bool disableS390CompareAndTrap = comp->getOption(TR_DisableTraps);

   // Try to compare directly to memory if if the child is a field access (load with no index reg)
   if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z10) && divisorIsFieldAccess &&
       !willUseIndexAndBaseReg &&
       (node->getFirstChild()->getOpCodeValue() == TR::idiv ||
        node->getFirstChild()->getOpCodeValue() == TR::irem))
      {
      divisorMr = generateS390MemoryReference(secondChild, cg);

      TR::InstOpCode::Mnemonic op = (dtype.isInt64())? TR::InstOpCode::CLGHSI : TR::InstOpCode::CLFHSI;
      generateSILInstruction(cg, op, node, divisorMr, 0);
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, snippetLabel);

      cursor->setExceptBranchOp();

      TR::Snippet * snippet = new (cg->trHeapMemory()) TR::S390HelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference());
      cg->addSnippet(snippet);
      }
   else if (cg->getHasResumableTrapHandler() && !disableS390CompareAndTrap)
      {
      TR::InstOpCode::Mnemonic op = (dtype.isInt64())? TR::InstOpCode::CLGIT : TR::InstOpCode::CLFIT;
      TR::Register * srcReg = cg->evaluate(secondChild);
      TR::S390RIEInstruction* cursor =
         new (cg->trHeapMemory()) TR::S390RIEInstruction(op, node, srcReg, (int16_t)0, TR::InstOpCode::COND_BE, cg);
      cursor->setExceptBranchOp();
      cg->setCanExceptByTrap(true);
      cursor->setNeedsGCMap(0x0000FFFF);
      if (TR::Compiler->target.isZOS())
         {
         killRegisterIfNotLocked(cg, TR::RealRegister::GPR4, cursor);
         }
      }
   // z9 legacy instructions
   else
      {
      // Generate explicit div by 0 test and snippet to jump to
      if (!constDivisor || (dtype.isInt32() && secondChild->getInt() == 0) || (dtype.isInt64() && secondChild->getLongInt() == 0))
         {
         // if divisor is a constant of zero, branch to the snippet to throw exception
         if (constDivisor)
            {
            cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, snippetLabel);
            cursor->setExceptBranchOp();
            }
         else
            {
            // if divisor is non-constant, need explicit test for 0
            TR::Register * srcReg;
            srcReg = cg->evaluate(secondChild);
            TR::InstOpCode::Mnemonic op = dtype.isInt64() ? TR::InstOpCode::LTGR : TR::InstOpCode::LTR;
            generateRRInstruction(cg, op, node, srcReg, srcReg);
            cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, snippetLabel);
            cursor->setExceptBranchOp();
            }
         TR::Snippet * snippet = new (cg->trHeapMemory()) TR::S390HelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference());
         cg->addSnippet(snippet);
         }
      }

   if (divisorMr)
      {
      switch (node->getFirstChild()->getOpCodeValue())
         {
         case TR::idiv:
            iDivRemGenericEvaluator(node->getFirstChild(), cg, true, divisorMr);
            break;
         case TR::irem:
            iDivRemGenericEvaluator(node->getFirstChild(), cg, false, divisorMr);
            break;
         }
      divisorMr->stopUsingMemRefRegister(cg);
      }
   else
      {
      cg->evaluate(node->getFirstChild());
      }
   cg->decReferenceCount(node->getFirstChild());

   return NULL;
   }


///////////////////////////////////////////////////////////////////////////////////////
// BNDCHKEvaluator - Array bounds check, checks that child 1 > child 2 >= 0
//   (child 1 is bound, 2 is index). Symbolref indicates failure action/destination
///////////////////////////////////////////////////////////////////////////////////////
TR::Register *
J9::Z::TreeEvaluator::BNDCHKEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();
   TR::LabelSymbol * boundCheckFailureLabel = generateLabelSymbol(cg);
   TR::Snippet * snippet;
   bool swap;
   TR::Instruction* cursor = NULL;
   TR::Compilation *comp = cg->comp();

   TR::Register * arrayLengthReg = firstChild->getRegister();
   TR::Register * arrayIndexReg = secondChild->getRegister();

   // skip l2i. Grab the low order register if it's a register pair.
   bool skipArrayLengthReg = false;
   bool skipArrayIndexReg = false;
   if (firstChild->getOpCodeValue() == TR::l2i &&
           firstChild->isSingleRefUnevaluated() &&
           firstChild->getFirstChild() &&
           firstChild->getFirstChild()->getRegister())
      {
      arrayLengthReg = firstChild->getFirstChild()->getRegister();
      skipArrayLengthReg = true;
      if(arrayLengthReg->getRegisterPair())
         {
         arrayLengthReg = arrayLengthReg->getRegisterPair()->getLowOrder();
         }
      }

   if (secondChild->getOpCodeValue() == TR::l2i &&
           secondChild->isSingleRefUnevaluated() &&
           secondChild->getFirstChild() &&
           secondChild->getFirstChild()->getRegister())
      {
      arrayIndexReg = secondChild->getFirstChild()->getRegister();
      skipArrayIndexReg = true;
      if(arrayIndexReg->getRegisterPair())
         {
         arrayIndexReg = arrayIndexReg->getRegisterPair()->getLowOrder();
         }
      }

   // use CLRT (RR) if possible
   bool useS390CompareAndTrap = !comp->getOption(TR_DisableTraps) && cg->getHasResumableTrapHandler();

   if (useS390CompareAndTrap &&
       (arrayIndexReg != NULL && arrayLengthReg != NULL))
      {
      //arrayIndex/arrayLength are max uint32, so 31 bit logical compare even in 64 bit JIT
      // The optimizer does not always fold away the BNDCHK if the index is a negative constant.
      // Explicit index<0 check is not needed here because negative array index is interpreted
      // as a large positive by the CLRT instruction.

      // ** Generate a NOP LR R0,R0.  The signal handler has to walk backwards to pattern match
      // the trap instructions.  All trap instructions besides CRT/CLRT are 6-bytes in length.
      // Insert 2-byte NOP in front of the 4-byte CLRT to ensure we do not mismatch accidentally.
      cursor = new (cg->trHeapMemory()) TR::S390NOPInstruction(TR::InstOpCode::NOP, 2, node, cg);

      TR::Instruction* cursor = generateRRFInstruction(cg, TR::InstOpCode::CLRT,
                                      node, arrayIndexReg, arrayLengthReg,
                                      getMaskForBranchCondition(TR::InstOpCode::COND_BNLR), true);
      cursor->setExceptBranchOp();
      cursor->setNeedsGCMap(0x0000FFFF);
      cg->setCanExceptByTrap(true);

      if (TR::Compiler->target.isZOS()) killRegisterIfNotLocked(cg, TR::RealRegister::GPR4, cursor);

      if (skipArrayLengthReg)
         {
         cg->decReferenceCount(firstChild->getFirstChild());
         }
      if (skipArrayIndexReg)
         {
         cg->decReferenceCount(secondChild->getFirstChild());
         }
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);

      return NULL;
      }
   else
      {
      // Perform a bound check.
      //
      // Value propagation or profile-directed optimization may have determined
      // that the array bound is a constant, and lowered TR::arraylength into an
      // iconst. In this case, make sure that the constant is the second child.
      //
      // Only type of scenario where first/second children are const is if we need it to force a branch
      //  otherwise simplifier should have cleaned it up

      /**
       *       Both Length and Index are constants
       */
      if (firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst())
         {
         int64_t secondChildConstValue = secondChild->get64bitIntegralValue();
         if (firstChild->getInt() > secondChildConstValue && secondChildConstValue >= 0)
            {
            //nothing to do since inside limit
            }
         else
            {
            // We must evaluate the non-const child if it has not been evaluated
            //
            if (!firstChild->getOpCode().isLoadConst() && firstChild->getRegister() == NULL)
               {
               cg->evaluate(firstChild);
               }

            // Check will always fail, just jump to failure snippet
            cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, boundCheckFailureLabel);
            cg->addSnippet(new (cg->trHeapMemory()) TR::S390HelperCallSnippet(cg, node, boundCheckFailureLabel, node->getSymbolReference()));
            cursor->setExceptBranchOp();
            }
         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);
         return NULL;
         }

      /**
       *       One of Length and Index is a constant
       */
      bool isForward = false;
      TR::Node * constNode = NULL;
      TR::Node * nonConstNode = NULL;
      bool oneConst = false; // exactly one child is a constant
      TR::Node * skippedL2iNode = NULL;

      if (firstChild->getOpCode().isLoadConst() || secondChild->getOpCode().isLoadConst())
         {
         oneConst = true;
         if (firstChild->getOpCode().isLoadConst())
            {
            isForward = false;
            constNode = firstChild;
            nonConstNode = secondChild;
            }
         else
            {
            isForward = true;
            constNode = secondChild;
            nonConstNode = firstChild;
            }

         skippedL2iNode = NULL;
         if (nonConstNode->getOpCodeValue() == TR::l2i &&
             nonConstNode->getRegister() == NULL &&
             nonConstNode->getReferenceCount() ==1)
            {
            skippedL2iNode = nonConstNode;
            nonConstNode = nonConstNode->getFirstChild();
            }
         }

      int64_t value = -1;
      int32_t constValue = -1;
      if (constNode)
         {
         value = getIntegralValue(constNode);
         constValue = constNode->getInt();
         }

      // always fail the BNDCHK if the index is negative.
      bool alwaysFailBNDCHK = oneConst && (constValue < 0) && isForward;

      if (oneConst &&
         constValue <= MAX_UNSIGNED_IMMEDIATE_VAL &&    // CLFIT takes 16bit unsigned immediate
         (constValue & 0xFF00) != 0xB900 &&             // signal handler might get confused with CLR (opcode 0xB973), etc
         useS390CompareAndTrap)
         {
         // Any constValue <= MAX_UNSIGNED_IMMEDIATE_VAL is taken here.
         // The length is assumed to be non-negative and is within [0, max_uint32] range.
         // The index can be negative or [0, max_uint32]. An unconditional branch is generated if it's negative.
         // No need to use unconditional BRC because it requires a proceeding NO-OP instruction for proper signal
         // handling. And NOP+BRC is of the same length as CLFIT.
         TR::Register * testRegister = cg->evaluate(nonConstNode);
         TR::InstOpCode::S390BranchCondition bc = alwaysFailBNDCHK ? TR::InstOpCode::COND_BRC :
                                                                     isForward ? TR::InstOpCode::COND_BNH :
                                                                                 TR::InstOpCode::COND_BNL ;

         TR::Instruction* cursor = generateRIEInstruction(cg, TR::InstOpCode::CLFIT,
                                                          node, testRegister, (int16_t)constValue, bc);


         cursor->setExceptBranchOp();
         cg->setCanExceptByTrap(true);
         cursor->setNeedsGCMap(0x0000FFFF);

         if (TR::Compiler->target.isZOS())
            {
            killRegisterIfNotLocked(cg, TR::RealRegister::GPR4, cursor);
            }

         if (skippedL2iNode)
            {
            cg->decReferenceCount(skippedL2iNode);
            }
         cg->decReferenceCount(constNode);
         cg->decReferenceCount(nonConstNode);

         return NULL;
         }
      else if (useS390CompareAndTrap &&
              (  (firstChild->getOpCode().isLoadVar() && firstChild->isSingleRefUnevaluated()) ||
                 (secondChild->getOpCode().isLoadVar() && secondChild->isSingleRefUnevaluated())) &&
              TR::Compiler->target.cpu.getSupportsArch(TR::CPU::zEC12))
         {
         // Assume 1st child is the memory operand.
         TR::Node * memChild = firstChild;
         TR::Node * regChild = secondChild;
         TR::InstOpCode::S390BranchCondition compareCondition = TR::InstOpCode::COND_BNL;

         // Check if first child is really the memory operand
         if (!(firstChild->getOpCode().isLoadVar() && firstChild->isSingleRefUnevaluated()))
            {
            // Nope... the second child is!
            memChild = secondChild;
            regChild = firstChild;
            compareCondition = TR::InstOpCode::COND_BNH;
            }

         // Ensure register operand is evaluated into register
         if (regChild->getRegister() == NULL)
            cg->evaluate(regChild);

         TR::InstOpCode::Mnemonic opCode = (regChild->getDataType()==TR::Int64) ? TR::InstOpCode::CLGT :
                                                                                  TR::InstOpCode::CLT;
         cursor = generateRSInstruction(cg, opCode,
                                         node, regChild->getRegister(),
                                         getMaskForBranchCondition(compareCondition),
                                         generateS390MemoryReference(memChild, cg));
         cursor->setExceptBranchOp();
         cg->setCanExceptByTrap(true);
         cursor->setNeedsGCMap(0x0000FFFF);

         if (TR::Compiler->target.isZOS())
            killRegisterIfNotLocked(cg, TR::RealRegister::GPR4, cursor);

         cg->decReferenceCount(memChild);
         cg->decReferenceCount(regChild);

         return NULL;
         }
      else if (oneConst)
         {
         TR::Register * testRegister = cg->evaluate(nonConstNode);
         TR::InstOpCode::S390BranchCondition bc = alwaysFailBNDCHK ? TR::InstOpCode::COND_BRC :
                                                                     isForward ? TR::InstOpCode::COND_BNH :
                                                                                 TR::InstOpCode::COND_BNL;
         TR::Instruction* cursor = NULL;

         if (alwaysFailBNDCHK)
            {
            cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, bc, node, boundCheckFailureLabel);
            }
         else
            {
            cursor = generateS390CompareAndBranchInstruction(cg,
                                                             TR::InstOpCode::CL,
                                                             node,
                                                             testRegister,
                                                             constValue,
                                                             bc,
                                                             boundCheckFailureLabel, false, true);
            }

         cursor->setExceptBranchOp();

         if (skippedL2iNode)
            {
            cg->decReferenceCount(skippedL2iNode);
            }
         cg->decReferenceCount(constNode);
         cg->decReferenceCount(nonConstNode);
         }


      // We assume that there is no GRA stuff hanging of this node
      TR_ASSERT( node->getNumChildren() < 3,"BNDCHK Eval: We are not expecting a third child on BNDCHK trees");

      /**
       *       Neither Length nor Index is constant
       */
      if (!oneConst)
         {
         // logical compare child1 (bound) and child2 (index).
         // Logical because all neg # > any pos # in unsigned form - for check that index > 0.
         // if child1 <= child2, branch on not high,
         // if the operands are switched, i.e.  compare child2 < child1, branch on high
         TR_S390BinaryCommutativeAnalyser temp(cg);
         temp.genericAnalyser(node, TR::InstOpCode::CLR, TR::InstOpCode::CL, TR::InstOpCode::LR, true);
         swap = temp.getReversedOperands();

         // There should be no register attached to the BNDCHK node, otherwise
         // the register would be kept live longer than it should.
         node->unsetRegister();
         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);

         // Generate compare code, find out if ops were reversed
         // MASK10 - reversed.  MASK12 - not reversed.
         TR::InstOpCode::Mnemonic brOp = TR::InstOpCode::BRC;
         TR::InstOpCode::S390BranchCondition brCond = (swap) ? TR::InstOpCode::COND_BNL : TR::InstOpCode::COND_BNH;
         cursor = generateS390BranchInstruction(cg, brOp, brCond, node, boundCheckFailureLabel);
         cursor->setExceptBranchOp();
         }

         cg->addSnippet(new (cg->trHeapMemory()) TR::S390HelperCallSnippet(cg, node, boundCheckFailureLabel, node->getSymbolReference()));
      }

   return NULL;
   }



///////////////////////////////////////////////////////////////////////////////////////
// ArrayCopyBNDCHKEvaluator - Array bounds check for arraycopy, checks that child 1 >= child 2
///////////////////////////////////////////////////////////////////////////////////////
TR::Register *
J9::Z::TreeEvaluator::ArrayCopyBNDCHKEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   // Check that first child >= second child
   //
   // If the first child is a constant and the second isn't, swap the children.
   //
   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();
   TR::LabelSymbol * boundCheckFailureLabel = generateLabelSymbol(cg);
   TR::Instruction * instr = NULL;
   bool useCIJ = false;
   TR::Compilation *comp = cg->comp();

   bool skipL2iArrayTargetLengthReg = false;
   bool skipL2iArrayCopyLengthReg = false;
   TR::Register * arrayTargetLengthReg = NULL;
   TR::Register * arrayCopyLengthReg = NULL;

   arrayTargetLengthReg = firstChild->getRegister();
   arrayCopyLengthReg = secondChild->getRegister();

   if (firstChild->getOpCodeValue() == TR::l2i &&
       firstChild->getFirstChild()->getRegister() != NULL &&
       firstChild->getReferenceCount() == 1 &&
       arrayTargetLengthReg == NULL)
      {
      skipL2iArrayTargetLengthReg = true;
      arrayTargetLengthReg = firstChild->getFirstChild()->getRegister();
      }

   if (secondChild->getOpCodeValue() == TR::l2i &&
       secondChild->getFirstChild()->getRegister() != NULL &&
       secondChild->getReferenceCount() == 1 &&
       arrayCopyLengthReg == NULL)
      {
      skipL2iArrayCopyLengthReg = true;
      arrayCopyLengthReg = secondChild->getFirstChild()->getRegister();
      }

   bool disableS390CompareAndTrap = comp->getOption(TR_DisableTraps);
   static const char*disableS390CompareAndBranch = feGetEnv("TR_DISABLES390CompareAndBranch");
   if (cg->getHasResumableTrapHandler() &&
       !disableS390CompareAndTrap &&
       arrayTargetLengthReg != NULL &&
       arrayCopyLengthReg != NULL )
      {
      //arrayIndex/arrayLength are max uint32, so 31 bit compare even in 64 bit JIT

      // Generate a NOP LR R0,R0.  The signal handler has to walk backwards to pattern match
      // the trap instructions.  All trap instructions besides CRT/CLRT are 6-bytes in length.
      // Insert 2-byte NOP in front of the 4-byte CRT to ensure we do not mismatch accidentally.
      TR::Instruction *cursor = new (cg->trHeapMemory()) TR::S390NOPInstruction(TR::InstOpCode::NOP, 2, node, cg);

      cursor = new (cg->trHeapMemory()) TR::S390RRFInstruction(TR::InstOpCode::CRT, node, arrayCopyLengthReg, arrayTargetLengthReg, getMaskForBranchCondition(TR::InstOpCode::COND_BH), true, cg);

      cursor->setExceptBranchOp();
      cg->setCanExceptByTrap(true);
      cursor->setNeedsGCMap(0x0000FFFF);
      if (TR::Compiler->target.isZOS()) killRegisterIfNotLocked(cg, TR::RealRegister::GPR4, cursor);

      if (skipL2iArrayTargetLengthReg)
         {
         cg->decReferenceCount(firstChild->getFirstChild());
         }
      if (skipL2iArrayCopyLengthReg)
         {
         cg->decReferenceCount(secondChild->getFirstChild());
         }
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);

      return NULL;
      }
   else
      {
      if (firstChild->getOpCode().isLoadConst())
         {
         if (secondChild->getOpCode().isLoadConst())
            {
            if (firstChild->getInt() < secondChild->getInt())
               {
               // Check will always fail, just jump to failure snippet
               //
               instr = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, boundCheckFailureLabel);
               instr->setExceptBranchOp();
               }
            else
               {
               // Check will always succeed, no need for an instruction
               //
               instr = NULL;
               }
            cg->decReferenceCount(firstChild);
            cg->decReferenceCount(secondChild);
            }
         else
            {
            int32_t arrayTargetLengthConst = firstChild->getInt();

            // CIT uses 16-bit immediates
            if (cg->getHasResumableTrapHandler() &&
                arrayTargetLengthConst <= MAX_IMMEDIATE_VAL &&
                arrayTargetLengthConst >= MIN_IMMEDIATE_VAL &&
                (arrayTargetLengthConst & 0xFF00) != 0xB900 && // signal handler might get confused with CRT (opcode 0xB972), etc
                !disableS390CompareAndTrap )
               {
               if (arrayCopyLengthReg == NULL)
                  {
                  arrayCopyLengthReg = cg->evaluate(secondChild);
                  }

               TR::S390RIEInstruction* cursor =
                  new (cg->trHeapMemory()) TR::S390RIEInstruction(TR::InstOpCode::CIT, node, arrayCopyLengthReg, (int16_t)arrayTargetLengthConst, TR::InstOpCode::COND_BH, cg);
               cursor->setExceptBranchOp();
               cursor->setNeedsGCMap(0x0000FFFF);
               cg->setCanExceptByTrap(true);
               if (TR::Compiler->target.isZOS()) killRegisterIfNotLocked(cg, TR::RealRegister::GPR4, cursor);

               if (skipL2iArrayCopyLengthReg)
                  {
                  cg->decReferenceCount(secondChild->getFirstChild());
                  }
               cg->decReferenceCount(firstChild);
               cg->decReferenceCount(secondChild);

               return NULL;
               }
            // check if we can use Compare-and-Branch at least
            else if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z10) &&
                     arrayTargetLengthConst <= MAX_IMMEDIATE_BYTE_VAL &&
                     arrayTargetLengthConst >= MIN_IMMEDIATE_BYTE_VAL &&
                     !disableS390CompareAndBranch)
               {
               useCIJ = true;
               if (arrayCopyLengthReg == NULL)
                  {
                  arrayCopyLengthReg = cg->evaluate(secondChild);
                  }

               TR::Instruction* cursor =
                       generateS390CompareAndBranchInstruction(cg,
                                                               TR::InstOpCode::C,
                                                               node,
                                                               arrayCopyLengthReg,
                                                               arrayTargetLengthConst,
                                                               TR::InstOpCode::COND_BH,
                                                               boundCheckFailureLabel,
                                                               false,
                                                               false,
                                                               NULL,
                                                               NULL);
               cursor->setExceptBranchOp();

               if (skipL2iArrayCopyLengthReg)
                  {
                  cg->decReferenceCount(secondChild->getFirstChild());
                  }
               cg->decReferenceCount(firstChild);
               cg->decReferenceCount(secondChild);
               }
            // z9 Instructions
            else
               {
               node->swapChildren();
               instr = generateS390CompareBranchLabel(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BH, TR::InstOpCode::COND_BL, boundCheckFailureLabel);
               node->swapChildren();
               instr->setExceptBranchOp();
               }
            }
         }
      else
         {
         // The first child is not loadConstant
         // CIT uses 16-bit immediates
         if (secondChild->getOpCode().isLoadConst() &&
             cg->getHasResumableTrapHandler() &&
             secondChild->getInt() <= MAX_IMMEDIATE_VAL &&
             secondChild->getInt() >= MIN_IMMEDIATE_VAL &&
             (secondChild->getInt() & 0xFF00) != 0xB900 && // signal handler might get confused with CRT (opcode 0xB972), etc
             !disableS390CompareAndTrap )
            {
            int32_t arrayCopyLengthConst = secondChild->getInt();
            if (arrayTargetLengthReg == NULL)
               {
               arrayTargetLengthReg = cg->evaluate(firstChild);
               }

            TR::S390RIEInstruction* cursor =
               new (cg->trHeapMemory()) TR::S390RIEInstruction(TR::InstOpCode::CIT, node, arrayTargetLengthReg, (int16_t)arrayCopyLengthConst, TR::InstOpCode::COND_BL, cg);
            cursor->setExceptBranchOp();
            cursor->setNeedsGCMap(0x0000FFFF);
            cg->setCanExceptByTrap(true);
            if (TR::Compiler->target.isZOS()) killRegisterIfNotLocked(cg, TR::RealRegister::GPR4, cursor);

            if (skipL2iArrayTargetLengthReg)
               {
               cg->decReferenceCount(firstChild->getFirstChild());
               }
            cg->decReferenceCount(firstChild);
            cg->decReferenceCount(secondChild);

            return NULL;
            }
         // check if we can use Compare-and-Branch at least
         else if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z10) &&
                  secondChild->getOpCode().isLoadConst() &&
                  secondChild->getInt() <= MAX_IMMEDIATE_BYTE_VAL &&
                  secondChild->getInt() >= MIN_IMMEDIATE_BYTE_VAL &&
                  !disableS390CompareAndBranch)
            {
            int32_t arrayCopyLengthConst = secondChild->getInt();
            if (arrayTargetLengthReg == NULL)
               {
               arrayTargetLengthReg = cg->evaluate(firstChild);
               }

            useCIJ = true;
            TR::Instruction* cursor =
                    generateS390CompareAndBranchInstruction(cg,
                                                            TR::InstOpCode::C,
                                                            node,
                                                            arrayTargetLengthReg,
                                                            arrayCopyLengthConst,
                                                            TR::InstOpCode::COND_BL,
                                                            boundCheckFailureLabel,
                                                            false,
                                                            false,
                                                            NULL,
                                                            NULL);

            cursor->setExceptBranchOp();

            if (skipL2iArrayTargetLengthReg)
               {
               cg->decReferenceCount(firstChild->getFirstChild());
               }
            cg->decReferenceCount(firstChild);
            cg->decReferenceCount(secondChild);
            }
         // z9
         else
            {
            instr = generateS390CompareOps(node, cg, TR::InstOpCode::COND_BL, TR::InstOpCode::COND_BH, boundCheckFailureLabel);

            instr->setExceptBranchOp();
            }
         }

      if (instr || useCIJ)
         {
         cg->addSnippet(new (cg->trHeapMemory()) TR::S390HelperCallSnippet(cg, node, boundCheckFailureLabel, node->getSymbolReference()));
         }
      }

   return NULL;
   }

void
J9::Z::TreeEvaluator::generateFillInDataBlockSequenceForUnresolvedField(TR::CodeGenerator *cg, TR::Node *node, TR::Snippet *dataSnippet, bool isWrite, TR::Register *sideEffectRegister)
   {
   TR::LabelSymbol *unresolvedLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *mergePointLabel = generateLabelSymbol(cg);
   TR::SymbolReference *symRef = node->getSymbolReference();
   bool isStatic = symRef->getSymbol()->getKind() == TR::Symbol::IsStatic;

   TR::Register *offsetReg = cg->allocateRegister();
   TR::Register *dataBlockReg = cg->allocateRegister();

   generateRILInstruction(cg, TR::InstOpCode::LARL, node, dataBlockReg, dataSnippet);

   intptr_t offsetInDataBlock = isStatic ? offsetof(J9JITWatchedStaticFieldData, fieldAddress) : offsetof(J9JITWatchedInstanceFieldData, offset);
   generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, offsetReg, generateS390MemoryReference(dataBlockReg, offsetInDataBlock, cg));
   // If the offset is not -1 then the field is already resolved. No more work is required and we can fall through to end (mergePointLabel).
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, offsetReg, -1, TR::InstOpCode::COND_BE, unresolvedLabel, false, false, NULL, NULL);

   // If the offset is -1, then we must call a VM helper routine (indicated by helperLink below) to resolve this field. The OOL code (below) inside unresolvedLabel
   // will prepare the registers and generate a directCall to the VM helper routine.
   TR_S390OutOfLineCodeSection *outlinedSlowPath = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(unresolvedLabel, mergePointLabel, cg);
   cg->getS390OutOfLineCodeSectionList().push_front(outlinedSlowPath);
   outlinedSlowPath->swapInstructionListsWithCompilation();

   // OOL code start.
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, unresolvedLabel);
 
   if (isStatic)
      {
      // Fills in J9JITWatchedStaticFieldData.fieldClass.
      TR::Register *fieldClassReg;
      if (isWrite)
         {
         fieldClassReg = cg->allocateRegister();
         generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, fieldClassReg, generateS390MemoryReference(sideEffectRegister, cg->comp()->fej9()->getOffsetOfClassFromJavaLangClassField(), cg));
         }
      else
         {
         fieldClassReg = sideEffectRegister;
         }
      generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, fieldClassReg, generateS390MemoryReference(dataBlockReg, offsetof(J9JITWatchedStaticFieldData, fieldClass), cg));
      if (isWrite)
         {
         cg->stopUsingRegister(fieldClassReg);
         }
      }

   // These will be used as argument registers for the direct call to the VM helper.
   TR::Register *cpAddressReg = cg->allocateRegister();
   TR::Register *cpIndexReg = cg->allocateRegister();

   // Populate the argument registers.
   TR::ResolvedMethodSymbol *methodSymbol = node->getByteCodeInfo().getCallerIndex() == -1 ? cg->comp()->getMethodSymbol() : cg->comp()->getInlinedResolvedMethodSymbol(node->getByteCodeInfo().getCallerIndex());
   generateRegLitRefInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, cpAddressReg, reinterpret_cast<uintptrj_t>(methodSymbol->getResolvedMethod()->constantPool()), TR_ConstantPool, NULL, 0, 0);
   generateRILInstruction(cg, TR::InstOpCode::LGFI, node, cpIndexReg, symRef->getCPIndex());

   TR_RuntimeHelper helperIndex = isWrite? (isStatic ? TR_jitResolveStaticFieldSetterDirect: TR_jitResolveFieldSetterDirect) :
                                           (isStatic ? TR_jitResolveStaticFieldDirect: TR_jitResolveFieldDirect);
   TR::S390HelperLinkage *helperLink = static_cast<TR::S390HelperLinkage*>(cg->getLinkage(runtimeHelperLinkage(helperIndex)));


   // We specify 2 preConditions because we need to provide 2 register arguments.
   // We specify 4 postConditions because both of the argument registers need to be specified as
   // register dependencies (GPR 1 as a dummy dependency and GPR2 is a return register), and we
   // need to specify 2 more register dependencies for Entry Point and Return Address register
   // when making a direct call.
   TR::RegisterDependencyConditions *deps =  generateRegisterDependencyConditions(2, 4, cg);
   int numArgs = 0;

   // The VM helper routine that we call expects cpAddress to be in GPR1 and cpIndex inside GPR2.
   // So we set those dependencies here.
   deps->addPreCondition(cpAddressReg, helperLink->getIntegerArgumentRegister(numArgs));
   deps->addPostCondition(cpAddressReg, helperLink->getIntegerArgumentRegister(numArgs));
   numArgs++;

   // Add pre and post condition because GPR2 is an argument register as well as return register.
   deps->addPreCondition(cpIndexReg, helperLink->getIntegerArgumentRegister(numArgs));
   deps->addPostCondition(cpIndexReg, helperLink->getIntegerReturnRegister()); // cpIndexReg (i.e. GPR2) will also hold the return value of the helper routine call.

   // These two registers are used for Return Address and Entry Point registers. These dependencies are required when generating directCalls on Z.
   TR::Register *scratchReg1 = cg->allocateRegister();
   TR::Register *scratchReg2 = cg->allocateRegister();
   deps->addPostCondition(scratchReg1, cg->getEntryPointRegister());
   deps->addPostCondition(scratchReg2, cg->getReturnAddressRegister());

   // Now make the call. Return value of the call is in GPR2 (cpIndexReg).
   TR::Instruction *call = generateDirectCall(cg, node, false /*myself*/, cg->symRefTab()->findOrCreateRuntimeHelper(helperIndex, false, false, false), deps);
   call->setNeedsGCMap(0x0000FFFF);
   call->setDependencyConditions(deps);

   // For instance fields, the offset (i.e. result value) returned by the vmhelper includes the header size.
   // We subtract the header size from the return value here to get the actual offset.
   if (!isStatic)
      {
      generateRILInstruction(cg, TR::InstOpCode::getSubtractLogicalImmOpCode(), node, cpIndexReg, static_cast<uint32_t>(TR::Compiler->om.objectHeaderSizeInBytes()));
      }

   // Store the field value into the data snippet to resolve it.
   generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, cpIndexReg, generateS390MemoryReference(dataBlockReg, offsetInDataBlock, cg));

   // End of OOL code. Branch back to mainline.
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, mergePointLabel);
   outlinedSlowPath->swapInstructionListsWithCompilation();
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, mergePointLabel);

   cg->stopUsingRegister(cpIndexReg);
   cg->stopUsingRegister(scratchReg1);
   cg->stopUsingRegister(scratchReg2);
   cg->stopUsingRegister(cpAddressReg);
   cg->stopUsingRegister(dataBlockReg);
   cg->stopUsingRegister(offsetReg);
   }

/*
 * This method will prepare the registers and then make a VM Helper call to report that a fieldwatch event has occurred 
 * in a Java class with field watch enabled.
 *
 * The possible VM Helpers are:
 *
 * For indirect nodes (i.e. instance fields):
 *    jitReportInstanceFieldRead (if node is indirect)
 *      arg1 pointer to static data block
 *      arg2 object being read
 *
 *    jitReportInstanceFieldWrite (if node is indirect)
 *      arg1 pointer to static data block
 *      arg2 object being written to (represented by sideEffectRegister)
 *      arg3 pointer to value being written
 *
 * For direct nodes (i.e. static fields):
 *    jitReportStaticFieldRead (for direct/static nodes)
 *      arg1 pointer to static data block
 *
 *    jitReportStaticFieldWrite
 *      arg1 pointer to static data block
 *      arg2 pointer to value being written
 */
void generateReportFieldAccessOutlinedInstructions(TR::Node *node, TR::LabelSymbol *fieldReportLabel, TR::LabelSymbol *mergePointLabel, TR::Snippet *dataSnippet, bool isWrite, TR::CodeGenerator *cg, TR::Register *sideEffectRegister, TR::Register *valueReg)
   {
   bool isInstanceField = node->getSymbolReference()->getSymbol()->getKind() != TR::Symbol::IsStatic;
   // Figure out the VM Helper we need to call.
   TR_RuntimeHelper helperIndex = isWrite ? (isInstanceField ? TR_jitReportInstanceFieldWrite: TR_jitReportStaticFieldWrite):
                                            (isInstanceField ? TR_jitReportInstanceFieldRead: TR_jitReportStaticFieldRead);

   // Figure out the number of dependencies needed to make the VM Helper call.
   // numPreConditions is equal to the number of arguments required by the VM Helper.
   uint8_t numPreConditions = 1; // All helpers need at least one parameter.
   if (helperIndex == TR_jitReportInstanceFieldWrite)
      {
      numPreConditions = 3;
      }
   else if (helperIndex == TR_jitReportInstanceFieldRead || helperIndex == TR_jitReportStaticFieldWrite)
      {
      numPreConditions = 2;
      }
   // Note: All preConditions need to be added as post dependencies (dummy dependencies). We also need to specify 2 more
   // post dependencies for Return Address register and Entry Point register.
   TR::RegisterDependencyConditions *dependencies = generateRegisterDependencyConditions(numPreConditions, numPreConditions + 2, cg);
   TR::S390HelperLinkage *helperLink = static_cast<TR::S390HelperLinkage*>(cg->getLinkage(runtimeHelperLinkage(helperIndex)));
   int numArgs = 0;

   // Initialize OOL path and generate label that marks beginning of the OOL code.
   TR_S390OutOfLineCodeSection *outlinedSlowPath = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(fieldReportLabel, mergePointLabel, cg);
   cg->getS390OutOfLineCodeSectionList().push_front(outlinedSlowPath);
   outlinedSlowPath->swapInstructionListsWithCompilation();
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, fieldReportLabel);

   // Populate the first argument needed by the VM Helper (address to the data snippet), and set the dependencies.
   TR::Register *dataBlockReg = cg->allocateRegister();
   generateRILInstruction(cg, TR::InstOpCode::LARL, node, dataBlockReg, dataSnippet);
   dependencies->addPreCondition(dataBlockReg, helperLink->getIntegerArgumentRegister(numArgs));
   dependencies->addPostCondition(dataBlockReg, helperLink->getIntegerArgumentRegister(numArgs));
   dataBlockReg->setPlaceholderReg();
   numArgs++;

   // Populate the next argument if needed.
   TR::Register *objectReg = NULL;
   if (isInstanceField)
      {
      dependencies->addPreCondition(sideEffectRegister, helperLink->getIntegerArgumentRegister(numArgs));
      dependencies->addPostCondition(sideEffectRegister, helperLink->getIntegerArgumentRegister(numArgs));
      sideEffectRegister->setPlaceholderReg();
      numArgs++;
      }

   // Populate the final argument if needed.
   // Note: In the event that we have to write to a value, the VM helper routine expects that a pointer to the value being written to
   // is passed in as a parameter. So we must store the value into memory and then load the address back into a register in order
   // to pass the address of that value as an argument. We prepare the register below.
   if (isWrite)
      {
      TR::Node *valueNode = node->getFirstChild();
      if (isInstanceField)
         {
         // Pass in valueNode so it can be set to the correct node.
         TR::TreeEvaluator::getIndirectWrtbarValueNode(cg, node, valueNode, false);
         }

      // First load the actual value into the register.
      TR::Register *valueReferenceReg = valueReg;

      TR::DataType nodeType = valueNode->getDataType();
      TR::SymbolReference *sr = cg->allocateLocalTemp(nodeType);
      TR::MemoryReference *valueMR = generateS390MemoryReference(valueNode, sr, cg);
      if (valueReferenceReg->getKind() == TR_GPR)
         {
         // Use STG if the dataType is an uncompressed TR::Address or TR::Int64. ST otherwise.
         auto mnemonic = TR::DataType::getSize(nodeType) == 8 ? TR::InstOpCode::STG : TR::InstOpCode::ST;
         // Now store the value onto the stack.
         generateRXInstruction(cg, mnemonic, node, valueReferenceReg, valueMR);
         }
      else if (valueReferenceReg->getKind() == TR_FPR)
         {
         auto mnemonic = nodeType == TR::Float ? TR::InstOpCode::STE : TR::InstOpCode::STD;
         // Now store the value onto the stack.
         generateRXInstruction(cg, mnemonic, node, valueReferenceReg, valueMR);
         }
      else
         {
         TR_ASSERT_FATAL(false, "Unsupported register kind (%d) for fieldwatch.", valueReferenceReg->getKind());
         }
      valueReferenceReg = cg->allocateRegister();

      // Now load the memory location back into the register so that it can be used
      // as an argument register for the VM helper call.
      TR::MemoryReference *tempMR = generateS390MemoryReference(*valueMR, 0, cg);
      generateRXInstruction(cg, TR::InstOpCode::LA, node, valueReferenceReg, tempMR);

      dependencies->addPreCondition(valueReferenceReg, helperLink->getIntegerArgumentRegister(numArgs));
      dependencies->addPostCondition(valueReferenceReg, helperLink->getIntegerArgumentRegister(numArgs));
      valueReferenceReg->setPlaceholderReg();

      cg->stopUsingRegister(valueReferenceReg);
      }

   // These registers will hold Entry Point and Return Address registers, which are required when generating a directCall.
   TR::Register *scratch1 = cg->allocateRegister();
   TR::Register *scratch2 = cg->allocateRegister();
   dependencies->addPostCondition(scratch1, cg->getEntryPointRegister());
   dependencies->addPostCondition(scratch2, cg->getReturnAddressRegister());

   // Now generate the call to VM Helper to report the fieldwatch.
   TR::Instruction *call = generateDirectCall(cg, node, false /*myself*/, cg->symRefTab()->findOrCreateRuntimeHelper(helperIndex, false, false, false), dependencies);
   call->setNeedsGCMap(0x0000FFFF);
   call->setDependencyConditions(dependencies);

   // After returning from the VM Helper, branch back to mainline code.
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, mergePointLabel);
   // End of OOL.
   outlinedSlowPath->swapInstructionListsWithCompilation();

   cg->stopUsingRegister(scratch1);
   cg->stopUsingRegister(scratch2);

   cg->stopUsingRegister(dataBlockReg);
   }

void
J9::Z::TreeEvaluator::generateTestAndReportFieldWatchInstructions(TR::CodeGenerator *cg, TR::Node *node, TR::Snippet *dataSnippet, bool isWrite, TR::Register *sideEffectRegister, TR::Register *valueReg)
   {
   bool isResolved = !node->getSymbolReference()->isUnresolved();
   TR::LabelSymbol *mergePointLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *fieldReportLabel = generateLabelSymbol(cg);

   TR::Register *fieldClassReg;
   TR::Register *fieldClassFlags = cg->allocateRegister();
   bool opCodeIsIndirect = node->getOpCode().isIndirect();

   if (opCodeIsIndirect)
      {
      // Load the class of the instance object into fieldClassReg.
      fieldClassReg = cg->allocateRegister();
      TR::TreeEvaluator::genLoadForObjectHeadersMasked(cg, node, fieldClassReg, generateS390MemoryReference(sideEffectRegister, static_cast<int32_t>(TR::Compiler->om.offsetOfObjectVftField()), cg), NULL);
      }
   else
      {
      if (isResolved)
         {
         fieldClassReg = cg->allocateRegister();
         if (!(cg->comp()->compileRelocatableCode()))
            {
            // For non-AOT compiles we don't need to use sideEffectRegister here as the class information is available to us at compile time.
            J9Class *fieldClass = static_cast<TR::J9WatchedStaticFieldSnippet *>(dataSnippet)->getFieldClass();
            TR_ASSERT_FATAL(fieldClass != NULL, "A valid J9Class must be provided for direct rdbar/wrtbar opcodes %p\n", node);
            generateRILInstruction(cg, TR::InstOpCode::LARL, node, fieldClassReg, static_cast<void *>(fieldClass));
            }
         else
            {
            // If this is an AOT compile, we generate instructions to load the fieldClass directly from the snippet because the fieldClass will be invalid
            // if we load using the dataSnippet's helper query at compile time.
            generateRILInstruction(cg, TR::InstOpCode::LARL, node, fieldClassReg, dataSnippet);
            generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, fieldClassReg, generateS390MemoryReference(fieldClassReg, offsetof(J9JITWatchedStaticFieldData, fieldClass), cg));
            }
         }
      else
         {
         if (isWrite)
            {
            fieldClassReg = cg->allocateRegister();
            generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, fieldClassReg, generateS390MemoryReference(sideEffectRegister, cg->comp()->fej9()->getOffsetOfClassFromJavaLangClassField(), cg));
            }
         else
            {
            fieldClassReg = sideEffectRegister;
            }
         }
      }
   // First load the class flags into a register.
   generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, fieldClassFlags, generateS390MemoryReference(fieldClassReg, cg->comp()->fej9()->getOffsetOfClassFlags(), cg));
   // Then test the bit to test with the relevant flag to check if fieldwatch is enabled.
   generateRIInstruction(cg, TR::InstOpCode::TMLL, node, fieldClassFlags, J9ClassHasWatchedFields);
   // If Condition Code from above test is not 0, then we branch to OOL (instructions) to report the fieldwatch event. Otherwise fall through to mergePointLabel.
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRNZ, node, fieldReportLabel);

   // Generate instructions to call a VM Helper and report the fieldwatch event. Also generates an instruction to
   // branch back to mainline (mergePointLabel).
   generateReportFieldAccessOutlinedInstructions(node, fieldReportLabel, mergePointLabel, dataSnippet, isWrite, cg, sideEffectRegister, valueReg);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, mergePointLabel);

   if (opCodeIsIndirect || isResolved || isWrite)
      {
      cg->stopUsingRegister(fieldClassReg);
      }

   cg->stopUsingRegister(fieldClassFlags);
   }

TR::Register *
J9::Z::TreeEvaluator::irdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Node *sideEffectNode = node->getFirstChild();
   TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, NULL);
      }
   cg->decReferenceCount(sideEffectNode);
   return TR::TreeEvaluator::iloadEvaluator(node, cg);
   }

TR::Register *
J9::Z::TreeEvaluator::irdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Register *sideEffectRegister = cg->evaluate(node->getFirstChild());

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, NULL);
      }

   // Note: For indirect rdbar nodes, the first child (sideEffectNode) is also used by the
   // load evaluator. The load evaluator will also evaluate+decrement it. In order to avoid double
   // decrementing the node we skip doing it here and let the load evaluator do it.
   return TR::TreeEvaluator::iloadEvaluator(node, cg);
   }

TR::Register *
J9::Z::TreeEvaluator::ardbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Node *sideEffectNode = node->getFirstChild();
   TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, NULL);
      }
   cg->decReferenceCount(sideEffectNode);
   return TR::TreeEvaluator::aloadEvaluator(node, cg);
   }

TR::Register *
J9::Z::TreeEvaluator::ardbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Register *sideEffectRegister = cg->evaluate(node->getFirstChild());

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, NULL);
      }

   TR::Register* resultReg = NULL;
   if (TR::Compiler->om.readBarrierType() != gc_modron_readbar_none)
      {
      bool dynLitPoolLoad = false;
      resultReg = TR::TreeEvaluator::checkAndAllocateReferenceRegister(node, cg, dynLitPoolLoad);
      // MemRef can generate BRCL to unresolved data snippet if needed.
      TR::MemoryReference* loadMemRef = generateS390MemoryReference(node, cg);

      if (TR::Compiler->target.cpu.getSupportsGuardedStorageFacility())
         {
         TR::TreeEvaluator::checkAndSetMemRefDataSnippetRelocationType(node, cg, loadMemRef);
         TR::InstOpCode::Mnemonic loadOp = cg->comp()->useCompressedPointers() ? TR::InstOpCode::LLGFSG : TR::InstOpCode::LGG;
         generateRXInstruction(cg, loadOp, node, resultReg, loadMemRef);
         }
      else
         {
         TR::TreeEvaluator::generateSoftwareReadBarrier(node, cg, resultReg, loadMemRef);
         }
      node->setRegister(resultReg);
      }
   else
      {
      resultReg = TR::TreeEvaluator::aloadEvaluator(node, cg);
      }
   // Note: For indirect rdbar nodes, the first child (sideEffectNode) is also used by the
   // load evaluator. The load evaluator will also evaluate+decrement it. In order to avoid double
   // decrementing the node we skip doing it here and let the load evaluator do it.
   return resultReg;
   }

TR::Register *
J9::Z::TreeEvaluator::fwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Register *valueReg = cg->evaluate(node->getSecondChild());
   TR::Node *sideEffectNode = node->getThirdChild();
   TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, valueReg);
      }

   // Note: The reference count for valueReg's node is not decremented here because the
   // store evaluator also uses it and so it will evaluate+decrement it. Thus we must skip decrementing here
   // to avoid double decrementing.
   cg->decReferenceCount(sideEffectNode);
   return TR::TreeEvaluator::fstoreEvaluator(node, cg);
   }

TR::Register *
J9::Z::TreeEvaluator::fwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Register *valueReg = cg->evaluate(node->getFirstChild());
   TR::Node *sideEffectNode = node->getSecondChild();
   TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, valueReg);
      }

   // Note: The reference count for valueReg's node is not decremented here because the
   // store evaluator also uses it and so it will evaluate+decrement it. Thus we must skip decrementing here
   // to avoid double decrementing.
   cg->decReferenceCount(sideEffectNode);
   return TR::TreeEvaluator::fstoreEvaluator(node, cg);
   }

TR::Register *
J9::Z::TreeEvaluator::dwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Register *valueReg = cg->evaluate(node->getSecondChild());
   TR::Node *sideEffectNode = node->getThirdChild();
   TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, valueReg);
      }

   // Note: The reference count for valueReg's node is not decremented here because the
   // store evaluator also uses it and so it will evaluate+decrement it. Thus we must skip decrementing here
   // to avoid double decrementing.
   cg->decReferenceCount(sideEffectNode);
   return TR::TreeEvaluator::dstoreEvaluator(node, cg);
   }

TR::Register *
J9::Z::TreeEvaluator::dwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // For rdbar and wrtbar nodes we first evaluate the children we need to
   // handle the side effects. Then we delegate the evaluation of the remaining
   // children and the load/store operation to the appropriate load/store evaluator.
   TR::Register *valueReg = cg->evaluate(node->getFirstChild());
   TR::Node *sideEffectNode = node->getSecondChild();
   TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

   if (cg->comp()->getOption(TR_EnableFieldWatch))
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, valueReg);
      }

   // Note: The reference count for valueReg's node is not decremented here because the
   // store evaluator also uses it and so it will evaluate+decrement it. Thus we must skip decrementing here
   // to avoid double decrementing.
   cg->decReferenceCount(sideEffectNode);
   return TR::TreeEvaluator::dstoreEvaluator(node, cg);
   }

TR::Register *
J9::Z::TreeEvaluator::awrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::awrtbariEvaluator(node, cg);
   }

TR::Register *
J9::Z::TreeEvaluator::awrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *owningObjectChild;
   TR::Node *sourceChild;
   TR::Compilation *comp = cg->comp();
   bool opCodeIsIndirect = node->getOpCode().isIndirect();
   if (opCodeIsIndirect)
      {
      owningObjectChild = node->getChild(2);
      sourceChild = node->getSecondChild();
      }
   else
      {
      owningObjectChild = node->getSecondChild();
      sourceChild = node->getFirstChild();
      }

   bool usingCompressedPointers = false;
   if (opCodeIsIndirect)
      {
      // Pass in valueNode so it can be set to the correct node. If the sourceChild is modified, usingCompressedPointers will be true.
      usingCompressedPointers = TR::TreeEvaluator::getIndirectWrtbarValueNode(cg, node, sourceChild, true);
      }

   bool doWrtBar = (TR::Compiler->om.writeBarrierType() == gc_modron_wrtbar_oldcheck ||
                    TR::Compiler->om.writeBarrierType() == gc_modron_wrtbar_cardmark_and_oldcheck ||
                    TR::Compiler->om.writeBarrierType() == gc_modron_wrtbar_always);
   bool doCrdMrk = ((TR::Compiler->om.writeBarrierType() == gc_modron_wrtbar_cardmark ||
                     TR::Compiler->om.writeBarrierType() == gc_modron_wrtbar_cardmark_incremental ||
                     TR::Compiler->om.writeBarrierType() == gc_modron_wrtbar_cardmark_and_oldcheck) && !node->isNonHeapObjectWrtBar());

   bool canSkip = false;
   TR::Register *owningObjectRegister = NULL;
   TR::Register *sourceRegister = NULL;

   if ((node->getOpCode().isWrtBar() && node->skipWrtBar()) ||
         ((node->getOpCodeValue() == TR::ArrayStoreCHK) &&
         node->getFirstChild()->getOpCode().isWrtBar() &&
         node->getFirstChild()->skipWrtBar()))
      {
      canSkip = true;
      }

   if ((doWrtBar || doCrdMrk) && !canSkip)
      {
      owningObjectRegister = cg->gprClobberEvaluate(owningObjectChild);
      }
   else
      {
      owningObjectRegister = cg->evaluate(owningObjectChild);
      }

   if (canSkip || opCodeIsIndirect)
      {
      sourceRegister = cg->evaluate(sourceChild);
      }
   else
      {
      sourceRegister = allocateWriteBarrierInternalPointerRegister(cg, sourceChild);
      }

   TR::Register * compressedRegister = sourceRegister;
   if (usingCompressedPointers)
      {
      compressedRegister = cg->evaluate(node->getSecondChild());
      }

   // Handle fieldwatch side effect first if it's enabled.
   if (cg->comp()->getOption(TR_EnableFieldWatch) && !node->getSymbolReference()->getSymbol()->isArrayShadowSymbol())
      {
      TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, owningObjectRegister /* sideEffectRegister */, sourceRegister /* valueReg */);
      }

   // We need to evaluate all the children first before we generate memory reference
   // since it will screw up the code sequence for patching when we do symbol resolution.
   TR::MemoryReference *tempMR = generateS390MemoryReference(node, cg);
   TR::InstOpCode::Mnemonic storeOp = usingCompressedPointers ? TR::InstOpCode::ST : TR::InstOpCode::getStoreOpCode();
   TR::Instruction * instr = generateRXInstruction(cg, storeOp, node, opCodeIsIndirect ? compressedRegister : sourceRegister, tempMR);

   // When a new object is stored into an old object, we need to invoke jitWriteBarrierStore
   // helper to update the remembered sets for GC.  Helper call is needed only if the object
   // is in old space or is scanned (black). Since the checking involves control flow, we delay
   // the code gen for write barrier since RA cannot handle control flow.
   VMwrtbarEvaluator(node, sourceRegister, owningObjectRegister, sourceChild->isNonNull(), cg);

   if (opCodeIsIndirect && comp->useCompressedPointers())
      {
      node->setStoreAlreadyEvaluated(true);
      }

   cg->decReferenceCount(sourceChild);
   if (usingCompressedPointers)
      {
      cg->decReferenceCount(node->getSecondChild());
      cg->recursivelyDecReferenceCount(owningObjectChild);
      }
   else
      {
      cg->decReferenceCount(owningObjectChild);
      }

   if (owningObjectRegister)
      {
      cg->stopUsingRegister(owningObjectRegister);
      }
   cg->stopUsingRegister(sourceRegister);
   tempMR->stopUsingMemRefRegister(cg);
   return NULL;
   }

TR::Register *
J9::Z::TreeEvaluator::BNDCHKwithSpineCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool needsBoundCheck = (node->getOpCodeValue() == TR::BNDCHKwithSpineCHK);
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   TR::Node *loadOrStoreChild = node->getFirstChild();
   TR::Node *baseArrayChild = node->getSecondChild();
   TR::Node *arrayLengthChild;
   TR::Node *indexChild;

   if (needsBoundCheck)
      {
      arrayLengthChild = node->getChild(2);
      indexChild = node->getChild(3);
      }
   else
      {
      arrayLengthChild = NULL;
      indexChild = node->getChild(2);
      }

   if (comp->getOption(TR_TraceCG))
      traceMsg(comp,"loadOrStoreChild: %p baseArrayChild: %p arrayLengthChild: %p indexChild: %p\n",loadOrStoreChild, baseArrayChild, arrayLengthChild, indexChild);

   // Order of evaluation dictates that the value to be stored needs to be evaluated first.
   if (loadOrStoreChild->getOpCode().isStore() && !loadOrStoreChild->getRegister())
      {
      TR::Node *valueChild = loadOrStoreChild->getSecondChild();
      cg->evaluate(valueChild);
      }

   TR::Register *baseArrayReg = cg->evaluate(baseArrayChild);
   preEvaluateEscapingNodesForSpineCheck(node, cg);

   // Generate the SpinCheck.
   TR::MemoryReference *contiguousArraySizeMR = generateS390MemoryReference(baseArrayReg, fej9->getOffsetOfContiguousArraySizeField(), cg);
   TR::MemoryReference *discontiguousArraySizeMR = generateS390MemoryReference(baseArrayReg, fej9->getOffsetOfDiscontiguousArraySizeField(), cg);

   bool doLoadOrStore = false;
   bool doAddressComputation = true;

   TR::Register* loadOrStoreReg = NULL;
   TR_Debug * debugObj = cg->getDebug();

   TR::LabelSymbol * oolStartLabel = generateLabelSymbol(cg);
   TR::LabelSymbol * oolReturnLabel = generateLabelSymbol(cg);
   TR::Register *indexReg = cg->evaluate(indexChild);
   TR::Register *valueReg = NULL;

   TR::Instruction * branchToOOL;

   if (needsBoundCheck)
      {
      generateRXInstruction(cg, TR::InstOpCode::CL, node, indexReg, contiguousArraySizeMR);

      // OOL Will actually throw the AIOB if necessary.
      branchToOOL = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNL, node, oolStartLabel);
      if (debugObj)
         debugObj->addInstructionComment(branchToOOL, "Start of OOL BNDCHKwithSpineCHK sequence");
      }
   else
      {
      // Load the Contiguous Array Size and test if it's zero.
      TR::Register *tmpReg = cg->allocateRegister();
      generateRSInstruction(cg, TR::InstOpCode::ICM, node, tmpReg, (uint32_t) 0xF, contiguousArraySizeMR);
      cg->stopUsingRegister(tmpReg);

      // Branch to OOL if contiguous array size is zero.
      branchToOOL = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, oolStartLabel);
      if (debugObj)
         debugObj->addInstructionComment(branchToOOL, "Start of OOL BNDCHKwithSpineCHK sequence");
      }

   // For reference stores, only evaluate the array element address because the store cannot
   // happen here (it must be done via the array store check).
   //
   // For primitive stores, evaluate them now.
   //
   // For loads, evaluate them now.
   //
   TR::Node * actualLoadOrStoreChild = loadOrStoreChild;
   TR::Node * evaluateConversionNode = loadOrStoreChild; // We want to match the top most conversion node and evaluate that.

   bool doLoadDecompress = false;

   // Top-level check whether a decompression sequence is necessary, because the first child
   // may have been created by a PRE temp.
   if ((loadOrStoreChild->getOpCodeValue() == TR::aload || loadOrStoreChild->getOpCodeValue() == TR::aRegLoad) &&
       node->isSpineCheckWithArrayElementChild() && TR::Compiler->target.is64Bit() && comp->useCompressedPointers())
      {
      doLoadDecompress = true;
      }

   while (actualLoadOrStoreChild->getOpCode().isConversion() ||
          ( ( actualLoadOrStoreChild->getOpCode().isAdd() || actualLoadOrStoreChild->getOpCode().isSub() ||
              actualLoadOrStoreChild->getOpCode().isLeftShift() || actualLoadOrStoreChild->getOpCode().isRightShift()) &&
            actualLoadOrStoreChild->containsCompressionSequence()))
      {
      // If we find a compression sequence, then reset the topmost conversion node to the child of the compression sequence.
      // i.e.  lshl
      //          i2l <--- set evaluateConversionNode to this node
      //
      if (! (actualLoadOrStoreChild->getOpCode().isConversion()))
         {
         evaluateConversionNode = actualLoadOrStoreChild->getFirstChild();
         }
      actualLoadOrStoreChild = actualLoadOrStoreChild->getFirstChild();
      }

   TR::Node * evaluatedNode = NULL;

   if (actualLoadOrStoreChild->getOpCode().isStore())
      {
      if (actualLoadOrStoreChild->getReferenceCount() > 1)
         {
         TR_ASSERT(actualLoadOrStoreChild->getOpCode().isWrtBar(), "Opcode must be wrtbar");
         loadOrStoreReg = cg->evaluate(actualLoadOrStoreChild->getFirstChild());
         cg->decReferenceCount(actualLoadOrStoreChild->getFirstChild());
         evaluatedNode = actualLoadOrStoreChild->getFirstChild();
         }
      else
         {
         loadOrStoreReg = cg->evaluate(actualLoadOrStoreChild);
         valueReg = actualLoadOrStoreChild->getSecondChild()->getRegister();
         evaluatedNode = actualLoadOrStoreChild;
         }
      }
   else
      {
      evaluatedNode = evaluateConversionNode;
      loadOrStoreReg = cg->evaluate(evaluateConversionNode);
      }

   if (comp->getOption(TR_TraceCG))
      traceMsg(comp,"Identified actualLoadOrStoreChild: %p and evaluated node: %p\n",actualLoadOrStoreChild, evaluatedNode);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, oolReturnLabel);

   if (loadOrStoreChild != evaluatedNode)
      cg->evaluate(loadOrStoreChild);

   // ---------------------------------------------
   // OOL Sequence to handle arraylet calculations.
   // ---------------------------------------------
   TR_S390OutOfLineCodeSection *outlinedDiscontigPath = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(oolStartLabel,oolReturnLabel,cg);
   cg->getS390OutOfLineCodeSectionList().push_front(outlinedDiscontigPath);
   outlinedDiscontigPath->swapInstructionListsWithCompilation();
   TR::Instruction * cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, oolStartLabel);

   // get the correct liveLocals from the OOL entry branch instruction, so the GC maps can be correct in OOL slow path
   cursor->setLiveLocals(branchToOOL->getLiveLocals());

   // Generate BNDCHK code.
   if (needsBoundCheck)
      {
      TR::LabelSymbol * boundCheckFailureLabel = generateLabelSymbol(cg);

      // Check if contiguous arraysize is zero first.  If not, throw AIOB
      TR::MemoryReference* contiguousArraySizeMR2 = generateS390MemoryReference(*contiguousArraySizeMR, 0, cg);
      TR::Register *tmpReg = cg->allocateRegister();
      cursor = generateRSInstruction(cg, TR::InstOpCode::ICM, node, tmpReg, (uint32_t) 0xF, contiguousArraySizeMR2, cursor);
      cg->stopUsingRegister(tmpReg);

      // Throw AIOB if continuousArraySizeMR is zero.
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, boundCheckFailureLabel, cursor);
      cursor->setExceptBranchOp();

      // Don't use CompareAndTrap to save the load of discontiguousArraySize into a register
      cursor = generateRXInstruction(cg, TR::InstOpCode::CL, node, indexReg, discontiguousArraySizeMR, cursor);

      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNL, node, boundCheckFailureLabel, cursor);
      cursor->setExceptBranchOp();
      cg->addSnippet(new (cg->trHeapMemory()) TR::S390HelperCallSnippet(cg, node, boundCheckFailureLabel, node->getSymbolReference()));
      }

   // TODO: Generate Arraylet calculation.
   TR::DataType dt = loadOrStoreChild->getDataType();
   int32_t elementSize = 0;
   if (dt == TR::Address)
      {
      elementSize = TR::Compiler->om.sizeofReferenceField();
      }
   else
      {
      elementSize = TR::Symbol::convertTypeToSize(dt);
      }

   int32_t spinePointerSize = (TR::Compiler->target.is64Bit() && !comp->useCompressedPointers()) ? 8 : 4;
   int32_t arrayHeaderSize = TR::Compiler->om.discontiguousArrayHeaderSizeInBytes();
   int32_t arrayletMask = fej9->getArrayletMask(elementSize);

   // Load the arraylet from the spine.
   int32_t spineShift = fej9->getArraySpineShift(elementSize);
   int32_t spinePtrShift = TR::TreeEvaluator::checkNonNegativePowerOfTwo(spinePointerSize);
   int32_t elementShift = TR::TreeEvaluator::checkNonNegativePowerOfTwo(elementSize);
   TR::Register* tmpReg = cg->allocateRegister();
   if (TR::Compiler->target.is64Bit())
      {
      if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::zEC12))
         {
         cursor = generateRIEInstruction(cg, TR::InstOpCode::RISBGN, node, tmpReg, indexReg,(32+spineShift-spinePtrShift), (128+63-spinePtrShift),(64-spineShift+spinePtrShift),cursor);
         }
      else if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z10))
         {
         cursor = generateRIEInstruction(cg, TR::InstOpCode::RISBG, node, tmpReg, indexReg,(32+spineShift-spinePtrShift), (128+63-spinePtrShift),(64-spineShift+spinePtrShift),cursor);
         }
      else
         {
         cursor = generateRRInstruction(cg, TR::InstOpCode::LGFR, node, tmpReg, indexReg, cursor);
         cursor = generateRSInstruction(cg, TR::InstOpCode::SRAG, node, tmpReg, tmpReg, spineShift, cursor);
         cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, tmpReg, tmpReg, spinePtrShift, cursor);
         }
      }
   else
      {
      cursor = generateRRInstruction(cg, TR::InstOpCode::LR, node, tmpReg, indexReg, cursor);
      cursor = generateRSInstruction(cg, TR::InstOpCode::SRA, node, tmpReg, tmpReg, spineShift, cursor);
      cursor = generateRSInstruction(cg, TR::InstOpCode::SLL, node, tmpReg, tmpReg, spinePtrShift, cursor);
      }

   // Load Arraylet pointer from Spine
   //   Pointer is compressed on 64-bit CmpRefs
   bool useCompressedPointers = TR::Compiler->target.is64Bit() && comp->useCompressedPointers();
   TR::MemoryReference * spineMR = generateS390MemoryReference(baseArrayReg, tmpReg, arrayHeaderSize, cg);
   cursor = generateRXInstruction(cg, (useCompressedPointers)?TR::InstOpCode::LLGF:TR::InstOpCode::getLoadOpCode(), node, tmpReg, spineMR, cursor);

   // Handle the compress shifting and addition of heap base.
   if (useCompressedPointers)
      {
      // Shift by compressed pointers shift amount if necessary.
      uint32_t cmpRefsShift = TR::Compiler->om.compressedReferenceShift();
      if (cmpRefsShift == 1)
         {
         TR::MemoryReference *cmpRefsShift1MR = generateS390MemoryReference(tmpReg, tmpReg, 0, cg);
         cursor = generateRXInstruction(cg, TR::InstOpCode::LA, node, tmpReg, cmpRefsShift1MR, cursor);
         }
      else if (cmpRefsShift >= 2)
         {
         cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, tmpReg, tmpReg, cmpRefsShift, cursor);
         }

      // Add heapbase value if necessary.
      uintptrj_t heapBaseValue = TR::Compiler->vm.heapBaseAddress();
      if (heapBaseValue != 0)
         cursor = generateRegLitRefInstruction(cg, TR::InstOpCode::AG, node, tmpReg, (int64_t)heapBaseValue, NULL, cursor, NULL, false);
      }

   // Calculate the offset with the arraylet for the index.
   TR::Register* tmpReg2 = cg->allocateRegister();
   TR::MemoryReference *arrayletMR;
   if (TR::Compiler->target.is64Bit())
      {
      if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::zEC12))
         {
         cursor = generateRIEInstruction(cg, TR::InstOpCode::RISBGN, node, tmpReg2, indexReg,(64-spineShift- elementShift), (128+63-elementShift),(elementShift),cursor);
         }
      else if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z10))
         {
         cursor = generateRIEInstruction(cg, TR::InstOpCode::RISBG, node, tmpReg2, indexReg,(64-spineShift- elementShift), (128+63-elementShift),(elementShift),cursor);
         }
      else
         {
         cursor = generateRRInstruction(cg, TR::InstOpCode::LGFR, node, tmpReg2, indexReg, cursor);
         cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, tmpReg2, tmpReg2, 64-spineShift, cursor);
         cursor = generateRSInstruction(cg, TR::InstOpCode::SRLG, node, tmpReg2, tmpReg2, 64-spineShift - elementShift, cursor);
         }
      }
   else
      {
      generateShiftAndKeepSelected31Bit(node, cg, tmpReg2, indexReg, 32-spineShift - elementShift, 31-elementShift, elementShift, true, false);
      }

   arrayletMR = generateS390MemoryReference(tmpReg, tmpReg2, 0, cg);
   cg->stopUsingRegister(tmpReg);
   cg->stopUsingRegister(tmpReg2);

   if (!actualLoadOrStoreChild->getOpCode().isStore())
      {
      TR::InstOpCode::Mnemonic op = TR::InstOpCode::BAD;

      TR::MemoryReference *highArrayletMR = NULL;
      TR::Register *highRegister = NULL;
      bool clearHighOrderBitsForUnsignedHalfwordLoads = false;

      // If we're not loading an array shadow then this must be an effective
      // address computation on the array element (for a write barrier).
      if ((!actualLoadOrStoreChild->getOpCode().hasSymbolReference() ||
           !actualLoadOrStoreChild->getSymbolReference()->getSymbol()->isArrayShadowSymbol()) &&
          !node->isSpineCheckWithArrayElementChild())
         {
         op = TR::InstOpCode::LA;
         }
      else
         {
         switch (dt)
            {
            case TR::Int8:   if (loadOrStoreChild->isZeroExtendedAtSource())
                               op = (TR::Compiler->target.is64Bit() ? TR::InstOpCode::LLGC : TR::InstOpCode::LLC);
                            else
                               op = (TR::Compiler->target.is64Bit() ? TR::InstOpCode::LGB : TR::InstOpCode::LB);
                            break;
            case TR::Int16:  if (actualLoadOrStoreChild->getOpCode().isShort())
                               {
                               op = TR::InstOpCode::getLoadHalfWordOpCode();
                               }
                            else
                               {
                               if (TR::Compiler->target.is64Bit())
                                  {
                                  op = TR::InstOpCode::LLGH;
                                  }
                               else
                                  {
                                  op = TR::InstOpCode::LLH;
                                  }
                               }
                            break;
            case TR::Int32:
                            if (loadOrStoreChild->isZeroExtendedAtSource())
                               op = (TR::Compiler->target.is64Bit() ? TR::InstOpCode::LLGF : TR::InstOpCode::L);
                            else
                               op = (TR::Compiler->target.is64Bit() ? TR::InstOpCode::LGF : TR::InstOpCode::L);
                            break;
            case TR::Int64:
                            if (TR::Compiler->target.is64Bit())
                               op = TR::InstOpCode::LG;
                            else
                               {
                               TR_ASSERT(loadOrStoreReg->getRegisterPair(), "expecting a register pair");

                               op = TR::InstOpCode::L;
                               highArrayletMR = generateS390MemoryReference(*arrayletMR, 4, cg);
                               highRegister = loadOrStoreReg->getHighOrder();
                               loadOrStoreReg = loadOrStoreReg->getLowOrder();
                               }
                            break;

            case TR::Float:  op = TR::InstOpCode::LE; break;
            case TR::Double: op = TR::InstOpCode::LD; break;

            case TR::Address:
                            if (TR::Compiler->target.is32Bit())
                               op = TR::InstOpCode::L;
                            else if (comp->useCompressedPointers())
                               op = TR::InstOpCode::LLGF;
                            else
                               op = TR::InstOpCode::LG;
                            break;

            default:
                            TR_ASSERT(0, "unsupported array element load type");
            }
         }
      cursor = generateRXInstruction(cg, op, node, loadOrStoreReg, arrayletMR, cursor);

      if (doLoadDecompress)
         {
         TR_ASSERT( dt == TR::Address, "Expecting loads with decompression trees to have data type TR::Address");

         // Shift by compressed pointers shift amount if necessary.
         //
         uint32_t cmpRefsShift = TR::Compiler->om.compressedReferenceShift();
         if (cmpRefsShift == 1)
            {
            TR::MemoryReference *cmpRefsShift1MR = generateS390MemoryReference(loadOrStoreReg, loadOrStoreReg, 0, cg);
            cursor = generateRXInstruction(cg, TR::InstOpCode::LA, node, loadOrStoreReg, cmpRefsShift1MR, cursor);
            }
         else if (cmpRefsShift >= 2)
            {
            cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, loadOrStoreReg, loadOrStoreReg, cmpRefsShift, cursor);
            }

         // Add heapbase value if necessary.
         uintptrj_t heapBaseValue = TR::Compiler->vm.heapBaseAddress();
         if (heapBaseValue != 0)
            cursor = generateRegLitRefInstruction(cg, TR::InstOpCode::AG, node, loadOrStoreReg, (int64_t)heapBaseValue, NULL, cursor, NULL, false);
         }

      if (highArrayletMR)
         {
         cursor = generateRXInstruction(cg, op, node, highRegister, highArrayletMR, cursor);
         }
      // We may need to clear the upper 16-bits of a unsign halfword load.
      if (clearHighOrderBitsForUnsignedHalfwordLoads)
         cursor = generateRIInstruction(cg, TR::InstOpCode::NILH, node, loadOrStoreReg, (int16_t)0x0000, cursor);
      }
   else
      {
      if (dt != TR::Address)
         {
         TR::InstOpCode::Mnemonic op;
         bool needStore = true;

         switch (dt)
            {
            case TR::Int8:   op = TR::InstOpCode::STC; break;
            case TR::Int16:  op = TR::InstOpCode::STH; break;
            case TR::Int32:  op = TR::InstOpCode::ST; break;
            case TR::Int64:
               if (TR::Compiler->target.is64Bit())
                  {
                  op = TR::InstOpCode::STG;
                  }
               else
                  {
                  TR_ASSERT(valueReg->getRegisterPair(), "value must be a register pair");
                  cursor = generateRXInstruction(cg, TR::InstOpCode::ST, node,  valueReg->getLowOrder(), arrayletMR, cursor);
                  cursor = generateRXInstruction(cg, TR::InstOpCode::ST, node, valueReg->getHighOrder(), generateS390MemoryReference(*arrayletMR,4,cg), cursor);
                  needStore = false;
                  }
               break;

            case TR::Float:  op = TR::InstOpCode::STE; break;
            case TR::Double: op = TR::InstOpCode::STD; break;

            default:
               TR_ASSERT(0, "unsupported array element store type");
               op = TR::InstOpCode::BAD;
            }

         if (needStore)
            cursor = generateRXInstruction(cg, op, node, valueReg, arrayletMR, cursor);
         }
      else
         {
         TR_ASSERT(0, "OOL reference stores not supported yet");
         }
      }


   cursor = generateS390BranchInstruction(cg,TR::InstOpCode::BRC,TR::InstOpCode::COND_BRC,node,oolReturnLabel, cursor);
   if (debugObj)
      debugObj->addInstructionComment(cursor, "End of OOL BNDCHKwithSpineCHK sequence");

   outlinedDiscontigPath->swapInstructionListsWithCompilation();

   cg->decReferenceCount(loadOrStoreChild);
   cg->decReferenceCount(baseArrayChild);
   cg->decReferenceCount(indexChild);
   if (arrayLengthChild)
      cg->recursivelyDecReferenceCount(arrayLengthChild);

   return NULL;
   }

static void
VMarrayStoreCHKEvaluator(
      TR::Node * node,
      TR::S390CHelperLinkage *helperLink,
      TR::Node *callNode,
      TR::Register * srcReg,
      TR::Register * owningObjectReg,
      TR::Register * t1Reg,
      TR::Register * t2Reg,
      TR::Register * litPoolBaseReg,
      TR::Register * owningObjectRegVal,
      TR::Register * srcRegVal,
      TR::LabelSymbol * wbLabel,
      TR::RegisterDependencyConditions * conditions,
      TR::CodeGenerator * cg)
   {
   TR::LabelSymbol * helperCallLabel = generateLabelSymbol(cg);
   TR::LabelSymbol * startOOLLabel = generateLabelSymbol(cg);
   TR::LabelSymbol * exitOOLLabel = generateLabelSymbol(cg);
   TR::LabelSymbol * exitPointLabel = wbLabel;
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   TR_S390OutOfLineCodeSection *arrayStoreCHKOOL;
   TR_Debug * debugObj = cg->getDebug();

   TR::InstOpCode::Mnemonic loadOp;
   TR::Instruction * cursor;
   TR::Instruction * gcPoint;
   TR::S390PrivateLinkage * linkage = TR::toS390PrivateLinkage(cg->getLinkage());
   int bytesOffset;

   TR::TreeEvaluator::genLoadForObjectHeadersMasked(cg, node, owningObjectRegVal, generateS390MemoryReference(owningObjectReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg), NULL);
   TR::TreeEvaluator::genLoadForObjectHeadersMasked(cg, node,          srcRegVal, generateS390MemoryReference(         srcReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg), NULL);

   // may need to convert the class offset from t1Reg into a J9Class pointer
   cursor = generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, t1Reg, generateS390MemoryReference(owningObjectRegVal, (int32_t) offsetof(J9ArrayClass, componentType), cg));

   // check if obj.class(in t1Reg) == array.componentClass in t2Reg
#ifdef OMR_GC_COMPRESSED_POINTERS
   cursor = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CR, node, t1Reg, srcRegVal, TR::InstOpCode::COND_BER, wbLabel, false, false);
#else
   cursor = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalRegOpCode(), node, t1Reg, srcRegVal, TR::InstOpCode::COND_BE, wbLabel, false, false);
#endif

   if (debugObj)
      debugObj->addInstructionComment(cursor, "Check if src.type == array.type");

   intptrj_t objectClass = (intptrj_t) fej9->getSystemClassFromClassName("java/lang/Object", 16, true);
   /*
    * objectClass is used for Object arrays check optimization: when we are storing to Object arrays we can skip all other array store checks
    * However, TR_J9SharedCacheVM::getSystemClassFromClassName can return 0 when it's impossible to relocate j9class later for AOT loads
    * in that case we don't want to generate the Object arrays check
    */
   bool doObjectArrayCheck = objectClass != 0;

   if (doObjectArrayCheck && (cg->wantToPatchClassPointer((TR_OpaqueClassBlock*)objectClass, node) || cg->needClassAndMethodPointerRelocations()))
      {
      if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z10) && cg->isLiteralPoolOnDemandOn())
         {
         TR::S390ConstantDataSnippet * targetsnippet;
         if (TR::Compiler->target.is64Bit())
            {
            targetsnippet = cg->findOrCreate8ByteConstant(node, (int64_t)objectClass);
            cursor = (TR::S390RILInstruction *) generateRILInstruction(cg, TR::InstOpCode::CLGRL, node, t1Reg, targetsnippet, 0);
            }
         else
            {
            targetsnippet = cg->findOrCreate4ByteConstant(node, (int32_t)objectClass);
            cursor = (TR::S390RILInstruction *) generateRILInstruction(cg, TR::InstOpCode::CLRL, node, t1Reg, targetsnippet, 0);
            }

         if(comp->getOption(TR_EnableHCR))
            comp->getSnippetsToBePatchedOnClassRedefinition()->push_front(targetsnippet);
         if (cg->needClassAndMethodPointerRelocations())
            {
            targetsnippet->setReloType(TR_ClassPointer);
            AOTcgDiag4(comp, "generateRegLitRefInstruction constantDataSnippet=%x symbolReference=%x symbol=%x reloType=%x\n",
                  targetsnippet, targetsnippet->getSymbolReference(), targetsnippet->getSymbolReference()->getSymbol(), TR_ClassPointer);
            }
         }
      else
         {
         if (cg->needClassAndMethodPointerRelocations())
            {
            generateRegLitRefInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, t2Reg,(uintptrj_t) objectClass, TR_ClassPointer, conditions, NULL, NULL);
            }
         else
            {
            genLoadAddressConstantInSnippet(cg, node, (intptr_t)objectClass, t2Reg, cursor, conditions, litPoolBaseReg, true);
            }

#ifdef OMR_GC_COMPRESSED_POINTERS
         generateRRInstruction(cg, TR::InstOpCode::CR, node, t1Reg, t2Reg);
#else
         generateRRInstruction(cg, TR::InstOpCode::getCmpLogicalRegOpCode(), node, t1Reg, t2Reg);
#endif
         }
      }
   else if (doObjectArrayCheck)
      {
      // make sure that t1Reg contains the class offset and not the J9Class pointer
      if (TR::Compiler->target.is64Bit())
         generateS390ImmOp(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, t1Reg, t1Reg, (int64_t) objectClass, conditions, litPoolBaseReg);
      else
         generateS390ImmOp(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, t1Reg, t1Reg, (int32_t) objectClass, conditions, litPoolBaseReg);
      }

   // Bringing back tests from outlined keeping only helper call in outlined section
   // TODO Attaching helper call predependency to BRASL instruction and combine ICF conditions with post dependency conditions of
   // helper call should fix the issue of unnecessary spillings in ICF. Currently bringing the tests back to main line here but
   // check performance of both case.
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, wbLabel);
   if (debugObj)
      debugObj->addInstructionComment(cursor, "Check if array.type is type object, if yes jump to wbLabel");

   generateRXInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, t1Reg,
      generateS390MemoryReference(srcRegVal, offsetof(J9Class, castClassCache), cg));

   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, wbLabel);
   if (debugObj)
      debugObj->addInstructionComment(cursor, "Check if src.class(in t1Reg).castClassCache == array.componentClass");

   // Check to see if array-type is a super-class of the src object
   if (TR::Compiler->target.is64Bit())
      {
      loadOp = TR::InstOpCode::LLGH;
      bytesOffset = 6;
      }
   else
      {
      loadOp = TR::InstOpCode::LLH;
      bytesOffset = 2;
      }

   // Get array element depth
   cursor = generateRXInstruction(cg, loadOp, node, owningObjectRegVal,
      generateS390MemoryReference(t1Reg, offsetof(J9Class, classDepthAndFlags) + bytesOffset, cg));

   // Get src depth
   cursor = generateRXInstruction(cg, loadOp, node, t2Reg,
      generateS390MemoryReference(srcRegVal, offsetof(J9Class, classDepthAndFlags) + bytesOffset, cg));

   TR_ASSERT(sizeof(((J9Class*)0)->classDepthAndFlags) == sizeof(uintptr_t),
      "VMarrayStoreCHKEvaluator::J9Class->classDepthAndFlags is wrong size\n");

   // Check super class values
   static_assert(J9AccClassDepthMask == 0xffff, "VMarrayStoreCHKEvaluator::J9AccClassDepthMask should have be 16 bit of ones");

   // Compare depths and makes sure depth(src) >= depth(array-type)
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, owningObjectRegVal, t2Reg, TR::InstOpCode::COND_BH, helperCallLabel, false, false);
   if (debugObj)
      debugObj->addInstructionComment(cursor, "Failure if depth(src) < depth(array-type)");

   if (TR::Compiler->target.is64Bit())
      {
      generateRSInstruction(cg, TR::InstOpCode::SLLG, node, owningObjectRegVal, owningObjectRegVal, 3);
      }
   else
      {
      generateRSInstruction(cg, TR::InstOpCode::SLL, node, owningObjectRegVal, 2);
      }

   generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, t2Reg,
      generateS390MemoryReference(srcRegVal, offsetof(J9Class, superclasses), cg));

   generateRXInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, t1Reg,
      generateS390MemoryReference(t2Reg, owningObjectRegVal, 0, cg));

   if (debugObj)
      debugObj->addInstructionComment(cursor, "Check if src.type is subclass");
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRNE, node, helperCallLabel);
   // FAIL
   arrayStoreCHKOOL = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(helperCallLabel,wbLabel,cg);
   cg->getS390OutOfLineCodeSectionList().push_front(arrayStoreCHKOOL);
   arrayStoreCHKOOL->swapInstructionListsWithCompilation();
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, helperCallLabel);
   TR::Register *dummyResReg = helperLink->buildDirectDispatch(callNode);
   if (dummyResReg)
      cg->stopUsingRegister(dummyResReg);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, wbLabel);
   arrayStoreCHKOOL->swapInstructionListsWithCompilation();
   }

///////////////////////////////////////////////////////////////////////////////////////
// ArrayStoreCHKEvaluator - Array store check. child 1 is object, 2 is array.
//   Symbolref indicates failure action/destination
///////////////////////////////////////////////////////////////////////////////////////
TR::Register *
J9::Z::TreeEvaluator::ArrayStoreCHKEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   // Note: we take advantages of the register conventions of the helpers by limiting register usages on
   //       the fast-path (most likely 4 registers; at most, 6 registers)

   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   TR::Node * firstChild = node->getFirstChild();
   auto gcMode = TR::Compiler->om.writeBarrierType();
   // As arguments to ArrayStoreCHKEvaluator helper function is children of first child,
   // We need to create a dummy call node for helper call with children containing arguments to helper call.
   bool doWrtBar = (gcMode == gc_modron_wrtbar_oldcheck ||
                    gcMode == gc_modron_wrtbar_cardmark_and_oldcheck ||
                    gcMode == gc_modron_wrtbar_always);

   bool doCrdMrk = ((gcMode == gc_modron_wrtbar_cardmark || gcMode == gc_modron_wrtbar_cardmark_and_oldcheck || gcMode == gc_modron_wrtbar_cardmark_incremental) && !firstChild->isNonHeapObjectWrtBar());

   TR::Node * litPoolBaseChild=NULL;
   TR::Node * sourceChild = firstChild->getSecondChild();
   TR::Node * classChild  = firstChild->getChild(2);

   bool nopASC = false;
   if (comp->performVirtualGuardNOPing() && node->getArrayStoreClassInNode() &&
      !comp->getOption(TR_DisableOOL) && !fej9->classHasBeenExtended(node->getArrayStoreClassInNode()))
      nopASC = true;

   bool usingCompressedPointers = false;
   if (comp->useCompressedPointers() && firstChild->getOpCode().isIndirect())
      {
       // pattern match the sequence
       //     iistore f     iistore f         <- node
       //       aload O       aload O
       //     value           l2i
       //                       lshr
       //                         lsub
       //                           a2l
       //                             value   <- sourceChild
       //                           lconst HB
       //                         iconst shftKonst
       //
       // -or- if the field is known to be null
       // iistore f
       //    aload O
       //    l2i
       //      a2l
       //        value  <- valueChild
       //
      TR::Node *translatedNode = sourceChild;
      if (translatedNode->getOpCode().isConversion())
         translatedNode = translatedNode->getFirstChild();
      if (translatedNode->getOpCode().isRightShift()) // optional
         translatedNode = translatedNode->getFirstChild();

      bool usingLowMemHeap = false;
      if (TR::Compiler->vm.heapBaseAddress() == 0 ||
             sourceChild->isNull())
         usingLowMemHeap = true;

      if (translatedNode->getOpCode().isSub() || usingLowMemHeap)
         usingCompressedPointers = true;

      if (usingCompressedPointers)
         {
         while ((sourceChild->getNumChildren() > 0) &&
                  (sourceChild->getOpCodeValue() != TR::a2l))
            sourceChild = sourceChild->getFirstChild();
         if (sourceChild->getOpCodeValue() == TR::a2l)
            sourceChild = sourceChild->getFirstChild();
         // artificially bump up the refCount on the value so
         // that different registers are allocated for the actual
         // and compressed values
         //
         sourceChild->incReferenceCount();
         }
      }
   TR::Node * memRefChild = firstChild->getFirstChild();

   TR::Register * srcReg, * classReg, * txReg, * tyReg, * baseReg, * indexReg, *litPoolBaseReg=NULL,*memRefReg;
   TR::MemoryReference * mr1, * mr2;
   TR::LabelSymbol * wbLabel, * cFlowRegionEnd, * simpleStoreLabel, * cFlowRegionStart;
   TR::RegisterDependencyConditions * conditions;
   TR::S390PrivateLinkage * linkage = TR::toS390PrivateLinkage(cg->getLinkage());
   TR::Register * tempReg = NULL;
   TR::Instruction *cursor;

   cFlowRegionStart = generateLabelSymbol(cg);
   wbLabel = generateLabelSymbol(cg);
   cFlowRegionEnd = generateLabelSymbol(cg);
   simpleStoreLabel = generateLabelSymbol(cg);

   txReg = cg->allocateRegister();
   tyReg = cg->allocateRegister();

   TR::Register * owningObjectRegVal = cg->allocateRegister();
   TR::Register * srcRegVal = cg->allocateRegister();

   // dst reg is read-only when we don't do wrtbar or crdmark
   // if destination node is the same as source node we also
   // need to create a copy because destination & source
   // are 1st and 2nd arguments to the call and as such
   // they need to be in 2 different registers
   if (doWrtBar || doCrdMrk || (classChild==sourceChild))
      {
      classReg = cg->gprClobberEvaluate(classChild);
      // evaluate using load and test
      if (sourceChild->getOpCode().isLoadVar() && sourceChild->getRegister()==NULL && !sourceChild->isNonNull())
         {
            srcReg = cg->allocateCollectedReferenceRegister();

            generateRXInstruction(cg, TR::InstOpCode::getLoadTestOpCode(), sourceChild, srcReg, generateS390MemoryReference(sourceChild, cg));

            sourceChild->setRegister(srcReg);
         }
      else
         {
            srcReg = cg->gprClobberEvaluate(sourceChild);
         }
      }
   else
      {
      classReg = cg->evaluate(classChild);
      srcReg = cg->evaluate(sourceChild);
      }
   TR::Node *callNode = TR::Node::createWithSymRef(node, TR::call, 2, node->getSymbolReference());
   callNode->setChild(0, sourceChild);
   callNode->setChild(1, classChild);
   mr1 = generateS390MemoryReference(firstChild, cg);

   TR::Register *compressedReg = srcReg;
   if (usingCompressedPointers)
      compressedReg = cg->evaluate(firstChild->getSecondChild());

   //  We need deps to setup args for arrayStoreCHK helper and/or wrtBAR helper call.
   //  We need 2 more regs for inline version of arrayStoreCHK (txReg & tyReg).  We use RA/EP for these
   //  We then need two extra regs for memref for the actual store.
   //  A seventh, eighth and ninth post dep may be needed to manufacture imm values
   //  used by the inlined version of arrayStoreCHK
   //  The tenth post dep may be needed to generateDirectCall if it creates a RegLitRefInstruction.
   conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 11, cg);
   conditions->addPostCondition(classReg, linkage->getIntegerArgumentRegister(0));
   conditions->addPostCondition(srcReg, linkage->getIntegerArgumentRegister(1));
   if (usingCompressedPointers)
      {
      conditions->addPostConditionIfNotAlreadyInserted(compressedReg, TR::RealRegister::AssignAny);
      }
   conditions->addPostCondition(txReg,  linkage->getReturnAddressRegister());
   conditions->addPostCondition(tyReg,  linkage->getEntryPointRegister());
   conditions->addPostCondition(srcRegVal,  TR::RealRegister::AssignAny);
   conditions->addPostCondition(owningObjectRegVal,  TR::RealRegister::AssignAny);

   TR::Instruction *current = cg->getAppendInstruction();
   TR_ASSERT( current != NULL, "Could not get current instruction");

   if (node->getNumChildren()==2)
      {
      litPoolBaseChild=node->getSecondChild();
      TR_ASSERT((litPoolBaseChild->getOpCodeValue()==TR::aload) || (litPoolBaseChild->getOpCodeValue()==TR::aRegLoad),
              "Literal pool base child expected\n");
      litPoolBaseReg=cg->evaluate(litPoolBaseChild);
      conditions->addPostCondition(litPoolBaseReg, TR::RealRegister::AssignAny);
      }

   if (!sourceChild->isNonNull())
      {
      // If we have an actionable wrtbar and a NULL ptr, branch around the wrt bar
      // as it needs not be exec'd
      TR::InstOpCode::Mnemonic compareAndBranchMnemonic = usingCompressedPointers ? TR::InstOpCode::C : TR::InstOpCode::getCmpOpCode();

      generateS390CompareAndBranchInstruction(cg, compareAndBranchMnemonic, node, srcReg, 0, TR::InstOpCode::COND_BE, (doWrtBar || doCrdMrk)?simpleStoreLabel:wbLabel, false, true);
      }
   TR::S390CHelperLinkage *helperLink = static_cast<TR::S390CHelperLinkage*>(cg->getLinkage(TR_CHelper));
   if (nopASC)
      {
      // Speculatively NOP the array store check if VP is able to prove that the ASC
      // would always succeed given the current state of the class hierarchy.
      //
      TR::LabelSymbol * oolASCLabel = generateLabelSymbol(cg);
      TR_VirtualGuard *virtualGuard = TR_VirtualGuard::createArrayStoreCheckGuard(comp, node, node->getArrayStoreClassInNode());
      TR::Instruction *vgnopInstr = generateVirtualGuardNOPInstruction(cg, node, virtualGuard->addNOPSite(), NULL, oolASCLabel);

      // nopASC assumes OOL is enabled
      TR_S390OutOfLineCodeSection *outlinedSlowPath = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(oolASCLabel, wbLabel, cg);
      cg->getS390OutOfLineCodeSectionList().push_front(outlinedSlowPath);
      outlinedSlowPath->swapInstructionListsWithCompilation();

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, oolASCLabel);
      TR::Register *dummyResReg = helperLink->buildDirectDispatch(callNode);
      if (dummyResReg)
         cg->stopUsingRegister(dummyResReg);

      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, wbLabel);
      outlinedSlowPath->swapInstructionListsWithCompilation();
      }
   else
      VMarrayStoreCHKEvaluator(node, helperLink, callNode, srcReg, classReg, txReg, tyReg, litPoolBaseReg, owningObjectRegVal, srcRegVal, wbLabel, conditions, cg);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, wbLabel);

   if (mr1->getBaseRegister())
      {
      conditions->addPostConditionIfNotAlreadyInserted(mr1->getBaseRegister(), TR::RealRegister::AssignAny);
      }
   if (mr1->getIndexRegister())
      {
      conditions->addPostConditionIfNotAlreadyInserted(mr1->getIndexRegister(), TR::RealRegister::AssignAny);
      }

   if (usingCompressedPointers)
      generateRXInstruction(cg, TR::InstOpCode::ST, node, compressedReg, mr1);
   else
      generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, srcReg, mr1);

   if (doWrtBar)
      {
      TR::SymbolReference *wbRef ;
      if (gcMode == gc_modron_wrtbar_cardmark_and_oldcheck || gcMode == gc_modron_wrtbar_oldcheck)
         wbRef = comp->getSymRefTab()->findOrCreateWriteBarrierStoreGenerationalSymbolRef(comp->getMethodSymbol());
      else
         wbRef = comp->getSymRefTab()->findOrCreateWriteBarrierStoreSymbolRef(comp->getMethodSymbol());

      // Cardmarking is not inlined for gencon. Consider doing so when perf issue arises.
      VMnonNullSrcWrtBarCardCheckEvaluator(firstChild, classReg, srcReg, tyReg, txReg, cFlowRegionEnd, wbRef, conditions, cg, false);
      }
   else if (doCrdMrk)
      {
      VMCardCheckEvaluator(firstChild, classReg, NULL, conditions, cg, true, cFlowRegionEnd);
      }

   // Store for case where we have a NULL ptr detected at runtime and
   // branches around the wrtbar
   //
   // For the non-NULL case we chose to simply exec the ST twice as this is
   // cheaper than branching around the a single ST inst.
   //
   if (!sourceChild->isNonNull() && (doWrtBar || doCrdMrk))
      {
      // As we could hit a gc when doing the gencon wrtbar, we have to not
      // re-do the ST.  We must branch around the second store.
      //
      if (doWrtBar)
         {
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cFlowRegionEnd);
         }

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, simpleStoreLabel);

      mr2 = generateS390MemoryReference(*mr1, 0, cg);
      if (usingCompressedPointers)
         generateRXInstruction(cg, TR::InstOpCode::ST, node, compressedReg, mr2);
      else
         generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, srcReg, mr2);
      }

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, conditions);
   cFlowRegionEnd->setEndInternalControlFlow();

   if (comp->useCompressedPointers() && firstChild->getOpCode().isIndirect())
      firstChild->setStoreAlreadyEvaluated(true);

   cg->decReferenceCount(sourceChild);
   cg->decReferenceCount(classChild);
   if (litPoolBaseChild!=NULL) cg->decReferenceCount(litPoolBaseChild);
   cg->decReferenceCount(firstChild);
   if (usingCompressedPointers)
      {
      cg->decReferenceCount(firstChild->getSecondChild());
      cg->stopUsingRegister(compressedReg);
      }
   mr1->stopUsingMemRefRegister(cg);
   cg->stopUsingRegister(txReg);
   cg->stopUsingRegister(tyReg);
   cg->stopUsingRegister(classReg);
   cg->stopUsingRegister(srcReg);
   cg->stopUsingRegister(owningObjectRegVal);
   cg->stopUsingRegister(srcRegVal);

   if (tempReg)
      {
      cg->stopUsingRegister(tempReg);
      }

   // determine where internal control flow begins by looking for the first branch
   // instruction after where the label instruction would have been inserted
   TR::Instruction *next = current->getNext();
   while(next != NULL && !next->isBranchOp())
      next = next->getNext();
   TR_ASSERT( next != NULL && next->getPrev() != NULL, "Could not find branch instruction where internal control flow begins");
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart, next->getPrev());
   cFlowRegionStart->setStartInternalControlFlow();

   return NULL;
   }

///////////////////////////////////////////////////////////////////////////////////////
// ArrayCHKEvaluator -  Array compatibility check. child 1 is object1, 2 is object2.
//    Symbolref indicates failure action/destination
///////////////////////////////////////////////////////////////////////////////////////
TR::Register *
J9::Z::TreeEvaluator::ArrayCHKEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return TR::TreeEvaluator::VMarrayCheckEvaluator(node, cg);
   }

TR::Register *
J9::Z::TreeEvaluator::conditionalHelperEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   // used by methodEnterhook, and methodExitHook
   // Decrement the reference count on the constant placeholder parameter to
   // the MethodEnterHook call.  An evaluation isn't necessary because the
   // constant value isn't used here.
   //
   if (node->getOpCodeValue() == TR::MethodEnterHook)
      {
      if (node->getSecondChild()->getOpCode().isCall() && node->getSecondChild()->getNumChildren() > 1)
         {
         cg->decReferenceCount(node->getSecondChild()->getFirstChild());
         }
      }

   // The child contains an inline test.
   //
   TR::Node * testNode = node->getFirstChild();
   TR::Node * firstChild = testNode->getFirstChild();
   TR::Node * secondChild = testNode->getSecondChild();
   TR::Register * src1Reg = cg->evaluate(firstChild);
   if (secondChild->getOpCode().isLoadConst())
      // &&
      //       secondChild->getRegister() == NULL)
      {
      int32_t value = secondChild->getInt();
      TR::Node * firstChild = testNode->getFirstChild();

      if (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL)
         {
         generateRIInstruction(cg, TR::InstOpCode::CHI, node, src1Reg, value);
         }
      else
         {
         TR::Register * tempReg = cg->evaluate(secondChild);
         generateRRInstruction(cg, TR::InstOpCode::CR, node, src1Reg, tempReg);
         }
      }
   else
      {
      TR::Register * src2Reg = cg->evaluate(secondChild);
      generateRRInstruction(cg, TR::InstOpCode::CR, node, src1Reg, src2Reg);
      }

   TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
   TR::LabelSymbol * snippetLabel = generateLabelSymbol(cg);
   TR::Instruction * gcPoint;

   TR::Register * tempReg1 = cg->allocateRegister();
   TR::Register * tempReg2 = cg->allocateRegister();
   TR::RegisterDependencyConditions * dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg);
   dependencies->addPostCondition(tempReg1, cg->getEntryPointRegister());
   dependencies->addPostCondition(tempReg2, cg->getReturnAddressRegister());
   snippetLabel->setEndInternalControlFlow();

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
   cFlowRegionStart->setStartInternalControlFlow();
   gcPoint = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, testNode->getOpCodeValue() == TR::icmpeq ?  TR::InstOpCode::COND_BE : TR::InstOpCode::COND_BNE, node, snippetLabel);

   TR::LabelSymbol * reStartLabel = generateLabelSymbol(cg);
   TR::Snippet * snippet = new (cg->trHeapMemory()) TR::S390HelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference(), reStartLabel);
   cg->addSnippet(snippet);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, reStartLabel, dependencies);

   gcPoint->setNeedsGCMap(0x0000FFFF);

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->decReferenceCount(testNode);
   cg->stopUsingRegister(tempReg1);
   cg->stopUsingRegister(tempReg2);

   return NULL;
   }




// genCoreInstanceofEvaluator is used by if instanceof and instanceof routines.
// The routine generates the 'core' code for instanceof evaluation. It requires a true and false label
// (which are the same and are just fall-through labels if no branching is required) as well as
// a boolean to indicate if the result should be calculated and returned in a register.
// The code also needs to indicate if the fall-through case if for 'true' or 'false'.
TR::Register *
J9::Z::TreeEvaluator::VMgenCoreInstanceofEvaluator(TR::Node * node, TR::CodeGenerator * cg, TR::LabelSymbol * falseLabel, TR::LabelSymbol * trueLabel, bool needsResult, bool trueFallThrough, TR::RegisterDependencyConditions* baseConditions, bool isIfInstanceof)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   TR::LabelSymbol * doneLabel = generateLabelSymbol(cg);
   TR::LabelSymbol * continueLabel = generateLabelSymbol(cg);
   TR::Instruction * gcPoint = NULL;

   //Two DataSnippet is used in genTestIsSuper for secondaryCacheSites.
   TR::S390WritableDataSnippet *classObjectClazzSnippet = NULL;
   TR::S390WritableDataSnippet *instanceOfClazzSnippet = NULL;

   if (trueLabel == NULL)
      {
      trueLabel = doneLabel;
      }
   if (falseLabel == NULL)
      {
      falseLabel = doneLabel;
      }

   TR::Node * objectNode      = node->getFirstChild();
   TR::Node * castClassNode   = node->getSecondChild();
   TR::SymbolReference * castClassSymRef = castClassNode->getSymbolReference();

   bool testEqualClass        = instanceOfOrCheckCastNeedEqualityTest(node, cg);
   bool testCastClassIsSuper  = instanceOfOrCheckCastNeedSuperTest(node, cg);
   bool isFinalClass          = (castClassSymRef == NULL) ? false : castClassSymRef->isNonArrayFinal(comp);
   bool needsHelperCall       = needHelperCall(node, testCastClassIsSuper, isFinalClass);
   bool testCache             = needTestCache(true, needsHelperCall, testCastClassIsSuper);
   bool performReferenceArrayTestInline = false;
   if (TR::TreeEvaluator::instanceOfOrCheckCastIsFinalArray(node, cg))
      {
      testEqualClass = true;
      testCastClassIsSuper = false;
      needsHelperCall = false;
      testCache = false;
      }
   else if (TR::TreeEvaluator::instanceOfOrCheckCastIsJavaLangObjectArray(node, cg)) // array of Object
      {
      testEqualClass = false;
      testCastClassIsSuper = false;
      needsHelperCall = false;
      testCache = false;
      performReferenceArrayTestInline = true;
      }

   //bool testPackedArray       = instanceOfOrCheckCastPackedArrayTest(node, cg);

   // came from transformed call isInstance to node instanceof, can't resolve at compile time
   bool dynamicClassPointer = isDynamicCastClassPointer(node) && testCastClassIsSuper;
   bool addDataSnippetForSuperTest = isIfInstanceof && dynamicClassPointer && comp->getOption(TR_EnableOnsiteCacheForSuperClassTest);
   TR::Register * objectClazzSnippetReg = NULL;
   TR::Register * instanceOfClazzSnippetReg = NULL;

   TR::LabelSymbol *callHelper = NULL;
   if (dynamicClassPointer)
      {
      testCache = true;
      callHelper = generateLabelSymbol(cg);
      //callHelper = new (cg->trHeapMemory()) TR::LabelSymbol(cg);
      }

   TR::Register * objectReg    = objectNode->getRegister();
   TR::Register * castClassReg = castClassNode->getRegister();
   TR_ASSERT(objectReg && castClassReg,
      "TR::TreeEvaluator::VMgenCoreInstanceofEvaluator: objectNode and castClassNode are assumed to beevaluated\n");

   TR::Register * litPoolReg   = establishLitPoolBaseReg(node, cg);

   TR::Register * resultReg;
   TR::Register * callResult   = NULL;
   TR::Register * objectCopyReg = NULL;
   TR::Register * castClassCopyReg = NULL;

   TR::Register * scratch1Reg = NULL;
   TR::Register * scratch2Reg = NULL;
   TR::Register * objClassReg = cg->allocateRegister();

   bool nullTestRequired = !objectNode->isNonNull() && !node->chkIsReferenceNonNull();
   bool nullCCSet        = false;

   TR::RegisterDependencyConditions* conditions;

   dumpOptDetails(comp, "\nInstanceOf: testEqual:%d testSuper: %d testCache: %d needsHelper:%d, dynamicClassPointer:%d falseLabel:%p trueLabel:%p",
      testEqualClass, testCastClassIsSuper, testCache, needsHelperCall, dynamicClassPointer, falseLabel, trueLabel);
   dumpOptDetails(comp, "\nInstanceOf: addDataSnippetForSuperTest: %d, performReferenceArrayTestInline: %d, true fall through:%d need result:%d\n", addDataSnippetForSuperTest, performReferenceArrayTestInline, trueFallThrough, needsResult);
   if (baseConditions)
      {
      conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(baseConditions, 16, maxInstanceOfPostDependencies(), cg);
      }
   else
      {
      conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(16, 16, cg);
      }

   if (needsResult)
      {
      resultReg = cg->allocateRegister();
      }
   else
      {
      resultReg = NULL;
      }

   if (litPoolReg)
      {
      conditions->addPostCondition(litPoolReg, TR::RealRegister::AssignAny);
      }

   if (nullTestRequired && needsResult)
      {
      generateRIInstruction(cg, TR::InstOpCode::LHI, node, resultReg, 0);
      }

   if (needsHelperCall)
      {
      // No GC point needed (confirmed with GAC)
      // the call will kill the parms, so copy them if required and ensure
      // they are in fixed registers
      bool objNodeNull = objectNode->getRegister() == NULL;
      if ((!objNodeNull &&  !cg->canClobberNodesRegister(objectNode)) || (!objNodeNull))
         {
         objectCopyReg = cg->allocateRegister();
         if (nullTestRequired)
            {
            genNullTest(cg, objectNode, objectCopyReg, objectReg, NULL);
            nullCCSet = true;
            }
         else
            {
            generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, objectCopyReg, objectReg);
            }
         conditions->addPostCondition(objectCopyReg, TR::RealRegister::GPR2);
         conditions->addPostConditionIfNotAlreadyInserted(objectReg, TR::RealRegister::AssignAny);
         callResult = objectCopyReg;
         }
      else
         {
         conditions->addPostCondition(objectReg, TR::RealRegister::GPR2);
         callResult = objectReg;
         }

      bool classCastNodeNull = castClassNode->getRegister() == NULL;
      if ((!classCastNodeNull && !cg->canClobberNodesRegister(castClassNode)) || (!classCastNodeNull))
         {
         castClassCopyReg = cg->allocateRegister();
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, castClassCopyReg, castClassReg);
         conditions->addPostCondition(castClassCopyReg, TR::RealRegister::GPR1);
         conditions->addPostConditionIfNotAlreadyInserted(castClassReg, TR::RealRegister::AssignAny);
         }
      else
         {
         conditions->addPostConditionIfNotAlreadyInserted(castClassReg, TR::RealRegister::GPR1);
         }

      if (needsResult)
         {
         conditions->addPostCondition(resultReg, TR::RealRegister::AssignAny);
         }
      conditions->addPostCondition(objClassReg, cg->getReturnAddressRegister());
      }
   else
      {
      conditions->addPostConditionIfNotAlreadyInserted(castClassReg, TR::RealRegister::AssignAny);
      if (needsResult)
         {
         conditions->addPostCondition(resultReg, TR::RealRegister::AssignAny);
         }
      conditions->addPostCondition(objClassReg, TR::RealRegister::AssignAny);
      conditions->addPostConditionIfNotAlreadyInserted(objectReg, TR::RealRegister::AssignAny);
      }

   if (testCastClassIsSuper || testCache)
      {
      int32_t castClassDepth = castClassSymRef->classDepth(comp);
      int32_t superClassOffset = castClassDepth * TR::Compiler->om.sizeofReferenceAddress();
      bool outOfBound = (superClassOffset > MAX_IMMEDIATE_VAL || superClassOffset < MIN_IMMEDIATE_VAL || dynamicClassPointer) ? true : false;
      // we don't use scratch2Reg when we do testCastClassIsSuper without testCache (unless it's outOfBound case)
      if (testCache || outOfBound)
         {
         scratch2Reg = cg->allocateRegister();
         conditions->addPostCondition(scratch2Reg, TR::RealRegister::AssignAny);
         }
      scratch1Reg = cg->allocateRegister();

      TR::RealRegister::RegDep scratch1RegAssignment;
#if defined(TR_TARGET_64BIT)
#if defined(J9ZOS390)
      if (comp->getOption(TR_EnableRMODE64))
#endif
         {
         // On 64-bit systems trampolines may kill the EP register so we need to add it to post-dependencies. If there
         // is no OOL path we need to assign any real register to scratch1Reg.
         scratch1RegAssignment = (needsHelperCall) ?  static_cast<TR::RealRegister::RegDep>(cg->getEntryPointRegister()) : TR::RealRegister::AssignAny;
         }
#elif !defined(TR_TARGET_64BIT) || (defined(TR_TARGET_64BIT) && defined(J9ZOS390))
#if (defined(TR_TARGET_64BIT) && defined(J9ZOS390))
      else if (!comp->getOption(TR_EnableRMODE64))
#endif
         {
         scratch1RegAssignment = TR::RealRegister::AssignAny;
         }
#endif
      conditions->addPostCondition(scratch1Reg, scratch1RegAssignment);
      }
#if defined(TR_TARGET_64BIT)
   else if ( needsHelperCall
#if defined(J9ZOS390)
             && comp->getOption(TR_EnableRMODE64)
#endif
           )
      {
      //on zLinux and zOS trampoline may kill EP reg so we need to add it to post conditions
      // when there is an OOL path, we cannot have an unused virtual register, so adding EP to a temp register
      TR::Register * dummyReg = cg->allocateRegister();
      conditions->addPostCondition(dummyReg, cg->getEntryPointRegister());
      dummyReg->setPlaceholderReg();
      cg->stopUsingRegister(dummyReg);
      }
#endif

   if ( performReferenceArrayTestInline )
      {
      if (scratch1Reg == NULL)
         {
         scratch1Reg = cg->allocateRegister();
         conditions->addPostCondition(scratch1Reg, TR::RealRegister::AssignAny);
         }
      }

   if (nullTestRequired)
      {
      // NULL instanceof X is false
      dumpOptDetails(comp, "InstanceOf: Generate NULL Branch\n");
      if (!nullCCSet)
         {
         genNullTest(cg, objectNode, objectReg, objectReg, NULL);
         }
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, falseLabel);
      }

   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/MethodEntry", comp->signature()),1,TR::DebugCounter::Undetermined);
   // this load could have been done after the equality test
   // but that would mean we would have a larger delay for the superclass
   // test. This is presupposing the equality test will not typically match
   TR::TreeEvaluator::genLoadForObjectHeadersMasked(cg, node, objClassReg, generateS390MemoryReference(objectReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg), NULL);

   if (performReferenceArrayTestInline)
      {
      // We expect the Array Test to either return True or False, There is no helper.
      // Following debug counter gives statistics about how many Array Test We have.
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/ArrayTest", comp->signature()),1,TR::DebugCounter::Undetermined);
      genIsReferenceArrayTest(node, objClassReg, scratch1Reg, scratch2Reg, needsResult ? resultReg : NULL, falseLabel, trueLabel, needsResult, trueFallThrough, cg);
      }

   if (testCache)
      {
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/Profiled", comp->signature()),1,TR::DebugCounter::Undetermined);
      generateInlineTest(cg, node, castClassNode, objClassReg, resultReg, scratch1Reg, litPoolReg, needsResult, falseLabel, trueLabel, doneLabel, false);
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/ProfiledFail", comp->signature()),1,TR::DebugCounter::Undetermined);

      TR::LabelSymbol * doneTestCacheLabel  = generateLabelSymbol(cg);

#ifdef OMR_GC_COMPRESSED_POINTERS
      // For the memory reference below, we may need to convert the
      // class offset from objClassReg into a J9Class pointer
#endif
      TR::MemoryReference * cacheMR = generateS390MemoryReference(objClassReg, offsetof(J9Class, castClassCache), cg);

      generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, scratch2Reg, cacheMR);

      //clearing last bit of cached value, which is a cached result of instanceof
      //(0: true, 1: false), we will need to check it below
      //Following Debug Counter is there just to match Total Debug Counters in new evaluator
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/CacheTest", comp->signature()),1,TR::DebugCounter::Undetermined);
      if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::zEC12))
         {
         auto i1 = TR::Compiler->target.is64Bit() ? 0 : 32;

         generateRIEInstruction(cg, TR::InstOpCode::RISBGN, node, scratch1Reg, scratch2Reg, i1, 62|0x80, 0);
         }
      else if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z10))
         {
         auto i1 = TR::Compiler->target.is64Bit() ? 0 : 32;

         generateRIEInstruction(cg, TR::InstOpCode::RISBG, node, scratch1Reg, scratch2Reg, i1, 62|0x80, 0);
         }
      else
         {
         generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, scratch1Reg, 0xFFFE);
         generateRRInstruction(cg, TR::InstOpCode::getAndRegOpCode(), node, scratch1Reg, scratch2Reg);
         }

#ifdef OMR_GC_COMPRESSED_POINTERS
      // May need to convert the J9Class pointer from scratch1Reg
      // into a class offset
#endif
      TR_ASSERT(needsHelperCall, "expecting a helper call after the testCache");
      generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, castClassReg, scratch1Reg, TR::InstOpCode::COND_BNE, doneTestCacheLabel, false, false);
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/CacheClassSuccess", comp->signature()),1,TR::DebugCounter::Undetermined);
      if (needsResult)
         {
         // For cases when cached value has a result (ie: instanceof)
         // value at offsetof(J9Class, castClassCache) is actually j9class + last bit is set for the result of instanceof:
         // 1: false, 0: true, so we need to check and set resultsReg the opposite (0: false, 1: true)

         if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z196))
            {
            generateRRInstruction(cg, TR::InstOpCode::getSubstractRegOpCode(), node, scratch1Reg, scratch2Reg);
            generateRIEInstruction(cg, TR::InstOpCode::AHIK, node, resultReg, scratch1Reg, 1);
            }
         else
            {
            generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, scratch1Reg, 0x1);
            generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, resultReg, scratch2Reg);
            generateRRInstruction(cg, TR::InstOpCode::getOrRegOpCode(), node, scratch1Reg, resultReg);
            generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, resultReg, scratch1Reg);
            }

         if (falseLabel != trueLabel)
            {
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, falseLabel);
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, trueLabel);
            }
         else
            {
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, doneLabel);
            }
         }
      else
         {
         if (falseLabel != trueLabel)
            {
            generateRIInstruction(cg, TR::InstOpCode::TMLL, node, scratch2Reg, 0x1);
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, trueLabel);
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, falseLabel);
            }
         else
            {
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, doneLabel);
            }
         }

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneTestCacheLabel);
      }

   if (testEqualClass)
      {
      if (needsResult)
         {
         generateRIInstruction(cg, TR::InstOpCode::LHI, node, resultReg, 1);
         }
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/Equality", comp->signature()),1,TR::DebugCounter::Undetermined);
      generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, objClassReg, castClassReg, TR::InstOpCode::COND_BE, trueLabel, false, false);
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/EqualityFail", comp->signature()),1,TR::DebugCounter::Undetermined);
      }

   if (testCastClassIsSuper)
      {
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/Profiled", comp->signature()),1,TR::DebugCounter::Undetermined);
      generateInlineTest(cg, node, castClassNode, objClassReg, resultReg, scratch1Reg, litPoolReg, needsResult, continueLabel, trueLabel, doneLabel, false, 1);
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/ProfileFail", comp->signature()),1,TR::DebugCounter::Undetermined);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, continueLabel);
      if (needsResult && !testEqualClass)
         {
         generateRIInstruction(cg, TR::InstOpCode::LHI, node, resultReg, 1);
         }
      int32_t castClassDepth = castClassSymRef->classDepth(comp);

      if ( addDataSnippetForSuperTest )
         {
         if (TR::Compiler->target.is64Bit())
            {
            classObjectClazzSnippet = (TR::S390WritableDataSnippet * )cg->Create8ByteConstant(node, -1, true);
            instanceOfClazzSnippet = (TR::S390WritableDataSnippet * )cg->Create8ByteConstant(node, -1, true);
            }
         else
            {
            classObjectClazzSnippet = (TR::S390WritableDataSnippet * )cg->Create4ByteConstant(node, -1, true);
            instanceOfClazzSnippet = (TR::S390WritableDataSnippet * )cg->Create4ByteConstant(node, -1, true);
            }
         if ( classObjectClazzSnippet == NULL || instanceOfClazzSnippet == NULL )
            {
            addDataSnippetForSuperTest = false;
            }
         else
            {
            objectClazzSnippetReg = cg->allocateRegister();
            instanceOfClazzSnippetReg = cg->allocateRegister();
            conditions->addPostCondition(objectClazzSnippetReg, TR::RealRegister::AssignAny);
            conditions->addPostCondition(instanceOfClazzSnippetReg, TR::RealRegister::AssignAny);
            generateRILInstruction(cg, TR::InstOpCode::LARL, node, objectClazzSnippetReg, classObjectClazzSnippet, 0);
            generateRILInstruction(cg, TR::InstOpCode::LARL, node, instanceOfClazzSnippetReg, instanceOfClazzSnippet, 0);
            }
         }
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/SuperClassTest", comp->signature()),1,TR::DebugCounter::Undetermined);
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/MethodExit", comp->signature()),1,TR::DebugCounter::Undetermined);
      genTestIsSuper(cg, node, objClassReg, castClassReg, scratch1Reg, scratch2Reg, needsResult ? resultReg : NULL, litPoolReg, castClassDepth, falseLabel, trueLabel, callHelper, conditions, NULL, addDataSnippetForSuperTest, objectClazzSnippetReg, instanceOfClazzSnippetReg);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, trueLabel);
      }

   if (scratch1Reg)
      cg->stopUsingRegister(scratch1Reg);
   if (scratch2Reg)
      cg->stopUsingRegister(scratch2Reg);

   if ((testCastClassIsSuper || !needsHelperCall)&& !performReferenceArrayTestInline)
       {
       if (needsResult)
          {
          generateRIInstruction(cg, TR::InstOpCode::LHI, node, resultReg, 0);
          }
       if (trueLabel != falseLabel)
          {
          dumpOptDetails(comp, "InstanceOf: if instanceof\n");
          if (trueFallThrough)
             {
             generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, falseLabel);
             }
          }
       }
   //If snippetAdded is true, we need to do Helper call, and when we comeback, we need to use the result to update
   //the dataSnippet that is added.
   if (needsHelperCall)
      {
      TR::LabelSymbol *doneOOLLabel = NULL;
      TR_Debug * debugObj = cg->getDebug();
      TR::Register * tempObjectClassReg = NULL;

      //jump here from genTestIsSuper
      TR_S390OutOfLineCodeSection *outlinedSlowPath = NULL;
      if (dynamicClassPointer)
         {
         if (falseLabel != trueLabel)
            {
            if (trueFallThrough)
               doneOOLLabel = trueLabel;
            else
               doneOOLLabel = falseLabel;
            }
         else
            doneOOLLabel = doneLabel;

         outlinedSlowPath =
               new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(callHelper, doneOOLLabel,cg);
         cg->getS390OutOfLineCodeSectionList().push_front(outlinedSlowPath);
         outlinedSlowPath->swapInstructionListsWithCompilation();
         TR::Instruction *temp = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, callHelper);

         if (debugObj)
            debugObj->addInstructionComment(temp, "Denotes start of OOL checkCast sequence");

         }
      if ( addDataSnippetForSuperTest )
         {
         tempObjectClassReg = cg->allocateRegister();
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, tempObjectClassReg, objClassReg);
         }
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOf/(%s)/Helper", comp->signature()),1,TR::DebugCounter::Undetermined);
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/Helper", comp->signature()),1,TR::DebugCounter::Undetermined);
      generateDirectCall(cg, node, false, node->getSymbolReference(), conditions);

      // this is annoying but since the result from the call has the same reg
      // as the 2nd parm (object reg), we end up having to make a copy to
      // get the result into the resultReg
      // If the false and true labels are the same, there is no branching required,
      // otherwise, need to branch to the right spot.
      if ( needsResult )
         {
         generateRRInstruction(cg, TR::InstOpCode::getLoadTestRegOpCode(), node, resultReg, callResult);
         }
      else
         {
         generateRRInstruction(cg, TR::InstOpCode::getLoadTestRegOpCode(), node, callResult, callResult);
         }

      //when Snippet is Added, do Update the dataSnippet with the result of the call. only when the callResult is true.
      if( addDataSnippetForSuperTest )
         {
         //when Snippet is Added, do Update the dataSnippet with the result of the call. only when the callResult is true.
         /* pseudocode for z
         * BRC to end of this block if callResult is 0
         * load -1 to temp Register
         * compare with "-1 loded register", dataSnippet1, and if not update datasnippet with objectClassReg(Compare and Swap instr)
         * if we didn't update, we don't update the next one->branch out to doneUpdateSnippetLabel
         * store dataSnippet2, castClassReg.//if we did update 1, we need to update both.
         * TestcallResultReg again to use in branch Instr
         * */

         TR::LabelSymbol *doneUpdateSnippetLabel = generateLabelSymbol(cg);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, doneUpdateSnippetLabel);
         TR::Register * tempNeg1LoadedRegister = cg->allocateRegister();
         //we do not need post condition since this code resides in OOL only.

         generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, tempNeg1LoadedRegister, -1);

         comp->getSnippetsToBePatchedOnClassUnload()->push_front(classObjectClazzSnippet);
         comp->getSnippetsToBePatchedOnClassUnload()->push_front(instanceOfClazzSnippet);
         generateRSInstruction(cg, TR::InstOpCode::getCmpAndSwapOpCode(), node, tempNeg1LoadedRegister, tempObjectClassReg, generateS390MemoryReference(objectClazzSnippetReg, 0, cg));
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, doneUpdateSnippetLabel);

         generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, castClassReg, generateS390MemoryReference(instanceOfClazzSnippetReg, 0, cg));
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneUpdateSnippetLabel);
         cg->stopUsingRegister(tempNeg1LoadedRegister);
         cg->stopUsingRegister(tempObjectClassReg);

         generateRRInstruction(cg, TR::InstOpCode::getLoadTestRegOpCode(), node, callResult, callResult);//condition code needs to be re-set. this is secondCache specific so included in this block.

         }

      if (falseLabel != trueLabel)
         {
         dumpOptDetails(comp, "InstanceOf: if instanceof\n");
         if (trueFallThrough)
            {
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, falseLabel);
            }
         else
            {
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, trueLabel);
            }
         }

      if (dynamicClassPointer )
         {
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, doneOOLLabel);
         outlinedSlowPath->swapInstructionListsWithCompilation();
         }
      }

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneLabel, conditions);

   if (needsResult)
      node->setRegister(resultReg);

   cg->decReferenceCount(objectNode);
   cg->decReferenceCount(castClassNode);

   cg->stopUsingRegister(objClassReg);
   if (litPoolReg)
      cg->stopUsingRegister(litPoolReg);
   if (objectCopyReg)
      cg->stopUsingRegister(objectCopyReg);
   if (castClassCopyReg)
      cg->stopUsingRegister(castClassCopyReg);
   if (objectClazzSnippetReg)
      cg->stopUsingRegister(objectClazzSnippetReg);
   if (instanceOfClazzSnippetReg)
      cg->stopUsingRegister(instanceOfClazzSnippetReg);

   if (callResult)
      cg->stopUsingRegister(callResult);

   return resultReg;
   }

static TR::Register *
reservationLockEnter(TR::Node *node, int32_t lwOffset, TR::Register *objectClassReg, TR::CodeGenerator *cg, TR::S390CHelperLinkage *helperLink)
   {
   TR::Register *objReg, *monitorReg, *valReg, *tempReg;
   TR::Register *EPReg, *returnAddressReg;
   TR::LabelSymbol *resLabel, *callLabel, *doneLabel;
   TR::Instruction *instr;
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   int numICFDeps = 6 + (comp->getOptions()->enableDebugCounters() ? 4: 0);
   TR::RegisterDependencyConditions *ICFConditions =
      new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, numICFDeps, cg);

   if (objectClassReg)
      objReg = objectClassReg;
   else
      objReg = node->getFirstChild()->getRegister();

   TR::Register *metaReg = cg->getMethodMetaDataRealRegister();

   monitorReg = cg->allocateRegister();
   valReg = cg->allocateRegister();
   tempReg = cg->allocateRegister();

   resLabel = generateLabelSymbol(cg);
   callLabel = generateLabelSymbol(cg);
   doneLabel = generateLabelSymbol(cg);

   // TODO - primitive monitors are disabled. Enable it after testing
   //TR::TreeEvaluator::isPrimitiveMonitor(node, cg);
   //
   TR::LabelSymbol *helperReturnOOLLabel, *doneOOLLabel = NULL;
   TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
   TR_S390OutOfLineCodeSection *outlinedSlowPath = NULL;
   TR_Debug *debugObj = cg->getDebug();
   TR::Snippet *snippet = NULL;

   // This is just for test. (may not work in all cases)
   static bool enforcePrimitive = feGetEnv("EnforcePrimitiveLockRes")? 1 : 0;
   bool isPrimitive = enforcePrimitive ? 1 : node->isPrimitiveLockedRegion();

   // Opcodes:
   bool use64b = true;
   if (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord())
      use64b = false;
   else if (!TR::Compiler->target.is64Bit())
      use64b = false;
   TR::InstOpCode::Mnemonic loadOp = use64b ? TR::InstOpCode::LG : TR::InstOpCode::L;
   TR::InstOpCode::Mnemonic loadRegOp = use64b ? TR::InstOpCode::LGR : TR::InstOpCode::LR;
   TR::InstOpCode::Mnemonic orImmOp = TR::InstOpCode::OILF;
   TR::InstOpCode::Mnemonic compareOp = use64b ? TR::InstOpCode::CGR : TR::InstOpCode::CR;
   TR::InstOpCode::Mnemonic compareImmOp = use64b ? TR::InstOpCode::CG : TR::InstOpCode::C;
   TR::InstOpCode::Mnemonic addImmOp = use64b ? TR::InstOpCode::AGHI : TR::InstOpCode::AHI;
   TR::InstOpCode::Mnemonic storeOp = use64b ? TR::InstOpCode::STG : TR::InstOpCode::ST;
   TR::InstOpCode::Mnemonic xorOp = use64b ? TR::InstOpCode::XGR : TR::InstOpCode::XR;
   TR::InstOpCode::Mnemonic casOp = use64b ? TR::InstOpCode::CSG : TR::InstOpCode::CS;
   TR::InstOpCode::Mnemonic loadImmOp = use64b ? TR::InstOpCode::LGHI : TR::InstOpCode::LHI ;
   TR::InstOpCode::Mnemonic andOp = use64b ? TR::InstOpCode::NGR : TR::InstOpCode::NR;

   //ICF RA constraints
   //////////////
   ICFConditions->addPostConditionIfNotAlreadyInserted(objReg, TR::RealRegister::AssignAny);
   ICFConditions->addPostConditionIfNotAlreadyInserted(monitorReg, TR::RealRegister::AssignAny);
   ICFConditions->addPostConditionIfNotAlreadyInserted(valReg, TR::RealRegister::AssignAny);
   ICFConditions->addPostConditionIfNotAlreadyInserted(tempReg, TR::RealRegister::AssignAny);
   //////////////

   // Main path instruction sequence (non-primitive).
   //    L     monitorReg, #lwOffset(objectReg)
   //    LR    valReg,     metaReg
   //    OILF  valReg,     LR-Bit
   //    CRJ   valReg,     monitorReg, MASK6, callLabel
   //    AHI   monitorReg, INC_DEC_VALUE
   //    ST    monitorReg, #lwOffset(objectReg)

   // load monitor reg
   generateRXInstruction(cg, loadOp, node, monitorReg, generateS390MemoryReference(objReg, lwOffset, cg));
   // load r13|LOCK_RESERVATION_BIT
   generateRRInstruction(cg, loadRegOp, node, valReg, metaReg);
   generateRILInstruction(cg, orImmOp, node, valReg, LOCK_RESERVATION_BIT);

   // Jump to OOL path if lock is not reserved (monReg != r13|LOCK_RESERVATION_BIT)
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
   cFlowRegionStart->setStartInternalControlFlow();
   instr = generateS390CompareAndBranchInstruction(cg, compareOp, node, valReg, monitorReg,
      TR::InstOpCode::COND_BNE, resLabel, false, false);

   if (!comp->getOption(TR_DisableOOL))
      {
      helperReturnOOLLabel = generateLabelSymbol(cg);
      doneOOLLabel = generateLabelSymbol(cg);
      if (debugObj)
         debugObj->addInstructionComment(instr, "Branch to OOL reservation enter sequence");
      outlinedSlowPath = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(resLabel, doneOOLLabel, cg);
      cg->getS390OutOfLineCodeSectionList().push_front(outlinedSlowPath);
      }
   else
      {
      TR_ASSERT(0, "Not implemented- Lock reservation with Disable OOL yet.");
      //Todo: Call VM helper maybe?
      }

   cg->generateDebugCounter("LockEnt/LR/LRSuccessfull", 1, TR::DebugCounter::Undetermined);
   if (!isPrimitive)
      {
      generateRIInstruction  (cg, addImmOp, node, monitorReg, (uintptrj_t) LOCK_INC_DEC_VALUE);
      generateRXInstruction(cg, storeOp, node, monitorReg, generateS390MemoryReference(objReg, lwOffset, cg));
      }

   if (outlinedSlowPath) // Means we have OOL
      {
      TR::LabelSymbol *reserved_checkLabel = generateLabelSymbol(cg);
      outlinedSlowPath->swapInstructionListsWithCompilation(); // Toggle instruction list
      TR::Instruction *temp = generateS390LabelInstruction(cg,TR::InstOpCode::LABEL,node,resLabel);
      if (debugObj)
         {
         if (isPrimitive)
            debugObj->addInstructionComment(temp, "Denotes start of OOL primitive reservation enter sequence");
         else
            debugObj->addInstructionComment(temp, "Denotes start of OOL non-primitive reservation enter sequence");
         }
      // XXX: Temporary fix, OOL instruction stream does not pick up live locals or monitors correctly.
      TR_ASSERT(!temp->getLiveLocals() && !temp->getLiveMonitors(), "Expecting first OOL instruction to not have live locals/monitors info");
      temp->setLiveLocals(instr->getLiveLocals());
      temp->setLiveMonitors(instr->getLiveMonitors());

      // Non-Primitive lockReservation enter sequence:         Primitive lockReservation enter sequence:

      // CIJ   monitorReg, 0, MASK6, checkLabel                TODO - Add Primitive lockReservation enter sequence
      // AHI   valReg, INC_DEC_VALUE
      // XR    monitorReg, monitorReg
      // CS    monitorReg, valReg, #lwOffset(objectReg)
      // BRC   MASK6, callHelper
      // BRC   returnLabel
      // checkLabel:
      // LGFI  tempReg, LOCK_RES_NON_PRIMITIVE_ENTER_MASK
      // NR    tempReg, monitorReg
      // CRJ   tempReg, valReg, MASK6, callHelper
      // AHI   monitorReg, INC_DEC_VALUE
      // ST    monitorReg, #lwOffset(objectReg)
      // BRC   returnLabel
      // callHelper:
      // BRASL R14, jitMonitorEntry
      //returnLabel:

      // Avoid CAS in case lock value is not zero
      generateS390CompareAndBranchInstruction(cg, compareImmOp, node, monitorReg, 0, TR::InstOpCode::COND_BNE, reserved_checkLabel, false);
      if (!isPrimitive)
         {
         generateRIInstruction  (cg, addImmOp, node, valReg, (uintptrj_t) LOCK_INC_DEC_VALUE);
         }
      // Try to acquire the lock using CAS
      generateRRInstruction(cg, xorOp, node, monitorReg, monitorReg);
      generateRSInstruction(cg, casOp, node, monitorReg, valReg, generateS390MemoryReference(objReg, lwOffset, cg));
      // Call VM helper if the CAS fails (contention)
      instr = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, callLabel);

      cg->generateDebugCounter("LockEnt/LR/CASSuccessfull", 1, TR::DebugCounter::Undetermined);

      // Lock is acquired successfully
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, helperReturnOOLLabel);

      generateS390LabelInstruction(cg,TR::InstOpCode::LABEL,node,reserved_checkLabel);
      // Mask the counter
      // Mask is 8 bit value which will be sign extended, We will be using cheaper instruction like LGHI or LHI
      generateRIInstruction(cg, loadImmOp, node, tempReg, ~(isPrimitive ? LOCK_RES_PRIMITIVE_ENTER_MASK : LOCK_RES_NON_PRIMITIVE_ENTER_MASK));
      generateRRInstruction(cg, andOp, node, tempReg, monitorReg);

      // Call VM helper if the R13 != (masked MonReg)
      generateS390CompareAndBranchInstruction(cg, compareOp,node, tempReg, valReg,
         TR::InstOpCode::COND_BNE, callLabel, false, false);

      cg->generateDebugCounter("LockEnt/LR/Recursive", 1, TR::DebugCounter::Undetermined);

      // Recursive lock. Increment the counter
      if (!isPrimitive)
         {
         generateRIInstruction  (cg, addImmOp, node, monitorReg, (uintptrj_t) LOCK_INC_DEC_VALUE);
         generateRXInstruction(cg, storeOp, node, monitorReg, generateS390MemoryReference(objReg, lwOffset, cg));
         }
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, helperReturnOOLLabel);
      // call to jithelper
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, callLabel);
      cg->generateDebugCounter("LockEnt/LR/VMHelper", 1, TR::DebugCounter::Undetermined);
      uintptrj_t returnAddress = (uintptrj_t) (node->getSymbolReference()->getMethodAddress());

      // We are calling helper within ICF so we need to combine dependency from ICF and helper call at merge label
      TR::RegisterDependencyConditions *deps = NULL;
      helperLink->buildDirectDispatch(node, &deps);
      TR::RegisterDependencyConditions *mergeConditions = mergeConditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(ICFConditions, deps, cg);
      // OOL return label
      instr = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, helperReturnOOLLabel, mergeConditions);
      helperReturnOOLLabel->setEndInternalControlFlow();
      if (debugObj)
         {
         debugObj->addInstructionComment(instr, "OOL reservation enter VMHelper return label");
         }

      instr = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, doneOOLLabel);
      if (debugObj)
         {
         if (isPrimitive)
            debugObj->addInstructionComment(instr, "Denotes end of OOL primitive reservation enter sequence: return to mainline");
         else
            debugObj->addInstructionComment(instr, "Denotes end of OOL non-primitive reservation enter sequence: return to mainline");
         }

      outlinedSlowPath->swapInstructionListsWithCompilation(); // Toggle instruction list

      instr = generateS390LabelInstruction(cg,TR::InstOpCode::LABEL,node,doneOOLLabel);
      if (debugObj)
         debugObj->addInstructionComment(instr, "OOL reservation enter return label");
      generateS390LabelInstruction(cg,TR::InstOpCode::LABEL,node, doneLabel);
      }
   else
      {
      TR_ASSERT(0, "Not implemented:Lock reservation with Disable OOL.");
      }
   if (monitorReg)
      cg->stopUsingRegister(monitorReg);
   if (valReg)
      cg->stopUsingRegister(valReg);
   if (tempReg)
      cg->stopUsingRegister(tempReg);

   cg->decReferenceCount(node->getFirstChild());
   return NULL;
   }

static TR::Register *
reservationLockExit(TR::Node *node, int32_t lwOffset, TR::Register *objectClassReg, TR::CodeGenerator *cg, TR::S390CHelperLinkage *helperLink )
   {
   TR::Register *objReg, *monitorReg, *valReg, *tempReg;
   TR::Register *EPReg, *returnAddressReg;
   TR::LabelSymbol *resLabel, *callLabel, *doneLabel;
   TR::Instruction *instr;
   TR::Instruction *startICF = NULL;
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   int numICFDeps = 6 + (comp->getOptions()->enableDebugCounters() ? 4: 0);
   TR::RegisterDependencyConditions *ICFConditions =
      new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, numICFDeps, cg);
   if (objectClassReg)
      objReg = objectClassReg;
   else
      objReg = node->getFirstChild()->getRegister();

   TR::Register *metaReg = cg->getMethodMetaDataRealRegister();

   monitorReg = cg->allocateRegister();
   valReg = cg->allocateRegister();
   tempReg = cg->allocateRegister();


   //ICF RA constraints
   //////////////
   ICFConditions->addPostConditionIfNotAlreadyInserted(objReg, TR::RealRegister::AssignAny);
   ICFConditions->addPostConditionIfNotAlreadyInserted(monitorReg, TR::RealRegister::AssignAny);
   ICFConditions->addPostConditionIfNotAlreadyInserted(valReg, TR::RealRegister::AssignAny);
   ICFConditions->addPostConditionIfNotAlreadyInserted(tempReg, TR::RealRegister::AssignAny);

   resLabel = generateLabelSymbol(cg);
   callLabel = generateLabelSymbol(cg);
   doneLabel = generateLabelSymbol(cg);

   TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);

   TR::LabelSymbol *helperReturnOOLLabel, *doneOOLLabel = NULL;
   TR_S390OutOfLineCodeSection *outlinedSlowPath = NULL;
   TR_Debug *debugObj = cg->getDebug();
   TR::Snippet *snippet = NULL;
   static bool enforcePrimitive = feGetEnv("EnforcePrimitiveLockRes")? 1 : 0;
   bool isPrimitive = enforcePrimitive ? 1 : node->isPrimitiveLockedRegion();

   // Opcodes:
   bool use64b = true;
   if (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord())
      use64b = false;
   else if (!TR::Compiler->target.is64Bit())
      use64b = false;
   TR::InstOpCode::Mnemonic loadOp = use64b ? TR::InstOpCode::LG : TR::InstOpCode::L;
   TR::InstOpCode::Mnemonic loadRegOp = use64b ? TR::InstOpCode::LGR : TR::InstOpCode::LR;
   TR::InstOpCode::Mnemonic orImmOp = TR::InstOpCode::OILF;
   TR::InstOpCode::Mnemonic compareOp = use64b ? TR::InstOpCode::CGR : TR::InstOpCode::CR;
   TR::InstOpCode::Mnemonic compareImmOp = use64b ? TR::InstOpCode::CG : TR::InstOpCode::C;
   TR::InstOpCode::Mnemonic addImmOp = use64b ? TR::InstOpCode::AGHI : TR::InstOpCode::AHI;
   TR::InstOpCode::Mnemonic storeOp = use64b ? TR::InstOpCode::STG : TR::InstOpCode::ST;
   TR::InstOpCode::Mnemonic xorOp = use64b ? TR::InstOpCode::XGR : TR::InstOpCode::XR;
   TR::InstOpCode::Mnemonic casOp = use64b ? TR::InstOpCode::CSG : TR::InstOpCode::CS;
   TR::InstOpCode::Mnemonic loadImmOp = use64b ? TR::InstOpCode::LGHI : TR::InstOpCode::LHI;
   TR::InstOpCode::Mnemonic andOp = use64b ? TR::InstOpCode::NGR : TR::InstOpCode::NR;
   TR::InstOpCode::Mnemonic andImmOp = TR::InstOpCode::NILF;

   // Main path instruction sequence (non-primitive).
   //   L     monitorReg, #lwOffset(objectReg)
   //   LR    valReg, metaReg
   //   OILF  valReg, INC_DEC_VALUE | LR-Bit
   //   CRJ   valReg, monitorReg, BNE, callLabel
   //   AHI   valReg, -INC_DEC_VALUE
   //   ST    valReg, #lwOffset(objectReg)

   generateRXInstruction(cg, loadOp, node, monitorReg, generateS390MemoryReference(objReg, lwOffset, cg));
   if (!isPrimitive)
      {
      generateRRInstruction(cg, loadRegOp, node, tempReg, metaReg);
      generateRILInstruction(cg, orImmOp, node, tempReg, LOCK_RESERVATION_BIT + LOCK_INC_DEC_VALUE);
      instr = generateS390CompareAndBranchInstruction(cg, compareOp, node, tempReg, monitorReg,
         TR::InstOpCode::COND_BNE, resLabel, false, false);
      cg->generateDebugCounter("LockExit/LR/LRSuccessfull", 1, TR::DebugCounter::Undetermined);
      }
   else
      {
      generateRRInstruction(cg, loadRegOp, node, tempReg, monitorReg);
      generateRILInstruction(cg, andImmOp, node, tempReg, LOCK_RES_PRIMITIVE_EXIT_MASK);
      instr = generateS390CompareAndBranchInstruction(cg, compareImmOp, node, tempReg, LOCK_RESERVATION_BIT,
         TR::InstOpCode::COND_BNE, resLabel, false);
      }

   if (!comp->getOption(TR_DisableOOL))
      {
      helperReturnOOLLabel = generateLabelSymbol(cg);
      doneOOLLabel = generateLabelSymbol(cg);
      if (debugObj)
         debugObj->addInstructionComment(instr, "Branch to OOL reservation exit sequence");
      outlinedSlowPath = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(resLabel, doneOOLLabel, cg);
      cg->getS390OutOfLineCodeSectionList().push_front(outlinedSlowPath);
      }
   else
      TR_ASSERT(0, "Not implemented: Lock reservation with Disable OOL.");

   if (!isPrimitive)
      {
      generateRIInstruction  (cg, use64b? TR::InstOpCode::AGHI : TR::InstOpCode::AHI, node, tempReg, -LOCK_INC_DEC_VALUE);
      generateRXInstruction(cg, use64b? TR::InstOpCode::STG : TR::InstOpCode::ST,
         node, tempReg, generateS390MemoryReference(objReg, lwOffset, cg));
      }

   if (outlinedSlowPath) // Means we have OOL
      {
      outlinedSlowPath->swapInstructionListsWithCompilation(); // Toggle instruction list
      TR::Instruction *temp = generateS390LabelInstruction(cg,TR::InstOpCode::LABEL,node,resLabel);
      if (debugObj)
         {
         if (isPrimitive)
            debugObj->addInstructionComment(temp, "Denotes start of OOL primitive reservation exit sequence");
         else
            debugObj->addInstructionComment(temp, "Denotes start of OOL non-primitive reservation exit sequence");
         }
      // XXX: Temporary fix, OOL instruction stream does not pick up live locals or monitors correctly.
      TR_ASSERT(!temp->getLiveLocals() && !temp->getLiveMonitors(), "Expecting first OOL instruction to not have live locals/monitors info");
      temp->setLiveLocals(instr->getLiveLocals());
      temp->setLiveMonitors(instr->getLiveMonitors());

      // Non-PRIMITIVE reservationLock exit sequence              PRIMITIVE reservationLock exit sequence
      // LGFI  tempReg, LOCK_RES_OWNING                           TODO - PRIMITIVE reservationLock exit sequence
      // NR    tempReg, monitorReg
      // LR    valReg, metaReg
      // AHI   valReg, LR-Bit
      // CRJ   tempReg, valReg, BNE, callHelper
      // LR    tempReg, monitorReg
      // NILF  tempReg, LOCK_RES_NON_PRIMITIVE_EXIT_MASK
      // BRC   BERC, callHelper
      // AHI   monitorReg, -INC_DEC_VALUE
      // ST    monitorReg, #lwOffset(objectReg)
      // BRC   returnLabel
      // callHelper:
      // BRASL R14, jitMonitorExit
      // returnLabel:

      generateRIInstruction(cg, loadImmOp, node, tempReg, ~(LOCK_RES_OWNING_COMPLEMENT));
      generateRRInstruction(cg, andOp, node, tempReg, monitorReg);
      generateRRInstruction(cg, loadRegOp, node, valReg, metaReg);
      generateRIInstruction  (cg, addImmOp, node, valReg, (uintptrj_t) LOCK_RESERVATION_BIT);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
      cFlowRegionStart->setStartInternalControlFlow();
      generateS390CompareAndBranchInstruction(cg, compareOp, node, tempReg, valReg,
         TR::InstOpCode::COND_BNE, callLabel, false, false);

      generateRRInstruction(cg, loadRegOp, node, tempReg, monitorReg);
      generateRILInstruction(cg, andImmOp, node, tempReg,
         isPrimitive ? OBJECT_HEADER_LOCK_RECURSION_MASK : LOCK_RES_NON_PRIMITIVE_EXIT_MASK);

      if (isPrimitive)
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, helperReturnOOLLabel);
      else
         {
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, callLabel/*,conditions*/);
         }
      cg->generateDebugCounter("LockExit/LR/Recursive", 1, TR::DebugCounter::Undetermined);
      generateRIInstruction  (cg, addImmOp, node, monitorReg,
         (uintptrj_t) (isPrimitive ? LOCK_INC_DEC_VALUE : -LOCK_INC_DEC_VALUE) & 0x0000FFFF);
      generateRXInstruction(cg, storeOp, node, monitorReg, generateS390MemoryReference(objReg, lwOffset, cg));

      if (!isPrimitive)
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, helperReturnOOLLabel);
      // call to jithelper
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, callLabel);
      cg->generateDebugCounter("LockExit/LR/VMHelper", 1, TR::DebugCounter::Undetermined);
      uintptrj_t returnAddress = (uintptrj_t) (node->getSymbolReference()->getMethodAddress());
      TR::RegisterDependencyConditions *deps = NULL;
      helperLink->buildDirectDispatch(node, &deps);
      TR::RegisterDependencyConditions *mergeConditions = mergeConditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(ICFConditions, deps, cg);
      instr = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, helperReturnOOLLabel, mergeConditions);
      // OOL return label
      helperReturnOOLLabel->setEndInternalControlFlow();
      if (debugObj)
         {
         debugObj->addInstructionComment(instr, "OOL reservation exit VMHelper return label");
         }
      instr = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, doneOOLLabel);
      if (debugObj)
         {
         if (isPrimitive)
            debugObj->addInstructionComment(instr, "Denotes end of OOL primitive reversation exit sequence: return to mainline");
         else
            debugObj->addInstructionComment(instr, "Denotes end of OOL non-primitive reversation exit sequence: return to mainline");
         }
      outlinedSlowPath->swapInstructionListsWithCompilation(); // Toggle instruction list
      instr = generateS390LabelInstruction(cg,TR::InstOpCode::LABEL,node,doneOOLLabel);
      if (debugObj)
         debugObj->addInstructionComment(instr, "OOL reservation exit return label");

      generateS390LabelInstruction(cg,TR::InstOpCode::LABEL,node, doneLabel);
      }
   else
      {
      TR_ASSERT(0, "Not implemented: Lock reservation with Disable OOL.");
      }

   if (monitorReg)
      cg->stopUsingRegister(monitorReg);
   if (valReg)
      cg->stopUsingRegister(valReg);
   if (tempReg)
      cg->stopUsingRegister(tempReg);

   cg->decReferenceCount(node->getFirstChild());
   return NULL;
   }

// the following routine is a bit grotty - it has to determine if there are any GRA
// assigned real registers that will conflict with real registers required by
// instance-of generation.
// it also has to verify that instance-of won't require more registers than are
// available.
static bool graDepsConflictWithInstanceOfDeps(TR::Node * depNode, TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * castClassNode = node->getSecondChild();
   TR::SymbolReference * castClassSymRef = castClassNode->getSymbolReference();
   TR::Compilation *comp = cg->comp();

   bool testCastClassIsSuper  = TR::TreeEvaluator::instanceOfOrCheckCastNeedSuperTest(node, cg);
   bool isFinalClass          = (castClassSymRef == NULL) ? false : castClassSymRef->isNonArrayFinal(comp);
   bool needsHelperCall       = needHelperCall(node, testCastClassIsSuper, isFinalClass);

   if (maxInstanceOfPostDependencies() + depNode->getNumChildren() > cg->getMaximumNumberOfAssignableGPRs())
      {
      return true;
      }
   if (!needsHelperCall)
      {
      return false;
      }

   for (int i=0; i<depNode->getNumChildren(); i++)
      {
      TR::Node * child = depNode->getChild(i);
      if ((child->getOpCodeValue() == TR::lRegLoad || child->getOpCodeValue() == TR::PassThrough)
          && TR::Compiler->target.is32Bit())
         {
         int32_t regIndex = child->getHighGlobalRegisterNumber();
         if (killedByInstanceOfHelper(regIndex, node, cg))
            {
            return true;
            }

         regIndex = child->getLowGlobalRegisterNumber();
         if (killedByInstanceOfHelper(regIndex, node, cg))
            {
            return true;
            }
         }
      else
         {
         int32_t regIndex = child->getGlobalRegisterNumber();
         if (killedByInstanceOfHelper(regIndex, node, cg))
            {
            return true;
            }
         }
      }
   return false;
   }



/**   \brief Generates ArrayOfJavaLangObjectTest (object class is reference array) for instanceOf or checkCast node
 *    \details
 *    scratchReg1 = load (objectClassReg+offset_romClass)
 *    scratchReg1 = load (ROMClass+J9ROMClass+modifiers)
 *    andImmediate with J9AccClassArray(0x10000)
 *    If not Array -> Branch to Fail Label
 *    testerReg = load (objectClassReg + leafcomponent_offset)
 *    testerReg = load (objectClassReg + offset_romClass)
 *    testerReg = load (objectClassReg + offset_modifiers)
 *    andImmediate with J9AccClassInternalPrimitiveType(0x20000)
 *    if not arrays of primitive set condition code to Zero indicating true result
 */
static
void genInstanceOfOrCheckcastArrayOfJavaLangObjectTest(TR::Node *node, TR::CodeGenerator *cg, TR::Register *objectClassReg, TR::LabelSymbol *failLabel, TR_S390ScratchRegisterManager *srm)
   {
   TR::Compilation *comp = cg->comp();
   TR_Debug *debugObj = cg->getDebug();
   TR::Instruction *cursor = NULL;
   TR::Register *scratchReg1 = srm->findOrCreateScratchRegister();
   generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, scratchReg1, generateS390MemoryReference(objectClassReg, offsetof(J9Class,romClass), cg));
   generateRXInstruction(cg, TR::InstOpCode::L, node, scratchReg1, generateS390MemoryReference(scratchReg1, offsetof(J9ROMClass, modifiers), cg));
   generateRILInstruction(cg, TR::InstOpCode::NILF, node, scratchReg1, static_cast<int32_t>(J9AccClassArray));
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, failLabel);
   if (debugObj)
      debugObj->addInstructionComment(cursor,"Fail instanceOf/checkCast if Not Array");
   generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, scratchReg1, generateS390MemoryReference(objectClassReg, offsetof(J9ArrayClass,componentType), cg));
   generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, scratchReg1, generateS390MemoryReference(scratchReg1, offsetof(J9Class,romClass), cg));
   generateRXInstruction(cg, TR::InstOpCode::L, node, scratchReg1, generateS390MemoryReference(scratchReg1, offsetof(J9ROMClass, modifiers), cg));
   generateRILInstruction(cg, TR::InstOpCode::NILF, node, scratchReg1, static_cast<int32_t>(J9AccClassInternalPrimitiveType));
   srm->reclaimScratchRegister(scratchReg1);
   }


/**   \brief Generates Superclass Test for both checkcast and instanceof nodes.
 *    \details
 *    It will generate pseudocode as follows.
 *    if (objectClassDepth <= castClassDepth) call Helper
 *    else
 *    load superClassArrReg,superClassOfObjectClass
 *    cmp superClassArrReg[castClassDepth], castClass
 *    Here It sets up the condition code for callee to react on.
 */
static
bool genInstanceOfOrCheckcastSuperClassTest(TR::Node *node, TR::CodeGenerator *cg, TR::Register *objClassReg, TR::Register *castClassReg, int32_t castClassDepth,
   TR::LabelSymbol *falseLabel, TR::LabelSymbol *callHelperLabel, TR_S390ScratchRegisterManager *srm)
   {
   TR::Compilation *comp = cg->comp();
   int32_t superClassDepth = castClassDepth * TR::Compiler->om.sizeofReferenceAddress();
   TR::Register *castClassDepthReg = NULL;
   TR::InstOpCode::Mnemonic loadOp;
   int32_t byteOffset;
   TR::Instruction *cursor = NULL;
   if (TR::Compiler->target.is64Bit())
      {
      loadOp = TR::InstOpCode::LLGH;
      byteOffset = 6;
      }
   else
      {
      loadOp = TR::InstOpCode::LLH;
      byteOffset = 2;
      }
   //Following Changes are for dynamicCastClass only
   bool dynamicCastClass = castClassDepth == -1;
   bool eliminateSuperClassArraySizeCheck = (!dynamicCastClass && (castClassDepth < cg->comp()->getOptions()->_minimumSuperclassArraySize));
   // In case of dynamic Cast Class, We do not know the depth of the cast Class at compile time. So following routine compares depth at run time.
   if ( dynamicCastClass )
      {
      TR::Register *scratchRegister1 = srm->findOrCreateScratchRegister();
      //TR::Register *scratchRegister1 = scratch1Reg;
      TR_ASSERT((node->getOpCodeValue() == TR::instanceof &&
            node->getSecondChild()->getOpCodeValue() != TR::loadaddr), "genTestIsSuper: castClassDepth == -1 is only supported for transformed isInstance calls.");
      cursor = generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, scratchRegister1,
            generateS390MemoryReference(castClassReg, offsetof(J9Class, romClass), cg), cursor);
      cursor = generateRXInstruction(cg, TR::InstOpCode::L, node, scratchRegister1,
            generateS390MemoryReference(scratchRegister1, offsetof(J9ROMClass, modifiers), cg), cursor);
      TR_ASSERT(((J9AccInterface | J9AccClassArray) < UINT_MAX && (J9AccInterface | J9AccClassArray) > 0),
            "genTestIsSuper::(J9AccInterface | J9AccClassArray) is not a 32-bit number\n");
      cursor = generateRILInstruction(cg, TR::InstOpCode::NILF, node, scratchRegister1, static_cast<int32_t>((J9AccInterface | J9AccClassArray)), cursor);
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, callHelperLabel, cursor);
      castClassDepthReg = srm->findOrCreateScratchRegister();
      cursor = generateRXInstruction(cg, loadOp, node, castClassDepthReg,
            generateS390MemoryReference(castClassReg, offsetof(J9Class, classDepthAndFlags) + byteOffset, cg), cursor);

      srm->reclaimScratchRegister(scratchRegister1);
      TR_ASSERT(sizeof(((J9Class*)0)->classDepthAndFlags) == sizeof(uintptr_t),
            "genTestIsSuper::J9Class->classDepthAndFlags is wrong size\n");
      }


   //objectClassDepthReg <- objectClassDepth
   if (!eliminateSuperClassArraySizeCheck)
      {
      TR::Register *objectClassDepthReg = srm->findOrCreateScratchRegister();
      cursor = generateRXInstruction(cg, loadOp, node, objectClassDepthReg,
         generateS390MemoryReference(objClassReg, offsetof(J9Class, classDepthAndFlags) + byteOffset, cg) , NULL);

      //Compare objectClassDepth and castClassDepth
      if (dynamicCastClass)
         cursor = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, objectClassDepthReg, castClassDepthReg, TR::InstOpCode::COND_BNH, falseLabel, false, false);
      else
         cursor = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, objectClassDepthReg, castClassDepth, TR::InstOpCode::COND_BNH, falseLabel, true, false, cursor);
      srm->reclaimScratchRegister(objectClassDepthReg);
      }

   //superClassArrReg <- objectClass->superClasses
   TR::Register *superClassArrReg = srm->findOrCreateScratchRegister();
   cursor = generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, superClassArrReg,
      generateS390MemoryReference(objClassReg, offsetof(J9Class, superclasses), cg), cursor);
   if (dynamicCastClass)
      {
      if (TR::Compiler->target.is64Bit())
         {
         cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, castClassDepthReg, castClassDepthReg, 3, cursor);
         }
      else
         {
         cursor = generateRSInstruction(cg, TR::InstOpCode::SLL, node, castClassDepthReg, 2, cursor);
         }
         cursor = generateRXInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, castClassReg,
            generateS390MemoryReference(superClassArrReg, castClassDepthReg, 0, cg), cursor);
         srm->reclaimScratchRegister(castClassDepthReg);
      }
   else
      {
      //CG superClassArrReg[castClassDepth],castClassReg
      cursor = generateRXInstruction (cg, TR::InstOpCode::getCmpOpCode(), node, castClassReg,
         generateS390MemoryReference(superClassArrReg, superClassDepth, cg), cursor);
      }
   srm->reclaimScratchRegister(superClassArrReg);
   return dynamicCastClass;
   //We expect Result of the test reflects in Condition Code. Callee should react on this.
   }

/** \brief
 *     Generates null test of \p objectReg for instanceof or checkcast \p node.
 *
 *  \param node
 *     The instanceof, checkcast, or checkcastAndNULLCHK node.
 *
 *  \param cg
 *     The code generator used to generate the instructions.
 *
 *  \param objectReg
 *     The object which to null test.
 *
 *  \return
 *     <c>true</c> if the null test will implicitly raise an exception; false otherwise.
 *
 *  \details
 *     Note that if this function returns <c>false</c> the appropriate null test condition code will be set and the
 *     callee is responsible for generating the branch instruction to act on the condition code.
 */
static
bool genInstanceOfOrCheckCastNullTest(TR::Node* node, TR::CodeGenerator* cg, TR::Register* objectReg)
   {
   const bool isNullTestImplicit = node->getOpCodeValue() == TR::checkcastAndNULLCHK && cg->getHasResumableTrapHandler();

   if (isNullTestImplicit)
      {
      TR::Instruction* compareAndTrapInsturction = generateRIEInstruction(cg, TR::InstOpCode::getCmpImmTrapOpCode(), node, objectReg, 0, TR::InstOpCode::COND_BE);
      compareAndTrapInsturction->setExceptBranchOp();
      compareAndTrapInsturction->setNeedsGCMap(0x0000FFFF);
      }
   else
      {
      genNullTest(cg, node, objectReg, objectReg, NULL);
      }

      return isNullTestImplicit;
   }


/** \brief
 *     Generates a dynamicCache test with helper call for instanceOf/ifInstanceOf node
 *
 *  \details
 *     This function generates a sequence to check per site cache for object class and cast class before calling out to jitInstanceOf helper
 */
static
void genInstanceOfDynamicCacheAndHelperCall(TR::Node *node, TR::CodeGenerator *cg, TR::Register *castClassReg, TR::Register *objClassReg, TR::Register *resultReg, TR_S390ScratchRegisterManager *srm, TR::LabelSymbol *doneLabel, TR::LabelSymbol *helperCallLabel, TR::LabelSymbol *dynamicCacheTestLabel, TR::LabelSymbol *branchLabel, TR::LabelSymbol *trueLabel, TR::LabelSymbol *falseLabel, bool dynamicCastClass, bool generateDynamicCache, bool cacheCastClass, bool ifInstanceOf, bool trueFallThrough )
   {
   TR::Compilation                *comp = cg->comp();
   bool needResult = resultReg != NULL;
   if (!castClassReg)
      castClassReg = cg->evaluate(node->getSecondChild());
   int32_t maxOnsiteCacheSlots = comp->getOptions()->getMaxOnsiteCacheSlotForInstanceOf();
   TR::Register *dynamicCacheReg = NULL;
   int32_t addressSize = TR::Compiler->om.sizeofReferenceAddress();
   /* Layout of the writable data snippet
    * Case - 1 : Cast class is runtime variable
    * [UpdateIndex][ObjClassSlot-0][CastClassSlot-0]...[ObjClassSlot-N][CastClassSlot-N]
    * Case - 2 : Cast Class is interface / unresolved
    * [UpdateIndex][ObjClassSlot-0]...[ObjClassSlot-N]
    * If there is only one cache slot, we will not have header.
    * Last bit of cached objectClass will set to 1 indicating false cast
    */
   int32_t snippetSizeInBytes = ((cacheCastClass ? 2 : 1) * maxOnsiteCacheSlots * addressSize) + (addressSize * (maxOnsiteCacheSlots != 1));
   if (generateDynamicCache)
      {
      TR::S390WritableDataSnippet *dynamicCacheSnippet = NULL;
      /* We can only request the snippet size of power 2, following table summarizes bytes needed for corresponding number of cache slots
       * Case 1 : Cast class is runtime variable
       * Case 2 : Cast class is interface / unresolved
       * Number Of Slots |  Bytes needed for Case 1 | Bytes needed for Case 2
       *        1        |              16          |           8
       *        2        |              64          |           32
       *        3        |              64          |           32
       *        4        |              128         |           64
       *        5        |              128         |           64
       *        6        |              128         |           64
       */
      int32_t requestedBytes = 1 << (int) (log2(snippetSizeInBytes-1)+1);
      traceMsg(comp, "Requested Bytes = %d\n",requestedBytes);
      // NOTE: For single slot cache, we initialize snippet with addressSize (4/8) assuming which can not be objectClass
      // In all cases, we use first addressSize bytes to store offset of the circular buffer and rest of buffer will be initialized with 0xf.
      TR_ASSERT_FATAL(maxOnsiteCacheSlots <= 7, "Maximum 7 slots per site allowed because we use a fixed stack allocated buffer to construct the snippet\n");
      UDATA initialSnippet[16] = { static_cast<UDATA>(addressSize) };
      dynamicCacheSnippet = (TR::S390WritableDataSnippet*)cg->CreateConstant(node, initialSnippet, requestedBytes, true);

      int32_t currentIndex = maxOnsiteCacheSlots > 1 ? addressSize : 0;
      dynamicCacheReg = srm->findOrCreateScratchRegister();
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, dynamicCacheTestLabel);
      generateRILInstruction(cg, TR::InstOpCode::LARL, node, dynamicCacheReg, dynamicCacheSnippet, 0);
      TR::Register *cachedObjectClass = srm->findOrCreateScratchRegister();
      TR::LabelSymbol *gotoNextTest = NULL;
      /* Dynamic Cache Test
       * LARL dynamicCacheReg, dynamicCacheSnippet
       * if (cacheCastClass)
       *    CG castClassReg, @(dynamicCacheSnippet+currentIndex+addressSize)
       *    BRC NE,isLastCacheSlot ? helperCall:checkNextSlot
       * LG cachedOjectClass,@(dynamicCacheSnippet+currentIndex)
       * XR cachedObjectClass,objClassReg
       * if (isLastCacheSlot && trueFallThrough)
       *    CGIJ cachedObjectClass, 1, Equal, falseLabel
       *    BRC NotZero, helperCallLabel
       * else if (isLastCacheSlot)
       *    BRC Zero, trueLabel
       *    CGIJ cachedObjectClass, 1, NotEqual, helperCallLabel
       * else
       *    BRC Zero ,trueLabel
       *    CGIJ cachedObjectClass,1,Equal,falseLabel
       * if (isLastCacheSlot)
       *    fallThroughLabel:
       * else
       *    checkNextSlot:
       */
      for (auto i=0; i<maxOnsiteCacheSlots; i++)
         {
         if (cacheCastClass)
            {
            gotoNextTest = (i+1 == maxOnsiteCacheSlots) ? helperCallLabel : generateLabelSymbol(cg);
            generateRXInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, castClassReg, generateS390MemoryReference(dynamicCacheReg,currentIndex+addressSize,cg));
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, gotoNextTest);
            }
         generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, cachedObjectClass, generateS390MemoryReference(dynamicCacheReg,currentIndex,cg));
         generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node,cachedObjectClass, objClassReg);
         if (i+1 == maxOnsiteCacheSlots)
            {
            if (trueFallThrough)
               {
               generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, cachedObjectClass, 1, TR::InstOpCode::COND_BE, falseLabel, false, false);
               generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, helperCallLabel);
               }
            else
               {
               generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, trueLabel);
               generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, cachedObjectClass, 1, TR::InstOpCode::COND_BNE, helperCallLabel, false, false);
               }
            }
         else
            {
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, trueLabel);
            generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, cachedObjectClass, 1, TR::InstOpCode::COND_BE, falseLabel, false, false);
            }

         if (gotoNextTest && gotoNextTest != helperCallLabel)
            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, gotoNextTest);
         currentIndex += (cacheCastClass? 2: 1)*addressSize;
         }
      srm->reclaimScratchRegister(cachedObjectClass);
      }
   else if (!dynamicCastClass)
      {
      // If dynamic Cache Test is not generated and it is not dynamicCastClass, we need to generate following branch
      // In cases of dynamic cache test / dynamic Cast Class, we would have a branch to helper call at appropriate location.
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, helperCallLabel);
      }

      /* helperCallLabel : jitInstanceOfCall prologue
       *                   BRASL jitInstanceOf
       *                   jitInstanceOfCall epilogue
       *                   CGIJ helperReturnReg,1,skipSettingBitForFalseResult <- Start of Internal Control Flow
       *                   OILL objClassReg,1
       * skipSettingBitForFalseResult:
       *               Case - 1 : maxOnsiteCacheSlots = 1
       *                   STG objClassReg, @(dynamicCacheReg)
       *                   if (cacheCastClass)
       *                      STG castClassReg, @(dynamicCacheReg,addressSize)
       *               Case - 2 : maxOnsiteCacheSlots > 1
       *                   LG offsetRegister,@(dynamicCacheReg)
       *                   STG objClassReg,@(dynamicCacheReg,offsetRegister)
       *                   if (cacheCastClass)
       *                      STG castClassReg, @(dynamicCacheReg,offsetRegister,addressSize)
       *                   AGHI offsetReg,addressSize
       *                   CIJ offsetReg,snippetSizeInBytes,NotEqual,skipResetOffset
       *                   LGHI offsetReg,addressSize
       * skipResetOffset:
       *                   STG offsetReg,@(dynamicCacheReg) -> End of Internal Control Flow
       *                   LT resultReg,helperReturnReg
       */
   TR_S390OutOfLineCodeSection *outlinedSlowPath = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(helperCallLabel, doneLabel, cg);
   cg->getS390OutOfLineCodeSectionList().push_front(outlinedSlowPath);
      outlinedSlowPath->swapInstructionListsWithCompilation();
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, helperCallLabel);
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOf/(%s)/Helper", comp->signature()),1,TR::DebugCounter::Undetermined);
   TR::S390CHelperLinkage *helperLink =  static_cast<TR::S390CHelperLinkage*>(cg->getLinkage(TR_CHelper));
   resultReg = helperLink->buildDirectDispatch(node, resultReg);
   if (generateDynamicCache)
      {
      TR::LabelSymbol *skipSettingBitForFalseResult = generateLabelSymbol(cg);
      TR::LabelSymbol *cFlowRegionStart = generateLabelSymbol(cg);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
      cFlowRegionStart->setStartInternalControlFlow();
      generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, resultReg, 1, TR::InstOpCode::COND_BE, skipSettingBitForFalseResult, false);
      // We will set the last bit of objectClassRegister to 1 if helper returns false.
      generateRIInstruction(cg, TR::InstOpCode::OILL, node, objClassReg, 0x1);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, skipSettingBitForFalseResult);
      // Update cache sequence
      if (maxOnsiteCacheSlots == 1)
         {
         skipSettingBitForFalseResult->setEndInternalControlFlow();
         generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, objClassReg, generateS390MemoryReference(dynamicCacheReg,0,cg));
         if (cacheCastClass)
            generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, castClassReg, generateS390MemoryReference(dynamicCacheReg,addressSize,cg));
         }
      else
         {
         TR::Register *offsetRegister = srm->findOrCreateScratchRegister();

         TR::LabelSymbol *skipResetOffsetLabel = generateLabelSymbol(cg);

         TR::RegisterDependencyConditions * OOLconditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 5, cg);
         OOLconditions->addPostCondition(objClassReg, TR::RealRegister::AssignAny);
         OOLconditions->addPostCondition(resultReg, TR::RealRegister::AssignAny);
         OOLconditions->addPostCondition(dynamicCacheReg, TR::RealRegister::AssignAny);
         OOLconditions->addPostCondition(offsetRegister, TR::RealRegister::AssignAny);
         if (cacheCastClass)
            OOLconditions->addPostCondition(castClassReg, TR::RealRegister::AssignAny);
         // NOTE: In OOL helper call is not within ICF hence we can avoid passing dependency to helper call dispatch function and stretching it to merge label.
         // Although internal control flow starts after returning from helper we need to define starting point and ending point of internal control flow.
         generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, offsetRegister, generateS390MemoryReference(dynamicCacheReg,0,cg));
         generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, objClassReg, generateS390MemoryReference(dynamicCacheReg,offsetRegister,0,cg));
         if (cacheCastClass)
            generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, castClassReg, generateS390MemoryReference(dynamicCacheReg,offsetRegister,addressSize,cg));
         generateRIInstruction(cg,TR::InstOpCode::getAddHalfWordImmOpCode(),node,offsetRegister,static_cast<int32_t>(cacheCastClass?addressSize*2:addressSize));
         generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::C, node, offsetRegister, snippetSizeInBytes, TR::InstOpCode::COND_BNE, skipResetOffsetLabel, false);
         generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode() , node, offsetRegister, addressSize);
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, skipResetOffsetLabel, OOLconditions);
         skipResetOffsetLabel->setEndInternalControlFlow();

         generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, offsetRegister, generateS390MemoryReference(dynamicCacheReg,0,cg));
         srm->reclaimScratchRegister(offsetRegister);
         }
      srm->reclaimScratchRegister(dynamicCacheReg);
      }

   // WARNING: It is not recommended to have two exit point in OOL section
   // In this case we need it in case of ifInstanceOf to save additional complex logic in mainline section
   // In case if there is GLRegDeps attached to ifInstanceOf node, it will be evaluated and attached as post dependency conditions
   // at the end of node
   // We can take a risk of having two exit points in OOL here as there is no other register instruction between them
   if (ifInstanceOf)
      {
      generateRRInstruction(cg, TR::InstOpCode::getLoadTestRegOpCode(), node, resultReg, resultReg);
      if (trueFallThrough)
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, branchLabel);
      else
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, branchLabel);
      }

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, doneLabel);
   outlinedSlowPath->swapInstructionListsWithCompilation();
   if (!needResult)
      cg->stopUsingRegister(resultReg);
   }

/**   \brief Generates inlined sequence of tests for instanceOf/ifInstanceOf node.
 *    \details
 *    It calls common function to generate list of inlined tests and generates instructions handling both instanceOf and ifInstanceOf case.
 */
TR::Register *
J9::Z::TreeEvaluator::VMgenCoreInstanceofEvaluator2(TR::Node * node, TR::CodeGenerator * cg, TR::LabelSymbol *trueLabel, TR::LabelSymbol *falseLabel,
   bool initialResult, bool needResult, TR::RegisterDependencyConditions *graDeps, bool ifInstanceOf)
   {
   TR::Compilation                *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (comp->fe());
   TR_OpaqueClassBlock           *compileTimeGuessClass;
   int32_t maxProfiledClasses = comp->getOptions()->getCheckcastMaxProfiledClassTests();
   traceMsg(comp, "%s:Maximum Profiled Classes = %d\n", node->getOpCode().getName(),maxProfiledClasses);
   InstanceOfOrCheckCastProfiledClasses profiledClassesList[maxProfiledClasses];

   TR::Node                      *objectNode = node->getFirstChild();
   TR::Node                      *castClassNode = node->getSecondChild();

   TR::Register                  *objectReg = cg->evaluate(objectNode);
   TR::Register                  *objClassReg = NULL;
   TR::Register                  *resultReg = NULL;
   TR::Register                  *castClassReg = NULL;

   // In the evaluator, We need at maximum two scratch registers, so creating a pool of scratch registers with 2 size.
   TR_S390ScratchRegisterManager *srm = cg->generateScratchRegisterManager(2);
   bool topClassWasCastClass=false;
   float topClassProbability=0.0;
   InstanceOfOrCheckCastSequences sequences[InstanceOfOrCheckCastMaxSequences];
   uint32_t numberOfProfiledClass;
   uint32_t                       numSequencesRemaining = calculateInstanceOfOrCheckCastSequences(node, sequences, &compileTimeGuessClass, cg, profiledClassesList, &numberOfProfiledClass, maxProfiledClasses, &topClassProbability, &topClassWasCastClass);
   bool outLinedSuperClass = false;
   TR::Instruction *cursor = NULL;
   TR::Instruction *gcPoint = NULL;

   // We load resultReg with the parameter initialResult when we need result as outcome for routine
   if (needResult)
      {
      resultReg = cg->allocateRegister();
      cursor = generateRIInstruction(cg,TR::InstOpCode::getLoadHalfWordImmOpCode(),node,resultReg,static_cast<int32_t>(initialResult));
      }

   TR_S390OutOfLineCodeSection *outlinedSlowPath = NULL;

   TR::LabelSymbol *doneOOLLabel = NULL;
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *callLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *doneTestCacheLabel = NULL;
   TR::LabelSymbol *oppositeResultLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *helperTrueLabel = NULL;
   TR::LabelSymbol *helperFalseLabel = NULL;
   TR::LabelSymbol *helperReturnLabel = NULL;
   TR::LabelSymbol *dynamicCacheTestLabel = NULL;
   TR::LabelSymbol *branchLabel = NULL;
   TR::LabelSymbol *jmpLabel = NULL;

   TR::InstOpCode::S390BranchCondition branchCond;
   TR_Debug *debugObj = cg->getDebug();
   bool trueFallThrough;
   bool dynamicCastClass = false;
   bool generateGoToFalseBRC = true;

   if (ifInstanceOf)
      {
      if (trueLabel)
         {
         traceMsg(comp,"IfInstanceOf Node : Branch True\n");
         falseLabel = (needResult) ? oppositeResultLabel : doneLabel;
         branchLabel = trueLabel;
         branchCond = TR::InstOpCode::COND_BE;
         jmpLabel = falseLabel;
         trueFallThrough = false;
         }
      else
         {
         traceMsg(comp,"IfInstanceOf Node : Branch False\n");
         trueLabel = (needResult)? oppositeResultLabel : doneLabel;
         branchLabel = falseLabel;
         branchCond = TR::InstOpCode::COND_BNE;
         jmpLabel = trueLabel;
         trueFallThrough = true;
         }
      }
   else
      {
      if (initialResult)
         {
         trueLabel = doneLabel;
         falseLabel = oppositeResultLabel;
         branchCond = TR::InstOpCode::COND_BE;
         trueFallThrough = false;
         }
      else
         {
         trueLabel = oppositeResultLabel;
         falseLabel = doneLabel;
         branchCond = TR::InstOpCode::COND_BNE;
         trueFallThrough = true;
         }
      branchLabel = doneLabel;
      jmpLabel = oppositeResultLabel;
      }

   bool generateDynamicCache = false;
   bool cacheCastClass = false;
   InstanceOfOrCheckCastSequences *iter = &sequences[0];
   while (numSequencesRemaining >   1 || (numSequencesRemaining==1 && *iter!=HelperCall))
      {
      switch (*iter)
         {
         case EvaluateCastClass:
            TR_ASSERT(!castClassReg, "Cast class already evaluated");
            if (comp->getOption(TR_TraceCG))
               traceMsg(comp, "%s: Class Not Evaluated. Evaluating it\n", node->getOpCode().getName());
            castClassReg = cg->evaluate(castClassNode);
            break;
         case LoadObjectClass:
            if (comp->getOption(TR_TraceCG))
               traceMsg(comp, "%s: Loading Object Class\n",node->getOpCode().getName());
            objClassReg = cg->allocateRegister();
            TR::TreeEvaluator::genLoadForObjectHeadersMasked(cg, node, objClassReg, generateS390MemoryReference(objectReg, static_cast<int32_t>(TR::Compiler->om.offsetOfObjectVftField()), cg), NULL);
            break;
         case GoToTrue:
            traceMsg(comp, "%s: Emitting GoToTrue\n", node->getOpCode().getName());
            // If fall through in True (Initial Result False)
            //if (trueLabel != oppositeResultLabel)
            if (trueLabel != oppositeResultLabel  || (ifInstanceOf && !trueFallThrough))
               generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BC, node, trueLabel);
            break;
         case GoToFalse:
            traceMsg(comp, "%s: Emitting GoToFalse\n", node->getOpCode().getName());
            // There is only one case when we generate a GoToFalse branch here, when we have a primitive Cast Class other wise all tests take care of generating terminating sequence
            if (generateGoToFalseBRC)
               generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BC, node, falseLabel);
            break;
         case NullTest:
            {
            if (comp->getOption(TR_TraceCG))
               traceMsg(comp, "%s: Emitting NullTest\n", node->getOpCode().getName());
            TR_ASSERT(!objectNode->isNonNull(), "Object is known to be non-null, no need for a null test");
            bool isNullTestImplicit = genInstanceOfOrCheckCastNullTest(node, cg, objectReg);
            if (!isNullTestImplicit)
               {
               //If object is Null, and initialResult is true, go to oppositeResultLabel else goto done Label
               generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, falseLabel);
               }
            }
            break;
         case ClassEqualityTest:
            if (comp->getOption(TR_TraceCG))
               traceMsg(comp, "%s: Emitting Class Equality Test\n", node->getOpCode().getName());
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/Equality", comp->signature()),1,TR::DebugCounter::Undetermined);
             /*   #IF NextTest = GoToFalse
              *      branchCond = ifInstanceOf ? (!trueFallThrough ? COND_BE : COND_BNE ) : (init=true ? COND_BE : COND_BNE )
              *      branchLabel = ifInstanceOf ? (!trueFallThrough ? trueLabel : falseLabel ) : doneLabel
              *      CGRJ castClassReg, objClassReg, branchCond, branchLabel
              *   #ELSE
              *      CGRJ castClassReg, objClassReg, COND_BE, trueLabel
              */
            if ( *(iter+1) == GoToFalse )
               {
               cursor = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, castClassReg, objClassReg, branchCond, branchLabel, false, false);
               generateGoToFalseBRC = false;
               }
            else
               {
               cursor = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, castClassReg, objClassReg, TR::InstOpCode::COND_BE, trueLabel, false, false);
               generateGoToFalseBRC = true;
               }
            if (debugObj)
               debugObj->addInstructionComment(cursor, "ClassEqualityTest");
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/EqualityFail", comp->signature()),1,TR::DebugCounter::Undetermined);
            break;
         case SuperClassTest:
            {
            /*** genInstanceOfOrCheckcastSuperClassTest generates sequences for Super Class Test handling all cases when we have a normal static class or dynamic class
               * Mostly this will be last test except in case of dynamic cast class.
               * case-1 instanceof , initial Result = false: BRC 0x8, doneLabel
               * case-2 instanceof , initial Result = true: BRC 0x6, doneLabel
               * case-3 ifInstanceOf , trueLabel == branchLabel : BRC 0x8, branchLabel
               * case-4 ifInstanceOf , falseLabel == branchLabel : BRC 0x6, branchLabel
               */
            int32_t castClassDepth = castClassNode->getSymbolReference()->classDepth(comp);
            dynamicCacheTestLabel = generateLabelSymbol(cg);
            if (comp->getOption(TR_TraceCG))
               traceMsg(comp, "%s: Emitting Super Class Test, Cast Class Depth = %d\n", node->getOpCode().getName(),castClassDepth);
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/SuperClassTest", comp->signature()),1,TR::DebugCounter::Undetermined);
            // For dynamic cast class genInstanceOfOrCheckcastSuperClassTest will generate branch to either helper call or dynamicCacheTest depending on the next generated test.
            dynamicCastClass = genInstanceOfOrCheckcastSuperClassTest(node, cg, objClassReg, castClassReg, castClassDepth, falseLabel, *(iter+1) == DynamicCacheDynamicCastClassTest ? dynamicCacheTestLabel : callLabel, srm);
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, branchCond, node, branchLabel);
            // If next test is dynamicCacheTest then generate a Branch to Skip it.
            if (*(iter+1) == DynamicCacheDynamicCastClassTest)
               generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BC, node, jmpLabel);
            generateGoToFalseBRC=false;
            break;
            }
         /**   Following switch case generates sequence of instructions for profiled class test for instanceOf node
          *    arbitraryClassReg1 <= profiledClass
          *    if (arbitraryClassReg1 == objClassReg)
          *       profiledClassIsInstanceOfCastClass ? return true : return false
          *    else
          *       continue to NextTest
          */
         case ProfiledClassTest:
            {
            if (comp->getOption(TR_TraceCG))
               traceMsg(comp, "%s: Emitting ProfiledClass Test\n", node->getOpCode().getName());
            TR::Register *arbitraryClassReg1 = srm->findOrCreateScratchRegister();
            uint8_t numPICs = 0;
            TR::Instruction *temp= NULL;
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/Profile", comp->signature()),1,TR::DebugCounter::Undetermined);
            while (numPICs < numberOfProfiledClass)
               {
               if (cg->needClassAndMethodPointerRelocations())
                  temp = generateRegLitRefInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, arbitraryClassReg1, (uintptrj_t) profiledClassesList[numPICs].profiledClass, TR_ClassPointer, NULL, NULL, NULL);
               else
                  temp = generateRILInstruction(cg, TR::InstOpCode::LARL, node, arbitraryClassReg1, profiledClassesList[numPICs].profiledClass);

               // Adding profiled class to the static PIC slots.
               if (fej9->isUnloadAssumptionRequired((TR_OpaqueClassBlock *)(profiledClassesList[numPICs].profiledClass), comp->getCurrentMethod()))
                  comp->getStaticPICSites()->push_front(temp);
               // Adding profiled class to static HCR PIC sites.
               if (cg->wantToPatchClassPointer(profiledClassesList[numPICs].profiledClass, node))
                  comp->getStaticHCRPICSites()->push_front(temp);

               if (profiledClassesList[numPICs].isProfiledClassInstanceOfCastClass)
                  generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, arbitraryClassReg1, objClassReg, TR::InstOpCode::COND_BE, trueLabel, false, false);
               else
                  generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, arbitraryClassReg1, objClassReg, TR::InstOpCode::COND_BE, falseLabel, false, false);
               numPICs++;
               }
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/ProfileFail", comp->signature()),1,TR::DebugCounter::Undetermined);
            srm->reclaimScratchRegister(arbitraryClassReg1);
            break;
            }
         /**   In case of Single Implementer of the Interface,
          *    arbitraryClassReg1 <= compileTimeGuessClass
          *    CGRJ arbitraryClassReg,objClassReg,0x8,trueLabel
          */
         case CompileTimeGuessClassTest:
            {
            TR::Register *arbitraryClassReg2 = srm->findOrCreateScratchRegister();
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/compTimeGuess", comp->signature()),1,TR::DebugCounter::Undetermined);
            genLoadAddressConstant(cg, node, (uintptrj_t)compileTimeGuessClass, arbitraryClassReg2);
            generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, arbitraryClassReg2, objClassReg, TR::InstOpCode::COND_BE, trueLabel, false, false);
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/compTimeGuessFail", comp->signature()),1,TR::DebugCounter::Undetermined);
            srm->reclaimScratchRegister(arbitraryClassReg2);
            break;
            }
         case ArrayOfJavaLangObjectTest:
            {
            if (comp->getOption(TR_TraceCG))
               traceMsg(comp,"Emitting ArrayOfJavaLangObjectTest\n",node->getOpCode().getName());
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfStats/(%s)/ArrayTest", comp->signature()),1,TR::DebugCounter::Undetermined);
            genInstanceOfOrCheckcastArrayOfJavaLangObjectTest(node, cg, objClassReg, falseLabel, srm) ;
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, branchCond, node, branchLabel);
            generateGoToFalseBRC = false;
            break;
            }
         /**   Following switch case generates sequence of instructions for cast class cache test
          *    Load castClassCacheReg, offsetOf(J9Class,castClassCache)
          *    castClassCacheReg <= castClassCacheReg XOR castClassReg
          *    if castClassCacheReg == 0 (Success)
          *       return true
          *    else if castClassCacheReg == 1 (Failed instanceOf)
          *       return false
          *    else
          *       continue
          */
         case CastClassCacheTest:
            {
            doneTestCacheLabel =  generateLabelSymbol(cg);
            if (comp->getOption(TR_TraceCG))
               traceMsg(comp,"Emitting CastClassCacheTest\n",node->getOpCode().getName());
            TR::Register *castClassCacheReg = srm->findOrCreateScratchRegister();
            generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, castClassCacheReg,
               generateS390MemoryReference(objClassReg, offsetof(J9Class, castClassCache), cg));
            generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, castClassCacheReg, castClassReg);
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, trueLabel);
            generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, castClassCacheReg, 1, TR::InstOpCode::COND_BE, falseLabel, false, false);
            srm->reclaimScratchRegister(castClassCacheReg);
            break;
            }
         case DynamicCacheObjectClassTest:
            {
            generateDynamicCache = true;
            dynamicCacheTestLabel = generateLabelSymbol(cg);
            if (comp->getOption(TR_TraceCG))
               traceMsg(comp,"Emitting Dynamic Cache for ObjectClass only\n",node->getOpCode().getName());
            break;
            }
         case DynamicCacheDynamicCastClassTest:
            {
            generateDynamicCache = true;
            cacheCastClass = true;
            TR_ASSERT(dynamicCacheTestLabel!=NULL, "DynamicCacheDynamicCastClassTest: dynamicCacheTestLabel should be generated by SuperClassTest before reaching this point");
            if (comp->getOption(TR_TraceCG))
               traceMsg(comp,"Emitting Dynamic Cache for CastClass and ObjectClass\n",node->getOpCode().getName());
            break;
            }
         case HelperCall:
            TR_ASSERT(false, "Doesn't make sense, HelperCall should be the terminal sequence");
            break;
         default:
            break;
         }
      --numSequencesRemaining;
      ++iter;
      }

   if (numSequencesRemaining > 0 && *iter == HelperCall)
      genInstanceOfDynamicCacheAndHelperCall(node, cg, castClassReg, objClassReg, resultReg, srm, doneLabel, callLabel, dynamicCacheTestLabel, branchLabel, trueLabel, falseLabel, dynamicCastClass, generateDynamicCache, cacheCastClass, ifInstanceOf, trueFallThrough);

   if (needResult)
      {
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, oppositeResultLabel);
      generateRIInstruction(cg,TR::InstOpCode::getLoadHalfWordImmOpCode(),node,resultReg,static_cast<int32_t>(!initialResult));
      }

   TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(graDeps, 0, 4+srm->numAvailableRegisters(), cg);
   if (objClassReg)
      conditions->addPostCondition(objClassReg, TR::RealRegister::AssignAny);
   if (needResult)
      conditions->addPostCondition(resultReg, TR::RealRegister::AssignAny);
   conditions->addPostConditionIfNotAlreadyInserted(objectReg, TR::RealRegister::AssignAny);
   if (castClassReg)
      conditions->addPostConditionIfNotAlreadyInserted(castClassReg, TR::RealRegister::AssignAny);
   srm->addScratchRegistersToDependencyList(conditions);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneLabel, conditions);
   if (objClassReg)
      cg->stopUsingRegister(objClassReg);
   if (castClassReg)
      cg->stopUsingRegister(castClassReg);
   srm->stopUsingRegisters();
   cg->decReferenceCount(objectNode);
   cg->decReferenceCount(castClassNode);
   if (needResult)
      node->setRegister(resultReg);
   return resultReg;
   }

/**   \brief Sets up parameters for VMgenCoreInstanceOfEvaluator2 when we have a ifInstanceOf node
 *    \details
 *    For ifInstanceOf node, it checks if the node has GRA dependency node as third child and if it has, calls normal instanceOf
 *    Otherwise calls VMgenCoreInstanceOfEvaluator2with parameters to generate instructions for ifInstanceOf.
 */
TR::Register *
J9::Z::TreeEvaluator::VMifInstanceOfEvaluator2(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node * graDepNode = NULL;
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR::Node * instanceOfNode = node->getFirstChild();
   TR::Node * valueNode     = node->getSecondChild();
   int32_t value = valueNode->getInt();
   TR::LabelSymbol * branchLabel = node->getBranchDestination()->getNode()->getLabel();
   TR::RegisterDependencyConditions * graDeps = NULL;

   TR::LabelSymbol * falseLabel = NULL;
   TR::LabelSymbol * trueLabel = NULL;

   if (node->getNumChildren() == 3)
      {
      graDepNode = node->getChild(2);
      }

   if (graDepNode && graDepsConflictWithInstanceOfDeps(graDepNode, instanceOfNode, cg))
      {
      return (TR::Register*) 1;
      }

   bool needResult = (instanceOfNode->getReferenceCount() > 1);

   if ((opCode == TR::ificmpeq && value == 1) || (opCode != TR::ificmpeq && value == 0))
      trueLabel       = branchLabel;
   else
      falseLabel      = branchLabel;

   if (graDepNode)
      {
      cg->evaluate(graDepNode);
      graDeps = generateRegisterDependencyConditions(cg, graDepNode, 0);
      }
   bool initialResult = trueLabel != NULL;

   VMgenCoreInstanceofEvaluator2(instanceOfNode, cg, trueLabel, falseLabel, initialResult, needResult, graDeps, true);

   cg->decReferenceCount(instanceOfNode);
   node->setRegister(NULL);

   return NULL;
   }


TR::Register *
J9::Z::TreeEvaluator::VMifInstanceOfEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation         *comp = cg->comp();
   static bool newIfInstanceOf = (feGetEnv("TR_oldInstanceOf")) == NULL;
   // We have support for new C helper functions with new instanceOf evaluator so bydefault we are calling it.
   if (true)
      return VMifInstanceOfEvaluator2(node,cg);

   TR::Node * graDepNode = NULL;

   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR::Node * instanceOfNode = node->getFirstChild();
   TR::Node * castClassNode = instanceOfNode->getSecondChild();
   TR::Node * objectNode    = instanceOfNode->getFirstChild();
   TR::Node * valueNode     = node->getSecondChild();
   int32_t value = valueNode->getInt();
   TR::LabelSymbol * branchLabel = node->getBranchDestination()->getNode()->getLabel();
   TR::RegisterDependencyConditions * graDeps = NULL;

   TR::LabelSymbol * falseLabel;
   TR::LabelSymbol * trueLabel;
   bool trueFallThrough;

   // GRA
   // If the result itself is assigned to a global register, we still have to do
   // something special ......
   if (node->getNumChildren() == 3)
      {
      graDepNode = node->getChild(2);
      }

   // Fast path failure check
   //  TODO: For now we cannot handle Global regs in this path
   //        due to possible collision with call out deps.
   if (graDepNode && graDepsConflictWithInstanceOfDeps(graDepNode, instanceOfNode, cg))
      {
      return (TR::Register*) 1;
      }

   // If the result itself is assigned to a global register, we still have to
   // evaluate it
   int32_t needResult = (instanceOfNode->getReferenceCount() > 1);

   if ((opCode == TR::ificmpeq && value == 1) || (opCode != TR::ificmpeq && value == 0))
      {
      falseLabel      = NULL;
      trueLabel       = branchLabel;
      trueFallThrough = false;
      }
   else
      {
      trueLabel       = NULL;
      falseLabel      = branchLabel;
      trueFallThrough = true;
      }

   TR::Register * objectReg    = cg->evaluate(objectNode);
   TR::Register * castClassReg = cg->evaluate(castClassNode);

   // GRA
   if (graDepNode)
      {
      cg->evaluate(graDepNode);
      graDeps = generateRegisterDependencyConditions(cg, graDepNode, 0);
      }

   TR::TreeEvaluator::VMgenCoreInstanceofEvaluator(instanceOfNode, cg, falseLabel, trueLabel, needResult, trueFallThrough, graDeps, true);

   cg->decReferenceCount(instanceOfNode);
   node->setRegister(NULL);

   return NULL;
   }

/**   \brief Sets up parameters for VMgenCoreInstanceOfEvaluator2 when we have a instanceOf node
 */

TR::Register *
J9::Z::TreeEvaluator::VMinstanceOfEvaluator2(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation            *comp = cg->comp();
   static bool initialResult = feGetEnv("TR_instanceOfInitialValue") != NULL;
   traceMsg(comp,"Initial result = %d\n",initialResult);
   // Complementing Initial Result to True if the floag is not passed.
   return VMgenCoreInstanceofEvaluator2(node,cg,NULL,NULL,!initialResult,1,NULL,false);
   }

TR::Register *
J9::Z::TreeEvaluator::VMinstanceOfEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   static bool newinstanceOf = (feGetEnv("TR_oldInstanceOf")) == NULL;
   // We have support for new C helper functions with new instanceOf evaluator so by default we are calling it.
   if (true)
      return VMinstanceOfEvaluator2(node,cg);
   TR::Node * objectNode       = node->getFirstChild();
   TR::Node * castClassNode    = node->getSecondChild();
   TR::Register * objectReg    = cg->evaluate(objectNode);
   TR::Register * castClassReg = cg->evaluate(castClassNode);

   return TR::TreeEvaluator::VMgenCoreInstanceofEvaluator(node, cg, NULL, NULL, true, true, NULL);
   }

/**   \brief Generates Sequence of inline tests for checkcast node.
 *    \details
 *    We call common function that generates an array of inline tests we need to generate for this node
 */
TR::Register *
J9::Z::TreeEvaluator::VMcheckcastEvaluator2(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation                *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (comp->fe());
   TR_OpaqueClassBlock           *profiledClass, *compileTimeGuessClass;

   int32_t maxProfiledClasses = comp->getOptions()->getCheckcastMaxProfiledClassTests();
   traceMsg(comp, "%s:Maximum Profiled Classes = %d\n", node->getOpCode().getName(),maxProfiledClasses);
   InstanceOfOrCheckCastProfiledClasses profiledClassesList[maxProfiledClasses];
   InstanceOfOrCheckCastSequences sequences[InstanceOfOrCheckCastMaxSequences];

   // We use this information to decide if we want to do SuperClassTest inline or not
   bool topClassWasCastClass=false;
   float topClassProbability=0.0;
   bool dynamicCastClass = false;
   uint32_t numberOfProfiledClass;
   uint32_t                       numSequencesRemaining = calculateInstanceOfOrCheckCastSequences(node, sequences, &compileTimeGuessClass, cg, profiledClassesList, &numberOfProfiledClass, maxProfiledClasses, &topClassProbability, &topClassWasCastClass);

   TR::Node                      *objectNode = node->getFirstChild();
   TR::Node                      *castClassNode = node->getSecondChild();
   TR::Register                  *objectReg = NULL;
   TR::Register                  *castClassReg = NULL;
   TR::Register                  *objClassReg = NULL;
   TR::Register                  *objectCopyReg = NULL;
   TR::Register                  *castClassCopyReg = NULL;
   TR::Register                  *resultReg = NULL;

   // We need here at maximum two scratch registers so forcing scratchRegisterManager to create pool of two registers only.
   TR_S390ScratchRegisterManager *srm = cg->generateScratchRegisterManager(2);

   TR::Instruction *gcPoint = NULL;
   TR::Instruction *cursor = NULL;
   TR_S390OutOfLineCodeSection *outlinedSlowPath = NULL;
   TR::LabelSymbol *doneOOLLabel = NULL;
   TR::LabelSymbol *startOOLLabel = NULL;
   TR::LabelSymbol *helperReturnOOLLabel = NULL;
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *callLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *resultLabel = doneLabel;

   TR_Debug * debugObj = cg->getDebug();
   objectReg = cg->evaluate(objectNode);

   // When we topProfiledClass in the profiled information is cast class with frequency greater than 0.5, we expect class equality to succeed so we put rest of the test outlined.
   bool outLinedTest = numSequencesRemaining >= 2 && sequences[numSequencesRemaining-2] == SuperClassTest && topClassProbability >= 0.5 && topClassWasCastClass;
   traceMsg(comp, "Outline Super Class Test: %d\n", outLinedTest);
   InstanceOfOrCheckCastSequences *iter = &sequences[0];

   while (numSequencesRemaining > 1)
      {
      switch(*iter)
         {
         case EvaluateCastClass:
            TR_ASSERT(!castClassReg, "Cast class already evaluated");
            if (comp->getOption(TR_TraceCG))
               traceMsg(comp, "%s: Class Not Evaluated. Evaluating it\n", node->getOpCode().getName());
            castClassReg = cg->evaluate(castClassNode);
            break;
         case LoadObjectClass:
            if (comp->getOption(TR_TraceCG))
               traceMsg(comp, "%s: Loading Object Class\n",node->getOpCode().getName());
            objClassReg = cg->allocateRegister();
            TR::TreeEvaluator::genLoadForObjectHeadersMasked(cg, node, objClassReg, generateS390MemoryReference(objectReg, static_cast<int32_t>(TR::Compiler->om.offsetOfObjectVftField()), cg), NULL);
            break;
         case GoToTrue:
            TR_ASSERT(false, "Doesn't Make sense, GoToTrue should not be part of multiple sequences");
            break;
         case GoToFalse:
            TR_ASSERT(false, "Doesn't make sense, GoToFalse should be the terminal sequence");
            break;
         case NullTest:
            {
            //If Object is Null, no need to carry out rest of test and jump to Done Label
            if (comp->getOption(TR_TraceCG))
               traceMsg(comp, "%s: Emitting NullTest\n", node->getOpCode().getName());
            TR_ASSERT(!objectNode->isNonNull(), "Object is known to be non-null, no need for a null test");
            bool isNullTestImplicit = genInstanceOfOrCheckCastNullTest(node, cg, objectReg);
            if (!isNullTestImplicit)
               {
               generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, doneLabel);
               }
            }
            break;
         case ClassEqualityTest:
            if (comp->getOption(TR_TraceCG))
               traceMsg(comp, "%s: Emitting Class Equality Test\n", node->getOpCode().getName());
            if (outLinedTest && !comp->getOption(TR_DisableOOL))
               {
               // This is the case when we are going to have an Internal Control Flow in the OOL
               startOOLLabel = generateLabelSymbol(cg);
               doneOOLLabel = doneLabel;
               helperReturnOOLLabel = generateLabelSymbol(cg);
               cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/EqualOOL", comp->signature()),1,TR::DebugCounter::Undetermined);
               generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, castClassReg, objClassReg, TR::InstOpCode::COND_BNE, startOOLLabel, false, false);
               cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/EqualOOLPass", comp->signature()),1,TR::DebugCounter::Undetermined);
               outlinedSlowPath = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(startOOLLabel,doneOOLLabel,cg);
               cg->getS390OutOfLineCodeSectionList().push_front(outlinedSlowPath);
               outlinedSlowPath->swapInstructionListsWithCompilation();
               generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, startOOLLabel);
               resultLabel = helperReturnOOLLabel;
               }
            else
               {
               cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/Equal", comp->signature()),1,TR::DebugCounter::Undetermined);
               generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, castClassReg, objClassReg, TR::InstOpCode::COND_BE, doneLabel, false, false);
               cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/EqualFail", comp->signature()),1,TR::DebugCounter::Undetermined);
               }
            break;
         case SuperClassTest:
            {
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/SuperClass", comp->signature()),1,TR::DebugCounter::Undetermined);
            int32_t castClassDepth = castClassNode->getSymbolReference()->classDepth(comp);
            TR_ASSERT(numSequencesRemaining == 2, "SuperClassTest should always be followed by a GoToFalse and must always be the second last test generated");
            if (comp->getOption(TR_TraceCG))
               traceMsg(comp, "%s: Emitting Super Class Test, Cast Class Depth=%d\n", node->getOpCode().getName(),castClassDepth);
            dynamicCastClass = genInstanceOfOrCheckcastSuperClassTest(node, cg, objClassReg, castClassReg, castClassDepth, callLabel, NULL, srm);
            /* outlinedSlowPath will be non-NULL if we have a higher probability of ClassEqualityTest succeeding.
             * In such cases we will do rest of the tests in OOL section, and as such we need to skip the helper call
             * if the result of SuperClassTest is true and branch to resultLabel which will branch back to the doneLabel from OOL code.
             * In normal cases SuperClassTest will be inlined with doneLabel as fallThroughLabel so we need to branch to callLabel to generate CastClassException
             * through helper call if result of SuperClassTest turned out to be false.
             */
            cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, outlinedSlowPath != NULL ? TR::InstOpCode::COND_BE : TR::InstOpCode::COND_BNE, node, outlinedSlowPath ? resultLabel : callLabel);
            break;
            }
         /**   Following switch case generates sequence of instructions for profiled class test for this checkCast node
          *    arbitraryClassReg1 <= profiledClass
          *    if (arbitraryClassReg1 == objClassReg)
          *       JMP DoneLabel
          *    else
          *       continue to NextTest
          */
         case ProfiledClassTest:
            {
            if (comp->getOption(TR_TraceCG))
               traceMsg(comp, "%s: Emitting Profiled Class Test\n", node->getOpCode().getName());
            TR::Register *arbitraryClassReg1 = srm->findOrCreateScratchRegister();
            uint8_t numPICs = 0;
            TR::Instruction *temp= NULL;
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/Profiled", comp->signature()),1,TR::DebugCounter::Undetermined);
            while (numPICs < numberOfProfiledClass)
               {
               if (cg->needClassAndMethodPointerRelocations())
                  temp = generateRegLitRefInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, arbitraryClassReg1, (uintptrj_t) profiledClassesList[numPICs].profiledClass, TR_ClassPointer, NULL, NULL, NULL);
               else
                  temp = generateRILInstruction(cg, TR::InstOpCode::LARL, node, arbitraryClassReg1, profiledClassesList[numPICs].profiledClass);

               // Adding profiled classes to static PIC sites
               if (fej9->isUnloadAssumptionRequired((TR_OpaqueClassBlock *)(profiledClassesList[numPICs].profiledClass), comp->getCurrentMethod()))
                  comp->getStaticPICSites()->push_front(temp);
               // Adding profiled classes to HCR PIC sites
               if (cg->wantToPatchClassPointer(profiledClassesList[numPICs].profiledClass, node))
                  comp->getStaticHCRPICSites()->push_front(temp);

               temp = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, arbitraryClassReg1, objClassReg, TR::InstOpCode::COND_BE, resultLabel, false, false);
               numPICs++;
               }
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/ProfiledFail", comp->signature()),1,TR::DebugCounter::Undetermined);
            srm->reclaimScratchRegister(arbitraryClassReg1);
            break;
            }
         case CompileTimeGuessClassTest:
            {
            if (comp->getOption(TR_TraceCG))
               traceMsg(comp, "%s: Emitting Compile Time Guess Class Test\n", node->getOpCode().getName());
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/CompTimeGuess", comp->signature()),1,TR::DebugCounter::Undetermined);
            TR::Register *arbitraryClassReg2 = srm->findOrCreateScratchRegister();
            genLoadAddressConstant(cg, node, (uintptrj_t)compileTimeGuessClass, arbitraryClassReg2);
            cursor = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, arbitraryClassReg2, objClassReg, TR::InstOpCode::COND_BE, resultLabel , false, false);
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/CompTimeFail", comp->signature()),1,TR::DebugCounter::Undetermined);
            srm->reclaimScratchRegister(arbitraryClassReg2);
            break;
            }
         case ArrayOfJavaLangObjectTest:
            {
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/ArrayTest", comp->signature()),1,TR::DebugCounter::Undetermined);
            if (comp->getOption(TR_TraceCG))
               traceMsg(comp,"%s: Emitting ArrayOfJavaLangObjectTest\n",node->getOpCode().getName());
            genInstanceOfOrCheckcastArrayOfJavaLangObjectTest(node, cg, objClassReg, callLabel, srm) ;
            cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, doneLabel);
            break;
            }
         /**   Following switch case generates sequence of instructions for cast class cache test for this checkCast node
          *    Load castClassCacheReg, offsetOf(J9Class,castClassCache)
          *    if castClassCacheReg == castClassReg
          *       JMP DoneLabel
          *    else
          *       continue to NextTest
          */
         case CastClassCacheTest:
            {
            if (comp->getOption(TR_TraceCG))
               traceMsg(comp,"%s: Emitting CastClassCacheTest\n",node->getOpCode().getName());
            TR::Register *castClassCacheReg = srm->findOrCreateScratchRegister();
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/Cache", comp->signature()),1,TR::DebugCounter::Undetermined);
            generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, castClassCacheReg,
               generateS390MemoryReference(objClassReg, offsetof(J9Class, castClassCache), cg));
            cursor = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, castClassCacheReg, castClassReg, TR::InstOpCode::COND_BE, resultLabel , false, false);
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/CacheFail", comp->signature()),1,TR::DebugCounter::Undetermined);
            srm->reclaimScratchRegister(castClassCacheReg);
            break;
            }
         case HelperCall:
            TR_ASSERT(false, "Doesn't make sense, HelperCall should be the terminal sequence");
            break;
         default:
            break;
         }
      --numSequencesRemaining;
      ++iter;
      }

   TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 7+srm->numAvailableRegisters(), cg);
   TR::RegisterDependencyConditions *outlinedConditions = NULL;

   // In case of Higher probability of quality test to pass, we put rest of the test outlined
   if (!outlinedSlowPath)
      outlinedConditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg);
   else
      outlinedConditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4+srm->numAvailableRegisters(), cg);

   conditions->addPostCondition(objectReg, TR::RealRegister::AssignAny);
   if (objClassReg)
      conditions->addPostCondition(objClassReg, TR::RealRegister::AssignAny);


   srm->addScratchRegistersToDependencyList(conditions);
   TR::S390CHelperLinkage *helperLink =  static_cast<TR::S390CHelperLinkage*>(cg->getLinkage(TR_CHelper));
   // We will be generating sequence to call Helper if we have either GoToFalse or HelperCall Test
   if (numSequencesRemaining > 0 && *iter != GoToTrue)
      {

      TR_ASSERT(*iter == HelperCall || *iter == GoToFalse, "Expecting helper call or fail here");
      bool helperCallForFailure = *iter != HelperCall;
      if (comp->getOption(TR_TraceCG))
         traceMsg(comp, "%s: Emitting helper call%s\n", node->getOpCode().getName(),helperCallForFailure?" for failure":"");
      //Following code is needed to put the Helper Call Outlined.
      if (!comp->getOption(TR_DisableOOL) && !outlinedSlowPath)
         {
         // As SuperClassTest is the costliest test and is guaranteed to give results for checkCast node. Hence it will always be second last test
         // in iter array followed by GoToFalse as last test for checkCastNode
         if ( *(iter-1) != SuperClassTest)
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, callLabel);
         doneOOLLabel = doneLabel;
         helperReturnOOLLabel = generateLabelSymbol(cg);
         outlinedSlowPath = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(callLabel,doneOOLLabel,cg);
         cg->getS390OutOfLineCodeSectionList().push_front(outlinedSlowPath);
         outlinedSlowPath->swapInstructionListsWithCompilation();
         }


      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, callLabel);
      outlinedConditions->addPostCondition(objectReg, TR::RealRegister::AssignAny);
      if (outLinedTest)
         {
         outlinedConditions->addPostCondition(objClassReg, TR::RealRegister::AssignAny);
         srm->addScratchRegistersToDependencyList(outlinedConditions);
         }

      if(!castClassReg)
         castClassReg = cg->evaluate(castClassNode);
      conditions->addPostCondition(castClassReg, TR::RealRegister::AssignAny);
      outlinedConditions->addPostCondition(castClassReg, TR::RealRegister::AssignAny);
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCast/(%s)/Helper", comp->signature()),1,TR::DebugCounter::Undetermined);
      TR::RegisterDependencyConditions *deps = NULL;
      resultReg = startOOLLabel ? helperLink->buildDirectDispatch(node, &deps) : helperLink->buildDirectDispatch(node);
      if (resultReg)
         outlinedConditions->addPostCondition(resultReg, TR::RealRegister::AssignAny);

      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/HelperCall", comp->signature()),1,TR::DebugCounter::Undetermined);
      if(outlinedSlowPath)
         {
         TR::RegisterDependencyConditions *mergeConditions = NULL;
         if (startOOLLabel)
            mergeConditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(outlinedConditions, deps, cg);
         else
            mergeConditions = outlinedConditions;
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, helperReturnOOLLabel, mergeConditions);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, doneOOLLabel);
         outlinedSlowPath->swapInstructionListsWithCompilation();
         }
      }
   if (resultReg)
      cg->stopUsingRegister(resultReg);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneLabel, conditions);
   cg->stopUsingRegister(castClassReg);
   if (objClassReg)
      cg->stopUsingRegister(objClassReg);
   srm->stopUsingRegisters();
   cg->decReferenceCount(objectNode);
   cg->decReferenceCount(castClassNode);
   return NULL;
   }

TR::Register *
J9::Z::TreeEvaluator::VMcheckcastEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   static bool newCheckCast = (feGetEnv("TR_oldCheckCast") == NULL);
   // We have support for new C helper functions with new checkCast evaluator so bydefault we are calling it.
   if (true)
      return VMcheckcastEvaluator2(node, cg);
   TR::Register * objReg, * castClassReg, * objClassReg, * scratch1Reg, * scratch2Reg;
   TR::LabelSymbol * doneLabel, * callLabel, * startOOLLabel, * doneOOLLabel, *helperReturnOOLLabel, *resultLabel, *continueLabel;
   TR::Node * objNode, * castClassNode;
   TR::RegisterDependencyConditions * conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(6, 7, cg);
   TR::Instruction * gcPoint;
   TR::Register * litPoolBaseReg=NULL;
   bool objRegMustBeKilled = false;
   TR::Register * compareReg = NULL;
   objNode = node->getFirstChild();
   castClassNode = node->getSecondChild();
   TR::SymbolReference * castClassSymRef = castClassNode->getSymbolReference();
   TR_Debug * debugObj = cg->getDebug();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   bool testEqualClass        = instanceOfOrCheckCastNeedEqualityTest(node, cg);
   bool testCastClassIsSuper  = instanceOfOrCheckCastNeedSuperTest(node, cg);
   bool isFinalClass          = (castClassSymRef == NULL) ? false : castClassSymRef->isNonArrayFinal(comp);
   bool needsHelperCall       = needHelperCall(node, testCastClassIsSuper, isFinalClass);
   bool testCache             = needTestCache(true, needsHelperCall, testCastClassIsSuper);
   bool needsNullTest         = !objNode->isNonNull() && !node->chkIsReferenceNonNull();
   bool nullCCSet             = false;

   bool isCheckcastAndNullChk = (node->getOpCodeValue() == TR::checkcastAndNULLCHK);

   castClassReg = cg->gprClobberEvaluate(castClassNode);

   objClassReg = cg->allocateRegister();
   scratch1Reg = cg->allocateRegister();

   // Find instances where L/LTR could be replaced by an ICM instruction
   if (needsNullTest)
      {
      nullCCSet = true;
      if(cg->getHasResumableTrapHandler() && isCheckcastAndNullChk)
         {
         compareReg = objReg = cg->evaluate(objNode);
         TR::S390RIEInstruction* cursor =
            new (cg->trHeapMemory()) TR::S390RIEInstruction(TR::InstOpCode::getCmpImmTrapOpCode(), node, objReg, (int16_t)0, TR::InstOpCode::COND_BE, cg);
         cursor->setExceptBranchOp();
         cursor->setNeedsGCMap(0x0000FFFF);
         }
      else if (needsNullTest &&
          !objNode->getRegister() &&
          !objNode->getOpCode().isLoadConst() &&
           (objNode->getOpCode().isLoad() || objNode->getOpCode().isLoadIndirect()) &&
          !(objNode->getSymbolReference()->isLiteralPoolAddress()))
         {
         TR::MemoryReference * tempMR;
         TR::Symbol * sym = objNode->getSymbolReference()->getSymbol();

         if ((objNode->getOpCode().isLoadIndirect() || objNode->getOpCodeValue() == TR::aload) && !sym->isInternalPointer())
            {
            compareReg = objReg = cg->allocateCollectedReferenceRegister();
            }
         else
            {
            compareReg = objReg = cg->allocateRegister();
            objReg->setContainsInternalPointer();
            objReg->setPinningArrayPointer(sym->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
            }

         tempMR = generateS390MemoryReference(objNode, cg);

         generateRXInstruction(cg, TR::InstOpCode::getLoadTestOpCode(), objNode, objReg, tempMR);

         objNode->setRegister(objReg);
         nullCCSet = true;
         tempMR->stopUsingMemRefRegister(cg);
         }
      else
         {
         objReg = cg->allocateRegister();
         TR::Register * origReg = cg->evaluate(objNode);
         genNullTest(cg, node, objReg, origReg, NULL);
         compareReg = origReg;  // Get's rid of a couple AGIs

         objRegMustBeKilled = true;
         }
      }
   else
      {
      compareReg = objReg = cg->evaluate(objNode);
      }

   if (needsNullTest && isCheckcastAndNullChk && !cg->getHasResumableTrapHandler())
      {
      // find the bytecodeInfo
      // of the compacted NULLCHK
      TR::Node *nullChkInfo = comp->findNullChkInfo(node);
      generateNullChkSnippet(nullChkInfo, cg);
      }

   conditions->addPostCondition(objReg, TR::RealRegister::GPR2);
   conditions->addPostCondition(castClassReg, TR::RealRegister::GPR1);
   conditions->addPostCondition(scratch1Reg, cg->getReturnAddressRegister());
   conditions->addPostCondition(objClassReg, cg->getEntryPointRegister());

   // Add in compareRef if is happens to not already be inserted
   //
   conditions->addPostConditionIfNotAlreadyInserted(compareReg, TR::RealRegister::AssignAny);

   if (!testCache && !testEqualClass && !testCastClassIsSuper)
      {
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/Helper", comp->signature()),1,TR::DebugCounter::Undetermined);
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCast/(%s)/Helper", comp->signature()),1,TR::DebugCounter::Undetermined);
      gcPoint = generateDirectCall(cg, node, false, node->getSymbolReference(), conditions);
      gcPoint->setDependencyConditions(conditions);
      gcPoint->setNeedsGCMap(0x0000FFFF);

      cg->stopUsingRegister(castClassReg);
      cg->stopUsingRegister(scratch1Reg);
      cg->stopUsingRegister(objClassReg);
      if (objRegMustBeKilled)
         cg->stopUsingRegister(objReg);

      cg->decReferenceCount(objNode);
      cg->decReferenceCount(castClassNode);

      return NULL;
      }

   doneLabel = generateLabelSymbol(cg);
   callLabel = generateLabelSymbol(cg);
   startOOLLabel = generateLabelSymbol(cg);
   doneOOLLabel = generateLabelSymbol(cg);
   helperReturnOOLLabel = generateLabelSymbol(cg);
   continueLabel = generateLabelSymbol(cg);
   resultLabel = doneLabel;

   if (needsNullTest && !isCheckcastAndNullChk)
      {
      if (!nullCCSet)
         {
         genNullTest(cg, node, objReg, objReg, NULL);
         }
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, doneLabel);
      }

   TR_S390OutOfLineCodeSection *outlinedSlowPath = NULL;

   if (testEqualClass)
      {
      TR::TreeEvaluator::genLoadForObjectHeadersMasked(cg, node, objClassReg, generateS390MemoryReference(compareReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg), NULL);


      if (testCastClassIsSuper)
         {
         // we should enable OOL only if the above compare has a high chance of passing
         // the profiler tells us the probability of a successful check cast
         TR_OpaqueClassBlock * castClassAddr = TR::TreeEvaluator::getCastClassAddress(castClassNode);
         TR_OpaqueClassBlock * topGuessClassAddr = TR::TreeEvaluator::interpreterProfilingInstanceOfOrCheckCastInfo(cg, node);
         float topProb = TR::TreeEvaluator::interpreterProfilingInstanceOfOrCheckCastTopProb(cg, node);
         // experimental : set the probability threshold = 50% to enable OOL
         if (!comp->getOption(TR_DisableOOL) && castClassAddr == topGuessClassAddr && topProb >= 0.5)
            {
            // OOL: Fall through if test passes, else call OOL sequence
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/EqualOOL", comp->signature()),1,TR::DebugCounter::Undetermined);
            TR::Instruction * temp = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, castClassReg, objClassReg, TR::InstOpCode::COND_BNE, startOOLLabel, false, false);
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/EqualOOLPass", comp->signature()),1,TR::DebugCounter::Undetermined);
            if (debugObj)
               debugObj->addInstructionComment(temp, "Branch to OOL checkCast sequence");

            if (comp->getOption(TR_TraceCG))
               traceMsg (comp, "OOL enabled: successful checkCast probability = (%.2f)%%\n", topProb * 100);

            //Using OOL but generating code manually
            outlinedSlowPath =
               new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(startOOLLabel,doneOOLLabel,cg);
            cg->getS390OutOfLineCodeSectionList().push_front(outlinedSlowPath);
            outlinedSlowPath->swapInstructionListsWithCompilation();
            temp = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, startOOLLabel);
            resultLabel = helperReturnOOLLabel;
            if (debugObj)
               debugObj->addInstructionComment(temp, "Denotes start of OOL checkCast sequence");
            }
         else
            {
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/Equal", comp->signature()),1,TR::DebugCounter::Undetermined);
            gcPoint = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, castClassReg, objClassReg, TR::InstOpCode::COND_BE, doneLabel, false, false);
            cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/EqualFail", comp->signature()),1,TR::DebugCounter::Undetermined);
            }
         }
      else
         {
         cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/Equal", comp->signature()),1,TR::DebugCounter::Undetermined);
         gcPoint = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, castClassReg, objClassReg, TR::InstOpCode::COND_BNE, callLabel, false, false);
         cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/EqualPass", comp->signature()),1,TR::DebugCounter::Undetermined);
         }
      }

   // the VM Helper should return to OOL sequence if it's enabled.
   TR::Snippet * snippet = new (cg->trHeapMemory()) TR::S390HelperCallSnippet(cg, node, callLabel, node->getSymbolReference(), resultLabel);
   cg->addSnippet(snippet);

   if ((testCache || testCastClassIsSuper) && !testEqualClass)
      {
      TR::TreeEvaluator::genLoadForObjectHeadersMasked(cg, node, objClassReg, generateS390MemoryReference(compareReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg), NULL);
      }

   if (testCache)
      {
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/Profiled", comp->signature()),1,TR::DebugCounter::Undetermined);
      generateInlineTest(cg, node, castClassNode, objClassReg, NULL, scratch1Reg, litPoolBaseReg, false, resultLabel, resultLabel, resultLabel, true);
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/ProfiledFail", comp->signature()),1,TR::DebugCounter::Undetermined);
      // The cached value could have been from a previously successful checkcast or instanceof.
      // An answer of 0 in the low order bit indicates 'success' (the cast or instanceof was successful).
      // An answer of 1 in the lower order bit indicates 'failure' (the cast would have thrown an exception, instanceof would have been unsuccessful)
      // Because of this, we can just do a simple load and compare of the 2 class pointers. If it succeeds, the low order bit
      // must be off (success) from a previous checkcast or instanceof. If the low order bit is on, it is guaranteed not to
      // compare and we will take the slow path.


#ifdef OMR_GC_COMPRESSED_POINTERS
      // for the following two instructions we may need to convert the
      // class offset from scratch1Reg into a J9Class pointer and
      // offset from castClassReg into a J9Pointer. Then we can compare
      // J9Class pointers
#endif
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/Cache", comp->signature()),1,TR::DebugCounter::Undetermined);
      TR::MemoryReference * cacheMR = generateS390MemoryReference(objClassReg, offsetof(J9Class, castClassCache), cg);
      generateRXInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, castClassReg, cacheMR);

      if (testCastClassIsSuper)
         {
         gcPoint = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, resultLabel);
         cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/CacheFail", comp->signature()),1,TR::DebugCounter::Undetermined);
         }
      else
         {
         gcPoint = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, callLabel);
         cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/CacheSuccess", comp->signature()),1,TR::DebugCounter::Undetermined);
         }
      }

   if (testCastClassIsSuper)
      {
      //see if we can use the cached value from interpreterProfilingInstanceOfOrCheckCastInfo
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/Profiled", comp->signature()),1,TR::DebugCounter::Undetermined);
      generateInlineTest(cg, node, castClassNode, objClassReg, NULL, scratch1Reg, litPoolBaseReg, false, continueLabel, resultLabel, resultLabel, true, 1);
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/ProfiledFail", comp->signature()),1,TR::DebugCounter::Undetermined);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, continueLabel);
      int32_t castClassDepth = castClassSymRef->classDepth(comp);

      scratch2Reg = cg->allocateRegister();
      // Should let the assigner decide (no interface to do it yet)
      conditions->addPostCondition(scratch2Reg, TR::RealRegister::GPR3);

      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "checkCastStats/(%s)/SuperClass", comp->signature()),1,TR::DebugCounter::Undetermined);
      genTestIsSuper(cg, node, objClassReg, castClassReg, scratch1Reg, scratch2Reg, NULL, litPoolBaseReg, castClassDepth, callLabel, NULL, NULL, conditions, NULL, false, NULL, NULL);
      gcPoint = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, callLabel);

      cg->stopUsingRegister(scratch2Reg);
      }

   if (outlinedSlowPath)
      {
      // Return label from VM Helper call back to OOL sequence
      // We can not branch directly back from VM Helper to main line because
      // there might be reg spills in the rest of the OOL sequence, these code need to be executed.
      TR::Instruction * temp = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, helperReturnOOLLabel);
      if (debugObj)
         {
         debugObj->addInstructionComment(temp, "OOL checkCast VMHelper return label");
         //printf ("OOL checkCast %s\n",cg->comp()->signature());
         //fflush (stdout);
         }
      temp = generateS390BranchInstruction(cg,TR::InstOpCode::BRC,TR::InstOpCode::COND_BRC,node,doneOOLLabel);
      if (debugObj)
         debugObj->addInstructionComment(temp, "Denotes end of OOL checkCast sequence: return to mainline");

      // Done using OOL with manual code generation
      outlinedSlowPath->swapInstructionListsWithCompilation();
      temp = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneOOLLabel);
      if (debugObj)
         debugObj->addInstructionComment(temp, "OOL checkCast return label");
      }

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneLabel, conditions);


   // We need the last real instruction to be the GC point
   gcPoint->setNeedsGCMap(0x0000FFFF);

   cg->stopUsingRegister(castClassReg);
   cg->stopUsingRegister(scratch1Reg);
   cg->stopUsingRegister(objClassReg);
   if (objRegMustBeKilled)
      cg->stopUsingRegister(objReg);

   cg->decReferenceCount(objNode);
   cg->decReferenceCount(castClassNode);

   return NULL;
   }

TR::Register *
J9::Z::TreeEvaluator::VMmonentEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   int32_t lwOffset = fej9->getByteOffsetToLockword((TR_OpaqueClassBlock *) cg->getMonClass(node));
   TR::S390CHelperLinkage *helperLink =  static_cast<TR::S390CHelperLinkage*>(cg->getLinkage(TR_CHelper));

   if (comp->getOption(TR_OptimizeForSpace) ||
       (comp->getOption(TR_FullSpeedDebug) && node->isSyncMethodMonitor()) ||
       comp->getOption(TR_DisableInlineMonEnt) ||
       comp->getOption(TR_FullSpeedDebug))  // Required for Live Monitor Meta Data in FSD.
      {
      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node::recreate(node, TR::call);
      TR::Register *targetRegister = helperLink->buildDirectDispatch(node);
      cg->decReferenceCount(node->getFirstChild());
      TR::Node::recreate(node, opCode);
      return targetRegister;
      }


   TR_S390ScratchRegisterManager *srm = cg->generateScratchRegisterManager();

   TR::Node                *objNode                   = node->getFirstChild();
   TR::Register            *objReg                    = cg->evaluate(objNode);
   TR::Register            *baseReg                   = objReg;
   TR::Register            *monitorReg                = cg->allocateRegister();
   TR::Register            *objectClassReg            = NULL;
   TR::Register            *lookupOffsetReg           = NULL;
   TR::Register            *tempRegister              = NULL;
   TR::Register            *metaReg                   = cg->getMethodMetaDataRealRegister();
   TR::Register            *wasteReg                  = NULL;
   TR::Register            *lockPreservingReg         = NULL;
   TR::Register            *dummyResultReg               = NULL;


   TR::LabelSymbol         *cFlowRegionEnd            = generateLabelSymbol(cg);
   TR::LabelSymbol         *callLabel                 = generateLabelSymbol(cg);
   TR::LabelSymbol         *monitorLookupCacheLabel   = generateLabelSymbol(cg);
   TR::Instruction         *gcPoint                   = NULL;
   TR::Instruction         *startICF                  = NULL;
   static char * disableInlineRecursiveMonitor = feGetEnv("TR_DisableInlineRecursiveMonitor");

   bool inlineRecursive = true;
   if (disableInlineRecursiveMonitor)
     inlineRecursive = false;

   int32_t numDeps = 4;

#if defined (J9VM_THR_LOCK_NURSERY)
   if (lwOffset <=0)
      {
      numDeps +=2;
      if (comp->getOption(TR_EnableMonitorCacheLookup))
         {
         numDeps +=2; // extra one for lit pool reg in disableZ9 mode
         }
      }
#endif

   if (comp->getOptions()->enableDebugCounters())
      numDeps += 5;
   bool simpleLocking = false;
   bool reserveLocking = false, normalLockWithReservationPreserving = false;


   bool disableOOL = comp->getOption(TR_DisableOOL);
   if (disableOOL)
      inlineRecursive = false;

   TR::RegisterDependencyConditions * conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, numDeps, cg);

   bool isAOT = comp->getOption(TR_AOT);
   TR_Debug * debugObj = cg->getDebug();

   conditions->addPostCondition(objReg, TR::RealRegister::AssignAny);
   conditions->addPostCondition(monitorReg, TR::RealRegister::AssignAny);

   static const char * peekFirst = feGetEnv("TR_PeekingMonEnter");
   // This debug option is for printing the locking mechanism.
   static int printMethodSignature = feGetEnv("PrintMethodSignatureForLockResEnt")? 1 : 0;
#if defined (J9VM_THR_LOCK_NURSERY)
   if (lwOffset <= 0)
      {
      inlineRecursive = false;
      // should not happen often, only on a subset of objects that don't have a lockword
      // set with option -Xlockword

      TR::LabelSymbol               *helperCallLabel = generateLabelSymbol(cg);
      TR::LabelSymbol               *helperReturnOOLLabel = generateLabelSymbol(cg);
      TR::MemoryReference * tempMR = generateS390MemoryReference(objReg, TR::Compiler->om.offsetOfObjectVftField(), cg);
      // TODO We don't need objectClassReg except in this ifCase. We can use scratchRegisterManager to allocate one here.
      objectClassReg = cg->allocateRegister();
      conditions->addPostCondition(objectClassReg, TR::RealRegister::AssignAny);
      TR::TreeEvaluator::genLoadForObjectHeadersMasked(cg, node, objectClassReg, tempMR, NULL);
      int32_t offsetOfLockOffset = offsetof(J9Class, lockOffset);
      tempMR = generateS390MemoryReference(objectClassReg, offsetOfLockOffset, cg);

      tempRegister = cg->allocateRegister();
      TR::LabelSymbol *targetLabel = callLabel;
      if (comp->getOption(TR_EnableMonitorCacheLookup))
         targetLabel = monitorLookupCacheLabel;

      generateRXInstruction(cg, TR::InstOpCode::getLoadTestOpCode(), node, tempRegister, tempMR);

      if (disableOOL)
         {
         TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
         cFlowRegionStart->setStartInternalControlFlow();
         }
      TR::Instruction *cmpInstr = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, targetLabel);

      if(TR::Compiler->target.is64Bit())
         generateRXInstruction(cg, TR::InstOpCode::LA, node, tempRegister, generateS390MemoryReference(objReg, tempRegister, 0, cg));
      else
         generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, tempRegister, objReg);

      if (comp->getOption(TR_EnableMonitorCacheLookup))
         {
         TR::RegisterDependencyConditions * OOLConditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 5, cg);
         OOLConditions->addPostCondition(objReg, TR::RealRegister::AssignAny);
         OOLConditions->addPostCondition(monitorReg, TR::RealRegister::AssignAny);
         OOLConditions->addPostCondition(tempRegister, TR::RealRegister::AssignAny);
         // pulling this chunk of code into OOL sequence for better Register allocation and avoid branches
         TR_S390OutOfLineCodeSection *monitorCacheLookupOOL;
         monitorCacheLookupOOL = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(monitorLookupCacheLabel,cFlowRegionEnd,cg);
         cg->getS390OutOfLineCodeSectionList().push_front(monitorCacheLookupOOL);
         monitorCacheLookupOOL->swapInstructionListsWithCompilation();

         TR::Instruction *cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, monitorLookupCacheLabel);

         if (!disableOOL)
            {
            if (debugObj)
               {
               debugObj->addInstructionComment(cmpInstr, "Branch to OOL monent monitorLookupCache");
               debugObj->addInstructionComment(cursor, "Denotes start of OOL monent monitorLookupCache");
               }
            }

         lookupOffsetReg = cg->allocateRegister();
         OOLConditions->addPostCondition(lookupOffsetReg, TR::RealRegister::AssignAny);

         int32_t offsetOfMonitorLookupCache = offsetof(J9VMThread, objectMonitorLookupCache);
         int32_t t = trailingZeroes(fej9->getObjectAlignmentInBytes());
         int32_t shiftAmount = trailingZeroes((int32_t) TR::Compiler->om.sizeofReferenceField()) - t;
         int32_t end = 63 - trailingZeroes((int32_t) TR::Compiler->om.sizeofReferenceField());
         int32_t start = end - trailingZeroes(J9VMTHREAD_OBJECT_MONITOR_CACHE_SIZE) + 1;

         if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::zEC12) && TR::Compiler->target.is64Bit())
            generateRIEInstruction(cg, TR::InstOpCode::RISBGN, node, lookupOffsetReg, objReg, start, end+0x80, shiftAmount);
         else if(TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z10) && TR::Compiler->target.is64Bit())
            generateRIEInstruction(cg, TR::InstOpCode::RISBG, node, lookupOffsetReg, objReg, start, end+0x80, shiftAmount);
         else
            {
            generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, lookupOffsetReg, objReg);

            if (TR::Compiler->target.is64Bit())
               generateRSInstruction(cg, TR::InstOpCode::SRAG, node, lookupOffsetReg, lookupOffsetReg, t);
            else
               generateRSInstruction(cg, TR::InstOpCode::SRA, node, lookupOffsetReg, t);

            J9JavaVM * jvm = fej9->getJ9JITConfig()->javaVM;

            if (TR::Compiler->target.is32Bit())
               generateS390ImmOp(cg, TR::InstOpCode::getAndOpCode(), node, lookupOffsetReg, lookupOffsetReg, (int32_t) J9VMTHREAD_OBJECT_MONITOR_CACHE_SIZE - 1, OOLConditions, 0);
            else
               generateS390ImmOp(cg, TR::InstOpCode::getAndOpCode(), node, lookupOffsetReg, lookupOffsetReg, (int64_t) J9VMTHREAD_OBJECT_MONITOR_CACHE_SIZE - 1, OOLConditions, 0);

            if (TR::Compiler->target.is64Bit())
               generateRSInstruction(cg, TR::InstOpCode::SLLG, node, lookupOffsetReg, lookupOffsetReg, trailingZeroes((int32_t) TR::Compiler->om.sizeofReferenceField()));
            else
               generateRSInstruction(cg, TR::InstOpCode::SLL, node, lookupOffsetReg, trailingZeroes((int32_t) TR::Compiler->om.sizeofReferenceField()));
            }

         TR::MemoryReference * temp2MR = generateS390MemoryReference(cg->getMethodMetaDataRealRegister(), lookupOffsetReg, offsetOfMonitorLookupCache, cg);

#if defined(OMR_GC_COMPRESSED_POINTERS)
         generateRXInstruction(cg, TR::InstOpCode::LLGF, node, tempRegister, temp2MR, NULL);
         startICF = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, tempRegister, NULLVALUE, TR::InstOpCode::COND_BE, helperCallLabel, false, true);
#else
         generateRXInstruction(cg, TR::InstOpCode::getLoadTestOpCode(), node, tempRegister, temp2MR);

         startICF = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, helperCallLabel);
#endif

         int32_t offsetOfMonitor = offsetof(J9ObjectMonitor, monitor);
         temp2MR = generateS390MemoryReference(tempRegister, offsetOfMonitor, cg);
         generateRXInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, objReg, temp2MR);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, helperCallLabel);

         int32_t offsetOfAlternateLockWord = offsetof(J9ObjectMonitor, alternateLockword);

         baseReg = tempRegister;
         lwOffset = 0 + offsetOfAlternateLockWord;

         if (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord())
            generateRRInstruction(cg, TR::InstOpCode::XR, node, monitorReg, monitorReg);
         else
            generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, monitorReg, monitorReg);

         if (peekFirst)
            {
            generateRXInstruction(cg, TR::InstOpCode::C, node, monitorReg, generateS390MemoryReference(baseReg, lwOffset, cg));
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, helperCallLabel);
            }

         if (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord())
            generateRSInstruction(cg, TR::InstOpCode::CS, node, monitorReg, metaReg,
                                  generateS390MemoryReference(baseReg, lwOffset, cg));
         else
            generateRSInstruction(cg, TR::InstOpCode::getCmpAndSwapOpCode(), node, monitorReg, metaReg,
                                  generateS390MemoryReference(baseReg, lwOffset, cg));

         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, helperReturnOOLLabel);
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, helperCallLabel );
         TR::RegisterDependencyConditions *deps = NULL;
         dummyResultReg = helperLink->buildDirectDispatch(node, &deps);
         TR::RegisterDependencyConditions *mergeConditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(OOLConditions, deps, cg);
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, helperReturnOOLLabel , mergeConditions);
         if (!disableOOL)
            {
            cursor = generateS390BranchInstruction(cg,TR::InstOpCode::BRC,TR::InstOpCode::COND_BRC,node,cFlowRegionEnd);
            if (debugObj)
               debugObj->addInstructionComment(cursor, "Denotes end of OOL monent monitorCacheLookup: return to mainline");

            // Done using OOL with manual code generation
            monitorCacheLookupOOL->swapInstructionListsWithCompilation();
            }
      }

      simpleLocking = true;
      lwOffset = 0;
      baseReg = tempRegister;
   }
#endif

   // Lock Reservation happens only for objects with lockword.
   // evaluateLockForReservation may output three different results:
   // 1- Lock Reservation: (reserveLocking = true)
   // 2- ReservationPreserving: (normalLockWithReservationPreserving = true)
   // 3- Normal lock: otherwise
   if (!simpleLocking && comp->getOption(TR_ReservingLocks))
      TR::TreeEvaluator::evaluateLockForReservation(node, &reserveLocking, &normalLockWithReservationPreserving, cg);

   if (printMethodSignature)
      printf("%s:\t%s\t%s\n",simpleLocking ? "lwOffset <= 0" : reserveLocking ? "Lock Reservation" :
             normalLockWithReservationPreserving ? "Reservation Preserving" : "Normal Lock",
             comp->signature(),comp->getHotnessName(comp->getMethodHotness()));

   if (reserveLocking)
      {

      // TODO - ScratchRegisterManager Should Manage these temporary Registers.
      if (wasteReg)
         cg->stopUsingRegister(wasteReg);
      cg->stopUsingRegister(monitorReg);
      // TODO : objectClassReg contains the J9Class for object which is set in lwOffset <= 0 case. Usually that is NULL in the following function call
      return reservationLockEnter(node, lwOffset, objectClassReg, cg, helperLink);
      }

   if (normalLockWithReservationPreserving)
      {
      lockPreservingReg = cg->allocateRegister();
      conditions->addPostCondition(lockPreservingReg, TR::RealRegister::AssignAny);
      }
   const char* debugCounterNamePrefix = normalLockWithReservationPreserving? "LockEnt/Preserving": "LockEnt/Normal";
   // Opcodes:
   bool use64b = true;
   if (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord())
      use64b = false;
   else if (!TR::Compiler->target.is64Bit())
      use64b = false;
   TR::InstOpCode::Mnemonic loadOp = use64b ? TR::InstOpCode::LG : TR::InstOpCode::L;
   TR::InstOpCode::Mnemonic loadRegOp = use64b ? TR::InstOpCode::LGR : TR::InstOpCode::LR;
   TR::InstOpCode::Mnemonic orImmOp = TR::InstOpCode::OILF;
   TR::InstOpCode::Mnemonic compareOp = use64b ? TR::InstOpCode::CGR : TR::InstOpCode::CR;
   TR::InstOpCode::Mnemonic addImmOp = use64b ? TR::InstOpCode::AGHI : TR::InstOpCode::AHI;
   TR::InstOpCode::Mnemonic storeOp = use64b ? TR::InstOpCode::STG : TR::InstOpCode::ST;
   TR::InstOpCode::Mnemonic xorOp = use64b ? TR::InstOpCode::XGR : TR::InstOpCode::XR;
   TR::InstOpCode::Mnemonic casOp = use64b ? TR::InstOpCode::CSG : TR::InstOpCode::CS;
   TR::InstOpCode::Mnemonic andOp = use64b ? TR::InstOpCode::NGR : TR::InstOpCode::NR;
   TR::InstOpCode::Mnemonic loadHalfWordImmOp = use64b ? TR::InstOpCode::LGHI : TR::InstOpCode::LHI;

   // MonitorReg = 0
   generateRRInstruction(cg, xorOp, node, monitorReg, monitorReg);

   // PeekFirst option read the lock value first and then issue CAS only the lock value is zero.
   // This causes an extra load operation when the lock is free, but it leads to avoidance of unnecessary CAS operations.
   if (peekFirst)
      {
      generateRXInstruction(cg, TR::InstOpCode::C, node, monitorReg, generateS390MemoryReference(baseReg, lwOffset, cg));
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, callLabel);
      }
   // Main path instruction sequence.
   // This sequence is the same for both normal locks and lock preservation.
   //    XR      monitorReg,monitorReg
   //    CS      monitorReg,GPR13,#lwOffset(objectReg)
   //    BRC     BLRC(0x4), callLabel (OOL path)

   //Compare and Swap the lock value with R13 if the lock value is 0.
   generateRSInstruction(cg, casOp, node, monitorReg, metaReg, generateS390MemoryReference(baseReg, lwOffset, cg));

   // Jump to OOL branch in case that the CAS is unsuccessful (Lockword had contained a non-zero value before CAS)
   // Both TR::InstOpCode::MASK6 and TR::InstOpCode::MASK4 are ok here. TR::InstOpCode::MASK4 is directly testing failure condition.
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, node, callLabel);
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "%s/CSSuccessfull", debugCounterNamePrefix), 1, TR::DebugCounter::Undetermined);
   TR_S390OutOfLineCodeSection *outlinedHelperCall = NULL;
   TR::Instruction *cursor;
   TR::LabelSymbol *returnLabel = generateLabelSymbol(cg);
   if (!disableOOL)
      {
      outlinedHelperCall = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(callLabel, cFlowRegionEnd, cg);
      cg->getS390OutOfLineCodeSectionList().push_front(outlinedHelperCall);
      outlinedHelperCall->swapInstructionListsWithCompilation();
      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, callLabel);
      if (debugObj)
         debugObj->addInstructionComment(cursor, "Denotes start of OOL monent sequence");
      }

   if (inlineRecursive)
      {
      TR::LabelSymbol * callHelper = generateLabelSymbol(cg);

      //Using OOL but generating code manually
      //Tasuki lock, inlined nested monitor handling
      //(on entry objectReg has been set up)

      //   Normal Lock                                        Lock reservation preserving
      // L     monitorReg, #lwOffset(objectReg)             L     monitorReg, #lwOffset(objectReg)
      // LHI   wasteReg, NON_INC_DEC_MASK           DIFF    LHI   wasteReg, LOCK_RES_PRESERVE_ENTER
      // AHI   monitorReg, INC_DEC_VALUE            DIFF
      // NR    wasteReg, monitorReg                         NR    wasteReg, monitorReg
      //                                            DIFF    LR    lockPreservingReg, metaReg
      //                                            DIFF    OILF  lockPreservingReg, LR-BIT
      // CRJ   wasteReg, metaReg, MASK6, callHelper DIFF    CRJ   wasteReg, lockPreservingReg, MASK6, callHelper
      //                                            DIFF    AHI   monitorReg,INC_DEC_VALUE
      // ST    monitorReg, #lwOffset(objectReg)             ST    monitorReg, #lwOffset(objectReg)
      // BRC   returnLabel                                  BRC   returnLabel
      // callHelper:                                        callHelper:
      // BRASL R14, jitMonitorEnter                         BRASL   R14, jitMonitorEnter
      // returnLabel:                                       returnLabel:

      TR::MemoryReference * tempMR = generateS390MemoryReference(baseReg, lwOffset, cg);
      TR::MemoryReference * tempMR1 = generateS390MemoryReference(baseReg, lwOffset, cg);
      wasteReg = cg->allocateRegister();
      conditions->addPostCondition(wasteReg, TR::RealRegister::AssignAny);
      // Loading Lock value into monitorReg
      generateRXInstruction(cg, loadOp, node, monitorReg, tempMR);
      generateRIInstruction(cg, loadHalfWordImmOp, node, wasteReg,
            normalLockWithReservationPreserving ? ~LOCK_RES_PRESERVE_ENTER_COMPLEMENT : ~OBJECT_HEADER_LOCK_RECURSION_MASK);

      // In normal lock, we first increment the counter and then do the mask and comparison.
      // However, in lock preserving first we do mask and compare and then we increment the counter
      // We can do the same technique for both. The reason for current implementation is to expose less differences between
      // this implementation and other architecture implementations.
      if (!normalLockWithReservationPreserving)
         generateRIInstruction(cg, addImmOp, node, monitorReg, OBJECT_HEADER_LOCK_FIRST_RECURSION_BIT);
      // Mask out the counter value from lockword.
      generateRRInstruction(cg, andOp, node, wasteReg, monitorReg);
      if (normalLockWithReservationPreserving)
         {
         generateRRInstruction(cg,loadRegOp, node, lockPreservingReg, metaReg);
         generateRILInstruction(cg, orImmOp, node, lockPreservingReg, LOCK_RESERVATION_BIT);
         }

      // The lock value (after masking out the counter) is being compared with R13 (or R13|LRbit for reservation preserving case)
      // to check whether the same thread has acquired the lock before.
      // if comparison fails (masked lock value != R13) that means another thread owns the lock.
      // In this case we call helper function and let the VM handle the situation.
      startICF = generateS390CompareAndBranchInstruction(cg, compareOp, node, wasteReg, normalLockWithReservationPreserving ? lockPreservingReg : metaReg, TR::InstOpCode::COND_BNE, callHelper, false, false);

      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "%s/Recursive", debugCounterNamePrefix), 1, TR::DebugCounter::Undetermined);
      // In case of recursive lock, the counter should be incremented.
      if (normalLockWithReservationPreserving)
         generateRIInstruction(cg, addImmOp, node, monitorReg, OBJECT_HEADER_LOCK_FIRST_RECURSION_BIT);
      generateRXInstruction(cg, storeOp, node, monitorReg, tempMR1);

      generateS390BranchInstruction(cg,TR::InstOpCode::BRC,TR::InstOpCode::COND_BRC,node,returnLabel);

      tempMR->stopUsingMemRefRegister(cg);
      tempMR1->stopUsingMemRefRegister(cg);

      // Helper Call
      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, callHelper);
      }

      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "%s/VMHelper", debugCounterNamePrefix), 1, TR::DebugCounter::Undetermined);
      TR::RegisterDependencyConditions *deps = NULL;
      dummyResultReg = inlineRecursive ? helperLink->buildDirectDispatch(node, &deps) : helperLink->buildDirectDispatch(node);
      TR::RegisterDependencyConditions *mergeConditions = NULL;
      if (inlineRecursive)
         mergeConditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(conditions, deps, cg);
      else
         mergeConditions = conditions;
      generateS390LabelInstruction(cg,TR::InstOpCode::LABEL,node,returnLabel,mergeConditions);

      if (!disableOOL)
         {
         // End of OOl path.
         cursor = generateS390BranchInstruction(cg,TR::InstOpCode::BRC,TR::InstOpCode::COND_BRC,node,cFlowRegionEnd);
         if (debugObj)
            {
            debugObj->addInstructionComment(cursor, "Denotes end of OOL monent: return to mainline");
            }

         // Done using OOL with manual code generation
         outlinedHelperCall->swapInstructionListsWithCompilation();
         }

   bool needDeps = false;
#if defined (J9VM_THR_LOCK_NURSERY)
   if (lwOffset <= 0 && disableOOL)
      needDeps = true;
#endif

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, conditions);

#if defined (J9VM_THR_LOCK_NURSERY)
   if (lwOffset <= 0 && disableOOL)
      cFlowRegionEnd->setEndInternalControlFlow();
#endif
   cg->stopUsingRegister(monitorReg);
   if (wasteReg)
      cg->stopUsingRegister(wasteReg);
   if (objectClassReg)
      cg->stopUsingRegister(objectClassReg);
   if (lookupOffsetReg)
      cg->stopUsingRegister(lookupOffsetReg);
   if (tempRegister && (tempRegister != objectClassReg))
      cg->stopUsingRegister(tempRegister);
   if (lockPreservingReg)
      cg->stopUsingRegister(lockPreservingReg);
   cg->decReferenceCount(objNode);
   return NULL;
   }

TR::Register *
J9::Z::TreeEvaluator::VMmonexitEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   int32_t lwOffset = fej9->getByteOffsetToLockword((TR_OpaqueClassBlock *) cg->getMonClass(node));
   TR::S390CHelperLinkage *helperLink =  static_cast<TR::S390CHelperLinkage*>(cg->getLinkage(TR_CHelper));
   if (comp->getOption(TR_OptimizeForSpace) ||
       comp->getOption(TR_DisableInlineMonExit) ||
       comp->getOption(TR_FullSpeedDebug))  // Required for Live Monitor Meta Data in FSD.
      {
      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node::recreate(node, TR::call);
      TR::Register * targetRegister = helperLink->buildDirectDispatch(node);
      cg->decReferenceCount(node->getFirstChild());
      TR::Node::recreate(node, opCode);
      return targetRegister;
      }

   TR::Node          *objNode             = node->getFirstChild();


   //TODO Use scratchRegisterManager here to avoid allocating un-necessary registers
   TR::Register      *dummyResultRegister = NULL;
   TR::Register      *objReg              = cg->evaluate(objNode);
   TR::Register      *baseReg             = objReg;
   TR::Register      *objectClassReg      = NULL;
   TR::Register      *lookupOffsetReg     = NULL;
   TR::Register      *tempRegister        = NULL;
   TR::Register      *monitorReg          = cg->allocateRegister();
   TR::Register      *metaReg             = cg->getMethodMetaDataRealRegister();
   TR::Register      *scratchRegister     = NULL;
   TR::Instruction         *startICF                  = NULL;
   bool disableOOL  = comp->getOption(TR_DisableOOL);
   static char * disableInlineRecursiveMonitor = feGetEnv("TR_DisableInlineRecursiveMonitor");
   bool inlineRecursive = true;
   if (disableInlineRecursiveMonitor || disableOOL)
     inlineRecursive = false;

   TR::LabelSymbol *callLabel                      = generateLabelSymbol(cg);
   TR::LabelSymbol *monitorLookupCacheLabel        = generateLabelSymbol(cg);
   TR::LabelSymbol *cFlowRegionEnd                 = generateLabelSymbol(cg);
   TR::LabelSymbol *callHelper                     = generateLabelSymbol(cg);
   TR::LabelSymbol *returnLabel                    = generateLabelSymbol(cg);

   int32_t numDeps = 4;
#if defined (J9VM_THR_LOCK_NURSERY)
   if (lwOffset <=0)
      {
      numDeps +=2;
      if (comp->getOption(TR_EnableMonitorCacheLookup))
         {
         numDeps +=2; // extra one for lit pool reg in disableZ9 mode
         }
      }
#endif

   if (comp->getOptions()->enableDebugCounters())
         numDeps += 4;

   TR::RegisterDependencyConditions * conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, numDeps, cg);


   TR::Instruction * gcPoint;
   bool isAOT = comp->getOption(TR_AOT);
   TR_Debug * debugObj = cg->getDebug();

   bool reserveLocking                          = false;
   bool normalLockWithReservationPreserving     = false;
   bool simpleLocking                           = false;


   conditions->addPostCondition(objReg, TR::RealRegister::AssignAny);
   conditions->addPostCondition(monitorReg, TR::RealRegister::AssignAny);


#if defined (J9VM_THR_LOCK_NURSERY)
   if (lwOffset <= 0)
      {
      inlineRecursive = false; // should not happen often, only on a subset of objects that don't have a lockword, set with option -Xlockword

      TR::LabelSymbol *helperCallLabel          = generateLabelSymbol(cg);
      TR::LabelSymbol *helperReturnOOLLabel     = generateLabelSymbol(cg);

      objectClassReg = cg->allocateRegister();
      TR::TreeEvaluator::genLoadForObjectHeadersMasked(cg, node, objectClassReg, generateS390MemoryReference(objReg, TR::Compiler->om.offsetOfObjectVftField(), cg), NULL);

      int32_t offsetOfLockOffset = offsetof(J9Class, lockOffset);
      TR::MemoryReference* tempMR = generateS390MemoryReference(objectClassReg, offsetOfLockOffset, cg);
      tempRegister = cg->allocateRegister();

      TR::LabelSymbol *targetLabel = callLabel;
      if (comp->getOption(TR_EnableMonitorCacheLookup))
         targetLabel = monitorLookupCacheLabel;

      generateRXInstruction(cg, TR::InstOpCode::getLoadTestOpCode(), node, tempRegister, tempMR);

      if (disableOOL)
         {
         TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
         cFlowRegionStart->setStartInternalControlFlow();
         }
      TR::Instruction *cmpInstr = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, targetLabel);

      if(TR::Compiler->target.is64Bit())
         generateRXInstruction(cg, TR::InstOpCode::LA, node, tempRegister, generateS390MemoryReference(objReg, tempRegister, 0, cg));
      else
         generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, tempRegister, objReg);

      if (comp->getOption(TR_EnableMonitorCacheLookup))
         {
         lookupOffsetReg = cg->allocateRegister();
         TR::RegisterDependencyConditions * OOLConditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 5, cg);
         OOLConditions->addPostCondition(objReg, TR::RealRegister::AssignAny);
         OOLConditions->addPostCondition(monitorReg, TR::RealRegister::AssignAny);
         // TODO Should be using SRM for tempRegister
         OOLConditions->addPostCondition(tempRegister, TR::RealRegister::AssignAny);
         OOLConditions->addPostCondition(lookupOffsetReg, TR::RealRegister::AssignAny);


         // pulling this chunk of code into OOL sequence for better Register allocation and avoid branches
         TR_S390OutOfLineCodeSection *monitorCacheLookupOOL = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(monitorLookupCacheLabel,cFlowRegionEnd,cg);
         cg->getS390OutOfLineCodeSectionList().push_front(monitorCacheLookupOOL);
         monitorCacheLookupOOL->swapInstructionListsWithCompilation();

         TR::Instruction *cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, monitorLookupCacheLabel);

         if (!disableOOL)
            {
            if (debugObj)
               {
               debugObj->addInstructionComment(cmpInstr, "Branch to OOL monexit monitorLookupCache");
               debugObj->addInstructionComment(cursor, "Denotes start of OOL monexit monitorLookupCache");
               }
            }


         int32_t offsetOfMonitorLookupCache = offsetof(J9VMThread, objectMonitorLookupCache);
         int32_t t = trailingZeroes(fej9->getObjectAlignmentInBytes());
         int32_t shiftAmount = trailingZeroes((int32_t) TR::Compiler->om.sizeofReferenceField()) - t;
         int32_t end = 63 - trailingZeroes((int32_t) TR::Compiler->om.sizeofReferenceField());
         int32_t start = end - trailingZeroes(J9VMTHREAD_OBJECT_MONITOR_CACHE_SIZE) + 1;

         if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::zEC12) && TR::Compiler->target.is64Bit())
            generateRIEInstruction(cg, TR::InstOpCode::RISBGN, node, lookupOffsetReg, objReg, start, end+0x80, shiftAmount);
         else if(TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z10) && TR::Compiler->target.is64Bit())
            generateRIEInstruction(cg, TR::InstOpCode::RISBG, node, lookupOffsetReg, objReg, start, end+0x80, shiftAmount);
         else
            {
            generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, lookupOffsetReg, objReg);

            if (TR::Compiler->target.is64Bit())
               generateRSInstruction(cg, TR::InstOpCode::SRAG, node, lookupOffsetReg, lookupOffsetReg, t);
            else
               generateRSInstruction(cg, TR::InstOpCode::SRA, node, lookupOffsetReg, t);

            J9JavaVM * jvm = fej9->getJ9JITConfig()->javaVM;

            if (TR::Compiler->target.is32Bit())
               generateS390ImmOp(cg, TR::InstOpCode::getAndOpCode(), node, lookupOffsetReg, lookupOffsetReg, (int32_t) J9VMTHREAD_OBJECT_MONITOR_CACHE_SIZE - 1, OOLConditions, 0);
            else
               generateS390ImmOp(cg, TR::InstOpCode::getAndOpCode(), node, lookupOffsetReg, lookupOffsetReg, (int64_t) J9VMTHREAD_OBJECT_MONITOR_CACHE_SIZE - 1, OOLConditions, 0);

            if (TR::Compiler->target.is64Bit())
               generateRSInstruction(cg, TR::InstOpCode::SLLG, node, lookupOffsetReg, lookupOffsetReg, trailingZeroes((int32_t) TR::Compiler->om.sizeofReferenceField()));
            else
               generateRSInstruction(cg, TR::InstOpCode::SLL, node, lookupOffsetReg, trailingZeroes((int32_t) TR::Compiler->om.sizeofReferenceField()));
            }

         // TODO No Need to use Memory Reference Here. Combine it with generateRXInstruction
         TR::MemoryReference * temp2MR = generateS390MemoryReference(cg->getMethodMetaDataRealRegister(), lookupOffsetReg, offsetOfMonitorLookupCache, cg);

#if defined(OMR_GC_COMPRESSED_POINTERS)
         generateRXInstruction(cg, TR::InstOpCode::LLGF, node, tempRegister, temp2MR, NULL);
         startICF = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, tempRegister, NULLVALUE, TR::InstOpCode::COND_BE, helperCallLabel, false, true);
#else
         generateRXInstruction(cg, TR::InstOpCode::getLoadTestOpCode(), node, tempRegister, temp2MR);
         startICF = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, helperCallLabel);
#endif

         int32_t offsetOfMonitor = offsetof(J9ObjectMonitor, monitor);
         // TODO No Need to use Memory Reference Here. Combine it with generateRXInstruction
         temp2MR = generateS390MemoryReference(tempRegister, offsetOfMonitor, cg);
         generateRXInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, objReg, temp2MR);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, helperCallLabel);

         int32_t offsetOfAlternateLockWord = offsetof(J9ObjectMonitor, alternateLockword);

         baseReg = tempRegister;
         lwOffset = 0 + offsetOfAlternateLockWord;

         // Check if the lockWord in the object contains our VMThread
         if (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord())
            generateRXInstruction(cg, TR::InstOpCode::C, node, metaReg, generateS390MemoryReference(baseReg, lwOffset, cg));
         else
            generateRXInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, metaReg, generateS390MemoryReference(baseReg, lwOffset, cg));

         // If VMThread does not match, call helper.
         TR::Instruction* helperBranch = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, helperCallLabel);

         // If VMThread matches, we can safely perform the monitor exit by zero'ing
         // out the lockWord on the object
         if (!TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z10))
            {
            if (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord())
               {
               generateRRInstruction(cg, TR::InstOpCode::XR, node, monitorReg, monitorReg);
               gcPoint = generateRXInstruction(cg, TR::InstOpCode::ST, node, monitorReg, generateS390MemoryReference(baseReg, lwOffset, cg));
               }
            else
               {
               generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, monitorReg, monitorReg);
               gcPoint = generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, monitorReg, generateS390MemoryReference(baseReg, lwOffset, cg));
               }
            }
         else
            {
            if (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord())
               gcPoint = generateSILInstruction(cg, TR::InstOpCode::MVHI, node, generateS390MemoryReference(baseReg, lwOffset, cg), 0);
            else
               gcPoint = generateSILInstruction(cg, TR::InstOpCode::getMoveHalfWordImmOpCode(), node, generateS390MemoryReference(baseReg, lwOffset, cg), 0);
            }

         generateS390BranchInstruction(cg,TR::InstOpCode::BRC,TR::InstOpCode::COND_BRC,node,helperReturnOOLLabel);

         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL , node, helperCallLabel );
         TR::RegisterDependencyConditions *deps = NULL;
         helperLink->buildDirectDispatch(node, &deps);
         TR::RegisterDependencyConditions *mergeConditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(OOLConditions, deps, cg);
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, helperReturnOOLLabel , mergeConditions);

         if (!disableOOL)
            {
            cursor = generateS390BranchInstruction(cg,TR::InstOpCode::BRC,TR::InstOpCode::COND_BRC,node,cFlowRegionEnd);
            if (debugObj)
               debugObj->addInstructionComment(cursor, "Denotes end of OOL monexit monitorCacheLookup: return to mainline");

            // Done using OOL with manual code generation
            monitorCacheLookupOOL->swapInstructionListsWithCompilation();
            }
         }

      lwOffset = 0;
      baseReg = tempRegister;
      simpleLocking = true;
      }
#endif

   // Lock Reservation happens only for objects with lockword.
   if (!simpleLocking && comp->getOption(TR_ReservingLocks))
      TR::TreeEvaluator::evaluateLockForReservation(node, &reserveLocking, &normalLockWithReservationPreserving, cg);
   if (reserveLocking)
      {
      // TODO - It would be much better to find a way not allocating these registers at the first place.
      cg->stopUsingRegister(monitorReg);
      return reservationLockExit(node, lwOffset, objectClassReg, cg, helperLink);
      }
   ////////////
   // Opcodes:
   bool use64b = true;
   if (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord())
      use64b = false;
   else if (!TR::Compiler->target.is64Bit())
      use64b = false;
   TR::InstOpCode::Mnemonic loadOp = use64b ? TR::InstOpCode::LG : TR::InstOpCode::L;
   TR::InstOpCode::Mnemonic loadRegOp = use64b ? TR::InstOpCode::LGR : TR::InstOpCode::LR;
   TR::InstOpCode::Mnemonic orImmOp = TR::InstOpCode::OILF;
   TR::InstOpCode::Mnemonic compareOp = use64b ? TR::InstOpCode::CGR : TR::InstOpCode::CR;
   TR::InstOpCode::Mnemonic compareImmOp = use64b ? TR::InstOpCode::CG : TR::InstOpCode::C;
   TR::InstOpCode::Mnemonic addImmOp = use64b ? TR::InstOpCode::AGHI : TR::InstOpCode::AHI;
   TR::InstOpCode::Mnemonic storeOp = use64b ? TR::InstOpCode::STG : TR::InstOpCode::ST;
   TR::InstOpCode::Mnemonic xorOp = use64b ? TR::InstOpCode::XGR : TR::InstOpCode::XR;
   TR::InstOpCode::Mnemonic casOp = use64b ? TR::InstOpCode::CSG : TR::InstOpCode::CS;
   TR::InstOpCode::Mnemonic loadImmOp = TR::InstOpCode::LGFI;
   TR::InstOpCode::Mnemonic andOp = use64b ? TR::InstOpCode::NGR : TR::InstOpCode::NR;
   TR::InstOpCode::Mnemonic andImmOp = TR::InstOpCode::NILF;
   TR::InstOpCode::Mnemonic moveImmOp = use64b ? TR::InstOpCode::MVGHI : TR::InstOpCode::MVHI;
   TR::InstOpCode::Mnemonic loadHalfWordImmOp = use64b ? TR::InstOpCode::LGHI : TR::InstOpCode::LHI;

   // Main path instruction sequence.
   // This sequence is the same for both normal locks and lock preservation.
   //    C       metaReg, #lwOffset(objectReg)
   //    BRC     MASK6, callLabel
   //    MVHI    #lwOffset(objectReg), 0

   //TODO - use compareAndBranch instruction
   // Check if the lockWord in the object contains our VMThread
   generateRXInstruction(cg, compareImmOp, node, metaReg, generateS390MemoryReference(baseReg, lwOffset, cg));
   // If VMThread does not match, call helper.
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, callLabel);
   if (normalLockWithReservationPreserving)
      cg->generateDebugCounter("LockExit/Preserving/MVHISuccessfull", 1, TR::DebugCounter::Undetermined);
   else
      cg->generateDebugCounter("LockExit/Normal/MVHISuccessfull", 1, TR::DebugCounter::Undetermined);
   // If VMThread matches, we can safely perform the monitor exit by zero'ing
   // out the lockWord on the object
   if (!TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z10))
      {
      generateRRInstruction(cg, xorOp, node, monitorReg, monitorReg);
      generateRXInstruction(cg, storeOp, node, monitorReg, generateS390MemoryReference(baseReg, lwOffset, cg));
      }
   else
      {
      generateSILInstruction(cg, moveImmOp, node, generateS390MemoryReference(baseReg, lwOffset, cg), 0);
      }

   TR_S390OutOfLineCodeSection *outlinedHelperCall = NULL;
   if (!disableOOL)
     {
     outlinedHelperCall = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(callLabel,cFlowRegionEnd,cg);
     cg->getS390OutOfLineCodeSectionList().push_front(outlinedHelperCall);
     outlinedHelperCall->swapInstructionListsWithCompilation();
     }
   TR::Instruction *cursor = generateS390LabelInstruction(cg,TR::InstOpCode::LABEL,node,callLabel);

   if (inlineRecursive)
      {
      // inlineRecursive is only enabled when OOL is enabled
      if (debugObj)
         {
         debugObj->addInstructionComment(cursor, "Denotes start of OOL monexit sequence");
         }

      // (on entry objectReg has been set up)

      //   Normal Lock                                           Lock reservation preserving
      // L     monitorReg, #lwOffset(objectReg)                L     monitorReg, #lwOffset(objectReg)
      // LHI   wasteReg, ~LOCK_RECURSION_MASK                  LHI   wasteReg, LOCK_OWNING_NON_INFLATED
      // AHI   monitorReg, -INC_DEC_VALUE
      // NR    wasteReg, monitorReg                            NR    wasteReg, monitorReg
      // CRJ   wasteReg, metaReg, MASK6, callHelper            CRJ   wasteReg, metaReg, MASK6, callHelper
      //                                                       LHI   wasteReg, LOCK_RECURSION_MASK
      //                                                       NR    wasteReg, monitorReg
      //                                                       BRC   BERC,     callHelper
      //                                                       LHI   wasteReg, LOCK_OWNING_NON_INFLATED
      //                                                       NR    wasteReg, monitorReg
      //                                                       CIJ   wasteReg, callHelper, BERC, LOCK_RES_CONTENDED_VALUE
      //                                                       AHI   monitorReg, -INC_DEC_VALUE
      // ST    monitorReg, #lwOffset(objectReg)                ST    monitorReg, #lwOffset(objectReg)
      // BRC   returnLabel                                     BRC   returnLabel
      // callHelper:                                           callHelper:
      // BRASL R14,jitMonitorExit                              BRASL R14, jitMonitorExit
      // returnLabel:                                          returnLabel:
      scratchRegister = cg->allocateRegister();
      conditions->addPostCondition(scratchRegister, TR::RealRegister::AssignAny);

      TR::MemoryReference * tempMR = generateS390MemoryReference(baseReg, lwOffset, cg);
      TR::MemoryReference * tempMR1 = generateS390MemoryReference(baseReg, lwOffset, cg);
      if(!normalLockWithReservationPreserving)
         {
         generateRXInstruction(cg, loadOp, node, monitorReg, tempMR);
         generateRIInstruction(cg, loadHalfWordImmOp, node, scratchRegister, ~OBJECT_HEADER_LOCK_RECURSION_MASK);
         generateRIInstruction(cg, addImmOp, node, monitorReg, -OBJECT_HEADER_LOCK_FIRST_RECURSION_BIT);
         generateRRInstruction(cg, andOp, node, scratchRegister, monitorReg);
         startICF = generateS390CompareAndBranchInstruction(cg, compareOp, node, scratchRegister, metaReg, TR::InstOpCode::COND_BNE, callHelper, false, false);
         cg->generateDebugCounter("LockExit/Normal/Recursive", 1, TR::DebugCounter::Undetermined);
         generateRXInstruction(cg, storeOp, node, monitorReg, tempMR1);
         }
      else
         {
         generateRXInstruction(cg, loadOp, node, monitorReg, tempMR);
         generateRIInstruction(cg, loadHalfWordImmOp, node, scratchRegister, ~LOCK_OWNING_NON_INFLATED_COMPLEMENT);
         generateRRInstruction(cg, andOp, node, scratchRegister, monitorReg);
         startICF = generateS390CompareAndBranchInstruction(cg, compareOp, node, scratchRegister, metaReg, TR::InstOpCode::COND_BNE, callHelper, false, false);
         generateRIInstruction(cg, loadHalfWordImmOp, node, scratchRegister, OBJECT_HEADER_LOCK_RECURSION_MASK);
         generateRRInstruction(cg, andOp, node, scratchRegister, monitorReg);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, callHelper);
         generateRIInstruction(cg, loadHalfWordImmOp, node, scratchRegister, LOCK_OWNING_NON_INFLATED_COMPLEMENT);
         generateRRInstruction(cg, andOp, node, scratchRegister, monitorReg);
         generateS390CompareAndBranchInstruction(cg, compareImmOp, node, scratchRegister, LOCK_RES_CONTENDED_VALUE, TR::InstOpCode::COND_BE, callHelper, false, false);
         cg->generateDebugCounter("LockExit/Preserving/Recursive", 1, TR::DebugCounter::Undetermined);
         generateRIInstruction(cg, addImmOp, node, monitorReg, -OBJECT_HEADER_LOCK_FIRST_RECURSION_BIT);
         generateRXInstruction(cg, storeOp, node, monitorReg, tempMR1);
         }
      generateS390BranchInstruction(cg,TR::InstOpCode::BRC,TR::InstOpCode::COND_BRC,node,returnLabel);
      tempMR->stopUsingMemRefRegister(cg);
      tempMR1->stopUsingMemRefRegister(cg);

      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, callHelper);
      if (normalLockWithReservationPreserving)
         cg->generateDebugCounter("LockExit/Preserving/VMHelper", 1, TR::DebugCounter::Undetermined);
      else
         cg->generateDebugCounter("LockExit/Normal/VMHelper", 1, TR::DebugCounter::Undetermined);
      }
   TR::RegisterDependencyConditions *deps = NULL;
   TR::Register *dummyResultReg = inlineRecursive ? helperLink->buildDirectDispatch(node, &deps) : helperLink->buildDirectDispatch(node);
   TR::RegisterDependencyConditions *mergeConditions = NULL;
   if (inlineRecursive)
      mergeConditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(conditions, deps, cg);
   else
      mergeConditions = conditions;

   generateS390LabelInstruction(cg,TR::InstOpCode::LABEL,node,returnLabel,mergeConditions);

   if (!disableOOL)
      {
      cursor = generateS390BranchInstruction(cg,TR::InstOpCode::BRC,TR::InstOpCode::COND_BRC,node,cFlowRegionEnd);
      if (debugObj)
         {
         debugObj->addInstructionComment(cursor, "Denotes end of OOL monexit: return to mainline");
         }
      // Done using OOL with manual code generation
      outlinedHelperCall->swapInstructionListsWithCompilation();
      }
   bool needDeps = false;
#if defined (J9VM_THR_LOCK_NURSERY)
   if (lwOffset <= 0 && disableOOL)
      needDeps = true;
#endif

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, conditions);

#if defined (J9VM_THR_LOCK_NURSERY)
   if (lwOffset <= 0 && disableOOL)
      cFlowRegionEnd->setEndInternalControlFlow();
#endif


   cg->stopUsingRegister(monitorReg);
   if (objectClassReg)
      cg->stopUsingRegister(objectClassReg);
   if (lookupOffsetReg)
      cg->stopUsingRegister(lookupOffsetReg);
   if (tempRegister && (tempRegister != objectClassReg))
      cg->stopUsingRegister(tempRegister);
   if (scratchRegister)
      cg->stopUsingRegister(scratchRegister);
   cg->decReferenceCount(objNode);

   return NULL;
   }

static void
roundArrayLengthToObjectAlignment(TR::CodeGenerator* cg, TR::Node* node, TR::Instruction*& iCursor, TR::Register* dataSizeReg,
      TR::RegisterDependencyConditions* conditions, TR::Register *litPoolBaseReg, int32_t allocSize, int32_t elementSize, TR::Register* sizeReg, TR::LabelSymbol * exitOOLLabel = NULL)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   int32_t alignmentConstant = fej9->getObjectAlignmentInBytes();
   if (exitOOLLabel)
      {
      TR_Debug * debugObj = cg->getDebug();
      iCursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, exitOOLLabel);
      //TODO if not outline stuff?
      if (debugObj)
         debugObj->addInstructionComment(iCursor, "Exit OOL, going back to main line");
      }

   // Size of array is headerSize + dataSize. If either aren't
   // multiples of alignment then their sum likely won't be
   bool needsAlignment = ( ((allocSize % alignmentConstant) != 0) ||
                           ((elementSize % alignmentConstant) != 0) );

   bool canCombineAGRs = ( ((allocSize % alignmentConstant) == 0) &&
                            (elementSize < alignmentConstant));

   if(!canCombineAGRs)
      iCursor = generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, sizeReg, dataSizeReg, iCursor);

   if(needsAlignment)
      {
      iCursor = generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, sizeReg, alignmentConstant - 1 + allocSize, iCursor);
      if (TR::Compiler->target.is64Bit())
         iCursor = generateS390ImmOp(cg, TR::InstOpCode::getAndOpCode(), node, sizeReg, sizeReg, -((int64_t) (alignmentConstant)), conditions, litPoolBaseReg);
      else
         iCursor = generateS390ImmOp(cg, TR::InstOpCode::getAndOpCode(), node, sizeReg, sizeReg, -((int32_t) (alignmentConstant)), conditions, litPoolBaseReg);
      }
   else
      {
      iCursor = generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, sizeReg, allocSize, iCursor);
      }
   }


static void
genHeapAlloc(TR::Node * node, TR::Instruction *& iCursor, bool isVariableLen, TR::Register * enumReg, TR::Register * resReg,
   TR::Register * zeroReg, TR::Register * dataSizeReg, TR::Register * sizeReg, TR::LabelSymbol * callLabel, int32_t allocSize,
   int32_t elementSize, TR::CodeGenerator * cg, TR::Register * litPoolBaseReg, TR::RegisterDependencyConditions * conditions,
   TR::Instruction *& firstBRCToOOL, TR::Instruction *& secondBRCToOOL, TR::LabelSymbol * exitOOLLabel = NULL)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   if (!TR::Options::getCmdLineOptions()->realTimeGC())
      {
      TR::Register *metaReg = cg->getMethodMetaDataRealRegister();

      // bool sizeInReg = (isVariableLen || (allocSize > MAX_IMMEDIATE_VAL));

      int alignmentConstant = fej9->getObjectAlignmentInBytes();

      if (isVariableLen)
         {
         if (exitOOLLabel)
            {
            TR_Debug * debugObj = cg->getDebug();
            iCursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, exitOOLLabel);
            if (debugObj)
               debugObj->addInstructionComment(iCursor, "Exit OOL, going back to main line");
            }
         // Detect large or negative number of elements, and call the helper in that case.
         // This 1MB limit comes from the cg.

         TR::Register * tmp = sizeReg;
         if (allocSize % alignmentConstant == 0 && elementSize < alignmentConstant)
            {
            tmp = dataSizeReg;
            }

         if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z196))
            {
            iCursor = generateRSInstruction(cg, TR::InstOpCode::SRAK, node, tmp, enumReg, 16, iCursor);
            }
         else
            {
            iCursor = generateRRInstruction(cg, TR::InstOpCode::LR, node, tmp, enumReg, iCursor);
            iCursor = generateRSInstruction(cg, TR::InstOpCode::SRA, node, tmp, 16, iCursor);
            }

         iCursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, callLabel, iCursor);
         if(!firstBRCToOOL)
            {
            firstBRCToOOL = iCursor;
            }
         else
            {
            secondBRCToOOL = iCursor;
            }
         }

      // We are loading up a partially constructed object. Don't let GC interfere with us
      // at this moment
      if (isVariableLen)
         {
         //Call helper to turn array length into size in bytes and do object alignment if necessary
         roundArrayLengthToObjectAlignment(cg, node, iCursor, dataSizeReg, conditions, litPoolBaseReg, allocSize, elementSize, sizeReg);

   #if defined(J9VM_INTERP_FLAGS_IN_CLASS_SLOT)
         // All arrays in combo builds will always be at least 12 bytes in size in all specs:
         //
         // 1)  class pointer + contig length + one or more elements
         // 2)  class pointer + 0 + 0 (for zero length arrays)
         //
         //Since objects are aligned to 8 bytes then the minimum size for an array must be 16 after rounding

         static_assert(J9_GC_MINIMUM_OBJECT_SIZE >= 8, "Expecting a minimum object size >= 8");
   #endif
         }

      // Calculate the after-allocation heapAlloc: if the size is huge,
      //   we need to check address wrap-around also. This is unsigned
      //   integer arithmetic, checking carry bit is enough to detect it.
      //   For variable length array, we did an up-front check already.

      static char * disableInitClear = feGetEnv("TR_disableInitClear");
      static char * disableBatchClear = feGetEnv("TR_DisableBatchClear");

      static char * useDualTLH = feGetEnv("TR_USEDUALTLH");

      TR::Register * addressReg = NULL, * lengthReg = NULL, * shiftReg = NULL;
      if (disableBatchClear && disableInitClear==NULL)
         {
         addressReg = cg->allocateRegister();
         lengthReg = cg->allocateRegister();
         shiftReg = cg->allocateRegister();

         if (conditions != NULL)
            {
            conditions->resetIsUsed();
            conditions->addPostCondition(addressReg, TR::RealRegister::AssignAny);
            conditions->addPostCondition(shiftReg, TR::RealRegister::AssignAny);
            conditions->addPostCondition(lengthReg, TR::RealRegister::AssignAny);
            }
         }

      if (isVariableLen)
         {
         if (disableBatchClear && disableInitClear==NULL)
            iCursor = generateRRInstruction(cg, TR::InstOpCode::LGR, node, lengthReg, sizeReg, iCursor);
         if (!comp->getOption(TR_DisableDualTLH) && useDualTLH && node->canSkipZeroInitialization())
            {
            iCursor = generateRXInstruction(cg, TR::InstOpCode::getAddOpCode(), node, sizeReg,
                  generateS390MemoryReference(metaReg, offsetof(J9VMThread, nonZeroHeapAlloc), cg), iCursor);
            }
         else
            {
            iCursor = generateRXInstruction(cg, TR::InstOpCode::getAddOpCode(), node, sizeReg,
                  generateS390MemoryReference(metaReg, offsetof(J9VMThread, heapAlloc), cg), iCursor);
            }
         }
      else
         {
         if (TR::Compiler->target.is64Bit())
            iCursor = genLoadLongConstant(cg, node, allocSize, sizeReg, iCursor, conditions);
         else
            iCursor = generateLoad32BitConstant(cg, node, allocSize, sizeReg, true, iCursor, conditions);

         if (disableBatchClear && disableInitClear==NULL)
            iCursor = generateRRInstruction(cg, TR::InstOpCode::LGR, node, lengthReg, sizeReg, iCursor);

         if (!comp->getOption(TR_DisableDualTLH) && useDualTLH && node->canSkipZeroInitialization())
            {
            iCursor = generateRXInstruction(cg, TR::InstOpCode::getAddOpCode(), node, sizeReg,
                  generateS390MemoryReference(metaReg, offsetof(J9VMThread, nonZeroHeapAlloc), cg), iCursor);
            }
         else
            {
            iCursor = generateRXInstruction(cg, TR::InstOpCode::getAddOpCode(), node, sizeReg,
                  generateS390MemoryReference(metaReg, offsetof(J9VMThread, heapAlloc), cg), iCursor);
            }

         }

      if (allocSize > cg->getMaxObjectSizeGuaranteedNotToOverflow())
         {
         iCursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BO, node, callLabel, iCursor);
         if(!firstBRCToOOL)
            {
            firstBRCToOOL = iCursor;
            }
         else
            {
            secondBRCToOOL = iCursor;
            }
         }

      if (!comp->getOption(TR_DisableDualTLH) && useDualTLH && node->canSkipZeroInitialization())
         {
               iCursor = generateRXInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, sizeReg,
                            generateS390MemoryReference(metaReg, offsetof(J9VMThread, nonZeroHeapTop), cg), iCursor);

            // Moving the BRC before load so that the return object can be dead right after BRASL when heap alloc OOL opt is enabled
         iCursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BH, node, callLabel, iCursor);
         if(!firstBRCToOOL)
            {
            firstBRCToOOL = iCursor;
            }
         else
            {
            secondBRCToOOL = iCursor;
            }


         iCursor = generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, resReg,
               generateS390MemoryReference(metaReg, offsetof(J9VMThread, nonZeroHeapAlloc), cg), iCursor);
         }
      else
         {
         iCursor = generateRXInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, sizeReg,
                      generateS390MemoryReference(metaReg, offsetof(J9VMThread, heapTop), cg), iCursor);

            // Moving the BRC before load so that the return object can be dead right after BRASL when heap alloc OOL opt is enabled
         iCursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BH, node, callLabel, iCursor);
         if(!firstBRCToOOL)
            {
            firstBRCToOOL = iCursor;
            }
         else
            {
            secondBRCToOOL = iCursor;
            }


         iCursor = generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, resReg,
                                   generateS390MemoryReference(metaReg, offsetof(J9VMThread, heapAlloc), cg), iCursor);
         }


      if (!comp->getOption(TR_DisableDualTLH) && useDualTLH && node->canSkipZeroInitialization())
         iCursor = generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, sizeReg,
                      generateS390MemoryReference(metaReg, offsetof(J9VMThread, nonZeroHeapAlloc), cg), iCursor);
      else
         iCursor = generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, sizeReg,
                      generateS390MemoryReference(metaReg, offsetof(J9VMThread, heapAlloc), cg), iCursor);
      TR::LabelSymbol * fillerRemLabel = generateLabelSymbol(cg);
      TR::LabelSymbol * doneLabel = generateLabelSymbol(cg);

      TR::LabelSymbol * fillerLoopLabel = generateLabelSymbol(cg);

      // do this clear, if disableBatchClear is on
      if (disableBatchClear && disableInitClear==NULL) //&& (node->getOpCodeValue() == TR::anewarray) && (node->getFirstChild()->getInt()>0) && (node->getFirstChild()->getInt()<6) )
         {
         iCursor = generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, addressReg, resReg, iCursor);
         // Dont overwrite the class
         //
         iCursor = generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, shiftReg, lengthReg, iCursor);
         iCursor = generateRSInstruction(cg, TR::InstOpCode::SRA, node, shiftReg, 8);

         iCursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZ, node, fillerRemLabel, iCursor);
         iCursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, fillerLoopLabel);
         iCursor = generateSS1Instruction(cg, TR::InstOpCode::XC, node, 255, generateS390MemoryReference(addressReg, 0, cg), generateS390MemoryReference(addressReg, 0, cg), iCursor);

         iCursor = generateRXInstruction(cg, TR::InstOpCode::LA, node, addressReg, generateS390MemoryReference(addressReg, 256, cg), iCursor);

         iCursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRCT, node, shiftReg, fillerLoopLabel);
         iCursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, fillerRemLabel);

         // and to only get the right 8 bits (remainder)
         iCursor = generateRIInstruction(cg, TR::InstOpCode::NILL, node, lengthReg, 0x00FF);
         iCursor = generateRIInstruction(cg, TR::InstOpCode::AHI, node, lengthReg, -1);
         // branch to done if length < 0

         iCursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, node, doneLabel, iCursor);

         iCursor = generateSS1Instruction(cg, TR::InstOpCode::XC, node, 0, generateS390MemoryReference(addressReg, 0, cg), generateS390MemoryReference(addressReg, 0, cg), iCursor);

         // minus 1 from lengthreg since xc auto adds 1 to it

         iCursor = generateEXDispatch(node, cg, lengthReg, shiftReg, iCursor);
         iCursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneLabel);
         }
      cg->stopUsingRegister(addressReg);
      cg->stopUsingRegister(shiftReg);
      cg->stopUsingRegister(lengthReg);

      if (zeroReg != NULL)
         {
         iCursor = generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, zeroReg, zeroReg, iCursor);
         }
      }
   else
      {
      TR_ASSERT(0, "genHeapAlloc() not supported for RT");
      }
   }



static void
genInitObjectHeader(TR::Node * node, TR::Instruction *& iCursor, TR_OpaqueClassBlock * classAddress, TR::Register * classReg, TR::Register * resReg,
      TR::Register * zeroReg, TR::Register * temp1Reg, TR::Register * litPoolBaseReg,
      TR::RegisterDependencyConditions * conditions,
      TR::CodeGenerator * cg, TR::Register * enumReg = NULL, bool canUseIIHF = false)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   TR_J9VM *fej9vm = (TR_J9VM *)(comp->fe());
   if (!TR::Options::getCmdLineOptions()->realTimeGC())
      {
      J9ROMClass *romClass = 0;
      int32_t staticFlag = 0;
      uint32_t orFlag = 0;
      TR::Register *metaReg = cg->getMethodMetaDataRealRegister();
      TR_ASSERT(classAddress, "Cannot have a null OpaqueClassBlock\n");
      romClass = TR::Compiler->cls.romClassOf(classAddress);
      staticFlag = romClass->instanceShape;

      // a pointer to the virtual register that will actually hold the class pointer.
      TR::Register * clzReg = classReg;
      // TODO: Following approach for initializing object header for array of objects in AOT is conservative.
      // We need support for relocation in generating RIL type instruction. If we have support, we can use
      // same sequence generated in JIT which saves us a load and store.
      if (comp->compileRelocatableCode())
         {
         if (node->getOpCodeValue() == TR::newarray)
            {
            iCursor = generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, temp1Reg,
                  generateS390MemoryReference(metaReg, offsetof(J9VMThread, javaVM), cg), iCursor);
            iCursor = generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, temp1Reg,
                  generateS390MemoryReference(temp1Reg,
                        fej9vm->getPrimitiveArrayOffsetInJavaVM(node->getSecondChild()->getInt()), cg),
                        iCursor);
            clzReg = temp1Reg;
            }
         else if (node->getOpCodeValue() == TR::anewarray)
            {
            TR_ASSERT(classReg, "must have a classReg for TR::anewarray in AOT mode");
            iCursor = generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, temp1Reg,
                  generateS390MemoryReference(classReg, offsetof(J9Class, arrayClass), cg), iCursor);
            clzReg = temp1Reg;
            //clzReg = classReg;
            }
         else
            {
            TR_ASSERT(node->getOpCodeValue() == TR::New && classReg,
                      "must have a classReg for TR::New in AOT mode");
            clzReg = classReg;
            }
         }

      // Store the class
      if (clzReg == NULL)
         {
         if (cg->wantToPatchClassPointer(classAddress, node))
            {
            iCursor = genLoadAddressConstantInSnippet(cg, node, (intptr_t) classAddress | (intptrj_t)orFlag, temp1Reg, iCursor, conditions, litPoolBaseReg, true);
            if (orFlag != 0)
               {
#ifdef OMR_GC_COMPRESSED_POINTERS
               iCursor = generateS390ImmOp(cg, TR::InstOpCode::O, node, temp1Reg, temp1Reg, (int32_t)orFlag, conditions, litPoolBaseReg);
#else
               if (TR::Compiler->target.is64Bit())
                  iCursor = generateS390ImmOp(cg, TR::InstOpCode::OG, node, temp1Reg, temp1Reg, (int64_t)orFlag, conditions, litPoolBaseReg);
               else
                  iCursor = generateS390ImmOp(cg, TR::InstOpCode::O, node, temp1Reg, temp1Reg, (int32_t)orFlag, conditions, litPoolBaseReg);
#endif
               }
            }
         else
            {
            //case for arraynew and anewarray for compressedrefs and 31 bit
            /*
             * node->getOpCodeValue() == TR::newarray
             [0x484DF88C20]   LGFI    GPR15,674009856
             [0x484DF88DD8]   ST      GPR15,#613 0(GPR3)
             [0x484DF88F60]   ST      GPR2,#614 4(GPR3)

            to
            IIHF
            STG      GPR2,#614 0(GPR3)

             */

            if (!canUseIIHF)
               iCursor = genLoadAddressConstant(cg, node, (intptr_t) classAddress | (intptrj_t)orFlag, temp1Reg, iCursor, conditions, litPoolBaseReg);
            }
         if (canUseIIHF)
            {
            iCursor = generateRILInstruction(cg, TR::InstOpCode::IIHF, node, enumReg, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(classAddress)) | orFlag, iCursor);
            }
         else
            {
#ifdef OMR_GC_COMPRESSED_POINTERS
            // must store just 32 bits (class offset)

            iCursor = generateRXInstruction(cg, TR::InstOpCode::ST, node, temp1Reg,
                  generateS390MemoryReference(resReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg), iCursor);
#else
            iCursor = generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, temp1Reg,
                  generateS390MemoryReference(resReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg), iCursor);
#endif
            }
         }
      else
         {
#ifdef OMR_GC_COMPRESSED_POINTERS
         // must store just 32 bits (class offset)
         iCursor = generateRXInstruction(cg, TR::InstOpCode::ST, node, clzReg,
               generateS390MemoryReference(resReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg), iCursor);
#else
         iCursor = generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, clzReg,
               generateS390MemoryReference(resReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg), iCursor);
#endif
         }

#ifndef J9VM_INTERP_FLAGS_IN_CLASS_SLOT
#if defined(J9VM_OPT_NEW_OBJECT_HASH)

      TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
      bool isStaticFlag = fej9->isStaticObjectFlags();

      // If the object flags cannot be determined at compile time, we have to add a load
      // for it. And then, OR it with temp1Reg.
      if (isStaticFlag)
         {
         // The object flags can be determined at compile time.
         staticFlag |= fej9->getStaticObjectFlags();
         if (staticFlag != 0)
            {
            if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z10) && staticFlag >= MIN_IMMEDIATE_VAL && staticFlag <= MAX_IMMEDIATE_VAL)
               {
               iCursor = generateSILInstruction(cg, TR::InstOpCode::MVHI, node, generateS390MemoryReference(resReg, TMP_OFFSETOF_J9OBJECT_FLAGS, cg), staticFlag, iCursor);
               }
            else
               {
               iCursor = generateLoad32BitConstant(cg, node, staticFlag, temp1Reg, true, iCursor, conditions, litPoolBaseReg);
               // Store the flags
               iCursor = generateRXInstruction(cg, TR::InstOpCode::ST, node, temp1Reg,
                     generateS390MemoryReference(resReg, TMP_OFFSETOF_J9OBJECT_FLAGS, cg), iCursor);
               }
            }
         }
      else
         {
         // If the object flags cannot be determined at compile time, we add a load for it.
         if(!comp->getOption(TR_DisableDualTLH) && useDualTLH && node->canSkipZeroInitialization())
            iCursor = generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, temp1Reg,
                  generateS390MemoryReference(metaReg, offsetof(J9VMThread, nonZeroAllocateThreadLocalHeap.objectFlags), cg), iCursor);
         else
            iCursor = generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, temp1Reg,
                  generateS390MemoryReference(metaReg, offsetof(J9VMThread, allocateThreadLocalHeap.objectFlags), cg), iCursor);

         // OR staticFlag with temp1Reg
         if (staticFlag)
            iCursor = generateS390ImmOp(cg, TR::InstOpCode::O, node, temp1Reg, temp1Reg, staticFlag, conditions, litPoolBaseReg);
         // Store the flags
         iCursor = generateRXInstruction(cg, TR::InstOpCode::ST, node, temp1Reg,
               generateS390MemoryReference(resReg, TMP_OFFSETOF_J9OBJECT_FLAGS, cg), iCursor);
         //iCursor = generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, temp1Reg,
         //           generateS390MemoryReference(resReg, TMP_OFFSETOF_J9OBJECT_FLAGS, cg), iCursor);
         }
#endif /* J9VM_OPT_NEW_OBJECT_HASH */
#endif /* FLAGS_IN_CLASS_SLOT */


#if !defined(J9VM_THR_LOCK_NURSERY)
      // Init monitor
      if (zeroReg != NULL)
         {

         if (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord())
            iCursor = generateRXInstruction(cg, TR::InstOpCode::ST, node, zeroReg,
                  generateS390MemoryReference(resReg, TMP_OFFSETOF_J9OBJECT_MONITOR, cg), iCursor);
         else
            iCursor = generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, zeroReg,
                  generateS390MemoryReference(resReg, TMP_OFFSETOF_J9OBJECT_MONITOR, cg), iCursor);
         }
#endif
#if defined(J9VM_THR_LOCK_NURSERY) && defined(J9VM_THR_LOCK_NURSERY_FAT_ARRAYS)
      // Initialize monitor slots
      // for arrays that have a lock
      // word
      int32_t lwOffset = fej9->getByteOffsetToLockword(classAddress);
      if ((zeroReg != NULL) &&
            (node->getOpCodeValue() != TR::New) &&
            (lwOffset > 0))
         {
         if (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord())
            iCursor = generateRXInstruction(cg, TR::InstOpCode::ST, node, zeroReg,
                  generateS390MemoryReference(resReg, TMP_OFFSETOF_J9INDEXABLEOBJECT_MONITOR, cg), iCursor);
         else
            iCursor = generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, zeroReg,
                  generateS390MemoryReference(resReg, TMP_OFFSETOF_J9INDEXABLEOBJECT_MONITOR, cg), iCursor);
         }
#endif
      }
   else
      {
      TR_ASSERT(0, "genInitObjecHeader not supported for RT");
      }

   }


static void
genAlignDoubleArray(TR::Node * node, TR::Instruction *& iCursor, bool isVariableLen, TR::Register * resReg, int32_t objectSize,
   int32_t dataBegin, TR::Register * dataSizeReg, TR::Register * temp1Reg, TR::Register * temp2Reg, TR::Register * litPoolBaseReg,
   TR::RegisterDependencyConditions * conditions, TR::CodeGenerator * cg)
   {
   TR::LabelSymbol * slotAtStart = generateLabelSymbol(cg);
   TR::LabelSymbol * doneAlign = generateLabelSymbol(cg);

   iCursor = generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, temp1Reg, resReg, iCursor);
   iCursor = generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, temp2Reg, 3, iCursor);
   iCursor = generateS390ImmOp(cg, TR::InstOpCode::N, node, temp1Reg, temp1Reg, 7, conditions, litPoolBaseReg);
   iCursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNZ, node, slotAtStart, iCursor);

   // The slop bytes are at the end of the allocated object.
   if (isVariableLen)
      {
      if (TR::Compiler->target.is64Bit())
         {
         iCursor = generateRRInstruction(cg, TR::InstOpCode::LGFR, node, dataSizeReg, dataSizeReg, iCursor);
         }

      iCursor = generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, temp2Reg,
            generateS390MemoryReference(resReg, dataSizeReg, dataBegin, cg), iCursor);
      }
   else if (objectSize >= MAXDISP)
      {
      iCursor = genLoadAddressConstant(cg, node, (intptrj_t) objectSize, temp1Reg, iCursor, conditions);
      iCursor = generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, temp2Reg,
                   generateS390MemoryReference(resReg, temp1Reg, 0, cg), iCursor);
      }
   else
      {
      iCursor = generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, temp2Reg,
                   generateS390MemoryReference(resReg, objectSize, cg), iCursor);
      }
   iCursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, doneAlign, iCursor);

   // the slop bytes are at the start of the allocation
   iCursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, slotAtStart, iCursor);
   iCursor = generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, temp2Reg,
                generateS390MemoryReference(resReg, (int32_t) 0, cg), iCursor);
   iCursor = generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, resReg, 4, iCursor);
   iCursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneAlign, iCursor);
   }


static void
genInitArrayHeader(TR::Node * node, TR::Instruction *& iCursor, bool isVariableLen, TR_OpaqueClassBlock * classAddress, TR::Register * classReg,
   TR::Register * resReg, TR::Register * zeroReg, TR::Register * eNumReg, TR::Register * dataSizeReg, TR::Register * temp1Reg,
   TR::Register * litPoolBaseReg, TR::RegisterDependencyConditions * conditions, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   bool canUseIIHF= false;
   if (!comp->compileRelocatableCode() && (node->getOpCodeValue() == TR::newarray || node->getOpCodeValue() == TR::anewarray)
#ifndef OMR_GC_COMPRESSED_POINTERS
         && TR::Compiler->target.is32Bit()
#endif
#ifndef J9VM_INTERP_FLAGS_IN_CLASS_SLOT
#if defined(J9VM_OPT_NEW_OBJECT_HASH)
         && false
#endif /* J9VM_OPT_NEW_OBJECT_HASH */
#endif /* FLAGS_IN_CLASS_SLOT */
      )
      {
      canUseIIHF = true;
      }
   genInitObjectHeader(node, iCursor, classAddress, classReg, resReg, zeroReg, temp1Reg, litPoolBaseReg, conditions, cg, eNumReg, canUseIIHF);

   // Store the array size
   if (canUseIIHF)
      {
      iCursor = generateRXInstruction(cg, TR::InstOpCode::STG, node, eNumReg,
                      generateS390MemoryReference(resReg, TR::Compiler->om.offsetOfObjectVftField(), cg), iCursor);
      }
   else
      iCursor = generateRXInstruction(cg, TR::InstOpCode::ST, node, eNumReg,
                generateS390MemoryReference(resReg, fej9->getOffsetOfContiguousArraySizeField(), cg), iCursor);

   static char * allocZeroArrayWithVM = feGetEnv("TR_VMALLOCZEROARRAY");
   static char * useDualTLH = feGetEnv("TR_USEDUALTLH");
   //write 0
   if(!comp->getOption(TR_DisableDualTLH) && useDualTLH && node->canSkipZeroInitialization() && allocZeroArrayWithVM == NULL)
      iCursor = generateRXInstruction(cg, TR::InstOpCode::ST, node, eNumReg,
                      generateS390MemoryReference(resReg, fej9->getOffsetOfDiscontiguousArraySizeField(), cg), iCursor);
   }

TR::Register *
J9::Z::TreeEvaluator::VMnewEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   int32_t allocateSize, objectSize, dataBegin;
   TR::ILOpCodes opCode;
   TR_OpaqueClassBlock * classAddress = 0;
   TR::Register *classReg              = NULL;
   TR::Register *resReg                = NULL;
   TR::Register *zeroReg               = NULL;
   TR::Register *litPoolBaseReg        = NULL;
   TR::Register *enumReg               = NULL;
   TR::Register *copyReg               = NULL;
   TR::Register *classRegAOT           = NULL;
   TR::Register *temp1Reg              = NULL;
   TR::Register *callResult            = NULL;
   TR::Register *dataSizeReg           = NULL;
   TR::Node     *litPoolBaseChild      = NULL;
   TR::Register *copyClassReg          = NULL;

   TR_S390ScratchRegisterManager *srm = cg->generateScratchRegisterManager();

   TR::LabelSymbol * callLabel, * cFlowRegionEnd;
   TR_S390OutOfLineCodeSection* outlinedSlowPath = NULL;
   TR::RegisterDependencyConditions * conditions;
   TR::Instruction * iCursor = NULL;
   bool isArray = false, isDoubleArray = false;
   bool isVariableLen;
   int32_t litPoolRegTotalUse, temp2RegTotalUse;
   int32_t elementSize;
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());


   /* New Evaluator Optimization: Using OOL instead of snippet for heap alloc
    * The purpose of moving to an OOL from a snippet is that we don't need to
    * hard code dependencies at the merge label, hence it could possibly reduce
    * spills. When we have a snippet, then all registers used in between the
    * branch to the snippet and the merge label need to be assigned to specific
    * registers and added to the dependencies at the merge label.
    * Option to disable it: disableHeapAllocOOL */

   /* Variables needed for Heap alloc OOL Opt */
   TR::Register * tempResReg;//Temporary register used to get the result from the BRASL call in heap alloc OOL
   TR::RegisterDependencyConditions * heapAllocDeps1;//Dependencies needed for BRASL call in heap alloc OOL
   TR::Instruction *firstBRCToOOL = NULL;
   TR::Instruction *secondBRCToOOL = NULL;

   bool outlineNew = false;
   bool disableOutlinedNew = comp->getOption(TR_DisableOutlinedNew);

   //Disabling outline new for now
   //TODO: enable later
   static bool enableOutline = (feGetEnv("TR_OutlineNew")!=NULL);
   disableOutlinedNew = !enableOutline;

   bool generateArraylets = comp->generateArraylets();

   // in time, the tlh will probably always be batch cleared, and therefore it will not be
   // necessary for the JIT-generated inline code to do the clearing of fields. But, 2 things
   // have to happen first:
   // 1.The JVM has to change it's code so that it has batch clearing on for 390 (it is currently only
   //   on if turned on as a runtime option)
   // 2.The JVM has to support the call - on z/OS, Modron GC is not enabled yet and so batch tlh clearing
   //   can not be enabled yet.
   static bool needZeroReg = !fej9->tlhHasBeenCleared();

   opCode = node->getOpCodeValue();

   // Since calls to canInlineAllocate could result in different results during the same compilation,
   // We must be conservative and only do inline allocation if the first call (in LocalOpts.cpp) has succeeded and we have the litPoolBaseChild added.
   // Refer to defects 161084 and 87089
   bool doInline = cg->doInlineAllocate(node);

   static int count = 0;
   doInline = doInline &&
   performTransformation(comp, "O^O <%3d> Inlining Allocation of %s [0x%p].\n", count++, node->getOpCode().getName(), node);

   if (doInline)
      {

      static char *maxNumOfOOLOptStr = feGetEnv("TR_MaxNumOfOOLOpt");
      if (maxNumOfOOLOptStr)
         {
         int maxNumOfOOLOpt = atoi(maxNumOfOOLOptStr);
         traceMsg(cg->comp(),"TR_MaxNumOfOOLOpt: count=%d, maxNumOfOOLOpt=%d %d\n", count, maxNumOfOOLOpt, __LINE__);
         if(maxNumOfOOLOpt < count)
            {
            traceMsg(cg->comp(),"TR_MaxNumOfOOLOpt: maxNumOfOOLOpt < count, Setting TR_DisableHeapAllocOOL %d\n", __LINE__);
            comp->setOption(TR_DisableHeapAllocOOL);
            }
         }

      objectSize = comp->canAllocateInline(node, classAddress);
      isVariableLen = (objectSize == 0);
      allocateSize = objectSize;
      callLabel = generateLabelSymbol(cg);
      cFlowRegionEnd = generateLabelSymbol(cg);
      conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(10, 13, cg);
      if (!comp->getOption(TR_DisableHeapAllocOOL))
         {
         heapAllocDeps1 = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(2, 4, cg);
         }
      TR::Node * firstChild = node->getFirstChild();
      TR::Node * secondChild = NULL;

      // load literal pool register
      if (((node->getNumChildren()==3) && ((node->getOpCodeValue()==TR::anewarray)
                        || (node->getOpCodeValue()==TR::newarray))) ||
            ((node->getNumChildren()==2) && (node->getOpCodeValue()==TR::New)))
         {
         litPoolBaseChild=node->getLastChild();
         TR_ASSERT((litPoolBaseChild->getOpCodeValue()==TR::aload) || (litPoolBaseChild->getOpCodeValue()==TR::aRegLoad),
               "Literal pool base child expected\n");
         litPoolBaseReg=cg->evaluate(litPoolBaseChild);
         litPoolRegTotalUse = litPoolBaseReg->getTotalUseCount();
         }

      //////////////////////////////////////////////////////////////////////////////////////////////////////
      ///============================ STAGE 1: Evaluate Children ========================================///
      //////////////////////////////////////////////////////////////////////////////////////////////////////
      if (opCode == TR::New)
         {
         classReg = cg->evaluate(firstChild);
         dataBegin = sizeof(J9Object);
         }
      else
         {
         isArray = true;
         TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
         if (generateArraylets || TR::Compiler->om.useHybridArraylets())
            {
            if (node->getOpCodeValue() == TR::newarray)
            elementSize = TR::Compiler->om.getSizeOfArrayElement(node);
            else if (comp->useCompressedPointers())
            elementSize = TR::Compiler->om.sizeofReferenceField();
            else
            elementSize = TR::Compiler->om.sizeofReferenceAddress();

            if (generateArraylets)
            dataBegin = fej9->getArrayletFirstElementOffset(elementSize, comp);
            else
            dataBegin = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
            }
         else
            {
            dataBegin = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
            elementSize = TR::Compiler->om.getSizeOfArrayElement(node);
            }
         secondChild = node->getSecondChild();
         // For TR::newarray, classReg is not the real class actually.
         if (!comp->getOption(TR_DisableHeapAllocOOL))
            {
            /* Evaluate the second child node with info about the type of object in the mainline only
             * when it's an evaluation for anewarray or packed anewarray or if the second child's opcode
             * is not a load const. Otherwise, we evaluate the second child manually in OOL since it's
             * not used anywhere in the mainline, hence keeping a register unnecessarily live for a very
             * long time before it is killed. */

            if (!secondChild->getOpCode().isLoadConst() || node->getOpCodeValue() == TR::anewarray)
               {
               classReg = cg->evaluate(secondChild);
               }
            }
         else
            {
            classReg = cg->evaluate(secondChild);
            }

         // Potential helper call requires us to evaluate the arguments always.
         enumReg = cg->evaluate(firstChild);
         if (!cg->canClobberNodesRegister(firstChild))
            {
            copyReg = cg->allocateRegister();
            TR::InstOpCode::Mnemonic loadOpCode = (firstChild->getType().isInt64()) ? TR::InstOpCode::LGR : TR::InstOpCode::LR;
            iCursor = generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, copyReg, enumReg, iCursor);
            enumReg = copyReg;
            }
         }

      //////////////////////////////////////////////////////////////////////////////////////////////////////
      ///============================ STAGE 1.1: Use OutlinedNew? =========================================///
      //////////////////////////////////////////////////////////////////////////////////////////////////////
      if (opCode != TR::anewarray && opCode != TR::New && opCode != TR::newarray)
         {
         //for now only enabling for TR::New
         disableOutlinedNew = true;
         }
      else if (generateArraylets)
         {
         if (comp->getOption(TR_TraceCG))
         traceMsg(comp, "OUTLINED NEW: Disable for %s %p because outlined allocation can't deal with arraylets\n", node->getOpCode().getName(), node);
         disableOutlinedNew = true;
         cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "cg.new/refusedToOutline/arraylets/%s", node->getOpCode().getName()), 1, TR::DebugCounter::Undetermined);
         }
      else if (comp->getMethodHotness() > warm)
         {
         if (comp->getOption(TR_TraceCG))
         traceMsg(comp, "OUTLINED NEW: Disable for %p because opt level is %s\n", node, comp->getHotnessName());
         disableOutlinedNew = true;
         cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "cg.new/refusedToOutline/optlevel/%s", comp->getHotnessName()), 1, TR::DebugCounter::Undetermined);
         }
      else if (needZeroReg)
         {
         if (comp->getOption(TR_TraceCG))
         traceMsg(comp, "OUTLINED NEW: Disable for %p due to tlhNotCleared\n", node);
         disableOutlinedNew = true;
         cg->generateDebugCounter("cg.new/refusedToOutline/tlhNotCleared", 1, TR::DebugCounter::Undetermined);
         }
      else if (allocateSize > cg->getMaxObjectSizeGuaranteedNotToOverflow())
         {
         if (comp->getOption(TR_TraceCG))
         traceMsg(comp, "OUTLINED NEW: Disable for %p due to allocate large object size\n", node);
         disableOutlinedNew = true;
         cg->generateDebugCounter("cg.new/refusedToOutline/largeObjectSize", 1, TR::DebugCounter::Undetermined);
         }

      if (!disableOutlinedNew && performTransformation(comp, "O^O OUTLINED NEW: outlining %s %p, size %d\n", node->getOpCode().getName(), node, allocateSize))
      outlineNew = true;

      //////////////////////////////////////////////////////////////////////////////////////////////////////
      ///============================ STAGE 2: Setup Register Dependencies===============================///
      //////////////////////////////////////////////////////////////////////////////////////////////////////

      temp1Reg = srm->findOrCreateScratchRegister();
      resReg = cg->allocateCollectedReferenceRegister();

      if (needZeroReg)
         zeroReg = srm->findOrCreateScratchRegister();
      conditions->addPostCondition(classReg, TR::RealRegister::AssignAny);
      if (enumReg)
         {
         conditions->addPostCondition(enumReg, TR::RealRegister::AssignAny);
         traceMsg(comp,"enumReg = %s\n", enumReg->getRegisterName(comp));
         }
      conditions->addPostCondition(resReg, TR::RealRegister::AssignAny);
      traceMsg(comp, "classReg = %s , resReg = %s \n", classReg->getRegisterName(comp), resReg->getRegisterName(comp));
      /* VM helper function for heap alloc expects these parameters to have these values:
       * GPR1 -> Type of Object
       * GPR2 -> Size/Number of objects (if applicable) */
      // We don't need these many registers dependencies as Outlined path will only contain helper call
      TR::Register *copyEnumReg = enumReg;
      TR::Register *copyClassReg = classReg;

      //////////////////////////////////////////////////////////////////////////////////////////////////////
      ///============================ STAGE 3: Calculate Allocation Size ================================///
      //////////////////////////////////////////////////////////////////////////////////////////////////////
      // Three possible outputs:
      // if variable-length array   - dataSizeReg will contain the (calculated) size
      // if outlined                - tmpReg will contain the value of
      // otherwise                  - size is in (int) allocateSize
      int alignmentConstant = fej9->getObjectAlignmentInBytes();

      if (isVariableLen)
         allocateSize += dataBegin;
      else
         allocateSize = (allocateSize + alignmentConstant - 1) & (-alignmentConstant);

      TR::LabelSymbol * exitOOLLabel = NULL;


      if (isVariableLen)
         {

         //want to fold some of the
         /*
          * figure out packed arrays
          * LTGFR   GPR14,GPR2
          * SLLG    GPR14,GPR14,1
          BRC     BE(0x8), Snippet Label [0x484BD04470] <------combine LTGFR + SLLG to RSIBG

          LR      GPR15,GPR2
          SRA     GPR15,16
          BRC     MASK6(0x6), Snippet Label [0x484BD04470]      # (Start of internal control flow)
          LG      GPR3,#511 96(GPR13)
          1     AGHI    GPR14,7  <---can combine 1 & 3 when allocateSize (8) is multiple of alignmentConstant (8), but need
          to re-arrange some of the registers, result is expected in GPR15
          2     NILF    GPR14,-8
          3     LGHI    GPR15,8
          4     AGR     GPR15,GPR14
          5     AGR     GPR15,GPR3
          CLG     GPR15,#513 104(GPR13)

          final:

          *
          */
         TR::Register * tmp = NULL;
         dataSizeReg = srm->findOrCreateScratchRegister();
         if (allocateSize % alignmentConstant == 0 && elementSize < alignmentConstant)
            {
            if (outlineNew)
               tmp = resReg;
            else
               tmp = temp1Reg;
            }
         else
            {
            tmp = dataSizeReg;
            }

         /*     if (elementSize >= 2)
          {
          if (TR::Compiler->target.is64Bit())
          iCursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, dataSizeReg, dataSizeReg, trailingZeroes(elementSize), iCursor);
          else
          iCursor = generateRSInstruction(cg, TR::InstOpCode::SLL, node, dataSizeReg, trailingZeroes(elementSize), iCursor);
          } */
         if (callLabel != NULL && (node->getOpCodeValue() == TR::anewarray ||
                     node->getOpCodeValue() == TR::newarray))
            {
            TR_Debug * debugObj = cg->getDebug();
            TR::LabelSymbol * startOOLLabel = generateLabelSymbol(cg);
            exitOOLLabel = generateLabelSymbol(cg);
            TR_S390OutOfLineCodeSection *zeroSizeArrayChckOOL;
            if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z10) && TR::Compiler->target.is64Bit())
               {
               //need 31 bit as well, combining lgfr + sllg into rsibg
               int32_t shift_amount = trailingZeroes(elementSize);
               iCursor = generateRIEInstruction(cg, TR::InstOpCode::RISBG, node, tmp, enumReg, (int8_t) (32 - shift_amount),
                     (int8_t)((63 - shift_amount) |0x80), (int8_t) shift_amount);
               }
            else
               {
               iCursor = generateRRInstruction(cg, TR::InstOpCode::getLoadTestRegWidenOpCode(), node, tmp, enumReg, iCursor);
               if (elementSize >= 2)
                  {
                  if (TR::Compiler->target.is64Bit())
                  iCursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, tmp, tmp, trailingZeroes(elementSize), iCursor);
                  else
                  iCursor = generateRSInstruction(cg, TR::InstOpCode::SLL, node, tmp, trailingZeroes(elementSize), iCursor);
                  }
               }

            static char * allocZeroArrayWithVM = feGetEnv("TR_VMALLOCZEROARRAY");
            // DualTLH: Remove when performance confirmed
            static char * useDualTLH = feGetEnv("TR_USEDUALTLH");
            // OOL
            if (!comp->getOption(TR_DisableOOL))
               {
               if (comp->getOption(TR_DisableDualTLH) && useDualTLH || allocZeroArrayWithVM == NULL)
                  {
                  iCursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, startOOLLabel, iCursor);
                  TR_Debug * debugObj = cg->getDebug();
                  zeroSizeArrayChckOOL = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(startOOLLabel,exitOOLLabel,cg);
                  cg->getS390OutOfLineCodeSectionList().push_front(zeroSizeArrayChckOOL);
                  zeroSizeArrayChckOOL->swapInstructionListsWithCompilation();
                  // Check to see if array-type is a super-class of the src object
                  //
                  TR::Instruction * cursor;
                  cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, startOOLLabel);
                  if (debugObj)
                  debugObj->addInstructionComment(cursor, "Denotes start of OOL for allocating zero size arrays");

                  /* using TR::Compiler->om.discontiguousArrayHeaderSizeInBytes() - TR::Compiler->om.contiguousArrayHeaderSizeInBytes()
                   * for byte size for discontiguous 0 size arrays because later instructions do ( + 15 & -8) to round it to object size header and adding a j9 class header
                   *
                   *
                   ----------- OOL: Beginning of out-of-line code section ---------------
                   Label [0x484BE2AC80]:    ; Denotes start of OOL for allocating zero size arrays
                   AGHI    GPR_0x484BE2A900,16
                   BRC     J(0xf), Label [0x484BE2ACE0]
                   --------------- OOL: End of out-of-line code section ------------------

                   Label [0x484BE2ACE0]:    ; Exit OOL, going back to main line
                   LR      GPR_0x484BE2AAE0,GPR_0x484BE2A7A0
                   SRA     GPR_0x484BE2AAE0,16
                   BRC     MASK6(0x6), Snippet Label [0x484BE2A530]      # (Start of internal control flow)
                   AGHI    GPR_0x484BE2A900,15 <----add 7 + 8
                   NILF    GPR_0x484BE2A900,-8 <---round to object size
                   AG      GPR_0x484BE2A900,#490 96(GPR13)

                   */
                  cursor = generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, tmp,
                        TR::Compiler->om.discontiguousArrayHeaderSizeInBytes() - TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cursor);

                  generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, exitOOLLabel,cursor);
                  zeroSizeArrayChckOOL->swapInstructionListsWithCompilation();
                  }
               else
                  {
                  iCursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, callLabel, iCursor);
                  if(!firstBRCToOOL)
                     {
                     firstBRCToOOL = iCursor;
                     }
                  else
                     {
                     secondBRCToOOL = iCursor;
                     }
                  }
               }
            else
               {
               iCursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, exitOOLLabel, iCursor);
               iCursor = generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, tmp,
                     TR::Compiler->om.discontiguousArrayHeaderSizeInBytes() - TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), iCursor);
               }

            }
         else
            {
            iCursor = generateRRInstruction(cg, TR::InstOpCode::getLoadRegWidenOpCode(), node, tmp, enumReg, iCursor);

            if (elementSize >= 2)
               {
               if (TR::Compiler->target.is64Bit())
               iCursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, tmp, tmp, trailingZeroes(elementSize), iCursor);
               else
               iCursor = generateRSInstruction(cg, TR::InstOpCode::SLL, node, tmp, trailingZeroes(elementSize), iCursor);
               }
            }

         }

      if (outlineNew && !isVariableLen)
         {
         TR::Register * tmpReg = NULL;
         if (opCode == TR::New)
            tmpReg = enumReg;
         else
            tmpReg = resReg;

         if (TR::Compiler->target.is64Bit())
            iCursor = genLoadLongConstant(cg, node, allocateSize, tmpReg, iCursor, conditions);
         else
            iCursor = generateLoad32BitConstant(cg, node, allocateSize, tmpReg, true, iCursor, conditions);
         }

      //////////////////////////////////////////////////////////////////////////////////////////////////////
      ///============================ STAGE 4: Generate HeapTop Test=====================================///
      //////////////////////////////////////////////////////////////////////////////////////////////////////
      TR::Instruction *current;
      TR::Instruction *firstInstruction;
      srm->addScratchRegistersToDependencyList(conditions);

      current = cg->getAppendInstruction();

      TR_ASSERT(current != NULL, "Could not get current instruction");

      if (outlineNew)
         {
         if (isVariableLen)
            roundArrayLengthToObjectAlignment(cg, node, iCursor, dataSizeReg, conditions, litPoolBaseReg, allocateSize, elementSize, resReg, exitOOLLabel );

         TR_RuntimeHelper helper;
         if (opCode == TR::New)
            helper = TR_S390OutlinedNew;
         else
            helper = TR_S390OutlinedNewArray;

#if !defined(PUBLIC_BUILD)
         static bool bppoutline = (feGetEnv("TR_BPRP_Outline")!=NULL);
         if (bppoutline)
            {
            TR::LabelSymbol * callLabel = generateLabelSymbol(cg);
            TR::Instruction * instr = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, callLabel);

            if (helper == TR_S390OutlinedNew && cg->_outlineCall._frequency == -1)
               {
               cg->_outlineCall._frequency = 1;
               cg->_outlineCall._callSymRef = cg->symRefTab()->findOrCreateRuntimeHelper(helper, false, false, false);
               cg->_outlineCall._callLabel = callLabel;
               }
            else if (helper == TR_S390OutlinedNewArray && cg->_outlineArrayCall._frequency == -1)
               {
               cg->_outlineArrayCall._frequency = 1;
               cg->_outlineArrayCall._callSymRef = cg->symRefTab()->findOrCreateRuntimeHelper(helper, false, false, false);
               cg->_outlineArrayCall._callLabel = callLabel;
               }
            }
#endif
         // outlineNew is disabled, so We don't use the hardcoded dependency for that
         // TODO When we decide to enable outlinedNew we need to make sure we have class and size in right register as per expectation
         iCursor = generateDirectCall(cg, node, false, cg->symRefTab()->findOrCreateRuntimeHelper(helper, false, false, false), conditions);
         //check why it fails when TR::InstOpCode::BRCL instead of TR::InstOpCode::BRC is used
         iCursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BH, node, callLabel, iCursor);
         if(!firstBRCToOOL)
            {
            firstBRCToOOL = iCursor;
            }
         else
            {
            secondBRCToOOL = iCursor;
            }

         }
      else
         {
         static char * useDualTLH = feGetEnv("TR_USEDUALTLH");
         //Here we set up backout paths if we overflow nonZeroTLH in genHeapAlloc.
         //If we overflow the nonZeroTLH, set the destination to the right VM runtime helper (eg jitNewObjectNoZeroInit, etc...)
         //The zeroed-TLH versions have their correct destinations already setup in TR_ByteCodeIlGenerator::genNew, TR_ByteCodeIlGenerator::genNewArray, TR_ByteCodeIlGenerator::genANewArray
         //TR::PPCHeapAllocSnippet retrieves this destination via node->getSymbolReference() below after genHeapAlloc.
         if(!comp->getOption(TR_DisableDualTLH) && useDualTLH && node->canSkipZeroInitialization())
            {
            if (node->getOpCodeValue() == TR::New)
            node->setSymbolReference(comp->getSymRefTab()->findOrCreateNewObjectNoZeroInitSymbolRef(comp->getMethodSymbol()));
            else if (node->getOpCodeValue() == TR::newarray)
            node->setSymbolReference(comp->getSymRefTab()->findOrCreateNewArrayNoZeroInitSymbolRef(comp->getMethodSymbol()));
            else if (node->getOpCodeValue() == TR::anewarray)
            node->setSymbolReference(comp->getSymRefTab()->findOrCreateANewArrayNoZeroInitSymbolRef(comp->getMethodSymbol()));
            }

         if (enumReg == NULL && opCode != TR::New)
            {
            enumReg = cg->allocateRegister();
            conditions->addPostCondition(enumReg, TR::RealRegister::AssignAny);
            traceMsg(comp,"enumReg = %s\n", enumReg->getRegisterName(comp));
            }
         // classReg and enumReg have to be intact still, in case we have to call the helper.
         // On return, zeroReg is set to 0, and dataSizeReg is set to the size of data area if
         // isVariableLen is true.
         genHeapAlloc(node, iCursor, isVariableLen, enumReg, resReg, zeroReg, dataSizeReg, temp1Reg, callLabel, allocateSize, elementSize, cg,
               litPoolBaseReg, conditions, firstBRCToOOL, secondBRCToOOL, exitOOLLabel);
         }

      //////////////////////////////////////////////////////////////////////////////////////////////////////
      ///============================ STAGE 5: Generate Fall-back Path ==================================///
      //////////////////////////////////////////////////////////////////////////////////////////////////////
         /* New Evaluator Optimization: Using OOL instead of snippet for heap alloc */

         /* Example of the OOL for newarray
          * Outlined Label L0048:    ; Denotes start of OOL for heap alloc
          * LHI     GPR_0120,0x5
          * ASSOCREGS
          * PRE:
          * {GPR2:GPR_0112:R} {GPR1:GPR_0120:R}
          * BRASL   GPR_0117,0x00000000
          * POST:
          * {GPR1:D_GPR_0116:R}* {GPR14:GPR_0117:R} {GPR2:&GPR_0118:R}
          * LR      &GPR_0115,&GPR_0118
          * BRC     J(0xf), Label L0049*/

      TR_Debug * debugObj = cg->getDebug();
      TR_S390OutOfLineCodeSection *heapAllocOOL = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(callLabel, cFlowRegionEnd, cg);
      cg->getS390OutOfLineCodeSectionList().push_front(heapAllocOOL);
      heapAllocOOL->swapInstructionListsWithCompilation();
      TR::Instruction * cursorHeapAlloc;
      // Generating OOL label: Outlined Label L00XX
      cursorHeapAlloc = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, callLabel);
      if (debugObj)
         debugObj->addInstructionComment(cursorHeapAlloc, "Denotes start of OOL for heap alloc");
      generateHelperCallForVMNewEvaluators(node, cg, true, resReg);
      /* Copying the return value from the temporary register to the actual register that is returned */
      /* Generating the branch to jump back to the merge label:
       * BRCL    J(0xf), Label L00YZ, labelTargetAddr=0xZZZZZZZZ*/
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cFlowRegionEnd);
      heapAllocOOL->swapInstructionListsWithCompilation();
      //////////////////////////////////////////////////////////////////////////////////////////////////////
      ///============================ STAGE 6: Initialize the new object header ==========================///
      //////////////////////////////////////////////////////////////////////////////////////////////////////
      if (isArray)
         {
         if ( comp->compileRelocatableCode() && opCode == TR::anewarray)
         genInitArrayHeader(node, iCursor, isVariableLen, classAddress, classReg, resReg, zeroReg,
               enumReg, dataSizeReg, temp1Reg, litPoolBaseReg, conditions, cg);
         else
         genInitArrayHeader(node, iCursor, isVariableLen, classAddress, NULL, resReg, zeroReg,
               enumReg, dataSizeReg, temp1Reg, litPoolBaseReg, conditions, cg);

         // Write Arraylet Pointer
         if (generateArraylets)
            {
            iCursor = generateS390ImmOp(cg, TR::InstOpCode::getAddOpCode(), node, temp1Reg, resReg, dataBegin, conditions, litPoolBaseReg);
            if(TR::Compiler->vm.heapBaseAddress() == 0)
            iCursor = generateS390ImmOp(cg, TR::InstOpCode::getAddOpCode(), node, temp1Reg, temp1Reg, -((int64_t)(TR::Compiler->vm.heapBaseAddress())), conditions, litPoolBaseReg);
            if(TR::Compiler->om.compressedReferenceShiftOffset() > 0)
            iCursor = generateRSInstruction(cg, TR::InstOpCode::SRL, node, temp1Reg, TR::Compiler->om.compressedReferenceShiftOffset(), iCursor);

            iCursor = generateRXInstruction(cg, (TR::Compiler->target.is64Bit()&& !comp->useCompressedPointers()) ? TR::InstOpCode::STG : TR::InstOpCode::ST, node, temp1Reg,
                  generateS390MemoryReference(resReg, fej9->getOffsetOfContiguousArraySizeField(), cg), iCursor);

            }
         }
      else
         {
         genInitObjectHeader(node, iCursor, classAddress, classReg , resReg, zeroReg, temp1Reg, litPoolBaseReg, conditions, cg);
         }

      TR_ASSERT((fej9->tlhHasBeenCleared() || J9JIT_TESTMODE || J9JIT_TOSS_CODE), "");

      //////////////////////////////////////////////////////////////////////////////////////////////////////
      ///============================ STAGE 6b: Prefetch after stores ===================================///
      //////////////////////////////////////////////////////////////////////////////////////////////////////
      if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z10) && cg->enableTLHPrefetching())
         {
         iCursor = generateS390MemInstruction(cg, TR::InstOpCode::PFD, node, 2, generateS390MemoryReference(resReg, 0x100, cg), iCursor);
         }

      //////////////////////////////////////////////////////////////////////////////////////////////////////
      ///============================ STAGE 7: AOT Relocation Records ===================================///
      //////////////////////////////////////////////////////////////////////////////////////////////////////
      if (comp->compileRelocatableCode() && (opCode == TR::New || opCode == TR::anewarray) )
         {
         firstInstruction = current->getNext();
         TR_RelocationRecordInformation *recordInfo =
         (TR_RelocationRecordInformation *) comp->trMemory()->allocateMemory(sizeof(TR_RelocationRecordInformation), heapAlloc);
         recordInfo->data1 = allocateSize;
         recordInfo->data2 = node->getInlinedSiteIndex();
         recordInfo->data3 = (uintptr_t) callLabel;
         recordInfo->data4 = (uintptr_t) firstInstruction;
         TR::SymbolReference * classSymRef;
         TR_ExternalRelocationTargetKind reloKind;
         TR_OpaqueClassBlock *classToValidate = classAddress;

         if (opCode == TR::New)
            {
            classSymRef = node->getFirstChild()->getSymbolReference();
            reloKind = TR_VerifyClassObjectForAlloc;
            }
         else
            {
            classSymRef = node->getSecondChild()->getSymbolReference();
            reloKind = TR_VerifyRefArrayForAlloc;
            // In AOT without SVM, we validate the class by pulling it from the constant pool which is not the array class as anewarray bytecode refers to the component class.
            // In the evaluator we directly refer to the array class.  In AOT with SVM we need to remember to validate the component class since relocation infrastructure is
            // expecting component class.
            if (comp->getOption(TR_UseSymbolValidationManager))
               classToValidate = comp->fej9()->getComponentClassFromArrayClass(classToValidate);
            }
         if (comp->getOption(TR_UseSymbolValidationManager))
            {
            TR_ASSERT_FATAL(classToValidate != NULL, "ClassToValidate Should not be NULL, clazz = %p\n", classAddress);
            recordInfo->data5 = (uintptr_t)classToValidate;
            }
         cg->addExternalRelocation(new (cg->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(firstInstruction,
                     (uint8_t *) classSymRef,
                     (uint8_t *) recordInfo,
                     reloKind, cg),
               __FILE__, __LINE__, node);

         }

      //////////////////////////////////////////////////////////////////////////////////////////////////////
      ///============================ STAGE 8: Done. Housekeeping items =================================///
      //////////////////////////////////////////////////////////////////////////////////////////////////////

      // Add these registers to the dep list if they are actually used in the evaluator body
      // We detect use by observing if the totalUseCounts on the registers increased since their first
      // instance at the top of the evaluator.
      //
      if (litPoolBaseReg!=NULL && litPoolBaseReg->getTotalUseCount()>litPoolRegTotalUse)
         {
         // reset the isUSed bit on the condition, this prevents the assertion
         // "ERROR: cannot add conditions to an used dependency, create a copy first" from firing up.
         conditions->resetIsUsed();
         if (comp->getOption(TR_DisableHeapAllocOOL))
            conditions->addPostCondition(litPoolBaseReg, TR::RealRegister::AssignAny);
         }

      if (!comp->getOption(TR_DisableHeapAllocOOL))
         {
         if (secondBRCToOOL)
            {
            TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
            TR::LabelSymbol * cFlowRegionEnd = generateLabelSymbol(cg);

            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart, firstBRCToOOL->getPrev());
            cFlowRegionStart->setStartInternalControlFlow();

            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, conditions, secondBRCToOOL);
            cFlowRegionEnd->setEndInternalControlFlow();
            }
         iCursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd);
         }
      else
         {
         // determine where internal control flow begins by looking for the first branch
         // instruction after where the label instruction would have been inserted
         TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);

         TR::Instruction *next = current->getNext();
         while(next != NULL && !next->isBranchOp())
            next = next->getNext();
         TR_ASSERT(next != NULL && next->getPrev() != NULL, "Could not find branch instruction where internal control flow begins");
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart, next->getPrev());
         cFlowRegionStart->setStartInternalControlFlow();

         iCursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, conditions);
         cFlowRegionEnd->setEndInternalControlFlow();
         }

      cg->decReferenceCount(firstChild);
      if (secondChild)
         {
         cg->decReferenceCount(secondChild);
         }
      if (litPoolBaseChild!=NULL)
         {
         cg->decReferenceCount(litPoolBaseChild);
         }

      if (classReg)
         cg->stopUsingRegister(classReg);
      if (copyClassReg)
         cg->stopUsingRegister(copyClassReg);
      if (copyEnumReg != enumReg)
         cg->stopUsingRegister(copyEnumReg);
      if (enumReg)
         cg->stopUsingRegister(enumReg);
      if (copyReg)
         cg->stopUsingRegister(copyReg);
      srm->stopUsingRegisters();
      node->setRegister(resReg);
      return resReg;
      }
   else
      {
      // The call to doInlineAllocate may return true during LocalOpts, but subsequent optimizations may prove
      // that the anewarray cannot be allocated inline (i.e. it will end up going to helper).  An example is
      // when arraysize is proven to be 0, which is considered a discontiguous array size in balanced mode GC.
      // In such cases, we need to remove the last litpool child before calling directCallEvaluator.
      if (((node->getNumChildren()==3) && ((node->getOpCodeValue()==TR::anewarray) || (node->getOpCodeValue()==TR::newarray))) ||
            ((node->getNumChildren()==2) && (node->getOpCodeValue()==TR::New)))
         {
         // Remove the last literal pool child.
         node->removeLastChild();
         }
      return generateHelperCallForVMNewEvaluators(node, cg);
      }
   }

TR::Register *
J9::Z::TreeEvaluator::VMarrayCheckEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR::Node     *object1    = node->getFirstChild();
   TR::Node     *object2    = node->getSecondChild();
   TR::Register *object1Reg = cg->evaluate(object1);
   TR::Register *object2Reg = cg->evaluate(object2);

   TR::LabelSymbol *cFlowRegionStart  = generateLabelSymbol(cg);
   TR::LabelSymbol *fallThrough  = generateLabelSymbol(cg);
   TR::LabelSymbol *snippetLabel = NULL;
   TR::Snippet     *snippet      = NULL;
   TR::Register    *tempReg      = cg->allocateRegister();
   TR::Register    *tempClassReg = cg->allocateRegister();
   TR::InstOpCode::Mnemonic loadOpcode;
   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 7, cg);


   // If the objects are the same and one of them is known to be an array, they
   // are compatible.
   //
   if (node->isArrayChkPrimitiveArray1() ||
       node->isArrayChkReferenceArray1() ||
       node->isArrayChkPrimitiveArray2() ||
       node->isArrayChkReferenceArray2())
      {
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
      cFlowRegionStart->setStartInternalControlFlow();
      generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, object1Reg, object2Reg, TR::InstOpCode::COND_BE, fallThrough, false, false);
      }

   else
      {
      // Neither object is known to be an array
      // Check that object 1 is an array. If not, throw exception.
      //
      TR::Register * class1Reg = cg->allocateRegister();
      TR::TreeEvaluator::genLoadForObjectHeadersMasked(cg, node, class1Reg, generateS390MemoryReference(object1Reg, TR::Compiler->om.offsetOfObjectVftField(), cg), NULL);

      // TODO: Can we check the value of J9AccClassRAMArray and use NILF here?
#ifdef TR_HOST_64BIT
      genLoadLongConstant(cg, node, J9AccClassRAMArray, tempReg, NULL, deps, NULL);
      generateRXInstruction(cg, TR::InstOpCode::NG, node, tempReg,
      new (cg->trHeapMemory()) TR::MemoryReference(class1Reg, offsetof(J9Class, classDepthAndFlags), cg));
#else
      generateLoad32BitConstant(cg, node, J9AccClassRAMArray, tempReg, true, NULL, deps, NULL);
      generateRXInstruction(cg, TR::InstOpCode::N, node, tempReg,
      new (cg->trHeapMemory()) TR::MemoryReference(class1Reg, offsetof(J9Class, classDepthAndFlags), cg));
#endif
      cg->stopUsingRegister(class1Reg);

      if (!snippetLabel)
         {
         snippetLabel = generateLabelSymbol(cg);
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
         cFlowRegionStart->setStartInternalControlFlow();
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZ,   node, snippetLabel);

         snippet      = new (cg->trHeapMemory()) TR::S390HelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference());
         cg->addSnippet(snippet);
         }
      else
         {
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
         cFlowRegionStart->setStartInternalControlFlow();
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZ,   node, snippetLabel);
         }
      }

   // Test equality of the object classes.
   //
   TR::TreeEvaluator::genLoadForObjectHeaders(cg, node, tempReg, generateS390MemoryReference(object1Reg, TR::Compiler->om.offsetOfObjectVftField(), cg), NULL);

#ifdef OMR_GC_COMPRESSED_POINTERS
   generateRXInstruction(cg, TR::InstOpCode::X, node, tempReg, generateS390MemoryReference(object2Reg, TR::Compiler->om.offsetOfObjectVftField(), cg));
#else
   generateRXInstruction(cg, TR::InstOpCode::getXOROpCode(), node, tempReg, generateS390MemoryReference(object2Reg, TR::Compiler->om.offsetOfObjectVftField(), cg));
#endif

   TR::TreeEvaluator::generateVFTMaskInstruction(node, tempReg, cg);

   // XOR doesn't set the proper condition codes, so test explicitly
   generateRIInstruction(cg, TR::InstOpCode::getCmpHalfWordImmOpCode(), node, tempReg, 0);

   // If either object is known to be a primitive array, we are done. Either
   // the equality test fails and we throw the exception or it succeeds and
   // we finish.
   //
   if (node->isArrayChkPrimitiveArray1() || node->isArrayChkPrimitiveArray2())
      {
      if (!snippetLabel)
         {
         snippetLabel = generateLabelSymbol(cg);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, snippetLabel);

         snippet      = new (cg->trHeapMemory()) TR::S390HelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference());
         cg->addSnippet(snippet);
         }
      else
         {
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, snippetLabel);
         }
      }

   // Otherwise, there is more testing to do. If the classes are equal we
   // are done, and branch to the fallThrough label.
   //
   else
      {
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, fallThrough);

      // If either object is not known to be a reference array type, check it
      // We already know that object1 is an array type but we may have to now
      // check object2.
      //
      if (!node->isArrayChkReferenceArray1())
         {
         TR::TreeEvaluator::genLoadForObjectHeadersMasked(cg, node, tempClassReg, generateS390MemoryReference(object1Reg, TR::Compiler->om.offsetOfObjectVftField(), cg), NULL);

         // ramclass->classDepth&flags
         generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, tempReg,
         new (cg->trHeapMemory()) TR::MemoryReference(tempClassReg, offsetof(J9Class, classDepthAndFlags), cg));

         // X = (ramclass->ClassDepthAndFlags)>>J9AccClassRAMShapeShift
         generateRSInstruction(cg, TR::InstOpCode::SRL, node, tempReg, J9AccClassRAMShapeShift);

         // X & OBJECT_HEADER_SHAPE_MASK
         generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, tempClassReg, tempClassReg);
         generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, tempClassReg, OBJECT_HEADER_SHAPE_MASK);
         generateRRInstruction(cg, TR::InstOpCode::NR, node, tempClassReg, tempReg);

         generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, tempReg, OBJECT_HEADER_SHAPE_POINTERS);

        if (!snippetLabel)
            {
            snippetLabel = generateLabelSymbol(cg);
            generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CR, node, tempReg, tempClassReg, TR::InstOpCode::COND_BNZ, snippetLabel, false, false);

            snippet      = new (cg->trHeapMemory()) TR::S390HelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference());
            cg->addSnippet(snippet);
            }
         else
            {
            generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CR, node, tempReg, tempClassReg, TR::InstOpCode::COND_BNZ, snippetLabel, false, false);
            }
         }
      if (!node->isArrayChkReferenceArray2())
         {
         // Check that object 2 is an array. If not, throw exception.
         TR::TreeEvaluator::genLoadForObjectHeadersMasked(cg, node, tempClassReg, generateS390MemoryReference(object2Reg, TR::Compiler->om.offsetOfObjectVftField(), cg), NULL);

         // TODO: Can we check the value of J9AccClassRAMArray and use NILF here?
#ifdef TR_HOST_64BIT
         {
         genLoadLongConstant(cg, node, J9AccClassRAMArray, tempReg, NULL, deps, NULL);
         generateRXInstruction(cg, TR::InstOpCode::NG, node, tempReg,
         new (cg->trHeapMemory()) TR::MemoryReference(tempClassReg, offsetof(J9Class, classDepthAndFlags), cg));
         }
#else
         {
         generateLoad32BitConstant(cg, node, J9AccClassRAMArray, tempReg, true, NULL, deps, NULL);
         generateRXInstruction(cg, TR::InstOpCode::N, node, tempReg,
         new (cg->trHeapMemory()) TR::MemoryReference(tempClassReg, offsetof(J9Class, classDepthAndFlags), cg));
         }
#endif
         if (!snippetLabel)
            {
            snippetLabel = generateLabelSymbol(cg);
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZ,   node, snippetLabel);

            snippet      = new (cg->trHeapMemory()) TR::S390HelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference());
            cg->addSnippet(snippet);
            }
         else
            {
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZ,   node, snippetLabel);
            }

         //* Test object2 is reference array
         TR::TreeEvaluator::genLoadForObjectHeadersMasked(cg, node, tempClassReg, generateS390MemoryReference(object2Reg, TR::Compiler->om.offsetOfObjectVftField(), cg), NULL);

         // ramclass->classDepth&flags
         generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, tempReg,
         new (cg->trHeapMemory()) TR::MemoryReference(tempClassReg, offsetof(J9Class, classDepthAndFlags), cg));

         // X = (ramclass->ClassDepthAndFlags)>>J9AccClassRAMShapeShift
         generateRSInstruction(cg, TR::InstOpCode::SRL, node, tempReg, J9AccClassRAMShapeShift);

         // X & OBJECT_HEADER_SHAPE_MASK
         generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, tempClassReg, tempClassReg);
         generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, tempClassReg,OBJECT_HEADER_SHAPE_MASK);
         generateRRInstruction(cg, TR::InstOpCode::NR, node, tempClassReg, tempReg);

         generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, tempReg, OBJECT_HEADER_SHAPE_POINTERS);

         generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CR, node, tempReg, tempClassReg, TR::InstOpCode::COND_BNZ, snippetLabel, false, false);
         }

      // Now both objects are known to be reference arrays, so they are
      // compatible for arraycopy.
      }

   // Now generate the fall-through label
   //
   deps->addPostCondition(object1Reg, TR::RealRegister::AssignAny);
   deps->addPostConditionIfNotAlreadyInserted(object2Reg, TR::RealRegister::AssignAny);  // 1st and 2nd object may be the same.
   deps->addPostCondition(tempReg, TR::RealRegister::AssignAny);
   deps->addPostCondition(tempClassReg, TR::RealRegister::AssignAny);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, fallThrough, deps);
   fallThrough->setEndInternalControlFlow();

   cg->stopUsingRegister(tempClassReg);
   cg->stopUsingRegister(tempReg);
   cg->decReferenceCount(object1);
   cg->decReferenceCount(object2);

   return 0;
   }



/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
static bool
inlineMathSQRT(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * targetRegister = NULL;
   static char * nosupportSQRT = feGetEnv("TR_NOINLINESQRT");

   if (NULL != nosupportSQRT)
      {
      return false;
      }


   // Calculate it for ourselves
   if (firstChild->getOpCode().isLoadConst())
      {
      union { double valD; int64_t valI; } result;
      targetRegister = cg->allocateRegister(TR_FPR);
      result.valD = sqrt(firstChild->getDouble());
      TR::S390ConstantDataSnippet * cds = cg->findOrCreate8ByteConstant(node, result.valI);
      generateRXInstruction(cg, TR::InstOpCode::LD, node, targetRegister, generateS390MemoryReference(cds, cg, 0, node));
      }
   else
      {
      TR::Register * opRegister = NULL;

      //See whether to use SQDB or SQDBR depending on how many times it is referenced
      if (firstChild->isSingleRefUnevaluated() && firstChild->getOpCodeValue() == TR::dloadi)
         {
         targetRegister = cg->allocateRegister(TR_FPR);
         generateRXEInstruction(cg, TR::InstOpCode::SQDB, node, targetRegister, generateS390MemoryReference(firstChild, cg), 0);
         }
      else
         {
         opRegister = cg->evaluate(firstChild);

         if (cg->canClobberNodesRegister(firstChild))
            {
            targetRegister = opRegister;
            }
         else
            {
            targetRegister = cg->allocateRegister(TR_FPR);
            }
         generateRRInstruction(cg, TR::InstOpCode::SQDBR, node, targetRegister, opRegister);
         }
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   return true;
   }

static bool inlineIsAssignableFrom(TR::Node *node, TR::CodeGenerator *cg)
   {
   static char *disable = feGetEnv("TR_disableInlineIsAssignableFrom");
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   if (disable)
      return false;

   TR::Node *thisClass = node->getFirstChild();
   if (thisClass->getOpCodeValue() == TR::aloadi &&
         thisClass->getFirstChild()->getOpCodeValue() == TR::loadaddr)
      {
      TR::SymbolReference *thisClassSymRef = thisClass->getFirstChild()->getSymbolReference();

      if (thisClassSymRef->isClassInterface(comp) || thisClassSymRef->isClassAbstract(comp))
         {
         return false;
         }
      }

   int32_t classDepth = -1;
   TR::Node     *javaLangClassFrom = node->getFirstChild();
   if((javaLangClassFrom->getOpCodeValue() == TR::aloadi
         && javaLangClassFrom->getSymbolReference() == comp->getSymRefTab()->findJavaLangClassFromClassSymbolRef()
         && javaLangClassFrom->getFirstChild()->getOpCodeValue() == TR::loadaddr))
      {
      TR::Node   *castClassRef =javaLangClassFrom->getFirstChild();

      TR::SymbolReference *castClassSymRef = NULL;
      if(castClassRef->getOpCode().hasSymbolReference())
         castClassSymRef= castClassRef->getSymbolReference();

      TR::StaticSymbol    *castClassSym = NULL;
      if (castClassSymRef && !castClassSymRef->isUnresolved())
         castClassSym= castClassSymRef ? castClassSymRef->getSymbol()->getStaticSymbol() : NULL;

      TR_OpaqueClassBlock * clazz = NULL;
      if (castClassSym)
         clazz = (TR_OpaqueClassBlock *) castClassSym->getStaticAddress();

      if(clazz)
         classDepth = (int32_t)TR::Compiler->cls.classDepthOf(clazz);
      }

   TR::Register        *returnRegister = NULL;
   TR::SymbolReference *symRef     = node->getSymbolReference();
   TR::MethodSymbol    *callSymbol = symRef->getSymbol()->castToMethodSymbol();

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
//   startLabel->setStartInternalControlFlow();
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *failLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *outlinedCallLabel = generateLabelSymbol(cg);
 //  doneLabel->setEndInternalControlFlow();

   TR::Register *thisClassReg = cg->evaluate(node->getFirstChild());
   TR::Register *checkClassReg = cg->evaluate(node->getSecondChild());

   TR::RegisterDependencyConditions * deps = NULL;


   TR::Register *tempReg = cg->allocateRegister();
   TR::Register *objClassReg, *castClassReg, *scratch1Reg,*scratch2Reg;
   int8_t numOfPostDepConditions = (thisClassReg == checkClassReg)? 2 : 3;


   if (classDepth != -1)
      {
      deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, numOfPostDepConditions+4, cg);
      objClassReg = cg->allocateRegister();
      castClassReg = cg->allocateRegister();
      scratch1Reg = cg->allocateRegister();
      scratch2Reg = cg->allocateRegister();
      deps->addPostCondition(scratch1Reg, TR::RealRegister::AssignAny);
      deps->addPostCondition(scratch2Reg, TR::RealRegister::AssignAny);
      deps->addPostCondition(castClassReg, TR::RealRegister::AssignAny);
      deps->addPostCondition(objClassReg, TR::RealRegister::AssignAny);

      }
   else
      {
      deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, numOfPostDepConditions, cg);
      objClassReg = tempReg;
      }

   deps->addPostCondition(thisClassReg, TR::RealRegister::AssignAny);
   if (thisClassReg != checkClassReg)
     {
     deps->addPostCondition(checkClassReg, TR::RealRegister::AssignAny);
     }
   deps->addPostCondition(tempReg, TR::RealRegister::AssignAny);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, startLabel);

   genNullTest(cg, node, thisClassReg, thisClassReg, NULL);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, outlinedCallLabel);
   genNullTest(cg, node, checkClassReg, checkClassReg, NULL);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, outlinedCallLabel);

   generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, objClassReg,
                generateS390MemoryReference(checkClassReg, fej9->getOffsetOfClassFromJavaLangClassField(), cg));

   generateRXInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, objClassReg,
         generateS390MemoryReference(thisClassReg, fej9->getOffsetOfClassFromJavaLangClassField(), cg));

   generateRIInstruction(cg, TR::InstOpCode::LHI, node, tempReg, 1);

   TR_Debug * debugObj = cg->getDebug();
   if (classDepth != -1)
      {
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, doneLabel);
      generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, castClassReg,
                                                generateS390MemoryReference(thisClassReg, fej9->getOffsetOfClassFromJavaLangClassField(), cg));

      genTestIsSuper(cg, node, objClassReg, castClassReg, scratch1Reg, scratch2Reg, tempReg, NULL, classDepth, failLabel, doneLabel, NULL, deps, NULL, false, NULL, NULL);

      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, doneLabel);
      }
   else
      {
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, outlinedCallLabel);
      }


   TR_S390OutOfLineCodeSection *outlinedHelperCall = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(node, TR::icall, tempReg, outlinedCallLabel, doneLabel, cg);
   cg->getS390OutOfLineCodeSectionList().push_front(outlinedHelperCall);
   outlinedHelperCall->generateS390OutOfLineCodeSectionDispatch();


   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());

   node->setRegister(tempReg);

   if (classDepth != -1)
      {
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, failLabel, deps);
      generateRIInstruction(cg, TR::InstOpCode::LHI, node, tempReg, 0);

      cg->stopUsingRegister(objClassReg);
      cg->stopUsingRegister(castClassReg);
      cg->stopUsingRegister(scratch1Reg);
      cg->stopUsingRegister(scratch2Reg);
      }
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneLabel, deps);

   return true;
   }


bool
J9::Z::TreeEvaluator::VMinlineCallEvaluator(TR::Node * node, bool indirect, TR::CodeGenerator * cg)
   {
   TR::ResolvedMethodSymbol * methodSymbol = node->getSymbol()->getResolvedMethodSymbol();

   if (!methodSymbol)
      {
      return false;
      }


   bool callWasInlined = false;
   if (methodSymbol)
      {
      switch (methodSymbol->getRecognizedMethod())
         {
         case TR::java_lang_StrictMath_sqrt:
         case TR::java_lang_Math_sqrt:
            {
            callWasInlined = inlineMathSQRT(node, cg);
            break;
            }
         case TR::java_lang_Class_isAssignableFrom:
            {
            callWasInlined = inlineIsAssignableFrom(node, cg);
            break;
            }
         }
      }

   return callWasInlined;
   }

void
J9::Z::TreeEvaluator::genGuardedLoadOOL(TR::Node *node, TR::CodeGenerator *cg,
                                        TR::Register *byteSrcReg, TR::Register *byteDstReg,
                                        TR::Register *byteLenReg, TR::LabelSymbol *mergeLabel,
                                        TR_S390ScratchRegisterManager *srm, bool isForward)
   {
   TR::LabelSymbol* slowPathLabel = generateLabelSymbol(cg);
   TR::Register *vmReg = cg->getMethodMetaDataRealRegister();
   auto baseMemRef = generateS390MemoryReference(vmReg, TR::Compiler->vm.thisThreadGetEvacuateBaseAddressOffset(cg->comp()), cg);
   generateSILInstruction(cg, cg->comp()->useCompressedPointers() ? TR::InstOpCode::CHSI : TR::InstOpCode::CGHSI, node, baseMemRef, -1);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, slowPathLabel);

   TR_S390OutOfLineCodeSection* outOfLineCodeSection = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(slowPathLabel, mergeLabel, cg);
   cg->getS390OutOfLineCodeSectionList().push_front(outOfLineCodeSection);
   outOfLineCodeSection->swapInstructionListsWithCompilation();
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, slowPathLabel);

   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(cg->comp(), "readBar/arraycopy/OOL"), 1, TR::DebugCounter::Cheap);

   // Call to generateMemToMemElementCopy generates core Array Copy sequence and identify starting instruction in ICF.
   TR::RegisterDependencyConditions *loopDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 10, cg);
   TR::TreeEvaluator::generateMemToMemElementCopy(node, cg, byteSrcReg, byteDstReg, byteLenReg, srm, isForward, true, false, loopDeps);

   TR::LabelSymbol *doneOOLLabel = generateLabelSymbol(cg);
   loopDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(loopDeps, 0, 3+srm->numAvailableRegisters(), cg);
   loopDeps->addPostCondition(byteSrcReg, TR::RealRegister::AssignAny);
   loopDeps->addPostCondition(byteDstReg, TR::RealRegister::AssignAny);
   loopDeps->addPostCondition(byteLenReg, TR::RealRegister::AssignAny);
   srm->addScratchRegistersToDependencyList(loopDeps);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneOOLLabel, loopDeps);
   doneOOLLabel->setEndInternalControlFlow();

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, mergeLabel);
   outOfLineCodeSection->swapInstructionListsWithCompilation();
   }

void
J9::Z::TreeEvaluator::genArrayCopyWithArrayStoreCHK(TR::Node* node,
                                                    TR::Register *srcObjReg,
                                                    TR::Register *dstObjReg,
                                                    TR::Register *srcAddrReg,
                                                    TR::Register *dstAddrReg,
                                                    TR::Register *lengthReg,
                                                    TR::CodeGenerator *cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());

   TR::RegisterDependencyConditions * deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(9, 9, cg);
   TR::LabelSymbol * doneLabel = generateLabelSymbol(cg);
   TR::LabelSymbol * callLabel = generateLabelSymbol(cg);
   TR::LabelSymbol * OKLabel   = generateLabelSymbol(cg);
   TR::Linkage * linkage = cg->getLinkage(node->getSymbol()->castToMethodSymbol()->getLinkageConvention());
   TR::SystemLinkage *sysLink = (TR::SystemLinkage *) cg->getLinkage(TR_System);

   TR::RealRegister *sspRegReal = sysLink->getStackPointerRealRegister();
   TR::Register *sspReg;

   TR::Compilation *comp = cg->comp();

   if (sspRegReal->getState() == TR::RealRegister::Locked)
      {
      sspReg = sspRegReal;
      }
   else
      {
      sspReg = cg->allocateRegister();
      }

   TR::Register *helperReg = cg->allocateRegister();
   int32_t  offset  = sysLink->getOffsetToFirstParm();
   int32_t  ptrSize = (int32_t)TR::Compiler->om.sizeofReferenceAddress();

   // Set the following parms in C parm area
   // 1) VM Thread
   // 2) srcObj
   // 3) dstObj
   // 4) srcAddr
   // 5) dstAddr
   // 6) num of slots
   // 7) VM referenceArrayCopy func desc
   TR::Register *metaReg = cg->getMethodMetaDataRealRegister();

   if (sspRegReal->getState() != TR::RealRegister::Locked)
      {
       deps->addPreCondition(sspReg, TR::RealRegister::GPR4);
       deps->addPostCondition(sspReg, TR::RealRegister::GPR4);
      }
   if (cg->supportsJITFreeSystemStackPointer())
      {
      generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, sspReg,
         generateS390MemoryReference(metaReg, (int32_t)(fej9->thisThreadGetSystemSPOffset()), cg));
      generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, helperReg, 0);
      generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, helperReg,
         generateS390MemoryReference(metaReg, (int32_t)(fej9->thisThreadGetSystemSPOffset()), cg));
      }

   // Ready parameter 5: count reg
   TR::Register *countReg  = cg->allocateRegister();
   generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, countReg, lengthReg);
   generateRSInstruction(cg, TR::InstOpCode::SRL, node,  countReg, trailingZeroes(TR::Compiler->om.sizeofReferenceField()));

   // Ready parameter 6: helper reg
   intptrj_t *funcdescrptr = (intptrj_t*) fej9->getReferenceArrayCopyHelperAddress();
   if (comp->compileRelocatableCode())
      {
      generateRegLitRefInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, helperReg, (intptrj_t)funcdescrptr, TR_ArrayCopyHelper, NULL, NULL, NULL);
      }
   else
      {
      genLoadAddressConstant(cg, node, (long) funcdescrptr, helperReg);
      }

   // Store 7 parameters
   generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, metaReg,
         generateS390MemoryReference(sspReg, offset+0*ptrSize, cg));
   generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, srcObjReg,
         generateS390MemoryReference(sspReg, offset+1*ptrSize, cg));
   generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, dstObjReg,
         generateS390MemoryReference(sspReg, offset+2*ptrSize, cg));
   generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, srcAddrReg,
         generateS390MemoryReference(sspReg, offset+3*ptrSize, cg));
   generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, dstAddrReg,
         generateS390MemoryReference(sspReg, offset+4*ptrSize, cg));
   generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, countReg,
         generateS390MemoryReference(sspReg, offset+5*ptrSize, cg));
   generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, helperReg,
         generateS390MemoryReference(sspReg, offset+6*ptrSize, cg));

   cg->stopUsingRegister(countReg);
   cg->stopUsingRegister(helperReg);

   TR::Register *rcReg     = cg->allocateRegister();
   TR::Register *raReg     = cg->allocateRegister();
   TR::Register *tmpReg    = cg->allocateRegister();
   TR::Register *R2SaveReg = cg->allocateRegister();

   TR::SymbolReference* helperCallSymRef = cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390referenceArrayCopyHelper, false, false, false);
   TR::Snippet * helperCallSnippet = new (cg->trHeapMemory()) TR::S390HelperCallSnippet(cg, node, callLabel,
                                                                              helperCallSymRef, doneLabel);
   cg->addSnippet(helperCallSnippet);

// The snippet kill r14 and may kill r15, the rc is in r2
   deps->addPostCondition(rcReg,  linkage->getIntegerReturnRegister());
   deps->addPostCondition(raReg,  linkage->getReturnAddressRegister());
   deps->addPostCondition(tmpReg, linkage->getEntryPointRegister());

   TR::Instruction *gcPoint =
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, callLabel);
   gcPoint->setNeedsGCMap(0xFFFFFFFF);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneLabel);

   if (cg->supportsJITFreeSystemStackPointer())
      {
      generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, sspReg,
      generateS390MemoryReference(metaReg, (int32_t)(fej9->thisThreadGetSystemSPOffset()), cg));
      }

   if (sspRegReal->getState() != TR::RealRegister::Locked)
      {
      cg->stopUsingRegister(sspReg);
      }

   generateRIInstruction(cg, TR::InstOpCode::getCmpHalfWordImmOpCode(), node, rcReg, 65535);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, OKLabel);

   // raise exceptions
   TR::SymbolReference *throwSymRef = comp->getSymRefTab()->findOrCreateArrayStoreExceptionSymbolRef(comp->getJittedMethodSymbol());
   TR::LabelSymbol *exceptionSnippetLabel = cg->lookUpSnippet(TR::Snippet::IsHelperCall, throwSymRef);
   if (exceptionSnippetLabel == NULL)
      {
      exceptionSnippetLabel = generateLabelSymbol(cg);
      cg->addSnippet(new (cg->trHeapMemory()) TR::S390HelperCallSnippet(cg, node, exceptionSnippetLabel, throwSymRef));
      }

   gcPoint = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, exceptionSnippetLabel);
   gcPoint->setNeedsGCMap(0xFFFFFFFF);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, OKLabel, deps);

   cg->stopUsingRegister(raReg);
   cg->stopUsingRegister(tmpReg);
   cg->stopUsingRegister(rcReg);
   cg->stopUsingRegister(R2SaveReg);

   return;
   }

void
J9::Z::TreeEvaluator::restoreGPR7(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::MemoryReference * tempMR = generateS390MemoryReference(cg->getMethodMetaDataRealRegister(), offsetof(J9VMThread, tempSlot), cg);
   TR::Register * tempReg = cg->allocateRegister();
   generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node,   cg->machine()->getRealRegister(TR::RealRegister::GPR7), tempMR);
   }

void J9::Z::TreeEvaluator::genWrtbarForArrayCopy(TR::Node *node, TR::Register *srcReg, TR::Register *dstReg, bool srcNonNull, TR::CodeGenerator *cg)
   {
   TR::Instruction * cursor;
   TR::RegisterDependencyConditions * conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg);
   TR::Compilation * comp = cg->comp();

   auto gcMode = TR::Compiler->om.writeBarrierType();
   bool doWrtBar = (gcMode == gc_modron_wrtbar_oldcheck || gcMode == gc_modron_wrtbar_cardmark_and_oldcheck || gcMode == gc_modron_wrtbar_always);
   // Do not do card marking when gcMode is gc_modron_wrtbar_cardmark_and_oldcheck - we go through helper, which performs CM, so it is redundant.
   bool doCrdMrk = (gcMode == gc_modron_wrtbar_cardmark || gcMode == gc_modron_wrtbar_cardmark_incremental);
   TR::LabelSymbol * doneLabel = generateLabelSymbol(cg);

   if (doWrtBar)
      {
      TR::Register * tempReg = cg->allocateRegister();
      TR::Register * tempReg2 = cg->allocateRegister();
      TR::SymbolReference * wbref = comp->getSymRefTab()->findOrCreateWriteBarrierBatchStoreSymbolRef(comp->getMethodSymbol());

      TR::Register * srcObjReg = srcReg;
      TR::Register * dstObjReg;
      // It's possible to have srcReg and dstReg point to same array
      // If so, we need to copy before calling helper
      if (srcReg == dstReg){
         dstObjReg = cg->allocateRegister();
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, dstObjReg, dstReg);
      }
      else
         dstObjReg = dstReg;

      conditions->addPostCondition(tempReg, cg->getReturnAddressRegister());
      conditions->addPostCondition(tempReg2, cg->getEntryPointRegister());
      conditions->addPostCondition(dstObjReg, TR::RealRegister::GPR1);

      /*** Start of VMnonNullSrcWrtBarEvaluator ***********************/
      // 83613: If this condition changes, please verify that the inline CM
      // conditions are still correct.  Currently, we don't perform inline CM
      // for old&CM objects, since this wrtbarEvaluator will call the helper,which
      // also performs CM.

      // check for old space or color black (fej9->getWriteBarrierGCFlagMaskAsByte())
      //
      //  object layout
      //   -------------
      //  |class_pointer|
      //   -------------
      //  |*****    flag|
      //   -------------
      //   .....
      //
      // flag is in the lower 2 bytes in a 8 byte slot on 64 bit obj.(4 byte slot in 32bit obj)
      // so the offset should be ...

      if (gcMode != gc_modron_wrtbar_always)
         {
         bool is64Bit = TR::Compiler->target.is64Bit();
         bool isConstantHeapBase = !comp->getOptions()->isVariableHeapBaseForBarrierRange0();
         bool isConstantHeapSize = !comp->getOptions()->isVariableHeapSizeForBarrierRange0();
         TR::Register * temp1Reg = cg->allocateRegister();

         conditions->addPostCondition(temp1Reg, TR::RealRegister::AssignAny);

         TR::MemoryReference * offset = generateS390MemoryReference(cg->getMethodMetaDataRealRegister(), offsetof(J9VMThread, heapBaseForBarrierRange0), cg);
         TR::MemoryReference * size = generateS390MemoryReference(cg->getMethodMetaDataRealRegister(), offsetof(J9VMThread, heapSizeForBarrierRange0), cg);
         generateRRInstruction(cg, is64Bit ? TR::InstOpCode::LGR : TR::InstOpCode::LR, node, temp1Reg, dstObjReg);
         generateRXInstruction(cg, is64Bit ? TR::InstOpCode::SG : TR::InstOpCode::S, node, temp1Reg, offset);
         generateRXInstruction(cg, is64Bit ? TR::InstOpCode::CLG : TR::InstOpCode::CL, node, temp1Reg, size);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BH, node, doneLabel);
         cg->stopUsingRegister(temp1Reg);
         // if not match, callout to the helper
         }

      generateDirectCall(cg, node, false, wbref, conditions);
      /*** End Of *****************************************************/
      cg->stopUsingRegister(tempReg);
      cg->stopUsingRegister(tempReg2);
      if (srcReg == dstReg)
         cg->stopUsingRegister(dstObjReg);
      }

   else if (doCrdMrk)
      {
      if (!TR::Options::getCmdLineOptions()->realTimeGC())
         {
         TR::Register * temp1Reg = cg->allocateRegister();
         conditions->addPostCondition(temp1Reg, TR::RealRegister::AssignAny);
         conditions->addPostCondition(dstReg, TR::RealRegister::AssignAny);
         VMCardCheckEvaluator(node, dstReg, temp1Reg, conditions, cg, false, doneLabel);
         cg->stopUsingRegister(temp1Reg);
         }
      else
         TR_ASSERT(0, "genWrtbarForArrayCopy card marking not supported for RT");
      }
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneLabel, conditions);
   }

extern TR::Register *
VMinlineCompareAndSwap(TR::Node *node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic casOp, bool isObj)
   {
   TR::Register *scratchReg = NULL;
   TR::Register *objReg, *oldVReg, *newVReg;
   TR::Register *resultReg = cg->allocateRegister();
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
   TR::MemoryReference* casMemRef = NULL;

   TR::Compilation * comp = cg->comp();

   TR::Node* thisNode   = node->getChild(0);
   TR::Node* objNode    = node->getChild(1);
   TR::Node* offsetNode = node->getChild(2);
   TR::Node* oldVNode   = node->getChild(3);
   TR::Node* newVNode   = node->getChild(4);

   // The card mark write barrier helper expects the source register to be a decompressed reference. As such if the
   // value we are storing (last argument) has been lowered we must extract the decompressed reference from the
   // compression sequence.
   bool isValueCompressedReference = false;

   TR::Node* decompressedValueNode = newVNode;

   if (isObj && comp->useCompressedPointers() && decompressedValueNode->getOpCodeValue() == TR::l2i)
      {
      // Pattern match the sequence:
      //
      //  <node>
      //    <thisNode>
      //    <objNode>
      //    <offsetNode>
      //    <oldVNode>
      //    l2i
      //      lushr
      //        a2l
      //          <decompressedValueNode>
      //        iconst

      if (decompressedValueNode->getOpCode().isConversion())
         {
         decompressedValueNode = decompressedValueNode->getFirstChild();
         }

      if (decompressedValueNode->getOpCode().isRightShift())
         {
         decompressedValueNode = decompressedValueNode->getFirstChild();
         }

      if (decompressedValueNode->getOpCode().isSub() || TR::Compiler->vm.heapBaseAddress() == 0 || newVNode->isNull())
         {
         isValueCompressedReference = true;
         }

      if (isValueCompressedReference)
         {
         while ((decompressedValueNode->getNumChildren() > 0) && (decompressedValueNode->getOpCodeValue() != TR::a2l))
            {
            decompressedValueNode = decompressedValueNode->getFirstChild();
            }

         if (decompressedValueNode->getOpCodeValue() == TR::a2l)
            {
            decompressedValueNode = decompressedValueNode->getFirstChild();
            }

         // Artificially bump the reference count on the value so that different registers are allocated for the
         // compressed and decompressed values. This is done so that the card mark write barrier helper uses the
         // decompressed value.
         decompressedValueNode->incReferenceCount();
         }
      }

   // Eval old and new vals
   //
   objReg  = cg->evaluate(objNode);
   oldVReg = cg->gprClobberEvaluate(oldVNode);  //  CS oldReg, newReg, OFF(objReg)
   newVReg = cg->evaluate(newVNode);                    //    oldReg is clobbered

   TR::Register* compressedValueRegister = newVReg;

   if (isValueCompressedReference)
      {
      compressedValueRegister = cg->evaluate(decompressedValueNode);
      }

   bool needsDup = false;

   if (objReg == newVReg)
      {
      // Make a copy of the register - reg deps later on expect them in different registers.
      newVReg = cg->allocateCollectedReferenceRegister();
      generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, newVReg, objReg);
      if (!isValueCompressedReference)
         compressedValueRegister = newVReg;

      needsDup = true;
      }

   generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, resultReg, 0x0);

   //  We can run into trouble when the offset value gets too big, or it may
   //  simply not nbe known at compile time.
   //
   if (offsetNode->getOpCode().isLoadConst() && offsetNode->getRegister()==NULL)
      {
      // We know at compile time
      intptr_t offsetValue = offsetNode->getLongInt();
      if (offsetValue>=0 && offsetValue<MAXDISP)
         {
         casMemRef = generateS390MemoryReference(objReg, offsetValue, cg);
         }
         //  ADD Golden Eagle support here if we ever see this path take (unlikely)
      }

   //  We couldn't figure out how to get the offset into the DISP field of the CAS inst
   //  So use an explicit local ADD
   //
   if (casMemRef == NULL)  // Not setup, hence we need a reg
      {
      scratchReg = cg->gprClobberEvaluate(offsetNode);

      generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, scratchReg,objReg);
      casMemRef = generateS390MemoryReference(scratchReg, 0, cg);
      }

   if (TR::Compiler->om.readBarrierType() != gc_modron_readbar_none && isObj)
      {
      TR::Register* tempReadBarrier = cg->allocateRegister();
      if (TR::Compiler->target.cpu.getSupportsGuardedStorageFacility())
         {
         auto guardedLoadMnemonic = comp->useCompressedPointers() ? TR::InstOpCode::LLGFSG : TR::InstOpCode::LGG;

         // Compare-And-Swap on object reference, while primarily is a store operation, it is also an implicit read (it
         // reads the existing value to be compared with a provided compare value, before the store itself), hence needs
         // a read barrier
         generateS390IEInstruction(cg, TR::InstOpCode::NIAI, 1, 0, node);
         generateRXInstruction(cg, guardedLoadMnemonic, node, tempReadBarrier, generateS390MemoryReference(*casMemRef, 0, cg));
         }
      else
         {
         TR::TreeEvaluator::generateSoftwareReadBarrier(node, cg, tempReadBarrier, generateS390MemoryReference(*casMemRef, 0, cg));
         }
      cg->stopUsingRegister(tempReadBarrier);
      }

   // Compare and swap
   //
   generateRSInstruction(cg, casOp, node, oldVReg, newVReg, casMemRef);

   //  Setup return
   //
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, doneLabel);

   generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, resultReg, 0x1);

   TR::RegisterDependencyConditions* cond = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 1, cg);
   cond->addPostCondition(resultReg, TR::RealRegister::AssignAny);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneLabel, cond);

   // Do wrtbar for Objects
   //
   auto gcMode = TR::Compiler->om.writeBarrierType();
   bool doWrtBar = (gcMode == gc_modron_wrtbar_oldcheck || gcMode == gc_modron_wrtbar_cardmark_and_oldcheck ||
                    gcMode == gc_modron_wrtbar_always);
   bool doCrdMrk = (gcMode == gc_modron_wrtbar_cardmark || gcMode == gc_modron_wrtbar_cardmark_and_oldcheck ||
                    gcMode == gc_modron_wrtbar_cardmark_incremental);

   if (isObj && (doWrtBar || doCrdMrk))
      {
      TR::LabelSymbol *doneLabelWrtBar = generateLabelSymbol(cg);
      TR::Register *epReg = cg->allocateRegister();
      TR::Register *raReg = cg->allocateRegister();
      TR::RegisterDependencyConditions* condWrtBar = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 5, cg);
      condWrtBar->addPostCondition(objReg, TR::RealRegister::GPR1);
      if (compressedValueRegister != newVReg)
         condWrtBar->addPostCondition(newVReg, TR::RealRegister::AssignAny); //defect 92001
      if (compressedValueRegister != objReg)  // add this because I got conflicting dependencies on GPR1 and GPR2!
         condWrtBar->addPostCondition(compressedValueRegister, TR::RealRegister::GPR2); //defect 92001
      condWrtBar->addPostCondition(epReg, cg->getEntryPointRegister());
      condWrtBar->addPostCondition(raReg, cg->getReturnAddressRegister());
      // Cardmarking is not inlined for gencon. Consider doing so when perf issue arises.
      if (doWrtBar)
         {
         TR::SymbolReference *wbRef;
         auto gcMode = TR::Compiler->om.writeBarrierType();

         if (gcMode == gc_modron_wrtbar_cardmark_and_oldcheck || gcMode == gc_modron_wrtbar_oldcheck)
            wbRef = comp->getSymRefTab()->findOrCreateWriteBarrierStoreGenerationalSymbolRef(comp->getMethodSymbol());
         else
            wbRef = comp->getSymRefTab()->findOrCreateWriteBarrierStoreSymbolRef(comp->getMethodSymbol());
         VMnonNullSrcWrtBarCardCheckEvaluator(node, objReg, compressedValueRegister, epReg, raReg, doneLabelWrtBar, wbRef, condWrtBar, cg, false);
         }

      else if (doCrdMrk)
         {
         VMCardCheckEvaluator(node, objReg, epReg, condWrtBar, cg, false, doneLabelWrtBar, false);
                                                                           // true #1 -> copy of objReg just happened, it's safe to clobber tempReg
                                                                           // false #2 -> Don't do compile time check for heap obj
         }

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneLabelWrtBar, condWrtBar);

      cg->stopUsingRegister(epReg);
      cg->stopUsingRegister(raReg);
      }

   // Value is not used, and not eval'd to avoid the extra reg
   //  So recursively decrement to compensate
   //
   cg->recursivelyDecReferenceCount(thisNode);

   cg->decReferenceCount(objNode);
   cg->decReferenceCount(offsetNode);
   cg->decReferenceCount(oldVNode);
   cg->decReferenceCount(newVNode);

   cg->stopUsingRegister(oldVReg);

   if (needsDup)
      {
      cg->stopUsingRegister(newVReg);
      }
   if (scratchReg)
      {
      cg->stopUsingRegister(scratchReg);
      }

   if (isValueCompressedReference)
      cg->decReferenceCount(decompressedValueNode);

   node->setRegister(resultReg);
   return resultReg;
   }


/////////////////////////////////////////////////////////////////////////////////
//  getTOCOffset()
//     return codertTOC offset from vmThread (R13)
////////////////////////////////////////////////////////////////////////////////
int
getTOCOffset()
   {
   return (offsetof(J9VMThread, codertTOC));
   }

TR::Instruction *
J9::Z::TreeEvaluator::generateVFTMaskInstruction(TR::Node *node, TR::Register *reg, TR::CodeGenerator *cg, TR::Instruction *preced)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR::Instruction *result = preced;
   uintptrj_t mask = TR::Compiler->om.maskOfObjectVftField();
   if (~mask == 0)
      {
      // no mask instruction required
      }
   else if (~mask <= 0xffff)
      {
      result = generateRIInstruction(cg, TR::InstOpCode::NILL, node, reg, mask, preced);
      }
   else
      {
      TR_ASSERT(0, "Can't mask out flag bits beyond the low 16 from the VFT pointer");
      }
   return result;
   }

// This routine generates RION and RIOFF guarded by VMThread->jitCurrentRIFlags
// based on test for bit: J9_JIT_TOGGLE_RI_IN_COMPILED_CODE
TR::Instruction *
J9::Z::TreeEvaluator::generateRuntimeInstrumentationOnOffSequence(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node, TR::Instruction *preced, bool postRA)
   {
   TR::Compilation *comp = cg->comp();
   TR_ASSERT(op == TR::InstOpCode::RION || op == TR::InstOpCode::RIOFF, "Unexpected Runtime Instrumentation OpCode");

#ifdef TR_HOST_S390
   TR::LabelSymbol * OOLStartLabel = generateLabelSymbol(cg);
   TR::LabelSymbol * OOLReturnLabel = generateLabelSymbol(cg);
   TR_Debug * debugObj = cg->getDebug();

   // Test the last byte of vmThread->jitCurrentRIFlags
   TR_ASSERT(0 != (J9_JIT_TOGGLE_RI_IN_COMPILED_CODE & 0xFF), "Cannot use TM to test for J9_JIT_TOGGLE_RI_IN_COMPILED_CODE");
   TR::MemoryReference *vmThreadMemRef = generateS390MemoryReference(cg->getMethodMetaDataRealRegister(), offsetof(J9VMThread, jitCurrentRIFlags) + sizeof(((J9VMThread *)0)->jitCurrentRIFlags) - 1, cg);
   preced = generateSIInstruction(cg, TR::InstOpCode::TM, node, vmThreadMemRef, J9_JIT_TOGGLE_RI_IN_COMPILED_CODE, preced);
   preced = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK7, node, OOLStartLabel, preced);

   if (debugObj)
      if (op == TR::InstOpCode::RION)
         debugObj->addInstructionComment(preced, "-->OOL RION");
      else
         debugObj->addInstructionComment(preced, "-->OOL RIOFF");


   TR_S390OutOfLineCodeSection *RIOnOffOOL = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(OOLStartLabel, OOLReturnLabel, cg);
   cg->getS390OutOfLineCodeSectionList().push_front(RIOnOffOOL);
   RIOnOffOOL->swapInstructionListsWithCompilation();

   TR::Instruction * cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, OOLStartLabel);
   if (debugObj)
      debugObj->addInstructionComment(cursor, "OOL RION/OFF seq");

   // Generate the RION/RIOFF instruction.
   cursor = generateRuntimeInstrumentationInstruction(cg, op, node, NULL, cursor);

   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, OOLReturnLabel, cursor);

   RIOnOffOOL->swapInstructionListsWithCompilation();

   preced = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, OOLReturnLabel, preced);

   // OOL's are appended to the instruction stream during RA.  If this is
   // emitted postRA, we have to attach it ourselves.
   if (postRA)
      {
      TR::Instruction *appendInstruction = cg->getAppendInstruction();
      appendInstruction->setNext(RIOnOffOOL->getFirstInstruction());
      RIOnOffOOL->getFirstInstruction()->setPrev(appendInstruction);
      cg->setAppendInstruction(RIOnOffOOL->getAppendInstruction());
      }
#endif /* TR_HOST_S390 */
   return preced;
   }


#if defined(TR_HOST_S390) && defined(J9ZOS390)
// psuedo-call to asm function
extern "C" void _getSTCKLSOOffset(int32_t* offsetArray);  /* 390 asm stub */
#endif

/**
 * generate a single precision sqrt instruction
 */
extern TR::Register * inlineSinglePrecisionSQRT(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * targetRegister = NULL;
   TR::Register * opRegister = cg->evaluate(firstChild);

   if (cg->canClobberNodesRegister(firstChild))
      {
      targetRegister = opRegister;
      }
   else
      {
      targetRegister = cg->allocateRegister(TR_FPR);
      }
   generateRRInstruction(cg, TR::InstOpCode::SQEBR, node, targetRegister, opRegister);
   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   return node->getRegister();
   }

/** \brief
 *     Evaluates a sequence of instructions which generate the current time in terms of 1/2048 of micro-seconds.
 *
 *  \param cg
 *     The code generator used to generate the instructions.
 *
 *  \param node
 *     The node with which to associate the generated instructions with.
 *
 *  \return
 *     A register (or register pair for 31-bit) containing the current time in terms of 1/2048 of micro-seconds.
 */
extern TR::Register* inlineCurrentTimeMaxPrecision(TR::CodeGenerator* cg, TR::Node* node)
   {
   // STCKF is an S instruction and requires a 64-bit memory reference
   TR::SymbolReference* reusableTempSlot = cg->allocateReusableTempSlot();

   generateSInstruction(cg, TR::InstOpCode::STCKF, node, generateS390MemoryReference(node, reusableTempSlot, cg));

   // Dynamic literal pool could have assigned us a literal base register
   TR::Register* literalBaseRegister = (node->getNumChildren() == 1) ? cg->evaluate(node->getFirstChild()) : NULL;

   TR::Register* targetRegister = cg->allocateRegister();

#if defined(TR_HOST_S390) && defined(J9ZOS390)
      int32_t offsets[3];
      _getSTCKLSOOffset(offsets);

      TR::Register* tempRegister = cg->allocateRegister();

      // z/OS requires time correction to account for leap seconds. The number of leap seconds is stored in the LSO
      // field of the MVS data area.
      if (TR::Compiler->target.isZOS())
         {
         // Load FFCVT(R0)
         generateRXInstruction(cg, TR::InstOpCode::LLGT, node, tempRegister, generateS390MemoryReference(offsets[0], cg));

         // Load CVTEXT2 - CVT
         generateRXInstruction(cg, TR::InstOpCode::LLGT, node, tempRegister, generateS390MemoryReference(tempRegister, offsets[1], cg));
         }
#endif

      generateRXInstruction(cg, TR::InstOpCode::LG, node, targetRegister, generateS390MemoryReference(node, reusableTempSlot, cg));

      int64_t todJanuary1970 = 0x7D91048BCA000000LL;
      generateRegLitRefInstruction(cg, TR::InstOpCode::SLG, node, targetRegister, todJanuary1970, NULL, NULL, literalBaseRegister);

#if defined(TR_HOST_S390) && defined(J9ZOS390)
      if (TR::Compiler->target.isZOS())
         {
         // Subtract the LSO offset
         generateRXInstruction(cg, TR::InstOpCode::SLG, node, targetRegister, generateS390MemoryReference(tempRegister, offsets[2],cg));
         }

      cg->stopUsingRegister(tempRegister);
#endif

      // Get current time in terms of 1/2048 of micro-seconds
      generateRSInstruction(cg, TR::InstOpCode::SRLG, node, targetRegister, targetRegister, 1);

   cg->freeReusableTempSlot();

   if (literalBaseRegister != NULL)
      {
      cg->decReferenceCount(node->getFirstChild());
      }

   node->setRegister(targetRegister);

   return targetRegister;
   }

extern TR::Register *inlineAtomicOps(
      TR::Node *node,
      TR::CodeGenerator *cg,
      int8_t size,
      TR::MethodSymbol *method,
      bool isArray = false)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   TR::Node *valueChild = node->getFirstChild();
   TR::Node *deltaChild = NULL;
   TR::Register *valueReg = cg->evaluate(valueChild);
   TR::Register *deltaReg = NULL;
   TR::Register *resultReg = NULL;

   int32_t delta = 0;
   int32_t numDeps = 4;

   bool isAddOp = true;
   bool isGetAndOp = true;
   bool isLong = false;
   bool isArgConstant = false;

   TR::RecognizedMethod currentMethod = method->getRecognizedMethod();

   // Gather information about the method
   //
   switch (currentMethod)
      {
      case TR::java_util_concurrent_atomic_AtomicBoolean_getAndSet:
      case TR::java_util_concurrent_atomic_AtomicInteger_getAndSet:
      case TR::java_util_concurrent_atomic_AtomicLong_getAndSet:
      case TR::java_util_concurrent_atomic_AtomicReference_getAndSet:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndSet:
      case TR::java_util_concurrent_atomic_AtomicLongArray_getAndSet:
      case TR::java_util_concurrent_atomic_AtomicReferenceArray_getAndSet:
         {
         isAddOp = false;
         break;
         }
      case TR::java_util_concurrent_atomic_AtomicInteger_addAndGet:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_addAndGet:
         {
         isGetAndOp = false;
         }
      case TR::java_util_concurrent_atomic_AtomicInteger_getAndAdd:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndAdd:
         {
         break;
         }
      case TR::java_util_concurrent_atomic_AtomicInteger_incrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_incrementAndGet:
         {
         isGetAndOp = false;
         }
      case TR::java_util_concurrent_atomic_AtomicInteger_getAndIncrement:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndIncrement:
         {
         delta = (int32_t)1;
         isArgConstant = true;
         resultReg = cg->allocateRegister();
         break;
         }
      case TR::java_util_concurrent_atomic_AtomicInteger_decrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_decrementAndGet:
         {
         isGetAndOp = false;
         }
      case TR::java_util_concurrent_atomic_AtomicInteger_getAndDecrement:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndDecrement:
         {
         delta = (int32_t)-1;
         isArgConstant = true;
         resultReg = cg->allocateRegister();
         break;
         }
      case TR::java_util_concurrent_atomic_AtomicLong_addAndGet:
      case TR::java_util_concurrent_atomic_AtomicLongArray_addAndGet:
         {
         isGetAndOp = false;
         }
      case TR::java_util_concurrent_atomic_AtomicLong_getAndAdd:
      case TR::java_util_concurrent_atomic_AtomicLongArray_getAndAdd:
         {
         isLong = true;
         break;
         }
      case TR::java_util_concurrent_atomic_AtomicLong_incrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicLongArray_incrementAndGet:
         {
         isGetAndOp = false;
         }
      case TR::java_util_concurrent_atomic_AtomicLong_getAndIncrement:
      case TR::java_util_concurrent_atomic_AtomicLongArray_getAndIncrement:
         {
         isLong = true;
         delta = (int64_t)1;
         break;
         }
      case TR::java_util_concurrent_atomic_AtomicLong_decrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicLongArray_decrementAndGet:
         {
         isGetAndOp = false;
         }
      case TR::java_util_concurrent_atomic_AtomicLong_getAndDecrement:
      case TR::java_util_concurrent_atomic_AtomicLongArray_getAndDecrement:
         {
         isLong = true;
         delta = (int64_t)-1;
         break;
         }
      }

   //Determine the offset of the value field
   //
   int32_t shiftAmount = 0;
   TR::Node *indexChild = NULL;
   TR::Register *indexRegister = NULL;
   TR::Register *fieldOffsetReg = NULL;
   int32_t fieldOffset;

   if (!isArray)
      {
      TR_OpaqueClassBlock * bdClass;
      char *className, *fieldSig;
      int32_t classNameLen, fieldSigLen;

      fieldSigLen = 1;

      switch (currentMethod)
         {
         case TR::java_util_concurrent_atomic_AtomicBoolean_getAndSet:
            className = "Ljava/util/concurrent/atomic/AtomicBoolean;";
            classNameLen = 43;
            fieldSig = "I";  // not a typo, the field is int
            break;
         case TR::java_util_concurrent_atomic_AtomicInteger_getAndSet:
         case TR::java_util_concurrent_atomic_AtomicInteger_addAndGet:
         case TR::java_util_concurrent_atomic_AtomicInteger_getAndAdd:
         case TR::java_util_concurrent_atomic_AtomicInteger_incrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicInteger_getAndIncrement:
         case TR::java_util_concurrent_atomic_AtomicInteger_decrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicInteger_getAndDecrement:
            className = "Ljava/util/concurrent/atomic/AtomicInteger;";
            classNameLen = 43;
            fieldSig = "I";
            break;
         case TR::java_util_concurrent_atomic_AtomicLong_getAndSet:
         case TR::java_util_concurrent_atomic_AtomicLong_addAndGet:
         case TR::java_util_concurrent_atomic_AtomicLong_getAndAdd:
         case TR::java_util_concurrent_atomic_AtomicLong_incrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicLong_getAndIncrement:
         case TR::java_util_concurrent_atomic_AtomicLong_decrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicLong_getAndDecrement:
            className = "Ljava/util/concurrent/atomic/AtomicLong;";
            classNameLen = 40;
            fieldSig = "J";
            break;
         case TR::java_util_concurrent_atomic_AtomicReference_getAndSet:
            className = "Ljava/util/concurrent/atomic/AtomicReference;";
            classNameLen = 45;
            fieldSig = "Ljava/lang/Object;";
         fieldSigLen = 18;
         break;
         default:
            TR_ASSERT( 0, "Unknown atomic operation method\n");
            return NULL;
         }

      TR_ResolvedMethod *owningMethod = node->getSymbolReference()->getOwningMethod(comp);
      TR_OpaqueClassBlock *containingClass = fej9->getClassFromSignature(className, classNameLen, owningMethod, true);
      fieldOffset = fej9->getInstanceFieldOffset(containingClass, "value", 5, fieldSig, fieldSigLen)
                    + fej9->getObjectHeaderSizeInBytes();  // size of a J9 object header
      }
   else
      {
      if (isArray)
         {
         indexChild = node->getChild(1);
         indexRegister = cg->evaluate(indexChild);
         fieldOffset = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
         if (size == 4)
            shiftAmount = 2;
         else if (size == 8)
            shiftAmount = 3;

         fieldOffsetReg = cg->allocateRegister();
         generateRSInstruction(cg, TR::InstOpCode::SLL, node, fieldOffsetReg, indexRegister, shiftAmount);
         }
      }

   // Exploit z196 interlocked-update instructions
   if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z196))
      {
      if (isAddOp) //getAndAdd or andAndGet
         {
         if (node->getNumChildren() > 1)
            {
            // 2nd operand needs to be in a register
            deltaChild = node->getSecondChild();
            deltaReg = cg->evaluate(deltaChild);
            cg->decReferenceCount(deltaChild);
            }
         else
            {
            // no 2nd child = Atomic.increment or decrement, delta should be +/- 1
            deltaReg = cg->allocateRegister();
            if (!isLong)
               {
               generateRIInstruction(cg, TR::InstOpCode::LHI, node, deltaReg, delta);
               }
            else
               {
               generateRIInstruction(cg, TR::InstOpCode::LGHI, node, deltaReg, delta);
               }
            }

         // Load And Add: LAA R1,R2,Mem
         // R1 = Mem; Mem = Mem + R2;
         // IMPORTANT: LAAG throws hardware exception if Mem is not double word aligned
         //            Class AtomicLong currently has its value field d.word aligned
         if (!resultReg)
            resultReg = cg->allocateRegister();

         if (!isLong)
            {
            if (fieldOffsetReg)
               generateRSInstruction(cg, TR::InstOpCode::LAA, node, resultReg, deltaReg, new (cg->trHeapMemory()) TR::MemoryReference(valueReg, fieldOffsetReg, fieldOffset, cg));
            else
               generateRSInstruction(cg, TR::InstOpCode::LAA, node, resultReg, deltaReg, new (cg->trHeapMemory()) TR::MemoryReference(valueReg, fieldOffset, cg));
            }
         else
            {
            if (fieldOffsetReg)
               generateRSInstruction(cg, TR::InstOpCode::LAAG, node, resultReg, deltaReg, new (cg->trHeapMemory()) TR::MemoryReference(valueReg, fieldOffsetReg, fieldOffset, cg));
            else
               generateRSInstruction(cg, TR::InstOpCode::LAAG, node, resultReg, deltaReg, new (cg->trHeapMemory()) TR::MemoryReference(valueReg, fieldOffset, cg));
            }
         if (!isGetAndOp)
            {
            // for addAndGet, the result needs to be recomputed. LAA loaded the original value into resultReg.
            if (!isLong)
               generateRRInstruction(cg, TR::InstOpCode::AR, node, resultReg, deltaReg);
            else
               generateRRInstruction(cg, TR::InstOpCode::AGR, node, resultReg, deltaReg);
            }

         cg->stopUsingRegister(deltaReg);
         cg->decReferenceCount(valueChild);
         cg->stopUsingRegister(valueReg);

         node->setRegister(resultReg);
         return resultReg;
         }
      }

   if (node->getNumChildren() > 1)
      {
      deltaChild = node->getSecondChild();

      //Determine if the delta is a constant.
      //
      if (deltaChild->getOpCode().isLoadConst() && !deltaChild->getRegister())
         {
         delta = (int32_t)(deltaChild->getInt());
         isArgConstant = true;
         resultReg = cg->allocateRegister();
         }
      else if (isAddOp)
         {
         deltaReg = cg->evaluate(deltaChild);
         resultReg = cg->allocateRegister();
         }
      else
         {
         resultReg = cg->evaluate(deltaChild);
         }
      }

   TR::RegisterDependencyConditions * dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, numDeps, cg);
   TR::LabelSymbol *cFlowRegionEnd = generateLabelSymbol(cg);
   TR::LabelSymbol *loopLabel = generateLabelSymbol(cg);


   // If this is a getAndSet of a constant, load the constant outside the loop.
   //
   if (!isAddOp && isArgConstant)
      generateLoad32BitConstant(cg, node, delta, resultReg, true);

   // Get the existing value
   //
   TR::Register *tempReg = cg->allocateRegister();
   if (fieldOffsetReg)
      generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, tempReg, new (cg->trHeapMemory()) TR::MemoryReference(valueReg, fieldOffsetReg, fieldOffset, cg));
   else
      generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, tempReg, new (cg->trHeapMemory()) TR::MemoryReference(valueReg, fieldOffset, cg));

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, loopLabel);
   loopLabel->setStartInternalControlFlow();

   // Perform the addition operation, if necessary
   //
   if (isAddOp)
      {
      generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(),node, resultReg, tempReg);
      if(isArgConstant)
         {
         generateS390ImmOp(cg, TR::InstOpCode::getAddOpCode(), node, resultReg, resultReg, (int32_t) delta, dependencies, NULL);
         }
      else
         {
         generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, resultReg ,deltaReg);
         }
      }

   // Compare and swap!
   //
   if (fieldOffsetReg)
      generateRSInstruction(cg, TR::InstOpCode::CS, node, tempReg, resultReg, new (cg->trHeapMemory()) TR::MemoryReference(valueReg, fieldOffsetReg, fieldOffset, cg));
   else
      generateRSInstruction(cg, TR::InstOpCode::CS, node, tempReg, resultReg, new (cg->trHeapMemory()) TR::MemoryReference(valueReg, fieldOffset, cg));

   // Branch if the compare and swap failed and try again.
   //
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC,TR::InstOpCode::COND_BL, node, loopLabel);

   dependencies->addPostCondition(valueReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(tempReg, TR::RealRegister::AssignAny);

   if (resultReg)
      dependencies->addPostCondition(resultReg, TR::RealRegister::AssignAny);
   if (deltaReg)
      dependencies->addPostCondition(deltaReg, TR::RealRegister::AssignAny);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, dependencies);
   cFlowRegionEnd->setEndInternalControlFlow();

   if (deltaChild != NULL)
      cg->decReferenceCount(deltaChild);
   if (deltaReg)
      cg->stopUsingRegister(deltaReg);

   cg->decReferenceCount(valueChild);
   cg->stopUsingRegister(valueReg);

   if (isGetAndOp)
      {
      // For Get And Op, the return value be stored in the temp register
      //
      if(resultReg)
         cg->stopUsingRegister(resultReg);
      node->setRegister(tempReg);
      return tempReg;
      }
   else
      {
      // For Op And Get,  the return  value will be stored in the result register
      //
      cg->stopUsingRegister(tempReg);
      node->setRegister(resultReg);
      return resultReg;
      }
   }

static TR::Register *
evaluateTwo32BitLoadsInA64BitRegister(
      TR::Node *node,
      TR::CodeGenerator *cg,
      TR::Node * highNode,
      TR::Node *lowNode)
   {
   TR::Register * targetRegister = cg->gprClobberEvaluate(highNode);
   TR::Instruction * cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, targetRegister, targetRegister, 32);

   generateRRInstruction(cg, TR::InstOpCode::LR, node, targetRegister, cg->evaluate(lowNode));
   return targetRegister;
   }

//TODO: CS clobbers first arg, and padLow ,refFirst
static TR::RegisterPair *
evaluateTwo32BitLoadsInAConsecutiveEvenOddPair(
      TR::Node *node,
      TR::CodeGenerator *cg,
      TR::Node * highNode,
      TR::Node *lowNode,
      TR::RegisterDependencyConditions * dependencies,
      bool isRefFirst,
      bool isClobberEval)
   {
   TR::Register * evenReg = (isClobberEval || (!isRefFirst))? cg->gprClobberEvaluate(highNode) : cg->evaluate(highNode);
   TR::Register * oddReg  = (isClobberEval || (isRefFirst))? cg->gprClobberEvaluate(lowNode) : cg->evaluate(lowNode);
   TR::Register *  padReg = isRefFirst ? oddReg : evenReg;
   generateRSInstruction(cg, TR::InstOpCode::SLLG, node, padReg, padReg, 32);

   TR::RegisterPair * newRegisterPair  =  cg->allocateConsecutiveRegisterPair(oddReg, evenReg);
   dependencies->addPostCondition(evenReg, TR::RealRegister::LegalEvenOfPair);
   dependencies->addPostCondition(oddReg, TR::RealRegister::LegalOddOfPair);
   dependencies->addPostCondition(newRegisterPair, TR::RealRegister::EvenOddPair);
   TR_ASSERT( newRegisterPair->getHighOrder() == evenReg, "evenReg is not high order\n");
   return newRegisterPair;
   }

extern TR::Register *
inlineAtomicFieldUpdater(
      TR::Node *node,
      TR::CodeGenerator *cg,
      TR::MethodSymbol *method)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   TR::Register * resultReg;
   TR::RecognizedMethod currentMethod = method->getRecognizedMethod();

   //Gather information about the method
   bool isAddOp = true;
   bool isGetAndOp = true;
   bool isArgConstant = false;
   int32_t delta = 1;
   char* className = "java/util/concurrent/atomic/AtomicIntegerFieldUpdater$AtomicIntegerFieldUpdaterImpl";
   int32_t classNameLen = 83;

   switch (currentMethod)
      {
      case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_getAndDecrement:
         delta = -1;
      case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_getAndIncrement:
         isArgConstant = true;
      case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_getAndAdd:
         break;
      case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_decrementAndGet:
         delta = -1;
      case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_incrementAndGet:
         isArgConstant = true;
      case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_addAndGet:
         isGetAndOp = false;
         break;
      }

   // getting the offsets to various fields: tclass, class, offset
   TR_ResolvedMethod *owningMethod = node->getSymbolReference()->getOwningMethod(comp);
   TR_OpaqueClassBlock *containingClass = fej9->getClassFromSignature(className, classNameLen, owningMethod, true);
   int32_t offset = fej9->getInstanceFieldOffset(containingClass, "offset", 6, "J", 1)
      + fej9->getObjectHeaderSizeInBytes();  // size of a J9 object header
   int32_t cclass = fej9->getInstanceFieldOffset(containingClass, "cclass", 6, "Ljava/lang/Class;", 17)
      + fej9->getObjectHeaderSizeInBytes();  // size of a J9 object header
   int32_t tclass = fej9->getInstanceFieldOffset(containingClass, "tclass", 6, "Ljava/lang/Class;", 17)
      + fej9->getObjectHeaderSizeInBytes();  // size of a J9 object header

   TR::Register * thisReg = cg->evaluate(node->getFirstChild());
   TR::Register * objReg = cg->evaluate(node->getSecondChild());
   TR::Register * tempReg = cg->allocateRegister();
   TR::Register * trueReg = cg->machine()->getRealRegister(TR::RealRegister::GPR5);
   TR::Register * deltaReg;
   TR::Register * offsetReg = cg->allocateRegister();
   TR::Register * tClassReg = cg->allocateRegister();
   TR::Register * objClassReg = cg->allocateRegister();

   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *callLabel = generateLabelSymbol(cg);

   // evaluate the delta node if it exists
   if (isArgConstant)
      {
      deltaReg = cg->allocateRegister();
      generateRIInstruction(cg, TR::InstOpCode::LHI, node, deltaReg, delta);
      }
   else
      {
      deltaReg = cg->evaluate(node->getChild(2));
      }

   bool is64Bit = TR::Compiler->target.is64Bit() && !comp->useCompressedPointers();

   // cclass == null?
   generateRRInstruction(cg, is64Bit ? TR::InstOpCode::XGR : TR::InstOpCode::XR, node, tempReg, tempReg);
   generateRXInstruction(cg, is64Bit ? TR::InstOpCode::CG : TR::InstOpCode::C, node, tempReg, generateS390MemoryReference(thisReg, cclass, cg));
   generateRRFInstruction(cg, TR::InstOpCode::LOCR, node, tempReg, trueReg, getMaskForBranchCondition(TR::InstOpCode::COND_BNER), true);

   // obj == null?
   generateRRInstruction(cg, TR::Compiler->target.is64Bit() ? TR::InstOpCode::LTGR : TR::InstOpCode::LTR, node, objReg, objReg);
   generateRRFInstruction(cg, TR::InstOpCode::LOCR, node, tempReg, trueReg, getMaskForBranchCondition(TR::InstOpCode::COND_BER), true);

   TR::TreeEvaluator::genLoadForObjectHeadersMasked(cg, node, objClassReg, generateS390MemoryReference(objReg, TR::Compiler->om.offsetOfObjectVftField(), cg), NULL);

   // obj.getClass() == tclass?
   if (comp->useCompressedPointers())
      {
      // inline the getClass() method = grab it from j9class
      generateRXInstruction(cg, TR::InstOpCode::LG, node, objClassReg, generateS390MemoryReference(objClassReg, fej9->getOffsetOfJavaLangClassFromClassField(), cg));

      // get tclass
      generateRXInstruction(cg, TR::InstOpCode::LLGF, node, tClassReg, generateS390MemoryReference(thisReg, tclass, cg));
      int32_t shiftAmount = TR::Compiler->om.compressedReferenceShift();
      if (shiftAmount != 0)
         {
         generateRSInstruction(cg, TR::InstOpCode::SLLG, node, tClassReg, tClassReg, shiftAmount);
         }
      }
   else
      {
      // inline the getClass() method = grab it from j9class
      generateRXInstruction(cg, TR::Compiler->target.is64Bit() ? TR::InstOpCode::LG : TR::InstOpCode::L, node, objClassReg, generateS390MemoryReference(objClassReg, fej9->getOffsetOfJavaLangClassFromClassField(), cg));

      // get tclass
      generateRXInstruction(cg, TR::Compiler->target.is64Bit() ? TR::InstOpCode::LG : TR::InstOpCode::L, node, tClassReg, generateS390MemoryReference(thisReg, tclass, cg));
      }
   generateRRInstruction(cg, TR::Compiler->target.is64Bit() ? TR::InstOpCode::CGR : TR::InstOpCode::CR, node, objClassReg, tClassReg);
   generateRRFInstruction(cg, TR::InstOpCode::LOCR, node, tempReg, trueReg, getMaskForBranchCondition(TR::InstOpCode::COND_BNER), true);

   // if any of the above has set the flag, we need to revert back to call the original method via OOL
   generateRRInstruction(cg, TR::InstOpCode::LTR, node, tempReg, tempReg);
   generateS390BranchInstruction (cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, callLabel);

   // start OOL
   TR_S390OutOfLineCodeSection *outlinedCall = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(callLabel, doneLabel, cg);
   cg->getS390OutOfLineCodeSectionList().push_front(outlinedCall);
   outlinedCall->swapInstructionListsWithCompilation();
   TR::Instruction * cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, callLabel);

   if (cg->getDebug())
      cg->getDebug()->addInstructionComment(cursor, "Denotes start of OOL AtomicFieldUpdater");

   // original call, this decrements node counts
   resultReg = TR::TreeEvaluator::performCall(node, false, cg);

   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, doneLabel);
   if (cg->getDebug())
      cg->getDebug()->addInstructionComment(cursor, "Denotes end of OOL AtomicFieldUpdater");

   outlinedCall->swapInstructionListsWithCompilation();

   // inline fast path: use Load-and-add. Get the offset of the value from the reflection object
   generateRXInstruction(cg, TR::InstOpCode::LG, node, offsetReg, generateS390MemoryReference(thisReg, offset, cg));
   generateRSInstruction(cg, TR::InstOpCode::LAA, node, resultReg, deltaReg, new (cg->trHeapMemory()) TR::MemoryReference(objReg, offsetReg, 0, cg));

   // for addAndGet we need to recompute the resultReg
   if (!isGetAndOp)
      {
      generateRRInstruction(cg, TR::InstOpCode::AR, node, resultReg, deltaReg);
      }

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneLabel);

   cg->stopUsingRegister(tempReg);
   cg->stopUsingRegister(deltaReg);
   cg->stopUsingRegister(offsetReg);
   cg->stopUsingRegister(tClassReg);
   cg->stopUsingRegister(objClassReg);

   return resultReg;
   }

extern TR::Register *
inlineKeepAlive(
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   TR::Node *paramNode = node->getFirstChild();
   TR::Register *paramReg = cg->evaluate(paramNode);
   TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 1, cg);
   conditions->addPreCondition(paramReg, TR::RealRegister::AssignAny);
   conditions->addPostCondition(paramReg, TR::RealRegister::AssignAny);
   TR::LabelSymbol *label = generateLabelSymbol(cg);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label, conditions);
   cg->decReferenceCount(paramNode);
   return NULL;
   }

/**
 * Helper routine to generate a write barrier sequence for the Transactional Memory inlined sequences.
 */
static void
genWrtBarForTM(
      TR::Node *node,
      TR::CodeGenerator *cg,
      TR::Register * objReg,
      TR::Register * srcReg,
      TR::Register * resultReg,
      bool checkResultRegForTMSuccess)
   {
   TR::Compilation *comp = cg->comp();
   auto gcMode = TR::Compiler->om.writeBarrierType();
   bool doWrtBar = (gcMode == gc_modron_wrtbar_oldcheck ||
         gcMode == gc_modron_wrtbar_cardmark_and_oldcheck ||
         gcMode == gc_modron_wrtbar_always);
   bool doCrdMrk = (gcMode == gc_modron_wrtbar_cardmark || gcMode == gc_modron_wrtbar_cardmark_and_oldcheck || gcMode == gc_modron_wrtbar_cardmark_incremental);

   if (doWrtBar || doCrdMrk)
      {
      TR::LabelSymbol *doneLabelWrtBar = generateLabelSymbol(cg);
      TR::Register *epReg = cg->allocateRegister();
      TR::Register *raReg = cg->allocateRegister();

      TR::RegisterDependencyConditions* condWrtBar = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 5, cg);

      condWrtBar->addPostCondition(objReg, TR::RealRegister::GPR1);
      condWrtBar->addPostCondition(srcReg, TR::RealRegister::GPR2);
      condWrtBar->addPostCondition(epReg, cg->getEntryPointRegister());
      condWrtBar->addPostCondition(raReg, cg->getReturnAddressRegister());

      // tmOffer returns 0 if transaction succeeds, tmPoll returns a non-Null object pointer if the transaction succeeds
      // we skip the wrtbar if TM failed
      if (checkResultRegForTMSuccess)
         {
         // the resultReg is not in the reg deps for tmOffer, add it for internal control flow
         condWrtBar->addPostCondition(resultReg, TR::RealRegister::AssignAny);
         generateRRInstruction(cg, TR::InstOpCode::LTR, node, resultReg, resultReg);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, doneLabelWrtBar);
         }
      else
         {
         generateRRInstruction(cg, TR::Compiler->target.is64Bit() ? TR::InstOpCode::LTGR : TR::InstOpCode::LTR, node, resultReg, resultReg);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, doneLabelWrtBar);
         }

      if (doWrtBar)
         {
         TR::SymbolReference *wbRef;

         if (gcMode == gc_modron_wrtbar_cardmark_and_oldcheck || gcMode == gc_modron_wrtbar_oldcheck)
            wbRef = comp->getSymRefTab()->findOrCreateWriteBarrierStoreGenerationalSymbolRef(comp->getMethodSymbol());
         else
            wbRef = comp->getSymRefTab()->findOrCreateWriteBarrierStoreSymbolRef(comp->getMethodSymbol());

         VMnonNullSrcWrtBarCardCheckEvaluator(node, objReg, srcReg, epReg, raReg, doneLabelWrtBar,
                                                                    wbRef, condWrtBar, cg, false);
         }
      else if (doCrdMrk)
         {
         VMCardCheckEvaluator(node, objReg, epReg, condWrtBar, cg, false, doneLabelWrtBar, false);
         // true #1 -> copy of objReg just happened, it's safe to clobber tempReg
         // false #2 -> Don't do compile time check for heap obj
         }

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneLabelWrtBar, condWrtBar);

      cg->stopUsingRegister(epReg);
      cg->stopUsingRegister(raReg);
      }
   }

extern TR::Register *
inlineConcurrentLinkedQueueTMOffer(
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   int32_t offsetTail = 0;
   int32_t offsetNext = 0;
   TR_OpaqueClassBlock * classBlock1 = NULL;
   TR_OpaqueClassBlock * classBlock2 = NULL;
   TR::Register * rReturn = cg->allocateRegister();
   TR::Register * rThis = cg->evaluate(node->getFirstChild());
   TR::Register * rP = cg->allocateCollectedReferenceRegister();
   TR::Register * rQ = cg->allocateCollectedReferenceRegister();
   TR::Register * rN = cg->evaluate(node->getSecondChild());
   TR::Instruction * cursor = NULL;
   TR::LabelSymbol * insertLabel =  generateLabelSymbol(cg);
   TR::LabelSymbol * doneLabel =  generateLabelSymbol(cg);
   TR::LabelSymbol * failLabel =  generateLabelSymbol(cg);

   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 5, cg);

   deps->addPostCondition(rReturn, TR::RealRegister::AssignAny);
   deps->addPostCondition(rThis, TR::RealRegister::AssignAny);
   deps->addPostCondition(rP, TR::RealRegister::AssignAny);
   deps->addPostCondition(rQ, TR::RealRegister::AssignAny);
   deps->addPostCondition(rN, TR::RealRegister::AssignAny);

   bool usesCompressedrefs = comp->useCompressedPointers();
   int32_t shiftAmount = TR::Compiler->om.compressedReferenceShift();
   static char * disableTMOfferenv = feGetEnv("TR_DisableTMOffer");
   bool disableTMOffer = (disableTMOfferenv != NULL);

   classBlock1 = fej9->getClassFromSignature("Ljava/util/concurrent/ConcurrentLinkedQueue$Node;", 49, comp->getCurrentMethod(), true);
   classBlock2 = fej9->getClassFromSignature("Ljava/util/concurrent/ConcurrentLinkedQueue;", 44, comp->getCurrentMethod(), true);


   if (classBlock1 && classBlock2)
      {
      offsetNext = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock1, "next", 4, "Ljava/util/concurrent/ConcurrentLinkedQueue$Node;", 49);
      offsetTail = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock2, "tail", 4, "Ljava/util/concurrent/ConcurrentLinkedQueue$Node;", 49);
      }
   else
      disableTMOffer = true;

   cursor = generateRIInstruction(cg, TR::InstOpCode::LHI, node, rReturn, 1);

   static char * debugTM= feGetEnv("TR_DebugTM");

   if (debugTM)
      {
      if (disableTMOffer)
         {
         printf ("\nTM: disabling TM CLQ.Offer in %s (%s)", comp->signature(), comp->getHotnessName(comp->getMethodHotness()));
         fflush(stdout);
         }
      else
         {
         printf ("\nTM: use TM CLQ.Offer in %s (%s)", comp->signature(), comp->getHotnessName(comp->getMethodHotness()));
         fflush(stdout);
         }
      }

   static char * useNonConstrainedTM = feGetEnv("TR_UseNonConstrainedTM");
   static char * disableNIAI = feGetEnv("TR_DisableNIAI");

   // the Transaction Diagnostic Block (TDB) is a memory location for the OS to write state info in the event of an abort
   TR::MemoryReference* TDBmemRef = generateS390MemoryReference(cg->getMethodMetaDataRealRegister(), fej9->thisThreadGetTDBOffset(), cg);

   if (!disableTMOffer)
      {
      if (useNonConstrainedTM)
         {
         // immediate field described in TR::TreeEvaluator::tstartEvaluator
         cursor = generateSILInstruction(cg, TR::InstOpCode::TBEGIN, node, TDBmemRef, 0xFF02);

         cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK7, node, failLabel);
         }
      else
         {
         // No TDB for constrained transactions. Immediate field reflects TBEGINC can't filter interrupts
         cursor = generateSILInstruction(cg, TR::InstOpCode::TBEGINC, node, generateS390MemoryReference(0, cg), 0xFF00);
         }

      if (!disableNIAI)
         cursor = generateS390IEInstruction(cg, TR::InstOpCode::NIAI, 1, 0, node);

      if (usesCompressedrefs)
         {
         cursor = generateRXInstruction(cg, TR::InstOpCode::LLGF, node, rP, generateS390MemoryReference(rThis, offsetTail, cg));

         if (shiftAmount != 0)
            {
            cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, rP, rP, shiftAmount);
            }
         }
      else
         {
         cursor = generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, rP, generateS390MemoryReference(rThis, offsetTail, cg));
         }

      if (!disableNIAI)
         cursor = generateS390IEInstruction(cg, TR::InstOpCode::NIAI, 1, 0, node);

      if (usesCompressedrefs)
         {
         cursor = generateRXInstruction(cg, TR::InstOpCode::LT, node, rQ, generateS390MemoryReference(rP, offsetNext, cg));
         cursor = generateRRInstruction(cg, TR::InstOpCode::LLGFR, node, rQ, rQ);

         if (shiftAmount != 0)
            {
            cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, rQ, rQ, shiftAmount);
            }
         }
      else
         {
         cursor = generateRXInstruction(cg, TR::InstOpCode::getLoadTestOpCode(), node, rQ, generateS390MemoryReference(rP, offsetNext, cg));
         }

      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, insertLabel);
      cursor = generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, rP, rQ);

      if (!disableNIAI)
         cursor = generateS390IEInstruction(cg, TR::InstOpCode::NIAI, 1, 0, node);

      if (usesCompressedrefs)
         {
         cursor = generateRXInstruction(cg, TR::InstOpCode::LT, node, rQ, generateS390MemoryReference(rP, offsetNext, cg));
         cursor = generateRRInstruction(cg, TR::InstOpCode::LLGFR, node, rQ, rQ);
         if (shiftAmount != 0)
            {
            cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, rQ, rQ, shiftAmount);
            }
         }
      else
         {
         cursor = generateRXInstruction(cg, TR::InstOpCode::getLoadTestOpCode(), node, rQ, generateS390MemoryReference(rP, offsetNext, cg));
         }

      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, insertLabel);
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, doneLabel);

      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, insertLabel);

      if (usesCompressedrefs)
         {
         if (shiftAmount != 0)
            {
            cursor = generateRSInstruction(cg, TR::InstOpCode::SRLG, node, rQ, rN, shiftAmount);
            cursor = generateRXInstruction(cg, TR::InstOpCode::ST, node, rQ, generateS390MemoryReference(rP, offsetNext, cg));
            cursor = generateRXInstruction(cg, TR::InstOpCode::ST, node, rQ, generateS390MemoryReference(rThis, offsetTail, cg));
            }
         else
            {
            cursor = generateRXInstruction(cg, TR::InstOpCode::ST, node, rN, generateS390MemoryReference(rP, offsetNext, cg));
            cursor = generateRXInstruction(cg, TR::InstOpCode::ST, node, rN, generateS390MemoryReference(rThis, offsetTail, cg));
            }
         }
      else
         {
         cursor = generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, rN, generateS390MemoryReference(rP, offsetNext, cg));
         cursor = generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, rN, generateS390MemoryReference(rThis, offsetTail, cg));
         }

      cursor = generateRRInstruction(cg, TR::InstOpCode::XR, node, rReturn, rReturn);

      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneLabel, deps);

      cursor = generateSInstruction(cg, TR::InstOpCode::TEND, node, generateS390MemoryReference(cg->machine()->getRealRegister(TR::RealRegister::GPR0),0,cg));
      }

   if (useNonConstrainedTM || disableTMOffer)
      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, failLabel, deps);

   genWrtBarForTM(node, cg, rP, rN, rReturn, true);
   genWrtBarForTM(node, cg, rThis, rN, rReturn, true);

   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   cg->stopUsingRegister(rP);
   cg->stopUsingRegister(rQ);

   node->setRegister(rReturn);
   return rReturn;
   }

extern TR::Register *
inlineConcurrentLinkedQueueTMPoll(
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   int32_t offsetHead = 0;
   int32_t offsetNext = 0;
   int32_t offsetItem = 0;
   TR_OpaqueClassBlock * classBlock1 = NULL;
   TR_OpaqueClassBlock * classBlock2 = NULL;

   TR::Register * rE = cg->allocateCollectedReferenceRegister();
   TR::Register * rP = cg->allocateCollectedReferenceRegister();
   TR::Register * rQ = cg->allocateCollectedReferenceRegister();
   TR::Register * rThis = cg->evaluate(node->getFirstChild());
   TR::Register * rTmp = NULL;
   TR::Instruction * cursor = NULL;
   TR::LabelSymbol * doneLabel =  generateLabelSymbol(cg);
   TR::LabelSymbol * failLabel =  generateLabelSymbol(cg);

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 5, cg);
   deps->addPostCondition(rE, TR::RealRegister::AssignAny);
   deps->addPostCondition(rP, TR::RealRegister::AssignAny);
   deps->addPostCondition(rQ, TR::RealRegister::AssignAny);
   deps->addPostCondition(rThis, TR::RealRegister::AssignAny);

   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   bool usesCompressedrefs = comp->useCompressedPointers();
   int32_t shiftAmount = TR::Compiler->om.compressedReferenceShift();

   if (usesCompressedrefs && shiftAmount !=0)
      {
      rTmp = cg->allocateRegister();
      deps->addPostCondition(rTmp, TR::RealRegister::AssignAny);
      }

   static char * disableTMPollenv = feGetEnv("TR_DisableTMPoll");
   bool disableTMPoll = disableTMPollenv;

   classBlock1 = fej9->getClassFromSignature("Ljava/util/concurrent/ConcurrentLinkedQueue;", 44, comp->getCurrentMethod(), true);
   classBlock2 = fej9->getClassFromSignature("Ljava/util/concurrent/ConcurrentLinkedQueue$Node;", 49, comp->getCurrentMethod(), true);

   if (classBlock1 && classBlock2)
      {
      offsetHead = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock1, "head", 4, "Ljava/util/concurrent/ConcurrentLinkedQueue$Node;", 49);
      offsetNext = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock2, "next", 4, "Ljava/util/concurrent/ConcurrentLinkedQueue$Node;", 49);
      offsetItem = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock2, "item", 4, "Ljava/lang/Object;", 18);
      }
   else
      disableTMPoll = true;

   cursor = generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, rE, rE);

   static char * debugTM= feGetEnv("TR_DebugTM");

   if (debugTM)
      {
      if (disableTMPoll)
         {
         printf ("\nTM: disabling TM CLQ.Poll in %s (%s)", comp->signature(), comp->getHotnessName(comp->getMethodHotness()));
         fflush(stdout);
         }
      else
         {
         printf ("\nTM: use TM CLQ.Poll in %s (%s)", comp->signature(), comp->getHotnessName(comp->getMethodHotness()));
         fflush(stdout);
         }
      }

   static char * useNonConstrainedTM = feGetEnv("TR_UseNonConstrainedTM");
   static char * disableNIAI = feGetEnv("TR_DisableNIAI");

   // the Transaction Diagnostic Block (TDB) is a memory location for the OS to write state info in the event of an abort
   TR::MemoryReference* TDBmemRef = generateS390MemoryReference(cg->getMethodMetaDataRealRegister(), fej9->thisThreadGetTDBOffset(), cg);

   if (!disableTMPoll)
      {
      if (useNonConstrainedTM)
         {
         // immediate field described in TR::TreeEvaluator::tstartEvaluator
         cursor = generateSILInstruction(cg, TR::InstOpCode::TBEGIN, node, TDBmemRef, 0xFF02);

         cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK7, node, failLabel);
         }
      else
         {
         // No TDB for constrained transactions. Immediate field reflects TBEGINC can't filter interrupts
         cursor = generateSILInstruction(cg, TR::InstOpCode::TBEGINC, node, generateS390MemoryReference(0, cg), 0xFF00);
         }

      if (!disableNIAI)
         cursor = generateS390IEInstruction(cg, TR::InstOpCode::NIAI, 1, 0, node);

      if (usesCompressedrefs)
         {
         cursor = generateRXInstruction(cg, TR::InstOpCode::LLGF, node, rP, generateS390MemoryReference(rThis, offsetHead, cg));

         if (shiftAmount != 0)
            {
            cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, rP, rP, shiftAmount);
            }
         }
      else
         {
         cursor = generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, rP, generateS390MemoryReference(rThis, offsetHead, cg));
         }

      if (!disableNIAI)
         cursor = generateS390IEInstruction(cg, TR::InstOpCode::NIAI, 1, 0, node);

      if (usesCompressedrefs)
         {
         cursor = generateRXInstruction(cg, TR::InstOpCode::LLGF, node, rE, generateS390MemoryReference(rP, offsetItem, cg));

         if (shiftAmount != 0)
            {
            cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, rE, rE, shiftAmount);
            }
         }
      else
         {
         cursor = generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, rE, generateS390MemoryReference(rP, offsetItem, cg));
         }

      if (!disableNIAI)
         cursor = generateS390IEInstruction(cg, TR::InstOpCode::NIAI, 1, 0, node);

      if (usesCompressedrefs)
         {
         cursor = generateRXInstruction(cg, TR::InstOpCode::LT, node, rQ, generateS390MemoryReference(rP, offsetNext, cg));
         cursor = generateSILInstruction(cg, TR::InstOpCode::MVHI, node, generateS390MemoryReference(rP, offsetItem, cg), 0);
         }
      else
         {
         cursor = generateRXInstruction(cg, TR::InstOpCode::getLoadTestOpCode(), node, rQ, generateS390MemoryReference(rP, offsetNext, cg));
         cursor = generateSILInstruction(cg, TR::InstOpCode::getMoveHalfWordImmOpCode(), node, generateS390MemoryReference(rP, offsetItem, cg), 0);
         }

      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, doneLabel);

      if (usesCompressedrefs)
         {
         cursor = generateRXInstruction(cg, TR::InstOpCode::ST, node, rQ, generateS390MemoryReference(rThis, offsetHead, cg));
         if (shiftAmount != 0)
            {
            cursor = generateRSInstruction(cg, TR::InstOpCode::SRLG, node, rTmp, rP, shiftAmount);
            cursor = generateRXInstruction(cg, TR::InstOpCode::ST, node, rTmp, generateS390MemoryReference(rP, offsetNext, cg));
            }
         else
            {
            cursor = generateRXInstruction(cg, TR::InstOpCode::ST, node, rP, generateS390MemoryReference(rP, offsetNext, cg));
            }
         }
      else
         {
         cursor = generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, rQ, generateS390MemoryReference(rThis, offsetHead, cg));
         cursor = generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, rP, generateS390MemoryReference(rP, offsetNext, cg));
         }

      if (useNonConstrainedTM)
         cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneLabel);
      else
         cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneLabel, deps);

      cursor = generateSInstruction(cg, TR::InstOpCode::TEND, node, generateS390MemoryReference(cg->machine()->getRealRegister(TR::RealRegister::GPR0),0,cg));
      }

   if (useNonConstrainedTM || disableTMPoll)
      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, failLabel, deps);

   if (usesCompressedrefs)
      {
      generateRRInstruction(cg, TR::InstOpCode::LLGFR, node, rQ, rQ);

      if (shiftAmount != 0)
         {
         cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, rQ, rQ, shiftAmount);
         }
      }

   genWrtBarForTM(node, cg, rThis, rQ, rQ, false);
   // we don't need wrtbar for P, it is dead (or has NULL)

   cg->decReferenceCount(node->getFirstChild());
   cg->stopUsingRegister(rP);
   cg->stopUsingRegister(rQ);

   if (usesCompressedrefs && shiftAmount != 0)
      {
      cg->stopUsingRegister(rTmp);
      }

   node->setRegister(rE);

   return rE;
   }

void
VMgenerateCatchBlockBBStartPrologue(
      TR::Node *node,
      TR::Instruction *fenceInstruction,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   TR::Block *block = node->getBlock();

   // Encourage recompilation
   if (fej9->shouldPerformEDO(block, comp))
      {
      TR::Register * biAddrReg = cg->allocateRegister();

      // Load address of counter into biAddrReg
      genLoadAddressConstant(cg, node, (uintptrj_t) comp->getRecompilationInfo()->getCounterAddress(), biAddrReg);

      // Counter is 32-bit, so only use 32-bit opcodes
      if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z10))
         {
         TR::MemoryReference * recompMR = generateS390MemoryReference(biAddrReg, 0, cg);
         generateSIInstruction(cg, TR::InstOpCode::ASI, node, recompMR, -1);
         recompMR->stopUsingMemRefRegister(cg);
         }
      else
         {
         TR::Register * recompCounterReg = cg->allocateRegister();
         TR::MemoryReference * loadbiMR = generateS390MemoryReference(biAddrReg, 0, cg);
         generateRXInstruction(cg, TR::InstOpCode::L, node, recompCounterReg, loadbiMR);
         generateRIInstruction(cg, TR::InstOpCode::AHI, node, recompCounterReg, -1);
         loadbiMR->stopUsingMemRefRegister(cg);
         TR::MemoryReference * storebiMR = generateS390MemoryReference(biAddrReg, 0, cg);
         generateRXInstruction(cg, TR::InstOpCode::ST, node, recompCounterReg, storebiMR);

         cg->stopUsingRegister(recompCounterReg);
         storebiMR->stopUsingMemRefRegister(cg);
         }

      // Check counter and induce recompilation if counter = 0
      TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
      TR::LabelSymbol * snippetLabel     = generateLabelSymbol(cg);
      TR::LabelSymbol * restartLabel     = generateLabelSymbol(cg);

      snippetLabel->setEndInternalControlFlow();

      TR::Register * tempReg1 = cg->allocateRegister();
      TR::Register * tempReg2 = cg->allocateRegister();

      TR::RegisterDependencyConditions * dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg);
      dependencies->addPostCondition(tempReg1, cg->getEntryPointRegister());
      dependencies->addPostCondition(tempReg2, cg->getReturnAddressRegister());
      // Branch to induceRecompilation helper routine if counter is 0 - based on condition code of the preceding adds.
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
      cFlowRegionStart->setStartInternalControlFlow();
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, snippetLabel);

      TR::Snippet * snippet = new (cg->trHeapMemory()) TR::S390ForceRecompilationSnippet(cg, node, restartLabel, snippetLabel);
      cg->addSnippet(snippet);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, restartLabel, dependencies);

      cg->stopUsingRegister(tempReg1);
      cg->stopUsingRegister(tempReg2);

      cg->stopUsingRegister(biAddrReg);
      }
   }

float
J9::Z::TreeEvaluator::interpreterProfilingInstanceOfOrCheckCastTopProb(TR::CodeGenerator * cg, TR::Node * node)
   {
   TR::Compilation *comp = cg->comp();
   TR_ByteCodeInfo bcInfo = node->getByteCodeInfo();
   TR_ValueProfileInfoManager * valueProfileInfo = TR_ValueProfileInfoManager::get(comp);

   if (!valueProfileInfo)
      return 0;

   TR_AddressInfo *valueInfo = static_cast<TR_AddressInfo*>(valueProfileInfo->getValueInfo(bcInfo, comp, AddressInfo, TR_ValueProfileInfoManager::justInterpreterProfileInfo));
   if (!valueInfo || valueInfo->getNumProfiledValues()==0)
      {
      return 0;
      }

   TR_OpaqueClassBlock *topValue = (TR_OpaqueClassBlock *) valueInfo->getTopValue();
   if (!topValue)
      {
      return 0;
      }

   if (valueInfo->getTopProbability() < TR::Options::getMinProfiledCheckcastFrequency())
      return 0;

   if (comp->getPersistentInfo()->isObsoleteClass(topValue, cg->fe()))
      {
      return 0;
      }

   return valueInfo->getTopProbability();
   }

/**
 * countDigitsEvaluator - count the number of decimal digits of an integer/long binary
 * value (excluding the negative sign).  The original counting digits Java loop is
 * reduced to this IL node by idiom recognition.
 */
TR::Register *
J9::Z::TreeEvaluator::countDigitsEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   // Idiom recognition will reduce the appropriate loop into the following
   // form:
   //    TR::countDigits
   //        inputValue  // either int or long
   //        digits10LookupTable
   //
   // Original loop:
   //      do { count ++; } while((l /= 10) != 0);
   //
   // Since the maximum number of decimal digits for an int is 10, and a long is 19,
   // we can perform binary search comparing the input value with pre-computed digits.


   TR::Node * inputNode = node->getChild(0);
   TR::Register * inputReg = cg->gprClobberEvaluate(inputNode);
   TR::Register * workReg = cg->evaluate(node->getChild(1));
   TR::Register * countReg = cg->allocateRegister();

   TR_ASSERT( inputNode->getDataType() == TR::Int64 || inputNode->getDataType() == TR::Int32, "child of TR::countDigits must be of integer type");

   bool isLong = (inputNode->getDataType() == TR::Int64);
   TR_ASSERT( !isLong || TR::Compiler->target.is64Bit(), "CountDigitEvaluator requires 64-bit support for longs");

   TR::RegisterDependencyConditions * dependencies;
   dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg);
   dependencies->addPostCondition(inputReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(workReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(countReg, TR::RealRegister::AssignAny);

   TR::MemoryReference * work[18];
   TR::LabelSymbol * label[18];
   TR::LabelSymbol * cFlowRegionEnd = generateLabelSymbol(cg);

   // Get the negative input value (2's complement) - We treat all numbers as
   // negative to simplify the absolute comparison, and take advance of the
   // CC trick in countsDigitHelper.

   // If the input is a 32-bit value on 64-bit architecture, we cannot simply use TR::InstOpCode::LNGR because the input may not be sign-extended.
   // If you want to use TR::InstOpCode::LNGR for a 32-bit value on 64-bit architecture, you'll need to additionally generate TR::InstOpCode::LGFR for the input.
   generateRRInstruction(cg, !isLong ? TR::InstOpCode::LNR : TR::InstOpCode::LNGR, node, inputReg, inputReg);

   TR::LabelSymbol * cFloWRegionStart = generateLabelSymbol(cg);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node,  cFloWRegionStart);
   cFloWRegionStart->setStartInternalControlFlow();

   if (isLong)
      {
      for (int32_t i = 0; i < 18; i++)
         {
         work[i] = generateS390MemoryReference(workReg, i*8, cg);
         label[i] = generateLabelSymbol(cg);
         }

      generateRXInstruction(cg, TR::InstOpCode::CG, node, inputReg, work[7]);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, label[11]);

      // LABEL 3
      generateRXInstruction(cg, TR::InstOpCode::CG, node, inputReg, work[3]);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, label[5]);

      // LABEL 1
      generateRXInstruction(cg, TR::InstOpCode::CG, node, inputReg, work[1]);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, label[2]);

      countDigitsHelper(node, cg, 0, work[0], inputReg, countReg, cFlowRegionEnd, isLong);           // 0 and 1

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label[2]);       // LABEL 2
      countDigitsHelper(node, cg, 2, work[2], inputReg, countReg, cFlowRegionEnd, isLong);           // 2 and 3

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label[5]);       // LABEL 5

      generateRXInstruction(cg, TR::InstOpCode::CG, node, inputReg, work[5]);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, label[6]);

      countDigitsHelper(node, cg, 4, work[4], inputReg, countReg, cFlowRegionEnd, isLong);           // 4 and 5

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label[6]);       // LABEL 6
      countDigitsHelper(node, cg, 6, work[6], inputReg, countReg, cFlowRegionEnd, isLong);          // 6 and 7

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label[11]);      // LABEL 11

      generateRXInstruction(cg, TR::InstOpCode::CG, node, inputReg, work[11]);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, label[14]);

      // LABEL 9
      generateRXInstruction(cg, TR::InstOpCode::CG, node, inputReg, work[9]);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, label[10]);

      countDigitsHelper(node, cg, 8, work[8], inputReg, countReg, cFlowRegionEnd, isLong);           // 8 and 9

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label[10]);      // LABEL 10
      countDigitsHelper(node, cg, 10, work[10], inputReg, countReg, cFlowRegionEnd, isLong);  // 10 and 11

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label[14]);      // LABEL 14

      generateRXInstruction(cg, TR::InstOpCode::CG, node, inputReg, work[14]);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, label[16]);

      // LABEL 12
      generateRXInstruction(cg, TR::InstOpCode::CG, node, inputReg, work[12]); // 12
      generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, countReg, 12+1);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BH, node, cFlowRegionEnd);

      // LABEL 13
      countDigitsHelper(node, cg, 13, work[13], inputReg, countReg, cFlowRegionEnd, isLong);  // 13 and 14

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label[16]);      // LABEL 16

      generateRXInstruction(cg, TR::InstOpCode::CG, node, inputReg, work[16]);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, label[17]);
      // LABEL 15
      countDigitsHelper(node, cg, 15, work[15], inputReg, countReg, cFlowRegionEnd, isLong);  // 15 and 16

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label[17]);      // LABEL 17
      countDigitsHelper(node, cg, 17, work[17], inputReg, countReg, cFlowRegionEnd, isLong);  // 17 and 18

      for (int32_t i = 0; i < 18; i++)
         {
         work[i]->stopUsingMemRefRegister(cg);
         }
      }
   else
      {
      for (int32_t i = 0; i < 9; i++)
         {
         work[i] = generateS390MemoryReference(workReg, i*8+4, cg);     // lower 32-bit
         label[i] = generateLabelSymbol(cg);
         }

      // We already generate the label instruction, why would we generate it again?
      //generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, startLabel);

      generateRXInstruction(cg, TR::InstOpCode::C, node, inputReg, work[3]);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, label[5]);

      // LABEL 1
      generateRXInstruction(cg, TR::InstOpCode::C, node, inputReg, work[1]);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, label[2]);

      countDigitsHelper(node, cg, 0, work[0], inputReg, countReg, cFlowRegionEnd, isLong);           // 0 and 1

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label[2]);       // LABEL 2
      countDigitsHelper(node, cg, 2, work[2], inputReg, countReg, cFlowRegionEnd, isLong);           // 2 and 3

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label[5]);       // LABEL 5

      generateRXInstruction(cg, TR::InstOpCode::C, node, inputReg, work[5]);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, label[7]);

      countDigitsHelper(node, cg, 4, work[4], inputReg, countReg, cFlowRegionEnd, isLong);           // 4 and 5

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label[7]);       // LABEL 7

      generateRXInstruction(cg, TR::InstOpCode::C, node, inputReg, work[7]);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, label[8]);

      countDigitsHelper(node, cg, 6, work[6], inputReg, countReg, cFlowRegionEnd, isLong);           // 6 and 7

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label[8]);       // LABEL 8
      countDigitsHelper(node, cg, 8, work[8], inputReg, countReg, cFlowRegionEnd, isLong);           // 8 and 9


      for (int32_t i = 0; i < 9; i++)
         {
         work[i]->stopUsingMemRefRegister(cg);
         }
      }

   cg->stopUsingRegister(inputReg);
   cg->stopUsingRegister(workReg);

   // End
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, dependencies);
   cFlowRegionEnd->setEndInternalControlFlow();

   node->setRegister(countReg);

   cg->decReferenceCount(inputNode);
   cg->decReferenceCount(node->getChild(1));
   return countReg;
   }

/**
 * countDigitsHelper emits code to determine whether the given input value has
 * memRefIndex or memRefIndex+1 digits.
 */
void
J9::Z::TreeEvaluator::countDigitsHelper(TR::Node * node, TR::CodeGenerator * cg,
                                        int32_t memRefIndex, TR::MemoryReference * memRef,
                                        TR::Register* inputReg, TR::Register* countReg,
                                        TR::LabelSymbol *doneLabel, bool isLong)
   {
   // Compare input value with the binary memRefIndex value. The instruction
   // sets CC1 if input <= [memRefIndex], which is also the borrow CC.  Since
   // the numbers are all negative, the equivalent comparison is set if
   // inputValue > [memRefIndex].
   generateRXInstruction(cg, (isLong)?TR::InstOpCode::CG:TR::InstOpCode::C, node, inputReg, memRef);        \

   // Clear countRegister and set it to 1 if inputValue > [memRefIndex].
   generateRRInstruction(cg, TR::InstOpCode::getSubtractWithBorrowOpCode(), node, countReg, countReg);
   generateRRInstruction(cg, TR::InstOpCode::getLoadComplementOpCode(), node, countReg, countReg);

   // Calculate final count of digits by adding to memRefIndex + 1.  The +1 is
   // required as our memRefIndex starts with index 0, but digit counts starts with 1.
   generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, countReg, memRefIndex+1);

   // CountReg has the number of digits.  Jump to done label.
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, doneLabel);

   }


/**
 * tstartEvaluator:  begin a transaction
 */
TR::Register *
J9::Z::TreeEvaluator::tstartEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
#ifndef PUBLIC_BUILD
   //   [0x00000000803797c8] (  0)  tstart
   //   [0x0000000080379738] (  1)    branch --> block 28 BBStart at [0x0000000080378bc8]
   //   [0x00000000803f15f8] (  1)      GlRegDeps
   //                        (  3)        ==>aRegLoad at [0x00000000803f1568] (in &GPR_0048)
   //                        (  2)        ==>aRegLoad at [0x00000000803f15b0] (in &GPR_0049)
   //   [0x0000000080379780] (  1)    branch --> block 29 BBStart at [0x0000000080378ed8]
   //   [0x00000000803f1640] (  1)      GlRegDeps
   //                        (  3)        ==>aRegLoad at [0x00000000803f1568] (in &GPR_0048)
   //                        (  2)        ==>aRegLoad at [0x00000000803f15b0] (in &GPR_0049)
   //   [0x00000000803796f0] (  1)    aload #422[0x000000008035e4b0]  Auto[<temp slot 2 holds monitoredObject syncMethod>]   <flags:"0x4" (X!=0 )/>
   //   [0x00000000803f1688] (  1)    GlRegDeps
   //                        (  3)      ==>aRegLoad at [0x00000000803f1568] (in &GPR_0048)


   // TBEGIN 0(R0),0xFF00
   // BRNEZ  OOL TM                        ; CC0 = success
   // ------ OOL TM ----
   // BRH    Block_Transient_Handler       ; CC2 = transient failure
   //    POST deps (persistent path)
   // BRC    Block_Persistent_Handler      ; CC1,CC3 = persistent failure
   //    Post deps (transient path)
   // BRC    mainline                      ; we need this brc for OOL mechanism, though it's never taken
   // -----------------------
   // LT     Rlw, lockword (obj)
   // BEQ    Label Start
   // TEND
   // BRC    Block_Transient_Handler
   // Label Start
   //    POST Deps

   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = static_cast<TR_J9VMBase*>(cg->fe());
   TR::Instruction * cursor = NULL;

   TR::Node * brPersistentNode = node->getFirstChild();
   TR::Node * brTransientNode = node->getSecondChild();
   TR::Node * fallThrough = node->getThirdChild();
   TR::Node * objNode = node->getChild(3);
   TR::Node * GRAChild = NULL;

   TR::LabelSymbol * labelPersistentFailure = brPersistentNode->getBranchDestination()->getNode()->getLabel();
   TR::LabelSymbol * labelTransientFailure = brTransientNode->getBranchDestination()->getNode()->getLabel();
   TR::LabelSymbol * startLabel = fallThrough->getBranchDestination()->getNode()->getLabel();

   TR::Register * objReg = cg->evaluate(objNode);
   TR::Register * monitorReg = cg->allocateRegister();

   TR::RegisterDependencyConditions *deps = NULL;
   TR::RegisterDependencyConditions *depsPersistent = NULL;
   TR::RegisterDependencyConditions *depsTransient = NULL;

   // GRA
   if (fallThrough->getNumChildren() !=0)
      {
      GRAChild = fallThrough->getFirstChild();
      cg->evaluate(GRAChild);
      deps = generateRegisterDependencyConditions(cg, GRAChild, 0);
      cg->decReferenceCount(GRAChild);
      }

   if (brPersistentNode->getNumChildren() != 0)
      {
      GRAChild = brPersistentNode->getFirstChild();
      cg->evaluate(GRAChild);
      depsPersistent = generateRegisterDependencyConditions(cg, GRAChild, 0);
      cg->decReferenceCount(GRAChild);
      }

   if (brTransientNode->getNumChildren() != 0)
      {
      GRAChild = brTransientNode->getFirstChild();
      cg->evaluate(GRAChild);
      depsTransient = generateRegisterDependencyConditions(cg, GRAChild, 0);
      cg->decReferenceCount(GRAChild);
      }

   // the Transaction Diagnostic Block (TDB) is a memory location for the OS to write state info in the event of an abort
   TR::MemoryReference* TDBmemRef = generateS390MemoryReference(cg->getMethodMetaDataRealRegister(), fej9->thisThreadGetTDBOffset(), cg);

   static char * debugTM = feGetEnv("debugTM");

   if (debugTM)
      {
      // artificially set CC to transientFailure, objReg is always > 0
      cursor = generateRRInstruction(cg, TR::Compiler->target.is64Bit() ? TR::InstOpCode::LTGR : TR::InstOpCode::LTR, node, objReg, objReg);
      }
   else
      {
      /// Immediate field of TBEGIN:
      /// bits 0-7:  FF - General Register Save Mask used to tell the hardware which pairs of registers need to be rolled back.
      ///                 always set to FF here because GRA will later decide which registers we actually need to roll back.
      /// bits 8-11:  0 - not used by hardware, always zero.
      /// bit 12:     0 - Allow access register modification
      /// bit 13:     0 - Allow floating-point operation
      /// bits 14-15: 2 - Program-Interruption-Filtering Control
      ///        PIFC bits needs to be set to 2, to allow 0C4 and 0C7 interrupts to resume, instead of being thrown.
      ///        Since all interrupts cause aborts, the PSW is rolled back to TBEGIN on interrupts. The 0C7 interrupts
      ///        are generated by trap instructions for Java exception handling. The 0C4 interrupts are used by z/OS LE to
      ///        detect guarded page exceptions which are used to trigger XPLINK stack growth. In both cases, either the
      ///        LE or JIT signal handler need the PSW of the actual instruction that generated the interrupt, not the
      ///        rolled back PSW pointing to TBEGIN. Without filtering these interrupts, the program will crash. Filtering
      ///        the interrupts allows us to resume execution following the abort and go to slow path so the exceptions
      ///        can be properly caught and handled.

      cursor = generateSILInstruction(cg, TR::InstOpCode::TBEGIN, node, TDBmemRef, 0xFF02);
      }

   if (labelTransientFailure == labelPersistentFailure)
      {
      if (depsPersistent != depsTransient) //only possible to be equal if they are NULL (i.e. non existent)
         {
         TR_ASSERT( depsPersistent && depsTransient, "regdeps wrong in tstart evaluator");
         uint32_t i = depsPersistent->getNumPostConditions();
         uint32_t j = depsTransient->getNumPostConditions();
         TR_ASSERT( i == j, "regdep postcondition number not the same");
         depsPersistent->getPostConditions()->getRegisterDependency(i);
         i = depsPersistent->getNumPreConditions();
         j = depsTransient->getNumPreConditions();
         TR_ASSERT( i == j, "regdep precondition number not the same");
         }
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK7, node, labelTransientFailure, depsPersistent);
      }
   else
      {
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BH, node, labelTransientFailure, depsTransient);
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK7, node, labelPersistentFailure, depsPersistent);
      }


   int32_t lwOffset = cg->fej9()->getByteOffsetToLockword((TR_OpaqueClassBlock *) cg->getMonClass(node));

   if (TR::Compiler->target.is64Bit() && cg->fej9()->generateCompressedLockWord())
      cursor = generateRXInstruction(cg, TR::InstOpCode::LT, node, monitorReg, generateS390MemoryReference(objReg, lwOffset, cg), cursor);
   else
      cursor = generateRXInstruction(cg, TR::InstOpCode::getLoadTestOpCode(), node, monitorReg, generateS390MemoryReference(objReg, lwOffset, cg),cursor);

   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, startLabel, deps, cursor);

   TR::MemoryReference * tempMR1 = generateS390MemoryReference(cg->machine()->getRealRegister(TR::RealRegister::GPR0),0,cg);

   // use TEND + BRC instead of TABORT for better performance
   cursor = generateSInstruction(cg, TR::InstOpCode::TEND, node, tempMR1, cursor);

   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelTransientFailure, depsTransient, cursor);

   cg->stopUsingRegister(monitorReg);
   cg->decReferenceCount(objNode);
   cg->decReferenceCount(brPersistentNode);
   cg->decReferenceCount(brTransientNode);
   cg->decReferenceCount(fallThrough);
#endif

   return NULL;
   }

/**
 * tfinishEvaluator:  end a transaction
 */
TR::Register *
J9::Z::TreeEvaluator::tfinishEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
#ifndef PUBLIC_BUILD
   TR::MemoryReference * tempMR1 = generateS390MemoryReference(cg->machine()->getRealRegister(TR::RealRegister::GPR0),0,cg);
   TR::Instruction * cursor = generateSInstruction(cg, TR::InstOpCode::TEND, node, tempMR1);
#endif

   return NULL;
   }

/**
 * tabortEvaluator:  abort a transaction
 */
TR::Register *
J9::Z::TreeEvaluator::tabortEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
#ifndef PUBLIC_BUILD
   TR::Instruction *cursor;
   TR::LabelSymbol * labelDone = generateLabelSymbol(cg);
   TR::Register *codeReg = cg->allocateRegister();
   generateRIInstruction(cg, TR::Compiler->target.is64Bit() ? TR::InstOpCode::LGHI : TR::InstOpCode::LHI, node, codeReg, 0);
   //Get the nesting depth
   cursor = generateRREInstruction(cg, TR::InstOpCode::ETND, node, codeReg, codeReg);

   generateRIInstruction(cg, TR::InstOpCode::CHI, node, codeReg, 0);
   //branch on zero to done label
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK8, node, labelDone);
   generateRIInstruction(cg, TR::Compiler->target.is64Bit() ? TR::InstOpCode::LGHI : TR::InstOpCode::LHI, node, codeReg, 0x100);
   TR::MemoryReference *codeMR = generateS390MemoryReference(codeReg, 0, cg);
   cursor = generateSInstruction(cg, TR::InstOpCode::TABORT, node, codeMR);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelDone);
   cg->stopUsingRegister(codeReg);
#endif
   return NULL;
   }

/**
 * \details
 * Resolved and unresolved reference field load get two slightly different sequences.
 *
 * Resolved reference fields load sequence for -XnocompressedRefs:
 * \verbatim
 *
 * Label:   startICF
 * LG   R_obj, Ref_field_MemRef
 *
 * // range check with implicit CS cycle check
 * CLG  R_obj, EvacuateBase(R_vmthread)
 * BRC  COND_BL, doneLabel
 * CLG  R_obj, EvacuateTop(R_vmthread)
 * BRC  COND_BH, doneLabel
 *
 * LAY  R_addr, Ref_field_MemRef
 * BRC  helper_call_snippet
 *
 * Label: jitReadBarrier return label
 * // reload evacuated reference
 * LG   R_obj, 0(R_addr)
 *
 * doneLabel: endICF
 * \endverbatim
 *
 *
 * Unresolved reference fields load sequence for -XnocompressedRefs:
 * \verbatim
 *
 * Label:   startICF
 * LAY  R_addr, Ref_field_MemRef
 * LG   R_obj, 0(R_addr)
 *
 * // range check with implicit CS cycle check
 * CLG  R_obj, EvacuateBase(R_vmthread)
 * BRC  COND_BL, doneLabel
 * CLG  R_obj, EvacuateTop(R_vmthread)
 * BRC  COND_BH, doneLabel
 *
 * BRC  helper_call_snippet
 *
 * Label: jitReadBarrier return label
 * // reload evacuated reference
 * LG   R_obj, 0(R_addr)
 *
 * doneLabel: endICF
 * \endverbatim
 *
 * If compressed pointer is enabled, the LG instructions above are replaced by LLGF+SLLG.
 */
TR::Register *
J9::Z::TreeEvaluator::generateSoftwareReadBarrier(TR::Node* node,
                                                  TR::CodeGenerator* cg,
                                                  TR::Register* resultReg,
                                                  TR::MemoryReference* loadMemRef,
                                                  TR::RegisterDependencyConditions* deps,
                                                  bool produceUnshiftedValue)
   {
   TR::Compilation* comp = cg->comp();
   TR::Register* fieldAddrReg = cg->allocateRegister();
   TR::RealRegister* raReg   = cg->machine()->getRealRegister(cg->getReturnAddressRegister());
   bool isCompressedRef = comp->useCompressedPointers();

   if (!isCompressedRef)
      {
      TR::TreeEvaluator::checkAndSetMemRefDataSnippetRelocationType(node, cg, loadMemRef);
      }

   const bool fieldUnresolved = node->getSymbolReference()->isUnresolved();
   if (comp->getOption(TR_TraceCG))
      {
      traceMsg(comp, "SoftwareReadBarrier: symbol is %s. Compr shift %d. RA reg: %s Entry reg %s\n",
               fieldUnresolved ? "unresolved" : "resolved",
               TR::Compiler->om.compressedReferenceShift(),
               raReg->getRegisterName(comp),
               cg->getEntryPointRealRegister()->getRegisterName(comp));
      }

   bool notInsideICF = (deps == NULL);
   if (notInsideICF)
      {
      deps = generateRegisterDependencyConditions(0, 6, cg);
      TR::LabelSymbol* startICFLabel = generateLabelSymbol(cg);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, startICFLabel);
      startICFLabel->setStartInternalControlFlow();
      }

   TR::Register* dummyRegForRA = cg->allocateRegister();
   TR::Register* dummyRegForEntry = cg->allocateRegister();
   dummyRegForRA->setPlaceholderReg();
   dummyRegForEntry->setPlaceholderReg();

   deps->addPostCondition(resultReg, TR::RealRegister::AssignAny);
   deps->addPostCondition(fieldAddrReg, TR::Compiler->target.isLinux() ? TR::RealRegister::GPR3 : TR::RealRegister::GPR2);
   deps->addPostCondition(dummyRegForRA, cg->getReturnAddressRegister());
   deps->addPostCondition(dummyRegForEntry, cg->getEntryPointRegister());

   cg->stopUsingRegister(dummyRegForRA);
   cg->stopUsingRegister(dummyRegForEntry);

   int32_t shiftAmount = TR::Compiler->om.compressedReferenceShift();
   bool shouldShift = (shiftAmount != 0) && !produceUnshiftedValue;
   TR::InstOpCode::Mnemonic loadOpCode = isCompressedRef ? TR::InstOpCode::LLGF: TR::InstOpCode::LG;

   if (fieldUnresolved)
      {
      generateRXInstruction(cg, TR::InstOpCode::LA, node, fieldAddrReg, loadMemRef);
      generateRXInstruction(cg, loadOpCode, node, resultReg, generateS390MemoryReference(fieldAddrReg, 0, cg));
      }
   else
      {
      generateRXInstruction(cg, loadOpCode, node, resultReg, loadMemRef);
      }

   deps->addAssignAnyPostCondOnMemRef(loadMemRef);

   TR::Register* vmReg = cg->getLinkage()->getMethodMetaDataRealRegister();

   TR::MemoryReference* baseMemRef = generateS390MemoryReference(vmReg, TR::Compiler->vm.thisThreadGetEvacuateBaseAddressOffset(comp), cg);
   TR::MemoryReference* topMemRef  = generateS390MemoryReference(vmReg, TR::Compiler->vm.thisThreadGetEvacuateTopAddressOffset(comp), cg);

   // Range check with implicit software CS status check.
   TR::LabelSymbol* doneLabel = generateLabelSymbol(cg);
   generateRXInstruction(cg, comp->useCompressedPointers() ? TR::InstOpCode::CL : TR::InstOpCode::CLG, node, resultReg, baseMemRef);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, node, doneLabel);
   generateRXInstruction(cg, comp->useCompressedPointers() ? TR::InstOpCode::CL : TR::InstOpCode::CLG, node, resultReg, topMemRef);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BH, node, doneLabel);
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(cg->comp(), "readBar/helperCall"), 1, TR::DebugCounter::Cheap);
   if (!fieldUnresolved)
      {
      generateRXInstruction(cg, TR::InstOpCode::LA, node, fieldAddrReg, generateS390MemoryReference(*loadMemRef, 0, cg));
      }

   TR::LabelSymbol* callLabel = generateLabelSymbol(cg);
   TR::LabelSymbol* callEndLabel = generateLabelSymbol(cg);
   TR::Instruction *gcPoint = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, callLabel);
   gcPoint->setNeedsGCMap(0);
   auto readBarHelperSnippet = new (cg->trHeapMemory()) TR::S390HelperCallSnippet(cg, node, callLabel,
                                                                                  cg->symRefTab()->findOrCreateRuntimeHelper(TR_softwareReadBarrier, false, false, false),
                                                                                  callEndLabel);
   cg->addSnippet(readBarHelperSnippet);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, callEndLabel);

   // Reload the object after helper call.
   generateRXInstruction(cg, loadOpCode, node, resultReg, generateS390MemoryReference(fieldAddrReg, 0, cg));
   TR::Instruction* cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneLabel);
   if (notInsideICF)
      {
      cursor->setDependencyConditions(deps);
      doneLabel->setEndInternalControlFlow();
      }

   // produce decompressed value in the end
   if (shouldShift)
      {
      generateRSInstruction(cg, TR::InstOpCode::SLLG, node, resultReg, resultReg, shiftAmount);
      }

   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(cg->comp(), "readBar/total"), 1, TR::DebugCounter::Cheap);
   cg->stopUsingRegister(fieldAddrReg);

   return resultReg;
   }

TR::Register *
J9::Z::TreeEvaluator::arraycopyEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   if (node->isReferenceArrayCopy())
      {
      TR::TreeEvaluator::referenceArraycopyEvaluator(node, cg);
      }
   else
      {
      OMR::TreeEvaluatorConnector::arraycopyEvaluator(node, cg);
      }
   return NULL;
   }

TR::Register *
J9::Z::TreeEvaluator::referenceArraycopyEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node* byteSrcObjNode = node->getChild(0);
   TR::Node* byteDstObjNode = node->getChild(1);
   TR::Node* byteSrcNode    = node->getChild(2);
   TR::Node* byteDstNode    = node->getChild(3);
   TR::Node* byteLenNode    = node->getChild(4);

   TR::Register* byteSrcObjReg = cg->evaluate(byteSrcObjNode);
   TR::Register* byteDstObjReg = cg->evaluate(byteDstObjNode);

   if (!node->chkNoArrayStoreCheckArrayCopy())
      {
      TR::Register* byteSrcReg = cg->evaluate(byteSrcNode);
      TR::Register* byteDstReg = cg->evaluate(byteDstNode);
      TR::Register* byteLenReg = cg->evaluate(byteLenNode);

      genArrayCopyWithArrayStoreCHK(node, byteSrcObjReg, byteDstObjReg, byteSrcReg, byteDstReg, byteLenReg, cg);

      cg->decReferenceCount(byteSrcNode);
      cg->decReferenceCount(byteDstNode);
      cg->decReferenceCount(byteLenNode);
      }
   else
      {
      TR_ASSERT_FATAL(node->getArrayCopyElementType() == TR::Address, "Reference arraycopy element type should be TR::Address but was '%s'", node->getArrayCopyElementType().toString());
      primitiveArraycopyEvaluator(node, cg, byteSrcNode, byteDstNode, byteLenNode);
      genWrtbarForArrayCopy(node, byteSrcObjReg, byteDstObjReg, byteSrcNode->isNonNull(), cg);
      }

   cg->decReferenceCount(byteSrcObjNode);
   cg->decReferenceCount(byteDstObjNode);
   return NULL;
   }

void
J9::Z::TreeEvaluator::forwardArrayCopySequenceGenerator(TR::Node *node, TR::CodeGenerator *cg,
                                                        TR::Register *byteSrcReg, TR::Register *byteDstReg,
                                                        TR::Register *byteLenReg, TR::Node *byteLenNode,
                                                        TR_S390ScratchRegisterManager *srm, TR::LabelSymbol *mergeLabel)
   {
   bool mustGenerateOOLGuardedLoadPath = TR::Compiler->om.readBarrierType() != gc_modron_readbar_none &&
                                         node->getArrayCopyElementType() == TR::Address;
   if (mustGenerateOOLGuardedLoadPath)
      {
      // It might be possible that we have constant byte length load and it is forward array copy.
      // In this case if we need to do guarded Load then need to evaluate byteLenNode.
      if (byteLenReg == NULL)
         byteLenReg = cg->gprClobberEvaluate(byteLenNode);
      TR::TreeEvaluator::genGuardedLoadOOL(node, cg, byteSrcReg, byteDstReg, byteLenReg, mergeLabel, srm, true);
      }

   OMR::TreeEvaluatorConnector::forwardArrayCopySequenceGenerator(node, cg, byteSrcReg, byteDstReg, byteLenReg, byteLenNode, srm, mergeLabel);
   }

TR::RegisterDependencyConditions *
J9::Z::TreeEvaluator::backwardArrayCopySequenceGenerator(TR::Node *node, TR::CodeGenerator *cg,
                                                         TR::Register *byteSrcReg, TR::Register *byteDstReg,
                                                         TR::Register *byteLenReg, TR::Node *byteLenNode,
                                                         TR_S390ScratchRegisterManager *srm, TR::LabelSymbol *mergeLabel)
   {
   bool mustGenerateOOLGuardedLoadPath = TR::Compiler->om.readBarrierType() != gc_modron_readbar_none &&
                                         node->getArrayCopyElementType() == TR::Address;
   if (mustGenerateOOLGuardedLoadPath)
      {
      TR::TreeEvaluator::genGuardedLoadOOL(node, cg, byteSrcReg, byteDstReg, byteLenReg, mergeLabel, srm, false);
      }

   return OMR::TreeEvaluatorConnector::backwardArrayCopySequenceGenerator(node, cg, byteSrcReg, byteDstReg, byteLenReg, byteLenNode, srm, mergeLabel);
   }

void
J9::Z::TreeEvaluator::generateLoadAndStoreForArrayCopy(TR::Node *node, TR::CodeGenerator *cg,
                                                       TR::MemoryReference *srcMemRef, TR::MemoryReference *dstMemRef,
                                                       TR_S390ScratchRegisterManager *srm,
                                                       TR::DataType elenmentType, bool needsGuardedLoad,
                                                       TR::RegisterDependencyConditions* deps)

   {
   if ((node->getArrayCopyElementType() == TR::Address)
           && needsGuardedLoad
           && (!TR::Compiler->target.cpu.getSupportsGuardedStorageFacility()))
      {
      TR::Register* resultReg = srm->findOrCreateScratchRegister();
      TR::TreeEvaluator::generateSoftwareReadBarrier(node, cg, resultReg, srcMemRef, deps, true);
      TR::InstOpCode::Mnemonic storeOp = TR::InstOpCode::ST;
      if (TR::Compiler->target.is64Bit() && !cg->comp()->useCompressedPointers())
         {
         storeOp = TR::InstOpCode::STG;
         }

      generateRXInstruction(cg, storeOp, node, resultReg, dstMemRef);
      srm->reclaimScratchRegister(resultReg);
      }
   else
      {
      OMR::TreeEvaluatorConnector::generateLoadAndStoreForArrayCopy(node, cg, srcMemRef, dstMemRef, srm, elenmentType, needsGuardedLoad, deps);
      }
   }
