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

#ifndef J9_X86_TREE_EVALUATOR_INCL
#define J9_X86_TREE_EVALUATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_TREE_EVALUATOR_CONNECTOR
#define J9_TREE_EVALUATOR_CONNECTOR
namespace J9 { namespace X86 { class TreeEvaluator; } }
namespace J9 { typedef J9::X86::TreeEvaluator TreeEvaluatorConnector; }
#endif

#include "codegen/Snippet.hpp"
#include "compiler/codegen/J9TreeEvaluator.hpp"  // include parent

namespace J9
{

namespace X86
{

class OMR_EXTENSIBLE TreeEvaluator: public J9::TreeEvaluator
   {
   public:

   static TR::Register *writeBarrierEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *monentEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *monexitEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *asynccheckEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *newEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *multianewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraycopyEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraylengthEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *exceptionRangeFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *NULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ZEROCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *resolveAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *evaluateNULLCHKWithPossibleResolve(TR::Node *node, bool needResolution, TR::CodeGenerator *cg);
   static TR::Register *DIVCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *BNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ArrayCopyBNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *BNDCHKwithSpineCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ArrayStoreCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ArrayCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *barrierFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *atccheckEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ScopeCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *readbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerHighestOneBit(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerLowestOneBit(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerNumberOfLeadingZeros(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerNumberOfTrailingZeros(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerBitCount(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *longHighestOneBit(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *longLowestOneBit(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *longNumberOfLeadingZeros(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *longNumberOfTrailingZeros(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *longBitCount(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *tstartEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *tfinishEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *tabortEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static void generateVFTMaskInstruction(TR::Node *node, TR::Register *reg, TR::CodeGenerator *cg);
   static bool VMinlineCallEvaluator(TR::Node *node, bool isIndirect, TR::CodeGenerator *cg);
   static TR::Register *VMmonentEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMmonexitEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMnewEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMarrayCheckEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *checkcastinstanceofEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static void asyncGCMapCheckPatching(TR::Node *node, TR::CodeGenerator *cg, TR::LabelSymbol *snippetLabel);
   static void inlineRecursiveMonitor(TR::Node *node, TR::CodeGenerator *cg, TR::LabelSymbol *startLabel, TR::LabelSymbol *snippetLabel, TR::LabelSymbol *JITMonitorEnterSnippetLabel, TR::Register *objectReg, int lwoffset, TR::LabelSymbol *snippetRestartLabel, bool reservingLock);

   /*
   * \brief
   *     Generates the sequence to handle cases where the monitor object
   *     is value type or value based class type
   *
   * \param node
   *     the monitor enter/exit node
   *
   * \param snippetLabel
   *     the label for OOL code calling VM monitor enter/exit helpers
   *
   * \details
   *     Call the VM helper if it's detected at runtime that the monitor object
   *     is value type or value based class type.
   *     The VM helper throws appropriate IllegalMonitorStateException for value type
   *     and VirtualMachineError for value based class type.
   *
   * \note
   *     This method only handles the cases where, at compile time, it's unknown whether the
   *     object is reference type or value type or value based class type.
   */
   static void generateCheckForValueMonitorEnterOrExit(TR::Node *node, int32_t classFlag, TR::LabelSymbol *snippetLabel, TR::CodeGenerator *cg);
   static void transactionalMemoryJITMonitorEntry(TR::Node *node, TR::CodeGenerator *cg, TR::LabelSymbol *startLabel, TR::LabelSymbol *snippetLabel, TR::LabelSymbol *JITMonitorEnterSnippetLabel, TR::Register *objectReg, int lwoffset);
   static void generateValueTracingCode(TR::Node *node, TR::Register *vmThreadReg, TR::Register *scratchReg, TR::Register *valueRegHigh, TR::Register *valueRegLow, TR::CodeGenerator *cg);
   static void generateValueTracingCode(TR::Node *node, TR::Register *vmThreadReg, TR::Register *scratchReg, TR::Register *valueReg, TR::CodeGenerator *cg);
   static bool monEntryExitHelper(bool entry, TR::Node* node, bool reservingLock, bool normalLockPreservingReservation, TR_RuntimeHelper &helper, TR::CodeGenerator* cg);
   static void VMarrayStoreCHKEvaluator(TR::Node *, TR::Node *, TR::Node *, TR_X86ScratchRegisterManager *, TR::LabelSymbol *, TR::Instruction *, TR::CodeGenerator *cg);
   static void VMwrtbarRealTimeWithoutStoreEvaluator(TR::Node *node, TR::MemoryReference *storeMRForRealTime, TR::Register *stoerAddressRegForRealTime, TR::Node *destOwningObject, TR::Node *sourceObject, TR::Register *srcReg, TR_X86ScratchRegisterManager *scratchRegisterManager, TR::CodeGenerator *cg);
   static void VMwrtbarWithoutStoreEvaluator(TR::Node *node, TR::Node *destOwningObject, TR::Node *sourceObject, TR::Register *srcReg, TR_X86ScratchRegisterManager *scratchRegisterManager, TR::CodeGenerator *cg);
   static void VMwrtbarWithStoreEvaluator(TR::Node *node, TR::MemoryReference  *storeMR, TR_X86ScratchRegisterManager *, TR::Node *destinationChild, TR::Node *sourceChild, bool isIndirect, TR::CodeGenerator *cg, bool nullAdjusted = false);

   /*
    * Generate instructions for static/instance field access report.
    * @param dataSnippetRegister: Optional, can be used to pass the address of the snippet inside the register.
    */
   static void generateTestAndReportFieldWatchInstructions(TR::CodeGenerator *cg, TR::Node *node, TR::Snippet *dataSnippet, bool isWrite, TR::Register *sideEffectRegister, TR::Register *valueReg, TR::Register *dataSnippetRegister);

   /*
    * Generates instructions to fill in the J9JITWatchedStaticFieldData.fieldAddress, J9JITWatchedStaticFieldData.fieldClass for static fields,
    * and J9JITWatchedInstanceFieldData.offset for instance fields at runtime. Used for fieldwatch support.
    * @param dataSnippetRegister: Optional, can be used to pass the address of the snippet inside the register.
    */
   static void generateFillInDataBlockSequenceForUnresolvedField (TR::CodeGenerator *cg, TR::Node *node, TR::Snippet *dataSnippet, bool isWrite, TR::Register *sideEffectRegister, TR::Register *dataSnippetRegister);
   static TR::Register *directCallEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *encodeUTF16Evaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *compressStringEvaluator(TR::Node *node, TR::CodeGenerator *cg, bool japaneseMethod);
   static TR::Register *compressStringNoCheckEvaluator(TR::Node *node, TR::CodeGenerator *cg, bool japaneseMethod);
   static TR::Register *andORStringEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *toUpperIntrinsicUTF16Evaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *toLowerIntrinsicUTF16Evaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *toUpperIntrinsicLatin1Evaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *toLowerIntrinsicLatin1Evaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *irdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *irdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ardbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ardbariEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *fwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *awrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *awrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *inlineStringLatin1Inflate(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *generateConcurrentScavengeSequence(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fpConvertToLong(TR::Node *node, TR::SymbolReference *helperSymRef, TR::CodeGenerator *cg);
   static TR::Register *f2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   class CaseConversionManager;
   static TR::Register *stringCaseConversionHelper(TR::Node *node, TR::CodeGenerator *cg, CaseConversionManager& manager);

   private:
   static TR::Register* performHeapLoadWithReadBarrier(TR::Node* node, TR::CodeGenerator* cg);
   };

}

}

#endif
