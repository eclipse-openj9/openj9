/*******************************************************************************
 * Copyright (c) 2017, 2020 IBM Corp. and others
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
#include "JProfilingValue.hpp"

#include "il/Block.hpp"
#include "infra/Cfg.hpp"
#include "infra/TRCfgEdge.hpp"
#include "infra/Checklist.hpp"
#include "infra/ILWalk.hpp"
#include "infra/List.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "il/Node_inlines.hpp"
#include "infra/Checklist.hpp"
#include "ras/DebugCounter.hpp"
#include "runtime/J9Profiler.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "codegen/CodeGenerator.hpp"
#include "optimizer/TransformUtil.hpp"

/**
 * Get the operation for direct store for a type.
 * Its not possible to reuse existing methods, as they may
 * widen the operation.
 *
 * \param The datatype to support.
 */
TR::ILOpCodes
directStore(TR::DataType dt)
   {
   switch (dt)
      {
      case TR::Address:
         return TR::astore;
      case TR::Int64:
         return TR::lstore;
      case TR::Int32:
         return TR::istore;
      case TR::Int16:
         return TR::sstore;
      case TR::Int8:
         return TR::bstore;
      default:
         TR_ASSERT_FATAL(0, "Datatype not supported for store");
      }
   return TR::BadILOp;
   }

/**
 * Get the operation for indirect store for a type.
 * Its not possible to reuse existing methods, as they may
 * widen the operation.
 *
 * \param The datatype to support.
 */
TR::ILOpCodes
indirectStore(TR::DataType dt)
   {
   switch (dt)
      {
      case TR::Address:
         return TR::astorei;
      case TR::Int64:
         return TR::lstorei;
      case TR::Int32:
         return TR::istorei;
      case TR::Int16:
         return TR::sstorei;
      case TR::Int8:
         return TR::bstorei;
      default:
         TR_ASSERT_FATAL(0, "Datatype not supported for indirect store");
      }
   return TR::BadILOp;
   }

/**
 * Get the operation for indirect load for a type.
 * Its not possible to reuse existing methods, as they may
 * widen the operation.
 *
 * \param The datatype to support.
 */
TR::ILOpCodes
indirectLoad(TR::DataType dt)
   {
   switch (dt)
      {
      case TR::Address:
         return TR::aloadi;
      case TR::Int64:
         return TR::lloadi;
      case TR::Int32:
         return TR::iloadi;
      case TR::Int16:
         return TR::sloadi;
      case TR::Int8:
         return TR::bloadi;
      default:
         TR_ASSERT_FATAL(0, "Datatype not supported for indirect load");
      }
   return TR::BadILOp;
   }

/**
 * Get the operation for const for a type.
 * Its not possible to reuse existing methods, as they may
 * widen the operation.
 *
 * \param The datatype to support.
 */
TR::ILOpCodes
loadConst(TR::DataType dt)
   {
   switch (dt)
      {
      case TR::Address:
         return TR::aconst;
      case TR::Int64:
         return TR::lconst;
      case TR::Int32:
         return TR::iconst;
      case TR::Int16:
         return TR::sconst;
      case TR::Int8:
         return TR::bconst;
      default:
         TR_ASSERT_FATAL(0, "Datatype not supported for const");
      }
   return TR::BadILOp;
   }

/**
 * JProfilingValue
 *
 * JProfilingValue will insert value profiling trees and lower any existing placeholder
 * profiling trees. It can operate in two different modes:
 *
 * For non-profiling compilations, placeholder trees are not expected. Instead, the optimization
 * pass will instrument virtual calls, instanceofs and checkcasts for profiling.
 *
 * For profiling compilations, placeholder trees will be identified and lowered, assuming the compilation
 * has been configured to use JProfiling.
 */
int32_t
TR_JProfilingValue::perform() 
   {
   if (comp()->getProfilingMode() == JProfiling)
      {
      if (trace())
         traceMsg(comp(), "JProfiling has been enabled for profiling compilations, run JProfilingValue\n");
      }
   else if (comp()->getOption(TR_EnableJProfiling))
      {
      if (trace())
         traceMsg(comp(), "JProfiling has been enabled, run JProfilingValue\n");
      }
   else 
      {
      if (trace())
         traceMsg(comp(), "JProfiling has been disabled, skip JProfilingValue\n");
      return 0;
      }
   
   cleanUpAndAddProfilingCandidates();
   if (trace())
      comp()->dumpMethodTrees("After Cleaning up Trees"); 
   lowerCalls();

   if (comp()->isProfilingCompilation())
      {
      TR::Recompilation *recomp = comp()->getRecompilationInfo();
      TR_ValueProfiler *profiler = recomp->getValueProfiler();
      TR_ASSERT(profiler, "Recompilation should have a ValueProfiler in a profiling compilation");
      profiler->setPostLowering();
      }
   return 1;
   }

