/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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

#include "optimizer/MethodHandleTransformer.hpp"
#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/VMJ9.h"
#include "env/j9method.h"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/Checklist.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/TransformUtil.hpp"
#include "optimizer/PreExistence.hpp"
#include "infra/Cfg.hpp"
#include "infra/ILWalk.hpp"
#include "il/AutomaticSymbol.hpp"
#include "ras/Logger.hpp"

static void printObjectInfo(TR_MethodHandleTransformer::ObjectInfo *objectInfo, TR::Compilation *comp)
   {
   OMR::Logger *log = comp->log();
   int localIndex = 0;
   for (auto it = objectInfo->begin(); it != objectInfo->end(); it++)
      {
      if (*it != TR::KnownObjectTable::UNKNOWN)
         {
         log->printf("(local #%2d: obj%d)  ", localIndex, *it);
         }
      localIndex++;
      }
   if (localIndex > 0)
      log->println();
   }

static bool isKnownObject(TR::KnownObjectTable::Index objectInfo)
   {
   if (objectInfo != TR::KnownObjectTable::UNKNOWN)
      return true;
   return false;
   }

int32_t TR_MethodHandleTransformer::perform()
   {
   // Only do the opt for LambdaForm generated methods, and for for peeking ILGen, since currently
   // peeking ILGen is only triggered for MethodHandle/VarHandle-related cases to propagate object info
   TR_ResolvedMethod* currentMethod = comp()->getCurrentMethod();
   if (!comp()->fej9()->isLambdaFormGeneratedMethod(currentMethod) && !comp()->isPeekingMethod())
      return 0;

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   if (trace())
      {
      if (comp()->isPeekingMethod())
         comp()->log()->printf("Start transforming peeking method %s\n", currentMethod->signature(trMemory()));
      else
         comp()->log()->printf("Start transforming LambdaForm generated method %s\n", currentMethod->signature(trMemory()));
      }

   // Assign local index to parms, autos and temps (including pending push temps)
   assignLocalIndices();
   if (_numLocals == 0)
       return 0;

   // For each block, compute an ObjectInfo array for all address typed parms or autos
   _blockEndObjectInfos = new (comp()->trMemory()->currentStackRegion()) BlockResultMap(std::less<int32_t>(), comp()->trMemory()->currentStackRegion());

   /*
    * Do a reverse post-order traversal of the CFG as the best effort to figure out object info in one traverse.
    * If there exist one or more unvisited predecessors, don't propagate object info from any predecessor
    */

   TR::ReversePostorderSnapshotBlockIterator blockIt(comp()->getFlowGraph(), comp());

   // Set up object info for the first block
   //
   //Initialize type info for parms from prex arg for the entry block
   TR::Block *firstBlock = blockIt.currentBlock();
   ObjectInfo* firstBlockObjectInfo = getMethodEntryObjectInfo();
   if (trace())
      {
      comp()->log()->printf("Entry Block (block_%d) object Info:\n", firstBlock->getNumber());
      printObjectInfo(firstBlockObjectInfo, comp());
      }

   processBlockAndUpdateObjectInfo(firstBlock, firstBlockObjectInfo);
   (*_blockEndObjectInfos)[firstBlock->getNumber()] = firstBlockObjectInfo;
   ++blockIt;

   // Process the remaining blocks
   //
   while (blockIt.currentBlock())
      {
      TR::Block *block = blockIt.currentBlock();
      int32_t blockNum = block->getNumber();
      ObjectInfo* blockObjectInfo = blockStartObjectInfoFromPredecessors(block);
      // Walk the trees in the block, update object info and optimize trees based on object info
      processBlockAndUpdateObjectInfo(block, blockObjectInfo);
      (*_blockEndObjectInfos)[blockNum] = blockObjectInfo;

      ++blockIt;
      }
   return 0;
   }

// Walk treeteops, find stores to auto or temp
void TR_MethodHandleTransformer::collectAutosFromTrees(List<TR::SymbolReference>& autoSymRefs)
   {
   TR_BitVector symRefBV(comp()->getSymRefTab()->getNumSymRefs(), comp()->trMemory()->currentStackRegion());

   for (auto treetop = comp()->getMethodSymbol()->getFirstTreeTop(); treetop != NULL; treetop = treetop->getNextTreeTop())
      {
      auto ttNode = treetop->getNode();
      auto storeNode = ttNode->getStoreNode();
      if (storeNode &&
          storeNode->getOpCode().isStoreDirect() &&
          storeNode->getSymbol()->isAuto())
         {
         auto symRef = storeNode->getSymbolReference();
         auto symRefNum = symRef->getReferenceNumber();
         if (!symRefBV.isSet(symRefNum))
            {
            symRefBV.set(symRefNum);
            }
         }
      }

   // Add sym refs to list
   TR_BitVectorIterator bvi(symRefBV);
   while (bvi.hasMoreElements())
      {
      auto symRefNum = bvi.getNextElement();
      auto symRef = comp()->getSymRefTab()->getSymRef(symRefNum);
      autoSymRefs.add(symRef);
      }
   }

