/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
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
#include "codegen/S390CHelperLinkage.hpp"
#include "codegen/S390PrivateLinkage.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/VirtualGuard.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/DataTypes.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Bit.hpp"
#include "OMR/Bytes.hpp"
#include "ras/Delimiter.hpp"
#include "ras/DebugCounter.hpp"
#include "env/VMJ9.h"
#include "z/codegen/J9S390Snippet.hpp"
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

void
J9::Z::TreeEvaluator::inlineEncodeASCII(TR::Node *node, TR::CodeGenerator *cg)
   {
   // tree looks as follows:
   // encodeASCIISymbol
   //    input ptr
   //    output ptr
   //    input length (in elements)
   //
   // The original Java loop that this IL is inlining is found in StringCoding.encodeASCII:
   /* if (coder == LATIN1) {
            byte[] dst = new byte[val.length];
            for (int i = 0; i < val.length; i++) {
                if (val[i] < 0) {
                    dst[i] = '?';
                } else {
                    dst[i] = val[i];
                }
            }
            return dst;
        }
   */
   // Get the children
   TR::Register *inputPtrReg = cg->gprClobberEvaluate(node->getChild(0));
   TR::Register *outputPtrReg = cg->gprClobberEvaluate(node->getChild(1));
   TR::Register *inputLengthRegister = cg->evaluate(node->getChild(2));

   TR::LabelSymbol *processMultiple16CharsStart = generateLabelSymbol(cg);
   TR::LabelSymbol *processMultiple16CharsEnd = generateLabelSymbol(cg);

   TR::LabelSymbol *processSaturatedInput1 = generateLabelSymbol(cg);

   TR::LabelSymbol *cFlowRegionEnd = generateLabelSymbol(cg);

   TR::Register *vInput1 = cg->allocateRegister(TR_VRF);
   TR::Register *vRange = cg->allocateRegister(TR_VRF);
   TR::Register *vRangeControl = cg->allocateRegister(TR_VRF);
   TR::Register *numCharsLeftToProcess = cg->allocateRegister();
   TR::Register *firstSaturatedCharacter = cg->allocateRegister(TR_VRF);

   uint32_t saturatedRange = 127;
   uint8_t saturatedRangeControl = 0x20; // > comparison

   // Replicate the limit character and comparison controller into vector registers
   generateVRIaInstruction(cg, TR::InstOpCode::VREPI, node, vRange, saturatedRange, 0);
   generateVRIaInstruction(cg, TR::InstOpCode::VREPI, node, vRangeControl, saturatedRangeControl, 0);

   // Copy length into numCharsLeftToProcess
   generateRRInstruction(cg, TR::InstOpCode::LR, node, numCharsLeftToProcess, inputLengthRegister);

   // Branch to the end of this section if there are less than 16 chars left to process
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::C, node, numCharsLeftToProcess, 16, TR::InstOpCode::COND_BL, processMultiple16CharsEnd, false, false);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, processMultiple16CharsStart);
   processMultiple16CharsStart->setStartInternalControlFlow();

   generateVRXInstruction(cg, TR::InstOpCode::VL, node, vInput1, generateS390MemoryReference(inputPtrReg, 0, cg));
   // Check for vector saturation and branch to copy the unsaturated bytes
   // VSTRC here will do an unsigned comparison and set the CC if any byte in the input vector is above 127.
   // If all numbers are below 128, then we can do a straight copy of the 16 bytes. If not, then we branch to
   // processSaturatedInput1 label that corrects the first 'bad' character and stores all characters up to and including the 'bad' character
   // in the output destination. Then we branch back to this mainline loop and continue processing the rest of the array.
   // The penalty for encountering 1 or more bad characters in a row can be big, but we bet that such cases are not
   // common.
   generateVRRdInstruction(cg, TR::InstOpCode::VSTRC, node, firstSaturatedCharacter, vInput1, vRange, vRangeControl, 0x1, 0);
   // If atleast one bad character was found, CC=1. So branch to handle this case.
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, node, processSaturatedInput1);

   // If we didn't take the branch above, then all 16 bytes can be copied directly over.
   generateVRXInstruction(cg, TR::InstOpCode::VST, node, vInput1, generateS390MemoryReference(outputPtrReg, 0, cg));

   // Update the counters
   generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, outputPtrReg, generateS390MemoryReference(outputPtrReg, 16, cg));
   generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, inputPtrReg, generateS390MemoryReference(inputPtrReg, 16, cg));
   generateRILInstruction(cg, TR::InstOpCode::SLFI, node, numCharsLeftToProcess, 16);

   // Branch back up if we still have more than 16 characters to process.
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::C, node, numCharsLeftToProcess, 15, TR::InstOpCode::COND_BH, processMultiple16CharsStart, false, false);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, processMultiple16CharsEnd);

   // start of sequence to process under 16 characters

   // numCharsLeftToProcess holds length of final load.
   // Branch to the end if there is no residue
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::C, node, numCharsLeftToProcess, 0, TR::InstOpCode::COND_BE, cFlowRegionEnd, false, false);

   // Zero out the input register to avoid invalid VSTRC result
   generateVRIaInstruction(cg, TR::InstOpCode::VGBM, node, vInput1, 0, 0 /*unused*/);

   // VLL and VSTL work on indices so we must subtract 1
   TR::Register *numCharsLeftToProcessMinus1 = cg->allocateRegister();
   // Due to the check above, the value in numCharsLeftToProcessMinus1 is guaranteed to be 0 or higher.
   generateRIEInstruction(cg, TR::InstOpCode::AHIK, node, numCharsLeftToProcessMinus1, numCharsLeftToProcess, -1);
   // Load residue bytes and check for saturation
   generateVRSbInstruction(cg, TR::InstOpCode::VLL, node, vInput1, numCharsLeftToProcessMinus1, generateS390MemoryReference(inputPtrReg, 0, cg));

   // Check for vector saturation and branch to copy the unsaturated bytes
   generateVRRdInstruction(cg, TR::InstOpCode::VSTRC, node, firstSaturatedCharacter, vInput1, vRange, vRangeControl, 0x1, 0);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, node, processSaturatedInput1);

   // If no bad characters found, the store with length.
   generateVRSbInstruction(cg, TR::InstOpCode::VSTL, node, vInput1, numCharsLeftToProcessMinus1, generateS390MemoryReference(outputPtrReg, 0, cg), 0);
   // Branch to end.
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cFlowRegionEnd);

   // Encountered an out of range character via the VSTRC instruction. Find it, replace it with '?', then jump back to mainline
   // to continue processing. This sequence is not the most efficient and hitting it one or more times can be expensive,
   // but we bet that this won't happen often for the targeted workload.
   // Algorithm works as follows:
   // First store upto and not including the bad character.
   // Then store '?' in place for the bad character.
   // Then, update the counters with the number of characters we have processed.
   // Then go back to mainline code. Where we jump to depends on how many characters are left to process.
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, processSaturatedInput1);

   TR::Register *firstSaturatedCharacterGR = cg->allocateRegister();
   // Extract the index of the first saturated char in the 2nd vector register
   generateVRScInstruction(cg, TR::InstOpCode::VLGV, node, firstSaturatedCharacterGR, firstSaturatedCharacter, generateS390MemoryReference(7, cg), 0);

   // Needed as VSTL operate on 0-based index.
   TR::Register *firstSaturatedCharacterMinus1GR = cg->allocateRegister();

   generateRIEInstruction(cg, TR::InstOpCode::AHIK, node, firstSaturatedCharacterMinus1GR, firstSaturatedCharacterGR, -1);

   // If the result is less than 0, then it means the first character is saturated. So skip storing any good characters and jump to fixing the bad
   // character.
   TR::LabelSymbol *fixReplacementCharacter = generateLabelSymbol(cg);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, node, fixReplacementCharacter);

   // Copy only the unsaturated results using the index we calculated earlier
   generateVRSbInstruction(cg, TR::InstOpCode::VSTL, node, vInput1, firstSaturatedCharacterMinus1GR, generateS390MemoryReference(outputPtrReg, 0, cg), 0);
   generateRRInstruction(cg, cg->comp()->target().is64Bit() ? TR::InstOpCode::AGFR : TR::InstOpCode::AR, node, outputPtrReg, firstSaturatedCharacterGR);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, fixReplacementCharacter);
   const uint32_t replacementCharacter = 63;
   generateSIInstruction(cg, TR::InstOpCode::MVI, node, generateS390MemoryReference(outputPtrReg, 0, cg), replacementCharacter);

   generateRILInstruction(cg, cg->comp()->target().is64Bit() ? TR::InstOpCode::AGFI : TR::InstOpCode::AFI, node, outputPtrReg, 1);

   // Now update the counters
   generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, inputPtrReg, generateS390MemoryReference(inputPtrReg, firstSaturatedCharacterGR, 1, cg));
   generateRILInstruction(cg, TR::InstOpCode::AFI, node, numCharsLeftToProcess, -1);
   generateRRInstruction(cg, TR::InstOpCode::SR, node, numCharsLeftToProcess, firstSaturatedCharacterGR);

   // Counters have been updated. Now branch back to mainline. Where we branch depends on how many chars are left.
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::C, node, numCharsLeftToProcess, 15, TR::InstOpCode::COND_BH, processMultiple16CharsStart, false, false);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, processMultiple16CharsEnd);

   TR::RegisterDependencyConditions* dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 11, cg);
   dependencies->addPostConditionIfNotAlreadyInserted(vInput1, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(firstSaturatedCharacter, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(vRange, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(vRangeControl, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(outputPtrReg, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(inputPtrReg, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(numCharsLeftToProcess, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(numCharsLeftToProcessMinus1, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(inputLengthRegister, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(firstSaturatedCharacterGR, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(firstSaturatedCharacterMinus1GR, TR::RealRegister::AssignAny);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionEnd, dependencies);
   cFlowRegionEnd->setEndInternalControlFlow();

   cg->decReferenceCount(node->getChild(0));
   cg->decReferenceCount(node->getChild(1));
   cg->decReferenceCount(node->getChild(2));

   cg->stopUsingRegister(vInput1);
   cg->stopUsingRegister(firstSaturatedCharacter);
   cg->stopUsingRegister(vRange);
   cg->stopUsingRegister(vRangeControl);
   cg->stopUsingRegister(numCharsLeftToProcess);
   cg->stopUsingRegister(numCharsLeftToProcessMinus1);
   cg->stopUsingRegister(firstSaturatedCharacterGR);
   cg->stopUsingRegister(firstSaturatedCharacterMinus1GR);
   }

TR::Register*
J9::Z::TreeEvaluator::inlineStringLatin1Inflate(TR::Node *node, TR::CodeGenerator *cg)
   {
   static bool disableStringInflate = feGetEnv("TR_DisableStringInflate") != NULL;
   if (disableStringInflate)
      {
      return NULL;
      }
   TR_ASSERT_FATAL(cg->getSupportsInlineStringLatin1Inflate(), "This evaluator should only be triggered when inlining StringLatin1.inflate([BI[CII)V is enabled on Java 11 onwards!\n");
   TR::Node *sourceArrayReferenceNode = node->getChild(0);
   TR::Node *srcOffNode = node->getChild(1);
   TR::Node *charArrayReferenceNode = node->getChild(2);
   TR::Node *dstOffNode = node->getChild(3);
   TR::Node *lenNode = node->getChild(4);

   TR::Register *lenRegister = cg->evaluate(lenNode);
   TR::Register *sourceArrayReferenceRegister = cg->gprClobberEvaluate(sourceArrayReferenceNode);
   TR::Register *srcOffRegister = cg->gprClobberEvaluate(srcOffNode);
   TR::Register *charArrayReferenceRegister = cg->gprClobberEvaluate(charArrayReferenceNode);
   TR::Register *dstOffRegister = cg->gprClobberEvaluate(dstOffNode);

   // Adjust the array reference (source and destination) with offset in advance
   if (srcOffNode->getOpCodeValue() == TR::iconst)
      {
      if (srcOffNode->getInt() != 0)
         {
         generateRILInstruction(cg, TR::InstOpCode::AFI, node, sourceArrayReferenceRegister, srcOffNode->getInt());
         }
      }
   else
      {
      generateRRInstruction(cg, TR::InstOpCode::AGFR, node, sourceArrayReferenceRegister, srcOffRegister);
      }

   if (dstOffNode->getOpCodeValue() == TR::iconst)
      {
      if (dstOffNode->getInt() != 0)
         {
         generateRILInstruction(cg, TR::InstOpCode::AFI, node, charArrayReferenceRegister, dstOffNode->getInt() * 2);
         }
      }
   else
      {
      generateRSInstruction(cg, TR::InstOpCode::SLAK, node, dstOffRegister, dstOffRegister, 1);
      generateRRInstruction(cg, TR::InstOpCode::AGFR, node, charArrayReferenceRegister, dstOffRegister);
      }

   // The vector tight loop (starting at vectorLoopStart label) overwrites sourceArrayReferenceRegister as it processes characters. So we keep a backup of the
   // copy here so that we can refer to it when handling the residual digits after the tight loop is finished executing.
   TR::Register *sourceArrayReferenceRegister2 = cg->allocateRegister();
   generateRRInstruction(cg, TR::InstOpCode::LGR, node, sourceArrayReferenceRegister2, sourceArrayReferenceRegister);

   // charArrayReference is the destination array. Since the vector loop below processes 16 bytes into 16 chars per iteration, we will store 32 bytes per iteration.
   // We use the `VST` instruction twice to store 16 bytes at a time. Hence, we need a "low" and "high" memref for the char array in order to store all 32 bytes per iteration
   // of the vector loop.
   TR::MemoryReference *charArrayReferenceMemRefLow = generateS390MemoryReference(charArrayReferenceRegister, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg);
   TR::MemoryReference *charArrayReferenceMemRefHigh = generateS390MemoryReference(charArrayReferenceRegister, TR::Compiler->om.contiguousArrayHeaderSizeInBytes() + 16, cg);

   // numCharsMinusResidue is used as a scratch register to hold temporary values throughout the algorithm.
   TR::Register *numCharsMinusResidue = cg->allocateRegister();
   generateRRInstruction(cg, TR::InstOpCode::LR, node, numCharsMinusResidue, lenRegister);
   generateRILInstruction(cg, TR::InstOpCode::SLFI, node, numCharsMinusResidue, 16);
   generateRRInstruction(cg, TR::InstOpCode::AR, node, numCharsMinusResidue, srcOffRegister);

   TR::LabelSymbol *cFlowRegionStart = generateLabelSymbol(cg);
   cFlowRegionStart->setStartInternalControlFlow();
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionStart);

   TR::LabelSymbol *cFlowRegionEnd = generateLabelSymbol(cg);
   TR::LabelSymbol *gprSequenceLabel = generateLabelSymbol(cg);
   cFlowRegionEnd->setEndInternalControlFlow();
   // Before starting the tight loop, check if length is 0. If so, then jump to end as there's no work to be done.
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::C, node, lenRegister, 0, TR::InstOpCode::COND_BE, cFlowRegionEnd, false, false);
   // Also check if length < 8. If yes, then jump to gprSequenceLabel to handle this case with regular GPRs for speed.
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::C, node, lenRegister, 8, TR::InstOpCode::COND_BL, gprSequenceLabel, false, false);

   TR::LabelSymbol *vectorLoopStart = generateLabelSymbol(cg);
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, vectorLoopStart);
   TR::LabelSymbol *handleResidueLabel = generateLabelSymbol(cg);

   // We keep executing the vector tight loop below until only the residual characters remain to process.
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CR, node, srcOffRegister, numCharsMinusResidue, TR::InstOpCode::COND_BH, handleResidueLabel, false, false);
   TR::Register* registerV1 = cg->allocateRegister(TR_VRF);
   TR::MemoryReference *sourceArrayMemRef = generateS390MemoryReference(sourceArrayReferenceRegister, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg);
   // Do a vector load to batch process the characters.
   generateVRXInstruction(cg, TR::InstOpCode::VL, node, registerV1, sourceArrayMemRef);
   TR::Register* registerV2 = cg->allocateRegister(TR_VRF);
   // Unpack the 1st and 2nd halves of the input vector. This will efectively inflate each character from 1 byte to 2 bytes.
   generateVRRaInstruction(cg, TR::InstOpCode::VUPLH, node, registerV2, registerV1);
   generateVRRaInstruction(cg, TR::InstOpCode::VUPLL, node, registerV1, registerV1);

   // Store all characters.
   generateVRXInstruction(cg, TR::InstOpCode::VST, node, registerV2, charArrayReferenceMemRefLow);
   generateVRXInstruction(cg, TR::InstOpCode::VST, node, registerV1, charArrayReferenceMemRefHigh);

   // Done storing 16 chars. Now do some bookkeeping and then branch back to start label.
   generateRILInstruction(cg, TR::InstOpCode::AFI, node, srcOffRegister, 16);
   generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, sourceArrayReferenceRegister, generateS390MemoryReference(sourceArrayReferenceRegister, 16, cg));
   generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, charArrayReferenceRegister, generateS390MemoryReference(charArrayReferenceRegister, 32, cg));
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, vectorLoopStart);

   // Once we reach this label, only the residual characters need to be processed.
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, handleResidueLabel);

   TR::MemoryReference *sourceArrayMemRef2 = generateS390MemoryReference(sourceArrayReferenceRegister2, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg);
   TR::MemoryReference *charArrayReferenceMemRefLow2 = generateS390MemoryReference(charArrayReferenceRegister, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg);
   TR::MemoryReference *charArrayReferenceMemRefHigh2 = generateS390MemoryReference(charArrayReferenceRegister, TR::Compiler->om.contiguousArrayHeaderSizeInBytes() + 16, cg);

   TR::Register *quoRegister = cg->allocateRegister();
   // Do lenRegister / 16 to calculate remaining number of chars using the Divide Logical (DLR) instruction.
   // The dividend in a DLR instruction is a 64-bit integer. The top half is in remRegister, and the bottom half is in quoRegister.
   // In our case the dividend is always a 32-bit integer (i.e. the length of the array). So we must always zero out the top half (i.e. remRegister) in order to make sure the dividend is never corrupted.
   // The bottom half doesn't need to be zeroed out because we move a 32-bit integer in it, and then never use that register again.
   TR::Register *remRegister = cg->allocateRegister();
   generateRRInstruction(cg, TR::InstOpCode::XGR, node, remRegister, remRegister);
   TR::RegisterPair *divRegisterPair = cg->allocateConsecutiveRegisterPair(quoRegister, remRegister); // rem is legal even of the pair
   generateRRInstruction(cg, TR::InstOpCode::LR, node, quoRegister, lenRegister);
   TR::Register *tempReg = cg->allocateRegister();
   generateRILInstruction(cg, TR::InstOpCode::LGFI, node, /*divisor*/ tempReg, 16);
   generateRRInstruction(cg, TR::InstOpCode::DLR, node, divRegisterPair, tempReg/*divisor*/);

   // Branch to end if length was a multiple of 16. (We would have processed this in the vectorloop already).
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::C, node, remRegister, 0, TR::InstOpCode::COND_BE, cFlowRegionEnd, false, false);
   // Now do srcOffRegister = lenRegister - remRegister to position index at the next character that we need to copy.
   generateRRRInstruction(cg, TR::InstOpCode::SRK, node, srcOffRegister, lenRegister, remRegister);
   // Now add that result to the base register
   generateRRInstruction(cg, TR::InstOpCode::AGFR, node, sourceArrayReferenceRegister2, srcOffRegister);

   // Now that you have the new index and the number of remaining characters, load those many chars into registerV1. We are guaranteed to have numChars < 16
   //
   // First load remainder (i.e. number of remaining chars) value into remRegister2 (Which is just a 0-index based version of remRegister).
   // Then we subtract remRegister2 by 1 to get an indexed number. Then we use VLL to load the remaining bytes int registerV1.
   TR::Register *remRegister2 = cg->allocateRegister();
   generateRRInstruction(cg, TR::InstOpCode::LR, node, remRegister2, remRegister);
   generateRILInstruction(cg, TR::InstOpCode::SLFI, node, remRegister2, 1);
   generateVRSbInstruction(cg, TR::InstOpCode::VLL, node, registerV1, remRegister2, sourceArrayMemRef2);
   // Now unpack the low order. If we have less than 8 chars to process, there will be zeros in the register
   generateVRRaInstruction(cg, TR::InstOpCode::VUPLH, node, registerV2, registerV1);
   // Multiply numChars remaining by 2 to get the number of bytes we need to write
   generateRSInstruction(cg, TR::InstOpCode::SLAK, node, tempReg, remRegister, 1);
   // Subtract one from tempReg since the byte position is 0 based.
   generateRILInstruction(cg, TR::InstOpCode::SLFI, node, tempReg, 1);
   generateVRSbInstruction(cg, TR::InstOpCode::VSTL, node, registerV2, tempReg, charArrayReferenceMemRefLow2, 0);
   // Now subtract numChars by 8 and see if there's still more bytes left to write
   generateRILInstruction(cg, TR::InstOpCode::SLFI, node, remRegister, 8);
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::C, node, remRegister, 0, TR::InstOpCode::COND_BNH, cFlowRegionEnd, false, false);
   // If we didn't branch, then there are still more characters to process, and the remaining amount is in remRegister.
   // So unpack the characters and store back in memory
   generateVRRaInstruction(cg, TR::InstOpCode::VUPLL, node, registerV1, registerV1);
   generateRSInstruction(cg, TR::InstOpCode::SLAK, node, tempReg, remRegister, 1);
   generateRILInstruction(cg, TR::InstOpCode::SLFI, node, tempReg, 1);
   generateVRSbInstruction(cg, TR::InstOpCode::VSTL, node, registerV1, tempReg, charArrayReferenceMemRefHigh2, 0);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cFlowRegionEnd);

   // We should only end up here if we initially detect that the input string's length < 8.
   // For the GPR sequence we simply load one byte at a time using LLC, then store it as a char.
   // If we are here, then lenRegister is less than remaining chars is in numCharsMinusResidue.
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, gprSequenceLabel);

   // Repurpose numCharsMinusResidue here as a temp/scratch reg.
   generateRSInstruction(cg, TR::InstOpCode::SLAK, node, numCharsMinusResidue, lenRegister, 1);
   generateRSInstruction(cg, TR::InstOpCode::SLAK, node, tempReg, lenRegister, 3);
   generateRRInstruction(cg, TR::InstOpCode::AR, node, numCharsMinusResidue, tempReg);

   // First we figure out exactly how many chars are left.
   generateRILInstruction(cg, TR::InstOpCode::LARL, node, tempReg, cFlowRegionEnd);

   generateRRInstruction(cg, TR::InstOpCode::SR, node, tempReg, numCharsMinusResidue);
   TR::Instruction *cursor = generateS390RegInstruction(cg, TR::InstOpCode::BCR, node, tempReg);
   ((TR::S390RegInstruction *)cursor)->setBranchCondition(TR::InstOpCode::COND_BCR);

   // 7 chars left
   generateRXInstruction(cg, TR::InstOpCode::LLC, node, tempReg, generateS390MemoryReference(sourceArrayReferenceRegister, TR::Compiler->om.contiguousArrayHeaderSizeInBytes() + 6, cg));
   generateRXInstruction(cg, TR::InstOpCode::STH, node, tempReg, generateS390MemoryReference(charArrayReferenceRegister, TR::Compiler->om.contiguousArrayHeaderSizeInBytes() + 12, cg));
   // 6 chars left
   generateRXInstruction(cg, TR::InstOpCode::LLC, node, tempReg, generateS390MemoryReference(sourceArrayReferenceRegister, TR::Compiler->om.contiguousArrayHeaderSizeInBytes() + 5, cg));
   generateRXInstruction(cg, TR::InstOpCode::STH, node, tempReg, generateS390MemoryReference(charArrayReferenceRegister, TR::Compiler->om.contiguousArrayHeaderSizeInBytes() + 10, cg));
   // 5 chars left
   generateRXInstruction(cg, TR::InstOpCode::LLC, node, tempReg, generateS390MemoryReference(sourceArrayReferenceRegister, TR::Compiler->om.contiguousArrayHeaderSizeInBytes() + 4, cg));
   generateRXInstruction(cg, TR::InstOpCode::STH, node, tempReg, generateS390MemoryReference(charArrayReferenceRegister, TR::Compiler->om.contiguousArrayHeaderSizeInBytes() + 8, cg));
   // 4 chars left
   generateRXInstruction(cg, TR::InstOpCode::LLC, node, tempReg, generateS390MemoryReference(sourceArrayReferenceRegister, TR::Compiler->om.contiguousArrayHeaderSizeInBytes() + 3, cg));
   generateRXInstruction(cg, TR::InstOpCode::STH, node, tempReg, generateS390MemoryReference(charArrayReferenceRegister, TR::Compiler->om.contiguousArrayHeaderSizeInBytes() + 6, cg));
   // 3 chars left
   generateRXInstruction(cg, TR::InstOpCode::LLC, node, tempReg, generateS390MemoryReference(sourceArrayReferenceRegister, TR::Compiler->om.contiguousArrayHeaderSizeInBytes() + 2, cg));
   generateRXInstruction(cg, TR::InstOpCode::STH, node, tempReg, generateS390MemoryReference(charArrayReferenceRegister, TR::Compiler->om.contiguousArrayHeaderSizeInBytes() + 4, cg));
   // 2 chars left
   generateRXInstruction(cg, TR::InstOpCode::LLC, node, tempReg, generateS390MemoryReference(sourceArrayReferenceRegister, TR::Compiler->om.contiguousArrayHeaderSizeInBytes() + 1, cg));
   generateRXInstruction(cg, TR::InstOpCode::STH, node, tempReg, generateS390MemoryReference(charArrayReferenceRegister, TR::Compiler->om.contiguousArrayHeaderSizeInBytes() + 2, cg));
   // 1 chars left
   generateRXInstruction(cg, TR::InstOpCode::LLC, node, tempReg, generateS390MemoryReference(sourceArrayReferenceRegister, TR::Compiler->om.contiguousArrayHeaderSizeInBytes() + 0, cg));
   generateRXInstruction(cg, TR::InstOpCode::STH, node, tempReg, generateS390MemoryReference(charArrayReferenceRegister, TR::Compiler->om.contiguousArrayHeaderSizeInBytes() + 0, cg));

   TR::RegisterDependencyConditions* dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 14, cg);
   dependencies->addPostConditionIfNotAlreadyInserted(sourceArrayReferenceRegister, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(lenRegister, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(srcOffRegister, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(dstOffRegister, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(charArrayReferenceRegister, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(numCharsMinusResidue, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(registerV1, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(registerV2, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(divRegisterPair, TR::RealRegister::EvenOddPair);
   dependencies->addPostConditionIfNotAlreadyInserted(remRegister, TR::RealRegister::LegalEvenOfPair);
   dependencies->addPostConditionIfNotAlreadyInserted(quoRegister, TR::RealRegister::LegalOddOfPair);
   dependencies->addPostConditionIfNotAlreadyInserted(tempReg, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(sourceArrayReferenceRegister2, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(remRegister2, TR::RealRegister::AssignAny);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionEnd, dependencies);

   cg->decReferenceCount(srcOffNode);
   cg->decReferenceCount(dstOffNode);
   cg->decReferenceCount(lenNode);
   cg->decReferenceCount(sourceArrayReferenceNode);
   cg->decReferenceCount(charArrayReferenceNode);

   cg->stopUsingRegister(remRegister2);
   cg->stopUsingRegister(numCharsMinusResidue);
   cg->stopUsingRegister(registerV1);
   cg->stopUsingRegister(registerV2);
   cg->stopUsingRegister(divRegisterPair);
   cg->stopUsingRegister(tempReg);
   cg->stopUsingRegister(sourceArrayReferenceRegister2);
   return charArrayReferenceRegister;
   }

TR::Register*
J9::Z::TreeEvaluator::zdloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdloadEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::zdloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdloadEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::zdstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdstoreEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::zdstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdstoreEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::zdsleLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdloadEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::zdslsLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdloadEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::zdstsLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdloadEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::zdsleLoadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdloadEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::zdslsLoadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdloadEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::zdstsLoadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdloadEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::zdsleStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdstoreEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::zdslsStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdstoreEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::zdstsStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdstoreEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::zdsleStoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdstoreEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::zdslsStoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdstoreEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::zdstsStoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdstoreEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::zd2zdsleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::zdsle2zdEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::zd2zdstsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::zd2zdslsEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::zdsle2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::zdsts2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::zdsls2pdEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::zdsts2zdEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::zdsls2zdEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::pd2zdslsSetSignEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pd2zdslsEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::pd2zdstsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pd2zdslsEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::pd2zdstsSetSignEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pd2zdslsEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::udLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdloadEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::udslLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdloadEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::udstLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdloadEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::udLoadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdloadEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::udslLoadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdloadEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::udstLoadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdloadEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::udStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdstoreEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::udslStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdstoreEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::udstStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdstoreEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::udStoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdstoreEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::udslStoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdstoreEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::udstStoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdstoreEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::pd2udstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pd2udslEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::udsl2udEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::udst2udEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::udst2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::udsl2pdEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::pdloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdloadEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::pdstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdstoreEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::pddivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pddivremEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::pdremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pddivremEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::pdabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::pdshrSetSignEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::pdshlSetSignEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::pdshlOverflowEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdshlEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::pd2iOverflowEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pd2iEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::pd2iuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pd2iEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::iu2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::pd2lOverflowEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pd2lEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::pd2luEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pd2lEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::lu2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::pd2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::pd2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::f2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::d2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::pdcleanEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::unImpOpEvaluator(node, cg);
   }

TR::Register*
J9::Z::TreeEvaluator::pdclearSetSignEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::pdclearEvaluator(node, cg);
   }

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

TR::Register*
J9::Z::TreeEvaluator::inlineVectorizedStringIndexOf(TR::Node* node, TR::CodeGenerator* cg, bool isUTF16)
   {
   #define iComment(str) if (compDebug) compDebug->addInstructionComment(cursor, (const_cast<char*>(str)));
   TR::Compilation *comp = cg->comp();
   const uint32_t elementSizeMask = isUTF16 ? 1 : 0;
   const int8_t vectorSize = cg->machine()->getVRFSize();
   const uintptr_t headerSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
   const bool supportsVSTRS = comp->target().cpu.supportsFeature(OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_2);
   TR_Debug *compDebug = comp->getDebug();
   TR::Instruction* cursor;

   static bool disableIndexOfStringIntrinsic = feGetEnv("TR_DisableIndexOfStringIntrinsic") != NULL;
   if (disableIndexOfStringIntrinsic)
      return NULL;

   if (comp->getOption(TR_TraceCG))
      traceMsg(comp, "inlineVectorizedStringIndexOf. Is isUTF16 %d\n", isUTF16);

   // This evaluator function handles different indexOf() intrinsics, some of which are static calls without a
   // receiver. Hence, the need for static call check.
   const bool isStaticCall = node->getSymbolReference()->getSymbol()->castToMethodSymbol()->isStatic();
   const uint8_t firstCallArgIdx = isStaticCall ? 0 : 1;

   TR_S390ScratchRegisterManager *srm = cg->generateScratchRegisterManager(9);

   // Get call parameters where stringValue and patternValue are byte arrays
   TR::Register* stringValueReg = cg->evaluate(node->getChild(firstCallArgIdx));
   TR::Register* stringLenReg = cg->gprClobberEvaluate(node->getChild(firstCallArgIdx+1));
   TR::Register* patternValueReg = cg->evaluate(node->getChild(firstCallArgIdx+2));
   TR::Register* patternLenReg = cg->gprClobberEvaluate(node->getChild(firstCallArgIdx+3));
   TR::Register* stringIndexReg = cg->gprClobberEvaluate(node->getChild(firstCallArgIdx+4));

   // Registers
   TR::Register* matchIndexReg    = cg->allocateRegister();
   TR::Register* maxIndexReg      = srm->findOrCreateScratchRegister();
   TR::Register* patternIndexReg  = srm->findOrCreateScratchRegister();
   TR::Register* loadLenReg       = srm->findOrCreateScratchRegister();
   TR::Register* stringVReg       = srm->findOrCreateScratchRegister(TR_VRF);
   TR::Register* patternVReg      = srm->findOrCreateScratchRegister(TR_VRF);
   TR::Register* searchResultVReg = srm->findOrCreateScratchRegister(TR_VRF);

   // Register dependencies
   TR::RegisterDependencyConditions* regDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, supportsVSTRS ? 16 : 13, cg);

   regDeps->addPostCondition(stringValueReg, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(stringLenReg, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(patternValueReg, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(patternLenReg, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(stringIndexReg, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(matchIndexReg, TR::RealRegister::AssignAny);

   // Labels
   TR::LabelSymbol* labelStart = generateLabelSymbol(cg);
   TR::LabelSymbol* labelFindPatternHead = generateLabelSymbol(cg);
   TR::LabelSymbol* labelLoadString16Bytes = generateLabelSymbol(cg);
   TR::LabelSymbol* labelLoadStringLenDone = generateLabelSymbol(cg);
   TR::LabelSymbol* labelMatchPatternLoop = generateLabelSymbol(cg);
   TR::LabelSymbol* labelMatchPatternResidue = generateLabelSymbol(cg);
   TR::LabelSymbol* labelMatchPatternLoopSetup = generateLabelSymbol(cg);
   TR::LabelSymbol* labelPartialPatternMatch  = generateLabelSymbol(cg);
   TR::LabelSymbol* labelLoadResult = generateLabelSymbol(cg);
   TR::LabelSymbol* labelResultDone = generateLabelSymbol(cg);
   TR::LabelSymbol* labelPatternNotFound = generateLabelSymbol(cg);
   TR::LabelSymbol* labelDone = generateLabelSymbol(cg);

   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelStart);
   iComment("retrieve string len, pattern len and starting pos");
   labelStart->setStartInternalControlFlow();

   // Decompressed strings have [byte_length = char_length * 2]
   if (isUTF16 && comp->target().is64Bit())
      {
      generateShiftThenKeepSelected64Bit(node, cg, stringLenReg, stringLenReg, 31, 62, 1);
      generateShiftThenKeepSelected64Bit(node, cg, patternLenReg, patternLenReg, 31, 62, 1);
      generateShiftThenKeepSelected64Bit(node, cg, stringIndexReg, stringIndexReg, 31, 62, 1);
      }
   else
      {
      generateRRInstruction(cg, TR::InstOpCode::LGFR, node, stringLenReg, stringLenReg);
      generateRRInstruction(cg, TR::InstOpCode::LGFR, node, patternLenReg, patternLenReg);
      generateRRInstruction(cg, TR::InstOpCode::LGFR, node, stringIndexReg, stringIndexReg);

      if (isUTF16)
         {
         generateRSInstruction(cg, TR::InstOpCode::SLL, node, stringLenReg, 1);
         generateRSInstruction(cg, TR::InstOpCode::SLL, node, patternLenReg, 1);
         generateRSInstruction(cg, TR::InstOpCode::SLL, node, stringIndexReg, 1);
         }
      }

   cursor = generateRRRInstruction(cg, TR::InstOpCode::getSubtractThreeRegOpCode(), node, maxIndexReg, stringLenReg, patternLenReg);
   iComment("maximum valid index for a potential match");
   generateRIEInstruction(cg, TR::InstOpCode::getCmpRegAndBranchRelOpCode(), node, maxIndexReg, stringIndexReg, labelPatternNotFound, TR::InstOpCode::COND_BLR);

   // patternLen debug counters
   static bool enableIndexOfDebugCounter = feGetEnv("TR_EnableIndexOfDebugCounter") != NULL;
   if (enableIndexOfDebugCounter)
      {
      TR::LabelSymbol* labelPatternLenGT10 = generateLabelSymbol(cg);
      TR::LabelSymbol* labelPatternLenGT30 = generateLabelSymbol(cg);
      TR::LabelSymbol* labelPatternLenGT60 = generateLabelSymbol(cg);
      TR::LabelSymbol* labelPatternLenGT100 = generateLabelSymbol(cg);
      TR::LabelSymbol* labelPatternLenCheckDone  = generateLabelSymbol(cg);

      uint8_t boundary10Char = isUTF16 ? 20 : 10;
      uint8_t boundary30Char = isUTF16 ? 60 : 30;
      uint8_t boundary60Char = isUTF16 ? 120 : 60;
      uint8_t boundary100Char = isUTF16 ? 200 : 100;

      generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, patternLenReg, boundary10Char, labelPatternLenGT10, TR::InstOpCode::COND_BH);
      cg->generateDebugCounter("indexOfString/PatternLen/below-10", 1, TR::DebugCounter::Cheap);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelPatternLenCheckDone);

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelPatternLenGT10);
      generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, patternLenReg, boundary30Char, labelPatternLenGT30, TR::InstOpCode::COND_BH);
      cg->generateDebugCounter("indexOfString/PatternLen/10-30", 1, TR::DebugCounter::Cheap);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelPatternLenCheckDone);

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelPatternLenGT30);
      generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, patternLenReg, boundary60Char, labelPatternLenGT60, TR::InstOpCode::COND_BH);
      cg->generateDebugCounter("indexOfString/PatternLen/30-60", 1, TR::DebugCounter::Cheap);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelPatternLenCheckDone);

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelPatternLenGT60);
      generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, patternLenReg, boundary100Char, labelPatternLenGT100, TR::InstOpCode::COND_BH);
      cg->generateDebugCounter("indexOfString/PatternLen/60-100", 1, TR::DebugCounter::Cheap);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelPatternLenCheckDone);

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelPatternLenGT100);
      cg->generateDebugCounter("indexOfString/PatternLen/above-100", 1, TR::DebugCounter::Cheap);

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelPatternLenCheckDone);
      }

   if (supportsVSTRS)
      {
      TR::Register* patternHeadVReg = srm->findOrCreateScratchRegister(TR_VRF); // used for first 16 bytes of the pattern
      TR::Register* patternLenVReg = srm->findOrCreateScratchRegister(TR_VRF); // length of the pattern being searched for through VSTRS instruction

      // Load the first piece of patternValue (pattern header) which is either 16 bytes or patternLen
      TR::LabelSymbol* labelPatternLoad16Bytes = generateLabelSymbol(cg);
      TR::LabelSymbol* labelPatternLoadDone = generateLabelSymbol(cg);

      generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, patternLenReg, (int8_t)vectorSize, labelPatternLoad16Bytes, TR::InstOpCode::COND_BNL);
      generateRIEInstruction(cg, TR::InstOpCode::getAddHalfWordImmDistinctOperandOpCode(), node, loadLenReg, patternLenReg, -1);
      generateVRSbInstruction(cg, TR::InstOpCode::VLL, node, patternHeadVReg, loadLenReg, generateS390MemoryReference(patternValueReg, headerSize, cg));
      generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, loadLenReg, patternLenReg);
      generateVRSbInstruction(cg, TR::InstOpCode::VLVG, node, patternLenVReg, patternLenReg, generateS390MemoryReference(7, cg), 0);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelPatternLoadDone);

      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelPatternLoad16Bytes);
      iComment("load first 16 bytes of the pattern");
      generateVRXInstruction(cg, TR::InstOpCode::VL, node, patternHeadVReg, generateS390MemoryReference(patternValueReg, headerSize, cg));
      generateRIInstruction(cg, TR::InstOpCode::LHI, node, loadLenReg, vectorSize);
      generateVRSbInstruction(cg, TR::InstOpCode::VLVG, node, patternLenVReg, loadLenReg, generateS390MemoryReference(7, cg), 0);
      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelPatternLoadDone);
      iComment("min(16,pattern length) bytes have been loaded");

      // Loop to search for pattern header in string
      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelFindPatternHead);
      iComment("look for pattern head in the string");

      // Determine string load length and load a piece of string
      generateRRRInstruction(cg, TR::InstOpCode::getSubtractThreeRegOpCode(), node, loadLenReg, stringLenReg, stringIndexReg);
      generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, loadLenReg, (int8_t)vectorSize, labelLoadString16Bytes, TR::InstOpCode::COND_BNL);
      TR::Register* stringCharPtrReg = srm->findOrCreateScratchRegister();
      generateRRRInstruction(cg, TR::InstOpCode::getAddThreeRegOpCode(), node, stringCharPtrReg, stringValueReg, stringIndexReg);
      // Needs -1 because VLL's third operand is the highest index to load.
      // e.g. If the load length is 8 bytes, the highest index is 7. Hence, the need for -1.
      cursor = generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, loadLenReg, -1);
      iComment("needs -1 because VLL's third operand is the highest index to load");
      generateVRSbInstruction(cg, TR::InstOpCode::VLL, node, stringVReg, loadLenReg, generateS390MemoryReference(stringCharPtrReg, headerSize, cg));
      generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, loadLenReg, 1);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelLoadStringLenDone);
      srm->reclaimScratchRegister(stringCharPtrReg);
      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelLoadString16Bytes);
      iComment("load 16 bytes of the string");
      generateVRXInstruction(cg, TR::InstOpCode::VL, node, stringVReg, generateS390MemoryReference(stringValueReg, stringIndexReg, headerSize, cg));
      generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, loadLenReg, vectorSize);
      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelLoadStringLenDone);
      iComment("bytes of the string have been loaded");

      // VSTRS sets CC with the following values:
      // CC = 0, no match or partial match, AND (zs = 0 OR no zero byte in source VRF)
      // CC = 1, no match AND (zs = 1) AND (zero byte in source VRF)
      // CC = 2, full match
      // CC = 3, partial match but no full match.
      TR::LabelSymbol* labelPatternHeadFullMatch = generateLabelSymbol(cg);
      TR::LabelSymbol* labelPatternHeadPartMatch = generateLabelSymbol(cg);

      generateVRRdInstruction(cg, TR::InstOpCode::VSTRS, node, searchResultVReg, stringVReg, patternHeadVReg, patternLenVReg, 0, elementSizeMask);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC2, node, labelPatternHeadFullMatch);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC3, node, labelPatternHeadPartMatch);

      // pattern header not found in first 16 bytes of the string
      // Load the next 16 bytes of the string and continue
      generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, stringIndexReg, loadLenReg);
      cursor = generateRIEInstruction(cg, TR::InstOpCode::getCmpRegAndBranchRelOpCode(), node, stringIndexReg, maxIndexReg, labelPatternNotFound, TR::InstOpCode::COND_BH);
      iComment("Updated stringIndex for next iteration exceeds maxIndex of valid match. Full pattern cannot be matched.")
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelFindPatternHead);
      iComment("neither full nor partial match was found for pattern head, load next 16 bytes of the string and try again");

      // pattern header full match
      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelPatternHeadFullMatch);
      iComment("full match found of pattern head");

      // If patternLen <= 16 then we are done, otherwise we continue to check the rest of pattern. We first handle residue bytes
      // of pattern, then handle the rest 16-byte chunks.
      cursor = generateVRScInstruction(cg, TR::InstOpCode::VLGV, node, matchIndexReg, searchResultVReg, generateS390MemoryReference(7, cg), 0);
      iComment("check 7th index of search result vec for byte index");
      generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, matchIndexReg, stringIndexReg);

      // An edge case failure can occur when the string we would like to search in is smaller than 16 characters and -XX:+CompactStrings is enabled.
      // The scenario can be illustrated by the example below:
      //    If V2 = "teststring", then vector representation of V2 = {116, 101, 115, 116, 83, 116, 114, 105, 110, 103, 0, 0, 0, 0, 0, 0}.
      //    If V3 = "\0", then vector representation of V3 = {0}.
      //    Therefore, V4 = {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0}. (i.e. substring length is 1).

      // In the above example, V1 would have the result 10 (i.e. pointing to the first occurrence of 0 in V2). This is obviously incorrect as an index
      // value of 10 is out of range in the string we are searching.
      // Therefore, we conditionally jump to labelPatternNotFound using the instruction below if matchIndexReg > maxIndexReg.
      cursor = generateRIEInstruction(cg, TR::InstOpCode::getCmpRegAndBranchRelOpCode(), node, matchIndexReg, maxIndexReg, labelPatternNotFound, TR::InstOpCode::COND_BH);
      iComment("Jump if pattern match found beyond end of string (i.e. we matched 0s in unused vector register slots).");
      cursor = generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, patternLenReg, (int8_t)vectorSize, labelLoadResult, TR::InstOpCode::COND_BNH);
      iComment("if patternLen <= 16 then we are done, otherwise we continue to check the rest of pattern");
      cursor = generateRIEInstruction(cg, TR::InstOpCode::getCmpRegAndBranchRelOpCode(), node, stringIndexReg, maxIndexReg, labelPatternNotFound, TR::InstOpCode::COND_BH);
      iComment("Updated stringIndex for start of matched section exceeds maxIndex. Full pattern cannot be matched.");
      generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, stringIndexReg, loadLenReg);
      generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, patternIndexReg, vectorSize);
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelMatchPatternResidue);
      iComment("find residual pattern");

      // pattern header partial match
      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelPatternHeadPartMatch);
      iComment("partial match of first 16 bytes of pattern was found");

      // Starting from the beginning of the partial match, load the next 16 bytes from string and redo pattern header search.
      // This implies that the partial match will be re-matched by the next VSTRS. This can potentially benefit string
      // search cases where pattern is shorter than 16 bytes. For short string strings, string search can potentially be done in
      // the next VSTRS and we can avoid residue matching which requires several index adjustments that do not provide
      // performance benefits.
      cursor = generateVRScInstruction(cg, TR::InstOpCode::VLGV, node, matchIndexReg, searchResultVReg, generateS390MemoryReference(7, cg), 0);
      iComment("check 7th index of search result vec for byte index");
      generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, stringIndexReg, matchIndexReg);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelFindPatternHead);

      srm->reclaimScratchRegister(patternLenVReg);
      srm->reclaimScratchRegister(patternHeadVReg);
      }
   else
      {
      TR::Register* patternFirstCharVReg  = srm->findOrCreateScratchRegister(TR_VRF);

      /************************************** 1st char of pattern ******************************************/
      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelFindPatternHead);
      iComment("find first character of pattern");
      generateVRXInstruction(cg, TR::InstOpCode::VLREP, node, patternFirstCharVReg, generateS390MemoryReference(patternValueReg, headerSize, cg), elementSizeMask);

      // Determine string load length. loadLenReg is either vectorSize-1 (15) or the 1st_char_matching residue length.
      generateRIEInstruction(cg, TR::InstOpCode::getAddHalfWordImmDistinctOperandOpCode(), node, loadLenReg, stringIndexReg, vectorSize);
      generateRIEInstruction(cg, TR::InstOpCode::getCmpRegAndBranchRelOpCode(), node, loadLenReg, stringLenReg, labelLoadString16Bytes, TR::InstOpCode::COND_BNHR);
      generateRRRInstruction(cg, TR::InstOpCode::getSubtractThreeRegOpCode(), node, loadLenReg, stringLenReg, stringIndexReg);
      generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, loadLenReg, -1);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelLoadStringLenDone);

      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelLoadString16Bytes);
      iComment("update loadLenReg to load 16 characters from the string later on");
      generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, loadLenReg, vectorSize-1);

      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelLoadStringLenDone);
      iComment("load 16 characters into string VRF register and search for first chracter of the pattern");

      TR::Register* stringCharPtrReg = srm->findOrCreateScratchRegister();
      TR::LabelSymbol* labelExtractFirstCharPos = generateLabelSymbol(cg);
      generateRRRInstruction(cg, TR::InstOpCode::getAddThreeRegOpCode(), node, stringCharPtrReg, stringValueReg, stringIndexReg);
      generateVRSbInstruction(cg, TR::InstOpCode::VLL, node, stringVReg, loadLenReg, generateS390MemoryReference(stringCharPtrReg, headerSize, cg));
      generateVRRbInstruction(cg, TR::InstOpCode::VFEE, node, searchResultVReg, stringVReg, patternFirstCharVReg, 0x1, elementSizeMask);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, node, labelExtractFirstCharPos);
      srm->reclaimScratchRegister(stringCharPtrReg);

      // 1st char not found. Loop back and retry from the next chunk
      generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, stringIndexReg, loadLenReg);
      generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, stringIndexReg, 1);
      generateRIEInstruction(cg, TR::InstOpCode::getCmpRegAndBranchRelOpCode(), node, stringIndexReg, maxIndexReg, labelPatternNotFound, TR::InstOpCode::COND_BHR);
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelFindPatternHead);
      iComment("1st char not found. Loop back and retry from the next chunk");

      // Found 1st char. check it's byte index in searchResultVReg byte 7.
      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelExtractFirstCharPos);
      iComment("check 7th index of search result vec for byte index");

      generateVRScInstruction(cg, TR::InstOpCode::VLGV, node, matchIndexReg, searchResultVReg, generateS390MemoryReference(7, cg), 0);
      generateRIEInstruction(cg, TR::InstOpCode::getCmpRegAndBranchRelOpCode(), node, matchIndexReg, loadLenReg, labelPatternNotFound, TR::InstOpCode::COND_BHR);

      generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, matchIndexReg, stringIndexReg); // convert relative index to absolute index
      generateRIEInstruction(cg, TR::InstOpCode::getCmpRegAndBranchRelOpCode(), node, matchIndexReg, maxIndexReg, labelPatternNotFound, TR::InstOpCode::COND_BHR);

      /************************************** s2 Residue matching ******************************************/
      generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, stringIndexReg, matchIndexReg); // use the absolute match index as starting index when matching rest of the pattern
      srm->reclaimScratchRegister(patternFirstCharVReg);
      }

   srm->addScratchRegistersToDependencyList(regDeps);
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelMatchPatternResidue);
   iComment("match remainder of the pattern");

   // pattern residue length  = patternLenReg mod 16
   generateRRInstruction(cg, TR::InstOpCode::LLGHR, node, loadLenReg, patternLenReg);
   generateRIInstruction(cg, TR::InstOpCode::NILL, node, loadLenReg, 0x000F);
   generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, loadLenReg, (int8_t)0, labelMatchPatternLoopSetup, TR::InstOpCode::COND_BE);

   TR::Register* stringCharPtrReg = srm->findOrCreateScratchRegister();
   generateRRRInstruction(cg, TR::InstOpCode::getAddThreeRegOpCode(), node, stringCharPtrReg, stringValueReg, stringIndexReg);

   // Vector loads use load index. And [load_index = load_len - 1]
   generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, loadLenReg, -1);
   generateVRSbInstruction(cg, TR::InstOpCode::VLL, node, stringVReg, loadLenReg, generateS390MemoryReference(stringCharPtrReg, headerSize, cg));
   srm->reclaimScratchRegister(stringCharPtrReg);
   // If VSTRS is supported, the first VSTRS already handled the 1st 16 bytes at this point (full match in the 1st 16
   // bytes). Hence, residue offset starts at 16.
   uint32_t patternResidueDisp = headerSize + (supportsVSTRS ? vectorSize : 0);

   generateVRSbInstruction(cg, TR::InstOpCode::VLL, node, patternVReg, loadLenReg, generateS390MemoryReference(patternValueReg, patternResidueDisp, cg));
   generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, loadLenReg, 1);

   if (supportsVSTRS)
      {
      generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, patternIndexReg, vectorSize);
      }

   generateVRRbInstruction(cg, TR::InstOpCode::VCEQ, node, searchResultVReg, stringVReg, patternVReg, 1, elementSizeMask);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC0, node, labelMatchPatternLoopSetup);

   // The residue does not match. Continue to find the 1st char in string, starting from the next element.
   generateRIEInstruction(cg, TR::InstOpCode::getAddHalfWordImmDistinctOperandOpCode(), node, stringIndexReg, matchIndexReg, isUTF16 ? 2 : 1);
   generateRIEInstruction(cg, TR::InstOpCode::getCmpRegAndBranchRelOpCode(), node, stringIndexReg, maxIndexReg, labelPatternNotFound, TR::InstOpCode::COND_BHR);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelFindPatternHead);

   /************************************** pattern matching loop ENTRY ******************************************/

   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelMatchPatternLoopSetup);
   iComment("loop setup to search for rest of the pattern");
   generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, stringIndexReg, loadLenReg);

   if (supportsVSTRS)
      {
      generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, patternIndexReg, loadLenReg);
      }
   else
      {
      generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, patternIndexReg, loadLenReg);
      }

   srm->reclaimScratchRegister(loadLenReg);
   TR::Register* loopCountReg = srm->findOrCreateScratchRegister();
   generateRSInstruction(cg, TR::InstOpCode::SRLG, node, loopCountReg, patternLenReg, 4);

   if (supportsVSTRS)
      {
      generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, loopCountReg, -1);
      }

   generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, loopCountReg, static_cast<int8_t>(0), labelLoadResult, TR::InstOpCode::COND_BE);

   /************************************** pattern matching loop ******************************************/
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelMatchPatternLoop);
   iComment("start search for reset of the pattern");
   // Start to match the reset of pattern
   generateVRXInstruction(cg, TR::InstOpCode::VL, node, stringVReg, generateS390MemoryReference(stringValueReg, stringIndexReg, headerSize, cg));
   generateVRXInstruction(cg, TR::InstOpCode::VL, node, patternVReg, generateS390MemoryReference(patternValueReg, patternIndexReg, headerSize, cg));

   generateVRRbInstruction(cg, TR::InstOpCode::VCEQ, node, searchResultVReg, stringVReg, patternVReg, 1, elementSizeMask);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC0, node, labelPartialPatternMatch);

   // pattern chunk does not match. Go back and search again
   generateRIEInstruction(cg, TR::InstOpCode::getAddHalfWordImmDistinctOperandOpCode(), node, stringIndexReg, matchIndexReg, isUTF16 ? 2 : 1);
   generateRIEInstruction(cg, TR::InstOpCode::getCmpRegAndBranchRelOpCode(), node, stringIndexReg, maxIndexReg, labelPatternNotFound, TR::InstOpCode::COND_BHR);
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelFindPatternHead);
   iComment("pattern chunk does not match. Go back and search again");

   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelPartialPatternMatch);
   iComment("there was a complete match for the characters currently loaded in pattern VRF register");
   generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, stringIndexReg, vectorSize);
   generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, patternIndexReg, vectorSize);
   generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, loopCountReg, -1);
   generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, loopCountReg, (int8_t)0, labelMatchPatternLoop, TR::InstOpCode::COND_BNE);
   srm->reclaimScratchRegister(loopCountReg);
   // Load -1 if pattern is no found in string or load the character index of the 1st character of pattern in string
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelLoadResult);

   if (isUTF16)
      {
      // Byte-index to char-index conversion
      cursor = generateRSInstruction(cg, TR::InstOpCode::SRA, node, matchIndexReg, 1);
      iComment("byte-index to char-index conversion");
      }
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelResultDone);

   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelPatternNotFound);
   iComment("pattern was not found in the string");
   generateRIInstruction(cg, TR::InstOpCode::LHI, node, matchIndexReg, -1);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelResultDone);

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

      generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, patternLenReg, boundary10Char, labelResultGT10, TR::InstOpCode::COND_BH);
      cg->generateDebugCounter("indexOfString/result/below-10", 1, TR::DebugCounter::Cheap);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelResultCheckDone);

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node,  labelResultGT10);
      generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, patternLenReg, boundary30Char, labelResultGT30, TR::InstOpCode::COND_BH);
      cg->generateDebugCounter("indexOfString/result/10-30", 1, TR::DebugCounter::Cheap);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelResultCheckDone);

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node,  labelResultGT30);
      generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, patternLenReg, boundary60Char, labelResultGT60, TR::InstOpCode::COND_BH);
      cg->generateDebugCounter("indexOfString/result/30-60", 1, TR::DebugCounter::Cheap);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelResultCheckDone);

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node,   labelResultGT60);
      generateRIEInstruction(cg, TR::InstOpCode::getCmpImmBranchRelOpCode(), node, patternLenReg, boundary100Char, labelResultGT100, TR::InstOpCode::COND_BH);
      cg->generateDebugCounter("indexOfString/result/60-100", 1, TR::DebugCounter::Cheap);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelResultCheckDone);

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelResultGT100);
      cg->generateDebugCounter("indexOfString/result/above-100", 1, TR::DebugCounter::Cheap);

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelResultCheckDone);
      }

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelDone, regDeps);
   labelDone->setEndInternalControlFlow();

   node->setRegister(matchIndexReg);

   for (int32_t i = 0; i < node->getNumChildren(); ++i)
      {
      cg->decReferenceCount(node->getChild(i));
      }
   cg->stopUsingRegister(stringIndexReg);
   srm->stopUsingRegisters();

   return matchIndexReg;
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
   const int32_t sizeOfVector = cg->machine()->getVRFSize();
   const bool is64 = cg->comp()->target().is64Bit();
   uintptr_t headerSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();

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
   // Characters (0xE0-0xF6) when to upper and (0xC0-0xD6) when to lower
   generateVRIaInstruction (cg, TR::InstOpCode::VLEIH, node, alphaRangeVector, isToUpper ? 0xE0F6 : 0xC0D6, 0x1);
   // Characters (0xF8-0xFE) when to upper and (0xD8-0xDE) when to lower
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
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionStart);
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

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, fullVectorConversion);

   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::C, node,
                                           lengthRegister, sizeOfVector,
                                           TR::InstOpCode::COND_BL, success, false);

   // Set the loopCounter to the amount of groups of 16 bytes left, ignoring already accounted for remainder
   generateRSInstruction(cg, TR::InstOpCode::SRL, node, loopCounter, loopCounter, 4);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, loop);

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

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, success);

   generateRIInstruction(cg, TR::InstOpCode::LHI, node, lengthRegister, 1);

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cFlowRegionEnd);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, handleInvalidChars);
   cg->generateDebugCounter(isToUpper? "z13/simd/toUpper/null"  : "z13/simd/toLower/null", 1, TR::DebugCounter::Cheap);
   generateRRInstruction(cg, TR::InstOpCode::XR, node, lengthRegister, lengthRegister);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionEnd, regDeps);
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

