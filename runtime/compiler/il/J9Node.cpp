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

#include "j9cfg.h"
#include "il/J9Node.hpp"

#include <algorithm>
#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "runtime/RuntimeAssumptions.hpp"
#include "env/CHTable.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/VMJ9.h"

#ifdef TR_TARGET_S390
#include "z/codegen/S390Register.hpp"
#endif

#if (HOST_COMPILER == COMPILER_MSVC)
#define MAX_PACKED_DECIMAL_SIZE 32
#define UNICODE_SIGN_SIZE 2
// On Windows, macros are used instead of TR::DataType:: static functions to size some temp arrays
// COMPILER_MSVC outputs 'error C2057: expected constant expression' when the static functions are used to determine the array's size
#endif

/**
 * Constructors and destructors
 */

J9::Node::Node(TR::Node *originatingByteCodeNode, TR::ILOpCodes op, uint16_t numChildren)
   : OMR::NodeConnector(originatingByteCodeNode, op, numChildren),
#ifdef TR_TARGET_S390
     _storageReferenceHint(NULL),
#endif
     _unionPropertyB()
   {
   // check that _unionPropertyA union is disjoint
   TR_ASSERT(
         self()->hasSymbolReference()
       + self()->hasRegLoadStoreSymbolReference()
       + self()->hasBranchDestinationNode()
       + self()->hasBlock()
       + self()->hasArrayStride()
       + self()->hasPinningArrayPointer()
       + self()->hasDataType() <= 1,
         "_unionPropertyA union is not disjoint for this node %s (%p):\n"
         "  has({SymbolReference, ...}, ..., DataType) = ({%1d,%1d},%1d,%1d,%1d,%1d,%1d)\n",
         self()->getOpCode().getName(), this,
         self()->hasSymbolReference(),
         self()->hasRegLoadStoreSymbolReference(),
         self()->hasBranchDestinationNode(),
         self()->hasBlock(),
         self()->hasArrayStride(),
         self()->hasPinningArrayPointer(),
         self()->hasDataType());

   // check that _unionPropertyB union is disjoint
   TR_ASSERT(self()->hasDecimalInfo() + self()->hasBCDFlags() <= 1,
     "_unionPropertyB union is not disjoint for this node %s (%p):\n"
     "  has(DecimalInfo, BCDFlags) = (%1d,%1d)\n",
        self()->getOpCode().getName(), this,
        self()->hasDecimalInfo(), self()->hasBCDFlags());

   if (self()->hasDecimalInfo())
      {
      // Every node that has decimal info must set a valid precision but adjust/fraction and round can be 0
      // NOTE: update isDecimalSizeAndShapeEquivalent() routine when adding fields that specify
      // the shape/size of a decimal node (such as precision/adjust/fraction/setSign etc) to _decimalInfo.
      // Flag type fields like clean/preferred signs do not need to be added to isDecimalSizeAndShapeEquivalent()
      _unionPropertyB._decimalInfo._decimalPrecision = TR::DataType::getInvalidDecimalPrecision(); // valid range is 0->TR_MAX_DECIMAL_PRECISION (defined in OMRDataTypes.hpp)
      _unionPropertyB._decimalInfo._decimalAdjustOrFractionOrDivisor = 0;            // valid range is -31->31 for adjust/fraction and 0->62 for divisor precision
      _unionPropertyB._decimalInfo._decimalSourcePrecisionOrDividend = 0;                      // valid range is 0->63 for source precision and 0->62 for dividend precision
      _unionPropertyB._decimalInfo._setSign = raw_bcd_sign_unknown;   // for isSetSignOnNode() opcodes such as TR::pdclearSetSign
      _unionPropertyB._decimalInfo._hasNoSignStateOnLoad = 0;         // 0 (false) is the conservative setting -- meaning do not clobber the sign code unless FE explicitly instructs
      _unionPropertyB._decimalInfo._castedToBCD = 0;                  // 0 (false) originally was a BCD type, 1 (true) casted from a non-BCD type (e.g. aggr, int) to a BCD type during optimization
      _unionPropertyB._decimalInfo._signStateIsKnown = 0;
      _unionPropertyB._decimalInfo._hasCleanSign = 0;
      _unionPropertyB._decimalInfo._hasPreferredSign = 0;
      _unionPropertyB._decimalInfo._round = 0; // 0 means no rounding, 1 means rounding
      _unionPropertyB._decimalInfo._signCode = raw_bcd_sign_unknown;
      }
   }

J9::Node::Node(TR::Node *from, uint16_t numChildren)
   : OMR::NodeConnector(from, numChildren)
   {
#ifdef TR_TARGET_S390
   if (self()->getOpCode().canHaveStorageReferenceHint())
      {
      self()->setStorageReferenceHint(from->getStorageReferenceHint());
      }
#endif

   _unionPropertyB = from->_unionPropertyB; // TODO: use copyValidProperties

   if (from->getOpCode().isConversionWithFraction())
      {
      self()->setDecimalFraction(from->getDecimalFraction());
      }
   }

J9::Node::~Node()
   {
   _unionPropertyB = UnionPropertyB();
   }

void
J9::Node::copyValidProperties(TR::Node *fromNode, TR::Node *toNode)
   {
   OMR::Node::copyValidProperties(fromNode, toNode);

#ifdef TR_TARGET_S390
   if (fromNode->getOpCode().canHaveStorageReferenceHint())
      {
      toNode->setStorageReferenceHint(fromNode->getStorageReferenceHint());
      }
#endif

   // in order to determine the UnionPropertyB_type correctly, require children and symRef (from UnionPropertyA) to have been defined
   // already as depends on getDataType which in term depends on these
   UnionPropertyB_Type fromUnionPropertyB_Type = fromNode->getUnionPropertyB_Type();
   UnionPropertyB_Type toUnionPropertyB_Type = toNode->getUnionPropertyB_Type();

   if ((fromUnionPropertyB_Type == toUnionPropertyB_Type))
      {
      switch (fromUnionPropertyB_Type)
         {
         case HasDecimalInfo:
#if !defined(ENABLE_RECREATE_WITH_COPY_VALID_PROPERTIES_COMPLETE)
            // TODO there is still confusion for DFP types what properties they have
            // eg a TR::deModifyPrecision has a DFPPrecision, but a TR::pd2df does not
            // where is this declared in the opcode?

            // for now have to copy forward all decimalInfo properties
            toNode->_unionPropertyB._decimalInfo = fromNode->_unionPropertyB._decimalInfo;
#else
            // _decimalPrecision
            if (toNode->getType().isDFP() && fromNode->getType().isDFP())
               // the above test is not enough to determine whether the node hasDFPPrecision
               toNode->setDFPPrecision(fromNode->getDFPPrecision());
            else if (toNode->hasDecimalPrecision() && fromNode->hasDecimalPrecision())
               toNode->setDecimalPrecision(fromNode->getDecimalPrecision());

            // _decimalSourcePrecisionOrDividend
            if (toNode->canHaveSourcePrecision() && fromNode->canHaveSourcePrecision())
               toNode->setSourcePrecision(fromNode->getSourcePrecision());

            // _decimalAdjustOrFractionOrDivisor
            if (toNode->hasDecimalAdjust() && fromNode->hasDecimalAdjust())
               toNode->setDecimalAdjust(fromNode->getDecimalAdjust());
            else if (toNode->hasDecimalFraction() && fromNode->hasDecimalFraction())
               toNode->setDecimalFraction(fromNode->getDecimalFraction());

            // _round
            if (toNode->hasDecimalRound() && fromNode->hasDecimalRound())
               toNode->setDecimalRound(fromNode->getDecimalRound());

            // _setSign
            if (toNode->hasSetSign() && fromNode->hasSetSign())
               toNode->setSetSign(fromNode->getSetSign());

            // _signStateIsKnown
            // _hasCleanSign
            // _hasPreferredSign
            // _signCode
            // _hasNoSignStateOnLoad
            if (toNode->getType().isBCD() && fromNode->getType().isBCD())
               {
               toNode->_unionPropertyB._decimalInfo._signStateIsKnown = fromNode->signStateIsKnown();
               toNode->_unionPropertyB._decimalInfo._hasCleanSign = fromNode->hasKnownOrAssumedCleanSign(); //  '10 0d' might be truncated to negative zero '0 00d' -- so no longer clean
               toNode->_unionPropertyB._decimalInfo._hasPreferredSign = fromNode->hasKnownOrAssumedPreferredSign();
               toNode->_unionPropertyB._decimalInfo._signCode = fromNode->getKnownOrAssumedSignCode();
               if (toNode->getOpCode().isBCDLoad() && fromNode->getOpCode().isBCDLoad())
                  toNode->setHasSignStateOnLoad(fromNode->hasSignStateOnLoad());
               if (toNode->getOpCode().chkOpsCastedToBCD() && fromNode->getOpCode().chkOpsCastedToBCD())
                  toNode->setCastedToBCD(fromNode->castedToBCD());
               }
#endif
            break;
         case HasBcdFlags:
            // should never get here, as only one type of opcodevalue for this flag
            TR_ASSERT(fromNode->getOpCodeValue() == toNode->getOpCodeValue(),
                  "Unexpected comparison. Nodes that have _bcdFlags property should always be BCDCHK nodes. These are %s %p and %s %p nodes",
                  fromNode->getOpCode().getName(), fromNode,
                  toNode->getOpCode().getName(), toNode);
            TR_ASSERT(false, "This makes no sense as recreate does not change the opcodevalue when copying forward properties from node %s %p to node %s %p",
                  fromNode->getOpCode().getName(), fromNode,
                  toNode->getOpCode().getName(), toNode);
            break;
         default:
            /* HasNoUnionPropertyB */
            break;
         }
      }

   }

/**
 * Constructors and destructors end
 */


uint32_t
J9::Node::getSize()
   {
   if (self()->getType().isBCD())
      {
      // this is a temporary workaround for a compiler bug where too many bits are getting pulled out of the bit container
/*
      TR_ASSERT(_unionPropertyB._decimalInfo._decimalPrecision >=0 && _unionPropertyB._decimalInfo._decimalPrecision <=TR_MAX_DECIMAL_PRECISION,
         "unexpected decimal precision %d on node %p\n", _unionPropertyB._decimalInfo._decimalPrecision, this);
      return TR::DataType::getSizeFromBCDPrecision(getDataType(), _unionPropertyB._decimalInfo._decimalPrecision);
*/
      TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalPrecision field for node %s %p that does not have it", self()->getOpCode().getName(), self());
      int32_t newPrec = _unionPropertyB._decimalInfo._decimalPrecision & 0x3F;
      return TR::DataType::getSizeFromBCDPrecision(self()->getDataType(), newPrec);
      }
   return OMR::NodeConnector::getSize();
   }

/**
 * Given a direct call to Object.clone node, return the class of the receiver.
 */
TR_OpaqueClassBlock *
J9::Node::getCloneClassInNode()
   {
   TR_ASSERT(!self()->hasNodeExtension(), "Node %p should not have node extension");
   return (TR_OpaqueClassBlock *)_unionBase._children[1];
   }


