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

#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "env/CompilerEnv.hpp"
#include "il/DataTypes.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/NodePool.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "p/codegen/GenerateInstructions.hpp"
#include "p/codegen/PPCEvaluator.hpp"
#include "p/codegen/PPCInstruction.hpp"
#include "env/VMJ9.h"

extern TR::Instruction *loadConstant(TR::CodeGenerator *cg, TR::Node * node, int32_t value,
                              TR::Register *trgReg, TR::Instruction *cursor, bool isPicSite);
extern TR::Instruction *loadConstant(TR::CodeGenerator *cg, TR::Node * node, int64_t value,
                              TR::Register *trgReg, TR::Instruction *cursor, bool isPicSite);

// used when querying VM for BigDecimal dfp field
static int16_t dfpFieldOffset = -1; // initialize to an illegal val

/*
 * The IEEE DFP default rounding mode is HALF_EVEN = 0x0 (for 32, 64 & 128 bit)
 * The rounding is modified only when the user-supplied rounding mode
 * differs from this default.  User supplied rounding mode can be one
 * of the ones listed below.
*/

static const int16_t rmIEEEdefault = 0x0;
static const int8_t rmRPSP = 0x7; //round to prepare for shorter precision

/* following used to make sure DFP is: (used for precision-based operations)
   zero with non-extreme exponent
   normal with non-extreme exponent and Leftmost zero
   normal with non-extreme exponent and Leftmost nonzero
*/
static const uint32_t tgdtLaxTest = 0x26;

/* following used to make sure DFP is: (used for scale-based operations)
   zero with non-extreme exponent
   normal with non-extreme zero and Leftmost zero
*/
static const uint32_t tgdtScaledTest = 0x24;
static const uint32_t dfpRMBitField = 7; //used for BF field
static const char * fieldName = "laside";
static const char * sig = "J";
static const uint32_t fieldLen = 6;
static const uint32_t sigLen = 1;
static const char * className = "Ljava/math/BigDecimal;";
static const int32_t len = 22;

extern bool loadAndEvaluateAsDouble(
      TR::Node* node,
      TR::Register*& reg,
      TR::CodeGenerator* cg)
   {
   if (node->getOpCodeValue() == TR::lloadi &&
      !node->getRegister() && node->getReferenceCount() == 1)
      {
      // change the opcode to indirect load double
      TR::Node::recreate(node, TR::dloadi);

      // evaluate into an FPR
      reg = cg->evaluate(node);
      cg->decReferenceCount(node);
      return true;
      }
   return false;
   }

