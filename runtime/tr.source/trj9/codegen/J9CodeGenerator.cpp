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

#pragma csect(CODE,"TRJ9CGBase#C")
#pragma csect(STATIC,"TRJ9CGBase#S")
#pragma csect(TEST,"TRJ9CGBase#T")

#include <algorithm>                            // for std::find
#include "codegen/AheadOfTimeCompile.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/PicHelpers.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/Instruction.hpp"              // for Instruction
#include "compile/AOTClassInfo.hpp"
#include "compile/Compilation.hpp"
#include "compile/OSRData.hpp"
#include "compile/VirtualGuard.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/CompilerEnv.hpp"
#include "env/VMJ9.h"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/NodePool.hpp"                // for NodePool
#include "il/Symbol.hpp"                  // for Symbol
#include "il/symbol/ParameterSymbol.hpp"       // for ParameterSymbol
#include "il/symbol/AutomaticSymbol.hpp"  // for AutomaticSymbol
#include "il/symbol/StaticSymbol.hpp"     // for StaticSymbol
#include "il/symbol/LabelSymbol.hpp"      // for LabelSymbol
#include "infra/List.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/TransformUtil.hpp"
#include "ras/Delimiter.hpp"                   // for Delimiter
#include "ras/DebugCounter.hpp"
#include "env/CHTable.hpp"
#include "trj9/env/PersistentCHTable.hpp"

#define OPT_DETAILS "O^O CODE GENERATION: "



J9::CodeGenerator::CodeGenerator() :
      OMR::CodeGeneratorConnector(),
   _gpuSymbolMap(self()->comp()->allocator()),
   _stackLimitOffsetInMetaData(self()->comp()->fej9()->thisThreadGetStackLimitOffset())
   {
   }

TR_J9VMBase *
J9::CodeGenerator::fej9()
   {
   return (TR_J9VMBase *)self()->fe();
   }

// J9
static TR::Node *lowerCASValues(
      TR::Node *parent,
      int32_t childNum,
      TR::Node *address,
      TR::Compilation *comp,
      TR::Node *shftOffset,
      bool isLowMem,
      TR::Node *heapBase)
   {
   TR::Node *l2iNode = NULL;

   if ((address->getOpCodeValue() == TR::aconst) &&
       (address->getAddress() == 0))
      {
      l2iNode = TR::Node::create(address, TR::iconst, 0, 0);
      }
   else
      {
      // -J9JIT_COMPRESSED_POINTER-
      // if the value is known to be null or if using lowMemHeap, do not
      // generate a compression sequence
      //
      TR::Node *a2lNode = TR::Node::create(TR::a2l, 1, address);
      bool isNonNull = false;
      if (address->isNonNull())
         isNonNull = true;

      TR::Node *addNode = NULL;

      if (address->isNull() || isLowMem)
         {
         addNode = a2lNode;
         }
      else
         {
         if (isNonNull)
            a2lNode->setIsNonZero(true);
         addNode = TR::Node::create(TR::lsub, 2, a2lNode, heapBase);
         addNode->setContainsCompressionSequence(true);
         if (isNonNull)
            addNode->setIsNonZero(true);
         }

      if (shftOffset)
         {
         addNode = TR::Node::create(TR::lushr, 2, addNode, shftOffset);
         addNode->setContainsCompressionSequence(true);
         }

      if (isNonNull)
         addNode->setIsNonZero(true);

      l2iNode = TR::Node::create(TR::l2i, 1, addNode);
      if (isNonNull)
         l2iNode->setIsNonZero(true);

      if (address->isNull())
         l2iNode->setIsNull(true);
      }

   parent->setAndIncChild(childNum, l2iNode);
   address->recursivelyDecReferenceCount();
   return l2iNode;
   }


// J9
//
// convert dual operators from DAG representation to cyclic representation by cloning
// eg
//    luaddh
//      xh
//      yh
//      luadd
//        xl
//        yl
//        ==> luaddh             <=== replace dummy node with this third child to complete cycle
//
void
J9::CodeGenerator::lowerDualOperator(
      TR::Node *parent,
      int32_t childNumber,
      TR::TreeTop *treeTop)
   {
   if (parent == NULL)
      {
      // should never need to process treetops
      return;
      }

   // any parent may have an adjunct
   TR::Node *child = parent->getChild(childNumber);
   if (child->isAdjunct())
      {
      TR_ASSERT(!child->isDualCyclic(), "Visitcount problem: trying to clone node %p when it has already been cloned.\n", child);

      // create clone with space for third child, but still with two children
      TR::Node *clone = self()->createOrFindClonedNode(child, 3);
      if (1 && performTransformation(self()->comp(), "%sCreating Cyclic Dual Representation, replacing %p (%s) by %p under %p (childNumber %d).\n",
                                       OPT_DETAILS, child, child->getOpCode().getName(), clone, parent, childNumber))
         {
         child = clone;
         parent->setChild(childNumber, child);
         if (parent->isDualHigh() && (childNumber == 2))
            {
            // build cycle
            TR_ASSERT(!parent->isDualCyclic(), "Attempting to lower a dual operator node %p that has already been lowered.\n", parent);
            child->setNumChildren(3);
            child->setAndIncChild(2, parent);
            }
         }
      }
   }


// J9
void
J9::CodeGenerator::lowerCompressedRefs(
      TR::TreeTop *treeTop,
      TR::Node *node,
      vcount_t visitCount,
      TR_BitVector *childrenToBeLowered)
   {
   bool isLowMem = false;
   if (TR::Compiler->vm.heapBaseAddress() == 0)
      {
      isLowMem = true;
      }

   if (node->getOpCode().isCall() && childrenToBeLowered)
      {
      TR_BitVectorIterator bvi(*childrenToBeLowered);
      while (bvi.hasMoreElements())
         {
         int32_t nextChild = bvi.getNextElement();
         TR::Node *valueChild = node->getChild(nextChild);
         if (valueChild->getOpCode().is8Byte())
            {
            TR::Node *shftOffset = NULL;
            if (TR::Compiler->om.compressedReferenceShiftOffset() > 0)
               {
               shftOffset = TR::Node::create(node, TR::iconst, 0, TR::Compiler->om.compressedReferenceShiftOffset());
               }

            TR::Node *heapBase = TR::Node::create(node, TR::lconst, 0, 0);
            heapBase->setLongInt(TR::Compiler->vm.heapBaseAddress());
            lowerCASValues(node, nextChild, valueChild, self()->comp(), shftOffset, isLowMem, heapBase);
            }
         }

      return;
      }


   TR::Node *loadOrStoreNode = node->getFirstChild();

   /*
    decompression:
    actual = compress + heap_base

    and compression:
    compress = actual - heap_base

    iaload f      l2a
      aload O       ladd
                      lshl
                        i2l
                          iiload f
                            aload O
                        iconst shftKonst
                      lconst HB

    -or- if the field is known to be null
    l2a
      i2l
        iiload f
          aload O


    iastore f     iistore f
      aload O       aload O
      value         l2i
                      lshr
                        lsub
                          a2l
                            aload O
                          lconst HB
                        iconst shftKonst

    -or- if the field is known to be null
    iistore f
      aload O
      l2i
        a2l      <- nop on most platforms
          aload O

    - J9JIT_COMPRESS_POINTER 32-bit -

    DEPRECATED - do *not* use, kept here for historical reasons

    compress = actual - heapBase + shadowBase = actual + disp
    actual = compress - disp

    iaload f     i2a
       aload O      isub
                      iiload f
                       aload O
                      iconst HB

    iastore f    iistore f
       aload O      aload O
                   iushr            // iushr only there to distinguish between
                     iadd           // real iistores with iadds as the value
                       a2i
                         value
                       iconst HB
                     iconst 0

    */

   // dont process loads/stores twice
   // cannot use visitCounts because compressedRefs
   // trees may appear after checks (in which case the node
   // would have already been visited, preventing lowering)
   //
   TR::ILOpCodes convertOp = TR::Compiler->target.is64Bit() ? TR::l2a : TR::i2a;
   if (loadOrStoreNode->getOpCodeValue() == convertOp)
      return;
   else if (loadOrStoreNode->getOpCode().isStoreIndirect())
      {
      convertOp = TR::Compiler->target.is64Bit() ? TR::l2i : TR::iushr;
      if (loadOrStoreNode->getSecondChild()->getOpCodeValue() == convertOp)
         return;
      }

   TR::Node *heapBase = node->getSecondChild();

   TR::SymbolReference *symRef = loadOrStoreNode->getSymbolReference();
   TR::ILOpCodes loadOrStoreOp;
   bool isLoad = true;

   TR::Node *address = NULL;

   bool shouldBeCompressed = false;
   if (loadOrStoreNode->getOpCode().isLoadIndirect() ||
         loadOrStoreNode->getOpCode().isStoreIndirect() ||
            loadOrStoreNode->getOpCodeValue() == TR::arrayset)
      {
      shouldBeCompressed = TR::TransformUtil::fieldShouldBeCompressed(loadOrStoreNode, self()->comp());
      if (!shouldBeCompressed)
         {
         // catch cases when a compressedRefs anchor is created for specific
         // unsafe loads by inliner
         //
         if (loadOrStoreNode->getSymbol()->isUnsafeShadowSymbol())
            shouldBeCompressed = true;
         }
      // Don't de-compress loads created by dynamicLitPool
      if (loadOrStoreNode->getOpCode().isLoadIndirect() &&
            loadOrStoreNode->getSymbolReference()->isFromLiteralPool())
         shouldBeCompressed = false;
      }

   if (loadOrStoreNode->getOpCode().isLoadIndirect() && shouldBeCompressed)
      {
      if (TR::Compiler->om.shouldGenerateReadBarriersForFieldLoads())
         {
         dumpOptDetails(self()->comp(), "compression sequence %p is not required for loads under concurrent scavenge\n", node);
         return;
         }

      // base object
      address = loadOrStoreNode->getFirstChild();
      loadOrStoreOp = self()->comp()->il.opCodeForIndirectLoad(TR::Int32);
      }
   else if ((loadOrStoreNode->getOpCode().isStoreIndirect() || 
              loadOrStoreNode->getOpCodeValue() == TR::arrayset) && 
                 shouldBeCompressed)
      {
      // store value
      address = loadOrStoreNode->getSecondChild();

      loadOrStoreOp = self()->comp()->il.opCodeForIndirectStore(TR::Int32);
      isLoad = false;
      }
   else
      {
      dumpOptDetails(self()->comp(), "compression sequence %p is not in required form\n", node);
      return;
      }

   // in future if shifted offsets are used, this value will be
   // a positive non-zero constant
   //
   TR::Node *shftOffset = NULL;
   if (TR::Compiler->om.compressedReferenceShiftOffset() > 0)
      shftOffset = TR::Node::create(loadOrStoreNode, TR::iconst, 0, TR::Compiler->om.compressedReferenceShiftOffset());

   static char *pEnv = feGetEnv("TR_disableLowHeapMem");
   if (pEnv)
      isLowMem = false;

   if (isLoad)
      {
      TR::Node *newLoad = TR::Node::createWithSymRef(loadOrStoreOp, 1, 1, address, symRef);
      newLoad->setByteCodeInfo(loadOrStoreNode->getByteCodeInfo());

      if (loadOrStoreNode->isNonNull())
         newLoad->setIsNonZero(true);

      // FIXME: this breaks commoning of address (which could be a regLoad)
      // it would be nice to get the node flags on the original
      //TR::Node *newLoad = loadOrStoreNode->duplicateTree();
      //TR::Node::recreate(newLoad, loadOrStoreOp);

      // -J9JIT_COMPRESSED_POINTER-
      //
      TR::Node *iu2lNode = TR::Node::create(TR::iu2l, 1, newLoad);

      if (!isLowMem && (loadOrStoreNode->getVisitCount() != visitCount))
         {
         TR::Node *ttNode = TR::Node::create(TR::treetop, 1, iu2lNode);
         //traceMsg(comp(), "3creating treetop %p\n", ttNode);
         TR::TreeTop *tt = TR::TreeTop::create(self()->comp(), ttNode);
         TR::TreeTop *prevTT = treeTop->getPrevTreeTop();
         prevTT->join(tt);
         tt->join(treeTop);
         }

      TR::Node *addNode = iu2lNode;
      if (loadOrStoreNode->isNonNull())
         addNode->setIsNonZero(true);

      // if the load is known to be null or if using lowMemHeap, do not
      // generate a compression sequence
      //
      if (loadOrStoreNode->isNull() || isLowMem)
         {
         addNode = iu2lNode;
         if (shftOffset)
            {
            addNode = TR::Node::create(TR::lshl, 2, iu2lNode, shftOffset);
            addNode->setContainsCompressionSequence(true);
            }
         }
      else
         {
         if (shftOffset)
            {
            addNode = TR::Node::create(TR::lshl, 2, iu2lNode, shftOffset);
            addNode->setContainsCompressionSequence(true);
            }
         if (loadOrStoreNode->isNonNull())
            addNode->setIsNonZero(true);
         addNode = TR::Node::create(TR::ladd, 2, addNode, heapBase);
         if (loadOrStoreNode->isNonNull())
            addNode->setIsNonZero(true);

         addNode->setContainsCompressionSequence(true);
         }

      TR::Node::recreate(loadOrStoreNode, TR::l2a);
      address->decReferenceCount();
      loadOrStoreNode->setAndIncChild(0, addNode);
      loadOrStoreNode->setNumChildren(1);
      }
   else
      {
      TR::Node *l2iNode = NULL;
      if ((address->getOpCodeValue() == TR::aconst) && (address->getAddress() == 0))
         {
         l2iNode = TR::Node::create(address, TR::iconst, 0, 0);
         }
      else
         {
         // -J9JIT_COMPRESSED_POINTER-
         // if the value is known to be null or if using lowMemHeap, do not
         // generate a compression sequence
         //
         TR::Node *a2lNode = TR::Node::create(TR::a2l, 1, address);
         bool isNonNull = false;
         if (address->isNonNull())
            isNonNull = true;

         TR::Node *addNode = NULL;

         if (address->isNull() || isLowMem)
            {
            addNode = a2lNode;
            }
         else
            {
            if (isNonNull)
               a2lNode->setIsNonZero(true);
            addNode = TR::Node::create(TR::lsub, 2, a2lNode, heapBase);
            addNode->setContainsCompressionSequence(true);
            if (isNonNull)
               addNode->setIsNonZero(true);
            }

            if (shftOffset)
               {
               addNode = TR::Node::create(TR::lushr, 2, addNode, shftOffset);
               addNode->setContainsCompressionSequence(true);
               }

            if (isNonNull)
               addNode->setIsNonZero(true);

         l2iNode = TR::Node::create(TR::l2i, 1, addNode);
         if (isNonNull)
            l2iNode->setIsNonZero(true);

         if (address->isNull())
            l2iNode->setIsNull(true);
         }
      
      // recreating an arrayset node will replace the TR::arrayset with an istorei, which is undesired
      // as arrayset nodes can set indirect references
      if (!loadOrStoreNode->getOpCode().isWrtBar() && loadOrStoreNode->getOpCodeValue() != TR::arrayset)
         {
         TR::Node::recreate(loadOrStoreNode, loadOrStoreOp);
         }

      loadOrStoreNode->setAndIncChild(1, l2iNode);
      address->recursivelyDecReferenceCount();
      }
   }


// J9
//
static bool scanForNativeMethodsUntilMonitorNode(TR::TreeTop *firstTree, TR::Compilation *comp)
   {
   TR::TreeTop *currTree = firstTree;
   while (currTree)
      {
      TR::Node *currNode = currTree->getNode();
      //traceMsg(comp(), "-> Looking at node %p\n", currNode);

      if ((currNode->getOpCodeValue() == TR::monexit) ||
          (currNode->getOpCodeValue() == TR::monent))
         {
         return false;
         }
      else if (currNode->getNumChildren() > 0 &&
               currNode->getFirstChild()->getNumChildren() > 0 &&
               ((currNode->getFirstChild()->getOpCodeValue() == TR::monexit) ||
                (currNode->getFirstChild()->getOpCodeValue() == TR::monent))
               )
         {
         return false;
         }


      TR::Node *callTestNode = NULL;

      if (currNode->getOpCode().isCall() &&
         !currNode->getSymbolReference()->isUnresolved() &&
          currNode->getSymbol()->castToMethodSymbol()->isNative())
         {
         callTestNode = currNode;
         }
      else if (currNode->getNumChildren() > 0 &&
               currNode->getFirstChild()->getOpCode().isCall() &&
              !currNode->getFirstChild()->getSymbolReference()->isUnresolved() &&
               currNode->getFirstChild()->getSymbol()->castToMethodSymbol()->isNative())
         {
         callTestNode = currNode->getFirstChild();
         }

      if (callTestNode)
         {
         TR::ResolvedMethodSymbol *symbol = callTestNode->getSymbol()->castToResolvedMethodSymbol();
         if (strstr(symbol->signature(comp->trMemory()), "java/lang/Object.notify") ||
             strstr(symbol->signature(comp->trMemory()), "java/lang/Object.wait"))
            return true;
         }

      currTree = currTree->getNextTreeTop();
      }

   return false;
   }


void
J9::CodeGenerator::preLowerTrees()
   {

   OMR::CodeGeneratorConnector::preLowerTrees();

/*
 * These initializations should move from OMR to J9

   int32_t symRefCount = comp()->getSymRefCount();
   _localsThatAreStored = new (comp()->trHeapMemory()) TR_BitVector(symRefCount, comp()->trMemory(), heapAlloc);
   _numLocalsWhenStoreAnalysisWasDone = symRefCount;
*/

   // For dual operator lowering
   _uncommmonedNodes.reset();
   _uncommmonedNodes.init(64, true);
   }


void
J9::CodeGenerator::lowerTreesPreTreeTopVisit(TR::TreeTop *tt, vcount_t visitCount)
   {
   OMR::CodeGeneratorConnector::lowerTreesPreTreeTopVisit(tt, visitCount);

   TR::Node *node = tt->getNode();

   if (self()->getSupportsBDLLHardwareOverflowCheck() && node->getNumChildren() > 0 &&
       node->getFirstChild() && node->getFirstChild()->getOpCodeValue() == TR::icall &&
       node->getFirstChild()->getSymbol() &&
       (node->getFirstChild()->getSymbol()->castToMethodSymbol()->getRecognizedMethod() == TR::java_math_BigDecimal_noLLOverflowAdd))
      {
      node->getFirstChild()->getChild(2)->setNodeRequiresConditionCodes(true);
      }

   }


void
J9::CodeGenerator::lowerTreesPreChildrenVisit(TR::Node *parent, TR::TreeTop *treeTop, vcount_t visitCount)
   {
   OMR::CodeGeneratorConnector::lowerTreesPreChildrenVisit(parent, treeTop, visitCount);

   /*
      rip out a SpineCHK under two conditions.
      1. If the first child has been the first child of another SpineCHK
      2. If the first child is not an array access. This can happens when
      an array access node is changed by common sub expression
   */

   bool doIt = false;

   if ( (parent->getOpCodeValue() == TR::BNDCHKwithSpineCHK) ||  (parent->getOpCodeValue() == TR::SpineCHK) )
      {
      TR::Node *firstChild = parent->getFirstChild();
      TR::ILOpCode opcode = firstChild->getOpCode();

      if( ( (opcode.isLoad() || opcode.isStore() ) && opcode.hasSymbolReference() && firstChild->getSymbolReference() != NULL &&
         firstChild->getSymbolReference()->getSymbol()->isArrayShadowSymbol()) || opcode.isArrayRef())
         {
         //first child of SpineCHK is an array load or store, check if this is the first time we evaluate this node
         bool found = (std::find(_nodesSpineCheckedList.begin(), _nodesSpineCheckedList.end(), firstChild) != _nodesSpineCheckedList.end());
         if ( (firstChild->getVisitCount() == visitCount) && found )
            {
            // we have check this array access before, rip out SpineCHK
            doIt = true;
            }
         else
            {
            _nodesSpineCheckedList.push_front(firstChild);
            }
         }
      else
         {
         // the first child is not an array access, rip out SpineCHK
         doIt = true;
         }

      if( doIt )
         {
         int32_t i = 0;
         int32_t numChildren = parent->getNumChildren();
         TR::TreeTop *prevTreeTop = treeTop;
         TR::TreeTop *nextTreeTop = treeTop->getNextTreeTop();
         while (i < numChildren)
            {
            TR::Node *childToBeAnchored = parent->getChild(i);
            TR::TreeTop *anchorTree = TR::TreeTop::create(self()->comp(), TR::Node::create(TR::treetop, 1, childToBeAnchored), NULL, NULL);
            prevTreeTop->join(anchorTree);
            anchorTree->join(nextTreeTop);
            prevTreeTop = anchorTree;
            i++;
            }
         TR::TransformUtil::removeTree(self()->comp(), treeTop);
         return;
         }
      }

   if (parent->getOpCode().isFunctionCall())
      {
      // J9
      //
      if (self()->comp()->useCompressedPointers())
         {
         TR::MethodSymbol *methodSymbol = parent->getSymbol()->castToMethodSymbol();
         // In Java9 Unsafe could be the jdk.internal JNI method or the sun.misc ordinary method wrapper,
         // while in Java8 it can only be the sun.misc package which will itself contain the JNI method.
         // Test for isNative to distinguish between them.
         if ((methodSymbol->getRecognizedMethod() == TR::sun_misc_Unsafe_compareAndSwapObject_jlObjectJjlObjectjlObject_Z) &&
               methodSymbol->isNative() &&
               (!TR::Compiler->om.canGenerateArraylets() || parent->isUnsafeGetPutCASCallOnNonArray()) && parent->isSafeForCGToFastPathUnsafeCall())
            {
            TR_BitVector childrenToBeLowered(parent->getNumChildren(), self()->comp()->trMemory(), stackAlloc);
            childrenToBeLowered.set(3);
            childrenToBeLowered.set(4);
            self()->lowerCompressedRefs(treeTop, parent, visitCount, &childrenToBeLowered);
            }
         else if ((methodSymbol->getRecognizedMethod() == TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordCAS) ||
               (methodSymbol->getRecognizedMethod() == TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordCAS))
            {
            TR_BitVector childrenToBeLowered(parent->getNumChildren(), self()->comp()->trMemory(), stackAlloc);
            childrenToBeLowered.set(1);
            childrenToBeLowered.set(2);
            self()->lowerCompressedRefs(treeTop, parent, visitCount, &childrenToBeLowered);
            }
         else if ((methodSymbol->getRecognizedMethod() == TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordSet) ||
               (methodSymbol->getRecognizedMethod() == TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordSet))
            {
            TR_BitVector childrenToBeLowered(parent->getNumChildren(), self()->comp()->trMemory(), stackAlloc);
            childrenToBeLowered.set(1);
            self()->lowerCompressedRefs(treeTop, parent, visitCount, &childrenToBeLowered);
            }
         }
      }

   // J9
   //
   if (parent->getOpCode().hasSymbolReference() &&
       (parent->getSymbolReference() == self()->comp()->getSymRefTab()->findThisRangeExtensionSymRef()))
      TR::Node::recreate(parent, TR::treetop);


   // J9
   //
   if (parent->getOpCode().isCall() &&
       !parent->getSymbolReference()->isUnresolved() &&
       parent->getSymbolReference()->getSymbol()->getMethodSymbol() &&
       !parent->getSymbolReference()->getSymbol()->castToMethodSymbol()->isHelper() &&
       !parent->getSymbolReference()->getSymbol()->castToMethodSymbol()->isSystemLinkageDispatch() &&
       parent->getSymbolReference()->getSymbol()->getResolvedMethodSymbol())
      {
      //this code should match the one in genInvoke (Walker.cpp)
      if (parent->getSymbolReference()->getSymbol()->castToResolvedMethodSymbol()->getRecognizedMethod() == TR::com_ibm_jit_JITHelpers_getAddressAsPrimitive32 ||
            parent->getSymbolReference()->getSymbol()->castToResolvedMethodSymbol()->getRecognizedMethod() == TR::com_ibm_jit_JITHelpers_getAddressAsPrimitive64
         )
         parent->removeChild(0);

      if (parent->getSymbolReference()->getSymbol()->castToResolvedMethodSymbol()->getRecognizedMethod() == TR::com_ibm_jit_JITHelpers_getAddressAsPrimitive32)
         TR::Node::recreate(parent, TR::a2i);
      else if (parent->getSymbolReference()->getSymbol()->castToResolvedMethodSymbol()->getRecognizedMethod() == TR::com_ibm_jit_JITHelpers_getAddressAsPrimitive64)
         TR::Node::recreate(parent, TR::a2l);
      }

   // J9
   //
   TR::ILOpCodes parentOpCodeValue = parent->getOpCodeValue();
   if (self()->comp()->useCompressedPointers())
      {
      if (parentOpCodeValue == TR::compressedRefs)
         self()->lowerCompressedRefs(treeTop, parent, visitCount, NULL);
      }

   // J9
   //
   // Prepare to lower dual operators
   //
   for (int32_t childCount = 0; childCount < parent->getNumChildren(); childCount++)
      {
      self()->lowerDualOperator(parent, childCount, treeTop);
      }

   }