TR::Node *
J9::Node::processJNICall(TR::TreeTop * callNodeTreeTop, TR::ResolvedMethodSymbol * owningSymbol)
   {
   TR::Compilation * comp = TR::comp();
   if (!comp->cg()->getSupportsDirectJNICalls() || comp->getOption(TR_DisableDirectToJNI) || (comp->compileRelocatableCode() && !comp->cg()->supportsDirectJNICallsForAOT()))
      return self();

   TR::ResolvedMethodSymbol * methodSymbol = self()->getSymbol()->castToResolvedMethodSymbol();
   TR_ResolvedMethod * resolvedMethod = methodSymbol->getResolvedMethod();

   // TR_DisableDirectToJNIInline means only convert calls in thunks
   // also don't directly call any native method that has tracing enabled
   //
   if (!comp->getCurrentMethod()->isJNINative() &&
       (comp->getOption(TR_DisableDirectToJNIInline) ||
        comp->fej9()->isAnyMethodTracingEnabled(resolvedMethod->getPersistentIdentifier())))
      return self();

   if (!comp->getOption(TR_DisableUnsafe) && !TR::Compiler->om.canGenerateArraylets() &&
       (methodSymbol->getRecognizedMethod() == TR::java_nio_Bits_copyToByteArray ||
        methodSymbol->getRecognizedMethod() == TR::java_nio_Bits_copyFromByteArray))
      return self();

#ifdef J9VM_OPT_JAVA_CRYPTO_ACCELERATION
   if (self()->processJNICryptoMethodCall(methodSymbol, comp))
      {
      return self();
      }
#endif

   if (comp->canTransformUnsafeCopyToArrayCopy() &&
       (methodSymbol->getRecognizedMethod() == TR::sun_misc_Unsafe_copyMemory))
      {
      return self();
      }
   if (comp->canTransformUnsafeSetMemory() &&
       (methodSymbol->getRecognizedMethod() == TR::sun_misc_Unsafe_setMemory))
      {
      return self();
      }

   if (methodSymbol->getRecognizedMethod() == TR::sun_misc_Unsafe_ensureClassInitialized)
      {
      return self();
      }


   if (methodSymbol->canReplaceWithHWInstr())
      return self();

   // don't convert synchronized jni calls to direct to jni calls unless we're compiling
   // the jni thunk.
   //
   if (resolvedMethod->isSynchronized() && !comp->getCurrentMethod()->isJNINative())
      {
      // todo: consider adding monent/monexit trees so that it can inlined
      return self();
      }

   if (self()->getOpCode().isCallIndirect())
      {
      // todo: handle nonoverridden indirect calls
      // || (methodSymbol->isVirtualMethod() && !virtualMethodIsOverridden(resolvedMethod))))
      return self();
      }

#if defined(TR_TARGET_POWER)
   if (((methodSymbol->getRecognizedMethod() == TR::java_util_zip_CRC32_update) ||
        (methodSymbol->getRecognizedMethod() == TR::java_util_zip_CRC32_updateBytes) ||
        (methodSymbol->getRecognizedMethod() == TR::java_util_zip_CRC32_updateByteBuffer)) &&
       !comp->requiresSpineChecks())
      {
      self()->setPreparedForDirectJNI();
      return self();
      }
#endif

   // In the latest round of VM drops, we've lowered the maximum outgoing argument size on the C stack to 32
   // (it used to be the maximum 255).  This means that fixed frame platforms (those who pre-allocate space
   // in the C stack for outgoing arguments, as opposed to buying new stack to pass arguments) who implement
   // direct JNI can no longer depend on the VM allocating the full 255 slots of backing store.
   //
   // The simplest solution would be to simply not translate JNI methods which have more than 32 arguments,
   // and fall back to the interpreter for those cases.  The VM handles the >32 case by calling to another
   // C function which does allocate space for the full 255 arguments, but this approach will of course not
   // work for the JIT.
   //
   // The 32 limit is on the method argument count (which includes the receiver
   // for virtual methods), but not the "fixed" parameters (the JNIEnv * and the
   // class parameter for static methods), so we actually have 34 slots of space
   // preallocated on fixed frame platforms.
   //
   uint32_t numChildren = self()->getNumChildren();
   if (numChildren - self()->getFirstArgumentIndex() > 32 && comp->cg()->hasFixedFrameC_CallingConvention())
      return self();

   if (callNodeTreeTop->getNode()->getOpCode().isNullCheck())
      {
      //TR_ASSERT(!callNodeTreeTop->getNode()->getOpCode().isResolveCheck(), "Node::processJNICall, expected NULLCHK node");
      TR::Node::recreate(callNodeTreeTop->getNode(), TR::NULLCHK);
      callNodeTreeTop->getNode()->extractTheNullCheck(callNodeTreeTop->getPrevTreeTop());
      }

   bool wrapObjects = !comp->fej9()->jniDoNotWrapObjects(resolvedMethod);

   // Add a level of indirection for each address argument to a JNI method
   //
   if (wrapObjects)
   {
   int32_t i;
   for (i = 0; i < numChildren; ++i)
      {
      TR::Node * n = self()->getChild(i);
      if (n->getDataType() == TR::Address)
         {
         if (n->getOpCode().hasSymbolReference() && n->getSymbol()->isAutoOrParm())
            {
            n->decReferenceCount();
            self()->setAndIncChild(i, TR::Node::createWithSymRef(n, TR::loadaddr, 0, n->getSymbolReference()));
            }
         else
            {
            TR::SymbolReference * symRef = comp->getSymRefTab()->createTemporary(owningSymbol, TR::Address);
            TR::TreeTop::create(comp, callNodeTreeTop->getPrevTreeTop(), TR::Node::createWithSymRef(TR::astore, 1, 1, n, symRef));
            // create will have inc'ed the reference count, but there really aren't more
            // parents, so need to dec it back.
            n->decReferenceCount();
            self()->setAndIncChild(i, TR::Node::createWithSymRef(n, TR::loadaddr, 0, symRef));
            }
         if (n->isNonNull())
            {
            self()->getChild(i)->setPointsToNonNull(true);
            }
         }
      }
   }

   self()->setPreparedForDirectJNI();

   if (methodSymbol->isStatic())
      {
      TR::Node * newNode = new (comp->getNodePool()) TR::Node(self(), numChildren + 1);
      for (int32_t i = numChildren; i; --i)
         newNode->setChild(i, self()->getChild(i - 1));
      newNode->setNumChildren(numChildren + 1);


      TR::ResolvedMethodSymbol * callerSymbol = self()->getSymbolReference()->getOwningMethodSymbol(comp);
      int32_t callerCP = self()->getSymbolReference()->getCPIndex();

      // For JNI thunks cp index is -1
      // passing cpIndex = -1 to findOrCreateClassSymbol shouldn't be an issue as it will figure out the address from resolvedMethod->containingClass()
      // we only need cpIndex for relocations in AOT and compiling JNI thunks is disable in AOT
      int32_t classCP = (callerCP != -1) ? callerSymbol->getResolvedMethod()->classCPIndexOfMethod(callerCP) : -1;

      TR_ASSERT ((callerCP != -1 || callerSymbol->isNative()), "Cannot have cp index -1 for JNI calls other than JNI thunks.\n");

      TR::Node *addressOfJ9Class = TR::Node::aconst(newNode, (uintptrj_t)resolvedMethod->containingClass());
      addressOfJ9Class->setIsClassPointerConstant(true);
      TR::Node *addressOfJavaLangClassReference;

      TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
      if (TR::Compiler->target.is64Bit())
         {
         addressOfJavaLangClassReference =
               TR::Node::create(TR::aladd, 2,
                     addressOfJ9Class,
                     TR::Node::lconst(newNode, fej9->getOffsetOfJavaLangClassFromClassField()));
         }
      else
         {
         addressOfJavaLangClassReference =
               TR::Node::create(TR::aiadd, 2,
                     addressOfJ9Class,
                     TR::Node::iconst(newNode, fej9->getOffsetOfJavaLangClassFromClassField()));
         }
      newNode->setAndIncChild(0, addressOfJavaLangClassReference);

      if (callNodeTreeTop->getNode() == self())
         {
         callNodeTreeTop->setNode(newNode);
         }
      else
         {
         TR_ASSERT(callNodeTreeTop->getNode()->getChild(0) == self(), "call node " POINTER_PRINTF_FORMAT " is not the child of a treetop", self());
         callNodeTreeTop->getNode()->setChild(0, newNode);
         }
      return newNode;
      }

   return self();
   }

void
J9::Node::devirtualizeCall(TR::TreeTop *treeTop)
   {
   OMR::NodeConnector::devirtualizeCall(treeTop);
   TR::ResolvedMethodSymbol *methodSymbol = self()->getSymbol()->castToResolvedMethodSymbol();

   if (methodSymbol->isJNI())
      self()->processJNICall(treeTop, TR::comp()->getMethodSymbol());
   }

bool
J9::Node::isEvenPrecision()
   {
   return ((self()->getDecimalPrecision() & 0x1) == 0);
   }

bool
J9::Node::isOddPrecision()
   {
   return ((self()->getDecimalPrecision() & 0x1) != 0);
   }

/**
 * A 'simple' widening or truncation is a pdshl node that changes the precision of its child but has a shift amount of 0
 */
bool
J9::Node::isSimpleTruncation()
   {
   if (self()->getOpCode().isModifyPrecision() &&
       self()->getDecimalPrecision() < self()->getFirstChild()->getDecimalPrecision())
      {
      return true;
      }
   else if (self()->getOpCodeValue() == TR::pdshl &&
            self()->getSecondChild()->getOpCode().isLoadConst() &&
            self()->getSecondChild()->get64bitIntegralValue() == 0 &&
            self()->getDecimalPrecision() < self()->getFirstChild()->getDecimalPrecision())
      {
      return true;
      }
   else
      {
      return false;
      }
   }

/**
 * Example of an intermediate truncation (no shifts involved)
 *     zd2pd p=4         0034  this
 *        pd2zd p=2        34  child
 *           pdx p=4     1234  grandChild
 * In this case it would be illegal to cancel out the zd2pd/pd2zd nodes because the child (pd2zd) has the
 * side effect of truncating during the conversion operation.
 *
 * The shift case is more complicated as only some of the child digits may survive the shift
 * Example of a truncating shift that is *not* an intermediate truncation:
 *     pdshl p=3      300   // survivingDigits = 1 <= child->prec (1) so not an intermediate truncation
 *        zd2pd p=1     3   // isTruncating=true
 *           zdx p=3  123
 *        shift=2
 *
 *     so
 *     zdshl p=3   300
 *        zdx p=3  123
 *     shift=2
 *     gives an equivalent answer
 *
 * Example of a truncating shift that *is* an intermediate truncation:
 *     pdshl p=4     0300   // survivingDigits = 2 > child->prec (1) so is an intermediate truncation
 *        zd2pd p=1     3   // isTruncating=true
 *           zdx p=3  123
 *        shift=2
 *
 *     so
 *     zdshl p=4     2300
 *        zdx p=3     123
 *     shift=2
 *     gives a different answer
 */
bool
J9::Node::hasIntermediateTruncation()
   {
   TR::Node *valueChild = self()->getValueChild();
   TR_ASSERT(self()->getType().isBCD() && valueChild->getType().isBCD(), "hasIntermediateTruncation only valid for BCD parent (dt=%s) and child (dt=%s)\n",
      self()->getDataType().toString(), valueChild->getDataType().toString());
   if (valueChild->isTruncating() &&
       self()->survivingDigits() > valueChild->getDecimalPrecision())
      {
      return true;
      }
   else
      {
      return false;
      }
   }

/**
 * For non-shift operations the surviving digits is simply the precision.
 *
 * Left shift example:
 * For a 3v0 conversion to a 2v2 result (left shift by 2) there are 2 surviving digits -- 123. -> 123.00 but result precision is 4 so only 23.00 survive.
 * The shiftedPrecision is 3+2 = 5 and this is > than the nodePrec of 4.
 * There is 1 truncated digit so reduce the shiftedPrecision by 1 to get the surviving digits value of 2.
 *
 * Right shift example:
 * For a 3v2 conversion to 2v1 result (right shift by 1) there are 3 surviving digits -- 123.45 -> 123.4 but result precision is 3 so only 23.4 survive.
 * The shiftedPrecision is 5-1 = 4 and this > than the nodePrec of 3.
 * There is 1 truncated digit so reduce the shiftedPrecision by 1 to get the surviving digits value of 3.
 *
 * @note that the shift formula for surviving digits degenerates to just getDecimalPrecision() when adjust == 0
 */
int32_t
J9::Node::survivingDigits()
   {
   TR_ASSERT(self()->getType().isBCD(), "node %p (op %d) is not a BCD type (dt=%s)\n", self(), self()->getOpCodeValue(), self()->getDataType().toString());
   int32_t survivingDigits = 0;
   if (self()->getOpCode().isShift())
      {
      TR::Node *child = self()->getFirstChild();                                             // left shift 3v0 -> 2v2
      int32_t adjust = self()->getDecimalAdjust();                                         // 2
      int32_t shiftedPrecision = child->getDecimalPrecision() + adjust;             // 5=3+2
      int32_t truncatedDigits = shiftedPrecision - self()->getDecimalPrecision();           // 1=5-4
      survivingDigits = child->getDecimalPrecision() - truncatedDigits;             // 2=3-1
      }
   else
      {
      survivingDigits = self()->getDecimalPrecision();
      }
   return survivingDigits;
   }

bool
J9::Node::isTruncating()
   {
   if (self()->getType().isBCD() && self()->getNumChildren() >= 1 && self()->getValueChild()->getType().isBCD())
      {
      if (self()->getOpCode().isShift())
         return self()->isTruncatingBCDShift();
      else if (self()->getDecimalPrecision() < self()->getValueChild()->getDecimalPrecision())
         return true;
      else
         return false;
      }
   // eg. dd2pd <prec=9, sourcePrecision=12> -> ddadd: truncation since the ddadd may have up to 12 non-zero digits
   else if (self()->getType().isBCD() && self()->getOpCode().isConversion() && self()->getNumChildren() >= 1 && !self()->getValueChild()->getType().isBCD())
      {
      if (self()->hasSourcePrecision() && self()->getDecimalPrecision() < self()->getSourcePrecision())
         return true;
      else if (!self()->hasSourcePrecision()) // Conservative, but necessary for correctness in some opts; best to always set sourcePrecision
         return true;
      else
         return false;
      }
   else
      {
      return false;
      }
   }