void TR_JProfilingValue::cleanUpAndAddProfilingCandidates()
   {
   TR::TreeTop *cursor = comp()->getStartTree();
   TR_BitVector *alreadyProfiledValues = new (comp()->trStackMemory()) TR_BitVector();
   TR::NodeChecklist checklist(comp());
   while (cursor)
      {
      TR::Node *node = cursor->getNode();
      /*
       * If we find a profiling candidate while walking the treetop, we add a place holder call to profile the value. 
       * In this scenario a placeholder call will be places after the treetop in which we encounter a candidate and decided to
       * profile it. To make sure we do not examine unnecessary trees, record next tree top before examining the tree and inserting a profiling placeholder. 
       */
      TR::TreeTop *nextTT = cursor->getNextTreeTop();
      if (node->isProfilingCode() 
         && node->getOpCodeValue() == TR::treetop 
         && node->getFirstChild()->getOpCode().isCall() 
         && (comp()->getSymRefTab()->isNonHelper(node->getFirstChild()->getSymbolReference(), TR::SymbolReferenceTable::jProfileValueSymbol) 
            || comp()->getSymRefTab()->isNonHelper(node->getFirstChild()->getSymbolReference(), TR::SymbolReferenceTable::jProfileValueWithNullCHKSymbol)))
         {
         TR::Node *value = node->getFirstChild()->getFirstChild();
         
         if ((alreadyProfiledValues->isSet(value->getGlobalIndex()) || value->getOpCode().isLoadConst()) &&
               performTransformation(comp(), "%s Removing profiling treetop, node n%dn is already profiled\n",
                  optDetailString(), value->getGlobalIndex()))
            /* Found that second and third child were having more than one ref counts. 
            TR_ASSERT_FATAL(node->getFirstChild()->getSecondChild()->getReferenceCount() == 1 &&
               node->getFirstChild()->getThirdChild()->getReferenceCount() == 1,
               "Second and Third Child of the value calls should be referenced only once");
            */
            {
            TR::TransformUtil::removeTree(comp(), cursor);
            }
         else
            {
            alreadyProfiledValues->set(value->getGlobalIndex());
            }
         
         }
      // Emptying a bit vector after scanning whole extended basic blocks will keep number of bits set in bit vector low.
      else if (node->getOpCodeValue() == TR::BBStart && !node->getBlock()->isExtensionOfPreviousBlock())
         {
         alreadyProfiledValues->empty();
         }
      else
         {
         performOnNode(node, cursor, alreadyProfiledValues, &checklist);
         }
      cursor = nextTT;
      }
   }

void
TR_JProfilingValue::performOnNode(TR::Node *node, TR::TreeTop *cursor, TR_BitVector *alreadyProfiledValues, TR::NodeChecklist *checklist)
   {
   if (checklist->contains(node))
      return;
   checklist->add(node);

   TR::TreeTop *preceedingTT = NULL;
   TR::Node *profiledNode = NULL;
   TR::Node *constNode = NULL;
   TR::SymbolReference *profiler = NULL;
   if (node->getOpCode().isCallIndirect() && !node->getByteCodeInfo().doNotProfile() 
      && (node->getSymbol()->getMethodSymbol()->isVirtual() 
         || node->getSymbol()->getMethodSymbol()->isInterface()))
      {
      profiledNode = node->getFirstChild();
      TR::Node *nextTTNode = cursor->getNextTreeTop() ? cursor->getNextTreeTop()->getNode() : NULL;
      /*
       * RecompilationModifier adds the profiling placeholder call for Virtual calls. 
       * Quickly check the next treetop, if it is profiling same virtual call target. In that case we do not need to add
       * Profiling Call as it is already there.
       */
      if (!(alreadyProfiledValues->isSet(profiledNode->getGlobalIndex()) 
         || (nextTTNode && nextTTNode->isProfilingCode() 
               && nextTTNode->getOpCodeValue() == TR::treetop 
               && nextTTNode->getFirstChild()->getOpCode().isCall() 
               && comp()->getSymRefTab()->isNonHelper(nextTTNode->getFirstChild()->getSymbolReference(), TR::SymbolReferenceTable::jProfileValueSymbol) 
               && nextTTNode->getFirstChild()->getFirstChild() == profiledNode)))
         {
         preceedingTT = cursor;
         profiler = comp()->getSymRefTab()->findOrCreateJProfileValuePlaceHolderSymbolRef();
         performTransformation(comp(), "%s Adding JProfiling PlaceHolder call to profile, virtual call node n%dn profiling n%dn\n",
            optDetailString(), node->getGlobalIndex(), profiledNode);
         }
      }
   else if (!node->getByteCodeInfo().doNotProfile() 
            && (node->getOpCodeValue() == TR::instanceof 
               || node->getOpCodeValue() == TR::checkcast 
               || node->getOpCodeValue() == TR::checkcastAndNULLCHK)
            && !alreadyProfiledValues->isSet(node->getFirstChild()->getGlobalIndex()))
      {
      preceedingTT = cursor->getPrevTreeTop();
      profiledNode = TR::Node::createWithSymRef(node, TR::aloadi, 1, node->getFirstChild(),
                                                getSymRefTab()->findOrCreateVftSymbolRef());
      profiler = comp()->getSymRefTab()->findOrCreateJProfileValuePlaceHolderWithNullCHKSymbolRef();
      performTransformation(comp(), "%s Adding JProfiling PlaceHolder call to profile, instanceof/checkcast at n%dn profiling vft load of n%dn\n",
         optDetailString(), node->getGlobalIndex(), node->getFirstChild());
      }
   // For Virtual call and class test nodes, we are actually profiling the VFT pointer of the object. So We need to set the Bit VE
   if (preceedingTT != NULL)
      {
      alreadyProfiledValues->set(node->getFirstChild()->getGlobalIndex());
      TR::Node *call = TR::Node::createWithSymRef(node, TR::call, 2, profiler);
      call->setAndIncChild(0, profiledNode);
      TR_ValueProfileInfo *valueProfileInfo = TR_PersistentProfileInfo::getCurrent(comp())->findOrCreateValueProfileInfo(comp());
      TR_AbstractHashTableProfilerInfo *info = static_cast<TR_AbstractHashTableProfilerInfo*>(valueProfileInfo->getOrCreateProfilerInfo(profiledNode->getByteCodeInfo(), comp(), AddressInfo, HashTableProfiler));
      call->setAndIncChild(1, TR::Node::aconst(node, (uintptr_t) info));
      TR::TreeTop *callTree = TR::TreeTop::create(comp(), preceedingTT, TR::Node::create(TR::treetop, 1, call));
      callTree->getNode()->setIsProfilingCode();
      }
   
   for (int i = 0; i < node->getNumChildren(); ++i)
      performOnNode(node->getChild(i), cursor, alreadyProfiledValues, checklist);
   }

