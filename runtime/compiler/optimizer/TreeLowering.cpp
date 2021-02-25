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

#include "optimizer/TreeLowering.hpp"

#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "il/Block.hpp"
#include "il/Block_inlines.hpp"
#include "infra/ILWalk.hpp"

const char *
TR::TreeLowering::optDetailString() const throw()
   {
   return "O^O TREE LOWERING: ";
   }

int32_t
TR::TreeLowering::perform()
   {
   if (!TR::Compiler->om.areValueTypesEnabled())
      {
      return 0;
      }

   TR::ResolvedMethodSymbol* methodSymbol = comp()->getMethodSymbol();
   for (TR::PreorderNodeIterator nodeIter(methodSymbol->getFirstTreeTop(), comp()); nodeIter != NULL ; ++nodeIter)
      {
      TR::Node* node = nodeIter.currentNode();
      TR::TreeTop* tt = nodeIter.currentTree();

      if (TR::Compiler->om.areValueTypesEnabled())
         {
         lowerValueTypeOperations(node, tt);
         }
      }

   return 0;
   }

/**
 * @brief Perform lowering related to Valhalla value types
 *
 */
void
TR::TreeLowering::lowerValueTypeOperations(TR::Node* node, TR::TreeTop* tt)
   {
   TR::SymbolReferenceTable * symRefTab = comp()->getSymRefTab();

   if (node->getOpCode().isCall() &&
         symRefTab->isNonHelper(node->getSymbolReference(), TR::SymbolReferenceTable::objectEqualityComparisonSymbol))
      {
      // turn the non-helper call into a VM helper call
      node->setSymbolReference(symRefTab->findOrCreateAcmpHelperSymbolRef());
      static const bool disableAcmpFastPath =  NULL != feGetEnv("TR_DisableAcmpFastpath");
      if (!disableAcmpFastPath)
         {
         fastpathAcmpHelper(node, tt);
         }
      }
   else if (node->getOpCodeValue() == TR::ArrayStoreCHK)
      {
      lowerArrayStoreCHK(node, tt);
      }
   }

/**
 * @brief Add checks to skip (fast-path) acmpHelper call
 *
 * @details
 *
 * This transformation adds checks for the cases where the acmp can be performed
 * without calling the VM helper. The transformed Trees represent the following operation:
 *
 * 1. If the address of lhs and rhs are the same, produce an eq (true) result
 *    and skip the call (note the two objects must be the same regardless of
 *    whether they are value types are reference types)
 * 2. Otherwise, do VM helper call
 *
 * The transformation looks as follows:
 *
 *  +----------------------+
 *  |ttprev                |
 *  |treetop               |
 *  |  icall acmpHelper    |
 *  |    aload lhs         |
 *  |    aload rhs         |
 *  |ificmpeq --> ...      |
 *  |  ==> icall           |
 *  |  iconst 0            |
 *  |BBEnd                 |
 *  +----------------------+
 *
 *  ...becomes...
 *
 *  +----------------------+
 *  |ttprev                |
 *  |iRegStore x           |
 *  |  iconst 1            |
 *  |ifacmpeq  -->---------*---------+
 *  |  aload lhs           |         |
 *  |  aload rhs           |         |
 *  |  GlRegDeps           |         |
 *  |    PassThrough x     |         |
 *  |      ==> iconst 1    |         |
 *  |    PassThrough ...   |         |
 *  |BBEnd                 |         |
 *  +----------------------+         |
 *  |BBStart (extension)   |         |
 *  |iRegStore x           |         |
 *  |  icall acmpHelper    |         |
 *  |    aload lhs         |         |
 *  |    aload rhs         |         |
 *  |BBEnd                 |         |
 *  |  GlRegDeps           |         |
 *  |    PassThrough x     |         |
 *  |      ==> icall acmpHelper      |
 *  |    PassThrough ...   |         |
 *  +----------------------+         |
 *        |                          |
 *        +--------------------------+
 *        |
 *        v
 *  +-----------------+
 *  |BBStart
 *  |ificmpeq --> ... |
 *  |  iRegLoad x     |
 *  |  iconst 0       |
 *  |BBEnd            |
 *  +-----------------+
 *
 * Any GlRegDeps on the extension block are created by OMR::Block::splitPostGRA
 * while those on the ifacmpeq at the end of the first block are copies of those,
 * with the exception of any register (x, above) holding the result of the compare
 *
 * @param node is the current node in the tree walk
 * @param tt is the treetop at the root of the tree ancoring the current node
 *
 */
