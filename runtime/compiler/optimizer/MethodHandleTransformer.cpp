/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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
#include "env/VMAccessCriticalSection.hpp"
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

static void printObjectInfo(TR_MethodHandleTransformer::ObjectInfo *objectInfo, TR::Compilation *comp)
   {
   int localIndex = 0;
   for (auto it = objectInfo->begin(); it != objectInfo->end(); it++)
      {
      if (*it != TR::KnownObjectTable::UNKNOWN)
         {
         traceMsg(comp, "(local #%2d: obj%d)  ", localIndex, *it);
         }
      localIndex++;
      }
   if (localIndex > 0)
      traceMsg(comp, "\n");
   }

static bool isKnownObject(TR::KnownObjectTable::Index objectInfo)
   {
   if (objectInfo != TR::KnownObjectTable::UNKNOWN)
      return true;
   return false;
   }

int32_t TR_MethodHandleTransformer::perform()
   {
   // Only do the opt for MethodHandle methods
   TR_ResolvedMethod* currentMethod = comp()->getCurrentMethod();
   if (!comp()->fej9()->isLambdaFormGeneratedMethod(currentMethod))
      return 0;

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   if (trace())
      traceMsg(comp(), "Start transforming LambdaForm generated method %s\n", currentMethod->signature(trMemory()));

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
      traceMsg(comp(), "Entry Block (block_%d) object Info:\n", firstBlock->getNumber());
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
   // Assign local index to parms
   ListIterator<TR::ParameterSymbol> parms(&comp()->getMethodSymbol()->getParameterList());
   for (TR::ParameterSymbol * p = parms.getFirst(); p; p = parms.getNext())
      {
      if (p->getDataType() == TR::Address)
         {
         if (trace())
            traceMsg(comp(), "Local #%2d is symbol %p <parm %d>\n", _numLocals, p, p->getSlot());
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
         if (trace())
            traceMsg(comp(), "Local #%2d is symbol %p [#%d]\n", _numLocals, p, symRef->getReferenceNumber());
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
            if (trace())
               traceMsg(comp(), "Local #%2d is parm %d is obj%d\n", p->getLocalIndex(), ordinal, arg->getKnownObjectIndex());
            }
         }
      }

   return objectInfo;
   }

TR_MethodHandleTransformer::ObjectInfo*
TR_MethodHandleTransformer::blockStartObjectInfoFromPredecessors(TR::Block* block)
   {
   auto blockNum = block->getNumber();
   // If there exist an exception edge coming into this block, we don't know the object info at
   // the time of the exception, thus initialize the object info for all locals to unknown for
   // this block
   //
   if (block->isCatchBlock())
      {
      if (trace())
         traceMsg(comp(), "block_%d has exception predecessor, initialize all local slots to unknown object\n", blockNum);

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
         if (trace())
            traceMsg(comp(), "Predecessor block_%d hasn't been visited yet, no object info is propagated for block_%d\n", fromBlockNum, blockNum);

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
      traceMsg(comp(), "Block start object info for block_%d is\n", blockNum);
      printObjectInfo(objectInfo, comp());
      }

   return objectInfo;
   }

// Merge second ObjectInfo into the first one
// The merge does an intersect, only entries with the same value will be kept
//
void TR_MethodHandleTransformer::mergeObjectInfo(ObjectInfo *first, ObjectInfo *second)
   {
   if (trace())
      {
      traceMsg(comp(), "Object info before merging:\n");
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
         traceMsg(comp(), "Object info after merging:\n");
         printObjectInfo(first, comp());
         }
      else
         traceMsg(comp(), "Object info is not changed after merging\n");
      }
   }

