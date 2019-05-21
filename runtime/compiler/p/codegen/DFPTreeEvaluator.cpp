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

#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "env/CompilerEnv.hpp"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "il/NodePool.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/LabelSymbol.hpp"
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
static const char * className ="Ljava/math/BigDecimal;\0";
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
   // check for direct move
   bool p8DirectMoveTest = TR::Compiler->target.cpu.id() == TR_PPCp8;

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
   if (TR::Compiler->target.is64Bit() && p8DirectMoveTest)
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
         tempLHSMR = new (cg->trHeapMemory()) TR::MemoryReference(node, tempLHSSymRef, 8, cg);
         if (TR::Compiler->target.is32Bit())
            {
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempLHSMR, lhsRegister->getHighOrder());
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node,
               new (cg->trHeapMemory()) TR::MemoryReference(node, *tempLHSMR, 4, 4, cg), lhsRegister->getLowOrder());
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
         tempRHSMR = new (cg->trHeapMemory()) TR::MemoryReference(node, tempRHSSymRef, 8, cg);
         if (TR::Compiler->target.is32Bit())
            {
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempRHSMR, rhsRegister->getHighOrder());
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node,
               new (cg->trHeapMemory()) TR::MemoryReference(node, *tempRHSMR, 4, 4, cg), rhsRegister->getLowOrder());
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
         TR::MemoryReference * tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, tempPrecSymRef, 8, cg);
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
            new (cg->trHeapMemory()) TR::MemoryReference(node, tempPrecSymRef, 8, cg));
         }

      if (!rhsLoaded)
         {
         rhsFPRegister = cg->allocateRegister(TR_FPR);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, rhsFPRegister,
            new (cg->trHeapMemory()) TR::MemoryReference(node, tempRHSSymRef, 8, cg));
         }

      if (!lhsLoaded)
         {
         lhsFPRegister = cg->allocateRegister(TR_FPR);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, lhsFPRegister,
            new (cg->trHeapMemory()) TR::MemoryReference(node, tempLHSSymRef, 8, cg));
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
   if (TR::Compiler->target.is64Bit())
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
      TR::MemoryReference * tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, tempSymRef, 8, cg);
      generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempMR, rmRegister);

      cg->generateGroupEndingNop(node);

      generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, fpRegister,
          new (cg->trHeapMemory()) TR::MemoryReference(node, tempSymRef, 8, cg));
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
   TR::MemoryReference * tempMR = new (cg->trHeapMemory()) TR::MemoryReference(objRegister, dfpFieldOffset, 8, cg);

   // store to memory
   generateMemSrc1Instruction(cg, TR::InstOpCode::stfd, node, tempMR, fprDFPRegister);
   }

static TR::Register *inlineBigDecimalConstructor64(
      TR::Node *node,
      TR::CodeGenerator *cg,
      bool isLong,
      bool exp)
   {
   bool p8DirectMoveTest = TR::Compiler->target.cpu.id() == TR_PPCp8;

   // second child is an expression representing the value to be converted to DFP
   TR::Register * valRegister = cg->evaluate(node->getSecondChild());
   cg->decReferenceCount(node->getSecondChild());

   TR::Register * dfpRegister = NULL;

   // sign extend if not long
   if (!isLong)
      generateTrg1Src1Instruction(cg, TR::InstOpCode::extsw, node, valRegister, valRegister);

   // do we have an exponent?
   TR::Register * expRegister = NULL;
   if (exp)
      {
      // the third child will be an expression representing
      // a 32-bit sint (biased exponent in the DFP format)
      expRegister = cg->evaluate(node->getChild(2));
      cg->decReferenceCount(node->getChild(2));
      }

   // are we going to round?
   int32_t toRound=0;

   //store precision and rounding mode nodes
   TR::Node * roundNode = NULL;
   TR::Node * rmNode = NULL;
   TR::Node * precNode = NULL;

   // see if we need to perform rounding for longexp constructor
   if (exp)
      {
      roundNode = node->getChild(3);
      precNode = node->getChild(4);
      rmNode = node->getChild(5);
      }
   else
      {
      roundNode = node->getChild(2);
      precNode = node->getChild(3);
      rmNode = node->getChild(4);
      }

   // check the rounding constant
   TR_ASSERT(roundNode->getOpCode().isLoadConst(), "inlineBigDecimalConstructor64: Unexpected Type\n");
   toRound = roundNode->getInt();
   if (toRound == 0)
      genSetDFPRoundingMode(node, cg, rmRPSP);

   TR::Register * precRegister = NULL;
   if (toRound == 1)
      precRegister = cg->evaluate(precNode);

   /*  Now a sequence of back2back direct moves if it is supported (generateMvFprGprInstructions() does this), otherwise Load-Store-Hit */

   // move value, exponent and precision over to FPRs
   dfpRegister = cg->allocateRegister(TR_FPR);
   TR::Register * expFPRegister = NULL;
   TR::Register * precFPRegister = NULL;
   if (exp)
      expFPRegister = cg->allocateRegister(TR_FPR);
   if (toRound == 1)
      precFPRegister = cg->allocateRegister(TR_FPR);

   // Direct move path for POWER 8 or newer processors
   if (p8DirectMoveTest)
      {
      // load the exponent first to avoid sign extension penalty
      if (exp)
         {
         generateTrg1Src1Instruction(cg, TR::InstOpCode::mtvsrwz, node, expFPRegister, expRegister);
         }
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mtvsrd, node, dfpRegister, valRegister);
      if (toRound == 1)
         generateTrg1Src1Instruction(cg, TR::InstOpCode::mtvsrd, node, precFPRegister, precRegister);
      }
   else
      {
      TR::MemoryReference * tempExpMR = NULL;
      TR::SymbolReference * tempValSymRef = NULL;
      TR::MemoryReference * tempValMR = NULL;
      TR::SymbolReference * tempPrecSymRef = NULL;

      // (1) prep the exponent for the Store-Load sequence
      if (exp)
         {
         tempExpMR = new (cg->trHeapMemory()) TR::MemoryReference(cg->getStackPointerRegister(), -4, 4, cg);
         tempExpMR->forceIndexedForm(node, cg);

         // touch memory before performing Store-Load
         generateTrg1MemInstruction(cg, TR::InstOpCode::lfiwax, node, expFPRegister, tempExpMR);

         // store 32-bits into stack slot
         generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(node, *tempExpMR, 0, 4, cg), expRegister);
         }

      // (2) prep the value for the Store-Load sequence
      // NOTE:  We don't differentiate between isLong or !isLong since we've
      // sign extended above - so we'll need to prep both halves of the register
      tempValSymRef = cg->allocateLocalTemp(TR::Int64);
      tempValMR = new (cg->trHeapMemory()) TR::MemoryReference(node, tempValSymRef, 8, cg);
      generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, tempValMR, valRegister);

      // (3) prep the precision for the Store-Load sequence
      if (toRound == 1)
         {
         tempPrecSymRef = cg->allocateLocalTemp(TR::Int64);
         TR::MemoryReference * tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, tempPrecSymRef, 8, cg);
         generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, tempMR, precRegister);
         }

      // now complete the Store-Load sequences in REVERSE!
      cg->generateGroupEndingNop(node);

      // complete the Store-Load sequence for precision
      if (toRound == 1)
         {
         generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, precFPRegister,
             new (cg->trHeapMemory()) TR::MemoryReference(node, tempPrecSymRef, 8, cg));
         }

      // complete the Store-Load sequence for value
      generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, dfpRegister,
            new (cg->trHeapMemory()) TR::MemoryReference(node, tempValSymRef, 8, cg));

      // complete the Store-Load sequence for exponent
      if (exp)
         {
         generateTrg1MemInstruction(cg, TR::InstOpCode::lfiwax, node, expFPRegister, new (cg->trHeapMemory()) TR::MemoryReference(node, *tempExpMR, 0, 4, cg));
         tempExpMR->decNodeReferenceCounts(cg);
         }
      }

   /* Now convert to DFP, insert exponent and round */
   int inBCDForm = 0;
   if (exp)
      {
      TR_ASSERT(node->getChild(6)->getOpCode().isLoadConst(), "inlineBigDecimalConstructor64: Unexpected Type\n");
      inBCDForm = node->getChild(6)->getInt();
      cg->decReferenceCount(node->getChild(6));
      }

   // convert to DFP
   if (!inBCDForm)
      {
      // Create a forced FPR register pair
      // NOTE:  We use fp14, fp15 because they are the last used during GRA, and are both volatile
      TR::Register * fp14Reg= cg->allocateRegister(TR_FPR);
      TR::Register * fp15Reg= cg->allocateRegister(TR_FPR);
      TR::RegisterDependencyConditions * dcffixqDeps =
         new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg->trMemory());
      dcffixqDeps->addPostCondition(fp14Reg, TR::RealRegister::fp14);
      dcffixqDeps->addPostCondition(fp15Reg, TR::RealRegister::fp15);

      // generate a dcffixq into a target FPR register pair
      generateTrg1Src1Instruction(cg, TR::InstOpCode::dcffixq, node, fp14Reg, dfpRegister);

      // generate a drdpq from a source FPR register pair into the even of the FPR pair
      generateTrg1Src1Instruction(cg, TR::InstOpCode::drdpq, node, fp14Reg, fp14Reg);

      // generate an FPR move from the even of the pair to dfpRegister
      // and stop using the FPR register pair
      generateTrg1Src1Instruction(cg, TR::InstOpCode::fmr, node, dfpRegister, fp14Reg);

      TR::LabelSymbol *depLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);  // somewhere to hang the dependencies
      generateDepLabelInstruction (cg, TR::InstOpCode::label, node, depLabel, dcffixqDeps);

      cg->stopUsingRegister(fp14Reg);
      cg->stopUsingRegister(fp15Reg);
      }
   else // convert bcd to dfp
      generateTrg1Src1Instruction(cg, TR::InstOpCode::denbcdu, node, dfpRegister, dfpRegister);

   if (exp)
      {
      generateTrg1Src2Instruction(cg, TR::InstOpCode::diex, node, dfpRegister, expFPRegister, dfpRegister);
      cg->stopUsingRegister(expFPRegister);
      }

   // to store the return values
   TR::Register * retRegister = cg->allocateRegister();

   if (toRound == 1)
      {
      TR::Register * rmRegister = cg->evaluate(rmNode);
      genSetDFPRoundingMode(node, cg, rmRegister);

      //perform the round
      generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::drrnd, node, dfpRegister, precFPRegister, dfpRegister, 0x3);
      cg->stopUsingRegister(precFPRegister);
      }

   // need TGDT only when rounding
   if (toRound == 1 )
      {
      // End label
      TR::LabelSymbol * labelEND = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

      // Test the data group and branch instruction
      TR::Register *crRegister = cg->allocateRegister(TR_CCR);
      genTestDataGroup(node, cg, dfpRegister, crRegister, tgdtLaxTest);

      // did we fail? - original non rounded value loaded earlier
      loadConstant(cg, node, 0, retRegister);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, labelEND, crRegister);

      // did we pass? - load the rounded value
      loadConstant(cg, node, 1, retRegister);
      genStoreDFP(node, cg, cg->evaluate(node->getFirstChild()), NULL, dfpRegister);
      cg->decReferenceCount(node->getFirstChild());

      // which registers do we need alive after branching sequence?
      TR::RegisterDependencyConditions * deps =
         new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg->trMemory());
      deps->addPostCondition(retRegister, TR::RealRegister::NoReg);
      deps->addPostCondition(dfpRegister, TR::RealRegister::NoReg);
      deps->addPostCondition(crRegister, TR::RealRegister::cr0);
      deps->addPostCondition(node->getFirstChild()->getRegister(), TR::RealRegister::NoReg);
      deps->getPostConditions()->getRegisterDependency(deps->getAddCursorForPost() - 1)->setExcludeGPR0();
      cg->stopUsingRegister(dfpRegister);
      cg->stopUsingRegister(crRegister);

      // end
      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, labelEND, deps);
      }
   else
      {
      loadConstant(cg, node, 1, retRegister);
      genStoreDFP(node, cg, cg->evaluate(node->getFirstChild()), NULL, dfpRegister);
      cg->decReferenceCount(node->getFirstChild());
      cg->stopUsingRegister(dfpRegister);
      }

   // restore appropriate rounding mode
   if (toRound == 0 || toRound == 1)
      genSetDFPRoundingMode(node, cg, rmIEEEdefault);

   cg->decReferenceCount(roundNode);
   cg->decReferenceCount(precNode);
   cg->decReferenceCount(rmNode);

   node->setRegister(retRegister);
   return retRegister;
   }

