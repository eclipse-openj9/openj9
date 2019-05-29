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
#include "codegen/BackingStore.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "optimizer/Simplifier.hpp"
#include "z/codegen/BinaryAnalyser.hpp"
#include "z/codegen/BinaryCommutativeAnalyser.hpp"
#include "z/codegen/CallSnippet.hpp"
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390Snippets.hpp"
#include "z/codegen/J9TreeEvaluator.hpp"

/**
  Notes to use when re-opening DFP on Z work later on:
   -cleanup TreeEvaluator.hpp across p and z
   -make sure the register allocator knows that we are using all 64-bits of a register when in 32 bit mode
**/

// used when querying VM for BigDecimal dfp field
static int16_t dfpFieldOffset = -1; // initialize to an illegal val

// Rounding modes
const uint8_t rmIEEEdefault = 0x0; // round to nearest ties to even
const uint8_t rmRPSP = 0x7; //round to prepare for shorter precision

// Exception related
const uint8_t maskIEEEMask = 0x1; //IEEE mask bits only (bits 0-4 of FPC)
const uint8_t maskIEEEException = 0x2; //IEEE exception bits only (bits 8-12 of FPC)
const uint32_t excInexactFlag = 0x00080000; //mask for the inexact result exception bit

/* following used to make sure DFP is: (used for precision-based operations)
   zero with non-extreme exponent
   normal with non-extreme exponent and Leftmost zero
   normal with non-extreme exponent and Leftmost nonzero
*/
const uint32_t tdgdtLaxTest = 0x3C3;

/* following used to make sure DFP is: (used for scale-based operations)
   zero with non-extreme exponent
   normal with non-extreme exponent and Leftmost zero
*/
const uint32_t tdgdtScaledTest = 0x3CF;

// For querying the VM for DFP field
const char * fieldName = "laside";
const char * sig = "J";
const uint32_t fieldLen = 6;
const uint32_t sigLen = 1;
const char * className = "Ljava/math/BigDecimal;\0";
const int32_t len = 22;

/*
 *  No need to stopUsingRegister(FPR) because the RA does
 *  not check for liveliness of these regs on zSeries (yet).
 *  But we'll do it anyways.
 */

/*
 * DFP instructions are only supported on zArch which is 64-bit.
 * If running a 32-bit JVM, we can access 64-bit GPRs!!
 */
void
genRestoreFPCMaskBits(
      TR::Node * node,
      TR::CodeGenerator * cg,
      TR::Register * restRegister)
   {
   // Extract FPC
   TR::Register * fpcRegister = cg->allocateRegister();
   generateRRInstruction(cg, TR::InstOpCode::EFPC, node, fpcRegister, fpcRegister);

   // OR FPC bits with register containing original mask bits
   generateRRInstruction(cg, TR::InstOpCode::OR, node, fpcRegister, restRegister);

   // Store the FPC
   generateRRInstruction(cg, TR::InstOpCode::SFPC, node, fpcRegister, fpcRegister);
   cg->stopUsingRegister(fpcRegister);
   }

static TR::Register *
genClearFPCBits(
      TR::Node * node,
      TR::CodeGenerator *cg,
      uint8_t mask)
   {
   // Extract FPC
   TR::Register * fpcRegister = cg->allocateRegister();
   generateRRInstruction(cg, TR::InstOpCode::EFPC, node, fpcRegister, fpcRegister);

   // Save a backup of the IEEE mask bits if we're clearing them
   TR::Register * maskRegister=NULL;
   if (mask & maskIEEEMask)
      {
      maskRegister = cg->allocateRegister();
      generateRRInstruction(cg, TR::InstOpCode::LR, node, maskRegister, fpcRegister); //make a copy
      generateRILInstruction(cg, TR::InstOpCode::NILF, node, maskRegister, 0xFF000000);
      }

   // Perform the clear
   if (mask & maskIEEEMask && !(mask & maskIEEEException)) //clear only the IEEE exception mask bits
      generateRILInstruction(cg, TR::InstOpCode::NILF, node, fpcRegister, 0x07FFFFFF);
   else if (mask & maskIEEEException && !(mask & maskIEEEMask)) //clear only the IEEE exception bits
      generateRILInstruction(cg, TR::InstOpCode::NILF, node, fpcRegister, 0xFF07FFFF);
   else if (mask & (maskIEEEMask|maskIEEEException))//maskIEEEBoth) //both types of FPC bits
      generateRILInstruction(cg, TR::InstOpCode::NILF, node, fpcRegister, 0x0707FFFF);

   generateRRInstruction(cg, TR::InstOpCode::SFPC, node, fpcRegister, fpcRegister);
   cg->stopUsingRegister(fpcRegister);
   return maskRegister;
   }

static TR::MemoryReference *
genSetDFPRoundingMode(
      TR::Node *node,
      TR::CodeGenerator *cg,
      int32_t rm)
   {
   // Generate an address to represent the rounding mode
   TR::MemoryReference * rmMR = generateS390MemoryReference(rm, cg);
   generateSInstruction(cg, TR::InstOpCode::SRNMT, node, rmMR);
   return rmMR;
   }

static TR::MemoryReference *
genSetDFPRoundingMode(
      TR::Node *node,
      TR::CodeGenerator *cg,
      TR::Register *rmRegister)
   {
   TR::MemoryReference * rmMR = generateS390MemoryReference(rmRegister, (int32_t)0x0, cg);
   generateSInstruction(cg, TR::InstOpCode::SRNMT, node, rmMR);
   return rmMR;
   }

static TR::MemoryReference *
genTestDataGroup(
      TR::Node * node,
      TR::CodeGenerator *cg,
      TR::Register *fprRegister,
      int32_t grp)
   {
   TR::MemoryReference * tdgdtMaskMR = generateS390MemoryReference(grp, cg);
   generateRXInstruction(cg, TR::InstOpCode::TDGDT, node, fprRegister, tdgdtMaskMR);
   return tdgdtMaskMR;
   }

static TR::Register *
genLoadDFP(
      TR::Node * node,
      TR::CodeGenerator *cg,
      TR::Node * fieldNode)
   {
   TR::Register * fprRegister = NULL;

   // if we have no register, and if reference count == 1
   // NOTE:  ref count == 1 iff rematerialization worked or if the code
   //        really does have no commoned node
   if (fieldNode->getOpCodeValue() == TR::lloadi &&
       !fieldNode->getRegister() && fieldNode->getReferenceCount() == 1)
      {
      // change the opcode to indirect load double
      TR::Node::recreate(fieldNode, TR::dloadi);

      // evaluate into an FPR
      fprRegister = cg->evaluate(fieldNode);
      cg->decReferenceCount(fieldNode);
      }
   else
      {
      // already evaluated, or not evaluated, but commoned
      // (or rematerialization didn't work), or TR::lload due
      // to stack-allocation
      TR::Register * newRegister = cg->evaluate(fieldNode);
      cg->decReferenceCount(fieldNode);
      fprRegister = cg->allocateRegister(TR_FPR);

      // move it from GPR to FPR
      if (TR::Compiler->target.cpu.getSupportsFloatingPointExtensionFacility())
         {
         generateRRInstruction(cg, TR::InstOpCode::LDGR, node, fprRegister, newRegister);
         }
      else
         {
         // OSC
         TR::SymbolReference * tempSR = cg->allocateLocalTemp(TR::Int64);
         TR::MemoryReference * tempMR = generateS390MemoryReference(node, tempSR, cg);
         generateRXInstruction(cg, TR::InstOpCode::STG, node, newRegister, tempMR);
         TR::MemoryReference * tempMR2 = generateS390MemoryReference(node, tempSR, cg);
         generateRXInstruction(cg, TR::InstOpCode::LD, node, fprRegister, tempMR2);
         }
      }
   return fprRegister;
   }

static void
genStoreDFP(
      TR::Node *node,
      TR::CodeGenerator *cg,
      TR::Node * objAddrNode,
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
   TR::Register * objRegister = cg->evaluate(objAddrNode);

   // query the VM for the field offset to which we are going to store
   if (dfpFieldOffset == -1)
      {
      TR_OpaqueClassBlock * bigDClass = NULL;
      bigDClass = fej9->getClassFromSignature((char*)className, len,
                  node->getSymbolReference()->getOwningMethod(comp), true);

      int16_t computedDfpFieldOffset = fej9->getInstanceFieldOffset(bigDClass, (char*)fieldName,
         fieldLen, (char*)sig, sigLen);

      if(computedDfpFieldOffset == -1)
         {
         TR_ASSERT( false,"offset for 'laside' field not found\n");
         comp->failCompilation<TR::CompilationException>("offset for dfp field is unknown, abort compilation\n");
         }

      dfpFieldOffset = computedDfpFieldOffset + fej9->getObjectHeaderSizeInBytes();  // size of a J9 object header to move past it
      }

   // need a hand-crafted memref
   TR::MemoryReference * dfpMR = generateS390MemoryReference(objRegister, dfpFieldOffset, cg);

   // make sure we use correct version of store... i.e. long displacement
   generateRXInstruction(cg, TR::InstOpCode::STD, node, fprDFPRegister, dfpMR);

   // need to make sure the hand-crafted memref is aliased to the field
   /*if (TR::TreeEvaluator::dfpFieldSymbolReference) // re-use
      dfpMR->setSymbolReference(TR::TreeEvaluator::dfpFieldSymbolReference);
   else //create our own (generic long shadow)
      dfpMR->setSymbolReference(cg->comp()->getSymRefTab()->
                                   findOrCreateGenericLongShadowSymbolReference(
                                   dfpFieldOffset, node->getSymbolReference()->getSymbol()->getResolvedMethodSymbol()));
   */
   }

/*
private final bool DFPIntConstructor(int val, int rndFlag, int prec, int rm)
private final bool DFPLongExpConstructor(long val, int biasedExp, int rndFlag, int prec, int rm, bool bcd)
*/
extern TR::Register *
inlineBigDecimalConstructor(
      TR::Node *node,
      TR::CodeGenerator *cg,
      bool isLong,
      bool exp)
   {
   //printf("\t390-DFPower::inlineBDConstructor (isLong=%d)(exp=%d)\n",isLong,exp);
   TR::Register * retRegister = cg->allocateRegister();
   // no need to clear IEEE mask bits or set rounding mode

   // Load the value into a GPR and convert it to DFP
   TR::Register * dfpRegister = cg->allocateRegister(TR_FPR);
   TR::Register * valRegister = NULL;
   TR::Register * valRegister2 = NULL;

   TR::SymbolReference *temp = NULL;

   //store precision and rounding mode nodes
   TR::Node * roundNode = NULL;
   TR::Node * rmNode = NULL;
   TR::Node * precNode = NULL;
   TR::Node * bcdNode = NULL;

   // see if we need to perform rounding for longexp constructor
   bool inBCDForm = 0;
   if (exp)
      {
      roundNode = node->getChild(3);
      precNode = node->getChild(4);
      rmNode = node->getChild(5);
      bcdNode = node->getChild(6);
      TR_ASSERT( bcdNode->getOpCode().isLoadConst(), "inlineBigDecimalConstructor: Unexpected Type\n");
      inBCDForm = bcdNode->getInt();
      cg->decReferenceCount(bcdNode);
      }
   else
      {
      roundNode = node->getChild(2);
      precNode = node->getChild(3);
      rmNode = node->getChild(4);
      }

   TR::Instruction *bdInstr;

   TR::LabelSymbol * OOLlabelEND = NULL;
   if (inBCDForm)
      OOLlabelEND = generateLabelSymbol(cg);

   // need to sign-extend
   if(!isLong)
      {
      valRegister = cg->evaluate(node->getSecondChild());
      cg->decReferenceCount(node->getSecondChild());
      valRegister2 = cg->allocateRegister();
      generateRRInstruction(cg, TR::InstOpCode::LGFR, node, valRegister2, valRegister);

      // don't need valRegister anymore
      valRegister = valRegister2;
      }
   else
      {
      valRegister = cg->evaluate(node->getSecondChild());
      cg->decReferenceCount(node->getSecondChild());
      }

   // convert to DFP
   if (inBCDForm)
      {
      TR::Instruction * instr = generateRRInstruction(cg, TR::InstOpCode::CDUTR, node, dfpRegister, valRegister);
      bdInstr = instr;
      TR::LabelSymbol * jump2callSymbol = generateLabelSymbol(cg);

      instr->setNeedsGCMap(0x0000FFFF);
      //CDUTR only has 4byte length, so append 2bytes
      TR::Instruction * nop = new (cg->trHeapMemory()) TR::S390NOPInstruction(TR::InstOpCode::NOP, 2, node, cg);

      TR::S390RestoreGPR7Snippet * restoreSnippet = new (cg->trHeapMemory()) TR::S390RestoreGPR7Snippet(cg, node,
                                                                                                      generateLabelSymbol(cg),
                                                                                                      jump2callSymbol);
      cg->addSnippet(restoreSnippet);
      TR::Instruction * cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_NOP, node, restoreSnippet->getSnippetLabel());

      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_NOP, node, jump2callSymbol);

      //setting up OOL path
      TR_S390OutOfLineCodeSection* outlinedHelperCall = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(jump2callSymbol, OOLlabelEND, cg);
      cg->getS390OutOfLineCodeSectionList().push_front(outlinedHelperCall);

      //switch to OOL generation
      outlinedHelperCall->swapInstructionListsWithCompilation();
      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, jump2callSymbol);

      TR_Debug * debugObj = cg->getDebug();
      if (debugObj)
         debugObj->addInstructionComment(cursor, "Denotes start of DFP inlineBigDecimalConstructor OOL BCDCHK sequence");

      generateLoad32BitConstant(cg, node, 0, retRegister, true);

      if (debugObj)
         debugObj->addInstructionComment(cursor, "Denotes end of DFP inlineBigDecimalBinaryOp OOL BCDCHK sequence");

      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, OOLlabelEND);
      outlinedHelperCall->swapInstructionListsWithCompilation();
      }
   else
      generateRRInstruction(cg, TR::InstOpCode::CDGTR, node, dfpRegister, valRegister); //CDGTR does not throw decimal operand exception

   if (!isLong)
      cg->stopUsingRegister(valRegister2);

   // store exponent
   if (exp)
      {
      // Load the biased 32-bit sint exponent
      bool clobbered=false;
      TR::Register * expRegister = NULL;
      if (node->getChild(2)->getReferenceCount() > 1)
         {
         clobbered=true;
         expRegister = cg->gprClobberEvaluate(node->getChild(2));
         }
      else
         expRegister = cg->evaluate(node->getChild(2));
      cg->decReferenceCount(node->getChild(2));

      // sign-extend
      generateRRInstruction(cg, TR::InstOpCode::LGFR, node, expRegister, expRegister);

      // insert exponent into DFP
      generateRRFInstruction(cg, TR::InstOpCode::IEDTR, node, dfpRegister, expRegister, dfpRegister);

      if (clobbered)
         cg->stopUsingRegister(expRegister); // since clobber eval'd
      }

   // to store the return values
   TR::Register * resRegister = dfpRegister;

   // store the unrounded value
   //genStoreDFP(node, cg, node->getFirstChild(), NULL, resRegister);
   //cg->decReferenceCount(node->getFirstChild());

   // rounding
   int32_t toRound=0;

   // check the rounding constant
   TR_ASSERT( roundNode->getOpCode().isLoadConst(), "inlineBigDecimalConstructor: Unexpected Type\n");
   toRound = roundNode->getInt();

   if (toRound == 1)
      {
      TR::Register * precRegister = cg->evaluate(precNode);
      TR::Register * rmRegister = cg->evaluate(rmNode);

      // set the rounding mode
      genSetDFPRoundingMode(node, cg, rmRegister);

      //perform the round
      generateRRFInstruction(cg, TR::InstOpCode::RRDTR, node, resRegister, precRegister, dfpRegister, USE_CURRENT_DFP_ROUNDING_MODE);
      cg->stopUsingRegister(dfpRegister);

      // reset to default IEEE rounding mode
      genSetDFPRoundingMode(node, cg, rmIEEEdefault);

      // Test the data group and branch instruction for non-extremes, and zero leading digit
      genTestDataGroup(node, cg, resRegister, tdgdtLaxTest);

      TR::LabelSymbol * labelEND = generateLabelSymbol(cg);

      // did we fail? - original non rounded value loaded earlier
      generateLoad32BitConstant(cg, node, 0, retRegister, false);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BLR, node, labelEND);

      // did we pass? - load the rounded value
      genStoreDFP(node, cg, node->getFirstChild(), NULL, resRegister);
      cg->decReferenceCount(node->getFirstChild());
      generateLoad32BitConstant(cg, node, 1, retRegister, true);

      // which registers should still be alive after branching sequence?
      TR::RegisterDependencyConditions * deps = generateRegisterDependencyConditions(0, 3, cg);
      deps->addPostCondition(retRegister, TR::RealRegister::AssignAny);
      deps->addPostCondition(resRegister, TR::RealRegister::AssignAny);
      deps->addPostCondition(node->getFirstChild()->getRegister(), TR::RealRegister::AssignAny);
      cg->stopUsingRegister(resRegister);

      // the end
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelEND, deps);
      }
   else
      {
      // store the unrounded value
      genStoreDFP(node, cg, node->getFirstChild(), NULL, resRegister);
      cg->decReferenceCount(node->getFirstChild());

      generateLoad32BitConstant(cg, node, 1, retRegister, true);
      cg->stopUsingRegister(resRegister);
      }

   if (inBCDForm)
      {

      TR::Register * sourceRegister = bdInstr->getRegisterOperand(2);
      TR::Register * targetRegister = bdInstr->getRegisterOperand(1);

      TR::RegisterDependencyConditions * deps = generateRegisterDependencyConditions(0, 4, cg);
      TR::TreeEvaluator::addToRegDep(deps, sourceRegister, true);
      TR::TreeEvaluator::addToRegDep(deps, targetRegister, true);


      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, OOLlabelEND, deps);
      }

   cg->decReferenceCount(roundNode);
   cg->decReferenceCount(precNode);
   cg->decReferenceCount(rmNode);

   node->setRegister(retRegister);
   return retRegister;
   }