void
J9::CodeGenerator::lowerTreeIfNeeded(
      TR::Node *node,
      int32_t childNumberOfNode,
      TR::Node *parent,
      TR::TreeTop *tt)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(self()->comp()->fe());
   OMR::CodeGeneratorConnector::lowerTreeIfNeeded(node, childNumberOfNode, parent, tt);

   // J9
   //
   // if we found this iterator method inlined in a scorching method
   // we should attempt to prefetch where it's used for performance
   // structure is needed to determine the loop size to use proper prefetch stride
   if (!self()->shouldBuildStructure() &&
      (self()->comp()->getMethodHotness() >= scorching) &&
      !tt->getEnclosingBlock()->isCold() &&
      strstr(fej9->sampleSignature(node->getOwningMethod(), 0, 0, self()->trMemory()),"java/util/TreeMap$UnboundedValueIterator.next()"))
      {
      self()->setShouldBuildStructure();
      }

   // J9
   if (node->getOpCode().isCall() &&
       node->isUnsafePutOrderedCall() &&
       node->isDontInlinePutOrderedCall())
      {
      // Remove this treetop
      tt->getPrevTreeTop()->setNextTreeTop(tt->getNextTreeTop());
      tt->getNextTreeTop()->setPrevTreeTop(tt->getPrevTreeTop());
      tt->getNode()->recursivelyDecReferenceCount();
      return;
      }

   // J9
   if (!self()->comp()->getOption(TR_DisableUnsafe) &&
       node->getOpCode().isCall() &&
       node->getOpCodeValue() == TR::call &&
       !TR::Compiler->om.canGenerateArraylets() &&
       ((node->getSymbol()->castToMethodSymbol()->getRecognizedMethod() == TR::java_nio_Bits_copyToByteArray) ||
        (node->getSymbol()->castToMethodSymbol()->getRecognizedMethod() == TR::java_nio_Bits_copyFromByteArray)) &&
       !fej9->isAnyMethodTracingEnabled(node->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod()->getPersistentIdentifier()) &&
       performTransformation(self()->comp(), "%s Change recognized nio call to arraycopy [%p] \n", OPT_DETAILS, node))
      {
      bool from = false;
      if (node->getSymbol()->castToMethodSymbol()->getRecognizedMethod() == TR::java_nio_Bits_copyFromByteArray)
         from = true;

      TR::Node *nativeSrc;
      TR::Node *javaTarget;
      TR::Node *nativeOffset;

      if (from)
         {
         nativeSrc = node->getChild(2);
         javaTarget = node->getFirstChild();
         nativeOffset = node->getSecondChild();
         }
      else
         {
         nativeSrc = node->getFirstChild();
         javaTarget = node->getSecondChild();
         nativeOffset = node->getChild(2);
         }

      TR::Node *javaOffset;
      if (TR::Compiler->target.is64Bit())
         javaOffset = TR::Node::lconst(node, (int64_t) TR::Compiler->om.contiguousArrayHeaderSizeInBytes());
      else
         javaOffset = TR::Node::iconst(node, TR::Compiler->om.contiguousArrayHeaderSizeInBytes());

      TR::Node *len = node->getChild(3);

      TR::Node *oldNativeSrc = nativeSrc;
      TR::Node *oldNativeOffset = nativeOffset;
      TR::Node *oldLen = len;

      if (TR::Compiler->target.is32Bit())
         {
         nativeSrc = TR::Node::create(TR::l2i, 1, nativeSrc);
         nativeOffset = TR::Node::create(TR::l2i, 1, nativeOffset);
         len = TR::Node::create(TR::l2i, 1, len);
         }

      TR::Node::recreate(node, TR::arraycopy);

      TR::Node *nativeAddr;
      TR::Node *javaAddr;

      if (TR::Compiler->target.is32Bit())
         {
         nativeAddr = nativeSrc;
         javaOffset = TR::Node::create(TR::iadd, 2, javaOffset, nativeOffset);
         javaAddr = TR::Node::create(TR::aiadd, 2, javaTarget, javaOffset);
         }
      else
         {
         nativeAddr = nativeSrc;
         javaOffset = TR::Node::create(TR::ladd, 2, javaOffset, nativeOffset);
         javaAddr = TR::Node::create(TR::aladd, 2, javaTarget, javaOffset);
         }

      node->setNumChildren(3);

      if (from)
         {
         node->setAndIncChild(0, javaAddr);
         node->setAndIncChild(1, nativeAddr);
         node->setAndIncChild(2, len);
         }
      else
         {
         node->setAndIncChild(0, nativeAddr);
         node->setAndIncChild(1, javaAddr);
         node->setAndIncChild(2, len);
         }

      javaTarget->recursivelyDecReferenceCount();
      oldNativeSrc->recursivelyDecReferenceCount();
      oldNativeOffset->recursivelyDecReferenceCount();
      oldLen->recursivelyDecReferenceCount();

      node->setArrayCopyElementType(TR::Int8);
      node->setForwardArrayCopy(true);
      }

   // J9
   if (self()->comp()->compileRelocatableCode() && (node->getOpCodeValue() == TR::loadaddr) && parent && ((parent->getOpCodeValue() == TR::instanceof) || (parent->getOpCodeValue() == TR::checkcast)))
      {
      TR::TreeTop::create(self()->comp(), tt->getPrevTreeTop(), TR::Node::create(TR::treetop, 1, node));
      }

   // J9
   if (node->getOpCode().hasSymbolReference() &&
       (node->getOpCode().isLoad() ||
        node->getOpCode().isStore()))
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      TR::Symbol *symbol = symRef->getSymbol();
      if (symbol->isVolatile() && node->getDataType() == TR::Int64 && !symRef->isUnresolved() && TR::Compiler->target.is32Bit() &&
          !self()->getSupportsInlinedAtomicLongVolatiles())
         {
         bool isLoad = false;
         TR::SymbolReference * volatileLongSymRef = NULL;
         if (node->getOpCode().isLoadVar())
            {
            volatileLongSymRef = self()->comp()->getSymRefTab()->findOrCreateRuntimeHelper(TR_volatileReadLong, false, false, true);
            isLoad = true;
            }
         else
            volatileLongSymRef = self()->comp()->getSymRefTab()->findOrCreateRuntimeHelper(TR_volatileWriteLong, false, false, true);

         node->setSymbolReference(volatileLongSymRef);

         TR::Node * address = NULL;
         if (node->getOpCode().isIndirect())
            address = node->getFirstChild();

         TR::Node * addrNode = NULL;
         if (address)
            {
            if (symRef->getOffset() == 0)
               addrNode = address;
            else
               {
               addrNode = TR::Node::create(TR::aiadd, 2, address,
                                TR::Node::create(node, TR::iconst, 0, symRef->getOffset()));
               addrNode->setIsInternalPointer(true);
               }
            }

         if (isLoad)
            {
            if (node->getOpCode().isIndirect())
               {
               if (tt->getNode()->getOpCodeValue() == TR::NULLCHK)
                  {
                  TR::Node * nullchkNode =
                      TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, TR::Node::create(TR::PassThrough, 1, address), tt->getNode()->getSymbolReference());
                  tt->getPrevTreeTop()->insertAfter(TR::TreeTop::create(self()->comp(), nullchkNode));
                  TR::Node::recreate(tt->getNode(), TR::treetop);
                  }
               node->setNumChildren(1);
               node->setAndIncChild(0, addrNode);
               }
            else
               {
               TR::Node * statics = TR::Node::createWithSymRef(node, TR::loadaddr, 0, symRef);
               node->setNumChildren(1);
               node->setAndIncChild(0, statics);
               }

            if ((tt->getNode()->getOpCodeValue() != TR::treetop) ||
                (tt->getNode()->getFirstChild() != node))
               {
               TR::Node * ttNode = TR::Node::create(TR::treetop, 1, node);
               tt->getPrevTreeTop()->insertAfter(TR::TreeTop::create(self()->comp(), ttNode));
               }
            }
         else
            {
            if (node->getOpCode().isIndirect())
               {
               if (tt->getNode()->getOpCodeValue() == TR::NULLCHK)
                  {
                  TR::Node * nullchkNode =
                      TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, TR::Node::create(TR::PassThrough, 1, address), tt->getNode()->getSymbolReference());
                  tt->getPrevTreeTop()->insertAfter(TR::TreeTop::create(self()->comp(), nullchkNode));
                  TR::Node::recreate(tt->getNode(), TR::treetop);
                  }

               node->setNumChildren(2);
               node->setChild(0, node->getSecondChild());
               node->setAndIncChild(1, addrNode);
               }
            else
               {
               TR::Node * statics = TR::Node::createWithSymRef(node, TR::loadaddr, 0, symRef);
                node->setNumChildren(2);
               node->setAndIncChild(1, statics);
               }

            TR::Node * ttNode = tt->getNode();
            if (ttNode == node)
               {
               TR::Node * newTTNode = TR::Node::create(TR::treetop, 1, ttNode);
               tt->setNode(newTTNode);
               }
            }

         if (isLoad)
            TR::Node::recreate(node, TR::lcall);
         else
            TR::Node::recreate(node, TR::call);

         if (address)
            address->recursivelyDecReferenceCount();
         }
      }

   // J9 (currentTimeMillis & OSR)
   if (node->getOpCode().isStore())
      {
      if ((node->getType().isInt64() &&
           node->isNOPLongStore()) ||
          (node->getSymbol()->isAutoOrParm() &&
          node->storedValueIsIrrelevant()))
         {
         TR_ASSERT(node == tt->getNode(), "A store is expected to be the root of its treetop");

         // Remove this treetop
         tt->getPrevTreeTop()->setNextTreeTop(tt->getNextTreeTop());
         tt->getNextTreeTop()->setPrevTreeTop(tt->getPrevTreeTop());
         node->recursivelyDecReferenceCount();
         }
      else
         {
         // Needed for OSR
         //
         _localsThatAreStored->set(node->getSymbolReference()->getReferenceNumber());
         }
      }
   // J9
   else if (node->getOpCodeValue() == TR::monent ||
            node->getOpCodeValue() == TR::monexit ||
            node->getOpCodeValue() == TR::tstart )
      {
      TR_OpaqueClassBlock * monClass = node->getMonitorClass(self()->comp()->getCurrentMethod());
      if (monClass)
         self()->addMonClass(node, monClass);

      // Clear the hidden second child that may be used by code generation
      //
      node->setMonitorInfo(0);
      }


   // J9
   if (self()->comp()->getOption(TR_ReservingLocks) &&
       node->getOpCodeValue() == TR::monent)
      {
      TR_OpaqueMethodBlock *owningMethod = node->getOwningMethod();
      TR_OpaqueClassBlock  *classPointer = fej9->getClassOfMethod(owningMethod);
      TR_PersistentClassInfo * persistentClassInfo =
         self()->comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(classPointer, self()->comp());

      if (persistentClassInfo && persistentClassInfo->isReservable())
         {
         bool allowedToReserve = !scanForNativeMethodsUntilMonitorNode(tt->getNextTreeTop(), self()->comp());
         if (!allowedToReserve)
            {
            persistentClassInfo->setReservable(false);
            }
         }
      }

   // J9
   if (  self()->comp()->getOptions()->enableDebugCounters()
      && node->getOpCode().isCall()
      && node->getSymbol()->getMethodSymbol() // compjazz 45988: zEmulator arrayset currently isCall and uses a generic int shadow.  Can't assume it's a method.
      && !node->getSymbol()->castToMethodSymbol()->isHelper())
      {
      bool insertByteCode = TR::Options::_debugCounterInsertByteCode;
      bool insertJittedBody = TR::Options::_debugCounterInsertJittedBody;
      bool insertMethod = TR::Options::_debugCounterInsertMethod;

      const char *caller = self()->comp()->signature();
      const char *callee = node->getSymbol()->castToMethodSymbol()->getMethod()->signature(self()->trMemory(), stackAlloc);
      TR_ByteCodeInfo &bcInfo = node->getByteCodeInfo();
      if (insertByteCode)
         {
         TR::DebugCounter::prependDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(),
               "compilationReport.instructions:byByteCode.numInvocations.(%s)=%d", caller, node->getByteCodeInfo().getByteCodeIndex()), tt);
         }
      if (insertJittedBody)
         {
         TR::DebugCounter::prependDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(),
               "compilationReport.instructions:byJittedBody.numInvocations.(%s).%s", caller, self()->comp()->getHotnessName()), tt);
         }
      if (insertMethod)
         {
         TR::DebugCounter::prependDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(),
               "compilationReport.instructions:byMethod.numInvocations.(%s)", caller), tt);
         }
      TR::DebugCounter::prependDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(), "callers/(%s)/%d=%d", caller, bcInfo.getCallerIndex(), bcInfo.getByteCodeIndex()), tt);
      TR::DebugCounter::prependDebugCounter(self()->comp(), TR::DebugCounter::debugCounterName(self()->comp(), "callees/(%s)/(%s)/%d=%d", callee, caller, bcInfo.getCallerIndex(), bcInfo.getByteCodeIndex()), tt);
      }

   // J9
   //
   // We uncommon all the address children of a direct to JNI call
   // because if the loadaddr is commoned across a GC point, we cannot mark the register
   // as collected (since it is not a Java object) and we can have a problem if we do not
   // mark in some other way if the stack grows (and is moved) at the GC point. The problem
   // will be that the register will continue to point at the old stack.
   //
   if (node->getOpCode().isCall() &&
       node->getSymbol()->getMethodSymbol() &&
       node->isPreparedForDirectJNI())
      {
      int32_t i;
      for (i = 0; i < node->getNumChildren(); ++i)
         {
         TR::Node * n = node->getChild(i);
         if (n->getDataType() == TR::Address)
            {
            //TR_ASSERT((n->getOpCodeValue() == TR::loadaddr), "Address child of JNI call is not a loadaddr\n");
            if ((n->getOpCodeValue() == TR::loadaddr) &&
                (n->getReferenceCount() > 1) &&
                n->getSymbol()->isAutoOrParm())
               {
               //printf("Uncommoned address child of JNI call in %s\n", comp()->signature());
               TR::Node *dupChild = n->duplicateTree();
               node->setAndIncChild(i, dupChild);
               n->recursivelyDecReferenceCount();
               }
            }
         }
      }

   // J9
   // code to push recompilation of methods whose caller is scorching
   if (self()->comp()->getOption(TR_EnableRecompilationPushing)                 &&
       !self()->getCurrentBlock()->isCold()                                   &&
       self()->comp()->allowRecompilation()                                     &&
       self()->comp()->getMethodHotness()>=veryHot                              &&
       node->getOpCode().isCall()                                       &&
       self()->comp()->getPersistentInfo()->getNumLoadedClasses() < TR::Options::_bigAppThreshold &&
       node->getSymbol()->getMethodSymbol()                             &&
      !node->isPreparedForDirectJNI())
      {
      bool pushCall = true;
      TR::MethodSymbol *methodSymbol = node->getSymbol()->getMethodSymbol();
      TR::SymbolReference  *methodSymRef  = node->getSymbolReference();

      if (methodSymRef->isUnresolved())
         {
         pushCall = false;
         }
      else if (!node->getOpCode().isCallDirect() || methodSymbol->isVirtual())
         {
         TR_ResolvedMethod *resolvedMethod = node->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod();
         if (!resolvedMethod || node->isTheVirtualCallNodeForAGuardedInlinedCall() ||
             resolvedMethod->virtualMethodIsOverridden()        ||
             resolvedMethod->isAbstract() ||
             (resolvedMethod == self()->comp()->getCurrentMethod()))
            {
            pushCall = false;
            }
         }

      if (pushCall)
         {
         if (!((methodSymbol && methodSymbol->getResolvedMethodSymbol() &&
               methodSymbol->getResolvedMethodSymbol()->getResolvedMethod() &&
               methodSymbol->getResolvedMethodSymbol()->getResolvedMethod()->isInterpreted()) ||
               methodSymbol->isVMInternalNative()      ||
               methodSymbol->isHelper()                ||
               methodSymbol->isNative()                ||
               methodSymbol->isSystemLinkageDispatch() ||
               methodSymbol->isJITInternalNative())    &&
               methodSymbol->getResolvedMethodSymbol() &&
               methodSymbol->getResolvedMethodSymbol()->getResolvedMethod())
            {
            void * methodAddress = methodSymbol->getResolvedMethodSymbol()->getResolvedMethod()->startAddressForInterpreterOfJittedMethod();
            TR_PersistentJittedBodyInfo * bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(methodAddress);
            //printf("Pushing node %p\n", node);
            //fflush(stdout);
            if (bodyInfo &&
                bodyInfo->getHotness() <= warm && !bodyInfo->getIsProfilingBody())
               {
               TR_ResolvedMethod *method = methodSymbol->castToResolvedMethodSymbol()->getResolvedMethod();
               int isInLoop = -1;
               TR::Block * block = self()->getCurrentBlock();

               TR_BlockStructure * blockStructure = block->getStructureOf();
               if (blockStructure)
                  {
                  TR_Structure *parentStructure = blockStructure->getParent();
                  while (parentStructure)
                     {
                     TR_RegionStructure *region = parentStructure->asRegion();
                     if (region->isNaturalLoop() ||
                         region->containsInternalCycles())
                        {
                        isInLoop =  region->getNumber();
                        break;
                        }
                     parentStructure = parentStructure->getParent();
                     }
                  }

               //printf ("Scorching method %s is calling warm (or called method) %s, in block_%d with frequency %d, is in loop %d\n", comp()->getMethodSymbol()->signature(), fej9->sampleSignature(method->getPersistentIdentifier(), 0, 0, trMemory()), getCurrentBlock()->getNumber(), getCurrentBlock()->getFrequency(), isInLoop);
               if ((self()->getCurrentBlock()->getFrequency() > MAX_COLD_BLOCK_COUNT) || (isInLoop && self()->getCurrentBlock()->getFrequency()==0))
                  {
                  bodyInfo->setCounter(1);
                  bodyInfo->setIsPushedForRecompilation();
                  }
               }
            }
         }
      }

   // J9
   if (node->getOpCodeValue() == TR::instanceof)
      {
      TR::Node * topNode = tt->getNode();
      if (topNode->getNumChildren() > 0)
         topNode = topNode->getFirstChild();

      if (topNode->getOpCode().isCall() &&
          topNode->getSymbol()->getMethodSymbol() &&
          (topNode->getSymbol()->getMethodSymbol()->isSystemLinkageDispatch() ||
           topNode->isPreparedForDirectJNI()))
         {
         TR::Node * ttNode = TR::Node::create(TR::treetop, 1, node);
         tt->getPrevTreeTop()->insertAfter(TR::TreeTop::create(self()->comp(), ttNode));
         }
      }

   // J9
   if (node->getOpCodeValue() == TR::NULLCHK)
      {
      if ((node->getFirstChild()->getReferenceCount() == 1) &&
          node->getFirstChild()->getOpCode().isLoadVar() &&
          !node->getFirstChild()->getSymbolReference()->getSymbol()->isVolatile())
         {
         TR::Node::recreate(node->getFirstChild(), TR::PassThrough);
         }
      }

   // J9
   //
   //Anchoring node to either extract register pressure(performance)
   //or ensure instanceof doesn't have a parent node of CALL (correctness)
   //
   char *anchoringReason = "register hog";
   switch (node->getOpCodeValue())
      {
      // Extract heavy register pressure trees when dictated at the start of the walk
      // (typically IA32 on system linkage calls when there is one fewer register).
      case TR::ldiv:
      case TR::lrem:
      case TR::lcmp:
        {

         if (!self()->removeRegisterHogsInLowerTreesWalk())
            break;
        }
      // Instanceof might requires this transformation for the reason of either register pressure
      // or ensure no parent node of CALL
      case TR::instanceof:
        {
         //  For correctness, all we really need here is to ensure instanceof
         //  doesn't have a parent of native call. But even java call would
         //  create control flow to setup linkage, which might cause conflict
         //  with the instanceof control flow.
         if(parent->getOpCode().isCall())
            {
            anchoringReason = "call-like";
            }

         else if (!self()->removeRegisterHogsInLowerTreesWalk())
            break;

         TR::Node *ttNode = TR::Node::create(TR::treetop, 1, node);
         tt->getPrevTreeTop()->insertAfter(TR::TreeTop::create(self()->comp(), ttNode));
         if (self()->comp()->getOption(TR_TraceCG))
            traceMsg(self()->comp(), "Anchoring %s node %s [%p] under treetop [%p]\n", anchoringReason, node->getOpCode().getName(), node, ttNode);
         break;
         }
      default:
         break;

      }

   }


static bool isArraySizeSymbolRef(TR::SymbolReference *s, TR::SymbolReferenceTable *symRefTab)
   {
   // TODO: Move to compile/SymbolReferenceTable.hpp
   return (s!=NULL) && (s == symRefTab->findContiguousArraySizeSymbolRef() || s == symRefTab->findDiscontiguousArraySizeSymbolRef());
   }


void
J9::CodeGenerator::moveUpArrayLengthStores(TR::TreeTop *insertionPoint)
   {
   // 174954: Until TR::arraylength has a symref with proper aliasing, we have to
   // make sure that stores to the array length field occur before all arraylength
   // trees.  Good news is that all such stores are inserted by escape
   // analysis, so they always have a loadaddr as one child and a constant as
   // the other, so they can be trivially moved to the top of the block.
   //
   for (TR::TreeTop *tt = insertionPoint->getNextTreeTop(); tt; tt = tt->getNextTreeTop())
      {
      if (tt->getNode()->getOpCodeValue() == TR::BBStart && !tt->getNode()->getBlock()->isExtensionOfPreviousBlock())
         break;
      TR::Node *store = tt->getNode()->getStoreNode();
      if (store && store->getOpCode().isStoreIndirect() && isArraySizeSymbolRef(store->getSymbolReference(), self()->symRefTab()))
         {
         if (store->getFirstChild()->getOpCodeValue() != TR::loadaddr)
            {
            dumpOptDetails(self()->comp(), "MOVE UP ARRAY LENGTH STORES: WARNING! First child of %p is %s; expected loadaddr\n", store, store->getFirstChild()->getOpCode().getName());
            }
         else if (!store->getSecondChild()->getOpCode().isLoadConst())
            {
            dumpOptDetails(self()->comp(), "MOVE UP ARRAY LENGTH STORES: WARNING! Second child of %p is %s; expected const\n", store, store->getSecondChild()->getOpCode().getName());
            }
         else
            {
            dumpOptDetails(self()->comp(), "MOVE UP ARRAY LENGTH STORES: Moving %s %p up after %p\n", tt->getNode()->getOpCode().getName(), tt->getNode(), insertionPoint->getNode());
            tt->unlink(false);
            insertionPoint->insertAfter(tt);
            insertionPoint = tt;
            }
         }
      }
   }