TR::Register *
J9::Z::TreeEvaluator::inlineIntrinsicIndexOf(TR::Node * node, TR::CodeGenerator * cg, bool isLatin1)
   {
   cg->generateDebugCounter("z13/simd/indexOf", 1, TR::DebugCounter::Free);

   TR::Register* array = cg->evaluate(node->getChild(1));
   TR::Register* ch = cg->evaluate(node->getChild(2));
   TR::Register* offset = cg->evaluate(node->getChild(3));
   TR::Register* length = cg->gprClobberEvaluate(node->getChild(4));


   const int32_t sizeOfVector = cg->machine()->getVRFSize();

   // load length isn't used after loop, size must is adjusted to become bytes left
   TR::Register* loopCounter = length;
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

   TR::RegisterDependencyConditions* regDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 8, cg);
   regDeps->addPostCondition(array, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(loopCounter, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(indexRegister, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(loadLength, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(offsetAddress, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(charBufferVector, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(resultVector, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(valueVector, TR::RealRegister::AssignAny);

   generateVRRfInstruction(cg, TR::InstOpCode::VLVGP, node, valueVector, offset, ch);

   // Byte or halfword mask
   const int elementSizeMask = isLatin1 ? 0x0 : 0x1;
   generateVRIcInstruction(cg, TR::InstOpCode::VREP, node, valueVector, valueVector, (cg->machine()->getVRFSize() / (1 << elementSizeMask)) - 1, elementSizeMask);
   generateVRIaInstruction(cg, TR::InstOpCode::VGBM, node, resultVector, 0, 0);
   generateVRIaInstruction(cg, TR::InstOpCode::VGBM, node, charBufferVector, 0, 0);

   if (cg->comp()->target().is64Bit())
      {
      generateRREInstruction(cg, TR::InstOpCode::LLGFR, node, indexRegister, offset);
      }
   else
      {
      generateRRInstruction(cg, TR::InstOpCode::LR, node, indexRegister, offset);
      }
   generateRRInstruction(cg, TR::InstOpCode::SR, node, length, offset);

   if (!isLatin1)
      {
      generateRSInstruction(cg, TR::InstOpCode::SLL, node, length, 1);
      generateRSInstruction(cg, TR::InstOpCode::SLL, node, indexRegister, 1);
      }

   generateRRInstruction(cg, TR::InstOpCode::LR, node, loadLength, length);
   generateRILInstruction(cg, TR::InstOpCode::NILF, node, loadLength, 0xF);
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionStart);
   cFlowRegionStart->setStartInternalControlFlow();
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZ, node, fullVectorLabel);

   // VLL takes an index, not a count, so subtract 1 from the count
   generateRILInstruction(cg, TR::InstOpCode::SLFI, node, loadLength, 1);

   generateRXInstruction(cg, TR::InstOpCode::LA, node, offsetAddress, generateS390MemoryReference(array, indexRegister, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg));
   generateVRSbInstruction(cg, TR::InstOpCode::VLL, node, charBufferVector, loadLength, generateS390MemoryReference(offsetAddress, 0, cg));

   generateVRRbInstruction(cg, TR::InstOpCode::VFEE, node, resultVector, charBufferVector, valueVector, 0x1, elementSizeMask);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK1, node, notFoundInResidue);

   generateVRScInstruction(cg, TR::InstOpCode::VLGV, node, scratch, resultVector, generateS390MemoryReference(7, cg), 0);
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CR, node,
                                           scratch, loadLength,
                                           TR::InstOpCode::COND_BNH, foundLabelExtractedScratch);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, notFoundInResidue);

   // Increment index by loaded length + 1, since we subtracted 1 earlier
   generateRIEInstruction(cg, TR::InstOpCode::AHIK, node, loadLength, loadLength, 1);
   generateRRInstruction(cg, TR::InstOpCode::AR, node, indexRegister, loadLength);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, fullVectorLabel);

   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::C, node,
                                           length, sizeOfVector,
                                           TR::InstOpCode::COND_BL, failureLabel);

   // Set loopcounter to 1/16 of the length, remainder has already been accounted for
   generateRSInstruction(cg, TR::InstOpCode::SRL, node, loopCounter, loopCounter, 4);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, loopLabel);

   generateVRXInstruction(cg, TR::InstOpCode::VL, node, charBufferVector, generateS390MemoryReference(array, indexRegister, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg));

   generateVRRbInstruction(cg, TR::InstOpCode::VFEE, node, resultVector, charBufferVector, valueVector, 0x1, elementSizeMask);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK4, node, foundLabel);

   generateRILInstruction(cg, TR::InstOpCode::AFI, node, indexRegister, cg->machine()->getVRFSize());

   generateS390BranchInstruction(cg, TR::InstOpCode::BRCT, node, loopCounter, loopLabel);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, failureLabel);
   generateRIInstruction(cg, TR::InstOpCode::LHI, node, indexRegister, 0xFFFF);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_B, node, cFlowRegionEnd);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, foundLabel);
   generateVRScInstruction(cg, TR::InstOpCode::VLGV, node, scratch, resultVector, generateS390MemoryReference(7, cg), 0);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, foundLabelExtractedScratch);
   generateRRInstruction(cg, TR::InstOpCode::AR, node, indexRegister, scratch);

   if (!isLatin1)
      {
      generateRSInstruction(cg, TR::InstOpCode::SRL, node, indexRegister, indexRegister, 1);
      }

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionEnd, regDeps);
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

TR::Register*
J9::Z::TreeEvaluator::inlineUTF16BEEncode(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation* comp = cg->comp();

   // Create the necessary registers
   TR::Register* output    = cg->gprClobberEvaluate(node->getChild(1));
   TR::Register* input     = cg->gprClobberEvaluate(node->getChild(0));

   TR::Register* inputLen  = cg->gprClobberEvaluate(node->getChild(2));
   TR::Register* inputLen8 = cg->allocateRegister();

   TR::Register* temp1 = cg->allocateRegister();
   TR::Register* temp2 = cg->allocateRegister();

   // Number of bytes currently translated (also used as a stride register)
   TR::Register* translated = cg->allocateRegister();

   // Convert input length in number of characters to number of bytes
   generateRSInstruction(cg, TR::InstOpCode::getShiftLeftLogicalSingleOpCode(), node, inputLen, inputLen, 1);

   // Calculate inputLen8 = inputLen / 8
   generateRSInstruction(cg, TR::InstOpCode::SRLK, node, inputLen8, inputLen, 3);

   // Initialize the number of translated bytes to 0
   generateRREInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, translated, translated);

   // Create the necessary labels
   TR::LabelSymbol * processChar4     = generateLabelSymbol( cg);
   TR::LabelSymbol * processChar4End  = generateLabelSymbol( cg);
   TR::LabelSymbol * processChar1     = generateLabelSymbol( cg);
   TR::LabelSymbol * processChar1End  = generateLabelSymbol( cg);
   TR::LabelSymbol * processChar1Copy = generateLabelSymbol( cg);

   const uint16_t surrogateRange1 = 0xD800;
   const uint16_t surrogateRange2 = 0xDFFF;

   const uint32_t surrogateMaskAND = 0xF800F800;
   const uint32_t surrogateMaskXOR = 0xD800D800;

   TR::RegisterDependencyConditions* dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 7, cg);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, processChar4);
   processChar4->setStartInternalControlFlow();

   // Branch to the end if there are no more multiples of 4 chars left to process
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, inputLen8, 0, TR::InstOpCode::COND_MASK8, processChar4End, false, false, NULL, dependencies);

   // Load 4 input characters from memory and make a copy
   generateRXInstruction(cg, TR::InstOpCode::LG,  node, temp1, generateS390MemoryReference(input, translated, 0, cg));
   generateRREInstruction(cg, TR::InstOpCode::LGR, node, temp2, temp1);

   // AND temp2 by the surrogate mask
   generateRILInstruction(cg, TR::InstOpCode::NIHF, node, temp2, surrogateMaskAND);
   generateRILInstruction(cg, TR::InstOpCode::NILF, node, temp2, surrogateMaskAND);

   // XOR temp2 by the surrogate mask and branch if CC = 1 (meaning there is a surrogate)
   generateRILInstruction(cg, TR::InstOpCode::XIHF, node, temp2, surrogateMaskXOR);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, node, processChar4End);
   generateRILInstruction(cg, TR::InstOpCode::XILF, node, temp2, surrogateMaskXOR);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, node, processChar4End);

   generateRXInstruction(cg, TR::InstOpCode::STG, node, temp1, generateS390MemoryReference(output, translated, 0, cg));

   // Advance the number of bytes processed
   generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, translated, 8);

   // Branch back to the start of the loop
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, node, processChar4);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, processChar4End);
   processChar4End->setEndInternalControlFlow();
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, processChar1);
   processChar1->setStartInternalControlFlow();

   // Branch to the end if there are no more characters left to process
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, translated, inputLen, TR::InstOpCode::COND_BNL, processChar1End, false, false);

   // Load an input character from memory
   generateRXInstruction(cg, TR::InstOpCode::LLH, node, temp1, generateS390MemoryReference(input, translated, 0, cg));

   // Compare the input character against the lower bound surrogate character range
   generateRILInstruction(cg, TR::InstOpCode::getCmpImmOpCode(), node, temp1, surrogateRange1);

   // Branch if < (non-surrogate char)
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK4, node, processChar1Copy);

   // Compare the input character against the upper bound surrogate character range
   generateRILInstruction(cg, TR::InstOpCode::getCmpImmOpCode(), node, temp1, surrogateRange2);

   // Branch if > (non-surrogate char)
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK2, node, processChar1Copy);

   // If we get here it must be a surrogate char
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, node, processChar1End);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, processChar1Copy);

   // Store the lower byte of the character into the output buffer
   generateRXInstruction (cg, TR::InstOpCode::STH, node, temp1, generateS390MemoryReference(output, translated, 0, cg));

   // Advance the number of bytes processed
   generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, translated, 2);

   // Branch back to the start of the loop
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, node, processChar1);

   // Set up the proper register dependencies
   dependencies->addPostCondition(input,      TR::RealRegister::AssignAny);
   dependencies->addPostCondition(inputLen,   TR::RealRegister::AssignAny);
   dependencies->addPostCondition(inputLen8,  TR::RealRegister::AssignAny);
   dependencies->addPostCondition(temp1,      TR::RealRegister::AssignAny);
   dependencies->addPostCondition(temp2,      TR::RealRegister::AssignAny);
   dependencies->addPostCondition(output,     TR::RealRegister::AssignAny);
   dependencies->addPostCondition(translated, TR::RealRegister::AssignAny);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, processChar1End, dependencies);
   processChar1End->setEndInternalControlFlow();

   // Convert translated length in number of bytes to number of characters
   generateRSInstruction(cg, TR::InstOpCode::getShiftRightLogicalSingleOpCode(), node, translated, translated, 1);

   // Cleanup nodes before returning
   cg->decReferenceCount(node->getChild(0));
   cg->decReferenceCount(node->getChild(1));
   cg->decReferenceCount(node->getChild(2));

   // Cleanup registers before returning
   cg->stopUsingRegister(input);
   cg->stopUsingRegister(inputLen);
   cg->stopUsingRegister(inputLen8);
   cg->stopUsingRegister(temp1);
   cg->stopUsingRegister(temp2);
   cg->stopUsingRegister(output);

   return node->setRegister(translated);
   }

/**
 * \brief Generate inline assembly for CRC32C.updateBytes and CRC32C.updateDirectByteBuffer
 * \details
 * CRC32C.updateBytes(crc, array, offset, end)
 *    buffer = array + offset
 *    remaining = offset - end
 *    if remaining < 16 goto callJava
 *    load vector folding constants into vConstR2R1, vConstR4R3, vConstR5, vConstRUPoly, vConstCRCPoly
 *    load crc into vScratch
 *    if remaining < 64 goto foldBy1
 *
 * ;;;; consume 64B at a time
 *
 * foldBy4:
 *    load 64B from buffer into vFold1..vFold4
 *    byteswap vFold1..Fold4
 *    vFold1 ^= vScratch
 *    goto advanceBy64B
 *
 * foldBy4Loop:
 *    load 64B from buffer into vInput1, vInput2, vInput3, vInput4
 *    byteswap vInput1, vInput2, vInput3, vInput4
 *    vFold1 = vFold1 * vConstR2R1 + vInput1  (GF(2))
 *    vFold2 = vFold2 * vConstR2R1 + vInput2  (GF(2))
 *    vFold3 = vFold3 * vConstR2R1 + vInput3  (GF(2))
 *    vFold4 = vFold4 * vConstR2R1 + vInput4  (GF(2))
 *
 * advanceBy64B:
 *    buffer += 64
 *    remaining -= 64
 *    if remaining >= 64 goto foldBy4Loop
 *
 * ;;;; reduce 4 vectors into 1
 *
 *    vFold1 = vFold1 * vConstR4R3 + vFold2  (GF(2))
 *    vFold1 = vFold1 * vConstR4R3 + vFold3  (GF(2))
 *    vFold1 = vFold1 * vConstR4R3 + vFold4  (GF(2))
 *    if remaining < 16 goto finalReduction
 *    goto foldBy1Loop    ; jump over foldBy1 header
 *
 * ;;;; consume 16B at a time
 *
 * foldBy1:
 *    load 16B from buffer into vFold1
 *    byteswap vFold1
 *    vFold1 ^= vScratch
 *    goto advanceBy16B
 *
 * foldBy1Loop:
 *    load 16B from buffer into vFold2
 *    byteswap vFold2
 *    vFold1 = vFold1 * vConstR4R3 + vFold2 (GF(2))
 *
 * advanceBy16B:
 *    buffer += 16
 *    remaining -= 16
 *    if remaining >= 16 goto foldBy1Loop
 *
 * ;;;; reduce vFold1 into 32 bit CRC-32C value
 *
 * finalReduction:
 *    move R4 in rightmost doubleword of vScratch and set leftmost doubleword to 1
 *    vFold1 *= vScratch (GF(2))
 *    vFold1 = vFold1 * vConstR5 + (rightmost word of vFold1) (GF(2))
 *    ; apply Barret reduction to vFold1 to produce crc value
 *    move leftmost words of vFold1 into doublewords of vFold2
 *    vFold2 *= vConstRUPoly (GF(2))
 *    move leftmost words of vFold2 into doublewords of vFold2
 *    vFold2 = vFold2 * vConstCRCPoly + vFold1
 *    crc = word element 2 of vFold2
 *    if remaining > 0 goto callJava
 *
 * end:
 *
 * -------------------------------------------------------------
 * ;;;; Out of line snippet
 * callJava:
 *    offset = end - remaining
 *    crc = crc32c.updateBytes(crc, array, offset, end)  ; original java implementation
 *    goto end
 */
