/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "runtime/J9Profiler.hpp"
#include <string.h>                               // for memset, strncmp, etc
#include "codegen/CodeGenerator.hpp"              // for CodeGenerator
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "compile/Compilation.hpp"                // for Compilation
#include "compile/Method.hpp"                     // for TR_AOTMethodInfo, etc
#include "compile/ResolvedMethod.hpp"             // for TR_ResolvedMethod
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/OptionsUtil.hpp"
#include "control/Options_inlines.hpp"            // for TR::Options, etc
#include "control/Recompilation.hpp"              // for TR_Recompilation, etc
#include "control/RecompilationInfo.hpp"          // for TR_Recompilation, etc
#include "env/IO.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/PersistentInfo.hpp"                 // for PersistentInfo
#include "env/StackMemoryRegion.hpp"
#include "env/jittypes.h"                         // for TR_ByteCodeInfo, etc
#include "env/VMJ9.h"
#include "il/Block.hpp"                           // for Block, toBlock
#include "il/DataTypes.hpp"                       // for CONSTANT64, etc
#include "il/ILOpCodes.hpp"                       // for ILOpCodes::iconst, etc
#include "il/ILOps.hpp"                           // for ILOpCode, etc
#include "il/Node.hpp"                            // for Node
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                          // for Symbol, etc
#include "il/SymbolReference.hpp"                 // for SymbolReference, etc
#include "il/TreeTop.hpp"                         // for TreeTop
#include "il/TreeTop_inlines.hpp"                 // for TreeTop::getNode, etc
#include "il/symbol/MethodSymbol.hpp"             // for MethodSymbol, etc
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Assert.hpp"                       // for TR_ASSERT
#include "infra/BitVector.hpp"                    // for TR_BitVector
#include "infra/Cfg.hpp"                          // for CFG, LOW_FREQ
#include "infra/List.hpp"                         // for List, ListIterator
#include "infra/TRCfgNode.hpp"                    // for CFGNode
#include "optimizer/Optimizations.hpp"
#include "optimizer/Structure.hpp"                // for TR_RegionStructure, etc
#include "optimizer/TransformUtil.hpp"            // for TransformUtil
#include "runtime/Runtime.hpp"
#include "runtime/ExternalProfiler.hpp"
#include "runtime/J9ValueProfiler.hpp"

//*************************************************************************
//
// Optimization passes to insert recompilation counters, etc
//
//*************************************************************************

#define OPT_DETAILS "O^O RECOMPILATION COUNTERS: "

TR_RecompilationModifier::TR_RecompilationModifier(TR::OptimizationManager *manager)
   : TR::Optimization(manager),
     _recompilation(manager->comp()->getRecompilationInfo())
   {
   if (_recompilation)
      {
      if (!comp()->fej9()->isAOT_DEPRECATED_DO_NOT_USE())
         requestOpt(OMR::recompilationModifier);
      }
   }

int32_t TR_RecompilationModifier::perform()
   {
   for (TR_RecompilationProfiler * rp = _recompilation->getFirstProfiler(); rp; rp = rp->getNext())
      rp->modifyTrees();
   return 1;
   }

const char *
TR_RecompilationModifier::optDetailString() const throw()
   {
   return "O^O RECOMPILATION COUNTERS: ";
   }

void TR_LocalRecompilationCounters::removeTrees()
   {
   TR::SymbolReference *symRef = _recompilation->getCounterSymRef();
   for (TR::TreeTop *treeTop = comp()->getStartTree();
        treeTop;
        treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *node = treeTop->getNode();
      if (node->getOpCodeValue() == TR::istore &&
          node->getSymbolReference() == symRef)
         {
         TR::TreeTop *prevTree = treeTop->getPrevTreeTop();
         TR::TransformUtil::removeTree(comp(), treeTop);
         treeTop = prevTree;
         }
      }
   }

void TR_LocalRecompilationCounters::modifyTrees()
   {
   if (!comp()->mayHaveLoops())
      return;

   if (debug("traceCounters"))
      {
      diagnostic("Starting Local Recompilation Counters for %s\n", comp()->signature());
      comp()->dumpMethodTrees("Trees before Local Recompilation Counters");
      }

   // Wherever there is an asynccheck tree, insert a counter decrement
   //
   for (TR::TreeTop *treeTop = comp()->getStartTree();
        treeTop;
        treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *node = treeTop->getNode();
      if (node->getOpCodeValue() == TR::asynccheck)
         {
         if (performTransformation(comp(), "%s LOCAL RECOMPILATION COUNTERS: Add recomp counter decrement at async check %p\n", OPT_DETAILS, node))
            {
            treeTop = TR::TreeTop::createIncTree(comp(), node, _recompilation->getCounterSymRef(), -1);
            setHasModifiedTrees(true);
            }
         }
      }

   if (debug("traceCounters"))
      {
      comp()->dumpMethodTrees("Trees after Local Recompilation Counters");
      }
   }

void TR_GlobalRecompilationCounters::modifyTrees()
   {
   if (!comp()->mayHaveLoops())
      return;

   if (debug("traceCounters"))
      {
      diagnostic("Starting Global Recompilation Counters for %s\n", comp()->signature());
      comp()->dumpMethodTrees("Trees before Global Recompilation Counters");
      }

   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   // Find all natural loops and insert a decrement of the counter to their
   // header blocks
   //
   TR::CFG *cfg = comp()->getFlowGraph();
   int32_t numBlocks = cfg->getNextNodeNumber();
   TR_BitVector headerBlocks(numBlocks, trMemory(), stackAlloc);

   examineStructure(cfg->getStructure(), headerBlocks);
   } // Scope of the stack memory region

   if (debug("traceCounters"))
      {
      comp()->dumpMethodTrees("Trees after Global Recompilation Counters");
      }
   }

void
TR_GlobalRecompilationCounters::examineStructure(
      TR_Structure *s,
      TR_BitVector &headerBlocks)
   {
   // If this is a block structure, insert a decrement of the recompilation
   // counter if the block has been marked as a header block
   //
   TR_BlockStructure *blockStructure = s->asBlock();
   if (blockStructure)
      {
      TR::Block *block = blockStructure->getBlock();
      if (headerBlocks.get(block->getNumber()))
         {
         if (performTransformation(comp(), "%s GLOBAL RECOMPILATION COUNTERS: Add recomp counter decrement at loop header block_%d\n", OPT_DETAILS, block->getNumber()))
            {
            TR::TreeTop::createIncTree(comp(), block->getEntry()->getNode(), _recompilation->getCounterSymRef(), -1, block->getEntry());
            setHasModifiedTrees(true);
            }
         }
      return;
      }

   // This is a region. If it is a natural loop, mark the header block
   //
   TR_RegionStructure *region = s->asRegion();
   if (region->isNaturalLoop())
      headerBlocks.set(region->getNumber());

   // Now look at the children
   //
   TR_RegionStructure::Cursor subNodes(*region);
   for (TR_StructureSubGraphNode *subNode = subNodes.getCurrent();
        subNode;
        subNode = subNodes.getNext())
      {
      examineStructure(subNode->getStructure(), headerBlocks);
      }
   }

TR_CatchBlockProfiler::TR_CatchBlockProfiler(
      TR::Compilation  * c,
      TR::Recompilation * r,
      bool initialCompilation)
   : TR_RecompilationProfiler(c, r, initialCompilation),
      _profileInfo(0),
      _throwCounterSymRef(0),
      _catchCounterSymRef(0)
   {
   }

void TR_CatchBlockProfiler::removeTrees()
   {
   if (debug("dontRemoveCatchBlockProfileTrees"))
      return;

   for (TR::TreeTop *tt = comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      if (node->getOpCodeValue() == TR::istore)
         {
         TR::SymbolReference * symRef = node->getSymbolReference();
         if (symRef == _throwCounterSymRef || symRef == _catchCounterSymRef)
            {
            TR::TreeTop *prevTree = tt->getPrevTreeTop();
            TR::TransformUtil::removeTree(comp(), tt);
            tt = prevTree;
            }
         }
      }
   }

void TR_CatchBlockProfiler::modifyTrees()
   {
   // neither the throw or catch symbol has been created then no need to walk the trees
   //
   TR::SymbolReferenceTable * symRefTab = comp()->getSymRefTab();
   if (!symRefTab->getSymRef(TR_aThrow) && !symRefTab->getSymRef(TR::SymbolReferenceTable::excpSymbol))
      return;

   TR::TreeTop * firstTree = comp()->getStartTree();
   for (TR::TreeTop * tt = firstTree; tt; tt = tt->getNextTreeTop())
      {
      TR::Node * node = tt->getNode();
      if (((node->getOpCodeValue() == TR::athrow && !node->throwInsertedByOSR()) ||
          (node->getNumChildren() > 0 &&
           (node->getFirstChild()->getOpCodeValue() == TR::athrow) &&
           (!node->getFirstChild()->throwInsertedByOSR()))))
         {
         if (performTransformation(comp(), "%s CATCH BLOCK PROFILER: Add profiling trees to track the execution frequency of throw %p\n", OPT_DETAILS, node))
            {
            if (!_throwCounterSymRef)
               _throwCounterSymRef = symRefTab->createKnownStaticDataSymbolRef(&findOrCreateProfileInfo()->getThrowCounter(), TR::Int32);
            TR::TreeTop *profilingTree = TR::TreeTop::createIncTree(comp(), node, _throwCounterSymRef, 1, tt->getPrevTreeTop());
            profilingTree->getNode()->setIsProfilingCode();
            setHasModifiedTrees(true);
            }
         }
      }

   for (TR::Block * b = firstTree->getNode()->getBlock(); b; b = b->getNextBlock())
      if (!b->getExceptionPredecessors().empty() && !b->isOSRCatchBlock())
         {
         if (performTransformation(comp(), "%s CATCH BLOCK PROFILER: Add profiling trees to track the execution frequency of catch block_%d\n", OPT_DETAILS, b->getNumber()))
            {
            if (!_catchCounterSymRef)
               _catchCounterSymRef = symRefTab->createKnownStaticDataSymbolRef(&findOrCreateProfileInfo()->getCatchCounter(), TR::Int32);
            TR::TreeTop *profilingTree = TR::TreeTop::createIncTree(comp(), b->getEntry()->getNode(), _catchCounterSymRef, 1, b->getEntry());
            profilingTree->getNode()->setIsProfilingCode();
            setHasModifiedTrees(true);
            }
         }
   }


TR_CatchBlockProfileInfo *
TR_CatchBlockProfiler::findOrCreateProfileInfo()
   {
   if (!_profileInfo)
      {
      _profileInfo = _recompilation->findOrCreateProfileInfo()->getCatchBlockProfileInfo();
      if (!_profileInfo)
         {
         _profileInfo = new (PERSISTENT_NEW) TR_CatchBlockProfileInfo;
         _recompilation->findOrCreateProfileInfo()->setCatchBlockProfileInfo(_profileInfo);
         }
      }

   return _profileInfo;
   }


TR_BlockFrequencyProfiler::TR_BlockFrequencyProfiler(TR::Compilation  * c, TR::Recompilation * r)
   : TR_RecompilationProfiler(c, r)
   {
   }

