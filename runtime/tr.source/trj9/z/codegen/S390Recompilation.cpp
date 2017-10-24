/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "z/codegen/S390Recompilation.hpp"

#include "codegen/CodeGenerator.hpp"
#include "codegen/Machine.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/CompilerEnv.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "z/codegen/CallSnippet.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/TRSystemLinkage.hpp"

// Allocate a machine-specific recompilation processor for this compilation
//
TR::Recompilation *
TR_S390Recompilation::allocate(TR::Compilation * comp)
   {
   if (comp->isRecompilationEnabled())
      {
      return new (comp->trHeapMemory()) TR_S390Recompilation(comp);
      }

   return NULL;
   }


TR_PersistentMethodInfo *
TR_S390Recompilation::getExistingMethodInfo(TR_ResolvedMethod * method)
   {
   // The method was previously compiled. Find its method info block from the
   // start PC address. The mechanism is different depending on whether the
   // method was compiled for sampling or counting.
   //
   char * startPC = (char *) method->startAddressForInterpreterOfJittedMethod();
   if (NULL == startPC)
      {
      return NULL;
      }
   // need to pass in StartPC, which is the entry point of the interpreter ie includes the loads
   TR_PersistentMethodInfo *info = getMethodInfoFromPC(startPC);
   if (debug("traceRecompilation"))
      {
      //diagnostic("RC>>Recompiling %s at level %d\n", signature(_compilation->getCurrentMethod()), info->getHotness());
      }

   return info;
   }

TR_S390Recompilation::TR_S390Recompilation(TR::Compilation * comp)
   : TR::Recompilation(comp),
     bodyInfoDataConstant(NULL)
   {
   _countingSupported = true;
   setupMethodInfo();
   }


//--------------------------------------------------------------------------------------------------
// Calculate size of instructions bracketed by, and including, start and end instructions
//--------------------------------------------------------------------------------------------------
uint32_t
CalcCodeSize(TR::Instruction * start, TR::Instruction * end)
   {
   uint32_t size = 0;
   TR::InstOpCode opcode;
   while (start != end)
      {
      // TODO: Absolute hack. We need to encoding the size of NOP instructions into the getInstructionLength API,
      // however this is not possible because we have three different kinds of NOP sizes. Instead we should
      // separate them into three different opcodes NOP2, NOP4, NOP6 and create an API 'isNOP' on instruction
      // and use that instead of checking for the mnemonic wherever it is used.
      if (start->getOpCode().getOpCodeValue() == TR::InstOpCode::NOP)
         {
         size += start->getBinaryLength();
         }
      else
         {
         size += TR::InstOpCode::getInstructionLength(start->getOpCode().getOpCodeValue());
         }
      start = start->getNext();
      }
   size += TR::InstOpCode::getInstructionLength(start->getOpCode().getOpCodeValue());
   //printf("calc code size says %d\n",size);
   return size;
   }