TR::Register*
J9::Z::TreeEvaluator::inlineCRC32CUpdateBytes(TR::Node *node, TR::CodeGenerator *cg, bool isDirectBuffer)
   {
   // Get call parameters
   TR::Register* crc = cg->gprClobberEvaluate(node->getChild(0));
   TR::Register* array = cg->gprClobberEvaluate(node->getChild(1));
   TR::Register* offset = cg->gprClobberEvaluate(node->getChild(2));
   TR::Register* end = cg->gprClobberEvaluate(node->getChild(3));

   // Zero extend incoming 32 bit values
   TR::Register* remaining = cg->allocateRegister();
   generateRREInstruction(cg, TR::InstOpCode::LLGFR, node, offset, offset);
   generateRREInstruction(cg, TR::InstOpCode::LLGFR, node, end, end);

   // Calculate buffer pointer = array + offset
   // For updateBytes need to account for array header size
   TR::Register* buffer = cg->allocateRegister();
   generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, buffer, generateS390MemoryReference(array, offset, isDirectBuffer ? 0 : TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg));

   // Adjust remaining count for offset index
   generateRRRInstruction(cg, TR::InstOpCode::getSubtractThreeRegOpCode(), node, remaining, end, offset);

   // If less than 16B of input, branch to bytewise path
   // The vectorized path only works for multiples of 16B
   TR::LabelSymbol* startICF = generateLabelSymbol(cg);
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, startICF);
   startICF->setStartInternalControlFlow();
   TR::RegisterDependencyConditions* dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 23, cg);
   TR::LabelSymbol* callJava = generateLabelSymbol(cg);
   TR::Instruction* cursor = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, remaining, 16, TR::InstOpCode::COND_BL, callJava, false, false, NULL, dependencies);
   TR_Debug* debugObj = cg->getDebug();
   if (debugObj)
      debugObj->addInstructionComment(cursor, "Jump to OOL call to original Java implementation if < 16B");

   /**
    * The CRC-32C vector constant block contains reduction constants to fold and process particular
    * chunks of the input data stream in parallel.
    *
    * The constants are precomputed according to these definitions:
    *
    *	R1 = [(x^(4*128+32) mod P'(x) << 32)]' << 1
    *	R2 = [(x^(4*128-32) mod P'(x) << 32)]' << 1
    *	R3 = [(x^(128+32) mod P'(x) << 32)]'   << 1
    *	R4 = [(x^(128-32) mod P'(x) << 32)]'   << 1
    *	R5 = [(x^64 mod P'(x) << 32)]'	      << 1
    *	R6 = [(x^32 mod P'(x) << 32)]'	      << 1
    *
    *	The bitreflected Barret reduction constant, u', is defined as the bit reversal of floor(x^64 / P(x))
    *	where P(x) is the polynomial in the normal domain and the P'(x) is the polynomial in the reversed (bitreflected) domain.
    *
    * CRC-32C (Castagnoli) polynomials:
    *
    *	P(x)  = 0x1EDC6F41
    *	P'(x) = 0x82F63B78
    */

   // Array of 16 rather than 12 because data snippets on Z must be a power of 2 size - See OMR issue #1815
   // TODO: find out why BE constants don't work, and why we need to byteswap data - See OpenJ9 issue #16474
   uint64_t crc32cVectorConstants[16] =
      {
      0x0F0E0D0C0B0A0908ull, 0x0706050403020100ull,   // BE->LE
      0x000000009E4ADDF8ull, 0x00000000740EEF02ull,   // R2, R1
      0x000000014CD00Bd6ull, 0x00000000F20C0dFEull,   // R4, R3
      0x0000000000000000ull, 0x00000000DD45AAB8ull,   // R5
      0x0000000000000000ull, 0x00000000DEA713F1ull,   // u'
      0x0000000000000000ull, 0x0000000105ec76f0ull,   // P'(x) << 1
      };

   TR::MemoryReference *constantsMemRef = generateS390MemoryReference(cg->findOrCreateConstant(node, crc32cVectorConstants, 128), cg, 0, node);
   dependencies->addAssignAnyPostCondOnMemRef(constantsMemRef);
   // Allocate vectors for reduction constants
   TR::Register* vConstPermLE2BE = cg->allocateRegister(TR_VRF);
   TR::Register* vConstR2R1 = cg->allocateRegister(TR_VRF);
   TR::Register* vConstR4R3 = cg->allocateRegister(TR_VRF);
   TR::Register* vConstR5 = cg->allocateRegister(TR_VRF);
   TR::Register* vConstRUPoly = cg->allocateRegister(TR_VRF);
   TR::Register* vConstCRCPoly = cg->allocateRegister(TR_VRF);
   // The constant vectors need to be adjacent since we use VLM
   dependencies->addPostCondition(vConstPermLE2BE, TR::RealRegister::VRF9);
   dependencies->addPostCondition(vConstR2R1, TR::RealRegister::VRF10);
   dependencies->addPostCondition(vConstR4R3, TR::RealRegister::VRF11);
   dependencies->addPostCondition(vConstR5, TR::RealRegister::VRF12);
   dependencies->addPostCondition(vConstRUPoly, TR::RealRegister::VRF13);
   dependencies->addPostCondition(vConstCRCPoly, TR::RealRegister::VRF14);
   cursor = generateVRSaInstruction(cg, TR::InstOpCode::VLM, node, vConstPermLE2BE, vConstCRCPoly, constantsMemRef, 0);
   if (debugObj)
      debugObj->addInstructionComment(cursor, "Populate vectors with CRC-32C reduction constants");

   TR::Register* vScratch = cg->allocateRegister(TR_VRF);
   // Zero vScratch and load the initial CRC value into the rightmost word
   generateVRIaInstruction(cg, TR::InstOpCode::VGBM, node, vScratch, 0, 0);
   generateVRSbInstruction(cg, TR::InstOpCode::VLVG, node, vScratch, crc, generateS390MemoryReference(3, cg), 2);

   // Jump to 16-byte processing path if less than 64 bytes of data
   TR::LabelSymbol* foldBy1 = generateLabelSymbol(cg);
   cursor = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, remaining, 64, TR::InstOpCode::COND_BL, foldBy1, false, false, NULL, dependencies);
   if (debugObj)
      debugObj->addInstructionComment(cursor, "if remaining < 64 goto foldBy1");

   /************************************** iteratively fold by 4 ******************************************/
   TR::LabelSymbol* foldBy4 = generateLabelSymbol(cg);
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, foldBy4);
   if (debugObj)
      debugObj->addInstructionComment(cursor, "foldBy4");

   // Allocate vectors for CRC computation
   TR::Register* vFold1 = cg->allocateRegister(TR_VRF);
   TR::Register* vFold2 = cg->allocateRegister(TR_VRF);
   TR::Register* vFold3 = cg->allocateRegister(TR_VRF);
   TR::Register* vFold4 = cg->allocateRegister(TR_VRF);
   // The CRC folding vectors need to be adjacent since we use VLM
   dependencies->addPostCondition(vFold1, TR::RealRegister::VRF1);
   dependencies->addPostCondition(vFold2, TR::RealRegister::VRF2);
   dependencies->addPostCondition(vFold3, TR::RealRegister::VRF3);
   dependencies->addPostCondition(vFold4, TR::RealRegister::VRF4);
   // Load first
   generateVRSaInstruction(cg, TR::InstOpCode::VLM, node, vFold1, vFold4, generateS390MemoryReference(buffer, 0, cg), 0);
   generateVRReInstruction(cg, TR::InstOpCode::VPERM, node, vFold1, vFold1, vFold1, vConstPermLE2BE);
   generateVRReInstruction(cg, TR::InstOpCode::VPERM, node, vFold2, vFold2, vFold2, vConstPermLE2BE);
   generateVRReInstruction(cg, TR::InstOpCode::VPERM, node, vFold3, vFold3, vFold3, vConstPermLE2BE);
   generateVRReInstruction(cg, TR::InstOpCode::VPERM, node, vFold4, vFold4, vFold4, vConstPermLE2BE);
   generateVRRcInstruction(cg, TR::InstOpCode::VX, node, vFold1, vScratch, vFold1, 0);

   TR::LabelSymbol* advanceBy64B = generateLabelSymbol(cg);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_B, node, advanceBy64B);

   TR::LabelSymbol* foldBy4Loop = generateLabelSymbol(cg);
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, foldBy4Loop);
   if (debugObj)
      debugObj->addInstructionComment(cursor, "foldBy4Loop");

   // Allocate vectors for loading input data
   TR::Register* vInput1 = cg->allocateRegister(TR_VRF);
   TR::Register* vInput2 = cg->allocateRegister(TR_VRF);
   TR::Register* vInput3 = cg->allocateRegister(TR_VRF);
   TR::Register* vInput4 = cg->allocateRegister(TR_VRF);
   // The input vectors also need to be adjacent since we use VLM
   dependencies->addPostCondition(vInput1, TR::RealRegister::VRF5);
   dependencies->addPostCondition(vInput2, TR::RealRegister::VRF6);
   dependencies->addPostCondition(vInput3, TR::RealRegister::VRF7);
   dependencies->addPostCondition(vInput4, TR::RealRegister::VRF8);
   // Load the next 64-byte chunk of input
   generateVRSaInstruction(cg, TR::InstOpCode::VLM, node, vInput1, vInput4, generateS390MemoryReference(buffer, 0, cg), 0);
   generateVRReInstruction(cg, TR::InstOpCode::VPERM, node, vInput1, vInput1, vInput1, vConstPermLE2BE);
   generateVRReInstruction(cg, TR::InstOpCode::VPERM, node, vInput2, vInput2, vInput2, vConstPermLE2BE);
   generateVRReInstruction(cg, TR::InstOpCode::VPERM, node, vInput3, vInput3, vInput3, vConstPermLE2BE);
   generateVRReInstruction(cg, TR::InstOpCode::VPERM, node, vInput4, vInput4, vInput4, vConstPermLE2BE);

   // GF(2) multiply vFold1..vFold4 with reduction constants, fold (accumulate) with next data chunk and store in vFold1..vFold4
   generateVRRdInstruction(cg, TR::InstOpCode::VGFMA, node, vFold1, vConstR2R1, vFold1, vInput1, 0, 3);
   generateVRRdInstruction(cg, TR::InstOpCode::VGFMA, node, vFold2, vConstR2R1, vFold2, vInput2, 0, 3);
   generateVRRdInstruction(cg, TR::InstOpCode::VGFMA, node, vFold3, vConstR2R1, vFold3, vInput3, 0, 3);
   generateVRRdInstruction(cg, TR::InstOpCode::VGFMA, node, vFold4, vConstR2R1, vFold4, vInput4, 0, 3);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, advanceBy64B);

   // Adjust buffer pointer and remaining count
   generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, buffer, generateS390MemoryReference(buffer, 64, cg));
   generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, remaining, -64);

   // Check remaining buffer size and jump to proper folding method
   cursor = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, remaining, 64, TR::InstOpCode::COND_BNL, foldBy4Loop, false, false, NULL, dependencies);

   TR::LabelSymbol* reduce4to1 = generateLabelSymbol(cg);
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, reduce4to1);
   if (debugObj)
      debugObj->addInstructionComment(cursor, "reduce 4 vectors into 1");

   // Fold vFold1..vFold4 into a single 128-bit value in vFold1
   generateVRRdInstruction(cg, TR::InstOpCode::VGFMA, node, vFold1, vConstR4R3, vFold1, vFold2, 0, 3);
   generateVRRdInstruction(cg, TR::InstOpCode::VGFMA, node, vFold1, vConstR4R3, vFold1, vFold3, 0, 3);
   generateVRRdInstruction(cg, TR::InstOpCode::VGFMA, node, vFold1, vConstR4R3, vFold1, vFold4, 0, 3);

   TR::LabelSymbol* finalReduction = generateLabelSymbol(cg);
   cursor = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, remaining, 16, TR::InstOpCode::COND_BL, finalReduction, false, false, NULL, dependencies);
   if (debugObj)
      debugObj->addInstructionComment(cursor, "if remaining < 16 goto finalReduction");

   // Since the fold-by-4 header already set up the vectors, skip the fold-by-1 header
   TR::LabelSymbol* foldBy1Loop = generateLabelSymbol(cg);
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_B, node, foldBy1Loop);
   if (debugObj)
      debugObj->addInstructionComment(cursor, "Jump over foldBy1 header");

   /************************************** fold by 1 ******************************************/
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, foldBy1);
   if (debugObj)
      debugObj->addInstructionComment(cursor, "foldBy1");

   if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_S390_Z15))
      {
      generateVRXInstruction(cg, TR::InstOpCode::VLBR, node, vFold1, generateS390MemoryReference(buffer, 0, cg), 4);
      }
   else
      {
      generateVRXInstruction(cg, TR::InstOpCode::VL, node, vFold1, generateS390MemoryReference(buffer, 0, cg));
      generateVRReInstruction(cg, TR::InstOpCode::VPERM, node, vFold1, vFold1, vFold1, vConstPermLE2BE);
      }
   generateVRRcInstruction(cg, TR::InstOpCode::VX, node, vFold1, vScratch, vFold1, 0);

   TR::LabelSymbol* advanceBy16B = generateLabelSymbol(cg);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_B, node, advanceBy16B);

   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, foldBy1Loop);
   if (debugObj)
      debugObj->addInstructionComment(cursor, "foldBy1Loop");

   // Load and fold next data chunk
   if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_S390_Z15))
      {
      generateVRXInstruction(cg, TR::InstOpCode::VLBR, node, vFold2, generateS390MemoryReference(buffer, 0, cg), 4);
      }
   else
      {
      generateVRXInstruction(cg, TR::InstOpCode::VL, node, vFold2, generateS390MemoryReference(buffer, 0, cg));
      generateVRReInstruction(cg, TR::InstOpCode::VPERM, node, vFold2, vFold2, vFold2, vConstPermLE2BE);
      }
   generateVRRdInstruction(cg, TR::InstOpCode::VGFMA, node, vFold1, vConstR4R3, vFold1, vFold2, 0, 3);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, advanceBy16B);

   // Adjust buffer pointer and remaining count
   generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, buffer, generateS390MemoryReference(buffer, 16, cg));
   generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, remaining, -16);

   // Check whether to continue with 16-bit folding
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, remaining, 16, TR::InstOpCode::COND_BNL, foldBy1Loop, false, false, NULL, dependencies);

   /************************************** final reduction of 128 bits ******************************************/
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, finalReduction);
   if (debugObj)
      debugObj->addInstructionComment(cursor, "Reduce vector into 32-bit CRC-32C value");

   // Set up a vector register for byte shifts.
   // The shift value must be loaded in bits 1-4 in byte element 7 of the vector.
   // Shift by 8 bytes: 0x40
   // Shift by 4 bytes: 0x20
   TR::Register* vShift = vConstPermLE2BE;  // Don't need this constant anymore, can recycle vector
   generateVRIaInstruction(cg, TR::InstOpCode::VLEIB, node, vShift, 0x40, 7);

   // Prepare vScratch for the next GF(2) multiplication: shift V0 by 8 bytes to move R4 into the rightmost doubleword and set the leftmost doubleword to 0x1.
   generateVRRcInstruction(cg, TR::InstOpCode::VSRLB, node, vScratch, vConstR4R3, vShift, 0);
   generateVRIaInstruction(cg, TR::InstOpCode::VLEIG, node, vScratch, 1, 0);

   // Compute GF(2) product of vFold1 and vScratch.
   // The rightmost doubleword of vFold1 is multiplied with R4.
   // The leftmost doubleword of vFold1 is multiplied by 0x1 then XORed with rightmost product.
   // Implicitly, the intermediate leftmost product becomes padded.
   generateVRRcInstruction(cg, TR::InstOpCode::VGFM, node, vFold1, vScratch, vFold1, 3);

   // Now do the final 32-bit fold by multiplying the rightmost word in vFold1 with R5 and XOR the result with the remaining bits in vFold1.
   //
   // To achieve this by a single VGFMAG, right shift vFold1 by a word and store the result in
   // vFold2 which is then accumulated. Use the vector unpack instruction to load the rightmost
   // half of the doubleword into the rightmost doubleword element of vFold1; the other half is
   // loaded in the leftmost doubleword. vConstR5 contains the R5 constant in the rightmost
   // doubleword and the leftmost doubleword is zero to ignore the leftmost product of vFold1.
   generateVRIaInstruction(cg, TR::InstOpCode::VLEIB, node, vShift, 0x20, 7);
   generateVRRcInstruction(cg, TR::InstOpCode::VSRLB, node, vFold2, vFold1, vShift, 0);
   generateVRRaInstruction(cg, TR::InstOpCode::VUPLL, node, vFold1, vFold1, 0, 0, 2);
   generateVRRdInstruction(cg, TR::InstOpCode::VGFMA, node, vFold1, vConstR5, vFold1, vFold2, 0, 3);

   /**
    * Apply a Barret reduction to compute the final 32-bit CRC value.
    *
    * The input values to the Barret reduction are the degree-63 polynomial in vFold1 (R(x)),
    * degree-32 generator polynomial, and the reduction constant u. The Barret reduction result is
    * the CRC value of R(x) mod P(x).
    *
    * The Barret reduction algorithm is defined as:
    *
    *    1. T1(x) = floor( R(x) / x^32 ) GF2MUL u
    *    2. T2(x) = floor( T1(x) / x^32 ) GF2MUL P(x)
    *    3. C(x)  = R(x) XOR T2(x) mod x^32
    *
    * Note: The leftmost doubleword of vConstRUPoly is zero and, thus, the intermediate GF(2)
    * product is zero and does not contribute to the final result.
    */

   generateVRRaInstruction(cg, TR::InstOpCode::VUPLL, node, vFold2, vFold1, 0, 0, 2);
   generateVRRcInstruction(cg, TR::InstOpCode::VGFM,  node, vFold2, vConstRUPoly, vFold2, 3);

   // Compute the GF(2) product of the CRC polynomial with T1(x) in vFold2 and XOR the intermediate
   // result, T2(x), with the value in vFold1. The final result is stored in word element 2 of vFold2.
   generateVRRaInstruction(cg, TR::InstOpCode::VUPLL, node, vFold2, vFold2, 0, 0, 2);
   generateVRRdInstruction(cg, TR::InstOpCode::VGFMA, node, vFold2, vConstCRCPoly, vFold2, vFold1, 0, 3);

   // Extract the CRC value from word element 2 of vFold2
   generateVRScInstruction(cg, TR::InstOpCode::VLGV, node, crc, vFold2, generateS390MemoryReference(2, cg), 2);

   cg->stopUsingRegister(vScratch);
   cg->stopUsingRegister(vFold1);
   cg->stopUsingRegister(vFold2);
   cg->stopUsingRegister(vFold3);
   cg->stopUsingRegister(vFold4);
   cg->stopUsingRegister(vInput1);
   cg->stopUsingRegister(vInput2);
   cg->stopUsingRegister(vInput3);
   cg->stopUsingRegister(vInput4);
   cg->stopUsingRegister(vShift);
   cg->stopUsingRegister(vConstR2R1);
   cg->stopUsingRegister(vConstR4R3);
   cg->stopUsingRegister(vConstR5);
   cg->stopUsingRegister(vConstRUPoly);
   cg->stopUsingRegister(vConstCRCPoly);
   cg->stopUsingRegister(buffer);

   cursor = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, remaining, 0, TR::InstOpCode::COND_BNE, callJava, false, false, NULL, dependencies);
   if (debugObj)
      debugObj->addInstructionComment(cursor, "Jump to OOL call to original Java implementation if remaining data");

   /************************************** call original implementation for remainder  ******************************************/
   TR::LabelSymbol* done = generateLabelSymbol(cg);
   TR_S390OutOfLineCodeSection *outlinedCall = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(callJava, done, cg);
   cg->getS390OutOfLineCodeSectionList().push_front(outlinedCall);
   outlinedCall->swapInstructionListsWithCompilation();

   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, callJava);
   if (debugObj)
      debugObj->addInstructionComment(cursor, "Denotes start of OOL call to original CRC32C.updateBytes Java implementation");

   // Calculate proper offset
   generateRRRInstruction(cg, TR::InstOpCode::getSubtractThreeRegOpCode(), node, offset, end, remaining);

   // Create dummy node for call
   TR::Node *callNode = TR::Node::createWithSymRef(node, TR::icall, 4, node->getSymbolReference());
   callNode->setChild(0, node->getChild(0));
   callNode->setChild(1, node->getChild(1));
   callNode->setChild(3, node->getChild(3));

   TR::Node *offsetNode = TR::Node::copy(node->getChild(2));
   offsetNode->setRegister(offset);
   callNode->setChild(2, offsetNode);

   // Call java implementation
   cg->incReferenceCount(node->getChild(0));
   cg->incReferenceCount(callNode);
   TR::Register *newCRC = TR::TreeEvaluator::performCall(callNode, false, cg);

   generateRRInstruction(cg, TR::InstOpCode::LGR, node, crc, newCRC);
   cg->stopUsingRegister(newCRC);
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, done);
   if (debugObj)
      debugObj->addInstructionComment(cursor, "Return to main-line");

   outlinedCall->swapInstructionListsWithCompilation();

   cg->stopUsingRegister(array);
   cg->stopUsingRegister(offset);
   cg->stopUsingRegister(end);
   cg->stopUsingRegister(remaining);

   dependencies->addPostCondition(array, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(offset, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(end, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(crc, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(vScratch, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(buffer, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(remaining, TR::RealRegister::AssignAny);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, done, dependencies);
   done->setEndInternalControlFlow();

   node->setRegister(crc);
   cg->decReferenceCount(node->getChild(0));
   cg->decReferenceCount(node->getChild(2));
   cg->decReferenceCount(callNode);

   return crc;
   }

TR::Register*
J9::Z::TreeEvaluator::inlineUTF16BEEncodeSIMD(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation* comp = cg->comp();

   // Create the necessary registers
   TR::Register* output = cg->gprClobberEvaluate(node->getChild(1));
   TR::Register* input  = cg->gprClobberEvaluate(node->getChild(0));

   TR::Register* inputLen;
   TR::Register* inputLen16 = cg->allocateRegister();
   TR::Register* inputLenMinus1 = inputLen16;

   // Number of characters currently translated
   TR::Register* translated = cg->allocateRegister();

   // Initialize the number of translated characters to 0
   generateRREInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, translated, translated);

   TR::Node* inputLenNode = node->getChild(2);

   // Optimize the constant length case
   bool isLenConstant = inputLenNode->getOpCode().isLoadConst() && performTransformation(comp, "O^O [%p] Reduce input length to constant.\n", inputLenNode);

   if (isLenConstant)
      {
      inputLen = cg->allocateRegister();

      // Convert input length in number of characters to number of bytes
      generateLoad32BitConstant(cg, inputLenNode, ((getIntegralValue(inputLenNode) * 2)), inputLen, true);
      generateLoad32BitConstant(cg, inputLenNode, ((getIntegralValue(inputLenNode) * 2) >> 4) << 4, inputLen16, true);
      }
   else
      {
      inputLen = cg->gprClobberEvaluate(inputLenNode, true);

      // Convert input length in number of characters to number of bytes
      generateRSInstruction(cg, TR::InstOpCode::getShiftLeftLogicalSingleOpCode(), node, inputLen, inputLen, 1);

      // Sign extend the value if needed
      if (cg->comp()->target().is64Bit() && !(inputLenNode->getOpCode().isLong()))
         {
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegWidenOpCode(), node, inputLen,   inputLen);
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegWidenOpCode(), node, inputLen16, inputLen);
         }
      else
         {
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, inputLen16, inputLen);
         }

      // Truncate the 4 right most bits
      generateRIInstruction(cg, TR::InstOpCode::NILL, node, inputLen16, static_cast <int16_t> (0xFFF0));
      }

   // Create the necessary vector registers
   TR::Register* vInput     = cg->allocateRegister(TR_VRF);
   TR::Register* vSurrogate = cg->allocateRegister(TR_VRF); // Track index of first surrogate char

   TR::Register* vRange        = cg->allocateRegister(TR_VRF);
   TR::Register* vRangeControl = cg->allocateRegister(TR_VRF);

   // Initialize the vector registers
   uint16_t surrogateRange1 = 0xD800;
   uint16_t surrogateRange2 = 0xDFFF;

   uint16_t surrogateControl1 = 0xA000; // >= comparison
   uint16_t surrogateControl2 = 0xC000; // <= comparison

   generateVRIaInstruction(cg, TR::InstOpCode::VGBM, node, vRange, 0, 0 /*unused*/);
   generateVRIaInstruction(cg, TR::InstOpCode::VGBM, node, vRangeControl, 0, 0 /*unused*/);

   generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, vRange, surrogateRange1, 0);
   generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, vRange, surrogateRange2, 1);

   generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, vRangeControl, surrogateControl1, 0);
   generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, vRangeControl, surrogateControl2, 1);

   // Create the necessary labels
   TR::LabelSymbol * process8Chars         = generateLabelSymbol(cg);
   TR::LabelSymbol * process8CharsEnd      = generateLabelSymbol(cg);

   TR::LabelSymbol * processUnder8Chars    = generateLabelSymbol(cg);
   TR::LabelSymbol * processUnder8CharsEnd = generateLabelSymbol(cg);

   TR::LabelSymbol * processSurrogate      = generateLabelSymbol(cg);
   TR::LabelSymbol * processSurrogateEnd   = generateLabelSymbol(cg);

   // Branch to the end if there are no more multiples of 8 chars left to process
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, inputLen16, 0, TR::InstOpCode::COND_MASK8, process8CharsEnd, false, false);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, process8Chars);
   process8Chars->setStartInternalControlFlow();

   // Load 16 bytes (8 chars) into vector register
   generateVRXInstruction(cg, TR::InstOpCode::VL, node, vInput, generateS390MemoryReference(input, translated, 0, cg));

   // Check for vector surrogates and branch to copy the non-surrogate bytes
   generateVRRdInstruction(cg, TR::InstOpCode::VSTRC, node, vSurrogate, vInput, vRange, vRangeControl, 0x1, 1);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, node, processSurrogate);

   // Store the result
   generateVRXInstruction(cg, TR::InstOpCode::VST, node, vInput, generateS390MemoryReference(output, translated, 0, cg));

   // Advance the stride register
   generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, translated, 16);

   // Loop back if there is at least 8 chars left to process
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, translated, inputLen16, TR::InstOpCode::COND_BL, process8Chars, false, false);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, process8CharsEnd);
   process8CharsEnd->setEndInternalControlFlow();

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, processUnder8Chars);
   processUnder8Chars->setStartInternalControlFlow();

   // Calculate the number of residue bytes available
   generateRRInstruction(cg, TR::InstOpCode::getSubstractRegOpCode(), node, inputLen, translated);

   // Branch to the end if there is no residue
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC0, node, processUnder8CharsEnd);

   // VLL and VSTL work on indices so we must subtract 1
   generateRIEInstruction(cg, TR::InstOpCode::getAddLogicalRegRegImmediateOpCode(), node, inputLenMinus1, inputLen, -1);

   // Zero out the input register to avoid invalid VSTRC result
   generateVRIaInstruction(cg, TR::InstOpCode::VGBM, node, vInput, 0, 0 /*unused*/);

   // VLL instruction can only handle memory references of type D(B), so increment the base input address
   generateRRInstruction (cg, TR::InstOpCode::getAddRegOpCode(), node, input, translated);

   // Load residue bytes into vector register
   generateVRSbInstruction(cg, TR::InstOpCode::VLL, node, vInput, inputLenMinus1, generateS390MemoryReference(input, 0, cg));

   // Check for vector surrogates and branch to copy the non-surrogate bytes
   generateVRRdInstruction(cg, TR::InstOpCode::VSTRC, node, vSurrogate, vInput, vRange, vRangeControl, 0x1, 1);

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC3, node, processSurrogateEnd);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, processSurrogate);

   // Extract the index of the first surrogate char
   generateVRScInstruction(cg, TR::InstOpCode::VLGV, node, inputLen, vSurrogate, generateS390MemoryReference(7, cg), 0);

   // Return in the case of saturation at index 0
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, inputLen, 0, TR::InstOpCode::COND_CC0, processUnder8CharsEnd, false, false);

   // VLL and VSTL work on indices so we must subtract 1
   generateRIEInstruction(cg, TR::InstOpCode::getAddLogicalRegRegImmediateOpCode(), node, inputLenMinus1, inputLen, -1);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, processSurrogateEnd);

   // VSTL instruction can only handle memory references of type D(B), so increment the base output address
   generateRRInstruction (cg, TR::InstOpCode::getAddRegOpCode(), node, output, translated);

   // Store the result
   generateVRSbInstruction(cg, TR::InstOpCode::VSTL, node, vInput, inputLenMinus1, generateS390MemoryReference(output, 0, cg), 0);

   // Advance the stride register
   generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, translated, inputLen);

   // Set up the proper register dependencies
   TR::RegisterDependencyConditions* dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 9, cg);

   dependencies->addPostCondition(input,      TR::RealRegister::AssignAny);
   dependencies->addPostCondition(inputLen,   TR::RealRegister::AssignAny);
   dependencies->addPostCondition(inputLen16, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(output,     TR::RealRegister::AssignAny);
   dependencies->addPostCondition(translated, TR::RealRegister::AssignAny);

   dependencies->addPostCondition(vInput,        TR::RealRegister::AssignAny);
   dependencies->addPostCondition(vSurrogate,    TR::RealRegister::AssignAny);
   dependencies->addPostCondition(vRange,        TR::RealRegister::AssignAny);
   dependencies->addPostCondition(vRangeControl, TR::RealRegister::AssignAny);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, processUnder8CharsEnd, dependencies);
   processUnder8CharsEnd->setEndInternalControlFlow();

   // Convert translated length in number of bytes to number of characters
   generateRSInstruction(cg, TR::InstOpCode::getShiftRightLogicalSingleOpCode(), node, translated, translated, 1);

   // Cleanup nodes before returning
   cg->decReferenceCount(node->getChild(0));
   cg->decReferenceCount(node->getChild(1));
   cg->decReferenceCount(node->getChild(2));

   // Cleanup registers before returning
   cg->stopUsingRegister(input);
   cg->stopUsingRegister(inputLen);
   cg->stopUsingRegister(inputLen16);
   cg->stopUsingRegister(output);

   cg->stopUsingRegister(vInput);
   cg->stopUsingRegister(vSurrogate);
   cg->stopUsingRegister(vRange);
   cg->stopUsingRegister(vRangeControl);

   return node->setRegister(translated);
   }

TR::Register*
J9::Z::TreeEvaluator::inlineStringHashCode(TR::Node* node, TR::CodeGenerator* cg, bool isCompressed)
   {
   TR::Compilation* comp = cg->comp();
   //stringSize = Number of bytes to load to process 4 characters in SIMD loop
   //terminateVal = SIMD loop cotroller allowing characters in multiple of 4 to be processes by loop
   //VLLEZ instruction will load word(compressed String) or double word (decompressed String), elementSize is used for that
   const short stringSize = (isCompressed ? 4 : 8);
   const short terminateVal = (isCompressed ? 3 : 6);
   const short elementSize = (isCompressed ? 2 : 3);

   TR::Node* nodeValue = node->getChild(0);
   TR::Node* nodeIndex = node->getChild(1);
   TR::Node* nodeCount = node->getChild(2);

   // Create the necessary labels
   TR::LabelSymbol * cFlowRegionStart  = generateLabelSymbol(cg);

   TR::LabelSymbol * labelVector       = generateLabelSymbol(cg);
   TR::LabelSymbol * labelVectorLoop   = generateLabelSymbol(cg);
   TR::LabelSymbol * labelVectorReduce = generateLabelSymbol(cg);

   TR::LabelSymbol * labelSerial       = generateLabelSymbol(cg);

   TR::LabelSymbol * labelSerialLoop   = generateLabelSymbol(cg);

   TR::LabelSymbol * cFlowRegionEnd    = generateLabelSymbol(cg);

   // Create the necessary registers
   TR::Register* registerHash = cg->allocateRegister();

   TR::Register* registerValue = cg->evaluate(nodeValue);
   TR::Register* registerIndex = cg->gprClobberEvaluate(nodeIndex);
   TR::Register* registerCount = cg->gprClobberEvaluate(nodeCount);

   if (cg->comp()->target().is64Bit())
      {
      generateRRInstruction(cg, TR::InstOpCode::getLoadRegWidenOpCode(), node, registerIndex, registerIndex);
      generateRRInstruction(cg, TR::InstOpCode::getLoadRegWidenOpCode(), node, registerCount, registerCount);
      }

   TR::Register* registerVA = cg->allocateRegister(TR_VRF);
   TR::Register* registerVB = cg->allocateRegister(TR_VRF);
   TR::Register* registerVC = cg->allocateRegister(TR_VRF);

   TR::Register* registerEnd = cg->allocateRegister(TR_GPR);

   TR::RegisterDependencyConditions* dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 12, cg);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionStart);
   cFlowRegionStart->setStartInternalControlFlow();

   if(!isCompressed)
      {
      // registerIndex *= 2 and registerCount *= 2
      generateRSInstruction(cg, TR::InstOpCode::getShiftLeftLogicalSingleOpCode(), node, registerIndex, registerIndex, 1);
      generateRSInstruction(cg, TR::InstOpCode::getShiftLeftLogicalSingleOpCode(), node, registerCount, registerCount, 1);
      }

   // registerEnd = registerIndex + registerCount
   generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, registerEnd, generateS390MemoryReference(registerIndex, registerCount, 0, cg));

   // registerHash = 0
   generateRREInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, registerHash, registerHash);

   // Branch to labelSerial if registerCount < stringSize
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, registerCount, static_cast<int32_t>(stringSize), TR::InstOpCode::COND_MASK4, labelSerial, false, false);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelVector);

   // registerEnd -= terminateVal
   generateRILInstruction(cg, TR::InstOpCode::getSubtractLogicalImmOpCode(), node, registerEnd, terminateVal);

   // snippetData1 = [31^4, 31^4, 31^4, 31^4]
   int32_t snippetData1[4] = {923521, 923521, 923521, 923521};

   TR::MemoryReference* memrefSnippet1 = generateS390MemoryReference(cg->findOrCreateConstant(node, snippetData1, 16), cg, 0, node);

   dependencies->addAssignAnyPostCondOnMemRef(memrefSnippet1);

   // registerVA = snippetData1
   generateVRXInstruction(cg, TR::InstOpCode::VL, node, registerVA, memrefSnippet1);

   // registerVB = 0
   generateVRIaInstruction(cg, TR::InstOpCode::VGBM, node, registerVB, 0, 0 /*unused*/);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelVectorLoop);

   // registerVC = 4 consecutive chars (16 bit shorts or 8 bit bytes depending on String Compression) at the current index
   generateVRXInstruction(cg, TR::InstOpCode::VLLEZ, node, registerVC, generateS390MemoryReference(registerValue, registerIndex, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg), elementSize);

   if (!isCompressed)
      {
      // registerVC = unpack 4 (16 bit) short elements into 4 (32 bit) int elements
      generateVRRaInstruction(cg, TR::InstOpCode::VUPLH, node, registerVC, registerVC, 0, 0, 1);
      }
   else
      {
      // registerVC = unpack 4 (8 bit) byte elements into 4 (32 bit) int elements
      generateVRRaInstruction(cg, TR::InstOpCode::VUPLH, node, registerVC, registerVC, 0, 0, 0);
      generateVRRaInstruction(cg, TR::InstOpCode::VUPLL, node, registerVC, registerVC, 0, 0, 1);
      }

   // registerVB = registerVB * registerVA + registerVC
   generateVRRdInstruction(cg, TR::InstOpCode::VMAL, node, registerVB, registerVB, registerVA, registerVC, 0, 2);

   // registerIndex += stringSize
   generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, registerIndex, generateS390MemoryReference(registerIndex, stringSize, cg));

   // Branch to labelVectorLoop if registerIndex < registerEnd
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalRegOpCode(), node, registerIndex, registerEnd, TR::InstOpCode::COND_MASK4, labelVectorLoop, false, false);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelVectorReduce);

   // snippetData2 = [31^3, 31^2, 31^1, 31^0]
   int32_t snippetData2[4] = {29791, 961, 31, 1};

   TR::MemoryReference* memrefSnippet2 = generateS390MemoryReference(cg->findOrCreateConstant(node, snippetData2, 16), cg, 0, node);

   dependencies->addAssignAnyPostCondOnMemRef(memrefSnippet2);

   // registerVA = snippetData2
   generateVRXInstruction(cg, TR::InstOpCode::VL, node, registerVA, memrefSnippet2);

   // registerVB = registerVB * registerVA
   generateVRRcInstruction(cg, TR::InstOpCode::VML, node, registerVB, registerVB, registerVA, 2);

   // registerVA = 0
   generateVRIaInstruction(cg, TR::InstOpCode::VGBM, node, registerVA, 0, 0 /*unused*/);

   // registerVA = sum of 4 (32 bit) int elements
   generateVRRcInstruction(cg, TR::InstOpCode::VSUMQ, node, registerVA, registerVB, registerVA, 0, 0, 2);

   // registerEnd += terminateVal
   generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, registerEnd, generateS390MemoryReference(registerEnd, terminateVal, cg));

   generateVRScInstruction(cg, TR::InstOpCode::VLGV, node, registerHash, registerVA, generateS390MemoryReference(3, cg), 2);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelSerial);
   labelSerial->setEndInternalControlFlow();

   // Branch to labelEnd if registerIndex >= registerEnd
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalRegOpCode(), node, registerIndex, registerEnd, TR::InstOpCode::COND_MASK10, cFlowRegionEnd, false, false);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelSerialLoop);
   labelSerialLoop->setStartInternalControlFlow();

   TR::Register* registerTemp = registerCount;

   // registerTemp = registerHash << 5
   generateRSInstruction(cg, TR::InstOpCode::SLLK, node, registerTemp, registerHash, 5);

   // registerTemp -= registerHash
   generateRRInstruction(cg, TR::InstOpCode::getSubstractRegOpCode(), node, registerTemp, registerHash);

   // registerHash = char at registerIndex
   if(isCompressed)
      generateRXInstruction(cg, TR::InstOpCode::LLGC, node, registerHash, generateS390MemoryReference(registerValue, registerIndex, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg));
   else
      generateRXInstruction(cg, TR::InstOpCode::LLH, node, registerHash, generateS390MemoryReference(registerValue, registerIndex, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg));

   if(isCompressed) //registerIndex += 1
      generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, registerIndex, generateS390MemoryReference(registerIndex, 1, cg));
   else //registerIndex += 2
      generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, registerIndex, generateS390MemoryReference(registerIndex, 2, cg));


   // registerHash += registerTemp
   generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, registerHash, registerTemp);

   // Branch to labelSerialLoop if registerIndex < registerEnd
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalRegOpCode(), node, registerIndex, registerEnd, TR::InstOpCode::COND_MASK4, labelSerialLoop, false, false);

   // Set up the proper register dependencies
   dependencies->addPostConditionIfNotAlreadyInserted(registerValue, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(registerIndex, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(registerCount, TR::RealRegister::AssignAny);

   dependencies->addPostConditionIfNotAlreadyInserted(registerHash, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(registerEnd, TR::RealRegister::AssignAny);

   dependencies->addPostConditionIfNotAlreadyInserted(registerVA, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(registerVB, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(registerVC, TR::RealRegister::AssignAny);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionEnd, dependencies);
   cFlowRegionEnd->setEndInternalControlFlow();

   // Cleanup nodes before returning
   cg->decReferenceCount(nodeValue);
   cg->decReferenceCount(nodeIndex);
   cg->decReferenceCount(nodeCount);

   // Cleanup registers before returning
   cg->stopUsingRegister(registerValue);
   cg->stopUsingRegister(registerIndex);
   cg->stopUsingRegister(registerCount);

   cg->stopUsingRegister(registerEnd);

   cg->stopUsingRegister(registerVA);
   cg->stopUsingRegister(registerVB);
   cg->stopUsingRegister(registerVC);

   return node->setRegister(registerHash);
   }

TR::Register*
J9::Z::TreeEvaluator::toUpperIntrinsic(TR::Node *node, TR::CodeGenerator *cg, bool isCompressedString)
   {
   cg->generateDebugCounter("z13/simd/toUpper", 1, TR::DebugCounter::Free);
   return caseConversionHelper(node, cg, true, isCompressedString);
   }

TR::Register*
J9::Z::TreeEvaluator::toLowerIntrinsic(TR::Node *node, TR::CodeGenerator *cg, bool isCompressedString)
   {
   cg->generateDebugCounter("z13/simd/toLower", 1, TR::DebugCounter::Free);
   return caseConversionHelper(node, cg, false, isCompressedString);
   }

TR::Register*
J9::Z::TreeEvaluator::inlineDoubleMax(TR::Node *node, TR::CodeGenerator *cg)
   {
   cg->generateDebugCounter("z13/simd/doubleMax", 1, TR::DebugCounter::Free);
   return doubleMaxMinHelper(node, cg, true);
   }

TR::Register*
J9::Z::TreeEvaluator::inlineDoubleMin(TR::Node *node, TR::CodeGenerator *cg)
   {
   cg->generateDebugCounter("z13/simd/doubleMin", 1, TR::DebugCounter::Free);
   return doubleMaxMinHelper(node, cg, false);
   }

TR::Register *
J9::Z::TreeEvaluator::inlineMathFma(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL(node->getNumChildren() == 3,
   "In function inlineMathFma, the node at address %p should have exactly 3 children, but got %u instead", node, node->getNumChildren());

   TR::Register * a      = cg->evaluate(node->getFirstChild());
   TR::Register * b      = cg->evaluate(node->getSecondChild());
   TR::Register * target = NULL;

   if (cg->getSupportsVectorRegisters() &&
         (!node->getOpCode().isFloat() ||
           cg->comp()->target().cpu.supportsFeature(OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_1)))
      {
      // target = a*b + c
      TR::Register * c = cg->evaluate(node->getThirdChild());
      target = cg->allocateRegister(TR_FPR);
      uint8_t mask6 = getVectorElementSizeMask(TR::DataType::getSize(node->getDataType()));
      generateVRReInstruction(cg, TR::InstOpCode::VFMA, node, target, a, b, c, mask6, 0);
      }
   else
      {
      // target = c
      // target += a*b
      target = cg->fprClobberEvaluate(node->getThirdChild());
      if (node->getDataType() == TR::Double)
         {
         generateRRDInstruction(cg, TR::InstOpCode::MADBR, node, target, a, b);
         }
      else
         {
         generateRRDInstruction(cg, TR::InstOpCode::MAEBR, node, target, a, b);
         }
      }

   node->setRegister(target);

   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   cg->decReferenceCount(node->getThirdChild());

   return target;
   }

/*
 * J9 S390 specific tree evaluator table overrides
 */
extern void TEMPORARY_initJ9S390TreeEvaluatorTable(TR::CodeGenerator *cg)
   {
   OMR::TreeEvaluatorFunctionPointerTable tet = cg->getTreeEvaluatorTable();

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

   tet[TR::NULLCHK] =               TR::TreeEvaluator::NULLCHKEvaluator;
   tet[TR::ResolveAndNULLCHK] =     TR::TreeEvaluator::resolveAndNULLCHKEvaluator;
   }


TR::Instruction *
J9::Z::TreeEvaluator::genLoadForObjectHeaders(TR::CodeGenerator *cg, TR::Node *node, TR::Register *reg, TR::MemoryReference *tempMR, TR::Instruction *iCursor)
   {
   if (TR::Compiler->om.compressObjectReferences())
      return generateRXInstruction(cg, TR::InstOpCode::LLGF, node, reg, tempMR, iCursor);
   return generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, reg, tempMR, iCursor);
   }

TR::Instruction *
J9::Z::TreeEvaluator::genLoadForObjectHeadersMasked(TR::CodeGenerator *cg, TR::Node *node, TR::Register *reg, TR::MemoryReference *tempMR, TR::Instruction *iCursor)
   {
   // Bit-mask for masking J9Object header to extract J9Class
   uint16_t mask = 0xFF00;
   TR::Compilation *comp = cg->comp();
   TR::Instruction *loadInstr;

   if (TR::Compiler->om.compressObjectReferences())
      {
      if (comp->target().cpu.isAtLeast(OMR_PROCESSOR_S390_Z13))
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
      }
   else
      {
      if (comp->target().cpu.isAtLeast(OMR_PROCESSOR_S390_Z13))
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
      }

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
         loadInstr->setNode(comp->findNullChkInfo(node));
         }
      }
   return iCursor;
   }

// max number of cache slots used by checkcat/instanceof
#define NUM_PICS 3

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
   bool eliminateSuperClassArraySizeCheck = (!dynamicCastClass && (castClassDepth < comp->getOptions()->_minimumSuperclassArraySize));


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
         // the helper call returns success.
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
         cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, notInterfaceLabel, cursor);
         }
      else
         {
        cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, callHelperLabel, cursor);
         }
      }


   TR::InstOpCode::Mnemonic loadOp;
   int32_t bytesOffset;

   if (comp->target().is64Bit())
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
         if (comp->target().is64Bit())
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
      if (comp->target().is64Bit())
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

   TR::Compilation *comp = cg->comp();
   int realReg = cg->getGlobalRegister(regIndex);

#if defined(TR_TARGET_64BIT)
   bool needsHelperCall = false;
#if defined(J9ZOS390)
   if (comp->getOption(TR_EnableRMODE64))
#endif
      {
      TR::Node * castClassNode = node->getSecondChild();
      TR::SymbolReference * castClassSymRef = castClassNode->getSymbolReference();
      bool testCastClassIsSuper = TR::TreeEvaluator::instanceOfOrCheckCastNeedSuperTest(node, cg);
      bool isFinalClass = (castClassSymRef == NULL) ? false : castClassSymRef->isNonArrayFinal(comp);
      needsHelperCall = needHelperCall(node, testCastClassIsSuper, isFinalClass);
      }