bool
J9::Node::isTruncatingBCDShift()
   {
   TR_ASSERT(self()->getType().isBCD() && (self()->getOpCode().isModifyPrecision() || self()->getOpCode().isShift()), "node %p (op %d) is not a BCD shift\n", this, self()->getOpCodeValue());

   if (self()->getOpCode().isModifyPrecision() &&
       self()->getDecimalPrecision() < self()->getFirstChild()->getDecimalPrecision())
      {
      return true;
      }
   else if (self()->getOpCode().isShift() &&
            self()->getDecimalPrecision() < (self()->getFirstChild()->getDecimalPrecision() + self()->getDecimalAdjust()))
      {
      return true;
      }
   else
      {
      return false;
      }
   }

bool
J9::Node::isWideningBCDShift()
   {
   TR_ASSERT(self()->getType().isBCD() && (self()->getOpCode().isModifyPrecision() || self()->getOpCode().isShift()), "node %p (op %d) is not a BCD shift\n", self(), self()->getOpCodeValue());

   if (self()->getOpCode().isModifyPrecision() &&
       self()->getDecimalPrecision() > self()->getFirstChild()->getDecimalPrecision())
      {
      return true;
      }
   else if (self()->getOpCode().isShift() &&
            self()->getDecimalPrecision() > (self()->getFirstChild()->getDecimalPrecision() + self()->getDecimalAdjust()))
      {
      return true;
      }
   else
      {
      return false;
      }
   }

bool
J9::Node::isSimpleWidening()
   {
   if (self()->getOpCode().isModifyPrecision() &&
       self()->getDecimalPrecision() > self()->getFirstChild()->getDecimalPrecision())
      {
      return true;
      }
   else if (self()->getOpCodeValue() == TR::pdshl &&
            self()->getSecondChild()->getOpCode().isLoadConst() &&
            self()->getSecondChild()->get64bitIntegralValue() == 0 &&
            self()->getDecimalPrecision() > self()->getFirstChild()->getDecimalPrecision())
      {
      return true;
      }
   else
      {
      return false;
      }
   }

bool
J9::Node::mustClean()
   {
   if (self()->getType().isAnyPacked() &&
      (self()->getOpCodeValue() == TR::pdclean || (self()->getOpCode().isStore() && self()->mustCleanSignInPDStoreEvaluator())))
      {
      return true;
      }
   return false;
   }

void
J9::Node::setKnownSignCodeFromRawSign(int32_t rawSignCode)
   {
   if (TR::Node::typeSupportedForSignCodeTracking(self()->getDataType()))
      {
      if (rawSignCode == 0xc)
         self()->setKnownSignCode(raw_bcd_sign_0xc);
      else if (rawSignCode == 0xd)
         self()->setKnownSignCode(raw_bcd_sign_0xd);
      else if (rawSignCode == 0xf)
         self()->setKnownSignCode(raw_bcd_sign_0xf);
      }
   }

/**
 * @todo Start tracking SeparateOneByte and SeparateTwoByte sign sizes too
 */
bool
J9::Node::typeSupportedForSignCodeTracking(TR::DataType dt)
   {
   return TR::DataType::getSignCodeSize(dt) == EmbeddedHalfByte;
   }

bool
J9::Node::typeSupportedForTruncateOrWiden(TR::DataType dt)
   {
   return dt == TR::PackedDecimal
       || dt == TR::ZonedDecimalSignLeadingEmbedded
       || dt == TR::ZonedDecimal ;
   }

void
J9::Node::truncateOrWidenBCDLiteral(TR::DataType dt, char *newLit, int32_t newPrecision, char *oldLit, int32_t oldPrecision)
   {
   TR_ASSERT(TR::Node::typeSupportedForTruncateOrWiden(dt),
      "datatype %s not supported in truncateOrWidenBCDLiteral\n", dt.toString());
   bool isLeadingSign = (dt == TR::ZonedDecimalSignLeadingEmbedded);
   int32_t newSize = TR::DataType::getSizeFromBCDPrecision(dt, newPrecision);
   int32_t oldSize = TR::DataType::getSizeFromBCDPrecision(dt, oldPrecision);
   memset(newLit, TR::DataType::getOneByteBCDFill(dt), newSize);
   char *shiftedNewLit = newLit;
   char *shiftedLit = oldLit;
   int32_t copySize = oldSize;
   if (newSize > oldSize)
      {
      shiftedNewLit += (newSize - oldSize);  // a widening into a right justified field so bump the destination
      copySize = oldSize;
      }
   else if (newSize < oldSize)
      {
      shiftedLit += (oldSize - newSize);     // a truncation from a right justified field so bump the source
      copySize = newSize;
      }
   memcpy(shiftedNewLit, shiftedLit, copySize);
   if (dt == TR::PackedDecimal && ((newPrecision&0x1)==0))   // zero top nibble for even precision results
      newLit[0] &= 0x0F;

   if (isLeadingSign)
      {
      TR_ASSERT(dt == TR::ZonedDecimalSignLeadingEmbedded, "only zdsle leading sign type supported in truncateOrWidenBCDLiteral (dt=%s)\n", dt.toString());
      uint8_t sign = oldLit[0]&0xf0;
      newLit[0] = (newLit[0]&0x0f) | sign;
      }
   }

void
J9::Node::setNewBCDSignOnLiteral(uint32_t newSignCode, TR::DataType dt, char *lit, int32_t litSize)
   {
   switch (dt)
      {
      case TR::PackedDecimal:
         TR_ASSERT(newSignCode <= 0xF, "expecting packed embedded sign type %s signCode 0x%x to be <= 0xF\n", dt.toString(), newSignCode);
         lit[litSize-1] = (newSignCode | (lit[litSize-1] & 0xf0));
         break;
      case TR::ZonedDecimal:
         TR_ASSERT(newSignCode <= 0xF, "expecting zoned embedded sign type %s signCode 0x%x to be <= 0xF\n", dt.toString(), newSignCode);
         lit[litSize-1] = ((newSignCode<<4) | (lit[litSize-1] & 0x0f));
         break;
      case TR::ZonedDecimalSignLeadingEmbedded:
         TR_ASSERT(newSignCode <= 0xF, "expecting zoned leading embedded sign type %s signCode 0x%x to be <= 0xF\n", dt.toString(), newSignCode);
         lit[0] = ((newSignCode<<4) | (lit[0] & 0x0f));
         break;
      case TR::ZonedDecimalSignLeadingSeparate:
         TR_ASSERT(newSignCode <= 0xFF, "expecting separate sign type %s signCode 0x%x to be <= 0xFF\n", dt.toString(), newSignCode);
         lit[0] = newSignCode;
         break;
      case TR::ZonedDecimalSignTrailingSeparate:
         TR_ASSERT(newSignCode <= 0xFF, "expecting separate sign type %s signCode 0x%x to be <= 0xFF\n", dt.toString(), newSignCode);
         lit[litSize-1] = newSignCode;
         break;
      case TR::UnicodeDecimalSignLeading:
         TR_ASSERT(newSignCode <= 0xFF, "expecting separate sign type %s signCode 0x%x to be <= 0xFF\n", dt.toString(), newSignCode);
         lit[0] = 0;
         lit[1] = newSignCode;
         break;
      case TR::UnicodeDecimalSignTrailing:
         TR_ASSERT(newSignCode <= 0xFF, "expecting separate sign type %s signCode 0x%x to be <= 0xFF\n", dt.toString(), newSignCode);
         lit[litSize-2] = 0;
         lit[litSize-1] = newSignCode;
         break;
      case TR::UnicodeDecimal:
         TR_ASSERT(false, "TR::UnicodeDecimal type (%s) does not have a sign code\n", dt.toString());
         break;
      default:
         TR_ASSERT(false, "unknown bcd type %s\n", dt.toString());
         break;
      }
   }

TR::Node *
J9::Node::getSetSignValueNode()
   {
   TR::Node *setSignValueNode = NULL;
   if (self()->getOpCode().isSetSign())
      {
      int32_t setSignValueIndex = TR::ILOpCode::getSetSignValueIndex(self()->getOpCodeValue());
      if (setSignValueIndex > 0)
         {
         setSignValueNode = self()->getChild(setSignValueIndex);
         }
      else
         {
         TR_ASSERT(false, "setSignValueIndex should be > 0 and not %d\n", setSignValueIndex);
         }
      }
   else
      {
      TR_ASSERT(false, "getSetSignValueNode only valid for setSign ops and not op %d\n", self()->getOpCodeValue());
      }
   return setSignValueNode;
   }

bool
J9::Node::alwaysGeneratesAKnownCleanSign()
   {
   TR::Compilation *comp = TR::comp();
   return comp->cg()->alwaysGeneratesAKnownCleanSign(self());
   }

bool
J9::Node::alwaysGeneratesAKnownPositiveCleanSign()
   {
   TR::Compilation *comp = TR::comp();
   return comp->cg()->alwaysGeneratesAKnownPositiveCleanSign(self());
   }

#ifdef TR_TARGET_S390
int32_t
J9::Node::getStorageReferenceSize()
   {
   TR::Compilation *comp = TR::comp();
   if (self()->getType().isAggregate())
      {
      return self()->getSize();
      }
   else if (self()->getType().isAnyZoned() && self()->getOpCode().isConversion() && self()->getFirstChild()->getType().isDFP())
      {
      if (self()->hasSourcePrecision() && self()->getSourcePrecision() <= TR::DataType::getMaxExtendedDFPPrecision())
         {
         return std::max<int32_t>(self()->getDecimalPrecision(), self()->getSourcePrecision());
         }
      else if (self()->getDataType() == TR::DecimalFloat || self()->getDataType() == TR::DecimalDouble)
         return TR::DataType::getMaxLongDFPPrecision();
      else
         return TR::DataType::getMaxExtendedDFPPrecision();
      }
   else if (self()->getOpCode().isBCDToNonBCDConversion())
      {
      return self()->getStorageReferenceSourceSize();
      }
   else
      {
      TR_ASSERT(self()->getType().isBCD(), "node result type should be a BCD type in getStorageReferenceSize and not type %s\n", self()->getDataType().toString());
      int32_t size = 0;
      switch (self()->getOpCodeValue())
         {
         case TR::l2pd:
            size = comp->cg()->getLongToPackedFixedSize();
            break;
         case TR::i2pd:
            size = comp->cg()->getIntegerToPackedFixedSize();
            break;
         case TR::pdadd:
         case TR::pdsub:
            size = comp->cg()->getPDAddSubEncodedSize(self());
            break;
         case TR::pdmul:
            size = comp->cg()->getPDMulEncodedSize(self());
            break;
         case TR::pddiv:
         case TR::pdrem:
            size = comp->cg()->getPDDivEncodedSize(self());
            break;
         case TR::ud2pd:
         case TR::udsl2pd:
         case TR::udst2pd:
            size = comp->cg()->getUnicodeToPackedFixedResultSize();
            break;
         default:
            size = self()->getSize();
            break;
         }

      if ((self()->getOpCode().isPackedRightShift() && self()->getDecimalRound() != 0) || self()->getOpCodeValue() == TR::pdshlOverflow)
         size = std::max(self()->getSize(), self()->getFirstChild()->getSize());

      return size;
      }
   }

int32_t
J9::Node::getStorageReferenceSourceSize()
   {
   TR::Compilation *comp = TR::comp();
   int32_t size = 0;
   switch (self()->getOpCodeValue())
      {
      case TR::pd2l:
      case TR::pd2lu:
      case TR::pd2lOverflow:
         size = comp->cg()->getPackedToLongFixedSize();
         break;
      case TR::pd2i:
      case TR::pd2iu:
      case TR::pd2iOverflow:
         size = comp->cg()->getPackedToIntegerFixedSize();
         break;
      default:
         if (self()->getOpCode().isConversion() &&
                  self()->getType().isDFP() &&
                  (
                   self()->getFirstChild()->getType().isAnyZoned() ||
                   self()->getFirstChild()->getType().isAnyPacked()))
            {
            size = self()->getFirstChild()->getSize();
            }
         else
            {
            TR_ASSERT(false, "node %p and type %s not supported in getStorageReferenceSourceSize\n", self(), self()->getDataType().toString());
            }
         break;
      }
   return size;
   }

