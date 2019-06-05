/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

#ifdef J9ZTPF
#define __TPF_DO_NOT_MAP_ATOE_REMOVE
#endif

#include "optimizer/BoolArrayStoreTransformer.hpp"
#include "compiler/il/OMRTreeTop_inlines.hpp"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "infra/Cfg.hpp"
#include "infra/ILWalk.hpp"
#include <deque>
#include <stack>

/*
 * This transformer is used to make sure the behavior of bastore in JIT code
 * complies with JVM specs:
 *
 * If the arrayref refers to an array whose components are of type boolean,
 * then the int value is narrowed by taking the bitwise AND of value and 1;
 * the result is stored as the component of the array indexed by index
 *
 * bastore can be array store to either boolean or byte array. To
 * figure out which bastore is for boolean array, the following steps are taken:
 *
 * 1. Normal IL generation pass solves the cases where the array base node is parm
 *    ,field, or call. It also collects info about if there are stores of boolean
 *    or bytes array type to any autos. If only one type of array or multi dimension
 *    array ever appears in the IL trees, the bstorei nodes can only be this one type.
 * 2. If arrays of both types exist in the IL trees, the transformer do a traverse
 *    of the CFG and tries to propagate auto types as best effort. This
 *    approach might still leave some array's type unknown when there are stores of NULL
 *    to an auto, or there are loops, or the base array node is from a multinewarray.
 * 3. A runtime check is added for the array base node if the type is still unknown
 *    for any bstorei nodes after the above steps. The check will very likely be folded
 *    away by global value propagation and the overhead for important method body
 *    should be minimum.
 */

#define OPT_DETAILS "O^O BOOL ARRAY STORE TRANSFORMER: "

static char * getTypeName(TR_YesNoMaybe type, char * buffer)
   {
   switch (type)
      {
      case TR_yes:
         strcpy(buffer, "[Z");
         break;
      case TR_no:
         strcpy(buffer, "[B");
         break;
      case TR_maybe:
         strcpy(buffer, "unknown type");
         break;
      }
   return buffer;
   }

static void printTypeInfo(TR_BoolArrayStoreTransformer::TypeInfo *typeInfo, TR::Compilation *comp)
   {
   int localIndex = 0;
   for (auto it = typeInfo->begin(); it != typeInfo->end(); it++)
      {
      if (*it != TR_maybe)
         {
         char buffer[15];
         traceMsg(comp, "( local #%2d: %s )  ", localIndex, getTypeName(*it, buffer));
         }
      localIndex++;
      }
   }

TR_BoolArrayStoreTransformer::TR_BoolArrayStoreTransformer(NodeSet *bstoreiUnknownArrayTypeNodes, NodeSet *bstoreiBoolArrayTypeNodes)
   :
   _bstoreiUnknownArrayTypeNodes(bstoreiUnknownArrayTypeNodes),
   _bstoreiBoolArrayTypeNodes(bstoreiBoolArrayTypeNodes),
   _hasBoolArrayAutoOrCheckCast(false),
   _hasByteArrayAutoOrCheckCast(false),
   _hasVariantArgs(false),
   _numLocals(0)
   {
   _comp = TR::comp();
   }