#endif

   if (realReg == TR::RealRegister::GPR1 ||
       realReg == TR::RealRegister::GPR2 ||
       realReg == cg->getReturnAddressRegister()
#if defined(TR_TARGET_64BIT)
       || (needsHelperCall &&
#if defined(J9ZOS390)
           comp->getOption(TR_EnableRMODE64) &&
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
            if (fej9->instanceOfOrCheckCast((J9Class*)tempGuessClassArray[i], (J9Class*)castClassAddr))
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

      unloadableConstInstr[i] = genLoadProfiledClassAddressConstant(cg, node, guessClassArray[i], scratchReg, NULL, NULL, NULL);
      result_bool = fej9->instanceOfOrCheckCast((J9Class*)(guessClassArray[i]), (J9Class*)castClassAddr);
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
   TR::Compilation *comp = cg->comp();
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
      bool is64Bit = comp->target().is64Bit();
      bool isConstantHeapBase = !comp->getOptions()->isVariableHeapBaseForBarrierRange0();
      bool isConstantHeapSize = !comp->getOptions()->isVariableHeapSizeForBarrierRange0();
      int32_t shiftAmount = TR::Compiler->om.compressedReferenceShift();
      TR::InstOpCode::Mnemonic opLoadReg = TR::InstOpCode::getLoadRegOpCode();
      TR::InstOpCode::Mnemonic opSubtractReg = TR::InstOpCode::getSubstractRegOpCode();
      TR::InstOpCode::Mnemonic opSubtract = TR::InstOpCode::getSubstractOpCode();
      TR::InstOpCode::Mnemonic opCmpLog = TR::InstOpCode::getCmpLogicalOpCode();
      bool disableSrcObjCheck = true; //comp->getOption(TR_DisableWrtBarSrcObjCheck);
      bool constantHeapCase = ((!comp->compileRelocatableCode()) && isConstantHeapBase && isConstantHeapSize && shiftAmount == 0 && (!is64Bit || TR::Compiler->om.generateCompressedObjectHeaders()));
      if (constantHeapCase)
         {
         // these return uintptr_t but because of the if(constantHeapCase) they are guaranteed to be <= MAX(uint32_t). The uses of heapSize, heapBase, and heapSum need to be uint32_t.
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
            generateS390CompareAndBranchInstruction(cg, is64Bit ? TR::InstOpCode::CLG : TR::InstOpCode::CL, node, temp1Reg, static_cast<int64_t>(heapSize), TR::InstOpCode::COND_BH, doneLabel, false, false, NULL, conditions);
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

      if (!comp->getOptions()->realTimeGC())
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
            if (comp->target().is64Bit())
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
            generateS390LabelInstruction(cg, TR::InstOpCode::label, node, srcObjChkLabel, conditions);
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
            // these return uintptr_t but because of the if(constantHeapCase) they are guaranteed to be <= MAX(uint32_t). The uses of heapSize, heapBase, and heapSum need to be uint32_t.
            uint32_t heapBase = comp->getOptions()->getHeapBaseForBarrierRange0();
            uint32_t heapSize = comp->getOptions()->getHeapSizeForBarrierRange0();
            generateRILInstruction(cg, is64Bit ? TR::InstOpCode::SLGFI : TR::InstOpCode::SLFI, node, temp1Reg, heapBase);
            generateS390CompareAndBranchInstruction(cg, is64Bit ? TR::InstOpCode::CLG : TR::InstOpCode::CL, node, temp1Reg, static_cast<int64_t>(heapSize), TR::InstOpCode::COND_BL, doneLabel, false);
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
         generateS390LabelInstruction(cg, TR::InstOpCode::label, node, noChkLabel, conditions);

      // inline checking remembered bit for generational or (gencon+cardmarking is inlined).
      static_assert(J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST <= 0xFF, "The constant is too big");
      int32_t offsetToAgeBits =  TR::Compiler->om.offsetOfHeaderFlags() + 3;
#if defined(J9VM_INTERP_FLAGS_IN_CLASS_SLOT) && defined(TR_TARGET_64BIT)
      if (!TR::Compiler->om.compressObjectReferences())
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
   TR::Compilation *comp = cg->comp();
   if (!comp->getOptions()->realTimeGC())
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
         if (comp->target().is64Bit())
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
   TR::Compilation *comp = cg->comp();
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
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);
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
   intptr_t value = comp->target().is64Bit() ? secondChild->getLongInt() : secondChild->getInt();

   TR_ASSERT( testNode->getOpCodeValue() == (comp->target().is64Bit() ? TR::lcmpeq : TR::icmpeq), "asynccheck bad format");
   TR_ASSERT( secondChild->getOpCode().isLoadConst() && secondChild->getRegister() == NULL, "asynccheck bad format");

   TR::LabelSymbol * snippetLabel = generateLabelSymbol(cg);
   TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
   TR::Instruction * gcPoint;

   TR::LabelSymbol * reStartLabel = generateLabelSymbol(cg);

   // (0)  asynccheck #4[0x004d7a88]Method[jitCheckAsyncMessages]
   // (1)    icmpeq
   // (1)      iload #281[0x00543138] MethodMeta[stackOverflowMark]+28
   // (1)      iconst -1

   if (comp->target().is32Bit() &&
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
      TR::MemoryReference * tempMR = TR::MemoryReference::create(cg, firstChild);

      TR_ASSERT( getIntegralValue(secondChild) == -1, "asynccheck bad format");
      TR_ASSERT( comp->target().is32Bit(), "ICM can be used for 32bit code-gen only!");

      static char * dontUseTM = feGetEnv("TR_DONTUSETMFORASYNC");
      if (firstChild->getReferenceCount()>1 || dontUseTM)
         {
         generateRSInstruction(cg, TR::InstOpCode::ICM, firstChild, testRegister, (uint32_t) 0xF, tempMR);
         gcPoint = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, node, snippetLabel);
         }
      else
         {
         generateSIInstruction(cg, TR::InstOpCode::TM, firstChild, tempMR, 0xFF);
         gcPoint = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BO, node, snippetLabel);
         }

      firstChild->setRegister(testRegister);
      tempMR->stopUsingMemRefRegister(cg);
      }
   else
      {
      if (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL)
         {
         TR::MemoryReference * tempMR = TR::MemoryReference::create(cg, firstChild);

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
         TR::Register * tempReg = cg->evaluate(secondChild);
         generateRRInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, src1Reg, tempReg);
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

   TR_Debug * debugObj = cg->getDebug();
   if (debugObj)
      debugObj->addInstructionComment(gcPoint, "Branch to OOL asyncCheck sequence");

   // starts OOL sequence, replacing the helper call snippet
   TR_S390OutOfLineCodeSection *outlinedHelperCall = NULL;
   outlinedHelperCall = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(snippetLabel, reStartLabel, cg);
   cg->getS390OutOfLineCodeSectionList().push_front(outlinedHelperCall);
   outlinedHelperCall->swapInstructionListsWithCompilation();

   // snippetLabel : OOL Start label
   TR::Instruction * cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, snippetLabel);
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
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, reStartLabel);
   if (debugObj)
      debugObj->addInstructionComment(cursor, "OOL asyncCheck return label");

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
   if (cg->comp()->target().is64Bit())
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
      TR_ASSERT(node->getSecondChild()->getOpCodeValue() != TR::loadaddr,
            "genTestIsSuper: castClassDepth == -1 is not supported for a loadaddr castClass.");
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
      if (cg->comp()->target().is64Bit())
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

///////////////////////////////////////////////////////////////////////////////////////
// instanceofEvaluator: symref is the class object, cp index is in the "int" field,
//   child is the object reference
///////////////////////////////////////////////////////////////////////////////////////
TR::Register *
J9::Z::TreeEvaluator::instanceofEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   static bool initialResult = feGetEnv("TR_instanceOfInitialValue") != NULL;
   traceMsg(comp,"Initial result = %d\n",initialResult);
   // Complementing Initial Result to True if the floag is not passed.
   return VMgenCoreInstanceofEvaluator(node,cg,NULL,NULL,!initialResult,1,NULL,false);
   }

/** \brief
 *     Generates null test of \p objectReg for instanceof or checkcast[AndNULLCHK] \p node. In case a NULLCHK is
 *     required this function will generate the sequence which throws the appropriate exception.
 *
 *  \param node
 *     The instanceof, checkcast, or checkcastAndNULLCHK node.
 *
 *  \param cg
 *     The code generator used to generate the instructions.
 *
 *  \param objectReg
 *     The object to null test.
 *
 *  \return
 *     \c true if a boolean condition code is set and the callee is expected to act on it; \c false otherwise, meaning
 *     a NULLCHK was performed and if \p objectReg was null an exception throwing fallback path will be taken.
 */
static bool
genInstanceOfOrCheckCastNullTest(TR::Node* node, TR::CodeGenerator* cg, TR::Register* objectReg)
   {
   if (node->getOpCodeValue() == TR::checkcastAndNULLCHK)
      {
      if (cg->getHasResumableTrapHandler())
         {
         TR::Instruction* compareAndTrapInstruction = generateRIEInstruction(cg, TR::InstOpCode::getCmpImmTrapOpCode(), node, objectReg, 0, TR::InstOpCode::COND_BE);
         compareAndTrapInstruction->setExceptBranchOp();
         compareAndTrapInstruction->setNeedsGCMap(0x0000FFFF);
         }
      else
         {
         generateRRInstruction(cg, TR::InstOpCode::getLoadTestRegOpCode(), node, objectReg, objectReg);

         TR::Compilation* comp = cg->comp();
         TR::LabelSymbol* snippetLabel = generateLabelSymbol(cg);
         TR::Node* nullChkInfo = comp->findNullChkInfo(node);

         TR::Instruction* branchInstruction = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, nullChkInfo, snippetLabel);
         branchInstruction->setExceptBranchOp();
         branchInstruction->setNeedsGCMap(0x0000FFFF);

         TR::SymbolReference* symRef = comp->getSymRefTab()->findOrCreateNullCheckSymbolRef(comp->getMethodSymbol());
         cg->addSnippet(new (cg->trHeapMemory()) TR::S390HelperCallSnippet(cg, nullChkInfo, snippetLabel, symRef));
         }

      return false;
      }
   else
      {
      generateRRInstruction(cg, TR::InstOpCode::getLoadTestRegOpCode(), node, objectReg, objectReg);

      return true;
      }
   }

///////////////////////////////////////////////////////////////////////////////////////
//  checkcastEvaluator - checkcast
///////////////////////////////////////////////////////////////////////////////////////
TR::Register *
J9::Z::TreeEvaluator::checkcastEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();

   // TODO: This is not the place to make such checks. If we really want to optimize for space or disable inlining
   // of instanceof/checkcast we should still go through the else path to the common infrastructure and it should just
   // generate a call to the helper (along with any null tests if needed for checkcastAndNULLCHK). This should be
   // handled at the common level.
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (comp->fe());
   TR_OpaqueClassBlock           *profiledClass, *compileTimeGuessClass;

   int32_t maxProfiledClasses = comp->getOptions()->getCheckcastMaxProfiledClassTests();
   traceMsg(comp, "%s:Maximum Profiled Classes = %d\n", node->getOpCode().getName(),maxProfiledClasses);
   InstanceOfOrCheckCastProfiledClasses* profiledClassesList = (InstanceOfOrCheckCastProfiledClasses*)alloca(maxProfiledClasses * sizeof(InstanceOfOrCheckCastProfiledClasses));
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
            const bool isCCSet = genInstanceOfOrCheckCastNullTest(node, cg, objectReg);

            if (isCCSet)
               {
               generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, doneLabel);
               }
            }
            break;
         case ClassEqualityTest:
            if (comp->getOption(TR_TraceCG))
               traceMsg(comp, "%s: Emitting Class Equality Test\n", node->getOpCode().getName());
            if (outLinedTest)
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
               generateS390LabelInstruction(cg, TR::InstOpCode::label, node, startOOLLabel);
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
            dynamicCastClass = genInstanceOfOrCheckcastSuperClassTest(node, cg, objClassReg, castClassReg, castClassDepth, callLabel, callLabel, srm);
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
               genLoadProfiledClassAddressConstant(cg, node, profiledClassesList[numPICs].profiledClass, arbitraryClassReg1, NULL, NULL, NULL);
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
            genLoadAddressConstant(cg, node, (uintptr_t)compileTimeGuessClass, arbitraryClassReg2);
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
   J9::Z::CHelperLinkage *helperLink =  static_cast<J9::Z::CHelperLinkage*>(cg->getLinkage(TR_CHelper));
   // We will be generating sequence to call Helper if we have either GoToFalse or HelperCall Test
   if (numSequencesRemaining > 0 && *iter != GoToTrue)
      {

      TR_ASSERT(*iter == HelperCall || *iter == GoToFalse, "Expecting helper call or fail here");
      bool helperCallForFailure = *iter != HelperCall;
      if (comp->getOption(TR_TraceCG))
         traceMsg(comp, "%s: Emitting helper call%s\n", node->getOpCode().getName(),helperCallForFailure?" for failure":"");
      //Following code is needed to put the Helper Call Outlined.
      if (!outlinedSlowPath)
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


      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, callLabel);
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
         generateS390LabelInstruction(cg, TR::InstOpCode::label, node, helperReturnOOLLabel, mergeConditions);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, doneOOLLabel);
         outlinedSlowPath->swapInstructionListsWithCompilation();
         }
      }
   if (resultReg)
      cg->stopUsingRegister(resultReg);
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);
   cg->stopUsingRegister(castClassReg);
   if (objClassReg)
      cg->stopUsingRegister(objClassReg);
   srm->stopUsingRegisters();
   cg->decReferenceCount(objectNode);
   cg->decReferenceCount(castClassNode);
   return NULL;
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
   J9::Z::CHelperLinkage *helperLink = static_cast<J9::Z::CHelperLinkage*>(cg->getLinkage(TR_CHelper));
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
      if (cg->comp()->target().is64Bit())
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
   TR::Compilation* comp = cg->comp();
   if (cg->comp()->suppressAllocationInlining() ||
       TR::TreeEvaluator::requireHelperCallValueTypeAllocation(node, cg))
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
   #define iComment(str) if (compDebug) compDebug->addInstructionComment(cursor, (const_cast<char*>(str)));
   TR::Compilation *comp = cg->comp();
   TR_Debug *compDebug = comp->getDebug();
   TR_ASSERT_FATAL(comp->target().is64Bit(), "multianewArrayEvaluator is only supported on 64-bit JVMs!");
   TR_J9VMBase *fej9 = comp->fej9();
   TR::Register *targetReg = cg->allocateRegister();
   TR::Instruction *cursor = NULL;

   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Node *thirdChild = node->getThirdChild();

   TR::LabelSymbol *cFlowRegionStart = generateLabelSymbol(cg);
   TR::LabelSymbol *nonZeroFirstDimLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *cFlowRegionDone = generateLabelSymbol(cg);
   TR::LabelSymbol *oolFailLabel = generateLabelSymbol(cg);

#if defined(TR_TARGET_64BIT)
   bool isIndexableDataAddrPresent = TR::Compiler->om.isIndexableDataAddrPresent();
   TR::LabelSymbol *populateFirstDimDataAddrSlot = isIndexableDataAddrPresent ? generateLabelSymbol(cg) : NULL;
#endif /* defined(TR_TARGET_64BIT) */

   // oolJumpLabel is a common point that all branches will jump to. From this label, we branch to OOL code.
   // We do this instead of jumping directly to OOL code from mainline because the RA can only handle the case where there's
   // a single jump point to OOL code.
   TR::LabelSymbol *oolJumpLabel = generateLabelSymbol(cg);

   cFlowRegionStart->setStartInternalControlFlow();
   cFlowRegionDone->setEndInternalControlFlow();

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionStart);

   TR::Register *dimsPtrReg = cg->evaluate(firstChild);
   TR::Register *dimReg = cg->evaluate(secondChild);
   TR::Register *classReg = cg->evaluate(thirdChild);

   // In the mainline, first load the first and second dimensions' lengths into registers.
   TR::Register *firstDimLenReg = cg->allocateRegister();
   cursor = generateRXInstruction(cg, TR::InstOpCode::LGF, node, firstDimLenReg, generateS390MemoryReference(dimsPtrReg, 4, cg));
   iComment("Load 1st dim length.");

   TR::Register *secondDimLenReg = cg->allocateRegister();
   cursor = generateRXInstruction(cg, TR::InstOpCode::L, node, secondDimLenReg, generateS390MemoryReference(dimsPtrReg, 0, cg));
   iComment("Load 2nd dim length.");

   // Check to see if second dimension is indeed 0. If yes, then proceed to handle the case here. Otherwise jump to OOL code.
   cursor = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CL, node, secondDimLenReg, 0, TR::InstOpCode::COND_BNE, oolJumpLabel, false);
   iComment("if 2nd dim is 0, we handle it here. Else, jump to oolJumpLabel.");

   // Now check to see if first dimension is also 0. If yes, continue below to handle the case when length for both dimensions is 0. Otherwise jump to nonZeroFirstDimLabel.
   cursor = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CL, node, firstDimLenReg, 0, TR::InstOpCode::COND_BNE, nonZeroFirstDimLabel, false);
   iComment("if 1st dim is also 0, we handle it here. Else, jump to nonZeroFirstDimLabel.");

   // First dimension zero, so only allocate 1 zero-length object array
   TR::Register *vmThreadReg = cg->getMethodMetaDataRealRegister();
   generateRXInstruction(cg, TR::InstOpCode::LG, node, targetReg, generateS390MemoryReference(vmThreadReg, offsetof(J9VMThread, heapAlloc), cg));

   // Take into account alignment requirements for the size of the zero-length array header
   int32_t zeroArraySizeAligned = OMR::align(TR::Compiler->om.discontiguousArrayHeaderSizeInBytes(), TR::Compiler->om.getObjectAlignmentInBytes());

   // Branch to OOL if there's not enough space for an array of size 0.
   TR::Register *temp1Reg = cg->allocateRegister();
   if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_S390_Z196))
      {
      generateRIEInstruction(cg, TR::InstOpCode::AGHIK, node, temp1Reg, targetReg, zeroArraySizeAligned);
      }
   else
      {
      generateRRInstruction(cg, TR::InstOpCode::LGR, node, temp1Reg, targetReg);
      generateRILInstruction(cg, TR::InstOpCode::AGFI, node, temp1Reg, zeroArraySizeAligned);
      }

   generateRXInstruction(cg, TR::InstOpCode::CLG, node, temp1Reg, generateS390MemoryReference(vmThreadReg, offsetof(J9VMThread, heapTop), cg));
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BH, node, oolJumpLabel);
   iComment("Branch to oolJumpLabel if there isn't enough space for a 0 size array.");

   // If there's enough space, then we can continue to allocate.
   generateRXInstruction(cg, TR::InstOpCode::STG, node, temp1Reg, generateS390MemoryReference(vmThreadReg, offsetof(J9VMThread, heapAlloc), cg));

   bool use64BitClasses = comp->target().is64Bit() && !TR::Compiler->om.generateCompressedObjectHeaders();

   generateRXInstruction(cg, use64BitClasses ? TR::InstOpCode::STG : TR::InstOpCode::ST, node, classReg, generateS390MemoryReference(targetReg, TR::Compiler->om.offsetOfObjectVftField(), cg));
#if defined(TR_TARGET_64BIT)
   if (isIndexableDataAddrPresent)
      {
      TR_ASSERT_FATAL_WITH_NODE(node,
         (TR::Compiler->om.compressObjectReferences()
               && (fej9->getOffsetOfDiscontiguousDataAddrField() - fej9->getOffsetOfContiguousDataAddrField()) == 8)
            || (!TR::Compiler->om.compressObjectReferences()
               && fej9->getOffsetOfDiscontiguousDataAddrField() == fej9->getOffsetOfContiguousDataAddrField()),
         "Offset of dataAddr field in discontiguous array is expected to be 8 bytes more than contiguous array if using compressed refs, "
         "or same if using full refs. But was %d bytes for discontiguous and %d bytes for contiguous array.\n",
         fej9->getOffsetOfDiscontiguousDataAddrField(), fej9->getOffsetOfContiguousDataAddrField());

      // Load dataAddr slot offset difference since 0 size arrays are treated as discontiguous.
      generateRIInstruction(cg,
         TR::InstOpCode::LGHI,
         node,
         temp1Reg,
         static_cast<int32_t>(fej9->getOffsetOfDiscontiguousDataAddrField() - fej9->getOffsetOfContiguousDataAddrField()));
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, populateFirstDimDataAddrSlot);
      }
   else
#endif /* TR_TARGET_64BIT */
      {
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cFlowRegionDone);
      }
   iComment("Init class field and jump.");

   // We end up in this region of the ICF if the first dimension is non-zero and the second dimension is zero.
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, nonZeroFirstDimLabel);
   iComment("nonZeroFirstDimLabel, 2nd dim length is 0.");

   TR::Register *componentClassReg = cg->allocateRegister();
   generateRXInstruction(cg, TR::InstOpCode::LG, node, componentClassReg, generateS390MemoryReference(classReg, offsetof(J9ArrayClass, componentType), cg));

   // Calculate maximum allowable object size in elements and jump to OOL if firstDimLenReg is higher than it.
   int32_t elementSize = TR::Compiler->om.sizeofReferenceField();
   uintptr_t maxObjectSize = cg->getMaxObjectSizeGuaranteedNotToOverflow();
   uintptr_t maxObjectSizeInElements = maxObjectSize / elementSize;
   cursor = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CL, node, firstDimLenReg, static_cast<int32_t>(maxObjectSizeInElements), TR::InstOpCode::COND_BHR, oolJumpLabel, false);
   iComment("Jump to oolJumpLabel if 1st dim len > the num of elements a block can fit.");

   // Now check to see if we have enough space to do the allocation. If not then jump to OOL code.
   int32_t elementSizeAligned = OMR::align(elementSize, TR::Compiler->om.getObjectAlignmentInBytes());
   int32_t alignmentCompensation = (elementSize == elementSizeAligned) ? 0 : elementSizeAligned - 1;
   static const uint8_t multiplierToStrideMap[] = {0, 0, 1, 0, 2, 0, 0, 0, 3};
   TR_ASSERT_FATAL(elementSize <= 8, "multianewArrayEvaluator - elementSize cannot be greater than 8!");
   generateRSInstruction(cg, TR::InstOpCode::SLLG, node, temp1Reg, firstDimLenReg, multiplierToStrideMap[elementSize]);
   generateRILInstruction(cg, TR::InstOpCode::AGFI, node, temp1Reg, static_cast<int32_t>(TR::Compiler->om.contiguousArrayHeaderSizeInBytes()) + alignmentCompensation);

   if (alignmentCompensation != 0)
      {
      generateRILInstruction(cg, TR::InstOpCode::NILF, node, temp1Reg, -elementSizeAligned);
      }

   TR::Register *temp2Reg = cg->allocateRegister();
   generateRRInstruction(cg, TR::InstOpCode::LGR, node, temp2Reg, firstDimLenReg);
   generateRILInstruction(cg, TR::InstOpCode::MSGFI, node, temp2Reg, zeroArraySizeAligned);

   cursor = generateRRInstruction(cg, TR::InstOpCode::AGR, node, temp2Reg, temp1Reg);
   iComment("Calculates (firstDimLen * zeroArraySizeAligned) + (arrayStrideInBytes + arrayHeaderSize)");

   generateRXInstruction(cg, TR::InstOpCode::LG, node, targetReg, generateS390MemoryReference(vmThreadReg, offsetof(J9VMThread, heapAlloc), cg));
   generateRRInstruction(cg, TR::InstOpCode::AGR, node, temp2Reg, targetReg);
   generateRXInstruction(cg, TR::InstOpCode::CLG, node, temp2Reg, generateS390MemoryReference(vmThreadReg, offsetof(J9VMThread, heapTop), cg));

   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BH, node, oolJumpLabel);
   iComment("Branch to oolJumpLabel if we don't have enough space for both 1st and 2nd dim.");

   // We have enough space, so proceed with the allocation.
   generateRXInstruction(cg, TR::InstOpCode::STG, node, temp2Reg, generateS390MemoryReference(vmThreadReg, offsetof(J9VMThread, heapAlloc), cg));


   // Init 1st dim array class and size fields.
   cursor = generateRXInstruction(cg, use64BitClasses ? TR::InstOpCode::STG : TR::InstOpCode::ST, node, classReg, generateS390MemoryReference(targetReg, TR::Compiler->om.offsetOfObjectVftField(), cg));
   iComment("Init 1st dim class field.");
   cursor = generateRXInstruction(cg, TR::InstOpCode::ST, node, firstDimLenReg, generateS390MemoryReference(targetReg, fej9->getOffsetOfContiguousArraySizeField(), cg));
   iComment("Init 1st dim size field.");
   // temp2 point to end of 1st dim array i.e. start of 2nd dim
   generateRRInstruction(cg, TR::InstOpCode::LGR, node, temp2Reg, targetReg);
   generateRRInstruction(cg, TR::InstOpCode::AGR, node, temp2Reg, temp1Reg);
   if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_S390_Z196))
      {
      generateRIEInstruction(cg, TR::InstOpCode::AGHIK, node, temp1Reg, targetReg, TR::Compiler->om.contiguousArrayHeaderSizeInBytes());
      }
   else
      {
      generateRRInstruction(cg, TR::InstOpCode::LGR, node, temp1Reg, targetReg);
      generateRILInstruction(cg, TR::InstOpCode::AGFI, node, temp1Reg, static_cast<int32_t>(TR::Compiler->om.contiguousArrayHeaderSizeInBytes()));
      }

   // Loop start
   TR::LabelSymbol *loopLabel = generateLabelSymbol(cg);
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, loopLabel);
   iComment("loopLabel: init 2nd dim's class field.");

   // Init 2nd dim element's class
   cursor = generateRXInstruction(cg, use64BitClasses ? TR::InstOpCode::STG : TR::InstOpCode::ST, node, componentClassReg, generateS390MemoryReference(temp2Reg, TR::Compiler->om.offsetOfObjectVftField(), cg));
   iComment("Init 2nd dim class field.");

   TR::Register *temp3Reg = cg->allocateRegister();

#if defined(TR_TARGET_64BIT)
   if (isIndexableDataAddrPresent)
      {
      // Populate dataAddr slot for 2nd dimension zero size array.
      generateRXInstruction(cg,
         TR::InstOpCode::LA,
         node,
         temp3Reg,
         generateS390MemoryReference(temp2Reg, TR::Compiler->om.discontiguousArrayHeaderSizeInBytes(), cg));
      generateRXInstruction(cg,
         TR::InstOpCode::STG,
         node,
         temp3Reg,
         generateS390MemoryReference(temp2Reg, fej9->getOffsetOfDiscontiguousDataAddrField(), cg));
      }
#endif /* TR_TARGET_64BIT */

   // Store 2nd dim element into 1st dim array slot, compress temp2 if needed
   if (comp->target().is64Bit() && comp->useCompressedPointers())
      {
      int32_t shiftAmount = TR::Compiler->om.compressedReferenceShift();
      generateRRInstruction(cg, TR::InstOpCode::LGR, node, temp3Reg, temp2Reg);
      if (shiftAmount != 0)
         {
         generateRSInstruction(cg, TR::InstOpCode::SRAG, node, temp3Reg, temp3Reg, shiftAmount);
         }
      generateRXInstruction(cg, TR::InstOpCode::ST, node, temp3Reg, generateS390MemoryReference(temp1Reg, 0, cg));
      }
   else
      {
      generateRXInstruction(cg, TR::InstOpCode::STG, node, temp2Reg, generateS390MemoryReference(temp1Reg, 0, cg));
      }

   // Advance cursors temp1 and temp2. Then branch back or fall through if done.
   generateRIInstruction(cg, TR::InstOpCode::AGHI, node, temp2Reg, zeroArraySizeAligned);
   generateRIInstruction(cg, TR::InstOpCode::AGHI, node, temp1Reg, elementSize);

   generateRILInstruction(cg, TR::InstOpCode::SLFI, node, firstDimLenReg, 1);
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CL, node, firstDimLenReg, 0, TR::InstOpCode::COND_BNE, loopLabel, false);

#if defined(TR_TARGET_64BIT)
   if (isIndexableDataAddrPresent)
      {
      // No offset is needed since 1st dimension array is contiguous.
      generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, temp1Reg, temp1Reg);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, populateFirstDimDataAddrSlot);
      }
   else
#endif /* TR_TARGET_64BIT */
      {
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cFlowRegionDone);
      }

   TR::RegisterDependencyConditions *dependencies = generateRegisterDependencyConditions(0,10,cg);
   dependencies->addPostCondition(dimReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(secondDimLenReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(firstDimLenReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(targetReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(dimsPtrReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(temp1Reg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(classReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(componentClassReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(temp2Reg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(temp3Reg, TR::RealRegister::AssignAny);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, oolJumpLabel);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, oolFailLabel);

#if defined(TR_TARGET_64BIT)
   if (isIndexableDataAddrPresent)
      {
      /* Populate dataAddr slot of 1st dimension array. Arrays of non-zero size
       * use contiguous header layout while zero size arrays use discontiguous header layout.
       */
      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, populateFirstDimDataAddrSlot);
      iComment("populateFirstDimDataAddrSlot.");

      generateRXInstruction(cg,
         TR::InstOpCode::LA,
         node,
         temp3Reg,
         generateS390MemoryReference(targetReg, temp1Reg, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg));
      generateRXInstruction(cg,
         TR::InstOpCode::STG,
         node,
         temp3Reg,
         generateS390MemoryReference(targetReg, temp1Reg, fej9->getOffsetOfContiguousDataAddrField(), cg));
      }
#endif /* TR_TARGET_64BIT */

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionDone, dependencies);

   TR::Register *targetRegisterFinal = cg->allocateCollectedReferenceRegister();
   generateRRInstruction(cg, TR::InstOpCode::LGR, node, targetRegisterFinal, targetReg);

   // Generate the OOL code before final bookkeeping.
   TR_S390OutOfLineCodeSection *outlinedSlowPath = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(oolFailLabel, cFlowRegionDone, cg);
   cg->getS390OutOfLineCodeSectionList().push_front(outlinedSlowPath);
   outlinedSlowPath->swapInstructionListsWithCompilation();
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, oolFailLabel);

   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR::Node::recreate(node, TR::acall);
   TR::Register *targetReg2 = TR::TreeEvaluator::performCall(node, false, cg);
   TR::Node::recreate(node, opCode);

   generateRRInstruction(cg, TR::InstOpCode::LGR, node, targetReg, targetReg2);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cFlowRegionDone);
   outlinedSlowPath->swapInstructionListsWithCompilation();

   // Note: We don't decrement the ref count node's children here (i.e. cg->decReferenceCount(node->getFirstChild())) because it is done by the performCall in the OOL code above.
   // Doing so here would end up double decrementing the children nodes' ref count.

   cg->stopUsingRegister(targetReg);
   cg->stopUsingRegister(firstDimLenReg);
   cg->stopUsingRegister(secondDimLenReg);
   cg->stopUsingRegister(temp1Reg);
   cg->stopUsingRegister(temp2Reg);
   cg->stopUsingRegister(temp3Reg);
   cg->stopUsingRegister(componentClassReg);

   node->setRegister(targetRegisterFinal);
   return targetRegisterFinal;
   #undef iComment
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

   if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_S390_Z196))
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

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, oolStartLabel);

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

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, oolReturnLabel);
      }

   cg->decReferenceCount(node->getFirstChild());
   node->setRegister(lengthReg);
   return lengthReg;
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

   // Try to compare directly to memory if the child is a field access (load with no index reg)
   if (divisorIsFieldAccess &&
       !willUseIndexAndBaseReg &&
       (node->getFirstChild()->getOpCodeValue() == TR::idiv ||
        node->getFirstChild()->getOpCodeValue() == TR::irem))
      {
      divisorMr = TR::MemoryReference::create(cg, secondChild);

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
      if (cg->comp()->target().isZOS())
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
           firstChild->getReferenceCount() == 1 &&
           firstChild->getRegister() == NULL &&
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
           secondChild->getReferenceCount() == 1 &&
           secondChild->getRegister() == NULL &&
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

      if (cg->comp()->target().isZOS()) killRegisterIfNotLocked(cg, TR::RealRegister::GPR4, cursor);

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

         if (cg->comp()->target().isZOS())
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
              ((firstChild->getOpCode().isLoadVar() &&
               firstChild->getReferenceCount() == 1 &&
               firstChild->getRegister() == NULL) ||
               (secondChild->getOpCode().isLoadVar() &&
               secondChild->getReferenceCount() == 1 &&
               secondChild->getRegister() == NULL)) &&
              cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_S390_ZEC12))
         {
         // Assume 1st child is the memory operand.
         TR::Node * memChild = firstChild;
         TR::Node * regChild = secondChild;
         TR::InstOpCode::S390BranchCondition compareCondition = TR::InstOpCode::COND_BNL;

         // Check if first child is really the memory operand
         if (!(firstChild->getOpCode().isLoadVar() &&
               firstChild->getReferenceCount() == 1 &&
               firstChild->getRegister() == NULL))
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
                                         TR::MemoryReference::create(cg, memChild));
         cursor->setExceptBranchOp();
         cg->setCanExceptByTrap(true);
         cursor->setNeedsGCMap(0x0000FFFF);

         if (cg->comp()->target().isZOS())
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
      if (cg->comp()->target().isZOS()) killRegisterIfNotLocked(cg, TR::RealRegister::GPR4, cursor);

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
               if (cg->comp()->target().isZOS()) killRegisterIfNotLocked(cg, TR::RealRegister::GPR4, cursor);

               if (skipL2iArrayCopyLengthReg)
                  {
                  cg->decReferenceCount(secondChild->getFirstChild());
                  }
               cg->decReferenceCount(firstChild);
               cg->decReferenceCount(secondChild);

               return NULL;
               }
            // check if we can use Compare-and-Branch at least
            else if (arrayTargetLengthConst <= MAX_IMMEDIATE_BYTE_VAL &&
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
            if (cg->comp()->target().isZOS()) killRegisterIfNotLocked(cg, TR::RealRegister::GPR4, cursor);

            if (skipL2iArrayTargetLengthReg)
               {
               cg->decReferenceCount(firstChild->getFirstChild());
               }
            cg->decReferenceCount(firstChild);
            cg->decReferenceCount(secondChild);

            return NULL;
            }
         // check if we can use Compare-and-Branch at least
         else if (secondChild->getOpCode().isLoadConst() &&
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
J9::Z::TreeEvaluator::generateFillInDataBlockSequenceForUnresolvedField(TR::CodeGenerator *cg, TR::Node *node, TR::Snippet *dataSnippet, bool isWrite, TR::Register *sideEffectRegister, TR::Register *dataSnippetRegister)
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
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, unresolvedLabel);

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
   generateRegLitRefInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, cpAddressReg, reinterpret_cast<uintptr_t>(methodSymbol->getResolvedMethod()->constantPool()), TR_ConstantPool, NULL, 0, 0);
   generateRILInstruction(cg, TR::InstOpCode::LGFI, node, cpIndexReg, symRef->getCPIndex());

   TR_RuntimeHelper helperIndex = isWrite? (isStatic ? TR_jitResolveStaticFieldSetterDirect: TR_jitResolveFieldSetterDirect) :
                                           (isStatic ? TR_jitResolveStaticFieldDirect: TR_jitResolveFieldDirect);
   J9::Z::HelperLinkage *helperLink = static_cast<J9::Z::HelperLinkage*>(cg->getLinkage(runtimeHelperLinkage(helperIndex)));


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
   TR::Instruction *call = generateDirectCall(cg, node, false /*myself*/, cg->symRefTab()->findOrCreateRuntimeHelper(helperIndex), deps);
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
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, mergePointLabel);

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
   J9::Z::HelperLinkage *helperLink = static_cast<J9::Z::HelperLinkage*>(cg->getLinkage(runtimeHelperLinkage(helperIndex)));
   int numArgs = 0;

   // Initialize OOL path and generate label that marks beginning of the OOL code.
   TR_S390OutOfLineCodeSection *outlinedSlowPath = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(fieldReportLabel, mergePointLabel, cg);
   cg->getS390OutOfLineCodeSectionList().push_front(outlinedSlowPath);
   outlinedSlowPath->swapInstructionListsWithCompilation();
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, fieldReportLabel);

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
   TR::Instruction *call = generateDirectCall(cg, node, false /*myself*/, cg->symRefTab()->findOrCreateRuntimeHelper(helperIndex), dependencies);
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
J9::Z::TreeEvaluator::generateTestAndReportFieldWatchInstructions(TR::CodeGenerator *cg, TR::Node *node, TR::Snippet *dataSnippet, bool isWrite, TR::Register *sideEffectRegister, TR::Register *valueReg, TR::Register *dataSnippetRegister)
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
         J9Class *fieldClass = static_cast<TR::J9WatchedStaticFieldSnippet *>(dataSnippet)->getFieldClass();
         if (!(cg->needClassAndMethodPointerRelocations()) && cg->canUseRelativeLongInstructions(reinterpret_cast<int64_t>(fieldClass)))
            {
            // For non-AOT (JIT and JITServer) compiles we don't need to use sideEffectRegister here as the class information is available to us at compile time.
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
   generateRXInstruction(cg, TR::InstOpCode::L, node, fieldClassFlags, generateS390MemoryReference(fieldClassReg, cg->comp()->fej9()->getOffsetOfClassFlags(), cg));
   // Then test the bit to test with the relevant flag to check if fieldwatch is enabled.
   generateRIInstruction(cg, TR::InstOpCode::TMLL, node, fieldClassFlags, J9ClassHasWatchedFields);
   // If Condition Code from above test is not 0, then we branch to OOL (instructions) to report the fieldwatch event. Otherwise fall through to mergePointLabel.
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRNZ, node, fieldReportLabel);

   // Generate instructions to call a VM Helper and report the fieldwatch event. Also generates an instruction to
   // branch back to mainline (mergePointLabel).
   generateReportFieldAccessOutlinedInstructions(node, fieldReportLabel, mergePointLabel, dataSnippet, isWrite, cg, sideEffectRegister, valueReg);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, mergePointLabel);

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
      TR::MemoryReference* loadMemRef = TR::MemoryReference::create(cg, node);

      if (cg->comp()->target().cpu.supportsFeature(OMR_FEATURE_S390_GUARDED_STORAGE))
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
   TR::MemoryReference *tempMR = TR::MemoryReference::create(cg, node);
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
       node->isSpineCheckWithArrayElementChild() && cg->comp()->target().is64Bit() && comp->useCompressedPointers())
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
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, oolReturnLabel);

   if (loadOrStoreChild != evaluatedNode)
      cg->evaluate(loadOrStoreChild);

   // ---------------------------------------------
   // OOL Sequence to handle arraylet calculations.
   // ---------------------------------------------
   TR_S390OutOfLineCodeSection *outlinedDiscontigPath = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(oolStartLabel,oolReturnLabel,cg);
   cg->getS390OutOfLineCodeSectionList().push_front(outlinedDiscontigPath);
   outlinedDiscontigPath->swapInstructionListsWithCompilation();
   TR::Instruction * cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, oolStartLabel);

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

   int32_t spinePointerSize = (cg->comp()->target().is64Bit() && !comp->useCompressedPointers()) ? 8 : 4;
   int32_t arrayHeaderSize = TR::Compiler->om.discontiguousArrayHeaderSizeInBytes();
   int32_t arrayletMask = fej9->getArrayletMask(elementSize);

   // Load the arraylet from the spine.
   int32_t spineShift = fej9->getArraySpineShift(elementSize);
   int32_t spinePtrShift = TR::TreeEvaluator::checkNonNegativePowerOfTwo(spinePointerSize);
   int32_t elementShift = TR::TreeEvaluator::checkNonNegativePowerOfTwo(elementSize);
   TR::Register* tmpReg = cg->allocateRegister();
   if (cg->comp()->target().is64Bit())
      {
      if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_S390_ZEC12))
         {
         cursor = generateRIEInstruction(cg, TR::InstOpCode::RISBGN, node, tmpReg, indexReg,(32+spineShift-spinePtrShift), (128+63-spinePtrShift),(64-spineShift+spinePtrShift),cursor);
         }
      else
         {
         cursor = generateRIEInstruction(cg, TR::InstOpCode::RISBG, node, tmpReg, indexReg,(32+spineShift-spinePtrShift), (128+63-spinePtrShift),(64-spineShift+spinePtrShift),cursor);
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
   bool useCompressedPointers = cg->comp()->target().is64Bit() && comp->useCompressedPointers();
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
      }

   // Calculate the offset with the arraylet for the index.
   TR::Register* tmpReg2 = cg->allocateRegister();
   TR::MemoryReference *arrayletMR;
   if (cg->comp()->target().is64Bit())
      {
      if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_S390_ZEC12))
         {
         cursor = generateRIEInstruction(cg, TR::InstOpCode::RISBGN, node, tmpReg2, indexReg,(64-spineShift- elementShift), (128+63-elementShift),(elementShift),cursor);
         }
      else
         {
         cursor = generateRIEInstruction(cg, TR::InstOpCode::RISBG, node, tmpReg2, indexReg,(64-spineShift- elementShift), (128+63-elementShift),(elementShift),cursor);
         }
      }
   else
      {
      generateShiftThenKeepSelected31Bit(node, cg, tmpReg2, indexReg, 32-spineShift - elementShift, 31-elementShift, elementShift);
      }

   arrayletMR = generateS390MemoryReference(tmpReg, tmpReg2, 0, cg);
   cg->stopUsingRegister(tmpReg);
   cg->stopUsingRegister(tmpReg2);

   if (!actualLoadOrStoreChild->getOpCode().isStore())
      {
      TR::InstOpCode::Mnemonic op = TR::InstOpCode::bad;

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
                               op = (cg->comp()->target().is64Bit() ? TR::InstOpCode::LLGC : TR::InstOpCode::LLC);
                            else
                               op = (cg->comp()->target().is64Bit() ? TR::InstOpCode::LGB : TR::InstOpCode::LB);
                            break;
            case TR::Int16:
                            if (loadOrStoreChild->isZeroExtendedAtSource())
                               op = (cg->comp()->target().is64Bit() ? TR::InstOpCode::LLGH : TR::InstOpCode::LLH);
                            else
                               op = (cg->comp()->target().is64Bit() ? TR::InstOpCode::LGH : TR::InstOpCode::LH);
                            break;
            case TR::Int32:
                            if (loadOrStoreChild->isZeroExtendedAtSource())
                               op = (cg->comp()->target().is64Bit() ? TR::InstOpCode::LLGF : TR::InstOpCode::L);
                            else
                               op = (cg->comp()->target().is64Bit() ? TR::InstOpCode::LGF : TR::InstOpCode::L);
                            break;
            case TR::Int64:
                            if (cg->comp()->target().is64Bit())
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
                            if (cg->comp()->target().is32Bit())
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
               if (cg->comp()->target().is64Bit())
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
               op = TR::InstOpCode::bad;
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
      J9::Z::CHelperLinkage *helperLink,
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
   J9::Z::PrivateLinkage * linkage = static_cast<J9::Z::PrivateLinkage *>(cg->getLinkage());
   int bytesOffset;

   TR::TreeEvaluator::genLoadForObjectHeadersMasked(cg, node, owningObjectRegVal, generateS390MemoryReference(owningObjectReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg), NULL);
   TR::TreeEvaluator::genLoadForObjectHeadersMasked(cg, node,          srcRegVal, generateS390MemoryReference(         srcReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg), NULL);

   // may need to convert the class offset from t1Reg into a J9Class pointer
   cursor = generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, t1Reg, generateS390MemoryReference(owningObjectRegVal, (int32_t) offsetof(J9ArrayClass, componentType), cg));

   // check if obj.class(in t1Reg) == array.componentClass in t2Reg
   if (TR::Compiler->om.compressObjectReferences())
      cursor = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CR, node, t1Reg, srcRegVal, TR::InstOpCode::COND_BER, wbLabel, false, false);
   else
      cursor = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalRegOpCode(), node, t1Reg, srcRegVal, TR::InstOpCode::COND_BE, wbLabel, false, false);

   if (debugObj)
      debugObj->addInstructionComment(cursor, "Check if src.type == array.type");

   intptr_t objectClass = (intptr_t) fej9->getSystemClassFromClassName("java/lang/Object", 16, true);
   /*
    * objectClass is used for Object arrays check optimization: when we are storing to Object arrays we can skip all other array store checks
    * However, TR_J9SharedCacheVM::getSystemClassFromClassName can return 0 when it's impossible to relocate j9class later for AOT loads
    * in that case we don't want to generate the Object arrays check
    */
   bool doObjectArrayCheck = objectClass != 0;

   if (doObjectArrayCheck && cg->needClassAndMethodPointerRelocations())
      {
      if (cg->isLiteralPoolOnDemandOn())
         {
         TR::S390ConstantDataSnippet * targetsnippet;
         if (cg->comp()->target().is64Bit())
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
            }
         }
      else
         {
         if (cg->needClassAndMethodPointerRelocations())
            {
            generateRegLitRefInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, t2Reg,(uintptr_t) objectClass, TR_ClassPointer, conditions, NULL, NULL);
            }
         else
            {
            genLoadAddressConstantInSnippet(cg, node, (intptr_t)objectClass, t2Reg, cursor, conditions, litPoolBaseReg, true);
            }

         if (TR::Compiler->om.compressObjectReferences())
            generateRRInstruction(cg, TR::InstOpCode::CR, node, t1Reg, t2Reg);
         else
            generateRRInstruction(cg, TR::InstOpCode::getCmpLogicalRegOpCode(), node, t1Reg, t2Reg);
         }
      }
   else if (doObjectArrayCheck)
      {
      // make sure that t1Reg contains the class offset and not the J9Class pointer
      if (cg->comp()->target().is64Bit())
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
   if (cg->comp()->target().is64Bit())
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

   if (cg->comp()->target().is64Bit())
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
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, helperCallLabel);
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
      !fej9->classHasBeenExtended(node->getArrayStoreClassInNode()))
      nopASC = true;

   bool usingCompressedPointers = false;
   if (comp->useCompressedPointers() && firstChild->getOpCode().isIndirect())
      {
      usingCompressedPointers = true;
      while (sourceChild->getNumChildren() > 0
         && sourceChild->getOpCodeValue() != TR::a2l)
         {
         sourceChild = sourceChild->getFirstChild();
         }
      if (sourceChild->getOpCodeValue() == TR::a2l)
         {
         sourceChild = sourceChild->getFirstChild();
         }
      // artificially bump up the refCount on the value so
      // that different registers are allocated for the actual
      // and compressed values
      //
      sourceChild->incReferenceCount();
      }
   TR::Node * memRefChild = firstChild->getFirstChild();

   TR::Register * srcReg, * classReg, * txReg, * tyReg, * baseReg, * indexReg, *litPoolBaseReg=NULL,*memRefReg;
   TR::MemoryReference * mr1, * mr2;
   TR::LabelSymbol * wbLabel, * cFlowRegionEnd, * simpleStoreLabel, * cFlowRegionStart;
   TR::RegisterDependencyConditions * conditions;
   J9::Z::PrivateLinkage * linkage = static_cast<J9::Z::PrivateLinkage *>(cg->getLinkage());
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

            generateRXInstruction(cg, TR::InstOpCode::getLoadTestOpCode(), sourceChild, srcReg, TR::MemoryReference::create(cg, sourceChild));

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
   mr1 = TR::MemoryReference::create(cg, firstChild);

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
      // Note the use of 64-bit compare for compressedRefs and use of the decompressed `srcReg` register
      // Compare object with NULL. If NULL, branch around ASC, WrtBar and CrdMrk as they are not required
      generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, srcReg, 0, TR::InstOpCode::COND_BE, (doWrtBar || doCrdMrk)?simpleStoreLabel:wbLabel, false, true);
      }

   J9::Z::CHelperLinkage *helperLink = static_cast<J9::Z::CHelperLinkage*>(cg->getLinkage(TR_CHelper));
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

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, oolASCLabel);
      TR::Register *dummyResReg = helperLink->buildDirectDispatch(callNode);
      if (dummyResReg)
         cg->stopUsingRegister(dummyResReg);

      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, wbLabel);
      outlinedSlowPath->swapInstructionListsWithCompilation();
      }
   else
      VMarrayStoreCHKEvaluator(node, helperLink, callNode, srcReg, classReg, txReg, tyReg, litPoolBaseReg, owningObjectRegVal, srcRegVal, wbLabel, conditions, cg);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, wbLabel);

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

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, simpleStoreLabel);

      mr2 = generateS390MemoryReference(*mr1, 0, cg);
      if (usingCompressedPointers)
         generateRXInstruction(cg, TR::InstOpCode::ST, node, compressedReg, mr2);
      else
         generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, srcReg, mr2);
      }

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionEnd, conditions);
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
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionStart, next->getPrev());
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

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionStart);
   cFlowRegionStart->setStartInternalControlFlow();
   gcPoint = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, testNode->getOpCodeValue() == TR::icmpeq ?  TR::InstOpCode::COND_BE : TR::InstOpCode::COND_BNE, node, snippetLabel);

   TR::LabelSymbol * reStartLabel = generateLabelSymbol(cg);
   TR::Snippet * snippet = new (cg->trHeapMemory()) TR::S390HelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference(), reStartLabel);
   cg->addSnippet(snippet);
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, reStartLabel, dependencies);

   gcPoint->setNeedsGCMap(0x0000FFFF);

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->decReferenceCount(testNode);
   cg->stopUsingRegister(tempReg1);
   cg->stopUsingRegister(tempReg2);

   return NULL;
   }