void TR_BlockFrequencyProfiler::modifyTrees()
   {
   TR_PersistentMethodInfo *methodInfo = _recompilation->getMethodInfo();
   if (!methodInfo)
      return;

   TR_PersistentProfileInfo *profileInfo = methodInfo->getProfileInfo();
   if (!profileInfo)
      return;

   if (!comp()->haveCommittedCallSiteInfo())
      {
      TR_CallSiteInfo * const initialCallSiteInfo = new (PERSISTENT_NEW) TR_CallSiteInfo(comp(), persistentAlloc);
      TR_ASSERT(profileInfo->getCallSiteInfo() == NULL, "Profile already conatins a CallSiteInfo");
      profileInfo->setCallSiteInfo(initialCallSiteInfo);
      profileInfo->clearInfo();
      comp()->setCommittedCallSiteInfo(true);
      }
   else if (profileInfo->getCallSiteInfo()->getNumCallSites() != comp()->getNumInlinedCallSites())
      {
      TR_CallSiteInfo * const originalCallSiteInfo = profileInfo->getCallSiteInfo();
      TR_ASSERT(originalCallSiteInfo != NULL, "Existing CallSiteInfo should not be NULL.");
      TR_CallSiteInfo * const updatedCallSiteInfo = new (PERSISTENT_NEW) TR_CallSiteInfo(comp(), persistentAlloc);
      profileInfo->setCallSiteInfo(updatedCallSiteInfo);
      // FIXME: originalCallSiteInfo and its _blocks array allocation appear to leak.
      }


   TR_BlockFrequencyInfo *blockFrequencyInfo = new (PERSISTENT_NEW) TR_BlockFrequencyInfo(comp(), persistentAlloc);
   profileInfo->setBlockFrequencyInfo(blockFrequencyInfo);

   TR_ByteCodeInfo invalidByteCodeInfo;
   invalidByteCodeInfo.setInvalidByteCodeIndex();
   invalidByteCodeInfo.setInvalidCallerIndex();
   invalidByteCodeInfo.setIsSameReceiver(0);
   invalidByteCodeInfo.setDoNotProfile(0);
   TR_ByteCodeInfo prevByteCodeInfo = invalidByteCodeInfo;
   for (TR::TreeTop *treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *node = treeTop->getNode();
      if (node->getOpCodeValue() == TR::BBStart)
         {
         // Start of a block

         // If the BBStart node has the same bytecode info as its predecessor
         // and is reachable from the predecessor, this indicates a block that
         // has been split and only the first of the two blocks needs to be
         // counted.
         //
         if ((node->getByteCodeInfo().getCallerIndex() == prevByteCodeInfo.getCallerIndex() && node->getByteCodeInfo().getByteCodeIndex() == prevByteCodeInfo.getByteCodeIndex()))
            {
            TR::Node *prevNode = treeTop->getPrevTreeTop()->getPrevRealTreeTop()->getNode();
            if (!prevNode->getOpCode().isBranch() || prevNode->getOpCode().isIf())
               {
               // However, insert the counters if the current block is reachable from multiple blocks
               //
               if (!(node->getBlock()->getPredecessors().size() > 1))
                  continue;
               }
            }

         // If the block is marked as not to be profiled, don't add the
         // frequency counting tree.
         //
         if (node->getBlock()->doNotProfile())
            {
            prevByteCodeInfo = invalidByteCodeInfo;
            continue;
            }

        if (!performTransformation(comp(), "%s BLOCK FREQUENCY PROFILER: Add profiling trees to track the execution frequency of block_%d\n", OPT_DETAILS, node->getBlock()->getNumber()))
           continue;

         // Create the block frequency counting tree and put it after the
         // BBStart node.
         //
         TR::Node *storeNode;
         TR::SymbolReference *symRef = comp()->getSymRefTab()->createKnownStaticDataSymbolRef(blockFrequencyInfo->getFrequencyForBlock(node->getBlock()->getNumber()), TR::Int32);
         treeTop = TR::TreeTop::createIncTree(comp(), node, symRef, 1, treeTop);
         storeNode = treeTop->getNode();

         storeNode->setIsProfilingCode();
         prevByteCodeInfo = node->getByteCodeInfo();
         }

      else if (node->getOpCodeValue() == TR::asynccheck)
         {
         // Count the next block anyway, even if it is split from the previous
         // block.
         //
         prevByteCodeInfo = invalidByteCodeInfo;
         }
      }
   }


TR_ValueProfileInfo *
TR_ValueProfiler::findOrCreateValueProfileInfo()
   {
   if (!_valueProfileInfo)
      {
      _valueProfileInfo = _recompilation->findOrCreateProfileInfo()->getValueProfileInfo();
      if (!_valueProfileInfo)
         {
         _valueProfileInfo = new (PERSISTENT_NEW) TR_ValueProfileInfo;
         _recompilation->findOrCreateProfileInfo()->setValueProfileInfo(_valueProfileInfo);
         }
      }

   return _valueProfileInfo;
   }

TR_AbstractInfo *
TR_ValueProfiler::getProfiledValueInfo(
      TR::Node *node,
      TR::Compilation *comp,
      TR_ValueInfoType type)
   {
   TR_ValueProfileInfo *valueProfilerInfo = TR_ValueProfileInfo::get(comp);

   return valueProfilerInfo ? valueProfilerInfo->getValueInfo(node, comp, type) : 0;
   }

void TR_ValueProfiler::modifyTrees()
   {
   // For now only the only value profiling done for an initial compilation is
   // overridden virtual call profiling for method with catch blocks.  This is a small hack
   // to allow overridden virtual functions to be inlined when recompiling methods
   // that have lots of catch blocks.  This allows the throw to goto transformation
   // to speed up javac by 10%.
   //
   //if (getInitialCompilation() && !comp()->getSymRefTab()->getSymRef(TR::SymbolReferenceTable::excpSymbol))
   //   return;

   vcount_t visitCount = comp()->incVisitCount();
   TR::Block *block = NULL;
   for (TR::TreeTop * tt = comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      TR::Node * node = tt->getNode();
      if (node->getOpCodeValue() == TR::BBStart)
         block = node->getBlock();

      TR::Node * firstChild = node->getNumChildren() > 0 ? node->getFirstChild() : 0;
      if (firstChild && firstChild->getOpCodeValue() == TR::arraycopy &&
          !getInitialCompilation() && !debug("disableProfileArrayCopy"))
         {
         TR::Node *arrayCopyLen = firstChild->getChild(firstChild->getNumChildren()-1);
         if (!arrayCopyLen->getOpCode().isLoadConst() &&
             !arrayCopyLen->getByteCodeInfo().doNotProfile() &&
             !(arrayCopyLen->getOpCode().isCallIndirect() &&
               !arrayCopyLen->isTheVirtualCallNodeForAGuardedInlinedCall()))
             {
             TR_AbstractInfo *valueInfo = TR_PersistentProfileInfo::get(comp())->getValueProfileInfo()->getOrCreateValueInfo(firstChild, false, comp());
             addProfilingTrees(arrayCopyLen, tt, valueInfo, true);
             }
         }
      else if (firstChild && firstChild->getOpCode().isCall() &&
               firstChild->getVisitCount() != visitCount &&
               !debug("disableProfilingOfVirtualCalls"))
         {
         firstChild->setVisitCount(visitCount);

         TR::Node *callNode = firstChild;
         TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
         TR::MethodSymbol *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();

         bool doit=true;

         if (firstChild->isTheVirtualCallNodeForAGuardedInlinedCall())
             {
             doit=false;
             TR::Block * callBlock = tt->getEnclosingBlock();
             TR::Block * guard = callBlock->findVirtualGuardBlock(comp()->getFlowGraph());

             if (!guard || (guard->getLastRealTreeTop()->getNode()->isProfiledGuard() || !guard->getLastRealTreeTop()->getNode()->isNopableInlineGuard()))
                doit=true;
             }



         if (doit && firstChild->getOpCode().isCallIndirect()
         //&& !firstChild->getFirstChild()->getByteCodeInfo().doNotProfile() // We should profile this situation even if the bytecode has doNotProfile on it since we could have a node
                                                                         // that was cloned by loop versioner (or any other opt that clones node) that we still want to profile. The usual
                                                                         // risk with profiling a value that is different than the intended one because of arbitrary sharing of bytecode info between a node created by IL gen originally and newly created
                                                                        //  nodes that have nothing to do with any value seen in the original problem is not an issue here I feel
                                                                         // mostly because I do not see how we can have such a value appear as the vft child of a virtual call anyway
                                                                         // The vft child should share the bc info with the virtual call and no opt should be able to stick in a different expression at that precise spot in the IL trees

            && !firstChild->getSymbol()->castToMethodSymbol()->isComputed()) // 182550: We currently don't support profiling computed call targets because we have no way to map a target address to a MethodHandle object or to inline a thunk body
            {
            // todo: may have to implement an atoi opcode
            //
            bool picMiss = false;

            if (methodSymbol->isInterface())
               {
               TR_Method *interfaceMethod = methodSymbol->getMethod();
               int32_t         cpIndex      = methodSymRef->getCPIndex();
               int32_t len = interfaceMethod->classNameLength();
               char * s = classNameToSignature(interfaceMethod->classNameChars(), len, comp());
               TR_OpaqueClassBlock *thisClass = comp()->fe()->getClassFromSignature(s, len, methodSymRef->getOwningMethod(comp()));
               if (thisClass)
                  {
                  TR_PersistentCHTable * chTable = comp()->getPersistentInfo()->getPersistentCHTable();
                  picMiss = chTable->isKnownToHaveMoreThanTwoInterfaceImplementers(thisClass, cpIndex, methodSymRef->getOwningMethod(comp()), comp());
                  }
               }

            bool doit = true;

            if (firstChild->isTheVirtualCallNodeForAGuardedInlinedCall())
               {
               TR::Block * callBlock = tt->getEnclosingBlock();
               TR::Block * guard = callBlock->findVirtualGuardBlock(comp()->getFlowGraph());

               if (guard &&
                   (guard->getSuccessors().size() == 2) &&
                   guard->getExit() &&
                  (guard->getLastRealTreeTop()->getNode()->getOpCode().isBranch()) &&
                  (guard->getLastRealTreeTop()->getNode()->getBranchDestination() == callBlock->getEntry()))
                  {
                  TR::Node *callChild = callNode->getFirstChild();

                  if ((callChild->getOpCode().isLoadVarDirect() && callChild->getSymbolReference()->getSymbol()->isAutoOrParm()) ||
                      (callChild->getOpCode().hasSymbolReference() && (callChild->getSymbolReference() == comp()->getSymRefTab()->findVftSymbolRef())))
                     {
                     TR::Block *fallThroughBlock = guard->getNextBlock();
                     if (fallThroughBlock->getPredecessors().size() > 1)
                        fallThroughBlock = fallThroughBlock->splitEdge(guard, fallThroughBlock, comp());

                     TR::SymbolReference *callChildSymRef = callChild->getSymbolReference();
                     if ((callChild->getSymbolReference() == comp()->getSymRefTab()->findVftSymbolRef()) &&
                         callChild->getFirstChild()->getOpCode().hasSymbolReference())
                        callChildSymRef = callChild->getFirstChild()->getSymbolReference();

                     bool correctToClone = true;
                     TR::TreeTop *cursorTree = callBlock->getEntry();
                     while (cursorTree != tt)
                        {
                        TR::Node *cursorStoreNode = cursorTree->getNode()->getStoreNode();
                        if (cursorStoreNode &&
                            (cursorStoreNode->getSymbolReference() == callChildSymRef))
                           {
                           correctToClone = false;
                           break;
                           }

                       cursorTree = cursorTree->getNextTreeTop();
                       }

                     int32_t visitBudget = 20;
                     if (  correctToClone && !callChild->isUnsafeToDuplicateAndExecuteAgain(&visitBudget)
                        && performTransformation(comp(), "%s CALL PROFILE VERIFICATION: Duplicating node %p so it can be profiled near %p\n", callChild, fallThroughBlock->getEntry()))
                        {
                        TR::Node *dupChild = callChild->duplicateTree();

                        if (picMiss)
                           {
                           addProfilingTrees(dupChild, fallThroughBlock->getEntry(), 0, true, 20);
                           }
                        else
                           {
                           addProfilingTrees(dupChild, fallThroughBlock->getEntry(), 0, true, 0);
                           }
                        }
                     else
                        {
                        // We can't safely profile the fall-through path, so
                        // don't profile either path, or else we could get
                        // skewed profiling info.
                        //
                        doit = false;
                        }
                     }
                  }
               }

            if (doit)
               {
               if (picMiss)
                  {
                  addProfilingTrees(firstChild->getFirstChild(), tt, 0, true, 20);
                  }
               else
                  addProfilingTrees(firstChild->getFirstChild(), tt, 0, true, 0);
               }
            }

         if (comp()->cg()->getSupportsBigDecimalLongLookasideVersioning() &&
             !methodSymRef->isUnresolved() && !methodSymbol->isHelper() /* && !firstChild->getByteCodeInfo().doNotProfile() */)
            {
            TR::ResolvedMethodSymbol *method = firstChild->getSymbol()->getResolvedMethodSymbol();
            if ((method->getRecognizedMethod() == TR::java_math_BigDecimal_add) ||
                (method->getRecognizedMethod() == TR::java_math_BigDecimal_subtract) ||
                (method->getRecognizedMethod() == TR::java_math_BigDecimal_multiply))
               {
               if (!firstChild->getByteCodeInfo().doNotProfile())
                  addProfilingTrees(firstChild, tt, 0, true, 0, false, true);

               TR::Node *child = firstChild->getChild(firstChild->getNumChildren()-2);
               TR_ByteCodeInfo bcInfo = child->getByteCodeInfo();
               TR_ByteCodeInfo childBcInfo = firstChild->getByteCodeInfo();
               childBcInfo.setByteCodeIndex(childBcInfo.getByteCodeIndex()+1);

               child->setByteCodeInfo(childBcInfo);
               if (!child->getByteCodeInfo().doNotProfile())
                  addProfilingTrees(child, tt, 0, true, 0, false, true);
               child->setByteCodeInfo(bcInfo);

               child = firstChild->getChild(firstChild->getNumChildren()-1);
               bcInfo = child->getByteCodeInfo();
               childBcInfo.setByteCodeIndex(childBcInfo.getByteCodeIndex()+1);

               child->setByteCodeInfo(childBcInfo);
               if (!child->getByteCodeInfo().doNotProfile())
                  addProfilingTrees(child, tt, 0, true, 0, false, true);
               child->setByteCodeInfo(bcInfo);
               }
            }

         static bool doStringOpt = feGetEnv("TR_EnableStringOpt") ? true : false;

         if (doStringOpt && !methodSymRef->isUnresolved() && !methodSymbol->isHelper())
            {
            TR::ResolvedMethodSymbol *method = firstChild->getSymbolReference()->getSymbol()->castToResolvedMethodSymbol();
            TR_ResolvedMethod *m = method->getResolvedMethod();
            char *sig = "java/lang/String.<init>(";
            if ((strncmp(m->signature(trMemory()), sig, strlen(sig)) == 0) &&
                (strncmp(m->signatureChars(), "([CII)", 6)==0))
               {
               if (!firstChild->getFirstChild()->getByteCodeInfo().doNotProfile())
                  addProfilingTrees(firstChild->getFirstChild(), tt, 0, true, 20, false, false, true);
               }
            }
         }
      else if ((node->getOpCodeValue() == TR::ificmpne) ||
               (node->getOpCodeValue() == TR::ificmpeq))
         {
         // Profile elementCount field in Hashtable so that
         // method Hashtable.elements can be optimized. If elementCount
         // was almost never zero then the 'if' that checks if elementCount is
         // zero and returns an empty enumerator can be eliminated
         //
         TR::Node *firstChild = node->getFirstChild();
         TR::Node *secondChild = node->getSecondChild();
         if ((firstChild->getOpCodeValue() == TR::iloadi) &&
             (secondChild->getOpCodeValue() == TR::iconst) &&
             (secondChild->getInt() == 0))
            {
            TR::SymbolReference *symRef = firstChild->getSymbolReference();
            if (symRef->getSymbol()->getRecognizedField() == TR::Symbol::Java_util_Hashtable_elementCount)
               addProfilingTrees(firstChild, tt->getPrevTreeTop(), 0, true, 10);
            }
         }


      if (!getInitialCompilation())
         visitNode(node, tt, visitCount);
      }
   }

