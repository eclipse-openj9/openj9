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

#include "codegen/InMemoryLoadStoreMarking.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "codegen/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/VMJ9.h"
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "infra/Assert.hpp"
#include "infra/Bit.hpp"


#define OPT_DETAILS "O^O IN MEMORY LOAD/STORE MARKING: "

void traceBCDOpportunities(TR::Node *node, TR::Compilation *comp);

void InMemoryLoadStoreMarking::perform()
   {
   if (!cg->getOptimizationPhaseIsComplete())
      return;

   LexicalTimer pt1("InMemoryLoadStoreMarking", comp()->phaseTimer());

   vcount_t visitCount = comp()->incOrResetVisitCount();

   clearAllLists();

   TR::Block *block = NULL;
   for (TR::TreeTop *tt = comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      TR::Node* node = tt->getNode();

      TR_ASSERT(node->getVisitCount() != visitCount, "Code Gen: error in lowering trees");
      TR_ASSERT(node->getReferenceCount() == 0, "Code Gen: error in lowering trees");

      if (node->getOpCodeValue() == TR::BBStart)
         {
         block = node->getBlock();
         if (!block->isExtensionOfPreviousBlock())
            {
            if (cg->traceBCDCodeGen())
               traceMsg(comp(),"block_%d is a new extension so clear all load and store lists\n",block->getNumber());
            clearAllLists();
            }
         }

      // depth first search on children to populate the commoned nodes lists
      visitChildren(node, tt, visitCount);

      traceBCDOpportunities(node, comp());

      // after descending to look at the children (and add to the lists) now see if the current treetop
      // is a possible def and any nodes have to be removed from the lists
      handleDef(node, tt);
      }
   }

void InMemoryLoadStoreMarking::visitChildren(TR::Node *parent, TR::TreeTop *tt, vcount_t visitCount)
   {
   parent->setVisitCount(visitCount);

   if (parent->getOpCode().isBinaryCodedDecimalOp())
      cg->setMethodContainsBinaryCodedDecimal();

   if (parent->getOpCode().isBinaryCodedDecimalOp())
      {
      cg->setAddStorageReferenceHints();
      }

   for (int32_t childCount = 0; childCount < parent->getNumChildren(); childCount++)
      {
      TR::Node *child = parent->getChild(childCount);
      refineConditionalCleanLoadList(child, parent);
      if (child->getVisitCount() != visitCount)
         {
         visitChildren(child, tt, visitCount);
         traceBCDOpportunities(child, comp());
         examineFirstReference(child, tt);
         }
      else
         {
         examineCommonedReference(child, parent, tt);
         }
      }
   }

void InMemoryLoadStoreMarking::refineConditionalCleanLoadList(TR::Node *child, TR::Node *parent)
   {
   if (!child->getOpCode().isBCDLoadVar())
      return;
   if (_BCDConditionalCleanLoadList.find(child))
      {
      if (cg->traceBCDCodeGen())
         traceMsg(comp(),"found %s (%p) in condCleanLoadList : examine parent %s (%p) to see if it relies on clean sign\n",
            child->getOpCode().getName(),child,parent->getOpCode().getName(),parent);
      if (cg->reliesOnAParticularSignEncoding(parent))
         {
         if (cg->traceBCDCodeGen())
            traceMsg(comp(),"\tz^z : parent %s (%p) does rely on a clean sign so remove child %s (%p) from condCleanLoadList\n",
               parent->getOpCode().getName(),parent,child->getOpCode().getName(),child);
         _BCDConditionalCleanLoadList.remove(child);
         }
      else
         {
         if (cg->traceBCDCodeGen())
            traceMsg(comp(),"\tparent %s (%p) does not rely on a clean sign so do not remove child %s (%p) from condCleanLoadList\n",
               parent->getOpCode().getName(),parent,child->getOpCode().getName(),child);
         }
      }
   }