void
TR_JProfilingValue::lowerCalls()
   {
   TR::TreeTop *cursor = comp()->getStartTree();
   TR_BitVector *backwardAnalyzedAddressNodesToCheck = new (comp()->trStackMemory()) TR_BitVector();
   while (cursor)
      {
      TR::Node * node = cursor->getNode();
      TR::TreeTop *nextTreeTop = cursor->getNextTreeTop();
      if (node->isProfilingCode() &&
         node->getOpCodeValue() == TR::treetop &&
         node->getFirstChild()->getOpCode().isCall() &&
         (comp()->getSymRefTab()->isNonHelper(node->getFirstChild()->getSymbolReference(), TR::SymbolReferenceTable::jProfileValueSymbol) ||
            comp()->getSymRefTab()->isNonHelper(node->getFirstChild()->getSymbolReference(), TR::SymbolReferenceTable::jProfileValueWithNullCHKSymbol)))
         {
         // Backward Analysis in the extended basic block to get list of address nodes 
         for (TR::TreeTop *iter = cursor->getPrevTreeTop();
            iter && (iter->getNode()->getOpCodeValue() != TR::BBStart || iter->getNode()->getBlock()->isExtensionOfPreviousBlock());
            iter = iter->getPrevTreeTop())
            {
            TR::Node *currentTreeTopNode = iter->getNode();
            if (currentTreeTopNode->getNumChildren() >= 1 && currentTreeTopNode->getFirstChild()->getType() == TR::Address)
               backwardAnalyzedAddressNodesToCheck->set(currentTreeTopNode->getFirstChild()->getGlobalIndex());
            }
         // Forward walk to check for compressedref anchors of any evaluated address nodes identified above
         for (TR::TreeTop *iter = cursor->getNextTreeTop();
            iter && (iter->getNode()->getOpCodeValue() != TR::BBStart || iter->getNode()->getBlock()->isExtensionOfPreviousBlock());
            iter = iter->getNextTreeTop())
            {
            TR::Node *currentTreeTopNode = iter->getNode();
            if (currentTreeTopNode->getOpCodeValue() == TR::compressedRefs
               && backwardAnalyzedAddressNodesToCheck->isSet(currentTreeTopNode->getFirstChild()->getGlobalIndex()))
               {
               dumpOptDetails(comp(), "%s Moving treetop node n%dn above the profiling call to avoid uncommoning\n",
                  optDetailString(), iter->getNode()->getGlobalIndex());
               iter->unlink(false);
               cursor->insertBefore(iter);
               }
            }

         backwardAnalyzedAddressNodesToCheck->empty();
         TR::Node *child = node->getFirstChild();
         dumpOptDetails(comp(), "%s Replacing profiling placeholder n%dn with value profiling trees\n",
            optDetailString(), child->getGlobalIndex());
         // Extract the arguments and add the profiling trees
         TR::Node *value = child->getFirstChild();
         TR_AbstractHashTableProfilerInfo *table = (TR_AbstractHashTableProfilerInfo*) child->getSecondChild()->getAddress();
         bool needNullTest =  comp()->getSymRefTab()->isNonHelper(child->getSymbolReference(), TR::SymbolReferenceTable::jProfileValueWithNullCHKSymbol);
         addProfilingTrees(comp(), cursor, value, table, needNullTest, true, trace());
         // Remove the original trees and continue from the tree after the profiling
         TR::TransformUtil::removeTree(comp(), cursor);
         if (trace())
            comp()->dumpMethodTrees("After Adding Profiling Trees");
         }
      cursor = nextTreeTop;
      }
   }

/*
 * Insert the trees and control flow to profile a node after an insertion point.
 * The original block will be split after the insertion point.
 *
 * An optional mapping, with a test is supported. An example use of
 * this is a vft lookup using an address that could be null. A null check is therefore
 * necessary..
 *
 * |---------------------------------------------------------------------------|
 * | originalBlock                                                             |
 * |---------------------------------------------------------------------------|
 * | ...                                                                       |
 * | call jProfileValueSymbol/jProfileValueWithNullCHKSymbol // insertionPoint |
 * |     value                                                                 |
 * |        object (In case of VFT Profiling - jProfileValueWithNullCHKSymbol) |
 * |     aconst <table address>                                                |
 * | ...                                                                       |
 * |---------------------------------------------------------------------------|
 * 
 * Becomes:
 *
 * |--------------------------------------|
 * | originalBlock                        |
 * |--------------------------------------|
 * | ...                                  |
 * |  insertionPoint                      |
 * |  uncommoning                         |
 * |  ificmpeq goto mainlineReturn        |
 * |     iload isQueuedForRecompilation   |
 * |     iconst -1                        |
 * |--------------------------------------|
 *          |
 *          |--------------------------------------------------------------------------|
 *          |                                                                          |
 *          v                                                                          |
 * |----------------------------------------------------|                              |
 * | addNullCheck (for jProfileValueWithNullCHKSymbol)  |                              |
 * |----------------------------------------------------|                              |
 * |  ifacmpeq goto mainlineReturn                      |                              |
 * |     object                                         |                              |
 * |     aconst NULL                                    |                              |
 * |----------------------------------------------------|                              |
 *          |                                                                          |
 *          |                                                                          |
 *          |--------------------------------------------------------------------------|
 *          |                                                                          |
 *          v                                                                          |
 * |-----------------------------------------------|                                   |
 * | quickTestBlock                                |                                   |
 * |-----------------------------------------------|                                   |
 * |  treetop (incIndexTreeTop)                    |                                   |
 * |     l/iselect                                 |                                   |
 * |        l/icmpeq (conditionNode)               |                                   |
 * |           value                               |                                   |
 * |           i/lloadi                            |                                   |
 * |              al/aiadd                         |                                   |
 * |                 addressOfKeys                 |                                   |
 * |                 l/imul                        |                                   |
 * |                    hashIndex                  |                                   |
 * |                    width                      |                                   |
 * |        => hashIndex                           |                                   |
 * |        otherIndex                             |                                   |
 * |  ificmpeq goto helper                         |                                   |
 * |     ior                                       |                                   |
 * |        => l/icmpeq (conditionNode)            |                                   |
 * |        scmpge (checkIfTableIsLockedNode)      |                                   |
 * |           => otherIndex                       |                                   |
 * |           sconst 0                            |                                   |
 * |-----------------------------------------------|                                   |
 *                         |                                                           |
 *                         |--------------------------------|                          |
 *                         |                                |                          |
 *                         v                                |                          | 
 *          |------------------------------|                |                          |
 *          | quickInc                     |                |                          |
 *          |------------------------------|                |                          |
 *          | istorei                      |                |                          |
 *          |    al/aiadd                  |                v                          |
 *          |       aconst <table address> |        |-------------------------------|  |
 *          |       l/imul                 |        | helper                        |  |
 *          |           l/iselect          |        |-------------------------------|  |
 *          |          width               |        | call TR_jProfile32/64BitValue |  |
 *          |     iadd                     |        |    value                      |  |
 *          |        iloadi                |        |    table address              |  |
 *          |           => al/aiadd        |        |-------------------------------|  |
 *          |        iconst 1              |                         |                 |
 *          |------------------------------|                         |                 |
 *                         |                                         |                 |
 *                         |<----------------------------------------|-----------------|
 *                         v
 *          |------------------------------|
 *          | mainlineReturn               |
 *          |------------------------------|
 *          |  ...                         |
 *          |------------------------------|
 *          
 *
 * \param insertionPoint Treetop to insert profiling code after.
 * \param value Value to profile.
 * \param table Persistent TR_HashMapInfo which will be filled and incremented during profiling.
 * \param optionalTest Option test node capable of preventing evaluation of value and using a fallbackValue instead.
 * \param extendBlocks Generates the blocks as extended, defaults true.
 * \param trace Enable tracing.
 */