void
TR_ValueProfiler::visitNode(
      TR::Node *node,
      TR::TreeTop *tt,
      vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   static char *profileLongParms = feGetEnv("TR_ProfileLongParms");
   if (profileLongParms &&
       (node->getType().isInt64()) &&
       node->getOpCode().isLoadVar() &&
       !node->getByteCodeInfo().doNotProfile())
      {
      TR::Node *intNode = TR::Node::create(TR::l2i, 1, TR::Node::create(TR::lushr, 2, node, TR::Node::create(node, TR::iconst, 0, 32)));
      TR::ILOpCode &placeHolderOpCode = tt->getNode()->getOpCode();
      if (placeHolderOpCode.isBranch() ||
          placeHolderOpCode.isJumpWithMultipleTargets() ||
          placeHolderOpCode.isReturn() ||
          placeHolderOpCode.getOpCodeValue() == TR::athrow)
        addProfilingTrees(intNode, tt->getPrevTreeTop(), 0, true);
      else
        addProfilingTrees(intNode, tt, 0, true);
      }

   int32_t i;
   for (i = 0; i < node->getNumChildren(); i++)
     visitNode(node->getChild(i), tt, visitCount);
   }


void
TR_ValueProfiler::addProfilingTrees(
      TR::Node *node,
      TR::TreeTop *cursorTree,
      TR_AbstractInfo *valueInfo,
      bool commonNode,
      int32_t numExpandedValues,
      bool decrementRecompilationCounter,
      bool doBigDecimalProfiling,
      bool doStringProfiling)
   {
   bool validBigDecimalFieldOffset = true;
   int32_t scaleOffset = 0, flagOffset = 0;
   if(doBigDecimalProfiling)
      {
      if (!_bdClass)
         {
         TR_ResolvedMethod *owningMethod = comp()->getCurrentMethod();
         _bdClass = comp()->fe()->getClassFromSignature("Ljava/math/BigDecimal;\0", 22, owningMethod);
         }
      TR_OpaqueClassBlock * bdClass = _bdClass;
      char *fieldName = "scale";
      int32_t fieldNameLen = 5;
      char *fieldSig = "I";
      int32_t fieldSigLen = 1;

      scaleOffset = comp()->fej9()->getInstanceFieldOffset(bdClass, fieldName, fieldNameLen, fieldSig, fieldSigLen);
      fieldName = "flags";
      flagOffset = comp()->fej9()->getInstanceFieldOffset(bdClass, fieldName, fieldNameLen, fieldSig, fieldSigLen);

      if (scaleOffset == -1)
         {
         fieldName = "cachedScale";
         fieldNameLen = 11;
         scaleOffset = comp()->fej9()->getInstanceFieldOffset(bdClass, fieldName, fieldNameLen, fieldSig, fieldSigLen);
         }

      if(scaleOffset == -1 || flagOffset == -1)
         {
         validBigDecimalFieldOffset = false;
         TR_ASSERT(false, "scale or flags offset not found from getInstanceFieldOffset(...) in addProfilingTrees\n");
         }

      flagOffset += (int32_t) comp()->fe()->getObjectHeaderSizeInBytes();  // size of a J9 object header to move past it
      scaleOffset += (int32_t) comp()->fe()->getObjectHeaderSizeInBytes();  // size of a J9 object header to move past it
      }

   bool validStringFieldOffset = true;
   int32_t charsOffset = 0, lengthOffset = 0;
   if (doStringProfiling)
      {
      if (!_stringClass)
         {
         TR_ResolvedMethod *owningMethod = comp()->getCurrentMethod();
         _stringClass = comp()->fe()->getClassFromSignature("Ljava/lang/String;", 18, owningMethod);
         }

      TR_OpaqueClassBlock * stringClass = _stringClass;
      char *fieldName = "count";
      int32_t fieldNameLen = 5;
      char *fieldSig = "I";
      int32_t fieldSigLen = 1;

      lengthOffset = comp()->fej9()->getInstanceFieldOffset(stringClass, fieldName, fieldNameLen, fieldSig, fieldSigLen);
      fieldName = "value";
      fieldSig = "[C";
      fieldSigLen = 2;
      charsOffset = comp()->fej9()->getInstanceFieldOffset(stringClass, fieldName, fieldNameLen, fieldSig, fieldSigLen);

      if(charsOffset == -1 || lengthOffset == -1)
         {
         validStringFieldOffset = false;
         TR_ASSERT(false, "value or count offset not found from getInstanceFieldOffset(...) in addProfilingTrees\n");
         }

      lengthOffset += (int32_t) comp()->fe()->getObjectHeaderSizeInBytes();  // size of a J9 object header to move past it
      charsOffset += (int32_t) comp()->fe()->getObjectHeaderSizeInBytes();  // size of a J9 object header to move past it
      }

   if (!validBigDecimalFieldOffset || !validStringFieldOffset || comp()->getOption(TR_DisableValueProfiling) ||
       !performTransformation(comp(), "%s VALUE PROFILER: Add profiling trees to track the value of node %p near tree %p, commonNode %d, decrementRecompilationCounter %d, up to %d distinct values will be tracked \n", OPT_DETAILS, node, cursorTree->getNode(), commonNode, decrementRecompilationCounter, numExpandedValues))
      return;

   TR_PersistentProfileInfo * profileInfo = TR_PersistentProfileInfo::get(comp());
   if (!comp()->haveCommittedCallSiteInfo())
      {
      profileInfo->setCallSiteInfo(new (PERSISTENT_NEW) TR_CallSiteInfo(comp(), persistentAlloc));
      profileInfo->clearInfo();
      _recompilation->findOrCreateProfileInfo()->setValueProfileInfo(NULL);
      _recompilation->getValueProfiler()->setValueProfileInfo(NULL);
      if (comp()->getValueProfileInfoManager())
         comp()->getValueProfileInfoManager()->setJitValueProfileInfo(NULL);
      profileInfo->setValueProfileInfo(_recompilation->getValueProfiler()->findOrCreateValueProfileInfo());
      comp()->setCommittedCallSiteInfo(true);
      }
   else if (profileInfo->getCallSiteInfo()->getNumCallSites() != comp()->getNumInlinedCallSites())
      {
      profileInfo->setCallSiteInfo(new (PERSISTENT_NEW) TR_CallSiteInfo(comp(), persistentAlloc));
      }

   // decrementRecompilationCounter is only set for megamorphic PIC call sites and only when
   // we are compiling at warm opt level.
   if (!valueInfo)
      valueInfo = profileInfo->getValueProfileInfo()->getOrCreateValueInfo(node, decrementRecompilationCounter, comp(), (doBigDecimalProfiling ? BigDecimal : (doStringProfiling ? String : NotBigDecimalOrString)));

   TR::SymbolReference *profiler = comp()->getSymRefTab()->findOrCreateRuntimeHelper
      ((node->getDataType() == TR::Address? ( decrementRecompilationCounter ? TR_jitProfileWarmCompilePICAddress : (doBigDecimalProfiling ? TR_jitProfileBigDecimalValue : (doStringProfiling ? TR_jitProfileStringValue : TR_jitProfileAddress))) : (node->getType().isInt64() ? TR_jitProfileLongValue : TR_jitProfileValue)),
       false, false, true);
#if defined(TR_HOST_POWER) || defined(TR_HOST_ARM)
   profiler->getSymbol()->castToMethodSymbol()->setLinkage(TR_System);
#else
#ifndef TR_HOST_X86
   profiler->getSymbol()->castToMethodSymbol()->setPreservesAllRegisters();
#endif
   profiler->getSymbol()->castToMethodSymbol()->setSystemLinkageDispatch();
#endif

   TR::Node *recompilationCounter = TR::Node::aconst(node, 0);
   if (decrementRecompilationCounter)
      {
      TR::Recompilation *recompilationInfo   = comp()->getRecompilationInfo();
      TR::SymbolReference *recompilationCounterSymRef = recompilationInfo->getCounterSymRef();
      recompilationCounter = TR::Node::createWithSymRef(node, TR::loadaddr, 0, recompilationCounterSymRef);
      }

   int32_t numChildren;
   if (doBigDecimalProfiling)
      numChildren = 7;
   else if (doStringProfiling)
      numChildren = 6;
   else
      numChildren = 4;

   TR::Node *call = TR::Node::createWithSymRef(node, TR::call, numChildren, profiler);
   call->setAndIncChild(0, (commonNode ? node : node->duplicateTree()));

   int32_t childNum = 1;

   if (doBigDecimalProfiling)
      {
      TR::Node *bdarg = TR::Node::aconst(node, (uintptrj_t)_bdClass) ;
      bdarg->setIsClassPointerConstant(true);

      call->setAndIncChild(childNum++, bdarg);
      call->setAndIncChild(childNum++, TR::Node::create(node, TR::iconst, 0, scaleOffset));
      call->setAndIncChild(childNum++, TR::Node::create(node, TR::iconst, 0, flagOffset));
      }
   else if (doStringProfiling)
      {
      call->setAndIncChild(childNum++, TR::Node::create(node, TR::iconst, 0, charsOffset));
      call->setAndIncChild(childNum++, TR::Node::create(node, TR::iconst, 0, lengthOffset));
      }

   TR::Node *arg = TR::Node::aconst(node, (uintptrj_t)valueInfo) ;

   call->setAndIncChild(childNum++, arg);
   call->setAndIncChild(childNum++, TR::Node::create(node, TR::iconst, 0, numExpandedValues));
   call->setAndIncChild(childNum++, recompilationCounter);

   TR::TreeTop *callTree = TR::TreeTop::create(comp(), cursorTree, TR::Node::create(TR::treetop, 1, call));
   callTree->getNode()->setIsProfilingCode();
   }