TR::Instruction*
TR_S390Recompilation::generatePrePrologue()
   {
   if (!couldBeCompiledAgain())
      {
      return NULL;
      }

   TR::Compilation* comp = TR::comp();

   TR::CodeGenerator* cg = comp->cg();

   // Associate all generated instructions with the first node
   TR::Node* node = comp->getStartTree()->getNode();

   // Initializing the cursor to NULL ensures instructions will get prepended to the start of the instruction stream
   TR::Instruction* cursor = NULL;

   TR::LabelSymbol* vmCallHelperSnippetLabel = generateLabelSymbol(cg);

   if (cg->mustGenerateSwitchToInterpreterPrePrologue())
      {
      cursor = cg->generateVMCallHelperSnippet(cursor, vmCallHelperSnippetLabel);
      }

   TR::Instruction* padInsertionPoint = cursor;
   TR::Instruction* basrInstruction = NULL;

   TR::S390RIInstruction* addImmediateToJITEPInstruction = NULL;

   if (useSampling())
      {
      TR::LabelSymbol* samplingRecompileMethodSnippetLabel = generateLabelSymbol(cg);

      // Generate the first label by using the placement new operator such that we are guaranteed to call the correct
      // overload of the constructor which can accept a NULL preceding instruction. If cursor is NULL the generated
      // label instruction will be prepended to the start of the instruction stream.
      cursor = new (cg->trHeapMemory()) TR::S390LabelInstruction(TR::InstOpCode::LABEL, node, samplingRecompileMethodSnippetLabel, cursor, cg);

      TR::Instruction* samplingRecompileMethodSnippetLabelInstruction = cursor;

      // Reserve space on stack for entry point register which is used for patching interface calls
      cursor = generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, cg->getStackPointerRealRegister(), -2 * cg->machine()->getGPRSize(), cursor);

      // Save the return address
      cursor = generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, cg->machine()->getS390RealRegister(TR::RealRegister::GPR0), cg->getReturnAddressRealRegister(), cursor);

      TR::MemoryReference * epSaveAreaMemRef = generateS390MemoryReference(cg->getStackPointerRealRegister(), 0, cg);

      // Save the EP register
      cursor = generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, cg->getEntryPointRealRegister(), epSaveAreaMemRef, cursor);

      // Load the EP register with the address of the next instruction
      cursor = generateRRInstruction(cg, TR::InstOpCode::BASR, node, cg->getEntryPointRealRegister(), cg->machine()->getS390RealRegister(TR::RealRegister::GPR0), cursor);

      basrInstruction = cursor;

      cursor = generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, cg->getReturnAddressRealRegister(), cg->getEntryPointRealRegister(), cursor);

      // Displacement will be updated later once we know the offset
      TR::MemoryReference* samplingRecompileMethodAddressMemRef = generateS390MemoryReference(cg->getEntryPointRealRegister(), 0, cg);

      // Load the address of samplingRecompileMethod
      cursor = generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, cg->getEntryPointRealRegister(), samplingRecompileMethodAddressMemRef, cursor);

      // Adjust the return address to point to the JIT entry point. Displacement will be updated later once we know
      // the offset.
      cursor = generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, cg->getReturnAddressRealRegister(), 0, cursor);

      addImmediateToJITEPInstruction = static_cast<TR::S390RIInstruction*>(cursor);

      // Call samplingRecompileMethod
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BCR, node, TR::InstOpCode::COND_BCR, cg->getEntryPointRealRegister(), cursor);

      int32_t padSize = CalcCodeSize(samplingRecompileMethodSnippetLabelInstruction, cursor) % TR::Compiler->om.sizeofReferenceAddress();

      if (padSize != 0)
         {
         padSize = TR::Compiler->om.sizeofReferenceAddress() - padSize;
         }

      // Align to the size of the reference field to ensure alignment of the below data constant for atomic patching
      cursor = cg->insertPad(node, cursor, padSize, false);

      const int32_t offsetFromEPRegisterValueToSamplingRecompileMethod = CalcCodeSize(basrInstruction->getNext(), cursor);

      samplingRecompileMethodAddressMemRef->setOffset(offsetFromEPRegisterValueToSamplingRecompileMethod);

      TR::SymbolReference* helperSymRef = cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390samplingRecompileMethod, false, false, false);

      // AOT relocation for the interpreter glue address
      TR::S390EncodingRelocation* encodingRelocation = new (cg->trHeapMemory()) TR::S390EncodingRelocation(TR_AbsoluteHelperAddress, helperSymRef);

      TR::Compilation* comp = cg->comp();

      AOTcgDiag4(comp, "Add encodingRelocation = %p reloType = %p symbolRef = %p helperId = %x\n", encodingRelocation, encodingRelocation->getReloType(), encodingRelocation->getSymbolReference(), TR_S390samplingRecompileMethod);

      const intptrj_t samplingRecompileMethodAddress = reinterpret_cast<intptrj_t>(helperSymRef->getMethodAddress());

      // Encode the address of the sampling method
      if (TR::Compiler->target.is64Bit())
         {
         cursor = generateDataConstantInstruction(cg, TR::InstOpCode::DC, node, UPPER_4_BYTES(samplingRecompileMethodAddress), cursor);
         cursor->setEncodingRelocation(encodingRelocation);

         cursor = generateDataConstantInstruction(cg, TR::InstOpCode::DC, node, LOWER_4_BYTES(samplingRecompileMethodAddress), cursor);
         }
      else
         {
         cursor = generateDataConstantInstruction(cg, TR::InstOpCode::DC, node, samplingRecompileMethodAddress, cursor);
         cursor->setEncodingRelocation(encodingRelocation);
         }

      // Update the pad insertion point since we have generated the sampling recompile snippet
      padInsertionPoint = cursor;

      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, samplingRecompileMethodSnippetLabel, cursor);
      }

   if (cg->mustGenerateSwitchToInterpreterPrePrologue())
      {
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, vmCallHelperSnippetLabel, cursor);
      }
   else
      {
      // To keep the offsets in PreprologueConst.hpp constant emit a 4 byte pad for simplicity
      cursor = new (cg->trHeapMemory()) TR::S390ImmInstruction(TR::InstOpCode::DC, node, 0xdeadf00d, cursor, cg);
      }

   // The following 4 bytes are used for various patching sequences that overwrite the JIT entry point with a 4 byte
   // branch (BRC) to some location. Before patching in the branch we must save the 4 bytes at the JIT entry point
   // to this location so that we can later reverse the patching at JIT entry point if needed.
   cursor = generateDataConstantInstruction(cg, TR::InstOpCode::DC, node, 0xdeafbeef, cursor);

   TR::Instruction* restoreInstruction = cursor;

   // AOT relocation for the body info
   TR::S390EncodingRelocation* encodingRelocation = new (cg->trHeapMemory()) TR::S390EncodingRelocation(TR_BodyInfoAddress, NULL);

   AOTcgDiag3(comp, "Add encodingRelocation = %p reloType = %p symbolRef = %p\n", encodingRelocation, encodingRelocation->getReloType(), encodingRelocation->getSymbolReference());

   const intptrj_t bodyInfoAddress = reinterpret_cast<intptrj_t>(getJittedBodyInfo());

   // Encode the persistent body info address. Note that we must generate this irregardless of whether we are sampling
   // or not as the counting recompilation generated in the prologue will use this location.
   if (TR::Compiler->target.is64Bit())
      {
      cursor = generateDataConstantInstruction(cg, TR::InstOpCode::DC, node, UPPER_4_BYTES(bodyInfoAddress), cursor);
      cursor->setEncodingRelocation(encodingRelocation);

      bodyInfoDataConstant = cursor;

      cursor = generateDataConstantInstruction(cg, TR::InstOpCode::DC, node, LOWER_4_BYTES(bodyInfoAddress), cursor);
      }
   else
      {
      cursor = generateDataConstantInstruction(cg, TR::InstOpCode::DC, node, bodyInfoAddress, cursor);
      cursor->setEncodingRelocation(encodingRelocation);

      bodyInfoDataConstant = cursor;
      }

   int32_t preprologueSize = CalcCodeSize(cg->getFirstInstruction(), cursor);

   // Adjust the size for the magic header
   preprologueSize += sizeof(int32_t);

   // Adjust the size to point to the JIT entry point
   preprologueSize += _loadArgumentSize;

   int32_t padSize = preprologueSize % TR::Compiler->om.sizeofReferenceAddress();

   if (padSize != 0)
      {
      padSize = TR::Compiler->om.sizeofReferenceAddress() - padSize;
      }

   // Align to the JIT entry point to the size of the reference field
   cg->insertPad(node, padInsertionPoint, padSize, false);

   preprologueSize += padSize;

   // Now that all instructions (including any potential alignment) have been inserted we can adjust the AHI /AGHI
   // displacement to point to the body info address
   if (useSampling())
      {
      const int32_t offsetToBodyInfo = CalcCodeSize(basrInstruction->getNext(), bodyInfoDataConstant->getPrev());

      addImmediateToJITEPInstruction->setSourceImmediate(offsetToBodyInfo);
      }

   // Save the preprologue size to the JIT entry point for use of JIT entry point alignment
   cg->setPreprologueOffset(preprologueSize);

   return cursor;
   }

