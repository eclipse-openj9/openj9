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

#ifndef TR_J9_Z_CODEGENERATORBASE_INCL
#define TR_J9_Z_CODEGENERATORBASE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef TRJ9_CODEGENERATORBASE_CONNECTOR
#define TRJ9_CODEGENERATORBASE_CONNECTOR

namespace J9 { namespace Z { class CodeGenerator; } }
namespace J9 { typedef J9::Z::CodeGenerator CodeGeneratorConnector; }

#else
#error J9::Z::CodeGenerator expected to be a primary connector, but a J9 connector is already defined
#endif



#include "compiler/codegen/J9CodeGenerator.hpp"
#include "j9cfg.h"
namespace TR { class S390EyeCatcherDataSnippet; }


namespace TR { class Node; }

namespace J9
{

namespace Z
{

class OMR_EXTENSIBLE CodeGenerator : public J9::CodeGenerator
   {
   public:

   CodeGenerator();

   TR::Recompilation *allocateRecompilationInfo();

   bool doInlineAllocate(TR::Node *node);

   TR::Linkage *createLinkage(TR_LinkageConventions lc);

   void lowerTreesPreChildrenVisit(TR::Node * parent, TR::TreeTop * treeTop, vcount_t visitCount);
   void lowerTreesPostChildrenVisit(TR::Node * parent, TR::TreeTop * treeTop, vcount_t visitCount);

   void lowerTreeIfNeeded(TR::Node *node, int32_t childNumber, TR::Node *parent, TR::TreeTop *tt);

   TR::S390EyeCatcherDataSnippet *CreateEyeCatcher(TR::Node *);

   void genZeroLeftMostUnicodeBytes(TR::Node *node, TR_PseudoRegister *reg, int32_t endByte, int32_t bytesToClear, TR::MemoryReference *targetMR);
   void widenZonedSignLeadingEmbedded(TR::Node *node, TR_PseudoRegister *reg, int32_t endByte, int32_t bytesToClear, TR::MemoryReference *targetMR);
   void widenUnicodeSignLeadingSeparate(TR::Node *node, TR_PseudoRegister *reg, int32_t endByte, int32_t bytesToClear, TR::MemoryReference *targetMR);
   void widenZonedSignLeadingSeparate(TR::Node *node, TR_PseudoRegister *reg, int32_t endByte, int32_t bytesToClear, TR::MemoryReference *targetMR);
   void genZeroLeftMostZonedBytes(TR::Node *node, TR_PseudoRegister *reg, int32_t endByte, int32_t bytesToClear, TR::MemoryReference *targetMR);

   bool alwaysGeneratesAKnownCleanSign(TR::Node *node);
   bool alwaysGeneratesAKnownPositiveCleanSign(TR::Node *node);
   TR_RawBCDSignCode alwaysGeneratedSign(TR::Node *node);

   uint32_t getPDMulEncodedSize(TR::Node *pdmul, TR_PseudoRegister *multiplicand, TR_PseudoRegister *multiplier);
   uint32_t getPDMulEncodedSize(TR::Node *pdmul);
   uint32_t getPDMulEncodedSize(TR::Node *pdmul, int32_t exponent);
   int32_t getPDMulEncodedPrecision(TR::Node *pdmul, TR_PseudoRegister *multiplicand, TR_PseudoRegister *multiplier);
   int32_t getPDMulEncodedPrecision(TR::Node *pdmul);
   int32_t getPDMulEncodedPrecision(TR::Node *pdmul, int32_t exponent);
   uint32_t getPackedToDecimalFloatFixedSize();
   uint32_t getPackedToDecimalDoubleFixedSize();
   uint32_t getPackedToDecimalLongDoubleFixedSize();
   uint32_t getDecimalFloatToPackedFixedSize();
   uint32_t getDecimalDoubleToPackedFixedSize();
   uint32_t getDecimalLongDoubleToPackedFixedSize();