bool
TR_JProfilingValue::addProfilingTrees(
   TR::Compilation *comp,
   TR::TreeTop *insertionPoint,
   TR::Node *value,
   TR_AbstractHashTableProfilerInfo *table,
   bool addNullCheck,
   bool extendBlocks,
   bool trace)
   {
   // Common types used in calculation
   TR::DataType counterType = TR::Int32;
   TR::DataType lockType    = TR::Int16;
   TR::DataType systemType  = comp->target().is64Bit() ? TR::Int64 : TR::Int32;

   // Type to use in calculations and table access
   TR::DataType roundedType    = value->getType();
   if (roundedType == TR::Int8 || roundedType == TR::Int16)
      roundedType = TR::Int32;
   if (roundedType == TR::Address)
      roundedType = systemType;

   /********************* original Block *********************/
   TR::Block *originalBlock = insertionPoint->getEnclosingBlock();
   TR::CFG *cfg = comp->getFlowGraph();
   cfg->setStructure(0);
   /*
    * Anchor the value node after the split point to make sure that it will be either stored in the register or temp slot before split point
    * Most of the time, a last simplifier run will clean it up but in case it does not, we end up with the unguarded null dereference in case we are
    * profiling VFT load from object. To avoid that we do not anchor the node when null test is needed.
    * If the profiling place holder is generated in this pass, we make sure that we generate the NULL Test before actual type test so in that case,
    * it will be uncommoned by the block splitter. If VFT Profiling is added in other passes, we will store the value in to temp slot for helper call
    * before we jump to helper call block.
    */
   if (!addNullCheck)
      insertionPoint->insertAfter(TR::TreeTop::create(comp, TR::Node::create(TR::treetop, 1, value)));

   /********************* mainline Return Block *********************/
   TR::Block *mainlineReturn = originalBlock->splitPostGRA(insertionPoint->getNextTreeTop(), cfg, true, NULL);
   
   TR::Node *origBlockGlRegDeps = originalBlock->getExit()->getNode()->getNumChildren() == 1 ? originalBlock->getExit()->getNode()->getFirstChild() : NULL;
   if (trace)
      traceMsg(comp, "\t\t\tUsing split mainline return block = %d\n", mainlineReturn->getNumber());
   TR::Node *profilingValue = value;
   /*
    * Look if profiling value is stored into a register (In case of post GRA split) or temp slot by splitter while uncommoning.
    * If value being profiled is referenced after the main split point, we should have stored it in either register or temp slot via splitPostGRA.
    * Change the profiling node with the node that stores the value before adding any new trees. This will be used for the helper call.
    */
   for (TR::TreeTop *iterTT = insertionPoint->getNextTreeTop(); iterTT->getNode()->getOpCodeValue() != TR::BBEnd; iterTT = iterTT->getNextTreeTop())
      {
      // Looking to find a node whose child if profiled value.
      TR::Node *ttNode = iterTT->getNode();
      if (ttNode->getOpCode().isStoreDirectOrReg() && ttNode->getFirstChild() == value)
         {
         profilingValue = ttNode;
         break;
         }
      }
   if (trace)
      traceMsg(comp, "\t\t\tProfiling value n%dn\n", profilingValue->getGlobalIndex());

   TR::Block *iter = originalBlock;
   TR::TreeTop *lastBranchToMainlineReturnTT = NULL;

   static bool disableJProfilingRecompQueueTest = (feGetEnv("TR_DontGenerateJProfilingRecompQueueTestJProfilingValue") != NULL);

   /* Adding a test to see if still need to profile value or can skip profiling */
   if (comp->isProfilingCompilation() && !disableJProfilingRecompQueueTest)
      {
      TR_PersistentProfileInfo *profileInfo = comp->getRecompilationInfo()->findOrCreateProfileInfo();
      TR_BlockFrequencyInfo *bfi = TR_BlockFrequencyInfo::get(profileInfo);
      TR::Node *loadIsQueuedForRecompilation = TR::Node::createWithSymRef(value, TR::iload, 0, comp->getSymRefTab()->createKnownStaticDataSymbolRef(bfi->getIsQueuedForRecompilation(), TR::Int32));
      TR::Node *checkIfQueueForRecompilation = TR::Node::createif(TR::ificmpeq, loadIsQueuedForRecompilation, TR::Node::iconst(value, -1), mainlineReturn->getEntry());
      TR::TreeTop *checkIfNeedToProfileValue = TR::TreeTop::create(comp, checkIfQueueForRecompilation);
      iter->append(checkIfNeedToProfileValue);
      if (origBlockGlRegDeps != NULL)
         {
         TR::Node *exitGlRegDeps = copyGlRegDeps(comp, origBlockGlRegDeps);
         checkIfQueueForRecompilation->addChildren(&exitGlRegDeps, 1); 
         }
      lastBranchToMainlineReturnTT = checkIfNeedToProfileValue;
      if (trace)
         traceMsg(comp, "\t\t\tCheck if queued for recompilation test performed in block_%d\n", iter->getNumber());
      }

   /* In case profiling vft , need to do null test for the object. */
   if (addNullCheck)
      {
      /*
       * Objects under instanceOf and checkCast needs NULL Check and we look for this candidate while we walk trees in JProfilingValue.
       * TODO: As we look for VFT Candidates for type test in this pass only, we have an object available to do the null test. 
       * In case some one else decides to add VFT profiling around type test in other pass, it should also add the null test to guard
       * the profiling, as due to other optimizations it might be possible that VFT load is uncommoned and converted to load from the temp slot or register.
       */
      TR_ASSERT_FATAL(value->getNumChildren() == 1, "JProfilingValue : NULL check can only be done when object node is available.");
      TR::Node *nullTest = TR::Node::createif(TR::ifacmpeq, value->getFirstChild(), TR::Node::aconst(value, 0), mainlineReturn->getEntry());
      TR::TreeTop *nullTestTree = TR::TreeTop::create(comp, nullTest);
      iter->append(nullTestTree);
      if (lastBranchToMainlineReturnTT != NULL)
         {
         TR::Block *temp = iter->split(nullTestTree, cfg, false, true);
         temp->setIsExtensionOfPreviousBlock();
         cfg->addEdge(iter, mainlineReturn);
         iter = temp;
         }
      if (origBlockGlRegDeps != NULL)
         {
         TR::Node *exitGlRegDeps = copyGlRegDeps(comp,origBlockGlRegDeps);
         nullTest->addChildren(&exitGlRegDeps, 1); 
         }
      lastBranchToMainlineReturnTT = nullTestTree;
      if (trace)
         traceMsg(comp, "\t\t\tNull test performed in in block_%d\n", iter->getNumber());
      }
   
   /********************* quickTest Block *********************/
   TR::Node *quickTestValue = convertType(value, roundedType);
   TR::Node *address = TR::Node::aconst(value, table->getBaseAddress());
   TR::Node *hashIndex = computeHash(comp, table, quickTestValue, address);

   TR::Node *lockOffsetAddress = TR::Node::aconst(value, table->getBaseAddress() + table->getLockOffset());
   TR::Node *lock = loadValue(comp, lockType, lockOffsetAddress);
   TR::Node *otherIndex = convertType(lock, roundedType, false);
   TR::Node *convertedHashIndex = convertType(hashIndex, systemType, true);
   TR::Node *addressOfKeys = TR::Node::aconst(value, table->getBaseAddress() + table->getKeysOffset());
   TR::Node *conditionNode = TR::Node::create(value, comp->il.opCodeForCompareEquals(roundedType), 2, quickTestValue,
      loadValue(comp, roundedType, addressOfKeys, convertedHashIndex));
   
   TR::Node *selectNode = TR::Node::create(comp->il.opCodeForSelect(roundedType), 3, conditionNode, hashIndex, otherIndex);
   TR::TreeTop *incIndexTreeTop = TR::TreeTop::create(comp, TR::Node::create(TR::treetop, 1, selectNode));
   iter->append(incIndexTreeTop);

   TR::Block *quickTestBlock = iter->split(incIndexTreeTop, cfg, false, true);
   quickTestBlock->setIsExtensionOfPreviousBlock();
   if (lastBranchToMainlineReturnTT != NULL)
      {
      cfg->addEdge(iter, mainlineReturn);
      }
   if (trace)
      traceMsg(comp, "\t\t\tQuick Test to check if value is already being profiled is in block_%d\n", quickTestBlock->getNumber());
   
   TR::Node *checkIfTableIsLockedNode = TR::Node::create(value, comp->il.opCodeForCompareGreaterOrEquals(lockType), 2, lock, TR::Node::sconst(value, 0));
   TR::Node *checkNode = TR::Node::createif(TR::ificmpeq,
      TR::Node::create(value, TR::ior, 2, conditionNode, checkIfTableIsLockedNode),
      TR::Node::iconst(0));
   if (origBlockGlRegDeps != NULL)
      {
      TR::Node *exitGlRegDeps = copyGlRegDeps(comp, origBlockGlRegDeps);
      checkNode->addChildren(&exitGlRegDeps, 1); 
      } 
   TR::TreeTop *checkNodeTreeTop = TR::TreeTop::create(comp, incIndexTreeTop, checkNode);

   /********************* quickInc index Block *********************/
   TR::Node *counterOffset = TR::Node::iconst(value, table->getFreqOffset());
   TR::Node *counterBaseAddress = TR::Node::aconst(value, table->getBaseAddress() + table->getFreqOffset());
   TR::TreeTop *incTree = TR::TreeTop::create(comp, checkNodeTreeTop,
      incrementMemory(comp, counterType, effectiveAddress(counterType, counterBaseAddress, convertType(selectNode, systemType, true))));
   TR::Block *quickInc = quickTestBlock->split(incTree, cfg, false, true);
   quickInc->setIsExtensionOfPreviousBlock();
   if (trace)
      traceMsg(comp, "\t\t\tQuick increment performed in block_%d\n", quickInc->getNumber());   


   /********************* helper call block *********************/
   TR::TreeTop *lastTreeTop = comp->getMethodSymbol()->getLastTreeTop();
   TR::Block *helper = TR::Block::createEmptyBlock(comp, MAX_COLD_BLOCK_COUNT + 1);
   helper->setIsCold();
   lastTreeTop->join(helper->getEntry());
   lastTreeTop = helper->getExit();
   cfg->addNode(helper);
   cfg->addEdge(quickTestBlock, helper);
   cfg->addEdge(helper, mainlineReturn);
   checkNode->setBranchDestination(helper->getEntry());
   
   TR::Node *glRegDepsOfMainlineEntry = mainlineReturn->getEntry()->getNode()->getNumChildren() == 1 ? mainlineReturn->getEntry()->getNode()->getFirstChild() : NULL;
   TR::Node *valueChildOfHelperCall = NULL;
   TR::Node *goToMainlineNode = TR::Node::create(value, TR::Goto, 0, mainlineReturn->getEntry());
   TR::TreeTop::create(comp, helper->getEntry(), goToMainlineNode);
   
   if (glRegDepsOfMainlineEntry != NULL)
      {
      // Copying GlRegDeps of the Mainline Entry block to the helper block.
      TR::Node *duplicateGlRegDeps = glRegDepsOfMainlineEntry->duplicateTree();
      helper->getEntry()->getNode()->setNumChildren(1);
      helper->getEntry()->getNode()->setAndIncChild(0, duplicateGlRegDeps);
      TR::Node *origDuplicateGlRegDeps = duplicateGlRegDeps;
      duplicateGlRegDeps = TR::Node::copy(duplicateGlRegDeps);
      
      for (int32_t i = origDuplicateGlRegDeps->getNumChildren() - 1; i >= 0; --i)
         {
         TR::Node * dep = origDuplicateGlRegDeps->getChild(i);
         duplicateGlRegDeps->setAndIncChild(i, dep);
         }
      duplicateGlRegDeps->setReferenceCount(0);
      goToMainlineNode->addChildren(&duplicateGlRegDeps, 1);
      
      if (profilingValue->getOpCode().isStoreReg() || profilingValue->getOpCode().isLoadReg())
         {
         // Looking for the regLoad in the entry
         TR::Node *helperCallGlRegDeps = helper->getEntry()->getNode()->getFirstChild(); 
         if (profilingValue->requiresRegisterPair(comp))
            {
            for (int i=0; i < helperCallGlRegDeps->getNumChildren(); i++)
               {
               TR::Node *tempNode = helperCallGlRegDeps->getChild(i);
               if (profilingValue->getLowGlobalRegisterNumber() == tempNode->getLowGlobalRegisterNumber() && profilingValue->getHighGlobalRegisterNumber() == tempNode->getHighGlobalRegisterNumber())
                  {
                  valueChildOfHelperCall = tempNode;
                  break;
                  }
               }
            }
         else
            {
            for (int i=0; i < helperCallGlRegDeps->getNumChildren(); i++)
               {
               TR::Node *tempNode = helperCallGlRegDeps->getChild(i);
               if (profilingValue->getGlobalRegisterNumber() == tempNode->getGlobalRegisterNumber())
                  {
                  valueChildOfHelperCall = tempNode;
                  break;
                  }
               }
            }
         if (valueChildOfHelperCall == NULL)
            {
            TR_ASSERT_FATAL(profilingValue->getOpCode().isLoadReg(), "In case profiled value is in register and we can not find it in GlRegDeps of mainlineReturn, it must be loadReg");
            valueChildOfHelperCall = TR::Node::copy(profilingValue);
            valueChildOfHelperCall->setReferenceCount(0);
            helperCallGlRegDeps->addChildren(&valueChildOfHelperCall, 1);
            goToMainlineNode->getFirstChild()->addChildren(&valueChildOfHelperCall, 1);
            }
         }
      }
   // In case we can not find value in register or temp slot, store the value to temp slot before going to helper block. 
   if (valueChildOfHelperCall == NULL)
      {
      TR::SymbolReference *storedValueSymRef = NULL; 
      if (profilingValue->getOpCode().isStoreDirect() || (profilingValue->getOpCode().isLoadDirect() && !profilingValue->getOpCode().isLoadConst()))
         {
         storedValueSymRef = profilingValue->getSymbolReference();
         }
      else
         {
         traceMsg(comp, "\t\t\tNode n%dn needs to be stored on temp slot\n", profilingValue->getGlobalIndex());
         // profiledValue is normal Node which is not referenced further. Store value to temp slot at the beginning of quick test
         TR::TreeTop *storeValue = TR::TreeTop::create(comp, quickTestBlock->getEntry(), storeNode(comp,  profilingValue, storedValueSymRef)); 
         }
      valueChildOfHelperCall = TR::Node::createLoad(value, storedValueSymRef);
      }

   // Add the call to the helper and return to the mainline
   TR::TreeTop *helperCallTreeTop = TR::TreeTop::create(comp, helper->getEntry(), createHelperCall(comp,
      valueChildOfHelperCall,
      TR::Node::aconst(value, table->getBaseAddress()))); 
   if (trace)
      traceMsg(comp, "\t\t\tHelper call in block_%d\n", helper->getNumber()); 
   TR::NodeChecklist checklist(comp);
   checklist.add(value);
   TR::TreeTop *tt = quickTestBlock->getEntry(), *end = mainlineReturn->getEntry(); 
   while (tt && tt != end)
      {
      TR::Node *node = tt->getNode();
      if (node->getOpCodeValue() != TR::BBStart && node->getOpCodeValue() != TR::BBEnd)
         setProfilingCode(node, checklist);
      tt = tt->getNextTreeTop();
      }
   tt = helper->getEntry();
   while (tt)
      {
      TR::Node *node = tt->getNode();
      if (node->getOpCodeValue() != TR::BBStart && node->getOpCodeValue() != TR::BBEnd)
         setProfilingCode(node, checklist);
      tt = tt->getNextTreeTop();
      }
   return true;
   }