void TR_MethodHandleTransformer::assignLocalIndices()
   {
   OMR::Logger *log = comp()->log();

   // Assign local index to parms
   ListIterator<TR::ParameterSymbol> parms(&comp()->getMethodSymbol()->getParameterList());
   for (TR::ParameterSymbol * p = parms.getFirst(); p; p = parms.getNext())
      {
      if (p->getDataType() == TR::Address)
         {
         logprintf(trace(), log, "Local #%2d is symbol %p <parm %d>\n", _numLocals, p, p->getSlot());
         p->setLocalIndex(_numLocals++);
         }
      }

   // getAutoSymRefs from method symbol is missing some temp sym refs created during block splitting.
   // Adding those sym refs to the list is causing test failures, as a workaround, walk the trees to
   // find all autos (auto, temps and pending push temps) in the method
   //
   List<TR::SymbolReference> autoSymRefs(comp()->trMemory()->currentStackRegion());
   collectAutosFromTrees(autoSymRefs);

   ListIterator<TR::SymbolReference> autosIt(&autoSymRefs);
   for (TR::SymbolReference* symRef = autosIt.getFirst(); symRef; symRef = autosIt.getNext())
      {
      TR::AutomaticSymbol *p = symRef->getSymbol()->getAutoSymbol();
      if (p && p->getDataType() == TR::Address)
         {
         logprintf(trace(), log, "Local #%2d is symbol %p [#%d]\n", _numLocals, p, symRef->getReferenceNumber());
         p->setLocalIndex(_numLocals++);
         }
      }
   }

TR_MethodHandleTransformer::ObjectInfo*
TR_MethodHandleTransformer::getMethodEntryObjectInfo()
   {
   TR_PrexArgInfo *argInfo = comp()->getCurrentInlinedCallArgInfo();
   ObjectInfo* objectInfo = new (comp()->trMemory()->currentStackRegion()) ObjectInfo(_numLocals, static_cast<int>(TR::KnownObjectTable::UNKNOWN), comp()->trMemory()->currentStackRegion());
   if (argInfo)
      {
      TR_ResolvedMethod* currentMethod = comp()->getCurrentMethod();
      int32_t numArgs = currentMethod->numberOfParameters();

      TR_ASSERT(argInfo->getNumArgs() == numArgs, "Number of prex arginfo %d doesn't match method parm number %d", argInfo->getNumArgs(), numArgs);

      ListIterator<TR::ParameterSymbol> parms(&comp()->getMethodSymbol()->getParameterList());
      for (TR::ParameterSymbol *p = parms.getFirst(); p != NULL; p = parms.getNext())
         {
         int32_t ordinal = p->getOrdinal();
         TR_PrexArgument *arg = argInfo->get(ordinal);
         if (arg && arg->getKnownObjectIndex() != TR::KnownObjectTable::UNKNOWN)
            {
            (*objectInfo)[p->getLocalIndex()] = arg->getKnownObjectIndex();
            logprintf(trace(), comp()->log(), "Local #%2d is parm %d is obj%d\n", p->getLocalIndex(), ordinal, arg->getKnownObjectIndex());
            }
         }
      }

   return objectInfo;
   }

TR_MethodHandleTransformer::ObjectInfo*
TR_MethodHandleTransformer::blockStartObjectInfoFromPredecessors(TR::Block* block)
   {
   OMR::Logger *log = comp()->log();

   auto blockNum = block->getNumber();
   // If there exist an exception edge coming into this block, we don't know the object info at
   // the time of the exception, thus initialize the object info for all locals to unknown for
   // this block
   //
   if (block->isCatchBlock())
      {
      logprintf(trace(), log, "block_%d has exception predecessor, initialize all local slots to unknown object\n", blockNum);

      return new (comp()->trMemory()->currentStackRegion()) ObjectInfo(_numLocals, static_cast<int>(TR::KnownObjectTable::UNKNOWN), comp()->trMemory()->currentStackRegion());
      }

   // If there exists one or more predecessor unvisited, the unvisited predecessor must be from a back edge.
   // Don't propagate as we don't know what might happen from the predecessor.
   //
   TR_PredecessorIterator pi(block);
   for (TR::CFGEdge *edge = pi.getFirst(); edge != NULL; edge = pi.getNext())
      {
      TR::Block *fromBlock = toBlock(edge->getFrom());
      int32_t fromBlockNum = fromBlock->getNumber();
      if (_blockEndObjectInfos->find(fromBlockNum) == _blockEndObjectInfos->end())
         {
         logprintf(trace(), log, "Predecessor block_%d hasn't been visited yet, no object info is propagated for block_%d\n", fromBlockNum, blockNum);

         return new (comp()->trMemory()->currentStackRegion()) ObjectInfo(_numLocals, static_cast<int>(TR::KnownObjectTable::UNKNOWN), comp()->trMemory()->currentStackRegion());
         }
      }

   // Get block start object info by merging object info from predecessors
   ObjectInfo* objectInfo = NULL;
   for (TR::CFGEdge *edge = pi.getFirst(); edge != NULL; edge = pi.getNext())
      {
      TR::Block *fromBlock = toBlock(edge->getFrom());
      int32_t fromBlockNum = fromBlock->getNumber();
      ObjectInfo* fromBlockObjectInfo = (*_blockEndObjectInfos)[fromBlockNum];
      if (!objectInfo)
         objectInfo = new (comp()->trMemory()->currentStackRegion()) ObjectInfo(*fromBlockObjectInfo);
      else
         mergeObjectInfo(objectInfo, fromBlockObjectInfo);
      }

   if (trace())
      {
      log->printf("Block start object info for block_%d is\n", blockNum);
      printObjectInfo(objectInfo, comp());
      }

   return objectInfo;
   }

