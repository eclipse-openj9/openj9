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

#ifndef J9_Z_TREE_EVALUATOR_INCL
#define J9_Z_TREE_EVALUATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_TREE_EVALUATOR_CONNECTOR
#define J9_TREE_EVALUATOR_CONNECTOR
namespace J9 { namespace Z { class TreeEvaluator; } }
namespace J9 { typedef J9::Z::TreeEvaluator TreeEvaluatorConnector; }
#else
#error J9::Z::TreeEvaluator expected to be a primary connector, but a J9 connector is already defined
#endif

#include "codegen/Snippet.hpp"
#include "compiler/codegen/J9TreeEvaluator.hpp"  // include parent

#define INSN_HEAP cg->trHeapMemory()

namespace J9
{

namespace Z
{

class OMR_EXTENSIBLE TreeEvaluator: public J9::TreeEvaluator
   {
   public:

   static TR::Register *monentEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *monexitEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *monexitfenceEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *asynccheckEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *instanceofEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *checkcastEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *checkcastAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *generateHelperCallForVMNewEvaluators(TR::Node *node, TR::CodeGenerator *cg, bool doInlineAllocation = false, TR::Register *resReg = NULL);
   static TR::Register *newObjectEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *newArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *anewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *multianewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraylengthEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *resolveCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *DIVCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *BNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ArrayCopyBNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *BNDCHKwithSpineCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ArrayStoreCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ArrayCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *conditionalHelperEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static float interpreterProfilingInstanceOfOrCheckCastTopProb(TR::CodeGenerator * cg, TR::Node * node);

   /**
    * \brief
    * In concurrent scavenge (CS) mode, this is used to evaluate ardbari nodes and inlined-compare-and-swap to
    * generate a guarded load software sequence that is functionally equivalent to LGG/LLGFSG instructions on z.
    *
    * \param node
    *       the read barrier node to evaluate
    *
    * \param cg
    *       the code generator object
    *
    * \param resultReg
    *       the read barrier's result register
    *
    * \param memRef
    *       a memory reference to the reference field to load from
    *
    * \param deps
    *       A register dependency condition pointer used when the software read barrier, which itself is an internal
    *       control flow (ICF), is built inside another ICF. This is the outer ICF's dependency conditions.
    *       When the deps parameter is not NULL, this evaluator helper does not build its own dependencies. Instead,
    *       it builds on top of this outer deps.
    *
    * \param produceUnshiftedValue
    *       A flag to make this evaluator produce unshifted reference loads for compressed reference Java.
    *       For compressed reference builds, a guarded reference field load normally produces a
    *       reference (64-bit value).
    *       However, for compressed reference array copy, it is faster to do 32-bit load, 32-bit range check,
    *       and 32-bit stores. Hence, eliminating the need for shift instructions (compression and decompression).
    *
    * \return
    *       returns a register that contains the reference field.
   */
   static TR::Register *generateSoftwareReadBarrier(TR::Node* node,
                                                    TR::CodeGenerator* cg,
                                                    TR::Register* resultReg,
                                                    TR::MemoryReference* memRef,
                                                    TR::RegisterDependencyConditions* deps = NULL,
                                                    bool produceUnshiftedValue = false);

   /** \brief
    *     Evaluates a reference arraycopy node. If software concurrent scavenge is not enabled, it generates an
    *     MVC memory-memory copy for a forward arraycopy and a
    *     loop based on the reference size for a backward arraycopy. For runtimes which support it, this function will
    *     also generate write barrier checks on \p byteSrcObjNode and \p byteDstObjNode.
    *
    *     When software concurrent scavenge is enabled, it generates a internal control flow that first checks if the current
    *     execution is in the GC cycle and performs either a loop-based array copy or a helper call.
    *
    *  \param node
    *     The reference arraycopy node.
    *
    *  \param cg
    *     The code generator used to generate the instructions.
    */
   static TR::Register *referenceArraycopyEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraycopyEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *generateConstantLengthReferenceArrayCopy(TR::Node *node, TR::CodeGenerator *cg);

   static void generateLoadAndStoreForArrayCopy(TR::Node *node, TR::CodeGenerator *cg, TR::MemoryReference *srcMemRef,
                                                TR::MemoryReference *dstMemRef,
                                                TR_S390ScratchRegisterManager *srm,
                                                TR::DataType elenmentType,
                                                bool needsGuardedLoad,
                                                TR::RegisterDependencyConditions* deps = NULL);