static TR::Register *inlineBigDecimalConstructor32(
      TR::Node *node,
      TR::CodeGenerator *cg,
      bool isLong,
      bool exp)
   {

   // Load the value into a GPR, move to FPR and convert it to DFP
   TR::Register * dfpRegister = NULL;
   TR::Register * valRegister = cg->evaluate(node->getSecondChild());
   cg->decReferenceCount(node->getSecondChild());

   TR::MemoryReference *tempMR = NULL;
   TR::SymbolReference * temp = NULL;

   /* We want to overlap as many loads and stores as possible...
      So we need to special case...

      value + exp
      value + exp
      value

      We split each up into stores, and then loads...
      store value
      store exp
      load exp
      load value
    */

   // prep the value for the Store-Load sequence...
   TR::MemoryReference * tempValMR = NULL;
   TR::MemoryReference * tempValMR2 = NULL;
   TR::MemoryReference * tempValMR3 = NULL;
   TR::SymbolReference * tempValSymRef = NULL;

   dfpRegister = cg->allocateRegister(TR_FPR);
   if(!isLong)
      {
      // setup for store 32-bits into stack slot
      tempValMR = new (cg->trHeapMemory()) TR::MemoryReference(cg->getStackPointerRegister(), -4, 4, cg);
      tempValMR2 = new (cg->trHeapMemory()) TR::MemoryReference(node, *tempValMR, 0, 4, cg);
      tempValMR3 = new (cg->trHeapMemory()) TR::MemoryReference(node, *tempValMR, 0, 4, cg);
      tempValMR3->forceIndexedForm(node, cg);
      tempValMR->forceIndexedForm(node, cg);

      // touch memory for warmup
      generateTrg1MemInstruction(cg, TR::InstOpCode::lfiwax, node, dfpRegister, tempValMR);

      // store 32-bits into stack slot
      generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempValMR2, valRegister);
      }
   else
      {
      // otherwise, store each register half and the load into FPR
      // unfortunately we can't use 390 shift left + load trick since
      // 32-bit AIX (and possibly Linux) doesn't guarantee
      // saving left half of 64-bit regs
      tempValSymRef = cg->allocateLocalTemp(TR::Int64);
      tempValMR = new (cg->trHeapMemory()) TR::MemoryReference(node, tempValSymRef, 8, cg);
      generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempValMR, valRegister->getHighOrder());
      generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node,
            new (cg->trHeapMemory()) TR::MemoryReference(node, *tempValMR, 4, 4, cg), valRegister->getLowOrder());
      }

   // prep the exponent for the Store-Load sequence
   TR::Register * expRegister = NULL;
   TR::Register * expFPRegister = NULL;
   TR::MemoryReference * tempExpMR = NULL;
   if (exp)
      {
      expFPRegister = cg->allocateRegister(TR_FPR);

      // the third child will be an expression representing
      // a 32-bit sint (biased exponent in the DFP format)
      expRegister = cg->evaluate(node->getChild(2));
      cg->decReferenceCount(node->getChild(2));

      // setup for store 32-bits into stack slot
      tempExpMR = new (cg->trHeapMemory()) TR::MemoryReference(cg->getStackPointerRegister(), -4, 4, cg);
      tempExpMR->forceIndexedForm(node, cg);

      // touch memory before performing Store-Load
      generateTrg1MemInstruction(cg, TR::InstOpCode::lfiwax, node, expFPRegister, tempExpMR);

      // store 32-bits into stack slot
      generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(node, *tempExpMR, 0, 4, cg), expRegister);
      }

   // prep Rounding (rounding mode + precision) for Store-Load sequence
   int32_t toRound=0;
   TR::Node * roundNode = NULL;
   TR::Node * rmNode = NULL;
   TR::Node * precNode = NULL;
   TR::Register * rmRegister= NULL;
   TR::Register * precRegister= NULL;
   TR::Register * precFPRegister= NULL;
   TR::SymbolReference * tempPrecSymRef=NULL;
   if (exp)
      {
      roundNode = node->getChild(3);
      precNode = node->getChild(4);
      rmNode = node->getChild(5);
      }
   else
      {
      roundNode = node->getChild(2);
      precNode = node->getChild(3);
      rmNode = node->getChild(4);
      }

   // check the rounding constant
   TR_ASSERT(roundNode->getOpCode().isLoadConst(), "inlineBigDecimalConstructor32: Expected Int32 toRound\n");
   toRound = roundNode->getInt();

   // prep the precision for Store-Load sequence
   if (toRound == 1)
      {
      rmRegister = cg->evaluate(rmNode);

      // move 32-bit sint precision over to FPR
      precRegister = cg->evaluate(precNode);
      precFPRegister = cg->allocateRegister(TR_FPR);

      // move over to FPR (can do this in both 32/64 bit since only lower 6-bits of precision are significant)
      generateMvFprGprInstructions(cg, node, gprLow2fpr, false, precFPRegister, precRegister);
      }

   // now complete the Store-Load sequences in REVERSE!
   if (!(toRound == 1 /*|| exp || !isLong*/))
      {
      cg->generateGroupEndingNop(node);
      }

   // complete the Store-Load sequence for exponent
   if (exp)
      {
      generateTrg1MemInstruction(cg, TR::InstOpCode::lfiwax, node, expFPRegister, new (cg->trHeapMemory()) TR::MemoryReference(node, *tempExpMR, 0, 4, cg));
      tempExpMR->decNodeReferenceCounts(cg);
      }

   // complete the Store-Load sequence for value
   if(!isLong)
      {
      generateTrg1MemInstruction(cg, TR::InstOpCode::lfiwax, node, dfpRegister, tempValMR3);
      tempValMR->decNodeReferenceCounts(cg); //since we forcedIndexedForm
      tempValMR3->decNodeReferenceCounts(cg);
      }
   else
      {
      generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, dfpRegister,
            new (cg->trHeapMemory()) TR::MemoryReference(node, tempValSymRef, 8, cg));
      }

   /* Now convert to DFP, insert exponent and round */
   int inBCDForm = 0;
   if (exp)
      {
      TR_ASSERT(node->getChild(6)->getOpCode().isLoadConst(), "inlineBigDecimalConstructor32: Unexpected Type\n");
      inBCDForm = node->getChild(6)->getInt();
      cg->decReferenceCount(node->getChild(6));
      }

   // convert to DFP
   if (!inBCDForm)
      {
      // Create a forced FPR register pair
      // NOTE:  We use fp14, fp15 because they are the last used during GRA, and are both volatile
      TR::Register * fp14Reg= cg->allocateRegister(TR_FPR);
      TR::Register * fp15Reg= cg->allocateRegister(TR_FPR);
      TR::RegisterDependencyConditions * dcffixqDeps =
         new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg->trMemory());
      dcffixqDeps->addPostCondition(fp14Reg, TR::RealRegister::fp14);
      dcffixqDeps->addPostCondition(fp15Reg, TR::RealRegister::fp15);

      // generate a dcffixq into a target FPR register pair
      generateTrg1Src1Instruction(cg, TR::InstOpCode::dcffixq, node, fp14Reg, dfpRegister);

      // generate a drdpq from a source FPR register pair into the even of the FPR pair
      generateTrg1Src1Instruction(cg, TR::InstOpCode::drdpq, node, fp14Reg, fp14Reg);

      // generate an FPR move from the even of the pair to dfpRegister
      // and stop using the FPR register pair
      generateTrg1Src1Instruction(cg, TR::InstOpCode::fmr, node, dfpRegister, fp14Reg);

      TR::LabelSymbol *depLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);  // somewhere to hang the dependencies
      generateDepLabelInstruction (cg, TR::InstOpCode::label, node, depLabel, dcffixqDeps);

      cg->stopUsingRegister(fp14Reg);
      cg->stopUsingRegister(fp15Reg);
      }
   else // convert bcd to dfp
      generateTrg1Src1Instruction(cg, TR::InstOpCode::denbcdu, node, dfpRegister, dfpRegister);

   // Insert the biased exponent
   if (exp)
      {
      generateTrg1Src2Instruction(cg, TR::InstOpCode::diex, node, dfpRegister, expFPRegister, dfpRegister);
      cg->stopUsingRegister(expFPRegister);
      }

   // to store the return values
   TR::Register * retRegister = cg->allocateRegister();

   // set the rounding mode & round
   if (toRound == 1)
      {
      genSetDFPRoundingMode(node, cg, rmRegister);

      generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::drrnd, node, dfpRegister, precFPRegister, dfpRegister, 0x3);
      cg->stopUsingRegister(precFPRegister);
      }

   // perform TGDT test only when rounding
   if (toRound == 1)
      {
      // End label
      TR::LabelSymbol * labelEND = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

      // Test the data group and branch instruction
      TR::Register *crRegister = cg->allocateRegister(TR_CCR);
      genTestDataGroup(node, cg, dfpRegister, crRegister, tgdtLaxTest);

      // did we fail?
      loadConstant(cg, node, 0, retRegister);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, labelEND, crRegister);

      // did we pass? - load the rounded value
      loadConstant(cg, node, 1, retRegister);
      genStoreDFP(node, cg, cg->evaluate(node->getFirstChild()), NULL, dfpRegister);
      cg->decReferenceCount(node->getFirstChild());

      // which registers do we need alive after branching sequence?
      TR::RegisterDependencyConditions * deps =
         new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg->trMemory());
      deps->addPostCondition(retRegister, TR::RealRegister::NoReg);
      deps->addPostCondition(dfpRegister, TR::RealRegister::NoReg);
      deps->addPostCondition(crRegister, TR::RealRegister::cr0);
      deps->addPostCondition(node->getFirstChild()->getRegister(), TR::RealRegister::NoReg);
      deps->getPostConditions()->getRegisterDependency(deps->getAddCursorForPost() - 1)->setExcludeGPR0();
      cg->stopUsingRegister(dfpRegister);
      cg->stopUsingRegister(crRegister);

      // end
      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, labelEND, deps);
      }
   else
      {
      loadConstant(cg, node, 1, retRegister);
      genStoreDFP(node, cg, cg->evaluate(node->getFirstChild()), NULL, dfpRegister);
      cg->decReferenceCount(node->getFirstChild());
      cg->stopUsingRegister(dfpRegister);
      }
   cg->decReferenceCount(roundNode);
   cg->decReferenceCount(precNode);
   cg->decReferenceCount(rmNode);

   //if (toRound == 0 || toRound == 1)
	if (toRound == 1)
      genSetDFPRoundingMode(node, cg, rmIEEEdefault);

   node->setRegister(retRegister);
   return retRegister;
   }