TR_ValueProfileInfoManager::TR_ValueProfileInfoManager(TR::Compilation *comp)
   {
   _jitValueProfileInfo = NULL;
   _jitBlockFrequencyInfo = NULL;
   _cachedJ9Method = NULL;
   _isCountZero = false;

   TR_PersistentProfileInfo *profileInfo = TR_PersistentProfileInfo::get(comp);

   if (profileInfo &&
       profileInfo->getValueProfileInfo())
      _jitValueProfileInfo = profileInfo->getValueProfileInfo();

   if (profileInfo &&
       profileInfo->getBlockFrequencyInfo())
      _jitBlockFrequencyInfo = profileInfo->getBlockFrequencyInfo();
   }

TR_AbstractInfo *
TR_ValueProfileInfoManager::getValueInfo(TR::Node *node, TR::Compilation *comp, uint32_t profileInfoKind, TR_ValueInfoType type)
   {
   return getValueInfo(node->getByteCodeInfo(), comp, profileInfoKind, type);
   }

TR_AbstractInfo *
TR_ValueProfileInfoManager::getValueInfo(TR_ByteCodeInfo &bcInfo, TR::Compilation *comp, uint32_t profileInfoKind, TR_ValueInfoType type)
   {
   TR_AbstractInfo *info = NULL;

   if (_jitValueProfileInfo && (profileInfoKind != justInterpreterProfileInfo))
      info = _jitValueProfileInfo->getValueInfo(bcInfo, comp, type);

   if ((!info ||
       (info->getTotalFrequency() == 0)) &&
       (profileInfoKind != justJITProfileInfo))
      {

      TR_ValueProfileInfo *iVPInfo = comp->fej9()->getValueProfileInfoFromIProfiler(bcInfo, comp);

      if (iVPInfo)
         info = iVPInfo->getValueInfoFromExternalProfiler(bcInfo, comp);

      }
   return info;
   }

int32_t
TR_ValueProfileInfoManager::getCallGraphProfilingCount(TR_OpaqueMethodBlock * method, int32_t byteCodeIndex, TR::Compilation * comp)
   {
   // the call count information is stored in the interpreter profiler
   // persistent database
   return comp->fej9()->getIProfilerCallCount(method, byteCodeIndex, comp);
   }

int32_t
TR_ValueProfileInfoManager::getCallGraphProfilingCount(TR_OpaqueMethodBlock *calleeMethod, TR_OpaqueMethodBlock * method, int32_t byteCodeIndex, TR::Compilation * comp)
   {
   // the call count information is stored in the interpreter profiler
   // persistent database
   return comp->fej9()->getIProfilerCallCount(calleeMethod, method, byteCodeIndex, comp);
   }

int32_t
TR_ValueProfileInfoManager::getCallGraphProfilingCount(TR::Node *node, TR_OpaqueMethodBlock *method, TR::Compilation *comp)
   {
   return comp->fej9()->getCGEdgeWeight(node, method, comp);
   }

int32_t
TR_ValueProfileInfoManager::getCallGraphProfilingCount(TR::Node *node, TR::Compilation *comp)
   {
   return comp->fej9()->getIProfilerCallCount(node->getByteCodeInfo(), comp);
   }

bool
TR_ValueProfileInfoManager::isColdCall(TR_OpaqueMethodBlock *method, int32_t byteCodeIndex, TR::Compilation *comp)
   {
   return (getCallGraphProfilingCount(method, byteCodeIndex, comp) < (comp->getFlowGraph()->getLowFrequency()));
   }

bool
TR_ValueProfileInfoManager::isColdCall(TR_OpaqueMethodBlock *calleeMethod, TR_OpaqueMethodBlock *method, int32_t byteCodeIndex, TR::Compilation *comp)
   {
   return (getCallGraphProfilingCount(calleeMethod, method, byteCodeIndex, comp) < (comp->getFlowGraph()->getLowFrequency()));
   }

bool
TR_ValueProfileInfoManager::isColdCall(TR::Node* node, TR::Compilation *comp)
   {
   return (comp->fej9()->getIProfilerCallCount(node->getByteCodeInfo(), comp) < (comp->getFlowGraph()->getLowFrequency()));
   }

bool
TR_ValueProfileInfoManager::isWarmCall(TR::Node* node, TR::Compilation *comp)
   {
   return (comp->fej9()->getIProfilerCallCount(node->getByteCodeInfo(), comp) < (comp->getFlowGraph()->getLowFrequency()<<1));
   }

bool
TR_ValueProfileInfoManager::isHotCall(TR::Node* node, TR::Compilation *comp)
   {
   int32_t maxCallCount = comp->fej9()->getMaxCallGraphCallCount();

   // if the maximum call count is very low then don't bother
   if (maxCallCount < (comp->getFlowGraph()->getLowFrequency()<<1))
      return false;

   int32_t currentCallCount = comp->fej9()->getIProfilerCallCount(node->getByteCodeInfo(), comp);
   float ratio = (float)currentCallCount/(float)maxCallCount;

   // if the current call count is within 80% of the maximum call count in the program
   // it's hot and help out inliner to boost its inlining priority
   return (ratio >= 0.8f);
   }

float
TR_ValueProfileInfoManager::getAdjustedInliningWeight(TR::Node *callNode, int32_t weight, TR::Compilation *comp)
   {
   if (!isCallGraphProfilingEnabled(comp))
      return (float)weight;

   float callGraphAdjustedWeight = (float)weight;
   int32_t callGraphWeight = getCallGraphProfilingCount(callNode, comp);

   if (isWarmCall(callNode, comp))
      callGraphAdjustedWeight = 5000.0f;
   else if (isHotCall(callNode, comp))
      {
      if (weight < 0)
         callGraphAdjustedWeight = ((float)weight)*1.5f;
      else
         callGraphAdjustedWeight = ((float)weight)/1.5f;
      }

   return callGraphAdjustedWeight;
   }

bool
TR_ValueProfileInfoManager::isWarmCallGraphCall(TR::Node *node, TR_OpaqueMethodBlock *method, TR::Compilation *comp)
   {
   if (node->getSymbolReference() && node->getSymbolReference()->getSymbol() &&
       ((node->getSymbolReference()->getSymbol()->castToMethodSymbol()->getMethodKind()==TR::MethodSymbol::Special) ||
        (node->getSymbolReference()->getSymbol()->castToMethodSymbol()->getMethodKind()==TR::MethodSymbol::Static)))
      return false;

   return (getCallGraphProfilingCount(node, method, comp) < comp->getFlowGraph()->getLowFrequency());
   }

bool
TR_ValueProfileInfoManager::isCallGraphProfilingEnabled(TR::Compilation *comp)
   {
   if (comp->getCurrentMethod()->getPersistentIdentifier() == _cachedJ9Method)
      {
      if (_isCountZero)
         return false;
      }
   else
      {
      _cachedJ9Method = comp->getCurrentMethod()->getPersistentIdentifier();

      if (TR::Options::getCmdLineOptions()->getInitialCount() == 0 ||
          TR::Options::getCmdLineOptions()->getInitialBCount())
         {
         _isCountZero = true;
         return false;
         }

      TR::OptionSet * optionSet = TR::Options::findOptionSet(comp->trMemory(), comp->getCurrentMethod(), false);

      if (optionSet &&
          (optionSet->getOptions()->getInitialCount() == 0 ||
          (optionSet->getOptions()->getInitialBCount() == 0)))
         {
         _isCountZero = true;
         return false;
         }
      }

   return comp->fej9()->isCallGraphProfilingEnabled();
   }

void
TR_ValueProfileInfoManager::updateCallGraphProfilingCount(
      TR::Block *block,
      TR_OpaqueMethodBlock *method,
      int32_t byteCodeIndex,
      TR::Compilation *comp)
   {

   if(comp->getMethodHotness() > hot)  // don't update using jit profiling info
      return;

   // Cannot use JIT profiling info on dummy blocks from ECS.
   int32_t currentBlockFreq = 0;

   // if we don't have proper JIT profiling info get it using interpreter profiler
   // set frequency
   //if (currentBlockFreq <= 0)
   currentBlockFreq = block->getFrequency();

   if (currentBlockFreq > 0)
      {
      comp->fej9()->setIProfilerCallCount(method, byteCodeIndex, currentBlockFreq, comp);
      }
   }



float
TR_BranchProfileInfoManager::getCallFactor(int32_t callSiteIndex, TR::Compilation *comp)
   {
   float callFactor = 1.0;

   if (_iProfiler == NULL)
      return callFactor;

   // Speed-up te search by leveraging the fact that we are searching
   // all elements from one list into another list and both are sorted
   TR::list<TR_MethodBranchProfileInfo*> &infos = comp->getMethodBranchInfos();
   auto info = infos.begin(); // first element in my list of TR_MethodBranchProfileInfo

   int32_t calleeSiteIndex = callSiteIndex;
   while (calleeSiteIndex >= 0)
      {
      // Search for mbpInfo from where we left off
      for (; (info != infos.end()) && (*info)->getCallSiteIndex() > calleeSiteIndex; ++info)
         {}
      if (info == infos.end())
         break; // stop searching because we reached the end of the list with infos
      if ((*info)->getCallSiteIndex() == calleeSiteIndex)
         callFactor *= (*info)->getCallFactor();

      // Prepare for next iteration by getting to the parent of this caller
      TR_InlinedCallSite & site = comp->getInlinedCallSite(calleeSiteIndex);
      int32_t parentSiteIndex = site._byteCodeInfo.getCallerIndex();
      TR_ASSERT(parentSiteIndex < calleeSiteIndex, "Caller site index should be smaller than the parentSiteIndex");
      calleeSiteIndex = parentSiteIndex;
      }

   return callFactor;
   }

