/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
#include "infra/Checklist.hpp"             // for TR::NodeChecklist
#include "ras/DebugCounter.hpp"
#include "runtime/J9Profiler.hpp"
#include "control/Recompilation.hpp"              // for TR_Recompilation, etc
#include "control/RecompilationInfo.hpp"              // for TR_Recompilation, etc
#include "codegen/CodeGenerator.hpp"
#include "optimizer/TransformUtil.hpp"            // for TransformUtil

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

   // Lower all existing calls
   lowerCalls();

   if (comp()->isProfilingCompilation())
      {
      TR::Recompilation *recomp = comp()->getRecompilationInfo();
      TR_ValueProfiler *profiler = recomp->getValueProfiler();
      TR_ASSERT(profiler, "Recompilation should have a ValueProfiler in a profiling compilation");
      profiler->setPostLowering();
      }
   else
      {
      TR::NodeChecklist checklist(comp());

      // Identify and instrument profiling candidates
      for (TR::TreeTop *cursor = comp()->getStartTree(); cursor; cursor = cursor->getNextTreeTop())
         performOnNode(cursor->getNode(), cursor, &checklist);
      }

   return 1;
   }

void
TR_JProfilingValue::performOnNode(TR::Node *node, TR::TreeTop *tt, TR::NodeChecklist *checklist)
   {
   if (checklist->contains(node))
      return;
   checklist->add(node);

   if (node->getOpCode().isCall() && !node->getByteCodeInfo().doNotProfile() &&
       (node->getSymbol()->getMethodSymbol()->isVirtual() ||
        node->getSymbol()->getMethodSymbol()->isInterface()))
      addProfiling(node->getFirstChild(), tt);
   else if (!node->getByteCodeInfo().doNotProfile() && 
       (node->getOpCodeValue() == TR::instanceof ||
        node->getOpCodeValue() == TR::checkcast))
      addVFTProfiling(node->getFirstChild(), tt->getPrevTreeTop(), true);

   for (int i = 0; i < node->getNumChildren(); ++i)
      performOnNode(node->getChild(i), tt, checklist);
   }

void
TR_JProfilingValue::addProfiling(TR::Node *value, TR::TreeTop *tt)
   {
   if (!performTransformation(comp(), "%s Add trees to track the value of node %p near tree %p\n", optDetailString(), value, tt->getNode()))
      return;

   TR_ValueProfileInfo *valueProfileInfo = TR_PersistentProfileInfo::getCurrent(comp())->findOrCreateValueProfileInfo(comp());
   TR_AbstractHashTableProfilerInfo *info = static_cast<TR_AbstractHashTableProfilerInfo*>(valueProfileInfo->getOrCreateProfilerInfo(value->getByteCodeInfo(), comp(), AddressInfo, HashTableProfiler));
   addProfilingTrees(comp(), tt, value, info, NULL, NULL, true, trace());
   }

void
TR_JProfilingValue::addVFTProfiling(TR::Node *address, TR::TreeTop *tt, bool addNullCheck)
   {
   if (!performTransformation(comp(), "%s Add trees to track the vft lookup of node %p near tree %p, null check %d\n", optDetailString(), address, tt->getNode(), addNullCheck))
      return;

   TR::Node *vftNode = TR::Node::createWithSymRef(address, TR::aloadi, 1, address,
      getSymRefTab()->findOrCreateVftSymbolRef());

   TR::Node *check = NULL;
   TR::Node *fallback = NULL;
   if (addNullCheck)
      {
      check = TR::Node::createif(TR::ifacmpeq, address, TR::Node::aconst(address, 0));
      fallback = TR::Node::aconst(address, 0);
      }

   TR_ValueProfileInfo *valueProfileInfo = TR_PersistentProfileInfo::getCurrent(comp())->findOrCreateValueProfileInfo(comp());
   TR_AbstractHashTableProfilerInfo *info = static_cast<TR_AbstractHashTableProfilerInfo*>(valueProfileInfo->getOrCreateProfilerInfo(address->getByteCodeInfo(), comp(), AddressInfo, HashTableProfiler));
   addProfilingTrees(comp(), tt, vftNode, info, check, fallback, true, trace());
   }