TR_PseudoRegister*
J9::Node::getPseudoRegister()
   {
   TR_ASSERT(self()->getOpCodeValue() != TR::BBStart, "don't call getPseudoRegister for a BBStart");
   TR::Register *reg = self()->getRegister();
   return reg ? reg->getPseudoRegister() : 0;
   }

TR_OpaquePseudoRegister*
J9::Node::getOpaquePseudoRegister()
   {
   TR_ASSERT(self()->getOpCodeValue() != TR::BBStart, "don't call getOpaquePseudoRegister for a BBStart");
   TR::Register *reg = self()->getRegister();
   return reg ? reg->getOpaquePseudoRegister() : 0;
   }

#endif //S390


bool
J9::Node::pdshrRoundIsConstantZero()
   {
   if (self()->getOpCode().isPackedRightShift())
      {
      if (self()->getThirdChild()->getOpCode().isLoadConst() &&
          self()->getThirdChild()->get64bitIntegralValue() == 0)
         {
         return true;
         }
      }
   else
      {
      TR_ASSERT(false, "only packed and DFP shift right operations have a round node (op %d)\n", self()->getOpCodeValue());
      }
   return false;
   }

/*
 * \brief
 *    Get the node type signature
 *
 * \parm len
 *
 * \parm allocKind
 *
 * \parm parmAsAuto
 *    For a node with parm symbol, the API is used in two different ways because the slot for dead
 *    parm symbol can be reused for variables of any type. When \parm parmAsAuto is true, parm is
 *    treated as other auto and the type signature is ignored. When \parm parmAsAuto is false, the
 *    type signature of the parm symbol is returned.
 */
const char *
J9::Node::getTypeSignature(int32_t & len, TR_AllocationKind allocKind, bool parmAsAuto)
   {
   TR::Compilation * c = TR::comp();
   // todo: can do better for array element references
   if (!self()->getOpCode().hasSymbolReference())
      return 0;

   TR::SymbolReference *symRef = self()->getSymbolReference();
   TR::Symbol *sym = symRef->getSymbol();
   if (parmAsAuto && sym->isParm())
      return 0;
   bool allowForAOT = c->getOption(TR_UseSymbolValidationManager);
   TR_PersistentClassInfo * classInfo = c->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(c->getCurrentMethod()->containingClass(), c, allowForAOT);
   TR::Node * node = self();
   TR_PersistentFieldInfo * fieldInfo = classInfo && classInfo->getFieldInfo() ? classInfo->getFieldInfo()->findFieldInfo(c, node, false) : 0;
    if (fieldInfo && fieldInfo->isTypeInfoValid() && fieldInfo->getNumChars() > 0)
       {
       len = fieldInfo->getNumChars();
       return fieldInfo->getClassPointer();
       }

   if (self()->getOpCodeValue() == TR::multianewarray)
      symRef = self()->getChild(self()->getNumChildren()-1)->getSymbolReference();
   const char * sig = symRef->getTypeSignature( len, allocKind);
   if (sig)
      return sig;

   if (self()->getOpCodeValue() == TR::aloadi && symRef->getCPIndex() == -1)
      {
      // Look for an array element reference. If this is such a reference, look
      // down to get the signature of the base and strip off one layer of array
      // indirection.
      //
      TR::Node * child = self()->getFirstChild();
      if (child->isInternalPointer())
         {
         child = child->getFirstChild();
         sig = child->getTypeSignature(len, allocKind, parmAsAuto);
         if (sig != NULL && *sig == '[')
            {
            --len;
            return sig+1;
            }
         }
      }

   // todo: could do more work to figure out types for 'new' nodes

   return 0;
   }



bool
J9::Node::isTruncatingOrWideningAggrOrBCD()
   {
   TR_ASSERT(self()->getType().isAggregate() || self()->getType().isBCD(),
           "truncatingOrWideningAggrOrBCD should only be called on BCD or aggr!");
   TR_ASSERT(self()->getValueChild() != NULL,
           "getValueChild should have a value!");

   int32_t  nodeSize = 0;
   int32_t valueSize = 0;

   if (self()->getType().isAggregate())
      {
      nodeSize = self()->getSize();
      valueSize = self()->getValueChild()->getSize();
      }
   else if (self()->getType().isBCD())
      {
      nodeSize = self()->getDecimalPrecision();
      valueSize = self()->getValueChild()->getDecimalPrecision();
      }

   return (nodeSize != valueSize);
   }

bool
J9::Node::canRemoveArithmeticOperand()
   {
   TR::Compilation * comp = TR::comp();
   if (!comp->getOption(TR_KeepBCDWidening) && self()->getOpCodeValue() == TR::pdclean)
      {
      return true;
      }
   else if (self()->getOpCodeValue() == TR::pdSetSign)
      {
      // could also do for pdshxSetSign if pdshx is left in place but shift with setSign is usually cheaper than without
      if (self()->isNonNegative() && self()->getFirstChild()->isNonNegative())
         return true;
      else if (self()->isNonPositive() && self()->getFirstChild()->isNonPositive())
         return true;
      }
   return false;
   }

uint32_t
J9::Node::hashOnBCDOrAggrLiteral(char *lit, size_t litSize)
   {
   uint32_t hash = 0;
   for (int32_t i = 0; i < litSize && i < TR_MAX_CHARS_FOR_HASH; i++)
      hash+=lit[i];
   hash+=(litSize*TR_DECIMAL_HASH); // avoid collisions for numbers like 10 and 100
   return hash;
   }



bool
J9::Node::referencesSymbolInSubTree(TR::SymbolReference* symRef, vcount_t visitCount)
   {
   // The visit count in the node must be maintained by this method.
   //
   vcount_t oldVisitCount = self()->getVisitCount();
   if (oldVisitCount == visitCount)
      return false;
   self()->setVisitCount(visitCount);

   if (self()->getOpCode().hasSymbolReference())
      {
      if (self()->getSymbolReference()->getReferenceNumber() == symRef->getReferenceNumber())
         {
         return true;
         }
      }

   // For all other subtrees collect all symbols that could be killed between
   // here and the next reference.
   //
   for (int32_t i = self()->getNumChildren()-1; i >= 0; i--)
      {
      TR::Node * child = self()->getChild(i);
      if (child->referencesSymbolInSubTree(symRef, visitCount)) return true;
      }

   return false;
   }

bool
J9::Node::referencesMayKillAliasInSubTree(TR::Node * rootNode, vcount_t visitCount)
   {
   TR::Compilation * comp = TR::comp();
   TR::SparseBitVector  references (comp->allocator());
   self()->getSubTreeReferences(references, visitCount);

   return rootNode->mayKill().containsAny(references, comp);
   }

void
J9::Node::getSubTreeReferences(TR::SparseBitVector &references, vcount_t visitCount) {
  // The visit count in the node must be maintained by this method.
  //
  vcount_t oldVisitCount = self()->getVisitCount();

  if (oldVisitCount == visitCount) return;

  self()->setVisitCount(visitCount);

  if (self()->getOpCode().hasSymbolReference() &&
      self()->getSymbolReference() &&
      (self()->getOpCodeValue() != TR::loadaddr)) // a loadaddr is not really a reference
    {
      references[self()->getSymbolReference()->getReferenceNumber()]=true;
    }

  for (int32_t i = self()->getNumChildren()-1; i >= 0; i--)
    self()->getChild(i)->getSubTreeReferences(references, visitCount);
}

TR_ParentOfChildNode*
J9::Node::referencesSymbolExactlyOnceInSubTree(
   TR::Node* parent, int32_t childNum, TR::SymbolReference* symRef, vcount_t visitCount)
   {
   // The visit count in the node must be maintained by this method.
   //
   TR::Compilation * comp = TR::comp();
   vcount_t oldVisitCount = self()->getVisitCount();
   if (oldVisitCount == visitCount)
      return NULL;
   self()->setVisitCount(visitCount);

   if (self()->getOpCode().hasSymbolReference() && (self()->getSymbolReference()->getReferenceNumber() == symRef->getReferenceNumber()))
      {
      TR_ParentOfChildNode* pNode = new (comp->trStackMemory()) TR_ParentOfChildNode(parent, childNum);
      return pNode;
      }

   // For all other subtrees, see if any has a ref. If exactly one does, return it
   TR_ParentOfChildNode* matchNode=NULL;
   for (int32_t i = self()->getNumChildren()-1; i >= 0; i--)
      {
      TR::Node * child = self()->getChild(i);
      TR_ParentOfChildNode* curr = child->referencesSymbolExactlyOnceInSubTree(self(), i, symRef, visitCount);
      if (curr)
         {
         if (matchNode)
            {
            return NULL;
            }
         matchNode = curr;
         }
      }

   return matchNode;
   }

/**
 * Node field functions
 */

#ifdef TR_TARGET_S390

TR_StorageReference *
J9::Node::getStorageReferenceHint()
   {
   TR_ASSERT(self()->getOpCode().canHaveStorageReferenceHint(), "attempting to access _storageReferenceHint field for node %s %p that does not have it", self()->getOpCode().getName(), this);
   if (_storageReferenceHint && !_storageReferenceHint->hintHasBeenUsed())
      return _storageReferenceHint;
   else
      return NULL;
   }

TR_StorageReference *
J9::Node::setStorageReferenceHint(TR_StorageReference *s)
   {
   TR_ASSERT(self()->getOpCode().canHaveStorageReferenceHint(), "attempting to access _storageReferenceHint field for node %s %p that does not have it", self()->getOpCode().getName(), this);
   return (_storageReferenceHint = s);
   }

#endif

#ifdef SUPPORT_DFP

long double
J9::Node::setLongDouble(long double d)
   {
   self()->freeExtensionIfExists();
   return (*(long double *)_unionBase._unionedWithChildren._constData= d);
   }

long double
J9::Node::getLongDouble()
   {
   return *(long double *)_unionBase._unionedWithChildren._constData;
   }

#endif

/**
 * Node field functions end
 */





/**
 * UnionPropertyB functions
 */

bool
J9::Node::hasDecimalInfo()
   {
   // _decimalInfo is only valid if opcode has datatype
   if (self()->getOpCode().hasNoDataType())
      return false;

   // _decimalInfo is used for those languages that return true as determined by the code below
   return
      // Any node with a BCD type (pdloadi, pdadd, zd2pd, pdstorei etc)
      self()->getType().isBCD() ||
      // Any node with DFP type (dfadd, dfloadi, deconst etc) for the result precision digits
      self()->getType().isDFP()
      // Conversions from a BCD type to a float to encode # of fractional digits (pd2f, pd2d, pd2de etc)
      || self()->getOpCode().isConversionWithFraction()
      // BCD types and also any BCD ifs and compares (e.g. ifpdcmpxx, pdcmpxx)
      || self()->chkOpsCastedToBCD();
   }

bool
J9::Node::hasBCDFlags()
   {
   // used for BCDCHK only
   return self()->getOpCodeValue() == TR::BCDCHK;
   }

J9::Node::UnionPropertyB_Type
J9::Node::getUnionPropertyB_Type()
   {
   if (self()->hasDecimalInfo())
      // in order to determine this correctly, require children and symRef to be already defined
      // as depends on getDataType which in term depends on these
      return HasDecimalInfo;
   else if (self()->hasBCDFlags())
      return HasBcdFlags;
   else
      return HasNoUnionPropertyB;
   }

bool
J9::Node::hasDecimalPrecision()
   {
   return self()->getType().isBCD();
   }

bool
J9::Node::hasDecimalAdjust()
   {
   return !self()->getOpCode().isShift() && !self()->getOpCode().isConversionWithFraction() && self()->getType().isBCD();
   }

bool
J9::Node::hasSetSign()
   {
   return self()->getType().isBCD() && self()->getOpCode().isSetSignOnNode();
   }

bool
J9::Node::hasDecimalFraction()
   {
   return self()->getOpCode().isConversionWithFraction();
   }

bool
J9::Node::hasDecimalRound()
   {
   return self()->getType().isBCD() && !self()->getOpCode().isRightShift();
   }



void
J9::Node::setDecimalPrecision(int32_t p)
   {
   TR_ASSERT(self()->getType().isBCD(), "opcode not supported for setDecimalPrecision on node %p\n", self());
   TR_ASSERT(p > 0 && p <= TR_MAX_DECIMAL_PRECISION, "unexpected decimal precision %d on node %p\n", p, self());
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalPrecision field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   if (_unionPropertyB._decimalInfo._decimalPrecision != TR::DataType::getInvalidDecimalPrecision() &&
       (uint32_t)p < _unionPropertyB._decimalInfo._decimalPrecision)
      {
      if (self()->getKnownOrAssumedSignCode() != raw_bcd_sign_0xc)
         {
         _unionPropertyB._decimalInfo._hasCleanSign = 0;
         }
      if (self()->chkSkipPadByteClearing())
         self()->setSkipPadByteClearing(false);
      }
   _unionPropertyB._decimalInfo._decimalPrecision = (uint32_t)p;
   }