extern TR::Register *inlineBigDecimalConstructor(TR::Node *node, TR::CodeGenerator *cg, bool isLong, bool exp)
   {
   if (TR::Compiler->target.is64Bit())
      return inlineBigDecimalConstructor64(node, cg, isLong, exp);
   return inlineBigDecimalConstructor32(node, cg, isLong, exp);
   }

extern TR::Register *inlineBigDecimalBinaryOp(
      TR::Node * node,
      TR::CodeGenerator *cg,
      TR::InstOpCode::Mnemonic op,
      bool scaled)
   {
   // This is the check used to determine whether or not to use direct move instructions in 64 bit
   // Is used consistently throughout this file
   bool p8DirectMoveTest = TR::Compiler->target.cpu.id() == TR_PPCp8;

   int32_t toRound = 0;
   bool isConst16Precision = false;
   if (!scaled)
      {
      TR_ASSERT(node->getChild(3)->getOpCode().isLoadConst(), "inlineBigDecimalBinaryOp: Expected Int32 toRound\n");
      toRound = node->getChild(3)->getInt();
      if(toRound == 1)
         {
         isConst16Precision = node->getChild(4)->getOpCode().isLoadConst() && node->getChild(4)->getInt() == 16;
         }
      }
   // load both operands into FPRs
   TR::Register * lhsFPRegister = NULL;
   TR::Register * rhsFPRegister = NULL;
   TR::Node * lhsNode = node->getSecondChild();
   TR::Node * rhsNode = node->getChild(2);

   TR::Register * precFPRegister = NULL;
   TR::Register * rmRegister= NULL;
   TR::Node* precChild = (node->getNumChildren() <= 4)? NULL: node->getChild(4);

   overlapDFPOperandAndPrecisionLoad(node, lhsNode, rhsNode, precChild,
      lhsFPRegister, rhsFPRegister, precFPRegister,
      toRound, isConst16Precision, cg);

   TR::MemoryReference *tempMR = NULL;

   // Set the rounding mode
   // NOTE:  All operations are to unlimited precision, so we set to rmRPSP
   //        and then adjust accordingly later...
   if (toRound == 0)
      genSetDFPRoundingMode(node, cg, rmRPSP);
   else if (toRound == 1)
      {
      if (isConst16Precision)
         {
         rmRegister = cg->evaluate(node->getChild(5));
         genSetDFPRoundingMode(node, cg, rmRegister);
         }
      else
         {
         genSetDFPRoundingMode(node, cg, rmRPSP);
         }
      }
   TR::Register * resRegister = cg->allocateRegister(TR_FPR);
   if(op == TR::InstOpCode::dsub) //flip operands for subtraction
      generateTrg1Src2Instruction(cg, op, node, resRegister, rhsFPRegister, lhsFPRegister);
   else
      generateTrg1Src2Instruction(cg, op, node, resRegister, lhsFPRegister, rhsFPRegister);
   cg->stopUsingRegister(lhsFPRegister); // can do this since longs end up in FPRs
   cg->stopUsingRegister(rhsFPRegister);

   //perform the round
   if (toRound == 1 && !isConst16Precision)
      {
      rmRegister = cg->evaluate(node->getChild(5));
      genSetDFPRoundingMode(node, cg, rmRegister);

      generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::drrnd, node, resRegister, precFPRegister, resRegister, 0x3);
      cg->stopUsingRegister(precFPRegister);
      }

   if (!scaled)
      {
      cg->decReferenceCount(node->getChild(3));
      cg->decReferenceCount(node->getChild(4));
      cg->decReferenceCount(node->getChild(5));
      }

   // Test the data group and branch instruction
   // if performing scaled operation - check for special value
   // else, check for non-extremes, and zero leading digit
   TR::Register *crRegister = cg->allocateRegister(TR_CCR);
   if (!scaled)
      genTestDataGroup(node, cg, resRegister, crRegister, tgdtLaxTest);
   else
      genTestDataGroup(node, cg, resRegister, crRegister, tgdtScaledTest);

   // did we pass?
   // According to DFP spec, we are looking for case where field1 of CR is all
   // 0010 or 1010 - both have 3rd bit on, which is same as EQUALS

   TR::Register * retRegister = cg->allocateRegister();
   TR::LabelSymbol * labelEND = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   // did we fail?
   loadConstant(cg, node, 0, retRegister);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, labelEND, crRegister);

   TR::RegisterDependencyConditions * deps = NULL;

   // did we pass?
   if (!scaled)
      {
      loadConstant(cg, node, 1, retRegister);
      genStoreDFP(node, cg, cg->evaluate(node->getFirstChild()),
                 node->getSecondChild()->getSymbolReference(), resRegister);
      cg->decReferenceCount(node->getFirstChild());

      // which registers do we need alive after branching sequence?
      deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg->trMemory());
      deps->addPostCondition(retRegister, TR::RealRegister::NoReg);
      deps->addPostCondition(resRegister, TR::RealRegister::NoReg);
      deps->addPostCondition(crRegister, TR::RealRegister::cr0);
      deps->addPostCondition(node->getFirstChild()->getRegister(), TR::RealRegister::NoReg);
      deps->getPostConditions()->getRegisterDependency(deps->getAddCursorForPost() - 1)->setExcludeGPR0();
      }
   else
      {
      // Check the desired biased exponent
      TR::Register * desiredBiasedExpRegister = cg->evaluate(node->getChild(3));
      if (TR::Compiler->target.is64Bit() && p8DirectMoveTest) // sign extend it...
         generateTrg1Src1Instruction(cg, TR::InstOpCode::extsw, node, desiredBiasedExpRegister, desiredBiasedExpRegister);
      cg->decReferenceCount(node->getChild(3));

      // The actual biased exponent
      TR::Register * fprActualBiasedExpRegister = cg->allocateRegister(TR_FPR);
      generateTrg1Src1Instruction(cg, TR::InstOpCode::dxex, node, fprActualBiasedExpRegister, resRegister);
      TR::Register * actualBiasedExpRegister = cg->allocateRegister();

      // move the actual biased exponent to a GPR...
      // Can do this in 32/64bit mode since biased exponents are guaranteed to be < 32 bits
      // and the instructions will work regardless of being 32/64 bits
      generateMvFprGprInstructions(cg, node, fpr2gprLow, false, actualBiasedExpRegister, fprActualBiasedExpRegister);
      cg->stopUsingRegister(fprActualBiasedExpRegister);

      // compare the two scales....
      generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp4, node, crRegister, actualBiasedExpRegister, desiredBiasedExpRegister);
      cg->stopUsingRegister(actualBiasedExpRegister);

      // assume we passed
      loadConstant(cg, node, 1, retRegister);
      genStoreDFP(node, cg, cg->evaluate(node->getFirstChild()),
                 node->getSecondChild()->getSymbolReference(), resRegister);
      cg->decReferenceCount(node->getFirstChild());
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, labelEND, crRegister);

      // we didn't pass, so try to rescale the result....
      // load the DFP who we'll use as the dummy (containing the required exponent)
      TR::Register * dfpDummyFPRegister = cg->allocateRegister(TR_FPR);
      generateTrg1Src1Instruction(cg, TR::InstOpCode::fmr, node, dfpDummyFPRegister, resRegister);

      // Insert the biased exponent
      TR::Register * fprDesiredBiasedExpRegister = cg->allocateRegister(TR_FPR);
      generateMvFprGprInstructions(cg, node, gprSp2fpr, false, fprDesiredBiasedExpRegister, desiredBiasedExpRegister);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::diex, node, dfpDummyFPRegister, fprDesiredBiasedExpRegister, dfpDummyFPRegister);
      cg->stopUsingRegister(fprDesiredBiasedExpRegister);
      cg->stopUsingRegister(desiredBiasedExpRegister);

      // Clear exception bits since we check them after scaling
      // this clears bits 44 through 47
      //generateImm2Instruction(cg, TR::InstOpCode::mtfsfi, node, 0x0, 0x3);

      // perform the quantize
      generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::dqua, node, resRegister, dfpDummyFPRegister, resRegister, 0x3);
      cg->stopUsingRegister(dfpDummyFPRegister);

      // Test the data group and branch instruction
      genTestDataGroup(node, cg, resRegister, crRegister, tgdtScaledTest);

      // assume we failed the TGDT
      loadConstant(cg, node, 0, retRegister);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, labelEND, crRegister);

      // Now the case where we had an inexact exception, or didn't fail...
      // need to check for inexact exception, since scaling might change
      // the value of the DFP - we can't allow this....


      // move field 3 in bits 32 to 64 (bits 44 to 47) to a CR
      //generateTrg1ImmInstruction(cg, TR::InstOpCode::mcrfs, node, crRegister, 0x3); //move FPSCR field 4 (bits 44 to 47)

      // assume we failed
      //loadConstant(cg, node, 0, retRegister);
      //generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, labelEND, crRegister);

      //else we passed
      loadConstant(cg, node, 1, retRegister);
      genStoreDFP(node, cg, node->getFirstChild()->getRegister(),
                  node->getSecondChild()->getSymbolReference(), resRegister);

      // which registers do we need alive after branching sequence?
      deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 9, cg->trMemory());
      deps->addPostCondition(retRegister, TR::RealRegister::NoReg);
      deps->addPostCondition(resRegister, TR::RealRegister::NoReg);
      deps->addPostCondition(crRegister, TR::RealRegister::cr0);
      deps->addPostCondition(node->getFirstChild()->getRegister(), TR::RealRegister::NoReg);
      deps->getPostConditions()->getRegisterDependency(deps->getAddCursorForPost() - 1)->setExcludeGPR0();
      deps->addPostCondition(fprDesiredBiasedExpRegister, TR::RealRegister::NoReg);
      deps->addPostCondition(desiredBiasedExpRegister, TR::RealRegister::NoReg);
      deps->addPostCondition(fprActualBiasedExpRegister, TR::RealRegister::NoReg);
      deps->addPostCondition(actualBiasedExpRegister, TR::RealRegister::NoReg);
      deps->addPostCondition(dfpDummyFPRegister, TR::RealRegister::NoReg);
      }
   cg->stopUsingRegister(crRegister);
   cg->stopUsingRegister(resRegister);

   // end
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, labelEND, deps);

	// reset rounding mode back to default
	if (toRound == 0 || toRound == 1)
		genSetDFPRoundingMode(node, cg, rmIEEEdefault);

	node->setRegister(retRegister);
   return retRegister;
   }

