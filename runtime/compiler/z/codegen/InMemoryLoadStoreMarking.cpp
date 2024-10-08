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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "codegen/InMemoryLoadStoreMarking.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/VMJ9.h"
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/Bit.hpp"


#define OPT_DETAILS "O^O IN MEMORY LOAD/STORE MARKING: "

void InMemoryLoadStoreMarking::perform()
   {

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

const char *InMemoryLoadStoreMarking::_TR_NodeListTypeNames[NodeList_NumTypes] =
   {
   "LoadList",
   "ConditionalCleanLoadList",
   "StoreList"
   };