/**
 * Identify helper calls to TR_jProfile32BitValue and TR_jProfile64BitValue, lowering them
 * into the fast, slow and helper paths.
 */
void
TR_JProfilingValue::lowerCalls()
   {
   TR::TreeTop *cursor = comp()->getStartTree();
   TR_BitVector *backwardAnalyzedAddressNodesToCheck = new (comp()->trStackMemory()) TR_BitVector();
   while (cursor)
      {
      TR::Node * node = cursor->getNode();
      if (node->isProfilingCode() &&
          node->getOpCodeValue() == TR::treetop &&
          node->getFirstChild()->getOpCode().isCall() &&
          (node->getFirstChild()->getSymbolReference()->getReferenceNumber() == TR_jProfile32BitValue ||
           node->getFirstChild()->getSymbolReference()->getReferenceNumber() == TR_jProfile64BitValue) &&
          node->getFirstChild()->getNumChildren() == 3)
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
         addProfilingTrees(comp(), cursor, value, table, NULL, NULL, true, trace());

         // Remove the original trees and continue from the tree after the profiling
         TR::TransformUtil::removeTree(comp(), cursor);
         }
      cursor = cursor->getNextTreeTop();
      }
   }

/*
 * Insert the trees and control flow to profile a node after an insertion point.
 * The original block will be split after the insertion point.
 *
 * An optional mapping, with a test and fallback value is supported. An example use of
 * this is a vft lookup using an address that could be null. A null check is therefore
 * necessary, using a fallback value of 0.
 *
 * ------------------
 * | ...            |
 * | insertionPoint |
 * | ...            |
 * ------------------
 * 
 * Becomes:
 *
 * | ...               |                                        
 * | insertionPoint    |                                        
 * | uncommoning       |
 * | optionalTest      |---------------
 * ---------------------              |
 *          |                         |
 *          v                         v
 * ---------------------    ---------------------                                       
 * | originalValue     |    | fallbackValue     |                                      
 * ---------------------    ---------------------                                      
 * | store temp 1      |    | store temp 1      |                                      
 * |  value            |    |  fallback         |                                      
 * ---------------------    ---------------------
 *          |                        |
 *          |-------------------------
 *          v
 * ---------------------                                        
 * | quickTest         |                                        
 * ---------------------                                        
 * | store temp 2      |                                        
 * |  hash             |                                        
 * |   load temp 1     |                                        
 * | ifne              |----------------------
 * |  =>load temp 1    |                     |
 * |  indirect load    |                     v
 * |   add             |           ---------------------
 * |    keysArray      |           | slowTest          |
 * |    mult           |           ---------------------
 * |     =>hash        |           | store temp 3      |
 * |     width         |           |   indirect load   |
 * ---------------------           |     otherIndex    |
 *          |                      | ifeq              |-------------------
 *          v                      |   load temp 3     |                  |
 * ---------------------           |   const 0         |                  v
 * | quickInc          |           ---------------------        ------------------
 * ---------------------                    |                   | helper         |
 * | incMemory         |                    v                   ------------------
 * |   add             |           ---------------------        | call helper    |
 * |     countsArray   |           | slowInc           |        |   load temp 1  |
 * |     mult          |           ---------------------        |   tableAddress |
 * |       load temp 2 |           | incMemory         |        ------------------
 * |       countWidth  |           |   add             |                 |
 * ---------------------           |     countsArray   |                 |
 *          |                      |     mult          |                 |
 *          |                      |       load temp 3 |                 |
 *          |                      |       countWidth  |                 |
 *          |                      ---------------------                 |
 *          |                                |                           |
 *          |-------------------------------------------------------------
 *          v
 * ---------------------
 * | uncommoning       |
 * | ...               |
 *
 * \param insertionPoint Treetop to insert profiling code after.
 * \param value Value to profile.
 * \param table Persistent TR_HashMapInfo which will be filled and incremented during profiling.
 * \param optionalTest Option test node capable of preventing evaluation of value and using a fallbackValue instead.
 * \param fallbackValue Fallback value to use with the optional test.
 * \param extendBlocks Generates the blocks as extended, defaults true.
 * \param trace Enable tracing.
 */