void
TR_BranchProfileInfoManager::getBranchCounters(TR::Node *node, TR::TreeTop *treeTop,
                             int32_t *branchToCount, int32_t *fallThroughCount, TR::Compilation *comp)
   {
   if (_iProfiler == NULL)
      {
      *branchToCount = 0;
      *fallThroughCount = 0;

      return;
      }

   TR_MethodBranchProfileInfo *mbpInfo = TR_MethodBranchProfileInfo::getMethodBranchProfileInfo(node->getInlinedSiteIndex(), comp);

   if (comp->getOption(TR_TraceBFGeneration))
      traceMsg(comp, "mbpInfo %p\n", mbpInfo);

   if (mbpInfo && node->getInlinedSiteIndex()>=0)
      {
      _iProfiler->getBranchCounters(node, treeTop, branchToCount, fallThroughCount, comp);

      float callFactor = getCallFactor(node->getInlinedSiteIndex(), comp);

      if (comp->getOption(TR_TraceBFGeneration))
         {
         traceMsg(comp, "Using call factor %f for callSiteIndex %d\n", callFactor, node->getInlinedSiteIndex());
         traceMsg(comp, "Orig branch to count %d and fall through count %d\n", *branchToCount, *fallThroughCount);
         }

      if ((*branchToCount <=0) && (*fallThroughCount<=0))
         {
         if (node->getBranchDestination()->getNode()->getBlock()->isCold())
            {
            *branchToCount = 0;

            return;
            }
         else
            *branchToCount = LOW_FREQ;

         if (treeTop->getEnclosingBlock()->getNextBlock() &&
             treeTop->getEnclosingBlock()->getNextBlock()->isCold())
            {
            *fallThroughCount = 0;

            return;
            }
         else
            *fallThroughCount = LOW_FREQ;

         }
      else
         {
         if (*branchToCount <=0) *branchToCount=1;
         if (*fallThroughCount<=0) *fallThroughCount=1;
         }

      if (comp->getOption(TR_TraceBFGeneration))
         traceMsg(comp, "Later branch to count %d and fall through count %d\n", *branchToCount, *fallThroughCount);

      int32_t breakEven = (*branchToCount > *fallThroughCount) ? 1 : -1;
      if (*branchToCount == *fallThroughCount) breakEven = 0;

      float edgeRatio = ((float)(*branchToCount) / (float)(*fallThroughCount));

      *branchToCount = (int32_t) ((float)(*branchToCount) * callFactor);
      *fallThroughCount = (int32_t) ((float)(*fallThroughCount) * callFactor);

      if (*branchToCount >= comp->getFlowGraph()->_max_edge_freq || *fallThroughCount >= comp->getFlowGraph()->_max_edge_freq)
         {
         if (breakEven > 0)
            {
            *branchToCount = comp->getFlowGraph()->_max_edge_freq;
            *fallThroughCount = (int32_t) ((float)comp->getFlowGraph()->_max_edge_freq / edgeRatio);
            }
         else
            {
            *fallThroughCount = comp->getFlowGraph()->_max_edge_freq;
            *branchToCount = (int32_t) ((float)comp->getFlowGraph()->_max_edge_freq * edgeRatio);
            }
         }

      if (((*branchToCount + breakEven) >= 0) && (*branchToCount == *fallThroughCount)) *branchToCount += breakEven;
      }
   else
      {
      _iProfiler->getBranchCounters(node, treeTop, branchToCount, fallThroughCount, comp);
      }
   }


TR_MethodBranchProfileInfo *
TR_MethodBranchProfileInfo::addMethodBranchProfileInfo (uint32_t callSiteIndex, TR::Compilation *comp)
   {
   // There is an embedded assumption that the new elements added to the list are in
   // increasing order. This helps keep the list of methodBranchProfileInfo sorted
   // which in turn speeds up searches.
   TR_ASSERT(comp->getMethodBranchInfos().empty() ||
             (int32_t)callSiteIndex > comp->getMethodBranchInfos().front()->getCallSiteIndex(),
             "Adding to MethodBranchProfileInfo must be done in order");
   TR_MethodBranchProfileInfo *methodBPInfo = new (comp->trHeapMemory()) TR_MethodBranchProfileInfo(callSiteIndex, comp);
   comp->getMethodBranchInfos().push_front(methodBPInfo);

   return methodBPInfo;
   }

TR_MethodBranchProfileInfo *
TR_MethodBranchProfileInfo::getMethodBranchProfileInfo(uint32_t callSiteIndex, TR::Compilation *comp)
   {
   TR::list<TR_MethodBranchProfileInfo*> &infos = comp->getMethodBranchInfos();

   // list is sorted in descending order; let's jump over elements that are bigger
   auto info = infos.begin();
   for (;  (info != infos.end()) && (*info)->getCallSiteIndex() > callSiteIndex; ++info)
      {}
   if ((info != infos.end()) && (*info)->getCallSiteIndex() == callSiteIndex)
      {
      return *info;
      }
   return NULL;
   }

void
TR_MethodBranchProfileInfo::resetMethodBranchProfileInfos(int32_t oldMaxFrequency, int32_t oldMaxEdgeFrequency, TR::Compilation *comp)
   {
   TR::list<TR_MethodBranchProfileInfo*> &infos = comp->getMethodBranchInfos();
   for (auto info = infos.begin(); info != infos.end(); ++info)
      {
      (*info)->_oldMaxFrequency = oldMaxFrequency;
      (*info)->_oldMaxEdgeFrequency = oldMaxEdgeFrequency;
      (*info)->setCallFactor(1.0f);
      }
   }

void
TR_MethodValueProfileInfo::addValueProfileInfo(TR_OpaqueMethodBlock *method, TR_ValueProfileInfo *vpInfo, TR::Compilation *comp)
   {
   TR_MethodValueProfileInfo *methodVPInfo = new (comp->trHeapMemory()) TR_MethodValueProfileInfo(method, vpInfo, comp);
   comp->getMethodVPInfos().push_front(methodVPInfo);
   }

TR_ValueProfileInfo *
TR_MethodValueProfileInfo::getValueProfileInfo(TR_OpaqueMethodBlock *method, TR::Compilation *comp)
   {
   return getValueProfileInfo(method, comp->getMethodVPInfos(), comp);
   }

void
TR_MethodValueProfileInfo::addHWValueProfileInfo(TR_OpaqueMethodBlock *method, TR_ValueProfileInfo *vpInfo, TR::Compilation *comp)
   {
   TR_MethodValueProfileInfo *methodVPInfo = new (comp->trHeapMemory()) TR_MethodValueProfileInfo(method, vpInfo, comp);
   comp->getMethodHWVPInfos().push_front(methodVPInfo);
   }

TR_ValueProfileInfo *
TR_MethodValueProfileInfo::getHWValueProfileInfo(TR_OpaqueMethodBlock *method, TR::Compilation *comp)
   {
   return getValueProfileInfo(method, comp->getMethodHWVPInfos(), comp);
   }

TR_ValueProfileInfo *
TR_MethodValueProfileInfo::getValueProfileInfo(TR_OpaqueMethodBlock *method, TR::list<TR_MethodValueProfileInfo*> &infos, TR::Compilation *comp)
   {
   for (auto info = infos.begin(); info != infos.end(); ++info)
      {
      if ((*info)->getPersistentIdentifier() == method)
         return (*info)->getValueProfileInfo();
      }

   return NULL;
   }


TR_ValueProfileInfo::TR_ValueProfileInfo()
   {
   _values = 0;
   _iProfilerValues = 0;
   _hwProfilerValues = 0;
   }

TR_ValueProfileInfo::~TR_ValueProfileInfo()
   {
   TR_AbstractInfo *tmpInfo;
   while (_values)
      {
      tmpInfo = _values;
      _values = _values->_next;
      tmpInfo->~TR_AbstractInfo();
      jitPersistentFree(tmpInfo);
      }
   while (_iProfilerValues)
      {
      tmpInfo = _iProfilerValues;
      _iProfilerValues = _iProfilerValues->_next;
      tmpInfo->~TR_AbstractInfo();
      jitPersistentFree(tmpInfo);
      }
   while (_hwProfilerValues)
      {
      tmpInfo = _hwProfilerValues;
      _hwProfilerValues = _hwProfilerValues->_next;
      tmpInfo->~TR_AbstractInfo();
      jitPersistentFree(tmpInfo);
      }
   _callSites = NULL;
   }

TR_AbstractInfo *
TR_ValueProfileInfo::getValueInfo(
      TR_ByteCodeInfo & bcInfo,
      TR::Compilation *comp,
      TR_ValueInfoType type)
   {
   //
   // Try to find a value profiling slot that matches the requested node
   //
   TR_CallSiteInfo * callSites = TR_CallSiteInfo::get(comp);

   if (!callSites)
      return 0;

   for (TR_AbstractInfo *valueInfo = _values; valueInfo; valueInfo = valueInfo->_next)
      if (callSites->hasSameBytecodeInfo(valueInfo->_byteCodeInfo, bcInfo, comp))
         {
         if ((type == Any) ||
             ((type == BigDecimal) && valueInfo->asBigDecimalValueInfo()) ||
             ((type == String) && valueInfo->asStringValueInfo()) ||
             ((type == NotBigDecimalOrString) && !valueInfo->asBigDecimalValueInfo() && !valueInfo->asStringValueInfo()))
            return valueInfo;
         }

   if (!comp->isProfilingCompilation())
      {
      TR_AbstractInfo *bestMatchedValueInfo = NULL;
      int32_t maxMatchedCount = 0;
      int32_t matchCount = 0;
      for (TR_AbstractInfo *valueInfo = _values; valueInfo; valueInfo = valueInfo->_next)
         if ((matchCount = callSites->hasSamePartialBytecodeInfo(valueInfo->_byteCodeInfo, bcInfo, comp)) > maxMatchedCount)
             {
            if ((type == Any) ||
                ((type == BigDecimal) && valueInfo->asBigDecimalValueInfo()) ||
                ((type == String) && valueInfo->asStringValueInfo()) ||
                ((type == NotBigDecimalOrString) && !valueInfo->asBigDecimalValueInfo() && !valueInfo->asStringValueInfo()))
               {
             bestMatchedValueInfo = valueInfo;
             maxMatchedCount = matchCount;
               }
             }


      if (maxMatchedCount > 0)
         return bestMatchedValueInfo;
      }

   return 0;
   }

TR_AbstractInfo *
TR_ValueProfileInfo::getValueInfoFromExternalProfiler(
      TR_ByteCodeInfo & bcInfo,
      TR::Compilation *comp)
   {

   //
   // Try to find a value profiling slot that matches the requested node
   //
   if (!_profiler)
      return 0;

   for (TR_AbstractInfo *valueInfo = _iProfilerValues; valueInfo; valueInfo = valueInfo->_next)
      if (_profiler->hasSameBytecodeInfo(valueInfo->_byteCodeInfo, bcInfo, comp))
         return valueInfo;

   return comp->fej9()->createIProfilingValueInfo(bcInfo, comp);
   }

TR_AbstractInfo *
TR_ValueProfileInfo::getValueInfo(
      TR::Node *node,
      TR::Compilation *comp,
      TR_ValueInfoType type)
   {
   return getValueInfo(node->getByteCodeInfo(), comp, type);
   }

TR_AbstractInfo *
TR_ValueProfileInfo::createAndInitializeValueInfo(
      TR::Node *node,
      bool warmCompilePICCallSite,
      TR::Compilation * comp,
      TR_AllocationKind allocKind = persistentAlloc,
      uintptrj_t initialValue = 0xdeadf00d,
      TR_ValueInfoType type)
   {
   return createAndInitializeValueInfo(node->getByteCodeInfo(), node->getDataType(), warmCompilePICCallSite, comp, allocKind, initialValue, type);
   }