void TR_BoolArrayStoreTransformer::perform()
   {
   if (comp()->getOption(TR_TraceILGen))
      traceMsg(comp(), "<BoolArrayStoreTransformer>\n");

   if (!_hasVariantArgs)
      {
      // There is no store to args and bstorei nodes with argument as array base have known type
      for (auto it = _bstoreiUnknownArrayTypeNodes->begin(); it != _bstoreiUnknownArrayTypeNodes->end();)
         {
         TR::Node *bstoreiNode = *it;
         it++;
         TR::Node *arrayBaseNode = bstoreiNode->getFirstChild()->getFirstChild();
         if (arrayBaseNode->getOpCode().hasSymbolReference() && arrayBaseNode->getSymbolReference()->getSymbol()->isParm())
            {
            if (isBoolArrayNode(arrayBaseNode, false /* parmAsAuto */))
               {
               if (comp()->getOption(TR_TraceILGen))
                  traceMsg(comp(), "bstorei node n%dn is [Z from parm type signature\n", bstoreiNode->getGlobalIndex());
               _bstoreiBoolArrayTypeNodes->insert(bstoreiNode);
               _bstoreiUnknownArrayTypeNodes->erase(bstoreiNode);
               }
            else if (isByteArrayNode(arrayBaseNode, false /* parmAsAuto */))
               {
               if (comp()->getOption(TR_TraceILGen))
                  traceMsg(comp(), "bstorei node n%dn is [B from parm type signature\n", bstoreiNode->getGlobalIndex());
               _bstoreiUnknownArrayTypeNodes->erase(bstoreiNode);
               }
            }
         }
      }
   else
      {
      // parm are treated as auto as well
      ListIterator<TR::ParameterSymbol> parms(&comp()->getMethodSymbol()->getParameterList());
      for (TR::ParameterSymbol * p = parms.getFirst(); p; p = parms.getNext())
         {
         if (isAnyDimensionBoolArrayParm(p))
            _hasBoolArrayAutoOrCheckCast = true;
         else if (isAnyDimensionByteArrayParm(p))
            _hasByteArrayAutoOrCheckCast = true;
         }
      }

   if (!_bstoreiUnknownArrayTypeNodes->empty())
      {
      if (_hasByteArrayAutoOrCheckCast && _hasBoolArrayAutoOrCheckCast) // only need to iterate CFG if both byte and boolean array exist
         findBoolArrayStoreNodes();
      else
         {
         if (_hasBoolArrayAutoOrCheckCast) // if only boolean array exist then all the bstorei nodes are operating on boolean array
            {
            if (comp()->getOption(TR_TraceILGen))
               traceMsg(comp(), "only boolean array exist as auto or checkcast type\n");
            _bstoreiBoolArrayTypeNodes->insert(_bstoreiUnknownArrayTypeNodes->begin(), _bstoreiUnknownArrayTypeNodes->end());
            }
         else
            {
            if (comp()->getOption(TR_TraceILGen))
               traceMsg(comp(), "only byte array exist as auto or checkcast type\n");
            }
         _bstoreiUnknownArrayTypeNodes->clear();
         }
      }

   if (!_bstoreiBoolArrayTypeNodes->empty())
      transformBoolArrayStoreNodes();

   if (!_bstoreiUnknownArrayTypeNodes->empty())
      transformUnknownTypeArrayStore();

   if (comp()->getOption(TR_TraceILGen))
      traceMsg(comp(), "</BoolArrayStoreTransformer>\n");
   }

void TR_BoolArrayStoreTransformer::collectLocals(TR_Array<List<TR::SymbolReference>> *autosListArray)
   {
   for (int i = 0; autosListArray && i < autosListArray->size(); i++)
      {
      List<TR::SymbolReference> autosList = (*autosListArray)[i];
      ListIterator<TR::SymbolReference> autosIt(&autosList);
      for (TR::SymbolReference* symRef = autosIt.getFirst(); symRef; symRef = autosIt.getNext())
         {
         TR::AutomaticSymbol *p = symRef->getSymbol()->getAutoSymbol();
         if (p && p->getDataType() == TR::Address)
            {
            if (comp()->getOption(TR_TraceILGen))
               traceMsg(comp(), "Local #%2d is symbol %p [#n%dn]\n", _numLocals, p, symRef->getReferenceNumber());
            p->setLocalIndex(_numLocals++);
            }
         }
      }
   }

/*
 * Merge second type info into first one if it has more concrete type for any auto
 */
void TR_BoolArrayStoreTransformer::mergeTypeInfo(TypeInfo *first, TypeInfo *second)
   {
   bool traceIt = comp()->getOption(TR_TraceILGen);
   if (traceIt)
      {
      traceMsg(comp(), "before merging: ");
      printTypeInfo(first, comp());
      traceMsg(comp(), "\n");
      }

   bool changed = false;
   for (int i = 0; i < _numLocals; i++)
      {
      TR_YesNoMaybe firstType = (*first)[i];
      TR_YesNoMaybe secondType = (*second)[i];
      if (secondType != TR_maybe)
         {
         if (firstType == TR_maybe)
            {
            (*first)[i] = secondType;
            changed = true;
            }
         else if (firstType != secondType )
            {
            // It doesn't matter which type wins because there must be a checkcast
            // or another kill with specific type later on every path reaching a bstorei
            if (traceIt)
               {
               char firstTypeBuffer[15];
               char secondTypeBuffer[15];
               traceMsg(comp(), "local #%2d has conflict types keep the first type for now: firstType %s, secondType %s\n", i, getTypeName(firstType, firstTypeBuffer), getTypeName(secondType, secondTypeBuffer));
               }
            }
         }
      }

   if (changed && traceIt)
      {
      traceMsg(comp(), "after merging: ");
      printTypeInfo(first, comp());
      traceMsg(comp(), "\n");
      }
   }

