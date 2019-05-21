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

#include "optimizer/SignExtendLoads.hpp"

#include <stdint.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "ras/Debug.hpp"

#define OPT_DETAILS "O^O SIGN EXTENDING LOADS TRANSFORMATION: "

// -------------------------------------------------------------------------------------------
// later this may become  more complex
// -------------------------------------------------------------------------------------------
bool shouldEnableSEL(TR::Compilation *comp)
   {
   static char * enableSEL = feGetEnv("TR_SIGNEXTENDLOADS");
   if (TR::Compiler->target.cpu.isZ())
      {
      // enable only for 390
      static char * nenableSEL = feGetEnv("TR_NSIGNEXTENDLOADS");
      if(nenableSEL ==NULL) enableSEL = "enable";
      }
   return ((enableSEL != NULL) &&
           TR::Compiler->target.is64Bit());
   }


// -------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------
static bool isNullCheck(TR::Node * node)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();
   if(opCode == TR::NULLCHK || opCode == TR::ResolveAndNULLCHK) return true;
   return false;
   }
// -------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------
void TR_SignExtendLoads::addNodeToHash(TR::Node *node,TR::Node *parent)
   {
   TR_ScratchList<TR::Node> *nodeList;
   nodeList = getListFromHash(node);
   if (NULL != nodeList)
      {
      nodeList->add(parent);
      }
   else
      {
      nodeList = new (trStackMemory()) TR_ScratchList<TR::Node>(parent, trMemory());
      addListToHash(node,nodeList);
      }
   }
// -------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------
void TR_SignExtendLoads::addListToHash(TR::Node *node, TR_ScratchList<TR::Node> *nodeList)
   {

   int32_t hashBucket = nodeHashBucket(node);

   HashTableEntry *entry = (HashTableEntry *)trMemory()->allocateStackMemory(sizeof(HashTableEntry));
   entry->_node = node;
   entry->_referringList = nodeList;

   HashTableEntry *prevEntry = _sharedNodesHash._buckets[hashBucket];
   if (prevEntry)
      {
      entry->_next = prevEntry->_next;
      prevEntry->_next = entry;
      }
   else
      entry->_next = entry;

   _sharedNodesHash._buckets[hashBucket] = entry;
   }

// -------------------------------------------------------------------------------------------
// empty the buckets--to prevent excess collisions for large basic blocks
// -------------------------------------------------------------------------------------------
void TR_SignExtendLoads::emptyHashTable()
   {
     if(false)traceMsg(comp(), "emptying the hash table\n");
   for(int i=0; i < _sharedNodesHash._numBuckets;++i)
    _sharedNodesHash._buckets[i] = NULL;;
   }

// -------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------
TR_ScratchList<TR::Node> * TR_SignExtendLoads::getListFromHash(TR::Node *node)
   {
   int32_t hashBucket = nodeHashBucket(node);

   HashTableEntry *firstEntry = _sharedNodesHash._buckets[hashBucket];
   if (firstEntry)
      {
      HashTableEntry *entry = firstEntry;

      do
         {
         if (entry->_node == node)
            {
            return entry->_referringList;
            }
         entry = entry->_next;
         }
      while (entry != firstEntry);
      }

   return NULL;
   }

// -------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------
void TR_SignExtendLoads::InitializeHashTable()
  {
   _sharedNodesHash._numBuckets = 127;
   _sharedNodesHash._buckets = (HashTableEntry **)trMemory()->allocateStackMemory(_sharedNodesHash._numBuckets*sizeof(HashTableEntry *));
   memset(_sharedNodesHash._buckets, 0, _sharedNodesHash._numBuckets*sizeof(HashTableEntry *));
  }