bool
TR_JProfilingValue::addProfilingTrees(
    TR::Compilation *comp,
    TR::TreeTop *insertionPoint,
    TR::Node *value,
    TR_AbstractHashTableProfilerInfo *table,
    TR::Node *optionalTest,
    TR::Node *fallbackValue,
    bool extendBlocks,
    bool trace)
   {
   // Common types used in calculation
   TR::DataType counterType = TR::Int32;
   TR::DataType lockType    = TR::Int16;
   TR::DataType systemType  = TR::Compiler->target.is64Bit() ? TR::Int64 : TR::Int32;

   // Type to use in calculations and table access
   TR::DataType roundedType    = value->getType();
   if (roundedType == TR::Int8 || roundedType == TR::Int16)
      roundedType = TR::Int32;
   if (roundedType == TR::Address)
      roundedType = systemType;

   if (trace)
      {
      traceMsg(comp, "Inserting value profiling trees:\n  Value n%dn At n%dn\n  Table: %p\n",
         value->getGlobalIndex(),
         insertionPoint->getNode()->getGlobalIndex(),
         table);
      table->dumpInfo(comp->getOutFile());
      traceMsg(comp, "  Test: %p Fallback: %p\n", optionalTest, fallbackValue);
      }

   TR::Block *originalBlock = insertionPoint->getEnclosingBlock();
   TR::Block *extendedBlock = originalBlock;
   while (extendedBlock->isExtensionOfPreviousBlock())
      extendedBlock = extendedBlock->getPrevBlock();

   if (trace)
      {
      traceMsg(comp, "  Modifying block_%d", originalBlock->getNumber());
      if (extendedBlock && extendedBlock != originalBlock)
         traceMsg(comp, " extending block_%d", extendedBlock->getNumber());
      traceMsg(comp, " with profiling\n");
      }

   // Cache the last treetop in the CFG
   TR::CFG *cfg = comp->getFlowGraph();
   cfg->setStructure(0);
   TR::Block *cursor = originalBlock, *prev = originalBlock;
   while (cursor && cursor != cfg->getEnd() && cursor != cfg->getStart() && cursor->getNumber())
      {
      prev = cursor;
      cursor = cursor->getNextBlock();
      }
   TR::TreeTop *lastTreeTop = prev->getExit();

   // Example node to use when constructing others 
   TR::Node *example = value;

   /********************* original Block *********************/
   if (trace)
      traceMsg(comp, " Profiled value n%dn into temp\n", value->getGlobalIndex());

   // Store the value and replace references to it, if no fallback value can overwrite its value
   TR::SymbolReference *storedValueSymRef = NULL;
   TR::TreeTop *storeValue = TR::TreeTop::create(comp, insertionPoint, storeNode(comp, value, storedValueSymRef));
   if (!optionalTest)
      replaceNode(comp, extendedBlock->getEntry(), storeValue->getNextTreeTop(),
         value, TR::Node::createLoad(example, storeValue->getNode()->getSymbolReference()));

   // Split after the store
   TR::Block *quickTest = originalBlock->split(storeValue->getNextTreeTop(), cfg, true, true);

   /********************* fallback Block *********************/

   // Insert the optional test and split after it
   if (optionalTest)
      {
      TR_ASSERT_FATAL(fallbackValue, "If introducing an optional test, a fallback value must be specified");
      TR::TreeTop *testTree = TR::TreeTop::create(comp, insertionPoint, optionalTest);

      TR::Block *mapping = originalBlock->split(testTree->getNextTreeTop(), cfg, true, true);
      if (extendBlocks)
         mapping->setIsExtensionOfPreviousBlock();
      
      // Create the fallback block and link it to the optional test
      TR::Block *fallback = TR::Block::createEmptyBlock(comp, MAX_COLD_BLOCK_COUNT + 1);
      fallback->setIsCold();
      lastTreeTop->join(fallback->getEntry());
      lastTreeTop = fallback->getExit();
      cfg->addNode(fallback);
      cfg->addEdge(originalBlock, fallback);
      cfg->addEdge(fallback, quickTest);
      testTree->getNode()->setBranchDestination(fallback->getEntry());     

      if (trace)
         traceMsg(comp, "  Fallback block_%d store to symRef #%d of node %p in \n", fallback->getNumber(),
            storedValueSymRef->getReferenceNumber(), fallback);

      // Store and return to the mainline
      TR::TreeTop::create(comp, fallback->getEntry(), TR::Node::create(example, TR::Goto, 0, quickTest->getEntry()));
      TR::TreeTop::create(comp, fallback->getEntry(), storeNode(comp, fallbackValue, storedValueSymRef));
      }
   else if (extendBlocks)
      quickTest->setIsExtensionOfPreviousBlock();

   /********************* quickTest Block *********************/
   // If blocks aren't being extended, its necessary to create a load
   TR::Node *quickTestValue = value;
   if (!extendBlocks || optionalTest)
      quickTestValue = TR::Node::createLoad(example, storedValueSymRef);
   quickTestValue = convertType(quickTestValue, roundedType);

   if (trace)
      traceMsg(comp, "  Hash calculation in block_%d\n", quickTest->getNumber());

   // Base address for the table accesses, simplifies codegen
   TR::Node *address = TR::Node::aconst(example, table->getBaseAddress());
   TR::Node *index = convertType(computeHash(comp, table, quickTestValue, address), systemType);
   
   if (trace)
      traceMsg(comp, "  Key test based on index n%dn\n", quickTest->getNumber(), index->getGlobalIndex());

   // Generate the check to ensure the value is already in the chosen slot
   TR::Node *checkKey = TR::Node::createif(comp->il.opCodeForIfCompareNotEquals(roundedType), quickTestValue,
      loadValue(comp, roundedType, address, index, systemConst(example, table->getKeysOffset())));
   TR::TreeTop *checkKeyTree = TR::TreeTop::create(comp, quickTest->getEntry(), checkKey);

   /********************* quickInc Block *********************/
   // If we aren't extending the blocks, anchor stores for the table address and the value
   TR::Node *quickIncAddress = address;
   TR::Node *quickIncIndex = index;
   if (!extendBlocks)
      {
      TR::SymbolReference *storedIndexSymRef = NULL;
      TR::SymbolReference *storedAddressSymRef = NULL;
      TR::TreeTop::create(comp, quickTest->getEntry(), storeNode(comp, index, storedIndexSymRef));
      TR::TreeTop::create(comp, quickTest->getEntry(), storeNode(comp, address, storedAddressSymRef));
      storedAddressSymRef->getSymbol()->setNotCollected();

      quickIncIndex = TR::Node::createLoad(example, storedIndexSymRef);
      quickIncAddress = TR::Node::createLoad(example, storedAddressSymRef);
      }

   // Split the block, creating a new block to hold the successful quick increment
   TR::Block *quickInc = quickTest->split(checkKeyTree->getNextTreeTop(), cfg, true, true);
   if (extendBlocks)
      quickInc->setIsExtensionOfPreviousBlock();

   if (trace)
      traceMsg(comp, "  Quick increment in block_%d\n", quickInc->getNumber());

   TR::Node *counterOffset = systemConst(example, table->getFreqOffset());
   TR::TreeTop *incTree = TR::TreeTop::create(comp, quickInc->getEntry(),
      incrementMemory(comp, counterType, effectiveAddress(counterType, quickIncAddress, quickIncIndex, counterOffset)));

   // Split the block again, after the increment, so cold paths can merge back
   TR::Block *mainlineReturn = quickInc->split(incTree->getNextTreeTop(), cfg, true, true);

   /********************* slowTest Block *********************/
   TR::Block *slowTest = TR::Block::createEmptyBlock(comp, MAX_COLD_BLOCK_COUNT + 1);
   slowTest->setIsCold();
   lastTreeTop->join(slowTest->getEntry());
   lastTreeTop = slowTest->getExit();
   cfg->addNode(slowTest);
   cfg->addEdge(quickTest, slowTest);
   cfg->addEdge(slowTest, mainlineReturn);
   checkKey->setBranchDestination(slowTest->getEntry());

   if (trace)
      traceMsg(comp, "  Lock test in block_%d\n", slowTest->getNumber());

   // Load the lock and test if its set
   TR::Node *lockAddress = TR::Node::aconst(example, table->getBaseAddress() + table->getLockOffset());
   TR::Node *lock = loadValue(comp, lockType, lockAddress);
   TR::Node *checkTableLock = TR::Node::createif(TR::ifscmplt, lock, TR::Node::sconst(example, 0));
   TR::TreeTop::create(comp, slowTest->getEntry(), checkTableLock);

   /********************* slowInc Block *********************/
   TR::Node *slowIncIndex = convertType(lock, systemType, false);
   if (!extendBlocks)
      {
      TR::SymbolReference *storedLockSymRef = NULL;
      TR::TreeTop::create(comp, slowTest->getEntry(), storeNode(comp, slowIncIndex, storedLockSymRef));
      slowIncIndex = TR::Node::createLoad(example, storedLockSymRef);
      }

   // Split the block, creating a new block to hold the other increment
   TR::Block *slowInc = slowTest->split(slowTest->getExit(), cfg, true, true);
   lastTreeTop = slowInc->getExit();
   if (extendBlocks)
      slowInc->setIsExtensionOfPreviousBlock();

   if (trace)
      traceMsg(comp, "  Slow increment in block_%d\n", slowInc->getNumber());

   // Add the other increment and return to mainline
   TR::TreeTop::create(comp, slowInc->getEntry(), TR::Node::create(example, TR::Goto, 0, mainlineReturn->getEntry()));
   TR::Node *counterAddress = TR::Node::aconst(example, table->getBaseAddress() + table->getFreqOffset());
   TR::TreeTop::create(comp, slowInc->getEntry(), incrementMemory(comp, counterType,
      effectiveAddress(counterType, counterAddress, slowIncIndex)));

   /********************* helper Block *********************/
   // Build the helper call path
   TR::Block *helper = TR::Block::createEmptyBlock(comp, MAX_COLD_BLOCK_COUNT + 1);
   helper->setIsCold();
   lastTreeTop->join(helper->getEntry());
   lastTreeTop = helper->getExit();
   cfg->addNode(helper);
   cfg->addEdge(slowTest, helper);
   cfg->addEdge(helper, mainlineReturn);
   checkTableLock->setBranchDestination(helper->getEntry());

   if (trace)
      traceMsg(comp, "  Helper call in block_%d\n", helper->getNumber());

   // Add the call to the helper and return to the mainline
   TR::TreeTop::create(comp, helper->getEntry(), TR::Node::create(example, TR::Goto, 0, mainlineReturn->getEntry()));
   TR::TreeTop *helperCallTreeTop = TR::TreeTop::create(comp, helper->getEntry(), createHelperCall(comp,
      convertType(TR::Node::createLoad(example, storedValueSymRef), roundedType),
      TR::Node::aconst(example, table->getBaseAddress())));

   // Set profiling code flags
   TR::NodeChecklist checklist(comp);
   checklist.add(value);
   TR::TreeTop *tt = quickTest->getEntry(), *end = mainlineReturn->getEntry(); 
   while (tt && tt != end)
      {
      TR::Node *node = tt->getNode();
      if (node->getOpCodeValue() != TR::BBStart && node->getOpCodeValue() != TR::BBEnd)
         setProfilingCode(node, checklist);
      tt = tt->getNextTreeTop();
      }
   tt = slowTest->getEntry();
   while (tt)
      {
      TR::Node *node = tt->getNode();
      if (node->getOpCodeValue() != TR::BBStart && node->getOpCodeValue() != TR::BBEnd)
         setProfilingCode(node, checklist);
      tt = tt->getNextTreeTop();
      }

   return true;
   }