void TR_BoolArrayStoreTransformer::findBoolArrayStoreNodes()
   {
   TR::Region currentRegion(comp()->region());

   ListIterator<TR::ParameterSymbol> parms(&comp()->getMethodSymbol()->getParameterList());
   for (TR::ParameterSymbol * p = parms.getFirst(); p; p = parms.getNext())
      {
      if (p->getDataType() == TR::Address)
         {
         if (comp()->getOption(TR_TraceILGen))
            traceMsg(comp(), "Local #%2d is symbol %p <parm %d>\n", _numLocals, p, p->getSlot());
         p->setLocalIndex(_numLocals++);
         }
      }
   collectLocals(comp()->getMethodSymbol()->getAutoSymRefs());
   collectLocals(comp()->getMethodSymbol()->getPendingPushSymRefs());

   typedef TR::typed_allocator<std::pair<const int32_t, TypeInfo *>, TR::Region &> ResultAllocator;
   typedef std::map<int32_t, TypeInfo *, std::less<int32_t>, ResultAllocator> BlockResultMap;
   BlockResultMap blockStartTypeInfos(std::less<int32_t>(), comp()->trMemory()->currentStackRegion());

   /*
    * Do a reverse post-order traversal of the CFG as the best effort to figure out types in one traverse
    */
   TR::ReversePostorderSnapshotBlockIterator blockIt (comp()->getFlowGraph(), comp());
   //Initialize type info for parms for the entry block
   if (blockIt.currentBlock())
      {
      TR::Block *firstBlock = blockIt.currentBlock();
      ListIterator<TR::ParameterSymbol> parms(&comp()->getMethodSymbol()->getParameterList());
      TypeInfo * typeInfo = NULL;
      for (TR::ParameterSymbol * p = parms.getFirst(); p; p = parms.getNext())
         {
         if (p->getDataType() == TR::Address)
            {
            TR_YesNoMaybe type = TR_maybe;
            if (isBoolArrayParm(p))
               type = TR_yes;
            else if (isByteArrayParm(p))
               type = TR_no;

            if (type != TR_maybe)
               {
               if (!typeInfo)
                  typeInfo = new (comp()->trMemory()->currentStackRegion()) TypeInfo(_numLocals, TR_maybe, comp()->trMemory()->currentStackRegion());
               (*typeInfo)[p->getLocalIndex()] = type;
               }
            }
         }

      if (typeInfo)
         {
         blockStartTypeInfos[firstBlock->getNumber()] = typeInfo;
         if (comp()->getOption(TR_TraceILGen))
            {
            traceMsg(comp(), "Entry Block (block_%d) type Info: ", firstBlock->getNumber());
            printTypeInfo(typeInfo, comp());
            traceMsg(comp(), "\n");
            }
         }
      }

   TR::BlockChecklist visitedBlock(comp());
   _NumOfBstoreiNodesToVisit = _bstoreiUnknownArrayTypeNodes->size();
   while (blockIt.currentBlock() && _NumOfBstoreiNodesToVisit > 0)
      {
      TR::Block *block = blockIt.currentBlock();
      int32_t blockNum = block->getNumber();
      TypeInfo *blockStartTypeInfo = blockStartTypeInfos.find(blockNum) != blockStartTypeInfos.end()? blockStartTypeInfos[blockNum]: NULL;
      TypeInfo *blockEndTypeInfo = processBlock(block, blockStartTypeInfo);
      visitedBlock.add(block);
      TR_SuccessorIterator bi(block);
      for (TR::CFGEdge *edge = bi.getFirst(); edge != NULL; edge = bi.getNext())
         {
         TR::Block *nextBlock = toBlock(edge->getTo());
         int32_t nextBlockNum = nextBlock->getNumber();
         //propagate auto type info to successor
         //if the type info exist for the successor merge with the new one
         if (blockEndTypeInfo && !visitedBlock.contains(nextBlock))
            {
            if (blockStartTypeInfos.find(nextBlockNum) != blockStartTypeInfos.end())
               {
               if (comp()->getOption(TR_TraceILGen))
                  traceMsg(comp(), "merging into type info of successor block_%d\n", nextBlockNum);
               mergeTypeInfo(blockStartTypeInfos[nextBlockNum], blockEndTypeInfo);
               }
            else
               blockStartTypeInfos[nextBlockNum] = new (currentRegion) TypeInfo(*blockEndTypeInfo);
            }
         }
      ++blockIt;
      }
   }