   uint32_t getLongToPackedFixedSize()               { return TR_LONG_TO_PACKED_SIZE; }
   uint32_t getIntegerToPackedFixedSize()            { return TR_INTEGER_TO_PACKED_SIZE; }
   uint32_t getPackedToLongFixedSize()               { return TR_PACKED_TO_LONG_SIZE; }
   uint32_t getPackedToIntegerFixedSize()            { return TR_PACKED_TO_INTEGER_SIZE; }
   uint32_t getPackedToUnicodeFixedSourceSize()      { return TR_PACKED_TO_UNICODE_SOURCE_SIZE; }
   uint32_t getUnicodeToPackedFixedResultSize()      { return TR_UNICODE_TO_PACKED_RESULT_SIZE; }
   uint32_t getAsciiToPackedFixedResultSize()        { return TR_ASCII_TO_PACKED_RESULT_SIZE; }
   uint32_t getPDDivEncodedSize(TR::Node *node);
   uint32_t getPDDivEncodedSize(TR::Node *node, TR_PseudoRegister *dividendReg, TR_PseudoRegister *divisorReg);
   uint32_t getPDAddSubEncodedSize(TR::Node *node);
   uint32_t getPDAddSubEncodedSize(TR::Node *node, TR_PseudoRegister *firstReg);
   int32_t getPDAddSubEncodedPrecision(TR::Node *node);
   int32_t getPDAddSubEncodedPrecision(TR::Node *node, TR_PseudoRegister *firstReg);
   int32_t getPDDivEncodedPrecisionCommon(TR::Node *node, int32_t dividendPrecision, int32_t divisorPrecision, bool isDivisorEvenPrecision);
   int32_t getPDDivEncodedPrecision(TR::Node *node);
   int32_t getPDDivEncodedPrecision(TR::Node *node, TR_PseudoRegister *dividendReg, TR_PseudoRegister *divisorReg);

   bool supportsPackedShiftRight(int32_t resultPrecision, TR::Node *shiftSource, int32_t shiftAmount);
   bool canGeneratePDBinaryIntrinsic(TR::ILOpCodes opCode, TR::Node * op1PrecNode, TR::Node * op2PrecNode, TR::Node * resultPrecNode);

   bool constLoadNeedsLiteralFromPool(TR::Node *node);
   
   bool supportsTrapsInTMRegion(){ return TR::Compiler->target.isZOS();}

   using J9::CodeGenerator::addAllocatedRegister;
   void addAllocatedRegister(TR_PseudoRegister * temp);

   TR_OpaquePseudoRegister * allocateOpaquePseudoRegister(TR::DataType dt);
   TR_OpaquePseudoRegister * allocateOpaquePseudoRegister(TR_OpaquePseudoRegister *reg);
   TR_PseudoRegister * allocatePseudoRegister(TR::DataType dt);
   TR_PseudoRegister * allocatePseudoRegister(TR_PseudoRegister *reg);

   TR_OpaquePseudoRegister * evaluateOPRNode(TR::Node* node);
   TR_PseudoRegister * evaluateBCDNode(TR::Node * node);

   // --------------------------------------------------------------------------
   // Storage references
   //
   bool getAddStorageReferenceHints() { return _cgFlags.testAny(S390CG_addStorageReferenceHints);}
   void setAddStorageReferenceHints() { _cgFlags.set(S390CG_addStorageReferenceHints);}

   bool storageReferencesMatch(TR_StorageReference *ref1, TR_StorageReference *ref2);
   void processUnusedStorageRef(TR_StorageReference *ref);
   void freeUnusedTemporaryBasedHint(TR::Node *node);



   /**
    * Will a BCD left shift always leave the sign code unchanged and thus
    * allow it to be propagated through and upwards
    */
   bool propagateSignThroughBCDLeftShift(TR::DataType type)
      {
      if (type.isAnyPacked())
         return false; // may use SRP that will always generate a 0xC or 0xD
      else
         return true;
      }

   bool isAcceptableDestructivePDModPrecision(TR::Node *storeNode, TR::Node *nodeForAliasing);
   bool isAcceptableDestructivePDShiftRight(TR::Node *storeNode, TR::Node *nodeForAliasing);

   bool validateAddressOneToAddressOffset(int32_t expectedOffset, TR::Node *addr1, int64_t addr1ExtraOffset, TR::Node *addr2, int64_t addr2ExtraOffset, TR::list<TR::Node*> *_baseLoadsThatAreNotKilled, bool trace);
   void getAddressOneToAddressTwoOffset(bool *canGetOffset, TR::Node *addr1, int64_t addr1ExtraOffset, TR::Node *addr2, int64_t addr2ExtraOffset, int32_t *offset, TR::list<TR::Node*> *_baseLoadsThatAreNotKilled, bool trace);

