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

#ifndef J9_NODE_INCL
#define J9_NODE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_NODE_CONNECTOR
#define J9_NODE_CONNECTOR
namespace J9 { class Node; }
namespace J9 { typedef J9::Node NodeConnector; }
#endif

#include "j9cfg.h"
#include "il/OMRNode.hpp"

class TR_OpaquePseudoRegister;
class TR_StorageReference;
class TR_PseudoRegister;
namespace TR { class Compilation; }
namespace TR { class Node; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class TreeTop; }

namespace J9 {

class OMR_EXTENSIBLE Node : public OMR::NodeConnector
   {

protected:

   Node(TR::Node *originatingByteCodeNode, TR::ILOpCodes op, uint16_t numChildren);

   Node(TR::Node *from, uint16_t numChildren = 0);

   Node(Node& from) : OMR::NodeConnector(from) {}


public:

   Node()
      : OMR::NodeConnector(),
      _unionPropertyB()
      { }

   ~Node();

   static void copyValidProperties(TR::Node *fromNode, TR::Node *toNode);

   uint32_t getSize();

   /// given a direct call to Object.clone node, return the class of the receiver.
   ///
   TR_OpaqueClassBlock* getCloneClassInNode();
   TR::Node *           processJNICall(TR::TreeTop *, TR::ResolvedMethodSymbol *);

   void                    devirtualizeCall(TR::TreeTop*);

   /**
    * @return the signature of the node's type if applicable.
    * @note the signature's storage may have been created on the stack!
    */
   const char * getTypeSignature(int32_t &, TR_AllocationKind = stackAlloc, bool parmAsAuto = false);

#if defined(TR_TARGET_S390)
   TR_PseudoRegister       *getPseudoRegister();
   TR_OpaquePseudoRegister *getOpaquePseudoRegister();
#endif

   /**
    * @brief Answers whether the act of evaluating this node will
    *        require a register pair (two registers) to hold the
    *        result.
    * @param comp, the TR::Compilation object
    * @return true if two registers are required; false otherwise
    */
   bool requiresRegisterPair(TR::Compilation *comp);

   TR::Node *getSetSignValueNode();
   void setKnownSignCodeFromRawSign(int32_t rawSignCode);
   static bool typeSupportedForTruncateOrWiden(TR::DataType dt);
   static bool typeSupportedForSignCodeTracking(TR::DataType dt);
   static void truncateOrWidenBCDLiteral(TR::DataType dt, char *newLit, int32_t newPrecision, char *oldLit, int32_t oldPrecision);
   static void setNewBCDSignOnLiteral(uint32_t newSignCode, TR::DataType dt, char *lit, int32_t litSize);

   bool alwaysGeneratesAKnownCleanSign();
   bool alwaysGeneratesAKnownPositiveCleanSign();
#ifdef TR_TARGET_S390
   int32_t getStorageReferenceSize();
   int32_t getStorageReferenceSourceSize();
#endif

   bool         isEvenPrecision();
   bool         isOddPrecision();
   bool pdshrRoundIsConstantZero();

   bool         isSimpleTruncation();
   bool         isSimpleWidening();
   bool         mustClean();
   bool         hasIntermediateTruncation();
   bool         isTruncatingBCDShift();
   bool         isWideningBCDShift();
   bool         isTruncating();
   int32_t      survivingDigits();



   bool isTruncatingOrWideningAggrOrBCD();

   bool canRemoveArithmeticOperand();
   bool canGCandReturn();

   static uint32_t hashOnBCDOrAggrLiteral(char *lit, size_t litSize);

   bool referencesSymbolInSubTree(TR::SymbolReference * symRef, vcount_t visitCount);
   bool referencesMayKillAliasInSubTree(TR::Node * rootNode, vcount_t visitCount);
   void getSubTreeReferences(TR::SparseBitVector &references, vcount_t visitCount);
   TR_ParentOfChildNode * referencesSymbolExactlyOnceInSubTree(TR::Node *, int32_t, TR::SymbolReference *, vcount_t);

   /**
    * Node field functions
    */

#ifdef TR_TARGET_S390
   TR_StorageReference *getStorageReferenceHint();
   TR_StorageReference *setStorageReferenceHint(TR_StorageReference *s);
#endif

#ifdef SUPPORT_DFP
   long double             setLongDouble(long double d);
   long double             getLongDouble();
#endif

   /**
    * Node field functions end
    */