TR_AbstractInfo *
TR_ValueProfileInfo::createAndInitializeValueInfo(
      TR_ByteCodeInfo &bcInfo,
      TR::DataType dataType,
      bool warmCompilePICCallSite,
      TR::Compilation * comp,
      TR_AllocationKind allocKind = persistentAlloc,
      uintptrj_t initialValue = 0xdeadf00d,
      uint32_t frequency,
      bool externalProfilerValue,
      TR_ValueInfoType type,
      bool hwProfilerValue)
   {
   TR_AbstractInfo *valueInfo;

   if (dataType == TR::Address )
      {
      if (warmCompilePICCallSite)
         {
         TR_ASSERT(allocKind == persistentAlloc, "Warm compile PIC info is always persistent");
         valueInfo = new (PERSISTENT_NEW) TR_WarmCompilePICAddressInfo();
         }
      else
         {
         if (type == BigDecimal)
            {
            uintptrj_t tmpValue = initialValue;

            if (allocKind == persistentAlloc)
               valueInfo = new (PERSISTENT_NEW) TR_BigDecimalValueInfo();
            else
               valueInfo = new (comp->trHeapMemory()) TR_BigDecimalValueInfo();

            ((TR_BigDecimalValueInfo *)valueInfo)->_scale1 = (int32_t) tmpValue;
            ((TR_BigDecimalValueInfo *)valueInfo)->_flag1 = (int32_t) tmpValue;
         }
      else if (type == String)
            {
            uintptrj_t tmpValue = initialValue;

            if (allocKind == persistentAlloc)
               valueInfo = new (PERSISTENT_NEW) TR_StringValueInfo();
            else
               valueInfo = new (comp->trHeapMemory()) TR_StringValueInfo();

            ((TR_StringValueInfo *)valueInfo)->_chars1 = (char *) tmpValue;
            ((TR_StringValueInfo *)valueInfo)->_length1 = (int32_t) tmpValue;
         }
      else
         {
         uintptrj_t tmpValue = initialValue;

         if (allocKind == persistentAlloc)
            valueInfo = new (PERSISTENT_NEW) TR_AddressInfo();
         else
            valueInfo = new (comp->trHeapMemory()) TR_AddressInfo();

         if ((tmpValue == 0xdeadf00d) &&
             (sizeof(uintptrj_t)==8))
            tmpValue = (uintptrj_t)CONSTANT64(0xdeadf00ddeadf00d);

         ((TR_AddressInfo *)valueInfo)->_value1 = tmpValue;
         }
      }
      }
   else
      {
      if (dataType == TR::Int64)
         {
         uint64_t tmpValue = initialValue;

         if (allocKind == persistentAlloc)
            valueInfo = new (PERSISTENT_NEW) TR_LongValueInfo();
         else
            valueInfo = new (comp->trHeapMemory()) TR_LongValueInfo();

         if (tmpValue == 0xdeadf00d)
             tmpValue = CONSTANT64(0xdeadf00ddeadf00d);

         ((TR_ValueInfo *)valueInfo)->_value1 = tmpValue;
         }
   else
      {
      if (allocKind == persistentAlloc)
         valueInfo = new (PERSISTENT_NEW) TR_ValueInfo();
      else
         valueInfo = new (comp->trHeapMemory()) TR_ValueInfo();

      ((TR_ValueInfo *)valueInfo)->_value1 = (uint32_t) initialValue;
      ((TR_ValueInfo *)valueInfo)->_maxValue = initialValue;
         }
      }

   valueInfo->_byteCodeInfo = bcInfo;
   valueInfo->_frequency1 = 0;
   valueInfo->_totalFrequency = 0;

   if (!(initialValue == 0xdeadf00d))
      {
      valueInfo->_frequency1 +=10;
      valueInfo->_totalFrequency +=10;
      }

   if (!externalProfilerValue)
      {
      valueInfo->_next = _values;
      _values = valueInfo;
      }
   else
      {
      if (frequency != 0)
         {
         valueInfo->_frequency1 = frequency;
         valueInfo->_totalFrequency = frequency;
         }
      if (hwProfilerValue)
         {
         valueInfo->_next = _hwProfilerValues;
         _hwProfilerValues = valueInfo;
         }
      else
         {
         valueInfo->_next = _iProfilerValues;
         _iProfilerValues = valueInfo;
         }
      }

   return valueInfo;
   }

TR_AbstractInfo *
TR_ValueProfileInfo::getOrCreateValueInfo(
      TR::Node *node,
      bool warmCompilePICCallSite,
      TR::Compilation *comp,
      TR_ValueInfoType type)
   {
   return getOrCreateValueInfo(node, warmCompilePICCallSite, persistentAlloc, 0xdeadf00d, comp, type);
   }

TR_AbstractInfo *
TR_ValueProfileInfo::getOrCreateValueInfo(
      TR::Node *node,
      bool warmCompilePICCallSite,
      TR_AllocationKind allocKind,
      uintptrj_t initialValue,
      TR::Compilation *comp,
      TR_ValueInfoType type)
   {
   return getOrCreateValueInfo(node->getByteCodeInfo(), node->getDataType(), warmCompilePICCallSite, allocKind, initialValue, comp, type);
   }

TR_AbstractInfo *
TR_ValueProfileInfo::getOrCreateValueInfo(
      TR_ByteCodeInfo &bcInfo,
      TR::DataType dataType,
      bool warmCompilePICCallSite,
      TR_AllocationKind allocKind,
      uintptrj_t initialValue,
      TR::Compilation *comp,
      TR_ValueInfoType type)
   {
   TR_AbstractInfo *valueInfo = getValueInfo(bcInfo, comp, type);

   if (valueInfo && valueInfo->asWarmCompilePICAddressInfo() && !warmCompilePICCallSite)
      {
      valueInfo = NULL;
      }

   if (!valueInfo)
      {
      valueInfo = createAndInitializeValueInfo(bcInfo, dataType, warmCompilePICCallSite, comp, allocKind, initialValue, 0, false, type);
      }

   return valueInfo;
   }

int32_t TR_BlockFrequencyInfo::_enableJProfilingRecompilation = -1;

TR_BlockFrequencyInfo::TR_BlockFrequencyInfo(TR_CallSiteInfo *callSiteInfo, int32_t numBlocks, TR_ByteCodeInfo *blocks, int32_t *frequencies) :
   _callSiteInfo(callSiteInfo),
   _numBlocks(numBlocks),
   _blocks(blocks),
   _frequencies(frequencies),
   _counterDerivationInfo(NULL),
   _entryBlockNumber(-1)
   {
   }

TR_BlockFrequencyInfo::TR_BlockFrequencyInfo(
   TR::Compilation *comp,
   TR_AllocationKind allocKind) :
   _callSiteInfo(TR_CallSiteInfo::get(comp)),
   _numBlocks(comp->getFlowGraph()->getNextNodeNumber()),
   _blocks(
      _numBlocks ?
      new (comp->trMemory(), allocKind, TR_Memory::BlockFrequencyInfo) TR_ByteCodeInfo[_numBlocks] :
      0
      ),
   _frequencies(
      _numBlocks ?
      /*
       * The explicit parens value initialize the array,
       * which in turn value initializes each array member,
       * which for ints is zero initializaiton.
       */
      new (comp->trMemory(), allocKind, TR_Memory::BlockFrequencyInfo) int32_t[_numBlocks]() :
      NULL
      ),
   _counterDerivationInfo(NULL),
   _entryBlockNumber(-1)
   {
   for (size_t i = 0; i < _numBlocks; ++i)
      {
      _blocks[i].setDoNotProfile(true);
      _blocks[i].setIsSameReceiver(false);
      _blocks[i].setInvalidCallerIndex();
      _blocks[i].setInvalidByteCodeIndex();
      }

   for (TR::CFGNode *node = comp->getFlowGraph()->getFirstNode(); node; node = node->getNext())
      {
      TR::TreeTop *startTree = node->asBlock()->getEntry();
      if (startTree)
         {
         TR_ByteCodeInfo &byteCodeInfo = startTree->getNode()->getByteCodeInfo();
         int32_t callSite = byteCodeInfo.getCallerIndex();
         TR_ASSERT(callSite < static_cast<int32_t>(TR_CallSiteInfo::get(comp)->getNumCallSites() & 0x7FFFFFFF), "Block call site number is unexpected");
         _blocks[node->getNumber()] = byteCodeInfo;
         }
      else
         {
         TR_ASSERT(
            node->getNumber() == 0 || node->getNumber() == 1,
            "Only the entry and exit nodes are expected not to have an entry tree."
            );
         }
      }
   }

TR_BlockFrequencyInfo::~TR_BlockFrequencyInfo()
   {
   _callSiteInfo = NULL;
   if (_blocks)
      {
      jitPersistentFree(_blocks);
      }
   if (_frequencies)
      {
      jitPersistentFree(_frequencies);
      }

   if (_counterDerivationInfo)
      {
      for (int i = 0; i < 2 * _numBlocks; ++i)
         {
         if (_counterDerivationInfo[i] &&
            ((int64_t)_counterDerivationInfo[i] & 0x1) != 1)
            {
            // Free the chunks held by each bitvector using setSize(0)
            _counterDerivationInfo[i]->setSize(0);
            // Free each bitvector
            jitPersistentFree(_counterDerivationInfo[i]);
            _counterDerivationInfo[i] = NULL;
            }
         }
      jitPersistentFree(_counterDerivationInfo);
      _counterDerivationInfo = NULL;
      }
   }

int32_t
TR_BlockFrequencyInfo::getFrequencyInfo(
      TR::Block *block,
      TR::Compilation *comp)
   {

   if (!block->getEntry())
      return -1;

   TR::Node *startNode = block->getEntry()->getNode();
   TR_ByteCodeInfo bci = startNode->getByteCodeInfo();
   bool normalizeForCallers = true;
   if (bci.getCallerIndex() == -10)
      {
      bci.setCallerIndex(comp->getCurrentInlinedSiteIndex());
      normalizeForCallers = false;
      }
   int32_t frequency = getFrequencyInfo(bci, comp, normalizeForCallers, true);
   if (comp->getOption(TR_TraceBFGeneration))
      traceMsg(comp, "@@ block_%d [%d,%d] has raw count %d\n", block->getNumber(), bci.getCallerIndex(), bci.getByteCodeIndex(), frequency);
   return frequency;
   }