/*
   For both divide methods, we have the following return value semantics:
   Return 1 if JIT compiled and DFP divide is successful, (returns DFP).
   Return 0 if JIT compiled, but DFP divide caused inexact exception (returns DFP).
   Return -1, if JIT compiled, but DFP divide caused other problem (i.e. overflow)
*/

extern TR::Register *inlineBigDecimalScaledDivide(
      TR::Node * node,
      TR::CodeGenerator *cg)
   {
   // used to check for direct move instruction support
   bool p8DirectMoveTest = TR::Compiler->target.cpu.id() == TR_PPCp8;

   // check the rounding constant
   TR::Register * rmRegister = NULL;
   int toRound = 0;
   TR_ASSERT(node->getChild(4)->getOpCode().isLoadConst(), "inlineBigDecimalScaledDivide: Unexpected Type\n");
   toRound = node->getChild(4)->getInt();
   if (toRound == 1)
      {
      rmRegister = cg->evaluate(node->getChild(5));
      genSetDFPRoundingMode(node, cg, rmRegister);
      }
   cg->decReferenceCount(node->getChild(4));
   cg->decReferenceCount(node->getChild(5));

   // load both operands into FPRs
   TR::Register * lhsFPRegister = NULL;
   TR::Register * rhsFPRegister = NULL;
   TR::Node * lhsNode = node->getSecondChild();
   TR::Node * rhsNode = node->getChild(2);

   /* We want to overlap the loading of the DFP operands and precision...
      Cases will be..

        use direct lfd if uncommoned, ref count = 1
        64-bit: mtvsrdLHS, mtvsrdRHS, mtvsrdPrecision
        32-bit:  Store-LoadLHS, Store-LoadRHS (storeLHS, storeRHS, loadRHS, loadLHS)
    */

   // Cases begin:

   // Handle the LHS and RHS
   bool lhsLoaded = loadAndEvaluateAsDouble(lhsNode, lhsFPRegister, cg);
   bool rhsLoaded = loadAndEvaluateAsDouble(rhsNode, rhsFPRegister, cg);

   // if couldn't lfd either of them, overlap the move to FPRs (case for using the POWER 8 direct move instructions)
   if (TR::Compiler->target.is64Bit() && p8DirectMoveTest)
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
      }
   else // in 32-bit mode, overlap the Store-Load sequence
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
         tempLHSMR = new (cg->trHeapMemory()) TR::MemoryReference(node, tempLHSSymRef, 8, cg);
         if (TR::Compiler->target.is32Bit())
            {
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempLHSMR, lhsRegister->getHighOrder());
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node,
                  new (cg->trHeapMemory()) TR::MemoryReference(node, *tempLHSMR, 4, 4, cg), lhsRegister->getLowOrder());
            }
         else // another case if we cannot use the POWER 8 direct move instructions
            generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, tempLHSMR, lhsRegister);
         }

      TR::SymbolReference *tempRHSSymRef = NULL;
      TR::MemoryReference *tempRHSMR = NULL;
      TR::Register *rhsRegister = NULL;

      if (!rhsLoaded)
         {
         rhsRegister = cg->evaluate(rhsNode);
         cg->decReferenceCount(rhsNode);

         // need to store each register word into mem & then load
         tempRHSSymRef = cg->allocateLocalTemp(TR::Int64);
         tempRHSMR = new (cg->trHeapMemory()) TR::MemoryReference(node, tempRHSSymRef, 8, cg);
         if (TR::Compiler->target.is32Bit())
            {
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempRHSMR, rhsRegister->getHighOrder());
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node,
                  new (cg->trHeapMemory()) TR::MemoryReference(node, *tempRHSMR, 4, 4, cg), rhsRegister->getLowOrder());
            }
         else
            generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, tempRHSMR, rhsRegister);
         }

      // now finish off the Store-Load sequence in REVERSE!
      if (!lhsLoaded || !rhsLoaded)
         {
         cg->generateGroupEndingNop(node);
         }

      if (!lhsLoaded)
         {
         lhsFPRegister = cg->allocateRegister(TR_FPR);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, lhsFPRegister,
             new (cg->trHeapMemory()) TR::MemoryReference(node, tempLHSSymRef, 8, cg));
         }
      if (!rhsLoaded)
         {
         rhsFPRegister = cg->allocateRegister(TR_FPR);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, rhsFPRegister,
             new (cg->trHeapMemory()) TR::MemoryReference(node, tempRHSSymRef, 8, cg));
         }
      }

   TR::MemoryReference *tempMR = NULL;

   // clear the inexact exception FPSCR bits
   genClearFPSCRBits(node, cg, 0x3);

   // Perform the operation
   TR::Register * resRegister = cg->allocateRegister(TR_FPR);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::ddiv, node, resRegister, rhsFPRegister, lhsFPRegister);
   cg->stopUsingRegister(lhsFPRegister);
   cg->stopUsingRegister(rhsFPRegister);

   /* At this point:
        -check Data Group, if fails, return -1
        -compare scale with desired scale (child(3))
	  -if scales equal
           -check inexact
            -if inexact, return 0, else return 1
          -if scales differ, rescale, and test the result
           -if tgdt fails, return -1
           -if inexact, return 0
           -else return 1
   */

   // Test the data group and branch instruction
   TR::Register *crRegister = cg->allocateRegister(TR_CCR);
   genTestDataGroup(node, cg, resRegister, crRegister, tgdtScaledTest);

   TR::Register * retRegister = cg->allocateRegister();
   TR::LabelSymbol * labelScalesEQUAL = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * labelEND = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   // did we TGDT fail the original divide?
   loadConstant(cg, node, -1, retRegister);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, labelEND, crRegister);

   // Check the desired biased exponent
   TR::Register * desiredBiasedExpRegister = cg->evaluate(node->getChild(3));
   if (TR::Compiler->target.is64Bit() || p8DirectMoveTest)
      generateTrg1Src1Instruction(cg, TR::InstOpCode::extsw, node, desiredBiasedExpRegister, desiredBiasedExpRegister);
   cg->decReferenceCount(node->getChild(3));

   // The actual biased exponent
   TR::Register * fprActualBiasedExpRegister = cg->allocateRegister(TR_FPR);
   generateTrg1Src1Instruction(cg, TR::InstOpCode::dxex, node, fprActualBiasedExpRegister, resRegister);
   TR::Register * actualBiasedExpRegister = cg->allocateRegister();

   // move the actual biased exponent to a GPR... (can do this in 32/64bit mode)
   if (TR::Compiler->target.is64Bit() && p8DirectMoveTest)
      {
      // generate a direct move instruction
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mfvsrd, node, actualBiasedExpRegister, fprActualBiasedExpRegister);
      }
    else
      {
      //NOTE:  Since exponent will never be > 32-bits, ok to use doubleword store-loads here
      TR::SymbolReference * tempExpSymRef = cg->allocateLocalTemp(TR::Int64);
      TR::MemoryReference * tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, tempExpSymRef, 8, cg);
      generateMemSrc1Instruction(cg, TR::InstOpCode::stfd, node, tempMR, fprActualBiasedExpRegister);

      // now finish off the Store-Load sequence in REVERSE!
      cg->generateGroupEndingNop(node);

      generateTrg1MemInstruction(cg, TR::InstOpCode::ld, node, actualBiasedExpRegister,
         new (cg->trHeapMemory()) TR::MemoryReference(node, tempExpSymRef, 8, cg));
      }
    cg->stopUsingRegister(fprActualBiasedExpRegister);

   // compare the two scales....
   generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp4, node, crRegister, actualBiasedExpRegister, desiredBiasedExpRegister);
   cg->stopUsingRegister(actualBiasedExpRegister);

   // branch to path where scales are equal
   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, labelScalesEQUAL, crRegister);

   /*
    *
    * PATH FOR WHEN SCALES ARE NOT EQUAL
    *
    */

   /*
     -clear inexact exception bits again,
     -rescale, and test the result
      -if tgdt fails, return -1
      -if inexact from divide, return 0
           -else return 1
    */

   // clear the inexact exception FPSCR bits
	genClearFPSCRBits(node, cg, 0x3);

   // need to rescale
   // load the DFP who we'll use as the dummy (containing the required exponent)
   TR::Register * dfpDummyFPRegister = cg->allocateRegister(TR_FPR);
   generateTrg1Src1Instruction(cg, TR::InstOpCode::fmr, node, dfpDummyFPRegister, resRegister);

   // Insert the biased exponent (biased exponents always < 32, so can do this in 32/64 bit if there is support)
   TR::Register * fprDesiredBiasedExpRegister = cg->allocateRegister(TR_FPR);
   if (TR::Compiler->target.is64Bit() && p8DirectMoveTest)
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mtvsrd, node, fprDesiredBiasedExpRegister, desiredBiasedExpRegister);
   else
      {
      // store register word to mem & then load
      TR::SymbolReference * tempSymRef = cg->allocateLocalTemp(TR::Int64);
      TR::MemoryReference * tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, tempSymRef, 8, cg);
      generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, tempMR, desiredBiasedExpRegister);

      cg->generateGroupEndingNop(node);

      generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, fprDesiredBiasedExpRegister,
         new (cg->trHeapMemory()) TR::MemoryReference(node, tempSymRef, 8, cg));
      }
   generateTrg1Src2Instruction(cg, TR::InstOpCode::diex, node, dfpDummyFPRegister, fprDesiredBiasedExpRegister, dfpDummyFPRegister);
   cg->stopUsingRegister(fprDesiredBiasedExpRegister);

   // perform the quantize
   generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::dqua, node, resRegister, dfpDummyFPRegister, resRegister, 0x3);
   cg->stopUsingRegister(dfpDummyFPRegister);

   // Test the data group
   genTestDataGroup(node, cg, resRegister, crRegister, tgdtScaledTest);

   // assume we failed
   loadConstant(cg, node, -1, retRegister);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, labelEND, crRegister);

   // now fall through to take the inexact exception checking path...

   /*
    *
    * PATH FOR WHEN SCALES ARE EQUAL
    *
    */
   generateLabelInstruction(cg, TR::InstOpCode::label, node, labelScalesEQUAL);

   /* store the resulting DFP, and then check:
       -inexact exception from divide happened - return 0
       -else return 1
   */

   genStoreDFP(node, cg, cg->evaluate(node->getFirstChild()),
               node->getSecondChild()->getSymbolReference(), resRegister);
   cg->decReferenceCount(node->getFirstChild());

   generateTrg1ImmInstruction(cg, TR::InstOpCode::mcrfs, node, crRegister, 0x3); //move FPSCR field 4 (bits 44 to 47)

   // assume inexact failed - so we'll load the result for BigDecimal class to test
   // against an UNNECESSARY rounding mode....
   loadConstant(cg, node, 0, retRegister);

   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, labelEND, crRegister);

   // otherwise inexact didn't fail
   loadConstant(cg, node, 1, retRegister);

   // which registers do we need alive after branching sequence?
   TR::RegisterDependencyConditions * deps =
      new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 9, cg->trMemory());
   deps->addPostCondition(retRegister, TR::RealRegister::NoReg);
   deps->addPostCondition(resRegister, TR::RealRegister::NoReg);
   deps->addPostCondition(crRegister, TR::RealRegister::cr0);
   deps->addPostCondition(node->getFirstChild()->getRegister(), TR::RealRegister::NoReg);
   deps->getPostConditions()->getRegisterDependency(deps->getAddCursorForPost() - 1)->setExcludeGPR0();
   deps->addPostCondition(fprDesiredBiasedExpRegister, TR::RealRegister::NoReg);
   deps->addPostCondition(desiredBiasedExpRegister, TR::RealRegister::NoReg);
   deps->addPostCondition(fprActualBiasedExpRegister, TR::RealRegister::NoReg);
   deps->addPostCondition(actualBiasedExpRegister, TR::RealRegister::NoReg);
   deps->addPostCondition(dfpDummyFPRegister, TR::RealRegister::NoReg);
   cg->stopUsingRegister(crRegister);
   cg->stopUsingRegister(resRegister);

	// end
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, labelEND, deps);

   // reset rounding mode back to default
	if (toRound == 1)
		genSetDFPRoundingMode(node, cg, rmIEEEdefault);

	node->setRegister(retRegister);
   return retRegister;
   }