   static void forwardArrayCopySequenceGenerator(TR::Node *node, TR::CodeGenerator *cg,
                                                           TR::Register *byteSrcReg, TR::Register *byteDstReg,
                                                           TR::Register *byteLenReg, TR::Node *byteLenNode,
                                                           TR_S390ScratchRegisterManager *srm, TR::LabelSymbol *mergeLabel);
   static TR::RegisterDependencyConditions * backwardArrayCopySequenceGenerator(TR::Node *node, TR::CodeGenerator *cg,
                                                            TR::Register *byteSrcReg, TR::Register *byteDstReg,
                                                            TR::Register *byteLenReg, TR::Node *byteLenNode,
                                                            TR_S390ScratchRegisterManager *srm, TR::LabelSymbol *mergeLabel);



   /*           START BCD Evaluators          */
   // Used for VPSOP in vector setSign operations
   enum SignOperationType
      {
      maintain,     // maintain sign
      complement,   // complement sign
      setSign       // force positive or negative
      };

   static TR::Register *df2zdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zd2ddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdsls2zdEvaluator(TR::Node * node, TR::CodeGenerator * cg);
   static TR::Register *zd2zdslsEvaluator(TR::Node * node, TR::CodeGenerator * cg);
   static TR::Register *pd2zdslsEvaluator(TR::Node * node, TR::CodeGenerator * cg);
   static TR::Register *zdsls2pdEvaluator(TR::Node * node, TR::CodeGenerator * cg);
   static void zonedToZonedSeparateSignHelper(TR::Node *node, TR_PseudoRegister *srcReg, TR_PseudoRegister *targetReg, TR::MemoryReference *sourceMR, TR::MemoryReference *destMR, TR::CodeGenerator * cg);
   static TR::MemoryReference *packedToZonedHelper(TR::Node *node, TR_PseudoRegister *targetReg, TR::MemoryReference *sourceMR, TR_PseudoRegister *childReg, TR::CodeGenerator * cg);
   static void pd2zdSignFixup(TR::Node *node, TR::MemoryReference *destMR, TR::CodeGenerator * cg);

   static void zonedSeparateSignToPackedOrZonedHelper(TR::Node *node, TR_PseudoRegister *targetReg, TR::MemoryReference *sourceMR, TR::MemoryReference *destMR, TR::CodeGenerator * cg);
   static TR::Register *zdsle2zdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::MemoryReference *zonedToPackedHelper(TR::Node *node, TR_PseudoRegister *targetReg, TR::MemoryReference *sourceMR, TR_PseudoRegister *childReg, TR::CodeGenerator * cg);
   static TR::MemoryReference *packedToUnicodeHelper(TR::Node *node, TR_PseudoRegister *targetReg, TR::MemoryReference *sourceMR, TR_PseudoRegister *childReg, bool isSeparateSign, TR::CodeGenerator * cg, TR_StorageReference* srcStorageReference);
   static TR::MemoryReference *asciiAndUnicodeToPackedHelper(TR::Node *node, TR_PseudoRegister *targetReg, TR::MemoryReference *sourceMR, TR_PseudoRegister *childReg, TR::CodeGenerator * cg);

   /*   PD -> UD/ZD   */
   static TR::Register *pd2udslEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2udslEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2udslVectorEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2udEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2udEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2udVectorEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2zdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2zdVectorEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg);

   /*   PD <- UD/ZD   */
   static TR::Register *ud2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ud2pdVectorEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udsl2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zd2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zd2pdVectorEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg);

   static bool isZonedOperationAnEffectiveNop(TR::Node * node, int32_t shiftAmount, bool isTruncation, TR_PseudoRegister *srcReg, bool isSetSign, int32_t sign,TR::CodeGenerator * cg);

   
   static TR::Register *BCDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *BCDCHKEvaluatorImpl(TR::Node * node,
                                            TR::CodeGenerator * cg,
                                            uint32_t numCallParam,
                                            uint32_t callChildStartIndex,
                                            bool isResultPD,
                                            bool isUseVector,
                                            bool isVariableParam);

   static TR::Register *df2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2ddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *i2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register* pd2lVariableEvaluator(TR::Node* node, TR::CodeGenerator* cg, bool isUseVectorBCD);
   static TR::Register *generatePackedToBinaryConversion(TR::Node * node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator * cg);
   static TR::Register *generateVectorPackedToBinaryConversion(TR::Node * node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator * cg);
   static TR::Register *generateBinaryToPackedConversion(TR::Node * node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator * cg);
   static TR::Register *generateVectorBinaryToPackedConversion(TR::Node * node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator * cg);

   static void correctPackedArithmeticPrecision(TR::Node *node, int32_t op1EncodingSize, TR_PseudoRegister *targetReg, int32_t computedResultPrecision, TR::CodeGenerator * cg);

   static TR::Register *pdaddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdsubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdaddsubEvaluatorHelper(TR::Node *node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator *cg);
   static TR::Register *pdArithmeticVectorEvaluatorHelper(TR::Node *node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator *cg);