/*
private final bool DFPAdd|Subtract|Multiply(long lhsDFP,long rhsDFP, int rndFlag, int precision, int rm)
private final bool DFPScaledAdd|Subtract|Multiply(long lhsDFP,long rhsDFP,int biasedExp)
*/
extern TR::Register *
inlineBigDecimalBinaryOp(
      TR::Node * node,
      TR::CodeGenerator *cg,
      TR::InstOpCode::Mnemonic op,
      bool scaled)
   {
   //printf("\t390-DFPower::inlineBDBinaryOp - ");

   /*switch(op)
      {
      case TR::InstOpCode::ADTR:
         if (!scaled)
            printf("ADTR\n");
         else
            printf("Scaled ADTR\n"); break;
      case TR::InstOpCode::SDTR:
         if (!scaled)
            printf("SDTR\n");
         else
            printf("Scaled SDTR\n"); break;
      case TR::InstOpCode::MDTR:
         if (!scaled)
            printf("MDTR\n");
         else
            printf("Scaled MDTR\n");  break;
      default:       printf("\n");      break;
      }*/

   int32_t toRound = 0;
   if (!scaled)
      {
      TR_ASSERT( node->getChild(3)->getOpCode().isLoadConst(), "inlineBigDecimalBinaryOp: Expected Int32 toRound\n");
      toRound = node->getChild(3)->getInt();
      }

   // load both operands into FPRs
   TR::Register * lhsFPRegister = genLoadDFP(node, cg, node->getSecondChild());
   TR::Register * rhsFPRegister = genLoadDFP(node, cg, node->getChild(2));
   TR::Register * rmRegister = NULL;
   // Set the rounding mode
   // NOTE:  All unscaled operations are to unlimited precision, so we set to rmRPSP
   //        and then adjust accordingly later...

   if((toRound == 1 && node->getChild(4)->getOpCode().isLoadConst() && node->getChild(4)->getInt() == 16))
      {
      rmRegister = cg->evaluate(node->getChild(5));
      genSetDFPRoundingMode(node, cg, rmRegister);
      }
   else if(toRound == 0 || toRound == 1)
      {
      genSetDFPRoundingMode(node, cg, rmRPSP);
      }

   TR::Register * retRegister = cg->allocateRegister();
   TR::LabelSymbol * labelEND = generateLabelSymbol(cg);

   // Perform the operation
   TR::Register * resFPRegister = cg->allocateRegister(TR_FPR);
   TR::Instruction * instr = generateRRFInstruction(cg, op, node, resFPRegister, rhsFPRegister, lhsFPRegister, USE_CURRENT_DFP_ROUNDING_MODE);
   cg->stopUsingRegister(lhsFPRegister); // can do this since longs end up in FPRs
   cg->stopUsingRegister(rhsFPRegister);

   // need to perform rounding?
   if (toRound == 1  && rmRegister == NULL)
      {
      //set rounding mode
      rmRegister = cg->evaluate(node->getChild(5));
      genSetDFPRoundingMode(node, cg, rmRegister);

      //load required precision
      TR::Register * precRegister = cg->evaluate(node->getChild(4));

      //perform the round
      generateRRFInstruction(cg, TR::InstOpCode::RRDTR, node, resFPRegister, precRegister, resFPRegister, USE_CURRENT_DFP_ROUNDING_MODE);
      }

   if (!scaled)
      {
      cg->decReferenceCount(node->getChild(3));
      cg->decReferenceCount(node->getChild(4));
      cg->decReferenceCount(node->getChild(5));
      }

   // Test the data group and branch instruction for non-extremes, and zero leading digit
   if (!scaled)
      genTestDataGroup(node, cg, resFPRegister, tdgdtLaxTest);
   else
      genTestDataGroup(node, cg, resFPRegister, tdgdtScaledTest);

   // did we pass?
   // According to DFP spec, we are looking for case where field1 of CR is all
   // 0010 or 1010 - both have 3rd bit on, which is same as EQUALS

   // did we fail?
   generateLoad32BitConstant(cg, node, 0, retRegister, false);

   if (toRound == 0 || toRound == 1)
      genSetDFPRoundingMode(node, cg, rmIEEEdefault);

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BLR, node, labelEND);

   TR::RegisterDependencyConditions * deps = NULL;

   // did we pass?
   if (!scaled)
      {

      genStoreDFP(node, cg, node->getFirstChild(), node->getSecondChild()->getSymbolReference(), resFPRegister);
      cg->decReferenceCount(node->getFirstChild());
      generateLoad32BitConstant(cg, node, 1, retRegister, true);

      // which registers should still be alive after branching sequence?
      deps = generateRegisterDependencyConditions(0, 3, cg);
      deps->addPostCondition(retRegister, TR::RealRegister::AssignAny);
      deps->addPostCondition(resFPRegister, TR::RealRegister::AssignAny);
      deps->addPostCondition(node->getFirstChild()->getRegister(), TR::RealRegister::AssignAny);
      cg->stopUsingRegister(resFPRegister);
      }
   else
      {
      // Check the desired biased exponent
      TR::Register * desiredBiasedExponent = cg->evaluate(node->getChild(3));
      cg->decReferenceCount(node->getChild(3));

      // sign extend into 64-bits
      TR::Register * desiredBiasedExponent2 = NULL;
      if (node->getChild(2)->getReferenceCount() == 0)
         desiredBiasedExponent2 = desiredBiasedExponent;
      else
         desiredBiasedExponent2 = cg->allocateRegister();

      generateRRInstruction(cg, TR::InstOpCode::LGFR, node, desiredBiasedExponent2, desiredBiasedExponent);
      desiredBiasedExponent = desiredBiasedExponent2;

      // The actual biased exponent
      TR::Register * actualBiasedExpRegister = cg->allocateRegister();
      generateRRInstruction(cg, TR::InstOpCode::EEDTR, node, actualBiasedExpRegister, resFPRegister);

      // Compare the two scales
      generateRRInstruction(cg, TR::InstOpCode::CR, node, desiredBiasedExponent, actualBiasedExpRegister);
      cg->stopUsingRegister(actualBiasedExpRegister);

      // assume we passed
      genStoreDFP(node, cg, node->getFirstChild(), node->getSecondChild()->getSymbolReference(), resFPRegister);
      cg->decReferenceCount(node->getFirstChild());
      generateLoad32BitConstant(cg, node, 1, retRegister, false);

      // check the condition code and branch
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, labelEND);

      // we didn't pass, so try to rescale the result....
      // load the DFP who we'll use as the dummy (containing the required exponent)
      TR::Register * dfpDummyFPRegister = cg->allocateRegister(TR_FPR);
      generateRRInstruction(cg, TR::InstOpCode::LDR, node, dfpDummyFPRegister, resFPRegister);

      // insert the biased exponent
      generateRRFInstruction(cg, TR::InstOpCode::IEDTR, node, dfpDummyFPRegister, desiredBiasedExponent, dfpDummyFPRegister);

      // perform the quantize
      generateRRFInstruction(cg, TR::InstOpCode::QADTR, node, resFPRegister, dfpDummyFPRegister, resFPRegister, USE_CURRENT_DFP_ROUNDING_MODE);
      cg->stopUsingRegister(dfpDummyFPRegister);

      if (toRound == 1) //since toRound ==1 taken care of above
         genSetDFPRoundingMode(node, cg, rmIEEEdefault);

      // Test data group and then branch
      genTestDataGroup(node, cg, resFPRegister, tdgdtScaledTest);

      // assume we failed
      generateLoad32BitConstant(cg, node, 0, retRegister, false);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BLR, node, labelEND);

      // else we passed  - note 2nd genStoreDFP, don't want to evaluate again...
      generateLoad32BitConstant(cg, node, 1, retRegister, true);
      genStoreDFP(node, cg, node->getFirstChild(), node->getSecondChild()->getSymbolReference(), resFPRegister);

      // which regs do we need alive after branching sequence?
      deps = generateRegisterDependencyConditions(0, 3, cg);
      deps->addPostCondition(retRegister, TR::RealRegister::AssignAny);
      deps->addPostCondition(resFPRegister, TR::RealRegister::AssignAny);
      deps->addPostCondition(node->getFirstChild()->getRegister(), TR::RealRegister::AssignAny);
      cg->stopUsingRegister(resFPRegister);
   }

   // the end
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelEND, deps);
   node->setRegister(retRegister);
   return retRegister;
   }

/*
   For both divide methods, we have the following return value semantics:
   Return 1 if JIT compiled and DFP divide is successful, (returns DFP).
   Return 0 if JIT compiled, but DFP divide caused inexact exception (returns DFP).
   Return -2, if JIT compiled, but DFP divide caused other problem (i.e. overflow)
*/