/**
 * Null check a pointer.  child 1 is indirect reference. Symbolref
 * indicates failure action/destination
 */
TR::Register *
J9::Z::TreeEvaluator::NULLCHKEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return TR::TreeEvaluator::evaluateNULLCHKWithPossibleResolve(node, false, cg);
   }

/**
 * resolveAndNULLCHKEvaluator - Resolve check a static, field or method and Null check
 *    the underlying pointer.  child 1 is reference to be resolved. Symbolref indicates
 *    failure action/destination
 */
TR::Register *
J9::Z::TreeEvaluator::resolveAndNULLCHKEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return TR::TreeEvaluator::evaluateNULLCHKWithPossibleResolve(node, true, cg);
   }

/**
 * This is a helper function used to generate the snippet
 */
TR::Register *
J9::Z::TreeEvaluator::evaluateNULLCHKWithPossibleResolve(TR::Node * node, bool needsResolve, TR::CodeGenerator * cg)
   {
   // NOTE:
   // If no code is generated for the null check, just evaluate the
   // child and decrement its use count UNLESS the child is a pass-through node
   // in which case some kind of explicit test or indirect load must be generated
   // to force the null check at this point.

   TR::Node * firstChild = node->getFirstChild();
   TR::ILOpCode & opCode = firstChild->getOpCode();
   TR::Compilation *comp = cg->comp();
   TR::Node * reference = NULL;
   TR::InstOpCode::S390BranchCondition branchOpCond = TR::InstOpCode::COND_BZ;

   bool hasCompressedPointers = false;

   TR::Node * n = firstChild;

   // NULLCHK has a special case with compressed pointers.
   // In the scenario where the first child is TR::l2a, the
   // node to be null checked is not the iiload, but its child.
   // i.e. aload, aRegLoad, etc.
   if (comp->useCompressedPointers()
         && firstChild->getOpCodeValue() == TR::l2a)
      {
      hasCompressedPointers = true;
      TR::ILOpCodes loadOp = comp->il.opCodeForIndirectLoad(TR::Int32);
      TR::ILOpCodes rdbarOp = comp->il.opCodeForIndirectReadBarrier(TR::Int32);
      while (n->getOpCodeValue() != loadOp && n->getOpCodeValue() != rdbarOp)
         n = n->getFirstChild();
      reference = n->getFirstChild();
      }
   else
      {
      reference = node->getNullCheckReference();
      }

   // Skip the NULLCHK for TR::loadaddr nodes.
   //
   if (cg->getSupportsImplicitNullChecks()
         && reference->getOpCodeValue() == TR::loadaddr)
      {
      cg->evaluate(firstChild);
      cg->decReferenceCount(firstChild);
      return NULL;
      }

   bool needExplicitCheck  = true;
   bool needLateEvaluation = true;

   // Add the explicit check after this instruction
   //
   TR::Instruction *appendTo = NULL;

   // determine if an explicit check is needed
   if (cg->getSupportsImplicitNullChecks()
         && !firstChild->isUnneededIALoad())
      {
      if (opCode.isLoadVar()
            || (cg->comp()->target().is64Bit() && opCode.getOpCodeValue()==TR::l2i)
            || (hasCompressedPointers && firstChild->getFirstChild()->getOpCode().getOpCodeValue() == TR::i2l))
         {
         TR::SymbolReference *symRef = NULL;

         if (opCode.getOpCodeValue()==TR::l2i)
            symRef = n->getFirstChild()->getSymbolReference();
         else
            symRef = n->getSymbolReference();

         // We prefer to generate an explicit NULLCHK vs an implicit one
         // to prevent potential costs of a cache miss on an unnecessary load.
         if (n->getReferenceCount() == 1
               && !n->getSymbolReference()->isUnresolved())
            {
            // If the child is only used here, we don't need to evaluate it
            // since all we need is the grandchild which will be evaluated by
            // the generation of the explicit check below.
            needLateEvaluation = false;

            // at this point, n is the raw iiload (created by lowerTrees) and
            // reference is the aload of the object. node->getFirstChild is the
            // l2a sequence; as a result, n's refCount will always be 1
            // and node->getFirstChild's refCount will be at least 2 (one under the nullchk
            // and the other under the translate treetop)
            //
            if (hasCompressedPointers
                  && node->getFirstChild()->getReferenceCount() > 2)
               needLateEvaluation = true;
            }

         // Check if offset from a NULL reference will fall into the inaccessible bytes,
         // resulting in an implicit trap being raised.
         else if (symRef
               && ((symRef->getSymbol()->getOffset() + symRef->getOffset()) < cg->getNumberBytesReadInaccessible()))
            {
            needExplicitCheck = false;

            // If the child is an arraylength which has been reduced to an iiload,
            // and is only going to be used immediately in a BNDCHK, combine the checks.
            //
            TR::TreeTop *nextTreeTop = cg->getCurrentEvaluationTreeTop()->getNextTreeTop();
            if (n->getReferenceCount() == 2 && nextTreeTop)
               {
               TR::Node *nextTopNode = nextTreeTop->getNode();

               if (nextTopNode)
                  {
                  if (nextTopNode->getOpCode().isBndCheck())
                     {
                     if ((nextTopNode->getOpCode().isSpineCheck() && (nextTopNode->getChild(2) == n))
                           || (!nextTopNode->getOpCode().isSpineCheck() && (nextTopNode->getFirstChild() == n)))
                        {
                        needLateEvaluation = false;
                        nextTopNode->setHasFoldedImplicitNULLCHK(true);
                        traceMsg(comp, "\nMerging NULLCHK [%p] and BNDCHK [%p] of load child [%p]", node, nextTopNode, n);
                        }
                     }
                  else if (nextTopNode->getOpCode().isIf()
                        && nextTopNode->isNonoverriddenGuard()
                        && nextTopNode->getFirstChild() == firstChild)
                     {
                     needLateEvaluation = false;
                     needExplicitCheck = true;
                     reference->incReferenceCount(); // will be decremented again later
                     }
                  }
               }
            }
         }
      else if (opCode.isStore())
         {
         TR::SymbolReference *symRef = n->getSymbolReference();
         if (n->getOpCode().hasSymbolReference()
               && (symRef->getSymbol()->getOffset() + symRef->getOffset() < cg->getNumberBytesWriteInaccessible()))
            {
            needExplicitCheck = false;
            }
         }
      else if (opCode.isCall()
            && opCode.isIndirect()
            && (cg->getNumberBytesReadInaccessible() > TR::Compiler->om.offsetOfObjectVftField()))
         {
         needExplicitCheck = false;
         }
      else if (opCode.getOpCodeValue() == TR::iushr
            && (cg->getNumberBytesReadInaccessible() > cg->fe()->getOffsetOfContiguousArraySizeField()))
         {
         // If the child is an arraylength which has been reduced to an iushr,
         // we must evaluate it here so that the implicit exception will occur
         // at the right point in the program.
         //
         // This can occur when the array length is represented in bytes, not elements.
         // The optimizer must intervene for this to happen.
         //
         cg->evaluate(n->getFirstChild());
         needExplicitCheck = false;
         }
      else if (opCode.getOpCodeValue() == TR::monent
            || opCode.getOpCodeValue() == TR::monexit)
         {
         // The child may generate inline code that provides an implicit null check
         // but we won't know until the child is evaluated.
         //
         reference->incReferenceCount(); // will be decremented again later
         needLateEvaluation = false;
         cg->evaluate(reference);
         appendTo = cg->getAppendInstruction();
         cg->evaluate(firstChild);

         if (cg->getImplicitExceptionPoint()
               && (cg->getNumberBytesReadInaccessible() > cg->fe()->getOffsetOfContiguousArraySizeField()))
            {
            needExplicitCheck = false;
            cg->decReferenceCount(reference);
            }
         }
      }

   // Generate the code for the null check
   //
   if(needExplicitCheck)
      {
      TR::Register * targetRegister = NULL;
      if (cg->getHasResumableTrapHandler())
         {
         // Use Load-And-Trap on zHelix if available.
         // This loads the field and performance a NULLCHK on the field value.
         // i.e.  o.f == NULL
         if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_S390_ZEC12)
               && reference->getOpCode().isLoadVar()
               && (reference->getOpCodeValue() != TR::ardbari)
               && reference->getRegister() == NULL)
            {
            targetRegister = cg->allocateCollectedReferenceRegister();
            appendTo = generateRXInstruction(cg, TR::InstOpCode::getLoadAndTrapOpCode(), node, targetRegister, TR::MemoryReference::create(cg, reference), appendTo);
            reference->setRegister(targetRegister);
            }
         else if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_S390_ZEC12)
               && reference->getRegister() == NULL
               && comp->useCompressedPointers()
               && reference->getOpCodeValue() == TR::l2a
               && reference->getFirstChild()->getOpCodeValue() == TR::iu2l
               && reference->getFirstChild()->getFirstChild()->getOpCode().isLoadVar()
               && TR::Compiler->om.compressedReferenceShiftOffset() == 0)
            {
            targetRegister = cg->evaluate(reference);
            appendTo = cg->getAppendInstruction();
            if (appendTo->getOpCodeValue() == TR::InstOpCode::LLGF)
               {
               appendTo->setOpCodeValue(TR::InstOpCode::LLGFAT);
               appendTo->setNode(node);
               }
            else
               {
               appendTo = generateRIEInstruction(cg, TR::InstOpCode::getCmpImmTrapOpCode(), node, targetRegister, (int16_t)0, TR::InstOpCode::COND_BE, appendTo);
               }
            }
         else
            {
            targetRegister = reference->getRegister();

            if (targetRegister == NULL)
               targetRegister = cg->evaluate(reference);

            appendTo = generateRIEInstruction(cg, TR::InstOpCode::getCmpImmTrapOpCode(), node, targetRegister, (int16_t)0, TR::InstOpCode::COND_BE, appendTo);
            }

         TR::Instruction* cursor = appendTo;
         cursor->setThrowsImplicitException();
         cursor->setExceptBranchOp();
         cg->setCanExceptByTrap(true);
         cursor->setNeedsGCMap(0x0000FFFF);
         if (cg->comp()->target().isZOS())
            killRegisterIfNotLocked(cg, TR::RealRegister::GPR4, cursor);
         }
      else
         {
         // NULLCHK snippet label.
         TR::LabelSymbol * snippetLabel = generateLabelSymbol(cg);
         TR::SymbolReference *symRef = node->getSymbolReference();
         cg->addSnippet(new (cg->trHeapMemory()) TR::S390HelperCallSnippet(cg, node, snippetLabel, symRef));

         if (!firstChild->getOpCode().isCall()
               && reference->getOpCode().isLoadVar()
               && (reference->getOpCodeValue() != TR::ardbari) // ardbari needs to be evaluated before being NULLCHK'ed because of its side effect.
               && reference->getOpCode().hasSymbolReference()
               && reference->getRegister() == NULL)
            {
            bool isInternalPointer = reference->getSymbolReference()->getSymbol()->isInternalPointer();
            if ((reference->getOpCode().isLoadIndirect() || reference->getOpCodeValue() == TR::aload)
                  && !isInternalPointer)
               {
               targetRegister = cg->allocateCollectedReferenceRegister();
               }
            else
               {
               targetRegister = cg->allocateRegister();
               if (isInternalPointer)
                  {
                  targetRegister->setPinningArrayPointer(reference->getSymbolReference()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
                  targetRegister->setContainsInternalPointer();
                  }
               }

            reference->setRegister(targetRegister);
            TR::MemoryReference * tempMR = TR::MemoryReference::create(cg, reference);
            appendTo = generateRXInstruction(cg, TR::InstOpCode::getLoadTestOpCode(), reference, targetRegister, tempMR, appendTo);
            tempMR->stopUsingMemRefRegister(cg);

            appendTo = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, snippetLabel, appendTo);
            TR::Instruction *brInstr = appendTo;
            brInstr->setExceptBranchOp();
            }
         else
            {
            TR::Node *n = NULL;

            // After the NULLCHK is generated, the nodes are guaranteed
            // to be non-zero.  Mark the nodes, so that subsequent
            // evaluations can be optimized.
            if (comp->useCompressedPointers()
                  && reference->getOpCodeValue() == TR::l2a)
               {
               reference->setIsNonNull(true);
               n = reference->getFirstChild();
               TR::ILOpCodes loadOp = comp->il.opCodeForIndirectLoad(TR::Int32);
               TR::ILOpCodes rdbarOp = comp->il.opCodeForIndirectReadBarrier(TR::Int32);
               while (n->getOpCodeValue() != loadOp && n->getOpCodeValue() != rdbarOp)
                  {
                  n->setIsNonZero(true);
                  n = n->getFirstChild();
                  }
               n->setIsNonZero(true);
               }

            TR::InstOpCode::Mnemonic cmpOpCode = TR::InstOpCode::bad;

            // For compressed pointers case, if we find the compressed value,
            // and it has already been evaluated into a register,
            // we can take advantage of the uncompressed value and evaluate
            // the compare result earlier.
            //
            // If it hasn't been evalauted yet, we want to evaluate the entire
            // l2a tree, which might generate LLGF.  In that case, the better
            // choice is to perform the NULLCHK on the decompressed address.
            if (n != NULL && n->getRegister() != NULL)
               {
               targetRegister = n->getRegister();
               cg->evaluate(reference);

               // For concurrent scavenge the source is loaded and shifted by the guarded load, thus we need to use CG
               // here for a non-zero compressedrefs shift value
               if (TR::Compiler->om.readBarrierType() != gc_modron_readbar_none)
                  {
                  cmpOpCode = TR::InstOpCode::getCmpOpCode();
                  }
               else
                  {
                  cmpOpCode = (n->getOpCode().getSize() > 4) ? TR::InstOpCode::CG: TR::InstOpCode::C;
                  }
               }
            else
               {
               targetRegister = cg->evaluate(reference);
               cmpOpCode = TR::InstOpCode::getCmpOpCode();   // reference is always an address type
               }
            appendTo = generateS390CompareAndBranchInstruction(cg, cmpOpCode, node, targetRegister, NULLVALUE, branchOpCond, snippetLabel, false, true, appendTo);
            TR::Instruction * cursor = appendTo;
            cursor->setExceptBranchOp();
            }
         }
      }

   if (needLateEvaluation)
      {
      cg->evaluate(firstChild);
      }
   else if (needExplicitCheck)
      {
      cg->decReferenceCount(reference);
      }

   if (comp->useCompressedPointers())
      cg->decReferenceCount(node->getFirstChild());
   else
      cg->decReferenceCount(firstChild);

   // If an explicit check has not been generated for the null check, there is
   // an instruction that will cause a hardware trap if the exception is to be
   // taken. If this method may catch the exception, a GC stack map must be
   // created for this instruction. All registers are valid at this GC point
   // TODO - if the method may not catch the exception we still need to note
   // that the GC point exists, since maps before this point and after it cannot
   // be merged.
   //
   if (cg->getSupportsImplicitNullChecks() && !needExplicitCheck)
      {
      TR::Instruction *faultingInstruction = cg->getImplicitExceptionPoint();
      if (faultingInstruction)
         {
         faultingInstruction->setNeedsGCMap(0x0000FFFF);
         faultingInstruction->setThrowsImplicitNullPointerException();
         cg->setCanExceptByTrap(true);

         TR_Debug * debugObj = cg->getDebug();
         if (debugObj)
            {
            debugObj->addInstructionComment(faultingInstruction, "Throws Implicit Null Pointer Exception");
            }
         }
      }

   if (comp->useCompressedPointers()
         && reference->getOpCodeValue() == TR::l2a)
      {
      reference->setIsNonNull(true);
      TR::Node *n = NULL;
      n = reference->getFirstChild();
      TR::ILOpCodes loadOp = comp->il.opCodeForIndirectLoad(TR::Int32);
      TR::ILOpCodes rdbarOp = comp->il.opCodeForIndirectReadBarrier(TR::Int32);
      while (n->getOpCodeValue() != loadOp && n->getOpCodeValue() != rdbarOp)
         {
         n->setIsNonZero(true);
         n = n->getFirstChild();
         }
      n->setIsNonZero(true);
      }

   reference->setIsNonNull(true);

   return NULL;
   }

static TR::Register *
reservationLockEnter(TR::Node *node, int32_t lwOffset, TR::Register *objectClassReg, TR::CodeGenerator *cg, J9::Z::CHelperLinkage *helperLink)
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
   if (cg->comp()->target().is64Bit() && fej9->generateCompressedLockWord())
      use64b = false;
   else if (!cg->comp()->target().is64Bit())
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
   //    # IF 64-Bit and JAVA_VERSION >= 19
   //       AGSI #ownedMonitorCountOffset(J9VMThread), 1

   // load monitor reg
   generateRXInstruction(cg, loadOp, node, monitorReg, generateS390MemoryReference(objReg, lwOffset, cg));
   // load r13|LOCK_RESERVATION_BIT
   generateRRInstruction(cg, loadRegOp, node, valReg, metaReg);
   generateRILInstruction(cg, orImmOp, node, valReg, LOCK_RESERVATION_BIT);

   // Jump to OOL path if lock is not reserved (monReg != r13|LOCK_RESERVATION_BIT)
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionStart);
   cFlowRegionStart->setStartInternalControlFlow();
   instr = generateS390CompareAndBranchInstruction(cg, compareOp, node, valReg, monitorReg,
      TR::InstOpCode::COND_BNE, resLabel, false, false);

   helperReturnOOLLabel = generateLabelSymbol(cg);
   doneOOLLabel = generateLabelSymbol(cg);
   if (debugObj)
      debugObj->addInstructionComment(instr, "Branch to OOL reservation enter sequence");
   outlinedSlowPath = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(resLabel, doneOOLLabel, cg);
   cg->getS390OutOfLineCodeSectionList().push_front(outlinedSlowPath);

   cg->generateDebugCounter("LockEnt/LR/LRSuccessfull", 1, TR::DebugCounter::Undetermined);
   if (!isPrimitive)
      {
      generateRIInstruction  (cg, addImmOp, node, monitorReg, (uintptr_t) LOCK_INC_DEC_VALUE);
      generateRXInstruction(cg, storeOp, node, monitorReg, generateS390MemoryReference(objReg, lwOffset, cg));
      }

#if (JAVA_SPEC_VERSION >= 19)
#if defined(TR_TARGET_64BIT)
   generateSIInstruction(cg, TR::InstOpCode::AGSI, node, generateS390MemoryReference(cg->getMethodMetaDataRealRegister(), fej9->thisThreadGetOwnedMonitorCountOffset(), cg), 1);
#else    /* TR_TARGET_64BIT */
   TR_ASSERT_FATAL(false, "Virtual Thread is not supported on 31-Bit platform\n");
#endif   /* TR_TARGET_64BIT */
#endif   /* JAVA_SPEC_VERSION >= 19 */

   if (outlinedSlowPath) // Means we have OOL
      {
      TR::LabelSymbol *reserved_checkLabel = generateLabelSymbol(cg);
      outlinedSlowPath->swapInstructionListsWithCompilation(); // Toggle instruction list
      TR::Instruction *temp = generateS390LabelInstruction(cg,TR::InstOpCode::label,node,resLabel);
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
      // #IF 64-Bit and JAVA_VERSION >= 19
      //    BRC   incrementOwnedMonitorCountLabel
      // #ELSEIF
      //    BRC   returnLabel
      // BRC   returnLabel
      // checkLabel:
      // LGFI  tempReg, LOCK_RES_NON_PRIMITIVE_ENTER_MASK
      // NR    tempReg, monitorReg
      // CRJ   tempReg, valReg, MASK6, callHelper
      // AHI   monitorReg, INC_DEC_VALUE
      // ST    monitorReg, #lwOffset(objectReg)
      // #IF 64-Bit && JAVA_VERSION >=19
      //    incrementOwnedMonitorCountLabel:
      //    AGSI #ownedMonitorCountOffset(J9VMThread), 1
      // BRC   returnLabel
      // callHelper:
      // BRASL R14, jitMonitorEntry
      //returnLabel:

      // Avoid CAS in case lock value is not zero
      generateS390CompareAndBranchInstruction(cg, compareImmOp, node, monitorReg, 0, TR::InstOpCode::COND_BNE, reserved_checkLabel, false);
      if (!isPrimitive)
         {
         generateRIInstruction  (cg, addImmOp, node, valReg, (uintptr_t) LOCK_INC_DEC_VALUE);
         }
      // Try to acquire the lock using CAS
      generateRRInstruction(cg, xorOp, node, monitorReg, monitorReg);
      generateRSInstruction(cg, casOp, node, monitorReg, valReg, generateS390MemoryReference(objReg, lwOffset, cg));
      // Call VM helper if the CAS fails (contention)
      instr = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, callLabel);

      cg->generateDebugCounter("LockEnt/LR/CASSuccessful", 1, TR::DebugCounter::Undetermined);

      // Lock is acquired successfully
#if (JAVA_SPEC_VERSION >= 19)
#if defined(TR_TARGET_64BIT)
      TR::LabelSymbol *incrementOwnedMonitorCountLabel = generateLabelSymbol(cg);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, incrementOwnedMonitorCountLabel);
#else    /* TR_TARGET_64BIT */
      TR_ASSERT_FATAL(false, "Virtual thread is not supported on 31-Bit platform\n");
#endif   /* TR_TARGET_64BIT */
#else    /* JAVA_SPEC_VERSION >= 19 */
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, helperReturnOOLLabel);
#endif   /* JAVA_SPEC_VERSION >= 19 */

      generateS390LabelInstruction(cg,TR::InstOpCode::label,node,reserved_checkLabel);
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
         generateRIInstruction  (cg, addImmOp, node, monitorReg, (uintptr_t) LOCK_INC_DEC_VALUE);
         generateRXInstruction(cg, storeOp, node, monitorReg, generateS390MemoryReference(objReg, lwOffset, cg));
         }

#if (JAVA_SPEC_VERSION >= 19)
#if defined(TR_TARGET_64BIT)
      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, incrementOwnedMonitorCountLabel);
      generateSIInstruction(cg, TR::InstOpCode::AGSI, node, generateS390MemoryReference(cg->getMethodMetaDataRealRegister(), fej9->thisThreadGetOwnedMonitorCountOffset(), cg), 1);
#else    /* TR_TARGET_64BIT */
      TR_ASSERT_FATAL(false, "Virtual thread is not supported on 31-Bit platform\n");
#endif   /* TR_TARGET_64BIT */
#endif   /* JAVA_SPEC_VERSION >= 19 */

      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, helperReturnOOLLabel);
      // call to jithelper
      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, callLabel);
      cg->generateDebugCounter("LockEnt/LR/VMHelper", 1, TR::DebugCounter::Undetermined);

      // We are calling helper within ICF so we need to combine dependency from ICF and helper call at merge label
      TR::RegisterDependencyConditions *deps = NULL;
      helperLink->buildDirectDispatch(node, &deps);
      TR::RegisterDependencyConditions *mergeConditions = mergeConditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(ICFConditions, deps, cg);
      // OOL return label
      instr = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, helperReturnOOLLabel, mergeConditions);
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

      instr = generateS390LabelInstruction(cg,TR::InstOpCode::label,node,doneOOLLabel);
      if (debugObj)
         debugObj->addInstructionComment(instr, "OOL reservation enter return label");
      generateS390LabelInstruction(cg,TR::InstOpCode::label,node, doneLabel);
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
reservationLockExit(TR::Node *node, int32_t lwOffset, TR::Register *objectClassReg, TR::CodeGenerator *cg, J9::Z::CHelperLinkage *helperLink )
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
   if (cg->comp()->target().is64Bit() && fej9->generateCompressedLockWord())
      use64b = false;
   else if (!cg->comp()->target().is64Bit())
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
   // #IF 64-Bit && JAVA_VERSION >=19
   //    AGSI #ownedMonitorCountOffset(J9VMThread), -1

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

   helperReturnOOLLabel = generateLabelSymbol(cg);
   doneOOLLabel = generateLabelSymbol(cg);
   if (debugObj)
      debugObj->addInstructionComment(instr, "Branch to OOL reservation exit sequence");
   outlinedSlowPath = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(resLabel, doneOOLLabel, cg);
   cg->getS390OutOfLineCodeSectionList().push_front(outlinedSlowPath);

   if (!isPrimitive)
      {
      generateRIInstruction  (cg, use64b? TR::InstOpCode::AGHI : TR::InstOpCode::AHI, node, tempReg, -LOCK_INC_DEC_VALUE);
      generateRXInstruction(cg, use64b? TR::InstOpCode::STG : TR::InstOpCode::ST,
         node, tempReg, generateS390MemoryReference(objReg, lwOffset, cg));
      }
#if (JAVA_SPEC_VERSION >= 19)
#if defined(TR_TARGET_64BIT)
      generateSIInstruction(cg, TR::InstOpCode::AGSI, node, generateS390MemoryReference(cg->getMethodMetaDataRealRegister(), fej9->thisThreadGetOwnedMonitorCountOffset(), cg), -1);
#else    /* TR_TARGET_64BIT */
      TR_ASSERT_FATAL(false, "Virtual thread is not supported on 31-Bit platform\n");
#endif   /* TR_TARGET_64BIT */
#endif   /* JAVA_SPEC_VERSION >= 19 */

   if (outlinedSlowPath) // Means we have OOL
      {
      outlinedSlowPath->swapInstructionListsWithCompilation(); // Toggle instruction list
      TR::Instruction *temp = generateS390LabelInstruction(cg,TR::InstOpCode::label,node,resLabel);
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
      // #IF 64-Bit && JAVA_VERSION >=19
      //    AGSI #ownedMonitorCountOffset(J9VMThread), -1
      // BRC   returnLabel
      // callHelper:
      // BRASL R14, jitMonitorExit
      // returnLabel:

      generateRIInstruction(cg, loadImmOp, node, tempReg, ~(LOCK_RES_OWNING_COMPLEMENT));
      generateRRInstruction(cg, andOp, node, tempReg, monitorReg);
      generateRRInstruction(cg, loadRegOp, node, valReg, metaReg);
      generateRIInstruction  (cg, addImmOp, node, valReg, (uintptr_t) LOCK_RESERVATION_BIT);

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionStart);
      cFlowRegionStart->setStartInternalControlFlow();
      generateS390CompareAndBranchInstruction(cg, compareOp, node, tempReg, valReg,
         TR::InstOpCode::COND_BNE, callLabel, false, false);

      generateRRInstruction(cg, loadRegOp, node, tempReg, monitorReg);
      generateRILInstruction(cg, andImmOp, node, tempReg,
         isPrimitive ? OBJECT_HEADER_LOCK_RECURSION_MASK : LOCK_RES_NON_PRIMITIVE_EXIT_MASK);

      if (isPrimitive)
         {
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, helperReturnOOLLabel);
         }
      else
         {
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, callLabel/*,conditions*/);
         }
      cg->generateDebugCounter("LockExit/LR/Recursive", 1, TR::DebugCounter::Undetermined);
      generateRIInstruction  (cg, addImmOp, node, monitorReg,
         (uintptr_t) (isPrimitive ? LOCK_INC_DEC_VALUE : -LOCK_INC_DEC_VALUE) & 0x0000FFFF);
      generateRXInstruction(cg, storeOp, node, monitorReg, generateS390MemoryReference(objReg, lwOffset, cg));

      if (!isPrimitive)
         {
#if (JAVA_SPEC_VERSION >= 19)
#if defined(TR_TARGET_64BIT)
         generateSIInstruction(cg, TR::InstOpCode::AGSI, node, generateS390MemoryReference(cg->getMethodMetaDataRealRegister(), fej9->thisThreadGetOwnedMonitorCountOffset(), cg), -1);
#else    /* TR_TARGET_64BIT */
         TR_ASSERT_FATAL(false, "Virtual thread is not supported on 31-Bit platform\n");
#endif   /* TR_TARGET_64BIT */
#endif   /* JAVA_SPEC_VERSION >= 19 */
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, helperReturnOOLLabel);
         }
      // call to jithelper
      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, callLabel);
      cg->generateDebugCounter("LockExit/LR/VMHelper", 1, TR::DebugCounter::Undetermined);
      TR::RegisterDependencyConditions *deps = NULL;
      helperLink->buildDirectDispatch(node, &deps);
      TR::RegisterDependencyConditions *mergeConditions = mergeConditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(ICFConditions, deps, cg);
      instr = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, helperReturnOOLLabel, mergeConditions);
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
      instr = generateS390LabelInstruction(cg,TR::InstOpCode::label,node,doneOOLLabel);
      if (debugObj)
         debugObj->addInstructionComment(instr, "OOL reservation exit return label");

      generateS390LabelInstruction(cg,TR::InstOpCode::label,node, doneLabel);
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
          && cg->comp()->target().is32Bit())
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

/** \brief
 *     Generates a dynamicCache test with helper call for instanceOf/ifInstanceOf node
 *
 *  \details
 *     This function generates a sequence to check per site cache for object class and cast class before calling out to jitInstanceOf helper
 */