extern TR::Register *inlineBigDecimalDivide(
      TR::Node * node,
      TR::CodeGenerator *cg)
   {

   // load both operands into FPRs
   TR::Register * lhsFPRegister = NULL;
   TR::Register * rhsFPRegister = NULL;
   TR::Node * lhsNode = node->getSecondChild();
   TR::Node * rhsNode = node->getChild(2);

   int32_t toRound = 0;
   int32_t checkInexact = 0;
   TR::Register * precFPRegister = NULL;

   // see if we need to check for inexact result
   TR_ASSERT(node->getChild(3)->getOpCode().isLoadConst(), "inlineBigDecimalDivide: Unexpected type\n");
   checkInexact = node->getChild(3)->getInt();
   cg->decReferenceCount(node->getChild(3));

   // see if we need to perform rounding
   TR_ASSERT(node->getChild(4)->getOpCode().isLoadConst(), "inlineBigDecimalDivide: Unexpected Type\n");
   toRound = node->getChild(4)->getInt();
   cg->decReferenceCount(node->getChild(4));
   bool isConst16Precision = false;
   if(toRound == 1)
      {
      isConst16Precision = node->getChild(5)->getOpCode().isLoadConst() && node->getChild(5)->getInt() == 16;
      }

   TR::Node* precChild = (node->getNumChildren() <= 5)? NULL: node->getChild(5);
   overlapDFPOperandAndPrecisionLoad(node, lhsNode, rhsNode, precChild,
      lhsFPRegister, rhsFPRegister, precFPRegister,
      toRound, isConst16Precision, cg);

   cg->decReferenceCount(node->getChild(5));

   TR::MemoryReference *tempMR = NULL;

   // Clear exception bits since we check them after division
   // this clears bits 44 through 47
   if (checkInexact)
      genClearFPSCRBits(node, cg, 0x3);

   // set DFP rounding mode to 'round for reround' i.e. no rounding
   if (toRound == 0)
      genSetDFPRoundingMode(node, cg, rmRPSP);
   else if (toRound == 1)
      {
      if (isConst16Precision)
         {
         genSetDFPRoundingMode(node, cg, cg->evaluate(node->getChild(6)));
         }
      else
         {
         genSetDFPRoundingMode(node, cg, rmRPSP);
         }
      }
   // Perform the divide
   TR::Register * resRegister = cg->allocateRegister(TR_FPR);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::ddiv, node, resRegister, rhsFPRegister, lhsFPRegister);
   cg->stopUsingRegister(lhsFPRegister);
   cg->stopUsingRegister(rhsFPRegister);

   /*
     -if need to check for inexact, but no rounding => no round, tgdt, check inexact
      --will return either -1, 1, or 0
     -if need to check for inexact + rounding => round, tgdt, check inexact
      --will return either -1, 1 or 0
     -if need to rounding but no inexact => round, tgdt
      --will return either 1 or -1

    * JIT compiled return value:
    * Return 1 if JIT compiled and DFP divide is successful.
    * Return 0 if JIT compiled, but DFP divide was inexact
    * Return -1 if JIT compiled, but other exception (i.e. overflow)
   */

   // perform rounding
   if (toRound == 1 && !isConst16Precision)
      {
      genSetDFPRoundingMode(node, cg, cg->evaluate(node->getChild(6)));

      //perform the round
      generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::drrnd, node, resRegister, precFPRegister, resRegister, 0x3);
      cg->stopUsingRegister(precFPRegister);
      }
   cg->decReferenceCount(node->getChild(6));

   // reset the rounding mode
   if (toRound == 0 || toRound == 1)
      genSetDFPRoundingMode(node, cg, rmIEEEdefault);

   // Test the data group and branch instruction
   TR::Register *crRegister = cg->allocateRegister(TR_CCR);
   genTestDataGroup(node, cg, resRegister, crRegister, tgdtLaxTest);

   TR::Register * retRegister = cg->allocateRegister();
   TR::LabelSymbol * labelEND = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   // did we fail?
   loadConstant(cg, node, -1, retRegister);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, labelEND, crRegister);

   // store the result
   genStoreDFP(node, cg, cg->evaluate(node->getFirstChild()),
               node->getSecondChild()->getSymbolReference(), resRegister);
   cg->decReferenceCount(node->getFirstChild());

   // do we need to check for inexact?
   if (checkInexact)
      {
      // did we fail ?
      loadConstant(cg, node, 0, retRegister);

      // need to check for inexact exception, since scaling might change
      // the value of the DFP - we can't allow this....

      // move field 3 in bits 32 to 64 (bits 44 to 47) to a CR
      generateTrg1ImmInstruction(cg, TR::InstOpCode::mcrfs, node, crRegister, 0x3); //move FPSCR field 4 (bits 44 to 47)
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, labelEND, crRegister);
      }

   //otherwise we have a good value
   loadConstant(cg, node, 1, retRegister);

   // which registers do we need alive after branching sequence?
   TR::RegisterDependencyConditions * deps =
      new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg->trMemory());
   deps->addPostCondition(retRegister, TR::RealRegister::NoReg);
   deps->addPostCondition(resRegister, TR::RealRegister::NoReg);
   deps->addPostCondition(crRegister, TR::RealRegister::cr0);
   deps->addPostCondition(node->getFirstChild()->getRegister(), TR::RealRegister::NoReg);
   deps->getPostConditions()->getRegisterDependency(deps->getAddCursorForPost() - 1)->setExcludeGPR0();
   cg->stopUsingRegister(crRegister);
   cg->stopUsingRegister(resRegister);

   // end
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, labelEND, deps);
   node->setRegister(retRegister);
   return retRegister;
   }