/*
 * Answer includes multi dimension boolean array
 */
bool TR_BoolArrayStoreTransformer::isAnyDimensionBoolArrayNode(TR::Node *node)
   {
   return getArrayDimension(node, true /*boolType*/, false /* parmAsAuto*/) >= 1;
   }

/*
 * Answer includes multi dimension byte array
 */
bool TR_BoolArrayStoreTransformer::isAnyDimensionByteArrayNode(TR::Node *node)
   {
   return getArrayDimension(node, false /*boolType*/, false /* parmAsAuto */) >= 1;
   }


/*
 * Answer includes multi dimension boolean array
 */
bool TR_BoolArrayStoreTransformer::isAnyDimensionBoolArrayParm(TR::ParameterSymbol *symbol)
   {
   int length;
   const char *signature = symbol->getTypeSignature(length);
   return getArrayDimension(signature, length, true /*boolType*/) >= 1;
   }

/*
 * Answer includes multi dimension byte array
 */
bool TR_BoolArrayStoreTransformer::isAnyDimensionByteArrayParm(TR::ParameterSymbol *symbol)
   {
   int length;
   const char *signature = symbol->getTypeSignature(length);
   return getArrayDimension(signature, length, false /*boolType*/) >= 1;
   }

/*
 * \brief
 *    Answer includes one dimension boolean array only
 *
 * \parm node
 *
 * \parm parmAsAuto
 *    For a \parm node with parm symbol, the API is used in two different ways because the slot for dead
 *    parm symbol can be reused for variables of any type. When \parm parmAsAuto is true, parm is
 *    treated as other auto and the type signature is ignored. When \parm parmAsAuto is false, the
 *    type signature of the parm is used for telling the actual type.
 */
bool TR_BoolArrayStoreTransformer::isBoolArrayNode(TR::Node *node, bool parmAsAuto)
   {
   if (parmAsAuto && node->getOpCode().hasSymbolReference() && node->getSymbolReference()->getSymbol()->isParm())
      return false;
   return getArrayDimension(node, true /*boolType*/, parmAsAuto) == 1;
   }

/*
 * \brief
 *    Answer includes one dimension byte array only
 *
 * \parm node
 *
 * \parm parmAsAuto
 *    For a \parm node with parm symbol, the API is used in two different ways because the slot for dead
 *    parm symbol can be reused for variables of any type. When \parm parmAsAuto is true, parm is
 *    treated as other auto and the type signature is ignored. When \parm parmAsAuto is false, the
 *    type signature of the parm is used for telling the actual type.
 */
bool TR_BoolArrayStoreTransformer::isByteArrayNode(TR::Node *node, bool parmAsAuto)
   {
   if (parmAsAuto && node->getOpCode().hasSymbolReference() && node->getSymbolReference()->getSymbol()->isParm())
      return false;
   return getArrayDimension(node, false /*boolType*/, parmAsAuto) == 1;
   }

/*
 * Answer includes one dimension boolean array only
 */
bool TR_BoolArrayStoreTransformer::isBoolArrayParm(TR::ParameterSymbol *symbol)
   {
   int length;
   const char *signature = symbol->getTypeSignature(length);
   return getArrayDimension(signature, length, true /*boolType*/) == 1;
   }

/*
 * Answer includes one dimension byte array only
 */
bool TR_BoolArrayStoreTransformer::isByteArrayParm(TR::ParameterSymbol *symbol)
   {
   int length;
   const char *signature = symbol->getTypeSignature(length);
   return getArrayDimension(signature, length, false /*boolType*/) == 1;
   }

int TR_BoolArrayStoreTransformer::getArrayDimension(const char * signature, int length, bool boolType)
   {
   char expectedTypeChar = boolType? 'Z' : 'B';
   return (signature && length >= 2 && signature[length-1] == expectedTypeChar && signature[length-2] == '[') ? length-1: -1;
   }