void
J9::CodeGenerator::doInstructionSelection()
   {
   self()->setNextAvailableBlockIndex(self()->comp()->getFlowGraph()->getNextNodeNumber() + 1);

   TR_SetMonitorStateOnBlockEntry::LiveMonitorStacks liveMonitorStacks(
      (TR_SetMonitorStateOnBlockEntry::LiveMonitorStacksComparator()),
      TR_SetMonitorStateOnBlockEntry::LiveMonitorStacksAllocator(self()->comp()->trMemory()->heapMemoryRegion()));



   // Set default value for pre-prologue size
   //
   TR::ResolvedMethodSymbol * methodSymbol = self()->comp()->getJittedMethodSymbol();
   self()->setPrePrologueSize(4 + (methodSymbol->isJNI() ? 4 : 0));

   if (self()->comp()->getOption(TR_TraceCG))
      diagnostic("\n<selection>");

   if (self()->comp()->getOption(TR_TraceCG) || debug("traceGRA"))
      self()->comp()->getDebug()->setupToDumpTreesAndInstructions("Performing Instruction Selection");

   self()->beginInstructionSelection();

   {
   TR::StackMemoryRegion stackMemoryRegion(*self()->trMemory());

   TR_BitVector * liveLocals = self()->getLiveLocals();
   TR_BitVector nodeChecklistBeforeDump(self()->comp()->getNodeCount(), self()->trMemory(), stackAlloc, growable);

   /*
     To enable instruction scheduling (both in the compiler and in out-of-order hardware),
     we would prefer not to reuse the same memory to represent multiple temps inside
     a tight loop. At higher opt levels, therefore, we only free variable size symrefs at
     back edges. To prevent pathological cases from consuming too much stack space, we set a
     cap on the number of extended basic blocks before we stop waiting for a back edge and free
     our temps anyway.
    */
   const uint32_t MAX_EBBS_BEFORE_FREEING_VARIABLE_SIZED_SYMREFS = 10;
   uint32_t numEBBsSinceFreeingVariableSizeSymRefs = 0;

   TR_BitVector * liveMonitors = 0;
   TR_Stack<TR::SymbolReference *> * liveMonitorStack = 0;
   int32_t numMonitorLocals = 0;
   static bool traceLiveMonEnv = feGetEnv("TR_traceLiveMonitors") ? true : false;
   bool traceLiveMon = self()->comp()->getOption(TR_TraceLiveMonitorMetadata) || traceLiveMonEnv;

   _lmmdFailed = false;
   if (self()->comp()->getMethodSymbol()->mayContainMonitors())
      {
      if(traceLiveMon)
         traceMsg(self()->comp(),"In doInstructionSelection: Method may contain monitors\n");

      if (!liveLocals)
         self()->comp()->getMethodSymbol()->resetLiveLocalIndices();

      ListIterator<TR::AutomaticSymbol> locals(&self()->comp()->getMethodSymbol()->getAutomaticList());
      for (TR::AutomaticSymbol * a = locals.getFirst(); a; a = locals.getNext())
         if (a->holdsMonitoredObject())
            {
            if(traceLiveMon)
               traceMsg(self()->comp(),"\tSymbol %p contains monitored object\n",a);
            if (!liveLocals)
               {
               if(traceLiveMon)
                  traceMsg(self()->comp(),"\tsetting LiveLocalIndex to %d on symbol %p\n",numMonitorLocals+1,a);
               a->setLiveLocalIndex(numMonitorLocals++, self()->fe());
               }
            else if (a->getLiveLocalIndex() + 1 > numMonitorLocals)
               {
               if(traceLiveMon)
                  traceMsg(self()->comp(),"\tsetting numMonitorLocals to %d while considering symbol %p\n",a->getLiveLocalIndex()+1,a);
               numMonitorLocals = a->getLiveLocalIndex() + 1;
               }
            }

      if (numMonitorLocals)
         {
         TR_SetMonitorStateOnBlockEntry monitorState(self()->comp(), &liveMonitorStacks);

         if(traceLiveMon)
            traceMsg(self()->comp(),"\tCreated monitorState %p\n",&monitorState);

         monitorState.set(_lmmdFailed, traceLiveMon);
         if (traceLiveMon)
            traceMsg(self()->comp(), "found numMonitorLocals %d\n", numMonitorLocals);
         }
      else if(traceLiveMon)
         traceMsg(self()->comp(),"\tnumMonitorLocals = %d\n",numMonitorLocals);
      }

   TR::SymbolReference **liveLocalSyms = NULL;
   TR_BitVector *unsharedSymsBitVector = NULL;
   int32_t maxLiveLocalIndex = -1;
   TR::list<TR::Block*> newBlocks(getTypedAllocator<TR::Block*>(self()->comp()->allocator()));
   TR_ScratchList<TR::Node> fsdStores(self()->trMemory());
   if (self()->comp()->getOption(TR_MimicInterpreterFrameShape) || self()->comp()->getOption(TR_PoisonDeadSlots))
      {
      if (self()->comp()->areSlotsSharedByRefAndNonRef() || self()->comp()->getOption(TR_PoisonDeadSlots))
         {
         TR_ScratchList<TR::SymbolReference> participatingLocals(self()->trMemory());

         TR::SymbolReference *autoSymRef = NULL;
         int32_t symRefNumber;
         int32_t symRefCount = self()->comp()->getSymRefCount();
         TR::SymbolReferenceTable *symRefTab = self()->comp()->getSymRefTab();
         for (symRefNumber = symRefTab->getIndexOfFirstSymRef(); symRefNumber < symRefCount; symRefNumber++)
            {
            autoSymRef = symRefTab->getSymRef(symRefNumber);
            if (autoSymRef &&
                autoSymRef->getSymbol() &&
                autoSymRef->getSymbol()->isAuto() &&
                (autoSymRef->getSymbol()->castToAutoSymbol()->getLiveLocalIndex() != (uint16_t)-1))
                {
                TR::AutomaticSymbol * autoSym = autoSymRef->getSymbol()->castToAutoSymbol();
                if (methodSymbol->getAutomaticList().find(autoSym))
                   {
                   participatingLocals.add(autoSymRef);

                   if (autoSym->getLiveLocalIndex() > maxLiveLocalIndex)
                      maxLiveLocalIndex = autoSym->getLiveLocalIndex();
                   }
                }
             }

         liveLocalSyms = (TR::SymbolReference **)self()->trMemory()->allocateStackMemory((maxLiveLocalIndex+1)*sizeof(TR::SymbolReference *));
         memset(liveLocalSyms, 0, (maxLiveLocalIndex+1)*sizeof(TR::SymbolReference *));
         unsharedSymsBitVector = new (self()->trStackMemory()) TR_BitVector(maxLiveLocalIndex+1, self()->trMemory(), stackAlloc);

         ListIterator<TR::SymbolReference> participatingLocalsIt(&participatingLocals);
         for (autoSymRef = participatingLocalsIt.getFirst(); autoSymRef; autoSymRef = participatingLocalsIt.getNext())
            {
            TR::AutomaticSymbol * autoSym = autoSymRef->getSymbol()->castToAutoSymbol();
            liveLocalSyms[autoSym->getLiveLocalIndex()] = autoSymRef;
            if (!autoSym->isSlotSharedByRefAndNonRef())
               {
               //dumpOptDetails("Unshared symRef %d live local index %d\n", autoSymRef->getReferenceNumber(), autoSym->getLiveLocalIndex());
               unsharedSymsBitVector->set(autoSym->getLiveLocalIndex());
               }
            }
         }
      else
         liveLocals = NULL;
      }

   bool fixedUpBlock = false;

   for (TR::TreeTop *tt = self()->comp()->getStartTree(); tt; tt = self()->getCurrentEvaluationTreeTop()->getNextTreeTop())
      {
      if(traceLiveMon)
         traceMsg(self()->comp(),"\tWalking TreeTops at tt %p with node %p\n",tt,tt->getNode());

      TR::Instruction *prevInstr = self()->comp()->getAppendInstruction();
      TR::Node * node = tt->getNode();
      TR::ILOpCodes opCode = node->getOpCodeValue();

      TR::Node * firstChild = node->getNumChildren() > 0 ? node->getFirstChild() : 0;

      if (opCode == TR::BBStart)
         {
         fixedUpBlock = false;
         TR::Block *block = node->getBlock();
         self()->setCurrentEvaluationBlock(block);
         self()->setCurrentBlockIndex(block->getNumber());
         self()->resetMethodModifiedByRA();

         liveMonitorStack = (liveMonitorStacks.find(block->getNumber()) != liveMonitorStacks.end()) ?
            liveMonitorStacks[block->getNumber()] :
            NULL;

         if (!block->isExtensionOfPreviousBlock())
            {
            // If we are keeping track of live locals, set up the live locals for
            // this block
            //
            if (liveLocals)
               {
               if (block->getLiveLocals())
                  liveLocals = new (self()->trHeapMemory()) TR_BitVector(*block->getLiveLocals());
               else
                  {
                  liveLocals = new (self()->trHeapMemory()) TR_BitVector(*liveLocals);
                  liveLocals->empty();
                  }

               if (self()->comp()->areSlotsSharedByRefAndNonRef() && unsharedSymsBitVector)
                  {
                  *liveLocals |= *unsharedSymsBitVector;
                  }
               }


            if (liveMonitorStack)
               {
               liveMonitors = new (self()->trHeapMemory()) TR_BitVector(numMonitorLocals, self()->trMemory());
               if (traceLiveMon)
                  traceMsg(self()->comp(), "created liveMonitors bitvector at block_%d for stack  %p size %d\n",
                                                            block->getNumber(), liveMonitorStack,
                                                            liveMonitorStack->size());
               for (int32_t i = liveMonitorStack->size() - 1; i >= 0; --i)
                  {
                  if (traceLiveMon)
                     traceMsg(self()->comp(), "about to set liveMonitors for symbol %p\n",(*liveMonitorStack)[i]->getSymbol());
                  liveMonitors->set((*liveMonitorStack)[i]->getSymbol()->castToRegisterMappedSymbol()->getLiveLocalIndex());
                  if (traceLiveMon)
                     traceMsg(self()->comp(), "setting livemonitor %d at block_%d\n",
                                                            (*liveMonitorStack)[i]->getSymbol()->castToRegisterMappedSymbol()->getLiveLocalIndex(),
                                                            block->getNumber());
                  }
               }
           else
               {
               liveMonitors = 0;
               if (traceLiveMon)
                  traceMsg(self()->comp(), "no liveMonitorStack for block_%d\n", block->getNumber());
               }
            numEBBsSinceFreeingVariableSizeSymRefs++;
            }

         if (self()->getDebug())
            self()->getDebug()->roundAddressEnumerationCounters();

#if DEBUG
         // Verify that we are only being more conservative by inheriting the live
         // local information from the previous block.
         //
         else if (liveLocals && debug("checkBlockEntryLiveLocals"))
            {
            TR_BitVector *extendedBlockLocals = new (self()->trStackMemory()) TR_BitVector(*(block->getLiveLocals()));
            *extendedBlockLocals -= *(liveLocals);

            TR_ASSERT(extendedBlockLocals->isEmpty(),
                   "Live local information is *less* pessimistic!\n");
            }
#endif
         }
      else if (opCode == TR::BBEnd)
         {
         TR::Block *b = self()->getCurrentEvaluationBlock();
         if (b->isLastWarmBlock())
            {
            // Mark the split point between warm and cold instructions, so they
            // can be allocated in different code sections.
            //
            prevInstr->setLastWarmInstruction(true);
            }

         // checks for consistent monitorStack
         //
         for (auto e = b->getSuccessors().begin(); e != b->getSuccessors().end(); ++e)
            if ((*e)->getTo() == self()->comp()->getFlowGraph()->getEnd())
               {
               // block could end in a throw,
               //
               TR::TreeTop *lastRealTT = b->getLastRealTreeTop();
               // last warm blocks could end in a goto
               //
               if (lastRealTT->getNode()->getOpCode().isGoto())
                  lastRealTT = lastRealTT->getPrevTreeTop();

               TR::Node *n = lastRealTT->getNode();
               if (n->getOpCodeValue() == TR::treetop ||
                     n->getOpCode().isCheck())
                  n = n->getFirstChild();

               bool endsInThrow = false;
               if (n->getOpCode().isCall() &&
                     (n->getSymbolReference()->getReferenceNumber() == TR_aThrow))
                  endsInThrow = true;
               else if (n->getOpCodeValue() == TR::Return)
                  {
                  // a check that is going to fail
                  //
                  TR::TreeTop *prev = lastRealTT->getPrevTreeTop();
                  if ((prev->getNode()->getOpCodeValue() == TR::asynccheck) ||
                        (prev->getNode()->getOpCodeValue() == TR::treetop))
                     prev = prev->getPrevTreeTop();
                  if (prev->getNode()->getOpCode().isCheck())
                     endsInThrow = true;
                  }

               if (liveMonitorStack &&
                     liveMonitorStack->size() != 0 &&
                     !endsInThrow)
                  dumpOptDetails(self()->comp(), "liveMonitorStack must be empty, unbalanced monitors found! %d\n",
                                                                           liveMonitorStack->size());

               if (traceLiveMon)
                  {
                  traceMsg(self()->comp(), "liveMonitorStack %p at CFG end (syncMethod %d)", liveMonitorStack,
                                                         self()->comp()->getMethodSymbol()->isSynchronised());
                  if (liveMonitorStack)
                     traceMsg(self()->comp(), "   size %d\n", liveMonitorStack->size());
                  else
                     traceMsg(self()->comp(), "   size empty\n");
                  }
               break;
               }

         bool endOfEBB = !(b->getNextBlock() && b->getNextBlock()->isExtensionOfPreviousBlock());
         if (endOfEBB &&
             (b->branchesBackwards() ||
              numEBBsSinceFreeingVariableSizeSymRefs > MAX_EBBS_BEFORE_FREEING_VARIABLE_SIZED_SYMREFS))
            {
            if (self()->traceBCDCodeGen())
               traceMsg(self()->comp(),"\tblock_%d branches backwards, so free all symbols in the _variableSizeSymRefPendingFreeList\n",b->getNumber());
            self()->freeAllVariableSizeSymRefs();
            numEBBsSinceFreeingVariableSizeSymRefs = 0;
            }
         }

      if (((opCode == TR::BBEnd) && !fixedUpBlock) ||
          node->getOpCode().isBranch() ||
          node->getOpCode().isSwitch())
         {
         fixedUpBlock = true;
         //GCMAP
         if ( (self()->comp()->getOption(TR_MimicInterpreterFrameShape) && self()->comp()->areSlotsSharedByRefAndNonRef() ) || self()->comp()->getOption(TR_PoisonDeadSlots))
            {
            // TODO : look at last warm block code above
            //
            if ((!self()->comp()->getOption(TR_PoisonDeadSlots)&& liveLocals) || (self()->comp()->getOption(TR_PoisonDeadSlots) &&  self()->getCurrentEvaluationBlock()->getLiveLocals()))
               {
               newBlocks.clear();
               TR::Block *block = self()->getCurrentEvaluationBlock();

               TR_BitVectorIterator bvi(self()->comp()->getOption(TR_PoisonDeadSlots) ? *block->getLiveLocals(): *liveLocals);

               if (self()->comp()->getOption(TR_TraceCG) && self()->comp()->getOption(TR_PoisonDeadSlots))
                  traceMsg(self()->comp(), "POISON DEAD SLOTS --- Parent Block Number: %d\n", block->getNumber());


               while (bvi.hasMoreElements())
                  {
                  int32_t liveLocalIndex = bvi.getNextElement();
                  TR_ASSERT((liveLocalIndex <= maxLiveLocalIndex), "Symbol has live local index higher than computed max\n");
                  TR::SymbolReference * liveAutoSymRef = liveLocalSyms[liveLocalIndex];

                  if (self()->comp()->getOption(TR_TraceCG) && self()->comp()->getOption(TR_PoisonDeadSlots))
                     traceMsg(self()->comp(), "POISON DEAD SLOTS --- Parent Block: %d, Maintained Live Local: %d\n", block->getNumber(), liveAutoSymRef->getReferenceNumber());

                  if(self()->comp()->getOption(TR_PoisonDeadSlots) && (!liveAutoSymRef || block->isCreatedAtCodeGen()))
                  {
                     //Don't process a block we created to poison a dead slot.
                     continue;
                  }

                  if(!liveAutoSymRef)
                     {
                     continue;
                     }
                  TR::AutomaticSymbol * liveAutoSym = liveAutoSymRef->getSymbol()->castToAutoSymbol();

                  //For slot poisoning, a monitored object is still in the GCMaps even if its liveness has ended.
                  //
                  if ((liveAutoSym->getType().isAddress() && liveAutoSym->isSlotSharedByRefAndNonRef()) || (self()->comp()->getOption(TR_PoisonDeadSlots) && !liveAutoSymRef->getSymbol()->holdsMonitoredObject()))
                     {
                     for (auto succ = block->getSuccessors().begin(); succ != block->getSuccessors().end();)
                        {
                        auto next = succ;
                        ++next;
                        if ((*succ)->getTo() == self()->comp()->getFlowGraph()->getEnd())
                           {
                           succ = next;
                           continue;
                           }
                        TR::Block *succBlock = (*succ)->getTo()->asBlock();

                        if (self()->comp()->getOption(TR_PoisonDeadSlots) && succBlock->isExtensionOfPreviousBlock())
                           {
                           if (self()->comp()->getOption(TR_TraceCG) && self()->comp()->getOption(TR_PoisonDeadSlots))
                              traceMsg(self()->comp(), "POISON DEAD SLOTS --- Successor Block Number %d is extension of Parent Block %d ... skipping \n", succBlock->getNumber(), block->getNumber());
                           succ = next;
                           continue;  //We cannot poison in an extended block as gcmaps are still live for maintained live locals  !!!! if target of jump, ignore, could be extension
                           }

                        if (self()->comp()->getOption(TR_TraceCG) && self()->comp()->getOption(TR_PoisonDeadSlots))
                           traceMsg(self()->comp(), "POISON DEAD SLOTS --- Successor Block Number %d, of Parent Block %d \n", succBlock->getNumber(), block->getNumber());

                        TR_BitVector *succLiveLocals = succBlock->getLiveLocals();
                        if (succLiveLocals && !succLiveLocals->get(liveLocalIndex)) //added
                           {
                           bool zeroOut = true;
                           TR_BitVectorIterator sbvi(*succLiveLocals);
                           while (sbvi.hasMoreElements())
                              {
                              int32_t succLiveLocalIndex = sbvi.getNextElement();
                              TR_ASSERT((succLiveLocalIndex <= maxLiveLocalIndex), "Symbol has live local index higher than computed max\n");
                              TR::SymbolReference * succLiveAutoSymRef = liveLocalSyms[succLiveLocalIndex];
                              if (self()->comp()->getOption(TR_TraceCG) && self()->comp()->getOption(TR_PoisonDeadSlots))
                                 traceMsg(self()->comp(), "POISON DEAD SLOTS --- Successor Block %d contains live local %d\n", succBlock->getNumber(), succLiveAutoSymRef->getReferenceNumber());

                              TR::AutomaticSymbol * succLiveAutoSym = succLiveAutoSymRef->getSymbol()->castToAutoSymbol();
                              if ( (succLiveAutoSym->getType().isAddress() && succLiveAutoSym->isSlotSharedByRefAndNonRef()) || self()->comp()->getOption(TR_PoisonDeadSlots))
                                 {
                                 if ((succLiveAutoSym->getGCMapIndex() == liveAutoSym->getGCMapIndex()) ||
                                     ((TR::Symbol::convertTypeToNumberOfSlots(succLiveAutoSym->getDataType()) == 2) &&
                                      ((succLiveAutoSym->getGCMapIndex()+1) == liveAutoSym->getGCMapIndex())))
                                    {
                                    zeroOut = false;
                                    break;
                                    }
                                 }
                              }

                           if (zeroOut)
                              {
                              self()->comp()->getFlowGraph()->setStructure(0);
                              self()->zeroOutAutoOnEdge(liveAutoSymRef, block, succBlock, &newBlocks, &fsdStores);
                              }
                           }
                        succ = next;
                        }

                     // TODO : Think about exc edges case below
                     //
                     for (auto esucc = block->getExceptionSuccessors().begin(); esucc != block->getExceptionSuccessors().end(); ++esucc)
                        {
                        TR::Block *esuccBlock = (*esucc)->getTo()->asBlock();
                        //since we have asked the liveness analysis to assume that uses in the OSR block don't exist
                        //in certain cases, this code thinks that some locals are dead in the OSR block so it tries
                        //to zero them out. But we don't want that to happen
                        if (esuccBlock->isOSRCodeBlock() || esuccBlock->isOSRCatchBlock())
                           continue;
                        TR_BitVector *esuccLiveLocals = esuccBlock->getLiveLocals();
                        TR_ASSERT(esuccLiveLocals, "No live locals for successor block\n");
                        if (!esuccLiveLocals->get(liveLocalIndex))
                           {
                           bool zeroOut = true;
                           TR_BitVectorIterator sbvi(*esuccLiveLocals);
                           while (sbvi.hasMoreElements())
                              {
                              int32_t succLiveLocalIndex = sbvi.getNextElement();
                              TR_ASSERT((succLiveLocalIndex <= maxLiveLocalIndex), "Symbol has live local index higher than computed max\n");
                              TR::SymbolReference * succLiveAutoSymRef = liveLocalSyms[succLiveLocalIndex];
                              TR::AutomaticSymbol * succLiveAutoSym = succLiveAutoSymRef->getSymbol()->castToAutoSymbol();
                              if ( succLiveAutoSym->getType().isAddress() && ( succLiveAutoSym->isSlotSharedByRefAndNonRef() || self()->comp()->getOption(TR_PoisonDeadSlots)))
                                 {
                                 if ((succLiveAutoSym->getGCMapIndex() == liveAutoSym->getGCMapIndex()) ||
                                     ((TR::Symbol::convertTypeToNumberOfSlots(succLiveAutoSym->getDataType()) == 2) &&
                                      ((succLiveAutoSym->getGCMapIndex()+1) == liveAutoSym->getGCMapIndex())))
                                    {
                                    zeroOut = false;
                                    break;
                                    }
                                 }
                              }

                           if (zeroOut)
                              {
                              TR::TreeTop *cursorTree = esuccBlock->getEntry()->getNextTreeTop();
                              TR::TreeTop *endTree = esuccBlock->getExit();
                              bool storeExists = false;
                              while (cursorTree != endTree)
                                 {
                                 TR::Node *cursorNode = cursorTree->getNode();
                                 if (cursorNode->getOpCode().isStore())
                                    {
                                    if ((cursorNode->getSymbolReference() == liveAutoSymRef) &&
                                        (cursorNode->getFirstChild()->getOpCodeValue() == TR::aconst) &&
                                        (cursorNode->getFirstChild()->getAddress() == 0 || cursorNode->getFirstChild()->getAddress() == 0xdeadf00d ))
                                       {
                                       storeExists = true;
                                       break;
                                       }
                                    }
                                 else
                                    break;

                                 cursorTree = cursorTree->getNextTreeTop();
                                 }

                              if (!storeExists)
                                 {
                                 TR::Node *storeNode;
                                 if (self()->comp()->getOption(TR_PoisonDeadSlots))
                                    storeNode = generatePoisonNode(self()->comp(), block, liveAutoSymRef);
                                 else
                                    storeNode = TR::Node::createStore(liveAutoSymRef, TR::Node::aconst(block->getEntry()->getNode(), 0));
                                 if (storeNode)
                                    {
                                    TR::TreeTop *storeTree = TR::TreeTop::create(self()->comp(), storeNode);
                                    esuccBlock->prepend(storeTree);
                                    fsdStores.add(storeNode);
                                   }
                                 }
                              }
                           }
                        }
                     }
                  }
               }
            }
         }

      self()->setLiveLocals(liveLocals);
      self()->setLiveMonitors(liveMonitors);

      if (self()->comp()->getOption(TR_TraceCG) || debug("traceGRA"))
         {
         // any evaluator that handles multiple trees will need to dump
         // the others
         self()->comp()->getDebug()->saveNodeChecklist(nodeChecklistBeforeDump);
         self()->comp()->getDebug()->dumpSingleTreeWithInstrs(tt, NULL, true, false, true, true);
         trfprintf(self()->comp()->getOutFile(),"\n------------------------------\n");
         trfflush(self()->comp()->getOutFile());
         }

      self()->setLastInstructionBeforeCurrentEvaluationTreeTop(self()->comp()->getAppendInstruction());
      self()->setCurrentEvaluationTreeTop(tt);
      self()->setImplicitExceptionPoint(NULL);

      bool doEvaluation = true;
      if ((node->getOpCode().isStore() &&
            node->getSymbol()->holdsMonitoredObject() &&
            !node->isLiveMonitorInitStore()) || node->getOpCode().getOpCodeValue() == TR::monexitfence)
         {
         if (traceLiveMon)
            {
            traceMsg(self()->comp(), "liveMonitorStack %p ", liveMonitorStack);
            if (liveMonitorStack)
               traceMsg(self()->comp(), "   size %d\n", liveMonitorStack->size());
            else
               traceMsg(self()->comp(), "   size empty\n");
            traceMsg(self()->comp(), "Looking at Node %p with symbol %p",node,node->getSymbol());
            }
         bool isMonent = node->getOpCode().getOpCodeValue() != TR::monexitfence;
         if (isMonent)
            {
            // monent
            if (liveMonitors)
               liveMonitors = new (self()->trHeapMemory()) TR_BitVector(*liveMonitors);
            else
               liveMonitors = new (self()->trHeapMemory()) TR_BitVector(numMonitorLocals, self()->trMemory());

            // add this monent to the block's stack
            //
            if (liveMonitorStack)
               {
               liveMonitorStack->push(node->getSymbolReference());
               if (traceLiveMon)
                  traceMsg(self()->comp(), "pushing symref %p (#%u) onto monitor stack\n", node->getSymbolReference(), node->getSymbolReference()->getReferenceNumber());
               }

            liveMonitors->set(node->getSymbol()->castToRegisterMappedSymbol()->getLiveLocalIndex());

            if (traceLiveMon)
               traceMsg(self()->comp(), "monitor %p went live at node %p\n", node->getSymbol(), node);
            }
         else if (!isMonent)
            {
            if (liveMonitorStack)
               {
               // monexit
               TR_ASSERT(liveMonitors, "inconsistent live monitor state");

               // pop this monexit from the block's stack
               //
               if (!liveMonitorStack->isEmpty())
                  {
                  liveMonitors = new (self()->trHeapMemory()) TR_BitVector(*liveMonitors);

                  if (self()->comp()->getOption(TR_PoisonDeadSlots))
                     {
                     TR::SymbolReference *symRef = liveMonitorStack->pop();
                     liveMonitors->reset(symRef->getSymbol()->castToRegisterMappedSymbol()->getLiveLocalIndex());
                     TR::Node *storeNode = NULL;

                     if (self()->comp()->getOption(TR_TraceCG) && self()->comp()->getOption(TR_PoisonDeadSlots))
                        traceMsg(self()->comp(), "POISON DEAD SLOTS --- MonExit Block Number: %d\n", self()->getCurrentEvaluationBlock()->getNumber());

                     storeNode = generatePoisonNode(self()->comp(), self()->getCurrentEvaluationBlock(), symRef);
                     if (storeNode)
                        {
                        TR::TreeTop *storeTree = TR::TreeTop::create(self()->comp(), storeNode);
                        self()->getCurrentEvaluationBlock()->prepend(storeTree);
                        fsdStores.add(storeNode);
                        }
                     }
                  else
                     {
                     TR::SymbolReference *symref = liveMonitorStack->pop();

                     if (traceLiveMon)
                        traceMsg(self()->comp(), "popping symref %p (#%u) off monitor stack\n", symref, symref->getReferenceNumber());
                     liveMonitors->reset(symref->getSymbol()->castToRegisterMappedSymbol()->getLiveLocalIndex());
                     }

                  }
               else
                  {
                  dumpOptDetails(self()->comp(), "liveMonitorStack %p is inconsistently empty at node %p!\n", liveMonitorStack, node);
                  if (liveMonitors)
                     liveMonitors->empty();
                  }

               if (traceLiveMon)
                  traceMsg(self()->comp(), "monitor %p went dead at node %p\n", node->getSymbol(), node);
               }
           // no need to generate code for this store
            //
            doEvaluation = false;
            }
         }

#ifdef TR_TARGET_S390
      if (self()->getAddStorageReferenceHints())
         self()->addStorageReferenceHints(node);
#endif


      if (doEvaluation)
         self()->evaluate(node);


      if (self()->comp()->getOption(TR_TraceCG) || debug("traceGRA"))
         {
         TR::Instruction *lastInstr = self()->comp()->getAppendInstruction();
         tt->setLastInstruction(lastInstr == prevInstr ? 0 : lastInstr);
         }

      if (liveLocals)
         {
         TR::AutomaticSymbol * liveSym = 0;
         if (debug("checkBlockEntryLiveLocals"))
            {
            // Check for a store into a local.
            // If so, this local becomes live at this point.
            //
            if (node->getOpCode().isStore())
               {
               liveSym = node->getSymbol()->getAutoSymbol();
               }

            // Check for a loadaddr of a local object.
            // If so, this local object becomes live at this point.
            //
            else if (opCode == TR::treetop)
               {
               if (firstChild->getOpCodeValue() == TR::loadaddr)
                  liveSym = firstChild->getSymbol()->getLocalObjectSymbol();
               }
            if (liveSym && liveSym->getLiveLocalIndex() == (uint16_t)-1)
               liveSym = NULL;
            }
         else
            {
            // Check for a store into a collected local reference.
            // If so, this local becomes live at this point.
            //
            if ((opCode == TR::astore) &&
                ((!self()->comp()->getOption(TR_MimicInterpreterFrameShape)) ||
                 (!self()->comp()->areSlotsSharedByRefAndNonRef()) ||
                 (!fsdStores.find(node))))
               {
               liveSym = node->getSymbol()->getAutoSymbol();
               }

            // Check for a loadaddr of a local object containing collected references.
            // If so, this local object becomes live at this point.
            //
            else if (opCode == TR::treetop)
               {
               if (firstChild->getOpCodeValue() == TR::loadaddr)
                  liveSym = firstChild->getSymbol()->getLocalObjectSymbol();
               }
            if (liveSym && !liveSym->isCollectedReference())
               liveSym = NULL;
            }

         bool newLiveLocals = false;
         if (liveSym)
            {
            liveLocals = new (self()->trHeapMemory()) TR_BitVector(*liveLocals);
            newLiveLocals = true;
            if (liveSym)
               liveLocals->set(liveSym->getLiveLocalIndex());
            }

         // In interpreter-frame mode a reference can share a slot with a nonreference so when we see a store to a nonreference
         // we need to make sure that any reference that it shares a slot with is marked as dead
         //
         if (self()->comp()->getOption(TR_MimicInterpreterFrameShape) && node->getOpCode().isStore() && opCode != TR::astore)
            {
            TR::AutomaticSymbol * nonRefStoreSym = node->getSymbol()->getAutoSymbol();
            if (nonRefStoreSym && (nonRefStoreSym->getGCMapIndex() != -1))  //defect 147894: don't check this for GCMapIndex = -1
               {
               bool isTwoSlots = ( TR::Symbol::convertTypeToNumberOfSlots(nonRefStoreSym->getDataType()) == 2);
               if (isTwoSlots || nonRefStoreSym->isSlotSharedByRefAndNonRef())
                  {
                  ListIterator<TR::AutomaticSymbol> autoIterator(&methodSymbol->getAutomaticList());
                  if (!newLiveLocals)
                     liveLocals = new (self()->trHeapMemory()) TR_BitVector(*liveLocals);
                  for (TR::AutomaticSymbol * autoSym = autoIterator.getFirst(); autoSym; autoSym = autoIterator.getNext())
                     if (autoSym->getType().isAddress() && autoSym->getLiveLocalIndex() != (uint16_t)-1)
                        {
                        if (autoSym->getGCMapIndex() == nonRefStoreSym->getGCMapIndex())
                           liveLocals->reset(autoSym->getLiveLocalIndex());
                        else if (isTwoSlots && autoSym->getGCMapIndex() == nonRefStoreSym->getGCMapIndex() + 1)
                           liveLocals->reset(autoSym->getLiveLocalIndex());
                        }
                  }
               }
            }
         }

      bool compactVSSStack = false;
      if (!self()->comp()->getOption(TR_DisableVSSStackCompaction))
         {
         if (self()->comp()->getMethodHotness() < hot)
            compactVSSStack = true;
         else if (self()->comp()->getOption(TR_ForceVSSStackCompaction))
            compactVSSStack = true;
         }

      if (compactVSSStack && !_variableSizeSymRefPendingFreeList.empty())
         {
         TR::Node *ttNode = (node->getOpCodeValue() == TR::treetop) ? node->getFirstChild() : node;
         auto it = _variableSizeSymRefPendingFreeList.begin();
         TR::SymbolReference *symRef;
         while (it != _variableSizeSymRefPendingFreeList.end())
            {
        	//Element is removed within freeVariableSizeSymRef. Need a reference to next element
        	auto next = it;
            ++next;
            TR_ASSERT((*it)->getSymbol()->isVariableSizeSymbol(),"symRef #%d must contain a variable size symbol\n",(*it)->getReferenceNumber());
            auto *sym = (*it)->getSymbol()->getVariableSizeSymbol();
            bool found = (std::find(_variableSizeSymRefFreeList.begin(), _variableSizeSymRefFreeList.end(), (*it)) != _variableSizeSymRefFreeList.end());
            if (self()->traceBCDCodeGen())
               {
               if (sym->getNodeToFreeAfter())
                  traceMsg(self()->comp(),"pending free temps : looking at symRef #%d (%s) refCount %d sym->getNodeToFreeAfter() %p ttNode %p (find sym in list %d)\n",
                		  (*it)->getReferenceNumber(),self()->getDebug()->getName(sym),sym->getReferenceCount(),
                     sym->getNodeToFreeAfter(),ttNode,found);
               else
                  traceMsg(self()->comp(),"pending free temps : looking at symRef #%d (%s) refCount %d (find sym in list %d)\n",
                		  (*it)->getReferenceNumber(),self()->getDebug()->getName(sym),sym->getReferenceCount(),found);
               }
            if (!sym->isAddressTaken() && !found)
               {
               TR::Node *nodeToFreeAfter = sym->getNodeToFreeAfter();
               bool nodeToFreeAfterIsCurrentNode = nodeToFreeAfter && (ttNode==nodeToFreeAfter);
               if (sym->getReferenceCount() == 0 &&
                  (!nodeToFreeAfter || nodeToFreeAfterIsCurrentNode))
                  {
                  self()->freeVariableSizeSymRef(*it);  // will also remove sym from the pending free list
                  }
               else if (sym->getReferenceCount() > 0 && nodeToFreeAfterIsCurrentNode)
                  {
                  if (self()->traceBCDCodeGen())
                     traceMsg(self()->comp(),"\treset nodeToFreeAfter %p->NULL for sym %p with refCount %d > 0\n",nodeToFreeAfter,sym,sym->getReferenceCount());
                  sym->setNodeToFreeAfter(NULL);
                  }
               else
                  {
                  // We'd like to assert the following, but refcounts are unsigned, so we can't
                  //TR_ASSERT(sym->getReferenceCount() >= 0,"sym %p refCount %d should be >= 0\n",sym,sym->getReferenceCount());
                  }
               }
            it = next;
            }
         }

      if (self()->comp()->getOption(TR_TraceCG) || debug("traceGRA"))
         {
         self()->comp()->getDebug()->restoreNodeChecklist(nodeChecklistBeforeDump);
         if (tt == self()->getCurrentEvaluationTreeTop())
            {
            trfprintf(self()->comp()->getOutFile(),"------------------------------\n");
            self()->comp()->getDebug()->dumpSingleTreeWithInstrs(tt, prevInstr->getNext(), true, true, true, false);
            }
         else
            {
            // dump all the trees that the evaluator handled
            trfprintf(self()->comp()->getOutFile(),"------------------------------");
            for (TR::TreeTop *dumptt = tt; dumptt != self()->getCurrentEvaluationTreeTop()->getNextTreeTop(); dumptt = dumptt->getNextTreeTop())
               {
               trfprintf(self()->comp()->getOutFile(),"\n");
               self()->comp()->getDebug()->dumpSingleTreeWithInstrs(dumptt, NULL, true, false, true, false);
               }
            // all instructions are on the tt tree
            self()->comp()->getDebug()->dumpSingleTreeWithInstrs(tt, prevInstr->getNext(), false, true, false, false);
            }
         trfflush(self()->comp()->getOutFile());
         }
      }

   if (self()->traceBCDCodeGen())
      traceMsg(self()->comp(),"\tinstruction selection is complete so free all symbols in the _variableSizeSymRefPendingFreeList\n");

   self()->freeAllVariableSizeSymRefs();

#if defined(TR_TARGET_S390) && !defined(PUBLIC_BUILD)
   // Virtual function insertInstructionPrefetches is implemented only for s390 platform,
   // for all other platforms the function is empty
   //
   self()->insertInstructionPrefetches();
#endif
   } // Stack memory region ends

   if (self()->comp()->getOption(TR_TraceCG) || debug("traceGRA"))
      self()->comp()->incVisitCount();

   if (self()->getDebug())
      self()->getDebug()->roundAddressEnumerationCounters();

   self()->endInstructionSelection();

   if (self()->comp()->getOption(TR_TraceCG))
      diagnostic("</selection>\n");
   }