/**
 * Utility function to Copy the GlRegDeps 
 * 
 * @param comp Compilation Object
 * @param origGlRegDeps Original GlRegDeps to bbe copied
 * @return TR::Node* New GlRegDeps copied from origGlRegDeps
 */
TR::Node *
TR_JProfilingValue::copyGlRegDeps(TR::Compilation *comp, TR::Node *origGlRegDeps)
   {
   TR::Node *copiedGlRegDeps = TR::Node::create(origGlRegDeps, TR::GlRegDeps, origGlRegDeps->getNumChildren());
   for (int32_t i = 0; i < origGlRegDeps->getNumChildren(); i++)
      {
      TR::Node* child = origGlRegDeps->getChild(i);
      if (child->getOpCodeValue() == TR::PassThrough)
         {
         TR::Node *origPassThrough = child;
         child = TR::Node::create(origPassThrough, TR::PassThrough, 1, origPassThrough->getFirstChild());
         child->setLowGlobalRegisterNumber(origPassThrough->getLowGlobalRegisterNumber());
         child->setHighGlobalRegisterNumber(origPassThrough->getHighGlobalRegisterNumber());
         }
      copiedGlRegDeps->setAndIncChild(i, child);
      }
   return copiedGlRegDeps;
   }

/*
 * Mark a node and its children as profiling code.
 *
 * \param node Node to process, marking it and its children.
 * \param checklist Checklist of nodes already marked.
 */