// -------------------------------------------------------------------------------------------
// Run through the basic block and gather a list of i2lNodes, and a hashtable of all iload & i2l nodes
// that are shared.  Return true if any nodes gathered
// -------------------------------------------------------------------------------------------
bool TR_SignExtendLoads::gatheri2lNodes(TR::Node* parent,TR::Node *node, TR_ScratchList<TR::Node> & i2lList,
                                        TR_ScratchList<TR::Node> & useri2lList,
                                        bool isIndexSubtree)
   {
   vcount_t curVisit = comp()->getVisitCount();

   bool nodesGathered=false;
   bool isi2lNode=false;
   TR::ILOpCodes opCode = node->getOpCodeValue();
   // If this is the first time through this node, examine the children
   //
   if (node->getVisitCount() != curVisit)
      {
      node->setVisitCount(curVisit);

      isi2lNode = opCode == TR::i2l;

      if (isi2lNode)
         {
         // only need to enter once

         if (trace()) traceMsg(comp(), "Found i2l %p, parent %p, is%s an aladd child\n",node,parent,isIndexSubtree?"":" not");
         if (isIndexSubtree)
            i2lList.add(parent); // basically any i2l found under an aladd
         else
            useri2lList.add(parent); // probably an i2l introduced by user
         nodesGathered = true;
         }

      bool isAlAdd = opCode == TR::aladd;
      // Will be moving around multiply referenced i2l nodes, or int children of
      // i2l and add or sub
      for (int i = 0; i < node->getNumChildren(); i++)
         {
         TR::Node *child = node->getChild(i);

         TR::ILOpCodes opCode = child->getOpCodeValue();
         switch(opCode)
            {
            case TR::iload: // only need to visit these if shared
            case TR::iconst:
            case TR::iloadi:
              if(child->getReferenceCount() < 2) break;
            case TR::i2l:
            case TR::iadd:
            case TR::isub:
              addNodeToHash(child,node);
              if(trace()) traceMsg(comp(), "node %p has %d references\n",child,child->getReferenceCount());
            default:
            	break;
            }
          nodesGathered = gatheri2lNodes(node, child,i2lList,useri2lList,isIndexSubtree || (i==1 && isAlAdd))
            || nodesGathered;
         }
      }

   return nodesGathered;
   }

// -------------------------------------------------------------------------------------------
// insert an i2l node newI2LNode above (parent of) targetNode, taking care of when targetNode is shared.
// -------------------------------------------------------------------------------------------
void TR_SignExtendLoads::Insertl2iNode(TR::Node *targetNode)
   {
   rcount_t refCount = targetNode->getReferenceCount();
   TR_ScratchList<TR::Node> * nodeList = getListFromHash(targetNode);
   TR::Node                 * parentNode;

   TR_ASSERT(nodeList,"No node list for child %p\n",targetNode);

   // now update references to targetNode

   ListIterator<TR::Node> listElement(nodeList);
   for (parentNode = listElement.getFirst(); parentNode!=NULL; parentNode = listElement.getNext())
      {
      int32_t numKids = parentNode->getNumChildren();
      --refCount;
      for (int32_t j=0; j < numKids;++j)
         {
         if (parentNode->getChild(j) == targetNode)
            {
            // if parent is not of type long, then create and insert the node
            //Fortunately, this is a free operation
            if (!parentNode->getOpCode().isLong() || parentNode->getOpCode().isCall())
               {
               TR::Node *newL2INode = TR::Node::create(targetNode,TR::l2i,1);

               if (!performTransformation(comp(), "%sInserting l2i node %p for %p\n", OPT_DETAILS,
                                           newL2INode,targetNode))
                  return;

               parentNode->setChild(j,newL2INode);
               newL2INode->setReferenceCount(1);
               newL2INode->setChild(0,targetNode);
               }
            else if (TR::i2l == parentNode->getOpCodeValue()) // must remove these
               {
               TR_ScratchList<TR::Node> * i2lParentList = getListFromHash(parentNode);
               TR_ASSERT(nodeList,"No node list for i2l node %p\n",parentNode);

               ListIterator<TR::Node> listElement(i2lParentList);
               TR::Node * i2lParent;
               uint32_t refCnt=0;
               for (i2lParent = listElement.getFirst(); i2lParent!=NULL; i2lParent = listElement.getNext())
                  {
                  int32_t numKids = i2lParent->getNumChildren();
                  for (int32_t j=0; j < numKids;++j)
                     {
                     if (i2lParent->getChild(j) == parentNode)
                        {
                        if (trace()) traceMsg(comp(), "Remove i2l node %p from %p, ->%p\n",parentNode,i2lParent,targetNode);
                        if (++refCnt > 1)
                           targetNode->incReferenceCount();
                        parentNode->decReferenceCount();
                        i2lParent->setChild(j,targetNode);
                        }
                     }
                  }
               }
            break;// if this parent has multiple references to this node, it will be seen again
            }
         }
      }
   TR_ASSERT(0 == refCount,"Refcount of %p=%d, not 0\n",targetNode,refCount);
   }
