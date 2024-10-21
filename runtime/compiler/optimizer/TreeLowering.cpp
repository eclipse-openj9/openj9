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

#include "optimizer/TreeLowering.hpp"

#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "il/Block.hpp"
#include "il/Block_inlines.hpp"
#include "infra/ILWalk.hpp"
#include "optimizer/J9TransformUtil.hpp"

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

   if (trace())
      {
      comp()->dumpMethodTrees("Trees before Tree Lowering Optimization");
      }

   TransformationManager transformations(comp()->region());

   TR::ResolvedMethodSymbol* methodSymbol = comp()->getMethodSymbol();
   for (TR::PreorderNodeIterator nodeIter(methodSymbol->getFirstTreeTop(), comp()); nodeIter != NULL; ++nodeIter)
      {
      TR::Node* node = nodeIter.currentNode();
      TR::TreeTop* tt = nodeIter.currentTree();

      lowerValueTypeOperations(transformations, node, tt);
      }

   transformations.doTransformations();

   if (trace())
      {
      comp()->dumpMethodTrees("Trees after Tree Lowering Optimization");
      }

   return 0;
   }

void
TR::TreeLowering::Transformer::moveNodeToEndOfBlock(TR::Block* const block, TR::TreeTop* const tt, TR::Node* const node, bool isAddress)
   {
   TR::Compilation* comp = this->comp();
   TR::TreeTop* blockExit = block->getExit();
   TR::TreeTop* iterTT = tt->getNextTreeTop();

   if (iterTT != blockExit)
      {
      if (trace())
         {
         traceMsg(comp, "Moving treetop containing node n%dn [%p] for helper call to end of prevBlock in preparation of final block split\n", tt->getNode()->getGlobalIndex(), tt->getNode());
         }

      // Remove TreeTop for call node, and gather it and the treetops for stores that
      // resulted from un-commoning in a TreeTop chain from tt to lastTTForCallBlock
      tt->unlink(false);
      TR::TreeTop* lastTTForCallBlock = tt;

      while (iterTT != blockExit)
         {
         TR::TreeTop* nextTT = iterTT->getNextTreeTop();
         TR::ILOpCodes op = iterTT->getNode()->getOpCodeValue();
         bool isStoreOp = false;

         if (isAddress)
            {
            isStoreOp = (op == TR::aRegStore || op == TR::astore);
            }
         else
            {
            isStoreOp = (op == TR::iRegStore || op == TR::istore);
            }

         if (isStoreOp && iterTT->getNode()->getFirstChild() == node)
            {
            if (trace())
               {
               traceMsg(comp, "Moving treetop containing node n%dn [%p] for store of helper call result to end of prevBlock in preparation of final block split\n", iterTT->getNode()->getGlobalIndex(), iterTT->getNode());
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
      blockExit->getPrevTreeTop()->join(tt);
      lastTTForCallBlock->join(blockExit);
      }
   }

TR::Block*
TR::TreeLowering::Transformer::splitForFastpath(TR::Block* const block, TR::TreeTop* const splitPoint, TR::Block* const targetBlock)
   {
   TR::CFG* const cfg = comp()->getFlowGraph();
   TR::Block* const newBlock = block->split(splitPoint, cfg);
   newBlock->setIsExtensionOfPreviousBlock(true);
   cfg->addEdge(block, targetBlock);
   return newBlock;
   }

/**
 * @brief Copy register dependencies between GlRegDeps node at exit points.
 *
 * This function is only intended to work with GlRegDeps nodes for exit points,
 * (i.e. BBEnd, branch, or jump nodes) within the same extended basic block.
 *
 * Register dependencies are copied "logically", meaning that the actual node
 * used to represent a dependency won't necessarily be copied. If the reg dep
 * is represented by a PassThrough, then the node itself is copied and its
 * child is commoned (so it's lifetime is extended; note that in correctly-formed
 * IL, the child must also be the child of a reg store in the containing block).
 * Otherwise, the dependency must be represented by a reg load, which must have
 * come from the GlRegDeps node at the entry point and *must* be commoned
 * (so it won't get copied).
 *
 * In addition, this function allows *one* register dependency to be changed
 * (substituted). That is, if a register dependency is found under `sourceNode`
 * for the same register that is set on `substituteNode`, then `substituteNode`
 * will be used instead of the dependency from `sourceNode`. Note that the
 * reference of of `substituteNode` is incremented if/when it gets added. If
 * `substituteNode` is NULL the no substitution will be attempted.
 *
 * @param targetNode is the GlRegDeps node that reg deps are copied to
 * @param sourceNode is the GlRegDeps node that reg deps are copied from
 * @param substituteNode is the reg dep node to substitute if a matching register is found in `sourceNode` (NULL if none)
 */
static void
copyExitRegDepsAndSubstitute(TR::Node* const targetNode, TR::Node* const sourceNode, TR::Node* const substituteNode)
   {
   for (int i = 0; i < sourceNode->getNumChildren(); ++i)
      {
      TR::Node* child = sourceNode->getChild(i);
      if (substituteNode
          && child->getLowGlobalRegisterNumber() == substituteNode->getLowGlobalRegisterNumber()
          && child->getHighGlobalRegisterNumber() == substituteNode->getHighGlobalRegisterNumber())
         targetNode->setAndIncChild(i, substituteNode);
      else if (child->getOpCodeValue() == TR::PassThrough)
         {
         // PassThrough nodes cannot be commoned because doing so does not
         // actually anchor the child, causing it's lifetime to not be extended
         child = TR::Node::copy(child);
         if (child->getFirstChild())
            {
            child->getFirstChild()->incReferenceCount();
            }
         child->setReferenceCount(1);
         targetNode->setChild(i, child);
         }
      else
         {
         // all other nodes must be commoned as they won't get evaluated otherwise
         targetNode->setAndIncChild(i, child);
         }
      }
   }

/**
 * @brief Add a GlRegDeps node to a branch by copying some other GlRegDeps.
 *
 * Given branch node, adds a GlRegDeps node by copying the dependencies from
 * a different GlRegDeps. This function allows *one* register dependency to
 * be changed (substituted). See `copyExitRegDepsAndSubstitue()` for details.
 *
 * Note that the branch node is assumed to *not* have a GlRegDeps node already.
 *
 * Returns a pointer to the newly created GlRegDeps. This is can be particularly
 * useful to have when doing a substitution (e.g. for chaining calls).
 *
 * If the source GlRegDeps is NULL, then nothing is done and NULL is returned.
 *
 * @param branchNode is the branch node the GlRegDeps will be added to
 * @param sourceGlRegDepsNode is the GlRegDeps node used to copy the reg deps from
 * @param substituteNode is the reg dep node to be subsituted (NULL if none)
 * @return TR::Node* the newly created GlRegDeps or NULL if `sourceGlRegDepsNode` was NULL
 */
static TR::Node*
copyBranchGlRegDepsAndSubstitute(TR::Node* const branchNode, TR::Node* const sourceGlRegDepsNode, TR::Node* const substituteNode)
   {
   TR::Node* glRegDepsCopy = NULL;
   if (sourceGlRegDepsNode != NULL)
      {
      glRegDepsCopy = TR::Node::create(TR::GlRegDeps, sourceGlRegDepsNode->getNumChildren());
      copyExitRegDepsAndSubstitute(glRegDepsCopy, sourceGlRegDepsNode, substituteNode);
      branchNode->addChildren(&glRegDepsCopy, 1);
      }
   return glRegDepsCopy;
   }

class AcmpTransformer: public TR::TreeLowering::Transformer
   {
   public:
   explicit AcmpTransformer(TR::TreeLowering* opt)
      : TR::TreeLowering::Transformer(opt)
      {}

   void lower(TR::Node* const node, TR::TreeTop* const tt);
   };

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
 *  +--------------------------+
 *  |ttprev                    |
 *  |treetop                   |
 *  |  icall acmp{eq|ne}Helper |
 *  |    aload lhs             |
 *  |    aload rhs             |
 *  |ificmpeq --> ...          |
 *  |  ==> icall               |
 *  |  iconst 0                |
 *  |BBEnd                     |
 *  +--------------------------+
 *
 *  ...becomes...
 *
 *
 * +------------------------------+
 * |ttprev                        |
 * |iRegStore x                   |
 * |  iconst E                    |
 * |ifacmpeq  +->---------------------------+
 * |  aload lhs                   |         |
 * |  aload rhs                   |         |
 * |  GlRegDeps                   |         |
 * |    PassThrough x             |         |
 * |      ==> iconst E            |         |
 * |    PassThrough ...           |         |
 * |BBEnd                         |         |
 * +------------------------------+         |
 * |BBStart (extension)           |         |
 * |iRegStore x                   |         |
 * |  iconst U                    |         |
 * |ifacmpeq +->----------------------------+
 * |  aload lhs                   |         |
 * |  aconst 0                    |         |
 * |  GlRegDeps                   |         |
 * |    PassThrough x             |         |
 * |      ==> iconst U            |         |
 * |    PassThrough ...           |         |
 * |BBEnd                         |         |
 * +------------------------------+         |
 * |BBStart (extension)           |         |
 * |ifacmpeq +------------------------------+
 * |  aload rhs                   |         |
 * |  ==> aconst 0                |         |
 * |  GlRegDeps                   |         |
 * |    PassThrough x             |         |
 * |      ==> iconst U            |         |
 * |    PassThrough ...           |         |
 * |BBEnd                         |         |
 * +------------------------------+         |
 * |BBStart (extension)           |         |
 * |ificmpeq +->----------------------------+
 * |  iand                        |         |
 * |    iloadi ClassFlags         |         |
 * |      aloadi J9Class          |         |
 * |        aload lhs             |         |
 * |    iconst J9ClassIsValueType |         |
 * |  iconst U                    |         |
 * |  GlRegDeps                   |         |
 * |    PassThrough x             |         |
 * |      ==> iconst U            |         |
 * |    PassThrough ...           |         |
 * |BBEnd                         |         |
 * +------------------------------+         |
 * |BBStart (extension)           |         |
 * |ificmpne +->-----------------------+    |
 * |  iand                        |    |    |
 * |    iloadi ClassFlags         |    |    |
 * |      aloadi J9Class          |    |    |
 * |        aload rhs             |    |    |
 * |    iconst J9ClassIsValueType |    |    |
 * |  iconst U                    |    |    |
 * |BBEnd                         |    |    |
 * |  GlRegDeps                   |    |    |
 * |    PassThrough x             |    |    |
 * |      ==> iconst U            |    |    |
 * |    PassThrough ...           |    |    |
 * +------------------------------+    |    |
 *       |                             |    |
 *       +-----------------------------|----+
 *       |                             |
 *       v                             |
 * +-----+-----------+                 |
 * |BBStart          *<----------------|----+
 * |ificmpeq +-> ... *                 |    |
 * |  iRegLoad x     |                 |    |
 * |  iconst 0       |                 |    |
 * |BBEnd            |                 |    |
 * +-----------------+                 |    |
 *                                     |    |
 * +------------------------------+    |    |
 * |BBStart                       *<---+    |
 * |iRegStore x                   |         |
 * |  icall acmp{eq|ne}Helper     |         |
 * |    aload lhs                 |         |
 * |    aload rhs                 |         |
 * |Goto -->----------------------*---------+
 * |  GlRegDeps                   |
 * |    PassThrough x             |
 * |      ==> icall acmp...       |
 * |    PassThrough ...           |
 * |BBEnd                         |
 * +------------------------------+
 *
 * where E represents the result if the two references are equal, and U
 * represents the result if the two references are unequal - that is,
 * E == 1 and U == 0 if a call to acmpeqHelper is being lowered, and
 * E == 0 and U == 1 if a call to acmpneHelper is being lowered.
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
AcmpTransformer::lower(TR::Node * const node, TR::TreeTop * const tt)
   {
   TR::Compilation* comp = this->comp();
   TR::CFG* cfg = comp->getFlowGraph();
   cfg->invalidateStructure();

   if (!performTransformation(comp, "%sPreparing for post-GRA block split by anchoring helper call and arguments\n", optDetailString()))
      return;

   // Anchor call node after split point to ensure the returned value goes into
   // either a temp or a global register.
   auto* const anchoredCallTT = TR::TreeTop::create(comp, tt, TR::Node::create(TR::treetop, 1, node));
   if (trace())
      traceMsg(comp, "Anchoring call node under treetop n%un (0x%p)\n", anchoredCallTT->getNode()->getGlobalIndex(), anchoredCallTT->getNode());

   // Anchor the call arguments just before the call. This ensures the values are
   // live before the call so that we can propagate their values in global registers if needed.
   auto* const anchoredCallArg1TT = TR::TreeTop::create(comp, tt->getPrevTreeTop(), TR::Node::create(TR::treetop, 1, node->getFirstChild()));
   auto* const anchoredCallArg2TT = TR::TreeTop::create(comp, tt->getPrevTreeTop(), TR::Node::create(TR::treetop, 1, node->getSecondChild()));
   if (trace())
      {
      traceMsg(comp, "Anchoring call arguments n%un and n%un under treetops n%un and n%un\n",
         node->getFirstChild()->getGlobalIndex(), node->getSecondChild()->getGlobalIndex(), anchoredCallArg1TT->getNode()->getGlobalIndex(), anchoredCallArg2TT->getNode()->getGlobalIndex());
      }

   // Are we working with an equality comparison or an inequality comparison?
   const bool isObjectEqualityTest = (node->getSymbolReference() == comp->getSymRefTab()->findOrCreateAcmpeqHelperSymbolRef());
   const int cmpResultForEquality = isObjectEqualityTest ? 1 : 0;
   const int cmpResultForInequality = isObjectEqualityTest ? 0 : 1;

   // Split the block at the call TreeTop so that the new block created
   // after the call can become a merge point for all the fastpaths.
   TR::Block* callBlock = tt->getEnclosingBlock();
   if (!performTransformation(comp, "%sSplitting block_%d at TreeTop [0x%p], which holds helper call node n%un\n", optDetailString(), callBlock->getNumber(), tt, node->getGlobalIndex()))
      return;
   TR::Block* targetBlock = callBlock->splitPostGRA(tt->getNextTreeTop(), cfg, true, NULL);
   if (trace())
      traceMsg(comp, "Call node n%un is in block %d, targetBlock is %d\n", node->getGlobalIndex(), callBlock->getNumber(), targetBlock->getNumber());

   // As the block is split after the helper call node, it is possible that as part of un-commoning
   // code to store nodes into registers or temp-slots is appended to the original block by the call
   // to splitPostGRA above.  Move the acmp helper call treetop to the end of prevBlock, along with
   // any stores resulting from un-commoning of the nodes in the helper call tree so that it can be
   // split into its own call block.
   moveNodeToEndOfBlock(callBlock, tt, node, false /* isAddress */);

   if (!performTransformation(comp, "%sInserting fastpath for lhs == rhs\n", optDetailString()))
      return;

   // Insert store of constant cmpResultForEquality as the result of the fastpath.
   // If the references are ultimately being tested for equality, cmpResultForEquality
   // is 1, indicating the test is satisfied; if they are being tested for inequality,
   // cmpResultForEquality is 0, indicating the test is unsatisfied.
   // The value must go wherever the value returned by the helper call goes
   // so that the code in the target block (merge point) picks up the constant
   // if the branch is taken. Use the TreeTop previously inserted to anchor the
   // call to figure out where the return value of the call is being put.
   TR::Node* anchoredNode = anchoredCallTT->getNode()->getFirstChild(); // call node is under a treetop node
   if (trace())
      traceMsg(comp, "Anchored call has been transformed into %s node n%un\n", anchoredNode->getOpCode().getName(), anchoredNode->getGlobalIndex());
   auto* constForEqualityNode = TR::Node::iconst(cmpResultForEquality);
   TR::Node* storeNode = NULL;
   TR::Node* regDepForStoreNode = NULL; // this is the reg dep for the store if one is needed
   if (anchoredNode->getOpCodeValue() == TR::iRegLoad)
      {
      if (trace())
         traceMsg(comp, "Storing constant %d in register %s\n", cmpResultForEquality, comp->getDebug()->getGlobalRegisterName(anchoredNode->getGlobalRegisterNumber()));
      auto const globalRegNum = anchoredNode->getGlobalRegisterNumber();
      storeNode = TR::Node::create(TR::iRegStore, 1, constForEqualityNode);
      storeNode->setGlobalRegisterNumber(globalRegNum);
      // Since the result is in a global register, we're going to need a PassThrough
      // on the exit point GlRegDeps.
      regDepForStoreNode = TR::Node::create(TR::PassThrough, 1, constForEqualityNode);
      regDepForStoreNode->setGlobalRegisterNumber(globalRegNum);
      }
   else if (anchoredNode->getOpCodeValue() == TR::iload)
      {
      if (trace())
         traceMsg(comp, "Storing constant %d to symref %d (%s)\n", cmpResultForEquality, anchoredNode->getSymbolReference()->getReferenceNumber(), anchoredNode->getSymbolReference()->getName(comp->getDebug()));
      storeNode = TR::Node::create(TR::istore, 1, constForEqualityNode);
      storeNode->setSymbolReference(anchoredNode->getSymbolReference());
      }
   else
      TR_ASSERT_FATAL_WITH_NODE(anchoredNode, false, "Anchored call has been turned into unexpected opcode\n");
   tt->insertBefore(TR::TreeTop::create(comp, storeNode));

   // If the BBEnd of the block containing the call has a GlRegDeps node,
   // a matching GlRegDeps node will be needed for all the branches. The
   // fallthrough of the call block and the branch targets will be the
   // same block. So, all register dependencies will be mostly the same.
   // `exitGlRegDeps` is intended to point to the "reference" node used to
   // create the GlRegDeps for each consecutive branch.
   TR::Node* exitGlRegDeps = NULL;
   if (callBlock->getExit()->getNode()->getNumChildren() > 0)
      {
      exitGlRegDeps = callBlock->getExit()->getNode()->getFirstChild();
      }

   // Insert fastpath for lhs == rhs (reference comparison), taking care to set the
   // proper register dependencies by copying them from the BBEnd of the call block
   // (through `exitGlRegDeps`) when needed.
   auto* ifacmpeqNode = TR::Node::createif(TR::ifacmpeq, anchoredCallArg1TT->getNode()->getFirstChild(), anchoredCallArg2TT->getNode()->getFirstChild(), targetBlock->getEntry());
   exitGlRegDeps = copyBranchGlRegDepsAndSubstitute(ifacmpeqNode, exitGlRegDeps, regDepForStoreNode);
   tt->insertBefore(TR::TreeTop::create(comp, ifacmpeqNode));
   callBlock = splitForFastpath(callBlock, tt, targetBlock);
   if (trace())
      traceMsg(comp, "Added check node n%un; call node is now in block_%d\n", ifacmpeqNode->getGlobalIndex(), callBlock->getNumber());

   if (!performTransformation(comp, "%sInserting fastpath for lhs == NULL\n", optDetailString()))
      return;

   // Create store of cmpResultForInequality as fastpath result for cases where the references
   // are unequal by duplicating the node used to store the constant cmpResultForEquality and
   // updating the value being stored.  Also duplicate the corresponding regdep if needed.
   storeNode = storeNode->duplicateTree(true);
   storeNode->getFirstChild()->setInt(cmpResultForInequality);
   tt->insertBefore(TR::TreeTop::create(comp, storeNode));
   if (regDepForStoreNode != NULL)
      {
      regDepForStoreNode = TR::Node::copy(regDepForStoreNode);
      regDepForStoreNode->setReferenceCount(0);
      regDepForStoreNode->setAndIncChild(0, storeNode->getFirstChild());
      }

   auto* const nullConst = TR::Node::aconst(0);
   // Using a similar strategy as above, insert check for lhs == NULL.
   if (!anchoredCallArg1TT->getNode()->getFirstChild()->isNonNull())
      {
      auto* const checkLhsNull = TR::Node::createif(TR::ifacmpeq, anchoredCallArg1TT->getNode()->getFirstChild(), nullConst, targetBlock->getEntry());
      exitGlRegDeps = copyBranchGlRegDepsAndSubstitute(checkLhsNull, exitGlRegDeps, regDepForStoreNode);
      // Set regDepForStoreNode to NULL so that the subsequent calls to copyBranchGlRegDepsAndSubstitute do not need to substitute it.
      regDepForStoreNode = NULL;
      tt->insertBefore(TR::TreeTop::create(comp, checkLhsNull));
      callBlock = splitForFastpath(callBlock, tt, targetBlock);
      if (trace())
         traceMsg(comp, "Added check node n%un; call node is now in block_%d\n", checkLhsNull->getGlobalIndex(), callBlock->getNumber());
      }
   else
      {
      if (trace())
         traceMsg(comp, "Skip fastpath for lhs == NULL because node n%un isNonNull\n", anchoredCallArg1TT->getNode()->getFirstChild()->getGlobalIndex());
      }


   if (!performTransformation(comp, "%sInserting fastpath for rhs == NULL\n", optDetailString()))
      return;

   if (!anchoredCallArg2TT->getNode()->getFirstChild()->isNonNull())
      {
      auto* const checkRhsNull = TR::Node::createif(TR::ifacmpeq, anchoredCallArg2TT->getNode()->getFirstChild(), nullConst, targetBlock->getEntry());
      // regDepForStoreNode might or might not be NULL here depending on if (lhs == NULL) is skipped
      exitGlRegDeps = copyBranchGlRegDepsAndSubstitute(checkRhsNull, exitGlRegDeps, regDepForStoreNode);
      // Set regDepForStoreNode to NULL so that the subsequent calls to copyBranchGlRegDepsAndSubstitute do not need to substitute it.
      regDepForStoreNode = NULL;
      tt->insertBefore(TR::TreeTop::create(comp, checkRhsNull));
      callBlock = splitForFastpath(callBlock, tt, targetBlock);
      if (trace())
         traceMsg(comp, "Added check node n%un; call node is now in block_%d\n", checkRhsNull->getGlobalIndex(), callBlock->getNumber());
      }
   else
      {
      if (trace())
         traceMsg(comp, "Skip fastpath for rhs == NULL because node n%un isNonNull\n", anchoredCallArg2TT->getNode()->getFirstChild()->getGlobalIndex());
      }

   if (!performTransformation(comp, "%sInserting fastpath for lhs is VT\n", optDetailString()))
      return;

   auto* const vftSymRef = comp->getSymRefTab()->findOrCreateVftSymbolRef();

   auto* const lhsVft = TR::Node::createWithSymRef(node, TR::aloadi, 1, anchoredCallArg1TT->getNode()->getFirstChild(), vftSymRef);
   auto* const isLhsValueType = comp->fej9()->testIsClassValueType(lhsVft);
   auto* const checkLhsIsVT = TR::Node::createif(TR::ificmpeq, isLhsValueType, TR::Node::iconst(0), targetBlock->getEntry());
   // regDepForStoreNode might or might not be NULL here depending on if (lhs == NULL) or  (rhs == NULL) is skipped
   exitGlRegDeps = copyBranchGlRegDepsAndSubstitute(checkLhsIsVT, exitGlRegDeps, regDepForStoreNode);
   // Set regDepForStoreNode to NULL so that the subsequent calls to copyBranchGlRegDepsAndSubstitute do not need to substitute it.
   regDepForStoreNode = NULL;
   tt->insertBefore(TR::TreeTop::create(comp, checkLhsIsVT));
   callBlock = splitForFastpath(callBlock, tt, targetBlock);
   if (trace())
      traceMsg(comp, "Added check node n%un; call node is now in block_%d\n", checkLhsIsVT->getGlobalIndex(), callBlock->getNumber());

   if (!performTransformation(comp, "%sInserting fastpath for rhs is VT\n", optDetailString()))
      return;

   // Put call in it's own block so it will be eaisy to move. Importantly,
   // the block *cannot* be an extension because everything *must* be uncommoned.
   auto* const prevBlock = callBlock;
   callBlock = callBlock->splitPostGRA(tt, cfg, true, NULL);

   if (trace())
      traceMsg(comp, "Call node isolated in block_%d by splitPostGRA\n", callBlock->getNumber());

   // Move call block out of line.
   // The CFG edge that exists from prevBlock to callBlock is kept because
   // it will be needed once the branch for the fastpath gets added.
   cfg->findLastTreeTop()->insertTreeTopsAfterMe(callBlock->getEntry(), callBlock->getExit());
   prevBlock->getExit()->join(targetBlock->getEntry());
   cfg->addEdge(prevBlock, targetBlock);
   if (trace())
      traceMsg(comp, "Moved call block to end of method\n");

   // Create and insert branch.
   auto* const rhsVft = TR::Node::createWithSymRef(node, TR::aloadi, 1, anchoredCallArg2TT->getNode()->getFirstChild(), vftSymRef);
   auto* const isRhsValueType = comp->fej9()->testIsClassValueType(rhsVft);
   auto* const checkRhsIsNotVT = TR::Node::createif(TR::ificmpne, isRhsValueType, TR::Node::iconst(0), callBlock->getEntry());
   // Because we've switched the fallthrough and target blocks, the register
   // dependencies also need to be switched.
   if (prevBlock->getExit()->getNode()->getNumChildren() > 0)
      {
      auto* const bbEnd = prevBlock->getExit()->getNode();
      checkRhsIsNotVT->setChild(2, bbEnd->getChild(0));
      checkRhsIsNotVT->setNumChildren(3);
      bbEnd->setNumChildren(0);
      }
   if (exitGlRegDeps)
      {
      auto* const bbEnd = prevBlock->getExit()->getNode();
      auto* glRegDeps = TR::Node::create(TR::GlRegDeps, exitGlRegDeps->getNumChildren());
      copyExitRegDepsAndSubstitute(glRegDeps, exitGlRegDeps, NULL);
      bbEnd->addChildren(&glRegDeps, 1);
      }
   prevBlock->append(TR::TreeTop::create(comp, checkRhsIsNotVT));
   // Note: there's no need to add a CFG edge because one already exists from
   // before callBlock was moved.
   if (trace())
      traceMsg(comp, "Added check node n%un\n", checkRhsIsNotVT->getGlobalIndex());

   // Insert goto target block in outline block.
   auto* const gotoNode = TR::Node::create(node, TR::Goto, 0, targetBlock->getEntry());
   callBlock->append(TR::TreeTop::create(comp, gotoNode));
   // Note: callBlock already has a CFG edge to targetBlock
   // from before it got moved, so adding one here is not required.

   // Move exit GlRegDeps in callBlock.
   // The correct dependencies should have been inserted by splitPostGRA,
   // so they just need to be moved from the BBEnd to the Goto.
   if (callBlock->getExit()->getNode()->getNumChildren() > 0)
      {
      auto* const bbEnd = callBlock->getExit()->getNode();
      auto* glRegDeps = bbEnd->getChild(0);
      bbEnd->setNumChildren(0);
      glRegDeps->decReferenceCount();
      gotoNode->addChildren(&glRegDeps, 1);
      }
   }

static void
copyRegisterDependency(TR::Node *fromNode, TR::Node *toNode)
   {
   if (fromNode->getNumChildren() != 0)
      {
      TR::Node *blkDeps = fromNode->getFirstChild();
      TR::Node *newDeps = TR::Node::create(blkDeps, TR::GlRegDeps);

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

         newDeps->addChildren(&regDep, 1);
         }

      toNode->addChildren(&newDeps, 1);
      }
   }

class NonNullableArrayNullStoreCheckTransformer: public TR::TreeLowering::Transformer
   {
   public:
   explicit NonNullableArrayNullStoreCheckTransformer(TR::TreeLowering* opt)
      : TR::TreeLowering::Transformer(opt)
      {}

   void lower(TR::Node* const node, TR::TreeTop* const tt);
   };

/**
 * If value types are enabled, a check of whether a null reference is being
 * assigned to a value type array might be needed.  This is represented in the
 * IL with a call to the <nonNullableArrayNullStoreCheck> non-helper.
 *
 * Lower the call by splitting the block before the call, and replacing it
 a with a NULLCHK guarded by tests of whether the value is null and whether
 * the array's component type is a value type.  If the value is known to be
 * non-null at this point in the compilation, the call is simply removed.
 *
 * @param node is the current node in the tree walk
 * @param tt is the treetop at the root of the tree ancoring the current node
 */
void
NonNullableArrayNullStoreCheckTransformer::lower(TR::Node* const node, TR::TreeTop* const tt)
   {
   // Transform
   //   +-----------------------------------------+
   //   | ttprev                                  |
   //   | treetop                                 |
   //   |   call <nonNullableArrayNullStoreCheck> |
   //   |     <value-reference>                   |
   //   |     <array-reference>                   |
   //   | ttnext                                  |
   //   +-----------------------------------------+
   //
   // into
   //   +--------------------------------+
   //   | ttprev                         |
   //   | treetop                        |
   //   |   <array-reference>            |
   //   | treetop                        |
   //   |   <value-reference>            |
   //   | ifacmpne  -->------------------*---------+
   //   |   ==><value-reference>         |         |
   //   |   aconst 0                     |         |
   //   | BBEnd                          |         |
   //   +--------------------------------+         |
   //   | BBStart (Extension)            |         |
   //   | ZEROCHK jitArrayStoreException |         |
   //   |   icmpeq                       |         |
   //   |     iand                       |         |
   //   |       iloadi <isClassFlags>    |         |
   //   |         aloadi <vft-symbol>    |         |
   //   |           ==><array-reference> |         |
   //   |       iconst J9ClassArrayIsNullRestricted|
   //   |     iconst 0                   |         |
   //   | BBEnd                          |         |
   //   +--------------------------------+         |
   //                   |                          |
   //                   +--------------------------+
   //                   |
   //                   v
   //   +--------------------------------+
   //   | BBStart                        |
   //   | ttnext                         |
   //   +--------------------------------+
   //
   TR::Node *sourceChild = node->getFirstChild();
   TR::Node *destChild = node->getSecondChild();

   if (!sourceChild->isNonNull())
      {
      TR::CFG * cfg = comp()->getFlowGraph();
      cfg->invalidateStructure();

      TR::Block *prevBlock = tt->getEnclosingBlock();
      TR::TreeTop *anchoredArrayTT = TR::TreeTop::create(comp(), tt->getPrevTreeTop(), TR::Node::create(TR::treetop, 1, destChild));
      TR::TreeTop *anchoredSourceTT = TR::TreeTop::create(comp(), anchoredArrayTT, TR::Node::create(TR::treetop, 1, sourceChild));

      TR::TreeTop *nextTT = tt->getNextTreeTop();
      tt->getPrevTreeTop()->join(nextTT);
      TR::Block *nextBlock = prevBlock->splitPostGRA(nextTT, cfg);

      TR::SymbolReference* const vftSymRef = comp()->getSymRefTab()->findOrCreateVftSymbolRef();

      TR::Node *vftNode = TR::Node::createWithSymRef(node, TR::aloadi, 1, destChild, vftSymRef);

      TR::Node *testIsNullRestrictedArray = comp()->fej9()->testIsArrayClassNullRestrictedType(vftNode);
      TR::Node *testIsNotNullRestrictedArray = TR::Node::create(TR::icmpeq, 2, testIsNullRestrictedArray, TR::Node::iconst(0));

      TR::ResolvedMethodSymbol *currentMethod = comp()->getMethodSymbol();

      TR::SymbolReference *jitThrowArrayStoreException = comp()->getSymRefTab()->findOrCreateArrayStoreExceptionSymbolRef(currentMethod);
      TR::Node *checkNotNullRestrictedArray = TR::Node::createWithSymRef(TR::ZEROCHK, 1, 1, testIsNotNullRestrictedArray, jitThrowArrayStoreException);
      TR::TreeTop *checkNotNullRestrictedArrayTT = prevBlock->append(TR::TreeTop::create(comp(), checkNotNullRestrictedArray));

      bool enableTrace = trace();
      auto* const nullConst = TR::Node::aconst(0);
      auto* const checkValueNull = TR::Node::createif(TR::ifacmpne, sourceChild, nullConst, nextBlock->getEntry());

      // Copy register dependencies from the end of the block split to the
      // ifacmpne, which checks for a null value, that's being added to the
      // end of that block
      //
      copyRegisterDependency(prevBlock->getExit()->getNode(), checkValueNull);

      TR::TreeTop *checkValueNullTT = checkNotNullRestrictedArrayTT->insertBefore(TR::TreeTop::create(comp(), checkValueNull));

      if (enableTrace)
         {
         traceMsg(comp(),"checkValueNull n%dn is inserted before  n%dn in prevBlock %d\n", checkValueNull->getGlobalIndex(), checkNotNullRestrictedArray->getGlobalIndex(), prevBlock->getNumber());
         }

      TR::Block *checkNotNullRestrictedBlock = prevBlock->split(checkNotNullRestrictedArrayTT, cfg);
      checkNotNullRestrictedBlock->setIsExtensionOfPreviousBlock(true);

      cfg->addEdge(prevBlock, nextBlock);

      if (enableTrace)
         {
         traceMsg(comp(),"checkNotNullRestrictedArray n%dn is isolated in checkNotNullRestrictedBlock %d\n", checkNotNullRestrictedArray->getGlobalIndex(), checkNotNullRestrictedBlock->getNumber());
         }

      cfg->addEdge(checkNotNullRestrictedBlock, nextBlock);
      }
   else
      {
      tt->getPrevTreeTop()->join(tt->getNextTreeTop());
      }

   node->recursivelyDecReferenceCount();
   }

static TR::Node *
cloneAndTweakGlRegDepsFromBBExit(TR::Node *bbExitNode, TR::Compilation *comp, bool enableTrace, TR_GlobalRegisterNumber registerToSkip)
   {
   TR::Node *tmpGlRegDeps = NULL;
   if (bbExitNode->getNumChildren() > 0)
      {
      TR::Node *origRegDeps = bbExitNode->getFirstChild();
      tmpGlRegDeps = TR::Node::create(origRegDeps, TR::GlRegDeps);

      for (int32_t i=0; i < origRegDeps->getNumChildren(); ++i)
         {
         TR::Node *regDep = origRegDeps->getChild(i);

         if ((registerToSkip != -1) &&
             regDep->getOpCodeValue() == TR::PassThrough &&
             regDep->getGlobalRegisterNumber() == registerToSkip)
            {
            if (enableTrace)
               traceMsg(comp,"tmpGlRegDeps skip n%dn [%d] %s %s\n",
                     regDep->getGlobalIndex(), i, regDep->getOpCode().getName(), comp->getDebug()->getGlobalRegisterName(regDep->getGlobalRegisterNumber()));
            continue;
            }
         else
            {
            if (enableTrace)
               traceMsg(comp,"tmpGlRegDeps add n%dn [%d] %s %s\n",
                     regDep->getGlobalIndex(), i, regDep->getOpCode().getName(), comp->getDebug()->getGlobalRegisterName(regDep->getGlobalRegisterNumber()));
            }

         if (regDep->getOpCodeValue() == TR::PassThrough)
            {
            TR::Node *orig = regDep;
            regDep = TR::Node::create(orig, TR::PassThrough, 1, orig->getFirstChild());
            regDep->setLowGlobalRegisterNumber(orig->getLowGlobalRegisterNumber());
            regDep->setHighGlobalRegisterNumber(orig->getHighGlobalRegisterNumber());
            }

         tmpGlRegDeps->addChildren(&regDep, 1);
         }
      }

   return tmpGlRegDeps;
   }

static TR::Node *
createStoreNodeForAnchoredNode(TR::Node *anchoredNode, TR::Node *nodeToBeStored, TR::Compilation *comp, bool enableTrace, const char *msg)
   {
   TR::Node *storeNode = NULL;

   // After splitPostGRA anchoredNode which was the helper call node
   // should have been transformed into aRegLoad or aload
   if (anchoredNode->getOpCodeValue() == TR::aRegLoad)
      {
      storeNode = TR::Node::create(TR::aRegStore, 1, nodeToBeStored);
      storeNode->setGlobalRegisterNumber(anchoredNode->getGlobalRegisterNumber());
      if (enableTrace)
         traceMsg(comp, "Storing %s n%dn in register %s storeNode n%dn anchoredNode n%dn\n", msg, nodeToBeStored->getGlobalIndex(), comp->getDebug()->getGlobalRegisterName(anchoredNode->getGlobalRegisterNumber()), storeNode->getGlobalIndex(), anchoredNode->getGlobalIndex());
      }
   else if (anchoredNode->getOpCodeValue() == TR::aload)
      {
      storeNode = TR::Node::create(TR::astore, 1, nodeToBeStored);
      storeNode->setSymbolReference(anchoredNode->getSymbolReference());
      if (enableTrace)
         traceMsg(comp, "Storing %s n%dn to symref %d (%s) storeNode n%dn anchoredNode n%dn\n", msg, nodeToBeStored->getGlobalIndex(), anchoredNode->getSymbolReference()->getReferenceNumber(), anchoredNode->getSymbolReference()->getName(comp->getDebug()), storeNode->getGlobalIndex(), anchoredNode->getGlobalIndex());
      }
   else
      {
      TR_ASSERT_FATAL_WITH_NODE(anchoredNode, false, "Anchored call node n%dn has been turned into unexpected opcode\n",
                                anchoredNode->getGlobalIndex());
      }

   return storeNode;
   }

class LoadArrayElementTransformer: public TR::TreeLowering::Transformer
   {
   public:
   explicit LoadArrayElementTransformer(TR::TreeLowering* opt)
      : TR::TreeLowering::Transformer(opt)
      {}

   void lower(TR::Node* const node, TR::TreeTop* const tt);
   };

static bool skipBoundChecks(TR::Compilation *comp, TR::Node *node)
   {
   TR::ResolvedMethodSymbol *method = comp->getOwningMethodSymbol(node->getOwningMethod());
   if (method && method->skipBoundChecks())
      return true;
   return false;
   }

/*
 * LoadArrayElementTransformer transforms the block that contains the jitLoadFlattenableArrayElement helper call into three blocks:
 *   1. The merge block (blockAfterHelperCall) that contains the tree tops after the helper call
 *   2. The helper call block (helperCallBlock) that contains the helper call and is moved to the end of the tree top list
 *   3. The new nullable array load block (arrayElementLoadBlock) which is an extended block of the original block
 *
 *      originalBlock----------+
 *      arrayElementLoadBlock  |
 *              |              |
 *              |              v
 *              |       helperCallBlock
 *              |              |
 *              v              v
 *            blockAfterHelperCall
 *
 *
 Before:
 +----------------------------------------+
 |NULLCHK                                 |
 |   PassThrough                          |
 |      ==>iRegLoad  (array address)      |
 |treetop                                 |
 |  acall  jitLoadFlattenableArrayElement |
 |     ==>iRegLoad                        |
 |     ==>aRegLoad                        |
 |ttAfterHelperCall                       |
 +----------------------------------------+

 After:
 +------------------------------------------+
 |BBStart                                   |
 |NULLCHK                                   |
 |   PassThrough                            |
 |      ==>iRegLoad (array address)         |
 |treetop                                   |
 |   ==>aRegLoad                            |
 |ificmpne ----->---------------------------+-----------+
 |   iand                                   |           |
 |      iloadi  <isClassFlags>              |           |
 |      ...                                 |           |
 |      iconst J9ClassArrayIsNullRestricted |           |
 |   iconst 0                               |           |
 |   GlRegDeps ()                           |           |
 |      ==>aRegLoad                         |           |
 |      ==>iRegLoad                         |           |
 |BBEnd                                     |           |
 +------------------------------------------+           |
 +------------------------------------------+           |
 |BBStart (extension of previous block)     |           |
 |BNDCHK                                    |           |
 |   ==>arraylength                         |           |
 |   ==>iRegLoad                            |           |
 |compressedRefs                            |           |
 |   aloadi                                 |           |
 |      aladd                               |           |
 |        ...                               |           |
 |   lconst 0                               |           |
 |aRegStore edi                             |           |
 |   ==>aloadi                              |           |
 |BBEnd                                     |           |
 |   GlRegDeps ()                           |           |
 |      PassThrough rdi                     |           |
 |         ==>aloadi                        |           |
 |      ==>aRegLoad                         |           |
 |      ==>iRegLoad                         |           |
 +---------------------+--------------------+           |
                       |                                |
                       +----------------------------+   |
                       |                            |   |
 +---------------------v--------------------+       |   |
 |BBStart                                   |       |   |
 |   GlRegDeps ()                           |       |   |
 |      aRegLoad r9d                        |       |   |
 |      iRegLoad ebx                        |       |   |
 |      aRegLoad edi                        |       |   |
 |treetop                                   |       |   |
 |   ==>aRegLoad                            |       |   |
 |ttAfterHelperCall                         |       |   |
 +------------------------------------------+       |   |
                                                    |   |
                       +----------------------------|---+
                       |                            |
                       |                            |
 +---------------------v---------------------+      |
 |BBStart                                    |      |
 |   GlRegDeps ()                            |      |
 |   ==>aRegLoad                             |      |
 |   ==>iRegLoad                             |      |
 |treetop                                    |      |
 |   acall  jitLoadFlattenableArrayElement   |      |
 |      ==>iRegLoad                          |      |
 |      ==>aRegLoad                          |      |
 |aRegStore edi                              |      |
 |    ==>acall                               |      |
 |goto  ----->-------------------------------+------+
 |   GlRegDeps ()                            |
 |      ==>aRegLoad                          |
 |      ==>iRegLoad                          |
 |      PassThrough rdi                      |
 |         ==>acall                          |
 |BBEnd                                      |
 +-------------------------------------------+
 *
 */
void
LoadArrayElementTransformer::lower(TR::Node* const node, TR::TreeTop* const tt)
   {
   bool enableTrace = trace();
   TR::Compilation *comp = this->comp();
   TR::Block *originalBlock = tt->getEnclosingBlock();

   TR::Node *elementIndexNode = node->getFirstChild();
   TR::Node *arrayBaseAddressNode = node->getSecondChild();

   if (!performTransformation(comp, "%sTransforming loadArrayElement treetop n%dn node n%dn [%p] in block_%d: elementIndexNode n%dn arrayBaseAddressNode n%dn ttAfterHelperCall n%dn\n", optDetailString(),
          tt->getNode()->getGlobalIndex(), node->getGlobalIndex(), node, originalBlock->getNumber(),
          elementIndexNode->getGlobalIndex(), arrayBaseAddressNode->getGlobalIndex(), tt->getNextTreeTop()->getNode()->getGlobalIndex()))
      return;

   const char *counterName = TR::DebugCounter::debugCounterName(comp, "vt-helper/inlinecheck/aaload/(%s)/bc=%d", comp->signature(), node->getByteCodeIndex());
   TR::DebugCounter::incStaticDebugCounter(comp, counterName);

   TR::CFG *cfg = comp->getFlowGraph();
   cfg->invalidateStructure();

   ///////////////////////////////////////
   // 1. Anchor helper call node after the helper call
   //    Anchor elementIndex and arrayBaseAddress before the helper call

   // Anchoring the helper call ensures that the return value from the helper call or from the nullable array element load
   // will be saved to a register or a temp.
   TR::TreeTop *anchoredCallTT = TR::TreeTop::create(comp, tt, TR::Node::create(TR::treetop, 1, node));
   TR::TreeTop *anchoredElementIndexTT = TR::TreeTop::create(comp, tt->getPrevTreeTop(), TR::Node::create(TR::treetop, 1, elementIndexNode));
   TR::TreeTop *anchoredArrayBaseAddressTT = TR::TreeTop::create(comp, anchoredElementIndexTT, TR::Node::create(TR::treetop, 1, arrayBaseAddressNode));

   if (enableTrace)
      traceMsg(comp, "Anchored call node under treetop n%un (0x%p), elementIndex under treetop n%un (0x%p), arrayBaseAddress under treetop n%un (0x%p)\n",
            anchoredCallTT->getNode()->getGlobalIndex(), anchoredCallTT->getNode(),
            anchoredElementIndexTT->getNode()->getGlobalIndex(), anchoredElementIndexTT->getNode(),
            anchoredArrayBaseAddressTT->getNode()->getGlobalIndex(), anchoredArrayBaseAddressTT->getNode());


   ///////////////////////////////////////
   // 2. Split (1) after the helper call
   TR::Block *blockAfterHelperCall = originalBlock->splitPostGRA(anchoredCallTT, cfg, true, NULL);

   if (enableTrace)
      traceMsg(comp, "Isolated the anchored call treetop n%dn in block_%d\n", anchoredCallTT->getNode()->getGlobalIndex(), blockAfterHelperCall->getNumber());


   ///////////////////////////////////////
   // 3. Save the GlRegDeps of the originalBlock's BBExit
   //
   // The GlRegDeps for the BBExit of the originalBlock after the first split is what is required for
   // the BBExit of arrayElementLoadBlock. It has to be saved because it might get changed after the next split at the helper call.
   //
   // If the anchoredNode is saved to a register, there is a PassThrough for the return value from
   // the helper call. It should not be copied here, otherwise the helper call will get moved up to
   // the GlRegDeps of arrayElementLoadBlock BBExit due to un-commoning in split.
   TR::Node *anchoredNode = anchoredCallTT->getNode()->getFirstChild();
   TR_GlobalRegisterNumber registerToSkip = (anchoredNode->getOpCodeValue() == TR::aRegLoad) ? anchoredNode->getGlobalRegisterNumber() : -1;
   TR::Node *tmpGlRegDeps = cloneAndTweakGlRegDepsFromBBExit(originalBlock->getExit()->getNode(), comp, enableTrace, registerToSkip);


   ///////////////////////////////////////
   // 4. Move the helper call and the aRegStore/astore of the call (if exist) to the end of the block before next split
   moveNodeToEndOfBlock(originalBlock, tt, node, true /* isAddress */);


   ///////////////////////////////////////
   // 5. Split (2) at the helper call
   TR::Block *helperCallBlock = originalBlock->splitPostGRA(tt, cfg, true, NULL);

   if (enableTrace)
      traceMsg(comp, "Isolated helper call treetop n%dn node n%dn in block_%d\n", tt->getNode()->getGlobalIndex(), node->getGlobalIndex(), helperCallBlock->getNumber());


   ///////////////////////////////////////
   // 6. Create array element load node and append to the originalBlock
   TR::Node *anchoredArrayBaseAddressNode = anchoredArrayBaseAddressTT->getNode()->getFirstChild();
   TR::Node *anchoredElementIndexNode = anchoredElementIndexTT->getNode()->getFirstChild();

   TR::Node *elementAddress = J9::TransformUtil::calculateElementAddress(comp, anchoredArrayBaseAddressNode, anchoredElementIndexNode, TR::Address);
   TR::SymbolReference *elementSymRef = comp->getSymRefTab()->findOrCreateArrayShadowSymbolRef(TR::Address, anchoredArrayBaseAddressNode);
   TR::Node *elementLoadNode = TR::Node::createWithSymRef(comp->il.opCodeForIndirectArrayLoad(TR::Address), 1, 1, elementAddress, elementSymRef);
   elementLoadNode->copyByteCodeInfo(node);

   TR::TreeTop *elementLoadTT = NULL;
   if (comp->useCompressedPointers())
      elementLoadTT = originalBlock->append(TR::TreeTop::create(comp, TR::Node::createCompressedRefsAnchor(elementLoadNode)));
   else
      elementLoadTT = originalBlock->append(TR::TreeTop::create(comp, TR::Node::create(node, TR::treetop, 1, elementLoadNode)));

   if (enableTrace)
      traceMsg(comp, "Created array element load treetop n%dn node n%dn\n", elementLoadTT->getNode()->getGlobalIndex(), elementLoadNode->getGlobalIndex());


   ///////////////////////////////////////
   // 7. Store the return value from the array element load to the same register or temp used by the anchored node
   TR::Node *storeArrayElementNode = createStoreNodeForAnchoredNode(anchoredNode, elementLoadNode, comp, enableTrace, "array element load");

   elementLoadTT->insertAfter(TR::TreeTop::create(comp, storeArrayElementNode));

   if (enableTrace)
      traceMsg(comp, "Store array element load node n%dn to n%dn %s\n", elementLoadNode->getGlobalIndex(), storeArrayElementNode->getGlobalIndex(), storeArrayElementNode->getOpCode().getName());


   ///////////////////////////////////////
   // 8. Split (3) at the array element load node
   TR::Block *arrayElementLoadBlock = originalBlock->split(elementLoadTT, cfg);

   arrayElementLoadBlock->setIsExtensionOfPreviousBlock(true);

   if (enableTrace)
      traceMsg(comp, "Isolated array element load treetop n%dn node n%dn in block_%d\n", elementLoadTT->getNode()->getGlobalIndex(), elementLoadNode->getGlobalIndex(), arrayElementLoadBlock->getNumber());

   int32_t dataWidth = TR::Symbol::convertTypeToSize(TR::Address);
   if (comp->useCompressedPointers())
      dataWidth = TR::Compiler->om.sizeofReferenceField();

   //ILGen for array element load already generates a NULLCHK

   // Some JCL methods are known never to trigger an ArrayIndexOutOfBoundsException,
   // so only add a BNDCHK if it's needed
   if (!skipBoundChecks(comp, node))
      {
      TR::Node *arraylengthNode = TR::Node::create(TR::arraylength, 1, anchoredArrayBaseAddressNode);
      arraylengthNode->setArrayStride(dataWidth);

      elementLoadTT->insertBefore(TR::TreeTop::create(comp, TR::Node::createWithSymRef(TR::BNDCHK, 2, 2, arraylengthNode, anchoredElementIndexNode, comp->getSymRefTab()->findOrCreateArrayBoundsCheckSymbolRef(comp->getMethodSymbol()))));

      // This might be the first time the various checking symbol references are used
      // Need to ensure aliasing for them is correctly constructed
      //
      this->optimizer()->setAliasSetsAreValid(false);
      }


   ///////////////////////////////////////
   // 9. Create ificmpne node that checks classFlags

   TR::SymbolReference* const vftSymRef = comp->getSymRefTab()->findOrCreateVftSymbolRef();

   TR::Node *vftNode = TR::Node::createWithSymRef(node, TR::aloadi, 1, anchoredArrayBaseAddressNode, vftSymRef);
   TR::Node *testIsArrayClassNullRestrictedNode = comp->fej9()->testIsArrayClassNullRestrictedType(vftNode);

   // The branch destination will be set up later
   TR::Node *ifNode = TR::Node::createif(TR::ificmpne, testIsArrayClassNullRestrictedNode, TR::Node::iconst(0));

   // Copy register dependency to the ificmpne node that's being appended to the current block
   copyRegisterDependency(arrayElementLoadBlock->getExit()->getNode(), ifNode);

   // Append the ificmpne node that checks classFlags to the original block
   originalBlock->append(TR::TreeTop::create(comp, ifNode));

   if (enableTrace)
      traceMsg(comp, "Append ifNode n%dn to block_%d\n", ifNode->getGlobalIndex(), originalBlock->getNumber());


   ///////////////////////////////////////
   // 10. Copy tmpGlRegDeps to arrayElementLoadBlock

   // Adjust the reference count of the GlRegDeps of the BBExit since it will be replaced by
   // the previously saved tmpGlRegDeps
   if (arrayElementLoadBlock->getExit()->getNode()->getNumChildren() > 0)
      {
      TR::Node *origRegDeps = arrayElementLoadBlock->getExit()->getNode()->getFirstChild();
      prepareToReplaceNode(origRegDeps);
      origRegDeps->decReferenceCount();
      arrayElementLoadBlock->getExit()->getNode()->setNumChildren(0);
      }

   if (tmpGlRegDeps)
      {
      arrayElementLoadBlock->getExit()->getNode()->setNumChildren(1);
      arrayElementLoadBlock->getExit()->getNode()->setAndIncChild(0, tmpGlRegDeps);
      }

   // Add storeArrayElementNode to GlRegDeps to pass the return value from the array element load to blockAfterHelperCall
   if (storeArrayElementNode->getOpCodeValue() == TR::aRegStore)
      {
      TR::Node *blkDeps = arrayElementLoadBlock->getExit()->getNode()->getFirstChild();

      TR::Node *depNode = TR::Node::create(TR::PassThrough, 1, storeArrayElementNode->getChild(0));
      depNode->setGlobalRegisterNumber(storeArrayElementNode->getGlobalRegisterNumber());
      blkDeps->addChildren(&depNode, 1);
      }


   ///////////////////////////////////////
   // 11. Set up the edges between the blocks
   ifNode->setBranchDestination(helperCallBlock->getEntry());

   cfg->addEdge(originalBlock, helperCallBlock);

   cfg->removeEdge(arrayElementLoadBlock, helperCallBlock);
   cfg->addEdge(arrayElementLoadBlock, blockAfterHelperCall);

   // Move helper call to the end of the tree list
   arrayElementLoadBlock->getExit()->join(blockAfterHelperCall->getEntry());

   TR::TreeTop *lastTreeTop = comp->getMethodSymbol()->getLastTreeTop();
   lastTreeTop->insertTreeTopsAfterMe(helperCallBlock->getEntry(), helperCallBlock->getExit());

   // Add Goto from the helper call to the merge block
   TR::Node *gotoAfterHelperCallNode = TR::Node::create(helperCallBlock->getExit()->getNode(), TR::Goto, 0, blockAfterHelperCall->getEntry());
   helperCallBlock->append(TR::TreeTop::create(comp, gotoAfterHelperCallNode));

   if (helperCallBlock->getExit()->getNode()->getNumChildren() > 0)
      {
      TR::Node *deps = helperCallBlock->getExit()->getNode()->getChild(0);
      helperCallBlock->getExit()->getNode()->setNumChildren(0);
      deps->decReferenceCount();
      gotoAfterHelperCallNode->addChildren(&deps, 1);
      }
   }

class StoreArrayElementTransformer: public TR::TreeLowering::Transformer
   {
   public:
   explicit StoreArrayElementTransformer(TR::TreeLowering* opt)
      : TR::TreeLowering::Transformer(opt)
      {}

   void lower(TR::Node* const node, TR::TreeTop* const tt);
   };

static bool skipArrayStoreChecks(TR::Compilation *comp, TR::Node *node)
   {
   TR::ResolvedMethodSymbol *method = comp->getOwningMethodSymbol(node->getOwningMethod());
   if (method && method->skipArrayStoreChecks())
      return true;
   return false;
   }

/*
 * StoreArrayElementTransformer transforms the block that contains the jitStoreFlattenableArrayElement helper call into three blocks:
 *   1. The merge block that contains the tree tops after the helper call
 *   2. The helper call block that contains the helper call and is moved to the end of the tree top list
 *   3. The new nullable array store block which is an extended block of the original block
 *
 *      originalBlock ----------+
 *      arrayElementStoreBlock  |
 *               |              |
 *               |              v
 *               |       helperCallBlock
 *               |              |
 *               v              v
 *             blockAfterHelperCall
 *
 Before:
 +-------------------------------------------+
 |NULLCHK                                    |
 |   PassThrough                             |
 |      aload <ArrayAddress>                 |
 |treetop                                    |
 |   acall  jitStoreFlattenableArrayElement  |
 |      aload <value>                        |
 |      iload <index>                        |
 |      aload <arrayAddress>                 |
 |ttAfterHelperCall                          |
 +-------------------------------------------+

 After:
 +-------------------------------------------+
 |BBStart                                    |
 |NULLCHK                                    |
 |   PassThrough                             |
 |      aload <ArrayAddress>                 |
 |treetop                                    |
 |   aload <index>                           |
 |treetop                                    |
 |   aload <value>                           |
 |ificmpne ------>---------------------------+---------------+
 |   iand                                    |               |
 |      iloadi  <isClassFlags>               |               |
 |      ...                                  |               |
 |      iconst J9ClassArrayIsNullRestricted  |               |
 |   iconst 0                                |               |
 |   GlRegDeps ()                            |               |
 |      PassThrough rcx                      |               |
 |         ==>aload                          |               |
 |      PassThrough r8                       |               |
 |         ==>aload                          |               |
 |      PassThrough rdi                      |               |
 |         ==>iload                          |               |
 |BBEnd                                      |               |
 +-------------------------------------------+               |
 +-------------------------------------------+               |
 |BBStart (extension of previous block)      |               |
 |BNDCHK                                     |               |
 |   ...                                     |               |
 |treetop                                    |               |
 |   ArrayStoreCHK                           |               |
 |      awrtbari                             |               |
 |      ...                                  |               |
 |BBEnd                                      |               |
 |   GlRegDeps ()                            |               |
 |      PassThrough rcx                      |               |
 |         ==>aload                          |               |
 |      PassThrough r8                       |               |
 |         ==>aload                          |               |
 |      PassThrough rdi                      |               |
 |         ==>iload                          |               |
 +---------------------+---------------------+               |
                       |                                     |
                       +---------------------------+         |
                       |                           |         |
                       |                           |         |
 +---------------------v---------------------+     |         |
 |BBStart                                    |     |         |
 |   GlRegDeps ()                            |     |         |
 |      aRegLoad ecx                         |     |         |
 |      aRegLoad r8d                         |     |         |
 |      iRegLoad edi                         |     |         |
 |ttAfterArrayElementStore                   |     |         |
 +-------------------------------------------+     |         |
                                                   |         |
                       +---------------------------|---------+
                       |                           |
 +---------------------v---------------------+     |
 |BBStart                                    |     |
 |   GlRegDeps ()                            |     |
 |      aRegLoad ecx                         |     |
 |      aRegLoad r8d                         |     |
 |      iRegLoad edi                         |     |
 |treetop                                    |     |
 |   acall  jitStoreFlattenableArrayElement  |     |
 |      ==>aRegLoad                          |     |
 |      ==>iRegLoad                          |     |
 |      ==>aRegLoad                          |     |
 |goto  ------->-----------------------------+-----+
 |   GlRegDeps ()                            |
 |      PassThrough rcx                      |
 |         ==>aload                          |
 |      PassThrough r8                       |
 |         ==>aload                          |
 |      PassThrough rdi                      |
 |         ==>iload                          |
 |BBEnd                                      |
 +-------------------------------------------+
 *
 */
void
StoreArrayElementTransformer::lower(TR::Node* const node, TR::TreeTop* const tt)
   {
   bool enableTrace = trace();
   TR::Compilation *comp = this->comp();
   TR::Block *originalBlock = tt->getEnclosingBlock();

   TR::Node *valueNode = node->getFirstChild();
   TR::Node *elementIndexNode = node->getSecondChild();
   TR::Node *arrayBaseAddressNode = node->getThirdChild();

   if (!performTransformation(comp, "%sTransforming storeArrayElement treetop n%dn node n%dn [%p] in block_%d: children (n%dn, n%dn, n%dn) ttAfterHelperCall n%dn\n", optDetailString(), tt->getNode()->getGlobalIndex(), node->getGlobalIndex(), node, originalBlock->getNumber(),
          valueNode->getGlobalIndex(), elementIndexNode->getGlobalIndex(), arrayBaseAddressNode->getGlobalIndex(), tt->getNextTreeTop()->getNode()->getGlobalIndex()))
      return;

   const char *counterName = TR::DebugCounter::debugCounterName(comp, "vt-helper/inlinecheck/aastore/(%s)/bc=%d", comp->signature(), node->getByteCodeIndex());
   TR::DebugCounter::incStaticDebugCounter(comp, counterName);

   TR::CFG *cfg = comp->getFlowGraph();
   cfg->invalidateStructure();


   ///////////////////////////////////////
   // 1. Anchor all the children nodes before the helper call
   TR::TreeTop *anchoredArrayBaseAddressTT = TR::TreeTop::create(comp, tt->getPrevTreeTop(), TR::Node::create(TR::treetop, 1, arrayBaseAddressNode));
   TR::TreeTop *anchoredElementIndexTT = TR::TreeTop::create(comp, anchoredArrayBaseAddressTT, TR::Node::create(TR::treetop, 1, elementIndexNode));
   TR::TreeTop *anchoredValueTT = TR::TreeTop::create(comp, anchoredElementIndexTT, TR::Node::create(TR::treetop, 1, valueNode));

   if (enableTrace)
      traceMsg(comp, "Anchored elementIndex under treetop n%un (0x%p), arrayBaseAddress under treetop n%un (0x%p), value under treetop n%un (0x%p), \n",
            anchoredElementIndexTT->getNode()->getGlobalIndex(), anchoredElementIndexTT->getNode(),
            anchoredArrayBaseAddressTT->getNode()->getGlobalIndex(), anchoredArrayBaseAddressTT->getNode(),
            anchoredValueTT->getNode()->getGlobalIndex(), anchoredValueTT->getNode());


   ///////////////////////////////////////
   // 2. Split (1) after the helper call
   TR::Block *blockAfterHelperCall = originalBlock->splitPostGRA(tt->getNextTreeTop(), cfg, true, NULL);

   if (enableTrace)
      traceMsg(comp, "Isolated the trees after the helper call in block_%d\n", blockAfterHelperCall->getNumber());


   ///////////////////////////////////////
   // 3. Clone the GlRegDeps of the originalBlock's BBExit
   TR::Node *tmpGlRegDeps = cloneAndTweakGlRegDepsFromBBExit(originalBlock->getExit()->getNode(), comp, enableTrace, -1 /* registerToSkip */);


   ///////////////////////////////////////
   // 4. Move the helper call node to the end of the originalBlock
   TR::TreeTop *originalBlockExit = originalBlock->getExit();
   if (tt->getNextTreeTop() != originalBlockExit)
      {
      tt->unlink(false);
      originalBlockExit->getPrevTreeTop()->join(tt);
      tt->join(originalBlockExit);
      }


   ///////////////////////////////////////
   // 5. Split (2) at the helper call node into its own helperCallBlock

   // Insert NULLCHK for null-restricted VT
   TR::Node *anchoredValueNode = anchoredValueTT->getNode()->getFirstChild();
   TR::TreeTop *ttForHelperCallBlock = tt;

   TR::Block *helperCallBlock = originalBlock->splitPostGRA(ttForHelperCallBlock, cfg, true, NULL);

   if (enableTrace)
      traceMsg(comp, "Isolated helper call treetop n%dn node n%dn in block_%d\n", tt->getNode()->getGlobalIndex(), node->getGlobalIndex(), helperCallBlock->getNumber());


   ///////////////////////////////////////
   // 6. Create the new ArrayStoreCHK
   TR::Node *anchoredElementIndexNode = anchoredElementIndexTT->getNode()->getFirstChild();
   TR::Node *anchoredArrayBaseAddressNode = anchoredArrayBaseAddressTT->getNode()->getFirstChild();

   TR::Node *elementAddress = J9::TransformUtil::calculateElementAddress(comp, anchoredArrayBaseAddressNode, anchoredElementIndexNode, TR::Address);

   TR::SymbolReference *elementSymRef = comp->getSymRefTab()->findOrCreateArrayShadowSymbolRef(TR::Address, anchoredArrayBaseAddressNode);
   TR::Node *elementStoreNode = TR::Node::createWithSymRef(TR::awrtbari, 3, 3, elementAddress, anchoredValueNode, anchoredArrayBaseAddressNode, elementSymRef);

   TR::Node *arrayStoreCHKNode = NULL;
   TR::TreeTop *arrayStoreTT = NULL;

   // Some JCL methods are known never to trigger an ArrayStoreException
   // Only add an ArrayStoreCHK if it's required
   if (!skipArrayStoreChecks(comp, node))
      {
      TR::SymbolReference *arrayStoreCHKSymRef = comp->getSymRefTab()->findOrCreateTypeCheckArrayStoreSymbolRef(comp->getMethodSymbol());
      arrayStoreCHKNode = TR::Node::createWithRoomForThree(TR::ArrayStoreCHK, elementStoreNode, 0, arrayStoreCHKSymRef);
      arrayStoreCHKNode->copyByteCodeInfo(node);
      arrayStoreTT = originalBlock->append(TR::TreeTop::create(comp, arrayStoreCHKNode));
      if (enableTrace)
         traceMsg(comp, "Created arrayStoreCHK treetop n%dn arrayStoreCHKNode n%dn\n", arrayStoreTT->getNode()->getGlobalIndex(), arrayStoreCHKNode->getGlobalIndex());

      // This might be the first time the various checking symbol references are used
      // Need to ensure aliasing for them is correctly constructed
      //
      this->optimizer()->setAliasSetsAreValid(false);
      }
   else
      {
      arrayStoreTT = originalBlock->append(TR::TreeTop::create(comp, TR::Node::create(TR::treetop, 1, elementStoreNode)));
      if (enableTrace)
         traceMsg(comp, "Created treetop node with awrtbari child as treetop n%dn elementStoreNode n%dn\n", arrayStoreTT->getNode()->getGlobalIndex(), elementStoreNode->getGlobalIndex());
      }

   ///////////////////////////////////////
   // 7. Split (3) at the regular array element store
   TR::Block *arrayElementStoreBlock = originalBlock->split(arrayStoreTT, cfg);

   arrayElementStoreBlock->setIsExtensionOfPreviousBlock(true);

   if (enableTrace)
      traceMsg(comp, "Isolated array element store treetop n%dn node n%dn in block_%d\n", arrayStoreTT->getNode()->getGlobalIndex(),
            arrayStoreCHKNode ? arrayStoreCHKNode->getGlobalIndex() : elementStoreNode->getGlobalIndex(), arrayElementStoreBlock->getNumber());

   int32_t dataWidth = TR::Symbol::convertTypeToSize(TR::Address);
   if (comp->useCompressedPointers())
      dataWidth = TR::Compiler->om.sizeofReferenceField();

   const bool needNullCHK = !anchoredArrayBaseAddressNode->isNonNull() && (tt->getNode()->getOpCodeValue() == TR::NULLCHK);
   const bool needBNDCHK = !skipBoundChecks(comp, node);

   if (needNullCHK || needBNDCHK)
      {
      TR::Node *arraylengthNode = TR::Node::create(TR::arraylength, 1, anchoredArrayBaseAddressNode);
      arraylengthNode->setArrayStride(dataWidth);

      //ILGen for array element store already generates a NULLCHK
      //If the helper call node is anchored under NULLCHK due to compactNullChecks, the NULLCHK is split into the helper call block.
      //The NULLCHK is required for regular array block.
      if (needNullCHK)
         {
         arrayStoreTT->insertBefore(TR::TreeTop::create(comp, TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, arraylengthNode, comp->getSymRefTab()->findOrCreateNullCheckSymbolRef(comp->getMethodSymbol()))));
         }


      // Some JCL methods are known never to trigger an ArrayIndexOutOfBoundsException,
      // so only add a BNDCHK if it's needed
      if (needBNDCHK)
         {
         arrayStoreTT->insertBefore(TR::TreeTop::create(comp, TR::Node::createWithSymRef(TR::BNDCHK, 2, 2, arraylengthNode, anchoredElementIndexNode, comp->getSymRefTab()->findOrCreateArrayBoundsCheckSymbolRef(comp->getMethodSymbol()))));
         }

      // This might be the first time the various checking symbol references are used
      // Need to ensure aliasing for them is correctly constructed
      //
      this->optimizer()->setAliasSetsAreValid(false);
      }

   if (comp->useCompressedPointers())
      {
      arrayStoreTT->insertAfter(TR::TreeTop::create(comp, TR::Node::createCompressedRefsAnchor(elementStoreNode)));
      }


   ///////////////////////////////////////
   // 8. Create the ificmpne node that checks classFlags

   TR::SymbolReference* const vftSymRef = comp->getSymRefTab()->findOrCreateVftSymbolRef();

   TR::Node *vftNode = TR::Node::createWithSymRef(node, TR::aloadi, 1, anchoredArrayBaseAddressNode, vftSymRef);
   TR::Node *testIsArrayClassNullRestrictedNode = comp->fej9()->testIsArrayClassNullRestrictedType(vftNode);

   // The branch destination will be set up later
   TR::Node *ifNode = TR::Node::createif(TR::ificmpne, testIsArrayClassNullRestrictedNode, TR::Node::iconst(0));

   // Copy register dependency to the ificmpne node that's being appended to the current block
   copyRegisterDependency(arrayElementStoreBlock->getExit()->getNode(), ifNode);

   // Append the ificmpne node that checks classFlags to the original block
   originalBlock->append(TR::TreeTop::create(comp, ifNode));

   if (enableTrace)
      traceMsg(comp, "Append ifNode n%dn to block_%d\n", ifNode->getGlobalIndex(), originalBlock->getNumber());


   ///////////////////////////////////////
   // 9. Fix the register dependency after ifNode copying

   // Adjust the reference count of the GlRegDeps of the BBExit for arrayElementStoreBlock since it will be replaced by
   // the previously saved tmpGlRegDeps
   if (arrayElementStoreBlock->getExit()->getNode()->getNumChildren() > 0)
      {
      TR::Node *origRegDeps = arrayElementStoreBlock->getExit()->getNode()->getFirstChild();
      prepareToReplaceNode(origRegDeps);
      origRegDeps->decReferenceCount();
      arrayElementStoreBlock->getExit()->getNode()->setNumChildren(0);
      }

   if (tmpGlRegDeps)
      {
      arrayElementStoreBlock->getExit()->getNode()->setNumChildren(1);
      arrayElementStoreBlock->getExit()->getNode()->setAndIncChild(0, tmpGlRegDeps);
      }


   ///////////////////////////////////////
   // 10. Set up the edges between the blocks
   ifNode->setBranchDestination(helperCallBlock->getEntry());

   cfg->addEdge(originalBlock, helperCallBlock);

   cfg->removeEdge(arrayElementStoreBlock, helperCallBlock);
   cfg->addEdge(arrayElementStoreBlock, blockAfterHelperCall);

   arrayElementStoreBlock->getExit()->join(blockAfterHelperCall->getEntry());

   TR::TreeTop *lastTreeTop = comp->getMethodSymbol()->getLastTreeTop();
   lastTreeTop->insertTreeTopsAfterMe(helperCallBlock->getEntry(), helperCallBlock->getExit());

   // Add Goto block from helper call to the merge block
   TR::Node *gotoAfterHelperCallNode = TR::Node::create(helperCallBlock->getExit()->getNode(), TR::Goto, 0, blockAfterHelperCall->getEntry());
   helperCallBlock->append(TR::TreeTop::create(comp, gotoAfterHelperCallNode));

   if (helperCallBlock->getExit()->getNode()->getNumChildren() > 0)
      {
      TR::Node *deps = helperCallBlock->getExit()->getNode()->getChild(0);
      helperCallBlock->getExit()->getNode()->setNumChildren(0);
      deps->decReferenceCount();
      gotoAfterHelperCallNode->addChildren(&deps, 1);
      }
   }

/**
 * @brief Perform lowering related to Valhalla value types
 *
 */
void
TR::TreeLowering::lowerValueTypeOperations(TransformationManager& transformations, TR::Node* node, TR::TreeTop* tt)
   {
   TR::SymbolReferenceTable * symRefTab = comp()->getSymRefTab();
   static char *disableInliningCheckAastore = feGetEnv("TR_DisableVT_AASTORE_Inlining");

   if (node->getOpCode().isCall())
      {
      if (symRefTab->isNonHelper(node->getSymbolReference(), TR::SymbolReferenceTable::loadFlattenableArrayElementNonHelperSymbol))
         {
         node->setSymbolReference(symRefTab->findOrCreateLoadFlattenableArrayElementSymbolRef());
         }
      if (symRefTab->isNonHelper(node->getSymbolReference(), TR::SymbolReferenceTable::storeFlattenableArrayElementNonHelperSymbol))
         {
         node->setSymbolReference(symRefTab->findOrCreateStoreFlattenableArrayElementSymbolRef());
         }

      // IL Generation only uses the <objectInequalityComparison> non-helper today,
      // but just in case, make sure TreeLowering can handle both possibilities.
      const bool isObjectEqualityTest = symRefTab->isNonHelper(node->getSymbolReference(), TR::SymbolReferenceTable::objectEqualityComparisonSymbol);
      const bool isObjectInequalityTest = symRefTab->isNonHelper(node->getSymbolReference(), TR::SymbolReferenceTable::objectInequalityComparisonSymbol);

      if (isObjectEqualityTest || isObjectInequalityTest)
         {
         // turn the non-helper call into a VM helper call
         node->setSymbolReference(isObjectEqualityTest ? symRefTab->findOrCreateAcmpeqHelperSymbolRef()
                                                       : symRefTab->findOrCreateAcmpneHelperSymbolRef());

         static const bool disableAcmpFastPath =  NULL != feGetEnv("TR_DisableVT_AcmpFastpath");
         if (!disableAcmpFastPath)
            {
            transformations.addTransformation(getTransformer<AcmpTransformer>(), node, tt);
            }
         }
      else if (symRefTab->isNonHelper(node->getSymbolReference(), TR::SymbolReferenceTable::nonNullableArrayNullStoreCheckSymbol))
         {
         transformations.addTransformation(getTransformer<NonNullableArrayNullStoreCheckTransformer>(), node, tt);
         }
      else if (node->getSymbolReference()->getReferenceNumber() == TR_ldFlattenableArrayElement)
         {
         static char *disableInliningCheckAaload = feGetEnv("TR_DisableVT_AALOAD_Inlining");

         if (!disableInliningCheckAaload)
            {
            TR_ASSERT_FATAL_WITH_NODE(tt->getNode(),
                                      (tt->getNode()->getOpCodeValue() == TR::treetop) || (tt->getNode()->getOpCodeValue() == TR::NULLCHK),
                                      "LoadArrayElementTransformer cannot process the treetop node that is neither a treetop nor a NULLCHK\n");

            transformations.addTransformation(getTransformer<LoadArrayElementTransformer>(), node, tt);
            }
         }
      else if (node->getSymbolReference()->getReferenceNumber() == TR_strFlattenableArrayElement)
         {
         if (!disableInliningCheckAastore)
            {
            TR_ASSERT_FATAL_WITH_NODE(tt->getNode(),
                                      (tt->getNode()->getOpCodeValue() == TR::treetop) || (tt->getNode()->getOpCodeValue() == TR::NULLCHK),
                                      "StoreArrayElementTransformer cannot process the treetop node that is neither a treetop nor a NULLCHK\n");
            transformations.addTransformation(getTransformer<StoreArrayElementTransformer>(), node, tt);
            }
         }
      }
   }