/*
private final int DFPScaledDivide(long lhsDFP, long rhsDFP, int scale, int rndFlg, int rm)
*/
extern TR::Register *
inlineBigDecimalScaledDivide(
      TR::Node * node,
      TR::CodeGenerator *cg)
   {
   //printf("\t390-DFPower::inlineBDBinaryOp - Scaled DDTR\n");

   // check the rounding constant
   TR::Register * rmRegister = NULL;
   int32_t toRound = 0;
   TR_ASSERT( node->getChild(4)->getOpCode().isLoadConst(), "inlineBigDecimalScaledDivide: Unexpected Type\n");
   toRound = node->getChild(4)->getInt();
   if (toRound == 1)
      {
      rmRegister = cg->evaluate(node->getChild(5));
      genSetDFPRoundingMode(node, cg, rmRegister);
      }
   else
      {
      // in this case however, we must set the rm if we rescale after
      genSetDFPRoundingMode(node, cg, rmRPSP);
      }

   cg->decReferenceCount(node->getChild(4));
   cg->decReferenceCount(node->getChild(5));

   // load both operands into FPRs
   TR::Register * lhsFPRegister = genLoadDFP(node, cg, node->getSecondChild());
   TR::Register * rhsFPRegister = genLoadDFP(node, cg, node->getChild(2));

   // clear FPC bits
   genClearFPCBits(node, cg, maskIEEEException);

   // Perform the operation
   TR::Register * resFPRegister = cg->allocateRegister(TR_FPR);

   //TODO: DDTR needs to have BCD fall back path
   TR::Instruction * instr = generateRRFInstruction(cg, TR::InstOpCode::DDTR, node, resFPRegister, rhsFPRegister, lhsFPRegister, USE_CURRENT_DFP_ROUNDING_MODE);

   TR::Register * retRegister = cg->allocateRegister();
   TR::LabelSymbol * labelEND = generateLabelSymbol(cg);
   cg->stopUsingRegister(lhsFPRegister); // can do this since longs end up in FPRs
   cg->stopUsingRegister(rhsFPRegister);

   /* At this point:
        -check Data Group, if fails, return -1
        -compare scale with desired scale (child(3))
     -if scales equal
           -check inexact
            -if inexact, return 0, else return 1
          -if scales differ, rescale, and test the result
           -if tdgdt fails, return -1
           -if inexact, return 0
           -else return 1
   */

   // Test data group and then branch
   genTestDataGroup(node, cg, resFPRegister, tdgdtScaledTest);

   TR::LabelSymbol * labelScalesEQUAL = generateLabelSymbol(cg);

   // did we TGDT fail the original divide?
   generateLoad32BitConstant(cg, node, -1, retRegister, false);

   if (toRound == 0)
      genSetDFPRoundingMode(node, cg, rmIEEEdefault);

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BLR, node, labelEND);

   // Check the desired biased exponent
   TR::Register * desiredBiasedExponent = cg->evaluate(node->getChild(3));
   cg->decReferenceCount(node->getChild(3));

   // sign extend into 64-bits
   TR::Register * desiredBiasedExponent2 = NULL;
   if (node->getChild(2)->getReferenceCount() == 0)
      desiredBiasedExponent2 = desiredBiasedExponent;
   else
      desiredBiasedExponent2 = cg->allocateRegister();

   generateRRInstruction(cg, TR::InstOpCode::LGFR, node, desiredBiasedExponent2, desiredBiasedExponent);
   desiredBiasedExponent = desiredBiasedExponent2;

   // The actual biased exponent
   TR::Register * actualBiasedExpRegister = cg->allocateRegister();
   generateRRInstruction(cg, TR::InstOpCode::EEDTR, node, actualBiasedExpRegister, resFPRegister);

   // Compare the two scales
   generateRRInstruction(cg, TR::InstOpCode::CR, node, desiredBiasedExponent, actualBiasedExpRegister);
   cg->stopUsingRegister(actualBiasedExpRegister);

   // check the condition code and branch
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, labelScalesEQUAL);

   /*
    *
    * PATH FOR WHEN SCALES ARE NOT EQUAL
    *
    */

   /*
     -clear inexact exception bits again,
     -rescale, and test the result
      -if tdgdt fails, return -1
      -if inexact from divide, return 0
           -else return 1
    */
   genClearFPCBits(node, cg, maskIEEEException);

   // need to rescale
   // load the DFP who we'll use as the dummy (containing the required exponent)
   TR::Register * dfpDummyFPRegister = cg->allocateRegister(TR_FPR);
   generateRRInstruction(cg, TR::InstOpCode::LDR, node, dfpDummyFPRegister, resFPRegister);

   // insert the biased exponent
   generateRRFInstruction(cg, TR::InstOpCode::IEDTR, node, dfpDummyFPRegister, desiredBiasedExponent, dfpDummyFPRegister);

   // perform the quantize
   generateRRFInstruction(cg, TR::InstOpCode::QADTR, node, resFPRegister, dfpDummyFPRegister, resFPRegister, USE_CURRENT_DFP_ROUNDING_MODE);
   cg->stopUsingRegister(dfpDummyFPRegister);

   genTestDataGroup(node, cg, resFPRegister, tdgdtScaledTest);

   // assume we failed
   generateLoad32BitConstant(cg, node, -1, retRegister, false);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BLR, node, labelEND);

   // now fall through to take the inexact exception checking path...

   /*
    *
    * PATH FOR WHEN SCALES ARE EQUAL
    *
    */
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelScalesEQUAL);

   /* store the resulting DFP, and then check:
       -inexact exception from divide happened - return 0
       -else return 1
   */
   genStoreDFP(node, cg, node->getFirstChild(), node->getSecondChild()->getSymbolReference(), resFPRegister);
   cg->decReferenceCount(node->getFirstChild());

   // check inexact exception bits for a failure
   generateRRInstruction(cg, TR::InstOpCode::EFPC, node, retRegister, retRegister);
   generateRILInstruction(cg, TR::InstOpCode::NILF, node, retRegister, excInexactFlag);
   generateRILInstruction(cg, TR::InstOpCode::CLFI, node, retRegister, 0); //compare against 0

   // assume inexact fail
   generateLoad32BitConstant(cg, node, 0, retRegister, false);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, labelEND);

   // otherwise inexact didn't fail
   generateLoad32BitConstant(cg, node, 1, retRegister, true);

   // what regs do we want alive all the way through?
   TR::RegisterDependencyConditions * deps = generateRegisterDependencyConditions(0, 3, cg);
   deps->addPostCondition(retRegister, TR::RealRegister::AssignAny);
   deps->addPostCondition(resFPRegister, TR::RealRegister::AssignAny);
   deps->addPostCondition(node->getFirstChild()->getRegister(), TR::RealRegister::AssignAny);
   cg->stopUsingRegister(resFPRegister);

   // end
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelEND, deps);
   if (toRound == 1) //since toRound == 0 taken care of above
      genSetDFPRoundingMode(node, cg, rmIEEEdefault);

   node->setRegister(retRegister);
   return retRegister;
   }

/*
private final int DFPDivide(long lhsDFP, long rhsDFP, bool checkForInexact, int rndFlag, int prec, int rm)
*/
extern TR::Register *
inlineBigDecimalDivide(
      TR::Node * node,
      TR::CodeGenerator *cg)
   {
   //printf("\t390-DFPower::inlineBDBinaryOp - DDTR\n");

   int32_t toRound = 0;
   int32_t checkInexact = 0;

   // see if we need to check for inexact result
   TR_ASSERT( node->getChild(3)->getOpCode().isLoadConst(), "inlineBigDecimalDivide: Unexpected type\n");
   checkInexact = node->getChild(3)->getInt();
   cg->decReferenceCount(node->getChild(3));

   // see if we need to perform rounding
   TR_ASSERT( node->getChild(4)->getOpCode().isLoadConst(), "inlineBigDecimalDivide: Unexpected Type\n");
   toRound = node->getChild(4)->getInt();
   cg->decReferenceCount(node->getChild(4));

   // load both operands into FPRs
   TR::Register * lhsFPRegister = genLoadDFP(node, cg, node->getSecondChild());
   TR::Register * rhsFPRegister = genLoadDFP(node, cg, node->getChild(2));
   TR::Register * rmRegister = NULL;
   // load precision for rounding
   TR::Register * precRegister = NULL;

   // Clear exception bits since we check after division
   if (checkInexact)
     genClearFPCBits(node, cg, maskIEEEException);

   // set DFP rounding mode to round for reround (i.e. no rounding)
   if ((toRound == 1) && (node->getChild(5)->getOpCode().isLoadConst() && node->getChild(5)->getInt() == 16))
      {
      rmRegister = cg->evaluate(node->getChild(6));
      genSetDFPRoundingMode(node, cg, rmRegister);
      }
   else if (toRound == 1 || toRound == 0)
      {
      if (toRound == 1)
         precRegister = cg->evaluate(node->getChild(5));
      genSetDFPRoundingMode(node, cg, rmRPSP);
      }
   cg->decReferenceCount(node->getChild(5));

   // Perform the divide
   TR::Register * resFPRegister = cg->allocateRegister(TR_FPR);
   TR::Register * retRegister = cg->allocateRegister();
   TR::LabelSymbol * labelEND = generateLabelSymbol(cg);

   TR::Instruction * instr = generateRRFInstruction(cg, TR::InstOpCode::DDTR, node, resFPRegister, rhsFPRegister, lhsFPRegister, USE_CURRENT_DFP_ROUNDING_MODE);
   cg->stopUsingRegister(lhsFPRegister); // can do this since longs end up in FPRs
   cg->stopUsingRegister(rhsFPRegister);

   /*
     -if need to check for inexact, but no rounding => no round, tdgdt, check inexact
      --will return either -1, 1, or 0
     -if need to check for inexact + rounding => round, tdgdt, check inexact
      --will return either -1, 1 or 0
     -if need to rounding but no inexact => round, tdgdt
      --will return either 1 or -1

    * JIT compiled return value:
    * Return 1 if JIT compiled and DFP divide is successful.
    * Return 0 if JIT compiled, but DFP divide was inexact
    * Return -1 if JIT compiled, but other exception (i.e. overflow)
   */

   // perform rounding
   if (toRound == 1 && rmRegister == NULL)
      {
      rmRegister = cg->evaluate(node->getChild(6));
      genSetDFPRoundingMode(node, cg, rmRegister);
      //perform the round
      generateRRFInstruction(cg, TR::InstOpCode::RRDTR, node, resFPRegister, precRegister, resFPRegister, USE_CURRENT_DFP_ROUNDING_MODE);
      }
   cg->decReferenceCount(node->getChild(6));

   // reset the rounding mode
   if (toRound == 1 || toRound == 0)
      {
      genSetDFPRoundingMode(node, cg, rmIEEEdefault);
      }

   // Test the data group and branch instruction for non-extremes, and zero leading digit
   genTestDataGroup(node, cg, resFPRegister, tdgdtLaxTest);


   // did we fail
   generateLoad32BitConstant(cg, node, -1, retRegister, false);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BLR, node, labelEND);
   //generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, labelEND);

   // store the result
   genStoreDFP(node, cg, node->getFirstChild(), node->getSecondChild()->getSymbolReference(), resFPRegister);
   cg->decReferenceCount(node->getFirstChild());

   if (checkInexact)
      {
      // did we fail?
      generateLoad32BitConstant(cg, node, 0, retRegister, true);

      // need to check for inexact exception
      generateRRInstruction(cg, TR::InstOpCode::EFPC, node, retRegister, retRegister);
      generateRILInstruction(cg, TR::InstOpCode::NILF, node, retRegister, excInexactFlag);
      generateRILInstruction(cg, TR::InstOpCode::CLFI, node, retRegister, 0); //compare against 0

      // did we fail?
      // 0 already loaded in return register
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, labelEND);
      }

   // otherwise we have a good value
   generateLoad32BitConstant(cg, node, 1, retRegister, true);

   // which registers do we need alive after branch sequence?
   TR::RegisterDependencyConditions * deps = generateRegisterDependencyConditions(0, 3, cg);
   deps->addPostCondition(retRegister, TR::RealRegister::AssignAny);
   deps->addPostCondition(resFPRegister, TR::RealRegister::AssignAny);
   deps->addPostCondition(node->getFirstChild()->getRegister(), TR::RealRegister::AssignAny);
   cg->stopUsingRegister(resFPRegister);

   // end
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelEND, deps);
   node->setRegister(retRegister);
   return retRegister;
   }

/*
private final boolean DFPRound(long dfpSrc, int precision, int rm)
*/
extern TR::Register *
inlineBigDecimalRound(
      TR::Node * node,
      TR::CodeGenerator *cg)
   {
   //printf("\t390-DFPower::inlineBDRound\n");

   // load DFP to be rounded
   TR::Register * dfpFPRegister = genLoadDFP(node, cg, node->getSecondChild());

   // load 32-bit precision to be used
   TR::Register * reqPrecRegister = cg->evaluate(node->getChild(2));
   cg->decReferenceCount(node->getChild(2));

   // Set the rounding mode
   TR::Register * rmRegister = cg->evaluate(node->getChild(3));
   cg->decReferenceCount(node->getChild(3));
   genSetDFPRoundingMode(node, cg, rmRegister);

   // perform the reround
   TR::Register * resRegister = cg->allocateRegister(TR_FPR);

   TR::Register * retRegister = cg->allocateRegister();
   TR::LabelSymbol * labelEND = generateLabelSymbol(cg);

   //TODO: RRDTR will throw exception
   TR::Instruction * instr = generateRRFInstruction(cg, TR::InstOpCode::RRDTR, node, resRegister, reqPrecRegister, dfpFPRegister, USE_CURRENT_DFP_ROUNDING_MODE);
   cg->stopUsingRegister(reqPrecRegister);
   cg->stopUsingRegister(dfpFPRegister);


   // Test the data group and branch instruction for non-extremes, and zero leading digit
   genTestDataGroup(node, cg, resRegister, tdgdtLaxTest);

   // did we fail?
   generateLoad32BitConstant(cg, node, 0, retRegister, false);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BLR, node, labelEND);

   // did we pass?
   genStoreDFP(node, cg, node->getFirstChild(), node->getSecondChild()->getSymbolReference(), resRegister);
   cg->decReferenceCount(node->getFirstChild());
   generateLoad32BitConstant(cg, node, 1, retRegister, true);

   // which registers should still be alive after branching sequence?
   TR::RegisterDependencyConditions * deps =
         generateRegisterDependencyConditions(0, 3, cg);
   deps->addPostCondition(retRegister, TR::RealRegister::AssignAny);
   deps->addPostCondition(resRegister, TR::RealRegister::AssignAny);
   deps->addPostCondition(node->getFirstChild()->getRegister(), TR::RealRegister::AssignAny);
   cg->stopUsingRegister(resRegister);

   // the end
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelEND, deps);

   // move it to here so OOL can also reset rounding mode
   // reset to default IEEE rounding mode
   genSetDFPRoundingMode(node, cg, rmIEEEdefault);

   node->setRegister(retRegister);
   return retRegister;
   }


/*
private final static int DFPCompareTo(long lhsDFP, long rhsDFP)
*/
extern TR::Register *
inlineBigDecimalCompareTo(
      TR::Node * node,
      TR::CodeGenerator * cg)
   {
   //printf("\t390-DFPower::inlineBDCompareTo\n");

   // load both operands into FPRs
   TR::Register * lhsFPRegister = genLoadDFP(node, cg, node->getFirstChild());
   TR::Register * rhsFPRegister = genLoadDFP(node, cg, node->getSecondChild());

   //return value address
   TR::Register * retRegister = cg->allocateRegister();

   //return label
   TR::LabelSymbol * labelEND = generateLabelSymbol(cg);

   // compare both values
   TR::Instruction * instr = generateRRInstruction(cg, TR::InstOpCode::CDTR, node, lhsFPRegister, rhsFPRegister);

   cg->stopUsingRegister(lhsFPRegister);
   cg->stopUsingRegister(rhsFPRegister);


   // Create a post-dependency for the return reg
   TR::RegisterDependencyConditions * deps =
         generateRegisterDependencyConditions(0, 1, cg);
   deps->addPostCondition(retRegister, TR::RealRegister::AssignAny);

   // was op1 < op2 ?
   generateLoad32BitConstant(cg, node, -1, retRegister, false);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BLR, node, labelEND);

   // was op1 > op2 ?
   generateLoad32BitConstant(cg, node, 1, retRegister, false);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BHR, node, labelEND);

   // op1 == op2
   generateLoad32BitConstant(cg, node, 0, retRegister, true);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelEND, deps);

   node->setRegister(retRegister);
   //cg->stopUsingRegister(retRegister);
   return retRegister;
   }