void
TR_JProfilingValue::setProfilingCode(TR::Node *node, TR::NodeChecklist &checklist)
   {
   if (checklist.contains(node))
      return;
   checklist.add(node);

   node->setIsProfilingCode();
   for (uint32_t i = 0; i < node->getNumChildren(); ++i)
      setProfilingCode(node->getChild(i), checklist);
   }

/*
 * Generate the tree to store a value into a temporary symref. If no symref is specified, one will be generated.
 *
 * \param value Value to store.
 * \param symRef Optional symref to store value into. Will be update to used symref if none specified.
 */
TR::Node *
TR_JProfilingValue::storeNode(TR::Compilation *comp, TR::Node *value, TR::SymbolReference* &symRef)
   {
   if (symRef == NULL)
      symRef = comp->getSymRefTab()->createTemporary(comp->getMethodSymbol(), value->getDataType());

   if (value->getType() == TR::Address && value->getOpCode().hasSymbolReference() && !value->getSymbol()->isCollectedReference())
      symRef->getSymbol()->setNotCollected();

   return TR::Node::createWithSymRef(value, directStore(value->getDataType()), 1, value, symRef);
   }

/*
 * Given an address in X86's LEA form, generate a node structure to calculate a memory address for a desired
 * data type. Address would be: base + width(dataType) * index + offset
 *
 * \param dataType Date type of value to address. Its width is used for calculations if index is specified.
 * \param base Base address to load from.
 * \param index Optional index.
 * \param offset Optional offset from the base.
 */