// -------------------------------------------------------------------------------------------
// insert an i2l node newI2LNode above targetNode, taking care of when targetNode is shared.
// -------------------------------------------------------------------------------------------
void TR_SignExtendLoads::Inserti2lNode(TR::Node *targetNode,TR::Node* newI2LNode)
   {
   rcount_t refCount = targetNode->getReferenceCount();
   newI2LNode->setChild(0,targetNode);
   newI2LNode->setReferenceCount(0);

   uint32_t extraRefs = refCount;

   if (refCount < 2) return;
   // now update references to targetNode

   TR::Node                 * parentNode;
   TR_ScratchList<TR::Node> * nodeList = getListFromHash(targetNode);
   TR_ASSERT(nodeList,"No node list for child %p\n",targetNode);

   if (!performTransformation(comp(), "%sInserting i2l node %p for %p\n", OPT_DETAILS, newI2LNode, targetNode))
      return;


   // now go through and update references accurately.  Add one for reference we added above,
   // and will subtract for each one we remove in the loop below
   targetNode->incReferenceCount();
   ListIterator<TR::Node> listElement(nodeList);
   bool skippedAll=true;
   for (parentNode = listElement.getFirst(); parentNode!=NULL; parentNode = listElement.getNext())
      {
      int32_t numKids = parentNode->getNumChildren();
      --refCount;
      bool skipChild = isNullCheck(parentNode);
      for (int32_t j=0; j < numKids;++j)
         {
         // Insert an l2i node and have it point to a shared i2l node
         if (parentNode->getChild(j) == targetNode && !skipChild)
            {
            skippedAll=false;
            targetNode->decReferenceCount();
            // if parent is not of type long, then we need to round down.
            //Fortunately, this is a free operation
            // Call nodes are special--type unrelated to type of children
            if (!parentNode->getOpCode().isLong() || parentNode->getOpCode().isCall())
               {
               TR::Node *newL2INode = TR::Node::create(newI2LNode,TR::l2i,1);
               parentNode->setChild(j,newL2INode);
               newL2INode->setReferenceCount(1);
               newL2INode->setChild(0,newI2LNode);
               }
            else // note: if parent is an i2l, it will be removed later
               {
               parentNode->setChild(j,newI2LNode);
               }
              newI2LNode->incReferenceCount();
              if(trace()) traceMsg(comp(), "Updated %p to point to %p\n",parentNode,newI2LNode);
              break;// if this parent has multiple references to this node, it will be seen again
            }
         }
     }
   // in unusual cases, we don't update any nodes, so back off the extra add we made
   if (skippedAll)
      {
      performTransformation(comp(), "%s* * BNDCHK case: i2l node %p not inserted\n", OPT_DETAILS, newI2LNode);
      targetNode->decReferenceCount();
      }
   TR_ASSERT(0 == refCount,"Refcount of %p=%d, not 0\n",targetNode,refCount);
   }