// Merge second ObjectInfo into the first one
// The merge does an intersect, only entries with the same value will be kept
//
void TR_MethodHandleTransformer::mergeObjectInfo(ObjectInfo *first, ObjectInfo *second)
   {
   OMR::Logger *log = comp()->log();

   if (trace())
      {
      log->prints("Object info before merging:\n");
      printObjectInfo(first, comp());
      }

   bool changed = false;
   for (int i = 0; i < _numLocals; i++)
      {
      TR::KnownObjectTable::Index firstObj = (*first)[i];
      TR::KnownObjectTable::Index secondObj = (*second)[i];

      if (firstObj != secondObj)
         {
         (*first)[i] = TR::KnownObjectTable::UNKNOWN;
         }

      if (firstObj != (*first)[i])
         changed = true;
      }

   if (trace())
      {
      if (changed)
         {
         log->prints("Object info after merging:\n");
         printObjectInfo(first, comp());
         }
      else
         log->prints("Object info is not changed after merging\n");
      }
   }

void
TR_MethodHandleTransformer::computeObjectInfoOfNode(TR::TreeTop *tt, TR::Node *node)
   {
   OMR::Logger *log = comp()->log();

   if (getObjectInfoOfNode(node) != TR::KnownObjectTable::UNKNOWN)
      {
      return; // already immediately available by inspecting the node
      }

   if (!node->getOpCode().hasSymbolReference())
      {
      return; // no known object, nothing to do
      }

   auto symRef = node->getSymbolReference();
   if (symRef->isUnresolved())
      {
      return; // no known object, nothing to do
      }

   auto knot = comp()->getKnownObjectTable();
   if (knot == NULL)
      {
      return; // no known object, nothing to do
      }

   logprintf(trace(), log, "Determining object info of n%dn\n", node->getGlobalIndex());

   TR::KnownObjectTable::Index koi = TR::KnownObjectTable::UNKNOWN;
   auto symbol = symRef->getSymbol();
   if (node->getOpCode().isLoadDirect() &&
       symbol->isAutoOrParm())
      {
      koi = (*_currentObjectInfo)[symbol->getLocalIndex()];
      }
   else if (node->getOpCode().isCall()
            && !symbol->castToMethodSymbol()->isHelper())
      {
      auto rm = symbol->castToMethodSymbol()->getMandatoryRecognizedMethod();
      switch (rm)
         {
         case TR::java_lang_invoke_DirectMethodHandle_internalMemberName:
         case TR::java_lang_invoke_DirectMethodHandle_internalMemberNameEnsureInit:
            {
            auto mhIndex = getObjectInfoOfNode(node->getFirstArgument());
            if (isKnownObject(mhIndex) && !knot->isNull(mhIndex))
               {
               auto mnIndex = comp()->fej9()->getMemberNameFieldKnotIndexFromMethodHandleKnotIndex(comp(), mhIndex, "member");
               logprintf(trace(), log, "Get DirectMethodHandle.member known object %d, update node n%dn known object\n", mnIndex, node->getGlobalIndex());

               koi = mnIndex;
               }

            break;
            }

         case TR::java_lang_invoke_DirectMethodHandle_constructorMethod:
            {
            auto mhIndex = getObjectInfoOfNode(node->getFirstArgument());
            if (isKnownObject(mhIndex) && !knot->isNull(mhIndex))
               {
               auto mnIndex = comp()->fej9()->getMemberNameFieldKnotIndexFromMethodHandleKnotIndex(comp(), mhIndex, "initMethod");
               logprintf(trace(), log, "Get DirectMethodHandle.initMethod known object %d, update node n%dn known object\n", mnIndex, node->getGlobalIndex());

               koi = mnIndex;
               }

            break;
            }

#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
         case TR::java_lang_invoke_DelegatingMethodHandle_getTarget:
            {
            TR::KnownObjectTable::Index dmhIndex =
               getObjectInfoOfNode(node->getArgument(0));

            koi = comp()->fej9()->delegatingMethodHandleTarget(
               comp(), dmhIndex, trace());
            break;
            }
#endif
         case TR::java_lang_invoke_Invokers_checkVarHandleGenericType:
            {
            auto vhIndex = getObjectInfoOfNode(node->getFirstArgument());
            auto adIndex = getObjectInfoOfNode(node->getLastChild());
            if (knot
               && isKnownObject(adIndex)
               && isKnownObject(vhIndex)
               && !knot->isNull(vhIndex)
               && !knot->isNull(adIndex))
               {
               auto mhIndex = comp()->fej9()->getMethodHandleTableEntryIndex(comp(), vhIndex, adIndex);
               logprintf(trace(), log, "Invokers_checkVarHandleGenericType with known VarHandle object %d, updating node n%dn with known MH object %d from MH table\n", vhIndex, node->getGlobalIndex(), mhIndex);
               koi = mhIndex;
               }
            break;
            }

         default:
            break;
         }
      }

   if (isKnownObject(koi)
       && !knot->isNull(koi)
       && performTransformation(
            comp(),
            "%snode n%un [%p] is obj%d\n",
            optDetailString(),
            node->getGlobalIndex(),
            node,
            koi))
      {
      node->setKnownObjectIndex(koi);
      }
   }

