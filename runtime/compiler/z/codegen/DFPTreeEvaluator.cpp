/*******************************************************************************
 * Copyright (c) 2000, 2021 IBM Corp. and others
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
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
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
int16_t dfpFieldOffset = -1; // initialize to an illegal val

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
const char * className = "Ljava/math/BigDecimal;";
const int32_t len = 22;


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
      if (cg->comp()->target().cpu.supportsFeature(OMR_FEATURE_S390_FPE))
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
      dfpMR->setSymbolReference(comp->getSymRefTab()->
                                   findOrCreateGenericLongShadowSymbolReference(
                                   dfpFieldOffset, node->getSymbolReference()->getSymbol()->getResolvedMethodSymbol()));
   */
   }