static
void genInstanceOfDynamicCacheAndHelperCall(TR::Node *node, TR::CodeGenerator *cg, TR::Register *castClassReg, TR::Register *objClassReg, TR::Register *resultReg, TR::RegisterDependencyConditions *deps, TR_S390ScratchRegisterManager *srm, TR::LabelSymbol *doneLabel, TR::LabelSymbol *helperCallLabel, TR::LabelSymbol *dynamicCacheTestLabel, TR::LabelSymbol *branchLabel, TR::LabelSymbol *trueLabel, TR::LabelSymbol *falseLabel, bool dynamicCastClass, bool generateDynamicCache, bool cacheCastClass, bool ifInstanceOf, bool trueFallThrough )
   {
   TR::Compilation                *comp = cg->comp();
   bool needResult = resultReg != NULL;
   if (!castClassReg)
      castClassReg = cg->gprClobberEvaluate(node->getSecondChild());

   int32_t maxOnsiteCacheSlots = comp->getOptions()->getMaxOnsiteCacheSlotForInstanceOf();
   int32_t sizeofJ9ClassFieldWithinReference = TR::Compiler->om.sizeofReferenceField();
   bool isTarget64Bit = comp->target().is64Bit();
   bool isCompressedRef = comp->useCompressedPointers();
   /* Layout of the writable data snippet
    * Case - 1 : Cast class is runtime variable
    *    Case - 1A: 64 Bit Compressedrefs / 31-Bit JVM
    *       -----------------------------------------------------------------------------------------
    *       |Header | ObjectClassSlot-0 | CastClassSlot-0 |...| ObjectClassSlot-N | CastClassSlot-N |
    *       -----------------------------------------------------------------------------------------
    *       0        8                   12                ... 8n                  8n+4
    *    Case - 1B: 64 Bit Non Compressedrefs
    *       -----------------------------------------------------------------------------------------
    *       |Header | ObjectClassSlot-0 | CastClassSlot-0 |...| ObjectClassSlot-N | CastClassSlot-N |
    *       -----------------------------------------------------------------------------------------
    *       0        16                  24                ... 16n                 16n+8
    * Case - 2 : Cast Class is resolved
    *    Case - 2A: 64 Bit Compressedrefs / 31-Bit JVM
    *       --------------------------------------------------------------------------
    *       | Header | ObjectClassSlot-0 | ObjectClassSlot-1 |...| ObjectClassSlot-N |
    *       --------------------------------------------------------------------------
    *       0         4                   8                   ... 4n
    *    Case - 2B: 64 Bit Non Compressedrefs
    *       --------------------------------------------------------------------------
    *       | Header | ObjectClassSlot-0 | ObjectClassSlot-1 |...| ObjectClassSlot-N |
    *       --------------------------------------------------------------------------
    *       0         8                   16                   ... 8n
    *
    * If there is only one cache slot, we will not have header.
    * Last bit of cached objectClass will set to 1 indicating false cast
    *
    * We can request the snippet size of power 2. Following Table summarizes bytes needed for corresponding number of cache slots.
    *
    * Following is the table for the number of bytes in snippet needed by each of the Cases mentioned above
    *
    * Number Of Slots | Case 1A | Case 1B | Case 2A | Case 2B |
    *       1         |    8    |   16    |    4    |    8    |
    *       2         |    16   |   64    |    16   |    32   |
    *       3         |    32   |   64    |    16   |    32   |
    *       4         |    64   |   128   |    32   |    64   |
    *       5         |    64   |   128   |    32   |    64   |
    *       6         |    64   |   128   |    32   |    64   |
    *
    */

   int32_t snippetSizeInBytes = ((cacheCastClass ? 2 : 1) * maxOnsiteCacheSlots * sizeofJ9ClassFieldWithinReference) + (sizeofJ9ClassFieldWithinReference * (maxOnsiteCacheSlots != 1) * (cacheCastClass ? 2 : 1));
   TR::Register *dynamicCacheReg = NULL;
   TR::Register *cachedObjectClass = NULL;
   TR::Register *cachedCastClass = NULL;
   TR::RegisterPair *cachedClassDataRegPair = NULL;

   if (generateDynamicCache)
      {
      TR::S390WritableDataSnippet *dynamicCacheSnippet = NULL;
      int32_t requestedBytes = 1 << (int) (log2(snippetSizeInBytes-1)+1);
      if (comp->getOption(TR_TraceCG))
         {
         traceMsg(comp, "Number Of Dynamic Cache Slots = %d, Caching CastClass: %s\n"
                        "Bytes needed for Snippet = %d, requested Bytes = %d\n",maxOnsiteCacheSlots, cacheCastClass ? "true" : "false", snippetSizeInBytes, requestedBytes);
         }

      TR_ASSERT_FATAL(maxOnsiteCacheSlots <= 7, "Maximum 7 slots per site allowed because we use a fixed stack allocated buffer to construct the snippet\n");
      U_32 initialSnippet[32] = { 0 };
      initialSnippet[0] = static_cast<U_32>( sizeofJ9ClassFieldWithinReference * (cacheCastClass ? 2 : 1) );
      dynamicCacheSnippet = (TR::S390WritableDataSnippet*)cg->CreateConstant(node, initialSnippet, requestedBytes, true);

      int32_t currentIndex = maxOnsiteCacheSlots > 1 ? sizeofJ9ClassFieldWithinReference * (cacheCastClass ? 2 : 1) : 0;

      dynamicCacheReg = srm->findOrCreateScratchRegister();

      // Start of the Dyanamic Cache Test
      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, dynamicCacheTestLabel);
      generateRILInstruction(cg, TR::InstOpCode::LARL, node, dynamicCacheReg, dynamicCacheSnippet, 0);

      // For 64-Bit Non Compressedrefs JVM, we need to make sure that we are loading associated class data from the cache that appears quadwoerd concurrent as observed by other CPUs/
      // For that reason, We need to use LPQ/STPQ instruction which needs register pair.
      // In case of 64 bit compressedrefs or 31-Bit JVM, size of J9Class pointer takes 4 bytes only, so in loading associated class data from the cache we can use instruction for 8 byte load/store.
      if (cacheCastClass && isTarget64Bit && !isCompressedRef)
         {
         cachedObjectClass = cg->allocateRegister();
         cachedCastClass = cg->allocateRegister();
         cachedClassDataRegPair = cg->allocateConsecutiveRegisterPair(cachedCastClass, cachedObjectClass);
         deps->addPostCondition(cachedObjectClass, TR::RealRegister::LegalEvenOfPair);
         deps->addPostCondition(cachedCastClass, TR::RealRegister::LegalOddOfPair);
         deps->addPostCondition(cachedClassDataRegPair, TR::RealRegister::EvenOddPair);
         }
      else
         {
         cachedObjectClass = srm->findOrCreateScratchRegister();
         }
      /**
       * Instructions generated for dynamicCache Test are as follows.
       * dynamicCacheTestLabel :
       *    LARL dynamicCacheReg, dynamicCacheSnippet
       *    if (cacheCastClass)
       *       if (isCompressedRef || targetIs31Bit)
       *          LG cachedData, @(dynamicCacheReg, currentIndex)
       *          CLRJ castClass, cachedData, COND_BNE, gotoNextTest
       *          RISBG cachaedData, cachedData, 32, 191, 32 // cachedData >> 32
       *       else
       *          LPQ cachedObjectClass:cachedCastClass, @(dynamicCacheReg, currentIndex)
       *          CLGRJ castClass, cachedCastClass, COND_BNE, gotoNextTest
       *    else
       *       Load cachedObjectClass, @(dynamicCacheReg, currentIndex)
       *    XOR cachedData/cachedObjectClass, objClass
       *    if (cachedData/cachedObjectClass == 0) gotoTrueLabel
       *    else if (cachedData/cachedObjectClass == 1) gotoFalseLabel
       *    gotoNextTest:
       */

      TR::LabelSymbol *gotoNextTest = NULL;
      for (auto i=0; i<maxOnsiteCacheSlots; i++)
         {
         if (cacheCastClass)
            {
            gotoNextTest = (i+1 == maxOnsiteCacheSlots) ? helperCallLabel : generateLabelSymbol(cg);
            if (isTarget64Bit && !isCompressedRef)
               {
               generateRXInstruction(cg, TR::InstOpCode::LPQ, node, cachedClassDataRegPair, generateS390MemoryReference(dynamicCacheReg, currentIndex, cg));
               generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, castClassReg, cachedCastClass, TR::InstOpCode::COND_BNE, gotoNextTest, false);
               }
            else
               {
               generateRXInstruction(cg, TR::InstOpCode::LG, node, cachedObjectClass, generateS390MemoryReference(dynamicCacheReg, currentIndex, cg));
               generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CLR, node, castClassReg, cachedObjectClass, TR::InstOpCode::COND_BNE, gotoNextTest, false);
               generateRIEInstruction(cg, TR::InstOpCode::RISBG, node, cachedObjectClass, cachedObjectClass, 32, 191, 32);
               }
            }
         else
            {
            generateRXInstruction(cg, isTarget64Bit ? (isCompressedRef ? TR::InstOpCode::LLGF : TR::InstOpCode::LG) : TR::InstOpCode::L, node, cachedObjectClass, generateS390MemoryReference(dynamicCacheReg,currentIndex,cg));
            }

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
            generateS390LabelInstruction(cg, TR::InstOpCode::label, node, gotoNextTest);

         currentIndex += ( cacheCastClass ? 2 : 1 ) * sizeofJ9ClassFieldWithinReference;
         }
      if (!cacheCastClass || !isTarget64Bit || isCompressedRef)
         srm->reclaimScratchRegister(cachedObjectClass);
      }
   else if (!dynamicCastClass)
      {
      // If dynamic Cache Test is not generated and it is not dynamicCastClass, we need to generate following branch
      // In cases of dynamic cache test / dynamic Cast Class, we would have a branch to helper call at appropriate location.
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, helperCallLabel);
      }

   TR_S390OutOfLineCodeSection *outlinedSlowPath = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(helperCallLabel, doneLabel, cg);
   cg->getS390OutOfLineCodeSectionList().push_front(outlinedSlowPath);
      outlinedSlowPath->swapInstructionListsWithCompilation();

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, helperCallLabel);
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOf/(%s)/Helper", comp->signature()),1,TR::DebugCounter::Undetermined);
   J9::Z::CHelperLinkage *helperLink =  static_cast<J9::Z::CHelperLinkage*>(cg->getLinkage(TR_CHelper));
   resultReg = helperLink->buildDirectDispatch(node, resultReg);

   if (generateDynamicCache)
      {
      TR::RegisterDependencyConditions *OOLConditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 9, cg);
      if (cacheCastClass && isTarget64Bit && !comp->useCompressedPointers())
         {
         OOLConditions->addPostCondition(cachedObjectClass, TR::RealRegister::LegalEvenOfPair);
         OOLConditions->addPostCondition(cachedCastClass, TR::RealRegister::LegalOddOfPair);
         OOLConditions->addPostCondition(cachedClassDataRegPair, TR::RealRegister::EvenOddPair);
         }
      OOLConditions->addPostCondition(objClassReg, TR::RealRegister::AssignAny);
      OOLConditions->addPostCondition(castClassReg, TR::RealRegister::AssignAny);
      OOLConditions->addPostCondition(resultReg, TR::RealRegister::AssignAny);
      OOLConditions->addPostCondition(dynamicCacheReg, TR::RealRegister::AssignAny);

      TR::LabelSymbol *cFlowRegionStart = generateLabelSymbol(cg);
      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionStart);
      cFlowRegionStart->setStartInternalControlFlow();
      TR::LabelSymbol *skipSettingBitForFalseResult = generateLabelSymbol(cg);
      generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, resultReg, 1, TR::InstOpCode::COND_BE, skipSettingBitForFalseResult, false);
      // We will set the last bit of objectClassRegister to 1 if helper returns false.
      generateRIInstruction(cg, TR::InstOpCode::OILL, node, objClassReg, 0x1);
      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, skipSettingBitForFalseResult);
      TR::MemoryReference *updateMemRef = NULL;
      // Update cache sequence

      TR::Register *offsetRegister = NULL;
      if (maxOnsiteCacheSlots == 1)
         {
         updateMemRef = generateS390MemoryReference(dynamicCacheReg, 0, cg);
         }
      else
         {
         offsetRegister = cg->allocateRegister();
         OOLConditions->addPostCondition(offsetRegister, TR::RealRegister::AssignAny);
         generateRXInstruction(cg, TR::InstOpCode::LLGF, node, offsetRegister, generateS390MemoryReference(dynamicCacheReg,0,cg));
         updateMemRef = generateS390MemoryReference(dynamicCacheReg, offsetRegister, 0, cg);
         }

      if (cacheCastClass)
         {
         if (isTarget64Bit && !isCompressedRef)
            {
            generateRRInstruction(cg, TR::InstOpCode::LGR, node, cachedObjectClass, objClassReg);
            generateRRInstruction(cg, TR::InstOpCode::LGR, node, cachedCastClass, castClassReg);
            generateRXInstruction(cg, TR::InstOpCode::STPQ, node, cachedClassDataRegPair, updateMemRef);
            }
         else
            {
            TR::Register *storeDataCacheReg = castClassReg;
            if (!isTarget64Bit)
               {
               storeDataCacheReg = cg->allocateRegister();
               OOLConditions->addPostCondition(storeDataCacheReg, TR::RealRegister::AssignAny);
               generateRRInstruction(cg, TR::InstOpCode::LGFR, node, storeDataCacheReg, castClassReg);
               }
            generateRIEInstruction(cg, TR::InstOpCode::RISBG, node, storeDataCacheReg, objClassReg, 0, 31, 32);
            generateRXInstruction(cg, TR::InstOpCode::STG, node, storeDataCacheReg, updateMemRef);
            if (!isTarget64Bit)
               cg->stopUsingRegister(storeDataCacheReg);
            }
         }
      else
         {
         generateRXInstruction(cg, sizeofJ9ClassFieldWithinReference == 8 ? TR::InstOpCode::STG : TR::InstOpCode::ST, node, objClassReg, updateMemRef);
         }

      if (maxOnsiteCacheSlots != 1)
         {
         generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, offsetRegister, static_cast<int32_t>(cacheCastClass?sizeofJ9ClassFieldWithinReference*2:sizeofJ9ClassFieldWithinReference));
         TR::LabelSymbol *skipResetOffsetLabel = generateLabelSymbol(cg);
         generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::C, node, offsetRegister, snippetSizeInBytes, TR::InstOpCode::COND_BNE, skipResetOffsetLabel, false);
         generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode() , node, offsetRegister, sizeofJ9ClassFieldWithinReference * (cacheCastClass ? 2 : 1));
         generateS390LabelInstruction(cg, TR::InstOpCode::label, node, skipResetOffsetLabel);
         generateRXInstruction(cg, TR::InstOpCode::ST, node, offsetRegister, generateS390MemoryReference(dynamicCacheReg,0,cg));
         }

      TR::LabelSymbol *doneCacheUpdateLabel = generateLabelSymbol(cg);
      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, doneCacheUpdateLabel, OOLConditions);
      doneCacheUpdateLabel->setEndInternalControlFlow();
      srm->reclaimScratchRegister(dynamicCacheReg);
      if (offsetRegister != NULL)
         cg->stopUsingRegister(offsetRegister);
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
J9::Z::TreeEvaluator::VMgenCoreInstanceofEvaluator(TR::Node * node, TR::CodeGenerator * cg, TR::LabelSymbol *trueLabel, TR::LabelSymbol *falseLabel,
   bool initialResult, bool needResult, TR::RegisterDependencyConditions *graDeps, bool ifInstanceOf)
   {
   TR::Compilation                *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (comp->fe());
   TR_OpaqueClassBlock           *compileTimeGuessClass;
   int32_t maxProfiledClasses = comp->getOptions()->getCheckcastMaxProfiledClassTests();
   traceMsg(comp, "%s:Maximum Profiled Classes = %d\n", node->getOpCode().getName(),maxProfiledClasses);
   InstanceOfOrCheckCastProfiledClasses* profiledClassesList = (InstanceOfOrCheckCastProfiledClasses*)alloca(maxProfiledClasses * sizeof(InstanceOfOrCheckCastProfiledClasses));

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
            castClassReg = cg->gprClobberEvaluate(node->getSecondChild());
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
            const bool isCCSet = genInstanceOfOrCheckCastNullTest(node, cg, objectReg);

            if (isCCSet)
               {
               // If object is Null, and initialResult is true, go to oppositeResultLabel else goto done Label
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
               genLoadProfiledClassAddressConstant(cg, node, profiledClassesList[numPICs].profiledClass, arbitraryClassReg1, NULL, NULL, NULL);
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
            genLoadAddressConstant(cg, node, (uintptr_t)compileTimeGuessClass, arbitraryClassReg2);
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

   TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(graDeps, 0, 8+srm->numAvailableRegisters(), cg);
   if (numSequencesRemaining > 0 && *iter == HelperCall)
      genInstanceOfDynamicCacheAndHelperCall(node, cg, castClassReg, objClassReg, resultReg, conditions, srm, doneLabel, callLabel, dynamicCacheTestLabel, branchLabel, trueLabel, falseLabel, dynamicCastClass, generateDynamicCache, cacheCastClass, ifInstanceOf, trueFallThrough);

   if (needResult)
      {
      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, oppositeResultLabel);
      generateRIInstruction(cg,TR::InstOpCode::getLoadHalfWordImmOpCode(),node,resultReg,static_cast<int32_t>(!initialResult));
      }

   if (objClassReg)
      conditions->addPostConditionIfNotAlreadyInserted(objClassReg, TR::RealRegister::AssignAny);
   if (needResult)
      conditions->addPostCondition(resultReg, TR::RealRegister::AssignAny);
   conditions->addPostConditionIfNotAlreadyInserted(objectReg, TR::RealRegister::AssignAny);
   if (castClassReg)
      conditions->addPostConditionIfNotAlreadyInserted(castClassReg, TR::RealRegister::AssignAny);
   srm->addScratchRegistersToDependencyList(conditions);
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);
   if (objClassReg)
      cg->stopUsingRegister(objClassReg);
   if (castClassReg)
      cg->stopUsingRegister(castClassReg);
   srm->stopUsingRegisters();
   cg->decReferenceCount(objectNode);
   cg->decReferenceCount(castClassNode);
   TR::Register *ret = needResult ? resultReg : NULL;
   conditions->stopUsingDepRegs(cg, objectReg, ret);
   if (needResult)
      node->setRegister(resultReg);
   return resultReg;
   }

/**   \brief Sets up parameters for VMgenCoreInstanceOfEvaluator when we have a ifInstanceOf node
 *    \details
 *    For ifInstanceOf node, it checks if the node has GRA dependency node as third child and if it has, calls normal instanceOf
 *    Otherwise calls VMgenCoreInstanceOfEvaluator with parameters to generate instructions for ifInstanceOf.
 */
TR::Register *
J9::Z::TreeEvaluator::VMifInstanceOfEvaluator(TR::Node * node, TR::CodeGenerator * cg)
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

   VMgenCoreInstanceofEvaluator(instanceOfNode, cg, trueLabel, falseLabel, initialResult, needResult, graDeps, true);

   cg->decReferenceCount(instanceOfNode);
   node->setRegister(NULL);

   return NULL;
   }

/**
 * Generates a quick runtime test for valueType/valueBased node and in case if node is of valueType or valueBased, generates a branch to helper call
 *
 * @param node monent/exit node
 * @param mergeLabel Label pointing to merge point
 * @param helperCallLabel Label pointing to helper call dispatch sequence.
 * @param cg Codegenerator object
 * @return Returns a register containing objectClassPointer
 */
static TR::Register*
generateCheckForValueMonitorEnterOrExit(TR::Node *node, TR::LabelSymbol* mergeLabel, TR::LabelSymbol *helperCallLabel, TR::CodeGenerator *cg)
   {
   TR::Register *objReg = cg->evaluate(node->getFirstChild());
   TR::Register *objectClassReg = cg->allocateRegister();

   TR::TreeEvaluator::genLoadForObjectHeadersMasked(cg, node, objectClassReg, generateS390MemoryReference(objReg, TR::Compiler->om.offsetOfObjectVftField(), cg), NULL);

   TR::Register *tempReg = cg->allocateRegister();
   generateLoad32BitConstant(cg, node, J9_CLASS_DISALLOWS_LOCKING_FLAGS, tempReg, false);

   TR::MemoryReference *classFlagsMemRef = generateS390MemoryReference(objectClassReg, static_cast<uint32_t>(static_cast<TR_J9VMBase *>(cg->comp()->fe())->getOffsetOfClassFlags()), cg);
   generateRXInstruction(cg, TR::InstOpCode::N, node, tempReg, classFlagsMemRef);

   bool generateOOLSection = helperCallLabel == NULL;
   if (generateOOLSection)
      helperCallLabel = generateLabelSymbol(cg);

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRNZ, node, helperCallLabel);

   // TODO: There is now the possibility of multiple distinct OOL sections with helper calls to be generated when
   // evaluating the TR::monent or TR::monexit nodes:
   //
   // 1. Monitor cache lookup OOL
   // 2. Lock reservation OOL
   // 3. Value types or value based object OOL
   // 4. Recursive CAS sequence for Locking
   //
   // These distinct OOL sections may perform non-trivial logic but what they all have in common is they all have a
   // call to the same JIT helper which acts as a fall back. This complexity exists because of the way the evaluators
   // are currently architected and due to the restriction that we cannot have nested OOL code sections. Whenever
   // making future changes to these evaluators we should consider refactoring them to reduce the complexity and
   // attempt to consolidate the calls to the JIT helper so as to not have multiple copies.
   if (generateOOLSection)
      {
      TR_S390OutOfLineCodeSection *helperCallOOLSection = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(helperCallLabel, mergeLabel, cg);
      cg->getS390OutOfLineCodeSectionList().push_front(helperCallOOLSection);
      helperCallOOLSection->swapInstructionListsWithCompilation();

      TR::Instruction *cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, helperCallLabel);

      TR_Debug *debugObj = cg->getDebug();
      if (debugObj)
         debugObj->addInstructionComment(cursor, "Denotes Start of OOL for ValueType or ValueBased Node");

      cg->getLinkage(TR_CHelper)->buildDirectDispatch(node);
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, mergeLabel);

      if (debugObj)
         debugObj->addInstructionComment(cursor, "Denotes End of OOL for ValueType or ValueBased Node");

      helperCallOOLSection->swapInstructionListsWithCompilation();
      }

   cg->stopUsingRegister(tempReg);
   return objectClassReg;
   }

TR::Register *
J9::Z::TreeEvaluator::VMmonentEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   int32_t lwOffset = fej9->getByteOffsetToLockword((TR_OpaqueClassBlock *) cg->getMonClass(node));
   J9::Z::CHelperLinkage *helperLink =  static_cast<J9::Z::CHelperLinkage*>(cg->getLinkage(TR_CHelper));
   TR_YesNoMaybe isMonitorValueBasedOrValueType = cg->isMonitorValueBasedOrValueType(node);

   if ((isMonitorValueBasedOrValueType == TR_yes) ||
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

   if (lwOffset <=0)
      {
      numDeps +=2;
      if (comp->getOption(TR_EnableMonitorCacheLookup))
         {
         numDeps +=2; // extra one for lit pool reg in disableZ9 mode
         }
      }

   if (comp->getOptions()->enableDebugCounters())
      numDeps += 5;
   bool simpleLocking = false;
   bool reserveLocking = false, normalLockWithReservationPreserving = false;

   if (isMonitorValueBasedOrValueType == TR_maybe)
      {
      numDeps += 1;
      // If we are generating code for MonitorCacheLookup then we will not have a separate OOL for inlineRecursive, and callLabel points
      // to the OOL Containing only helper call. Otherwise, OOL will have other code apart from helper call which we do not want to execute
      // for ValueType or ValueBased object and in that scenario we will need to generate another OOL that just contains helper call.
      objectClassReg = generateCheckForValueMonitorEnterOrExit(node, cFlowRegionEnd, lwOffset <= 0 ? callLabel : NULL, cg);
      }
   TR::RegisterDependencyConditions * conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, numDeps, cg);

   TR_Debug * debugObj = cg->getDebug();


   conditions->addPostCondition(objReg, TR::RealRegister::AssignAny);
   conditions->addPostCondition(monitorReg, TR::RealRegister::AssignAny);
   if (objectClassReg != NULL)
      conditions->addPostCondition(objectClassReg, TR::RealRegister::AssignAny);

   static const char * peekFirst = feGetEnv("TR_PeekingMonEnter");
   // This debug option is for printing the locking mechanism.
   static int printMethodSignature = feGetEnv("PrintMethodSignatureForLockResEnt")? 1 : 0;
   if (lwOffset <= 0)
      {
      inlineRecursive = false;
      // should not happen often, only on a subset of objects that don't have a lockword
      // set with option -Xlockword

      TR::LabelSymbol               *helperCallLabel = generateLabelSymbol(cg);
      TR::LabelSymbol               *helperReturnOOLLabel = generateLabelSymbol(cg);
      TR::MemoryReference * tempMR = NULL;
      if (objectClassReg == NULL)
         {
         tempMR = generateS390MemoryReference(objReg, TR::Compiler->om.offsetOfObjectVftField(), cg);
         // TODO We don't need objectClassReg except in this ifCase. We can use scratchRegisterManager to allocate one here.
         objectClassReg = cg->allocateRegister();
         conditions->addPostCondition(objectClassReg, TR::RealRegister::AssignAny);
         TR::TreeEvaluator::genLoadForObjectHeadersMasked(cg, node, objectClassReg, tempMR, NULL);
         }
      int32_t offsetOfLockOffset = offsetof(J9Class, lockOffset);
      tempMR = generateS390MemoryReference(objectClassReg, offsetOfLockOffset, cg);

      tempRegister = cg->allocateRegister();
      TR::LabelSymbol *targetLabel = callLabel;
      if (comp->getOption(TR_EnableMonitorCacheLookup))
         targetLabel = monitorLookupCacheLabel;

      generateRXInstruction(cg, TR::InstOpCode::getLoadTestOpCode(), node, tempRegister, tempMR);

      TR::Instruction *cmpInstr = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, targetLabel);

      if(cg->comp()->target().is64Bit())
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

         TR::Instruction *cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, monitorLookupCacheLabel);

         if (debugObj)
            {
            debugObj->addInstructionComment(cmpInstr, "Branch to OOL monent monitorLookupCache");
            debugObj->addInstructionComment(cursor, "Denotes start of OOL monent monitorLookupCache");
            }

         lookupOffsetReg = cg->allocateRegister();
         OOLConditions->addPostCondition(lookupOffsetReg, TR::RealRegister::AssignAny);

         int32_t offsetOfMonitorLookupCache = offsetof(J9VMThread, objectMonitorLookupCache);
         int32_t t = trailingZeroes(TR::Compiler->om.getObjectAlignmentInBytes());
         int32_t shiftAmount = trailingZeroes((int32_t) TR::Compiler->om.sizeofReferenceField()) - t;
         int32_t end = 63 - trailingZeroes((int32_t) TR::Compiler->om.sizeofReferenceField());
         int32_t start = end - trailingZeroes(J9VMTHREAD_OBJECT_MONITOR_CACHE_SIZE) + 1;

         if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_S390_ZEC12) && cg->comp()->target().is64Bit())
            generateRIEInstruction(cg, TR::InstOpCode::RISBGN, node, lookupOffsetReg, objReg, start, end+0x80, shiftAmount);
         else if(cg->comp()->target().is64Bit())
            generateRIEInstruction(cg, TR::InstOpCode::RISBG, node, lookupOffsetReg, objReg, start, end+0x80, shiftAmount);
         else
            {
            generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, lookupOffsetReg, objReg);

            if (cg->comp()->target().is64Bit())
               generateRSInstruction(cg, TR::InstOpCode::SRAG, node, lookupOffsetReg, lookupOffsetReg, t);
            else
               generateRSInstruction(cg, TR::InstOpCode::SRA, node, lookupOffsetReg, t);

            J9JavaVM * jvm = fej9->getJ9JITConfig()->javaVM;

            if (cg->comp()->target().is32Bit())
               generateS390ImmOp(cg, TR::InstOpCode::getAndOpCode(), node, lookupOffsetReg, lookupOffsetReg, (int32_t) J9VMTHREAD_OBJECT_MONITOR_CACHE_SIZE - 1, OOLConditions, 0);
            else
               generateS390ImmOp(cg, TR::InstOpCode::getAndOpCode(), node, lookupOffsetReg, lookupOffsetReg, (int64_t) J9VMTHREAD_OBJECT_MONITOR_CACHE_SIZE - 1, OOLConditions, 0);

            if (cg->comp()->target().is64Bit())
               generateRSInstruction(cg, TR::InstOpCode::SLLG, node, lookupOffsetReg, lookupOffsetReg, trailingZeroes((int32_t) TR::Compiler->om.sizeofReferenceField()));
            else
               generateRSInstruction(cg, TR::InstOpCode::SLL, node, lookupOffsetReg, trailingZeroes((int32_t) TR::Compiler->om.sizeofReferenceField()));
            }

         TR::MemoryReference * temp2MR = generateS390MemoryReference(cg->getMethodMetaDataRealRegister(), lookupOffsetReg, offsetOfMonitorLookupCache, cg);

         if (TR::Compiler->om.compressObjectReferences())
            {
            generateRXInstruction(cg, TR::InstOpCode::LLGF, node, tempRegister, temp2MR, NULL);
            startICF = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, tempRegister, NULLVALUE, TR::InstOpCode::COND_BE, helperCallLabel, false, true);
            }
         else
            {
            generateRXInstruction(cg, TR::InstOpCode::getLoadTestOpCode(), node, tempRegister, temp2MR);
            startICF = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, helperCallLabel);
            }

         int32_t offsetOfMonitor = offsetof(J9ObjectMonitor, monitor);
         temp2MR = generateS390MemoryReference(tempRegister, offsetOfMonitor, cg);
         generateRXInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, objReg, temp2MR);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, helperCallLabel);

         int32_t offsetOfAlternateLockWord = offsetof(J9ObjectMonitor, alternateLockword);

         baseReg = tempRegister;
         lwOffset = 0 + offsetOfAlternateLockWord;

         if (cg->comp()->target().is64Bit() && fej9->generateCompressedLockWord())
            generateRRInstruction(cg, TR::InstOpCode::XR, node, monitorReg, monitorReg);
         else
            generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, monitorReg, monitorReg);

         if (peekFirst)
            {
            generateRXInstruction(cg, TR::InstOpCode::C, node, monitorReg, generateS390MemoryReference(baseReg, lwOffset, cg));
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, helperCallLabel);
            }

         if (cg->comp()->target().is64Bit() && fej9->generateCompressedLockWord())
            generateRSInstruction(cg, TR::InstOpCode::CS, node, monitorReg, metaReg,
                                  generateS390MemoryReference(baseReg, lwOffset, cg));
         else
            generateRSInstruction(cg, TR::InstOpCode::getCmpAndSwapOpCode(), node, monitorReg, metaReg,
                                  generateS390MemoryReference(baseReg, lwOffset, cg));

#if (JAVA_SPEC_VERSION >= 19)
#if defined(TR_TARGET_64BIT)
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, helperCallLabel);
         generateSIInstruction(cg, TR::InstOpCode::AGSI, node, generateS390MemoryReference(cg->getMethodMetaDataRealRegister(), fej9->thisThreadGetOwnedMonitorCountOffset(), cg), 1);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, helperReturnOOLLabel);
#else    /* TR_TARGET_64BIT */
         TR_ASSERT_FATAL(false, "Virtual Thread is not supported on 31-Bit platform\n");
#endif   /* TR_TARGET_64BIT */
#else    /* JAVA_SPEC_VERSION >= 19 */
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, helperReturnOOLLabel);
#endif   /* JAVA_SPEC_VERSION >= 19 */

         generateS390LabelInstruction(cg, TR::InstOpCode::label, node, helperCallLabel );
         TR::RegisterDependencyConditions *deps = NULL;
         dummyResultReg = helperLink->buildDirectDispatch(node, &deps);
         TR::RegisterDependencyConditions *mergeConditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(OOLConditions, deps, cg);
         generateS390LabelInstruction(cg, TR::InstOpCode::label, node, helperReturnOOLLabel , mergeConditions);

         cursor = generateS390BranchInstruction(cg,TR::InstOpCode::BRC,TR::InstOpCode::COND_BRC,node,cFlowRegionEnd);
         if (debugObj)
            debugObj->addInstructionComment(cursor, "Denotes end of OOL monent monitorCacheLookup: return to mainline");

         // Done using OOL with manual code generation
         monitorCacheLookupOOL->swapInstructionListsWithCompilation();
      }

      simpleLocking = true;
      lwOffset = 0;
      baseReg = tempRegister;
   }

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
   if (cg->comp()->target().is64Bit() && fej9->generateCompressedLockWord())
      use64b = false;
   else if (!cg->comp()->target().is64Bit())
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

#if (JAVA_SPEC_VERSION >= 19)
#if defined(TR_TARGET_64BIT)
   generateSIInstruction(cg, TR::InstOpCode::AGSI, node, generateS390MemoryReference(cg->getMethodMetaDataRealRegister(), fej9->thisThreadGetOwnedMonitorCountOffset(), cg), 1);
#else    /* TR_TARGET_64BIT */
   TR_ASSERT_FATAL(false, "Virtual Thread is not supported on 31-Bit platform\n");
#endif   /* TR_TARGET_64BIT */
#endif   /* JAVA_SPEC_VERSION >= 19 */

   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "%s/CSSuccessfull", debugCounterNamePrefix), 1, TR::DebugCounter::Undetermined);
   TR_S390OutOfLineCodeSection *outlinedHelperCall = NULL;
   TR::Instruction *cursor;
   TR::LabelSymbol *returnLabel = generateLabelSymbol(cg);

   outlinedHelperCall = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(callLabel, cFlowRegionEnd, cg);
   cg->getS390OutOfLineCodeSectionList().push_front(outlinedHelperCall);
   outlinedHelperCall->swapInstructionListsWithCompilation();
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, callLabel);
   if (debugObj)
      debugObj->addInstructionComment(cursor, "Denotes start of OOL monent sequence");

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

#if (JAVA_SPEC_VERSION >= 19)
#if defined(TR_TARGET_64BIT)
      generateSIInstruction(cg, TR::InstOpCode::AGSI, node, generateS390MemoryReference(cg->getMethodMetaDataRealRegister(), fej9->thisThreadGetOwnedMonitorCountOffset(), cg), 1);
#else    /* TR_TARGET_64BIT */
      TR_ASSERT_FATAL(false, "Virtual Thread is not supported on 31-Bit platform\n");