// Given a address type node, try to retrieve or compute its value
//
TR::KnownObjectTable::Index
TR_MethodHandleTransformer::getObjectInfoOfNode(TR::Node* node)
   {
   TR_ASSERT(node->getType() == TR::Address, "Can't have object info on non-address type node n%dn %p", node->getGlobalIndex(), node);

   if (trace())
      {
      traceMsg(comp(), "Looking for object info of n%dn\n", node->getGlobalIndex());
      }

   if (!node->getOpCode().hasSymbolReference())
      return TR::KnownObjectTable::UNKNOWN;

   auto symRef = node->getSymbolReference();
   auto symbol = symRef->getSymbol();

   if (symRef->isUnresolved())
      return TR::KnownObjectTable::UNKNOWN;

   if (symRef->hasKnownObjectIndex())
      return symRef->getKnownObjectIndex();

   if (node->getOpCode().isLoadDirect() &&
       symbol->isAutoOrParm())
      {
      if (trace())
         traceMsg(comp(), "getObjectInfoOfNode n%dn is load from auto or parm, local #%d\n", node->getGlobalIndex(), symbol->getLocalIndex());
      return (*_currentObjectInfo)[symbol->getLocalIndex()];
      }

   auto knot = comp()->getKnownObjectTable();
   if (knot &&
       node->getOpCode().isCall() &&
       !symbol->castToMethodSymbol()->isHelper())
      {
      auto rm = symbol->castToMethodSymbol()->getMandatoryRecognizedMethod();
      switch (rm)
        {
        case TR::java_lang_invoke_DirectMethodHandle_internalMemberName:
        case TR::java_lang_invoke_DirectMethodHandle_internalMemberNameEnsureInit:
           {
           auto mhIndex = getObjectInfoOfNode(node->getFirstArgument());
           if (knot && isKnownObject(mhIndex) && !knot->isNull(mhIndex))
              {
              auto mnIndex = comp()->fej9()->getMemberNameFieldKnotIndexFromMethodHandleKnotIndex(comp(), mhIndex, "member");
              if (trace())
                 traceMsg(comp(), "Get DirectMethodHandle.member known object %d\n", mnIndex);
              return mnIndex;
              }
           }
        case TR::java_lang_invoke_DirectMethodHandle_constructorMethod:
           {
           auto mhIndex = getObjectInfoOfNode(node->getFirstArgument());
           if (knot && isKnownObject(mhIndex) && !knot->isNull(mhIndex))
              {
              auto mnIndex = comp()->fej9()->getMemberNameFieldKnotIndexFromMethodHandleKnotIndex(comp(), mhIndex, "initMethod");
              if (trace())
                 traceMsg(comp(), "Get DirectMethodHandle.initMethod known object %d\n", mnIndex);
              return mnIndex;
              }
           }

         default:
            break;
        }
      }

   return TR::KnownObjectTable::UNKNOWN;
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
      if (trace())
         traceMsg(comp(), "rhs of store n%dn is obj%d\n", node->getGlobalIndex(), newObject);

      TR::KnownObjectTable::Index oldObject = (*_currentObjectInfo)[local->getLocalIndex()];
      if (newObject != oldObject && trace())
         {
         traceMsg(comp(), "Local #%2d obj%d -> obj%d at node n%dn\n", local->getLocalIndex(), oldObject, newObject,  node->getGlobalIndex());
         }

      (*_currentObjectInfo)[local->getLocalIndex()] = newObject;
      }
   }

// Visit indirect load, discover known object by folding the load if applicable
void TR_MethodHandleTransformer::visitIndirectLoad(TR::TreeTop* tt, TR::Node* node)
   {
   auto symRef = node->getSymbolReference();
   if (symRef->hasKnownObjectIndex())
      {
      if (trace())
         traceMsg(comp(), "Indirect load n%dn is obj%d\n", node->getGlobalIndex(), symRef->getKnownObjectIndex());
      return;
      }

   auto symbol = node->getSymbol();
   if (!symRef->isUnresolved() && symbol &&
       (symbol->isFinal() || symbol->isArrayShadowSymbol()))
      {
      auto baseNode = symbol->isArrayShadowSymbol() ? node->getFirstChild()->getFirstChild() : node->getFirstChild();
      auto baseSymRef = baseNode->getSymbolReference();
      TR::KnownObjectTable::Index baseObj = getObjectInfoOfNode(baseNode);
      if (trace())
         traceMsg(comp(), "base object for indirect load n%dn is obj%d\n", node->getGlobalIndex(), baseObj);

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
            if (trace())
               traceMsg(comp(), "Improve regular array-shadow to immutable-array-shadow for n%dn\n", node->getGlobalIndex());
            }

         TR::Node* removedNode = NULL;
         bool succeed = TR::TransformUtil::transformIndirectLoadChain(comp(), node, baseNode, baseObj, &removedNode);
         if (!succeed && trace())
            traceMsg(comp(), "Failed to fold indirect load n%dn from base object obj%d\n", node->getGlobalIndex(), baseObj);
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
      return;

   visitedNodes.add(node);

   if (trace() && node == tt->getNode())
      traceMsg(comp(), "Looking at treetop node n%dn\n", node->getGlobalIndex());

   for (int32_t i = 0; i < node->getNumChildren(); i++)
       visitNode(tt, node->getChild(i), visitedNodes);

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
      traceMsg(comp(), "Start processing block_%d\n", block->getNumber());
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
      traceMsg(comp(), "End processing block_%d\n", block->getNumber());
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
   if (trace())
      traceMsg(comp(), "MethodHandle is obj%d\n", objIndex);

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
   if (trace())
      traceMsg(comp(), "MemberName is obj%d\n", objIndex);

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
   auto knot = comp()->getKnownObjectTable();
   if (knot && isKnownObject(mhIndex) && isKnownObject(expectedTypeIndex))
      {
      TR::VMAccessCriticalSection vmAccess(fej9);
      uintptr_t mhObject = knot->getPointer(mhIndex);
      uintptr_t mtObject = fej9->getReferenceField(mhObject, "type", "Ljava/lang/invoke/MethodType;");
      uintptr_t etObject = knot->getPointer(expectedTypeIndex);

      if (etObject == mtObject && performTransformation(comp(), "%sChanging checkExactType call node n%dn to PassThrough\n", optDetailString(), node->getGlobalIndex()))
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