void InMemoryLoadStoreMarking::examineFirstReference(TR::Node *node, TR::TreeTop* tt) // called the first time 'node' is encountered
   {
   if ((node->getType().isAggregate() || node->getType().isBCD()) &&
       node->getReferenceCount() > 1)
      {
      if (tt->getNode()->getOpCode().isBCDStore() &&
          tt->getNode()->getValueChild() == node)
         {
         if (cg->traceBCDCodeGen())
            traceMsg(comp(),"store first encounter: %s (%p) under %s (%p), add store to list and set node futureUseCount %d\n",
               node->getOpCode().getName(),node,tt->getNode()->getOpCode().getName(),tt->getNode(),node->getReferenceCount()-1);
         node->setFutureUseCount(node->getReferenceCount()-1);
         _BCDAggrStoreList.add(tt->getNode());
         }
      // in cases like:
      // pdstore     <- tt->getNode()
      //    pdload   <- node
      // the pdstore is added the _BCDAggrStoreList and the node is added to the _BCDAggrLoadList and the node futureUseCount is set twice but to the same value
      if (node->getOpCode().isBCDLoadVar())
         {
         if (cg->traceBCDCodeGen())
            traceMsg(comp(),"load var first encounter: %s (%p), add load to list and set node futureUseCount %d\n",node->getOpCode().getName(),node,node->getReferenceCount()-1);
         node->setFutureUseCount(node->getReferenceCount()-1);
         _BCDAggrLoadList.add(node);
         }
      }
   }

void InMemoryLoadStoreMarking::handleLoadLastReference(TR::Node *node, List<TR::Node> &nodeList, TR_NodeListTypes listType)
   {
   bool isLastReferenceInList = node->getOpCode().isBCDLoadVar() && nodeList.find(node);

   if (isLastReferenceInList &&
       performTransformation(comp(),"%ssetSkipCopyOnLoad to true on %s (0x%p) and remove from %s\n",OPT_DETAILS,node->getOpCode().getName(),node,getName(listType)))
      {
      node->setSkipCopyOnLoad(true);
      nodeList.remove(node);
      }

   if (isLastReferenceInList &&
       listType == ConditionalCleanLoadList &&
       node->hasSignStateOnLoad() &&
       performTransformation(comp(),"%ssetHasSignStateOnLoad to false on %s (0x%p) for node in %s\n",OPT_DETAILS,node->getOpCode().getName(),node,getName(listType)))
      {
      // if every reference does not rely on the sign state then it is safe to consider that the load has no sign state on the load (as all parent will not care about a particular sign)
      node->setHasSignStateOnLoad(false);
      }
   }

void InMemoryLoadStoreMarking::examineCommonedReference(TR::Node *child, TR::Node *parent, TR::TreeTop *tt)
   {
   if (!child->getType().isBCD())
      return;
   int32_t isLastReference = child->decFutureUseCount() == 0;
   if (cg->traceBCDCodeGen())
      traceMsg(comp(),"looking at commoned reference to %s (%p) with parent %s (%p) and tt %s (%p) (isLastReference %s)\n",
         child->getOpCode().getName(),child,
         parent?parent->getOpCode().getName():"NULL",parent,
         tt?tt->getNode()->getOpCode().getName():"NULL",tt?tt->getNode():0,
         isLastReference?"yes":"no");

   if (!isLastReference)
      return;

   if (child->getType().isBCD() && !_BCDAggrStoreList.isEmpty())
      {
      ListIterator<TR::Node> listIt(&_BCDAggrStoreList);
      for (TR::Node *store=listIt.getFirst(); store; store = listIt.getNext())
         {
         if (isLastReference &&
             store->getValueChild() == child &&
             performTransformation(comp(),"%ssetSkipCopyOnStore to true on %s (0x%p) and remove from list for last commoned ref to valueChild %s (0x%p)\n",
               OPT_DETAILS,store->getOpCode().getName(),store,child->getOpCode().getName(),child))
            {
            store->setSkipCopyOnStore(true);
            _BCDAggrStoreList.remove(store);
            }
         }
      }

   if (isLastReference)
      {
      handleLoadLastReference(child, _BCDAggrLoadList, LoadList);
      handleLoadLastReference(child, _BCDConditionalCleanLoadList, ConditionalCleanLoadList);
      }
   }