#endif   /* TR_TARGET_64BIT */
#endif   /* JAVA_SPEC_VERSION >= 19 */

      generateS390BranchInstruction(cg,TR::InstOpCode::BRC,TR::InstOpCode::COND_BRC,node,returnLabel);

      tempMR->stopUsingMemRefRegister(cg);
      tempMR1->stopUsingMemRefRegister(cg);

      // Helper Call
      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, callHelper);
      }

   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "%s/VMHelper", debugCounterNamePrefix), 1, TR::DebugCounter::Undetermined);
   TR::RegisterDependencyConditions *deps = NULL;
   dummyResultReg = inlineRecursive ? helperLink->buildDirectDispatch(node, &deps) : helperLink->buildDirectDispatch(node);
   TR::RegisterDependencyConditions *mergeConditions = NULL;
   if (inlineRecursive)
      mergeConditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(conditions, deps, cg);
   else
      mergeConditions = conditions;
   generateS390LabelInstruction(cg,TR::InstOpCode::label,node,returnLabel,mergeConditions);

   // End of OOl path.
   cursor = generateS390BranchInstruction(cg,TR::InstOpCode::BRC,TR::InstOpCode::COND_BRC,node,cFlowRegionEnd);
   if (debugObj)
      {
      debugObj->addInstructionComment(cursor, "Denotes end of OOL monent: return to mainline");
      }

   // Done using OOL with manual code generation
   outlinedHelperCall->swapInstructionListsWithCompilation();

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionEnd, conditions);

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
   J9::Z::CHelperLinkage *helperLink =  static_cast<J9::Z::CHelperLinkage*>(cg->getLinkage(TR_CHelper));
   TR_YesNoMaybe isMonitorValueBasedOrValueType = cg->isMonitorValueBasedOrValueType(node);

   if ((isMonitorValueBasedOrValueType == TR_yes) ||
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

   static char * disableInlineRecursiveMonitor = feGetEnv("TR_DisableInlineRecursiveMonitor");
   bool inlineRecursive = true;
   if (disableInlineRecursiveMonitor)
     inlineRecursive = false;

   TR::LabelSymbol *callLabel                      = generateLabelSymbol(cg);
   TR::LabelSymbol *monitorLookupCacheLabel        = generateLabelSymbol(cg);
   TR::LabelSymbol *cFlowRegionEnd                 = generateLabelSymbol(cg);
   TR::LabelSymbol *callHelper                     = generateLabelSymbol(cg);
   TR::LabelSymbol *returnLabel                    = generateLabelSymbol(cg);

   int32_t numDeps = 4;
   if (lwOffset <=0)
      {
      numDeps +=2;
      if (comp->getOption(TR_EnableMonitorCacheLookup))
         {
         numDeps +=2; // extra one for lit pool reg in disableZ9 mode
         }
      }

   if (comp->getOptions()->enableDebugCounters())
         numDeps += 4;

   if (isMonitorValueBasedOrValueType == TR_maybe)
      {
      numDeps += 1;
      objectClassReg = generateCheckForValueMonitorEnterOrExit(node, cFlowRegionEnd, lwOffset <= 0 ? callLabel : NULL, cg);
      }
   TR::RegisterDependencyConditions * conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, numDeps, cg);


   TR::Instruction * gcPoint;
   TR_Debug * debugObj = cg->getDebug();

   bool reserveLocking                          = false;
   bool normalLockWithReservationPreserving     = false;
   bool simpleLocking                           = false;


   conditions->addPostCondition(objReg, TR::RealRegister::AssignAny);
   conditions->addPostCondition(monitorReg, TR::RealRegister::AssignAny);
   if (objectClassReg != NULL)
      conditions->addPostCondition(objectClassReg, TR::RealRegister::AssignAny);


   if (lwOffset <= 0)
      {
      inlineRecursive = false; // should not happen often, only on a subset of objects that don't have a lockword, set with option -Xlockword

      TR::LabelSymbol *helperCallLabel          = generateLabelSymbol(cg);
      TR::LabelSymbol *helperReturnOOLLabel     = generateLabelSymbol(cg);
      TR::MemoryReference *tempMR = NULL;

      if (objectClassReg == NULL)
         {
         tempMR = generateS390MemoryReference(objReg, TR::Compiler->om.offsetOfObjectVftField(), cg);
         objectClassReg = cg->allocateRegister();
         conditions->addPostCondition(objectClassReg, TR::RealRegister::AssignAny);
         TR::TreeEvaluator::genLoadForObjectHeadersMasked(cg, node, objectClassReg, tempMR, NULL);
         }
      int32_t offsetOfLockOffset = offsetof(J9Class, lockOffset);
      tempMR = generateS390MemoryReference(objectClassReg, offsetOfLockOffset, cg);

      tempRegister = cg->allocateRegister();
      TR::LabelSymbol *targetLabel = callLabel;
      if (comp->getOption(TR_EnableMonitorCacheLookup))
         targetLabel = monitorLookupCacheLabel;

      generateRXInstruction(cg, TR::InstOpCode::getLoadTestOpCode(), node, tempRegister, tempMR);

      TR::Instruction *cmpInstr = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, targetLabel);

      if(comp->target().is64Bit())
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

         TR::Instruction *cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, monitorLookupCacheLabel);

         if (debugObj)
            {
            debugObj->addInstructionComment(cmpInstr, "Branch to OOL monexit monitorLookupCache");
            debugObj->addInstructionComment(cursor, "Denotes start of OOL monexit monitorLookupCache");
            }


         int32_t offsetOfMonitorLookupCache = offsetof(J9VMThread, objectMonitorLookupCache);
         int32_t t = trailingZeroes(TR::Compiler->om.getObjectAlignmentInBytes());
         int32_t shiftAmount = trailingZeroes((int32_t) TR::Compiler->om.sizeofReferenceField()) - t;
         int32_t end = 63 - trailingZeroes((int32_t) TR::Compiler->om.sizeofReferenceField());
         int32_t start = end - trailingZeroes(J9VMTHREAD_OBJECT_MONITOR_CACHE_SIZE) + 1;

         if (comp->target().cpu.isAtLeast(OMR_PROCESSOR_S390_ZEC12) && comp->target().is64Bit())
            generateRIEInstruction(cg, TR::InstOpCode::RISBGN, node, lookupOffsetReg, objReg, start, end+0x80, shiftAmount);
         else if(comp->target().is64Bit())
            generateRIEInstruction(cg, TR::InstOpCode::RISBG, node, lookupOffsetReg, objReg, start, end+0x80, shiftAmount);
         else
            {
            generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, lookupOffsetReg, objReg);

            if (comp->target().is64Bit())
               generateRSInstruction(cg, TR::InstOpCode::SRAG, node, lookupOffsetReg, lookupOffsetReg, t);
            else
               generateRSInstruction(cg, TR::InstOpCode::SRA, node, lookupOffsetReg, t);

            J9JavaVM * jvm = fej9->getJ9JITConfig()->javaVM;

            if (comp->target().is32Bit())
               generateS390ImmOp(cg, TR::InstOpCode::getAndOpCode(), node, lookupOffsetReg, lookupOffsetReg, (int32_t) J9VMTHREAD_OBJECT_MONITOR_CACHE_SIZE - 1, OOLConditions, 0);
            else
               generateS390ImmOp(cg, TR::InstOpCode::getAndOpCode(), node, lookupOffsetReg, lookupOffsetReg, (int64_t) J9VMTHREAD_OBJECT_MONITOR_CACHE_SIZE - 1, OOLConditions, 0);

            if (comp->target().is64Bit())
               generateRSInstruction(cg, TR::InstOpCode::SLLG, node, lookupOffsetReg, lookupOffsetReg, trailingZeroes((int32_t) TR::Compiler->om.sizeofReferenceField()));
            else
               generateRSInstruction(cg, TR::InstOpCode::SLL, node, lookupOffsetReg, trailingZeroes((int32_t) TR::Compiler->om.sizeofReferenceField()));
            }

         // TODO No Need to use Memory Reference Here. Combine it with generateRXInstruction
         TR::MemoryReference * temp2MR = generateS390MemoryReference(cg->getMethodMetaDataRealRegister(), lookupOffsetReg, offsetOfMonitorLookupCache, cg);

         if (TR::Compiler->om.compressObjectReferences())
            {
            generateRXInstruction(cg, TR::InstOpCode::LLGF, node, tempRegister, temp2MR, NULL);
            startICF = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, tempRegister, NULLVALUE, TR::InstOpCode::COND_BE, helperCallLabel, false, true);
            }
         else
            {
            generateRXInstruction(cg, TR::InstOpCode::getLoadTestOpCode(), node, tempRegister, temp2MR);
            startICF = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, helperCallLabel);
            }

         int32_t offsetOfMonitor = offsetof(J9ObjectMonitor, monitor);
         // TODO No Need to use Memory Reference Here. Combine it with generateRXInstruction
         temp2MR = generateS390MemoryReference(tempRegister, offsetOfMonitor, cg);
         generateRXInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, objReg, temp2MR);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, helperCallLabel);

         int32_t offsetOfAlternateLockWord = offsetof(J9ObjectMonitor, alternateLockword);

         baseReg = tempRegister;
         lwOffset = 0 + offsetOfAlternateLockWord;

         // Check if the lockWord in the object contains our VMThread
         if (comp->target().is64Bit() && fej9->generateCompressedLockWord())
            generateRXInstruction(cg, TR::InstOpCode::C, node, metaReg, generateS390MemoryReference(baseReg, lwOffset, cg));
         else
            generateRXInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, metaReg, generateS390MemoryReference(baseReg, lwOffset, cg));

         // If VMThread does not match, call helper.
         TR::Instruction* helperBranch = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, helperCallLabel);

         // If VMThread matches, we can safely perform the monitor exit by zero'ing
         // out the lockWord on the object
         if (comp->target().is64Bit() && fej9->generateCompressedLockWord())
            gcPoint = generateSILInstruction(cg, TR::InstOpCode::MVHI, node, generateS390MemoryReference(baseReg, lwOffset, cg), 0);
         else
            gcPoint = generateSILInstruction(cg, TR::InstOpCode::getMoveHalfWordImmOpCode(), node, generateS390MemoryReference(baseReg, lwOffset, cg), 0);

#if (JAVA_SPEC_VERSION >= 19)
#if defined(TR_TARGET_64BIT)
         generateSIInstruction(cg, TR::InstOpCode::AGSI, node, generateS390MemoryReference(cg->getMethodMetaDataRealRegister(), fej9->thisThreadGetOwnedMonitorCountOffset(), cg), -1);
#else    /* TR_TARGET_64BIT */
         TR_ASSERT_FATAL(false, "Virtual Thread is not supported on 31-Bit platform\n");
#endif   /* TR_TARGET_64BIT */
#endif   /* JAVA_SPEC_VERSION >= 19 */

         generateS390BranchInstruction(cg,TR::InstOpCode::BRC,TR::InstOpCode::COND_BRC,node,helperReturnOOLLabel);

         generateS390LabelInstruction(cg, TR::InstOpCode::label , node, helperCallLabel );
         TR::RegisterDependencyConditions *deps = NULL;
         helperLink->buildDirectDispatch(node, &deps);
         TR::RegisterDependencyConditions *mergeConditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(OOLConditions, deps, cg);
         generateS390LabelInstruction(cg, TR::InstOpCode::label, node, helperReturnOOLLabel , mergeConditions);

         cursor = generateS390BranchInstruction(cg,TR::InstOpCode::BRC,TR::InstOpCode::COND_BRC,node,cFlowRegionEnd);
         if (debugObj)
            debugObj->addInstructionComment(cursor, "Denotes end of OOL monexit monitorCacheLookup: return to mainline");

         // Done using OOL with manual code generation
         monitorCacheLookupOOL->swapInstructionListsWithCompilation();
         }

      lwOffset = 0;
      baseReg = tempRegister;
      simpleLocking = true;
      }

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
   if (comp->target().is64Bit() && fej9->generateCompressedLockWord())
      use64b = false;
   else if (!comp->target().is64Bit())
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
   generateSILInstruction(cg, moveImmOp, node, generateS390MemoryReference(baseReg, lwOffset, cg), 0);

#if (JAVA_SPEC_VERSION >= 19)
#if defined(TR_TARGET_64BIT)
   generateSIInstruction(cg, TR::InstOpCode::AGSI, node, generateS390MemoryReference(cg->getMethodMetaDataRealRegister(), fej9->thisThreadGetOwnedMonitorCountOffset(), cg), -1);
#else    /* TR_TARGET_64BIT */
   TR_ASSERT_FATAL(false, "Virtual Thread is not supported on 31-Bit platform\n");
#endif   /* TR_TARGET_64BIT */
#endif   /* JAVA_SPEC_VERSION >= 19 */

   TR_S390OutOfLineCodeSection *outlinedHelperCall = NULL;

   outlinedHelperCall = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(callLabel,cFlowRegionEnd,cg);
   cg->getS390OutOfLineCodeSectionList().push_front(outlinedHelperCall);
   outlinedHelperCall->swapInstructionListsWithCompilation();

   TR::Instruction *cursor = generateS390LabelInstruction(cg,TR::InstOpCode::label,node,callLabel);

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

#if (JAVA_SPEC_VERSION >= 19)
#if defined(TR_TARGET_64BIT)
      generateSIInstruction(cg, TR::InstOpCode::AGSI, node, generateS390MemoryReference(cg->getMethodMetaDataRealRegister(), fej9->thisThreadGetOwnedMonitorCountOffset(), cg), -1);
#else    /* TR_TARGET_64BIT */
      TR_ASSERT_FATAL(false, "Virtual Thread is not supported on 31-Bit platform\n");
#endif   /* TR_TARGET_64BIT */
#endif   /* JAVA_SPEC_VERSION >= 19 */

      generateS390BranchInstruction(cg,TR::InstOpCode::BRC,TR::InstOpCode::COND_BRC,node,returnLabel);
      tempMR->stopUsingMemRefRegister(cg);
      tempMR1->stopUsingMemRefRegister(cg);

      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, callHelper);
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

   generateS390LabelInstruction(cg,TR::InstOpCode::label,node,returnLabel,mergeConditions);

   cursor = generateS390BranchInstruction(cg,TR::InstOpCode::BRC,TR::InstOpCode::COND_BRC,node,cFlowRegionEnd);
   if (debugObj)
      {
      debugObj->addInstructionComment(cursor, "Denotes end of OOL monexit: return to mainline");
      }
   // Done using OOL with manual code generation
   outlinedHelperCall->swapInstructionListsWithCompilation();

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionEnd, conditions);

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
   int32_t alignmentConstant = TR::Compiler->om.getObjectAlignmentInBytes();
   if (exitOOLLabel)
      {
      TR_Debug * debugObj = cg->getDebug();
      iCursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, exitOOLLabel);
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
      if (cg->comp()->target().is64Bit())
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
   if (!comp->getOptions()->realTimeGC())
      {
      TR::Register *metaReg = cg->getMethodMetaDataRealRegister();

      // bool sizeInReg = (isVariableLen || (allocSize > MAX_IMMEDIATE_VAL));

      int alignmentConstant = TR::Compiler->om.getObjectAlignmentInBytes();

      if (isVariableLen)
         {
         if (exitOOLLabel)
            {
            TR_Debug * debugObj = cg->getDebug();
            iCursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, exitOOLLabel);
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

         if (comp->target().cpu.isAtLeast(OMR_PROCESSOR_S390_Z196))
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
         // All arrays in combo builds will always be at least 12 bytes in size in all specs if
         // dual header shape is enabled and 20 bytes otherwise:
         //
         // Dual header shape is enabled:
         // 1)  class pointer + contig length + one or more elements
         // 2)  class pointer + 0 + 0 (for zero length arrays)
         //
         // Dual header shape is disabled:
         // 1)  class pointer + contig length + dataAddr + one or more elements
         // 2)  class pointer + 0 + 0 (for zero length arrays) + dataAddr
         //
         // Since objects are aligned to 8 bytes then the minimum size for an array must be 16 and 24 after rounding
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
         if (comp->target().is64Bit())
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
         iCursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, fillerLoopLabel);
         iCursor = generateSS1Instruction(cg, TR::InstOpCode::XC, node, 255, generateS390MemoryReference(addressReg, 0, cg), generateS390MemoryReference(addressReg, 0, cg), iCursor);

         iCursor = generateRXInstruction(cg, TR::InstOpCode::LA, node, addressReg, generateS390MemoryReference(addressReg, 256, cg), iCursor);

         iCursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRCT, node, shiftReg, fillerLoopLabel);
         iCursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, fillerRemLabel);

         // and to only get the right 8 bits (remainder)
         iCursor = generateRIInstruction(cg, TR::InstOpCode::NILL, node, lengthReg, 0x00FF);
         iCursor = generateRIInstruction(cg, TR::InstOpCode::AHI, node, lengthReg, -1);
         // branch to done if length < 0

         iCursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, node, doneLabel, iCursor);

         iCursor = generateSS1Instruction(cg, TR::InstOpCode::XC, node, 0, generateS390MemoryReference(addressReg, 0, cg), generateS390MemoryReference(addressReg, 0, cg), iCursor);

         // minus 1 from lengthreg since xc auto adds 1 to it

         iCursor = generateEXDispatch(node, cg, lengthReg, shiftReg, iCursor);
         iCursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, doneLabel);
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
   if (!comp->getOptions()->realTimeGC())
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
            iCursor = genLoadAddressConstant(cg, node, (intptr_t) classAddress | (intptr_t)orFlag, temp1Reg, iCursor, conditions, litPoolBaseReg);

         if (canUseIIHF)
            {
            iCursor = generateRILInstruction(cg, TR::InstOpCode::IIHF, node, enumReg, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(classAddress)) | orFlag, iCursor);
            }
         else
            {
            if (TR::Compiler->om.compressObjectReferences())
               // must store just 32 bits (class offset)

               iCursor = generateRXInstruction(cg, TR::InstOpCode::ST, node, temp1Reg,
                     generateS390MemoryReference(resReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg), iCursor);
            else
               iCursor = generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, temp1Reg,
                     generateS390MemoryReference(resReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg), iCursor);
            }
         }
      else
         {
         if (TR::Compiler->om.compressObjectReferences())
            // must store just 32 bits (class offset)
            iCursor = generateRXInstruction(cg, TR::InstOpCode::ST, node, clzReg,
                  generateS390MemoryReference(resReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg), iCursor);
         else
            iCursor = generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, clzReg,
                  generateS390MemoryReference(resReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), cg), iCursor);
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
            if (staticFlag >= MIN_IMMEDIATE_VAL && staticFlag <= MAX_IMMEDIATE_VAL)
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
      if (cg->comp()->target().is64Bit())
         {
         iCursor = generateRRInstruction(cg, TR::InstOpCode::LGFR, node, dataSizeReg, dataSizeReg, iCursor);
         }

      iCursor = generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, temp2Reg,
            generateS390MemoryReference(resReg, dataSizeReg, dataBegin, cg), iCursor);
      }
   else if (objectSize >= MAXDISP)
      {
      iCursor = genLoadAddressConstant(cg, node, (intptr_t) objectSize, temp1Reg, iCursor, conditions);
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
   iCursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, slotAtStart, iCursor);
   iCursor = generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, temp2Reg,
                generateS390MemoryReference(resReg, (int32_t) 0, cg), iCursor);
   iCursor = generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, resReg, 4, iCursor);
   iCursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, doneAlign, iCursor);
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
         && (TR::Compiler->om.compressObjectReferences() || comp->target().is32Bit())
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
   TR_J9VMBase *fej9 = comp->fej9();


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

   bool generateArraylets = comp->generateArraylets();

   // in time, the tlh will probably always be batch cleared, and therefore it will not be
   // necessary for the JIT-generated inline code to do the clearing of fields. But, 2 things
   // have to happen first:
   // 1.The JVM has to change it's code so that it has batch clearing on for 390 (it is currently only
   //   on if turned on as a runtime option)
   // 2.The JVM has to support the call - on z/OS, Modron GC is not enabled yet and so batch tlh clearing
   //   can not be enabled yet.
   bool needZeroReg = !fej9->tlhHasBeenCleared();

   opCode = node->getOpCodeValue();

   // Since calls to canInlineAllocate could result in different results during the same compilation,
   // We must be conservative and only do inline allocation if the first call (in LocalOpts.cpp) has succeeded and we have the litPoolBaseChild added.
   // Refer to defects 161084 and 87089
   if (cg->doInlineAllocate(node)
            && performTransformation(comp, "O^O Inlining Allocation of %s [0x%p].\n", node->getOpCode().getName(), node))
      {
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
         dataBegin = TR::Compiler->om.objectHeaderSizeInBytes();
         }
      else
         {
         isArray = true;
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
      ///============================ STAGE 1: Setup Register Dependencies===============================///
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
      ///============================ STAGE 2: Calculate Allocation Size ================================///
      //////////////////////////////////////////////////////////////////////////////////////////////////////
      // Three possible outputs:
      // if variable-length array   - dataSizeReg will contain the (calculated) size
      // if outlined                - tmpReg will contain the value of
      // otherwise                  - size is in (int) allocateSize
      int alignmentConstant = TR::Compiler->om.getObjectAlignmentInBytes();

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
            tmp = temp1Reg;
            }
         else
            {
            tmp = dataSizeReg;
            }

         /*     if (elementSize >= 2)
          {
          if (comp->target().is64Bit())
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
            if (comp->target().is64Bit())
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
                  if (comp->target().is64Bit())
                  iCursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, tmp, tmp, trailingZeroes(elementSize), iCursor);
                  else
                  iCursor = generateRSInstruction(cg, TR::InstOpCode::SLL, node, tmp, trailingZeroes(elementSize), iCursor);
                  }
               }

            static char * allocZeroArrayWithVM = feGetEnv("TR_VMALLOCZEROARRAY");
            // DualTLH: Remove when performance confirmed
            static char * useDualTLH = feGetEnv("TR_USEDUALTLH");

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
               cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, startOOLLabel);
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
            iCursor = generateRRInstruction(cg, TR::InstOpCode::getLoadRegWidenOpCode(), node, tmp, enumReg, iCursor);

            if (elementSize >= 2)
               {
               if (comp->target().is64Bit())
               iCursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, tmp, tmp, trailingZeroes(elementSize), iCursor);
               else
               iCursor = generateRSInstruction(cg, TR::InstOpCode::SLL, node, tmp, trailingZeroes(elementSize), iCursor);
               }
            }

         }

      //////////////////////////////////////////////////////////////////////////////////////////////////////
      ///============================ STAGE 3: Generate HeapTop Test=====================================///
      //////////////////////////////////////////////////////////////////////////////////////////////////////
      TR::Instruction *current;
      TR::Instruction *firstInstruction;
      srm->addScratchRegistersToDependencyList(conditions);

      current = cg->getAppendInstruction();

      TR_ASSERT(current != NULL, "Could not get current instruction");

      static char * useDualTLH = feGetEnv("TR_USEDUALTLH");
      //Here we set up backout paths if we overflow nonZeroTLH in genHeapAlloc.
      //If we overflow the nonZeroTLH, set the destination to the right VM runtime helper (eg jitNewObjectNoZeroInit, etc...)
      //The zeroed-TLH versions have their correct destinations already setup in TR_ByteCodeIlGenerator::genNew, TR_ByteCodeIlGenerator::genNewArray, TR_ByteCodeIlGenerator::genANewArray
      //To retrieve the destination node->getSymbolReference() is used below after genHeapAlloc.
      if(!comp->getOption(TR_DisableDualTLH) && useDualTLH && node->canSkipZeroInitialization())
         {
         // For value types, the backout path should call jitNewValue helper call which is set up before code gen
         if ((node->getOpCodeValue() == TR::New)
             && (!TR::Compiler->om.areValueTypesEnabled() || (node->getSymbolReference() != comp->getSymRefTab()->findOrCreateNewValueSymbolRef(comp->getMethodSymbol()))))
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

      //////////////////////////////////////////////////////////////////////////////////////////////////////
      ///============================ STAGE 4: Generate Fall-back Path ==================================///
      //////////////////////////////////////////////////////////////////////////////////////////////////////
         /* New Evaluator Optimization: Using OOL instead of snippet for heap alloc */

         /* Example of the OOL for newarray
          * Outlined Label L0048:    ; Denotes start of OOL for heap alloc
          * LHI     GPR_0120,0x5
          * assocreg
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
      cursorHeapAlloc = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, callLabel);
      if (debugObj)
         debugObj->addInstructionComment(cursorHeapAlloc, "Denotes start of OOL for heap alloc");
      generateHelperCallForVMNewEvaluators(node, cg, true, resReg);
      /* Copying the return value from the temporary register to the actual register that is returned */
      /* Generating the branch to jump back to the merge label:
       * BRCL    J(0xf), Label L00YZ, labelTargetAddr=0xZZZZZZZZ*/
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cFlowRegionEnd);
      heapAllocOOL->swapInstructionListsWithCompilation();
      //////////////////////////////////////////////////////////////////////////////////////////////////////
      ///============================ STAGE 5: Initialize the new object header ==========================///
      //////////////////////////////////////////////////////////////////////////////////////////////////////
      if (isArray)
         {
         if ( comp->compileRelocatableCode() && opCode == TR::anewarray)
         genInitArrayHeader(node, iCursor, isVariableLen, classAddress, classReg, resReg, zeroReg,
               enumReg, dataSizeReg, temp1Reg, litPoolBaseReg, conditions, cg);
         else
         genInitArrayHeader(node, iCursor, isVariableLen, classAddress, NULL, resReg, zeroReg,
               enumReg, dataSizeReg, temp1Reg, litPoolBaseReg, conditions, cg);

#ifdef TR_TARGET_64BIT
         if (TR::Compiler->om.isIndexableDataAddrPresent())
            {
            /* Here we'll update dataAddr slot for both fixed and variable length arrays. Fixed length arrays are
             * simple as we just need to check first child of the node for array size. For variable length arrays
             * runtime size checks are needed to determine whether to use contiguous or discontiguous header layout.
             *
             * In both scenarios, arrays of non-zero size use contiguous header layout while zero size arrays use
             * discontiguous header layout.
             */
            TR::Register *offsetReg = NULL;
            TR::MemoryReference *dataAddrMR = NULL;
            TR::MemoryReference *dataAddrSlotMR = NULL;

            if (isVariableLen && TR::Compiler->om.compressObjectReferences())
               {
               /* We need to check enumReg (array size) at runtime to determine correct offset of dataAddr field.
                * Here we deal only with compressed refs because dataAddr offset for discontiguous
                * and contiguous arrays is the same in full refs.
                */
               if (comp->getOption(TR_TraceCG))
                  traceMsg(comp, "Node (%p): Dealing with compressed refs variable length array.\n", node);

               TR_ASSERT_FATAL_WITH_NODE(node,
                  (fej9->getOffsetOfDiscontiguousDataAddrField() - fej9->getOffsetOfContiguousDataAddrField()) == 8,
                  "Offset of dataAddr field in discontiguous array is expected to be 8 bytes more than contiguous array. "
                  "But was %d bytes for discontiguous and %d bytes for contiguous array.\n",
                  fej9->getOffsetOfDiscontiguousDataAddrField(), fej9->getOffsetOfContiguousDataAddrField());

               offsetReg = cg->allocateRegister();
               // Invert enumReg sign. 0 and negative numbers remain unchanged.
               iCursor = generateRREInstruction(cg, TR::InstOpCode::LNGFR, node, offsetReg, enumReg, iCursor);
               iCursor = generateRSInstruction(cg, TR::InstOpCode::SRLG, node, temp1Reg, offsetReg, 63, iCursor);
               iCursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, offsetReg, temp1Reg, 3, iCursor);
               // Inverting the sign bit will leave us with either -8 (if enumCopyReg > 0) or 0 (if enumCopyReg == 0).
               iCursor = generateRREInstruction(cg, TR::InstOpCode::LNGR, node, offsetReg, offsetReg, iCursor);

               dataAddrMR = generateS390MemoryReference(resReg, offsetReg, TR::Compiler->om.discontiguousArrayHeaderSizeInBytes(), cg);
               dataAddrSlotMR = generateS390MemoryReference(resReg, offsetReg, fej9->getOffsetOfDiscontiguousDataAddrField(), cg);
               }
            else if (!isVariableLen && node->getFirstChild()->getOpCode().isLoadConst() && node->getFirstChild()->getInt() == 0)
               {
               if (comp->getOption(TR_TraceCG))
                  traceMsg(comp, "Node (%p): Dealing with full/compressed refs fixed length zero size array.\n", node);

               dataAddrMR = generateS390MemoryReference(resReg, TR::Compiler->om.discontiguousArrayHeaderSizeInBytes(), cg);
               dataAddrSlotMR = generateS390MemoryReference(resReg, fej9->getOffsetOfDiscontiguousDataAddrField(), cg);
               }
            else
               {
               if (comp->getOption(TR_TraceCG))
                  {
                  traceMsg(comp,
                     "Node (%p): Dealing with either full/compressed refs fixed length non-zero size array or full refs variable length array.\n",
                     node);
                  }

               if (!TR::Compiler->om.compressObjectReferences())
                  {
                  TR_ASSERT_FATAL_WITH_NODE(node,
                     fej9->getOffsetOfDiscontiguousDataAddrField() == fej9->getOffsetOfContiguousDataAddrField(),
                     "dataAddr field offset is expected to be same for both contiguous and discontiguous arrays in full refs. "
                     "But was %d bytes for discontiguous and %d bytes for contiguous array.\n",
                     fej9->getOffsetOfDiscontiguousDataAddrField(), fej9->getOffsetOfContiguousDataAddrField());
                  }

               dataAddrMR = generateS390MemoryReference(resReg, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg);
               dataAddrSlotMR = generateS390MemoryReference(resReg, fej9->getOffsetOfContiguousDataAddrField(), cg);
               }

            iCursor = generateRXInstruction(cg, TR::InstOpCode::LA, node, temp1Reg, dataAddrMR, iCursor);
            iCursor = generateRXInstruction(cg, TR::InstOpCode::STG, node, temp1Reg, dataAddrSlotMR, iCursor);

            if (offsetReg)
               {
               conditions->addPostCondition(offsetReg, TR::RealRegister::AssignAny);
               cg->stopUsingRegister(offsetReg);
               }
            }
#endif /* TR_TARGET_64BIT */

         // Write Arraylet Pointer
         if (generateArraylets)
            {
            iCursor = generateS390ImmOp(cg, TR::InstOpCode::getAddOpCode(), node, temp1Reg, resReg, dataBegin, conditions, litPoolBaseReg);
            iCursor = generateS390ImmOp(cg, TR::InstOpCode::getAddOpCode(), node, temp1Reg, temp1Reg, -((int64_t)(0)), conditions, litPoolBaseReg);
            if(TR::Compiler->om.compressedReferenceShiftOffset() > 0)
            iCursor = generateRSInstruction(cg, TR::InstOpCode::SRL, node, temp1Reg, TR::Compiler->om.compressedReferenceShiftOffset(), iCursor);

            iCursor = generateRXInstruction(cg, (comp->target().is64Bit()&& !comp->useCompressedPointers()) ? TR::InstOpCode::STG : TR::InstOpCode::ST, node, temp1Reg,
                  generateS390MemoryReference(resReg, fej9->getOffsetOfContiguousArraySizeField(), cg), iCursor);

            }
         }
      else
         {
         genInitObjectHeader(node, iCursor, classAddress, classReg , resReg, zeroReg, temp1Reg, litPoolBaseReg, conditions, cg);
         }

      TR_ASSERT((fej9->tlhHasBeenCleared() || J9JIT_TOSS_CODE), "");

      //////////////////////////////////////////////////////////////////////////////////////////////////////
      ///============================ STAGE 5b: Prefetch after stores ===================================///
      //////////////////////////////////////////////////////////////////////////////////////////////////////
      if (cg->enableTLHPrefetching())
         {
         iCursor = generateS390MemInstruction(cg, TR::InstOpCode::PFD, node, 2, generateS390MemoryReference(resReg, 0x100, cg), iCursor);
         }

      //////////////////////////////////////////////////////////////////////////////////////////////////////
      ///============================ STAGE 6: AOT Relocation Records ===================================///
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
               classToValidate = fej9->getComponentClassFromArrayClass(classToValidate);
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
      ///============================ STAGE 7: Done. Housekeeping items =================================///
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

            generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionStart, firstBRCToOOL->getPrev());
            cFlowRegionStart->setStartInternalControlFlow();

            generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionEnd, conditions, secondBRCToOOL);
            cFlowRegionEnd->setEndInternalControlFlow();
            }
         iCursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionEnd);
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
         generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionStart, next->getPrev());
         cFlowRegionStart->setStartInternalControlFlow();

         iCursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionEnd, conditions);
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
      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionStart);
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
         generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionStart);
         cFlowRegionStart->setStartInternalControlFlow();
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZ,   node, snippetLabel);

         snippet      = new (cg->trHeapMemory()) TR::S390HelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference());
         cg->addSnippet(snippet);
         }
      else
         {
         generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionStart);
         cFlowRegionStart->setStartInternalControlFlow();
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZ,   node, snippetLabel);
         }
      }

   // Test equality of the object classes.
   //
   TR::TreeEvaluator::genLoadForObjectHeaders(cg, node, tempReg, generateS390MemoryReference(object1Reg, TR::Compiler->om.offsetOfObjectVftField(), cg), NULL);

   if (TR::Compiler->om.compressObjectReferences())
      generateRXInstruction(cg, TR::InstOpCode::X, node, tempReg, generateS390MemoryReference(object2Reg, TR::Compiler->om.offsetOfObjectVftField(), cg));
   else
      generateRXInstruction(cg, TR::InstOpCode::getXOROpCode(), node, tempReg, generateS390MemoryReference(object2Reg, TR::Compiler->om.offsetOfObjectVftField(), cg));

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
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, fallThrough, deps);
   fallThrough->setEndInternalControlFlow();

   cg->stopUsingRegister(tempClassReg);
   cg->stopUsingRegister(tempReg);
   cg->decReferenceCount(object1);
   cg->decReferenceCount(object2);

   return 0;
   }



/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
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

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, startLabel);

   generateRRInstruction(cg, TR::InstOpCode::getLoadTestRegOpCode(), node, thisClassReg, thisClassReg);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, outlinedCallLabel);
   generateRRInstruction(cg, TR::InstOpCode::getLoadTestRegOpCode(), node, checkClassReg, checkClassReg);
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
      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, failLabel, deps);
      generateRIInstruction(cg, TR::InstOpCode::LHI, node, tempReg, 0);

      cg->stopUsingRegister(objClassReg);
      cg->stopUsingRegister(castClassReg);
      cg->stopUsingRegister(scratch1Reg);
      cg->stopUsingRegister(scratch2Reg);
      }
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, deps);

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
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, slowPathLabel);

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
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, doneOOLLabel, loopDeps);
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
   intptr_t *funcdescrptr = (intptr_t*) fej9->getReferenceArrayCopyHelperAddress();
   if (comp->compileRelocatableCode() || comp->isOutOfProcessCompilation())
      {
      generateRegLitRefInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, helperReg, (intptr_t)funcdescrptr, TR_ArrayCopyHelper, NULL, NULL, NULL);
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

   TR::SymbolReference* helperCallSymRef = cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390referenceArrayCopyHelper);
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

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, doneLabel);

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

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, OKLabel, deps);

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
         bool is64Bit = comp->target().is64Bit();
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
      if (!comp->getOptions()->realTimeGC())
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
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);
   }

TR::Register*
J9::Z::TreeEvaluator::VMinlineCompareAndSwap(TR::Node *node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic casOp, bool isObj)
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

      isValueCompressedReference = true;

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
      if (comp->target().cpu.supportsFeature(OMR_FEATURE_S390_GUARDED_STORAGE))
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

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, cond);

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

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, doneLabelWrtBar, condWrtBar);

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
   uintptr_t mask = TR::Compiler->om.maskOfObjectVftField();
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

   TR::Instruction * cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, OOLStartLabel);
   if (debugObj)
      debugObj->addInstructionComment(cursor, "OOL RION/OFF seq");

   // Generate the RION/RIOFF instruction.
   cursor = generateRuntimeInstrumentationInstruction(cg, op, node, NULL, cursor);

   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, OOLReturnLabel, cursor);

   RIOnOffOOL->swapInstructionListsWithCompilation();

   preced = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, OOLReturnLabel, preced);

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

TR::Register*
J9::Z::TreeEvaluator::inlineSinglePrecisionSQRT(TR::Node *node, TR::CodeGenerator *cg)
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

TR::Register*
J9::Z::TreeEvaluator::inlineCurrentTimeMaxPrecision(TR::CodeGenerator* cg, TR::Node* node)
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
      if (cg->comp()->target().isZOS())
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
      if (cg->comp()->target().isZOS())
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

TR::Register*
J9::Z::TreeEvaluator::inlineAtomicOps(TR::Node *node, TR::CodeGenerator *cg, int8_t size, TR::MethodSymbol *method, bool isArray)
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
   if (comp->target().cpu.isAtLeast(OMR_PROCESSOR_S390_Z196))
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

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, loopLabel);
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

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionEnd, dependencies);
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

TR::Register*
J9::Z::TreeEvaluator::inlineAtomicFieldUpdater(TR::Node *node, TR::CodeGenerator *cg, TR::MethodSymbol *method)
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

   bool is64Bit = comp->target().is64Bit() && !comp->useCompressedPointers();

   // cclass == null?
   generateRRInstruction(cg, is64Bit ? TR::InstOpCode::XGR : TR::InstOpCode::XR, node, tempReg, tempReg);
   generateRXInstruction(cg, is64Bit ? TR::InstOpCode::CG : TR::InstOpCode::C, node, tempReg, generateS390MemoryReference(thisReg, cclass, cg));
   generateRRFInstruction(cg, TR::InstOpCode::LOCR, node, tempReg, trueReg, getMaskForBranchCondition(TR::InstOpCode::COND_BNER), true);

   // obj == null?
   generateRRInstruction(cg, comp->target().is64Bit() ? TR::InstOpCode::LTGR : TR::InstOpCode::LTR, node, objReg, objReg);
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
      generateRXInstruction(cg, comp->target().is64Bit() ? TR::InstOpCode::LG : TR::InstOpCode::L, node, objClassReg, generateS390MemoryReference(objClassReg, fej9->getOffsetOfJavaLangClassFromClassField(), cg));

      // get tclass
      generateRXInstruction(cg, comp->target().is64Bit() ? TR::InstOpCode::LG : TR::InstOpCode::L, node, tClassReg, generateS390MemoryReference(thisReg, tclass, cg));
      }
   generateRRInstruction(cg, comp->target().is64Bit() ? TR::InstOpCode::CGR : TR::InstOpCode::CR, node, objClassReg, tClassReg);
   generateRRFInstruction(cg, TR::InstOpCode::LOCR, node, tempReg, trueReg, getMaskForBranchCondition(TR::InstOpCode::COND_BNER), true);

   // if any of the above has set the flag, we need to revert back to call the original method via OOL
   generateRRInstruction(cg, TR::InstOpCode::LTR, node, tempReg, tempReg);
   generateS390BranchInstruction (cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, callLabel);

   // start OOL
   TR_S390OutOfLineCodeSection *outlinedCall = new (cg->trHeapMemory()) TR_S390OutOfLineCodeSection(callLabel, doneLabel, cg);
   cg->getS390OutOfLineCodeSectionList().push_front(outlinedCall);
   outlinedCall->swapInstructionListsWithCompilation();
   TR::Instruction * cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, callLabel);

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

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, doneLabel);

   cg->stopUsingRegister(tempReg);
   cg->stopUsingRegister(deltaReg);
   cg->stopUsingRegister(offsetReg);
   cg->stopUsingRegister(tClassReg);
   cg->stopUsingRegister(objClassReg);

   return resultReg;
   }

TR::Register*
J9::Z::TreeEvaluator::inlineKeepAlive(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *paramNode = node->getFirstChild();
   TR::Register *paramReg = cg->evaluate(paramNode);
   TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 1, cg);
   conditions->addPreCondition(paramReg, TR::RealRegister::AssignAny);
   conditions->addPostCondition(paramReg, TR::RealRegister::AssignAny);
   TR::LabelSymbol *label = generateLabelSymbol(cg);
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, label, conditions);
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
         generateRRInstruction(cg, comp->target().is64Bit() ? TR::InstOpCode::LTGR : TR::InstOpCode::LTR, node, resultReg, resultReg);
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

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, doneLabelWrtBar, condWrtBar);

      cg->stopUsingRegister(epReg);
      cg->stopUsingRegister(raReg);
      }
   }

TR::Register*
J9::Z::TreeEvaluator::inlineConcurrentLinkedQueueTMOffer(TR::Node *node, TR::CodeGenerator *cg)
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

      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, insertLabel);

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

      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, deps);

      cursor = generateSInstruction(cg, TR::InstOpCode::TEND, node, generateS390MemoryReference(cg->machine()->getRealRegister(TR::RealRegister::GPR0),0,cg));
      }

   if (useNonConstrainedTM || disableTMOffer)
      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, failLabel, deps);

   genWrtBarForTM(node, cg, rP, rN, rReturn, true);
   genWrtBarForTM(node, cg, rThis, rN, rReturn, true);

   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   cg->stopUsingRegister(rP);
   cg->stopUsingRegister(rQ);

   node->setRegister(rReturn);
   return rReturn;
   }

TR::Register*
J9::Z::TreeEvaluator::inlineConcurrentLinkedQueueTMPoll(TR::Node *node, TR::CodeGenerator *cg)
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
         cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, doneLabel);
      else
         cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, deps);

      cursor = generateSInstruction(cg, TR::InstOpCode::TEND, node, generateS390MemoryReference(cg->machine()->getRealRegister(TR::RealRegister::GPR0),0,cg));
      }

   if (useNonConstrainedTM || disableTMPoll)
      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, failLabel, deps);

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
      genLoadAddressConstant(cg, node, (uintptr_t) comp->getRecompilationInfo()->getCounterAddress(), biAddrReg);

      // Counter is 32-bit, so only use 32-bit opcodes
      TR::MemoryReference * recompMR = generateS390MemoryReference(biAddrReg, 0, cg);
      generateSIInstruction(cg, TR::InstOpCode::ASI, node, recompMR, -1);
      recompMR->stopUsingMemRefRegister(cg);

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
      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionStart);
      cFlowRegionStart->setStartInternalControlFlow();
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, snippetLabel);

      TR::Snippet * snippet = new (cg->trHeapMemory()) TR::S390ForceRecompilationSnippet(cg, node, restartLabel, snippetLabel);
      cg->addSnippet(snippet);
      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, restartLabel, dependencies);
      TR_ASSERT_FATAL(cg->comp()->getRecompilationInfo(), "Recompilation info should be available");
      cg->comp()->getRecompilationInfo()->getJittedBodyInfo()->setHasEdoSnippet();

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
   TR_ASSERT( !isLong || cg->comp()->target().is64Bit(), "CountDigitEvaluator requires 64-bit support for longs");

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
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node,  cFloWRegionStart);
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

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, label[2]);       // LABEL 2
      countDigitsHelper(node, cg, 2, work[2], inputReg, countReg, cFlowRegionEnd, isLong);           // 2 and 3

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, label[5]);       // LABEL 5

      generateRXInstruction(cg, TR::InstOpCode::CG, node, inputReg, work[5]);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, label[6]);

      countDigitsHelper(node, cg, 4, work[4], inputReg, countReg, cFlowRegionEnd, isLong);           // 4 and 5

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, label[6]);       // LABEL 6
      countDigitsHelper(node, cg, 6, work[6], inputReg, countReg, cFlowRegionEnd, isLong);          // 6 and 7

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, label[11]);      // LABEL 11

      generateRXInstruction(cg, TR::InstOpCode::CG, node, inputReg, work[11]);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, label[14]);

      // LABEL 9
      generateRXInstruction(cg, TR::InstOpCode::CG, node, inputReg, work[9]);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, label[10]);

      countDigitsHelper(node, cg, 8, work[8], inputReg, countReg, cFlowRegionEnd, isLong);           // 8 and 9

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, label[10]);      // LABEL 10
      countDigitsHelper(node, cg, 10, work[10], inputReg, countReg, cFlowRegionEnd, isLong);  // 10 and 11

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, label[14]);      // LABEL 14

      generateRXInstruction(cg, TR::InstOpCode::CG, node, inputReg, work[14]);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, label[16]);

      // LABEL 12
      generateRXInstruction(cg, TR::InstOpCode::CG, node, inputReg, work[12]); // 12
      generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, countReg, 12+1);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BH, node, cFlowRegionEnd);

      // LABEL 13
      countDigitsHelper(node, cg, 13, work[13], inputReg, countReg, cFlowRegionEnd, isLong);  // 13 and 14

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, label[16]);      // LABEL 16

      generateRXInstruction(cg, TR::InstOpCode::CG, node, inputReg, work[16]);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, label[17]);
      // LABEL 15
      countDigitsHelper(node, cg, 15, work[15], inputReg, countReg, cFlowRegionEnd, isLong);  // 15 and 16

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, label[17]);      // LABEL 17
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
      //generateS390LabelInstruction(cg, TR::InstOpCode::label, node, startLabel);

      generateRXInstruction(cg, TR::InstOpCode::C, node, inputReg, work[3]);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, label[5]);

      // LABEL 1
      generateRXInstruction(cg, TR::InstOpCode::C, node, inputReg, work[1]);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, label[2]);

      countDigitsHelper(node, cg, 0, work[0], inputReg, countReg, cFlowRegionEnd, isLong);           // 0 and 1

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, label[2]);       // LABEL 2
      countDigitsHelper(node, cg, 2, work[2], inputReg, countReg, cFlowRegionEnd, isLong);           // 2 and 3

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, label[5]);       // LABEL 5

      generateRXInstruction(cg, TR::InstOpCode::C, node, inputReg, work[5]);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, label[7]);

      countDigitsHelper(node, cg, 4, work[4], inputReg, countReg, cFlowRegionEnd, isLong);           // 4 and 5

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, label[7]);       // LABEL 7

      generateRXInstruction(cg, TR::InstOpCode::C, node, inputReg, work[7]);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, label[8]);

      countDigitsHelper(node, cg, 6, work[6], inputReg, countReg, cFlowRegionEnd, isLong);           // 6 and 7

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, label[8]);       // LABEL 8
      countDigitsHelper(node, cg, 8, work[8], inputReg, countReg, cFlowRegionEnd, isLong);           // 8 and 9


      for (int32_t i = 0; i < 9; i++)
         {
         work[i]->stopUsingMemRefRegister(cg);
         }
      }

   cg->stopUsingRegister(inputReg);
   cg->stopUsingRegister(workReg);

   // End
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionEnd, dependencies);
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
      cursor = generateRRInstruction(cg, comp->target().is64Bit() ? TR::InstOpCode::LTGR : TR::InstOpCode::LTR, node, objReg, objReg);
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

   if (comp->target().is64Bit() && cg->fej9()->generateCompressedLockWord())
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

   return NULL;
   }