// -------------------------------------------------------------------------------------------
// Modify all integer operator nodes to long, and insert i2l nodes in leaves.  Return false
// if we cannot change all of the leaves.  To be used in two passes: once with no changes,
// and then a second time to actually modify the nodes
// It is assumed this is part of an array reference address, so the overflow assumptions
// can be less pessimistic
//
// -------------------------------------------------------------------------------------------
bool TR_SignExtendLoads::ConvertSubTreeToLong(TR::Node *parent, TR::Node *node, bool changeNode)
   {

   TR::Node *secondChild = NULL;
   TR::ILOpCodes opCode;
   TR::ILOpCodes secondOpCode;
   bool returnValue;
   switch(node->getOpCodeValue())
      {
      case TR::lconst:  return true;
      // can get inserted by iconst case below, then later a parent iadd/isub can get converted
      // to ladd/lsub, and we can toss the l2i
      case TR::l2i:
         if (changeNode && parent->getOpCode().isLong() /* && node->getReferenceCount()== 1 */)
            {
            int32_t numKids = parent->getNumChildren();
            for (int32_t j=0; j < numKids;++j)
               {
               if (parent->getChild(j) == node)
                  {
                  parent->setAndIncChild(j,node->getChild(0));
                  addNodeToHash(node->getChild(0), parent);
                  node->recursivelyDecReferenceCount();
                  if(trace()) traceMsg(comp(), "Get rid of l2i %p of %p\n",node,parent);
                  break;
                  }
               }
            }
         return true;

        /* In general it's not free to an an l2i to an iconst, unless the constant is small enough
         * which we check in below case
         */
      case TR::iconst:
        {
        int32_t constVal = node->getInt();
         /* Motivating case is:
          * i2l              lsub
          *   isub     =>      i2l
          *     iload            iload
          *     iconst         lconst
          *
          * This is done such that memory reference can fold it into base/index/disp form.
          * Need to investigate if removal of disp range check causes issues.
          * Even if lconst did not lie in allowed disp range, it may not cause harm to keep it around.
          */
        if (changeNode)
           {
           TR::Node *newNode;
           if (node->getReferenceCount() < 2)
              {
              // lets reuse the node
              newNode = node;
              TR::Node::recreate(node, TR::lconst);
              }
           else
              {
              newNode = TR::Node::create(node, TR::lconst);
              }
           if (!performTransformation(comp(), "%sReplace %p iconst->%p lconst(%d)\n", OPT_DETAILS,
                                      node,newNode,constVal))
              return false;

           node->decReferenceCount();
           newNode->setReferenceCount(1);
           int32_t numKids = parent->getNumChildren();
           for (int32_t j=0; j < numKids;++j)
              {
              if (parent->getChild(j) == node)
                 {
                 parent->setChild(j,newNode);
                 break;
                 }
              }
           newNode->setLongInt((int64_t)constVal);

           if (TR::i2l == parent->getOpCodeValue()) //unlikely to happen
              ReplaceI2LNode(parent,newNode);
           }
        return true;
       }

      case TR::iloadi: if(isNullCheck(parent)) return false;
      case TR::iload:

         if (trace()) traceMsg(comp(), "inspecting load/i2l etc %p\n",node);
         if (changeNode)
            {
            TR::Node *newI2LNode = TR::Node::create(node, TR::i2l, 1);
            if (node->getReferenceCount() < 2) // these nodes not hashed for speed--must handle manually here
               {
               int32_t numKids = parent->getNumChildren();
               for (int32_t j=0; j < numKids;++j)
                   {
                   if (parent->getChild(j) == node)
                      {
                      parent->setChild(j, newI2LNode);
                      newI2LNode->setChild(0, node);
                      newI2LNode->incReferenceCount();
                      break;
                      }
                   }
               }
            else
               Inserti2lNode(node, newI2LNode);
            }
         if (trace()) traceMsg(comp(), "...ok iload etc\n");
         return true;

      case TR::isub:
         if (trace())traceMsg(comp(), "inspecting isub %p\n",node);
         if (!node->cannotOverflow()) return false;

         // need to change nodes top-down to avoid sticking in
         // superflous l2i nodes
         opCode = node->getOpCodeValue();
         if (changeNode)
            {
            if(trace()) traceMsg(comp(), "Converting isub %p\n",node);
            if (!performTransformation(comp(), "%sConvert %p isub->lsub\n", OPT_DETAILS, node))
               return false;
            TR::Node::recreate(node, TR::lsub);
            }

         secondChild = node->getChild(1);
       secondOpCode = secondChild->getOpCodeValue();

         returnValue = true;

         if (!ConvertSubTreeToLong(node, node->getChild(0), false))
            returnValue = false;

         if (returnValue)
            {
            if (!ConvertSubTreeToLong(node, node->getChild(1), false))
               returnValue = false;
            }

         if (returnValue && changeNode)
            {
            ConvertSubTreeToLong(node, node->getChild(0), changeNode);
            if ((node->getChild(1) == secondChild) &&
                (node->getChild(1)->getOpCodeValue() == secondOpCode))
               ConvertSubTreeToLong(node, node->getChild(1), changeNode);
            }

         if (changeNode)
            {
            if (returnValue)
               Insertl2iNode(node);
            else
               TR::Node::recreate(node, opCode);
            }


         if (trace())traceMsg(comp(), "...ok isub->lsub %p\n",node);
         return returnValue;
         break;

      // punt if wraparound from +ve to -ve, or vice versa
      case TR::iadd:
         if (trace())traceMsg(comp(), "inspecting iadd %p\n",node);
         if (!node->cannotOverflow()) return false;

         opCode = node->getOpCodeValue();
         if (changeNode)
            {
            if (!performTransformation(comp(), "%sConvert %p iadd->ladd\n", OPT_DETAILS, node))
               return false;
            TR::Node::recreate(node, TR::ladd);
            if (trace()) traceMsg(comp(), "Converting isub %p\n",node);
            }

         secondChild = node->getChild(1);
       secondOpCode = secondChild->getOpCodeValue();

         returnValue = true;

         if (!ConvertSubTreeToLong(node,node->getChild(0),false))
            returnValue = false;

         if (returnValue)
            {
            if (!ConvertSubTreeToLong(node,node->getChild(1),false))
               returnValue = false;
            }

         if (returnValue && changeNode)
            {
            ConvertSubTreeToLong(node,node->getChild(0),changeNode);
            if ((node->getChild(1) == secondChild) &&
                (node->getChild(1)->getOpCodeValue() == secondOpCode))
               ConvertSubTreeToLong(node,node->getChild(1),changeNode);
            }

         if (changeNode)
            {
            if (returnValue)
               Insertl2iNode(node);
            else
               TR::Node::recreate(node, opCode);
            }

         if (trace())traceMsg(comp(), "...ok add->ladd\n");
         return returnValue;
         break;
      default: return false;
      }
   return false;
   }