/*
 * \brief
 *     Get dimension of the array of given type
 *
 * \parm node
 *    The node to look at
 *
 * \parm boolType
 *    True if asking for boolean array and false if asking for byte array
 *
 * \return
 *    The dimension of the array of given type. Return -1 if the node is not the
 *    given type.
 */
int TR_BoolArrayStoreTransformer::getArrayDimension(TR::Node *node, bool boolType, bool parmAsAuto)
   {
   int nodeArrayDimension = -1;
   if (node->getOpCodeValue() == TR::newarray)
      {
      int32_t expectedTypeValue = boolType ? 4: 8;
      TR::Node *arrayTypeNode = node->getSecondChild();
      TR_ASSERT(arrayTypeNode->getOpCode().isLoadConst(), "expect the second child of TR::newarray to be constant " POINTER_PRINTF_FORMAT, arrayTypeNode);
      if (arrayTypeNode->getInt() == expectedTypeValue)
         nodeArrayDimension = 1;
      }
   else
      {
      int length;
      const char * signature = node->getTypeSignature(length, stackAlloc, parmAsAuto);
      nodeArrayDimension = getArrayDimension(signature, length, boolType);
      }
	return nodeArrayDimension;
   }

/*
 * \brief
 *    Find all the loads of autos or parms in a subtree and figure out its type
 *    first time the load is referenced.
 *
 * \parm node
 *    The subtree to look at
 *
 * \parm typeInfo
 *    The type information of each auto at the current subtree
 *
 * \parm boolArrayNodes
 *    Load of autos or parms that are [Z
 *
 * \parm byteArrayNodes
 *    Load of autos or parms that are [B
 *
 * \parm visitedNodes
 *    All the nodes in the containing block. A node is added to this list the first time seen in the trees
 */
void TR_BoolArrayStoreTransformer::findLoadAddressAutoAndFigureOutType(TR::Node *node, TypeInfo * typeInfo, TR::NodeChecklist &boolArrayNodes, TR::NodeChecklist &byteArrayNodes, TR::NodeChecklist &visitedNodes)
   {
   if (visitedNodes.contains(node))
      return;

   for (int i = 0; i < node->getNumChildren(); i++)
      findLoadAddressAutoAndFigureOutType(node->getChild(i), typeInfo, boolArrayNodes, byteArrayNodes, visitedNodes);

   if (node->getType() == TR::Address && node->getOpCode().isLoadDirect() && node->getOpCode().hasSymbolReference() &&
         node->getSymbolReference()->getSymbol()->isAutoOrParm() && !visitedNodes.contains(node))
      {
      TR_YesNoMaybe type = (*typeInfo)[node->getSymbolReference()->getSymbol()->getLocalIndex()];
      if (type == TR_yes)
         boolArrayNodes.add(node);
      else if (type == TR_no)
         byteArrayNodes.add(node);
      }
   visitedNodes.add(node);
   }

/*
 * \brief
 *   This function calculcates the type info of each auto and figure out whether a
 *   bstorei node is for boolean array
 *
 * \parm block
 *    The block to process
 *
 * \parm blockStartTypeInfo
 *    The auto type info at block start
 *
 * \return currentTypeInfo
 *    The working auto type info
 *
 * \note
 *    For each load of auto, record whether the type is boolean array (TR_yes), byte array type
 *    (TR_no), other or unknown type (TR_maybe). For each store of auto, propagate the type from
 *    right hand side to the left hand side auto.
 */