TR::KnownObjectTable::Index
TR_MethodHandleTransformer::getObjectInfoOfNode(TR::Node *node)
   {
   TR_ASSERT_FATAL_WITH_NODE(
      node,
      node->getType() == TR::Address,
      "Can't have object info on non-address type node");

   if (node->hasKnownObjectIndex())
      {
      return node->getKnownObjectIndex();
      }

   if (!node->getOpCode().hasSymbolReference())
      {
      return TR::KnownObjectTable::UNKNOWN;
      }

   return node->getSymbolReference()->getKnownObjectIndex(); // possibly UNKNOWN
   }

// Store to local variable will change object info
// Update _currentObjectInfo
void
TR_MethodHandleTransformer::visitStoreToLocalVariable(TR::TreeTop* tt, TR::Node* node)
   {
   TR::Node *rhs = node->getFirstChild();
   TR::Symbol *local = node->getSymbolReference()->getSymbol();
   if (rhs->getDataType().isAddress())
      {
      // Get object info of the rhs
      TR::KnownObjectTable::Index newObject = getObjectInfoOfNode(rhs);
      logprintf(trace(), comp()->log(), "rhs of store n%dn is obj%d\n", node->getGlobalIndex(), newObject);

      TR::KnownObjectTable::Index oldObject = (*_currentObjectInfo)[local->getLocalIndex()];
      if (newObject != oldObject && trace())
         {
         comp()->log()->printf("Local #%2d obj%d -> obj%d at node n%dn\n", local->getLocalIndex(), oldObject, newObject,  node->getGlobalIndex());
         }

      (*_currentObjectInfo)[local->getLocalIndex()] = newObject;
      }
   }

// Visit indirect load, discover known object by folding the load if applicable
void TR_MethodHandleTransformer::visitIndirectLoad(TR::TreeTop* tt, TR::Node* node)
   {
   OMR::Logger *log = comp()->log();
   auto symRef = node->getSymbolReference();
   if (symRef->hasKnownObjectIndex())
      {
      logprintf(trace(), log, "Indirect load n%dn is obj%d\n", node->getGlobalIndex(), symRef->getKnownObjectIndex());
      return;
      }

   auto symbol = node->getSymbol();
   if (!symRef->isUnresolved() && symbol &&
       (symbol->isFinal() || symbol->isArrayShadowSymbol()))
      {
      auto baseNode = symbol->isArrayShadowSymbol() ? node->getFirstChild()->getFirstChild() : node->getFirstChild();
      auto baseSymRef = baseNode->getSymbolReference();
      TR::KnownObjectTable::Index baseObj = getObjectInfoOfNode(baseNode);
      logprintf(trace(), log, "base object for indirect load n%dn is obj%d\n", node->getGlobalIndex(), baseObj);

      auto knot = comp()->getKnownObjectTable();
      if (knot && isKnownObject(baseObj) && !knot->isNull(baseObj))
         {
         // Since address node is not null, remove nullchk
         // This step is needed before transforming indirect load, as the indirect load can be
         // transformed into a const node, the base node will be removed
         //
         if (tt->getNode()->getOpCode().isNullCheck())
            {
            if (!performTransformation(comp(), "%sChange NULLCHK node n%dn to treetop\n", optDetailString(), tt->getNode()->getGlobalIndex()))
               return;
            TR::Node::recreate(tt->getNode(), TR::treetop);
            }

         // Have to improve the regular array-shadow to immutable-array-shadow in order to fold it
         if (symbol->isArrayShadowSymbol() && knot->isArrayWithConstantElements(baseObj))
            {
            TR::SymbolReference* improvedSymRef = comp()->getSymRefTab()->findOrCreateImmutableArrayShadowSymbolRef(symbol->getDataType());
            node->setSymbolReference(improvedSymRef);
            logprintf(trace(), log, "Improve regular array-shadow to immutable-array-shadow for n%dn\n", node->getGlobalIndex());
            }

         TR::Node* removedNode = NULL;
         bool succeed = TR::TransformUtil::transformIndirectLoadChain(comp(), node, baseNode, baseObj, &removedNode);
         if (!succeed && trace())
            log->printf("Failed to fold indirect load n%dn from base object obj%d\n", node->getGlobalIndex(), baseObj);
         else if (removedNode)
            {
            removedNode->recursivelyDecReferenceCount();
            }
         }
      }
   }