   /**
    * UnionPropertyB functions
    */

   void    setDecimalPrecision(int32_t p);
   uint8_t getDecimalPrecision();

   // These precision setting routines set a precision big enough to hold the
   // full computed result A caller or codegen may choose to set a different
   // value (bigger or smaller) to satisfy a specific semantic or encoding
   // requirement -- see getPDMulEncodedPrecision et al. in the platform code
   // generators
   void    setPDMulPrecision();
   void    setPDAddSubPrecision();

   bool    isDFPModifyPrecision();
   void    setDFPPrecision(int32_t p);
   uint8_t getDFPPrecision();

   void    setDecimalAdjust(int32_t a);
   int32_t getDecimalAdjust();

   void    setDecimalFraction(int32_t f);
   int32_t getDecimalFraction();

   // Source precisions are valid for conversions from a non-BCD type to a BCD type
   bool    canHaveSourcePrecision();
   bool    hasSourcePrecision();
   void    setSourcePrecision(int32_t prec);
   int32_t getSourcePrecision();

   int32_t getDecimalAdjustOrFractionOrDivisor();
   int32_t getDecimalRoundOrDividend();

   bool              isSetSignValueOnNode();
   void              setSetSign(TR_RawBCDSignCode setSign);
   TR_RawBCDSignCode getSetSign();
   bool              isDecimalSizeAndShapeEquivalent(TR::Node *other);

   void    setDecimalRound(int32_t r);
   uint8_t getDecimalRound();

   bool signStateIsKnown();
   void setSignStateIsKnown();
   bool signStateIsAssumed();
   void setSignStateIsAssumed();

   bool hasKnownCleanSign();
   void setHasKnownCleanSign(bool v);
   bool hasAssumedCleanSign();
   void setHasAssumedCleanSign(bool v);
   bool hasKnownOrAssumedCleanSign();
   void setHasKnownAndAssumedCleanSign(bool v);
   void transferCleanSign(TR::Node *srcNode);

   bool hasKnownPreferredSign();
   void setHasKnownPreferredSign(bool v);
   bool hasAssumedPreferredSign();
   void setHasAssumedPreferredSign(bool v);
   bool hasKnownOrAssumedPreferredSign();

   bool              hasKnownSignCode();
   TR_RawBCDSignCode getKnownSignCode();
   bool              hasAssumedSignCode();
   TR_RawBCDSignCode getAssumedSignCode();
   bool              hasKnownOrAssumedSignCode();
   TR_RawBCDSignCode getKnownOrAssumedSignCode();
   void              setKnownOrAssumedSignCode(TR_RawBCDSignCode sign, bool isKnown);
   void              setKnownSignCode(TR_RawBCDSignCode sign);
   void              transferSignCode(TR::Node *srcNode);

   // The mapping to/from TR_BCDSignCode from/to the 0xc/0xd/0xf 'raw'
   // encodings in setKnownSignCode/knownSignCodeIs below // is temporary until
   // TR_BCDSignCode is tracked on the nodes instead of TR_RawBCDSignCode
   // Doing it this way allows new code (such as VP constrainBCDAggrLoad) to
   // start using this new more general interface // The final code will be just
   // calling setKnownOrAssumedSignCode(sign, true, c) but now the 'sign' parm will
   // be TR_BCDSignCode of TR_RawBCDSignCode
   bool knownSignCodeIs(TR_BCDSignCode sign);
   void setKnownSignCode(TR_BCDSignCode sign);
   bool assumedSignCodeIs(TR_BCDSignCode sign);
   void setAssumedSignCode(TR_RawBCDSignCode sign);
   bool knownOrAssumedSignCodeIs(TR_BCDSignCode sign);

   // the hasSignState property is stored in the reverse sense to make the interface easier to use (at the expense of some confusion here);
   bool hasSignStateOnLoad();
   void setHasSignStateOnLoad(bool v);

   void transferSignState(TR::Node *srcNode, bool digitsLost);
   bool hasAnyKnownOrAssumedSignState();
   bool hasAnyDecimalSignState();
   void resetDecimalSignFlags();
   void resetSignState();

   bool isSignStateEquivalent(TR::Node *other);
   bool isSignStateAnImprovementOver(TR::Node *other);

   bool chkOpsCastedToBCD();
   bool castedToBCD();
   void setCastedToBCD(bool v);

   /**
    * UnionPropertyB functions end
    */



   /**
    * Node flag functions
    */