TR_BoolArrayStoreTransformer::TypeInfo * TR_BoolArrayStoreTransformer::processBlock(TR::Block *block, TR_BoolArrayStoreTransformer::TypeInfo *blockStartTypeInfo)
   {
   TR_BoolArrayStoreTransformer::TypeInfo *currentTypeInfo = blockStartTypeInfo;
   TR::NodeChecklist boolArrayNodes(comp());
   TR::NodeChecklist byteArrayNodes(comp());
   TR::NodeChecklist visitedNodes(comp());

   if (comp()->getOption(TR_TraceILGen))
      {
      traceMsg(comp(), "start processing block_%d: ", block->getNumber());
      if (currentTypeInfo)
         printTypeInfo(currentTypeInfo, comp());
      traceMsg(comp(), "\n");
      }

   for (TR::TreeTop *tt = block->getEntry(); tt != block->getExit(); tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      // find all aload auto from the current tree top first
      if (currentTypeInfo)
         findLoadAddressAutoAndFigureOutType(node, currentTypeInfo, boolArrayNodes, byteArrayNodes, visitedNodes);

      if (node->getOpCode().isStoreDirect() && node->getSymbolReference()->getSymbol()->isAutoOrParm())
         {
         TR::Node *rhs = node->getFirstChild();
         TR::Symbol *local = node->getSymbolReference()->getSymbol();
         if (rhs->getDataType().isAddress())
            {
            TR_YesNoMaybe newType = TR_maybe;
            if (isBoolArrayNode(rhs, _hasVariantArgs) || boolArrayNodes.contains(rhs))
               newType = TR_yes;
            else if (isByteArrayNode(rhs, _hasVariantArgs) || byteArrayNodes.contains(rhs))
               newType = TR_no;

            if (newType != TR_maybe || currentTypeInfo)
               {
               if (!currentTypeInfo)
                  currentTypeInfo = new (comp()->trMemory()->currentStackRegion()) TypeInfo(_numLocals, TR_maybe, comp()->trMemory()->currentStackRegion());
               if (comp()->getOption(TR_TraceILGen))
                  {
                  char newTypeBuffer[15];
                  char oldTypeBuffer[15];
                  TR_YesNoMaybe oldType = (*currentTypeInfo)[local->getLocalIndex()];
                  traceMsg(comp(), "Local #%2d %s -> %s at node n%dn\n", local->getLocalIndex(), getTypeName(oldType, oldTypeBuffer), getTypeName(newType, newTypeBuffer),  node->getGlobalIndex());
                  }
               (*currentTypeInfo)[local->getLocalIndex()] = newType;
               }
            }
         }
      else if (node->getOpCodeValue() == TR::bstorei && (_bstoreiUnknownArrayTypeNodes->find(node) != _bstoreiUnknownArrayTypeNodes->end()))
         {
         _NumOfBstoreiNodesToVisit--;
         TR_ASSERT(node->getFirstChild()->isInternalPointer(), "node in _bstoreiUnknownArrayTypeNodes can only be array store");
         TR::Node *arrayBaseNode = node->getFirstChild()->getFirstChild();
         if (boolArrayNodes.contains(arrayBaseNode))
            {
            if (comp()->getOption(TR_TraceILGen))
               {
               char buffer[15];
               traceMsg(comp(), "bstorei node n%dn is %s\n", node->getGlobalIndex(), getTypeName(TR_yes, buffer));
               }
            _bstoreiUnknownArrayTypeNodes->erase(node);
            _bstoreiBoolArrayTypeNodes->insert(node);
            }
         else if (byteArrayNodes.contains(arrayBaseNode))
            {
            if (comp()->getOption(TR_TraceILGen))
               {
               char buffer[15];
               traceMsg(comp(), "bstorei node n%dn is %s\n", node->getGlobalIndex(), getTypeName(TR_no, buffer));
               }
            _bstoreiUnknownArrayTypeNodes->erase(node);
            }
         }
      else if (node->getOpCode().isCheckCast())
         {
         TR::Node *typeNode = node->getSecondChild();
         TR::Node *checkcastedNode = node->getFirstChild();
         if (isBoolArrayNode(typeNode))
            {
            if (byteArrayNodes.contains(checkcastedNode)) // this can happen when [Z and [B are merged at one point
               byteArrayNodes.remove(checkcastedNode);
            if (comp()->getOption(TR_TraceILGen))
               traceMsg(comp(), "checkcast node n%dn force node n%dn to be [Z\n", node->getGlobalIndex(), checkcastedNode->getGlobalIndex());
            boolArrayNodes.add(checkcastedNode);
            }
         else if (isByteArrayNode(typeNode))
            {
            if (boolArrayNodes.contains(checkcastedNode)) // this can happen when [Z and [B are merged at one point
               boolArrayNodes.remove(checkcastedNode);
            if (comp()->getOption(TR_TraceILGen))
               traceMsg(comp(), "checkcast node n%dn force node n%dn to be [B\n", node->getGlobalIndex(), checkcastedNode->getGlobalIndex());
            byteArrayNodes.add(checkcastedNode);
            }
         }
      }

   if (comp()->getOption(TR_TraceILGen))
      {
      traceMsg(comp(), "end processing block_%d: ", block->getNumber());
      if (currentTypeInfo)
         printTypeInfo(currentTypeInfo, comp());
      traceMsg(comp(), "\n");
      }

   return currentTypeInfo;
   }