// -------------------------------------------------------------------------------------------
// The child of the i2lNode, an iload, is shared.  Need to
// create an i2l node and redirect all parents of the iload to this i2lnode.  Then,
// use it to replace 'i2lNode'.  When doing this replacement, if i2lNode is itself shared,
// need to update all of it's parents.
// isIndexSubtree indicates the i2l is part of addressing calculation for dereferencing an
// array, and so we can make some assumptions.
// -------------------------------------------------------------------------------------------
void TR_SignExtendLoads::Propagatei2lNode(TR::Node *i2lNode,TR::Node *parent,int32_t childIndex,
                                          bool isIndexSubtree)
   {
  /*
   TR_ASSERT(i2lNode->getOpCodeValue() == TR::i2l,"Unexpected opcode %d\n",i2lNode->getOpCodeValue());
   TR_ASSERT(parent->getChild(childIndex) == i2lNode,"childIndex %d is wrong %p != %p\n",childIndex,
             parent->getChild(childIndex),i2lNode);

   TR::Node *i2lChild = i2lNode->getChild(0);
   TR::Node *newI2LNode = TR::Node::create(i2lNode, TR::i2l, 1, i2lNode->getSymbolReference());


   rcount_t refCount = i2lNode->getReferenceCount(); // get count before Inserti2lNode

   Inserti2lNode(i2lChild,newI2LNode);

   // Now, get rid of original i2l node as it's been propagated into it's children
   rcount_t numi2lRefs = i2lNode->getReferenceCount();
   if (numi2lRefs <2) // should be the common case
      {
      parent->setChild(childIndex,newI2LNode);
      if (0 == newI2LNode->getReferenceCount()) newI2LNode->incReferenceCount();
      }
   else
      {
      TR_ScratchList<TR::Node> * nodeList = getListFromHash(i2lNode);
      TR_ASSERT(nodeList,"No node list for child %p\n",i2lNode);
      ListIterator<TR::Node> listElement(nodeList);
      int32_t newRefs=0;
      for (TR::Node *parentNode = listElement.getFirst(); parentNode!=NULL; parentNode = listElement.getNext())
         {
         if(trace()) traceMsg(comp(), "Inspecting parent %p\n",parentNode);
         int32_t numKids = parentNode->getNumChildren();
         for (int32_t j=0; j < numKids;++j)
            {
            if (parentNode->getChild(j) == i2lNode)
               {
               parentNode->setChild(j,newI2LNode);
               newI2LNode->incReferenceCount();
               if(trace()) traceMsg(comp(), "updated i2l parent %p point to %p\n",parentNode,newI2LNode);
               ++newRefs;
               }
            }
         }
      // If there are N references to this i2l node, then we need to
      // bump the new inserted i2l by N-1 references.  The above loop
      // adds N references, below is the -1
      newI2LNode->decReferenceCount();
      TR_ASSERT(newRefs == numi2lRefs,"Missed a ref %d != %d for %p\n",newRefs,numi2lRefs,newI2LNode);
      }
   */
   }

