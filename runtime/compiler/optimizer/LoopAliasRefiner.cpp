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

#include "optimizer/LoopAliasRefiner.hpp"

#include <stddef.h>
#include <stdint.h>
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "env/TRMemory.hpp"
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/List.hpp"
#include "optimizer/LoopVersioner.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/SPMDPreCheck.hpp"
#include "optimizer/Structure.hpp"

namespace TR { class TreeTop; }

TR_LoopAliasRefiner::TR_LoopAliasRefiner(TR::OptimizationManager *manager)
   : TR_LoopVersioner(manager, true, true)
   {}
  
const char *
TR_LoopAliasRefiner::optDetailString() const throw()
   {
   return "O^O LOOP ALIAS REFINER: ";
   }

void TR_LoopAliasRefiner::collectArrayAliasCandidates(TR::Node *node,  vcount_t visitCount)
   {
   bool isStore = node->getOpCode().isStoreIndirect();
   if (isStore)
      collectArrayAliasCandidates(node, node->getSecondChild(), visitCount, false); //2nd child first

   collectArrayAliasCandidates(node, node->getFirstChild(), visitCount, isStore);
   }

void TR_LoopAliasRefiner::collectArrayAliasCandidates(TR::Node *parentArrayNode, TR::Node *node,  vcount_t visitCount, bool isStore)
   {
   if (node->getOpCodeValue() == TR::aiadd || node->getOpCodeValue() == TR::aladd)
      {
      if (trace())
         traceMsg(comp(), "LAR: Inspecting aiadd %p\n", node);

      if (!parentArrayNode->getOpCode().isLoadIndirect() && !parentArrayNode->getOpCode().isStoreIndirect())
         {
         _addressingTooComplicated = true;
         if (trace()) 
            dumpOptDetails(comp(), "FAIL: Unexpected parentArrayNode to aiadd/aladd [%p]\n", parentArrayNode);
         return;
         }

      // trivialarrayindependence may have already refined this shadow--if so ignore it
      if (comp()->getSymRefTab()->isRefinedArrayShadow(parentArrayNode->getSymbolReference()))
         {
         if (trace()) 
            traceMsg(comp(), "FAIL: Shadow #%d in [%p] already refined\n", 
                            parentArrayNode->getSymbolReference()->getReferenceNumber(),
                            parentArrayNode);
         return;
         }

      //we only handle array shadows
      if (!parentArrayNode->getSymbol()->isArrayShadowSymbol())
         {
         if (trace())
            traceMsg(comp(), "FAIL: Shadow #%d in [%p] is not an array shadow\n",parentArrayNode->getSymbolReference()->getReferenceNumber(),parentArrayNode);
         return;
         }

      // make sure we don't try to refine unsafe shadows...
      if (parentArrayNode->getSymbol()->isUnsafeShadowSymbol())
         {
         if (trace())
            traceMsg(comp(), "FAIL: Shadow #%d in [%p] is an unsafe shadow\n",
                            parentArrayNode->getSymbolReference()->getReferenceNumber(),
                            parentArrayNode);
         return;
         }
      
      TR::Node *arrayAddress = node->getFirstChild();

      if (!_containsCall &&
         (arrayAddress->getOpCodeValue() == TR::aload ||
          (arrayAddress->getOpCodeValue() == TR::aloadi &&
           arrayAddress->getFirstChild()->getOpCodeValue() == TR::aload)) &&
          _currentNaturalLoop->isExprInvariant(arrayAddress) &&
           arrayAddress->getSymbol()->isCollectedReference() &&
          !arrayAddress->getSymbol()->isInternalPointerAuto())
         {
         if (trace())
            traceMsg(comp(), "\tA) Adding candidate node %p parent %p for block_%d\n", node, parentArrayNode, _currentBlock->getNumber());

         _arrayMemberLoadCandidates->add(new (trStackMemory()) TR_NodeParentBlockTuple(node, parentArrayNode, _currentBlock));
         }
      }
   }