/*
private final static long DFPBCDDigits(long dfp)
private final static int DFPSignificance(long dfp)
private final static int DFPExponent(long dfp)
*/
extern TR::Register *
inlineBigDecimalUnaryOp(
      TR::Node * node,
      TR::CodeGenerator * cg,
      TR::InstOpCode::Mnemonic op)
   {
   /*
   printf("\t390-DFPower::inlineBDUnaryOp - ");

   switch(op)
      {
      case TR::InstOpCode::EEDTR:  printf("EEDTR - extract biased exponent\n");     break;
      case TR::InstOpCode::ESDTR:  printf("ESDTR - extract significance\n");        break;
      case TR::InstOpCode::CUDTR:  printf("CUDTR - convert to unsigned BCD\n");     break;
      default:        printf("\n");                                    break;
      }
   */


   // load DFP
   TR::Register * dfpFPRegister = genLoadDFP(node, cg, node->getFirstChild());

   // perform the operation
   TR::Register * resRegister = cg->allocateRegister();
   TR::Instruction* cursor = generateRRInstruction(cg, op, node, resRegister, dfpFPRegister);
   cg->stopUsingRegister(dfpFPRegister);

   node->setRegister(resRegister);
   return resRegister;
   }

/*
private final int DFPSetScale(long srcDFP, int biasedExp, boolean round, int rm, boolean checkInexact)
*/
extern TR::Register *
inlineBigDecimalSetScale(
      TR::Node * node,
      TR::CodeGenerator * cg)
   {
   //printf("\t390-DFPower::inlineBDSetScale\n");

   // see if we need to perform rounding
   TR_ASSERT( node->getChild(3)->getOpCode().isLoadConst(), "inlineBigDecimalSetScale: Unexpected Type\n");
   int32_t toRound = node->getChild(3)->getInt();
   cg->decReferenceCount(node->getChild(3));

   // see if we need to check for inexact exceptions
   TR_ASSERT( node->getChild(5)->getOpCode().isLoadConst(), "inlineBigDecimalSetScale: Unexpected Type\n");
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
   TR::Register * dfpFPRegister = genLoadDFP(node, cg, node->getSecondChild());

   // load the DFP who we'll use as the dummy (containing the required exponent)
   TR::Register * dfpDummyFPRegister = cg->allocateRegister(TR_FPR);
   generateRRInstruction(cg, TR::InstOpCode::LDR, node, dfpDummyFPRegister, dfpFPRegister);

   // load and sign extend the 32-bit sint target exponent
   TR::Register * expRegister = cg->gprClobberEvaluate(node->getChild(2));
   cg->decReferenceCount(node->getChild(2));
   generateRRInstruction(cg, TR::InstOpCode::LGFR, node, expRegister, expRegister);

   TR::LabelSymbol * labelEND = generateLabelSymbol(cg);
   TR::Register * retRegister = cg->allocateRegister();

   // insert the biased exponent
   TR::Instruction * instr = generateRRFInstruction(cg, TR::InstOpCode::IEDTR, node, dfpDummyFPRegister, expRegister, dfpDummyFPRegister);
   cg->stopUsingRegister(expRegister); // since clobber eval'd

   // Clear exception bits since we check them after scaling
   if (inexCheck)
      genClearFPCBits(node, cg, maskIEEEException);

   // perform the quantize
   TR::Register * resRegister = cg->allocateRegister(TR_FPR);
   generateRRFInstruction(cg, TR::InstOpCode::QADTR, node, resRegister, dfpDummyFPRegister, dfpFPRegister, USE_CURRENT_DFP_ROUNDING_MODE);
   cg->stopUsingRegister(dfpDummyFPRegister);
   cg->stopUsingRegister(dfpFPRegister);

   // reset to default IEEE rounding mode
   genSetDFPRoundingMode(node, cg, rmIEEEdefault);

   // assume we failed
   generateLoad32BitConstant(cg, node, 0, retRegister, true);

   // need to check for inexact exception, since scaling might change
   // the value of the DFP - we can't allow this....
   if (inexCheck)
      {
      generateRRInstruction(cg, TR::InstOpCode::EFPC, node, retRegister, retRegister);
      generateRILInstruction(cg, TR::InstOpCode::NILF, node, retRegister, excInexactFlag);
      generateRILInstruction(cg, TR::InstOpCode::CLFI, node, retRegister, 0); //compare against 0

      // did we fail?
      // 0 already loaded in return register
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, labelEND);
      }

   // Test the data group and branch instruction for non-extremes, and zero leading digit
   genTestDataGroup(node, cg, resRegister, tdgdtScaledTest);

   // did we fail?
   generateLoad32BitConstant(cg, node, -1, retRegister, false);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BLR, node, labelEND);

   // did we pass?
   genStoreDFP(node, cg, node->getFirstChild(), node->getSecondChild()->getSymbolReference(), resRegister);
   cg->decReferenceCount(node->getFirstChild());
   generateLoad32BitConstant(cg, node, 1, retRegister, true);

   // which registers should still be alive after branching sequence?
   TR::RegisterDependencyConditions * deps =
         generateRegisterDependencyConditions(0, 3, cg);
   deps->addPostCondition(retRegister, TR::RealRegister::AssignAny);
   deps->addPostCondition(resRegister, TR::RealRegister::AssignAny);
   deps->addPostCondition(node->getFirstChild()->getRegister(), TR::RealRegister::AssignAny);
   cg->stopUsingRegister(resRegister);

   // the end
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelEND, deps);
   node->setRegister(retRegister);
   return retRegister;
   }

/*
private final static long DFPUnscaledValue(long srcDFP)
*/
extern TR::Register *
inlineBigDecimalUnscaledValue(
      TR::Node * node,
      TR::CodeGenerator * cg)
   {
   //printf("\t390-DFPower::inlineBDUnscaledValue\n");
   // will store the returned digits
   TR::Register * retRegister = cg->allocateRegister();

   // load DFP to be quantized
   TR::Register * dfpFPRegister = genLoadDFP(node, cg, node->getFirstChild());

   // load the DFP who we'll use as the dummy (containing the required exponent)
   TR::Register * dfpDummyFPRegister = cg->allocateRegister(TR_FPR);
   generateRRInstruction(cg, TR::InstOpCode::LDR, node, dfpDummyFPRegister, dfpFPRegister);

   // load and sign extend biased exponent 0 (398)
   TR::Register * expRegister = cg->allocateRegister();
   generateLoad32BitConstant(cg, node, 398, expRegister, true);
   generateRRInstruction(cg, TR::InstOpCode::LGFR, node, expRegister, expRegister);

   // insert the biased exponent
   TR::Instruction * instr = generateRRFInstruction(cg, TR::InstOpCode::IEDTR, node, dfpDummyFPRegister, expRegister, dfpDummyFPRegister);
   cg->stopUsingRegister(expRegister); // since allocated temp
   cg->stopUsingRegister(dfpDummyFPRegister);
   cg->stopUsingRegister(dfpFPRegister);


   // otherwise convert to long
   instr = generateRRInstruction(cg, TR::InstOpCode::CGDTR, node, retRegister, dfpDummyFPRegister);

   cg->stopUsingRegister(dfpDummyFPRegister);
   node->setRegister(retRegister);

   return retRegister;
   }

// bool DFPConvertPackedToDFP(long longPack, int scale, boolean sign)
// method signature: TR::java_math_BigDecimal_DFPConvertPackedToDFP, "DFPConvertPackedToDFP", "(JIZ)Z"
extern TR::Register *
inlineBigDecimalFromPackedConverter(
      TR::Node * node,
      TR::CodeGenerator *cg)
   {
   // see if we need to check for inexact exceptions
   TR_ASSERT( node->getChild(3)->getOpCode().isLoadConst(), "inlineBigDecimalFromPackedConverter: Unexpected Type\n");
   int32_t signedPacked = node->getChild(3)->getInt();

   // load the packed decimal
   bool clobbered = false;
   TR::Register * valRegister = cg->evaluate(node->getSecondChild());

   //move rv register here, so it can be accessed by both OOL path and fast path
   TR::Register * retRegister = cg->allocateRegister();

   // convert to dfp (signed or unsigned)

   TR::Register * dfpRegister = cg->allocateRegister(TR_FPR);
   TR::Instruction * instr;
   if(signedPacked == 1)
      {
      instr = generateRRInstruction(cg, TR::InstOpCode::CDSTR, node, dfpRegister, valRegister);
      }
   else
      {
      instr = generateRRInstruction(cg, TR::InstOpCode::CDUTR, node, dfpRegister, valRegister);
      }
   TR::Instruction * bdInstr = instr;

   instr->setNeedsGCMap(0x0000FFFF);

   //CDSTR/CDUTR only has 4byte length, so append 2bytes
   TR::Instruction * nop = new (cg->trHeapMemory()) TR::S390NOPInstruction(TR::InstOpCode::NOP, 2, node, cg);

   TR::LabelSymbol * passThroughLabel = generateLabelSymbol(cg);
   TR::LabelSymbol * jump2callSymbol = generateLabelSymbol(cg);
   TR::S390RestoreGPR7Snippet * restoreSnippet = new (cg->trHeapMemory()) TR::S390RestoreGPR7Snippet(cg, node,
                                                                                                   generateLabelSymbol(cg), jump2callSymbol);
   cg->addSnippet(restoreSnippet);
   TR::Instruction * brcInstr = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_NOP, node, restoreSnippet->getSnippetLabel());

   //setting OOL path
   TR::Instruction * cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_NOP, node, jump2callSymbol);

   TR_S390OutOfLineCodeSection * outlinedHelperCall = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(jump2callSymbol, passThroughLabel, cg);
   cg->getS390OutOfLineCodeSectionList().push_front(outlinedHelperCall);

   //switch to OOL generation
   outlinedHelperCall->swapInstructionListsWithCompilation();
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, jump2callSymbol);

   TR_Debug * debugObj = cg->getDebug();
   if (debugObj)
      debugObj->addInstructionComment(cursor, "Denotes start of DFP DFPConvertPackedToDFP OOL BCDCHK sequence");

   generateLoad32BitConstant(cg, node, 0, retRegister, true);

   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, passThroughLabel);

   if (debugObj)
      debugObj->addInstructionComment(cursor, "Denotes end of DFP DFPConvertPackedToDFP OOL BCDCHK sequence: return to mainline");

   //we will have a label so that our OOL path could be redirected to the end of inlining method.
   outlinedHelperCall->swapInstructionListsWithCompilation();

   //put dec reference here because we may need to use it to actually evaluate the call node
   cg->decReferenceCount(node->getSecondChild());
   cg->decReferenceCount(node->getChild(3));

   if(clobbered)
      {
      cg->stopUsingRegister(valRegister);
      }

   // load and sign extend the the exponent
   TR::Register * expRegister = NULL;
   clobbered = false;
   if (node->getChild(2)->getReferenceCount() > 1)
      {
      clobbered=true;
      expRegister = cg->gprClobberEvaluate(node->getChild(2));
      }
   else
      {
      expRegister = cg->evaluate(node->getChild(2));
      }
   cg->decReferenceCount(node->getChild(2));
   generateRRInstruction(cg, TR::InstOpCode::LGFR, node, expRegister, expRegister);

   // TR::InstOpCode::IEDTR also potentially throws exceptions
   instr = generateRRFInstruction(cg, TR::InstOpCode::IEDTR, node, dfpRegister, expRegister, dfpRegister);

   if(clobbered)
      {
      cg->stopUsingRegister(valRegister);
      }

   // store the dfp value into the BigDecimal object
   genStoreDFP(node, cg, node->getFirstChild(), NULL, dfpRegister);
   cg->decReferenceCount(node->getFirstChild());
   cg->stopUsingRegister(dfpRegister);

   // put true in the return value
   generateLoad32BitConstant(cg, node, 1, retRegister, false);

   TR::Register * sourceRegister = bdInstr->getRegisterOperand(2);
   TR::Register * targetRegister = bdInstr->getRegisterOperand(1);
   TR::RegisterDependencyConditions * deps = generateRegisterDependencyConditions(0, 4, cg);

   TR::TreeEvaluator::addToRegDep(deps, sourceRegister, true);
   TR::TreeEvaluator::addToRegDep(deps, targetRegister, true);

   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, passThroughLabel, deps);

   node->setRegister(retRegister);

   //cg->stopUsingRegister(retRegister);
   return retRegister;
   }

// static long DFPConvertDFPToPacked(long dfp, boolean sign)
extern TR::Register *
inlineBigDecimalToPackedConverter(
      TR::Node * node,
      TR::CodeGenerator *cg)
   {
   // see if we need to check for inexact exceptions
   TR_ASSERT( node->getSecondChild()->getOpCode().isLoadConst(), "inlineBigDecimalToPackedConverter: Unexpected Type\n");
   int32_t signedPacked = node->getSecondChild()->getInt();
   cg->decReferenceCount(node->getSecondChild());

   // load the dfp value into register
   TR::Register * dfpRegister = genLoadDFP(node, cg, node->getFirstChild());

   // convert the dfp value into packed decimal
   TR::Register * retRegister = cg->allocateRegister();
   if(signedPacked == 1)
      {
      generateRRInstruction(cg, TR::InstOpCode::CSDTR, node, retRegister, dfpRegister);
      }
   else
      {
      generateRRInstruction(cg, TR::InstOpCode::CUDTR, node, retRegister, dfpRegister);
      }

   cg->stopUsingRegister(dfpRegister);  // FIXME: investigate whether this is needed

   // return the long value in a register
   node->setRegister(retRegister);
   return retRegister;
   }