/* \brief
 *    Generating nodes to and the value to be stored to the array with the provided mask
 *
 * \parm bstoreiNode
 *
 * \parm int mask
 *
 * \note
 *    Transform bstorei node from:
 *    bstorei
 *       aladd (internal pointer)
 *          ...
 *       value node
 *
 *    to:
 *    bstorei
 *       aladd (internal pointer)
 *          ...
 *       i2b
 *          iand
 *             b2i
 *                value node
 *             mask node
 */
static void generateiAndNode(TR::Node *bstoreiNode, TR::Node *mask, TR::Compilation *comp)
   {
   if (comp->getOption(TR_TraceILGen))
      traceMsg(comp, "truncating mask node n%dn\n", mask->getGlobalIndex());
   TR::Node *bValueChild = bstoreiNode->getSecondChild();
   TR::Node *iValueChild = TR::Node::create(bstoreiNode, TR::b2i, 1, bValueChild);
   TR::Node *iandNode = TR::Node::create(bstoreiNode, TR::iand, 2, iValueChild, mask);
   TR::Node *i2bNode = TR::Node::create(bstoreiNode, TR::i2b, 1, iandNode);
   bstoreiNode->setAndIncChild(1, i2bNode);
   bValueChild->decReferenceCount();
   }

/*
 * \brief
 *    For bstorei with unknown type info, and the value to be stored with a mask.
 *
 * \note
 *    The mask value is calculated based on runtime array type info:
 *    mask = (class of array base node == J9class of [Z ? 1: 0)*2 - 1. It's 1 for
 *    boolean array and -1 (0xFFFFFFFF) for byte array.

 *    Mask calculation in tree:
 *     iadd
 *        ishl
 *           acmpeq
 *              aloadi <vft-symbol>
 *                 => array base node
 *              aconst [Z J9Class
 *           2
 *        iconst -1 (0xffffffff)
 */
void TR_BoolArrayStoreTransformer::transformUnknownTypeArrayStore()
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp()->fe());
   // this check should be deleted after the new AOT work is delivered
   if (fej9->isAOT_DEPRECATED_DO_NOT_USE())
      return;
   //get j9class of [Z
   uintptrj_t j9class =  (uintptrj_t) fej9->getClassFromNewArrayType(4);
   for (auto it = _bstoreiUnknownArrayTypeNodes->begin(); it != _bstoreiUnknownArrayTypeNodes->end(); it++)
      {
      TR::Node *bstoreiNode = *it;
      dumpOptDetails(comp(), "%s transform value child of bstorei node of unknown type n%dn\n", OPT_DETAILS, bstoreiNode->getGlobalIndex());
      TR::Node *arrayBaseNode = bstoreiNode->getFirstChild()->getFirstChild();
      TR::Node *vft = TR::Node::createWithSymRef(TR::aloadi, 1, 1, arrayBaseNode, comp()->getSymRefTab()->findOrCreateVftSymbolRef());
      TR::Node *aconstNode = TR::Node::aconst(bstoreiNode, j9class);
      aconstNode->setIsClassPointerConstant(true);
      TR::Node *compareNode = TR::Node::create(arrayBaseNode, TR::acmpeq, 2, vft, aconstNode);
      TR::Node *shift1Node = TR::Node::create(TR::ishl, 2 , compareNode, TR::Node::iconst(bstoreiNode, 1));
      TR::Node *iandMaskNode = TR::Node::create(TR::iadd, 2 , shift1Node, TR::Node::iconst(bstoreiNode, -1));
      generateiAndNode(bstoreiNode, iandMaskNode, comp());
      }
   }

void TR_BoolArrayStoreTransformer::transformBoolArrayStoreNodes()
   {
   for (auto it = _bstoreiBoolArrayTypeNodes->begin(); it != _bstoreiBoolArrayTypeNodes->end(); it++)
      {
      TR::Node *node = *it;
      dumpOptDetails(comp(), "%s truncate value child of bstorei node n%dn to 1 bit\n", OPT_DETAILS, node->getGlobalIndex());
      generateiAndNode(node, TR::Node::iconst(node, 1), comp());
      }
   }