void
TR::TreeLowering::fastpathAcmpHelper(TR::Node *node, TR::TreeTop *tt)
   {
   TR::Compilation* comp = self()->comp();
   TR::CFG* cfg = comp->getFlowGraph();
   cfg->invalidateStructure();

   // anchor call node after split point to ensure the returned value goes into
   // either a temp or a global register
   auto* anchoredCallTT = TR::TreeTop::create(comp, tt, TR::Node::create(TR::treetop, 1, node));
   if (trace())
      traceMsg(comp, "Anchoring call node under treetop n%dn (0x%p)\n", anchoredCallTT->getNode()->getGlobalIndex(), anchoredCallTT->getNode());

   // anchor the call arguments just before the call
   // this ensures the values are live before the call so that we can
   // propagate their values in global registers if needed
   auto* anchoredCallArg1TT = TR::TreeTop::create(comp, tt->getPrevTreeTop(), TR::Node::create(TR::treetop, 1, node->getFirstChild()));
   auto* anchoredCallArg2TT = TR::TreeTop::create(comp, tt->getPrevTreeTop(), TR::Node::create(TR::treetop, 1, node->getSecondChild()));
   if (trace())
      {
      traceMsg(comp, "Anchoring call arguments n%dn and n%dn under treetops n%dn and n%dn\n",
         node->getFirstChild()->getGlobalIndex(), node->getSecondChild()->getGlobalIndex(), anchoredCallArg1TT->getNode()->getGlobalIndex(), anchoredCallArg2TT->getNode()->getGlobalIndex());
      }

   // put non-helper call in its own block by block splitting at the
   // next treetop and then at the current one
   TR::Block* prevBlock = tt->getEnclosingBlock();
   TR::Block* targetBlock = prevBlock->splitPostGRA(tt->getNextTreeTop(), cfg, true, NULL);

   // As the block is split after the helper call node, it is possible that as part of un-commoning
   // code to store nodes into registers or temp-slots is appended to the original block by the call
   // to splitPostGRA above.  Move the acmp helper call treetop to the end of prevBlock, along with
   // any stores resulting from un-commoning of the nodes in the helper call tree so that it can be
   // split into its own call block.
   TR::TreeTop* prevBlockExit = prevBlock->getExit();
   TR::TreeTop* iterTT = tt->getNextTreeTop();

   if (iterTT != prevBlockExit)
      {
      if (trace())
         {
         traceMsg(comp, "Moving treetop containing node n%dn [%p] for acmp helper call to end of prevBlock in preparation of final block split\n", tt->getNode()->getGlobalIndex(), tt->getNode());
         }

      // Remove TreeTop for call node, and gather it and the treetops for stores that
      // resulted from un-commoning in a TreeTop chain from tt to lastTTForCallBlock
      tt->unlink(false);
      TR::TreeTop* lastTTForCallBlock = tt;

      while (iterTT != prevBlockExit)
         {
         TR::TreeTop* nextTT = iterTT->getNextTreeTop();
         TR::ILOpCodes op = iterTT->getNode()->getOpCodeValue();

         if ((op == TR::iRegStore || op == TR::istore) && iterTT->getNode()->getFirstChild() == node)
            {
            if (trace())
               {
               traceMsg(comp, "Moving treetop containing node n%dn [%p] for store of acmp helper result to end of prevBlock in preparation of final block split\n", iterTT->getNode()->getGlobalIndex(), iterTT->getNode());
               }

            // Remove store node from prevBlock temporarily
            iterTT->unlink(false);
            lastTTForCallBlock->join(iterTT);
            lastTTForCallBlock = iterTT;
            }

         iterTT = nextTT;
         }

      // Move the treetops that were gathered for the call and any stores of the
      // result to the end of the block in preparation for the split of the call block
      prevBlockExit->getPrevTreeTop()->join(tt);
      lastTTForCallBlock->join(prevBlockExit);
      }

   TR::Block* callBlock = prevBlock->split(tt, cfg);
   callBlock->setIsExtensionOfPreviousBlock(true);
   if (trace())
      traceMsg(comp, "Isolated call node n%dn in block_%d\n", node->getGlobalIndex(), callBlock->getNumber());

   // insert store of constant 1
   // the value must go wherever the value returned by the helper call goes
   // so that the code in the target block picks up the constant if we fast-path
   // (i.e. jump around) the call
   TR::Node* anchoredNode = anchoredCallTT->getNode()->getFirstChild(); // call node is under a treetop node
   if (trace())
      traceMsg(comp, "Anchored call has been transformed into %s node n%dn\n", anchoredNode->getOpCode().getName(), anchoredNode->getGlobalIndex());
   auto* const1Node = TR::Node::iconst(1);
   TR::Node* storeNode = NULL;
   if (anchoredNode->getOpCodeValue() == TR::iRegLoad)
      {
      if (trace())
         traceMsg(comp, "Storing constant 1 in register %s\n", comp->getDebug()->getGlobalRegisterName(anchoredNode->getGlobalRegisterNumber()));
      storeNode = TR::Node::create(TR::iRegStore, 1, const1Node);
      storeNode->setGlobalRegisterNumber(anchoredNode->getGlobalRegisterNumber());
      }
   else if (anchoredNode->getOpCodeValue() == TR::iload)
      {
      if (trace())
         traceMsg(comp, "Storing constant 1 to symref %d (%s)\n", anchoredNode->getSymbolReference()->getReferenceNumber(), anchoredNode->getSymbolReference()->getName(comp->getDebug()));
      storeNode = TR::Node::create(TR::istore, 1, const1Node);
      storeNode->setSymbolReference(anchoredNode->getSymbolReference());
      }
   else
      TR_ASSERT_FATAL_WITH_NODE(anchoredNode, false, "Anchored call has been turned into unexpected opcode\n");
   prevBlock->append(TR::TreeTop::create(comp, storeNode));

   // insert acmpeq for fastpath, taking care to set the proper register dependencies
   // Any register dependencies added by splitPostGRA will now be on the BBExit for
   // the call block.  As the ifacmpeq branching around the call block will reach the same
   // target block, copy any GlRegDeps from the end of the call block to the ifacmpeq
   auto* ifacmpeqNode = TR::Node::createif(TR::ifacmpeq, anchoredCallArg1TT->getNode()->getFirstChild(), anchoredCallArg2TT->getNode()->getFirstChild(), targetBlock->getEntry());
   if (callBlock->getExit()->getNode()->getNumChildren() > 0)
      {
      TR::Node* glRegDeps = TR::Node::create(TR::GlRegDeps);
      TR::Node* depNode = NULL;

      if (anchoredNode->getOpCodeValue() == TR::iRegLoad)
         {
         depNode = TR::Node::create(TR::PassThrough, 1, storeNode->getChild(0));
         depNode->setGlobalRegisterNumber(storeNode->getGlobalRegisterNumber());
         glRegDeps->addChildren(&depNode, 1);
         }

      ifacmpeqNode->addChildren(&glRegDeps, 1);

      TR::Node* expectedDeps = callBlock->getExit()->getNode()->getFirstChild();
      for (int i = 0; i < expectedDeps->getNumChildren(); ++i)
         {
         TR::Node* temp = expectedDeps->getChild(i);
         if (depNode && temp->getGlobalRegisterNumber() == depNode->getGlobalRegisterNumber())
            continue;
         else if (temp->getOpCodeValue() == TR::PassThrough)
            {
            // PassThrough nodes cannot be commoned because doing so does not
            // actually anchor the child, causing it's lifetime to not be extended
            TR::Node* original = temp;
            temp = TR::Node::create(original, TR::PassThrough, 1, original->getFirstChild());
            temp->setLowGlobalRegisterNumber(original->getLowGlobalRegisterNumber());
            temp->setHighGlobalRegisterNumber(original->getHighGlobalRegisterNumber());
            }
         glRegDeps->addChildren(&temp, 1);
         }
      }

   prevBlock->append(TR::TreeTop::create(comp, ifacmpeqNode));
   cfg->addEdge(prevBlock, targetBlock);
   }