/*
 * look for array
 * array defs contains all candidate array definitions found.  useCandidates are an ordered sequence of candidate
 * defs and uses
 * Versioning processes loops from outer to inner.  If there are no problems refining an outer loop,
 * there is no reason to refine the inner ones.  If a given candidate cannot be analysed, allow
 * reprocessing of that particular loop, if it is a child loop.
 */

bool TR_LoopAliasRefiner::hasMulShadowTypes(TR_ScratchList<TR_NodeParentBlockTuple> *candList)
   {
   ListIterator<TR_NodeParentBlockTuple> candIterator(candList);
   TR_NodeParentBlockTuple* npbt = candIterator.getFirst();

   TR::SymbolReference * sourceSymRef=npbt->_parent->getSymbolReference();
   npbt = candIterator.getNext();
   for (; npbt; npbt = candIterator.getNext())
      {
          TR::SymbolReference *targetSymRef = npbt->_parent->getSymbolReference();
      
      if(targetSymRef!=sourceSymRef && !sourceSymRef->getUseDefAliases().contains(targetSymRef, comp()))
         return true;
      }

   return false;
   }

bool TR_LoopAliasRefiner::processArrayAliasCandidates()
   {
   _arrayRanges = NULL;
   bool haveGoodMemberCandidates = false;
   bool foundSecondArray = false; // no point improving aliasing if there is only one array
   bool haveNonMemberCandidates = false;
   int32_t numUses = 0;

   if (!_arrayMemberLoadCandidates->isEmpty())
      {
      haveGoodMemberCandidates = true;
      foundSecondArray = true;
      }

   if (isAlreadyProcessed(_currentNaturalLoop->getNumber()))
      {
      if (trace())
         traceMsg(comp(), "Already processed loop %d\n", _currentNaturalLoop->getNumber());
      return false;
      }

   if (trace())
      traceMsg(comp(), "LAR: Processing loop %d\n", _currentNaturalLoop->getNumber());
  
   markAsProcessed(_currentNaturalLoop->getNumber());

   if (!SPMDPreCheck::isSPMDCandidate(comp(), _currentNaturalLoop))
      {
      if (trace())
         traceMsg(comp(), "LAR: SPMDPreCheck failed - skipping consideration of loop %d\n", _currentNaturalLoop->getNumber());
      return false;
      }

   TR_ScratchList<IVValueRange> ivList(trMemory()); // list of all ivs possibly referenced in this loop

   ListIterator<TR_NodeParentBlockTuple> useCand(_arrayLoadCandidates);
   TR_NodeParentBlockTuple *curTuple;
 
   _arrayRanges = new(trStackMemory()) TR_ScratchList<ArrayRangeLimits>(trMemory());
 
   numUses = 0;

   bool goodCandidateDetected = false;
   
   if (trace())
      traceMsg(comp(), "LAR: Finished loop processing\n\t%s\n", 
                     (haveGoodMemberCandidates) &&
                     foundSecondArray?"Candidates exist":"No Candidates");

   if (!foundSecondArray || !haveGoodMemberCandidates) return false;
   
   ListIterator<TR_NodeParentBlockTuple> memberCand(_arrayMemberLoadCandidates);

   numUses = 0;
   goodCandidateDetected = false;
   bool atLeastOneStore = false;

   while (curTuple = _arrayMemberLoadCandidates->popHead())
      { 
      memberCand.reset();
      int32_t refCount = 0;

      TR_ASSERT(curTuple->_node->getFirstChild()->getOpCodeValue() == TR::aload ||
                curTuple->_node->getFirstChild()->getOpCodeValue() == TR::aloadi,
                "unexpected node %p\n", curTuple->_node->getFirstChild());

      atLeastOneStore |= curTuple->_parent->getOpCode().isStoreIndirect();

      TR::SymbolReference *currentBaseSymRef = NULL;
      TR::SymbolReference *currentMemberSymRef = NULL;
      if (curTuple->_node->getFirstChild()->getOpCodeValue() == TR::aloadi)
         {
         currentBaseSymRef = curTuple->_node->getFirstChild()->getFirstChild()->getSymbolReference();
         currentMemberSymRef = curTuple->_node->getFirstChild()->getSymbolReference();
         }
      else
         {
         currentBaseSymRef = curTuple->_node->getFirstChild()->getSymbolReference();
         }


      TR_ScratchList<TR_NodeParentBlockTuple> *copyOfCandidateRefs = new(trStackMemory()) TR_ScratchList<TR_NodeParentBlockTuple>(trMemory());
      TR::SymbolReference *arrayAccessSymRef = curTuple->_parent->getSymbolReference();

      for (; curTuple; curTuple = memberCand.getNext())
         {
         if (false)
            traceMsg(comp(), "    this:#%d member:%d offset %d indshadow:#%d\n",  
                            curTuple->_node->getFirstChild()->getFirstChild()->getSymbolReference()->getReferenceNumber(), 
                            curTuple->_node->getFirstChild()->getSymbolReference()->getReferenceNumber(),  
                            curTuple->_node->getFirstChild()->getSymbolReference()->getOffset(), 
                            curTuple->_parent->getSymbolReference()->getReferenceNumber());
         
         //if (numUses >= 2)
            goodCandidateDetected = true;// expect at least two uses between stores for commoning

         atLeastOneStore |= curTuple->_parent->getOpCode().isStoreIndirect();

         TR::SymbolReference *curTupleBaseSymRef = NULL;
         TR::SymbolReference *curTupleMemberSymRef = NULL;
         if (curTuple->_node->getFirstChild()->getOpCodeValue() == TR::aloadi)
            {
            curTupleBaseSymRef = curTuple->_node->getFirstChild()->getFirstChild()->getSymbolReference();
            curTupleMemberSymRef = curTuple->_node->getFirstChild()->getSymbolReference();
            }
         else
            {
            curTupleBaseSymRef = curTuple->_node->getFirstChild()->getSymbolReference();
            }

         // TODO: clarify this heuristic
         if (curTupleBaseSymRef == currentBaseSymRef &&
             curTupleMemberSymRef == currentMemberSymRef)
            {
            bool isStore;
            if (isStore = curTuple->_parent->getOpCode().isStoreIndirect())
               {
               numUses = 0;
               }
            else
               {
               ++numUses;
               }
            ++refCount;
            copyOfCandidateRefs->add(curTuple);
            _arrayMemberLoadCandidates->remove(curTuple);
            }
         }

      if(goodCandidateDetected)
         goodCandidateDetected = !hasMulShadowTypes(copyOfCandidateRefs);

      if (goodCandidateDetected)
         {
         if (trace()) 
            traceMsg(comp(), "\tAdding entry for base #%d member #%d offset %d with %d refs\n",
                             currentBaseSymRef->getReferenceNumber(),
                             currentMemberSymRef ? currentMemberSymRef->getReferenceNumber() : 0, 
                             currentMemberSymRef ? currentMemberSymRef->getOffset() : 0,
                             refCount);

         ArrayRangeLimits *arl = new(comp()->trStackMemory())
                                 ArrayRangeLimits(copyOfCandidateRefs, currentBaseSymRef, currentMemberSymRef, arrayAccessSymRef);
         _arrayRanges->add(arl);
         }
      }

   if (!goodCandidateDetected || _arrayRanges->getSize() <= 1 || !atLeastOneStore)
      return false;

   requestOpt(OMR::treeSimplification);
   requestOpt(OMR::treesCleansing);
   requestOpt(OMR::redundantGotoElimination);
   return true;
   }