//-----------------------------------------------------------------------
/*
 * Generate function prologue to support counting recompilation.  Assume that
   generatePreProloge has stuck methodInfo address before magic word
   There are two code gen possibilities: G5 or freeway+.  Freeway allows us to
   use LARL w/ -ve displacement and we can access the methodInfo in preprologue,
   however for simplicity's sake we'll just use the G5 solution.
   See the header file for the details.

 */
//-----------------------------------------------------------------------
TR::Instruction*
TR_S390Recompilation::generatePrologue(TR::Instruction* cursor)
   {
   if (!couldBeCompiledAgain() || useSampling())
      {
      return cursor;
      }

   TR::Compilation* comp = TR::comp();

   TR::CodeGenerator* cg = comp->cg();

   // Associate all generated instructions with the first node
   TR::Node* node = comp->getStartTree()->getNode();

   TR::LabelSymbol* startOfPrologueLabel = generateLabelSymbol(cg);

   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, startOfPrologueLabel, cursor);

   // Reserve space on stack for four register sized spill slots
   cursor = generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, cg->getStackPointerRealRegister(), -4 * cg->machine()->getGPRSize(), cursor);

   TR::MemoryReference* spillSlotMemRef = generateS390MemoryReference(cg->getStackPointerRealRegister(), cg->machine()->getGPRSize(), cg);

   // Spill GPR1 and GPR2
   cursor = generateRSInstruction(cg, TR::InstOpCode::getStoreMultipleOpCode(), node, cg->machine()->getS390RealRegister(TR::RealRegister::GPR1), cg->machine()->getS390RealRegister(TR::RealRegister::GPR2), spillSlotMemRef, cursor);

   // Load the EP register with the address of the next instruction
   cursor = generateRRInstruction(cg, TR::InstOpCode::BASR, node, cg->getEntryPointRealRegister(), cg->machine()->getS390RealRegister(TR::RealRegister::GPR0), cursor);

   TR::Instruction* basrInstruction = cursor;

   // Copy EP register into GPR1
   cursor = generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, cg->machine()->getS390RealRegister(TR::RealRegister::GPR1), cg->getEntryPointRealRegister(), cursor);

   // Use a negative offset since pre-prologue is generated before the prologue
   int32_t offsetToIntEP = -CalcCodeSize(startOfPrologueLabel->getInstruction(), basrInstruction) - _loadArgumentSize;

   // Adjust the EP register to point to the interpreter entry point
   cursor = generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, cg->getEntryPointRealRegister(), offsetToIntEP, cursor);

   // Use a negative offset since pre-prologue is generated before the prologue
   int32_t offsetToBodyInfo = -CalcCodeSize(bodyInfoDataConstant, basrInstruction);

   TR::MemoryReference* bodyInfoAddressMemRef = generateS390MemoryReference(cg->machine()->getS390RealRegister(TR::RealRegister::GPR1), offsetToBodyInfo, cg);

   // Load the body info address. Note the use of getExtendedLoadOpCode because our offset is negative. This means on
   // a 31-bit target we will generate an LY instruction (6-byte) vs. and L instruction (4-byte).
   cursor = generateRXInstruction(cg, TR::InstOpCode::getExtendedLoadOpCode(), node, cg->machine()->getS390RealRegister(TR::RealRegister::GPR1), bodyInfoAddressMemRef, cursor);

   TR::MemoryReference* counterFieldMemRef = generateS390MemoryReference(cg->machine()->getS390RealRegister(TR::RealRegister::GPR1), offsetof(TR_PersistentJittedBodyInfo, _counter), cg);

   // Load the counter field value from the body info pointer
   cursor = generateRXInstruction(cg, TR::InstOpCode::L, node, cg->machine()->getS390RealRegister(TR::RealRegister::GPR2), counterFieldMemRef, cursor);

   // Compare the counter value to zero
   cursor = generateRRInstruction(cg, TR::InstOpCode::LTR, node, cg->machine()->getS390RealRegister(TR::RealRegister::GPR2), cg->machine()->getS390RealRegister(TR::RealRegister::GPR2), cursor);

   TR::LabelSymbol* resumeMethodExecutionLabel = generateLabelSymbol(cg);

   // Resume method execution if counter value >= 0
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNL, node, resumeMethodExecutionLabel, cursor);

   // Load GPR2 with the address of the next instruction
   cursor = generateRRInstruction(cg, TR::InstOpCode::BASR, node, cg->machine()->getS390RealRegister(TR::RealRegister::GPR2), cg->machine()->getS390RealRegister(TR::RealRegister::GPR0), cursor);

   TR::Instruction* basrForCountingRecompileInstruction = cursor;

   // Adjust GPR2 to point to the end of the prologue. Displacement will be updated later once we know the offset.
   cursor = generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, cg->machine()->getS390RealRegister(TR::RealRegister::GPR2), 0, cursor);

   TR::S390RIInstruction* addImmediateToEndOfPrologueInstruction = static_cast<TR::S390RIInstruction*>(cursor);

   TR::MemoryReference* endOfPrologueSlotMemRef = generateS390MemoryReference(cg->getStackPointerRealRegister(), 0, cg);

   // Store GPR2 off to the stack so countingRecompileMethod can access it
   cursor = generateRSInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, cg->machine()->getS390RealRegister(TR::RealRegister::GPR2), endOfPrologueSlotMemRef, cursor);

   // Adjust GPR2 to point to the countingRecompileMethod address
   cursor = generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, cg->machine()->getS390RealRegister(TR::RealRegister::GPR2), 0, cursor);

   TR::S390RIInstruction* addImmediateToCountingRecompileMethodAddressInstruction = static_cast<TR::S390RIInstruction*>(cursor);

   TR::MemoryReference* countingRecompileMethodAddressMemRef = generateS390MemoryReference(cg->machine()->getS390RealRegister(TR::RealRegister::GPR2), 0, cg);

   // Load the countingRecompileMethod address
   cursor = generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, cg->machine()->getS390RealRegister(TR::RealRegister::GPR2), countingRecompileMethodAddressMemRef, cursor);

   // Save the return address in GPR0 so countingRecompileMethod can access it
   cursor = generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, cg->machine()->getS390RealRegister(TR::RealRegister::GPR0), cg->getReturnAddressRealRegister(), cursor);

   // Call countingRecompileMethod
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BCR, node, TR::InstOpCode::COND_BCR, cg->machine()->getS390RealRegister(TR::RealRegister::GPR2), cursor);

   TR::Instruction* countingRecompileMethodAddressDataConstant = NULL;

   TR::SymbolReference* helperSymRef = cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390countingRecompileMethod, false, false, false);

   // AOT relocation for the helper address
   TR::S390EncodingRelocation* encodingRelocation = new (cg->trHeapMemory()) TR::S390EncodingRelocation(TR_AbsoluteHelperAddress, NULL);

   AOTcgDiag4(comp, "Add encodingRelocation = %p reloType = %p symbolRef = %p helperId = %x\n", encodingRelocation, encodingRelocation->getReloType(), encodingRelocation->getSymbolReference(), TR_S390countingRecompileMethod);

   const intptrj_t countingRecompileMethodAddress = reinterpret_cast<intptrj_t>(helperSymRef->getMethodAddress());

   if (TR::Compiler->target.is64Bit())
      {
      cursor = generateDataConstantInstruction(cg, TR::InstOpCode::DC, node, UPPER_4_BYTES(countingRecompileMethodAddress), cursor);
      cursor->setEncodingRelocation(encodingRelocation);

      countingRecompileMethodAddressDataConstant = cursor;

      cursor = generateDataConstantInstruction(cg, TR::InstOpCode::DC, node, LOWER_4_BYTES(countingRecompileMethodAddress), cursor);
      }
   else
      {
      cursor = generateDataConstantInstruction(cg, TR::InstOpCode::DC, node, countingRecompileMethodAddress, cursor);
      cursor->setEncodingRelocation(encodingRelocation);

      countingRecompileMethodAddressDataConstant = cursor;
      }

   // When a method gets recompiled the J9::Recompilation::methodHasBeenRecompiled API is called to patch the JIT
   // entry point of the now stale method body to branch to the recompiled method. The noted API will patch the JIT
   // entry point to branch to patchCallerBranchToCountingRecompiledMethodLabel which ends up calling a shared
   // assembly stub which takes care of patching the callers call instruction to point to the address of the
   // recompiled method body.
   TR::LabelSymbol* patchCallerBranchToCountingRecompiledMethodLabel = generateLabelSymbol(cg);

   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, patchCallerBranchToCountingRecompiledMethodLabel, cursor);

   // Reserve space on stack for the EP register as it may be used for patching of caller interface calls
   cursor = generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, cg->getStackPointerRealRegister(), -2 * cg->machine()->getGPRSize(), cursor);

   TR::MemoryReference* epRegisterSlotMemRef = generateS390MemoryReference(cg->getStackPointerRealRegister(), 0, cg);

   // Save EP register
   cursor = generateRSInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, cg->getEntryPointRealRegister(), epRegisterSlotMemRef, cursor);

   // Save the return address in GPR0 so the patching assembly stub can access it
   cursor = generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, cg->machine()->getS390RealRegister(TR::RealRegister::GPR0), cg->getReturnAddressRealRegister(), cursor);

   // Load GPR14 with the address of the next instruction
   cursor = generateRRInstruction(cg, TR::InstOpCode::BASR, node, cg->getReturnAddressRealRegister(), cg->machine()->getS390RealRegister(TR::RealRegister::GPR0), cursor);

   basrInstruction = cursor;

   TR::MemoryReference* countingPatchCallSiteAddressMemRef = generateS390MemoryReference(cg->getReturnAddressRealRegister(), 0, cg);

   // Load the countingPatchCallSite address
   cursor = generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, cg->getEntryPointRealRegister(), countingPatchCallSiteAddressMemRef, cursor);

   // Use a negative offset since pre-prologue is generated before the prologue
   offsetToIntEP = -CalcCodeSize(startOfPrologueLabel->getInstruction(), basrInstruction) - _loadArgumentSize;

   // Adjust the return address register to point to the interpreter entry point
   cursor = generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, cg->getReturnAddressRealRegister(), offsetToIntEP, cursor);

   TR::MemoryReference* intEPAddressSlotMemRef = generateS390MemoryReference(cg->getStackPointerRealRegister(), cg->machine()->getGPRSize(), cg);

   // Store the interpreter entry point address to the stack in case countingPatchCallSite needs to revert to the
   // recompileMethod helper in case the J9Method.extra field contains an unexpected value
   cursor = generateRXInstruction(cg, TR::InstOpCode::getStoreOpCode(), node, cg->getReturnAddressRealRegister(), intEPAddressSlotMemRef, cursor);

   // Use a negative offset since pre-prologue is generated before the prologue
   offsetToBodyInfo = -CalcCodeSize(bodyInfoDataConstant, startOfPrologueLabel->getInstruction()) + _loadArgumentSize;

   // Adjust the return address register to point to the body info address. Note that we already adjusted the return
   // address register previously to point to the interpreter entry point.
   cursor = generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, cg->getReturnAddressRealRegister(), offsetToBodyInfo, cursor);

   bodyInfoAddressMemRef = generateS390MemoryReference(cg->getReturnAddressRealRegister(), 0, cg);

   // Load the body info address
   cursor = generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, cg->getReturnAddressRealRegister(), bodyInfoAddressMemRef, cursor);

   // Call countingPatchCallSite
   cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BCR, node, TR::InstOpCode::COND_BCR, cg->getEntryPointRealRegister(), cursor);

   TR::Instruction* countingPatchCallSiteAddressDataConstant = NULL;

   helperSymRef = cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390countingPatchCallSite, false, false, false);

   // AOT relocation for the helper address
   encodingRelocation = new (cg->trHeapMemory()) TR::S390EncodingRelocation(TR_AbsoluteHelperAddress, NULL);

   AOTcgDiag4(comp, "Add encodingRelocation = %p reloType = %p symbolRef = %p helperId = %x\n", encodingRelocation, encodingRelocation->getReloType(), encodingRelocation->getSymbolReference(), TR_S390countingPatchCallSite);

   const intptrj_t countingPatchCallSiteAddress = reinterpret_cast<intptrj_t>(helperSymRef->getMethodAddress());

   if (TR::Compiler->target.is64Bit())
      {
      cursor = generateDataConstantInstruction(cg, TR::InstOpCode::DC, node, UPPER_4_BYTES(countingPatchCallSiteAddress), cursor);
      cursor->setEncodingRelocation(encodingRelocation);

      countingPatchCallSiteAddressDataConstant = cursor;

      cursor = generateDataConstantInstruction(cg, TR::InstOpCode::DC, node, LOWER_4_BYTES(countingPatchCallSiteAddress), cursor);
      }
   else
      {
      cursor = generateDataConstantInstruction(cg, TR::InstOpCode::DC, node, countingPatchCallSiteAddress, cursor);
      cursor->setEncodingRelocation(encodingRelocation);

      countingPatchCallSiteAddressDataConstant = cursor;
      }

   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, resumeMethodExecutionLabel, cursor);

   spillSlotMemRef = generateS390MemoryReference(*spillSlotMemRef, 0, cg);

   // Fill GPR1 and GPR2
   cursor = generateRSInstruction(cg, TR::InstOpCode::getLoadMultipleOpCode(), node, cg->machine()->getS390RealRegister(TR::RealRegister::GPR1), cg->machine()->getS390RealRegister(TR::RealRegister::GPR2), spillSlotMemRef, cursor);

   // Free space on stack for four register sized spill slots
   cursor = generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, cg->getStackPointerRealRegister(), 4 * cg->machine()->getGPRSize(), cursor);

   TR::LabelSymbol* endOfPrologueLabel = generateLabelSymbol(cg);

   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, endOfPrologueLabel, cursor);

   // Now that all instructions have been inserted we can adjust the two add immediate instructions which point to
   // labels or data constants generated in the future
   int32_t offsetToEndOfPrologue = CalcCodeSize(basrForCountingRecompileInstruction->getNext(), endOfPrologueLabel->getInstruction());

   addImmediateToEndOfPrologueInstruction->setSourceImmediate(offsetToEndOfPrologue);

   int32_t offsetToCountingRecompileMethodAddress = -CalcCodeSize(countingRecompileMethodAddressDataConstant, endOfPrologueLabel->getInstruction());

   addImmediateToCountingRecompileMethodAddressInstruction->setSourceImmediate(offsetToCountingRecompileMethodAddress);

   int32_t offsetToCountingPatchCallSiteAddress = CalcCodeSize(basrInstruction->getNext(), countingPatchCallSiteAddressDataConstant->getPrev());

   countingPatchCallSiteAddressMemRef->setOffset(offsetToCountingPatchCallSiteAddress);

   return cursor;
   }

