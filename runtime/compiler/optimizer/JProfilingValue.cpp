/*******************************************************************************
 * Copyright IBM Corp. and others 2017
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
#include "ras/Logger.hpp"

/**
 * Get the operation for direct store for a type.
 * It's not possible to reuse existing methods, as they may
 * widen the operation.
 *
 * \param The datatype to support.
 */
TR::ILOpCodes directStore(TR::DataType dt)
{
    switch (dt) {
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
 * It's not possible to reuse existing methods, as they may
 * widen the operation.
 *
 * \param The datatype to support.
 */
TR::ILOpCodes indirectStore(TR::DataType dt)
{
    switch (dt) {
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
 * It's not possible to reuse existing methods, as they may
 * widen the operation.
 *
 * \param The datatype to support.
 */
TR::ILOpCodes indirectLoad(TR::DataType dt)
{
    switch (dt) {
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
 * It's not possible to reuse existing methods, as they may
 * widen the operation.
 *
 * \param The datatype to support.
 */
TR::ILOpCodes loadConst(TR::DataType dt)
{
    switch (dt) {
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
int32_t TR_JProfilingValue::perform()
{
    OMR::Logger *log = comp()->log();

    if (comp()->getProfilingMode() == JProfiling) {
        logprints(trace(), log, "JProfiling has been enabled for profiling compilations, run JProfilingValue\n");
    } else if (comp()->getOption(TR_EnableJProfiling)) {
        logprints(trace(), log, "JProfiling has been enabled, run JProfilingValue\n");
    } else {
        logprints(trace(), log, "JProfiling has been disabled, skip JProfilingValue\n");
        return 0;
    }

    TR::list<TR::TreeTop *> valueProfilingPlaceHolderCalls(getTypedAllocator<TR::TreeTop *>(comp()->allocator()));

    cleanUpAndAddProfilingCandidates(valueProfilingPlaceHolderCalls);

    if (trace())
        comp()->dumpMethodTrees(log, "After Cleaning up Trees");

    lowerCalls(valueProfilingPlaceHolderCalls);

    if (comp()->isProfilingCompilation()) {
        TR::Recompilation *recomp = comp()->getRecompilationInfo();
        TR_ValueProfiler *profiler = recomp->getValueProfiler();
        TR_ASSERT_FATAL(profiler, "Recompilation should have a ValueProfiler in a profiling compilation");
        // This flag allows optimizer to add value profiling trees through profile post JProfilingValue
        profiler->setPostLowering();
    }
    return 1;
}

void TR_JProfilingValue::cleanUpAndAddProfilingCandidates(TR::list<TR::TreeTop *> &valueProfilingPlaceHolderCalls)
{
    TR::TreeTop *cursor = comp()->getStartTree();
    TR_BitVector *alreadyProfiledValues = new (comp()->trStackMemory()) TR_BitVector();
    TR::NodeChecklist checklist(comp());
    while (cursor) {
        TR::Node *node = cursor->getNode();
        /*
         * If we find a profiling candidate while walking the treetop, we add a
         * place holder call to profile the value. In this scenario a placeholder
         * call will be placed after the treetop in which we encounter a candidate
         * and decided to profile it. To make sure we do not examine unnecessary
         * trees, record next tree top before examining the tree and inserting a
         * profiling placeholder.
         */
        TR::TreeTop *nextTT = cursor->getNextTreeTop();
        if (node->isProfilingCode() && node->getOpCodeValue() == TR::treetop
            && node->getFirstChild()->getOpCode().isCall()
            && (comp()->getSymRefTab()->isNonHelper(node->getFirstChild()->getSymbolReference(),
                    TR::SymbolReferenceTable::jProfileValueSymbol)
                || comp()->getSymRefTab()->isNonHelper(node->getFirstChild()->getSymbolReference(),
                    TR::SymbolReferenceTable::jProfileValueWithNullCHKSymbol))) {
            TR::Node *value = node->getFirstChild()->getFirstChild();

            if ((alreadyProfiledValues->isSet(value->getGlobalIndex()) || value->getOpCode().isLoadConst())
                && performTransformation(comp(), "%s Removing profiling treetop, node n%dn is already profiled\n",
                    optDetailString(), value->getGlobalIndex())) {
                TR::TransformUtil::removeTree(comp(), cursor);
            } else {
                alreadyProfiledValues->set(value->getGlobalIndex());
                valueProfilingPlaceHolderCalls.push_back(cursor);
            }
        }
        // Emptying a bit vector after scanning whole extended basic blocks will keep number of bits set in bit vector
        // low.
        else if (node->getOpCodeValue() == TR::BBStart && !node->getBlock()->isExtensionOfPreviousBlock()) {
            alreadyProfiledValues->empty();
        } else {
            performOnNode(node, cursor, alreadyProfiledValues, &checklist, valueProfilingPlaceHolderCalls);
        }
        cursor = nextTT;
    }
}

void TR_JProfilingValue::performOnNode(TR::Node *node, TR::TreeTop *cursor, TR_BitVector *alreadyProfiledValues,
    TR::NodeChecklist *checklist, TR::list<TR::TreeTop *> &valueProfilingPlaceHolderCalls)
{
    if (checklist->contains(node))
        return;
    checklist->add(node);

    TR::TreeTop *preceedingTT = NULL;
    TR::Node *profiledNode = node->getNumChildren() > 0 ? node->getFirstChild() : NULL;
    TR::Node *constNode = NULL;
    TR::SymbolReference *profiler = NULL;

    if (profiledNode != NULL && !alreadyProfiledValues->isSet(profiledNode->getGlobalIndex())
        && !node->getByteCodeInfo().doNotProfile()) {
        if (node->getOpCode().isCallIndirect()
            && (node->getSymbol()->getMethodSymbol()->isVirtual()
                || node->getSymbol()->getMethodSymbol()->isInterface())) {
            preceedingTT = cursor;
            profiler = comp()->getSymRefTab()->findOrCreateJProfileValuePlaceHolderSymbolRef();
            performTransformation(comp(),
                "%s Adding JProfiling PlaceHolder call to profile virtual call node n%dn profiling n%dn\n",
                optDetailString(), node->getGlobalIndex(), profiledNode->getGlobalIndex());
        } else if (node->getOpCodeValue() == TR:: instanceof || node->getOpCodeValue() == TR::checkcast
                       || node->getOpCodeValue() == TR::checkcastAndNULLCHK) {
            preceedingTT = cursor->getPrevTreeTop();
            profiler = comp()->getSymRefTab()->findOrCreateJProfileValuePlaceHolderWithNullCHKSymbolRef();
            performTransformation(comp(),
                "%s Adding JProfiling PlaceHolder call with NULLCHK to profile checkcast/instanceOf node n%dn "
                "profiling VFT load of n%dn\n",
                optDetailString(), node->getGlobalIndex(), profiledNode->getGlobalIndex());
        }
    }

    if (profiler != NULL) {
        /*
         * In case of virtual/interface call, we already have VFT load of receiver
         * as first child of the call node for which we are profiling. In case of the
         * instanceof/checkcast we would not have VFT load of the object class.
         * During instruction selection, this will be loaded conditionally if the
         * object is not NULL. We would need profing data for instance of and
         * checkcast object class before we execute the code for such node and
         * hence trees are inserted before it. In case we can not lower all
         * profiling calls we have in the method, we would need to clean-up the
         * placeholder calls. In such case it is possible that we end up with the
         * VFT load of the object will be left anchored in its original position
         * unguarded. To prevent this issue, generating VFTLoad for such case is
         * delayed till we are inserting trees for profiling that value. Given that
         * we do not profile objects currently, it is OK to set object child of
         * instanceof/checkcast in the list to keep track of already profiled
         * values, if in future this changes, we need to ensure that we use correct
         * child to keep track of values being profiled.
         */
        alreadyProfiledValues->set(profiledNode->getGlobalIndex());
        TR::Node *call = TR::Node::createWithSymRef(node, TR::call, 2, profiler);
        call->setAndIncChild(0, profiledNode);
        TR_ValueProfileInfo *valueProfileInfo
            = TR_PersistentProfileInfo::getCurrent(comp())->findOrCreateValueProfileInfo(comp());
        TR_AbstractHashTableProfilerInfo *info = static_cast<TR_AbstractHashTableProfilerInfo *>(
            valueProfileInfo->getOrCreateProfilerInfo(node->getByteCodeInfo(), comp(), AddressInfo, HashTableProfiler));
        call->setAndIncChild(1, TR::Node::aconst(node, (uintptr_t)info));
        TR::TreeTop *callTree = TR::TreeTop::create(comp(), preceedingTT, TR::Node::create(TR::treetop, 1, call));
        callTree->getNode()->setIsProfilingCode();
        valueProfilingPlaceHolderCalls.push_back(callTree);
    }

    for (int i = 0; i < node->getNumChildren(); ++i)
        performOnNode(node->getChild(i), cursor, alreadyProfiledValues, checklist, valueProfilingPlaceHolderCalls);
}

void TR_JProfilingValue::lowerCalls(TR::list<TR::TreeTop *> &valueProfilingPlaceHolderCalls)
{
    TR::TreeTop *lastTreeTop = comp()->getMethodSymbol()->getLastTreeTop();

    bool stopProfiling = false;

    for (auto iter = valueProfilingPlaceHolderCalls.begin(); iter != valueProfilingPlaceHolderCalls.end(); ++iter) {
        TR::TreeTop *cursor = (*iter);
        TR::Node *node = cursor->getNode();
        TR::TreeTop *nextTreeTop = cursor->getNextTreeTop();
        int32_t ipMax = comp()->maxInternalPointers() / 2;
        static const char *ipl = feGetEnv("TR_ProfilingIPLimit");
        if (ipl) {
            static const int32_t ipLimit = atoi(ipl);
            ipMax = ipLimit;
        }

        if (!stopProfiling && (comp()->getSymRefTab()->getNumInternalPointers() >= ipMax))
            stopProfiling = true;

        if (!stopProfiling) {
            TR::Node *child = node->getFirstChild();
            dumpOptDetails(comp(), "%s Replacing profiling placeholder n%dn with value profiling trees\n",
                optDetailString(), child->getGlobalIndex());
            TR_AbstractHashTableProfilerInfo *table
                = (TR_AbstractHashTableProfilerInfo *)child->getSecondChild()->getAddress();
            lastTreeTop = addProfilingTrees(comp(), cursor, child, table, lastTreeTop, true, trace());
            // Remove the original trees and continue from the tree after the profiling
        } else {
            // Need to anchor the value node before the helper call node is removed.
            // Otherwise, the child value node could be currently anchored under the
            // helper call node. When the helper call node is removed, the value node
            // will be moved down and anchored where the next reference is.
            // It will be a problem if there is a store into this value between the helper
            // call node and the next reference. After the helper call node is removed,
            // the reference will load the updated value instead of the original value.
            //
            TR::Node *child = node->getFirstChild();
            TR::Node *value = child->getFirstChild();
            dumpOptDetails(comp(), "%s Anchoring n%dn before cursor n%dn is removed\n", optDetailString(),
                value->getGlobalIndex(), cursor->getNode()->getGlobalIndex());

            cursor->insertAfter(TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, value)));
        }

        TR::TransformUtil::removeTree(comp(), cursor);
        if (trace())
            comp()->dumpMethodTrees(comp()->log(), "After Adding Profiling Trees");
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
 * In case of profiling compilations where trees are inserted in mainline, it looks like
 * following.
 *
 * |---------------------------------------------------------------------------|
 * | originalBlock                                                             |
 * |---------------------------------------------------------------------------|
 * | ...                                                                       |
 * | call jProfileValueSymbol/jProfileValueWithNullCHKSymbol // insertionPoint |
 * |     value                                                                 |
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
 * |           value    OR   aloadi <vft-symbol>   |                                   |
 * |                            value              |                                   |
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
 *          | istorei                      |                v                          |
 *          |    al/aiadd                  |     |-------------------------------|     |
 *          |       aconst <table address> |     | helper                        |     |
 *          |       l/imul                 |     |-------------------------------|     |
 *          |           l/iselect          |     | call TR_jProfile32/64BitValue |     |
 *          |          width               |     |    value   OR   aloadi <vft-symbol> |
 *          |     iadd                     |     |                    value      |     |
 *          |        iloadi                |     |    table address              |     |
 *          |           => al/aiadd        |     |-------------------------------|     |
 *          |        iconst 1              |                         |                 |
 *          |------------------------------|                         |                 |
 *                         |                                         |                 |
 *                         |                                         |                 |
 *                         |<----------------------------------------|-----------------|
 *                         v
 *          |------------------------------|
 *          | mainlineReturn               |
 *          |------------------------------|
 *          |  ...                         |
 *          |------------------------------|
 *
 * \param insertionPoint Treetop to insert profiling code after.
 * \param value Value to profile.
 * \param table Persistent TR_HashMapInfo which will be filled and incremented during profiling.
 * \param optionalTest Option test node capable of preventing evaluation of value and using a fallbackValue instead.
 * \param extendBlocks Generates the blocks as extended, defaults true.
 * \param trace Enable tracing.
 */
TR::TreeTop *TR_JProfilingValue::addProfilingTrees(TR::Compilation *comp, TR::TreeTop *insertionPoint, TR::Node *node,
    TR_AbstractHashTableProfilerInfo *table, TR::TreeTop *lastTreeTop, bool extendBlocks, bool trace)
{
    OMR::Logger *log = comp->log();

    // Common types used in calculation
    TR::DataType counterType = TR::Int32;
    TR::DataType lockType = TR::Int16;
    TR::DataType systemType = comp->target().is64Bit() ? TR::Int64 : TR::Int32;

    TR::Node *value = node;

    /*
     * We can have a different bytecode info of the actual value we are profiling
     * compared to the node for which we are profiling to get the context for
     * which the value profiling is done.
     * To avoid generating trees stashed against incorrect bytecode info, if
     * trees are added during JProfilingValue::lowerTrees, this function is
     * called using the placeholder call. So following code checks for such case
     * and extracts the actual value child from the placeholder call. This will
     * allow us to generate all new nodes for the profiling using the bytecode
     * info of the actual node for which we are profiling.
     *
     */

    bool addNullCheck = false;
    if (node->getOpCode().isCall()) {
        TR::SymbolReference *callSymRef = node->getSymbolReference();
        bool isJProfileValueNonHelperCall = comp->getSymRefTab()->isNonHelper(node->getSymbolReference(),
            TR::SymbolReferenceTable::jProfileValueSymbol);

        bool isJProfileValueWithNullCHKNonHelperCall = comp->getSymRefTab()->isNonHelper(node->getSymbolReference(),
            TR::SymbolReferenceTable::jProfileValueWithNullCHKSymbol);
        if (isJProfileValueNonHelperCall || isJProfileValueWithNullCHKNonHelperCall) {
            value = node->getFirstChild();
            addNullCheck = isJProfileValueWithNullCHKNonHelperCall;
        }
    }
    logprintf(trace, log, "\t\tProfiling value n%dn , addNullCheck = %s\n", value->getGlobalIndex(),
        addNullCheck ? "true" : "false");

    TR::DataType roundedType = value->getType();
    if (roundedType == TR::Int8 || roundedType == TR::Int16)
        roundedType = TR::Int32;
    if (roundedType == TR::Address)
        roundedType = systemType;

    /********************* original Block *********************/
    TR::Block *originalBlock = insertionPoint->getEnclosingBlock();
    TR::CFG *cfg = comp->getFlowGraph();
    cfg->setStructure(NULL);

    /*
     * Anchor the value node after the split point to make sure that it will be either stored in the register or temp
     * slot before split point Most of the time, a last simplifier run will clean it up but in case it does not, we end
     * up with the unguarded null dereference in case we are profiling VFT load from object. To avoid that we do not
     * anchor the node when null test is needed. If the profiling place holder is generated in this pass, we make sure
     * that we generate the NULL Test before actual type test so in that case, it will be uncommoned by the block
     * splitter. If VFT Profiling is added in other passes, we will store the value in to temp slot for helper call
     * before we jump to helper call block.
     */
    if (!addNullCheck)
        insertionPoint->insertAfter(TR::TreeTop::create(comp, TR::Node::create(TR::treetop, 1, value)));

    /********************* mainline Return Block *********************/
    TR::Block *mainlineReturn = originalBlock->splitPostGRA(insertionPoint->getNextTreeTop(), cfg, true, NULL);

    logprintf(trace, log, "\t\tOriginal Block block_%d, mainline return block = block_%d\n", originalBlock->getNumber(),
        mainlineReturn->getNumber());
    bool generateProfilingTreesInMainline = comp->isProfilingCompilation();

    /*
     * If trees are inserted in profiling compilation (hot / very-hot), trees
     * will be inserted inlined as we expect these compilation to be short-lived.
     * In case we are adding trees in warm/cold optimizations with profiling
     * capability which can turn on profiling data collection and turn off by
     * simply patching the guard so in these cases, trees are added in the cold
     * path. In case of inserting trees in mainline, we will use the original
     * node as the profilingValue node, otherwise if trees are inserted in cold
     * block - we need to find the value node in the glRegDeps or store it in
     * temp slot for profiling code.
     */
    TR::Node *profilingValue = generateProfilingTreesInMainline ? value : NULL;

    TR::Node *originalBlockExitGlRegDeps = originalBlock->getExit()->getNode()->getNumChildren() == 1
        ? originalBlock->getExit()->getNode()->getFirstChild()
        : NULL;
    TR::Node *mainlineReturnEntryGlRegDeps
        = originalBlockExitGlRegDeps != NULL ? mainlineReturn->getEntry()->getNode()->getFirstChild() : NULL;

    // Now looking into the regdeps to see if we have uncommoned the profilingValue through global register.
    int32_t childRegDepIndexForProfilingValue = -1;
    if (originalBlockExitGlRegDeps != NULL) {
        for (auto childID = 0; childID < originalBlockExitGlRegDeps->getNumChildren(); ++childID) {
            TR::Node *child = originalBlockExitGlRegDeps->getChild(childID);

            while (child->getOpCodeValue() == TR::PassThrough)
                child = child->getFirstChild();

            if (child == value) {
                childRegDepIndexForProfilingValue = childID;
                logprintf(trace, log,
                    "\t\t\tFound value to be profiled stored in register - node index in regdeps = %d\n",
                    childRegDepIndexForProfilingValue);
                break;
            }
        }
    }

    // Register was not available to store the value, must be stored in tempslot by block splitter.
    TR::SymbolReference *storeProfileValueSymRef = NULL;
    if (childRegDepIndexForProfilingValue == -1) {
        for (TR::TreeTopIterator iter(insertionPoint->getNextTreeTop(), comp); iter != originalBlock->getExit();
             ++iter) {
            TR::Node *currentNode = iter.currentNode();
            if (currentNode->getOpCode().isStoreDirect() && currentNode->getFirstChild() == value) {
                storeProfileValueSymRef = currentNode->getSymbolReference();
                break;
            }
        }
        if (storeProfileValueSymRef == NULL) {
            // Very rare, but can happen when the value we are profiling is not stored in temp slot or register
            // This can happen if it is constant load (aconst / loadaddr) where rather than using a register or
            // temp slot, block splitter will copy the node.
            // There is no advantage in profiling constant addresses and pre lower treetop walks should weed out
            // such cases.
            TR::TreeTop *storeValueTT = TR::TreeTop::create(comp, storeNode(comp, value, storeProfileValueSymRef));
            originalBlock->append(storeValueTT);
        }
    }

    TR::Node *profilingCodeGuardNode = NULL;
    TR::Node *glRegDepsToCopyInProfilingCodeExit = NULL;
    TR::Block *mergeBlockForProfilingCode = NULL;
    TR::Block *profilingCodeBlock = NULL;

    if (lastTreeTop == NULL)
        lastTreeTop = comp->getMethodSymbol()->getLastTreeTop();

    if (generateProfilingTreesInMainline) {
        profilingCodeBlock = originalBlock->split(originalBlock->getExit(), cfg);
        profilingCodeBlock->setIsExtensionOfPreviousBlock();
        mergeBlockForProfilingCode = mainlineReturn;
        glRegDepsToCopyInProfilingCodeExit = originalBlockExitGlRegDeps;

        // In case of the profiling compilations, if the method is queued for
        // recompilation, we turn-off the profiling data collection so that it
        // does not cause disturbances with recompilation which consumes this
        // data.
        static bool disableJProfilingRecompQueueTest
            = feGetEnv("TR_DontGenerateJProfilingRecompQueueTestJProfilingValue") != NULL;
        if (!disableJProfilingRecompQueueTest) {
            TR_PersistentProfileInfo *profileInfo = comp->getRecompilationInfo()->findOrCreateProfileInfo();
            TR_BlockFrequencyInfo *bfi = TR_BlockFrequencyInfo::get(profileInfo);
            if (bfi != NULL) {
                TR::Node *loadIsQueuedForRecompilation = TR::Node::createWithSymRef(node, TR::iload, 0,
                    comp->getSymRefTab()->createKnownStaticDataSymbolRef(bfi->getIsQueuedForRecompilation(),
                        TR::Int32));
                profilingCodeGuardNode = TR::Node::createif(TR::ificmpeq, loadIsQueuedForRecompilation,
                    TR::Node::iconst(node, -1), mainlineReturn->getEntry());
            }
        }
    } else {
        // Trees are generated in the out-of-line section with the guard test to
        // check if enableJProfiling flag is set or not.
        // TODO: For patchable JProfiling, guard node should be changed to a
        // patchable dummy guard once we add runtime assumptions for them,
        profilingCodeBlock = TR::Block::createEmptyBlock(comp, MAX_COLD_BLOCK_COUNT + 1);
        cfg->addNode(profilingCodeBlock);
        lastTreeTop->join(profilingCodeBlock->getEntry());
        cfg->addEdge(originalBlock, profilingCodeBlock);
        mergeBlockForProfilingCode = profilingCodeBlock->split(profilingCodeBlock->getExit(), cfg);
        lastTreeTop = mergeBlockForProfilingCode->getExit();
        cfg->addEdge(mergeBlockForProfilingCode, mainlineReturn);
        TR::Node *gotoMainlineNode = TR::Node::create(node, TR::Goto, 0, mainlineReturn->getEntry());
        TR::TreeTop *gotoMainlineTT
            = TR::TreeTop::create(comp, mergeBlockForProfilingCode->getEntry(), gotoMainlineNode);
        if (mainlineReturnEntryGlRegDeps != NULL) {
            // TODO: Add comments for following section of the code that manages GlRegDeps
            // GlRegDeps - attached to BBStart of profilingCodeBlock
            TR::Node *glRegDepsToAttach = mainlineReturnEntryGlRegDeps->duplicateTree();
            profilingCodeBlock->getEntry()->getNode()->addChildren(&glRegDepsToAttach, 1);
            glRegDepsToAttach = copyGlRegDeps(comp, glRegDepsToAttach);
            profilingCodeBlock->getExit()->getNode()->addChildren(&glRegDepsToAttach, 1);

            // In case of adding profiling trees in cold code, we need to get the node that contains the value we are
            // profiling.
            profilingValue = childRegDepIndexForProfilingValue != -1
                ? glRegDepsToAttach->getChild(childRegDepIndexForProfilingValue)
                : NULL;

            glRegDepsToCopyInProfilingCodeExit = glRegDepsToAttach;

            glRegDepsToAttach = glRegDepsToAttach->duplicateTree();
            mergeBlockForProfilingCode->getEntry()->getNode()->addChildren(&glRegDepsToAttach, 1);
            glRegDepsToAttach = copyGlRegDeps(comp, glRegDepsToAttach);
            gotoMainlineNode->addChildren(&glRegDepsToAttach, 1);
        }

        if (profilingValue != NULL) {
            TR_ASSERT_FATAL_WITH_NODE(value, storeProfileValueSymRef != NULL,
                "Value node must be stored in register of temp slot before split point");
            profilingValue = TR::Node::createLoad(node, storeProfileValueSymRef);
        }

        TR_PersistentProfileInfo *profileInfo = comp->getRecompilationInfo()->findOrCreateProfileInfo();
        TR_BlockFrequencyInfo *bfi = TR_BlockFrequencyInfo::get(profileInfo);
        if (bfi != NULL) {
            TR::Node *loadIsJProfilingEnabled = TR::Node::createWithSymRef(node, TR::iload, 0,
                comp->getSymRefTab()->createKnownStaticDataSymbolRef(bfi->getEnableJProfilingRecompilation(),
                    TR::Int32));
            profilingCodeGuardNode = TR::Node::createif(TR::ificmpeq, loadIsJProfilingEnabled,
                TR::Node::iconst(node, -1), profilingCodeBlock->getEntry());
        }
    }

    // Inserting ProfilingCodeGuardNode in the mainline
    if (profilingCodeGuardNode != NULL) {
        TR::TreeTop *profilingCodeGuardTT = TR::TreeTop::create(comp, profilingCodeGuardNode);
        originalBlock->append(profilingCodeGuardTT);
        if (originalBlockExitGlRegDeps != NULL) {
            TR::Node *profilingGuardNodeGlRegDeps = copyGlRegDeps(comp, originalBlockExitGlRegDeps);
            profilingCodeGuardNode->addChildren(&profilingGuardNodeGlRegDeps, 1);
        }
        logprintf(trace, log, "\t\t\tGuard node for profiling code - n%dn\n", profilingCodeGuardNode->getGlobalIndex());
        cfg->addEdge(originalBlock, mainlineReturn);
    }

    TR::Block *iter = profilingCodeBlock;
    TR::Node *actualValueToTest = profilingValue;

    if (addNullCheck) {
        TR::Node *nullTest = TR::Node::createif(TR::ifacmpeq, profilingValue, TR::Node::aconst(node, 0),
            mergeBlockForProfilingCode->getEntry());
        TR::TreeTop *nullTestTreeTop = TR::TreeTop::create(comp, nullTest);
        if (glRegDepsToCopyInProfilingCodeExit != NULL) {
            TR::Node *glRegDepsForNullTest = copyGlRegDeps(comp, glRegDepsToCopyInProfilingCodeExit);
            nullTest->addChildren(&glRegDepsForNullTest, 1);
        }
        iter->append(nullTestTreeTop);
        cfg->addEdge(iter, mergeBlockForProfilingCode);
        logprintf(trace, log, "\t\t\tAdded nulltest in block_%d\n", iter->getNumber());
        iter = iter->split(nullTestTreeTop->getNextTreeTop(), cfg);
        iter->setIsExtensionOfPreviousBlock();
        actualValueToTest = TR::Node::createWithSymRef(node, TR::aloadi, 1, profilingValue,
            comp->getSymRefTab()->findOrCreateVftSymbolRef());
    }

    logprintf(trace, log, "Actual Value Node used in the profiling trees = n%dn\n",
        actualValueToTest->getGlobalIndex());

    TR::Node *quickTestValue = convertType(actualValueToTest, roundedType);
    TR::Node *address = TR::Node::aconst(node, table->getBaseAddress());
    TR::Node *hashIndex = computeHash(comp, table, quickTestValue, address);

    TR::Node *lockOffsetAddress = TR::Node::aconst(node, table->getBaseAddress() + table->getLockOffset());
    TR::Node *lock = loadValue(comp, lockType, lockOffsetAddress);
    TR::Node *otherIndex = convertType(lock, roundedType, false);
    TR::Node *convertedHashIndex = convertType(hashIndex, systemType, true);
    TR::Node *addressOfKeys = TR::Node::aconst(node, table->getBaseAddress() + table->getKeysOffset());
    TR::Node *conditionNode = TR::Node::create(node, comp->il.opCodeForCompareEquals(roundedType), 2, quickTestValue,
        loadValue(comp, roundedType, addressOfKeys, convertedHashIndex));

    TR::Node *selectNode
        = TR::Node::create(comp->il.opCodeForSelect(roundedType), 3, conditionNode, hashIndex, otherIndex);
    TR::TreeTop *incIndexTreeTop = TR::TreeTop::create(comp, TR::Node::create(TR::treetop, 1, selectNode));
    iter->append(incIndexTreeTop);

    TR::Node *checkIfTableIsLockedNode = TR::Node::create(node, comp->il.opCodeForCompareGreaterOrEquals(lockType), 2,
        lock, TR::Node::sconst(node, 0));
    TR::Node *checkNode = TR::Node::createif(TR::ificmpeq,
        TR::Node::create(node, TR::ior, 2, conditionNode, checkIfTableIsLockedNode), TR::Node::iconst(0));
    if (glRegDepsToCopyInProfilingCodeExit != NULL) {
        TR::Node *exitGlRegDeps = copyGlRegDeps(comp, glRegDepsToCopyInProfilingCodeExit);
        checkNode->addChildren(&exitGlRegDeps, 1);
    }

    TR::TreeTop *checkNodeTreeTop = TR::TreeTop::create(comp, incIndexTreeTop, checkNode);

    /********************* quickInc index Block *********************/
    TR::Node *counterOffset = TR::Node::iconst(node, table->getFreqOffset());
    TR::Node *counterBaseAddress = TR::Node::aconst(node, table->getBaseAddress() + table->getFreqOffset());
    TR::TreeTop *incTree = TR::TreeTop::create(comp, checkNodeTreeTop,
        incrementMemory(comp, counterType,
            effectiveAddress(counterType, counterBaseAddress, convertType(selectNode, systemType, true))));
    TR::Block *quickTestBlock = iter;
    iter = iter->split(incTree, cfg);
    iter->setIsExtensionOfPreviousBlock();
    logprintf(trace, log, "\t\t\tQuickTest in block_%d, trees to increment table in block_%d\n",
        quickTestBlock->getNumber(), iter->getNumber());

    /********************* helper call block *********************/
    TR::Block *helper = TR::Block::createEmptyBlock(comp, MAX_COLD_BLOCK_COUNT + 1);
    helper->setIsCold();
    lastTreeTop->join(helper->getEntry());
    lastTreeTop = helper->getExit();
    cfg->addNode(helper);
    cfg->addEdge(quickTestBlock, helper);
    cfg->addEdge(helper, mergeBlockForProfilingCode);
    checkNode->setBranchDestination(helper->getEntry());

    TR::Node *valueChildOfHelperCall = NULL;
    TR::Node *goToMergeBlockNode = TR::Node::create(node, TR::Goto, 0, mergeBlockForProfilingCode->getEntry());
    TR::TreeTop::create(comp, helper->getEntry(), goToMergeBlockNode);

    if (mergeBlockForProfilingCode->getEntry()->getNode()->getNumChildren() == 1) {
        // Copying GlRegDeps of the Mainline Entry block to the helper block.
        TR::Node *duplicateGlRegDeps
            = mergeBlockForProfilingCode->getEntry()->getNode()->getFirstChild()->duplicateTree();
        helper->getEntry()->getNode()->setNumChildren(1);
        helper->getEntry()->getNode()->setAndIncChild(0, duplicateGlRegDeps);
        if (childRegDepIndexForProfilingValue != -1) {
            valueChildOfHelperCall = duplicateGlRegDeps->getChild(childRegDepIndexForProfilingValue);
        }
        duplicateGlRegDeps = copyGlRegDeps(comp, duplicateGlRegDeps);
        goToMergeBlockNode->addChildren(&duplicateGlRegDeps, 1);
    }

    if (valueChildOfHelperCall == NULL) {
        TR_ASSERT_FATAL_WITH_NODE(value, storeProfileValueSymRef != NULL,
            "Value to profile must be stored in temp slot or in register");
        valueChildOfHelperCall = TR::Node::createLoad(node, storeProfileValueSymRef);
    }

    if (addNullCheck) {
        valueChildOfHelperCall = TR::Node::createWithSymRef(node, TR::aloadi, 1, valueChildOfHelperCall,
            comp->getSymRefTab()->findOrCreateVftSymbolRef());
    }

    // Add the call to the helper and return to the mainline
    TR::TreeTop *helperCallTreeTop = TR::TreeTop::create(comp, helper->getEntry(),
        createHelperCall(comp, valueChildOfHelperCall, TR::Node::aconst(value, table->getBaseAddress())));
    logprintf(trace, log, "\t\t\tHelper call in block_%d\n", helper->getNumber());
    TR::NodeChecklist checklist(comp);
    checklist.add(value);
    TR::TreeTop *tt = profilingCodeBlock->getEntry();
    TR::TreeTop *end = generateProfilingTreesInMainline ? mainlineReturn->getEntry() : helper->getEntry();
    while (tt && tt != end) {
        TR::Node *node = tt->getNode();
        if (node->getOpCodeValue() != TR::BBStart && node->getOpCodeValue() != TR::BBEnd)
            setProfilingCode(node, checklist);
        tt = tt->getNextTreeTop();
    }
    tt = helper->getEntry();
    while (tt) {
        TR::Node *node = tt->getNode();
        if (node->getOpCodeValue() != TR::BBStart && node->getOpCodeValue() != TR::BBEnd)
            setProfilingCode(node, checklist);
        tt = tt->getNextTreeTop();
    }
    return lastTreeTop;
}

/**
 * Utility function to Copy the GlRegDeps
 *
 * @param comp Compilation Object
 * @param origGlRegDeps Original GlRegDeps to be copied
 * @return TR::Node* New GlRegDeps copied from origGlRegDeps
 */
TR::Node *TR_JProfilingValue::copyGlRegDeps(TR::Compilation *comp, TR::Node *origGlRegDeps)
{
    TR::Node *copiedGlRegDeps = TR::Node::create(origGlRegDeps, TR::GlRegDeps, origGlRegDeps->getNumChildren());
    for (int32_t i = 0; i < origGlRegDeps->getNumChildren(); i++) {
        TR::Node *child = origGlRegDeps->getChild(i);
        if (child->getOpCodeValue() == TR::PassThrough) {
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
void TR_JProfilingValue::setProfilingCode(TR::Node *node, TR::NodeChecklist &checklist)
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
TR::Node *TR_JProfilingValue::storeNode(TR::Compilation *comp, TR::Node *value, TR::SymbolReference *&symRef)
{
    if (symRef == NULL)
        symRef = comp->getSymRefTab()->createTemporary(comp->getMethodSymbol(), value->getDataType());

    if (value->getType() == TR::Address && value->getOpCode().hasSymbolReference()
        && !value->getSymbol()->isCollectedReference())
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
TR::Node *TR_JProfilingValue::effectiveAddress(TR::DataType dataType, TR::Node *base, TR::Node *index, TR::Node *offset)
{
    if (offset) {
        if (offset->getDataType() == TR::Int64)
            base = TR::Node::create(base, TR::aladd, 2, base, offset);
        else if (offset->getDataType() == TR::Int32)
            base = TR::Node::create(base, TR::aiadd, 2, base, offset);
        else
            TR_ASSERT_FATAL(0, "Invalid type for address calculation integer");
    }

    if (index) {
        uint8_t size = TR::DataType::getSize(dataType);
        if (index->getDataType() == TR::Int64)
            base = TR::Node::create(base, TR::aladd, 2, base,
                TR::Node::create(base, TR::lmul, 2, index, TR::Node::lconst(base, size)));
        else if (index->getDataType() == TR::Int32)
            base = TR::Node::create(base, TR::aiadd, 2, base,
                TR::Node::create(base, TR::imul, 2, index, TR::Node::iconst(base, size)));
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
TR::Node *TR_JProfilingValue::loadValue(TR::Compilation *comp, TR::DataType dataType, TR::Node *base, TR::Node *index,
    TR::Node *offset)
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
TR::Node *TR_JProfilingValue::createHelperCall(TR::Compilation *comp, TR::Node *value, TR::Node *table)
{
    TR::SymbolReference *profiler;
    TR::DataType valueType = value->getDataType();
    // If Profiling 1/2/4 Bytes values, we need to call TR_jProfile32BitValue
    // Instead of checking the type, checking size (in bytes) which should cover address type on 32 Bit platforms as
    // well
    if (value->getSize() <= 4) {
        if (value->getDataType() != TR::Address)
            value = convertType(value, TR::Int32);
        profiler = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_jProfile32BitValue);
    } else {
        profiler = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_jProfile64BitValue);
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
TR::Node *TR_JProfilingValue::incrementMemory(TR::Compilation *comp, TR::DataType counterType, TR::Node *address)
{
    TR::SymbolReference *symRef = comp->getSymRefTab()->findOrCreateArrayShadowSymbolRef(counterType, NULL);

    TR::Node *load = TR::Node::createWithSymRef(address, indirectLoad(counterType), 1, address, symRef);
    TR::Node *incCounter = TR::Node::create(address, counterType == TR::Int32 ? TR::iadd : TR::ladd, 2, load,
        TR::Node::create(address, loadConst(counterType), 0, 1));
    TR::Node *storeCounter = TR::Node::createWithSymRef(address, indirectStore(counterType), 2, address, symRef);
    storeCounter->setAndIncChild(1, incCounter);

    return storeCounter;
}

/*
 * Generate a constant node matching the system's address width. Used for address calculations.
 *
 * \param example Example node to copy BCI.
 * \param value Value to store in const.
 */
TR::Node *TR_JProfilingValue::systemConst(TR::Compilation *comp, TR::Node *example, uint64_t value)
{
    TR::ILOpCodes constOp = comp->target().is64Bit() ? TR::lconst : TR::iconst;
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
TR::Node *TR_JProfilingValue::convertType(TR::Node *index, TR::DataType dataType, bool zeroExtend)
{
    if (index->getDataType() == dataType)
        return index;

    return TR::Node::create(index, TR::ILOpCode::getProperConversion(index->getDataType(), dataType, zeroExtend), 1,
        index);
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
TR::Node *TR_JProfilingValue::computeHash(TR::Compilation *comp, TR_AbstractHashTableProfilerInfo *table,
    TR::Node *value, TR::Node *baseAddr)
{
    TR_ASSERT(table->getDataType() == TR::Int32 || table->getDataType() == TR::Int64,
        "HashTable only expected to hold 32bit and 64bit values");

    if (!baseAddr)
        baseAddr = TR::Node::aconst(value, (uintptr_t)table);

    TR::ILOpCodes addSys = comp->target().is64Bit() ? TR::aladd : TR::aiadd;
    TR::ILOpCodes constSys = comp->target().is64Bit() ? TR::lconst : TR::iconst;

    TR::Node *hash = NULL;
    if (table->getHashType() == BitIndexHash) {
        // Build the bit permute tree
        TR::Node *hashAddr = TR::Node::create(value, addSys, 2, baseAddr,
            TR::Node::create(value, constSys, 0, table->getHashOffset()));
        hash = TR::Node::create(value, value->getDataType() == TR::Int32 ? TR::ibitpermute : TR::lbitpermute, 3);
        hash->setAndIncChild(0, value);
        hash->setAndIncChild(1, hashAddr);
        hash->setAndIncChild(2, TR::Node::iconst(value, table->getBits()));
    } else if (table->getHashType() == BitShiftHash) {
        // Common operations, based on the value's width
        TR::ILOpCodes shiftOp = TR::lushr;
        TR::ILOpCodes andOp = TR::land;
        TR::ILOpCodes orOp = TR::lor;
        TR::ILOpCodes constOp = TR::lconst;
        if (table->getDataType() == TR::Int32) {
            shiftOp = TR::iushr;
            andOp = TR::iand;
            orOp = TR::ior;
            constOp = TR::iconst;
        }

        TR::SymbolReference *symRef = comp->getSymRefTab()->findOrCreateArrayShadowSymbolRef(TR::Int8, NULL);

        // Extract each bit and merge into final result
        for (uint32_t bit = 0; bit < table->getBits(); ++bit) {
            uint32_t shiftOffset = table->getHashOffset() + bit * sizeof(uint8_t);
            TR::Node *shiftAddress
                = TR::Node::create(value, addSys, 2, baseAddr, TR::Node::create(value, constSys, 0, shiftOffset));
            TR::Node *shift = TR::Node::createWithSymRef(value, TR::bloadi, 1, shiftAddress, symRef);
            TR::Node *bitShift = TR::Node::create(value, shiftOp, 2, value, convertType(shift, TR::Int32));
            TR::Node *bitExtract
                = TR::Node::create(value, andOp, 2, bitShift, TR::Node::create(value, constOp, 0, 1 << bit));
            if (hash)
                hash = TR::Node::create(value, orOp, 2, hash, bitExtract);
            else
                hash = bitExtract;
        }
    } else
        TR_ASSERT(0, "Unsupported hash type");

    return hash;
}

const char *TR_JProfilingValue::optDetailString() const throw() { return "O^O JPROFILING VALUE: "; }