   // Flag used by TR::BNDCHK nodes
   bool isSpineCheckWithArrayElementChild();
   void setSpineCheckWithArrayElementChild(bool v);
   bool chkSpineCheckWithArrayElementChild();
   const char *printSpineCheckWithArrayElementChild();

   // Flags used by call nodes
   bool isUnsafePutOrderedCall();
   bool isDontInlinePutOrderedCall();
   void setDontInlinePutOrderedCall();
   bool chkDontInlineUnsafePutOrderedCall();
   const char * printIsDontInlineUnsafePutOrderedCall();

   bool isUnsafeGetPutCASCallOnNonArray();
   void setUnsafeGetPutCASCallOnNonArray();

   bool isProcessedByCallCloneConstrain();
   void setProcessedByCallCloneConstrain();

   bool isDAAVariableSlowCall();
   void setDAAVariableSlowCall(bool v);

   // Flag used by binary coded decimal load nodes
   bool isBCDStoreTemporarilyALoad();
   void setBCDStoreIsTemporarilyALoad(bool v);

   // Flag used by isPackedLeftShift nodes
   bool cleanSignDuringPackedLeftShift();
   void setCleanSignDuringPackedLeftShift(bool v);
   bool chkCleanSignDuringPackedLeftShift();
   const char *printCleanSignDuringPackedLeftShift();

   // Flag used by non-store and non-call binary coded decimal and aggregate nodes
   bool chkOpsSkipCopyOnLoad();
   bool skipCopyOnLoad();
   void setSkipCopyOnLoad(bool v);
   bool chkSkipCopyOnLoad();
   const char *printSkipCopyOnLoad();

   // Flag used by binary coded decimal and aggregate store nodes
   bool chkOpsSkipCopyOnStore();
   bool skipCopyOnStore();
   void setSkipCopyOnStore(bool v);
   bool chkSkipCopyOnStore();
   const char *printSkipCopyOnStore();

   // Flag used by packed decimal stores
   bool chkOpsCleanSignInPDStoreEvaluator();
   bool mustCleanSignInPDStoreEvaluator();
   void setCleanSignInPDStoreEvaluator(bool v);
   bool chkCleanSignInPDStoreEvaluator();
   const char *printCleanSignInPDStoreEvaluator();

   // Flag used by binary coded decimal stores and aggregate stores
   bool chkOpsUseStoreAsAnAccumulator();
   bool useStoreAsAnAccumulator();
   void setUseStoreAsAnAccumulator(bool v);
   bool chkUseStoreAsAnAccumulator();
   const char *printUseStoreAsAnAccumulator();

   // Flag used by non-store packed types
   bool canSkipPadByteClearing();
   void setSkipPadByteClearing(bool v);
   bool chkSkipPadByteClearing();
   const char *printSkipPadByteClearing();

   // Flag used for BCD and Aggr type stores
   bool chkOpsIsInMemoryCopyProp();
   bool isInMemoryCopyProp();
   void setIsInMemoryCopyProp(bool v);
   bool chkIsInMemoryCopyProp();
   const char *printIsInMemoryCopyProp();

   // Flag used by address type nodes in Java
   bool chkSharedMemory();
   void setSharedMemory(bool v);
   const char *printSharedMemory();


   bool isArrayCopyCall();

   /**
    * Node flag functions end
    */

   inline float            getFloat();
   inline float            setFloat(float f);

#ifdef J9VM_OPT_JAVA_CRYPTO_ACCELERATION
   bool processJNICryptoMethodCall(TR::ResolvedMethodSymbol *methodSymbol, TR::Compilation *comp);
#endif

protected:

   /// \note update the following routines when adding field to DecimalInfo
   ///       isDecimalSizeAndShapeEquivalent()
   ///       isSignStateEquivalent()
   ///       resetDecimalSignFlags()
   ///       transferSignState()
   /// Also be sure to initialize any new field in the Node.cpp constructor
   struct DecimalInfo
      {
      uint32_t  _decimalPrecision                  : 6;  ///< range 0->63
      int32_t   _decimalAdjustOrFractionOrDivisor  : 7;  ///< range as Adjust -63->63 (on all except conversion nodes)
                                                         ///< range as Fraction -63->63 (only on isConversionWithFraction nodes)
                                                         ///< range as divisorPrecision 0->63 (on select nodes)
                                                         ///< range as extFloatSrcIntDigits 0->63 (but should only need 16, on extFloat to other conversions, e.g. zf2uf,zf2d)
      uint32_t  _decimalSourcePrecisionOrDividend  : 6;  ///< range as source precision (on non-BCD-to-BCD conversions) 0->63, as dividendPrecision 0->63 (on select nodes)
      TR_RawBCDSignCode _setSign                   : 3;  ///< range on isSetSign opcodes
      uint32_t  _signStateIsKnown                  : 1;  ///< true (known) or false (assumed)
      uint32_t  _hasCleanSign                      : 1;  ///< true/false
      uint32_t  _hasPreferredSign                  : 1;  ///< true/false
      uint32_t  _round                             : 1;  ///< true/false
      uint32_t  _hasNoSignStateOnLoad              : 1;  ///< true/false // for BCD loads : false for PRE created loads that must preserve sign state (conservative setting), true for FE created loads
      uint32_t  _castedToBCD                       : 1;  ///< true/false // if true node was not originally a BCD type but casted to BCD by the optimizer from a non-BCD type (e.g. aggr or int)
      TR_RawBCDSignCode  _signCode                 : 3;
      };

   // keep nodes as small as possible for frontends that do not need the data below
   // The fields below are all disjoint, which fields exist for which platforms/opcodes are provided by the given has<Field> functions
   union UnionPropertyB
      {
      // field exists when following is true: only one of these should be true for a given node
      // eventually should replace by exclusive representation (eg. enum)
      DecimalInfo _decimalInfo;    ///< hasDecimalInfo()
      // overflow/rounding option
      UnionPropertyB()
         {
         memset(this, 0, sizeof(UnionPropertyB)); ///< in C++11 notation: _unionPropertyB = {0};
         }
      };

   enum UnionPropertyB_Type
      {
      HasNoUnionPropertyB = 0,
      HasDecimalInfo,          ///< hasDecimalInfo()
      HasBcdFlags              ///< hasBCDFlags()
      };

   bool hasDecimalInfo();
   bool hasBCDFlags();
   UnionPropertyB_Type getUnionPropertyB_Type();

   // for DecimalInfo subtypes
   bool hasDecimalPrecision();
   bool hasDecimalAdjust();
   bool hasSetSign();
   bool hasDecimalFraction();
   bool hasDecimalRound();

// Fields
private:

#ifdef TR_TARGET_S390
   ///< hasStorageReferenceHint()
   TR_StorageReference * _storageReferenceHint;
#endif

   // Holds DecimalInfo and BCDFlags
   UnionPropertyB _unionPropertyB;

   friend class ::TR_DebugExt;

protected:
   // Flag bits
   enum
      {
      // Flag used by TR::BNDCHK nodes
      spineCHKWithArrayElementChild         = 0x00004000,

      // Flags used by call nodes
      dontInlineUnsafePutOrderedCall        = 0x00000800, ///< unsafe putOrdered calls
      processedByCallCloneConstrain         = 0x00100000,
      unsafeGetPutOnNonArray                = 0x00200000,
      DAAVariableSlowCall                   = 0x00400000, ///< Used to avoid Variable precision DAA optimization

      // Flag used by binary coded decimal load nodes
      IsBCDStoreTemporarilyALoad            = 0x00400000,

      // Flag used by isPackedLeftShift nodes (pdshl,pdshlSetSign...)
      CleanSignDuringPackedLeftShift        = 0x00400000,

      // Flag used by non-store and non-call binary coded decimal and aggregate nodes
      SkipCopyOnLoad                        = 0x00800000,

      // Flag used by binary coded decimal and aggregate store nodes
      SkipCopyOnStore                       = 0x00080000,

      // Flag used by packed decimal stores (eg pdstore/ipdstore)
      cleanSignInPDStoreEvaluator           = 0x00040000,

      // Flag used by binary coded decimal stores and aggregate stores (eg pdstore/ipdstore/zdstore/izdstore etc)
      // (all nodes where ILProp3::CanUseStoreAsAnAccumulator is set)
      UseStoreAsAnAccumulator               = 0x00200000,

      // Flag used by non-store packed types
      skipPadByteClearing                   = 0x00020000,

      // Flag used by BCD and Aggr type stores -- for in memory types set when 1 or more child of the store was changed
      // during local or global copy propagation
      IsInMemoryCopyProp                    = 0x08000000,

      // Flag used by address type nodes in Java
      sharedMemory                          = 0x00000400,

      };

   };

}

#endif