int32_t
TR_BlockFrequencyInfo::getFrequencyInfo(
      TR_ByteCodeInfo &bci,
      TR::Compilation *comp,
      bool normalizeForCallers,
      bool trace)
   {
   int64_t maxCount = normalizeForCallers ? getMaxRawCount() : getMaxRawCount(bci.getCallerIndex());
   int32_t callerIndex = bci.getCallerIndex();
   int32_t frequency = getRawCount(callerIndex < 0 ? comp->getMethodSymbol() : comp->getInlinedResolvedMethodSymbol(callerIndex), bci, _callSiteInfo, maxCount, comp);
   traceMsg(comp,"raw frequency on outter level was %d for bci %d:%d\n", frequency, bci.getCallerIndex(), bci.getByteCodeIndex());
   if (frequency > -1 || _counterDerivationInfo == NULL)
      return frequency;

   // if we still don't have any data we didn't inline this method previously
   // so now we need to look at profiling data from other JProfiled bodies in the
   // hope that we have data there
   if (callerIndex > -1)
      {
      if (trace)
         traceMsg(comp, "Previous inlining was different - looking for a grafting point\n");

      // step 1 - build the callstack in reverse order for searching
      TR_ByteCodeInfo bciToCheck = bci;
      TR::list<std::pair<TR_OpaqueMethodBlock*, TR_ByteCodeInfo> > callStack(comp->allocator());
      while (bciToCheck.getCallerIndex() > -1)
         {
         TR_InlinedCallSite *callSite = &comp->getInlinedCallSite(bciToCheck.getCallerIndex());
         callStack.push_back(std::make_pair(comp->fe()->getInlinedCallSiteMethod(callSite), bciToCheck));
         bciToCheck = callSite->_byteCodeInfo;
         }
      
      // step 2 - find the level at which the inlining has begun to differ for the previous compile
      // eg find the point where the current profiling info has no profiling data for the given bci
      TR_ByteCodeInfo lastProfiledBCI = bciToCheck;
      int64_t outterProfiledFrequency = getRawCount(comp->getMethodSymbol(), bciToCheck, _callSiteInfo, maxCount, comp);
      if (outterProfiledFrequency == 0)
         return 0;

      while (!callStack.empty())
         {
         bciToCheck = callStack.back().second;
         callStack.pop_back();
         int32_t callerIndex = bciToCheck.getCallerIndex();
         TR_ResolvedMethod *resolvedMethod = callerIndex > -1 ? comp->getInlinedResolvedMethod(callerIndex) : comp->getCurrentMethod();
         TR::ResolvedMethodSymbol *resolvedMethodSymbol = callerIndex > -1 ? comp->getInlinedResolvedMethodSymbol(callerIndex) : comp->getMethodSymbol();
         int32_t callerFrequency = getRawCount(resolvedMethodSymbol, bciToCheck, _callSiteInfo, maxCount, comp);
         double innerFrequencyScale = 1;
         // we have found a frame where we don't have profiling info
         if (callerFrequency < 0)
            {
            traceMsg(comp, "  found frame for %s with no outter profiling info\n", resolvedMethodSymbol->signature(comp->trMemory()));
            // has this method been compiled so we might have had a chance to profile it?
            if (!resolvedMethod->isInterpreted()
                && !resolvedMethod->isNative()
                && !resolvedMethod->isJNINative())
               {
               void *startPC = resolvedMethod->startAddressForInterpreterOfJittedMethod();
               if (startPC)
                  {
                  TR_PersistentMethodInfo *methodInfo = comp->getRecompilationInfo()->getMethodInfoFromPC(startPC);
                  // do we have JProfiling data for this frame
                  if (methodInfo
                      && methodInfo->getProfileInfo()
                      && methodInfo->getProfileInfo()->getBlockFrequencyInfo()
                      && methodInfo->getProfileInfo()->getBlockFrequencyInfo()->_counterDerivationInfo)
                     {
                     traceMsg(comp, "  method has profiling\n");
                     int32_t effectiveCallerIndex = -1;
                     TR_BlockFrequencyInfo *bfi = methodInfo->getProfileInfo()->getBlockFrequencyInfo();
                     if (callStack.empty() || methodInfo->getProfileInfo()->getCallSiteInfo()->computeEffectiveCallerIndex(comp, callStack, effectiveCallerIndex))
                        {
                        TR_ByteCodeInfo callee(bci);
                        callee.setCallerIndex(effectiveCallerIndex);
                        if (trace && effectiveCallerIndex > -1)
                           traceMsg(comp, "  checking bci %d:%d\n", callee.getCallerIndex(), callee.getByteCodeIndex());
                        int32_t computedFrequency = bfi->getRawCount(resolvedMethodSymbol, callee, methodInfo->getProfileInfo()->getCallSiteInfo(), normalizeForCallers ? bfi->getMaxRawCount() : bfi->getMaxRawCount(callee.getCallerIndex()), comp);
                        if (normalizeForCallers)
                           {
                           callee.setCallerIndex(-1);
                           callee.setByteCodeIndex(0);
                           int32_t entry = bfi->getRawCount(resolvedMethodSymbol, callee, methodInfo->getProfileInfo()->getCallSiteInfo(), bfi->getMaxRawCount(), comp);
                           if (entry == 0)
                              {
                              frequency = 0;
                              break;
                              }
                           else if (entry > -1)
                              {
                              innerFrequencyScale *= entry;
                              }

                           if (computedFrequency > -1)
                              {
                              traceMsg(comp, " effective caller %s gave frequency %d\n", resolvedMethodSymbol->signature(comp->trMemory()), computedFrequency);
                              frequency = (int32_t)((outterProfiledFrequency * computedFrequency) / innerFrequencyScale);
                              break;
                              }
                           }
                        else
                           {
                           frequency = computedFrequency;
                           break;
                           }
                        }
                     else if (normalizeForCallers)
                        {
                        TR_ByteCodeInfo entry;
                        entry.setCallerIndex(-1);
                        entry.setByteCodeIndex(0);
                        int32_t entryCount = bfi->getRawCount(resolvedMethodSymbol, entry, methodInfo->getProfileInfo()->getCallSiteInfo(), bfi->getMaxRawCount(), comp);
                        TR_ByteCodeInfo call = callStack.back().second;
                        call.setCallerIndex(-1);
                        int32_t rawCount = bfi->getRawCount(resolvedMethodSymbol, call, methodInfo->getProfileInfo()->getCallSiteInfo(), bfi->getMaxRawCount(), comp);
                        if (rawCount == 0)
                           {
                           frequency = 0;
                           break;
                           }
                        else if (rawCount > -1)
                           {
                           innerFrequencyScale = (innerFrequencyScale * rawCount) / entryCount;
                           continue;
                           }
                        }
                     }
                  }
               }
            // if we get here we have not been able to compute a frequency based on JProfiling data so
            // we will resort to IProfiler data stashed during ilgen (if available)
            // we need to get two frequencies 1) the entry frequency and 2) the frequency of the block we are interested in
               {
               int32_t computedFrequency = resolvedMethodSymbol->getProfilerFrequency(bci.getByteCodeIndex());
               int32_t entryFrequency = resolvedMethodSymbol->getProfilerFrequency(0);
               if (computedFrequency < 0 || entryFrequency < 0)
                  continue;
               //printf("here %s\n", comp->signature());
               if (normalizeForCallers)
                  {
                  if (callStack.size() > 0)
                     {
                     innerFrequencyScale = (innerFrequencyScale * computedFrequency) / entryFrequency;
                     }
                  else
                     {
                     //if (TR::Options::isAnyVerboseOptionSet())
                     //   TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, " BFINFO: %s [%s:%d] - from IProfiler", comp->signature(), (bci.getCallerIndex() < 0 ? comp->getMethodSymbol() : comp->getInlinedResolvedMethodSymbol(bci.getCallerIndex()))->signature(comp->trMemory()), bci.getByteCodeIndex());
                     frequency = (int32_t)((outterProfiledFrequency * computedFrequency) / (innerFrequencyScale * entryFrequency));
                     break;
                     }
                  }
               else if (callStack.size() == 0)
                  frequency = computedFrequency;
               }
            }
         else
            {
            lastProfiledBCI = bciToCheck;
            outterProfiledFrequency = callerFrequency;
            }
         }
      }

   return frequency;
   }

int32_t
TR_BlockFrequencyInfo::getRawCount(TR::ResolvedMethodSymbol *resolvedMethod, TR_ByteCodeInfo &bci, TR_CallSiteInfo *callSiteInfo, int64_t maxCount, TR::Compilation *comp)
   {
   int32_t frequency = getRawCount(bci, callSiteInfo, maxCount, comp);
   if (frequency > -1 || _counterDerivationInfo == NULL)
      return frequency;

   int32_t byteCodeToSearch = resolvedMethod->getProfilingByteCodeIndex(bci.getByteCodeIndex());
   if (byteCodeToSearch > -1)
      {
      TR_ByteCodeInfo searchBCI = bci;
      searchBCI.setByteCodeIndex(byteCodeToSearch);
      frequency = getRawCount(searchBCI, callSiteInfo, maxCount, comp);
      }
   return frequency;
   }

int32_t
TR_BlockFrequencyInfo::getRawCount(TR_ByteCodeInfo &bci, TR_CallSiteInfo *callSiteInfo, int64_t maxCount, TR::Compilation *comp)
   {
   // Try to find a block profiling slot that matches the requested block
   //
   int64_t frequency = 0;
   int32_t blocksMatched = 0;
   bool currentCallSiteInfo = TR_CallSiteInfo::get(comp) == callSiteInfo;

   for (uint32_t i = 0; i < _numBlocks; ++i)
      {
      if ((currentCallSiteInfo && callSiteInfo->hasSameBytecodeInfo(_blocks[i], bci, comp))
          || (!currentCallSiteInfo && _blocks[i].getCallerIndex() == bci.getCallerIndex() && _blocks[i].getByteCodeIndex() == bci.getByteCodeIndex()))
         {
         int64_t rawCount = 0;
         if (_counterDerivationInfo == NULL)
            {
            rawCount += _frequencies[i];
            }
         else
            {
            if (bci.getCallerIndex() == -1 && bci.getByteCodeIndex() == 0 && i != 2)
               continue;

            TR_BitVector *toAdd = _counterDerivationInfo[i * 2];
            if (toAdd == NULL)
               {
               continue;
               }
            else if (((uintptr_t)toAdd & 0x1) == 1)
               {
               rawCount = _frequencies[(uintptr_t)toAdd >> 1];
               }
            else
               {
               TR_BitVectorIterator addBVI(*toAdd);
               while (addBVI.hasMoreElements())
                  rawCount += _frequencies[addBVI.getNextElement()];
               }

            TR_BitVector *toSub = _counterDerivationInfo[i * 2 + 1];
            if (toSub != NULL)
               {
               if (((uintptr_t)toSub & 0x1) == 1)
                  {
                  rawCount -= _frequencies[(uintptr_t)toSub >> 1];
                  }
               else
                  {
                  TR_BitVectorIterator subBVI(*toSub);
                  while (subBVI.hasMoreElements())
                     rawCount -= _frequencies[subBVI.getNextElement()];
                  }
               }
            if (comp->getOption(TR_TraceBFGeneration))
               traceMsg(comp, "   Slot %d has raw frequency %d\n", i, rawCount);

            if (maxCount > 0)
               rawCount = ((10000 * (uintptr_t)rawCount) / maxCount);
            else
               rawCount = 0;
            }

         if (comp->getOption(TR_TraceBFGeneration))
            traceMsg(comp, "   Slot %d has frequency %d\n", i, rawCount);

         frequency += rawCount;
         blocksMatched++;
         }
      }

   if (blocksMatched == 0)
      return -1;
   else if (_counterDerivationInfo == NULL)
      return frequency;
   else
      return frequency / blocksMatched;
   }

int32_t TR_BlockFrequencyInfo::getCallCount()
   {
   if (_counterDerivationInfo == NULL || _entryBlockNumber < 0)
      return -1;

   int32_t count = 0;
   TR_BitVector *toAdd = _counterDerivationInfo[_entryBlockNumber * 2];
   if (toAdd == NULL)
      return -1;

   if (((uintptr_t)toAdd & 0x1) == 1)
      {
      count += _frequencies[(uintptr_t)toAdd >> 1];
      }
   else
      {
      TR_BitVectorIterator addBVI(*toAdd);
      while (addBVI.hasMoreElements())
         count += _frequencies[addBVI.getNextElement()];
      }

   TR_BitVector *toSub = _counterDerivationInfo[_entryBlockNumber * 2 + 1];
   if (toSub != NULL)
      {
      if (((uintptr_t)toSub & 0x1) == 1)
         {
         count -= _frequencies[(uintptr_t)toSub >> 1];
         }
      else
         {
         TR_BitVectorIterator subBVI(*toSub);
         while (subBVI.hasMoreElements())
            count -= _frequencies[subBVI.getNextElement()];
         }
      }

   return count;
   }

int32_t TR_BlockFrequencyInfo::getMaxRawCount(int32_t callerIndex)
   {
   int32_t maxCount = 0;
   if (_counterDerivationInfo == NULL)
      {
      for (int32_t i = 0; i < _numBlocks; ++i)
         {
         if (_blocks[i].getCallerIndex() != callerIndex)
            continue;

         if (_frequencies[i] > maxCount)
            maxCount = _frequencies[i];
         }
      }
   else
      {
      for (int32_t i = 0; i < _numBlocks; ++i)
         {
         if (_blocks[i].getCallerIndex() != callerIndex)
            continue;

         int32_t count = 0;
         TR_BitVector *toAdd = _counterDerivationInfo[i * 2];
         if (toAdd == NULL)
            {
            continue;
            }
         else if (((uintptr_t)toAdd & 0x1) == 1)
            {
            count += _frequencies[(uintptr_t)toAdd >> 1];
            }
         else
            {
            TR_BitVectorIterator addBVI(*toAdd);
            while (addBVI.hasMoreElements())
               count += _frequencies[addBVI.getNextElement()];
            }

         TR_BitVector *toSub = _counterDerivationInfo[i * 2 + 1];
         if (toSub != NULL)
            {
            if (((uintptr_t)toSub & 0x1) == 1)
               {
               count -= _frequencies[(uintptr_t)toSub >> 1];
               }
            else
               {
               TR_BitVectorIterator subBVI(*toSub);
               while (subBVI.hasMoreElements())
                  count -= _frequencies[subBVI.getNextElement()];
               }
            }
         if (count > maxCount)
            maxCount = count;
         }
      }
   return maxCount;
   }

int32_t TR_BlockFrequencyInfo::getMaxRawCount()
   {
   int32_t maxCount = 0;
   if (_counterDerivationInfo == NULL)
      {
      for (int32_t i = 0; i < _numBlocks; ++i)
         {
         if (_frequencies[i] > maxCount)
            maxCount = _frequencies[i];
         }
      }
   else
      {
      for (int32_t i = 0; i < _numBlocks; ++i)
         {
         int32_t count = 0;
         TR_BitVector *toAdd = _counterDerivationInfo[i * 2];
         if (toAdd == NULL)
            {
            continue;
            }
         else if (((uintptr_t)toAdd & 0x1) == 1)
            {
            count += _frequencies[(uintptr_t)toAdd >> 1];
            }
         else
            {
            TR_BitVectorIterator addBVI(*toAdd);
            while (addBVI.hasMoreElements())
               count += _frequencies[addBVI.getNextElement()];
            }

         TR_BitVector *toSub = _counterDerivationInfo[i * 2 + 1];
         if (toSub != NULL)
            {
            if (((uintptr_t)toSub & 0x1) == 1)
               {
               count -= _frequencies[(uintptr_t)toSub >> 1];
               }
            else
               {
               TR_BitVectorIterator subBVI(*toSub);
               while (subBVI.hasMoreElements())
                  count -= _frequencies[subBVI.getNextElement()];
               }
            }
         if (count > maxCount)
            maxCount = count;
         }
      }
   return maxCount;
   }