void TR_LoopAliasRefiner::initAdditionalDataStructures()
   {
   _processedLoops = new(trStackMemory()) TR_BitVector(1, trMemory(), stackAlloc, growable);
   }

void TR_LoopAliasRefiner::buildAliasRefinementComparisonTrees(List<TR::TreeTop> *nullCheckTrees, List<TR::TreeTop> *divCheckTrees, List<TR::TreeTop> *checkCastTrees, List<TR::TreeTop> *arrayStoreCheckTrees, TR_ScratchList<TR::Node> *comparisonTrees,  TR::Block *exitGotoBlock)
   { 
   if (!_arrayRanges)
      {
      if (trace())
         traceMsg(comp(),  "array ranges is null for %s\n", comp()->signature());
      return;
      }

   TR_ASSERT(comparisonTrees, "comparison trees is null\n");
   TR_ASSERT(exitGotoBlock, "goto block is null");

   ListIterator<ArrayRangeLimits> arangeIterator(_arrayRanges);
     ArrayRangeLimits *arrayPtr;
     for (arrayPtr = arangeIterator.getFirst(); arrayPtr ; arrayPtr = arangeIterator.getNext())
        {
        TR::Node *nodeInfo  = arrayPtr->getCandidateList()->getListHead()->getData()->_node;
        TR::Node * nodeToBeNullChkd = nodeInfo->getFirstChild();
        vcount_t visitCount = comp()->incVisitCount();
        // Collecting all prereq tests
        collectAllExpressionsToBeChecked(nullCheckTrees, divCheckTrees, checkCastTrees, arrayStoreCheckTrees, nodeToBeNullChkd, comparisonTrees, exitGotoBlock, visitCount);
        }

   TR_ScratchList<ArrayRangeLimits> *listCopy = new(trStackMemory()) TR_ScratchList<ArrayRangeLimits>(trMemory());

   while( ArrayRangeLimits *arlAPtr = _arrayRanges->popHead())
      {
      listCopy->add(arlAPtr);
    
      ListIterator<ArrayRangeLimits> arIterator(_arrayRanges);
      ArrayRangeLimits *arlBPtr;
      for (arlBPtr = arIterator.getFirst(); arlBPtr; arlBPtr = arIterator.getNext())
         {
         TR::Node *testExpr  =  arlAPtr->createRangeTestExpr(comp(), arlBPtr, exitGotoBlock, trace());
         if (testExpr && performTransformation(comp(), "%sAdding test [%p] to refine aliases for loop %d\n", 
                                           optDetailString(),
                                           testExpr, _currentNaturalLoop->getNumber()))
            {
            comparisonTrees->add(testExpr);
            }
         }
      }
   _arrayRanges = listCopy;
   }