inline TR::Register *
dfp2fpHelper(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * srcReg = cg->evaluate(firstChild);

   TR::DataType srcType = firstChild -> getDataType();
   TR::DataType tgtType = node -> getDataType();

   TR::Register *r0Reg = cg->allocateRegister();
   TR::Register *r1Reg = cg->allocateRegister();
   TR::RegisterDependencyConditions * deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(3, 6, cg);
   deps->addPreCondition(r0Reg, TR::RealRegister::GPR0);
   deps->addPostCondition(r0Reg, TR::RealRegister::GPR0);
   deps->addPostCondition(r1Reg, TR::RealRegister::GPR1);

   TR::Register * srcCopy;
   TR::Register * lowSrcReg;
   TR::Register * highSrcReg;
   if (srcType == TR::DecimalLongDouble)
      {
      lowSrcReg = cg->allocateRegister(TR_FPR);
      highSrcReg = cg->allocateRegister(TR_FPR);
      deps->addPreCondition(highSrcReg, TR::RealRegister::FPR4);
      deps->addPostCondition(highSrcReg, TR::RealRegister::FPR4);
      deps->addPreCondition(lowSrcReg, TR::RealRegister::FPR6);
      deps->addPostCondition(lowSrcReg, TR::RealRegister::FPR6);
      srcCopy = cg->allocateFPRegisterPair(lowSrcReg, highSrcReg);
      }
   else
      {
      srcCopy = cg->allocateRegister(TR_FPR);
      deps->addPreCondition(srcCopy, TR::RealRegister::FPR4);
      deps->addPostCondition(srcCopy, TR::RealRegister::FPR4);
      }

   TR::InstOpCode::Mnemonic loadOp;
   if (srcType == TR::Float || srcType == TR::DecimalFloat)
      loadOp = TR::InstOpCode::LER;
   else if (srcType == TR::Double || srcType == TR::DecimalDouble)
      loadOp = TR::InstOpCode::LDR;
   else // must be long double
      loadOp = TR::InstOpCode::LXR;

   generateRRInstruction(cg, loadOp, node, srcCopy, srcReg);

   TR::Register * tgtCopy;
   TR::Register * lowTgtCopy;
   TR::Register * highTgtCopy;
   if (tgtType == TR::DecimalLongDouble)
      {
      lowTgtCopy = cg->allocateRegister(TR_FPR);
      highTgtCopy = cg->allocateRegister(TR_FPR);
      deps->addPostCondition(highTgtCopy, TR::RealRegister::FPR0);
      deps->addPostCondition(lowTgtCopy, TR::RealRegister::FPR2);
      tgtCopy = cg->allocateFPRegisterPair(lowTgtCopy, highTgtCopy);
      }
   else
      {
      tgtCopy = cg->allocateRegister(TR_FPR);
      deps->addPostCondition(tgtCopy, TR::RealRegister::FPR0);
      }

   // The conversion operation is specified by the function code in general register 0
   // condition code is set to indicate the result. When there are no exceptional
   // conditions, condition code 0 is set. When an IEEE nontrap exception is recognized, condition
   // code 1 is set. When an IEEE trap exception with alternate action is recognized, condition code 2
   // is set. A 32-bit return code is placed in bits 32-63 of general register 1; bits 0-31 of
   // general register 1 remain unchanged.

   // For the PFPO-convert-floatingpoint-radix operation, the second operand is converted
   // to the format of the first operand and placed at the first-operand location, a return code is placed in
   // bits 32-63 of general register 1, and the condition code is set to indicate whether an exceptional condition
   // was recognized.  The first and second operands are in implicit floatingpoint registers. The first operand
   // is in FPR0 (paired with FPR2 for extended). The second operand is in FPR4 (paired with FPR6 for extended).
/*******************************************************
     bit 32: test bit
               0 -- operation is performed based on bit 33-63 (must be valid values);
               1 -- operation is not performed, but set condition code, but, instead, the
                    condition code is set to indicate whether these bits specify a valid and installed function;
      bits 33-39:   specify the operation type. Only one operation type is currently defined: 01, hex,
                    is PFPO Convert Floating-Point Radix.
      bits 40-47:   PFPO-operand-format code for operand 1
      bits 48-55:   PFPO-operand-format code for operand 2
      bits 56:      Inexact-suppression control
      bits 57:      Alternate-exception-action control
      bits 58-59:   Target-radix-dependent controls
      bits 60-63:   PFPO rounding method
**********************************************************/
   int32_t funcCode = 0x01 << 24;   // perform radix conversion
   int32_t srcTypeCode, tgtTypeCode;
   switch (tgtType)
      {
      case TR::DecimalFloat:
         tgtTypeCode = 0x8;
         break;
      case TR::DecimalDouble:
         tgtTypeCode = 0x9;
         break;
      case TR::DecimalLongDouble:
         tgtTypeCode = 0xA;
         break;
      case TR::Float:
         tgtTypeCode = 0x5;
         break;
      case TR::Double:
         tgtTypeCode = 0x6;
         break;
      default:
         TR_ASSERT( 0, "Error: unsupported target data type for PFPO");
         break;
      }
   funcCode |= tgtTypeCode << 16;

   switch (srcType)
      {
      case TR::DecimalFloat:
         srcTypeCode = 0x8;
         break;
      case TR::DecimalDouble:
         srcTypeCode = 0x9;
         break;
      case TR::DecimalLongDouble:
         srcTypeCode = 0xA;
         break;
      case TR::Float:
         srcTypeCode = 0x5;
         break;
      case TR::Double:
         srcTypeCode = 0x6;
         break;
      default:
         TR_ASSERT( 0, "Error: unsupported src data type for PFPO");
         break;
      }

   funcCode |= srcTypeCode << 8;


   generateRILInstruction(cg, TR::InstOpCode::IILF, node, r0Reg, funcCode);

   generateS390EInstruction(cg, TR::InstOpCode::PFPO, node, tgtCopy, r1Reg, srcCopy, r0Reg, deps);

   cg->stopUsingRegister(r0Reg);
   cg->stopUsingRegister(r1Reg);
   cg->stopUsingRegister(srcCopy);
   cg->decReferenceCount(firstChild);
   node->setRegister(tgtCopy);
   return tgtCopy;
   }

TR::Register *
J9::Z::TreeEvaluator::df2fEvaluator( TR::Node * node, TR::CodeGenerator * cg)
   {
   return dfp2fpHelper(node, cg);
   }

TR::Register *
J9::Z::TreeEvaluator::f2dfEvaluator( TR::Node * node, TR::CodeGenerator * cg)
   {
   return dfp2fpHelper(node, cg);
   }

/**
 * Convert Decimal float/Double/LongDouble to signed long int in 64 bit mode;
 */
inline TR::Register *
dfp2l64(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * srcReg = cg->evaluate(firstChild);
   TR::Register * tempDDReg = NULL;
   TR::Register * targetReg = cg->allocateRegister();

   TR::InstOpCode::Mnemonic convertOpCode;
   switch (firstChild->getDataType())
      {
      case TR::DecimalFloat:
         convertOpCode = TR::InstOpCode::CGDTR;
         tempDDReg = cg->allocateRegister(TR_FPR);
         break;
      case TR::DecimalDouble:
         convertOpCode = TR::InstOpCode::CGDTR;
         tempDDReg = srcReg;
         break;
      case TR::DecimalLongDouble:
         convertOpCode = TR::InstOpCode::CGXTR;
         tempDDReg = srcReg;
         break;
      default:
         TR_ASSERT( 0, "dfpToLong: unsupported opcode\n");
         break;
      }

   // Float to double
    if (firstChild->getDataType() == TR::DecimalFloat)
        generateRRFInstruction(cg, TR::InstOpCode::LDETR, node, tempDDReg, srcReg, 0, false);

   generateRRFInstruction(cg, convertOpCode, node, targetReg, tempDDReg, 0x9, true);

   cg->decReferenceCount(firstChild);
   if (firstChild->getDataType() == TR::DecimalFloat)
     cg->stopUsingRegister(tempDDReg);

   node->setRegister(targetReg);
   return targetReg;
   }

/**
 * Convert Decimal float/Double/LongDouble to signed long int in 64 bit mode;
 */
inline TR::Register *
dfp2lu64(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * srcReg = cg->evaluate(firstChild);
   TR::Register * tempDDReg = NULL;
   TR::Register * targetReg = cg->allocateRegister();

   TR::InstOpCode::Mnemonic convertOpCode;
   switch (firstChild->getDataType())
      {
      case TR::DecimalFloat:
         convertOpCode = TR::InstOpCode::CGDTR;
         tempDDReg = cg->allocateRegister(TR_FPR);
         break;
      case TR::DecimalDouble:
         convertOpCode = TR::InstOpCode::CGDTR;
         tempDDReg = srcReg;
         break;
      case TR::DecimalLongDouble:
         convertOpCode = TR::InstOpCode::CGXTR;
         tempDDReg = srcReg;
         break;
      default:
         TR_ASSERT( 0, "dfpToLong: unsupported opcode\n");
         break;
      }

   // Float to double
    if (firstChild->getDataType() == TR::DecimalFloat)
        generateRRFInstruction(cg, TR::InstOpCode::LDETR, node, tempDDReg, srcReg, 0, false);

   generateRRFInstruction(cg, convertOpCode, node, targetReg, tempDDReg, 0x9, true);

   cg->decReferenceCount(firstChild);
   if (firstChild->getDataType() == TR::DecimalFloat)
     cg->stopUsingRegister(tempDDReg);

   node->setRegister(targetReg);
   return targetReg;

   }

/**
 * Convert to signed long to Decimal float/Double/LongDouble in 64bit mode
 */
inline TR::Register *
l2dfp64(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * srcReg = cg->evaluate(firstChild);

   TR::Register * targetReg;

   TR::InstOpCode::Mnemonic convertOpCode;
   bool isFloatTgt = false;
   if( node->getDataType() != TR::DecimalLongDouble)
      targetReg = cg->allocateRegister(TR_FPR);
   else
      targetReg = cg->allocateFPRegisterPair();

   switch (node->getDataType())
      {
      case TR::DecimalDouble  :
         convertOpCode = TR::InstOpCode::CDGTR;
         break;
      case TR::DecimalLongDouble  :
         convertOpCode = TR::InstOpCode::CXGTR;
         break;
      default         :
         TR_ASSERT( 0,"l2dfp: unsupported data types");
         return NULL;
      }
   //convert to DFP
   generateRRInstruction(cg, convertOpCode, node, targetReg, srcReg);
   node->setRegister(targetReg);
   cg->decReferenceCount(firstChild);
   return targetReg;
   }

/**
 * Convert to unsigned long to Decimal float/Double/LongDouble in 64bit mode
 */
inline TR::Register *
lu2dfp64(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * srcReg = cg->evaluate(firstChild);
   TR::Register * tempReg = cg->allocateRegister();
   TR::Register * tempDEReg = NULL;
   TR::Compilation *comp = cg->comp();

   generateRRInstruction(cg, TR::InstOpCode::LGR, node, tempReg, srcReg);
   // subtract (INTMAX + 1) from the upper 32 bit register using x_or operation.
   generateRILInstruction(cg, TR::InstOpCode::XIHF, node, tempReg, 0x80000000);

   TR::Register * targetReg;
   if( node->getDataType() != TR::DecimalLongDouble)
      {
      targetReg = cg->allocateRegister(TR_FPR);
      tempDEReg = cg->allocateFPRegisterPair();
      generateRRInstruction(cg, TR::InstOpCode::CXGTR, node, tempDEReg, tempReg);
      }
   else
      {
      targetReg = cg->allocateFPRegisterPair();
      generateRRInstruction(cg, TR::InstOpCode::CXGTR, node, targetReg, tempReg);
      }

   // add back (INTMAX + 1)
   int64_t value1 = 0x2208000000000000LL;
   int64_t value2 = 0x948DF20DA5CFD42ELL;

   size_t offset1 = cg->findOrCreateLiteral(&value1, 8);
   size_t offset2 = cg->findOrCreateLiteral(&value2, 8);

   TR::Register * litReg = cg->allocateRegister();
   generateLoadLiteralPoolAddress(cg, node, litReg);

   TR::MemoryReference * mrHi = new (cg->trHeapMemory()) TR::MemoryReference(litReg, offset1, cg);
   TR::MemoryReference * mrLo = new (cg->trHeapMemory()) TR::MemoryReference(litReg, offset2, cg);

   TR::Register * tempMaxReg = cg->allocateFPRegisterPair();
   generateRXInstruction(cg, TR::InstOpCode::LD, node, tempMaxReg->getHighOrder(), mrHi);
   generateRXInstruction(cg, TR::InstOpCode::LD, node, tempMaxReg->getLowOrder(), mrLo);
   TR::Register * tempTgtReg = NULL;

   if( node->getDataType() != TR::DecimalLongDouble)
      {
      generateRRRInstruction(cg, TR::InstOpCode::AXTR, node, tempDEReg, tempDEReg, tempMaxReg);
      uint8_t m3 = 0x0, m4 =0x0;
      tempTgtReg = cg->allocateFPRegisterPair();
      generateRRFInstruction(cg, TR::InstOpCode::LDXTR, node, tempTgtReg, tempDEReg, m3, m4);
      generateRRInstruction(cg, TR::InstOpCode::LDR, node, targetReg, tempTgtReg->getHighOrder());
      }
   else
      generateRRRInstruction(cg, TR::InstOpCode::AXTR, node, targetReg, targetReg, tempMaxReg);

   cg->stopUsingRegister(tempReg);
   cg->stopUsingRegister(tempMaxReg);
   mrHi->stopUsingMemRefRegister(cg);
   mrLo->stopUsingMemRefRegister(cg);
   if( node->getDataType() != TR::DecimalLongDouble)
      {
      cg->stopUsingRegister(tempDEReg);
      cg->stopUsingRegister(tempTgtReg);
      }

   node->setRegister(targetReg);
   cg->decReferenceCount(firstChild);
   return targetReg;
   }

/************************************************************************************************
Convert Decimal float/Double/LongDouble to fixed number i.e. int,short,byte, or char
Conversion is performed in compliance with what's described in 'zOS XLC V1R10 Language Reference'
  -  Floating-point (binary or decimal) to integer conversion
     The fractional part is discarded (i.e., the value is truncated toward zero).
     If the value of the integral part cannot be represented by the integer type,
     the result is one of the following:
       - If the integer type is unsigned, the result is the largest representable number if the
           floating-point number is positive, or 0 otherwise.
       - If the integer type is signed, the result is the most negative or positive representable
           number according to the sign of the floating-point number
currently tobey follows this rule for DFP to signed/unsigned int, but seems not for short/byte types
     where the upper bits are simple choped off, i think it's just a bug in tobey

     DFP to fixed type conversions are expanded in WCode IlGen phase, so don't need it here anymore
inline TR::Register *
dfpToFixed(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR_ASSERT( 0,"Error: dfpToFixed() -- should not be here ");
   return NULL;
   }

************************************************************************************************/