// Visit a call node, compute its result or transform the call node with current object info
//
void TR_MethodHandleTransformer::visitCall(TR::TreeTop* tt, TR::Node* node)
   {
   auto knot = comp()->getKnownObjectTable();
   TR::RecognizedMethod rm = node->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod();
   switch (rm)
      {
      case TR::java_lang_invoke_MethodHandle_invokeBasic:
         process_java_lang_invoke_MethodHandle_invokeBasic(tt, node);
         break;
      case TR::java_lang_invoke_MethodHandle_linkToSpecial:
      case TR::java_lang_invoke_MethodHandle_linkToVirtual:
      case TR::java_lang_invoke_MethodHandle_linkToStatic:
         process_java_lang_invoke_MethodHandle_linkTo(tt, node);
         break;
      case TR::java_lang_invoke_Invokers_checkCustomized:
         process_java_lang_invoke_Invokers_checkCustomized(tt, node);
         break;
      case TR::java_lang_invoke_Invokers_checkExactType:
         process_java_lang_invoke_Invokers_checkExactType(tt, node);
         break;
      case TR::java_lang_invoke_Invokers_checkVarHandleGenericType:
         process_java_lang_invoke_Invokers_checkVarHandleGenericType(tt, node);
         break;

      default:
         break;
      }
   }

// Visit a node may change the object info
//
void
TR_MethodHandleTransformer::visitNode(TR::TreeTop* tt, TR::Node* node, TR::NodeChecklist &visitedNodes)
   {
   if (visitedNodes.contains(node))
      {
      return;
      }

   visitedNodes.add(node);

   if (trace() && node == tt->getNode())
      {
      comp()->log()->printf("Looking at treetop node n%dn\n", node->getGlobalIndex());
      }

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      visitNode(tt, node->getChild(i), visitedNodes);
      }

   if (node->getType() == TR::Address)
      {
      computeObjectInfoOfNode(tt, node);
      }

   if (node->getOpCode().isStoreDirect() && node->getSymbolReference()->getSymbol()->isAutoOrParm() && node->getType() == TR::Address)
      {
      visitStoreToLocalVariable(tt, node);
      }
   else if (node->getOpCode().isLoadIndirect() && node->getType() == TR::Address)
      {
      visitIndirectLoad(tt, node);
      }
   else if (node->getOpCode().isCall())
      {
      visitCall(tt, node);
      }
   }

void
TR_MethodHandleTransformer::processBlockAndUpdateObjectInfo(TR::Block *block, TR_MethodHandleTransformer::ObjectInfo *blockStartObjectInfo)
   {
   _currentObjectInfo = blockStartObjectInfo;
   TR::NodeChecklist visitedNodes(comp());

   if (trace())
      {
      comp()->log()->printf("Start processing block_%d\n", block->getNumber());
      printObjectInfo(_currentObjectInfo, comp());
      }

   // Find stores to auto, and calculate value of the auto after the store
   // If the value is a load of final field, try fold the final field
   // If the value is result of a call, try compute the call result
   //
   for (TR::TreeTop *tt = block->getEntry(); tt != block->getExit(); tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      visitNode(tt, node, visitedNodes);
      }

   if (trace())
      {
      comp()->log()->printf("End processing block_%d\n", block->getNumber());
      printObjectInfo(_currentObjectInfo, comp());
      }
   }

const char *
TR_MethodHandleTransformer::optDetailString() const throw()
   {
   return "O^O METHODHANDLE TRANSFORMER: ";
   }

void
TR_MethodHandleTransformer::process_java_lang_invoke_MethodHandle_invokeBasic(TR::TreeTop* tt, TR::Node* node)
   {
   auto mhNode = node->getFirstArgument();
   TR::KnownObjectTable::Index objIndex = getObjectInfoOfNode(mhNode);
   logprintf(trace(), comp()->log(), "MethodHandle is obj%d\n", objIndex);

   auto knot = comp()->getKnownObjectTable();
   bool transformed = false;
   if (isKnownObject(objIndex) && knot && !knot->isNull(objIndex))
      transformed = TR::TransformUtil::refineMethodHandleInvokeBasic(comp(), tt, node, objIndex, trace());

   if (!transformed)
      {
      TR::DebugCounter::prependDebugCounter(comp(),
                                            TR::DebugCounter::debugCounterName(comp(),
                                                                               "MHUnknownObj/invokeBasic/(%s %s)",
                                                                               comp()->signature(),
                                                                               comp()->getHotnessName(comp()->getMethodHotness())),
                                                                               tt);
      }
   }

void
TR_MethodHandleTransformer::process_java_lang_invoke_MethodHandle_linkTo(TR::TreeTop* tt, TR::Node* node)
   {
   auto mnNode = node->getLastChild();
   TR::KnownObjectTable::Index objIndex = getObjectInfoOfNode(mnNode);
   logprintf(trace(), comp()->log(), "MemberName is obj%d\n", objIndex);

   auto knot = comp()->getKnownObjectTable();
   bool transformed = false;
   if (knot && isKnownObject(objIndex) && !knot->isNull(objIndex))
      transformed = TR::TransformUtil::refineMethodHandleLinkTo(comp(), tt, node, objIndex, trace());

   if (!transformed)
      {
      TR::DebugCounter::prependDebugCounter(comp(),
                                            TR::DebugCounter::debugCounterName(comp(),
                                                                               "MHUnknownObj/linkTo/(%s %s)",
                                                                               comp()->signature(),
                                                                               comp()->getHotnessName(comp()->getMethodHotness())),
                                                                               tt);
      }
   }