TR::Node *
TR_JProfilingValue::effectiveAddress(TR::DataType dataType, TR::Node *base, TR::Node *index, TR::Node *offset)
   {
   if (offset)
      {
      if (offset->getDataType() == TR::Int64)
         base = TR::Node::create(base, TR::aladd, 2, base, offset);
      else if (offset->getDataType() == TR::Int32)
         base = TR::Node::create(base, TR::aiadd, 2, base, offset);
      else
         TR_ASSERT_FATAL(0, "Invalid type for address calculation integer");
      }

   if (index)
      {
      uint8_t size = TR::DataType::getSize(dataType);
      if (index->getDataType() == TR::Int64)
         base = TR::Node::create(base, TR::aladd, 2, base, TR::Node::create(base, TR::lmul, 2, index, TR::Node::lconst(base, size)));
      else if (index->getDataType() == TR::Int32)
         base = TR::Node::create(base, TR::aiadd, 2, base, TR::Node::create(base, TR::imul, 2, index, TR::Node::iconst(base, size)));
      else
         TR_ASSERT_FATAL(0, "Invalid type for address calculation integer");
      }

   return base;
   }

/*
 * Given an address in X86's LEA form, generate a node structure to load a value from the memory address of the desired
 * data type. Address would be: base + width(dataType) * index + offset
 *
 * \param dataType Date type of value to load. Its width is used for calculations if index is specified.
 * \param base Base address to load from.
 * \param index Optional index.
 * \param offset Optional offset from the base.
 */
TR::Node *
TR_JProfilingValue::loadValue(TR::Compilation *comp, TR::DataType dataType, TR::Node *base, TR::Node *index, TR::Node *offset)
   {
   base = effectiveAddress(dataType, base, index, offset);
   TR::SymbolReference *symRef = comp->getSymRefTab()->findOrCreateArrayShadowSymbolRef(dataType, NULL);
   TR::Node *result = TR::Node::createWithSymRef(base, indirectLoad(dataType), 1, base, symRef);

   return result;
   }

/*
 * Generate the helper call tree, which adds values to the table along the slowest path.
 * Will return a treetop, with the call as its child.
 *
 * \param value Node representing the value to add to the table.
 * \param table Address of the table's base.
 */
TR::Node *
TR_JProfilingValue::createHelperCall(TR::Compilation *comp, TR::Node *value, TR::Node *table)
   {
   TR::SymbolReference *profiler;
   TR::DataType valueType = value->getDataType();
   // If Profiling 1/2/4 Bytes values, we need to call TR_jProfile32BitValue
   // Instead of checking the type, checking size (in bytes) which should cover address type on 32 Bit platforms as well
   if (value->getSize() <=4)
      {
      if (value->getDataType() != TR::Address)
         value = convertType(value, TR::Int32);
      profiler = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_jProfile32BitValue, false, false, false);
      }
   else
      {
      profiler = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_jProfile64BitValue, false, false, false);
      }

#if defined(TR_HOST_POWER) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64) || defined(TR_HOST_S390)
   profiler->getSymbol()->castToMethodSymbol()->setLinkage(TR_System);