/**
 * Convert to Decimal Double/LongDouble from fixed number i.e. int,short,byte, or char
 */
inline TR::Register *
fixedToDFP(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * srcRegister = cg->evaluate(firstChild);

   TR::Register * targetRegister;
   TR::Register * lowReg, * highReg;

   TR::InstOpCode::Mnemonic convertOpCode;
   // convert the srcRegister value to appropriate type, if needed
   int32_t nshift = 0;
   bool isSrcUnsigned = false;
   bool isFloatTgt = false;
   if( node->getDataType() != TR::DecimalLongDouble)
      targetRegister = cg->allocateRegister(TR_FPR);
   else
      {
      lowReg = cg->allocateRegister(TR_FPR);
      highReg = cg->allocateRegister(TR_FPR);
      targetRegister = cg->allocateFPRegisterPair(lowReg, highReg);
      }

   TR::Register *tempReg = cg->allocateRegister();
   TR::RegisterDependencyConditions * deps = NULL;
   if (TR::Compiler->target.is32Bit())
      {
      deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 1, cg);
      deps->addPostCondition(tempReg, TR::RealRegister::GPR0);
      }

   switch (node->getOpCodeValue())
     {
     case TR::b2df:
     case TR::b2dd:
     case TR::b2de:
     case TR::s2df:
     case TR::s2dd:
     case TR::s2de:
     case TR::i2df:
     case TR::i2dd:
     case TR::i2de:
         generateRRInstruction(cg, TR::InstOpCode::LGFR, node, tempReg,srcRegister);
         break;
     case TR::bu2df:
     case TR::bu2dd:
     case TR::bu2de:
         nshift = 56;
         break;
     case TR::su2df:
     case TR::su2dd:
     case TR::su2de:
         nshift = 48;
         break;
     case TR::iu2df:
     case TR::iu2dd:
     case TR::iu2de:
         nshift = 32;
         isSrcUnsigned = true;
         generateRSInstruction(cg, TR::InstOpCode::SLLG, node, tempReg, srcRegister, nshift);
         generateRSInstruction(cg, TR::InstOpCode::SRLG, node, tempReg, tempReg, nshift);
         break;
      default:
         TR_ASSERT( 0, "fixedToDFP: unsupported opcode\n");
         break;
      }
   switch (node->getDataType())
      {
      case TR::DecimalDouble  :
         convertOpCode = TR::InstOpCode::CDGTR;
         break;
      case TR::DecimalLongDouble  :
         convertOpCode = TR::InstOpCode::CXGTR;
         break;
      default         :
         TR_ASSERT(false, "should be unreachable");
         return NULL;
      }
   //convert to DFP
   generateRRInstruction(cg, convertOpCode, node, targetRegister, tempReg);
   cg->stopUsingRegister(tempReg);
   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   return targetRegister;
   }

TR::Register *
J9::Z::TreeEvaluator::i2ddEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return fixedToDFP(node,cg);
   }

TR::Register *
J9::Z::TreeEvaluator::dd2lEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return dfp2l64(node, cg);
   }

TR::Register *
J9::Z::TreeEvaluator::dd2luEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return dfp2lu64(node, cg);
   }

TR::Register *
J9::Z::TreeEvaluator::l2ddEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return l2dfp64(node,cg);
   }

TR::Register *
J9::Z::TreeEvaluator::lu2ddEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return lu2dfp64(node, cg);
   }

TR::Register *
J9::Z::TreeEvaluator::df2ddEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * targetReg = cg->allocateRegister(TR_FPR);
   TR::Register * srcReg = cg->evaluate(firstChild);

   uint8_t m4 =0x0;
   generateRRFInstruction(cg, TR::InstOpCode::LDETR, node, targetReg, srcReg, m4, false);
   node->setRegister(targetReg);
   cg->decReferenceCount(firstChild);
   cg->stopUsingRegister(srcReg);
   return targetReg;
   }

TR::Register *
J9::Z::TreeEvaluator::df2deEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * tmpReg = cg->allocateRegister(TR_FPR);
   TR::Register * targetReg = cg->allocateFPRegisterPair();
   TR::Register * srcReg = cg->evaluate(firstChild);
   uint8_t m4 =0x0;
   generateRRFInstruction(cg, TR::InstOpCode::LDETR, node, tmpReg, srcReg, m4, false);
   generateRRFInstruction(cg, TR::InstOpCode::LXDTR, node, targetReg, tmpReg, m4, false);
   cg->stopUsingRegister(srcReg);
   cg->stopUsingRegister(tmpReg);
   cg->decReferenceCount(firstChild);
   node->setRegister(targetReg);
   return targetReg;
   }

TR::Register *
J9::Z::TreeEvaluator::dd2dfEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * targetReg = cg->allocateRegister(TR_FPR);
   TR::Register * srcReg = cg->evaluate(firstChild);
   uint8_t m3 =0x0, m4 =0x0;
   generateRRFInstruction(cg, TR::InstOpCode::LEDTR, node, targetReg, srcReg, m3, m4);
   node->setRegister(targetReg);
   cg->decReferenceCount(firstChild);
   cg->stopUsingRegister(srcReg);
   return targetReg;
   }

TR::Register *
J9::Z::TreeEvaluator::dd2deEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * targetReg = cg->allocateFPRegisterPair();
   TR::Register * srcReg = cg->evaluate(firstChild);
   uint8_t m4 =0x0;
   generateRRFInstruction(cg, TR::InstOpCode::LXDTR, node, targetReg, srcReg, m4, false);
   node->setRegister(targetReg);
   cg->decReferenceCount(firstChild);
   cg->stopUsingRegister(srcReg);
   return targetReg;
   }

TR::Register *
J9::Z::TreeEvaluator::de2dxHelperAndSetRegister(TR::Register *targetReg, TR::Register *srcReg, TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getDataType() == TR::DecimalDouble || node->getDataType() == TR::DecimalFloat,"expecting node %s (%p) dataType to be DecimalDouble or DecimalFloat\n",node->getOpCode().getName(),node);
   uint8_t m3 = 0x0;
   uint8_t m4 = 0x0;
   generateRRFInstruction(cg, TR::InstOpCode::LDXTR, node, targetReg, srcReg, m3, m4);
   if (node->getDataType() == TR::DecimalFloat)
      generateRRFInstruction(cg, TR::InstOpCode::LEDTR, node, targetReg->getHighOrder(), targetReg->getHighOrder(), m3, m4);

   // The converted dd value is in the high order part of targetReg but the low order and the reg pair itself
   // have to be killed -- in order to do this the high is first set on the node and then the low and pair are
   // killed by calling stopUsingRegister.
   TR::Register *newTargetReg = node->setRegister(targetReg->getHighOrder());
   cg->stopUsingRegister(targetReg);
   targetReg = newTargetReg;
   return targetReg;
   }

/**
 * Handles TR::de2dd, TR::de2df
 */
TR::Register *
J9::Z::TreeEvaluator::de2ddEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * srcReg = cg->evaluate(firstChild);
   TR::Register * targetReg = cg->allocateFPRegisterPair();
   targetReg = de2dxHelperAndSetRegister(targetReg, srcReg, node, cg);
   cg->decReferenceCount(firstChild);
   return targetReg;
   }

TR::Register *
J9::Z::TreeEvaluator::dfaddEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register* sreg1 = cg->fprClobberEvaluate(node->getFirstChild());
   TR::Register* sreg2 = cg->fprClobberEvaluate(node->getSecondChild());
   TR::Register* treg = cg->allocateRegister(TR_FPR);
   TR::Register* resReg = cg->allocateRegister(TR_FPR);

   generateRRFInstruction(cg, TR::InstOpCode::LDETR, node, sreg1, sreg1, 0, false);
   generateRRFInstruction(cg, TR::InstOpCode::LDETR, node, sreg2, sreg2, 0, false);

   // Extract FPC and save in register
   TR::Register * fpcReg = cg->allocateRegister();
   generateRRInstruction(cg, TR::InstOpCode::EFPC, node, fpcReg, fpcReg);

   generateRSInstruction(cg, TR::InstOpCode::SRL, node, fpcReg, 4);
   generateRILInstruction(cg, TR::InstOpCode::NILF, node, fpcReg, 0x07);

   // set DFP rounding mode before arith op...
   TR::MemoryReference * rmMR = generateS390MemoryReference(0x07, cg);
   generateSInstruction(cg, TR::InstOpCode::SRNMT, node, rmMR);

   generateRRRInstruction(cg, TR::InstOpCode::ADTR, node, treg, sreg1, sreg2);
   generateRSInstruction(cg, TR::InstOpCode::SLL, node, fpcReg, 4);

   TR::Register *tmpReg = cg->allocateRegister();
   generateRRInstruction(cg, TR::InstOpCode::EFPC, node, tmpReg, tmpReg);

   generateRIInstruction(cg, TR::InstOpCode::NILL, node, tmpReg, 0xFF8F);

   generateRRInstruction(cg, TR::InstOpCode::OR, node, fpcReg, tmpReg);
   // reset FPC reg
   generateRRInstruction(cg, TR::InstOpCode::SFPC, node, fpcReg, fpcReg);

   // reround  to 7 digit significance before the double->float conversion
   TR::Register * precReg = cg->allocateRegister();
   generateRIInstruction(cg, TR::InstOpCode::LA, node, precReg, 0x7);

   generateRRFInstruction(cg, TR::InstOpCode::RRDTR, node, resReg, precReg, treg, USE_CURRENT_DFP_ROUNDING_MODE);

   generateRRFInstruction(cg, TR::InstOpCode::LEDTR, node, resReg, resReg, 0, false);

   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   cg->stopUsingRegister(sreg1);
   cg->stopUsingRegister(sreg2);
   cg->stopUsingRegister(fpcReg);
   cg->stopUsingRegister(tmpReg);
   cg->stopUsingRegister(treg);
   cg->stopUsingRegister(precReg);
   node->setRegister(resReg);
   return node->getRegister();
   }

TR::Register *
J9::Z::TreeEvaluator::ddaddEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register* sreg1 = cg->evaluate(node->getFirstChild());
   TR::Register* sreg2 = cg->evaluate(node->getSecondChild());
   TR::Register* treg = cg->allocateRegister(TR_FPR);

   generateRRRInstruction(cg, TR::InstOpCode::ADTR, node, treg, sreg1, sreg2);
   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   node->setRegister(treg);
   return node->getRegister();
   }

TR::Register *
J9::Z::TreeEvaluator::deaddEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register* sreg1 = cg->evaluate(node->getFirstChild());
   TR::Register* sreg2 = cg->evaluate(node->getSecondChild());
   TR::Register* treg = cg->allocateFPRegisterPair();

   generateRRRInstruction(cg, TR::InstOpCode::AXTR, node, treg, sreg1, sreg2);
   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   node->setRegister(treg);
   return node->getRegister();
   }

TR::Register *
J9::Z::TreeEvaluator::dfsubEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register* sreg1 = cg->fprClobberEvaluate(node->getFirstChild());
   TR::Register* sreg2 = cg->fprClobberEvaluate(node->getSecondChild());
   TR::Register* treg = cg->allocateRegister(TR_FPR);

   generateRRFInstruction(cg, TR::InstOpCode::LDETR, node, sreg1, sreg1, 0, false);
   generateRRFInstruction(cg, TR::InstOpCode::LDETR, node, sreg2, sreg2, 0, false);

   generateRRRInstruction(cg, TR::InstOpCode::SDTR, node, treg, sreg1, sreg2);
   generateRRFInstruction(cg, TR::InstOpCode::LEDTR, node, treg, treg, 0, false);
   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   cg->stopUsingRegister(sreg1);
   cg->stopUsingRegister(sreg2);
   node->setRegister(treg);
   return node->getRegister();
   }

TR::Register *
J9::Z::TreeEvaluator::ddsubEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register* sreg1 = cg->evaluate(node->getFirstChild());
   TR::Register* sreg2 = cg->evaluate(node->getSecondChild());
   TR::Register* treg = cg->allocateRegister(TR_FPR);

   generateRRRInstruction(cg, TR::InstOpCode::SDTR, node, treg, sreg1, sreg2);
   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   node->setRegister(treg);
   return node->getRegister();
   }

TR::Register *
J9::Z::TreeEvaluator::desubEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register* sreg1 = cg->evaluate(node->getFirstChild());
   TR::Register* sreg2 = cg->evaluate(node->getSecondChild());
   TR::Register* treg = cg->allocateFPRegisterPair();

   generateRRRInstruction(cg, TR::InstOpCode::SXTR, node, treg, sreg1, sreg2);
   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   node->setRegister(treg);
   return node->getRegister();
   }

TR::Register *
J9::Z::TreeEvaluator::dfmulEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register* sreg1 = cg->fprClobberEvaluate(node->getFirstChild());
   TR::Register* sreg2 = cg->fprClobberEvaluate(node->getSecondChild());
   TR::Register* treg = cg->allocateRegister(TR_FPR);

   generateRRFInstruction(cg, TR::InstOpCode::LDETR, node, sreg1, sreg1, 0, false);
   generateRRFInstruction(cg, TR::InstOpCode::LDETR, node, sreg2, sreg2, 0, false);

   generateRRRInstruction(cg, TR::InstOpCode::MDTR, node, treg, sreg1, sreg2);
   generateRRFInstruction(cg, TR::InstOpCode::LEDTR, node, treg, treg, 0, false);
   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   cg->stopUsingRegister(sreg1);
   cg->stopUsingRegister(sreg2);
   node->setRegister(treg);
   return node->getRegister();
   }

TR::Register *
J9::Z::TreeEvaluator::ddmulEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register* sreg1 = cg->evaluate(node->getFirstChild());
   TR::Register* sreg2 = cg->evaluate(node->getSecondChild());
   TR::Register* treg = cg->allocateRegister(TR_FPR);

   generateRRRInstruction(cg, TR::InstOpCode::MDTR, node, treg, sreg1, sreg2);
   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   node->setRegister(treg);
   return node->getRegister();
   }