// -------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------
void TR_SignExtendLoads::ReplaceI2LNode(TR::Node *i2lNode,TR::Node *newNode)
   {

   rcount_t numi2lRefs = i2lNode->getReferenceCount();
   TR_ScratchList<TR::Node> * nodeList = getListFromHash(i2lNode);
   TR_ASSERT(nodeList,"No node list for i2l %p\n",i2lNode);
   ListIterator<TR::Node> listElement(nodeList);
   int32_t newRefs=0;
   for (TR::Node *parentNode = listElement.getFirst(); parentNode!=NULL; parentNode = listElement.getNext())
      {
      int32_t numKids = parentNode->getNumChildren();
      if(trace())traceMsg(comp(), "looking at parent %p of %p\n",parentNode,i2lNode);
      for (int32_t j=0; j < numKids;++j)
         {
         if (parentNode->getChild(j) == i2lNode)
            {
            parentNode->setChild(j,newNode);
            if(trace()) traceMsg(comp(), "updated i2l parent %p point to %p\n",parentNode,newNode);
            ++newRefs;
            if (newRefs > 1)
               newNode->incReferenceCount();

            if (!performTransformation(comp(), "%sUpdating reference to node %p with %p\n", OPT_DETAILS,i2lNode ,newNode))
               return;

           }
        }
     }
   TR_ASSERT(newRefs == numi2lRefs,"Missed a ref %d != %d for %p\n",newRefs,numi2lRefs,i2lNode);
   }