// J9, Z
//
// This can surely be done more efficiently.  Walk the CFG and go block by block
// rather than iterating through TreeTops looking for BBStarts.
//
void
J9::CodeGenerator::splitWarmAndColdBlocks()
   {
   if (!self()->allowSplitWarmAndColdBlocks() || self()->comp()->compileRelocatableCode())
      {
      return;
      }

   TR::Block * block = NULL;
   TR::TreeTop * tt;
   TR::Node * node;
   TR::Block * firstColdBlock = NULL, * firstColdExtendedBlock = NULL;

   for (tt = self()->comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      node = tt->getNode();

      if (node->getOpCodeValue() == TR::BBStart)
         {
         block = node->getBlock();

         // If this is the first cold block, remember where the warm blocks ended.
         // If it is a warm block and cold blocks have already been found, they
         // are treated as warm.
         //
         if (block->isCold())
            {
            if (!firstColdBlock)
               firstColdBlock = block;
            }
         else
            {
            firstColdBlock = NULL;
            firstColdExtendedBlock = NULL;
            }

         if (!block->isExtensionOfPreviousBlock())
            {
            if (firstColdBlock && !firstColdExtendedBlock)
               {
               if (!block->getPrevBlock() ||
                   !block->getPrevBlock()->canFallThroughToNextBlock())
                  firstColdExtendedBlock = block;
               else
                  firstColdBlock = NULL;
               }
            }
         }
      }

   // Mark the split point between warm and cold blocks, so they can be
   // allocated in different code sections.
   //
   TR::Block *lastWarmBlock;
   if (firstColdExtendedBlock)
      {
      lastWarmBlock = firstColdExtendedBlock->getPrevBlock();
      if (!lastWarmBlock)
         {
         // All the blocks are cold: insert a new goto block ahead of real blocks
         lastWarmBlock = TR::TransformUtil::insertNewFirstBlockForCompilation(self()->comp());
         }
      }
   else
      {
      // All the blocks are warm - the split point is between the last
      // instruction and the snippets.
      //
      lastWarmBlock = block;
      }

   lastWarmBlock->setIsLastWarmBlock();

   // If the last tree in the last warm block is not a TR::Goto, insert a goto tree
   // at the end of the block.
   // If there is a following block the goto will branch to it so that when the
   // code is split any fall-through will go to the right place.
   // If there is no following block the goto will branch to the first block; in
   // this case the goto should never be reached, it is there only to
   // make sure that the instruction following the last real treetop will be in
   // warm code, so if it is a helper call (e.g. for a throw) the return address
   // is in this method's code.
   //
   if (lastWarmBlock->getNumberOfRealTreeTops() == 0)
      tt = lastWarmBlock->getEntry();
   else
      tt = lastWarmBlock->getLastRealTreeTop();

   node = tt->getNode();

   if (!(node->getOpCode().isGoto() || node->getOpCode().isJumpWithMultipleTargets() || node->getOpCode().isReturn()))
      {
      // Find the block to be branched to
      //
      TR::TreeTop * targetTreeTop = lastWarmBlock->getExit()->getNextTreeTop();
      if (targetTreeTop)
         {
         // Branch to following block. Make sure it is not marked as an
         // extension block so that it will get a label generated.
         //
         targetTreeTop->getNode()->getBlock()->setIsExtensionOfPreviousBlock(false);
         }
      else
         {
         // Branch to the first block. This will not be marked as an extension
         // block.
         targetTreeTop = self()->comp()->getStartBlock()->getEntry();
         }

      // Generate the goto and insert it into the end of the last warm block.
      //
      TR::TreeTop *gotoTreeTop = TR::TreeTop::create(self()->comp(), TR::Node::create(node, TR::Goto, 0, targetTreeTop));

      // Move reg deps from BBEnd to goto
      //
      TR::Node *bbEnd = lastWarmBlock->getExit()->getNode();
      if (bbEnd->getNumChildren() > 0)
         {
         TR::Node *glRegDeps = bbEnd->getChild(0);

         gotoTreeTop->getNode()->setNumChildren(1);
         gotoTreeTop->getNode()->setChild(0, glRegDeps);

         bbEnd->setChild(0,NULL);
         bbEnd->setNumChildren(0);
         }

      tt->insertAfter(gotoTreeTop);
      }

   }


void
J9::CodeGenerator::populateOSRBuffer()
   {

   if (!self()->comp()->getOption(TR_EnableOSR))
      return;

//The following struct definitions are coming from VM include files and are intended as
//a legend for OSR buffer

    // typedef struct J9OSRBuffer {
    //   U_32 numberOfFrames;
    //   // frames here - reachable by "(J9OSRFrame*)(buffer + 1)"
    // } J9OSRBuffer;
    // typedef struct J9OSRFrame {
    //   J9Method *method;
    //   U_8 *bytecodePC;
    //   U_32 maxStack;
    //   U_32 pendingStackHeight;
    //   // stack slots here - reachable by "(UDATA*)(frame + 1)"
    // } J9OSRFrame;

   self()->comp()->getOSRCompilationData()->buildSymRefOrderMap();
   const TR_Array<TR_OSRMethodData *>& methodDataArray = self()->comp()->getOSRCompilationData()->getOSRMethodDataArray();
   bool traceOSR = self()->comp()->getOption(TR_TraceOSR);
   uint32_t maxScratchBufferSize = 0;
   const int firstSymChildIndex = 2;

/*
   for (int32_t i = 0; i < methodDataArray.size(); ++i)
      for (int32_t j = i+1; j < methodDataArray.size(); ++j)
         TR_ASSERT((methodDataArray[i] == NULL && methodDataArray[j] == NULL) || (methodDataArray[i] != methodDataArray[j]),
                 "methodDataArray elements %d and %d are equal\n", i, j);
*/

   TR::Block * block = NULL;
   for(TR::TreeTop * tt = self()->comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      // Write pending pushes, parms, and locals to vmthread's OSR buffer

      TR::Node* n = tt->getNode();
      if (n->getOpCodeValue() == TR::BBStart)
         {
         block = n->getBlock();
         continue;
         }
      if (n->getOpCodeValue() == TR::treetop && n->getNumChildren() == 1)
         n = n->getFirstChild();
      else
         continue;
      if (n->getOpCodeValue() != TR::call ||
          n->getSymbolReference()->getReferenceNumber() != TR_prepareForOSR)
         continue;

      TR::Node *callNode = n;
      TR_OSRMethodData* osrMethodData = methodDataArray[callNode->getChild(1)->getInt()+1];
      TR_ASSERT(osrMethodData != NULL && osrMethodData->getOSRCodeBlock() != NULL,
             "osr method data or its block is NULL\n");

      if (traceOSR)
         traceMsg(self()->comp(), "Lowering trees in OSR block_%d...\n", block->getNumber());

      //osrFrameIndex is a field in the vmThread that is initialized by the VM to the offset
      //of the start of the first (deepest) frame in the OSR buffer
      //Once we are done with generating code that populates the current frame, we generate code
      //that advances this field to the point to the next frame at the end of the OSR code block
      //of each frame
      TR::ResolvedMethodSymbol *methSym = osrMethodData->getMethodSymbol();
      TR::Node *osrBufferNode = TR::Node::createLoad(callNode, self()->symRefTab()->findOrCreateOSRBufferSymbolRef());
      TR::Node *osrFrameIndex = TR::Node::createLoad(callNode, self()->symRefTab()->findOrCreateOSRFrameIndexSymbolRef());
      TR::Node *osrScratchBufferNode =  TR::Node::createLoad(callNode, self()->symRefTab()->findOrCreateOSRScratchBufferSymbolRef());

      TR::TreeTop* insertionPoint = tt->getPrevRealTreeTop();
      bool inlinesAnyMethod = osrMethodData->inlinesAnyMethod();

      if (traceOSR)
         traceMsg(self()->comp(), "callerIndex %d: max pending push slots=%d, # of auto slots=%d, # of arg slots=%d\n",
                 osrMethodData->getInlinedSiteIndex(), methSym->getNumPPSlots(),
            methSym->getResolvedMethod()->numberOfTemps(), methSym->getNumParameterSlots());

      uint32_t numOfSymsThatShareSlot = 0;
      int32_t scratchBufferOffset = 0;
      for (int32_t child = firstSymChildIndex; child+2 < callNode->getNumChildren(); child += 3)
         {
         TR::Node* loadNode = callNode->getChild(child);
         int32_t symRefNumber = callNode->getChild(child+1)->getInt();
         int32_t symRefOrder = callNode->getChild(child+2)->getInt();
         TR::SymbolReference* symRef = self()->symRefTab()->getSymRef(symRefNumber);

         int32_t specialCasedSlotIndex = -1;
         //if (methSym->getSyncObjectTemp() == symRef)
         if (symRef->getSymbol()->holdsMonitoredObject())
            specialCasedSlotIndex = methSym->getSyncObjectTempIndex();
         //else if (methSym->getThisTempForObjectCtor() == symRef)
         else if (symRef->getSymbol()->isThisTempForObjectCtor())
            specialCasedSlotIndex = methSym->getThisTempForObjectCtorIndex();
         //else if (methSym->getATCDeferredCountTemp() == symRef)

         int32_t slotIndex = symRef->getCPIndex() >= methSym->getFirstJitTempIndex()
            ? methSym->getFirstJitTempIndex()
            : symRef->getCPIndex();

         if (symRef->getCPIndex() >= methSym->getFirstJitTempIndex())
            {
            TR_ASSERT(((slotIndex == methSym->getSyncObjectTempIndex()) ||
                     (slotIndex == methSym->getThisTempForObjectCtorIndex())), "Unknown temp sym ref being written to the OSR buffer; probably needs special casing\n");
            }

         if (specialCasedSlotIndex != -1)
            slotIndex = specialCasedSlotIndex;

         int32_t symSize = symRef->getSymbol()->getSize();
         bool sharedSlot = (symRefOrder != -1);
         if (sharedSlot)
            {
            insertionPoint = self()->genSymRefStoreToArray(callNode, osrScratchBufferNode, NULL, loadNode, scratchBufferOffset, insertionPoint);
            osrMethodData->addScratchBufferOffset(slotIndex, symRefOrder, scratchBufferOffset);
            scratchBufferOffset += symSize;
            numOfSymsThatShareSlot++;
            }
         else
            {
            TR::DataType dt = symRef->getSymbol()->getDataType();
            bool takesTwoSlots = dt == TR::Int64 || dt == TR::Double;
            int32_t offset = osrMethodData->slotIndex2OSRBufferIndex(slotIndex, symSize, takesTwoSlots);
            insertionPoint = self()->genSymRefStoreToArray(callNode, osrBufferNode, osrFrameIndex, loadNode, offset, insertionPoint);
            }
         }

      osrMethodData->setNumOfSymsThatShareSlot(numOfSymsThatShareSlot);
      maxScratchBufferSize = (maxScratchBufferSize > scratchBufferOffset) ? maxScratchBufferSize : scratchBufferOffset;

      if (traceOSR)
         {
         traceMsg(self()->comp(), "%s %s %s: written out bytes in OSR buffer\n",
                 osrMethodData->getInlinedSiteIndex() == -1 ? "Method," : "Inlined method,",
                 inlinesAnyMethod? "inlines another method,": "doesn't inline any method,",
                 methSym->signature(self()->trMemory()));
         }
      int32_t totalNumOfSlots = osrMethodData->getTotalNumOfSlots();
      //The OSR helper call will print the contents of the OSR buffer (if trace option is on)
      //and populate the OSR buffer with the correct values of the shared slots (if there is any)
      bool emitCall = false;
      if ((numOfSymsThatShareSlot > 0) ||
          self()->comp()->getOption(TR_EnablePrepareForOSREvenIfThatDoesNothing))
         emitCall = true;

      int32_t startIndex = 0;
      if (emitCall)
         startIndex = firstSymChildIndex;

      for (int32_t i = startIndex; i < callNode->getNumChildren(); i++)
         callNode->getChild(i)->recursivelyDecReferenceCount();

      if (emitCall)
         {
         callNode->setNumChildren(firstSymChildIndex+1);
         TR_ASSERT(totalNumOfSlots < (1 << 16) - 1, "only 16 bits are reserved for number of slots");
         TR_ASSERT(numOfSymsThatShareSlot < (1 << 16) -1, "only 16 bits are reserved for number of syms that share slots");
         callNode->setAndIncChild(firstSymChildIndex,
            TR::Node::create(callNode, TR::iconst, 0, totalNumOfSlots | (numOfSymsThatShareSlot << 16)));
         insertionPoint = tt;
         }
      else
         {
         TR::TreeTop *prev = tt->getPrevTreeTop();
         TR::TreeTop *next = tt->getNextTreeTop();
         prev->join(next);
         insertionPoint = prev;
         }

      //at the end of each OSR code block, we need to advance osrFrameIndex such that
      //it points to the beginning of the next osr frame
      //osrFrameIndex += osrMethodData->getTotalDataSize();
      TR::TreeTop* osrFrameIndexAdvanceTreeTop = TR::TreeTop::create(self()->comp(),
         TR::Node::createStore(self()->symRefTab()->findOrCreateOSRFrameIndexSymbolRef(),
            TR::Node::create(TR::iadd, 2, osrFrameIndex,
                            TR::Node::create(callNode, TR::iconst, 0, osrMethodData->getTotalDataSize())
               )
            )
         );
      insertionPoint->insertTreeTopsAfterMe(osrFrameIndexAdvanceTreeTop);
      }

   for (int32_t i = 0; i < methodDataArray.size(); i++)
      {
      TR_OSRMethodData* osrMethodData = methodDataArray[i];
      //osrMethodData can be NULL when the inlined method didn't cause a call to ILGen (e.g., a jni method)
      if (methodDataArray[i] == NULL)
         continue;
      //Initialize the number of syms that share slots to zero if it's hasn't been already initialized.
      if (osrMethodData->getNumOfSymsThatShareSlot() == -1)
         {
         osrMethodData->setNumOfSymsThatShareSlot(0);
         }
      }

   self()->comp()->getOSRCompilationData()->setMaxScratchBufferSize(maxScratchBufferSize);
   }