static void overlapDFPOperandAndPrecisionLoad(
      TR::Node* node,
      TR::Node* lhsNode,
      TR::Node* rhsNode,
      TR::Node* precChild,
      TR::Register*& lhsFPRegister,
      TR::Register*& rhsFPRegister,
      TR::Register*& precFPRegister,
      int32_t toRound,
      bool isConst16Precision,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();

   // check for direct move
   bool p8DirectMoveTest = comp->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P8);

   /* We want to overlap the loading of the DFP operands and precision...
      Cases will be..

        use direct lfd if uncommoned, ref count = 1
        64-bit: mtvsrdLHS, mtvsrdRHS, mtvsrdPrecision
        32-bt:  Store-LoadLHS, Store-LoadRHS, Store-LoadPrec, (storeLHS, storeRHS, storePrec, loadPrec, loadRHS, loadLHS)
    */

   // Handle the LHS and RHS
   bool lhsLoaded = loadAndEvaluateAsDouble(lhsNode, lhsFPRegister, cg);
   bool rhsLoaded = loadAndEvaluateAsDouble(rhsNode, rhsFPRegister, cg);

   // if couldn't lfd either of them, overlap the move to FPRs
   if (comp->target().is64Bit() && p8DirectMoveTest)
      {
      if (!lhsLoaded)
         {
         lhsFPRegister = cg->allocateRegister(TR_FPR);
         generateTrg1Src1Instruction(cg, TR::InstOpCode::mtvsrd, node, lhsFPRegister, cg->evaluate(lhsNode));
         cg->decReferenceCount(lhsNode);
         }

      if (!rhsLoaded)
         {
         rhsFPRegister = cg->allocateRegister(TR_FPR);
         generateTrg1Src1Instruction(cg, TR::InstOpCode::mtvsrd, node, rhsFPRegister, cg->evaluate(rhsNode));
         cg->decReferenceCount(rhsNode);
         }

      if (toRound == 1 && !isConst16Precision)
         {
         precFPRegister = cg->allocateRegister(TR_FPR);
         generateTrg1Src1Instruction(cg, TR::InstOpCode::mtvsrd, node, precFPRegister, cg->evaluate(precChild));
         }
      }
   else // in 32-bit mode or if there is no direct move, overlap the Store-Load sequence
      {

      TR::SymbolReference *tempLHSSymRef = NULL;
      TR::MemoryReference *tempLHSMR = NULL;
      TR::Register *lhsRegister = NULL;

      if (!lhsLoaded)
         {
         lhsRegister = cg->evaluate(lhsNode);
         cg->decReferenceCount(lhsNode);

         // need to store each register word into mem & then load
         tempLHSSymRef = cg->allocateLocalTemp(TR::Int64);
         tempLHSMR = TR::MemoryReference::createWithSymRef(cg, node, tempLHSSymRef, 8);
         if (comp->target().is32Bit())
            {
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempLHSMR, lhsRegister->getHighOrder());
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node,
               TR::MemoryReference::createWithMemRef(cg, node, *tempLHSMR, 4, 4), lhsRegister->getLowOrder());
            }
         else // !p8DirectMoveTest
            generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, tempLHSMR, lhsRegister);
         }

      TR::SymbolReference *tempRHSSymRef = NULL;
      TR::MemoryReference *tempRHSMR = NULL;
      TR::Register *rhsRegister = NULL;
      TR::SymbolReference * tempPrecSymRef= NULL;

      if (!rhsLoaded)
         {
         rhsRegister = cg->evaluate(rhsNode);
         cg->decReferenceCount(rhsNode);

         // need to store each register word into mem & then load
         tempRHSSymRef = cg->allocateLocalTemp(TR::Int64);
         tempRHSMR = TR::MemoryReference::createWithSymRef(cg, node, tempRHSSymRef, 8);
         if (comp->target().is32Bit())
            {
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempRHSMR, rhsRegister->getHighOrder());
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node,
               TR::MemoryReference::createWithMemRef(cg, node, *tempRHSMR, 4, 4), rhsRegister->getLowOrder());
            }
         else
            generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, tempRHSMR, rhsRegister);
         }

      if (toRound == 1 && !isConst16Precision)
         {
         // move 32-bit sint precision over to FPR
         precFPRegister = cg->allocateRegister(TR_FPR);

         // store register word to mem
         tempPrecSymRef = cg->allocateLocalTemp(TR::Int64);
         TR::MemoryReference * tempMR = TR::MemoryReference::createWithSymRef(cg, node, tempPrecSymRef, 8);
         generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, tempMR, cg->evaluate(precChild));
         }

      // now finish off the Store-Load sequence in REVERSE!
      if (!lhsLoaded || !rhsLoaded || toRound == 1)
         {
         cg->generateGroupEndingNop(node);
         }

      if (toRound == 1 && !isConst16Precision)
         {
         generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, precFPRegister,
            TR::MemoryReference::createWithSymRef(cg, node, tempPrecSymRef, 8));
         }

      if (!rhsLoaded)
         {
         rhsFPRegister = cg->allocateRegister(TR_FPR);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, rhsFPRegister,
            TR::MemoryReference::createWithSymRef(cg, node, tempRHSSymRef, 8));
         }

      if (!lhsLoaded)
         {
         lhsFPRegister = cg->allocateRegister(TR_FPR);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, lhsFPRegister,
            TR::MemoryReference::createWithSymRef(cg, node, tempLHSSymRef, 8));
         }
      }
   }



/*
 * DFP instructions are only supported on p6, which has 64-bit registers.
 * If running a 32-bit JVM, need to store GPR register words into memory and then
 * materialize them in 64-bit FPRs.
 */

/*
 * IEEE Exception Enable Bits:
 * Never check or alter these bits.  Document that we expect all exceptions to remain disabled,
 * and that we will not support code that changes this.
 * We need these bits to be off when executing DFP instructions so that the classes of the results can
 * be recorded in FPSCR_fprf.
 */