extern TR::Register *inlineBigDecimalRound(
      TR::Node * node,
      TR::CodeGenerator *cg)
   {
   // check for direct move
   bool p8DirectMoveTest = TR::Compiler->target.cpu.id() == TR_PPCp8;

   // load DFP to be rounded
   TR::Register * dfpFPRegister = NULL;
   TR::Node * dfpNode = node->getSecondChild();

   TR::SymbolReference * tempPrecSymRef= NULL;
   TR::Register * precFPRegister = NULL;
   TR::Register * rmRegister= NULL;

   /* We want to overlap the loading of the DFP operand and the precision...

        64-bit: mtvsrdLHS, mtvsrdPrecision
    */

   bool dfpLoaded = loadAndEvaluateAsDouble(dfpNode, dfpFPRegister, cg);

   // if couldn't lfd the load, overlap the move to FPRs
   if (TR::Compiler->target.is64Bit() && p8DirectMoveTest)
      {
      if (!dfpLoaded)
         {
         dfpFPRegister = cg->allocateRegister(TR_FPR);
         generateTrg1Src1Instruction(cg, TR::InstOpCode::mtvsrd, node, dfpFPRegister, cg->evaluate(dfpNode));
         cg->decReferenceCount(dfpNode);
         }

      precFPRegister = cg->allocateRegister(TR_FPR);
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mtvsrd, node, precFPRegister, cg->evaluate(node->getChild(2)));
      }
   else // in 32-bit mode or if no direct move support, generate Store-Load sequence
      {

      TR::SymbolReference *tempSymRef = NULL;
      TR::MemoryReference *tempMR = NULL;
      TR::Register *dfpRegister = NULL;

      if (!dfpLoaded)
         {
         TR::Register * dfpRegister = cg->evaluate(dfpNode);
         cg->decReferenceCount(dfpNode);

         dfpFPRegister = cg->allocateRegister(TR_FPR);

         tempSymRef = cg->allocateLocalTemp(TR::Int64);
         tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, tempSymRef, 8, cg);

         // need to store each register word into mem & then load
         if (TR::Compiler->target.is32Bit())
            {
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempMR, dfpRegister->getHighOrder());
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node,
                  new (cg->trHeapMemory()) TR::MemoryReference(node, *tempMR, 4, 4, cg), dfpRegister->getLowOrder());
            }
         else //!p8DirectMoveTest
            generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, tempMR, dfpRegister);
         }

      // store the precision
      precFPRegister = cg->allocateRegister(TR_FPR);

      // store register word to mem
      tempPrecSymRef = cg->allocateLocalTemp(TR::Int64);
      TR::MemoryReference * tempPrecMR = new (cg->trHeapMemory()) TR::MemoryReference(node, tempPrecSymRef, 8, cg);
      generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, tempPrecMR, cg->evaluate(node->getChild(2)));

      cg->generateGroupEndingNop(node);

      // load the precision
      generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, precFPRegister,
         new (cg->trHeapMemory()) TR::MemoryReference(node, tempPrecSymRef, 8, cg));

      // load the dfp
      if (!dfpLoaded)
         {
         generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, dfpFPRegister,
            new (cg->trHeapMemory()) TR::MemoryReference(node, tempSymRef, 8, cg));
         }
      }

   cg->decReferenceCount(node->getChild(2));

   // set the rounding mode
   rmRegister = cg->evaluate(node->getChild(3));
   cg->decReferenceCount(node->getChild(3));
   genSetDFPRoundingMode(node, cg, rmRegister);

   // perform the reround
   TR::Register * resFPRegister = cg->allocateRegister(TR_FPR);
   generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::drrnd, node, resFPRegister, precFPRegister, dfpFPRegister, 0x3);
   cg->stopUsingRegister(precFPRegister);
   cg->stopUsingRegister(dfpFPRegister);

   // reset rounding mode back to default
   genSetDFPRoundingMode(node, cg, rmIEEEdefault);

   // Test the data group and branch instruction
   TR::Register *crRegister = cg->allocateRegister(TR_CCR);
   genTestDataGroup(node, cg, resFPRegister, crRegister, tgdtLaxTest);

   TR::Register * retRegister = cg->allocateRegister();
   TR::LabelSymbol * labelEND = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   // did we fail?
   loadConstant(cg, node, 0, retRegister);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, labelEND, crRegister);

   // did we pass?
   loadConstant(cg, node, 1, retRegister);
   genStoreDFP(node, cg, cg->evaluate(node->getFirstChild()),
               node->getSecondChild()->getSymbolReference(), resFPRegister);
   cg->decReferenceCount(node->getFirstChild());

   // which registers do we need alive after branching sequence?
   TR::RegisterDependencyConditions * deps =
      new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg->trMemory());
   deps->addPostCondition(retRegister, TR::RealRegister::NoReg);
   deps->addPostCondition(resFPRegister, TR::RealRegister::NoReg);
   deps->addPostCondition(crRegister, TR::RealRegister::cr0);
   deps->addPostCondition(node->getFirstChild()->getRegister(), TR::RealRegister::NoReg);
   deps->getPostConditions()->getRegisterDependency(deps->getAddCursorForPost() - 1)->setExcludeGPR0();
   cg->stopUsingRegister(crRegister);
   cg->stopUsingRegister(resFPRegister);

   // end
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, labelEND, deps);
   node->setRegister(retRegister);
   return retRegister;
   }

extern TR::Register *inlineBigDecimalCompareTo(
      TR::Node * node,
      TR::CodeGenerator * cg)
   {
   bool p8DirectMoveTest = TR::Compiler->target.cpu.id() == TR_PPCp8;

   // load both operands into FPRs
   TR::Register * lhsFPRegister = NULL;
   TR::Register * rhsFPRegister = NULL;
   TR::Node * lhsNode = node->getFirstChild();
   TR::Node * rhsNode = node->getSecondChild();

   /* We want to overlap the loading of the DFP operands...
      Cases will be..

        use direct lfd if uncommoned, ref count = 1
        64-bit: mtvsrdLHS, mtvsrdRHS
        32-bt:  Store-LoadLHS, Store-LoadRHS (storeLHS, storeRHS, loadRHS, loadLHS)
    */

   // Handle the LHS and RHS
   bool lhsLoaded = loadAndEvaluateAsDouble(lhsNode, lhsFPRegister, cg);
   bool rhsLoaded = loadAndEvaluateAsDouble(rhsNode, rhsFPRegister, cg);

   // if couldn't lfd either of them, overlap the move to FPRs
   if (TR::Compiler->target.is64Bit() && p8DirectMoveTest)
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
      }
   else // in 32-bit mode, overlap the Store-Load sequence
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
         tempLHSMR = new (cg->trHeapMemory()) TR::MemoryReference(node, tempLHSSymRef, 8, cg);
         if (TR::Compiler->target.is32Bit())
            {
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempLHSMR, lhsRegister->getHighOrder());
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node,
                  new (cg->trHeapMemory()) TR::MemoryReference(node, *tempLHSMR, 4, 4, cg), lhsRegister->getLowOrder());
            }
         else //!p8DirectMoveTest
            generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, tempLHSMR, lhsRegister);
         }

      TR::SymbolReference *tempRHSSymRef = NULL;
      TR::MemoryReference *tempRHSMR = NULL;
      TR::Register *rhsRegister = NULL;

      if (!rhsLoaded)
         {
         rhsRegister = cg->evaluate(rhsNode);
         cg->decReferenceCount(rhsNode);

         // need to store each register word into mem & then load
         tempRHSSymRef = cg->allocateLocalTemp(TR::Int64);
         tempRHSMR = new (cg->trHeapMemory()) TR::MemoryReference(node, tempRHSSymRef, 8, cg);

         if (TR::Compiler->target.is32Bit())
            {
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempRHSMR, rhsRegister->getHighOrder());
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node,
                  new (cg->trHeapMemory()) TR::MemoryReference(node, *tempRHSMR, 4, 4, cg), rhsRegister->getLowOrder());
            }
         else //!p8DirectMoveTest
            generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, tempRHSMR, rhsRegister);
         }

       // now finish off the Store-Load sequence in REVERSE!
      if (!lhsLoaded || !rhsLoaded)
         {
         cg->generateGroupEndingNop(node);
         }

      if (!rhsLoaded)
         {
         rhsFPRegister = cg->allocateRegister(TR_FPR);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, rhsFPRegister,
             new (cg->trHeapMemory()) TR::MemoryReference(node, tempRHSSymRef, 8, cg));
         }
      if (!lhsLoaded)
         {
         lhsFPRegister = cg->allocateRegister(TR_FPR);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, lhsFPRegister,
             new (cg->trHeapMemory()) TR::MemoryReference(node, tempLHSSymRef, 8, cg));
         }
      }

   // compare both values
   TR::Register * crRegister = cg->allocateRegister(TR_CCR);
   TR::Instruction * cursor = NULL;
   TR::RegisterDependencyConditions * deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 1, cg->trMemory());

   cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::dcmpu, node, crRegister, lhsFPRegister, rhsFPRegister);
   cg->stopUsingRegister(lhsFPRegister);
   cg->stopUsingRegister(rhsFPRegister);

   /* We can manipulate the CR0 bits to get the desired return
      value of -1, 0 or 1 as follows (works in both 32 and 64-bit mode)

      mfcr grX
      rlwinm gr2,grX,1,31,31
      srawi grX,grX,30
      add  gr2,gr2,grX
    */

   TR::Register * crGPRegister = cg->allocateRegister();
   TR::Register * retRegister = cg->allocateRegister();
   generateTrg1ImmInstruction(cg, TR::InstOpCode::mfcr, node, crGPRegister, 0x80);
   generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, retRegister, crGPRegister, 1, 0x1);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::srawi, node, crGPRegister, crGPRegister, 30);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, retRegister, retRegister, crGPRegister);
   cg->stopUsingRegister(crGPRegister);

   TR::addDependency(deps, crRegister, TR::RealRegister::cr0, TR_CCR, cg);
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, TR::LabelSymbol::create(cg->trHeapMemory(),cg), deps);
   deps->stopUsingDepRegs(cg);

   node->setRegister(retRegister);
   return retRegister;
   }