const uint32_t TR_CatchBlockProfileInfo::EDOThreshold = 50;


TR_CallSiteInfo::TR_CallSiteInfo(TR::Compilation * comp, TR_AllocationKind allocKind) :
   _numCallSites(comp->getNumInlinedCallSites()),
   _callSites(
      _numCallSites ?
      new (comp->trMemory(), allocKind, TR_Memory::CallSiteInfo) TR_InlinedCallSite[_numCallSites] :
      NULL
      ),
   _allocKind(allocKind)
   {
   for (uint32_t i = 0; i < _numCallSites; ++i)
      _callSites[i] = comp->getInlinedCallSite(i);
   }

TR_CallSiteInfo::~TR_CallSiteInfo()
   {
   if (_callSites)
      if (_allocKind == persistentAlloc)
      {
      jitPersistentFree(_callSites);
      }
   }

bool
TR_CallSiteInfo::computeEffectiveCallerIndex(TR::Compilation *comp, TR::list<std::pair<TR_OpaqueMethodBlock *, TR_ByteCodeInfo> > &callStack, int32_t &effectiveCallerIndex)
   {
   for (uint32_t i = 0; i < _numCallSites; ++i)
      {
      if (comp->fe()->getInlinedCallSiteMethod(&_callSites[i]) != callStack.front().first)
         continue;

      TR_InlinedCallSite *cursor = &_callSites[i];
      auto itr = callStack.begin(), end = callStack.end();
      while (cursor && itr != end)
         {
         if (comp->fe()->getInlinedCallSiteMethod(cursor) != itr->first)
            break;

         if (cursor->_byteCodeInfo.getCallerIndex() > -1)
            cursor = &_callSites[cursor->_byteCodeInfo.getCallerIndex()];
         else
            cursor = NULL;
         itr++;
         }

      // both have terminated at the same time means we have a match for our callstack fragment
      // so return it
      if (itr == end && cursor == NULL)
         {
         effectiveCallerIndex = i;
         return true;
         }
      }
   return false;
   }

bool
TR_CallSiteInfo::hasSameBytecodeInfo(
      TR_ByteCodeInfo & persistentBytecodeInfo,
      TR_ByteCodeInfo & currentBytecodeInfo,
      TR::Compilation *comp)
   {
   TR_ASSERT(
      currentBytecodeInfo.getByteCodeIndex() != TR_ByteCodeInfo::invalidByteCodeIndex,
      "Current byte code info is invalid."
      );

   if (persistentBytecodeInfo.getByteCodeIndex() != currentBytecodeInfo.getByteCodeIndex())
      return false;

   // Match call site information
   //
   int32_t persistentCallSite = persistentBytecodeInfo.getCallerIndex();
   int32_t currentCallSite = currentBytecodeInfo.getCallerIndex();

   TR_ASSERT(
      persistentCallSite < static_cast<int32_t>(_numCallSites & 0x7FFFFFFF),
      "Unexpected call site will overflow. persistentCallSite = %d, _numCallSites = %d",
      persistentCallSite,
      _numCallSites
      );

   while (persistentCallSite >= 0 && currentCallSite >= 0)
      {
      TR_InlinedCallSite &persistentCallSiteInfo = _callSites[persistentCallSite];
      TR_InlinedCallSite &currentCallSiteInfo = comp->getInlinedCallSite(currentCallSite);

      int32_t const persistentCallSiteByteCodeIndex = persistentCallSiteInfo._byteCodeInfo.getByteCodeIndex();
      int32_t const currentCallSiteByteCodeIndex = currentCallSiteInfo._byteCodeInfo.getByteCodeIndex();

      if (persistentCallSiteByteCodeIndex != currentCallSiteByteCodeIndex)
         break;

      TR_OpaqueMethodBlock *persitentCallSiteMethod = comp->fe()->getInlinedCallSiteMethod(&persistentCallSiteInfo);
      TR_OpaqueMethodBlock *currentCallSiteMethod = comp->fe()->getInlinedCallSiteMethod(&currentCallSiteInfo);

      if (persitentCallSiteMethod != currentCallSiteMethod)
         break;

      persistentCallSite = persistentCallSiteInfo._byteCodeInfo.getCallerIndex();
      currentCallSite = currentCallSiteInfo._byteCodeInfo.getCallerIndex();
      }

   return persistentCallSite < 0 && currentCallSite < 0;
   }


int32_t
TR_CallSiteInfo::hasSamePartialBytecodeInfo(
      TR_ByteCodeInfo & persistentBytecodeInfo,
      TR_ByteCodeInfo & currentBytecodeInfo,
      TR::Compilation *comp)
   {
   if (persistentBytecodeInfo.getByteCodeIndex() != currentBytecodeInfo.getByteCodeIndex())
      return 0;

   // Match call site information
   //
   int32_t callSite1 = currentBytecodeInfo.getCallerIndex();
   int32_t callSite2 = persistentBytecodeInfo.getCallerIndex();
   int matchLevelCount = 0;
   while (callSite1 >= 0 && callSite2 >= 0)
      {
      TR_InlinedCallSite &callSiteInfo1 = comp->getInlinedCallSite(callSite1);
      TR_InlinedCallSite &callSiteInfo2 = _callSites[callSite2];

      if (callSiteInfo1._byteCodeInfo.getByteCodeIndex() != callSiteInfo2._byteCodeInfo.getByteCodeIndex())
         break;
      TR_OpaqueMethodBlock *method1 = callSiteInfo1._methodInfo;
      if (comp->fej9()->isAOT_DEPRECATED_DO_NOT_USE())
         method1 = ((TR_AOTMethodInfo *)method1)->resolvedMethod->getPersistentIdentifier();
      TR_OpaqueMethodBlock *method2 = callSiteInfo2._methodInfo;
      if (comp->fej9()->isAOT_DEPRECATED_DO_NOT_USE())
         method2 = ((TR_AOTMethodInfo *)method2)->resolvedMethod->getPersistentIdentifier();
      if (method1 != method2)
         break;
      callSite1 = callSiteInfo1._byteCodeInfo.getCallerIndex();
      callSite2 = callSiteInfo2._byteCodeInfo.getCallerIndex();
      //traceMsg(comp, "\t\tMatched profiling info at level %d, method %p, bcIndex %d\n", matchLevelCount, method1, callSiteInfo1._byteCodeInfo._byteCodeIndex);
      matchLevelCount ++;
      }

   return matchLevelCount;
   }

TR_PersistentProfileInfo::~TR_PersistentProfileInfo()
   {
   if (_catchBlockProfileInfo)
      {
      jitPersistentFree(_catchBlockProfileInfo);
      _catchBlockProfileInfo = NULL;
      }
   if (_valueProfileInfo)
      {
      _valueProfileInfo->~TR_ValueProfileInfo();
      jitPersistentFree(_valueProfileInfo);
      _valueProfileInfo = NULL;
      }
   if (_blockFrequencyInfo)
      {
      _blockFrequencyInfo->~TR_BlockFrequencyInfo();
      jitPersistentFree(_blockFrequencyInfo);
      _blockFrequencyInfo = NULL;
      }
   if (_callSiteInfo)
      {
      _callSiteInfo->~TR_CallSiteInfo();
      jitPersistentFree(_callSiteInfo);
      _callSiteInfo = NULL;
      }
   }

TR_PersistentProfileInfo *
TR_PersistentProfileInfo::get(TR::Compilation *comp)
   {
   TR_PersistentMethodInfo * methodInfo = TR_PersistentMethodInfo::get(comp);

   return methodInfo ? methodInfo->getProfileInfo() : 0;
   }

TR_PersistentProfileInfo *
TR_PersistentProfileInfo::get(TR_ResolvedMethod * feMethod)
   {
   TR_PersistentMethodInfo * methodInfo = TR_PersistentMethodInfo::get(feMethod);
   return methodInfo ? methodInfo->getProfileInfo() : 0;
   }


#ifdef DEBUG

void TR_ValueProfileInfo::dumpInfo(TR_FrontEnd * fe, TR::FILE *logFile)
   {
   if (logFile == NULL)
      return;
   trfprintf(logFile, "\nvalue profile\n");
   for (TR_AbstractInfo * valueInfo = _values; valueInfo; valueInfo = valueInfo->_next)
      {
      trfprintf(logFile, "   Bytecode index = %d Caller index\n", valueInfo->_byteCodeInfo.getByteCodeIndex(), valueInfo->_byteCodeInfo.getCallerIndex());
      trfprintf(logFile, "   Total frequency = %d\n", valueInfo->_totalFrequency);
      trfprintf(logFile, "   Value1 = %d (or Value1=%d) Frequency1 = %d\n",
                ((TR_ValueInfo *)valueInfo)->_value1, ((TR_AddressInfo *)valueInfo)->_value1,valueInfo->_frequency1);
      }
   }

void TR_BlockFrequencyInfo::dumpInfo(TR_FrontEnd * fe, TR::FILE *logFile)
   {
   if (logFile == NULL)
      return;
   trfprintf(logFile, "\nblock frequency profile\n");
   for (int32_t i = 0; i < _numBlocks; i++)
      trfprintf(logFile, "   Block index = %d, caller = %d, frequency = %d\n", _blocks[i].getByteCodeIndex(), _blocks[i].getCallerIndex(), _frequencies[i]);
   }


static int32_t totalCatches, totalThrows;
void TR_CatchBlockProfileInfo::dumpInfo(TR_FrontEnd * fe, TR::FILE *logFile, const char * sig)
   {
   if (logFile == NULL)
      return;
   totalCatches += _catchCounter;
   totalThrows += _throwCounter;

   if (debug("compactCatchInfo"))
      {
      if (_catchCounter || _throwCounter)
         trfprintf(logFile, "\ncatch %7d throw %7d %sn", _catchCounter, _throwCounter, sig);
      }
   else
      {
      trfprintf(logFile, "\ncatch block profile\n");
      trfprintf(logFile, "   catch counter %d\n", _catchCounter);
      trfprintf(logFile, "   throw counter %d\n\n", _throwCounter);
      }
   }


void TR_CallSiteInfo::dumpInfo(TR_FrontEnd * fe, TR::FILE *logFile)
   {
   if (logFile == NULL)
      return;
   trfprintf(logFile, "\nCall site info\n");
   for (int32_t i = 0; i < _numCallSites; i++)
      trfprintf(logFile, "   Call site index = %d, method = %p, parent = %d\n", _callSites[i]._byteCodeInfo.getByteCodeIndex(), _callSites[i]._methodInfo, _callSites[i]._byteCodeInfo.getCallerIndex());
   }


void TR_PersistentProfileInfo::dumpInfo(TR_FrontEnd * fe, TR::FILE *logFile, const char * sig)
   {
   if (logFile == NULL)
      return;

   if (!debug("compactCatchInfo"))
      {
      trfprintf(logFile, "profiling frequency %d\n", getProfilingFrequency());
      trfprintf(logFile, "profiling count %d\n\n", getProfilingCount());

      if (_callSiteInfo)
         _callSiteInfo->dumpInfo(fe, logFile);

      if (_blockFrequencyInfo)
         _blockFrequencyInfo->dumpInfo(fe, logFile);
      }


   if (_catchBlockProfileInfo)
      _catchBlockProfileInfo->dumpInfo(fe, logFile, sig);


   if (_valueProfileInfo)
      _valueProfileInfo->dumpInfo(fe, logFile);
   }

#endif