bool InMemoryLoadStoreMarking::allListsAreEmpty()
   {
   if (_BCDAggrLoadList.isEmpty() &&
       _BCDAggrStoreList.isEmpty() &&
       _BCDConditionalCleanLoadList.isEmpty())
      {
      return true;
      }
   else
      {
      return false;
      }
   }

void InMemoryLoadStoreMarking::handleDef(TR::Node* node, TR::TreeTop* tt)
   {
   if (!allListsAreEmpty())
      {
      if (tt->isPossibleDef())
         {
         TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
         TR::Node *defNode = tt->getNode()->getOpCodeValue() == TR::treetop ? tt->getNode()->getFirstChild() : tt->getNode();
         processBCDAggrNodeList(_BCDAggrLoadList,             defNode, LoadList);
         processBCDAggrNodeList(_BCDConditionalCleanLoadList, defNode, ConditionalCleanLoadList);
         processBCDAggrNodeList(_BCDAggrStoreList,            defNode, StoreList);
         }
      }
   }

bool InMemoryLoadStoreMarking::isConditionalCleanLoad(TR::Node *listNode, TR::Node *defNode)
   {
   if (defNode->getOpCode().isBCDStore() &&
       defNode->chkCleanSignInPDStoreEvaluator() &&
       listNode->getOpCode().isBCDLoadVar() &&
       defNode->getValueChild() == listNode &&
       defNode->getDecimalPrecision() == listNode->getDecimalPrecision() &&
       cg->loadOrStoreAddressesMatch(defNode, listNode))
      {
      //
      // pdstore "a" clean
      //    pdload "a"
      //
      //    =>pdload "a"
      //
      // In this case, if all the parents of pdload "a" do not rely on the sign being clean then this exact store from "a" to "a" does not
      // have to be considered a kill and the skipCopyOnLoad flag can still be set.
      // The listNode will be added to the _BCDConditionalCleanLoadList so all subsequent references have their parents checked (for not relying on the clean sign).
      //
      // The _BCDConditionalCleanLoadList also must be checked for other possible kills as with the regular _BCDAggrLoadList
      // The caller is removing listNode from _BCDAggrLoadList so a particular listNode will only be in one of (or neither of) _BCDConditionalCleanLoadList and _BCDAggrLoadList
      if (cg->traceBCDCodeGen())
         traceMsg(comp(),"\t\tfound conditional clean listNode %s (%p) under %s (%p) : do not consider a def but add to condCleanLoadList if not already present\n",
            listNode->getOpCode().getName(),listNode,defNode->getOpCode().getName(),defNode);
      return true;
      }
   return false;
   }

void InMemoryLoadStoreMarking::addToConditionalCleanLoadList(TR::Node *listNode)
   {
   if (_BCDConditionalCleanLoadList.find(listNode))
      {
      // This condition may be hit if the commoned load is present under two or more different stores to the same location that also clean:
      // pdstore "a" clean
      //    pdload "a"
      //
      // pdstore "a" clean
      //    =>pdload "a"   <-- will already be in list at this point
      //
      if (cg->traceBCDCodeGen())
         traceMsg(comp(),"\t\tlistNode %s (%p) already present in condCleanLoadList : do not add again\n",
            listNode->getOpCode().getName(),listNode);
      }
   else
      {
      if (cg->traceBCDCodeGen())
         traceMsg(comp(),"\t\tlistNode %s (%p) not already present in condCleanLoadList : add to list\n",
            listNode->getOpCode().getName(),listNode);
      _BCDConditionalCleanLoadList.add(listNode);
      }
   }