static void genSetDFPRoundingMode(
      TR::Node * node,
      TR::CodeGenerator *cg,
      int16_t rm)
   {
   generateImm2Instruction(cg, TR::InstOpCode::mtfsfi, node, rm, dfpRMBitField);
   }

static void genSetDFPRoundingMode(
      TR::Node * node,
      TR::CodeGenerator *cg,
      TR::Register *rmRegister)
   {
   /* We want to copy bits 28 to 31 of an FPR containing the rounding mode to field 7 in FPSCR
      In 64-bit mode, need to get bits 60 to 63 into proper position 28 to 31...
   */
   TR::Register * fpRegister = cg->allocateRegister(TR_FPR);
   if (cg->comp()->target().is64Bit())
      {
      // shift left by 32 bits
      TR::Register * newRMRegister = cg->allocateRegister();
      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicr, node, newRMRegister, rmRegister, 32, CONSTANT64(0xFFFFFFFF00000000));
      generateMvFprGprInstructions(cg, node, gpr2fprHost64, false, fpRegister, newRMRegister);
      cg->stopUsingRegister(newRMRegister);
      }
   else
      {
      // store into first 32-bits of 64-bit memory, then lfd into an fpr
      TR::SymbolReference * tempSymRef = cg->allocateLocalTemp(TR::Int64);
      TR::MemoryReference * tempMR = TR::MemoryReference::createWithSymRef(cg, node, tempSymRef, 8);
      generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempMR, rmRegister);

      cg->generateGroupEndingNop(node);

      generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, fpRegister,
          TR::MemoryReference::createWithSymRef(cg, node, tempSymRef, 8));
      }

   generateSrc1Instruction(cg, TR::InstOpCode::mtfsfw, node, fpRegister, 0x1);
   cg->stopUsingRegister(fpRegister);
   }

static void genTestDataGroup(
      TR::Node * node,
      TR::CodeGenerator *cg,
      TR::Register *fprRegister,
      TR::Register * crRegister,
      int32_t grp)
   {
   /* The grp is a 6-bit field in the instruction...
      It is being placed in the last 16 bits of the instruction
      when encoded... so let's left shift by 10 bits...
    */
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::dtstdg, node, crRegister, fprRegister, grp);
   }

static void genClearFPSCRBits(
      TR::Node *node,
      TR::CodeGenerator *cg,
      int32_t bf)
   {
   // Clear bit field bf in the 64-bit FPSCR
   generateImm2Instruction(cg, TR::InstOpCode::mtfsfi, node, 0x0, bf);
   }

static void genStoreDFP(
      TR::Node *node,
      TR::CodeGenerator *cg,
      TR::Register * objRegister,
      TR::SymbolReference * dfpFieldSymbolReference,
      TR::Register * fprDFPRegister)
   {
   /*
    * objAddrNode represent an aload of the result BigDecimal we
    * are storing the result to... need to query VM for offset of
    * field we wish to store to
    */
   TR::Compilation* comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   // query the VM for the field offset to which we are going to store
   if (dfpFieldOffset == -1)
      {
      TR_OpaqueClassBlock * bigDClass = NULL;
      bigDClass =
         fej9->getClassFromSignature((char*)className, len,
            node->getSymbolReference()->getOwningMethod(comp), true);

      int16_t computedDfpFieldOffset =
         fej9->getInstanceFieldOffset(bigDClass, (char*)fieldName,
            fieldLen, (char*)sig, sigLen);
      if (computedDfpFieldOffset == -1)
         {
         TR_ASSERT(false, "offset for 'laside' field not found\n");
         comp->failCompilation<TR::CompilationException>("offset for dfp field is unknown, abort compilation\n");
         }
      dfpFieldOffset = computedDfpFieldOffset + fej9->getObjectHeaderSizeInBytes();  // size of a J9 object header to move past it
      }

   // need a hand-crafted memref
   TR::MemoryReference * tempMR = TR::MemoryReference::createWithDisplacement(cg, objRegister, dfpFieldOffset, 8);

   // store to memory
   generateMemSrc1Instruction(cg, TR::InstOpCode::stfd, node, tempMR, fprDFPRegister);
   }