extern TR::Register *inlineBigDecimalUnaryOp(
      TR::Node * node,
      TR::CodeGenerator * cg,
      TR::InstOpCode::Mnemonic op,
      bool precision)
   {
   // check for direct move
   bool p8DirectMoveTest = TR::Compiler->target.cpu.id() == TR_PPCp8;

   // load DFP
   TR::Register * dfpFPRegister = NULL;
   TR::Node * dfpNode = node->getFirstChild();

   bool dfpLoaded = loadAndEvaluateAsDouble(dfpNode, dfpFPRegister, cg);

   // if couldn't lfd the load, overlap the move to FPRs
   if (TR::Compiler->target.is64Bit() && p8DirectMoveTest)
      {
      if (!dfpLoaded)
         {
         dfpFPRegister = cg->allocateRegister(TR_FPR);
         generateTrg1Src1Instruction(cg, TR::InstOpCode::mtvsrd, node, dfpFPRegister, cg->evaluate(dfpNode));
         cg->decReferenceCount(dfpNode);
         }
      }
   else // in 32-bit mode or when there is no direct move support, generate Store-Load sequence
      {

      TR::SymbolReference *tempSymRef = NULL;
      TR::MemoryReference *tempMR = NULL;
      TR::Register *dfpRegister = NULL;

      if (!dfpLoaded)
         {
         dfpRegister = cg->evaluate(dfpNode);
         cg->decReferenceCount(dfpNode);

         // need to store each register word into mem & then load
         tempSymRef = cg->allocateLocalTemp(TR::Int64);
         tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, tempSymRef, 8, cg);
         if(TR::Compiler->target.is32Bit())
            {
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempMR, dfpRegister->getHighOrder());
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node,
                  new (cg->trHeapMemory()) TR::MemoryReference(node, *tempMR, 4, 4, cg), dfpRegister->getLowOrder());
            }
         else
            generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, tempMR, dfpRegister);

         cg->generateGroupEndingNop(node);

         dfpFPRegister = cg->allocateRegister(TR_FPR);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, dfpFPRegister,
            new (cg->trHeapMemory()) TR::MemoryReference(node, tempSymRef, 8, cg));
         }

      }

   // perform the operation
   // BOTH dxex & ddedpd return 64-bit values in FPRs
   TR::Register * resFPRegister = cg->allocateRegister(TR_FPR);
   generateTrg1Src1Instruction(cg, op, node, resFPRegister, dfpFPRegister);
   cg->stopUsingRegister(dfpFPRegister);

   //Convert from FPR to GPR
   TR::Register * resRegister = NULL;

   TR::SymbolReference *temp = NULL;
   TR::MemoryReference *tempMR = NULL;

   // 64-bit platform allows a move from FPR to GPR
   // handles all 64-bit, as well as 32-bit dxex
   if (TR::Compiler->target.is64Bit() || op == TR::InstOpCode::dxex)
      {
      resRegister = cg->allocateRegister();
      if (TR::Compiler->target.is64Bit() && p8DirectMoveTest)
         {
         generateTrg1Src1Instruction(cg, TR::InstOpCode::mfvsrd, node, resRegister, resFPRegister);
         }
      else
         {
         TR::SymbolReference * tempSymRef = cg->allocateLocalTemp(TR::Int64);
         TR::MemoryReference * tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, tempSymRef, 8, cg);
         generateMemSrc1Instruction(cg, TR::InstOpCode::stfd, node, tempMR, resFPRegister);

         // now finish off the Store-Load sequence in REVERSE!
         cg->generateGroupEndingNop(node);

         generateTrg1MemInstruction(cg, TR::InstOpCode::ld, node, resRegister,
            new (cg->trHeapMemory()) TR::MemoryReference(node, tempSymRef, 8, cg));
         }

      //we can do 64-bit precision
      if (op == TR::InstOpCode::ddedpd && precision)
         {
         /* precision calculation :
          * x = cntlz (count number of leading zeros)
          * x >>= 2  (divide by 4)
          * x = 16 - x (number of trailing binary non-zeros)
          */
         generateTrg1Src1Instruction(cg, TR::InstOpCode::cntlzd, node, resRegister, resRegister);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::srawi, node, resRegister, resRegister, 2);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::subfic, node, resRegister, resRegister, 16);
         }
      }
   else // handles all 32-bit, except dxex
      {
      // Allocate temporary memory
      temp = cg->allocateLocalTemp(TR::Int64);
      tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, temp, 8, cg);

      // store the 64-bit result to memory
      generateMemSrc1Instruction(cg, TR::InstOpCode::stfd, node, tempMR, resFPRegister);

      // separates the previous store & coming loads into separate dispatch groups
      // minimizes impact of Load-Hit-Store
      cg->generateGroupEndingNop(node);

      //load the 16 4-byte bcds (64-bits in total) into a reg pair
      TR::Register           *highRegister  = cg->allocateRegister();
      TR::Register           *lowRegister = cg->allocateRegister();
      resRegister = cg->allocateRegisterPair(lowRegister, highRegister);
      generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, highRegister,
         new (cg->trHeapMemory()) TR::MemoryReference(node, *tempMR, 0, 4, cg));
      generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, lowRegister,
         new (cg->trHeapMemory()) TR::MemoryReference(node, *tempMR, 4, 4, cg));

      // some extra processing for extracting significance
      if (precision)
         {
         TR::Register * tempRegister = cg->allocateRegister();

         /*
            perform 64-bit algo above on highReg
            if result is > 8, ignore lowReg
            else analyze lowReg
	       cntlzw + div by 4
               subtract from 8
          */
         generateTrg1Src1Instruction(cg, TR::InstOpCode::cntlzw, node, tempRegister, highRegister);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::srawi, node, tempRegister, tempRegister, 2);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::subfic, node, tempRegister, tempRegister, 16);

         TR::Register * crRegister = cg->allocateRegister(TR_CCR);
         TR::LabelSymbol * labelEND = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

         // which registers do we need alive after branching sequence?
         TR::RegisterDependencyConditions * deps =
             new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg->trMemory());
         deps->addPostCondition(tempRegister, TR::RealRegister::NoReg);
         deps->addPostCondition(highRegister, TR::RealRegister::NoReg);
         deps->addPostCondition(lowRegister, TR::RealRegister::NoReg);
         deps->addPostCondition(crRegister, TR::RealRegister::cr0);

         // compare result
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, crRegister, tempRegister, 8);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bgt, node, labelEND, crRegister);

         // perform lowReg case
         generateTrg1Src1Instruction(cg, TR::InstOpCode::cntlzw, node, tempRegister, lowRegister);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::srawi, node, tempRegister, tempRegister, 2);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::subfic, node, tempRegister, tempRegister, 8);

         generateDepLabelInstruction(cg, TR::InstOpCode::label, node, labelEND, deps);
         cg->stopUsingRegister(crRegister);
         cg->stopUsingRegister(highRegister);
         cg->stopUsingRegister(lowRegister);
         cg->stopUsingRegister(resRegister);

         resRegister = tempRegister;
         }
      }
   cg->stopUsingRegister(resFPRegister);
   node->setRegister(resRegister);
   return resRegister;
   }

extern TR::Register *inlineBigDecimalSetScale(
      TR::Node * node,
      TR::CodeGenerator * cg)
   {
   // check for direct move support
   bool p8DirectMoveTest = TR::Compiler->target.cpu.id() == TR_PPCp8;

   TR::MemoryReference *tempMR = NULL;
   TR::SymbolReference * temp = NULL;

   // see if we need to perform rounding
   TR_ASSERT(node->getChild(3)->getOpCode().isLoadConst(), "inlineBigDecimalSetScale: Unexpected Type\n");
   int32_t toRound = node->getChild(3)->getInt();
   cg->decReferenceCount(node->getChild(3));

   // see if we need to perform inexact check
   TR_ASSERT(node->getChild(5)->getOpCode().isLoadConst(), "inlineBigDecimalSetScale: Unexpected Type\n");
   int32_t inexCheck = node->getChild(5)->getInt();
   cg->decReferenceCount(node->getChild(5));

   if (toRound)
      {
      //set rounding mode
      TR::Register * rmRegister = cg->evaluate(node->getChild(4));
      genSetDFPRoundingMode(node, cg, rmRegister);
      }
   else
      {
      // set DFP rounding mode to 'round for reround' i.e. no rounding
      genSetDFPRoundingMode(node, cg, rmRPSP);
      }
   cg->decReferenceCount(node->getChild(4));

   // load DFP to be quantized
   TR::Register * dfpFPRegister = NULL;
   TR::Node * dfpNode = node->getSecondChild();

   // the third child will be an expression representing
   // a 32-bit sint (biased exponent in the DFP format)
   TR::Register * expRegister = cg->evaluate(node->getChild(2));
   cg->decReferenceCount(node->getChild(2));
   TR::Register * expFPRegister = cg->allocateRegister(TR_FPR);

   /* We want to overlap the loading of the DFP operand and the exponent...

        64-bit: mtvsrdLHS, mtvsrdExponent
        32-bt:  Store-LoadLHS, Store-LoadExponent
    */

   bool dfpLoaded = loadAndEvaluateAsDouble(dfpNode, dfpFPRegister, cg);

   // if couldn't lfd the load, overlap the move to FPRs
   if (TR::Compiler->target.is64Bit() && p8DirectMoveTest)
      {

      // move the desired exponent over first to avoid the sign extension penalty
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mtvsrwz, node, expFPRegister, expRegister);

      if (!dfpLoaded)
         {
         dfpFPRegister = cg->allocateRegister(TR_FPR);
         generateTrg1Src1Instruction(cg, TR::InstOpCode::mtvsrd, node, dfpFPRegister, cg->evaluate(dfpNode));
         cg->decReferenceCount(dfpNode);
         }
      }
   else // in 32-bit mode, generate Store-Load sequence
      {

      TR::SymbolReference *tempDFPSymRef = NULL;
      TR::MemoryReference *tempDFPMR = NULL;
      TR::Register *dfpRegister = NULL;

      // prep the DFP for the Store-Load sequence
      if (!dfpLoaded)
         {
         dfpRegister = cg->evaluate(dfpNode);
         cg->decReferenceCount(dfpNode);

         // need to store each register word into mem & then load
         tempDFPSymRef = cg->allocateLocalTemp(TR::Int64);
         tempDFPMR = new (cg->trHeapMemory()) TR::MemoryReference(node, tempDFPSymRef, 8, cg);
         if (TR::Compiler->target.is32Bit())
            {
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempDFPMR, dfpRegister->getHighOrder());
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node,
                  new (cg->trHeapMemory()) TR::MemoryReference(node, *tempDFPMR, 4, 4, cg), dfpRegister->getLowOrder());
            }
         else
            generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, tempDFPMR, dfpRegister);
         }

      // prep the exponent for the Store-Load sequence
      TR::MemoryReference * tempExpMR = NULL;

      // setup for store 32-bits into stack slot

      tempExpMR = new (cg->trHeapMemory()) TR::MemoryReference(cg->getStackPointerRegister(), -4, 4, cg);
      tempExpMR->forceIndexedForm(node, cg);

      // touch memory before performing Store-Load
      generateTrg1MemInstruction(cg, TR::InstOpCode::lfiwax, node, expFPRegister, tempExpMR);

      // store 32-bits into stack slot
      generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(node, *tempExpMR, 0, 4, cg), expRegister);

      // now load them in REVERSE!
      cg->generateGroupEndingNop(node);

      generateTrg1MemInstruction(cg, TR::InstOpCode::lfiwax, node, expFPRegister, new (cg->trHeapMemory()) TR::MemoryReference(node, *tempExpMR, 0, 4, cg));
      tempExpMR->decNodeReferenceCounts(cg);

      if (!dfpLoaded)
         {
         dfpFPRegister = cg->allocateRegister(TR_FPR);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, dfpFPRegister,
             new (cg->trHeapMemory()) TR::MemoryReference(node, tempDFPSymRef, 8, cg));
         }
      }

   // load the DFP who we'll use as the dummy (containing the required exponent)
   TR::Register * dfpDummyFPRegister = cg->allocateRegister(TR_FPR);
   generateTrg1Src1Instruction(cg, TR::InstOpCode::fmr, node, dfpDummyFPRegister, dfpFPRegister);

   // Insert the biased exponent
   generateTrg1Src2Instruction(cg, TR::InstOpCode::diex, node, dfpDummyFPRegister, expFPRegister, dfpDummyFPRegister);
   cg->stopUsingRegister(expFPRegister);
   cg->stopUsingRegister(expFPRegister);

   // Clear exception bits since we check them after scaling
   // this clears bits 44 through 47
   if (inexCheck)
      genClearFPSCRBits(node, cg, 0x3);

   // perform the quantize
   TR::Register * resRegister = cg->allocateRegister(TR_FPR);
   generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::dqua, node, resRegister, dfpDummyFPRegister, dfpFPRegister, 0x3);
   cg->stopUsingRegister(dfpDummyFPRegister);
   cg->stopUsingRegister(dfpFPRegister);

   // reset rounding mode back to default
   genSetDFPRoundingMode(node, cg, rmIEEEdefault);

   TR::LabelSymbol * labelEND = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::Register * retRegister = cg->allocateRegister();
   TR::Register *crRegister = cg->allocateRegister(TR_CCR);

   // need to check for inexact exception, since scaling might change
   // the value of the DFP - we can't allow this for one of the BigDecimal APIs
   if (inexCheck)
      {
      // assume we failed, and branch on CR bit 3
      loadConstant(cg, node, 0, retRegister);

      // move field 3 in bits 32 to 64 (bits 44 to 47) to a CR
      generateTrg1ImmInstruction(cg, TR::InstOpCode::mcrfs, node, crRegister, 0x3); //move FPSCR field 4 (bits 44 to 47)

      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, labelEND, crRegister);
      }

   // Test the data group and branch instruction
   genTestDataGroup(node, cg, resRegister, crRegister, tgdtScaledTest);

   // did we fail?
   loadConstant(cg, node, -1, retRegister);

   // 0 already loaded in return register
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, labelEND, crRegister);

   // did we pass?
   loadConstant(cg, node, 1, retRegister);
   genStoreDFP(node, cg, cg->evaluate(node->getFirstChild()),
               node->getSecondChild()->getSymbolReference(), resRegister);
   cg->decReferenceCount(node->getFirstChild());
   cg->stopUsingRegister(crRegister);

   // which registers do we need alive after branching sequence?
   TR::RegisterDependencyConditions * deps =
      new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg->trMemory());
   deps->addPostCondition(retRegister, TR::RealRegister::NoReg);
   deps->addPostCondition(resRegister, TR::RealRegister::NoReg);
   deps->addPostCondition(crRegister, TR::RealRegister::cr0);
   deps->addPostCondition(node->getFirstChild()->getRegister(), TR::RealRegister::NoReg);
   deps->getPostConditions()->getRegisterDependency(deps->getAddCursorForPost() - 1)->setExcludeGPR0();
   cg->stopUsingRegister(resRegister);

   // the end
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, labelEND, deps);
   node->setRegister(retRegister);
   return retRegister;
   }