/*
Transforms calls to java/lang/invoke/Invokers.checkExactType to the more performant ZEROCHK.

Blocks before transformation: ==>

start Block_A
...
treetop
        call  java/lang/invoke/Invokers.checkExactType(Ljava/lang/invoke/MethodHandle;Ljava/lang/invoke/MethodType;)
           ==>aload
           ==>aload
...
end Block_A

Blocks after transformation: ==>

If MethodHandle and expected type are known objects at compile time and match: ==>

start Block_A
...
treetop
        PassThrough
          aload <MethodHandle>
...
end Block_A

else: ==>

start Block_A
...
treetop
        ZEROCHK
          acmpeq
             ==>aload <expected type>
             aloadi <MethodHandle.type>
               ==>aload <MethodHandle>
...
end Block_A

*/
void
TR_MethodHandleTransformer::process_java_lang_invoke_Invokers_checkExactType(TR::TreeTop* tt, TR::Node* node)
   {
   auto methodHandleNode = node->getArgument(0);
   auto expectedTypeNode = node->getArgument(1);
   TR_J9VMBase* fej9 = static_cast<TR_J9VMBase*>(comp()->fe());

   TR::KnownObjectTable::Index mhIndex = getObjectInfoOfNode(methodHandleNode);
   TR::KnownObjectTable::Index expectedTypeIndex = getObjectInfoOfNode(expectedTypeNode);
   if (isKnownObject(mhIndex) && isKnownObject(expectedTypeIndex))
      {
      bool typesMatch = fej9->isMethodHandleExpectedType(comp(), mhIndex, expectedTypeIndex);

      if (typesMatch && performTransformation(comp(), "%sChanging checkExactType call node n%dn to PassThrough\n", optDetailString(), node->getGlobalIndex()))
         {
         TR::TransformUtil::transformCallNodeToPassThrough(this, node, tt, node->getFirstArgument());
         return;
         }
      }
   if (!performTransformation(comp(), "%sChanging checkExactType call node n%dn to ZEROCHK\n", optDetailString(), node->getGlobalIndex()))
      return;
   uint32_t typeOffset = fej9->getInstanceFieldOffsetIncludingHeader("Ljava/lang/invoke/MethodHandle;", "type", "Ljava/lang/invoke/MethodType;", comp()->getCurrentMethod());
   auto typeSymRef = comp()->getSymRefTab()->findOrFabricateShadowSymbol(comp()->getMethodSymbol(),
                                                                                         TR::Symbol::Java_lang_invoke_MethodHandle_type,
                                                                                         TR::Address,
                                                                                         typeOffset,
                                                                                         false,
                                                                                         true,
                                                                                         true,
                                                                                         "java/lang/invoke/MethodHandle.type Ljava/lang/invoke/MethodType;");
   auto handleTypeNode = TR::Node::createWithSymRef(node, comp()->il.opCodeForIndirectLoad(TR::Address), 1, methodHandleNode, typeSymRef);
   auto nullCheckSymRef = comp()->getSymRefTab()->findOrCreateNullCheckSymbolRef(comp()->getMethodSymbol());
   auto nullCheckNode = TR::Node::createWithSymRef(node, TR::NULLCHK, 1, handleTypeNode, nullCheckSymRef);
   tt->insertBefore(TR::TreeTop::create(comp(), nullCheckNode));
   auto cmpEqNode = TR::Node::create(node, TR::acmpeq, 2, expectedTypeNode, handleTypeNode);
   TR::Node* zerochkNode = TR::Node::createWithSymRef(TR::ZEROCHK, 1, 1,
                                                       cmpEqNode,
                                                       comp()->getSymRefTab()->findOrCreateMethodTypeCheckSymbolRef(comp()->getMethodSymbol()));
   zerochkNode->setByteCodeInfo(node->getByteCodeInfo());
   tt->insertBefore(TR::TreeTop::create(comp(), zerochkNode));
   TR::TransformUtil::transformCallNodeToPassThrough(this, node, tt, node->getFirstArgument());
   }