#elif defined(TR_HOST_X86)
   profiler->getSymbol()->castToMethodSymbol()->setSystemLinkageDispatch();
#endif

   TR::Node *helperCall = TR::Node::createWithSymRef(value, TR::call, 2, profiler);
   helperCall->setAndIncChild(0, value);
   helperCall->setAndIncChild(1, table);
   return TR::Node::create(TR::treetop, 1, helperCall);
   }

/*
 * Increment a memory address by 1. Used to increment the table's counters on both the match and other
 * paths.
 *
 * \param counterType The memory addresses type, usually Int32 or Int64.
 * \param address The memory address to increment.
 */
TR::Node *
TR_JProfilingValue::incrementMemory(TR::Compilation *comp, TR::DataType counterType, TR::Node *address)
   {
   TR::SymbolReference *symRef = comp->getSymRefTab()->findOrCreateArrayShadowSymbolRef(counterType, NULL);

   TR::Node *load = TR::Node::createWithSymRef(address, indirectLoad(counterType), 1, address, symRef);
   TR::Node *incCounter = TR::Node::create(address, counterType == TR::Int32 ? TR::iadd : TR::ladd, 2, load,
      TR::Node::create(address, loadConst(counterType), 0, 1));
   TR::Node *storeCounter = TR::Node::createWithSymRef(address, indirectStore(counterType), 2,
      address, symRef);
   storeCounter->setAndIncChild(1, incCounter);

   return storeCounter;
   }

/*
 * Generate a constant node matching the system's address width. Used for address calculations.
 *
 * \param example Example node to copy BCI.
 * \param value Value to store in const.
 */
TR::Node *
TR_JProfilingValue::systemConst(TR::Node *example, uint64_t value)
   {
   TR::ILOpCodes constOp = TR::comp()->target().is64Bit() ? TR::lconst : TR::iconst;
   return TR::Node::create(example, constOp, 0, value);
   }

/*
 * Its sometimes necessary to convert values to their rounded integer representation, in Int32
 * or Int64, or to convert values to the system type, again in Int32 or Int64. This method
 * handles those conversions as necessary. All conversions are unsigned.
 *
 * \param index The value to convert.
 * \param dataType The datatype to convert to.
 */
TR::Node *
TR_JProfilingValue::convertType(TR::Node *index, TR::DataType dataType, bool zeroExtend)
   {
   if (index->getDataType() == dataType)
      return index;

   return TR::Node::create(index, TR::ILOpCode::getProperConversion(index->getDataType(), dataType, zeroExtend), 1, index);
   }

/**
 * Generate the hash calculation in nodes.
 * Supports generating calculations based on a series of shifts or a series of bit indices.
 *
 * \param comp The current compilation.
 * \param table The table to use for instrumentation.
 * \param value The value to hash.
 * \param baseAddr Optional base address of the table, if already known.
 */
TR::Node *
TR_JProfilingValue::computeHash(TR::Compilation *comp, TR_AbstractHashTableProfilerInfo *table, TR::Node *value, TR::Node *baseAddr)
   {
   TR_ASSERT(table->getDataType() == TR::Int32 || table->getDataType() == TR::Int64, "HashTable only expected to hold 32bit and 64bit values");

   if (!baseAddr)
      baseAddr = TR::Node::aconst(value, (uintptr_t) table);

   TR::ILOpCodes addSys   = comp->target().is64Bit() ? TR::aladd : TR::aiadd;
   TR::ILOpCodes constSys = comp->target().is64Bit() ? TR::lconst : TR::iconst;

   TR::Node *hash = NULL;
   if (table->getHashType() == BitIndexHash)
      {
      // Build the bit permute tree
      TR::Node *hashAddr = TR::Node::create(value, addSys, 2, baseAddr, TR::Node::create(value, constSys, 0, table->getHashOffset()));
      hash = TR::Node::create(value, value->getDataType() == TR::Int32 ? TR::ibitpermute : TR::lbitpermute, 3);
      hash->setAndIncChild(0, value);
      hash->setAndIncChild(1, hashAddr);
      hash->setAndIncChild(2, TR::Node::iconst(value, table->getBits()));
      }
   else if (table->getHashType() == BitShiftHash)
      {
      // Common operations, based on the value's width
      TR::ILOpCodes shiftOp = TR::lushr;
      TR::ILOpCodes andOp   = TR::land;
      TR::ILOpCodes orOp    = TR::lor;
      TR::ILOpCodes constOp = TR::lconst;
      if (table->getDataType() == TR::Int32)
         {
         shiftOp = TR::iushr;
         andOp   = TR::iand;
         orOp    = TR::ior;
         constOp = TR::iconst;
         }
   
      TR::SymbolReference *symRef = comp->getSymRefTab()->findOrCreateArrayShadowSymbolRef(TR::Int8, NULL);

      // Extract each bit and merge into final result
      for (uint32_t bit = 0; bit < table->getBits(); ++bit)
         {
         uint32_t shiftOffset = table->getHashOffset() + bit * sizeof(uint8_t);
         TR::Node *shiftAddress = TR::Node::create(value, addSys, 2, baseAddr, TR::Node::create(value, constSys, 0, shiftOffset));
         TR::Node *shift = TR::Node::createWithSymRef(value, TR::bloadi, 1, shiftAddress, symRef);
         TR::Node *bitShift = TR::Node::create(value, shiftOp, 2, value, convertType(shift, TR::Int32));
         TR::Node *bitExtract = TR::Node::create(value, andOp, 2, bitShift, TR::Node::create(value, constOp, 0, 1 << bit));
         if (hash)
            hash = TR::Node::create(value, orOp, 2, hash, bitExtract);
         else
            hash = bitExtract;
         }
      }
   else
      TR_ASSERT(0, "Unsupported hash type");

   return hash;
   }

const char *
TR_JProfilingValue::optDetailString() const throw()
   {
   return "O^O JPROFILING VALUE: ";
   }