extern TR::Register *inlineBigDecimalUnscaledValue(
      TR::Node * node,
      TR::CodeGenerator * cg)
   {
   // check for direct move support
   bool p8DirectMoveTest = TR::Compiler->target.cpu.id() == TR_PPCp8;

   // load DFP to be quantized
   TR::Node * dfpNode = node->getFirstChild();
   TR::Register * dfpFPRegister= NULL;

   // use an exponent of 0 (biased to +398) to force into our desired format
   TR::Register * expRegister = cg->allocateRegister();
   TR::Register * expFPRegister = cg->allocateRegister(TR_FPR);

   /* We want to overlap the loading of the DFP operand and the exponent...

        64-bit: mtvsrdLHS, mtvsrdExponent
        32-bt:  Store-LoadLHS, Store-LoadExponent
    */

   bool dfpLoaded = loadAndEvaluateAsDouble(dfpNode, dfpFPRegister, cg);

   // if couldn't lfd the load, overlap the move to FPRs
   if (TR::Compiler->target.is64Bit() && p8DirectMoveTest)
      {
      // move the exponent over first to avoid the load constant penalty
      loadConstant(cg, node, (int64_t)CONSTANT64(0x18E), expRegister);
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mtvsrd, node, expFPRegister, expRegister);
      cg->stopUsingRegister(expRegister);

      if (!dfpLoaded)
         {
         dfpFPRegister = cg->allocateRegister(TR_FPR);
         generateTrg1Src1Instruction(cg, TR::InstOpCode::mtvsrd, node, dfpFPRegister, cg->evaluate(dfpNode));
         cg->decReferenceCount(dfpNode);
         }
      }
   else // in 32-bit mode, generate Store-Load sequence
      {
      // load the exponent constant first to avoid the load penalty
      // we use 64-bit load to get it into 1 register
      loadConstant(cg, node, (int64_t)CONSTANT64(0x18E), expRegister);

      TR::Register *dfpRegister = NULL;
      TR::MemoryReference *tempDFPMR = NULL;
      TR::SymbolReference * tempDFPSymRef = NULL;

      // prep the DFP for the Store-Load sequence
      if (!dfpLoaded)
         {
         dfpRegister = cg->evaluate(dfpNode);
         cg->decReferenceCount(dfpNode);

         // need to store each register word into mem & then load
         tempDFPSymRef = cg->allocateLocalTemp(TR::Int64);
         tempDFPMR = new (cg->trHeapMemory()) TR::MemoryReference(node, tempDFPSymRef, 8, cg);
         if (TR::Compiler->target.is32Bit())
            {
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempDFPMR, dfpRegister->getHighOrder());
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node,
                  new (cg->trHeapMemory()) TR::MemoryReference(node, *tempDFPMR, 4, 4, cg), dfpRegister->getLowOrder());
            }
         else
             generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, tempDFPMR, dfpRegister);
         }

      // prep the exponent for the Store-Load sequence
      TR::MemoryReference * tempExpMR = new (cg->trHeapMemory()) TR::MemoryReference(cg->getStackPointerRegister(), -4, 4, cg);
      tempExpMR->forceIndexedForm(node, cg);

      // touch memory before performing Store-Load
      generateTrg1MemInstruction(cg, TR::InstOpCode::lfiwax, node, expFPRegister, tempExpMR);

      // store 32-bits into stack slot
      generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(node, *tempExpMR, 0, 4, cg), expRegister);
      cg->stopUsingRegister(expRegister);

      // now load them in REVERSE!
      cg->generateGroupEndingNop(node);

      generateTrg1MemInstruction(cg, TR::InstOpCode::lfiwax, node, expFPRegister, new (cg->trHeapMemory()) TR::MemoryReference(node, *tempExpMR, 0, 4, cg));
      tempExpMR->decNodeReferenceCounts(cg);

      if (!dfpLoaded)
         {
         dfpFPRegister = cg->allocateRegister(TR_FPR);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, dfpFPRegister,
             new (cg->trHeapMemory()) TR::MemoryReference(node, tempDFPSymRef, 8, cg));
         }
      }

   // load the DFP who we'll use as the one containing the required exponent
   TR::Register * dfpTempFPRegister = cg->allocateRegister(TR_FPR);
   generateTrg1Src1Instruction(cg, TR::InstOpCode::fmr, node, dfpTempFPRegister, dfpFPRegister);

   // Insert the biased exponent
   generateTrg1Src2Instruction(cg, TR::InstOpCode::diex, node, dfpTempFPRegister, expFPRegister, dfpTempFPRegister);
   cg->stopUsingRegister(expFPRegister);
   cg->stopUsingRegister(dfpFPRegister);

   TR::Register * retRegister = NULL;
   TR::Register           *highRegister  = NULL;
   TR::Register           *lowRegister = NULL;

   // convert from DFP to fixed
   generateTrg1Src1Instruction(cg, TR::InstOpCode::dctfix, node, dfpTempFPRegister, dfpTempFPRegister);

   // move fixed to GPR
   if(TR::Compiler->target.is64Bit() && p8DirectMoveTest)
      {
	  // path for direct move sequence
      retRegister = cg->allocateRegister();
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mfvsrd, node, retRegister, dfpTempFPRegister);
      }
   else // 32-bit or if no direct move support
      {
      // Allocate temporary memory
      TR::SymbolReference * tempSymRef = cg->allocateLocalTemp(TR::Int64);
      TR::MemoryReference * tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, tempSymRef, 8, cg);


      // store the 64-bit result to memory
      generateMemSrc1Instruction(cg, TR::InstOpCode::stfd, node, tempMR, dfpTempFPRegister);

      // separates the previous store & coming loads into separate dispatch groups
      // minimizes impact of Load-Hit-Store
      cg->generateGroupEndingNop(node);

      //load the 64-bit fixed value into a reg pair
      if (TR::Compiler->target.is32Bit())
         {
         highRegister  = cg->allocateRegister();
         lowRegister = cg->allocateRegister();
         retRegister = cg->allocateRegisterPair(lowRegister, highRegister);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, highRegister,
            new (cg->trHeapMemory()) TR::MemoryReference(node, *tempMR, 0, 4, cg));
         generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, lowRegister,
            new (cg->trHeapMemory()) TR::MemoryReference(node, *tempMR, 4, 4, cg));
         }
      else //!p8DirectMoveTest
         {
         retRegister = cg->allocateRegister();
         generateTrg1MemInstruction(cg, TR::InstOpCode::ld, node, retRegister,
            new (cg->trHeapMemory()) TR::MemoryReference(node, tempSymRef, 8, cg));
         }
      }
   cg->stopUsingRegister(dfpTempFPRegister);
   node->setRegister(retRegister);
   return retRegister;
   }