/**
 * If value types are enabled, and the value that is being assigned to the array
 * element might be a null reference, lower the ArrayStoreCHK by splitting the
 * block before the ArrayStoreCHK, and inserting a NULLCHK guarded by a check
 * of whether the array's component type is a value type.
 *
 * @param node is the current node in the tree walk
 * @param tt is the treetop at the root of the tree ancoring the current node
 */
void
TR::TreeLowering::lowerArrayStoreCHK(TR::Node *node, TR::TreeTop *tt)
   {
   // Pattern match the ArrayStoreCHK operands to get the source of the assignment
   // (sourceChild) and the array to which an element will have a value assigned (destChild)
   TR::Node *firstChild = node->getFirstChild();

   TR::Node *sourceChild = firstChild->getSecondChild();
   TR::Node *destChild = firstChild->getChild(2);

   // Only need to lower if it is possible that the value is a null reference
   if (!sourceChild->isNonNull())
      {
      TR::CFG * cfg = comp()->getFlowGraph();
      cfg->invalidateStructure();

      TR::Block *prevBlock = tt->getEnclosingBlock();

      performTransformation(comp(), "%sTransforming ArrayStoreCHK n%dn [%p] by splitting block block_%d, and inserting a NULLCHK guarded with a check of whether the component type of the array is a value type\n", optDetailString(), node->getGlobalIndex(), node, prevBlock->getNumber());

      // Anchor the node containing the source of the array element
      // assignment and the node that contains the destination array
      // to ensure they are available for the ificmpeq and NULLCHK
      TR::TreeTop *anchoredArrayTT = TR::TreeTop::create(comp(), tt->getPrevTreeTop(), TR::Node::create(TR::treetop, 1, destChild));
      TR::TreeTop *anchoredSourceTT = TR::TreeTop::create(comp(), anchoredArrayTT, TR::Node::create(TR::treetop, 1, sourceChild));

      // Transform
      //   +--------------------------------+
      //   | ttprev                         |
      //   | ArrayStoreCHK                  |
      //   |   astorei/awrtbari             |
      //   |     aladd                      |
      //   |       <array-reference>        |
      //   |       index-offset-calculation |
      //   |     <value-reference>          |
      //   +--------------------------------+
      //
      // into
      //   +--------------------------------+
      //   | treetop                        |
      //   |   <array-reference>            |
      //   | treetop                        |
      //   |   <value-reference>            |
      //   | ificmpeq  -->------------------*---------+
      //   |   iand                         |         |
      //   |     iloadi <isClassFlags>      |         |
      //   |       aloadi <componentClass>  |         |
      //   |         aloadi <vft-symbol>    |         |
      //   |           <array-reference>    |         |
      //   |     iconst J9ClassIsValueType  |         |
      //   |   iconst 0                     |         |
      //   | BBEnd                          |         |
      //   +--------------------------------+         |
      //   | BBStart (Extension)            |         |
      //   | NULLCHK                        |         |
      //   |   Passthrough                  |         |
      //   |     <value-reference>          |         |
      //   | BBEnd                          |         |
      //   +--------------------------------+         |
      //                   |                          |
      //                   +--------------------------+
      //                   |
      //                   v
      //   +--------------------------------+
      //   | BBStart                        |
      //   | ArrayStoreCHK                  |
      //   |   astorei/awrtbari             |
      //   |     aladd                      |
      //   |       aload <array>            |
      //   |       index-offset-calculation |
      //   |     aload <value>              |
      //   +--------------------------------+
      //
      TR::SymbolReference *vftSymRef = comp()->getSymRefTab()->findOrCreateVftSymbolRef();
      TR::SymbolReference *arrayCompSymRef = comp()->getSymRefTab()->findOrCreateArrayComponentTypeSymbolRef();
      TR::SymbolReference *classFlagsSymRef = comp()->getSymRefTab()->findOrCreateClassFlagsSymbolRef();

      TR::Node *vft = TR::Node::createWithSymRef(node, TR::aloadi, 1, anchoredArrayTT->getNode()->getFirstChild(), vftSymRef);
      TR::Node *arrayCompClass = TR::Node::createWithSymRef(node, TR::aloadi, 1, vft, arrayCompSymRef);
      TR::Node *loadClassFlags = TR::Node::createWithSymRef(node, TR::iloadi, 1, arrayCompClass, classFlagsSymRef);
      TR::Node *isValueTypeNode = TR::Node::create(node, TR::iand, 2, loadClassFlags, TR::Node::iconst(node, J9ClassIsValueType));

      TR::Node *ifNode = TR::Node::createif(TR::ificmpeq, isValueTypeNode, TR::Node::iconst(node, 0));
      ifNode->copyByteCodeInfo(node);

      TR::Node *passThru  = TR::Node::create(node, TR::PassThrough, 1, sourceChild);
      TR::ResolvedMethodSymbol *currentMethod = comp()->getMethodSymbol();

      TR::Block *arrayStoreCheckBlock = prevBlock->splitPostGRA(tt, cfg);

      ifNode->setBranchDestination(arrayStoreCheckBlock->getEntry());

      // Copy register dependencies from the end of the block split before the
      // ArrayStoreCHK to the ificmpeq that's being added to the end of that block
      if (prevBlock->getExit()->getNode()->getNumChildren() != 0)
         {
         TR::Node *blkDeps = prevBlock->getExit()->getNode()->getFirstChild();
         TR::Node *ifDeps = TR::Node::create(blkDeps, TR::GlRegDeps);

         for (int i = 0; i < blkDeps->getNumChildren(); i++)
            {
            TR::Node *regDep = blkDeps->getChild(i);

            if (regDep->getOpCodeValue() == TR::PassThrough)
               {
               TR::Node *orig= regDep;
               regDep = TR::Node::create(orig, TR::PassThrough, 1, orig->getFirstChild());
               regDep->setLowGlobalRegisterNumber(orig->getLowGlobalRegisterNumber());
               regDep->setHighGlobalRegisterNumber(orig->getHighGlobalRegisterNumber());
               }

            ifDeps->addChildren(&regDep, 1);
            }

         ifNode->addChildren(&ifDeps, 1);
         }

      prevBlock->append(TR::TreeTop::create(comp(), ifNode));

      TR::Node *nullCheck = TR::Node::createWithSymRef(node, TR::NULLCHK, 1, passThru,
                               comp()->getSymRefTab()->findOrCreateNullCheckSymbolRef(currentMethod));
      TR::TreeTop *nullCheckTT = prevBlock->append(TR::TreeTop::create(comp(), nullCheck));

      TR::Block *nullCheckBlock = prevBlock->split(nullCheckTT, cfg);

      nullCheckBlock->setIsExtensionOfPreviousBlock(true);

      cfg->addEdge(prevBlock, arrayStoreCheckBlock);
      }
   }