void TR_S390Recompilation::postCompilation()
   {
   // if won't be recompiled, no need for saving code for patch
   if(!couldBeCompiledAgain()) return;

   uint8_t  *startPC = _compilation->cg()->getCodeStart();
   TR_LinkageInfo *linkageInfo = TR_LinkageInfo::get(startPC);
   int32_t jitEntryOffset = linkageInfo->getReservedWord() & 0x0ffff;
   uint32_t * jitEntryPoint = (uint32_t*)(startPC + jitEntryOffset);
   uint32_t * saveLocn = (uint32_t*)(startPC + OFFSET_INTEP_JITEP_SAVE_RESTORE_LOCATION);


/*
      printf("RC>>Saved entry point %p(%x) to saveLocn %p(%x)\n",jitEntryPoint,*jitEntryPoint,
                  saveLocn,*saveLocn);
*/
   saveJitEntryPoint(startPC, (uint8_t*)jitEntryPoint);

   TR_ASSERT((*saveLocn == 0xdeafbeef || *saveLocn == *jitEntryPoint),"Unexpect value %x(%p) != 0xdeafbeef or %x\n",
            *saveLocn,saveLocn,*jitEntryPoint);

   if (debug("traceRecompilation"))
      {
      diagnostic("RC>>Saved entry point %p(%x) to saveLocn %p(%x)\n",jitEntryPoint,*jitEntryPoint,
                  saveLocn,*saveLocn);
      }
   }