TR::Register *
J9::Z::TreeEvaluator::demulEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register* sreg1 = cg->evaluate(node->getFirstChild());
   TR::Register* sreg2 = cg->evaluate(node->getSecondChild());
   TR::Register* treg = cg->allocateFPRegisterPair();

   generateRRRInstruction(cg, TR::InstOpCode::MXTR, node, treg, sreg1, sreg2);
   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   node->setRegister(treg);
   return node->getRegister();
   }

TR::Register *
J9::Z::TreeEvaluator::dfdivEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register* sreg1, *sreg2, *treg;

   // Clobber since we'll need to lengthen to perform 64-bit math
   if (node->getDataType() == TR::DecimalFloat)
      {
      sreg1 = cg->fprClobberEvaluate(node->getFirstChild());
      sreg2 = cg->fprClobberEvaluate(node->getSecondChild());
      }
   else
      {
      sreg1 = cg->evaluate(node->getFirstChild());
      sreg2 = cg->evaluate(node->getSecondChild());
      }

   if (node->getDataType() == TR::DecimalLongDouble)
      treg = cg->allocateFPRegisterPair();
   else
      treg = cg->allocateRegister(TR_FPR);

   // Lengthen to 64 bits
   if (node->getDataType() == TR::DecimalFloat)
      {
      generateRRFInstruction(cg, TR::InstOpCode::LDETR, node, sreg1, sreg1, 0, false);
      generateRRFInstruction(cg, TR::InstOpCode::LDETR, node, sreg2, sreg2, 0, false);
      }

   // RTC 91058: The COBOL runtime was previously setting the IEEE divide-by-zero mask
   // on when it was first brought up, so COBOL programs would terminate with an
   // exception on a divide by zero. In a mixed-language environment, other languages
   // might want to handle the exception instead of terminate. To accommodate this,
   // the runtime will no longer set the divide-by-zero mask. COBOL programs will then
   // need to ensure the mask is on exactly when an exception would be hit.
   //
   // So, we insert code here that compares the divisor to zero, and if it is zero, we
   // turn the mask on before executing the divide.
   TR::Compilation *comp = cg->comp();
   bool checkDivisorForZero = comp->getOption(TR_ForceIEEEDivideByZeroException);
   if (checkDivisorForZero)
      {
      TR::LabelSymbol *oolEntryPoint = generateLabelSymbol(cg);
      TR::LabelSymbol *oolReturnPoint = generateLabelSymbol(cg);
      TR::LabelSymbol * cflowRegionStart   = generateLabelSymbol(cg);
      TR::LabelSymbol * cflowRegionEnd     = generateLabelSymbol(cg);

      TR::Register *IMzMaskReg = cg->allocateRegister();

      TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 1, cg);
      deps->addPostCondition(IMzMaskReg, TR::RealRegister::AssignAny);

      // Load and test the divisor; branch to the OOL sequence if zero
      if (node->getDataType() == TR::DecimalLongDouble)
         generateRREInstruction(cg, TR::InstOpCode::LTXTR, node, sreg2, sreg2);
      else
         generateRREInstruction(cg, TR::InstOpCode::LTDTR, node, sreg2, sreg2);

      cflowRegionStart->setStartInternalControlFlow();
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cflowRegionStart, deps);

      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZ, node, oolEntryPoint);

      // Start OOL sequence
      TR_S390OutOfLineCodeSection *oolHelper = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(oolEntryPoint, oolReturnPoint, cg);
      cg->getS390OutOfLineCodeSectionList().push_front(oolHelper);
      oolHelper->swapInstructionListsWithCompilation();
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, oolEntryPoint);

      // Turn on the divide-by-zero exception flag (second bit from the left in the FPC dword)
      int32_t IMzMask = 0x40000000;
      generateRILInstruction(cg, TR::InstOpCode::LGFI, node, IMzMaskReg, IMzMask);
      generateRREInstruction(cg, TR::InstOpCode::SFPC, node, IMzMaskReg, IMzMaskReg);
      cg->stopUsingRegister(IMzMaskReg);

      // End OOL sequence
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, oolReturnPoint);
      oolHelper->swapInstructionListsWithCompilation();
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, oolReturnPoint);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cflowRegionEnd, deps);
      cflowRegionEnd->setEndInternalControlFlow();
      }

   if (node->getDataType() == TR::DecimalLongDouble)
      generateRRRInstruction(cg, TR::InstOpCode::DXTR, node, treg, sreg1, sreg2);
   else
      generateRRRInstruction(cg, TR::InstOpCode::DDTR, node, treg, sreg1, sreg2);

   // Shorten the result to 32 bits
   if (node->getDataType() == TR::DecimalFloat)
      generateRRFInstruction(cg, TR::InstOpCode::LEDTR, node, treg, treg, 0, false);

   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());

   if (node->getDataType() == TR::DecimalFloat)
      {
      cg->stopUsingRegister(sreg1);
      cg->stopUsingRegister(sreg2);
      }

   node->setRegister(treg);
   return node->getRegister();
   }

TR::Register *
J9::Z::TreeEvaluator::ifddcmpeqEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, TR::InstOpCode::COND_BE, false);
   }

TR::Register *
J9::Z::TreeEvaluator::ifddcmpneEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, TR::InstOpCode::COND_BNE, false);
   }

TR::Register *
J9::Z::TreeEvaluator::ifddcmpltEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, TR::InstOpCode::COND_BH, false);
   }

TR::Register *
J9::Z::TreeEvaluator::ifddcmpgeEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNL, TR::InstOpCode::COND_BNH, false);
   }

TR::Register *
J9::Z::TreeEvaluator::ifddcmpgtEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BH, TR::InstOpCode::COND_BL, false);
   }

TR::Register *
J9::Z::TreeEvaluator::ifddcmpleEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, TR::InstOpCode::COND_BNL, false);
   }

TR::Register *
J9::Z::TreeEvaluator::ifddcmpequEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, TR::InstOpCode::COND_BE, true);
   }

TR::Register *
J9::Z::TreeEvaluator::ifddcmpneuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, TR::InstOpCode::COND_BNE, true);
   }

TR::Register *
J9::Z::TreeEvaluator::ifddcmpltuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, TR::InstOpCode::COND_BH, true);
   }

TR::Register *
J9::Z::TreeEvaluator::ifddcmpgeuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNL, TR::InstOpCode::COND_BNH, true);
   }

TR::Register *
J9::Z::TreeEvaluator::ifddcmpgtuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BH, TR::InstOpCode::COND_BL, true);
   }

TR::Register *
J9::Z::TreeEvaluator::ifddcmpleuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBranch(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, TR::InstOpCode::COND_BNL, true);
   }

TR::Register *
J9::Z::TreeEvaluator::ddcmpeqEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, TR::InstOpCode::COND_BE, false);
   }

TR::Register *
J9::Z::TreeEvaluator::ddcmpneEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, TR::InstOpCode::COND_BNE, false);
   }

TR::Register *
J9::Z::TreeEvaluator::ddcmpltEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, TR::InstOpCode::COND_BH, false);
   }

TR::Register *
J9::Z::TreeEvaluator::ddcmpgeEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNL, TR::InstOpCode::COND_BNH, false);
   }

TR::Register *
J9::Z::TreeEvaluator::ddcmpgtEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BH, TR::InstOpCode::COND_BL, false);
   }

TR::Register *
J9::Z::TreeEvaluator::ddcmpleEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, TR::InstOpCode::COND_BNL, false);
   }

TR::Register *
J9::Z::TreeEvaluator::ddcmpequEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, TR::InstOpCode::COND_BE, true);
   }

TR::Register *
J9::Z::TreeEvaluator::ddcmpneuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, TR::InstOpCode::COND_BNE, true);
   }

TR::Register *
J9::Z::TreeEvaluator::ddcmpltuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, TR::InstOpCode::COND_BH, true);
   }

TR::Register *
J9::Z::TreeEvaluator::ddcmpgeuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNL, TR::InstOpCode::COND_BNH, true);
   }

TR::Register *
J9::Z::TreeEvaluator::ddcmpgtuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BH, TR::InstOpCode::COND_BL, true);
   }

TR::Register *
J9::Z::TreeEvaluator::ddcmpleuEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return generateS390CompareBool(node, cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, TR::InstOpCode::COND_BNL, true);
   }


/**
 * ddneg handles all decimal DFP types
 */
TR::Register *
J9::Z::TreeEvaluator::ddnegEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node *firstChild = node->getFirstChild();

   TR::Register *srcReg = cg->evaluate(firstChild);
   TR::Register *targetReg;

   if (node->getDataType()==TR::DecimalLongDouble)
      {
      if (cg->canClobberNodesRegister(firstChild))
         targetReg = srcReg;
      else
         targetReg = cg->allocateFPRegisterPair();
      generateRRInstruction(cg, TR::InstOpCode::LCDFR, node, targetReg->getHighOrder(), srcReg->getHighOrder());

      if (!cg->canClobberNodesRegister(firstChild))
         generateRRInstruction(cg, TR::InstOpCode::LDR, node, targetReg->getLowOrder(), srcReg->getLowOrder());
      }
   else
      {
      if (cg->canClobberNodesRegister(firstChild))
         targetReg = srcReg;
      else
         targetReg = cg->allocateRegister(TR_FPR);
      generateRRInstruction(cg, TR::InstOpCode::LCDFR, node, targetReg, srcReg);
      }

   node->setRegister(targetReg);
   cg->decReferenceCount(firstChild);
   return targetReg;
   }

/**
 * Handles TR::ddInsExp, TR::deInsExp
 */
TR::Register *
J9::Z::TreeEvaluator::ddInsExpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getOpCodeValue() == TR::ddInsExp || node->getOpCodeValue() == TR::deInsExp,"expecting op to be ddInsExp or deInsExp and not %d\n",node->getOpCodeValue());

   TR::Node *biasedExpNode = node->getSecondChild();
   TR::Node *srcNode = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(srcNode);

   TR::Register *targetReg = NULL;
   if(node->getDataType() == TR::DecimalLongDouble)
      targetReg = cg->allocateFPRegisterPair();
   else
      targetReg = cg->allocateRegister(TR_FPR);

   TR::RegisterDependencyConditions *deps = NULL;
   TR::Register *biasedExpReg = cg->evaluate(biasedExpNode);
   TR_ASSERT(biasedExpNode->getOpCode().getSize() == 8,"expecting a biasedExpNode of size 8 and not size %d\n",biasedExpNode->getOpCode().getSize());

   TR::Instruction *inst = generateRRFInstruction(cg, (node->getDataType() == TR::DecimalLongDouble ? TR::InstOpCode::IEXTR : TR::InstOpCode::IEDTR), node, targetReg, biasedExpReg, srcReg);
   if (deps)
      inst->setDependencyConditions(deps);

   cg->decReferenceCount(srcNode);
   cg->decReferenceCount(biasedExpNode);
   node->setRegister(targetReg);
   return targetReg;
   }

TR::Register *
J9::Z::TreeEvaluator::dffloorEvaluator(TR::Node *node, TR::CodeGenerator * cg)
   {
   TR::Register* srcReg = cg->evaluate(node->getFirstChild());
   TR::Register* trgReg = cg->allocateRegister(TR_FPR);

   generateRRFInstruction(cg, TR::InstOpCode::LDETR, node, trgReg, srcReg, 0, false);
   generateRRFInstruction(cg, TR::InstOpCode::FIDTR, node, trgReg, trgReg, (uint8_t)9, (uint8_t)0); //  mask3=9 round to 0, mask4=0
   generateRRFInstruction(cg, TR::InstOpCode::LEDTR, node, trgReg, trgReg, 0, false);

   cg->decReferenceCount(node->getFirstChild());
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *
J9::Z::TreeEvaluator::ddfloorEvaluator(TR::Node *node, TR::CodeGenerator * cg)
   {
   TR::Register* srcReg = cg->evaluate(node->getFirstChild());
   TR::Register* trgReg = cg->allocateRegister(TR_FPR);

   generateRRFInstruction(cg, TR::InstOpCode::FIDTR, node, trgReg, srcReg, (uint8_t)9, (uint8_t)0); //  mask3=9 round to 0, mask4=0

   cg->decReferenceCount(node->getFirstChild());
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *
J9::Z::TreeEvaluator::defloorEvaluator(TR::Node *node, TR::CodeGenerator * cg)
   {
   TR::Register* srcReg = cg->evaluate(node->getFirstChild());
   TR::Register * trgReg = cg->allocateFPRegisterPair();

   generateRRFInstruction(cg, TR::InstOpCode::FIXTR, node, trgReg, srcReg, (uint8_t)9, (uint8_t)0); //  mask3=9 round to 0, mask4=0

   cg->decReferenceCount(node->getFirstChild());
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *
J9::Z::TreeEvaluator::deconstEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
#ifdef SUPPORT_DFP
   TR::Register * highReg = cg->allocateRegister(TR_FPR);
   TR::Register * lowReg = cg->allocateRegister(TR_FPR);
   TR::Register * targetReg = cg->allocateFPRegisterPair(lowReg, highReg);

   long double value = node->getLongDouble();
   typedef struct
      {
      double dH;
      double dL;
      } twoDblType;
   union
      {
      twoDblType dd;
      long double ldbl;
      } udd;

   udd.ldbl = value;
   generateS390ImmOp(cg, TR::InstOpCode::LD, node, highReg, udd.dd.dH);
   generateS390ImmOp(cg, TR::InstOpCode::LD, node, lowReg, udd.dd.dL);

   node->setRegister(targetReg);
   return targetReg;
#else
   TR_ASSERT_FATAL(false, "No evaulator available for deconst if SUPPORT_DFP is not set");
   return NULL;
#endif
   }

inline TR::Register *
deloadHelper(TR::Node * node, TR::CodeGenerator * cg, TR::MemoryReference * srcMR)
   {
   TR::MemoryReference * loMR = srcMR;
   TR::MemoryReference * hiMR;
   if (loMR == NULL)
      {
      loMR = generateS390MemoryReference(node, cg);
      }
   hiMR = generateS390MemoryReference(*loMR, 8, cg);
   // FP reg pairs for long double: FPR0 & FPR2, FPR4 & FPR6, FPR1 & FPR3, FPR5 & FPR7 etc..
   TR::Register * lowReg = cg->allocateRegister(TR_FPR);
   TR::Register * highReg = cg->allocateRegister(TR_FPR);
   TR::Register * targetReg = cg->allocateFPRegisterPair(lowReg, highReg);

   generateRXInstruction(cg, TR::InstOpCode::LD, node, highReg, loMR);
   loMR->stopUsingMemRefRegister(cg);
   generateRXInstruction(cg, TR::InstOpCode::LD, node, lowReg, hiMR);
   hiMR->stopUsingMemRefRegister(cg);
   node->setRegister(targetReg);
   return targetReg;
   }

TR::Register *
J9::Z::TreeEvaluator::deloadEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return deloadHelper(node, cg, NULL);
   }

inline TR::MemoryReference *
destoreHelper(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * valueChild;
   if (node->getOpCode().isIndirect())
      {
      valueChild = node->getSecondChild();
      }
   else
      {
      valueChild = node->getFirstChild();
      }
  // source returns a reg pair, so... TODO
  TR::Register * srcReg = cg->evaluate(valueChild);

   TR::MemoryReference * loMR = generateS390MemoryReference(node, cg);
   TR::MemoryReference * hiMR = generateS390MemoryReference(*loMR, 8, cg);

   generateRXInstruction(cg, TR::InstOpCode::STD, node, srcReg->getHighOrder(), loMR);
   loMR->stopUsingMemRefRegister(cg);
   generateRXInstruction(cg, TR::InstOpCode::STD, node, srcReg->getLowOrder(), hiMR);
   hiMR->stopUsingMemRefRegister(cg);

   cg->decReferenceCount(valueChild);

   return loMR;
   }

TR::Register *
J9::Z::TreeEvaluator::destoreEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   destoreHelper(node, cg);
   return NULL;
   }