/*
 * Recursive call to replace origNode with newNode underneath a target node.
 *
 * \param check Node to search for reference to origNode.
 * \param origNode Node to replace.
 * \param newNode Node to use in its place.
 * \param checklist Checklist of nodes that have already been searched.
 */
void
TR_JProfilingValue::replaceNode(TR::Node* check, TR::Node* origNode, TR::Node *newNode, TR::NodeChecklist &checklist)
   {
   if (checklist.contains(check))
      return;
   checklist.add(check);

   for (uint32_t i = 0; i < check->getNumChildren(); ++i)
      {
      if (origNode && check->getChild(i) == origNode)
         {
         check->setAndIncChild(i, newNode);
         origNode->decReferenceCount();
         }
      else
         replaceNode(check->getChild(i), origNode, newNode, checklist);
      }
   }

/*
 * Replace all references to origNode under and after replaceStart with newNode.
 *
 * \param blockStart Start of the extended block containing replaceStart.
 * \param origNode Node to replace.
 * \param newNode Node to use in its place.
 */
void
TR_JProfilingValue::replaceNode(TR::Compilation *comp, TR::TreeTop *blockStart, TR::TreeTop *replaceStart,
    TR::Node *origNode, TR::Node *newNode)
   {
   TR::NodeChecklist checklist(comp);
   TR::TreeTop *tt = blockStart; 

   // Collect all nodes seen before start of replace, these shouldn't be replaced
   while (tt != replaceStart)
      {
      replaceNode(tt->getNode(), NULL, NULL, checklist);
      tt = tt->getNextTreeTop();
      }

   // Begin replacing until the end of the extended block
   while (tt)
      {
      if (tt->getNode()->getOpCodeValue() == TR::BBStart && !tt->getNode()->getBlock()->isExtensionOfPreviousBlock())
         break;
      replaceNode(tt->getNode(), origNode, newNode, checklist);
      tt = tt->getNextTreeTop();
      }
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
   if (value->getDataType() == TR::Int32)
      profiler = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_jProfile32BitValue, false, false, false);
   else
      profiler = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_jProfile64BitValue, false, false, false);

#if defined(TR_HOST_POWER) || defined(TR_HOST_ARM) || defined(TR_HOST_S390)
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
   TR::ILOpCodes constOp = TR::Compiler->target.is64Bit() ? TR::lconst : TR::iconst;
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

   TR::ILOpCodes addSys   = TR::Compiler->target.is64Bit() ? TR::aladd : TR::aiadd;
   TR::ILOpCodes constSys = TR::Compiler->target.is64Bit() ? TR::lconst : TR::iconst;

   TR::Node *hash = NULL;
   if (table->getHashType() == BitIndexHash)
      {
      // Build the bit permute tree
      TR::Node *hashAddr = TR::Node::create(value, addSys, 2, baseAddr, TR::Node::create(value, constSys, 0, table->getHashOffset()));
      hash = TR::Node::create(value, value->getDataType() == TR::Int32 ? TR::ibitpermute : TR::lbitpermute, 3);
      hash->setAndIncChild(0, value);
      hash->setAndIncChild(1, hashAddr);
      hash->setAndIncChild(2, TR::Node::iconst(value, table->getBits()));

      // Convert to the platform address width
      hash = convertType(hash, TR::Compiler->target.is64Bit() ? TR::Int64 : TR::Int32);
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