uint8_t
J9::Node::getDecimalPrecision()
   {
   TR_ASSERT(self()->getType().isBCD(), "opcode not supported for getDecimalPrecision on node %p\n", self());
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalPrecision field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   TR_ASSERT(_unionPropertyB._decimalInfo._decimalPrecision > 0 && _unionPropertyB._decimalInfo._decimalPrecision <= TR_MAX_DECIMAL_PRECISION,
             "unexpected decimal precision %d on node %p\n", _unionPropertyB._decimalInfo._decimalPrecision, self());
   return _unionPropertyB._decimalInfo._decimalPrecision;
   }



// These precision setting routines set a precision big enough to hold the full computed result
// A caller or codegen may choose to set a different value (bigger or smaller) to satisfy a specific
// semantic or encoding requirement -- see getPDMulEncodedPrecision et al. in the platform code generators
void
J9::Node::setPDMulPrecision()
   {
   TR_ASSERT(self()->getOpCode().isPackedMultiply(), "setPDMulPrecision only valid for pdmul nodes\n");
   TR_ASSERT(self()->getNumChildren() >= 2, "expecting >= 2 children and not %d children on a packed multiply node\n", self()->getNumChildren());
   self()->setDecimalPrecision(self()->getFirstChild()->getDecimalPrecision() + self()->getSecondChild()->getDecimalPrecision());
   }

void
J9::Node::setPDAddSubPrecision()
   {
   TR_ASSERT(self()->getOpCode().isPackedAdd() || self()->getOpCode().isPackedSubtract(), "setPDAddSubPrecision only valid for pdadd/pdsub nodes\n");
   TR_ASSERT(self()->getNumChildren() >= 2, "expecting >= 2 children and not %d children on a packed add/sub node\n", self()->getNumChildren());
   self()->setDecimalPrecision(std::max(self()->getFirstChild()->getDecimalPrecision(), self()->getSecondChild()->getDecimalPrecision())+1);
   }



bool
J9::Node::isDFPModifyPrecision()
   {
   TR_ASSERT(self()->getType().isDFP(), "opcode not supported for isDFPModifyPrecision on node %p\n", self());
   if (self()->getOpCodeValue() == TR::ILOpCode::modifyPrecisionOpCode(self()->getDataType()))
      return true;
   return false;
   }

void
J9::Node::setDFPPrecision(int32_t p)
   {
   TR_ASSERT(self()->getType().isDFP(), "opcode not supported for setDFPPrecision on node %p\n", self());
   TR_ASSERT(p > 0 && p <= TR_MAX_DECIMAL_PRECISION, "unexpected DFP precision %d on node %p\n", p, self());
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalPrecision field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   _unionPropertyB._decimalInfo._decimalPrecision = (uint32_t)p;
   }

uint8_t
J9::Node::getDFPPrecision()
   {
   TR_ASSERT(self()->getType().isDFP(), "opcode not supported for getDFPPrecision on node %s %p\n", self()->getOpCode().getName(), self());
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalPrecision field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   TR_ASSERT(_unionPropertyB._decimalInfo._decimalPrecision > 0 && _unionPropertyB._decimalInfo._decimalPrecision <= TR_MAX_DECIMAL_PRECISION,
             "unexpected DFP precision %d on node %s %p\n", _unionPropertyB._decimalInfo._decimalPrecision, self()->getOpCode().getName(), self());
   return _unionPropertyB._decimalInfo._decimalPrecision;
   }



void
J9::Node::setDecimalAdjust(int32_t a)
   {
   TR_ASSERT(a >= TR::DataType::getMinDecimalAdjust() && a <= TR::DataType::getMaxDecimalAdjust(), "unexpected decimal adjust %d\n", a);
   // conversions should not have an adjust
   TR_ASSERT(!self()->getOpCode().isShift(), "decimalAdjust is the 2nd child on pdshr/pdshl nodes\n");
   TR_ASSERT(!self()->getOpCode().isConversionWithFraction(), "conversions nodes should not use setDecimalAdjust\n");
   TR_ASSERT(self()->getType().isBCD(), "type not supported for setDecimalAdjust\n");
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalAdjustOrFractionOrDivisor field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   _unionPropertyB._decimalInfo._decimalAdjustOrFractionOrDivisor = a;
   }

int32_t
J9::Node::getDecimalAdjust()
   {
   // conversions should not have an adjust
   TR_ASSERT(!self()->getOpCode().isConversionWithFraction(), "conversions nodes should not use getDecimalAdjust\n");
   TR_ASSERT(self()->getType().isBCD(), "type not supported for getDecimalAdjust\n");
   int64_t adjust = 0;
   if (self()->getOpCode().isShift() && self()->getSecondChild()->getOpCode().isLoadConst())
      adjust = self()->getOpCode().isRightShift() ? -self()->getSecondChild()->get64bitIntegralValue() : self()->getSecondChild()->get64bitIntegralValue();
   else
      {
      TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalAdjustOrFractionOrDivisor field for node %s %p that does not have it", self()->getOpCode().getName(), self());
      adjust = _unionPropertyB._decimalInfo._decimalAdjustOrFractionOrDivisor;
      }
   TR_ASSERT(adjust >= TR::DataType::getMinDecimalAdjust() && adjust <= TR::DataType::getMaxDecimalAdjust(),
             "unexpected decimal adjust %d\n", (int32_t)adjust);
   return (int32_t)adjust;
   }



void
J9::Node::setDecimalFraction(int32_t f)
   {
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalAdjustOrFractionOrDivisor field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   TR_ASSERT(f >= TR::DataType::getMinDecimalFraction() && f <= TR::DataType::getMaxDecimalFraction(), "unexpected decimal fraction %d\n", f);
   TR_ASSERT(self()->getOpCode().isConversionWithFraction(), "only valid for conversion nodes that have a fraction\n"); // such as f2pd or pd2f
   _unionPropertyB._decimalInfo._decimalAdjustOrFractionOrDivisor = f;
   }

int32_t
J9::Node::getDecimalFraction()
   {
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalAdjustOrFractionOrDivisor field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   TR_ASSERT(_unionPropertyB._decimalInfo._decimalAdjustOrFractionOrDivisor >= TR::DataType::getMinDecimalFraction() && _unionPropertyB._decimalInfo._decimalAdjustOrFractionOrDivisor <= TR::DataType::getMaxDecimalFraction(),
             "unexpected decimal fraction %d\n", _unionPropertyB._decimalInfo._decimalAdjustOrFractionOrDivisor);
   TR_ASSERT(self()->getOpCode().isConversionWithFraction(), "only valid for conversion nodes that have a fraction\n"); // such as f2pd or pd2f
   return _unionPropertyB._decimalInfo._decimalAdjustOrFractionOrDivisor;
   }



/**
 * Source precisions are valid for conversions from a non-BCD type to a BCD type
 */
bool
J9::Node::canHaveSourcePrecision()
   {
   if (self()->getOpCode().isConversion() && self()->getType().isBCD() && !self()->getFirstChild()->getType().isBCD())
      return true;
   return false;
   }

bool
J9::Node::hasSourcePrecision()
   {
   if (!self()->canHaveSourcePrecision())
      return false;
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalSourcePrecisionOrDividend field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   return (_unionPropertyB._decimalInfo._decimalSourcePrecisionOrDividend > 0);
   }

void
J9::Node::setSourcePrecision(int32_t prec)
   {
   TR_ASSERT(self()->canHaveSourcePrecision(), "setSourcePrecision can only be called on a non-BCD-to-BCD conversion node\n");
   TR_ASSERT(prec >= 1, "source precision must be >= 1\n");
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalSourcePrecisionOrDividend field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   _unionPropertyB._decimalInfo._decimalSourcePrecisionOrDividend = prec;
   }

int32_t
J9::Node::getSourcePrecision()
   {
   TR_ASSERT(self()->canHaveSourcePrecision(), "getSourcePrecision can only be called on a non-BCD-to-BCD conversion node\n");
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalSourcePrecisionOrDividend field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   if (_unionPropertyB._decimalInfo._decimalSourcePrecisionOrDividend == 0)
      return TR_MAX_DECIMAL_PRECISION;
   else
      return _unionPropertyB._decimalInfo._decimalSourcePrecisionOrDividend;
   }

int32_t
J9::Node::getDecimalAdjustOrFractionOrDivisor()
   {
   if (self()->getOpCode().isConversionWithFraction())
      return self()->getDecimalFraction();
   else
      return self()->getDecimalAdjust();
   }

int32_t
J9::Node::getDecimalRoundOrDividend()
   {
   return self()->getDecimalRound();
   }

bool
J9::Node::isSetSignValueOnNode()
   {
   if (self()->hasSetSign())
      {
      TR_ASSERT(self()->getSetSign() != raw_bcd_sign_unknown,"%s (%p) is a setSign node but does not have a value setSign value\n", self()->getOpCode().getName(), self());
      return true;
      }
   else
      {
      return false;
      }
   }

void
J9::Node::setSetSign(TR_RawBCDSignCode setSign)
   {
   TR_ASSERT(self()->hasSetSign(), "setSetSign only supported for setsign nodes (%s %p)\n", self()->getOpCode().getName(), self());
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo._setSign field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   _unionPropertyB._decimalInfo._setSign = setSign;
   }

TR_RawBCDSignCode
J9::Node::getSetSign()
   {
   TR_ASSERT(self()->hasSetSign(), "getSetSign only supported for setsign nodes (%s %p)\n", self()->getOpCode().getName(), self());
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo._setSign field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   return _unionPropertyB._decimalInfo._setSign;
   }

bool
J9::Node::isDecimalSizeAndShapeEquivalent(TR::Node *other)
   {
   if (self()->getDecimalPrecision()                 == other->getDecimalPrecision() &&
       self()->getDecimalAdjustOrFractionOrDivisor() == other->getDecimalAdjustOrFractionOrDivisor() &&
       self()->getDecimalRoundOrDividend()           == other->getDecimalRoundOrDividend())
      {
      if (self()->getOpCode().isSetSignOnNode() && other->getOpCode().isSetSignOnNode() &&
          self()->getSetSign() != other->getSetSign())
         {
         return false;
         }
      else
         {
         return true;
         }
      }
   else
      {
      return false;
      }
   }



void
J9::Node::setDecimalRound(int32_t r)
   {
   TR_ASSERT(r >= 0 && r <= TR::DataType::getMaxDecimalRound(), "unexpected decimal round %d\n", r);
   TR_ASSERT(self()->getType().isBCD(), "type not supported for setDecimalRound\n");
   TR_ASSERT(!self()->getOpCode().isRightShift(), "decimalRound is the 3rd child on pdshr nodes\n");
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo._round field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   _unionPropertyB._decimalInfo._round = (r > 0 ? 1 : 0);
   }

uint8_t
J9::Node::getDecimalRound()
   {
   TR_ASSERT(self()->getType().isBCD(), "type not supported for getDecimalRound\n");
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo._round field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   int64_t round = 0;
   if (self()->getOpCode().isPackedRightShift() && self()->getChild(2)->getOpCode().isLoadConst())
      round = self()->getChild(2)->get64bitIntegralValue();
   else
      round = (_unionPropertyB._decimalInfo._round == 1 ? 5 : 0);
   TR_ASSERT(round >= 0 && round <= TR::DataType::getMaxDecimalRound() && round <= TR::getMaxUnsigned<TR::Int8>(), "unexpected decimal round %d\n", (int32_t)round);
   return (uint8_t)round;
   }



bool
J9::Node::signStateIsKnown()
   {
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo._signStateIsKnown field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   return _unionPropertyB._decimalInfo._signStateIsKnown == 1;
   }

void
J9::Node::setSignStateIsKnown()
   {
   if (self()->signStateIsAssumed())
      self()->resetDecimalSignFlags(); // ensure any lingering assumed sign state is cleared
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo._signStateIsKnown field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   _unionPropertyB._decimalInfo._signStateIsKnown = 1;
   }

bool
J9::Node::signStateIsAssumed()
   {
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo._signStateIsKnown field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   return _unionPropertyB._decimalInfo._signStateIsKnown == 0;
   }

void
J9::Node::setSignStateIsAssumed()
   {
   if (self()->signStateIsKnown())
      self()->resetDecimalSignFlags(); // ensure any lingering known sign state is cleared
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo._signStateIsKnown field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   _unionPropertyB._decimalInfo._signStateIsKnown = 0;
   }