void
TR_LoopAliasRefiner::refineArrayAliases(TR_RegionStructure *whileLoop)
   {
   if (_arrayRanges &&
      !performTransformation(comp(), "%sRefining aliasing in loop %d\n", optDetailString(), whileLoop->getNumber())) return;

   vcount_t visitCount = comp()->incVisitCount();
   ListIterator<ArrayRangeLimits> arIterator(_arrayRanges);
   TR_ScratchList<TR::SymbolReference> newShadowList(trMemory());

   for (ArrayRangeLimits *arlPtr = arIterator.getFirst(); arlPtr; arlPtr = arIterator.getNext())
      {
      TR_ScratchList<TR_NodeParentBlockTuple> *list = arlPtr->getCandidateList();
      ListIterator<TR_NodeParentBlockTuple> candIterator(list);
      TR_NodeParentBlockTuple* npbt;
    
      if (!performTransformation(comp(), "%sReplacing shadows for array reference #%d\n",
                                         optDetailString(),
                                         arlPtr->getBaseSymRef()->getReferenceNumber()))
         continue;

      TR::SymbolReference * oldShadow = NULL;
      TR::SymbolReference * newShadow = NULL;
   
      for (npbt = candIterator.getFirst(); npbt; npbt = candIterator.getNext())
         {
         TR::Node *targetNode = npbt->_parent;

         if (targetNode->getVisitCount() == visitCount) continue;
         targetNode->setVisitCount(visitCount);

         TR_ASSERT(targetNode->getOpCode().isLoad() || targetNode->getOpCode().isStore(), "Unexpected opcode on %p\n", targetNode);
         oldShadow = targetNode->getSymbolReference();

         if (!newShadow)
            {
            newShadow = comp()->getSymRefTab()->createRefinedArrayShadowSymbolRef(oldShadow->getSymbol()->getDataType());

            dumpOptDetails(comp(), "Replacing1 shadow #%d with #%d in [%p] %d %d\n", oldShadow->getReferenceNumber(), 
                          newShadow->getReferenceNumber(), targetNode, oldShadow->getSymbol()->getDataType().getDataType(), 
                                                                       newShadow->getSymbol()->getDataType().getDataType());

            ListIterator<TR::SymbolReference> symRefIterator(&newShadowList);

            for (TR::SymbolReference *symRef = symRefIterator.getFirst(); symRef; symRef = symRefIterator.getNext())
               {
               newShadow->makeIndependent(comp()->getSymRefTab(), symRef);
               }

            newShadowList.add(newShadow);
            }

         dumpOptDetails(comp(), "Replacing2 shadow #%d with #%d in [%p] %d %d\n", oldShadow->getReferenceNumber(), 
                          newShadow->getReferenceNumber(), targetNode, oldShadow->getSymbol()->getDataType().getDataType(),
                          newShadow->getSymbol()->getDataType().getDataType());


         TR_ASSERT(newShadow->getSymbol()->getDataType() == oldShadow->getSymbol()->getDataType(), "shadow's types don't match\n");

         targetNode->setSymbolReference(newShadow);
         }
      }
   }