/*
java/lang/invoke/Invokers.checkCustomized is redundant if its argument is a known object. This transformation
transforms calls to java/lang/invoke/Invokers.checkCustomized to a PassThrough.

Blocks before transformation: ==>

start Block_A
...
treetop
        call  java/lang/invoke/Invokers.checkCustomized(Ljava/lang/invoke/MethodHandle;)V
          aload  <MethodHandle>
...
end Block_A

Blocks after transformation ==>
start Block_A
...
treetop
        PassThrough
          aload <MethodHandle>
...
end Block_A

*/
void
TR_MethodHandleTransformer::process_java_lang_invoke_Invokers_checkCustomized(TR::TreeTop* tt, TR::Node* node)
   {
   TR::KnownObjectTable::Index objIndex = getObjectInfoOfNode(node->getFirstArgument());
   auto knot = comp()->getKnownObjectTable();
   if (isKnownObject(objIndex) && knot && !knot->isNull(objIndex))
      {
      if (!performTransformation(comp(), "%sRemoving checkCustomized call node n%dn as it is now redundant as MethodHandle has known object index\n", optDetailString(), node->getGlobalIndex()))
         return;
      TR::TransformUtil::transformCallNodeToPassThrough(this, node, tt, node->getFirstArgument());
      }
   }