// -------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------
void TR_SignExtendLoads::ProcessNodeList(TR_ScratchList<TR::Node> &list,bool isAddressCalc)
   {
   ListIterator<TR::Node> listElement(&list);
   TR::Node * parentNode;

   for (parentNode = listElement.getFirst(); parentNode!=NULL; parentNode = listElement.getNext())
      {
      for (int i = 0; i < parentNode->getNumChildren(); i++)
         {
         TR::Node *child = parentNode->getChild(i);
         if (child->getOpCodeValue() == TR::i2l)
            {
            if (child->getReferenceCount() > 1 && (NULL == getListFromHash(child)))
               {
               if(trace()) traceMsg(comp(), "Already processed parent %p--skipping %p\n",parentNode,child);
               continue;
               }
            if(trace()) traceMsg(comp(), "Processing i2l node %p (parent:%p)\n",child,parentNode);
            TR::Node *i2lchild = child->getChild(0);
            switch(i2lchild->getOpCodeValue())
               {

               case TR::iloadi:
               case TR::iload:
               if (i2lchild->getReferenceCount() < 2)
                  {
                  if(trace()) traceMsg(comp(), "i2l(%p):iload not shared--skip\n",child);
                  continue; // not shared--do nothing
                  }
               if (!performTransformation(comp(), "%si2l inserted for %p\n", OPT_DETAILS, child))
                  break;

               Propagatei2lNode(child,parentNode,i,isAddressCalc);
               break;

               // if we're part of an address calculation for an array,
               // then we cannot wrap
               case TR::isub:
               case TR::iadd:
                  if (!isAddressCalc) continue;
                  if (trace())traceMsg(comp(), "child of %p is add/sub\n",i2lchild);
                  if (ConvertSubTreeToLong(child, i2lchild, false))
                     ConvertSubTreeToLong(child, i2lchild, true);
                  break;

               default:
                  if (i2lchild->getOpCode().isLong()) // get rid of unnecessary i2l
                     {
                     if (!performTransformation(comp(), "%sRemoving i2l node %p from parent %p\n", OPT_DETAILS, child,parentNode))
                        break;

                     rcount_t numi2lRefs = child->getReferenceCount();
                     if (numi2lRefs < 2)
                        {
                        parentNode->setChild(i, i2lchild);
                        }
                     else
                        {
                        ReplaceI2LNode(child, i2lchild);
                        }
                     }
                  continue; // look at next child
               }
            }
         }
      }
   }

// -------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------
int32_t TR_SignExtendLoads::perform()
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   if (trace())
     {
       traceMsg(comp(), "Starting Sign Extention of Loads\n");
       traceMsg(comp(), "\nCFG before loop simplification:\n");
       getDebug()->print(comp()->getOutFile(), comp()->getFlowGraph());
     }

   TR::TreeTop* currentTree = comp()->getStartTree();
   TR::TreeTop* prevTree = NULL;

   vcount_t visitCount1 = comp()->incVisitCount();
   TR_ScratchList<TR::Node> i2lList(trMemory());
   TR_ScratchList<TR::Node> useri2lList(trMemory());

   InitializeHashTable();

   bool nodesGatheredInLastBlock = false;
   bool alreadyChecked=false;
   bool isExtensionBlock = false;
   while (currentTree)
      {
      TR::Node *firstNodeInTree = currentTree->getNode();
      TR::ILOpCodes opCode = firstNodeInTree->getOpCodeValue();


      /*
       * Process node per every EBB segment.
       * We know the end of EBB if we encounter:
       *
       * BBEnd (last one)
       *                       or
       * BBEnd
       * BBStart       // doesn't have "is extension of previous block flag"
       */
      if (nodesGatheredInLastBlock &&
          opCode == TR::BBEnd &&
          (currentTree->getNextTreeTop() == NULL ||
          (currentTree->getNextTreeTop() &&
          !currentTree->getNextTreeTop()->getNode()->getBlock()->isExtensionOfPreviousBlock())))
         {
         nodesGatheredInLastBlock = false;

         if (!alreadyChecked && !performTransformation(comp(), "%sSign Extending Loads\n", OPT_DETAILS))
            break;

         alreadyChecked = true;
         static char * noAddSub = feGetEnv("TR_NOADDSUB");
         ProcessNodeList(i2lList, noAddSub == NULL);
         ProcessNodeList(useri2lList, false);

         // reset
         emptyHashTable(); //minimize collisions
         i2lList.init();
         useri2lList.init();
         }
      else
         {
         nodesGatheredInLastBlock = gatheri2lNodes(NULL, firstNodeInTree, i2lList, useri2lList, false)
                                    || nodesGatheredInLastBlock;
         }
      prevTree = currentTree;
      currentTree = currentTree->getNextTreeTop();
      }

   return 1;
   }

const char *
TR_SignExtendLoads::optDetailString() const throw()
   {
   return "O^O SIGN EXTEND LOADS: ";
   }