/**
 * tfinishEvaluator:  end a transaction
 */
TR::Register *
J9::Z::TreeEvaluator::tfinishEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::MemoryReference * tempMR1 = generateS390MemoryReference(cg->machine()->getRealRegister(TR::RealRegister::GPR0),0,cg);
   TR::Instruction * cursor = generateSInstruction(cg, TR::InstOpCode::TEND, node, tempMR1);

   return NULL;
   }

/**
 * tabortEvaluator:  abort a transaction
 */
TR::Register *
J9::Z::TreeEvaluator::tabortEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Instruction *cursor;
   TR::LabelSymbol * labelDone = generateLabelSymbol(cg);
   TR::Register *codeReg = cg->allocateRegister();
   generateRIInstruction(cg, cg->comp()->target().is64Bit() ? TR::InstOpCode::LGHI : TR::InstOpCode::LHI, node, codeReg, 0);
   //Get the nesting depth
   cursor = generateRREInstruction(cg, TR::InstOpCode::ETND, node, codeReg, codeReg);

   generateRIInstruction(cg, TR::InstOpCode::CHI, node, codeReg, 0);
   //branch on zero to done label
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK8, node, labelDone);
   generateRIInstruction(cg, cg->comp()->target().is64Bit() ? TR::InstOpCode::LGHI : TR::InstOpCode::LHI, node, codeReg, 0x100);
   TR::MemoryReference *codeMR = generateS390MemoryReference(codeReg, 0, cg);
   cursor = generateSInstruction(cg, TR::InstOpCode::TABORT, node, codeMR);
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, labelDone);
   cg->stopUsingRegister(codeReg);
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

      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, startICFLabel);
      startICFLabel->setStartInternalControlFlow();
      }

   TR::Register* dummyRegForRA = cg->allocateRegister();
   TR::Register* dummyRegForEntry = cg->allocateRegister();
   dummyRegForRA->setPlaceholderReg();
   dummyRegForEntry->setPlaceholderReg();

   deps->addPostCondition(resultReg, TR::RealRegister::AssignAny);
   deps->addPostCondition(fieldAddrReg, comp->target().isLinux() ? TR::RealRegister::GPR3 : TR::RealRegister::GPR2);
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
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "readBar/helperCall"), 1, TR::DebugCounter::Cheap);
   if (!fieldUnresolved)
      {
      generateRXInstruction(cg, TR::InstOpCode::LA, node, fieldAddrReg, generateS390MemoryReference(*loadMemRef, 0, cg));
      }

   TR::LabelSymbol* callLabel = generateLabelSymbol(cg);
   TR::LabelSymbol* callEndLabel = generateLabelSymbol(cg);
   TR::Instruction *gcPoint = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, callLabel);
   gcPoint->setNeedsGCMap(0);
   auto readBarHelperSnippet = new (cg->trHeapMemory()) TR::S390HelperCallSnippet(cg, node, callLabel,
                                                                                  cg->symRefTab()->findOrCreateRuntimeHelper(TR_softwareReadBarrier),
                                                                                  callEndLabel);
   cg->addSnippet(readBarHelperSnippet);
   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, callEndLabel);

   // Reload the object after helper call.
   generateRXInstruction(cg, loadOpCode, node, resultReg, generateS390MemoryReference(fieldAddrReg, 0, cg));
   TR::Instruction* cursor = generateS390LabelInstruction(cg, TR::InstOpCode::label, node, doneLabel);
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

   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "readBar/total"), 1, TR::DebugCounter::Cheap);
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
   TR::Compilation *comp = cg->comp();

   if ((node->getArrayCopyElementType() == TR::Address)
           && needsGuardedLoad
           && (!comp->target().cpu.supportsFeature(OMR_FEATURE_S390_GUARDED_STORAGE)))
      {
      TR::Register* resultReg = srm->findOrCreateScratchRegister();
      TR::TreeEvaluator::generateSoftwareReadBarrier(node, cg, resultReg, srcMemRef, deps, true);
      TR::InstOpCode::Mnemonic storeOp = TR::InstOpCode::ST;
      if (comp->target().is64Bit() && !comp->useCompressedPointers())
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

TR::Register*
J9::Z::TreeEvaluator::inlineIntegerToCharsForLatin1Strings(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();

   TR_ResolvedMethod *candidateToStringMethod = NULL;
   if (node->getInlinedSiteIndex() != -1)
      {
      candidateToStringMethod = comp->getInlinedResolvedMethod(node->getInlinedSiteIndex());
      }
   else
      {
      candidateToStringMethod = comp->getCurrentMethod();
      }
   // If method caller of Integer.stringSize or Long.stringSize is not Integer.toString(I) or Long.toString(J), then we don't inline
   if (candidateToStringMethod->getRecognizedMethod() != TR::java_lang_Long_toString &&
            candidateToStringMethod->getRecognizedMethod() != TR::java_lang_Integer_toString)
      {
      return NULL;
      }

   if (comp->getOption(TR_TraceCG))
      {
      traceMsg(comp, "inlineIntegerToCharsForLatin1Strings (compressed strings)\n");
      }
   TR::Node *inputValueNode = node->getChild(0);
   TR::Node *stringSizeNode = node->getChild(1);
   TR::Node *byteArrayNode = node->getChild(2);

   TR::Register *inputValueReg = cg->evaluate(inputValueNode);
   TR::Register *stringSizeReg = cg->gprClobberEvaluate(stringSizeNode, true);
   TR::Register *byteArrayReg = cg->gprClobberEvaluate(byteArrayNode, true);

   bool inputIs64Bit = inputValueNode->getDataType() == TR::Int64;

   TR::MemoryReference *destinationArrayMemRef = generateS390MemoryReference(byteArrayReg, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg);

   TR::LabelSymbol *cFlowRegionStart = generateLabelSymbol(cg);
   cFlowRegionStart->setStartInternalControlFlow();
   TR::LabelSymbol *cFlowRegionEnd = generateLabelSymbol(cg);
   cFlowRegionEnd->setEndInternalControlFlow();

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionStart);

   // If input is 0, then do the work in GPRs and exit.
   // TODO: Measure performance of [1,9] here vs vanilla Java code to see what's faster. If vanilla java, then we should bail from here accordingly.
   // (See https://github.ibm.com/runtimes/openj9/pull/385#discussion_r5004355 for discussion)
   TR::Register *numCharsRemainingReg = cg->allocateRegister(); // this is also the index of the position of the first char after we have populated the buffer
   TR::LabelSymbol *nonZeroInputLabel = generateLabelSymbol(cg);
   generateS390CompareAndBranchInstruction(cg, inputIs64Bit ? TR::InstOpCode::CG : TR::InstOpCode::C, node, inputValueReg, 0, TR::InstOpCode::COND_BNE, nonZeroInputLabel, false);
   generateSIInstruction(cg, TR::InstOpCode::MVI, node, generateS390MemoryReference(*destinationArrayMemRef, 0, cg), 48);
   generateRILInstruction(cg, TR::InstOpCode::IILF, node, numCharsRemainingReg, 0);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cFlowRegionEnd);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, nonZeroInputLabel);

   TR::LabelSymbol *handleDigitsLabel = generateLabelSymbol(cg);
   // First handle negative sign if needed. Then proceed to handleDigitsLabel to process the digits.
   generateS390CompareAndBranchInstruction(cg, inputIs64Bit ? TR::InstOpCode::CG : TR::InstOpCode::C, node, inputValueReg, 0, TR::InstOpCode::COND_BNL, handleDigitsLabel, false);
   generateRILInstruction(cg, TR::InstOpCode::SLFI, node, stringSizeReg, 1);
   generateSIInstruction(cg, TR::InstOpCode::MVI, node, generateS390MemoryReference(*destinationArrayMemRef, 0, cg), 45);
   generateRILInstruction(cg, TR::InstOpCode::getAddImmOpCode(), node, byteArrayReg, 1);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, handleDigitsLabel);
   TR::Register *intToPDReg = cg->allocateRegister(TR_VRF);
   // Load all digits into packed decimal format.
   generateVRIiInstruction(cg, inputIs64Bit ? TR::InstOpCode::VCVDG : TR::InstOpCode::VCVD, node, intToPDReg, inputValueReg, inputIs64Bit ? 19 : 10, 0x1);
   TR::Register *maskReg = cg->allocateRegister(TR_VRF);
   TR::Register *zonedDecimalReg1 = cg->allocateRegister(TR_VRF);
   TR::Register *zonedDecimalReg2 = NULL;

   TR::RegisterDependencyConditions *dependencies = NULL;

   if (inputIs64Bit)
      {
      // if the long input value is greater than 16 digits in length, then we need two vector registers to do the conversion. so jump to lengthgreaterthan16label
      // to handle that case.
      TR::LabelSymbol *lengthGreaterThan16Label = generateLabelSymbol(cg);
      generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::C, node, stringSizeReg, 16, TR::InstOpCode::COND_BH, lengthGreaterThan16Label, false);
      // this instruction unpacks the packed decimal in inttppdreg to zoned decimal format. it will do this for the rightmost 16 digits.
      // it populates the higher 4 bits of each byte with the "zone" bits and the bottom 4 bits with each digit from the packed decimal sequence.
      generateVRRkInstruction(cg, TR::InstOpCode::VUPKZL, node, zonedDecimalReg1, intToPDReg, 0 /*m3*/);
      // now we zero out the zone bits because we don't need them.
      generateVRIbInstruction(cg, TR::InstOpCode::VGM, node, maskReg, 4, 7, 0);
      generateVRRcInstruction(cg, TR::InstOpCode::VN, node, zonedDecimalReg1, zonedDecimalReg1, maskReg, 0, 0, 0);
      // now the rightmost 10 bytes should hold the entire integer in packed decimal format. so let's add 48 to each byte to convert each digit to ascii.
      generateVRIaInstruction(cg, TR::InstOpCode::VREPI, node, maskReg, 48, 0);
      generateVRRcInstruction(cg, TR::InstOpCode::VA, node, zonedDecimalReg1, zonedDecimalReg1, maskReg, 0);
      // for the purposes of this evaluator, stringsizereg contains the length of the resulting string. ex if input is 2147483647, stringsizereg will be 10.
      // when storing using vstrl, the index register specifying the first byte to store is 0 based. meanwhile
      // stringsizereg is 1 based. so we must first subtract 1 from stringsizereg so the calculation is done correctly by the instruction.
      // ex. if  we specify 10 in vstrl, the instruction will do 15-10=5 to figure out that it needs to store bytes 5 to 15 instead of 6 to 15.
      generateRILInstruction(cg, TR::InstOpCode::SLFI, node, stringSizeReg, 1);
      // the memory reference should already be pointing to where the most significant digit is to be stored. so we just have to create the vstrl instruction now.
      generateVRSdInstruction(cg, TR::InstOpCode::VSTRLR, node, stringSizeReg, zonedDecimalReg1, generateS390MemoryReference(*destinationArrayMemRef, 0, cg));
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cFlowRegionEnd);

      // if we end up here, then there are more than 16 digits in the input value. this instruction sequence is similar to the one above, except that
      // we handle the 1 to 3 of the most significant digits in a separate register. we then store the value in this register before storing the remainder of the digits.
      generateS390LabelInstruction(cg, TR::InstOpCode::label, node, lengthGreaterThan16Label);
      zonedDecimalReg2 = cg->allocateRegister(TR_VRF); // holds the most significant digits. can be anywhere from 1-3 digits.
      generateVRRkInstruction(cg, TR::InstOpCode::VUPKZL, node, zonedDecimalReg1, intToPDReg, 0 /*m3*/);
      generateVRRkInstruction(cg, TR::InstOpCode::VUPKZH, node, zonedDecimalReg2, intToPDReg, 0 /*m3*/);
      // now we zero out the zone bits because we don't need them.
      generateVRIbInstruction(cg, TR::InstOpCode::VGM, node, maskReg, 4, 7, 0);
      generateVRRcInstruction(cg, TR::InstOpCode::VN, node, zonedDecimalReg1, zonedDecimalReg1, maskReg, 0, 0, 0);
      generateVRRcInstruction(cg, TR::InstOpCode::VN, node, zonedDecimalReg2, zonedDecimalReg2, maskReg, 0, 0, 0);
      // now add 48 to each byte.
      generateVRIaInstruction(cg, TR::InstOpCode::VREPI, node, maskReg, 48, 0);
      generateVRRcInstruction(cg, TR::InstOpCode::VA, node, zonedDecimalReg1, zonedDecimalReg1, maskReg, 0);
      generateVRRcInstruction(cg, TR::InstOpCode::VA, node, zonedDecimalReg2, zonedDecimalReg2, maskReg, 0);
      // now calculate how many digits are in the top half of the zoned decimal (i.e. zoneddecimalreg2) --> (stringsizereg - 16) - 1 = stringsizereg - 17
      generateRILInstruction(cg, TR::InstOpCode::SLFI, node, stringSizeReg, 17);
      // the memory reference should already be pointing to where the most significant digit is to be stored. so we just have to create the vstrl instruction now.
      generateVRSdInstruction(cg, TR::InstOpCode::VSTRLR, node, stringSizeReg, zonedDecimalReg2, generateS390MemoryReference(*destinationArrayMemRef, 0, cg));
      // increment bytearrayreg by stringsizereg+1 to move buffer pointer forward so we can write remaining bytes.
      generateRILInstruction(cg, TR::InstOpCode::AFI, node, stringSizeReg, 1);
      generateRRInstruction(cg, TR::InstOpCode::getAddRegWidenOpCode(), node, byteArrayReg, stringSizeReg);
      generateVSIInstruction(cg, TR::InstOpCode::VSTRL, node, zonedDecimalReg1, generateS390MemoryReference(*destinationArrayMemRef, 0, cg), 15);

      dependencies = generateRegisterDependencyConditions(0, 8, cg);
      dependencies->addPostCondition(zonedDecimalReg2, TR::RealRegister::AssignAny);
      }
   else
      {
      // This instruction unpacks the packed decimal in intTpPDReg to zoned decimal format. It will do this for the rightmost 16 digits.
      // It populates the higher 4 bits of each byte with the "zone" bits and the bottom 4 bits with each digit from the packed decimal sequence.
      generateVRRkInstruction(cg, TR::InstOpCode::VUPKZL, node, zonedDecimalReg1, intToPDReg, 0 /*M3*/);
      // Now we zero out the zone bits because we don't need them.
      generateVRIbInstruction(cg, TR::InstOpCode::VGM, node, maskReg, 4, 7, 0);
      generateVRRcInstruction(cg, TR::InstOpCode::VN, node, zonedDecimalReg1, zonedDecimalReg1, maskReg, 0, 0, 0);
      // Now the rightmost 10 bytes should hold the entire integer value in packed decimal form. So let's add 48 to each byte to convert each digit to ASCII.
      generateVRIaInstruction(cg, TR::InstOpCode::VREPI, node, maskReg, 48, 0);
      generateVRRcInstruction(cg, TR::InstOpCode::VA, node, zonedDecimalReg1, zonedDecimalReg1, maskReg, 0);

      // For the purposes of this evaluator, stringSizeReg contains the length of the resulting string. Ex if input is 2147483647, stringSizeReg will be 10.
      // When storing using VSTRL, the index register specifying the first byte to store is 0 based. Meanwhile
      // stringSizeReg is 1 based. So we must first subtract 1 from stringSizeReg so the calculation is done correctly by the instruction.
      // ex. if  we specify 10 in VSTRL, the instruction will do 15-10=5 to figure out that it needs to store bytes 5 to 15 instead of 6 to 15.
      generateRILInstruction(cg, TR::InstOpCode::SLFI, node, stringSizeReg, 1);
      // The memory reference should already be pointing to where the most significant digit is to be stored. So we just have to create the VSTRL instruction now.
      generateVRSdInstruction(cg, TR::InstOpCode::VSTRLR, node, stringSizeReg, zonedDecimalReg1, generateS390MemoryReference(*destinationArrayMemRef, 0, cg));

      dependencies = generateRegisterDependencyConditions(0, 7, cg);
      }

   dependencies->addPostCondition(inputValueReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(byteArrayReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(stringSizeReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(numCharsRemainingReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(intToPDReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(maskReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(zonedDecimalReg1, TR::RealRegister::AssignAny);

   // For the purposes of inlining Integer.toString and Long.toString, the return value of the getChars API will always be 0. So we load it here manually.
   generateRILInstruction(cg, TR::InstOpCode::IILF, node, numCharsRemainingReg, 0);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionEnd, dependencies);

   cg->decReferenceCount(inputValueNode);
   cg->decReferenceCount(stringSizeNode);
   cg->decReferenceCount(byteArrayNode);

   cg->stopUsingRegister(intToPDReg);
   cg->stopUsingRegister(maskReg);
   cg->stopUsingRegister(zonedDecimalReg1);

   cg->stopUsingRegister(byteArrayReg);
   cg->stopUsingRegister(stringSizeReg);
   cg->stopUsingRegister(zonedDecimalReg2);

   return node->setRegister(numCharsRemainingReg);
   }

TR::Register*
J9::Z::TreeEvaluator::inlineIntegerToCharsForUTF16Strings(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();

   TR_ResolvedMethod *candidateToStringMethod = NULL;
   if (node->getInlinedSiteIndex() != -1)
      {
      candidateToStringMethod = comp->getInlinedResolvedMethod(node->getInlinedSiteIndex());
      }
   else
      {
      candidateToStringMethod = comp->getCurrentMethod();
      }
   // If method caller of Integer.stringSize or Long.stringSize is not Integer.toString(I) or Long.toString(J), then we don't inline
   if (candidateToStringMethod->getRecognizedMethod() != TR::java_lang_Long_toString &&
            candidateToStringMethod->getRecognizedMethod() != TR::java_lang_Integer_toString)
      {
      return NULL;
      }

   if (comp->getOption(TR_TraceCG))
      {
      traceMsg(comp, "inlineIntegerToCharsForUTF16Strings (decompressed strings)\n");
      }
   TR::Node *inputValueNode = node->getChild(0);
   TR::Node *stringSizeNode = node->getChild(1);
   TR::Node *byteArrayNode = node->getChild(2);

   TR::Register *inputValueReg = cg->evaluate(inputValueNode);
   TR::Register *stringSizeReg = cg->gprClobberEvaluate(stringSizeNode, true);
   TR::Register *byteArrayReg = cg->gprClobberEvaluate(byteArrayNode, true);

   bool inputIs64Bit = inputValueNode->getDataType() == TR::Int64;

   TR::LabelSymbol *cFlowRegionStart = generateLabelSymbol(cg);
   cFlowRegionStart->setStartInternalControlFlow();
   TR::LabelSymbol *cFlowRegionEnd = generateLabelSymbol(cg);
   cFlowRegionEnd->setEndInternalControlFlow();

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionStart);

   // If input is 0, just do the work in GPRs and exit.
   TR::Register *numCharsRemainingReg = cg->allocateRegister(); // this is also the index of the position of the first char after we have populated the buffer
   TR::LabelSymbol *nonZeroInputLabel = generateLabelSymbol(cg);
   generateS390CompareAndBranchInstruction(cg, inputIs64Bit ? TR::InstOpCode::CG : TR::InstOpCode::C, node, inputValueReg, 0, TR::InstOpCode::COND_BNE, nonZeroInputLabel, false);
   TR::MemoryReference *destinationArrayMemRef = generateS390MemoryReference(byteArrayReg, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg);
   generateSILInstruction(cg, TR::InstOpCode::MVHHI, node, destinationArrayMemRef, 48);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cFlowRegionEnd);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, nonZeroInputLabel);

   TR::LabelSymbol *handleDigitsLabel = generateLabelSymbol(cg);
   // Handle negative sign first.
   generateS390CompareAndBranchInstruction(cg, inputIs64Bit ? TR::InstOpCode::CG : TR::InstOpCode::C, node, inputValueReg, 0, TR::InstOpCode::COND_BNL, handleDigitsLabel, false);
   generateRILInstruction(cg, TR::InstOpCode::SLFI, node, stringSizeReg, 1);
   generateSILInstruction(cg, TR::InstOpCode::MVHHI, node, generateS390MemoryReference(*destinationArrayMemRef, 0, cg), 45);
   generateRILInstruction(cg, TR::InstOpCode::getAddImmOpCode(), node, byteArrayReg, 2);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, handleDigitsLabel);

   TR::Register *intToPDReg = cg->allocateRegister(TR_VRF);
   // Load all digits into packed decimal format.
   generateVRIiInstruction(cg, inputIs64Bit ? TR::InstOpCode::VCVDG : TR::InstOpCode::VCVD, node, intToPDReg, inputValueReg, inputIs64Bit ? 19 : 10, 0x1);
   TR::Register *maskReg = cg->allocateRegister(TR_VRF);
   TR::Register *asciiOffset = cg->allocateRegister(TR_VRF);
   generateVRIbInstruction(cg, TR::InstOpCode::VGM, node, maskReg, 4, 7, 0);
   generateVRIaInstruction(cg, TR::InstOpCode::VREPI, node, asciiOffset, 48, 0);
   TR::Register *zonedDecimalRegLower = cg->allocateRegister(TR_VRF);

   TR::LabelSymbol *moreThan9DigitsLabel = generateLabelSymbol(cg);
   // Depending on the length of the resulting string, we will need different amounts of vector registers to do the conversion. We test for that
   // here and then branch to a handcrafted routine for each scenario. This creates some redundancy in the code generated (hence increasing footprint),
   // however it reduces checks/branches during runtime preventing bottlenecks.
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::C, node, stringSizeReg, 8, TR::InstOpCode::COND_BH, moreThan9DigitsLabel, false);

   // In this scenario we only need one vector register to do the conversion as there are less than 9 digits to process
   // If we are here, then 0 =< stringSizeReg <= 8
   // Unpack packed decimal into zoned decimal. This should take a maximum of 8 bytes.
   generateVRRkInstruction(cg, TR::InstOpCode::VUPKZL, node, zonedDecimalRegLower, intToPDReg, 0 /*M3*/);
   // Remove the zone bits
   generateVRRcInstruction(cg, TR::InstOpCode::VN, node, zonedDecimalRegLower, zonedDecimalRegLower, maskReg, 0, 0, 0);
   // Now the rightmost 10 bytes should hold the entire integer we care about. So let's add 48 to each byte.
   generateVRRcInstruction(cg, TR::InstOpCode::VA, node, zonedDecimalRegLower, zonedDecimalRegLower, asciiOffset, 0);

   // For the purposes of this evaluator, stringSizeReg contains the length of the resulting string. Ex if input is 2147483647, stringSizeReg will be 10.
   // When storing using VSTRL, the index register specifying the first byte to store is 0 based. Meanwhile
   // stringSizeReg is 1 based. So we must first subtract 1 from stringSizeReg so the calculation is done correctly by the instruction.
   // ex. if  we specify 10 in VSTRL, the instruction will do 15-10=5 to figure out that it needs to store bytes 5 to 15 instead of 6 to 15.
   // Since each character is 2 bytes in length, we must first multiply by 2.
   generateRSInstruction(cg, TR::InstOpCode::SLA, node, stringSizeReg, 1);
   generateRILInstruction(cg, TR::InstOpCode::SLFI, node, stringSizeReg, 1);
   // Finally, unpack the data in zonedDecimalReg1 using VUPL. The result should take no more than 16 bytes.
   generateVRRaInstruction(cg, TR::InstOpCode::VUPLL, node, zonedDecimalRegLower, zonedDecimalRegLower, 0, 0, 0);
   // The memory reference should already be pointing to where the most significant digit is to be stored. So we just have to create the VSTRL instruction now.
   generateVRSdInstruction(cg, TR::InstOpCode::VSTRLR, node, stringSizeReg, zonedDecimalRegLower, generateS390MemoryReference(*destinationArrayMemRef, 0, cg));
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cFlowRegionEnd);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, moreThan9DigitsLabel);
   TR::LabelSymbol *moreThan16DigitsLabel = generateLabelSymbol(cg);
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::C, node, stringSizeReg, 16, TR::InstOpCode::COND_BH, moreThan16DigitsLabel, false);
   // Need two intermediate vector registers to process input values that are greater than 8 digits and less than 17.
   // For Integers between the length of 8 and 16, we must do as above but also use VUPLH to load the higher order digits. Note that the result
   // of converting packed decimal to zoned decimal will fit in 16 bytes, so we only need to use VUPKZL still.
   generateVRRkInstruction(cg, TR::InstOpCode::VUPKZL, node, zonedDecimalRegLower, intToPDReg, 0 /*M3*/);
   // Remove the zone bits
   generateVRRcInstruction(cg, TR::InstOpCode::VN, node, zonedDecimalRegLower, zonedDecimalRegLower, maskReg, 0, 0, 0);
   // Now the rightmost 16 bytes should hold the entire intege we care about. So let's add 48 to each byte.
   generateVRRcInstruction(cg, TR::InstOpCode::VA, node, zonedDecimalRegLower, zonedDecimalRegLower, asciiOffset, 0);
   // We know zonedDecimalRegLower will be full when we unpack. So we store all bytes in it. But we don't know if
   // zonedDecimalRegLowerUpperHalf will be full. So we must calculate "stringSize-8" to figure out how many extra digits remain.
   // ex if stringSize = 10, then 10-8 = 2 digits in upper half. 15 - 2*2-1 = 12 --> 12,13,14,15 are the bytes stored to memory.
   generateRILInstruction(cg, TR::InstOpCode::SLFI, node, stringSizeReg, 8);
   generateRSInstruction(cg, TR::InstOpCode::SLA, node, stringSizeReg, 1);
   generateRILInstruction(cg, TR::InstOpCode::SLFI, node, stringSizeReg, 1);
   // now stringSize will have position of byte in upper half.
   // Finally, unpack the higher 8 bytes in zonedDecimalReg1 using VUPLH.
   TR::Register *zonedDecimalRegLowerUpperHalf = cg->allocateRegister(TR_VRF);
   generateVRRaInstruction(cg, TR::InstOpCode::VUPLH, node, zonedDecimalRegLowerUpperHalf, zonedDecimalRegLower, 0, 0, 0);
   // And unpack the lower 8 bytes using VUPLL
   generateVRRaInstruction(cg, TR::InstOpCode::VUPLL, node, zonedDecimalRegLower, zonedDecimalRegLower, 0, 0, 0);
   // Store the higher half first as it holds the most significant digits.
   generateVRSdInstruction(cg, TR::InstOpCode::VSTRLR, node, stringSizeReg, zonedDecimalRegLowerUpperHalf, generateS390MemoryReference(*destinationArrayMemRef, 0, cg));

   // Advance the memoryReference pointer then store the bottom half
   generateRILInstruction(cg, TR::InstOpCode::AFI, node, stringSizeReg, 1);
   generateRRInstruction(cg, TR::InstOpCode::getAddRegWidenOpCode(), node, byteArrayReg, stringSizeReg);
   generateVSIInstruction(cg, TR::InstOpCode::VSTRL, node, zonedDecimalRegLower, generateS390MemoryReference(*destinationArrayMemRef, 0, cg), 15);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cFlowRegionEnd);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, moreThan16DigitsLabel);
   // In this scenario we have between 17 and 19 digits. The logic is similar to before, except we use VUPKZH to unpack the most significant digits (could be anywhere from 1 to 3 digits).
   // We first unpack into lower and upper zoned decimal halves.
   generateVRRkInstruction(cg, TR::InstOpCode::VUPKZL, node, zonedDecimalRegLower, intToPDReg, 0 /*M3*/);
   TR::Register *zonedDecimalRegHigher = cg->allocateRegister(TR_VRF);
   generateVRRkInstruction(cg, TR::InstOpCode::VUPKZH, node, zonedDecimalRegHigher, intToPDReg, 0 /*M3*/);
   // Now remove the zone bits in both
   generateVRRcInstruction(cg, TR::InstOpCode::VN, node, zonedDecimalRegLower, zonedDecimalRegLower, maskReg, 0, 0, 0);
   generateVRRcInstruction(cg, TR::InstOpCode::VN, node, zonedDecimalRegHigher, zonedDecimalRegHigher, maskReg, 0, 0, 0);
   // And add 0x30 to all
   generateVRRcInstruction(cg, TR::InstOpCode::VA, node, zonedDecimalRegLower, zonedDecimalRegLower, asciiOffset, 0);
   generateVRRcInstruction(cg, TR::InstOpCode::VA, node, zonedDecimalRegHigher, zonedDecimalRegHigher, asciiOffset, 0);
   // Now unpack the higher half --> i.e. process the most significant digits (anywhere from 1 to 3 digits)
   generateVRRaInstruction(cg, TR::InstOpCode::VUPLL, node, zonedDecimalRegHigher, zonedDecimalRegHigher, 0, 0, 0);
   // Calculate how many digits we need to store in this higher half.
   generateRILInstruction(cg, TR::InstOpCode::SLFI, node, stringSizeReg, 16);
   generateRSInstruction(cg, TR::InstOpCode::SLA, node, stringSizeReg, 1);
   generateRILInstruction(cg, TR::InstOpCode::SLFI, node, stringSizeReg, 1);
   // Store that many bytes from this register
   generateVRSdInstruction(cg, TR::InstOpCode::VSTRLR, node, stringSizeReg, zonedDecimalRegHigher, generateS390MemoryReference(*destinationArrayMemRef, 0, cg));
   // Advance buffer pointer
   generateRILInstruction(cg, TR::InstOpCode::AFI, node, stringSizeReg, 1);
   generateRRInstruction(cg, TR::InstOpCode::getAddRegWidenOpCode(), node, byteArrayReg, stringSizeReg);
   // unpack zonedDecimalRegLower into upper and lower halves --> i.e. we now process the least significant 16 digits.
   generateVRRaInstruction(cg, TR::InstOpCode::VUPLH, node, zonedDecimalRegLowerUpperHalf, zonedDecimalRegLower, 0, 0, 0);
   generateVRRaInstruction(cg, TR::InstOpCode::VUPLL, node, zonedDecimalRegLower, zonedDecimalRegLower, 0, 0, 0);
   generateVSIInstruction(cg, TR::InstOpCode::VSTRL, node, zonedDecimalRegLowerUpperHalf, generateS390MemoryReference(*destinationArrayMemRef, 0, cg), 15);
   // Advance Pointer then store again.
   generateRILInstruction(cg, TR::InstOpCode::getAddImmOpCode(), node, byteArrayReg, 16);
   generateVSIInstruction(cg, TR::InstOpCode::VSTRL, node, zonedDecimalRegLower, generateS390MemoryReference(*destinationArrayMemRef, 0, cg), 15);

   TR::RegisterDependencyConditions *dependencies = generateRegisterDependencyConditions(0, 10, cg);
   dependencies->addPostCondition(inputValueReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(byteArrayReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(stringSizeReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(numCharsRemainingReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(intToPDReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(maskReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(asciiOffset, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(zonedDecimalRegLower, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(zonedDecimalRegLowerUpperHalf, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(zonedDecimalRegHigher, TR::RealRegister::AssignAny);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionEnd, dependencies);

   // For the purposes of inlining Integer.toString and Long.toString, the return value of the getChars API will always be 0. So we load it here manually.
   generateRILInstruction(cg, TR::InstOpCode::IILF, node, numCharsRemainingReg, 0);

   cg->decReferenceCount(inputValueNode);
   cg->decReferenceCount(stringSizeNode);
   cg->decReferenceCount(byteArrayNode);

   cg->stopUsingRegister(intToPDReg);
   cg->stopUsingRegister(maskReg);
   cg->stopUsingRegister(asciiOffset);
   cg->stopUsingRegister(zonedDecimalRegLower);

   cg->stopUsingRegister(byteArrayReg);
   cg->stopUsingRegister(stringSizeReg);

   cg->stopUsingRegister(zonedDecimalRegLowerUpperHalf);
   cg->stopUsingRegister(zonedDecimalRegHigher);

   return node->setRegister(numCharsRemainingReg);
   }

 /*
  * This method inlines calls to Integer.stringSize and Long.stringSize using the VCLZDP instruction on z16
  */
TR::Register*
J9::Z::TreeEvaluator::inlineIntegerStringSize(TR::Node* node, TR::CodeGenerator* cg)
   {
   TR::Compilation *comp = cg->comp();
   static const bool disableIntegerStringSizeBranch = feGetEnv("TR_disableStringSizeBranch") != NULL;
   TR::Node *inputValueNode = node->getChild(0);
   bool inputIs64Bit = inputValueNode->getDataType() == TR::Int64;
   TR::Register *inputValueReg = cg->evaluate(inputValueNode);

   TR::LabelSymbol *cFlowRegionStart = generateLabelSymbol(cg);
   cFlowRegionStart->setStartInternalControlFlow();
   TR::LabelSymbol *cFlowRegionEnd = generateLabelSymbol(cg);
   cFlowRegionEnd->setEndInternalControlFlow();
   TR::LabelSymbol *inputValueZeroLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *countNumDigitsLabel = generateLabelSymbol(cg);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionStart);
   TR::Register *lengthReg = cg->allocateRegister();
   // If value is 0, we branch to end as string is "0"
   generateRIInstruction(cg, TR::InstOpCode::LHI, node, lengthReg, 1);
   generateS390CompareAndBranchInstruction(cg, inputIs64Bit ? TR::InstOpCode::CG : TR::InstOpCode::C, node, inputValueReg, 0, TR::InstOpCode::COND_BE, cFlowRegionEnd, false);

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, countNumDigitsLabel);
   TR::Register *intToPDReg = cg->allocateRegister(TR_VRF);
   TR::Register *maxNumDigitsReg = cg->allocateRegister(TR_VRF);
   TR::Register *lengthVectorReg = cg->allocateRegister(TR_VRF);
   TR::Register *signBitConstant = disableIntegerStringSizeBranch ? cg->allocateRegister(TR_VRF) : NULL;
   if (disableIntegerStringSizeBranch)
      {
      generateVRIaInstruction(cg, TR::InstOpCode::VLEIB, node, signBitConstant, 1, 15);
      }
   generateVRIaInstruction(cg, TR::InstOpCode::VLEIB, node, maxNumDigitsReg, 31, 7);
   generateVRIiInstruction(cg, inputIs64Bit ? TR::InstOpCode::VCVDG : TR::InstOpCode::VCVD, node, intToPDReg, inputValueReg, inputIs64Bit ? 19 : 10, 0x1);
   TR::Register *leadingZerosReg = cg->allocateRegister(TR_VRF);
   generateVRRkInstruction(cg, TR::InstOpCode::VCLZDP, node, leadingZerosReg, intToPDReg, 0 /*M3*/);
   // Now subtract to get length of string
   generateVRRcInstruction(cg, TR::InstOpCode::VS, node, lengthVectorReg, maxNumDigitsReg, leadingZerosReg, 0);

   if (disableIntegerStringSizeBranch)
      {
      generateVRRcInstruction(cg, TR::InstOpCode::VN, node, intToPDReg, intToPDReg, signBitConstant, 0, 0, 0);
      generateVRRcInstruction(cg, TR::InstOpCode::VA, node, lengthVectorReg, lengthVectorReg, intToPDReg, 0);
      }
   else
      {
      generateVRScInstruction(cg, TR::InstOpCode::VLGV, node, lengthReg, lengthVectorReg, generateS390MemoryReference(7, cg), 0);
      // If value is greater than 0, we branch to end. Otherwise we add 1 to lengthReg to account for '-' sign.
      generateS390CompareAndBranchInstruction(cg, inputIs64Bit ? TR::InstOpCode::CG : TR::InstOpCode::C, node, inputValueReg, 0, TR::InstOpCode::COND_BNL, cFlowRegionEnd, false);
      generateRILInstruction(cg, TR::InstOpCode::AFI, node, lengthReg, 1);
      }

   TR::RegisterDependencyConditions *dependencies = generateRegisterDependencyConditions(0, disableIntegerStringSizeBranch ? 7 : 6, cg);
   dependencies->addPostCondition(inputValueReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(intToPDReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(leadingZerosReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(lengthReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(maxNumDigitsReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(lengthVectorReg, TR::RealRegister::AssignAny);
   if (disableIntegerStringSizeBranch)
      {
      dependencies->addPostCondition(signBitConstant, TR::RealRegister::AssignAny);
      }

   generateS390LabelInstruction(cg, TR::InstOpCode::label, node, cFlowRegionEnd, dependencies);

   cg->decReferenceCount(inputValueNode);
   cg->stopUsingRegister(intToPDReg);
   cg->stopUsingRegister(leadingZerosReg);
   cg->stopUsingRegister(maxNumDigitsReg);
   cg->stopUsingRegister(lengthVectorReg);
   if (disableIntegerStringSizeBranch)
      {
      cg->stopUsingRegister(signBitConstant);
      }

   return node->setRegister(lengthReg);
   }