   template <class TR_AliasSetInterface>
   bool canUseSingleStoreAsAnAccumulator(TR::Node *parent,
                                         TR::Node *node,
                                         TR::Node *store,
                                         TR_AliasSetInterface &storeAliases,
                                         TR::list<TR::Node*> *conflictingAddressNodes,
                                         bool justLookForConflictingAddressNodes,
                                         bool isChainOfFirstChildren,
                                         bool mustCheckAllNodes);

   TR::Node *getAddressLoadVar(TR::Node *node, bool trace);

   void markStoreAsAnAccumulator(TR::Node *node);
   void addStorageReferenceHints(TR::Node *node);
   void examineNode(TR::Node *parent, TR::Node *node, TR::Node *&bestNode, int32_t &storeSize, TR::list<TR::Node*> &leftMostNodesList);
   void processNodeList(TR::Node *&bestNode, int32_t &storeSize, TR::list<TR::Node*> &leftMostNodesList);

   void correctBadSign(TR::Node *node, TR_PseudoRegister *reg, int32_t endByte, TR::MemoryReference *memRef);

   int32_t genSignCodeSetting(TR::Node *node, TR_PseudoRegister *targetReg, int32_t endByte, TR::MemoryReference *signCodeMR, int32_t sign, TR_PseudoRegister *srcReg, int32_t digitsToClear, bool numericNibbleIsZero);

   void widenBCDValue(TR::Node *node, TR_PseudoRegister *reg, int32_t startByte, int32_t endByte, TR::MemoryReference *targetMR);

   void widenBCDValueIfNeeded(TR::Node *node, TR_PseudoRegister *reg, int32_t startByte, int32_t endByte, TR::MemoryReference *targetMR);
   void genZeroLeftMostDigitsIfNeeded(TR::Node *node, TR_PseudoRegister *reg, int32_t endByte, int32_t digitsToClear, TR::MemoryReference *targetMR, bool widenOnLeft=false);
   void clearByteRangeIfNeeded(TR::Node *node, TR_PseudoRegister *reg, TR::MemoryReference *targetMR, int32_t startByte, int32_t endByte, bool widenOnLeft=false);
   void genZeroLeftMostPackedDigits(TR::Node *node, TR_PseudoRegister *reg, int32_t endByte, int32_t digitsToClear, TR::MemoryReference *targetMR, int32_t memRefOffset=0);

   void initializeStorageReference(TR::Node *node,
                                   TR_OpaquePseudoRegister *destReg,
                                   TR::MemoryReference *destMR,
                                   int32_t destSize,
                                   TR::Node *srcNode,
                                   TR_OpaquePseudoRegister *srcReg,
                                   TR::MemoryReference *sourceMR,
                                   int32_t sourceSize,
                                   bool performExplicitWidening,
                                   bool alwaysLegalToCleanSign,
                                   bool trackSignState);

   TR_StorageReference *initializeNewTemporaryStorageReference(TR::Node *node,
                                                               TR_OpaquePseudoRegister *destReg,
                                                               int32_t destSize,
                                                               TR::Node *srcNode,
                                                               TR_OpaquePseudoRegister *srcReg,
                                                               int32_t sourceSize,
                                                               TR::MemoryReference *sourceMR,
                                                               bool performExplicitWidening,
                                                               bool alwaysLegalToCleanSign,
                                                               bool trackSignState);

   TR_OpaquePseudoRegister *privatizePseudoRegister(TR::Node *node, TR_OpaquePseudoRegister *reg, TR_StorageReference *storageRef, size_t sizeOverride = 0);
   TR_OpaquePseudoRegister *privatizePseudoRegisterIfNeeded(TR::Node *parent, TR::Node *child, TR_OpaquePseudoRegister *childReg);

   TR_StorageReference *privatizeStorageReference(TR::Node *node, TR_OpaquePseudoRegister *reg, TR::MemoryReference *sourceMR);
   TR_PseudoRegister *privatizeBCDRegisterIfNeeded(TR::Node *parent, TR::Node *child, TR_OpaquePseudoRegister *childReg);

   TR::MemoryReference *materializeFullBCDValue(TR::Node *node, TR_PseudoRegister *&reg, int32_t resultSize, int32_t clearSize=0, bool updateStorageReference=false, bool alwaysEnforceSSLimits=true);

   bool useMoveImmediateCommon(TR::Node *node,
                               char *srcLiteral,
                               size_t srcSize,
                               TR::Node *srcNode,
                               size_t destSize,
                               intptr_t destBaseOffset,
                               size_t destLeftMostByte,
                               TR::MemoryReference *destMR);