void InMemoryLoadStoreMarking::processBCDAggrNodeList(List<TR::Node> &nodeList, TR::Node *defNode, TR_NodeListTypes listType)
   {
   if (!nodeList.isEmpty())
      {
      if (cg->traceBCDCodeGen())
         traceMsg(comp(),"\titerate through non-empty %s\n",getName(listType));
      bool isRegularLoadList = (listType == LoadList);
      bool isConditionalCleanLoadList = (listType == ConditionalCleanLoadList);
      bool isAnyTypeOfLoadList = (isRegularLoadList || isConditionalCleanLoadList);
      ListIterator<TR::Node> listIt(&nodeList);
      for (TR::Node *listNode=listIt.getFirst(); listNode; listNode = listIt.getNext())
         {
         TR_ASSERT((isAnyTypeOfLoadList && listNode->getOpCode().isBCDLoadVar()) || listNode->getOpCode().isBCDStore(),
            "only bcd/aggr loadVars or stores should be in the list (%s (%p))\n",listNode->getOpCode().getName(),listNode);
         if (isAnyTypeOfLoadList ||
             listNode != defNode)   // skip the current store treetop node
            {
            bool defNodeHasSymRef = defNode->getOpCode().hasSymbolReference() && defNode->getSymbolReference();
            if (cg->traceBCDCodeGen())
               traceMsg(comp(),"\t\tintersect defNode %s (%p) #%d aliases with listNode %s (%p) #%d\n",
                  defNode->getOpCode().getName(),defNode,defNodeHasSymRef ? defNode->getSymbolReference()->getReferenceNumber() : -1,
                  listNode->getOpCode().getName(),listNode,listNode->getSymbolReference()->getReferenceNumber());

            if (!defNodeHasSymRef || defNode->getSymbolReference()->getUseDefAliases().contains(listNode->getSymbolReference(), comp()))
               {
               if (isAnyTypeOfLoadList)
                  {
                  // There are two possible list types (condClean and regular load list) that can reach here and two conditions (clean or not clean) so expressing 4 states as a table
                  //
                  //                           |   _BCDConditionalCleanLoadList (cleanList)  |   _BCDAggrLoadList (regList)
                  //---------------------------|---------------------------------------------|---------------------------------
                  //  isConditionalCleanLoad   |  1) add listNode to cleanList               |  1) add listNode to cleanList
                  //                           |                                             |  2) remove listNode from regList
                  // --------------------------|---------------------------------------------|----------------------------------
                  // !isConditionalCleanLoad   |  1) remove listNode from cleanList          |  1) remove listNode from regList
                  //

                  // first check if we are able to place listNode in the conditional load list vs more conservatively just marking setSkipCopyOnLoad to false
                  if (isConditionalCleanLoad(listNode, defNode))
                     {
                     addToConditionalCleanLoadList(listNode);
                     if (isRegularLoadList)
                        {
                        if (cg->traceBCDCodeGen())
                           traceMsg(comp(),"\t\t\tremove listNode %s (%p) from regular load list\n",listNode->getOpCode().getName(),listNode);
                        nodeList.remove(listNode);
                        }
                     }
                  else
                     {
                     // this path may either be removing a load from the regular or conditional load list (in the latter case when some other type of def is possibly seen)
                     if (cg->traceBCDCodeGen())
                        traceMsg(comp(),"\t\t\tfound an intersection so remove listNode %s (%p) from %s and set setSkipCopyOnLoad flag to false (defNodeHasSymRef=%s)\n",
                           listNode->getOpCode().getName(),listNode,getName(listType),defNodeHasSymRef?"yes":"no");
                     listNode->setSkipCopyOnLoad(false);
                     nodeList.remove(listNode);
                     }
                  }
               else
                  {
                  if (cg->traceBCDCodeGen())
                     traceMsg(comp(),"\t\t\tfound an intersection so remove listNode %s (%p) from %s and set setSkipCopyOnStore flag to false (defNodeHasSymRef=%s)\n",
                        listNode->getOpCode().getName(),listNode,getName(listType),defNodeHasSymRef?"yes":"no");
                  TR_ASSERT(listType == StoreList,"expecting StoreList for listNode %s (%p)\n",listNode->getOpCode().getName(),listNode);
                  listNode->setSkipCopyOnStore(false);
                  nodeList.remove(listNode);
                  }
               }
            }
         else if (cg->traceBCDCodeGen())
            {
            traceMsg(comp(),"\t\tskipping current store treetop listNode %s (%p)\n",listNode->getOpCode().getName(),listNode);
            }
         }
      }
   }