bool
J9::Node::hasKnownCleanSign()
   {
   TR_ASSERT(self()->getType().isBCD(), "hasKnownCleanSign only supported for BCD type nodes\n");
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo._hasCleanSign field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   if (self()->alwaysGeneratesAKnownCleanSign())
      return true;
   else
      return self()->signStateIsKnown() && _unionPropertyB._decimalInfo._hasCleanSign == 1;
   }

void
J9::Node::setHasKnownCleanSign(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->getType().isBCD(), "setHasKnownCleanSign only supported for BCD type nodes\n");
   if (self()->getType().isBCD() &&
       performNodeTransformation2(c, "O^O NODE FLAGS: Setting hasKnownCleanSign flag on node %p to %d\n", self(), v))
      {
      self()->setSignStateIsKnown();
      TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo._hasCleanSign field for node %s %p that does not have it", self()->getOpCode().getName(), self());
      _unionPropertyB._decimalInfo._hasCleanSign = v ? 1 : 0;
      }
   }

bool
J9::Node::hasAssumedCleanSign()
   {
   TR_ASSERT(self()->getType().isBCD(), "hasAssumedCleanSign only supported for BCD type nodes\n");
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo._hasCleanSign field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   return self()->signStateIsAssumed() && _unionPropertyB._decimalInfo._hasCleanSign == 1;
   }

void
J9::Node::setHasAssumedCleanSign(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getType().isBCD(), "setHasAssumedCleanSign only supported for BCD type nodes\n");
   if (self()->getType().isBCD() &&
       performNodeTransformation2(c, "O^O NODE FLAGS: Setting hasAssumedCleanSign flag on node %p to %d\n", self(), v))
      {
      self()->setSignStateIsAssumed();
      TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo._hasCleanSign field for node %s %p that does not have it", self()->getOpCode().getName(), self());
      _unionPropertyB._decimalInfo._hasCleanSign = v ? 1 : 0;
      }
   }

bool
J9::Node::hasKnownOrAssumedCleanSign()
   {
   TR_ASSERT(self()->getType().isBCD(), "hasKnownOrAssumedCleanSign only supported for BCD type nodes\n");
   return self()->hasKnownCleanSign() || self()->hasAssumedCleanSign();
   }

void
J9::Node::setHasKnownAndAssumedCleanSign(bool v)
   {
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo._hasCleanSign field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   _unionPropertyB._decimalInfo._hasCleanSign = 0;
   }

void
J9::Node::transferCleanSign(TR::Node *srcNode)
   {
   if (srcNode == NULL) return;

   if (srcNode->hasKnownCleanSign())
      self()->setHasKnownCleanSign(true);
   else if (srcNode->hasAssumedCleanSign())
      self()->setHasAssumedCleanSign(true);
   }



bool
J9::Node::hasKnownPreferredSign()
   {
   TR_ASSERT(self()->getType().isBCD(), "hasKnownPreferredSign only supported for BCD type nodes\n");
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo._hasPreferredSign field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   return self()->signStateIsKnown() && _unionPropertyB._decimalInfo._hasPreferredSign == 1;
   }

void
J9::Node::setHasKnownPreferredSign(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->getType().isBCD(), "setHasKnownPreferredSign only supported for BCD type nodes\n");
   if (self()->getType().isBCD() &&
       performNodeTransformation2(c, "O^O NODE FLAGS: Setting hasKnownPreferredSign flag on node %p to %d\n", self(), v))
      {
      self()->setSignStateIsKnown();
      TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo._hasPreferredSign field for node %s %p that does not have it", self()->getOpCode().getName(), self());
      _unionPropertyB._decimalInfo._hasPreferredSign= v ? 1 : 0;
      }
   }

bool
J9::Node::hasAssumedPreferredSign()
   {
   TR_ASSERT(self()->getType().isBCD(), "hasAssumedPreferredSign only supported for BCD type nodes\n");
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo._hasPreferredSign field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   return self()->signStateIsAssumed() && _unionPropertyB._decimalInfo._hasPreferredSign == 1;
   }

void
J9::Node::setHasAssumedPreferredSign(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->getType().isBCD(), "setHasAssumedPreferredSign only supported for BCD type nodes\n");
   if (self()->getType().isBCD() &&
       performNodeTransformation2(c, "O^O NODE FLAGS: Setting hasAssumedPreferredSign flag on node %p to %d\n", self(), v))
      {
      self()->setSignStateIsAssumed();
      TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo._hasPreferredSign field for node %s %p that does not have it", self()->getOpCode().getName(), self());
      _unionPropertyB._decimalInfo._hasPreferredSign= v ? 1 : 0;
      }
   }

bool
J9::Node::hasKnownOrAssumedPreferredSign()
   {
   TR_ASSERT(self()->getType().isBCD(), "hasKnownOrAssumedPreferredSign only supported for BCD type nodes\n");
   return self()->hasKnownPreferredSign() || self()->hasAssumedPreferredSign();
   }

bool
J9::Node::hasKnownSignCode()
   {
   TR_ASSERT(self()->getType().isBCD(), "hasKnownSignCode only supported for BCD type nodes\n");
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo._signCode field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   return self()->signStateIsKnown() && (_unionPropertyB._decimalInfo._signCode != raw_bcd_sign_unknown);
   }

TR_RawBCDSignCode
J9::Node::getKnownSignCode()
   {
   TR_ASSERT(self()->getType().isBCD(), "getKnownSignCode only supported for BCD type nodes\n");
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo._signCode field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   return self()->signStateIsKnown() ? _unionPropertyB._decimalInfo._signCode : raw_bcd_sign_unknown;
   }

bool
J9::Node::hasAssumedSignCode()
   {
   TR_ASSERT(self()->getType().isBCD(), "hasAssumedSignCode only supported for BCD type nodes\n");
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo._signCode field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   return self()->signStateIsAssumed() && (_unionPropertyB._decimalInfo._signCode != raw_bcd_sign_unknown);
   }

TR_RawBCDSignCode
J9::Node::getAssumedSignCode()
   {
   TR_ASSERT(self()->getType().isBCD(), "getAssumedSignCode only supported for BCD type nodes\n");
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo._signCode field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   return self()->signStateIsAssumed() ? _unionPropertyB._decimalInfo._signCode : raw_bcd_sign_unknown;
   }

bool
J9::Node::hasKnownOrAssumedSignCode()
   {
   TR_ASSERT(self()->getType().isBCD(), "hasKnownOrAssumedSignCode only supported for BCD type nodes\n");
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalPrecision field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   return self()->hasKnownSignCode() || self()->hasAssumedSignCode();
   }

TR_RawBCDSignCode
J9::Node::getKnownOrAssumedSignCode()
   {
   TR_ASSERT(self()->getType().isBCD(), "getKnownSignCode only supported for BCD type nodes\n");
   if (self()->hasKnownSignCode())
      return self()->getKnownSignCode();
   else if (self()->hasAssumedSignCode())
      return self()->getAssumedSignCode();
   else
      return raw_bcd_sign_unknown;
   }

void
J9::Node::setKnownOrAssumedSignCode(TR_RawBCDSignCode sign, bool isKnown)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->getType().isBCD(), "setKnownSignCode only supported for BCD type nodes\n");
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo fields for node %s %p that does not have it", self()->getOpCode().getName(), self());
   // TODO: start tracking SeparateOneByte and SeparateTwoByte sign sizes too
   if (self()->getType().isBCD() && TR::Node::typeSupportedForSignCodeTracking(self()->getDataType()))
      {
      if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting knownSignCode on node %p to %s\n", self(), TR::DataType::getName(sign)))
         {
         if (isKnown)
            self()->setSignStateIsKnown();
         else
            self()->setSignStateIsAssumed();
         _unionPropertyB._decimalInfo._signCode = sign;
         if (TR::DataType::rawSignIsPositive(self()->getDataType(), TR::DataType::getValue(sign)))
            self()->setIsNonNegative(true); // >= 0
         else if (TR::DataType::rawSignIsNegative(self()->getDataType(), TR::DataType::getValue(sign)))
            self()->setIsNonPositive(true); // <= 0 negative zero is possible
         }
      if ((sign == raw_bcd_sign_0xc) && (0xc == TR::DataType::getPreferredPlusCode()))
         _unionPropertyB._decimalInfo._hasCleanSign = 1;
      if ((sign == raw_bcd_sign_0xc || sign == raw_bcd_sign_0xd) && (0xc == TR::DataType::getPreferredPlusCode() && 0xd == TR::DataType::getPreferredMinusCode()))
         _unionPropertyB._decimalInfo._hasPreferredSign = 1;
      }
   }

void
J9::Node::setKnownSignCode(TR_RawBCDSignCode sign)
   {
   self()->setKnownOrAssumedSignCode(sign, true); // isKnown=true
   }

void
J9::Node::transferSignCode(TR::Node *srcNode)
   {
   if (srcNode == NULL) return;

   if (srcNode->hasKnownSignCode())
      self()->setKnownSignCode(srcNode->getKnownSignCode());
   else if (srcNode->hasAssumedSignCode())
      self()->setAssumedSignCode(srcNode->getAssumedSignCode());
   }



/**
 * The mapping to/from TR_BCDSignCode from/to the 0xc/0xd/0xf 'raw' encodings in setKnownSignCode/knownSignCodeIs below
 * is temporary until TR_BCDSignCode is tracked on the nodes instead of TR_RawBCDSignCode
 * Doing it this way allows new code (such as VP constrainBCDAggrLoad) to start using this new more general interface
 * The final code will be just calling setKnownOrAssumedSignCode(sign, true, c) but now the 'sign' parm will be TR_BCDSignCode of TR_RawBCDSignCode
 */
bool
J9::Node::knownSignCodeIs(TR_BCDSignCode sign)
   {
   if (self()->hasKnownSignCode() &&
       TR::DataType::getBCDSignFromRawSign(self()->getKnownSignCode()) == sign)
      {
      return true;
      }
   else
      {
      return false;
      }
   }

void
J9::Node::setKnownSignCode(TR_BCDSignCode sign)
   {
   if (TR::Node::typeSupportedForSignCodeTracking(self()->getDataType()))
      {
      TR_RawBCDSignCode rawBCDSign = TR::DataType::getRawSignFromBCDSign(sign);
      if (rawBCDSign != raw_bcd_sign_unknown)
         self()->setKnownSignCode(rawBCDSign);
      }
   }

bool
J9::Node::assumedSignCodeIs(TR_BCDSignCode sign)
   {
   if (self()->hasAssumedSignCode() &&
       TR::DataType::getBCDSignFromRawSign(self()->getAssumedSignCode()) == sign)
      {
      return true;
      }
   else
      {
      return false;
      }
   }

void
J9::Node::setAssumedSignCode(TR_RawBCDSignCode sign)
   {
   self()->setKnownOrAssumedSignCode(sign, false); // isKnown=false
   }

bool
J9::Node::knownOrAssumedSignCodeIs(TR_BCDSignCode sign)
   {
   if (self()->knownSignCodeIs(sign) || self()->assumedSignCodeIs(sign))
      {
      return true;
      }
   else
      {
      return false;
      }
   }



/**
 * The hasSignState property is stored in the reverse sense to make the interface easier to use (at the expense of some confusion here)
 */
bool
J9::Node::hasSignStateOnLoad()
   {
   if (!self()->getOpCode().isBCDLoad())
      return false;

   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo._hasNoSignStateOnLoad field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   if (_unionPropertyB._decimalInfo._hasNoSignStateOnLoad == 0) // !NoSign == HasSignState
      return true;   // i.e. sign state of load must be preserved (no ZAP widening, for example, as this could change 0xf -> 0xc) -- this is the conservative setting
   else if (_unionPropertyB._decimalInfo._hasNoSignStateOnLoad == 1)
      return false;  // i.e. no particular sign state had assumed on a load (so ZAP widening, for example, is allowed)
   else
      TR_ASSERT(false, "unexpected noSignState setting on %d on node %p\n", _unionPropertyB._decimalInfo._hasNoSignStateOnLoad, self());

   return true; // conservative setting is that there is sign state to be preserved.
   }

void
J9::Node::setHasSignStateOnLoad(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->getOpCode().isBCDLoad(), "only BCDLoads can use setHasSignStateOnLoad (%s %p)\n", self()->getOpCode().getName(), self());

   if (self()->getOpCode().isBCDLoad() &&
       performNodeTransformation2(c, "O^O NODE FLAGS: Setting _hasNoSignStateOnLoad flag on node %p to %d\n", self(), v?0:1))
      {
      TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo._hasNoSignStateOnLoad field for node %s %p that does not have it", self()->getOpCode().getName(), self());
      _unionPropertyB._decimalInfo._hasNoSignStateOnLoad = v ? 0 : 1; // v=true is the conservative setting, v=false will allow clobbering of the sign code
      }
   }



