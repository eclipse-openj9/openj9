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
#include "il/MethodSymbol.hpp"

#define INSN_HEAP cg->trHeapMemory()

namespace J9
{

namespace Z
{

class OMR_EXTENSIBLE TreeEvaluator: public J9::TreeEvaluator
   {
   public:

   static void inlineEncodeASCII(TR::Node *node, TR::CodeGenerator *cg);

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
   static TR::Register *inlineCurrentTimeMaxPrecision(TR::CodeGenerator *cg, TR::Node *node);
   /**
    * generate a single precision sqrt instruction
    */
   static TR::Register *inlineSinglePrecisionSQRT(TR::Node *node, TR::CodeGenerator *cg);
   /*
    * Inline Java's (Java 11 onwards) StringLatin1.inflate([BI[CII)V
    */
   static TR::Register *inlineStringLatin1Inflate(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMinlineCompareAndSwap( TR::Node *node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic casOp, bool isObj);
   static TR::Register *inlineAtomicOps(TR::Node *node, TR::CodeGenerator *cg, int8_t size, TR::MethodSymbol *method, bool isArray = false);
   static TR::Register *inlineAtomicFieldUpdater(TR::Node *node, TR::CodeGenerator *cg, TR::MethodSymbol *method);
   static TR::Register *inlineKeepAlive(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *inlineConcurrentLinkedQueueTMOffer(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *inlineConcurrentLinkedQueueTMPoll(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *toUpperIntrinsic(TR::Node *node, TR::CodeGenerator *cg, bool isCompressedString);
   static TR::Register *toLowerIntrinsic(TR::Node *node, TR::CodeGenerator *cg, bool isCompressedString);

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
    * inside the source/main string. If the first character is located, it'll
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
    * com.ibm.jit.JITHelpers.intrinsicIndexOfStringLatin1(Object s1Value, int s1len, Object s2Value, int s2len, int start);
    * com.ibm.jit.JITHelpers.intrinsicIndexOfStringUTF16(Object s1Value, int s1len, Object s2Value, int s2len, int start);
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
   static TR::Register *inlineVectorizedStringIndexOf(TR::Node *node, TR::CodeGenerator *cg, bool isCompressed);
   static TR::Register *inlineIntrinsicIndexOf(TR::Node *node, TR::CodeGenerator *cg, bool isLatin1);
   static TR::Register *inlineDoubleMax(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *inlineDoubleMin(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *inlineMathFma(TR::Node *node, TR::CodeGenerator *cg);

   /* This Evaluator generates the SIMD routine for methods
    * java/lang/String.hashCodeImplCompressed and
    * java/lang/String.hashCodeImplDecompressed depending on the "isCompressed"
    * parameter passed to it.
    */
   static TR::Register *inlineStringHashCode(TR::Node *node, TR::CodeGenerator *cg, bool isCompressed);
   static TR::Register *inlineUTF16BEEncodeSIMD(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register* inlineUTF16BEEncode    (TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *inlineCRC32CUpdateBytes(TR::Node *node, TR::CodeGenerator *cg, bool isDirectBuffer);

   static TR::Register *zdloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdsleLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdslsLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdstsLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdsleLoadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdslsLoadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdstsLoadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdsleStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdslsStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdstsStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdsleStoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdslsStoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdstsStoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zd2zdsleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zd2zdstsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdsle2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdsts2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdsts2zdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2zdslsSetSignEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2zdstsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2zdstsSetSignEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udslLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udstLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udLoadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udslLoadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udstLoadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udslStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udstStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udStoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udslStoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udstStoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2udstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udsl2udEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udst2udEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udst2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pddivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdabsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdshrSetSignEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdshlSetSignEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdshlOverflowEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2iOverflowEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2iuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iu2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2lOverflowEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2luEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lu2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdcleanEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdclearSetSignEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *monentEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *monexitEvaluator(TR::Node *node, TR::CodeGenerator *cg);
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
   static TR::Register *DIVCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *BNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ArrayCopyBNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *BNDCHKwithSpineCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ArrayStoreCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ArrayCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *conditionalHelperEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *NULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *resolveAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *evaluateNULLCHKWithPossibleResolve(TR::Node *node, bool needResolution, TR::CodeGenerator *cg);
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

   static TR::Register *zdsls2zdEvaluator(TR::Node * node, TR::CodeGenerator * cg);
   static TR::Register *zd2zdslsEvaluator(TR::Node * node, TR::CodeGenerator * cg);
   static TR::Register *pd2zdslsEvaluator(TR::Node * node, TR::CodeGenerator * cg);
   static TR::Register *zdsls2pdEvaluator(TR::Node * node, TR::CodeGenerator * cg);
   static void zonedToZonedSeparateSignHelper(TR::Node *node, TR_PseudoRegister *srcReg, TR_PseudoRegister *targetReg, TR::MemoryReference *sourceMR, TR::MemoryReference *destMR, TR::CodeGenerator * cg);
   static TR::MemoryReference *packedToZonedHelper(TR::Node *node, TR_PseudoRegister *targetReg, TR::MemoryReference *sourceMR, TR_PseudoRegister *childReg, TR::CodeGenerator * cg);
   static void pd2zdSignFixup(TR::Node *node, TR::MemoryReference *destMR, TR::CodeGenerator * cg, bool useLeftAlignedMR);
   static TR::Register *zdstoreiVectorEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg);

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
   static TR::Register *VMgenCoreInstanceofEvaluator(TR::Node *node, TR::CodeGenerator *cg, TR::LabelSymbol * trueLabel, TR::LabelSymbol * falseLabel, bool initialResult, bool needResult, TR::RegisterDependencyConditions * conditions, bool ifInstanceOf=false);
   static TR::Register *VMmonentEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMmonexitEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMnewEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMarrayCheckEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static void         restoreGPR7(TR::Node *node, TR::CodeGenerator *cg);

   /*
    * Generate instructions for static/instance field access report.
    * @param dataSnippetRegister: Optional, can be used to pass the address of the snippet inside the register.
    */ 
   static void generateTestAndReportFieldWatchInstructions(TR::CodeGenerator *cg, TR::Node *node, TR::Snippet *dataSnippet, bool isWrite, TR::Register *sideEffectRegister, TR::Register *valueReg, TR::Register *dataSnippetRegister);


   /*
    * Generate instructions to fill in the J9JITWatchedStaticFieldData.fieldAddress, J9JITWatchedStaticFieldData.fieldClass for static fields,
    * and J9JITWatchedInstanceFieldData.offset for instance fields at runtime. Used for fieldwatch support.
    * @param dataSnippetRegister: Optional, can be used to pass the address of the snippet inside the register.  
    */
   static void generateFillInDataBlockSequenceForUnresolvedField (TR::CodeGenerator *cg, TR::Node *node, TR::Snippet *dataSnippet, bool isWrite, TR::Register *sideEffectRegister, TR::Register *dataSnippetRegister);
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

   static TR::Register *inlineIntegerStringSize(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *inlineIntegerToCharsForLatin1Strings(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *inlineIntegerToCharsForUTF16Strings(TR::Node *node, TR::CodeGenerator *cg);
   };
}

}

#endif