   bool checkFieldAlignmentForAtomicLong();

   bool canCopyWithOneOrTwoInstrs(char *lit, size_t size);
   bool inlineSmallLiteral(size_t srcSize, char *srcLiteral, size_t destSize, bool trace);

   bool allowSplitWarmAndColdBlocks() { return true; }

#if defined(J9VM_JIT_FREE_SYSTEM_STACK_POINTER)
   /** \brief
    *     Determines whether the JIT supports freeing up the system stack pointer (SSP) for register allocation.
    *
    *  \return
    *     <c>true</c> if <c>J9VM_JIT_FREE_SYSTEM_STACK_POINTER</c> is defined; <c>false</c> otherwise.
    *
    *  \note
    *     <c>J9VM_JIT_FREE_SYSTEM_STACK_POINTER</c> is currently only defined on z/OS. For exception handling,
    *     additional frames are typically allocated on the system stack.  Hence, the OS needs to be able to locate
    *     the system stack at all times in case of interrupts.  In case of z/OS, we have the mechanism to save the
    *     SSP in a memory location (off <c>J9VMThread</c>) that is registered with the OS.  We don't have such
    *     support on Linux, and hence, this is z/OS specific at the moment.
    */
   bool supportsJITFreeSystemStackPointer()
      {
      return true;
      }
#endif

   bool suppressInliningOfRecognizedMethod(TR::RecognizedMethod method);

   bool inlineDirectCall(TR::Node *node, TR::Register *&resultReg);

#ifdef J9VM_OPT_JAVA_CRYPTO_ACCELERATION
   bool inlineCryptoMethod(TR::Node *node, TR::Register *&resultReg);
#endif

   void incRefCountForOpaquePseudoRegister(TR::Node * node);

   /** \brief
    *     Generates a VM call helper sequence along with the necessary metadata in the instruction stream which when
    *     executed reverts the execution of this JIT body back to the interpreter.
    *
    *  \param cursor
    *     The cursor to which the generated instructions will be appended.
    *
    *  \param vmCallHelperSnippetLabel
    *     The label which is used to call this snippet.
    *
    *  \return
    *     The last generated instruction.
    */
   TR::Instruction* generateVMCallHelperSnippet(TR::Instruction* cursor, TR::LabelSymbol* vmCallHelperSnippetLabel);

   /** \brief
    *     Generates a VM call helper preprologue using \see generateVMCallHelperSnippet and generates the necessary
    *     metadata as well as data constants needed for invoking the snippet.
    *
    *  \param cursor
    *     The cursor to which the generated instructions will be appended.
    *
    *  \return
    *     The last generated instruction.
    */
   TR::Instruction* generateVMCallHelperPrePrologue(TR::Instruction* cursor);

   /**
    * \brief
    *    The number of nodes between a monexit and the next monent before transforming
    *    a monitored region with transactional lock elision.  On Z, 25-30 cycles are
    *    required between transactions or else the latter transaction will be aborted
    *    (with significant penalty).
    *
    * \return
    *    45.  This is an estimate based on CPI of 1.5-2 and an average of 1 instruction
    *    per node.
    */
   int32_t getMinimumNumberOfNodesBetweenMonitorsForTLE() { return 45; }

   /** \brief
    *     Sets whether decimal overflow or fixed point overflow checks should be generated for instructions which
    *     support such by-passes.
    *
    *  \note
    *     This is applicable to z15 hardware accelerated vector packed decimal operations and is typically used to
    *     control whether the ignore overflow mask (IOM) bit is set in vector packed decimal instructions.
    */
   void setIgnoreDecimalOverflowException(bool v)
      {
      _ignoreDecimalOverflowException = v;
      }

   /** \brief
    *     Gets whether decimal overflow or fixed point overflow checks should be generated for instructions which
    *     support such by-passes.
    *
    *  \note
    *     This is applicable to z15 hardware accelerated vector packed decimal operations and is typically used to
    *     control whether the ignore overflow mask (IOM) bit is set in vector packed decimal instructions.
    */
   bool getIgnoreDecimalOverflowException()
      {
      return _ignoreDecimalOverflowException;
      }

   private:

   /** \brief
    *     Determines whether decimal overflow or fixed point overflow checks should be generated for instructions which
    *     support such by-passes.
    */
   bool _ignoreDecimalOverflowException;
   };

}

}

#endif