void
J9::Node::transferSignState(TR::Node *srcNode, bool digitsLost)
   {
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo fields for node %s %p that does not have it", self()->getOpCode().getName(), self());
   _unionPropertyB._decimalInfo._signStateIsKnown = srcNode->signStateIsKnown();
   _unionPropertyB._decimalInfo._hasCleanSign = digitsLost ? 0 : srcNode->hasKnownOrAssumedCleanSign(); //  '10 0d' might be truncated to negative zero '0 00d' -- so no longer clean
   _unionPropertyB._decimalInfo._hasPreferredSign = srcNode->hasKnownOrAssumedPreferredSign();
   _unionPropertyB._decimalInfo._signCode = srcNode->getKnownOrAssumedSignCode();
   if (self()->getOpCode().isBCDLoad())
      self()->setHasSignStateOnLoad(srcNode->hasSignStateOnLoad());
   }

bool
J9::Node::hasAnyKnownOrAssumedSignState()
   {
   if (self()->hasKnownOrAssumedCleanSign() ||
       self()->hasKnownOrAssumedPreferredSign() ||
       self()->hasKnownOrAssumedSignCode())
      {
      return true;
      }
   else
      {
      return false;
      }
   }

bool
J9::Node::hasAnyDecimalSignState()
   {
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo fields for node %s %p that does not have it", self()->getOpCode().getName(), self());
   if (_unionPropertyB._decimalInfo._hasCleanSign == 1 ||
       _unionPropertyB._decimalInfo._hasPreferredSign == 1 ||
       (self()->getOpCode().isLoadVar() && self()->hasSignStateOnLoad()) ||
       _unionPropertyB._decimalInfo._signCode != raw_bcd_sign_unknown)
      {
      return true;
      }
   else
      {
      return false;
      }
   }

void
J9::Node::resetDecimalSignFlags()
   {
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo fields for node %s %p that does not have it", self()->getOpCode().getName(), self());
   _unionPropertyB._decimalInfo._signStateIsKnown = 0;
   _unionPropertyB._decimalInfo._hasCleanSign = 0;
   _unionPropertyB._decimalInfo._hasPreferredSign = 0;
   _unionPropertyB._decimalInfo._hasNoSignStateOnLoad = 0;
   _unionPropertyB._decimalInfo._signCode=raw_bcd_sign_unknown;
   }

void
J9::Node::resetSignState()
   {
   self()->resetDecimalSignFlags();
   self()->setIsZero(false);
   self()->setIsNonZero(false);
   self()->setIsNonPositive(false);
   self()->setIsNonNegative(false);
   }



bool
J9::Node::isSignStateEquivalent(TR::Node *other)
   {
   if (self()->signStateIsKnown()       == other->signStateIsKnown() &&
       self()->signStateIsAssumed()     == other->signStateIsAssumed() &&
       self()->hasKnownCleanSign()       == other->hasKnownCleanSign() &&
       self()->hasAssumedCleanSign()     == other->hasAssumedCleanSign() &&
       self()->hasKnownPreferredSign()   == other->hasKnownPreferredSign() &&
       self()->hasAssumedPreferredSign() == other->hasAssumedPreferredSign() &&
       self()->hasKnownSignCode()         == other->hasKnownSignCode() &&
       self()->hasAssumedSignCode()       == other->hasAssumedSignCode() &&
       self()->hasSignStateOnLoad()       == other->hasSignStateOnLoad())
      {
      return true;
      }
   else
      {
      return false;
      }
   }

/**
 * if the 'other' node does not currently have any sign state then an improvement will be any sign state set on 'this' node
 */
bool
J9::Node::isSignStateAnImprovementOver(TR::Node *other)
   {
   if (other->hasSignStateOnLoad() || other->hasAnyKnownOrAssumedSignState())
      return false;

   bool thisHasAnyKnownOrAssumedSignState = self()->hasAnyKnownOrAssumedSignState();
   // having some (but undetermined) sign state for loads can prevent optimization or hinder codegen vs not having to assume anything
   // so return false in this case (but wouldn't be incorrect to return true in this case assuming other conditions below hold)
   if (self()->hasSignStateOnLoad() && !thisHasAnyKnownOrAssumedSignState)
      return false;

   return thisHasAnyKnownOrAssumedSignState;  // any sign state info is better than nothing
   }



bool
J9::Node::chkOpsCastedToBCD()
   {
   if (self()->getType().isBCD())
      {
      return true;
      }
   else if (self()->getOpCode().isAnyBCDCompareOp())
      {
      // e.g. pdcmpxx
      return true;
      }
   else
      {
      return false;
      }
   }

bool
J9::Node::castedToBCD()
   {
   if (self()->chkOpsCastedToBCD())
      {
      TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo._castedToBCD field for node %s %p that does not have it", self()->getOpCode().getName(), self());
      return _unionPropertyB._decimalInfo._castedToBCD==1;
      }
   else
      {
      return false;
      }
   }

void
J9::Node::setCastedToBCD(bool v)
   {
   TR_ASSERT(self()->chkOpsCastedToBCD(), "node %s %p must be a BCD type or a BCD if or compare\n", self()->getOpCode().getName(), self());
   TR_ASSERT(self()->hasDecimalInfo(), "attempting to access _decimalInfo._castedToBCD field for node %s %p that does not have it", self()->getOpCode().getName(), self());
   _unionPropertyB._decimalInfo._castedToBCD = v ? 1 : 0;
   }

/**
 * UnionPropertyB functions end
 */





/**
 * Node flag functions
 */

bool
J9::Node::isSpineCheckWithArrayElementChild()
   {
   TR_ASSERT(self()->getOpCode().isSpineCheck(), "assertion failure");
   return _flags.testAny(spineCHKWithArrayElementChild);
   }

void
J9::Node::setSpineCheckWithArrayElementChild(bool v)
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCode().isSpineCheck(), "assertion failure");
   if (performNodeTransformation2(c, "O^O NODE FLAGS: Setting spineCHKWithArrayElementChild flag on node %p to %d\n", self(), v))
      _flags.set(spineCHKWithArrayElementChild, v);
   }

bool
J9::Node::chkSpineCheckWithArrayElementChild()
   {
   return self()->getOpCode().isSpineCheck() && _flags.testAny(spineCHKWithArrayElementChild);
   }

const char *
J9::Node::printSpineCheckWithArrayElementChild()
   {
   return self()->chkSpineCheckWithArrayElementChild() ? "spineCHKWithArrayElementChild " : "";
   }



bool
J9::Node::isUnsafePutOrderedCall()
   {
   if (!self()->getOpCode().isCall())
      return false;

   if (!self()->getSymbol()->isMethod())
      return false;

   bool isPutOrdered = false;
   TR::MethodSymbol *symbol = self()->getSymbol()->getMethodSymbol();
   if (!symbol)
      return false;

   if ((symbol->getRecognizedMethod() == TR::sun_misc_Unsafe_putBooleanOrdered_jlObjectJZ_V) ||
       (symbol->getRecognizedMethod() == TR::sun_misc_Unsafe_putByteOrdered_jlObjectJB_V) ||
       (symbol->getRecognizedMethod() == TR::sun_misc_Unsafe_putCharOrdered_jlObjectJC_V) ||
       (symbol->getRecognizedMethod() == TR::sun_misc_Unsafe_putShortOrdered_jlObjectJS_V) ||
       (symbol->getRecognizedMethod() == TR::sun_misc_Unsafe_putIntOrdered_jlObjectJI_V) ||
       (symbol->getRecognizedMethod() == TR::sun_misc_Unsafe_putLongOrdered_jlObjectJJ_V) ||
       (symbol->getRecognizedMethod() == TR::sun_misc_Unsafe_putFloatOrdered_jlObjectJF_V) ||
       (symbol->getRecognizedMethod() == TR::sun_misc_Unsafe_putDoubleOrdered_jlObjectJD_V) ||
       (symbol->getRecognizedMethod() == TR::sun_misc_Unsafe_putObjectOrdered_jlObjectJjlObject_V))
      isPutOrdered = true;

   return isPutOrdered;
   }

bool
J9::Node::isDontInlinePutOrderedCall()
   {
   TR_ASSERT(self()->getOpCode().isCall(), " Can only call this routine for a call node \n");
   bool isPutOrdered = self()->isUnsafePutOrderedCall();

   TR_ASSERT(isPutOrdered, "attempt to set dontInlinePutOrderedCall flag and not a putOrdered call");
   if (isPutOrdered)
      return _flags.testAny(dontInlineUnsafePutOrderedCall);
   else
      return false;
   }

void
J9::Node::setDontInlinePutOrderedCall()
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getOpCode().isCall(), " Can only call this routine for a call node \n");
   bool isPutOrdered = self()->isUnsafePutOrderedCall();

   TR_ASSERT(isPutOrdered, "attempt to set dontInlinePutOrderedCall flag and not a putOrdered call");
   if (isPutOrdered)
      {
      if (performNodeTransformation1(c, "O^O NODE FLAGS: Setting dontInlineUnsafePutOrderedCall flag on node %p\n", self()))
         _flags.set(dontInlineUnsafePutOrderedCall);
      }

   }

bool
J9::Node::chkDontInlineUnsafePutOrderedCall()
   {
   bool isPutOrdered = self()->isUnsafePutOrderedCall();
   return isPutOrdered && _flags.testAny(dontInlineUnsafePutOrderedCall);
   }

const char *
J9::Node::printIsDontInlineUnsafePutOrderedCall()
   {
   return self()->chkDontInlineUnsafePutOrderedCall() ? "dontInlineUnsafePutOrderedCall " : "";
   }



bool
J9::Node::isUnsafeGetPutCASCallOnNonArray()
   {
   if (!self()->getSymbol()->isMethod())
      return false;
   TR::MethodSymbol *symbol = self()->getSymbol()->getMethodSymbol();
   if (!symbol)
      return false;

   //TR_ASSERT(symbol->castToResolvedMethodSymbol()->getResolvedMethod()->isUnsafeWithObjectArg(), "Attempt to check flag on a method that is not JNI Unsafe that needs special care for arraylets\n");
   TR_ASSERT(symbol->getMethod()->isUnsafeWithObjectArg() || symbol->getMethod()->isUnsafeCAS(),"Attempt to check flag on a method that is not JNI Unsafe that needs special care for arraylets\n");
   return _flags.testAny(unsafeGetPutOnNonArray);
   }

void
J9::Node::setUnsafeGetPutCASCallOnNonArray()
   {
   TR::Compilation * c = TR::comp();
   TR_ASSERT(self()->getSymbol()->isMethod() && self()->getSymbol()->getMethodSymbol(), "setUnsafeGetPutCASCallOnNonArray called on node which is not a call");

   //TR_ASSERT(getSymbol()->getMethodSymbol()->castToResolvedMethodSymbol()->getResolvedMethod()->isUnsafeWithObjectArg(),"Attempt to change flag on a method that is not JNI Unsafe that needs special care for arraylets\n");
   TR_ASSERT(self()->getSymbol()->getMethodSymbol()->getMethod()->isUnsafeWithObjectArg() || self()->getSymbol()->getMethodSymbol()->getMethod()->isUnsafeCAS(),"Attempt to check flag on a method that is not JNI Unsafe that needs special care for arraylets\n");
   if (performNodeTransformation1(c, "O^O NODE FLAGS: Setting unsafeGetPutOnNonArray flag on node %p\n", self()))
      _flags.set(unsafeGetPutOnNonArray);
   }



bool
J9::Node::isProcessedByCallCloneConstrain()
   {
   return self()->getOpCode().isCall() && self()->getOpCodeValue() != TR::arraycopy && _flags.testAny(processedByCallCloneConstrain);
   }

void
J9::Node::setProcessedByCallCloneConstrain()
   {
   _flags.set(processedByCallCloneConstrain, true);
   }



bool
J9::Node::isDAAVariableSlowCall()
   {
   TR_ASSERT(self()->getOpCode().isCall(), "Opcode must be a call");
   return _flags.testAny(DAAVariableSlowCall);
   }

void
J9::Node::setDAAVariableSlowCall(bool v)
   {
   TR_ASSERT(self()->getOpCode().isCall(), "Opcode must be a call");
   _flags.set(DAAVariableSlowCall);
   }



bool
J9::Node::isBCDStoreTemporarilyALoad()
   {
   TR_ASSERT(self()->getOpCode().isBCDLoadVar(),
             "flag only valid for BCD load ops\n");
   if (self()->getOpCode().isBCDLoadVar())
      return _flags.testAny(IsBCDStoreTemporarilyALoad);
   else
      return false;

   }