   static TR::Register *pdmulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdmulEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *pddivremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pddivremEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pddivremVectorEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *pdnegEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static void clearAndSetSign(TR::Node *node,
                               TR_PseudoRegister *targetReg,
                               int32_t leftMostByteForClear,
                               int32_t digitsToClear,
                               TR::MemoryReference *destMR,
                               TR_PseudoRegister *srcReg,
                               TR::MemoryReference *sourceMR,
                               bool isSetSign,
                               int32_t sign,
                               bool signCodeIsInitialized,
                               TR::CodeGenerator *cg);

   static TR_PseudoRegister *simpleWideningOrTruncation(TR::Node *node,
                                                        TR_PseudoRegister *srcReg,
                                                        bool isSetSign,
                                                        int32_t sign,
                                                        TR::CodeGenerator *cg);

   static TR::Register *pdshrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdshrEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdshrVectorEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *pdshlEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdshiftEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg, bool isRightShift);

   static TR::Register *pdModifyPrecisionEvaluator(TR::Node * node, TR::CodeGenerator * cg);
   static TR::Register *pdshlVectorEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg);

   /** \brief
    *     Helper to generate a VPSOP instruction.
    *
    *  \param node 
    *     The node to which the instruction is associated.
    *  \param cg 
    *     The codegen object.
    *  \param setPrecision
    *     Determines whether the VPSOP instruction will set precision.
    *  \param precision
    *     The new precision value.
    *  \param signedStatus
    *     Determines whether positive results will carry the preferred positive sign 0xC if true or 0xF if false.
    *  \param soType
    *     See enum SignOperationType.
    *  \param signValidityCheck
    *     Checks if originalSignCode is a valid sign.
    *  \param digitValidityCheck
    *     Validate the input digits
    *  \param sign
    *     The new sign. Used if signOpType is SignOperationType::setSign. Possible values include:
    *       - positive: 0xA, 0xC 
    *       - negative: 0xB, 0xD
    *       - unsigned: 0xF
    *  \param setConditionCode
    *     Determines if this instruction sets ConditionCode or not.
    *  \param ignoreDecimalOverflow
    *     Turns on the Instruction Overflow Mask (IOM) so no decimal overflow is triggered
    */
   static TR::Register *vectorPerformSignOperationHelper(TR::Node *node,
                                                         TR::CodeGenerator *cg,
                                                         bool setPrecision,
                                                         uint32_t precision,
                                                         bool signedStatus,
                                                         SignOperationType soType,
                                                         bool signValidityCheck = false,
                                                         bool digitValidityCheck = true,
                                                         int32_t sign = 0,
                                                         bool setConditionCode = true,
                                                         bool ignoreDecimalOverflow = false);

   static TR::Register *pdchkEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *pdcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdcmpVectorEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *zdabsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdloadEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdloadVectorEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdstoreEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdstoreVectorEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR_OpaquePseudoRegister *evaluateValueModifyingOperand(TR::Node * node, bool initTarget, TR::MemoryReference *sourceMR, TR::CodeGenerator * cg, bool trackSignState=false, int32_t srcSize=0, bool alwaysLegalToCleanSign=false);
   static TR_PseudoRegister *evaluateBCDValueModifyingOperand(TR::Node * node, bool initTarget, TR::MemoryReference *sourceMR, TR::CodeGenerator * cg, bool trackSignState=false, int32_t srcSize=0, bool alwaysLegalToCleanSign=false);
   static TR_OpaquePseudoRegister *evaluateSignModifyingOperand(TR::Node *node, bool isEffectiveNop, bool isNondestructiveNop, bool initTarget, TR::MemoryReference *sourceMR, TR::CodeGenerator *cg);
   static TR_PseudoRegister *evaluateBCDSignModifyingOperand(TR::Node *node, bool isEffectiveNop, bool isNondestructiveNop, bool initTarget, TR::MemoryReference *sourceMR, TR::CodeGenerator *cg);

   static TR::Register *pdclearEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdSetSignHelper(TR::Node *node, int32_t sign, TR::CodeGenerator *cg);
   static TR::Register *pdSetSignEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   /*           END BCD Evaluators          */


   /*           START DFP evaluators          */
   static TR::Register *deconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *deloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *destoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *deRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *deRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *df2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2dfEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *i2ddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2ddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lu2ddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dd2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dd2luEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *df2ddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *df2deEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dd2dfEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dd2deEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *de2ddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *de2dxHelperAndSetRegister(TR::Register *targetReg, TR::Register *srcReg, TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dfaddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddaddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *deaddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dfsubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddsubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *desubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dfmulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddmulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *demulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dfdivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifddcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifddcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifddcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifddcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifddcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifddcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifddcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifddcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifddcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifddcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifddcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifddcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddnegEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *ddInsExpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dffloorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddfloorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *defloorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddshlEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddshrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddshrRoundedEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddSetNegativeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddModifyPrecisionEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddcleanEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *decleanEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dd2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *de2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   /*           END DFP evaluators          */


   static TR::Register *countDigitsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static void          countDigitsHelper(TR::Node * node, TR::CodeGenerator * cg,
                                          int32_t memRefIndex, TR::MemoryReference * memRef,
                                          TR::Register* inputReg, TR::Register* countReg,
                                          TR::LabelSymbol *doneLabel, bool isLong);

   static TR::Register *tstartEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *tfinishEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *tabortEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Instruction    *generateRuntimeInstrumentationOnOffSequence(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node, TR::Instruction *preced = NULL, bool postRA = false);
   static TR::Instruction* genLoadForObjectHeaders      (TR::CodeGenerator *cg, TR::Node *node, TR::Register *reg, TR::MemoryReference *tempMR, TR::Instruction *iCursor);
   static TR::Instruction* genLoadForObjectHeadersMasked(TR::CodeGenerator *cg, TR::Node *node, TR::Register *reg, TR::MemoryReference *tempMR, TR::Instruction *iCursor);
   static TR::Instruction    *generateVFTMaskInstruction(TR::Node *node, TR::Register *reg, TR::CodeGenerator *cg, TR::Instruction *preced = NULL);
   static TR::Register *VMifInstanceOfEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static bool         VMinlineCallEvaluator(TR::Node *node, bool, TR::CodeGenerator *cg);

/** \brief
    *  Generates Sequence to check and use Guarded Load for ArrayCopy
    *  
    * \param node
    *     The arraycopy node.
    *
    *  \param cg
    *     The code generator used to generate the instructions.
    * 
    *  \param byteSrcReg
    *     Register holding starting address of source
    *
    *  \param byteDstReg
    *     Register holding starting address of destination
    *
    *  \param byteLenReg
    *     Register holding number of bytes to copy
    * 
    *  \param mergeLabel
    *     Label Symbol to merge from generated OOL sequence
    * 
    *  \param srm
    *     Scratch Register Manager providing pool of scratch registers to use
    * 
    *  \param isForward
    *     Boolean specifying if we need to copy elements in forward direction
    * 
    */
   static void         genGuardedLoadOOL(TR::Node *node, TR::CodeGenerator *cg, TR::Register *byteSrcReg, TR::Register *byteDstReg, TR::Register *byteLenReg, TR::LabelSymbol *mergeLabel, TR_S390ScratchRegisterManager *srm, bool isForward);
   static void         genArrayCopyWithArrayStoreCHK(TR::Node *node, TR::Register *srcObjReg, TR::Register *dstObjReg, TR::Register *srcAddrReg, TR::Register *dstAddrReg, TR::Register *lengthReg, TR::CodeGenerator *cg);
   static void         genWrtbarForArrayCopy(TR::Node *node, TR::Register *srcObjReg, TR::Register *dstObjReg, bool srcNonNull, TR::CodeGenerator *cg);
   static TR::Register *VMgenCoreInstanceofEvaluator(TR::Node *node, TR::CodeGenerator *cg, TR::LabelSymbol * falseLabel, TR::LabelSymbol * trueLabel, bool needsResult, bool trueFallThrough, TR::RegisterDependencyConditions * conditions, bool isIfInstanceOf=false);
   static TR::Register *VMgenCoreInstanceofEvaluator2(TR::Node *node, TR::CodeGenerator *cg, TR::LabelSymbol * trueLabel, TR::LabelSymbol * falseLabel, bool initialResult, bool needResult, TR::RegisterDependencyConditions * conditions, bool ifInstanceOf=false);
   static TR::Register *VMinstanceOfEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMinstanceOfEvaluator2(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMifInstanceOfEvaluator2(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMcheckcastEvaluator2(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMcheckcastEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMmonentEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMmonexitEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMnewEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMarrayCheckEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static void         restoreGPR7(TR::Node *node, TR::CodeGenerator *cg);

   /*
    * Generate instructions for static/instance field access report.
    */ 
   static void generateTestAndReportFieldWatchInstructions(TR::CodeGenerator *cg, TR::Node *node, TR::Snippet *dataSnippet, bool isWrite, TR::Register *sideEffectRegister, TR::Register *valueReg);

   /*
    * Generate instructions to fill in the J9JITWatchedStaticFieldData.fieldAddress, J9JITWatchedStaticFieldData.fieldClass for static fields,
    * and J9JITWatchedInstanceFieldData.offset for instance fields at runtime. Used for fieldwatch support.
    */
   static void generateFillInDataBlockSequenceForUnresolvedField (TR::CodeGenerator *cg, TR::Node *node, TR::Snippet *dataSnippet, bool isWrite, TR::Register *sideEffectRegister);
   static TR::Register *irdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *irdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ardbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ardbariEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *awrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *awrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   };
}

}

#endif