/* 
 * Create a conditional branch based on the limits of current range vs other range.  The test should look like
 * if (a == b && (other.low <= this.high && this.low <= other.high)) goto unrefined loop
 *
 * Since ranges are not implemented we just do: if (a == b)
 */
TR::Node *
TR_LoopAliasRefiner::ArrayRangeLimits::createRangeTestExpr(TR::Compilation *comp, ArrayRangeLimits *other, TR::Block * targetBlock, bool trace)
   {
   TR::Node *nodeInfo  = getCandidateList()->getListHead()->getData()->_node;
   TR::Node *arrayA = NULL, *arrayB = NULL;

   dumpOptDetails(comp, "#%d(%d) (member #%d(%d) vs. #%d(%d) (member #%d(%d))\n",
                          getBaseSymRef()->getReferenceNumber(),
                          getBaseSymRef()->getOffset(), 
                          getMemberSymRef() ? getMemberSymRef()->getReferenceNumber() : 0,
                          getMemberSymRef() ? getMemberSymRef()->getOffset() : 0,
                          other->getBaseSymRef()->getReferenceNumber(),
                          other->getBaseSymRef()->getOffset(), 
                          other->getMemberSymRef() ? other->getMemberSymRef()->getReferenceNumber() : 0,
                          other->getMemberSymRef() ? other->getMemberSymRef()->getOffset() : 0);


   if (getMemberSymRef())
      {
      arrayA = TR::Node::createWithSymRef(nodeInfo, TR::aloadi, 1, getMemberSymRef());
      arrayA->setAndIncChild(0, TR::Node::createLoad(nodeInfo, getBaseSymRef()));
      }
   else
      {
      arrayA = TR::Node::createLoad(nodeInfo, getBaseSymRef());
      }

   if (other->getMemberSymRef())
      {
      arrayB = TR::Node::createWithSymRef(nodeInfo, TR::aloadi, 1, other->getMemberSymRef());
      arrayB->setAndIncChild(0, TR::Node::createLoad(nodeInfo, other->getBaseSymRef()));
      }
   else
      {
      arrayB = TR::Node::createLoad(nodeInfo, other->getBaseSymRef());
      }

  //if the 2 array are not aliasing with each other there is no need to generate the cmpeq
   bool isAliased = false;
   if ((getArrayAccessSymRef() == other->getArrayAccessSymRef()) ||
       getArrayAccessSymRef()->getUseDefAliases().contains(other->getArrayAccessSymRef(), comp))
      isAliased = true;

   if (trace)
      traceMsg(comp, "access sym ref1 %d access sym ref2 %d isAliased %d\n", getArrayAccessSymRef(), other->getArrayAccessSymRef(), isAliased);

   TR::Node *addressTest = NULL;
   TR::Node *ifNode = NULL;
   if (isAliased)
      {
      addressTest = TR::Node::create(TR::acmpeq, 2, arrayA, arrayB);
      ifNode = TR::Node::createif (TR::ificmpne, addressTest, TR::Node::iconst(nodeInfo, 0), targetBlock->getEntry());
      }
   return ifNode;
   }