void
J9::CodeGenerator::processRelocations()
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(self()->comp()->fe());

   //Project neutral non-AOT processRelocation
   OMR::CodeGenerator::processRelocations();

   int32_t missedSite = -1;

   if (self()->comp()->getOption(TR_AOT) != false)
      {
//#if (defined(TR_HOST_X86) || defined(TR_HOST_S390) || defined(TR_HOST_POWER))
      uint32_t inlinedCallSize = self()->comp()->getNumInlinedCallSites();

      // Create temporary hashtable for ordering AOT guard relocations
      int32_t counter = inlinedCallSize;
      TR_InlinedSiteHastTableEntry *orderedInlinedSiteListTable;
      if (inlinedCallSize > 0)
         {
         orderedInlinedSiteListTable= (TR_InlinedSiteHastTableEntry*)self()->comp()->trMemory()->allocateMemory(sizeof(TR_InlinedSiteHastTableEntry) * inlinedCallSize, heapAlloc);
         memset(orderedInlinedSiteListTable, 0, sizeof(TR_InlinedSiteHastTableEntry)*inlinedCallSize);
         }
      else
         orderedInlinedSiteListTable = NULL;

      TR_InlinedSiteLinkedListEntry *entry = NULL;

      // Traverse list of AOT-specific guards and create relocation records
      TR::list<TR_AOTGuardSite*> *aotGuardSites = self()->comp()->getAOTGuardPatchSites();
      for(auto it = aotGuardSites->begin(); it != aotGuardSites->end(); ++it)
         {
         intptrj_t inlinedSiteIndex = -1;

         // first, figure out the appropriate relocation record type from the guard type and symbol
         TR_ExternalRelocationTargetKind type;
         switch ((*it)->getType())
            {
            case TR_DirectMethodGuard:
               if ((*it)->getGuard()->getSymbolReference()->getSymbol()->getMethodSymbol()->isStatic())
                  type = TR_InlinedStaticMethodWithNopGuard;
               else if ((*it)->getGuard()->getSymbolReference()->getSymbol()->getMethodSymbol()->isSpecial())
                  type = TR_InlinedSpecialMethodWithNopGuard;
               else if ((*it)->getGuard()->getSymbolReference()->getSymbol()->getMethodSymbol()->isVirtual())
                  type = TR_InlinedVirtualMethodWithNopGuard;
               else
                  TR_ASSERT(0, "unexpected AOTDirectMethodGuard method symbol");
               break;

            case TR_NonoverriddenGuard:
               type = TR_InlinedVirtualMethodWithNopGuard;
               break;
            case TR_RemovedNonoverriddenGuard:
               type = TR_InlinedVirtualMethod;
               break;

            case TR_InterfaceGuard:
               type = TR_InlinedInterfaceMethodWithNopGuard;
               break;
            case TR_RemovedInterfaceGuard:
               traceMsg(self()->comp(), "TR_RemovedInterfaceMethod\n");
               type = TR_InlinedInterfaceMethod;
               break;

            case TR_HCRGuard:
               // devinmp: TODO/FIXME this should arrange to create an AOT
               // relocation which, when loaded, creates a
               // TR_PatchNOPedGuardSiteOnClassRedefinition or similar.
               // Here we would previously create a TR_HCR relocation,
               // which is for replacing J9Class or J9Method pointers.
               // These would be the 'unresolved' variant
               // (TR_RedefinedClassUPicSite), which would (hopefully) never
               // get patched. If it were patched, it seems like it would
               // replace code with a J9Method pointer.
               if (!self()->comp()->getOption(TR_UseOldHCRGuardAOTRelocations))
                  continue;
               type = TR_HCR;
               break;

            case TR_MethodEnterExitGuard:
               if ((*it)->getGuard()->getCallNode()->getOpCodeValue() == TR::MethodEnterHook)
                  type = TR_CheckMethodEnter;
               else if ((*it)->getGuard()->getCallNode()->getOpCodeValue() == TR::MethodExitHook)
                  type = TR_CheckMethodExit;
               else
                  TR_ASSERT(0,"Unexpected TR_MethodEnterExitGuard at site %p guard %p node %p\n",
                                    *it, (*it)->getGuard(), (*it)->getGuard()->getCallNode());
               break;

            case TR_RemovedProfiledGuard:
               traceMsg(self()->comp(), "TR_ProfiledInlinedMethodRelocation\n");
               type = TR_ProfiledInlinedMethodRelocation;
               break;

            case TR_ProfiledGuard:
               if ((*it)->getGuard()->getTestType() == TR_MethodTest)
                  {
                  type = TR_ProfiledMethodGuardRelocation;
                  traceMsg(self()->comp(), "TR_ProfiledMethodGuardRelocation\n");
                  }
               else if ((*it)->getGuard()->getTestType() == TR_VftTest)
                  {
                  type = TR_ProfiledClassGuardRelocation;
                  traceMsg(self()->comp(), "TR_ProfiledClassGuardRelocation\n");
                  }
               else
                  TR_ASSERT(false, "unexpected profiled guard test type");
               break;

            default:
               TR_ASSERT(false, "got a unknown/non-AOT guard at AOT site");
               break;
            }

         switch (type)  // relocation record type
            {
            case TR_InlinedStaticMethodWithNopGuard:
            case TR_InlinedSpecialMethodWithNopGuard:
            case TR_InlinedVirtualMethodWithNopGuard:
            case TR_InlinedInterfaceMethodWithNopGuard:
            case TR_InlinedHCRMethod:
            case TR_ProfiledClassGuardRelocation:
            case TR_ProfiledMethodGuardRelocation:
            case TR_ProfiledInlinedMethodRelocation:
            case TR_InlinedVirtualMethod:
            case TR_InlinedInterfaceMethod:
               TR_ASSERT(inlinedCallSize, "TR_AOT expect inlinedCallSize to be larger than 0\n");
               inlinedSiteIndex = (intptrj_t)(*it)->getGuard()->getCurrentInlinedSiteIndex();
               entry = (TR_InlinedSiteLinkedListEntry *)self()->comp()->trMemory()->allocateMemory(sizeof(TR_InlinedSiteLinkedListEntry), heapAlloc);

               entry->reloType = type;
               entry->location = (uint8_t *)(*it)->getLocation();
               entry->destination = (uint8_t *)(*it)->getDestination();
               entry->guard = (uint8_t *)(*it)->getGuard();
               entry->next = NULL;

               if (orderedInlinedSiteListTable[inlinedSiteIndex].first)
                  {
                  orderedInlinedSiteListTable[inlinedSiteIndex].last->next = entry;
                  orderedInlinedSiteListTable[inlinedSiteIndex].last = entry;
                  }
               else
                  {
                  orderedInlinedSiteListTable[inlinedSiteIndex].first = entry;
                  orderedInlinedSiteListTable[inlinedSiteIndex].last = entry;
                  }
               break;

            case TR_CheckMethodEnter:
            case TR_CheckMethodExit:
            case TR_HCR:
               self()->addAOTRelocation(new (self()->trHeapMemory()) TR::ExternalRelocation((uint8_t *)(*it)->getLocation(),
                                                                                (uint8_t *)(*it)->getDestination(),
                                                                                type, self()),
                                __FILE__, __LINE__, NULL);
               break;

            default:
               TR_ASSERT(false, "got a unknown/non-AOT guard at AOT site");
               break;
            }
         }

      TR::list<TR::AOTClassInfo*>* classInfo = self()->comp()->_aotClassInfo;
      if (!classInfo->empty())
         {
         for (auto info = classInfo->begin(); info != classInfo->end(); ++info)
            {
            traceMsg(self()->comp(), "processing AOT class info: %p in %s\n", *info, self()->comp()->signature());
            traceMsg(self()->comp(), "ramMethod: %p cp: %p cpIndex: %x relo %d\n", (*info)->_method, (*info)->_constantPool, (*info)->_cpIndex, (*info)->_reloKind);
            traceMsg(self()->comp(), "clazz: %p classChain: %p\n", (*info)->_clazz, (*info)->_classChain);

            TR_OpaqueMethodBlock *ramMethod = (*info)->_method;

            int32_t siteIndex = -1;

            if (ramMethod != self()->comp()->getCurrentMethod()->getPersistentIdentifier()) // && info->_reloKind != TR_ValidateArbitraryClass)
               {
               int32_t i;
               for (i = 0; i < self()->comp()->getNumInlinedCallSites(); i++)
                  {
                  TR_InlinedCallSite &ics = self()->comp()->getInlinedCallSite(i);
                  TR_OpaqueMethodBlock *inlinedMethod = fej9->getInlinedCallSiteMethod(&ics);

                  traceMsg(self()->comp(), "\tinline site %d inlined method %p\n", i, inlinedMethod);
                  if (ramMethod == inlinedMethod)
                  //if (((uintptrj_t)ramMethod == (uintptrj_t)aotMethodInfo->resolvedMethod->getPersistentIdentifier()) && orderedInlinedSiteListTable[counter].first)
                     {
                     traceMsg(self()->comp(), "\t\tmatch!\n");
                     siteIndex = i;
                     break;
                     }
                  }

               if (i >= (int32_t) self()->comp()->getNumInlinedCallSites())
                  {
                  // this assumption isn't associated with a method directly in the compilation
                  // so we can't use a constant pool approach to validate: transform into TR_ValidateArbitraryClass
                  // kind of overkill for TR_ValidateStaticField, but still correct
                  (*info)->_reloKind = TR_ValidateArbitraryClass;
                  siteIndex = -1;   // invalidate main compiled method
                  traceMsg(self()->comp(), "\ttransformed into TR_ValidateArbitraryClass\n");
                  }
               }

            traceMsg(self()->comp(), "Found inlined site %d\n", siteIndex);

            TR_ASSERT(siteIndex < (int32_t) self()->comp()->getNumInlinedCallSites(), "did not find AOTClassInfo %p method in inlined site table", *info);

            self()->addAOTRelocation(new (self()->trHeapMemory()) TR::ExternalRelocation(NULL,
                                                                             (uint8_t *)(intptr_t)siteIndex,
                                                                             (uint8_t *)(*info),
                                                                             (*info)->_reloKind, self()),
                                                                             __FILE__, __LINE__, NULL);
            }
         }

      // If have inlined calls, now add the relocation records in descending order of inlined site index (at relocation time, the order is reverse)
      if (inlinedCallSize > 0)
         {
         counter= inlinedCallSize - 1;
         int numSitesAdded = 0;
         for (; counter >= 0 ; counter--)
            {
            TR_InlinedSiteLinkedListEntry *currentSite = orderedInlinedSiteListTable[counter].first;
            if (!currentSite)
               missedSite = counter;

            while (currentSite)
               {
               self()->addAOTRelocation(new (self()->trHeapMemory()) TR::ExternalRelocation(currentSite->location,
                                                                                currentSite->destination,
                                                                                currentSite->guard,
                                                                                currentSite->reloType, self()),
                               __FILE__,__LINE__, NULL);
               currentSite = currentSite->next;
               numSitesAdded++;
               }
            }
         }
//#endif
      // Now call the platform specific processing of relocations
      self()->getAheadOfTimeCompile()->processRelocations();
      }

   for (auto aotIterator = self()->getAOTRelocationList().begin(); aotIterator != self()->getAOTRelocationList().end(); ++aotIterator)
      {
      // Traverse the AOT/external labels
	  (*aotIterator)->apply(self());
      }
   }

void J9::CodeGenerator::addProjectSpecializedRelocation(uint8_t *location, uint8_t *target, uint8_t *target2,
      TR_ExternalRelocationTargetKind kind, char *generatingFileName, uintptr_t generatingLineNumber, TR::Node *node)
   {
   (target2 == NULL) ?
         self()->addAOTRelocation(new (self()->trHeapMemory()) TR::ExternalRelocation(location, target, kind, self()),
               generatingFileName, generatingLineNumber, node) :
         self()->addAOTRelocation(new (self()->trHeapMemory()) TR::ExternalRelocation(location, target, target2, kind, self()),
               generatingFileName, generatingLineNumber, node);
   }