TR::Register *
J9::Z::TreeEvaluator::deRegLoadEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register * globalReg = node->getRegister();

   if (globalReg == NULL)
      {
      TR::Register * lowReg = cg->allocateRegister(TR_FPR);
      TR::Register * highReg = cg->allocateRegister(TR_FPR);
      globalReg = cg->allocateFPRegisterPair(lowReg, highReg);

      node->setRegister(globalReg);
      }
   return globalReg;
   }

TR::Register *
J9::Z::TreeEvaluator::deRegStoreEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * child = node->getFirstChild();
   TR::Register * globalReg = cg->evaluate(child);
   cg->decReferenceCount(child);
   return globalReg;
   }

/**
 * Handles DFP setNegative ops
 */
TR::Register *
J9::Z::TreeEvaluator::ddSetNegativeEvaluator(TR::Node * node, TR::CodeGenerator * cg)
    {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register * sourceReg = cg->evaluate(firstChild);
   TR::Register * targetReg = NULL;

   if (node->getDataType()==TR::DecimalLongDouble)
      {
      targetReg = cg->allocateFPRegisterPair();
      generateRRInstruction(cg, TR::InstOpCode::LNDFR, node, targetReg->getHighOrder(), sourceReg->getHighOrder());
      generateRRInstruction(cg, TR::InstOpCode::LDR, node, targetReg->getLowOrder(), sourceReg->getLowOrder());
      }
   else
      {
      targetReg = cg->allocateRegister(TR_FPR);
      generateRRInstruction(cg, TR::InstOpCode::LNDFR, node, targetReg, sourceReg);
      }

   node->setRegister(targetReg);
   cg->decReferenceCount(firstChild);
   return node->getRegister();
    }

/**
 * Handles DFP shl ops
 */
TR::Register *
J9::Z::TreeEvaluator::ddshlEvaluator(TR::Node * node, TR::CodeGenerator * cg)
    {
   TR::Node *numNode = node->getChild(0);
   TR::Node *shiftNode = node->getChild(1);

   TR::Register *numReg = cg->evaluate(numNode);
   TR::Register *targetReg;
   unsigned int shift = (int32_t)shiftNode->get64bitIntegralValue();
   cg->decReferenceCount(shiftNode);
   TR::MemoryReference *shiftRef = generateS390MemoryReference(shift, cg);

   // Float to double
    if (node->getDataType() == TR::DecimalFloat)
        generateRRFInstruction(cg, TR::InstOpCode::LDETR, node, numReg, numReg, 0, false);

   if (node->getDataType()==TR::DecimalLongDouble)
    {
    targetReg = cg->allocateFPRegisterPair();
      generateRXFInstruction(cg, TR::InstOpCode::SLXT, node, targetReg, numReg, shiftRef);
    }
    else
    {
      targetReg = cg->allocateRegister(TR_FPR);
      generateRXFInstruction(cg, TR::InstOpCode::SLDT, node, targetReg, numReg, shiftRef);
    }

    // Double to float
    if (node->getDataType() == TR::DecimalFloat)
        generateRRFInstruction(cg, TR::InstOpCode::LEDTR, node, targetReg, targetReg, 0, false);

   node->setRegister(targetReg);
   cg->decReferenceCount(numNode);
   return node->getRegister();
    }

/**
 * Handles DFP shr ops
 */
TR::Register *
J9::Z::TreeEvaluator::ddshrEvaluator(TR::Node * node, TR::CodeGenerator * cg)
    {
   TR::Node *numNode = node->getChild(0);
   TR::Node *shiftNode = node->getChild(1);

   TR::Register *numReg = cg->evaluate(numNode);
   TR::Register *targetReg;
   unsigned int shift = (int32_t)shiftNode->get64bitIntegralValue();
   cg->decReferenceCount(shiftNode);
   TR::MemoryReference *shiftRef = generateS390MemoryReference(shift, cg);

   // Float to double
    if (node->getDataType() == TR::DecimalFloat)
        generateRRFInstruction(cg, TR::InstOpCode::LDETR, node, numReg, numReg, 0, false);

   if (node->getDataType()==TR::DecimalLongDouble)
    {
    targetReg = cg->allocateFPRegisterPair();
      generateRXFInstruction(cg, TR::InstOpCode::SRXT, node, targetReg, numReg, shiftRef);
    }
   else
    {
      targetReg = cg->allocateRegister(TR_FPR);
      generateRXFInstruction(cg, TR::InstOpCode::SRDT, node, targetReg, numReg, shiftRef);
    }

    // Double to float
    if (node->getDataType() == TR::DecimalFloat)
        generateRRFInstruction(cg, TR::InstOpCode::LEDTR, node, targetReg, targetReg, 0, false);

   node->setRegister(targetReg);
   cg->decReferenceCount(numNode);
   return node->getRegister();
    }

/**
 * Handles DFP shr rounded
 */
TR::Register *
J9::Z::TreeEvaluator::ddshrRoundedEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   // This opcode is a replacement for pdshr (with round = 5); a ddshrRounded would get created by converting a pdshr to ddshrRound.
   // The assumption is thus made here that the value is represented with exponent 0. Thus, to get the correct results, we modify the
   // exponent and then use quantize to convert back to a number with exponent 0, rounding in the process.
   // eg. 1234567 (exp=0) shifted right by 2:
   // Set the exponent to -2: 1234567 (exp=-2), or 12345.67
   // Quantize to a value with exponent 0 using the value 1 (exp=0): the 6 causes a round up, so the result is 12346 (exp=0).
   // Using the rounding method "round to nearest with ties away from 0" gives the "round half away from zero" behaviour used
   // in Cobol and works with negative numbers (since the sign bit is separate and not even considered with that rounding
   // mode): -1234567 shifted right two will be -12346.
   TR::Node *numNode = node->getChild(0);
   TR::Node *shiftNode = node->getChild(1);
   TR::Node *roundQuantumNode = node->getChild(2);

   TR::Register *numReg = cg->evaluate(numNode);
   TR::Register *targetReg;

   // Get the shift amount and set the exponent: for shift right of two, the exponent will become -2
   int32_t shift = (int32_t)shiftNode->get64bitIntegralValue();
   if (node->getDataType()==TR::DecimalLongDouble)
      shift = TR_DECIMAL_LONG_DOUBLE_BIAS - shift;
   else
      shift = TR_DECIMAL_DOUBLE_BIAS - shift;

   TR::Register *biasedExpReg = cg->allocateRegister();
   generateRIInstruction(cg, TR::InstOpCode::LGHI, node, biasedExpReg, shift);

   // Float to double
   if (node->getDataType() == TR::DecimalFloat)
      generateRRFInstruction(cg, TR::InstOpCode::LDETR, node, numReg, numReg, 0, false);

   if (node->getDataType()==TR::DecimalLongDouble)
      {
      targetReg = cg->allocateFPRegisterPair();
      TR::Instruction *inst = generateRRFInstruction(cg, TR::InstOpCode::IEXTR, node, targetReg, biasedExpReg, numReg);
      }
   else
      {
      targetReg = cg->allocateRegister(TR_FPR);
      generateRRFInstruction(cg, TR::InstOpCode::IEDTR, node, targetReg, biasedExpReg, numReg);
      }

   cg->decReferenceCount(shiftNode);

    // Round, using the quantize instruction
   TR::Register *constReg = cg->evaluate(roundQuantumNode);
   if (node->getDataType() == TR::DecimalLongDouble)
      generateRRFInstruction(cg, TR::InstOpCode::QAXTR, node, targetReg, constReg, targetReg, 0xC);
   else
      generateRRFInstruction(cg, TR::InstOpCode::QADTR, node, targetReg, constReg, targetReg, 0xC);

   cg->decReferenceCount(roundQuantumNode);

   // Double to float
   if (node->getDataType() == TR::DecimalFloat)
      generateRRFInstruction(cg, TR::InstOpCode::LEDTR, node, targetReg, targetReg, 0, false);

   node->setRegister(targetReg);
   cg->stopUsingRegister(biasedExpReg);
   cg->decReferenceCount(numNode);
   return node->getRegister();
   }

/**
 * Handles DFP modify precision ops
 */
TR::Register *
J9::Z::TreeEvaluator::ddModifyPrecisionEvaluator(TR::Node * node, TR::CodeGenerator * cg)
    {
   TR::Node *numNode = node->getChild(0);
   TR::Register *numReg = cg->evaluate(numNode);
   TR::Register *targetReg;

   // To set precision to p, shift left by maximum_precision - p (shifting out all but p digits of the original), then shift right by the same amount
   unsigned int newP = node->getDFPPrecision();
   if (node->getDataType() == TR::DecimalLongDouble)
      newP = TR::DataType::getMaxExtendedDFPPrecision() - newP;
   else
      newP = TR::DataType::getMaxLongDFPPrecision() - newP;
   TR::MemoryReference *shiftRef = generateS390MemoryReference(newP, cg);

   // Float to double
    if (node->getDataType() == TR::DecimalFloat)
        generateRRFInstruction(cg, TR::InstOpCode::LDETR, node, numReg, numReg, 0, false);

   if (node->getDataType()==TR::DecimalLongDouble)
    {
    targetReg = cg->allocateFPRegisterPair();
      generateRXFInstruction(cg, TR::InstOpCode::SLXT, node, targetReg, numReg, shiftRef);
      shiftRef = generateS390MemoryReference(newP, cg);
      generateRXFInstruction(cg, TR::InstOpCode::SRXT, node, targetReg, targetReg, shiftRef);
    }
   else
    {
      targetReg = cg->allocateRegister(TR_FPR);
      generateRXFInstruction(cg, TR::InstOpCode::SLDT, node, targetReg, numReg, shiftRef);
      shiftRef = generateS390MemoryReference(newP, cg);
      generateRXFInstruction(cg, TR::InstOpCode::SRDT, node, targetReg, targetReg, shiftRef);
    }

    // Double to float
    if (node->getDataType() == TR::DecimalFloat)
        generateRRFInstruction(cg, TR::InstOpCode::LEDTR, node, targetReg, targetReg, 0, false);

   node->setRegister(targetReg);
   cg->decReferenceCount(numNode);
   return node->getRegister();
    }

TR::Register *
J9::Z::TreeEvaluator::ddcleanEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register* sreg = cg->evaluate(node->getFirstChild());
   TR::Register* zreg = cg->allocateRegister(TR_FPR);
   TR::Register* treg = cg->allocateRegister(TR_FPR);

   // the below sequence will change a -0 to +0 and leave all other values unchanged
   generateRRRInstruction(cg, TR::InstOpCode::SDTR, node, zreg, sreg, sreg);
   generateRRRInstruction(cg, TR::InstOpCode::ADTR, node, treg, sreg, zreg);

   cg->decReferenceCount(node->getFirstChild());
   cg->stopUsingRegister(zreg);
   node->setRegister(treg);
   return node->getRegister();
   }

TR::Register *
J9::Z::TreeEvaluator::decleanEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register* sreg = cg->evaluate(node->getFirstChild());
   TR::Register* zreg = cg->allocateFPRegisterPair();
   TR::Register* treg = cg->allocateFPRegisterPair();

   // the below sequence will change a -0 to +0 and leave all other values unchanged
   generateRRRInstruction(cg, TR::InstOpCode::SXTR, node, zreg, sreg, sreg);
   generateRRRInstruction(cg, TR::InstOpCode::AXTR, node, treg, sreg, zreg);

   cg->decReferenceCount(node->getFirstChild());
   cg->stopUsingRegister(zreg);
   node->setRegister(treg);
   return node->getRegister();
   }

TR::Register *
J9::Z::TreeEvaluator::dd2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * srcReg = cg->evaluate(firstChild);
   TR::Register * tempDDReg = NULL;
   TR::Register * targetReg = cg->allocateRegister();

   TR::InstOpCode::Mnemonic convertOpCode;
   switch (firstChild->getDataType())
      {
      case TR::DecimalFloat:
         convertOpCode = TR::InstOpCode::CGDTR;
         tempDDReg = cg->allocateRegister(TR_FPR);
         break;
      case TR::DecimalDouble:
         convertOpCode = TR::InstOpCode::CGDTR;
         tempDDReg = srcReg;
         break;
      case TR::DecimalLongDouble:
         convertOpCode = TR::InstOpCode::CGXTR;
         tempDDReg = srcReg;
         break;
      default:
         TR_ASSERT( 0, "dfpToLong: unsupported opcode\n");
         break;
      }

   // Float to double
    if (firstChild->getDataType() == TR::DecimalFloat)
        generateRRFInstruction(cg, TR::InstOpCode::LDETR, node, tempDDReg, srcReg, 0, false);

   generateRRFInstruction(cg, convertOpCode, node, targetReg, tempDDReg, 0x9, true);

   cg->decReferenceCount(firstChild);
   if (firstChild->getDataType() == TR::DecimalFloat)
     cg->stopUsingRegister(tempDDReg);

   node->setRegister(targetReg);
   return targetReg;
   }