void
J9::Node::setBCDStoreIsTemporarilyALoad(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->getOpCode().isBCDLoadVar(),
             "flag only valid for BCD load ops\n");
   if (self()->getOpCode().isBCDLoadVar() &&
       performNodeTransformation2(c, "O^O NODE FLAGS: Setting IsBCDStoreTemporarilyALoad flag on node %p to %d\n", self(), v))
      _flags.set(IsBCDStoreTemporarilyALoad, v);
   }



bool
J9::Node::cleanSignDuringPackedLeftShift()
   {
   TR_ASSERT(self()->getOpCode().isPackedLeftShift(), "flag only valid for isPackedLeftShift nodes\n");
   if (self()->getOpCode().isPackedLeftShift())
      return _flags.testAny(CleanSignDuringPackedLeftShift);
   else
      return false;
   }

void
J9::Node::setCleanSignDuringPackedLeftShift(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->getOpCode().isPackedLeftShift(), "flag only valid for isPackedLeftShift nodes\n");
   if (self()->getOpCode().isPackedLeftShift() && performNodeTransformation2(c, "O^O NODE FLAGS: Setting CleanSignDuringPackedLeftShift flag on node %p to %d\n", self(), v))
      _flags.set(CleanSignDuringPackedLeftShift, v);
   }

bool
J9::Node::chkCleanSignDuringPackedLeftShift()
   {
   return self()->getOpCode().isPackedLeftShift() &&
      _flags.testAny(CleanSignDuringPackedLeftShift);
   }

const char *
J9::Node::printCleanSignDuringPackedLeftShift()
   {
   return self()->chkCleanSignDuringPackedLeftShift() ? "cleanSignDuringPackedLeftShift " : "";
   }

bool
J9::Node::chkOpsSkipCopyOnLoad()
   {
   return (self()->getType().isBCD() || self()->getType().isAggregate()) && !self()->getOpCode().isStore() && !self()->getOpCode().isCall();
   }

bool
J9::Node::skipCopyOnLoad()
   {
   TR_ASSERT(self()->chkOpsSkipCopyOnLoad(), "flag only valid for BCD or aggregate non-store and non-call ops\n");
   if ((self()->getType().isBCD() || self()->getType().isAggregate()) && !self()->getOpCode().isStore() && !self()->getOpCode().isCall())
      return _flags.testAny(SkipCopyOnLoad);
   else
      return false;
   }

void
J9::Node::setSkipCopyOnLoad(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->chkOpsSkipCopyOnLoad(), "flag only valid for BCD or aggregate non-store and non-call ops\n");
   if ((self()->getType().isBCD() || self()->getType().isAggregate()) && !self()->getOpCode().isStore() && !self()->getOpCode().isCall() &&
       performNodeTransformation2(c, "O^O NODE FLAGS: Setting skipCopyOnLoad flag on node %p to %d\n", self(), v))
      _flags.set(SkipCopyOnLoad, v);
   }

bool
J9::Node::chkSkipCopyOnLoad()
   {
   return self()->chkOpsSkipCopyOnLoad() && _flags.testAny(SkipCopyOnLoad);
   }

const char *
J9::Node::printSkipCopyOnLoad()
   {
   return self()->chkSkipCopyOnLoad() ? "skipCopyOnLoad " : "";
   }



bool
J9::Node::chkOpsSkipCopyOnStore()
   {
   return self()->getOpCode().isBCDStore();
   }

bool
J9::Node::skipCopyOnStore()
   {
   TR_ASSERT(self()->chkOpsSkipCopyOnStore(), "flag only valid for BCD or aggregate store ops\n");
   if (self()->chkOpsSkipCopyOnStore())
      return _flags.testAny(SkipCopyOnStore);
   else
      return false;
   }

void
J9::Node::setSkipCopyOnStore(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->chkOpsSkipCopyOnStore(), "flag only valid for BCD or aggregate store ops\n");
   if (self()->chkOpsSkipCopyOnStore() &&
       performNodeTransformation2(c, "O^O NODE FLAGS: Setting skipCopyOnStore flag on node %p to %d\n", self(), v))
      {
      _flags.set(SkipCopyOnStore, v);
      }
   }

bool
J9::Node::chkSkipCopyOnStore()
   {
   return self()->chkOpsSkipCopyOnStore() && _flags.testAny(SkipCopyOnStore);
   }

const char *
J9::Node::printSkipCopyOnStore()
   {
   return self()->chkSkipCopyOnStore() ? "skipCopyOnStore " : "";
   }



bool
J9::Node::chkOpsCleanSignInPDStoreEvaluator()
   {
   return self()->getDataType() == TR::PackedDecimal && self()->getOpCode().isStore();
   }

bool
J9::Node::mustCleanSignInPDStoreEvaluator()
   {
   TR_ASSERT(self()->chkOpsCleanSignInPDStoreEvaluator(), "flag only valid for packed decimal store nodes\n");
   if (self()->chkOpsCleanSignInPDStoreEvaluator())
      return _flags.testAny(cleanSignInPDStoreEvaluator);
   else
      return false;
   }

void
J9::Node::setCleanSignInPDStoreEvaluator(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->chkOpsCleanSignInPDStoreEvaluator(), "flag only valid for packed decimal store nodes\n");
   if (self()->chkOpsCleanSignInPDStoreEvaluator() &&
       performNodeTransformation2(c, "O^O NODE FLAGS: Setting cleanSignInPDStoreEvaluator flag on node %p to %d\n", self(), v))
      _flags.set(cleanSignInPDStoreEvaluator, v);
   }

bool
J9::Node::chkCleanSignInPDStoreEvaluator()
   {
   return self()->chkOpsCleanSignInPDStoreEvaluator() && _flags.testAny(cleanSignInPDStoreEvaluator);
   }

const char *
J9::Node::printCleanSignInPDStoreEvaluator()
   {
   return self()->chkCleanSignInPDStoreEvaluator() ? "cleanSignInPDStoreEvaluator " : "";
   }



bool
J9::Node::chkOpsUseStoreAsAnAccumulator()
   {
   return self()->getOpCode().canUseStoreAsAnAccumulator();
   }

bool
J9::Node::useStoreAsAnAccumulator()
   {
   TR_ASSERT(self()->chkOpsUseStoreAsAnAccumulator(), "flag only valid for BCD or aggregate store ops\n");
   if (self()->chkOpsUseStoreAsAnAccumulator())
      return _flags.testAny(UseStoreAsAnAccumulator);
   else
      return false;
   }

void
J9::Node::setUseStoreAsAnAccumulator(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->chkOpsUseStoreAsAnAccumulator(), "flag only valid for BCD or aggregate store ops\n");
   if (self()->chkOpsUseStoreAsAnAccumulator() &&
       performNodeTransformation2(c, "O^O NODE FLAGS: Setting UseStoreAsAnAccumulator flag on node %p to %d\n", self(), v))
      {
      _flags.set(UseStoreAsAnAccumulator, v);
      }
   }

bool
J9::Node::chkUseStoreAsAnAccumulator()
   {
   return self()->chkOpsUseStoreAsAnAccumulator() && _flags.testAny(UseStoreAsAnAccumulator);
   }

const char *
J9::Node::printUseStoreAsAnAccumulator()
   {
   return self()->chkUseStoreAsAnAccumulator() ? "useStoreAsAnAccumulator " : "";
   }



bool
J9::Node::canSkipPadByteClearing()
   {
   TR_ASSERT(self()->getType().isAnyPacked() && !self()->getOpCode().isStore(),
             "flag only valid for wcode non-store packed operations\n");
   if (self()->getType().isAnyPacked() && !self()->getOpCode().isStore())
      return _flags.testAny(skipPadByteClearing);
   else
      return false;
   }

void
J9::Node::setSkipPadByteClearing(bool v)
   {
   TR::Compilation *c = TR::comp();
   TR_ASSERT(self()->getType().isAnyPacked() && !self()->getOpCode().isStore(),
             "flag only valid for wcode non-store packed operations\n");
   if (self()->getType().isAnyPacked() && !self()->getOpCode().isStore() &&
       (c==NULL || performNodeTransformation2(c, "O^O NODE FLAGS: Setting skipPadByteClearing flag on node %p to %d\n", self(), v)))
      _flags.set(skipPadByteClearing, v);
   }

bool
J9::Node::chkSkipPadByteClearing()
   {
   return self()->getType().isAnyPacked() && !self()->getOpCode().isStore() &&
      _flags.testAny(skipPadByteClearing);
   }

const char *
J9::Node::printSkipPadByteClearing()
   {
   return self()->chkSkipPadByteClearing() ? "skipPadByteClearing " : "";
   }



bool
J9::Node::chkOpsIsInMemoryCopyProp()
   {
   TR::Compilation *c = TR::comp();
   return self()->getOpCode().isStore() && c->cg()->IsInMemoryType(self()->getType());
   }

bool
J9::Node::isInMemoryCopyProp()
   {
   if (self()->chkOpsIsInMemoryCopyProp())
      return _flags.testAny(IsInMemoryCopyProp);
   else
      return false;
   }

void
J9::Node::setIsInMemoryCopyProp(bool v)
   {
   if (self()->chkOpsIsInMemoryCopyProp())
      {
      _flags.set(IsInMemoryCopyProp, v);
      }
   }

bool
J9::Node::chkIsInMemoryCopyProp()
   {
   return self()->chkOpsIsInMemoryCopyProp() && _flags.testAny(IsInMemoryCopyProp);
   }

const char *
J9::Node::printIsInMemoryCopyProp()
   {
   return self()->chkIsInMemoryCopyProp() ? "IsInMemoryCopyProp " : "";
   }



bool
J9::Node::chkSharedMemory()
   {
   TR_ASSERT(self()->getType().isAddress(), "flag only valid for addresses\n");
   return _flags.testAny(sharedMemory);
   }

void
J9::Node::setSharedMemory(bool v)
   {
   TR_ASSERT(self()->getType().isAddress(), "flag only valid for addresses\n");
   _flags.set(sharedMemory, v);
   }

const char *
J9::Node::printSharedMemory()
   {
   return self()->getType().isAddress() ? "sharedMemory " : "";
   }

/**
 * Node flag functions end
 */

bool
J9::Node::isArrayCopyCall()
   {
   if (self()->getOpCode().isCall() && self()->getSymbol()->isMethod())
   {
   TR::RecognizedMethod recognizedMethod = self()->getSymbol()->castToMethodSymbol()->getRecognizedMethod();

   if (recognizedMethod == TR::java_lang_System_arraycopy ||
       recognizedMethod == TR::java_lang_String_compressedArrayCopy_BIBII ||
       recognizedMethod == TR::java_lang_String_compressedArrayCopy_BICII ||
       recognizedMethod == TR::java_lang_String_compressedArrayCopy_CIBII ||
       recognizedMethod == TR::java_lang_String_compressedArrayCopy_CICII ||
       recognizedMethod == TR::java_lang_String_decompressedArrayCopy_BIBII ||
       recognizedMethod == TR::java_lang_String_decompressedArrayCopy_BICII ||
       recognizedMethod == TR::java_lang_String_decompressedArrayCopy_CIBII ||
       recognizedMethod == TR::java_lang_String_decompressedArrayCopy_CICII)
      {
      return true;
      }
   else
      {
      TR_Method *method = self()->getSymbol()->castToMethodSymbol()->getMethod();

      // Method may be unresolved but we would still like to transform arraycopy calls so pattern match the signature as well
      if (method != NULL &&
         (method->nameLength() == 9) &&
         (method->classNameLength() == 16) &&
         (strncmp(method->nameChars(), "arraycopy", 9) == 0) &&
         (strncmp(method->classNameChars(), "java/lang/System", 16) == 0))
         {
         return true;
         }
      }
   }

   return OMR::Node::isArrayCopyCall();
   }


bool
J9::Node::requiresRegisterPair(TR::Compilation *comp)
   {
   return self()->getType().isLongDouble() || OMR::NodeConnector::requiresRegisterPair(comp);
   }

bool
J9::Node::canGCandReturn()
   {
   TR::Compilation * comp = TR::comp();
   if (comp->getOption(TR_EnableFieldWatch))
      {
      if (self()->getOpCodeValue() == TR::treetop || self()->getOpCode().isNullCheck() || self()->getOpCode().isAnchor())
         {
         TR::Node * node = self()->getFirstChild();
         if (node->getOpCode().isReadBar() || node->getOpCode().isWrtBar())
            return true;
         }
      }
   return OMR::NodeConnector::canGCandReturn();
   }