void InMemoryLoadStoreMarking::clearAllLists()
   {
   clearLoadLists();
   _BCDAggrStoreList.deleteAll();
   }

void InMemoryLoadStoreMarking::clearLoadLists()
   {
   _BCDAggrLoadList.deleteAll();
   _BCDConditionalCleanLoadList.deleteAll();
   }

char *InMemoryLoadStoreMarking::_TR_NodeListTypeNames[NodeList_NumTypes] =
   {
   "LoadList",
   "ConditionalCleanLoadList",
   "StoreList"
   };

// ----------- BCDOpportunities start
// BCDOpportunities tracing has nothing to do with in memory load/store marking but this is a convenient place (late node walk) to do this
bool isBCDReductionOpportunity(TR::Node *node)
   {
   if (node->getOpCode().isLoadConst())
      return true;

   if (node->getOpCodeValue() == TR::i2pd || node->getOpCodeValue() == TR::l2pd)
      return true;

   if (node->getNumChildren() > 0 &&
       !node->getOpCode().isLoadVar() &&
       !node->getOpCode().isBasicPackedArithmetic() &&
       !node->getOpCode().isExponentiation() &&
       isBCDReductionOpportunity(node->getFirstChild()))
      {
      return true;
      }

   return false;
   }

void traceBCDOpportunities(TR::Node *node, TR::Compilation *comp)
   {
   if (!comp->cg()->traceBCDCodeGen())
      return;

   if (comp->getOption(TR_DisableBCDOppTracing))
      return;

   if (node->getOpCode().isBasicPackedArithmetic())
      {
      TR::Node *first = node->getFirstChild();
      TR::Node *second = node->getSecondChild();

      if (first == second)
         {
         traceMsg(comp,"x^x : found arith with identical children -- node %s (%p) with child (%p)\n",
            node->getOpCode().getName(),node,second);
         }

      if (isBCDReductionOpportunity(first) && isBCDReductionOpportunity(second))
         {
         traceMsg(comp,"x^x : binaryReduction possibility -- node %s p=%d (%p) with op1 %s p=%d, op2 %s p=%d - line_no=%d (%s)\n",
            node->getOpCode().getName(),node->getDecimalPrecision(),node,
            first->getOpCode().getName(),first->getDecimalPrecision(),
            second->getOpCode().getName(),second->getDecimalPrecision(),
            comp->getLineNumber(node),
            node->getDecimalPrecision()<=TR::getMaxSignedPrecision<TR::Int64>() ? "intOpp" : (node->getDecimalPrecision()<=TR::DataType::getMaxExtendedDFPPrecision() ?"dfpOpp" : "noOpp"));
         }
      }

   if (node->getOpCode().isBasicOrSpecialPackedArithmetic() ||
       node->getOpCodeValue() == TR::pdneg ||
       node->getOpCodeValue() == TR::pdabs ||
       node->getOpCode().isPackedArithmeticOverflowMessage() ||
       (node->getOpCode().isConversion() && node->getType().isIntegral() && node->getFirstChild()->getType().isAnyPacked()) ||
       (node->getOpCode().isIf() && node->getFirstChild()->getType().isAnyPacked()))
      {
      if (node->getFirstChild()->getOpCode().isSetSign() &&
          node->getFirstChild()->getFirstChild()->getOpCode().isConversion() &&
          node->getFirstChild()->getFirstChild()->getType().isAnyPacked() &&
          node->getFirstChild()->getFirstChild()->getFirstChild()->getType().isAnyZoned())
         {
         traceMsg(comp,"x^x : found ascii pack opp -- %s/%s/%s (%p/%p/%p)\n",
            node->getOpCode().getName(),
            node->getFirstChild()->getOpCode().getName(),
            node->getFirstChild()->getFirstChild()->getOpCode().getName(),
            node,
            node->getFirstChild(),
            node->getFirstChild()->getFirstChild());
         }
      }

   if (node->getOpCode().isConversion() &&
       node->getFirstChild()->getOpCode().isLoadConst() &&
       (node->getType().isLongDouble() || node->getFirstChild()->getType().isLongDouble()))
      {
      traceMsg(comp,"x^x : long double const conv -- %s (%p)\n",node->getOpCode().getName(),node);
      }

   if (node->getOpCode().isConversion() &&
       node->getType().isIntegral() &&
       node->getFirstChild()->getType().isBCD() &&
       node->getFirstChild()->getOpCode().isSetSign())
      {
      traceMsg(comp,"x^x : intAbs for setSign -- %s (%p) of %s (%p)\n",
         node->getOpCode().getName(),node,node->getFirstChild()->getOpCode().getName(),node->getFirstChild());
      }

   if ((node->getOpCodeValue() == TR::pd2l || node->getOpCodeValue() == TR::pd2i) &&
       node->getNumChildren() >= 1 &&
       isBCDReductionOpportunity(node->getFirstChild()))
      {
      traceMsg(comp,"x^x : binaryReduction possibility -- node %s (%p) with op1 %s p=%d - line_no=%d\n",
         node->getOpCode().getName(),node,
         node->getFirstChild()->getOpCode().getName(),node->getFirstChild()->getDecimalPrecision(),
         comp->getLineNumber(node));
      }

   // checking for lshl to see if the VP handler for this opt should have reduceLongOpToIntegerOp case
   if (node->getOpCodeValue() == TR::lshl)
      {
      traceMsg(comp,"x^x : lshl for VP opp -- %s (%p)\n",node->getOpCode().getName(),node);
      }

   if (node->getOpCode().isConversion() &&
       (node->getOpCode().isBinaryCodedDecimalOp()) &&
       node->getFirstChild()->getOpCode().isLoadConst())
      {
      traceMsg(comp,"x^x : BCD const conv -- %s (%p)\n",node->getOpCode().getName(),node);
      }

   if (node->getType().isLongDouble() && node->getOpCode().isArithmetic() && node->getNumChildren() == 2 &&
       node->getFirstChild()->getOpCode().isLoadConst() && node->getSecondChild()->getOpCode().isLoadConst())
      {
      traceMsg(comp,"x^x : long double const arith -- %s (%p)\n",node->getOpCode().getName(),node);
      }

   if (node->getOpCode().isBasicOrSpecialPackedArithmetic())
      {
      // setsign should be removed in this case as it isn't needed if first/second->getFirstChild() is already fixing up sign
      TR::Node *first = node->getFirstChild();
      TR::Node *second = node->getOpCode().isBasicPackedArithmetic() ? node->getSecondChild() : NULL;
      if (first->getOpCodeValue() == TR::pdSetSign && first->getFirstChild()->getOpCode().isBasicOrSpecialPackedArithmetic())
         {
         traceMsg(comp,"x^x : found arith with setsign first child over other arith -- node %s (%p), otherArith %s (%p)\n",
            node->getOpCode().getName(),node,first->getFirstChild()->getOpCode().getName(),first->getFirstChild());
         }
      else if (second && second->getOpCodeValue() == TR::pdSetSign && second->getFirstChild()->getOpCode().isBasicOrSpecialPackedArithmetic())
         {
         traceMsg(comp,"x^x : found arith with setsign second child over other arith -- node %s (%p), otherArith %s (%p)\n",
            node->getOpCode().getName(),node,second->getFirstChild()->getOpCode().getName(),second->getFirstChild());
         }
      }

   if (node->getOpCode().isPackedShift() && node->getFirstChild()->getOpCode().isPackedShift())
      {
      traceMsg(comp,"x^x : found shift %d over shift %d -- parent %s (%p), child %s (%p)\n",
         node->getDecimalAdjust(),node->getFirstChild()->getDecimalAdjust(),
         node->getOpCode().getName(),node,node->getFirstChild()->getOpCode().getName(),node->getFirstChild());
      }

   // should fold pdshl/pdsetsign to pdshlSetSign to get better codegen -- common reason for missing this is lack of folding with child refCount > 1
   if (node->getOpCode().isPackedShift() && !node->getOpCode().isSetSign() && node->getFirstChild()->getOpCode().isSetSign())
      {
      traceMsg(comp,"x^x : found shift over setSign -- parent %s (%p), child %s (%p)\n",
         node->getOpCode().getName(),node,node->getFirstChild()->getOpCode().getName(),node->getFirstChild());
      }

   if (node->getOpCode().isSetSign() && node->getFirstChild()->getOpCode().isSetSign())
      {
      traceMsg(comp,"x^x : found setsign over setsign -- parent %s (%p), child %s (%p)\n",
         node->getOpCode().getName(),node,node->getFirstChild()->getOpCode().getName(),node->getFirstChild());
      }

   if (node->getOpCode().isPackedLeftShift() && node->getOpCode().isSetSign())
      {
      traceMsg(comp,"x^x : found pdshlSetSign by %d (%s) -- node %s (%p)\n",
         node->getDecimalAdjust(),isOdd(node->getDecimalAdjust())?"odd":"even",node->getOpCode().getName(),node);
      }

   if (node->getOpCode().isConversion() && node->getType().isIntegral() &&
       node->getFirstChild()->getOpCode().isConversion() && node->getFirstChild()->getType().isAnyPacked() &&
       node->getFirstChild()->getFirstChild()->getType().isAnyZoned())
      {
      traceMsg(comp,"x^x : found zoned to packed to binary conversion (task 59856) - node %s (%p), child %s, grandchild %s\n",
              node->getOpCode().getName(),
              node,
              node->getFirstChild()->getOpCode().getName(),
              node->getFirstChild()->getFirstChild()->getOpCode().getName());
      }
   else if (node->getOpCode().isConversion() && node->getType().isAnyZoned() &&
            node->getFirstChild()->getOpCode().isConversion() && node->getFirstChild()->getType().isAnyPacked() &&
            node->getFirstChild()->getFirstChild()->getType().isIntegral())
      {
      traceMsg(comp,"x^x : found binary to packed to zoned (task 59856) - node %s (%p), child %s, grandchild %s\n",
              node->getOpCode().getName(),
              node,
              node->getFirstChild()->getOpCode().getName(),
              node->getFirstChild()->getFirstChild()->getOpCode().getName());
      }

   if (node->getOpCodeValue() == TR::arraycmpWithPad)
      {
      TR::Node *cmpChildOne = node->getChild(0);
      TR::Node *cmpLenOne = node->getChild(1);
      TR::Node *cmpChildTwo = node->getChild(2);
      TR::Node *cmpLenTwo = node->getChild(3);
      TR::Node *padChar = node->getChild(4);
      bool cmpChildOneIsLit = false;
      bool cmpChildTwoIsLit = false;
      if (cmpChildOneIsLit || cmpChildTwoIsLit)
         {
         traceMsg(comp,"x^x : found arraycmpWithPad with lit cmpOps (oneIsLit=%s,twoIsLit=%s) -- node %s (%p) lenOne %d, lenTwo %d, padCharIsLit=%s (0x%x) line_no=%d\n",
            cmpChildOneIsLit?"yes":"no",
            cmpChildTwoIsLit?"yes":"no",
            node->getOpCode().getName(),node,
            cmpLenOne->getOpCode().isLoadConst() ? (int32_t)cmpLenOne->get64bitIntegralValue() : -1,
            cmpLenTwo->getOpCode().isLoadConst() ? (int32_t)cmpLenTwo->get64bitIntegralValue() : -1,
            padChar?"yes":"no",
            padChar->getOpCode().isLoadConst() ? (int32_t)padChar->get64bitIntegralValue() : 0,
            comp->getLineNumber(node));
         }
      }

   // csubexp.cbl line_no=304 ADD EL15(5I, 5J, 5K) EL25(5I, 5J, 5L) GIVING 5A
   if (node->getOpCode().isConversion() &&
       node->getType().isAnyZoned() &&
       node->getFirstChild()->getType().isAnyPacked() &&
       node->getFirstChild()->getOpCode().isShift() &&
       node->getFirstChild()->getOpCode().isSetSign())
      {
      traceMsg(comp,"x^x : found pd2zdshlSetSign opp - node %s (%p) child %s (%p)\n",
         node->getOpCode().getName(),node,node->getFirstChild()->getOpCode().getName(),node->getFirstChild());
      }

   if ((node->getOpCodeValue() == TR::pdSetSign || node->getOpCode().isPackedModifyPrecision()) &&
        node->getFirstChild()->getOpCode().isConversion() && node->getFirstChild()->getFirstChild()->getType().isIntegral() &&
        node->getDecimalPrecision() < node->getFirstChild()->getDecimalPrecision() &&
        node->isEvenPrecision() && node->getFirstChild()->isEvenPrecision())
      {
      // in the below case both the pdModPrec and the l2pd will generate an NI to correct the top nibble when only the pdModPrec NI is needed
      // pdModPrec p = 4
      //    l2pd p = 18
      //
      traceMsg(comp,"x^x : found double even correction -- %s (%p) prec = %d with child %s (%p) prec = %d\n",
         node->getOpCode().getName(),
         node,
         node->getDecimalPrecision(),
         node->getFirstChild()->getOpCode().getName(),
         node->getFirstChild(),
         node->getFirstChild()->getDecimalPrecision());
      }

   // ddModifyPrecision <prec=4>
   //    pd2dd
   //       pdsub <prec=31 (len=16)>
   //
   // on < ARCH(11) the pd2ddEvaluator will have to use the longDouble conversion instruction as the double conv instruction only converts 15 digits
   // and >= 16 digits are needed
   // When > 16 digits are needed then an NILL to clear the 17 digit has to be done before returning the pd2dd result
   //
   // In this case it would be good to insert a pdModPrec to some odd amount (byte boundary) > the ddModPrec value under the pd2dd as this will avoid the
   // NILL and also loading more data then needed in the pd2ddEvaluator
   //
   if (!comp->cg()->supportsFastPackedDFPConversions() &&
       node->getOpCodeValue() == TR::ddModifyPrecision &&
       node->getFirstChild()->getOpCodeValue() == TR::pd2dd)
      {
      TR::Node *ddModPrec = node;
      TR::Node *pd2dd = ddModPrec->getFirstChild();
      TR::Node *pd2ddChild = pd2dd->getFirstChild();
      if (pd2ddChild->getDecimalPrecision() >= TR::DataType::getMaxLongDFPPrecision() &&
          ddModPrec->getDFPPrecision() < TR::DataType::getMaxLongDFPPrecision())
         {
         traceMsg(comp,"x^x : found larger srcLoad and/or redundant 17th digit correction about to happen in pd2ddEvaluator -- %s (%p) prec = %d with grandChild %s (%p) prec = %d on line_no=%d (offset %06X)\n",
            ddModPrec->getOpCode().getName(),
            ddModPrec,
            ddModPrec->getDFPPrecision(),
            pd2ddChild->getFirstChild()->getOpCode().getName(),
            pd2ddChild,
            pd2ddChild->getDecimalPrecision(),
            comp->getLineNumber(ddModPrec),
            comp->getLineNumber(ddModPrec));
         }
      }
/*
   if ((node->getOpCode().isDiv() || node->getOpCode().isRem()) &&
       node->getType().isIntegral() &&
       node->getSecondChild()->getOpCode().isLoadConst() &&
       isPositivePowerOfTen(node->getSecondChild()->get64bitIntegralValue()))
      {
      traceMsg(comp,"x^x : found %s with power of ten divisor -- node (%p), divisor %lld (%d digits)\n",
         node->getOpCode().getName(),node,node->getSecondChild()->get64bitIntegralValue(),trailingZeroes(node->getSecondChild()->get64bitIntegralValue()));
      }
*/

   }
// ----------- BCDOpportunities end