/*
When the VarHandle object and the AccessDescriptor objects are known, we can perform a compile-time lookup of the corresponding
entry in the VarHandle's MethodHandle table. This function replaces Invokers.checkVarHandleGenericType call nodes with a
aloadi node performing the load of the MethodHandle object that would have been returned by Invokers.checkVarHandleGenericType.
This is achieved through inserting trees that perform loads of the field containing the MethodHandle table. The various checks that
Invokers.checkVarHandleGenericType would have performed are done in TR_J9VMBase::getMethodHandleTableEntryIndex which returns the
object index of the MethodHandle entry in the table if the checks succeed, following which the Invokers.checkVarHandleGenericType
can be folded.

Blocks before transformation ==>
start Block_A
...
treetop
acall  java/lang/invoke/Invokers.checkVarHandleGenericType(Ljava/lang/invoke/VarHandle;Ljava/lang/invoke/VarHandle$AccessDescriptor;)Ljava/lang/invoke/MethodHandle;
   aload  <VarHandle>
   aload  <AccessDescriptor>
...
astore  <auto slot n>
   ==>acall <MethodHandle returned by checkVarHandleGenericType>
...
end Block_A

Blocks after transformation (JAVA_SPEC_VERSION <= 17, compressedRefs) ==>
start Block_A
...
compressedRefs
  aloadi  java/lang/invoke/VarHandle.typesAndInvokers Ljava/lang/invoke/VarHandle$TypesAndInvokers;
    aload  (node known obj#) <VarHandle>
  lconst 0 (highWordZero X==0 X>=0 X<=0 )
compressedRefs
  aloadi  java/lang/invoke/VarHandle$TypesAndInvokers.methodHandle_table [Ljava/lang/invoke/MethodHandle;
    ==>aloadi
compressedRefs
  aloadi  unknown field [# Shadow] (known obj#) <MethodHandle>
    aladd (internalPtr )
        ==>aloadi  <VarHandle$TypesAndInvokers.methodHandle_table>
      ladd (X>=0 )
        lshl
          i2l
            iconst 0 (X==0 X>=0 X<=0 )
          iconst 2 (X!=0 X>=0 )
        lconst 8 (highWordZero X!=0 X>=0 )
  lconst 0 (highWordZero X==0 X>=0 X<=0 )
treetop
  ==>aloadi <MethodHandle object>
astore  <auto slot n>
  ==>aloadi <MethodHandle object>
...
end Block_A

For JDK 21 and above, the trees would look similar to the above, but not require an additional load of the
VarHandle$TypesAndInvokers field as the MethodHandle table exists as a field of the VarHandle object.
*/
void
TR_MethodHandleTransformer::process_java_lang_invoke_Invokers_checkVarHandleGenericType(TR::TreeTop* tt, TR::Node* node)
   {
   static const bool disableFoldingVHGenType = feGetEnv("TR_disableFoldingVHGenType") != NULL;
   if (disableFoldingVHGenType)
      {
      return;
      }

   auto vhIndex = getObjectInfoOfNode(node->getFirstArgument());
   auto adIndex = getObjectInfoOfNode(node->getLastChild());
   auto knot = comp()->getKnownObjectTable();
   if (knot == NULL
      || !isKnownObject(adIndex)
      || !isKnownObject(vhIndex)
      || knot->isNull(vhIndex)
      || knot->isNull(adIndex))
      {
      return;
      }

   auto mhIndex = comp()->fej9()->getMethodHandleTableEntryIndex(comp(), vhIndex, adIndex);
   int32_t mhEntryIndex = comp()->fej9()->getVarHandleAccessDescriptorMode(comp(), adIndex);
   if (mhIndex == TR::KnownObjectTable::UNKNOWN || mhEntryIndex < 0)
      {
      return;
      }

   if (!performTransformation(
         comp(),
         "%sReplacing Invokers.checkVarHandleGenericType call n%un [%p] "
         "with VH obj%d MH table entry obj%d\n",
         optDetailString(),
         node->getGlobalIndex(),
         node,
         vhIndex,
         mhIndex))
      {
      return;
      }

   // obtain symref(s) for creating load for the MH table base
#if JAVA_SPEC_VERSION <= 17
   uint32_t typesAndInvokersOffset = comp()->fej9()->getInstanceFieldOffsetIncludingHeader("Ljava/lang/invoke/VarHandle;", "typesAndInvokers", "Ljava/lang/invoke/VarHandle$TypesAndInvokers;", comp()->getCurrentMethod());
   const char * tisFieldNameAndSig = "java/lang/invoke/VarHandle.typesAndInvokers Ljava/lang/invoke/VarHandle$TypesAndInvokers;";
   TR::SymbolReference *tisSymRef = comp()->getSymRefTab()->findOrFabricateShadowSymbol(comp()->getMethodSymbol(),
                                                                           TR::Symbol::Java_lang_invoke_VarHandle_typesAndInvokers,
                                                                           TR::Address,
                                                                           typesAndInvokersOffset,
                                                                           false,
                                                                           false,
                                                                           false,
                                                                           tisFieldNameAndSig);
   TR::Node *tisNode = TR::Node::createWithSymRef(TR::aloadi, 1, 1, node->getFirstArgument(), tisSymRef);
   tisNode->copyByteCodeInfo(node);
   if (comp()->useCompressedPointers())
      {
      tt->insertBefore(TR::TreeTop::create(comp(), TR::Node::createCompressedRefsAnchor(tisNode)));
      }

   uint32_t mhTableOffset = comp()->fej9()->getInstanceFieldOffsetIncludingHeader("Ljava/lang/invoke/VarHandle$TypesAndInvokers;", "methodHandle_table", "[Ljava/lang/invoke/MethodHandle;", comp()->getCurrentMethod());
   const char * mhTableFieldNameAndSig = "java/lang/invoke/VarHandle$TypesAndInvokers.methodHandle_table [Ljava/lang/invoke/MethodHandle;";
   TR::Node *mhTableHolder = tisNode;
   bool mhTableIsFinal = true;
#else
   uint32_t mhTableOffset = comp()->fej9()->getInstanceFieldOffsetIncludingHeader("Ljava/lang/invoke/VarHandle;", "methodHandleTable", "[Ljava/lang/invoke/MethodHandle;", comp()->getCurrentMethod());
   const char * mhTableFieldNameAndSig = "java/lang/invoke/VarHandle.methodHandleTable [Ljava/lang/invoke/MethodHandle;";
   TR::Node *mhTableHolder = node->getFirstArgument();
   bool mhTableIsFinal = false;
#endif /* JAVA_SPEC_VERSION <= 17 */
   TR::SymbolReference *mhTableSymRef = comp()->getSymRefTab()->findOrFabricateShadowSymbol(comp()->getMethodSymbol(),
                                                                           TR::Symbol::Java_lang_invoke_VarHandle_methodHandleTable,
                                                                           TR::Address,
                                                                           mhTableOffset,
                                                                           false,
                                                                           false,
                                                                           mhTableIsFinal,
                                                                           mhTableFieldNameAndSig);
   TR::Node *mhTableNode = TR::Node::createWithSymRef(TR::aloadi, 1, 1, mhTableHolder, mhTableSymRef);
   mhTableNode->copyByteCodeInfo(node);
   if (comp()->useCompressedPointers())
      {
      tt->insertBefore(TR::TreeTop::create(comp(), TR::Node::createCompressedRefsAnchor(mhTableNode)));
      }

   // create node representing index for the load
   TR::Node *arrayIndexNode = TR::Node::create(TR::iconst, 0, mhEntryIndex);
   arrayIndexNode->copyByteCodeInfo(node);

   // calculate element address, transform call node into load from array, and improve MH symref with known object info
   TR::Node *mhAddressNode = J9::TransformUtil::calculateElementAddress(comp(), mhTableNode, arrayIndexNode, TR::Address);
   mhAddressNode->copyByteCodeInfo(node);

   anchorAllChildren(node, tt);
   node->removeAllChildren();
   TR::Node::recreateWithSymRef(node, TR::aloadi, comp()->getSymRefTab()->findOrCreateArrayShadowSymbolRef(TR::Address, mhTableNode));
   node->setNumChildren(1);
   node->setAndIncChild(0, mhAddressNode);
   TR::SymbolReference *improvedMHSymRef = comp()->getSymRefTab()->findOrCreateSymRefWithKnownObject(node->getSymbolReference(), mhIndex);
   node->setSymbolReference(improvedMHSymRef);

   // Insert spine check for arraylets
   if (TR::Compiler->om.canGenerateArraylets())
      {
      TR::SymbolReference * spineCHKSymRef = comp()->getSymRefTab()->findOrCreateArrayBoundsCheckSymbolRef(comp()->getMethodSymbol());
      TR::Node * spineCHKNode = TR::Node::create(node, TR::SpineCHK, 3);
      spineCHKNode->setAndIncChild(0, node);
      spineCHKNode->setAndIncChild(1, mhTableNode);
      spineCHKNode->setAndIncChild(2, arrayIndexNode);
      spineCHKNode->setSymbolReference(spineCHKSymRef);
      spineCHKNode->setSpineCheckWithArrayElementChild(true, comp());
      tt->insertBefore(TR::TreeTop::create(comp(), spineCHKNode));
      }

   if (comp()->useCompressedPointers())
      {
      tt->insertBefore(TR::TreeTop::create(comp(), TR::Node::createCompressedRefsAnchor(node)));
      }
   }