void J9::CodeGenerator::addProjectSpecializedRelocation(TR::Instruction *instr, uint8_t *target, uint8_t *target2,
      TR_ExternalRelocationTargetKind kind, char *generatingFileName, uintptr_t generatingLineNumber, TR::Node *node)
   {
   (target2 == NULL) ?
         self()->addAOTRelocation(new (self()->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(instr, target, kind, self()),
               generatingFileName, generatingLineNumber, node) :
         self()->addAOTRelocation(new (self()->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(instr, target, target2, kind, self()),
               generatingFileName, generatingLineNumber, node);
   }

void J9::CodeGenerator::addProjectSpecializedPairRelocation(uint8_t *location, uint8_t *location2, uint8_t *target,
      TR_ExternalRelocationTargetKind kind, char *generatingFileName, uintptr_t generatingLineNumber, TR::Node *node)
   {
   self()->addAOTRelocation(new (self()->trHeapMemory()) TR::ExternalOrderedPair32BitRelocation(location, location2, target, kind, self()),
         generatingFileName, generatingLineNumber, node);
   }

void
J9::CodeGenerator::jitAddUnresolvedAddressMaterializationToPatchOnClassRedefinition(void *firstInstruction)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(self()->fe());
   if (self()->comp()->compileRelocatableCode())
      {
      self()->addAOTRelocation(new (self()->trHeapMemory()) TR::ExternalRelocation((uint8_t *)firstInstruction, 0, TR_HCR, self()),
                                 __FILE__,__LINE__, NULL);
      }
   else
      {
      createClassRedefinitionPicSite((void*)-1, firstInstruction, 1 /* see OMR::RuntimeAssumption::isForAddressMaterializationSequence */, true, self()->comp()->getMetadataAssumptionList());
      self()->comp()->setHasClassRedefinitionAssumptions();
      }
   }


// J9
//
void
J9::CodeGenerator::compressedReferenceRematerialization()
   {
   TR::TreeTop * tt;
   TR::Node *node;
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(self()->fe());

   static bool disableRematforCP = feGetEnv("TR_DisableWrtBarOpt") != NULL;

   if (TR::Compiler->om.shouldGenerateReadBarriersForFieldLoads())
      {
      // We need this restriction because the compressedrefs remat opt
      // removes decompression/compression sequences from loads/stores where there doesn't exist
      // a gc point between the load and the store, and the load doesn't need to be dereferenced.
      // In Guarded Storage, we can't not do a guarded load because the object that is loaded may
      // not be in the root set, and as a consequence, may get moved.
      disableRematforCP = true;
      }

   // no need to rematerialize for lowMemHeap
   //
   if (self()->comp()->useCompressedPointers() &&
         ((TR::Compiler->vm.heapBaseAddress() != 0) ||
         (TR::Compiler->om.compressedReferenceShift() != 0)) &&
         !disableRematforCP)
      {
      self()->comp()->dumpMethodTrees("Trees before this remat phase", self()->comp()->getMethodSymbol());

      List<TR::Node> rematerializedNodes(self()->trMemory());
      vcount_t visitCount = self()->comp()->incVisitCount();
      TR::SymbolReference *autoSymRef = NULL;
      for (tt = self()->comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
         {
         node = tt->getNode();
         if (node->getOpCodeValue() == TR::BBStart && !node->getBlock()->isExtensionOfPreviousBlock())
            {
            _compressedRefs.clear();

            ListIterator<TR::Node> nodesIt(&rematerializedNodes);
            for (TR::Node * rematNode = nodesIt.getFirst(); rematNode != NULL; rematNode = nodesIt.getNext())
               {
               if (rematNode->getReferenceCount() == 0)
                  rematNode->getFirstChild()->recursivelyDecReferenceCount();
               }

            rematerializedNodes.deleteAll();
            }

        bool alreadyVisitedFirstChild = false;
        if ((node->getOpCodeValue() == TR::compressedRefs) &&
            (node->getFirstChild()->getOpCodeValue() == TR::l2a))
           {
           if (node->getFirstChild()->getVisitCount() == visitCount)
              alreadyVisitedFirstChild = true;
           _compressedRefs.push_front(node->getFirstChild());
           }

         self()->rematerializeCompressedRefs(autoSymRef, tt, NULL, -1, node, visitCount, &rematerializedNodes);

         if ((node->getOpCodeValue() == TR::compressedRefs) &&
             (node->getFirstChild()->getOpCodeValue() == TR::l2a))
            {
            TR::TreeTop *prevTree = tt->getPrevTreeTop();
            TR::TreeTop *nextTree = tt->getNextTreeTop();
            if (node->getFirstChild()->getReferenceCount() > 1)
               {
               if (!alreadyVisitedFirstChild)
                  {
                  if (!rematerializedNodes.find(node->getFirstChild()))
                     {
                     ////traceMsg(comp(), "Adding %p\n", node->getFirstChild());
                     rematerializedNodes.add(node->getFirstChild());
                     }
                  node->getFirstChild()->setVisitCount(visitCount-1);
                  }


               if (rematerializedNodes.find(node->getFirstChild()))
                  {
                  TR::Node *cursorNode = node->getFirstChild()->getFirstChild();
                  while (cursorNode &&
                         (cursorNode->getOpCodeValue() != TR::iu2l))
                     cursorNode = cursorNode->getFirstChild();

                  TR::Node *ttNode = TR::Node::create(TR::treetop, 1, cursorNode);

                  ///traceMsg(comp(), "5 ttNode %p\n", ttNode);
                  TR::TreeTop *treeTop = TR::TreeTop::create(self()->comp(), ttNode);
                  TR::TreeTop *prevTreeTop = tt->getPrevTreeTop();
                  prevTreeTop->join(treeTop);
                  treeTop->join(tt);
                  prevTree = treeTop;
                  }
               }

            node->getFirstChild()->recursivelyDecReferenceCount();
            node->getSecondChild()->recursivelyDecReferenceCount();
            prevTree->join(nextTree);
            }

         if (node->canGCandReturn())
            {
            ListIterator<TR::Node> nodesIt(&rematerializedNodes);
            for (TR::Node * rematNode = nodesIt.getFirst(); rematNode != NULL; rematNode = nodesIt.getNext())
               {
               if (rematNode->getVisitCount() != visitCount)
                  {
                  rematNode->setVisitCount(visitCount);
                  TR::Node *ttNode = TR::Node::create(TR::treetop, 1, rematNode);

                  ///traceMsg(comp(), "5 ttNode %p\n", ttNode);
                  TR::TreeTop *treeTop = TR::TreeTop::create(self()->comp(), ttNode);
                  TR::TreeTop *prevTree = tt->getPrevTreeTop();
                  prevTree->join(treeTop);
                  treeTop->join(tt);
                  }
               }
            rematerializedNodes.deleteAll();
            }
         }

      self()->comp()->dumpMethodTrees("Trees after this remat phase", self()->comp()->getMethodSymbol());

      if (self()->shouldYankCompressedRefs())
         {
         visitCount = self()->comp()->incVisitCount();
         vcount_t secondVisitCount = self()->comp()->incVisitCount();
         TR::TreeTop *nextTree = NULL;
         for (tt = self()->comp()->getStartTree(); tt; tt = nextTree)
            {
            node = tt->getNode();
            nextTree = tt->getNextTreeTop();
            self()->yankCompressedRefs(tt, NULL, -1, node, visitCount, secondVisitCount);
            }

         self()->comp()->dumpMethodTrees("Trees after this yank phase", self()->comp()->getMethodSymbol());
         }
      }

   if (self()->comp()->useCompressedPointers() &&
         (TR::Compiler->vm.heapBaseAddress() == 0) &&
         !disableRematforCP)
      {
      for (tt = self()->comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
         {
         node = tt->getNode();

         if ((node->getOpCodeValue() == TR::compressedRefs) &&
            (node->getFirstChild()->getOpCodeValue() == TR::l2a))
            {
            TR::TreeTop *prevTree = tt->getPrevTreeTop();
            TR::TreeTop *nextTree = tt->getNextTreeTop();

            if (nextTree->getNode()->getOpCode().isNullCheck())
               {
               TR::Node *firstChild = nextTree->getNode()->getFirstChild();
               TR::Node *reference = NULL;
               if (firstChild->getOpCodeValue() == TR::l2a)
                  {
                  TR::ILOpCodes loadOp = self()->comp()->il.opCodeForIndirectLoad(TR::Int32);
                  while (firstChild->getOpCodeValue() != loadOp)
                     firstChild = firstChild->getFirstChild();
                  reference = firstChild->getFirstChild();
                  }
               else
                  reference = nextTree->getNode()->getNullCheckReference();

               if (reference == node->getFirstChild())
                  {
                  node->getFirstChild()->recursivelyDecReferenceCount();
                  node->getSecondChild()->recursivelyDecReferenceCount();
                  prevTree->join(nextTree);
                  }
               }
            }
         }
      }

   }



void
J9::CodeGenerator::rematerializeCompressedRefs(
      TR::SymbolReference * & autoSymRef,
      TR::TreeTop *tt,
      TR::Node *parent,
      int32_t childNum,
      TR::Node *node,
      vcount_t visitCount,
      List<TR::Node> *rematerializedNodes)
   {
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   bool alreadyVisitedNullCheckReference = false;
   bool alreadyVisitedReferenceInNullTest = false;
   bool alreadyVisitedReferenceInStore = false;

   bool isLowMemHeap = (TR::Compiler->vm.heapBaseAddress() == 0);

   TR::Node *reference = NULL;
   TR::Node *address = NULL;

   if (node->getOpCode().isNullCheck())
      {
      // check for either
      // a) HB!=0
      //    l2a
      //      ladd (compressionSequence)
      // b) HB=0, shifted offsets
      //    l2a
      //      lshl
      //
      if ((node->getFirstChild()->getOpCodeValue() == TR::l2a) &&
          (((node->getFirstChild()->getFirstChild()->getOpCodeValue() == TR::ladd) &&
          node->getFirstChild()->getFirstChild()->containsCompressionSequence()) ||
           (node->getFirstChild()->getFirstChild()->getOpCodeValue() == TR::lshl)))
         {
         TR::ILOpCodes loadOp = self()->comp()->il.opCodeForIndirectLoad(TR::Int32);
         TR::Node *n = node->getFirstChild();
         while (n->getOpCodeValue() != loadOp)
            n = n->getFirstChild();
         reference = n->getFirstChild();
         }
      else
         {
         reference = node->getNullCheckReference();
         }

      if (reference->getVisitCount() == visitCount)
         alreadyVisitedNullCheckReference = true;
      }

   if ((node->getOpCodeValue() == TR::ifacmpeq) ||
       (node->getOpCodeValue() == TR::ifacmpne))
      {
      TR::Node *cmpValue = node->getFirstChild();
      if (cmpValue->getVisitCount() == visitCount)
        alreadyVisitedReferenceInNullTest = true;
      }

   if (node->getOpCode().isStoreIndirect())
      {
      // check for either
      // a) HB!=0
      //    l2a
      //      lsub (compressionSequence)
      // b) HB=0, shifted offsets
      //    l2a
      //      lshr
      //
      bool isCompressed = false;
      if ((node->getSecondChild()->getOpCodeValue() == TR::l2i) &&
          (((node->getSecondChild()->getFirstChild()->getOpCodeValue() == TR::lsub) ||
            (node->getSecondChild()->getFirstChild()->getOpCodeValue() == TR::lushr)) &&
     node->getSecondChild()->getFirstChild()->containsCompressionSequence()))
         {
         TR::Node *n = node->getSecondChild()->getFirstChild();
         while (n->getOpCodeValue() != TR::a2l)
            n = n->getFirstChild();
         address = n->getFirstChild();
         isCompressed = true;
         }

      if (address && (address->getVisitCount() == visitCount))
         alreadyVisitedReferenceInStore = true;

      // check for loads that have occurred before this store
      // if so, anchor the load right before the store
      //
      self()->anchorRematNodesIfNeeded(node, tt, rematerializedNodes);
      }
   else if ((node->getOpCodeValue() == TR::arraycopy) || (node->getOpCodeValue() == TR::arrayset))
      {
      self()->anchorRematNodesIfNeeded(node, tt, rematerializedNodes);
      }

   if (node->getOpCodeValue() == TR::l2a)
      {
      rematerializedNodes->remove(node);
      }

   if ((node->getOpCodeValue() == TR::l2a) &&
      ((node->getFirstChild()->getOpCodeValue() == TR::ladd &&
       node->getFirstChild()->containsCompressionSequence()) ||
       ((node->getFirstChild()->getOpCodeValue() == TR::lshl) &&
        (self()->canFoldLargeOffsetInAddressing() || (TR::Compiler->vm.heapBaseAddress() == 0)) &&
        self()->isAddressScaleIndexSupported((1 << TR::Compiler->om.compressedReferenceShiftOffset())))))
      {
      if (parent &&
          (node->getReferenceCount() > 1) &&
          ((parent->getOpCode().isStoreIndirect() && (childNum == 0)) ||
           parent->getOpCode().isLoadVar() ||
           (self()->getSupportsConstantOffsetInAddressing() && parent->getOpCode().isArrayRef() &&
            (self()->canFoldLargeOffsetInAddressing() || parent->getSecondChild()->getOpCode().isLoadConst()))) &&
          performTransformation(self()->comp(), "%sRematerializing node %p(%s) in decompression sequence\n", OPT_DETAILS, node, node->getOpCode().getName()))
         {
         if ((node->getReferenceCount() > 1) &&
             !rematerializedNodes->find(node))
            {
            rematerializedNodes->add(node);
            }

         TR::Node *dupNode= NULL;//
         TR::Node *cursorNode = node;
         TR::Node *cursorParent = parent;
         int32_t cursorChildNum = childNum;
         while (cursorNode &&
                ((cursorNode->getOpCodeValue() != TR::iu2l) ||
                 (cursorNode->getFirstChild()->getOpCodeValue() != TR::iloadi)))
            {
            TR::Node *copyCursorNode = TR::Node::copy(cursorNode);
            copyCursorNode->setReferenceCount(0);
            if (cursorNode == node)
               dupNode = copyCursorNode;

            for (int32_t j = 0; j < cursorNode->getNumChildren(); ++j)
               {
               TR::Node *cursorChild = cursorNode->getChild(j);
               copyCursorNode->setAndIncChild(j, cursorChild);
               }

            cursorParent->setAndIncChild(cursorChildNum, copyCursorNode);
            cursorNode->decReferenceCount();

            cursorParent = cursorNode;
            cursorChildNum = 0;
            cursorNode = cursorNode->getFirstChild();
            }

         node->setVisitCount(visitCount-1);
         dupNode->setVisitCount(visitCount);
         node = dupNode;
         }
      else
         {
         if (node->getReferenceCount() > 1)
            {
            // on x86, prevent remat of the l2a again thereby allowing
            // nodes to use the result of the add already done
            //
            if (!self()->canFoldLargeOffsetInAddressing())
               {
               if (!rematerializedNodes->find(node))
                  rematerializedNodes->add(node);
               node->setVisitCount(visitCount-1);
               }
            }
         else
            rematerializedNodes->remove(node);

         if (parent &&
             ((parent->getOpCode().isArrayRef() &&
               !self()->canFoldLargeOffsetInAddressing() &&
               !parent->getSecondChild()->getOpCode().isLoadConst()) ||
               !self()->getSupportsConstantOffsetInAddressing()) &&
              performTransformation(self()->comp(), "%sYanking %p(%s) in decompression sequence\n", OPT_DETAILS, node, node->getOpCode().getName()))
            {
            if ((node->getOpCodeValue() == TR::l2a) &&
                (node->getFirstChild()->getOpCodeValue() == TR::ladd))
               {
               TR::TreeTop *cursorTree = tt;
               while (cursorTree)
                  {
                  bool addTree = false;
                  TR::Node *cursorNode = cursorTree->getNode();
                  if (cursorNode->getOpCodeValue() == TR::NULLCHK)
                     {
                     TR::Node *nullchkRef = cursorNode->getNullCheckReference();
                     if ((nullchkRef->getOpCodeValue() == TR::l2a) &&
                         (nullchkRef->getFirstChild() == node->getFirstChild()->getFirstChild()))
                        {
                        addTree = true;
                        }
                     }

                  if (!addTree && (cursorNode->getOpCodeValue() == TR::treetop) &&
                      (cursorNode->getFirstChild() == node->getFirstChild()->getFirstChild()))
                     {
                     addTree = true;
                     }

                  if (addTree)
                     {
                     TR::Node *ttNode = TR::Node::create(TR::treetop, 1, node);

                     if (self()->comp()->getOption(TR_TraceCG))
                        traceMsg(self()->comp(), "Placing treetop %p (to hide delay) after tree %p for l2a %p\n", ttNode, cursorNode, node);

                     TR::TreeTop *treeTop = TR::TreeTop::create(self()->comp(), ttNode);
                     TR::TreeTop *nextTT = cursorTree->getNextTreeTop();
                     cursorTree->join(treeTop);
                     treeTop->join(nextTT);
                     break;
                     }
                  else
                     {
                     if ((cursorNode->getOpCodeValue() == TR::BBStart) &&
                         (!cursorNode->getBlock()->isExtensionOfPreviousBlock()))
                        break;
                     }

                  cursorTree = cursorTree->getPrevTreeTop();
                  }
               }
            }
         }
      }

   for (int32_t i = 0; i < node->getNumChildren(); ++i)
      {
      TR::Node *child = node->getChild(i);
      self()->rematerializeCompressedRefs(autoSymRef, tt, node, i, child, visitCount, rematerializedNodes);
      }

   if (node->getOpCode().isNullCheck() && reference &&
          (!isLowMemHeap || self()->performsChecksExplicitly() || (node->getFirstChild()->getOpCodeValue() == TR::PassThrough)) &&
          ((node->getFirstChild()->getOpCodeValue() == TR::l2a) ||
           (reference->getOpCodeValue() == TR::l2a)) &&
         performTransformation(self()->comp(), "%sTransforming null check reference %p in null check node %p to be checked explicitly\n", OPT_DETAILS, reference, node))
      {
      if (node->getFirstChild()->getOpCodeValue() != TR::PassThrough)
         {
         TR::Node *immChild = node->getFirstChild();
         TR::Node *ttNode = NULL;
         bool addedToList = false;
         if (node->getOpCode().isResolveCheck())
            {
            ttNode = TR::Node::createWithSymRef(TR::ResolveCHK, 1, 1, immChild, node->getSymbolReference());
            TR::Node::recreate(node, TR::NULLCHK);
            }
         else
            {
            if (immChild->getOpCodeValue() == TR::l2a)
               {
               if ((immChild->getReferenceCount() > 1) &&
                   !rematerializedNodes->find(immChild))
                  {
                  rematerializedNodes->add(immChild);
                  addedToList = true;
                  }

               immChild->setVisitCount(visitCount-1);
               TR::Node *anchorNode = TR::Node::create(TR::treetop, 1, immChild->getFirstChild()->getFirstChild());
               TR::TreeTop *anchorTree = TR::TreeTop::create(self()->comp(), anchorNode);
               immChild->getFirstChild()->getFirstChild()->setVisitCount(visitCount-1);
               TR::TreeTop *nextTT = tt->getNextTreeTop();
               tt->join(anchorTree);
               anchorTree->join(nextTT);

               TR::Node *n = immChild->getFirstChild();
                  {
                  while ((n != reference) &&
                        (n->getOpCodeValue() != TR::l2a))
                     {
                     n->setVisitCount(visitCount-1);
                     n = n->getFirstChild();
                     }
                  }
               }
            else
               ttNode = TR::Node::create(TR::treetop, 1, immChild);
            }

         if (ttNode)
            {
            TR::TreeTop *treeTop = TR::TreeTop::create(self()->comp(), ttNode);
            immChild->setVisitCount(visitCount-1);
            TR::TreeTop *nextTT = tt->getNextTreeTop();
            tt->join(treeTop);
            treeTop->join(nextTT);
            }

         TR::Node *passThroughNode = TR::Node::create(TR::PassThrough, 1, reference);
         passThroughNode->setVisitCount(visitCount);
         node->setAndIncChild(0, passThroughNode);
         if (ttNode || !addedToList)
            immChild->recursivelyDecReferenceCount();
         else
            immChild->decReferenceCount();
         }

      if ((reference->getOpCodeValue() == TR::l2a) &&
          (!alreadyVisitedNullCheckReference || (reference->getReferenceCount() == 1)) &&
          (((reference->getFirstChild()->getOpCodeValue() == TR::ladd) &&
          reference->getFirstChild()->containsCompressionSequence()) ||
           reference->getFirstChild()->getOpCodeValue() == TR::lshl) &&
          performTransformation(self()->comp(), "%sStrength reducing null check reference %p in null check node %p \n", OPT_DETAILS, reference, node))
          {
          bool addedToList = false;
          if (node->getFirstChild()->getOpCodeValue() == TR::PassThrough)
             {
             if ((reference->getReferenceCount() > 1) &&
                 !rematerializedNodes->find(reference))
                {
                rematerializedNodes->add(reference);
                addedToList = true;
                }

            TR::Node *passThroughNode = node->getFirstChild();
            TR::Node *grandChild = reference->getFirstChild()->getFirstChild();
            TR::Node *l2aNode = TR::Node::create(TR::l2a, 1, grandChild);
            if (reference->isNonNull())
               l2aNode->setIsNonNull(true);
            else if (reference->isNull())
               l2aNode->setIsNull(true);
            passThroughNode->setAndIncChild(0, l2aNode);
            if (addedToList)
               reference->decReferenceCount();
            else
               reference->recursivelyDecReferenceCount();
            reference->setVisitCount(visitCount-1);
            }
         }
      }

   if ((node->getOpCodeValue() == TR::ifacmpeq) ||
       (node->getOpCodeValue() == TR::ifacmpne))
      {
      TR::Node *reference = node->getFirstChild();
      TR::Node *secondChild = node->getSecondChild();

      if ((reference->getOpCodeValue() == TR::l2a) &&
          (!alreadyVisitedReferenceInNullTest || (reference->getReferenceCount() == 1)) &&
          (((reference->getFirstChild()->getOpCodeValue() == TR::ladd) &&
          reference->getFirstChild()->containsCompressionSequence())||
           reference->getFirstChild()->getOpCodeValue() == TR::lshl))
          {
          if ((secondChild->getOpCodeValue() == TR::aconst) &&
              (secondChild->getAddress() == 0) &&
              performTransformation(self()->comp(), "%sTransforming reference %p in null comparison node %p \n", OPT_DETAILS, reference, node))
             {
             bool addedToList = false;
             if ((reference->getReferenceCount() > 1) &&
                 !rematerializedNodes->find(reference))
                {
                rematerializedNodes->add(reference);
                addedToList = true;
                }

             TR::Node *compressedValue = reference->getFirstChild()->getFirstChild();
             TR::Node *l2aNode = TR::Node::create(TR::l2a, 1, compressedValue);
             if (reference->isNonNull())
                l2aNode->setIsNonNull(true);
             else if (reference->isNull())
                l2aNode->setIsNull(true);

             node->setAndIncChild(0, l2aNode);
             if (addedToList)
                reference->decReferenceCount();
             else
                reference->recursivelyDecReferenceCount();
             reference->setVisitCount(visitCount-1);
             }
          }
      }

   if (self()->materializesHeapBase() &&
       !isLowMemHeap &&
       parent && (!parent->getOpCode().isStore()) &&
       (node->getOpCodeValue() == TR::lconst) &&
       (node->getLongInt() == TR::Compiler->vm.heapBaseAddress()) &&
       performTransformation(self()->comp(), "%sTransforming heap base constant node %p to auto load \n", OPT_DETAILS, node))
      {
      if (!autoSymRef)
         {
         autoSymRef = self()->comp()->getSymRefTab()->createTemporary(self()->comp()->getMethodSymbol(), node->getDataType());
         TR::TreeTop *startTree = self()->comp()->getStartTree();
         TR::TreeTop *nextTree = startTree->getNextTreeTop();

         TR::Node *lconstNode = TR::Node::create(node, TR::lconst, 0, 0);
         lconstNode->setLongInt(node->getLongInt());
         TR::Node *storeNode = TR::Node::createWithSymRef(TR::lstore, 1, 1, lconstNode, autoSymRef);
         TR::TreeTop *tt = TR::TreeTop::create(self()->comp(), storeNode);
         startTree->join(tt);
         tt->join(nextTree);
         }

      TR::Node::recreate(node, TR::lload);
      node->setSymbolReference(autoSymRef);
      }


   if (address && node->getOpCode().isStoreIndirect())
      {
      if (address->getOpCodeValue() == TR::l2a && (address->getReferenceCount() == 1 || !alreadyVisitedReferenceInStore) &&
         ((address->getFirstChild()->getOpCodeValue() == TR::ladd && address->getFirstChild()->containsCompressionSequence()) ||
           address->getFirstChild()->getOpCodeValue() == TR::lshl))
         {
         // Check for write barriers that we can skip and which are not underneath an ArrayStoreCHK. In these cases we are safe
         // to optimize the write barrier to a simple store, thus avoiding the need to compress / uncompress the pointer.
         if (node->getOpCode().isWrtBar() && node->skipWrtBar())
            {
            // This check is overly conservative to ensure functional correctness.
            bool isPossiblyUnderArrayStoreCheck = tt->getNode()->getOpCodeValue() == TR::ArrayStoreCHK || (node->getReferenceCount() > 1 && !tt->getNode()->getOpCode().isResolveCheck());

            if (!isPossiblyUnderArrayStoreCheck && performTransformation(self()->comp(), "%sStoring compressed pointer [%p] directly into %p in tree %p\n", OPT_DETAILS, address, node, tt->getNode()))
               {
               bool addedToList = false;
               if ((address->getReferenceCount() > 1) && !rematerializedNodes->find(address))
                  {
                  rematerializedNodes->add(address);
                  addedToList = true;
                  }

               TR::Node *l2iNode = NULL;
               TR::ILOpCodes loadOp = self()->comp()->il.opCodeForIndirectLoad(TR::Int32);
               TR::Node *n = address;
               while (n->getOpCodeValue() != loadOp)
                  n = n->getFirstChild();
               l2iNode = n;

               if (node->getOpCode().isWrtBar())
                  {
                  int32_t lastChildNum = node->getNumChildren()-1;
                  node->getChild(lastChildNum)->recursivelyDecReferenceCount();
                  node->setNumChildren(lastChildNum);
                  }

               TR::Node::recreate(node, self()->comp()->il.opCodeForIndirectStore(TR::Int32));

               TR::Node *immChild = node->getSecondChild();
               node->setAndIncChild(1, l2iNode);

               address->incReferenceCount();
               immChild->recursivelyDecReferenceCount();

               if (addedToList)
                  address->decReferenceCount();
               else
                  address->recursivelyDecReferenceCount();

               address->setVisitCount(visitCount-1);
               }
            }
         }
      }
   }


void
J9::CodeGenerator::yankCompressedRefs(
      TR::TreeTop *tt,
      TR::Node *parent,
      int32_t childNum,
      TR::Node *node,
      vcount_t visitCount,
      vcount_t secondVisitCount)
   {
   if (node->getVisitCount() >= visitCount)
      return;

   node->setVisitCount(visitCount);

   for (int32_t i = 0; i < node->getNumChildren(); ++i)
      {
      TR::Node *child = node->getChild(i);
      self()->yankCompressedRefs(tt, node, i, child, visitCount, secondVisitCount);
      }

   if (parent &&
       (parent->getOpCodeValue() == TR::treetop) &&
       (node->getOpCodeValue() == TR::l2a) &&
       (node->getFirstChild()->getOpCodeValue() == TR::ladd &&
        node->getFirstChild()->containsCompressionSequence()))
      {

      //printf("Looking at node %p in %s\n", node, comp()->signature()); fflush(stdout);
      TR::TreeTop *firstTree = tt;
      TR::TreeTop *lastTree = tt;
      bool nullCheckTree = false;
      bool exprNeedsChecking = true;
      if ((node->getFirstChild()->getFirstChild()->getOpCodeValue() == TR::iu2l) &&
          (node->getFirstChild()->getFirstChild()->getFirstChild()->getOpCodeValue() == TR::iloadi) &&
          ((node->getFirstChild()->getFirstChild()->getFirstChild()->getFirstChild()->getOpCode().isLoadVarDirect() &&
            node->getFirstChild()->getFirstChild()->getFirstChild()->getFirstChild()->getSymbolReference()->getSymbol()->isAutoOrParm()) ||
           (node->getFirstChild()->getFirstChild()->getFirstChild()->getFirstChild()->getOpCodeValue() == TR::aRegStore)))
         exprNeedsChecking = false;

      TR::TreeTop *prevTree = tt->getPrevTreeTop();
         TR::Node *prevNode = prevTree->getNode();
         if (prevNode->getOpCodeValue() == TR::NULLCHK)
            {
            if (prevNode->getFirstChild()->getOpCodeValue() == TR::PassThrough)
               {
               TR::Node *reference = prevNode->getNullCheckReference();
               if ((reference == node) ||
                   ((reference->getOpCodeValue() == TR::l2a) &&
                    (reference->getFirstChild() == node->getFirstChild()->getFirstChild())))
                  {
                  nullCheckTree = true;
                  firstTree = prevTree;
                  prevTree = prevTree->getPrevTreeTop();
                  prevNode = prevTree->getNode();
                  }
               }
            }

      if ((prevNode->getOpCodeValue() == TR::treetop) &&
          (prevNode->getFirstChild() == node->getFirstChild()->getFirstChild()))
         firstTree = prevTree;
      else
         firstTree = tt;

      if (firstTree != tt)
         {
         TR_BitVector symbolReferencesInNode(self()->comp()->getSymRefCount(), self()->comp()->trMemory(), stackAlloc);

         ////bool canYank = collectSymRefs(node, &symbolReferencesInNode, secondVisitCount);
         // since symRefs need to be collected for each treetop, we'll need a fresh visitCount
         // for every walk of a tree
         //
         bool canYank = self()->collectSymRefs(node, &symbolReferencesInNode, self()->comp()->incVisitCount());

         TR_BitVector intersection(self()->comp()->getSymRefCount(), self()->comp()->trMemory(), stackAlloc);

         //printf("canYank %d node %d in %s\n", canYank, node, comp()->signature()); fflush(stdout);

         if (canYank)
            {
            TR::TreeTop *cursorTree = firstTree->getPrevTreeTop();
            int32_t numTrees = 0;
            while (cursorTree)
              {
              numTrees++;
              TR::Node *cursorNode = cursorTree->getNode();
              //printf("canYank %d node %p cursor %p in %s\n", canYank, node, cursorNode, comp()->signature()); fflush(stdout);
              TR::Node *childNode = NULL;
              if (cursorNode->getNumChildren() > 0)
                 childNode = cursorNode->getFirstChild();

              if (cursorNode && cursorNode->getOpCode().hasSymbolReference() &&
                  (cursorNode->getOpCode().isStore() || cursorNode->getOpCode().isCall()))
                 {
                 if (symbolReferencesInNode.get(cursorNode->getSymbolReference()->getReferenceNumber()))
                    {
                    break;
                    }

                 intersection.empty();
                 cursorNode->getSymbolReference()->getUseDefAliases().getAliasesAndUnionWith(intersection);
                 intersection &= symbolReferencesInNode;
                 if (!intersection.isEmpty())
                    {
                    break;
                    }
                 }

              if (childNode && childNode->getOpCode().hasSymbolReference())
                 {
                 if (childNode && childNode->getOpCode().hasSymbolReference() &&
                     (childNode->getOpCode().isStore() || childNode->getOpCode().isCall()))
                    {
                    if (symbolReferencesInNode.get(childNode->getSymbolReference()->getReferenceNumber()))
                       {
                       break;
                       }

                    intersection.empty();
                    childNode->getSymbolReference()->getUseDefAliases().getAliasesAndUnionWith(intersection);
                    intersection &= symbolReferencesInNode;
                    if (!intersection.isEmpty())
                       {
                       break;
                       }
                    }
                 }

             if (nullCheckTree)
                {
                if (cursorNode->getOpCode().isStore())
                   {
                   if (cursorNode->getSymbol()->isStatic() ||
                       cursorNode->getSymbol()->isShadow() ||
                       !cursorNode->getSymbolReference()->getUseonlyAliases().isZero(self()->comp()))
                      {
                      break;
                      }
                   }
                }

              if (cursorNode->exceptionsRaised())
                 {
                 if (nullCheckTree || exprNeedsChecking)
                    break;
                 }

              if (cursorNode->getOpCodeValue() == TR::BBStart)
                 {
                 break;
                 }

              cursorTree = cursorTree->getPrevTreeTop();
              }

            if (cursorTree != firstTree->getPrevTreeTop())
               {
               /////printf("Yanking l2a node %p past %d trees in %s\n", node, numTrees, comp()->signature()); fflush(stdout);
               TR::TreeTop *nextTree = cursorTree->getNextTreeTop();
               TR::TreeTop *prevTreeAtSrc = firstTree->getPrevTreeTop();
               TR::TreeTop *nextTreeAtSrc = lastTree->getNextTreeTop();
               prevTreeAtSrc->join(nextTreeAtSrc);
               cursorTree->join(firstTree);
               lastTree->join(nextTree);
               }
            }
         }
      }
   }


void
J9::CodeGenerator::anchorRematNodesIfNeeded(
      TR::Node *node,
      TR::TreeTop *tt,
      List<TR::Node> *rematerializedNodes)
   {
   TR::SymbolReference *symRef = node->getSymbolReference();
   TR::SparseBitVector aliases(self()->comp()->allocator());
   if (symRef->sharesSymbol())
      symRef->getUseDefAliases().getAliases(aliases);

   ListIterator<TR::Node> nodesIt(rematerializedNodes);
   for (TR::Node * rematNode = nodesIt.getFirst(); rematNode != NULL; rematNode = nodesIt.getNext())
      {
      if (rematNode->getOpCodeValue() == TR::l2a)
         {
         TR::Node *load = rematNode->getFirstChild();
         while (load->getOpCodeValue() != TR::iu2l)
            load = load->getFirstChild();
         load = load->getFirstChild();
         if (load->getOpCode().isLoadIndirect() &&
               ((load->getSymbolReference() == node->getSymbolReference()) ||
                  (aliases.ValueAt(load->getSymbolReference()->getReferenceNumber()))))
            {
            rematerializedNodes->remove(rematNode);
            rematNode->setVisitCount(self()->comp()->getVisitCount());
            if (self()->comp()->getOption(TR_TraceCG))
               {
               if (node->getOpCode().isStoreIndirect())
                  traceMsg(self()->comp(), "Found previous load %p same as store %p, anchoring load\n", load, node);
               else
                  traceMsg(self()->comp(), "Found previous load %p aliases with node %p, anchoring load\n", load, node);
               }
            TR::Node *ttNode = TR::Node::create(TR::treetop, 1, rematNode);
            TR::TreeTop *treeTop = TR::TreeTop::create(self()->comp(), ttNode);
            TR::TreeTop *prevTree = tt->getPrevTreeTop();
            prevTree->join(treeTop);
            treeTop->join(tt);
            }
         }
      }
   }


 /**
 * Insert asyncCheck's before method returns.  Without this, methods
 * with no loops or calls will never be sa/mpled, and will be stuck
 * forever at their initial opt-level.  (Important for mpegaudio,
 * which has some large, warm methods with no loops or calls).
 */
void J9::CodeGenerator::insertEpilogueYieldPoints()
   {
   // Look for all returns, and insert async check before them
   TR::CFG * cfg = self()->comp()->getFlowGraph();

   for (TR::TreeTop * treeTop = self()->comp()->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {

      TR::Node * node = treeTop->getNode();
      TR::ILOpCodes opCode = node->getOpCodeValue();

      if (opCode == TR::BBStart)
         {
         TR::Block * block = node->getBlock();

         TR::TreeTop * tt1 = block->getLastRealTreeTop();
         TR::Node * node1 = tt1->getNode();

         if (node1->getOpCode().isReturn())
            {
            TR::TreeTop *prevTT = tt1->getPrevTreeTop();
            if (node1->getNumChildren()>0)
               {
               //anchor the return value
               TR::Node *ttNode = TR::Node::create(TR::treetop, 1, node1->getFirstChild());
               TR::TreeTop *anchorTree = TR::TreeTop::create(self()->comp(), ttNode);
               prevTT->join(anchorTree);
               anchorTree->join(tt1);
               prevTT = anchorTree;
               }

            TR::Node *asyncNode = TR::Node::createWithSymRef(node, TR::asynccheck, 0,
                                                 self()->comp()->getSymRefTab()->findOrCreateAsyncCheckSymbolRef(self()->comp()->getMethodSymbol()));
            TR::TreeTop *asyncTree = TR::TreeTop::create(self()->comp(), asyncNode);
            prevTT->join(asyncTree);
            asyncTree->join(tt1);
            treeTop = tt1->getNextTreeTop();
#if 0
            // Asynccheck's need to be at the beginning of blocks
            TR::Block * returnBlock = block->split(tt1, cfg);
            treeTop = tt1->getNextTreeTop();
            TR::Node *asyncNode = TR::Node::createWithSymRef(node, TR::asynccheck, 1, 0,
                                                 comp()->getSymRefTab()->findOrCreateAsyncCheckSymbolRef(comp()->getMethodSymbol()));
            TR::TreeTop *asyncTree = TR::TreeTop::create(comp(), asyncNode);

            returnBlock->prepend(asyncTree);
#endif
            }
         }
      }
   }


TR::TreeTop *
J9::CodeGenerator::genSymRefStoreToArray(
      TR::Node* refNode,
      TR::Node* arrayAddressNode,
      TR::Node* firstOffset,
      TR::Node* loadNode,
      int32_t secondOffset,
      TR::TreeTop* insertionPoint)
   {
   TR::Node* offsetNode;

   if (firstOffset)
      offsetNode = TR::Node::create(TR::iadd, 2,
         firstOffset,
         TR::Node::create(refNode, TR::iconst, 0, secondOffset));
   else
      offsetNode = TR::Node::create(refNode, TR::iconst, 0, secondOffset);

   if (TR::Compiler->target.is64Bit())
      {
      offsetNode = TR::Node::create(TR::i2l, 1, offsetNode);
      }

   TR::Node* addrNode = TR::Node::create(TR::Compiler->target.is64Bit()?TR::aladd:TR::aiadd,
      2, arrayAddressNode, offsetNode);
   TR::Node* storeNode =
      TR::Node::createWithSymRef(self()->comp()->il.opCodeForIndirectStore(loadNode->getDataType()), 2, 2,
                      addrNode, loadNode,
                      self()->symRefTab()->findOrCreateGenericIntShadowSymbolReference(0));
   TR::TreeTop* storeTreeTop = TR::TreeTop::create(self()->comp(), storeNode);
   insertionPoint->insertTreeTopsAfterMe(storeTreeTop);
   return storeTreeTop;
   }


// J9, x86?
//
void
J9::CodeGenerator::detectEndOfVMThreadGlobalRegisterLiveRange(TR::Block *b)
   {
   if (b != b->startOfExtendedBlock()) // Skip extensions
       return;

   TR::Node * glRegDeps = NULL;
   TR::CFGEdge *relevantEdge = NULL;
   bool vmThreadLiveOnEntry = false;
   TR::TreeTop * startTT = b->getEntry();
   TR::Node * startNode = startTT->getNode();

   if (startNode->getNumChildren() > 0)
      {
      glRegDeps = startNode->getFirstChild();
      TR_ASSERT(glRegDeps->getOpCodeValue() == TR::GlRegDeps, "expected TR::GlRegDeps");
      for (int32_t i = glRegDeps->getNumChildren() - 1; i >= 0; --i)
         {
         TR::Node * dep = glRegDeps->getChild(i);

         if (dep->getGlobalRegisterNumber() == self()->comp()->cg()->getVMThreadGlobalRegisterNumber())
            {
            vmThreadLiveOnEntry = true;
            break;
            }

         if (dep->getType().isInt64() && TR::Compiler->target.is32Bit())
            {
            if (dep->getHighGlobalRegisterNumber() == self()->comp()->cg()->getVMThreadGlobalRegisterNumber())
               {
               vmThreadLiveOnEntry = true;
               break;
               }
            }
         }
      }

   if (!vmThreadLiveOnEntry)
      {
      if (debug("traceGRA_Split"))
         diagnostic("\nConsidering block_%d for splitting", b->getNumber());

      for (auto e = b->getPredecessors().begin(); e != b->getPredecessors().end();)
         {
         TR::CFGEdge * current = *(e++);
         TR::Block * pred = toBlock(current->getFrom());
         if (pred == self()->comp()->getFlowGraph()->getStart())
            continue;

         bool vmThreadLiveOnEntryInPred = false;
         TR::Block *extendedPred = pred->startOfExtendedBlock();
         TR::TreeTop *predStartTT = extendedPred->getEntry();
         TR::Node *predStartNode = predStartTT->getNode();
         if (predStartNode->getNumChildren() > 0)
            {
            TR::Node * predGlRegDeps = predStartNode->getFirstChild();
            TR_ASSERT(predGlRegDeps->getOpCodeValue() == TR::GlRegDeps, "expected TR::GlRegDeps");
            for (int32_t i = predGlRegDeps->getNumChildren() - 1; i >= 0; --i)
               {
               TR::Node * dep = predGlRegDeps->getChild(i);

               if (dep->getGlobalRegisterNumber() == self()->comp()->cg()->getVMThreadGlobalRegisterNumber())
                  {
                  vmThreadLiveOnEntryInPred = true;
                  break;
                  }

               if (dep->getType().isInt64() && TR::Compiler->target.is32Bit())
                  {
                  if (dep->getHighGlobalRegisterNumber() == self()->comp()->cg()->getVMThreadGlobalRegisterNumber())
                     {
                     vmThreadLiveOnEntryInPred = true;
                     break;
                     }
                  }
               }
            }

         bool vmThreadLiveOnExitInPred = false;

         TR::TreeTop *exitTree = predStartTT->getExtendedBlockExitTreeTop();
         TR::TreeTop *cursorExit;
         TR::Block *cursorBlock = extendedPred;
         do
            {
            TR::CFGEdgeList & cursorSuccessors = cursorBlock->getSuccessors();
            for (auto e = cursorSuccessors.begin(); e != cursorSuccessors.end(); ++e)
               {
               TR::Block *succ = (*e)->getTo()->asBlock();

               TR::TreeTop *succStartTT = succ->getEntry();

               if (succStartTT)
                  {
                  TR::Node *succStartNode = succStartTT->getNode();
                  if (succStartNode->getNumChildren() > 0)
                     {
                     TR::Node * succGlRegDeps = succStartNode->getFirstChild();
                     TR_ASSERT(succGlRegDeps->getOpCodeValue() == TR::GlRegDeps, "expected TR::GlRegDeps");
                     for (int32_t i = succGlRegDeps->getNumChildren() - 1; i >= 0; --i)
                        {
                        TR::Node * dep = succGlRegDeps->getChild(i);

                        if (dep->getGlobalRegisterNumber() == self()->comp()->cg()->getVMThreadGlobalRegisterNumber())
                           {
                           vmThreadLiveOnExitInPred = true;
                           break;
                           }

                        if (dep->getType().isInt64() && TR::Compiler->target.is32Bit())
                           {
                           if (dep->getHighGlobalRegisterNumber() == self()->comp()->cg()->getVMThreadGlobalRegisterNumber())
                              {
                              vmThreadLiveOnExitInPred = true;
                              break;
                              }
                           }
                        }
                     }
                  }
               }

            cursorExit = cursorBlock->getExit();
            cursorBlock = cursorBlock->getNextBlock();
            }
         while (exitTree != cursorExit);


         if (vmThreadLiveOnEntryInPred || vmThreadLiveOnExitInPred)
            {
            relevantEdge = current;
            if (relevantEdge->mustRestoreVMThreadRegister())
               continue;

            if (!(b->getPredecessors().size() == 1) /* && !toBlock(relevantEdge->getFrom())->getSuccessors().size() == 1) */)
               {
               TR::Block *newBlock = pred->splitEdge(pred, b, self()->comp());
               if (debug("traceGRA_Split"))
                  diagnostic("\nSplitting edge, create new intermediate block_%d", newBlock->getNumber());

               newBlock->takeGlRegDeps(self()->comp(), glRegDeps);

               relevantEdge = newBlock->getPredecessors().front();
               }
            relevantEdge->setMustRestoreVMThreadRegister(true);

            if (debug("vmThreadGRA"))
               {
               diagnostic("\nmustRestoreVMThreadRegister to ebp on entry to block_%d on edge from block_%d going to block_%d\n",
                           toBlock(relevantEdge->getTo())->getNumber(), toBlock(relevantEdge->getFrom())->getNumber(), b->getNumber());
               }
            }
         }
      }
   }


bool
J9::CodeGenerator::collectSymRefs(
      TR::Node *node,
      TR_BitVector *symRefs,
      vcount_t visitCount)
   {
   if (node->getVisitCount() >= visitCount)
      return true;

   node->setVisitCount(visitCount);


   if (node->getOpCode().hasSymbolReference())
      {
      if (node->getOpCode().isLoadVar())
         {
         TR::SymbolReference *symRef = node->getSymbolReference();
         symRef->getUseDefAliases().getAliasesAndUnionWith(*symRefs);

         symRefs->set(symRef->getReferenceNumber());
         }
      else
         return false;
      }

   for (int32_t i = 0; i < node->getNumChildren(); ++i)
      {
      TR::Node *child = node->getChild(i);
      if (!self()->collectSymRefs(child, symRefs, visitCount))
         return false;
      }

   return true;
   }


void
J9::CodeGenerator::allocateLinkageRegisters()
   {
   if (self()->comp()->isGPUCompilation())
      return;

   TR::Delimiter d(self()->comp(), self()->comp()->getOptions()->getAnyOption(TR_TraceOptDetails|TR_CountOptTransformations), "AllocateLinkageRegisters");

   if (!self()->prepareForGRA())
      {
      dumpOptDetails(self()->comp(), "  prepareForGRA failed -- giving up\n");
      return;
      }

   TR::Block     *firstBlock         = self()->comp()->getStartBlock();
   const int32_t numParms           = self()->comp()->getMethodSymbol()->getParameterList().getSize();

   if (numParms == 0) return ;

   TR_BitVector  globalRegsWithRegLoad(self()->getNumberOfGlobalRegisters(), self()->comp()->trMemory(), stackAlloc); // indexed by global register number
   TR_BitVector  killedParms(numParms, self()->comp()->trMemory(), stackAlloc); // indexed by parm->getOrdinal()
   TR::Node     **regLoads = (TR::Node**)self()->trMemory()->allocateStackMemory(numParms*sizeof(regLoads[0])); // indexed by parm->getOrdinal() to give the RegLoad for a given parm
   memset(regLoads, 0, numParms*sizeof(regLoads[0]));

   // If the first block is in a loop, then it can be reached by parm stores in other blocks.
   // Conservatively, don't use RegLoads for any parm that is stored anywhere in the method.
   //
   if (firstBlock->getPredecessors().size() > 1)
      {
      // Rather than put regStores in all predecessors, we give up.
      //
      dumpOptDetails(self()->comp(), "  First basic block is in a loop -- giving up\n");
      return;
      }

   // Initialize regLoads and usedGlobalRegs from the RegLoads already present on the BBStart node
   //
   TR::Node *bbStart    = self()->comp()->getStartTree()->getNode();
   TR_ASSERT(bbStart->getOpCodeValue() == TR::BBStart, "assertion failure");
   TR::Node *oldRegDeps = (bbStart->getNumChildren() > 0)? bbStart->getFirstChild() : NULL;
   if (oldRegDeps)
      {
      TR_ASSERT(oldRegDeps->getOpCodeValue() == TR::GlRegDeps, "assertion failure");
      for (uint16_t i=0; i < oldRegDeps->getNumChildren(); i++)
         {
         TR::Node *regLoad = oldRegDeps->getChild(i);
         TR_ASSERT(regLoad->getSymbol() && regLoad->getSymbol()->isParm(), "First basic block can have only parms live on entry");
         dumpOptDetails(self()->comp(), "  Parm %d has RegLoad %s\n", regLoad->getSymbol()->getParmSymbol()->getOrdinal(), self()->comp()->getDebug()->getName(regLoad));
         regLoads[regLoad->getSymbol()->getParmSymbol()->getOrdinal()] = regLoad;
         if (regLoad->getType().isInt64() && TR::Compiler->target.is32Bit() && !self()->use64BitRegsOn32Bit())
            {
            globalRegsWithRegLoad.set(regLoad->getLowGlobalRegisterNumber());
            globalRegsWithRegLoad.set(regLoad->getHighGlobalRegisterNumber());
            }
         else
            {
            globalRegsWithRegLoad.set(regLoad->getGlobalRegisterNumber());
            }
         }
      }
   if (self()->comp()->getOption(TR_TraceOptDetails))
      {
      dumpOptDetails(self()->comp(), "  Initial globalRegsWithRegLoad: ");
      self()->getDebug()->print(self()->comp()->getOptions()->getLogFile(), &globalRegsWithRegLoad);
      dumpOptDetails(self()->comp(), "\n");
      }


   // Recursively replace parm loads with regLoads; create new RegLoads as necessary
   //
   vcount_t visitCount = self()->comp()->incVisitCount();
   int32_t  numRegLoadsAdded = 0;
   for(TR::TreeTop *tt = firstBlock->getFirstRealTreeTop(); tt; tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      if (node->getOpCodeValue() == TR::BBStart && !node->getBlock()->isExtensionOfPreviousBlock())
         break;
      numRegLoadsAdded += self()->changeParmLoadsToRegLoads(node, regLoads, &globalRegsWithRegLoad, killedParms, visitCount);
      if (node->getOpCode().isStoreDirect() && node->getSymbol()->isParm())
         {
         killedParms.set(node->getSymbol()->getParmSymbol()->getOrdinal());
         if (self()->comp()->getOption(TR_TraceOptDetails))
            {
            dumpOptDetails(self()->comp(), "  Found store %s\n  killedParms is now ", self()->comp()->getDebug()->getName(node));
            self()->getDebug()->print(self()->comp()->getOptions()->getLogFile(), &killedParms);
            dumpOptDetails(self()->comp(), "\n");
            }
         }
      }

   // Make sure all RegLoads are present on the BBStart's regdeps
   //
   if (numRegLoadsAdded > 0)
      {
      uint16_t numOldRegDeps = oldRegDeps? oldRegDeps->getNumChildren() : 0;
      uint16_t numNewRegDeps = numOldRegDeps + numRegLoadsAdded;

      // Create GlRegDeps
      //
      TR::Node *newRegDeps = TR::Node::create(bbStart, TR::GlRegDeps, numNewRegDeps);
      uint16_t childNum=0;

      for (int32_t parmNum=0; parmNum < numParms; parmNum++)
         if (regLoads[parmNum])
            newRegDeps->setAndIncChild(childNum++, regLoads[parmNum]);

      // Remove existing regdeps from oldRegDeps
      //
      for (childNum = 0; childNum < numOldRegDeps; childNum++)
         oldRegDeps->getChild(childNum)->decReferenceCount();

      // Stick the new regDeps on bbStart
      //
      bbStart->setAndIncChild(0, newRegDeps);
      bbStart->setNumChildren(1);

      dumpOptDetails(self()->comp(), "  Created new GlRegDeps %s on BBStart %s\n",
         self()->comp()->getDebug()->getName(newRegDeps),
         self()->comp()->getDebug()->getName(bbStart));
      }
   }

#define OPT_DETAILS_CLEAN "O^O CLEAN FOLDING: "
// Having the pdstore also do a clean results in better code as the sign cleaning instruction (on z at least) also moves data.
// It is done late at > noOpt so a side-effect (the cleaning) is not added to the the store operation.
// Having a side-effect like this means several optimizations, such as local and global copy propagation and value numbering,
// need to handle this cleaning side-effect.
// Cannot do this during lowerTrees because the pass in lowerTrees that adds skipCopyOnStore/skipCopyOnLoad flags is sensitive
// to referenceCounts and foldSignCleaningIntoStore is changing the number of node references
//
void
J9::CodeGenerator::foldSignCleaningIntoStore()
   {
   LexicalTimer foldTimer("foldSignCleaning", self()->comp()->phaseTimer());
   for(TR::TreeTop * tt = self()->comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      if (node->getOpCode().isPackedStore() &&
          node->getValueChild()->getOpCode().isSimpleBCDClean() &&
          node->getValueChild()->getDecimalPrecision() >= node->getValueChild()->getFirstChild()->getDecimalPrecision() && // don't lose truncation side effect
          node->getDecimalPrecision() <= TR::DataType::getMaxPackedDecimalPrecision() &&
          performTransformation(self()->comp(), "%sFold %s [%s] into store by setting CleanSignInPDStoreEvaluator flag on %s [%s]\n",
            OPT_DETAILS_CLEAN,node->getValueChild()->getOpCode().getName(),node->getValueChild()->getName(self()->comp()->getDebug()),node->getOpCode().getName(),node->getName(self()->comp()->getDebug())))
         {
         node->setCleanSignInPDStoreEvaluator(true);
         TR::Node *valueChild = node->getValueChild();
         valueChild->getFirstChild()->incReferenceCount();
         valueChild->recursivelyDecReferenceCount();
         valueChild = node->setValueChild(valueChild->getFirstChild());
         self()->swapChildrenIfNeeded(node, OPT_DETAILS_CLEAN);
         }
      }
   }

void
J9::CodeGenerator::swapChildrenIfNeeded(TR::Node *store, char *optDetails)
   {
   TR::Node *valueChild = store->getValueChild();

   // swap children to increase the chances of being able to use location "a" as an accumulator instead of needing a temp copy
   //
   // could also do this for another commutative operation -- like pdmul, but the advantage isn't as clear with multiply as the
   // the relative size of the operands and how the instruction is actually encoded are important factors too for determining the best operand ordering
   // reorder:
   //    pdstore "a"
   //       pdadd
   //          x
   //          pdload "a"
   // to:
   //    pdstore "a"
   //       pdadd
   //          pdload "a"
   //          x
   //
   if (valueChild->getOpCode().isCommutative() && (valueChild->getOpCode().isPackedAdd()))
      {
      if (valueChild->getFirstChild()->getOpCode().isLoadVar() &&
          valueChild->getSecondChild()->getOpCode().isLoadVar() &&
          valueChild->getFirstChild()->getSymbolReference() == valueChild->getSecondChild()->getSymbolReference())
         {
         // avoid continual swapping of this case
         // pdstore "a"
         //    pdadd
         //       pdload "a"
         //       pdload "a"
         }
      else if (valueChild->getSecondChild()->getOpCode().isLoadVar() &&
               (valueChild->getSecondChild()->getSymbolReference() == store->getSymbolReference()) &&
               !self()->comp()->getOption(TR_DisableBCDArithChildOrdering) &&
               performTransformation(self()->comp(), "%s%s valueChild %s [%s] second child %s  [%s] symRef matches store symRef (#%d) so swap children\n",
                  optDetails, store->getOpCode().getName(),valueChild->getOpCode().getName(),
                  valueChild->getName(self()->comp()->getDebug()), valueChild->getSecondChild()->getOpCode().getName(),valueChild->getSecondChild()->getName(self()->comp()->getDebug()),store->getSymbolReference()->getReferenceNumber()))
         {
         valueChild->swapChildren();
         }
      }
   }


uint16_t
J9::CodeGenerator::changeParmLoadsToRegLoads(TR::Node *node, TR::Node **regLoads, TR_BitVector *globalRegsWithRegLoad, TR_BitVector &killedParms, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      {
      return 0;
      }
   else
      node->setVisitCount(visitCount);

   uint16_t numNewRegLoads = 0;

   if (node->getOpCode().isLoadAddr() && node->getOpCode().hasSymbolReference() && node->getSymbol()->isParm())
      {
      killedParms.set(node->getSymbol()->getParmSymbol()->getOrdinal());
      if (self()->comp()->getOption(TR_TraceOptDetails))
         {
         dumpOptDetails(self()->comp(), "  Found loadaddr %s\n  killedParms is now ", self()->comp()->getDebug()->getName(node));
         self()->getDebug()->print(self()->comp()->getOptions()->getLogFile(), &killedParms);
         dumpOptDetails(self()->comp(), "\n");
         }
      }

   if (node->getOpCode().isLoadVar() && node->getSymbol()->isParm())
      {
      TR::ParameterSymbol *parm      = node->getSymbol()->getParmSymbol();
      int8_t              lri       = parm->getLinkageRegisterIndex();
      TR::ILOpCodes        regLoadOp = self()->comp()->il.opCodeForRegisterLoad(parm->getDataType());

      if (regLoads[parm->getOrdinal()] == NULL && lri != -1 && !killedParms.isSet(parm->getOrdinal()))
         {
         // Transmute this node into a regLoad

         if ((node->getType().isInt64() && TR::Compiler->target.is32Bit() && !self()->use64BitRegsOn32Bit())
              || node->getType().isLongDouble())
            {
            if (self()->getDisableLongGRA())
               {
               dumpOptDetails(self()->comp(), "  GRA not supported for longs; leaving %s unchanged\n", self()->comp()->getDebug()->getName(node));
               }
            else
               {
               // Endianness affects how longs are passed
               //
               int8_t lowLRI, highLRI;
               if (TR::Compiler->target.cpu.isBigEndian() || node->getType().isLongDouble())
                  {
                  highLRI = lri;
                  lowLRI  = lri+1;
                  }
               else
                  {
                  lowLRI  = lri;
                  highLRI = lri+1;
                  }
               TR_GlobalRegisterNumber lowReg  = self()->getLinkageGlobalRegisterNumber(lowLRI,  node->getDataType());
               TR_GlobalRegisterNumber highReg = self()->getLinkageGlobalRegisterNumber(highLRI, node->getDataType());

               if (lowReg != -1 && highReg != -1 && !globalRegsWithRegLoad->isSet(lowReg) && !globalRegsWithRegLoad->isSet(highReg)
                  && performTransformation(self()->comp(), "O^O LINKAGE REGISTER ALLOCATION: transforming %s into %s\n", self()->comp()->getDebug()->getName(node), self()->comp()->getDebug()->getName(regLoadOp)))
                  {
                  // Both halves are in regs, and both regs are available.
                  // Transmute load into regload
                  //
                  if(parm->getDataType() == TR::Aggregate)
                     {
                     dumpOptDetails(self()->comp(), "\tNot doing transformation for parm %p because it is an aggregate.\n",node);
                     }
                  else
                     {
                     TR::Node::recreate(node, self()->comp()->il.opCodeForRegisterLoad(parm->getDataType()));
                     node->setLowGlobalRegisterNumber(lowReg);
                     node->setHighGlobalRegisterNumber(highReg);

                     // Update state to include the new regLoad
                     //
                     regLoads[parm->getOrdinal()] = node;
                     globalRegsWithRegLoad->set(lowReg);
                     globalRegsWithRegLoad->set(highReg);
                     numNewRegLoads++;
                     }
                  }
               }
            }
         else if (TR::Compiler->target.cpu.isZ() && TR::Compiler->target.isLinux() && parm->getDataType() == TR::Aggregate &&
                  (parm->getSize() <= 2 ||  parm->getSize() == 4 ||  parm->getSize() == 8))
            {
            // On zLinux aggregates with a size of 1, 2, 4 or 8 bytes are passed by value in registers.
            // Otherwise they are passed by reference via buffer
            // Here transform the value in register to aggregate again
            TR::DataType dt = TR::NoType;
            if (parm->getSize() == 8)
               dt = (node->getOpCode().isDouble()) ? TR::Double : TR::Int64;
            else if (parm->getSize() == 4)
               dt = (node->getOpCode().isFloat()) ? TR::Float : TR::Int32;
            else if (parm->getSize() == 2)
               dt = TR::Int16;
            else if (parm->getSize() == 1)
               dt = TR::Int8;

            // if not 64 bit and data type is 64 bit, need to place it into two registers
            if ((TR::Compiler->target.is32Bit() && !self()->use64BitRegsOn32Bit()) && dt == TR::Int64)
               {
               TR_GlobalRegisterNumber lowReg  = self()->getLinkageGlobalRegisterNumber(lri+1, dt);
               TR_GlobalRegisterNumber highReg = self()->getLinkageGlobalRegisterNumber(lri, dt);

               if (lowReg != -1 && highReg != -1 && !globalRegsWithRegLoad->isSet(lowReg) && !globalRegsWithRegLoad->isSet(highReg) &&
                   performTransformation(self()->comp(), "O^O LINKAGE REGISTER ALLOCATION: transforming aggregate parm %s into xRegLoad\n", self()->comp()->getDebug()->getName(node)))
                  {
                  TR::Node::recreate(node, self()->comp()->il.opCodeForRegisterLoad(dt));

                  node->setLowGlobalRegisterNumber(lowReg);
                  node->setHighGlobalRegisterNumber(highReg);

                  globalRegsWithRegLoad->set(lowReg);
                  globalRegsWithRegLoad->set(highReg);

                  regLoads[parm->getOrdinal()] = node;
                  numNewRegLoads++;
                  }
               }
            else
               {
               TR_GlobalRegisterNumber reg = self()->getLinkageGlobalRegisterNumber(lri, dt);

               if (reg != -1 && !globalRegsWithRegLoad->isSet(reg) &&
                   performTransformation(self()->comp(), "O^O LINKAGE REGISTER ALLOCATION: transforming aggregate parm %s into xRegLoad\n", self()->comp()->getDebug()->getName(node)))
                  {
                  TR::Node::recreate(node, self()->comp()->il.opCodeForRegisterLoad(dt));

                  node->setGlobalRegisterNumber(reg);
                  globalRegsWithRegLoad->set(reg);

                  regLoads[parm->getOrdinal()] = node;
                  numNewRegLoads++;
                  }
               }
            }
         else
            {
            TR_GlobalRegisterNumber reg = self()->getLinkageGlobalRegisterNumber(parm->getLinkageRegisterIndex(), node->getDataType());
            if (reg != -1 && !globalRegsWithRegLoad->isSet(reg)
               && performTransformation(self()->comp(), "O^O LINKAGE REGISTER ALLOCATION: transforming %s into %s\n", self()->comp()->getDebug()->getName(node), self()->comp()->getDebug()->getName(regLoadOp)))
               {
               // Transmute load into regload
               //
               if(parm->getDataType() == TR::Aggregate) // for aggregates, must look at node type to determine register type as parm type is still 'aggregate'
                  {
                  dumpOptDetails(self()->comp(), "\tNot doing transformation for parm %p because it is an aggregate.\n",node);
                  }
               else
                  {
                  TR::Node::recreate(node, self()->comp()->il.opCodeForRegisterLoad(parm->getDataType()));
                  node->setGlobalRegisterNumber(reg);

                  // Update state to include the new regLoad
                  //
                  regLoads[parm->getOrdinal()] = node;
                  globalRegsWithRegLoad->set(reg);
                  numNewRegLoads++;
                  }
               }
            }
         }
      else
         {
         // We already have a regLoad for this parm.
         // It's awkward to common the parm at this point because we'd need a pointer to its parent.
         // Let's conservatively do nothing, on the assumption that CSE usually
         // commons all the parm loads anyway, so we should rarely hit this
         // case.
         }
      }
   else
      {
      for (int i = 0; i < node->getNumChildren(); i++)
         numNewRegLoads += self()->changeParmLoadsToRegLoads(node->getChild(i), regLoads, globalRegsWithRegLoad, killedParms, visitCount);
      }

   return numNewRegLoads;
   }


void
J9::CodeGenerator::setUpForInstructionSelection()
   {
   self()->comp()->incVisitCount();

   // prepareNodeForInstructionSelection is called during a separate walk of the treetops because
   // the _register and _label fields are unioned members of a node.  prepareNodeForInstructionSelection
   // zeros the _register field while the second for loop sets label fields on destination nodes.
   //
   TR::TreeTop * tt=NULL, *prev = NULL;

   if (self()->comp()->getOption(TR_EnableOSR))
      {
      TR::Block *block;
      for (tt = self()->comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
         {
         if (tt->getNode()->getOpCodeValue() == TR::BBStart)
            {
            block = tt->getNode()->getBlock();
            if (!block->isOSRCodeBlock())
               {
               tt = block->getExit();
               continue;
               }
            }
         self()->eliminateLoadsOfLocalsThatAreNotStored(tt->getNode(), -1);
         }

      self()->comp()->incVisitCount();
      }

   for (tt = self()->comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      self()->prepareNodeForInstructionSelection(tt->getNode());
      }

   bool doRefinedAliasing = self()->enableRefinedAliasSets();

   if (doRefinedAliasing)
      {
      _refinedAliasWalkCollector.methodInfo = TR_PersistentMethodInfo::get(self()->comp());
      _refinedAliasWalkCollector.killsEverything = !_refinedAliasWalkCollector.methodInfo;
      _refinedAliasWalkCollector.killsAddressStatics = false;
      _refinedAliasWalkCollector.killsIntStatics = false;
      _refinedAliasWalkCollector.killsNonIntPrimitiveStatics = false;
      _refinedAliasWalkCollector.killsAddressFields = false;
      _refinedAliasWalkCollector.killsIntFields = false;
      _refinedAliasWalkCollector.killsNonIntPrimitiveFields = false;
      _refinedAliasWalkCollector.killsAddressArrayShadows = false;
      _refinedAliasWalkCollector.killsIntArrayShadows = false;
      _refinedAliasWalkCollector.killsNonIntPrimitiveArrayShadows = false;
      }

   for (tt = self()->comp()->getStartTree(); tt; prev=tt, tt = tt->getNextTreeTop())
      {
      TR::Node * node = tt->getNode();

      if ((node->getOpCodeValue() == TR::treetop) ||
          node->getOpCode().isAnchor() ||
          node->getOpCode().isCheck())
         {
         node = node->getFirstChild();
         if (node->getOpCode().isResolveCheck() && doRefinedAliasing)
            {
            _refinedAliasWalkCollector.killsEverything = true;
            }
         }

      TR::ILOpCode & opcode = node->getOpCode();

      if (opcode.getOpCodeValue() == TR::BBStart)
         {
         self()->setCurrentBlock(node->getBlock());
         }
      else if (opcode.isLoadVarOrStore())
         {
         TR::Symbol * sym = node->getSymbol();
         TR::AutomaticSymbol *local = sym->getAutoSymbol();
         if (local)
            {
            local->incReferenceCount();
            }
         else if (doRefinedAliasing && !_refinedAliasWalkCollector.killsEverything)
            {
            if (sym->getStaticSymbol())
               {
               if (sym->getType().isAddress())    _refinedAliasWalkCollector.killsAddressStatics = true;
               else if (sym->getType().isInt32()) _refinedAliasWalkCollector.killsIntStatics = true;
               else                               _refinedAliasWalkCollector.killsNonIntPrimitiveStatics = true;
               }
            else if (sym->isArrayShadowSymbol())
               {
               if (sym->getType().isAddress())    _refinedAliasWalkCollector.killsAddressArrayShadows = true;
               else if (sym->getType().isInt32()) _refinedAliasWalkCollector.killsIntArrayShadows = true;
               else                               _refinedAliasWalkCollector.killsNonIntPrimitiveArrayShadows = true;
               }
            else if (sym->getShadowSymbol())
               {
               if (sym->getType().isAddress())    _refinedAliasWalkCollector.killsAddressFields = true;
               else if (sym->getType().isInt32()) _refinedAliasWalkCollector.killsIntFields = true;
               else                               _refinedAliasWalkCollector.killsNonIntPrimitiveFields = true;
               }
            }
         }
      else if (opcode.isBranch())
         {
         if (node->getBranchDestination()->getNode()->getLabel() == NULL)
            {
            // need to get the label type from the target block for RAS
            TR::LabelSymbol * label =
                TR::LabelSymbol::create(self()->trHeapMemory(),self(),node->getBranchDestination()->getNode()->getBlock());

            node->getBranchDestination()->getNode()->setLabel(label);

            }
         }
      else if (opcode.isJumpWithMultipleTargets() && !opcode.isSwitch())
         {
         for (auto e = self()->getCurrentBlock()->getSuccessors().begin(); e != self()->getCurrentBlock()->getSuccessors().end(); ++e)
            {
            if (toBlock((*e)->getTo())->getEntry()!=NULL &&
                toBlock((*e)->getTo())->getEntry()->getNode()->getLabel() == NULL)
               {
               TR::LabelSymbol * label = generateLabelSymbol(self());
               toBlock((*e)->getTo())->getEntry()->getNode()->setLabel(label);
               }
            }
         }
      else if (opcode.isSwitch())
         {
         uint16_t upperBound = node->getCaseIndexUpperBound();
         for (int i = 1; i < upperBound; ++i)
            {
            if (node->getChild(i)->getBranchDestination()->getNode()->getLabel() == NULL)
               {
               TR::LabelSymbol *label = generateLabelSymbol(self());
               node->getChild(i)->getBranchDestination()->getNode()->setLabel(label);

               }
            }
         }
      else if (opcode.isCall() || opcode.getOpCodeValue() == TR::arraycopy)
         {
         self()->setUpStackSizeForCallNode(node);

         if (doRefinedAliasing)
            {
            TR::ResolvedMethodSymbol * callSymbol = node->getSymbol()->getResolvedMethodSymbol();
            TR_PersistentMethodInfo * callInfo;
            if (!_refinedAliasWalkCollector.killsEverything && !opcode.isCallIndirect() && callSymbol &&
                (callInfo = TR_PersistentMethodInfo::get(callSymbol->getResolvedMethod())) &&
                callInfo->hasRefinedAliasSets())
               {
               if (!callInfo->doesntKillAddressStatics())               _refinedAliasWalkCollector.killsAddressStatics = true;
               if (!callInfo->doesntKillIntStatics())                   _refinedAliasWalkCollector.killsIntStatics = true;
               if (!callInfo->doesntKillNonIntPrimitiveStatics())       _refinedAliasWalkCollector.killsNonIntPrimitiveStatics = true;
               if (!callInfo->doesntKillAddressFields())                _refinedAliasWalkCollector.killsAddressFields = true;
               if (!callInfo->doesntKillIntFields())                    _refinedAliasWalkCollector.killsIntFields = true;
               if (!callInfo->doesntKillNonIntPrimitiveFields())        _refinedAliasWalkCollector.killsNonIntPrimitiveFields = true;
               if (!callInfo->doesntKillAddressArrayShadows())          _refinedAliasWalkCollector.killsAddressArrayShadows = true;
               if (!callInfo->doesntKillIntArrayShadows())              _refinedAliasWalkCollector.killsIntArrayShadows = true;
               if (!callInfo->doesntKillNonIntPrimitiveArrayShadows())  _refinedAliasWalkCollector.killsNonIntPrimitiveArrayShadows = true;
               }
            else
               {
               _refinedAliasWalkCollector.killsEverything = true;
               }
            }

         }
      else if (opcode.getOpCodeValue() == TR::monent)
         {
         _refinedAliasWalkCollector.killsEverything = true;
         }
      }

   if (doRefinedAliasing && !_refinedAliasWalkCollector.killsEverything)
      {
      TR_PersistentMethodInfo *methodInfo = _refinedAliasWalkCollector.methodInfo;

      methodInfo->setDoesntKillEverything(true);
      if (!_refinedAliasWalkCollector.killsAddressStatics)               methodInfo->setDoesntKillAddressStatics(true);
      if (!_refinedAliasWalkCollector.killsIntStatics)                   methodInfo->setDoesntKillIntStatics(true);
      if (!_refinedAliasWalkCollector.killsNonIntPrimitiveStatics)       methodInfo->setDoesntKillNonIntPrimitiveStatics(true);
      if (!_refinedAliasWalkCollector.killsAddressFields)                methodInfo->setDoesntKillAddressFields(true);
      if (!_refinedAliasWalkCollector.killsIntFields)                    methodInfo->setDoesntKillIntFields(true);
      if (!_refinedAliasWalkCollector.killsNonIntPrimitiveFields)        methodInfo->setDoesntKillNonIntPrimitiveFields(true);
      if (!_refinedAliasWalkCollector.killsAddressArrayShadows)          methodInfo->setDoesntKillAddressArrayShadows(true);
      if (!_refinedAliasWalkCollector.killsIntArrayShadows)              methodInfo->setDoesntKillIntArrayShadows(true);
      if (!_refinedAliasWalkCollector.killsNonIntPrimitiveArrayShadows)  methodInfo->setDoesntKillNonIntPrimitiveArrayShadows(true);
      }

   if (TR::Compiler->target.cpu.isX86() && self()->getInlinedGetCurrentThreadMethod())
      {
      TR::RealRegister *ebpReal = self()->getRealVMThreadRegister();

      if (ebpReal)
         {
         ebpReal->setState(TR::RealRegister::Locked);
         ebpReal->setAssignedRegister(ebpReal->getRegister());
         }
      }
   }

bool
J9::CodeGenerator::wantToPatchClassPointer(TR::Compilation *comp,
                             const TR_OpaqueClassBlock *allegedClassPointer,
                             const uint8_t *inCodeAt)
   {
   return TR::CodeGenerator::wantToPatchClassPointer(comp, allegedClassPointer, "in code at", inCodeAt);
   }

bool
J9::CodeGenerator::wantToPatchClassPointer(const TR_OpaqueClassBlock *allegedClassPointer, const uint8_t *inCodeAt)
   {
   return TR::CodeGenerator::wantToPatchClassPointer(self()->comp(), allegedClassPointer, inCodeAt);
   }

bool
J9::CodeGenerator::wantToPatchClassPointer(const TR_OpaqueClassBlock *allegedClassPointer, const TR::Node *forNode)
   {
   return TR::CodeGenerator::wantToPatchClassPointer(self()->comp(), allegedClassPointer, "for node", forNode);
   }

bool
J9::CodeGenerator::supportsMethodEntryPadding()
   {
   return self()->fej9()->supportsMethodEntryPadding();
   }

bool
J9::CodeGenerator::mustGenerateSwitchToInterpreterPrePrologue()
   {
   TR::Compilation *comp = self()->comp();

   return comp->usesPreexistence() ||
      comp->getOption(TR_EnableHCR) ||
      !comp->fej9()->isAsyncCompilation() ||
      comp->getOption(TR_FullSpeedDebug);
   }

extern void VMgenerateCatchBlockBBStartPrologue(TR::Node *node, TR::Instruction *fenceInstruction, TR::CodeGenerator *cg);

void
J9::CodeGenerator::generateCatchBlockBBStartPrologue(
      TR::Node *node,
      TR::Instruction *fenceInstruction)
   {
   VMgenerateCatchBlockBBStartPrologue(node, fenceInstruction, self());
}

void
J9::CodeGenerator::registerAssumptions()
   {
   for(auto it = self()->getJNICallSites().begin();
		it != self()->getJNICallSites().end(); ++it)
      {
      TR_OpaqueMethodBlock *method = (*it)->getKey()->getPersistentIdentifier();
      TR::Instruction *i = (*it)->getValue();
      TR_PatchJNICallSite::make(self()->fe(), self()->trPersistentMemory(), (uintptrj_t) method, i->getBinaryEncoding(), self()->comp()->getMetadataAssumptionList());
      }
   }

void
J9::CodeGenerator::jitAddPicToPatchOnClassUnload(void *classPointer, void *addressToBePatched)
   {
   createClassUnloadPicSite(classPointer, addressToBePatched, sizeof(uintptrj_t), self()->comp()->getMetadataAssumptionList());
   self()->comp()->setHasClassUnloadAssumptions();
   }
void
J9::CodeGenerator::jitAdd32BitPicToPatchOnClassUnload(void *classPointer, void *addressToBePatched)
   {
   createClassUnloadPicSite(classPointer, addressToBePatched,4, self()->comp()->getMetadataAssumptionList());
   self()->comp()->setHasClassUnloadAssumptions();
   }

void
J9::CodeGenerator::jitAddPicToPatchOnClassRedefinition(void *classPointer, void *addressToBePatched, bool unresolved)
   {
    if (!self()->comp()->compileRelocatableCode())
      {
      createClassRedefinitionPicSite(unresolved? (void*)-1 : classPointer, addressToBePatched, sizeof(uintptrj_t), unresolved, self()->comp()->getMetadataAssumptionList());
      self()->comp()->setHasClassRedefinitionAssumptions();
      }
   }

void
J9::CodeGenerator::jitAdd32BitPicToPatchOnClassRedefinition(void *classPointer, void *addressToBePatched, bool unresolved)
   {
   if (!self()->comp()->compileRelocatableCode())
      {
      createClassRedefinitionPicSite(unresolved? (void*)-1 : classPointer, addressToBePatched, 4, unresolved, self()->comp()->getMetadataAssumptionList());
      self()->comp()->setHasClassRedefinitionAssumptions();
      }
   }

void
J9::CodeGenerator::createHWPRecords()
   {
   if (self()->comp()->getPersistentInfo()->isRuntimeInstrumentationEnabled() &&
       TR::Options::getCmdLineOptions()->getOption(TR_EnableHardwareProfileIndirectDispatch))
      {
      self()->comp()->fej9()->createHWProfilerRecords(self()->comp());
      }
   }


TR::Linkage *
J9::CodeGenerator::createLinkageForCompilation()
   {
   return self()->getLinkage(self()->comp()->getJittedMethodSymbol()->getLinkageConvention());
   }


TR::TreeTop *
J9::CodeGenerator::lowerTree(TR::Node *root, TR::TreeTop *treeTop)
   {
   return self()->fej9()->lowerTree(self()->comp(), root, treeTop);
   }


bool
J9::CodeGenerator::needClassAndMethodPointerRelocations()
   {
   return self()->fej9()->needClassAndMethodPointerRelocations();
   }


bool
J9::CodeGenerator::needRelocationsForStatics()
   {
   return self()->fej9()->needRelocationsForStatics();
   }


bool
J9::CodeGenerator::isMethodInAtomicLongGroup(TR::RecognizedMethod rm)
   {
   switch (rm)
      {
      case TR::java_util_concurrent_atomic_AtomicLong_addAndGet:
      case TR::java_util_concurrent_atomic_AtomicLongArray_addAndGet:
      case TR::java_util_concurrent_atomic_AtomicLongArray_decrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicLongArray_getAndAdd:
      case TR::java_util_concurrent_atomic_AtomicLongArray_getAndDecrement:
      case TR::java_util_concurrent_atomic_AtomicLongArray_getAndIncrement:
      case TR::java_util_concurrent_atomic_AtomicLongArray_getAndSet:
      case TR::java_util_concurrent_atomic_AtomicLongArray_incrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicLong_decrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicLong_getAndAdd:
      case TR::java_util_concurrent_atomic_AtomicLong_getAndDecrement:
      case TR::java_util_concurrent_atomic_AtomicLong_getAndIncrement:
      case TR::java_util_concurrent_atomic_AtomicLong_getAndSet:
      case TR::java_util_concurrent_atomic_AtomicLong_incrementAndGet:
         return true;

      default:
         return false;
      }
   }

